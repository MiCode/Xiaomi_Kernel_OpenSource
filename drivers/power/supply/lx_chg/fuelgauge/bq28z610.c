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
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "bq28z610.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "../common/lx_voter.h"
#endif
#include "charger_partition.h"
#include "../common/lx_notify.h"
#include <linux/hardware_info.h>

enum product_name {
	XAGA_NO,
	XAGA,
	XAGAPRO,
	DAUMIER,
};

static int log_level = 1;
static int product_name = XAGA_NO;
static ktime_t time_init = -1;
#define FGTAG                     "[LX_CHG][FG]"
#define fg_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR FGTAG ":%s:" fmt, __func__, ##__VA_ARGS__);	\
} while (0)

#define fg_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_INFO FGTAG ":%s:" fmt, __func__, ##__VA_ARGS__);	\
} while (0)

#define fg_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR FGTAG ":%s:" fmt, __func__, ##__VA_ARGS__);	\
} while (0)

static struct regmap_config fg_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret = 0;

	ret =  i2c_smbus_read_byte_data(client, reg);
	if(ret < 0)
	{
		fg_info("i2c read byte failed: can't read from reg 0x%02X faild\n", reg);
		return ret;
	}

	*val = (u8)ret;

	return 0;
}

static int fg_read_byte(struct bq_fg_chip *bq, u8 reg, u8 *val)
{
	int ret;
	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_read_word(struct bq_fg_chip *bq, u8 reg, u16 *val)
{
	u8 data[2] = {0, 0};
	int ret = 0;

	if(atomic_read(&bq->fg_in_sleep))
	{
		fg_err("%s in sleep\n", __func__);
		return -EINVAL;
	}

	ret = regmap_raw_read(bq->regmap, reg, data, 2);
	if (ret) {
		fg_info("%s I2C failed to read 0x%02x\n", bq->log_tag, reg);
		return ret;
	}

	*val = (data[1] << 8) | data[0];
	return ret;
}

static int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len)
{
	int ret = 0;

	if(atomic_read(&bq->fg_in_sleep))
	{
		fg_err("%s in sleep\n", __func__);
		return -EINVAL;
	}

	ret = regmap_bulk_read(bq->regmap, reg, buf, len);
	if (ret) {
		fg_info("%s I2C failed to read block\n", bq->log_tag);
		return ret;
	}

	return ret;
}

static int fg_write_block(struct bq_fg_chip *bq, u8 reg, u8 *data, u8 len)
{
	int ret = 0;

	if(atomic_read(&bq->fg_in_sleep))
	{
		fg_err("%s in sleep\n", __func__);
		return -EINVAL;
	}

	ret = regmap_bulk_write(bq->regmap, reg, data, len);
	if (ret) {
		fg_info("%s I2C failed to write block\n", bq->log_tag);
		return ret;
	}

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
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;

	msleep(4);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0)
		return ret;

	cksum = t_buf[34];
	t_len = t_buf[35];

	if (t_len <= 3) {
		fg_err("%s len is less invaild, force vaild\n", bq->log_tag);
		t_len = 3;
	}
	if (t_len >= 42) {
		fg_err("%s len is over invaild, force vaild\n", bq->log_tag);
		t_len = 42;
	}

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

	cksum = fg_checksum(t_buf, len + 2);
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
	ret = regmap_write(bq->regmap, bq->regs[BQ_FG_REG_MAC_CHKSUM], cksum_calc);
	if (ret < 0)
		return ret;

	/*5. Write the length to address 0x61.*/
	ret = regmap_write(bq->regmap, bq->regs[BQ_FG_REG_MAC_DATA_LEN], length + 4);
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
	static bool pre_batt_fc = 0;

	if (bq->i2c_err_flag) {
		bq->batt_fc = 0;
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
	if (ret < 0) {
		fg_err("read battery status fail");
		bq->batt_fc = pre_batt_fc;
		return ret;
	}

	bq->batt_fc = !!(flags & BIT(5));
	pre_batt_fc = bq->batt_fc;
	return 0;
}

static int fg_read_rsoc(struct bq_fg_chip *bq)
{
	u16 soc = 0;
	int ret = 0;
	static u16 pre_soc = 0;

	if (bq->i2c_err_flag) {
		return 15;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		soc = pre_soc;
		fg_err("failed to read RSOC, soc:%d\n", soc);
	}

	pre_soc = soc;
	return soc;
}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	u16 tbat = 0;
	int ret = 0;
	static u16 pre_tbat = 0;

	if (bq->fake_tbat != -FAKE_TBAT_NODATA)
		return bq->fake_tbat;

	if (bq->i2c_err_flag) {
		return 250;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &tbat);
	if (ret < 0) {
		tbat = pre_tbat;
		fg_err("failed to read TBAT,tbat:%d\n", tbat);
	}

	fg_dbg("read FG TBAT = %d\n", tbat);
	if (!tbat || tbat >= 3730)
		tbat = 2980;

	if (tbat - 2730 > 1000) {
		fg_err("abnormal temp = %d,return pre_tbat = %d\n", tbat, pre_tbat);
		tbat = pre_tbat;
	}

	pre_tbat = tbat;
	return tbat - 2730;
}

__maybe_unused
static void fg_read_cell_voltage(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	int ret = 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 32);
	if (ret) {
		fg_err("%s failed to read cell voltage\n", bq->log_tag);
		bq->cell_voltage[0] = 4000;
		bq->cell_voltage[1] = 4000;
	} else {
		bq->cell_voltage[0] = (data[1] << 8) | data[0];
		bq->cell_voltage[1] = (data[3] << 8) | data[2];
	}

	bq->cell_voltage[2] = 2 * max(bq->cell_voltage[0], bq->cell_voltage[1]);
}

static int fg_read_volt(struct bq_fg_chip *bq)
{
	u16 vbat = 0;
	int ret = 0;

	if (bq->i2c_err_flag) {
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &vbat);
	if (ret < 0) {
		vbat = bq->vbat;
		fg_err("failed to read VBAT:%d bq-VBAT:%d\n", vbat,  bq->vbat);
	}

	if (vbat > 10000 || vbat < 1800) {
		fg_err("vbat:%d < 1800 or > 10000, set vbat to 4000\n", vbat);
		vbat = 4000;
	}

	bq->vbat = (int)vbat;
	bq->cell_voltage[0] = bq->cell_voltage[1] = bq->cell_voltage[2] = bq->vbat;

	return ret;
}

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
static int fuelguage_check_i2c_function(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int ret = 0;

	ret = fg_read_volt(bq);
	return ret;
}
#endif

static int fg_read_avg_current(struct bq_fg_chip *bq)
{
	s16 avg_ibat = 0;
	int ret = 0;
	static s16 pre_avg_ibat = 0;

	if (bq->i2c_err_flag) {
		return -500;//-500mA
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], (u16 *)&avg_ibat);
	if (ret < 0) {
		fg_err("failed to read pre_avg_ibat:%d\n", pre_avg_ibat);
		avg_ibat = pre_avg_ibat;
	}
	pre_avg_ibat = avg_ibat;
	return avg_ibat;
}

static int fg_read_current(struct bq_fg_chip *bq)
{
	s16 ibat = 0;
	int ret = 0;
	static s16 pre_ibat = 0;

	if (bq->i2c_err_flag) {
		return -500;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CN], (u16 *)&ibat);
	if (ret < 0) {
		ibat = pre_ibat;//-500mA
		fg_err("%s failed to read IBAT ibat:%d\n", bq->log_tag, ibat);
	}
	fg_dbg("%s:ibat:%d\n", bq->log_tag, ibat);
	pre_ibat = ibat;
	return ibat;
}

static int fg_read_fcc(struct bq_fg_chip *bq)
{
	u16 fcc = 0;
	int ret = 0;
	static u16 pre_fcc = 0;

	if (bq->i2c_err_flag) {
		bq->fcc = 7000;
		return 7000;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);
	if (ret < 0) {
		fcc = pre_fcc;
		fg_err("%s failed to read FCC,FCC=%d\n", bq->log_tag, fcc);
	}
	pre_fcc = fcc;
	bq->fcc = fcc;
	return fcc;
}

static int fg_read_rm(struct bq_fg_chip *bq)
{
	u16 rm = 0;
	int ret = 0;
	static u16 pre_rm = 0;

	if (bq->i2c_err_flag) {
		bq->rm = 3500;
		return 3500;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);
	if (ret < 0) {
		rm = pre_rm;
		fg_err("%s failed to read RM,RM=%d\n", bq->log_tag, rm);
	}

	pre_rm = rm;
	bq->rm = rm;
	return rm;
}

static int fg_read_dc(struct bq_fg_chip *bq)
{
	u16 dc = 0;
	int ret = 0;
	static u16 pre_dc = 0;

	if (bq->i2c_err_flag) {
		return 7000;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_DC], &dc);
	if (ret < 0) {
		dc = pre_dc;
		fg_err("%s failed to read DC:%d\n", bq->log_tag, dc);
	}

	pre_dc = dc;
	return dc;
}

static int fg_read_soh(struct bq_fg_chip *bq)
{
	u16 soh = 0;
	int ret = 0;
	static u16 pre_soh = 0;

	if (bq->i2c_err_flag) {
		return 50;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		soh = pre_soh;
		fg_err("%s failed to read SOH:%d\n", bq->log_tag, soh);
	}

	pre_soh = soh;
	return soh;
}

static int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	u16 cc = 0;
	int ret = 0;
	static u16 pre_cc = 0;

	if(bq->fake_cycle_count > 0)
	{
		return bq->fake_cycle_count;
	}

	if (bq->i2c_err_flag) {
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);
	if (ret < 0) {
		fg_err("%s failed to read CC\n", bq->log_tag);
		cc = pre_cc;
	}
	pre_cc = cc;
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

static bool fuel_guage_get_chip_ok(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	return bq->chip_ok;
}

static int fuel_guage_get_resistance_id(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	if(bq)
		return 100000;
	else
		return 0;
}

static int fuel_guage_get_battery_id(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	fg_dbg("%s:bq->device_chem:%s, battery_id:%d\n", __func__, bq->device_chem, bq->cell_supplier);
	return bq->cell_supplier;
}

static int fuel_guage_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
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

static int fuel_guage_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int rsoc, raw_soc;

	if (!bq)
		return 0;

	if (bq->i2c_err_flag)
		return 0;

	rsoc = fg_read_rsoc(bq);
	raw_soc = fg_get_raw_soc(bq);

	if (bq->ui_soc > rsoc)
		return 0;

	return raw_soc % 100;
}

static int fg_get_soc_decimal_rate(struct bq_fg_chip *bq)
{
	int soc, i;

	if (bq->dec_rate_len <= 0)
		return 0;

	if (bq->i2c_err_flag)
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

	if (bq->i2c_err_flag)
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

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B) {
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

static int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	u8 data[5] = {0};
	int ret = 0;

	if (bq->fast_chg == enable)
		return ret;
	else
		data[0] = bq->fast_chg = enable;

	if (bq->device_name == BQ_FG_BQ28Z610)
		return ret;

	if (enable) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_EN, data, 2);
		fg_info("%s write 3e fastcharge = %d success\n", bq->log_tag, ret);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		fg_info("%s write 3f fastcharge = %d success\n", bq->log_tag, ret);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	}

	return ret;
}

static int fg_set_charger_to_full(struct bq_fg_chip* bq)
{
	u8 data[5] = { 1 };
	int ret = 0;

	if (bq->device_name == BQ_FG_BQ28Z610)
		return ret;
	if (bq->full_count == 0){
		bq->full_count++;
		bq->rsoc_smooth = bq->raw_soc;
		ret = fg_mac_write_block(bq, FG_MAC_CMD_TO_FULL, data, 2);
		if (ret) {
			fg_err("%s failed to write 31 charger done to full = %d\n", bq->log_tag, ret);
			return ret;
		} else {
			fg_info("%s write 31 charger done to full = %d %d success\n", bq->log_tag, bq->full_count, ret);
		}
	}

	bq->raw_soc = 10000;//R_SOC set 100%

	return ret;
}

static int fuelguage_set_charger_to_full(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	return fg_set_charger_to_full(bq);
}

static int fuelguage_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool en)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int ret = 0;

	ret = fg_set_fastcharge_mode(bq, en);
	return ret;
}

static int fuelguage_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	return bq->fast_chg;
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

struct ffc_smooth {
	int curr_lim;
	int time;
};

struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    50000},
	{-300,  20000},
	{-600,   15000},
	{-1000,  10000},
};

static int bq_battery_soc_smooth_tracking_sencond(struct bq_fg_chip *bq,
	int raw_soc, int batt_soc, int soc)
{
	static ktime_t changed_time = -1;
	int unit_time = 0, delta_time = 0;
	int change_delta = 0;
	int soc_changed = 0;
	int status = 0;

	if (bq->tbat < 150) {
		bq->monitor_delay = FG_MONITOR_DELAY_5S;
	}
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
			status = bq->bq_charging_status;

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
	int change_delta = 0;
	int optimiz_delta = 0, status;
	static ktime_t last_change_time;
	static ktime_t last_optimiz_time;
	int unit_time = 0;
	int soc_changed = 0, delta_time = 0;
	static int optimiz_soc, last_raw_soc;
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
		status = bq->bq_charging_status;
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

	if (bq->fast_chg && raw_soc >= bq->report_full_rsoc && raw_soc != FG_RAW_SOC_FULL) {
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

static int bq_battery_soc_smooth_tracking_new(struct bq_fg_chip *bq, int raw_soc, int batt_rsoc, int batt_ma)
{
	static int system_soc, last_system_soc;
	int soc_changed = 0, unit_time = 10000, delta_time = 0, soc_delta = 0;
	int sub_time = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0;
	int charging_status, i=0, batt_ma_avg = 0;
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	static bool is_full_flag = false;
	struct votable	*is_full_votable = NULL;
#endif
	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if((batt_ma < 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if(batt_ma >= 0)
		ibat_pos_count = 0;

	charging_status = bq->bq_charging_status;

	if (bq->tbat < 150) {
		bq->monitor_delay = FG_MONITOR_DELAY_3S;
	}
	if (!raw_soc) {
		bq->monitor_delay = FG_MONITOR_DELAY_10S;
	}
	batt_ma_avg = fg_read_avg_current(bq);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	is_full_votable = find_votable("IS_FULL");
	if (!is_full_votable) {
		fg_err("smooth_new1:failed to get is_full_votable\n");
	} else {
		if (charging_status == POWER_SUPPLY_STATUS_FULL) {
			fg_info("smooth_new2: report full, is_full_flag = %d\n", is_full_flag);
			is_full_flag = true;
			vote(is_full_votable, SMOOTH_NEW_VOTER, true, 0);
		}
		if (batt_ma_avg <= -10 || charging_status == POWER_SUPPLY_STATUS_DISCHARGING)
			bq->rsoc_smooth = 0;
		if (bq->rsoc_smooth <= 9800 && bq->rsoc_smooth) {
			is_full_flag = false;
			raw_soc = bq->rsoc_smooth;
			bq->rsoc_smooth = bq->rsoc_smooth + 100;
		} else {
			bq->rsoc_smooth = 0;
		}
		if (is_full_flag && (raw_soc >= 9700)) {
			fg_info("smooth_new3: report full, old raw_soc = %d\n", raw_soc);
			raw_soc = 100000;
			fg_info("smooth_new4: report full, new raw_soc = %d\n", raw_soc);
		} else {
			is_full_flag = false;
			vote(is_full_votable, SMOOTH_NEW_VOTER, false, 0);
		}
		fg_dbg("smooth_new5:is_full_flag = %d\n", is_full_flag);
	}
#endif
	/*Map system_soc value according to raw_soc */
	if(raw_soc >= bq->report_full_rsoc) {
		system_soc = 100;
	} else if (bq->max_chg_power_120w || product_name == XAGAPRO) {
		system_soc = ((raw_soc + 94) / 95);
		if(system_soc > 99)
			system_soc = 99;
	} else if (batt_rsoc == 0 && (charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING) && last_system_soc > 0) {
		calc_delta_time(last_change_time, &change_delta);
		sub_time = change_delta / 1000;
		if (sub_time < 0) {
			last_change_time = ktime_get();
			sub_time = 0;
		}
		if (sub_time >= 20) {
			system_soc = last_system_soc - 1;
		}
		fg_info("smooth_new6:sub_time= %d\n", sub_time);
	} else {
		system_soc = ((raw_soc + 97) / 98);
		if(system_soc > 99)
			system_soc = 99;
    }

    fg_info("smooth_new7: fisrt step, system_soc:%d, last_system_soc:%d, raw_soc:%d, batt_rsoc:%d\n",
										system_soc, last_system_soc, raw_soc, batt_rsoc);
	/*Get the initial value for the first time */
	if (last_change_time == -1) {
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = batt_rsoc;
	}

	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING ) &&
		!bq->rm && bq->tbat < 150 && last_system_soc >= 1) {
		for (i = FFC_SMOOTH_LEN-1; i >= 0; i--) {
			if (batt_ma_avg < ffc_dischg_smooth[i].curr_lim) {
				unit_time = ffc_dischg_smooth[i].time;
				break;
			}
		}
		fg_info("smooth_new8:enter low temperature smooth unit_time=%d batt_ma_avg=%d\n", unit_time, batt_ma_avg);
	}

	/*If the soc jump, will smooth one cap every 10S */
	soc_delta = abs(system_soc - last_system_soc);
	if (soc_delta > 1 || (bq->vbat < 3300 && system_soc > 0) || (unit_time != 10000 && soc_delta == 1)) {
		//unit_time != 10000 && soc_delta == 1 fix low temperature 2% jump to 0%
		calc_delta_time(last_change_time, &change_delta);
		delta_time = change_delta / unit_time;
		if (delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}
		fg_info("smooth_new9:delta_time=%d\n", delta_time);
		soc_changed = min(1, delta_time);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		if (is_full_flag) {
			soc_changed = 0;
		}
#endif
		fg_info("smooth_new10:soc_changed=%d\n", soc_changed);
		if (soc_changed) {
			if(charging_status == POWER_SUPPLY_STATUS_CHARGING && system_soc > last_system_soc)
				system_soc = last_system_soc + soc_changed;
			else if(charging_status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc)
				system_soc = last_system_soc - soc_changed;
		} else {
			system_soc = last_system_soc;
		}
		fg_info("smooth_new11:system_soc:%d, last_system_soc:%d\n", system_soc, last_system_soc);
	}

	if(system_soc < last_system_soc)
		system_soc = last_system_soc - 1;
	/*Avoid mismatches between charging status and soc changes  */
	if (((charging_status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc)) || ((charging_status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;
	fg_info("smooth_new12:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_avg=%d\n" ,
		system_soc, last_system_soc, soc_delta, charging_status, unit_time, batt_ma_avg);

	if (system_soc != last_system_soc) {
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}
	if(system_soc > 100)
		system_soc =100;
	if(system_soc < 0)
		system_soc =0;

	if ((system_soc == 0) && ((bq->vbat >= 3100) || ((time.tv_sec <= 10)))) {
		system_soc = 1;
		fg_err("smooth_new13:uisoc::hold 1 when volt > 3100mv. \n");
	}

	if(bq->last_soc != system_soc){
		bq->last_soc = system_soc;
	}

	return system_soc;
}

static void calculate_average_current(struct bq_fg_chip *bq)
{
	int i;
	int iavg_ma = bq->param.batt_ma;
	/* only continue if ibat has changed */
	if (bq->param.batt_ma == bq->param.batt_ma_prev)
		goto unchanged;
	else
		bq->param.batt_ma_prev = bq->param.batt_ma;
	bq->param.batt_ma_avg_samples[bq->param.samples_index] = iavg_ma;
	bq->param.samples_index = (bq->param.samples_index + 1) % BATT_MA_AVG_SAMPLES;
	bq->param.samples_num++;
	if (bq->param.samples_num >= BATT_MA_AVG_SAMPLES)
		bq->param.samples_num = BATT_MA_AVG_SAMPLES;
	if (bq->param.samples_num) {
		iavg_ma = 0;
		/* maintain a AVG_SAMPLES sample average of ibat */
		for (i = 0; i < bq->param.samples_num; i++) {
			fg_dbg("iavg_samples_ma[%d] = %d\n", i, bq->param.batt_ma_avg_samples[i]);
			iavg_ma += bq->param.batt_ma_avg_samples[i];
		}
		bq->param.batt_ma_avg = DIV_ROUND_CLOSEST(iavg_ma, bq->param.samples_num);
	}
unchanged:
	fg_info("current_now_ma = %d, averaged_iavg_ma = %d\n",
			bq->param.batt_ma, bq->param.batt_ma_avg);
}

static int calculate_delta_time(ktime_t *time_stamp, int *delta_time_s)
{
	ktime_t now_time;
	/* default to delta time = 0 if anything fails */
	*delta_time_s = 0;
	//now_time = ktime_get_seconds();
	now_time = ktime_get_boottime();
	*delta_time_s = ktime_ms_delta(now_time, *time_stamp) / 1000;
	if(*delta_time_s < 0)
		*delta_time_s = 0;
	/* remember this time */
	*time_stamp = now_time;
	return 0;
}

#define LOW_TBAT_THRESHOLD		150
#define CHANGE_SOC_TIME_LIMIT_10S	10
#define CHANGE_SOC_TIME_LIMIT_20S	20
#define CHANGE_SOC_TIME_LIMIT_60S	60
#define HEAVY_DISCHARGE_CURRENT		-1000
#define FORCE_TO_FULL_SOC		95
#define MIN_DISCHARGE_CURRENT		(-25)
#define MIN_CHARGING_CURRENT		25
#define FULL_SOC			100
#define FORCE_TO_FULL_RAW_SOC_EEA 	99
static int bq_battery_soc_smooth_tracking_eea(struct bq_fg_chip *bq)
{
	static ktime_t last_change_time;
	int delta_time = 0;
	int soc_changed;
	int last_batt_soc = bq->param.batt_soc;
	int time_since_last_change_sec = 0;
	static int raw_soc = 0;
	struct power_supply *bat_psy = NULL;

	bat_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(bat_psy)) {
		fg_err("eea 1.0 get battery psy fail\n");
		bat_psy = power_supply_get_by_name("battery");
	}

	raw_soc = bq->param.batt_raw_soc * 100;
	last_change_time = bq->param.last_soc_change_time;
	calculate_delta_time(&last_change_time, &time_since_last_change_sec);
	if (bq->param.batt_temp > LOW_TBAT_THRESHOLD) {
		/* Battery in normal temperture */
		if (bq->param.batt_ma > 0 || abs(bq->param.batt_raw_soc - bq->param.batt_soc) > 2)
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_20S;
		else
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_60S;
	} else {
		/* Battery in low temperture */
		calculate_average_current(bq);
		/* Calculated average current > 1000mA */
		if (bq->param.batt_ma_avg < HEAVY_DISCHARGE_CURRENT || abs(bq->param.batt_raw_soc - bq->param.batt_soc > 2))
			/* Heavy loading current, ignore battery soc limit*/
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_10S;
		else
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_20S;
	}

	if (delta_time < 0)
		delta_time = 0;

	soc_changed = min(1, delta_time);
	if (raw_soc <= 10000)
		last_batt_soc = (raw_soc + 100) / 101;

	fg_info("eea 2.0 smooth_new:soc:%d, last_soc:%d, raw_soc:%d, soc_changed:%d, update_now:%d, charge_status:%d, batt_ma:%d\n",
			bq->param.batt_soc, last_batt_soc, raw_soc, soc_changed, bq->param.update_now,
			bq->param.batt_status, bq->param.batt_ma);

	if (last_batt_soc >= 0) {
		if (bq->param.batt_status == POWER_SUPPLY_STATUS_FULL) {
		//	if (last_batt_soc != FULL_SOC && bq->param.batt_raw_soc >= FORCE_TO_FULL_RAW_SOC_EEA) {
			if (last_batt_soc != FULL_SOC && bq->tbat < 450) {
				/* Unlikely status */
				last_batt_soc = bq->param.update_now ? FULL_SOC : last_batt_soc + soc_changed;
				fg_dbg("eea 2.10 charge full, smooth soc to 100 \n");
			}

			if (last_batt_soc == FULL_SOC && bq->param.batt_raw_soc >= 100) {
				/* Unlikely status */
				last_batt_soc = bq->param.update_now ? FULL_SOC : last_batt_soc;
				fg_dbg("eea 2.11 charge full, keep soc to 100 \n");
			}

			if (last_batt_soc > bq->param.batt_raw_soc && bq->param.batt_ma < 0) { //在full状态下电池放电UI_SOC追RAW_SOC
				last_batt_soc = bq->param.update_now ? bq->param.batt_raw_soc : last_batt_soc - soc_changed;
				fg_dbg("eea 2.12 soc reduce with raw_soc before recharge\n");
			}

		} else if (bq->param.batt_status == POWER_SUPPLY_STATUS_CHARGING) {
			if (last_batt_soc < bq->param.batt_raw_soc) {
				/* Battery in charging status
				* update the soc when resuming device
				*/
				last_batt_soc = bq->param.update_now ? bq->param.batt_raw_soc : last_batt_soc + soc_changed;
			} else if(bq->param.batt_soc == 99 && bq->param.batt_raw_soc == 100) {
				/* Battery in charging status
				* but cannot update soc to 100
				* keep soc in 99 untill charge termination
				*/
				last_batt_soc = bq->param.batt_soc;
			}
		} else {
			if (last_batt_soc > bq->param.batt_raw_soc) {
				/* Battery in discharging status
				* update the soc when resuming device
				*/
				last_batt_soc = bq->param.update_now ? bq->param.batt_raw_soc : last_batt_soc - soc_changed;
			}
			fg_info("eea 2.13 last_batt_soc:%d\n", last_batt_soc);
		}
		bq->param.update_now = false;
	} else {
		last_batt_soc = bq->param.batt_raw_soc;
	}

	if (last_batt_soc > FULL_SOC)
		last_batt_soc = FULL_SOC;
	else if (last_batt_soc < 0)
		last_batt_soc = 0;

	if (bq->first_flag == true && bq->param.batt_soc == 0 && last_batt_soc > 0) {
		bq->first_flag = false;
		bq->param.batt_soc = last_batt_soc;
		bq->param.last_soc_change_time = last_change_time;
		fg_info("eea 3.0 batt_soc:%d, last_batt_soc:%d\n", bq->param.batt_soc, last_batt_soc);
		if (bat_psy)
			power_supply_changed(bat_psy);
	} else if (bq->param.batt_soc > last_batt_soc && bq->param.batt_ma < 0) {
		bq->param.batt_soc = bq->param.batt_soc - 1;
		bq->param.last_soc_change_time = last_change_time;
		fg_info("eea 4.0 update down soc %d %d\n", bq->param.batt_soc, last_batt_soc);
		if (bat_psy)
			power_supply_changed(bat_psy);
	} else if (bq->param.batt_soc < last_batt_soc && bq->param.batt_ma == 0 &&
					 bq->param.batt_status == POWER_SUPPLY_STATUS_CHARGING) {
		bq->param.batt_soc = bq->param.batt_soc;
		fg_info("eea 5.0 update soc %d %d\n", bq->param.batt_soc, last_batt_soc);
	} else if (bq->param.batt_soc < last_batt_soc  &&
					(bq->param.batt_status == POWER_SUPPLY_STATUS_FULL ||
					bq->param.batt_status == POWER_SUPPLY_STATUS_CHARGING)) {
		bq->param.batt_soc = bq->param.batt_soc + 1;
		bq->param.last_soc_change_time = last_change_time;
		if (bat_psy)
			power_supply_changed(bat_psy);
		fg_info("eea 6.0 update up soc %d %d\n", bq->param.batt_soc, last_batt_soc);
	}

	if (bq->param.batt_soc > 100) {
		bq->param.batt_soc = 100;
	}
	if (bq->param.batt_soc < 0) {
		bq->param.batt_soc = 0;
	}

	return 0;
}

static int fg_set_shutdown_mode(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 data[5] = {0};

	fg_info("%s fg_set_shutdown_mode\n", bq->log_tag);
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
	bq->batt_psy = power_supply_get_by_name("battery");
	if (!bq->batt_psy) {
		fg_err("%s failed to get batt_psy", bq->log_tag);
		return false;
	}
	return true;
}

static void fg_update_status(struct bq_fg_chip *bq)
{
	int temp_soc = 0,  delta_temp = 0;
	static int last_soc = 0, last_temp = 0;
	ktime_t time_now = -1;
	union power_supply_propval pval = {0,};
	int rc;
	#if 0
        struct mtk_battery *gm;
	#endif

	if (bq->i2c_err_flag) {
		bq->cycle_count = 0;
		bq->rsoc = 15;
		bq->ui_soc = 15;
		bq->soh = 50;
		bq->ibat = -500;
		bq->tbat = 250;
		bq->rm = 3500;
		bq->fcc = 7000;
		bq->dc = 7000;
		return;
	}

	mutex_lock(&bq->data_lock);
	bq->cycle_count = fg_read_cyclecount(bq);
	bq->rsoc = fg_read_rsoc(bq);
	bq->soh = fg_read_soh(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	bq->ibat = fg_read_current(bq);
	bq->tbat = fg_read_temperature(bq);
	fg_read_volt(bq);
	fg_read_status(bq);
	mutex_unlock(&bq->data_lock);
	fg_dbg("fg_update rsoc=%d, raw_soc=%d, vbat=%d, cycle_count=%d\n", bq->rsoc, bq->raw_soc, bq->vbat, bq->cycle_count);

	time_now = ktime_get();
	if (time_init != -1 && (time_now - time_init < 10000 )) {
		bq->ui_soc = bq->rsoc;
		goto out;
	}

	if (!battery_get_psy(bq)) {
		fg_err("%s fg_update failed to get battery psy\n", bq->log_tag);
		bq->bq_charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		rc = power_supply_get_property(
			bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
		bq->bq_charging_status = pval.intval;
	}
		if (bq->bq_charging_status != POWER_SUPPLY_STATUS_FULL)
			bq->full_count = 0;
		if (bq->is_eu_mode == true) {
			bq->param.batt_ma = bq->ibat;
			bq->param.batt_temp = bq->tbat;
			bq->param.batt_raw_soc = bq->rsoc;
			bq->param.batt_status = bq->bq_charging_status;
			bq_battery_soc_smooth_tracking_eea(bq);
			bq->ui_soc = bq->param.batt_soc;
		} else {
			bq->ui_soc = bq_battery_soc_smooth_tracking_new(bq, bq->raw_soc, bq->rsoc, bq->ibat);
		}

		if(bq->night_charging && bq->ui_soc > 80 && bq->ui_soc > last_soc){
			bq->ui_soc = last_soc;
			fg_err("%s last_soc = %d, night_charging = %d,\n", __func__, last_soc, bq->night_charging);
		}

		goto out;
		temp_soc = bq_battery_soc_smooth_tracking(bq, bq->raw_soc, bq->rsoc, bq->tbat, bq->ibat);
		bq->ui_soc = bq_battery_soc_smooth_tracking_sencond(bq, bq->raw_soc, bq->rsoc, temp_soc);

out:
		fg_info("%s [FG_STATUS] [UISOC RSOC RAWSOC TEMP_SOC SOH] = [%d %d %d %d %d], [VBAT CELL0 CELL1 IBAT TBAT FC FAST_MODE] = [%d %d %d %d %d %d %d]\n", bq->log_tag,
			bq->ui_soc, bq->rsoc, bq->raw_soc, temp_soc, bq->soh, bq->vbat, bq->cell_voltage[0], bq->cell_voltage[1], bq->ibat, bq->tbat, bq->batt_fc, bq->fast_chg);

		delta_temp = abs(last_temp - bq->tbat);
		if (bq->batt_psy && (last_soc != bq->ui_soc || delta_temp > 5 || bq->ui_soc == 0 || bq->rsoc == 0)) {
			fg_err("%s last_soc = %d, last_temp = %d, delta_temp = %d\n", __func__, last_soc, last_temp, delta_temp);
			power_supply_changed(bq->batt_psy);
		}

		last_soc = bq->ui_soc;
		if (delta_temp > 5)
			last_temp = bq->tbat;
}

static void fuel_gauge_update_status(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	//pr_err("%s fg_update_status in!", __func__);
	fg_update_status(bq);
}

#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)

static int get_count(struct bq_fg_chip *bq, int *count_1000)
{
	u8 buf[32] = {0};
	int ret = 0;
	int count_1, count_2, count_3;
	if(bq->dod_count>0){
		*count_1000 = bq->dod_count*1000;
		return ret;
	}
	// TODO: read count_1 count_2 count_3 from guage
	ret = fg_mac_read_block(bq, 0x2001, buf, 32);
	if(ret){
		msleep(150);
		ret = fg_mac_read_block(bq, 0x2001, buf, 32);
	}
	if(ret){
		return ret;
	}
	// get count1/2/3 from buf
	count_1 = buf[9]<<8 | buf[8];
	count_2 = buf[11]<<8 | buf[10];
	count_3 = buf[13]<<8 | buf[12];
	*count_1000 = (2000*count_1 + 1500*count_2 + 1300*count_3); //(2*count1+1.5*count2+1.3*count3)
  	pr_info("%s count_1=%d, count_2=%d, count_3=%d, count_1000=%d\n", __func__, count_1, count_2, count_3, *count_1000);

	return ret;
}

static int set_termv(struct bq_fg_chip *bq, int termv)
{
	int ret = -EINVAL;
	u8 data[5] = {0};
  	u8 read_back[2] = {0};
	u16 ter_mv = termv;
  	u16 read_mv = 0;
	if(bq->i2c_err_flag){
		return 0;
	}
	data[0] = (u8)ter_mv;
	data[1] = (u8)(ter_mv>>8);
	// TODO: set termv to guage
	ret = fg_mac_write_block(bq, 0x0050, data, 2);
	if(ret){
		msleep(150);
		ret = fg_mac_write_block(bq, 0x0050, data, 2);
	}
	if(!ret){
          	msleep(150);
          	ret = fg_mac_read_block(bq, 0x0050, read_back, 2);
          	if(!ret){
                	read_mv = (read_back[1] << 8) | read_back[0];
                  	pr_err("%s set_termv=%d, read_mv=%d", __func__, ter_mv, read_mv);
                  	if(ter_mv != read_mv){
                        	pr_err("%s Termv written failed!", __func__);
                        } else{
                          	bq->termv = termv;
                        }
                } else{
                	pr_err("%s Failed to read back termv value!", __func__);
                }
	}
	return ret;
}

static void fg_select_poweroff_voltage_config(struct bq_fg_chip *bq)
{
	int ret;
	int count_x1000;
	int poweroff_vol, shutdown_delay_vol, termv;
  	static int report_count = 0;
	struct poweroff_voltage_config conf_count = {0};
	struct poweroff_voltage_config conf_cyclecount = {0};
	ret = get_count(bq, &count_x1000);
	if(!ret){
		select_poweroff_by_count(bq->tbat, count_x1000, &conf_count);
	}
	select_poweroff_by_cyclecount(bq->tbat, bq->cycle_count, &conf_cyclecount);
	// compare the voltage form count and cyclecount
	if(conf_count.shutdown_delay_voltage >= conf_cyclecount.shutdown_delay_voltage ){
		poweroff_vol = conf_count.poweroff_voltage;
		shutdown_delay_vol = conf_count.shutdown_delay_voltage;
		termv = conf_count.termv;
	} else {
		poweroff_vol = conf_cyclecount.poweroff_voltage;
		shutdown_delay_vol = conf_cyclecount.shutdown_delay_voltage;
		termv = conf_cyclecount.termv;
	}
	// set termv
	if(termv!=0 && termv!=bq->termv){
		ret = set_termv(bq, termv);
		if(ret){
			pr_err("%s set_termv fail, ret=%d!\n", __func__, termv);
		}
	}
	if(poweroff_vol!=0 && shutdown_delay_vol!=0) {
		if(bq->poweroff_conf.poweroff_voltage != poweroff_vol \
					|| bq->poweroff_conf.shutdown_delay_voltage != shutdown_delay_vol){
			if( (poweroff_vol>2500&&poweroff_vol<3500) \
						&& (shutdown_delay_vol>2800 && shutdown_delay_vol<3600) ){
				bq->poweroff_conf.poweroff_voltage = poweroff_vol;
				bq->poweroff_conf.shutdown_delay_voltage = shutdown_delay_vol;
				lx_charger_notifier_call_chain(CHARGER_EVENT_SHUTDOWN_VOLTAGE_CHANGED, (void *)&bq->poweroff_conf);
                          	report_count = 0;
				pr_err("notify poweroff config[%d,%d] \n", poweroff_vol, shutdown_delay_vol);
			}
		}
          	if(bq->poweroff_conf.poweroff_voltage == poweroff_vol \
					&& bq->poweroff_conf.shutdown_delay_voltage == shutdown_delay_vol){
                  	report_count++;
                  	if(report_count < 10){
                          	lx_charger_notifier_call_chain(CHARGER_EVENT_SHUTDOWN_VOLTAGE_CHANGED, (void *)&bq->poweroff_conf);
				pr_err("notify poweroff config[%d,%d] \n", poweroff_vol, shutdown_delay_vol);
                        }
                }

	}
}

static int dod_count_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if(gm && gm->dod_count > 0)
	{
		*val = gm->dod_count;
		return 0;
	}
	if (gm)
		*val = gm->dod_count;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int dod_count_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->dod_count = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static void low_vbat_power_off(struct bq_fg_chip *bq)
{
	static int count = 0;
	int tbat = 0;

	if (!bq->batt_psy) {
		if(!battery_get_psy(bq)) {
			pr_err("%s %s failed to get battery psy\n",__func__, bq->log_tag);
			return;
		}
	}
	// check vbat<=2.9V 4 times
	do {
		if((bq->vbat <= 2900 && tbat >= 0) || (bq->vbat <= 2700 && tbat < 0)){
			count++;
			msleep(1000);
			fg_read_volt(bq); // update vbat
			tbat = fg_read_temperature(bq); // update tbat
			continue;
		} else{
			count = 0;
			break;
		}
	} while(count<5);
	if(count>4){
		bq->batt_cap_level_critical = 1;
		power_supply_changed(bq->batt_psy);
		return;
	}
}
#endif //IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);

	bq->is_eu_mode = get_eu_mode();
	fg_dbg("%s: is_eu_mode:%d \n", __func__, bq->is_eu_mode);
	fg_update_status(bq);
#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
	if(!bq->i2c_err_flag){
		fg_select_poweroff_voltage_config(bq);
		low_vbat_power_off(bq);
	}
#endif // IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)

	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(bq->monitor_delay));
	if (bq->bms_wakelock->active)
		__pm_relax(bq->bms_wakelock);
}

static int fastcharge_mode_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->fast_chg;
	else
		*val = 0;
	fg_dbg("%s %d\n", __func__, *val);
	return 0;
}

static int fastcharge_mode_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_fastcharge_mode(gm, !!val);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int monitor_delay_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->monitor_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int monitor_delay_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->monitor_delay = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int fcc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->fcc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int rm_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->rm;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int rsoc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->rsoc;
	else
		*val = 0;
	fg_dbg("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->shutdown_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_shutdown_delay_enable = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int capacity_raw_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->raw_soc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soc_decimal_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fg_get_soc_decimal(gm);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int av_current_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fg_read_avg_current(gm);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soc_decimal_rate_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fg_get_soc_decimal_rate(gm);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int resistance_id_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = 100000;
	else
		*val = 0;
	fg_dbg("%s %d\n", __func__, *val);
	return 0;
}

static int resistance_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = 100000;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int authentic_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->authenticate;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}
#define USBPD_RETRY_COUNT 4
static int authentic_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	static int auth_done;
	if (gm)
		gm->authenticate = !!val;
	fg_err("%s %d\n", __func__, val);
	auth_done++;
	if (val == 1 || auth_done == USBPD_RETRY_COUNT) {
		lx_charger_notifier_call_chain(CHARGER_EVENT_BMS_AUTH_DONE, NULL);
	}
	return 0;
}

static int shutdown_mode_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->authenticate;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_mode_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_shutdown_mode(gm);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int chip_ok_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->chip_ok;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int charge_done_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->batt_fc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soh_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->soh;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int bms_slave_connect_error_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	*val = gpio_get_value(gm->slave_connect_gpio);
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int cell_supplier_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->cell_supplier;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int i2c_error_count_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if(gm && gm->fake_i2c_error_count > 0)
	{
		*val = gm->fake_i2c_error_count;
		return 0;
	}
	if (gm)
		*val = gm->i2c_error_count;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int i2c_error_count_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_i2c_error_count = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static ssize_t bms_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq_fg_chip *gm;
	struct mtk_bms_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_bms_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(gm, usb_attr, val);

	return count;
}

static ssize_t bms_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *gm;
	struct mtk_bms_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_bms_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(gm, usb_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static int temp_max_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime1[32];
	int ret = 0;

	memset(data_limetime1, 0, sizeof(data_limetime1));

	ret = fg_mac_read_block(gm, FG_MAC_CMD_LIFETIME1, data_limetime1, sizeof(data_limetime1));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_LIFETIME1\n");
	*val = data_limetime1[6];

	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int time_ot_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime3[32];
	char data[32];
	int ret = 0;

	memset(data_limetime3, 0, sizeof(data_limetime3));
	memset(data, 0, sizeof(data));

	ret = fg_mac_read_block(gm, FG_MAC_CMD_LIFETIME3, data_limetime3, sizeof(data_limetime3));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_LIFETIME3\n");

	ret = fg_mac_read_block(gm, FG_MAC_CMD_MANU_NAME, data, sizeof(data));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_MANU_NAME\n");

	if (data[2] == 'C') //TI
	{
		ret = fg_mac_read_block(gm, FG_MAC_CMD_FW_VER, data, sizeof(data));
		if (ret)
			fg_err("failed to get FG_MAC_CMD_FW_VER\n");

		if ((data[3] == 0x0) && (data[4] == 0x1)) //R0 FW
			*val = ((data_limetime3[15] << 8) | (data_limetime3[14] << 0)) << 2;
		else if ((data[3] == 0x1) && (data[4] == 0x2)) //R1 FW
			*val = ((data_limetime3[9] << 8) | (data_limetime3[8] << 0)) << 2;
	}
	else if (data[2] == '4') //NVT
		*val = (data_limetime3[15] << 8) | (data_limetime3[14] << 0);

	fg_err("%s %d\n", __func__, *val);
	return 0;
}

int soa_alert_level_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 soa_alert_level = 0;

	if(gm->device_name != BQ_FG_NFG1000A && gm->device_name != BQ_FG_NFG1000B)
	{
		fg_err("%s: this Bq_Fg is not support this function.\n", __func__);
		return -1;
	}

	ret = fg_read_byte(gm,gm->regs[NVT_FG_REG_SOA_L], &soa_alert_level);

	if(ret < 0)
	{
		fg_err("%s: read soa_alert_level occur error.\n", __func__);
		return ret;
	}
	*val = soa_alert_level;
	fg_err("%s:now soa:%d.\n", __func__, *val);
	return ret;
}

int charging_state_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u16 charging_state = 0;

	ret = fg_read_word(gm, FG_MAC_CMD_CHARGING_STATUS, &charging_state);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", __func__);

	*val = !!(charging_state & BIT(3));
	fg_err("%s:charging_state:%d.\n", __func__, *val);
	return ret;
}

int cis_alert_level_get(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 isc_alert_level = 0;

	if (bq->device_name != BQ_FG_NFG1000A &&
	    bq->device_name != BQ_FG_NFG1000B && 
		bq->device_name != MPC_FG_MPC8011B) {
		fg_err("this Bq_Fg is not support this function.\n");
		return -1;
	}

	if(bq->fake_cis_alert != 0){
		fg_info("%s fake_cis_alert:%d\n", bq->log_tag, bq->fake_cis_alert);
		return bq->fake_cis_alert;
	}

	ret = fg_read_byte(bq, bq->regs[NVT_FG_REG_ISC], &isc_alert_level);

	if (ret < 0) {
		fg_err("read isc_alert_level occur error.\n");
		isc_alert_level = bq->cis_alert_level ;
	}
	bq->cis_alert_level = isc_alert_level;
	fg_dbg("now bq->cis_alert_level:%d.\n", bq->cis_alert_level);
	return isc_alert_level;
}

static int cis_alert_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm && (gm->fake_cis_alert != 0)) {
		*val = gm->fake_cis_alert;
		fg_info("gm->fake_cis_alert:%d\n", gm->fake_cis_alert);
		return 0;
	}
	if (gm)
		*val = gm->cis_alert_level;
	else
		*val = 0;
	fg_info("gm->cis_alert_level:%d.\n", gm->cis_alert_level);
	return 0;
}

static int cis_alert_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_cis_alert = val;
	fg_info("gm->fake_cis_alert:%d\n", gm->fake_cis_alert);
	return 0;
}

/* Must be in the same order as BMS_PROP_* */
static struct mtk_bms_sysfs_field_info bms_sysfs_field_tbl[] = {
	BMS_SYSFS_FIELD_RW(fastcharge_mode, BMS_PROP_FASTCHARGE_MODE),
	BMS_SYSFS_FIELD_RW(monitor_delay, BMS_PROP_MONITOR_DELAY),
	BMS_SYSFS_FIELD_RO(fcc, BMS_PROP_FCC),
	BMS_SYSFS_FIELD_RO(rm, BMS_PROP_RM),
	BMS_SYSFS_FIELD_RO(rsoc, BMS_PROP_RSOC),
	BMS_SYSFS_FIELD_RW(shutdown_delay, BMS_PROP_SHUTDOWN_DELAY),
	BMS_SYSFS_FIELD_RO(capacity_raw, BMS_PROP_CAPACITY_RAW),
	BMS_SYSFS_FIELD_RO(soc_decimal, BMS_PROP_SOC_DECIMAL),
	BMS_SYSFS_FIELD_RO(soc_decimal_rate, BMS_PROP_SOC_DECIMAL_RATE),
	BMS_SYSFS_FIELD_RO(resistance_id, BMS_PROP_RESISTANCE_ID),
	BMS_SYSFS_FIELD_RW(authentic, BMS_PROP_AUTHENTIC),
	BMS_SYSFS_FIELD_RW(shutdown_mode, BMS_PROP_SHUTDOWN_MODE),
	BMS_SYSFS_FIELD_RO(chip_ok, BMS_PROP_CHIP_OK),
	BMS_SYSFS_FIELD_RO(charge_done, BMS_PROP_CHARGE_DONE),
	BMS_SYSFS_FIELD_RO(soh, BMS_PROP_SOH),
	BMS_SYSFS_FIELD_RO(resistance, BMS_PROP_RESISTANCE),
	BMS_SYSFS_FIELD_RW(i2c_error_count, BMS_PROP_I2C_ERROR_COUNT),
	BMS_SYSFS_FIELD_RO(av_current, BMS_PROP_AV_CURRENT),
	BMS_SYSFS_FIELD_RO(temp_max, BMS_PROP_TEMP_MAX),
	BMS_SYSFS_FIELD_RO(time_ot, BMS_PROP_TIME_OT),
	BMS_SYSFS_FIELD_RO(bms_slave_connect_error, BMS_PROP_BMS_SLAVE_CONNECT_ERROR),
	BMS_SYSFS_FIELD_RO(cell_supplier, BMS_PROP_CELL_SUPPLIER),
	BMS_SYSFS_FIELD_RO(soa_alert_level, BMS_PROP_SOA_ALERT_LEVEL),
	BMS_SYSFS_FIELD_RO(charging_state, BMS_PROP_CHARGING_STATE),
#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
	BMS_SYSFS_FIELD_RW(dod_count, BMS_PROP_DOD_COUNT),
#endif
	BMS_SYSFS_FIELD_RW(cis_alert, BMS_PROP_CIS_ALERT),
};

int bms_get_property(enum bms_property bp,
			    int *val)
{
	struct bq_fg_chip *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].get(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		fg_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(bms_get_property);

int bms_set_property(enum bms_property bp,
			    int val)
{
	struct bq_fg_chip *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].set(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		fg_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(bms_set_property);

static struct attribute *
	bms_sysfs_attrs[ARRAY_SIZE(bms_sysfs_field_tbl) + 1];

static const struct attribute_group bms_sysfs_attr_group = {
	.attrs = bms_sysfs_attrs,
};

static void bms_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(bms_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		bms_sysfs_attrs[i] = &bms_sysfs_field_tbl[i].attr.attr;

	bms_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int bms_sysfs_create_group(struct power_supply *psy)
{
	bms_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&bms_sysfs_attr_group);
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MIN,
};

#define SHUTDOWN_DELAY_VOL	3300
#define SHUTDOWN_VOL	3400
static int fg_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		mutex_lock(&bq->data_lock);
		fg_read_volt(bq);
		val->intval = bq->cell_voltage[2] * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		if (bq->i2c_err_flag) {
			val->intval = 500000;
		} else {
			bq->ibat = fg_read_current(bq);
			val->intval = bq->ibat * 1000;
		}
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc) {
			val->intval = bq->fake_soc;
			break;
		}
		if (bq->i2c_err_flag) {
			val->intval = 15;
		} else {
			val->intval = bq->ui_soc;
		}

		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_tbat != -FAKE_TBAT_NODATA) {
			val->intval = bq->fake_tbat;
			break;
		}
		if (bq->i2c_err_flag) {
			val->intval = 250;
		} else {
			val->intval = bq->tbat;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B)
			val->intval = bq->fcc;
		else
			val->intval = 7000;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B)
			val->intval = bq->dc;
		else
			val->intval = 7000;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		mutex_lock(&bq->data_lock);
		bq->rm = fg_read_rm(bq);
		val->intval = bq->rm * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		mutex_lock(&bq->rw_lock);
		val->intval = fg_read_cyclecount(bq);
		mutex_unlock(&bq->rw_lock);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		if(bq->i2c_err_flag){
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
			break;
		}
	#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
		if(bq->batt_cap_level_critical){
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
			break;
		}
		if( bq->vbat < bq->poweroff_conf.poweroff_voltage){
			pr_err("%s vbat:%d poweroff voltage:%d \n",__func__, bq->vbat, bq->poweroff_conf.poweroff_voltage);
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
			break;
		}
	#else
		if(bq->vbat<=SHUTDOWN_DELAY_VOL_LOW){
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		}
	#endif
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MIN:
		val->intval = cis_alert_level_get(bq);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        struct votable	*fv_votable = NULL;
#endif
	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		bq->fake_tbat = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq->fake_soc = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		bq->fake_cycle_count = val->intval;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		fv_votable = find_votable("MAIN_FV");
		if (!fv_votable) {
			pr_err("%s failed to get fv_votable\n", __func__);
		} else {
			rerun_election(fv_votable);
		}
#endif
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MIN:
		bq->fake_cis_alert = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static char *mtk_bms_supplied_to[] = {
        "battery",
        "usb",
};

static int fg_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MIN:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int fg_init_psy(struct bq_fg_chip *bq)
{
	struct power_supply_config fg_psy_cfg = {};

	bq->fg_psy_d.name = "bms";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_get_property;
	bq->fg_psy_d.set_property = fg_set_property;
	bq->fg_psy_d.property_is_writeable = fg_prop_is_writeable;
	fg_psy_cfg.supplied_to = mtk_bms_supplied_to;
	fg_psy_cfg.num_supplicants = ARRAY_SIZE(mtk_bms_supplied_to);
	fg_psy_cfg.drv_data = bq;

	bq->fg_psy = devm_power_supply_register(bq->dev, &bq->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		fg_err("%s failed to register fg_psy", bq->log_tag);
		return PTR_ERR(bq->fg_psy);
	} else
	    bms_sysfs_create_group(bq->fg_psy);

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

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B) {
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

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);
	fg_stringtohex(kbuf, random, &i);
	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B)
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

static ssize_t fg_show_ui_soh(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	static u16 addr = 0x007B;
	u8 ui_soh_data[70] = {0};
	int ret = 0;

	ret = fg_mac_read_block(bq, addr, ui_soh_data, 32);
	if (ret < 0)
	{
		fg_err("failed to read ui_soh_data \n");
		return ret;
	}
	ret = snprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %d %d %d %d %d \n",
		ui_soh_data[0],ui_soh_data[1],ui_soh_data[2],ui_soh_data[3],ui_soh_data[4],ui_soh_data[5],
		ui_soh_data[6],ui_soh_data[7],ui_soh_data[8],ui_soh_data[9],ui_soh_data[10]);
	fg_err("%s: latest_ui_soh = %d \n", __func__, ui_soh_data[0]);
	return ret;
}

static ssize_t fg_store_ui_soh(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	char t_data[70] = {0};
	char *pchar = NULL, *qchar = NULL;
	u8 ui_soh_data[40] = {0,};
	u8 manuInfoC_data[32];
	int ret = 0, i = 0;
	u8 val = 0;

	fg_err("%s raw data buf: %s \n", __func__, buf);
	memset(t_data, 0, sizeof(t_data));
	strncpy(t_data, buf, count);
	fg_err("%s t_data : %s\n", __func__, t_data);

	qchar = t_data;

	while ((pchar = strsep(&qchar, " ")))
	{
		ret = kstrtou8(pchar, 10, &val);
		if (ret < 0) {
			fg_err("kstrtou8 error return %d \n", ret);
			return count;
		}
		ui_soh_data[i] = val;
		val = 0;
		fg_err("%s ui_soh_data[%d]: %d \n", __func__ ,i, ui_soh_data[i]);
		i++;
	}

	bq->ui_soh = ui_soh_data[0];
	fg_err("%s: bq->ui_soh = %d \n",__func__, bq->ui_soh);
	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, manuInfoC_data, 32);
	if (ret) {
		fg_err("get old FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
		return -EPERM;
	}
	memcpy(manuInfoC_data, ui_soh_data, UISOH_LEN);
	ret = fg_mac_write_block(bq, FG_MAC_CMD_MANU_INFOC, manuInfoC_data, 32);
	if (ret < 0)
	{
		fg_err("failed to write ui_soh_data \n");
		return count;
	}

	return count;
}
static ssize_t ntc_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u16 ntc_temp;
	int temp;
	int ret;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RAW_TEMP], &ntc_temp);
	if (ret < 0) {
		fg_err("get FG_MAC_CMD_NTC_TEMP failed:%d\n", ret);
		return -EPERM;
	}
	temp = ntc_temp - 2730;
	temp = temp * 100;
	fg_dbg("battery raw temp:%d\n", temp);

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static DEVICE_ATTR(fcc, S_IRUGO, fg_show_fcc, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_show_rm, NULL);
static DEVICE_ATTR(rsoc, S_IRUGO, fg_show_rsoc, NULL);
static DEVICE_ATTR(cell0_voltage, S_IRUGO, fg_show_cell0_voltage, NULL);
static DEVICE_ATTR(cell1_voltage, S_IRUGO, fg_show_cell1_voltage, NULL);
static DEVICE_ATTR(qmax0, S_IRUGO, fg_show_qmax0, NULL);
static DEVICE_ATTR(qmax1, S_IRUGO, fg_show_qmax1, NULL);
static DEVICE_ATTR(verify_digest, S_IRUGO | S_IWUSR, fg_verify_digest_show, fg_verify_digest_store);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, fg_show_log_level, fg_store_log_level);
static DEVICE_ATTR(ui_soh, S_IRUGO | S_IWUSR, fg_show_ui_soh, fg_store_ui_soh);
static DEVICE_ATTR(ntc_temp, S_IRUGO, ntc_temp_show, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_cell0_voltage.attr,
	&dev_attr_cell1_voltage.attr,
	&dev_attr_qmax0.attr,
	&dev_attr_qmax1.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_log_level.attr,
	&dev_attr_ui_soh.attr,
	&dev_attr_ntc_temp.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};

static int fg_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret = 0, size = 0;

	bq->max_chg_power_120w = of_property_read_bool(node, "max_chg_power_120w");
	bq->enable_shutdown_delay = of_property_read_bool(node, "enable_shutdown_delay");

#if  defined (CONFIG_TARGET_PRODUCT_YUECHU)
        bq->slave_connect_gpio = of_get_named_gpio(node, "slave_connect_gpio", 0);
          fg_err("%s slave_connect_gpio = %d \n", bq->log_tag, bq->slave_connect_gpio );
          if (!gpio_is_valid(bq->slave_connect_gpio)) {
                 fg_info("failed to parse slave_connect_gpio\n");
                 return -1;
          }
#endif

	ret = of_property_read_u32(node, "normal_shutdown_vbat_1s", &bq->normal_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse normal_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "critical_shutdown_vbat_1s", &bq->critical_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "cool_critical_shutdown_vbat_1s", &bq->cool_critical_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse cool_critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "old_critical_shutdown_vbat_1s", &bq->old_critical_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse old_critical_shutdown_vbat_1s\n", bq->log_tag);

#ifndef CONFIG_TARGET_PRODUCT_XAGA
	ret = of_property_read_u32(node, "report_full_rsoc_1s", &bq->report_full_rsoc);
	if (ret)
		fg_err("%s failed to parse report_full_rsoc_1s\n", bq->log_tag);
#else
	if (product_name == XAGA)
		bq->report_full_rsoc = 9700;
	else if (product_name == XAGAPRO)
		bq->report_full_rsoc = 9500;
#endif

	ret = of_property_read_u32(node, "soc_gap_1s", &bq->soc_gap);
	if (ret)
		fg_err("%s failed to parse soc_gap_1s\n", bq->log_tag);

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
	int retry = 0;
	static char ch = '\0';
	while (retry++ < 5) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_NAME, data, 32);
		if (ret) {
			bq->i2c_err_flag = true;
			fg_err("failed to get FG_MAC_CMD_DEVICE_NAME ret =%d\n", ret);
		} else {
			bq->i2c_err_flag = false;
			fg_err("data : %s\n", data);
			break;
		}
		msleep(5);
	}

	for (i = 0; i < 8; i++) {
		if (data[i] >= 'A' && data[i] <= 'Z')
			data[i] += 32;
	}

	if (!strncmp(data, "xm81@bn70#", 10)) {
		bq->device_name = BQ_FG_NFG1000B;
		strcpy(bq->model_name, "nfg1000b");
		strcpy(bq->log_tag, "[LXCHG_NFG1000]");
	} else if (!strncmp(data, "sn27z565R1", 8)) {
		bq->device_name = BQ_FG_BQ27Z561;
		strcpy(bq->model_name, "sn27z565R1");
		strcpy(bq->log_tag, "[LXCHG_SN27Z565R1]");
	} else if (!strncmp(data, "bq27z561", 8)) {
		bq->device_name = BQ_FG_BQ27Z561;
		strcpy(bq->model_name, "bq27z561");
		strcpy(bq->log_tag, "[LXCHG_BQ27Z561]");
	} else if (!strncmp(data, "bq28z610", 8)) {
		bq->device_name = BQ_FG_BQ28Z610;
		strcpy(bq->model_name, "bq28z610");
		strcpy(bq->log_tag, "[LXCHG_BQ28Z610]");
	} else if (!strncmp(data, "xn25@bm7a#", 8)) {
		bq->device_name = MPC_FG_MPC8011B;
		strcpy(bq->model_name, "mpc8011b");
		strcpy(bq->log_tag, "[LXCHG_MPC8011B]");
	}else if (!strncmp(data, "nfg1000a", 8)) {
		bq->device_name = BQ_FG_NFG1000A;
		strcpy(bq->model_name, "nfg1000a");
		strcpy(bq->log_tag, "[LXCHG_NFG1000A]");
	} else if (!strncmp(data, "nfg1000b", 8)) {
		bq->device_name = BQ_FG_NFG1000B;
		strcpy(bq->model_name, "nfg1000b");
		strcpy(bq->log_tag, "[LXCHG_NFG1000B]");
	} else if (!strncmp(data, "m9@bp4p#", 8)) {
		bq->device_name = BQ_FG_NFG1000A;
		strcpy(bq->model_name, "nfg1000a");
		strcpy(bq->log_tag, "[LXCHG_NFG1000A]");
	} else if (!strncmp(data, "m12a@bm5t#", 8)) {
		bq->device_name = BQ_FG_NFG1000A;
		strcpy(bq->model_name, "nfg1000a");
		strcpy(bq->log_tag, "[LXCHG_NFG1000A]");
	} else if (!strncmp(data, "m11r@bm5f#", 8)) {
		bq->device_name = BQ_FG_NFG1000B;
		strcpy(bq->model_name, "nfg1000b");
		strcpy(bq->log_tag, "[LXCHG_NFG1000B]");
	} else if (!strncmp(data, "n6@bm5t#", 8)) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, 32);
		fg_err("FG_MAC_CMD_MANU_NAME:data : %s\n", data);
		if (!strncmp(&data[2], "C", 1)) {
			bq->device_name = BQ_FG_BQ27Z561;
			strcpy(bq->model_name, "bq27z561");
			strcpy(bq->log_tag, "[LXCHG_BQ27Z561]");
		} else if (!strncmp(&data[2], "5", 1)) {
			bq->device_name = BQ_FG_NFG1000A;
			strcpy(bq->model_name, "nfg1000a");
			strcpy(bq->log_tag, "[LXCHG_NFG1000A]");
		}
	} else if (!strncmp(data, "xm32@bp59#", 10)) {
		bq->device_name = BQ_FG_NFG1000B;
		strcpy(bq->model_name, "nfg1000b");
		strcpy(bq->log_tag, "[LXCHG_NFG1000]");
	} else {
		bq->device_name = BQ_FG_UNKNOWN;
		strcpy(bq->model_name, "UNKNOWN");
		strcpy(bq->log_tag, "[LXCHG_UNKNOWN_FG]");
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, 32);
	if (ret)
		fg_info("failed to get FG_MAC_CMD_MANU_NAME\n");

	ch = data[4];
	fg_info("ch:%c\n", ch);
	bq->ch = !strncmp(&ch, "6", 1);

	if (!strncmp(data, "MI", 2) && bq->device_name != BQ_FG_UNKNOWN)
		bq->chip_ok = true;
	else
		bq->chip_ok = false;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_CHEM, data, 32);
	if (ret) {
		fg_info("failed to get FG_MAC_CMD_DEVICE_CHEM\n");
	} else {
		fg_info("data[2]:%c\n", data[2]);
		fg_info("FG_MAC_CMD_DEVICE_CHEM data:%s\n", data);
	}

	if (!strncmp(&data[2], "N", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_NVT_ATL;
		strcpy(bq->device_chem, "NVT_ATL");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY");
	} else if(!strncmp(&data[2], "C", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_COS_COS;
		strcpy(bq->device_chem, "COS_COS");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY");
	} else if(!strncmp(&data[2], "S", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_SWD_LWN;
		strcpy(bq->device_chem, "SWD_LWN");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY");
	} else {
		bq->cell_supplier = BMS_CELL_UNKNOWN;
		strcpy(bq->device_chem, "UNKNOWN");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_UNKNOWN");
	}

	return ret;
}

static int fuelguage_get_input_suspend(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return bq->input_suspend;
}

static int fuel_guage_get_avg_ibat(struct fuel_gauge_dev *fuel_gauge)
{
	int ret = 0;
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	mutex_lock(&bq->data_lock);
	ret = fg_read_avg_current(bq);
	mutex_unlock(&bq->data_lock);
	return ret;
}

static int fuelguage_get_lpd_charging(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return bq->lpd_charging;
}

static int fuelguage_get_mtbf_current(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return bq->mtbf_current;
}

static int fuel_guage_reg_read_block(struct fuel_gauge_dev *fuel_gauge, u8 reg, u8 *buf, u8 len)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return fg_read_block(bq, reg, buf, len);
}
static int fuel_guage_reg_write_block(struct fuel_gauge_dev *fuel_gauge, u8 reg, u8 *data, u8 len)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return fg_write_block(bq, reg, data, len);
}

static int fuel_guage_read_block(struct fuel_gauge_dev *fuel_gauge, u16 cmd, u8 *buf, u8 len)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return fg_mac_read_block(bq, cmd, buf, len);
}
static int fuel_guage_write_block(struct fuel_gauge_dev *fuel_gauge, u16 cmd, u8 *data, u8 len)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	return fg_mac_write_block(bq, cmd, data, len);
}

int charger_partition_info_get_prop(struct fuel_gauge_dev *fuel_gauge, int type, int *val)
{
	return charger_partition_info1_get_prop(type, val);
}
int charger_partition_info_set_prop(struct fuel_gauge_dev *fuel_gauge, int type, int val)
{
	return charger_partition_info1_set_prop(type, val);
}
static struct fuel_gauge_ops fuel_gauge_ops = {
	.get_avg_ibat = fuel_guage_get_avg_ibat,
	.get_soc_decimal = fuel_guage_get_soc_decimal,
	.get_soc_decimal_rate = fuel_guage_get_soc_decimal_rate,
	.get_chip_ok = fuel_guage_get_chip_ok,
	.get_resistance_id = fuel_guage_get_resistance_id,
	.get_fg_battery_id = fuel_guage_get_battery_id,
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	.check_i2c_function = fuelguage_check_i2c_function,
#endif
	.get_input_suspend = fuelguage_get_input_suspend,
	.get_lpd_charging = fuelguage_get_lpd_charging,
	.get_mtbf_current = fuelguage_get_mtbf_current,
	.set_fastcharge_mode = fuelguage_set_fastcharge_mode,
	.fg_set_charger_to_full = fuelguage_set_charger_to_full,
	.get_fastcharge_mode = fuelguage_get_fastcharge_mode,
	.fg_write_block = fuel_guage_write_block,
	.fg_read_block = fuel_guage_read_block,
	.fg_reg_write_block = fuel_guage_reg_write_block,
	.fg_reg_read_block = fuel_guage_reg_read_block,
	.fg_get_partition_prop = charger_partition_info_get_prop,
	.fg_set_partition_prop = charger_partition_info_set_prop,
	.fg_update = fuel_gauge_update_status,
};

static ssize_t soh_sn_show(struct class *c,
				struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	if (!IS_ERR_OR_NULL(bq)) {
		if (!bq->bat_sn[0]) {
			mutex_lock(&bq->rw_lock);
			ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFO, bq->bat_sn, 32);
			mutex_unlock(&bq->rw_lock);
			if (ret) {
				memset(ubuf, '0', 32);
				ubuf[32] = '\0';
				fg_err("failed to get FG_MAC_CMD_MANU_INFO:%d\n", ret);
				return -EINVAL;
			}
		}
		memcpy(ubuf, bq->bat_sn, 32);
		ubuf[32] = '\0';
		print_hex_dump(KERN_ERR, "battery sn hex:", DUMP_PREFIX_NONE, 16, 1, ubuf, 32, 0);
		fg_dbg("battery sn string:%s\n", ubuf);
	}
	return strlen(ubuf);
}
static CLASS_ATTR_RO(soh_sn);

static ssize_t manufacturing_date_show(struct class *c,
				struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (!bq->bat_sn[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFO, bq->bat_sn, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			fg_err("failed to get FG_MAC_CMD_MANU_INFO:%d\n", ret);
			return -EINVAL;
		}
	}
	fg_err("battery manufacture date:%02X %02X %02X %02X\n",bq->bat_sn[6], bq->bat_sn[7], bq->bat_sn[8], bq->bat_sn[9]);
	// Year
	ubuf[0] = '2';
	ubuf[1] = '0';
	ubuf[2] = '2';
	ubuf[3] = bq->bat_sn[6];
	// month
	if(bq->bat_sn[7] >= '0' && bq->bat_sn[7] <= '9'){
		ubuf[4] = '0';
		ubuf[5] = bq->bat_sn[7];
	}else if (bq->bat_sn[7] >= 'A' && bq->bat_sn[7] <= 'F'){
		ubuf[4] = '1';
		ubuf[5] = bq->bat_sn[7] - ('A'-'0');		
	}else if(bq->bat_sn[7] >= 'a' && bq->bat_sn[7] <= 'f'){
		ubuf[4] = '1';
		ubuf[5] = bq->bat_sn[7] - ('a'-'0');		
	}
	// day
	ubuf[6] = bq->bat_sn[8];
	ubuf[7] = bq->bat_sn[9];
	ubuf[8] = '\0';
	fg_err("battery manufacture date:%s\n", ubuf);

	return strlen(ubuf);
}
static CLASS_ATTR_RO(manufacturing_date);

static ssize_t first_usage_date_show(struct class *c,
				struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '9', 8);
			ubuf[8] = '\0';
			fg_err("failed to get FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EINVAL;
		} else
			bq->mi_infoC_valid = 1;
	}
	fg_err("battery activiate date hex: %02X%02X%02X\n",
	bq->mi_infoC[11], bq->mi_infoC[12], bq->mi_infoC[13]);
	if (bq->mi_infoC[11] == 0x00 && bq->mi_infoC[12] == 0x00 && bq->mi_infoC[13] == 0x00) {
		memset(ubuf, '0', 8);
		ubuf[8] = '\0';
		fg_err("reset data to 0\n");
	} else {
		ubuf[0] = '2';
		ubuf[1] = '0';
		ubuf[2] = '0' + bq->mi_infoC[11] / 10;
		ubuf[3] = '0' + bq->mi_infoC[11] % 10;
		ubuf[4] = '0' + bq->mi_infoC[12] / 10;
		ubuf[5] = '0' + bq->mi_infoC[12] % 10;
		ubuf[6] = '0' + bq->mi_infoC[13] / 10;
		ubuf[7] = '0' + bq->mi_infoC[13] % 10;
		ubuf[8] = '\0';
		fg_err("battery activiate date string:%s\n", ubuf);
	}

	return strlen(ubuf);
}

static ssize_t first_usage_date_store(struct class *c,
				struct class_attribute *attr, const char *buf, size_t len)
{
	char date_str[8], date_u8[3];
	int i, j = 0, ret = -EINVAL, date_len;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			fg_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}

	date_len = strlen(buf);
	for (i = 0; i < date_len; i++) {
		if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n') {
			if (j >= 8) {
				fg_err("date length too large\n");
				return -E2BIG;
			}
			if (buf[i] < '0' || buf[i] > '9') {
				fg_err("date has invalid char:%c(0x%02x)\n", buf[i], buf[i]);
				return -EINVAL;
			}
			date_str[j++] = buf[i];
		}
	}
	fg_err("activiate date hex: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		date_str[0], date_str[1], date_str[2], date_str[3], date_str[4], date_str[5],  date_str[6],  date_str[7]);
	date_u8[0] = (date_str[2] - '0') * 10 + (date_str[3] - '0');
	date_u8[1] = (date_str[4] - '0') * 10 + (date_str[5] - '0');
	date_u8[2] = (date_str[6] - '0') * 10 + (date_str[7] - '0');
	if (date_u8[0] == bq->mi_infoC[11] && date_u8[1] == bq->mi_infoC[12] && date_u8[2] == bq->mi_infoC[13]) {
		fg_err("repeated date write, not allowed\n");
		return len;
	}
	bq->mi_infoC[11] = (date_str[2] - '0') * 10 + (date_str[3] - '0');
	bq->mi_infoC[12] = (date_str[4] - '0') * 10 + (date_str[5] - '0');
	bq->mi_infoC[13] = (date_str[6] - '0') * 10 + (date_str[7] - '0');
	mutex_lock(&bq->rw_lock);
	ret = fg_mac_write_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		bq->mi_infoC_valid = 0;
		fg_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
		return -EPERM;
	}
	return len;
}
static CLASS_ATTR_RW(first_usage_date);

static ssize_t authentic_show(struct class *c,
				struct class_attribute *attr, char *ubuf)
{
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", bq->authenticate);
}
static CLASS_ATTR_RO(authentic);

static ssize_t fg1_soh_show(struct class *c,
	struct class_attribute *attr, char *ubuf)
{
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", fg_read_soh(bq));
}
static CLASS_ATTR_RO(fg1_soh);

static ssize_t ui_soh_show(struct class *c,
    struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			fg_err("get FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}
	return snprintf(ubuf, PAGE_SIZE, "%u %u %u %u %u %u %u %u %u %u %u\n",
				bq->mi_infoC[0], bq->mi_infoC[1], bq->mi_infoC[2], bq->mi_infoC[3], bq->mi_infoC[4],
				bq->mi_infoC[5], bq->mi_infoC[6], bq->mi_infoC[7], bq->mi_infoC[8], bq->mi_infoC[9], bq->mi_infoC[10]);
}

static ssize_t ui_soh_store(struct class *c, struct class_attribute *attr,
                            const char *buf, size_t count)
{
    int ret, cnt = 0;
    ssize_t len = 0;
	char tx_data[64], tx_char[64];
	char *pchar = NULL, *qchar = NULL;
    u8 val, ui_soh_data[UISOH_LEN];
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			pr_err("get old FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}
	memset(tx_data, 0, sizeof(tx_data));
	memset(ui_soh_data, 0, sizeof(ui_soh_data));
	if (count > sizeof(tx_data)) {
		fg_err("%s: data len:%d invalid\n", __func__, count);
		return -EINVAL;
	}
	strncpy(tx_data, buf, count);
	qchar = tx_data;
	while ((pchar = strsep(&qchar, " "))) {
		if (kstrtou8(pchar, 10, &val) < 0) {
			fg_err("parse data:%d failed\n", cnt);
			return -EINVAL;
		}
		if (cnt == UISOH_LEN) {
			fg_err("write ui soh data len invalid, force quit\n");
			break;
		}
		ui_soh_data[cnt++] = val;
	}
	memset(tx_char, 0, sizeof(tx_char));
	len += snprintf(tx_char, sizeof(tx_char), "ui soh data:");
	for (cnt = 0; cnt < UISOH_LEN; cnt++)
		len += snprintf(tx_char + len, sizeof(tx_char), " %u", ui_soh_data[cnt]);
	fg_err("%s\n", tx_char);
	memcpy(bq->mi_infoC, ui_soh_data, UISOH_LEN);
	mutex_lock(&bq->rw_lock);
	ret = fg_mac_write_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		bq->mi_infoC_valid = 0;
		fg_err("get old FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
		return -EPERM;
	}

	return count;
}
static CLASS_ATTR_RW(ui_soh);

static ssize_t soh_new_show(struct class *c,
    struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			fg_err("read FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}
    return snprintf(ubuf, PAGE_SIZE, "%u\n", bq->mi_infoC[0]);
}
static CLASS_ATTR_RO(soh_new);

static ssize_t fg1_cycle_show(struct class *c,
	struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	mutex_lock(&bq->rw_lock);
	ret = fg_read_cyclecount(bq);
	mutex_unlock(&bq->rw_lock);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", ret);
}
static CLASS_ATTR_RO(fg1_cycle);

static ssize_t clear_cycle_store(
	struct class *c, struct class_attribute *attr,
	const char *buf, size_t count)
{
	int i, j, k, ret, cycle_count;
	u8 opr_data[32], wr_stat_data[32], rd_stat_data[32];
	u8 seal_fg[2] = { 0x30, 0x00 };
	u8 unseal_key[4] = { 0xA6, 0x35, 0xCF, 0x41 };
	char reset_key[16] = {
		'c',  'l',  'r',  'c',  'l',  's',  '\0', '\0',
		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
	};
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);

	if (count < strlen(reset_key)) {
		pr_err("%s: len invalid:%d\n", __func__, count);
		return -EINVAL;
	}

	if (memcmp(buf, reset_key, strlen(reset_key))) {
		pr_err("%s: key error\n", __func__);
		return -EINVAL;
	}
	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			pr_err("%s, read FG_MAC_CMD_MANU_INFOC failed:%d\n", __func__, ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}

	if (bq->mi_infoC[14] == 0x01) {
		pr_err("%s, cycle count already been reset\n", __func__);
		return -EPERM;
	}

	mutex_lock(&bq->rw_lock);
	cycle_count = fg_read_cyclecount(bq);
	mutex_unlock(&bq->rw_lock);
	if (cycle_count >= 10) {
		pr_err("%s, invalid cycle count:%d\n", __func__, cycle_count);
		return -EINVAL;
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			for (k = 0; k < 3; k++) {
				mutex_lock(&bq->rw_lock);
				ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], &unseal_key[0], 2);
				if (ret < 0) {
					pr_err("%s, write first unseal key failed:%d\n", __func__, ret);
					mutex_unlock(&bq->rw_lock);
					return -EINVAL;
				}
				ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], &unseal_key[2], 2);
				if (ret < 0) {
					pr_err("%s, write second unseal key failed:%d\n", __func__, ret);
					mutex_unlock(&bq->rw_lock);
					return -EINVAL;
				}
				ret = fg_mac_read_block(bq, FG_MAC_CMD_OPR_STAT, opr_data, 32);
				if (ret) {
					pr_err("%s, read operation status failed:%d\n", __func__, ret);
					mutex_unlock(&bq->rw_lock);
					return -EINVAL;
				}
				mutex_unlock(&bq->rw_lock);
				if ((opr_data[1] & 0x03) == 0x03) {
					pr_err("effect data error:%02X\n", opr_data[1]);
					msleep(100);
					continue;
				} else
					break;
			}
			if (k >= 3) {
				pr_err("%s, unseal fg failed\n", __func__);
				return -EPERM;
			}
			mutex_lock(&bq->rw_lock);
			ret = fg_mac_read_block(bq, FG_MAC_CMD_FG_STAT, wr_stat_data, 32);
			mutex_unlock(&bq->rw_lock);
			if (ret) {
				pr_err("%s, read fg status failed:%d\n", __func__, ret);
				continue;
			} else
				break;
		}
		if (j >= 3) {
			pr_err("%s, read fg status error\n", __func__);
			msleep(100);
			return -EPERM;
		}
		wr_stat_data[14] = wr_stat_data[15] = 0x00;
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FG_STAT, wr_stat_data, 32);
		msleep(100);
		mutex_unlock(&bq->rw_lock);
		if (ret)
			pr_err("%s, write fg status failed:%d\n", __func__, ret);
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_FG_STAT, rd_stat_data, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret)
			pr_err("%s, read back fg status failed:%d\n", __func__, ret);
		if (memcmp(wr_stat_data, rd_stat_data, 32)) {
			pr_err("%s, updata fg status failed\n", __func__);
			msleep(100);
			continue;
		} else
			break;
	}
	if (i >= 3)
		pr_err("%s, clear cycle count failed\n", __func__);
	mutex_lock(&bq->rw_lock);
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], &seal_fg[0], 2);
	if (ret < 0)
		pr_err("%s, first seal fg failed:%d\n", __func__, ret);
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], &seal_fg[0], 2);
	if (ret < 0)
		pr_err("%s, second seal fg failed:%d\n", __func__, ret);
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], &seal_fg[0], 2);
	if (ret < 0)
		pr_err("%s, third seal fg failed:%d\n", __func__, ret);
	cycle_count = fg_read_cyclecount(bq);
	mutex_unlock(&bq->rw_lock);
	if (!cycle_count) {
		pr_err("reset cycle count succeeded\n");
		bq->mi_infoC[14] = 0x01;
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_write_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret < 0) {
			bq->mi_infoC_valid = 0;
			pr_err("update reset cycle count flag failed\n");
	} else
		return count;
	}

	return -EINVAL;
}
static CLASS_ATTR_WO(clear_cycle);

static ssize_t clear_cycle_flag_show(struct class *c,
    struct class_attribute *attr, char *ubuf)
{
	int ret;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			fg_err("get FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}
	pr_info("cycle count flag:%d\n", bq->mi_infoC[14]);
	return snprintf(ubuf, PAGE_SIZE, "%u\n", bq->mi_infoC[14]);
}
static CLASS_ATTR_RO(clear_cycle_flag);

static ssize_t input_suspend_show(struct class *c, struct class_attribute *attr,char *ubuf)
{
		struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
		return scnprintf(ubuf, PAGE_SIZE, "%d\n", bq->input_suspend);
}

static ssize_t input_suspend_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	int val;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (kstrtoint(ubuf, 10, &val))
		return -EINVAL;

	bq->input_suspend = val;
	lx_info("bq->input_suspend = %d\n", bq->input_suspend);
	return count;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t lpd_charging_show(struct class *c, struct class_attribute *attr,char *ubuf)
{
		struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
		return scnprintf(ubuf, PAGE_SIZE, "%d\n", bq->lpd_charging);
}

static ssize_t lpd_charging_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	int val;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (kstrtoint(ubuf, 10, &val))
		return -EINVAL;

	bq->lpd_charging = val;
	lx_info("bq->lpd_charging = %d\n", bq->lpd_charging);
	return count;
}
static CLASS_ATTR_RW(lpd_charging);

static ssize_t mtbf_current_show(struct class *c, struct class_attribute *attr,char *ubuf)
{
		struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
		if (IS_ERR_OR_NULL(bq)) {
			fg_info("bq is null pointer\n");
			return PTR_ERR(bq);
		}
		return scnprintf(ubuf, PAGE_SIZE, "%d\n", bq->mtbf_current);
}

static ssize_t mtbf_current_store(struct class *c, struct class_attribute *attr,
			const char *ubuf, size_t count)
{
	int val;
	struct bq_fg_chip *bq = container_of(c, struct bq_fg_chip, qcom_batt_class);
	if (IS_ERR_OR_NULL(bq)) {
		fg_info("bq is null pointer\n");
		return PTR_ERR(bq);
	}
	if (kstrtoint(ubuf, 10, &val))
		return -EINVAL;

	bq->mtbf_current = val;
	lx_info("bq->mtbf_current = %d\n", bq->mtbf_current);
	return count;
}
static CLASS_ATTR_RW(mtbf_current);

static ssize_t is_eu_model_show(struct class *c,
				struct class_attribute *attr, char *ubuf)
{
	int rc = 0;
	int val = 0;
	rc = charger_partition_get_prop(CHARGER_PARTITION_PROP_EU_MODE, &val);
	if(rc < 0){
		pr_err("[charger] %s get eu_mode from charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	pr_err("[charger] %s eu_mode_val: %d \n", __func__, val);
	return scnprintf(ubuf, PAGE_SIZE, "%d\n", val);
}
static ssize_t is_eu_model_store(struct class *c,
				struct class_attribute *attr,const char *ubuf, size_t len)
{
	int rc = 0;
	int val = 0;
	rc = kstrtoint(ubuf, 10, &val);
	if (rc) {
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}
	rc = charger_partition_set_prop(CHARGER_PARTITION_PROP_EU_MODE, val);
	if(rc < 0){
		pr_err("[charger] %s set eu_mode to charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	return len;
}
static CLASS_ATTR_RW(is_eu_model);

static struct attribute *batt_class_attrs[] = {
	[0]	= &class_attr_soh_sn.attr,
	[1]	= &class_attr_manufacturing_date.attr,
	[2]	= &class_attr_first_usage_date.attr,
	[3]	= &class_attr_authentic.attr,
	[4]	= &class_attr_fg1_soh.attr,
	[5]	= &class_attr_ui_soh.attr,
	[6]	= &class_attr_soh_new.attr,
	[7]	= &class_attr_fg1_cycle.attr,
	[8]	= &class_attr_input_suspend.attr,
	[9]	= &class_attr_is_eu_model.attr,
	[10] = &class_attr_mtbf_current.attr,
	[11] = &class_attr_clear_cycle.attr,
	[12] = &class_attr_lpd_charging.attr,
	[13] = &class_attr_clear_cycle_flag.attr,
	NULL,
};
ATTRIBUTE_GROUPS(batt_class);

static int fg_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq_fg_chip *bq;
	int ret = 0;
	char *name = NULL;
	u8 data[5] = {0};

#if defined(CONFIG_TARGET_PRODUCT_XAGA)
	const char *sku = get_hw_sku();
	if (!strncmp(sku, "xagapro", strlen("xagapro")))
		product_name = XAGAPRO;
	else if (!strncmp(sku, "xaga", strlen("xaga")))
		product_name = XAGA;
#endif

#if defined(CONFIG_TARGET_PRODUCT_DAUMIER)
	product_name=DAUMIER;
#endif

	fg_info("FG probe enter\n");
	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_DMA);
	if (!bq)
		return -ENOMEM;

	bq->is_eu_mode = get_eu_mode();
	if(bq->is_eu_mode < 0){
		pr_err("%s: get eu_modefailed1: %d \n", __func__, bq->is_eu_mode);
		//return -EPROBE_DEFER;
	}else{
		pr_err("%s: is_eu_mode1:%d \n", __func__, bq->is_eu_mode);
	}
	bq->dev = &client->dev;
	bq->client = client;
	bq->monitor_delay = FG_MONITOR_DELAY_10S;
	bq->rsoc_smooth = 0;

	memcpy(bq->regs, bq_fg_regs, NUM_REGS);

	i2c_set_clientdata(client, bq);
	name = devm_kasprintf(bq->dev, GFP_KERNEL, "%s",
		"bms suspend wakelock");
	bq->bms_wakelock = wakeup_source_register(NULL, name);
	bq->shutdown_mode = false;
	bq->shutdown_flag = false;
	bq->fake_cycle_count = 0;
	bq->raw_soc = -ENODATA;
	bq->last_soc = -EINVAL;
	bq->i2c_error_count = 0;
	bq->first_flag = true;
	bq->ui_soh = 100;
	bq->lpd_charging = 0;
	bq->fake_tbat = -FAKE_TBAT_NODATA;
	bq->fake_cis_alert = 0;
	bq->cis_alert_level = 0;
	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	mutex_init(&bq->rw_lock);
	atomic_set(&bq->fg_in_sleep, 0);

	bq->regmap = devm_regmap_init_i2c(client, &fg_regmap_config);
	if (IS_ERR(bq->regmap)) {
		fg_err("failed to allocate regmap\n");
		return PTR_ERR(bq->regmap);
	}

	fg_check_device(bq);

	ret = fg_parse_dt(bq);
	if (ret) {
		fg_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	time_init = ktime_get();
	fg_update_status(bq);
#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
	fg_select_poweroff_voltage_config(bq);
#endif // IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
	ret = fg_init_psy(bq);
	if (ret) {
		fg_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret) {
		fg_err("%s failed to register sysfs ret=%d\n", bq->log_tag, ret);
		return ret;
	}

	bq->update_now = true;
	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(5000));

	bq->dc = fg_read_dc(bq);

	/* init fast charge mode */
	data[0] = 0;
	fg_err("-fastcharge init-\n");
	ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
	if (ret) {
		fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
	}

	bq->fuel_gauge = fuel_gauge_register("fuel_gauge",
						bq->dev, &fuel_gauge_ops, bq);
	if (!bq->fuel_gauge) {
		ret = PTR_ERR(bq->fuel_gauge);
		fg_err("%s failed to register fuel_gauge\n", bq->log_tag);
		return ret;
	}

	bq->charger = charger_find_dev_by_name("primary_chg");
	if (!bq->charger)
		lx_err("failed to master_charge device\n");

	bq->qcom_batt_class.name = "qcom-battery",
	bq->qcom_batt_class.owner = THIS_MODULE,
	bq->qcom_batt_class.class_groups = batt_class_groups;
	ret = class_register(&bq->qcom_batt_class);
	if (ret < 0)
		lx_err("register qcom-battery class failed:%d\n", ret);

	hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "gauge");
	fg_info("%s FG probe success\n", bq->log_tag);

	return 0;
}

static int fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	atomic_set(&bq->fg_in_sleep, 1);
	fg_err("%s in sleep\n", __func__);

	cancel_delayed_work_sync(&bq->monitor_work);

	return 0;
}

static int fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	atomic_set(&bq->fg_in_sleep, 0);
	fg_err("%s resume in sleep\n", __func__);
	if (!bq->bms_wakelock->active)
		__pm_stay_awake(bq->bms_wakelock);
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
