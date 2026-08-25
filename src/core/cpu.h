#ifndef CORE_CPU_H
#define CORE_CPU_H

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
} CpuPolicy;

CpuPolicy **cpu_policies       (gsize *count);
void          cpu_policy_free    (CpuPolicy *policy);
void          cpu_policies_free  (CpuPolicy **policies, gsize count);

gboolean      cpu_set_governor   (CpuPolicy *policy,
                                     const gchar *governor,
                                     GError     **error);
gboolean      cpu_set_epp        (CpuPolicy *policy,
                                     const gchar *preference,
                                     GError     **error);
gboolean      cpu_set_min_freq   (CpuPolicy *policy,
                                     gint64       khz,
                                     GError     **error);
gboolean      cpu_set_max_freq   (CpuPolicy *policy,
                                     gint64       khz,
                                     GError     **error);

G_END_DECLS

#endif
