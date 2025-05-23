/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

DECLARE_SYSTEM_HEADER

#if USE(APPLE_INTERNAL_SDK)

#include <Security/SecAccessControlPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCode.h>
#include <Security/SecCodePriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecStaticCode.h>
#include <Security/SecTask.h>
#include <Security/SecTrustPriv.h>

#if PLATFORM(MAC)
#include <Security/keyTemplates.h>
#endif

#else

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>

#if __has_include(<Security/CSCommon.h>)
#include <Security/CSCommon.h>
#endif

typedef uint32_t SecSignatureHashAlgorithm;
enum {
    kSecSignatureHashAlgorithmUnknown = 0,
    kSecSignatureHashAlgorithmMD2 = 1,
    kSecSignatureHashAlgorithmMD4 = 2,
    kSecSignatureHashAlgorithmMD5 = 3,
    kSecSignatureHashAlgorithmSHA1 = 4,
    DeprecatedKSecSignatureHashAlgorithmSHA224 = 5,
    kSecSignatureHashAlgorithmSHA256 = 6,
    kSecSignatureHashAlgorithmSHA384 = 7,
    kSecSignatureHashAlgorithmSHA512 = 8
};

WTF_EXTERN_C_BEGIN

#if !__has_include(<Security/CSCommon.h>)
typedef struct CF_BRIDGED_TYPE(id) __SecCode const *SecStaticCodeRef;

typedef uint32_t SecCSFlags;
enum {
    kSecCSDefaultFlags = 0,
};
#endif

#if PLATFORM(IOS_FAMILY)
extern const CFStringRef kSecCodeInfoUnique;

OSStatus SecStaticCodeCreateWithPath(CFURLRef, SecCSFlags, SecStaticCodeRef * CF_RETURNS_RETAINED);
OSStatus SecCodeCopySigningInformation(SecStaticCodeRef, SecCSFlags, CFDictionaryRef * CF_RETURNS_RETAINED);
#endif

#if PLATFORM(MAC)
OSStatus SecTrustedApplicationCreateFromPath(const char* path, SecTrustedApplicationRef * CF_RETURNS_RETAINED);
#endif

SecSignatureHashAlgorithm SecCertificateGetSignatureHashAlgorithm(SecCertificateRef);
extern const CFStringRef kSecAttrNoLegacy;

extern const CFStringRef kSecAttrAlias;

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)

typedef struct CF_BRIDGED_TYPE(id) __SecTask *SecTaskRef;
typedef struct CF_BRIDGED_TYPE(id) __SecTrust *SecTrustRef;

WTF_EXTERN_C_BEGIN

SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef, audit_token_t);
SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef);
CFStringRef SecTaskCopySigningIdentifier(SecTaskRef, CFErrorRef *);
CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef, CFStringRef entitlement, CFErrorRef*);
uint32_t SecTaskGetCodeSignStatus(SecTaskRef);
SecIdentityRef SecIdentityCreate(CFAllocatorRef, SecCertificateRef, SecKeyRef);
SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef, CFDataRef, CFErrorRef*);
CFDataRef SecAccessControlCopyData(SecAccessControlRef);

CFDataRef SecKeyCopySubjectPublicKeyInfo(SecKeyRef);

OSStatus SecCodeValidateFileResource(SecStaticCodeRef, CFStringRef, CFDataRef, SecCSFlags);

#if PLATFORM(MAC)
#include <Security/SecAsn1Types.h>
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
extern const SecAsn1Template kSecAsn1AlgorithmIDTemplate[];
extern const SecAsn1Template kSecAsn1SubjectPublicKeyInfoTemplate[];
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

#if PLATFORM(COCOA)
CF_RETURNS_RETAINED CFDataRef SecTrustSerialize(SecTrustRef, CFErrorRef *);
CF_RETURNS_RETAINED SecTrustRef SecTrustDeserialize(CFDataRef serializedTrust, CFErrorRef *);
CF_RETURNS_RETAINED CFPropertyListRef SecTrustCopyPropertyListRepresentation(SecTrustRef, CFErrorRef *);
CF_RETURNS_RETAINED SecTrustRef SecTrustCreateFromPropertyListRepresentation(CFPropertyListRef trustPlist, CFErrorRef *);
#endif

CF_RETURNS_RETAINED CFDictionaryRef SecTrustCopyInfo(SecTrustRef);

OSStatus SecTrustSetClientAuditToken(SecTrustRef, CFDataRef);

extern const CFStringRef kSecTrustInfoExtendedValidationKey;
extern const CFStringRef kSecTrustInfoCompanyNameKey;
extern const CFStringRef kSecTrustInfoRevocationKey;

WTF_EXTERN_C_END
