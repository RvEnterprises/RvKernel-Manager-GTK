#ifndef RV_CORE_SYSTEM_INFO_H
#define RV_CORE_SYSTEM_INFO_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
        guint64 idle;
        guint64 total;
} RvCpuSample;

typedef struct {
        gchar   *hostname;
        gchar   *distro;
        gchar   *kernel;
        gchar   *arch;
        gchar   *cpu_model;
        gchar   *loadavg;
        gsize    n_cores;
        guint64  uptime_s;
        guint64  mem_total_kb;
        guint64  mem_available_kb;
        guint64  swap_total_kb;
        guint64  swap_free_kb;
        guint64  disk_swap_total_kb;
        guint64  disk_swap_used_kb;
} RvSystemInfo;

RvSystemInfo *rv_system_info_get     (void);
void          rv_system_info_free    (RvSystemInfo *info);

gsize         rv_cpu_sample_count    (void);
void          rv_cpu_usage_sample    (RvCpuSample *prev,
                                      gdouble     *per_core,
                                      gsize        n_cores,
                                      gdouble     *overall);

G_END_DECLS

#endif
