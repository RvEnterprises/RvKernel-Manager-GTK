#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <glib.h>

G_BEGIN_DECLS

#ifdef CONFIG_DEBUG

void log_write(const gchar *level,
                  const gchar *fmt,
                  ...) G_GNUC_PRINTF(2, 3);

#define log_debug(...) log_write("debug", __VA_ARGS__)
#define log_info(...)  log_write("info",  __VA_ARGS__)
#define log_warn(...)  log_write("warn",  __VA_ARGS__)
#define log_error(...) log_write("error", __VA_ARGS__)

#else

#define log_debug(...) ((void)0)
#define log_info(...)  ((void)0)
#define log_warn(...)  ((void)0)
#define log_error(...) ((void)0)

#endif

G_END_DECLS

#endif
