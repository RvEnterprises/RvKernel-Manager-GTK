#ifndef RV_UTIL_SYSFS_H
#define RV_UTIL_SYSFS_H

#include <glib.h>

G_BEGIN_DECLS

gchar   *rv_read_trimmed   (const gchar *path);
gchar   *rv_read_first_line(const gchar *path);

gboolean rv_write_string   (const gchar *path,
                            const gchar *value,
                            GError     **error);
gboolean rv_write_int64    (const gchar *path,
                            gint64       value,
                            GError     **error);

gboolean rv_read_int64     (const gchar *path,
                            gint64      *out_value);
gboolean rv_read_double    (const gchar *path,
                            gdouble     *out_value);

gboolean rv_path_exists    (const gchar *path);
gboolean rv_is_dir         (const gchar *path);
gboolean rv_is_root        (void);

gchar  **rv_list_dir       (const gchar *path,
                            gsize       *count);

gint64  *rv_parse_int_list (const gchar *text,
                            gsize       *count);

G_END_DECLS

#endif
