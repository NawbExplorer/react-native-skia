/*
* Copyright (C) 1994-2021 OpenTV, Inc. and Nagravision S.A.
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/


#include "jsi/JSIDynamic.h"

#include "ReactCommon/TurboModule.h"

#include "ReactSkia/utils/RnsLog.h"
#include "ReactSkia/JSITurboModuleManager.h"
#include "RSkNetworkingModuleBase.h"

namespace facebook {
namespace react {
RSkNetworkingModuleBase::RSkNetworkingModuleBase(
    const std::string &name,
    std::shared_ptr<CallInvoker> jsInvoker,
    Instance *bridgeInstance)
    : RSkEventEmitter(name, jsInvoker, bridgeInstance) {
    methodMap_["sendRequest"] = MethodMetadata{1, sendRequestWrapper};
    methodMap_["abortRequest"] = MethodMetadata{1, abortRequestWrapper};

}


jsi::Value RSkNetworkingModuleBase::sendRequestWrapper(
     jsi::Runtime &rt,
     TurboModule &turboModule,
     const jsi::Value *args,
     size_t count)  {

    auto &self = static_cast<RSkNetworkingModuleBase &>(turboModule);
    auto query = jsi::dynamicFromValue(rt, args[0]);
    jsi::Object callbackObj = args[1].getObject(rt);    
    return self.sendRequest(query, callbackObj, rt);
}

jsi::Value RSkNetworkingModuleBase::abortRequestWrapper(
     jsi::Runtime &rt,
     TurboModule &turboModule,
     const jsi::Value *args,
     size_t count) {

    auto &self = static_cast<RSkNetworkingModuleBase &>(turboModule);
    auto requestId = jsi::dynamicFromValue(rt, args[0]);
    return self.abortRequest(requestId);

}

}// namespace react
}//namespace facebook

