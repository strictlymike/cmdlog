#pragma once

#define BAR "------------------------------------------------------------\r\n"
#define ENVVAR_CMDLOG_LOG_PATH  "CMDLOG_LOG_PATH"
#define CMD_QUERY_STATUS_W	    L"rem status"

#if defined(LOG_TO_USERPROFILE_PATH) && LOG_TO_USERPROFILE_PATH
#define CMDLOG_FNAME_FMT \
    "%%USERPROFILE%%\\cmdlog-%04d.%02d.%02dT%02d.%02d.%02d.%02d-UTC-%1.1f.log"
#else
#define CMDLOG_FNAME_FMT \
    "cmdlog-%04d.%02d.%02dT%02d.%02d.%02d.%02d-UTC-%1.1f.log"
#endif

#define BUFSIZE_OUTPUT          8 * 4096
#define BUFSIZE_INPUT           8 * 4096

#define STATIC_BUFFER_LEN(__b)  (sizeof(__b) / sizeof(__b[0]))
