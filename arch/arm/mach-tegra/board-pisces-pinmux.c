/*
 * arch/arm/mach-tegra/board-pisces-pinmux.c
 *
 * Copyright (C) 2012 NVIDIA Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include "board-pisces.h"
#include "devices.h"
#include "gpio-names.h"

#include <mach/pinmux-t11.h>

static __initdata struct tegra_drive_pingroup_config pluto_drive_pinmux[] = {
	/* DEFAULT_DRIVE(<pin_group>), */
	SET_DRIVE(DAP2, DISABLE, ENABLE, DIV_1, 31, 31, FASTEST, FASTEST),

	/* SDMMC1 */
	SET_DRIVE(SDIO1, ENABLE, DISABLE, DIV_1, 36, 20, SLOW, SLOW),

	/* SDMMC3 */
	SET_DRIVE(SDIO3, ENABLE, DISABLE, DIV_1, 22, 36, FASTEST, FASTEST),

	/* SDMMC4 */
	SET_DRIVE_WITH_TYPE(GMA, ENABLE, DISABLE, DIV_1, 2, 1, FASTEST,
								FASTEST, 1),
};

/* Initially setting all used GPIO's to non-TRISTATE */
static __initdata struct tegra_pingroup_config pluto_pinmux_set_nontristate[] = {
	/* Mi3_PinMux_P0.1_V0.3_macro_enabled.xlsm */
	DEFAULT_PINMUX(GPIO_X6_AUD, RSVD3,   PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GPIO_X7_AUD, RSVD,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_W2_AUD, RSVD1,   NORMAL, NORMAL, INPUT),
	DEFAULT_PINMUX(GPIO_W3_AUD, SPI6,    PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GPIO_X3_AUD, RSVD3,   PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(ULPI_DATA5, ULPI,     NORMAL,  NORMAL, OUTPUT),
	DEFAULT_PINMUX(ULPI_DATA6, ULPI,     NORMAL,  NORMAL, INPUT),
	DEFAULT_PINMUX(ULPI_NXT, ULPI,	NORMAL, NORMAL, INPUT),
	DEFAULT_PINMUX(ULPI_STP, ULPI,	NORMAL, NORMAL, INPUT),
	DEFAULT_PINMUX(GPIO_PBB3, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB4, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB5, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB7, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_PCC1, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_PCC2, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD1, GMI,      NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD7, GMI,      NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD6, GMI,      NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD10, GMI,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD11, GMI,      NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD12, GMI,      PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD13, GMI,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD14, GMI,      PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_AD15, GMI,      NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_ADV_N, GMI,      PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_CLK, GMI,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_CS0_N, GMI,      PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_CS1_N, GMI,      PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_CS3_N, GMI,      PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_CS4_N, GMI,      PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_CS6_N, GMI,      PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_CS7_N, GMI,      NORMAL, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_DQS_P, GMI,      PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_RST_N, GMI,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GMI_WAIT, GMI,	PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(GMI_WP_N, GMI, 	PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(CLK2_OUT, RSVD3,    PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(CLK2_REQ, RSVD3,     NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(SDMMC1_WP_N, SDMMC1, PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(CLK_32K_OUT, SOC,    PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(KB_COL3, KBC,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(KB_COL4, KBC,      PULL_UP, NORMAL, INPUT),
	DEFAULT_PINMUX(KB_COL5, KBC,      PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(KB_COL7, KBC,      PULL_UP, NORMAL, OUTPUT),
	DEFAULT_PINMUX(KB_ROW3, KBC,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(KB_ROW6, KBC,      PULL_DOWN, NORMAL, INPUT),
	DEFAULT_PINMUX(KB_ROW7, KBC,      PULL_DOWN, NORMAL, INPUT),
	DEFAULT_PINMUX(KB_ROW8, KBC,      PULL_DOWN, NORMAL, INPUT),
	DEFAULT_PINMUX(KB_ROW9, KBC,      PULL_DOWN, NORMAL, OUTPUT),
	DEFAULT_PINMUX(CLK3_REQ, RSVD3,    NORMAL, NORMAL, OUTPUT),
	DEFAULT_PINMUX(GPIO_PU5, DISPLAYB,     NORMAL, NORMAL, INPUT),
	DEFAULT_PINMUX(GPIO_PU6, DISPLAYB,     NORMAL, NORMAL, INPUT),
	DEFAULT_PINMUX(HDMI_INT, RSVD,      PULL_DOWN, NORMAL, INPUT),
	DEFAULT_PINMUX_OD_DISABLE(USB_VBUS_EN0, RSVD3,     PULL_DOWN, NORMAL, OUTPUT),
};

#ifdef CONFIG_SC8800G_MODEM
static __initdata struct tegra_pingroup_config sc8800g_pluto_pinmux_set_nontristate[] = {
	DEFAULT_PINMUX(SDMMC3_CLK,			SDMMC3,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(KB_ROW1,				KBC,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(SDMMC3_CLK_LB_OUT,	SDMMC3,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GMI_AD8,				GMI,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GMI_A18,				GMI,	PULL_DOWN,	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GPIO_PV1,			RSVD,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(KB_ROW5,				KBC,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GPIO_X1_AUD,			RSVD3,	PULL_DOWN,	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GPIO_PBB6,			RSVD3,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(KB_ROW4,				KBC,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(KB_ROW2,				KBC,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(KB_ROW10,			KBC,	PULL_DOWN, 	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GPIO_X4_AUD,			RSVD, 	PULL_DOWN,	TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GPIO_PV0,			RSVD3,	PULL_DOWN, 	TRISTATE, OUTPUT),
};
#endif

#ifdef CONFIG_TEGRA_BB_XMM_POWER
static __initdata struct tegra_pingroup_config xmm636_pluto_pinmux_set_nontristate[] = {
	DEFAULT_PINMUX(GMI_A18,         GMI,    PULL_UP,        NORMAL, OUTPUT),/* bp power on*/
	DEFAULT_PINMUX(GPIO_X1_AUD,     RSVD3,  PULL_DOWN,      NORMAL, OUTPUT),/* bb on key*/
	DEFAULT_PINMUX(ULPI_DIR,       ULPI,    PULL_DOWN,      NORMAL, OUTPUT),/* bb x_off*/
	DEFAULT_PINMUX(KB_ROW5,         KBC,    PULL_UP,        NORMAL, OUTPUT),/* bb reset*/
	DEFAULT_PINMUX(ULPI_CLK,        ULPI,   PULL_DOWN,      NORMAL, OUTPUT),/* host active*/
	DEFAULT_PINMUX(GPIO_PV0,        RSVD3,  NORMAL,         NORMAL, INPUT),/* host wakeup*/
	DEFAULT_PINMUX(KB_ROW10,        KBC,    NORMAL,         NORMAL, INPUT),/* host suepend request*/
	DEFAULT_PINMUX(KB_ROW1,         KBC,    PULL_UP,        NORMAL, OUTPUT),/* slave wakeup*/
	DEFAULT_PINMUX(ULPI_NXT,         ULPI,    PULL_DOWN,        NORMAL, INPUT),/* ipc_mdm2ap*/
	DEFAULT_PINMUX(GMI_AD8,         GMI,    PULL_DOWN,      NORMAL, OUTPUT),/* ipc_ap2mdm*/
	DEFAULT_PINMUX(GPIO_PV1,       RSVD,    PULL_DOWN,        NORMAL, INPUT),/* ipc reset2_N*/
	DEFAULT_PINMUX(KB_ROW4,       KBC,    PULL_DOWN,        NORMAL, INPUT),/*core dump int*/
	DEFAULT_PINMUX(GPIO_PBB6,       RSVD3,    PULL_DOWN,        NORMAL, INPUT),/*mdm ready*/
	DEFAULT_PINMUX(KB_COL6,         KBC,    PULL_DOWN,      TRISTATE, OUTPUT),/* not used in imc modem*/
	DEFAULT_PINMUX(ULPI_DATA4,      ULPI,   PULL_DOWN,      TRISTATE, OUTPUT),/* not used in imc modem*/
	DEFAULT_PINMUX(ULPI_DATA7,      ULPI,   PULL_UP,        TRISTATE, OUTPUT),/* not used in imc modem*/
	DEFAULT_PINMUX(KB_ROW2,         KBC,    NORMAL,         TRISTATE, INPUT),/* not used in imc modem*/
	DEFAULT_PINMUX(GPIO_X5_AUD,     RSVD,   NORMAL,         TRISTATE, INPUT),/* not used in imc modem*/

};
#endif
#include "board-pisces-pinmux-t11x.h"

#ifdef CONFIG_PM_SLEEP
/* pinmux settings during low power mode for power saving purpose */
static struct tegra_pingroup_config pluto_sleep_pinmux[] = {
};
#endif

/* THIS IS FOR EXPERIMENTAL OR WORKAROUND PURPOSES. ANYTHING INSIDE THIS TABLE
 * SHOULD BE CONSIDERED TO BE PUSHED TO PINMUX SPREADSHEET FOR CONSISTENCY
 */
static __initdata struct tegra_pingroup_config manual_config_pinmux[] = {
};

static void __init pluto_gpio_init_configure(void)
{
	int len;
	int i;
	struct gpio_init_pin_info *pins_info;

	len = ARRAY_SIZE(init_gpio_mode_pluto_common);
	pins_info = init_gpio_mode_pluto_common;

	for (i = 0; i < len; ++i) {
		tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
		pins_info++;
	}
	if (TEGRA_BB_SPRD == tegra_get_modem_id()) {
		len = ARRAY_SIZE(sc8800g_init_gpio_mode_pluto_common);
		pins_info = sc8800g_init_gpio_mode_pluto_common;
		for (i = 0; i < len; ++i) {
			tegra_gpio_init_configure(pins_info->gpio_nr,
			pins_info->is_input, pins_info->value);
			pins_info++;
		}
	}
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	else if (TEGRA_BB_IFX == tegra_get_modem_id()) {
		len = ARRAY_SIZE(xmm636_init_gpio_mode_pluto_common);
		pins_info = xmm636_init_gpio_mode_pluto_common;
		for (i = 0; i < len; ++i) {
			tegra_gpio_init_configure(pins_info->gpio_nr,
				pins_info->is_input, pins_info->value);
			pins_info++;
		}
	}
#endif
}

int __init pluto_pinmux_init(void)
{
	tegra_pinmux_config_table(pluto_pinmux_set_nontristate,
					ARRAY_SIZE(pluto_pinmux_set_nontristate));

	pluto_gpio_init_configure();

	tegra_pinmux_config_table(pluto_pinmux_common, ARRAY_SIZE(pluto_pinmux_common));
	tegra_drive_pinmux_config_table(pluto_drive_pinmux,
					ARRAY_SIZE(pluto_drive_pinmux));
	tegra_pinmux_config_table(unused_pins_lowpower,
		ARRAY_SIZE(unused_pins_lowpower));
	tegra_pinmux_config_table(manual_config_pinmux,
		ARRAY_SIZE(manual_config_pinmux));
	tegra11x_set_sleep_pinmux(pluto_sleep_pinmux,
		ARRAY_SIZE(pluto_sleep_pinmux));

	if (TEGRA_BB_SPRD == tegra_get_modem_id())
		tegra_pinmux_config_table(sc8800g_pluto_pinmux_set_nontristate,
				ARRAY_SIZE(sc8800g_pluto_pinmux_set_nontristate));
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	else if (TEGRA_BB_IFX == tegra_get_modem_id())
		tegra_pinmux_config_table(xmm636_pluto_pinmux_set_nontristate,
				ARRAY_SIZE(xmm636_pluto_pinmux_set_nontristate));
#endif

	return 0;
}
