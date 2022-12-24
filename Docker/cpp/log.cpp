/* log.cpp: log handler
 */

#include <iostream>
#include <stdarg.h>
#include "log.hpp"
#include "variables_ext.hpp"

using namespace std;
bool SetLogLevel(int log_level){
	if(LOG_ERROR<=log_level && log_level<=LOG_DEBUG){
		LOG_LEVEL=log_level;
		Log(LOG_INFO, "Set log_level to %d", LOG_LEVEL);
		return true;
	}else{
	  Log(LOG_ERROR, "log_level %d is invalid", log_level);
		return false;
	}
}

void Log(int level, const char* format, ...){
	va_list args;
	va_start(args, format);

	if(level<=LOG_LEVEL){
		switch(level){
		case LOG_ERROR:
			printf("[Error] ");
			break;
		case LOG_WARN:
			printf("[Warn]  ");
			break;
		case LOG_INFO:
			printf("[Info]  ");
			break;
		case LOG_DEBUG:
			printf("[Debug] ");
			break;
		}
		vprintf(format, args);
		printf("\n");
	}
	va_end(args);
}
