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
#ifndef FSA4480_I2C_H
#define FSA4480_I2C_H

#include <linux/of.h>
#include <linux/notifier.h>

enum fsa_function {
	FSA_MIC_GND_SWAP,
	FSA_USBC_ORIENTATION_CC1,
	FSA_USBC_ORIENTATION_CC2,
	FSA_USBC_DISPLAYPORT_DISCONNECTED,
	FSA_EVENT_MAX,
};

#ifdef CONFIG_QCOM_FSA4480_I2C
int fsa4480_switch_event(struct device_node *node,
			 enum fsa_function event);
int fsa4480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node);
int fsa4480_unreg_notifier(struct notifier_block *nb,
			   struct device_node *node);
#else
static inline int fsa4480_switch_event(struct device_node *node,
				       enum fsa_function event)
{
	return 0;
}

static inline int fsa4480_reg_notifier(struct notifier_block *nb,
				       struct device_node *node)
{
	return 0;
}

static inline int fsa4480_unreg_notifier(struct notifier_block *nb,
					 struct device_node *node)
{
	return 0;
}
#endif /* CONFIG_QCOM_FSA4480_I2C */

#endif /* FSA4480_I2C_H */

