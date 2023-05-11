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
#include <sh366101_fg.h>
#include <linux/pmic-voter.h>

#undef FG_DEBUG
#define FG_DEBUG 1

#undef pr_debug
#undef pr_info
#if FG_DEBUG
#define pr_debug pr_err
#define pr_info pr_err
#endif

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
	SH_FG_REG_SHUTDOWN_EN,
	SH_FG_REG_SHUTDOWN,
	SH_FG_REG_BAT_RMC,
	SH_FG_REG_BAT_FCC,
	SH_FG_REG_RESET,
	SH_FG_REG_SOC_CYCLE,
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

struct sh_fg_chip;

struct sh_fg_chip {
	struct device* dev;
	struct i2c_client* client;
	struct mutex i2c_rw_lock; /* I2C Read/Write Lock */
	struct mutex data_lock;	  /* Data Lock */
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
	s32 batt_volt;
	s32 aver_batt_volt;
	s32 batt_temp;
	s32 batt_curr;
	s32 is_charging;    /* Charging informaion from charger IC */
	s32 batt_soc_cycle; /* Battery SOC cycle */
        s32 charge_status;
	s32 health;
	s32 recharge_vol;
	bool usb_present;
	bool batt_sw_fc;
	bool fast_mode;
	bool shutdown_delay_enable;
	bool shutdown_delay;

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

	/* Debug */
 	/* s32 skip_reads; */
 	/* s32 skip_writes; */
	/* s32 fake_soc; */
	/* s32 fake_temp; */
	/* s32 fake_chip_ok; */
	struct dentry* debug_root;
	struct power_supply* fg_psy;
#if !(IS_PACK_ONLY)
	struct power_supply* usb_psy;
	struct power_supply* batt_psy;
	struct power_supply* bbc_psy;
#endif
	struct power_supply_desc fg_psy_d;
};

static bool fg_init(struct i2c_client* client);

static int __fg_read_word(struct i2c_client* client, u8 reg, u16* val)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg); /* little endian */
	if (ret < 0) {
		pr_err("i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*val = (u16)ret;

	return 0;
}

static int __fg_write_word(struct i2c_client* client, u8 reg, u16 val)
{
	s32 ret;

	ret = i2c_smbus_write_word_data(client, reg, val); /* little endian */
	if (ret < 0) {
		pr_err("i2c write word fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
		return ret;
	}

	return 0;
}

static int fg_read_sbs_word(struct sh_fg_chip* sm, u32 reg, u16* val)
{
	int ret = -1;

	pr_info("fg_read_sbs_word start, reg=%08X", reg);
	/* 
	if (sm->skip_reads) {
		*val = 0;
		return 0;
	} 
	*/

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_MASK) == CMDMASK_ALTMAC_R) {
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
	if ((reg & CMDMASK_MASK) == CMDMASK_ALTMAC_R) {
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

#if 0
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

static int fg_write_block(struct sh_fg_chip* sm, u32 reg, u8 length, u8* val)
{
	int ret;

	/* 
	if (sm->skip_writes)
		return 0; 
	*/

	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_write_buffer(sm->client, (u8)reg, length, val);
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

	pr_err("cntl=0x%04X, bat_flags=0x%04X", cntl, flags1);
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

	return IRQ_HANDLED;
}

static irqreturn_t fg_irq_thread(int irq, void* dev_id)
{
	struct sh_fg_chip* sm = dev_id;

	fg_status_changed(sm);
	pr_info("fg_read_int");

	return 0;
}
#endif

static s32 fg_read_gaugeinfo_block(struct sh_fg_chip* sm)
{
	static u8 buf[GAUGEINFO_LEN];
	static char str[GAUGESTR_LEN];
	int i, j = 0;

	int ret;

	u64 jiffies_now = jiffies;
	s64 tick = (s64)(jiffies_now - sm->log_lastUpdate);
	if (tick < 0)  //overflow
	{
		tick = (s64)(U64_MAXVALUE - (u64)sm->log_lastUpdate);
		tick += jiffies_now + 1;
	}
	tick /= HZ;
	if (tick < GAUGE_LOG_MIN_TIMESPAN)
		return 0;
	sm->log_lastUpdate = jiffies_now;

	/* Cali Info */
	ret = fg_read_block(sm, CMD_CALIINFO, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read CALIINFO, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Elasp=%d, ", tick);
	i += sprintf(&str[i], "Voltage=%d, ", (s16)BUF2U16_LT(&buf[12]));
	i += sprintf(&str[i], "Current=%d, ", (s16)BUF2U32_LT(&buf[8]));
	i += sprintf(&str[i], "TS1Temp=%d, ", (s16)(BUF2U16_LT(&buf[22]) - TEMPER_OFFSET));
	i += sprintf(&str[i], "IntTemper=%d, ", (s16)(BUF2U16_LT(&buf[18]) - TEMPER_OFFSET));
	j = max(i, j);
	pr_err("SH366101_GaugeLog: CMD_CALIINFO is %s", str);

	/* Gauge Info */
	ret = fg_read_block(sm, CMD_GAUGEINFO, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366101_GaugeLog: could not read GAUGEINFO, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
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
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
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
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
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
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
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
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
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
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
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
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "FusionModel=");
	for (ret = 0; ret < 15; ret++)
		i += sprintf(&str[i], "0x%04X ", BUF2U16_BG(&buf[ret * 2]));

	j = max(i, j);
	pr_err("SH366101_GaugeLog: FusionModel is %s", str);
	pr_err("SH366101_GaugeLog: max len=%d", j);

	ret = 0;

	/* fg_read_gaugeinfo_block_end: */
	return ret;
}

static s32 fg_read_soc(struct sh_fg_chip* sm)
{
	int ret;
	s32 soc = 0;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_SOC], &data);
	if (ret < 0) {
		pr_err("could not read SOC, ret = %d\n", ret);
		return ret;
	} else {
		soc = (s32)data;
		pr_info("fg_read_soc soc=%d\n", soc);
		return soc;
	}
}

static u32 fg_read_ocv(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;
	u32 ocv;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_OCV], &data);
	if (ret < 0) {
		pr_err("could not read OCV, ret = %d\n", ret);
		ocv = 4000 * MA_TO_UA;
	} else {
		ocv = data * MA_TO_UA;
	}

	return ocv;
}

static s32 fg_read_temperature(struct sh_fg_chip* sm, enum sh_fg_temperature_type temperature_type)
{
	s32 ret;
	s32 temp = 0;
	u16 data = 0;

	if (temperature_type == TEMPERATURE_IN) {
		temp = sm->regs[SH_FG_REG_TEMPERATURE_IN];
	} else if (temperature_type == TEMPERATURE_EX) {
		temp = sm->regs[SH_FG_REG_TEMPERATURE_EX];
	} else {
		return -EINVAL;
	}

	ret = fg_read_sbs_word(sm, temp, &data);
	if (ret < 0) {
		pr_err("could not read temperature, ret = %d\n", ret);
		return ret;
	} else {
		temp = (s32)data - 2731;
		pr_info("fg_read_temperature temp=%d\n", temp);
		return temp;
	}
}

static s32 fg_read_volt(struct sh_fg_chip* sm)
{
	s32 ret;
	s32 volt = 0;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_VOLTAGE], &data);
	if (ret < 0) {
		pr_err("could not read voltage, ret = %d\n", ret);
		return ret;
	} else {
		volt = (s32)data * MA_TO_UA;
	}

	sm->aver_batt_volt = (((sm->aver_batt_volt) * 4) + volt) / 5; /*cal avgvoltage*/
	return volt;
}

static s32 fg_get_cycle(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_SOC_CYCLE], &data);
	if (ret < 0) {
		pr_err("read cycle reg fail ret = %d\n", ret);
		data = 0;
	}

	return (s32)data;
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

	return (s32)((s16)data * MA_TO_UA);
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

	return (s32)((s16)data * MA_TO_UA);
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

	return (s32)((s16)data * MA_TO_UA);
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
	pr_err("battery power supply is not registered\n");
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

static s32 fg_get_batt_status(struct sh_fg_chip* sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_STATUS_UNKNOWN;
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
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
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
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
};

static void fg_monitor_workfunc(struct work_struct* work);

#define SHUTDOWN_DELAY_VOL  3300
static s32 fg_get_property(struct power_supply* psy, enum power_supply_property psp, union power_supply_propval* val)
{
	struct sh_fg_chip* sm = power_supply_get_drvdata(psy);
	s32 ret;
	s32 vbat_uv;
	static bool shutdown_delay_cancel;
	static bool last_shutdown_delay;

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

	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = sm->shutdown_delay;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = fg_read_volt(sm);
		mutex_lock(&sm->data_lock);
		if (ret >= 0)
			sm->batt_volt = ret;
		val->intval = sm->batt_volt;
		mutex_unlock(&sm->data_lock);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sm->batt_present;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&sm->data_lock);
		sm->batt_curr = fg_read_current(sm);
		val->intval = sm->batt_curr;
		mutex_unlock(&sm->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
	/*	if (sm->fake_soc >= 0) {
			val->intval = sm->fake_soc;
			break;
		} */
		ret = fg_read_soc(sm);
		mutex_lock(&sm->data_lock);
		if (ret >= 0)
			sm->batt_soc = ret;
		val->intval = sm->batt_soc;
		
		mutex_unlock(&sm->data_lock);
		if(sm->shutdown_delay_enable) {
			if(val->intval == 0) {
				fg_read_volt(sm);
				if((vbat_uv/1000) > SHUTDOWN_DELAY_VOL &&
					(sm->charge_status != POWER_SUPPLY_STATUS_CHARGING)){
					sm->shutdown_delay = true;
					val->intval = 1;
				} else if (sm->charge_status == POWER_SUPPLY_STATUS_CHARGING &&
						sm->shutdown_delay){
						sm->shutdown_delay = false;
						shutdown_delay_cancel = true;
						val->intval = 1;
				}
			} else {
				sm->shutdown_delay = false;
				shutdown_delay_cancel = false;
			}
			if (last_shutdown_delay != sm->shutdown_delay){
				last_shutdown_delay = sm->shutdown_delay;
				if(sm->fg_psy)
					power_supply_changed(sm->fg_psy);
			}
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(sm);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		/* 
		if (sm->fake_temp != -EINVAL) {
			val->intval = sm->fake_temp;
			break;
		} 
		*/
		if (sm->en_temp_in)
			ret = fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			ret = fg_read_temperature(sm, TEMPERATURE_EX);
		else
			ret = -ENODATA;
		mutex_lock(&sm->data_lock);
		if (ret > 0)
			sm->batt_temp = ret;
		val->intval = sm->batt_temp / 10;
		mutex_unlock(&sm->data_lock);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = fg_read_fcc(sm);
		mutex_lock(&sm->data_lock);
		if (ret > 0)
			sm->batt_fcc = ret;
		val->intval = sm->batt_fcc;
		mutex_unlock(&sm->data_lock);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = fg_get_batt_health(sm);
		break;

	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = 330000;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static s32 fg_set_property(struct power_supply* psy, enum power_supply_property prop, const union power_supply_propval* val)
{
#if 0 /* 20211029, Ethan. */
	struct sh_fg_chip* sm = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		sm->fake_temp = val->intval;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		sm->fake_soc = val->intval;
		//ret = fg_read_soc(sm);
		power_supply_changed(sm->fg_psy);
		break;

	default:
		return -EINVAL;
	}
#endif
	return 0;
}

static s32 fg_prop_is_writeable(struct power_supply* psy, enum power_supply_property prop)
{
	s32 ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
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
	sm->fg_psy_d.type = POWER_SUPPLY_TYPE_BMS;
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
	int rm, len;

	rm = fg_read_rmc(sm);
	len = snprintf(buf, MAX_BUF_LEN, "%d\n", rm);

	return len;
}

static ssize_t fg_attr_show_fcc(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sh_fg_chip* sm = i2c_get_clientdata(client);
	int fcc, len;

	fcc = fg_read_fcc(sm);
	len = snprintf(buf, MAX_BUF_LEN, "%d\n", fcc);

	return len;
}

static ssize_t fg_attr_show_batt_volt(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sh_fg_chip* sm = i2c_get_clientdata(client);
	int volt, len;

	volt = fg_read_volt(sm);
	len = snprintf(buf, MAX_BUF_LEN, "%d\n", volt);

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

static void fg_refresh_status(struct sh_fg_chip* sm)
{
	bool last_batt_inserted = sm->batt_present;
	bool last_batt_fc = sm->batt_fc;
	bool last_batt_ot = sm->batt_ot;
	bool last_batt_ut = sm->batt_ut;
	static s32 last_soc, last_temp;

	fg_read_status(sm);
	pr_err("batt_present=%d", sm->batt_present);

	if (!last_batt_inserted && sm->batt_present) { /* battery inserted */
		pr_err("Battery inserted\n");
	} else if (last_batt_inserted && !sm->batt_present) { /* battery removed */
		pr_err("Battery removed\n");
		sm->batt_soc = -ENODATA;
		sm->batt_fcc = -ENODATA;
		sm->batt_volt = -ENODATA;
		sm->batt_curr = -ENODATA;
		sm->batt_temp = -ENODATA;
	}

	if ((last_batt_inserted != sm->batt_present) || (last_batt_fc != sm->batt_fc) || (last_batt_ot != sm->batt_ot) || (last_batt_ut != sm->batt_ut))
		power_supply_changed(sm->fg_psy);

	if (sm->batt_present) {
		fg_read_gaugeinfo_block(sm); /* 20211016, Ethan */
		sm->batt_soc = fg_read_soc(sm);
		sm->batt_ocv = fg_read_ocv(sm);
		sm->batt_volt = fg_read_volt(sm);
		sm->batt_curr = fg_read_current(sm);
		sm->batt_soc_cycle = fg_get_cycle(sm);
		sm->batt_rmc = fg_read_rmc(sm);
		if (sm->en_temp_in)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_EX);
		else
			sm->batt_temp = -ENODATA;
#if !(IS_PACK_ONLY)
		fg_cal_carc(sm);
#endif

		pr_err("RSOC:%d, Volt:%d, Current:%d, Temperature:%d\n", sm->batt_soc, sm->batt_volt, sm->batt_curr, sm->batt_temp);
		pr_err("RM:%d,FC:%d,FAST:%d", sm->batt_rmc, sm->batt_fcc, sm->fast_mode);

		if ((last_soc != sm->batt_soc) || (last_temp != sm->batt_temp)) {
			if (sm->fg_psy)
				power_supply_changed(sm->fg_psy);
		}

		last_soc = sm->batt_soc;
		last_temp = sm->batt_temp;
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
	s32 term_curr, full_volt, rc;
	s32 interval = MONITOR_WORK_10S;

	if (!sm->usb_psy)
		return interval;

	if (!sm->chg_dis_votable)
		sm->chg_dis_votable = find_votable("CHG_DISABLE");

	if (!sm->fv_votable)
		sm->fv_votable = find_votable("BBC_FV");

	rc = power_supply_get_property(sm->usb_psy,
		POWER_SUPPLY_PROP_PRESENT, &prop);
	if (!prop.intval) {
		vote(sm->chg_dis_votable, BMS_FC_VOTER, false, 0);
		sm->batt_sw_fc = false;
		full_check = 0;
		return interval;
	}

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
	full_volt = get_effective_result(sm->fv_votable) / 1000 - 20;

	pr_info("term:%d, full_volt:%d, usb_present:%d, batt_sw_fc:%d", term_curr, full_volt, sm->usb_present, sm->batt_sw_fc);

	if (sm->usb_present 
	&& (sm->batt_soc == SM_RAW_SOC_FULL) 
	&& (sm->batt_volt > full_volt) 
	&& (sm->batt_curr < 0) 
	&& (sm->batt_curr > term_curr * (-1)) 
	&& (!sm->batt_sw_fc)) {
		full_check++;
		pr_err("full_check:%d\n", full_check);
		if (full_check > BAT_FULL_CHECK_TIME) {
			sm->batt_sw_fc = true;
			vote(sm->chg_dis_votable, BMS_FC_VOTER, true, 0);
			pr_err("detect charge termination sm->batt_sw_fc:%d\n", sm->batt_sw_fc);
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

	if ((sm->batt_soc <= SM_RECHARGE_SOC) 
		&& sm->batt_sw_fc 
		&& (sm->health != POWER_SUPPLY_HEALTH_WARM)) {
		sm->batt_sw_fc = false;
		prop.intval = true;
		vote(sm->chg_dis_votable, BMS_FC_VOTER, false, 0);
		rc = power_supply_get_property(sm->batt_psy, POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);		
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

	pr_info("device_id = 0x%04X\n", data);
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
	pr_info("gpio_int=%d\n", sm->gpio_int);

	if (!gpio_is_valid(sm->gpio_int)) {
		pr_info("gpio_int is not valid\n");
		sm->gpio_int = -EINVAL;
	}

	/* EN TEMP EX/IN */
	if (of_property_read_bool(np, "sm,en_temp_ex"))
		sm->en_temp_ex = true;
	else
		sm->en_temp_ex = 0;
	pr_info("Temperature EX enabled = %d\n", sm->en_temp_ex);

	if (of_property_read_bool(np, "sm,en_temp_in"))
		sm->en_temp_in = true;
	else
		sm->en_temp_in = 0;
	pr_info("Temperature IN enabled = %d\n", sm->en_temp_in);

	/* EN BATT DET  */
	if (of_property_read_bool(np, "sm,en_batt_det"))
		sm->en_batt_det = true;
	else
		sm->en_batt_det = 0;
	pr_info("Batt Det enabled = %d\n", sm->en_batt_det);
	/* Shutdown feature */
        if (of_property_read_bool(np,"sm,shutdown-delay-enable"))
		sm->shutdown_delay_enable = true;
	else
		sm->shutdown_delay_enable = 0;

	return 0;
}

static s32 get_battery_id(struct sh_fg_chip* sm)
{
	return 0;
}

static s32 Check_Chip_Version(struct sh_fg_chip* sm)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;
	u32 version_main, version_date, version_afi, version_ts;
	s32 ret = CHECK_VERSION_ERR;
	u16 temp;
	s32 date;
	/* 20211025, Ethan. IAP Fail Check */
	struct sh_decoder decoder;
	u8 iap_read[IAP_READ_LEN];

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("Check_Chip_Version: Cannot find child node \"battery_params\"\n");
		return CHECK_VERSION_ERR;
	}

	of_property_read_u32(np, "version_main", &version_main);
	of_property_read_u32(np, "version_date", &version_date);
	of_property_read_u32(np, "version_afi", &version_afi);
	of_property_read_u32(np, "version_ts", &version_ts);
	of_property_read_u8(np, "iap_twiadr", &decoder.addr); /* 20211025, Ethan */

	pr_err("Check_Chip_Version: main=0x%04X, date=0x%08X, afi=0x%04X, ts=0x%04X", version_main, version_date, version_afi, version_ts);

	/* 20211025, Ethan. IAP Fail Check. iap addr may differ from normal addr */
	decoder.reg = (u8)CMD_IAPSTATE_CHECK;
	decoder.length = IAP_READ_LEN;
	if ((fg_decode_iic_read(sm, &decoder, iap_read) >= 0) && (iap_read[0] != 0) && (iap_read[1] != 0)) {
		pr_err("Check_Chip_Version: ic is in iap mode, force update all");
		ret = CHECK_VERSION_FW | CHECK_VERSION_AFI | CHECK_VERSION_TS;
		goto Check_Chip_Version_End;
	}
	HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */

	/* unseal IC */
	if (fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_UNSEALKEY) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */

	if (fg_write_sbs_word(sm, CMD_ALTMAC, (u16)(CMD_UNSEALKEY >> 16)) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */

	/* check fw version */
	if (fg_read_sbs_word(sm, CMD_FWVERSION_MAIN, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	pr_err(" Chip_Version: ic main=0x%04X ", temp);

	if (temp < version_main) {
		ret = CHECK_VERSION_FW | CHECK_VERSION_AFI | CHECK_VERSION_TS;
		goto Check_Chip_Version_End;
	} else if (temp > version_main)
		ret = CHECK_VERSION_OK;
	else { /* version equal, check date */
		if (fg_read_sbs_word(sm, CMD_FWDATE1, &temp) < 0) {
			ret = CHECK_VERSION_ERR;
			goto Check_Chip_Version_End;
		}
		msleep(CMD_SBS_DELAY);
		date = (u32)temp << 16;

		if (fg_read_sbs_word(sm, CMD_FWDATE2, &temp) < 0) {
			ret = CHECK_VERSION_ERR;
			goto Check_Chip_Version_End;
		}
		date |= (temp & FW_DATE_MASK);
		pr_err(" Chip_Version: ic date=0x%08X ", date);
		if (date < version_date) {
			ret = CHECK_VERSION_FW | CHECK_VERSION_AFI | CHECK_VERSION_TS;
			goto Check_Chip_Version_End;
		} else
			ret = CHECK_VERSION_OK;
	}

	/* check afi */
	if (fg_read_sbs_word(sm, CMD_AFI_STATIC_SUM, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	pr_err(" Chip_Version: ic afi=0x%04X ", temp);
	if (temp != version_afi)
		ret |= CHECK_VERSION_AFI;

	/* check TS */
	if (fg_read_sbs_word(sm, CMD_TS_VER, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	pr_err(" Chip_Version: ic ts=0x%04X ", temp);
	if (temp != version_ts)
		ret |= CHECK_VERSION_TS;

Check_Chip_Version_End:
	fg_write_sbs_word(sm, CMD_ALTMAC, CMD_SEAL);
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
	pr_err("file_decode_process: ele_len=%d, key=%s", buflen, profile_name);

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
				//pr_debug("file_decode_process loop compare: length=%d, IC read=%s", line_length, strDebug);
				//print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &(pBuf[INDEX_DATA + i], line_length);
				//pr_debug("file_decode_process loop compare: length=%d, host=%s", line_length, strDebug);

				result = 0;
				for (j = 0; j < line_length; j++) {
					if (pBuf[INDEX_DATA + i + j] != pBuf_Read[j]) {
						result = ERRORTYPE_COMPARE;
						break;
					}
				}

				if (result == 0)
					break;

				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process compare error! index=%d, retry=%d, host=%s", i, retry, strDebug);
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, 32);
				pr_err("ic=%s", i, strDebug);

			file_decode_process_compare_loop_end:
				HOST_DELAY(COMPARE_RETRY_WAIT); /* 20211029, Ethan */
			}

			if (retry >= COMPARE_RETRY_CNT)
				goto main_process_error;

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
	pr_info("battery id = %d\n", battery_id);

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
			pm_stay_awake(sm->dev);
		} else {
			sm->batt_sw_fc = false;
			sm->usb_present = false;
			pm_relax(sm->dev);
		}
	}
#endif
	return NOTIFY_OK;
}

static s32 sh_fg_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	s32 ret;
	s32 version_ret;
	s32 retry;
	struct sh_fg_chip* sm;
	u32* regs;
	pr_err("2021.09.10 wsy %s: start\n", __func__);

	pr_info("enter\n");
	sm = devm_kzalloc(&client->dev, sizeof(*sm), GFP_KERNEL);

	if (!sm)
		return -ENOMEM;

	sm->dev = &client->dev;
	sm->client = client;
	sm->chip = id->driver_data;

	sm->batt_soc = -ENODATA;
	sm->batt_fcc = -ENODATA;
	sm->batt_volt = -ENODATA;
	sm->batt_temp = -ENODATA;
	sm->batt_curr = -ENODATA;
 	/* sm->fake_soc = -EINVAL; */
	/* sm->fake_temp = -EINVAL; */

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

	/* 20211013, Ethan. Firmware Update */
	version_ret = Check_Chip_Version(sm);
	if (version_ret == CHECK_VERSION_ERR) {
		pr_err("Probe: Check version error!");
	} else if (version_ret == CHECK_VERSION_OK) {
		pr_err("Probe: Check version ok!");
	} else {
		pr_err("Probe: Check version update: %X", version_ret);

		if (version_ret & CHECK_VERSION_FW) {
			pr_err("Probe: Firmware Update start");
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				ret = file_decode_process(sm, "sinofs_image_data");
				if (ret == ERRORTYPE_NONE)
					break;
				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
			}
			pr_err("Probe: Firmware Update end, ret=%d", ret);
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
		}

		if (version_ret & CHECK_VERSION_AFI) {
			pr_err("Probe: AFI Update start");
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				ret = file_decode_process(sm, "sinofs_afi_data");
				if (ret == ERRORTYPE_NONE)
					break;
				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
			}
			pr_err("Probe: AFI Update end, ret=%d", ret);
		}
	}

	if (!hal_fg_init(client)) {
		pr_err("Failed to Initialize Fuelgauge\n");
		goto err_0;
	}

	INIT_DELAYED_WORK(&sm->monitor_work, fg_monitor_workfunc);

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

	ret = sysfs_create_group(&sm->dev->kobj, &fg_attr_group);
	if (ret)
		pr_err("Failed to register sysfs:%d\n", ret);

	schedule_delayed_work(&sm->monitor_work, 10 * HZ);
	pr_info("sh fuel gauge probe successfully, %s\n", device2str[sm->chip]);
	//pr_err("2021.09.10 wsy %s: end\n", __func__);

	return 0;

err_0:
	return ret;
}

static s32 sh_fg_remove(struct i2c_client* client)
{
	struct sh_fg_chip* sm = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&sm->monitor_work);

	fg_psy_unregister(sm);

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

