/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSNumericValue.h"
#include "CSSTransformComponent.h"

namespace WebCore {

class CSSFunctionValue;
class Document;

template<typename> class ExceptionOr;

class CSSSkew : public CSSTransformComponent {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CSSSkew);
public:
    static ExceptionOr<Ref<CSSSkew>> create(Ref<CSSNumericValue>, Ref<CSSNumericValue>);
    static ExceptionOr<Ref<CSSSkew>> create(Ref<const CSSFunctionValue>, Document&);

    const CSSNumericValue& ax() const { return m_ax.get(); }
    const CSSNumericValue& ay() const { return m_ay.get(); }
    
    ExceptionOr<void> setAx(Ref<CSSNumericValue>);
    ExceptionOr<void> setAy(Ref<CSSNumericValue>);

    void serialize(StringBuilder&) const final;
    ExceptionOr<Ref<DOMMatrix>> toMatrix() final;
    void setIs2D(bool) final { };

    CSSTransformType getType() const final { return CSSTransformType::Skew; }

    RefPtr<CSSValue> toCSSValue() const final;

private:
    CSSSkew(Ref<CSSNumericValue> ax, Ref<CSSNumericValue> ay);

    Ref<CSSNumericValue> m_ax;
    Ref<CSSNumericValue> m_ay;
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSSkew)
    static bool isType(const WebCore::CSSTransformComponent& transform) { return transform.getType() == WebCore::CSSTransformType::Skew; }
SPECIALIZE_TYPE_TRAITS_END()
