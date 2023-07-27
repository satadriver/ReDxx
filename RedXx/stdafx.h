#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifdef __cplusplus
extern "C" 
{

#endif

#include "VisualDDKHelpers.h"
#include <ntddk.h>
#include <ntddstor.h>
#include <mountdev.h>
#include <ntddvol.h>
#include <tdikrnl.h>

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#include "memini.h"

#define FILE_DEVICE_DRIVERCODE	0x8000
#define DRIVERCODE_IOCTL_BASE	0x800
#define CTL_CODE_DRIVERCODE(i)	\
	CTL_CODE(FILE_DEVICE_DRIVERCODE, DRIVERCODE_IOCTL_BASE+i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DRIVERCODE_ADDCFG		CTL_CODE_DRIVERCODE(11)
#define IOCTL_DRIVERCODE_CLRCFG		CTL_CODE_DRIVERCODE(12)

#pragma pack(push,1)

typedef struct _ADPT_PCRE_URL2
{
	USHORT   usTotalLength;
	USHORT	 usHtmlIdx;		//主机HASH数值
	USHORT	 usPrceLen;
	ULONG32	 ulHash;
	CHAR	 szData[2];
}ADPT_PCRE_URL2,*PADPT_PCRE_URL2;

#pragma pack(pop)

typedef struct _ADPT_PCRE_URL_CONTEXT2
{
	LIST_ENTRY		ListEntry;
	//	LIST_ENTRY		ReportList;
	ADPT_PCRE_URL2	Url;
}ADPT_PCRE_URL_CONTEXT2,*PADPT_PCRE_URL_CONTEXT2;

typedef struct
{
	LIST_ENTRY	ListEntry;
	ULONG		ulIdx;
	PCHAR		strTarget;
	ULONG		ulLen;
}TG_HTML,*PTG_HTML;

typedef struct
{
	LIST_ENTRY			ListEntry;
	PFILE_OBJECT		pFileObj;
	PFILE_OBJECT		pAddrObj;
	PVOID				pConnCtx;
	PVOID				EventRecviceHandler;
	PVOID				EventRecviceContext;
	PVOID				EventChainedRecviceHandler;
	PVOID				EventChainedRecviceContext;
	PVOID				EventChainedRecviceHandlerEx;
	PVOID				EventChainedRecviceContextEx;
	ULONG				ulIdx;
	BOOLEAN				bJmp;
}ADPT_CONN_CONTEXT,*PADPT_CONN_CONTEXT;

#define MAX_HASH_NUM		4096

typedef struct{
	ULONG					MajorVersion;
	ULONG					MinorVersion;
	ULONG					BuildNumber;
	PDRIVER_OBJECT			DriverObject;
	PDEVICE_OBJECT			CtlDevice;

	//链接上下文
	LIST_ENTRY				leConnList;
	KSPIN_LOCK				slConnLock;

	KSPIN_LOCK				csTargetLock;
	LIST_ENTRY				TargetList;

	LIST_ENTRY				UrlList[MAX_HASH_NUM];
	KSPIN_LOCK				csUrlLock;

	PDEVICE_OBJECT			FltDevice;
	PDEVICE_OBJECT			LowerDevice;

}GLOBAL_DATA,*PGLOBAL_DATA;

PVOID   KmAlloc( SIZE_T PoolSize );
VOID    KmFree( PVOID PoolBase );

PGLOBAL_DATA GetGlobal(PDRIVER_OBJECT DriverObject,PDEVICE_OBJECT DevObj);
VOID		 InitGlobalData();
VOID		 UninitGlobalData();