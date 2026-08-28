#include "log.h"

#ifdef CONFIG_DEBUG

#include <stdio.h>
#include <time.h>

void log_write(const gchar *level, const gchar *fmt, ...)
{
	va_list args;
	struct timespec ts;
	struct tm tm;
	char stamp[16];
	gchar *msg;

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);
	if (strftime(stamp, sizeof(stamp), "%H:%M:%S", &tm) == 0)
		stamp[0] = '\0';

	va_start(args, fmt);
	msg = g_strdup_vprintf(fmt, args);
	va_end(args);

	fprintf(stderr, "[%s.%03ld] [%s] %s\n", stamp, ts.tv_nsec / 1000000L,
		level, msg);
	g_free(msg);
}

#endif
