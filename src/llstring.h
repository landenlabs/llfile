// ---------------------------------------------------------------------------
// llstring - Simple string wrapper
// 
// Author: Dennis Lang - 2015
// http://landenlabs.com/
//
// This file is part of LLFile project.
//
// ----- License ----
//
// Copyright (c) 2015 Dennis Lang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// ---------------------------------------------------------------------------


#pragma once

#include <assert.h>
#include <string>
#include <vector>

class lstring : public std::string
{
public:
    lstring() : std::string()
    { }

    lstring(const char* rhs) : std::string(rhs)
    { }

    lstring(const std::string& rhs) : std::string(rhs)
    { }

    operator const char*() const
    { return c_str(); }

    char back() const
    { return std::string::back(); }
};

// ---------------------------------------------------------------------------
char* PadRight(char* str, size_t maxLen, size_t padLen, char padChr);
char* PadLeft(char* str, size_t maxLen, size_t padLen, char padChr);
char* TrimString(char* string);


// Split string into parts.
class Split : public std::vector<std::string>
{
public:
    Split(const std::string& str, const char* delimList)
    {
        size_t lastPos = 0;
        size_t pos = str.find_first_of(delimList);

        while (pos != std::string::npos)
        {
            if (pos != lastPos)
                push_back(str.substr(lastPos, pos-lastPos));
            lastPos = pos + 1;
            pos = str.find_first_of(delimList, lastPos);
        }
        if (lastPos < str.length())
            push_back(str.substr(lastPos, std::string::npos));
    }
};


///
/// LLPool manages memory in buckets.
// ---------------------------------------------------------------------------
template <typename T>
class LLPool
{
public:
    LLPool() noexcept {}
    ~LLPool() { Clear(); }

    void Clear() noexcept
    {
        for (size_t i=0; i < m_buckets.size(); ++i)
        {
            delete m_buckets[i];
            m_buckets[i] = nullptr;
        }

        m_buckets.clear();
    }

    const T* Add(const T* pObj, size_t len)
    {
        assert(len < Bucket::sBucketSize);

        // Make sure we have atleast one bucket.
        if (m_buckets.size() == 0)
            m_buckets.push_back(new Bucket());
        Bucket* pBucket = m_buckets[m_buckets.size()-1];

        if (pBucket->nextPtr + len >= pBucket->endPtr)
        {
        _CrtCheckMemory( );
            // Need more room, add another bucket.
            m_buckets.push_back(new Bucket());
            pBucket = m_buckets[m_buckets.size()-1];
        _CrtCheckMemory( );
        }

        if (pObj)
        {
            assert(pBucket->nextPtr + len < pBucket->endPtr);
            memcpy(pBucket->nextPtr, pObj, len);
        }
        pObj = (const T*)pBucket->nextPtr;
        pBucket->nextPtr += len;

        return pObj;
    }

private:
    struct Bucket
    {
        static const int sBucketSize = 4096*16;
        Bucket() noexcept : topPtr(new char[sBucketSize]), nextPtr(topPtr), endPtr(topPtr+sBucketSize){}
        ~Bucket() { delete [] topPtr; }
        char* topPtr;
        char* nextPtr;
        char* endPtr;
    };
    std::vector<Bucket*> m_buckets;
};

// ---------------------------------------------------------------------------
// Warning - Does not free memory so heavy use will consume a lot of memory.
// Ideal for limited burst use.

struct LLString
{
    LLString(const char* str) :
        m_str(m_pool.Add(str, sizeof(char)*(strlen(str)+1)))
    {}

    operator const char*() noexcept
    { return m_str; }

    operator const char*() const noexcept
    { return m_str; }

    const char* c_str() const noexcept
    { return m_str; }

    const char* m_str;

    static LLPool<const char> m_pool;
};

inline std::ostream & operator<<(std::ostream& out, const LLString& lstr)
{
    out << lstr.c_str();
    return out;
}