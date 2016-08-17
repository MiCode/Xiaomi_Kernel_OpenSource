/*
 * arch/arm/mach-tegra/i2c_error_recovery.c
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
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <mach/gpio-tegra.h>

#include "board.h"

#define RETRY_MAX_COUNT (9*8+1) /*I2C controller supports eight-byte burst transfer*/

int arb_lost_recovery(int scl_gpio, int sda_gpio)
{
	int ret;
	int retry = RETRY_MAX_COUNT;
	int recovered_successfully = 0;
	int val;

	if ((!scl_gpio) || (!sda_gpio)) {
		pr_err("not proper input:scl_gpio 0x%08x,"
			"sda_gpio 0x%08x\n", scl_gpio, sda_gpio);
		return -EINVAL;;
	}

	ret = gpio_request(scl_gpio, "scl_gpio");
	if (ret < 0) {
		pr_err("error in gpio 0x%08x request 0x%08x\n",
			scl_gpio, ret);
		return -EINVAL;;
	}

	ret = gpio_request(sda_gpio, "sda_gpio");
	if (ret < 0) {
		pr_err("error in gpio 0x%08x request 0x%08x\n",
			sda_gpio, ret);
		goto err;
	}
	gpio_direction_input(sda_gpio);

	while (retry--) {
		gpio_direction_output(scl_gpio,0);
		udelay(5);
		gpio_direction_output(scl_gpio,1);
		udelay(5);

		/* check whether sda struct low release */
		val = gpio_get_value(sda_gpio);
		if (val) {
			/* send START */
			gpio_direction_output(sda_gpio,0);
			udelay(5);

			/* send STOP in next clock cycle */
			gpio_direction_output(scl_gpio,0);
			udelay(5);
			gpio_direction_output(scl_gpio,1);
			udelay(5);
			gpio_direction_output(sda_gpio,1);
			udelay(5);

			recovered_successfully = 1;
			break;
		}
	}

	gpio_free(scl_gpio);
	gpio_free(sda_gpio);

	if (likely(recovered_successfully)) {
		pr_err("arbitration lost recovered by re-try-count 0x%08x\n",
			RETRY_MAX_COUNT - retry);
		return 0;
	} else {
		pr_err("Un-recovered arbitration lost.\n");
		return -EINVAL;
	}

err:
	gpio_free(scl_gpio);
	return ret;
}

