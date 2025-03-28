//-----------------------------------------------------------------------------
// llfind - Find files along directory scan or env paths
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
#include <string.h>
#include <assert.h>

#include "LLFind.h"
#include "comma.h"


// ---------------------------------------------------------------------------

static const char sHelp[] =
" Find " LLVERSION "\n"
" Find a file along directory path or env paths\n"
"\n"
"  !0eSyntax:!0f\n"
"    [<switches>] <Pattern>... \n"
"\n"
"  !0eWhere switches are:!0f\n"
"   -?                  ; Show this help\n"
"   -A=[nrhs]           ; Limit files by attribute (n=normal r=readonly, h=hidden, s=system)\n"
"   -D                  ; Only directories in matching, default is all types\n"
"   -D=<dirPattern>     ; Only directories matching dirPttern \n"

"   -e=<envName>[,...]  ; Search env environment directories for pattern\n"
"   -F                  ; Only files in matching, default is all types\n"
"   -F=<filePat>,...    ; Limit to matching file patterns \n"
"   -G=<grepPattern>    ; Find only if file contains grepPattern \n"
"   -g=<grepRange>      ;  default is search entire file, +n=first n lines \n"
"   -I=<file>           ; Read list of files from this file\n"
"   -p                  ; Search PATH environment directories for pattern\n"
"   -p                  ; Short cut for -e=PATH, search path \n"
"   -P=<srcPathPat>     ; Optional regular expression pattern on source files full path\n"
"   -q                  ; Quiet, default is echo command\n"
"   -Q=n                ; Quit after 'n' matches\n"
"   -s                  ; Show file size size\n"
"   -t[acm]             ; Show Time a=access, c=creation, m=modified, n=none\n"
"   -X=<pathPat>,...    ; Exclude patterns  -X=*.lib,*.obj,*.exe\n"
"                       ;  No space in patterns. Pattern applied against fullpath\n"
"                       ;  So *\\ma will exclude a directory ma or file ma \n"
"   -V                  ; Verbose\n"
"   -E=[cFDdsa]         ; Return exit code, c=file+dir count, F=file count, D=dir Count\n"
"                       ;    d=depth, s=size, a=age \n"
"   -r                  ; Don't recurse into subdirectories\n"
"   -R                  ; Relative path \n"
"   -B                  ; aBsolute path \n"
"   -1=<file>           ; Redirect output to file \n"
"   -~                  ; Relative time, use with -t...  \n"
"   -,                  ; Comma in size, use with -s  \n"
"\n"
"  !0eWhere Pattern is:!0f\n"
"    <file|Pattern> \n"
"    [<directory|pattern> \\]... <file|Pattern|#n> \n"
"\n"
"  !0ePattern:!0f\n"
"      * = zero or more characters\n"
"      ? = any character\n"
"\n"
"  !0eExample:!0f\n"
"    ; Assuming lf.exe and where.exe are linked to llfile.exe\n"
"    ; Example to find where an executable is in current search PATH\n"
"    llfile -xw  cmd.exe              ; -xw force where command\n"
"    lf -p cmd.exe                    ; -p enables PATH search\n"
"    where cmd.exe                    ; where command auto adds '-p'\n"
"    lf -Ar -i=c:\\fileList.txt >nul   ; check for non-readonly files from list \n"
"\n"
"    lf -e=LIB libfoo.lib             ; search env LIB's path for libfoo.lib\n"
"    lf -F=*.exe,*.com,*.bat .\\*      ; search for executables or batch files\n"
"    lf -F -X=*.obj,*.exe  build\\*    ; search for not obj or exe files\n"
"\n"
"\n";

LLFindConfig LLFind::sConfig;

// ---------------------------------------------------------------------------
LLFind::LLFind() :
    m_force(false),
    m_envStr(),
    m_totalSize(0)
{
    m_exitOpts = "c";
    m_showAttr  =
        m_showCtime =
        m_showMtime =
        m_showAtime =
        m_showPath =
        m_showSize  =  false;
    m_showRelPath = true;

    memset(&m_fileData, 0, sizeof(m_fileData));
    m_dirScan.m_recurse = true;

    sConfigp = &GetConfig();
}

// ---------------------------------------------------------------------------
LLConfig& LLFind::GetConfig() 
{
    return sConfig;
}

// ---------------------------------------------------------------------------
int LLFind::StaticRun(const char* cmdOpts, int argc, const char* pDirs[])
{
    LLFind llFind;
    return llFind.Run(cmdOpts, argc, pDirs);
}

// ---------------------------------------------------------------------------
int LLFind::Run(const char* cmdOpts, int argc, const char* pDirs[])
{
    const char missingRetMsg[] = "Missing return value, c=file count, d=depth, s=size, a=age, syntax -0=<code>";
    const char missingEnvMsg[] = "Environment variable name, syntax -E=<envName> ";

    const std::string endPathStr(";");
    LLSup::StringList envList;
    std::string str;

    // Initialize stuff
    size_t nFiles = 0;
    bool sortNeedAllData = m_showSize;
	bool whereMode = false;	// where mode add [.](exe|com|cmd|bat) to search pattern.
    EnableCommaCout();

    // Setup default as needed.
    if (argc == 0 && strstr(cmdOpts, "I=") == 0)
    {
        const char* sDefDir[] = {"*"};
        argc  = sizeof(sDefDir)/sizeof(sDefDir[0]);
        pDirs = sDefDir;
    }

    // Get current directory as default starting point.
    char currentDir[LL_MAX_PATH];
    GetCurrentDirectory(ARRAYSIZE(currentDir), currentDir);
    m_pCwd = currentDir;
 

    // Parse options
    while (*cmdOpts)
    {
        switch (*cmdOpts)
        {
        case 'e':   // -e=<envName>[,<envName>
            cmdOpts = LLSup::ParseList(cmdOpts+1, envList, missingEnvMsg);
            for (unsigned envIdx = 0; envIdx != envList.size(); envIdx++)
            {
                std::string envStr = envList[envIdx];
                VerboseMsg() << "-e " << envStr << std::endl;
                m_envStr += LLSup::GetEnvStr(envStr.c_str(), "");    // pointer needs to be freed
                m_envStr += endPathStr;
                m_dirScan.m_recurse = false;
            }
            break;

        case 'p': // Default to path environment
			whereMode = true;
            m_dirScan.m_recurse = false;
            {
                std::string pathStr = LLSup::GetEnvStr("PATH", "");     // pointer needs to be freed
                VerboseMsg() << "Searching PATH " << pathStr << std::endl;
                pathStr.insert(0, ".\\;");      // Force local directory search first
                m_envStr += pathStr;
                m_envStr += endPathStr;
            }
            break;

         case 'B': // Absolute   (-A used for attributes)
            m_showRelPath = false;
            break;
        case 'R': // Relative
            m_showRelPath = true;
            break;

        case 's':   // Toggle showing size.
            sortNeedAllData |= m_showSize = !m_showSize;
            // m_dirSort.SetSortData(sortNeedAllData);
            break;

        case 't':   // Display Time selection
            {
                bool unknownOpt = false;
                while (cmdOpts[1] && !unknownOpt)
                {
                    cmdOpts++;
                    switch (ToLower(*cmdOpts))
                    {
                    case 'a':
                        sortNeedAllData = m_showAtime = true;
                        break;
                    case 'c':
                        sortNeedAllData = m_showCtime = true;
                        break;
                    case 'm':
                        sortNeedAllData = m_showMtime = true;
                        break;
                    case 'n':   // NoTime
                        sortNeedAllData  = false;
                        m_showAtime = m_showCtime = m_showMtime = false;
                        break;
                    default:
                        cmdOpts--;
                        unknownOpt = true;
                        break;
                    }
                }
                // m_dirSort.SetSortData(sortNeedAllData);
            }
            break;

        case ',':
            DisableCommaCout();
            break;
        case '~':   // Toggle relative time display 
            LLFind::sConfig.m_refDateTime = !LLFind::sConfig.m_refDateTime;
            break;
        case '?':
            Colorize(std::cout, sHelp);
            return sIgnore;
        default:
            if ( !ParseBaseCmds(cmdOpts))
                return sError;
        }

        // Advance to next parameter
        LLSup::AdvCmd(cmdOpts);
    }


    VerboseMsg() << "LLFind  Env:" << m_envStr << std::endl;

    // Move arguments and input files into inFileList.
    std::vector<std::string> inFileList;

    for (int argn=0; argn < argc; argn++)
    {
        inFileList.push_back(pDirs[argn]);
    }

    if (m_inFile.length() != 0)
    {
        FILE* fin = stdin;

        if (strcmp(m_inFile.c_str(), "-") == 0 ||
            0 == fopen_s(&fin, m_inFile.c_str(), "rt"))
        {
            char fileName[LL_MAX_PATH];
            while (fgets(fileName, ARRAYSIZE(fileName), fin))
            {
                TrimString(fileName);       // remove extra space or control characters.
                if (*fileName == '\0')
                    continue;
                inFileList.push_back(fileName);
            }
        }
    }

    if ( !m_envStr.empty())
    {
        VerboseMsg() << " EnvDir:" << m_envStr << std::endl;

        m_onlyAttr = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE;
        for (unsigned argn = 0; argn < inFileList.size(); argn++)
        {
            lstring lookFor = inFileList[argn];
            size_t pos = lookFor.find_last_of('.');
            size_t extCnt = 1;
            const char* sExeExtn[] = { "", ".exe", ".bat", ".cmd", ".com", ".ps1" };
            if (pos == std::string::npos) {
                pos = lookFor.length();
                extCnt = ARRAYSIZE(sExeExtn);
            }
            unsigned len = 0;
            for (unsigned idx = 0; idx < extCnt; idx++) {
                lookFor.replace(pos, len, sExeExtn[idx]);
                len = strlen(sExeExtn[idx]);
                m_includeFileList.push_back(lookFor);
            }
        }

        // Iterate over env paths
        Split envPaths(m_envStr, ";");
		for (uint envIdx = 0; envIdx != envPaths.size(); envIdx++)
		{
			// lstring  dirPat = LLPath::Join(envPaths[envIdx], inFileList[argn].c_str());
            lstring  dirPath = envPaths[envIdx];	
			VerboseMsg() << "  ScanDir:" << dirPath << std::endl;
            // std::cerr << " Looking for:" << inFileList.at(0) << " in:" << dirPath << std::endl;
			m_dirScan.Init(dirPath, NULL);
            nFiles += m_dirScan.GetFilesInDirectory();
        }
    }
    else
    {
        // Iterate over dir patterns.
        for (unsigned argn=0; argn < inFileList.size(); argn++)
        {
            VerboseMsg() <<" Dir:" << inFileList[argn] << std::endl;
            m_dirScan.Init(inFileList[argn].c_str(), NULL);

            nFiles += m_dirScan.GetFilesInDirectory();
        }
    }

    // Return status, c=file count, d=depth, s=size, a=age

    if (m_exitOpts.length() != 0)
    switch ((char)m_exitOpts[0u])
    {
    case 'a':   // age,  -0=a
        {
        FILETIME   ltzFT;
        SYSTEMTIME sysTime;
        FileTimeToLocalFileTime(&m_fileData.ftLastWriteTime, &ltzFT);    // convert UTC to local Timezone
        FileTimeToSystemTime(&ltzFT, &sysTime);

        // TODO - compare to local time and return age.
        return 0;
        }
        break;
    case 's':   // file size, -0=s
        VerboseMsg() << ";Size:" << m_totalSize << std::endl;
        return (int)m_fileSize;
    case 'd':   // file depth, -o=d
        VerboseMsg() << ";Depth (-E=d depth currently not implemented,use -E=D for #directories):" << 0 << std::endl;
        return 0;    // TODO - return maximum file depth.
    case 'D':   // Directory count, -o=D
        VerboseMsg() << ";Directory Count:" << m_countOutDir << std::endl;
        return m_countOutDir;
    case 'F':   // File count, -o=F
        VerboseMsg() << ";File Count:" << m_countOutFiles << std::endl;
        return m_countOutFiles;
    case 'c':   // File and directory count, -o=c
    default:
        VerboseMsg() << ";File + Directory Count:" << (m_countOutDir + m_countOutFiles) << std::endl;
        return (int)(m_countOutDir + m_countOutFiles);
    }

	return ExitStatus(0);
}

 

// ---------------------------------------------------------------------------
static FILETIME operator-(const FILETIME& leftFt, const FILETIME& rightFt)
{
    FILETIME fTime;
    ULARGE_INTEGER v_ui;

    v_ui.LowPart = leftFt.dwLowDateTime;
    v_ui.HighPart = leftFt.dwHighDateTime;
    __int64 v_left = v_ui.QuadPart;

    v_ui.LowPart = rightFt.dwLowDateTime;
    v_ui.HighPart = rightFt.dwHighDateTime;
    __int64 v_right = v_ui.QuadPart;

    __int64 v_res = v_left - v_right;

    v_ui.QuadPart = v_res;
    fTime.dwLowDateTime = v_ui.LowPart;
    fTime.dwHighDateTime = v_ui.HighPart;
    return fTime;
}

// ---------------------------------------------------------------------------
// returns a UNIX-like timestamp (in seconds since 1970) with sub-second resolution.
static double time_d(const FILETIME& ft)
{
    // GetSystemTimeAsFileTime(&ft);
    const __int64* val = (const __int64*)&ft;
    return static_cast<double>(*val) / 10000000.0 - 11644473600.0;   // epoch is Jan. 1, 1601: 134774 days to Jan. 1, 1970
}
 
// ---------------------------------------------------------------------------
static std::ostream& Format(std::ostream& out, const FILETIME& utcFT)
{
    FILETIME   locTmFT;
    SYSTEMTIME sysTime;


    if (LLFind::sConfig.m_refDateTime)
    {
        SYSTEMTIME nowStUtc;
        GetSystemTime(&nowStUtc);
        FILETIME nowFtUtc;
        SystemTimeToFileTime(&nowStUtc, &nowFtUtc);

        FILETIME deltaFt = nowFtUtc - utcFT;
        __int64* delta64 = (__int64*)&deltaFt;
        double seconds = static_cast<double>(*delta64) / 10000000.0;
        double minutes = seconds / 60.0;
        double hours = minutes / 60.0;
        double days = hours / 24.0;
        double years = days / 365.25;
        const int width = 12;
        if (years >= 1.0)
            out << std::fixed << std::setw(width) << std::setprecision(1) << years << " years ";
        else if (days >= 1.0)
            out << std::fixed << std::setw(width) << std::setprecision(1) << days << " days  ";
        else if (hours >= 1.0)
            out << std::fixed << std::setw(width) << std::setprecision(1) << hours << " hours ";
        else if (minutes >= 1.0)
            out << std::fixed << std::setw(width) << std::setprecision(1) << minutes << " mins  ";
        else
            out << std::fixed << std::setw(width) << std::setprecision(1) << seconds << " secs  ";

        return out;
    }

    FileTimeToSystemTime(&utcFT, &sysTime);
    LLSup::Format(out, utcFT);

    return out;
}

// ---------------------------------------------------------------------------
const char* relative(const char* fullPath, const char* cwd) {
    unsigned cwdLen = strlen(cwd);
    return (strnicmp(fullPath, cwd, cwdLen) == 0)
        ? fullPath + cwdLen + 1
        : fullPath;
}

// ---------------------------------------------------------------------------
int LLFind::ProcessEntry(
        const char* pDir,
        const WIN32_FIND_DATA* pFileData,
        int depth)      // 0...n is directory depth, -n end-of nth diretory
{
    if (depth < 0)
        return sIgnore;     // ignore end-of-directory

    //  Filter on:
    //      m_onlyAttr      File or Directory, -F or -D
    //      m_onlyRhs       Attributes, -A=rhs
    //      m_includeList   File patterns,  -F=<filePat>[,<filePat>]...
    //      m_onlySize      File size, -Z op=(Greater|Less|Equal) value=num<units G|M|K>, ex -Zg100M
    //      m_excludeList   Exclude path patterns, -X=<pathPat>[,<pathPat>]...
    //      m_timeOp        Time, -T[acm]<op><value>  ; Test Time a=access, c=creation, m=modified\n
    //
    //  If pass, populate m_srcPath
    if ( !FilterDir(pDir, pFileData, depth))
        return sIgnore;

    VerboseMsg() << m_srcPath << "\n";

    if (m_isDir)
    {
        if (PatternMatch(m_dirScan.m_fileFilter, pFileData->cFileName) == false)
            return sIgnore;

		if (m_onlyAttr == FILE_ATTRIBUTE_DIRECTORY &&
			!LLSup::PatternListMatches(m_includeDirList, pFileData->cFileName, true))
			return sIgnore;
    }


    if ( !FilterGrep())
        return sIgnore;

    if (m_echo && !IsQuit())
    {
        if (m_showAttr)
        {
            // ShowAttributes(LLMsg::Out(), pDir, *pFileData, false);
            LLMsg::Out() << LLFind::sConfig.m_dirFieldSep;
        }
        if (m_showCtime)
            Format(LLMsg::Out(), pFileData->ftCreationTime) << LLFind::sConfig.m_dirFieldSep ;
        if (m_showMtime)
            Format(LLMsg::Out(), pFileData->ftLastWriteTime) << LLFind::sConfig.m_dirFieldSep;
        if (m_showAtime)
            Format(LLMsg::Out(), pFileData->ftLastAccessTime) << LLFind::sConfig.m_dirFieldSep;
        if (m_showSize)
            LLMsg::Out() << std::setw(LLFind::sConfig.m_fzWidth) << m_fileSize << LLFind::sConfig.m_dirFieldSep;

        // m_dirScan.
        if (m_showRelPath) 
            LLMsg::Out() << relative(m_srcPath, m_pCwd) << std::endl;
        else
            LLMsg::Out() << m_srcPath << std::endl;
    }

    m_totalSize += m_fileSize;
    m_fileData = *pFileData;
    if (m_isDir)
        m_countOutDir++;
    else
        m_countOutFiles++;

    return sOkay;
}

