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
#include <linux/time.h>
#include <linux/hardware_info.h>
#include "bq27z561_iio.h"
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

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

#define BATTERY_DIGEST_LEN 32

#define DEFUALT_FULL_DESIGN		5000

#define BQ_REPORT_FULL_SOC	9700
#define BQ_CHARGE_FULL_SOC	9650
#define BQ_RECHARGE_SOC		9900

#define BQ27Z561_DEFUALT_TERM		-200
#define BQ27Z561_DEFUALT_FFC_TERM	-680
#define BQ27Z561_DEFUALT_RECHARGE_VOL	4380

#define PD_CHG_UPDATE_DELAY_US	20	/*20 sec*/
#define BQ_I2C_FAILED_SOC	-107
#define BQ_I2C_FAILED_TEMP	-307
#define BMS_FG_VERIFY		"BMS_FG_VERIFY"
#define CC_CV_STEP		"CC_CV_STEP"

#define BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC			4490
#define BQ_MAXIUM_VOLTAGE_FOR_CELL			4480
#define BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC_SAFETY	4477
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
	FG_MAC_CMD_CHEM_NAME	= 0x004B,
	FG_MAC_CMD_MANU_NAME	= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS = 0x0055,
	FG_MAC_CMD_GAGUE_STATUS = 0x0056,
	FG_MAC_CMD_LIFETIME1	= 0x0060,
	FG_MAC_CMD_LIFETIME3	= 0x0062,
	FG_MAC_CMD_ITSTATUS1	= 0x0073,
	FG_MAC_CMD_QMAX		= 0x0075,
	FG_MAC_CMD_FCC_SOH	= 0x0077,
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

#define STEP_TABLE_MAX 3
struct step_config {
	int volt_lim;
	int curr_lim;
};
/* for test
struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4150-2,    5500},
	{4190-1,    4200},
	{4225-1,    3000},
};*/

struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4200-3,    8820},
	{4300-3,    7350},
	{4400-3,    5880},
};
struct bq_fg_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap    *regmap;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;
	struct iio_channel	**ext_iio_chans;
	struct platform_device *pdev;

	struct mutex i2c_rw_lock;
	struct mutex data_lock;

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

	bool batt_fc_1;
	bool batt_fd_1;	/* full depleted */
	bool batt_tc_1;
	bool batt_td_1;	/* full depleted */

	int seal_state; /* 0 - Full Access, 1 - Unsealed, 2 - Sealed */
	int batt_tte;
	int batt_soc;
	int batt_rsoc;
	int batt_soc_old;
	int batt_soc_flag;
	int batt_fcc;	/* Full charge capacity */
	int batt_rm;	/* Remaining capacity */
	int batt_dc;	/* Design Capacity */
	int batt_volt;
	int batt_temp;
	int old_batt_temp;
	int batt_curr;
	int old_batt_curr;
	int fcc_curr;
	int batt_resistance;
	int batt_cyclecnt;	/* cycle count */
	int batt_st;
	int raw_soc;
	int last_soc;
	int batt_id;
	int Qmax_old;
	int rm_adjust;
	int rm_adjust_max;
	bool rm_flag;

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
	struct power_supply_desc fg_psy_d;
	struct timeval suspend_time;

	u8 digest[BATTERY_DIGEST_LEN];
	bool verify_digest_success;
	int constant_charge_current_max;
	//struct votable *fcc_votable;

	bool charge_done;
	bool charge_full;
	int health;
	int batt_recharge_vol;

	/* workaround for debug or other purpose */
	bool	ignore_digest_for_debug;
	bool	old_hw;

	int	*dec_rate_seq;
	int	dec_rate_len;

	struct cold_thermal *cold_thermal_seq;
	int	cold_thermal_len;
	bool	update_now;
	bool    resume_update;
	bool	fast_mode;
	int	optimiz_soc;
	bool	ffc_smooth;
	bool	shutdown_delay;
	bool	shutdown_delay_enable;

	int cell1_max;
	int max_charge_current;
	int max_discharge_current;
	int max_temp_cell;
	int min_temp_cell;
	int total_fw_runtime;
	int time_spent_in_lt;
	int time_spent_in_ht;
	int time_spent_in_ot;

	int cell_ov_check;
};

struct bq_fg_chip *g_bq;

#define bq_dbg(reason, fmt, ...)			\
	do {						\
		if (debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);	\
		else					\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

static int bq_battery_soc_smooth_tracking(struct bq_fg_chip *chip, int raw_soc, int soc, int temp, int curr);
static int fg_get_raw_soc(struct bq_fg_chip *bq);
static int fg_read_current(struct bq_fg_chip *bq, int *curr);
static int fg_read_temperature(struct bq_fg_chip *bq);
/*
static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		bq_dbg(PR_OEM, "i2c read byte fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u8)ret;

	return 0;
}
*/

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

	for(i = 0; i < len; i++) {
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

	for(i = 0; i < len; i++) {
		ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
		if (ret < 0) {
			bq_dbg(PR_REGISTER, "i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return ret;
}

/*
static int fg_read_byte(struct bq_fg_chip *bq, u8 reg, u8 *val)
{
	int ret;

	if (bq->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}
*/

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

static int fg_read_word(struct bq_fg_chip *bq, u8 reg, u16 *val)
{
	int ret;

	if (bq->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_word(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len)
{
	int ret;

	if (bq->skip_reads)
		return 0;

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

	bq_dbg(PR_REGISTER, "%s buf: ", msg);
	for (i = 0; i < len; i++) {
		num = sprintf(&strbuf[idx], "%02X ", buf[i]);
		idx += num;
	}
	bq_dbg(PR_REGISTER, "%s\n", strbuf);
}

#if 0
#define TIMEOUT_INIT_COMPLETED	100
static int fg_check_init_completed(struct bq_fg_chip *bq)
{
	int ret;
	int i = 0;
	u16 status;

	while (i++ < TIMEOUT_INIT_COMPLETED) {
		ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &status);
		if (ret >= 0 && (status & 0x0080))
			return 0;
		msleep(100);
	}
	bq_dbg(PR_OEM, "wait for FG INITCOMP timeout\n");
	return ret;
}
#endif

#if 0
static int fg_get_seal_state(struct bq_fg_chip *bq)
{
	int ret;
	u16 status;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CTRL], &status);
	if (ret < 0) {
		bq_dbg(PR_OEM, "Failed to read control status, ret = %d\n", ret);
		return ret;
	}
	status &= 0x6000;
	status >>= 13;

	if (status == 1)
		bq->seal_state = SEAL_STATE_FA;
	else if (status == 2)
		bq->seal_state = SEAL_STATE_UNSEALED;
	else if (status == 3)
		bq->seal_state = SEAL_STATE_SEALED;

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

#define FG_DEFAULT_UNSEAL_KEY	0x80008000
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

#define FG_DEFAULT_UNSEAL_FA_KEY	0x36724614
static int fg_unseal_full_access(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_unseal_send_key(bq, FG_DEFAULT_UNSEAL_FA_KEY);
	if (!ret) {
		while (retry++ < 100) {
			fg_get_seal_state(bq);
			if (bq->seal_state == SEAL_STATE_FA) {
				bq_dbg(PR_OEM, "FG is in full access.");
				return 0;
			}
			msleep(200);
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
#endif


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

	fg_print_buf("mac_read_block", t_buf, 36);

	cksum = t_buf[34];
	t_len = t_buf[35];

	cksum_calc = checksum(t_buf, t_len - 2);
	if (cksum_calc != cksum)
		return 1;

	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];

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
	msleep(2);

	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], rand_num, length);
	if (ret < 0)
		return ret;

	/*4. Write the checksum (2’s complement sum of (1), (2), and (3)) to address 0x60.*/
	cksum_calc = checksum(rand_num, length);
	ret = fg_write_byte(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], cksum_calc);
	if (ret < 0)
		return ret;

	/*5. Write the length 0x24 to address 0x61.*/
	ret = fg_write_byte(bq, bq->regs[BQ_FG_REG_MAC_DATA_LEN], 0x24);
	if (ret < 0)
		return ret;

	msleep(300);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], bq->digest, length);
	if (ret < 0)
		return ret;

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
	if (ret < 0)
		return ret;

	fg_print_buf("mac_write_block", t_buf, len + 2);

	cksum = checksum(data, len + 2);
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
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

static int fg_get_gague_mode(struct bq_fg_chip *bq)
{
	u8 data[4];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_GAGUE_STATUS, data, 4);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
		return ret;
	}

	bq->batt_fd_1		= !!(data[0] & BIT(0));
	bq->batt_fc_1		= !!(data[0] & BIT(1));
	bq->batt_td_1		= !!(data[0] & BIT(2));
	bq->batt_tc_1		= !!(data[0] & BIT(3));
	bq_dbg(PR_OEM, "gague status FD %d, FC = %d, TD %d TC %d \n",
		bq->batt_fd_1, bq->batt_fc_1, bq->batt_td_1, bq->batt_tc_1);
	return 0;
}


static int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	u8 data[2];
	int ret;

	data[0] = enable;

	bq_dbg(PR_OEM, "set fastcharge mode: enable: %d\n", enable);
	if (enable) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_EN, data, 2);
		if (ret < 0) {
			bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		if (ret < 0) {
			bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
			return ret;
		}
	}
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

	bq_dbg(PR_OEM, "base:%d, index:%d, step:%d\n", base, index, step);
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
		/* for draco p0 and p0.1 */
		if (bq->ignore_digest_for_debug)
			bq->old_hw = true;
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
		bq_dbg(PR_OEM, "manu_info[%d].data %d\n", i, manu_info[i].data);
	}

	return 0;
}

static int fg_get_chem_data(struct bq_fg_chip *bq)
{
	u8 t_buf[40];
	int ret;


	ret = fg_mac_read_block(bq, FG_MAC_CMD_CHEM_NAME, t_buf, 4);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get MANE NAME\n");
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "NOT_DEFAULT_BATTERY");
#endif
		bq->batt_id = 0;
		return 0;
	}
	bq_dbg(PR_OEM, "%s\n", t_buf);

	if (strcmp(t_buf, "FLS0") == 0) {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "S88006_SWD_4V45_5000mAh");
#endif
		bq->batt_id = 1;
	} else if (strcmp(t_buf, "FFN0") == 0) {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "S88006_NVT_4V45_5000mAh");
#endif
		bq->batt_id = 2;
	} else if (strcmp(t_buf, "FFN2") == 0) {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "NVT_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "S88006_NVT_4V45_5000mAh");
#endif
		bq->batt_id = 4;
	} else {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "NOT_DEFAULT_BATTERY");
#endif
		bq->batt_id = 0;
	}
	return 0;
}

static int fg_read_rsoc(struct bq_fg_chip *bq)
{
	int soc, ret;

	if (bq->fake_soc > 0)
		return bq->fake_soc;

	if (bq->skip_reads)
		return bq->last_soc;

	ret = regmap_read(bq->regmap, bq->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read RSOC, ret = %d\n", ret);
		if (bq->last_soc >= 0)
			return bq->last_soc;
		else
			soc = BQ_I2C_FAILED_SOC;
	}

	return soc;
}

static int fg_read_system_soc(struct bq_fg_chip *bq)
{
	int soc, curr, temp, raw_soc;
	int tmp_soc, tmp_ram_soc;

	soc = fg_read_rsoc(bq);
	bq->batt_rsoc = soc;
	if(soc == BQ_I2C_FAILED_SOC)
		return BQ_I2C_FAILED_SOC;
	raw_soc = fg_get_raw_soc(bq);
	if (bq->charge_full) {
		if (raw_soc > BQ_REPORT_FULL_SOC)
			soc = 100;
		else if (raw_soc > BQ_CHARGE_FULL_SOC)
			soc = 99;
		bq->update_now = true;
	}

	fg_read_current(bq, &curr);
	temp = fg_read_temperature(bq);

	bq_dbg(PR_OEM, "rsoc %d raw_soc:%d,batt_volt:%d,Cur:%d", soc, bq->batt_soc, bq->batt_volt, bq->batt_curr);
	if(!bq->batt_soc_flag) {
		bq->batt_soc_flag = 1;
		if(raw_soc > 3000 && raw_soc < 9700)
			tmp_ram_soc = raw_soc / 100 - 1;
		else
			tmp_ram_soc = raw_soc / 100;
		tmp_soc = soc * 100;
		soc = bq_battery_soc_smooth_tracking(bq, tmp_soc, tmp_ram_soc, temp, curr);
		bq_dbg(PR_OEM, "first rsoc %d raw_soc:%d,batt_volt:%d,Cur:%d", soc, bq->batt_soc, bq->batt_volt, bq->batt_curr);
	} else {
		tmp_ram_soc = raw_soc / 100;
		tmp_soc = soc * 100;
		//soc = bq_battery_soc_smooth_tracking(bq, raw_soc, soc, temp, curr); //old
		soc = bq_battery_soc_smooth_tracking(bq, tmp_soc, tmp_ram_soc, temp, curr);
	}
	bq->last_soc = soc;

	if(soc > 100)
		soc = 100;

	return soc;
}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	int ret;
	u16 temp = 0;
	static int last_temp;

	if (bq->fake_temp > 0)
		return bq->fake_temp;

	if (bq->skip_reads)
		return last_temp;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &temp);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read temperature, ret = %d\n", ret);
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
		bq_dbg(PR_OEM, "could not read voltage, ret = %d\n", ret);
		return 3800;
	}

	if(volt < 3450) {
		bq->rm_adjust_max = 0;
		bq->rm_adjust = 0;
	}

	return volt;
}

static int fg_read_avg_current(struct bq_fg_chip *bq, int *curr)
{
	int ret;
	s16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], (u16 *)&avg_curr);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read current, ret = %d\n", ret);
		return ret;
	}
	*curr = -1 * avg_curr;

	return ret;
}

static int fg_read_current(struct bq_fg_chip *bq, int *curr)
{
	int ret;
	s16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CN], (u16 *)&avg_curr);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read current, ret = %d\n", ret);
		*curr = 0;
		return ret;
	}
	*curr = -1 * avg_curr;

	return ret;
}

static int fg_read_fcc(struct bq_fg_chip *bq)
{
	int ret;
	u16 fcc;

	if (bq->regs[BQ_FG_REG_FCC] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "FCC command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);

	if (ret < 0)
		bq_dbg(PR_OEM, "could not read FCC, ret=%d\n", ret);

	return fcc;
}

static int fg_read_dc(struct bq_fg_chip *bq)
{

	int ret;
	u16 dc;

	if (bq->regs[BQ_FG_REG_DC] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM,"DesignCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_DC], &dc);

	if (ret < 0) {
		bq_dbg(PR_OEM,"could not read DC, ret=%d\n", ret);
		return ret;
	}

	return dc;
}

static int fg_read_rm(struct bq_fg_chip *bq)
{
	int ret;
	u16 rm;
	u8 t_buf[64];
	int tmp = 0;

	memset(t_buf, 0, 64);

	if (bq->regs[BQ_FG_REG_RM] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "RemainingCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read DC, ret=%d\n", ret);
		return ret;
	}

	if(rm == 0){
		ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
		tmp = (t_buf[1] << 8) | t_buf[0];
		if(tmp > 65000 && !bq->rm_flag) {
			bq->rm_flag = true;
			bq->rm_adjust_max = 65535 - tmp;
			bq_dbg(PR_OEM, "bq->rm_adjust_max =%d\n", bq->rm_adjust_max);
		}
		if(tmp > 65000) {
			bq->rm_adjust = tmp + bq->rm_adjust_max  - 65535;
		}
		bq_dbg(PR_OEM, "bq->rm_adjust =%d\n", bq->rm_adjust);
	}

	return (rm + bq->rm_adjust);
}

static int fg_read_soh(struct bq_fg_chip *bq)
{
	int ret;
	u16 soh;

	if (bq->regs[BQ_FG_REG_SOH] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "SOH command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read DC, ret=%d\n", ret);
		return ret;
	}

	return soh;
}

static int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CC] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "Cycle Count not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Cycle Count, ret=%d\n", ret);
		return ret;
	}

	return cc;
}

static int fg_read_tte(struct bq_fg_chip *bq)
{
	int ret;
	u16 tte;

	if (bq->regs[BQ_FG_REG_TTE] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "Time To Empty not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TTE], &tte);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return tte;
}
#if 0
static int fg_read_charging_current(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CHG_CUR] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, " not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_CUR], &cc);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return cc;
}
#endif
static int fg_read_charging_voltage(struct bq_fg_chip *bq)
{
	int ret;
	u16 cv;

	if (bq->regs[BQ_FG_REG_CHG_VOL] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, " not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_VOL], &cv);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return cv;
}

static int fg_read_ttf(struct bq_fg_chip *bq)
{
	int ret;
	u16 ttf;

	if (bq->regs[BQ_FG_REG_TTE] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM,"Time To Empty not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TTF], &ttf);

	if (ret < 0) {
		bq_dbg(PR_OEM,"could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return ttf;
}


static int fg_get_batt_status(struct bq_fg_chip *bq)
{

	fg_read_status(bq);

	if (bq->batt_fc)
		return POWER_SUPPLY_STATUS_FULL;
	else if (bq->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (bq->batt_curr <= 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_DISCHARGING;

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

//	if (bq->batt_soc > (raw_soc / 100))
//		return 0;

	return raw_soc % 100;
}

#define SHUTDOWN_DELAY_VOL	3300

static const u8 fg_dump_regs[] = {
	0x00, 0x02, 0x04, 0x06,
	0x08, 0x0A, 0x0C, 0x0E,
	0x10, 0x16, 0x18, 0x1A,
	0x1C, 0x1E, 0x20, 0x28,
	0x2A, 0x2C, 0x2E, 0x30,
	0x66, 0x68, 0x6C, 0x6E,
};

static ssize_t fg_attr_show_Ra_table(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 t_buf[40];
	u8 temp_buf[40];
	int ret;
	int i, idx, len;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_RA_TABLE, t_buf, 32);
	if (ret < 0)
		return 0;

	idx = 0;
	len = sprintf(temp_buf, "Ra Flag:0x%02X\n", t_buf[0] << 8 | t_buf[1]);
	memcpy(&buf[idx], temp_buf, len);
	idx += len;
	len = sprintf(temp_buf, "RaTable:\n");
	memcpy(&buf[idx], temp_buf, len);
	idx += len;
	for (i = 1; i < 16; i++) {
		len = sprintf(temp_buf, "%d ", t_buf[i*2] << 8 | t_buf[i*2 + 1]);
		memcpy(&buf[idx], temp_buf, len);
		idx += len;
	}

	return idx;
}

static ssize_t fg_attr_show_Qmax(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;
	int tmp;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, t_buf, 2);
	if (ret < 0) {
		tmp = bq->Qmax_old;
		len = snprintf(buf, sizeof(t_buf), "%d\n", tmp);
		return 0;
	}
	tmp = (t_buf[1] << 8) | t_buf[0];

	if(bq->batt_temp >= 200) {
		if((tmp < 4500) || (tmp > 5500)){
			tmp = bq->Qmax_old;
		}
	}

	len = snprintf(buf, sizeof(t_buf), "%d\n", tmp);
//	pr_err("BQ27z561 Qmax %d\n", tmp);
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

ssize_t verify_digest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int i;
	u8 random[32] = {0};
	char kbuf[70] = {0};

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);

	StringToHex(kbuf, random, &i);
	fg_sha256_auth(bq, random, BATTERY_DIGEST_LEN);

	return count;
}
#ifdef CONFIG_WT_QGKI
int get_verify_digest(char *buf)
{
	u8 digest_buf[4];
	int i;
	int len;

	for (i = 0; i < BATTERY_DIGEST_LEN; i++) {
		memset(digest_buf, 0, sizeof(digest_buf));
		snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", g_bq->digest[i]);
		strlcat(buf, digest_buf, BATTERY_DIGEST_LEN * 2 + 1);
	}
	len = strlen(buf);
	buf[len] = '\0';

	bq_dbg(PR_REGISTER, "buf = %s\n", buf);
	return strlen(buf) + 1;
}
EXPORT_SYMBOL(get_verify_digest);

int set_verify_digest(u8 *rand_num)
{
	int i;
	u8 random[32] = {0};
	char kbuf[70] = {0};

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, rand_num, 64);

	StringToHex(kbuf, random, &i);
	fg_sha256_auth(g_bq, random, BATTERY_DIGEST_LEN);

	return 0;
}
EXPORT_SYMBOL(set_verify_digest);
#endif
static ssize_t fg_attr_show_TRemQ(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[1] << 8) | t_buf[0]);

	return len;
}

static ssize_t fg_attr_show_TFullChgQ(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[9] << 8) | t_buf[8]);

	return len;
}

static ssize_t fg_attr_show_TSim(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[13] << 8) | t_buf[12]);

	return len;
}

static ssize_t fg_attr_show_TAmbient(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[15] << 8) | t_buf[14]);

	return len;
}

static ssize_t fg_attr_show_cell1_max(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->cell1_max);

	return len;
}

static ssize_t fg_attr_show_max_charge_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->max_charge_current);

	return len;
}

static ssize_t fg_attr_show_max_discharge_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->max_discharge_current);

	return len;
}

static ssize_t fg_attr_show_max_temp_cell(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->max_temp_cell);

	return len;
}

static ssize_t fg_attr_show_min_temp_cell(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->min_temp_cell);

	return len;
}

static ssize_t fg_attr_show_total_fw_runtime(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->total_fw_runtime);

	return len;
}

static ssize_t fg_attr_show_time_spent_in_lt(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->time_spent_in_lt);

	return len;
}

static ssize_t fg_attr_show_time_spent_in_ht(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->time_spent_in_ht);

	return len;
}

static ssize_t fg_attr_show_time_spent_in_ot(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->time_spent_in_ot);

	return len;
}

static ssize_t fg_attr_show_avg_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int curr, len;

	fg_read_avg_current(bq, &curr);

	len = snprintf(buf, PAGE_SIZE, "%d\n", curr);

	return len;
}

static ssize_t fg_attr_show_rsoc(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int soc, len;

	soc = fg_read_rsoc(bq);

	len = snprintf(buf, PAGE_SIZE, "%d\n", soc);

	return len;
}

static DEVICE_ATTR(verify_digest, 0660, verify_digest_show, verify_digest_store);
static DEVICE_ATTR(RaTable, S_IRUGO, fg_attr_show_Ra_table, NULL);
static DEVICE_ATTR(Qmax, S_IRUGO, fg_attr_show_Qmax, NULL);
static DEVICE_ATTR(fcc_soh, S_IRUGO, fg_attr_show_fcc_soh, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_attr_show_rm, NULL);
static DEVICE_ATTR(fcc, S_IRUGO, fg_attr_show_fcc, NULL);
static DEVICE_ATTR(soh, S_IRUGO, fg_attr_show_soh, NULL);
static DEVICE_ATTR(TRemQ, S_IRUGO, fg_attr_show_TRemQ, NULL);
static DEVICE_ATTR(TFullChgQ, S_IRUGO, fg_attr_show_TFullChgQ, NULL);
static DEVICE_ATTR(TSim, S_IRUGO, fg_attr_show_TSim, NULL);
static DEVICE_ATTR(TAmbient, S_IRUGO, fg_attr_show_TAmbient, NULL);
static DEVICE_ATTR(cell1_max, S_IRUGO, fg_attr_show_cell1_max, NULL);
static DEVICE_ATTR(max_charge_current, S_IRUGO, fg_attr_show_max_charge_current, NULL);
static DEVICE_ATTR(max_discharge_current, S_IRUGO, fg_attr_show_max_discharge_current, NULL);
static DEVICE_ATTR(max_temp_cell, S_IRUGO, fg_attr_show_max_temp_cell, NULL);
static DEVICE_ATTR(min_temp_cell, S_IRUGO, fg_attr_show_min_temp_cell, NULL);
static DEVICE_ATTR(total_fw_runtime, S_IRUGO, fg_attr_show_total_fw_runtime, NULL);
static DEVICE_ATTR(time_spent_in_lt, S_IRUGO, fg_attr_show_time_spent_in_lt, NULL);
static DEVICE_ATTR(time_spent_in_ht, S_IRUGO, fg_attr_show_time_spent_in_ht, NULL);
static DEVICE_ATTR(time_spent_in_ot, S_IRUGO, fg_attr_show_time_spent_in_ot, NULL);
static DEVICE_ATTR(avg_current, S_IRUGO, fg_attr_show_avg_current, NULL);
static DEVICE_ATTR(rsoc, S_IRUGO, fg_attr_show_rsoc, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_RaTable.attr,
	&dev_attr_Qmax.attr,
	&dev_attr_fcc_soh.attr,
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_soh.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_TRemQ.attr,
	&dev_attr_TFullChgQ.attr,
	&dev_attr_TSim.attr,
	&dev_attr_TAmbient.attr,
	&dev_attr_cell1_max.attr,
	&dev_attr_max_charge_current.attr,
	&dev_attr_max_discharge_current.attr,
	&dev_attr_max_temp_cell.attr,
	&dev_attr_min_temp_cell.attr,
	&dev_attr_total_fw_runtime.attr,
	&dev_attr_time_spent_in_lt.attr,
	&dev_attr_time_spent_in_ht.attr,
	&dev_attr_time_spent_in_ot.attr,
	&dev_attr_avg_current.attr,
	&dev_attr_rsoc.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};


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

static int fg_get_lifetime_data(struct bq_fg_chip *bq)
{
	int ret;
	u8 t_buf[40];

	memset(t_buf, 0, sizeof(t_buf));

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME1, t_buf, 32);
	if (ret < 0)
		return ret;

	bq->cell1_max = (t_buf[1] << 8) | t_buf[0];
	bq->max_charge_current = (t_buf[3] << 8) | t_buf[2];
	bq->max_discharge_current = (signed short int)((t_buf[5] << 8) | t_buf[4]);
	bq->max_temp_cell = t_buf[6];
	bq->min_temp_cell = t_buf[7];

	memset(t_buf, 0, sizeof(t_buf));
	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME3, t_buf, 32);
	if (ret < 0)
		return ret;

	bq->total_fw_runtime = (t_buf[1] << 8) | t_buf[0];
	bq->time_spent_in_lt = (t_buf[5] << 8) | t_buf[4];
	bq->time_spent_in_ht = (t_buf[13] << 8) | t_buf[12];
	bq->time_spent_in_ot = (t_buf[15] << 8) | t_buf[14];

	return ret;
}

static void fg_update_status(struct bq_fg_chip *bq)
{
	static int last_st, last_soc, last_temp;
	mutex_lock(&bq->data_lock);

	bq->batt_soc = fg_read_system_soc(bq);
	bq->batt_volt = fg_read_volt(bq);
	fg_read_current(bq, &bq->batt_curr);
	bq->batt_temp = fg_read_temperature(bq);
	bq->batt_st = fg_get_batt_status(bq);
	bq->batt_rm = fg_read_rm(bq);
	bq->batt_fcc = fg_read_fcc(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	fg_get_lifetime_data(bq);
	fg_get_gague_mode(bq);

	mutex_unlock(&bq->data_lock);

	if(bq->batt_temp == BQ_I2C_FAILED_TEMP) {
		bq->batt_temp = bq->old_batt_temp;
	} else {
		bq->old_batt_temp = bq->batt_temp;
	}
	bq_dbg(PR_OEM, "SOC:%d,Volt:%d,Cur:%d,Temp:%d,RM:%d,FC:%d,FAST:%d Chg_Status:%d FC%d,FD%d",
			bq->batt_soc, bq->batt_volt, bq->batt_curr,
			bq->batt_temp, bq->batt_rm, bq->batt_fcc, bq->fast_mode, bq->batt_st, bq->batt_fc, bq->batt_fd);

	if ((last_soc != bq->batt_soc) || (last_temp != bq->batt_temp)
			|| (last_st != bq->batt_st)) {
		if (bq->fg_psy)
			power_supply_changed(bq->fg_psy);
	}
	if (bq->batt_st == POWER_SUPPLY_STATUS_DISCHARGING)
		bq->cell_ov_check = 0;

	last_soc = bq->batt_soc;
	last_temp = bq->batt_temp;
	last_st = bq->batt_st;
}


static int fg_get_raw_soc(struct bq_fg_chip *bq)
{
	int rm, fcc;
	int raw_soc;
	int tmp_soc;

	rm = fg_read_rm(bq);
	fcc = fg_read_fcc(bq);

	if(bq->batt_curr < 0) {
		if(bq->batt_temp < 0){
			tmp_soc = rm * 10000 / fcc;
			if(tmp_soc >= 9400)
				raw_soc = 10000;
			else
				raw_soc = (tmp_soc * 100) / 94;
		} else if(bq->batt_temp >= 0 && bq->batt_temp < 50){
			tmp_soc = rm * 10000 / fcc;
			if(tmp_soc >= 9500)
				raw_soc = 10000;
			else
				raw_soc = (tmp_soc * 100) / 95;
		} else {
			if(bq->fast_mode) {
				tmp_soc = rm * 10000 / fcc;
				if(bq->batt_id == 2 || bq->batt_id == 4) {
					if(tmp_soc >= 9650)
						raw_soc = 10000;
					else
						raw_soc = (tmp_soc * 1000) / 965;
				} else {
					if(tmp_soc >= 9670)
						raw_soc = 10000;
					else
						raw_soc = (tmp_soc * 1000) / 967;
				}
			} else {
				tmp_soc = rm * 10000 / fcc;
				if(tmp_soc >= 9730)
					raw_soc = 10000;
				else
					raw_soc = (tmp_soc * 1000) / 973;
			}
			if(tmp_soc - bq->rm_adjust_max * 10000 / fcc > 9730){
				bq->rm_adjust_max = 0;
				bq->rm_adjust = 0;
			}
		}

	} else {
		tmp_soc = rm * 10000 / fcc;
		if(tmp_soc >= 9730)
			raw_soc = 10000;
		else
			raw_soc = (tmp_soc * 1000) / 973;
	}

	//raw_soc = DIV_ROUND_CLOSEST(rm * 10000 / fcc, 100);


	return raw_soc;
}

static int fg_update_charge_full(struct bq_fg_chip *bq)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return 0;
		}
	}

//	rc = power_supply_get_property(bq->batt_psy,
//		POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	bq->charge_done = prop.intval;

	rc = power_supply_get_property(bq->batt_psy,
		POWER_SUPPLY_PROP_HEALTH, &prop);
	bq->health = prop.intval;

	bq_dbg(PR_OEM, "raw:%d,done:%d,full:%d,health:%d\n",
			bq->raw_soc, bq->charge_done, bq->charge_full, bq->health);

	if (bq->charge_done && !bq->charge_full) {
		if (bq->raw_soc >= BQ_REPORT_FULL_SOC) {
			bq_dbg(PR_OEM, "Setting charge_full to true\n");
			bq->charge_full = true;
			bq->cell_ov_check = 0;
		} else {
			bq_dbg(PR_OEM, "charging is done raw soc:%d\n", bq->raw_soc);
		}
	} else if (bq->raw_soc <= BQ_CHARGE_FULL_SOC && !bq->charge_done && bq->charge_full) {

		if (bq->charge_done)
			goto out;

		bq->charge_full = false;
	}

	if ((bq->raw_soc <= BQ_RECHARGE_SOC) && bq->charge_done && bq->health != POWER_SUPPLY_HEALTH_WARM) {
		prop.intval = true;
//		rc = power_supply_set_property(bq->batt_psy,
	//			POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
		if (rc < 0) {
			bq_dbg(PR_OEM, "bq could not set force recharging!\n");
			return rc;
		}
	}

out:
	return 0;
}

static int calc_suspend_time(struct timeval *time_start, int *delta_time)
{
	struct timeval time_now;

	*delta_time = 0;

	//do_gettimeofday(&time_now);
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
#define LOW_TEMP_CHARGING_DELTA		20000
#define LOW_TEMP_DISCHARGING_DELTA	40000
#define FFC_SMOOTH_LEN			4
#define FG_RAW_SOC_FULL			10000
#define FG_REPORT_FULL_SOC		9400
#define FG_OPTIMIZ_FULL_TIME		64000

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

static int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int batt_soc, int batt_temp, int batt_ma)
{
	static int last_batt_soc = -1, system_soc, cold_smooth;
	static int last_status;
	int change_delta = 0, rc;
	int status;
//	int optimiz_delta = 0;
	static ktime_t last_change_time;
	static ktime_t last_optimiz_time;
	int unit_time = 0;
	int soc_changed = 0, delta_time = 0;
	static int optimiz_soc, last_raw_soc;
	union power_supply_propval pval = {0, };
	int batt_ma_avg, i;
	static int old_batt_ma = 0;

	if (bq->optimiz_soc > 0) {
		bq->ffc_smooth = true;
		last_batt_soc = bq->optimiz_soc;
		system_soc = bq->optimiz_soc;
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
#if 0
	if (bq->fast_mode && raw_soc >= FG_REPORT_FULL_SOC && raw_soc != FG_RAW_SOC_FULL) {
		if (last_optimiz_time == 0)
			last_optimiz_time = ktime_get();
		calc_delta_time(last_optimiz_time, &optimiz_delta);
		delta_time = optimiz_delta / FG_OPTIMIZ_FULL_TIME;
		soc_changed = min(1, delta_time);
		if (raw_soc > last_raw_soc && soc_changed) {
			last_raw_soc = raw_soc;
			optimiz_soc += soc_changed;
			last_optimiz_time = ktime_get();
			bq_dbg(PR_DEBUG, "optimiz_soc:%d, last_optimiz_time%ld\n",
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
#else
	optimiz_soc = batt_soc + 1;
	last_raw_soc = raw_soc;
	last_optimiz_time = ktime_get();
#endif
	calc_delta_time(last_change_time, &change_delta);
	fg_read_avg_current(bq, &batt_ma_avg);
	if (batt_temp > 150 && !cold_smooth && batt_soc != 0) {
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

	bq_dbg(PR_DEBUG, "batt_ma_avg:%d, batt_ma:%d, cold_smooth:%d, optimiz_soc:%d",
			batt_ma_avg, batt_ma, cold_smooth, optimiz_soc);
	bq_dbg(PR_DEBUG, "delta_time:%d, change_delta:%d, unit_time:%d"
			" soc_changed:%d, bq->update_now:%d, bq->ffc_smooth %d",
			delta_time, change_delta, unit_time,
			soc_changed, bq->update_now, bq->ffc_smooth);

	if(batt_ma > 0 && old_batt_ma <= 0)
	{
		change_delta = 0;
		last_change_time = ktime_get();
		bq_dbg(PR_OEM, "old_batt_ma:%d", old_batt_ma);
	}
	old_batt_ma = batt_ma;

	if (last_batt_soc < batt_soc && batt_ma < 0) {
		/* Battery in charging status
		 * update the soc when resuming device
		 */
		if(batt_soc - last_batt_soc >= 1) {
			if(bq->fast_mode){
				if(change_delta > 5000) {
					last_batt_soc++;
					last_change_time = ktime_get();
					bq->update_now = false;
				}
			} else {
				if(change_delta > 30000) {
					last_batt_soc++;
					last_change_time = ktime_get();
					bq->update_now = false;
				}
			}
			bq_dbg(PR_OEM, "raw_soc:%d batt_soc:%d,last_batt_soc:%d,change_delta:%d bq->resume_update:%d",
				raw_soc, batt_soc, last_batt_soc, change_delta, bq->resume_update);
		}
		else
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc + soc_changed;
	} else if (last_batt_soc > batt_soc && batt_ma > 0) {
		/* Battery in discharging status
		 * update the soc when resuming device
		 */
		if(bq->resume_update && batt_soc >= 80 && (last_batt_soc - batt_soc <= 3)){
			bq->resume_update = false;
		}
		if(last_batt_soc - batt_soc >= 1 && !bq->resume_update) {
			if(batt_soc == 100 && change_delta > 60000){
				last_batt_soc--;
				last_change_time = ktime_get();
				bq->update_now = false;
			} else if(batt_soc >= 30 && batt_soc < 100 && change_delta > 20000) {
				last_batt_soc--;
				last_change_time = ktime_get();
				bq->update_now = false;
			} else if(batt_soc < 30 && change_delta > 20000) {
				last_batt_soc--;
				last_change_time = ktime_get();
				bq->update_now = false;
			}
			bq_dbg(PR_OEM, "raw_soc:%d batt_soc:%d,last_batt_soc:%d,change_delta:%d bq->resume_update:%d",
				raw_soc, batt_soc, last_batt_soc, change_delta, bq->resume_update);
		} else {
			last_batt_soc = bq->update_now ?
				batt_soc : last_batt_soc - soc_changed;
				bq->resume_update = false;
		}
	}
	if(batt_ma == 0 && bq->batt_volt > 4350){
		if(last_batt_soc < 100 && change_delta > 60000)
			last_batt_soc++;
	}

	if(bq->batt_curr < 0 && bq->batt_curr > -1200 && bq->batt_volt > 4460 && bq->fast_mode){
		if(last_batt_soc < 100 && change_delta > 30000) {
			last_change_time = ktime_get();
			last_batt_soc++;
			bq_dbg(PR_OEM, "last_batt_soc:%d\n", last_batt_soc);
		}
	}

	bq->update_now = false;

	if (system_soc != last_batt_soc) {
		system_soc = last_batt_soc;
		last_change_time = ktime_get();
	}

	bq_dbg(PR_DEBUG, "raw_soc:%d batt_soc:%d,last_batt_soc:%d,system_soc:%d"
			" bq->fast_mode:%d",
			raw_soc, batt_soc, last_batt_soc, system_soc,
			bq->fast_mode);

	return system_soc;
}

#define STEP_DOWN_CURR_MA 100
void fg_cc_cv_step_check(struct bq_fg_chip *bq)
{
	/*int batt_curr = 0, batt_volt = 0;
	u32 i = 0, stepdown = 0, finalFCC = 0, votFCC = 0;
	static u32 count = 0;
	struct step_config *pcc_cv_step_config;
	u32 step_table_max;

	if(bq->batt_st != POWER_SUPPLY_STATUS_CHARGING){
		vote(bq->fcc_votable, CC_CV_STEP, false, votFCC);
		return;
	}

	pcc_cv_step_config = cc_cv_step_config;
	step_table_max = STEP_TABLE_MAX;

	batt_curr = bq->batt_curr*(-1);
	batt_volt = bq->batt_volt;
	bq_dbg(PR_OEM, "fg_cc_cv_step_check: batt_volt:%d batt_curr:%d", batt_volt, batt_curr);
	for(i = 0; i < step_table_max; i++){
		if(batt_volt >= pcc_cv_step_config[i].volt_lim && batt_curr > pcc_cv_step_config[i].curr_lim){
			count++;
			if(count >= 2){
				stepdown = true;
				count = 0;
				bq_dbg(PR_OEM,"fg_cc_cv_step_check:stepdown");
			}
			break;
		}
	}
	finalFCC = get_effective_result(bq->fcc_votable);
	if(!stepdown || finalFCC <= pcc_cv_step_config[step_table_max-1].curr_lim)
		return;

	if(finalFCC - pcc_cv_step_config[i].curr_lim < STEP_DOWN_CURR_MA)
		votFCC = pcc_cv_step_config[i].curr_lim;
	else
		votFCC = finalFCC - STEP_DOWN_CURR_MA;

	vote(bq->fcc_votable, CC_CV_STEP, true, votFCC);
	bq_dbg(PR_OEM, "fg_cc_cv_step_check:i:%d cccv_step vote:%d stepdown:%d", i, votFCC, stepdown);*/
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);
	int rc;

	if (!bq->old_hw) {
		rc = fg_dump_registers(bq);
		/*if (rc < 0)
			return;*/
		fg_update_status(bq);
		fg_update_charge_full(bq);
	}
	fg_cc_cv_step_check(bq);
	if(bq->fast_mode)
		schedule_delayed_work(&bq->monitor_work, 3 * HZ);
	else
		schedule_delayed_work(&bq->monitor_work, 10 * HZ);
}
static int bq_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret, size;

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

	bq->shutdown_delay_enable = of_property_read_bool(node,
						"bq,shutdown-delay-enable");

	ret = of_property_read_u32(node, "bq,charge-full-design",
			&bq->batt_dc);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get bq,charge-full-designe\n");
		bq->batt_dc = DEFUALT_FULL_DESIGN;
		return ret;
	}

	ret = of_property_read_u32(node, "bq,recharge-voltage",
			&bq->batt_recharge_vol);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get bq,recharge-voltage\n");
		bq->batt_recharge_vol = -EINVAL;
		return ret;
	}

	bq->ignore_digest_for_debug = of_property_read_bool(node,
				"bq,ignore-digest-debug");


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

static int qg_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct bq_fg_chip *bq = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_TEMP:
		bq->fake_temp = val1;
		break;
	case PSY_IIO_CAPACITY:
		bq->fake_soc = val1;
		break;
	case PSY_IIO_UPDATE_NOW:
		fg_dump_registers(bq);
		break;
	case PSY_IIO_PARALLEL_FCC_MAX:
		bq->fcc_curr = val1;
		break;
	case PSY_IIO_BMS_CHIP_OK:
		bq->fake_chip_ok = !!val1;
		break;
	case PSY_IIO_BATTERY_AUTH:
		bq->verify_digest_success = !!val1;
		break;
	case PSY_IIO_SHUTDOWN_DELAY:
		bq->shutdown_delay = val1;
		break;
	case PSY_IIO_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(bq, val1);
		break;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int qg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct bq_fg_chip *bq = iio_priv(indio_dev);
	union power_supply_propval pval = {0, };
	static int ov_count;
	int vbat_mv = 0;
	static bool shutdown_delay_cancel;
	static bool last_shutdown_delay;
	int rc = 0;
	int ret = 0, status;
	u16 flags;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_PRESENT:
		*val1 = 1;
		break;
	case PSY_IIO_STATUS:
		*val1 = fg_get_batt_status(bq);
		break;
	case PSY_IIO_VOLTAGE_NOW:
		ret = fg_read_volt(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_volt = ret;
		*val1 = bq->batt_volt * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case PSY_IIO_VOLTAGE_MAX:
		*val1 = 4450 * 1000;
		*val1 = fg_read_charging_voltage(bq);
		if (*val1 == BQ_MAXIUM_VOLTAGE_FOR_CELL) {
			if (bq->batt_volt > BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC_SAFETY) {
				ov_count++;
				if (ov_count > 2) {
					ov_count = 0;
					bq->cell_ov_check++;
				}
			} else {
				ov_count = 0;
			}
			if (bq->cell_ov_check > 4)
				bq->cell_ov_check = 4;

			*val1 = BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC - bq->cell_ov_check * 10;
			if ((bq->batt_soc == 100) && (*val1 == BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC))
				*val1 = BQ_MAXIUM_VOLTAGE_FOR_CELL;
		}
		*val1 *= 1000;
		break;
	case PSY_IIO_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		fg_read_current(bq, &bq->batt_curr);
		if(bq->batt_curr != BQ_I2C_FAILED_SOC){
			bq->old_batt_curr = bq->batt_curr;
		}
		if(bq->batt_soc == 100 && bq->batt_curr == BQ_I2C_FAILED_SOC) {
			bq->batt_curr = bq->old_batt_curr;
		}
		*val1 = bq->batt_curr * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case PSY_IIO_CAPACITY:
		//add shutdown delay feature
		if (bq->fake_soc >= 0) {
			*val1 = bq->fake_soc;
			if(bq->batt_volt < 3450){
				*val1 = 1;
				bq->fake_soc = -1;
			}
			break;
		}
		bq->batt_soc = fg_read_system_soc(bq);
		if (bq->batt_soc < 0)
			bq->batt_soc = bq->batt_soc_old;
		else
			bq->batt_soc_old = bq->batt_soc;
		*val1 = bq->batt_soc;

		if ((*val1 == 0) && bq->batt_volt >= 3450) {
			*val1 = 1;
		}

		if(bq->batt_temp < 0){
			if ((*val1 == 0) && bq->batt_volt > 3400)
				*val1 = 1;
		}

		if (bq->shutdown_delay_enable && (*val1 <= 5)) {
			if (*val1 == 0) {
				vbat_mv = fg_read_volt(bq);
				pr_err("vbat_mv %d bq->shutdown_delay = %d, shutdown_delay_enable = %d, batt_soc = %d\n",
				vbat_mv, bq->shutdown_delay, bq->shutdown_delay_enable, bq->batt_soc);
				if (bq->usb_psy) {
					power_supply_get_property(bq->usb_psy,
						POWER_SUPPLY_PROP_ONLINE, &pval);
					status = pval.intval;
				}
				if (vbat_mv > SHUTDOWN_DELAY_VOL
					&& !status) {
					bq->shutdown_delay = true;
					*val1 = 1;
				} else if (status && bq->shutdown_delay) {
					bq->shutdown_delay = false;
					shutdown_delay_cancel = true;
					*val1 = 1;
				} else {
					bq->shutdown_delay = false;
					*val1 = 1;
				}
				if(vbat_mv <= SHUTDOWN_DELAY_VOL){
					bq->shutdown_delay = true;
					*val1 = 1;
				}
			} else {
				bq->shutdown_delay = false;
				shutdown_delay_cancel = false;
			}
			if (last_shutdown_delay != bq->shutdown_delay) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->batt_psy)
					power_supply_changed(bq->batt_psy);
			}
		}
		break;
	case PSY_IIO_CAPACITY_LEVEL:
		*val1 = fg_get_batt_capacity_level(bq);
		break;
	case PSY_IIO_TEMP:
		if (bq->fake_temp != -EINVAL) {
			*val1 = bq->fake_temp;
			break;
		}
		ret = fg_read_temperature(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_temp = ret;
		*val1 = bq->batt_temp;
		mutex_unlock(&bq->data_lock);
#ifdef CONFIG_DISABLE_TEMP_PROTECT
		*val1 = 250;
#endif
		break;
	case PSY_IIO_TIME_TO_EMPTY_NOW:
		ret = fg_read_tte(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_tte = ret;

		*val1 = bq->batt_tte * 60;
		mutex_unlock(&bq->data_lock);
		break;

	case PSY_IIO_CHARGE_FULL:
		ret = fg_read_fcc(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_fcc = ret;
		*val1 = bq->batt_fcc * 1000;
		mutex_unlock(&bq->data_lock);
		break;

	case PSY_IIO_CHARGE_FULL_DESIGN:
		ret = fg_read_dc(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_dc = ret;
		*val1 = bq->batt_dc * 1000;
		mutex_unlock(&bq->data_lock);
		break;

	case PSY_IIO_CYCLE_COUNT:
		ret = fg_read_cyclecount(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_cyclecnt = ret;
		*val1 = bq->batt_cyclecnt;
		mutex_unlock(&bq->data_lock);
		break;
	case PSY_IIO_TIME_TO_FULL_NOW:
		*val1 = fg_read_ttf(bq) * 60;
		break;

	case PSY_IIO_RESISTANCE_ID:
		*val1 = fg_read_soh(bq);
		break;
	case PSY_IIO_UPDATE_NOW:
		*val1 = 0;
		break;
	case PSY_IIO_PARALLEL_FCC_MAX:
		*val1 = bq->fcc_curr;
		break;
	case PSY_IIO_BMS_CHIP_OK:
		if (bq->fake_chip_ok != -EINVAL) {
			*val1 = bq->fake_chip_ok;
			break;
		}
		ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
		if (ret < 0)
			*val1 = 0;
		else
			*val1 = 1;
		break;
	case PSY_IIO_BATTERY_AUTH:
		*val1 = bq->verify_digest_success;
		break;
	case PSY_IIO_BMS_SOC_DECIMAL:
		*val1 = fg_get_soc_decimal(bq);
		break;
	case PSY_IIO_BMS_SOC_DECIMAL_RATE:
		*val1 = fg_get_soc_decimal_rate(bq);
		break;
	case PSY_IIO_SOH:
		*val1 = fg_read_soh(bq);
		break;
	case PSY_IIO_BATTERY_ID:
		*val1 = bq->batt_id;
		break;
	case PSY_IIO_CC_SOC:
		*val1 = (bq->batt_rm * 10000) / bq->batt_fcc;
		break;
	case PSY_IIO_SHUTDOWN_DELAY:
		*val1 = bq->shutdown_delay;
		break;
	case PSY_IIO_FASTCHARGE_MODE:
		*val1 = bq->fast_mode;
		break;	
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int qg_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct bq_fg_chip *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(qg_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info qg_iio_info = {
	.read_raw	= qg_iio_read_raw,
	.write_raw	= qg_iio_write_raw,
	.of_xlate	= qg_iio_of_xlate,
};

static int bq27z561_init_iio_psy(struct bq_fg_chip *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int qg_num_iio_channels = ARRAY_SIZE(qg_iio_psy_channels);
	int rc, i;

	chip->iio_chan = devm_kcalloc(chip->dev, qg_num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				qg_num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	chip->ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(qg_ext_iio_chan_name),
				sizeof(*chip->ext_iio_chans),
				GFP_KERNEL);
	if (!chip->ext_iio_chans)
		return -ENOMEM;

	indio_dev->info = &qg_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->name = "bq27z561,qg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = qg_num_iio_channels;

	for (i = 0; i < qg_num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = qg_iio_psy_channels[i].channel_num;
		chan->type = qg_iio_psy_channels[i].type;
		chan->datasheet_name =
			qg_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			qg_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			qg_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register QG IIO device, rc=%d\n", rc);

	pr_err("BQ27z561 IIO device, rc=%d\n", rc);
	return rc;
}



static int bq_fg_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;
	struct bq_fg_chip *bq;
	u8 *regs;
	struct iio_dev *indio_dev;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*bq));
	bq = iio_priv(indio_dev);
	bq->indio_dev = indio_dev;

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
	bq->batt_temp	= 250;
	bq->old_batt_temp = 250;
	bq->batt_curr	= -ENODATA;
	bq->batt_cyclecnt = -ENODATA;
	bq->batt_tte = -ENODATA;
	bq->raw_soc = -ENODATA;
	bq->fcc_curr = 12200;
	bq->last_soc = -EINVAL;
	bq->cell_ov_check = 0;
	bq->batt_soc_old = -107;
	bq->Qmax_old = 5000;
	bq->rm_flag = false;
	bq->rm_adjust = 0;
	bq->rm_adjust_max = 0;
	bq->batt_soc_flag = 0;
	bq->old_batt_curr = 0;

	bq->fake_soc	= -EINVAL;
	bq->fake_temp	= -EINVAL;
	bq->fake_volt	= -EINVAL;
	bq->fake_chip_ok = -EINVAL;

	if (bq->chip == BQ27Z561) {
		regs = bq27z561_regs;
	} else {
		bq_dbg(PR_OEM, "unexpected fuel gauge: %d\n", bq->chip);
		regs = bq27z561_regs;
	}
	memcpy(bq->regs, regs, NUM_REGS);

	i2c_set_clientdata(client, bq);

	bq_parse_dt(bq);

	g_bq = bq;
	bq->regmap = devm_regmap_init_i2c(client, &i2c_bq27z561_regmap_config);
	if (!bq->regmap)
		return -ENODEV;

	ret = bq27z561_init_iio_psy(bq);
	if (ret < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", ret);
	}

	fg_get_manufacture_data(bq);
	fg_set_fastcharge_mode(bq, false);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	device_init_wakeup(bq->dev, 1);

	fg_update_status(bq);

	//bq->fcc_votable = find_votable("FCC");
	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret)
		bq_dbg(PR_OEM, "Failed to register sysfs, err:%d\n", ret);

	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work,10 * HZ);

	bq_dbg(PR_OEM, "bq fuel gauge probe successfully, %s\n",
			device2str[bq->chip]);

	fg_get_chem_data(bq);
	return 0;
}


static int bq_fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bq->monitor_work);
	bq->skip_reads = true;
	//do_gettimeofday(&bq->suspend_time);

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
	bq->resume_update = true;
	schedule_delayed_work(&bq->monitor_work, HZ);

	return 0;
}

static int bq_fg_remove(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

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

