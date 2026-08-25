#include "sysfs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

gchar *
read_trimmed(const gchar *path)
{
        gchar *contents = NULL;

        if (path == NULL)
                return NULL;
        if (!g_file_get_contents(path, &contents, NULL, NULL))
                return NULL;

        return g_strstrip(contents);
}

gchar *
read_first_line(const gchar *path)
{
        gchar *text = read_trimmed(path);
        gchar *end;

        if (text == NULL)
                return NULL;

        end = strchr(text, '\n');
        if (end != NULL)
                *end = '\0';

        return text;
}

gboolean
write_string(const gchar *path, const gchar *value, GError **error)
{
        int fd;
        gssize written;

        fd = open(path, O_WRONLY);
        if (fd < 0) {
                int saved = errno;
                g_set_error(error, G_IO_ERROR,
                            g_io_error_from_errno(saved),
                            "%s: %s", path, g_strerror(saved));
                return FALSE;
        }

        written = write(fd, value, strlen(value));
        if (written < 0) {
                int saved = errno;
                close(fd);
                g_set_error(error, G_IO_ERROR,
                            g_io_error_from_errno(saved),
                            "%s: %s", path, g_strerror(saved));
                return FALSE;
        }

        close(fd);
        return TRUE;
}

gboolean
write_int64(const gchar *path, gint64 value, GError **error)
{
        gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

        g_snprintf(buf, sizeof(buf), "%" G_GINT64_FORMAT, value);
        return write_string(path, buf, error);
}

gboolean
read_int64(const gchar *path, gint64 *out_value)
{
        gchar *text;
        gchar *end;
        gint64 value;

        text = read_trimmed(path);
        if (text == NULL || text[0] == '\0') {
                g_free(text);
                return FALSE;
        }

        value = g_ascii_strtoll(text, &end, 10);
        if (end == text) {
                g_free(text);
                return FALSE;
        }

        if (out_value != NULL)
                *out_value = value;

        g_free(text);
        return TRUE;
}

gboolean
read_double(const gchar *path, gdouble *out_value)
{
        gint64 value;

        if (!read_int64(path, &value))
                return FALSE;
        if (out_value != NULL)
                *out_value = (gdouble)value;
        return TRUE;
}

gboolean
path_exists(const gchar *path)
{
        return access(path, F_OK) == 0;
}

gboolean
is_dir(const gchar *path)
{
        struct stat st;

        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

gboolean
is_root(void)
{
        return geteuid() == 0;
}

static gint
cmp_names(gconstpointer a, gconstpointer b)
{
        return g_strcmp0(*(const gchar *const *)a, *(const gchar *const *)b);
}

gchar **
list_dir(const gchar *path, gsize *count)
{
        DIR *dir;
        struct dirent *entry;
        GPtrArray *names;

        names = g_ptr_array_new_with_free_func(g_free);
        dir = opendir(path);
        if (dir == NULL) {
                if (count != NULL)
                        *count = 0;
                g_ptr_array_add(names, NULL);
                return (gchar **)g_ptr_array_free(names, FALSE);
        }

        while ((entry = readdir(dir)) != NULL) {
                if (g_strcmp0(entry->d_name, ".") == 0 ||
                    g_strcmp0(entry->d_name, "..") == 0)
                        continue;
                g_ptr_array_add(names, g_strdup(entry->d_name));
        }
        closedir(dir);

        g_ptr_array_sort(names, cmp_names);

        if (count != NULL)
                *count = names->len;

        g_ptr_array_add(names, NULL);
        return (gchar **)g_ptr_array_free(names, FALSE);
}

gint64 *
parse_int_list(const gchar *text, gsize *count)
{
        GArray *values;
        const gchar *p;

        values = g_array_new(FALSE, FALSE, sizeof(gint64));
        p = text;
        while (*p != '\0') {
                gchar *end;
                gint64 v;

                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                        p++;
                if (*p == '\0')
                        break;

                v = g_ascii_strtoll(p, &end, 10);
                if (end == p)
                        break;
                g_array_append_val(values, v);
                p = end;
        }

        if (count != NULL)
                *count = values->len;

        return (gint64 *)g_array_free(values, FALSE);
}
