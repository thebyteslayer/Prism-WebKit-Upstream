/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <CommonCrypto/CommonCryptoError.h>
#include <wtf/Vector.h>

typedef uint32_t CCDigestAlgorithm;
typedef uint32_t CCHmacAlgorithm;
typedef uint32_t CCOperation;

namespace WebCore {

template<typename> class ExceptionOr;

ExceptionOr<Vector<uint8_t>> transformAESCTR(CCOperation, const Vector<uint8_t>& counter, size_t counterLength, const Vector<uint8_t>& key, std::span<const uint8_t> data);
CCStatus keyDerivationHMAC(CCDigestAlgorithm, std::span<const uint8_t> keyDerivationKey, std::span<const uint8_t> context, std::span<const uint8_t> salt, Vector<uint8_t>& derivedKey);
ExceptionOr<Vector<uint8_t>> deriveHDKFBits(CCDigestAlgorithm, std::span<const uint8_t> key, std::span<const uint8_t> salt, std::span<const uint8_t> info, size_t length);
ExceptionOr<Vector<uint8_t>> deriveHDKFSHA256Bits(std::span<const uint8_t> key, std::span<const uint8_t> salt, std::span<const uint8_t> info, size_t length);
Vector<uint8_t> calculateHMACSignature(CCHmacAlgorithm, const Vector<uint8_t>& key, std::span<const uint8_t> data);
Vector<uint8_t> calculateSHA256Signature(const Vector<uint8_t>& key, std::span<const uint8_t> data);

} // namespace WebCore
