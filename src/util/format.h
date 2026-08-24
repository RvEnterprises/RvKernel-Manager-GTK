#ifndef RV_UTIL_FORMAT_H
#define RV_UTIL_FORMAT_H

#include <glib.h>

G_BEGIN_DECLS

gchar **rv_tokenize_ws      (const gchar *text);

gchar  *rv_format_khz       (gint64 khz);
gchar  *rv_format_hz        (gint64 hz);
gchar  *rv_format_bytes     (guint64 bytes);
gchar  *rv_format_uptime    (guint64 seconds);

gchar **rv_split_lines      (const gchar *text, gsize *count);
gboolean rv_str_has_prefix_any (const gchar         *text,
                                const gchar *const  *prefixes,
                                gsize                n_prefixes);

gint    rv_cmp_int64        (gconstpointer a, gconstpointer b);

G_END_DECLS

#endif
