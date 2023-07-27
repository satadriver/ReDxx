#pragma once
#include "Singletion.h"

class CDriver
{
public:
	CDriver(void);
	~CDriver(void);

	BOOL	Install(HMODULE h,LPCSTR strSevice,LPCSTR strFile);
	BOOL	AddProcess(LPCSTR strProcesses);
	BOOL	StartFilter(USHORT usPort);
	BOOL	StopFilter();
	BOOL	ClearProcess();
	BOOL	GetConnectInfo(ULONG ulAddr,USHORT usPort,ULONG & ulOut,USHORT & usOut,ULONG64 & ulPid);

protected:
	BOOL	ReleaseResource(HMODULE hModule, WORD wResource, LPCSTR lpType,LPCSTR strFile);
	BOOL	fixServicePath(CStringA strName,CStringA strPath);

private:
	HANDLE		m_hDev;

};

BOOL DeviceIoControlEx(CHAR *pDevName,DWORD dwIoControlCode,LPVOID lpInBuffer,DWORD nInBufferSize,LPVOID lpOutBuffer,DWORD nOutBufferSize,LPDWORD lpBytesReturned);