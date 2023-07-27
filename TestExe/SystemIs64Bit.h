#pragma once
#include "Singletion.h"
#include <Windows.h>

class CSystemIs64Bit
{
	DECLARE_SINGLETON_CLASS(CSystemIs64Bit);
public:
	CSystemIs64Bit(void);
	~CSystemIs64Bit(void);

	BOOL	IsX64Systemt() { return m_IsX64; }
	BOOL	IsWow64() { return m_IsWow64; }

	VOID	Wow64DisableWow64FsRedirection(PVOID * oldValue);
	VOID	Wow64RevertWow64FsRedirection(PVOID oldValue);

	BOOL	IsWin7() { return m_IsWin7; }

private:
	BOOL	m_IsX64;
	BOOL	m_IsWow64;
	BOOL	m_IsWin7;

	PVOID	m_oldValue;
};

typedef ISingleton::CSingleton<CSystemIs64Bit> CSystemBit;