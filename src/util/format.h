#ifndef UTIL_FORMAT_H
#define UTIL_FORMAT_H

#include <glib.h>

G_BEGIN_DECLS

gchar **tokenize_ws      (const gchar *text);

gchar  *format_khz       (gint64 khz);
gchar  *format_hz        (gint64 hz);
gchar  *format_bytes     (guint64 bytes);
gchar  *format_uptime    (guint64 seconds);

gchar **split_lines      (const gchar *text, gsize *count);
gboolean str_has_prefix_any (const gchar         *text,
                                const gchar *const  *prefixes,
                                gsize                n_prefixes);

gint    cmp_int64        (gconstpointer a, gconstpointer b);

G_END_DECLS

#endif
