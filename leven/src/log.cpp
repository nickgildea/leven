#include <stdio.h>
#include <stdarg.h>

#include "log.h"

namespace
{
    FILE* g_file = NULL;
}

bool LogInit()
{
    FILE* f = fopen("log.txt", "w");
    if (f == NULL)
    {
        return false;
    }

    g_file = f;
    return true;
}

void LogShutdown()
{
    if (g_file)
    {
        fclose(g_file);
        g_file = NULL;
    }
}

void LogPrintf(const char* fmt, ...)
{
    if (g_file == NULL)
    {
        return;
    }

    static char buffer[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(buffer, fmt, argptr);
    va_end(argptr);

    fprintf(g_file, buffer);
    fflush(g_file);
}

void LogDebugf(const char* fmt, ...)
{
#ifdef _DEBUG
    if (g_file == NULL)
    {
        return;
    }

    static char buffer[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(buffer, fmt, argptr);
    va_end(argptr);

    fprintf(g_file, buffer);
    fflush(g_file);
#endif
}

