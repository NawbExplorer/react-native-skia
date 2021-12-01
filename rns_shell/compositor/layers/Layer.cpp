/*
* Copyright 2016 Google Inc.
* Copyright (C) 1994-2021 OpenTV, Inc. and Nagravision S.A.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#include "compositor/layers/Layer.h"
#include "compositor/layers/PictureLayer.h"

namespace RnsShell {

#if USE(RNS_SHELL_PARTIAL_UPDATES)
static inline void addDamgeRect(PaintContext& context, SkIRect dirtyAbsFrameRect) {
    // TODO Walk through the dirtyRect vector and
    // 1. If new dirty rect contains inside any pf existing dirtyRect then ignore
    // 2. If new dirty rect entirely covers any of existing dirtyRect then remove them from vector and add the new one

    context.damageRect.push_back(dirtyAbsFrameRect);
}
#endif

SharedLayer Layer::Create(LayerType type) {
    switch(type) {
        case LAYER_TYPE_PICTURE:
            return std::make_shared<PictureLayer>();
        case LAYER_TYPE_DEFAULT:
        default:
            RNS_LOG_ASSERT(false, "Default layers can be created only from RSkComponent constructor");
            return nullptr;
    }
}

uint64_t Layer::nextUniqueId() {
    static std::atomic<uint64_t> nextId(1);
    uint64_t id;
    do {
        id = nextId.fetch_add(1);
    } while (id == 0);  // 0 invalid id.
    return id;
}

Layer::Layer(LayerType type)
    : layerId_(nextUniqueId())
    , parent_(nullptr)
    , type_(type)
    , frame_(SkIRect::MakeEmpty())
    , absFrame_(SkIRect::MakeEmpty())
    , anchorPosition_(SkPoint::Make(0,0))
    , invalidateMask_(LayerInvalidateAll) {
    RNS_LOG_DEBUG("Layer Constructed(" << this << ") with ID : " << layerId_);
}

Layer* Layer::rootLayer() {
    Layer* layer = this;
    while (layer->parent())
        layer = layer->parent();
    return layer;
}

void Layer::appendChild(SharedLayer child) {
    insertChild(child, children_.size());
}

void Layer::insertChild(SharedLayer child, size_t index) {
    if(!child || !child.get()) {
        RNS_LOG_ERROR("Invalid child node passed");
        return;
    }

    child->removeFromParent();
    child->setParent(this);

    index = std::min(index, children_.size());
    RNS_LOG_DEBUG("Insert Child(" << child.get()->layerId() << ") at index : " << index << " and with parent : " << layerId_);
    children_.insert(children_.begin() + index, child);
}

void Layer::removeChild(Layer *child, size_t index) {
    if(index >= children_.size()) {
        RNS_LOG_ERROR("Invalid index passed for remove");
        return;
    }
    child->setParent(nullptr);

    RNS_LOG_DEBUG("Remove Child(" << child->layerId() << ") at index : " << index << " from parent : " << layerId_);
    children_.erase(children_.begin() + index);
}

void Layer::removeChild(Layer *child) {
    size_t index = 0;
    for (auto iter = children_.begin(); iter != children_.end(); ++iter, index++) {
        if (iter->get() != child)
            continue;
        removeChild(child, index);
        return;
    }
}

void Layer::removeFromParent() {
  if (parent_)
    parent_->removeChild(this);
}

void Layer::preRoll(PaintContext& context, bool forceLayout) {
    // Layer Layout has changed or parent has forced Layout change for child. Need to recalculate absolute frame and update absFrame_
    if(forceLayout || (invalidateMask_ & LayerLayoutInvalidate)) {
        // Adjust absolute position
        //TODO Once we have skia transform matrix, need to first update frame_ i.e before calculating newAbsFrame
        // layer_->setFrame( trnsfromed x, y width hight
        int frameAbsX = frame_.x(), frameAbsY = frame_.y();
        if(parent_) {
            frameAbsX += parent_->absFrame_.x();
            frameAbsY += parent_->absFrame_.y();
        }

        SkIRect newAbsFrame = SkIRect::MakeXYWH(frameAbsX, frameAbsY, frame_.width(), frame_.height());
#if USE(RNS_SHELL_PARTIAL_UPDATES)
        if((invalidateMask_ & LayerLayoutInvalidate)) {
            // Add previous frame to damage rect only if layer layout was invalidated and new layout is different from old layout
            if(context.supportPartialUpdate && (invalidateMask_ & LayerLayoutInvalidate)) {
                if(absFrame_.isEmpty() != true && newAbsFrame.contains(absFrame_) != true ) {
                    addDamgeRect(context, absFrame_); // Previous frame rect
                    RNS_LOG_DEBUG("New frame layout is different from previous frame. Add to damage rect..");
                }
            }
            RNS_LOG_DEBUG("PreRoll Layer(ID:" << layerId_ << ", ParentID:" << (parent_ ? parent_->layerId() : -1) <<
                ") Frame [" << frame_.x() << "," << frame_.y() << "," << frame_.width() << "," << frame_.height() <<
                "], AbsFrame(Prev,New) ([" << absFrame_.x() << "," << absFrame_.y() << "," << absFrame_.width() << "," << absFrame_.height() << "]" <<
                " - [" << newAbsFrame.x() << "," << newAbsFrame.y() << "," << newAbsFrame.width() << "," << newAbsFrame.height() << "])");
        }
#endif
        absFrame_ = newAbsFrame; // Save new absFrame
    }

#if USE(RNS_SHELL_PARTIAL_UPDATES)
    if(invalidateMask_ != LayerInvalidateNone) {// As long as there is some invalidation , it creates a dirty rect.
        RNS_LOG_DEBUG("AddDamage Layer(ID:" << layerId_ <<
            ") AbsFrame[" << absFrame_.x() << "," << absFrame_.y() << "," << absFrame_.width() << "," << absFrame_.height() << "]");
        addDamgeRect(context, absFrame_); // Previous frame rect
    }
#endif
}

void Layer::prePaint(PaintContext& context, bool forceLayout) {
    //Adjust absolute Layout frame and dirty rects
    bool forceChildrenLayout = (forceLayout || (invalidateMask_ & LayerLayoutInvalidate));
    preRoll(context, forceLayout);
    invalidateMask_ = LayerInvalidateNone;

    // prePaint children recursively
    for (auto& layer : children()) {
        layer->prePaint(context, forceChildrenLayout);
    }
}

void Layer::paintSelf(PaintContext& context) {
#if !defined(GOOGLE_STRIP_LOG) || (GOOGLE_STRIP_LOG <= INFO)
    RNS_GET_TIME_STAMP_US(start);
#endif

    this->onPaint(context.canvas);

#if !defined(GOOGLE_STRIP_LOG) || (GOOGLE_STRIP_LOG <= INFO)
    RNS_GET_TIME_STAMP_US(end);
    RNS_LOG_TRACE("Layer (" << layerId_ << ") took " <<  (end - start) << " us to paint self");
#endif
}

void Layer::paint(PaintContext& context) {
    RNS_LOG_DEBUG("Layer (" << layerId_ << ") has " << children_.size() << " childrens");

    SkAutoCanvasRestore save(context.canvas, true); // Save current clip and matrix state.

    paintSelf(context); // First paint self and then children if any
    context.canvas->translate(frame_.x(), frame_.y());

    if(masksToBounds_) { // Need to clip children.
        SkRect intRect = SkRect::Make(frame_);
        if(!context.dirtyClipBound.isEmpty() && intRect.intersect(context.dirtyClipBound) == false) {
            RNS_LOG_WARN("We should not call paint if it doesnt intersect with non empty dirtyClipBound...");
        }
        context.canvas->clipRect(intRect);
    }

    for (auto& layer : children_) {
        if(layer->needsPainting(context))
            layer->paint(context);
    }
}

bool Layer::hasAncestor(const Layer* ancestor) const {
    for (const Layer* layer = parent(); layer; layer = layer->parent()) {
        if (layer == ancestor)
            return true;
    }
    return false;
}

void Layer::setParent(Layer* layer) {
    if(layer && layer->hasAncestor(this)) {
        RNS_LOG_WARN("Child Layer cant be ancestor :)");
        return;
    }
    parent_ = layer;
}

bool Layer::needsPainting(PaintContext& context) {
    if (frame_.isEmpty() || isHidden_) { // If layer is hidden or layers paint bounds is empty then skip paint
      RNS_LOG_TRACE(this << " Layer (" << layerId_ << ") Bounds empty or hidden");
      return false;
    }

    if(context.damageRect.size() == 0) // No damage rect set, so need to paint the layer
        return true;

    // As long as paintBounds interset with one of the dirty rect, we will need repainting.
    SkIRect dummy;
    for (auto& dirtRect : context.damageRect) {
        if(dummy.intersect(absFrame_, dirtRect)) { // this layer intersects with one of the dirty rect, so needs repainting
            return true;
        }
    }

    RNS_LOG_TRACE("Skip Layer (" << layerId_ << ") Frame [" << frame_.x() << "," << frame_.y() << "," << frame_.width() << "," << frame_.height() << "]");
    return false;
}

}   // namespace RnsShell
