/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include "RegisteredEventListener.h"
#include <atomic>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomString.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThread.h"
#endif

namespace WebCore {

class EventTarget;

using EventListenerVector = Vector<RefPtr<RegisteredEventListener>, 1, CrashOnOverflow, 2>;

class EventListenerMap {
public:
    WEBCORE_EXPORT EventListenerMap();

    bool isEmpty() const { return m_entries.isEmpty(); }
    bool contains(const AtomString& eventType) const { return find(eventType); }
    bool containsCapturing(const AtomString& eventType) const;
    bool containsActive(const AtomString& eventType) const;

    void clear();
    void clearEntriesForTearDown()
    {
        releaseAssertOrSetThreadUID();
        m_entries.clear();
    }

    void replace(const AtomString& eventType, EventListener& oldListener, Ref<EventListener>&& newListener, const RegisteredEventListener::Options&);
    bool add(const AtomString& eventType, Ref<EventListener>&&, const RegisteredEventListener::Options&);
    bool remove(const AtomString& eventType, EventListener&, bool useCapture);
    WEBCORE_EXPORT EventListenerVector* find(const AtomString& eventType);
    const EventListenerVector* find(const AtomString& eventType) const { return const_cast<EventListenerMap*>(this)->find(eventType); }
    Vector<AtomString> eventTypes() const;

    template<typename CallbackType>
    void enumerateEventListenerTypes(NOESCAPE const CallbackType& callback) const
    {
        for (auto& entry : m_entries)
            callback(entry.first, entry.second.size());
    }

    template<typename CallbackType>
    bool containsMatchingEventListener(NOESCAPE const CallbackType& callback) const
    {
        for (auto& entry : m_entries) {
            if (callback(entry.first, m_entries))
                return true;
        }
        return false;
    }

    void removeFirstEventListenerCreatedFromMarkup(const AtomString& eventType);
    void copyEventListenersNotCreatedFromMarkupToTarget(EventTarget*);
    
    template<typename Visitor> void visitJSEventListeners(Visitor&);
    Lock& lock() { return m_lock; }

private:
    void releaseAssertOrSetThreadUID()
    {
#if PLATFORM(IOS_FAMILY)
        if (WebThreadIsEnabled())
            return;
#endif
        if (m_threadUID)
            RELEASE_ASSERT(m_threadUID == Thread::currentSingleton().uid());
        else
            m_threadUID = Thread::currentSingleton().uid();
    }

    Vector<std::pair<AtomString, EventListenerVector>, 0, CrashOnOverflow, 4> m_entries;
    Lock m_lock;
    uint32_t m_threadUID { 0 };
};

template<typename Visitor>
void EventListenerMap::visitJSEventListeners(Visitor& visitor)
{
    Locker locker { m_lock };
    for (auto& entry : m_entries) {
        for (auto& eventListener : entry.second)
            eventListener->callback().visitJSFunction(visitor);
    }
}

} // namespace WebCore
