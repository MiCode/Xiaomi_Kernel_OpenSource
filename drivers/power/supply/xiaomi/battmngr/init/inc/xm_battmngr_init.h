
#ifndef __XM_BATTMNGR_INIT_H
#define __XM_BATTMNGR_INIT_H

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
#include <linux/debugfs.h>
#include <linux/device.h>

#include <linux/battmngr/battmngr_voter.h>
#include <linux/battmngr/battmngr_notifier.h>
#include <linux/battmngr/xm_battmngr_iio.h>
#include <linux/battmngr/xm_charger_core.h>
#include <linux/battmngr/xm_battery_core.h>

struct xm_battmngr {
	struct device *dev;
	struct class battmngr_class;

	struct xm_battmngr_iio battmngr_iio;
	struct xm_battery battery;
	struct xm_charger charger;
	struct notifier_block battmngr_nb;
	struct battmngr_notify battmngr_noti;
};

extern int get_verify_digest(char *buf);
extern int set_verify_digest(u8 *rand_num);

int battmngr_class_init(struct xm_battmngr *battmngr);
void battmngr_class_exit(struct xm_battmngr *battmngr);

#endif /* __XM_BATTMNGR_INIT_H */

