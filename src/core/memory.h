#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

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
} Zram;

typedef struct {
        const gchar *name;
        const gchar *label;
        gint         min;
        gint         max;
        gint         step;
} VmTunable;

extern const VmTunable VM_TUNABLES[];
extern const gsize       VM_TUNABLES_COUNT;

Zram     **zram_list      (gsize *count);
void          zram_refresh   (Zram *z);
void          zram_free      (Zram *z);
void          zram_list_free (Zram **list, gsize count);

gboolean      zram_set_algo  (Zram *z, const gchar *algo, GError **error);

gboolean      vm_tunable_get    (const VmTunable *t, gint *value);
gboolean      vm_tunable_set    (const VmTunable *t, gint value, GError **error);

gchar       **tcp_cc_list    (gsize *count);
gchar        *tcp_cc_current (void);
gboolean      tcp_cc_set     (const gchar *cc, GError **error);

G_END_DECLS

#endif
