#ifndef UI_SIDEBAR_H
#define UI_SIDEBAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *sidebar_new(void);
void sidebar_set_stack(GtkWidget *sidebar, GtkStack *stack);

G_END_DECLS

#endif
