/*
 * i2c-algo-busclear: Bus clear logic implementation based on GPIO
 *
 * Copyright (c) 2013, NVIDIA Corporation.
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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/export.h>

/*I2C controller supports eight-byte burst transfer*/
#define RETRY_MAX_COUNT (9 * 8 + 1)

int i2c_algo_busclear_gpio(struct device *dev, int scl_gpio, int scl_gpio_flags,
	int sda_gpio, int sda_gpio_flags,
	int max_retry_clock, int clock_speed_hz)
{
	int ret;
	int retry_clk = min(max_retry_clock, RETRY_MAX_COUNT);
	int recovered_successfully = 0;
	int val;
	int clk_delay = DIV_ROUND_UP(1000000, 2 * clock_speed_hz);
	int clk_count = 0;

	if ((!gpio_is_valid(scl_gpio)) || !gpio_is_valid((sda_gpio))) {
		dev_err(dev, "GPIOs are not valid\n");
		return -EINVAL;
	}

	ret = gpio_request_one(scl_gpio,
		scl_gpio_flags | GPIOF_OUT_INIT_HIGH, "scl_gpio");
	if (ret < 0) {
		dev_err(dev, "GPIO request for scl_gpio failed, err = %d\n",
			ret);
		return ret;
	}

	ret = gpio_request_one(sda_gpio,
			sda_gpio_flags | GPIOF_IN, "sda_gpio");
	if (ret < 0) {
		dev_err(dev, "GPIO request for sda_gpio failed, err = %d\n",
			ret);
		gpio_free(scl_gpio);
		return ret;
	}

	gpio_direction_input(sda_gpio);
	while (retry_clk--) {
		clk_count++;
		gpio_direction_output(scl_gpio, 0);
		udelay(clk_delay);
		gpio_direction_output(scl_gpio, 1);
		udelay(clk_delay);

		/* check whether sda struct low release */
		val = gpio_get_value(sda_gpio);
		if (val) {
			/* send START */
			gpio_direction_output(sda_gpio, 0);
			udelay(clk_delay);

			/* send STOP in next clock cycle */
			gpio_direction_output(scl_gpio, 0);
			udelay(clk_delay);
			gpio_direction_output(scl_gpio, 1);
			udelay(clk_delay);
			gpio_direction_output(sda_gpio, 1);
			udelay(clk_delay);
			recovered_successfully = 1;
			break;
		}
	}

	gpio_free(scl_gpio);
	gpio_free(sda_gpio);

	if (!recovered_successfully) {
		dev_err(dev, "arb lost recovered failed with total clk %d\n",
			clk_count);
		return -EINVAL;
	}
	dev_err(dev, "arb lost recovered in clock count %d\n", clk_count);
	return 0;
}
EXPORT_SYMBOL_GPL(i2c_algo_busclear_gpio);
