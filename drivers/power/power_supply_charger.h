
#ifndef __POWER_SUPPLY_CHARGER_H__

#define __POWER_SUPPLY_CHARGER_H__
#include <linux/power/battery_id.h>
#include <linux/power_supply.h>

#define MAX_CUR_VOLT_SAMPLES 3
#define DEF_CUR_VOLT_SAMPLE_JIFF (30*HZ)

enum psy_algo_stat {
	PSY_ALGO_STAT_UNKNOWN,
	PSY_ALGO_STAT_NOT_CHARGE,
	PSY_ALGO_STAT_CHARGE,
	PSY_ALGO_STAT_FULL,
	PSY_ALGO_STAT_MAINT,
};

struct batt_props {
	struct list_head node;
	const char *name;
	long voltage_now;
	long voltage_now_cache[MAX_CUR_VOLT_SAMPLES];
	long current_now;
	long current_now_cache[MAX_CUR_VOLT_SAMPLES];
	int temperature;
	long status;
	unsigned long tstamp;
	enum psy_algo_stat algo_stat;
	int health;
};

struct charger_props {
	struct list_head node;
	const char *name;
	bool present;
	bool is_charging;
	int health;
	bool online;
	unsigned long cable;
	unsigned long tstamp;
	int throttle_state;
};

struct psy_batt_thresholds {
	int temp_min;
	int temp_max;
	unsigned int iterm;
};

struct charging_algo {
	struct list_head node;
	unsigned int chrg_prof_type;
	char *name;
	enum psy_algo_stat (*get_next_cc_cv)(struct batt_props,
			struct ps_batt_chg_prof, unsigned long *cc,
			unsigned long *cv);
	int (*get_batt_thresholds)(struct ps_batt_chg_prof,
			struct psy_batt_thresholds *bat_thr);
};


extern int power_supply_register_charging_algo(struct charging_algo *);
extern int power_supply_unregister_charging_algo(struct charging_algo *);

static inline int set_ps_int_property(struct power_supply *psy,
				      enum power_supply_property psp,
				      int prop_val)
{

	union power_supply_propval val;
	int ret = -ENODEV;

	val.intval = prop_val;

	if (psy->set_property)
		ret = psy->set_property(psy, psp, &val);

	return ret;
}

static inline int get_ps_int_property(struct power_supply *psy,
				      enum power_supply_property psp)
{
	union power_supply_propval val;

	val.intval = 0;

	if (psy->get_property)
		psy->get_property(psy, psp, &val);

	return val.intval;
}
/* Define a TTL for some properies to optimize the frequency of
* algorithm calls. This can be used by properties which will be changed
* very frequently (eg. current, volatge..)
*/
#define PROP_TTL (HZ*10)
#define enable_charging(psy) \
		({if ((CABLE_TYPE(psy) != POWER_SUPPLY_CHARGER_TYPE_NONE) &&\
			!IS_CHARGING_ENABLED(psy)) { \
		enable_charger(psy); \
		set_ps_int_property(psy, POWER_SUPPLY_PROP_ENABLE_CHARGING,\
					true); } })
#define disable_charging(psy) \
		set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_ENABLE_CHARGING, false);

#define enable_charger(psy) \
		set_ps_int_property(psy, POWER_SUPPLY_PROP_ENABLE_CHARGER, true)
#define disable_charger(psy) \
		({  disable_charging(psy); \
			set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_ENABLE_CHARGER, false); })

#define set_cc(psy, cc) \
		set_ps_int_property(psy, POWER_SUPPLY_PROP_CHARGE_CURRENT, cc)

#define set_cv(psy, cv) \
		set_ps_int_property(psy, POWER_SUPPLY_PROP_CHARGE_VOLTAGE, cv)

#define set_inlmt(psy, inlmt) \
		set_ps_int_property(psy, POWER_SUPPLY_PROP_INLMT, inlmt)

#define set_present(psy, present) \
		set_ps_int_property(psy, POWER_SUPPLY_PROP_PRESENT, present)

#define SET_MAX_CC(psy, max_cc) \
		set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT, max_cc)
#define SET_ITERM(psy, iterm) \
		set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_CHARGE_TERM_CUR, iterm)
#define SET_MAX_TEMP(psy, temp) \
		set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_MAX_TEMP, temp)
#define SET_MIN_TEMP(psy, temp) \
		set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_MIN_TEMP, temp)
#define switch_cable(psy, new_cable) \
		set_ps_int_property(psy,\
				POWER_SUPPLY_PROP_CABLE_TYPE, new_cable)

#define HEALTH(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_HEALTH)
#define CV(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_CHARGE_VOLTAGE)
#define CC(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_CHARGE_CURRENT)
#define INLMT(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_INLMT)
#define MAX_CC(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT)
#define MAX_CV(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE)
#define VOLTAGE_NOW(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW)
#define VOLTAGE_OCV(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_VOLTAGE_OCV)
#define CURRENT_NOW(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW)
#define STATUS(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_STATUS)
#define TEMPERATURE(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_TEMP)
#define BATTERY_TYPE(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_TECHNOLOGY)
#define PRIORITY(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_PRIORITY)
#define CABLE_TYPE(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_CABLE_TYPE)
#define ONLINE(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_ONLINE)
#define INLMT(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_INLMT)
#define ITERM(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_CHARGE_TERM_CUR)

#define IS_CHARGING_ENABLED(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_ENABLE_CHARGING)
#define IS_CHARGER_ENABLED(psy) \
		get_ps_int_property(psy, POWER_SUPPLY_PROP_ENABLE_CHARGER)
#define IS_BATTERY(psy) (psy->type == POWER_SUPPLY_TYPE_BATTERY)
#define IS_CHARGER(psy) (psy->type == POWER_SUPPLY_TYPE_USB ||\
				psy->type == POWER_SUPPLY_TYPE_USB_CDP || \
			psy->type == POWER_SUPPLY_TYPE_USB_DCP || \
			psy->type == POWER_SUPPLY_TYPE_USB_ACA || \
			psy->type == POWER_SUPPLY_TYPE_USB_TYPEC)
#define IS_ONLINE(psy) \
		(get_ps_int_property(psy, POWER_SUPPLY_PROP_ONLINE) == 1)
#define IS_PRESENT(psy) \
		(get_ps_int_property(psy, POWER_SUPPLY_PROP_PRESENT) == 1)
#define IS_SUPPORTED_CABLE(psy, cable_type) \
		(psy->supported_cables & cable_type)
#define IS_CABLE_ACTIVE(status) \
	((status != EXTCON_CHRGR_CABLE_DISCONNECTED))

#define IS_CHARGER_PROP_CHANGED(prop, cache_prop)\
	((cache_prop.online != prop.online) || \
	(cache_prop.present != prop.present) || \
	(cache_prop.is_charging != prop.is_charging) || \
	(cache_prop.health != prop.health) || \
	(cache_prop.throttle_state != prop.throttle_state))

#define IS_BAT_PROP_CHANGED(bat_prop, bat_cache)\
	((bat_cache.voltage_now != bat_prop.voltage_now) || \
	(time_after64(bat_prop.tstamp, (bat_cache.tstamp + PROP_TTL)) &&\
	((bat_cache.current_now != bat_prop.current_now) || \
	(bat_cache.voltage_now != bat_prop.voltage_now))) || \
	(bat_cache.temperature != bat_prop.temperature) || \
	(bat_cache.health != bat_prop.health))

#define MAX_THROTTLE_STATE(psy)\
		(get_ps_int_property(psy,\
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX))

#define CURRENT_THROTTLE_STATE(psy)\
		(get_ps_int_property(psy,\
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT))

#ifndef CONFIG_RAW_CC_THROTTLE

#define THROTTLE_ACTION(psy, state)\
		(((psy->throttle_states)+state)->throttle_action)

#define THROTTLE_VALUE(psy, state)\
		(((psy->throttle_states)+state)->throttle_val)
#define SET_MAX_THROTTLE_STATE(psy) \
		(set_ps_int_property(psy,\
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,\
			psy->num_throttle_states + 1))
#else
#define RAW_CC_STEP 100 /* 100 ma*/
#define THROTTLE_ACTION(psy, state)\
		(CURRENT_THROTTLE_STATE(psy) < (MAX_THROTTLE_STATE(psy) - 1) ?\
			PSY_THROTTLE_CC_LIMIT : PSY_THROTTLE_DISABLE_CHARGING)

#define THROTTLE_VALUE(psy, state)\
		((MAX_THROTTLE_STATE(psy) - CURRENT_THROTTLE_STATE(psy) - 1) * \
			RAW_CC_STEP)
#define SET_MAX_THROTTLE_STATE(psy)\
		(set_ps_int_property(psy,\
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,\
			DIV_ROUND_CLOSEST(MAX_CC(psy), RAW_CC_STEP) + 1))
#endif

#define CURRENT_THROTTLE_ACTION(psy)\
		THROTTLE_ACTION(psy, CURRENT_THROTTLE_STATE(psy))

#define IS_CHARGER_CAN_BE_ENABLED(psy) \
	(CURRENT_THROTTLE_ACTION(psy) != PSY_THROTTLE_DISABLE_CHARGER)

#define IS_HEALTH_GOOD(psy)\
	(HEALTH(psy) == POWER_SUPPLY_HEALTH_GOOD)

static inline int set_battery_status(struct power_supply *psy, int status)
{
	if (STATUS(psy) != status) {
		set_ps_int_property(psy, POWER_SUPPLY_PROP_STATUS, status);
		return true;
	}
	return false;
}

static inline void set_charger_online(struct power_supply *psy, int online)
{

	if (ONLINE(psy) != online)
		set_ps_int_property(psy, POWER_SUPPLY_PROP_ONLINE, online);

}

#endif
