#ifndef RV_UI_SIDEBAR_H
#define RV_UI_SIDEBAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *rv_sidebar_new (void);
void       rv_sidebar_set_stack (GtkWidget *sidebar,
                                 GtkStack  *stack);

G_END_DECLS

#endif
