
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
#include <linux/battmngr/xm_charger_core.h>
#include "../../extSOC/inc/virtual_fg.h"

struct xm_battmngr {
	struct device *dev;
	struct xm_charger charger;
	struct vir_bq_fg_chip fg;
	struct notifier_block battmngr_nb;
	struct battmngr_notify battmngr_noti;
};

#endif /* __XM_BATTMNGR_INIT_H */

