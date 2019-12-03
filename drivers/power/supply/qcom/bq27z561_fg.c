/*
 * bq27z561 fuel gauge driver
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) 2019 XiaoMi, Inc.
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

#define bq_info	pr_info
#define bq_dbg	pr_debug
#define bq_err	pr_debug
#define bq_log	pr_err


#define	INVALID_REG_ADDR	0xFF

#define FG_FLAGS_FD				BIT(4)
#define	FG_FLAGS_FC				BIT(5)
#define	FG_FLAGS_DSG				BIT(6)
#define FG_FLAGS_RCA				BIT(9)
#define FG_FLAGS_FASTCHAGE			BIT(5)

#define BATTERY_DIGEST_LEN 32

#define DEFUALT_FULL_DESIGN		100000

enum bq_fg_reg_idx {
	BQ_FG_REG_CTRL = 0,
	BQ_FG_REG_TEMP,		/* Battery Temperature */
	BQ_FG_REG_VOLT,		/* Battery Voltage */
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
	0x0C,	/* AVG CURRENT */
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
	FG_MAC_CMD_MANU_NAME	= 0x004C,
	FG_MAC_CMD_CHARGING_STATUS = 0x0055,
	FG_MAC_CMD_RA_TABLE	= 0x40C0,
};


enum {
	SEAL_STATE_RSVED,
	SEAL_STATE_UNSEALED,
	SEAL_STATE_SEALED,
	SEAL_STATE_FA,
};


enum bq_fg_device {
	BQ27Z561,
};

static const unsigned char *device2str[] = {
	"bq27z561",
};

struct bq_fg_chip {
	struct device *dev;
	struct i2c_client *client;


	struct mutex i2c_rw_lock;
	struct mutex data_lock;
	struct regmap    *regmap;

	int fw_ver;
	int df_ver;

	u8 chip;
	u8 regs[NUM_REGS];

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

	/* debug */
	int skip_reads;
	int skip_writes;

	int fake_soc;
	int fake_temp;

	struct	delayed_work monitor_work;
	struct power_supply *fg_psy;
	struct power_supply *chg_psy;
	struct power_supply *qcom_psy;
	struct power_supply_desc fg_psy_d;

	u8 digest[BATTERY_DIGEST_LEN];
	bool verify_digest_success;

	/* workaround for debug or other purpose */
	bool	ignore_digest_for_debug;

};



/*
static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		bq_err("i2c read byte fail: can't read from reg 0x%02X\n", reg);
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
		bq_err("i2c write byte fail: can't write 0x%02X to reg 0x%02X\n",
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
		bq_err("i2c read word fail: can't read from reg 0x%02X\n", reg);
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
			bq_err("i2c read reg 0x%02X faild\n", reg + i);
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
			bq_err("i2c read reg 0x%02X faild\n", reg + i);
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

	bq_dbg("%s buf: ", msg);
	for (i = 0; i < len; i++) {
		num = snprintf(&strbuf[idx], sizeof(buf), "%02X ", buf[i]);
		idx += num;
	}
	bq_dbg("%s\n", strbuf);
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
	bq_err("wait for FG INITCOMP timeout\n");
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
		bq_err("Failed to read control status, ret = %d\n", ret);
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
		bq_err("unable to write unseal key step 1, ret = %d\n", ret);
		return ret;
	}

	msleep(5);

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], (key >> 16) & 0xFFFF);
	if (ret < 0) {
		bq_err("unable to write unseal key step 2, ret = %d\n", ret);
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
				bq_log("FG is unsealed");
				return 0;
			}
		}
	}

	return -EINVAL;
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
				bq_log("FG is in full access.");
				return 0;
			}
			msleep(200);
		}
	}

	return -EINVAL;
}


static int fg_seal(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], FG_MAC_CMD_SEAL);

	if (ret < 0) {
		bq_err("Failed to send seal command\n");
		return ret;
	}

	while (retry++ < 100) {
		fg_get_seal_state(bq);
		if (bq->seal_state == SEAL_STATE_SEALED) {
			bq_log("FG is sealed successfully");
			return 0;
		}
		msleep(200);
	}

	return -EINVAL;
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

	msleep(2);

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

	msleep(200);

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
		return -EINVAL;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	for (i = 0; i < len; i++)
		t_buf[i+2] = data[i];

	cksum = checksum(data, len + 2);
	/*write command/addr, data*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, len + 2);
	if (ret < 0)
		return ret;
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);

	return ret;
}

static int fg_get_fastcharge_mode(struct bq_fg_chip *bq)
{
	u8 data[3];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CHARGING_STATUS, data, 3);
	if (ret < 0) {
		bq_err("could not write fastcharge = %d\n", ret);
		return ret;
	}

	return (data[2] & FG_FLAGS_FASTCHAGE) >> 5;
}

static int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	u8 data[2];
	int ret;

	data[0] = enable;

	pr_info("set fastcharge mode: enable: %d\n", enable);
	if (enable) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_EN, data, 2);
		if (ret < 0) {
			bq_err("could not write fastcharge = %d\n", ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		if (ret < 0) {
			bq_err("could not write fastcharge = %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int fg_read_status(struct bq_fg_chip *bq)
{
	int ret;
	u16 flags;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
	if (ret < 0)
		return ret;

	mutex_lock(&bq->data_lock);
	bq->batt_fc		= !!(flags & FG_FLAGS_FC);
	bq->batt_fd		= !!(flags & FG_FLAGS_FD);
	bq->batt_rca		= !!(flags & FG_FLAGS_RCA);
	bq->batt_dsg		= !!(flags & FG_FLAGS_DSG);
	mutex_unlock(&bq->data_lock);

	return 0;
}

static int fg_read_resistance_id(struct bq_fg_chip *bq)
{
	u8 t_buf[40];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, t_buf, 32);
	if (ret < 0)
		return 100000;

	return 0;
}

static int fg_read_resistance(struct bq_fg_chip *bq)
{
	int ret, i;
	int ra_table[15];
	int ra_flag;
	u8 t_buf[40];
	int sum = 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_RA_TABLE, t_buf, 32);
	if (ret < 0)
		return 0;

	ra_flag = t_buf[0] << 8 | t_buf[1];
	for (i = 1; i < 16; i++) {
		ra_table[i] = t_buf[i*2] << 8 | t_buf[i*2 + 1];
		sum += ra_table[i];
	}

	return sum / 15;
}

static int fg_read_rsoc(struct bq_fg_chip *bq)
{
	int ret;
	u16 soc = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		bq_err("could not read RSOC, ret = %d\n", ret);
		return 50;
	}

	return soc;

}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	int ret;
	u16 temp = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &temp);
	if (ret < 0) {
		bq_err("could not read temperature, ret = %d\n", ret);
		return 250;
	}

	return temp - 2730;

}

static int fg_read_volt(struct bq_fg_chip *bq)
{
	int ret;
	u16 volt = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &volt);
	if (ret < 0) {
		bq_err("could not read voltage, ret = %d\n", ret);
		return 3700;
	}

	return volt;

}

static int fg_read_current(struct bq_fg_chip *bq, int *curr)
{
	int ret;
	u16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], &avg_curr);
	if (ret < 0) {
		bq_err("could not read current, ret = %d\n", ret);
		return ret;
	}
	*curr = (int)((s16)avg_curr);

	return ret;
}

static int fg_read_fcc(struct bq_fg_chip *bq)
{
	int ret;
	u16 fcc;

	if (bq->regs[BQ_FG_REG_FCC] == INVALID_REG_ADDR) {
		bq_err("FCC command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);

	if (ret < 0)
		bq_err("could not read FCC, ret=%d\n", ret);

	return fcc;
}

static int fg_read_rm(struct bq_fg_chip *bq)
{
	int ret;
	u16 rm;

	if (bq->regs[BQ_FG_REG_RM] == INVALID_REG_ADDR) {
		bq_err("RemainingCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);

	if (ret < 0) {
		bq_err("could not read DC, ret=%d\n", ret);
		return ret;
	}

	return rm;

}

static int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CC] == INVALID_REG_ADDR) {
		bq_err("Cycle Count not supported!\n");
		return -EINVAL;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);

	if (ret < 0) {
		bq_err("could not read Cycle Count, ret=%d\n", ret);
		return ret;
	}

	return cc;
}

static int fg_read_tte(struct bq_fg_chip *bq)
{
	int ret;
	u16 tte;

	if (bq->regs[BQ_FG_REG_TTE] == INVALID_REG_ADDR) {
		bq_err("Time To Empty not supported!\n");
		return -EINVAL;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TTE], &tte);

	if (ret < 0) {
		bq_err("could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return tte;
}

static int fg_read_charging_current(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CHG_CUR] == INVALID_REG_ADDR) {
		bq_err(" not supported!\n");
		return -EINVAL;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_CUR], &cc);

	if (ret < 0) {
		bq_err("could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return cc;
}

static int fg_read_charging_voltage(struct bq_fg_chip *bq)
{
	int ret;
	u16 cv;

	if (bq->regs[BQ_FG_REG_CHG_VOL] == INVALID_REG_ADDR) {
		bq_err(" not supported!\n");
		return -EINVAL;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_VOL], &cv);

	if (ret < 0) {
		bq_err("could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return cv;
}

static int fg_get_batt_status(struct bq_fg_chip *bq)
{

	fg_read_status(bq);

	if (bq->batt_fc)
		return POWER_SUPPLY_STATUS_FULL;
	else if (bq->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (bq->batt_curr > 0)
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


static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
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
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int fg_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
	int ret;
	u16 flags;

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
		if (ret < 0)
			val->strval = "unknown";
		else
			val->strval = "bq27z561";
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_get_batt_status(bq);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = fg_read_volt(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_volt = ret;
		val->intval = bq->batt_volt * 1000;
		mutex_unlock(&bq->data_lock);

		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		fg_read_current(bq, &bq->batt_curr);
		val->intval = -bq->batt_curr * 1000;
		mutex_unlock(&bq->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc >= 0) {
			val->intval = bq->fake_soc;
			break;
		}
		ret = fg_read_rsoc(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_soc = ret;
		val->intval = bq->batt_soc;
		mutex_unlock(&bq->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(bq);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_temp != -EINVAL) {
			val->intval = bq->fake_temp;
			break;
		}
		ret = fg_read_temperature(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_temp = ret;
		val->intval = bq->batt_temp;
		mutex_unlock(&bq->data_lock);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = fg_read_tte(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_tte = ret;

		val->intval = bq->batt_tte;
		mutex_unlock(&bq->data_lock);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = fg_read_fcc(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_fcc = ret;
		val->intval = bq->batt_fcc * 1000;
		mutex_unlock(&bq->data_lock);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq->batt_dc;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = fg_read_cyclecount(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_cyclecnt = ret;
		val->intval = bq->batt_cyclecnt;
		mutex_unlock(&bq->data_lock);
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
		val->intval = fg_read_charging_current(bq);
		if (bq->verify_digest_success == false
				&& !bq->ignore_digest_for_debug) {
			val->intval = min(val->intval, 2000);
		}
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = fg_read_charging_voltage(bq);
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = bq->verify_digest_success;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		val->intval = fg_get_fastcharge_mode(bq);
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
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		bq->verify_digest_success = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(bq, !!val->intval);
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
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
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
		bq_err("Failed to register fg_psy");
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
	len = snprintf(temp_buf, sizeof(temp_buf), "Ra Flag:0x%02X\n", t_buf[0] << 8 | t_buf[1]);
	memcpy(&buf[idx], temp_buf, len);
	idx += len;
	len = snprintf(temp_buf, sizeof(temp_buf), "RaTable:\n");
	memcpy(&buf[idx], temp_buf, len);
	idx += len;
	for (i = 1; i < 16; i++) {
		len = snprintf(temp_buf, sizeof(temp_buf), "%d ", t_buf[i*2] << 8 | t_buf[i*2 + 1]);
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

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, 0x4146, t_buf, 2);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(buf), "Qmax Cell 0 = %d\n", (t_buf[0] << 8) | t_buf[1]);

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
	u8 random[32] = {0};
	char kbuf[70] = {0};

	memset(kbuf, 0, sizeof(kbuf));
	strlcpy(kbuf, buf, count - 1);

	StringToHex(kbuf, random, &i);
	fg_sha256_auth(bq, random, BATTERY_DIGEST_LEN);

	return count;
}

static DEVICE_ATTR(verify_digest, 0660, verify_digest_show, verify_digest_store);
static DEVICE_ATTR(RaTable, S_IRUGO, fg_attr_show_Ra_table, NULL);
static DEVICE_ATTR(Qmax, S_IRUGO, fg_attr_show_Qmax, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_RaTable.attr,
	&dev_attr_Qmax.attr,
	&dev_attr_verify_digest.attr,
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
			bq_dbg("Reg[%02X] = 0x%04X\n", fg_dump_regs[i], val);
	}

	return ret;
}

static void fg_update_status(struct bq_fg_chip *bq)
{
	mutex_lock(&bq->data_lock);

	bq->batt_soc = fg_read_rsoc(bq);
	bq->batt_volt = fg_read_volt(bq);
	fg_read_current(bq, &bq->batt_curr);
	bq->batt_temp = fg_read_temperature(bq);
	bq->batt_rm = fg_read_rm(bq);
	bq->batt_resistance = fg_read_resistance(bq);
	fg_read_resistance_id(bq);

	mutex_unlock(&bq->data_lock);
	bq_log("RSOC:%d, Volt:%d, Current:%d, Temperature:%d\n",
			bq->batt_soc, bq->batt_volt, bq->batt_curr, bq->batt_temp);
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);
	int rc;

	rc = fg_dump_registers(bq);
	/*if (rc < 0)
		return;*/

	fg_update_status(bq);
	if (bq->fg_psy)
		power_supply_changed(bq->fg_psy);

	schedule_delayed_work(&bq->monitor_work, 10 * HZ);
}
static int bq_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret;

	ret = of_property_read_u32(node, "bq,charge-full-design",
			&bq->batt_dc);
	if (ret < 0) {
		bq_err("failed to get bq,charge-full-designe\n");
		bq->batt_dc = DEFUALT_FULL_DESIGN;
		return ret;
	}

	bq->ignore_digest_for_debug = of_property_read_bool(node,
				"bq,ignore-digest-debug");

	return 0;
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

	bq->batt_soc	= -ENODATA;
	bq->batt_fcc	= -ENODATA;
	bq->batt_rm	= -ENODATA;
	bq->batt_dc	= -ENODATA;
	bq->batt_volt	= -ENODATA;
	bq->batt_temp	= -ENODATA;
	bq->batt_curr	= -ENODATA;
	bq->batt_cyclecnt = -ENODATA;

	bq->fake_soc	= -EINVAL;
	bq->fake_temp	= -EINVAL;

	if (bq->chip == BQ27Z561) {
		regs = bq27z561_regs;
	} else {
		bq_err("unexpected fuel gauge: %d\n", bq->chip);
		regs = bq27z561_regs;
	}
	memcpy(bq->regs, regs, NUM_REGS);

	i2c_set_clientdata(client, bq);

	bq_parse_dt(bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	device_init_wakeup(bq->dev, 1);

	fg_psy_register(bq);

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret)
		bq_err("Failed to register sysfs, err:%d\n", ret);

	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, 10 * HZ);

	bq_log("bq fuel gauge probe successfully, %s\n",
			device2str[bq->chip]);

	return 0;
}


static int bq_fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	cancel_delayed_work(&bq->monitor_work);

	return 0;
}

static int bq_fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	schedule_delayed_work(&bq->monitor_work, 10 * HZ);

	power_supply_changed(bq->fg_psy);

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
	pr_info("bq fuel gauge driver shutdown!\n");
}

static struct of_device_id bq_fg_match_table[] = {
	{.compatible = "ti,bq27z561",},
	{},
};
MODULE_DEVICE_TABLE(of, bq_fg_match_table);

static const struct i2c_device_id bq_fg_id[] = {
	{ "bq27z561", BQ27Z561 },
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

