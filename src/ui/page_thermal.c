#include "pages.h"
#include "widgets.h"
#include "window.h"

#include "../core/thermal.h"
#include "../util/format.h"
#include "../util/sysfs.h"

typedef struct {
        GtkWidget     *window;
        RvThermalZone *zone;
        GtkWidget     *temp_row;
} ZoneUi;

typedef struct {
        RvCoolingDevice *device;
        GtkWidget       *state_row;
} CoolingUi;

typedef struct {
        RvThermalZone   **zones;
        gsize             n_zones;
        RvCoolingDevice **cooling;
        gsize             n_cooling;
        GPtrArray        *zone_uis;
        GPtrArray        *cooling_uis;
} ThermalCtx;

static void thermal_ctx_free(ThermalCtx *ctx);

static void
show_write_error(GtkWidget *window, GError **error)
{
        gchar *msg = g_strdup_printf("Failed to apply setting: %s",
                                     rv_window_error_text(error));
        rv_window_show_toast(window, msg);
        g_free(msg);
}

static void
on_zone_mode_selected(GtkDropDown *dropdown, GParamSpec *pspec,
                      gpointer user_data)
{
        ZoneUi *ui = user_data;
        GtkWidget *row = g_object_get_data(G_OBJECT(dropdown), "rv-row-ref");
        const gchar *id;
        GError *error = NULL;

        (void)pspec;

        if (row == NULL)
                return;

        id = rv_option_row_active_id(row);
        if (id == NULL)
                return;

        if (!rv_thermal_zone_set_mode(ui->zone, id, &error))
                show_write_error(ui->window, &error);
        g_clear_error(&error);
}

static void
refresh(GtkWidget *page)
{
        ThermalCtx *ctx = g_object_get_data(G_OBJECT(page), "rv-ctx");

        if (ctx == NULL)
                return;

        for (gsize i = 0; i < ctx->zone_uis->len; i++) {
                ZoneUi *ui = g_ptr_array_index(ctx->zone_uis, i);

                rv_thermal_zone_refresh(ui->zone);
                if (ui->zone->has_temp)
                        rv_kv_set(ui->temp_row, "%.1f °C", ui->zone->temp_c);
                else
                        rv_kv_set(ui->temp_row, "-");
        }

        for (gsize i = 0; i < ctx->cooling_uis->len; i++) {
                CoolingUi *ui = g_ptr_array_index(ctx->cooling_uis, i);

                rv_cooling_device_refresh(ui->device);
                if (ui->device->has_states)
                        rv_kv_set(ui->state_row, "%d / %d",
                                  ui->device->cur_state,
                                  ui->device->max_state);
                else
                        rv_kv_set(ui->state_row, "-");
        }
}

GtkWidget *
rv_page_thermal_new(GtkWidget *window)
{
        GtkWidget *scrolled, *content, *title, *card;
        ThermalCtx *ctx;

        content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_add_css_class(content, "rv-page");

        ctx = g_new0(ThermalCtx, 1);
        ctx->zone_uis = g_ptr_array_new_with_free_func(g_free);
        ctx->cooling_uis = g_ptr_array_new_with_free_func(g_free);
        ctx->zones = rv_thermal_zones(&ctx->n_zones);
        ctx->cooling = rv_cooling_devices(&ctx->n_cooling);

        title = gtk_label_new("Thermal");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_add_css_class(title, "rv-title");
        gtk_box_append(GTK_BOX(content), title);

        if (ctx->n_zones == 0 && ctx->n_cooling == 0) {
                GtkWidget *label = gtk_label_new(
                        "No thermal zones or cooling devices exposed by "
                        "the kernel.");
                gtk_widget_add_css_class(label, "dim-label");
                gtk_box_append(GTK_BOX(content), label);
        }

        if (ctx->n_zones > 0) {
                card = rv_card_new("Thermal zones");

                for (gsize i = 0; i < ctx->n_zones; i++) {
                        RvThermalZone *zone = ctx->zones[i];
                        ZoneUi *ui = g_new0(ZoneUi, 1);
                        gchar *label_text;

                        ui->window = window;
                        ui->zone = zone;

                        label_text = g_strdup_printf("%s (%d)", zone->type,
                                                     zone->index);
                        ui->temp_row = rv_kv_row(label_text);
                        rv_card_add(card, ui->temp_row);
                        g_free(label_text);

                        if (zone->has_temp)
                                rv_kv_set(ui->temp_row, "%.1f °C",
                                          zone->temp_c);

                        if (zone->has_mode) {
                                GtkWidget *row =
                                        rv_option_row_new("Mode");

                                rv_option_row_append(row, "enabled",
                                                     "enabled");
                                rv_option_row_append(row, "disabled",
                                                     "disabled");
                                if (zone->mode != NULL &&
                                    zone->mode[0] != '\0' &&
                                    g_strcmp0(zone->mode, "unknown") != 0)
                                        rv_option_row_select_id(
                                                row, zone->mode);

                                g_object_set_data(
                                        G_OBJECT(rv_option_row_dropdown(row)),
                                        "rv-row-ref", row);
                                g_signal_connect(
                                        rv_option_row_dropdown(row),
                                        "notify::selected",
                                        G_CALLBACK(on_zone_mode_selected),
                                        ui);
                                rv_card_add(card, row);
                        }

                        g_ptr_array_add(ctx->zone_uis, ui);
                }

                gtk_box_append(GTK_BOX(content), card);
        }

        if (ctx->n_cooling > 0) {
                card = rv_card_new("Cooling devices");

                for (gsize i = 0; i < ctx->n_cooling; i++) {
                        RvCoolingDevice *dev = ctx->cooling[i];
                        CoolingUi *ui = g_new0(CoolingUi, 1);
                        gchar *label_text;

                        ui->device = dev;

                        label_text = g_strdup_printf("%s (%d)", dev->type,
                                                     dev->index);
                        ui->state_row = rv_kv_row(label_text);
                        rv_kv_set(ui->state_row, dev->has_states ?
                                                          "%d / %d" : "-",
                                  dev->cur_state, dev->max_state);
                        rv_card_add(card, ui->state_row);
                        g_free(label_text);

                        g_ptr_array_add(ctx->cooling_uis, ui);
                }

                gtk_box_append(GTK_BOX(content), card);
        }

        scrolled = rv_page_wrap(content);
        g_object_set_data_full(G_OBJECT(scrolled), "rv-ctx", ctx,
                               (GDestroyNotify)thermal_ctx_free);
        rv_page_set_refresh(scrolled, refresh);

        return scrolled;
}

static void
thermal_ctx_free(ThermalCtx *ctx)
{
        g_clear_pointer(&ctx->zone_uis, g_ptr_array_unref);
        g_clear_pointer(&ctx->cooling_uis, g_ptr_array_unref);
        if (ctx->zones != NULL)
                rv_thermal_zones_free(ctx->zones, ctx->n_zones);
        if (ctx->cooling != NULL)
                rv_cooling_devices_free(ctx->cooling, ctx->n_cooling);
        g_free(ctx);
}
