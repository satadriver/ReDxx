#ifndef Convert_h__
#define Convert_h__

#include <atlstr.h>

class CSystem
{
public:
	static int GetWCharToMByteSize(LPCWSTR lpString);
	static int GetMByteToWCharSize(LPCSTR lpString);
	static BOOL WCharToMByte(LPCWSTR lpWString, LPSTR lpString, DWORD dwSize);
	static BOOL MByteToWChar(LPCSTR lpString, LPWSTR lpWString, DWORD dwSize);

	static CStringA ConvertW2A(LPCWSTR lpString);
	static CStringA Convert2A(LPCTSTR lpString);
	static CStringW ConvertA2W(LPCSTR lpString);
	static CStringW Convert2W(LPCTSTR lpString);

	static CString ConvertA2(LPCSTR lpString);
	static CString ConvertW2(LPCWSTR lpString);

	static CString UnicodeToUTF8(LPCSTR lpString);
	static CString UnicodeToUTF8(LPCWSTR lpString);
	static CString UTF8ToUnicode(LPCWSTR lpString);
	static CString UTF8ToUnicode(LPCSTR lpString);

	static CString UrlEncode(LPCTSTR lpString);
	static CString UrlDecode(LPCTSTR lpString);
};
#endif // Convert_h__