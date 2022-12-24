/* log.hpp: log handler

# verbisity
- 0 (LOG_ERROR): only error(s) (the program stops at the error point)
- 1 (LOG_WARN) : error(s) and warnings (the program proceeds even if warnings happen)
- 2 (LOG_INFO) : error(s), warnings and infomation
- 3 (LOG_DEBUG): error(s), warnings, information, and debug logs
*/

#define LOG_INCLUDED

#define LOG_ERROR 0
#define LOG_WARN  1
#define LOG_INFO  2
#define LOG_DEBUG 3

bool SetLogLevel(int log_level);

void Log(int level, const char* format, ...);
