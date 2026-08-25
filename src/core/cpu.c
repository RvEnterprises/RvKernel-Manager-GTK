#include "cpu.h"

#include "../util/format.h"
#include "../util/sysfs.h"

#include <stdlib.h>

static const char *const CPU_BASE = "/sys/devices/system/cpu";

static gint
cmp_policy_cpus_desc(gconstpointer a, gconstpointer b)
{
        const CpuPolicy *pa = *(const CpuPolicy *const *)a;
        const CpuPolicy *pb = *(const CpuPolicy *const *)b;

        return strcmp(pa->cpus_desc, pb->cpus_desc);
}

static gchar *
realpath_dup(const gchar *path)
{
        gchar *resolved = realpath(path, NULL);
        gchar *copy = g_strdup(resolved != NULL ? resolved : path);

        free(resolved);
        return copy;
}

static gchar **
read_token_list(const gchar *path)
{
        gchar *text;
        gchar **tokens;

        text = read_trimmed(path);
        tokens = text != NULL ? tokenize_ws(text) : g_new0(gchar *, 1);
        g_free(text);
        return tokens;
}

static void
policy_read_state(CpuPolicy *p)
{
        gchar *tmp;

        g_free(p->governor);
        g_free(p->epp);
        g_free(p->cur_freq_khz);
        g_free(p->min_freq_khz);
        g_free(p->max_freq_khz);

        tmp = g_build_filename(p->path, "scaling_governor", NULL);
        p->governor = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(p->path, "scaling_cur_freq", NULL);
        p->cur_freq_khz = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(p->path, "scaling_min_freq", NULL);
        p->min_freq_khz = read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(p->path, "scaling_max_freq", NULL);
        p->max_freq_khz = read_first_line(tmp);
        g_free(tmp);

        if (p->has_epp) {
                tmp = g_build_filename(p->path,
                                       "energy_performance_preference", NULL);
                p->epp = read_first_line(tmp);
                g_free(tmp);
        }
}

static void
policy_load_freqs(CpuPolicy *p)
{
        gchar *text;

        g_clear_pointer(&p->freqs_khz, free);
        p->n_freqs = 0;

        text = g_build_filename(p->path, "scaling_available_frequencies",
                                NULL);
        {
                gchar *list = read_trimmed(text);
                if (list != NULL && list[0] != '\0')
                        p->freqs_khz = parse_int_list(list, &p->n_freqs);
                g_free(list);
        }
        g_free(text);

        if ((p->freqs_khz == NULL || p->n_freqs < 2)) {
                gint64 min = 0, max = 0;
                gboolean have_min, have_max;
                gchar *tmp;

                tmp = g_build_filename(p->path, "scaling_min_freq", NULL);
                have_min = read_int64(tmp, &min);
                g_free(tmp);

                tmp = g_build_filename(p->path, "scaling_max_freq", NULL);
                have_max = read_int64(tmp, &max);
                g_free(tmp);

                if (!have_min || !have_max || min == max) {
                        g_clear_pointer(&p->freqs_khz, free);
                        p->n_freqs = 0;
                        return;
                }

                if (max < min) {
                        gint64 t = max;
                        max = min;
                        min = t;
                }

                p->freqs_khz = malloc(sizeof(gint64) * 2);
                p->n_freqs = 2;
                p->freqs_khz[0] = min;
                p->freqs_khz[1] = max;
        }
}

static CpuPolicy *
load_policy(const gchar *dir_path)
{
        CpuPolicy *p;
        gchar *tmp;

        p = g_new0(CpuPolicy, 1);
        p->path = g_strdup(dir_path);
        p->cpus_desc = g_path_get_basename(dir_path);

        tmp = g_build_filename(dir_path, "scaling_available_governors", NULL);
        p->governors = read_token_list(tmp);
        g_free(tmp);

        if (p->governors[0] == NULL) {
                g_strfreev(p->governors);
                p->governors = g_new0(gchar *, 2);
                tmp = g_build_filename(dir_path, "scaling_governor", NULL);
                p->governors[0] = read_first_line(tmp);
                g_free(tmp);
        }

        tmp = g_build_filename(dir_path,
                               "energy_performance_available_preferences",
                               NULL);
        p->has_epp = path_exists(tmp);
        g_free(tmp);

        if (p->has_epp) {
                tmp = g_build_filename(
                        dir_path, "energy_performance_available_preferences",
                        NULL);
                p->epps = read_token_list(tmp);
                g_free(tmp);
        }

        policy_load_freqs(p);
        policy_read_state(p);

        return p;
}

CpuPolicy **
cpu_policies(gsize *count)
{
        GPtrArray *result = g_ptr_array_new();
        GPtrArray *seen_paths = g_ptr_array_new_with_free_func(g_free);
        gchar *cpufreq_dir = g_build_filename(CPU_BASE, "cpufreq", NULL);
        gsize n_entries = 0;
        gchar **entries;

        entries = list_dir(cpufreq_dir, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *path;
                gchar *rp;
                gboolean dup = FALSE;

                if (!g_str_has_prefix(entries[i], "policy"))
                        continue;

                path = g_build_filename(cpufreq_dir, entries[i], NULL);
                rp = realpath_dup(path);

                for (gsize k = 0; k < seen_paths->len; k++) {
                        if (g_strcmp0(g_ptr_array_index(seen_paths, k),
                                      rp) == 0) {
                                dup = TRUE;
                                break;
                        }
                }

                if (dup) {
                        g_free(rp);
                        g_free(path);
                        continue;
                }
                g_ptr_array_add(seen_paths, rp);

                g_ptr_array_add(result, load_policy(path));
                g_free(path);
        }
        g_strfreev(entries);

        if (result->len == 0) {
                gsize n_cpus = 0;
                gchar **cpus;

                cpus = list_dir(CPU_BASE, &n_cpus);
                for (gsize i = 0; i < n_cpus; i++) {
                        gchar *cf;
                        gchar *full;

                        if (!g_str_has_prefix(cpus[i], "cpu"))
                                continue;
                        if (!(cpus[i][3] >= '0' && cpus[i][3] <= '9'))
                                continue;

                        full = g_build_filename(CPU_BASE, cpus[i], NULL);
                        cf = g_build_filename(full, "cpufreq", NULL);
                        if (is_dir(cf))
                                g_ptr_array_add(result, load_policy(cf));
                        g_free(cf);
                        g_free(full);
                }
                g_strfreev(cpus);
        }

        g_ptr_array_sort(result, cmp_policy_cpus_desc);
        g_free(cpufreq_dir);
        g_ptr_array_unref(seen_paths);

        if (count != NULL)
                *count = result->len;
        g_ptr_array_add(result, NULL);
        return (CpuPolicy **)g_ptr_array_free(result, FALSE);
}

void
cpu_policy_free(CpuPolicy *p)
{
        if (p == NULL)
                return;
        g_free(p->path);
        g_free(p->cpus_desc);
        g_free(p->governor);
        g_strfreev(p->governors);
        g_free(p->epp);
        g_strfreev(p->epps);
        g_free(p->cur_freq_khz);
        g_free(p->min_freq_khz);
        g_free(p->max_freq_khz);
        free(p->freqs_khz);
        g_free(p);
}

void
cpu_policies_free(CpuPolicy **policies, gsize count)
{
        if (policies == NULL)
                return;
        for (gsize i = 0; i < count; i++)
                cpu_policy_free(policies[i]);
        g_free(policies);
}

gboolean
cpu_set_governor(CpuPolicy *policy, const gchar *governor, GError **error)
{
        gchar *path = g_build_filename(policy->path, "scaling_governor", NULL);
        gboolean ok = write_string(path, governor, error);
        g_free(path);
        return ok;
}

gboolean
cpu_set_epp(CpuPolicy *policy, const gchar *preference, GError **error)
{
        gchar *path = g_build_filename(policy->path,
                                       "energy_performance_preference", NULL);
        gboolean ok = write_string(path, preference, error);
        g_free(path);
        return ok;
}

gboolean
cpu_set_min_freq(CpuPolicy *policy, gint64 khz, GError **error)
{
        gchar *path = g_build_filename(policy->path, "scaling_min_freq", NULL);
        gboolean ok = write_int64(path, khz, error);
        g_free(path);
        return ok;
}

gboolean
cpu_set_max_freq(CpuPolicy *policy, gint64 khz, GError **error)
{
        gchar *path = g_build_filename(policy->path, "scaling_max_freq", NULL);
        gboolean ok = write_int64(path, khz, error);
        g_free(path);
        return ok;
}
