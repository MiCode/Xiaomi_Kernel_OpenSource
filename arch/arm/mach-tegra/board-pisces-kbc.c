/*
 * arch/arm/mach-tegra/board-pisces-kbc.c
 * Keys configuration for Nvidia tegra3 pluto platform.
 *
 * Copyright (C) 2012 NVIDIA, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/kbc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/platform_data/fsa8108.h>
#include <linux/i2c.h>

#include "board.h"
#include "board-pisces.h"
#include "devices.h"

#define PLUTO_ROW_COUNT	1
#define PLUTO_COL_COUNT	3

static const u32 kbd_keymap[] = {
	KEY(0, 0, KEY_POWER),
	KEY(0, 1, KEY_VOLUMEUP),
	KEY(0, 2, KEY_VOLUMEDOWN),
};

static const struct matrix_keymap_data keymap_data = {
	.keymap		= kbd_keymap,
	.keymap_size	= ARRAY_SIZE(kbd_keymap),
};

static struct tegra_kbc_wake_key pluto_wake_cfg[] = {
	[0] = {
		.row = 0,
		.col = 0,
	},
	[1] = {
		.row = 0,
		.col = 1,
	},
	[2] = {
		.row = 0,
		.col = 2,
	},
};

static struct tegra_kbc_platform_data pluto_kbc_platform_data = {
	.debounce_cnt = 20 * 32,	/* 20 ms debaunce time */
	.repeat_cnt = 1,
	.scan_count = 5,
	.wakeup = true,
	.keymap_data = &keymap_data,
	.wake_cnt = ARRAY_SIZE(pluto_wake_cfg),
	.wake_cfg = &pluto_wake_cfg[0],
	.wakeup_key = KEY_POWER,
#ifdef CONFIG_ANDROID
	.disable_ev_rep = true,
#endif
};

int __init pluto_kbc_init(void)
{
	struct tegra_kbc_platform_data *data = &pluto_kbc_platform_data;
	int i;

	tegra_kbc_device.dev.platform_data = &pluto_kbc_platform_data;
	pr_info("Registering tegra-kbc\n");

	BUG_ON((KBC_MAX_ROW + KBC_MAX_COL) > KBC_MAX_GPIO);
	for (i = 0; i < PLUTO_ROW_COUNT; i++) {
		data->pin_cfg[i].num = i;
		data->pin_cfg[i].type = PIN_CFG_ROW;
	}
	for (i = 0; i < PLUTO_COL_COUNT; i++) {
		data->pin_cfg[i + KBC_PIN_GPIO_11].num = i;
		data->pin_cfg[i + KBC_PIN_GPIO_11].type = PIN_CFG_COL;
	}

	platform_device_register(&tegra_kbc_device);
	pr_info("Registering successful tegra-kbc\n");

	return 0;
}

/*-------------------------------------------------------------------*/
/*add i2c device for jack device */
#define JACK_IRQ_GPIO		TEGRA_GPIO_PR7
#define JACK_RST_GPIO		TEGRA_GPIO_PJ3

#ifdef CONFIG_INPUT_JACK_FSA8108
static struct reg_default fsa8108_reg_map_default[] = {
	{.reg = INTERRUPT_MASK_1_REG,	.def = 0x00,}, /*0x04:Interrupt 1 Mask*/
	{.reg = INTERRUPT_MASK_2_REG,	.def = 0x00,}, /*0x05:Interrupt 2 Mask*/
	{.reg = GLOBAL_MULTIPLIER_REG,	.def = 0x04,}, /*0x06:Global Multiplier */
	{.reg = J_DET_TIMING_REG,	.def = 0x82,}, /*0x07:J_DET Timing */
	{.reg = KEY_PRESS_TIMING_REG,	.def = 0x08,}, /*0x08:Key Press Timing */
	{.reg = MP3_MODE_TIMING_REG,	.def = 0x28,}, /*0x09:MP3 Mode Timing */
	{.reg = DETECTION_TIMING_REG,	.def = 0x55,}, /*0x0a:Detection Timing */
	{.reg = DEBOUNCE_TIMIG_REG,	.def = 0x98,}, /*0x0b:Debounce Timing */
	{.reg = CONTROL_REG,	.def = DEFAULT_VALUE_CONTROL_REG,}, /*0x0c:Control */
	{.reg = DET_THRESHOLDS_1_REG,	.def = 0xFA,}, /*0x0d:Detection Thresholds 1*/
	{.reg = DET_THRESHOLDS_2_REG,	.def = 0x79,}, /*0x0e:Detection Thresholds 2*/
	{.reg = RESET_CONTROL_REG,	.def = 0x00,}, /*0x0f:Reset Control */
};

/*
  * intmask :
  * 1<<3(click press),1<<4(double press),1<<5(long press) belong to a physical key,middle key
  * 1<<8(click press),1<<9(long press),1<<10(long release) belong to a physical key, up key
  * 1<<11(click press),1<<12(long press),1<<13(long release) belong to a physical key, down key
  *
  *value :if type is EV_SW, true plugin, false pullout.
  *           if type is EV_KEY,false is release,true is press or click
  *
  *Note That: you can change the key code if you like.
  */
static struct fsa8108_intmask_event fsa8108_int_mask_event[] = {
	{.intmask = FSA8108_3POLE_INSERTED_MASK, .code = SW_HEADPHONE_INSERT,
			.type = EV_SW, .value = true,},
	{.intmask = FSA8108_4POLE_INSERTED_MASK, .code = SW_HEADPHONE_INSERT,
			.type = EV_SW, .value = true,},
	{.intmask = FSA8108_4POLE_INSERTED_MASK, .code = SW_MICROPHONE_INSERT,
			.type = EV_SW, .value = true,},
	{.intmask = FSA8108_DISCONNECT_MASK, .code = SW_HEADPHONE_INSERT,
			.type = EV_SW, .value = false,},
	{.intmask = FSA8108_DISCONNECT_MASK, .code = SW_MICROPHONE_INSERT,
			.type = EV_SW, .value = false,},
	{.intmask = FSA8108_SW_MASK, .code = BTN_0, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_DSW_MASK, .code = BTN_3, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_LSW_MASK, .code = BTN_0, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_VOLUP_MASK, .code = BTN_1, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_LVOLUP_PRESS_MASK, .code = BTN_1, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_LVOLUP_RELEASE_MASK, .code = BTN_1, .type = EV_KEY, .value = false,},
	{.intmask = FSA8108_VOLDOWN_MASK, .code = BTN_2, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_LVOLDOWN_PRESS_MASK, .code = BTN_2, .type = EV_KEY, .value = true,},
	{.intmask = FSA8108_LVOLDOWN_RELEASE_MASK, .code = BTN_2, .type = EV_KEY, .value = false,},
};

static struct fsa8108_platform_data fsa8108_data = {
	.reg_map                    = fsa8108_reg_map_default,
	.reg_map_size            = ARRAY_SIZE(fsa8108_reg_map_default),
	.fsa8108_event           = fsa8108_int_mask_event,
	.fsa8108_event_size   = ARRAY_SIZE(fsa8108_int_mask_event),
	.irq_gpio                     = JACK_IRQ_GPIO,
	.reset_gpio                  = JACK_RST_GPIO,
	.supply                      = "MIC2 Bias",
};
#endif

static struct i2c_board_info pisces_i2c1_jack_board_info[] = {
#ifdef CONFIG_INPUT_JACK_FSA8108
	{
		I2C_BOARD_INFO("fsa8108", 0x23),
		.platform_data = &fsa8108_data,
	},
#endif
};

int __init pluto_jack_init(void)
{
	int err = 0;

	err = i2c_register_board_info(0, pisces_i2c1_jack_board_info,
				ARRAY_SIZE(pisces_i2c1_jack_board_info));
	if (err)
		pr_err("%s: jack devices register failed.\n", __func__);

	return 0;
}
