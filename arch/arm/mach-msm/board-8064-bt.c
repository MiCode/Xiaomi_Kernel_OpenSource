/* Copyright (c) 2011-2013, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/marimba.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <mach/rpc_pmapp.h>
#include <mach/msm_xo.h>
#include <mach/socinfo.h>

#include "board-8064.h"

#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)

#define BAHAMA_SLAVE_ID_FM_ADDR  0x2A
#define BAHAMA_SLAVE_ID_QMEMBIST_ADDR   0x7B

struct bt_vreg_info {
	const char *name;
	unsigned int pmapp_id;
	unsigned int min_level;
	unsigned int max_level;
	unsigned int is_pin_controlled;
	struct regulator *reg;
};

struct bahama_config_register {
	u8 reg;
	u8 value;
	u8 mask;
};
static struct bt_vreg_info bt_vregs[] = {
	{"bha_vddxo",  2, 1800000, 1800000, 0, NULL},
	{"bha_vddpx", 21, 1800000, 1800000, 0, NULL},
	{"bha_vddpa", 21, 2900000, 3300000, 0, NULL}
};

static struct msm_xo_voter *bt_clock;

static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
	.id = -1,
};

static unsigned bt_config_pcm_on[] = {
	/*PCM_DOUT*/
	GPIO_CFG(43, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*PCM_DIN*/
	GPIO_CFG(44, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*PCM_SYNC*/
	GPIO_CFG(45, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*PCM_CLK*/
	GPIO_CFG(46, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static unsigned bt_config_pcm_off[] = {
	/*PCM_DOUT*/
	GPIO_CFG(43, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*PCM_DIN*/
	GPIO_CFG(44, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*PCM_SYNC*/
	GPIO_CFG(45, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*PCM_CLK*/
	GPIO_CFG(46, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
};

static int config_pcm(int mode)
{
	int pin, rc = 0;

	if (mode == BT_PCM_ON) {
		pr_err("%s mode =BT_PCM_ON", __func__);
		for (pin = 0; pin < ARRAY_SIZE(bt_config_pcm_on);
			pin++) {
				rc = gpio_tlmm_config(bt_config_pcm_on[pin],
					GPIO_CFG_ENABLE);
				if (rc < 0)
					return rc;
		}
	} else if (mode == BT_PCM_OFF) {
		pr_err("%s mode =BT_PCM_OFF", __func__);
		for (pin = 0; pin < ARRAY_SIZE(bt_config_pcm_off);
			pin++) {
				rc = gpio_tlmm_config(bt_config_pcm_off[pin],
					GPIO_CFG_ENABLE);
				if (rc < 0)
					return rc;
		}

	}

	return rc;
}

static int bahama_bt(int on)
{
	int rc = 0;
	int i;

	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};

	struct bahama_variant_register {
		const size_t size;
		const struct bahama_config_register *set;
	};

	const struct bahama_config_register *p;

	int version;

	const struct bahama_config_register v10_bt_on[] = {
		{ 0xE9, 0x00, 0xFF },
		{ 0xF4, 0x80, 0xFF },
		{ 0xE4, 0x00, 0xFF },
		{ 0xE5, 0x00, 0x0F },
#ifdef CONFIG_WLAN
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
		{ 0x01, 0x0C, 0x1F },
		{ 0x01, 0x08, 0x1F },
	};

	const struct bahama_config_register v20_bt_on_fm_off[] = {
		{ 0x11, 0x0C, 0xFF },
		{ 0x13, 0x01, 0xFF },
		{ 0xF4, 0x80, 0xFF },
		{ 0xF0, 0x00, 0xFF },
		{ 0xE9, 0x00, 0xFF },
#ifdef CONFIG_WLAN
		{ 0x81, 0x00, 0x7F },
		{ 0x82, 0x00, 0xFF },
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0x8E, 0x15, 0xFF },
		{ 0x8F, 0x15, 0xFF },
		{ 0x90, 0x15, 0xFF },

		{ 0xE9, 0x21, 0xFF },
	};

	const struct bahama_config_register v20_bt_on_fm_on[] = {
		{ 0x11, 0x0C, 0xFF },
		{ 0x13, 0x01, 0xFF },
		{ 0xF4, 0x86, 0xFF },
		{ 0xF0, 0x06, 0xFF },
		{ 0xE9, 0x00, 0xFF },
#ifdef CONFIG_WLAN
		{ 0x81, 0x00, 0x7F },
		{ 0x82, 0x00, 0xFF },
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
	};

	const struct bahama_config_register v10_bt_off[] = {
		{ 0xE9, 0x00, 0xFF },
	};

	const struct bahama_config_register v20_bt_off_fm_off[] = {
		{ 0xF4, 0x84, 0xFF },
		{ 0xF0, 0x04, 0xFF },
		{ 0xE9, 0x00, 0xFF }
	};

	const struct bahama_config_register v20_bt_off_fm_on[] = {
		{ 0xF4, 0x86, 0xFF },
		{ 0xF0, 0x06, 0xFF },
		{ 0xE9, 0x00, 0xFF }
	};

	const struct bahama_variant_register bt_bahama[2][3] = {
	{
		{ ARRAY_SIZE(v10_bt_off), v10_bt_off },
		{ ARRAY_SIZE(v20_bt_off_fm_off), v20_bt_off_fm_off },
		{ ARRAY_SIZE(v20_bt_off_fm_on), v20_bt_off_fm_on }
	},
	{
		{ ARRAY_SIZE(v10_bt_on), v10_bt_on },
		{ ARRAY_SIZE(v20_bt_on_fm_off), v20_bt_on_fm_off },
		{ ARRAY_SIZE(v20_bt_on_fm_on), v20_bt_on_fm_on }
	}
	};

	u8 offset = 0; /* index into bahama configs */
	on = on ? 1 : 0;
	version = marimba_read_bahama_ver(&config);
	if (version < 0 || version == BAHAMA_VER_UNSUPPORTED) {
		dev_err(&msm_bt_power_device.dev,
			"%s : Bahama version read Error, version = %d\n",
			__func__, version);
		return -EIO;
	}
	if (version == BAHAMA_VER_2_0) {
		if (marimba_get_fm_status(&config))
			offset = 0x01;
	}

	p = bt_bahama[on][version + offset].set;

	dev_dbg(&msm_bt_power_device.dev,
		"%s: found version %d\n", __func__, version);

	for (i = 0; i < bt_bahama[on][version + offset].size; i++) {
		u8 value = (p+i)->value;
		rc = marimba_write_bit_mask(&config,
			(p+i)->reg,
			&value,
			sizeof((p+i)->value),
			(p+i)->mask);
		if (rc < 0) {
			dev_err(&msm_bt_power_device.dev,
				"%s: reg %x write failed: %d\n",
				__func__, (p+i)->reg, rc);
			return rc;
		}
		dev_dbg(&msm_bt_power_device.dev,
			"%s: reg 0x%02x write value 0x%02x mask 0x%02x\n",
				__func__, (p+i)->reg,
				value, (p+i)->mask);
		value = 0;
		if (marimba_read_bit_mask(&config,
				(p+i)->reg, &value,
				sizeof((p+i)->value), (p+i)->mask) < 0)
			dev_err(&msm_bt_power_device.dev,
				"%s marimba_read_bit_mask- error",
				__func__);
		dev_dbg(&msm_bt_power_device.dev,
			"%s: reg 0x%02x read value 0x%02x mask 0x%02x\n",
				__func__, (p+i)->reg,
				value, (p+i)->mask);
	}
	/* Update BT Status */
	if (on)
		marimba_set_bt_status(&config, true);
	else
		marimba_set_bt_status(&config, false);
	return rc;
}

static int bluetooth_switch_regulators(int on)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(bt_vregs); i++) {
		if (IS_ERR_OR_NULL(bt_vregs[i].reg)) {
			bt_vregs[i].reg =
				regulator_get(&msm_bt_power_device.dev,
						bt_vregs[i].name);
			if (IS_ERR(bt_vregs[i].reg)) {
				rc = PTR_ERR(bt_vregs[i].reg);
				dev_err(&msm_bt_power_device.dev,
					"%s: invalid regulator handle for %s: %d\n",
						__func__, bt_vregs[i].name, rc);
				goto reg_disable;
			}
		}
		rc = on ? regulator_set_voltage(bt_vregs[i].reg,
					bt_vregs[i].min_level,
						bt_vregs[i].max_level) : 0;
		if (rc) {
			dev_err(&msm_bt_power_device.dev,
				"%s: could not set voltage for %s: %d\n",
					__func__, bt_vregs[i].name, rc);
			goto reg_disable;
		}

		rc = on ? regulator_enable(bt_vregs[i].reg) : 0;
		if (rc) {
			dev_err(&msm_bt_power_device.dev,
				"%s: could not %sable regulator %s: %d\n",
					__func__, "en", bt_vregs[i].name, rc);
			goto reg_disable;
		}

		rc = on ? 0 : regulator_disable(bt_vregs[i].reg);

		if (rc) {
			dev_err(&msm_bt_power_device.dev,
				"%s: could not %sable regulator %s: %d\n",
					__func__, "dis", bt_vregs[i].name, rc);
		}
	}

	return rc;
reg_disable:
	pr_err("bluetooth_switch_regulators - FAIL!!!!\n");
	if (on) {
		while (i) {
			i--;
			regulator_disable(bt_vregs[i].reg);
			regulator_put(bt_vregs[i].reg);
			bt_vregs[i].reg = NULL;
		}
	}
	return rc;
}

static unsigned int msm_bahama_setup_power(void)
{
	int rc = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(bt_vregs); i++) {
		bt_vregs[i].reg = regulator_get(&msm_bt_power_device.dev,
						bt_vregs[i].name);
		if (IS_ERR(bt_vregs[i].reg)) {
			rc = PTR_ERR(bt_vregs[i].reg);
			pr_err("%s: could not get regulator %s: %d\n",
					__func__, bt_vregs[i].name, rc);
			goto reg_fail;
		}
		rc = regulator_set_voltage(bt_vregs[i].reg,
					bt_vregs[i].min_level,
						bt_vregs[i].max_level);
		if (rc) {
			pr_err("%s: could not set voltage for %s: %d\n",
					__func__, bt_vregs[i].name, rc);
			goto reg_fail;
		}
		rc = regulator_enable(bt_vregs[i].reg);
		if (rc) {
			pr_err("%s: could not enable regulator %s: %d\n",
					__func__, bt_vregs[i].name, rc);
			goto reg_fail;
		}
	}
	return rc;
reg_fail:
	pr_err("msm_bahama_setup_power FAILED !!!\n");

	while (i) {
		i--;
		regulator_disable(bt_vregs[i].reg);
		regulator_put(bt_vregs[i].reg);
		bt_vregs[i].reg = NULL;
	}
	return rc;
}

static unsigned int msm_bahama_shutdown_power(int value)
{
	int rc = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(bt_vregs); i++) {
		rc = regulator_disable(bt_vregs[i].reg);

		if (rc < 0) {
			pr_err("%s: could not disable regulator %s: %d\n",
					__func__, bt_vregs[i].name, rc);
			goto out;
		}

		regulator_put(bt_vregs[i].reg);
		bt_vregs[i].reg = NULL;
	}
out:
	return rc;
}

static unsigned int msm_bahama_core_config(int type)
{
	int rc = 0;

	if (type == BAHAMA_ID) {
		int i;
		struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};
		const struct bahama_config_register v20_init[] = {
			/* reg, value, mask */
			{ 0xF4, 0x84, 0xFF }, /* AREG */
			{ 0xF0, 0x04, 0xFF } /* DREG */
		};
		if (marimba_read_bahama_ver(&config) == BAHAMA_VER_2_0) {
			for (i = 0; i < ARRAY_SIZE(v20_init); i++) {
				u8 value = v20_init[i].value;
				rc = marimba_write_bit_mask(&config,
					v20_init[i].reg,
					&value,
					sizeof(v20_init[i].value),
					v20_init[i].mask);
				if (rc < 0) {
					pr_err("%s: reg %d write failed: %d\n",
						__func__, v20_init[i].reg, rc);
					return rc;
				}
				pr_debug("%s: reg 0x%02x value 0x%02x mask 0x%02x\n",
					__func__, v20_init[i].reg,
					v20_init[i].value, v20_init[i].mask);
			}
		}
	}
	pr_debug("core type: %d\n", type);
	return rc;
}

static int bluetooth_power(int on)
{
	int rc = 0;
	const char *id = "BTPW";
	int cid = 0;
	int bt_state = 0;
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};

	cid = adie_get_detected_connectivity_type();
	if (cid != BAHAMA_ID) {
		pr_err("%s: unexpected adie connectivity type: %d\n",
					__func__, cid);
		return -ENODEV;
	}

	if (on) {
		pr_debug("%s: Powering up the BT module.\n", __func__);
		rc = bluetooth_switch_regulators(on);
		if (rc < 0) {
			pr_err("%s: bluetooth_switch_regulators rc = %d",
					__func__, rc);
			goto exit;
		}
		/* UART GPIO configuration to be done by by UART module*/
		/*Setup BT clocks*/
		pr_debug("%s: Voting for the 19.2MHz clock\n", __func__);
		bt_clock = msm_xo_get(MSM_XO_TCXO_A2, id);
		if (IS_ERR(bt_clock)) {
			rc = PTR_ERR(bt_clock);
			pr_err("%s: failed to get the handle for A2(%d)\n",
					__func__, rc);
			goto fail_power;
		}
		rc = msm_xo_mode_vote(bt_clock, MSM_XO_MODE_ON);
		if (rc < 0) {
			pr_err("%s: Failed to vote for TCXO_A2 ON\n", __func__);
			goto fail_xo_vote;
		}
		msleep(20);

		/*I2C config for Bahama*/
		pr_debug("%s: BT Turn On sequence in-progress.\n", __func__);
		rc = bahama_bt(1);
		if (rc < 0) {
			pr_err("%s: bahama_bt rc = %d", __func__, rc);
			goto fail_xo_vote;
		}
		msleep(20);

		/*setup BT PCM lines*/
		pr_debug("%s: Configuring PCM lines.\n", __func__);
		rc = config_pcm(BT_PCM_ON);
		if (rc < 0) {
			pr_err("%s: config_pcm , rc = %d\n",
				__func__, rc);
			goto fail_i2c;
		}
		pr_debug("%s: BT Turn On complete.\n", __func__);
		/* TO DO - Enable PIN CTRL */
		/*
		rc = msm_xo_mode_vote(bt_clock, MSM_XO_MODE_PIN_CTRL);
		if (rc < 0) {
			pr_err("%s: Failed to vote for TCXO_A2 in PIN_CTRL\n",
				__func__);
			goto fail_xo_vote;
		} */
	} else {
		pr_debug("%s: Turning BT Off.\n", __func__);
		bt_state = marimba_get_bt_status(&config);
		if (!bt_state) {
			pr_err("%s: BT is already turned OFF.\n", __func__);
			return 0;
		}

		rc = config_pcm(BT_PCM_OFF);
		if (rc < 0) {
			pr_err("%s: msm_bahama_setup_pcm_i2s, rc =%d\n",
				__func__, rc);
		}
fail_i2c:
		rc = bahama_bt(0);
		if (rc < 0)
			pr_err("%s: bahama_bt rc = %d", __func__, rc);
fail_xo_vote:
		pr_debug("%s: Voting off the 19.2MHz clk\n", __func__);
		msm_xo_put(bt_clock);
fail_power:
		pr_debug("%s: Switching off voltage regulators.\n", __func__);
		rc = bluetooth_switch_regulators(0);
		if (rc < 0) {
			pr_err("%s: switch_regulators : rc = %d",\
				__func__, rc);
			goto exit;
		}
		pr_debug("%s: BT Power Off complete.\n", __func__);
	}
	return rc;
exit:
	pr_err("%s: failed with rc = %d", __func__, rc);
	return rc;
}

static struct marimba_platform_data marimba_pdata = {
	.slave_id[SLAVE_ID_BAHAMA_FM]	= BAHAMA_SLAVE_ID_FM_ADDR,
	.slave_id[SLAVE_ID_BAHAMA_QMEMBIST] = BAHAMA_SLAVE_ID_QMEMBIST_ADDR,
	.bahama_setup			= msm_bahama_setup_power,
	.bahama_shutdown		= msm_bahama_shutdown_power,
	.bahama_core_config		= msm_bahama_core_config,
	.fm			        = NULL,
};

static struct i2c_board_info bahama_devices[] = {
{
	I2C_BOARD_INFO("marimba", 0xc),
	.platform_data = &marimba_pdata,
},
};

void __init apq8064_bt_power_init(void)
{
	int rc = 0;
	struct device *dev;

	rc = i2c_register_board_info(APQ_8064_GSBI5_QUP_I2C_BUS_ID,
				bahama_devices,
				ARRAY_SIZE(bahama_devices));
	if (rc < 0) {
		pr_err("%s: I2C Register failed\n", __func__);
		return;
	}
	rc = platform_device_register(&msm_bt_power_device);
	if (rc < 0) {
		pr_err("%s: device register failed\n", __func__);
		platform_device_unregister(&msm_bt_power_device);
		return;
	}

	dev = &msm_bt_power_device.dev;
	dev->platform_data = &bluetooth_power;

	return;
}
#endif
