#include "stdafx.h"
#include "memini.h"

#define  INI_MEM_TAG	'T*7y'

#pragma pack(push,1)
typedef struct _AD_MEM_INI_SECTION
{
	LIST_ENTRY		ListEntry;
	LIST_ENTRY		leKeys;
	USHORT			NameLen;
	CHAR			szData[2];
}AD_MEM_INI_SECTION,*PAD_MEM_INI_SECTION;

typedef struct _AD_MEM_INI_KEY
{
	LIST_ENTRY		ListEntry;
	USHORT			NameLen;
	ULONG32			StringLen;
	CHAR			szData[2];
}AD_MEM_INI_KEY,*PAD_MEM_INI_KEY;
#pragma pack(pop)

BOOLEAN	MemIni_GetLines(PAD_MEM_INI Ini,PCHAR szMem);
BOOLEAN MemIni_RemoveNotes(PCHAR lpLine,PCHAR * lpLine1,CONST CHAR* lpFlags);

VOID MemIni_Initialize(PAD_MEM_INI Ini,CHAR* szMem)
{
	INT	nSize;
	if( Ini != NULL && szMem != NULL )
	{
		InitializeListHead(&Ini->leSections);
		Ini->pCurSection = NULL;
		if( !MemIni_GetLines(Ini,szMem) )
			MemIni_Uninitialize(Ini);
	}
}

VOID MemIni_Uninitialize(PAD_MEM_INI Ini)
{
	if( Ini != NULL )
	{
		Ini->pCurSection = NULL;
		while( !IsListEmpty(&Ini->leSections) )
		{
			PAD_MEM_INI_SECTION lpSection = CONTAINING_RECORD(RemoveTailList(&Ini->leSections),AD_MEM_INI_SECTION,ListEntry);
			while(!IsListEmpty(&lpSection->leKeys))
			{
				PAD_MEM_INI_KEY pKey = CONTAINING_RECORD(RemoveTailList(&lpSection->leKeys),AD_MEM_INI_KEY,ListEntry);
				KmFree(pKey);
			}
			KmFree(lpSection);
		}
	}
}

BOOLEAN MemIni_GetLines(PAD_MEM_INI Ini,PCHAR szMem)
{
	PCHAR	start,end;
	INT		l,nSize;
	BOOLEAN	bRet = FALSE;
	PCHAR	strLine = NULL,strLine1;

	if( !Ini || !szMem || !IsListEmpty(&Ini->leSections) )
		return FALSE;

	start = szMem;
	while(1)
	{
		end = strstr(start,"\r\n");
		if( end == NULL )
		{
			l = strlen(start);
		}
		else
		{
			l = (INT)(end - start);
		}

		strLine = (PCHAR)KmAlloc(l+1);
		if( strLine == NULL )
			break;

		RtlCopyMemory(strLine,start,l);
		strLine[l] = '\0';
		if( !MemIni_RemoveNotes(strLine,&strLine1,"##") )
			break;

		l = strlen(strLine1);
		if( l > 0 )
		{
			PCHAR ptr;
			if( strLine1[0] == '[' )
			{
				PAD_MEM_INI_SECTION lpLine;
				strLine1++;
				if( !MemIni_RemoveNotes(strLine1,&ptr,"]") )
					break;

				l = strlen(ptr);
				if( l <= 0 )
					break;

				lpLine = (PAD_MEM_INI_SECTION)KmAlloc(sizeof(AD_MEM_INI_SECTION) + l);
				if( lpLine == NULL )
					break;

				InitializeListHead(&lpLine->leKeys);
				lpLine->NameLen = (USHORT)l;
				RtlCopyMemory(lpLine->szData,ptr,l);
				lpLine->szData[l] = '\0';
				InsertTailList(&Ini->leSections,&lpLine->ListEntry);
				Ini->pCurSection = lpLine;
			}
			else
			{
				int m;
				PAD_MEM_INI_KEY lpLine;
				if( Ini->pCurSection == NULL )
					break;

				ptr = strchr(strLine1,'=');
				if( ptr == NULL )
					break;

				*ptr = '\0';
				if( !MemIni_RemoveNotes(strLine1,&strLine1,NULL) )
					break;

				l = strlen(strLine1);
				if( l <= 0 )
					break;

				ptr ++;
				if( !MemIni_RemoveNotes(ptr,&ptr,NULL) )
					break;

				m = strlen(ptr);
				lpLine = (PAD_MEM_INI_KEY)KmAlloc(sizeof(AD_MEM_INI_KEY) + l + m);
				if( lpLine == NULL )
					break;

				lpLine->NameLen = l + 1;
				RtlCopyMemory(lpLine->szData,strLine1,l);
				lpLine->szData[l] = '\0';
				l++;
				RtlCopyMemory(&lpLine->szData[l],ptr,m);
				lpLine->szData[l+m] = '\0';
				lpLine->StringLen = m;
				InsertTailList(&((PAD_MEM_INI_SECTION)Ini->pCurSection)->leKeys,&lpLine->ListEntry);
			}
		}

		KmFree(strLine);
		strLine = NULL;
		if( end == NULL )
		{
			if( !IsListEmpty(&Ini->leSections) )
			{
				bRet = TRUE;
			}
			break;
		}

		start = end + 2;
	}

	if( strLine != NULL )
	{
		KmFree(strLine);
		strLine = NULL;
	}

	return bRet;
}

BOOLEAN MemIni_RemoveNotes(PCHAR lpLine,PCHAR * lpLine1,CONST CHAR* lpFlags)
{
	int l;
	PCHAR ptr;
	if( lpLine == NULL || lpLine1 == NULL )
		return FALSE;

	*lpLine1 = NULL;
	if( lpFlags == NULL )
	{
		l = strlen(lpLine);
		if( l <= 0 )
			ptr = lpLine;
		else
			ptr = lpLine + l - 1; //123 \0
	}
	else
	{
		l = strlen(lpFlags);
		if( l <= 0 )
			return FALSE;

		if( l == 1 )
			ptr = strchr(lpLine,lpFlags[0]);
		else
			ptr = strstr(lpLine,(const char*)lpFlags);

		if( ptr != NULL )
		{
			*ptr = '\0';
			ptr--;
		}
		else
		{
			l = strlen(lpLine);
			if( l <= 0 )
				ptr = lpLine;
			else
				ptr = lpLine + l - 1; //123 \0
		}
	}
	
	while( ptr > lpLine && ( (*ptr) == ' ' || (*ptr) == '\t' ) ) 
	{
		*ptr = '\0';
		ptr--;
	}

	while( (*lpLine) == ' ' || (*ptr) == '\t' ) lpLine++;
	*lpLine1 = lpLine;
	return TRUE;
}

BOOLEAN MemIni_IsValid(PAD_MEM_INI Ini)
{
	if( Ini == NULL )	return FALSE;
	if( !IsListEmpty(&Ini->leSections) )
		return TRUE;

	return FALSE;
}

BOOLEAN MemIni_GetSection(PAD_MEM_INI Ini,PCHAR * lpTmp,INT * nSize)
{
	PCHAR lpStr = NULL;
	INT nTmp = 0;
	PLIST_ENTRY		pList;

	if( Ini == NULL || IsListEmpty(&Ini->leSections) || lpTmp == NULL || nSize == NULL )
		return FALSE;

	*lpTmp = NULL;
	*nSize = 0;
	for( pList = Ini->leSections.Flink; pList != (&Ini->leSections) ; pList = pList->Flink )
	{
		PAD_MEM_INI_SECTION pTmp = CONTAINING_RECORD(pList,AD_MEM_INI_SECTION,ListEntry);
		nTmp += pTmp->NameLen + 1;
	}

	if( nTmp > 0 )
	{
		INT nLen = 0;
		nTmp++;
		lpStr = (PCHAR)KmAlloc(nTmp);
		if( lpStr != NULL )
		{
			for( pList = Ini->leSections.Flink; pList != (&Ini->leSections) ; pList = pList->Flink )
			{
				PAD_MEM_INI_SECTION pTmp = CONTAINING_RECORD(pList,AD_MEM_INI_SECTION,ListEntry);
				RtlCopyMemory(&lpStr[nLen],pTmp->szData,pTmp->NameLen + 1);
				nLen += pTmp->NameLen + 1;
			}

			lpStr[nLen] = '\0';
			*lpTmp = lpStr;
			*nSize = nTmp;
			return TRUE;
		}
	}
	
	return FALSE;
}

BOOLEAN MemIni_SectionExists(PAD_MEM_INI Ini,LPCSTR strSectionName)
{
	PLIST_ENTRY		pList;

	if( Ini == NULL || IsListEmpty(&Ini->leSections) || strSectionName == NULL )
		return FALSE;

	for( pList = Ini->leSections.Flink; pList != (&Ini->leSections) ; pList = pList->Flink )
	{
		PAD_MEM_INI_SECTION pTmp = CONTAINING_RECORD(pList,AD_MEM_INI_SECTION,ListEntry);
		
		if( _stricmp(pTmp->szData,strSectionName) == 0 )
			return TRUE;
	}

	return FALSE;
}

BOOLEAN MemIni_ReadString(PAD_MEM_INI Ini,LPCSTR strSectionName,LPCSTR strValueName,PCHAR * strValue,INT * nStrLen)
{
	PAD_MEM_INI_SECTION	pCurSection;
	PLIST_ENTRY		pList;
	PCHAR			pSecName;
	PCHAR			pValName;
	INT				m,n;

	if( Ini == NULL || strSectionName == NULL || strValueName == NULL || strValue == NULL || nStrLen == NULL )
		return FALSE;

	*strValue = NULL;
	*nStrLen = 0;

	m = strlen(strSectionName);
	n = strlen(strValueName);
	if( m <= 0 || n <= 0 )
		return FALSE;

	m++;
	n++;
	pSecName = (PCHAR)KmAlloc(m);
	if( pSecName == NULL )	return FALSE;
	pValName = (PCHAR)KmAlloc(n);
	if( pValName == NULL )
	{
		KmFree(pSecName);
		return FALSE;
	}

	RtlCopyMemory(pSecName,strSectionName,m);
	RtlCopyMemory(pValName,strValueName,n);

	if( !MemIni_RemoveNotes(pSecName,(PCHAR*)&strSectionName,NULL) )
	{
		KmFree(pSecName);
		KmFree(pValName);
		return FALSE;
	}

	if( !MemIni_RemoveNotes(pValName,(PCHAR*)&strValueName,NULL) )
	{
		KmFree(pSecName);
		KmFree(pValName);
		return FALSE;
	}

	if( strlen(strSectionName) <= 0 || strlen(strValueName) <= 0 )
	{
		KmFree(pSecName);
		KmFree(pValName);
		return FALSE;
	}

	if( Ini->pCurSection != NULL )
	{
		pCurSection = (PAD_MEM_INI_SECTION)Ini->pCurSection;
		if( _stricmp(pCurSection->szData,strSectionName) == 0 )
		{
__next:
			for( pList = pCurSection->leKeys.Flink; pList != (&pCurSection->leKeys) ; pList = pList->Flink )
			{
				PAD_MEM_INI_KEY pTmp = CONTAINING_RECORD(pList,AD_MEM_INI_KEY,ListEntry);
				if( _stricmp(pTmp->szData,strValueName) == 0 )
				{
					PCHAR pStr = (PCHAR)KmAlloc(pTmp->StringLen+1);
					if( pStr == NULL )
					{
						KmFree(pSecName);
						KmFree(pValName);
						return FALSE;
					}

					RtlCopyMemory(pStr,&pTmp->szData[pTmp->NameLen],pTmp->StringLen+1);
					*strValue = pStr;
					*nStrLen = pTmp->StringLen;
					KmFree(pSecName);
					KmFree(pValName);
					return TRUE;
				}
			}

			KmFree(pSecName);
			KmFree(pValName);
			return FALSE;
		}
	}

	for( pList = Ini->leSections.Flink; pList != (&Ini->leSections) ; pList = pList->Flink )
	{
		pCurSection = CONTAINING_RECORD(pList,AD_MEM_INI_SECTION,ListEntry);
		if( _stricmp(pCurSection->szData,strSectionName) == 0 )
		{
			Ini->pCurSection = pCurSection;
			goto __next;
		}
	}

	KmFree(pSecName);
	KmFree(pValName);
	return FALSE;
}

VOID MemIni_MemFree(PVOID pMem)
{
	if( pMem != NULL )
		KmFree(pMem);
}

int xtoi(PCHAR pStart)
{
	int m = 0;
	int i = 0;

	while( (*pStart) == ' ' ) pStart++;
	for(;;i++)
	{
		if( pStart[i] < '0' || pStart[i] > '9' )
			break;

		m = m * 10;
		m += (INT)(pStart[i] - '0');
	}

	return m;
}

BOOLEAN MemIni_ReadInteger(PAD_MEM_INI Ini,LPCSTR strSectionName,LPCSTR strValueName,INT * nValue)
{
	PCHAR strValue = NULL;
	INT nSize= 0;

	if( nValue == NULL )
		return FALSE;

	if( MemIni_ReadString(Ini,strSectionName,strValueName,&strValue,&nSize) )
	{
		PCHAR Ptr;
		if( !MemIni_RemoveNotes(strValue,&Ptr,NULL) )
		{
			MemIni_MemFree(strValue);
			return FALSE;
		}

		*nValue = xtoi(Ptr);
		MemIni_MemFree(strValue);
		return TRUE;
	}

	return FALSE;
}

BOOLEAN MemIni_ReadBOOL(PAD_MEM_INI Ini,LPCSTR strSectionName,LPCSTR strValueName,BOOLEAN * bValue)
{
	INT nValue = 0;

	if( bValue == NULL )
		return FALSE;

	if( MemIni_ReadInteger(Ini,strSectionName,strValueName,&nValue) )
	{
		*bValue = (nValue == 1) ? TRUE : FALSE;
		return TRUE;
	}

	return FALSE;
}

BOOLEAN MemIni_LoadDataFromFile(PAD_MEM_INI Ini,LPCWSTR lpFile)
{
	HANDLE				hFile;
	NTSTATUS			status;
	OBJECT_ATTRIBUTES	Oa;
	UNICODE_STRING		ufile_name;
	PCHAR				lpBuf = NULL;
	INT					nMax = 0;
	int					nLen = 0;
	IO_STATUS_BLOCK		io_status = {0};
	LARGE_INTEGER		offset = {0};
	FILE_STANDARD_INFORMATION file_standard_info = { 0 };

	if( Ini == NULL || lpFile == NULL )
		return FALSE;

	RtlInitUnicodeString(&ufile_name,(PCWSTR)lpFile);
	InitializeObjectAttributes(&Oa,&ufile_name,OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,NULL,NULL);

	status = ZwCreateFile(&hFile,GENERIC_READ|GENERIC_WRITE,&Oa,&io_status,NULL,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ,
		FILE_OPEN,FILE_NON_DIRECTORY_FILE|FILE_RANDOM_ACCESS|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);

	if( !NT_SUCCESS(status) )
	{
		return FALSE;
	}

	status = ZwQueryInformationFile(
		hFile,
		&io_status,
		&file_standard_info,
		sizeof(file_standard_info),
		FileStandardInformation);

	if( !NT_SUCCESS(status) )
	{
		ZwClose(hFile);
		return FALSE;
	}

	if ( file_standard_info.EndOfFile.QuadPart > ( (2i64 << 20) - 2 ) )
	{
		ZwClose(hFile);
		return FALSE;
	}

	nMax = (INT)file_standard_info.EndOfFile.QuadPart;
	lpBuf = (PCHAR)KmAlloc(nMax+2);	
	if( !lpBuf )
	{
		ZwClose(hFile);
		return FALSE;
	}

	nLen = 0;
	while(nLen<nMax)
	{
		io_status.Information = 0;
		status = ZwReadFile(hFile,NULL,NULL,NULL,&io_status,lpBuf+nLen,nMax-nLen,&offset,NULL);
		if( !NT_SUCCESS(status) )
		{
			ZwClose(hFile);
			ExFreePool(lpBuf);
			return FALSE;
		}

		nLen += (int)io_status.Information;
		offset.QuadPart = nLen;
	}

	ZwClose(hFile);
	if( nLen != nMax )
	{
		ExFreePool(lpBuf);
		return FALSE;
	}

	lpBuf[nLen] = '\0';
	lpBuf[nLen+1] = '\0';

	MemIni_Initialize(Ini,lpBuf);
	KmFree(lpBuf);

	return TRUE;
}

PVOID MemIni_MemAlloc(INT nSize)
{
	return KmAlloc(nSize);
}
