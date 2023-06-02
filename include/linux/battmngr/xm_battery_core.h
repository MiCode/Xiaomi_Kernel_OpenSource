
#ifndef __XM_BATTERY_CORE_H
#define __XM_BATTERY_CORE_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/device.h>

#include <linux/battmngr/xm_battmngr_iio.h>
#include <linux/battmngr/xm_charger_core.h>
#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/battmngr_notifier.h>
#include <linux/battmngr/xm_battery_feature.h>

static int bat_log_level = 2;
#define battery_err(fmt, ...)							\
do {										\
	if (bat_log_level >= 0)							\
		printk(KERN_ERR "[xm_battery_core] " fmt, ##__VA_ARGS__);	\
} while (0)

#define battery_info(fmt, ...)							\
do {										\
	if (bat_log_level >= 1)							\
		printk(KERN_ERR "[xm_battery_core] " fmt, ##__VA_ARGS__);	\
} while (0)

#define battery_dbg(fmt, ...)							\
do {										\
	if (bat_log_level >= 2)							\
		printk(KERN_ERR "[xm_battery_core] " fmt, ##__VA_ARGS__);	\
} while (0)

#define MAX_TEMP_LEVEL		16

#define BATT_OVERHEAT_THRESHOLD		580
#define BATT_WARM_THRESHOLD		480
#define BATT_COOL_THRESHOLD		150
#define BATT_COLD_THRESHOLD		0

struct battery_dt_props {

};

struct xm_battery {
	struct device *dev;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct batt_feature_info *batt_feature;

	struct battery_dt_props	dt;
};

extern struct xm_battery *g_xm_battery;

extern int xm_battery_init(struct xm_battery *battery);
extern int battery_process_event_fg(struct battmngr_notify *noti_data);
int xm_batt_feature_init(struct xm_battery *battery);

#endif /* __XM_BATTERY_CORE_H */

