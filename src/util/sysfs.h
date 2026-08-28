#ifndef UTIL_SYSFS_H
#define UTIL_SYSFS_H

#include <glib.h>

G_BEGIN_DECLS

gchar *read_trimmed(const gchar *path);
gchar *read_first_line(const gchar *path);

gboolean write_string(const gchar *path, const gchar *value, GError **error);
gboolean write_int64(const gchar *path, gint64 value, GError **error);

gboolean read_int64(const gchar *path, gint64 *out_value);
gboolean read_double(const gchar *path, gdouble *out_value);

gchar *read_trimmed_in(const gchar *dir, const gchar *name);
gchar *read_first_line_in(const gchar *dir, const gchar *name);
gboolean read_int64_in(const gchar *dir, const gchar *name, gint64 *out_value);
gboolean read_double_in(const gchar *dir, const gchar *name,
			gdouble *out_value);

gboolean path_exists(const gchar *path);
gboolean is_dir(const gchar *path);
gboolean is_root(void);

gchar **list_dir(const gchar *path, gsize *count);

gint64 *parse_int_list(const gchar *text, gsize *count);

G_END_DECLS

#endif
