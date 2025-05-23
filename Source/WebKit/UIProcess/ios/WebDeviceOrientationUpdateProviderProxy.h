/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)

#include "MessageReceiver.h"
#include <WebCore/MotionManagerClient.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class SecurityOriginData;
}
namespace WebKit {

class WebPageProxy;
struct SharedPreferencesForWebProcess;

class WebDeviceOrientationUpdateProviderProxy : public WebCore::MotionManagerClient, private IPC::MessageReceiver, public RefCounted<WebDeviceOrientationUpdateProviderProxy> {
    WTF_MAKE_TZONE_ALLOCATED(WebDeviceOrientationUpdateProviderProxy);
public:
    static Ref<WebDeviceOrientationUpdateProviderProxy> create(WebPageProxy&);
    ~WebDeviceOrientationUpdateProviderProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void startUpdatingDeviceOrientation(const WebCore::SecurityOriginData&);
    void stopUpdatingDeviceOrientation();

    void startUpdatingDeviceMotion(const WebCore::SecurityOriginData&);
    void stopUpdatingDeviceMotion();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(IPC::Connection&) const;

private:
    explicit WebDeviceOrientationUpdateProviderProxy(WebPageProxy&);

    // WebCore::WebCoreMotionManagerClient
    void orientationChanged(double, double, double, double, double) final;
    void motionChanged(double, double, double, double, double, double, std::optional<double>, std::optional<double>, std::optional<double>) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    WeakPtr<WebPageProxy> m_page;
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
