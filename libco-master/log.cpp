#include "log.h"

void info(const char*file, const int line, const char*fmt,...){
#ifdef LOG
	char buf[1024]={'\0'};
	va_list ap;
	strncpy(buf, file, sizeof(buf));
	printf("%s:%d  ",basename(buf), line);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
#endif
}
