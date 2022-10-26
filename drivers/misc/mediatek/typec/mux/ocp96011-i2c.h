/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef OCP96011_I2C_H
#define OCP96011_I2C_H

#include <linux/of.h>
#include <linux/notifier.h>

enum fsa_function {
	FSA_SWITCH_TO_USB,
	FSA_SWITCH_TO_AUDIO,
	FSA_USBC_ORIENTATION_CC1,
	FSA_USBC_ORIENTATION_CC2,
	FSA_USBC_DISPLAYPORT_DISCONNECTED,
	FSA_SWITCH_TO_HIGH_Z_STATUS,
	FSA_EVENT_MAX,
};

/*
enum fsa_switch_select {
	FSA_SWITCH_TO_USB,
	FSA_SWITCH_TO_L_R_ONLY,
	FSA_SWITCH_TO_L_R_MIC,
	FSA_SWITCH_MAX,
};
*/
struct ocp96011_priv {
	struct regmap *regmap;
	struct device *dev;
	unsigned short addr;	
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	/*z17 add fsa4480/ocp96011 notifier call*/
	struct blocking_notifier_head ocp96011_notifier;
	struct mutex notification_lock;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc_dev;
	struct notifier_block tcpc_nb;
#endif
	struct typec_mux		*mux;
};

u32 ocp96011_get_headset_status(void);
int ocp96011_reg_notifier(struct notifier_block *nb);
int ocp96011_unreg_notifier(struct notifier_block *nb);
int ocp960_get_headset_count(void);


#endif /* OCP96011_I2C_H */

