/*
 * Copyright (c) 2011, Google, Inc.
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irqchip/tegra.h>

#include <mach/gpio-tegra.h>
#include <mach/irqs.h>

#include "gpio-names.h"
#include "iomap.h"

/* TODO: We could populate the other table from this one at runtime
 * instead of always searching twice */
static int tegra_gpio_wakes[] = {
	[0]  = TEGRA_GPIO_PO5,
	[1]  = TEGRA_GPIO_PV3,
	[2]  = TEGRA_GPIO_PL1,
	[3]  = TEGRA_GPIO_PB6,
	[4]  = TEGRA_GPIO_PN7,
	[5]  = TEGRA_GPIO_PA0,
	[6]  = TEGRA_GPIO_PU5,
	[7]  = TEGRA_GPIO_PU6,
	[8]  = TEGRA_GPIO_PC7,
	[9]  = TEGRA_GPIO_PS2,
	[10] = TEGRA_GPIO_PAA1,
	[11] = TEGRA_GPIO_PW3,
	[12] = TEGRA_GPIO_PW2,
	[13] = TEGRA_GPIO_PY6,
	[14] = TEGRA_GPIO_PV6,
	[15] = TEGRA_GPIO_PJ7,
	[16] = -EINVAL,
	[17] = -EINVAL,
	[18] = -EINVAL,
	[19] = -EINVAL,	 /* TEGRA_USB1_VBUS, */
	[20] = -EINVAL, /* TEGRA_USB3_VBUS, */
	[21] = -EINVAL, /* TEGRA_USB1_ID, */
	[22] = -EINVAL, /* TEGRA_USB3_ID, */
	[23] = TEGRA_GPIO_PI5,
	[24] = TEGRA_GPIO_PV2,
	[25] = TEGRA_GPIO_PS4,
	[26] = TEGRA_GPIO_PS5,
	[27] = TEGRA_GPIO_PS0,
	[28] = TEGRA_GPIO_PQ6,
	[29] = TEGRA_GPIO_PQ7,
	[30] = TEGRA_GPIO_PN2,
};

static int tegra_wake_event_irq[] = {
	[0]  = -EAGAIN, /* Search the GPIO table */
	[1]  = -EAGAIN,
	[2]  = -EAGAIN,
	[3]  = -EAGAIN,
	[4]  = -EAGAIN,
	[5]  = -EAGAIN,
	[6]  = -EAGAIN,
	[7]  = -EAGAIN,
	[8]  = -EAGAIN,
	[9]  = -EAGAIN,
	[10] = -EAGAIN,
	[11] = -EAGAIN,
	[12] = -EAGAIN,
	[13] = -EAGAIN,
	[14] = -EAGAIN,
	[15] = -EAGAIN,
	[16] = INT_RTC,
	[17] = INT_KBC,
	[18] = INT_EXTERNAL_PMU,
	[19] = -EINVAL, /* TEGRA_USB1_VBUS, */
	[20] = -EINVAL, /* TEGRA_USB3_VBUS, */
	[21] = -EINVAL, /* TEGRA_USB1_ID, */
	[22] = -EINVAL, /* TEGRA_USB3_ID, */
	[23] = -EAGAIN,
	[24] = -EAGAIN,
	[25] = -EAGAIN,
	[26] = -EAGAIN,
	[27] = -EAGAIN,
	[28] = -EAGAIN,
	[29] = -EAGAIN,
	[30] = -EAGAIN,
};

static int __init tegra2_wakeup_table_init(void)
{
	tegra_gpio_wake_table = tegra_gpio_wakes;
	tegra_irq_wake_table = tegra_wake_event_irq;
	tegra_wake_table_len = ARRAY_SIZE(tegra_gpio_wakes);
	return 0;
}
postcore_initcall(tegra2_wakeup_table_init);
