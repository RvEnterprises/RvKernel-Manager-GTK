#ifndef RV_UI_PAGES_H
#define RV_UI_PAGES_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*RvPageRefreshFunc)(GtkWidget *page);

GtkWidget *rv_page_dashboard_new (GtkWidget *window);
GtkWidget *rv_page_cpu_new       (GtkWidget *window);
GtkWidget *rv_page_gpu_new       (GtkWidget *window);
GtkWidget *rv_page_battery_new   (GtkWidget *window);
GtkWidget *rv_page_memory_new    (GtkWidget *window);
GtkWidget *rv_page_thermal_new   (GtkWidget *window);
GtkWidget *rv_page_about_new     (GtkWidget *window);

void       rv_page_set_refresh   (GtkWidget       *page,
                                  RvPageRefreshFunc fn);
void       rv_page_refresh       (GtkWidget       *page);

void       rv_stack_refresh_visible (GtkStack *stack);

const gchar *rv_window_error_text (GError **error);

G_END_DECLS

#endif
