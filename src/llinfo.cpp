//-----------------------------------------------------------------------------
// llInfo - List information about an executable
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


#include <iostream>
#include <assert.h>
#include "llinfo.h"

#include <string>
#define byte win_byte_override  // Fix for c++ v17
#include <windows.h>
#undef byte                     // Fix for c++ v17
#include <winioctl.h>
#include <format>

#include <vector>
#include <SetupAPI.h>
#include <devguid.h>

#include <Wbemidl.h> // For WMI
#include <comdef.h>  // For _bstr_t

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "setupapi.lib")

#include <windows.h>
#include <iostream>
#include <vector>
#include <ctime>

#pragma comment(lib, "version.lib")


// ---------------------------------------------------------------------------

static const char sHelp[] =
" Size " LLVERSION "\n"
"\n"
"  List information about executables and dlls\n"
"\n"
"  !0eSyntax:!0f\n"
"    [<switches>]... \n"
"\n"
"  !0eWhere switches are:!0f\n"
"   !02-?!0f                ; Show this help\n"
"   !02-G!0f=<regPattern>   ; Grep output \n"
"\n"
"  !0eExamples:!0f\n"
"   ; List executable properties if machine type is 64 bit\n"
"   x -G=\"Machine: 64\" *.exe \n"
"\n"
"\n";

bool PrintVersionStrings(std::ostream& out, const char* filePath) {
    bool okay = false;
    DWORD dwHandle = 0;
    DWORD dwSize = GetFileVersionInfoSize(filePath, &dwHandle);
    if (dwSize == 0) 
        return false;

    std::vector<BYTE> buffer(dwSize);
    if (!GetFileVersionInfo(filePath, dwHandle, dwSize, &buffer[0]))
        return false;

    // 1. Get the Translation info (Language and CodePage)
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;
    UINT cbTranslate = 0;

    if (VerQueryValue(&buffer[0], "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate)) {
        // Typically take the first available language
        char subBlock[256];
        const char* keys[] = {"CompanyName", "LegalTrademarks", "FileDescription", "Comments", "OriginalFileName", "FileVersion", "LegalCopyright", "ProductName", "ProductVersion"};

        for (auto key : keys) {
            sprintf_s(subBlock, "\\StringFileInfo\\%04x%04x\\%s",
                       lpTranslate[0].wLanguage, lpTranslate[0].wCodePage, key);

            const char* lpBuffer = NULL;
            UINT dwBytes = 0;
            if (VerQueryValue(&buffer[0], subBlock, (LPVOID*)&lpBuffer, &dwBytes)) {
               out << "\t" << key << ": " << lpBuffer << std::endl;
            }
            okay = true;
        }

        if (okay) {
            int numLangs = cbTranslate / sizeof(LANGANDCODEPAGE);
            out << "\tLanguages: " << numLangs << std::endl;
        }
    }

    if (okay) {
        VS_FIXEDFILEINFO* lpFileInfo;
        UINT cbFileInfo;
        if (VerQueryValue(&buffer[0], "\\", (LPVOID*)&lpFileInfo, &cbFileInfo)) {
            // You now have access to numeric versioning
            DWORD dwMajor = HIWORD(lpFileInfo->dwFileVersionMS);
            DWORD dwMinor = LOWORD(lpFileInfo->dwFileVersionMS);

            // Check flags like:
            // VS_FF_DEBUG        - Is it a debug build?
            // VS_FF_PRERELEASE   - Is it a beta/pre-release?
            if (( lpFileInfo->dwFileFlags & VS_FF_DEBUG ) == VS_FF_DEBUG)
                out << "\tBuildType: Debug\n";
            else if (lpFileInfo->dwFileFlags != 0)
                out << "\tBuildFlags: " << lpFileInfo->dwFileFlags << std::endl;
        }
    }

    return okay;
}

bool PrintBinaryInfo(std::ostream& out, const char* filePath) {
    bool okay = false;
    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    LPVOID lpBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)lpBase;

    if (dosHeader->e_magic == 0x5A4D) {
        // out << "\tDos Header\n";
        out << "\tCode Pages: " << dosHeader->e_cp << std::endl;
        out << "\tRelocations: " << dosHeader->e_crlc << std::endl;
        out << "\tMin extra: " << dosHeader->e_minalloc << std::endl;
        out << "\tMax extra: " << dosHeader->e_maxalloc << std::endl;
        out << "\tStack Size: " << dosHeader->e_ss << std::endl;
                      
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)( (BYTE*)lpBase + dosHeader->e_lfanew );

        if (ntHeaders->Signature == 0x00004550) {
            // out << "\tNT Header\n";
            // Machine Type
            if (ntHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
                out << "\tMachine: 64-bit" << std::endl;
            else if (ntHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
                out << "\tMachine: 32-bit" << std::endl;


            // Link Date (TimeDateStamp is seconds since Jan 1, 1970)
            time_t linkTime = (time_t)ntHeaders->FileHeader.TimeDateStamp;
            char timeBuf[26];
            ctime_s(timeBuf, sizeof(timeBuf), &linkTime);
            out << "\tCompiled: " << timeBuf;
            okay = true;
        }
    }

    UnmapViewOfFile(lpBase);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return okay;
}


LLInfoConfig LLInfo::sConfig;

// ---------------------------------------------------------------------------
LLInfo::LLInfo() :
	m_exitOp(LLSup::eOpNone),
	m_exitValue(0),
	m_countExitOkayCnt(0),
	m_countExitShowCnt(0)
{
    sConfigp = &GetConfig();
}
    
// ---------------------------------------------------------------------------
LLConfig& LLInfo::GetConfig() 
{
    return sConfig;
}

// ---------------------------------------------------------------------------
int LLInfo::StaticRun(const char* cmdOpts, int argc, const char* pDirs[])
{
    LLInfo llInfo;
    return llInfo.Run(cmdOpts, argc, pDirs);
}

// ---------------------------------------------------------------------------
int LLInfo::Run(const char* cmdOpts, int argc, const char* pDirs[])
{
    std::string str;

    // Initialize run stats
    size_t nFiles = 0;

    if (argc == 0 && *cmdOpts == '\0')
        cmdOpts = "?";

    // Setup default as needed.
    if (argc == 0 && strstr(cmdOpts, "I=") == 0)
    {
        const char* sDefDir[] = {"*"};
        argc  = sizeof(sDefDir)/sizeof(sDefDir[0]);
        pDirs = sDefDir;
    }

    // Parse options
    while (*cmdOpts)
    {
        switch (*cmdOpts)
        {
        case '?':
            Colorize(std::cout, sHelp);
            return sIgnore;
        default:
            if ( !ParseBaseCmds(cmdOpts))
                 return sError;
            break;
        }

        // Advance to next parameter
        LLSup::AdvCmd(cmdOpts);
    }


    if (m_inFile.length() != 0)
    {
        LLSup::ReadFileList(m_inFile.c_str(), EntryCb, this);
    }

    // Iterate over dir patterns.
    for (int argn = 0; argn < argc; argn++)
    {
        bool recurse = m_dirScan.m_recurse;
        m_dirScan.m_recurse &= !LLPath::IsFile(pDirs[argn]);
        m_dirScan.Init(pDirs[argn], NULL, m_dirScan.m_recurse);
        nFiles += m_dirScan.GetFilesInDirectory();
        m_dirScan.m_recurse = recurse;
    }

    if (m_echo)
    {
        if (m_countError != 0)
            LLMsg::Out() << m_countError << " Errors\n";
        if (m_countIgnored != 0)
            LLMsg::Out() <<  m_countIgnored << " Ignored\n";
		if (m_countExitOkayCnt != 0)
			LLMsg::Out() << m_countExitOkayCnt << " ExitOkay\n";
		if (m_countExitShowCnt != 0)
			LLMsg::Out() << m_countExitShowCnt << " ExitShow\n";

        DWORD mseconds = GetTickCount() - m_startTick;
        LLMsg::PresentMseconds(mseconds);
    }  
	else if (m_exitOp != LLSup::eOpNone)
	{
		const char* sOp[] = { "", "Equal", "Greater", "Less", "NotEqual" };
		LLMsg::Out() << "Exit Test:" << sOp[m_exitOp] << " " << m_exitValue << std::endl;
		LLMsg::Out() << m_countExitOkayCnt << " ExitOkay\n";
		LLMsg::Out() << m_countExitShowCnt << " ExitShow\n";
	}

	return ExitStatus((int)nFiles);
}


// ---------------------------------------------------------------------------
int LLInfo::ProcessEntry(
        const char* pDir,
        const WIN32_FIND_DATA* pFileData,
        int depth)      // 0...n is directory depth, -n end-of nth diretory
{
    if (!FilterDir(pDir, pFileData, depth))
        return sIgnore;

    if (m_isDir)
        return sIgnore;

    std::ostringstream sout;
 
    if (PrintVersionStrings(sout, m_srcPath) ) {
        PrintBinaryInfo(sout, m_srcPath);

        std::string str = sout.str();
        int flags = m_grepLinePat.flags();
        if (flags == 0 || std::regex_search(str.begin(), str.end(), m_grepLinePat)) {
            std::cout << "File: " << m_srcPath << "\n";
            std::cout << str;
            std::cout << "\n";
        }
    } else {
        VerboseMsg() << "Ignored " << m_srcPath << std::endl;
    }

    return sIgnore;     // ignore end-of-directory
}
