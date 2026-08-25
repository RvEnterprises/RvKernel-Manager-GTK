#include "system_info.h"

#include "../util/format.h"
#include "../util/sysfs.h"

#include <string.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <time.h>

static gchar *
read_os_pretty_name(void)
{
        gchar *content;
        gchar **lines;
        gchar *result = NULL;
        gsize i;

        content = read_trimmed("/etc/os-release");
        if (content == NULL)
                content = read_trimmed("/usr/lib/os-release");
        if (content == NULL)
                return g_strdup("Unknown");

        lines = g_strsplit(content, "\n", -1);
        for (i = 0; lines[i] != NULL; i++) {
                if (g_str_has_prefix(lines[i], "PRETTY_NAME=")) {
                        result = g_strdup(lines[i] + strlen("PRETTY_NAME="));
                        break;
                }
        }
        g_strfreev(lines);
        g_free(content);

        if (result != NULL) {
                gsize len = strlen(result);
                if (len >= 2 && result[0] == '"' && result[len - 1] == '"') {
                        result[len - 1] = '\0';
                        memmove(result, result + 1, len - 1);
                }
        }

        return result != NULL ? result : g_strdup("Unknown");
}

static gchar *
read_cpu_model(void)
{
        static const gchar *const keys[] = { "model name", "Processor",
                                             "processor model" };
        gchar *content;
        gchar **lines;
        gchar *result = NULL;
        gsize i, j;

        content = read_trimmed("/proc/cpuinfo");
        if (content == NULL)
                return g_strdup("Unknown");

        lines = g_strsplit(content, "\n", -1);
        for (j = 0; j < G_N_ELEMENTS(keys) && result == NULL; j++) {
                for (i = 0; lines[i] != NULL; i++) {
                        const gchar *colon;
                        if (!g_str_has_prefix(lines[i], keys[j]))
                                continue;
                        colon = strchr(lines[i], ':');
                        if (colon == NULL)
                                continue;
                        result = g_strdup(g_strstrip((gchar *)colon + 1));
                        if (result[0] != '\0')
                                break;
                        g_clear_pointer(&result, g_free);
                }
        }
        g_strfreev(lines);
        g_free(content);

        return result != NULL ? result : g_strdup("Unknown");
}

static void
read_disk_swap(SystemInfo *info)
{
        gchar *content;
        gchar **lines;

        content = read_trimmed("/proc/swaps");
        if (content == NULL)
                return;

        lines = g_strsplit(content, "\n", -1);
        for (gsize i = 1; lines[i] != NULL; i++) {
                gchar *base;
                gchar **fields;

                if (lines[i][0] == '\0')
                        continue;

                base = g_path_get_basename(lines[i]);
                if (g_str_has_prefix(base, "zram")) {
                        g_free(base);
                        continue;
                }
                g_free(base);

                fields = tokenize_ws(lines[i]);
                if (fields[0] != NULL && fields[1] != NULL &&
                    fields[2] != NULL && fields[3] != NULL) {
                        info->disk_swap_total_kb +=
                                g_ascii_strtoull(fields[2], NULL, 10);
                        info->disk_swap_used_kb +=
                                g_ascii_strtoull(fields[3], NULL, 10);
                }
                g_strfreev(fields);
        }
        g_strfreev(lines);
        g_free(content);
}

static struct {
        gchar *hostname;
        gchar *distro;
        gchar *kernel;
        gchar *arch;
        gchar *cpu_model;
        gsize n_cores;
} info_cache;

static void
info_cache_init(void)
{
        struct utsname uts;
        glong cores;

        if (uname(&uts) == 0) {
                info_cache.hostname = g_strdup(uts.nodename);
                info_cache.kernel = g_strdup_printf("%s %s", uts.sysname,
                                                    uts.release);
                info_cache.arch = g_strdup(uts.machine);
        } else {
                info_cache.hostname = g_strdup("localhost");
                info_cache.kernel = g_strdup("unknown");
                info_cache.arch = g_strdup("unknown");
        }

        info_cache.distro = read_os_pretty_name();
        info_cache.cpu_model = read_cpu_model();

        cores = sysconf(_SC_NPROCESSORS_ONLN);
        info_cache.n_cores = cores > 0 ? (gsize)cores : 1;
}

SystemInfo *
system_info_get(void)
{
        SystemInfo *info = g_new0(SystemInfo, 1);
        struct sysinfo si;
        gchar *meminfo;
        gchar **lines;

        if (G_UNLIKELY(info_cache.hostname == NULL))
                info_cache_init();

        info->hostname = g_strdup(info_cache.hostname);
        info->distro = g_strdup(info_cache.distro);
        info->kernel = g_strdup(info_cache.kernel);
        info->arch = g_strdup(info_cache.arch);
        info->cpu_model = g_strdup(info_cache.cpu_model);
        info->n_cores = info_cache.n_cores;

        info->loadavg = read_first_line("/proc/loadavg");
        if (info->loadavg != NULL) {
                gchar *space = strchr(info->loadavg, ' ');
                if (space != NULL) {
                        space = strchr(space + 1, ' ');
                        if (space != NULL) {
                                space = strchr(space + 1, ' ');
                                if (space != NULL)
                                        *space = '\0';
                        }
                }
        }
        if (info->loadavg == NULL)
                info->loadavg = g_strdup("-");

        if (sysinfo(&si) == 0) {
                info->uptime_s = (guint64)si.uptime;
                info->mem_total_kb = (guint64)si.totalram * si.mem_unit / 1024;
                info->mem_available_kb = ((guint64)si.totalram -
                                          (guint64)si.freeram -
                                          (guint64)si.bufferram) *
                                         si.mem_unit / 1024;
                info->swap_total_kb = (guint64)si.totalswap * si.mem_unit / 1024;
                info->swap_free_kb = (guint64)si.freeswap * si.mem_unit / 1024;
        }

        meminfo = read_trimmed("/proc/meminfo");
        if (meminfo != NULL) {
                lines = g_strsplit(meminfo, "\n", -1);
                for (gsize i = 0; lines[i] != NULL; i++) {
                        guint64 kb;
                        const gchar *colon;
                        const char *names[] = { "MemTotal:", "MemAvailable:",
                                                "SwapTotal:", "SwapFree:" };
                        guint64 *slots[] = { &info->mem_total_kb,
                                             &info->mem_available_kb,
                                             &info->swap_total_kb,
                                             &info->swap_free_kb };
                        for (gint k = 0; k < 4; k++) {
                                if (!g_str_has_prefix(lines[i], names[k]))
                                        continue;
                                colon = strchr(lines[i], ':');
                                if (colon == NULL)
                                        continue;
                                kb = g_ascii_strtoull(colon + 1, NULL, 10);
                                *slots[k] = kb;
                        }
                }
                g_strfreev(lines);
                g_free(meminfo);
        }

        read_disk_swap(info);

        return info;
}

void
system_info_free(SystemInfo *info)
{
        if (info == NULL)
                return;
        g_free(info->hostname);
        g_free(info->distro);
        g_free(info->kernel);
        g_free(info->arch);
        g_free(info->cpu_model);
        g_free(info->loadavg);
        g_free(info);
}

gsize
cpu_sample_count(void)
{
        gchar *stat = read_trimmed("/proc/stat");
        gsize count = 0;

        if (stat != NULL) {
                gchar **lines = split_lines(stat, NULL);
                for (gsize i = 0; lines[i] != NULL; i++) {
                        if (g_str_has_prefix(lines[i], "cpu") &&
                            lines[i][3] >= '0' && lines[i][3] <= '9')
                                count++;
                }
                g_strfreev(lines);
                g_free(stat);
        }

        return count > 0 ? count : 1;
}

static gboolean
parse_stat_line(const gchar *line, guint64 *idle, guint64 *total)
{
        gint64 fields[10];
        gsize n = 0;
        const gchar *p;

        p = strchr(line, ' ');
        if (p == NULL)
                return FALSE;

        while (*p != '\0' && n < G_N_ELEMENTS(fields)) {
                gchar *end;
                gint64 v;

                while (*p == ' ')
                        p++;
                if (*p == '\0')
                        break;

                v = g_ascii_strtoll(p, &end, 10);
                if (end == p)
                        break;
                fields[n++] = v;
                p = end;
        }

        if (n < 5)
                return FALSE;

        *idle = (guint64)fields[3] + (n > 4 ? (guint64)fields[4] : 0);
        *total = 0;
        for (gsize i = 0; i < n; i++)
                *total += (guint64)fields[i];

        return TRUE;
}

void
cpu_usage_sample(CpuSample *prev, gdouble *per_core,
                    gsize n_cores, gdouble *overall)
{
        gchar *stat;
        gchar **lines;
        gsize core = 0;
        guint64 idle_sum = 0, total_sum = 0;
        gsize counted = 0;

        if (overall != NULL)
                *overall = -1.0;
        if (per_core != NULL) {
                for (gsize i = 0; i < n_cores; i++)
                        per_core[i] = -1.0;
        }

        stat = read_trimmed("/proc/stat");
        if (stat == NULL)
                return;

        lines = split_lines(stat, NULL);
        for (gsize i = 0; lines[i] != NULL; i++) {
                guint64 idle = 0, total = 0;

                if (!g_str_has_prefix(lines[i], "cpu") ||
                    !(lines[i][3] >= '0' && lines[i][3] <= '9'))
                        continue;
                if (!parse_stat_line(lines[i], &idle, &total))
                        continue;

                idle_sum += idle;
                total_sum += total;
                counted++;

                if (per_core != NULL && prev != NULL &&
                    core < n_cores && prev[core].total > 0 &&
                    total > prev[core].total) {
                        gdouble didle = (gdouble)(idle - prev[core].idle);
                        gdouble dtotal = (gdouble)(total - prev[core].total);
                        per_core[core] = 1.0 - didle / dtotal;
                }
                if (prev != NULL && core < n_cores) {
                        prev[core].idle = idle;
                        prev[core].total = total;
                }
                core++;
        }
        g_strfreev(lines);
        g_free(stat);

        if (overall != NULL && prev != NULL && counted > 0 &&
            prev[n_cores].total > 0 && total_sum > prev[n_cores].total) {
                gdouble didle = (gdouble)(idle_sum - prev[n_cores].idle);
                gdouble dtotal = (gdouble)(total_sum - prev[n_cores].total);
                *overall = CLAMP(1.0 - didle / dtotal, 0.0, 1.0);
        }

        if (prev != NULL && counted > 0) {
                prev[n_cores].idle = idle_sum;
                prev[n_cores].total = total_sum;
        }
}
