#ifndef RV_CORE_CPU_H
#define RV_CORE_CPU_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
        gchar    *path;
        gchar    *cpus_desc;
        gchar    *governor;
        gchar   **governors;
        gchar    *epp;
        gchar   **epps;
        gboolean  has_epp;
        gchar    *cur_freq_khz;
        gchar    *min_freq_khz;
        gchar    *max_freq_khz;
        gint64   *freqs_khz;
        gsize     n_freqs;
} RvCpuPolicy;

RvCpuPolicy **rv_cpu_policies       (gsize *count);
void          rv_cpu_policy_free    (RvCpuPolicy *policy);
void          rv_cpu_policies_free  (RvCpuPolicy **policies, gsize count);

gboolean      rv_cpu_set_governor   (RvCpuPolicy *policy,
                                     const gchar *governor,
                                     GError     **error);
gboolean      rv_cpu_set_epp        (RvCpuPolicy *policy,
                                     const gchar *preference,
                                     GError     **error);
gboolean      rv_cpu_set_min_freq   (RvCpuPolicy *policy,
                                     gint64       khz,
                                     GError     **error);
gboolean      rv_cpu_set_max_freq   (RvCpuPolicy *policy,
                                     gint64       khz,
                                     GError     **error);

G_END_DECLS

#endif
