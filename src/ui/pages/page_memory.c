#include "pages.h"
#include "../components/widgets.h"
#include "../window.h"

#include "../../core/memory.h"
#include "../../util/format.h"
#include "../../util/log.h"
#include "../../util/sysfs.h"

typedef struct {
        GtkWidget *window;
        Zram    *zram;
        GtkWidget *disksize_row;
        GtkWidget *usage_row;
        GtkWidget *ratio_row;
} ZramUi;

typedef struct {
        Zram   **zrams;
        gsize      n_zrams;
        GPtrArray *zram_uis;
} MemCtx;

static void mem_ctx_free(MemCtx *ctx);

static GtkWidget *
make_kv(GtkWidget *card, const gchar *key)
{
        GtkWidget *row = kv_row(key);

        card_add(card, row);
        return row;
}

static void
show_write_error(GtkWidget *window, GError **error)
{
        gchar *msg;

        if (window == NULL)
                return;

        msg = g_strdup_printf("Failed to apply setting: %s",
                              window_error_text(error));
        log_error("memory page: apply failed: %s",
                  window_error_text(error));
        window_show_toast(window, msg);
        g_free(msg);
}

static void
on_vm_tunable_selected(GtkSpinButton *spin, gpointer user_data)
{
        const VmTunable *tunable = user_data;
        GtkWidget *window = g_object_get_data(G_OBJECT(spin), "window");
        GError *error = NULL;
        gint value = (gint)gtk_spin_button_get_value(spin);

        if (!vm_tunable_set(tunable, value, &error))
                show_write_error(window, &error);
        g_clear_error(&error);
}

static void
on_zram_algo_selected(GtkDropDown *dropdown, GParamSpec *pspec,
                      gpointer user_data)
{
        ZramUi *ui = user_data;
        GtkWidget *row = g_object_get_data(G_OBJECT(dropdown), "row-ref");
        const gchar *id;
        GError *error = NULL;

        (void)pspec;
        if (row == NULL) {
                id = NULL;
                return;
        }

        id = option_row_active_id(row);
        if (id == NULL)
                return;

        if (!zram_set_algo(ui->zram, id, &error)) {
                show_write_error(ui->window, &error);
        } else {
                gchar *msg = g_strdup_printf(
                        "zram algorithm set to %s "
                        "(takes effect on next device reset)", id);
                log_info("memory page: zram algo -> %s", id);
                window_show_toast(ui->window, msg);
                g_free(msg);
        }
        g_clear_error(&error);
}

static void
on_tcp_selected(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
        ZramUi *ui_ctx = user_data;
        GtkWidget *row = g_object_get_data(G_OBJECT(dropdown), "row-ref");
        const gchar *id;
        GError *error = NULL;

        (void)pspec;
        (void)ui_ctx;

        if (row == NULL)
                return;

        id = option_row_active_id(row);
        if (id == NULL)
                return;

        if (!tcp_cc_set(id, &error))
                show_write_error(NULL, &error);
        g_clear_error(&error);
}

static GtkWidget *
build_vm_card(GtkWidget *window)
{
        GtkWidget *card = NULL;

        for (gsize i = 0; i < VM_TUNABLES_COUNT; i++) {
                const VmTunable *t = &VM_TUNABLES[i];
                gint current = 0;
                GtkWidget *row;
                GtkSpinButton *spin;

                if (!vm_tunable_get(t, &current))
                        continue;

                if (card == NULL)
                        card = card_new("Kernel memory parameters");

                row = spin_row(t->label, t->min, t->max, t->step, current);
                spin = spin_row_spin(row);
                g_object_set_data(G_OBJECT(spin), "window", window);
                g_signal_connect_after(spin, "value-changed",
                                       G_CALLBACK(on_vm_tunable_selected),
                                       (gpointer)t);
                card_add(card, row);
        }

        return card;
}

static void
tag_dropdown(GtkWidget *row)
{
        g_object_set_data(G_OBJECT(option_row_dropdown(row)), "row-ref",
                          row);
}

static GtkWidget *
build_zram_card(GtkWidget *window, Zram *zram, ZramUi *ui)
{
        GtkWidget *card;

        ui->window = window;
        ui->zram = zram;
        card = card_new(zram->name);

        ui->disksize_row = make_kv(card, "Disk size");

        if (zram->algos != NULL && zram->algos[0] != NULL) {
                GtkWidget *row = option_row_new("Algorithm");

                for (gsize i = 0; zram->algos[i] != NULL; i++)
                        option_row_append(row, zram->algos[i],
                                             zram->algos[i]);
                if (zram->algo != NULL)
                        option_row_select_id(row, zram->algo);

                tag_dropdown(row);
                g_signal_connect(option_row_dropdown(row),
                                 "notify::selected",
                                 G_CALLBACK(on_zram_algo_selected), ui);
                card_add(card, row);
        } else {
                GtkWidget *row = make_kv(card, "Algorithm");
                kv_set(row, "%s", zram->algo != NULL ? zram->algo : "-");
        }

        ui->usage_row = make_kv(card, "Used");
        ui->ratio_row = make_kv(card, "Compression ratio");

        return card;
}

static GtkWidget *
build_tcp_card(GtkWidget *window)
{
        gsize n_cc = 0;
        gchar **cc_list = tcp_cc_list(&n_cc);
        gchar *current = cc_list != NULL ? tcp_cc_current() : NULL;
        GtkWidget *card = NULL;

        if (n_cc > 0 && current != NULL) {
                GtkWidget *row = option_row_new(
                        "TCP congestion control");

                for (gsize i = 0; cc_list[i] != NULL; i++)
                        option_row_append(row, cc_list[i], cc_list[i]);
                option_row_select_id(row, current);

                tag_dropdown(row);
                g_signal_connect(option_row_dropdown(row),
                                 "notify::selected",
                                 G_CALLBACK(on_tcp_selected), NULL);
                card = card_new("Network");
                card_add(card, row);
                (void)window;
        }

        g_strfreev(cc_list);
        g_free(current);
        return card;
}

static void
refresh(GtkWidget *page)
{
        MemCtx *ctx = g_object_get_data(G_OBJECT(page), "ctx");

        if (ctx == NULL)
                return;

        for (gsize i = 0; i < ctx->zram_uis->len; i++) {
                ZramUi *ui = g_ptr_array_index(ctx->zram_uis, i);
                Zram *z = ui->zram;
                gdouble ratio;

                zram_refresh(z);

                kv_set(ui->disksize_row, "%s",
                          z->disksize_str != NULL ? z->disksize_str : "-");

                if (z->has_stats) {
                        gchar *used = format_bytes(z->used_bytes);
                        gchar *orig = format_bytes(z->orig_bytes);
                        gchar *compr = format_bytes(z->compr_bytes);

                        kv_set(ui->usage_row, "%s (data %s → %s)", used,
                                  orig, compr);
                        g_free(used);
                        g_free(orig);
                        g_free(compr);
                } else {
                        kv_set(ui->usage_row, "-");
                }

                ratio = 0;
                if (z->has_stats && z->compr_bytes > 0)
                        ratio = (gdouble)z->orig_bytes /
                                (gdouble)z->compr_bytes;
                kv_set(ui->ratio_row, ratio > 0 ? "%.2fx" : "-", ratio);
        }
}

GtkWidget *
page_memory_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title, *card;
        MemCtx *ctx;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "page");

        ctx = g_new0(MemCtx, 1);
        ctx->zram_uis = g_ptr_array_new_with_free_func(g_free);
        ctx->zrams = zram_list(&ctx->n_zrams);
        log_debug("Memory page: %u zram devices", (guint)ctx->n_zrams);

        title = gtk_label_new("Memory");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "title");
        gtk_box_append(GTK_BOX(content), title);

        card = build_vm_card(window);
        if (card != NULL)
                gtk_box_append(GTK_BOX(content), card);

        for (gsize i = 0; i < ctx->n_zrams; i++) {
                ZramUi *ui = g_new0(ZramUi, 1);

                gtk_box_append(GTK_BOX(content),
                               build_zram_card(window, ctx->zrams[i], ui));
                g_ptr_array_add(ctx->zram_uis, ui);
        }

        card = build_tcp_card(window);
        if (card != NULL)
                gtk_box_append(GTK_BOX(content), card);

        scrolled = page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "ctx", ctx,
                               (GDestroyNotify)mem_ctx_free);
        page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
mem_ctx_free(MemCtx *ctx)
{
        g_clear_pointer(&ctx->zram_uis, g_ptr_array_unref);
        if (ctx->zrams != NULL)
                zram_list_free(ctx->zrams, ctx->n_zrams);
        g_free(ctx);
}
