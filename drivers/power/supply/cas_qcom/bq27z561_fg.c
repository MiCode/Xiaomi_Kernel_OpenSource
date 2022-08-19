/*
 * bq27z561 fuel gauge driver
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

#define QUICK_CHARGE_SUPER	4

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
	int batt_ttf;
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
	int cell1;
	int cell2;
	int cell_delta;

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
	struct votable *cp_disable_votable;
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

static int bq_battery_soc_smooth_tracking(struct bq_fg_chip *chip,
		int raw_soc, int soc, int temp, int curr);
static int calc_delta_time(ktime_t time_last, int *delta_time);

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

static int __fg_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;
	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0) {
		bq_dbg(PR_OEM, "i2c write word fail: can't write 0x%02X to reg 0x%02X\n",
				val, reg);
		return ret;
	}

	return 0;
}

static int fg_write_word(struct bq_fg_chip *bq, u8 reg, u16 val)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_word(bq->client, reg, val);
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
	bq_dbg(PR_OEM, "%s\n", strbuf);
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

	mutex_lock(&bq->data_lock);
	/*
	1. The host writes 0x00 to 0x3E.
	2. The host writes 0x00 to 0x3F
	*/
	t_buf[0] = 0x00;
	t_buf[1] = 0x00;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		goto end;
	/*
	3. Write the random challenge should be written in a 32-byte block to address 0x40-0x5F
	*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], rand_num, length);
	if (ret < 0)
		goto end;

	/*4. Write the checksum (2’s complement sum of (1), (2), and (3)) to address 0x60.*/
	cksum_calc = checksum(rand_num, length);

	t_buf[0] = cksum_calc;
	t_buf[1] = length + 4;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);
	if (ret < 0)
		goto end;

	msleep(250);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], bq->digest, length);
	if (ret < 0)
		goto end;
end:
	mutex_unlock(&bq->data_lock);

	return ret;
}

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
	bq_dbg(PR_OEM,  "checksum:%x, len:%d\n", cksum, len);

	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);

	return ret;
}

#if 0
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
static int fg_get_seal_state(struct bq_fg_chip *bq)
{
	int ret;
	u8 t_buf[40] = {0, };
	int status;

	ret = fg_mac_read_block(bq, 0x0054, t_buf, 32);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get 0x0002\n");
		return -EINVAL;
	}
	status = t_buf[1];

	if (status == 1)
		bq->seal_state = SEAL_STATE_FA;
	else if (status == 2)
		bq->seal_state = SEAL_STATE_UNSEALED;
	else if (status == 3)
		bq->seal_state = SEAL_STATE_SEALED;
	bq_dbg(PR_OEM, "status:%x, bq->seal_state:%x\n", status, bq->seal_state);

	return 0;
}

static int fg_unseal_send_key(struct bq_fg_chip *bq, int key)
{
	int ret;

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], key & 0xFFFF);

	if (ret < 0) {
		bq_dbg(PR_OEM, "unable to write unseal key step 1, ret = %d\n", ret);
		return ret;
	}

	msleep(5);

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], (key >> 16) & 0xFFFF);
	if (ret < 0) {
		bq_dbg(PR_OEM, "unable to write unseal key step 2, ret = %d\n", ret);
		return ret;
	}

	msleep(100);

	return 0;
}

#define FG_DEFAULT_UNSEAL_KEY	0xCF235964

static int fg_unseal(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_unseal_send_key(bq, FG_DEFAULT_UNSEAL_KEY);
	if (!ret) {
		while (retry++ < 100) {
			ret = fg_get_seal_state(bq);
			if (bq->seal_state == SEAL_STATE_UNSEALED ||
					bq->seal_state == SEAL_STATE_FA) {
				bq_dbg(PR_OEM, "FG is unsealed");
				return 0;
			}
		}
	}

	return -1;
}

static int fg_seal(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], FG_MAC_CMD_SEAL);

	if (ret < 0) {
		bq_dbg(PR_OEM, "Failed to send seal command\n");
		return ret;
	}

	while (retry++ < 100) {
		fg_get_seal_state(bq);
		if (bq->seal_state == SEAL_STATE_SEALED) {
			bq_dbg(PR_OEM, "FG is sealed successfully");
			return 0;
		}
		msleep(200);
	}

	return -1;
}

#define BQ28Z610_NEED_UPDATE_FW1	0x7165
#define BQ28Z610_NEED_UPDATE_FW2	0x404B
#define BQ28Z610_NEED_UPDATE_FW3	0x07AF

#define BQ28Z610_UPDATE_DONE_FW1	0x4537
#define BQ28Z610_UPDATE_DONE_FW3	0x33FD

static int fg_disable_sleep_mode(struct bq_fg_chip *bq)
{
	u8 t_buf[40] = {0, };
	int ret;
	int static_df;
	u8 data[2] = {0, };

	ret = fg_mac_read_block(bq, 0x0005, t_buf, 32);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get static df\n");
		return -EINVAL;
	}
	static_df = t_buf[1] << 8 | t_buf[0];
	bq_dbg(PR_OEM, "static_df:%x \n", static_df);
	if (static_df == BQ28Z610_UPDATE_DONE_FW1 ||
			static_df == BQ28Z610_UPDATE_DONE_FW3) {
		bq_dbg(PR_OEM, "fw is new\n");
		return 0;
	} else if (static_df == BQ28Z610_NEED_UPDATE_FW1 ||
			static_df == BQ28Z610_NEED_UPDATE_FW2 ||
			static_df == BQ28Z610_NEED_UPDATE_FW3) {
		ret = fg_unseal(bq);
		if (ret < 0) {
			bq_dbg(PR_OEM, "failed to unseal\n");
			return 0;
		}
	} else
		return 0;

	/*disable sleep mode*/
	data[0] = true;
	ret = fg_mac_write_block(bq, 0x469B, data, 1);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to disable sleep mode\n");
		goto seal;
	}
	msleep(100);

	/*warm temp to 45->48*/
	data[0] = 0x30;
	ret = fg_mac_write_block(bq, 0x4663, data, 1);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to disable sleep mode\n");
		goto seal;
	}
	msleep(100);

	if (static_df == BQ28Z610_NEED_UPDATE_FW1 || static_df == BQ28Z610_NEED_UPDATE_FW2) {
		data[0] = BQ28Z610_UPDATE_DONE_FW1  & 0xFF;
		data[1] = BQ28Z610_UPDATE_DONE_FW1 >> 8;
	} else if (static_df == BQ28Z610_NEED_UPDATE_FW3) {
		data[0] = BQ28Z610_UPDATE_DONE_FW3  & 0xFF;
		data[1] = BQ28Z610_UPDATE_DONE_FW3 >> 8;
	}
	ret = fg_mac_write_block(bq, 0x4061, data, 2);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to set checksum\n");
		goto seal;
	}
	msleep(100);

	ret = fg_mac_read_block(bq, 0x0005, t_buf, 32);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get static df\n");
		return -EINVAL;
	}
seal:
	fg_seal(bq);

	return 0;
}

static int fg_get_manufacture_data(struct bq_fg_chip *bq)
{
	u8 t_buf[40];
	int ret;
	int i;
	int byte, base, step;

	return 0;

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

	ret = fg_read_byte(bq, bq->regs[BQ_FG_REG_SOC], (u8 *)soc);
	if (ret < 0) {
		if (bq->last_soc > 0)
			*soc = bq->last_soc;
		else
			*soc = BQ_I2C_FAILED_SOC;
		return ret;
	}

	return ret;
}

static int fg_read_system_soc(struct bq_fg_chip *bq)
{
	int batt_soc = 0, curr = 0, temp = 0, raw_soc = 0, soc = 0;
	static ktime_t last_change_time = -1;
	int unit_time = 0, delta_time = 0;
	int change_delta = 0;
	int soc_changed = 0;
	int ret = 0;

	if (bq->fake_soc != -EINVAL)
		return bq->fake_soc;

	ret = fg_read_rsoc(bq, &batt_soc);
	if (ret < 0)
		return batt_soc;

	if (last_change_time == -1) {
		last_change_time = ktime_get();
		bq->last_soc = batt_soc;
	}

	raw_soc = bq->raw_soc;
	curr = bq->batt_curr;
	temp = bq->batt_temp;

	soc = bq_battery_soc_smooth_tracking(bq, raw_soc, batt_soc, temp, curr);

	if (raw_soc > 9600) {
		if (raw_soc == 10000 && bq->last_soc < 99) {
			unit_time = 40000;
			calc_delta_time(last_change_time, &change_delta);
			if (delta_time < 0) {
				last_change_time = ktime_get();
				delta_time = 0;
			}
			delta_time = change_delta / unit_time;
			soc_changed = min(1, delta_time);
			if (soc_changed) {
				soc = bq->last_soc + soc_changed;
				bq_dbg(PR_OEM, "soc increase changed = %d\n", soc_changed);
			} else
				soc = bq->last_soc;
		} else
			soc = 100;
	} else {
		if (raw_soc == 0 && bq->last_soc > 1) {
			bq->ffc_smooth = false;
			unit_time = 10000;
			calc_delta_time(last_change_time, &change_delta);
			delta_time = change_delta / unit_time;
			if (delta_time < 0) {
				last_change_time = ktime_get();
				delta_time = 0;
			}
			soc_changed = min(1, delta_time);
			if (soc_changed) {
				bq_dbg(PR_OEM, "soc reduce changed = %d\n", soc_changed);
				soc = bq->last_soc - soc_changed;
			} else
				soc = bq->last_soc;
		} else {
			soc = (raw_soc + 95) / 96;
			if (soc <= 102 && soc > 99)
				soc = 99;
		}
	}

	if (soc > 100)
		soc = 100;
	if (soc < 0)
		soc = batt_soc;

	if (bq->last_soc != soc) {
		last_change_time = ktime_get();
		bq->last_soc = soc;
	}

	return soc;
}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	int ret;
	int temp = 0;
	static int last_temp;
	static int temp_check;
	static bool first_read_flag;
	int final_temp;

	if (bq->fake_temp != -EINVAL)
		return bq->fake_temp;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], (u16 *)&temp);
	if (ret < 0) {
		bq_dbg(PR_DEBUG, "could not read temperature, ret = %d\n", ret);
		return BQ_I2C_FAILED_TEMP;
	}

	final_temp = temp - 2730;

	if (final_temp > 600 && temp_check < 3) {
		if (!first_read_flag) {
			last_temp = 590;
			temp_check++;
			first_read_flag = true;
			return last_temp;
		} else if (final_temp > last_temp + 100) {
			temp_check++;
			final_temp = last_temp;
			return final_temp;
		}
	}

	temp_check = 0;
	last_temp = final_temp;

	return final_temp;
}

static int fg_read_volt(struct bq_fg_chip *bq)
{
	int ret;
	u16 volt = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &volt);
	if (ret < 0) {
		volt = BQ_I2C_FAILED_VOL;
	}
	volt = volt + bq->cell_delta;

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

static int fg_read_ttf(struct bq_fg_chip *bq)
{
	int ret;
	u16 ttf;

	if (bq->regs[BQ_FG_REG_TTF] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "Time To Empty not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TTF], &ttf);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Full, ret=%d\n", ret);
		return ret;
	}

	return ttf;

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
	POWER_SUPPLY_PROP_VOLTAGE_CELL1,
	POWER_SUPPLY_PROP_VOLTAGE_CELL2,
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
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
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
#define SHUTDOWN_VOL		6800
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
	case POWER_SUPPLY_PROP_VOLTAGE_CELL1:
		if (bq->fake_volt != -EINVAL) {
			val->intval = bq->fake_volt;
			break;
		}
		val->intval = bq->cell1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_CELL2:
		if (bq->fake_volt != -EINVAL) {
			val->intval = bq->fake_volt;
			break;
		}
		val->intval = bq->cell2;
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
				if (vbat_mv > SHUTDOWN_VOL
					&& status != POWER_SUPPLY_STATUS_CHARGING) {
					val->intval = 1;
					break;
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
		val->intval = fg_get_soc_decimal(bq);
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
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		val->intval = bq->batt_ttf;
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
		if (bq->ffc_smooth)
			val->intval = bq->batt_soc;
		else
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
		val->intval = bq->batt_fcc * 2000;
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
		if (bq->raw_soc > 9600) {
			bq->optimiz_soc = val->intval;
		} else if (bq->raw_soc > 770) {
			bq->optimiz_soc = val->intval - 3;
		} else
			bq->optimiz_soc = bq->batt_soc;
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

#define CELL_DELTA_UPDATE_TIME  1800000000 /*30min*/
static int fg_get_cell_voltage(struct bq_fg_chip *bq, bool force_update)
{
	int delta_max;
	int ret = 0;
	u8 t_buf[40];
	static u64 last_update_time, elapsed_us;

	if (force_update)
		goto update;

	if (!bq->verify_digest_success)
		return 0;

	if (bq->batt_temp < 200 || bq->batt_temp > 350)
		return 0;

	if (bq->batt_curr < -100 || bq->batt_curr > 100)
		return 0;

	if (bq->batt_soc < 35 || bq->batt_soc > 80)
		return 0;

	if (bq->batt_volt < 7700 || bq->batt_volt > 8200)
		return 0;

	elapsed_us = ktime_us_delta(ktime_get(), last_update_time);
	if (elapsed_us < CELL_DELTA_UPDATE_TIME)
		return 0;

update:
	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, t_buf, 8);
	if (ret < 0)
		return 0;

	bq->cell1 = (t_buf[1] << 8) | t_buf[0];
	bq->cell2 = (t_buf[3] << 8) | t_buf[2];

	if (bq->cell1 > bq->cell2)
		delta_max = bq->cell1 - bq->cell2;
	else
		delta_max = bq->cell2 - bq->cell1;

	if (delta_max > bq->cell_delta)
		bq->cell_delta = delta_max;

	last_update_time = ktime_get();

	bq_dbg(PR_OEM, "cell1:%d,cell2:%d,delta:%d,temp:%d,curr:%d\n",
			bq->cell1, bq->cell2, bq->cell_delta, bq->batt_temp, bq->batt_curr);

	return ret;
}

static ssize_t fg_attr_show_batt_volt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	fg_get_cell_voltage(bq, false);

	len = snprintf(buf, 1024, "%d,%d,%d\n", bq->cell1, bq->cell2, bq->cell_delta);

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
	int delta = 20;

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
		delta = 20;
		term_curr = bq->ffc_term_curr;
		interval = MONITOR_WORK_1S;
	} else {
		if (bq->batt_temp < BQ28Z610_COLD_TEMP_TERM) {
			bq->term_curr = bq->cold_term;
			delta = 40;
		} else {
			delta = 30;
			bq->term_curr = bq->normal_term;
		}
		term_curr = bq->term_curr;
		if (bq->usb_present)
			interval = MONITOR_WORK_5S;
		else
			interval = MONITOR_WORK_10S;
	}
	full_volt = get_effective_result(bq->fv_votable) / 1000 - delta;

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

#define DECIMAL_BUFF_LEN	10
#define DEFUAL_DECIMAL		3
#define I2C_ERROR_CHECK_TIME	10
#define I2C_ERROR_FCC		2000000
#define I2C_ERROR_FV		8000000
static void fg_update_status(struct bq_fg_chip *bq)
{
	static int last_st, last_soc, last_temp, check;
	static int soc_decimal[10];
	static int index, last_raw_soc, last_present;
	int i, decimal_rate = 0;

	mutex_lock(&bq->data_lock);

	bq->batt_curr = fg_read_current(bq);
	bq->batt_cyclecnt = fg_read_cyclecount(bq);
	bq->batt_ttf =  fg_read_ttf(bq);
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
	fg_get_cell_voltage(bq, false);

	if (bq->batt_rm <= 0 && bq->batt_fcc <= 0 &&
			bq->batt_volt == BQ_I2C_FAILED_VOL) {
		if (check < I2C_ERROR_CHECK_TIME) {
			check++;
		} else {
			if (!bq->cp_disable_votable)
				bq->cp_disable_votable = find_votable("CP_DISABLE");
			vote(bq->cp_disable_votable, BMS_FG_VERIFY, true, 0);
			vote(bq->fv_votable, BMS_FG_VERIFY, true, I2C_ERROR_FV);
			vote(bq->fcc_votable, BMS_FG_VERIFY, true, I2C_ERROR_FCC);
		}
	} else
		check = 0;

	if (bq->fast_mode) {
		if (last_present != bq->usb_present || last_raw_soc == 0) {
			for (i = 0; i < DECIMAL_BUFF_LEN; i++) {
				soc_decimal[i] = DEFUAL_DECIMAL;
			}
			index = 0;
			last_raw_soc = bq->raw_soc;
		} else {
			if (index == DECIMAL_BUFF_LEN)
				index = 0;
			soc_decimal[index] = bq->raw_soc - last_raw_soc;
			index++;
		}

		for (i = 0; i < DECIMAL_BUFF_LEN; i++) {
			decimal_rate += soc_decimal[i];
		}
		last_present = bq->usb_present;
		last_raw_soc = bq->raw_soc;
		if (bq->soc_decimal_rate < decimal_rate)
			bq->soc_decimal_rate = decimal_rate;
	}
	mutex_unlock(&bq->data_lock);

	bq_dbg(PR_OEM, "SOC:%d,Volt:%d,Cur:%d,Temp:%d,RM:%d,FC:%d, RAW_SOC:%d,SOH:%d,"
			"FAST:%d,DR1:%d,Cell1:%d,Cell2:%d,Delta:%d",
			bq->batt_soc, bq->batt_volt, bq->batt_curr,
			bq->batt_temp, bq->batt_rm, bq->batt_fcc,
			bq->raw_soc, bq->soh, bq->fast_mode, bq->soc_decimal_rate,
			bq->cell1, bq->cell2, bq->cell_delta);

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

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	bq_dbg(PR_DEBUG,  "now:%ld, last:%ld, delta:%d\n", time_now, time_last, *delta_time);

	return 0;
}

#define BATT_HIGH_AVG_CURRENT		1000000
#define NORMAL_TEMP_CHARGING_DELTA	10000
#define NORMAL_DISTEMP_CHARGING_DELTA	60000
#define LOW_TEMP_CHARGING_DELTA		10000
#define LOW_TEMP_DISCHARGING_DELTA	20000
#define FFC_SMOOTH_LEN			4
#define FG_RAW_SOC_FULL			10000
#define FG_REPORT_FULL_SOC		8600
#define FG_OPTIMIZ_FULL_TIME		25000

#define HW_OPTIMIZ_REPORT_FULL		1
#define HW_REPORT_FULL_SOC		9500
#define HW_ONE_PERCENT_RAW_SOC		((HW_REPORT_FULL_SOC - FG_REPORT_FULL_SOC) * 100 / \
		(FG_RAW_SOC_FULL - FG_REPORT_FULL_SOC - 100))

struct ffc_smooth {
	int curr_lim;
	int time;
};

struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    300000},
	{150,  150000},
	{300,   72000},
	{500,   50000},
};

static int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int batt_soc, int batt_temp, int batt_ma)
{
	static int last_batt_soc = -1, last_raw_soc, system_soc;
	static int last_status;
	int change_delta = 0, rc, status;
	static ktime_t last_change_time;
	int unit_time = 0, delta_time = 0;
	int soc_changed = 0;
	static int optimiz_soc;
	union power_supply_propval pval = {0, };
	int batt_ma_avg, i;

	if (bq->optimiz_soc > 0) {
		bq->ffc_smooth = true;
		last_batt_soc = bq->optimiz_soc;
		last_change_time = ktime_get();
		bq->optimiz_soc = 0;
	}

	if (!bq->usb_psy || !bq->batt_psy) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (!bq->usb_psy) {
			return batt_soc;
		}
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return batt_soc;
		}
	}

	if (last_batt_soc < 0)
		last_batt_soc = batt_soc;

	if (raw_soc == FG_RAW_SOC_FULL)
		bq->ffc_smooth = false;

	if (bq->ffc_smooth) {
		rc = power_supply_get_property(bq->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if (rc < 0) {
			bq_dbg(PR_OEM, "failed get batt staus\n");
			return -EINVAL;
		}
		status = pval.intval;
		if (batt_soc == last_batt_soc) {
			bq_dbg(PR_DEBUG, "batt_soc:%d, last_batt_soc:%d, is samed set ffc smooth to false\n",
					batt_soc, last_batt_soc);
			bq->ffc_smooth = false;
			return batt_soc;
		}
		if (status != last_status) {
			if (last_status == POWER_SUPPLY_STATUS_CHARGING
					&& status == POWER_SUPPLY_STATUS_DISCHARGING)
				last_change_time = ktime_get();
			bq_dbg(PR_DEBUG, "update last_statu:%d, last_change_time:%ld\n", status, last_change_time);
			last_status = status;
		}
	}
	rc = power_supply_get_property(bq->usb_psy,
			POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE, &pval);
	if (rc < 0) {
		bq_dbg(PR_OEM, "could not get quick charge type\n");
		return -EINVAL;
	}

	if (pval.intval == QUICK_CHARGE_SUPER && raw_soc >= FG_REPORT_FULL_SOC && raw_soc != FG_RAW_SOC_FULL) {
		bq_dbg(PR_DEBUG, "raw_soc:%d, last_raw_soc:%d HW_ONE_PERCENT_RAW_SOC:%d optimiz_soc:%d\n",
					raw_soc, last_raw_soc, HW_ONE_PERCENT_RAW_SOC, optimiz_soc);
		if (raw_soc - last_raw_soc > HW_ONE_PERCENT_RAW_SOC) {
			last_raw_soc += HW_ONE_PERCENT_RAW_SOC;
			soc_changed = 1;
			optimiz_soc += soc_changed;
			if (optimiz_soc > 100)
				optimiz_soc = 100;
			bq->ffc_smooth = true;
		}
		if (batt_soc > optimiz_soc) {
			optimiz_soc = batt_soc;
		}
		if (bq->ffc_smooth)
			batt_soc = optimiz_soc;
		last_change_time = ktime_get();
	} else {
		optimiz_soc = batt_soc + 1;
		last_raw_soc = raw_soc;
	}

	calc_delta_time(last_change_time, &change_delta);
	fg_read_avg_current(bq, &batt_ma_avg);
	if (batt_temp > BATT_COOL_THRESHOLD && batt_soc != 0) {
		if (bq->ffc_smooth && (status == POWER_SUPPLY_STATUS_DISCHARGING ||
					status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
					batt_ma > 1)) {
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
	} else if (batt_ma > 1 && (batt_temp <= BATT_COOL_THRESHOLD)) {
		/* Calculated average current > 1000mA */
		if (batt_ma_avg > BATT_HIGH_AVG_CURRENT)
			/* Heavy loading current, ignore battery soc limit*/
			unit_time = LOW_TEMP_CHARGING_DELTA;
		else
			unit_time = LOW_TEMP_DISCHARGING_DELTA;
	}

	if (unit_time > 0) {
		delta_time = change_delta / unit_time;
		soc_changed = min(1, delta_time);
	} else {
		if (!bq->ffc_smooth)
			bq->update_now = true;
	}

	if (last_batt_soc < batt_soc && batt_ma < 0) {
		/* Battery in charging status
		 * update the soc when resuming device
		 */
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc + soc_changed;
	} else if (last_batt_soc > batt_soc && batt_ma > 0) {
		/* Battery in discharging status
		 * update the soc when resuming device
		 */
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc - soc_changed;
	}
	bq_dbg(PR_OEM, "avg:%d ma:%d, changed:%d update:%d smooth:%d "
			"optimiz:%d soc:%d system:%d last:%d time:%d",
			batt_ma_avg, batt_ma, soc_changed, bq->update_now, bq->ffc_smooth,
			optimiz_soc, batt_soc, system_soc, last_batt_soc, unit_time);

	if (system_soc != last_batt_soc) {
		system_soc = last_batt_soc;
		last_change_time = ktime_get();
	}

	bq->update_now = false;

	return last_batt_soc;
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
		bq->usb_present = pval.intval;
		if (pval.intval) {
			pm_stay_awake(bq->dev);
		} else {
			bq->batt_sw_fc = false;
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
	bq->batt_ttf = -ENODATA;
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

	fg_disable_sleep_mode(bq);

	ret = fg_get_manufacture_data(bq);
	bq->regmap = devm_regmap_init_i2c(client, &i2c_bq27z561_regmap_config);
	if (!bq->regmap)
		return -ENODEV;

	fg_get_cell_voltage(bq, true);
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

