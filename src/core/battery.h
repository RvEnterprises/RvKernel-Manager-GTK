#ifndef CORE_BATTERY_H
#define CORE_BATTERY_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
        PS_BATTERY,
        PS_AC,
        PS_OTHER
} PowerSupplyKind;

typedef struct {
        PowerSupplyKind  kind;
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
} PowerSupply;

PowerSupply **power_supply_list (gsize *count);
void            power_supply_refresh (PowerSupply *ps);
void            power_supply_free (PowerSupply *ps);
void            power_supply_list_free (PowerSupply **list, gsize count);

gboolean        power_supply_set_charge_limit (PowerSupply *ps,
                                                  gint           percent,
                                                  GError       **error);

G_END_DECLS

#endif
