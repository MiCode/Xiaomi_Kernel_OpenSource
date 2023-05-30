/*
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* This driver is compatible for BQ27Z561/NFG1000A/NFG1000B/BQ28Z610 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>

#include "xmc_core.h"

enum product_name {
	PISSARRO,
	PISSARROPRO,
};

enum fg_reg_idx {
	BQ_FG_REG_CTRL = 0,
	BQ_FG_REG_TEMP,		/* Battery Temperature */
	BQ_FG_REG_VOLT,		/* Battery Voltage */
	BQ_FG_REG_CN,		/* Current Now */
	BQ_FG_REG_AI,		/* Average Current */
	BQ_FG_REG_BATT_STATUS,	/* BatteryStatus */
	BQ_FG_REG_TTE,		/* Time to Empty */
	BQ_FG_REG_TTF,		/* Time to Full */
	BQ_FG_REG_FCC,		/* Full Charge Capacity */
	BQ_FG_REG_RM,		/* Remaining Capacity */
	BQ_FG_REG_CC,		/* Cycle Count */
	BQ_FG_REG_SOC,		/* Relative State of Charge */
	BQ_FG_REG_SOH,		/* State of Health */
	BQ_FG_REG_CHG_VOL,	/* Charging Voltage*/
	BQ_FG_REG_CHG_CUR,	/* Charging Current*/
	BQ_FG_REG_DC,		/* Design Capacity */
	BQ_FG_REG_ALT_MAC,	/* AltManufactureAccess*/
	BQ_FG_REG_MAC_DATA,	/* MACData*/
	BQ_FG_REG_MAC_CHKSUM,	/* MACChecksum */
	BQ_FG_REG_MAC_DATA_LEN,	/* MACDataLen */
	NUM_REGS,
};

static u8 fg_regs[NUM_REGS] = {
	0x00,	/* CONTROL */
	0x06,	/* TEMP */
	0x08,	/* VOLT */
	0x0C,	/* CURRENT NOW */
	0x14,	/* AVG CURRENT */
	0x0A,	/* FLAGS */
	0x16,	/* Time to empty */
	0x18,	/* Time to full */
	0x12,	/* Full charge capacity */
	0x10,	/* Remaining Capacity */
	0x2A,	/* CycleCount */
	0x2C,	/* State of Charge */
	0x2E,	/* State of Health */
	0x30,	/* Charging Voltage*/
	0x32,	/* Charging Current*/
	0x3C,	/* Design Capacity */
	0x3E,	/* AltManufacturerAccess*/
	0x40,	/* MACData*/
	0x60,	/* MACChecksum */
	0x61,	/* MACDataLen */
};

enum fg_mac_cmd {
	FG_MAC_CMD_CTRL_STATUS		= 0x0000,
	FG_MAC_CMD_DEV_TYPE		= 0x0001,
	FG_MAC_CMD_FW_VER		= 0x0002,
	FG_MAC_CMD_HW_VER		= 0x0003,
	FG_MAC_CMD_IF_SIG		= 0x0004,
	FG_MAC_CMD_CHEM_ID		= 0x0006,
	FG_MAC_CMD_SHUTDOWN		= 0x0010,
	FG_MAC_CMD_GAUGING		= 0x0021,
	FG_MAC_CMD_SEAL			= 0x0030,
	FG_MAC_CMD_FASTCHARGE_EN	= 0x003E,
	FG_MAC_CMD_FASTCHARGE_DIS	= 0x003F,
	FG_MAC_CMD_DEV_RESET		= 0x0041,
	FG_MAC_CMD_DEVICE_NAME		= 0x004A,
	FG_MAC_CMD_DEVICE_CHEM		= 0x004B,
	FG_MAC_CMD_MANU_NAME		= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS	= 0x0055,
	FG_MAC_CMD_LIFETIME1		= 0x0060,
	FG_MAC_CMD_LIFETIME3		= 0x0062,
	FG_MAC_CMD_DASTATUS1		= 0x0071,
	FG_MAC_CMD_ITSTATUS1		= 0x0073,
	FG_MAC_CMD_QMAX			= 0x0075,
	FG_MAC_CMD_FCC_SOH		= 0x0077,
	FG_MAC_CMD_RA_TABLE		= 0x40C0,
};

static int product_name = PISSARRO;
static struct charge_chip *g_chip = NULL;

static int __fg_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	s32 ret = 0;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		xmc_info("i2c write byte fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
		return ret;
	}

	return 0;
}

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret = 0;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		xmc_info("i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u16)ret;

	return 0;
}

static int __fg_read_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{
	int ret = 0, i = 0;

	for(i = 0; i < len; i++) {
		ret = i2c_smbus_read_byte_data(client, reg + i);
		if (ret < 0) {
			xmc_info("i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
		buf[i] = ret;
	}

	return ret;
}

static int __fg_write_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{
	int ret = 0, i = 0;

	for(i = 0; i < len; i++) {
		ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
		if (ret < 0) {
			xmc_info("i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return ret;
}

static int fg_write_byte(struct fg_chip *chip, u8 reg, u8 val)
{
	int ret;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __fg_write_byte(chip->client, reg, val);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int fg_read_word(struct fg_chip *chip, u8 reg, u16 *val)
{
	int ret;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __fg_read_word(chip->client, reg, val);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int fg_read_block(struct fg_chip *chip, u8 reg, u8 *buf, u8 len)
{
	int ret;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __fg_read_block(chip->client, reg, buf, len);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int fg_write_block(struct fg_chip *chip, u8 reg, u8 *data, u8 len)
{
	int ret;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __fg_write_block(chip->client, reg, data, len);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static u8 fg_checksum(u8 *data, u8 len)
{
	u8 i;
	u16 sum = 0;

	for (i = 0; i < len; i++) {
		sum += data[i];
	}

	sum &= 0xFF;

	return 0xFF - sum;
}

static int fg_mac_read_block(struct fg_chip *chip, u16 cmd, u8 *buf, u8 len)
{
	int ret;
	u8 cksum_calc, cksum;
	u8 t_buf[40];
	u8 t_len;
	int i;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	ret = fg_write_block(chip, chip->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;

	msleep(4);

	ret = fg_read_block(chip, chip->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0)
		return ret;

	cksum = t_buf[34];
	t_len = t_buf[35];

	cksum_calc = fg_checksum(t_buf, t_len - 2);
	if (cksum_calc != cksum) {
		xmc_err("%s failed to checksum\n", chip->log_tag);
		return 1;
	}

	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];

	return 0;
}

static int fg_sha256_auth(struct fg_chip *chip, u8 *challenge, int length)
{
	int ret = 0;
	u8 cksum_calc = 0, data[2] = {0};

	data[0] = 0x00;
	data[1] = 0x00;
	ret = fg_write_block(chip, chip->regs[BQ_FG_REG_ALT_MAC], data, 2);
	if (ret < 0)
		return ret;

	msleep(2);

	ret = fg_write_block(chip, chip->regs[BQ_FG_REG_MAC_DATA], challenge, length);
	if (ret < 0)
		return ret;

	cksum_calc = fg_checksum(challenge, length);
	ret = fg_write_byte(chip, chip->regs[BQ_FG_REG_MAC_CHKSUM], cksum_calc);
	if (ret < 0)
		return ret;

	ret = fg_write_byte(chip, chip->regs[BQ_FG_REG_MAC_DATA_LEN], length + 4);
	if (ret < 0)
		return ret;

	msleep(300);

	ret = fg_read_block(chip, chip->regs[BQ_FG_REG_MAC_DATA], chip->digest, length);
	if (ret < 0)
		return ret;

	return 0;
}

static int fg_read_rsoc(struct fg_chip *chip)
{
	u16 soc = 0;
	static u16 s_soc = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_soc ? s_soc : 50;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read RSOC\n", chip->log_tag);
			soc = (s_soc ? s_soc : 50);
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_soc = soc;
	return soc;
}

static int fg_read_temperature(struct fg_chip *chip)
{
	u16 tbat = 0;
	static u16 s_tbat = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_tbat - 2730;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_TEMP], &tbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read TBAT\n", chip->log_tag);
			tbat = 2980;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_tbat = tbat;
	return tbat - 2730;
}

static int fg_read_volt(struct fg_chip *chip)
{
	u16 vbat = 0;
	static u16 s_vbat = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_vbat ? s_vbat : 3800;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_VOLT], &vbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read VBAT\n", chip->log_tag);
			vbat = (product_name == PISSARRO) ? 4000 : 8000;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_vbat = vbat;

	return vbat;
}

static int fg_read_current(struct fg_chip *chip)
{
	s16 ibat = 0;
	static s16 s_ibat = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_ibat;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_CN], (u16 *)&ibat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read IBAT\n", chip->log_tag);
			ibat = 0;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_ibat = ibat = -1 * ibat;

	return ibat;
}

static int fg_read_fcc(struct fg_chip *chip)
{
	u16 fcc = 0;
	static u16 s_fcc = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_fcc;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_FCC], &fcc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read FCC\n", chip->log_tag);
			fcc = (product_name == PISSARRO) ? 5160 : 4500;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_fcc = fcc;
	return fcc;
}

static int fg_read_rm(struct fg_chip *chip)
{
	u16 rm = 0;
	static u16 s_rm = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_rm;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_RM], &rm);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read RM\n", chip->log_tag);
			rm = (product_name == PISSARRO) ? 2580 : 2250;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_rm = rm;
	return rm;
}

static int fg_read_dc(struct fg_chip *chip)
{
	u16 dc = 0;
	static u16 s_dc = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_dc;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_DC], &dc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read DC\n", chip->log_tag);
			dc = (product_name == PISSARRO) ? 5160 : 4500;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_dc = dc;
	return dc;
}

/*
static int fg_read_soh(struct fg_chip *chip)
{
	u16 soh = 0;
	static u16 s_soh = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_soh;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read SOH\n", chip->log_tag);
			soh = 50;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_soh = soh;
	return soh;
}
*/

static int fg_read_cyclecount(struct fg_chip *chip)
{
	u16 cc = 0;
	static u16 s_cc = 0;
	bool retry = false;
	int ret = 0;

	if (chip->rw_lock)
		return s_cc;

retry:
	ret = fg_read_word(chip, chip->regs[BQ_FG_REG_CC], &cc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			xmc_err("%s failed to read CC\n", chip->log_tag);
			cc = 0;
			if (chip->i2c_error_count < 10)
				chip->i2c_error_count++;
		}
	} else {
		if (chip->i2c_error_count > 0)
			chip->i2c_error_count--;
	}

	s_cc = cc;
	return cc;
}

static void fg_clear_rw_lock(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip, clear_rw_lock_work.work);

	chip->rw_lock = false;
	__pm_relax(chip->gauge_wakelock);
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int fg_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct fg_chip *chip = &g_chip->battery;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->chip_ok;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = fg_read_cyclecount(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = fg_read_volt(chip) * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = fg_read_current(chip) * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = fg_read_dc(chip) * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = fg_read_fcc(chip) * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fg_read_rm(chip) * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = fg_read_rsoc(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = fg_read_temperature(chip);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->desc->type;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->log_tag;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct fg_chip *chip = &g_chip->battery;

	switch (prop) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		chip->authenticate = !!val->intval;
		chip->rw_lock = false;
		xmc_info("%s authenticate = %d\n", chip->log_tag, chip->authenticate);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static const struct power_supply_desc fg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = fg_props,
	.num_properties = ARRAY_SIZE(fg_props),
	.get_property = fg_get_property,
	.set_property = fg_set_property,
	.property_is_writeable = fg_prop_is_writeable,
};

static int fg_register_psy(struct fg_chip *chip)
{
	struct power_supply_config fg_psy_cfg = {};

	fg_psy_cfg.drv_data = g_chip;
	fg_psy_cfg.of_node = g_chip->dev->of_node;

	g_chip->bms_psy = devm_power_supply_register(g_chip->dev, &fg_psy_desc, &fg_psy_cfg);
	if (IS_ERR(g_chip->bms_psy)) {
		xmc_err("%s failed to register bms_psy", chip->log_tag);
		return PTR_ERR(g_chip->bms_psy);
	}

	return 0;
}

int fg_stringtohex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);

	while (cnt < (tmplen / 2)) {
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p++;
		cnt++;
	}

	if (tmplen % 2 != 0)
		out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if (outlen != NULL)
		*outlen = tmplen / 2 + tmplen % 2;

	return tmplen / 2 + tmplen % 2;
}

static ssize_t fg_verify_digest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fg_chip *chip = &g_chip->battery;
	u8 digest_buf[4] = {0};
	int len = 0, i = 0;

	if (chip->device_name == FG_BQ27Z561) {
		for (i = 0; i < RANDOM_CHALLENGE_LEN_BQ27Z561; i++) {
			memset(digest_buf, 0, sizeof(digest_buf));
			snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", chip->digest[i]);
			strlcat(buf, digest_buf, RANDOM_CHALLENGE_LEN_BQ27Z561 * 2 + 1);
		}
	} else if (chip->device_name == FG_BQ28Z610) {
		for (i = 0; i < RANDOM_CHALLENGE_LEN_BQ28Z610; i++) {
			memset(digest_buf, 0, sizeof(digest_buf));
			snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", chip->digest[i]);
			strlcat(buf, digest_buf, RANDOM_CHALLENGE_LEN_BQ28Z610 * 2 + 1);
		}
	} else {
		xmc_err("%s not support device name\n", chip->log_tag);
	}

	len = strlen(buf);
	buf[len] = '\0';

	return strlen(buf) + 1;
}

ssize_t fg_verify_digest_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fg_chip *chip = &g_chip->battery;
	int i = 0;
	u8 random[RANDOM_CHALLENGE_LEN_MAX] = {0};
	char kbuf[70] = {0};
	ktime_t ktime_now;
	struct timespec64 time_now;
	static bool once_flag = false;

	if (!once_flag) {
		once_flag = true;
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		if (time_now.tv_sec <= 40 && !chip->gauge_wakelock->active) {
			xmc_info("%s fg verifing, set a flag to block iic rw\n", chip->log_tag);
			chip->rw_lock = true;
			__pm_stay_awake(chip->gauge_wakelock);
			schedule_delayed_work(&chip->clear_rw_lock_work, msecs_to_jiffies((45 - time_now.tv_sec) * 1000));
		}
	}

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);
	fg_stringtohex(kbuf, random, &i);
	if (chip->device_name == FG_BQ27Z561)
		fg_sha256_auth(chip, random, RANDOM_CHALLENGE_LEN_BQ27Z561);
	else if (chip->device_name == FG_BQ28Z610)
		fg_sha256_auth(chip, random, RANDOM_CHALLENGE_LEN_BQ28Z610);

	return count;
}

static DEVICE_ATTR(verify_digest, S_IRUGO | S_IWUSR, fg_verify_digest_show, fg_verify_digest_store);

static struct attribute *fg_attributes[] = {
	&dev_attr_verify_digest.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};

static int fg_parse_dt(struct fg_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	int ret = 0;

	chip->enable_shutdown_delay = of_property_read_bool(node, "enable_shutdown_delay");

	if (product_name == PISSARRO) {
		ret = of_property_read_u32(node, "normal_shutdown_vbat_1s", &chip->normal_shutdown_vbat);
		if (ret)
			xmc_err("%s failed to parse normal_shutdown_vbat_1s\n", chip->log_tag);

		ret = of_property_read_u32(node, "critical_shutdown_vbat_1s", &chip->critical_shutdown_vbat);
		if (ret)
			xmc_err("%s failed to parse critical_shutdown_vbat_1s\n", chip->log_tag);

		ret = of_property_read_u32(node, "report_full_rsoc_1s", &chip->report_full_rsoc);
		if (ret)
			xmc_err("%s failed to parse report_full_rsoc_1s\n", chip->log_tag);

		ret = of_property_read_u32(node, "soc_gap_1s", &chip->soc_gap);
		if (ret)
			xmc_err("%s failed to parse soc_gap_1s\n", chip->log_tag);
	} else {
		ret = of_property_read_u32(node, "normal_shutdown_vbat_2s", &chip->normal_shutdown_vbat);
		if (ret)
			xmc_err("%s failed to parse normal_shutdown_vbat_2s\n", chip->log_tag);

		ret = of_property_read_u32(node, "critical_shutdown_vbat_2s", &chip->critical_shutdown_vbat);
		if (ret)
			xmc_err("%s failed to parse critical_shutdown_vbat_2s\n", chip->log_tag);

		ret = of_property_read_u32(node, "report_full_rsoc_2s", &chip->report_full_rsoc);
		if (ret)
			xmc_err("%s failed to parse report_full_rsoc_2s\n", chip->log_tag);

		ret = of_property_read_u32(node, "soc_gap_2s", &chip->soc_gap);
		if (ret)
			xmc_err("%s failed to parse soc_gap_2s\n", chip->log_tag);
	}

	return ret;
}

static int fg_check_device(struct fg_chip *chip)
{
	u8 data1[32], data2[32], data3[32];
	int ret = 0, i = 0;

	ret = fg_mac_read_block(chip, FG_MAC_CMD_DEVICE_NAME, data1, 32);
	if (ret)
		xmc_err("[FG_UNKNOWN]failed to get FG_MAC_CMD_DEVICE_NAME\n");

	for (i = 0; i < 8; i++) {
		if (data1[i] >= 'A' && data1[i] <= 'Z')
			data1[i] += 32;
	}

	ret = fg_mac_read_block(chip, FG_MAC_CMD_MANU_NAME, data2, 32);
	if (ret)
		xmc_err("[FG_UNKNOWN]failed to get FG_MAC_CMD_MANU_NAME\n");

	for (i = 0; i < 2; i++) {
		if (data2[i] >= 'A' && data2[i] <= 'Z')
			data2[i] += 32;
	}

	ret = fg_mac_read_block(chip, FG_MAC_CMD_DEVICE_CHEM, data3, 32);
	if (ret)
		xmc_info("[FG_UNKNOWN]failed to get FG_MAC_CMD_DEVICE_CHEM\n");

	if (!strncmp(data1, "bq27z561", 8)) {
		chip->device_name = FG_BQ27Z561;
		strcat(chip->log_tag, "[FG_BQ27Z561_");
	} else if (!strncmp(data1, "nfg1000a", 8)) {
		chip->device_name = FG_NFG1000A;
		strcat(chip->log_tag, "[FG_NFG1000A_");
	} else if (!strncmp(data1, "nfg1000b", 8)) {
		chip->device_name = FG_NFG1000B;
		strcat(chip->log_tag, "[FG_NFG1000B_");
	} else if (!strncmp(data1, "bq28z610", 8)) {
		chip->device_name = FG_BQ28Z610;
		strcat(chip->log_tag, "[FG_BQ28Z610_");
	} else {
		chip->device_name = FG_UNKNOWN;
		strcat(chip->log_tag, "[FG_UNKNOWN_");
	}

	if (!strncmp(&data3[1], "L", 1)) {
		chip->device_chem = CHEM_LWN;
		strcat(chip->log_tag, "LWN]");
	} else if(!strncmp(&data3[1], "F", 1)) {
		chip->device_chem = CHEM_ATL;
		strcat(chip->log_tag, "ATL]");
	} else {
		chip->device_chem = CHEM_UNKNOWN;
		strcat(chip->log_tag, "UNKNOWN]");
	}

	if (!strncmp(data2, "mi", 2) && chip->device_name != FG_UNKNOWN && chip->device_chem != CHEM_UNKNOWN)
		chip->chip_ok = true;
	else
		chip->chip_ok = false;

	xmc_info("%s device_name = %s, manu_name = %s, device_chem = %s\n", chip->log_tag, data1, data2, data3);

	return ret;
}
static int fg_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fg_chip *chip = &g_chip->battery;
	int ret = 0;

	xmc_info("[FG_UNKNOWN]FG probe start\n");

	chip->dev = &client->dev;
	chip->client = client;

	i2c_set_clientdata(client, chip);

	memcpy(chip->regs, fg_regs, NUM_REGS);
	chip->rw_lock = false;
	chip->i2c_error_count = 0;
	mutex_init(&chip->i2c_rw_lock);
	chip->gauge_wakelock = wakeup_source_register(chip->dev, "gauge_wakelock");

	fg_check_device(chip);

	ret = fg_parse_dt(chip);
	if (ret) {
		xmc_err("%s failed to parse DTS\n", chip->log_tag);
		return ret;
	}

	ret = fg_register_psy(chip);
	if (ret) {
		xmc_err("%s failed to register psy\n", chip->log_tag);
		return ret;
	}

	INIT_DELAYED_WORK(&chip->clear_rw_lock_work, fg_clear_rw_lock);

	ret = sysfs_create_group(&g_chip->dev->kobj, &fg_attr_group);
	if (ret) {
		xmc_err("%s failed to register sysfs\n", chip->log_tag);
		return ret;
	}

	xmc_info("%s FG probe success\n", chip->log_tag);

	return 0;
}

static int fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fg_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->clear_rw_lock_work);

	return 0;
}

static int fg_resume(struct device *dev)
{
	return 0;
}

static int fg_remove(struct i2c_client *client)
{
	struct fg_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(g_chip->bms_psy);
	mutex_destroy(&chip->i2c_rw_lock);
	sysfs_remove_group(&chip->dev->kobj, &fg_attr_group);

	return 0;
}

static void fg_shutdown(struct i2c_client *client)
{
	struct fg_chip *chip = i2c_get_clientdata(client);

	xmc_info("%s chip fuel gauge driver shutdown!\n", chip->log_tag);
}

static struct of_device_id fg_match_table[] = {
	{.compatible = "bq27z561",},
	{},
};
MODULE_DEVICE_TABLE(of, fg_match_table);

static const struct i2c_device_id fg_id[] = {
	{ "bq27z561", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, fg_id);

static const struct dev_pm_ops fg_pm_ops = {
	.resume		= fg_resume,
	.suspend	= fg_suspend,
};

static struct i2c_driver fg_driver = {
	.driver	= {
		.name   = "bq27z561",
		.owner  = THIS_MODULE,
		.of_match_table = fg_match_table,
		.pm     = &fg_pm_ops,
	},
	.id_table       = fg_id,

	.probe          = fg_probe,
	.remove		= fg_remove,
	.shutdown	= fg_shutdown,
};

bool bq27z561_init(struct charge_chip *chip)
{
	g_chip = chip;

        if (i2c_add_driver(&fg_driver))
                return false;
        else
                return true;
}
