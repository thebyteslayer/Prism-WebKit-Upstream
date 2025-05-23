/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include <wtf/TimeWithDynamicClockType.h>

#include <cmath>
#include <wtf/Condition.h>
#include <wtf/PrintStream.h>
#include <wtf/Lock.h>

namespace WTF {

TimeWithDynamicClockType TimeWithDynamicClockType::now(ClockType type)
{
    switch (type) {
    case ClockType::Wall:
        return WallTime::now();
    case ClockType::Monotonic:
        return MonotonicTime::now();
    case ClockType::Approximate:
        return ApproximateTime::now();
    case ClockType::Continuous:
        return ContinuousTime::now();
    case ClockType::ContinuousApproximate:
        return ContinuousApproximateTime::now();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return TimeWithDynamicClockType();
}

TimeWithDynamicClockType TimeWithDynamicClockType::nowWithSameClock() const
{
    return now(clockType());
}

WallTime TimeWithDynamicClockType::wallTime() const
{
    RELEASE_ASSERT(m_type == ClockType::Wall);
    return WallTime::fromRawSeconds(m_value);
}

MonotonicTime TimeWithDynamicClockType::monotonicTime() const
{
    RELEASE_ASSERT(m_type == ClockType::Monotonic);
    return MonotonicTime::fromRawSeconds(m_value);
}

ApproximateTime TimeWithDynamicClockType::approximateTime() const
{
    RELEASE_ASSERT(m_type == ClockType::Approximate);
    return ApproximateTime::fromRawSeconds(m_value);
}

ContinuousTime TimeWithDynamicClockType::continuousTime() const
{
    RELEASE_ASSERT(m_type == ClockType::Continuous);
    return ContinuousTime::fromRawSeconds(m_value);
}

ContinuousApproximateTime TimeWithDynamicClockType::continuousApproximateTime() const
{
    RELEASE_ASSERT(m_type == ClockType::ContinuousApproximate);
    return ContinuousApproximateTime::fromRawSeconds(m_value);
}

WallTime TimeWithDynamicClockType::approximateWallTime() const
{
    switch (m_type) {
    case ClockType::Wall:
        return wallTime();
    case ClockType::Monotonic:
        return monotonicTime().approximateWallTime();
    case ClockType::Approximate:
        return approximateTime().approximateWallTime();
    case ClockType::Continuous:
        return continuousTime().approximateWallTime();
    case ClockType::ContinuousApproximate:
        return ContinuousApproximateTime().approximateWallTime();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return WallTime();
}

MonotonicTime TimeWithDynamicClockType::approximateMonotonicTime() const
{
    switch (m_type) {
    case ClockType::Wall:
        return wallTime().approximateMonotonicTime();
    case ClockType::Monotonic:
        return monotonicTime();
    case ClockType::Approximate:
        return approximateTime().approximateMonotonicTime();
    case ClockType::Continuous:
        return continuousTime().approximateMonotonicTime();
    case ClockType::ContinuousApproximate:
        return ContinuousApproximateTime().approximateMonotonicTime();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return MonotonicTime();
}

Seconds TimeWithDynamicClockType::operator-(const TimeWithDynamicClockType& other) const
{
    RELEASE_ASSERT(m_type == other.m_type);
    return Seconds(m_value - other.m_value);
}

std::partial_ordering operator<=>(const TimeWithDynamicClockType& a, const TimeWithDynamicClockType& b)
{
    RELEASE_ASSERT(a.m_type == b.m_type);
    return a.m_value <=> b.m_value;
}

void TimeWithDynamicClockType::dump(PrintStream& out) const
{
    out.print(m_type, "(", m_value, " sec)");
}

void sleep(const TimeWithDynamicClockType& time)
{
    Lock fakeLock;
    Condition fakeCondition;
    Locker fakeLocker { fakeLock };
    fakeCondition.waitUntil(fakeLock, time);
}

bool hasElapsed(const TimeWithDynamicClockType& time)
{
    // Avoid doing now().
    if (!(time > time.withSameClockAndRawSeconds(0)))
        return true;
    if (time.secondsSinceEpoch().isInfinity())
        return false;
    
    return time <= time.nowWithSameClock();
}

} // namespace WTF


