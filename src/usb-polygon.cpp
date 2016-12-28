
#include <windows.h>
#include <Setupapi.h>
#include <tchar.h>
#include <iostream>
#include <stdio.h>		// для _tprintf


void OutFormatMsg(const TCHAR *Msg){
    LPVOID lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (TCHAR*) &lpMsgBuf,
        0,
        NULL
        );

    _tprintf(_T("%s: %s\n"), Msg, (TCHAR*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

#define LocalFreeIf(Pointer) if(Pointer) { LocalFree(Pointer); Pointer = NULL; }

int main()
{
    setlocale(0, "");

    PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData = NULL;
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    SP_DEVINFO_DATA DeviceInfoData;
    HDEVINFO hDevInfo;

    // \\?\usbstor#disk&ven_<name>&rev_0001#7&4e...
    // GUID_DEVINTERFACE_DISK
    //const GUID InterfaceGuid = { 0x53F56307,0xB6BF,0x11D0,{0x94,0xF2,0x00,0xA0,0xC9,0x1E,0xFB,0x8B} };

    // \\?\storage#removablemedia#8...
    // GUID_DEVINTERFACE_VOLUME
    // нельзя FriendlyName
    const GUID InterfaceGuid = { 0x53f5630dL, 0xb6bf, 0x11d0, {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b} };

    hDevInfo = SetupDiGetClassDevs( &InterfaceGuid,0, 0, DIGCF_PRESENT  | DIGCF_DEVICEINTERFACE  );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        OutFormatMsg(_T("SetupDiGetClassDevs"));
        return 1;
    }

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for(DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, 0, &InterfaceGuid, i, &DeviceInterfaceData); ++i)
    {
        ULONG RequiredLength = 0;

        while ( !SetupDiGetDeviceInterfaceDetail( hDevInfo,
            &DeviceInterfaceData, pDeviceInterfaceDetailData, RequiredLength, &RequiredLength,
            &DeviceInfoData ) )
        {
            if( GetLastError() == ERROR_INSUFFICIENT_BUFFER )
            {
                LocalFreeIf( pDeviceInterfaceDetailData );
                pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LMEM_FIXED, RequiredLength );
                pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            } else {
                OutFormatMsg(_T("SetupDiGetDeviceInterfaceDetail"));
                continue;
            }
        }

        HANDLE hDevice=CreateFile(
			pDeviceInterfaceDetailData->DevicePath,
                GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ or FILE_SHARE_WRITE, //0,
                NULL,
                OPEN_EXISTING,
				0, //FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING, //FILE_ATTRIBUTE_NORMAL,
                NULL
                );

        if(hDevice == INVALID_HANDLE_VALUE) {
        	//	_tprintf(_T("CreateFile failed! \n"));
        } else {
        	_tprintf(_T("pDeviceInterfaceDetailData->DevicePath: %s\n"), pDeviceInterfaceDetailData->DevicePath);
			_tprintf(_T("CreateFile done! \n"));

			CloseHandle(hDevice);
        }

        LocalFreeIf(pDeviceInterfaceDetailData);
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return 0;
}
