/*
 * bq28z610 fuel gauge driver
 *
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

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/hwid.h>
#include <mt-plat/v1/charger_class.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#include "../mediatek/charger/mtk_charger_init.h"
#include "../mediatek/charger/mtk_charger_intf.h"

#define RANDOM_CHALLENGE_LEN_MAX	32
#define RANDOM_CHALLENGE_LEN_BQ27Z561	32
#define RANDOM_CHALLENGE_LEN_BQ28Z610	20
#define FG_DEVICE_CHEM_LEN_MAX		10

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

#define BQ_REPORT_FULL_SOC	9800
#define BQ_CHARGE_FULL_SOC	9750
#define BQ_RECHARGE_SOC		9800
#define BQ_DEFUALT_FULL_SOC	100

#define FG_MONITOR_DELAY_3S	3000
#define FG_MONITOR_DELAY_8S     8000
#define FG_MONITOR_DELAY_10S	10000

enum bq_fg_device_name {
	BQ_FG_UNKNOWN,
	BQ_FG_BQ27Z561,
	BQ_FG_BQ28Z610,
	BQ_FG_NFG1000A,
	BQ_FG_NFG1000B,
};

enum bq_fg_reg_idx {
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

static u8 bq_fg_regs[NUM_REGS] = {
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

enum bq_fg_mac_cmd {
	FG_MAC_CMD_CTRL_STATUS	= 0x0000,
	FG_MAC_CMD_DEV_TYPE	= 0x0001,
	FG_MAC_CMD_FW_VER	= 0x0002,
	FG_MAC_CMD_HW_VER	= 0x0003,
	FG_MAC_CMD_IF_SIG	= 0x0004,
	FG_MAC_CMD_CHEM_ID	= 0x0006,
	FG_MAC_CMD_SHUTDOWN	= 0x0010,
	FG_MAC_CMD_GAUGING	= 0x0021,
	FG_MAC_CMD_SEAL		= 0x0030,
	FG_MAC_CMD_FASTCHARGE_EN = 0x003E,
	FG_MAC_CMD_FASTCHARGE_DIS = 0x003F,
	FG_MAC_CMD_DEV_RESET	= 0x0041,
	FG_MAC_CMD_DEVICE_NAME	= 0x004A,
	FG_MAC_CMD_DEVICE_CHEM	= 0x004B,
	FG_MAC_CMD_MANU_NAME	= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS = 0x0055,
	FG_MAC_CMD_LIFETIME1	= 0x0060,
	FG_MAC_CMD_LIFETIME3	= 0x0062,
	FG_MAC_CMD_DASTATUS1	= 0x0071,
	FG_MAC_CMD_ITSTATUS1	= 0x0073,
	FG_MAC_CMD_QMAX		= 0x0075,
	FG_MAC_CMD_FCC_SOH	= 0x0077,
	FG_MAC_CMD_RA_TABLE	= 0x40C0,
};

struct bq_fg_chip {
	struct device *dev;
	struct i2c_client *client;
	struct mutex i2c_rw_lock;
	struct mutex data_lock;

	u8 regs[NUM_REGS];
	char model_name[I2C_NAME_SIZE];
	char log_tag[I2C_NAME_SIZE];
	char device_chem[FG_DEVICE_CHEM_LEN_MAX];
	int device_name;
	bool chip_ok;
	bool rw_lock;
	int i2c_error_count;
	bool batt_fc;

	int monitor_delay;
	int ui_soc;
	int rsoc;
	int raw_soc;
	int fcc;
	int rm;
	int dc;
	int soh;
	u16 qmax[2];
	u16 trmq;
	u16 tfcc;
	u16 cell_voltage[3];
	bool fast_chg;
	int vbat;
	int tbat;
	int ibat;
	int charge_current;
	int charge_voltage;
	int cycle_count;
	int fake_cycle_count;
	int last_soc;

	int fake_soc;
	int fake_tbat;

	struct wakeup_source *gauge_wakelock;
	struct delayed_work monitor_work;
	struct delayed_work clear_rw_lock_work;
	struct power_supply *fg_psy;
	struct power_supply *batt_psy;
	struct power_supply_desc fg_psy_d;

	u8 digest[RANDOM_CHALLENGE_LEN_MAX];
	bool authenticate;
	bool	update_now;
	int	optimiz_soc;
	bool	ffc_smooth;
	int	*dec_rate_seq;
	int	dec_rate_len;

	int	report_full_rsoc;
	int	soc_gap;
	int	normal_shutdown_vbat;
	int	critical_shutdown_vbat;
	int	cool_critical_shutdown_vbat;
	bool	shutdown_delay;
	bool	enable_shutdown_delay;
	bool	shutdown_flag;
	bool	shutdown_mode;

	int adapting_power;
	int slave_connect_gpio;
};

static int product_name = UNKNOW;
static int log_level = 1;

#define fg_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

static int __fg_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	s32 ret = 0;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		fg_info("i2c write byte fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
		return ret;
	}

	return 0;
}

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret = 0;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		fg_info("i2c read word fail: can't read from reg 0x%02X\n", reg);
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
			fg_info("i2c read reg 0x%02X faild\n", reg + i);
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
			fg_info("i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return ret;
}

static int fg_write_byte(struct bq_fg_chip *bq, u8 reg, u8 val)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_read_word(struct bq_fg_chip *bq, u8 reg, u16 *val)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_word(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_block(bq->client, reg, buf, len);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_write_block(struct bq_fg_chip *bq, u8 reg, u8 *data, u8 len)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_block(bq->client, reg, data, len);
	mutex_unlock(&bq->i2c_rw_lock);

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

static int fg_mac_read_block(struct bq_fg_chip *bq, u16 cmd, u8 *buf, u8 len)
{
	int ret;
	u8 cksum_calc, cksum;
	u8 t_buf[40];
	u8 t_len;
	int i;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_block(bq->client, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0){
		mutex_unlock(&bq->i2c_rw_lock);
		return ret;
	}

	msleep(4);

	ret = __fg_read_block(bq->client, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0){
		mutex_unlock(&bq->i2c_rw_lock);
		return ret;
	}
	mutex_unlock(&bq->i2c_rw_lock);

	cksum = t_buf[34];
	t_len = t_buf[35];

	cksum_calc = fg_checksum(t_buf, t_len - 2);
	if (cksum_calc != cksum) {
		fg_err("%s failed to checksum\n", bq->log_tag);
		return 1;
	}

	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];

	return 0;
}

static int fg_mac_write_block(struct bq_fg_chip *bq, u16 cmd, u8 *data, u8 len)
{
	int ret;
	u8 cksum;
	u8 t_buf[40];
	int i;

	if (len > 32)
		return -1;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	for (i = 0; i < len; i++)
		t_buf[i+2] = data[i];

	/*write command/addr, data*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, len + 2);
	if (ret < 0) {
		fg_err("%s failed to write block\n", bq->log_tag);
		return ret;
	}

	cksum = fg_checksum(data, len + 2);
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);

	return ret;
}

static int fg_sha256_auth(struct bq_fg_chip *bq, u8 *challenge, int length)
{
	int ret = 0;
	u8 cksum_calc = 0, data[2] = {0};

	/*
	1. The host writes 0x00 to 0x3E.
	2. The host writes 0x00 to 0x3F
	*/
	data[0] = 0x00;
	data[1] = 0x00;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], data, 2);
	if (ret < 0)
		return ret;
	/*
	3. Write the random challenge should be written in a 32-byte block to address 0x40-0x5F
	*/
	msleep(2);

	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], challenge, length);
	if (ret < 0)
		return ret;

	/*4. Write the checksum (2’s complement sum of (1), (2), and (3)) to address 0x60.*/
	cksum_calc = fg_checksum(challenge, length);
	ret = fg_write_byte(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], cksum_calc);
	if (ret < 0)
		return ret;

	/*5. Write the length to address 0x61.*/
	ret = fg_write_byte(bq, bq->regs[BQ_FG_REG_MAC_DATA_LEN], length + 4);
	if (ret < 0)
		return ret;

	msleep(300);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], bq->digest, length);
	if (ret < 0)
		return ret;

	return 0;
}

static int fg_read_status(struct bq_fg_chip *bq)
{
	u16 flags = 0;
	int ret = 0;

	if (bq->rw_lock)
		return bq->batt_fc;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
	if (ret < 0)
		return ret;

	bq->batt_fc = !!(flags & BIT(5));

	return 0;
}

static int fg_read_rsoc(struct bq_fg_chip *bq)
{
	u16 soc = 0;
	static u16 s_soc = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_soc ? s_soc : 50;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read RSOC\n", bq->log_tag);
			soc = (s_soc ? s_soc : 50);
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_soc = soc;
	return soc;
}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	u16 tbat = 0;
	static u16 s_tbat = 0;
	bool retry = false;
	int ret = 0;

	if (bq->fake_tbat)
		return bq->fake_tbat;

	if (bq->rw_lock)
		return s_tbat - 2730;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &tbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read TBAT\n", bq->log_tag);
			tbat = 2980;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_tbat = tbat;
	return tbat - 2730;
}

static void fg_read_cell_voltage(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock) {
		if (bq->cell_voltage[0] == 0 && bq->cell_voltage[1] == 0 && bq->cell_voltage[2] == 0) {
			bq->cell_voltage[0] = 4000;
			bq->cell_voltage[1] = 4000;
			bq->cell_voltage[2] = 2 * max(bq->cell_voltage[0], bq->cell_voltage[1]);
		}
		return;
	}

retry:
	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 32);
	if (ret) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read cell voltage\n", bq->log_tag);
			bq->cell_voltage[0] = 4000;
			bq->cell_voltage[1] = 4000;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		bq->cell_voltage[0] = (data[1] << 8) | data[0];
		bq->cell_voltage[1] = (data[3] << 8) | data[2];
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	bq->cell_voltage[2] = 2 * max(bq->cell_voltage[0], bq->cell_voltage[1]);
}

static void fg_read_volt(struct bq_fg_chip *bq)
{
	u16 vbat = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock) {
		if (bq->vbat == 0)
			bq->vbat = (product_name == RUBYPLUS) ? 8000 : 4000;

		if (bq->device_name == BQ_FG_BQ28Z610)
			fg_read_cell_voltage(bq);
		else
			bq->cell_voltage[0] = bq->cell_voltage[1] = bq->cell_voltage[2] = bq->vbat;

		return;
	}

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &vbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read VBAT\n", bq->log_tag);
			vbat = (product_name == RUBYPLUS) ? 8000 : 4000;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	bq->vbat = (int)vbat;

	if (bq->device_name == BQ_FG_BQ28Z610)
		fg_read_cell_voltage(bq);
	else
		bq->cell_voltage[0] = bq->cell_voltage[1] = bq->cell_voltage[2] = bq->vbat;
}

static int fg_read_avg_current(struct bq_fg_chip *bq)
{
	s16 avg_ibat = 0;
	static s16 s_avg_ibat = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_avg_ibat;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], (u16 *)&avg_ibat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read AVG_IBAT\n", bq->log_tag);
			avg_ibat = 0;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_avg_ibat = avg_ibat = -1 * avg_ibat;

	return avg_ibat;
}

static int fg_read_current(struct bq_fg_chip *bq)
{
	s16 ibat = 0;
	static s16 s_ibat = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_ibat;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CN], (u16 *)&ibat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read IBAT\n", bq->log_tag);
			ibat = 0;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_ibat = ibat = -1 * ibat;

	return ibat;
}

static int fg_read_fcc(struct bq_fg_chip *bq)
{
	u16 fcc = 0;
	static u16 s_fcc = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_fcc;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read FCC\n", bq->log_tag);
			fcc = (product_name == RUBYPLUS) ? 4500 : 5160;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_fcc = fcc;
	return fcc;
}

static int fg_read_rm(struct bq_fg_chip *bq)
{
	u16 rm = 0;
	static u16 s_rm = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_rm;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read RM\n", bq->log_tag);
			rm = (product_name == RUBYPLUS) ? 2250 : 2580;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_rm = rm;
	return rm;
}

static int fg_read_dc(struct bq_fg_chip *bq)
{
	u16 dc = 0;
	static u16 s_dc = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_dc;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_DC], &dc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read DC\n", bq->log_tag);
			dc = (product_name == RUBYPLUS) ? 4500 : 5160;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_dc = dc;
	return dc;
}

static int fg_read_soh(struct bq_fg_chip *bq)
{
	u16 soh = 0;
	static u16 s_soh = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_soh;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read SOH\n", bq->log_tag);
			soh = 50;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_soh = soh;
	return soh;
}

static int fg_read_cv(struct bq_fg_chip *bq)
{
	u16 cv = 0;
	static u16 s_cv = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_cv;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_VOL], &cv);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read CV\n", bq->log_tag);
			cv = (product_name == RUBYPLUS) ? 8960 : 4480;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_cv = cv;
	return cv;
}

static int fg_read_cc(struct bq_fg_chip *bq)
{
	u16 cc = 0;
	static u16 s_cc = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_cc;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_CUR], &cc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read CC\n", bq->log_tag);
			cc = (product_name == RUBYPLUS) ? 11000 : 12400;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_cc = cc;
	return cc;
}

static int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	u16 cc = 0;
	static u16 s_cc = 0;
	bool retry = false;
	int ret = 0;

	if (bq->rw_lock)
		return s_cc;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read CC\n", bq->log_tag);
			cc = 0;
			if (bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		if (bq->i2c_error_count > 0)
			bq->i2c_error_count--;
	}

	s_cc = cc;
	return cc;
}

static int fg_get_raw_soc(struct bq_fg_chip *bq)
{
	int raw_soc = 0;

	bq->rm = fg_read_rm(bq);
	bq->fcc = fg_read_fcc(bq);

	raw_soc = bq->rm * 10000 / bq->fcc;

	return raw_soc;
}

static int fg_get_soc_decimal_rate(struct bq_fg_chip *bq)
{
	int soc, i;

	if (bq->dec_rate_len <= 0)
		return 0;

	soc = fg_read_rsoc(bq);

	for (i = 0; i < bq->dec_rate_len; i += 2) {
		if (soc < bq->dec_rate_seq[i]) {
			return bq->dec_rate_seq[i - 1];
		}
	}

	return bq->dec_rate_seq[bq->dec_rate_len - 1];
}

static int fg_get_soc_decimal(struct bq_fg_chip *bq)
{
	int rsoc, raw_soc;

	if (!bq)
		return 0;

	rsoc = fg_read_rsoc(bq);
	raw_soc = fg_get_raw_soc(bq);

	if (bq->ui_soc > rsoc)
		return 0;

	return raw_soc % 100;
}

static void fg_read_qmax(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	int ret = 0;

	if (bq->rw_lock)
		return;

	if (bq->device_name == BQ_FG_BQ27Z561|| bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, data, 14);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", bq->log_tag);
	} else if (bq->device_name == BQ_FG_BQ28Z610) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, data, 20);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", bq->log_tag);
	} else {
		fg_err("%s not support device name\n", bq->log_tag);
	}

	bq->qmax[0] = (data[1] << 8) | data[0];
	bq->qmax[1] = (data[3] << 8) | data[2];
}

static void fg_read_trmq(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	int ret = 0;

	if (bq->rw_lock)
		return;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, data, 24);
	if (ret < 0)
		fg_err("%s failed to read MAC\n", bq->log_tag);

	if (bq->device_name == BQ_FG_BQ28Z610 || bq->device_name == BQ_FG_BQ27Z561) {
		bq->trmq = (data[1] << 8) | data[0];
		bq->tfcc = (data[9] << 8) | data[8];
	} else if (bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B) {
		bq->trmq = (data[1] << 8) | data[0];
		bq->tfcc = (data[3] << 8) | data[2];
	}
}

static int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	u8 data[5] = {0};
	int ret = 0;

	if (bq->rw_lock)
		return ret;

	data[0] = bq->fast_chg = enable;

	if (bq->device_name == BQ_FG_BQ28Z610)
		return ret;

	if (enable) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_EN, data, 2);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	}

	return ret;
}

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	fg_dbg("now:%ld, last:%ld, delta:%d\n", time_now, time_last, *delta_time);

	return 0;
}

#define BATT_HIGH_AVG_CURRENT		1000
#define NORMAL_TEMP_CHARGING_DELTA	10000
#define NORMAL_DISTEMP_CHARGING_DELTA	60000
#define LOW_TEMP_CHARGING_DELTA		5000
#define LOW_TEMP_DISCHARGING_DELTA	10000
#define FFC_SMOOTH_LEN			4
#define RUBYPLUS_FFC_SMOOTH_LEN	4
#define FG_RAW_SOC_FULL			10000
#define FG_REPORT_FULL_SOC		9100
#define FG_OPTIMIZ_FULL_TIME		80000

struct ffc_smooth {
	int curr_lim;
	int time;
};

struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    300000},
	{300,  150000},
	{600,   72000},
	{1000,  50000},
};

struct ffc_smooth rubyplus_ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    150000},
	{300,  100000},
	{600,   20000},
	{1000,   10000},
};

struct ffc_smooth rubyplus_ffc_chg_smooth[RUBYPLUS_FFC_SMOOTH_LEN] = {
	{-15000,  2000},
	{-10000,  3000},
	{-5000,  4000},
	{0,      5000},
};

static int bq_battery_soc_smooth_tracking_sencond(struct bq_fg_chip *bq,
	int raw_soc, int batt_soc, int soc)
{
	static ktime_t changed_time = -1;
	int unit_time = 0, delta_time = 0;
	int change_delta = 0;
	int soc_changed = 0;

	if (raw_soc > bq->report_full_rsoc) {
		if (raw_soc == 10000 && bq->last_soc < 99) {
			unit_time = 20000;
			calc_delta_time(changed_time, &change_delta);
			if (delta_time < 0) {
				changed_time = ktime_get();
				delta_time = 0;
			}
			delta_time = change_delta / unit_time;
			soc_changed = min(1, delta_time);
			if (soc_changed) {
				soc = bq->last_soc + soc_changed;
				fg_info("%s soc increase changed = %d\n", bq->log_tag, soc_changed);
			} else {
				soc = bq->last_soc;
			}
		} else {
			soc = 100;
		}
	} else if (raw_soc > 990) {
		soc += bq->soc_gap;
		if (soc > 99)
			soc = 99;
	} else {
		if (raw_soc == 0 && bq->last_soc > 1) {
			bq->ffc_smooth = false;
			unit_time = 5000;
			calc_delta_time(changed_time, &change_delta);
			delta_time = change_delta / unit_time;
			if (delta_time < 0) {
				changed_time = ktime_get();
				delta_time = 0;
			}
			soc_changed = min(1, delta_time);
			if (soc_changed) {
				fg_info("%s soc reduce changed = %d\n", bq->log_tag, soc_changed);
				soc = bq->last_soc - soc_changed;
			} else
				soc = bq->last_soc;
		} else {
			soc = (raw_soc + 89) / 90;
		}
	}

	if (soc >= 100)
		soc = 100;
	if (soc < 0)
		soc = batt_soc;

	if (bq->last_soc <= 0)
		bq->last_soc = soc;
	if (bq->last_soc != soc) {
		if(abs(soc - bq->last_soc) > 1){
			union power_supply_propval pval = {0, };
			int status,rc;

			rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
			status = pval.intval;

			calc_delta_time(changed_time, &change_delta);
			delta_time = change_delta / LOW_TEMP_CHARGING_DELTA;
			if (delta_time < 0) {
				changed_time = ktime_get();
				delta_time = 0;
			}
			soc_changed = min(1, delta_time);
			if(soc_changed){
				changed_time = ktime_get();
			}

			fg_info("avoid jump soc = %d last = %d soc_change = %d state = %d ,delta_time = %d\n",
					soc,bq->last_soc ,soc_changed,status,change_delta);

			if(status == POWER_SUPPLY_STATUS_CHARGING){
				if(soc > bq->last_soc){
					soc = bq->last_soc + soc_changed;
					bq->last_soc = soc;
				}else{
					fg_info("Do not smooth waiting real soc increase here\n");
					soc = bq->last_soc;
				}
			} else if(status != POWER_SUPPLY_STATUS_FULL){
				if(soc < bq->last_soc){
					soc = bq->last_soc - soc_changed;
					bq->last_soc = soc;
				}else{
					fg_info("Do not smooth waiting real soc decrease here\n");
					soc = bq->last_soc;
				}
			}
		}else{
			changed_time = ktime_get();
			bq->last_soc = soc;
		}
	}
	return soc;
}

static int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int batt_soc, int batt_temp, int batt_ma)
{
	static int last_batt_soc = -1, system_soc, cold_smooth;
	static int last_status;
	int change_delta = 0, rc;
	int optimiz_delta = 0, status;
	static ktime_t last_change_time;
	static ktime_t last_optimiz_time;
	int unit_time = 0;
	int soc_changed = 0, delta_time = 0;
	static int optimiz_soc, last_raw_soc;
	union power_supply_propval pval = {0, };
	int batt_ma_avg, i;

	if (bq->optimiz_soc > 0) {
		bq->ffc_smooth = true;
		last_batt_soc = bq->optimiz_soc;
		system_soc = bq->optimiz_soc;
		last_change_time = ktime_get();
		bq->optimiz_soc = 0;
	}

	if (last_batt_soc < 0)
		last_batt_soc = batt_soc;

	if (raw_soc == FG_RAW_SOC_FULL)
		bq->ffc_smooth = false;

	if (bq->ffc_smooth) {
		rc = power_supply_get_property(bq->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if (rc < 0) {
			fg_info("failed get batt staus\n");
			return -EINVAL;
		}
		status = pval.intval;
		if (batt_soc == system_soc) {
			bq->ffc_smooth = false;
			return batt_soc;
		}
		if (status != last_status) {
			if (last_status == POWER_SUPPLY_STATUS_CHARGING
					&& status == POWER_SUPPLY_STATUS_DISCHARGING)
				last_change_time = ktime_get();
			last_status = status;
		}
	}

	if (bq->fast_chg && raw_soc >= FG_REPORT_FULL_SOC && raw_soc != FG_RAW_SOC_FULL) {
		if (last_optimiz_time == 0)
			last_optimiz_time = ktime_get();
		calc_delta_time(last_optimiz_time, &optimiz_delta);
		delta_time = optimiz_delta / FG_OPTIMIZ_FULL_TIME;
		soc_changed = min(1, delta_time);
		if (raw_soc > last_raw_soc && soc_changed) {
			last_raw_soc = raw_soc;
			optimiz_soc += soc_changed;
			last_optimiz_time = ktime_get();
			fg_info("optimiz_soc:%d, last_optimiz_time%ld\n",
					optimiz_soc, last_optimiz_time);
			if (optimiz_soc > 100)
				optimiz_soc = 100;
			bq->ffc_smooth = true;
		}
		if (batt_soc > optimiz_soc) {
			optimiz_soc = batt_soc;
			last_optimiz_time = ktime_get();
		}
		if (bq->ffc_smooth)
			batt_soc = optimiz_soc;
		last_change_time = ktime_get();
	} else {
		optimiz_soc = batt_soc + 1;
		last_raw_soc = raw_soc;
		last_optimiz_time = ktime_get();
	}

	calc_delta_time(last_change_time, &change_delta);
	batt_ma_avg = fg_read_avg_current(bq);
	if (batt_temp > 150/* BATT_COOL_THRESHOLD */ && !cold_smooth && batt_soc != 0) {
		if (bq->ffc_smooth && (status == POWER_SUPPLY_STATUS_DISCHARGING ||
					status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
					batt_ma_avg > 50)) {
			for (i = 1; i < FFC_SMOOTH_LEN; i++) {
				if (batt_ma_avg < ffc_dischg_smooth[i].curr_lim) {
					unit_time = ffc_dischg_smooth[i-1].time;
					break;
				}
			}
			if (i == FFC_SMOOTH_LEN) {
				unit_time = ffc_dischg_smooth[FFC_SMOOTH_LEN-1].time;
			}
		}
	} else {
		/* Calculated average current > 1000mA */
		if (batt_ma_avg > BATT_HIGH_AVG_CURRENT)
			/* Heavy loading current, ignore battery soc limit*/
			unit_time = LOW_TEMP_CHARGING_DELTA;
		else
			unit_time = LOW_TEMP_DISCHARGING_DELTA;
		if (batt_soc != last_batt_soc)
			cold_smooth = true;
		else
			cold_smooth = false;
	}
	if (unit_time > 0) {
		delta_time = change_delta / unit_time;
		soc_changed = min(1, delta_time);
	} else {
		if (!bq->ffc_smooth)
			bq->update_now = true;
	}

	fg_info("batt_ma_avg:%d, batt_ma:%d, cold_smooth:%d, optimiz_soc:%d",
			batt_ma_avg, batt_ma, cold_smooth, optimiz_soc);
	fg_info("delta_time:%d, change_delta:%d, unit_time:%d"
			" soc_changed:%d, bq->update_now:%d, bq->ffc_smooth:%d,bq->fast_chg:%d",
			delta_time, change_delta, unit_time,
			soc_changed, bq->update_now, bq->ffc_smooth,bq->fast_chg);

	if (last_batt_soc < batt_soc && batt_ma < 0)
		/* Battery in charging status
		 * update the soc when resuming device
		 */
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc + soc_changed;
	else if (last_batt_soc > batt_soc && batt_ma > 0) {
		/* Battery in discharging status
		 * update the soc when resuming device
		 */
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc - soc_changed;
	}
	bq->update_now = false;

	if (system_soc != last_batt_soc) {
		system_soc = last_batt_soc;
		last_change_time = ktime_get();
	}

	fg_info("raw_soc:%d batt_soc:%d,last_batt_soc:%d,system_soc:%d",
			raw_soc, batt_soc, last_batt_soc, system_soc);

	return system_soc;
}

static int bq_battery_soc_smooth_tracking_new(struct bq_fg_chip *bq, int raw_soc, int batt_soc, int batt_ma)
{
	static int system_soc, last_system_soc;
	int soc_changed = 0, unit_time = 10000, delta_time = 0, soc_delta = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0;
	int  rc, charging_status, i=0, batt_ma_avg = 0;
	union power_supply_propval pval = {0, };
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;

	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if((batt_ma > 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if(batt_ma <= 0)
		ibat_pos_count = 0;

	rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0) {
		fg_info("%s: failed get batt staus\n", __func__);
		return batt_soc;
	}
	charging_status = pval.intval;
	if (bq->tbat < 150) {
		bq->monitor_delay = FG_MONITOR_DELAY_3S;
	}
	if (!raw_soc) {
		bq->monitor_delay = FG_MONITOR_DELAY_10S;
	}
	/*Map system_soc value according to raw_soc */
	if(raw_soc >= bq->report_full_rsoc)
		system_soc = 100;
	else if (product_name == RUBYPRO) {
		system_soc = ((raw_soc + 94) / 95);
		if(system_soc > 99)
			system_soc = 99;
	} else if (product_name == RUBYPLUS) {
		system_soc = ((raw_soc * 10 + 946) / 947);
		if(system_soc > 99)
			system_soc = 99;
	} else {
		system_soc = ((raw_soc + 97) / 98);
		if(system_soc > 99)
			system_soc = 99;
        }
	/*Get the initial value for the first time */
	if(last_change_time == -1){
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = batt_soc;
	}

	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING ) &&
		!bq->rm && bq->tbat < 150 && last_system_soc >= 1)
	{
		batt_ma_avg = fg_read_avg_current(bq);
		for (i = FFC_SMOOTH_LEN-1; i >= 0; i--) {
			if (batt_ma_avg > ((product_name == RUBYPLUS) ? rubyplus_ffc_dischg_smooth[i].curr_lim : ffc_dischg_smooth[i].curr_lim)) {
				unit_time = (product_name == RUBYPLUS) ? rubyplus_ffc_dischg_smooth[i].time :  ffc_dischg_smooth[i].time;
				break;
			}
		}
		fg_info("enter low temperature smooth unit_time=%d batt_ma_avg=%d\n", unit_time, batt_ma_avg);
	}
	else if((product_name == RUBYPLUS) && (charging_status == POWER_SUPPLY_STATUS_CHARGING) && bq->tbat > 150)
	{
		batt_ma_avg = fg_read_avg_current(bq);
		for (i = 0; i < RUBYPLUS_FFC_SMOOTH_LEN; i++) {
			if (batt_ma_avg < (rubyplus_ffc_chg_smooth[i].curr_lim)) {
				unit_time = rubyplus_ffc_chg_smooth[i].time;
				break;
			}
		}
		fg_info("enter ffc charge normal temperature smooth unit_time=%d batt_ma_avg=%d\n", unit_time, batt_ma_avg);
	}

	/*If the soc jump, will smooth one cap every 10S */
	soc_delta = abs(system_soc - last_system_soc);
	if(soc_delta > 1 || (bq->vbat < 3300 && system_soc > 0) || (unit_time != 10000 && soc_delta == 1)){
		//unit_time != 10000 && soc_delta == 1 fix low temperature 2% jump to 0%
		calc_delta_time(last_change_time, &change_delta);
		delta_time = change_delta / unit_time;
		if (delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}
		soc_changed = min(1, delta_time);
		if (soc_changed) {
			if(charging_status == POWER_SUPPLY_STATUS_CHARGING && system_soc > last_system_soc)
				system_soc = last_system_soc + soc_changed;
			else if(charging_status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc)
				system_soc = last_system_soc - soc_changed;
		} else
			system_soc = last_system_soc;
		fg_info("fg jump smooth soc_changed=%d\n", soc_changed);
	}
	if(system_soc < last_system_soc)
		system_soc = last_system_soc - 1;
	/*Avoid mismatches between charging status and soc changes  */
	if (((charging_status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc)) || ((charging_status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;
	fg_info("smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_avg=%d\n" ,
		system_soc, last_system_soc, soc_delta, charging_status, unit_time, batt_ma_avg);

	if(system_soc != last_system_soc){
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}
	if(system_soc > 100)
		system_soc =100;
	if(system_soc < 0)
		system_soc =0;

	if ((system_soc == 0) && ((bq->vbat >= bq->normal_shutdown_vbat) || ((time.tv_sec <= 10)))) {
		system_soc = 1;
		fg_err("uisoc::hold 1 when volt > normal_shutdown_vbat. \n");
	}

	if(bq->last_soc != system_soc){
		bq->last_soc = system_soc;
	}

	return system_soc;
}

static int fg_set_shutdown_mode(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 data[5] = {0};

	fg_info("%s fg_set_shutdown_mode\n", bq->log_tag);

	if (bq->rw_lock)
		return ret;

	bq->shutdown_mode = true;
	data[0] = 1;

	ret = fg_mac_write_block(bq, FG_MAC_CMD_SHUTDOWN, data, 2);
	if (ret)
		fg_err("%s failed to send shutdown cmd 0\n", bq->log_tag);

	ret = fg_mac_write_block(bq, FG_MAC_CMD_SHUTDOWN, data, 2);
	if (ret)
		fg_err("%s failed to send shutdown cmd 1\n", bq->log_tag);

	return ret;
}

static bool battery_get_psy(struct bq_fg_chip *bq)
{
	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			fg_err("%s failed to get batt_psy", bq->log_tag);
			return false;
		}
        }

	return true;
}

static void fg_update_status(struct bq_fg_chip *bq)
{
	int temp_soc = 0;
	static int last_soc = 0, last_temp = 0;

	mutex_lock(&bq->data_lock);
	bq->cycle_count = fg_read_cyclecount(bq);
	bq->rsoc = fg_read_rsoc(bq);
	bq->soh = fg_read_soh(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	bq->ibat = fg_read_current(bq);
	bq->tbat = fg_read_temperature(bq);
	fg_read_status(bq);
	fg_read_volt(bq);
	if (bq->dc == 0)
		bq->dc = fg_read_dc(bq);
	mutex_unlock(&bq->data_lock);

	if (!battery_get_psy(bq)) {
		fg_err("%s fg_update failed to get battery psy\n", bq->log_tag);
		bq->ui_soc = bq->rsoc;
		return;
	} else {
		bq->ui_soc = bq_battery_soc_smooth_tracking_new(bq, bq->raw_soc, bq->rsoc, bq->ibat);
		goto out;
		temp_soc = bq_battery_soc_smooth_tracking(bq, bq->raw_soc, bq->rsoc, bq->tbat, bq->ibat);
		bq->ui_soc = bq_battery_soc_smooth_tracking_sencond(bq, bq->raw_soc, bq->rsoc, temp_soc);

out:
		fg_info("%s [FG_STATUS] [UISOC RSOC RAWSOC TEMP_SOC SOH] = [%d %d %d %d %d], [VBAT CELL0 CELL1 IBAT TBAT FC FAST_MODE CYCLE_COUNT] = [%d %d %d %d %d %d %d %d]\n", bq->log_tag,
			bq->ui_soc, bq->rsoc, bq->raw_soc, temp_soc, bq->soh, bq->vbat, bq->cell_voltage[0], bq->cell_voltage[1], bq->ibat, bq->tbat, bq->batt_fc, bq->fast_chg, bq->cycle_count);

		if (bq->batt_psy && (last_soc != bq->ui_soc || last_temp != bq->tbat || bq->ui_soc == 0 || bq->rsoc == 0)) {
			fg_err("%s last_soc = %d, last_temp = %d\n", __func__, last_soc, last_temp);
			power_supply_changed(bq->batt_psy);
		}

		last_soc = bq->ui_soc;
		last_temp = bq->tbat;
        }
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);

	fg_update_status(bq);

	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(bq->monitor_delay));
}

static void fg_clear_rw_lock(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, clear_rw_lock_work.work);

	bq->rw_lock = false;
	__pm_relax(bq->gauge_wakelock);
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_REPORT_FULL_RAWSOC,
	POWER_SUPPLY_PROP_SOC_DECIMAL,
	POWER_SUPPLY_PROP_SOC_DECIMAL_RATE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_FAKE_CYCLE_COUNT,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_SOH,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_SHUTDOWN_MODE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_I2C_ERROR_COUNT,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MONITOR_DELAY,
	POWER_SUPPLY_PROP_DEVICE_CHEM,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_RSOC,
	POWER_SUPPLY_PROP_RM,
	POWER_SUPPLY_PROP_FCC,
	POWER_SUPPLY_PROP_BMS_SLAVE_CONNECT_ERROR,
};

static int fg_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
	static bool last_shutdown_delay = false;
	union power_supply_propval pval = {0, };
	int tem;

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_DEVICE_CHEM:
		val->strval = bq->device_chem;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		mutex_lock(&bq->data_lock);
		fg_read_volt(bq);
		val->intval = bq->cell_voltage[2] * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		bq->ibat = fg_read_current(bq);
		val->intval = bq->ibat * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc) {
			val->intval = bq->fake_soc;
			break;
		}

		val->intval = bq->ui_soc;
		//add shutdown delay feature
		if (bq->enable_shutdown_delay) {
			if (val->intval == 0) {
				tem = fg_read_temperature(bq);

				if (!battery_get_psy(bq)) {
					fg_err("%s get capacity failed to get battery psy\n", bq->log_tag);
					break;
				} else
					power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);

				if (pval.intval != POWER_SUPPLY_STATUS_CHARGING) {
					if (bq->cell_voltage[2] >= bq->normal_shutdown_vbat) {
						bq->shutdown_delay = false;
						val->intval = 1;
					}else if (((tem > 0) && (bq->cell_voltage[2] >= bq->critical_shutdown_vbat))
						|| ((tem <= 0) && (bq->cell_voltage[2] >= bq->cool_critical_shutdown_vbat))) {
						bq->shutdown_delay = true;
						val->intval = 1;
					} else {
						bq->shutdown_delay = false;
					}
					fg_err("last_shutdown= %d. shutdown= %d, soc =%d, voltage =%d\n", last_shutdown_delay, bq->shutdown_delay, val->intval, bq->cell_voltage[2]);
				} else {
					bq->shutdown_delay = false;
					if (((tem > 0) && (bq->cell_voltage[2] >= bq->critical_shutdown_vbat))
						|| ((tem < 0) && (bq->cell_voltage[2] >= bq->cool_critical_shutdown_vbat)))
						val->intval = 1;
				}
			} else {
				bq->shutdown_delay = false;
			}

			if (val->intval == 0)
				bq->shutdown_flag = true;
			if (bq->shutdown_flag){
				val->intval = 0;
				bq->shutdown_delay = false;
			}

			if (last_shutdown_delay != bq->shutdown_delay) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->fg_psy)
					power_supply_changed(bq->fg_psy);
			}
		}
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = bq->shutdown_delay;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		val->intval = bq->raw_soc;
		break;
	case POWER_SUPPLY_PROP_REPORT_FULL_RAWSOC:
		val->intval = bq->report_full_rsoc;
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
		mutex_lock(&bq->data_lock);
		val->intval = fg_get_soc_decimal(bq);
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
		mutex_lock(&bq->data_lock);
		val->intval = fg_get_soc_decimal_rate(bq);
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_tbat) {
			val->intval = bq->fake_tbat;
			break;
		}
		val->intval = bq->tbat;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B)
			val->intval = bq->fcc;
		else if (bq->device_name == BQ_FG_BQ28Z610)
			val->intval = bq->fcc * 2;
		else
			val->intval = 4500;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (bq->device_name == BQ_FG_BQ27Z561|| bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B)
			val->intval = bq->dc;
		else if (bq->device_name == BQ_FG_BQ28Z610)
			val->intval = bq->dc * 2;
		else
			val->intval = 4500;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = bq->rm * 1000;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		if (bq->fake_cycle_count)
			val->intval = bq->fake_cycle_count;
		else
			val->intval = bq->cycle_count;
		break;
	case POWER_SUPPLY_PROP_FAKE_CYCLE_COUNT:
		val->intval = bq->fake_cycle_count;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = 100000;
		break;
	case POWER_SUPPLY_PROP_SOH:
		val->intval = bq->soh;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = bq->authenticate;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		val->intval = bq->fast_chg;
		break;
	case POWER_SUPPLY_PROP_MONITOR_DELAY:
		val->intval = bq->monitor_delay;
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_MODE:
		val->intval = bq->shutdown_mode;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		if (product_name == RUBY ) {
			val->intval = gpio_get_value(bq->slave_connect_gpio);
			if(val->intval == 1){
					val->intval = 0;/*slave connect fail*/
					break;
			}
		}
		val->intval = bq->chip_ok;
		break;
	case POWER_SUPPLY_PROP_I2C_ERROR_COUNT:
		val->intval = bq->i2c_error_count;
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		mutex_lock(&bq->data_lock);
		fg_read_status(bq);
		val->intval = bq->batt_fc;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_RSOC:
		val->intval = bq->rsoc;
		break;
	case POWER_SUPPLY_PROP_RM:
		val->intval = bq->rm;
		break;
	case POWER_SUPPLY_PROP_FCC:
		val->intval = bq->fcc;
		break;
	case POWER_SUPPLY_PROP_BMS_SLAVE_CONNECT_ERROR:
	 	if (product_name == RUBY ) {
			val->intval = gpio_get_value(bq->slave_connect_gpio);
        }
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		bq->fake_tbat = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq->fake_soc = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		bq->authenticate = !!val->intval;
		bq->rw_lock = false;
		fg_info("%s authenticate = %d\n", bq->log_tag, bq->authenticate);
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(bq, !!val->intval);
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_MODE:
		fg_set_shutdown_mode(bq);
		break;
	case POWER_SUPPLY_PROP_MONITOR_DELAY:
		bq->monitor_delay = val->intval;
		break;
	case POWER_SUPPLY_PROP_FAKE_CYCLE_COUNT:
		bq->fake_cycle_count = val->intval;
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
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
	case POWER_SUPPLY_PROP_SHUTDOWN_MODE:
	case POWER_SUPPLY_PROP_MONITOR_DELAY:
	case POWER_SUPPLY_PROP_FAKE_CYCLE_COUNT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int fg_register_psy(struct bq_fg_chip *bq)
{
	struct power_supply_config fg_psy_cfg = {};

	bq->fg_psy_d.name = "bms";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_get_property;
	bq->fg_psy_d.set_property = fg_set_property;
	bq->fg_psy_d.property_is_writeable = fg_prop_is_writeable;
	fg_psy_cfg.drv_data = bq;
	fg_psy_cfg.num_supplicants = 0;

	bq->fg_psy = devm_power_supply_register(bq->dev, &bq->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		fg_err("%s failed to register fg_psy", bq->log_tag);
		return PTR_ERR(bq->fg_psy);
	}

	return 0;
}

static ssize_t fg_show_qmax0(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&bq->data_lock);
	fg_read_qmax(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->qmax[0]);

	return ret;
}

static ssize_t fg_show_qmax1(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->qmax[1]);

	return ret;
}

static ssize_t fg_show_trmq(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&bq->data_lock);
	fg_read_trmq(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->trmq);

	return ret;
}

static ssize_t fg_show_tfcc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->tfcc);

	return ret;
}

static ssize_t fg_show_cell0_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->cell_voltage[0]);

	return ret;
}

static ssize_t fg_show_cell1_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->cell_voltage[1]);

	return ret;
}

static ssize_t fg_show_rsoc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int rsoc = 0, ret = 0;

	mutex_lock(&bq->data_lock);
	rsoc = fg_read_rsoc(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", rsoc);

	return ret;
}

static ssize_t fg_show_fcc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int fcc = 0, ret = 0;

	mutex_lock(&bq->data_lock);
	fcc = fg_read_fcc(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", fcc);

	return ret;
}

static ssize_t fg_show_rm(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int rm = 0, ret = 0;

	mutex_lock(&bq->data_lock);
	rm = fg_read_rm(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", rm);

	return ret;
}

int fg_stringtohex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while(cnt < (tmplen / 2))
	{
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p ++;
		cnt ++;
	}
	if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;

	return tmplen / 2 + tmplen % 2;
}

static ssize_t fg_verify_digest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 digest_buf[4] = {0};
	int len = 0, i = 0;

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B) {
		for (i = 0; i < RANDOM_CHALLENGE_LEN_BQ27Z561; i++) {
			memset(digest_buf, 0, sizeof(digest_buf));
			snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bq->digest[i]);
			strlcat(buf, digest_buf, RANDOM_CHALLENGE_LEN_BQ27Z561 * 2 + 1);
		}
	} else if (bq->device_name == BQ_FG_BQ28Z610) {
		for (i = 0; i < RANDOM_CHALLENGE_LEN_BQ28Z610; i++) {
			memset(digest_buf, 0, sizeof(digest_buf));
			snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bq->digest[i]);
			strlcat(buf, digest_buf, RANDOM_CHALLENGE_LEN_BQ28Z610 * 2 + 1);
		}
	} else {
		fg_err("%s not support device name\n", bq->log_tag);
	}

	len = strlen(buf);
	buf[len] = '\0';

	return strlen(buf) + 1;
}

ssize_t fg_verify_digest_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int i = 0;
	u8 random[RANDOM_CHALLENGE_LEN_MAX] = {0};
	char kbuf[70] = {0};
	struct timespec time;
	static bool once_flag = false;

	if (!once_flag) {
		once_flag = true;
		get_monotonic_boottime(&time);
		if (time.tv_sec <= 40 && !bq->gauge_wakelock->active) {
			fg_info("fg verify, set a flag to block iic rw\n");
			bq->rw_lock = true;
			__pm_stay_awake(bq->gauge_wakelock);
			schedule_delayed_work(&bq->clear_rw_lock_work, msecs_to_jiffies((45 - time.tv_sec) * 1000));
		}
	}

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);
	fg_stringtohex(kbuf, random, &i);
	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B)
		fg_sha256_auth(bq, random, RANDOM_CHALLENGE_LEN_BQ27Z561);
	else if (bq->device_name == BQ_FG_BQ28Z610)
		fg_sha256_auth(bq, random, RANDOM_CHALLENGE_LEN_BQ28Z610);

	return count;
}

static ssize_t fg_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	fg_info("%s show log_level = %d\n", bq->log_tag, log_level);

	return ret;
}

static ssize_t fg_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	fg_info("%s store log_level = %d\n", bq->log_tag, log_level);

	return count;
}

static DEVICE_ATTR(fcc, S_IRUGO, fg_show_fcc, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_show_rm, NULL);
static DEVICE_ATTR(rsoc, S_IRUGO, fg_show_rsoc, NULL);
static DEVICE_ATTR(cell0_voltage, S_IRUGO, fg_show_cell0_voltage, NULL);
static DEVICE_ATTR(cell1_voltage, S_IRUGO, fg_show_cell1_voltage, NULL);
static DEVICE_ATTR(qmax0, S_IRUGO, fg_show_qmax0, NULL);
static DEVICE_ATTR(qmax1, S_IRUGO, fg_show_qmax1, NULL);
static DEVICE_ATTR(trmq, S_IRUGO, fg_show_trmq, NULL);
static DEVICE_ATTR(tfcc, S_IRUGO, fg_show_tfcc, NULL);
static DEVICE_ATTR(verify_digest, S_IRUGO | S_IWUSR, fg_verify_digest_show, fg_verify_digest_store);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, fg_show_log_level, fg_store_log_level);

static struct attribute *fg_attributes[] = {
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_cell0_voltage.attr,
	&dev_attr_cell1_voltage.attr,
	&dev_attr_qmax0.attr,
	&dev_attr_qmax1.attr,
	&dev_attr_trmq.attr,
	&dev_attr_tfcc.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_log_level.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};

static int fg_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret = 0, size = 0;

	bq->enable_shutdown_delay = of_property_read_bool(node, "enable_shutdown_delay");

        if (product_name == RUBY ) {
          bq->slave_connect_gpio = of_get_named_gpio(node, "slave_connect_gpio", 0);
          if (!gpio_is_valid(bq->slave_connect_gpio)) {
                 fg_info("failed to parse slave_connect_gpio\n");
                 return -1;
          }
        }
	if (product_name == RUBY || product_name == RUBYPRO) {
		ret = of_property_read_u32(node, "normal_shutdown_vbat_1s", &bq->normal_shutdown_vbat);
		if (ret)
			fg_err("%s failed to parse normal_shutdown_vbat_1s\n", bq->log_tag);

		ret = of_property_read_u32(node, "critical_shutdown_vbat_1s", &bq->critical_shutdown_vbat);
		if (ret)
			fg_err("%s failed to parse critical_shutdown_vbat_1s\n", bq->log_tag);

		ret = of_property_read_u32(node, "cool_critical_shutdown_vbat_1s", &bq->cool_critical_shutdown_vbat);
		if (ret)
			fg_err("%s failed to parse cool_critical_shutdown_vbat_1s\n", bq->log_tag);

		ret = of_property_read_u32(node, "report_full_rsoc_1s", &bq->report_full_rsoc);
		if (ret)
			fg_err("%s failed to parse report_full_rsoc_1s\n", bq->log_tag);
		if (product_name == RUBYPRO)
			bq->report_full_rsoc = 9500;

		ret = of_property_read_u32(node, "soc_gap_1s", &bq->soc_gap);
		if (ret)
			fg_err("%s failed to parse soc_gap_1s\n", bq->log_tag);
	} else {
		ret = of_property_read_u32(node, "normal_shutdown_vbat_2s", &bq->normal_shutdown_vbat);
		if (ret)
			fg_err("%s failed to parse normal_shutdown_vbat_2s\n", bq->log_tag);

		ret = of_property_read_u32(node, "critical_shutdown_vbat_2s", &bq->critical_shutdown_vbat);
		if (ret)
			fg_err("%s failed to parse critical_shutdown_vbat_2s\n", bq->log_tag);

		ret = of_property_read_u32(node, "cool_critical_shutdown_vbat_2s", &bq->cool_critical_shutdown_vbat);
		if (ret)
			fg_err("%s failed to parse cool_critical_shutdown_vbat_2s\n", bq->log_tag);

		ret = of_property_read_u32(node, "report_full_rsoc_2s", &bq->report_full_rsoc);
		if (ret)
			fg_err("%s failed to parse report_full_rsoc_2s\n", bq->log_tag);

		ret = of_property_read_u32(node, "soc_gap_2s", &bq->soc_gap);
		if (ret)
			fg_err("%s failed to parse soc_gap_2s\n", bq->log_tag);
	}

	of_get_property(node, "soc_decimal_rate", &size);
	if (size) {
		bq->dec_rate_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->dec_rate_seq) {
			bq->dec_rate_len =
				(size / sizeof(*bq->dec_rate_seq));
			if (bq->dec_rate_len % 2) {
				fg_err("%s invalid soc decimal rate seq\n", bq->log_tag);
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"soc_decimal_rate",
					bq->dec_rate_seq,
					bq->dec_rate_len);
		} else {
			fg_err("%s error allocating memory for dec_rate_seq\n", bq->log_tag);
		}
	}

	return ret;
}

static int fg_check_device(struct bq_fg_chip *bq)
{
	u8 data[32];
	int ret = 0, i = 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_NAME, data, 32);
	if (ret)
		fg_info("failed to get FG_MAC_CMD_DEVICE_NAME\n");

	for (i = 0; i < 8; i++) {
		if (data[i] >= 'A' && data[i] <= 'Z')
			data[i] += 32;
	}

	fg_info("parse device_name: %s\n", data);
	if (!strncmp(data, "m16u@bp4H#", 10)) {
		//TODO PLUS
	} else if(!strncmp(data, "m16u@bp4J#", 10)){
		//TODO PRO
	} else if(!strncmp(data, "m16@bp4k#", 10)){
		//TODO
	} else {
		bq->device_name = BQ_FG_UNKNOWN;
		strcpy(bq->model_name, "UNKNOWN");
		strcpy(bq->log_tag, "[XMCHG_UNKNOWN_FG]");
		strcpy(bq->device_chem, "UNKNOWN");
		bq->chip_ok = false;
		bq->adapting_power = 10;
		return ret;
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, 32);
	if (ret)
		fg_info("failed to get FG_MAC_CMD_MANU_NAME\n");
	else
	      fg_info("parse manu_name: %s\n", data);

	if (!strncmp(data, "MI", 2)){
		switch (data[2]){
			case '4':
				bq->device_name = BQ_FG_NFG1000B;
				strcpy(bq->model_name, "nfg1000b");
				strcpy(bq->log_tag, "[XMCHG_NFG1000B]");
				break;
			case '5':
				bq->device_name = BQ_FG_NFG1000A;
				strcpy(bq->model_name, "nfg1000a");
				strcpy(bq->log_tag, "[XMCHG_NFG1000A]");
				break;
			case 'C':
				bq->device_name = BQ_FG_BQ27Z561;
				strcpy(bq->model_name, "bq27z561");
				strcpy(bq->log_tag, "[XMCHG_BQ27Z561]");
				break;
			case 'D':
				bq->device_name = BQ_FG_UNKNOWN;
				strcpy(bq->model_name, "bq30z55");
				strcpy(bq->log_tag, "[XMCHG_BQ30Z55]");
				break;
			case 'E':
				bq->device_name = BQ_FG_UNKNOWN;
				strcpy(bq->model_name, "bq40z50");
				strcpy(bq->log_tag, "[XMCHG_BQ40Z50]");
				break;
			case 'F':
				bq->device_name = BQ_FG_UNKNOWN;
				strcpy(bq->model_name, "bq27z746");
				strcpy(bq->log_tag, "[XMCHG_BQ27Z746]");
				break;
			case 'G':
				bq->device_name = BQ_FG_BQ28Z610;
				strcpy(bq->model_name, "bq28z610");
				strcpy(bq->log_tag, "[XMCHG_BQ28Z610]");
				break;
			case 'H':
				bq->device_name = BQ_FG_UNKNOWN;
				strcpy(bq->model_name, "max1789");
				strcpy(bq->log_tag, "[XMCHG_MAX1789]");
				break;
			case 'I':
				bq->device_name = BQ_FG_UNKNOWN;
				strcpy(bq->model_name, "raa241200");
				strcpy(bq->log_tag, "[XMCHG_RAA241200]");
				break;
			default:
				bq->device_name = BQ_FG_UNKNOWN;
				strcpy(bq->model_name, "UNKNOWN");
				strcpy(bq->log_tag, "[XMCHG_UNKNOWN_FG]");
				break;
		}
		switch (data[4]){
			case '0': bq->adapting_power = 10; break;
			case '1': bq->adapting_power = 15; break;
			case '2': bq->adapting_power = 18; break;
			case '3': bq->adapting_power = 25; break;
			case '4': bq->adapting_power = 33; break;
			case '5': bq->adapting_power = 35; break;
			case '6': bq->adapting_power = 40; break;
			case '7': bq->adapting_power = 55; break;
			case '8': bq->adapting_power = 60; break;
			case '9': bq->adapting_power = 67; break;
			case 'A': bq->adapting_power = 80; break;
			case 'B': bq->adapting_power = 90; break;
			case 'C': bq->adapting_power = 100; break;
			case 'D': bq->adapting_power = 120; break;
			case 'E': bq->adapting_power = 140; break;
			case 'F': bq->adapting_power = 160; break;
			case 'G': bq->adapting_power = 180; break;
			case 'H': bq->adapting_power = 200; break;
			case 'I': bq->adapting_power = 220; break;
			case 'J': bq->adapting_power = 240; break;
			default: bq->adapting_power = 10; break;
		}
		fg_info("manufacturer data2[%c]--device_name[%d],data4[%c]--adapting_power[%d]\n", data[2], bq->device_name, data[4], bq->adapting_power);
		bq->chip_ok = true;
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_CHEM, data, 32);
	if (ret)
		fg_info("failed to get FG_MAC_CMD_DEVICE_CHEM\n");

	if (!strncmp(&data[1], "L", 1) && bq->device_name != BQ_FG_UNKNOWN)
		strcpy(bq->device_chem, "LWN");
	else if(!strncmp(&data[1], "F", 1) && bq->device_name != BQ_FG_UNKNOWN)
		strcpy(bq->device_chem, "ATL");
	else
		strcpy(bq->device_chem, "UNKNOWN");

	return ret;
}

static void pdm_parse_cmdline(void)
{
	char *ruby = NULL, *rubypro = NULL, *rubyplus = NULL;
	const char *sku = get_hw_sku();

	ruby = strnstr(sku, "ruby", strlen(sku));
	rubypro = strnstr(sku, "rubypro", strlen(sku));
	rubyplus = strnstr(sku, "rubyplus", strlen(sku));

	if (rubyplus)
		product_name = RUBYPLUS;
	else if (rubypro)
		product_name = RUBYPRO;
	else if (ruby)
		product_name = RUBY;

	fg_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static int fg_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq_fg_chip *bq;
	int ret = 0;

	fg_info("FG probe start\n");

	pdm_parse_cmdline();

	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_DMA);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	bq->monitor_delay = FG_MONITOR_DELAY_8S;

	memcpy(bq->regs, bq_fg_regs, NUM_REGS);

	i2c_set_clientdata(client, bq);

	bq->shutdown_mode = false;
	bq->shutdown_flag = false;
	bq->rw_lock = false;
	bq->fake_cycle_count = 0;
	bq->dc = 0;
	bq->raw_soc = -ENODATA;
	bq->last_soc = -EINVAL;
	bq->i2c_error_count = 0;
	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	bq->gauge_wakelock = wakeup_source_register(NULL, "gauge_wakelock");

	fg_check_device(bq);

	ret = fg_parse_dt(bq);
	if (ret) {
		fg_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	fg_update_status(bq);

	ret = fg_register_psy(bq);
	if (ret) {
		fg_err("%s failed to register psy\n", bq->log_tag);
		return ret;
	}

	INIT_DELAYED_WORK(&bq->clear_rw_lock_work, fg_clear_rw_lock);

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret) {
		fg_err("%s failed to register sysfs\n", bq->log_tag);
		return ret;
	}

	bq->update_now = true;
	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(5000));
	bq->dc = fg_read_dc(bq);
	fg_info("%s FG probe success\n", bq->log_tag);

	return 0;
}

static int fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bq->clear_rw_lock_work);
	cancel_delayed_work_sync(&bq->monitor_work);

	return 0;
}

static int fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	schedule_delayed_work(&bq->monitor_work, 0);

	return 0;
}

static int fg_remove(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	power_supply_unregister(bq->fg_psy);
	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);
	sysfs_remove_group(&bq->dev->kobj, &fg_attr_group);

	return 0;
}

static void fg_shutdown(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	fg_info("%s bq fuel gauge driver shutdown!\n", bq->log_tag);
}

static struct of_device_id fg_match_table[] = {
	{.compatible = "bq28z610",},
	{},
};
MODULE_DEVICE_TABLE(of, fg_match_table);

static const struct i2c_device_id fg_id[] = {
	{ "bq28z610", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, fg_id);

static const struct dev_pm_ops fg_pm_ops = {
	.resume		= fg_resume,
	.suspend	= fg_suspend,
};

static struct i2c_driver fg_driver = {
	.driver	= {
		.name   = "bq28z610",
		.owner  = THIS_MODULE,
		.of_match_table = fg_match_table,
		.pm     = &fg_pm_ops,
	},
	.id_table       = fg_id,

	.probe          = fg_probe,
	.remove		= fg_remove,
	.shutdown	= fg_shutdown,
};

module_i2c_driver(fg_driver);

MODULE_DESCRIPTION("TI GAUGE Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
