#ifndef memini_h__
#define memini_h__

typedef struct _AD_MEM_INI
{
	LIST_ENTRY			leSections;
	PVOID				pCurSection;
}AD_MEM_INI,*PAD_MEM_INI;

BOOLEAN MemIni_LoadDataFromFile(PAD_MEM_INI Ini,LPCWSTR lpFile);
BOOLEAN	MemIni_GetSection(PAD_MEM_INI Ini,PCHAR * lpTmp,INT * nSize);
BOOLEAN	MemIni_SectionExists(PAD_MEM_INI Ini,LPCSTR strSectionName);
BOOLEAN	MemIni_ReadString(PAD_MEM_INI Ini,LPCSTR strSectionName,LPCSTR strValueName,PCHAR * strValue,INT * nStrLen);
BOOLEAN	MemIni_ReadInteger(PAD_MEM_INI Ini,LPCSTR strSectionName,LPCSTR strValueName,INT * nValue);
BOOLEAN	MemIni_ReadBOOL(PAD_MEM_INI Ini,LPCSTR strSectionName,LPCSTR strValueName,BOOLEAN * bValue);
BOOLEAN	MemIni_IsValid(PAD_MEM_INI Ini);

VOID	MemIni_Initialize(PAD_MEM_INI Ini,CHAR* szMem);
VOID	MemIni_Uninitialize(PAD_MEM_INI Ini);

VOID	MemIni_MemFree(PVOID pMem);
PVOID	MemIni_MemAlloc(INT nSize);

void	DeCodeData(unsigned char* pFileMem,ULONG uSize);

#endif // memini_h__
