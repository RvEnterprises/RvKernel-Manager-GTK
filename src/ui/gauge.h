#ifndef RV_UI_GAUGE_H
#define RV_UI_GAUGE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *rv_gauge_new         (const gchar *caption);

void       rv_gauge_set_fraction(GtkWidget *gauge,
                                 gdouble    fraction);

void       rv_gauge_set_text    (GtkWidget   *gauge,
                                 const gchar *fmt,
                                 ...) G_GNUC_PRINTF(2, 3);

G_END_DECLS

#endif
