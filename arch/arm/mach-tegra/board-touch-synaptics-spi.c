/*
 * arch/arm/mach-tegra/board-touch-raydium_spi.c
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 * Copyright (c) 2012, Synaptics Inc
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

#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "board-touch.h"

#define SYNAPTICS_SPI_CS 0
#define SYNAPTICS_BUTTON_CODES {KEY_HOME, KEY_BACK,}

static unsigned char synaptics_button_codes[] = SYNAPTICS_BUTTON_CODES;

struct rmi_button_map synaptics_button_map = {
	.nbuttons = ARRAY_SIZE(synaptics_button_codes),
	.map = synaptics_button_codes,
};

int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	struct synaptics_gpio_data *syna_gpio_data =
		(struct synaptics_gpio_data *)gpio_data;

	if (!gpio_data)
		return -EINVAL;

	pr_info("%s: called with configure=%d, SYNAPTICS_ATTN_GPIO=%d\n",
		__func__, configure, syna_gpio_data->attn_gpio);

	if (configure) {
		gpio_request(syna_gpio_data->attn_gpio, "synaptics-irq");
		gpio_direction_input(syna_gpio_data->attn_gpio);

		gpio_request(syna_gpio_data->reset_gpio, "synaptics-reset");
		gpio_direction_output(syna_gpio_data->reset_gpio, 0);

		msleep(20);
		gpio_set_value(syna_gpio_data->reset_gpio, 1);
		msleep(100);
	} else {
		gpio_free(syna_gpio_data->attn_gpio);
		gpio_free(syna_gpio_data->reset_gpio);
	}
	return 0;
}

int __init touch_init_synaptics(struct spi_board_info *board_info,
						int board_info_size)
{
	msleep(100);

	pr_info("%s: registering synaptics_spi_board\n", __func__);
	pr_info("               modalias     = %s\n",
					board_info->modalias);
	pr_info("               bus_num      = %d\n",
					board_info->bus_num);
	pr_info("               chip_select  = %d\n",
					board_info->chip_select);
	pr_info("               irq          = %d\n",
					board_info->irq);
	pr_info("               max_speed_hz = %d\n",
					board_info->max_speed_hz);
	pr_info("               mode         = %d\n",
					board_info->mode);

	spi_register_board_info(board_info, board_info_size);
	return 0;
}
