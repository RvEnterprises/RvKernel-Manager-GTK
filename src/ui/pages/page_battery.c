#include "pages.h"
#include "../components/widgets.h"
#include "../window.h"

#include "../../core/battery.h"
#include "../../util/format.h"
#include "../../util/log.h"
#include "../../util/sysfs.h"

typedef struct {
        GtkWidget *window;
        PowerSupply *ps;
        GtkWidget *status_row;
        GtkWidget *capacity_row;
        GtkLevelBar *level_bar;
        GtkWidget *health_row;
        GtkWidget *voltage_row;
        GtkWidget *temp_row;
        GtkWidget *power_row;
        GtkWidget *cycle_row;
        GtkWidget *energy_row;
        GtkRange  *scale_range;
        GtkWidget *limit_value_label;
} BatteryUi;

typedef struct {
        PowerSupply *ps;
        GtkWidget *row;
} AcRow;

typedef struct {
        GtkWidget      *window;
        PowerSupply **supplies;
        gsize           n_supplies;
        GPtrArray      *uis;
        GPtrArray      *ac_rows;
} BatteryCtx;

static void battery_ctx_free(BatteryCtx *ctx);

static GtkWidget *
make_kv(GtkWidget *card, const gchar *key)
{
        GtkWidget *row = kv_row(key);

        card_add(card, row);
        return row;
}

static void
on_limit_changed(GtkRange *range, gpointer user_data)
{
        GtkLabel *label = GTK_LABEL(user_data);
        gchar *text =
                g_strdup_printf("%d %%", (gint)gtk_range_get_value(range));

        gtk_label_set_text(label, text);
        g_free(text);
}

static void
on_limit_apply(GtkButton *button, gpointer user_data)
{
        BatteryUi *ui = user_data;
        GError *error = NULL;
        gint percent;

        (void)button;
        percent = (gint)gtk_range_get_value(ui->scale_range);

        if (!power_supply_set_charge_limit(ui->ps, percent, &error)) {
                gchar *msg = g_strdup_printf("Failed to set charge limit: %s",
                                             window_error_text(&error));
                log_error("battery page: limit -> %d%% failed: %s",
                          percent, window_error_text(&error));
                window_show_toast(ui->window, msg);
                g_free(msg);
                g_clear_error(&error);
                return;
        }

        {
                gchar *msg = g_strdup_printf(
                        "Charge limit set to %d %% "
                        "(effective after recharge below the new limit)",
                        percent);
                log_info("battery page: charge limit -> %d%%", percent);
                window_show_toast(ui->window, msg);
                g_free(msg);
        }
}

static GtkWidget *
build_charge_limit_row(BatteryUi *ui)
{
        GtkWidget *box, *label, *scale, *apply_btn;

        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(box, "row");

        label = gtk_label_new("Charge limit");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_box_append(GTK_BOX(box), label);

        scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 40, 100,
                                         5);
        gtk_range_set_value(GTK_RANGE(scale),
                            ui->ps->charge_limit > 0 ?
                                    ui->ps->charge_limit : 100.0);
        gtk_widget_set_size_request(scale, 180, -1);
        gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
        gtk_box_append(GTK_BOX(box), scale);
        ui->scale_range = GTK_RANGE(scale);

        ui->limit_value_label = gtk_label_new(NULL);
        on_limit_changed(GTK_RANGE(scale), ui->limit_value_label);
        g_signal_connect(scale, "value-changed",
                         G_CALLBACK(on_limit_changed), ui->limit_value_label);
        gtk_box_append(GTK_BOX(box), ui->limit_value_label);

        apply_btn = gtk_button_new_with_label("Apply");
        g_signal_connect(apply_btn, "clicked", G_CALLBACK(on_limit_apply), ui);
        gtk_box_append(GTK_BOX(box), apply_btn);

        return box;
}

static GtkWidget *
build_battery_card(PowerSupply *ps, BatteryUi *ui)
{
        GtkWidget *card;

        card = card_new(ps->name);

        ui->status_row = make_kv(card, "Status");
        ui->capacity_row = make_kv(card, "Charge");

        ui->level_bar = GTK_LEVEL_BAR(gtk_level_bar_new());
        gtk_level_bar_set_min_value(ui->level_bar, 0.0);
        gtk_level_bar_set_max_value(ui->level_bar, 1.0);
        {
                GtkWidget *wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

                gtk_widget_add_css_class(wrap, "row");
                gtk_widget_add_css_class(GTK_WIDGET(ui->level_bar),
                                         "battery-bar");
                gtk_box_append(GTK_BOX(wrap), GTK_WIDGET(ui->level_bar));
                card_add(card, wrap);
        }

        ui->health_row = make_kv(card, "Health");
        ui->voltage_row = make_kv(card, "Voltage");
        ui->temp_row = make_kv(card, "Temperature");
        ui->power_row = make_kv(card, "Power draw");

        if (ps->has_cycle_count)
                ui->cycle_row = make_kv(card, "Cycle count");

        if (ps->has_charge_design && ps->has_charge_full)
                ui->energy_row = make_kv(card, "Capacity");

        if (ps->has_charge_limit)
                card_add(card, build_charge_limit_row(ui));

        return card;
}

static void
refresh_battery_ui(BatteryUi *ui)
{
        PowerSupply *ps = ui->ps;

        power_supply_refresh(ps);

        kv_set(ui->status_row, "%s", ps->status);

        if (ps->capacity >= 0)
                kv_set(ui->capacity_row, "%d %%", ps->capacity);
        else
                kv_set(ui->capacity_row, "-");

        gtk_level_bar_set_value(
                ui->level_bar,
                CLAMP(ps->capacity < 0 ? 0 : ps->capacity, 0, 100) / 100.0);

        if (ps->has_health && ps->health >= 0)
                kv_set(ui->health_row, "%d%% (%.1f Wh / %.2f Wh design)",
                          ps->health, ps->charge_full_wh,
                          ps->charge_design_wh);
        else
                kv_set(ui->health_row, "-");

        if (ps->has_voltage)
                kv_set(ui->voltage_row, "%.3f V", ps->voltage_v);
        else
                kv_set(ui->voltage_row, "-");

        if (ps->has_temp)
                kv_set(ui->temp_row, "%.1f °C", ps->temp_c);
        else
                kv_set(ui->temp_row, "-");

        if (ps->has_power)
                kv_set(ui->power_row, "%.2f W", ps->power_w);
        else
                kv_set(ui->power_row, "-");

        if (ui->cycle_row != NULL)
                kv_set(ui->cycle_row, "%d cycles", ps->cycle_count);

        if (ui->energy_row != NULL)
                kv_set(ui->energy_row, "%.2f / %.2f Wh",
                          ps->charge_full_wh, ps->charge_design_wh);
}

static void
refresh(GtkWidget *page)
{
        BatteryCtx *ctx = g_object_get_data(G_OBJECT(page), "ctx");

        if (ctx == NULL)
                return;

        for (gsize i = 0; i < ctx->uis->len; i++)
                refresh_battery_ui(g_ptr_array_index(ctx->uis, i));

        for (gsize i = 0; i < ctx->ac_rows->len; i++) {
                AcRow *ar = g_ptr_array_index(ctx->ac_rows, i);

                power_supply_refresh(ar->ps);
                kv_set(ar->row, "%s",
                          ar->ps->online ? "Connected" : "Disconnected");
        }
}

GtkWidget *
page_battery_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title;
        GtkWidget *ac_card = NULL;
        BatteryCtx *ctx;
        gboolean any = FALSE;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "page");

        ctx = g_new0(BatteryCtx, 1);
        ctx->window = window;
        ctx->uis = g_ptr_array_new_with_free_func(g_free);
        ctx->ac_rows = g_ptr_array_new_with_free_func(g_free);
        ctx->supplies = power_supply_list(&ctx->n_supplies);
        log_debug("Battery page: %u supplies", (guint)ctx->n_supplies);

        title = gtk_label_new("Battery");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "title");
        gtk_box_append(GTK_BOX(content), title);

        for (gsize i = 0; i < ctx->n_supplies; i++) {
                PowerSupply *ps = ctx->supplies[i];
                BatteryUi *ui;

                if (ps->kind != PS_BATTERY)
                        continue;

                any = TRUE;
                ui = g_new0(BatteryUi, 1);
                ui->window = window;
                ui->ps = ps;

                gtk_box_append(GTK_BOX(content),
                               build_battery_card(ps, ui));
                g_ptr_array_add(ctx->uis, ui);
        }

        for (gsize i = 0; i < ctx->n_supplies; i++) {
                PowerSupply *ps = ctx->supplies[i];
                AcRow *ar;

                if (ps->kind != PS_AC)
                        continue;

                any = TRUE;
                if (ac_card == NULL)
                        ac_card = card_new("AC adapter");

                ar = g_new0(AcRow, 1);
                ar->ps = ps;
                ar->row = kv_row(ps->name);
                kv_set(ar->row, "%s",
                          ps->online ? "Connected" : "Disconnected");
                card_add(ac_card, ar->row);
                g_ptr_array_add(ctx->ac_rows, ar);
        }

        if (ac_card != NULL)
                gtk_box_append(GTK_BOX(content), ac_card);

        if (!any) {
                GtkWidget *label = gtk_label_new(
                        "No battery or AC adapter detected.");
                gtk_widget_add_css_class(label, "dim-label");
                gtk_box_append(GTK_BOX(content), label);
        }

        scrolled = page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "ctx", ctx,
                               (GDestroyNotify)battery_ctx_free);
        page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
battery_ctx_free(BatteryCtx *ctx)
{
        g_clear_pointer(&ctx->uis, g_ptr_array_unref);
        g_clear_pointer(&ctx->ac_rows, g_ptr_array_unref);
        if (ctx->supplies != NULL)
                power_supply_list_free(ctx->supplies, ctx->n_supplies);
        g_free(ctx);
}
