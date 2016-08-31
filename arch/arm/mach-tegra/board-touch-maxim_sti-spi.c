/*
 * arch/arm/mach-tegra/board-touch-maxim_sti-spi.c
 *
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#if defined(CONFIG_TOUCHSCREEN_MAXIM_STI) || \
	defined(CONFIG_TOUCHSCREEN_MAXIM_STI_MODULE)

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/spi/spi-tegra.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/maxim_sti.h>

#define MAXIM_STI_GPIO_ERROR(ret, gpio, op)                                \
	({if (ret < 0) {                                                   \
		pr_err("%s: GPIO %d %s failed (%d)\n", __func__, gpio, op, \
			ret);                                              \
		return ret;                                                \
	} })
static int maxim_sti_init(struct maxim_sti_pdata *pdata, bool init)
{
	int  ret;

	if (init) {
		ret = gpio_request(pdata->gpio_irq, "maxim_sti_irq");
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_irq, "request");
		ret = gpio_direction_input(pdata->gpio_irq);
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_irq, "direction");

		ret = gpio_request(pdata->gpio_reset, "maxim_sti_reset");
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_reset, "request");
		ret = gpio_direction_output(pdata->gpio_reset,
					    pdata->default_reset_state);
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_reset, "direction");
	} else {
		gpio_free(pdata->gpio_irq);
		gpio_free(pdata->gpio_reset);
	}

	return 0;
}

static void maxim_sti_reset(struct maxim_sti_pdata *pdata, int value)
{
	gpio_set_value(pdata->gpio_reset, !!value);
}

static int maxim_sti_irq(struct maxim_sti_pdata *pdata)
{
	return gpio_get_value(pdata->gpio_irq);
}

void __init touch_init_maxim_sti(struct spi_board_info *spi_board)
{
	struct maxim_sti_pdata  *pdata;

	pdata = (struct maxim_sti_pdata *)spi_board->platform_data;
	pdata->init  = maxim_sti_init;
	pdata->reset = maxim_sti_reset;
	pdata->irq   = maxim_sti_irq;

	spi_board->irq = gpio_to_irq(pdata->gpio_irq);

	spi_register_board_info(spi_board, 1);
}

#endif

