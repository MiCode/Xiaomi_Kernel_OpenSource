/*
 * arch/arm/mach-tegra/board-loki-pinmux.c
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
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
#include <linux/init.h>
#include <linux/gpio.h>
#include <mach/pinmux.h>
#include <mach/gpio-tegra.h>

#include "board.h"
#include "board-loki.h"
#include "tegra-board-id.h"
#include "devices.h"
#include "gpio-names.h"

#include <mach/pinmux-t12.h>

static __initdata struct tegra_drive_pingroup_config loki_drive_pinmux[] = {

	/* SDMMC1 */
	SET_DRIVE(SDIO1, ENABLE, DISABLE, DIV_1, 54, 70, FASTEST, FASTEST),

	/* SDMMC3 */
	SET_DRIVE(SDIO3, ENABLE, DISABLE, DIV_1, 20, 42, FASTEST, FASTEST),

	/* SDMMC4 */
	SET_DRIVE_WITH_TYPE(GMA, ENABLE, DISABLE, DIV_1, 1, 2, FASTEST,
								FASTEST, 1),
};

/* Pinmux changes to support UART over uSD adapter E2542 */
static __initdata struct tegra_pingroup_config loki_sdmmc3_uart_pinmux[] = {

	DEFAULT_PINMUX(SDMMC3_CMD,    UARTA,      NORMAL,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT1,   UARTA,      NORMAL,   NORMAL,   OUTPUT),
};

#include "board-loki-pinmux-t12x.h"

static void __init loki_gpio_init_configure(void)
{
	int len;
	int i;
	struct gpio_init_pin_info *pins_info;
	struct board_info bi;

	tegra_get_board_info(&bi);

	len = ARRAY_SIZE(init_gpio_mode_loki_common);
	pins_info = init_gpio_mode_loki_common;

	for (i = 0; i < len; ++i) {
		tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
		pins_info++;
	}

	if (bi.board_id == BOARD_P2530) {
		len = ARRAY_SIZE(init_gpio_mode_loki_ffd_common);
		pins_info = init_gpio_mode_loki_ffd_common;

		for (i = 0; i < len; ++i) {
			tegra_gpio_init_configure(pins_info->gpio_nr,
				pins_info->is_input, pins_info->value);
			pins_info++;
		}
	}
}

int __init loki_pinmux_init(void)
{
	struct board_info bi;

	tegra_get_board_info(&bi);

	loki_gpio_init_configure();

	tegra_pinmux_config_table(loki_pinmux_common, ARRAY_SIZE(loki_pinmux_common));

	if (bi.board_id == BOARD_P2530)
		tegra_pinmux_config_table(loki_ffd_pinmux_common,
			ARRAY_SIZE(loki_ffd_pinmux_common));

	tegra_drive_pinmux_config_table(loki_drive_pinmux,
					ARRAY_SIZE(loki_drive_pinmux));
	tegra_pinmux_config_table(unused_pins_lowpower,
		ARRAY_SIZE(unused_pins_lowpower));

	if (is_uart_over_sd_enabled()) {
		tegra_pinmux_config_table(loki_sdmmc3_uart_pinmux,
				 ARRAY_SIZE(loki_sdmmc3_uart_pinmux));
		/* On loki, UART-A is the physical device for
		 * UART over uSD card
		 */
		set_sd_uart_port_id(0);
	}
	return 0;
}
