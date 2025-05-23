/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSCallbackData.h"

#include "Document.h"
#include "JSDOMBinding.h"
#include "JSExecState.h"
#include "JSExecStateInstrumentation.h"
#include <JavaScriptCore/Exception.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(JSCallbackData);

// https://webidl.spec.whatwg.org/#call-a-user-objects-operation
JSValue JSCallbackData::invokeCallback(JSDOMGlobalObject& globalObject, JSObject* callback, JSValue thisValue, MarkedArgumentBuffer& args, CallbackType method, PropertyName functionName, NakedPtr<JSC::Exception>& returnedException)
{
    ASSERT(callback);

    JSGlobalObject* lexicalGlobalObject = &globalObject;
    VM& vm = lexicalGlobalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue function;
    CallData callData;

    if (method != CallbackType::Object) {
        function = callback;
        callData = JSC::getCallData(callback);
    }
    if (callData.type == CallData::Type::None) {
        if (method == CallbackType::Function) {
            returnedException = JSC::Exception::create(vm, createTypeError(lexicalGlobalObject));
            return JSValue();
        }

        ASSERT(!functionName.isNull());
        function = callback->get(lexicalGlobalObject, functionName);
        if (scope.exception()) [[unlikely]] {
            returnedException = scope.exception();
            scope.clearException();
            return JSValue();
        }

        callData = JSC::getCallData(function);
        if (callData.type == CallData::Type::None) {
            returnedException = JSC::Exception::create(vm, createTypeError(lexicalGlobalObject, makeString('\'', String(functionName.uid()), "' property of callback interface should be callable"_s)));
            return JSValue();
        }

        thisValue = callback;
    }

    ASSERT(!function.isEmpty());
    ASSERT(callData.type != CallData::Type::None);

    ScriptExecutionContext* context = globalObject.scriptExecutionContext();
    // We will fail to get the context if the frame has been detached.
    if (!context)
        return JSValue();

    JSExecState::instrumentFunction(context, callData);

    returnedException = nullptr;
    JSValue result = JSExecState::profiledCall(lexicalGlobalObject, JSC::ProfilingReason::Other, function, callData, thisValue, args, returnedException);

    InspectorInstrumentation::didCallFunction(context);

    return result;
}

template<typename Visitor>
void JSCallbackData::visitJSFunction(Visitor& visitor)
{
    visitor.append(m_callback);
}

template void JSCallbackData::visitJSFunction(JSC::AbstractSlotVisitor&);
template void JSCallbackData::visitJSFunction(JSC::SlotVisitor&);

} // namespace WebCore
