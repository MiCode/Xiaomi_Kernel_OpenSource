
#ifndef __BATTMNGR_NOTIFIER_H
#define __BATTMNGR_NOTIFIER_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

enum battmngr_event_type {
	BATTMNGR_EVENT_NONE = 0,
	BATTMNGR_EVENT_FG,
	BATTMNGR_EVENT_CP,
	BATTMNGR_EVENT_MAINCHG,
	BATTMNGR_EVENT_PD,
};

enum battmngr_msg_type {
	BATTMNGR_MSG_NONE = 0,

	/* fg */
	BATTMNGR_MSG_FG,

	/* cp */
	BATTMNGR_MSG_CP_MASTER,
	BATTMNGR_MSG_CP_SLAVE,

	/* mainchg */
	BATTMNGR_MSG_MAINCHG_TYPE,

	/* pd */
	BATTMNGR_MSG_PD_ACTIVE,
	BATTMNGR_MSG_PD_VERIFED,
};

struct battmngr_ny_fg {
	int msg_type;
};

struct battmngr_ny_cp {
	int msg_type;
};

struct battmngr_ny_mainchg {
	int msg_type;
	int chg_plugin;
};

struct battmngr_ny_pd {
	int msg_type;
	int pd_active;
	int pd_verified;
};

struct battmngr_notify {
	struct battmngr_ny_fg fg_msg;
	struct battmngr_ny_cp cp_msg;
	struct battmngr_ny_mainchg mainchg_msg;
	struct battmngr_ny_pd pd_msg;
};

extern struct battmngr_notify *g_battmngr_noti;
extern struct xm_battmngr *g_battmngr;
extern int battmngr_notifier_register(struct notifier_block *n);
extern int battmngr_notifier_unregister(struct notifier_block *n);
extern int battmngr_notifier_call_chain(unsigned long event,
			struct battmngr_notify *data);

#endif /* __BATTMNGR_NOTIFIER_H */

