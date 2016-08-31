/*
 * arch/arm/mach-tegra/board-pismo-pinmux.c
 *
 * Copyright (C) 2012 NVIDIA Corporation
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
#include "board-pismo.h"
#include "devices.h"
#include "gpio-names.h"

#include <mach/pinmux-t11.h>

static __initdata struct tegra_drive_pingroup_config pismo_drive_pinmux[] = {
	/* DEFAULT_DRIVE(<pin_group>), */
	/* SDMMC1 */
	SET_DRIVE(SDIO1, ENABLE, DISABLE, DIV_1, 36, 20, SLOW, SLOW),

	/* SDMMC3 */
	SET_DRIVE(SDIO3, ENABLE, DISABLE, DIV_1, 22, 36, FASTEST, FASTEST),

	/* SDMMC4 */
	SET_DRIVE_WITH_TYPE(GMA, ENABLE, DISABLE, DIV_1, 2, 1, FASTEST,
								FASTEST, 1),
};

/* Initially setting all used GPIO's to non-TRISTATE */
static __initdata struct tegra_pingroup_config pismo_pinmux_set_nontristate[] = {
	DEFAULT_PINMUX(CLK1_REQ,        RSVD3,  PULL_DOWN,    NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_X4_AUD,     RSVD,   PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_X5_AUD,     RSVD,   PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_X6_AUD,     RSVD3,  PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_X7_AUD,     RSVD,   PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_W2_AUD,     RSVD1,  PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_W3_AUD,     SPI6,   PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_X1_AUD,     RSVD3,  PULL_DOWN,    NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_X3_AUD,     RSVD3,  PULL_UP,      NORMAL,    INPUT),

	DEFAULT_PINMUX(DAP3_FS,         I2S2,   PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(DAP3_DIN,        I2S2,   PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(DAP3_DOUT,       I2S2,   PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(DAP3_SCLK,       I2S2,   PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PV0,        RSVD3,  NORMAL,       NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_PV1,        RSVD,   NORMAL,       NORMAL,    INPUT),

	DEFAULT_PINMUX(GPIO_PBB3,       RSVD3,  PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB5,       RSVD3,  PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB6,       RSVD3,  PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB7,       RSVD3,  PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PCC1,       RSVD3,  PULL_DOWN,    NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_PCC2,       RSVD3,  PULL_DOWN,    NORMAL,    INPUT),

	DEFAULT_PINMUX(GMI_AD0,         GMI,    NORMAL,       NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_AD1,         GMI,    NORMAL,       NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_AD10,        GMI,    PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_AD11,        GMI,    PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_AD12,        GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_AD13,        GMI,    PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_AD2,         GMI,    NORMAL,       NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_AD3,         GMI,    NORMAL,       NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_AD8,         GMI,    PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_ADV_N,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_CLK,         GMI,    PULL_DOWN,    NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_CS0_N,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_CS2_N,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_CS3_N,       GMI,    PULL_UP,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GMI_CS4_N,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_CS7_N,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_DQS_P,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_IORDY,       GMI,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GMI_WP_N,        GMI,    PULL_UP,      NORMAL,    INPUT),

	DEFAULT_PINMUX(SDMMC1_WP_N,     SPI4,   PULL_UP,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(CLK2_REQ,        RSVD3,  NORMAL,       NORMAL,    OUTPUT),

	DEFAULT_PINMUX(KB_COL3,         KBC,    PULL_UP,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(KB_COL5,         KBC,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(KB_COL6,         KBC,    PULL_UP,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(KB_COL7,         KBC,    PULL_UP,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(KB_ROW3,         KBC,    PULL_DOWN,    NORMAL,    INPUT),
	DEFAULT_PINMUX(KB_ROW4,         KBC,    PULL_DOWN,    NORMAL,    INPUT),
	DEFAULT_PINMUX(KB_ROW6,         KBC,    PULL_DOWN,    NORMAL,    INPUT),
	DEFAULT_PINMUX(KB_ROW7,         KBC,    PULL_UP,      NORMAL,    INPUT),
	DEFAULT_PINMUX(KB_ROW8,         KBC,    PULL_UP,      NORMAL,    INPUT),

	DEFAULT_PINMUX(CLK3_REQ,        RSVD3,  NORMAL,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PU4,        RSVD3,  NORMAL,      NORMAL,    OUTPUT),
	DEFAULT_PINMUX(GPIO_PU5,        RSVD3,  NORMAL,      NORMAL,    INPUT),
	DEFAULT_PINMUX(GPIO_PU6,        RSVD3,  NORMAL,      NORMAL,    INPUT),

	DEFAULT_PINMUX(HDMI_INT,        RSVD,   PULL_DOWN,    NORMAL,    INPUT),

	DEFAULT_PINMUX(GMI_AD9,         PWM1,   NORMAL,    NORMAL,     OUTPUT),
};

#include "board-pismo-pinmux-t11x.h"

/* THIS IS FOR EXPERIMENTAL OR WORKAROUND PURPOSES. ANYTHING INSIDE THIS TABLE
 * SHOULD BE CONSIDERED TO BE PUSHED TO PINMUX SPREADSHEET FOR CONSISTENCY
 */
static __initdata struct tegra_pingroup_config manual_config_pinmux[] = {
};

static void __init pismo_gpio_init_configure(void)
{
	int len;
	int i;
	struct gpio_init_pin_info *pins_info;

	len = ARRAY_SIZE(init_gpio_mode_pismo_common);
	pins_info = init_gpio_mode_pismo_common;

	for (i = 0; i < len; ++i) {
		tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
		pins_info++;
	}
}

int __init pismo_pinmux_init(void)
{
	tegra_pinmux_config_table(pismo_pinmux_set_nontristate,
				ARRAY_SIZE(pismo_pinmux_set_nontristate));
	pismo_gpio_init_configure();

	tegra_pinmux_config_table(pismo_pinmux_common,
			ARRAY_SIZE(pismo_pinmux_common));
	tegra_drive_pinmux_config_table(pismo_drive_pinmux,
					ARRAY_SIZE(pismo_drive_pinmux));
	tegra_pinmux_config_table(manual_config_pinmux,
		ARRAY_SIZE(manual_config_pinmux));
	tegra_pinmux_config_table(unused_pins_lowpower,
		ARRAY_SIZE(unused_pins_lowpower));

	return 0;
}
