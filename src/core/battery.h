#ifndef RV_CORE_BATTERY_H
#define RV_CORE_BATTERY_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
        RV_PS_BATTERY,
        RV_PS_AC,
        RV_PS_OTHER
} RvPowerSupplyKind;

typedef struct {
        RvPowerSupplyKind  kind;
        gchar             *path;
        gchar             *name;
        gchar             *status;
        gint               capacity;
        gboolean           online;
        gboolean           has_temp;
        gdouble            temp_c;
        gboolean           has_voltage;
        gdouble            voltage_v;
        gboolean           has_power;
        gdouble            power_w;
        gboolean           has_current;
        gdouble            current_a;
        gboolean           has_health;
        gint               health;
        gboolean           has_cycle_count;
        gint               cycle_count;
        gboolean           has_charge_full;
        gdouble            charge_full_wh;
        gboolean           has_charge_design;
        gdouble            charge_design_wh;
        gchar             *charge_limit_path;
        gboolean           has_charge_limit;
        gint               charge_limit;
} RvPowerSupply;

RvPowerSupply **rv_power_supply_list (gsize *count);
void            rv_power_supply_refresh (RvPowerSupply *ps);
void            rv_power_supply_free (RvPowerSupply *ps);
void            rv_power_supply_list_free (RvPowerSupply **list, gsize count);

gboolean        rv_power_supply_set_charge_limit (RvPowerSupply *ps,
                                                  gint           percent,
                                                  GError       **error);

G_END_DECLS

#endif
