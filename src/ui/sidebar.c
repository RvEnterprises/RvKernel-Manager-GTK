#include "sidebar.h"

#include "../util/log.h"

typedef struct {
        GtkStack *stack;
        GtkListBox *list;
        gulong items_changed_id;
        gulong visible_child_id;
        gboolean syncing;
} Sidebar;

static void
ctx_free(gpointer data)
{
        Sidebar *self = data;

        if (self->stack != NULL) {
                g_signal_handler_disconnect(
                        gtk_stack_get_pages(self->stack),
                        self->items_changed_id);
                g_signal_handler_disconnect(self->stack,
                                            self->visible_child_id);
                g_object_unref(self->stack);
        }
        g_free(self);
}

static void
update_row(GtkWidget *row)
{
        GtkStackPage *page = g_object_get_data(G_OBJECT(row), "page");
        GtkWidget *box = gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(row));
        GtkWidget *icon = gtk_widget_get_first_child(box);
        GtkWidget *label = gtk_widget_get_next_sibling(icon);
        const gchar *icon_name;
        const gchar *title;

        icon_name = gtk_stack_page_get_icon_name(page);
        title = gtk_stack_page_get_title(page);

        if (icon_name != NULL && icon_name[0] != '\0') {
                gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name);
                gtk_widget_set_visible(icon, TRUE);
        } else {
                gtk_widget_set_visible(icon, FALSE);
        }
        gtk_label_set_text(GTK_LABEL(label),
                           title != NULL ? title : "");
        gtk_widget_set_visible(row, gtk_stack_page_get_visible(page));
}

static void
page_notify_cb(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
        const gchar *name = g_param_spec_get_name(pspec);

        (void)obj;
        if (g_strcmp0(name, "title") == 0 ||
            g_strcmp0(name, "icon-name") == 0 ||
            g_strcmp0(name, "visible") == 0)
                update_row(GTK_WIDGET(user_data));
}

static GtkWidget *
create_row(GtkStackPage *page)
{
        GtkWidget *row, *box, *icon, *label;

        row = gtk_list_box_row_new();
        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start(box, 12);
        gtk_widget_set_margin_end(box, 12);
        gtk_widget_set_margin_top(box, 7);
        gtk_widget_set_margin_bottom(box, 7);

        icon = gtk_image_new_from_icon_name("image-missing");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);

        label = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);

        gtk_box_append(GTK_BOX(box), icon);
        gtk_box_append(GTK_BOX(box), label);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

        g_object_set_data_full(G_OBJECT(row), "page",
                               g_object_ref(page), g_object_unref);
        g_signal_connect_object(page, "notify",
                                G_CALLBACK(page_notify_cb), row, 0);
        update_row(row);
        return row;
}

static void
sync_selection(Sidebar *self)
{
        GtkWidget *visible, *row;

        visible = gtk_stack_get_visible_child(self->stack);
        for (row = gtk_widget_get_first_child(GTK_WIDGET(self->list));
             row != NULL; row = gtk_widget_get_next_sibling(row)) {
                GtkStackPage *page;

                if (!GTK_IS_LIST_BOX_ROW(row))
                        continue;
                page = g_object_get_data(G_OBJECT(row), "page");
                if (page != NULL &&
                    gtk_stack_page_get_child(page) == visible) {
                        gtk_list_box_select_row(self->list,
                                                GTK_LIST_BOX_ROW(row));
                        return;
                }
        }
        gtk_list_box_select_row(self->list, NULL);
}

static void
visible_child_cb(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
        Sidebar *self = user_data;

        (void)obj;
        (void)pspec;
        self->syncing = TRUE;
        sync_selection(self);
        self->syncing = FALSE;
}

static void
row_selected_cb(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
        Sidebar *self = user_data;
        GtkStackPage *page;

        (void)list;
        if (self->syncing || row == NULL)
                return;
        page = g_object_get_data(G_OBJECT(row), "page");
        if (page != NULL)
                gtk_stack_set_visible_child(
                        self->stack, gtk_stack_page_get_child(page));
}

static void
rebuild_rows(Sidebar *self)
{
        GtkSelectionModel *model;
        guint i, n;

        self->syncing = TRUE;
        while (TRUE) {
                GtkWidget *child =
                        gtk_widget_get_first_child(GTK_WIDGET(self->list));

                if (!GTK_IS_LIST_BOX_ROW(child))
                        break;
                gtk_list_box_remove(self->list, child);
        }

        model = gtk_stack_get_pages(self->stack);
        n = g_list_model_get_n_items(G_LIST_MODEL(model));
        for (i = 0; i < n; i++) {
                GtkStackPage *page =
                        g_list_model_get_item(G_LIST_MODEL(model), i);

                gtk_list_box_append(self->list, create_row(page));
                g_object_unref(page);
        }
        sync_selection(self);
        self->syncing = FALSE;
}

static void
items_changed_cb(GListModel *model, guint position, guint removed,
                 guint added, gpointer user_data)
{
        Sidebar *self = user_data;

        (void)model;
        (void)position;
        (void)removed;
        (void)added;
        rebuild_rows(self);
}

GtkWidget *
sidebar_new(void)
{
        GtkWidget *list;
        Sidebar *self;

        list = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
                                        GTK_SELECTION_SINGLE);
        gtk_widget_add_css_class(list, "navigation-sidebar");

        self = g_new0(Sidebar, 1);
        self->list = GTK_LIST_BOX(list);
        g_object_set_data_full(G_OBJECT(list), "ctx", self, ctx_free);

        g_signal_connect(list, "row-selected",
                         G_CALLBACK(row_selected_cb), self);

        return list;
}

void
sidebar_set_stack(GtkWidget *widget, GtkStack *stack)
{
        Sidebar *self = g_object_get_data(G_OBJECT(widget), "ctx");

        if (self->stack == stack)
                return;

        if (self->stack != NULL) {
                g_signal_handler_disconnect(
                        gtk_stack_get_pages(self->stack),
                        self->items_changed_id);
                g_signal_handler_disconnect(self->stack,
                                            self->visible_child_id);
                g_object_unref(self->stack);
        }

        self->stack = g_object_ref(stack);
        self->items_changed_id =
                g_signal_connect(gtk_stack_get_pages(stack),
                                 "items-changed",
                                 G_CALLBACK(items_changed_cb), self);
        self->visible_child_id =
                g_signal_connect(stack, "notify::visible-child",
                                  G_CALLBACK(visible_child_cb), self);

        log_debug("sidebar bound to stack");
        rebuild_rows(self);
}
