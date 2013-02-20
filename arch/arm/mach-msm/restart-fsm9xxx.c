/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#define FEMTO_GPIO_PS_HOLD 161

void fsm_restart(char mode, const char *cmd)
{
	pr_notice("Going down for restart now\n");
	msleep(3000);

	/* Configure FEMTO_GPIO_PS_HOLD as a general purpose output */
	if (gpio_tlmm_config(GPIO_CFG(FEMTO_GPIO_PS_HOLD, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), GPIO_CFG_ENABLE))
		pr_err("%s: gpio_tlmm_config (gpio=%d) failed\n",
			__func__, FEMTO_GPIO_PS_HOLD);

	/* Now set it low to power cycle the entire board */
	gpio_set_value(FEMTO_GPIO_PS_HOLD, 0);

	msleep(10000);
	pr_err("Restarting has failed\n");
}
