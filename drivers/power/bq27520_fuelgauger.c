/* Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

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
#include <asm/unaligned.h>
#include <linux/time.h>
#include <linux/i2c/bq27520.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/err.h>
#include <linux/msm-charger.h>

#define DRIVER_VERSION			"1.1.0"
/* Bq27520 standard data commands */
#define BQ27520_REG_CNTL		0x00
#define BQ27520_REG_AR			0x02
#define BQ27520_REG_ARTTE		0x04
#define BQ27520_REG_TEMP		0x06
#define BQ27520_REG_VOLT		0x08
#define BQ27520_REG_FLAGS		0x0A
#define BQ27520_REG_NAC			0x0C
#define BQ27520_REG_FAC			0x0e
#define BQ27520_REG_RM			0x10
#define BQ27520_REG_FCC			0x12
#define BQ27520_REG_AI			0x14
#define BQ27520_REG_TTE			0x16
#define BQ27520_REG_TTF			0x18
#define BQ27520_REG_SI			0x1a
#define BQ27520_REG_STTE		0x1c
#define BQ27520_REG_MLI			0x1e
#define BQ27520_REG_MLTTE		0x20
#define BQ27520_REG_AE			0x22
#define BQ27520_REG_AP			0x24
#define BQ27520_REG_TTECP		0x26
#define BQ27520_REG_SOH			0x28
#define BQ27520_REG_SOC			0x2c
#define BQ27520_REG_NIC			0x2e
#define BQ27520_REG_ICR			0x30
#define BQ27520_REG_LOGIDX		0x32
#define BQ27520_REG_LOGBUF		0x34
#define BQ27520_FLAG_DSC		BIT(0)
#define BQ27520_FLAG_FC			BIT(9)
#define BQ27520_FLAG_BAT_DET		BIT(3)
#define BQ27520_CS_DLOGEN		BIT(15)
#define BQ27520_CS_SS		    BIT(13)
/* Control subcommands */
#define BQ27520_SUBCMD_CTNL_STATUS  0x0000
#define BQ27520_SUBCMD_DEVCIE_TYPE  0x0001
#define BQ27520_SUBCMD_FW_VER  0x0002
#define BQ27520_SUBCMD_HW_VER  0x0003
#define BQ27520_SUBCMD_DF_CSUM  0x0004
#define BQ27520_SUBCMD_PREV_MACW   0x0007
#define BQ27520_SUBCMD_CHEM_ID   0x0008
#define BQ27520_SUBCMD_BD_OFFSET   0x0009
#define BQ27520_SUBCMD_INT_OFFSET  0x000a
#define BQ27520_SUBCMD_CC_VER   0x000b
#define BQ27520_SUBCMD_OCV  0x000c
#define BQ27520_SUBCMD_BAT_INS   0x000d
#define BQ27520_SUBCMD_BAT_REM   0x000e
#define BQ27520_SUBCMD_SET_HIB   0x0011
#define BQ27520_SUBCMD_CLR_HIB   0x0012
#define BQ27520_SUBCMD_SET_SLP   0x0013
#define BQ27520_SUBCMD_CLR_SLP   0x0014
#define BQ27520_SUBCMD_FCT_RES   0x0015
#define BQ27520_SUBCMD_ENABLE_DLOG  0x0018
#define BQ27520_SUBCMD_DISABLE_DLOG 0x0019
#define BQ27520_SUBCMD_SEALED   0x0020
#define BQ27520_SUBCMD_ENABLE_IT    0x0021
#define BQ27520_SUBCMD_DISABLE_IT   0x0023
#define BQ27520_SUBCMD_CAL_MODE  0x0040
#define BQ27520_SUBCMD_RESET   0x0041

#define ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN   (-2731)
#define BQ27520_INIT_DELAY ((HZ)*1)
#define BQ27520_POLLING_STATUS ((HZ)*3)
#define BQ27520_COULOMB_POLL ((HZ)*30)

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

struct bq27520_device_info;
struct bq27520_access_methods {
	int (*read)(u8 reg, int *rt_value, int b_single,
		struct bq27520_device_info *di);
};

struct bq27520_device_info {
	struct device				*dev;
	int					id;
	struct bq27520_access_methods		*bus;
	struct i2c_client			*client;
	const struct bq27520_platform_data	*pdata;
	struct work_struct			counter;
	/* 300ms delay is needed after bq27520 is powered up
	 * and before any successful I2C transaction
	 */
	struct  delayed_work			hw_config;
	uint32_t				irq;
};

enum {
	GET_BATTERY_STATUS,
	GET_BATTERY_TEMPERATURE,
	GET_BATTERY_VOLTAGE,
	GET_BATTERY_CAPACITY,
	NUM_OF_STATUS,
};

struct bq27520_status {
	/* Informations owned and maintained by Bq27520 driver, updated
	 * by poller or SOC_INT interrupt, decoupling from I/Oing
	 * hardware directly
	 */
	int			status[NUM_OF_STATUS];
	spinlock_t		lock;
	struct delayed_work	poller;
};

static struct bq27520_status current_battery_status;
static struct bq27520_device_info *bq27520_di;
static int coulomb_counter;
static spinlock_t lock; /* protect access to coulomb_counter */
static struct timer_list timer; /* charge counter timer every 30 secs */

static int bq27520_i2c_txsubcmd(u8 reg, unsigned short subcmd,
		struct bq27520_device_info *di);

static int bq27520_read(u8 reg, int *rt_value, int b_single,
			struct bq27520_device_info *di)
{
	return di->bus->read(reg, rt_value, b_single, di);
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
static int bq27520_battery_temperature(struct bq27520_device_info *di)
{
	int ret, temp = 0;

	ret = bq27520_read(BQ27520_REG_TEMP, &temp, 0, di);
	if (ret) {
		dev_err(di->dev, "error %d reading temperature\n", ret);
		return ret;
	}

	return temp + ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN;
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
static int bq27520_battery_voltage(struct bq27520_device_info *di)
{
	int ret, volt = 0;

	ret = bq27520_read(BQ27520_REG_VOLT, &volt, 0, di);
	if (ret) {
		dev_err(di->dev, "error %d reading voltage\n", ret);
		return ret;
	}

	return volt;
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27520_battery_rsoc(struct bq27520_device_info *di)
{
	int ret, rsoc = 0;

	ret = bq27520_read(BQ27520_REG_SOC, &rsoc, 0, di);

	if (ret) {
		dev_err(di->dev,
			"error %d reading relative State-of-Charge\n", ret);
		return ret;
	}

	return rsoc;
}

static void bq27520_cntl_cmd(struct bq27520_device_info *di,
				int subcmd)
{
	bq27520_i2c_txsubcmd(BQ27520_REG_CNTL, subcmd, di);
}

/*
 * i2c specific code
 */
static int bq27520_i2c_txsubcmd(u8 reg, unsigned short subcmd,
		struct bq27520_device_info *di)
{
	struct i2c_msg msg;
	unsigned char data[3];

	if (!di->client)
		return -ENODEV;

	memset(data, 0, sizeof(data));
	data[0] = reg;
	data[1] = subcmd & 0x00FF;
	data[2] = (subcmd & 0xFF00) >> 8;

	msg.addr = di->client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	if (i2c_transfer(di->client->adapter, &msg, 1) < 0)
		return -EIO;

	return 0;
}

static int bq27520_chip_config(struct bq27520_device_info *di)
{
	int flags = 0, ret = 0;

	bq27520_cntl_cmd(di, BQ27520_SUBCMD_CTNL_STATUS);
	udelay(66);
	ret = bq27520_read(BQ27520_REG_CNTL, &flags, 0, di);
	if (ret < 0) {
		dev_err(di->dev, "error %d reading register %02x\n",
			 ret, BQ27520_REG_CNTL);
		return ret;
	}
	udelay(66);

	bq27520_cntl_cmd(di, BQ27520_SUBCMD_ENABLE_IT);
	udelay(66);

	if (di->pdata->enable_dlog && !(flags & BQ27520_CS_DLOGEN)) {
		bq27520_cntl_cmd(di, BQ27520_SUBCMD_ENABLE_DLOG);
		udelay(66);
	}

	return 0;
}

static void bq27520_every_30secs(unsigned long data)
{
	struct bq27520_device_info *di = (struct bq27520_device_info *)data;

	schedule_work(&di->counter);
	mod_timer(&timer, jiffies + BQ27520_COULOMB_POLL);
}

static void bq27520_coulomb_counter_work(struct work_struct *work)
{
	int value = 0, temp = 0, index = 0, ret = 0, count = 0;
	struct bq27520_device_info *di;
	unsigned long flags;

	di = container_of(work, struct bq27520_device_info, counter);

	/* retrieve 30 values from FIFO of coulomb data logging buffer
	 * and average over time
	 */
	do {
		ret = bq27520_read(BQ27520_REG_LOGBUF, &temp, 0, di);
		if (ret < 0)
			break;
		if (temp != 0x7FFF) {
			++count;
			value += temp;
		}
		udelay(66);
		ret = bq27520_read(BQ27520_REG_LOGIDX, &index, 0, di);
		if (ret < 0)
			break;
		udelay(66);
	} while (index != 0 || temp != 0x7FFF);

	if (ret < 0) {
		dev_err(di->dev, "Error %d reading datalog register\n", ret);
		return;
	}

	if (count) {
		spin_lock_irqsave(&lock, flags);
		coulomb_counter = value/count;
		spin_unlock_irqrestore(&lock, flags);
	}
}

static int bq27520_is_battery_present(void)
{
	return 1;
}

static int bq27520_is_battery_temp_within_range(void)
{
	return 1;
}

static int bq27520_is_battery_id_valid(void)
{
	return 1;
}

static int bq27520_status_getter(int function)
{
	int status = 0;
	unsigned long flags;

	spin_lock_irqsave(&current_battery_status.lock, flags);
	status = current_battery_status.status[function];
	spin_unlock_irqrestore(&current_battery_status.lock, flags);

	return status;
}

static int bq27520_get_battery_mvolts(void)
{
	return bq27520_status_getter(GET_BATTERY_VOLTAGE);
}

static int bq27520_get_battery_temperature(void)
{
	return bq27520_status_getter(GET_BATTERY_TEMPERATURE);
}

static int bq27520_get_battery_status(void)
{
	return bq27520_status_getter(GET_BATTERY_STATUS);
}

static int bq27520_get_remaining_capacity(void)
{
	return bq27520_status_getter(GET_BATTERY_CAPACITY);
}

static struct msm_battery_gauge bq27520_batt_gauge = {
	.get_battery_mvolts		= bq27520_get_battery_mvolts,
	.get_battery_temperature	= bq27520_get_battery_temperature,
	.is_battery_present		= bq27520_is_battery_present,
	.is_battery_temp_within_range	= bq27520_is_battery_temp_within_range,
	.is_battery_id_valid		= bq27520_is_battery_id_valid,
	.get_battery_status		= bq27520_get_battery_status,
	.get_batt_remaining_capacity	= bq27520_get_remaining_capacity,
};

static void update_current_battery_status(int data)
{
	int status[4], ret = 0;
	unsigned long flag;

	memset(status, 0, sizeof status);
	ret = bq27520_battery_rsoc(bq27520_di);
	status[GET_BATTERY_CAPACITY] = (ret < 0) ? 0 : ret;

	status[GET_BATTERY_VOLTAGE] = bq27520_battery_voltage(bq27520_di);
	status[GET_BATTERY_TEMPERATURE] =
				bq27520_battery_temperature(bq27520_di);

	spin_lock_irqsave(&current_battery_status.lock, flag);
	current_battery_status.status[GET_BATTERY_STATUS] = data;
	current_battery_status.status[GET_BATTERY_VOLTAGE] =
						status[GET_BATTERY_VOLTAGE];
	current_battery_status.status[GET_BATTERY_TEMPERATURE] =
						status[GET_BATTERY_TEMPERATURE];
	current_battery_status.status[GET_BATTERY_CAPACITY] =
						status[GET_BATTERY_CAPACITY];
	spin_unlock_irqrestore(&current_battery_status.lock, flag);
}

/* only if battery charging satus changes then notify msm_charger. otherwise
 * only refresh current_batter_status
 */
static int if_notify_msm_charger(int *data)
{
	int ret = 0, flags = 0, status = 0;
	unsigned long flag;

	ret = bq27520_read(BQ27520_REG_FLAGS, &flags, 0, bq27520_di);
	if (ret < 0) {
		dev_err(bq27520_di->dev, "error %d reading register %02x\n",
			ret, BQ27520_REG_FLAGS);
		status = POWER_SUPPLY_STATUS_UNKNOWN;
	} else {
		if (flags & BQ27520_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (flags & BQ27520_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	}

	*data = status;
	spin_lock_irqsave(&current_battery_status.lock, flag);
	ret = (status != current_battery_status.status[GET_BATTERY_STATUS]);
	spin_unlock_irqrestore(&current_battery_status.lock, flag);
	return ret;
}

static void battery_status_poller(struct work_struct *work)
{
	int status = 0, temp = 0;

	temp = if_notify_msm_charger(&status);
	update_current_battery_status(status);
	if (temp)
		msm_charger_notify_event(NULL, CHG_BATT_STATUS_CHANGE);

	schedule_delayed_work(&current_battery_status.poller,
				BQ27520_POLLING_STATUS);
}

static void bq27520_hw_config(struct work_struct *work)
{
	int ret = 0, flags = 0, type = 0, fw_ver = 0, status = 0;
	struct bq27520_device_info *di;

	di  = container_of(work, struct bq27520_device_info, hw_config.work);

	pr_debug(KERN_INFO "Enter bq27520_hw_config\n");
	ret = bq27520_chip_config(di);
	if (ret) {
		dev_err(di->dev, "Failed to config Bq27520 ret = %d\n", ret);
		return;
	}
	/* bq27520 is ready for access, update current_battery_status by reading
	 * from hardware
	 */
	if_notify_msm_charger(&status);
	update_current_battery_status(status);
	msm_battery_gauge_register(&bq27520_batt_gauge);
	msm_charger_notify_event(NULL, CHG_BATT_STATUS_CHANGE);

	enable_irq(di->irq);

	/* poll battery status every 3 seconds, if charging status changes,
	 * notify msm_charger
	 */
	schedule_delayed_work(&current_battery_status.poller,
				BQ27520_POLLING_STATUS);

	if (di->pdata->enable_dlog) {
		schedule_work(&di->counter);
		init_timer(&timer);
		timer.function = &bq27520_every_30secs;
		timer.data = (unsigned long)di;
		timer.expires = jiffies + BQ27520_COULOMB_POLL;
		add_timer(&timer);
	}

	bq27520_cntl_cmd(di, BQ27520_SUBCMD_CTNL_STATUS);
	udelay(66);
	bq27520_read(BQ27520_REG_CNTL, &flags, 0, di);
	bq27520_cntl_cmd(di, BQ27520_SUBCMD_DEVCIE_TYPE);
	udelay(66);
	bq27520_read(BQ27520_REG_CNTL, &type, 0, di);
	bq27520_cntl_cmd(di, BQ27520_SUBCMD_FW_VER);
	udelay(66);
	bq27520_read(BQ27520_REG_CNTL, &fw_ver, 0, di);

	dev_info(di->dev, "DEVICE_TYPE is 0x%02X, FIRMWARE_VERSION\
		is 0x%02X\n", type, fw_ver);
	dev_info(di->dev, "Complete bq27520 configuration 0x%02X\n", flags);
}

static int bq27520_read_i2c(u8 reg, int *rt_value, int b_single,
			struct bq27520_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	err = i2c_transfer(client->adapter, msg, 1);

	if (err >= 0) {
		if (!b_single)
			msg->len = 2;
		else
			msg->len = 1;

		msg->flags = I2C_M_RD;
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0) {
			if (!b_single)
				*rt_value = get_unaligned_le16(data);
			else
				*rt_value = data[0];

			return 0;
		}
	}
	return err;
}

#ifdef CONFIG_BQ27520_TEST_ENABLE
static int reg;
static int subcmd;
static ssize_t bq27520_read_stdcmd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	int temp = 0;
	struct platform_device *client;
	struct bq27520_device_info *di;

	client = to_platform_device(dev);
	di = platform_get_drvdata(client);

	if (reg <= BQ27520_REG_ICR && reg > 0x00) {
		ret = bq27520_read(reg, &temp, 0, di);
		if (ret)
			ret = snprintf(buf, PAGE_SIZE, "Read Error!\n");
		else
			ret = snprintf(buf, PAGE_SIZE, "0x%02x\n", temp);
	} else
		ret = snprintf(buf, PAGE_SIZE, "Register Error!\n");

	return ret;
}

static ssize_t bq27520_write_stdcmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;

	sscanf(buf, "%x", &cmd);
	reg = cmd;
	dev_info(dev, "recv'd cmd is 0x%02X\n", reg);
	return ret;
}

static ssize_t bq27520_read_subcmd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, temp = 0;
	struct platform_device *client;
	struct bq27520_device_info *di;

	client = to_platform_device(dev);
	di = platform_get_drvdata(client);

	if (subcmd == BQ27520_SUBCMD_DEVCIE_TYPE ||
		 subcmd == BQ27520_SUBCMD_FW_VER ||
		 subcmd == BQ27520_SUBCMD_HW_VER ||
		 subcmd == BQ27520_SUBCMD_CHEM_ID) {

		bq27520_cntl_cmd(di, subcmd);/* Retrieve Chip status */
		udelay(66);
		ret = bq27520_read(BQ27520_REG_CNTL, &temp, 0, di);

		if (ret)
			ret = snprintf(buf, PAGE_SIZE, "Read Error!\n");
		else
			ret = snprintf(buf, PAGE_SIZE, "0x%02x\n", temp);
	} else
		ret = snprintf(buf, PAGE_SIZE, "Register Error!\n");

	return ret;
}

static ssize_t bq27520_write_subcmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int cmd;

	sscanf(buf, "%x", &cmd);
	subcmd = cmd;
	return ret;
}

static DEVICE_ATTR(std_cmd, S_IRUGO|S_IWUGO, bq27520_read_stdcmd,
	bq27520_write_stdcmd);
static DEVICE_ATTR(sub_cmd, S_IRUGO|S_IWUGO, bq27520_read_subcmd,
	bq27520_write_subcmd);
static struct attribute *fs_attrs[] = {
	&dev_attr_std_cmd.attr,
	&dev_attr_sub_cmd.attr,
	NULL,
};
static struct attribute_group fs_attr_group = {
	.attrs = fs_attrs,
};

static struct platform_device this_device = {
	.name = "bq27520-test",
	.id = -1,
	.dev.platform_data = NULL,
};
#endif

static irqreturn_t soc_irqhandler(int irq, void *dev_id)
{
	int status = 0, temp = 0;

	temp = if_notify_msm_charger(&status);
	update_current_battery_status(status);
	if (temp)
		msm_charger_notify_event(NULL, CHG_BATT_STATUS_CHANGE);
	return IRQ_HANDLED;
}

static struct regulator *vreg_bq27520;
static int bq27520_power(bool enable, struct bq27520_device_info *di)
{
	int rc = 0, ret;
	const struct bq27520_platform_data *platdata;

	platdata = di->pdata;
	if (enable) {
		/* switch on Vreg_S3 */
		rc = regulator_enable(vreg_bq27520);
		if (rc < 0) {
			dev_err(di->dev, "%s: vreg %s %s failed (%d)\n",
				__func__, platdata->vreg_name, "enable", rc);
			goto vreg_fail;
		}

		/* Battery gauge enable and switch on onchip 2.5V LDO */
		rc = gpio_request(platdata->chip_en, "GAUGE_EN");
		if (rc) {
			dev_err(di->dev, "%s: fail to request gpio %d (%d)\n",
				__func__, platdata->chip_en, rc);
			goto vreg_fail;
		}

		gpio_direction_output(platdata->chip_en, 0);
		gpio_set_value(platdata->chip_en, 1);
		rc = gpio_request(platdata->soc_int, "GAUGE_SOC_INT");
		if (rc) {
			dev_err(di->dev, "%s: fail to request gpio %d (%d)\n",
				__func__, platdata->soc_int, rc);
			goto gpio_fail;
		}
		gpio_direction_input(platdata->soc_int);
		di->irq = gpio_to_irq(platdata->soc_int);
		rc = request_threaded_irq(di->irq, NULL, soc_irqhandler,
				IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
				"BQ27520_IRQ", di);
		if (rc) {
			dev_err(di->dev, "%s: fail to request irq %d (%d)\n",
				__func__, platdata->soc_int, rc);
			goto irqreq_fail;
		} else {
			disable_irq_nosync(di->irq);
		}
	} else {
		free_irq(di->irq, di);
		gpio_free(platdata->soc_int);
		/* switch off on-chip 2.5V LDO and disable Battery gauge */
		gpio_set_value(platdata->chip_en, 0);
		gpio_free(platdata->chip_en);
		/* switch off Vreg_S3 */
		rc = regulator_disable(vreg_bq27520);
		if (rc < 0) {
			dev_err(di->dev, "%s: vreg %s %s failed (%d)\n",
				__func__, platdata->vreg_name, "disable", rc);
			goto vreg_fail;
		}
	}
	return rc;

irqreq_fail:
	gpio_free(platdata->soc_int);
gpio_fail:
	gpio_set_value(platdata->chip_en, 0);
	gpio_free(platdata->chip_en);
vreg_fail:
	ret = !enable ? regulator_enable(vreg_bq27520) :
		regulator_disable(vreg_bq27520);
	if (ret < 0) {
		dev_err(di->dev, "%s: vreg %s %s failed (%d) in err path\n",
			__func__, platdata->vreg_name,
			!enable ? "enable" : "disable", ret);
	}
	return rc;
}

static int bq27520_dev_setup(bool enable, struct bq27520_device_info *di)
{
	int rc;
	const struct bq27520_platform_data *platdata;

	platdata = di->pdata;
	if (enable) {
		/* enable and set voltage Vreg_S3 */
		vreg_bq27520 = regulator_get(NULL,
				platdata->vreg_name);
		if (IS_ERR(vreg_bq27520)) {
			dev_err(di->dev, "%s: regulator get of %s\
				failed (%ld)\n", __func__, platdata->vreg_name,
				PTR_ERR(vreg_bq27520));
			rc = PTR_ERR(vreg_bq27520);
			goto vreg_get_fail;
		}
		rc = regulator_set_voltage(vreg_bq27520,
			platdata->vreg_value, platdata->vreg_value);
		if (rc) {
			dev_err(di->dev, "%s: regulator_set_voltage(%s) failed\
				 (%d)\n", __func__, platdata->vreg_name, rc);
			goto vreg_get_fail;
		}
	} else {
		regulator_put(vreg_bq27520);
	}
	return 0;

vreg_get_fail:
	regulator_put(vreg_bq27520);
	return rc;
}

static int bq27520_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq27520_device_info *di;
	struct bq27520_access_methods *bus;
	const struct bq27520_platform_data  *pdata;
	int num, retval = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	pdata = client->dev.platform_data;

	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}
	di->id = num;
	di->pdata = pdata;

	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus) {
		dev_err(&client->dev, "failed to allocate data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	i2c_set_clientdata(client, di);
	di->dev = &client->dev;
	bus->read = &bq27520_read_i2c;
	di->bus = bus;
	di->client = client;

#ifdef CONFIG_BQ27520_TEST_ENABLE
	platform_set_drvdata(&this_device, di);
	retval = platform_device_register(&this_device);
	if (!retval) {
		retval = sysfs_create_group(&this_device.dev.kobj,
			 &fs_attr_group);
		if (retval)
			goto batt_failed_3;
	} else
		goto batt_failed_3;
#endif

	retval = bq27520_dev_setup(true, di);
	if (retval) {
		dev_err(&client->dev, "failed to setup ret = %d\n", retval);
		goto batt_failed_3;
	}

	retval = bq27520_power(true, di);
	if (retval) {
		dev_err(&client->dev, "failed to powerup ret = %d\n", retval);
		goto batt_failed_3;
	}

	spin_lock_init(&lock);

	bq27520_di = di;
	if (pdata->enable_dlog)
		INIT_WORK(&di->counter, bq27520_coulomb_counter_work);

	INIT_DELAYED_WORK(&current_battery_status.poller,
			battery_status_poller);
	INIT_DELAYED_WORK(&di->hw_config, bq27520_hw_config);
	schedule_delayed_work(&di->hw_config, BQ27520_INIT_DELAY);

	return 0;

batt_failed_3:
	kfree(bus);
batt_failed_2:
	kfree(di);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27520_battery_remove(struct i2c_client *client)
{
	struct bq27520_device_info *di = i2c_get_clientdata(client);

	if (di->pdata->enable_dlog) {
		del_timer_sync(&timer);
		cancel_work_sync(&di->counter);
		bq27520_cntl_cmd(di, BQ27520_SUBCMD_DISABLE_DLOG);
		udelay(66);
	}

	bq27520_cntl_cmd(di, BQ27520_SUBCMD_DISABLE_IT);
	cancel_delayed_work_sync(&di->hw_config);
	cancel_delayed_work_sync(&current_battery_status.poller);

	bq27520_dev_setup(false, di);
	bq27520_power(false, di);

	kfree(di->bus);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);
	return 0;
}

#ifdef CONFIG_PM
static int bq27520_suspend(struct device *dev)
{
	struct bq27520_device_info *di = dev_get_drvdata(dev);

	disable_irq_nosync(di->irq);
	if (di->pdata->enable_dlog) {
		del_timer_sync(&timer);
		cancel_work_sync(&di->counter);
	}

	cancel_delayed_work_sync(&current_battery_status.poller);
	return 0;
}

static int bq27520_resume(struct device *dev)
{
	struct bq27520_device_info *di = dev_get_drvdata(dev);

	enable_irq(di->irq);
	if (di->pdata->enable_dlog)
		add_timer(&timer);

	schedule_delayed_work(&current_battery_status.poller,
				BQ27520_POLLING_STATUS);
	return 0;
}

static const struct dev_pm_ops bq27520_pm_ops = {
	.suspend = bq27520_suspend,
	.resume = bq27520_resume,
};
#endif

static const struct i2c_device_id bq27520_id[] = {
	{ "bq27520", 1 },
	{},
};
MODULE_DEVICE_TABLE(i2c, BQ27520_id);

static struct i2c_driver bq27520_battery_driver = {
	.driver = {
		.name = "bq27520-battery",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &bq27520_pm_ops,
#endif
	},
	.probe = bq27520_battery_probe,
	.remove = bq27520_battery_remove,
	.id_table = bq27520_id,
};

static void init_battery_status(void)
{
	spin_lock_init(&current_battery_status.lock);
	current_battery_status.status[GET_BATTERY_STATUS] =
			POWER_SUPPLY_STATUS_UNKNOWN;
}

static int __init bq27520_battery_init(void)
{
	int ret;

	/* initialize current_battery_status, and register with msm-charger */
	init_battery_status();

	ret = i2c_add_driver(&bq27520_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register driver ret = %d\n", ret);

	return ret;
}
module_init(bq27520_battery_init);

static void __exit bq27520_battery_exit(void)
{
	i2c_del_driver(&bq27520_battery_driver);
	msm_battery_gauge_unregister(&bq27520_batt_gauge);
}
module_exit(bq27520_battery_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("BQ27520 battery monitor driver");
