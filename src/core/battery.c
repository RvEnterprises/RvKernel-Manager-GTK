#include "battery.h"

#include "../util/sysfs.h"

#include <gio/gio.h>

static const char *const PS_BASE = "/sys/class/power_supply";

static const char *const CHARGE_LIMIT_FILES[] = {
        "charge_control_end_threshold",
        "charge_stop_threshold",
        "max_charge_percent",
};

static RvPowerSupplyKind
kind_from_type(const gchar *type)
{
        if (type == NULL)
                return RV_PS_OTHER;
        if (g_ascii_strcasecmp(type, "Battery") == 0 ||
            g_ascii_strcasecmp(type, "UPS") == 0)
                return RV_PS_BATTERY;
        if (g_ascii_strcasecmp(type, "Mains") == 0 ||
            g_ascii_strcasecmp(type, "USB") == 0 ||
            g_ascii_strcasecmp(type, "Wireless") == 0)
                return RV_PS_AC;
        return RV_PS_OTHER;
}

static RvPowerSupply *
rv_power_supply_new(const gchar *path, const gchar *name)
{
        RvPowerSupply *ps = g_new0(RvPowerSupply, 1);
        gchar *tmp;

        ps->path = g_strdup(path);
        ps->name = g_strdup(name);
        ps->capacity = -1;
        ps->health = -1;
        ps->cycle_count = -1;
        ps->charge_limit = -1;

        tmp = g_build_filename(path, "type", NULL);
        ps->kind = kind_from_type(rv_read_first_line(tmp));
        g_free(tmp);

        rv_power_supply_refresh(ps);

        return ps;
}

void
rv_power_supply_refresh(RvPowerSupply *ps)
{
        gchar *tmp;
        gint64 value;

        if (ps == NULL)
                return;

        tmp = g_build_filename(ps->path, "status", NULL);
        g_free(ps->status);
        ps->status = rv_read_first_line(tmp);
        if (ps->status == NULL)
                ps->status = g_strdup("Unknown");
        g_free(tmp);

        if (ps->kind != RV_PS_BATTERY) {
                tmp = g_build_filename(ps->path, "online", NULL);
                ps->online = rv_path_exists(tmp) &&
                             rv_read_int64(tmp, &value) && value > 0;
                g_free(tmp);
                return;
        }

        tmp = g_build_filename(ps->path, "capacity", NULL);
        if (!rv_read_int64(tmp, &value))
                value = -1;
        ps->capacity = (gint)value;
        g_free(tmp);

        tmp = g_build_filename(ps->path, "temp", NULL);
        ps->has_temp = rv_read_double(tmp, &ps->temp_c);
        if (ps->has_temp)
                ps->temp_c /= 10.0;
        g_free(tmp);

        tmp = g_build_filename(ps->path, "voltage_now", NULL);
        ps->has_voltage = rv_read_double(tmp, &ps->voltage_v);
        if (ps->has_voltage)
                ps->voltage_v /= 1000000.0;
        g_free(tmp);

        tmp = g_build_filename(ps->path, "power_now", NULL);
        ps->has_power = rv_read_double(tmp, &ps->power_w);
        if (ps->has_power)
                ps->power_w /= 1000000.0;
        g_free(tmp);

        if (!ps->has_power) {
                gchar *cur = g_build_filename(ps->path, "current_now", NULL);
                ps->has_current = rv_read_double(cur, &ps->current_a);
                if (ps->has_current) {
                        ps->current_a /= 1000000.0;
                        if (ps->has_voltage) {
                                ps->power_w = ps->current_a * ps->voltage_v;
                                ps->has_power = TRUE;
                        }
                }
                g_free(cur);
        }

        tmp = g_build_filename(ps->path, "cycle_count", NULL);
        if (rv_read_int64(tmp, &value) && value > 0) {
                ps->has_cycle_count = TRUE;
                ps->cycle_count = (gint)value;
        } else {
                ps->has_cycle_count = FALSE;
        }
        g_free(tmp);

        tmp = g_build_filename(ps->path, "charge_full", NULL);
        ps->has_charge_full = rv_read_double(tmp, &ps->charge_full_wh);
        if (ps->has_charge_full)
                ps->charge_full_wh /= 1000000.0;
        g_free(tmp);

        tmp = g_build_filename(ps->path, "energy_full", NULL);
        if (!ps->has_charge_full &&
            rv_read_double(tmp, &ps->charge_full_wh)) {
                ps->has_charge_full = TRUE;
                ps->charge_full_wh /= 1000000.0;
        }
        g_free(tmp);

        tmp = g_build_filename(ps->path, "charge_full_design", NULL);
        ps->has_charge_design = rv_read_double(tmp, &ps->charge_design_wh);
        if (ps->has_charge_design)
                ps->charge_design_wh /= 1000000.0;
        g_free(tmp);

        tmp = g_build_filename(ps->path, "energy_full_design", NULL);
        if (!ps->has_charge_design &&
            rv_read_double(tmp, &ps->charge_design_wh)) {
                ps->has_charge_design = TRUE;
                ps->charge_design_wh /= 1000000.0;
        }
        g_free(tmp);

        if (ps->has_charge_full && ps->has_charge_design &&
            ps->charge_design_wh > 0) {
                ps->health = (gint)((ps->charge_full_wh /
                                     ps->charge_design_wh) * 100.0 + 0.5);
                ps->has_health = TRUE;
        }

        for (gsize i = 0; i < G_N_ELEMENTS(CHARGE_LIMIT_FILES); i++) {
                tmp = g_build_filename(ps->path, CHARGE_LIMIT_FILES[i], NULL);
                if (rv_path_exists(tmp)) {
                        gint64 v;
                        if (rv_read_int64(tmp, &v)) {
                                g_free(ps->charge_limit_path);
                                ps->charge_limit_path = tmp;
                                ps->has_charge_limit = TRUE;
                                ps->charge_limit = (gint)v;
                                return;
                        }
                }
                g_free(tmp);
        }

        ps->has_charge_limit = FALSE;
}

RvPowerSupply **
rv_power_supply_list(gsize *count)
{
        GPtrArray *result;
        gsize n_entries = 0;
        gchar **entries;

        result = g_ptr_array_new();
        entries = rv_list_dir(PS_BASE, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *path = g_build_filename(PS_BASE, entries[i], NULL);
                g_ptr_array_add(result,
                                rv_power_supply_new(path, entries[i]));
                g_free(path);
        }
        g_strfreev(entries);

        if (count != NULL)
                *count = result->len;
        g_ptr_array_add(result, NULL);
        return (RvPowerSupply **)g_ptr_array_free(result, FALSE);
}

void
rv_power_supply_free(RvPowerSupply *ps)
{
        if (ps == NULL)
                return;
        g_free(ps->path);
        g_free(ps->name);
        g_free(ps->status);
        g_free(ps->charge_limit_path);
        g_free(ps);
}

void
rv_power_supply_list_free(RvPowerSupply **list, gsize count)
{
        if (list == NULL)
                return;
        for (gsize i = 0; i < count; i++)
                rv_power_supply_free(list[i]);
        g_free(list);
}

gboolean
rv_power_supply_set_charge_limit(RvPowerSupply *ps, gint percent,
                                 GError **error)
{
        if (ps->charge_limit_path == NULL) {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "Charge limit control not supported");
                return FALSE;
        }

        return rv_write_int64(ps->charge_limit_path, percent, error);
}
