#include "stdafx.h"
#include <strsafe.h>

#include "commonlib.h"

using namespace std;

char SERVICE_NAME[128];

static SERVICE_STATUS          gSvcStatus; 
static SERVICE_STATUS_HANDLE   gSvcStatusHandle; 

char *getServiceName() { return SERVICE_NAME; }

void ReportSvcStatus( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
  static DWORD dwCheckPoint = 1;

  // Fill in the SERVICE_STATUS structure.
  gSvcStatus.dwCurrentState = dwCurrentState;
  gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
  gSvcStatus.dwWaitHint = dwWaitHint;

  if (dwCurrentState == SERVICE_START_PENDING)
	gSvcStatus.dwControlsAccepted = 0;
  else
	gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

  if ( (dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED) )
	gSvcStatus.dwCheckPoint = 0;
  else
	gSvcStatus.dwCheckPoint = dwCheckPoint++;

  // Report the status of the service to the SCM.
  SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

static void WINAPI SvcCtrlHandler( DWORD dwCtrl )
{
  // Handle the requested control code. 
  switch(dwCtrl)
  {
	case SERVICE_CONTROL_STOP:
	   ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
	   // Signal the service to stop.
	   SignalInterruptHandler(SIGINT);
	   return;
	case SERVICE_CONTROL_INTERROGATE:
	   // Fall through to send current status.
	   break;
	default:
	   break;
  } 
  ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
}

void SvcReportEvent(LPCTSTR szFunction, WORD eventType, DWORD eventId)
{ 
  HANDLE hEventSource;
  LPCTSTR lpszStrings[2];
  TCHAR Buffer[80];

  hEventSource = RegisterEventSource(NULL, SERVICE_NAME);

  if( NULL != hEventSource )
  {
	StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

	lpszStrings[0] = SERVICE_NAME;
	lpszStrings[1] = Buffer;

	ReportEvent(hEventSource,        // event log handle
				eventType,           // event type
				0,                   // event category
				eventId,             // event identifier
				NULL,                // no security identifier
				2,                   // size of lpszStrings array
				0,                   // no binary data
				lpszStrings,         // array of strings
				NULL);               // no binary data

	DeregisterEventSource(hEventSource);
  }
}

bool WINAPI SvcInit(DWORD dwWaitHint)
{
  gSvcStatusHandle = RegisterServiceCtrlHandler( SERVICE_NAME, SvcCtrlHandler);
  if( !gSvcStatusHandle )
  {
	  SvcReportEvent(TEXT("RegisterServiceCtrlHandler"), EVENTLOG_ERROR_TYPE, SVC_ERROR);
	  return false;
  }

  // These SERVICE_STATUS members remain as set here.
  gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  gSvcStatus.dwServiceSpecificExitCode = 0;

  // Report initial status to the SCM.
  ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, dwWaitHint);

  return true;
}
