#ifndef _BATTERY_CHARGALG_H_
#define _BATTERY_CHARGALG_H_

#include <linux/types.h>

#define BATTERY_CHARGALG_NAME "chargalg"

enum battery_charalg_temperature {
	BATTERY_CHARGALG_TEMP_COLD,
	BATTERY_CHARGALG_TEMP_NORMAL,
	BATTERY_CHARGALG_TEMP_WARM,
	BATTERY_CHARGALG_TEMP_OVERHEAT,
	BATTERY_CHARGALG_TEMP_MAX_NBR,
};

/**
 * struct battery_regulation_vs_temperature
 * @temp: The battery temperature in degree C
 * @volt: The battery voltage in mV
 * @curr: The battery current in mA
 *
 * Up to (<) temperature 'temp' charge with maximum 'volt' mV and 'curr' mA.
 */
struct battery_regulation_vs_temperature {
	s8 temp[BATTERY_CHARGALG_TEMP_MAX_NBR];
	u16 volt[BATTERY_CHARGALG_TEMP_MAX_NBR];
	u16 curr[BATTERY_CHARGALG_TEMP_MAX_NBR];
};

/**
 * struct ambient_temperature_limit
 * @base: The ambient temp limit
 * @hyst: The ambient temp hysteresis
 *
 */
struct ambient_temperature_limit {
	s8 base[BATTERY_CHARGALG_TEMP_MAX_NBR];
	s8 hyst[BATTERY_CHARGALG_TEMP_MAX_NBR];
};

struct ambient_temperature_data {
	struct ambient_temperature_limit *limit_tbl;
};

struct battery_chargalg_platform_data {
	const char *name;
	char **supplied_to;
	size_t num_supplicants;

	u16 overvoltage_max_design; /* mV */
	struct battery_regulation_vs_temperature *id_bat_reg;
	struct battery_regulation_vs_temperature *unid_bat_reg;
	u8 ext_eoc_recharge_enable;
	u8 recharge_threshold_capacity; /* % */
	u16 eoc_current_term; /* mA */
	s16 eoc_current_flat_time; /* seconds */
	s8 temp_hysteresis_design; /* degree C */
	const u16 *battery_capacity_mah;  /* mAh */
	u8 disable_usb_host_charging;
	struct ambient_temperature_data *ambient_temp;
	char *batt_volt_psy_name;
	char *batt_curr_psy_name;

	int (*turn_on_charger)(u8 usb_compliant);
	int (*turn_off_charger)(void);
	int (*set_charger_voltage)(u16 mv);
	int (*set_charger_current)(u16 mv);
	int (*set_eoc_current_term)(u16 ma);
	int (*set_charger_safety_timer)(u16 minutes);
	int (*set_input_current_limit)(u16 ma);
	int (*set_charging_status)(int status);
	unsigned int (*get_supply_current_limit)(void);

	unsigned int allow_dynamic_charge_current_ctrl;
	u16 charge_set_current_1; /* mA */
	u16 charge_set_current_2; /* mA */
	u16 charge_set_current_3; /* mA */
	int average_current_min_limit; /* mA */
	int average_current_max_limit; /* mA */
};

extern struct ambient_temperature_data battery_chargalg_platform_ambient_temp;

extern const u16 battery_capacity_mah;

void battery_chargalg_set_battery_health(int health);

#endif /* _BATTERY_CHARGALG_H_ */
