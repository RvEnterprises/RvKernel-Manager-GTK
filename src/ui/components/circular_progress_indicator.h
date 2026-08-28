#ifndef UI_CIRCULAR_PROGRESS_INDICATOR_H
#define UI_CIRCULAR_PROGRESS_INDICATOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *circular_progress_indicator_new(const gchar *caption);

void circular_progress_indicator_set_fraction(GtkWidget *cpi, gdouble fraction);

void circular_progress_indicator_set_text(GtkWidget *cpi, const gchar *fmt, ...)
	G_GNUC_PRINTF(2, 3);

G_END_DECLS

#endif
