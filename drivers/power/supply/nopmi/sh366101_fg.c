/*
 * Fuelgauge battery driver
 *
 * Copyright (C) 2021 SinoWealth
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#define pr_fmt(fmt)	"[sh366101] %s(%d): " fmt, __func__, __LINE__
//#include <stdbool.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/poll.h>
#include <sh366101_fg.h>
#include "sh366101_iio.h"
#include <linux/pmic-voter.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#undef FG_DEBUG
#define FG_DEBUG 0
#if FG_DEBUG
#undef pr_debug
#undef pr_info
#define pr_debug pr_err
#define pr_info pr_err
#endif

#define VERSION_INFO_LENGTH  (sizeof(union power_supply_propval))
enum sh_fg_reg_idx {
	SH_FG_REG_DEVICE_ID = 0,
	SH_FG_REG_CNTL,
	SH_FG_REG_INT,
	SH_FG_REG_STATUS,
	SH_FG_REG_SOC,
	SH_FG_REG_OCV,
	SH_FG_REG_VOLTAGE,
	SH_FG_REG_CURRENT,
	SH_FG_REG_TEMPERATURE_IN,
	SH_FG_REG_TEMPERATURE_EX,
	/* 20211029, Ethan */
/* 	SH_FG_REG_SHUTDOWN_EN, */
/*	SH_FG_REG_SHUTDOWN, */
	SH_FG_REG_BAT_RMC,
	SH_FG_REG_BAT_FCC,
	SH_FG_REG_RESET,
	SH_FG_REG_SOC_CYCLE,
	SH_FG_REG_DESIGN_CAPCITY, /* 20211116, Ethan */
	NUM_REGS,
};

static u32 sh366101_regs[NUM_REGS] = {
    CMDMASK_ALTMAC_R | 0x0001, /* DEVICE_ID */
    CMDMASK_ALTMAC_R | 0x0000, /* CNTL */
    CMDMASK_SINGLE | 0x6E,     /* INT */
    0x06,		       /* STATUS */
    0x1C,		       /* SOC */
    0x64,		       /* OCV */
    0x04,		       /* VOLTAGE */
    0x10,		       /* CURRENT */
    0x1E,		       /* TEMPERATURE_IN */
    0x02,		       /* TEMPERATURE_EX */
//    CMDMASK_ALTMAC_W | 0x1B,   /* SHUTDOWN_EN */
//    CMDMASK_ALTMAC_W | 0x1C,   /* SHUTDOWN */
    0x0C,		       /* BAT_RMC */
    0x0E,		       /* BAT_FCC */
    CMDMASK_ALTMAC_W | 0x41,   /* RESET */
    0x1A,		       /* SOC_CYCLE */
    0x3C,		       /* SH_FG_REG_DESIGN_CAPCITY. 20211116, Ethan */
};

enum sh_fg_device {
	SH366101,
};

enum sh_fg_temperature_type {
	TEMPERATURE_IN = 0,
	TEMPERATURE_EX,
};

const unsigned char* device2str[] = {
    "sh366101",
};

enum battery_table_type {
	BATTERY_TABLE0 = 0,
	BATTERY_TABLE1,
	BATTERY_TABLE2,
	BATTERY_TABLE_MAX,
};

#define BATT_MA_AVG_SAMPLES	8
struct batt_params {
	bool			update_now;
	int			batt_raw_soc;
	int			batt_soc;
	int			samples_num;
	int			samples_index;
	int			batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	int			batt_ma_avg;
	int			batt_ma_prev;
	int			batt_ma;
	int			batt_mv;
	int			batt_temp;
	//struct timespec		last_soc_change_time;
};

/* 20211112, Ethan. Charge Block */
enum sh_fg_charge_temper_range {
	TEMPER_RANGE_T1T2 = 0, /* value also used as block_chargingvoltage index */
	TEMPER_RANGE_T2T3 = 1,
	TEMPER_RANGE_T3T4 = 2,
	TEMPER_RANGE_T4T5 = 3,
	TEMPER_RANGE_T5T6 = 4,
	TEMPER_RANGE_BELOW_T1 = 5,
	TEMPER_RANGE_ABOVE_T6 = 6,
};

/* 20211112, Ethan. Charge Block */
enum sh_fg_charge_degrade_flag {
	DEGRADE_PHASE_0 = 0,
	DEGRADE_PHASE_1,
	DEGRADE_PHASE_2,
	DEGRADE_PHASE_3,
	DEGRADE_PHASE_4,
};

struct sh_fg_chip;

struct sh_fg_chip {
	struct device* dev;
	struct i2c_client* client;
	struct mutex i2c_rw_lock; /* I2C Read/Write Lock */
	struct mutex data_lock;	  /* Data Lock */
	struct mutex cali_lock;	  /* Cali Lock. 20220106, Ethan */
	u8 chip;
	u32 regs[NUM_REGS];
	s32 batt_id;
	s32 gpio_int;

	struct notifier_block nb;

	/* Status Tracking */
	bool batt_present;
	bool batt_fc;	/* Battery Full Condition */
	bool batt_tc;	/* Battery Full Condition */
	bool batt_ot;	/* Battery Over Temperature */
	bool batt_ut;	/* Battery Under Temperature */
	bool batt_soc1; /* SOC Low */
	bool batt_socp; /* SOC Poor */
	bool batt_dsg;	/* Discharge Condition*/
	s32 batt_soc;
	s32 batt_ocv;
	s32 batt_fcc; /* Full charge capacity */
	s32 batt_rmc; /* Remaining capacity */
	s32 batt_designcap; /* 20211116, Ethan */
	s32 batt_volt;
	s32 aver_batt_volt;
	s32 batt_temp;
	s32 fake_temp;
	s32 fake_cycle_count;
	s32 fake_soc;
	s32 batt_curr;
	s32 is_charging;    /* Charging informaion from charger IC */
	s32 batt_soc_cycle; /* Battery SOC cycle */
	s32 health;
	s32 recharge_vol;
	bool usb_present;
	bool batt_sw_fc;
	bool fast_mode;
	bool shutdown_delay_enable;
	bool shutdown_delay;
	u8 check_fw_version;
	bool soc_reporting_ready;

	/* 20211210, Ethan */
	u32 dtsi_version_fw, dtsi_version_fwdate, dtsi_version_ts, dtsi_version_afi;
	u32 chip_version_fw, chip_version_fwdate, chip_version_ts, chip_version_afi;
	u32 fw_update_error_state,  fw_update_config;
	char version_info[VERSION_INFO_LENGTH];

	/* previous battery voltage current*/
	s32 p_batt_voltage;
	s32 p_batt_current;

	/* DT */
	bool en_temp_ex;
	bool en_temp_in;
	bool en_batt_det;
	s32 fg_irq_set;

	struct delayed_work monitor_work;
	u64 last_update;
	u64 log_lastUpdate; /* 20211025, Ethan */
	struct votable* fcc_votable;
	struct votable* fv_votable;
	struct votable* chg_dis_votable;

	enum sh_fg_charge_temper_range temper_range; /* 20211112, Ethan. Charge Block */
	enum sh_fg_charge_degrade_flag degrade_flag; /* 20211112, Ethan. Charge Block */
	s32 terminate_voltage;			     /* 20211112, Ethan. Termniate Voltage */
	struct dentry* debug_root;
	struct power_supply* fg_psy;
#if !(IS_PACK_ONLY)
	struct power_supply* usb_psy;
	struct power_supply* batt_psy;
	struct power_supply* bbc_psy;
#endif
	struct power_supply_desc fg_psy_d;

	struct batt_params  	param;
	struct delayed_work 	soc_monitor_work;

	struct iio_dev  *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel      *int_iio_chans;
	struct iio_channel	**ds_iio;
	struct iio_channel	**nopmi_chg_iio;
	struct iio_channel	**cp_iio;
	struct iio_channel	**main_iio;
	struct iio_channel	*bat_id_vol;
};

static bool fg_init(struct i2c_client* client);
/*
extern int board_id_get_hwversion_product_num(void);
extern int board_id_get_hwversion_major_num(void);
extern int batt_id_get_voltage(void);
*/

enum sh366101_iio_type {
	SM_DS,
	SM_USB,
	SM_CP,
	SM_MAIN,
};

enum nopmi_chg_iio_channels {
	NOPMI_CHG_USB_REAL_TYPE,
};

static const char * const nopmi_chg_iio_chan_name[] = {
	[NOPMI_CHG_USB_REAL_TYPE] = "usb_real_type",
};

enum main_iio_channels {
	MAIN_CHARGE_DONE,
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHARGE_DONE] = "charge_done",
};

static bool is_nopmi_chg_chan_valid(struct sh_fg_chip *sm,
		enum nopmi_chg_iio_channels chan)
{
	int rc;

	if(IS_ERR_OR_NULL(sm->nopmi_chg_iio) || IS_ERR(sm->nopmi_chg_iio[chan])) {
		return false;
        }
	if (!sm->nopmi_chg_iio[chan]) {
		sm->nopmi_chg_iio[chan] = iio_channel_get(sm->dev,
					nopmi_chg_iio_chan_name[chan]);
		if (IS_ERR(sm->nopmi_chg_iio[chan])) {
			rc = PTR_ERR(sm->nopmi_chg_iio[chan]);
			if (rc == -EPROBE_DEFER)
				sm->nopmi_chg_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				nopmi_chg_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_main_chan_valid(struct sh_fg_chip *sm,
		enum main_iio_channels chan)
{
	int rc;
	if(!sm->bbc_psy) {
		sm->bbc_psy = power_supply_get_by_name("bbc");
	}
	if(!sm->bbc_psy){
		return false;
        }
	if(IS_ERR_OR_NULL(sm->main_iio) || IS_ERR(sm->main_iio[chan])){
		return false;
        }
	if (!sm->main_iio[chan]) {
		sm->main_iio[chan] = iio_channel_get(sm->dev,
					main_iio_chan_name[chan]);
		if (IS_ERR(sm->main_iio[chan])) {
			rc = PTR_ERR(sm->main_iio[chan]);
			if (rc == -EPROBE_DEFER)
				sm->main_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				main_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

int sh366101_get_iio_channel(struct sh_fg_chip *sm,
			enum sh366101_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
	case SM_DS:
	/*
		if (!is_ds_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->ds_iio[channel];
	*/
		break;
	case SM_USB:
		if (!is_nopmi_chg_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->nopmi_chg_iio[channel];
		break;
	case SM_CP:
	/*
		if (!is_cp_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->cp_iio[channel];
	*/
		break;
	case SM_MAIN:
		if (!is_main_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->main_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);
	pr_err("rc = %d, val = %d.\n",rc ,val);

	return rc < 0 ? rc : 0;
}

int sh366101_set_iio_channel(struct sh_fg_chip *sm,
			enum sh366101_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
	case SM_DS:
	/*
		if (!is_ds_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->ds_iio[channel];
	*/
		break;
	case SM_USB:
		if (!is_nopmi_chg_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->nopmi_chg_iio[channel];
		break;
	case SM_CP:
	/*
		if (!is_cp_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->cp_iio[channel];
	*/
		break;
	case SM_MAIN:
		if (!is_main_chan_valid(sm, channel))
			return -ENODEV;
		iio_chan_list = sm->main_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
static int __fg_read_word(struct i2c_client* client, u8 reg, u16* val)
{
	s32 ret;
	int retry = 0;

	for(retry = 0; retry < 3; retry++){
		ret = i2c_smbus_read_word_data(client, reg); /* little endian */
		if (ret < 0) {
			pr_err("__fg_read_word fail: can't read from reg 0x%02X, retry: %d\n", reg, retry);
			udelay(200);
		}else{
			break;
		}
	}
	if(retry >= 3){
		*val = 0;
		return ret;
	}
	*val = (u16)ret;

	return 0;
}

static int __fg_write_word(struct i2c_client* client, u8 reg, u16 val)
{
	s32 ret;
	int retry = 0;

	for(retry = 0; retry < 3; retry++){
		ret = i2c_smbus_write_word_data(client, reg, val); /* little endian */
		if (ret < 0) {
			pr_err("__fg_write_word fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
			udelay(200);
		}else{
			break;
		}
	}
	if(retry >= 3){
		return ret;
	}

	return 0;
}

static int fg_read_sbs_word(struct sh_fg_chip* sm, u32 reg, u16* val)
{
	int ret = -1;

	//pr_info("fg_read_sbs_word start, reg=%08X", reg);
	/*
	if (sm->skip_reads) {
		*val = 0;
		return 0;
	}
	*/

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_R) == CMDMASK_ALTMAC_R) { /* 20211116, Ethan */
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_read_sbs_word_end;

		HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */
		ret = __fg_read_word(sm->client, CMD_ALTBLOCK, val);
	} else {
		ret = __fg_read_word(sm->client, (u8)reg, val);
	}
fg_read_sbs_word_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

#if 1
static int fg_write_sbs_word(struct sh_fg_chip* sm, u32 reg, u16 val)
{
	int ret;

	/*
	if (sm->skip_writes)
		return 0;
	*/

	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_write_word(sm->client, (u8)reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}
#endif

/* return -1: error; else return string valid length */
static s32 print_buffer(char* str, s32 strlen, u8* buf, s32 buflen)
{
#define PRINT_BUFFER_FORMAT_LEN 3
	s32 i, j;

	if ((strlen <= 0) || (buflen <= 0))
		return -1;

	memset(str, 0, strlen * sizeof(char));

	j = min(buflen, strlen / PRINT_BUFFER_FORMAT_LEN);
	for (i = 0; i < j; i++) {
		sprintf(&str[i * PRINT_BUFFER_FORMAT_LEN], "%02X ", buf[i]);
	}

	return i * PRINT_BUFFER_FORMAT_LEN;
}

static s32 __fg_read_buffer(struct i2c_client* client, u8 reg, u8 length, u8* val)
{
	static struct i2c_msg msg[2];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(u8);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = val;
	msg[1].len = length;

	return (s32)i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
}
static int fg_read_block(struct sh_fg_chip* sm, u32 reg, u8 length, u8* val)
{
	int ret = -1;
	int i;
	u8 sum;
	u16 checksum;

	/*
	if (sm->skip_reads) {
		*val = 0;
		return 0;
	}
	*/

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_R) == CMDMASK_ALTMAC_R) { /* 20211116, Ethan */
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_read_block_end;
		msleep(CMD_SBS_DELAY);

		if (length > 32)
			length = 32;

		ret = __fg_read_buffer(sm->client, CMD_ALTBLOCK, length, val);
		if (ret < 0)
			goto fg_read_block_end;
		msleep(CMD_SBS_DELAY);

		/* check buffer */
		ret = __fg_read_word(sm->client, CMD_ALTCHK, &checksum);
		if (ret < 0)
			goto fg_read_block_end;

		i = (checksum >> 8) - 4;
		if (i <= 0)
			goto fg_read_block_end;

		sum = (u8)(reg & 0xFF) + (u8)((reg >> 8) & 0xFF);
		while (i--)
			sum += val[i];
		sum = ~sum;
		if (sum != (u8)checksum)
			ret = -1;
		else
			ret = 0;
	} else {
		ret = __fg_read_buffer(sm->client, reg, length, val);
	}

fg_read_block_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

#if 1
static s32 __fg_write_buffer(struct i2c_client* client, u8 reg, u8 length, u8* val)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	s32 ret;

	if (!client->adapter)
		return -ENODEV;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN)) {
		pr_err("i2c write buffer fail: length invalid!");
		return -1;
	}

	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = reg;
	memcpy(&write_buf[1], val, length);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", reg);
		return (s32)ret;
	}

	return 0;
}

/* 20211116, Ethan */
static int fg_write_block(struct sh_fg_chip* sm, u32 reg, u8 length, u8* val)
{
	int ret;
	int i;
	u8 sum;
	u16 checksum;

	if (length > 32)
		length = 32;

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_W) == CMDMASK_ALTMAC_W) {
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_write_block_end;
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_buffer(sm->client, CMD_ALTBLOCK, length, val);
		if (ret < 0)
			goto fg_write_block_end;
		msleep(CMD_SBS_DELAY);

		sum = (u8)reg + (u8)(reg >> 8);
		for (i = 0; i < length; i++)
			sum += val[i];
                sum = ~sum;
		//sum = (u8)(0x100 - sum);
		checksum = length + 4;
		checksum = (checksum << 8) | sum;

		ret = __fg_write_word(sm->client, CMD_ALTCHK, checksum);
		if (ret < 0)
			goto fg_write_block_end;
	} else {
		ret = __fg_write_buffer(sm->client, (u8)reg, length, val);
	}
fg_write_block_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}
#endif

#if 1 /* 20211026, Ethan. FileDecode Struct */
struct sh_decoder;

struct sh_decoder {
	u8 addr;
	u8 reg;
	u8 length;
	u8 buf_first_val;
};

static s32 fg_decode_iic_read(struct sh_fg_chip* sm, struct sh_decoder* decoder, u8* pBuf)
{
	static struct i2c_msg msg[2];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	s32 ret;

	if (!sm->client->adapter)
		return -ENODEV;

	mutex_lock(&sm->i2c_rw_lock);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = &(decoder->reg);
	msg[0].len = sizeof(u8);
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = pBuf;
	msg[1].len = decoder->length;
	ret = (s32)i2c_transfer(sm->client->adapter, msg, ARRAY_SIZE(msg));

	mutex_unlock(&sm->i2c_rw_lock);
	return ret;
}

static s32 fg_decode_iic_write(struct sh_fg_chip* sm, struct sh_decoder* decoder)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	u8 length = decoder->length;
	s32 ret;

	if (!sm->client->adapter)
		return -ENODEV;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN)) {
		pr_err("i2c write buffer fail: length invalid!");
		return -1;
	}

	mutex_lock(&sm->i2c_rw_lock);
	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = decoder->reg;
	memcpy(&write_buf[1], &(decoder->buf_first_val), length);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	ret = i2c_transfer(sm->client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", decoder->reg);
	}

	mutex_unlock(&sm->i2c_rw_lock);
	return (ret < 0) ? ret : 0;
}
#endif

static int fg_read_status(struct sh_fg_chip* sm)
{
	int ret;
	u16 flags1, cntl;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_CNTL], &cntl);
	if (ret < 0)
		return ret;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_STATUS], &flags1);
	if (ret < 0)
		return ret;

	pr_debug("cntl=0x%04X, bat_flags=0x%04X", cntl, flags1);
	mutex_lock(&sm->data_lock);
	sm->batt_present = !!(flags1 & FG_STATUS_BATT_PRESENT);
	sm->batt_ot = !!(flags1 & FG_STATUS_HIGH_TEMPERATURE);
	sm->batt_ut = !!(flags1 & FG_STATUS_LOW_TEMPERATURE);
	sm->batt_tc = !!(flags1 & FG_STATUS_TERM_SOC);
	sm->batt_fc = !!(flags1 & FG_STATUS_FULL_SOC);
	sm->batt_soc1 = !!(flags1 & FG_STATUS_LOW_SOC2);
	sm->batt_socp = !!(flags1 & FG_STATUS_LOW_SOC1);
	sm->batt_dsg = !!(flags1 & FG_OP_STATUS_CHG_DISCHG);
	mutex_unlock(&sm->data_lock);

	return 0;
}

#if (FG_REMOVE_IRQ == 0)
static int fg_status_changed(struct sh_fg_chip* sm)
{
	cancel_delayed_work(&sm->monitor_work);
	schedule_delayed_work(&sm->monitor_work, 0);
	power_supply_changed(sm->fg_psy);
	pr_err("FG_IRQ_IN");
	return IRQ_HANDLED;
}

static irqreturn_t fg_irq_thread(int irq, void* dev_id)
{
	struct sh_fg_chip* sm = dev_id;

	fg_status_changed(sm);
	pr_debug("fg_read_int");

	return 0;
}
#endif

static s32 fg_read_gaugeinfo_block(struct sh_fg_chip* sm)
{
	static u8 buf[GAUGEINFO_LEN];
	static char str[GAUGESTR_LEN];
	int i, j = 0;

	int ret;
	u16 temp;

	/* 20211029, Ethan. Tick Start*/
	u64 jiffies_now = get_jiffies_64(); /* 20211203, Ethan */
	s64 tick = (s64)(jiffies_now - sm->log_lastUpdate);
	if (tick < 0)  //overflow
	{
		tick = (s64)(U64_MAXVALUE - (u64)sm->log_lastUpdate);
		tick += jiffies_now + 1;
	}
	tick /= HZ;
	if (tick < GAUGE_LOG_MIN_TIMESPAN)
		return 0;

	if (!mutex_trylock(&sm->cali_lock)) { /* 20220106, Ethan */
		pr_err("SH366101_GaugeLog: could not get mutex cali_lock!");
		return 0;
	}

	sm->log_lastUpdate = jiffies_now;
	/* 20211029, Ethan. Tick End */

	/* Cali Info */
	ret = fg_read_block(sm, CMD_CALIINFO, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CALIINFO, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now); //20211111, Ethan. In case print twice
	i += sprintf(&str[i], "Elasp=%d, ", tick);
	i += sprintf(&str[i], "Voltage=%d, ", (s16)BUF2U16_LT(&buf[12]));
	i += sprintf(&str[i], "Current=%d, ", (s16)BUF2U32_LT(&buf[8]));
	i += sprintf(&str[i], "TS1Temp=%d, ", (s16)(BUF2U16_LT(&buf[22]) - TEMPER_OFFSET));
	i += sprintf(&str[i], "IntTemper=%d, ", (s16)(BUF2U16_LT(&buf[18]) - TEMPER_OFFSET));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: CMD_CALIINFO is %s", str);

	/* 20211116, Ethan. Charge Info */
	ret = fg_read_block(sm, CMD_CHARGESTATUS, LEN_CHARGESTATUS, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CHARGESTATUS, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	ret = BUF2U16_BG(&buf[1]);
	ret |= ((u32)buf[0]) << 16;

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "ChgStatus=0x%06X, ", (u32)(ret & 0xFFFFFF));
	i += sprintf(&str[i], "DegradeFlag=0x%08X, ", BUF2U32_BG(&buf[3]));

	/* 20211116, Ethan. Term Volt */
	ret = fg_read_block(sm, CMD_TERMINATEVOLT, LEN_TERMINATEVOLT, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read TERMINATEVOLT, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	i += sprintf(&str[i], "TermVolt=%d, ", BUF2U16_LT(&buf[0]));
	i += sprintf(&str[i], "TermVoltTime=%d, ", buf[2]);

	/* 20211123, Ethan */
	ret = fg_read_sbs_word(sm, CMD_CONTROLSTATUS, &temp);
		if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_CONTROLSTATUS, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	i += sprintf(&str[i], "ControlStatus=0x%04X, ", temp);

	ret = fg_read_sbs_word(sm, CMD_RUNFLAG, &temp);
		if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_RUNFLAG, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	i += sprintf(&str[i], "Flags=0x%04X, ", temp);

	ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &temp); /* 20211126, Ethan */
		if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_OEMFLAG, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	i += sprintf(&str[i], "OEMFLAG=0x%04X, ", temp);


	j = max(i, j);
	pr_err("SH366101_GaugeLog: CHARGESTATUS is %s", str);

	/* Lifetime Info. 20211126, Ethan */
	ret = fg_read_block(sm, CMD_LIFETIMEADC, LEN_LIFETIMEADC, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read LIFETIMEADC, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "LT_MaxVolt=%d, ", (s16)BUF2U16_BG(&buf[0]));
	i += sprintf(&str[i], "LT_MinVolt=%d, ", (s16)BUF2U16_BG(&buf[2]));
	i += sprintf(&str[i], "LT_MaxChgCUR=%d, ", (s16)BUF2U16_BG(&buf[4]));
	i += sprintf(&str[i], "LT_MaxDsgCUR=%d, ", (s16)BUF2U16_BG(&buf[6]));
	i += sprintf(&str[i], "LT_MaxTemper=%d, ", (s8)buf[8]);
	i += sprintf(&str[i], "LT_MinTemper=%d, ", (s8)buf[9]);
	i += sprintf(&str[i], "LT_MaxIntTemper=%d, ", (s8)buf[10]);
	i += sprintf(&str[i], "LT_MinIntTemper=%d, ", (s8)buf[11]);
	j = max(i, j);
	pr_err("SH366101_GaugeLog: LIFETIMEADC is %s", str);

	/* Gauge Info */
	ret = fg_read_block(sm, CMD_GAUGEINFO, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read GAUGEINFO, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "RunState=0x%08X, ", BUF2U32_LT(&buf[0]));
	i += sprintf(&str[i], "GaugeState=0x%08X, ", BUF2U32_LT(&buf[4]));
	i += sprintf(&str[i], "GaugeS2=0x%04X, ", BUF2U16_LT(&buf[8]));
	i += sprintf(&str[i], "WorkState=0x%04X, ", BUF2U16_LT(&buf[10]));
	i += sprintf(&str[i], "TimeInc=%d, ", buf[12]);
	i += sprintf(&str[i], "MainTick=%d, ", buf[13]);
	i += sprintf(&str[i], "SysTick=%d, ", buf[14]);
	i += sprintf(&str[i], "ClockH=%d, ", BUF2U16_LT(&buf[15]));
	i += sprintf(&str[i], "RamCheckT=%d, ", buf[17]);
	i += sprintf(&str[i], "AutoCaliT=%d, ", buf[18]);
	i += sprintf(&str[i], "LTHour=%d, ", buf[19]);
	i += sprintf(&str[i], "LTTimer=%d, ", buf[20]);
	i += sprintf(&str[i], "FlashT=%d, ", buf[21]);
	i += sprintf(&str[i], "LTFlag=0x%02X, ", buf[22]);
	i += sprintf(&str[i], "RSTS=0x%02X, ", buf[23]);
	j = max(i, j);
	pr_err("SH366101_GaugeLog: CMD_GAUGEINFO is %s", str);

	/* Gauge Block 2 */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK2, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_GAUGEBLOCK2, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "QF1=0x%02X, ", buf[0]);
	i += sprintf(&str[i], "QF2=0x%02X, ", buf[1]);
	i += sprintf(&str[i], "PackQmax=%d, ", (s16)BUF2U16_BG(&buf[2]));
	i += sprintf(&str[i], "CycleCount=%d, ", BUF2U16_BG(&buf[14]));
	i += sprintf(&str[i], "QmaxCount=%d, ", BUF2U16_BG(&buf[16]));
	i += sprintf(&str[i], "QmaxCycle=%d, ", BUF2U16_BG(&buf[18]));
	i += sprintf(&str[i], "VatEOC=%d, ", (s16)BUF2U16_BG(&buf[4]));
	i += sprintf(&str[i], "IatEOC=%d, ", (s16)BUF2U16_BG(&buf[6]));
	i += sprintf(&str[i], "ChgVEOC=%d, ", (s16)BUF2U16_BG(&buf[8]));
	i += sprintf(&str[i], "AVILR=%d, ", (s16)BUF2U16_BG(&buf[10]));
	i += sprintf(&str[i], "AVPLR=%d, ", (s16)BUF2U16_BG(&buf[12]));
	i += sprintf(&str[i], "ModelCount=%d, ", BUF2U16_BG(&buf[20]));
	i += sprintf(&str[i], "ModelCycle=%d, ", BUF2U16_BG(&buf[22]));
	i += sprintf(&str[i], "VCTCount=%d, ", BUF2U16_BG(&buf[24]));
	i += sprintf(&str[i], "VCTCycle=%d, ", BUF2U16_BG(&buf[26]));
	i += sprintf(&str[i], "RelaxCycle=%d, ", BUF2U16_BG(&buf[28]));
	i += sprintf(&str[i], "RatioCycle=%d, ", BUF2U16_BG(&buf[30]));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: GAUGEBLOCK2 is %s", str);

	/* Gauge Block 3 */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK3, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_GAUGEBLOCK3, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "SFR_RC=%d, ", (s16)BUF2U16_BG(&buf[0]));
	i += sprintf(&str[i], "SFR_DCC=%d, ", (s16)BUF2U16_BG(&buf[2]));
	i += sprintf(&str[i], "SFR_ICC=%d, ", (s16)BUF2U16_BG(&buf[4]));
	i += sprintf(&str[i], "RCOffset=%d, ", (s16)BUF2U16_BG(&buf[6]));
	i += sprintf(&str[i], "C0DOD1=%d, ", (s16)BUF2U16_BG(&buf[8]));
	i += sprintf(&str[i], "PasCol=%d, ", (s16)BUF2U16_BG(&buf[10]));
	i += sprintf(&str[i], "PasEgy=%d, ", (s16)BUF2U16_BG(&buf[12]));
	i += sprintf(&str[i], "Qstart=%d, ", (s16)BUF2U16_BG(&buf[14]));
	i += sprintf(&str[i], "Estart=%d, ", (s16)BUF2U16_BG(&buf[16]));
	i += sprintf(&str[i], "FastTim=%d, ", buf[18]);
	i += sprintf(&str[i], "FILFLG=0x%02X, ", buf[19]);
	i += sprintf(&str[i], "StateTime=%d, ", BUF2U32_BG(&buf[20]));
	i += sprintf(&str[i], "StateHour=%d, ", BUF2U16_BG(&buf[24]));
	i += sprintf(&str[i], "StateSec=%d, ", BUF2U16_BG(&buf[26]));
	i += sprintf(&str[i], "OCVTim=%d, ", BUF2U16_BG(&buf[28]));
	i += sprintf(&str[i], "RaCalT1=%d, ", BUF2U16_BG(&buf[30]));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: GAUGEBLOCK3 is %s", str);

	/* Gauge Block 4 */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK4, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_GAUGEBLOCK4, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "GUInx=0x%02X, ", buf[0]);
	i += sprintf(&str[i], "GULoad=0x%02X, ", buf[1]);
	i += sprintf(&str[i], "GUStatus=0x%08X, ", BUF2U32_BG(&buf[2]));
	i += sprintf(&str[i], "EodLoad=%d, ", (s16)BUF2U16_BG(&buf[6]));
	i += sprintf(&str[i], "CRatio=%d, ", (s16)BUF2U16_BG(&buf[8]));
	i += sprintf(&str[i], "C0DOD0=%d, ", (s16)BUF2U16_BG(&buf[10]));
	i += sprintf(&str[i], "C0EOC=%d, ", (s16)BUF2U16_BG(&buf[12]));
	i += sprintf(&str[i], "C0EOD=%d, ", (s16)BUF2U16_BG(&buf[14]));
	i += sprintf(&str[i], "C0ACV=%d, ", (s16)BUF2U16_BG(&buf[16]));
	i += sprintf(&str[i], "ThemT=%d, ", (s16)BUF2U16_BG(&buf[18]));
	i += sprintf(&str[i], "Told=%d, ", (s16)BUF2U16_BG(&buf[20]));
	i += sprintf(&str[i], "Tout=%d, ", (s16)BUF2U16_BG(&buf[22]));
	i += sprintf(&str[i], "RCRaw=%d, ", (s16)BUF2U16_BG(&buf[24]));
	i += sprintf(&str[i], "FCCRaw=%d, ", (s16)BUF2U16_BG(&buf[26]));
	i += sprintf(&str[i], "RERaw=%d, ", (s16)BUF2U16_BG(&buf[28]));
	i += sprintf(&str[i], "FCERaw=%d, ", (s16)BUF2U16_BG(&buf[30]));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: GAUGEBLOCK4 is %s", str);

	/* Gauge Block 5 */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK5, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_GAUGEBLOCK5, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "IdealFCC=%d, ", (s16)BUF2U16_BG(&buf[0]));
	i += sprintf(&str[i], "IdealFCE=%d, ", (s16)BUF2U16_BG(&buf[2]));
	i += sprintf(&str[i], "FilRC=%d, ", (s16)BUF2U16_BG(&buf[4]));
	i += sprintf(&str[i], "FilFCC=%d, ", (s16)BUF2U16_BG(&buf[6]));
	i += sprintf(&str[i], "FSOC=%d, ", buf[8]);
	i += sprintf(&str[i], "TrueRC=%d, ", (s16)BUF2U16_BG(&buf[9]));
	i += sprintf(&str[i], "TrueFCC=%d, ", (s16)BUF2U16_BG(&buf[11]));
	i += sprintf(&str[i], "RSOC=%d, ", buf[13]);
	i += sprintf(&str[i], "FilRE=%d, ", (s16)BUF2U16_BG(&buf[14]));
	i += sprintf(&str[i], "FilFCE=%d, ", (s16)BUF2U16_BG(&buf[16]));
	i += sprintf(&str[i], "FSOCW=%d, ", buf[18]);
	i += sprintf(&str[i], "TrueRE=%d, ", (s16)BUF2U16_BG(&buf[19]));
	i += sprintf(&str[i], "TrueFCE=%d, ", (s16)BUF2U16_BG(&buf[21]));
	i += sprintf(&str[i], "RSOCW=%d, ", buf[23]);
	i += sprintf(&str[i], "EquRC=%d, ", (s16)BUF2U16_BG(&buf[24]));
	i += sprintf(&str[i], "EquFCC=%d, ", (s16)BUF2U16_BG(&buf[26]));
	i += sprintf(&str[i], "EquRE=%d, ", (s16)BUF2U16_BG(&buf[28]));
	i += sprintf(&str[i], "EquFCE=%d, ", (s16)BUF2U16_BG(&buf[30]));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: GAUGEBLOCK5 is %s", str);

	/* Gauge Block 6 */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK6, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_GAUGEBLOCK6, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "GaugeS3=0x%04X, ", BUF2U16_BG(&buf[0]));
	i += sprintf(&str[i], "CCON1=0x%02X, ", buf[2]);
	i += sprintf(&str[i], "CCON2=0x%02X, ", buf[3]);
	i += sprintf(&str[i], "ModelS=0x%02X, ", buf[4]);
	i += sprintf(&str[i], "FGUpdate=0x%02X, ", buf[5]);
	i += sprintf(&str[i], "FMGrid=0x%02X, ", buf[6]);
	i += sprintf(&str[i], "ToggleCnt=%d, ", buf[7]);
	i += sprintf(&str[i], "ORUpdate=0x%02X, ", buf[8]);
	i += sprintf(&str[i], "UpState=0x%02X, ", buf[9]);
	i += sprintf(&str[i], "ChgVol=%d, ", (s16)BUF2U16_BG(&buf[10]));
	i += sprintf(&str[i], "TapCur=%d, ", (s16)BUF2U16_BG(&buf[12]));
	i += sprintf(&str[i], "ChgCur=%d, ", (s16)BUF2U16_BG(&buf[14]));
	i += sprintf(&str[i], "ChgRes=%d, ", (s16)BUF2U16_BG(&buf[16]));
	i += sprintf(&str[i], "PrevI=%d, ", (s16)BUF2U16_BG(&buf[18]));
	i += sprintf(&str[i], "DeltaC=%d, ", (s16)BUF2U16_BG(&buf[20]));
	i += sprintf(&str[i], "SOCJmpCnt=%d, ", buf[22]);
	i += sprintf(&str[i], "SOWJmpCnt=%d, ", buf[23]);
	i += sprintf(&str[i], "OcvVcell=%d, ", (s16)BUF2U16_BG(&buf[24]));
	i += sprintf(&str[i], "FGMeas=%d, ", (s16)BUF2U16_BG(&buf[26]));
	i += sprintf(&str[i], "FGPrid=%d, ", (s16)BUF2U16_BG(&buf[28]));
	i += sprintf(&str[i], "FastTime=%d, ", (s16)BUF2U16_BG(&buf[30]));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: GAUGEBLOCK6 is %s", str);

	/* Gauge Fusion Model */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK_FG, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_GAUGEBLOCK_FG, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "FusionModel=");
	for (ret = 0; ret < 15; ret++)
		i += sprintf(&str[i], "0x%04X ", BUF2U16_BG(&buf[ret * 2]));
	j = max(i, j); //20211116, Ethan
	pr_err("SH366101_GaugeLog: FusionModel is %s", str); //20211116, Ethan

	//20211111, Ethan
	/* CADC Info */
	ret = fg_read_block(sm, CMD_CADCINFO, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CMD_CADCINFO, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "TEOFFSET=%d, ", (s16)BUF2U16_LT(&buf[0]));
	i += sprintf(&str[i], "UserOFFSET=%d, ", (s16)BUF2U16_LT(&buf[2]));
	i += sprintf(&str[i], "BoardOffset=%d, ", (s16)BUF2U16_LT(&buf[4]));
	i += sprintf(&str[i], "CADC25DEG=%d, ", (s16)BUF2U16_LT(&buf[6]));
	i += sprintf(&str[i], "CADCKR=%d, ", (s16)BUF2U16_LT(&buf[8]));
	i += sprintf(&str[i], "ChosenOffset=%d, ", (s16)BUF2U16_LT(&buf[10]));
	i += sprintf(&str[i], "TELiner=%d, ", (s16)BUF2U16_LT(&buf[12]));
	i += sprintf(&str[i], "UserLiner=%d, ", (s16)BUF2U16_LT(&buf[14]));
	i += sprintf(&str[i], "CurLiner=%d, ", (s16)BUF2U16_LT(&buf[16]));
	i += sprintf(&str[i], "COR=%d, ", (s16)BUF2U16_LT(&buf[18]));
	i += sprintf(&str[i], "CADC=%d, ", (s32)BUF2U32_LT(&buf[20]));
	i += sprintf(&str[i], "Current=%d, ", (s16)BUF2U16_LT(&buf[24]));
	i += sprintf(&str[i], "PackConfig=0x%04X, ", BUF2U16_LT(&buf[26]));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: CADCINFO is %s", str);

	//20211116, Ethan
	// j = max(i, j);
	// pr_err("SH366101_GaugeLog: FusionModel is %s", str);
	//pr_debug("SH366101_GaugeLog: max len=%d", j);

	ret = 0;

fg_read_gaugeinfo_block_end: /* 20220105, Ethan */
	mutex_unlock(&sm->cali_lock); /* 20220105, Ethan */
	return ret;
}

static s32 fg_read_soc(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;
	static u8 buf[GAUGEINFO_LEN];

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_SOC], &data);
	if (ret < 0) {
		pr_err("could not read SOC, ret = %d\n", ret);
		return ret;
	}
	data = data * 100;
	if(data==0){
		ret = fg_read_block(sm, CMD_GAUGEINFO, GAUGEINFO_LEN, buf);
		if(ret < 0) {
			pr_err("SH366101_Gaugelog: could not read GAUGEINFO, ret = %d\n", ret);
			return ret;
		}
		pr_info("Gaugestatus2 = 0x%02x%02x\n",buf[6],buf[7]);
		if((buf[6] & CMDMASK_LOCK1_TRIGGERED)==0){
			data = 50;
		}
	}

	mutex_lock(&sm->data_lock);
	sm->batt_soc = data;
	mutex_unlock(&sm->data_lock);
	return 0;
}

static u32 fg_read_ocv(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_OCV], &data);
	if (ret < 0) {
		pr_err("could not read OCV, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_ocv = (s32)((s16)data * MA_TO_UA);
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_read_temperature(struct sh_fg_chip* sm, enum sh_fg_temperature_type temperature_type)
{
	s32 ret;
	u16 data = 0;

	if (temperature_type == TEMPERATURE_IN) {
		ret = sm->regs[SH_FG_REG_TEMPERATURE_IN];
	} else if (temperature_type == TEMPERATURE_EX) {
		ret = sm->regs[SH_FG_REG_TEMPERATURE_EX];
	} else {
		return -EINVAL;
	}

	ret = fg_read_sbs_word(sm, ret, &data);
	if (ret < 0) {
		pr_err("could not read temperature, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_temp = (s32)((s16)(data - 2731));
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_read_volt(struct sh_fg_chip* sm)
{
	s32 ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_VOLTAGE], &data);
	if (ret < 0) {
		pr_err("could not read voltage, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_volt = (s32)data * MA_TO_UA;
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_get_cycle(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_SOC_CYCLE], &data);
	if (ret < 0) {
		pr_err("read cycle reg fail ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_soc_cycle = data;
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_read_current(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_CURRENT], &data);
	if (ret < 0) {
		pr_err("could not read current, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_curr = (s32)((s16)data * MA_TO_UA);
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_read_fcc(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_BAT_FCC], &data);
	if (ret < 0) {
		pr_err("could not read FCC, ret=%d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_fcc = (s32)((s16)data * MA_TO_UA);
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_read_rmc(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_BAT_RMC], &data);
	if (ret < 0) {
		pr_err("could not read RMC, ret=%d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_rmc = (s32)((s16)data * MA_TO_UA);
	mutex_unlock(&sm->data_lock);
	return 0;
}

static s32 fg_read_designcap(struct sh_fg_chip* sm) /* 20211108, Ethan */
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_DESIGN_CAPCITY], &data);
	if (ret < 0) {
		pr_err("could not read DesignCap, ret=%d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_designcap = (s32)((s16)data * MA_TO_UA); /* 20211112, Ethan */
	mutex_unlock(&sm->data_lock);
	return 0;
}

#if !(IS_PACK_ONLY)
static s32 get_battery_status(struct sh_fg_chip* sm)
{
	union power_supply_propval ret = {
	    0,
	};
	s32 rc;

	if (sm->batt_psy == NULL)
		sm->batt_psy = power_supply_get_by_name("battery");
	if (sm->batt_psy) {
		/* if battery has been registered, use the status property */
		rc = power_supply_get_property(sm->batt_psy, POWER_SUPPLY_PROP_STATUS, &ret);
		if (rc) {
			pr_err("Battery does not export status: %d\n", rc);
			return POWER_SUPPLY_STATUS_UNKNOWN;
		}
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static bool is_battery_charging(struct sh_fg_chip* sm)
{
	return get_battery_status(sm) == POWER_SUPPLY_STATUS_CHARGING;
}

static void fg_vbatocv_check(struct sh_fg_chip* sm)
{
	sm->p_batt_voltage = sm->batt_volt;
	sm->p_batt_current = sm->batt_curr;
}

static s32 fg_cal_carc(struct sh_fg_chip* sm)
{
	fg_vbatocv_check(sm);
	sm->is_charging = is_battery_charging(sm);

	return 1;
}
#endif
static int fg_get_prop_batt_id(struct sh_fg_chip* sm)
{
	int rc, bat_id_uv = 0;
	int batt_id_volt;

	sm->bat_id_vol = iio_channel_get(sm->dev, "batt_id");
	if (IS_ERR_OR_NULL(sm->bat_id_vol)) {
		pr_err("Error iio_channel_get\n");
		return -1;
	}

	rc = iio_read_channel_processed(sm->bat_id_vol, &bat_id_uv);
	if (rc < 0) {
		pr_err("Error in reading batt_id channel, rc:%d\n", rc);
		return -1;
	}

	batt_id_volt = bat_id_uv / 1000;
	if(batt_id_volt > 1350)
		sm->batt_id = 1;
	else if(batt_id_volt > 1000 && batt_id_volt < 1350)
		sm->batt_id = 2;
	else
		sm->batt_id = 0;

	return 0;
}

/* share batt_id to other modules ,this is only a API, DONOT TOUCH. */
int of_get_batt_id(void)
{
	struct sh_fg_chip dts_sm;
	fg_get_prop_batt_id(&dts_sm);
	return dts_sm.batt_id;
}
EXPORT_SYMBOL(of_get_batt_id);

static s32 fg_get_batt_status(struct sh_fg_chip* sm)
{
	int charge_done = 0;
	int ret = 0;

	if(sm->batt_fc){
		ret = sh366101_get_iio_channel(sm, SM_MAIN,
							MAIN_CHARGE_DONE, &charge_done);
		pr_info("ret=%d, batt_fc=%d, charge_done=%d\n", ret, sm->batt_fc, charge_done);
	}

	if (!sm->batt_present)
		return POWER_SUPPLY_STATUS_UNKNOWN;
	else if (sm->batt_fc && charge_done && ((sm->batt_soc/100)/10 > 95))
		return POWER_SUPPLY_STATUS_FULL;
	else if (sm->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (sm->batt_curr > 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static s32 fg_get_batt_capacity_level(struct sh_fg_chip* sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	else if (sm->batt_fc)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (sm->batt_tc) /* [tc] always set when [fc] set */
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (sm->batt_socp) /* [soc1] always set when [socp] set */
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		//return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else if (sm->batt_soc1)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
}

static s32 fg_get_batt_health(struct sh_fg_chip* sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	else if (sm->batt_ot)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (sm->batt_ut)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	/*
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_FG_VERSION,
	POWER_SUPPLY_PROP_SOH,
	*/
};

static void fg_monitor_workfunc(struct work_struct* work);

#define SHUTDOWN_DELAY_VOL  3300
#define FULL_CHARGE_CAPACITY  (7950*1000)  // uAh
static s32 fg_get_property(struct power_supply* psy, enum power_supply_property psp, union power_supply_propval* val)
{
	struct sh_fg_chip* sm = power_supply_get_drvdata(psy);
	//static bool shutdown_delay_cancel;
	static bool last_shutdown_delay;
	static int shutdown_soc = -1;
	union power_supply_propval pval = {0, };
	s32 last_batt_temp = -1000;
	int retry = 0;
	static u8 buf[GAUGEINFO_LEN];
	int ret;

	/*
		if (time_is_before_jiffies((unsigned long)sm->last_update + 2 * HZ)) {
			cancel_delayed_work_sync(&sm->monitor_work);
			fg_monitor_workfunc(&sm->monitor_work.work);
		}
	*/

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_get_batt_status(sm);
		/* pr_info("fg POWER_SUPPLY_PROP_STATUS:%d\n", val->intval); */
		break;
	/*
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = sm->shutdown_delay;
		break;
	*/

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#if IS_ADC_HIGHFREQ
		fg_read_volt(sm);
#endif
		val->intval = sm->batt_volt;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sm->batt_present;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
#if IS_ADC_HIGHFREQ
		fg_read_current(sm);
#endif
		val->intval = sm->batt_curr;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (sm->fake_soc > 0) {
			val->intval = sm->fake_soc;
			break;
		}
#if IS_ADC_HIGHFREQ
		fg_read_soc(sm);
#endif
		//ret = fg_read_soc(sm);
		if (sm->param.batt_soc >= 0)
			val->intval = sm->param.batt_soc;
		else if((sm->batt_soc >=0) && (sm->param.batt_soc == -EINVAL ))
				val->intval = ((sm->batt_soc + 96)/97);
		else
			val->intval = 50;

		if (sm->shutdown_delay_enable) {
		if (val->intval == 0) {
			sm->is_charging = is_battery_charging(sm);
			fg_read_volt(sm);
			pr_info("vbat_uv: %d, is_charging: %d, shutdown_delay: %d",
				 sm->batt_volt, sm->is_charging, sm->shutdown_delay);
			if (sm->is_charging && sm->shutdown_delay) {
				sm->shutdown_delay = false;
				val->intval = 1;
			} else {
				if ( sm->batt_volt/1000 > 3400) {
					val->intval = 1;
				} else if (sm->batt_volt/1000 > 3300) {
					if (!sm->is_charging) {
						sm->shutdown_delay = true;
					}
					val->intval = 1;
				} else {
					sm->shutdown_delay = false;
					val->intval = 1;
					shutdown_soc = 0;
				}
			}
		} else {
			sm->shutdown_delay = false;
		}
		if (last_shutdown_delay != sm->shutdown_delay){
				pr_info("shutdown_delay: %d => %d", last_shutdown_delay, sm->shutdown_delay);
				last_shutdown_delay = sm->shutdown_delay;
				if(sm->fg_psy){
					power_supply_changed(sm->fg_psy);
				}
					pval.intval = sm->shutdown_delay;
					msleep(500);
			}
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(sm);
		if ((shutdown_soc == 0) || (sm->batt_volt/1000 < 3300)) {
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
			}
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (sm->fake_temp == -EINVAL) {
#if IS_ADC_HIGHFREQ
			if (sm->en_temp_in)
				fg_read_temperature(sm, TEMPERATURE_IN);
			else if (sm->en_temp_ex)
				fg_read_temperature(sm, TEMPERATURE_EX);
#endif
			val->intval = sm->batt_temp;
			for(retry = 0; retry < 3; retry++){
				if((last_batt_temp > -1000)&&(abs(last_batt_temp-val->intval) > 100))
				{
					mdelay(100);
                                  	if (sm->en_temp_in){
						fg_read_temperature(sm, TEMPERATURE_IN);
					}else if (sm->en_temp_ex){
						fg_read_temperature(sm, TEMPERATURE_EX);
					}
					val->intval = sm->batt_temp;
                                  	pr_err("sh366161 retry = %d, last_batt_temp = %d, sm->batt_temp = %d\n", retry, last_batt_temp, sm->batt_temp);
				}else{
					break;
				}
			}
			last_batt_temp = sm->batt_temp;
		} else {
			val->intval = sm->fake_temp;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
#if IS_ADC_HIGHFREQ
		fg_read_fcc(sm);
#endif
		ret = fg_read_block(sm, CMD_GAUGEBLOCK2, GAUGEINFO_LEN, buf);
		if (ret < 0) {
			val->intval = sm->batt_fcc;
			break;
                }
		val->intval = ((s16)BUF2U16_BG(&buf[2]) *1000 + sm->batt_fcc)/2;
		if (val->intval >= FULL_CHARGE_CAPACITY) {
			val->intval = FULL_CHARGE_CAPACITY;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
#if IS_ADC_HIGHFREQ
		fg_read_designcap(sm);
#endif
		//val->intval = sm->batt_designcap;
		val->intval = 8000000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
#if IS_ADC_HIGHFREQ
		fg_read_rmc(sm);
#endif
		val->intval = sm->batt_rmc;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
#if IS_ADC_HIGHFREQ
		fg_get_cycle(sm);
#endif
		if (sm->fake_cycle_count == -EINVAL){
			val->intval = sm->batt_soc_cycle;
		}else{
			val->intval = sm->fake_cycle_count;
		}
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = fg_get_batt_health(sm);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	/*
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = sm->batt_id;
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
  		switch(sm->batt_id)
  		{
  			case 1:
  				val->strval = "M355-NVT-5000mAh";
  				break;
  			case 2:
 				val->strval = "M355-GuanYu-6000mAh";
  				break;
  			case 3:
  				val->strval = "M355-Sunwoda-5000mAh";
  				break;
			case 4:
  				val->strval = "M355-Sunwoda-6000mAh";
  				break;
  			default:
  				val->strval = "M355-unknown";
				break;
		}
		break;
	case POWER_SUPPLY_PROP_FRSION:
		val->intval = sm->check_fw_version;
		break;
	case POWER_SUPPLY_PROP_SOH:
		val->intval = 100;
		break;
	*/
	default:
		return -EINVAL;
	}
	return 0;
}

static s32 fg_set_property(struct power_supply* psy, enum power_supply_property prop, const union power_supply_propval* val)
{
	/* 20211029, Ethan. */
	struct sh_fg_chip* sm = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		sm->fake_temp = val->intval;
		pr_info("Set bms temp prop, value:%d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		sm->fake_cycle_count = val->intval;
		pr_info("Set bms cycle count prop, value:%d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		sm->fake_soc = val->intval;
		pr_info("Set bms capacity prop, value:%d\n", val->intval);
		//ret = fg_read_soc(sm);
		power_supply_changed(sm->fg_psy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static s32 fg_prop_is_writeable(struct power_supply* psy, enum power_supply_property prop)
{
	s32 ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void fg_external_power_changed(struct power_supply* psy)
{
	struct sh_fg_chip* sm = power_supply_get_drvdata(psy);

	cancel_delayed_work(&sm->monitor_work);
	schedule_delayed_work(&sm->monitor_work, 0);
}

static s32 fg_psy_register(struct sh_fg_chip* sm)
{
	struct power_supply_config fg_psy_cfg = {};

#if IS_PACK_ONLY
	sm->fg_psy_d.name = "battery";
	sm->fg_psy_d.type = POWER_SUPPLY_TYPE_UNKNOWN;
#else
	sm->fg_psy_d.name = "bms";
	//sm->fg_psy_d.type = POWER_SUPPLY_TYPE_BMS;
#endif
	sm->fg_psy_d.properties = fg_props;
	sm->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	sm->fg_psy_d.get_property = fg_get_property;
	sm->fg_psy_d.set_property = fg_set_property;
	sm->fg_psy_d.external_power_changed = fg_external_power_changed;
	sm->fg_psy_d.property_is_writeable = fg_prop_is_writeable;

	fg_psy_cfg.drv_data = sm;
	fg_psy_cfg.num_supplicants = 0;

	sm->fg_psy = devm_power_supply_register(sm->dev, &sm->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(sm->fg_psy)) {
		pr_err("Failed to register fg_psy");
		return PTR_ERR(sm->fg_psy);
	}

	return 0;
}

static void fg_psy_unregister(struct sh_fg_chip* sm)
{
	power_supply_unregister(sm->fg_psy);
}

static ssize_t fg_attr_show_rm(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sh_fg_chip* sm = i2c_get_clientdata(client);
	int len;

	fg_read_rmc(sm);
	len = snprintf(buf, MAX_BUF_LEN, "%d\n", sm->batt_rmc);

	return len;
}

static ssize_t fg_attr_show_fcc(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sh_fg_chip* sm = i2c_get_clientdata(client);
	int len;

	fg_read_fcc(sm);
	len = snprintf(buf, MAX_BUF_LEN, "%d\n", sm->batt_fcc);

	return len;
}

static ssize_t fg_attr_show_batt_volt(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sh_fg_chip* sm = i2c_get_clientdata(client);
	int len;

    fg_read_volt(sm);
	len = snprintf(buf, MAX_BUF_LEN, "%d\n", sm->batt_volt);

	return len;
}

static DEVICE_ATTR(rm, S_IRUGO, fg_attr_show_rm, NULL);
static DEVICE_ATTR(fcc, S_IRUGO, fg_attr_show_fcc, NULL);
static DEVICE_ATTR(batt_volt, S_IRUGO, fg_attr_show_batt_volt, NULL);

static struct attribute* fg_attributes[] = {
    &dev_attr_rm.attr,
    &dev_attr_fcc.attr,
    &dev_attr_batt_volt.attr,
    NULL,
};

static const struct attribute_group fg_attr_group = {
    .attrs = fg_attributes,
};

#if 0
static int calculate_delta_time(struct timespec *time_stamp, int *delta_time_s)
{
	struct timespec now_time;

	/* default to delta time = 0 if anything fails */
	*delta_time_s = 0;

	get_monotonic_boottime(&now_time);
	*delta_time_s = (now_time.tv_sec - time_stamp->tv_sec);

	/* remember this time */
	*time_stamp = now_time;
	return 0;
}

static void calculate_average_current(struct sh_fg_chip *sm)
{
	int i;
	int iavg_ma = sm->param.batt_ma;

	/* only continue if ibat has changed */
	if (sm->param.batt_ma == sm->param.batt_ma_prev)
		goto unchanged;
	else
		sm->param.batt_ma_prev = sm->param.batt_ma;

	sm->param.batt_ma_avg_samples[sm->param.samples_index] = iavg_ma;
	sm->param.samples_index = (sm->param.samples_index + 1) % BATT_MA_AVG_SAMPLES;
	sm->param.samples_num++;

	if (sm->param.samples_num >= BATT_MA_AVG_SAMPLES)
		sm->param.samples_num = BATT_MA_AVG_SAMPLES;

	if (sm->param.samples_num) {
		iavg_ma = 0;
		/* maintain a AVG_SAMPLES sample average of ibat */
		for (i = 0; i < sm->param.samples_num; i++) {
			pr_debug("iavg_samples_ma[%d] = %d\n", i, sm->param.batt_ma_avg_samples[i]);
			iavg_ma += sm->param.batt_ma_avg_samples[i];
		}
		sm->param.batt_ma_avg = DIV_ROUND_CLOSEST(iavg_ma, sm->param.samples_num);
	}

unchanged:
	pr_info("current_now_ma = %d, averaged_iavg_ma = %d\n",
			sm->param.batt_ma, sm->param.batt_ma_avg);
}
#endif

static int calculate_delta_time(ktime_t time_stamp, int *delta_time_s)
{
	ktime_t now_time;

	now_time = ktime_get();
	*delta_time_s = ktime_ms_delta(now_time, time_stamp) / 1000;
	if(*delta_time_s < 0)
		*delta_time_s = 0;
	return 0;
}

#define LOW_TBAT_THRESHOLD			15//Degree //150
#define CHANGE_SOC_TIME_LIMIT_10S	10
#define CHANGE_SOC_TIME_LIMIT_20S	20
#define CHANGE_SOC_TIME_LIMIT_60S	60
#define HEAVY_DISCHARGE_CURRENT		(-1000)//mA //1000
#define FORCE_TO_FULL_SOC			950 //SOC 95.0 = 950
#define MIN_DISCHARGE_CURRENT		(-25)//25 //mA
#define MIN_CHARGING_CURRENT		25//(-25) //mA
#define FULL_SOC					1000 //SOC 100.0 = 1000

#if 0
static void qg_battery_soc_smooth_tracking(struct sh_fg_chip *sm)
{
	int delta_time = 0;
	int soc_changed;
	int last_batt_soc = sm->param.batt_soc;
	int time_since_last_change_sec;
	static ktime_t last_change_time = 0;

	//struct timespec last_change_time = sm->param.last_soc_change_time;

	calculate_delta_time(last_change_time, &time_since_last_change_sec);

	if (sm->param.batt_temp > LOW_TBAT_THRESHOLD) {
		/* Battery in normal temperture */
		if (sm->param.batt_ma < 0 || abs(sm->param.batt_raw_soc - sm->param.batt_soc) > 2)
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_20S;
		else
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_60S;
	} else {
		/* Battery in low temperture */
		calculate_average_current(sm);
		/* Calculated average current > 1000mA */
		if (sm->param.batt_ma_avg > HEAVY_DISCHARGE_CURRENT || abs(sm->param.batt_raw_soc - sm->param.batt_soc > 2))
			/* Heavy loading current, ignore battery soc limit*/
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_10S;
		else
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_20S;
	}

	if (delta_time < 0)
		delta_time = 0;

	soc_changed = min(1, delta_time);

	pr_info("soc:%d, last_soc:%d, raw_soc:%d, batt_sw_fc:%d, update_now:%d, batt_ma:%d\n",
			sm->param.batt_soc, last_batt_soc, sm->param.batt_raw_soc, sm->batt_sw_fc, sm->param.update_now,
			sm->param.batt_ma);

	if (last_batt_soc >= 0) {
		if (last_batt_soc != FULL_SOC && sm->param.batt_raw_soc >= FORCE_TO_FULL_SOC && sm->batt_sw_fc == true /*sm->charge_status == POWER_SUPPLY_STATUS_FULL*/)
			/* Unlikely status */
			last_batt_soc = sm->param.update_now ? FULL_SOC : last_batt_soc + soc_changed;
		else if (last_batt_soc < sm->param.batt_raw_soc && sm->param.batt_ma > MIN_CHARGING_CURRENT)
			/* Battery in charging status
			* update the soc when resuming device
			*/
			last_batt_soc = sm->param.update_now ? sm->param.batt_raw_soc : last_batt_soc + soc_changed;
		else if (last_batt_soc > sm->param.batt_raw_soc && sm->param.batt_ma < MIN_DISCHARGE_CURRENT)
			/* Battery in discharging status
			* update the soc when resuming device
			*/
			last_batt_soc = sm->param.update_now ? sm->param.batt_raw_soc : last_batt_soc - soc_changed;

		sm->param.update_now = false;
	} else {
		last_batt_soc = sm->param.batt_raw_soc;
	}

	if (last_batt_soc > FULL_SOC)
		last_batt_soc = FULL_SOC;
	else if (last_batt_soc < 0)
		last_batt_soc = 0;

	if (sm->param.batt_soc != last_batt_soc) {
		sm->param.batt_soc = last_batt_soc;
		//sm->param.last_soc_change_time = last_change_time;
		//if (sm->batt_psy)
		//	power_supply_changed(sm->batt_psy);
	}
}
#endif

static void battery_soc_smooth_tracking_new(struct sh_fg_chip *sm)
{
	static int system_soc, last_system_soc, raw_soc;
	int soc_changed = 0, unit_time = 10, delta_time = 0, soc_delta = 0;
	static ktime_t last_change_time;

	static int firstcheck = 0;
	int change_delta = 0, rc = 0;
  	union power_supply_propval prop = {0, };
  	int charging_status = 0, charge_type = 0;

	if (!sm->usb_psy)
  		sm->usb_psy = power_supply_get_by_name("usb");
  	if (sm->usb_psy) {
  		rc = sh366101_get_iio_channel(sm, SM_USB, NOPMI_CHG_USB_REAL_TYPE, &prop.intval);
  		if (rc < 0) {
  			pr_err("sm could not get real type!\n");
  		}
  		charge_type = prop.intval;
  	}
  	if (!sm->batt_psy)
  		sm->batt_psy = power_supply_get_by_name("battery");
  	if (sm->batt_psy) {
  		rc = power_supply_get_property(sm->batt_psy, POWER_SUPPLY_PROP_STATUS, &prop);
  		if (rc < 0) {
  			pr_err("sm could not get status!\n");
  		}
  		charging_status = prop.intval;
  	}

  	/*Map system_soc value according to raw_soc */
  	raw_soc = sm->param.batt_raw_soc * 100;
  	if (raw_soc >= 9700)
  		system_soc = 100;
  	else {
  		system_soc = ((raw_soc + 96) / 97);
  		if (system_soc > 99)
  			system_soc = 99;
  	}
  	pr_info("smooth_tracking_new:charge_type:%d charging_status:%d raw_soc:%d system_soc:%d\n",
  		charge_type, charging_status, sm->param.batt_raw_soc, system_soc);
  	/*Get the initial value for the first time */
  	if (!firstcheck) {
  		last_change_time = ktime_get();
  		last_system_soc = system_soc;
  		firstcheck = 1;
  	}

	if ((sm->param.batt_raw_soc > 31 && sm->param.batt_raw_soc < 34) ||
		(sm->param.batt_raw_soc > 63 && sm->param.batt_raw_soc < 66))
	{
		unit_time = 30;
	} else {
		unit_time = 10;
	}

	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING || charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
  		&& !sm->batt_rmc && sm->param.batt_temp < LOW_TBAT_THRESHOLD && last_system_soc > 1) {
  		unit_time = 50;
  	}

	/*If the soc jump, will smooth one cap every 10S */
  	soc_delta = abs(system_soc - last_system_soc);
  	if (soc_delta >= 1 || (sm->batt_volt < 3300 && system_soc > 0)) {
  		calculate_delta_time(last_change_time, &change_delta);
  		delta_time = change_delta / unit_time;
  		if (delta_time < 0) {
  			last_change_time = ktime_get();
  			delta_time = 0;
  		}
  		soc_changed = min(1, delta_time);
  		if (soc_changed) {
  			if ((sm->batt_curr > 0 || charging_status == POWER_SUPPLY_STATUS_CHARGING || charging_status == POWER_SUPPLY_STATUS_FULL) && (system_soc > last_system_soc))
  				system_soc = last_system_soc + soc_changed;
  			else if ((sm->batt_curr < 0 || charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
  				(charging_status == POWER_SUPPLY_STATUS_CHARGING && charge_type == POWER_SUPPLY_TYPE_USB))
  				&& (system_soc < last_system_soc))
  				system_soc = last_system_soc - soc_changed;
  			else
  				system_soc = last_system_soc;
  			pr_info("soc_changed:%d charging_status:%d", soc_changed, charging_status);
  		} else
  			system_soc = last_system_soc;
  	}

	/*Avoid mismatches between charging status and soc changes  */
  	if(charging_status == POWER_SUPPLY_STATUS_DISCHARGING && (system_soc > last_system_soc))
  		system_soc = last_system_soc;

  	pr_info("smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d",
  		system_soc, last_system_soc, soc_delta);

  	if (system_soc != last_system_soc) {
  		last_change_time = ktime_get();
  		last_system_soc = system_soc;
  	}
  	sm->param.batt_soc = system_soc;
}


#define MONITOR_SOC_WAIT_MS		1000
#define MONITOR_SOC_WAIT_PER_MS		10000
static void soc_monitor_work(struct work_struct *work)
{
	struct sh_fg_chip *sm = container_of(work,
				struct sh_fg_chip,
				soc_monitor_work.work);

	/* Update battery information */
	fg_read_current(sm);
	sm->param.batt_ma = sm->batt_curr;
	if (sm->en_temp_in){
		fg_read_temperature(sm, TEMPERATURE_IN);
		sm->param.batt_temp = sm->batt_temp;
	}else if (sm->en_temp_ex){
		fg_read_temperature(sm, TEMPERATURE_EX);
		sm->param.batt_temp = sm->batt_temp;
	}
	fg_read_soc(sm);
	sm->param.batt_raw_soc = (sm->batt_soc/100);

	if (sm->soc_reporting_ready)
		//qg_battery_soc_smooth_tracking(sm);
		pr_err("soc_reporting is %d, use new battery_soc_smooth_tracking \n", sm->soc_reporting_ready);
		battery_soc_smooth_tracking_new(sm);
		pr_err("soc:%d, raw_soc:%d, batt_curr_ma:%d, fc_status:%d\n",
			sm->param.batt_soc, sm->param.batt_raw_soc,
			sm->param.batt_ma, sm->batt_sw_fc/*sm->charge_status*/);

	schedule_delayed_work(&sm->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_PER_MS));
}

/* 20211112, Ethan. Termniate Voltage */
static s32 fg_read_terminate_voltage(struct sh_fg_chip* sm)
{
	int ret;
	u8 buf[LEN_TERMINATEVOLT];

	ret = fg_read_block(sm, CMD_TERMINATEVOLT, LEN_TERMINATEVOLT, buf);
	if (ret < 0) {
		pr_err("could not read Terminate Voltage, ret=%d\n", ret);
		return ret;
	}

	ret = (s32)(buf[0] | ((u16)buf[1] << 8));
	mutex_lock(&sm->data_lock);
	sm->terminate_voltage = ret * MA_TO_UA; /* 20211208, Ethan */
	mutex_unlock(&sm->data_lock);
	return 0;
}
static void fg_refresh_status(struct sh_fg_chip* sm)
{
	bool last_batt_inserted = sm->batt_present;
	bool last_batt_fc = sm->batt_fc;
	bool last_batt_ot = sm->batt_ot;
	bool last_batt_ut = sm->batt_ut;
	//static s32 last_soc, last_temp;
	static s32 last_temp;

	fg_read_status(sm);
	pr_debug("batt_present=%d", sm->batt_present);

	if (!last_batt_inserted && sm->batt_present) { /* battery inserted */
		pr_debug("Battery inserted\n");
	} else if (last_batt_inserted && !sm->batt_present) { /* battery removed */
		pr_debug("Battery removed\n");
		sm->batt_soc = -ENODATA;
		sm->batt_fcc = -ENODATA;
		sm->batt_volt = -ENODATA;
		sm->batt_curr = -ENODATA;
		sm->batt_temp = -ENODATA;
	}

	if ((last_batt_inserted != sm->batt_present) || (last_batt_fc != sm->batt_fc) || (last_batt_ot != sm->batt_ot) || (last_batt_ut != sm->batt_ut))
		power_supply_changed(sm->fg_psy);

	if (sm->batt_present) {
		fg_read_gaugeinfo_block(sm);

		fg_read_current(sm);
		fg_read_soc(sm);
		fg_read_ocv(sm);
		fg_read_volt(sm);
		fg_get_cycle(sm);
		fg_read_rmc(sm);
		fg_read_fcc(sm);
		fg_read_designcap(sm);
		if (sm->en_temp_in)
			fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			fg_read_temperature(sm, TEMPERATURE_EX);

#if !(IS_PACK_ONLY)
		fg_cal_carc(sm);
#endif

		pr_debug("RSOC:%d, Volt:%d, Current:%d, Temperature:%d\n", sm->batt_soc, sm->batt_volt, sm->batt_curr, sm->batt_temp);
		pr_debug("RM:%d,FC:%d,FAST:%d", sm->batt_rmc, sm->batt_fcc, sm->fast_mode);

		//if ((last_soc != sm->param.batt_soc) || (last_temp != sm->batt_temp))
		if ((last_temp != sm->batt_temp)) {
			if (sm->fg_psy){
				power_supply_changed(sm->fg_psy);
			}
			if (sm->batt_psy){
				power_supply_changed(sm->batt_psy);
			}
		}

		//last_soc = sm->param.batt_soc;
		last_temp = sm->batt_temp;
		sm->soc_reporting_ready = 1;
	}

	sm->last_update = jiffies;
}

#define SH366101_FFC_TERM_WAM_TEMP 350
#define SH366101_COLD_TEMP_TERM 0
#define BAT_FULL_CHECK_TIME 1

static s32 fg_check_full_status(struct sh_fg_chip *sm)
{
	union power_supply_propval prop = {0, };
	static s32 last_term, full_check;
	s32 term_curr = 0, full_volt, rc;
	s32 interval = MONITOR_WORK_10S;

	if (!sm->usb_psy)
		return interval;

	if (!sm->chg_dis_votable)
		sm->chg_dis_votable = find_votable("CHG_DISABLE");

	if (!sm->fv_votable)
		sm->fv_votable = find_votable("FV");

	rc = power_supply_get_property(sm->usb_psy,
		POWER_SUPPLY_PROP_PRESENT, &prop);
	if (!prop.intval) {
		vote(sm->chg_dis_votable, BMS_FC_VOTER, false, 0);
		sm->batt_sw_fc = false;
		full_check = 0;
		return interval;
	}
	sm->usb_present = prop.intval;

	if (sm->fast_mode) {
		interval = MONITOR_WORK_1S;
	} else {
		if (sm->batt_temp < SH366101_COLD_TEMP_TERM) {
		} else if (sm->usb_present) {
			interval = MONITOR_WORK_5S;
		} else {
			interval = MONITOR_WORK_10S;
		}
	}
	full_volt = get_effective_result(sm->fv_votable) - 20;

	rc = power_supply_get_property(sm->usb_psy,
		POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
	if (!prop.intval) {
		return interval;
	}
	term_curr = prop.intval;

	pr_debug("term:%d, full_volt:%d, usb_present:%d, batt_sw_fc:%d", term_curr, full_volt, sm->usb_present, sm->batt_sw_fc);

	if (sm->usb_present
	&& ((sm->batt_soc/100) == SM_RAW_SOC_FULL)
	&& (sm->batt_volt > full_volt)
	&& (sm->batt_curr > 0)
	&& (sm->batt_curr < term_curr)
	&& (!sm->batt_sw_fc)) {
		full_check++;
		pr_debug("full_check:%d\n", full_check);
		if (full_check > BAT_FULL_CHECK_TIME) {
			sm->batt_sw_fc = true;
			vote(sm->chg_dis_votable, BMS_FC_VOTER, true, 0);
			pr_debug("detect charge termination sm->batt_sw_fc:%d\n", sm->batt_sw_fc);
		}
		return MONITOR_WORK_1S;
	} else {
		full_check = 0;
	}

	if (term_curr == last_term)
		return interval;

	last_term = term_curr;

	return interval;
}

static s32 fg_check_recharge_status(struct sh_fg_chip *sm)
{
	s32 rc;
	union power_supply_propval prop = {0, };

	if (!sm->batt_psy) {
		sm->batt_psy = power_supply_get_by_name("battery");
		if (!sm->batt_psy) {
			return 0;
		}
	}

	rc = power_supply_get_property(sm->batt_psy,
		POWER_SUPPLY_PROP_HEALTH, &prop);
	sm->health = prop.intval;

	if (((sm->batt_soc/100) <= SM_RECHARGE_SOC)
		&& sm->batt_sw_fc
		&& (sm->health != POWER_SUPPLY_HEALTH_WARM)) {
		sm->batt_sw_fc = false;
		prop.intval = true;
		vote(sm->chg_dis_votable, BMS_FC_VOTER, false, 0);
		//rc = power_supply_get_property(sm->batt_psy, POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
		if (rc < 0) {
			pr_err("sm could not set force recharging!\n");
			return rc;
		}
	}

	return 0;
}

static void fg_monitor_workfunc(struct work_struct* work)
{
	struct sh_fg_chip *sm = container_of(work, struct sh_fg_chip, monitor_work.work);
	s32 interval;

	mutex_lock(&sm->data_lock);
	fg_init(sm->client);
	mutex_unlock(&sm->data_lock);

	fg_refresh_status(sm);
	interval = fg_check_full_status(sm);
	fg_check_recharge_status(sm);

	if (interval > 0) {
		schedule_delayed_work(&sm->monitor_work, interval * HZ);
	}
}

static s32 fg_get_device_id(struct i2c_client* client)
{
	struct sh_fg_chip* sm = i2c_get_clientdata(client);
	s32 ret;
	u16 data;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_DEVICE_ID], &data);
	if (ret < 0) {
		pr_err("Failed to read DEVICE_ID, ret = %d\n", ret);
		return ret;
	}

	pr_debug("device_id = 0x%04X\n", data);
	return ret;
}

static bool fg_init(struct i2c_client* client)
{
	s32 ret;

	/*sh366101 i2c read check*/
	ret = fg_get_device_id(client);
	if (ret < 0) {
		pr_err("%s: fail to do i2c read(%d)\n", __func__, ret);
		return false;
	}

	return true;
}

static s32 fg_common_parse_dt(struct sh_fg_chip* sm)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;

	BUG_ON(dev == 0);
	BUG_ON(np == 0);

	sm->gpio_int = of_get_named_gpio(np, "qcom,irq-gpio", 0);
	pr_debug("gpio_int=%d\n", sm->gpio_int);

	if (!gpio_is_valid(sm->gpio_int)) {
		pr_info("gpio_int is not valid\n");
		sm->gpio_int = -EINVAL;
	}

	/* EN TEMP EX/IN */
	if (of_property_read_bool(np, "sm,en_temp_ex"))
		sm->en_temp_ex = true;
	else
		sm->en_temp_ex = 0;
	pr_debug("Temperature EX enabled = %d\n", sm->en_temp_ex);

	if (of_property_read_bool(np, "sm,en_temp_in"))
		sm->en_temp_in = true;
	else
		sm->en_temp_in = 0;
	pr_debug("Temperature IN enabled = %d\n", sm->en_temp_in);

	/* EN BATT DET  */
	if (of_property_read_bool(np, "sm,en_batt_det"))
		sm->en_batt_det = true;
	else
		sm->en_batt_det = 0;
	pr_debug("Batt Det enabled = %d\n", sm->en_batt_det);

	/* Shutdown feature */
	if (of_property_read_bool(np,"sm,shutdown-delay-enable"))
		sm->shutdown_delay_enable = true;
	else
		sm->shutdown_delay_enable = false;

	return 0;
}

static s32 get_battery_id(struct sh_fg_chip* sm)
{
	return 0;
}

static s32 fg_gauge_unseal(struct sh_fg_chip* sm) /* 20211122, Ethan. Gauge Enable */
{
	s32 ret;

	ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_UNSEALKEY);
	if (ret < 0)
		goto fg_gauge_unseal_End;
	HOST_DELAY(CMD_SBS_DELAY);

	ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)(CMD_UNSEALKEY >> 16));
	if (ret < 0)
		goto fg_gauge_unseal_End;
	HOST_DELAY(CMD_SBS_DELAY);

	ret = 0;
fg_gauge_unseal_End:
	return ret;
}

static s32 fg_gauge_seal(struct sh_fg_chip* sm) /* 20211122, Ethan. Gauge Enable */
{
	return fg_write_sbs_word(sm, CMD_ALTMAC, CMD_SEAL);
}

static s32 fg_gauge_runstate_check(struct sh_fg_chip* sm) /* 20211126, Ethan */
{
	s32 ret;
	u16 oemflag;
	s32 retry_cnt;
	u32 socFlag = 0, qenFlag = 0, ltFlag = 0;

	/* 20211208, Ethan. In case por with poor connection */
	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
		ret = fg_read_soc(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_volt(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_current(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		//20220106, Ethan
		ret = fg_read_temperature(sm,  TEMPERATURE_EX);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;

		ret = fg_read_terminate_voltage(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &oemflag);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;

		//20220106, Ethan
		socFlag = !!(sm->batt_temp < TEMPER_MIN_RESET);
		if (sm->batt_curr <= 0) {
			socFlag |= !!((sm->batt_volt > VOLT_MIN_RESET) && ((sm->batt_soc/100) < SOC_MIN_RESET));
			socFlag |= !!((sm->batt_soc == 0) && ((sm->batt_volt - sm->terminate_voltage) > DELTA_VOLT));
		}
		if (socFlag) {
			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;

			ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_RESET);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;
			HOST_DELAY(DELAY_RESET);
		}

		 /* por with poor connection */
		qenFlag = !!((oemflag & CMD_MASK_OEM_GAUGEEN) != CMD_MASK_OEM_GAUGEEN); /* Gauge Un-enable */
		if (qenFlag) {  //Gauge Disable. Re-enable gauge
			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;

			ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_ENABLE_GAUGE);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;
			HOST_DELAY(DELAY_ENABLE_GAUGE);
		}

		ltFlag = !!((oemflag & CMD_MASK_OEM_LIFETIMEEN) != CMD_MASK_OEM_LIFETIMEEN);
		if (ltFlag) { //Lifetime Disable. Re-enable lifetime
			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;

		ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_ENABLE_LIFETIME);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(DELAY_ENABLE_GAUGE);
	}

		ret = 0;
		break;

fg_gauge_runstate_check_End:
		HOST_DELAY(CMD_SBS_DELAY << 1);
	}

	pr_err("fg_gauge_runstate_check: soc=%d, volt=%d, termVolt=%d, OEMFlag=0x%04X, QEN_FLAG=%u, SOC_FLAG=%u, LifeTime_Flag=%u",
		sm->batt_soc, sm->batt_volt, sm->terminate_voltage, oemflag, qenFlag, socFlag, ltFlag);

	fg_gauge_seal(sm);
	return ret;
}

static s32 Check_Chip_Version(struct sh_fg_chip* sm)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;
	s32 ret = CHECK_VERSION_ERR;
	u16 temp;
	static int batid_count = 0;
	/* 20211025, Ethan. IAP Fail Check */
	struct sh_decoder decoder;
	u8 iap_read[IAP_READ_LEN];

	/* 20211210, Ethan */
	sm->dtsi_version_fw = -1;
	sm->dtsi_version_fwdate = -1;
	sm->dtsi_version_ts = -1;
	sm->dtsi_version_afi = -1;
	sm->chip_version_fw = -1;
	sm->chip_version_fwdate = -1;
	sm->chip_version_ts = -1;
	sm->chip_version_afi = -1;
	batid_count++;

	/* battery_params node*/
	ret = fg_get_prop_batt_id(sm);
	if (ret < 0) {
		if (batid_count < 100)
			return -EPROBE_DEFER;
	}
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("Check_Chip_Version: Cannot find child node \"battery_params\"\n");
		return CHECK_VERSION_ERR;
	}

	of_property_read_u32(np, "version_main", &sm->dtsi_version_fw); /* 20211211, Ethan. */
	of_property_read_u32(np, "version_date", &sm->dtsi_version_fwdate); /* 20211211, Ethan.*/
	switch(sm->batt_id){
	case 1:
		of_property_read_u32(np, "version_afi_01", &sm->dtsi_version_afi);
		break;
	case 2:
		of_property_read_u32(np, "version_afi_02", &sm->dtsi_version_afi);
		break;
	case 3:
		of_property_read_u32(np, "version_afi_03", &sm->dtsi_version_afi);
		break;
	case 4:
		of_property_read_u32(np, "version_afi_04", &sm->dtsi_version_afi);
		break;
	default:
		of_property_read_u32(np, "version_afi_01", &sm->dtsi_version_afi);
                break;
	}
	of_property_read_u32(np, "version_ts", &sm->dtsi_version_ts); /* 20211211, Ethan. */
	of_property_read_u8(np, "iap_twiadr", &decoder.addr); /* 20211025, Ethan */

	pr_err("Check_Chip_Version: main=0x%04X, date=0x%08X, afi=0x%04X, ts=0x%04X", sm->dtsi_version_fw, sm->dtsi_version_fwdate, sm->dtsi_version_afi, sm->dtsi_version_ts);

	/* 20211025, Ethan. IAP Fail Check. iap addr may differ from normal addr */
	decoder.reg = (u8)CMD_IAPSTATE_CHECK;
	decoder.length = IAP_READ_LEN;
	if ((fg_decode_iic_read(sm, &decoder, iap_read) >= 0) && (iap_read[0] != 0) && (iap_read[1] != 0)) {
		pr_err("Check_Chip_Version: ic is in iap mode, force update all");
		ret = CHECK_VERSION_WHOLE_CHIP;
		goto Check_Chip_Version_End;
	}
	HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */

	if (fg_gauge_unseal(sm) < 0) { /* 20211122, Ethan. Gauge Enable */
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}

	/* check fw version. FW update must update afi(for iap check flag) */
	if (fg_read_sbs_word(sm, CMD_FWVERSION_MAIN, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}

	/* 20211211, Ethan */
	sm->chip_version_fw = temp;
	pr_err(" Chip_Version: ic main=0x%04X ", sm->chip_version_fw);

	if (fg_read_sbs_word(sm, CMD_FWDATE1, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	msleep(CMD_SBS_DELAY);
	sm->chip_version_fwdate = (u32)temp << 16;

	if (fg_read_sbs_word(sm, CMD_FWDATE2, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	sm->chip_version_fwdate |= (temp & FW_DATE_MASK);
	pr_err(" Chip_Version: ic date=0x%08X ", sm->chip_version_fwdate);

	if (sm->chip_version_fw < sm->dtsi_version_fw) {
		ret = CHECK_VERSION_WHOLE_CHIP;
	} else if (sm->chip_version_fw > sm->dtsi_version_fw) {
		ret = CHECK_VERSION_OK;
	} else if (sm->chip_version_fwdate < sm->dtsi_version_fwdate) {
		ret = CHECK_VERSION_WHOLE_CHIP;
	} else {
		ret = CHECK_VERSION_OK;
	}

	/* check afi */
	if (fg_read_sbs_word(sm, CMD_AFI_STATIC_SUM, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	/* 20211211, Ethan */
	sm->chip_version_afi = temp;
	pr_err(" Chip_Version: ic afi=0x%04X ", sm->chip_version_afi);
	if (sm->chip_version_afi != sm->dtsi_version_afi)
		ret |= CHECK_VERSION_AFI;

	/* check TS */
	if (fg_read_sbs_word(sm, CMD_TS_VER, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}

	/* 20211211, Ethan */
	sm->chip_version_ts = temp;
	pr_err(" Chip_Version: ic ts=0x%04X ", sm->chip_version_ts);
	if (sm->chip_version_ts != sm->dtsi_version_ts)
		ret |= CHECK_VERSION_TS;

Check_Chip_Version_End:
	fg_gauge_seal(sm); /* 20211122, Ethan. Gauge Enable */
	return ret;
}

int file_decode_process(struct sh_fg_chip* sm, char* profile_name)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;
	u8* pBuf = NULL;
	u8* pBuf_Read = NULL;
	char strDebug[FILEDECODE_STRLEN];
	int buflen;
	int wait_ms;
	int i, j;
	int line_length;
	int result = -1;
	int retry;

	pr_err("file_decode_process: start");

	/* battery_params node*/
        np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("file_decode_process: Cannot find child node \"battery_params\"");
		return -EINVAL;
	}

	buflen = of_property_count_u8_elems(np, profile_name);
	pr_debug("file_decode_process: ele_len=%d, key=%s", buflen, profile_name);

	pBuf = (u8*)devm_kzalloc(dev, buflen, 0);
	pBuf_Read = (u8*)devm_kzalloc(dev, BUF_MAX_LENGTH, 0);

	if ((pBuf == NULL) || (pBuf_Read == NULL)) {
		result = ERRORTYPE_ALLOC;
		pr_err("file_decode_process: kzalloc error");
		goto main_process_error;
	}

	result = of_property_read_u8_array(np, profile_name, pBuf, buflen);
	if (result) {
		pr_err("file_decode_process: read dts fail %s\n", profile_name);
		goto main_process_error;
	}
	print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf, 32);
	pr_err("file_decode_process: first data=%s", strDebug);

	i = 0;
	j = 0;
	while (i < buflen) {
		/* delay: b0: operate, b1: 2, b2-b3: time, big-endian */
		/* other: b0: operate, b1: TWIADR, b2: reg, b3: data_length, b4...end: item */
		if (pBuf[i + INDEX_TYPE] == OPERATE_WAIT) {
			wait_ms = ((int)pBuf[i + INDEX_WAIT_HIGH] * 256) + pBuf[i + INDEX_WAIT_LOW];

			if (pBuf[i + INDEX_WAIT_LENGTH] == 2) {
				HOST_DELAY(wait_ms); /* 20211029, Ethan */
				i += LINELEN_WAIT;
			} else {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process wait error! index=%d, str=%s", i, strDebug);
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_READ) {
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			/* 20211026, Ethan. IAP addr may differ from default addr */
			/*if (fg_read_block(sm, pBuf[i + INDEX_REG], line_length, pBuf_Read) < 0) { */
			if (fg_decode_iic_read(sm, (struct sh_decoder*)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0) {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process read error! index=%d, str=%s", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_READ;
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_COMPARE) {
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			for (retry = 0; retry < COMPARE_RETRY_CNT; retry++) {
				/* 20211026, Ethan. IAP addr may differ from default addr */
				/*if (fg_read_block(sm, pBuf[i + INDEX_REG], line_length, pBuf_Read) < 0) { */
				if (fg_decode_iic_read(sm, (struct sh_decoder*)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0) {
					print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
					pr_err("file_decode_process compare_read error! index=%d, str=%s", i, strDebug);
					result = ERRORTYPE_COMM;
					goto file_decode_process_compare_loop_end;
				}

				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, line_length);
				//pr_debug("file_decode_process loop compare: IC read=%s", strDebug);

				result = 0;
				for (j = 0; j < line_length; j++) {
					if (pBuf[INDEX_DATA + i + j] != pBuf_Read[j]) {
						result = ERRORTYPE_COMPARE;
						break;
					}
				}

				if (result == 0)
					break;

				/* compare fail */
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process compare error! index=%d, retry=%d, host=%s", i, retry, strDebug);
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, 32);
				pr_err("ic=%s", strDebug);

			file_decode_process_compare_loop_end:
				HOST_DELAY(COMPARE_RETRY_WAIT); /* 20211029, Ethan */
			}

			if (retry >= COMPARE_RETRY_CNT) {
				result = ERRORTYPE_COMPARE; /* 20211125, Ethan */
				goto main_process_error;
			}

			i += LINELEN_COMPARE + line_length;
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_WRITE) {
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			/* 20211026, Ethan. IAP addr may differ from default addr */
			/* if (fg_write_block(sm, pBuf[i + INDEX_REG], line_length, &pBuf[i + INDEX_DATA]) != 0) { */
			if (fg_decode_iic_write(sm, (struct sh_decoder*)&pBuf[i + INDEX_ADDR]) != 0) {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process write error! index=%d, str=%s", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_WRITE + line_length;
		} else {
			result = ERRORTYPE_LINE;
			goto main_process_error;
		}
	}
	result = ERRORTYPE_NONE;

main_process_error:
	pr_err("file_decode_process end: result=%d", result);
	return result;
}

static s32 fg_gauge_calibrate_board(struct sh_fg_chip* sm) /* 20211228, Ethan */
{
#define CALI_ERR_NONE 0
#define CALI_ERR_COMM -1
#define CALI_ERR_LARGE_CURR -2
#define CALI_ERR_CALIMODE -3
#define CAIL_ERR_USER_CORRUPT -4

#define WAIT_CALI_BOARD 1250
#define CURRENT_NO_LOAD 100 * MA_TO_UA /* 20220101, Ethan */

#define LEN_ENTERCALI 6
#define CMD_ENTERCALI (CMDMASK_ALTMAC_W | 0xE014)
	u8 BUF_ENTERCALI[LEN_ENTERCALI] = { 0x45, 0x54, 0x43, 0x41, 0x4C, 0x49 };

#define LEN_EXITCALI 6
#define CMD_EXITCALI (CMDMASK_ALTMAC_W | 0xE015)
	u8 BUF_EXITCALI[LEN_EXITCALI] = { 0x65, 0x78, 0x63, 0x61, 0x6C, 0x69 };

#define LEN_CALIBOARD 9
#define CMD_CALIBOARD (CMDMASK_ALTMAC_W | 0xE016)
	u8 BUF_CALIBOARD[LEN_CALIBOARD] = { 0x43, 0x61, 0x4C, 0x69, 0x42, 0x6F, 0x61, 0x72, 0x64 };

#define FLAG_CALIED_FLAG 0x0055
#define LEN_CALIED_FLAG 2
#define CMD_CALIED_FLAG (CMDMASK_ALTMAC_W | 0x4042)
#define CMD_BOARDOFFSET (CMDMASK_ALTMAC_W | 0x4044)
#define VAL_MAX_BOARDOFFSET (2000)
	u8 BUF_CALIED_FLAG[32];

	s32 ret;
	u16 oemflag = 0;
	u32 oemflag_cali = 0;
	s32 retry_cnt;
	s16 boardoffset = 0;
	u16 cali_flag = 0;

	ret = fg_read_current(sm);
	if (ret < 0) {
		pr_err("fg_gauge_calibrate_board Comm error! cannot read 1st current!"); //20220104, Ethan
		ret = CALI_ERR_COMM;
		goto fg_gauge_calibrate_board_end;
	}
	if (abs(sm->batt_curr) >= CURRENT_NO_LOAD) {
		pr_err("fg_gauge_calibrate_board current error! current too large!"); //20220104, Ethan
		ret = CALI_ERR_LARGE_CURR;
		goto fg_gauge_calibrate_board_end;
	}

	ret = fg_gauge_unseal(sm);
	if (ret < 0) {
		pr_err("fg_gauge_calibrate_board Comm error! cannot 1st unseal!"); //20220104, Ethan
		ret = CALI_ERR_COMM;
		goto fg_gauge_calibrate_board_exitcali; /* retry may result ic in cali */
	}
	HOST_DELAY(CMD_SBS_DELAY);

	ret = fg_read_block(sm, CMD_BOARDOFFSET, 32, BUF_CALIED_FLAG);
	if (ret < 0) {
		pr_err("fg_gauge_calibrate_board Comm error! cannot read 1st cali flag!"); //20220104, Ethan
		ret = CALI_ERR_COMM;
		goto fg_gauge_calibrate_board_exitcali; /* retry may result ic in cali */
	}
	boardoffset = (s16)BUF2U16_BG(BUF_CALIED_FLAG);

	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
		oemflag_cali = 0;
		oemflag = 0;
		HOST_DELAY(CMD_SBS_DELAY << 2);

		ret = fg_gauge_unseal(sm);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot 2nd unseal!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_exitcali; /* retry may result ic in cali */
		}
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_block(sm, CMD_CALIED_FLAG, 32, BUF_CALIED_FLAG);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot read 2nd cali flag!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_exitcali; /* retry may result ic in cali */
		}
		cali_flag = BUF2U16_BG(BUF_CALIED_FLAG);
		if (cali_flag == FLAG_CALIED_FLAG) {
			ret = CALI_ERR_NONE;
			pr_err("fg_gauge_calibrate_board cali_flag ok!");
			goto fg_gauge_calibrate_board_exitcali; /* retry may result ic in cali */
		}
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_write_block(sm, CMD_ENTERCALI, LEN_ENTERCALI, BUF_ENTERCALI);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot enter cali mode!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_exitcali;
		}
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &oemflag);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot read oem flag!"); //20220104, Ethan
			ret = CALI_ERR_COMM; //20220104, Ethan
			goto fg_gauge_calibrate_board_exitcali;
		}
		oemflag_cali = !!((oemflag & CMD_MASK_OEM_CALI) == CMD_MASK_OEM_CALI);
		if (!oemflag_cali) //enter cali fail
			continue;

		ret = fg_write_block(sm, CMD_CALIBOARD, LEN_CALIBOARD, BUF_CALIBOARD);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot cali board!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_exitcali;
		}
		HOST_DELAY(WAIT_CALI_BOARD);

		ret = fg_read_current(sm);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot read 2nd current!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_exitcali;
		}
		if (sm->batt_curr != 0) {
			pr_err("fg_gauge_calibrate_board User Corrupt! calied current too large! %d", sm->batt_curr); //20220104, Ethan
			ret = CAIL_ERR_USER_CORRUPT;
			continue;
		}

		ret = fg_read_block(sm, CMD_BOARDOFFSET, 32, BUF_CALIED_FLAG);
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot read board2!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_exitcali; /* retry may result ic in cali */
		}
		ret = (s32)((s16)BUF2U16_BG(BUF_CALIED_FLAG));
		if (abs(ret) < VAL_MAX_BOARDOFFSET) {
			ret = CALI_ERR_NONE;
			break;
		} else  {
			pr_err("fg_gauge_calibrate_board User Corrupt! calied board too large! %d", ret);
			ret = CAIL_ERR_USER_CORRUPT;
		}
	}
	if (!oemflag_cali)
		ret = CALI_ERR_CALIMODE;
	else {
fg_gauge_calibrate_board_exitcali:
		if (ret == CALI_ERR_NONE) { //cali ok
			BUF_CALIED_FLAG[0] = (u8)(FLAG_CALIED_FLAG >> 8);
			BUF_CALIED_FLAG[1] = (u8)(FLAG_CALIED_FLAG);
			fg_write_block(sm, CMD_CALIED_FLAG, LEN_CALIED_FLAG, BUF_CALIED_FLAG);
			HOST_DELAY(CMD_SBS_DELAY);
		} else { //cali fail. restore boardoffset
			BUF_CALIED_FLAG[0] = (u8)(boardoffset >> 8);
			BUF_CALIED_FLAG[1] = (u8)(boardoffset);
			fg_write_block(sm, CMD_BOARDOFFSET, LEN_CALIED_FLAG, BUF_CALIED_FLAG);
			HOST_DELAY(CMD_SBS_DELAY);
		}

		ret |= fg_write_block(sm, CMD_EXITCALI, LEN_EXITCALI, BUF_EXITCALI); /* exit cali and force update e2 */
		if (ret < 0) {
			pr_err("fg_gauge_calibrate_board Comm error! cannot exit cali!"); //20220104, Ethan
			ret = CALI_ERR_COMM;
			goto fg_gauge_calibrate_board_end;
		}
		HOST_DELAY(DELAY_WRITE_E2ROM);

		ret = CALI_ERR_NONE;
	}


fg_gauge_calibrate_board_end:
	fg_gauge_seal(sm);

	pr_err("fg_gauge_calibrate_board: ret=%d, curr=%d, OEMFlag=0x%04X, OEMCali=%u, CaliFlag=%04X",
		ret, sm->batt_curr, oemflag, oemflag_cali, cali_flag);

	return ret;
}

void fg_run_calibration(struct power_supply* psy)
{
	struct sh_fg_chip* sm = power_supply_get_drvdata(psy);
	int i;
	int ret;

	for (i = 0; i < 5; i++) {
		ret = -1;
		if (mutex_trylock(&sm->cali_lock)) {
			ret = fg_gauge_calibrate_board(sm);
			mutex_unlock(&sm->cali_lock);
		} else {
			pr_err("fg_run_calibration: could not get mutex cali_lock!");
		}

		if (ret >= 0)
			break;
		HOST_DELAY(200);
	}
}
EXPORT_SYMBOL(fg_run_calibration);

static s32 fg_battery_parse_dt(struct sh_fg_chip* sm)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;
	s32 battery_id = -1;

	BUG_ON(dev == 0);
	BUG_ON(np == 0);

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_info("Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
		pr_err("not battery,id property\n");
	if (battery_id == -1)
		battery_id = get_battery_id(sm);
	pr_debug("battery id = %d\n", battery_id);

	return 0;
}

bool hal_fg_init(struct i2c_client* client)
{
	struct sh_fg_chip* sm = i2c_get_clientdata(client);

	pr_info("sh366101 hal_fg_init...\n");
	mutex_lock(&sm->data_lock);
	if (client->dev.of_node) {
		/* Load common data from DTS*/
		fg_common_parse_dt(sm);
		/* Load battery data from DTS*/
		fg_battery_parse_dt(sm);
	}

	sm->log_lastUpdate = jiffies; /* 20211025, Ethan */

	if (!fg_init(client))
		return false;

	mutex_unlock(&sm->data_lock);
	pr_info("hal fg init OK\n");
	return true;
}

static s32 sh366101_get_psy(struct sh_fg_chip* sm)
{
#if !(IS_PACK_ONLY)
	if (!sm->usb_psy || !sm->batt_psy)
		return -EINVAL;

	sm->usb_psy = power_supply_get_by_name("usb");
	if (!sm->usb_psy) {
		pr_err("USB supply not found, defer probe\n");
		return -EINVAL;
	}

	sm->batt_psy = power_supply_get_by_name("battery");
	if (!sm->batt_psy) {
		pr_err("bms supply not found, defer probe\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static s32 sh366101_notifier_call(struct notifier_block* nb, unsigned long ev, void* v)
{
#if !(IS_PACK_ONLY)
	struct power_supply* psy = v;
	struct sh_fg_chip* sm = container_of(nb, struct sh_fg_chip, nb);
	union power_supply_propval pval = {
	    0,
	};
	s32 rc;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	rc = sh366101_get_psy(sm);
	if (rc < 0) {
		return NOTIFY_OK;
	}

	if (strcmp(psy->desc->name, "usb") != 0)
		return NOTIFY_OK;

	if (sm->usb_psy) {
		rc = power_supply_get_property(sm->usb_psy, POWER_SUPPLY_PROP_PRESENT, &pval);

		if (rc < 0) {
			pr_err("failed get usb present\n");
			return -EINVAL;
		}
		if (pval.intval) {
			sm->usb_present = true;
			//pm_stay_awake(sm->dev);
		} else {
			sm->batt_sw_fc = false;
			sm->usb_present = false;
			//pm_relax(sm->dev);
		}
	}
#endif
	return NOTIFY_OK;
}

static int sh366101_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sh_fg_chip *sm = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_SHUTDOWN_DELAY:
		*val1 = sm->shutdown_delay;
		break;
	case PSY_IIO_RESISTANCE:
		*val1 = 0;
		break;
	case PSY_IIO_RESISTANCE_ID:
		if(sm->batt_id == 1)
			*val1 = 330000;
		else if (sm->batt_id == 2)
			*val1 = 220000;
		else
			*val1 = 0;
		break;
	/*case PSY_IIO_SOC_DECIMAL:
		*val1 = fg_get_soc_decimal(sm);
		break;
	case PSY_IIO_SOC_DECIMAL_RATE:
		*val1 = fg_get_soc_decimal_rate(sm);
		break;
	case PSY_IIO_FASTCHARGE_MODE:
		*val1 = sm->fast_mode;
		break;
	*/
	case PSY_IIO_RAW_SOC:
		*val1 = sm->param.batt_raw_soc;
		break;
	case PSY_IIO_BATTERY_TYPE:
	/*
		switch (get_battery_id(sm)) {
			case BATTERY_VENDOR_NVT:
				*val1 = 0;		//"M376-NVT-5000mAh";
				break;
			case BATTERY_VENDOR_GY:
				*val1 = 1;		//"M376-GuanYu-5000mAh";
				break;
			case BATTERY_VENDOR_XWD:
				*val1 = 2;		//"M376-Sunwoda-5000mAh";
				break;
			default:
				*val1 = 3;		//"M376-unknown-5000mAh";
				break;
		}
	*/
		break;

	case PSY_IIO_SOH:
		*val1 = 100;
		break;
	case PSY_IIO_BATT_ID:
		*val1 = 0;
		break;
	case PSY_IIO_VERSION:
		*val1 = sm->check_fw_version;
	default:
		pr_info("Unsupported sh366101 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int sh366101_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	//struct sh_fg_chip *sm = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_FG_MONITOR_WORK:
		if(!!val1) {
			pr_info("start_fg_monitor_work. \n");
			//start_fg_monitor_work(sm->fg_psy);
		} else {
			pr_info("stop_fg_monitor_work. \n");
			//stop_fg_monitor_work(sm->fg_psy);
		}
		break;

	default:
		pr_err("Unsupported SH366101 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
	return rc;
}

static int sh366101_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sh_fg_chip *sm = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = sm->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(sh366101_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info sh366101_iio_info = {
	.read_raw	= sh366101_iio_read_raw,
	.write_raw	= sh366101_iio_write_raw,
	.of_xlate	= sh366101_iio_of_xlate,
};

static int sh366101_init_iio_psy(struct sh_fg_chip *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(sh366101_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("sh366101_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);

	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &sh366101_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "sh366101";

	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = sh366101_iio_psy_channels[i].channel_num;
		chan->type = sh366101_iio_psy_channels[i].type;
		chan->datasheet_name =
			sh366101_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			sh366101_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			sh366101_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);

	if (rc)
		pr_err("Failed to register sh366101 IIO device, rc=%d\n", rc);

	pr_info("battery IIO device, rc=%d\n", rc);

	return rc;
}


static int sh366101_ext_init_iio_psy(struct sh_fg_chip *sm)
{
	if (!sm)
		return -ENOMEM;

	pr_err("ds_iio_init start\n");
	/*sm->ds_iio = devm_kcalloc(sm->dev,
		ARRAY_SIZE(ds_iio_chan_name), sizeof(*sm->ds_iio), GFP_KERNEL);
	if (!sm->ds_iio)
		return -ENOMEM;
	*/
	sm->nopmi_chg_iio = devm_kcalloc(sm->dev,
		ARRAY_SIZE(nopmi_chg_iio_chan_name), sizeof(*sm->nopmi_chg_iio), GFP_KERNEL);
	if (!sm->nopmi_chg_iio)
		return -ENOMEM;
	/*
	sm->cp_iio = devm_kcalloc(sm->dev,
		ARRAY_SIZE(cp_iio_chan_name), sizeof(*sm->cp_iio), GFP_KERNEL);
	if (!sm->cp_iio)
		return -ENOMEM;
	*/
	sm->main_iio = devm_kcalloc(sm->dev,
		ARRAY_SIZE(main_iio_chan_name), sizeof(*sm->main_iio), GFP_KERNEL);
	if (!sm->main_iio)
		return -ENOMEM;
	pr_err("ds_iio_init end\n");

	return 0;
}

static s32 sh_fg_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	s32 ret;
	s32 version_ret;
	s32 retry;
	struct sh_fg_chip* sm;
	struct iio_dev *indio_dev = NULL;
	static probe_cnt = 0;
	u32* regs;

	if (probe_cnt == 0) {
		pr_err("%s enter !\n",__func__);
	}
	probe_cnt ++;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*sm));

	if (!indio_dev){
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	sm = iio_priv(indio_dev);
	sm->indio_dev = indio_dev;
	sm->dev = &client->dev;
	sm->client = client;
	sm->chip = id->driver_data;

	sm->batt_soc = -ENODATA;
	sm->batt_fcc = -ENODATA;
	sm->batt_volt = -ENODATA;
	sm->batt_temp = -ENODATA;
	sm->batt_curr = -ENODATA;
	/* 20211029, Ethan. */
	/* sm->fake_soc = -EINVAL; */
	sm->fake_temp = -EINVAL;
	sm->fake_cycle_count = -EINVAL;

	if (sm->chip == SH366101) {
		regs = sh366101_regs;
	} else {
		pr_err("unexpected fuel gauge: %d\n", sm->chip);
		regs = sh366101_regs;
	}

	memcpy(sm->regs, regs, NUM_REGS * sizeof(u32));

	i2c_set_clientdata(client, sm);

	mutex_init(&sm->i2c_rw_lock);
	mutex_init(&sm->data_lock);
	mutex_init(&sm->cali_lock); /* 20220105, Ethan */

	fg_read_gaugeinfo_block(sm); /* 20211115. LJQ Debug */

	/* 20211013, Ethan. Firmware Update */
	version_ret = Check_Chip_Version(sm);
	sm->fw_update_config = version_ret; /* 20211211, Ethan */
	sm->fw_update_error_state = 0; /* 20211211, Ethan */
	if (version_ret == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	}
	if (version_ret == CHECK_VERSION_ERR) {
		sm->check_fw_version = 1 ;
		pr_err("Probe: Check version error!");
	} else if (version_ret == CHECK_VERSION_OK) {
		sm->check_fw_version = 0 ;
		pr_err("Probe: Check version ok!");
	} else {
		pr_err("Probe: Check version update: %X", version_ret);

		sm->check_fw_version = 0 ;
		if (version_ret & CHECK_VERSION_FW) {
			pr_err("Probe: Firmware Update start");
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				ret = file_decode_process(sm, "sinofs_image_data");
				if (ret == ERRORTYPE_NONE)
					break;
				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
			}
			pr_err("Probe: Firmware Update end, ret=%d", ret);
			if (ret == ERRORTYPE_NONE) { /* 20211211, Ethan */
				sm->fw_update_error_state |= CHECK_VERSION_FW;
				sm->check_fw_version = 1;
			}
		}

		if (version_ret & CHECK_VERSION_TS) {
			pr_err("Probe: TS Update start");
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				ret = file_decode_process(sm, "sinofs_ts_data");
				if (ret == ERRORTYPE_NONE)
					break;
				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
			}
			pr_err("Probe: TS Update end, ret=%d", ret);
			if (ret == ERRORTYPE_NONE) { /* 20211211, Ethan */
				sm->fw_update_error_state |= CHECK_VERSION_TS;
				sm->check_fw_version = 1;
			}
		}

		if (version_ret & CHECK_VERSION_AFI) {
			pr_err("Probe: AFI Update start");
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				switch(sm->batt_id){
					case 1:
					      ret = file_decode_process(sm, "sinofs_afi01_data");
				       	      break;
					case 2:
					      ret = file_decode_process(sm, "sinofs_afi02_data");
				       	      break;
					case 3:
					      ret = file_decode_process(sm, "sinofs_afi03_data");
				       	      break;
					case 4:
					      ret = file_decode_process(sm, "sinofs_afi04_data");
				       	      break;
					default:
					      ret = file_decode_process(sm, "sinofs_afi01_data");
				       	      break;
				}
				if (ret == ERRORTYPE_NONE)
					break;
				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
			}
			pr_err("Probe: AFI Update end, ret=%d", ret);
			if (ret == ERRORTYPE_NONE) { /* 20211211, Ethan */
				sm->fw_update_error_state |= CHECK_VERSION_AFI;
				sm->check_fw_version = 1;
			}
		}
		if(sm->fw_update_error_state){
			sm->check_fw_version = 1;
		}else{
			sm->check_fw_version = 0;
		}
	}

	if (fg_gauge_runstate_check(sm) < 0) { /* 20211122, Ethan. Gauge Enable */
		pr_err("Failed to Enable Gauge\n");
		goto err_0;
	}

	if (!hal_fg_init(client)) {
		pr_err("Failed to Initialize Fuelgauge\n");
		goto err_0;
	}

	INIT_DELAYED_WORK(&sm->monitor_work, fg_monitor_workfunc);
	INIT_DELAYED_WORK(&sm->soc_monitor_work, soc_monitor_work);
	fg_psy_register(sm);

#if FG_REMOVE_IRQ == 0
	if (sm->gpio_int != -EINVAL)
		pr_err("unuse\n");
	else {
		pr_err("Failed to registe gpio interrupt\n");
		goto err_0;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
						fg_irq_thread,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						"sh fuel gauge irq", sm);
		if (ret < 0) {
			pr_err("request irq for irq=%d failed, ret = %d\n", client->irq, ret);
		}
	}
#endif

	sm->nb.notifier_call = &sh366101_notifier_call;
	ret = power_supply_reg_notifier(&sm->nb);
	if (ret < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", ret);
		return ret;
	}

	ret = sh366101_ext_init_iio_psy(sm);
	if (ret < 0) {
		pr_err("Failed to initialize sh366101 ext IIO PSY, ret=%d\n", ret);
	}
	ret = sh366101_init_iio_psy(sm);
	if (ret < 0) {
		pr_err("Failed to initialize sh366101 IIO PSY, ret=%d\n", ret);
	}

	ret = sysfs_create_group(&sm->dev->kobj, &fg_attr_group);
	if (ret)
		pr_err("Failed to register sysfs:%d\n", ret);

	schedule_delayed_work(&sm->monitor_work, 10 * HZ);
	sm->param.batt_soc = -EINVAL;
	schedule_delayed_work(&sm->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_MS));

	pr_debug("sh fuel gauge probe successfully, %s\n", device2str[sm->chip]);

	ret = enable_irq_wake(client->irq);
	pr_err("sh_fg_probe:enable_irq_wake. end. ret=%d.\n", ret );

	return 0;

err_0:
	return ret;
}

static s32 sh_fg_remove(struct i2c_client* client)
{
	struct sh_fg_chip* sm = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&sm->monitor_work);

	cancel_delayed_work_sync(&sm->soc_monitor_work);

	fg_psy_unregister(sm);

	mutex_destroy(&sm->cali_lock); /* 20220105, Ethan */
	mutex_destroy(&sm->data_lock);
	mutex_destroy(&sm->i2c_rw_lock);

	debugfs_remove_recursive(sm->debug_root);

	sysfs_remove_group(&sm->dev->kobj, &fg_attr_group);

	return 0;
}

static void sh_fg_shutdown(struct i2c_client* client)
{
	pr_info("sm fuel gauge driver shutdown!\n");
}

static const struct of_device_id sh_fg_match_table[] = {
    {
	.compatible = "sh,sh366101",
    },
    {},
};
MODULE_DEVICE_TABLE(of, sh_fg_match_table);

static const struct i2c_device_id sh_fg_id[] = {
    {"sh366101", SH366101},
    {},
};
MODULE_DEVICE_TABLE(i2c, sh_fg_id);

static struct i2c_driver sh_fg_driver = {

    .driver = {
	.name = "sh366101",
	.owner = THIS_MODULE,
	.of_match_table = sh_fg_match_table,
    },
    .id_table = sh_fg_id,
    .probe = sh_fg_probe,
    .remove = sh_fg_remove,
    .shutdown = sh_fg_shutdown,
};

module_i2c_driver(sh_fg_driver);

MODULE_DESCRIPTION("SH SH366101 Gauge Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sinowealth");

