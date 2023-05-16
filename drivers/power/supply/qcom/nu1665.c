/* Copyright (c) 2022 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* this driver is compatible for nuvolta wireless charge ic */

#include <linux/i2c.h>
#include <linux/alarmtimer.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/unaligned.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <asm/uaccess.h>
#include <linux/pmic-voter.h>
#include <linux/irq.h>

#include "nu1665.h"

static int log_level = 1;
static struct nuvolta_1665_chg *g_chip;
static struct wls_fw_parameters g_wls_fw_data = {0};
static int last_valid_pen_soc = -1;
static int pen_soc_count = 0;
static u8 sram_buffer[256];
static int curr_count = 0;

static int nuvolta_1665_set_enable_mode(struct nuvolta_1665_chg *chip, bool enable);
static int nuvolta_1665_set_reverse_chg_mode(struct nuvolta_1665_chg *chip, int enable);
static int tx_info_update(struct nuvolta_1665_chg * chip, u8* buff);
static int fw_crc_chk(struct nuvolta_1665_chg *chip);
static int read_fw_version(struct nuvolta_1665_chg *chip, u8 *version);
static int nuvolta_1665_download_fw(struct nuvolta_1665_chg *chip, bool power_on, bool force);
static int nuvolta_1665_get_reverse_soc(struct nuvolta_1665_chg * chip);

static struct regmap_config nuvolta_1665_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static struct params_t fod_params_l2_50W[] = {
	{.gain = 0,    .offset = 13},
	{.gain = 0,    .offset = 9},
	{.gain = 0,    .offset = 9},
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 16},
	{.gain = 3,    .offset = 7},
	{.gain = 3,    .offset = 5},
	{.gain = 1,    .offset = 5}
};

static struct params_t fod_params_k1_80W_27V[] = {
	{.gain = 0,   .offset = 30},
	{.gain = 0, .offset = 24},
	{.gain = 0,    .offset = 23},
	{.gain = 0,    .offset = 38},
	{.gain = 0,    .offset = 23},
	{.gain = 0,    .offset = 20},
	{.gain = 0,    .offset = 14},
	{.gain = 0,    .offset = 14},
	{.gain = 0,    .offset = 17},
	{.gain = 0,    .offset = 13},
	{.gain = 0,    .offset = 13}
};

static struct params_t fod_params_k1_80W[] = {
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 6},
	{.gain = 0,    .offset = 7},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 20},
	{.gain = 0,    .offset = 29},
	{.gain = 0,    .offset = 32},
	{.gain = 3,    .offset = 19},
	{.gain = 5,    .offset = 17},
	{.gain = 7,    .offset = 5},
	{.gain = 7,    .offset = 5}
};

static struct params_t fod_params_k8_100W[] = {
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 6},
	{.gain = 0,    .offset = 7},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 17},
	{.gain = 0,    .offset = 23},
	{.gain = 0,    .offset = 30},
	{.gain = 4,    .offset = 18},
	{.gain = 5,    .offset = 17},
	{.gain = 7,    .offset = 6},
	{.gain = 7,    .offset = 1}
};

static struct params_t fod_params_moving_20W[] = {
	{.gain = 0,    .offset = 9},
	{.gain = 0,    .offset = 1},
	{.gain = 0,    .offset = 2},
	{.gain = 0,    .offset = 1},
	{.gain = 0,    .offset = 5},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0}
};

static struct params_t fod_params_j1s_55W[] = {
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 12}
};

static struct params_t fod_params_white_stand_30W[] = {
	{.gain = 0,    .offset = 13},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 9},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 17},
	{.gain = 0,    .offset = 19},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 10}
};

static struct params_t fod_params_bluetooth_30W[] = {
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 14},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 27},
	{.gain = 0,    .offset = 28},
	{.gain = 0,    .offset = 28},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 10}
};

static struct params_t fod_params_zimi_20W[] = {
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 12},
	{.gain = 0,    .offset = 14},
	{.gain = 0,    .offset = 16},
	{.gain = 0,    .offset = 18},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0}
};

static struct params_t fod_params_white_20W[] = {
	{.gain = 0,    .offset = 14},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 13},
	{.gain = 0,    .offset = 18},
	{.gain = 0,    .offset = 20},
	{.gain = 0,    .offset = 21},
	{.gain = 0,    .offset = 21},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0}
};

static struct params_t fod_params_power_bank_30W[] = {
	{.gain = 0,    .offset = 2},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 3},
	{.gain = 0,    .offset = 13},
	{.gain = 0,    .offset = 23},
	{.gain = 0,    .offset = 30},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0}
};

static struct params_t fod_params_zimi_car_20W[] = {
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 16},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0}
};

static struct params_t fod_params_multcoil_20W[] = {
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 6},
	{.gain = 0,    .offset = 8},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 12},
	{.gain = 0,    .offset = 14},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0},
	{.gain = 0,    .offset = 0}
};

static struct params_t fod_params_bpp_plus[] = {
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 10},
	{.gain = 0,    .offset = 11},
	{.gain = 0,    .offset = 12},
	{.gain = 0,    .offset = 12},
};

static struct params_t fod_params_default[] = {
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 15},
	{.gain = 0,    .offset = 16},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 22},
	{.gain = 0,    .offset = 29},
	{.gain = 0,    .offset = 32},
	{.gain = 4,    .offset = 18},
	{.gain = 5,    .offset = 17},
	{.gain = 7,    .offset = 6},
	{.gain = 7,    .offset = 5}
};

//bpp plus, no care UUID
static struct fod_params_t fuda1651_bpp_plus_fod_param = {
	.type = FOD_PARAM_BPP_PLUS,
	.length = sizeof(fod_params_bpp_plus)/sizeof(fod_params_bpp_plus[0]),
	.uuid = {0x00, 0x00, 0x00, 0x00},
	.params = fod_params_bpp_plus
};

//default fod, no care UUID
static struct fod_params_t fuda1651_fod_param_default = {
	.type = FOD_PARAM_20V,
	.length = sizeof(fod_params_default)/sizeof(fod_params_default[0]),
	.uuid = {0x00, 0x00, 0x00, 0x00},
	.params = fod_params_default
};

static struct fod_params_t fuda1651_fod_params[] = {
	{
		//l2_50w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_l2_50W)/sizeof(fod_params_l2_50W[0]),
		.uuid = {0x09, 0x01, 0x09, 0x01},
		.params = fod_params_l2_50W
	},
	{
		//k1_80w 20V
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_k1_80W)/sizeof(fod_params_k1_80W[0]),
		.uuid = {0x09, 0x01, 0x09, 0x04},
		.params = fod_params_k1_80W
	},
	{
		//k1_80w 27V
		.type = FOD_PARAM_27V,
		.length = sizeof(fod_params_k1_80W_27V)/sizeof(fod_params_k1_80W_27V[0]),
		.uuid = {0x09, 0x01, 0x09, 0x04},
		.params = fod_params_k1_80W_27V
	},
	{
		//k8_100w 20V
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_k8_100W)/sizeof(fod_params_k8_100W[0]),
		.uuid = {0x09, 0x01, 0x09, 0x0C},
		.params = fod_params_k8_100W
	},
	{
		//j1s_55w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_j1s_55W)/sizeof(fod_params_j1s_55W[0]),
		.uuid = {0x09, 0x01, 0x01, 0x0b},
		.params = fod_params_j1s_55W
	},
	{
		//white_stand_30w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_white_stand_30W)/sizeof(fod_params_white_stand_30W[0]),
		.uuid = {0x09, 0x01, 0x04, 0x07},
		.params = fod_params_white_stand_30W
	},
	{
		//bluetooth_30w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_bluetooth_30W)/sizeof(fod_params_bluetooth_30W[0]),
		.uuid = {0x09, 0x08, 0x06, 0x07},
		.params = fod_params_bluetooth_30W
	},
	{
		//moving_20w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_moving_20W)/sizeof(fod_params_moving_20W[0]),
		.uuid = {0x09, 0x01, 0x05, 0x06},
		.params = fod_params_moving_20W
	},
	{
		//zimi_black_20w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_zimi_20W)/sizeof(fod_params_zimi_20W[0]),
		.uuid = {0x09, 0x08, 0x01, 0x08},
		.params = fod_params_zimi_20W
	},
	{
		//white_20W
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_white_20W)/sizeof(fod_params_white_20W[0]),
		.uuid = {0x06, 0x01, 0x01, 0x01},
		.params = fod_params_white_20W
	},
	{
		//power_bank_20w_30W
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_power_bank_30W)/sizeof(fod_params_power_bank_30W[0]),
		.uuid = {0x07, 0x03, 0x08, 0x01},
		.params = fod_params_power_bank_30W
	},
	{
		//zimi_car_20w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_zimi_car_20W)/sizeof(fod_params_zimi_car_20W[0]),
		.uuid = {0x06, 0x02, 0x08, 0x01},
		.params = fod_params_zimi_car_20W
	},
	{
		//multcoil tx 20w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_multcoil_20W)/sizeof(fod_params_multcoil_20W[0]),
		.uuid = {0x0c, 0x09, 0x09, 0x06},
		.params = fod_params_multcoil_20W
	},
	{
		//multcoil tx 20w
		.type = FOD_PARAM_20V,
		.length = sizeof(fod_params_multcoil_20W)/sizeof(fod_params_multcoil_20W[0]),
		.uuid = {0x0c, 0x09, 0x09, 0x08},
		.params = fod_params_multcoil_20W
	}
};

struct delayed_work *pen_notifier_work;

static BLOCKING_NOTIFIER_HEAD(pen_charge_state_notifier_list);

static void pen_charge_notifier_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip = container_of(work,
							struct nuvolta_1665_chg,
							pen_notifier_work.work);
	blocking_notifier_call_chain(&pen_charge_state_notifier_list,
								chip->pen_val,
								chip->pen_v);
	return;
}

int pen_charge_state_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pen_charge_state_notifier_list, nb);
}
EXPORT_SYMBOL(pen_charge_state_notifier_register_client);

int pen_charge_state_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pen_charge_state_notifier_list, nb);
}
EXPORT_SYMBOL(pen_charge_state_notifier_unregister_client);

void pen_charge_state_notifier_call_chain(unsigned long val, void *v)
{
	struct nuvolta_1665_chg *chip = container_of(pen_notifier_work,
							struct nuvolta_1665_chg,
							pen_notifier_work);
	chip->pen_val = val;
	chip->pen_v = v;
	schedule_delayed_work(&chip->pen_notifier_work, msecs_to_jiffies(0));
}
EXPORT_SYMBOL(pen_charge_state_notifier_call_chain);

static void pen_charge_state_notifier_call_chain_booting(unsigned long val, void *v)
{
	struct nuvolta_1665_chg *chip = container_of(pen_notifier_work,
							struct nuvolta_1665_chg,
							pen_notifier_work);
	chip->pen_val = val;
	chip->pen_v = v;
	schedule_delayed_work(&chip->pen_notifier_work, msecs_to_jiffies(2000));
}

static int rx1665_read(struct nuvolta_1665_chg *chip, u8 *val, u16 addr)
{
	unsigned int temp;
	int rc = 0;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc < 0)
		nuvolta_err("i2c read error: %d, address:%x\n", rc, addr);
	else
		*val = (u8)temp;

	return rc;
}

static int rx1665_read_buffer(struct nuvolta_1665_chg *chip, u8 *buf, u16 addr, int size)
{
	int rc = 0;

	while (size--) {
		rc = rx1665_read(chip, buf++, addr++);
		if (rc < 0) {
			nuvolta_err("[%s]i2c read error: %d\n", __func__, rc);
			return rc;
		}
	}

	return rc;
}

static int rx1665_write(struct nuvolta_1665_chg *chip, u8 val, u16 addr)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, addr, val);
	if (rc < 0)
		nuvolta_err("i2c write error: %d, address:%x\n", rc, addr);

	return rc;
}

static int rx1665_write_buffer(struct nuvolta_1665_chg *chip, u8 *buf, u16 addr, int size)
{
	int rc = 0;

	while (size--) {
		rc = rx1665_write(chip, *buf++, addr++);
		if (rc < 0) {
			nuvolta_err("[%s]i2c write error: %d\n", __func__, rc);
			return rc;
		}
	}

	return rc;
}

void nu1665_sent_pen_mac(struct nuvolta_1665_chg *chip)
{
	int64_t ble_mac = 0;
	union power_supply_propval val = { 0, };

	memcpy(&ble_mac, chip->pen_mac_data, MAC_LEN);
	val.int64val = ble_mac;
	if (chip->wireless_psy)
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_PEN_MAC, &val);
	else
		nuvolta_err("pen ble mac not find wireless psy\n");
}

static int fuda_trx_get_ble_mac(struct nuvolta_1665_chg *chip)
{
	int rc = 0, i = 0;
	//u8 mac_buf[MAC_LEN] = { 0 };

	rc = tx_info_update(chip, sram_buffer);
	if (rc < 0) {
		nuvolta_err("pen ble mac addr get error:\n");
		return rc;
	}
	for (i = 0; i < MAC_LEN; i++)
		chip->pen_mac_data[i] = sram_buffer[i+36];
	//kmem_copy(mac_buf, &sram_buffer[36], 6);

	nuvolta_info("mac addr of pen: 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
		sram_buffer[36], sram_buffer[37], sram_buffer[38], sram_buffer[39], 
		sram_buffer[40], sram_buffer[41]);

	rc = nuvolta_1665_get_reverse_soc(chip);
	nu1665_sent_pen_mac(chip);

	return rc;

}

static bool nuvolta_1665_check_cmd_free(struct nuvolta_1665_chg *chip, u16 reg)
{
	u8 rx_cmd_busy = 0, retry = 0;

	while(retry++ < 100) {
		rx1665_read(chip, &rx_cmd_busy, reg);
		if (rx_cmd_busy != 0x55)
			return true;
	}

	nuvolta_info("%s reg: %x always busy\n", __func__, reg);
	return false;
}

static bool nuvolta_1665_check_buffer_ready(struct nuvolta_1665_chg *chip)
{
	return nuvolta_1665_check_cmd_free(chip, 0x0024);
}

static bool nuvolta_1665_check_rx_ready(struct nuvolta_1665_chg *chip)
{
	return nuvolta_1665_check_cmd_free(chip, 0x0025);
}

static int nuvolta_1665_start_tx_function(struct nuvolta_1665_chg *chip, bool en)
{
	int ret = 0;
	//u8 mode = RX_MODE;

	nuvolta_err("%s enable:%d\n", __func__, en);
	if (!chip->fw_update) {
		if (en) {
			ret = rx1665_write(chip, 0x01, TRX_MODE_EN);
			if (ret >= 0) {
				nuvolta_info("ic work on rtx mode,start reverse charging,ret:%d\n", ret);
				chip->is_reverse_mode = 1;
				return 1;
			}
		}
		else
			ret = rx1665_write(chip, 0x00, TRX_MODE_EN);

		nuvolta_info("[%s] ret = %d\n", __func__, ret);
	}

	nuvolta_info("Not open reverse charging start:%d\n", en);
	ret = nuvolta_1665_set_reverse_chg_mode(chip, false);
	return 0;
}

#define REVERSE_GPIO_STATE_UNSET 0
#define REVERSE_GPIO_STATE_START 1
#define REVERSE_GPIO_STATE_END     2
static int nu1665_set_reverse_gpio_state(struct nuvolta_1665_chg *chip,
					   int enable)
{
	union power_supply_propval reverse_val = { 0, };

	if (!chip->wireless_psy)
		chip->wireless_psy = power_supply_get_by_name("wireless");

	if (chip->wireless_psy) {
		nuvolta_dbg("set_reverse_gpio_state\n",
			reverse_val.intval);
		if (enable) {
			reverse_val.intval = REVERSE_GPIO_STATE_START;
		} else {
			reverse_val.intval = REVERSE_GPIO_STATE_END;
		}

		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_REVERSE_GPIO_STATE,
					  &reverse_val);

	} else {
		nuvolta_err("no wireless_psy,return\n");
		return -EINVAL;
	}
	return 0;

}

static int rx_set_reverse_boost_enable_gpio(struct nuvolta_1665_chg *chip, int enable)
{
   int ret = 0;
   if (gpio_is_valid(chip->reverse_boost_gpio)) {
	   ret = gpio_request(chip->reverse_boost_gpio,
				  "reverse-boost-enable-gpio");
	   if (ret) {
		   nuvolta_err( "%s: unable to reverse_boost_enable_gpio [%d]\n",
			   __func__, chip->reverse_boost_gpio);
	   }

	   ret = gpio_direction_output(chip->reverse_boost_gpio, !!enable);
	   if (ret) {
		   nuvolta_err("%s: cannot set direction for reverse_boost_enable_gpio  gpio [%d]\n",
			   __func__, chip->reverse_boost_gpio);
	   }
	   gpio_free(chip->reverse_boost_gpio);
   } else
	   nuvolta_err("%s: unable to set reverse_boost_enable_gpio\n", __func__);

	return ret;
}

static int nuvolta_1665_set_reverse_gpio(struct nuvolta_1665_chg *chip, int enable)
{
	int ret = 0;
	union power_supply_propval val = { 0, };
	if (!chip->wireless_psy)
		chip->wireless_psy = power_supply_get_by_name("wireless");

	if (!chip->wireless_psy) {
		nuvolta_err("no wireless_psy,return\n");
		return -EINVAL;
	}

	val.intval = !!enable;

	if (gpio_is_valid(chip->tx_on_gpio)) {
		if (!enable) {
			nu1665_set_reverse_gpio_state(chip, enable);
			power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_SW_DISABLE_DC_EN,
						&val);
			rx_set_reverse_boost_enable_gpio(chip, enable);
			msleep(100);
		}

		ret = gpio_request(chip->tx_on_gpio,
				"tx-on-gpio");
		if (ret) {
			nuvolta_err("%s: unable to request tx_on gpio\n", __func__);
		}
		ret = gpio_direction_output(chip->tx_on_gpio, enable);
		if (ret) {
			nuvolta_err("%s: cannot set direction for tx_on gpio\n", __func__);
		}

		ret = gpio_get_value(chip->tx_on_gpio);
		nuvolta_info("txon gpio: %d\n", ret);
		nuvolta_err("%s-2 chip->tx_on_gpio:%d, gpio is valid:%d,\n", 
		__func__, chip->tx_on_gpio, gpio_is_valid(chip->tx_on_gpio));
		gpio_free(chip->tx_on_gpio);
		if (enable) {
			msleep(100);
			rx_set_reverse_boost_enable_gpio(chip, enable);
			nu1665_set_reverse_gpio_state(chip, enable);
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_SW_DISABLE_DC_EN,
						  &val);
		}
	} else
		nuvolta_err("%s: unable to set tx_on gpio\n", __func__);

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy)
		nuvolta_err("no wireless_psy,return\n");
	else {
		power_supply_changed(chip->wireless_psy);
	}

	return ret;
}

/*
static int nuvolta_1665_set_reverse_pmic_boost(struct nuvolta_1665_chg *chip, int enable)
{
	struct regulator *vbus = chip->pmic_boost;
	int ret = 0, otg_enable = 0;

	ret = usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);
	if (ret < 0)
		nuvolta_err("get otg enable status failed\n");

	nuvolta_err("set_reverse_pmic_boost enable = %d otg enable = %d\n", enable, otg_enable);
	if (enable) {
		charger_dev_enable_cp_wpc_gate(chip->master_cp_dev, true);
		msleep(200);
		ret = regulator_set_voltage(vbus, 5500000, INT_MAX);
		if (ret) {
			nuvolta_err("vbus regulator set voltage failed\n");
			charger_dev_enable_cp_wpc_gate(chip->master_cp_dev, false);
			return ret;
		}

		ret = regulator_set_current_limit(vbus, 1800000, 1800000);
		if (ret) {
			nuvolta_err("vbus regulator set current failed\n");
			charger_dev_enable_cp_wpc_gate(chip->master_cp_dev, false);
			return ret;
		}

		ret = regulator_enable(vbus);
		if (ret) {
			nuvolta_err("vbus regulator enable failed\n");
			charger_dev_enable_cp_wpc_gate(chip->master_cp_dev, false);
			return ret;
		}
		nuvolta_err("vbus regulator enable\n");
	} else {
		if (otg_enable) {
			ret = regulator_set_voltage(vbus, 5000000, INT_MAX);
			regulator_disable(vbus);
			charger_dev_enable_cp_usb_gate(chip->master_cp_dev, true);
			nuvolta_err("otg mode update firmware end change usb boost mode\n");
		} else {
			regulator_disable(vbus);
			charger_dev_enable_cp_wpc_gate(chip->master_cp_dev, false);
			nuvolta_err("vbus regulator disable\n");
		}
	}

	return ret;
}
*/

static int nuvolta_1665_set_reverse_chg_mode(struct nuvolta_1665_chg *chip, int enable)
{
	int rc = 0;
	union power_supply_propval wk_val = { 0, };

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		nuvolta_err("[idt] no wireless_psy,return\n");
		if (!enable) {
			chip->is_reverse_mode = 0;
			cancel_delayed_work(&chip->reverse_chg_state_work);
			cancel_delayed_work(&chip->reverse_dping_state_work);
			//cancel_delayed_work(&chip->reverse_ept_type_work);
			nuvolta_1665_set_reverse_gpio(chip, false);
		}
		goto out;
	}

	if (chip->fw_update) {
		goto out;
	}

	nuvolta_1665_set_reverse_gpio(chip, enable);

	if (enable) {
		chip->is_boost_mode = 1;
		nuvolta_info("enable reverse charging\n");
		chip->reverse_chg_en = true;
		
		msleep(100);
		rc = nuvolta_1665_start_tx_function(chip, true);
		if (rc) {
			if (chip->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(chip->wireless_psy,
							POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							&wk_val);
			}
			alarm_start_relative(&chip->reverse_dping_alarm,
				ms_to_ktime(REVERSE_DPING_CHECK_DELAY_MS));
		}
	} else {
		chip->is_boost_mode = 0;
		nuvolta_info("disable reverse charging\n");
		if (chip->wireless_psy) {
			wk_val.intval = 0;
			power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
						&wk_val);
		}
		chip->reverse_chg_en = false;
		chip->alarm_flag = false;
		chip->is_reverse_mode = 0;
		chip->reverse_pen_soc = 255;
		pen_soc_count = 0;
		curr_count = 0;

		msleep(100);
		cancel_delayed_work(&chip->reverse_chg_state_work);
		cancel_delayed_work(&chip->reverse_dping_state_work);
		cancel_delayed_work(&chip->reverse_chg_work);
				/* reset pen mac addr */
		memset(chip->pen_mac_data, 0x0, sizeof(chip->pen_mac_data));
		nu1665_sent_pen_mac(chip);

		rc = alarm_cancel(&chip->reverse_dping_alarm);
		if (rc < 0)
			nuvolta_err("Couldn't cancel reverse_dping_alarm\n");

		rc = alarm_cancel(&chip->reverse_chg_alarm);
		if (rc < 0)
			nuvolta_err("Couldn't cancel reverse_chg_alarm\n");
		pm_relax(chip->dev);
	}

out:
	return 0;
}

static void nuvolta_1665_set_fod(struct nuvolta_1665_chg *chip, struct fod_params_t *params_base)
{
	u8 params_offset;
	u32 buffer_length;
	struct params_t params_buffer[5];
	int i = 0;
	bool status = true;
	int ret = 0;

	nuvolta_info("%s type: %d\n", __func__, params_base->type);
	for(i = 0; i < 11; ++i) {
		nuvolta_info("nuvolta_1665_set_fod params[%d]: %d, %d",
			i, params_base->params[i].gain, params_base->params[i].offset);
	}

	params_offset = 0;
	buffer_length = sizeof(struct params_t) * 5;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return;

	ret = rx1665_write(chip, 0x98, 0x0000); //cmd 0x98
	if (ret < 0)
		return;

	ret = rx1665_write(chip, 0x0D, 0x0001); //cmd length:13
	if (ret < 0)
		return;

	ret = rx1665_write(chip, params_base->type, 0x0002); //params type
	if (ret < 0)
		return;

	ret = rx1665_write(chip, params_offset, 0x0003); //params offset:0
	if (ret < 0)
		return;

	ret = rx1665_write(chip, buffer_length, 0x0004); //params length:max 10 one time
	if (ret < 0)
		return;

	memcpy((void *)params_buffer, (const void *)&params_base->params[params_offset], buffer_length);

	ret = rx1665_write_buffer(chip, (u8 *)params_buffer, 0x0005, buffer_length); //params length:max 10 one time
	if (ret < 0)
		return;	

	ret = rx1665_write(chip, 15, 0x0060); //triger int to rx
	if (ret < 0)
		return;

	msleep(20); //wait rx save for params

	params_offset += 5;
	buffer_length = sizeof(struct params_t) * 5;

	ret = rx1665_write(chip, params_offset, 0x0003); //params offset:5
	if (ret < 0)
		return;

	ret = rx1665_write(chip, buffer_length, 0x0004); //params length:max 10 one time
	if (ret < 0)
		return;

	memcpy((void *)params_buffer, (const void *)&params_base->params[params_offset], buffer_length);

	ret = rx1665_write_buffer(chip, (u8 *)params_buffer, 0x0005, buffer_length); //params length:max 10 one time
	if (ret < 0)
		return;	

	ret = rx1665_write(chip, 15, 0x0060); //triger int to rx
	if (ret < 0)
		return;

	msleep(20); //wait rx save for params

	params_offset += 5;
	buffer_length = sizeof(struct params_t) * 1;

	ret = rx1665_write(chip, 0x05, 0x0001); //cmd length:13
	if (ret < 0)
		return;

	ret = rx1665_write(chip, params_offset, 0x0003); //params offset:10
	if (ret < 0)
		return;

	ret = rx1665_write(chip, buffer_length, 0x0004); //params length:max 10 one time
	if (ret < 0)
		return;

	memcpy((void *)params_buffer, (const void *)&params_base->params[params_offset], buffer_length);

	ret = rx1665_write_buffer(chip, (u8 *)params_buffer, 0x0005, buffer_length); //params length:max 10 one time
	if (ret < 0)
		return; 

	ret = rx1665_write(chip, 7, 0x0060); //triger int to rx
	if (ret < 0)
		return;

	msleep(20); //wait rx save for params
	return;
}

static void nuvolta_1665_set_bpp_plus_fod(struct nuvolta_1665_chg *chip, struct fod_params_t *params_base)
{
	u8 params_offset;
	u32 buffer_length;
	struct params_t params_buffer[5];
	bool status = true;
	int ret = 0;

	nuvolta_info("%s\n", __func__);

	params_offset = 0;
	buffer_length = sizeof(struct params_t) * 5;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return;

	ret = rx1665_write(chip, 0x98, 0x0000); //cmd 0x98
	if (ret < 0)
		return;

	ret = rx1665_write(chip, 0x0D, 0x0001); //cmd length:13
	if (ret < 0)
		return;

	ret = rx1665_write(chip, params_base->type, 0x0002); //params type
	if (ret < 0)
		return;

	ret = rx1665_write(chip, params_offset, 0x0003); //params offset:0
	if (ret < 0)
		return;

	ret = rx1665_write(chip, buffer_length, 0x0004); //params length:max 10 one time
	if (ret < 0)
		return;

	memcpy((void *)params_buffer, (const void *)&params_base->params[params_offset], buffer_length);

	ret = rx1665_write_buffer(chip, (u8 *)params_buffer, 0x0005, buffer_length); //params length:max 10 one time
	if (ret < 0)
		return;	

	ret = rx1665_write(chip, 15, 0x0060); //triger int to rx
	if (ret < 0)
		return;

	msleep(20); //wait rx save for params

	return;
}

static void nuvolta_1665_set_fod_params(struct nuvolta_1665_chg *chip)
{
	int i = 0, j = 0;
	bool found = true;

	for (i = 0; i < sizeof(fuda1651_fod_params)/sizeof(fuda1651_fod_params[0]); i++) {
		found = true;
		for (j = 0; j < 4; j++) {
			if (chip->uuid[j] != fuda1651_fod_params[i].uuid[j]) {
				found = false;
				break;
			}
		}
		/* found fod by uuid */
		if (found) {
			nuvolta_info("%s uuid: 0x%x,0x%x,0x%x,0x%x\n", __func__, chip->uuid[0],
				chip->uuid[1], chip->uuid[2], chip->uuid[3]);
			if (NULL != fuda1651_fod_params[i].params)
				nuvolta_1665_set_fod(chip, &fuda1651_fod_params[i]);
			return;
		}
	}

	if (((chip->adapter_type == ADAPTER_QC3) || (chip->adapter_type == ADAPTER_PD))
		&& (!chip->epp)) {
		found = true;
		nuvolta_info("%s bpp plus\n", __func__);
		nuvolta_1665_set_bpp_plus_fod(chip, &fuda1651_bpp_plus_fod_param);
	} else if (chip->adapter_type >= ADAPTER_XIAOMI_QC3) {
		found = true;
		nuvolta_info("%s epp+ default\n", __func__);
		nuvolta_1665_set_fod(chip, &fuda1651_fod_param_default);
	}

	if (!found)
		nuvolta_info("%s can not found fod params, uuid: 0x%x,0x%x,0x%x,0x%x\n", __func__,
			chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

	return;
}

/*
static int nuvolta_1665_set_adapter_voltage(struct nuvolta_1665_chg *chip, int voltage)
{
	int ret = 0;
	bool status = true;
	u8 vol_h = 0, vol_l = 0;

	if ((voltage < 4000) || (voltage > 30000))
		voltage = 6000;

	vol_h = (u8)(voltage >> 8);
	vol_l = (u8)(voltage & 0xFF);

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return -1;	

	ret = rx1665_write(chip, 0x69, 0x0000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x05, 0x0001);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x02, 0x0002);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x38, 0x0003);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x0A, 0x0004);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, vol_l, 0x0005);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, vol_h, 0x0006);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x07, 0x0060);
	if (ret < 0)
		return ret;

	nuvolta_info("[%s] adapter voltage setted: %d\n", __func__, voltage);
	return ret;
}*/

static int nuvolta_1665_get_cep(struct nuvolta_1665_chg * chip, int *cep)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];

	if (!chip->power_good_flag) {
		*cep = 0;
		return ret;
	}

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return ret;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 30);
	if (ret < 0)
		return ret;

	*cep = read_buf[10];
	nuvolta_info("get rx cep: %d\n", *cep);

	return ret;
}

/*
static int nuvolta_1665_get_trx_cep(struct nuvolta_1665_chg * chip, int *trx_cep)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return ret;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 30);
	if (ret < 0)
		return ret;

	*trx_cep = read_buf[7];
	nuvolta_info("get trx cep: %d\n", *trx_cep);

	//reset trx cep 
	ret = rx1665_write(chip, 0x21, 0x0000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x00, 0x0001);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x02, 0x0060);
	if (ret < 0)
		return ret;

	return ret;
}*/

static int nuvolta_1665_set_vout(struct nuvolta_1665_chg * chip, int vout)
{
	int ret = 0;
	bool status = true;
	u8 vout_l = 0, vout_h = 0;
	int max_vol = 19500;
	int cep = 0;

	if (!chip->power_good_flag) {
		nuvolta_info("power good disonline, don't set vout\n");
		return 0;
	}

	if (chip->parallel_charge) {
		ret = nuvolta_1665_get_cep(chip, &cep);
		if (ret < 0) {
			nuvolta_info("get cep failed : %d\n", ret);
			return ret;
		} else if (ABS(cep) > ABS_CEP_VALUE) {
			nuvolta_info("[%s] vout: %d, cep: %d, not set vout\n", vout, cep);
			return 0;
		}
	}

	if (vout < 4000) {
		vout = 6000;
	} else if (vout > max_vol) {
		vout = max_vol;
	}

	vout_h = (u8)(vout >> 8);
	vout_l = (u8)(vout & 0xFF);

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x31, 0x0000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x02, 0x0001);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, vout_l, 0x0002);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, vout_h, 0x0003);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x04, 0x0060);
	if (ret < 0)
		return ret;

	chip->vout_setted = vout;
	nuvolta_info("set rx vout: %d\n", vout);

	return ret;
}

static int nuvolta_1665_get_vrect(struct nuvolta_1665_chg * chip, int *vrect)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];

	if (!chip->power_good_flag) {
		*vrect = 0;
		return ret;
	}

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return ret;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 30);
	if (ret < 0)
		return ret;

	*vrect = read_buf[19] * 256 +read_buf[18];
	nuvolta_info("get rx vrect: %d\n", *vrect);

	return ret;
}

static int nuvolta_1665_get_vout(struct nuvolta_1665_chg * chip, int *vout)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];
	s8 cep = 0;

	if (!chip->power_good_flag) {
		*vout = 0;
		return ret;
	}

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return ret;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 30);
	if (ret < 0)
		return ret;

	*vout = read_buf[21] * 256 +read_buf[20];
	cep = read_buf[10];
	nuvolta_info("get rx vout: %d, cep: %d\n", *vout, cep);
	chip->reverse_vout = *vout;

	return ret;
}

static int nuvolta_1665_get_iout(struct nuvolta_1665_chg * chip, int *iout)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];

	if (!chip->power_good_flag) {
		*iout = 0;
		return ret;
	}

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return ret;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 30);
	if (ret < 0)
		return ret;

	*iout = read_buf[17] * 256 +read_buf[16];

	nuvolta_info("get rx iout: %d\n", *iout);
	chip->reverse_iout = *iout;
	return ret;
}

static int nuvolta_1665_get_temp(struct nuvolta_1665_chg * chip, int *temp)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];

	if (!chip->power_good_flag) {
		*temp = 0;
		return ret;
	}

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return 0;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 30);
	if (ret < 0)
		return ret;

	*temp = read_buf[15] * 256 +read_buf[14];
	nuvolta_info("get rx temp: %d\n", *temp);

	return ret;
}

static int tx_info_update(struct nuvolta_1665_chg * chip, u8* buff)
{
	int ret = 0;

	ret = rx1665_write(chip, 0x88, 0x0000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x01, 0x0060);
	if (ret < 0)
		return ret;
	
	msleep(20);

	ret = rx1665_read_buffer(chip, buff, 0x1200, 256);
	if (ret < 0) {
		nuvolta_err("Update %s failed!\n", __func__);
		return ret;
	}

	return ret;
}

static int nuvolta_1665_get_reverse_vout(struct nuvolta_1665_chg * chip, int *vout)
{
	int ret = 0;

	*vout = 256 * sram_buffer[19] + sram_buffer[18];

	nuvolta_info("get tx reverse vout: %d\n", *vout);
	chip->reverse_vout = *vout;

	return ret;
}

static int nuvolta_1665_get_reverse_iout(struct nuvolta_1665_chg * chip, int *iout)
{
	int ret = 0;

	*iout = 256 * sram_buffer[17] + sram_buffer[16];

	nuvolta_info("get tx reverse iout: %d\n", *iout);
	chip->reverse_iout = *iout;

	if (chip->reverse_iout > 500 || chip->reverse_iout < 50) {
		curr_count++;
	}
	if (curr_count >= 5) {
		curr_count = 0;
		nuvolta_info("The pen position is out of the right place.\n");
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_mode = 0;
		chip->is_reverse_chg = 2;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
	}
	return ret;
}

static int nuvolta_1665_get_reverse_temp(struct nuvolta_1665_chg * chip, int *temp)
{
	int ret = 0;

	*temp = 256 * sram_buffer[15] + sram_buffer[14];

	nuvolta_info("get tx reverse temperature: %d\n", *temp);
	chip->reverse_temp = *temp;

	return ret;
}

#define SOC_100_RETRY 6
static int soc_count;
static int nuvolta_1665_get_reverse_soc(struct nuvolta_1665_chg * chip)
{
	int ret = 0;
	u8 soc = 0xFF;
	static int last_soc;

	soc = sram_buffer[9];

	if ((soc < 0) || (soc > 0x64)) {
		if (soc == 0xFF) {
			nuvolta_info("[reverse] soc is default 0xFF\n");
			chip->reverse_pen_soc = 0xFF;
			return ret;
		} else {
			nuvolta_info("[reverse] soc illegal: %d\n", soc);
			return ret;
		}
	}

	chip->reverse_pen_soc = soc + 1;
	if (chip->reverse_pen_soc > 100)
		chip->reverse_pen_soc = 100;
	nuvolta_info("get tx reverse raw_soc: %d, UI_soc:%d\n", soc, chip->reverse_pen_soc);
	if (chip->wireless_psy) {
		power_supply_changed(chip->wireless_psy);
	}

	if ((soc == 100) && (pen_soc_count < SOC_100_RETRY)) {
		nuvolta_info("[reverse] soc is 100 count: %d\n", pen_soc_count);
		pen_soc_count ++;
	} else {
		pen_soc_count = 0;
	}

	if ((soc - last_soc) == 0)
		soc_count++;
	else {
		nuvolta_info("pen soc is change soc_count=%d!\n", soc_count);
		soc_count = 0;
	}

	if (pen_soc_count == SOC_100_RETRY) {
		nuvolta_info("[reverse] soc is 100 exceed 6 times, disable reverse chg!\n");
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_mode = 0;
		chip->is_reverse_chg = 2;
		pen_soc_count = 0;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
	}

	if (soc_count >= 180) {
		nuvolta_info("Happen pen lock, need disable/enable reverse chg!\n");
		soc_count = 0;
		ret = nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_mode = 0;
		chip->is_reverse_chg = 2;
		msleep(30);
		ret = nuvolta_1665_set_reverse_chg_mode(chip, true);
	}

	last_soc = soc;

	return ret;
}



static void nuvolta_1665_set_pmic_icl(struct nuvolta_1665_chg * chip, int mA)
{
	if (chip->icl_votable)
		vote(chip->icl_votable, WLS_CHG_VOTER, true, mA);
	else
		nuvolta_err("no icl votable, don't set icl\n");

	nuvolta_info("wls set pmic icl: %d\n", mA);
	return;
}

static void nuvolta_1665_stepper_pmic_icl(struct nuvolta_1665_chg * chip,
	int start_icl, int end_icl, int step_ma, int ms)
{
	int temp_icl = start_icl;
	nuvolta_1665_set_pmic_icl(chip, temp_icl);

	if (start_icl < end_icl) {
		while (temp_icl < end_icl) {
			nuvolta_1665_set_pmic_icl(chip, temp_icl);
			temp_icl += step_ma;
			msleep(ms);
		}
	} else {
		while (temp_icl > end_icl) {
			nuvolta_1665_set_pmic_icl(chip, temp_icl);
			if (temp_icl > step_ma)
				temp_icl -= step_ma;
			else
				temp_icl = 0;
			msleep(ms);
		}
	}

	nuvolta_1665_set_pmic_icl(chip, end_icl);
	return;
}

static void nuvolta_1665_set_pmic_ichg(struct nuvolta_1665_chg * chip,int mA)
{
	if (chip->fcc_votable)
		vote(chip->fcc_votable, WLS_CHG_VOTER, true, mA);
	else
		nuvolta_err("no fcc votable, don't set fcc\n");

	nuvolta_info("wls set fcc: %d\n", mA);
	return;
}

static int nuvolta_1665_get_fcc(struct nuvolta_1665_chg * chip)
{
	int effective_fcc = 0;
	effective_fcc = get_effective_result(chip->fcc_votable);
	nuvolta_info("wls get fcc: %d\n", effective_fcc);
	return effective_fcc;
}

static void nuvolta_1665_epp_uuid_func(struct nuvolta_1665_chg *chip)
{
	u8 vendor = 0, module = 0, version = 0, power = 0;

	vendor  = chip->uuid[0];
	module  = chip->uuid[1];
	version = chip->uuid[2];
	power   = chip->uuid[3];
	nuvolta_info("epp uuid: vendor:0x%x, module:0x%x, version:0x%x, power:0x%x",
		vendor, module, version, power);

	if ((vendor == 0x9) && (module == 0x8) && (version == 0x6) && (power == 0x7))
		chip->is_music_tx = true;

	if (((vendor == 0x9) && (module == 0x1) && (version == 0x5) && (power == 0x6))
		|| ((vendor == 0xc) && (module == 0x9) && (version == 0x9) && (power == 0x8))
		|| ((vendor == 0xc) && (module == 0x9) && (version == 0x9) && (power == 0x6)))
		chip->is_plate_tx = true;

	if (((vendor == 0x6) && (module == 0x2) && (version == 0x8) && (power == 0x1))
		|| ((vendor == 0x1) && (module == 0x8) && (version == 0x2) && (power == 0x5)))
		chip->is_car_tx = true;

	if ((vendor == 0x1) && (module == 0x1) && (version == 0xE) && (power == 0x1))
		chip->is_train_tx = true;

	if ((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0x1))
		chip->is_standard_tx = true;

	if ((chip->is_car_tx) && (chip->adapter_type >= ADAPTER_XIAOMI_QC3)) {
		nuvolta_info("[TODO] is car tx");
		//TODO set wls car adapter node to 1
	}

	if (chip->is_music_tx)
		chip->adapter_type = ADAPTER_VOICE_BOX;

	return;
}

static int nuvolta_1665_send_transparent_data(struct nuvolta_1665_chg *chip,
	u8 *send_data, u8 length)
{
	int ret = 0;
	bool status = true;
	u8 i = 0;

	nuvolta_info("[%s] send_data[0]:0x%x, send_data[1]:0x%x, send_data[2]:0x%x, length:%d\n",
		send_data[0], send_data[1], send_data[2], length);

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x69, 0x0000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, length, 0x0001);
	if (ret < 0)
		return ret;

	for (i = 0; i < length; i++) {
		ret = rx1665_write(chip, send_data[i], (0x0002 + i));
		if (ret < 0)
			return ret;
		nuvolta_info("[%s] send_data[%d] = 0x%x, reg:0x%x\n", __func__, i, send_data[i], (0x0002 + i));
	}

	ret = rx1665_write(chip, (length + 2), 0x0060);
	if (ret < 0)
		return ret;

	return ret;
}

static void nuvolta_1665_process_factory_cmd(struct nuvolta_1665_chg *chip, u8 cmd)
{
	int ret = 0;
	u8 send_data[8] = {0};
	u8 data_h = 0, data_l = 0;
	u8 index = 0;
	int rx_iout = 0, rx_vout = 0;

	switch (cmd) {
	case FACTORY_TEST_CMD_RX_IOUT:
		ret = nuvolta_1665_get_iout(chip, &rx_iout);
		if (ret >= 0) {
			data_h = (rx_iout & 0x00ff);
			data_l = (rx_iout & 0xff00) >> 8;
		}
		index = 0;
		send_data[index++] = 0x02;
		send_data[index++] = 0x38;
		send_data[index++] = 0x12;
		send_data[index++] = data_h;
		send_data[index++] = data_l;
		ret = nuvolta_1665_send_transparent_data(chip, send_data, index);
		if (ret < 0)
			nuvolta_err("[%s] send transparent data failed\n", __func__);
		nuvolta_info("[%s] rx_iout: 0x%x, 0x%x, iout = %d\n", __func__,
			data_h, data_l, rx_iout);
		break;
	case FACTORY_TEST_CMD_RX_VOUT:
		ret = nuvolta_1665_get_vout(chip, &rx_vout);
		if (ret >= 0) {
			data_h = (rx_vout & 0x00ff);
			data_l = (rx_vout & 0xff00) >> 8;
		}
		index = 0;
		send_data[index++] = 0x02;
		send_data[index++] = 0x38;
		send_data[index++] = 0x13;
		send_data[index++] = data_h;
		send_data[index++] = data_l;
		ret = nuvolta_1665_send_transparent_data(chip, send_data, index);
		if (ret < 0)
			nuvolta_err("[%s] send transparent data failed\n", __func__);
		nuvolta_info("[%s] rx_vout: 0x%x, 0x%x, iout = %d\n", __func__,
			data_h, data_l, rx_vout);
		break;
	case FACTORY_TEST_CMD_RX_FW_ID:
		index = 0;
		send_data[index++] = 0x02;
		send_data[index++] = 0x58;
		send_data[index++] = 0x24;
		send_data[index++] = 0x0;
		send_data[index++] = 0x0;
		send_data[index++] = g_wls_fw_data.fw_rx_id;
		send_data[index++] = g_wls_fw_data.fw_tx_id;
		ret = nuvolta_1665_send_transparent_data(chip, send_data, index);
		if (ret < 0)
			nuvolta_err("[%s] send transparent data failed\n", __func__);
		nuvolta_info("[%s] fw_version: 0x%x0x%x\n", __func__,
			g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);
		break;
	case FACTORY_TEST_CMD_RX_CHIP_ID:
		index = 0;
		send_data[index++] = 0x02;
		send_data[index++] = 0x38;
		send_data[index++] = 0x23;
		send_data[index++] = 16;
		send_data[index++] = 51;
		ret = nuvolta_1665_send_transparent_data(chip, send_data, index);
		if (ret < 0)
			nuvolta_err("[%s] send transparent data failed\n", __func__);
		nuvolta_info("[%s] chip id: 0x%x0x%x\n", __func__,
			g_wls_fw_data.hw_id_h, g_wls_fw_data.hw_id_l);
		break;
	case FACTORY_TEST_CMD_ADAPTER_TYPE:
		index = 0;
		send_data[index++] = 0x02;
		send_data[index++] = 0x28;
		send_data[index++] = 0x0b;
		send_data[index++] = chip->adapter_type;
		ret = nuvolta_1665_send_transparent_data(chip, send_data, index);
		if (ret < 0)
			nuvolta_err("[%s] send transparent data failed\n", __func__);
		nuvolta_info("[%s] adapter type: %d\n", __func__, chip->adapter_type);
		break;
	case FACTORY_TEST_CMD_REVERSE_REQ:
		index = 0;
		send_data[index++] = 0x02;
		send_data[index++] = 0x18;
		send_data[index++] = 0x30;
		ret = nuvolta_1665_send_transparent_data(chip, send_data, index);
		if (ret < 0)
			nuvolta_err("[%s] send transparent data failed\n", __func__);
		chip->wait_for_reverse_test = true;
		chip->wait_for_reverse_test_status = 1;
		nuvolta_info("[%s] reverse charge start\n", __func__);
		break;
	default:
		nuvolta_info("[%s] unknown cmd: %d\n", __func__, cmd);
		break;
	}

	return;
}

static void nuvolta_1665_rcv_factory_test_cmd(struct nuvolta_1665_chg *chip,
	u8 *rev_data, u8 *length)
{
	int ret = 0;
	bool status = true;
	u8 read_buf[128];
	u8 data_length = 0, i = 0;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 128);
	if (ret < 0)
		return;

	data_length = read_buf[56];
	*length = data_length;
	nuvolta_info("%s data length: %d\n", __func__, data_length);

	for (i = 0; i < data_length; i++) {
		rev_data[i] = read_buf[57 + i];
		nuvolta_info("%s data[%d] = 0x%x\n", __func__, i, rev_data[i]);
	}

	return;
}

static void nuvolta_1665_get_tx_manu_id(struct nuvolta_1665_chg *chip)
{
	u8 tx_manu_id_l = 0;
	u8 tx_manu_id_h = 0;
	bool status = true;
	int ret = 0;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return;

	ret = rx1665_read(chip, &tx_manu_id_l, TX_MANU_ID_L);
	ret = rx1665_read(chip, &tx_manu_id_h, TX_MANU_ID_H);
	if (ret < 0)
		return;

	chip->tx_manu_id_l = tx_manu_id_l;
	chip->tx_manu_id_h = tx_manu_id_h;

	nuvolta_info("tx manu id l: 0x%x, h: 0x%x\n", tx_manu_id_l, tx_manu_id_h);
	return;
}

static u8 nuvolta_1665_get_rx_power_mode(struct nuvolta_1665_chg *chip)
{
	u8 power_mode = 0;
	u8 read_buf[5] = {0};
	bool status = true;
	int ret = 0;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return 0;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return 0;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 5);
	if (ret < 0)
		return 0;

	power_mode = read_buf[3];
	nuvolta_info("rx power mode: %d\n", power_mode);
	nuvolta_info("boot: 0x%x, rx: 0x%x, tx: 0x%x: %d\n",
		read_buf[0], read_buf[1], read_buf[2]);

	return power_mode;
}

static u8 nuvolta_1665_get_fastchg_result(struct nuvolta_1665_chg *chip)
{
	u8 fastchg_result = 0;
	bool status = true;
	int ret = 0;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return fastchg_result;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return fastchg_result;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return fastchg_result;

	ret = rx1665_read(chip, &fastchg_result, RX_FASTCHG_RESULT);
	if (ret < 0)
		return fastchg_result;

	nuvolta_info("[%s] fastch result: %d\n", __func__, fastchg_result);

	return fastchg_result;
}

static u8 nuvolta_1665_get_auth_value(struct nuvolta_1665_chg *chip)
{
	u8 read_buf[40];
	u8 auth_data = 0;
	bool status = true;
	int ret = 0;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return auth_data;

	ret = rx1665_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return auth_data;

	status = nuvolta_1665_check_buffer_ready(chip);
	if (!status)
		return auth_data;

	ret = rx1665_read_buffer(chip, read_buf, RX_DATA_INFO, 40);
	if (ret < 0)
		return auth_data;

	auth_data = read_buf[8];
	nuvolta_info("[%s] auth_data = 0x%x\n", __func__, auth_data);

	if (auth_data > AUTH_STATUS_FAILED) {
		chip->epp_tx_id_l = read_buf[26];
		chip->epp_tx_id_h = read_buf[27];
	}

	chip->adapter_type = read_buf[7];
	if (chip->adapter_type == ADAPTER_NONE)
		chip->adapter_type = ADAPTER_AUTH_FAILED;

	if (auth_data > AUTH_STATUS_UUID_OK) {
		chip->uuid[0] = read_buf[28];
		chip->uuid[1] = read_buf[29];
		chip->uuid[2] = read_buf[30];
		chip->uuid[3] = read_buf[31];
	}

	nuvolta_info("[%s] tx_id_l: 0x%x, tx_id_h: 0x%x\n",
		__func__, chip->epp_tx_id_l, chip->epp_tx_id_h);
	nuvolta_info("[%s] adapter type: %d\n", __func__, chip->adapter_type);
	nuvolta_info("[%s] uuid: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
		chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);
	return auth_data;
}

/*static void nuvolta_1665_get_rx_rtx_mode(struct nuvolta_1665_chg *chip, u8 *mode)
{
	u8 data = 0;

	rx1665_read(chip, &data, RX_RTX_MODE);
	nuvolta_info("[%s] data = %d\n", __func__, data);
	if (data == 0x03)
		*mode = RTX_MODE;
	else
		*mode = RX_MODE;
	return;
}*/

static void nuvolta_1665_power_off_err(struct nuvolta_1665_chg *chip)
{
	int ret = 0;
	u8 err_code = 0;

	ret = rx1665_read(chip, &err_code, RX_POWER_OFF_ERR);
	if (ret < 0)
		return;

	/*unknown:0x00 otp:0x03 ovp:0x04 ocp:0x05 sc:0x06 hop:0x10
	 *sop:0x11 sleep:0x0B ovl:0x13 vup:0x14 rect_err:0x15
	 */
	nuvolta_info("[%s] power off err = 0x%x\n", __func__, err_code);
	return;
}

static void nuvolta_1665_do_renego(struct nuvolta_1665_chg *chip, u8 max_power)
{
	bool status = true;
	int ret = 0;

	nuvolta_info("[%s] max_power = %d\n", __func__, max_power);

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return;

	ret = rx1665_write(chip, 0xA8, 0x0000);
	if (ret < 0)
		return;	

	ret = rx1665_write(chip, 0x01, 0x0001);
	if (ret < 0)
		return; 

	ret = rx1665_write(chip, max_power, 0x0002);
	if (ret < 0)
		return; 

	ret = rx1665_write(chip, 0x03, 0x0060);
	if (ret < 0)
		return; 
	return;
}

static void nuvolta_1665_adapter_handle(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("[%s] adapter: %d, epp: %d\n", __func__,
		chip->adapter_type, chip->epp);

	if (!chip->fc_flag) {
		switch (chip->adapter_type) {
		case ADAPTER_SDP:
		case ADAPTER_CDP:
		case ADAPTER_DCP:
		case ADAPTER_QC2:
			nuvolta_info("set icl for SDP/CDP/DCP/QC2 adapter\n");
			nuvolta_1665_set_pmic_icl(chip, 750);
			nuvolta_1665_set_pmic_ichg(chip, 1000);
			chip->pre_curr = 750;
			break;
		case ADAPTER_QC3:
		case ADAPTER_PD:
		case ADAPTER_AUTH_FAILED:
			if (chip->epp) {
				nuvolta_info("set icl in EPP for QC3/PD/FAIL adapter\n");
				nuvolta_1665_set_pmic_icl(chip, 850);
				nuvolta_1665_set_pmic_ichg(chip, 2000);
				chip->pre_curr = 850;
			} else {
				nuvolta_info("set icl in BPP for QC3/PD/FAIL adapter\n");
				nuvolta_1665_set_pmic_icl(chip, 750);
				nuvolta_1665_set_pmic_ichg(chip, 1000);
				chip->pre_curr = 750;
				chip->pre_vol = BPP_DEFAULT_VOUT;
			}
			break;
		case ADAPTER_XIAOMI_QC3:
		case ADAPTER_XIAOMI_PD:
		case ADAPTER_ZIMI_CAR_POWER:
		case ADAPTER_XIAOMI_PD_40W:
		case ADAPTER_VOICE_BOX:
		case ADAPTER_XIAOMI_PD_50W:
		case ADAPTER_XIAOMI_PD_60W:
		case ADAPTER_XIAOMI_PD_100W:
			nuvolta_info("set icl for adapter more than 9\n");
			nuvolta_1665_set_pmic_icl(chip, 850);
			nuvolta_1665_set_pmic_ichg(chip, 2000);
			chip->pre_curr = 850;
			chip->pre_vol = EPP_DEFAULT_VOUT;
			break;
		default:
			nuvolta_info("other adapter type\n");
			break;
		}
	} else {
		switch (chip->adapter_type) {
		case ADAPTER_QC3:
		case ADAPTER_PD:
			msleep(2000);
			nuvolta_info("set icl for QC3/PD in FC\n");
			nuvolta_1665_stepper_pmic_icl(chip, 800, 1100, 100, 20);
			nuvolta_1665_set_pmic_ichg(chip, 2000);
			chip->pre_curr = 1100;
			chip->pre_vol = BPP_PLUS_VOUT;
			break;
		case ADAPTER_XIAOMI_QC3:
		case ADAPTER_XIAOMI_PD:
		case ADAPTER_ZIMI_CAR_POWER:
			nuvolta_info("set fcc for 20W adapter\n");
			nuvolta_1665_set_pmic_ichg(chip, 4000);
			chip->pre_vol = EPP_DEFAULT_VOUT;
			chip->qc_enable = true;
			break;
		case ADAPTER_XIAOMI_PD_40W:
		case ADAPTER_VOICE_BOX:
			nuvolta_info("set fcc for 30W adapter\n");
			nuvolta_1665_set_pmic_ichg(chip, 6000);
			chip->pre_vol = EPP_DEFAULT_VOUT;
			chip->qc_enable = true;
			break;
		case ADAPTER_XIAOMI_PD_50W:
		case ADAPTER_XIAOMI_PD_60W:
		case ADAPTER_XIAOMI_PD_100W:
			nuvolta_info("set fcc for adapters of more than 50W\n");
			nuvolta_1665_set_pmic_ichg(chip, 9200);
			chip->pre_vol = EPP_DEFAULT_VOUT;
			chip->qc_enable = true;
			break;
		default:
			nuvolta_info("other adapter type, break\n");
			break;
		}
	}

	if (chip->wireless_psy) {
		power_supply_changed(chip->wireless_psy);
	}

	schedule_delayed_work(&chip->chg_monitor_work,
			msecs_to_jiffies(1000));
	return;
}

static void nuvolta_1665_start_renego(struct nuvolta_1665_chg *chip)
{
	u8 max_power = 0;

	switch (chip->adapter_type) {
	case ADAPTER_XIAOMI_PD_50W:
		max_power = 20;
		break;
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		max_power = 20;
		break;
	default:
		break;
	}

	if (max_power > 0)
		nuvolta_1665_do_renego(chip, max_power);
	return;
}

/*
static void nuvolta_1665_set_fastchg_adapter_v(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	switch (chip->adapter_type) {
	case ADAPTER_QC3:
	case ADAPTER_PD:
		if (!chip->epp) {
			nuvolta_info("bpp+ set adapter voltage to 9V\n");
			ret = nuvolta_1665_set_adapter_voltage(chip, BPP_PLUS_VOUT);
			if (ret < 0)
				nuvolta_info("bpp+ set adapter voltage failed!!!\n");
		}
		break;
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_50W:
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		// close pmic ovp trigger befor raise voltage to 15V
		//nuvolta_1665_set_pmic_icl(chip, 200);
		//msleep(100);
		if (!chip->cp_master_dev)
			chip->cp_master_dev = get_charger_by_name("cp_master");
		if (chip->cp_master_dev) {
			charger_dev_enable_pmic_ovp(chip->cp_master_dev, false);
			nuvolta_info("disable pmic ovp\n");
		}

		nuvolta_info("EPP+ set adapter voltage to 15V\n");
		ret = nuvolta_1665_set_adapter_voltage(chip, EPP_PLUS_VOUT);
		if (ret < 0)
			nuvolta_info("epp+ set adapter voltage failed!!!\n");
		break;
	default:
		nuvolta_info("other adapter, don't set adapter voltage\n");
		break;
	}
	return;
}
*/

static int nuvolta_1665_enable_power_path(bool en)
{
	static struct power_supply *chg_psy;
	union power_supply_propval val;

	nuvolta_info("%s: %d\n", __func__, en);

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		nuvolta_err("%s Couldn't get chg_psy\n", __func__);
		return -1;
	}

	val.intval = !en;
	return power_supply_set_property(chg_psy,
					 POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
					 &val);
	return 0;
}

static int nuvolta_1665_enable_bc12(struct nuvolta_1665_chg *chip, bool attach)
{
	int ret = 0;
	union power_supply_propval prop;
	static struct power_supply *bc12_psy;

	bc12_psy = power_supply_get_by_name("xmusb350");
	if (IS_ERR_OR_NULL(bc12_psy)) {
		nuvolta_err("%s Couldn't get bc12_psy\n", __func__);
		return ret;
	} else {
		prop.intval = attach;
		return power_supply_set_property(bc12_psy,
					 POWER_SUPPLY_PROP_ONLINE, &prop);
	}
}

static void nuvolta_1665_clear_int(struct nuvolta_1665_chg *chip)
{
	//bool status = true;
	int ret = 0;

	/*status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return;*/

	ret = rx1665_write(chip, 0x68, 0x0000);
	ret = rx1665_write(chip, 0x01, 0x0060);
	/*ret = rx1665_write(chip, 0x02, 0x0001);
	ret = rx1665_write(chip, 0xff, 0x0002);
	ret = rx1665_write(chip, 0xff, 0x0003);
	ret = rx1665_write(chip, 0x04, 0x0060);*/

	nuvolta_info("[%s] ret: %d\n", __func__, ret);
	return;
}
static void reverse_chg_sent_state_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
	    container_of(work, struct nuvolta_1665_chg,
			 reverse_sent_state_work.work);
	union power_supply_propval val = { 0, };
	int rc = 0;

	if (chip->wireless_psy) {
		if (chip->is_reverse_chg == 4) {
			rc = tx_info_update(chip, sram_buffer);
			nuvolta_1665_get_reverse_soc(chip);
		}
		val.intval = chip->is_reverse_chg;
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_REVERSE_PEN_CHG_STATE,
					  &val);
		power_supply_changed(chip->wireless_psy);
		nuvolta_info("uevent: %d for reverse charging state\n",
			 chip->is_reverse_chg);
	} else
		nuvolta_info("get wls property error\n");
}

static void reverse_chg_state_set_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		 container_of(work, struct nuvolta_1665_chg,
					reverse_chg_state_work.work);
	int ret;

	nuvolta_info("no rx found and disable reverse charging\n");
	mutex_lock(&chip->reverse_op_lock);
	ret = nuvolta_1665_set_reverse_chg_mode(chip, false);
	chip->is_reverse_chg = REVERSE_STATE_TIMEOUT;
	chip->is_reverse_mode = 0;
	mutex_unlock(&chip->reverse_op_lock);

	schedule_delayed_work(&chip->reverse_sent_state_work, 0);

	return;
}

static void reverse_dping_state_set_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		 container_of(work, struct nuvolta_1665_chg,
					reverse_dping_state_work.work);
	int ret;

	nuvolta_info("tx mode fault and disable reverse charging\n");
	mutex_lock(&chip->reverse_op_lock);
	ret = nuvolta_1665_set_reverse_chg_mode(chip, false);
	chip->is_reverse_mode = 0;
	chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
	mutex_unlock(&chip->reverse_op_lock);

	schedule_delayed_work(&chip->reverse_sent_state_work, 0);

	return;
}

static enum alarmtimer_restart reverse_chg_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct nuvolta_1665_chg *chip =
		 container_of(alarm, struct nuvolta_1665_chg,
					reverse_chg_alarm);

	nuvolta_info(" Reverse Chg Alarm Triggered %lld\n", ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->reverse_chg_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart reverse_dping_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct nuvolta_1665_chg *chip =
		 container_of(alarm, struct nuvolta_1665_chg,
					reverse_dping_alarm);

	nuvolta_info("Reverse Dping Alarm Triggered %lld\n", ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->reverse_dping_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static int nuvolta_1665_reverse_enable_fod(struct nuvolta_1665_chg *chip, bool enable)
{
	int ret = 0;
	bool status = true;
	u8 gain = REVERSE_FOD_GAIN;
	u8 offset = REVERSE_FOD_OFFSET;

	if (!enable)
		return ret;

	status = nuvolta_1665_check_rx_ready(chip);
	if (!status)
		return -1;

	ret = rx1665_write(chip, 0x23, 0x0000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x01, 0x0001);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, gain, 0x0002);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, offset, 0x0003);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x04, 0x0060);
	if (ret < 0)
		return ret;

	nuvolta_info("[%s] gain: %d, offset: %d\n", __func__, gain, offset);
	return ret;
}

static void nuvolta_1665_reverse_chg_handler(struct nuvolta_1665_chg *chip, u16 int_flag)
{
	int rc = 0;

	if (int_flag & RTX_INT_EPT) {
		alarm_start_relative(&chip->reverse_dping_alarm,
				ms_to_ktime(REVERSE_DPING_CHECK_DELAY_MS));
		if (tx_info_update(chip, sram_buffer) >= 0)
			nuvolta_info("tx mode ept. the code:0x%02x\n", sram_buffer[10]);
		goto out;
	}

	if (int_flag & INT_GET_DPING) {
		nuvolta_info("TRX get dping and disable reverse charging \n");
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_mode = 0;
		chip->is_reverse_chg = 2;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
		goto out;
	}

	if (int_flag & RTX_INT_START_DPING) {
		if (!chip->alarm_flag) {
			alarm_start_relative(&chip->reverse_chg_alarm,
				ms_to_ktime(REVERSE_CHG_CHECK_DELAY_MS));
			chip->alarm_flag = true;
		}

		rc = alarm_cancel(&chip->reverse_dping_alarm);
		if (rc < 0)
			nuvolta_err("Couldn't cancel reverse_dping_alarm\n");
		pm_relax(chip->dev);
		nuvolta_info("tx mode ping\n");
	}

	if (int_flag & RTX_INT_GET_CFG) {
		rc = alarm_cancel(&chip->reverse_chg_alarm);
		if (rc < 0)
			nuvolta_err("Couldn't cancel reverse_chg_alarm\n");

		pm_stay_awake(chip->dev);
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);

		/* set reverse charging state to started*/
		nuvolta_1665_reverse_enable_fod(chip, true);
		//chip->is_reverse_chg = REVERSE_STATE_TRANSFER;
		msleep(50);
		
		//start reverse chg infor work
		cancel_delayed_work_sync(&chip->reverse_chg_work);
		msleep(10);
		schedule_delayed_work(&chip->reverse_chg_work, 0);
		/* set reverse charging state to started */
		if (chip->is_reverse_mode || chip->is_boost_mode) {
			chip->is_reverse_chg = 4;
			nuvolta_info("notify pmic reverse charging!\n");
			schedule_delayed_work(&chip->reverse_sent_state_work, 100);
		}	
		nuvolta_info("tx mode get rx\n");
	}

	if (int_flag & INT_GET_SS) 
		nuvolta_info("TRX get ss\n");
	if (int_flag & INT_GET_ID) 
		nuvolta_info("TRX get id\n");
	if (int_flag & INT_INIT_TX) 
		nuvolta_info("TRX reset done\n");
		
	if (int_flag & INT_GET_BLE_ADDR) {
		nuvolta_info(" TRX get ble mac\n");
		fuda_trx_get_ble_mac(chip);
	}

	if (int_flag & INT_GET_PPP) {
		nuvolta_info("INT_GET_PPP.\n");
		if (tx_info_update(chip, sram_buffer) >= 0) {
			nuvolta_info("receive dates: 0x%02x: ,0x%02x: ,0x%02x: ,0x%02x: ,0x%02x: ,0x%02x: ,0x%02x: ,0x%02x:\n", sram_buffer[42],
			sram_buffer[43], sram_buffer[44], sram_buffer[45], sram_buffer[46], sram_buffer[47], sram_buffer[48], sram_buffer[49]);
		}
	}

	/*case RTX_INT_CEP_TIMEOUT:
		alarm_start_relative(&chip->reverse_dping_alarm,
				ms_to_ktime(REVERSE_DPING_CHECK_DELAY_MS));
		chip->is_reverse_chg = REVERSE_STATE_WAITPING;
		nuvolta_info("tx mode cep timeout\n");
		break;
	case RTX_INT_PROTECTION:
		nuvolta_1665_power_off_err(chip);
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
		nuvolta_info("tx mode protection\n");
		break;
	case RTX_INT_GET_TX:
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
		nuvolta_info("tx mode get tx\n");
		break;
	case RTX_INT_REVERSE_TEST_READY:
		chip->wait_for_reverse_test_status = 3;
		nuvolta_info("tx mode reverse test ready, cancel timer\n");
		cancel_delayed_work(&chip->factory_reverse_stop_work);
		break;
	case RTX_INT_REVERSE_TEST_DONE:
		chip->wait_for_reverse_test_status = 4;
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		nuvolta_info("tx mode reverse test done\n");
		break;
	case RTX_INT_FOD:
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
		break;*/
out:
	if (chip->wireless_psy) {
		if (chip->reverse_pen_soc != 0xFF)
			power_supply_changed(chip->wireless_psy);
	}

	return;
}

static void nuvolta_1665_chg_handler(struct nuvolta_1665_chg *chip, u16 int_flag)
{
	u8 auth_status = 0;
	u8 rcv_value[128] = {0};
	u8 val_length = 0;
	int vout = 0;

	switch (int_flag) {
	case RX_INT_POWER_ON:
		chip->epp = nuvolta_1665_get_rx_power_mode(chip);
		nuvolta_info("[%s] RX_INT_POWER_ON epp: %d\n", __func__, chip->epp);
		break;
	case RX_INT_LDO_ON:
		//TODO disable aicl
		nuvolta_info("[%s] RX_INT_LDO_ON!\n", __func__);
		chip->epp = nuvolta_1665_get_rx_power_mode(chip);

		g_wls_fw_data.hw_id_h = 0x16;
		g_wls_fw_data.hw_id_l = 0x19;
		/* enable xmusb350 apsd */
		nuvolta_1665_enable_bc12(chip, true);
		nuvolta_1665_enable_power_path(true);

		if (chip->epp)
			nuvolta_1665_stepper_pmic_icl(chip, 200, 800, 100, 20);
		else
			nuvolta_1665_stepper_pmic_icl(chip, 250, 750, 100, 20);

		nuvolta_1665_get_tx_manu_id(chip);
		break;
	case RX_INT_AUTHEN_FINISH:
		nuvolta_info("[%s] authen finish!\n", __func__);
		auth_status = nuvolta_1665_get_auth_value(chip);
		if (auth_status != AUTH_STATUS_FAILED) {
			if (auth_status >= AUTH_STATUS_UUID_OK)
				nuvolta_1665_set_fod_params(chip);
			if (chip->epp)
				nuvolta_1665_epp_uuid_func(chip);

			if (chip->adapter_type >= ADAPTER_XIAOMI_PD_50W)
				nuvolta_1665_start_renego(chip);
			else {
				nuvolta_1665_adapter_handle(chip);
				//nuvolta_1665_set_fastchg_adapter_v(chip);
			}
		} else {
			nuvolta_info("[%s] authen failed!\n", __func__);
			nuvolta_1665_adapter_handle(chip);
		}
		break;
	case RX_INT_RENEGO_DONE:
		nuvolta_info("[%s] renego done\n", __func__);
		//nuvolta_1665_set_fastchg_adapter_v(chip);
		break;
	case RX_INT_FAST_CHARGE:
		nuvolta_info("[%s] fastchg finish!\n", __func__);
		chip->fc_flag = nuvolta_1665_get_fastchg_result(chip);
		if (chip->fc_flag) {
			if (chip->adapter_type >= ADAPTER_XIAOMI_QC3) {
				nuvolta_1665_set_vout(chip, EPP_DEFAULT_VOUT);
				msleep(2000);
				nuvolta_1665_get_vout(chip, &vout);
				if (vout < 12000) {
					//if (!chip->cp_master_dev)
						//chip->cp_master_dev = get_charger_by_name("cp_master");
					if (chip->cp_master_dev) {
						//charger_dev_enable_pmic_ovp(chip->cp_master_dev, true);
						nuvolta_info("enable pmic ovp en");
					}
					//nuvolta_1665_stepper_pmic_icl(chip, 200, 800, 100, 20);
				}
			}
			nuvolta_1665_adapter_handle(chip);
			//TODO
			//set tx fan speed tx_speed
			//if chip->epp, enable vdd
		} else if (chip->set_fastcharge_vout_cnt++ < 3) {
			nuvolta_info("set fastchg vol failed, retry %d\n",
				chip->set_fastcharge_vout_cnt);
			msleep(2000);
			//nuvolta_1665_set_fastchg_adapter_v(chip);
		} else {
			nuvolta_info("set fastchg vol failed finally\n");
			nuvolta_1665_adapter_handle(chip);
			//if (chip->adapter_type >= ADAPTER_XIAOMI_QC3)
				//charger_dev_enable_pmic_ovp(chip->cp_master_dev, true);
		}
		break;
	case RX_INT_OCP_OTP_ALARM:
		nuvolta_info("[%s] OCP OR OTP trigger\n", __func__);
		schedule_delayed_work(&chip->rx_alarm_work,
					msecs_to_jiffies(500));
		break;
	case RX_INT_POWER_OFF:
		nuvolta_info("[%s] POWER OFF INT trigger\n", __func__);
		nuvolta_1665_power_off_err(chip);
		break;
	case RX_INT_FACTORY_TEST:
		nuvolta_info("[%s] factory test\n", __func__);
		nuvolta_1665_rcv_factory_test_cmd(chip, rcv_value, &val_length);
		nuvolta_info("[%s] factory test: 0x%x, 0x%x, 0x%x\n", __func__,
			rcv_value[0], rcv_value[1],rcv_value[2]);
		if (rcv_value[0] == FACTORY_TEST_CMD)
			nuvolta_1665_process_factory_cmd(chip, rcv_value[1]);
		break;
	default:
		break;
	}

	return;
}

static void nu1665_dump_regs(struct nuvolta_1665_chg *chip)
{
	u8 int_l = 0;
	int ret = 0;

	ret = rx1665_read(chip, &int_l, REG_RX_REV_CMD); //0x0020
	if (ret < 0) {
		nuvolta_err("%s read int 0x20 error\n", __func__);
		goto exit;
	}
	//nuvolta_info("Print reg info. 0x20: 0x%x\n", int_l);

	ret = rx1665_read(chip, &int_l, 0x0021); //0x0021
	if (ret < 0) {
		nuvolta_err("%s read int 0x21 error\n", __func__);
		goto exit;
	}
	//nuvolta_info("Print reg info. 0x21: 0x%x\n", int_l);

	ret = rx1665_read(chip, &int_l, 0x0022); //0x0022
	if (ret < 0) {
		nuvolta_err("%s read int 0x22 error\n", __func__);
		goto exit;
	}
	//nuvolta_info("Print reg info. 0x22: 0x%x\n", int_l);

	ret = rx1665_read(chip, &int_l, 0x0023); //0x0022
	if (ret < 0) {
		nuvolta_err("%s read int 0x23 error\n", __func__);
		goto exit;
	}
	//nuvolta_info("Print reg info. 0x23: 0x%x\n", int_l);
exit:
	return;
}

static void nuvolta_1665_wireless_int_work(struct work_struct *work)
{
	u16 int_flags = 0;
	u8 tmp = 0; //int_l = 0, int_h = 0,
	u8 int_trx_mode = RTX_MODE;
	int ret = 0;
	int irq_level;

	struct nuvolta_1665_chg *chip = container_of(work,
		struct nuvolta_1665_chg, wireless_int_work.work);

	if (gpio_is_valid(chip->irq_gpio))
		irq_level = gpio_get_value(chip->irq_gpio);
	else {
		nuvolta_err("%s: irq gpio not provided\n", __func__);
		irq_level = -1;
		pm_relax(chip->dev);
		return;
	}
	nuvolta_info("irq gpio status: %d\n", irq_level);
	if (irq_level) {
		nuvolta_info("irq is high level, ignore%d\n", irq_level);
		pm_relax(chip->dev);
		return;
	}
	mutex_lock(&chip->wireless_chg_int_lock);

	ret = rx1665_read(chip, &tmp, REG_RX_REV_CMD); //0x0020
	if (ret < 0) {
		nuvolta_err("%s read int 0x20 error\n", __func__);
		goto exit;
	}
	int_flags |= tmp;
	tmp = 0;

	ret = rx1665_read(chip, &tmp, REG_RX_REV_DATA1); //0x0021
	if (ret < 0) {
		nuvolta_err("%s read int 0x21 error\n", __func__);
		goto exit;
	}
    int_flags |= (tmp << 8);
    tmp = 0;

    if (rx1665_read(chip, &tmp, 0x0022) < 0) {
		nuvolta_err("%s read int 0x22 error\n", __func__);
		goto exit;
	}
    int_flags |= (tmp << 16);
    tmp = 0;

    if (rx1665_read(chip, &tmp, 0x0023) < 0) {
		nuvolta_err("%s read int 0x23 error\n", __func__);
		goto exit;
	}
    int_flags |= (tmp << 24);

	nu1665_dump_regs(chip);
	//int_flag = (int_h << 8) | int_l;
	nuvolta_info("int_flag: 0x%x\n", int_flags);
	nuvolta_1665_clear_int(chip);

	//nuvolta_1665_get_rx_rtx_mode(chip, &int_trx_mode);

	nuvolta_info("ic work only rtx mode\n");
	if (int_trx_mode == RTX_MODE) {
		nuvolta_info("ic work on rtx mode\n");
		nuvolta_1665_reverse_chg_handler(chip, int_flags);
	} else {
		nuvolta_info("ic work on rx mode\n");
		nuvolta_1665_chg_handler(chip, int_flags);
	}


exit:
	mutex_unlock(&chip->wireless_chg_int_lock);
	return;
}

static irqreturn_t nuvolta_1665_interrupt_handler(int irq, void *dev_id)
{
	struct nuvolta_1665_chg *chip = dev_id;

	nuvolta_info("[%s]\n", __func__);
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->wireless_int_work, 0);

	return IRQ_HANDLED;
}

static void nuvolta_1665_reset_parameters(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("%s\n", __func__);

	chip->power_good_flag = 0;
	chip->ss = 2;
	chip->epp = 0;
	chip->qc_enable = false;
	chip->chg_phase = NORMAL_MODE;
	chip->adapter_type = 0;
	chip->fc_flag = 0;
	chip->set_fastcharge_vout_cnt = 0;
	chip->is_car_tx = false;
	chip->is_music_tx = false;
	chip->is_plate_tx = false;
	chip->is_train_tx = false;
	chip->is_standard_tx = false;
	chip->parallel_charge = false;
	chip->reverse_chg_en = false;
	chip->alarm_flag = false;

	return;
}

static void nuvolta_1665_pg_det_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip = container_of(work, struct nuvolta_1665_chg, wireless_pg_det_work.work);
	int ret = 0, wls_switch_usb = 0;

	if (!chip->wireless_psy) {
		chip->wireless_psy = power_supply_get_by_name("wireless");
		if (!chip->wireless_psy)
			nuvolta_err("failed to get wireless psy\n");
	}

	if (gpio_is_valid(chip->power_good_gpio)) {
		ret = gpio_get_value(chip->power_good_gpio);
		if (ret) {
			nuvolta_info("power_good high, wireless attached\n");
			chip->power_good_flag = 1;
			chip->adapter_type = ADAPTER_SDP;
			nuvolta_1665_set_pmic_icl(chip, 0);
			cancel_delayed_work(&chip->delay_report_status_work);
			cancel_delayed_work(&chip->rx_enable_usb_work);
			//TODO: set usb suspend
			//TODO: high soc > 70%, limit fcc to 7A
			//TODO: auto judge firmware update
		} else {
			nuvolta_info("power_good low, wireless detached\n");
			nuvolta_1665_reset_parameters(chip);

			cancel_delayed_work(&chip->chg_monitor_work);
			cancel_delayed_work(&chip->max_power_control_work);
			cancel_delayed_work(&chip->rx_alarm_work);
			schedule_delayed_work(&chip->rx_enable_usb_work, msecs_to_jiffies(500));
			schedule_delayed_work(&chip->delay_report_status_work, msecs_to_jiffies(2000));

			nuvolta_1665_set_pmic_icl(chip, 0);
			if (chip->icl_votable)
				vote(chip->icl_votable, WLS_CHG_VOTER, false, 0);
			if (chip->fcc_votable)
				vote(chip->fcc_votable, WLS_CHG_VOTER, false, 0);

			//wls_get_property(WLS_PROP_SWITCH_USB, &wls_switch_usb);
			nuvolta_info("wireless switch to usb: %d\n", wls_switch_usb);
			if (!wls_switch_usb) {
				nuvolta_1665_enable_bc12(chip, false);
				nuvolta_1665_enable_power_path(false);
			}

			if (chip->wait_for_reverse_test) {
				nuvolta_info("factory reverse charge start\n");
				schedule_delayed_work(&chip->factory_reverse_start_work, msecs_to_jiffies(2000));
			}

			//TODO: disable vdd
			//TODO: enable aicl
		}
		if (chip->wireless_psy) {
			power_supply_changed(chip->wireless_psy);
		}
	}
}

static void nuvolta_1665_get_charge_phase(struct nuvolta_1665_chg *chip, int *chg_phase)
{
	switch (*chg_phase) {
	case NORMAL_MODE:
		if (chip->batt_soc == 100) {
			*chg_phase = TAPER_MODE;
			nuvolta_info("change normal mode to tapter mode");
		}
		break;
	case TAPER_MODE:
		if ((chip->batt_soc == 100) && (chip->chg_status == POWER_SUPPLY_STATUS_FULL)) {
			*chg_phase = FULL_MODE;
			nuvolta_info("change taper mode to full mode");
		} else if (chip->batt_soc < 99) {
			*chg_phase = NORMAL_MODE;
			nuvolta_info("change taper mode to normal mode");
		}
		break;
	case FULL_MODE:
		if ((chip->chg_status == POWER_SUPPLY_STATUS_CHARGING) && (chip->batt_soc < 100)) {
			*chg_phase = RECHG_MODE;
			nuvolta_info("change full mode to recharge mode");
		}
		break;
	case RECHG_MODE:
		if (chip->chg_status == POWER_SUPPLY_STATUS_FULL) {
			*chg_phase = FULL_MODE;
			nuvolta_info("change recharge mode to full mode");
		}
		break;
	default:
		break;
	}
	return;
}

static void nuvolta_1665_get_adapter_current(struct nuvolta_1665_chg *chip, u8 adapter)
{
	nuvolta_info("[%s] adapter = 0x%x \n", __func__, adapter);

	switch (adapter) {
	case ADAPTER_QC2:
		chip->target_vol = BPP_QC2_VOUT;
		chip->target_curr = 750;		
	case ADAPTER_QC3:
	case ADAPTER_PD:
		if (chip->epp) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = 850;
		} else if (chip->fc_flag) {
			chip->target_vol = BPP_PLUS_VOUT;
			chip->target_curr = 1100;
		} else {
			chip->target_vol = BPP_DEFAULT_VOUT;
			chip->target_curr = 750;
		}
		break;
	case ADAPTER_AUTH_FAILED:
		if (chip->epp) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = 850;
		} else {
			chip->target_vol = BPP_DEFAULT_VOUT;
			chip->target_curr = 750;
		}
		break;
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_XIAOMI_PD_50W:
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		if (chip->fc_flag) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = 850;	
		} else {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = 850;	
		}
		break;
	default:
		if (chip->epp) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = 850;
		} else {
			chip->target_vol = BPP_DEFAULT_VOUT;
			chip->target_curr = 750;
		}
		break;
	}

	nuvolta_info("[%s]target_vout: %ld, target_icl: %ld", __func__,
		chip->target_vol, chip->target_curr);
	return;
}

static void nuvolta_1665_get_charging_info(struct nuvolta_1665_chg *chip)
{
	int vout, iout, vrect;
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!chip)
		return;

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy)
		nuvolta_err("failed to get batt_psy\n");
	else {
		power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		chip->batt_soc = val.intval;
		power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
		chip->chg_status = val.intval;
		nuvolta_1665_get_charge_phase(chip, &chip->chg_phase);
	}

	ret = nuvolta_1665_get_iout(chip, &iout);
	if (ret < 0 ) {
		nuvolta_err("get iout failed\n");
		iout = 0;
	}
	ret = nuvolta_1665_get_vout(chip, &vout);
	if (ret < 0 ) {
		nuvolta_err("get vout failed\n");
		vout = 0;
	}
	ret = nuvolta_1665_get_vrect(chip, &vrect);
	if (ret < 0 ) {
		nuvolta_err("get vrect failed\n");
		vrect = 0;
	}

	nuvolta_info("%s:Vout:%d, Iout:%d, Vrect:%d, soc: %d, status: %d, chg_phase: %d\n",
			__func__, vout, iout, vrect, chip->batt_soc, chip->chg_status, chip->chg_phase);
}

static void nuvolta_1665_standard_epp_work(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("run standard epp work\n");
	if (chip->chg_phase == FULL_MODE)
		chip->target_curr = 250;
	if (chip->chg_phase == RECHG_MODE)
		chip->target_curr = 550;

	if (chip->target_vol != chip->pre_vol) {
		nuvolta_info("set new vout: %lu, pre vout: %lu\n",
			chip->target_vol, chip->pre_vol);
		nuvolta_1665_set_vout(chip, chip->target_vol);
		chip->pre_vol = chip->target_vol;
	}

	if (chip->target_curr != chip->pre_curr) {
		nuvolta_info("set new icl: %lu, pre icl: %lu\n",
			chip->target_curr, chip->pre_curr);
		nuvolta_1665_set_pmic_icl(chip, chip->target_curr);
		chip->pre_curr = chip->target_curr;
	}

	return;
}

static void nuvolta_1665_bpp_plus_work(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("run bpp plus work\n");

	if (chip->batt_soc >= 95) {
		chip->target_vol = BPP_DEFAULT_VOUT;
		chip->target_curr = 750;
	}
	if (chip->chg_phase == FULL_MODE)
		chip->target_curr = 250;

	if (chip->target_vol != chip->pre_vol) {
		nuvolta_info("set new vout: %lu, pre vout: %lu\n",
			chip->target_vol, chip->pre_vol);
		nuvolta_1665_set_vout(chip, chip->target_vol);
		chip->pre_vol = chip->target_vol;
	}

	if (chip->target_curr != chip->pre_curr) {
		nuvolta_info("set new icl: %lu, pre icl: %lu\n",
			chip->target_curr, chip->pre_curr);
		nuvolta_1665_set_pmic_icl(chip, chip->target_curr);
		chip->pre_curr = chip->target_curr;
	}

	return;
}

static void nuvolta_1665_epp_compatible_work(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("run epp compatible work\n");

	if (chip->chg_phase == FULL_MODE) {
		chip->target_vol = EPP_DEFAULT_VOUT;
		chip->target_curr = 250;
	}
	if (chip->chg_phase == RECHG_MODE) {
		chip->target_vol = EPP_DEFAULT_VOUT;
		chip->target_curr = 550;
	}

	if (chip->target_vol != chip->pre_vol) {
		nuvolta_info("set new vout: %lu, pre vout: %lu\n",
			chip->target_vol, chip->pre_vol);
		nuvolta_1665_set_vout(chip, chip->target_vol);
		chip->pre_vol = chip->target_vol;
	}

	if (chip->target_curr != chip->pre_curr) {
		nuvolta_info("set new icl: %lu, pre icl: %lu\n",
			chip->target_curr, chip->pre_curr);
		nuvolta_1665_set_pmic_icl(chip, chip->target_curr);
		chip->pre_curr = chip->target_curr;
	}

	return;
}

static void nuvolta_1665_epp_plus_work(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("run epp plus work\n");

	if (chip->chg_phase == FULL_MODE) {
		chip->target_vol = EPP_DEFAULT_VOUT;
		chip->target_curr = 250;
	}
	if (chip->chg_phase == RECHG_MODE) {
		chip->target_vol = EPP_DEFAULT_VOUT;
		chip->target_curr = 550;
	}

	if (chip->target_vol != chip->pre_vol) {
		nuvolta_info("set new vout: %lu, pre vout: %lu\n",
			chip->target_vol, chip->pre_vol);
		nuvolta_1665_set_vout(chip, chip->target_vol);
		chip->pre_vol = chip->target_vol;
	}

	if (chip->target_curr != chip->pre_curr) {
		nuvolta_info("set new icl: %lu, pre icl: %lu\n",
			chip->target_curr, chip->pre_curr);
		nuvolta_1665_set_pmic_icl(chip, chip->target_curr);
		chip->pre_curr = chip->target_curr;
	}

	return;
}

static void nuvolta_1665_charging_loop(struct nuvolta_1665_chg *chip)
{
	nuvolta_1665_get_adapter_current(chip, chip->adapter_type);

	if (chip->is_plate_tx) {
		nuvolta_info("run plate tx work\n");
		nuvolta_1665_epp_compatible_work(chip);
		return;
	}

	if (chip->is_train_tx) {
		nuvolta_info("run train tx work\n");
		nuvolta_1665_epp_compatible_work(chip);
		return;
	}

	if (chip->is_music_tx) {
		nuvolta_info("run music tx work\n");
		nuvolta_1665_epp_compatible_work(chip);
		return;
	}

	switch (chip->adapter_type) {
	case ADAPTER_PD:
	case ADAPTER_QC3:
		if (chip->epp)
			nuvolta_1665_standard_epp_work(chip);
		else
			nuvolta_1665_bpp_plus_work(chip);
		break;
	case ADAPTER_AUTH_FAILED:
		nuvolta_1665_standard_epp_work(chip);
		break;
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_50W:
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		nuvolta_1665_epp_plus_work(chip);
		break;
	default:
		break;
	}

	return;
}

static void nuvolta_1665_monitor_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		 container_of(work, struct nuvolta_1665_chg,
					chg_monitor_work.work);
	nuvolta_1665_get_charging_info(chip);

	nuvolta_1665_charging_loop(chip);

	schedule_delayed_work(&chip->chg_monitor_work,
			msecs_to_jiffies(5000));
}

static void nuvolta_1665_factory_reverse_start_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		factory_reverse_start_work.work);

	//TODO:enable reverse charge
	nuvolta_info("just for use chip: %d\n", chip->power_good_flag);
	return;
}

static void nuvolta_1665_factory_reverse_stop_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		factory_reverse_stop_work.work);

	//TODO:disable reverse charge
	nuvolta_info("just for use chip: %d\n", chip->power_good_flag);
	return;
}

static void nuvolta_1665_delay_report_status_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		delay_report_status_work.work);

	//TODO:delay report discharging
	nuvolta_info("just for use chip: %d\n", chip->power_good_flag);
	return;
}

static void nuvolta_1665_check_rx_alarm(struct nuvolta_1665_chg *chip,
	bool *ocp_flag, bool *otp_flag)
{
	int iout = 0, temp = 0;
	int ret = 0;

	ret = nuvolta_1665_get_iout(chip, &iout);
	if (ret < 0)
		*ocp_flag = false;
	else
		*ocp_flag = (iout >= RX_MAX_IOUT);

	ret = nuvolta_1665_get_temp(chip, &temp);
	if (ret < 0)
		*otp_flag = false;
	else
		*otp_flag = (temp >= RX_MAX_TEMP);

	return;
}

static void nuvolta_1665_rx_alarm_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		rx_alarm_work.work);

	bool ocp_flag = false, otp_flag = false;
	int fcc_setted = 0;

	nuvolta_1665_check_rx_alarm(chip, &ocp_flag, &otp_flag);
	if ((!ocp_flag) && (!otp_flag))
		return;

	fcc_setted = nuvolta_1665_get_fcc(chip);

	if (ocp_flag) {
		nuvolta_info("soft ocp, reduce fcc 100mA\n");
		if (fcc_setted - 100 > 0)
			nuvolta_1665_set_pmic_ichg(chip, fcc_setted - 100);
	}

	if (otp_flag) {
		nuvolta_info("soft otp, reduce fcc 500mA\n");
		if (fcc_setted - 500 > 0)
			nuvolta_1665_set_pmic_ichg(chip, fcc_setted - 500);
	}

	schedule_delayed_work(&chip->rx_alarm_work,
				msecs_to_jiffies(4000));
	return;
}

static void nuvolta_1665_rx_enable_usb_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		rx_enable_usb_work.work);

	//TODO:
	nuvolta_info("just for use chip: %d\n", chip->power_good_flag);
	return;
}

static void nuvolta_1665_max_power_control_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		max_power_control_work.work);

	//TODO:
	nuvolta_info("just for use chip: %d\n", chip->power_good_flag);
	return;
}

static void nuvolta_1665_fw_state_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		fw_state_work.work);

	//TODO:
	nuvolta_info("just for use chip: %d\n", chip->power_good_flag);
	return;
}

static void nu1665_hall3_irq_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
	    container_of(work, struct nuvolta_1665_chg,
			 hall3_irq_work.work);

	if (chip->fw_update) {
		nuvolta_info("[hall3] fw updating, don't enable reverse chg\n");
		return;
	}

	if (chip->hall3_online)
		nuvolta_1665_set_reverse_chg_mode(chip, true);
	else if (!chip->hall4_online) {
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_mode = 0;
		chip->is_reverse_chg = 2;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
	} else
		nuvolta_info("[hall3] hall4 online, don't disable reverse charge\n");

	return;
}

static void nu1665_hall4_irq_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
	    container_of(work, struct nuvolta_1665_chg,
			 hall4_irq_work.work);

	if (chip->fw_update) {
		nuvolta_info("[hall4] fw updating, don't enable reverse chg\n");
		return;
	}

	if (chip->hall4_online)
		nuvolta_1665_set_reverse_chg_mode(chip, true);
	else if (!chip->hall3_online) {
		nuvolta_1665_set_reverse_chg_mode(chip, false);
		chip->is_reverse_mode = 0;
		chip->is_reverse_chg = 2;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
	} else
		nuvolta_info("[hall4] hall3 online, don't disable reverse charge\n");

	return;
}

static void nu_reverse_chg_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
	    container_of(work, struct nuvolta_1665_chg,
			 reverse_chg_work.work);
	int temp = 0, rc = 0;

	if (chip->is_reverse_mode || chip->is_boost_mode) {
		rc = tx_info_update(chip, sram_buffer);
		if (rc < 0)
			goto exit;
		nuvolta_1665_get_reverse_vout(chip, &temp);
		nuvolta_1665_get_reverse_iout(chip, &temp);
		nuvolta_1665_get_reverse_temp(chip, &temp);
		nuvolta_1665_get_reverse_soc(chip);
	}
	else {
		nuvolta_info("reverse chg closed, return\n");
		chip->reverse_pen_soc = 255;
		chip->reverse_vout = 0;
		chip->reverse_iout = 0;
		return;
	}
exit:
	if (chip->reverse_pen_soc == 255)
		schedule_delayed_work(&chip->reverse_chg_work, 100);
	else
		schedule_delayed_work(&chip->reverse_chg_work, 10 * HZ);

	return;
}

static void nu1665_probe_fw_download_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
	    container_of(work, struct nuvolta_1665_chg,
			 probe_fw_download_work.work);
	bool crc_ok = false;
	int rc = 0;
	bool fw_ok = false;
	u8 fw_version = 0;

	nuvolta_info("[nuvo] enter %s\n", __func__);

	if (chip->fw_update) {
		nuvolta_info("[nu1665] [%s] FW Update is on going!\n",
			 __func__);
		return;
	}

	pm_stay_awake(chip->dev);
	chip->fw_update = true;
	rc = nuvolta_1665_set_reverse_gpio(chip, true);
	msleep(100);

	rc = fw_crc_chk(chip);
	if (rc < 0) {
		nuvolta_err("update crc verify failed.\n");
		crc_ok = false;
	} else {
		nuvolta_info("update crc verify success.\n");
		crc_ok = true;
	}

	rc = read_fw_version(chip, &fw_version);
	nuvolta_info("%s, FW Version:0x%02x\n", __func__, fw_version);
	chip->fw_version = fw_version;

	if (rc < 0)
		chip->chip_ok = 0;
	else
		chip->chip_ok = 1;

	rc = nuvolta_1665_set_reverse_gpio(chip, false);
	msleep(1000);

	if (fw_version >= FW_VERSION)
		fw_ok = true;
	if (crc_ok && fw_ok)
		nuvolta_info("Don't need update FW,so skip.\n");
	else {
		rc = nuvolta_1665_set_reverse_gpio(chip, true);
		msleep(100);

		nuvolta_info("%s: FW download start\n", __func__);
		rc = nuvolta_1665_download_fw(chip, false, true);
		if (rc < 0) {
			nuvolta_err("[%s] fw download failed!\n", __func__);
		} else {
			nuvolta_info("%s: FW download end\n", __func__);
		}

		rc = nuvolta_1665_set_reverse_gpio(chip, false);
		msleep(1000);

		// start crc verify
		rc = nuvolta_1665_set_reverse_gpio(chip, true);
		msleep(100);

		if (fw_crc_chk(chip) < 0)
			nuvolta_err("[%s] fw crc failed!\n", __func__);

		rc = nuvolta_1665_set_reverse_gpio(chip, false);
	}

	chip->fw_update = false;
	pm_relax(chip->dev);
	if (chip->hall3_online)
		schedule_delayed_work(&chip->hall3_irq_work, msecs_to_jiffies(2000));
	else if (chip->hall4_online)
		schedule_delayed_work(&chip->hall4_irq_work, msecs_to_jiffies(2000));
	else
		return;

}


static irqreturn_t nuvolta_1665_power_good_handler(int irq, void *dev_id)
{
	struct nuvolta_1665_chg *chip = dev_id;

	if (chip->fw_update)
		return IRQ_HANDLED;
	schedule_delayed_work(&chip->wireless_pg_det_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static irqreturn_t nuvolta_1665_hall3_irq_handler(int irq, void *dev_id)
{
    struct nuvolta_1665_chg *chip = dev_id;

	if (gpio_is_valid(chip->hall3_gpio)) {
		if (gpio_get_value(chip->hall3_gpio)) {
			nuvolta_err("hall3_irq_handler: pen detach\n");
			chip->hall3_online = 0;
			pen_charge_state_notifier_call_chain(0, NULL);
			if (chip->hall4_online) {
				nuvolta_err("hall3_irq_handler: hall4 online, return\n");
				pen_charge_state_notifier_call_chain(1, NULL);
				return IRQ_HANDLED;
			}
			schedule_delayed_work(&chip->hall3_irq_work, msecs_to_jiffies(0));
			return IRQ_HANDLED;
		} else {
			nuvolta_err("hall3_irq_handler: pen attach\n");
			chip->hall3_online = 1;
			pen_charge_state_notifier_call_chain(1, NULL);
		}
	}

	if (chip->hall4_online) {
		nuvolta_err("[hall3] reverse charging already running, return\n");
		return IRQ_HANDLED;
	}
	else
		schedule_delayed_work(&chip->hall3_irq_work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static irqreturn_t nuvolta_1665_hall4_irq_handler(int irq, void *dev_id)
{
    struct nuvolta_1665_chg *chip = dev_id;

	if (gpio_is_valid(chip->hall4_gpio)) {
		if (gpio_get_value(chip->hall4_gpio)) {
			nuvolta_err("hall4_irq_handler: pen detach\n");
			chip->hall4_online = 0;
			pen_charge_state_notifier_call_chain(0, NULL);
			if (chip->hall3_online) {
				nuvolta_err("hall4_irq_handler: hall3 online, return\n");
				pen_charge_state_notifier_call_chain(1, NULL);
				return IRQ_HANDLED;
			}
			schedule_delayed_work(&chip->hall4_irq_work, msecs_to_jiffies(0));
			return IRQ_HANDLED;
		} else {
			nuvolta_err("hall4_irq_handler: pen attach\n");
			chip->hall4_online = 1;
			pen_charge_state_notifier_call_chain(1, NULL);
		}
	}

	if (chip->hall3_online) {
		nuvolta_err("[hall4] reverse charging already running, return\n");
		return IRQ_HANDLED;
	}
	else
		schedule_delayed_work(&chip->hall4_irq_work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int nuvolta_1665_parse_dt(struct nuvolta_1665_chg *chip)
{
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		nuvolta_err("%s:No DT data Failing Probe\n", __func__);
		return -EINVAL;
	}

	chip->tx_on_gpio = of_get_named_gpio(node, "reverse_chg_ovp_gpio", 0);
	nuvolta_err("[%s] print tx_on gpio %d\n",
						__func__, chip->tx_on_gpio);
	if (!gpio_is_valid(chip->tx_on_gpio)) {
		nuvolta_err("[%s] fail_tx_on gpio %d\n",
						 __func__, chip->tx_on_gpio);
		return -EINVAL;
	}

/*	chip->enable_gpio = of_get_named_gpio(node, "rx_sleep_gpio", 0);
	if ((!gpio_is_valid(chip->enable_gpio)))
		return -EINVAL;
*/
	chip->irq_gpio = of_get_named_gpio(node, "rx_irq_gpio", 0);
	nuvolta_err("[%s] print irq_gpio %d\n",
						__func__, chip->irq_gpio);
	if (!gpio_is_valid(chip->irq_gpio)) {
		nuvolta_err("[%s] fail_irq_gpio %d\n",
						 __func__, chip->irq_gpio);
		return -EINVAL;
	}

/*	chip->power_good_gpio = of_get_named_gpio(node, "pwr_det_gpio", 0);
	if (!gpio_is_valid(chip->power_good_gpio)) {
		nuvolta_err("[%s] fail_power_good_gpio %d\n",
						 __func__, chip->power_good_gpio);
		return -EINVAL;
	}
*/
	chip->reverse_boost_gpio = of_get_named_gpio(node, "reverse_boost_gpio", 0);
	nuvolta_err("[%s] print reverse_boost_gpio %d\n",
						__func__, chip->reverse_boost_gpio);
	if (!gpio_is_valid(chip->reverse_boost_gpio)) {
		nuvolta_err("[%s] fail reverse_boost_gpio %d\n",
						 __func__, chip->reverse_boost_gpio);
		return -EINVAL;
	}

	chip->hall3_gpio = of_get_named_gpio(node, "hall,int3", 0);
	nuvolta_err("[%s] print chip->hall3_gpio %d\n",
						__func__, chip->hall3_gpio);
	if ((!gpio_is_valid(chip->hall3_gpio))) {
		nuvolta_err("[%s] chip->hall3_gpio %d\n",
						 __func__, chip->hall3_gpio);
		return -EINVAL;
	}

	chip->hall4_gpio = of_get_named_gpio(node, "hall,int4", 0);
	nuvolta_err("[%s] print chip->hall4_gpio %d\n",
						__func__, chip->hall4_gpio);
	if ((!gpio_is_valid(chip->hall4_gpio))) {
		nuvolta_err("[%s] chip->hall4_gpio %d\n",
						 __func__, chip->hall4_gpio);
		return -EINVAL;
	}

	return 0;
}

static int nuvolta_rx1665_gpio_init(struct nuvolta_1665_chg *chip)
{
	int ret = 0;
    int irqn = 0;

	chip->idt_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->idt_pinctrl)) {
		nuvolta_err("No pinctrl config specified\n");
		ret = PTR_ERR(chip->dev);
		return ret;
	}
	chip->idt_gpio_active =
	    pinctrl_lookup_state(chip->idt_pinctrl, "idt_active");
	if (IS_ERR_OR_NULL(chip->idt_gpio_active)) {
		nuvolta_err("No active config specified\n");
		ret = PTR_ERR(chip->idt_gpio_active);
		return ret;
	}
	chip->idt_gpio_suspend =
	    pinctrl_lookup_state(chip->idt_pinctrl, "idt_suspend");
	if (IS_ERR_OR_NULL(chip->idt_gpio_suspend)) {
		nuvolta_err("No suspend config specified\n");
		ret = PTR_ERR(chip->idt_gpio_suspend);
		return ret;
	}

	ret = pinctrl_select_state(chip->idt_pinctrl, chip->idt_gpio_active);
	if (ret < 0) {
		nuvolta_err("fail to select pinctrl active rc=%d\n", ret);
		return ret;
	}

	if (gpio_is_valid(chip->irq_gpio)) {
		irqn = gpio_to_irq(chip->irq_gpio);
		if (irqn < 0) {
			nuvolta_err("[%s] gpio_to_irq Fail!, irq_gpio:%d \n", __func__, chip->irq_gpio);
            ret = -1;
			goto fail_irq_gpio;
		}
		chip->irq = irqn;
	} else {
		nuvolta_err("%s: irq gpio not provided\n", __func__);
        ret = -1;
		goto fail_irq_gpio;
	}

/*	if (gpio_is_valid(chip->power_good_gpio)) {
		irqn = gpio_to_irq(chip->power_good_gpio);
		if (irqn < 0) {
			nuvolta_err("[%s] gpio_to_irq Fail! \n", __func__);
            ret = -1;
			goto fail_power_good_gpio;
		}
		chip->power_good_irq = irqn;
	} else {
		nuvolta_err("%s: power good gpio not provided\n", __func__);
        ret = -1;
		goto fail_power_good_gpio;
	}
*/

	if (gpio_is_valid(chip->hall3_gpio)) {
		irqn = gpio_to_irq(chip->hall3_gpio);
		if (irqn < 0) {
			ret = irqn;
			nuvolta_err("%s:hall3 irq gpio failed\n", __func__);
			goto fail_irq_gpio;
		}
		chip->hall3_irq = irqn;
	} else {
		nuvolta_err("%s:hall3 irq gpio not provided\n", __func__);
		goto err_hall3_irq_gpio;
	}

	if (gpio_is_valid(chip->hall4_gpio)) {
		irqn = gpio_to_irq(chip->hall4_gpio);
		if (irqn < 0) {
			ret = irqn;
			nuvolta_err("%s:hall4 irq gpio failed\n", __func__);
			goto fail_irq_gpio;
		}
		chip->hall4_irq = irqn;
	} else {
		nuvolta_err("%s:hall4 irq gpio not provided\n", __func__);
		goto err_hall4_irq_gpio;
	}

	return ret;

fail_irq_gpio:
	gpio_free(chip->irq_gpio);
//fail_power_good_gpio:
//	gpio_free(chip->power_good_gpio);
err_hall4_irq_gpio:
	gpio_free(chip->hall4_gpio);
err_hall3_irq_gpio:
	gpio_free(chip->hall3_gpio);
	return ret;
}


static int nuvolta_rx1665_irq_request(struct nuvolta_1665_chg *chip)
{
    int ret;

// config irq 
    if (!chip->irq) {
        nuvolta_err("irq is wrong = %s \n", __func__);
        return -EINVAL;
    }

    ret = request_irq(chip->irq,
            nuvolta_1665_interrupt_handler,
            (IRQF_TRIGGER_FALLING | IRQF_ONESHOT),
            "nuvolta_1665_chg_stat_irq", chip);
    if (ret) {
        nuvolta_err("Failed irq = %d ret = %d\n", chip->irq, ret);
        return ret;
    }
	ret = enable_irq_wake(chip->irq);
    if (ret) {
        nuvolta_err("%s: enable request irq is failed\n", __func__);
        return ret;
    }

// config hall3 irq
    if (!chip->hall3_irq) {
        nuvolta_err("hall3 irq is wrong = %s \n", __func__);
        return -EINVAL;
    }

    ret = request_irq(chip->hall3_irq,
            nuvolta_1665_hall3_irq_handler,
            ( IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING),
            "hall3_irq", chip);
    if (ret) {
        nuvolta_err("Failed hall3-irq = %d ret = %d\n", chip->hall3_irq, ret);
        return ret;
    }
	
	enable_irq_wake(chip->hall3_irq);
    if (ret) {
        nuvolta_err("%s: enable request hall3 irq is failed\n", __func__);
        return ret;
    }
// config hall4 irq
    if (!chip->hall4_irq) {
        nuvolta_err("hall4 irq is wrong = %s \n", __func__);
        return -EINVAL;
    }

    ret = request_irq(chip->hall4_irq,
            nuvolta_1665_hall4_irq_handler,
            ( IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING),
            "hall4_irq", chip);
    if (ret) {
        nuvolta_err("Failed hall4-irq = %d ret = %d\n", chip->hall4_irq, ret);
        return ret;
    }
	
	enable_irq_wake(chip->hall4_irq);
    if (ret) {
        nuvolta_err("%s: enable request hall4 irq is failed\n", __func__);
        return ret;
    }

    return 0;
// config power good irq
    if (!chip->power_good_irq) {
        nuvolta_err("power good irq is wrong = %s \n", __func__);
        return -EINVAL;
    }

    ret = devm_request_threaded_irq(&chip->client->dev, chip->power_good_irq, NULL,
            nuvolta_1665_power_good_handler,
            (IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
            "nuvolta_1665_power_good_irq", chip);
    if (ret) {
        nuvolta_err("Failed irq = %d ret = %d\n", chip->power_good_irq, ret);
        return ret;
    }

	enable_irq_wake(chip->power_good_irq);
    if (ret) {
        nuvolta_err("%s: enable request power good irq is failed\n", __func__);
        return ret;
    }
	return -EINVAL;
}

static ssize_t chip_vrect_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int vrect = 0, ret = 0;

	ret = nuvolta_1665_get_vrect(g_chip, &vrect);
	if (ret < 0 ) {
		nuvolta_err("get vrect failed\n");
		vrect = 0;
	}

	return scnprintf(buf, PAGE_SIZE, "rx1665 Vrect : %d mV\n", vrect);
}

static ssize_t chip_iout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int iout = 0, ret = 0;

	ret = nuvolta_1665_get_iout(g_chip, &iout);
	if (ret < 0 ) {
		nuvolta_err("get iout failed\n");
		iout = 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", iout);
}

static ssize_t chip_vout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int vout = 0, ret = 0;

	ret = nuvolta_1665_get_vout(g_chip, &vout);
	if (ret < 0 ) {
		nuvolta_err("get vout failed\n");
		vout = 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", vout);
}

static ssize_t chip_vout_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;

	index = (int)simple_strtoul(buf, NULL, 10);
	nuvolta_info("[rx1665] [%s] --Store output_voltage = %d\n",
							__func__, index);
	if ((index < 4000) || (index > 21000)) {
		nuvolta_err("[rx1665] [%s] Store Voltage %s is invalid\n",
							__func__, buf);
		nuvolta_1665_set_vout(g_chip, 0);
		return count;
	}

	nuvolta_1665_set_vout(g_chip, index);

	return count;
}

static int nuvolta_1665_check_i2c(struct nuvolta_1665_chg *chip)
{
	int ret = 0;
	u8 data = 0;

	ret = rx1665_write(chip, 0x88, 0x0000);
	if (ret < 0)
		return ret;
	msleep(10);

	ret = rx1665_read(chip, &data, 0x0000);
	if (ret < 0)
		return ret;

	if (data == 0x88) {
		nuvolta_info("[%s] i2c check ok!\n", __func__);
		return 1;
	} else {
		nuvolta_info("[%s] i2c check failed!\n", __func__);
		return -1;		
	}

	return ret;
}

/*static int nuvolta_1665_enter_dtm_mode(struct nuvolta_1665_chg *chip)
{
	int ret = 0;
	u8 data;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0x41, 0x0090);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x00, 0x2017);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x2d, 0x2017);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0xd2, 0x2017);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x22, 0x2017);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0xdd, 0x2017);
	if (ret < 0)
		return ret;

	ret = rx1665_read(chip, &data, 0x2017);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x00, 0x2020);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x4b, 0x2020);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0xb4, 0x2020);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x44, 0x2020);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0xbb, 0x2020);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x41, 0x0090);
	if (ret < 0)
		return ret;

	nuvolta_info("[%s] reg:0x2717 = 0x%x\n", __func__, data);

	return ret;
}

static int nuvolta_1665_exit_dtm_mode(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0x00, 0x2020);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x00, 0x2017);
	if (ret < 0)
		return ret;

	return ret;
}

static int nuvolta_1665_disable_mcu(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0xc0, 0x1000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x3f, 0x1153);
	if (ret < 0)
		return ret;

	return ret;
}

static int nuvolta_1665_mux_burn_free(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0x80, 0x1001);
	if (ret < 0)
		return ret;

	return ret;
}

static int nuvolta_1665_select_all_sector(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0xff, 0x0012);
	if (ret < 0)
		return ret;

	return ret;
}

static int nuvolta_1665_enter_write_mode(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0x5a, 0x001a);
	if (ret < 0)
		return ret;

	return ret;
}

static int nuvolta_1665_exit_write_mode(struct nuvolta_1665_chg *chip)
{
	int ret = 0;

	nuvolta_info("[%s] \n", __func__);

	ret = rx1665_write(chip, 0x00, 0x001a);
	if (ret < 0)
		return ret;

	return ret;
}*/

/*static int nuvolta_1665_set_confirm_data(struct nuvolta_1665_chg *chip, u8 confirm_data)
{
	int ret = 0;
	u8 data = 0;

	ret = nuvolta_1665_check_i2c(chip);
	if (ret < 0)
		return ret;

	ret = nuvolta_1665_enter_dtm_mode(chip);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0xe0, 0x1000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x41, 0x0090);
	if (ret < 0)
		return ret;

	msleep(10);

	ret = rx1665_write(chip, confirm_data, 0x008b);
	if (ret < 0)
		return ret;

	msleep(10);

	ret = rx1665_read(chip, &data, 0x008b);
	if (ret < 0)
		return ret;

	if (data != confirm_data) {
		nuvolta_err("[%s]set failed, read data: %d\n", __func__, data);
		return -1;
	}

	nuvolta_info("[%s] set confirm data success: %d\n", __func__, confirm_data);

	ret = rx1665_write(chip, 0x3c, 0x001a);
	if (ret < 0)
		return ret;

	return ret;
}
*/

/*static int nuvolta_1665_get_confirm_data(struct nuvolta_1665_chg *chip, u8 *confirm_data)
{
	int ret = 0;
	u8 data = 0;

	ret = nuvolta_1665_check_i2c(chip);
	if (ret < 0)
		return ret;

	ret = nuvolta_1665_enter_dtm_mode(chip);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0xe0, 0x1000);
	if (ret < 0)
		return ret;

	ret = rx1665_write(chip, 0x41, 0x0090);
	if (ret < 0)
		return ret;

	msleep(10);

	ret = rx1665_read(chip, &data, 0x008b);
	if (ret < 0)
		return ret;

	*confirm_data = data;
	nuvolta_info("[%s] get confirm data: 0x%x\n", __func__, data);

	ret = rx1665_write(chip, 0x3c, 0x001a);
	if (ret < 0)
		return ret;

	return ret;
}*/
#if 0
static u8 nuvolta_1665_get_boot_fw_version()
{
	return ((~fw_data_1665[1*1024-9]) & 0xFF);
}

static u8 nuvolta_1665_get_rx_fw_version()
{
	return ((~fw_data_1665[23*1024-9]) & 0xFF);
}

static u8 nuvolta_1665_get_tx_fw_version()
{
	return ((~fw_data_1665[32*1024-9]) & 0xFF);
}


static u8 nuvolta_1665_get_fw_version(struct nuvolta_1665_chg *chip)
{
	int ret = 0;
	u8 status = 0;
	u8 read_buf[7] = {0};
	u8 fw_boot_check = 0, fw_rx_check = 0, fw_tx_check = 0;

	ret = rx1665_write(chip, 0x02, 0x0063);
	if (ret < 0)
		return status;

	msleep(100);

	ret = rx1665_read_buffer(chip, read_buf, 0x0028, 7);
	if (ret < 0)
		return status;

	fw_boot_check = read_buf[0];
	fw_rx_check = read_buf[1];
	fw_tx_check = read_buf[2];

	nuvolta_info("[%s]boot_check: 0x%x, rx_check: 0x%x, tx_check: 0x%x\n",
		__func__, fw_boot_check, fw_rx_check, fw_tx_check);

	if (fw_boot_check == 0x66)
		g_wls_fw_data.fw_boot_id = read_buf[4];
	else
		g_wls_fw_data.fw_boot_id = 0xFE;

	if (fw_rx_check == 0x66)
		g_wls_fw_data.fw_rx_id = read_buf[5];
	else
		g_wls_fw_data.fw_rx_id = 0xFE;

	if (fw_tx_check == 0x66)
		g_wls_fw_data.fw_tx_id = read_buf[6];
	else
		g_wls_fw_data.fw_tx_id = 0xFE;

	if ((g_wls_fw_data.fw_boot_id != 0xFE)
		&& (g_wls_fw_data.fw_boot_id >= nuvolta_1665_get_boot_fw_version()))
		status |= BOOT_CHECK_SUCCESS;

	if ((g_wls_fw_data.fw_rx_id != 0xFE)
		&& (g_wls_fw_data.fw_rx_id >= nuvolta_1665_get_rx_fw_version()))
		status |= RX_CHECK_SUCCESS;

	if ((g_wls_fw_data.fw_tx_id != 0xFE)
		&& (g_wls_fw_data.fw_tx_id >= nuvolta_1665_get_tx_fw_version()))
		status |= TX_CHECK_SUCCESS;

	nuvolta_info("fw_data_1665 version: boot = 0x%x, rx = 0x%x, tx = 0x%x\n",
		nuvolta_1665_get_boot_fw_version(), nuvolta_1665_get_rx_fw_version(),
		nuvolta_1665_get_tx_fw_version());

	nuvolta_info("ic fw version: boot = 0x%x, rx = 0x%x, tx = 0x%x\n",
		g_wls_fw_data.fw_boot_id, g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);

	return status;
}
#endif
static int nuvolta_1665_download_fw_data(struct nuvolta_1665_chg *chip,
	unsigned char *fw_data, int fw_data_length)
{
	int ret = 0;
	u8 read_data = 0;//, wrfail = 0, busy = 0;
	int i = 0, j = 0;
	u8 __1st_word = 1;

	nuvolta_info("[%s] start\n", __func__);

	//ret = nuvolta_1665_enter_dtm_mode(chip);
	if (rx1665_write(chip, 0x41, 0x0090) < 0)
		goto exit;
	if (rx1665_write(chip, 0xC0, 0x1000) < 0)
		goto exit;
	if (rx1665_write(chip, 0xFF, 0x0012) < 0)
		goto exit;
	if (ret < 0) {
		nuvolta_err("[%s] failed to enter dtm mode\n", __func__);
		return ret;
	}

/*	ret = nuvolta_1665_disable_mcu(chip);
	if (ret < 0) {
		nuvolta_err("[%s] failed to disable_mcu\n", __func__);
		goto exit;
	}

	ret = nuvolta_1665_mux_burn_free(chip);
	if (ret < 0) {
		nuvolta_err("[%s] failed to mux_burn_free\n", __func__);
		goto exit;
	}

	ret = nuvolta_1665_select_all_sector(chip);
	if (ret < 0) {
		nuvolta_err("[%s] failed to select_all_sector\n", __func__);
		goto exit;
	}

	ret = nuvolta_1665_enter_write_mode(chip);
	if (ret < 0) {
		nuvolta_err("[%s] failed to enter_write_mode\n", __func__);
		goto exit;
	}

	msleep(20);*/

	for (i = 0; i < fw_data_length; i += 4) {
		if (__1st_word) {
			__1st_word = 0;

			if (rx1665_write(chip, 0x01, 0x0017) < 0)
				goto exit;
			
			if (rx1665_write(chip, fw_data[i+3], 0x001C) < 0)
				goto exit;			
			if (rx1665_write(chip, fw_data[i+2], 0x001D) < 0)
				goto exit;			
			if (rx1665_write(chip, fw_data[i+1], 0x001E) < 0)
				goto exit;	
			if (rx1665_write(chip, fw_data[i+0], 0x001F) < 0)
				goto exit;
			
			if (rx1665_write(chip, 0x01, 0x0019) < 0)
				goto exit;
			if (rx1665_write(chip, 0x00, 0x0019) < 0)
				goto exit;
			
			if (rx1665_write(chip, 0x5A, 0x001A) < 0)
				goto exit;
			msleep(20);
		}

		if (rx1665_write(chip, fw_data[i+3], 0x001C) < 0)
			goto exit;			
		if (rx1665_write(chip, fw_data[i+2], 0x001D) < 0)
			goto exit;
		if (rx1665_write(chip, fw_data[i+1], 0x001E) < 0)
			goto exit;
		if (rx1665_write(chip, fw_data[i+0], 0x001F) < 0)
			goto exit;

		for (j = 0; j < 250; j++) {
			if (rx1665_read(chip, &read_data, 0x001b) < 0)
				goto exit;
			if (!(read_data & (1 << 7)))
				break;
			if (read_data & (1 << 6)) {
				nuvolta_err("[%s] write failed \n", __func__);
				goto exit;
			}
			msleep(1);
		}
		
		if (j == 250) {
			nuvolta_err("[%s] write timeout \n", __func__);
			goto exit;
		}
	}
	if (rx1665_write(chip, 0x00, 0x001A) < 0)
		goto exit;

/*	ret = nuvolta_1665_exit_write_mode(chip);
	if (ret < 0)
		goto exit;

	ret = nuvolta_1665_exit_dtm_mode(chip);
	if (ret < 0)
		goto exit;*/

	return ret;

exit:
	msleep(100);
	nuvolta_err("[%s] wrfail, MTP error\n", __func__);

/*	ret = nuvolta_1665_exit_write_mode(chip);
	if (ret < 0)
		return ret;

	ret = nuvolta_1665_exit_dtm_mode(chip);
	if (ret < 0)
		return ret;*/

	return -1;
}

static int key_open(struct nuvolta_1665_chg *chip)
{
	if (rx1665_write(chip, 0x00, 0x2017) < 0)
		goto exit;
	if (rx1665_write(chip, 0x2D, 0x2017) < 0)
		goto exit;
	if (rx1665_write(chip, 0xD2, 0x2017) < 0)
		goto exit;
	if (rx1665_write(chip, 0x22, 0x2017) < 0)
		goto exit;
	if (rx1665_write(chip, 0xDD, 0x2017) < 0)
		goto exit;
	return 0;
exit:
	nuvolta_err("[%s] failed \n", __func__);
	return -1;
}

static int write_key0(struct nuvolta_1665_chg *chip)
{
	if (rx1665_write(chip, 0x00, 0x2018) < 0) {
		nuvolta_err("[%s] failed \n", __func__);
		return -1;
	}
	return 0;
}

static int write_key1(struct nuvolta_1665_chg *chip)
{
	if (rx1665_write(chip, 0x00, 0x2019) < 0) {
		nuvolta_err("[%s] failed \n", __func__);
		return -1;
	}
	return 0;
}

static int exit_key0(struct nuvolta_1665_chg *chip)
{
	if (rx1665_write(chip, 0xFF, 0x2018) < 0) {
		nuvolta_err("[%s] failed \n", __func__);
		return -1;
	}
	return 0;
}

static int exit_key1(struct nuvolta_1665_chg *chip)
{
	if (rx1665_write(chip, 0xFF, 0x2019) < 0) {
		nuvolta_err("[%s] failed \n", __func__);
		return -1;
	}
	return 0;
}

static int exit_key_open(struct nuvolta_1665_chg *chip)
{
	if (rx1665_write(chip, 0x00, 0x2017) < 0) {
		nuvolta_err("[%s] failed \n", __func__);
		return -1;
	}
	return 0;
}

static int nuvolta_1665_download_fw(struct nuvolta_1665_chg *chip, bool power_on, bool force)
{
	int ret = 0;
	unsigned char *fw_data = NULL;
	int fw_data_length = 0;
	//u8 check_result = 0, confirm_data = 0;

	if (power_on) {
		nuvolta_info("[%s]auto update when power on\n", __func__);
		//TODO add judgement for auto fw download
	}

	fw_data = fw_data_1665;
	fw_data_length = sizeof(fw_data_1665);
	nuvolta_info("[%s] fw data length: %ld\n", __func__, fw_data_length);

/*	check_result = nuvolta_1665_get_fw_version(chip);
	if ((check_result == (RX_CHECK_SUCCESS | TX_CHECK_SUCCESS | BOOT_CHECK_SUCCESS)) && (!force)) {
		nuvolta_info("[%s] no need update, check result:%d\n", __func__, check_result);
		return ret;
	}*/

	//start down firmware
/*	ret = nuvolta_1665_get_confirm_data(chip, &confirm_data);
	if (ret < 0)
		return ret;

	ret = nuvolta_1665_set_confirm_data(chip, 0x00);
	if (ret < 0)
		return ret;*/

//  new patch
	if (key_open(chip) < 0)
		return -1;
	
	if (write_key0(chip) < 0)
		return -1;
	
	if (write_key1(chip) < 0)
		return -1;



	ret = nuvolta_1665_download_fw_data(chip, fw_data, fw_data_length);
	if (ret < 0) {
		nuvolta_err("[] nuvolta_1665_download_fw_data failed \n");
		return -1;
	}

	if (exit_key0(chip) < 0)
		return -1;
	
	if (exit_key1(chip) < 0)
		return -1;
	
	if (exit_key_open(chip) < 0)
		return -1;
	
	return 0;

	//return ret;
}

static int fw_crc_chk(struct nuvolta_1665_chg *chip)
{
	u8 read_data = 0;
	
	if (rx1665_write(chip, 0x02, 0x0063) < 0)
		goto exit;
	msleep(100);
	if (rx1665_read(chip, &read_data, 0x0028) < 0)
		goto exit;
	if (read_data == 0x66) {
		nuvolta_info("fw crc chk good \n");
		return 0;
	}
	nuvolta_info("fw crc chk res %x \n", read_data);
exit:
	nuvolta_err("[%s] failed \n", __func__);
	return -1;
}

static int read_fw_version(struct nuvolta_1665_chg *chip, u8 *version)
{
	if (!version)
		return -1;
	
	*version = 0xFF;
	
	if (rx1665_read(chip, version, 0x002C) < 0) {
		nuvolta_err("[%s] failed \n", __func__);
		return -1;
	}
	nuvolta_info("fw chk version %x \n", *version);
	return 0;
}

static int nuvolta_1665_firmware_update_func(struct nuvolta_1665_chg *chip, u8 cmd)
{
	int ret = 0;
	u8 fw_version = 0;

	//TODO1 disable reverse charge if it run
	chip->fw_update = true;
	//TODO2 sleep rx before start download and resume after
	nuvolta_1665_set_reverse_gpio(chip, true);
	//nuvolta_1665_set_reverse_pmic_boost(chip, true);
	msleep(100);

	ret = nuvolta_1665_check_i2c(chip);
	if (ret < 0)
		goto exit;

	switch (cmd) {
	case FW_UPDATE_CHECK:
		ret = nuvolta_1665_download_fw(chip, false, false);
		if (ret < 0) {
			nuvolta_err("[%s] fw download failed! cmd: %d\n", __func__, cmd);
			goto exit;
		}
		break;
	case FW_UPDATE_FORCE:
		ret = nuvolta_1665_download_fw(chip, false, true);
		if (ret < 0) {
			nuvolta_err("[%s] fw download failed! cmd: %d\n", __func__, cmd);
			goto exit;
		}
		break;
	case FW_UPDATE_FROM_BIN:
		//TODO add fw download from bin
		break;
	case FW_UPDATE_ERASE:
		//TODO add erase func
		break;
	case FW_UPDATE_AUTO:
		ret = nuvolta_1665_download_fw(chip, true, false);
		if (ret < 0) {
			nuvolta_err("[%s] fw download failed! cmd: %d\n", __func__, cmd);
			goto exit;
		}
		break;
	default:
		nuvolta_err("[%s] unknown cmd: %d\n", __func__, cmd);
		break;
	}

	nuvolta_1665_set_reverse_gpio(chip, false);
	//nuvolta_1665_set_reverse_pmic_boost(chip, false);
	msleep(1000);
	nuvolta_1665_set_reverse_gpio(chip, true);

	msleep(100);

	if (fw_crc_chk(chip) < 0)
		ret = -1;
	else {
		if (read_fw_version(chip, &fw_version) < 0)
			ret = -1;
		else
			chip->fw_version = fw_version;
	}
	nuvolta_info("check fw version %x \n", chip->fw_version);
exit:
	chip->fw_update = false;
	nuvolta_1665_set_reverse_gpio(chip, false);
	return ret;
}

static ssize_t chip_firmware_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int cmd = 0, ret = 0;

	if (g_chip->fw_update){
		nuvolta_info("[%s] Firmware Update is on going!\n", __func__);
		return count;
	}

	cmd = (int)simple_strtoul(buf, NULL, 10);
	nuvolta_info("[%s] value %d\n", __func__, cmd);

	if ((cmd > FW_UPDATE_NONE) && (cmd < FW_UPDATE_MAX)) {
		ret = nuvolta_1665_firmware_update_func(g_chip, cmd);
		if (ret < 0) {
			nuvolta_err("[%s] Firmware Update:failed!\n", __func__);
			return count;
		} else {
			nuvolta_info("[%s] Firmware Update:Success!\n", __func__);
			return count;
		}
	} else {
		nuvolta_err("[%s] Firmware Update:invalid cmd\n", __func__);
	}

	return count;
}

static ssize_t chip_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int check_result = 0;
	u8 default_FW_Ver = 0xFE;

	if (g_chip->fw_update) {
		nuvolta_info("[%s] fw update going, can not show version\n", __func__);
		return scnprintf(buf, PAGE_SIZE, "updating\n");
	} else {
		nuvolta_1665_set_reverse_gpio(g_chip, true);
		//nuvolta_1665_set_reverse_pmic_boost(g_chip, true);
		msleep(100);

		//check_result = nuvolta_1665_get_fw_version(g_chip);
		check_result = fw_crc_chk(g_chip);
		if (check_result >= 0) {
			read_fw_version(g_chip, &default_FW_Ver);
		}
		nuvolta_1665_set_reverse_gpio(g_chip, false);
		//nuvolta_1665_set_reverse_pmic_boost(g_chip, false);

/*		return scnprintf(buf, PAGE_SIZE, "fw_ver:%02x.%02x.%02x.%x%x\n",
				g_wls_fw_data.fw_boot_id, g_wls_fw_data.fw_tx_id, g_wls_fw_data.fw_rx_id,
				g_wls_fw_data.hw_id_h, g_wls_fw_data.hw_id_l); nuvolta_1665_get_tx_fw_version()
*/
		return scnprintf(buf, PAGE_SIZE, "fw_ver:%02x\n", default_FW_Ver);
	}
}

static ssize_t chip_fw_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (g_chip->fw_update) {
		nuvolta_info("[%s] Firmware Update is on going!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "Firmware Update is on going!\n");
	}

	nuvolta_info("[%s] Start fireware update process\n", __func__);
	ret = nuvolta_1665_firmware_update_func(g_chip, 2);
	if (ret < 0) {
		nuvolta_err("[%s] Firmware Update:failed!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "Firmware Update:Failed\n");
	} else {
		nuvolta_info("[%s] Firmware Update:Success!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "Firmware Update:Success\n");
	}

}

static DEVICE_ATTR(chip_vrect, S_IRUGO, chip_vrect_show, NULL);
static DEVICE_ATTR(chip_firmware_update, S_IWUSR, NULL, chip_firmware_update_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IRUGO, chip_iout_show, NULL);
static DEVICE_ATTR(chip_fw, S_IWUSR | S_IRUGO, chip_fw_show, NULL);

static struct attribute *rx1665_sysfs_attrs[] = {
	&dev_attr_chip_vrect.attr,
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_chip_iout.attr,
	&dev_attr_chip_firmware_update.attr,
	&dev_attr_chip_fw.attr,
	NULL,
};

static const struct attribute_group rx1665_sysfs_group_attrs = {
	.attrs = rx1665_sysfs_attrs,
};

/*
static int wls_is_wireless_present(struct nuvolta_1665_chg *chip, bool *present)
{
	if (chip->power_good_flag)
		*present = true;
	else
		*present = false;
	return 0;
}

static int wls_is_qc_enable(struct nuvolta_1665_chg *chip, bool *enable)
{
	if (chip->qc_enable)
		*enable = true;
	else
		*enable = false;
	return 0;
}

static int wls_is_firmware_update(struct nuvolta_1665_chg *chip, bool *update)
{
	if (chip->fw_update)
		*update = true;
	else
		*update = false;
	return 0;
}

static int wls_is_car_adapter(struct nuvolta_1665_chg *chip, bool *enable)
{
	if (chip->is_car_tx)
		*enable = true;
	else
		*enable = false;
	return 0;
}

static int wls_get_reverse_charge(struct nuvolta_1665_chg *chip, bool *enable)
{
	if (chip->reverse_chg_en)
		*enable = true;
	else
		*enable = false;
	return 0;
}

static int wls_enable_reverse_chg(struct nuvolta_1665_chg *chip, bool enable)
{
	int ret = 0;
	int en = !!enable;

	if (chip->fw_update) {
		nuvolta_info("fw update going, break\n");
		chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
		return 0;
	}
	chip->is_reverse_chg = REVERSE_STATE_OPEN;
	if (!chip->power_good_flag) {
	} else
		chip->is_reverse_chg = REVERSE_STATE_FORWARD;
	return ret;
}

static int wls_set_vout(struct nuvolta_1665_chg *chip, int vout)
{
	int ret = 0;

	if (chip->power_good_flag)
		ret = nuvolta_1665_set_vout(chip, vout);
	else
		nuvolta_info("power good off, can't set vout\n");
	nuvolta_info("wls_set_vout: %d\n", vout);

	return ret;
}

static int wls_get_vout(struct nuvolta_1665_chg *chip, int *vout)
{
	int ret = 0;

	if (chip->power_good_flag) {
		ret = nuvolta_1665_get_vout(chip, vout);
		if (ret < 0)
			*vout = 0;
	} else
		*vout = 0;

	nuvolta_info("wls_get_vout: %d\n", *vout);
	return 0;
}

static int wls_get_iout(struct nuvolta_1665_chg *chip, int *iout)
{
	int ret = 0;

	if (chip->power_good_flag) {
		ret = nuvolta_1665_get_iout(chip, iout);
		if (ret < 0)
			*iout = 0;
	} else
		*iout = 0;

	nuvolta_info("wls_get_iout: %d\n", *iout);
	return 0;
}

static int wls_get_vrect(struct nuvolta_1665_chg *chip, int *vrect)
{
	int ret = 0;

	if (chip->power_good_flag) {
		ret = nuvolta_1665_get_vrect(chip, vrect);
		if (ret < 0)
			*vrect = 0;
	} else
		*vrect = 0;

	nuvolta_info("wls_get_vrect: %d\n", *vrect);
	return 0;
}

static int wls_get_tx_adapter(struct nuvolta_1665_chg *chip, int *adapter)
{
	if (chip->power_good_flag)
		*adapter = chip->adapter_type;
	else
		*adapter = 0;
	nuvolta_info("wls_get_adapter: %d\n", chip->adapter_type);

	return 0;
}

static int wls_get_reverse_chg_state(struct nuvolta_1665_chg *chip, int *state)
{
	*state = chip->is_reverse_chg;

	nuvolta_info("wls_get_reverse_chg_state: %d\n", chip->is_reverse_chg);

	return 0;
}
*/

static int nuvolta_1665_set_enable_mode(struct nuvolta_1665_chg *chip, bool enable)
{
	int ret = 0;
	int gpio_enable_val = 0;
	int en = !!enable;

	if (gpio_is_valid(chip->enable_gpio)) {
		ret = gpio_request(chip->enable_gpio,
				"rx-enable-gpio");
		if (ret) {
			nuvolta_err("%s: unable to request enable gpio [%d]\n",
					__func__, chip->enable_gpio);
		}

		ret = gpio_direction_output(chip->enable_gpio, !en);
		if (ret) {
			nuvolta_err("%s: cannot set direction for idt enable gpio [%d]\n",
					__func__, chip->enable_gpio);
		}
		gpio_enable_val = gpio_get_value(chip->enable_gpio);
		nuvolta_info("nuvolta enable gpio val is :%d\n", gpio_enable_val);
		gpio_free(chip->enable_gpio);
	}

	return ret;
}
static enum power_supply_property nu1665_props[] = {
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_WIRELESS_VERSION,
	POWER_SUPPLY_PROP_WIRELESS_FW_VERSION,
	POWER_SUPPLY_PROP_SIGNAL_STRENGTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT,
	POWER_SUPPLY_PROP_RX_IOUT,
	POWER_SUPPLY_PROP_WIRELESS_TX_ID,
	POWER_SUPPLY_PROP_TX_ADAPTER,
	POWER_SUPPLY_PROP_REVERSE_CHG_MODE,
	POWER_SUPPLY_PROP_DIV_2_MODE,
	POWER_SUPPLY_PROP_OTG_STATE,
	POWER_SUPPLY_PROP_REVERSE_CHG_HALL3,
	POWER_SUPPLY_PROP_REVERSE_CHG_HALL4,
	POWER_SUPPLY_PROP_REVERSE_PEN_SOC,
	POWER_SUPPLY_PROP_REVERSE_VOUT,
	POWER_SUPPLY_PROP_REVERSE_IOUT,
};

static int nu1665_get_prop(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	struct nuvolta_1665_chg *chip = power_supply_get_drvdata(psy);
	int temp = 0;
	int rc = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_WIRELESS_FW_VERSION:
		val->intval = chip->fw_version;
		break;
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		val->intval = chip->ss;
		break;
/*	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		if (!chip->power_good_flag) {
			val->intval = 0;
			break;
		}
		rc = nuvolta_1665_get_vout(chip, &val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT:
		if (!chip->power_good_flag) {
			val->intval = 0;
			break;
		}
		rc = nuvolta_1665_get_vrect(chip, &val->intval);
		break;
	case POWER_SUPPLY_PROP_RX_IOUT:
		if (!chip->power_good_flag) {
			val->intval = 0;
			break;
		}
		rc = nuvolta_1665_get_iout(chip, &val->intval);
		break;*/
	case POWER_SUPPLY_PROP_WIRELESS_TX_ID:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TX_ADAPTER:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		val->intval = gpio_get_value(chip->tx_on_gpio);
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_HALL3:
		if (gpio_is_valid(chip->hall3_gpio)) {
			val->intval = gpio_get_value(chip->hall3_gpio);
			//nuvolta_info("hall3 gpio:%d\n", val->intval);
		} else {
			val->intval = -1;
			//nuvolta_err("hall3 gpio is not provided\n");
		}
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_HALL4:
		if (gpio_is_valid(chip->hall4_gpio)) {
			val->intval = gpio_get_value(chip->hall4_gpio);
			//nuvolta_info("hall4 gpio:%d\n", val->intval);
		} else {
			val->intval = -1;
			//nuvolta_info("hall4 gpio is not provided\n");
		}
		break;
	case POWER_SUPPLY_PROP_REVERSE_PEN_SOC:
		if (chip->is_reverse_mode || chip->is_boost_mode) {
			if (chip->reverse_pen_soc != 255) {
				last_valid_pen_soc = chip->reverse_pen_soc;
				val->intval = chip->reverse_pen_soc;
			} else {
				val->intval = last_valid_pen_soc;
				nuvolta_err("print last valid pen soc: %d\n", val->intval);
			}
		} else {
			val->intval = last_valid_pen_soc;
			if (val->intval)
				nuvolta_err("report last valid pen soc: %d\n", val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_REVERSE_VOUT:
		if (chip->is_reverse_mode || chip->is_boost_mode)
			val->intval = chip->reverse_vout;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_REVERSE_IOUT:
		if (chip->is_reverse_mode || chip->is_boost_mode)
			val->intval = chip->reverse_iout;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_OTG_STATE:
		if (!chip->power_good_flag) {
			val->intval = 0;
			break;
		}
		rc = nuvolta_1665_get_temp(chip, &temp);
		if (rc > 0)
			val->intval = temp;
		else
			val->intval = 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static int nu1665_set_prop(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct nuvolta_1665_chg *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		chip->ss = val->intval;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		if (chip->fw_update) {
			nuvolta_info("fw update going, break\n");
			break;
		}

		if (chip->hall3_online || chip->hall4_online) {
			if (chip->is_reverse_mode || chip->is_boost_mode) {
				nuvolta_info("reverse charge running, return\n");
				break;
			}
		} else {
			nuvolta_info("pen detach, don't open reverse charge\n");
			break;
		}

		chip->is_reverse_chg = 0;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
		if (!chip->power_good_flag) {
			nuvolta_1665_set_reverse_chg_mode(chip, val->intval);
		} else {
			chip->is_reverse_chg = 3;
			schedule_delayed_work(&chip->reverse_sent_state_work, 0);
		}
		break;
	case POWER_SUPPLY_PROP_OTG_STATE:
		//rx_set_otg_state(di, val->intval);
		nuvolta_1665_set_reverse_chg_mode(chip, val->intval);
		break;
	/*case POWER_SUPPLY_PROP_DIV_2_MODE:
		break;*/
	default:
		return -EINVAL;
	}

	return 0;
}

static int nu1665_prop_is_writeable(struct power_supply *psy,
				      enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_PIN_ENABLED:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
	case POWER_SUPPLY_PROP_OTG_STATE:
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		return 1;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc nuvo_psy_desc ={
	.name = "fuda",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = nu1665_props,
	.num_properties = ARRAY_SIZE(nu1665_props),
	.get_property = nu1665_get_prop,
	.set_property = nu1665_set_prop,
	.property_is_writeable = nu1665_prop_is_writeable,
};

/*
static const struct wireless_charger_properties nuvolta_1665_chg_props = {
	.alias_name = "nuvolta_wireless_chg",
};

static const struct wireless_charger_ops nuvolta_1665_chg_ops = {
	.wls_enable_reverse_chg = wls_enable_reverse_chg,
	.wls_is_wireless_present = wls_is_wireless_present,
	.wls_is_qc_enable = wls_is_qc_enable,
	.wls_is_firmware_update = wls_is_firmware_update,
	.wls_set_vout = wls_set_vout,
	.wls_get_vout = wls_get_vout,
	.wls_get_iout = wls_get_iout,
	.wls_get_vrect = wls_get_vrect,
	.wls_get_tx_adapter = wls_get_tx_adapter,
	.wls_get_reverse_chg_state = wls_get_reverse_chg_state,
	.wls_enable_chg = nuvolta_1665_set_enable_mode,
	.wls_is_car_adapter = wls_is_car_adapter,
	.wls_get_reverse_chg = wls_get_reverse_charge,
};

static int nuvolta_1665_chg_init_chgdev(struct nuvolta_1665_chg *chip)
{
	nuvolta_info("enter %s\n", __func__);
	chip->wlschgdev = wireless_charger_device_register(chip->wlsdev_name, chip->dev,
						chip, &nuvolta_1665_chg_ops,
						&nuvolta_1665_chg_props);
	return IS_ERR(chip->wlschgdev) ? PTR_ERR(chip->wlschgdev) : 0;
}
*/

/*static bool nuvolta_1665_check_votable(struct nuvolta_1665_chg *chip)
{
	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");

	if (!chip->fcc_votable) {
		nuvolta_err("failed to get fcc_votable\n");
		return false;
	}

	if (!chip->icl_votable)
		chip->icl_votable = find_votable("USB_ICL");

	if (!chip->icl_votable) {
		nuvolta_err("failed to get icl_votable\n");
		return false;
	}
	return true;
}*/

static void nuvolta_1665_init_detect_work(struct work_struct *work)
{
	struct nuvolta_1665_chg *chip =
		container_of(work, struct nuvolta_1665_chg,
		init_detect_work.work);
	int ret = 0;

	if (gpio_is_valid(chip->power_good_gpio)) {
		ret = gpio_get_value(chip->power_good_gpio);
		nuvolta_info("[%s]init power good: %d\n", __func__, ret);
		if (ret) {
			nuvolta_1665_set_enable_mode(chip, false);
			usleep_range(20000, 25000);
			nuvolta_1665_set_enable_mode(chip, true);
		}
	}

	//nuvolta_1665_set_reverse_chg_mode(chip, true);
	return;
}

extern char *saved_command_line;

static int get_cmdline(struct nuvolta_1665_chg *chip)
{
	if (strnstr(saved_command_line, "androidboot.mode=",
		    strlen(saved_command_line))) {

		chip->power_off_mode = 1;
		nuvolta_info("enter power off charging app\n");
	} else {
		chip->power_off_mode = 0;
		nuvolta_info(" enter normal boot mode\n");
	}
	return 1;
}

static int nuvolta_1665_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	int hall3_val = 1, hall4_val = 1;
	struct nuvolta_1665_chg *chip;

	struct power_supply_config nuvo_cfg = { };


	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		nuvolta_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	chip->regmap = devm_regmap_init_i2c(client, &nuvolta_1665_regmap_config);
	if (IS_ERR(chip->regmap)) {
		nuvolta_err("failed to allocate register map\n");
		return PTR_ERR(chip->regmap);
	}

	chip->client = client;
	chip->dev = &client->dev;
	chip->fw_update = false;
	chip->fw_version = 0;
	chip->ss = 2;
	chip->wlsdev_name = NUVOLTA_1665_DRIVER_NAME;
	chip->chg_phase = NORMAL_MODE;
	g_chip = chip;
	chip->reverse_pen_soc = 255;

	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, chip);

	mutex_init(&chip->wireless_chg_int_lock);
	mutex_init(&chip->reverse_op_lock);


	INIT_DELAYED_WORK(&chip->wireless_int_work, nuvolta_1665_wireless_int_work);
	INIT_DELAYED_WORK(&chip->wireless_pg_det_work, nuvolta_1665_pg_det_work);
	INIT_DELAYED_WORK(&chip->chg_monitor_work, nuvolta_1665_monitor_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_state_work, reverse_chg_state_set_work);
	INIT_DELAYED_WORK(&chip->reverse_dping_state_work, reverse_dping_state_set_work);
	INIT_DELAYED_WORK(&chip->init_detect_work, nuvolta_1665_init_detect_work);
	INIT_DELAYED_WORK(&chip->factory_reverse_start_work, nuvolta_1665_factory_reverse_start_work);
	INIT_DELAYED_WORK(&chip->factory_reverse_stop_work, nuvolta_1665_factory_reverse_stop_work);
	INIT_DELAYED_WORK(&chip->delay_report_status_work, nuvolta_1665_delay_report_status_work);
	INIT_DELAYED_WORK(&chip->rx_alarm_work, nuvolta_1665_rx_alarm_work);
	INIT_DELAYED_WORK(&chip->rx_enable_usb_work, nuvolta_1665_rx_enable_usb_work);
	INIT_DELAYED_WORK(&chip->max_power_control_work, nuvolta_1665_max_power_control_work);
	INIT_DELAYED_WORK(&chip->fw_state_work, nuvolta_1665_fw_state_work);
    INIT_DELAYED_WORK(&chip->hall3_irq_work, nu1665_hall3_irq_work);
	INIT_DELAYED_WORK(&chip->hall4_irq_work, nu1665_hall4_irq_work);
	INIT_DELAYED_WORK(&chip->pen_notifier_work, pen_charge_notifier_work);
	pen_notifier_work = &chip->pen_notifier_work;
	INIT_DELAYED_WORK(&chip->reverse_chg_work, nu_reverse_chg_work);
	INIT_DELAYED_WORK(&chip->probe_fw_download_work, nu1665_probe_fw_download_work);

	ret = nuvolta_1665_parse_dt(chip);
    if (ret < 0) {
        nuvolta_err("device tree init is failed = %s\n", __func__);
        goto error_sysfs;
    }

	ret = nuvolta_rx1665_gpio_init(chip);
    if (ret < 0) {
        nuvolta_err("gpio init is failed = %s\n", __func__);
        goto error_sysfs;
    }

    ret = nuvolta_rx1665_irq_request(chip);
    if (ret < 0) {
        nuvolta_err("irq init is failed = %s\n", __func__);
        goto error_sysfs;
    }

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_dping_alarm,
			ALARM_BOOTTIME, reverse_dping_alarm_cb);
	} else {
		nuvolta_err("Failed to initialize reverse dping alarm\n");
		return -ENODEV;
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_chg_alarm,
			ALARM_BOOTTIME, reverse_chg_alarm_cb);
	} else {
		nuvolta_err("Failed to initialize reverse chg alarm\n");
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&chip->reverse_sent_state_work,
			  reverse_chg_sent_state_work);
	/* register charger device for wireless 
	ret = nuvolta_1665_chg_init_chgdev(chip);
	if (ret < 0) {
		nuvolta_err("failed to register wireless chgdev %d\n", ret);
		return -ENODEV;
	}*/

	// get cp master charger device
	//if (!chip->cp_master_dev)
		//chip->cp_master_dev = get_charger_by_name("cp_master");

	/* check vote for icl and ichg
	temp = nuvolta_1665_check_votable(chip);
	if (!temp)
		nuvolta_err("failed to check vote %d\n", temp);*/

	ret = sysfs_create_group(&chip->dev->kobj, &rx1665_sysfs_group_attrs);
	if (ret < 0)
	{
		nuvolta_err("sysfs_create_group fail %d\n", ret);
		goto error_sysfs;
	}
	nuvo_cfg.drv_data = chip;
	chip->nuvo_psy = power_supply_register(chip->dev,
					     &nuvo_psy_desc, &nuvo_cfg);
	/* pmic boost  */
/*	chip->pmic_boost = devm_regulator_get(chip->dev, "pmic_vbus");
	if (IS_ERR(chip->pmic_boost)) {
		nuvolta_err("failed to get pmic vbus\n");
		goto error_sysfs;
	}
*/
	/* get master cp dev  */
	/*if (!chip->master_cp_dev)
		chip->master_cp_dev = get_charger_by_name("cp_master");

	if (!chip->master_cp_dev) {
		nuvolta_err("failed to get master_cp_dev\n");
		goto error_sysfs;
	}*/

	/* reset wls charge when power good online */
	schedule_delayed_work(&chip->init_detect_work, msecs_to_jiffies(20000));

	if (gpio_is_valid(chip->hall3_gpio)) {
		hall3_val = gpio_get_value(chip->hall3_gpio);
		if (!hall3_val) {
			nuvolta_info("pen online, start reverse charge\n");
			chip->hall3_online = 1;
			pen_charge_state_notifier_call_chain_booting(1, NULL);
			schedule_delayed_work(&chip->hall3_irq_work, msecs_to_jiffies(6000));
		}
	} else
		nuvolta_err("%s: hall3 gpio not provided\n", __func__);

	if (gpio_is_valid(chip->hall4_gpio)) {
		hall4_val = gpio_get_value(chip->hall4_gpio);
		if (!hall4_val) {
			nuvolta_info("pen online, start reverse charge\n");
			chip->hall4_online = 1;
			pen_charge_state_notifier_call_chain_booting(1, NULL);
			schedule_delayed_work(&chip->hall4_irq_work, msecs_to_jiffies(6000));
		}
	} else
		nuvolta_err("%s: hall4 gpio not provided\n", __func__);


	get_cmdline(chip);
#ifndef CONFIG_FACTORY_BUILD
	if (!chip->power_off_mode)
		schedule_delayed_work(&chip->probe_fw_download_work, 10 * HZ);
#endif
/**/
	return 0;

error_sysfs:
	sysfs_remove_group(&chip->dev->kobj, &rx1665_sysfs_group_attrs);
	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);
	if (chip->power_good_gpio > 0)
		gpio_free(chip->power_good_gpio);
	cancel_delayed_work_sync(&chip->hall3_irq_work);
	cancel_delayed_work_sync(&chip->hall4_irq_work);
	return 0;
}

static int nuvolta_1665_remove(struct i2c_client *client)
{
	struct nuvolta_1665_chg *chip = i2c_get_clientdata(client);

	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);
	if (chip->power_good_gpio > 0)
		gpio_free(chip->power_good_gpio);

	return 0;
}

static int nuvolta_1665_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nuvolta_1665_chg *chip = i2c_get_clientdata(client);

	return enable_irq_wake(chip->irq);
}

static int nuvolta_1665_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nuvolta_1665_chg *chip = i2c_get_clientdata(client);

	return disable_irq_wake(chip->irq);
}

static const struct dev_pm_ops nuvolta_1665_pm_ops = {
	.suspend	= nuvolta_1665_suspend,
	.resume		= nuvolta_1665_resume,
};

static void nuvolta_1665_shutdown(struct i2c_client *client)
{
	struct nuvolta_1665_chg *chip = i2c_get_clientdata(client);

	if (chip->power_good_flag) {
		nuvolta_1665_set_enable_mode(chip, false);
		usleep_range(20000, 25000);
		nuvolta_1665_set_enable_mode(chip, true);
	}

	nuvolta_info("%s: shutdown: %s\n", __func__, chip->wlsdev_name);
	return;
}

static const struct of_device_id nuvolta_1665_match_table[] = {
	{ .compatible = "nuvolta,rx1665",},
	{ },
};

static const struct i2c_device_id nuvolta_1665_id[] = {
	{"nuvolta_1665", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, nuvolta_1665_id);

static struct i2c_driver nuvolta_1665_driver = {
	.driver		= {
		.name		= "nuvolta_1665",
		.owner		= THIS_MODULE,
		.of_match_table	= nuvolta_1665_match_table,
		.pm		= &nuvolta_1665_pm_ops,
	},
	.probe		= nuvolta_1665_probe,
	.remove		= nuvolta_1665_remove,
	.id_table	= nuvolta_1665_id,
	.shutdown	= nuvolta_1665_shutdown,
};

module_i2c_driver(nuvolta_1665_driver);

MODULE_AUTHOR("anxufeng <anxufeng@xiaomi.com>");
MODULE_DESCRIPTION("nuvolta wireless charge driver");
MODULE_LICENSE("GPL");
