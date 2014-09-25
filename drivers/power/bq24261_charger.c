/*
 * bq24261_charger.c - BQ24261 Charger I2C client driver
 *
 * Copyright (C) 2014 Intel Corporation
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
 * You should have received a copy of the GNU General Public License along
 * with this program;
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Jenny TC <jenny.tc@intel.com>
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/usb/otg.h>
#include <linux/power/bq24261_charger.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>

#include <asm/intel_scu_ipc.h>

#define DEV_NAME "bq24261_charger"
#define DEV_MANUFACTURER "TI"
#define MODEL_NAME_SIZE 8
#define DEV_MANUFACTURER_NAME_SIZE 4

#define CHRG_TERM_WORKER_DELAY (30 * HZ)
#define EXCEPTION_MONITOR_DELAY (60 * HZ)
#define WDT_RESET_DELAY (15 * HZ)

/* BQ24261 registers */
#define BQ24261_STAT_CTRL0_ADDR		0x00
#define BQ24261_CTRL_ADDR		0x01
#define BQ24261_BATT_VOL_CTRL_ADDR	0x02
#define BQ24261_VENDOR_REV_ADDR		0x03
#define BQ24261_TERM_FCC_ADDR		0x04
#define BQ24261_VINDPM_STAT_ADDR	0x05
#define BQ24261_ST_NTC_MON_ADDR		0x06

#define BQ24261_RESET_MASK		(0x01 << 7)
#define BQ24261_RESET_ENABLE		(0x01 << 7)

#define BQ24261_FAULT_MASK		0x07
#define BQ24261_STAT_MASK		(0x03 << 4)
#define BQ24261_BOOST_MASK		(0x01 << 6)
#define BQ24261_TMR_RST_MASK		(0x01 << 7)
#define BQ24261_TMR_RST			(0x01 << 7)

#define BQ24261_ENABLE_BOOST		(0x01 << 6)

#define BQ24261_VOVP			0x01
#define BQ24261_LOW_SUPPLY		0x02
#define BQ24261_THERMAL_SHUTDOWN	0x03
#define BQ24261_BATT_TEMP_FAULT		0x04
#define BQ24261_TIMER_FAULT		0x05
#define BQ24261_BATT_OVP		0x06
#define BQ24261_NO_BATTERY		0x07
#define BQ24261_STAT_READY		0x00

#define BQ24261_STAT_CHRG_PRGRSS	(0x01 << 4)
#define BQ24261_STAT_CHRG_DONE		(0x02 << 4)
#define BQ24261_STAT_FAULT		(0x03 << 4)

#define BQ24261_CE_MASK			(0x01 << 1)
#define BQ24261_CE_DISABLE		(0x01 << 1)

#define BQ24261_HiZ_MASK			(0x01)
#define BQ24261_HiZ_ENABLE		(0x01)

#define BQ24261_ICHRG_MASK		(0x1F << 3)

#define BQ24261_ITERM_MASK		(0x03)
#define BQ24261_MIN_ITERM 50 /* 50 mA */
#define BQ24261_MAX_ITERM 300 /* 300 mA */

#define BQ24261_VBREG_MASK		(0x3F << 2)

#define BQ24261_INLMT_MASK		(0x03 << 4)
#define BQ24261_INLMT_100		0x00
#define BQ24261_INLMT_150		(0x01 << 4)
#define BQ24261_INLMT_500		(0x02 << 4)
#define BQ24261_INLMT_900		(0x03 << 4)
#define BQ24261_INLMT_1500		(0x04 << 4)
#define BQ24261_INLMT_2000		(0x05 << 4)
#define BQ24261_INLMT_2500		(0x06 << 4)

#define BQ24261_TE_MASK			(0x01 << 2)
#define BQ24261_TE_ENABLE		(0x01 << 2)
#define BQ24261_STAT_ENABLE_MASK	(0x01 << 3)
#define BQ24261_STAT_ENABLE		(0x01 << 3)

#define BQ24261_VENDOR_MASK		(0x07 << 5)
#define BQ24261_VENDOR			(0x02 << 5)
#define BQ24261_REV_MASK		(0x07)
#define BQ24261_2_3_REV			(0x06)
#define BQ24261_REV			(0x02)
#define BQ24260_REV			(0x01)

#define BQ24261_TS_MASK			(0x01 << 3)
#define BQ24261_TS_ENABLED		(0x01 << 3)
#define BQ24261_BOOST_ILIM_MASK		(0x01 << 4)
#define BQ24261_BOOST_ILIM_500ma	(0x0)
#define BQ24261_BOOST_ILIM_1A		(0x01 << 4)
#define BQ24261_VINDPM_OFF_MASK		(0x01 << 0)
#define BQ24261_VINDPM_OFF_5V		(0x0)
#define BQ24261_VINDPM_OFF_12V		(0x01 << 0)

#define BQ24261_SAFETY_TIMER_MASK	(0x03 << 5)
#define BQ24261_SAFETY_TIMER_40MIN	0x00
#define BQ24261_SAFETY_TIMER_6HR	(0x01 << 5)
#define BQ24261_SAFETY_TIMER_9HR	(0x02 << 5)
#define BQ24261_SAFETY_TIMER_DISABLED	(0x03 << 5)

/* 1% above voltage max design to report over voltage */
#define BQ24261_OVP_MULTIPLIER			1010
#define BQ24261_OVP_RECOVER_MULTIPLIER		990
#define BQ24261_DEF_BAT_VOLT_MAX_DESIGN		4200000

/* Settings for Voltage / DPPM Register (05) */
#define BQ24261_VBATT_LEVEL1		3700000
#define BQ24261_VBATT_LEVEL2		3960000
#define BQ24261_VINDPM_MASK		(0x07)
#define BQ24261_VINDPM_320MV		(0x01 << 2)
#define BQ24261_VINDPM_160MV		(0x01 << 1)
#define BQ24261_VINDPM_80MV		(0x01 << 0)
#define BQ24261_CD_STATUS_MASK		(0x01 << 3)
#define BQ24261_DPM_EN_MASK		(0x01 << 4)
#define BQ24261_DPM_EN_FORCE		(0x01 << 4)
#define BQ24261_LOW_CHG_MASK		(0x01 << 5)
#define BQ24261_LOW_CHG_EN		(0x01 << 5)
#define BQ24261_LOW_CHG_DIS		(~BQ24261_LOW_CHG_EN)
#define BQ24261_DPM_STAT_MASK		(0x01 << 6)
#define BQ24261_MINSYS_STAT_MASK	(0x01 << 7)

#define BQ24261_MIN_CC			500 /* 500mA */
#define BQ24261_MAX_CC			3000 /* 3A */
#define BQ24261_MED_CC			1500 /* 1A */

#define BQ24261_INT_COUNTER "bq24261_irq_counter"

u16 bq24261_sfty_tmr[][2] = {
	{0, BQ24261_SAFETY_TIMER_DISABLED}
	,
	{40, BQ24261_SAFETY_TIMER_40MIN}
	,
	{360, BQ24261_SAFETY_TIMER_6HR}
	,
	{540, BQ24261_SAFETY_TIMER_9HR}
	,
};


u16 bq24261_inlmt[][2] = {
	{100, BQ24261_INLMT_100}
	,
	{150, BQ24261_INLMT_150}
	,
	{500, BQ24261_INLMT_500}
	,
	{900, BQ24261_INLMT_900}
	,
	{1500, BQ24261_INLMT_1500}
	,
	{2000, BQ24261_INLMT_2000}
	,
	{2500, BQ24261_INLMT_2500}
	,
};

#define BQ24261_MIN_CV 3500
#define BQ24261_MAX_CV 4440
#define BQ24261_CV_DIV 20
#define BQ24261_CV_BIT_POS 2

static enum power_supply_property bq24261_usb_props[] = {
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
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MAX_TEMP,
	POWER_SUPPLY_PROP_MIN_TEMP,
};

enum bq24261_chrgr_stat {
	BQ24261_CHRGR_STAT_UNKNOWN,
	BQ24261_CHRGR_STAT_READY,
	BQ24261_CHRGR_STAT_CHARGING,
	BQ24261_CHRGR_STAT_BAT_FULL,
	BQ24261_CHRGR_STAT_FAULT,
};

struct bq24261_otg_event {
	struct list_head node;
	bool is_enable;
};

struct bq24261_charger {

	struct mutex stat_lock;
	struct i2c_client *client;
	struct bq24261_plat_data *pdata;
	struct power_supply psy_usb;
	struct delayed_work sw_term_work;
	struct delayed_work wdt_work;
	struct delayed_work low_supply_fault_work;
	struct delayed_work exception_mon_work;
	struct notifier_block otg_nb;
	struct usb_phy *transceiver;
	struct work_struct otg_work;
	struct work_struct irq_work;
	struct list_head otg_queue;
	struct list_head irq_queue;
	wait_queue_head_t wait_ready;
	spinlock_t otg_queue_lock;

	int chrgr_health;
	int bat_health;
	int cc;
	int cv;
	int inlmt;
	int max_cc;
	int max_cv;
	int iterm;
	int cable_type;
	int cntl_state;
	int max_temp;
	int min_temp;
	int revision;
	enum bq24261_chrgr_stat chrgr_stat;
	bool online;
	bool present;
	bool is_charging_enabled;
	bool is_charger_enabled;
	bool is_vsys_on;
	bool boost_mode;
	bool is_hw_chrg_term;
	char model_name[MODEL_NAME_SIZE];
	char manufacturer[DEV_MANUFACTURER_NAME_SIZE];
	struct wake_lock chrgr_en_wakelock;
	u32 irq_counter;
};

enum bq2426x_model_num {
	BQ2426X = 0,
	BQ24260,
	BQ24261,
};

struct bq2426x_model {
	char model_name[MODEL_NAME_SIZE];
	enum bq2426x_model_num model;
};

static struct bq2426x_model bq24261_model_name[] = {
	{ "bq2426x", BQ2426X },
	{ "bq24260", BQ24260 },
	{ "bq24261", BQ24261 },
};

static struct power_supply_throttle bq24261_throttle_states[] = {
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BQ24261_MAX_CC,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BQ24261_MED_CC,
	},
	{
		.throttle_action = PSY_THROTTLE_CC_LIMIT,
		.throttle_val = BQ24261_MIN_CC
,
	},
	{
		.throttle_action = PSY_THROTTLE_DISABLE_CHARGING,
	},
};

struct i2c_client *bq24261_client;
static inline int get_battery_voltage(int *volt);
static inline int get_battery_current(int *cur);
static int bq24261_handle_irq(struct bq24261_charger *chip, u8 stat_reg);
static inline int bq24261_set_iterm(struct bq24261_charger *chip, int iterm);

enum power_supply_type get_power_supply_type(
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
	case POWER_SUPPLY_CHARGER_TYPE_SE1:
		return POWER_SUPPLY_TYPE_USB_DCP;
	case POWER_SUPPLY_CHARGER_TYPE_NONE:
	case POWER_SUPPLY_CHARGER_TYPE_USB_SDP:
	default:
		return POWER_SUPPLY_TYPE_USB;
	}

	return POWER_SUPPLY_TYPE_USB;
}

static void lookup_regval(u16 tbl[][2], size_t size, u16 in_val, u8 *out_val)
{
	int i;
	for (i = 1; i < size; ++i)
		if (in_val < tbl[i][0])
			break;

	*out_val = (u8) tbl[i - 1][1];
}

void bq24261_cc_to_reg(int cc, u8 *reg_val)
{
	/* Ichrg bits are B3-B7
	 * Icharge = 500mA + IchrgCode * 100mA
	 */
	cc = clamp_t(int, cc, BQ24261_MIN_CC, BQ24261_MAX_CC);
	cc = cc - BQ24261_MIN_CC;
	*reg_val = (cc / 100) << 3;
}

void bq24261_cv_to_reg(int cv, u8 *reg_val)
{
	int val;

	val = clamp_t(int, cv, BQ24261_MIN_CV, BQ24261_MAX_CV);
	*reg_val =
		(((val - BQ24261_MIN_CV) / BQ24261_CV_DIV)
			<< BQ24261_CV_BIT_POS);
}

void bq24261_inlmt_to_reg(int inlmt, u8 *regval)
{
	return lookup_regval(bq24261_inlmt, ARRAY_SIZE(bq24261_inlmt),
			     inlmt, regval);
}

static inline void bq24261_iterm_to_reg(int iterm, u8 *regval)
{
	/* Iterm bits are B0-B2
	 * Icharge = 50mA + ItermCode * 50mA
	 */
	iterm = clamp_t(int, iterm, BQ24261_MIN_ITERM,  BQ24261_MAX_ITERM);
	iterm = iterm - BQ24261_MIN_ITERM;
	*regval =  iterm / 50;
}

static inline void bq24261_sfty_tmr_to_reg(int tmr, u8 *regval)
{
	return lookup_regval(bq24261_sfty_tmr, ARRAY_SIZE(bq24261_sfty_tmr),
			     tmr, regval);
}

static inline int bq24261_read_reg(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "Error(%d) in reading reg %d\n", ret,
			reg);

	return ret;
}


static inline void bq24261_dump_regs(bool dump_master)
{
	int i;
	int ret;
	int bat_cur, bat_volt;
	struct bq24261_charger *chip;
	char buf[1024] = {0};
	int used = 0;

	if (!bq24261_client)
		return;

	chip = i2c_get_clientdata(bq24261_client);

	dev_info(&bq24261_client->dev, "*======================*\n");
	ret = get_battery_current(&bat_cur);
	if (ret)
		dev_err(&bq24261_client->dev,
			"%s: Error in getting battery current", __func__);
	else
		dev_info(&bq24261_client->dev, "Battery Current=%dma\n",
				(bat_cur/1000));

	ret = get_battery_voltage(&bat_volt);
	if (ret)
		dev_err(&bq24261_client->dev,
			"%s: Error in getting battery voltage", __func__);
	else
		dev_info(&bq24261_client->dev, "Battery VOlatge=%dmV\n",
			(bat_volt/1000));


	dev_info(&bq24261_client->dev, "BQ24261 Register dump:\n");

	for (i = 0; i < 7; ++i) {
		ret = bq24261_read_reg(bq24261_client, i);
		if (ret < 0)
			dev_err(&bq24261_client->dev,
				"Error in reading REG 0x%X\n", i);
		else
			used += snprintf(buf + used, sizeof(buf) - used,
					" 0x%X=0x%X,", i, ret);
	}
	dev_info(&bq24261_client->dev, "%s\n", buf);
	dev_info(&bq24261_client->dev, "*======================*\n");
}


#ifdef CONFIG_DEBUG_FS
static int bq24261_reg_show(struct seq_file *seq, void *unused)
{
	int val;
	u8 reg;

	reg = *((u8 *)seq->private);
	val = bq24261_read_reg(bq24261_client, reg);

	seq_printf(seq, "0x%02x\n", val);
	return 0;
}

static int bq24261_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq24261_reg_show, inode->i_private);
}

static u32 bq24261_register_set[] = {
	BQ24261_STAT_CTRL0_ADDR,
	BQ24261_CTRL_ADDR,
	BQ24261_BATT_VOL_CTRL_ADDR,
	BQ24261_VENDOR_REV_ADDR,
	BQ24261_TERM_FCC_ADDR,
	BQ24261_VINDPM_STAT_ADDR,
	BQ24261_ST_NTC_MON_ADDR,
};

static struct dentry *bq24261_dbgfs_dir;

static const struct file_operations bq24261_dbg_fops = {
	.open = bq24261_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static void bq24261_debugfs_init(void)
{
	struct dentry *fentry;
	u32 count = ARRAY_SIZE(bq24261_register_set);
	struct bq24261_charger *chip = i2c_get_clientdata(bq24261_client);
	u32 i;
	char name[6] = {0};

	bq24261_dbgfs_dir = debugfs_create_dir(DEV_NAME, NULL);
	if (bq24261_dbgfs_dir == NULL)
		goto debugfs_root_exit;

	for (i = 0; i < count; i++) {
		snprintf(name, 6, "%02x", bq24261_register_set[i]);
		fentry = debugfs_create_file(name, S_IRUGO,
						bq24261_dbgfs_dir,
						&bq24261_register_set[i],
						&bq24261_dbg_fops);
		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	fentry = debugfs_create_u32(BQ24261_INT_COUNTER, S_IRUGO,
			bq24261_dbgfs_dir, &chip->irq_counter);

	if (fentry == NULL)
		goto debugfs_err_exit;

	dev_err(&bq24261_client->dev, "Debugfs created successfully!!\n");
	return;

debugfs_err_exit:
	debugfs_remove_recursive(bq24261_dbgfs_dir);
debugfs_root_exit:
	dev_err(&bq24261_client->dev, "Error Creating debugfs!!\n");
	return;
}

static void bq24261_debugfs_exit(void)
{
	debugfs_remove_recursive(bq24261_dbgfs_dir);

	return;
}

#else
static void bq24261_debugfs_init(void)
{
	return;
}

static void bq24261_debugfs_exit(void)
{
	return;
}
#endif

static inline int bq24261_write_reg(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0)
		dev_err(&client->dev, "Error(%d) in writing %d to reg %d\n",
			ret, data, reg);

	return ret;
}

static inline int bq24261_read_modify_reg(struct i2c_client *client, u8 reg,
					  u8 mask, u8 val)
{
	int ret;

	ret = bq24261_read_reg(client, reg);
	if (ret < 0)
		return ret;
	ret = (ret & ~mask) | (mask & val);
	return bq24261_write_reg(client, reg, ret);
}

static inline int bq24261_tmr_ntc_init(struct bq24261_charger *chip)
{
	u8 reg_val;
	int ret;

	bq24261_sfty_tmr_to_reg(chip->pdata->safety_timer, &reg_val);

	if (chip->pdata->is_ts_enabled)
		reg_val |= BQ24261_TS_ENABLED;

	/* Check if boost mode current configuration is above 1A*/
	if (chip->pdata->boost_mode_ma >= 1000)
		reg_val |= BQ24261_BOOST_ILIM_1A;

	ret = bq24261_read_modify_reg(chip->client, BQ24261_ST_NTC_MON_ADDR,
			BQ24261_TS_MASK|BQ24261_SAFETY_TIMER_MASK|
			BQ24261_BOOST_ILIM_MASK, reg_val);

	return ret;
}

static inline int bq24261_enable_charging(
	struct bq24261_charger *chip, bool val)
{
	int ret;
	u8 reg_val;
	bool is_ready;

	dev_dbg(&chip->client->dev, "%s=%d\n", __func__, val);
	ret = bq24261_read_reg(chip->client,
					BQ24261_STAT_CTRL0_ADDR);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Error(%d) in reading BQ24261_STAT_CTRL0_ADDR\n", ret);
	}

	is_ready =  (ret & BQ24261_STAT_MASK) != BQ24261_STAT_FAULT;

	/* If status is fault, wait for READY before enabling the charging */
	if (!is_ready && val) {
		ret = wait_event_timeout(chip->wait_ready,
			(chip->chrgr_stat == BQ24261_CHRGR_STAT_READY),
				HZ);
		dev_info(&chip->client->dev,
			"chrgr_stat=%x\n", chip->chrgr_stat);
		if (ret == 0) {
			dev_err(&chip->client->dev,
				"ChgrReady timeout, enable charging anyway\n");
		}
	}

	if (chip->pdata->enable_charging) {
		ret = chip->pdata->enable_charging(val);
		if (ret) {
			dev_err(&chip->client->dev,
				"Error(%d) in master enable-charging\n", ret);
		}
	}

	if (val) {
		reg_val = (~BQ24261_CE_DISABLE & BQ24261_CE_MASK);
		if (chip->is_hw_chrg_term)
			reg_val |= BQ24261_TE_ENABLE;
	} else {
		reg_val = BQ24261_CE_DISABLE;
	}

	reg_val |=  BQ24261_STAT_ENABLE;

	ret = bq24261_read_modify_reg(chip->client, BQ24261_CTRL_ADDR,
		       BQ24261_STAT_ENABLE_MASK|BQ24261_RESET_MASK|
				BQ24261_CE_MASK|BQ24261_TE_MASK,
					reg_val);
	if (ret || !val)
		return ret;

	bq24261_set_iterm(chip, chip->iterm);
	ret = bq24261_tmr_ntc_init(chip);
	if (ret) {
		dev_err(&chip->client->dev,
			"Error(%d) in tmr_ntc_init\n", ret);
	}

	dev_info(&chip->client->dev, "Completed %s=%d\n", __func__, val);
	bq24261_dump_regs(false);

	return ret;
}

static inline int bq24261_reset_timer(struct bq24261_charger *chip)
{
	return bq24261_read_modify_reg(chip->client, BQ24261_STAT_CTRL0_ADDR,
			BQ24261_TMR_RST_MASK, BQ24261_TMR_RST);
}

static inline int bq24261_enable_charger(
	struct bq24261_charger *chip, int val)
{

	/* TODO: Implement enable/disable HiZ mode to enable/
	*  disable charger
	*/
	u8 reg_val;
	int ret;

	dev_dbg(&chip->client->dev, "%s=%d\n", __func__, val);
	reg_val = val ? (~BQ24261_HiZ_ENABLE & BQ24261_HiZ_MASK)  :
			BQ24261_HiZ_ENABLE;

	ret = bq24261_read_modify_reg(chip->client, BQ24261_CTRL_ADDR,
		       BQ24261_HiZ_MASK|BQ24261_RESET_MASK, reg_val);
	if (ret)
		return ret;

	return bq24261_reset_timer(chip);
}

static inline int bq24261_set_cc(struct bq24261_charger *chip, int cc_mA)
{
	u8 reg_val;
	int ret;

	dev_dbg(&chip->client->dev, "%s=%d\n", __func__, cc_mA);
	if (chip->pdata->set_cc) {
		ret = chip->pdata->set_cc(cc_mA);
		if (unlikely(ret))
			return ret;
	}

	if (cc_mA && (cc_mA < BQ24261_MIN_CC)) {
		dev_dbg(&chip->client->dev, "Set LOW_CHG bit\n");
		reg_val = BQ24261_LOW_CHG_EN;
		ret = bq24261_read_modify_reg(chip->client,
				BQ24261_VINDPM_STAT_ADDR,
				BQ24261_LOW_CHG_MASK, reg_val);
	} else {
		dev_dbg(&chip->client->dev, "Clear LOW_CHG bit\n");
		reg_val = BQ24261_LOW_CHG_DIS;
		ret = bq24261_read_modify_reg(chip->client,
				BQ24261_VINDPM_STAT_ADDR,
				BQ24261_LOW_CHG_MASK, reg_val);
	}

	/* cc setting will be done by platform specific hardware
	 * but, in case of error-conditions or if the setting fails,
	 * the following will be a fail-safe mechanism.
	 */

	bq24261_cc_to_reg(cc_mA, &reg_val);

	return bq24261_read_modify_reg(chip->client, BQ24261_TERM_FCC_ADDR,
			BQ24261_ICHRG_MASK, reg_val);
}

static inline int bq24261_set_cv(struct bq24261_charger *chip, int cv_mV)
{
	int bat_volt;
	int ret;
	u8 reg_val;
	u8 vindpm_val = 0x0;

	dev_dbg(&chip->client->dev, "%s=%d\n", __func__, cv_mV);
	/*
	* Setting VINDPM value as per the battery voltage
	*  VBatt           Vindpm     Register Setting
	*  < 3.7v           4.2v       0x0 (default)
	*  3.71v - 3.96v    4.36v      0x2
	*  > 3.96v          4.6v       0x5
	*/
	ret = get_battery_voltage(&bat_volt);
	if (ret) {
		dev_err(&chip->client->dev,
			"Error getting battery voltage!!\n");
	} else {
		if (bat_volt > BQ24261_VBATT_LEVEL2)
			vindpm_val =
				(BQ24261_VINDPM_320MV | BQ24261_VINDPM_80MV);
		else if (bat_volt > BQ24261_VBATT_LEVEL1)
			vindpm_val = BQ24261_VINDPM_160MV;
	}

	ret = bq24261_read_modify_reg(chip->client,
			BQ24261_VINDPM_STAT_ADDR,
			BQ24261_VINDPM_MASK,
			vindpm_val);
	if (ret) {
		dev_err(&chip->client->dev,
			"Error setting VINDPM setting!!\n");
		return ret;
	}

	if (chip->pdata->set_cv)
		chip->pdata->set_cv(cv_mV);

	/* cv setting will be done by platform specific hardware
	 * but, in case of error-conditions or if the setting fails,
	 * the following will be a fail-safe mechanism.
	 */
	bq24261_cv_to_reg(cv_mV, &reg_val);

	return bq24261_read_modify_reg(chip->client, BQ24261_BATT_VOL_CTRL_ADDR,
				       BQ24261_VBREG_MASK, reg_val);
}

static inline int bq24261_set_inlmt(struct bq24261_charger *chip, int inlmt)
{
	u8 reg_val;

	dev_dbg(&chip->client->dev, "%s=%d\n", __func__, inlmt);
	if (chip->pdata->set_inlmt)
		return chip->pdata->set_inlmt(inlmt);

	bq24261_inlmt_to_reg(inlmt, &reg_val);

	return bq24261_read_modify_reg(chip->client, BQ24261_CTRL_ADDR,
		       BQ24261_RESET_MASK|BQ24261_INLMT_MASK, reg_val);

}

static inline void resume_charging(struct bq24261_charger *chip)
{

	if (chip->is_charger_enabled)
		bq24261_enable_charger(chip, true);
	if (chip->inlmt)
		bq24261_set_inlmt(chip, chip->inlmt);
	if (chip->cc)
		bq24261_set_cc(chip, chip->cc);
	if (chip->cv)
		bq24261_set_cv(chip, chip->cv);
	if (chip->is_charging_enabled)
		bq24261_enable_charging(chip, true);
}

static inline int bq24261_set_iterm(struct bq24261_charger *chip, int iterm)
{
	u8 reg_val;

	if (chip->pdata->set_iterm)
		return chip->pdata->set_iterm(iterm);

	bq24261_iterm_to_reg(iterm, &reg_val);

	return bq24261_read_modify_reg(chip->client, BQ24261_TERM_FCC_ADDR,
				       BQ24261_ITERM_MASK, reg_val);
}

static inline int bq24261_enable_hw_charge_term(
	struct bq24261_charger *chip, bool val)
{
	u8 data;
	int ret;

	data = val ? BQ24261_TE_ENABLE : (~BQ24261_TE_ENABLE & BQ24261_TE_MASK);


	ret = bq24261_read_modify_reg(chip->client, BQ24261_CTRL_ADDR,
			       BQ24261_RESET_MASK|BQ24261_TE_MASK, data);

	if (ret)
		return ret;

	chip->is_hw_chrg_term = val ? true : false;

	return ret;
}

static inline int bq24261_enable_boost_mode(
	struct bq24261_charger *chip, bool enable)
{
	int ret = 0;


	if (enable) {

		if (((chip->revision & BQ24261_REV_MASK) == BQ24261_REV) ||
				chip->pdata->is_wdt_kick_needed) {
			if (chip->pdata->enable_vbus)
				chip->pdata->enable_vbus(true);
		}

		if (chip->pdata->handle_otgmode)
			chip->pdata->handle_otgmode(true);

		/* TODO: Support different Host Mode Current limits */

		bq24261_enable_charger(chip, true);
		ret =
		    bq24261_read_modify_reg(chip->client,
					    BQ24261_STAT_CTRL0_ADDR,
					    BQ24261_BOOST_MASK,
					    BQ24261_ENABLE_BOOST);
		if (unlikely(ret))
			return ret;

		ret = bq24261_tmr_ntc_init(chip);
		if (unlikely(ret))
			return ret;
		chip->boost_mode = true;

		if (((chip->revision & BQ24261_REV_MASK) == BQ24261_REV) ||
				chip->pdata->is_wdt_kick_needed)
			schedule_delayed_work(&chip->wdt_work, 0);

		dev_info(&chip->client->dev, "Boost Mode enabled\n");
	} else {

		ret =
		    bq24261_read_modify_reg(chip->client,
					    BQ24261_STAT_CTRL0_ADDR,
					    BQ24261_BOOST_MASK,
					    ~BQ24261_ENABLE_BOOST);

		if (unlikely(ret))
			return ret;
		/* if charging need not to be enabled, disable
		* the charger else keep the charger on
		*/
		if (!chip->is_charging_enabled)
			bq24261_enable_charger(chip, false);
		chip->boost_mode = false;
		dev_info(&chip->client->dev, "Boost Mode disabled\n");

		if (((chip->revision & BQ24261_REV_MASK) == BQ24261_REV) ||
				chip->pdata->is_wdt_kick_needed) {
			cancel_delayed_work_sync(&chip->wdt_work);

			if (chip->pdata->enable_vbus)
				chip->pdata->enable_vbus(false);
		}

		if (chip->pdata->handle_otgmode)
			chip->pdata->handle_otgmode(false);

		/* Notify power supply subsystem to enable charging
		 * if needed. Eg. if DC adapter is connected
		 */
		power_supply_changed(&chip->psy_usb);
	}

	return ret;
}

static inline bool bq24261_is_vsys_on(struct bq24261_charger *chip)
{
	int ret;
	struct i2c_client *client = chip->client;

	ret = bq24261_read_reg(client, BQ24261_CTRL_ADDR);
	if (ret < 0) {
		dev_err(&client->dev,
			"Error(%d) in reading BQ24261_CTRL_ADDR\n", ret);
		return false;
	}

	if (((ret & BQ24261_HiZ_MASK) == BQ24261_HiZ_ENABLE) &&
			chip->is_charger_enabled) {
		dev_err(&client->dev, "Charger in Hi Z Mode\n");
		bq24261_dump_regs(true);
		return false;
	}

	ret = bq24261_read_reg(client, BQ24261_VINDPM_STAT_ADDR);
	if (ret < 0) {
		dev_err(&client->dev,
			"Error(%d) in reading BQ24261_VINDPM_STAT_ADDR\n", ret);
		return false;
	}

	if (ret & BQ24261_CD_STATUS_MASK) {
		dev_err(&client->dev, "CD line asserted\n");
		bq24261_dump_regs(true);
		return false;
	}

	return true;
}


static inline bool bq24261_is_online(struct bq24261_charger *chip)
{
	if (chip->cable_type == POWER_SUPPLY_CHARGER_TYPE_NONE)
		return false;
	else if (!chip->is_charger_enabled)
		return false;
	/* BQ24261 gives interrupt only on stop/resume charging.
	 * If charging is already stopped, we need to query the hardware
	 * to see charger is still active and can supply vsys or not.
	 */
	else if ((chip->chrgr_stat == BQ24261_CHRGR_STAT_FAULT) ||
		 (!chip->is_charging_enabled))
		return bq24261_is_vsys_on(chip);
	else
		return chip->is_vsys_on;
}

static int bq24261_usb_set_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	struct bq24261_charger *chip = container_of(psy,
						    struct bq24261_charger,
						    psy_usb);
	int ret = 0;


	mutex_lock(&chip->stat_lock);


	switch (psp) {

	case POWER_SUPPLY_PROP_PRESENT:
		chip->present = val->intval;
		/*If charging capable cable is present, then
		hold the charger wakelock so that the target
		does not enter suspend mode when charging is
		in progress.
		If charging cable has been removed, then
		unlock the wakelock to allow the target to
		enter the sleep mode*/
		if (!wake_lock_active(&chip->chrgr_en_wakelock) &&
					val->intval)
			wake_lock(&chip->chrgr_en_wakelock);
		else if (wake_lock_active(&chip->chrgr_en_wakelock) &&
					!val->intval)
			wake_unlock(&chip->chrgr_en_wakelock);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		chip->online = val->intval;
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGING:

		/* Reset charging to avoid issues of not starting
		 * charging when we're recovering from fault-cases.
		 */
		if (val->intval) {
			dev_info(&chip->client->dev, "Charging reset");
			ret = bq24261_enable_charging(chip, false);
			if (ret)
				dev_err(&chip->client->dev,
					"Error(%d) in charging reset", ret);
		}

		ret = bq24261_enable_charging(chip, val->intval);

		if (ret)
			dev_err(&chip->client->dev,
				"Error(%d) in %s charging", ret,
				(val->intval ? "enable" : "disable"));
		else
			chip->is_charging_enabled = val->intval;

		if (val->intval)
			bq24261_enable_hw_charge_term(chip, true);
		else
			cancel_delayed_work_sync(&chip->sw_term_work);

		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGER:

		/* Don't enable the charger unless overvoltage is recovered */

		if (chip->bat_health != POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
			ret = bq24261_enable_charger(chip, val->intval);

			if (ret)
				dev_err(&chip->client->dev,
					"Error(%d) in %s charger", ret,
					(val->intval ? "enable" : "disable"));
			else
				chip->is_charger_enabled = val->intval;
		} else {
			dev_info(&chip->client->dev, "Battery Over Voltage. Charger will be disabled\n");
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_CURRENT:
		ret = bq24261_set_cc(chip, val->intval);
		if (!ret)
			chip->cc = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_VOLTAGE:
		ret = bq24261_set_cv(chip, val->intval);
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
		ret = bq24261_set_iterm(chip, val->intval);
		if (!ret)
			chip->iterm = val->intval;
		break;
	case POWER_SUPPLY_PROP_CABLE_TYPE:

		chip->cable_type = val->intval;
		chip->psy_usb.type = get_power_supply_type(chip->cable_type);
		if (chip->cable_type != POWER_SUPPLY_CHARGER_TYPE_NONE) {
			chip->chrgr_health = POWER_SUPPLY_HEALTH_GOOD;
			chip->chrgr_stat = BQ24261_CHRGR_STAT_UNKNOWN;

			/* Adding this processing in order to check
			for any faults during connect */

			ret = bq24261_read_reg(chip->client,
						BQ24261_STAT_CTRL0_ADDR);
			if (ret < 0)
				dev_err(&chip->client->dev,
				"Error (%d) in reading status register(0x00)\n",
				ret);
			else
				bq24261_handle_irq(chip, ret);
		} else {
			chip->chrgr_stat = BQ24261_CHRGR_STAT_UNKNOWN;
			chip->chrgr_health = POWER_SUPPLY_HEALTH_UNKNOWN;
			cancel_delayed_work_sync(&chip->low_supply_fault_work);
		}


		break;
	case POWER_SUPPLY_PROP_INLMT:
		ret = bq24261_set_inlmt(chip, val->intval);
		if (!ret)
			chip->inlmt = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		chip->cntl_state = val->intval;
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		chip->max_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_MIN_TEMP:
		chip->min_temp = val->intval;
		break;
	default:
		ret = -ENODATA;
	}

	mutex_unlock(&chip->stat_lock);
	return ret;
}

static int bq24261_usb_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct bq24261_charger *chip = container_of(psy,
						    struct bq24261_charger,
						    psy_usb);

	mutex_lock(&chip->stat_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->chrgr_health;
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
			val->intval = (chip->is_charging_enabled &&
			(chip->chrgr_stat == BQ24261_CHRGR_STAT_CHARGING));
		break;
	case POWER_SUPPLY_PROP_ENABLE_CHARGER:
		val->intval = bq24261_is_online(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = chip->cntl_state;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = chip->pdata->num_throttle_states;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->model_name;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = chip->manufacturer;
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		val->intval = chip->max_temp;
		break;
	case POWER_SUPPLY_PROP_MIN_TEMP:
		val->intval = chip->min_temp;
		break;
	default:
		mutex_unlock(&chip->stat_lock);
		return -EINVAL;
	}

	mutex_unlock(&chip->stat_lock);
	return 0;
}

static inline struct power_supply *get_psy_battery(void)
{
	struct class_dev_iter iter;
	struct device *dev;
	static struct power_supply *pst;

	class_dev_iter_init(&iter, power_supply_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		pst = (struct power_supply *)dev_get_drvdata(dev);
		if (pst->type == POWER_SUPPLY_TYPE_BATTERY) {
			class_dev_iter_exit(&iter);
			return pst;
		}
	}
	class_dev_iter_exit(&iter);

	return NULL;
}

static inline int get_battery_voltage(int *volt)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = get_psy_battery();
	if (!psy)
		return -EINVAL;

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (!ret)
		*volt = (val.intval);

	return ret;
}

static inline int get_battery_volt_max_design(int *volt)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = get_psy_battery();
	if (!psy)
		return -EINVAL;

	ret = psy->get_property(psy,
		POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &val);
	if (!ret)
		(*volt = val.intval);
	return ret;
}

static inline int get_battery_current(int *cur)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = get_psy_battery();
	if (!psy)
		return -EINVAL;

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (!ret)
		*cur = val.intval;

	return ret;
}

static void bq24261_wdt_reset_worker(struct work_struct *work)
{

	struct bq24261_charger *chip = container_of(work,
			    struct bq24261_charger, wdt_work.work);
	int ret;
	ret = bq24261_reset_timer(chip);

	if (ret)
		dev_err(&chip->client->dev, "Error (%d) in WDT reset\n", ret);
	else
		dev_info(&chip->client->dev, "WDT reset\n");

	schedule_delayed_work(&chip->wdt_work, WDT_RESET_DELAY);
}

static void bq24261_sw_charge_term_worker(struct work_struct *work)
{

	struct bq24261_charger *chip = container_of(work,
						    struct bq24261_charger,
						    sw_term_work.work);

	power_supply_changed(NULL);

	schedule_delayed_work(&chip->sw_term_work,
			      CHRG_TERM_WORKER_DELAY);

}

int bq24261_get_bat_health(void)
{

	struct bq24261_charger *chip;

	if (!bq24261_client)
		return -ENODEV;

	chip = i2c_get_clientdata(bq24261_client);

	return chip->bat_health;
}


static void bq24261_low_supply_fault_work(struct work_struct *work)
{
	struct bq24261_charger *chip = container_of(work,
						    struct bq24261_charger,
						    low_supply_fault_work.work);

	if (chip->chrgr_stat == BQ24261_CHRGR_STAT_FAULT) {
		dev_err(&chip->client->dev, "Low Supply Fault detected!!\n");
		chip->chrgr_health = POWER_SUPPLY_HEALTH_DEAD;
		power_supply_changed(&chip->psy_usb);
		schedule_delayed_work(&chip->exception_mon_work,
					EXCEPTION_MONITOR_DELAY);
		bq24261_dump_regs(true);
	}
	return;
}


/* is_bat_over_voltage: check battery is over voltage or not
*  @chip: bq24261_charger context
*
*  This function is used to verify the over voltage condition.
*  In some scenarios, HW generates Over Voltage exceptions when
*  battery voltage is normal. This function uses the over voltage
*  condition (voltage_max_design * 1.01) to verify battery is really
*  over charged or not.
*/

static bool is_bat_over_voltage(struct bq24261_charger *chip,
		bool verify_recovery)
{

	int bat_volt, bat_volt_max_des, ret;

	ret = get_battery_voltage(&bat_volt);
	if (ret)
		return verify_recovery ? false : true;

	ret = get_battery_volt_max_design(&bat_volt_max_des);

	if (ret)
		bat_volt_max_des = BQ24261_DEF_BAT_VOLT_MAX_DESIGN;

	dev_info(&chip->client->dev,
			"bat_volt=%d Voltage Max Design=%d OVP_VOLT=%d OVP recover volt=%d\n",
			bat_volt, bat_volt_max_des,
			(bat_volt_max_des/1000 * BQ24261_OVP_MULTIPLIER),
			(bat_volt_max_des/1000 *
				BQ24261_OVP_RECOVER_MULTIPLIER));
	if (verify_recovery) {
		if ((bat_volt) <= (bat_volt_max_des / 1000 *
				BQ24261_OVP_RECOVER_MULTIPLIER))
			return true;
		else
			return false;
	} else {
		if ((bat_volt) >= (bat_volt_max_des / 1000 *
					BQ24261_OVP_MULTIPLIER))
			return true;
		else
			return false;
	}

	return false;
}

#define IS_BATTERY_OVER_VOLTAGE(chip) \
	is_bat_over_voltage(chip , false)

#define IS_BATTERY_OVER_VOLTAGE_RECOVERED(chip) \
	is_bat_over_voltage(chip , true)

static void handle_battery_over_voltage(struct bq24261_charger *chip)
{
	/* Set Health to Over Voltage. Disable charger to discharge
	*  battery to reduce the battery voltage.
	*/
	chip->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	bq24261_enable_charger(chip, false);
	chip->is_charger_enabled = false;
	cancel_delayed_work_sync(&chip->exception_mon_work);
	schedule_delayed_work(&chip->exception_mon_work,
			EXCEPTION_MONITOR_DELAY);
}

static void bq24261_exception_mon_work(struct work_struct *work)
{
	struct bq24261_charger *chip = container_of(work,
			struct bq24261_charger,
			exception_mon_work.work);
	int ret;

	if (chip->bat_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
		if (IS_BATTERY_OVER_VOLTAGE_RECOVERED(chip)) {
			dev_info(&chip->client->dev,
					"Battery OVP Exception Recovered\n");
			chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
			bq24261_enable_charger(chip, true);
			chip->is_charger_enabled = true;
			power_supply_changed(&chip->psy_usb);
		} else {
			schedule_delayed_work(&chip->exception_mon_work,
					EXCEPTION_MONITOR_DELAY);
		}
	}

	if ((chip->chrgr_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
		(chip->chrgr_health == POWER_SUPPLY_HEALTH_DEAD)) {
		ret = bq24261_read_reg(chip->client, BQ24261_STAT_CTRL0_ADDR);
		if (ret < 0) {
			dev_err(&chip->client->dev, "Error reading reg %x\n",
					BQ24261_STAT_CTRL0_ADDR);
		} else {
			mutex_lock(&chip->stat_lock);
			bq24261_handle_irq(chip, ret);
			mutex_unlock(&chip->stat_lock);
			if ((ret & BQ24261_STAT_MASK) == BQ24261_STAT_READY) {
				dev_info(&chip->client->dev,
				"Charger OVP/Low Supply Exception recovered\n");
				power_supply_changed(&chip->psy_usb);
			}
		}
	}
}

static int bq24261_handle_irq(struct bq24261_charger *chip, u8 stat_reg)
{
	struct i2c_client *client = chip->client;
	bool notify = true;

	dev_info(&client->dev, "%s:%d stat=0x%x\n",
			__func__, __LINE__, stat_reg);

	switch (stat_reg & BQ24261_STAT_MASK) {
	case BQ24261_STAT_READY:
		chip->chrgr_stat = BQ24261_CHRGR_STAT_READY;
		chip->chrgr_health = POWER_SUPPLY_HEALTH_GOOD;
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		dev_info(&client->dev, "Charger Status: Ready\n");
		notify = false;
		break;
	case BQ24261_STAT_CHRG_PRGRSS:
		chip->chrgr_stat = BQ24261_CHRGR_STAT_CHARGING;
		chip->chrgr_health = POWER_SUPPLY_HEALTH_GOOD;
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		dev_info(&client->dev, "Charger Status: Charge Progress\n");
		bq24261_dump_regs(false);
		break;
	case BQ24261_STAT_CHRG_DONE:
		chip->chrgr_health = POWER_SUPPLY_HEALTH_GOOD;
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		dev_info(&client->dev, "Charger Status: Charge Done\n");

		bq24261_enable_hw_charge_term(chip, false);
		resume_charging(chip);
		schedule_delayed_work(&chip->sw_term_work, 0);
		break;

	case BQ24261_STAT_FAULT:
		break;
	}

	if (stat_reg & BQ24261_BOOST_MASK)
		dev_info(&client->dev, "Boost Mode\n");

	if ((stat_reg & BQ24261_STAT_MASK) == BQ24261_STAT_FAULT) {
		bool dump_master = true;
		chip->chrgr_stat = BQ24261_CHRGR_STAT_FAULT;

		switch (stat_reg & BQ24261_FAULT_MASK) {
		case BQ24261_VOVP:
			chip->chrgr_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			schedule_delayed_work(&chip->exception_mon_work,
					EXCEPTION_MONITOR_DELAY);
			dev_err(&client->dev, "Charger OVP Fault\n");
			break;

		case BQ24261_LOW_SUPPLY:
			notify = false;

			if (chip->cable_type !=
					POWER_SUPPLY_CHARGER_TYPE_NONE) {
				schedule_delayed_work
					(&chip->low_supply_fault_work,
					5*HZ);
				dev_dbg(&client->dev,
					"Schedule Low Supply Fault work!!\n");
			}
			break;

		case BQ24261_THERMAL_SHUTDOWN:
			chip->chrgr_health = POWER_SUPPLY_HEALTH_OVERHEAT;
			dev_err(&client->dev, "Charger Thermal Fault\n");
			break;

		case BQ24261_BATT_TEMP_FAULT:
			chip->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
			dev_err(&client->dev, "Battery Temperature Fault\n");
			break;

		case BQ24261_TIMER_FAULT:
			chip->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			chip->chrgr_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			dev_err(&client->dev, "Charger Timer Fault\n");
			break;

		case BQ24261_BATT_OVP:
			notify = false;
			if (chip->bat_health !=
					POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
				if (!IS_BATTERY_OVER_VOLTAGE(chip)) {
					chip->chrgr_stat =
						BQ24261_CHRGR_STAT_UNKNOWN;
					resume_charging(chip);
				} else {
					dev_err(&client->dev, "Battery Over Voltage Fault\n");
					handle_battery_over_voltage(chip);
					notify = true;
				}
			}
			break;
		case BQ24261_NO_BATTERY:
			dev_err(&client->dev, "No Battery Connected\n");
			break;

		}

		if (chip->chrgr_stat == BQ24261_CHRGR_STAT_FAULT && notify)
			bq24261_dump_regs(dump_master);
	}

	wake_up(&chip->wait_ready);

	chip->is_vsys_on = bq24261_is_vsys_on(chip);
	if (notify)
		power_supply_changed(&chip->psy_usb);

	return 0;
}

static void bq24261_irq_worker(struct work_struct *work)
{
	struct bq24261_charger *chip =
	    container_of(work, struct bq24261_charger, irq_work);
	int ret;

	/*Lock to ensure that interrupt register readings are done
	* and processed sequentially. The interrupt Fault registers
	* are read on clear and without sequential processing double
	* fault interrupts or fault recovery cannot be handlled propely
	*/

	mutex_lock(&chip->stat_lock);

	dev_dbg(&chip->client->dev, "%s\n", __func__);

	ret = bq24261_read_reg(chip->client, BQ24261_STAT_CTRL0_ADDR);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Error (%d) in reading BQ24261_STAT_CTRL0_ADDR\n", ret);
	} else {
		bq24261_handle_irq(chip, ret);
#ifdef CONFIG_DEBUG_FS
		chip->irq_counter++;
#endif
	}

	mutex_unlock(&chip->stat_lock);
}

static irqreturn_t bq24261_thread_handler(int id, void *data)
{
	struct bq24261_charger *chip = (struct bq24261_charger *)data;

	queue_work(system_nrt_wq, &chip->irq_work);
	return IRQ_HANDLED;
}

static irqreturn_t bq24261_irq_handler(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

static void bq24261_boostmode_worker(struct work_struct *work)
{
	struct bq24261_charger *chip =
	    container_of(work, struct bq24261_charger, otg_work);
	struct bq24261_otg_event *evt, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&chip->otg_queue_lock, flags);
	list_for_each_entry_safe(evt, tmp, &chip->otg_queue, node) {
		list_del(&evt->node);
		spin_unlock_irqrestore(&chip->otg_queue_lock, flags);

		dev_info(&chip->client->dev,
			"%s:%d state=%d\n", __FILE__, __LINE__,
				evt->is_enable);
		mutex_lock(&chip->stat_lock);
		if (evt->is_enable)
			bq24261_enable_boost_mode(chip, 1);
		else
			bq24261_enable_boost_mode(chip, 0);

		mutex_unlock(&chip->stat_lock);
		spin_lock_irqsave(&chip->otg_queue_lock, flags);
		kfree(evt);

	}
	spin_unlock_irqrestore(&chip->otg_queue_lock, flags);
}

static int otg_handle_notification(struct notifier_block *nb,
				   unsigned long event, void *param)
{

	struct bq24261_charger *chip =
	    container_of(nb, struct bq24261_charger, otg_nb);
	struct bq24261_otg_event *evt;

	dev_dbg(&chip->client->dev, "OTG notification: %lu\n", event);
	if (!param || event != USB_EVENT_DRIVE_VBUS)
		return NOTIFY_DONE;

	evt = kzalloc(sizeof(*evt), GFP_ATOMIC);
	if (!evt) {
		dev_err(&chip->client->dev,
			"failed to allocate memory for OTG event\n");
		return NOTIFY_DONE;
	}

	evt->is_enable = *(int *)param;
	INIT_LIST_HEAD(&evt->node);

	spin_lock(&chip->otg_queue_lock);
	list_add_tail(&evt->node, &chip->otg_queue);
	spin_unlock(&chip->otg_queue_lock);

	queue_work(system_nrt_wq, &chip->otg_work);
	return NOTIFY_OK;
}

static inline int register_otg_notifications(struct bq24261_charger *chip)
{

	int retval;

	INIT_LIST_HEAD(&chip->otg_queue);
	INIT_WORK(&chip->otg_work, bq24261_boostmode_worker);
	spin_lock_init(&chip->otg_queue_lock);

	chip->otg_nb.notifier_call = otg_handle_notification;

	chip->transceiver = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!chip->transceiver || IS_ERR(chip->transceiver)) {
		dev_err(&chip->client->dev, "failed to get otg transceiver\n");
		return -EINVAL;
	}
	retval = usb_register_notifier(chip->transceiver, &chip->otg_nb);
	if (retval) {
		dev_err(&chip->client->dev,
			"failed to register otg notifier\n");
		return -EINVAL;
	}

	return 0;
}

static enum bq2426x_model_num bq24261_get_model(int bq24261_rev_reg)
{
	switch (bq24261_rev_reg & BQ24261_REV_MASK) {
	case BQ24260_REV:
		return BQ24260;
	case BQ24261_REV:
	case BQ24261_2_3_REV:
		return BQ24261;
	default:
		return BQ2426X;
	}
}

static int bq24261_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;
	struct bq24261_charger *chip;
	int ret;
	int bq2426x_rev;
	enum bq2426x_model_num bq24261_rev_index;

	adapter = to_i2c_adapter(client->dev.parent);

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "platform data is null");
		return -EFAULT;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
			"I2C adapter %s doesn'tsupport BYTE DATA transfer\n",
			adapter->name);
		return -EIO;
	}

	bq2426x_rev = bq24261_read_reg(client, BQ24261_VENDOR_REV_ADDR);
	if (bq2426x_rev < 0) {
		dev_err(&client->dev,
			"Error (%d) in reading BQ24261_VENDOR_REV_ADDR\n",
			bq2426x_rev);
		return bq2426x_rev;
	}
	dev_info(&client->dev, "bq2426x revision: 0x%x found!!\n", bq2426x_rev);

	bq24261_rev_index = bq24261_get_model(bq2426x_rev);
	if ((bq2426x_rev & BQ24261_VENDOR_MASK) != BQ24261_VENDOR) {
		dev_err(&client->dev,
			"Invalid Vendor/Revision number in BQ24261_VENDOR_REV_ADDR: %d",
			bq2426x_rev);
		return -ENODEV;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&chip->wait_ready);
	i2c_set_clientdata(client, chip);
	chip->pdata = client->dev.platform_data;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	chip->psy_usb.name = DEV_NAME;
	chip->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	chip->psy_usb.properties = bq24261_usb_props;
	chip->psy_usb.num_properties = ARRAY_SIZE(bq24261_usb_props);
	chip->psy_usb.get_property = bq24261_usb_get_property;
	chip->psy_usb.set_property = bq24261_usb_set_property;
	chip->psy_usb.supplied_to = chip->pdata->supplied_to;
	chip->psy_usb.num_supplicants = chip->pdata->num_supplicants;
	chip->psy_usb.throttle_states = chip->pdata->throttle_states;
	chip->psy_usb.num_throttle_states = chip->pdata->num_throttle_states;
	chip->psy_usb.supported_cables = POWER_SUPPLY_CHARGER_TYPE_USB;
	chip->max_cc = chip->pdata->max_cc;
	chip->max_cv = chip->pdata->max_cv;
	chip->chrgr_stat = BQ24261_CHRGR_STAT_UNKNOWN;
	chip->chrgr_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chip->revision = bq2426x_rev;

	strncpy(chip->model_name,
		bq24261_model_name[bq24261_rev_index].model_name,
		MODEL_NAME_SIZE);
	strncpy(chip->manufacturer, DEV_MANUFACTURER,
		DEV_MANUFACTURER_NAME_SIZE);

	mutex_init(&chip->stat_lock);
	wake_lock_init(&chip->chrgr_en_wakelock,
			WAKE_LOCK_SUSPEND, "chrgr_en_wakelock");
	ret = power_supply_register(&client->dev, &chip->psy_usb);
	if (ret) {
		dev_err(&client->dev, "Failed: power supply register (%d)\n",
			ret);
		return ret;
	}

	INIT_DELAYED_WORK(&chip->sw_term_work, bq24261_sw_charge_term_worker);
	INIT_DELAYED_WORK(&chip->low_supply_fault_work,
				bq24261_low_supply_fault_work);
	INIT_DELAYED_WORK(&chip->exception_mon_work,
				bq24261_exception_mon_work);
	if (((chip->revision & BQ24261_REV_MASK) == BQ24261_REV) ||
			chip->pdata->is_wdt_kick_needed) {
		INIT_DELAYED_WORK(&chip->wdt_work,
					bq24261_wdt_reset_worker);
	}

	INIT_WORK(&chip->irq_work, bq24261_irq_worker);
	if (chip->client->irq) {
		ret = request_threaded_irq(chip->client->irq,
					   bq24261_irq_handler,
					   bq24261_thread_handler,
					   IRQF_SHARED|IRQF_NO_SUSPEND,
					   DEV_NAME, chip);
		if (ret) {
			dev_err(&client->dev, "Failed: request_irq (%d)\n",
				ret);
			power_supply_unregister(&chip->psy_usb);
			return ret;
		}
	}

	if (IS_BATTERY_OVER_VOLTAGE(chip))
		handle_battery_over_voltage(chip);
	else
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;

	if (register_otg_notifications(chip))
		dev_err(&client->dev, "Error in registering OTG notifications. Unable to supply power to Host\n");

	bq24261_client = client;
	power_supply_changed(&chip->psy_usb);
	bq24261_debugfs_init();

	return 0;
}

static int bq24261_remove(struct i2c_client *client)
{
	struct bq24261_charger *chip = i2c_get_clientdata(client);

	if (client->irq)
		free_irq(client->irq, chip);

	flush_scheduled_work();
	wake_lock_destroy(&chip->chrgr_en_wakelock);
	if (chip->transceiver)
		usb_unregister_notifier(chip->transceiver, &chip->otg_nb);

	power_supply_unregister(&chip->psy_usb);
	bq24261_debugfs_exit();
	return 0;
}

static int bq24261_suspend(struct device *dev)
{
	struct bq24261_charger *chip = dev_get_drvdata(dev);

	if (((chip->revision & BQ24261_REV_MASK) == BQ24261_REV) ||
			chip->pdata->is_wdt_kick_needed) {
		if (chip->boost_mode)
			cancel_delayed_work_sync(&chip->wdt_work);
	}
	dev_dbg(&chip->client->dev, "bq24261 suspend\n");
	return 0;
}

static int bq24261_resume(struct device *dev)
{
	struct bq24261_charger *chip = dev_get_drvdata(dev);

	if (((chip->revision & BQ24261_REV_MASK) == BQ24261_REV) ||
			chip->pdata->is_wdt_kick_needed) {
		if (chip->boost_mode)
			bq24261_enable_boost_mode(chip, 1);
	}

	dev_dbg(&chip->client->dev, "bq24261 resume\n");
	return 0;
}

static int bq24261_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq24261_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int bq24261_runtime_idle(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static const struct dev_pm_ops bq24261_pm_ops = {
	.suspend = bq24261_suspend,
	.resume = bq24261_resume,
	.runtime_suspend = bq24261_runtime_suspend,
	.runtime_resume = bq24261_runtime_resume,
	.runtime_idle = bq24261_runtime_idle,
};

char *bq24261_supplied_to[] = {
	"max170xx_battery",
	"max17042_battery",
	"max17047_battery",
	"intel_fuel_gauge",
};

struct bq24261_plat_data bq24261_drvdata = {
	.throttle_states = bq24261_throttle_states,
	.supplied_to = bq24261_supplied_to,
	.num_throttle_states = ARRAY_SIZE(bq24261_throttle_states),
	.num_supplicants = ARRAY_SIZE(bq24261_supplied_to),
	.is_wdt_kick_needed = true,
	.max_cc = BQ24261_MAX_CC,
	.max_cv = BQ24261_MAX_CV,
};

static const struct i2c_device_id bq24261_id[] = {
	{DEV_NAME, 0},
	{"ext-charger", (kernel_ulong_t)&bq24261_drvdata},
	{},
};

MODULE_DEVICE_TABLE(i2c, bq24261_id);

static struct i2c_driver bq24261_driver = {
	.driver = {
		   .name = DEV_NAME,
		   .pm = &bq24261_pm_ops,
		   },
	.probe = bq24261_probe,
	.remove = bq24261_remove,
	.id_table = bq24261_id,
};

static int __init bq24261_init(void)
{
	return i2c_add_driver(&bq24261_driver);
}

module_init(bq24261_init);

static void __exit bq24261_exit(void)
{
	i2c_del_driver(&bq24261_driver);
}

module_exit(bq24261_exit);

MODULE_AUTHOR("Jenny TC <jenny.tc@intel.com>");
MODULE_DESCRIPTION("BQ24261 Charger Driver");
MODULE_LICENSE("GPL");
