#ifndef RV_CORE_GPU_H
#define RV_CORE_GPU_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
        gchar    *devfreq_path;
        gchar    *name;
        gchar    *governor;
        gchar   **governors;
        gchar    *cur_freq_hz;
        gchar    *min_freq_hz;
        gchar    *max_freq_hz;
} RvDevfreq;

typedef struct {
        gchar      *card_path;
        gchar      *card_name;
        gchar      *driver;
        gchar      *vendor_name;
        gchar      *pci_id;
        RvDevfreq  *devfreq;
        gboolean    has_busy_percent;
        gchar      *busy_percent;
        gchar      *cur_clock_note;
} RvGpuCard;

void          rv_devfreq_refresh (RvDevfreq *d);
RvDevfreq   **rv_devfreq_list    (gsize *count);
void          rv_devfreq_free    (RvDevfreq *d);

gboolean      rv_devfreq_set_governor (RvDevfreq  *d,
                                       const gchar *governor,
                                       GError     **error);
gboolean      rv_devfreq_set_min_freq (RvDevfreq  *d,
                                       gint64       hz,
                                       GError     **error);
gboolean      rv_devfreq_set_max_freq (RvDevfreq  *d,
                                       gint64       hz,
                                       GError     **error);

RvGpuCard   **rv_gpu_cards       (gsize *count);
void          rv_gpu_card_refresh(RvGpuCard *card);
void          rv_gpu_card_free   (RvGpuCard *card);
void          rv_gpu_cards_free  (RvGpuCard **cards, gsize count);

G_END_DECLS

#endif
