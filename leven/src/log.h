#ifndef     __CLYDE_LOG_H___
#define     __CLYDE_LOG_H___

bool		LogInit();
void		LogShutdown();
void		LogPrintf(const char* fmt, ...);
void		LogDebugf(const char* fmt, ...);

#endif

