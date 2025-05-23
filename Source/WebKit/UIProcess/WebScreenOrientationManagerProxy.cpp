/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebScreenOrientationManagerProxy.h"

#include "APIUIClient.h"
#include "MessageSenderInlines.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebScreenOrientationManagerMessages.h"
#include "WebScreenOrientationManagerProxyMessages.h"
#include <WebCore/Exception.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebScreenOrientationManagerProxy);

Ref<WebScreenOrientationManagerProxy> WebScreenOrientationManagerProxy::create(WebPageProxy& page, WebCore::ScreenOrientationType orientation)
{
    return adoptRef(*new WebScreenOrientationManagerProxy(page, orientation));
}

WebScreenOrientationManagerProxy::WebScreenOrientationManagerProxy(WebPageProxy& page, WebCore::ScreenOrientationType orientation)
    : m_page(page)
    , m_currentOrientation(orientation)
{
    protectedPage()->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebScreenOrientationManagerProxy::messageReceiverName(), m_page->webPageIDInMainFrameProcess(), *this);
}

WebScreenOrientationManagerProxy::~WebScreenOrientationManagerProxy()
{
    unlockIfNecessary();

    protectedPage()->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::WebScreenOrientationManagerProxy::messageReceiverName(), m_page->webPageIDInMainFrameProcess());
}

std::optional<SharedPreferencesForWebProcess> WebScreenOrientationManagerProxy::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

void WebScreenOrientationManagerProxy::currentOrientation(CompletionHandler<void(WebCore::ScreenOrientationType)>&& completionHandler)
{
    completionHandler(m_currentOrientation);
}

void WebScreenOrientationManagerProxy::setCurrentOrientation(WebCore::ScreenOrientationType orientation)
{
    if (m_currentOrientation == orientation)
        return;
    m_currentOrientation = orientation;

    if (!m_shouldSendChangeNotifications)
        return;

    protectedPage()->protectedLegacyMainFrameProcess()->send(Messages::WebScreenOrientationManager::OrientationDidChange(orientation), m_page->webPageIDInMainFrameProcess());
    if (m_currentLockRequest)
        m_currentLockRequest(std::nullopt);
}

static WebCore::ScreenOrientationType resolveScreenOrientationLockType(WebCore::ScreenOrientationType currentOrientation, WebCore::ScreenOrientationLockType lockType)
{
    switch (lockType) {
    case WebCore::ScreenOrientationLockType::Any:
        return currentOrientation;
    case WebCore::ScreenOrientationLockType::PortraitPrimary:
        return WebCore::ScreenOrientationType::PortraitPrimary;
    case WebCore::ScreenOrientationLockType::Landscape:
        if (WebCore::isLandscape(currentOrientation))
            return currentOrientation;
        return WebCore::ScreenOrientationType::LandscapePrimary;
    case WebCore::ScreenOrientationLockType::PortraitSecondary:
        return WebCore::ScreenOrientationType::PortraitSecondary;
    case WebCore::ScreenOrientationLockType::LandscapePrimary:
        return WebCore::ScreenOrientationType::LandscapePrimary;
    case WebCore::ScreenOrientationLockType::LandscapeSecondary:
        return WebCore::ScreenOrientationType::LandscapeSecondary;
    case WebCore::ScreenOrientationLockType::Natural: {
        auto naturalOrientation = WebCore::naturalScreenOrientationType();
        if (WebCore::isPortrait(naturalOrientation) == WebCore::isPortrait(currentOrientation))
            return currentOrientation;
        return naturalOrientation;
    }
    case WebCore::ScreenOrientationLockType::Portrait:
        break;
    }
    if (WebCore::isPortrait(currentOrientation))
        return currentOrientation;
    return WebCore::ScreenOrientationType::PortraitPrimary;
}

void WebScreenOrientationManagerProxy::lock(WebCore::ScreenOrientationLockType lockType, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (m_currentLockRequest)
        m_currentLockRequest(WebCore::Exception { WebCore::ExceptionCode::AbortError, "A new lock request was started"_s });

    if (auto exception = platformShouldRejectLockRequest()) {
        completionHandler(*exception);
        return;
    }

    m_currentLockRequest = WTFMove(completionHandler);
    auto resolvedLockedOrientation = resolveScreenOrientationLockType(m_currentOrientation, lockType);
    bool shouldOrientationChange = m_currentOrientation != resolvedLockedOrientation;

    if (resolvedLockedOrientation != m_currentlyLockedOrientation) {
        bool didLockOrientation = false;
        Ref page = m_page.get();
#if ENABLE(FULLSCREEN_API)
        if (CheckedPtr fullscreenManager = page->fullScreenManager(); fullscreenManager && fullscreenManager->isFullScreen()) {
            if (!fullscreenManager->lockFullscreenOrientation(resolvedLockedOrientation)) {
                m_currentLockRequest(WebCore::Exception { WebCore::ExceptionCode::NotSupportedError, "Screen orientation locking is not supported"_s });
                return;
            }
            didLockOrientation = true;
        }
#endif
        if (!didLockOrientation && !page->uiClient().lockScreenOrientation(page, resolvedLockedOrientation)) {
            m_currentLockRequest(WebCore::Exception { WebCore::ExceptionCode::NotSupportedError, "Screen orientation locking is not supported"_s });
            return;
        }
    }
    m_currentlyLockedOrientation = resolvedLockedOrientation;
    if (!shouldOrientationChange)
        m_currentLockRequest(std::nullopt);
}

void WebScreenOrientationManagerProxy::unlock()
{
    if (!m_currentlyLockedOrientation)
        return;

    if (m_currentLockRequest)
        m_currentLockRequest(WebCore::Exception { WebCore::ExceptionCode::AbortError, "Unlock request was received"_s });

    bool didUnlockOrientation = false;
    Ref page = m_page.get();
#if ENABLE(FULLSCREEN_API)
    if (CheckedPtr fullScreenManager = page->fullScreenManager(); fullScreenManager && fullScreenManager->isFullScreen()) {
        fullScreenManager->unlockFullscreenOrientation();
        didUnlockOrientation = true;
    }
#endif
    if (!didUnlockOrientation)
        page->uiClient().unlockScreenOrientation(page);

    m_currentlyLockedOrientation = std::nullopt;
}

void WebScreenOrientationManagerProxy::setShouldSendChangeNotification(bool shouldSend)
{
    m_shouldSendChangeNotifications = shouldSend;
}

void WebScreenOrientationManagerProxy::unlockIfNecessary()
{
    if (m_currentlyLockedOrientation)
        unlock();
    if (m_currentLockRequest)
        m_currentLockRequest(WebCore::Exception { WebCore::ExceptionCode::AbortError, "Screen lock request was aborted"_s });
}

#if !PLATFORM(IOS_FAMILY)
std::optional<WebCore::Exception> WebScreenOrientationManagerProxy::platformShouldRejectLockRequest() const
{
    return std::nullopt;
}
#endif

Ref<WebPageProxy> WebScreenOrientationManagerProxy::protectedPage() const
{
    return m_page.get();
}

} // namespace WebKit
