/*
 * bq27z561 fuel gauge driver
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) 2020 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"[bq27z561] %s: " fmt, __func__
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
#include <linux/pmic-voter.h>
#include "step-chg-jeita.h"

enum print_reason {
	PR_INTERRUPT    = BIT(0),
	PR_REGISTER     = BIT(1),
	PR_OEM		= BIT(2),
	PR_DEBUG	= BIT(3),
};

static int debug_mask = PR_OEM;
module_param_named(
	debug_mask, debug_mask, int, 0600
);

#define	INVALID_REG_ADDR	0xFF

#define FG_FLAGS_FD				BIT(4)
#define	FG_FLAGS_FC				BIT(5)
#define	FG_FLAGS_DSG				BIT(6)
#define FG_FLAGS_RCA				BIT(9)
#define FG_FLAGS_FASTCHAGE			BIT(5)

#define BATTERY_DIGEST_LEN 20

#define DEFUALT_FULL_DESIGN		100000

#define DEFUALT_TERM_CURR		200
#define DEFUALT_FFC_TERM_CURR		484

#define BQ_RAW_SOC_FULL		10000
#define BQ_REPORT_FULL_SOC	9800
#define BQ_CHARGE_FULL_SOC	9750
#define BQ_RECHARGE_SOC		9850

#define BQ27Z561_DEFUALT_TERM		-200
#define BQ27Z561_DEFUALT_FFC_TERM	-484
#define BQ27Z561_DEFUALT_RECHARGE_VOL	8800

#define PD_CHG_UPDATE_DELAY_US	20	/*20 sec*/
#define BQ_I2C_FAILED_SOC	15
#define BQ_I2C_FAILED_TEMP	250
#define BQ_I2C_FAILED_VOL	8888
#define BMS_FG_VERIFY		"BMS_FG_VERIFY"
#define BMS_FC_VOTER		"BMS_FC_VOTER"

#define BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC			4490
#define BQ_MAXIUM_VOLTAGE_FOR_CELL			4480

#define MONITOR_WORK_10S	10
#define MONITOR_WORK_5S		5
#define MONITOR_WORK_1S		1


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

static u8 bq27z561_regs[NUM_REGS] = {
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
	FG_MAC_CMD_GAUGING	= 0x0021,
	FG_MAC_CMD_SEAL		= 0x0030,
	FG_MAC_CMD_FASTCHARGE_EN = 0x003E,
	FG_MAC_CMD_FASTCHARGE_DIS = 0x003F,
	FG_MAC_CMD_DEV_RESET	= 0x0041,
	FG_MAC_CMD_DEVICE_NAME	= 0x004A,
	FG_MAC_CMD_MANU_NAME	= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS = 0x0055,
	FG_MAC_CMD_LIFETIME1	= 0x0060,
	FG_MAC_CMD_LIFETIME3	= 0x0062,
	FG_MAC_CMD_DASTATUS1	= 0x0071,
	FG_MAC_CMD_ITSTATUS1	= 0x0073,
	FG_MAC_CMD_ITSTATUS2	= 0x0074,
	FG_MAC_CMD_ITSTATUS3	= 0x0075,
	FG_MAC_CMD_CBSTATUS	= 0x0076,
	FG_MAC_CMD_FCC_SOH	= 0x0077,
	FG_MAC_CMD_610_NAME	= 0x4080,
	FG_MAC_CMD_561_NAME	= 0x40A1,
	FG_MAC_CMD_RA_TABLE	= 0x40C0,
};


enum {
	SEAL_STATE_RSVED,
	SEAL_STATE_UNSEALED,
	SEAL_STATE_SEALED,
	SEAL_STATE_FA,
};


enum bq_fg_device {
	BQ27Z561 = 0,
	BQ28Z610,
};

static const unsigned char *device2str[] = {
	"bq27z561",
	"bq28z610",
};

struct cold_thermal {
	int index;
	int temp_l;
	int temp_h;
	int curr_th;
};

struct bq_fg_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap    *regmap;

	struct mutex i2c_rw_lock;
	struct mutex data_lock;
	struct notifier_block   nb;

	int fw_ver;
	int df_ver;

	u8 chip;
	u8 regs[NUM_REGS];
	char *model_name;

	/* status tracking */

	bool batt_fc;
	bool batt_fd;	/* full depleted */

	bool batt_dsg;
	bool batt_rca;	/* remaining capacity alarm */

	int seal_state; /* 0 - Full Access, 1 - Unsealed, 2 - Sealed */
	int batt_tte;
	int batt_soc;
	int batt_fcc;	/* Full charge capacity */
	int batt_rm;	/* Remaining capacity */
	int batt_dc;	/* Design Capacity */
	int batt_volt;
	int batt_temp;
	int batt_curr;
	int batt_resistance;
	int batt_cyclecnt;	/* cycle count */
	int batt_st;
	int term_curr;
	int ffc_term_curr;
	int ffc_warm_term;
	int ffc_normal_term;
	int cold_term;
	int normal_term;
	int raw_soc;
	int last_soc;
	int soc_decimal;
	int soc_decimal_rate;
	int soh;

	/* debug */
	int skip_reads;
	int skip_writes;

	int fake_soc;
	int fake_temp;
	int fake_volt;
	int	fake_chip_ok;

	struct	delayed_work monitor_work;
	struct power_supply *fg_psy;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *bbc_psy;
	struct power_supply_desc fg_psy_d;
	struct timeval suspend_time;

	u8 digest[BATTERY_DIGEST_LEN];
	bool verify_digest_success;
	int constant_charge_current_max;
	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable	*chg_dis_votable;

	int health;
	int recharge_vol;

	/* workaround for debug or other purpose */
	bool	ignore_digest_for_debug;
	bool	batt_2s_chg;

	int	*dec_rate_seq;
	int	dec_rate_len;

	struct cold_thermal *cold_thermal_seq;
	int	cold_thermal_len;
	int	cold_thermal_level;
	bool	update_now;
	bool	fast_mode;
	int	optimiz_soc;
	bool	ffc_smooth;
	bool	batt_sw_fc;
	bool	shutdown_delay;
	bool	shutdown_delay_enable;
	bool	usb_present;

	int cell1_max;
	int max_charge_current;
	int max_discharge_current;
	int max_temp_cell;
	int min_temp_cell;
	int total_fw_runtime;
	int time_spent_in_lt;
	int time_spent_in_ht;
	int time_spent_in_ot;
};

#define bq_dbg(reason, fmt, ...)			\
	do {						\
		if (debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);	\
	} while (0)

static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "i2c read byte fail: can't read from reg 0x%02X\n", reg);
		return ret;
	} 
	*val = (u8)ret;

	return 0;
}

/*
static int __fg_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		bq_dbg(PR_REGISTER, "i2c write byte fail: can't write 0x%02X to reg 0x%02X\n",
				val, reg);
		return ret;
	}

	return 0;
}
*/

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		bq_dbg(PR_REGISTER, "i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u16)ret;

	return 0;
}

static int __fg_read_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{

	int ret;
	int i;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_read_byte_data(client, reg + i);
		if (ret < 0) {
			bq_dbg(PR_REGISTER, "i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
		buf[i] = ret;
	}

	//ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);

	return ret;
}

static int __fg_write_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{
	int ret;
	int i = 0;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
		if (ret < 0) {
			bq_dbg(PR_REGISTER, "i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return ret;
}

static int fg_read_byte(struct bq_fg_chip *bq, u8 reg, u8 *val)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

/*
static int fg_write_byte(struct bq_fg_chip *bq, u8 reg, u8 val)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}
*/

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

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_block(bq->client, reg, data, len);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static u8 checksum(u8 *data, u8 len)
{
	u8 i;
	u16 sum = 0;

	for (i = 0; i < len; i++) {
		sum += data[i];
	}

	sum &= 0xFF;

	return 0xFF - sum;
}

static void fg_print_buf(const char *msg, u8 *buf, u8 len)
{
	int i;
	int idx = 0;
	int num;
	u8 strbuf[128];

	for (i = 0; i < len; i++) {
		num = snprintf(&strbuf[idx], sizeof(strbuf), "%02x", buf[i]);
		idx += num;
	}
	bq_dbg(PR_REGISTER, "%s\n", strbuf);
}

static int fg_mac_read_block(struct bq_fg_chip *bq, u16 cmd, u8 *buf, u8 len)
{
	int ret;
	u8 t_buf[40];
	int i, t_len;
	u8 cksum_calc, cksum;

	mutex_lock(&bq->i2c_rw_lock);

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	ret = __fg_write_block(bq->client, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0) {
		mutex_unlock(&bq->i2c_rw_lock);
		return ret;
	}

	msleep(100);

	ret = __fg_read_block(bq->client, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0) {
		mutex_unlock(&bq->i2c_rw_lock);
		return ret;
	}

	fg_print_buf("mac_read_block", t_buf, 36);

	if (t_buf[0] != (u8)cmd || t_buf[1] != (u8)(cmd >> 8)) {
		mutex_unlock(&bq->i2c_rw_lock);
		return 1;
	}

	cksum = t_buf[34];
	t_len = t_buf[35];

	cksum_calc = checksum(t_buf, t_len - 2);

	if (cksum_calc != cksum) {
		mutex_unlock(&bq->i2c_rw_lock);
		return 1;
	}

	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];

	mutex_unlock(&bq->i2c_rw_lock);

	return 0;
}

static int fg_sha256_auth(struct bq_fg_chip *bq, u8 *rand_num, int length)
{
	int ret;
	u8 cksum_calc;
	u8 t_buf[2];

	/*
	1. The host writes 0x00 to 0x3E.
	2. The host writes 0x00 to 0x3F
	*/
	t_buf[0] = 0x00;
	t_buf[1] = 0x00;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;
	/*
	3. Write the random challenge should be written in a 32-byte block to address 0x40-0x5F
	*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], rand_num, length);
	if (ret < 0)
		return ret;

	/*4. Write the checksum (2’s complement sum of (1), (2), and (3)) to address 0x60.*/
	cksum_calc = checksum(rand_num, length);

	t_buf[0] = cksum_calc;
	t_buf[1] = length + 4;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);
	if (ret < 0)
		return ret;

	msleep(250);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], bq->digest, length);
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int fg_mac_write_block(struct bq_fg_chip *bq, u16 cmd, u8 *data, u8 len)
{
	int ret;
	u8 cksum;
	u8 t_buf[40];
	int i;

	if (len > 32)
		return -EINVAL;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	for (i = 0; i < len; i++)
		t_buf[i+2] = data[i];

	/*write command/addr, data*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, len + 2);
	if (ret < 0)
		return ret;

	fg_print_buf("mac_write_block", t_buf, len + 2);

	cksum = checksum(t_buf, len + 2);
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
	bq_dbg(PR_DEBUG,  "checksum:%x, len:%d\n", cksum, len);
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);

	return ret;
}
static int fg_get_fastcharge_mode(struct bq_fg_chip *bq)
{
	u8 data[3];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CHARGING_STATUS, data, 3);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
		return ret;
	}

	return (data[2] & FG_FLAGS_FASTCHAGE) >> 5;
}
#endif

static int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	int ret;

	bq->fast_mode = enable;

	return ret;
}


static int fg_read_status(struct bq_fg_chip *bq)
{
	int ret;
	u16 flags;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
	if (ret < 0)
		return ret;

	bq->batt_fc		= !!(flags & FG_FLAGS_FC);
	bq->batt_fd		= !!(flags & FG_FLAGS_FD);
	bq->batt_rca		= !!(flags & FG_FLAGS_RCA);
	bq->batt_dsg		= !!(flags & FG_FLAGS_DSG);

	return 0;
}

enum manu_macro {
	TERMINATION = 0,
	RECHARGE_VOL,
	FFC_TERMINATION,
	MANU_NAME,
	MANU_DATA_LEN,
};

#define TERMINATION_BYTE	6
#define TERMINATION_BASE	30
#define TERMINATION_STEP	5

#define RECHARGE_VOL_BYTE	7
#define RECHARGE_VOL_BASE	4200
#define RECHARGE_VOL_STEP	5

#define FFC_TERMINATION_BYTE	8
#define FFC_TERMINATION_BASE	400
#define FFC_TERMINATION_STEP	20

#define MANU_NAME_BYTE		3
#define MANU_NAME_BASE		0x0C
#define MANU_NAME_STEP		1

struct manu_data {
	int byte;
	int base;
	int step;
	int data;
};

struct manu_data manu_info[MANU_DATA_LEN] = {
	{TERMINATION_BYTE, TERMINATION_BASE, TERMINATION_STEP},
	{RECHARGE_VOL_BYTE, RECHARGE_VOL_BASE, RECHARGE_VOL_STEP},
	{FFC_TERMINATION_BYTE, FFC_TERMINATION_BASE, FFC_TERMINATION_STEP},
	{MANU_NAME, MANU_NAME_BASE, MANU_NAME_STEP},
};

static int fg_get_manu_info(unsigned char val, int base, int step)
{
	int index = 0;
	int data = 0;

	bq_dbg(PR_OEM, "val:%d, '0':%d, 'A':%d, 'a':%d\n", val, '0', 'A', 'a');
	if (val > '0' && val < '9')
		index = val - '0';
	if (val > 'A' && val < 'Z')
		index = val - 'A' + 10;
	if (val > 'a' && val < 'z')
		index = val - 'a' + 36;

	data = base + index * step;

	return data;
}

static int fg_get_manufacture_data(struct bq_fg_chip *bq)
{
	u8 t_buf[40];
	int ret;
	int i;
	int byte, base, step;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, t_buf, 32);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get MANE NAME\n");
		return -ENODEV;
	}

	if (strncmp(t_buf, "MI", 2) != 0) {
		bq_dbg(PR_OEM, "Can not get MI battery data\n");
		manu_info[TERMINATION].data = BQ27Z561_DEFUALT_TERM;
		manu_info[FFC_TERMINATION].data = BQ27Z561_DEFUALT_FFC_TERM;
		manu_info[RECHARGE_VOL].data = BQ27Z561_DEFUALT_RECHARGE_VOL;
		return 0;
	}

	for (i = 0; i < MANU_DATA_LEN; i++) {
		byte = manu_info[i].byte;
		base = manu_info[i].base;
		step = manu_info[i].step;
		manu_info[i].data = fg_get_manu_info(t_buf[byte], base, step);
	}

	return 0;
}

static int fg_read_rsoc(struct bq_fg_chip *bq, int *soc)
{
	int ret;

	if (bq->fake_soc != -EINVAL)
		return bq->fake_soc;

	ret = fg_read_byte(bq, bq->regs[BQ_FG_REG_SOC], (u8 *)soc);
	if (ret < 0) {
		*soc = BQ_I2C_FAILED_SOC;
		return ret;
	}

	return ret;
}

static int fg_read_system_soc(struct bq_fg_chip *bq)
{
	int batt_soc;

	fg_read_rsoc(bq, &batt_soc);

	return batt_soc;
}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	int ret;
	u16 temp = 0;
	static int last_temp;

	if (bq->fake_temp != -EINVAL)
		return bq->fake_temp;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &temp);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read temperature, ret = %d\n", ret);
		return BQ_I2C_FAILED_TEMP;
	}
	last_temp = temp - 2730;

	return temp - 2730;

}

static int fg_read_volt(struct bq_fg_chip *bq)
{
	int ret;
	u16 volt = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &volt);
	if (ret < 0) {
		volt = BQ_I2C_FAILED_VOL;
	}

	return volt;
}

static int fg_read_avg_current(struct bq_fg_chip *bq, int *curr)
{
	int ret;
	s16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], (u16 *)&avg_curr);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read current, ret = %d\n", ret);
		return ret;
	}
	*curr = -1 * avg_curr;

	return ret;
}

static int fg_read_current(struct bq_fg_chip *bq)
{
	int ret;
	s16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CN], (u16 *)&avg_curr);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read current, ret = %d\n", ret);
		return ret;
	}

	return  avg_curr * (-1) ;
}

static int fg_read_fcc(struct bq_fg_chip *bq)
{
	int ret;
	u16 fcc;

	if (bq->regs[BQ_FG_REG_FCC] == INVALID_REG_ADDR) {
		bq_dbg(PR_DEBUG, "FCC command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);

	if (ret < 0)
		bq_dbg(PR_DEBUG, "could not read FCC, ret=%d\n", ret);

	return fcc;
}

static int fg_read_rm(struct bq_fg_chip *bq)
{
	int ret;
	u16 rm;

	if (bq->regs[BQ_FG_REG_RM] == INVALID_REG_ADDR) {
		bq_dbg(PR_DEBUG, "RemainingCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);

	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read RM, ret=%d\n", ret);
		return ret;
	}

	return rm;
}

static int fg_get_raw_soc(struct bq_fg_chip *bq)
{
	int raw_soc, ret;
	u16 rm, fcc;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read RM, ret=%d\n", ret);
		return BQ_I2C_FAILED_SOC * 100;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read FCC, ret=%d\n", ret);
		return BQ_I2C_FAILED_SOC * 100;
	}

	raw_soc = rm * 10000 / fcc;

	return raw_soc;
}

static int fg_read_soh(struct bq_fg_chip *bq)
{
	int ret;
	u16 soh;

	if (bq->regs[BQ_FG_REG_SOH] == INVALID_REG_ADDR) {
		bq_dbg(PR_DEBUG, "SOH command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read DC, ret=%d\n", ret);
		return ret;
	}

	return soh;
}

static int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CC] == INVALID_REG_ADDR) {
		bq_dbg(PR_DEBUG, "Cycle Count not supported!\n");
		return -EINVAL;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);

	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read Cycle Count, ret=%d\n", ret);
		return ret;
	}

	return cc;
}

static int fg_get_batt_status(struct bq_fg_chip *bq)
{

	fg_read_status(bq);

	if (bq->batt_sw_fc)
		return POWER_SUPPLY_STATUS_FULL;
	else if (bq->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (bq->batt_curr < 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

}

static int fg_get_batt_capacity_level(struct bq_fg_chip *bq)
{

	if (bq->batt_fc)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (bq->batt_rca)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (bq->batt_fd)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

}

static int fg_get_soc_decimal_rate(struct bq_fg_chip *bq)
{
	int soc, i;

	if (bq->dec_rate_len <= 0)
		return 0;

	soc = bq->batt_soc;

	for (i = 0; i < bq->dec_rate_len; i += 2) {
		if (soc < bq->dec_rate_seq[i]) {
			return bq->dec_rate_seq[i - 1];
		}
	}

	return bq->dec_rate_seq[bq->dec_rate_len - 1];
}

static int fg_get_soc_decimal(struct bq_fg_chip *bq)
{
	if (!bq)
		return 0;

	return bq->raw_soc % 100;
}

static int fg_get_cold_thermal_level(struct bq_fg_chip *bq)
{
	union power_supply_propval pval = {0, };
	int curr, i, rc, temp, volt, status;

	if (!bq)
		return -EINVAL;

	if (!bq->cold_thermal_len)
		return 0;

	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return 0;
		}
	}

	rc = power_supply_get_property(bq->batt_psy,
		POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "failed get batt staus\n");
		return -EINVAL;
	}

	status = pval.intval;
	temp = bq->batt_temp;
	volt = bq->batt_volt;

	if (status == POWER_SUPPLY_STATUS_CHARGING ||
			temp > 100 || volt > 3500000)
		return 0;

	fg_read_avg_current(bq, &curr);

	for (i = 0; i < bq->cold_thermal_len; i++) {
		if (temp > bq->cold_thermal_seq[i].temp_l &&
				temp <= bq->cold_thermal_seq[i].temp_h &&
				curr > bq->cold_thermal_seq[i].curr_th) {
			bq_dbg(PR_OEM, "cold thermal trigger status:%d, temp:%d, volt:%d\n",
					status, temp, volt);
			bq_dbg(PR_OEM, "curr:%d, bq->cold_thermal_seq[i].index:%d\n", curr,
					bq->cold_thermal_seq[i].index);
			return bq->cold_thermal_seq[i].index;
		}
	}

	return 0;
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_SOC_DECIMAL,
	POWER_SUPPLY_PROP_SOC_DECIMAL_RATE,
	POWER_SUPPLY_PROP_COLD_THERMAL_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	/*POWER_SUPPLY_PROP_HEALTH,*//*implement it in battery power_supply*/
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_UPDATE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TERMINATION_CURRENT,
	POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT,
	POWER_SUPPLY_PROP_RECHARGE_VBAT,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_SOH,
};
#define SHUTDOWN_DELAY_VOL	6600
static int fg_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
	int ret, status;
	u16 flags;
	int vbat_mv;
	static bool shutdown_delay_cancel;
	static bool last_shutdown_delay;
	union power_supply_propval pval = {0, };
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_get_batt_status(bq);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (bq->fake_volt != -EINVAL) {
			val->intval = bq->fake_volt;
			break;
		}
		bq->batt_volt = fg_read_volt(bq);
		val->intval = bq->batt_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq->batt_curr * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc >= 0) {
			val->intval = bq->fake_soc;
			break;
		}
		val->intval = bq->batt_soc;
		//add shutdown delay feature
		if (bq->shutdown_delay_enable) {
			if (val->intval == 0) {
				vbat_mv = fg_read_volt(bq);
				if (bq->batt_psy) {
					power_supply_get_property(bq->batt_psy,
						POWER_SUPPLY_PROP_STATUS, &pval);
					status = pval.intval;
				}
				if (vbat_mv > SHUTDOWN_DELAY_VOL
					&& status != POWER_SUPPLY_STATUS_CHARGING) {
					bq->shutdown_delay = true;
					val->intval = 1;
				} else if (status == POWER_SUPPLY_STATUS_CHARGING
								&& bq->shutdown_delay) {
					bq->shutdown_delay = false;
					shutdown_delay_cancel = true;
					val->intval = 1;
				} else {
					bq->shutdown_delay = false;
					if (shutdown_delay_cancel)
						val->intval = 1;
				}
			} else {
				bq->shutdown_delay = false;
				shutdown_delay_cancel = false;
			}

			if (last_shutdown_delay != bq->shutdown_delay) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->fg_psy)
					power_supply_changed(bq->fg_psy);
			}
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(bq);
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = bq->shutdown_delay;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		val->intval = bq->raw_soc;
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
		val->intval = bq->soc_decimal;
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
		val->intval = bq->soc_decimal_rate;
		break;
	case POWER_SUPPLY_PROP_COLD_THERMAL_LEVEL:
		val->intval = bq->cold_thermal_level;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq->batt_temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = bq->batt_fcc * 1000 * 2;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq->batt_dc * 2;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = bq->batt_cyclecnt;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = bq->batt_resistance;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = 100000;
		break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (bq->batt_2s_chg) {
			val->intval = 12000000;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (bq->batt_2s_chg) {
			val->intval = 8900000;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = bq->verify_digest_success;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		if (bq->fake_chip_ok != -EINVAL) {
			val->intval = bq->fake_chip_ok;
			break;
		}
		ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
		if (ret < 0)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = bq->constant_charge_current_max;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		val->intval = bq->fast_mode;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = bq->batt_fcc * 1000;
		break;
	case POWER_SUPPLY_PROP_TERMINATION_CURRENT:
		val->intval = bq->term_curr;
		break;
	case POWER_SUPPLY_PROP_FFC_TERMINATION_CURRENT:
		val->intval = bq->ffc_term_curr;
		break;
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		val->intval = bq->recharge_vol;
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		val->intval = bq->batt_sw_fc;
		break;
	case POWER_SUPPLY_PROP_SOH:
		val->intval = bq->soh;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_set_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       const union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		bq->fake_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq->fake_soc = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		bq->optimiz_soc = val->intval;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		bq->verify_digest_success = !!val->intval;
		if (!bq->fcc_votable)
			bq->fcc_votable = find_votable("FCC");
		vote(bq->fcc_votable, BMS_FG_VERIFY, !bq->verify_digest_success,
			!bq->verify_digest_success ? 2000000 : 0);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		bq->fake_chip_ok = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		bq->constant_charge_current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(bq, !!val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		bq->fake_volt = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int fg_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_UPDATE_NOW:
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_CHIP_OK:
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}



static int fg_psy_register(struct bq_fg_chip *bq)
{
	struct power_supply_config fg_psy_cfg = {};

	bq->fg_psy_d.name = "bms";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BMS;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_get_property;
	bq->fg_psy_d.set_property = fg_set_property;
	bq->fg_psy_d.property_is_writeable = fg_prop_is_writeable;

	fg_psy_cfg.drv_data = bq;
	fg_psy_cfg.num_supplicants = 0;
	bq->fg_psy = devm_power_supply_register(bq->dev,
						&bq->fg_psy_d,
						&fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		bq_dbg(PR_OEM, "Failed to register fg_psy");
		return PTR_ERR(bq->fg_psy);
	}

	return 0;
}


static void fg_psy_unregister(struct bq_fg_chip *bq)
{

	power_supply_unregister(bq->fg_psy);
}

static const u8 fg_dump_regs[] = {
	0x00, 0x02, 0x04, 0x06,
	0x08, 0x0A, 0x0C, 0x0E,
	0x10, 0x16, 0x18, 0x1A,
	0x1C, 0x1E, 0x20, 0x28,
	0x2A, 0x2C, 0x2E, 0x30,
};

static ssize_t fg_attr_show_Qmax(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	if (!bq->verify_digest_success)
		return 0;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS3, t_buf, 2);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n", (t_buf[1] << 8) | t_buf[0]);

	return len;
}

static ssize_t fg_attr_show_fcc_soh(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	if (!bq->verify_digest_success)
		return 0;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_FCC_SOH, t_buf, 2);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n", (t_buf[1] << 8) | t_buf[0]);

	return len;
}

static ssize_t fg_attr_show_rm(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int rm, len;

	rm = fg_read_rm(bq);
	len = snprintf(buf, 1024, "%d\n", rm);

	return len;
}

static ssize_t fg_attr_show_fcc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int fcc, len;

	fcc = fg_read_fcc(bq);
	len = snprintf(buf, 1024, "%d\n", fcc);

	return len;
}

static ssize_t fg_attr_show_soh(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int soh, len;

	soh = fg_read_soh(bq);
	len = snprintf(buf, 1024, "%d\n", soh);

	return len;
}

static ssize_t fg_attr_show_batt_volt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret, len;
	int cell1, cell2, pack, bat;
	u8 t_buf[40];

	if (!bq->verify_digest_success)
		return 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, t_buf, 8);
	if (ret < 0)
		return 0;

	cell1 = (t_buf[1] << 8) | t_buf[0];
	cell2 = (t_buf[3] << 8) | t_buf[2];
	pack = (t_buf[5] << 8) | t_buf[4];
	bat = (t_buf[7] << 8) | t_buf[6];

	len = snprintf(buf, 1024, "%d,%d,%d,%d\n", cell1, cell2, pack, bat);

	return len;
}

#define ITSTATUS1_DATA_LEN	24
static ssize_t fg_attr_show_itstatus1(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret, len, i;
	u8 t_buf[40], strbuf[4];

	if (!bq->verify_digest_success)
		return 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, ITSTATUS1_DATA_LEN);
	if (ret < 0)
		return 0;

	for (i = 0; i < ITSTATUS1_DATA_LEN; i++) {
		memset(strbuf, 0, sizeof(strbuf));
		snprintf(strbuf, sizeof(strbuf) - 1, "%02x", t_buf[i]);
		strlcat(buf, strbuf, ITSTATUS1_DATA_LEN * 2 + 1);
	}

	len = strlen(buf);
	buf[len] = '\n';
	return strlen(buf) + 1;
}

#define ITSTATUS2_DATA_LEN	28
static ssize_t fg_attr_show_itstatus2(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret, len, i;
	u8 t_buf[40], strbuf[4];

	if (!bq->verify_digest_success)
		return 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS2, t_buf, ITSTATUS2_DATA_LEN);
	if (ret < 0)
		return 0;

	for (i = 0; i < ITSTATUS2_DATA_LEN; i++) {
		memset(strbuf, 0, sizeof(strbuf));
		snprintf(strbuf, sizeof(strbuf) - 1, "%02x", t_buf[i]);
		strlcat(buf, strbuf, ITSTATUS2_DATA_LEN * 2 + 1);
	}

	len = strlen(buf);
	buf[len] = '\n';
	return strlen(buf) + 1;
}

#define ITSTATUS3_DATA_LEN	20
static ssize_t fg_attr_show_itstatus3(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret, len, i;
	u8 t_buf[40], strbuf[4];

	if (!bq->verify_digest_success)
		return 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS3, t_buf, ITSTATUS3_DATA_LEN);
	if (ret < 0)
		return 0;

	for (i = 0; i < ITSTATUS3_DATA_LEN; i++) {
		memset(strbuf, 0, sizeof(strbuf));
		snprintf(strbuf, sizeof(strbuf) - 1, "%02x", t_buf[i]);
		strlcat(buf, strbuf, ITSTATUS3_DATA_LEN * 2 + 1);
	}

	len = strlen(buf);
	buf[len] = '\n';
	return strlen(buf) + 1;
}

#define CBSTATUS_DATA_LEN	10
static ssize_t fg_attr_show_cbstatus(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret, len, i;
	u8 t_buf[40], strbuf[4];

	if (!bq->verify_digest_success)
		return 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CBSTATUS, t_buf, CBSTATUS_DATA_LEN);
	if (ret < 0)
		return 0;

	for (i = 0; i < CBSTATUS_DATA_LEN; i++) {
		memset(strbuf, 0, sizeof(strbuf));
		snprintf(strbuf, sizeof(strbuf) - 1, "%02x", t_buf[i]);
		strlcat(buf, strbuf, CBSTATUS_DATA_LEN * 2 + 1);
	}

	len = strlen(buf);
	buf[len] = '\n';
	return strlen(buf) + 1;
}

static ssize_t verify_digest_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 digest_buf[4];
	int i;
	int len;

	for (i = 0; i < BATTERY_DIGEST_LEN; i++) {
		memset(digest_buf, 0, sizeof(digest_buf));
		snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bq->digest[i]);
		strlcat(buf, digest_buf, BATTERY_DIGEST_LEN * 2 + 1);
	}
	len = strlen(buf);
	buf[len] = '\0';
	return strlen(buf) + 1;
}

int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
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

ssize_t verify_digest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int i;
	u8 random[BATTERY_DIGEST_LEN] = {0};
	char kbuf[2 * BATTERY_DIGEST_LEN + 1] = {0};

	memset(kbuf, 0, sizeof(kbuf));
	strlcpy(kbuf, buf, 2 * BATTERY_DIGEST_LEN + 1);

	StringToHex(kbuf, random, &i);
	fg_sha256_auth(bq, random, BATTERY_DIGEST_LEN);

	return count;
}

static DEVICE_ATTR(verify_digest, 0660, verify_digest_show, verify_digest_store);
static DEVICE_ATTR(Qmax, S_IRUGO, fg_attr_show_Qmax, NULL);
static DEVICE_ATTR(fcc_soh, S_IRUGO, fg_attr_show_fcc_soh, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_attr_show_rm, NULL);
static DEVICE_ATTR(fcc, S_IRUGO, fg_attr_show_fcc, NULL);
static DEVICE_ATTR(soh, S_IRUGO, fg_attr_show_soh, NULL);
static DEVICE_ATTR(batt_volt, S_IRUGO, fg_attr_show_batt_volt, NULL);
static DEVICE_ATTR(itstatus1, S_IRUGO, fg_attr_show_itstatus1, NULL);
static DEVICE_ATTR(itstatus2, S_IRUGO, fg_attr_show_itstatus2, NULL);
static DEVICE_ATTR(itstatus3, S_IRUGO, fg_attr_show_itstatus3, NULL);
static DEVICE_ATTR(cbstatus, S_IRUGO, fg_attr_show_cbstatus, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_Qmax.attr,
	&dev_attr_fcc_soh.attr,
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_soh.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_batt_volt.attr,
	&dev_attr_itstatus1.attr,
	&dev_attr_itstatus2.attr,
	&dev_attr_itstatus3.attr,
	&dev_attr_cbstatus.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};


#if 0
static int fg_dump_registers(struct bq_fg_chip *bq)
{
	int i;
	int ret;
	u16 val;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		ret = fg_read_word(bq, fg_dump_regs[i], &val);
		if (!ret)
			bq_dbg(PR_REGISTER, "Reg[%02X] = 0x%04X\n", fg_dump_regs[i], val);
	}

	return ret;
}
#endif

#define BQ28Z610_FFC_TERM_WAM_TEMP		350
#define BQ28Z610_COLD_TEMP_TERM			  0
#define BQ28Z610_FFC_FULL_FV			8940
#define BQ28Z610_NOR_FULL_FV			8880
#define BAT_FULL_CHECK_TIME	1

static int fg_check_full_status(struct bq_fg_chip *bq)
{
	union power_supply_propval prop = {0, };
	static int last_term, full_check;
	int term_curr, full_volt, rc;
	int interval = MONITOR_WORK_10S;


	if (!bq->usb_psy)
		return interval;

	if (!bq->chg_dis_votable)
		bq->chg_dis_votable = find_votable("CHG_DISABLE");

	if (!bq->fv_votable)
		bq->fv_votable = find_votable("BBC_FV");

	rc = power_supply_get_property(bq->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &prop);
	if (!prop.intval) {
		vote(bq->chg_dis_votable, BMS_FC_VOTER, false, 0);
		bq->batt_sw_fc = false;
		full_check = 0;
		return interval;
	}

	if (bq->fast_mode) {
		if (bq->batt_temp > BQ28Z610_FFC_TERM_WAM_TEMP) {
			bq->ffc_term_curr = bq->ffc_warm_term;
		} else {
			bq->ffc_term_curr = bq->ffc_normal_term;
		}
		term_curr = bq->ffc_term_curr;
		interval = MONITOR_WORK_1S;
	} else {
		if (bq->batt_temp < BQ28Z610_COLD_TEMP_TERM) {
			bq->term_curr = bq->cold_term;
		} else
			bq->term_curr = bq->normal_term;
		term_curr = bq->term_curr;
		if (bq->usb_present)
			interval = MONITOR_WORK_5S;
		else
			interval = MONITOR_WORK_10S;
	}
	full_volt = get_effective_result(bq->fv_votable) / 1000 - 20;

	bq_dbg(PR_OEM, "term:%d, full_volt:%d, usb_present:%d, batt_sw_fc:%d",
			term_curr, full_volt, bq->usb_present, bq->batt_sw_fc);

	if (bq->usb_present && bq->raw_soc == BQ_RAW_SOC_FULL && bq->batt_volt > full_volt &&
			bq->batt_curr < 0 && (bq->batt_curr > term_curr * (-1)) &&
			!bq->batt_sw_fc) {
		full_check++;
		bq_dbg(PR_OEM, "full_check:%d\n", full_check);
		if (full_check > BAT_FULL_CHECK_TIME) {
			bq->batt_sw_fc = true;
			vote(bq->chg_dis_votable, BMS_FC_VOTER, true, 0);
			bq_dbg(PR_OEM, "detect charge termination bq->batt_sw_fc:%d\n", bq->batt_sw_fc);
		}
		return MONITOR_WORK_1S;
	} else {
		full_check = 0;
	}

	if (term_curr == last_term)
		return interval;

	if (!bq->bbc_psy)
		bq->bbc_psy = power_supply_get_by_name("bbc");
	if (bq->bbc_psy) {
		prop.intval = term_curr;
		bq_dbg(PR_OEM, "bq dymanic set term curr:%d\n", term_curr);
		rc = power_supply_set_property(bq->bbc_psy,
				POWER_SUPPLY_PROP_TERMINATION_CURRENT, &prop);
		if (rc < 0) {
			bq_dbg(PR_OEM, "bq could not set termi current!\n");
			return interval;
		}
	}
	last_term = term_curr;

	return interval;
}

static void fg_update_status(struct bq_fg_chip *bq)
{
	static int last_st, last_soc, last_temp;

	mutex_lock(&bq->data_lock);

	bq->batt_curr = fg_read_current(bq);
	bq->batt_cyclecnt = fg_read_cyclecount(bq);
	bq->batt_temp = fg_read_temperature(bq);
	bq->batt_volt = fg_read_volt(bq);
	bq->batt_rm = fg_read_rm(bq);
	bq->batt_fcc = fg_read_fcc(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	bq->soc_decimal = fg_get_soc_decimal(bq);
	bq->soc_decimal_rate = fg_get_soc_decimal_rate(bq);
	bq->cold_thermal_level = fg_get_cold_thermal_level(bq);
	bq->batt_soc = fg_read_system_soc(bq);
	bq->soh = fg_read_soh(bq);

	mutex_unlock(&bq->data_lock);

	bq_dbg(PR_OEM, "SOC:%d,Volt:%d,Cur:%d,Temp:%d,RM:%d,FC:%d,RAW_SOC:%d,SOH:%d,FAST:%d",
			bq->batt_soc, bq->batt_volt, bq->batt_curr,
			bq->batt_temp, bq->batt_rm, bq->batt_fcc,
			bq->raw_soc, bq->soh, bq->fast_mode);

	if ((last_soc != bq->batt_soc) || (last_temp != bq->batt_temp)
			|| (last_st != bq->batt_st)) {
		if (bq->fg_psy)
			power_supply_changed(bq->fg_psy);
	}

	last_soc = bq->batt_soc;
	last_temp = bq->batt_temp;
	last_st = bq->batt_st;
}

static int fg_check_recharge_status(struct bq_fg_chip *bq)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return 0;
		}
	}

	rc = power_supply_get_property(bq->batt_psy,
		POWER_SUPPLY_PROP_HEALTH, &prop);
	bq->health = prop.intval;

	if ((bq->raw_soc <= BQ_RECHARGE_SOC) && bq->batt_sw_fc &&
			bq->health != POWER_SUPPLY_HEALTH_WARM) {
		bq->batt_sw_fc = false;
		prop.intval = true;
		vote(bq->chg_dis_votable, BMS_FC_VOTER, false, 0);
		rc = power_supply_set_property(bq->batt_psy,
				POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
		if (rc < 0) {
			bq_dbg(PR_OEM, "bq could not set force recharging!\n");
			return rc;
		}
	}

	return 0;
}

static int calc_suspend_time(struct timeval *time_start, int *delta_time)
{
	struct timeval time_now;

	*delta_time = 0;

	do_gettimeofday(&time_now);
	*delta_time = (time_now.tv_sec - time_start->tv_sec);
	if (*delta_time < 0)
		*delta_time = 0;

	return 0;
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);
	int interval;

	fg_update_status(bq);
	interval = fg_check_full_status(bq);
	fg_check_recharge_status(bq);

	schedule_delayed_work(&bq->monitor_work, interval * HZ);
}

static int bq_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret, size;

	ret = of_property_read_u32(node, "bq,charge-full-design",
			&bq->batt_dc);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get bq,charge-full-designe\n");
		bq->batt_dc = DEFUALT_FULL_DESIGN;
		return ret;
	}

	ret = of_property_read_u32(node, "bq,ffc-warm-term",
			&bq->ffc_warm_term);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get ffc term curr\n");
		bq->ffc_warm_term = DEFUALT_FFC_TERM_CURR;
	}

	ret = of_property_read_u32(node, "bq,ffc-normal-term",
			&bq->ffc_normal_term);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get ffc term curr\n");
		bq->ffc_normal_term = DEFUALT_FFC_TERM_CURR;
	}


	ret = of_property_read_u32(node, "bq,cold-term",
			&bq->cold_term);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get cold term curr\n");
		bq->cold_term = DEFUALT_TERM_CURR;
	}

	ret = of_property_read_u32(node, "bq,normal-term",
			&bq->normal_term);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get normal term curr\n");
		bq->normal_term = DEFUALT_TERM_CURR;
	}

	ret = of_property_read_u32(node, "bq,recharge-voltage",
			&bq->recharge_vol);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get bq,recharge-voltage\n");
		bq->recharge_vol = -EINVAL;
		return ret;
	}

	bq->ignore_digest_for_debug = of_property_read_bool(node,
				"bq,ignore-digest-debug");
	bq->shutdown_delay_enable = of_property_read_bool(node,
						"bq,shutdown-delay-enable");
	bq->batt_2s_chg = of_property_read_bool(node,
				"qcom,2s-battery-charging");

	size = 0;
	of_get_property(node, "bq,soc_decimal_rate", &size);
	if (size) {
		bq->dec_rate_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->dec_rate_seq) {
			bq->dec_rate_len =
				(size / sizeof(*bq->dec_rate_seq));
			if (bq->dec_rate_len % 2) {
				bq_dbg(PR_OEM, "invalid soc decimal rate seq\n");
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"bq,soc_decimal_rate",
					bq->dec_rate_seq,
					bq->dec_rate_len);
		} else {
			bq_dbg(PR_OEM, "error allocating memory for dec_rate_seq\n");
		}
	}

	size = 0;
	of_get_property(node, "bq,cold_thermal_seq", &size);
	if (size) {
		bq->cold_thermal_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->cold_thermal_seq) {
			bq->cold_thermal_len =
				(size / sizeof(int));
			if (bq->cold_thermal_len % 4) {
				bq_dbg(PR_OEM, "invalid cold thermal seq\n");
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"bq,cold_thermal_seq",
					(int *)bq->cold_thermal_seq,
					bq->cold_thermal_len);
			bq->cold_thermal_len = bq->cold_thermal_len / 4;
		} else {
			bq_dbg(PR_OEM, "error allocating memory for cold thermal seq\n");
		}
	}



	return 0;
}

static struct regmap_config i2c_bq27z561_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFFFF,
};

static int bq27z561_get_psy(struct bq_fg_chip *bq)
{

	if (!bq->usb_psy || !bq->batt_psy)
		return -EINVAL;

	bq->usb_psy = power_supply_get_by_name("usb");
	if (!bq->usb_psy) {
		bq_dbg(PR_OEM, "USB supply not found, defer probe\n");
		return -EINVAL;
	}

	bq->batt_psy = power_supply_get_by_name("battery");
	if (!bq->batt_psy) {
		bq_dbg(PR_OEM, "bms supply not found, defer probe\n");
		return -EINVAL;
	}

	return 0;
}

static int bq27z561_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct bq_fg_chip *bq = container_of(nb, struct bq_fg_chip, nb);
	union power_supply_propval pval = {0, };
	int rc;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	rc = bq27z561_get_psy(bq);
	if (rc < 0) {
		return NOTIFY_OK;
	}

	if (strcmp(psy->desc->name, "usb") != 0)
		return NOTIFY_OK;

	if (bq->usb_psy) {
		rc = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0) {
			bq_dbg(PR_OEM, "failed get usb present\n");
			return -EINVAL;
		}
		if (pval.intval) {
			bq->usb_present = true;
			pm_stay_awake(bq->dev);
		} else {
			bq->batt_sw_fc = false;
			bq->usb_present = false;
			pm_relax(bq->dev);
		}
	}

	return NOTIFY_OK;
}

static int bq_fg_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;
	struct bq_fg_chip *bq;
	u8 *regs;

	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_DMA);

	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	bq->chip = id->driver_data;
	bq->model_name = (char *)id->name;

	bq->batt_soc	= -ENODATA;
	bq->batt_fcc	= -ENODATA;
	bq->batt_rm	= -ENODATA;
	bq->batt_dc	= -ENODATA;
	bq->batt_volt	= -ENODATA;
	bq->batt_temp	= -ENODATA;
	bq->batt_curr	= -ENODATA;
	bq->batt_cyclecnt = -ENODATA;
	bq->batt_tte = -ENODATA;
	bq->raw_soc = -ENODATA;
	bq->last_soc = -EINVAL;

	bq->fake_soc	= -EINVAL;
	bq->fake_temp	= -EINVAL;
	bq->fake_volt	= -EINVAL;
	bq->fake_chip_ok = -EINVAL;
	bq->ffc_term_curr = DEFUALT_FFC_TERM_CURR;
	bq->term_curr = DEFUALT_TERM_CURR;
	if (bq->chip == BQ27Z561) {
		regs = bq27z561_regs;
	} else {
		bq_dbg(PR_OEM, "unexpected fuel gauge: %d\n", bq->chip);
		regs = bq27z561_regs;
	}
	memcpy(bq->regs, regs, NUM_REGS);

	i2c_set_clientdata(client, bq);

	bq_parse_dt(bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	device_init_wakeup(bq->dev, 1);

	ret = fg_get_manufacture_data(bq);
	bq->regmap = devm_regmap_init_i2c(client, &i2c_bq27z561_regmap_config);
	if (!bq->regmap)
		return -ENODEV;

	fg_set_fastcharge_mode(bq, false);
	fg_psy_register(bq);
	fg_update_status(bq);

	bq->nb.notifier_call = bq27z561_notifier_call;
	ret = power_supply_reg_notifier(&bq->nb);
	if (ret < 0) {
		bq_dbg(PR_OEM, "Couldn't register psy notifier rc = %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret)
		bq_dbg(PR_OEM, "Failed to register sysfs:%d\n", ret);

	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, 10 * HZ);

	bq_dbg(PR_OEM, "bq fuel gauge probe successfully, %s\n",
			device2str[bq->chip]);

	return 0;
}


static int bq_fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bq->monitor_work);
	bq->skip_reads = true;
	do_gettimeofday(&bq->suspend_time);

	return 0;
}
#define BQ_RESUME_UPDATE_TIME	600

static int bq_fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int delta_time;

	bq->skip_reads = false;
	calc_suspend_time(&bq->suspend_time, &delta_time);

	if (delta_time > BQ_RESUME_UPDATE_TIME) {
		bq_dbg(PR_OEM, "suspend more than %d, update soc now\n",
				BQ_RESUME_UPDATE_TIME);
		bq->update_now = true;
	}

	schedule_delayed_work(&bq->monitor_work, HZ);

	return 0;
}

static int bq_fg_remove(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	fg_psy_unregister(bq);

	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &fg_attr_group);

	return 0;

}

static void bq_fg_shutdown(struct i2c_client *client)
{
	bq_dbg(PR_OEM, "bq fuel gauge driver shutdown!\n");
}

static struct of_device_id bq_fg_match_table[] = {
	{.compatible = "ti,bq27z561",},
	{.compatible = "ti,bq28z610",},
	{},
};
MODULE_DEVICE_TABLE(of, bq_fg_match_table);

static const struct i2c_device_id bq_fg_id[] = {
	{ "bq27z561", BQ27Z561 },
	{ "bq28z610", BQ28Z610 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq_fg_id);

static const struct dev_pm_ops bq_fg_pm_ops = {
	.resume		= bq_fg_resume,
	.suspend	= bq_fg_suspend,
};

static struct i2c_driver bq_fg_driver = {
	.driver	= {
		.name   = "bq_fg",
		.owner  = THIS_MODULE,
		.of_match_table = bq_fg_match_table,
		.pm     = &bq_fg_pm_ops,
	},
	.id_table       = bq_fg_id,

	.probe          = bq_fg_probe,
	.remove		= bq_fg_remove,
	.shutdown	= bq_fg_shutdown,

};

module_i2c_driver(bq_fg_driver);

MODULE_DESCRIPTION("TI BQ27Z561 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");

