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
	FSA_MIC_GND_SWAP,
	FSA_USBC_ORIENTATION_CC1,
	FSA_USBC_ORIENTATION_CC2,
	//FSA_USBC_DISPLAYPORT_DISCONNECTED,
	//FSA_EVENT_MAX,
};

enum fsa_switch_select {
	FSA_SWITCH_TO_USB,
	FSA_SWITCH_TO_L_R_ONLY,
	FSA_SWITCH_TO_L_R_MIC,
	FSA_SWITCH_MAX,
};

#ifdef CONFIG_OCP96011_I2C
int ocp96011_switch_event(enum fsa_function event);
/*int ocp96011_reg_notifier(struct notifier_block *nb,
			 struct device_node *node);
int ocp96011_unreg_notifier(struct notifier_block *nb,
			   struct device_node *node);*/
#else
static inline int ocp96011_switch_event(enum fsa_function event)
{
	return 0;
}
/*
static inline int ocp96011_reg_notifier(struct notifier_block *nb,
				       struct device_node *node)
{
	return 0;
}

static inline int ocp96011_unreg_notifier(struct notifier_block *nb,
					 struct device_node *node)
{
	return 0;
}*/
#endif /* CONFIG_OCP96011_I2C */

#endif /* OCP96011_I2C_H */
