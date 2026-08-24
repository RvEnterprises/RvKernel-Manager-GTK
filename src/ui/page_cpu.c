#include "pages.h"
#include "widgets.h"
#include "window.h"

#include "../core/cpu.h"
#include "../util/format.h"
#include "../util/sysfs.h"

typedef struct {
        GtkWidget   *window;
        RvCpuPolicy *policy;
        gchar       *cur_freq_path;

        GtkWidget *freq_row;
        GtkWidget *gov_row;
        GtkWidget *epp_row;
        GtkWidget *min_row;
        GtkWidget *max_row;
} PolicyUi;

typedef struct {
        RvCpuPolicy **policies;
        gsize         n_policies;
        GPtrArray    *uis;
} CpuCtx;

static void cpu_ctx_free(CpuCtx *ctx);
static void policy_ui_free(gpointer data);

static void
show_write_error(GtkWidget *window, GError **error)
{
        gchar *msg = g_strdup_printf("Failed to apply setting: %s",
                                     rv_window_error_text(error));
        rv_window_show_toast(window, msg);
        g_free(msg);
}

static void
on_governor_selected(GtkDropDown *dropdown, GParamSpec *pspec,
                     gpointer user_data)
{
        PolicyUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->gov_row);
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        if (!rv_cpu_set_governor(ui->policy, id, &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
on_epp_selected(GtkDropDown *dropdown, GParamSpec *pspec,
                gpointer user_data)
{
        PolicyUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->epp_row);
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        if (!rv_cpu_set_epp(ui->policy, id, &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
on_min_selected(GtkDropDown *dropdown, GParamSpec *pspec,
                gpointer user_data)
{
        PolicyUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->min_row);
        gint64 khz;
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        khz = g_ascii_strtoll(id, NULL, 10);
        if (khz <= 0)
                return;

        if (!rv_cpu_set_min_freq(ui->policy, khz, &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
on_max_selected(GtkDropDown *dropdown, GParamSpec *pspec,
                gpointer user_data)
{
        PolicyUi *ui = user_data;
        const gchar *id = rv_option_row_active_id(ui->max_row);
        const gchar *min_id = rv_option_row_active_id(ui->min_row);
        gint64 khz, min_khz;
        GError *error = NULL;

        (void)dropdown;
        (void)pspec;

        if (id == NULL)
                return;

        khz = g_ascii_strtoll(id, NULL, 10);
        min_khz = min_id != NULL ? g_ascii_strtoll(min_id, NULL, 10) : -1;

        if (khz <= 0)
                return;

        if (min_khz > 0 && khz < min_khz) {
            gchar *msg =
                    g_strdup_printf("Maximum frequency cannot be below the "
                                    "selected minimum (%s)",
                                    rv_format_khz(min_khz));
            rv_window_show_toast(ui->window, msg);
            g_free(msg);

            if (ui->policy->max_freq_khz != NULL)
                    rv_option_row_select_id(ui->max_row,
                                            ui->policy->max_freq_khz);
            return;
    }

    if (!rv_cpu_set_max_freq(ui->policy, khz, &error))
            show_write_error(ui->window, &error);
    g_clear_error(&error);
}

static GtkWidget *
build_policy_card(RvCpuPolicy *policy, PolicyUi *ui)
{
        GtkWidget *card, *row;
        gchar *card_title;
        gchar *header;

        card_title = g_strdup(policy->cpus_desc);
        for (gsize i = 0; card_title[i] != '\0'; i++)
                if (card_title[i] == '/')
                        card_title[i] = ' ';

        header = g_strdup_printf("Cluster %s", card_title);
        card = rv_card_new(header);
        g_free(header);
        g_free(card_title);

        ui->freq_row = rv_kv_row("Current frequency");
        ui->cur_freq_path = g_build_filename(policy->path,
                                             "scaling_cur_freq", NULL);
        rv_card_add(card, ui->freq_row);

        if (policy->governors[0] != NULL) {
                row = rv_option_row_new("Governor");
                for (gsize i = 0; policy->governors[i] != NULL; i++)
                        rv_option_row_append(row, policy->governors[i],
                                             policy->governors[i]);
                if (policy->governor != NULL)
                        rv_option_row_select_id(row, policy->governor);

                g_signal_connect(rv_option_row_dropdown(row),
                                 "notify::selected",
                                 G_CALLBACK(on_governor_selected), ui);
                rv_card_add(card, row);
                ui->gov_row = row;
        } else {
                row = rv_kv_row("Governor");
                rv_card_add(card, row);
                rv_kv_set(row, "%s", policy->governor != NULL ?
                                             policy->governor : "-");
        }

        if (policy->has_epp && policy->epps[0] != NULL) {
                row = rv_option_row_new("Energy performance");
                for (gsize i = 0; policy->epps[i] != NULL; i++)
                        rv_option_row_append(row, policy->epps[i],
                                             policy->epps[i]);
                if (policy->epp != NULL)
                        rv_option_row_select_id(row, policy->epp);

                g_signal_connect(rv_option_row_dropdown(row),
                                 "notify::selected",
                                 G_CALLBACK(on_epp_selected), ui);
                rv_card_add(card, row);
                ui->epp_row = row;
        }

        if (policy->freqs_khz != NULL && policy->n_freqs >= 2) {
                GtkWidget *min_row = rv_option_row_new("Minimum frequency");
                GtkWidget *max_row = rv_option_row_new("Maximum frequency");

                for (gsize i = 0; i < policy->n_freqs; i++) {
                        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT,
                                                    policy->freqs_khz[i]);
                        gchar *label = rv_format_khz(policy->freqs_khz[i]);

                        rv_option_row_append(min_row, id, label);
                        rv_option_row_append(max_row, id, label);
                        g_free(id);
                        g_free(label);
                }

                if (policy->min_freq_khz != NULL)
                        rv_option_row_select_id(min_row,
                                                policy->min_freq_khz);
                if (policy->max_freq_khz != NULL)
                        rv_option_row_select_id(max_row,
                                                policy->max_freq_khz);

                g_signal_connect(rv_option_row_dropdown(min_row),
                                 "notify::selected",
                                 G_CALLBACK(on_min_selected), ui);
                g_signal_connect(rv_option_row_dropdown(max_row),
                                 "notify::selected",
                                 G_CALLBACK(on_max_selected), ui);

                rv_card_add(card, min_row);
                rv_card_add(card, max_row);
                ui->min_row = min_row;
                ui->max_row = max_row;
        }

        return card;
}

static void
refresh(GtkWidget *page)
{
        CpuCtx *ctx = g_object_get_data(G_OBJECT(page), "rv-ctx");

        if (ctx == NULL)
                return;

        for (gsize i = 0; i < ctx->uis->len; i++) {
                PolicyUi *ui = g_ptr_array_index(ctx->uis, i);
                gint64 cur;

                if (rv_read_int64(ui->cur_freq_path, &cur)) {
                        gchar *s = rv_format_khz(cur);
                        rv_kv_set(ui->freq_row, "%s", s);
                        g_free(s);
                } else {
                        rv_kv_set(ui->freq_row, "-");
                }
        }
}

GtkWidget *
rv_page_cpu_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title;
        CpuCtx *ctx;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "rv-page");

        ctx = g_new0(CpuCtx, 1);
        ctx->uis = g_ptr_array_new_with_free_func(policy_ui_free);
        ctx->policies = rv_cpu_policies(&ctx->n_policies);

        title = gtk_label_new("CPU");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "rv-title");
        gtk_box_append(GTK_BOX(content), title);

        if (ctx->n_policies == 0) {
                GtkWidget *label = gtk_label_new(
                        "No cpufreq policies found on this system.");
                gtk_widget_add_css_class(label, "dim-label");
                gtk_box_append(GTK_BOX(content), label);
        }

        for (gsize i = 0; i < ctx->n_policies; i++) {
                PolicyUi *ui = g_new0(PolicyUi, 1);

                ui->window = window;
                ui->policy = ctx->policies[i];

                gtk_box_append(GTK_BOX(content),
                               build_policy_card(ctx->policies[i], ui));
                g_ptr_array_add(ctx->uis, ui);
        }

        scrolled = rv_page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "rv-ctx", ctx,
                               (GDestroyNotify)cpu_ctx_free);
        rv_page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
cpu_ctx_free(CpuCtx *ctx)
{
        g_clear_pointer(&ctx->uis, g_ptr_array_unref);
        if (ctx->policies != NULL)
                rv_cpu_policies_free(ctx->policies, ctx->n_policies);
        g_free(ctx);
}

static void
policy_ui_free(gpointer data)
{
        PolicyUi *ui = data;

        g_free(ui->cur_freq_path);
        g_free(ui);
}
