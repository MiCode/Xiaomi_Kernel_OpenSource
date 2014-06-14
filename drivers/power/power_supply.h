/*
 *  Functions private to power supply class
 *
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

struct device;
struct device_type;
struct power_supply;

#ifdef CONFIG_SYSFS

extern void power_supply_init_attrs(struct device_type *dev_type);
extern int power_supply_uevent(struct device *dev, struct kobj_uevent_env *env);

#else

static inline void power_supply_init_attrs(struct device_type *dev_type) {}
#define power_supply_uevent NULL

#endif /* CONFIG_SYSFS */

#ifdef CONFIG_LEDS_TRIGGERS

extern void power_supply_update_leds(struct power_supply *psy);
extern int power_supply_create_triggers(struct power_supply *psy);
extern void power_supply_remove_triggers(struct power_supply *psy);

#else

static inline void power_supply_update_leds(struct power_supply *psy) {}
static inline int power_supply_create_triggers(struct power_supply *psy)
{ return 0; }
static inline void power_supply_remove_triggers(struct power_supply *psy) {}

#endif /* CONFIG_LEDS_TRIGGERS */
#ifdef CONFIG_POWER_SUPPLY_CHARGER

extern void power_supply_trigger_charging_handler(struct power_supply *psy);
extern int power_supply_register_charger(struct power_supply *psy);
extern int power_supply_unregister_charger(struct power_supply *psy);
extern int psy_charger_throttle_charger(struct power_supply *psy,
					unsigned long state);

#else

static inline void
	power_supply_trigger_charging_handler(struct power_supply *psy) { }
static inline int power_supply_register_charger(struct power_supply *psy)
{ return 0; }
static inline int power_supply_unregister_charger(struct power_supply *psy)
{ return 0; }
static inline int psy_charger_throttle_charger(struct power_supply *psy,
					unsigned long state)
{ return 0; }

#endif
