//-----------------------------------------------------------------------------
// ll_stdhr - Projects standard header
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
//-----------------------------------------------------------------------------

#pragma once

#pragma warning (disable:4100 4189 4267 4365 4514 4571 4625 4626 4640 4668 4710 4820 498 26493 26462 26496 26446 26485 4464)
#include <string>
#include <iosfwd>

// https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry
// enable in registry 
//   [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\FileSystem]  "LongPathsEnabled" = dword : 00000001
// enable long path in ap manifest
//   longPathAware 
#define LL_MAX_PATH 512     // Windows OS MAX_PATH is 256

using namespace std;
#if 0
typedef long            Long;
typedef short           Short;
typedef unsigned short  UShort;
typedef unsigned short  Word;
#endif
typedef unsigned char   Byte;
typedef unsigned int uint;

const int sInvert = 2;
const int sOkay   = 1;
const int sIgnore = 0;
const int sError  = -1;


inline char ToUpper(char c) noexcept
{  return (char)toupper(c); }
inline char ToLower(char c) noexcept
{  return (char)tolower(c); }
inline char ToUnixSlash(char c) noexcept
{ return (c == '\\') ? '/' : c; }
inline char ToDosSlash(char c) noexcept
{ return (c == '/') ? '\\' : c; }
