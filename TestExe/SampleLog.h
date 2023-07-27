#pragma once

#define  HANDLE_IS_VALID(x)	( (x) != NULL && (x) != INVALID_HANDLE_VALUE )
#define APPNAME "odrvin"
#define DBGINFO __FUNCTION__,__FILE__,__LINE__

int DbgInfo(const CHAR* func, const CHAR* file, int line, const char* fmt, ...);