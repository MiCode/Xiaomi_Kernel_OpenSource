/*
 * arch/arm/mach-tegra/board-touch-synaptics-spi.c
 *
 * Copyright (C) 2010-2012 NVIDIA Corporation
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

#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/rmi.h>

#include <mach/gpio-tegra.h>

#include "board.h"
#include "board-kai.h"

#define SYNAPTICS_SPI_CS 0
#define SYNAPTICS_BUTTON_CODES {KEY_HOME, KEY_BACK,}

static unsigned char synaptics_button_codes[] = SYNAPTICS_BUTTON_CODES;

static struct rmi_f19_button_map synaptics_button_map = {
	.nbuttons = ARRAY_SIZE(synaptics_button_codes),
	.map = synaptics_button_codes,
};

static int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	if (configure) {
		gpio_request(SYNAPTICS_ATTN_GPIO, "synaptics-irq");
		gpio_direction_input(SYNAPTICS_ATTN_GPIO);

		gpio_request(SYNAPTICS_RESET_GPIO, "synaptics-reset");
		gpio_direction_output(SYNAPTICS_RESET_GPIO, 0);

		msleep(1);
		gpio_set_value(SYNAPTICS_RESET_GPIO, 1);
		msleep(100);
	} else {
		gpio_free(SYNAPTICS_ATTN_GPIO);
		gpio_free(SYNAPTICS_RESET_GPIO);
	}
	return 0;
}

static struct rmi_device_platform_data synaptics_platformdata = {
	.driver_name = "rmi_generic",
	.irq = SYNAPTICS_ATTN_GPIO,
	.irq_polarity = RMI_IRQ_ACTIVE_LOW,
	.gpio_config = synaptics_touchpad_gpio_setup,
	.spi_data = {
		.block_delay_us = 15,
		.read_delay_us = 15,
		.write_delay_us = 2,
	},
	.axis_align = {
		.flip_y = true,
	},
	.button_map = &synaptics_button_map,
};

struct spi_board_info synaptics_2002_spi_board[] = {
	{
		.modalias = "rmi_spi",
		.bus_num = 0,
		.chip_select = 0,
		.irq = 999,  /* just to make sure this one is not being used */
		.max_speed_hz = 1*1000*1000,
		.mode = SPI_MODE_3,
		.platform_data = &synaptics_platformdata,
	},
};

int __init touch_init_synaptics_kai(void)
{
	pr_info("%s: registering synaptics_2002_spi_board\n", __func__);
	pr_info("		modalias     = %s\n",
					synaptics_2002_spi_board->modalias);
	pr_info("		bus_num      = %d\n",
					synaptics_2002_spi_board->bus_num);
	pr_info("		chip_select  = %d\n",
					synaptics_2002_spi_board->chip_select);
	pr_info("		irq          = %d\n",
					synaptics_2002_spi_board->irq);
	pr_info("		max_speed_hz = %d\n",
					synaptics_2002_spi_board->max_speed_hz);
	pr_info("		mode         = %d\n",
					synaptics_2002_spi_board->mode);

	msleep(100);
	spi_register_board_info(synaptics_2002_spi_board,
					ARRAY_SIZE(synaptics_2002_spi_board));
	return 0;
}
