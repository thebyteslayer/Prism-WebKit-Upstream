/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FunctionOverridesTest.h"

#include "FunctionOverrides.h"
#include "InitializeThreading.h"
#include "JavaScript.h"
#include "Options.h"

using JSC::Options;

int testFunctionOverrides()
{
    bool failed = false;

    JSC::initialize();

    const char* oldFunctionOverrides = Options::functionOverrides();
    
    Options::functionOverrides() = "./testapiScripts/testapi-function-overrides.js";
    JSC::FunctionOverrides::reinstallOverrides();

    JSGlobalContextRef context = JSGlobalContextCreateInGroup(nullptr, nullptr);

    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    ASSERT_UNUSED(globalObject, JSValueIsObject(context, globalObject));

    const char* scriptString =
        "var str = '';" "\n"
        "function f1() { /* Original f1 */ }" "\n"
        "str += f1 + '\\n';" "\n"
        "var f2 = function() {" "\n"
        "    // Original f2" "\n"
        "}" "\n"
        "str += f2 + '\\n';" "\n"
        "str += (function() { /* Original f3 */ }) + '\\n';" "\n"
        "var f4Source = '/* Original f4 */'" "\n"
        "var f4 =  new Function(f4Source);" "\n"
        "str += f4 + '\\n';" "\n"
        "\n"
        "var expectedStr =" "\n"
        "'function f1() { /* Overridden f1 */ }\\n"
        "function() { /* Overridden f2 */ }\\n"
        "function() { /* Overridden f3 */ }\\n"
        "function anonymous(\\n) { /* Overridden f4 */ }\\n';"
        "var result = (str == expectedStr);" "\n"
        "result";

    JSStringRef script = JSStringCreateWithUTF8CString(scriptString);
    JSValueRef exception = nullptr;
    JSValueRef resultRef = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
    JSStringRelease(script);

    if (!JSValueIsBoolean(context, resultRef) || !JSValueToBoolean(context, resultRef))
        failed = true;

    JSGlobalContextRelease(context);
    
    JSC::Options::functionOverrides() = oldFunctionOverrides;
    JSC::FunctionOverrides::reinstallOverrides();

    SAFE_PRINTF("%s: function override tests.\n", failed ? "FAIL"_s : "PASS"_s);

    return failed;
}
