/*
 * bq2589x_charger.c - Charger driver for TI BQ25890,BQ25892 and BQ25895
 *
 * Copyright (C) 2011 Intel Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Zhang Yu <yu.d.zhang@intel.com>
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/power/bq2589x_charger.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/version.h>
#include <linux/usb/otg.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/completion.h>
#include <linux/notifier.h>
#include <linux/extcon.h>
#include <asm/intel_em_config.h>
#include "bq2589x_reg.h"
#include <linux/ktime.h>
#include <linux/alarmtimer.h>

#define DRV_NAME "bq2589x_charger"
#define DEV_NAME "bq2589x"

/* BQ2589X IINLIM REG 00 */
#define BQ2589X_REG_00		0x0
/* D7 is used to control HIZ */
#define INPUT_SRC_CNTL_EN_HIZ			(1 << 7)
#define BATTERY_NEAR_FULL(a)			((a * 98)/100)
/*
 * D0, D1, D2, D3, D4, D5 represent the input current limit
 * offset = 100mA
 */
#define BQ2589X_IINLIM_MASK		    0x3F
#define BQ2589X_IINLIM_SHIFT		0
#define BQ2589X_IINLIM_BASE         100
#define BQ2589X_IINLIM_LSB          50
#define BQ2589X_IINLIM_MIN          100
#define BQ2589X_IINLIM_MAX          3250

/* BQ2589X VINDPMOS REG 01 */
#define BQ2589X_REG_01		0x1
/*
 * VINDPM = VBUS-VINDPMOS, when VBUS@noload <= 6v
 * VINDPM = VBUS-VINDPMOS*2, when VBUS@noload > 6v
 * VINDPMOS default is 600mV
 * D0, D1, D2, D3, D4 can be used to voltage limit offset
 */
#define BQ2589X_VINDPMOS_MASK       0x1F
#define BQ2589X_VINDPMOS_SHIFT      0
#define BQ2589X_VINDPMOS_BASE       0
#define BQ2589X_VINDPMOS_LSB        100

/* BQ2589X VINDPMOS REG 02 */
#define BQ2589X_REG_02		0x2
/* D3 show HVDCP enable bit */
#define CHRG_HVDCP_EN	(1 << 3)
/* D2 show MAXC enable bit */
#define CHRG_MAXC_EN	(1 << 2)
/* D1 show FORCE_DP_DM bit */
#define CHRG_FORCE_DP_DM	(1 << 1)

/* BQ2419X CHRG/OTG CONFIG REG 03 */
#define BQ2589X_REG_03		0x03
/* D6 show watchdog timer reset */
#define WDTIMER_RESET_MASK			0x40
/* D4, D5 can be used to control the charger
 * BQ2419X series charger and OTG enable bits
 * */
#define CHR_CFG_BIT_POS				4
#define CHR_CFG_BIT_LEN				2
#define CHR_CFG_CHRG_MASK			3
#define POWER_ON_CFG_CHRG_OTG_CFG_DIS	(0 << 4)
#define POWER_ON_CFG_CHRG_CFG_EN		(1 << 4)
#define POWER_ON_CFG_CHRG_CFG_OTG		(2 << 4)
/* D1, D2, D3 can be used to set min sys voltage limit
 * default min sys is 3.5v */

/* BQ2589X FAST CHARGE CURRENT LIMIT REG 04 */
#define BQ2589X_REG_04		0x04
/* D0, D1, D2, D3, D4, D5, D6 show fast charge current limit
 * Charge Current range from 0 to 5056 mA */
#define BQ2589X_CHRG_CUR_OFFSET		0	/* 0 mA */
#define BQ2589X_CHRG_CUR_LSB_TO_CUR	64	/* 64 mA */
#define BQ2589X_GET_CHRG_CUR(reg) ((reg>>2)*BQ2589X_CHRG_CUR_LSB_TO_CUR\
			+ BQ2589X_CHRG_CUR_OFFSET) /* in mA */

/* BQ2589X IPRECHG & ITERM REG 05 */
#define BQ2589X_REG_05	0x05
/* D4, D5, D6, D7 show Precharge current limit  */
#define BQ2589X_PRE_CHRG_CURR_256		(3 << 4)  /* 256mA */
/* D0, D1, D2, D3 show Termination current limit  */
#define BQ2589X_CHRG_ITERM_OFFSET       64
#define BQ2589X_CHRG_CUR_LSB_TO_ITERM   64
#define BQ2589X_CHRG_ITERM_MAX       1024
#define BQ2589X_CHRG_IPRE_OFFSET       64
#define BQ2589X_CHRG_CUR_LSB_TO_IPRE   64
#define BQ2589X_CHRG_IPRE_MAX       1024

/* BQ2589X CHARGE VOLTAGE LIMIT REG 06 */
#define BQ2589X_REG_06	0x06
/* D2, D3, D4, D5, D6, D7 show charge voltage limit */
#define BQ2589X_CHRG_VOLT_OFFSET	3840	/* 3840 mV */
#define BQ2589X_CHRG_VOLT_LSB_TO_VOLT	16	/* 16 mV */
/* D1 show precharge to fast charge threshold
 * Low voltage setting 0 - 2.8V and 1 - 3.0V
 * */
#define CHRG_VOLT_CNTL_BATTLOWV		(1 << 1)
/* D0 show recharge threshold
 * Battery Recharge threshold 0 - 100mV and 1 - 200mV
 * */
#define CHRG_VOLT_CNTL_VRECHRG		(1 << 0)
#define BQ2589X_GET_CHRG_VOLT(reg) ((reg>>2)*BQ2589X_CHRG_VOLT_LSB_TO_VOLT\
			+ BQ2589X_CHRG_VOLT_OFFSET) /* in mV */

/* BQ2589X TIMER CNTL REG 07 */
#define BQ2589X_REG_07		0x07
/* D7 show charging termination enable */
#define CHRG_TIMER_EXP_CNTL_EN_TERM	(1 << 7)
/* D6 show termination indication disable */
#define CHRG_TIMER_EXP_TERM_STAT_DIS	(1 << 6)
/* D4, D5 show watchdog timer setting
 * WDT Timer uses 2 bits
 * */
#define WDT_TIMER_BIT_POS			4
#define WDT_TIMER_BIT_LEN			2
#define CHRG_TIMER_EXP_CNTL_WDTDISABLE		(0 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT40SEC		(1 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT80SEC		(2 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT160SEC		(3 << 4)
/* D3 show safety timer enable bit */
#define CHRG_TIMER_EXP_CNTL_EN_TIMER		(1 << 3)
/* Charge Timer uses 2bits(20 hrs) */
#define SFT_TIMER_BIT_POS			1
#define SFT_TIMER_BIT_LEN			2
#define CHRG_TIMER_EXP_CNTL_SFT_TIMER		(3 << 1)
/* D1, D2 show fast charger timer setting
 * default is 12 hours(10) */




/* BQ2589X SYSTEM STAT REG 0A */
#define BQ2589X_REG_0A          0x0A
/* D4, D5, D6, D7 show boost mode voltage regulation
 * default is 4.998v(0111) for bq25890/2
 * default is 5.126v(1001) for bq25895
 * */

/* D0, D1, D2 show boost mode current limit */
#define BOOST_CURR_LIM_POS          0
#define BOOST_CURR_LIM_LEN          3
#define BOOST_CURRENT_LIM_500_MA        (0 << 0)
#define BOOST_CURRENT_LIM_700_MA        (1 << 0)
#define BOOST_CURRENT_LIM_1100_MA       (2 << 0)
#define BOOST_CURRENT_LIM_1300_MA       (3 << 0)
#define BOOST_CURRENT_LIM_1600_MA       (4 << 0)
#define BOOST_CURRENT_LIM_1800_MA       (5 << 0)
#define BOOST_CURRENT_LIM_2100_MA       (6 << 0)
#define BOOST_CURRENT_LIM_2400_MA       (7 << 0)

/* BQ2589X SYSTEM STAT REG 0B */
#define BQ2589X_REG_0B			0x0B
/* D5, D6, D7 show VBUS status
 * for bq25890/5 chip
 */
#define SYSTEM_STAT_VBUS_BITS			(7 << 5)
#define SYSTEM_STAT_VBUS_UNKNOWN		(0 << 5)
#define SYSTEM_STAT_VBUS_SDP			(1 << 5)
#define SYSTEM_STAT_VBUS_CDP			(2 << 5)
#define SYSTEM_STAT_VBUS_DCP			(3 << 5)
#define SYSTEM_STAT_VBUS_HVDCP          (4 << 5)
#define SYSTEM_STAT_VBUS_UNK_ADP        (5 << 5)
#define SYSTEM_STAT_VBUS_UNK_STD_ADP    (6 << 5)
#define SYSTEM_STAT_VBUS_OTG            (7 << 5)
/* D3, D4 show charger status */
#define SYSTEM_STAT_CHRG_MASK			(3 << 3)
#define SYSTEM_STAT_NOT_CHRG			(0 << 3)
#define SYSTEM_STAT_PRE_CHRG			(1 << 3)
#define SYSTEM_STAT_FAST_CHRG			(2 << 3)
#define SYSTEM_STAT_CHRG_DONE			(3 << 3)
/* D2 show power good status  */
#define SYSTEM_STAT_PWR_GOOD            (1 << 2)
/* D1 show USB input status*/
#define SYSTEM_STAT_SDP				(1 << 1)
/* D0 show SYS status*/
#define SYSTEM_STAT_VSYS_LOW			(1 << 0)

/* BQ2589X FAULT STAT REG 0C */
#define BQ2589X_REG_0C			0x0C
/* D7 show WDT EXP */
#define FAULT_STAT_WDT_TMR_EXP			(1 << 7)
/* D6 show boost mode status*/
#define FAULT_STAT_OTG_FLT			(1 << 6)
/* D4, D5 show charger fault status */
#define FAULT_STAT_CHRG_BITS			(3 << 4)
#define FAULT_STAT_CHRG_NORMAL			(0 << 4)
#define FAULT_STAT_CHRG_IN_FLT			(1 << 4)
#define FAULT_STAT_CHRG_THRM_FLT		(2 << 4)
#define FAULT_STAT_CHRG_TMR_FLT			(3 << 4)
/* D3 show battery status */
#define FAULT_STAT_BATT_FLT			(1 << 3)
/* D0, D1, D2 show NTC status
 * for bq25890/2 chip */
#define FAULT_STAT_BATT_TEMP_BITS		(7 << 0)
#define FAULT_STAT_BATT_TEMP_NORMAL     (0 << 0)
#define FAULT_STAT_BATT_TEMP_WARM       (2 << 0)
#define FAULT_STAT_BATT_TEMP_COOL       (3 << 0)
#define FAULT_STAT_BATT_TEMP_COLD       (5 << 0)
#define FAULT_STAT_BATT_TEMP_HOT        (6 << 0)

/* BQ2589X VINDPM REG 0D */
#define BQ2589X_REG_0D		0x0D
/* D7 is used to VINDPM EN */
#define BQ2589X_FORCE_VINDPM			(1 << 7)
/* D0, D1, D2, D3, D4, D5, D6 can be used to voltage limit */
#define BQ2589X_VINDPM_MASK         0x7F
#define BQ2589X_VINDPM_SHIFT        0
#define BQ2589X_VINDPM_BASE         2600
#define BQ2589X_VINDPM_LSB          100
#define BQ2589X_VINDPM_MAX          15300
#define BQ2589X_VINDPM_MIN          3900

/* BQ2589X DEVICE VBUS REG 11 */
#define BQ2589X_REG_11			0x11
/* D7 indicates the vbus gd */
#define BQ25890_VBUS_GOOD			(1 << 7)

/* BQ2589X DEVICE CONFIG REG 14 */
#define BQ2589X_REG_14			0x14
/* D3, D4, D5 indicates the chip model number */
#define BQ2589X_VERSION_MASK			0x38
#define BQ25890_IC_VERSION			0x3
#define BQ25892_IC_VERSION			0x0
#define BQ25895_IC_VERSION			0x7

#define NR_RETRY_CNT		3

#define CHARGER_PS_NAME				"bq2589x_charger"

#define CHARGER_TASK_JIFFIES		(HZ * 150)/* 150sec */
#define CHARGER_HOST_JIFFIES		(HZ * 60) /* 60sec */
#define FULL_THREAD_JIFFIES		(HZ * 30) /* 30sec */
#define TEMP_THREAD_JIFFIES		(HZ * 30) /* 30sec */

/* Battery profile needs to be modified according to SPEC */
#define BATT_TEMP_MAX_DEF	60	/* 60 degrees */
#define BATT_TEMP_MIN_DEF	0
#define BATT_CV_MAX_DEF     4400  /* 4400mV */
/* Note: because there is no inlim info from charger, so we have
 * to configure cc max to large current as default,otherwise,
 * the fast charge current will be throttled */
#define BATT_CC_MAX_DEF     3000  /* 3000mA */

/* Max no. of tries to clear the charger from Hi-Z mode */
#define MAX_TRY		3

/* Max no. of tries to reset the bq2589x WDT */
#define MAX_RESET_WDT_RETRY 8
#define VBUS_DET_TIMEOUT msecs_to_jiffies(50) /* 50 msec */

/* hvdcp charger current limit needs to
 * be mofified according to specific charger
 * */
#define DC_CHARGE_CUR_HVDCP       2000
#define DC_CHARGE_CUR_DCP         2000
#define DC_CHARGE_CUR_CDP		  1500
#define DC_CHARGE_CUR_SDP		  500

/* dump all of the registers in chrg task worker for debugging */
/* #define DEBUG */

static struct power_supply *fg_psy;

enum bq2589x_chrgr_stat {
	BQ2589X_CHRGR_STAT_UNKNOWN,
	BQ2589X_CHRGR_STAT_CHARGING,
	BQ2589X_CHRGR_STAT_BAT_FULL,
	BQ2589X_CHRGR_STAT_FAULT,
	BQ2589X_CHRGR_STAT_LOW_SUPPLY_FAULT
};

enum bq2589x_chip_type {
	BQ25890, BQ25892, BQ25895
};

struct bq2589x_chip {
	struct i2c_client *client;
	struct bq2589x_platform_data *pdata;
	enum bq2589x_chip_type chip_type;
	struct power_supply usb;
	struct delayed_work chrg_full_wrkr;
	struct delayed_work chrg_temp_wrkr;
	struct delayed_work irq_wrkr;
	struct delayed_work reg_wrkr;
	struct delayed_work cdp_wrkr;
	struct delayed_work hvdcp_wrkr;
	struct mutex event_lock;
	struct power_supply_cable_props cap;
	struct power_supply_cable_props cached_cap;
	struct usb_phy *transceiver;
	struct completion vbus_detect;
	struct work_struct	otg_work;
	struct extcon_specific_cable_nb cable_obj;
	struct notifier_block	id_nb;
	bool   id_short;

	enum bq2589x_chrgr_stat chgr_stat;
	enum power_supply_charger_cable_type cable_type;
	int cc;
	int cv;
	int inlmt;
	int max_cc;
	int max_cv;
	int max_temp;
	int min_temp;
	int iterm;
	int batt_status;
	int bat_health;
	int cntl_state;
	int cntl_state_max;
	int irq;
	bool is_charging_enabled;
	bool a_bus_enable;
	bool is_pwr_good;
	bool boost_mode;
	bool online;
	bool present;
	struct wake_lock wl;
	struct alarm alarm;
	struct delayed_work feed_wdt_work;
	struct wake_lock feed_wdt_wl;
};

#ifdef CONFIG_DEBUG_FS
#define BQ2589X_MAX_MEM		21
static struct dentry *bq2589x_dbgfs_root;
static char bq2589x_dbg_regs[BQ2589X_MAX_MEM][4];
#endif

static struct i2c_client *bq2589x_client;

static enum power_supply_property bq2589x_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INLMT,
	POWER_SUPPLY_PROP_ENABLE_CHARGING,
	POWER_SUPPLY_PROP_CHARGE_TERM_CUR,
	POWER_SUPPLY_PROP_CABLE_TYPE,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_MAX_TEMP,
	POWER_SUPPLY_PROP_MIN_TEMP,
	POWER_SUPPLY_PROP_VBUS_GOOD,
	POWER_SUPPLY_PROP_ENABLE_HVDCP,
	POWER_SUPPLY_PROP_SET_VINDPM,
};

/* Need to be modified according to the thermal test  */
#define BYTCR_CHRG_CUR_NOLIMIT  BATT_CC_MAX_DEF
#define BYTCR_CHRG_CUR_MEDIUM   1400
#define BYTCR_CHRG_CUR_LOW      1000

static struct ps_batt_chg_prof byt_ps_batt_chrg_prof;
static struct power_supply_throttle byt_throttle_states[] = {
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BYTCR_CHRG_CUR_NOLIMIT,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BYTCR_CHRG_CUR_MEDIUM,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BYTCR_CHRG_CUR_LOW,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGING,
	},
};

static void *platform_byt_get_batt_charge_profile(void)
{
	get_batt_prop(&byt_ps_batt_chrg_prof);

	return &byt_ps_batt_chrg_prof;
}

static enum power_supply_type get_power_supply_type(
		enum power_supply_charger_cable_type cable)
{

	switch (cable) {

	case POWER_SUPPLY_CHARGER_TYPE_USB_DCP:
		return POWER_SUPPLY_TYPE_USB_DCP;
	case POWER_SUPPLY_CHARGER_TYPE_USB_CDP:
		return POWER_SUPPLY_TYPE_USB_CDP;
	case POWER_SUPPLY_CHARGER_TYPE_USB_ACA:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK:
		return POWER_SUPPLY_TYPE_USB_ACA;
	case POWER_SUPPLY_CHARGER_TYPE_AC:
		return POWER_SUPPLY_TYPE_MAINS;
	case POWER_SUPPLY_CHARGER_TYPE_NONE:
	case POWER_SUPPLY_CHARGER_TYPE_USB_SDP:
	default:
		return POWER_SUPPLY_TYPE_USB;
	}

	return POWER_SUPPLY_TYPE_USB;
}

/*-------------------------------------------------------------------------*/


/*
 * Genenric register read/write interfaces to access registers in charger ic
 */

static int bq2589x_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
	int ret, i;

	/* if the setting make both otg and charger disabled, panic*/
	if ((BQ2589X_REG_03 == reg) && !(value & 0x30))
		panic("The setting will put bq2589x ***NEITHER***"
			"in charger mode ***NOR*** in boost mode \n");

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_write_byte_data(client, reg, value);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Write error:%d\n", ret);

	return ret;
}

static int bq2589x_read_reg(struct i2c_client *client, u8 reg)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Read error:%d\n", ret);

	return ret;
}

static int bq2589x_mask_write(struct i2c_client *client, u8 reg, u8 mask, u8 data)
{
	int ret, tmp;

	tmp = bq2589x_read_reg(client, reg);
	if (tmp < 0)
		return tmp;

	tmp &= ~mask;
	tmp |= data & mask;
	pr_debug("%s: reg %x mask %x, data %x tmp %x\n",
			__func__, reg, mask, data, tmp);
	ret = bq2589x_write_reg(client, reg, tmp);

	return ret;
}

/*
 * This function dumps the bq2589x registers
 */
static void bq2589x_dump_registers(struct bq2589x_chip *chip)
{
	int i, ret;
	unsigned char reg[21];
	char temp[10];
	/* a3:01: means a3 product version 01 debug log */
	char log[128] = "a3:01: ";

	for (i = 0; i < 21; i++) {
		reg[i] = bq2589x_read_reg(chip->client, i);
		if (ret < 0)
			dev_warn(&chip->client->dev, "BQ2589X_REG_0x%02x read fail\n", i);
		sprintf(temp, "0x%02x ", reg[i]);
		strcat(log, temp);
	}
	strcat(log, "\n");
	printk(log);
}

/*
 * If the bit_set is TRUE then val 1s will be SET in the reg else val 1s will
 * be CLEARED
 */
static int bq2589x_reg_read_modify(struct i2c_client *client, u8 reg,
							u8 val, bool bit_set)
{
	int ret;

	ret = bq2589x_read_reg(client, reg);

	if (bit_set)
		ret |= val;
	else
		ret &= (~val);

	ret = bq2589x_write_reg(client, reg, ret);

	return ret;
}

/* multi-bit will be configured according to the val, from the start of
 * the pos
 * */
static int bq2589x_reg_multi_bitset(struct i2c_client *client, u8 reg,
						u8 val, u8 pos, u8 len)
{
	int ret;
	u8 data;

	ret = bq2589x_read_reg(client, reg);
	if (ret < 0) {
		dev_warn(&client->dev, "I2C SMbus Read error:%d\n", ret);
		return ret;
	}

	data = (1 << len) - 1;
	ret = (ret & ~(data << pos)) | val;
	ret = bq2589x_write_reg(client, reg, ret);

	return ret;
}

static void bq2589x_feed_wdt_work(struct work_struct *work)
{
	int ret = 0;
	struct bq2589x_chip *chip = container_of(work,
			struct bq2589x_chip, feed_wdt_work.work);

	/*  reset WDT timer */
	ret = bq2589x_reg_read_modify(chip->client, BQ2589X_REG_03,
					WDTIMER_RESET_MASK, true);
	if (ret < 0)
		dev_warn(&chip->client->dev, "I2C write failed:%s\n", __func__);
}

#define FEED_WDT_INTERVAL_NS	150000000000
static enum alarmtimer_restart bq2589x_feed_wdt(struct alarm *alarm,
							ktime_t now)
{
	struct bq2589x_chip *chip = container_of(alarm,
				struct bq2589x_chip, alarm);

	pr_info("%s\n", __func__);
	wake_lock_timeout(&chip->feed_wdt_wl, msecs_to_jiffies(500));
	schedule_delayed_work(&chip->feed_wdt_work,
			msecs_to_jiffies(200));
	alarm_forward_now(&chip->alarm,
			ns_to_ktime(FEED_WDT_INTERVAL_NS));
	return ALARMTIMER_RESTART;
}

/*
 * This function verifies if the bq2589x charger chip is in Hi-Z
 * If yes, then clear the Hi-Z to resume the charger operations
 */
static int bq2589x_clear_hiz(struct bq2589x_chip *chip)
{
	int ret, count;

	dev_info(&chip->client->dev, "%s\n", __func__);

	/*
	 * Read the bq2589x REG00 register for charger Hi-Z mode.
	 * If it is in Hi-Z, then clear the Hi-Z to resume the charging
	 * operations.
	 */
	ret = bq2589x_read_reg(chip->client,
			BQ2589X_REG_00);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
				"BQ2589X_REG_00 read failed\n");
		goto i2c_error;
	}

	if (ret & INPUT_SRC_CNTL_EN_HIZ) {
		dev_warn(&chip->client->dev,
					"Charger IC in Hi-Z mode\n");
		/* Clear the Charger from Hi-Z mode */
		ret &= ~INPUT_SRC_CNTL_EN_HIZ;

		/* Write the values back */
		ret = bq2589x_write_reg(chip->client,
				BQ2589X_REG_00, ret);
		if (ret < 0) {
			dev_warn(&chip->client->dev,
					"BQ2589X_REG_00 write failed\n");
			goto i2c_error;
		}
	} else {
		dev_info(&chip->client->dev,
					"Charger is not in Hi-Z\n");
	}
	return ret;
i2c_error:
	dev_err(&chip->client->dev, "%s\n", __func__);
	return ret;
}

static int bq2589x_enable_hiz(struct bq2589x_chip *chip)
{
	int ret, count;

	dev_info(&chip->client->dev, "%s\n", __func__);

	/*
	 * Read the bq2589x REG00 register for charger Hi-Z mode.
	 * If it is in Hi-Z, then clear the Hi-Z to resume the charging
	 * operations.
	 */
	ret = bq2589x_read_reg(chip->client,
			BQ2589X_REG_00);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
				"BQ2589X_REG_00 read failed\n");
		goto i2c_error;
	}

	if (ret & INPUT_SRC_CNTL_EN_HIZ) {
		dev_warn(&chip->client->dev,
					"Charger IC in Hi-Z mode\n");
	} else {
		/* Clear the Charger from Hi-Z mode */
		ret |= INPUT_SRC_CNTL_EN_HIZ;

		/* Write the values back */
		ret = bq2589x_write_reg(chip->client,
				BQ2589X_REG_00, ret);
		if (ret < 0) {
			dev_warn(&chip->client->dev,
					"BQ2589X_REG_00 write failed\n");
			goto i2c_error;
		}
		dev_info(&chip->client->dev,
					"Charger is not in Hi-Z\n");
	}
	return ret;
i2c_error:
	dev_err(&chip->client->dev, "%s\n", __func__);
	return ret;
}

/* check_batt_psy -check for whether power supply type is battery
 * @dev : Power Supply dev structure
 * @data : Power Supply Driver Data
 * Context: can sleep
 *
 * Return true if power supply type is battery
 *
 */
static int check_batt_psy(struct device *dev, void *data)
{
	struct power_supply *psy = dev_get_drvdata(dev);

	/* check for whether power supply type is battery */
	if (psy->type == POWER_SUPPLY_TYPE_BATTERY) {
		fg_psy = psy;
		return 1;
	}
	return 0;
}

/**
 * get_fg_chip_psy - identify the Fuel Gauge Power Supply device
 * Context: can sleep
 *
 * Return Fuel Gauge power supply structure
 */
static struct power_supply *get_fg_chip_psy(void)
{
	if (fg_psy)
		return fg_psy;

	/* loop through power supply class */
	class_for_each_device(power_supply_class, NULL, NULL,
			check_batt_psy);
	return fg_psy;
}

/**
 * fg_chip_get_property - read a power supply property from Fuel Gauge driver
 * @psp : Power Supply property
 *
 * Return power supply property value
 *
 */
static int fg_chip_get_property(enum power_supply_property psp)
{
	union power_supply_propval val;
	int ret = -ENODEV;

	if (!fg_psy)
		fg_psy = get_fg_chip_psy();
	if (fg_psy) {
		ret = fg_psy->get_property(fg_psy, psp, &val);
		if (!ret)
			return val.intval;
	}
	return ret;
}

/**
 * bq2589x_get_charger_health - to get the charger health status
 *
 * Returns charger health status
 */
int bq2589x_get_charger_health(void)
{
	int ret_status, ret_fault;

	struct bq2589x_chip *chip =
		i2c_get_clientdata(bq2589x_client);

	dev_dbg(&chip->client->dev, "%s\n", __func__);

	/* If we do not have any cable connected, return health as UNKNOWN */
	if (chip->cable_type == POWER_SUPPLY_CHARGER_TYPE_NONE)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	ret_fault = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (ret_fault < 0) {
		dev_warn(&chip->client->dev,
			"read reg failed %s\n", __func__);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	/* Check if the error VIN condition occured */
	ret_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (ret_status < 0) {
		dev_warn(&chip->client->dev,
			"read reg failed %s\n", __func__);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (!(ret_status & SYSTEM_STAT_PWR_GOOD) &&
	((ret_fault & FAULT_STAT_CHRG_BITS) == FAULT_STAT_CHRG_IN_FLT))
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	if (!(ret_status & SYSTEM_STAT_PWR_GOOD) &&
	((ret_status & SYSTEM_STAT_VBUS_BITS) == SYSTEM_STAT_VBUS_UNKNOWN))
		return POWER_SUPPLY_HEALTH_DEAD;
	return POWER_SUPPLY_HEALTH_GOOD;
}

/**
 * bq2589x_get_battery_health - to get the battery health status
 *
 * Returns battery health status
 */
int bq2589x_get_battery_health(void)
{
	int  temp, vnow;
	struct bq2589x_chip *chip;
	if (!bq2589x_client)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	chip = i2c_get_clientdata(bq2589x_client);

	dev_info(&chip->client->dev, "+%s\n", __func__);

	/* If power supply is emulating as battery, return health as good */
	if (!chip->pdata->sfi_tabl_present)
		return POWER_SUPPLY_HEALTH_GOOD;

	/* Report the battery health w.r.t battery temperature from FG */
	temp = fg_chip_get_property(POWER_SUPPLY_PROP_TEMP);
	if (temp == -ENODEV || temp == -EINVAL) {
		dev_err(&chip->client->dev,
				"Failed to read batt profile\n");
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}

	temp /= 10;

	if ((temp <= chip->min_temp) ||
		(temp > chip->max_temp))
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	/* read the battery voltage */
	vnow = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
	if (vnow == -ENODEV || vnow == -EINVAL) {
		dev_err(&chip->client->dev, "Can't read voltage from FG\n");
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}

	/* convert voltage into millivolts */
	vnow /= 1000;
	dev_warn(&chip->client->dev, "vnow = %d\n", vnow);

	if (vnow > chip->max_cv)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	dev_dbg(&chip->client->dev, "-%s\n", __func__);
	return POWER_SUPPLY_HEALTH_GOOD;
}
EXPORT_SYMBOL(bq2589x_get_battery_health);

/***********************************************************************/

/* convert the input voltage limit value
 * into equivalent register setting.
 * Note: vindpm must be in mV.
 */
static int chrg_vindpm_to_reg(int vindpm)
{
	int reg, val;
	struct bq2589x_chip *chip;

	if (!bq2589x_client)
		return -ENODEV;

	chip = i2c_get_clientdata(bq2589x_client);

	reg = bq2589x_read_reg(chip->client, BQ2589X_REG_0D);

	if (reg < 0) {
		dev_info(&chip->client->dev, "read failed %d", reg);
		return reg;
	}

	reg &= ~BQ2589X_VINDPM_MASK;

	val = ((vindpm - BQ2589X_VINDPM_BASE)/BQ2589X_VINDPM_LSB);
	reg |= (val << BQ2589X_VINDPM_SHIFT);
	/* FORCE_VINDPM = 1, absolute vindpm
	 * FORCE_VINDPM = 0, relative vindpm */
	reg |= BQ2589X_FORCE_VINDPM;

	return reg;
}

/***********************************************************************/

/* convert the input current limit value
 * into equivalent register setting.
 * Note: ilim must be in mA.
 */
static int chrg_ilim_to_reg(int ilim)
{
	int reg, val;
	struct bq2589x_chip *chip;

	if (!bq2589x_client)
		return -ENODEV;

	chip = i2c_get_clientdata(bq2589x_client);

	reg = bq2589x_read_reg(chip->client, BQ2589X_REG_00);

	if (reg < 0) {
		dev_info(&chip->client->dev, "read failed %d", reg);
		return reg;
	}

	reg &= ~BQ2589X_IINLIM_MASK;

	val = ((ilim - BQ2589X_IINLIM_BASE)/BQ2589X_IINLIM_LSB);
	reg |= (val << BQ2589X_IINLIM_SHIFT);

	return reg;
}

/* convert the termination current value
 * into equivalent register setting
 */
static u8 chrg_iterm_to_reg(int iterm)
{
	u8 reg;

	if (iterm <= BQ2589X_CHRG_ITERM_OFFSET)
		reg = 0;
	else
		reg = ((iterm - BQ2589X_CHRG_ITERM_OFFSET) /
			BQ2589X_CHRG_CUR_LSB_TO_ITERM);
	return reg;
}

/* convert the precharge current value
 * into equivalent register setting
 */
static u8 chrg_ipre_to_reg(int iterm)
{
	u8 reg;

	if (iterm <= BQ2589X_CHRG_IPRE_OFFSET)
		reg = 0;
	else
		reg = ((iterm - BQ2589X_CHRG_IPRE_OFFSET) /
			BQ2589X_CHRG_CUR_LSB_TO_IPRE);
	return reg;
}


/* convert the charge current value
 * into equivalent register setting
 */
static u8 chrg_cur_to_reg(int cur)
{
	u8 reg;

	if (cur <= BQ2589X_CHRG_CUR_OFFSET)
		reg = 0x0;
	else
		reg = ((cur - BQ2589X_CHRG_CUR_OFFSET) /
				BQ2589X_CHRG_CUR_LSB_TO_CUR) + 1;

	return reg;
}

/* convert the charge voltage value
 * into equivalent register setting
 */
static u8 chrg_volt_to_reg(int volt)
{
	u8 reg;

	if (volt <= BQ2589X_CHRG_VOLT_OFFSET)
		reg = 0x0;
	else
		reg = (volt - BQ2589X_CHRG_VOLT_OFFSET) /
				BQ2589X_CHRG_VOLT_LSB_TO_VOLT;

	/* precharge to fast charge threshold is 3.0v */
	reg = (reg << 2) | CHRG_VOLT_CNTL_BATTLOWV;
	return reg;
}


static int bq2589x_enable_hvdcp(struct bq2589x_chip *chip, bool hw_hvdcp_en)
{
	int ret = 0, is_hvdcp_enabled;

	/* Read Reg02 register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_02);
	if (ret < 0)
		dev_err(&chip->client->dev, "Reg 02 read failed\n");

	is_hvdcp_enabled = !!(ret & CHRG_HVDCP_EN);
	if (hw_hvdcp_en == is_hvdcp_enabled)
		return 0;
	else if (!is_hvdcp_enabled && hw_hvdcp_en)
		ret |= CHRG_HVDCP_EN;
	else
		ret &= (~CHRG_HVDCP_EN);

	ret = bq2589x_write_reg(chip->client,
				BQ2589X_REG_02,	ret);

	if (ret < 0)
		dev_err(&chip->client->dev, "Reg 02 write failed\n");

	return ret;
}

static int bq2589x_force_dpdm(struct bq2589x_chip *chip)
{
	int ret = 0;

	/* Read Reg02 register */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_02);
	if (ret < 0)
		dev_err(&chip->client->dev, "Reg 02 read failed\n");

	ret |= CHRG_FORCE_DP_DM;

	ret = bq2589x_write_reg(chip->client,
				BQ2589X_REG_02,	ret);

	if (ret < 0)
		dev_err(&chip->client->dev, "Reg 02 write failed\n");

	return ret;
}

static int bq2589x_set_vindpmos(struct bq2589x_chip *chip, int vindpm)
{
	int ret = 0, reg;

	pr_info("%s: vindpm %d\n", __func__, vindpm);
	reg = bq2589x_read_reg(chip->client, BQ2589X_REG_01);
	if (reg < 0)
		dev_err(&chip->client->dev, "Reg 01 read failed\n");

	pr_info("%s: reg01 0x%x\n", __func__, reg);
	if ((reg&BQ2589X_VINDPMOS_MASK) == vindpm/BQ2589X_VINDPMOS_LSB)
		return ret;

	reg &= ~BQ2589X_VINDPM_MASK;
	reg |= (vindpm/BQ2589X_VINDPMOS_LSB);
	pr_info("%s: reg01 0x%x\n", __func__, reg);
	ret = bq2589x_write_reg(chip->client,
		BQ2589X_REG_01, reg);
	if (ret < 0) {
		dev_info(&chip->client->dev, "VINDPM failed\n");
		return ret;
	}

	return ret;
}

int bq2589x_disable_watchdog_timer(struct bq2589x_chip *chip)
{
	int ret = bq2589x_mask_write(chip->client,
			BQ2589X_REG_07, BQ2589X_WDT_MASK,
			BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT);
	if (ret < 0)
		pr_err("fail to disable charger wdt");
}
int bq2589x_enable_watchdog_timer(struct bq2589x_chip *chip)
{
	int ret = bq2589x_mask_write(chip->client,
			BQ2589X_REG_07, BQ2589X_WDT_MASK,
			BQ2589X_WDT_160S << BQ2589X_WDT_SHIFT);
	if (ret < 0)
		pr_err("fail to enable charger wdt");
}

/*
 *This function will modify the absolute VINDPM threshold
 */
static int bq2589x_modify_vindpm(int vindpm)
{
	int ret, regval;
	u8 vindpm_prev, val;
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);

	dev_info(&chip->client->dev, "%s\n", __func__);

	/* Set the input source voltage limit
	 * between BQ2589X_VINDPM_MIN to BQ2589X_VINDpM_MAX */
	if (vindpm <= BQ2589X_VINDPM_MIN)
		vindpm = BQ2589X_VINDPM_MIN;

	if (vindpm >= BQ2589X_VINDPM_MAX)
		vindpm = BQ2589X_VINDPM_MAX;

	regval = chrg_vindpm_to_reg(vindpm);

	if (regval < 0) {
		dev_warn(&chip->client->dev, "INPUT CTRL VINDPM reg read failed\n");
		return ret;
	}

	/* Get the input src ctrl values programmed */
	ret = bq2589x_read_reg(chip->client,
				BQ2589X_REG_0D);

	if (ret < 0) {
		dev_warn(&chip->client->dev, "INPUT CTRL reg read failed\n");
		return ret;
	}

	/* Assign the return value of REG0D to vindpm_prev */
	vindpm_prev = (ret & BQ2589X_VINDPM_MASK);
	val = (regval & BQ2589X_VINDPM_MASK);

	/*
	 * If both the previous and current values are same do not program
	 * the register.
	*/
	if (vindpm_prev != val) {
		ret = bq2589x_write_reg(chip->client,
					BQ2589X_REG_0D, regval);

		if (ret < 0) {
			dev_info(&chip->client->dev, "VINDPM failed\n");
			return ret;
		}
	}
	return ret;
}

/* This function should be called with the mutex held */
static int bq2589x_turn_otg_vbus(struct bq2589x_chip *chip, bool votg_on)
{
	int ret = 0;

	dev_info(&chip->client->dev, "%s %d\n", __func__, votg_on);

	if (votg_on && chip->a_bus_enable) {
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"TIMER enable failed %s\n", __func__);
				goto i2c_write_fail;
			}
			bq2589x_enable_watchdog_timer(chip);
			alarm_start_relative(&chip->alarm,
				ns_to_ktime(FEED_WDT_INTERVAL_NS));
			/* Configure the charger in OTG mode */
			ret = bq2589x_reg_multi_bitset(chip->client,
					BQ2589X_REG_03,
					POWER_ON_CFG_CHRG_CFG_OTG,
					CHR_CFG_BIT_POS,
					CHR_CFG_BIT_LEN);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"i2c reg write failed: reg: %d, ret: %d\n",
					BQ2589X_REG_03, ret);
				goto i2c_write_fail;
			}

			/* Put the charger IC in reverse boost mode. Since
			 * SDP charger can supply max 500mA charging current
			 * Setting the boost current to 500mA
			 */
			ret = bq2589x_reg_multi_bitset(chip->client,
					BQ2589X_REG_0A,
					BOOST_CURRENT_LIM_500_MA,
					BOOST_CURR_LIM_POS,
					BOOST_CURR_LIM_LEN);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"i2c reg write failed: reg: %d, ret: %d\n",
					BQ2589X_REG_0A, ret);
				goto i2c_write_fail;
			}
			chip->boost_mode = true;
	} else {
			/* Clear the charger from the OTG mode */
			ret = bq2589x_reg_multi_bitset(chip->client,
					BQ2589X_REG_03,
					POWER_ON_CFG_CHRG_CFG_EN,
					CHR_CFG_BIT_POS,
					CHR_CFG_BIT_LEN);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"i2c reg write failed: reg: %d, ret: %d\n",
					BQ2589X_REG_03, ret);
				goto i2c_write_fail;
			}

			/* Put the charger IC out of reverse boost mode 500mA */
			ret = bq2589x_reg_multi_bitset(chip->client,
					BQ2589X_REG_0A,
					BOOST_CURRENT_LIM_1300_MA,
					BOOST_CURR_LIM_POS,
					BOOST_CURR_LIM_LEN);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"i2c reg write failed: reg: %d, ret: %d\n",
					BQ2589X_REG_0A, ret);
				goto i2c_write_fail;
			}
			chip->boost_mode = false;
			alarm_try_to_cancel(&chip->alarm);
			bq2589x_disable_watchdog_timer(chip);
	}

	/*
	 *  Drive the gpio to turn ON/OFF the VBUS
	 */
	if (chip->pdata->drive_vbus)
		chip->pdata->drive_vbus(votg_on && chip->a_bus_enable);

	return ret;
i2c_write_fail:
	dev_err(&chip->client->dev, "%s: Failed\n", __func__);
	return ret;
}

int bq2589x_vbus_enable(void)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	return bq2589x_turn_otg_vbus(chip, true);
}
EXPORT_SYMBOL(bq2589x_vbus_enable);

int bq2589x_vbus_disable(void)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	return bq2589x_turn_otg_vbus(chip, false);
}
EXPORT_SYMBOL(bq2589x_vbus_disable);

#ifdef CONFIG_DEBUG_FS
#define DBGFS_REG_BUF_LEN	3

static int bq2589x_show(struct seq_file *seq, void *unused)
{
	u16 val;
	long addr;

	if (kstrtol((char *)seq->private, 16, &addr))
		return -EINVAL;

	val = bq2589x_read_reg(bq2589x_client, addr);
	seq_printf(seq, "%x\n", val);

	return 0;
}

static int bq2589x_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq2589x_show, inode->i_private);
}

static ssize_t bq2589x_dbgfs_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[DBGFS_REG_BUF_LEN];
	long addr;
	unsigned long value;
	int ret;
	struct seq_file *seq = file->private_data;

	if (!seq || kstrtol((char *)seq->private, 16, &addr))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, DBGFS_REG_BUF_LEN-1))
		return -EFAULT;

	buf[DBGFS_REG_BUF_LEN-1] = '\0';
	if (kstrtoul(buf, 16, &value))
		return -EINVAL;

	dev_info(&bq2589x_client->dev,
			"[dbgfs write] Addr:0x%x Val:0x%x\n",
			(u32)addr, (u32)value);


	ret = bq2589x_write_reg(bq2589x_client, addr, value);
	if (ret < 0)
		dev_warn(&bq2589x_client->dev, "I2C write failed\n");

	return count;
}

static const struct file_operations bq2589x_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= bq2589x_dbgfs_open,
	.read		= seq_read,
	.write		= bq2589x_dbgfs_reg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int bq2589x_create_debugfs(struct bq2589x_chip *chip)
{
	int i;
	struct dentry *entry;

	bq2589x_dbgfs_root = debugfs_create_dir(DEV_NAME, NULL);
	if (IS_ERR(bq2589x_dbgfs_root)) {
		dev_warn(&chip->client->dev, "DEBUGFS DIR create failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < BQ2589X_MAX_MEM; i++) {
		sprintf((char *)&bq2589x_dbg_regs[i], "%x", i);
		entry = debugfs_create_file(
					(const char *)&bq2589x_dbg_regs[i],
					S_IRUGO,
					bq2589x_dbgfs_root,
					&bq2589x_dbg_regs[i],
					&bq2589x_dbgfs_fops);
		if (IS_ERR(entry)) {
			debugfs_remove_recursive(bq2589x_dbgfs_root);
			bq2589x_dbgfs_root = NULL;
			dev_warn(&chip->client->dev,
					"DEBUGFS entry Create failed\n");
			return -ENOMEM;
		}
	}

	return 0;
}
static inline void bq2589x_remove_debugfs(struct bq2589x_chip *chip)
{
	debugfs_remove_recursive(bq2589x_dbgfs_root);
}
#else
static inline int bq2589x_create_debugfs(struct bq2589x_chip *chip)
{
	return 0;
}
static inline void bq2589x_remove_debugfs(struct bq2589x_chip *chip)
{
}
#endif

static inline int bq2589x_enable_charging(struct bq2589x_chip *chip)
{
	int ret, regval;

	dev_warn(&chip->client->dev, "%s\n", __func__);

	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_03);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"pwr cfg read failed: %d\n", ret);
		return ret;
	}

	/* clear the charge enbale bit mask first */
	ret &= ~(CHR_CFG_CHRG_MASK << CHR_CFG_BIT_POS);
	regval = ret | POWER_ON_CFG_CHRG_CFG_EN;
	ret = bq2589x_write_reg(chip->client, BQ2589X_REG_03, regval);
	if (ret < 0) {
		chip->is_charging_enabled = 0;
		dev_warn(&chip->client->dev, "charger enable/disable failed\n");
	} else
		chip->is_charging_enabled = 1;

	return ret;
}

static inline int bq2589x_set_cc(struct bq2589x_chip *chip, int cc)
{
	u8 regval;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, cc);

	/* chip->max_cc is from throttle case */
	if (cc > chip->max_cc)
		cc = chip->max_cc;

	regval = chrg_cur_to_reg(cc);

	return bq2589x_write_reg(chip->client, BQ2589X_REG_04,
				regval);
}

static inline int bq2589x_set_cv(struct bq2589x_chip *chip, int cv)
{
	u8 regval;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, cv);
	regval = chrg_volt_to_reg(cv);
	/* battery recharge threshold offset is 100mv */
	return bq2589x_write_reg(chip->client, BQ2589X_REG_06,
					regval & ~CHRG_VOLT_CNTL_VRECHRG);
}

static inline int bq2589x_set_inlmt(struct bq2589x_chip *chip, int inlmt)
{
	int regval, timeout;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, inlmt);

	/* Set the input source current limit
	 * between BQ2589X_IINLIM_MIN to BQ2589X_IINLIM_MAX */
	if (inlmt <= BQ2589X_IINLIM_MIN)
		inlmt = BQ2589X_IINLIM_MIN;

	if (inlmt >= BQ2589X_IINLIM_MAX)
		inlmt = BQ2589X_IINLIM_MAX;

	chip->inlmt = inlmt;
	regval = chrg_ilim_to_reg(inlmt);

	if (regval < 0)
		return regval;

	/* Wait for VBUS if inlimit > 0 */
	if (inlmt > 0) {
		timeout = wait_for_completion_timeout(&chip->vbus_detect,
					VBUS_DET_TIMEOUT);
		if (timeout == 0)
			dev_warn(&chip->client->dev,
				"VBUS Detect timedout. Setting INLIMIT");
	}

	return bq2589x_write_reg(chip->client, BQ2589X_REG_00,
				regval);
}

static inline int bq2589x_set_iterm(struct bq2589x_chip *chip, int iterm)
{
	u8 reg_val;

	if (iterm > BQ2589X_CHRG_ITERM_MAX)
		iterm = BQ2589X_CHRG_ITERM_MAX;

	reg_val = chrg_iterm_to_reg(iterm);

	return bq2589x_mask_write(chip->client,
			BQ2589X_REG_05, BQ2589X_ITERM_MASK, reg_val);
}

static inline int bq2589x_set_ipre(struct bq2589x_chip *chip, int ipre)
{
	u8 reg_val;

	if (ipre > BQ2589X_CHRG_IPRE_MAX)
		ipre = BQ2589X_CHRG_IPRE_MAX;

	reg_val = chrg_ipre_to_reg(ipre);

	return bq2589x_mask_write(chip->client, BQ2589X_REG_05,
			BQ2589X_IPRECHG_MASK, reg_val << BQ2589X_IPRECHG_SHIFT);
}

static enum bq2589x_chrgr_stat bq2589x_is_charging(struct bq2589x_chip *chip)
{
	int ret;
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (ret < 0)
		dev_err(&chip->client->dev, "STATUS register read failed\n");
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (ret < 0)
		dev_err(&chip->client->dev, "STATUS register read failed\n");

	ret &= SYSTEM_STAT_CHRG_MASK;

	switch (ret) {
	case SYSTEM_STAT_NOT_CHRG:
		chip->chgr_stat = BQ2589X_CHRGR_STAT_FAULT;
		break;
	case SYSTEM_STAT_CHRG_DONE:
		chip->chgr_stat = BQ2589X_CHRGR_STAT_BAT_FULL;
		break;
	case SYSTEM_STAT_PRE_CHRG:
	case SYSTEM_STAT_FAST_CHRG:
		chip->chgr_stat = BQ2589X_CHRGR_STAT_CHARGING;
		break;
	default:
		break;
	}

	return chip->chgr_stat;
}

static int bq2589x_usb_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct bq2589x_chip *chip = container_of(psy,
						struct bq2589x_chip,
						usb);
	int ret = 0, inlimit;

	dev_dbg(&chip->client->dev, "%s %d\n", __func__, psp);
	if (mutex_is_locked(&chip->event_lock)) {
		dev_dbg(&chip->client->dev,
			"%s: mutex is already acquired",
				__func__);
	}
	mutex_lock(&chip->event_lock);

	switch (psp) {

	case POWER_SUPPLY_PROP_PRESENT:
		chip->present = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		chip->online = val->intval;
		pr_info("POWER_SUPPLY_PROP_ONLINE %d\n", chip->online);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		chip->usb.type = val->intval;
		pr_info("POWER_SUPPLY_PROP_TYPE %d\n", chip->usb.type);
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:
		if (!val->intval) {
			cancel_delayed_work_sync(&chip->chrg_full_wrkr);
			inlimit = 0;
		} else
			inlimit = DC_CHARGE_CUR_DCP;

		bq2589x_set_inlmt(chip, inlimit);
		chip->is_charging_enabled = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CURRENT:
		ret = bq2589x_set_cc(chip, val->intval);
		if (!ret)
			chip->cc = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_VOLTAGE:
		ret = bq2589x_set_cv(chip, val->intval);
		if (!ret)
			chip->cv = val->intval;
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT:
		chip->max_cc = val->intval;
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE:
		chip->max_cv = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CUR:
		ret = bq2589x_set_iterm(chip, val->intval);
		if (!ret)
			chip->iterm = val->intval;
		break;
	case POWER_SUPPLY_PROP_CABLE_TYPE:
		chip->cable_type = val->intval;
		chip->usb.type = get_power_supply_type(chip->cable_type);
		pr_info("POWER_SUPPLY_PROP_TYPE %d\n", chip->usb.type);
		break;
	case POWER_SUPPLY_PROP_INLMT:
		ret = bq2589x_set_inlmt(chip, val->intval);
		if (!ret)
			chip->inlmt = val->intval;
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		chip->max_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_MIN_TEMP:
		chip->min_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		chip->cntl_state_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (val->intval < chip->cntl_state_max)
			chip->cntl_state = val->intval;
		else
			ret = -EINVAL;
		break;
	case POWER_SUPPLY_PROP_ENABLE_HVDCP:
		if (val->intval)
			bq2589x_enable_hvdcp(chip, true);
		else
			bq2589x_enable_hvdcp(chip, false);
		break;
	case POWER_SUPPLY_PROP_SET_VINDPM:
		bq2589x_set_vindpmos(chip, val->intval);
		break;
	default:
		ret = -ENODATA;
	}

	mutex_unlock(&chip->event_lock);
	return ret;
}

static int bq2589x_usb_property_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static int is_vbus_good(struct bq2589x_chip *chip)
{
	int reg_vbus = 0;

	reg_vbus = bq2589x_read_reg(chip->client, BQ2589X_REG_11);
	if (reg_vbus < 0)
		dev_err(&chip->client->dev, "VBUS register read failed:\n");

	dev_dbg(&chip->client->dev, "VBUS reg 1st %x\n", reg_vbus);
	/* try to read twice, avoid read value is NULL, recommended by vendor  */
	reg_vbus = bq2589x_read_reg(chip->client, BQ2589X_REG_11);
	if (reg_vbus < 0)
		dev_err(&chip->client->dev, "VBUS register read failed:\n");

	dev_dbg(&chip->client->dev, "VBUS reg 2nd %x\n", reg_vbus);
	reg_vbus = reg_vbus & BQ25890_VBUS_GOOD;

	return reg_vbus;
}

static int bq2589x_usb_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct bq2589x_chip *chip = container_of(psy,
					struct bq2589x_chip,
					usb);
	enum bq2589x_chrgr_stat charging;

	dev_dbg(&chip->client->dev, "%s %d\n", __func__, psp);
	mutex_lock(&chip->event_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = chip->usb.type;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq2589x_get_charger_health();
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_CURRENT:
		val->intval = chip->max_cc;
		break;
	case POWER_SUPPLY_PROP_MAX_CHARGE_VOLTAGE:
		val->intval = chip->max_cv;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CURRENT:
		val->intval = chip->cc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_VOLTAGE:
		val->intval = chip->cv;
		break;
	case POWER_SUPPLY_PROP_INLMT:
		val->intval = chip->inlmt;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CUR:
		val->intval = chip->iterm;
		break;
	case POWER_SUPPLY_PROP_CABLE_TYPE:
		val->intval = chip->cable_type;
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:
		if (chip->boost_mode)
			val->intval = false;
		else
			val->intval = chip->is_charging_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = chip->cntl_state_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = chip->cntl_state;
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		val->intval = chip->max_temp;
		break;
	case POWER_SUPPLY_PROP_MIN_TEMP:
		val->intval = chip->min_temp;
		break;
	case POWER_SUPPLY_PROP_VBUS_GOOD:
		val->intval = is_vbus_good(chip);
		break;
	case POWER_SUPPLY_PROP_ENABLE_HVDCP:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_SET_VINDPM:
		val->intval = -ENODATA;
		break;
	default:
		mutex_unlock(&chip->event_lock);
		return -EINVAL;
	}

	mutex_unlock(&chip->event_lock);
	return 0;
}

static void bq2589x_full_worker(struct work_struct *work)
{
	struct bq2589x_chip *chip = container_of(work,
						    struct bq2589x_chip,
						    chrg_full_wrkr.work);
	power_supply_changed(NULL);

	/* schedule the thread to let the framework know about FULL */
	schedule_delayed_work(&chip->chrg_full_wrkr, FULL_THREAD_JIFFIES);
}

static int handle_chrg_det(struct bq2589x_chip *chip)
{
	int reg_status, reg_vbus;
	static bool notify_otg, notify_charger;
	bool vbus_attach = false;
	static struct power_supply_cable_props cable_props;
	int vbus_mask = 0;
	enum bq2589x_support_special_chgr_type {
		CHARGER_TYPE_USB_UNKOWN,
		CHARGER_TYPE_USB_HVDCP,
		CHARGER_TYPE_USB_UNK_ADP,
		CHARGER_TYPE_USB_UNK_STD_ADP,
	} special_chgr_type;

	special_chgr_type = CHARGER_TYPE_USB_UNKOWN;


	reg_vbus = bq2589x_read_reg(chip->client, BQ2589X_REG_11);
	if (reg_vbus < 0)
		dev_err(&chip->client->dev, "VBUS register read failed:\n");

	dev_dbg(&chip->client->dev, "VBUS reg 1st %x\n", reg_vbus);
	/* try to read twice, avoid read value is NULL, recommended by vendor  */
	reg_vbus = bq2589x_read_reg(chip->client, BQ2589X_REG_11);
	if (reg_vbus < 0)
		dev_err(&chip->client->dev, "VBUS register read failed:\n");

	dev_dbg(&chip->client->dev, "VBUS reg 2nd %x\n", reg_vbus);
	reg_vbus = reg_vbus & BQ25890_VBUS_GOOD;
	if (reg_vbus)
		bq2589x_enable_hvdcp(chip, true);

	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	dev_dbg(&chip->client->dev, "STATUS reg 1st %x\n", reg_status);
	/* try to read twice, avoid read value is NULL, recommended by vendor  */
	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	dev_dbg(&chip->client->dev, "STATUS reg 2nd %x\n", reg_status);
	reg_status = reg_status & SYSTEM_STAT_VBUS_BITS;

	if (reg_status == SYSTEM_STAT_VBUS_SDP) {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB;
	} else if (reg_status == SYSTEM_STAT_VBUS_DCP) {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
	} else if (reg_status == SYSTEM_STAT_VBUS_HVDCP) {
		bq2589x_set_inlmt(chip, 1000);
		/* disable hvdcp */
		bq2589x_enable_hvdcp(chip, false);
		special_chgr_type = CHARGER_TYPE_USB_HVDCP;
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
	} else if (reg_status == SYSTEM_STAT_VBUS_UNK_ADP) {
		special_chgr_type = CHARGER_TYPE_USB_UNK_ADP;
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
	} else if (reg_status == SYSTEM_STAT_VBUS_UNK_STD_ADP) {
		special_chgr_type = CHARGER_TYPE_USB_UNK_STD_ADP;
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
	} else if (reg_status == SYSTEM_STAT_VBUS_CDP) {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_CDP;
	} else if (reg_status == SYSTEM_STAT_VBUS_OTG) {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_NONE;
		chip->usb.type = POWER_SUPPLY_TYPE_UNKNOWN;
	} else if (reg_vbus) {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_DCP;
	} else {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_NONE;
		chip->usb.type = POWER_SUPPLY_TYPE_USB;
	}

	if (chip->cable_type == POWER_SUPPLY_CHARGER_TYPE_USB_SDP) {
		dev_info(&chip->client->dev, "SDP cable connecetd\n");
		vbus_attach = true;
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		cable_props.ma = DC_CHARGE_CUR_SDP;
		bq2589x_enable_hvdcp(chip, false);
	} else if (chip->cable_type == POWER_SUPPLY_CHARGER_TYPE_USB_CDP) {
		dev_info(&chip->client->dev, "CDP cable connecetd\n");
		vbus_attach = true;
		notify_otg = true;
		vbus_mask = 1;
		notify_charger = true;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		cable_props.ma = DC_CHARGE_CUR_CDP;
		bq2589x_enable_hvdcp(chip, false);
		schedule_delayed_work(&chip->cdp_wrkr, 5 * HZ);
	} else if (chip->cable_type == POWER_SUPPLY_CHARGER_TYPE_USB_DCP) {
		dev_info(&chip->client->dev, "DCP cable connecetd\n");
		vbus_attach = true;
		notify_charger = true;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;

		if (special_chgr_type == CHARGER_TYPE_USB_UNK_STD_ADP)
			cable_props.ma = DC_CHARGE_CUR_SDP;
		else if (special_chgr_type == CHARGER_TYPE_USB_UNK_ADP)
			cable_props.ma = DC_CHARGE_CUR_SDP;
		else if (special_chgr_type == CHARGER_TYPE_USB_HVDCP)
			cable_props.ma = DC_CHARGE_CUR_HVDCP;
		else
			cable_props.ma = DC_CHARGE_CUR_DCP;
		schedule_delayed_work(&chip->hvdcp_wrkr, 5 * HZ);
	} else {
		dev_info(&chip->client->dev, "disconnect or unknown or ID event\n");
		vbus_attach = false;
		cable_props.ma = 0;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
	}

	if (!vbus_attach) {	/* disconnevt event */
		if (notify_otg) {
			atomic_notifier_call_chain(&chip->transceiver->notifier,
				vbus_mask ? USB_EVENT_VBUS : USB_EVENT_NONE,
				NULL);
			notify_otg = false;
		}
		if (notify_charger) {
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
			notify_charger = false;
		}
	} else {
		if (notify_otg) {
			/*
			 * TODO:close mux path to switch
			 * b/w device mode and host mode.
			 */
			atomic_notifier_call_chain(&chip->transceiver->notifier,
				vbus_mask ? USB_EVENT_VBUS : USB_EVENT_NONE,
				NULL);
		}
		if (notify_charger)
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);

	}

	wake_lock_timeout(&chip->wl, HZ * 1.5);

	return 0;
}

static void bq2589x_cdp_worker(struct work_struct *work)
{
	int reg_status, reg_fault;
	struct bq2589x_chip *chip = container_of(work,
						    struct bq2589x_chip,
						    cdp_wrkr.work);

	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	dev_dbg(&chip->client->dev, "STATUS reg 2nd %x\n", reg_status);
	reg_status = reg_status & SYSTEM_STAT_VBUS_BITS;


	if (reg_status == SYSTEM_STAT_VBUS_CDP) {
		chip->cable_type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
		chip->usb.type = POWER_SUPPLY_TYPE_USB_CDP;
		bq2589x_enable_hiz(chip);
		bq2589x_clear_hiz(chip);
	}
}

static void bq2589x_hvdcp_worker(struct work_struct *work)
{
	int reg_status, reg_fault;
	struct bq2589x_chip *chip = container_of(work,
						    struct bq2589x_chip,
						    hvdcp_wrkr.work);

	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	dev_dbg(&chip->client->dev, "STATUS reg 2nd %x\n", reg_status);
	reg_status = reg_status & SYSTEM_STAT_VBUS_BITS;


	if (reg_status == SYSTEM_STAT_VBUS_DCP) {
		bq2589x_enable_hvdcp(chip, false);
	}
}



static void bq2589x_irq_worker(struct work_struct *work)
{
	int reg_status, reg_fault;
	struct bq2589x_chip *chip = container_of(work,
						    struct bq2589x_chip,
						    irq_wrkr.work);
	if (chip->chip_type == BQ25890 || chip->chip_type == BQ25895)
		handle_chrg_det(chip);
	/*
	 * check the bq2589x status/fault registers to see what is the
	 * source of the interrupt
	 */
	reg_status = bq2589x_read_reg(chip->client, BQ2589X_REG_0B);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	dev_info(&chip->client->dev, "STATUS reg 3rd %x\n", reg_status);

	/*
	 * On VBUS detect set completion to wake waiting thread. On VBUS
	 * disconnect, re-init completion so that setting INLIMIT would be
	 * delayed till VBUS is detected.
	 */
	if (reg_status & SYSTEM_STAT_VBUS_BITS)
		complete(&chip->vbus_detect);
	else
		reinit_completion(&chip->vbus_detect);

	reg_status &= SYSTEM_STAT_CHRG_DONE;

	if (reg_status == SYSTEM_STAT_CHRG_DONE) {
		dev_warn(&chip->client->dev, "HW termination happened\n");
		/* schedule the thread to let the framework know about FULL */
		schedule_delayed_work(&chip->chrg_full_wrkr, 0);
	}

	/* Check if battery fault condition occured. Reading the register
	   value two times to get reliable reg value, recommended by vendor*/
	reg_fault = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (reg_fault < 0)
		dev_err(&chip->client->dev, "FAULT register read failed:\n");

	reg_fault = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (reg_fault < 0)
		dev_err(&chip->client->dev, "FAULT register read failed:\n");

	dev_info(&chip->client->dev, "FAULT reg %x\n", reg_fault);
	if (reg_fault & FAULT_STAT_WDT_TMR_EXP) {
		dev_warn(&chip->client->dev, "WDT expiration fault\n");
	}
	if ((reg_fault & FAULT_STAT_CHRG_TMR_FLT)
			== FAULT_STAT_CHRG_TMR_FLT) {
		dev_err(&chip->client->dev, "Safety timer expired\n");
	}
	if (reg_status != SYSTEM_STAT_CHRG_DONE)
		power_supply_changed(&chip->usb);

	if (reg_fault & FAULT_STAT_BATT_TEMP_BITS) {
		dev_err(&chip->client->dev,
			"%s:Battery over temp occured!!!!\n", __func__);
		schedule_delayed_work(&chip->chrg_temp_wrkr, 0);
	}
}


#define CHARGING_PERIOD_MS 500
#define NOT_CHARGING_PERIOD_MS 5000
static void bq2589x_reg_worker(struct work_struct *work)
{
	struct bq2589x_chip *chip = container_of(work,
						    struct bq2589x_chip,
						    reg_wrkr.work);

	bq2589x_dump_registers(chip);
	if (chip->online)
		schedule_delayed_work(&chip->reg_wrkr, CHARGING_PERIOD_MS * HZ);
	else
		schedule_delayed_work(&chip->reg_wrkr, NOT_CHARGING_PERIOD_MS * HZ);
}

/* IRQ handler for charger Interrupts configured to GPIO pin */
static irqreturn_t bq2589x_irq_isr(int irq, void *devid)
{
	struct bq2589x_chip *chip = (struct bq2589x_chip *)devid;

	/**TODO: This hanlder will be used for charger Interrupts */
	dev_dbg(&chip->client->dev,
		"IRQ Handled for charger interrupt: %d\n", irq);

	return IRQ_WAKE_THREAD;
}

/* IRQ handler for charger Interrupts configured to GPIO pin */
static irqreturn_t bq2589x_irq_thread(int irq, void *devid)
{
	struct bq2589x_chip *chip = (struct bq2589x_chip *)devid;

	dev_err(&chip->client->dev, "bq2589x_irq_thread occur!!!!\n");
	schedule_delayed_work(&chip->irq_wrkr, 0);
	return IRQ_HANDLED;
}


static void bq2589x_temp_update_worker(struct work_struct *work)
{
	struct bq2589x_chip *chip =
	    container_of(work, struct bq2589x_chip, chrg_temp_wrkr.work);
	int fault_reg = 0, fg_temp = 0;
	static bool is_otp_notified;

	dev_info(&chip->client->dev, "%s\n", __func__);
	/* Check if battery fault condition occured. Reading the register
	   value two times to get reliable reg value, recommended by vendor*/
	fault_reg = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (fault_reg < 0) {
		dev_err(&chip->client->dev,
			"Fault status read failed: %d\n", fault_reg);
		goto temp_wrkr_error;
	}
	fault_reg = bq2589x_read_reg(chip->client, BQ2589X_REG_0C);
	if (fault_reg < 0) {
		dev_err(&chip->client->dev,
			"Fault status read failed: %d\n", fault_reg);
		goto temp_wrkr_error;
	}

	fg_temp = fg_chip_get_property(POWER_SUPPLY_PROP_TEMP);
	if (fg_temp == -ENODEV || fg_temp == -EINVAL) {
		dev_err(&chip->client->dev,
			"Failed to read FG temperature\n");
		/* If failed to read fg temperature, use charger fault
		 * status to identify the recovery */
		if (fault_reg & FAULT_STAT_BATT_TEMP_BITS) {
			schedule_delayed_work(&chip->chrg_temp_wrkr,
				TEMP_THREAD_JIFFIES);
		} else {
			power_supply_changed(&chip->usb);
		}
		goto temp_wrkr_error;
	}
	fg_temp = fg_temp/10;

	if (fg_temp >= chip->max_temp
		|| fg_temp <= chip->min_temp) {
		if (!is_otp_notified) {
			dev_info(&chip->client->dev,
				"Battery over temp occurred!!!!\n");
			power_supply_changed(&chip->usb);
			is_otp_notified = true;
		}
	} else if (!(fault_reg & FAULT_STAT_BATT_TEMP_BITS)) {
		/* over temperature is recovered if battery temp
		 * is between min_temp to max_temp and charger
		 * temperature fault bits are cleared */
		is_otp_notified = false;
		dev_info(&chip->client->dev,
			"Battery over temp recovered!!!!\n");
		power_supply_changed(&chip->usb);
		/*Return without reschedule as over temp recovered*/
		return;
	}
	schedule_delayed_work(&chip->chrg_temp_wrkr, TEMP_THREAD_JIFFIES);
	return;

temp_wrkr_error:
	is_otp_notified = false;
	return;
}

static int bq2589x_usb_otg_enable(struct usb_phy *phy, int on)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	int ret;

	mutex_lock(&chip->event_lock);
	ret = bq2589x_turn_otg_vbus(chip, on);
	mutex_unlock(&chip->event_lock);
	if (ret < 0) {
		dev_err(&chip->client->dev, "VBUS mode(%d) failed\n", on);
		return ret;
	}
	return ret;
}

static inline int register_otg_vbus(struct bq2589x_chip *chip)
{

	chip->transceiver = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!chip->transceiver) {
		dev_err(&chip->client->dev, "Failed to get the USB transceiver\n");
		return -EINVAL;
	}
	chip->transceiver->set_vbus = bq2589x_usb_otg_enable;

	return 0;
}

int bq2589x_slave_mode_enable_charging(int volt, int cur, int ilim)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	int ret;

	mutex_lock(&chip->event_lock);
	chip->inlmt = ilim;
	if (chip->inlmt >= 0)
		bq2589x_set_inlmt(chip, chip->inlmt);
	mutex_unlock(&chip->event_lock);

	chip->cc = chrg_cur_to_reg(cur);
	if (chip->cc)
		bq2589x_set_cc(chip, chip->cc);

	chip->cv = chrg_volt_to_reg(volt);
	if (chip->cv)
		bq2589x_set_cv(chip, chip->cv);

	mutex_lock(&chip->event_lock);
	ret = bq2589x_enable_charging(chip);
	if (ret < 0)
		dev_err(&chip->client->dev, "charge enable failed\n");

	mutex_unlock(&chip->event_lock);
	return ret;
}
EXPORT_SYMBOL(bq2589x_slave_mode_enable_charging);

int bq2589x_slave_mode_disable_charging(void)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(bq2589x_client);
	int ret;

	mutex_lock(&chip->event_lock);
	ret = bq2589x_enable_charging(chip);
	if (ret < 0)
		dev_err(&chip->client->dev, "charge enable failed\n");

	mutex_unlock(&chip->event_lock);
	return ret;
}
static int bq2589x_get_chip_version(struct bq2589x_chip *chip)
{
	int ret;

	/* check chip model number */
	ret = bq2589x_read_reg(chip->client, BQ2589X_REG_14);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c read err:%d\n", ret);
		return -EIO;
	}
	dev_info(&chip->client->dev, "version reg:%x\n", ret);

	ret = (ret & BQ2589X_VERSION_MASK) >> 3;
	switch (ret) {
	case BQ25890_IC_VERSION:
		chip->chip_type = BQ25890;
		break;
	case BQ25892_IC_VERSION:
		chip->chip_type = BQ25892;
		break;
	case BQ25895_IC_VERSION:
		chip->chip_type = BQ25895;
		break;
	default:
		dev_err(&chip->client->dev,
			"device version mismatch: %x\n", ret);
		return -EIO;
	}

	dev_info(&chip->client->dev, "chip type:%x\n", chip->chip_type);
	return 0;
}

/* configure bq2589x for otg/charger path setting */
static void bq2589x_otg_event_worker(struct work_struct *work)
{
	struct bq2589x_chip *chip =
	    container_of(work, struct bq2589x_chip, otg_work);
	int ret;

	dev_info(&chip->client->dev, "%s: id_short=%d\n", __func__,
			chip->id_short);

	mutex_lock(&chip->event_lock);
	ret = bq2589x_turn_otg_vbus(chip, !chip->id_short);
	mutex_unlock(&chip->event_lock);

	if (ret < 0)
		dev_err(&chip->client->dev, "VBUS mode(id: %d) failed\n",
				chip->id_short);
}


static bool is_usb_host_mode(struct extcon_dev *evdev)
{
	return !!evdev->state;
}

/* handle otg id event when insert/remove OTG B-Device */
static int bq2589x_handle_otg_event(struct notifier_block *nb,
				   unsigned long event, void *param)
{
	struct bq2589x_chip *chip =
	    container_of(nb, struct bq2589x_chip, id_nb);
	struct extcon_dev *edev = param;
	int usb_host = is_usb_host_mode(edev);

	dev_info(&chip->client->dev,
		"[extcon notification] evt:USB-Host val:%s\n",
		usb_host ? "Connected" : "Disconnected");

	/*
	 * in case of id short(usb_host = 1)
	 * enable vbus else disable vbus.
	 */
	chip->id_short = usb_host;
	schedule_work(&chip->otg_work);

	return NOTIFY_OK;
}

static int bq2589x_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	const struct acpi_device_id *acpi_id;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq2589x_chip *chip;
	struct device *dev;
	struct gpio_desc *gpio;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"SMBus doesn't support BYTE transactions\n");
		return -EIO;
	}

	chip = kzalloc(sizeof(struct bq2589x_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	chip->client = client;
	if (id) {
		chip->pdata = (struct bq2589x_platform_data *)id->driver_data;
	} else {
#ifdef CONFIG_ACPI
		dev = &client->dev;
		if (!ACPI_HANDLE(dev)) {
			i2c_set_clientdata(client, NULL);
			kfree(chip);
			return -ENODEV;
		}
		acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
		if (!acpi_id) {
			i2c_set_clientdata(client, NULL);
			kfree(chip);
			return -ENODEV;
		}

		chip->pdata = (struct bq2589x_platform_data *)
						acpi_id->driver_data;
#else
		chip->pdata = client->dev.platform_data;
#endif
	}
	if (!chip->pdata) {
		dev_err(&client->dev, "pdata NULL!!\n");
		kfree(chip);
		return -EINVAL;
	}
	chip->pdata->chg_profile = (struct ps_batt_chg_prof *)
				platform_byt_get_batt_charge_profile();
	chip->irq = -1;
	chip->a_bus_enable = true;

	/*assigning default value for min and max temp*/
	chip->max_temp = chip->pdata->max_temp;
	chip->min_temp = chip->pdata->min_temp;
	i2c_set_clientdata(client, chip);
	bq2589x_client = client;

	/* check chip model number */
	ret = bq2589x_get_chip_version(chip);
	if (ret < 0) {
		dev_err(&client->dev, "i2c read err:%d\n", ret);
		i2c_set_clientdata(client, NULL);
		kfree(chip);
		return -EIO;
	}

	ret = bq2589x_disable_watchdog_timer(chip);
	if (ret < 0)
		dev_warn(&chip->client->dev, "charger wdt disable failed\n");

	bq2589x_set_iterm(chip, 128);
	bq2589x_set_ipre(chip, 512);
	bq2589x_set_vindpmos(chip, 800);
	bq2589x_enable_charging(chip);
	/*
	 * Initialize the platform data
	 */
	if (chip->pdata->init_platform_data) {
		ret = chip->pdata->init_platform_data();
		if (ret < 0) {
			dev_err(&chip->client->dev,
					"FAILED: init_platform_data\n");
		}
	}

	INIT_DELAYED_WORK(&chip->chrg_full_wrkr, bq2589x_full_worker);
	INIT_DELAYED_WORK(&chip->chrg_temp_wrkr, bq2589x_temp_update_worker);
	INIT_DELAYED_WORK(&chip->irq_wrkr, bq2589x_irq_worker);
	INIT_DELAYED_WORK(&chip->cdp_wrkr, bq2589x_cdp_worker);
	INIT_DELAYED_WORK(&chip->hvdcp_wrkr, bq2589x_hvdcp_worker);
	INIT_DELAYED_WORK(&chip->reg_wrkr, bq2589x_reg_worker);
	INIT_DELAYED_WORK(&chip->feed_wdt_work, bq2589x_feed_wdt_work);
	mutex_init(&chip->event_lock);
	wake_lock_init(&chip->wl, WAKE_LOCK_SUSPEND, "bq2589x_chg_insertion");
	wake_lock_init(&chip->feed_wdt_wl, WAKE_LOCK_SUSPEND, "bq2589x_feed_wdt");
	alarm_init(&chip->alarm, ALARM_REALTIME, bq2589x_feed_wdt);

	init_completion(&chip->vbus_detect);

	/* register bq2589x usb with power supply subsystem */
	if (!chip->pdata->slave_mode) {
		chip->usb.name = CHARGER_PS_NAME;
		chip->usb.type = POWER_SUPPLY_TYPE_USB;
		chip->usb.supplied_to = chip->pdata->supplied_to;
		chip->usb.num_supplicants = chip->pdata->num_supplicants;
		chip->usb.throttle_states = chip->pdata->throttle_states;
		chip->usb.num_throttle_states =
					chip->pdata->num_throttle_states;
		chip->usb.supported_cables = chip->pdata->supported_cables;
		chip->max_cc = chip->pdata->max_cc;
		chip->max_cv = chip->pdata->max_cv;
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		chip->chgr_stat = BQ2589X_CHRGR_STAT_UNKNOWN;
		chip->usb.properties = bq2589x_usb_props;
		chip->usb.num_properties = ARRAY_SIZE(bq2589x_usb_props);
		chip->usb.get_property = bq2589x_usb_get_property;
		chip->usb.set_property = bq2589x_usb_set_property;
		chip->usb.property_is_writeable = bq2589x_usb_property_is_writeable;
		ret = power_supply_register(&client->dev, &chip->usb);
		if (ret) {
			dev_err(&client->dev, "failed:power supply register\n");
			i2c_set_clientdata(client, NULL);
			kfree(chip);
			return ret;
		}
	}
	/* Init Runtime PM State */
	pm_runtime_put_noidle(&chip->client->dev);
	pm_schedule_suspend(&chip->client->dev, MSEC_PER_SEC);

	/* create debugfs for maxim registers */
	ret = bq2589x_create_debugfs(chip);
	if (ret < 0) {
		dev_err(&client->dev, "debugfs create failed\n");
		power_supply_unregister(&chip->usb);
		i2c_set_clientdata(client, NULL);
		kfree(chip);
		return ret;
	}

	INIT_WORK(&chip->otg_work, bq2589x_otg_event_worker);

	/* Register for OTG notification */
	chip->id_nb.notifier_call = bq2589x_handle_otg_event;
	ret = extcon_register_interest(&chip->cable_obj, NULL, "USB-Host",
				       &chip->id_nb);
	if (ret)
		dev_err(&client->dev, "failed to register extcon notifier\n");

	if (chip->cable_obj.edev) {
		chip->id_short = is_usb_host_mode(chip->cable_obj.edev);
	    schedule_work(&chip->otg_work);
	}
	/*
	 * Register to get USB transceiver events
	 */
	ret = register_otg_vbus(chip);
	if (ret) {
		dev_err(&chip->client->dev,
					"REGISTER OTG VBUS failed\n");
	}

	/*
	 * Request for charger chip gpio.This will be used to
	 * register for an interrupt handler for servicing charger
	 * interrupts
	 */
	if (client->irq) {
		chip->irq = client->irq;
	} else {
#ifdef CONFIG_ACPI
		gpio = devm_gpiod_get_index(dev, "bq2589x_int", 0);
		if (IS_ERR(gpio)) {
			dev_err(dev, "acpi gpio get index failed\n");
			i2c_set_clientdata(client, NULL);
			kfree(chip);
			return PTR_ERR(gpio);
		}

		chip->irq = gpiod_to_irq(gpio);
#else
		if (chip->pdata->get_irq_number)
			chip->irq = chip->pdata->get_irq_number();
#endif
	}
	if (chip->irq < 0) {
		dev_err(&chip->client->dev,
			"chgr_int_n GPIO is not available\n");
	} else {
		ret = request_threaded_irq(chip->irq,
				bq2589x_irq_isr, bq2589x_irq_thread,
				IRQF_TRIGGER_FALLING, "BQ2589X", chip);
		if (ret) {
			dev_warn(&bq2589x_client->dev,
				"failed to register irq for pin %d\n",
				chip->irq);
		} else {
			dev_dbg(&bq2589x_client->dev,
				"registered charger irq for pin %d\n",
				chip->irq);
		}
	}

	bq2589x_set_inlmt(chip, 900);
	bq2589x_set_cc(chip, 1000);
	bq2589x_enable_hvdcp(chip, true);
	bq2589x_force_dpdm(chip);

	schedule_delayed_work(&chip->reg_wrkr, 0);
	return 0;
}

static int bq2589x_remove(struct i2c_client *client)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(client);

	bq2589x_remove_debugfs(chip);

	if (!chip->pdata->slave_mode)
		power_supply_unregister(&chip->usb);

	if (chip->irq > 0)
		free_irq(chip->irq, chip);

	extcon_unregister_interest(&chip->cable_obj);
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	return 0;
}

static void bq2589x_shutdown(struct i2c_client *client)
{
	struct bq2589x_chip *chip = i2c_get_clientdata(client);
	int ret;

	bq2589x_enable_hvdcp(chip, false);
	bq2589x_enable_charging(chip);
	ret = bq2589x_enable_watchdog_timer(chip);
	pr_info("%s: enable chg wdt %s\n", __func__,
			ret ? "failed" : "succeeded");
}

#ifdef CONFIG_PM
static int bq2589x_suspend(struct device *dev)
{
	struct bq2589x_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->reg_wrkr);
	dev_dbg(&chip->client->dev, "bq2589x suspend\n");
	return 0;
}

static int bq2589x_resume(struct device *dev)
{
	struct bq2589x_chip *chip = dev_get_drvdata(dev);

	schedule_delayed_work(&chip->reg_wrkr, 0);
	power_supply_changed(NULL);
	dev_dbg(&chip->client->dev, "bq2589x resume\n");
	return 0;
}


#else
#define bq2589x_suspend	NULL
#define bq2589x_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int bq2589x_runtime_suspend(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq2589x_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq2589x_runtime_idle(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#else
#define bq2589x_runtime_suspend	NULL
#define bq2589x_runtime_resume	NULL
#define bq2589x_runtime_idle	NULL
#endif

char *bq2589x_supplied_to[] = {
	"battery",
};

/* Xiaomi supports high voltage charger, so
 * max_cc need large compared with standard charge
 * */
struct bq2589x_platform_data tbg2589x_drvdata = {
	.throttle_states = byt_throttle_states,
	.supplied_to = bq2589x_supplied_to,
	.num_throttle_states = ARRAY_SIZE(byt_throttle_states),
	.num_supplicants = ARRAY_SIZE(bq2589x_supplied_to),
	.supported_cables = POWER_SUPPLY_CHARGER_TYPE_USB,
	.sfi_tabl_present = true,
	.max_cc = BATT_CC_MAX_DEF,
	.max_cv = BATT_CV_MAX_DEF,
	.max_temp = BATT_TEMP_MAX_DEF,
	.min_temp = BATT_TEMP_MIN_DEF,
};

static const struct i2c_device_id bq2589x_id[] = {
	{ "TBQ2589X", (kernel_ulong_t)&tbg2589x_drvdata},
	/* Note: if charger connected with PMIC,use "ext-charger",
	 * otherwise remove it
	 * */
	{ "ext-charger", (kernel_ulong_t)&tbg2589x_drvdata },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq2589x_id);

#ifdef CONFIG_ACPI
static struct acpi_device_id bq2589x_acpi_match[] = {
	{"TBQ2589X", (kernel_ulong_t)&tbg2589x_drvdata},
	{}
};
MODULE_DEVICE_TABLE(acpi, bq2589x_acpi_match);
#endif

static const struct dev_pm_ops bq2589x_pm_ops = {
	.suspend		= bq2589x_suspend,
	.resume			= bq2589x_resume,
	.runtime_suspend	= bq2589x_runtime_suspend,
	.runtime_resume		= bq2589x_runtime_resume,
	.runtime_idle		= bq2589x_runtime_idle,
};

static struct i2c_driver bq2589x_i2c_driver = {
	.driver	= {
		.name	= DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &bq2589x_pm_ops,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(bq2589x_acpi_match),
#endif
	},
	.probe		= bq2589x_probe,
	.remove		= bq2589x_remove,
	.shutdown	= bq2589x_shutdown,
	.id_table	= bq2589x_id,
};

static int __init bq2589x_init(void)
{
	return i2c_add_driver(&bq2589x_i2c_driver);
}
module_init(bq2589x_init);

static void __exit bq2589x_exit(void)
{
	i2c_del_driver(&bq2589x_i2c_driver);
}
module_exit(bq2589x_exit);

MODULE_AUTHOR("Zhang Yu <yu.d.zhang@intel.com>");
MODULE_DESCRIPTION("BQ2589X Charger Driver");
MODULE_LICENSE("GPL");
