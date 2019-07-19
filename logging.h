#pragma once

#include <windows.h>
#include <stdio.h>

BOOL LogInit();
void LogOutput(FILE *out, const char *fmt, ...);
void LogAdapterInfoToFile(FILE *out);
void LogInfoStampToFile(FILE *out);
void LogTimeStampToFile(FILE *out);

void QueryStatus();
