#ifndef RV_CORE_MEMORY_H
#define RV_CORE_MEMORY_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
        gchar    *path;
        gchar    *name;
        gchar    *disksize_str;
        gchar    *algo;
        gchar   **algos;
        guint64   disksize_bytes;
        guint64   orig_bytes;
        guint64   compr_bytes;
        guint64   used_bytes;
        gboolean  has_stats;
} RvZram;

typedef struct {
        const gchar *name;
        const gchar *label;
        gint         min;
        gint         max;
        gint         step;
} RvVmTunable;

extern const RvVmTunable RV_VM_TUNABLES[];
extern const gsize       RV_VM_TUNABLES_COUNT;

RvZram     **rv_zram_list      (gsize *count);
void          rv_zram_refresh   (RvZram *z);
void          rv_zram_free      (RvZram *z);
void          rv_zram_list_free (RvZram **list, gsize count);

gboolean      rv_zram_set_algo  (RvZram *z, const gchar *algo, GError **error);

gboolean      vm_tunable_get    (const RvVmTunable *t, gint *value);
gboolean      vm_tunable_set    (const RvVmTunable *t, gint value, GError **error);

gchar       **rv_tcp_cc_list    (gsize *count);
gchar        *rv_tcp_cc_current (void);
gboolean      rv_tcp_cc_set     (const gchar *cc, GError **error);

G_END_DECLS

#endif
