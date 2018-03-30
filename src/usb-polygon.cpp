#include <windows.h>
#include <Setupapi.h>
#include <tchar.h>
#include <iostream>
#include <stdio.h>		// ��� _tprintf

#include <ntddscsi.h>

#define IOCTL_VOLUME_BASE   ((ULONG) 'V')
#define IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS \
  CTL_CODE(IOCTL_VOLUME_BASE, 0, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ��� ��������� _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER,
// ������� e��� � spti.h �� ������� Windows DDK
// � ��� � MinGW
struct scsi_st {
	SCSI_PASS_THROUGH_DIRECT t_spti;
	DWORD tmp;				// realign buffer to double word boundary
	byte sensebuf[32];
} myspti;

typedef struct _DISK_EXTENT {
	DWORD DiskNumber;
	LARGE_INTEGER StartingOffset;
	LARGE_INTEGER ExtentLength;
} DISK_EXTENT, *PDISK_EXTENT;

typedef struct _VOLUME_DISK_EXTENTS {
	DWORD NumberOfDiskExtents;
	DISK_EXTENT Extents[ANYSIZE_ARRAY];
} VOLUME_DISK_EXTENTS;

void OutFormatMsg(const TCHAR *Msg)
{
	LPVOID lpMsgBuf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (TCHAR*) &lpMsgBuf, 0,
			NULL);

	_tprintf(_T("%s: %s\n"), Msg, (TCHAR*) lpMsgBuf);
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
	//const GUID InterfaceGuid = {0x53F56307, 0xB6BF, 0x11D0,
    //  {0x94,0xF2,0x00,0xA0,0xC9,0x1E,0xFB,0x8B}};

	// \\?\storage#removablemedia#8...
	// GUID_DEVINTERFACE_VOLUME
	// ������ FriendlyName
	const GUID InterfaceGuid = {0x53f5630dL, 0xb6bf, 0x11d0, {0x94, 0xf2,
			0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b}};

	hDevInfo = SetupDiGetClassDevs(&InterfaceGuid, 0, 0,
			DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hDevInfo == INVALID_HANDLE_VALUE) {
		OutFormatMsg(_T("SetupDiGetClassDevs"));
		return 1;
	}

	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (DWORD i = 0;
			SetupDiEnumDeviceInterfaces(hDevInfo, 0, &InterfaceGuid, i,
					&DeviceInterfaceData); ++i) {
		ULONG RequiredLength = 0;

		while (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &DeviceInterfaceData,
				pDeviceInterfaceDetailData, RequiredLength, &RequiredLength,
				&DeviceInfoData)) {
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
				LocalFreeIf(pDeviceInterfaceDetailData);
				pDeviceInterfaceDetailData =
						(PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(
								LMEM_FIXED, RequiredLength);
				pDeviceInterfaceDetailData->cbSize =
						sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			} else {
				OutFormatMsg(_T("SetupDiGetDeviceInterfaceDetail"));
				continue;
			}
		}

		HANDLE hDevice = CreateFile(pDeviceInterfaceDetailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ or FILE_SHARE_WRITE, //0,
				NULL,
				OPEN_EXISTING, 0,
				NULL);

		if (hDevice == INVALID_HANDLE_VALUE) {
		} else {
			_tprintf(_T("pDeviceInterfaceDetailData->DevicePath: %s\n"),
					pDeviceInterfaceDetailData->DevicePath);
			_tprintf(_T("CreateFile done! \n"));

			char vbuf[512];				// ����� ��� ��������� ������
			unsigned long returned;		// ����� ��� ���������� ����������� ����

			memset(&myspti, 0, sizeof(scsi_st));	// ������������� ���������
			myspti.t_spti.Length = sizeof(SCSI_PASS_THROUGH_DIRECT); // �����
            // ���������� ����� ���������� (� ����� ���������� ����� ���� ��������� ����������)
			myspti.t_spti.Lun = 0;
           	// ������� ���������� ��� ���������� �� ����
			myspti.t_spti.TargetId = 0;
			myspti.t_spti.PathId = 0;		// SCSII-���� ��� ���� ��� �������.
            // ����� command descriptor block (CDB, ��� ����� ������
            // �� 0x1F ����� 6)
			myspti.t_spti.CdbLength = 6;
			myspti.t_spti.DataIn = SCSI_IOCTL_DATA_IN;	// �� �����
            // ����� ����� sensebuf � ��� ��������
			myspti.t_spti.SenseInfoLength = 32;
			myspti.t_spti.SenseInfoOffset = sizeof(SCSI_PASS_THROUGH_DIRECT)
					+ sizeof(DWORD);
            // ������� �������� ��������� ��������
			myspti.t_spti.TimeOutValue = 10;
			myspti.t_spti.DataTransferLength = 36;	// ����� ������ ��� ������
			myspti.t_spti.DataBuffer = vbuf;		// ��������� �� ����� ������
			// ���������� CDB (���� �������� �������):
			myspti.t_spti.Cdb[0] = 0x12;	// ��� ������� INQUIRY
			myspti.t_spti.Cdb[4] = 0x24;	// 36 - ������ ������

			if (DeviceIoControl(hDevice,				// ���������� ����������
                    // dwIoControlCode ����������� ��� �������� -
                    //   ��������� ��� �������� CDB
					IOCTL_SCSI_PASS_THROUGH_DIRECT,
					&myspti,						// ������� �����
					sizeof(scsi_st),				// ��� ������
					&myspti,						// �������� �����
					sizeof(scsi_st),				// ��� ������
					&returned,		// ������� ������ �������� (���� �� �������)
					NULL)) {			// ��������� �� OVERLAPPED, �� �����.

				vbuf[36] = 0;					// ��� �������� ������ � �������
				_tprintf(_T("PDT = %x\n"), vbuf[0]);	// ��� ���������� SCSII
                // �������/���
				_tprintf(_T("RMB = %x\n"), (vbuf[1] & 0x080) >> 7);
				_tprintf(_T("ver. SPC = %x\n"), vbuf[2]);	// ������ SPC
                // ��������� ����������� �������������
				_tprintf(_T("vendor = %s\n"), &vbuf[8]);
                // ��������� ����������� ��������
				_tprintf(_T("product = %s\n"), &vbuf[16]);
                // ��������� ����������� ������
				_tprintf(_T("ver = %s\n"), &vbuf[32]);

				if (!_tcscmp(&vbuf[8], _T("LUFA"))) {// ����� ������ ����������
					_tprintf(_T("--- This is my device! ---\n"));

					// ----- ������ � ����������� -----
					int diskNum = 0;				// ���������� ������
                    // ������ ��� "\\\\.\\PhysicalDrive%d"
					char physicalDrive[50] = { 0 };	
					DWORD bufsize, bufsizeret;		// ������� ������
                    // ����� ��� GET_VOLUME_DISK_EXTENTS
					VOLUME_DISK_EXTENTS buf_ioctl;
					bufsize = sizeof(VOLUME_DISK_EXTENTS);

					BOOL result;
					UCHAR q[512 * 4 * 2];
					DWORD q1, q2 = 0;
					q1 = 512 * 2 * 1;
					//q1 = 512;
					q[0] = 0x17;

					result = DeviceIoControl(hDevice,
                            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                            NULL, 0, &buf_ioctl, bufsize, &bufsizeret,
                            NULL);
					if (result == 0) {
						OutFormatMsg("PhysicDiscErr");
					} else {
						_tprintf("DiscNum = %lu\n",
								buf_ioctl.NumberOfDiskExtents);
						diskNum = buf_ioctl.Extents[0].DiskNumber;
						_tprintf("Disk = %d\n", diskNum);
						if (diskNum != 0) {
							sprintf(physicalDrive, "\\\\.\\PhysicalDrive%d",
									diskNum);
						}
						_tprintf("physicalDrive: %s\n", physicalDrive);
					}

					HANDLE hDevice2 = CreateFile(physicalDrive,
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
                                NULL);
					if (hDevice2 == INVALID_HANDLE_VALUE) {
						OutFormatMsg("CreateFile 2 Error");
					} else {
						_tprintf(_T("CreateFile 2 done! \n"));

						SetFilePointer(hDevice2, 5, NULL, FILE_BEGIN);

						ZeroMemory(&myspti, sizeof(scsi_st));

						myspti.t_spti.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
						myspti.t_spti.PathId = 0;
						myspti.t_spti.TargetId = 0;
						myspti.t_spti.Lun = 0;
						myspti.t_spti.CdbLength = 10;
						myspti.t_spti.DataIn = SCSI_IOCTL_DATA_OUT;
						myspti.t_spti.SenseInfoLength = 32;
						myspti.t_spti.DataTransferLength = q1;
						myspti.t_spti.TimeOutValue = 10;
						myspti.t_spti.DataBuffer = &q;
						myspti.t_spti.SenseInfoOffset =
								sizeof(SCSI_PASS_THROUGH_DIRECT)
										+ sizeof(DWORD);
						//myspti.t_spti.Cdb[0] = 0x2a; //SCSIOP_WRITE;
						myspti.t_spti.Cdb[0] = 0xC1; 	//;

						myspti.t_spti.Cdb[2] = 0x00;
						myspti.t_spti.Cdb[3] = 0x00;
						myspti.t_spti.Cdb[4] = 0x00;
						myspti.t_spti.Cdb[5] = 0x00;

						myspti.t_spti.Cdb[7] = 0x00;
						myspti.t_spti.Cdb[8] = 0x02;

						ULONG length = sizeof(scsi_st);
						result = DeviceIoControl(hDevice2,
						IOCTL_SCSI_PASS_THROUGH_DIRECT, &myspti, length,
								&myspti, length, &q2,
								FALSE);
						if (result == 0) {
							OutFormatMsg("Write Error DevIoCtl");
						} else {
							_tprintf("Write done\n");
							_tprintf("len = %lu\n", q2);
						}

						q2 = 0;
						myspti.t_spti.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
						myspti.t_spti.PathId = 0;
						myspti.t_spti.TargetId = 0;
						myspti.t_spti.Lun = 0;
						myspti.t_spti.CdbLength = 10;
						myspti.t_spti.DataIn = SCSI_IOCTL_DATA_IN;
						myspti.t_spti.SenseInfoLength = 32;
						myspti.t_spti.DataTransferLength = q1;
						myspti.t_spti.TimeOutValue = 10;
						myspti.t_spti.DataBuffer = &q;
						myspti.t_spti.SenseInfoOffset =
								sizeof(SCSI_PASS_THROUGH_DIRECT)
										+ sizeof(DWORD);

						myspti.t_spti.Cdb[0] = 0x28; //SCSIOP_READ;

						myspti.t_spti.Cdb[2] = 0x00;
						myspti.t_spti.Cdb[3] = 0x00;
						myspti.t_spti.Cdb[4] = 0x00;
						myspti.t_spti.Cdb[5] = 0x00;

						myspti.t_spti.Cdb[7] = 0x00;
						myspti.t_spti.Cdb[8] = 0x02;

						result = DeviceIoControl(hDevice2,
						IOCTL_SCSI_PASS_THROUGH_DIRECT, &myspti, q1, &myspti,
								q1, &q2,
								FALSE);
						if (result == 0) {
							OutFormatMsg("Read Error DevIoCtl");
						} else {
							_tprintf("data_2 = %x\n", q[0]);
							_tprintf("len = %lu\n", q2);
						}

						CloseHandle(hDevice2);
					}
					// --------------------------------
				}
				_tprintf(_T("\n"));			// ��� ��������������� ������ ������
			}
			CloseHandle(hDevice);
		}
		LocalFreeIf(pDeviceInterfaceDetailData);
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return 0;
}
