// Exit with a printf-formatted message.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
void die (char *format, ...) __attribute__ ((noreturn));
void die(char *format, ...)
{
	va_list args;
	va_start(args, format);
	
	vfprintf(stderr, format, args);
	
	va_end(args);
	exit(EXIT_FAILURE);
}
