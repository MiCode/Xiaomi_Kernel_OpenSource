/*
 * arch/arm/mach-tegra/board-touch-raydium_spi.c
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/rm31080a_ts.h>

/*#include <mach/gpio-tegra.h>*/

int __init touch_init_raydium(int irq_gpio, int reset_gpio,
				struct rm_spi_ts_platform_data *rm31080ts_data,
				struct spi_board_info *rm31080a_spi_board,
				int asize)
{
	int err = 0;
	gpio_request(irq_gpio, "raydium-irq");
	gpio_direction_input(irq_gpio);

	gpio_request(reset_gpio, "raydium-reset");
	gpio_direction_output(reset_gpio, 0);

	rm31080ts_data->gpio_reset = reset_gpio;

	rm31080a_spi_board->irq = gpio_to_irq(irq_gpio);

	spi_register_board_info(rm31080a_spi_board, asize);
	pr_info("Raydium touch platform_id:  %d\n",
			rm31080ts_data->platform_id);

	return err;
}
