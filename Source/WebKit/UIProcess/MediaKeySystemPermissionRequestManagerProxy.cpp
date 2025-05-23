/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "MediaKeySystemPermissionRequestManagerProxy.h"

#include "APISecurityOrigin.h"
#include "APIUIClient.h"
#include "DeviceIdHashSaltStorage.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "WebAutomationSession.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcess.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/MediaKeySystemRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

#if !RELEASE_LOG_DISABLED
static ASCIILiteral logClassName()
{
    return "MediaKeySystemPermissionRequestManagerProxy"_s;
}

static WTFLogChannel& logChannel()
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, EME);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaKeySystemPermissionRequestManagerProxy);

const Logger& MediaKeySystemPermissionRequestManagerProxy::logger() const
{
    return Ref { m_page.get() }->logger();
}
#endif

MediaKeySystemPermissionRequestManagerProxy::MediaKeySystemPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
}

MediaKeySystemPermissionRequestManagerProxy::~MediaKeySystemPermissionRequestManagerProxy()
{
    invalidatePendingRequests();
}

void MediaKeySystemPermissionRequestManagerProxy::invalidatePendingRequests()
{
    for (auto& request : m_pendingRequests.values())
        request->invalidate();

    m_pendingRequests.clear();
}

void MediaKeySystemPermissionRequestManagerProxy::denyRequest(MediaKeySystemPermissionRequestProxy& request, const String& message)
{
    Ref page = m_page.get();
    if (!page->hasRunningProcess())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, request.mediaKeySystemID().toUInt64(), ", reason: ", message);

#if ENABLE(ENCRYPTED_MEDIA)
    page->protectedLegacyMainFrameProcess()->send(Messages::WebPage::MediaKeySystemWasDenied(request.mediaKeySystemID(), message), page->webPageIDInMainFrameProcess());
#else
    UNUSED_PARAM(message);
#endif
}

void MediaKeySystemPermissionRequestManagerProxy::grantRequest(MediaKeySystemPermissionRequestProxy& request)
{
    Ref page = m_page.get();
    if (!page->hasRunningProcess())
        return;

#if ENABLE(ENCRYPTED_MEDIA)
    ALWAYS_LOG(LOGIDENTIFIER, request.mediaKeySystemID().toUInt64(), ", keySystem: ", request.keySystem());

    page->protectedLegacyMainFrameProcess()->send(Messages::WebPage::MediaKeySystemWasGranted { request.mediaKeySystemID(), request.mediaKeysHashSalt() }, page->webPageIDInMainFrameProcess());
#else
    UNUSED_PARAM(request);
#endif
}

void MediaKeySystemPermissionRequestManagerProxy::createRequestForFrame(MediaKeySystemRequestIdentifier mediaKeySystemID, FrameIdentifier frameID, Ref<SecurityOrigin>&& mediaKeyRequestOrigin, Ref<SecurityOrigin>&& topLevelDocumentOrigin, const String& keySystem, CompletionHandler<void(Ref<MediaKeySystemPermissionRequestProxy>&&)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, mediaKeySystemID.toUInt64());
    Ref page = m_page.get();
    auto request = MediaKeySystemPermissionRequestProxy::create(*this, mediaKeySystemID, page->mainFrame()->frameID(), frameID, WTFMove(mediaKeyRequestOrigin), WTFMove(topLevelDocumentOrigin), keySystem);
    m_pendingRequests.add(mediaKeySystemID, request.ptr());

    Ref mediaKeyRequestDocumentSecurityOrigin = request->mediaKeyRequestSecurityOrigin();
    Ref topLevelDocumentSecurityOrigin = request->topLevelDocumentSecurityOrigin();
    page->protectedWebsiteDataStore()->ensureProtectedDeviceIdHashSaltStorage()->deviceIdHashSaltForOrigin(mediaKeyRequestDocumentSecurityOrigin, topLevelDocumentSecurityOrigin, [request = WTFMove(request), completionHandler = WTFMove(completionHandler)] (String&& mediaKeysHashSalt) mutable {
        request->setMediaKeysHashSalt(WTFMove(mediaKeysHashSalt));
        completionHandler(WTFMove(request));
    });
}

void MediaKeySystemPermissionRequestManagerProxy::ref() const
{
    m_page->ref();
}

void MediaKeySystemPermissionRequestManagerProxy::deref() const
{
    m_page->deref();
}

} // namespace WebKit
