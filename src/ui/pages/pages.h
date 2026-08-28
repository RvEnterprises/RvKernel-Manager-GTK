#ifndef UI_PAGES_H
#define UI_PAGES_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*PageRefreshFunc)(GtkWidget *page);

GtkWidget *page_dashboard_new(GtkWidget *window);
GtkWidget *page_cpu_new(GtkWidget *window);
GtkWidget *page_gpu_new(GtkWidget *window);
GtkWidget *page_battery_new(GtkWidget *window);
GtkWidget *page_memory_new(GtkWidget *window);
GtkWidget *page_about_new(GtkWidget *window);

void page_set_refresh(GtkWidget *page, PageRefreshFunc fn);
void page_refresh(GtkWidget *page);

void stack_refresh_visible(GtkStack *stack);

const gchar *window_error_text(GError **error);

G_END_DECLS

#endif
