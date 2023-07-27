//This file is used to build a precompiled header
#include "stdafx.h"

PGLOBAL_DATA GetGlobal(PDRIVER_OBJECT DriverObject,PDEVICE_OBJECT DevObj)
{
	static BOOLEAN		bInitGlobal = FALSE;
	static GLOBAL_DATA	Global;

	if( !bInitGlobal )
	{
		bInitGlobal = TRUE;
		RtlZeroMemory(&Global,sizeof(Global));
		Global.CtlDevice = DevObj;
		Global.DriverObject = DriverObject;
		InitGlobalData();
	}

	return &Global;
}

VOID InitGlobalData()
{
	ULONG i;
	PGLOBAL_DATA pGlobal = GetGlobal(NULL,NULL);

	if( pGlobal == NULL )
		return;

	PsGetVersion(&pGlobal->MajorVersion, &pGlobal->MinorVersion, &pGlobal->BuildNumber, 0);

	KeInitializeSpinLock(&pGlobal->slConnLock);
	InitializeListHead(&pGlobal->leConnList);

	KeInitializeSpinLock(&pGlobal->csTargetLock);
	KeInitializeSpinLock(&pGlobal->csUrlLock);

	InitializeListHead(&pGlobal->TargetList);


	for(i = 0 ; i < MAX_HASH_NUM; i++)
		InitializeListHead(&pGlobal->UrlList[i]);
}
