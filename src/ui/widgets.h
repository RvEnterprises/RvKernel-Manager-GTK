#ifndef RV_UI_WIDGETS_H
#define RV_UI_WIDGETS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *rv_card_new        (const gchar *title);

void       rv_card_add        (GtkWidget  *card,
                               GtkWidget  *child);

GtkWidget *rv_kv_row          (const gchar *key);
void       rv_kv_set          (GtkWidget   *row,
                               const gchar *fmt,
                               ...) G_GNUC_PRINTF(2, 3);

GtkWidget *rv_option_row_new      (const gchar *title);

void       rv_option_row_append   (GtkWidget   *row,
                                   const gchar *id,
                                   const gchar *label);

void       rv_option_row_select_id(GtkWidget   *row,
                                   const gchar *id);

const gchar *
           rv_option_row_active_id(GtkWidget   *row);

GtkDropDown *
           rv_option_row_dropdown (GtkWidget   *row);

GtkWidget *rv_spin_row        (const gchar *title,
                               gdouble      min,
                               gdouble      max,
                               gdouble      step,
                               gdouble      value);

GtkSpinButton *
           rv_spin_row_spin    (GtkWidget *row);

GtkWidget *rv_page_wrap       (GtkWidget *content);

G_END_DECLS

#endif
