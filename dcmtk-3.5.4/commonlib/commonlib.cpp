#include "stdafx.h"
#include "commonlib.h"
#define PATH_SEPARATOR '\\'

using namespace std;

COMMONLIB_API void displayErrorToCerr(TCHAR *lpszFunction)
{
	TCHAR *lpMsgBuf;
	TCHAR *lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
	// Display the error message
	lpDisplayBuf = (TCHAR *)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	sprintf_s(lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf); 
	//StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf); 
	cerr << lpDisplayBuf << endl;
	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}

COMMONLIB_API errno_t setEnvParentPID()
{
  char pidString[16];
  _itoa_s(_getpid(), pidString, 16, 10);
  return _putenv_s("PARENT_PID", pidString);
}

COMMONLIB_API bool IsASCII(const char *str)
{
  bool isAscii = true;
  if(str == NULL) return isAscii;
  size_t length = strlen(str);
  for(unsigned int i = 0; i < length; i++)
  {
    if(str[i] > '~' || str[i] < 0)
    {
      isAscii = false;
      break;
    }
  }
  return isAscii;
}

/* param:
  s:	  input string
  maxLen: max length of string s
 */
COMMONLIB_API char *rtrim(char *s, int maxLen)
{
  if(s == NULL) return 0;
  if(maxLen == -1)
	maxLen = strlen(s) - 1;
  else
	maxLen = min(maxLen, static_cast<int>(strlen(s))) - 1;
  int i;
  for(i = maxLen; i >= 0; --i)
  {
	if(s[i]==' ' || s[i]=='\n' || s[i] == '\r' || s[i]=='\t')
	  s[i]='\0';
	else
	  break;
  }
  // new size = i + 1
  return s;
}

static int signalInterruptFlag = 0;

COMMONLIB_API void SignalInterruptHandler(int signal)
{
  signalInterruptFlag = signal;
}

COMMONLIB_API void Capture_Ctrl_C()
{
  signalInterruptFlag = 0;
  signal(SIGINT, SignalInterruptHandler);
}

COMMONLIB_API int GetSignalInterruptValue()
{
  return signalInterruptFlag;
}

COMMONLIB_API LONGLONG GetFileInfo(const char *filePath, PSYSTEMTIME localTime)
{
  FILETIME fileTime;
  SYSTEMTIME utcTime;
  LARGE_INTEGER fileSize;

  HANDLE handle = ::CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if( handle != INVALID_HANDLE_VALUE) 
  {
    if(localTime != NULL && ::GetFileTime(handle, NULL, NULL, &fileTime))
    {
      FileTimeToSystemTime(&fileTime, &utcTime);
      SystemTimeToTzSpecificLocalTime(NULL, &utcTime, localTime);
    }
    ::GetFileSizeEx(handle, &fileSize);
    ::CloseHandle(handle);
	return fileSize.QuadPart;
  }
  else
	return -1LL;
}

COMMONLIB_API bool MkdirRecursive(const char *subdir)
{
  // check if the subdirectory is already existent
  if( _mkdir(subdir) )
  {
	if(errno == EEXIST)
	  return true;
  }
  else
  {
	return true;
  }

  string subdirectoryPath = subdir;
  size_t position = subdirectoryPath.rfind(PATH_SEPARATOR);
  if(position != string::npos)
  {
    string upperLevel = subdirectoryPath.substr(0, position);
	bool mkResult = MkdirRecursive(upperLevel.c_str());
    if(mkResult != true)
    {
      return mkResult;
    }
    // else: upper level exist, create current level
  }

  // if it is not existent create it
  if( _mkdir( subdirectoryPath.c_str() ) == -1 && errno != EEXIST)
  {
	cerr << "Could not create subdirectory " << subdirectoryPath.c_str() << endl;
    return false;
  }
  else
  {
    return true;
  }
}

COMMONLIB_API bool prepareFileDir(const char *path)
{
  string filePath = path;
  string::size_type p = filePath.rfind('\\');
  if(p == string::npos || p == 0) return true;
  return MkdirRecursive(filePath.substr(0, p).c_str());
}

//return 0 if successful, otherwise errno
COMMONLIB_API int GenerateLogPath(char *buf, size_t bufLen, const char *appName, const char pathSeparator)
{
  time_t now = time(NULL);
  struct tm calendar;
  errno_t err = localtime_s(&calendar, &now);
  if(!err)
  {
	int pid = _getpid();
	ostringstream format;
	format << "pacs_log" << pathSeparator << "%Y" << pathSeparator << "%m" << pathSeparator << "%d" << pathSeparator << "%H%M%S_" << appName << '_' << pid << ".txt" << std::ends;
	size_t pathLen = strftime(buf, bufLen, format.rdbuf()->str().c_str(), &calendar);
	if( ! pathLen ) err = EINVAL;
  }
  return err;
}

COMMONLIB_API BOOL DeleteEmptyFile(const char *filePath)
{
  LARGE_INTEGER fileSize;
  HANDLE handle = ::CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if( handle != INVALID_HANDLE_VALUE) 
  {
    ::GetFileSizeEx(handle, &fileSize);
    ::CloseHandle(handle);
	if(fileSize.QuadPart == 0LL)
	  return  ::DeleteFile(filePath);
	else
	  return TRUE;
  }
  else
	return FALSE;
}

COMMONLIB_API time_t dcmdate2tm(int dcmdate)
{
  struct tm timeBirth;
  timeBirth.tm_year = dcmdate / 10000 - 1900;
  timeBirth.tm_mon = dcmdate % 10000 / 100 - 1;
  timeBirth.tm_mday = dcmdate % 100;
  timeBirth.tm_hour = 0;
  timeBirth.tm_min = 0;
  timeBirth.tm_sec = 0;
  return mktime(&timeBirth);
}

COMMONLIB_API int changeWorkingDirectory(int argc, char **argv, char *pPacsBase)
{
	if(argc > 0)
	{
		char **endPos = argv + argc;
		char **pos = find_if(argv, endPos, not1(bind1st(ptr_fun(strcmp), "-wd")));
		if(pos == endPos) pos = find_if(argv, endPos, not1(bind1st(ptr_fun(strcmp), "--working-directory")));
		if(pos != endPos)
		{
			if(! _chdir(*++pos))
			{
				if(pPacsBase) strcpy_s(pPacsBase, MAX_PATH, *++pos);
				return -2;
			}
			else
				return 0;
		}
		// else get working dir from PACS_BASE in env
	}

	char* workingDirBuffer;
	size_t requiredSize;
	getenv_s( &requiredSize, NULL, 0, "PACS_BASE");
	if(requiredSize > 0)
	{
		char pacsdir[] = "\\pacs";
		workingDirBuffer = new char[requiredSize + sizeof(pacsdir)];
		getenv_s( &requiredSize, workingDirBuffer, requiredSize, "PACS_BASE");
		if(pPacsBase)
			strcpy_s(pPacsBase, MAX_PATH, workingDirBuffer);
		strcpy_s(workingDirBuffer + requiredSize - 1, sizeof(pacsdir), pacsdir);
		int chdirFail = _chdir(workingDirBuffer);
		delete workingDirBuffer;
		if(chdirFail) return -1;
	}
	else
	{
		if(!_chdir("C:\\usr\\local\\dicom\\pacs"))
			return -3;
		if(pPacsBase)
		{
			char base[] = "C:\\usr\\local\\dicom";
			strcpy_s(pPacsBase, sizeof(base), base);
		}
	}
	return 0;
}
