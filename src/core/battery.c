#include "battery.h"

#include "../util/log.h"
#include "../util/sysfs.h"

#include <gio/gio.h>

static const char *const PS_BASE = "/sys/class/power_supply";

static const char *const CHARGE_LIMIT_FILES[] = {
	"charge_control_end_threshold",
	"charge_stop_threshold",
	"max_charge_percent",
};

static PowerSupplyKind kind_from_type(const gchar *type)
{
	if (type == NULL)
		return PS_OTHER;
	if (g_ascii_strcasecmp(type, "Battery") == 0 ||
	    g_ascii_strcasecmp(type, "UPS") == 0)
		return PS_BATTERY;
	if (g_ascii_strcasecmp(type, "Mains") == 0 ||
	    g_ascii_strcasecmp(type, "USB") == 0 ||
	    g_ascii_strcasecmp(type, "Wireless") == 0)
		return PS_AC;
	return PS_OTHER;
}

static void power_supply_read_static(PowerSupply *ps);

static PowerSupply *power_supply_new(const gchar *path, const gchar *name)
{
	PowerSupply *ps = g_new0(PowerSupply, 1);

	ps->path = g_strdup(path);
	ps->name = g_strdup(name);
	ps->capacity = -1;
	ps->health = -1;
	ps->cycle_count = -1;
	ps->charge_limit = -1;

	ps->kind = kind_from_type(read_first_line_in(path, "type"));

	power_supply_read_static(ps);
	power_supply_refresh(ps);

	return ps;
}

static void power_supply_read_static(PowerSupply *ps)
{
	gint64 value;

	if (read_int64_in(ps->path, "cycle_count", &value) && value > 0) {
		ps->has_cycle_count = TRUE;
		ps->cycle_count = (gint)value;
	}

	ps->has_charge_full =
		read_double_in(ps->path, "charge_full", &ps->charge_full_wh);
	if (ps->has_charge_full)
		ps->charge_full_wh /= 1000000.0;

	if (!ps->has_charge_full &&
	    read_double_in(ps->path, "energy_full", &ps->charge_full_wh)) {
		ps->has_charge_full = TRUE;
		ps->charge_full_wh /= 1000000.0;
	}

	ps->has_charge_design = read_double_in(ps->path, "charge_full_design",
					       &ps->charge_design_wh);
	if (ps->has_charge_design)
		ps->charge_design_wh /= 1000000.0;

	if (!ps->has_charge_design &&
	    read_double_in(ps->path, "energy_full_design",
			   &ps->charge_design_wh)) {
		ps->has_charge_design = TRUE;
		ps->charge_design_wh /= 1000000.0;
	}

	if (ps->has_charge_full && ps->has_charge_design &&
	    ps->charge_design_wh > 0) {
		ps->health =
			(gint)((ps->charge_full_wh / ps->charge_design_wh) *
				       100.0 +
			       0.5);
		ps->has_health = TRUE;
	}

	for (gsize i = 0; i < G_N_ELEMENTS(CHARGE_LIMIT_FILES); i++) {
		if (read_int64_in(ps->path, CHARGE_LIMIT_FILES[i], &value)) {
			g_free(ps->charge_limit_path);
			ps->charge_limit_path = g_build_filename(
				ps->path, CHARGE_LIMIT_FILES[i], NULL);
			ps->has_charge_limit = TRUE;
			ps->charge_limit = (gint)value;
			return;
		}
	}
}

void power_supply_refresh(PowerSupply *ps)
{
	gint64 value;

	if (ps == NULL)
		return;

	g_free(ps->status);
	ps->status = read_first_line_in(ps->path, "status");
	if (ps->status == NULL)
		ps->status = g_strdup("Unknown");

	if (ps->kind != PS_BATTERY) {
		ps->online = read_int64_in(ps->path, "online", &value) &&
			     value > 0;
		return;
	}

	if (!read_int64_in(ps->path, "capacity", &value))
		value = -1;
	ps->capacity = (gint)value;

	ps->has_temp = read_double_in(ps->path, "temp", &ps->temp_c);
	if (ps->has_temp)
		ps->temp_c /= 10.0;

	ps->has_voltage =
		read_double_in(ps->path, "voltage_now", &ps->voltage_v);
	if (ps->has_voltage)
		ps->voltage_v /= 1000000.0;

	ps->has_power = read_double_in(ps->path, "power_now", &ps->power_w);
	if (ps->has_power)
		ps->power_w /= 1000000.0;

	if (!ps->has_power) {
		ps->has_current =
			read_double_in(ps->path, "current_now", &ps->current_a);
		if (ps->has_current) {
			ps->current_a /= 1000000.0;
			if (ps->has_voltage) {
				ps->power_w = ps->current_a * ps->voltage_v;
				ps->has_power = TRUE;
			}
		}
	}

	if (ps->has_charge_limit && ps->charge_limit_path != NULL &&
	    read_int64(ps->charge_limit_path, &value))
		ps->charge_limit = (gint)value;
}

PowerSupply **power_supply_list(gsize *count)
{
	GPtrArray *result;
	gsize n_entries = 0;
	gchar **entries;

	result = g_ptr_array_new();
	entries = list_dir(PS_BASE, &n_entries);
	for (gsize i = 0; i < n_entries; i++) {
		gchar *path = g_build_filename(PS_BASE, entries[i], NULL);
		g_ptr_array_add(result, power_supply_new(path, entries[i]));
		g_free(path);
	}
	g_strfreev(entries);

	if (count != NULL)
		*count = result->len;
	log_debug("power_supply: %u supplies", (guint)result->len);
	g_ptr_array_add(result, NULL);
	return (PowerSupply **)g_ptr_array_free(result, FALSE);
}

void power_supply_free(PowerSupply *ps)
{
	if (ps == NULL)
		return;
	g_free(ps->path);
	g_free(ps->name);
	g_free(ps->status);
	g_free(ps->charge_limit_path);
	g_free(ps);
}

void power_supply_list_free(PowerSupply **list, gsize count)
{
	if (list == NULL)
		return;
	for (gsize i = 0; i < count; i++)
		power_supply_free(list[i]);
	g_free(list);
}

gboolean power_supply_set_charge_limit(PowerSupply *ps, gint percent,
				       GError **error)
{
	gboolean ok;

	if (ps->charge_limit_path == NULL) {
		log_warn("%s: no charge limit control", ps->path);
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			    "Charge limit control not supported");
		return FALSE;
	}

	ok = write_int64(ps->charge_limit_path, percent, error);
	if (ok)
		log_info("%s: charge limit -> %d%%", ps->charge_limit_path,
			 percent);
	else
		log_warn("%s: charge limit -> %d%% not applied",
			 ps->charge_limit_path, percent);
	return ok;
}
