// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
// TODO: 在此处引用程序需要的其他头文件

#define DRIVERCODE_WIN32_DEVICE_NAME_A	"\\\\.\\RedXx0"

#define FILE_DEVICE_DRIVERCODE	0x8000
#define DRIVERCODE_IOCTL_BASE	0x800
#define CTL_CODE_DRIVERCODE(i)	\
	CTL_CODE(FILE_DEVICE_DRIVERCODE, DRIVERCODE_IOCTL_BASE+i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DRIVERCODE_ADDCFG		CTL_CODE_DRIVERCODE(11)
#define IOCTL_DRIVERCODE_CLRCFG		CTL_CODE_DRIVERCODE(12)
