/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
	-EINVAL,				/* wake2 */
	TEGRA_GPIO_PB6,				/* wake3 */
	TEGRA_GPIO_PN7,				/* wake4 */
	-EINVAL,				/* wake5 */
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
	-EINVAL,				/* wake19 */
	-EINVAL,				/* wake20 */
	-EINVAL,				/* wake21 */
	-EINVAL,				/* wake22 */
	TEGRA_GPIO_PI5,				/* wake23 */
	TEGRA_GPIO_PV0,				/* wake24 */
	TEGRA_GPIO_PS4,				/* wake25 */
	TEGRA_GPIO_PS5,				/* wake26 */
	TEGRA_GPIO_PS0,				/* wake27 */
	TEGRA_GPIO_PS6,				/* wake28 */
	TEGRA_GPIO_PS7,				/* wake29 */
	TEGRA_GPIO_PN2,				/* wake30 */
	-EINVAL,				/* wake31 */
	TEGRA_GPIO_PO4,				/* wake32 */
	TEGRA_GPIO_PJ0,				/* wake33 */
	TEGRA_GPIO_PK2,				/* wake34 */
	TEGRA_GPIO_PI6,				/* wake35 */
	-EINVAL,				/* wake36 */
	-EINVAL,				/* wake37 */
	-EINVAL,				/* wake38 */
	-EINVAL,				/* wake39 */
	-EINVAL,				/* wake40 */
	-EINVAL,				/* wake41 */
	-EINVAL,				/* wake42 */
	-EINVAL,				/* wake43 */
	TEGRA_GPIO_PC5,				/* wake44 */
	TEGRA_GPIO_PBB6,			/* wake45 */
	TEGRA_GPIO_PZ7,				/* wake46 */
	TEGRA_GPIO_PT6,				/* wake47 */
	TEGRA_GPIO_PBB6,			/* wake48 */
	TEGRA_GPIO_PR7,				/* wake49 */
	TEGRA_GPIO_PR4,				/* wake50 */
	TEGRA_GPIO_PQ0,				/* wake51 */
	TEGRA_GPIO_PEE3,			/* wake52 */
	TEGRA_GPIO_PBB1,			/* wake53 */
	TEGRA_GPIO_PQ5,				/* wake54 */
	TEGRA_GPIO_PA1,				/* wake55 */
	TEGRA_GPIO_PV2,				/* wake56 */
	TEGRA_GPIO_PK6,				/* wake57 */
	-EINVAL,				/* wake58 */
	TEGRA_GPIO_PFF2,			/* wake59 */
	-EINVAL,				/* wake60 */
};

static int tegra_wake_event_irq[] = {
	-EAGAIN, /* ULPI DATA4 */		/* wake0 */
	-EAGAIN,				/* wake1 */
	-EINVAL,				/* wake2 */
	-EAGAIN, /* SDMMC3 DAT1 */		/* wake3 */
	-EAGAIN, /* HDMI INT */		/* wake4 */
	-EAGAIN,				/* wake5 */
	-EAGAIN,				/* wake6 */
	-EAGAIN,				/* wake7 */
	-EAGAIN,				/* wake8 */
	-EAGAIN, /* UART3 RXD */		/* wake9 */
	-EAGAIN, /* SDMMC4 DAT1 */		/* wake10 */
	-EAGAIN,				/* wake11 */
	-EAGAIN,				/* wake12 */
	-EAGAIN, /* SDMMC1 DAT1 */		/* wake13 */
	-EAGAIN, /* PEX_WAKE_N */		/* wake14 */
	-EAGAIN, /* soc_therm_oc4_n:i, PG_OC */	/* wake15 */
	INT_RTC,				/* wake16 */
	INT_KBC,				/* wake17 */
	INT_EXTERNAL_PMU,			/* wake18 */
	-EINVAL, /* INT_USB */			/* wake19 */
	-EINVAL,				/* wake20 */
	-EINVAL, /* INT_USB */			/* wake21 */
	-EINVAL,				/* wake22 */
	-EAGAIN,				/* wake23 */
	-EAGAIN,				/* wake24 */
	-EAGAIN,				/* wake25 */
	-EAGAIN,				/* wake26 */
	-EAGAIN,				/* wake27 */
	-EAGAIN,				/* wake28 */
	-EAGAIN, /* soc_therm_oc1_n:i, GPU_OC_INT */	/* wake29 */
	-EAGAIN, /* I2S0 SDATA OUT */		/* wake30 */
	-EINVAL,				/* wake31 */
	-EAGAIN, /* ULPI DATA3 */		/* wake32 */
	-EAGAIN,				/* wake33 */
	-EAGAIN,				/* wake34 */
	-EAGAIN,				/* wake35 */
	-EINVAL,				/* wake36 */
	-EINVAL,				/* wake37 */
	-EINVAL,				/* wake38 */
	INT_USB, /* TEGRA_USB1_UTMIP, */	/* wake39 */
	INT_USB2, /* TEGRA_USB2_UTMIP */	/* wake40 */
	INT_USB3, /* TEGRA_USB3_UTMIP, */	/* wake41 */
	INT_USB2, /* TEGRA_USB2_UHSIC */	/* wake42 */
	INT_USB3, /* TEGRA_USB3_UHSIC */	/* wake43 */
	-EAGAIN, /* I2C1 DAT */			/* wake44 */
	-EAGAIN,				/* wake45 */
	-EAGAIN, /* PWR I2C DAT */		/* wake46 */
	-EAGAIN, /* I2C2 DAT */			/* wake47 */
	-EAGAIN, /* I2C3 DAT */			/* wake48 */
	-EAGAIN,				/* wake49 */
	-EAGAIN,				/* wake50 */
	-EAGAIN, /* KBC11 */			/* wake51 */
	-EAGAIN, /* HDMI CEC */			/* wake52 */
	-EAGAIN, /* I2C3 CLK */			/* wake53 */
	-EAGAIN,				/* wake54 */
	-EAGAIN, /* UART3 CTS */		/* wake55 */
	-EAGAIN, /* SDMMC3 CD */		/* wake56 */
	-EAGAIN, /* EN_VDD_HDMI, */		/* wake57 */
	INT_XUSB_PADCTL,			/* wake58 */
	-EAGAIN,				/* wake59 */
	-EINVAL,				/* wake60 */
};

static int __init tegra12x_wakeup_table_init(void)
{
	tegra_gpio_wake_table = tegra_gpio_wakes;
	tegra_irq_wake_table = tegra_wake_event_irq;
	tegra_wake_table_len = ARRAY_SIZE(tegra_gpio_wakes);
	return 0;
}
postcore_initcall(tegra12x_wakeup_table_init);
