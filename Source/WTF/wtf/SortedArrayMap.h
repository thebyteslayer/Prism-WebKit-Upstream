/*

Copyright (C) 2021 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include <ranges>
#include <wtf/IndexedRange.h>
#include <wtf/text/StringView.h>

namespace WTF {

// SortedArrayMap is a map like HashMap, but it's read-only. It uses much less memory than HashMap.
// It uses binary search instead of hashing, so can be outperformed by HashMap for large maps.
// The array passed to the constructor has std::pair elements: keys first and values second.
// The array and the SortedArrayMap should typically both be global constant expressions.

class SortedArrayBase {
protected:
    // Some informal empirical tests indicate that arrays shorter than this are faster to
    // search with linear search than with binary search. Even if we don't get this threshold
    // exactly right, it's helpful for both performance and code size to use linear search at
    // least for very small arrays, and important for performance to make sure that we use
    // binary search for much larger ones.
    static constexpr size_t binarySearchThreshold = 20;
};

template<typename T>
using ArrayElementType = std::remove_reference_t<decltype(*std::begin(std::declval<T&>()))>;

template<typename ArrayType> class SortedArrayMap : public SortedArrayBase {
public:
    using ElementType = ArrayElementType<ArrayType>;
    using ValueType = typename ElementType::second_type;

    constexpr SortedArrayMap(const ArrayType&);
    template<typename KeyArgument> bool contains(const KeyArgument&) const;

    // FIXME: To match HashMap interface better, would be nice to get the default value from traits.
    template<typename KeyArgument> ValueType get(const KeyArgument&, const ValueType& defaultValue = { }) const;

    // FIXME: Should add a function like this to HashMap so the two kinds of maps are more interchangable.
    template<typename KeyArgument> const ValueType* tryGet(const KeyArgument&) const;

private:
    const ArrayType& m_array;
};

template<typename ArrayType> class SortedArraySet : public SortedArrayBase {
public:
    constexpr SortedArraySet(const ArrayType&);
    template<typename KeyArgument> bool contains(const KeyArgument&) const;

private:
    const ArrayType& m_array;
};

struct ComparableStringView {
    StringView string;
};

template<typename SortedArrayKeyType> struct SortedArrayKeyTraits {
    static std::optional<SortedArrayKeyType> parse(const SortedArrayKeyType& key) { return key; }
};

// NoUppercaseLettersOptimized means no characters with the 0x20 bit set.
// That means the strings can't include control characters, uppercase letters, or any of @[\]_.
enum class ASCIISubset : uint8_t { All, NoUppercaseLetters, NoUppercaseLettersOptimized };

template<ASCIISubset> struct ComparableASCIISubsetLiteral {
    ASCIILiteral literal;
    constexpr ComparableASCIISubsetLiteral(ASCIILiteral);
};

template<ASCIISubset subset> constexpr bool operator==(ComparableASCIISubsetLiteral<subset>, ComparableASCIISubsetLiteral<subset>);
template<ASCIISubset subset> constexpr bool operator<(ComparableASCIISubsetLiteral<subset>, ComparableASCIISubsetLiteral<subset>);

using ComparableASCIILiteral = ComparableASCIISubsetLiteral<ASCIISubset::All>;
using ComparableCaseFoldingASCIILiteral = ComparableASCIISubsetLiteral<ASCIISubset::NoUppercaseLetters>;
using ComparableLettersLiteral = ComparableASCIISubsetLiteral<ASCIISubset::NoUppercaseLettersOptimized>;

bool operator==(ComparableStringView, ComparableASCIILiteral);
bool operator==(ComparableStringView, ComparableCaseFoldingASCIILiteral);
bool operator==(ComparableStringView, ComparableLettersLiteral);
bool operator<(ComparableStringView, ComparableASCIILiteral);
bool operator<(ComparableStringView, ComparableCaseFoldingASCIILiteral);
bool operator<(ComparableStringView, ComparableLettersLiteral);
bool operator<(ComparableASCIILiteral, ComparableStringView);
bool operator<(ComparableCaseFoldingASCIILiteral, ComparableStringView);
bool operator<(ComparableLettersLiteral, ComparableStringView);

template<typename StorageInteger, ASCIISubset> class PackedASCIISubsetLiteral {
public:
    static_assert(std::is_unsigned_v<StorageInteger>);

    constexpr PackedASCIISubsetLiteral(ASCIILiteral);
    constexpr StorageInteger value() const { return m_value; }

    template<typename CharacterType> static std::optional<PackedASCIISubsetLiteral> parse(std::span<const CharacterType>);

private:
    static constexpr StorageInteger pack(ASCIILiteral);
    explicit constexpr PackedASCIISubsetLiteral(StorageInteger);
    StorageInteger m_value { 0 };
};

template<typename StorageInteger, ASCIISubset subset> constexpr bool operator==(PackedASCIISubsetLiteral<StorageInteger, subset>, PackedASCIISubsetLiteral<StorageInteger, subset>);
template<typename StorageInteger, ASCIISubset subset> constexpr bool operator<(PackedASCIISubsetLiteral<StorageInteger, subset>, PackedASCIISubsetLiteral<StorageInteger, subset>);

template<typename StorageInteger> using PackedASCIILiteral = PackedASCIISubsetLiteral<StorageInteger, ASCIISubset::All>;
template<typename StorageInteger> using PackedASCIILowerCodes = PackedASCIISubsetLiteral<StorageInteger, ASCIISubset::NoUppercaseLetters>;
template<typename StorageInteger> using PackedLettersLiteral = PackedASCIISubsetLiteral<StorageInteger, ASCIISubset::NoUppercaseLettersOptimized>;

template<ASCIISubset subset> constexpr bool isInSubset(char character)
{
    if (!(character && isASCII(character)))
        return false;
    switch (subset) {
    case ASCIISubset::All:
        return true;
    case ASCIISubset::NoUppercaseLetters:
        return !isASCIIUpper(character);
    case ASCIISubset::NoUppercaseLettersOptimized:
        return character == toASCIILowerUnchecked(character);
    }
}

template<ASCIISubset subset, typename CharacterType> constexpr std::make_unsigned_t<CharacterType> foldForComparison(CharacterType character)
{
    switch (subset) {
    case ASCIISubset::All:
        return character;
    case ASCIISubset::NoUppercaseLetters:
        return toASCIILower(character);
    case ASCIISubset::NoUppercaseLettersOptimized:
        return toASCIILowerUnchecked(character);
    }
}

template<ASCIISubset subset> constexpr ComparableASCIISubsetLiteral<subset>::ComparableASCIISubsetLiteral(ASCIILiteral inputLiteral)
    : literal { inputLiteral }
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(std::ranges::all_of(literal.span(), isInSubset<subset>));
}

template<typename ArrayType> constexpr SortedArrayMap<ArrayType>::SortedArrayMap(const ArrayType& array)
    : m_array { array }
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(std::is_sorted(std::begin(array), std::end(array), [](auto& a, auto b) {
        return a.first < b.first;
    }));
}

template<typename ArrayType> template<typename KeyArgument> inline auto SortedArrayMap<ArrayType>::tryGet(const KeyArgument& key) const -> const ValueType*
{
    using KeyType = typename ElementType::first_type;
    auto parsedKey = SortedArrayKeyTraits<KeyType>::parse(key);
    if (!parsedKey)
        return nullptr;
    decltype(std::begin(m_array)) iterator;
    if (std::size(m_array) < binarySearchThreshold) {
        iterator = std::find_if(std::begin(m_array), std::end(m_array), [&parsedKey](auto& pair) {
            return pair.first == *parsedKey;
        });
        if (iterator == std::end(m_array))
            return nullptr;
    } else {
        iterator = std::lower_bound(std::begin(m_array), std::end(m_array), *parsedKey, [](auto& pair, auto& value) {
            return pair.first < value;
        });
        if (iterator == std::end(m_array) || !(iterator->first == *parsedKey))
            return nullptr;
    }
    return &iterator->second;
}

template<typename ArrayType> template<typename KeyArgument> inline auto SortedArrayMap<ArrayType>::get(const KeyArgument& key, const ValueType& defaultValue) const -> ValueType
{
    auto result = tryGet(key);
    return result ? *result : defaultValue;
}

template<typename ArrayType> template<typename KeyArgument> inline bool SortedArrayMap<ArrayType>::contains(const KeyArgument& key) const
{
    return tryGet(key);
}

template<typename ArrayType> constexpr SortedArraySet<ArrayType>::SortedArraySet(const ArrayType& array)
    : m_array { array }
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(std::is_sorted(std::begin(array), std::end(array)));
}

template<typename ArrayType> template<typename KeyArgument> inline bool SortedArraySet<ArrayType>::contains(const KeyArgument& key) const
{
    using KeyType = typename std::remove_extent_t<ArrayType>;
    auto parsedKey = SortedArrayKeyTraits<KeyType>::parse(key);
    if (!parsedKey)
        return false;
    if (std::size(m_array) < binarySearchThreshold)
        return std::find(std::begin(m_array), std::end(m_array), *parsedKey) != std::end(m_array);
    auto iterator = std::lower_bound(std::begin(m_array), std::end(m_array), *parsedKey);
    return iterator != std::end(m_array) && *iterator == *parsedKey;
}

constexpr int compareSpansConstExpr(std::span<const char> a, std::span<const char> b)
{
    auto commonLength = std::min(a.size(), b.size());
    size_t i = 0;
    while (i < commonLength && a[i] == b[i])
        ++i;
    if (i == commonLength) {
        if (a.size() == b.size())
            return 0;
        return a.size() < b.size() ? -1 : 1;
    }
    auto aCharacter = a[i];
    auto bCharacter = b[i];
    return aCharacter == bCharacter ? 0 : aCharacter < bCharacter ? -1 : 1;
}

template<typename CharacterType> inline bool lessThanASCIICaseFolding(std::span<const CharacterType> characters, ASCIILiteral literalWithNoUppercase)
{
    for (auto [index, character] : indexedRange(characters.first(std::min(characters.size(), literalWithNoUppercase.length())))) {
        auto literalCharacter = literalWithNoUppercase[index];
        auto lowercaseCharacter = toASCIILower(character);
        if (lowercaseCharacter != literalCharacter)
            return lowercaseCharacter < literalCharacter;
    }
    return literalWithNoUppercase.length() < characters.size();
}

inline bool lessThanASCIICaseFolding(StringView string, ASCIILiteral literalWithNoUppercase)
{
    if (string.is8Bit())
        return lessThanASCIICaseFolding(string.span8(), literalWithNoUppercase);
    return lessThanASCIICaseFolding(string.span16(), literalWithNoUppercase);
}

template<typename CharacterType> inline bool lessThanASCIICaseFolding(ASCIILiteral literalWithNoUppercase, std::span<const CharacterType> characters)
{
    for (auto [index, character] : indexedRange(characters.first(std::min(characters.size(), literalWithNoUppercase.length())))) {
        auto literalCharacter = literalWithNoUppercase[index];
        auto lowercaseCharacter = toASCIILower(character);
        if (lowercaseCharacter != literalCharacter)
            return literalCharacter < lowercaseCharacter;
    }
    return literalWithNoUppercase.length() < characters.size();
}

inline bool lessThanASCIICaseFolding(ASCIILiteral literalWithNoUppercase, StringView string)
{
    if (string.is8Bit())
        return lessThanASCIICaseFolding(literalWithNoUppercase, string.span8());
    return lessThanASCIICaseFolding(literalWithNoUppercase, string.span16());
}

template<ASCIISubset subset> constexpr bool operator==(ComparableASCIISubsetLiteral<subset> a, ComparableASCIISubsetLiteral<subset> b)
{
    return !compareSpansConstExpr(a.literal.span(), b.literal.span());
}

template<ASCIISubset subset> constexpr bool operator<(ComparableASCIISubsetLiteral<subset> a, ComparableASCIISubsetLiteral<subset> b)
{
    return compareSpansConstExpr(a.literal.span(), b.literal.span()) < 0;
}

inline bool operator==(ComparableStringView a, ComparableASCIILiteral b)
{
    return a.string == b.literal;
}

inline bool operator<(ComparableStringView a, ComparableASCIILiteral b)
{
    return codePointCompare(a.string, b.literal) < 0;
}

inline bool operator<(ComparableASCIILiteral a, ComparableStringView b)
{
    return codePointCompare(a.literal, b.string) < 0;
}

inline bool operator==(ComparableStringView a, ComparableLettersLiteral b)
{
    return equalLettersIgnoringASCIICaseCommon(a.string, b.literal);
}

inline bool operator<(ComparableStringView a, ComparableLettersLiteral b)
{
    return lessThanASCIICaseFolding(a.string, b.literal);
}

inline bool operator<(ComparableLettersLiteral a, ComparableStringView b)
{
    return lessThanASCIICaseFolding(a.literal, b.string);
}

inline bool operator==(ComparableStringView a, ComparableCaseFoldingASCIILiteral b)
{
    return equalIgnoringASCIICase(a.string, b.literal);
}

inline bool operator<(ComparableStringView a, ComparableCaseFoldingASCIILiteral b)
{
    return lessThanASCIICaseFolding(a.string, b.literal);
}

inline bool operator<(ComparableCaseFoldingASCIILiteral a, ComparableStringView b)
{
    return lessThanASCIICaseFolding(a.literal, b.string);
}

template<typename StorageInteger, ASCIISubset subset> constexpr PackedASCIISubsetLiteral<StorageInteger, subset>::PackedASCIISubsetLiteral(ASCIILiteral string)
    : m_value { pack(string) }
{
}

template<typename StorageInteger, ASCIISubset subset> constexpr PackedASCIISubsetLiteral<StorageInteger, subset>::PackedASCIISubsetLiteral(StorageInteger value)
    : m_value { value }
{
}

template<typename StorageInteger, ASCIISubset subset> constexpr StorageInteger PackedASCIISubsetLiteral<StorageInteger, subset>::pack(ASCIILiteral string)
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(string.length());
    ASSERT_UNDER_CONSTEXPR_CONTEXT(string.length() <= sizeof(StorageInteger));
    StorageInteger result = 0;
    for (unsigned index = 0; index < string.length(); ++index) {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(isInSubset<subset>(string[index]));
        StorageInteger code = static_cast<uint8_t>(string[index]);
        result |= code << ((sizeof(StorageInteger) - index - 1) * 8);
    }
    return result;
}

template<typename StorageInteger, ASCIISubset subset> template<typename CharacterType> auto PackedASCIISubsetLiteral<StorageInteger, subset>::parse(std::span<const CharacterType> span) -> std::optional<PackedASCIISubsetLiteral>
{
    if (span.size() > sizeof(StorageInteger))
        return std::nullopt;
    StorageInteger result = 0;
    for (unsigned index = 0; index < span.size(); ++index) {
        auto code = span[index];
        if (!isASCII(code))
            return std::nullopt;
        result |= static_cast<StorageInteger>(foldForComparison<subset>(code)) << ((sizeof(StorageInteger) - index - 1) * 8);
    }
    return PackedASCIISubsetLiteral(result);
}

template<typename StorageInteger, ASCIISubset subset> constexpr bool operator==(PackedASCIISubsetLiteral<StorageInteger, subset> a, PackedASCIISubsetLiteral<StorageInteger, subset> b)
{
    return a.value() == b.value();
}

template<typename StorageInteger, ASCIISubset subset> constexpr bool operator<(PackedASCIISubsetLiteral<StorageInteger, subset> a, PackedASCIISubsetLiteral<StorageInteger, subset> b)
{
    return a.value() < b.value();
}

template<ASCIISubset subset> struct SortedArrayKeyTraits<ComparableASCIISubsetLiteral<subset>> {
    static std::optional<ComparableStringView> parse(StringView string)
    {
        return { { string } };
    }
};

template<typename StorageInteger, ASCIISubset subset> struct SortedArrayKeyTraits<PackedASCIISubsetLiteral<StorageInteger, subset>> {
    template<typename CharacterType> static std::optional<PackedASCIISubsetLiteral<StorageInteger, subset>> parse(std::span<const CharacterType> span)
    {
        return PackedASCIISubsetLiteral<StorageInteger, subset>::parse(span);
    }
    static std::optional<PackedASCIISubsetLiteral<StorageInteger, subset>> parse(StringView string)
    {
        return string.is8Bit() ? parse(string.span8()) : parse(string.span16());
    }
};

template<typename ValueType> constexpr std::optional<ValueType> makeOptionalFromPointer(const ValueType* pointer)
{
    if (!pointer)
        return std::nullopt;
    return *pointer;
}

}

// FIXME: Rename the Comparable and Packed types for clarity and to align them better with each other.
using WTF::ASCIISubset;
using WTF::ComparableASCIILiteral;
using WTF::ComparableASCIISubsetLiteral;
using WTF::ComparableCaseFoldingASCIILiteral;
using WTF::ComparableLettersLiteral;
using WTF::PackedASCIILiteral;
using WTF::PackedASCIILowerCodes;
using WTF::PackedLettersLiteral;
using WTF::SortedArrayMap;
using WTF::SortedArraySet;
using WTF::makeOptionalFromPointer;
