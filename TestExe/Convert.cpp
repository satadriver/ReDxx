#include "stdafx.h"
#include "Convert.h"

CStringA CSystem::ConvertW2A(LPCWSTR lpString)
{
	CStringA str;
	if (lpString)
	{
		int nLen = GetWCharToMByteSize(lpString);
		LPSTR pData = str.GetBufferSetLength(nLen);
		WCharToMByte(lpString, pData, nLen);
		str.ReleaseBufferSetLength(nLen - 1);
	}
	return str;
}

CStringA CSystem::Convert2A(LPCTSTR lpString)
{
#if defined UNICODE || defined _UNICODE
	return ConvertW2A(lpString);
#else
	return CStringA(lpString);
#endif
}

CStringW CSystem::ConvertA2W(LPCSTR lpString)
{
	CStringW str;
	if (lpString)
	{
		int nLen = GetMByteToWCharSize(lpString);
		LPWSTR pData = str.GetBufferSetLength(nLen);
		MByteToWChar(lpString, pData, nLen);
		str.ReleaseBufferSetLength(nLen - 1);
	}
	return str;
}

CStringW CSystem::Convert2W(LPCTSTR lpString)
{
#if defined UNICODE || defined _UNICODE
	return CStringW(lpString);
#else
	return ConvertA2W(lpString);
#endif
}

CString CSystem::ConvertA2(LPCSTR lpString)
{
#if defined UNICODE || defined _UNICODE
	return ConvertA2W(lpString);
#else
	return CString(lpString);
#endif
}

CString CSystem::ConvertW2(LPCWSTR lpString)
{
#if defined UNICODE || defined _UNICODE
	return CString(lpString);
#else
	return ConvertW2A(lpString);
#endif
}

int CSystem::GetWCharToMByteSize(LPCWSTR lpString)
{
	return WideCharToMultiByte(CP_OEMCP,0,lpString, -1, NULL, 0, NULL, NULL);
}

int CSystem::GetMByteToWCharSize(LPCSTR lpString)
{
	return MultiByteToWideChar (CP_ACP, 0, lpString, -1, NULL, 0);
}

BOOL CSystem::WCharToMByte(LPCWSTR lpWString, LPSTR lpString, DWORD dwSize)
{
	int nRet = WideCharToMultiByte(CP_OEMCP, NULL, lpWString, -1, lpString, dwSize, NULL, NULL);
	return (nRet > 0) && (dwSize > 0);
}

BOOL CSystem::MByteToWChar(LPCSTR lpString, LPWSTR lpWString, DWORD dwSize)
{
	int nRet = MultiByteToWideChar (CP_ACP, 0, lpString, -1, lpWString, dwSize);  
	return (nRet > 0) && (dwSize > 0);
}


CString CSystem::UnicodeToUTF8(LPCSTR lpString)
{
	CStringW strString = ConvertA2W(lpString);
	CStringA strOut;
	int nLen = WideCharToMultiByte( CP_UTF8, 0, strString, -1, 0, 0, 0, 0 );
	LPSTR lpOut = strOut.GetBufferSetLength(nLen);
	::WideCharToMultiByte( CP_UTF8, 0, strString, -1, lpOut, nLen, 0, 0 );
	return ConvertA2(strOut);
}

CString CSystem::UnicodeToUTF8(LPCWSTR lpString)
{
	CStringA strOut;
	int nLen = WideCharToMultiByte( CP_UTF8, 0, lpString, -1, 0, 0, 0, 0 );
	LPSTR lpOut = strOut.GetBufferSetLength(nLen);
	::WideCharToMultiByte( CP_UTF8, 0, lpString, -1, lpOut, nLen, 0, 0 );
	return ConvertA2(strOut);
}

CString CSystem::UTF8ToUnicode(LPCWSTR lpString)
{
	CStringA strString = ConvertW2A(lpString);
	CStringW strOut;
	int nLen = MultiByteToWideChar( CP_UTF8, 0, strString, -1, NULL, 0 );
	LPWSTR lpOut = strOut.GetBufferSetLength(nLen);
	::MultiByteToWideChar( CP_UTF8, 0, strString, -1, lpOut, nLen);
	return ConvertW2(strOut);
}

CString CSystem::UTF8ToUnicode(LPCSTR lpString)
{
	CStringW strOut;
	int nLen = MultiByteToWideChar( CP_UTF8, 0, lpString, -1, NULL, 0 );
	LPWSTR lpOut = strOut.GetBufferSetLength(nLen);
	::MultiByteToWideChar( CP_UTF8, 0, lpString, -1, lpOut, nLen);
	return ConvertW2(strOut);
}

CString CSystem::UrlEncode(LPCTSTR lpString)
{
	char hex[] = "0123456789ABCDEF";

	CStringA strRet;
	CStringA strA = CSystem::Convert2A(lpString);
	for (int i = 0; i < strA.GetLength(); i++)
	{
		unsigned char ch = (unsigned char)strA.GetAt(i);
		if (isalnum(ch))
		{
			strRet += CStringA((char)ch);
		}
		else if (ch == ' ')
		{
			strRet += CStringA('+');
		}
		else
		{
			strRet += CStringA('%');
			strRet += hex[ch / 16];
			strRet += hex[ch % 16];
		}
	}
	return ConvertA2(strRet);
}

CString CSystem::UrlDecode(LPCTSTR lpString)
{
	CStringA strRet;
	CStringA strA = CSystem::Convert2A(lpString);

	for (int i = 0; i < strA.GetLength(); i++)
	{
		unsigned char ch = (unsigned char)strA.GetAt(i);
		if (ch == '%')
		{
			unsigned char ch1 = (unsigned char)strA.GetAt(i+1);
			unsigned char ch2 = (unsigned char)strA.GetAt(i+2);
			if(isxdigit(ch1) && isxdigit(ch2))
			{
				ch1 = ch1 - 48 - ((ch1 >= 'A') ? 7 : 0) - ((ch1 >= 'a') ? 32 : 0);
				ch2 = ch2 - 48 - ((ch2 >= 'A') ? 7 : 0) - ((ch2 >= 'a') ? 32 : 0);
				strRet += CString((char)(ch1 * 16 + ch2));
			}
		}
		else if (ch == '+')
		{
			strRet += CString(' ');
		}
		else
		{
			strRet += CString((char)ch);
		}
	}

	return ConvertA2(strRet);;
}