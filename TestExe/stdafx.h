// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
// TODO: �ڴ˴����ó�����Ҫ������ͷ�ļ�

#define DRIVERCODE_WIN32_DEVICE_NAME_A	"\\\\.\\RedXx0"

#define FILE_DEVICE_DRIVERCODE	0x8000
#define DRIVERCODE_IOCTL_BASE	0x800
#define CTL_CODE_DRIVERCODE(i)	\
	CTL_CODE(FILE_DEVICE_DRIVERCODE, DRIVERCODE_IOCTL_BASE+i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DRIVERCODE_ADDCFG		CTL_CODE_DRIVERCODE(11)
#define IOCTL_DRIVERCODE_CLRCFG		CTL_CODE_DRIVERCODE(12)
