#include "memory.h"

#include "../util/format.h"
#include "../util/log.h"
#include "../util/sysfs.h"

#include <stdlib.h>
#include <string.h>

const VmTunable VM_TUNABLES[] = {
        { "swappiness",             "Swappiness",               0, 200,  1 },
        { "vfs_cache_pressure",     "VFS cache pressure",       0, 1000000, 10 },
        { "dirty_ratio",            "Dirty ratio",              1, 100,  1 },
        { "dirty_background_ratio", "Dirty background ratio",   1, 100,  1 },
};

const gsize VM_TUNABLES_COUNT = G_N_ELEMENTS(VM_TUNABLES);

static const char *const ZRAM_BLOCK_BASE = "/sys/block";
static const char *const VM_SYSCTL_BASE = "/proc/sys/vm";
static const char *const TCP_CC_PATH =
        "/proc/sys/net/ipv4/tcp_congestion_control";
static const char *const TCP_CC_AVAIL_PATH =
        "/proc/sys/net/ipv4/tcp_available_congestion_control";

static gchar *
zram_parse_active_algo(const gchar *text)
{
        const gchar *start;
        const gchar *end;

        start = strchr(text, '[');
        if (start == NULL)
                return NULL;
        end = strchr(start + 1, ']');
        if (end == NULL)
                return NULL;

        return g_strndup(start + 1, (gsize)(end - start - 1));
}

static void
zram_read_static(Zram *z)
{
        gchar *text;
        gint64 bytes;

        if (read_int64_in(z->path, "disksize", &bytes) && bytes > 0) {
                z->disksize_bytes = (guint64)bytes;
                z->disksize_str = format_bytes((guint64)bytes);
        }

        text = read_trimmed_in(z->path, "comp_algorithm");

        z->algo = text != NULL ? zram_parse_active_algo(text) : NULL;
        if (z->algo == NULL && text != NULL && text[0] != '\0') {
                gchar **tokens = tokenize_ws(text);
                if (tokens[0] != NULL)
                        z->algo = g_strdup(tokens[0]);
                g_strfreev(tokens);
        }

        if (text != NULL) {
                GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);
                gchar **tokens = tokenize_ws(text);
                for (gsize i = 0; tokens[i] != NULL; i++) {
                        gsize len = strlen(tokens[i]);
                        if (len >= 2 && tokens[i][0] == '[' &&
                            tokens[i][len - 1] == ']')
                                tokens[i][len - 1] = '\0';
                        g_ptr_array_add(arr,
                                        g_strdup(tokens[i] +
                                                 (tokens[i][0] == '[' ? 1 : 0)));
                }
                g_strfreev(tokens);
                g_ptr_array_add(arr, NULL);
                z->algos = (gchar **)g_ptr_array_free(arr, FALSE);
        } else {
                z->algos = g_new0(gchar *, 1);
        }
        g_free(text);
}

Zram *
zram_new(const gchar *path, const gchar *name)
{
        Zram *z = g_new0(Zram, 1);

        z->path = g_strdup(path);
        z->name = g_strdup(name);
        zram_read_static(z);
        zram_refresh(z);
        return z;
}

void
zram_refresh(Zram *z)
{
        gchar *text;

        if (z == NULL)
                return;

        text = read_trimmed_in(z->path, "mm_stat");
        z->has_stats = FALSE;
        if (text != NULL) {
                gint64 *values = NULL;
                gsize n_values = 0;
                values = parse_int_list(text, &n_values);
                if (n_values >= 3) {
                        z->orig_bytes = (guint64)values[0];
                        z->compr_bytes = (guint64)values[1];
                        z->used_bytes = (guint64)values[2];
                        z->has_stats = TRUE;
                }
                free(values);
        }
        g_free(text);
}

Zram **
zram_list(gsize *count)
{
        GPtrArray *result;
        gsize n_entries = 0;
        gchar **entries;

        result = g_ptr_array_new();
        entries = list_dir(ZRAM_BLOCK_BASE, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *path;
                Zram *z;

                if (!g_str_has_prefix(entries[i], "zram"))
                        continue;
                path = g_build_filename(ZRAM_BLOCK_BASE, entries[i], NULL);
                z = zram_new(path, entries[i]);
                g_ptr_array_add(result, z);
                g_free(path);
        }
        g_strfreev(entries);

        if (count != NULL)
                *count = result->len;
        log_debug("zram: %u devices", (guint)result->len);
        g_ptr_array_add(result, NULL);
        return (Zram **)g_ptr_array_free(result, FALSE);
}

void
zram_free(Zram *z)
{
        if (z == NULL)
                return;
        g_free(z->path);
        g_free(z->name);
        g_free(z->disksize_str);
        g_free(z->algo);
        g_strfreev(z->algos);
        g_free(z);
}

void
zram_list_free(Zram **list, gsize count)
{
        if (list == NULL)
                return;
        for (gsize i = 0; i < count; i++)
                zram_free(list[i]);
        g_free(list);
}

gboolean
zram_set_algo(Zram *z, const gchar *algo, GError **error)
{
        gchar *tmp = g_build_filename(z->path, "comp_algorithm", NULL);
        gboolean ok = write_string(tmp, algo, error);

        if (ok)
                log_info("%s: algorithm -> %s", z->path, algo);
        else
                log_warn("%s: algorithm -> %s not applied", z->path, algo);
        g_free(tmp);
        return ok;
}

gboolean
vm_tunable_get(const VmTunable *t, gint *value)
{
        gchar *path = g_build_filename(VM_SYSCTL_BASE, t->name, NULL);
        gint64 v;
        gboolean ok = read_int64(path, &v);
        g_free(path);

        if (ok && value != NULL)
                *value = (gint)v;
        return ok;
}

gboolean
vm_tunable_set(const VmTunable *t, gint value, GError **error)
{
        gchar *path = g_build_filename(VM_SYSCTL_BASE, t->name, NULL);
        gboolean ok = write_int64(path, value, error);

        if (ok)
                log_info("vm.%s -> %d", t->name, value);
        else
                log_warn("vm.%s -> %d not applied", t->name, value);
        g_free(path);
        return ok;
}

gchar **
tcp_cc_list(gsize *count)
{
        gchar *text;
        gchar **tokens;

        if (count != NULL)
                *count = 0;

        text = read_trimmed(TCP_CC_AVAIL_PATH);
        if (text == NULL)
                return g_new0(gchar *, 1);

        tokens = tokenize_ws(text);
        g_free(text);

        if (count != NULL) {
                gsize n = 0;
                while (tokens[n] != NULL)
                        n++;
                *count = n;
        }

        return tokens;
}

gchar *
tcp_cc_current(void)
{
        return read_first_line(TCP_CC_PATH);
}

gboolean
tcp_cc_set(const gchar *cc, GError **error)
{
        gboolean ok = write_string(TCP_CC_PATH, cc, error);

        if (ok)
                log_info("tcp congestion control -> %s", cc);
        else
                log_warn("tcp congestion control -> %s not applied", cc);
        return ok;
}
