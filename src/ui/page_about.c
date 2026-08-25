#include "pages.h"
#include "widgets.h"
#include "window.h"

#include "../core/system_info.h"
#include "../util/sysfs.h"

typedef struct {
        GtkWidget *root_row;
        RvSystemInfo *info;
} AboutCtx;

static void about_ctx_free(AboutCtx *ctx);

static void
refresh(GtkWidget *page)
{
        AboutCtx *ctx = g_object_get_data(G_OBJECT(page), "rv-ctx");

        if (ctx == NULL)
                return;

        if (rv_is_root())
                rv_kv_set(ctx->root_row, "%s",
                          "Yes — kernel parameters are writable");
        else
                rv_kv_set(ctx->root_row, "%s", "No — read-only mode");
}

GtkWidget *
rv_page_about_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title;
        GtkWidget *card, *row;
        AboutCtx *ctx;
        gchar *desc;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "rv-page");

        ctx = g_new0(AboutCtx, 1);
        ctx->info = NULL;
        ctx->root_row = NULL;

        title = gtk_label_new("About");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "rv-title");
        gtk_box_append(GTK_BOX(content), title);

        card = rv_card_new(RV_APP_NAME);

        row = rv_kv_row("Version");
        rv_kv_set(row, "%s", RV_VERSION);
        rv_card_add(card, row);

        desc = g_strdup_printf(
                "Desktop port of the Android app: monitor and tune CPU "
                "governors and frequencies, GPU devfreq devices, battery "
                "charge limits, memory tunables and ZRAM.");
        row = rv_kv_row("Description");
        {
                GtkLabel *value =
                        GTK_LABEL(g_object_get_data(G_OBJECT(row),
                                                    "rv-value-label"));
                gtk_label_set_wrap(value, TRUE);
                gtk_label_set_xalign(value, 1.0f);
        }
        rv_kv_set(row, "%s", desc);
        g_free(desc);
        rv_card_add(card, row);

        row = rv_kv_row("Developer");
        rv_kv_set(row, "%s", "Rve");
        rv_card_add(card, row);

        row = rv_kv_row("Organization");
        rv_kv_set(row, "%s", "RvEnterprises");
        rv_card_add(card, row);

        row = rv_kv_row("Project");
        rv_kv_set(row, "github.com/RvEnterprises/RvKernel-Manager-GTK");
        rv_card_add(card, row);

        row = rv_kv_row("Original project");
        rv_kv_set(row, "github.com/Rve27/RvKernel-Manager");
        rv_card_add(card, row);

        row = rv_kv_row("License");
        rv_kv_set(row, "GNU GPL v3 or later");
        rv_card_add(card, row);

        ctx->root_row = rv_kv_row("Root access");
        rv_card_add(card, ctx->root_row);

        gtk_box_append(GTK_BOX(content), card);

        scrolled = rv_page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "rv-ctx", ctx,
                               (GDestroyNotify)about_ctx_free);
        rv_page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
about_ctx_free(AboutCtx *ctx)
{
        g_free(ctx);
}
