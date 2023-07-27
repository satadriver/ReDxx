// TestExe.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "SystemIs64Bit.h"
#include "Driver.h"
#include "Convert.h"
#include "SampleLog.h"

BOOL AddCfgFile(CStringA strDir)
{
	CStringW strDirW;

	strDir = strDir.Trim();
	if( strDir.IsEmpty() )
		return FALSE;

	strDir += "\\";
	strDir.Replace("\\\\","\\");

	strDirW = L"\\??\\";
	strDirW += CSystem::ConvertA2W(strDir);

	if( DeviceIoControlEx(DRIVERCODE_WIN32_DEVICE_NAME_A,IOCTL_DRIVERCODE_ADDCFG,(LPVOID)(LPCWSTR)strDirW,(strDirW.GetLength() + 1) << 1,NULL,0,NULL) )
		return TRUE;

	return FALSE;
}


int _tmain(int argc, _TCHAR* argv[])
{
	do
	{
		CDriver Drv;
		CHAR szDir[MAX_PATH] = {0};

		if( !Drv.Install(NULL,"RedXx","C:\\windows\\redxx.sys")) 
		{
			DbgInfo(DBGINFO,"��װ����ʧ��.\r\n");
			break;
		}
		DbgInfo(DBGINFO,"��װ�����ɹ�.\r\n");

		{
			szDir[0] = '\0';
			::GetModuleFileNameA(NULL,szDir,sizeof(szDir));
			CHAR* pp = strrchr(szDir,'\\');
			if( pp != NULL )
			{
				pp++;
				*pp = '\0';
			}
		}

		if( !AddCfgFile(szDir) )
		{
			DbgInfo(DBGINFO,"add cfg file failed.\r\n");
			break;
		}

		DbgInfo(DBGINFO,"program start ok.\r\n");
	}
	while(FALSE);

	return 0;
}

