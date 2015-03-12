/*
 * bq24192_charger.c - Charger driver for TI BQ24192,BQ24191 and BQ24190
 *
 * Copyright (C) 2011 Intel Corporation
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
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 * Author: Raj Pandey <raj.pandey@intel.com>
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
#include <linux/power/bq24192_charger.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include <linux/usb/otg.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/completion.h>

#include <asm/intel_em_config.h>


#define DRV_NAME "bq24192_charger"
#define DEV_NAME "bq24192"

/*
 * D0, D1, D2 can be used to set current limits
 * and D3, D4, D5, D6 can be used to voltage limits
 */
#define BQ24192_INPUT_SRC_CNTL_REG		0x0
#define INPUT_SRC_CNTL_EN_HIZ			(1 << 7)
#define BATTERY_NEAR_FULL(a)			((a * 98)/100)
/*
 * set input voltage lim to 4.68V. This will help in charger
 * instability issue when duty cycle reaches 100%.
 */
#define INPUT_SRC_VOLT_LMT_DEF                 (3 << 4)
#define INPUT_SRC_VOLT_LMT_404                 (1 << 4)
#define INPUT_SRC_VOLT_LMT_412                 (3 << 3)
#define INPUT_SRC_VOLT_LMT_444                 (7 << 3)
#define INPUT_SRC_VOLT_LMT_460                 (9 << 3)
#define INPUT_SRC_VOLT_LMT_468                 (5 << 4)
#define INPUT_SRC_VOLT_LMT_476                 (0xB << 3)

#define INPUT_SRC_VINDPM_MASK                  (0xF << 3)
#define INPUT_SRC_LOW_VBAT_LIMIT               3600
#define INPUT_SRC_MIDL_VBAT_LIMIT1             3800
#define INPUT_SRC_MIDL_VBAT_LIMIT2             4100
#define INPUT_SRC_HIGH_VBAT_LIMIT              4350

/* D0, D1, D2 represent the input current limit */
#define INPUT_SRC_CUR_MASK		0x7
#define INPUT_SRC_CUR_LMT0		0x0	/* 100mA */
#define INPUT_SRC_CUR_LMT1		0x1	/* 150mA */
#define INPUT_SRC_CUR_LMT2		0x2	/* 500mA */
#define INPUT_SRC_CUR_LMT3		0x3	/* 900mA */
#define INPUT_SRC_CUR_LMT4		0x4	/* 1200mA */
#define INPUT_SRC_CUR_LMT5		0x5	/* 1500mA */
#define INPUT_SRC_CUR_LMT6		0x6	/* 2000mA */
#define INPUT_SRC_CUR_LMT7		0x7	/* 3000mA */

/*
 * D1, D2, D3 can be used to set min sys voltage limit
 * and D4, D5 can be used to control the charger
 */
#define BQ24192_POWER_ON_CFG_REG		0x1
#define POWER_ON_CFG_RESET			(1 << 7)
#define POWER_ON_CFG_I2C_WDTTMR_RESET		(1 << 6)
/* BQ2419X series charger and OTG enable bits */
#define CHR_CFG_BIT_POS				4
#define CHR_CFG_BIT_LEN				2
#define CHR_CFG_CHRG_MASK			3
#define POWER_ON_CFG_CHRG_CFG_DIS		(0 << 4)
#define POWER_ON_CFG_CHRG_CFG_EN		(1 << 4)
#define POWER_ON_CFG_CHRG_CFG_OTG		(3 << 4)
/* BQ2429X series charger and OTG enable bits */
#define POWER_ON_CFG_BQ29X_OTG_EN		(1 << 5)
#define POWER_ON_CFG_BQ29X_CHRG_EN		(1 << 4)
#define POWER_ON_CFG_BOOST_LIM			(1 << 0)

/*
 * Charge Current control register
 * with range from 500 - 4532mA
 */
#define BQ24192_CHRG_CUR_CNTL_REG		0x2
#define BQ24192_CHRG_CUR_OFFSET		500	/* 500 mA */
#define BQ24192_CHRG_CUR_LSB_TO_CUR	64	/* 64 mA */
#define BQ24192_GET_CHRG_CUR(reg) ((reg>>2)*BQ24192_CHRG_CUR_LSB_TO_CUR\
			+ BQ24192_CHRG_CUR_OFFSET) /* in mA */
#define BQ24192_CHRG_ITERM_OFFSET       128
#define BQ24192_CHRG_CUR_LSB_TO_ITERM   128

/* Pre charge and termination current limit reg */
#define BQ24192_PRECHRG_TERM_CUR_CNTL_REG	0x3
#define BQ24192_TERM_CURR_LIMIT_128		0	/* 128mA */
#define BQ24192_PRE_CHRG_CURR_256		(1 << 4)  /* 256mA */

/* Charge voltage control reg */
#define BQ24192_CHRG_VOLT_CNTL_REG	0x4
#define BQ24192_CHRG_VOLT_OFFSET	3504	/* 3504 mV */
#define BQ24192_CHRG_VOLT_LSB_TO_VOLT	16	/* 16 mV */
/* Low voltage setting 0 - 2.8V and 1 - 3.0V */
#define CHRG_VOLT_CNTL_BATTLOWV		(1 << 1)
/* Battery Recharge threshold 0 - 100mV and 1 - 300mV */
#define CHRG_VOLT_CNTL_VRECHRG		(1 << 0)
#define BQ24192_GET_CHRG_VOLT(reg) ((reg>>2)*BQ24192_CHRG_VOLT_LSB_TO_VOLT\
			+ BQ24192_CHRG_VOLT_OFFSET) /* in mV */

/* Charge termination and Timer control reg */
#define BQ24192_CHRG_TIMER_EXP_CNTL_REG		0x5
#define CHRG_TIMER_EXP_CNTL_EN_TERM		(1 << 7)
#define CHRG_TIMER_EXP_CNTL_TERM_STAT		(1 << 6)
/* WDT Timer uses 2 bits */
#define WDT_TIMER_BIT_POS			4
#define WDT_TIMER_BIT_LEN			2
#define CHRG_TIMER_EXP_CNTL_WDTDISABLE		(0 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT40SEC		(1 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT80SEC		(2 << 4)
#define CHRG_TIMER_EXP_CNTL_WDT160SEC		(3 << 4)
#define WDTIMER_RESET_MASK			0x40
/* Safety Timer Enable bit */
#define CHRG_TIMER_EXP_CNTL_EN_TIMER		(1 << 3)
/* Charge Timer uses 2bits(20 hrs) */
#define SFT_TIMER_BIT_POS			1
#define SFT_TIMER_BIT_LEN			2
#define CHRG_TIMER_EXP_CNTL_SFT_TIMER		(3 << 1)

#define BQ24192_CHRG_THRM_REGL_REG		0x6

#define BQ24192_MISC_OP_CNTL_REG		0x7
#define MISC_OP_CNTL_DPDM_EN			(1 << 7)
#define MISC_OP_CNTL_TMR2X_EN			(1 << 6)
#define MISC_OP_CNTL_BATFET_DIS			(1 << 5)
#define MISC_OP_CNTL_BATGOOD_EN			(1 << 4)
/* To mask INT's write 0 to the bit */
#define MISC_OP_CNTL_MINT_CHRG			(1 << 1)
#define MISC_OP_CNTL_MINT_BATT			(1 << 0)

#define BQ24192_SYSTEM_STAT_REG			0x8
/* D6, D7 show VBUS status */
#define SYSTEM_STAT_VBUS_BITS			(3 << 6)
#define SYSTEM_STAT_VBUS_UNKNOWN		0
#define SYSTEM_STAT_VBUS_HOST			(1 << 6)
#define SYSTEM_STAT_VBUS_ADP			(2 << 6)
#define SYSTEM_STAT_VBUS_OTG			(3 << 6)
/* D4, D5 show charger status */
#define SYSTEM_STAT_NOT_CHRG			(0 << 4)
#define SYSTEM_STAT_PRE_CHRG			(1 << 4)
#define SYSTEM_STAT_FAST_CHRG			(2 << 4)
#define SYSTEM_STAT_CHRG_DONE			(3 << 4)
#define SYSTEM_STAT_DPM				(1 << 3)
#define SYSTEM_STAT_PWR_GOOD			(1 << 2)
#define SYSTEM_STAT_THERM_REG			(1 << 1)
#define SYSTEM_STAT_VSYS_LOW			(1 << 0)
#define SYSTEM_STAT_CHRG_MASK			(3 << 4)

#define BQ24192_FAULT_STAT_REG			0x9
#define FAULT_STAT_WDT_TMR_EXP			(1 << 7)
#define FAULT_STAT_OTG_FLT			(1 << 6)
/* D4, D5 show charger fault status */
#define FAULT_STAT_CHRG_BITS			(3 << 4)
#define FAULT_STAT_CHRG_NORMAL			(0 << 4)
#define FAULT_STAT_CHRG_IN_FLT			(1 << 4)
#define FAULT_STAT_CHRG_THRM_FLT		(2 << 4)
#define FAULT_STAT_CHRG_TMR_FLT			(3 << 4)
#define FAULT_STAT_BATT_FLT			(1 << 3)
#define FAULT_STAT_BATT_TEMP_BITS		(3 << 0)

#define BQ24192_VENDER_REV_REG			0xA
/* D3, D4, D5 indicates the chip model number */
#define BQ24190_IC_VERSION			0x0
#define BQ24191_IC_VERSION			0x1
#define BQ24192_IC_VERSION			0x2
#define BQ24192I_IC_VERSION			0x3
#define BQ24296_IC_VERSION			0x4
#define BQ24297_IC_VERSION			0xC

#define BQ24192_MAX_MEM		12
#define NR_RETRY_CNT		3

#define CHARGER_PS_NAME				"bq24192_charger"

#define CHARGER_TASK_JIFFIES		(HZ * 150)/* 150sec */
#define CHARGER_HOST_JIFFIES		(HZ * 60) /* 60sec */
#define FULL_THREAD_JIFFIES		(HZ * 30) /* 30sec */
#define TEMP_THREAD_JIFFIES		(HZ * 30) /* 30sec */

#define BATT_TEMP_MAX_DEF	60	/* 60 degrees */
#define BATT_TEMP_MIN_DEF	0

/* Max no. of tries to clear the charger from Hi-Z mode */
#define MAX_TRY		3

/* Max no. of tries to reset the bq24192i WDT */
#define MAX_RESET_WDT_RETRY 8
#define VBUS_DET_TIMEOUT msecs_to_jiffies(50) /* 50 msec */

#define BQ_CHARGE_CUR_SDP	500
#define BQ_CHARGE_CUR_DCP	2000

#define BQ_GPIO_MUX_SEL_PMIC	0
#define BQ_GPIO_MUX_SEL_SOC		1

static struct power_supply *fg_psy;

enum bq24192_chrgr_stat {
	BQ24192_CHRGR_STAT_UNKNOWN,
	BQ24192_CHRGR_STAT_CHARGING,
	BQ24192_CHRGR_STAT_BAT_FULL,
	BQ24192_CHRGR_STAT_FAULT,
	BQ24192_CHRGR_STAT_LOW_SUPPLY_FAULT
};

enum bq24192_chip_type {
	BQ24190, BQ24191, BQ24192,
	BQ24192I, BQ24296, BQ24297
};

struct bq24192_chip {
	struct i2c_client *client;
	struct bq24192_platform_data *pdata;
	enum bq24192_chip_type chip_type;
	struct power_supply usb;
	struct delayed_work chrg_task_wrkr;
	struct delayed_work chrg_full_wrkr;
	struct delayed_work chrg_temp_wrkr;
	struct delayed_work irq_wrkr;
	struct mutex event_lock;
	struct power_supply_cable_props cap;
	struct power_supply_cable_props cached_cap;
	struct notifier_block otg_notify;
	struct usb_phy *transceiver;
	struct completion vbus_detect;
	/* Wake lock to prevent platform from going to S3 when charging */
	struct wake_lock wakelock;


	enum bq24192_chrgr_stat chgr_stat;
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
	struct gpio_desc *switch_io;
	bool is_charger_enabled;
	bool is_charging_enabled;
	bool a_bus_enable;
	bool is_pwr_good;
	bool boost_mode;
	bool online;
	bool present;
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *bq24192_dbgfs_root;
static char bq24192_dbg_regs[BQ24192_MAX_MEM][4];
#endif

static struct i2c_client *bq24192_client;

static enum power_supply_property bq24192_usb_props[] = {
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
	POWER_SUPPLY_PROP_ENABLE_CHARGER,
	POWER_SUPPLY_PROP_CHARGE_TERM_CUR,
	POWER_SUPPLY_PROP_CABLE_TYPE,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_MAX_TEMP,
	POWER_SUPPLY_PROP_MIN_TEMP
};

#define BYTCR_CHRG_CUR_NOLIMIT  1800
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

static int bq24192_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
	int ret, i;

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

static int bq24192_read_reg(struct i2c_client *client, u8 reg)
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

#ifdef DEBUG
/*
 * This function dumps the bq24192 registers
 */
static void bq24192_dump_registers(struct bq24192_chip *chip)
{
	int ret;

	dev_info(&chip->client->dev, "%s\n", __func__);

	/* Input Src Ctrl register */
	ret = bq24192_read_reg(chip->client, BQ24192_INPUT_SRC_CNTL_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Input Src Ctrl reg read fail\n");
	dev_info(&chip->client->dev, "REG00 %x\n", ret);

	/* Pwr On Cfg register */
	ret = bq24192_read_reg(chip->client, BQ24192_POWER_ON_CFG_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Pwr On Cfg reg read fail\n");
	dev_info(&chip->client->dev, "REG01 %x\n", ret);

	/* Chrg Curr Ctrl register */
	ret = bq24192_read_reg(chip->client, BQ24192_CHRG_CUR_CNTL_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Chrg Curr Ctrl reg read fail\n");
	dev_info(&chip->client->dev, "REG02 %x\n", ret);

	/* Pre-Chrg Term register */
	ret = bq24192_read_reg(chip->client,
					BQ24192_PRECHRG_TERM_CUR_CNTL_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Pre-Chrg Term reg read fail\n");
	dev_info(&chip->client->dev, "REG03 %x\n", ret);

	/* Chrg Volt Ctrl register */
	ret = bq24192_read_reg(chip->client, BQ24192_CHRG_VOLT_CNTL_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Chrg Volt Ctrl reg read fail\n");
	dev_info(&chip->client->dev, "REG04 %x\n", ret);

	/* Chrg Term and Timer Ctrl register */
	ret = bq24192_read_reg(chip->client, BQ24192_CHRG_TIMER_EXP_CNTL_REG);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
			"Chrg Term and Timer Ctrl reg read fail\n");
	}
	dev_info(&chip->client->dev, "REG05 %x\n", ret);

	/* Thermal Regulation register */
	ret = bq24192_read_reg(chip->client, BQ24192_CHRG_THRM_REGL_REG);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
				"Thermal Regulation reg read fail\n");
	}
	dev_info(&chip->client->dev, "REG06 %x\n", ret);

	/* Misc Operations Ctrl register */
	ret = bq24192_read_reg(chip->client, BQ24192_MISC_OP_CNTL_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Misc Op Ctrl reg read fail\n");
	dev_info(&chip->client->dev, "REG07 %x\n", ret);

	/* System Status register */
	ret = bq24192_read_reg(chip->client, BQ24192_SYSTEM_STAT_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "System Status reg read fail\n");
	dev_info(&chip->client->dev, "REG08 %x\n", ret);

	/* Fault Status register */
	ret = bq24192_read_reg(chip->client, BQ24192_FAULT_STAT_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Fault Status reg read fail\n");
	dev_info(&chip->client->dev, "REG09 %x\n", ret);

	/* Vendor Revision register */
	ret = bq24192_read_reg(chip->client, BQ24192_VENDER_REV_REG);
	if (ret < 0)
		dev_warn(&chip->client->dev, "Vendor Rev reg read fail\n");
	dev_info(&chip->client->dev, "REG0A %x\n", ret);
}
#endif

/*
 * If the bit_set is TRUE then val 1s will be SET in the reg else val 1s will
 * be CLEARED
 */
static int bq24192_reg_read_modify(struct i2c_client *client, u8 reg,
							u8 val, bool bit_set)
{
	int ret;

	ret = bq24192_read_reg(client, reg);

	if (bit_set)
		ret |= val;
	else
		ret &= (~val);

	ret = bq24192_write_reg(client, reg, ret);

	return ret;
}

static int bq24192_reg_multi_bitset(struct i2c_client *client, u8 reg,
						u8 val, u8 pos, u8 len)
{
	int ret;
	u8 data;

	ret = bq24192_read_reg(client, reg);
	if (ret < 0) {
		dev_warn(&client->dev, "I2C SMbus Read error:%d\n", ret);
		return ret;
	}

	data = (1 << len) - 1;
	ret = (ret & ~(data << pos)) | val;
	ret = bq24192_write_reg(client, reg, ret);

	return ret;
}

/*
 * This function verifies if the bq24192i charger chip is in Hi-Z
 * If yes, then clear the Hi-Z to resume the charger operations
 */
static int bq24192_clear_hiz(struct bq24192_chip *chip)
{
	int ret, count;

	dev_info(&chip->client->dev, "%s\n", __func__);

	for (count = 0; count < MAX_TRY; count++) {
		/*
		 * Read the bq24192i REG00 register for charger Hi-Z mode.
		 * If it is in Hi-Z, then clear the Hi-Z to resume the charging
		 * operations.
		 */
		ret = bq24192_read_reg(chip->client,
				BQ24192_INPUT_SRC_CNTL_REG);
		if (ret < 0) {
			dev_warn(&chip->client->dev,
					"Input src cntl read failed\n");
			goto i2c_error;
		}

		if (ret & INPUT_SRC_CNTL_EN_HIZ) {
			dev_warn(&chip->client->dev,
						"Charger IC in Hi-Z mode\n");
#ifdef DEBUG
			bq24192_dump_registers(chip);
#endif
			/* Clear the Charger from Hi-Z mode */
			ret &= ~INPUT_SRC_CNTL_EN_HIZ;

			/* Write the values back */
			ret = bq24192_write_reg(chip->client,
					BQ24192_INPUT_SRC_CNTL_REG, ret);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"Input src cntl write failed\n");
				goto i2c_error;
			}
			msleep(150);
		} else {
			dev_info(&chip->client->dev,
						"Charger is not in Hi-Z\n");
			break;
		}
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
 * bq24192_get_charger_health - to get the charger health status
 *
 * Returns charger health status
 */
int bq24192_get_charger_health(void)
{
	int ret_status, ret_fault;
	struct bq24192_chip *chip =
		i2c_get_clientdata(bq24192_client);

	dev_dbg(&chip->client->dev, "%s\n", __func__);

	/* If we do not have any cable connected, return health as UNKNOWN */
	if (chip->cable_type == POWER_SUPPLY_CHARGER_TYPE_NONE)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	ret_fault = bq24192_read_reg(chip->client, BQ24192_FAULT_STAT_REG);
	if (ret_fault < 0) {
		dev_warn(&chip->client->dev,
			"read reg failed %s\n", __func__);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	/* Check if the error VIN condition occured */
	ret_status = bq24192_read_reg(chip->client, BQ24192_SYSTEM_STAT_REG);
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
 * bq24192_get_battery_health - to get the battery health status
 *
 * Returns battery health status
 */
int bq24192_get_battery_health(void)
{
	int  temp, vnow;
	struct bq24192_chip *chip;
	if (!bq24192_client)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	chip = i2c_get_clientdata(bq24192_client);

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
EXPORT_SYMBOL(bq24192_get_battery_health);

static u8 chrg_iterm_to_reg(int iterm)
{
	u8 reg;

	if (iterm <= BQ24192_CHRG_ITERM_OFFSET)
		reg = 0;
	else
		reg = ((iterm - BQ24192_CHRG_ITERM_OFFSET) /
			BQ24192_CHRG_CUR_LSB_TO_ITERM);
	return reg;
}

/* convert the charge current value
 * into equivalent register setting
 */
static u8 chrg_cur_to_reg(int cur)
{
	u8 reg;

	if (cur <= BQ24192_CHRG_CUR_OFFSET)
		reg = 0x0;
	else
		reg = ((cur - BQ24192_CHRG_CUR_OFFSET) /
				BQ24192_CHRG_CUR_LSB_TO_CUR) + 1;

	/* D0, D1 bits of Charge Current
	 * register are not used */
	reg = reg << 2;
	return reg;
}

/* convert the charge voltage value
 * into equivalent register setting
 */
static u8 chrg_volt_to_reg(int volt)
{
	u8 reg;

	if (volt <= BQ24192_CHRG_VOLT_OFFSET)
		reg = 0x0;
	else
		reg = (volt - BQ24192_CHRG_VOLT_OFFSET) /
				BQ24192_CHRG_VOLT_LSB_TO_VOLT;

	reg = (reg << 2) | CHRG_VOLT_CNTL_BATTLOWV;
	return reg;
}

static int bq24192_enable_hw_term(struct bq24192_chip *chip, bool hw_term_en)
{
	int ret = 0;

	dev_info(&chip->client->dev, "%s\n", __func__);

	/* Disable and enable charging to restart the charging */
	ret = bq24192_reg_multi_bitset(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_CHRG_CFG_DIS,
					CHR_CFG_BIT_POS,
					CHR_CFG_BIT_LEN);
	if (ret < 0) {
		dev_warn(&chip->client->dev,
			"i2c reg write failed: reg: %d, ret: %d\n",
			BQ24192_POWER_ON_CFG_REG, ret);
		return ret;
	}

	/* Read the timer control register */
	ret = bq24192_read_reg(chip->client, BQ24192_CHRG_TIMER_EXP_CNTL_REG);
	if (ret < 0) {
		dev_warn(&chip->client->dev, "TIMER CTRL reg read failed\n");
		return ret;
	}

	/*
	 * Enable the HW termination. When disabled the HW termination, battery
	 * was taking too long to go from charging to full state. HW based
	 * termination could cause the battery capacity to drop but it would
	 * result in good battery life.
	 */
	if (hw_term_en)
		ret |= CHRG_TIMER_EXP_CNTL_EN_TERM;
	else
		ret &= ~CHRG_TIMER_EXP_CNTL_EN_TERM;

	/* Program the TIMER CTRL register */
	ret = bq24192_write_reg(chip->client,
				BQ24192_CHRG_TIMER_EXP_CNTL_REG,
				ret);
	if (ret < 0)
		dev_warn(&chip->client->dev, "TIMER CTRL I2C write failed\n");

	return ret;
}

/*
 * chip->event_lock need to be acquired before calling this function
 * to avoid the race condition
 */
static int program_timers(struct bq24192_chip *chip, int wdt_duration,
				bool sfttmr_enable)
{
	int ret;

	/* Read the timer control register */
	ret = bq24192_read_reg(chip->client, BQ24192_CHRG_TIMER_EXP_CNTL_REG);
	if (ret < 0) {
		dev_warn(&chip->client->dev, "TIMER CTRL reg read failed\n");
		return ret;
	}

	/* Program the time with duration passed */
	ret |=  wdt_duration;

	/* Enable/Disable the safety timer */
	if (sfttmr_enable)
		ret |= CHRG_TIMER_EXP_CNTL_EN_TIMER;
	else
		ret &= ~CHRG_TIMER_EXP_CNTL_EN_TIMER;

	/* Program the TIMER CTRL register */
	ret = bq24192_write_reg(chip->client,
				BQ24192_CHRG_TIMER_EXP_CNTL_REG,
				ret);
	if (ret < 0)
		dev_warn(&chip->client->dev, "TIMER CTRL I2C write failed\n");

	return ret;
}

/* This function should be called with the mutex held */
static int reset_wdt_timer(struct bq24192_chip *chip)
{
	int ret = 0;

	/* reset WDT timer */
	ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					WDTIMER_RESET_MASK, true);
	if (ret < 0)
		dev_warn(&chip->client->dev, "I2C write failed:%s\n",
						__func__);
	return ret;
}

/*
 *This function will modify the VINDPM as per the battery voltage
 */
static int bq24192_modify_vindpm(u8 vindpm)
{
	int ret;
	u8 vindpm_prev;
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);

	dev_info(&chip->client->dev, "%s\n", __func__);

	/* Get the input src ctrl values programmed */
	ret = bq24192_read_reg(chip->client,
				BQ24192_INPUT_SRC_CNTL_REG);

	if (ret < 0) {
		dev_warn(&chip->client->dev, "INPUT CTRL reg read failed\n");
		return ret;
	}

	/* Assign the return value of REG00 to vindpm_prev */
	vindpm_prev = (ret & INPUT_SRC_VINDPM_MASK);
	ret &= ~INPUT_SRC_VINDPM_MASK;

	/*
	 * If both the previous and current values are same do not program
	 * the register.
	*/
	if (vindpm_prev != vindpm) {
		vindpm |= ret;
		ret = bq24192_write_reg(chip->client,
					BQ24192_INPUT_SRC_CNTL_REG, vindpm);
		if (ret < 0) {
			dev_info(&chip->client->dev, "VINDPM failed\n");
			return ret;
		}
	}
	return ret;
}

/* This function should be called with the mutex held */
static int bq24192_turn_otg_vbus(struct bq24192_chip *chip, bool votg_on)
{
	int ret = 0;

	dev_info(&chip->client->dev, "%s %d\n", __func__, votg_on);

	if (votg_on && chip->a_bus_enable) {
			/* Program the timers */
			ret = program_timers(chip,
						CHRG_TIMER_EXP_CNTL_WDT80SEC,
						false);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
					"TIMER enable failed %s\n", __func__);
				goto i2c_write_fail;
			}
			/* Configure the charger in OTG mode */
			if ((chip->chip_type == BQ24296) ||
				(chip->chip_type == BQ24297))
				ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_BQ29X_OTG_EN, true);
			else
				ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_CHRG_CFG_OTG, true);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"read reg modify failed\n");
				goto i2c_write_fail;
			}

			/* Put the charger IC in reverse boost mode. Since
			 * SDP charger can supply max 500mA charging current
			 * Setting the boost current to 500mA
			 */
			ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_BOOST_LIM, false);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"read reg modify failed\n");
				goto i2c_write_fail;
			}
			chip->boost_mode = true;
			/* Schedule the charger task worker now */
			schedule_delayed_work(&chip->chrg_task_wrkr,
						0);
	} else {
			/* Clear the charger from the OTG mode */
			if ((chip->chip_type == BQ24296) ||
				(chip->chip_type == BQ24297))
				ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_BQ29X_OTG_EN, false);
			else
				ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_CHRG_CFG_OTG, false);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"read reg modify failed\n");
				goto i2c_write_fail;
			}

			/* Put the charger IC out of reverse boost mode 500mA */
			ret = bq24192_reg_read_modify(chip->client,
					BQ24192_POWER_ON_CFG_REG,
					POWER_ON_CFG_BOOST_LIM, false);
			if (ret < 0) {
				dev_warn(&chip->client->dev,
						"read reg modify failed\n");
				goto i2c_write_fail;
			}
			chip->boost_mode = false;
			/* Cancel the charger task worker now */
			cancel_delayed_work_sync(&chip->chrg_task_wrkr);
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

int bq24192_vbus_enable(void)
{
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);
	return bq24192_turn_otg_vbus(chip, true);
}
EXPORT_SYMBOL(bq24192_vbus_enable);

int bq24192_vbus_disable(void)
{
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);
	return bq24192_turn_otg_vbus(chip, false);
}
EXPORT_SYMBOL(bq24192_vbus_disable);

int bq24192_set_usb_port(int port_mode)
{
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);
	if ((chip->chip_type == BQ24297) &&
			chip->switch_io)
		gpiod_set_value(chip->switch_io, port_mode);
	return 0;
}
EXPORT_SYMBOL(bq24192_set_usb_port);

#ifdef CONFIG_DEBUG_FS
#define DBGFS_REG_BUF_LEN	3

static int bq24192_show(struct seq_file *seq, void *unused)
{
	u16 val;
	long addr;

	if (kstrtol((char *)seq->private, 16, &addr))
		return -EINVAL;

	val = bq24192_read_reg(bq24192_client, addr);
	seq_printf(seq, "%x\n", val);

	return 0;
}

static int bq24192_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq24192_show, inode->i_private);
}

static ssize_t bq24192_dbgfs_reg_write(struct file *file,
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

	dev_info(&bq24192_client->dev,
			"[dbgfs write] Addr:0x%x Val:0x%x\n",
			(u32)addr, (u32)value);


	ret = bq24192_write_reg(bq24192_client, addr, value);
	if (ret < 0)
		dev_warn(&bq24192_client->dev, "I2C write failed\n");

	return count;
}

static const struct file_operations bq24192_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= bq24192_dbgfs_open,
	.read		= seq_read,
	.write		= bq24192_dbgfs_reg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int bq24192_create_debugfs(struct bq24192_chip *chip)
{
	int i;
	struct dentry *entry;

	bq24192_dbgfs_root = debugfs_create_dir(DEV_NAME, NULL);
	if (IS_ERR(bq24192_dbgfs_root)) {
		dev_warn(&chip->client->dev, "DEBUGFS DIR create failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < BQ24192_MAX_MEM; i++) {
		sprintf((char *)&bq24192_dbg_regs[i], "%x", i);
		entry = debugfs_create_file(
					(const char *)&bq24192_dbg_regs[i],
					S_IRUGO,
					bq24192_dbgfs_root,
					&bq24192_dbg_regs[i],
					&bq24192_dbgfs_fops);
		if (IS_ERR(entry)) {
			debugfs_remove_recursive(bq24192_dbgfs_root);
			bq24192_dbgfs_root = NULL;
			dev_warn(&chip->client->dev,
					"DEBUGFS entry Create failed\n");
			return -ENOMEM;
		}
	}

	return 0;
}
static inline void bq24192_remove_debugfs(struct bq24192_chip *chip)
{
	debugfs_remove_recursive(bq24192_dbgfs_root);
}
#else
static inline int bq24192_create_debugfs(struct bq24192_chip *chip)
{
	return 0;
}
static inline void bq24192_remove_debugfs(struct bq24192_chip *chip)
{
}
#endif

static inline int bq24192_set_inlmt(struct bq24192_chip *chip, int inlmt)
{
	int regval, regval_prev, timeout, ret = 0;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, inlmt);
	chip->inlmt = inlmt;

	regval_prev = bq24192_read_reg(chip->client,
					BQ24192_INPUT_SRC_CNTL_REG);

	if (regval_prev < 0) {
		dev_info(&chip->client->dev, "read failed %d\n", regval_prev);
		return regval_prev;
	}

	/* Set the input source current limit
	 * between 100 to 1500mA */
	if (inlmt <= 100)
		regval = INPUT_SRC_CUR_LMT0;
	else if (inlmt <= 150)
		regval = INPUT_SRC_CUR_LMT1;
	else if (inlmt <= 500)
		regval = INPUT_SRC_CUR_LMT2;
	else if (inlmt <= 900)
		regval = INPUT_SRC_CUR_LMT3;
	else if (inlmt <= 1200)
		regval = INPUT_SRC_CUR_LMT4;
	else if (inlmt <= 1500)
		regval = INPUT_SRC_CUR_LMT5;
	else if (inlmt <= 2000)
		regval = INPUT_SRC_CUR_LMT6;
	else if (inlmt <= 3000)
		regval = INPUT_SRC_CUR_LMT7;
	else /* defaulting to 500MA*/
		regval = INPUT_SRC_CUR_LMT2;

	/* if both the previous and current values are same, do not
	 * program the register.
	 */
	if (regval == (regval_prev & INPUT_SRC_CUR_MASK))
		return ret;
	/* Wait for VBUS if inlimit > 0 */
	if (inlmt > 0) {
		timeout = wait_for_completion_timeout(&chip->vbus_detect,
					VBUS_DET_TIMEOUT);
		if (timeout == 0)
			dev_warn(&chip->client->dev,
				"VBUS Detect timedout. Setting INLIMIT");
	}

	regval = (regval_prev & ~INPUT_SRC_CUR_MASK) | regval;

	ret =  bq24192_write_reg(chip->client,
				BQ24192_INPUT_SRC_CNTL_REG, regval);
	if (ret < 0)
		dev_info(&chip->client->dev, "Inlimit set failed %d\n", ret);

	return ret;
}

static inline int bq24192_enable_charging(
			struct bq24192_chip *chip, bool val)
{
	int ret, regval;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, val);

	ret = program_timers(chip, CHRG_TIMER_EXP_CNTL_WDT160SEC, false);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"program_timers failed: %d\n", ret);
		return ret;
	}

	ret = bq24192_read_reg(chip->client, BQ24192_POWER_ON_CFG_REG);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"pwr cfg read failed: %d\n", ret);
		return ret;
	}

	if ((chip->chip_type == BQ24296) || (chip->chip_type == BQ24297)) {
		if (val)
			regval = ret | POWER_ON_CFG_BQ29X_CHRG_EN;
		else
			regval = ret & ~POWER_ON_CFG_BQ29X_CHRG_EN;
	} else {
		/* clear the charge enbale bit mask first */
		ret &= ~(CHR_CFG_CHRG_MASK << CHR_CFG_BIT_POS);
		if (val)
			regval = ret | POWER_ON_CFG_CHRG_CFG_EN;
		else
			regval = ret | POWER_ON_CFG_CHRG_CFG_DIS;
	}

	/* when coming out of fault condition we need to set inlimit
	 * because HW will set the inlimit to default in fault condition
	 */
	bq24192_set_inlmt(chip, chip->inlmt);

	ret = bq24192_write_reg(chip->client, BQ24192_POWER_ON_CFG_REG, regval);
	if (ret < 0)
		dev_warn(&chip->client->dev, "charger enable/disable failed\n");

	if (val) {
		/* Schedule the charger task worker now */
		schedule_delayed_work(&chip->chrg_task_wrkr, 0);

		/*
		 * Prevent system from entering s3 while charger is connected
		 */
		if (!wake_lock_active(&chip->wakelock))
			wake_lock(&chip->wakelock);
	} else {
		/* Release the wake lock */
		if (wake_lock_active(&chip->wakelock))
			wake_unlock(&chip->wakelock);

		/*
		 * Cancel the worker since it need not run when charging is not
		 * happening
		 */
		cancel_delayed_work_sync(&chip->chrg_full_wrkr);

		/* Read the status to know about input supply */
		ret = bq24192_read_reg(chip->client, BQ24192_SYSTEM_STAT_REG);
		if (ret < 0) {
			dev_warn(&chip->client->dev,
				"read reg failed %s\n", __func__);
		}

		/* If no charger connected, cancel the workers */
		if (!(ret & SYSTEM_STAT_VBUS_OTG)) {
			dev_info(&chip->client->dev, "NO charger connected\n");
			cancel_delayed_work_sync(&chip->chrg_task_wrkr);
		}
	}

	return ret;
}

static inline int bq24192_enable_charger(
			struct bq24192_chip *chip, int val)
{
	int ret = 0;

#ifndef CONFIG_RAW_CC_THROTTLE
	/*stop charger for throttle state 3, by putting it in HiZ mode*/
	if (chip->cntl_state == 0x3) {
		ret = bq24192_reg_read_modify(chip->client,
			BQ24192_INPUT_SRC_CNTL_REG,
				INPUT_SRC_CNTL_EN_HIZ, true);

		if (ret < 0)
			dev_warn(&chip->client->dev,
				"Input src cntl write failed\n");
		else
			ret = bq24192_enable_charging(chip, val);
	}
#else
	if (val)
		ret = bq24192_reg_read_modify(chip->client,
			BQ24192_INPUT_SRC_CNTL_REG,
				INPUT_SRC_CNTL_EN_HIZ, false);
#endif


	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, val);

	return ret;
}

static inline int bq24192_set_cc(struct bq24192_chip *chip, int cc)
{
	u8 regval;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, cc);
	regval = chrg_cur_to_reg(cc);

	return bq24192_write_reg(chip->client, BQ24192_CHRG_CUR_CNTL_REG,
				regval);
}

static inline int bq24192_set_cv(struct bq24192_chip *chip, int cv)
{
	u8 regval;

	dev_warn(&chip->client->dev, "%s:%d %d\n", __func__, __LINE__, cv);
	regval = chrg_volt_to_reg(cv);

	return bq24192_write_reg(chip->client, BQ24192_CHRG_VOLT_CNTL_REG,
					regval & ~CHRG_VOLT_CNTL_VRECHRG);
}

static inline int bq24192_set_iterm(struct bq24192_chip *chip, int iterm)
{
	u8 reg_val;

	if (iterm > BQ24192_CHRG_ITERM_OFFSET)
		dev_warn(&chip->client->dev,
			"%s ITERM set for >128mA", __func__);

	reg_val = chrg_iterm_to_reg(iterm);

	return bq24192_write_reg(chip->client,
			BQ24192_PRECHRG_TERM_CUR_CNTL_REG,
				(BQ24192_PRE_CHRG_CURR_256 | reg_val));
}

static enum bq24192_chrgr_stat bq24192_is_charging(struct bq24192_chip *chip)
{
	int ret;
	ret = bq24192_read_reg(chip->client, BQ24192_SYSTEM_STAT_REG);
	if (ret < 0)
		dev_err(&chip->client->dev, "STATUS register read failed\n");

	ret &= SYSTEM_STAT_CHRG_MASK;

	switch (ret) {
	case SYSTEM_STAT_NOT_CHRG:
		chip->chgr_stat = BQ24192_CHRGR_STAT_FAULT;
		break;
	case SYSTEM_STAT_CHRG_DONE:
		chip->chgr_stat = BQ24192_CHRGR_STAT_BAT_FULL;
		break;
	case SYSTEM_STAT_PRE_CHRG:
	case SYSTEM_STAT_FAST_CHRG:
		chip->chgr_stat = BQ24192_CHRGR_STAT_CHARGING;
		break;
	default:
		break;
	}

	return chip->chgr_stat;
}

static int bq24192_usb_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
						struct bq24192_chip,
						usb);
	int ret = 0;

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
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:
		bq24192_enable_hw_term(chip, val->intval);
		ret = bq24192_enable_charging(chip, val->intval);

		if (ret < 0)
			dev_err(&chip->client->dev,
				"Error(%d) in %s charging", ret,
				(val->intval ? "enable" : "disable"));
		else
			chip->is_charging_enabled = val->intval;

		if (!val->intval)
			cancel_delayed_work_sync(&chip->chrg_full_wrkr);
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGER:
		ret = bq24192_enable_charger(chip, val->intval);

		if (ret < 0) {
			dev_err(&chip->client->dev,
			"Error(%d) in %s charger", ret,
			(val->intval ? "enable" : "disable"));
		} else
			chip->is_charger_enabled = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CURRENT:
		ret = bq24192_set_cc(chip, val->intval);
		if (!ret)
			chip->cc = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_VOLTAGE:
		ret = bq24192_set_cv(chip, val->intval);
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
		ret = bq24192_set_iterm(chip, val->intval);
		if (!ret)
			chip->iterm = val->intval;
		break;
	case POWER_SUPPLY_PROP_CABLE_TYPE:
		chip->cable_type = val->intval;
		chip->usb.type = get_power_supply_type(chip->cable_type);
		break;
	case POWER_SUPPLY_PROP_INLMT:
		ret = bq24192_set_inlmt(chip, val->intval);
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
	default:
		ret = -ENODATA;
	}

	mutex_unlock(&chip->event_lock);
	return ret;
}

static int bq24192_usb_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct bq24192_chip *chip = container_of(psy,
					struct bq24192_chip,
					usb);
	enum bq24192_chrgr_stat charging;

	dev_dbg(&chip->client->dev, "%s %d\n", __func__, psp);
	mutex_lock(&chip->event_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24192_get_charger_health();
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
		else {
			charging = bq24192_is_charging(chip);
			val->intval = (chip->is_charging_enabled && charging);
		}
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGER:
		val->intval = (bq24192_get_charger_health() ==
				POWER_SUPPLY_HEALTH_GOOD);
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
	default:
		mutex_unlock(&chip->event_lock);
		return -EINVAL;
	}

	mutex_unlock(&chip->event_lock);
	return 0;
}

static void bq24192_resume_charging(struct bq24192_chip *chip)
{
	if (chip->inlmt)
		bq24192_set_inlmt(chip, chip->inlmt);
	if (chip->cc)
		bq24192_set_cc(chip, chip->cc);
	if (chip->cv)
		bq24192_set_cv(chip, chip->cv);
	if (chip->is_charging_enabled)
		bq24192_enable_charging(chip, true);
	if (chip->is_charger_enabled)
		bq24192_enable_charger(chip, true);
}

static void bq24192_full_worker(struct work_struct *work)
{
	struct bq24192_chip *chip = container_of(work,
						    struct bq24192_chip,
						    chrg_full_wrkr.work);
	power_supply_changed(NULL);

	/* schedule the thread to let the framework know about FULL */
	schedule_delayed_work(&chip->chrg_full_wrkr, FULL_THREAD_JIFFIES);
}

static int check_cable_status(struct bq24192_chip *chip, int reg_stat)
{
	int ret = 0, vbus_mask = 0;
	static struct power_supply_cable_props cable_props;
	static bool notify_otg, notify_charger;

	if (reg_stat & SYSTEM_STAT_VBUS_HOST) {
		/* SDP charger connected */
		dev_dbg(&chip->client->dev,
					"SDP charger connected\n");
		notify_otg = true;
		notify_charger = true;
		vbus_mask = 1;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
		cable_props.ma = BQ_CHARGE_CUR_SDP;

	} else if (reg_stat & SYSTEM_STAT_VBUS_ADP) {
		/* AC Adapter or DCP connected */
		dev_dbg(&chip->client->dev,
					"DCP or CDP type Charger connected\n");
		notify_charger = true;
		/* CDP also gets detetcted as DCP, send notify */
		notify_otg = true;
		vbus_mask = 1;
		cable_props.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cable_props.chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
		cable_props.ma = BQ_CHARGE_CUR_DCP;

	} else if (reg_stat & SYSTEM_STAT_VBUS_OTG) {
		/* OTG Device connected */
		dev_dbg(&chip->client->dev,
					"USB device connected through OTG port\n");
		notify_otg = true;
		vbus_mask = 1;

	} else {
		/* No Cable connected */
			notify_otg = true;
			notify_charger = true;
			vbus_mask = 0;
			cable_props.chrg_evt =
					POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
			cable_props.ma = 0;
			dev_warn(&chip->client->dev,
						"disconnect or unknown ID event\n");
	}

	if (!vbus_mask) {
		if (notify_otg) {
			/* Cable removed, switch the USB MUX back to PMIC */
			gpiod_set_value(chip->switch_io,
					BQ_GPIO_MUX_SEL_PMIC);
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
			/* Close the MUX path to switch to OTG */
			gpiod_set_value(chip->switch_io,
					BQ_GPIO_MUX_SEL_SOC);
			atomic_notifier_call_chain(&chip->transceiver->notifier,
				vbus_mask ? USB_EVENT_VBUS : USB_EVENT_NONE,
				NULL);
			dev_info(&chip->client->dev,
			"Sent notification for USB %d\n", vbus_mask);
		}
		if (notify_charger) {
			dev_info(&chip->client->dev,
			"Sent the notification for Power Supply\n");
			atomic_notifier_call_chain(&power_supply_notifier,
					PSY_CABLE_EVENT, &cable_props);
		}

	}

	return ret;
}

static void bq24192_irq_worker(struct work_struct *work)
{
	int reg_status, reg_fault, ret;
	struct bq24192_chip *chip = container_of(work,
						    struct bq24192_chip,
						    irq_wrkr.work);

	/*
	 * check the bq24192 status/fault registers to see what is the
	 * source of the interrupt
	 */
	reg_status = bq24192_read_reg(chip->client, BQ24192_SYSTEM_STAT_REG);
	if (reg_status < 0)
		dev_err(&chip->client->dev, "STATUS register read failed:\n");

	dev_dbg(&chip->client->dev, "STATUS reg %x\n", reg_status);
	/*
	 * On VBUS detect set completion to wake waiting thread. On VBUS
	 * disconnect, re-init completion so that setting INLIMIT would be
	 * delayed till VBUS is detected.
	 */
	if (reg_status & SYSTEM_STAT_VBUS_HOST)
		complete(&chip->vbus_detect);
	/*
	 * If chip version is BQ24297, then it can detect DCP charger
	 * cable type also. Hence complete the VBUS detect wait if
	 * the cable type is ADP for BQ24297
	 */
	else if ((chip->chip_type == BQ24297) &&
			(reg_status & SYSTEM_STAT_VBUS_ADP))
		complete(&chip->vbus_detect);
	else
		reinit_completion(&chip->vbus_detect);

	/*
	 * In case of BQ24297, notify the power supply class about
	 * the Input and the type of charger cable through cable properties
	 */
	if (chip->chip_type == BQ24297) {
		ret = check_cable_status(chip, reg_status);
		if (ret < 0)
			dev_err(&chip->client->dev,
					"Failed to check cable status\n");
	}

	reg_status &= SYSTEM_STAT_CHRG_DONE;

	if (reg_status == SYSTEM_STAT_CHRG_DONE) {
		dev_warn(&chip->client->dev, "HW termination happened\n");
		mutex_lock(&chip->event_lock);
		bq24192_enable_hw_term(chip, false);
		bq24192_resume_charging(chip);
		mutex_unlock(&chip->event_lock);
		/* schedule the thread to let the framework know about FULL */
		schedule_delayed_work(&chip->chrg_full_wrkr, 0);
	}

	/* Check if battery fault condition occured. Reading the register
	   value two times to get reliable reg value, recommended by vendor*/
	reg_fault = bq24192_read_reg(chip->client, BQ24192_FAULT_STAT_REG);
	if (reg_fault < 0)
		dev_err(&chip->client->dev, "FAULT register read failed:\n");

	reg_fault = bq24192_read_reg(chip->client, BQ24192_FAULT_STAT_REG);
	if (reg_fault < 0)
		dev_err(&chip->client->dev, "FAULT register read failed:\n");

	dev_info(&chip->client->dev, "FAULT reg %x\n", reg_fault);
	if (reg_fault & FAULT_STAT_WDT_TMR_EXP) {
		dev_warn(&chip->client->dev, "WDT expiration fault\n");

		if (chip->is_charging_enabled) {
			program_timers(chip,
					CHRG_TIMER_EXP_CNTL_WDT160SEC, false);
			mutex_lock(&chip->event_lock);
			bq24192_resume_charging(chip);
			mutex_unlock(&chip->event_lock);
		} else
			dev_info(&chip->client->dev, "No charger connected\n");
	}
	if ((reg_fault & FAULT_STAT_CHRG_TMR_FLT) == FAULT_STAT_CHRG_TMR_FLT) {
		dev_info(&chip->client->dev, "Safety timer expired\n");
	}
	if (reg_status != SYSTEM_STAT_CHRG_DONE)
		power_supply_changed(&chip->usb);

	if (reg_fault & FAULT_STAT_BATT_TEMP_BITS) {
		dev_info(&chip->client->dev,
			"%s:Battery over temp occured!!!!\n", __func__);
		schedule_delayed_work(&chip->chrg_temp_wrkr, 0);
	}
}

/* IRQ handler for charger Interrupts configured to GPIO pin */
static irqreturn_t bq24192_irq_isr(int irq, void *devid)
{
	struct bq24192_chip *chip = (struct bq24192_chip *)devid;

	/**TODO: This hanlder will be used for charger Interrupts */
	dev_dbg(&chip->client->dev,
		"IRQ Handled for charger interrupt: %d\n", irq);

	return IRQ_WAKE_THREAD;
}

/* IRQ handler for charger Interrupts configured to GPIO pin */
static irqreturn_t bq24192_irq_thread(int irq, void *devid)
{
	struct bq24192_chip *chip = (struct bq24192_chip *)devid;

	schedule_delayed_work(&chip->irq_wrkr, 0);
	return IRQ_HANDLED;
}

static void bq24192_task_worker(struct work_struct *work)
{
	struct bq24192_chip *chip =
	    container_of(work, struct bq24192_chip, chrg_task_wrkr.work);
	int ret, jiffy = CHARGER_TASK_JIFFIES, vbatt;
	static int prev_health = POWER_SUPPLY_HEALTH_GOOD;
	int curr_health;
	u8 vindpm = INPUT_SRC_VOLT_LMT_DEF;

	dev_info(&chip->client->dev, "%s\n", __func__);

	/* Reset the WDT */
	mutex_lock(&chip->event_lock);
	ret = reset_wdt_timer(chip);
	mutex_unlock(&chip->event_lock);
	if (ret < 0)
		dev_warn(&chip->client->dev, "WDT reset failed:\n");

	/*
	 * If we have an OTG device connected, no need to modify the VINDPM
	 * check for Hi-Z
	 */
	if (chip->boost_mode) {
		jiffy = CHARGER_HOST_JIFFIES;
		goto sched_task_work;
	}

#ifndef CONFIG_RAW_CC_THROTTLE
	if (!(chip->cntl_state == 0x3)) {
		/* Clear the charger from Hi-Z */
		ret = bq24192_clear_hiz(chip);
		if (ret < 0)
			dev_warn(&chip->client->dev, "HiZ clear failed:\n");
	}
#else
	ret = bq24192_clear_hiz(chip);
	if (ret < 0)
		dev_warn(&chip->client->dev, "HiZ clear failed:\n");
#endif

	/* Modify the VINDPM */

	/* read the battery voltage */
	vbatt = fg_chip_get_property(POWER_SUPPLY_PROP_VOLTAGE_NOW);
	if (vbatt == -ENODEV || vbatt == -EINVAL) {
		dev_err(&chip->client->dev, "Can't read voltage from FG\n");
		goto sched_task_work;
	}

	/* convert voltage into millivolts */
	vbatt /= 1000;
	dev_warn(&chip->client->dev, "vbatt = %d\n", vbatt);


	/* The charger vindpm voltage changes are causing charge current
	 * throttle resulting in a prolonged changing time.
	 * Hence disabling dynamic vindpm update  for bq24296 chip.
	*/
	if (chip->chip_type != BQ24296) {
		if (vbatt > INPUT_SRC_LOW_VBAT_LIMIT &&
			vbatt <= INPUT_SRC_MIDL_VBAT_LIMIT1)
			vindpm = INPUT_SRC_VOLT_LMT_412;
		else if (vbatt > INPUT_SRC_MIDL_VBAT_LIMIT1 &&
			vbatt <= INPUT_SRC_MIDL_VBAT_LIMIT2)
			vindpm = INPUT_SRC_VOLT_LMT_444;
		else if (vbatt > INPUT_SRC_MIDL_VBAT_LIMIT2)
			vindpm = INPUT_SRC_VOLT_LMT_460;

		mutex_lock(&chip->event_lock);
		ret = bq24192_modify_vindpm(vindpm);
		if (ret < 0)
			dev_err(&chip->client->dev, "%s failed\n", __func__);
		mutex_unlock(&chip->event_lock);
	}

	/*
	 * BQ driver depends upon the charger interrupt to send notification
	 * to the framework about the HW charge termination and then framework
	 * starts to poll the driver for declaring FULL. Presently the BQ
	 * interrupts are not coming properly, so the driver would notify the
	 * framework when battery is nearing FULL.
	*/
	if (vbatt >= BATTERY_NEAR_FULL(chip->max_cv))
		power_supply_changed(NULL);

sched_task_work:

	curr_health = bq24192_get_battery_health();
	if (prev_health != curr_health) {
		power_supply_changed(&chip->usb);
		dev_warn(&chip->client->dev,
			"%s health status  %d",
				__func__, prev_health);
	}
	prev_health = curr_health;

	if (BQ24192_CHRGR_STAT_BAT_FULL == bq24192_is_charging(chip)) {
		power_supply_changed(&chip->usb);
		dev_warn(&chip->client->dev,
			"%s battery full", __func__);
	}

	schedule_delayed_work(&chip->chrg_task_wrkr, jiffy);
}

static void bq24192_temp_update_worker(struct work_struct *work)
{
	struct bq24192_chip *chip =
	    container_of(work, struct bq24192_chip, chrg_temp_wrkr.work);
	int fault_reg = 0, fg_temp = 0;
	static bool is_otp_notified;

	dev_info(&chip->client->dev, "%s\n", __func__);
	/* Check if battery fault condition occured. Reading the register
	   value two times to get reliable reg value, recommended by vendor*/
	fault_reg = bq24192_read_reg(chip->client, BQ24192_FAULT_STAT_REG);
	if (fault_reg < 0) {
		dev_err(&chip->client->dev,
			"Fault status read failed: %d\n", fault_reg);
		goto temp_wrkr_error;
	}
	fault_reg = bq24192_read_reg(chip->client, BQ24192_FAULT_STAT_REG);
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

	if (fg_temp >= chip->pdata->max_temp
		|| fg_temp <= chip->pdata->min_temp) {
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

static int bq24192_usb_notify_handle(struct notifier_block *nb,
					unsigned long event, void *param)
{
	/*
	 * If required check the event ID returned from OTG
	 * and updated the cable property and notify
	 * power supply class
	 */

	return NOTIFY_OK;
}

static void bq24192_usb_otg_enable(struct usb_phy *phy, int on)
{
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);
	int ret, id_value = -1;

	mutex_lock(&chip->event_lock);
	ret =  bq24192_turn_otg_vbus(chip, on);
	mutex_unlock(&chip->event_lock);
	if (ret < 0)
		dev_err(&chip->client->dev, "VBUS mode(%d) failed\n", on);
}

static inline int register_otg_vbus(struct bq24192_chip *chip)
{
	int ret;

	chip->transceiver = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!chip->transceiver) {
		dev_err(&chip->client->dev, "Failed to get the USB transceiver\n");
		return -EINVAL;
	}
	chip->transceiver->set_vbus = bq24192_usb_otg_enable;

	if (chip->chip_type == BQ24297) {
		chip->otg_notify.notifier_call = bq24192_usb_notify_handle;
		ret = usb_register_notifier(chip->transceiver,
							&chip->otg_notify);
		if (ret) {
			dev_err(&chip->client->dev,
						"Failed to register OTG notifier\n");
			return ret;
		}
	}
	return 0;
}

int bq24192_slave_mode_enable_charging(int volt, int cur, int ilim)
{
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);
	int ret;

	mutex_lock(&chip->event_lock);
	chip->inlmt = ilim;
	if (chip->inlmt >= 0)
		bq24192_set_inlmt(chip, chip->inlmt);
	mutex_unlock(&chip->event_lock);

	chip->cc = chrg_cur_to_reg(cur);
	if (chip->cc)
		bq24192_set_cc(chip, chip->cc);

	chip->cv = chrg_volt_to_reg(volt);
	if (chip->cv)
		bq24192_set_cv(chip, chip->cv);

	mutex_lock(&chip->event_lock);
	ret = bq24192_enable_charging(chip, true);
	if (ret < 0)
		dev_err(&chip->client->dev, "charge enable failed\n");

	mutex_unlock(&chip->event_lock);
	return ret;
}
EXPORT_SYMBOL(bq24192_slave_mode_enable_charging);

int bq24192_slave_mode_disable_charging(void)
{
	struct bq24192_chip *chip = i2c_get_clientdata(bq24192_client);
	int ret;

	mutex_lock(&chip->event_lock);
	ret = bq24192_enable_charging(chip, false);
	if (ret < 0)
		dev_err(&chip->client->dev, "charge enable failed\n");

	mutex_unlock(&chip->event_lock);
	return ret;
}
static int bq24192_get_chip_version(struct bq24192_chip *chip)
{
	int ret;

	/* check chip model number */
	ret = bq24192_read_reg(chip->client, BQ24192_VENDER_REV_REG);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c read err:%d\n", ret);
		return -EIO;
	}
	dev_info(&chip->client->dev, "version reg:%x\n", ret);

	ret = ret >> 3;
	switch (ret) {
	case BQ24190_IC_VERSION:
		chip->chip_type = BQ24190;
		break;
	case BQ24191_IC_VERSION:
		chip->chip_type = BQ24191;
		break;
	case BQ24192_IC_VERSION:
		chip->chip_type = BQ24192;
		break;
	case BQ24192I_IC_VERSION:
		chip->chip_type = BQ24192I;
		break;
	case BQ24296_IC_VERSION:
		chip->chip_type = BQ24296;
		break;
	case BQ24297_IC_VERSION:
		chip->chip_type = BQ24297;
		break;
	default:
		dev_err(&chip->client->dev,
			"device version mismatch: %x\n", ret);
		return -EIO;
	}

	dev_info(&chip->client->dev, "chip type:%x\n", chip->chip_type);
	return 0;
}

static int bq24192_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	const struct acpi_device_id *acpi_id;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq24192_chip *chip;
	struct device *dev;
	struct gpio_desc *gpio;
	int ret;


	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"SMBus doesn't support BYTE transactions\n");
		return -EIO;
	}

	chip = kzalloc(sizeof(struct bq24192_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	chip->client = client;
	if (id) {
		chip->pdata = (struct bq24192_platform_data *)id->driver_data;
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

		chip->pdata = (struct bq24192_platform_data *)
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
	bq24192_client = client;

	/* check chip model number */
	ret = bq24192_get_chip_version(chip);
	if (ret < 0) {
		dev_err(&client->dev, "i2c read err:%d\n", ret);
		i2c_set_clientdata(client, NULL);
		kfree(chip);
		return -EIO;
	}

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
	/*
	 * Request for charger chip gpio.This will be used to
	 * register for an interrupt handler for servicing charger
	 * interrupts
	 */
	if (client->irq > 0) {
		chip->irq = client->irq;
		dev_dbg(dev, "CHRG INT Client IRQ# = %d\n", client->irq);
	} else {
#ifdef CONFIG_ACPI

		gpio = devm_gpiod_get_index(dev, "bq24192_int", 0);
		if (IS_ERR(gpio)) {
			dev_err(dev, "acpi gpio get index failed\n");
			i2c_set_clientdata(client, NULL);
			kfree(chip);
			return PTR_ERR(gpio);
		} else {
			ret = gpiod_direction_input(gpio);
			if (ret < 0)
				dev_err(dev, "BQ CHRG set direction failed\n");

			chip->irq = gpiod_to_irq(gpio);
		}

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
				bq24192_irq_isr, bq24192_irq_thread,
				IRQF_TRIGGER_FALLING, "BQ24192", chip);
		if (ret) {
			dev_warn(&bq24192_client->dev,
				"failed to register irq for pin %d\n",
				chip->irq);
		} else {
			dev_warn(&bq24192_client->dev,
				"registered charger irq for pin %d\n",
				chip->irq);
		}
	}
#ifdef CONFIG_ACPI
	chip->switch_io = devm_gpiod_get_index(dev, "bq24192_USB_MUX_IO", 1);
	if (IS_ERR_OR_NULL(chip->switch_io)) {
			dev_err(dev, "Failed to get the Switch IO Pin\n");
			chip->switch_io = NULL;
	} else {
		ret = gpiod_direction_output(chip->switch_io, 0);
		if (ret < 0)
			dev_err(dev, "failed to set OUT direction for BQ switch IO\n");
	}
	if (chip->switch_io)
		gpiod_set_value(chip->switch_io,
					BQ_GPIO_MUX_SEL_PMIC);
#endif

	INIT_DELAYED_WORK(&chip->chrg_full_wrkr, bq24192_full_worker);
	INIT_DELAYED_WORK(&chip->chrg_task_wrkr, bq24192_task_worker);
	INIT_DELAYED_WORK(&chip->chrg_temp_wrkr, bq24192_temp_update_worker);
	INIT_DELAYED_WORK(&chip->irq_wrkr, bq24192_irq_worker);
	mutex_init(&chip->event_lock);

	/* Initialize the wakelock */
	wake_lock_init(&chip->wakelock, WAKE_LOCK_SUSPEND,
						"ctp_charger_wakelock");
	init_completion(&chip->vbus_detect);

	/* register bq24192 usb with power supply subsystem */
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
		chip->chgr_stat = BQ24192_CHRGR_STAT_UNKNOWN;
		chip->usb.properties = bq24192_usb_props;
		chip->usb.num_properties = ARRAY_SIZE(bq24192_usb_props);
		chip->usb.get_property = bq24192_usb_get_property;
		chip->usb.set_property = bq24192_usb_set_property;
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
	ret = bq24192_create_debugfs(chip);
	if (ret < 0) {
		dev_err(&client->dev, "debugfs create failed\n");
		power_supply_unregister(&chip->usb);
		i2c_set_clientdata(client, NULL);
		kfree(chip);
		return ret;
	}

	/*
	 * Register to get USB transceiver events
	 */
	ret = register_otg_vbus(chip);
	if (ret) {
		dev_err(&chip->client->dev,
					"REGISTER OTG VBUS failed\n");
	}

	if (chip->chip_type == BQ24297) {
		/*
		 * For BQ24297, check the initial cable status.
		 * During Probe, a charger cable might have been
		 * connected prior to the power on of the device.
		 * Hence deliberately trigger a DPDM detection
		 * during probe so that the BQ charger will detect the
		 * charger cable connected before hand and will trigger
		 * an interrupt accordingly.
		 */
		ret = bq24192_reg_read_modify(chip->client,
			BQ24192_MISC_OP_CNTL_REG, MISC_OP_CNTL_DPDM_EN, true);
		if (ret < 0)
			dev_warn(&chip->client->dev,
				"Failed to trigger DPDM detection during probe\n");
	}

	return 0;
}

static int bq24192_remove(struct i2c_client *client)
{
	struct bq24192_chip *chip = i2c_get_clientdata(client);

	bq24192_remove_debugfs(chip);

	if (!chip->pdata->slave_mode)
		power_supply_unregister(&chip->usb);

	if (chip->irq > 0)
		free_irq(chip->irq, chip);

	i2c_set_clientdata(client, NULL);
	wake_lock_destroy(&chip->wakelock);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM
static int bq24192_suspend(struct device *dev)
{
	struct bq24192_chip *chip = dev_get_drvdata(dev);

	dev_dbg(&chip->client->dev, "bq24192 suspend\n");
	return 0;
}

static int bq24192_resume(struct device *dev)
{
	struct bq24192_chip *chip = dev_get_drvdata(dev);

	dev_dbg(&chip->client->dev, "bq24192 resume\n");
	return 0;
}
#else
#define bq24192_suspend	NULL
#define bq24192_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int bq24192_runtime_suspend(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq24192_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq24192_runtime_idle(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#else
#define bq24192_runtime_suspend	NULL
#define bq24192_runtime_resume	NULL
#define bq24192_runtime_idle	NULL
#endif

char *bq24192_supplied_to[] = {
	"max170xx_battery",
	"max17042_battery",
	"max17047_battery",
	"intel_fuel_gauge",
};

struct bq24192_platform_data tbg24296_drvdata = {
	.throttle_states = byt_throttle_states,
	.supplied_to = bq24192_supplied_to,
	.num_throttle_states = ARRAY_SIZE(byt_throttle_states),
	.num_supplicants = ARRAY_SIZE(bq24192_supplied_to),
	.supported_cables = POWER_SUPPLY_CHARGER_TYPE_USB,
	.sfi_tabl_present = true,
	.max_cc = 1800,	/* 1800 mA */
	.max_cv = 4350,	/* 4350 mV */
	.max_temp = 45,	/* 45 DegC */
	.min_temp = 0,	/* 0 DegC */
};

static const struct i2c_device_id bq24192_id[] = {
	{ "bq24192", },
	{ "TBQ24296", (kernel_ulong_t)&tbg24296_drvdata},
	{ "TBQ24297", (kernel_ulong_t)&tbg24296_drvdata},
	{ "ext-charger", (kernel_ulong_t)&tbg24296_drvdata},
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq24192_id);

#ifdef CONFIG_ACPI
static struct acpi_device_id bq24192_acpi_match[] = {
	{"TBQ24296", (kernel_ulong_t)&tbg24296_drvdata},
	{"TBQ24297", (kernel_ulong_t)&tbg24296_drvdata},
	{}
};
MODULE_DEVICE_TABLE(acpi, bq24192_acpi_match);
#endif

static const struct dev_pm_ops bq24192_pm_ops = {
	.suspend		= bq24192_suspend,
	.resume			= bq24192_resume,
	.runtime_suspend	= bq24192_runtime_suspend,
	.runtime_resume		= bq24192_runtime_resume,
	.runtime_idle		= bq24192_runtime_idle,
};

static struct i2c_driver bq24192_i2c_driver = {
	.driver	= {
		.name	= DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &bq24192_pm_ops,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(bq24192_acpi_match),
#endif
	},
	.probe		= bq24192_probe,
	.remove		= bq24192_remove,
	.id_table	= bq24192_id,
};

static int __init bq24192_init(void)
{
	return i2c_add_driver(&bq24192_i2c_driver);
}
module_init(bq24192_init);

static void __exit bq24192_exit(void)
{
	i2c_del_driver(&bq24192_i2c_driver);
}
module_exit(bq24192_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_AUTHOR("Raj Pandey <raj.pandey@intel.com>");
MODULE_DESCRIPTION("BQ24192 Charger Driver");
MODULE_LICENSE("GPL");
