//-----------------------------------------------------------------------------
// llsize - List device storage size
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
#include "llsize.h"

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


// ---------------------------------------------------------------------------

static const char sHelp[] =
" Size " LLVERSION "\n"
"\n"
"  !0eSyntax:!0f\n"
"    [<switches>] -c \"command\" <Pattern>... \n"
"\n"
"  !0eWhere switches are:!0f\n"
"   !02-?!0f                  ; Show this help\n"
"   !02-A=!0f[nrhs]           ; Limit files by attribute (n=normal r=readonly, h=hidden, s=system)\n"
"   !02-B=!0fc                ; Add additional field separators to use with #n selection\n"
"   !02-D!0f                  ; Only process directories \n"
"   !02-e=!0f[lgen]<value>[L<loopCnt> ;  Echo if exit status less, greater, equal notEqual to value \n"
"                       ;   Optionally continue Looping executing loopCnt while exit status okay \n"
"   !02-F!0f                  ; Only process files \n"
"   !02-F=!0f<filePat>,...    ; Limit to matching file patterns \n"
"\n"
"  !0eExamples:!0f\n"
"   s   \n"
"\n"
"\n";

 

bool IsProcessElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;

    // Get a handle to the access token for the current process
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        std::cerr << "OpenProcessToken failed: " << GetLastError() << std::endl;
        if (hToken) {
            CloseHandle(hToken);
        }
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD dwSize;

    // Retrieve information about the token's elevation status
    if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
        std::cerr << "GetTokenInformation failed: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return false;
    }

    isElevated = elevation.TokenIsElevated;
    CloseHandle(hToken);

    return isElevated;
}

// Function to get the drive type as a string
std::string GetDriveTypeAsString(STORAGE_BUS_TYPE type) {
    switch (type) {
    case BusTypeScsi:
        return "SCSI";
    case BusTypeAta:
        return "ATA";
    case BusTypeSata:
        return "SATA";
    case BusTypeUsb:
        return "USB";
    case BusType1394:
        return "IEEE 1394";
    case BusTypeSsa:
        return "SSA";
    case BusTypeFibre:
        return "Fibre Channel";
    case BusTypeSas:
        return "SAS";
    case BusTypeNvme:
        return "NVMe";
    default:
        return "Unknown";
    }
}


/*

To get the size of a physical storage drive on Windows without administrator privileges,
you can't directly query the raw physical device. Instead, you need to query the logical
volumes or partitions that reside on the physical drive. User accounts typically have access
to these logical representations, which are assigned drive letters (like C:, D:, etc.),
without requiring elevated permissions.

Using the Windows API (C++)
The recommended way to do this programmatically is by using the Windows Management
Instrumentation (WMI) via the Win32_LogicalDisk class. This approach is widely supported
and doesn't require administrator rights for basic information queries like disk size.

*/
// Does NOT requires admin privilege
int ListStorageDevicesNoAdmin() {
    // 1. Initialize COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM, Error: " << hr << std::endl;
        return 1;
    }

    // 2. Set general COM security levels
    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to initialize security, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 3. Obtain a locator to the WMI service
    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create IWbemLocator object, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 4. Connect to the WMI namespace
    IWbemServices* pSvc = NULL;
    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hr)) {
        std::cerr << "Could not connect to WMI, Error: " << hr << std::endl;
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 5. Set security on the proxy
    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        std::cerr << "Could not set proxy blanket, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 6. Query for all logical disks
    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        // _bstr_t(L"SELECT * FROM Win32_LogicalDisk WHERE DriveType = 3"), // DriveType 3 is a local disk
        _bstr_t(L"SELECT * FROM Win32_LogicalDisk"), // DriveType 3 is a local disk
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        std::cerr << "Query for logical disks failed, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 7. Iterate through the results and get the size
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;
        // Get the drive letter (DeviceID)
        hr = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
        std::wcout << std::setw(4) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"VolumeName", 0, &vtProp, 0, 0);
        if (hr == S_OK) {
            std::wcout << std::setw(20) << ( vtProp.bstrVal != nullptr ? vtProp.bstrVal : L"" );
        }
        VariantClear(&vtProp);

        long long totalSize=0, freeSize=0;

        // Get the total size
        hr = pclsObj->Get(L"Size", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr) {
            totalSize = _wtoi64(vtProp.bstrVal);
            std::wcout << L" Size:" << std::setw(7) << std::format(L"{:L}", totalSize / ( 1024 * 1024 * 1024 )) << L" GB";
        }
        VariantClear(&vtProp);

        // Get the free space
        hr = pclsObj->Get(L"FreeSpace", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr) {
            freeSize = _wtoi64(vtProp.bstrVal);
            std::wcout << L" Free:" << std::setw(7) << std::format(L"{:L}", freeSize / ( 1024 * 1024 * 1024 )) << L" GB";
            if (totalSize > 0)
                std::wcout << L" (" << std::setw(3) << ( freeSize * 100 / totalSize ) << "%)";
        }
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"MediaType", 0, &vtProp, 0, 0);
        if (hr == S_OK) {
            const char* type = "Unknown";
            switch (vtProp.intVal) {
            case 12:
                type = "Fixed";
                break;
            case 11:
                type = "Removable";
                break;
            case 5:
                type = "CDRom";
                break;
            default:
                break;
            }
            std::wcout << std::setw(10) << type;
        }
        VariantClear(&vtProp);

        pclsObj->Release();
        std::wcout << std::endl;
    }

    // 8. Clean up
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}


int ListMediaNoAdmin() {
    // 1. Initialize COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM, Error: " << hr << std::endl;
        return 1;
    }

    // 2. Set general COM security levels
    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to initialize security, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 3. Obtain a locator to the WMI service
    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create IWbemLocator object, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 4. Connect to the WMI namespace
    IWbemServices* pSvc = NULL;
    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hr)) {
        std::cerr << "Could not connect to WMI, Error: " << hr << std::endl;
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 5. Set security on the proxy
    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        std::cerr << "Could not set proxy blanket, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 6. Query for all logical disks
    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM Win32_PhysicalMedia"), // DriveType 3 is a local disk
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        std::cerr << "Query for media failed, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 7. Iterate through the results and get the size
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    std::wcout << L"\nMedia\n";
    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        hr = pclsObj->Get(L"Tag", 0, &vtProp, 0, 0);
        if (hr == S_OK)
            std::wcout << std::setw(22) << ( vtProp.bstrVal != nullptr ? vtProp.bstrVal : L"" );
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
        if (hr == S_OK)
            std::wcout << std::setw(22) << ( vtProp.bstrVal != nullptr ? vtProp.bstrVal : L"" );
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        pclsObj->Release();
        std::wcout << std::endl;
    }

    // 8. Clean up
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}


/*
`\\.\` (Device Namespace):
 * This prefix, also known as the "device namespace," allows you to access devices and objects within
   the operating system's kernel, similar to how /dev works in Linux.
 * Paths starting with `\\.\` are normalized, meaning they are converted to a standard format,
   and the path components like "." and ".." are resolved.
 * Examples include accessing a physical drive (\\.\C:) or a named pipe (\\.\Pipe\MyPipe).
 * You can browse the contents of the device namespace using tools like WinObjEx.

\\? (Extended Path):
 * This prefix, also known as the "extended path," bypasses all path normalization,
   meaning the path is passed to the file system verbatim.
 * This allows you to use paths longer than the standard MAX_PATH limit (260 characters).
 * You can use forward slashes (/) in the path, which are not allowed in standard paths.
 * Examples include accessing a file with a very long name (\\?\C:\Long\Path\To\A\File.txt).
 * It's important to note that the \\? prefix is only for absolute paths, not relative paths.
 *
*/
// Requires admin privilege
int ListStorageDevicesWithAdmin() {
    std::cout << "Listing all physical disk drives..." << std::endl;

    for (int i = 0; i < 64; ++i) { // Check up to 64 physical drives
        std::string devicePath = std::format("\\\\.\\PhysicalDrive{}", i);
        HANDLE hDevice = CreateFileA(
            devicePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hDevice != INVALID_HANDLE_VALUE) {
            // Get disk size
            GET_LENGTH_INFORMATION diskLengthInfo;
            DWORD bytesReturned;
            if (DeviceIoControl(
                hDevice,
                IOCTL_DISK_GET_LENGTH_INFO,
                NULL,
                0,
                &diskLengthInfo,
                sizeof(diskLengthInfo),
                &bytesReturned,
                NULL
                )) {
                double sizeGB = static_cast<double>( diskLengthInfo.Length.QuadPart ) / ( 1024.0 * 1024.0 * 1024.0 );

                // Get drive type
                STORAGE_PROPERTY_QUERY query;
                STORAGE_DESCRIPTOR_HEADER header;
                query.PropertyId = StorageDeviceProperty;
                query.QueryType = PropertyStandardQuery;

                if (DeviceIoControl(
                    hDevice,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &query,
                    sizeof(query),
                    &header,
                    sizeof(header),
                    &bytesReturned,
                    NULL
                    )) {
                    // Allocate memory for the full descriptor
                    std::vector<BYTE> buffer(header.Size);
                    PSTORAGE_DEVICE_DESCRIPTOR pDesc = reinterpret_cast<PSTORAGE_DEVICE_DESCRIPTOR>( buffer.data() );

                    if (DeviceIoControl(
                        hDevice,
                        IOCTL_STORAGE_QUERY_PROPERTY,
                        &query,
                        sizeof(query),
                        pDesc,
                        header.Size,
                        &bytesReturned,
                        NULL
                        )) {
                        std::string driveType = GetDriveTypeAsString(pDesc->BusType);
                        std::cout << "  - Disk " << i << ": " << std::fixed << std::setprecision(2) << sizeGB << " GB, Type: " << driveType << std::endl;
                    }
                }
            }
            CloseHandle(hDevice);
        }
    }
    return 0;
}



struct StorageDevice {
    std::string path;
    std::string   friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr;
    unsigned long long sizeInBytes;
};

bool getProperty(const HDEVINFO& hDevInfo, SP_DEVINFO_DATA& devInfoData, DWORD propertyId, string& result) {
    char property[1024];
    if (SetupDiGetDeviceRegistryPropertyA(
        hDevInfo,
        &devInfoData,
        propertyId,
        NULL,
        (PBYTE)property,
        sizeof(property),
        NULL
        )) {
        result = property;
        return true;
    }
    return false;
}


/*
`\\.\` (Device Namespace):
 * This prefix, also known as the "device namespace," allows you to access devices and objects within
   the operating system's kernel, similar to how /dev works in Linux.
 * Paths starting with `\\.\` are normalized, meaning they are converted to a standard format,
   and the path components like "." and ".." are resolved.
 * Examples include accessing a physical drive (\\.\C:) or a named pipe (\\.\Pipe\MyPipe).
 * You can browse the contents of the device namespace using tools like WinObjEx.

\\? (Extended Path):
 * This prefix, also known as the "extended path," bypasses all path normalization,
   meaning the path is passed to the file system verbatim.
 * This allows you to use paths longer than the standard MAX_PATH limit (260 characters).
 * You can use forward slashes (/) in the path, which are not allowed in standard paths.
 * Examples include accessing a file with a very long name (\\?\C:\Long\Path\To\A\File.txt).
 * It's important to note that the \\? prefix is only for absolute paths, not relative paths.
 *
*/
// Requires admin privilege
std::vector<StorageDevice> getStorageDevicesWithAdmin() {
    std::vector<StorageDevice> devices;
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_DISKDRIVE,
        NULL,
        NULL,
        DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get device info, Error: " << GetLastError() << std::endl;
        return devices;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
        string friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr;

        if (getProperty(hDevInfo, devInfoData, SPDRP_FRIENDLYNAME, friendlyName)) {
            getProperty(hDevInfo, devInfoData, SPDRP_CLASS, className);
            getProperty(hDevInfo, devInfoData, SPDRP_COMPATIBLEIDS, compatibleIds);
            getProperty(hDevInfo, devInfoData, SPDRP_DEVICEDESC, devDescription);
            getProperty(hDevInfo, devInfoData, SPDRP_ENUMERATOR_NAME, enumName);
            getProperty(hDevInfo, devInfoData, SPDRP_UI_NUMBER_DESC_FORMAT, uiNumberStr);

            // getProperty(hDevInfo, devInfoData, SPDRP_REMOVAL_POLICY,  xxx);

            char deviceInstanceId[MAX_PATH];
            SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, deviceInstanceId, sizeof(deviceInstanceId), NULL);

            // Constructing a device path
            std::string devicePath = "\\\\.\\" + std::string(deviceInstanceId);

            // Getting disk size
            HANDLE hDevice = CreateFileA(
                devicePath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (hDevice != INVALID_HANDLE_VALUE) {
                DISK_GEOMETRY_EX diskGeometry;
                DWORD bytesReturned;
                if (DeviceIoControl(
                    hDevice,
                    IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                    NULL,
                    0,
                    &diskGeometry,
                    sizeof(diskGeometry),
                    &bytesReturned,
                    NULL
                    )) {
                    devices.push_back({devicePath, friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr, ( unsigned long long )diskGeometry.DiskSize.QuadPart});
                } else {
                    std::cerr << "Failed to get disk geometry. Error: " << GetLastError() << std::endl;
                    devices.push_back({devicePath, friendlyName, 0});
                }
                CloseHandle(hDevice);
            } else {
                std::cerr << "Failed to open device  " << friendlyName << ", Error: " << GetLastError() << std::endl;
                devices.push_back({"", friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr, 0});
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return devices;
}

void ListStorageSizes() {
    std::locale::global(std::locale("en_US.UTF-8"));    // Required to get comma when using format("{:L}", value)

    bool hasAdmin = IsProcessElevated();
    if (hasAdmin) {
        std::cout << "Have admin privilege\n";
        auto devices = getStorageDevicesWithAdmin();
        if (devices.empty()) {
            std::cout << "No storage devices found." << std::endl;
        } else {
            std::cout << "Available Storage Devices:" << std::endl;
            for (const auto& dev : devices) {
                std::cout << dev.friendlyName << std::endl;
                std::cout << "\t" << dev.className << std::endl;
                std::cout << "\t" << dev.compatibleIds << std::endl;
                std::cout << "\t" << dev.devDescription << std::endl;
                std::cout << "\t" << dev.enumName << std::endl;
                std::cout << "\t" << dev.uiNumberStr << std::endl;

                if (dev.sizeInBytes > 0) {
                    std::cout << "Path: " << dev.path << std::endl;
                    std::cout << "Size: " << ( dev.sizeInBytes / ( 1024.0 * 1024.0 * 1024.0 ) ) << " GB" << std::endl;
                }
            }
        }

        ListStorageDevicesWithAdmin();
    } else {
        std::cout << "No admin privilege\n";
        ListStorageDevicesNoAdmin();
        ListMediaNoAdmin();
    }
}


LLSizeConfig LLSize::sConfig;

// ---------------------------------------------------------------------------
LLSize::LLSize() :
	m_exitOp(LLSup::eOpNone),
	m_exitValue(0),
	m_countExitOkayCnt(0),
	m_countExitShowCnt(0)
{
    sConfigp = &GetConfig();
}
    
// ---------------------------------------------------------------------------
LLConfig& LLSize::GetConfig() 
{
    return sConfig;
}

// ---------------------------------------------------------------------------
int LLSize::StaticRun(const char* cmdOpts, int argc, const char* pDirs[])
{
    LLSize llExec;
    return llExec.Run(cmdOpts, argc, pDirs);
}

// ---------------------------------------------------------------------------
int LLSize::Run(const char* cmdOpts, int argc, const char* pDirs[])
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
        }

        // Advance to next parameter
        LLSup::AdvCmd(cmdOpts);
    }


    VerboseMsg() << "LLExecute:\n"
              << (m_prompt ? " Prompt\n" : "")
              << (m_echo ? " Echo\n" : "Quit\n")
              << (m_exec ? "": " NoExecute\n")
              << " Separators:[" << m_separators << "]\n"
              << "\n";

    ListStorageSizes();

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
int LLSize::ProcessEntry(
        const char* pDir,
        const WIN32_FIND_DATA* pFileData,
        int depth)      // 0...n is directory depth, -n end-of nth diretory
{
       return sIgnore;     // ignore end-of-directory
}
