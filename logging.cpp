#define _CRT_SECURE_NO_WARNINGS /* Technical debt? I think so. */
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <share.h>

#include <map>
#include <string>
#include <iterator>

#include "logging.h"

#include "cmdlog_defs.h"

BOOL _LogInitAsPrimary();
void _LogFmtTimestampFilename(char *fmt, char *buf, size_t buflen);
BOOL _LogNormalizePath(char *fname, DWORD len_fname, char *tmp, DWORD len_tmp);
BOOL _LogGetPathFromEnv(char path[MAX_PATH]);
BOOL _LogSetPathToEnv(char path[MAX_PATH]);
void _FmtAndLogToFile(FILE *out, const char *fmt, va_list argp);
void _LogToFile(FILE *out, const char *data);

/* Plumbing for client and server sides of named pipe-based logging
 * communication that was thought to be necessary due to anticipated race
 * conditions. Instead of a file path, an environment variable would be used
 * to define a pseudo-random name of the pipe created by the first DLL instance
 * to load (by noting the absence of the variable definition in the
 * environment). Race conditions have yet to manifest, so this design appears
 * to be merely a vestige of an unwarranted concern. It can be further
 * developed if race conditions do arise during testing. Currently only the
 * "output" buffer (client side) is used, and it is only 
 */

CRITICAL_SECTION cs_output;
char cmdlog_fname[MAX_PATH];

char buf_output[BUFSIZE_OUTPUT];
#if 0
char buf_input[BUFSIZE_OUTPUT];
#endif

std::map<std::string, std::string> last_adapters;

BOOL
LogInit()
{
    BOOL Ok = FALSE;

    InitializeCriticalSection(&cs_output);

    Ok = _LogGetPathFromEnv(cmdlog_fname);

    if (!Ok) {
        Ok = _LogInitAsPrimary();
    }

    return Ok;
}

/* Useful: http://c-faq.com/varargs/handoff.html */
void
LogOutput(FILE *out, const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    _FmtAndLogToFile(out, fmt, argp);
    va_end(argp);
}

BOOL
_AdaptersChanged(PIP_ADAPTER_INFO pAdapterList)
{
    PIP_ADAPTER_INFO pAdapter = NULL;

    std::map<std::string, std::string> current_adapters;

	for (pAdapter = pAdapterList; pAdapter; pAdapter = pAdapter->Next) {
        std::string ipinfo;

		switch (pAdapter->Type)
		{
			case IF_TYPE_IEEE80211:
                ipinfo = std::string("Wireless: ");
				break;
			case MIB_IF_TYPE_OTHER:
			case MIB_IF_TYPE_ETHERNET:
                ipinfo = std::string("Ethernet: ");
				break;
		}

        if (ipinfo.size()) {
            ipinfo += pAdapter->IpAddressList.IpAddress.String;
            ipinfo += " / ";
            ipinfo += pAdapter->IpAddressList.IpMask.String;
            ipinfo += " gw ";
            ipinfo += pAdapter->GatewayList.IpAddress.String;

            current_adapters.insert(
                std::make_pair(pAdapter->AdapterName, ipinfo));
        }
    }

    if (current_adapters != last_adapters) {
        last_adapters = current_adapters;
        return TRUE;
    }

    return FALSE;
}

void
LogAdapterInfoToFile(FILE *out)
{
	DWORD Ret;
	ULONG Buflen = 0;
	PIP_ADAPTER_INFO pAdapterList = NULL;

	Ret = GetAdaptersInfo(NULL, &Buflen);
	if (Ret != ERROR_BUFFER_OVERFLOW) { goto exit_LogAdapterInfo; }

	pAdapterList = (PIP_ADAPTER_INFO)malloc(Buflen);
	if (pAdapterList == NULL) { goto exit_LogAdapterInfo; }

	Ret = GetAdaptersInfo(pAdapterList, &Buflen);
	if (Ret != NO_ERROR) { goto exit_LogAdapterInfo; }

    if (!_AdaptersChanged(pAdapterList)) {
        goto exit_LogAdapterInfo;
    }

    for (auto it = last_adapters.begin(); it != last_adapters.end(); it++) {
        LogOutput(out, "%s\r\n", it->second.c_str());
	}

exit_LogAdapterInfo:
	if (pAdapterList) { free(pAdapterList); }
}

void
LogTimeStampToFile(FILE *out)
{
	SYSTEMTIME tm;
	TIME_ZONE_INFORMATION tzi;
	DWORD tzd;

	GetLocalTime(&tm);
	tzd = GetTimeZoneInformation(&tzi);

	LogOutput(
		out,
		"Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%02d %S\r\n",
		tm.wYear,
		tm.wMonth,
		tm.wDay,
		tm.wHour,
		tm.wMinute,
		tm.wSecond,
		tm.wMilliseconds,
		(tzd == TIME_ZONE_ID_STANDARD)? tzi.StandardName: tzi.DaylightName
	   );
}

void
LogInfoStampToFile(FILE *out)
{
	LogOutput(out, "\r\n%s", BAR);
#if 0
	LogOutput(out, "Executable module name: %s\r\n", exename);
#ifdef _WIN64
	LogOutput(out, "Architecture width: 64-bit\r\n");
#else
	LogOutput(out, "Architecture width: 32-bit\r\n");
#endif
#endif
	LogTimeStampToFile(out);
	LogAdapterInfoToFile(out);
	LogOutput(out, "%s", BAR);
}

void
QueryStatus()
{
    printf("Logging to %s\n", cmdlog_fname);
    LogInfoStampToFile(stdout);
}

BOOL
_LogInitAsPrimary()
{
    DWORD n = 0;
    BOOL Ok = FALSE;
    BOOL Ret = FALSE;
    FILE *test = NULL;
    char tmp[MAX_PATH];

    memset(tmp, 0, STATIC_BUFFER_LEN(tmp));

    /* Fill in timestamp format of log file name. */
    _LogFmtTimestampFilename(CMDLOG_FNAME_FMT, tmp, STATIC_BUFFER_LEN(tmp));

    /* Expand environment variables if applicable */
    n = ExpandEnvironmentStringsA(tmp, cmdlog_fname, MAX_PATH);
    Ok = (n && (n < MAX_PATH));
    if (!Ok) {
        fprintf( stderr,
            "Failed to expand environment strings, logging disabled (a)\n"
           );
        goto exit__InitLoggingPrimary;
    }

    /* Normalize relative paths to absolute ones */
    Ok = _LogNormalizePath(
        cmdlog_fname,
        STATIC_BUFFER_LEN(cmdlog_fname),
        tmp,
        STATIC_BUFFER_LEN(tmp)
       );
    if (!Ok) {
        fprintf(stderr, "Failed to normalize path, logging disabled (b)\n");
        goto exit__InitLoggingPrimary;
    }

    /* Test access */
    test = _fsopen(cmdlog_fname, "ab", _SH_DENYWR);
    if (!test) {
        fprintf(
            stderr,
            "fopen_s(%s, \"ab\") failed, logging disabled (c)\n",
            cmdlog_fname
           );
        goto exit__InitLoggingPrimary;
    }

    fclose(test);

    printf("Logging to %s\n", cmdlog_fname);
    _LogSetPathToEnv(cmdlog_fname);

    Ret = TRUE;

exit__InitLoggingPrimary:
    return Ret;
}

/* Expand format string similar to strftime */
void
_LogFmtTimestampFilename(char *fmt, char *buf, size_t buflen)
{
    SYSTEMTIME tm = {0};
    TIME_ZONE_INFORMATION tzi = {0};
    char tmp[MAX_PATH];

    memset(tmp, 0, STATIC_BUFFER_LEN(tmp));

    GetLocalTime(&tm);
    GetTimeZoneInformation(&tzi);

    /* My recollection is that strftime can't do correct time zones without
     * serious rigamarole. */
    _snprintf(
        buf,
        buflen,
        fmt,
        tm.wYear,
        tm.wMonth,
        tm.wDay,
        tm.wHour,
        tm.wMinute,
        tm.wSecond,
        tm.wMilliseconds,
        ((float)tzi.Bias)/60
    );
}

/* Normalize to an absolute path to ensure the log file doesn't split across
 * directories when the working directory changes. Unsure if GetFullPathNameA
 * handles lpFileName == lpBuffer, hence the temporary copy. */
BOOL
_LogNormalizePath(char *fname, DWORD len_fname, char *tmp, DWORD len_tmp)
{
    DWORD Ret = 0;
    BOOL Ok = FALSE;

    strncpy(tmp, cmdlog_fname, len_tmp);

    Ret = GetFullPathNameA(
        tmp,
        len_fname,
        fname,
        NULL
       );
    Ok = (Ret && (Ret < STATIC_BUFFER_LEN(cmdlog_fname)));

    return Ok;
}

BOOL
_LogGetPathFromEnv(char path[MAX_PATH])
{
    DWORD n = 0;
    BOOL Ok = FALSE;

    n = GetEnvironmentVariableA(ENVVAR_CMDLOG_LOG_PATH, path, MAX_PATH);
    Ok = n && (n < MAX_PATH);
    return Ok;
}

BOOL
_LogSetPathToEnv(char path[MAX_PATH])
{
    return SetEnvironmentVariableA(ENVVAR_CMDLOG_LOG_PATH, path);
}

void
_LogToFile(FILE *out, const char *data)
{
    int do_fclose = 0;

    if (!out) {
        do_fclose = 1;
        out = _fsopen(cmdlog_fname, "ab", _SH_DENYWR);
    }

    if (!out) {
        fprintf(
            stderr,
            "fopen_s(%s, \"ab\") failed, logging disabled (d)\n",
            cmdlog_fname
           );
    } else {
        fprintf(out, "%s", data);
        fflush(out);
        if (do_fclose) {
            fclose(out);
        }
    }
}

void
_FmtAndLogToFile(FILE *out, const char *fmt, va_list argp)
{
    EnterCriticalSection(&cs_output);
    memset(buf_output, 0, BUFSIZE_OUTPUT);
    vsnprintf(buf_output, BUFSIZE_OUTPUT, fmt, argp);
    buf_output[BUFSIZE_OUTPUT-1] = '\0';

    _LogToFile(out, buf_output);

    LeaveCriticalSection(&cs_output);
}
