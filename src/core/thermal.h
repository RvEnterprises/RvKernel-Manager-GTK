#ifndef RV_CORE_THERMAL_H
#define RV_CORE_THERMAL_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
        gchar    *path;
        gint      index;
        gchar    *type;
        gdouble   temp_c;
        gboolean  has_temp;
        gboolean  has_mode;
        gchar    *mode;
        gchar   **available_modes;
} RvThermalZone;

typedef struct {
        gchar *path;
        gint   index;
        gchar *type;
        gint   cur_state;
        gint   max_state;
        gboolean has_states;
} RvCoolingDevice;

RvThermalZone **rv_thermal_zones     (gsize *count);
void            rv_thermal_zone_refresh (RvThermalZone *zone);
void            rv_thermal_zone_free (RvThermalZone *zone);
void            rv_thermal_zones_free (RvThermalZone **zones, gsize count);

gboolean        rv_thermal_zone_set_mode (RvThermalZone *zone,
                                          const gchar   *mode,
                                          GError       **error);

RvCoolingDevice **rv_cooling_devices (gsize *count);
void              rv_cooling_device_refresh (RvCoolingDevice *dev);
void              rv_cooling_device_free (RvCoolingDevice *dev);
void              rv_cooling_devices_free (RvCoolingDevice **devices,
                                           gsize count);

G_END_DECLS

#endif
