#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *card_new(const gchar *title);

void card_add(GtkWidget *card, GtkWidget *child);

GtkWidget *kv_row(const gchar *key);
void kv_set(GtkWidget *row, const gchar *fmt, ...) G_GNUC_PRINTF(2, 3);

GtkWidget *option_row_new(const gchar *title);

void option_row_append(GtkWidget *row, const gchar *id, const gchar *label);

void option_row_select_id(GtkWidget *row, const gchar *id);

const gchar *option_row_active_id(GtkWidget *row);

GtkDropDown *option_row_dropdown(GtkWidget *row);

GtkWidget *spin_row(const gchar *title, gdouble min, gdouble max, gdouble step,
		    gdouble value);

GtkSpinButton *spin_row_spin(GtkWidget *row);

GtkWidget *page_wrap(GtkWidget *content);

G_END_DECLS

#endif
