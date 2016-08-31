/*
 * arch/arm/mach-tegra/board-vcm30_t124-pinmux.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/init.h>
#include <linux/gpio.h>
#include <mach/pinmux.h>
#include <mach/gpio-tegra.h>
#include <mach/pinmux-t12.h>
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>

#include "board.h"
#include "board-vcm30_t124.h"
#include "devices.h"
#include "gpio-names.h"

#include "board-vcm30_t124-pinmux-t12x.h"

/* FIXME: Check these drive strengths for VCM30_T124. */
static __initdata
struct tegra_drive_pingroup_config vcm30_t124_drive_pinmux[] = {

	/*Set DAP2 drive (required for Codec Master Mode)*/
	SET_DRIVE(DAP2, DISABLE, ENABLE, DIV_1, 51, 51, FASTEST, FASTEST),

	/* SDMMC1 */
	SET_DRIVE(SDIO1, ENABLE, DISABLE, DIV_1, 54, 70, FASTEST, FASTEST),

	/* SDMMC3 */
	SET_DRIVE(SDIO3, ENABLE, DISABLE, DIV_1, 20, 42, FASTEST, FASTEST),

	/* SDMMC4 */
	SET_DRIVE_WITH_TYPE(GMA, ENABLE, DISABLE, DIV_1, 1, 2, FASTEST,
			FASTEST, 1),
};


static void __init vcm30_t124_gpio_init_configure(void)
{
	int len;
	int i;
	struct gpio_init_pin_info *pins_info;

	len = ARRAY_SIZE(init_gpio_mode_vcm30_t124_common);
	pins_info = init_gpio_mode_vcm30_t124_common;

	for (i = 0; i < len; ++i) {
		tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
		pins_info++;
	}
}

int __init vcm30_t124_pinmux_init(void)
{
	vcm30_t124_gpio_init_configure();

	tegra_pinmux_config_table(vcm30_t124_pinmux_common,
					ARRAY_SIZE(vcm30_t124_pinmux_common));

	tegra_drive_pinmux_config_table(vcm30_t124_drive_pinmux,
					ARRAY_SIZE(vcm30_t124_drive_pinmux));
	tegra_pinmux_config_table(unused_pins_lowpower,
		ARRAY_SIZE(unused_pins_lowpower));

	return 0;
}

/*
 * GPIO init table for PCA9539 MISC IO GPIOs
 * that have to be brought up to a known good state
 * except for WiFi as it is handled via the
 * WiFi stack.
 */
static struct gpio vcm30_t124_system_gpios[] = {
	{MISCIO_BT_WAKEUP_GPIO,	GPIOF_OUT_INIT_HIGH,	"bt_wk"},
	{MISCIO_ABB_RST_GPIO,	GPIOF_OUT_INIT_HIGH,	"ebb_rst"},
	{MISCIO_USER_LED2_GPIO,	GPIOF_OUT_INIT_LOW,	"usr_led2"},
	{MISCIO_USER_LED1_GPIO, GPIOF_OUT_INIT_LOW,	"usr_led1"},
};

static int __init vcm30_t124_system_gpio_init(void)
{
	int ret, pin_count = 0;
	struct gpio *gpios_info = NULL;
	gpios_info = vcm30_t124_system_gpios;
	pin_count = ARRAY_SIZE(vcm30_t124_system_gpios);

	/* Set required system GPIOs to initial bootup values */
	ret = gpio_request_array(gpios_info, pin_count);

	if (ret)
		pr_err("%s gpio_request_array failed(%d)\r\n",
				 __func__, ret);

	/* Export the LED GPIOs to userland for any check */
	gpio_export(MISCIO_USER_LED2_GPIO, false);
	gpio_export(MISCIO_USER_LED1_GPIO, false);

	return ret;
}

/*
 * TODO: Check for the correct pca953x before invoking client
 *  init functions
 */
static int pca953x_client_setup(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context)
{
	int ret = 0;

	ret = vcm30_t124_system_gpio_init();
	if (ret < 0)
		goto fail;

	return 0;
fail:
	pr_err("%s failed(%d)\r\n", __func__, ret);
	return ret;
}

static struct pca953x_platform_data vcm30_t124_miscio_pca9539_data = {
	.gpio_base  = PCA953X_MISCIO_GPIO_BASE,
	.setup = pca953x_client_setup,
};

static struct i2c_board_info vcm30_t124_i2c2_board_info_pca9539[] = {
	{
		I2C_BOARD_INFO("pca9539", PCA953X_MISCIO_ADDR),
		.platform_data = &vcm30_t124_miscio_pca9539_data,
	},
};

int __init vcm30_t124_pca953x_init(void)
{
	i2c_register_board_info(1, vcm30_t124_i2c2_board_info_pca9539,
		ARRAY_SIZE(vcm30_t124_i2c2_board_info_pca9539));
	return 0;
}
