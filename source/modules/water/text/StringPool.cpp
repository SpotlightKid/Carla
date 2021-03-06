/*
  ==============================================================================

   This file is part of the Water library.
   Copyright (c) 2016 ROLI Ltd.
   Copyright (C) 2017 Filipe Coelho <falktx@falktx.com>

   Permission is granted to use this software under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license/

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
   OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
   USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
   TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
   OF THIS SOFTWARE.

  ==============================================================================
*/

#include "StringPool.h"
#include "../misc/Time.h"

namespace water {

static const int minNumberOfStringsForGarbageCollection = 300;
static const uint32 garbageCollectionInterval = 30000;


StringPool::StringPool() noexcept  : lastGarbageCollectionTime (0) {}
StringPool::~StringPool() {}

struct StartEndString
{
    StartEndString (String::CharPointerType s, String::CharPointerType e) noexcept : start (s), end (e) {}
    operator String() const   { return String (start, end); }

    String::CharPointerType start, end;
};

static int compareStrings (const String& s1, const String& s2) noexcept     { return s1.compare (s2); }
static int compareStrings (CharPointer_UTF8 s1, const String& s2) noexcept  { return s1.compare (s2.getCharPointer()); }

static int compareStrings (const StartEndString& string1, const String& string2) noexcept
{
    String::CharPointerType s1 (string1.start), s2 (string2.getCharPointer());

    for (;;)
    {
        const int c1 = s1 < string1.end ? (int) s1.getAndAdvance() : 0;
        const int c2 = (int) s2.getAndAdvance();
        const int diff = c1 - c2;

        if (diff != 0)  return diff < 0 ? -1 : 1;
        if (c1 == 0)    break;
    }

    return 0;
}

template <typename NewStringType>
static String addPooledString (Array<String>& strings, const NewStringType& newString)
{
    int start = 0;
    int end = strings.size();

    while (start < end)
    {
        const String& startString = strings.getReference (start);
        const int startComp = compareStrings (newString, startString);

        if (startComp == 0)
            return startString;

        const int halfway = (start + end) / 2;

        if (halfway == start)
        {
            if (startComp > 0)
                ++start;

            break;
        }

        const String& halfwayString = strings.getReference (halfway);
        const int halfwayComp = compareStrings (newString, halfwayString);

        if (halfwayComp == 0)
            return halfwayString;

        if (halfwayComp > 0)
            start = halfway;
        else
            end = halfway;
    }

    strings.insert (start, newString);
    return strings.getReference (start);
}

String StringPool::getPooledString (const char* const newString)
{
    if (newString == nullptr || *newString == 0)
        return String();

    const CarlaRecursiveMutexLocker sl (lock);
    garbageCollectIfNeeded();
    return addPooledString (strings, CharPointer_UTF8 (newString));
}

String StringPool::getPooledString (String::CharPointerType start, String::CharPointerType end)
{
    if (start.isEmpty() || start == end)
        return String();

    const CarlaRecursiveMutexLocker sl (lock);
    garbageCollectIfNeeded();
    return addPooledString (strings, StartEndString (start, end));
}

String StringPool::getPooledString (StringRef newString)
{
    if (newString.isEmpty())
        return String();

    const CarlaRecursiveMutexLocker sl (lock);
    garbageCollectIfNeeded();
    return addPooledString (strings, newString.text);
}

String StringPool::getPooledString (const String& newString)
{
    if (newString.isEmpty())
        return String();

    const CarlaRecursiveMutexLocker sl (lock);
    garbageCollectIfNeeded();
    return addPooledString (strings, newString);
}

void StringPool::garbageCollectIfNeeded()
{
    if (strings.size() > minNumberOfStringsForGarbageCollection
         && Time::getApproximateMillisecondCounter() > lastGarbageCollectionTime + garbageCollectionInterval)
        garbageCollect();
}

void StringPool::garbageCollect()
{
    const CarlaRecursiveMutexLocker sl (lock);

    for (int i = strings.size(); --i >= 0;)
        if (strings.getReference(i).getReferenceCount() == 1)
            strings.remove (i);

    lastGarbageCollectionTime = Time::getApproximateMillisecondCounter();
}

StringPool& StringPool::getGlobalPool() noexcept
{
    static StringPool pool;
    return pool;
}

}
