/*
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

#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "iomap.h"

static int tegra_gpio_wakes[] = {
	TEGRA_GPIO_PO5,				/* wake0 */
	TEGRA_GPIO_PV1,				/* wake1 */
	TEGRA_GPIO_PL1,				/* wake2 */
	TEGRA_GPIO_PB6,				/* wake3 */
	TEGRA_GPIO_PN7,				/* wake4 */
	TEGRA_GPIO_PBB6,			/* wake5 */
	TEGRA_GPIO_PU5,				/* wake6 */
	TEGRA_GPIO_PU6,				/* wake7 */
	TEGRA_GPIO_PC7,				/* wake8 */
	TEGRA_GPIO_PS2,				/* wake9 */
	TEGRA_GPIO_PAA1,			/* wake10 */
	TEGRA_GPIO_PW3,				/* wake11 */
	TEGRA_GPIO_PW2,				/* wake12 */
	TEGRA_GPIO_PY6,				/* wake13 */
	TEGRA_GPIO_PDD3,			/* wake14 */
	TEGRA_GPIO_PJ2,				/* wake15 */
	-EINVAL,				/* wake16 */
	-EINVAL,				/* wake17 */
	-EINVAL,				/* wake18 */
	-EINVAL, /* TEGRA_USB1_VBUS, */		/* wake19 */
	-EINVAL, /* TEGRA_USB2_VBUS, */		/* wake20 */
	-EINVAL, /* TEGRA_USB1_ID, */		/* wake21 */
	-EINVAL, /* TEGRA_USB2_ID, */		/* wake22 */
	TEGRA_GPIO_PI5,				/* wake23 */
	TEGRA_GPIO_PV0,				/* wake24 */
	TEGRA_GPIO_PS4,				/* wake25 */
	TEGRA_GPIO_PS5,				/* wake26 */
	TEGRA_GPIO_PS0,				/* wake27 */
	TEGRA_GPIO_PS6,				/* wake28 */
	TEGRA_GPIO_PS7,				/* wake29 */
	TEGRA_GPIO_PN2,				/* wake30 */
	-EINVAL, /* not used */			/* wake31 */
	TEGRA_GPIO_PO4,				/* wake32 */
	TEGRA_GPIO_PJ0,				/* wake33 */
	TEGRA_GPIO_PK2,				/* wake34 */
	TEGRA_GPIO_PI6,				/* wake35 */
	TEGRA_GPIO_PBB1,			/* wake36 */
	-EINVAL, /* TEGRA_USB3_VBUS, */		/* wake37 */
	-EINVAL, /* TEGRA_USB3_ID, */		/* wake38 */
	-EINVAL, /* TEGRA_USB1_UTMIP, */	/* wake39 */
	-EINVAL, /* TEGRA_USB2_UTMIP, */	/* wake40 */
	-EINVAL, /* TEGRA_USB3_UTMIP, */	/* wake41 */
};

static int tegra_wake_event_irq[] = {
	-EAGAIN,				/* wake0 */
	-EAGAIN,				/* wake1 */
	-EAGAIN,				/* wake2 */
	-EAGAIN,				/* wake3 */
	-EAGAIN,				/* wake4 */
	-EAGAIN,				/* wake5 */
	-EAGAIN,				/* wake6 */
	-EAGAIN,				/* wake7 */
	-EAGAIN,				/* wake8 */
	-EAGAIN,				/* wake9 */
	-EAGAIN,				/* wake10 */
	-EAGAIN,				/* wake11 */
	-EAGAIN,				/* wake12 */
	-EAGAIN,				/* wake13 */
	-EAGAIN,				/* wake14 */
	-EAGAIN,				/* wake15 */
	INT_RTC,				/* wake16 */
	INT_KBC,				/* wake17 */
	INT_EXTERNAL_PMU,			/* wake18 */
	-EINVAL, /* TEGRA_USB1_VBUS, */		/* wake19 */
	-EINVAL, /* TEGRA_USB2_VBUS, */		/* wake20 */
	-EINVAL, /* TEGRA_USB1_ID, */		/* wake21 */
	-EINVAL, /* TEGRA_USB2_ID, */		/* wake22 */
	-EAGAIN,				/* wake23 */
	-EAGAIN,				/* wake24 */
	-EAGAIN,				/* wake25 */
	-EAGAIN,				/* wake26 */
	-EAGAIN,				/* wake27 */
	-EAGAIN,				/* wake28 */
	-EAGAIN,				/* wake29 */
	-EAGAIN,				/* wake30 */
	-EINVAL, /* not used */			/* wake31 */
	-EAGAIN,				/* wake32 */
	-EAGAIN,				/* wake33 */
	-EAGAIN,				/* wake34 */
	-EAGAIN,				/* wake35 */
	-EAGAIN,				/* wake36 */
	-EINVAL, /* TEGRA_USB3_VBUS, */		/* wake37 */
	-EINVAL, /* TEGRA_USB3_ID, */		/* wake38 */
	INT_USB, /* TEGRA_USB1_UTMIP, */	/* wake39 */
	INT_USB2, /* TEGRA_USB2_UTMIP, */	/* wake40 */
	INT_USB3, /* TEGRA_USB3_UTMIP, */	/* wake41 */
	INT_USB2, /* TEGRA_USB2_UHSIC, */	/* wake42 */
};

static int __init tegra3_wakeup_table_init(void)
{
	tegra_gpio_wake_table = tegra_gpio_wakes;
	tegra_irq_wake_table = tegra_wake_event_irq;
	tegra_wake_table_len = ARRAY_SIZE(tegra_gpio_wakes);
	return 0;
}
postcore_initcall(tegra3_wakeup_table_init);
