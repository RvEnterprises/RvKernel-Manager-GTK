#include "thermal.h"

#include "../util/sysfs.h"

static const char *const THERMAL_BASE = "/sys/class/thermal";

RvThermalZone *
rv_thermal_zone_new(const gchar *path, gint index)
{
        RvThermalZone *zone = g_new0(RvThermalZone, 1);
        gchar *tmp;

        zone->path = g_strdup(path);
        zone->index = index;

        tmp = g_build_filename(path, "type", NULL);
        zone->type = rv_read_first_line(tmp);
        g_free(tmp);

        tmp = g_build_filename(path, "mode", NULL);
        zone->has_mode = rv_path_exists(tmp);
        if (zone->has_mode) {
                zone->available_modes = g_new0(gchar *, 3);
                zone->available_modes[0] = g_strdup("enabled");
                zone->available_modes[1] = g_strdup("disabled");
                zone->mode = rv_read_first_line(tmp);
        }
        g_free(tmp);

        rv_thermal_zone_refresh(zone);
        return zone;
}

void
rv_thermal_zone_refresh(RvThermalZone *zone)
{
        gchar *tmp;
        gdouble temp;

        if (zone == NULL)
                return;

        tmp = g_build_filename(zone->path, "temp", NULL);
        zone->has_temp = rv_read_double(tmp, &temp);
        if (zone->has_temp)
                zone->temp_c = temp / 1000.0;
        g_free(tmp);

        if (zone->has_mode) {
                tmp = g_build_filename(zone->path, "mode", NULL);
                g_free(zone->mode);
                zone->mode = rv_read_first_line(tmp);
                g_free(tmp);
        }
}

RvThermalZone **
rv_thermal_zones(gsize *count)
{
        GPtrArray *result;
        gsize n_entries = 0;
        gchar **entries;
        gint index = 0;

        result = g_ptr_array_new();
        entries = rv_list_dir(THERMAL_BASE, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *full;

                if (!g_str_has_prefix(entries[i], "thermal_zone"))
                        continue;

                full = g_build_filename(THERMAL_BASE, entries[i], NULL);
                if (rv_is_dir(full))
                        g_ptr_array_add(result,
                                        rv_thermal_zone_new(full, index++));
                g_free(full);
        }
        g_strfreev(entries);

        if (count != NULL)
                *count = result->len;
        g_ptr_array_add(result, NULL);
        return (RvThermalZone **)g_ptr_array_free(result, FALSE);
}

gboolean
rv_thermal_zone_set_mode(RvThermalZone *zone, const gchar *mode,
                         GError **error)
{
        gchar *tmp = g_build_filename(zone->path, "mode", NULL);
        gboolean ok = rv_write_string(tmp, mode, error);
        g_free(tmp);
        return ok;
}

void
rv_thermal_zone_free(RvThermalZone *zone)
{
        if (zone == NULL)
                return;
        g_free(zone->path);
        g_free(zone->type);
        g_free(zone->mode);
        g_strfreev(zone->available_modes);
        g_free(zone);
}

void
rv_thermal_zones_free(RvThermalZone **zones, gsize count)
{
        if (zones == NULL)
                return;
        for (gsize i = 0; i < count; i++)
                rv_thermal_zone_free(zones[i]);
        g_free(zones);
}

RvCoolingDevice *
rv_cooling_device_new(const gchar *path, gint index)
{
        RvCoolingDevice *dev = g_new0(RvCoolingDevice, 1);
        gchar *tmp;

        dev->path = g_strdup(path);
        dev->index = index;

        tmp = g_build_filename(path, "type", NULL);
        dev->type = rv_read_first_line(tmp);
        g_free(tmp);

        rv_cooling_device_refresh(dev);
        return dev;
}

void
rv_cooling_device_refresh(RvCoolingDevice *dev)
{
        gchar *tmp;
        gint64 v;

        if (dev == NULL)
                return;

        tmp = g_build_filename(dev->path, "cur_state", NULL);
        dev->has_states = rv_read_int64(tmp, &v);
        if (dev->has_states)
                dev->cur_state = (gint)v;
        g_free(tmp);

        if (!dev->has_states)
                return;

        tmp = g_build_filename(dev->path, "max_state", NULL);
        if (rv_read_int64(tmp, &v))
                dev->max_state = (gint)v;
        else
                dev->has_states = FALSE;
        g_free(tmp);
}

RvCoolingDevice **
rv_cooling_devices(gsize *count)
{
        GPtrArray *result;
        gsize n_entries = 0;
        gchar **entries;
        gint index = 0;

        result = g_ptr_array_new();
        entries = rv_list_dir(THERMAL_BASE, &n_entries);
        for (gsize i = 0; i < n_entries; i++) {
                gchar *full;

                if (!g_str_has_prefix(entries[i], "cooling_device"))
                        continue;

                full = g_build_filename(THERMAL_BASE, entries[i], NULL);
                if (rv_is_dir(full))
                        g_ptr_array_add(result,
                                        rv_cooling_device_new(full, index++));
                g_free(full);
        }
        g_strfreev(entries);

        if (count != NULL)
                *count = result->len;
        g_ptr_array_add(result, NULL);
        return (RvCoolingDevice **)g_ptr_array_free(result, FALSE);
}

void
rv_cooling_device_free(RvCoolingDevice *dev)
{
        if (dev == NULL)
                return;
        g_free(dev->path);
        g_free(dev->type);
        g_free(dev);
}

void
rv_cooling_devices_free(RvCoolingDevice **devices, gsize count)
{
        if (devices == NULL)
                return;
        for (gsize i = 0; i < count; i++)
                rv_cooling_device_free(devices[i]);
        g_free(devices);
}
