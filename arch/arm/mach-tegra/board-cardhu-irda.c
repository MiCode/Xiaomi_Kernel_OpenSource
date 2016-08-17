/*
 * arch/arm/mach-tegra/board-cardhu-irda.c
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* This driver tested with tfdu6103 irda transceiver */

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/serial_8250.h>
#include <linux/tegra_uart.h>

#include "gpio-names.h"
#include "board-cardhu.h"
#include "board.h"
#include "devices.h"

/* Uncomment the next line to get the function entry logs */
/*#define DRV_FUNC	1*/

#undef FPRINT
#ifdef DRV_FUNC
#define FPRINT(fmt, args...) printk(KERN_INFO "IRDA: " fmt, ## args)
#else
#define FPRINT(fmt, args...)
#endif

#define CARDHU_IRDA_SD TEGRA_GPIO_PJ6
#define CARDHU_IRDA_TX TEGRA_GPIO_PC2
#define CARDHU_IRDA_RX TEGRA_GPIO_PC3

#define IRDA_DELAY	1

#define SIR		1
#define FIR		2
#define VFIR		3	/* tfdu6108 doesn't support */


static int irda_mode;

/* If mode = SIR	mode switch will be FIR -> SIR
   If mode = FIR	mode switch will be SIR ->FIR	*/

static int cardhu_irda_mode_switch(int mode)
{
	int ret = -1;

	FPRINT("Start of Func %s\n", __func__);

	if ((mode != SIR) && (mode != FIR)) {
		pr_err("Unsupported irda mode\n");
		return ret;
	}

	gpio_set_value(CARDHU_IRDA_SD, 1);

	udelay(IRDA_DELAY);

	ret = gpio_request(CARDHU_IRDA_TX, "irda_tx");
	if (ret < 0) {
		pr_err("%s: cardhu_irda_tx gpio request failed %d\n",
							 __func__, ret);
		gpio_set_value(CARDHU_IRDA_SD, 0);
		return ret;
	}

	if (mode == SIR)
		ret = gpio_direction_output(CARDHU_IRDA_TX, 0);
	else if (mode == FIR)
		ret = gpio_direction_output(CARDHU_IRDA_TX, 1);

	if (ret) {
		pr_err("%s: cardhu_irda_tx Direction configuration failed %d\n",
							__func__, ret);
		gpio_set_value(CARDHU_IRDA_SD, 0);
		goto closure;
	}

	udelay(IRDA_DELAY);

	gpio_set_value(CARDHU_IRDA_SD, 0);

	udelay(IRDA_DELAY);

	if (mode == FIR) {
		gpio_set_value(CARDHU_IRDA_TX, 0);
		irda_mode = FIR;
		pr_info("IrDA Transceiver is switched to FIR mode\n");
	} else {
		pr_info("IrDA Transceiver is switched to SIR mode\n");
		irda_mode = SIR;
	}

	udelay(IRDA_DELAY);

closure:
	gpio_free(CARDHU_IRDA_TX);
	return ret;
}

static int SD_config(void)
{
	int ret = -1;

	FPRINT("Start of the Func %s\n", __func__);
	/* Gpio enable for SD */
	ret = gpio_request(CARDHU_IRDA_SD, "irda_sd");
	if (ret < 0) {
		pr_err("%s: cardhu_irda_sd gpio request failed %d\n",
							__func__, ret);
		return ret;
	}

	ret = gpio_direction_output(CARDHU_IRDA_SD, 1);
	if (ret)
		pr_err("%s: cardhu_irda_sd Direction configuration failed %d\n",
						__func__, ret);
	return ret;
}

static void cardhu_irda_start(void)
{
	FPRINT("Start of the Func %s\n", __func__);
	pr_info("IrDA transceiver is enabled\n");
	gpio_set_value(CARDHU_IRDA_SD, 0);
	irda_mode = SIR;
}

static void cardhu_irda_shutdown(void)
{
	FPRINT("Start of the Func %s\n", __func__);
	pr_info("IrDA transceiver is disabled\n");
	/* Setting the IrDA transceiver into shutdown mode*/
	gpio_set_value(CARDHU_IRDA_SD, 1);
}

static int cardhu_irda_init(void)
{
	int ret = 0;

	FPRINT("Start of the Func %s\n", __func__);
	if (SD_config() < 0) {
		pr_err("%s: Error in IRDA_SD signal configuration\n", __func__);
		ret = -1;
	}
	return ret;
}

static void cardhu_irda_remove(void)
{
	FPRINT("Start of the Func %s\n", __func__);
	gpio_free(CARDHU_IRDA_SD);
}


struct tegra_uart_platform_data cardhu_irda_pdata = {
	.is_irda		= true,
	.irda_init		= cardhu_irda_init,
	.irda_start		= cardhu_irda_start,
	.irda_mode_switch	= cardhu_irda_mode_switch,
	.irda_shutdown		= cardhu_irda_shutdown,
	.irda_remove		= cardhu_irda_remove,
};
