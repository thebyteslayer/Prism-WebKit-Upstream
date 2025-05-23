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

#include "config.h"

#if ENABLE(WEB_AUTHN)

#include "FidoTestData.h"
#include "PlatformUtilities.h"
#include <WebCore/CBORReader.h>
#include <WebCore/CBORValue.h>
#include <WebCore/CryptoAlgorithmAESCBC.h>
#include <WebCore/CryptoAlgorithmAesCbcCfbParams.h>
#include <WebCore/CryptoAlgorithmECDH.h>
#include <WebCore/CryptoKeyAES.h>
#include <WebCore/CryptoKeyEC.h>
#include <WebCore/CryptoKeyHMAC.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/Pin.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <WebCore/WebAuthenticationUtils.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {
using namespace WebCore;
using namespace cbor;
using namespace fido;
using namespace fido::pin;

TEST(CtapPinTest, TestValidateAndConvertToUTF8)
{
    // Failure cases
    auto result = validateAndConvertToUTF8("123"_s);
    EXPECT_FALSE(result);
    result = validateAndConvertToUTF8(emptyString());
    EXPECT_FALSE(result);
    result = validateAndConvertToUTF8("1234567812345678123456781234567812345678123456781234567812345678"_s);
    EXPECT_FALSE(result);

    // Success cases
    result = validateAndConvertToUTF8("1234"_s);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->length(), 4u);
    EXPECT_STREQ(result->data(), "1234");

    result = validateAndConvertToUTF8("123456781234567812345678123456781234567812345678123456781234567"_s);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->length(), 63u);
    EXPECT_STREQ(result->data(), "123456781234567812345678123456781234567812345678123456781234567");
}

TEST(CtapPinTest, TestSetPinRequest)
{
    // Generate an EC key pair as the peer key.
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    ASSERT_FALSE(keyPairResult.hasException());
    auto keyPair = keyPairResult.releaseReturnValue();

    String pin = "1234"_s;

    auto request = SetPinRequest::tryCreate(pin, downcast<CryptoKeyEC>(*keyPair.publicKey));
    EXPECT_TRUE(request);
    auto result = encodeAsCBOR(*request);

    EXPECT_EQ(result.size(), 187u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorClientPin));

    // Decode the CBOR binary to check if each field is encoded correctly.
    Vector<uint8_t> buffer;
    buffer.append(result.subspan(1));
    auto decodedResponse = cbor::CBORReader::read(buffer);
    EXPECT_TRUE(decodedResponse);
    EXPECT_TRUE(decodedResponse->isMap());
    const auto& responseMap = decodedResponse->getMap();

    const auto& it1 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kProtocol)));
    EXPECT_NE(it1, responseMap.end());
    EXPECT_EQ(it1->second.getInteger(), kProtocolVersion);

    const auto& it2 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kSubcommand)));
    EXPECT_NE(it2, responseMap.end());
    EXPECT_EQ(it2->second.getInteger(), static_cast<uint8_t>(Subcommand::kSetPin));

    // COSE
    auto it = responseMap.find(CBORValue(static_cast<int>(RequestKey::kKeyAgreement)));
    EXPECT_TRUE(decodedResponse);
    EXPECT_TRUE(decodedResponse->isMap());
    const auto& coseKey = it->second.getMap();

    const auto& it3 = coseKey.find(CBORValue(COSE::kty));
    EXPECT_NE(it3, coseKey.end());
    EXPECT_EQ(it3->second.getInteger(), COSE::EC2);

    const auto& it4 = coseKey.find(CBORValue(COSE::alg));
    EXPECT_NE(it4, coseKey.end());
    EXPECT_EQ(it4->second.getInteger(), COSE::ECDH256);

    const auto& it5 = coseKey.find(CBORValue(COSE::crv));
    EXPECT_NE(it5, coseKey.end());
    EXPECT_EQ(it5->second.getInteger(), COSE::P_256);

    // Check the cose key.
    const auto& xIt = coseKey.find(CBORValue(COSE::x));
    EXPECT_NE(xIt, coseKey.end());
    const auto& yIt = coseKey.find(CBORValue(COSE::y));
    EXPECT_NE(yIt, coseKey.end());
    auto cosePublicKey = CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, encodeRawPublicKey(xIt->second.getByteString(), yIt->second.getByteString()), true, CryptoKeyUsageDeriveBits);
    EXPECT_TRUE(cosePublicKey);

    // Check the encrypted Pin.
    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), *cosePublicKey);
    EXPECT_TRUE(sharedKeyResult);

    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(sharedKeyResult->span());
    auto sharedKeyHash = crypto->computeHash();

    auto aesKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(sharedKeyHash), true, CryptoKeyUsageDecrypt);
    EXPECT_TRUE(aesKey);

    const auto& it6 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kNewPinEnc)));
    EXPECT_NE(it6, responseMap.end());
    auto newPinEncResult = CryptoAlgorithmAESCBC::platformDecrypt({ }, *aesKey, it6->second.getByteString(), CryptoAlgorithmAESCBC::Padding::No);
    EXPECT_FALSE(newPinEncResult.hasException());
    auto newPinEnc = newPinEncResult.releaseReturnValue();

    constexpr std::array<uint8_t, 64> expectedNewPinEnc { 0x31, 0x32, 0x33, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    EXPECT_EQ(newPinEnc.size(), 64u);
    EXPECT_TRUE(equalSpans(newPinEnc.span(), std::span { expectedNewPinEnc }));

    String pin2 = "123"_s;
    auto request2 = SetPinRequest::tryCreate(pin2, downcast<CryptoKeyEC>(*keyPair.publicKey));
    EXPECT_FALSE(request2);

    String pin3 = "01234567891011121314151617181920212223242526272829303132333435363738394041424344454647484950"_s;
    auto request3 = SetPinRequest::tryCreate(pin3, downcast<CryptoKeyEC>(*keyPair.publicKey));
    EXPECT_FALSE(request3);
}

TEST(CtapPinTest, TestRetriesRequest)
{
    auto result = encodeAsCBOR(RetriesRequest { });
    EXPECT_EQ(result.size(), sizeof(TestData::kCtapClientPinRetries));
    EXPECT_TRUE(equalSpans(result.span(), std::span { TestData::kCtapClientPinRetries }));
}

TEST(CtapPinTest, TestRetriesResponse)
{
    // Failure cases
    auto result = RetriesResponse::parse({ });
    EXPECT_FALSE(result);

    constexpr std::array<uint8_t, 1> testData1 { 0x05 }; // wrong response code
    result = RetriesResponse::parse(std::span { testData1 });
    EXPECT_FALSE(result);

    constexpr std::array<uint8_t, 2> testData2 { 0x00, 0x00 }; // wrong CBOR map
    result = RetriesResponse::parse(std::span { testData2 });
    EXPECT_FALSE(result);

    result = RetriesResponse::parse(std::span { TestData::kCtapClientPinTokenResponse }); // wrong response
    EXPECT_FALSE(result);

    // Success cases
    result = RetriesResponse::parse(std::span { TestData::kCtapClientPinRetriesResponse });
    EXPECT_TRUE(result);
    EXPECT_EQ(result->retries, 8u);
}

TEST(CtapPinTest, TestKeyAgreementRequest)
{
    auto result = encodeAsCBOR(KeyAgreementRequest { });
    EXPECT_EQ(result.size(), sizeof(TestData::kCtapClientPinKeyAgreement));
    EXPECT_TRUE(equalSpans(result.span(), std::span { TestData::kCtapClientPinKeyAgreement }));
}

TEST(CtapPinTest, TestKeyAgreementResponse)
{
    // Failure cases
    auto result = KeyAgreementResponse::parse({ });
    EXPECT_FALSE(result);

    constexpr std::array<uint8_t, 1> testData1 { 0x05 }; // wrong response code
    result = KeyAgreementResponse::parse(std::span { testData1 });
    EXPECT_FALSE(result);

    constexpr std::array<uint8_t, 2> testData2 { 0x00, 0x00 }; // wrong CBOR map
    result = KeyAgreementResponse::parse(std::span { testData2 });
    EXPECT_FALSE(result);

    result = KeyAgreementResponse::parse(std::span { TestData::kCtapClientPinTokenResponse }); // wrong response
    EXPECT_FALSE(result);

// FIXME: When can we enable this for non-Cocoa platforms?
// FIXME: Can we enable this for watchOS and tvOS?
#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)
    result = KeyAgreementResponse::parse(std::span { TestData::kCtapClientPinInvalidKeyAgreementResponse }); // The point is not on the curve.
    EXPECT_FALSE(result);
#endif

    // Test COSE
    auto coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::kty)] = CBORValue(0); // wrong kty
    auto coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::alg)] = CBORValue(0); // wrong alg
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::crv)] = CBORValue(0); // wrong crv
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::x)] = CBORValue(0); // wrong x
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::y)] = CBORValue(0); // wrong y
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    // Success cases
    result = KeyAgreementResponse::parse(std::span { TestData::kCtapClientPinKeyAgreementResponse });
    EXPECT_TRUE(result);
    auto exportedRawKey = result->peerKey->exportRaw();
    EXPECT_FALSE(exportedRawKey.hasException());
    Vector<uint8_t> expectedRawKey;
    expectedRawKey.reserveCapacity(65);
    expectedRawKey.append(0x04);
    expectedRawKey.append(std::span { TestData::kCtapClientPinKeyAgreementResponse }.subspan(14, 32)); // X
    expectedRawKey.append(std::span { TestData::kCtapClientPinKeyAgreementResponse }.subspan(49, 32)); // Y
    EXPECT_TRUE(exportedRawKey.returnValue() == expectedRawKey);
}

TEST(CtapPinTest, TestTokenRequest)
{
    // Generate an EC key pair as the peer key.
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, true, CryptoKeyUsageDeriveBits);
    ASSERT_FALSE(keyPairResult.hasException());
    auto keyPair = keyPairResult.releaseReturnValue();

    CString pin = "1234";

    auto token = TokenRequest::tryCreate(pin, downcast<CryptoKeyEC>(*keyPair.publicKey));
    EXPECT_TRUE(token);
    auto result = encodeAsCBOR(*token);

    EXPECT_EQ(result.size(), 103u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorClientPin));

    // Decode the CBOR binary to check if each field is encoded correctly.
    Vector<uint8_t> buffer;
    buffer.append(result.subspan(1));
    auto decodedResponse = cbor::CBORReader::read(buffer);
    EXPECT_TRUE(decodedResponse);
    EXPECT_TRUE(decodedResponse->isMap());
    const auto& responseMap = decodedResponse->getMap();

    const auto& it1 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kProtocol)));
    EXPECT_NE(it1, responseMap.end());
    EXPECT_EQ(it1->second.getInteger(), kProtocolVersion);

    const auto& it2 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kSubcommand)));
    EXPECT_NE(it2, responseMap.end());
    EXPECT_EQ(it2->second.getInteger(), static_cast<uint8_t>(Subcommand::kGetPinToken));

    // COSE
    auto it = responseMap.find(CBORValue(static_cast<int>(RequestKey::kKeyAgreement)));
    EXPECT_TRUE(decodedResponse);
    EXPECT_TRUE(decodedResponse->isMap());
    const auto& coseKey = it->second.getMap();

    const auto& it3 = coseKey.find(CBORValue(COSE::kty));
    EXPECT_NE(it3, coseKey.end());
    EXPECT_EQ(it3->second.getInteger(), COSE::EC2);

    const auto& it4 = coseKey.find(CBORValue(COSE::alg));
    EXPECT_NE(it4, coseKey.end());
    EXPECT_EQ(it4->second.getInteger(), COSE::ECDH256);

    const auto& it5 = coseKey.find(CBORValue(COSE::crv));
    EXPECT_NE(it5, coseKey.end());
    EXPECT_EQ(it5->second.getInteger(), COSE::P_256);

    // Check the cose key.
    const auto& xIt = coseKey.find(CBORValue(COSE::x));
    EXPECT_NE(xIt, coseKey.end());
    const auto& yIt = coseKey.find(CBORValue(COSE::y));
    EXPECT_NE(yIt, coseKey.end());
    auto cosePublicKey = CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, encodeRawPublicKey(xIt->second.getByteString(), yIt->second.getByteString()), true, CryptoKeyUsageDeriveBits);
    EXPECT_TRUE(cosePublicKey);

    // Check the encrypted Pin.
    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), *cosePublicKey);
    EXPECT_TRUE(sharedKeyResult);

    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(sharedKeyResult->span());
    auto sharedKeyHash = crypto->computeHash();

    auto aesKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(sharedKeyHash), true, CryptoKeyUsageDecrypt);
    EXPECT_TRUE(aesKey);

    const auto& it6 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kPinHashEnc)));
    EXPECT_NE(it6, responseMap.end());
    auto pinHashResult = CryptoAlgorithmAESCBC::platformDecrypt({ }, *aesKey, it6->second.getByteString(), CryptoAlgorithmAESCBC::Padding::No);
    EXPECT_FALSE(pinHashResult.hasException());
    auto pinHash = pinHashResult.releaseReturnValue();
    constexpr std::array<uint8_t, 16> expectedPinHash { 0x03, 0xac, 0x67, 0x42, 0x16, 0xf3, 0xe1, 0x5c, 0x76, 0x1e, 0xe1, 0xa5, 0xe2, 0x55, 0xf0, 0x67 };
    EXPECT_EQ(pinHash.size(), 16u);
    EXPECT_TRUE(equalSpans(pinHash.span(), std::span { expectedPinHash }));
}

TEST(CtapPinTest, TestTokenResponse)
{
    constexpr std::array<uint8_t, 32> sharedKeyData {
        0x29, 0x9E, 0x65, 0xB8, 0xE7, 0x71, 0xB8, 0x1D,
        0xB1, 0xC4, 0x8D, 0xBE, 0xCE, 0x50, 0x2A, 0x84,
        0x05, 0x44, 0x7F, 0x46, 0x2D, 0xE6, 0x81, 0xFA,
        0xEF, 0x0A, 0x6C, 0x67, 0xA7, 0x2B, 0xB5, 0x0F, };
    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, std::span { sharedKeyData }, true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT_TRUE(sharedKey);

    // Failure cases
    auto result = TokenResponse::parse(*sharedKey, { });
    EXPECT_FALSE(result);

    constexpr std::array<uint8_t, 1> testData1 { 0x05 }; // wrong response code
    result = TokenResponse::parse(*sharedKey, std::span { testData1 });
    EXPECT_FALSE(result);

    constexpr std::array<uint8_t, 2> testData2 { 0x00, 0x00 }; // wrong CBOR map
    result = TokenResponse::parse(*sharedKey, std::span { testData2 });
    EXPECT_FALSE(result);

    result = TokenResponse::parse(*sharedKey, std::span { TestData::kCtapClientPinKeyAgreementResponse }); // wrong response
    EXPECT_FALSE(result);

    // Success cases
    result = TokenResponse::parse(*sharedKey, std::span { TestData::kCtapClientPinTokenResponse });
    EXPECT_TRUE(result);
    constexpr std::array<uint8_t, 16> expectedToken { 0x03, 0xac, 0x67, 0x42, 0x16, 0xf3, 0xe1, 0x5c, 0x76, 0x1e, 0xe1, 0xa5, 0xe2, 0x55, 0xf0, 0x67 };
    EXPECT_EQ(result->token().size(), 16u);
    EXPECT_TRUE(equalSpans(result->token().span(), std::span { expectedToken }));
}

TEST(CtapPinTest, TestPinAuth)
{
    // 1. Generate the token.
    constexpr std::array<uint8_t, 32> sharedKeyData {
        0x29, 0x9E, 0x65, 0xB8, 0xE7, 0x71, 0xB8, 0x1D,
        0xB1, 0xC4, 0x8D, 0xBE, 0xCE, 0x50, 0x2A, 0x84,
        0x05, 0x44, 0x7F, 0x46, 0x2D, 0xE6, 0x81, 0xFA,
        0xEF, 0x0A, 0x6C, 0x67, 0xA7, 0x2B, 0xB5, 0x0F, };
    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, std::span { sharedKeyData }, true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT_TRUE(sharedKey);
    auto result = TokenResponse::parse(*sharedKey, std::span { TestData::kCtapClientPinTokenResponse });
    ASSERT_TRUE(result);

    // 2. Generate the pinAuth.
    auto pinAuth = result->pinAuth(std::span { sharedKeyData }); // sharedKeyData pretends to be clientDataHash
    constexpr std::array<uint8_t, 16> expectedPinAuth { 0x0b, 0xec, 0x9d, 0xba, 0x69, 0xb0, 0x0f, 0x45, 0x0b, 0xec, 0x66, 0xb4, 0x75, 0x7f, 0x93, 0x85 };
    EXPECT_EQ(pinAuth.size(), 16u);
    EXPECT_TRUE(equalSpans(pinAuth.span(), std::span { expectedPinAuth }));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
