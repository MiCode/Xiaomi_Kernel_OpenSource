/*
 * BQ2589x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#define pr_fmt(fmt) "BQ2591X %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include "bq25910_reg.h"
#include <linux/qpnp/qpnp-adc.h>

#ifdef pr_debug
#undef pr_debug
#define pr_debug pr_err
#endif

enum bq2591x_part_no {
	BQ25910 = 0x01,
};

enum reason {
	USER	= BIT(0),
	THERMAL = BIT(1),
	CURRENT = BIT(2),
	SOC	= BIT(3),
};

#define  SUSPEND_CURRENT_MA 2

#define CONN_HEALTH_WARM_TRH           530
#define CONN_HEALTH_HOT_TRH            560
#define CONN_HEALTH_OVERHEAT_TRH       710

struct bq2591x_config {
	int chg_mv;
	int chg_ma;

	int ivl_mv;
	int icl_ma;

	int iterm_ma;
	int batlow_mv;

	bool enable_term;
};


struct bq2591x {
	struct	device	*dev;
	struct	i2c_client *client;
	enum bq2591x_part_no part_no;
	int revision;

	struct bq2591x_config cfg;
	struct delayed_work monitor_work;
	struct delayed_work icl_softstart_work;
	struct delayed_work fcc_softstart_work;

	bool iindpm;
	bool vindpm;

	bool in_therm_regulation;

	int chg_mv;
	int chg_ma;
	int ivl_mv;
	int icl_ma;

	int vfloat_mv;
	int usb_psy_ma;
	int fast_cc_ma;

	int charge_state;
	int fault_status;

	int prev_stat_flag;
	int prev_fault_flag;

	int c_health;
	int max_fcc;

	int reg_stat;
	int reg_fault;
	int reg_stat_flag;
	int reg_fault_flag;

	struct mutex i2c_rw_lock;

	struct power_supply *usb_psy;
	struct power_supply *parallel_psy;
	struct power_supply_desc parallel_psy_d;

	struct dentry *debug_root;
	int skip_reads;
	int skip_writes;

	/* adc parameters */
	struct qpnp_vadc_chip   *vadc_dev;
};

static int bq2591x_set_fast_chg_current(struct bq2591x *bq, int current_ma);

static int __bq2591x_read_reg(struct bq2591x *bq, u8 reg, u8 *data)
{
	s32 ret;

	pm_stay_awake(bq->dev);
	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		pm_relax(bq->dev);
		return ret;
	}

	*data = (u8)ret;

	pm_relax(bq->dev);

	return 0;
}

static int __bq2591x_write_reg(struct bq2591x *bq, int reg, u8 val)
{
	s32 ret;

	pm_stay_awake(bq->dev);
	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		pm_relax(bq->dev);
		return ret;
	}

	pm_relax(bq->dev);

	return 0;
}

static int bq2591x_read_byte(struct bq2591x *bq, u8 *data, u8 reg)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2591x_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}


static int bq2591x_write_byte(struct bq2591x *bq, u8 reg, u8 data)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2591x_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}


static int bq2591x_update_bits(struct bq2591x *bq, u8 reg,
					u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2591x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2591x_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq2591x_enable_charger(struct bq2591x *bq)
{
	int ret;
	u8 val = BQ2591X_CHG_ENABLE << BQ2591X_EN_CHG_SHIFT;

	ret = bq2591x_update_bits(bq, BQ2591X_REG_06,
				BQ2591X_EN_CHG_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2591x_enable_charger);

static int bq2591x_disable_charger(struct bq2591x *bq)
{
	int ret;
	u8 val = BQ2591X_CHG_DISABLE << BQ2591X_EN_CHG_SHIFT;

	ret = bq2591x_update_bits(bq, BQ2591X_REG_06,
				BQ2591X_EN_CHG_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2591x_disable_charger);

static int bq2591x_enable_term(struct bq2591x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2591X_TERM_ENABLE;
	else
		val = BQ2591X_TERM_DISABLE;

	val <<= BQ2591X_EN_TERM_SHIFT;

	ret = bq2591x_update_bits(bq, BQ2591X_REG_05,
				BQ2591X_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2591x_enable_term);

int bq2591x_set_chargecurrent(struct bq2591x *bq, int curr)
{
	u8 ichg;

	ichg = (curr - BQ2591X_ICHG_BASE)/BQ2591X_ICHG_LSB;

	ichg <<= BQ2591X_ICHG_SHIFT;

	return bq2591x_update_bits(bq, BQ2591X_REG_01,
				BQ2591X_ICHG_MASK, ichg);

}
EXPORT_SYMBOL_GPL(bq2591x_set_chargecurrent);

static int bq2591x_get_chargecurrent(struct bq2591x *bq, int *curr)
{
	int ret;
	u8 ichg;

	ret = bq2591x_read_byte(bq, &ichg, BQ2591X_REG_01);
	if (!ret) {
		ichg = ichg & BQ2591X_ICHG_MASK;
		ichg = ichg >> BQ2591X_ICHG_SHIFT;
		*curr = ichg * BQ2591X_ICHG_LSB + BQ2591X_ICHG_BASE;
	}

	return ret;
}


int bq2591x_set_chargevoltage(struct bq2591x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2591X_VREG_BASE)/BQ2591X_VREG_LSB;
	val <<= BQ2591X_VREG_SHIFT;
	return bq2591x_update_bits(bq, BQ2591X_REG_00,
				BQ2591X_VREG_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2591x_set_chargevoltage);

static int bq2591x_get_chargevoltage(struct bq2591x *bq, int *volt)
{
	int ret;
	u8 vreg;

	ret = bq2591x_read_byte(bq, &vreg, BQ2591X_REG_00);
	if (!ret) {
		vreg = vreg & BQ2591X_VREG_MASK;
		vreg = vreg >> BQ2591X_VREG_SHIFT;
		*volt = vreg * BQ2591X_VREG_LSB + BQ2591X_VREG_BASE;
	}

	return ret;

}

int bq2591x_set_input_volt_limit(struct bq2591x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2591X_VINDPM_BASE) / BQ2591X_VINDPM_LSB;
	val <<= BQ2591X_VINDPM_SHIFT;

	return bq2591x_update_bits(bq, BQ2591X_REG_02,
				BQ2591X_VINDPM_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2591x_set_input_volt_limit);

int bq2591x_set_input_current_limit(struct bq2591x *bq, int curr)
{
	u8 val;

	if (curr < 500)
		curr = 500;

	val = (curr - BQ2591X_IINLIM_BASE) / BQ2591X_IINLIM_LSB;
	val <<= BQ2591X_IINLIM_SHIFT;

	return bq2591x_update_bits(bq, BQ2591X_REG_03,
				BQ2591X_IINLIM_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2591x_set_input_current_limit);

static int bq2591x_get_input_current_limit(struct bq2591x *bq, int *curr)
{
	int ret;
	u8 val;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_03);
	if (!ret) {
		val = val & BQ2591X_IINLIM_MASK;
		val = val >> BQ2591X_IINLIM_SHIFT;
		*curr = val * BQ2591X_IINLIM_LSB + BQ2591X_IINLIM_BASE;
	}
	return ret;
}


int bq2591x_set_watchdog_timer(struct bq2591x *bq, u8 timeout)
{
	u8 val;

	val = (timeout - BQ2591X_WDT_BASE) / BQ2591X_WDT_LSB;

	val <<= BQ2591X_WDT_SHIFT;

	return bq2591x_update_bits(bq, BQ2591X_REG_05,
				BQ2591X_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2591x_set_watchdog_timer);

int bq2591x_disable_watchdog_timer(struct bq2591x *bq)
{
	u8 val = BQ2591X_WDT_DISABLE << BQ2591X_WDT_SHIFT;

	return bq2591x_update_bits(bq, BQ2591X_REG_05,
				BQ2591X_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2591x_disable_watchdog_timer);

int bq2591x_reset_watchdog_timer(struct bq2591x *bq)
{
	u8 val = BQ2591X_WDT_RESET << BQ2591X_WDT_RESET_SHIFT;

	return bq2591x_update_bits(bq, BQ2591X_REG_05,
				BQ2591X_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2591x_reset_watchdog_timer);

int bq2591x_reset_chip(struct bq2591x *bq)
{
	int ret;
	u8 val = BQ2591X_RESET << BQ2591X_RESET_SHIFT;

	ret = bq2591x_update_bits(bq, BQ2591X_REG_0D,
				BQ2591X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2591x_reset_chip);

int bq2591x_set_vbatlow_volt(struct bq2591x *bq, int volt)
{
	int ret;
	u8 val;

	if (volt == 2600)
		val = BQ2591X_VBATLOWV_2600MV;
	else if (volt == 2900)
		val = BQ2591X_VBATLOWV_2900MV;
	else if (volt == 3200)
		val = BQ2591X_VBATLOWV_3200MV;
	else if (volt == 3500)
		val = BQ2591X_VBATLOWV_3500MV;
	else
		val = BQ2591X_VBATLOWV_3500MV;

	val <<= BQ2591X_VBATLOWV_SHIFT;

	ret = bq2591x_update_bits(bq, BQ2591X_REG_06,
				BQ2591X_VBATLOWV_MASK, val);

	return ret;
}

static ssize_t bq2591x_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bq2591x *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2591x Reg");
	for (addr = 0x0; addr <= 0x0D; addr++) {
		ret = bq2591x_read_byte(bq, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%02X] = 0x%02X\n",	addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t bq2591x_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bq2591x *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x0D)
		bq2591x_write_byte(bq, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2591x_show_registers,
						bq2591x_store_registers);

static struct attribute *bq2591x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2591x_attr_group = {
	.attrs = bq2591x_attributes,
};

static int show_registers(struct seq_file *m, void *data)
{
	struct bq2591x *bq = m->private;
	int addr;
	int ret;
	u8 val;

	for (addr = 0x0; addr <= 0x0D; addr++) {
		ret = bq2591x_read_byte(bq, &val, addr);
		if (!ret)
			seq_printf(m, "Reg[%02X] = 0x%02X\n", addr, val);
	}
	return 0;
}

static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct bq2591x *bq = inode->i_private;

	return single_open(file, show_registers, bq);
}

static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entries(struct bq2591x *bq)
{
	bq->debug_root = debugfs_create_dir("bq2591x", NULL);
	if (!bq->debug_root)
		pr_err("Failed to create debug dir\n");

	if (bq->debug_root) {
		debugfs_create_file("registers", S_IFREG | S_IRUGO,
					bq->debug_root, bq, &reg_debugfs_ops);

		debugfs_create_x32("skip_writes",  S_IFREG | S_IRUGO,
					bq->debug_root, &(bq->skip_writes));

		debugfs_create_x32("skip_reads",  S_IFREG | S_IRUGO,
					bq->debug_root, &(bq->skip_reads));

		debugfs_create_x32("fault_status", S_IFREG | S_IRUGO,
					bq->debug_root, &(bq->fault_status));

		debugfs_create_x32("charge_state", S_IFREG | S_IRUGO,
					bq->debug_root, &(bq->charge_state));
	}
}

static int bq2591x_usb_suspend(struct bq2591x *bq, bool suspend)
{
	int rc = 0;

	if (suspend){
		rc = bq2591x_disable_charger(bq);
	} else {
		rc = bq2591x_enable_charger(bq);
	}

	if (rc) {
		pr_err("Couldn't %s rc = %d\n",
			suspend ? "suspend" : "resume",  rc);
		return rc;
	}

	return rc;
}



int bq2591x_get_charging_status(struct bq2591x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_07);
	if (ret < 0) {
		pr_err("Failed to read register 0x0b:%d\n", ret);
		return ret;
	}
	val &= BQ2591X_CHRG_STAT_MASK;
	val >>= BQ2591X_CHRG_STAT_SHIFT;
	if (val == BQ2591X_CHRG_STAT_NCHG)
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	else if (val == BQ2591X_CHRG_STAT_FCHG)
		return POWER_SUPPLY_STATUS_CHARGING;
	else if (val == BQ2591X_CHRG_STAT_TCHG)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_UNKNOWN;
}
EXPORT_SYMBOL_GPL(bq2591x_get_charging_status);


static int bq2591x_get_prop_charge_type(struct bq2591x *bq)
{
	u8 val;
	int ret;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_07);
	if (ret) {
		pr_err("failed to read status register, ret:%d\n", ret);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	val = (val & BQ2591X_CHRG_STAT_MASK) >> BQ2591X_CHRG_STAT_SHIFT;

	if (val == BQ2591X_CHRG_STAT_NCHG)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (val == BQ2591X_CHRG_STAT_FCHG)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;


	else
		return  POWER_SUPPLY_CHARGE_TYPE_NONE;

}

static int bq2591x_get_prop_health(struct bq2591x *bq)
{
	int rc = 0;
	int conn_temp = 0;
	struct qpnp_vadc_result results;

	if (bq->vadc_dev) {
		rc = qpnp_vadc_read(bq->vadc_dev, VADC_AMUX_THM3_PU2, &results);
		if (rc)
			pr_err("Unable to read adc connector temp rc=%d\n", rc);
		else
			conn_temp = (int)results.physical/100;
	}

	if (conn_temp > CONN_HEALTH_OVERHEAT_TRH)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (conn_temp > CONN_HEALTH_HOT_TRH)
		return POWER_SUPPLY_HEALTH_HOT;
	else if (conn_temp > CONN_HEALTH_WARM_TRH)
		return POWER_SUPPLY_HEALTH_WARM;
	else
		return POWER_SUPPLY_HEALTH_COOL;
}

static bool bq2591x_is_usb_present(struct bq2591x *bq)
{
	int rc;
	union power_supply_propval val = {0, };

	if (!bq->usb_psy)
		bq->usb_psy = power_supply_get_by_name("usb");
	if (!bq->usb_psy) {
		pr_err("USB psy not found\n");
		return false;
	}

	rc = power_supply_get_property(bq->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &val);

	if (rc < 0) {
		pr_err("Failed to get present property rc=%d\n", rc);
		return false;
	}

	if (val.intval)
		return true;

	return false;

}


static bool bq2591x_is_input_current_limited(struct bq2591x *bq)
{
	int rc;
	u8 status;

	rc = bq2591x_read_byte(bq, &status, BQ2591X_REG_07);
	if (rc) {
		pr_err("Failed to read INDPM STATUS register:%d\n", rc);
		return false;
	}

	return !!(status & BQ2591X_IINDPM_STAT_MASK);

}

static void bq2591x_icl_softstart_workfunc(struct work_struct *work)
{
	struct bq2591x *bq = container_of(work, struct bq2591x,
					icl_softstart_work.work);
	int icl_set;
	int ret;

	ret = bq2591x_get_input_current_limit(bq, &icl_set);
	if (!ret) {
		if (bq->usb_psy_ma < icl_set) {
			bq2591x_set_input_current_limit(bq, bq->usb_psy_ma);
		} else 	if (bq->usb_psy_ma - icl_set < BQ2591X_IINLIM_LSB) {
			pr_debug("icl softstart done!\n");
			return;/*softstart done*/
		} else {
			icl_set = icl_set + BQ2591X_IINLIM_LSB;
			pr_debug("icl softstart set:%d\n", icl_set);
			bq2591x_set_input_current_limit(bq, icl_set);
			schedule_delayed_work(&bq->icl_softstart_work, HZ / 10);
		}

	} else {
		schedule_delayed_work(&bq->icl_softstart_work, HZ / 10);
	}

}

static int bq2591x_set_usb_chg_current(struct bq2591x *bq, int current_ma)
{
	int rc = 0;

	if (bq->revision == 0) /*PG1.0*/
		schedule_delayed_work(&bq->icl_softstart_work, 0);
	else
		rc = bq2591x_set_input_current_limit(bq, current_ma);

	if (rc) {
		pr_err("failed to set input current limit:%d\n", rc);
		return rc;
	}

	return rc;
}

static void bq2591x_fcc_softstart_workfunc(struct work_struct *work)
{
	struct bq2591x *bq = container_of(work, struct bq2591x,
					fcc_softstart_work.work);
	int fcc_set;
	int ret;

	ret = bq2591x_get_chargecurrent(bq, &fcc_set);
	if (!ret) {
		if (bq->fast_cc_ma < fcc_set) {
			bq2591x_set_chargecurrent(bq, bq->fast_cc_ma);
		} else 	if (bq->fast_cc_ma - fcc_set < BQ2591X_ICHG_LSB) {
			pr_debug("fcc softstart done!\n");
			return;/*softstart done*/
		} else {
			fcc_set = fcc_set + BQ2591X_ICHG_LSB;
			pr_debug("fcc softstart set fcc:%d\n", fcc_set);
			bq2591x_set_chargecurrent(bq, fcc_set);
			schedule_delayed_work(&bq->fcc_softstart_work, HZ / 10);
		}

	} else {
		schedule_delayed_work(&bq->fcc_softstart_work, HZ / 10);
	}

}


static int bq2591x_set_fast_chg_current(struct bq2591x *bq, int current_ma)
{
	int ret = 0;

	if (bq->revision == 0) {
		bq2591x_set_chargecurrent(bq, 1000);
		schedule_delayed_work(&bq->fcc_softstart_work, 0);
	} else {
		ret = bq2591x_set_chargecurrent(bq, current_ma);
	}

	return ret;
}

static enum power_supply_property bq2591x_charger_properties[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_CHARGER_TEMP,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_CONNECTOR_HEALTH,
	POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE,
	POWER_SUPPLY_PROP_PARALLEL_FCC_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_MIN_ICL,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_DIE_HEALTH,
	POWER_SUPPLY_PROP_BUCK_FREQ,
};

static int bq2591x_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0;
	struct bq2591x *bq = power_supply_get_drvdata(psy);


	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = bq2591x_usb_suspend(bq, val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		bq->fast_cc_ma = val->intval / 1000;
		rc = bq2591x_set_fast_chg_current(bq, bq->fast_cc_ma);
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		bq->usb_psy_ma = val->intval / 1000;
		rc = bq2591x_set_usb_chg_current(bq,
						bq->usb_psy_ma);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		bq->vfloat_mv = val->intval / 1000;
		rc = bq2591x_set_chargevoltage(bq, bq->vfloat_mv);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		bq->c_health = val->intval;
		power_supply_changed(bq->parallel_psy);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		rc = 0;
		break;
	case POWER_SUPPLY_PROP_BUCK_FREQ:
		rc = 0;
		break;
	default:
		pr_err("unsupported prop:%d", prop);
		return -EINVAL;
	}


	return rc;


}

static int bq2591x_charger_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

#define MIN_PARALLEL_ICL_UA 250000
static int bq2591x_charger_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct bq2591x *bq = power_supply_get_drvdata(psy);
	u8 temp;
	int itemp = 0;
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		rc = bq2591x_read_byte(bq, &temp, BQ2591X_REG_07);
		if (rc >= 0)
			val->intval = !!(temp & BQ2591X_PG_STAT_MASK);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		/* assume it is always enabled, using SUSPEND to control charging */
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = bq2591x_read_byte(bq, &temp, BQ2591X_REG_06);
		if (rc >= 0)
			val->intval = (bool)!(temp & BQ2591X_EN_CHG_MASK);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = bq2591x_get_input_current_limit(bq, &itemp);
		if (rc >= 0)
			val->intval = itemp * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = bq2591x_get_chargevoltage(bq, &itemp);
		if (rc >= 0)
			val->intval = itemp * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;

		if (bq2591x_is_usb_present(bq)) {
			val->intval = bq2591x_get_prop_charge_type(bq);
			if (val->intval == POWER_SUPPLY_CHARGE_TYPE_UNKNOWN) {
				pr_debug("Failed to get charge type, charger may be absent\n");
				return -ENODEV;
			}
		}
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = bq2591x_get_chargecurrent(bq, &itemp);
		if (rc >= 0)
			val->intval = itemp * 1000;
		break;

	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq2591x_get_charging_status(bq);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		val->intval = bq2591x_is_input_current_limited(bq) ? 1 : 0;
		break;

	case POWER_SUPPLY_PROP_PARALLEL_MODE:
		val->intval = POWER_SUPPLY_PL_USBIN_USBIN;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "smbq2591x";
		break;

	case POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE:
		/* used for 910 output to connect to vphpwr*/
		val->intval = POWER_SUPPLY_PL_STACKED_BATFET;
		break;
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		val->intval = 0;
		break;
#if 0
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;/*TODO?*/
		break;
#endif

	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		if (bq->c_health == -EINVAL)
			val->intval = bq2591x_get_prop_health(bq);
		else
			val->intval = bq->c_health;
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP:
		val->intval = 200;
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		val->intval = 800;
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_MIN_ICL:
		val->intval = MIN_PARALLEL_ICL_UA;
		break;

	case POWER_SUPPLY_PROP_PARALLEL_FCC_MAX:
		val->intval = bq->max_fcc;
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_COOL;
		break;
	case POWER_SUPPLY_PROP_BUCK_FREQ:
		val->intval = 0;
		break;
	default:
		pr_err("unsupported prop:%d\n", prop);
		return -EINVAL;
	}
	return 0;

}

static int bq2591x_parse_dt(struct device *dev, struct bq2591x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;


	bq->cfg.enable_term = of_property_read_bool(np,
					"ti,bq2591x,enable-term");

	ret = of_property_read_u32(np, "ti,bq2591x,charge-voltage",
					&bq->cfg.chg_mv);
	if (ret)
		return ret;

	bq->vfloat_mv = bq->cfg.chg_mv;

	ret = of_property_read_u32(np, "ti,bq2591x,charge-current",
					&bq->cfg.chg_ma);
	if (ret)
		return ret;

	bq->fast_cc_ma = bq->cfg.chg_ma;

	ret = of_property_read_u32(np, "ti,bq2591x,input-current-limit",
					&bq->cfg.icl_ma);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2591x,input-voltage-limit",
					&bq->cfg.ivl_mv);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2591x,vbatlow-volt",
					&bq->cfg.batlow_mv);

	return ret;
}

static int bq2591x_detect_device(struct bq2591x *bq)
{
	int ret;
	u8 data;

	ret = bq2591x_read_byte(bq, &data, BQ2591X_REG_0D);
	if (ret == 0) {
		bq->part_no = (data & BQ2591X_PN_MASK) >> BQ2591X_PN_SHIFT;
		bq->revision = (data & BQ2591X_DEV_REV_MASK) >> BQ2591X_DEV_REV_SHIFT;
	}

	return ret;
}

static int bq2591x_set_charge_profile(struct bq2591x *bq)
{
	int ret;

	ret = bq2591x_set_chargevoltage(bq, bq->cfg.chg_mv);
	if (ret < 0) {
		pr_err("Failed to set charge voltage:%d\n", ret);
		return ret;
	}

	ret = bq2591x_set_chargecurrent(bq, bq->cfg.chg_ma);
	if (ret < 0) {
		pr_err("Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = bq2591x_set_input_current_limit(bq, bq->cfg.icl_ma);
	if (ret < 0) {
		pr_err("Failed to set input current limit:%d\n", ret);
		return ret;
	}

	ret = bq2591x_set_input_volt_limit(bq, bq->cfg.ivl_mv);
	if (ret < 0) {
		pr_err("Failed to set input voltage limit:%d\n", ret);
		return ret;
	}
	return 0;
}

static int bq2591x_init_device(struct bq2591x *bq)
{
	int ret;

	bq->chg_mv = bq->cfg.chg_mv;
	bq->chg_ma = bq->cfg.chg_ma;
	bq->ivl_mv = bq->cfg.ivl_mv;
	bq->icl_ma = bq->cfg.icl_ma;


	ret = bq2591x_disable_watchdog_timer(bq);
	if (ret < 0)
		pr_err("Failed to disable watchdog timer:%d\n", ret);

	/*as slave charger, disable it by default*/
	bq2591x_usb_suspend(bq, true);

	ret = bq2591x_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);

	ret = bq2591x_set_vbatlow_volt(bq, bq->cfg.batlow_mv);
	if (ret < 0)
		pr_err("Failed to set vbatlow volt to %d,rc=%d\n",
					bq->cfg.batlow_mv, ret);

	bq2591x_set_charge_profile(bq);

	return 0;
}

static void bq2591x_dump_regs(struct bq2591x *bq)
{
	int ret;
	u8 addr;
	u8 val;
	char regs_val[48];

	for (addr = 0x00; addr <= 0x0D; addr++) {
		msleep(2);
		ret = bq2591x_read_byte(bq, &val, addr);
		if (!ret)
			snprintf(regs_val+addr*3, 4, "%02X ", val);
	}

	pr_err("%s\n", regs_val);

}

static void bq2591x_stat_handler(struct bq2591x *bq)
{
	if (bq->prev_stat_flag == bq->reg_stat_flag)
		return;

	bq->prev_stat_flag = bq->reg_stat_flag;
	pr_info("%s\n", (bq->reg_stat & BQ2591X_PG_STAT_MASK) ?
					"Power Good" : "Power Poor");

	if (bq->reg_stat & BQ2591X_IINDPM_STAT_MASK)
		pr_info("IINDPM Triggered\n");

	if (bq->reg_stat & BQ2591X_VINDPM_STAT_MASK)
		pr_info("VINDPM Triggered\n");

	if (bq->reg_stat & BQ2591X_TREG_STAT_MASK)
		pr_info("TREG Triggered\n");

	if (bq->reg_stat & BQ2591X_WD_STAT_MASK)
		pr_err("Watchdog overflow\n");

	bq->charge_state = (bq->reg_stat & BQ2591X_CHRG_STAT_MASK)
					>> BQ2591X_CHRG_STAT_SHIFT;
	if (bq->charge_state == BQ2591X_CHRG_STAT_NCHG) {
		pr_info("Not Charging\n");
		cancel_delayed_work_sync(&bq->monitor_work);
	} else if (bq->charge_state == BQ2591X_CHRG_STAT_FCHG) {
		pr_info("Fast Charging\n");
		if (!delayed_work_pending(&bq->monitor_work))
			schedule_delayed_work(&bq->monitor_work, 0);
	}else if (bq->charge_state == BQ2591X_CHRG_STAT_TCHG)
		pr_info("Taper Charging\n");

}

static void bq2591x_fault_handler(struct bq2591x *bq)
{
	if (bq->prev_fault_flag == bq->reg_fault_flag)
		return;

	bq->prev_fault_flag = bq->reg_fault_flag;

	if (bq->reg_fault_flag & BQ2591X_VBUS_OVP_FLAG_MASK)
		pr_info("VBus OVP fault occured, current stat:%d",
				bq->reg_fault & BQ2591X_VBUS_OVP_STAT_MASK);

	if (bq->reg_fault_flag & BQ2591X_TSHUT_FLAG_MASK)
		pr_info("Thermal shutdown occured, current stat:%d",
				bq->reg_fault & BQ2591X_TSHUT_STAT_MASK);

	if (bq->reg_fault_flag & BQ2591X_BATOVP_FLAG_MASK)
		pr_info("Battery OVP fault occured, current stat:%d",
				bq->reg_fault & BQ2591X_BATOVP_STAT_MASK);

	if (bq->reg_fault_flag & BQ2591X_CFLY_FLAG_MASK)
		pr_info("CFLY fault occured, current stat:%d",
				bq->reg_fault & BQ2591X_CFLY_STAT_MASK);

	if (bq->reg_fault_flag & BQ2591X_TMR_FLAG_MASK)
		pr_info("Charge safety timer fault, current stat:%d",
				bq->reg_fault & BQ2591X_TMR_STAT_MASK);

	if (bq->reg_fault_flag & BQ2591X_CAP_COND_FLAG_MASK)
		pr_info("CAP conditon fault occured, current stat:%d",
				bq->reg_fault & BQ2591X_CAP_COND_STAT_MASK);
}


static irqreturn_t bq2591x_charger_interrupt(int irq, void *data)
{
	struct bq2591x *bq = data;
	int ret;
	u8  val;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_07);
	if (ret)
		return IRQ_HANDLED;
	bq->reg_stat = val;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_08);
	if (ret)
		return IRQ_HANDLED;
	bq->reg_fault = val;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_09);
	if (ret)
		return IRQ_HANDLED;
	bq->reg_stat_flag = val;

	ret = bq2591x_read_byte(bq, &val, BQ2591X_REG_0A);
	if (ret)
		return IRQ_HANDLED;
	bq->reg_fault_flag = val;

	bq2591x_stat_handler(bq);
	bq2591x_fault_handler(bq);

	bq2591x_dump_regs(bq);

	return IRQ_HANDLED;
}

static void bq2591x_monitor_workfunc(struct work_struct *work)
{
	struct bq2591x *bq = container_of(work, struct bq2591x, monitor_work.work);

	bq2591x_dump_regs(bq);

	schedule_delayed_work(&bq->monitor_work, 5 * HZ);
}

static int bq2591x_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct bq2591x *bq = NULL;
	int ret;
	struct power_supply_config parallel_psy_cfg = {};

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2591x), GFP_KERNEL);
	if (!bq) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}

	bq->dev = &client->dev;
	bq->client = client;

	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);

	ret = bq2591x_detect_device(bq);
	if (!ret && bq->part_no == BQ25910) {
		pr_info("charger device bq25910 detected, revision:%d\n",
							bq->revision);
	} else {
		pr_info("no bq25910 charger device found:%d\n", ret);
		return -ENODEV;
	}

	if (client->dev.of_node)
		bq2591x_parse_dt(&client->dev, bq);

	ret = bq2591x_init_device(bq);
	if (ret) {
		pr_err("device init failure: %d\n", ret);
		goto err_0;
	}

	bq->vadc_dev = qpnp_get_vadc(bq->dev, "chg");
	if (IS_ERR(bq->vadc_dev)) {
		ret = PTR_ERR(bq->vadc_dev);
		if (ret != -EPROBE_DEFER)
			pr_err("vadc property missing\n");
		return ret;
	}

	bq->max_fcc = INT_MAX;
	bq->c_health = -EINVAL;

	INIT_DELAYED_WORK(&bq->monitor_work, bq2591x_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->icl_softstart_work, bq2591x_icl_softstart_workfunc);
	INIT_DELAYED_WORK(&bq->fcc_softstart_work, bq2591x_fcc_softstart_workfunc);

	bq->parallel_psy_d.name	= "parallel";
	bq->parallel_psy_d.type	= POWER_SUPPLY_TYPE_PARALLEL;
	bq->parallel_psy_d.get_property = bq2591x_charger_get_property;
	bq->parallel_psy_d.set_property = bq2591x_charger_set_property;
	bq->parallel_psy_d.properties   = bq2591x_charger_properties;
	bq->parallel_psy_d.property_is_writeable = bq2591x_charger_is_writeable;
	bq->parallel_psy_d.num_properties = ARRAY_SIZE(bq2591x_charger_properties);

	parallel_psy_cfg.drv_data = bq;
	parallel_psy_cfg.num_supplicants = 0;
	bq->parallel_psy = devm_power_supply_register(bq->dev,
			&bq->parallel_psy_d,
			&parallel_psy_cfg);
	if (IS_ERR(bq->parallel_psy)) {
		pr_err("Couldn't register parallel psy rc=%ld\n",
				PTR_ERR(bq->parallel_psy));
		ret = PTR_ERR(bq->parallel_psy);
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				bq2591x_charger_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"bq2591x charger irq", bq);
		if (ret < 0) {
			pr_err("request irq for irq=%d failed,ret =%d\n",
				client->irq, ret);
			goto err_0;
		}
	}

	create_debugfs_entries(bq);

	ret = sysfs_create_group(&bq->dev->kobj, &bq2591x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

	pr_info("BQ2591X PARALLEL charger driver probe successfully\n");

	return 0;

err_0:
	power_supply_unregister(bq->parallel_psy);
	return ret;
}

static int bq2591x_charger_remove(struct i2c_client *client)
{
	struct bq2591x *bq = i2c_get_clientdata(client);

	devm_free_irq(&client->dev, client->irq, bq);

	cancel_delayed_work_sync(&bq->monitor_work);

	power_supply_unregister(bq->parallel_psy);

	mutex_destroy(&bq->i2c_rw_lock);

	debugfs_remove_recursive(bq->debug_root);
	sysfs_remove_group(&bq->dev->kobj, &bq2591x_attr_group);

	return 0;
}


static void bq2591x_charger_shutdown(struct i2c_client *client)
{
	struct bq2591x *bq = i2c_get_clientdata(client);

	devm_free_irq(&client->dev, client->irq, bq);

	cancel_delayed_work_sync(&bq->monitor_work);

	bq2591x_usb_suspend(bq, true);

	mutex_destroy(&bq->i2c_rw_lock);
	debugfs_remove_recursive(bq->debug_root);
	sysfs_remove_group(&bq->dev->kobj, &bq2591x_attr_group);

	pr_info("shutdown\n");

}

static struct of_device_id bq2591x_charger_match_table[] = {
	{.compatible = "ti,smbq2591x"},
	{},
};


static const struct i2c_device_id bq2591x_charger_id[] = {
	{ "bq2591x", BQ25910 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2591x_charger_id);

static struct i2c_driver bq2591x_charger_driver = {
	.driver		= {
		.name	= "smbq2591x",
		.of_match_table = bq2591x_charger_match_table,
	},
	.id_table	= bq2591x_charger_id,

	.probe		= bq2591x_charger_probe,
	.remove		= bq2591x_charger_remove,
	.shutdown   = bq2591x_charger_shutdown,
};

module_i2c_driver(bq2591x_charger_driver);

MODULE_DESCRIPTION("TI BQ2591x Charger Driver");
MODULE_LICENSE("GPL2");
