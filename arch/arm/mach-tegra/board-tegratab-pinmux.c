/*
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
#include "board.h"
#include "board-tegratab.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra-board-id.h"

#include <mach/pinmux-t11.h>

static __initdata struct tegra_drive_pingroup_config tegratab_drive_pinmux[] = {
	/* DEFAULT_DRIVE(<pin_group>), */
	/* SDMMC1 */
	SET_DRIVE(SDIO1, ENABLE, DISABLE, DIV_1, 36, 20, SLOW, SLOW),

	/* SDMMC3 */
	SET_DRIVE(SDIO3, ENABLE, DISABLE, DIV_1, 22, 36, FASTEST, FASTEST),

	/* SDMMC4 */
	SET_DRIVE_WITH_TYPE(GMA, ENABLE, DISABLE, DIV_1, 2, 2, FASTEST,
								FASTEST, 1),
};

#include "board-tegratab-pinmux-t11x.h"

/* THIS IS FOR TESTING OR WORKAROUND PURPOSES. ANYTHING INSIDE THIS TABLE
 * SHOULD BE PUSHED TO PINMUX SPREADSHEET FOR AUTOGEN OR FIXED
 * */
static __initdata struct tegra_pingroup_config manual_config_pinmux[] = {

	/* ULPI SFIOs are not supposed to be supported.
	 * This setting is only for Tegratab. */
	DEFAULT_PINMUX(ULPI_DATA0,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA1,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA5,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA6,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA7,    ULPI,        NORMAL,    NORMAL,   INPUT),
};

static __initdata struct tegra_pingroup_config dvt_a00_manual_config_pinmux[] = {
	GPIO_PINMUX(KB_COL1, PULL_UP, NORMAL, INPUT, DISABLE), /* hall sensor input */
};

static struct gpio_init_pin_info dvt_a00_manual_gpio_mode[] = {
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PQ1, true, 0), /* hall sensor input */
};

static void __init tegratab_gpio_init_configure(void)
{
	int len;
	int i;
	struct gpio_init_pin_info *pins_info;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_P1640) {
		len = ARRAY_SIZE(init_gpio_mode_tegratab_ffd_common);
		pins_info = init_gpio_mode_tegratab_ffd_common;
	} else { /* ERS */
		len = ARRAY_SIZE(init_gpio_mode_tegratab_common);
		pins_info = init_gpio_mode_tegratab_common;
	}

	for (i = 0; i < len; ++i) {
		tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
		pins_info++;
	}

	if (board_info.board_id == BOARD_P1640) {
		len = ARRAY_SIZE(dvt_a00_manual_gpio_mode);
		pins_info = dvt_a00_manual_gpio_mode;
		for (i = 0; i < len; ++i) {
			tegra_gpio_init_configure(pins_info->gpio_nr,
				pins_info->is_input, pins_info->value);
			pins_info++;
		}
	}

}

int __init tegratab_pinmux_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	tegratab_gpio_init_configure();

	tegra_drive_pinmux_config_table(tegratab_drive_pinmux,
					ARRAY_SIZE(tegratab_drive_pinmux));

	if (board_info.board_id == BOARD_P1640) {
		tegra_pinmux_config_table(tegratab_ffd_pinmux_common,
					ARRAY_SIZE(tegratab_ffd_pinmux_common));
		tegra_pinmux_config_table(ffd_unused_pins_lowpower,
					ARRAY_SIZE(ffd_unused_pins_lowpower));
		tegra_pinmux_config_table(dvt_a00_manual_config_pinmux,
					ARRAY_SIZE(dvt_a00_manual_config_pinmux));
	} else { /* ERS */
		tegra_pinmux_config_table(tegratab_pinmux_common,
					ARRAY_SIZE(tegratab_pinmux_common));
		tegra_pinmux_config_table(unused_pins_lowpower,
					ARRAY_SIZE(unused_pins_lowpower));
	}

	tegra_pinmux_config_table(manual_config_pinmux,
		ARRAY_SIZE(manual_config_pinmux));

	return 0;
}
