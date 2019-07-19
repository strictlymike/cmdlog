/* Detours DLL for hooking and logging console I/O for 32- and 64-bit cmd.exe
 * and powershell.exe */

#include <stdio.h>
#include <windows.h>
#include <stdarg.h>
#include "detours.h"

#include "logging.h"
#include "cmdlog_defs.h"

#pragma comment(lib, "iphlpapi.lib")

#define NUM_DLL_NAMES           1

///////////////////////////////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////////////////////////////

enum CmdLogLabelState {
	cllsLabel,
	cllsNoLabel,
};

enum CmdLogRWConOutState {
    cl_rwco_neutral,
    cl_rwco_entering,
    cl_rwco_entered,
};

///////////////////////////////////////////////////////////////////////////////
// Prototypes
///////////////////////////////////////////////////////////////////////////////

void RWConOutInit();
void RWConOutCopyEntry(CHAR_INFO *lpBuffer, COORD dwBufferSize);
PWCHAR RWConOutGetLast();
void RWConOutClearBuf();

BOOL __stdcall HookCreateProcessW(LPCWSTR lpApplicationName,
	LPWSTR lpCommandLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCWSTR lpCurrentDirectory,
	LPSTARTUPINFOW lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation
   );
BOOL __stdcall HookCreateProcessA(LPCSTR lpApplicationName,
	LPSTR lpCommandLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCSTR lpCurrentDirectory,
	LPSTARTUPINFO lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation
   );
BOOL WINAPI LogReadConsoleW(
	HANDLE hConsoleInput,
	PWCHAR lpBuffer,
	DWORD nNumberOfCharsToRead,
	LPDWORD lpNumberOfCharsRead,
	PCONSOLE_READCONSOLE_CONTROL pInputControl
);
BOOL WINAPI LogWriteConsoleW(
	HANDLE hConsoleOutput,
	const VOID *lpBuffer,
	DWORD nNumberOfCharsToWrite,
	LPDWORD lpNumberOfCharsWritten,
	LPVOID lpReserved
);
BOOL WINAPI LogReadConsoleInputW(
    HANDLE hConsoleInput,
    PINPUT_RECORD lpBuffer,
    DWORD nLength,
    LPDWORD lpNumberOfEventsRead
   );
BOOL WINAPI LogWriteConsoleOutputW(
    HANDLE hConsoleOutput,
    CHAR_INFO* lpBuffer,
    COORD dwBufferSize,
    COORD dwBufferCoord,
    PSMALL_RECT lpWriteRegion
   );

///////////////////////////////////////////////////////////////////////////////
// Data
///////////////////////////////////////////////////////////////////////////////

BOOL CfgLogCreatedProcesses = FALSE;

/* Read/write console output state and buffer control */
struct {
    CmdLogRWConOutState State;
    COORD wbuf_size;
    WCHAR wbuf[10000 * 512];
} g_conout;

enum CmdLogLabelState LabelState = cllsLabel;
static CHAR dllname[MAX_PATH];
static CHAR exename[MAX_PATH];

LPCSTR dllnamesA[NUM_DLL_NAMES] = {
    dllname,
};

BOOL (__stdcall * pCreateProcessA)(
	LPCSTR a0,
	LPSTR a1,
	LPSECURITY_ATTRIBUTES a2,
	LPSECURITY_ATTRIBUTES a3,
	BOOL a4,
	DWORD a5,
	LPVOID a6,
	LPCSTR a7,
	LPSTARTUPINFOA a8,
	LPPROCESS_INFORMATION a9
) = CreateProcessA;

BOOL (__stdcall * pCreateProcessW)(
	LPCWSTR a0,
	LPWSTR a1,
	LPSECURITY_ATTRIBUTES a2,
	LPSECURITY_ATTRIBUTES a3,
	BOOL a4,
	DWORD a5,
	LPVOID a6,
	LPCWSTR a7,
	LPSTARTUPINFOW a8,
	LPPROCESS_INFORMATION a9
) = CreateProcessW;

static BOOL (WINAPI *pReadConsoleW)(
	HANDLE hConsoleInput,
	LPVOID lpBuffer,
	DWORD nNumberOfCharsToRead,
	LPDWORD lpNumberOfCharsRead,
	PCONSOLE_READCONSOLE_CONTROL pInputControl
) = ReadConsoleW;

static BOOL (WINAPI *pWriteConsoleW)(
	HANDLE hConsoleOutput,
	const VOID *lpBuffer,
	DWORD nNumberOfCharsToWrite,
	LPDWORD lpNumberOfCharsWritten,
	LPVOID lpReserved
) = WriteConsoleW;

static BOOL (WINAPI * pReadConsoleInputW)(
    HANDLE a0,
    PINPUT_RECORD a1,
    DWORD a2,
    LPDWORD a3) = ReadConsoleInputW;

static BOOL (WINAPI * pWriteConsoleOutputW)(HANDLE a0,
    CONST CHAR_INFO* a1,
    COORD a2,
    COORD a3,
    PSMALL_RECT a4) = WriteConsoleOutputW;

///////////////////////////////////////////////////////////////////////////////
// Definitions
///////////////////////////////////////////////////////////////////////////////

/* Adapted from Detours sample traceapi.cpp */
BOOL __stdcall HookCreateProcessA(LPCSTR lpApplicationName,
	LPSTR lpCommandLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCSTR lpCurrentDirectory,
	LPSTARTUPINFO lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation
   )
{

    if (CfgLogCreatedProcesses) {
        LogOutput(NULL, "CreateProcessA(%hs,%hs,%p,%p,%p,%p,%p,%hs,%p,%p)\r\n",
            lpApplicationName,
            lpCommandLine,
            lpProcessAttributes,
            lpThreadAttributes,
            bInheritHandles,
            dwCreationFlags,
            lpEnvironment,
            lpCurrentDirectory,
            lpStartupInfo,
            lpProcessInformation
           );
        LogOutput(
            NULL,
            BAR
           );
    }

	PROCESS_INFORMATION procInfo;
	if (lpProcessInformation == NULL) {
		lpProcessInformation= &procInfo;
		ZeroMemory(&procInfo, sizeof(procInfo));
	}

	BOOL rv = 0;
	__try {
		rv = DetourCreateProcessWithDllsA(lpApplicationName,
			lpCommandLine,
			lpProcessAttributes,
			lpThreadAttributes,
			bInheritHandles,
			dwCreationFlags,
			lpEnvironment,
			lpCurrentDirectory,
			lpStartupInfo,
			lpProcessInformation,
            NUM_DLL_NAMES,
			dllnamesA,
			pCreateProcessA
		   );
	} __finally { };
	return rv;
}

/* Adapted from Detours sample traceapi.cpp */
BOOL
__stdcall
HookCreateProcessW(LPCWSTR lpApplicationName,
	LPWSTR lpCommandLine,
	LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles,
	DWORD dwCreationFlags,
	LPVOID lpEnvironment,
	LPCWSTR lpCurrentDirectory,
	LPSTARTUPINFOW lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation
   )
{
    if (CfgLogCreatedProcesses) {
        LogOutput(NULL, "CreateProcessW(%ls,%ls,%p,%p,%p,%p,%p,%ls,%p,%p)\r\n",
            lpApplicationName,
            lpCommandLine,
            lpProcessAttributes,
            lpThreadAttributes,
            bInheritHandles,
            dwCreationFlags,
            lpEnvironment,
            lpCurrentDirectory,
            lpStartupInfo,
            lpProcessInformation
           );
        LogOutput(
            NULL,
            "%s",
            BAR
           );
    }

	PROCESS_INFORMATION procInfo;
	if (lpProcessInformation == NULL) {
		lpProcessInformation= &procInfo;
		ZeroMemory(&procInfo, sizeof(procInfo));
	}

	BOOL rv = 0;
	__try {
		rv = DetourCreateProcessWithDllsW(lpApplicationName,
			lpCommandLine,
			lpProcessAttributes,
			lpThreadAttributes,
			bInheritHandles,
			dwCreationFlags,
			lpEnvironment,
			lpCurrentDirectory,
			lpStartupInfo,
			lpProcessInformation,
            NUM_DLL_NAMES,
			dllnamesA,
			pCreateProcessW
		   );
	} __finally { };
	return rv;
}

BOOL WINAPI
LogReadConsoleW(
	HANDLE hConsoleInput,
	PWCHAR lpBuffer,
	DWORD nNumberOfCharsToRead,
	LPDWORD lpNumberOfCharsRead,
	PCONSOLE_READCONSOLE_CONTROL pInputControl
)
{
	BOOL ret = pReadConsoleW(
		hConsoleInput,
		lpBuffer,
		nNumberOfCharsToRead,
		lpNumberOfCharsRead,
		pInputControl
	   );

    /* NULL-terminate the input buffer as a convenience for myself, provided it
     * doesn't exceed caller-specified bounds */
    if (*lpNumberOfCharsRead <= nNumberOfCharsToRead) {
        lpBuffer[*lpNumberOfCharsRead] = '\0';
    }

	/* TODO: Replace this crap code with more accurate parsing */
	if (!wcsncmp((const wchar_t *)lpBuffer, CMD_QUERY_STATUS_W,
				wcslen(CMD_QUERY_STATUS_W)))
	{
        QueryStatus();
	}

	LogOutput(NULL, "%S", lpBuffer);
	LabelState = cllsLabel;

    LogInfoStampToFile(NULL);

	return ret;
}

BOOL WINAPI
LogWriteConsoleW(
	HANDLE  hConsoleOutput,
	const VOID *lpBuffer,
	DWORD nNumberOfCharsToWrite,
	LPDWORD lpNumberOfCharsWritten,
	LPVOID lpReserved
)
{
	if (cllsLabel == LabelState) {
        // Built to precede every series of output lines with an info stamp,
        // but it was retired in favor of post-ReadConsoleW info stamp. Leaving
        // the state maintenance intact in case the desire arises to use it
        // again.
		LabelState = cllsNoLabel;
	}
	LogOutput(NULL, "%S", lpBuffer);

	return pWriteConsoleW(
		hConsoleOutput,
		lpBuffer,
		nNumberOfCharsToWrite,
		lpNumberOfCharsWritten,
		lpReserved
	   );
}

BOOL
WINAPI
LogReadConsoleInputW(
    HANDLE hConsoleInput,
    PINPUT_RECORD lpBuffer,
    DWORD nLength,
    LPDWORD lpNumberOfEventsRead
   )
{
    BOOL ret = 0;
    PWCHAR entry = NULL;

    ret = pReadConsoleInputW(
        hConsoleInput,
        lpBuffer,
        nLength,
        lpNumberOfEventsRead
       );

    /* PowerShell specifically uses buffers of nLength == 1 */
    if ((nLength == 1) && lpNumberOfEventsRead && (1 == *lpNumberOfEventsRead))
    {
        if (lpBuffer[0].EventType == KEY_EVENT) {
            if (VK_RETURN == lpBuffer[0].Event.KeyEvent.wVirtualKeyCode) {
                entry = RWConOutGetLast();
                LogOutput(NULL, "%S\n", entry);
                RWConOutClearBuf();

                LogInfoStampToFile(NULL);
            }
        }
    }

    return ret;
}

BOOL
WINAPI
LogWriteConsoleOutputW(
    HANDLE hConsoleOutput,
    CHAR_INFO* lpBuffer,
    COORD dwBufferSize,
    COORD dwBufferCoord,
    PSMALL_RECT lpWriteRegion
   )
{
    BOOL ret = 0;

    ret = pWriteConsoleOutputW(
        hConsoleOutput,
        lpBuffer,
        dwBufferSize,
        dwBufferCoord,
        lpWriteRegion
       );

    RWConOutCopyEntry(lpBuffer, dwBufferSize);

    return ret;
}

void
RWConOutInit()
{
    memset(&g_conout, 0, sizeof(g_conout));
    g_conout.State = cl_rwco_neutral;
}

void
RWConOutCopyEntry(CHAR_INFO *lpBuffer, COORD dwBufferSize)
{
    short x = 0;
    short y = 0;
    WCHAR c = L'\0';

    if ((dwBufferSize.X > 0) && (dwBufferSize.Y > 0)) {
        g_conout.State = cl_rwco_entering;
        g_conout.wbuf_size = dwBufferSize;
        RWConOutClearBuf();

        for (y=0; y<dwBufferSize.Y; y++) {
            for (x=0; x<dwBufferSize.X; x++) {
                c = lpBuffer[y * dwBufferSize.X + x].Char.UnicodeChar;
                g_conout.wbuf[y * dwBufferSize.X + x] = c;
            }
        }
    }
}

PWCHAR
RWConOutGetLast()
{
    g_conout.State = cl_rwco_entered;
    return g_conout.wbuf;
}

void
RWConOutClearBuf()
{
    memset(&g_conout.wbuf, 0, sizeof(g_conout.wbuf));
}

///////////////////////////////////////////////////////////////////////////////
// Entry point
///////////////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
	LONG error = NO_ERROR;
    DWORD Ret = 0;

	UNREFERENCED_PARAMETER(reserved);

    // See documentation for DetourCreateProcessWithDlls
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

	if (dwReason == DLL_PROCESS_ATTACH) {
		DetourRestoreAfterWith();

        LogInit();
        RWConOutInit();

        Ret = GetModuleFileNameA(hinst, dllname, ARRAYSIZE(dllname));
        if (Ret == 0) {
            error = GetLastError();
            LogOutput(
                NULL,
                "GetModuleFileNameA failed, %d\r\n",
                error
               );
        } else {
			Ret = GetModuleFileNameA(NULL, exename, ARRAYSIZE(exename));
			if (Ret == 0) {
                error = GetLastError();
				LogOutput(
					NULL,
					"GetModuleFileNameA failed, %d\r\n",
					error
				   );
			}
		}

		if (error == NO_ERROR) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)pReadConsoleW, LogReadConsoleW);
            DetourAttach(&(PVOID&)pWriteConsoleW, LogWriteConsoleW);
            DetourAttach(&(PVOID&)pReadConsoleInputW, LogReadConsoleInputW);
            DetourAttach(&(PVOID&)pWriteConsoleOutputW, LogWriteConsoleOutputW);
            DetourAttach(&(PVOID&)pCreateProcessA, HookCreateProcessA);
            DetourAttach(&(PVOID&)pCreateProcessW, HookCreateProcessW);
            error = DetourTransactionCommit();
        }

		if (error != NO_ERROR) {
			LogOutput(
				NULL,
				"Error detouring {Read,Write}ConsoleW(): %d\r\n",
				error
			   );
            return FALSE;
		}
	}
	else if (dwReason == DLL_PROCESS_DETACH) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)pReadConsoleW, LogReadConsoleW);
		DetourDetach(&(PVOID&)pWriteConsoleW, LogWriteConsoleW);
		DetourDetach(&(PVOID&)pReadConsoleInputW, LogReadConsoleInputW);
		DetourDetach(&(PVOID&)pWriteConsoleOutputW, LogWriteConsoleOutputW);
		error = DetourTransactionCommit();
	}
	return TRUE;
}
