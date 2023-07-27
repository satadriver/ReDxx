#include "StdAfx.h"
#include <WinIoCtl.h>
#include "Driver.h"
#include "SampleLog.h"
#include "SystemIs64Bit.h"
#include <vector>
#include "Convert.h"
#include "resource.h"

#pragma comment(lib,"ws2_32.lib")


BOOL DeviceIoControlEx(CHAR *pDevName,DWORD dwIoControlCode,LPVOID lpInBuffer,DWORD nInBufferSize,LPVOID lpOutBuffer,DWORD nOutBufferSize,LPDWORD lpBytesReturned)
{
	BOOL  bRet = FALSE;
	HANDLE hDriver = CreateFileA(pDevName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (INVALID_HANDLE_VALUE != hDriver)
	{
		DbgInfo(DBGINFO,"Open Device Ok\n");
		DWORD dwRet;
		bRet = ::DeviceIoControl(hDriver, dwIoControlCode,lpInBuffer,nInBufferSize,lpOutBuffer,nOutBufferSize,&dwRet,NULL);
		if( lpBytesReturned )
			*lpBytesReturned = dwRet;
		if( !bRet )
		{
			DbgInfo(DBGINFO,"Device IoControl Failed(%08x) （%d)\n",dwIoControlCode,GetLastError());
		}
		else
		{
			DbgInfo(DBGINFO,"Device IoControl Ok(%08x)\n",dwIoControlCode);
		}
		CloseHandle(hDriver);
	}
	else
	{
		DbgInfo(DBGINFO,"Open Device Failed\n");
	}

	return bRet;
}

CDriver::CDriver(void)
: m_hDev(INVALID_HANDLE_VALUE)
{
}

CDriver::~CDriver(void)
{

}

BOOL CDriver::Install(HMODULE hMod,LPCSTR strSevice,LPCSTR strFile)
{
	WORD	dwRes64Id = IDR_DRV64,dwRes32Id = IDR_DRV32;
	if( CSystemBit::Instance()->IsX64Systemt() )
	{
		if (!ReleaseResource(hMod, dwRes64Id, "BIN",strFile)){
			DbgInfo(DBGINFO, "资源释放失败!");
			::DeleteFileA(strFile);
			return FALSE;
		}
	}
	else
	{
		if (!ReleaseResource(hMod, dwRes32Id, "BIN",strFile)){
			DbgInfo(DBGINFO, "资源释放失败!");
			::DeleteFileA(strFile);
			return FALSE;
		}
	}

	SC_HANDLE	hSCManager, hService, hService2;
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	hService = CreateServiceA(
		hSCManager,
		strSevice,
		strSevice,//"Win7HP.sys",
		SERVICE_ALL_ACCESS,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		strFile,//"C:\\Win7HP.sys",
		NULL, NULL, NULL, NULL, NULL
		);

	if (!hService && GetLastError()!= ERROR_SERVICE_EXISTS){
		DbgInfo(DBGINFO, "创建服务失败 Failed(%d)!",GetLastError());
		::DeleteFileA(strFile);
		return FALSE;
	}

	CloseServiceHandle(hService);

	fixServicePath(strSevice,strFile);

	//打开服务
	hService2 = OpenServiceA(
		hSCManager,
		strSevice,
		SERVICE_ALL_ACCESS
		);
	if (!hService2){
		DbgInfo(DBGINFO, "打开服务失败 Failed(%d)!", GetLastError());
		::DeleteFileA(strFile);
		return FALSE;
	}

	//启动服务
	int result = StartService(hService2,0,NULL); 
	if (result == 0 && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING ){
		DbgInfo(DBGINFO, "启动服务失败 Failed(%d)!", GetLastError());
		::DeleteFileA(strFile);
		return FALSE;
	}

	CloseServiceHandle(hSCManager);
	CloseServiceHandle(hService2);
	::DeleteFileA(strFile);

	return TRUE;
}

BOOL CDriver::ReleaseResource(HMODULE hModule, WORD wResource, LPCSTR lpType,LPCSTR strFile)
{
	HGLOBAL hRes;
	HRSRC hResInfo;
	HANDLE hFile;
	DWORD dwBytes;

	hResInfo = FindResourceA(hModule, MAKEINTRESOURCEA(wResource), lpType);
	if (hResInfo == NULL)
		return FALSE;

	hRes = LoadResource(hModule, hResInfo);
	if (hRes == NULL)
		return FALSE;

	hFile = CreateFileA(strFile,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFile == NULL)
	{
		FreeResource(hRes);
		return FALSE;
	}

	WriteFile(hFile, hRes, SizeofResource(hModule, hResInfo), &dwBytes, NULL);

	CloseHandle(hFile);
	FreeResource(hRes);
	return TRUE;
}

BOOL CDriver::fixServicePath(CStringA strName,CStringA strPath)
{
	CStringA strSubkey;
	HKEY hkey;
	DWORD dwDisposition;
	LSTATUS lsRet = -1;

	lsRet = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\services\\" + strName, 0, NULL, 0, KEY_WRITE, NULL,&hkey, &dwDisposition); //RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\services\\" + strName,0,KEY_WRITE,&hkey);//
	if (lsRet != ERROR_SUCCESS)
	{
		return FALSE;
	}

	strPath = "\\??\\" + strPath;
	lsRet = RegSetValueExA(hkey,"ImagePath",0,REG_EXPAND_SZ,(CONST BYTE*)(LPCSTR)strPath,1 + strPath.GetLength());
	::RegCloseKey(hkey);
	if (lsRet != ERROR_SUCCESS)
	{
		return FALSE;
	}

	//SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
	//Sleep(100);
	return TRUE;
}
