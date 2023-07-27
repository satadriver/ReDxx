#include "StdAfx.h"
#include "SystemIs64Bit.h"

CSystemIs64Bit::CSystemIs64Bit(void)
{
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;
	BOOL bIsWow64 = FALSE;
	m_IsX64 = FALSE;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress( GetModuleHandleA("kernel32"),"IsWow64Process");
	if (NULL != fnIsWow64Process)     
	{         
		fnIsWow64Process(GetCurrentProcess(),&bIsWow64);
	}     
	
	m_IsWow64 = bIsWow64; 

	SYSTEM_INFO  si;

	GetSystemInfo(&si);

	if((si.wProcessorArchitecture & PROCESSOR_ARCHITECTURE_IA64)
		|| (si.wProcessorArchitecture & PROCESSOR_ARCHITECTURE_AMD64) )
		m_IsX64 = TRUE;

	m_oldValue = NULL;
	m_IsWin7 = FALSE;

	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	if( GetVersionEx((OSVERSIONINFO*) &osvi) && osvi.dwMajorVersion >= 6 )
	{
		m_IsWin7 = TRUE;
	}

#ifdef _AMD64_
	m_IsX64 = TRUE;
#else
	if( fnIsWow64Process == NULL )
		m_IsX64 = FALSE;
	else
		m_IsX64 = bIsWow64;
#endif

	printf("%s : %s\r\n",IsWin7() ? "Win7" : "Xp",IsX64Systemt() ? "64" : "32");
}

CSystemIs64Bit::~CSystemIs64Bit(void)
{
}

VOID CSystemIs64Bit::Wow64DisableWow64FsRedirection(PVOID * oldValue)
{
	typedef BOOL(WINAPI *LPFN_Wow64DisableWow64FsRedirection)(PVOID *OldValue);
	LPFN_Wow64DisableWow64FsRedirection fnWow64DisableWow64FsRedirection;

	fnWow64DisableWow64FsRedirection = (LPFN_Wow64DisableWow64FsRedirection)GetProcAddress(	GetModuleHandle(TEXT("kernel32.dll")), "Wow64DisableWow64FsRedirection");

	if (fnWow64DisableWow64FsRedirection != NULL)
	{
		fnWow64DisableWow64FsRedirection(oldValue);
	}
}

VOID CSystemIs64Bit::Wow64RevertWow64FsRedirection(PVOID oldValue)
{
	typedef	BOOL(WINAPI *LPFN_Wow64RevertWow64FsRedirection)(PVOID OlValue);
	LPFN_Wow64RevertWow64FsRedirection fnWow64RevertWow64FsRedirection;
	fnWow64RevertWow64FsRedirection = (LPFN_Wow64RevertWow64FsRedirection)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "Wow64RevertWow64FsRedirection");

	if (fnWow64RevertWow64FsRedirection != NULL)
	{
		fnWow64RevertWow64FsRedirection(oldValue);
	}
}
