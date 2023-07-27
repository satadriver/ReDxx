#include "StdAfx.h"
#include <stdio.h>
#include "SampleLog.h"

int DbgInfo(const CHAR* func, const CHAR* file, int line, const char* fmt, ...)
{
// 	do
// 	{
// 		HANDLE hEv = ::OpenEventA(NULL,FALSE,"Global\\{DBF1FF5B-4684-48d4-B711-1503FB410F40}");
// 		if( hEv != NULL && hEv != INVALID_HANDLE_VALUE )
// 		{
// 			if( WaitForSingleObject(hEv,0) == WAIT_OBJECT_0 )
// 			{
// 				CloseHandle(hEv);
// 				break;
// 			}
// 
// 			::CloseHandle(hEv);
// 		}
// 
// 		return 0;
// 	}while(FALSE);

	CHAR	szBuffer[8192];
	va_list argptr;
	va_start(argptr, fmt);
	int idx = sprintf(szBuffer, "[%s][%s][%s][%d]", APPNAME, file, func, line);
	int index = vsnprintf(&szBuffer[idx], 8191 - idx, fmt, argptr) + idx;
	va_end(argptr);
	OutputDebugStringA(szBuffer);
	printf("%s",szBuffer);
	return index;
}
