#ifndef CORE_GPU_H
#define CORE_GPU_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gchar *devfreq_path;
	gchar *name;
	gchar *governor;
	gchar **governors;
	gchar *cur_freq_hz;
	gchar *min_freq_hz;
	gchar *max_freq_hz;
} Devfreq;

typedef struct {
	gchar *card_path;
	gchar *card_name;
	gchar *device_path;
	gchar *driver;
	gchar *vendor_name;
	gchar *pci_id;
	Devfreq *devfreq;
	gboolean has_busy_percent;
	gchar *busy_percent;
	gchar *cur_clock_note;
} GpuCard;

void devfreq_refresh(Devfreq *d);
Devfreq **devfreq_list(gsize *count);
void devfreq_free(Devfreq *d);

gboolean devfreq_set_governor(Devfreq *d, const gchar *governor,
			      GError **error);
gboolean devfreq_set_min_freq(Devfreq *d, gint64 hz, GError **error);
gboolean devfreq_set_max_freq(Devfreq *d, gint64 hz, GError **error);

GpuCard **gpu_cards(gsize *count);
void gpu_card_refresh(GpuCard *card);
void gpu_card_free(GpuCard *card);
void gpu_cards_free(GpuCard **cards, gsize count);

G_END_DECLS

#endif
