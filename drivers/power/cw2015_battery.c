/*
 * Gas_Gauge driver for CW2015/2013
 * Copyright (C) 2012, CellWise
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Authors: ChenGang <ben.chen@cellwise-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.And this driver depends on
 * I2C and uses IIC bus for communication with the host.
 *
 */
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/power/cw2015_battery.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <linux/wakelock.h>


#define REG_VERSION             	0x0
#define REG_VCELL               	0x2
#define REG_SOC                 	0x4
#define REG_RRT_ALERT           	0x6
#define REG_CONFIG              	0x8
#define REG_MODE                	0xA
#define REG_BATINFO             	0x10

#define MODE_SLEEP_MASK         	(0x3<<6)
#define MODE_SLEEP              	(0x3<<6)
#define MODE_NORMAL             	(0x0<<6)
#define MODE_QUICK_START        	(0x3<<4)
#define MODE_RESTART            	(0xf<<0)

#define CONFIG_UPDATE_FLG       	(0x1<<1)
#define ATHD                    	(0x0<<3)


#define BATTERY_UP_MAX_CHANGE   	420
#define BATTERY_DOWN_CHANGE   	60
#define BATTERY_DOWN_MIN_CHANGE_RUN 	30
#define BATTERY_DOWN_MIN_CHANGE_SLEEP 	1800
#define BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE 	1800



#define BAT_LOW_INTERRUPT    	0
#define INVALID_GPIO        	(-1)
#define GPIO_LOW             	0
#define GPIO_HIGH            	1
#define USB_CHARGER_MODE        	1
#define AC_CHARGER_MODE         	2
int cw_capacity;

struct cw_battery {
	struct i2c_client *client;
	struct workqueue_struct *battery_workqueue;
	struct delayed_work battery_delay_work;

	struct delayed_work bat_low_wakeup_work;
	const struct cw_bat_platform_data *plat_data;
	struct power_supply rk_bat;
	struct power_supply	*batt_psy;



	long sleep_time_capacity_change;
	long run_time_capacity_change;

	long sleep_time_charge_start;
	long run_time_charge_start;

	int charger_mode;
	int charger_init_mode;
	int capacity;
	int voltage;
	int status;
	int time_to_empty;
	int alt;

	int bat_change;
	struct regulator *vcc_i2c;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
};


static u8 config_info[SIZE_BATINFO] = {
	#include "profile_WT801_88509_NT_XinWangDa.h"
};
static u8 config_info_xinwangda[SIZE_BATINFO] = {
	#include "profile_WT801_88509_NT_XinWangDa.h"
};
static u8 config_info_feimaotui[SIZE_BATINFO] = {
	#include "profile_WT1001_88509_NT_Feimaotui.h"
};




/* write data to address */
static int cw_i2c_write(
	struct i2c_client *client,
	u8 addr,
	u8 *pdata,
	unsigned int datalen)
{
	int ret = 0;
	u8 tmp_buf[128];
	unsigned int bytelen = 0;

	if (datalen > 125) {
		pr_debug("%s too big datalen = %d!\n", __func__, datalen);
		return -EPERM;
	}

	tmp_buf[0] = addr;
	bytelen++;

	if (datalen != 0 && pdata != NULL) {
		memcpy(&tmp_buf[bytelen], pdata, datalen);
		bytelen += datalen;
	}
	ret = i2c_master_send(client, tmp_buf, bytelen);
	return ret;
}

/* read data from addr */
static int cw_i2c_read(
	struct i2c_client *client,
	u8 addr,
	u8 *pdata,
	unsigned int datalen)
{
	int ret = 0;
	if (datalen > 126) {
		pr_debug("%s too big datalen = %d!\n", __func__, datalen);
		return -EPERM;
	}

	/* set data address */
	ret = cw_i2c_write(client, addr, NULL, 0);
	if (ret < 0) {
		pr_debug("%s set data address fail!, ret is %d\n", __func__, ret);
		return ret;
	}
	/* read data */
	return i2c_master_recv(client, pdata, datalen);
}

static int cw_update_config_info(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val;
	int i;
	u8 reset_val;

	pr_debug("func: %s-------\n", __func__);

	/* make sure no in sleep mode */
	ret = cw_i2c_read(cw_bat->client, REG_MODE, &reg_val, 1);
	if (ret < 0)
		return ret;

	reset_val = reg_val;
	if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
		dev_err(&cw_bat->client->dev, "Error, device in sleep mode, cannot update battery info\n");
		return -EPERM;
	}

	/* update new battery info */
	for (i = 0; i < SIZE_BATINFO; i++) {
		pr_debug("cw_bat->plat_data->cw_bat_config_info[%d] = 0x%x\n", i, \
					cw_bat->plat_data->cw_bat_config_info[i]);
		ret = cw_i2c_write(cw_bat->client, REG_BATINFO + i, &cw_bat->plat_data->cw_bat_config_info[i], 1);

		if (ret < 0)
			return ret;
	}

	/* readback & check */
	for (i = 0; i < SIZE_BATINFO; i++) {
		ret = cw_i2c_read(cw_bat->client, REG_BATINFO + i, &reg_val, 1);
		if (reg_val != cw_bat->plat_data->cw_bat_config_info[i])
			return -EPERM;
	}

	/* set cw2015/cw2013 to use new battery info */
	ret = cw_i2c_read(cw_bat->client, REG_CONFIG, &reg_val, 1);
	if (ret < 0)
		return ret;

	reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
	reg_val &= 0x07;                /* clear ATHD */
	reg_val |= ATHD;                /* set ATHD */
	ret = cw_i2c_write(cw_bat->client, REG_CONFIG, &reg_val, 1);
	if (ret < 0)
		return ret;

	/* check 2015/cw2013 for ATHD & update_flag */
	ret = cw_i2c_read(cw_bat->client, REG_CONFIG, &reg_val, 1);
	if (ret < 0)
		return ret;

	if (!(reg_val & CONFIG_UPDATE_FLG)) {

		pr_debug("update flag for new battery info have not set..\n");
		reg_val = MODE_SLEEP;
		ret = cw_i2c_write(cw_bat->client, REG_MODE, &reg_val, 1);
		pr_debug("report battery capacity error");
		return -EPERM;

	}

	if ((reg_val & 0xf8) != ATHD) {
		pr_debug("the new ATHD have not set..\n");
	}

	/* reset */
	reset_val &= ~(MODE_RESTART);
	reg_val = reset_val | MODE_RESTART;
	ret = cw_i2c_write(cw_bat->client, REG_MODE, &reg_val, 1);
	if (ret < 0)
		return ret;

	msleep(10);
	ret = cw_i2c_write(cw_bat->client, REG_MODE, &reset_val, 1);
	if (ret < 0)
		return ret;

	return 0;
}

static int cw_check_ic(struct cw_battery *cw_bat)
{
	int ret = 1 ;
	u8 reg_val = 0;

	ret = cw_i2c_read(cw_bat->client, REG_MODE/*REG_VERSION*/, &reg_val, 1);

	if (ret < 0) {
		if (ret == -107) {
				return -107;
		} else {
			return ret;
		}
	}
	if ((reg_val == 0xC0) || (reg_val == 0x00)) {
		ret = 0;
	}

	return ret;
}

static int cw_init(struct cw_battery *cw_bat)
{
	int ret;
	int i;
	u8 reg_val = MODE_SLEEP;
	if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
		reg_val = MODE_NORMAL;
		ret = cw_i2c_write(cw_bat->client, REG_MODE, &reg_val, 1);
		if (ret < 0)
			return ret;
	}

	ret = cw_i2c_read(cw_bat->client, REG_CONFIG, &reg_val, 1);
	if (ret < 0)
			return ret;

	if ((reg_val & 0xf8) != ATHD) {
		pr_debug("the new ATHD have not set\n");
		reg_val &= 0x07;    /* clear ATHD */
		reg_val |= ATHD;    /* set ATHD */
		ret = cw_i2c_write(cw_bat->client, REG_CONFIG, &reg_val, 1);
		if (ret < 0)
			return ret;
	}

	ret = cw_i2c_read(cw_bat->client, REG_CONFIG, &reg_val, 1);
	if (ret < 0)
		return ret;

	if (!(reg_val & CONFIG_UPDATE_FLG)) {
		pr_debug("update flag for new battery info have not set\n");
		ret = cw_update_config_info(cw_bat);
		if (ret < 0)
			return ret;
	} else {
		for (i = 0; i < SIZE_BATINFO; i++) {
			ret = cw_i2c_read(cw_bat->client, (REG_BATINFO + i), &reg_val, 1);
			if (ret < 0)
				return ret;

			if (cw_bat->plat_data->cw_bat_config_info[i] != reg_val)
				break;
		}

		if (i != SIZE_BATINFO) {
			pr_debug("update flag for new battery info have not set\n");
			ret = cw_update_config_info(cw_bat);
			if (ret < 0)
				return ret;
		}
	}

	for (i = 0; i < 30; i++) {
		msleep(100);
		ret = cw_i2c_read(cw_bat->client, REG_SOC, &reg_val, 1);
		if (ret < 0)
			return ret;
		else if (reg_val <= 0x64)
			break;

		if (i > 25)
			dev_err(&cw_bat->client->dev, "cw2015/cw2013 input unvalid power error\n");

	}
	if (i >= 30) {
		reg_val = MODE_SLEEP;
		ret = cw_i2c_write(cw_bat->client, REG_MODE, &reg_val, 1);
		pr_debug("report battery capacity error");
		return -EPERM;
	}
	return 0;
}

static void cw_update_time_member_charge_start(struct cw_battery *cw_bat)
{
	struct timespec ts;
	int new_run_time;
	int new_sleep_time;

	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;

	cw_bat->run_time_charge_start = new_run_time;
	cw_bat->sleep_time_charge_start = new_sleep_time;
}

static void cw_update_time_member_capacity_change(struct cw_battery *cw_bat)
{
	struct timespec ts;
	int new_run_time;
	int new_sleep_time;

	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;

	cw_bat->run_time_capacity_change = new_run_time;
	cw_bat->sleep_time_capacity_change = new_sleep_time;
}

static int cw_quickstart(struct cw_battery *cw_bat)
{
	int ret = 0;
	u8 reg_val = MODE_QUICK_START;

	ret = cw_i2c_write(cw_bat->client, REG_MODE, &reg_val, 1);
	if (ret < 0) {
		dev_err(&cw_bat->client->dev, "Error quick start1\n");
		return ret;
	}

	reg_val = MODE_NORMAL;
	ret = cw_i2c_write(cw_bat->client, REG_MODE, &reg_val, 1);
	if (ret < 0) {
		dev_err(&cw_bat->client->dev, "Error quick start2\n");
		return ret;
	}
	return 1;
}

static int cw_get_capacity(struct cw_battery *cw_bat)
{

	int ret;
	u8 reg_val[2];

	struct timespec ts;
	long new_run_time;
	long new_sleep_time;
	long capacity_or_aconline_time;
	int allow_change;
	int allow_capacity;
	static int if_quickstart;
	static int jump_flag;
		static int jump_flag2;
	static int reset_loop;
	int charge_time;
	u8 reset_val;




	ret = cw_i2c_read(cw_bat->client, REG_SOC, reg_val, 2);
	if (ret < 0)
		return ret;

	cw_capacity = reg_val[0];
	if ((cw_capacity < 0) || (cw_capacity > 100)) {
		dev_err(&cw_bat->client->dev, "get cw_capacity error; cw_capacity = %d\n", cw_capacity);
		reset_loop++;

		if (reset_loop > 5) {
			reset_val = MODE_SLEEP;
			ret = cw_i2c_write(cw_bat->client, REG_MODE, &reset_val, 1);
			if (ret < 0)
				return ret;
			reset_val = MODE_NORMAL;
			msleep(10);
			ret = cw_i2c_write(cw_bat->client, REG_MODE, &reset_val, 1);
			if (ret < 0)
				return ret;
			pr_debug("report battery capacity error");
			ret = cw_update_config_info(cw_bat);
			if (ret)
				return ret;
			reset_loop = 0;
		}
		return cw_capacity;
	} else {
		reset_loop = 0;
	}

	if (cw_capacity == 0)
				pr_debug("the cw201x capacity is 0 !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
	else
				pr_debug("the cw201x capacity is %d, funciton: %s\n", cw_capacity, __func__);



	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;

	if (((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 30/*9*/)))
				|| ((cw_bat->charger_mode == 0) && (cw_capacity == (cw_bat->capacity + 1)))) {

		if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) {
			cw_capacity = cw_bat->capacity;
		}
	}

	if ((cw_bat->charger_mode > 0) && (cw_capacity >= 95) && (cw_capacity <= cw_bat->capacity)) {

		capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
		capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
		allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_UP_MAX_CHANGE;
		if (allow_change > 0) {
			allow_capacity = cw_bat->capacity + allow_change;
			cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
			jump_flag = 1;
		} else if (cw_capacity <= cw_bat->capacity) {
			cw_capacity = cw_bat->capacity;
		}

	} else if ((cw_bat->charger_mode == 0) && cw_bat->capacity == 100 && cw_capacity < cw_bat->capacity && jump_flag2 == 0) {
		cw_capacity = cw_bat->capacity;
		jump_flag2 = 1;
	} else if ((cw_bat->charger_mode == 0) && (cw_capacity <= cw_bat->capacity) && (cw_capacity >= 90) && ((jump_flag == 1) || (jump_flag2 == 1))) {
		capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
		capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
		allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_DOWN_CHANGE;
		if (allow_change > 0) {
			allow_capacity = cw_bat->capacity - allow_change;
			if (cw_capacity >= allow_capacity) {
				jump_flag = 0;
				jump_flag2 = 0;
			} else{
				cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
			}
		} else if (cw_capacity <= cw_bat->capacity) {
			cw_capacity = cw_bat->capacity;
		}
	}

	if ((cw_capacity == 0) && (cw_bat->capacity > 1)) {
		allow_change = ((new_run_time - cw_bat->run_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_RUN);
		allow_change += ((new_sleep_time - cw_bat->sleep_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_SLEEP);

		allow_capacity = cw_bat->capacity - allow_change;
		cw_capacity = (allow_capacity >= cw_capacity) ? allow_capacity : cw_capacity;
		pr_debug("report GGIC POR happened");
		reg_val[0] = MODE_NORMAL;
		ret = cw_i2c_write(cw_bat->client, REG_MODE, reg_val, 1);
		if (ret < 0)
			return ret;
		pr_debug("report battery capacity jump 0 ");
	}

#if 1
	if ((cw_bat->charger_mode > 0) && (cw_capacity == 0)) {
		charge_time = new_sleep_time + new_run_time - cw_bat->sleep_time_charge_start - cw_bat->run_time_charge_start;
		if ((charge_time > BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE) && (if_quickstart == 0)) {
			cw_quickstart(cw_bat);
			reset_val = MODE_SLEEP;
			ret = cw_i2c_write(cw_bat->client, REG_MODE, &reset_val, 1);
			if (ret < 0)
				return ret;
			reset_val = MODE_NORMAL;
			msleep(10);
			ret = cw_i2c_write(cw_bat->client, REG_MODE, &reset_val, 1);
			if (ret < 0)
				return ret;
			pr_debug("report battery capacity error");
			ret = cw_update_config_info(cw_bat);
			if (ret)
				return ret;
			pr_debug("report battery capacity still 0 if in changing");
			if_quickstart = 1;
		}
	} else if ((if_quickstart == 1) && (cw_bat->charger_mode == 0)) {
		if_quickstart = 0;
	}

#endif

#ifdef SYSTEM_SHUTDOWN_VOLTAGE
	if ((cw_bat->charger_mode == 0) && (cw_capacity <= 20) && (cw_bat->voltage <= SYSTEM_SHUTDOWN_VOLTAGE)) {
		if (if_quickstart == 10) {

			allow_change = ((new_run_time - cw_bat->run_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_RUN);
			allow_change += ((new_sleep_time - cw_bat->sleep_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_SLEEP);

			allow_capacity = cw_bat->capacity - allow_change;
			cw_capacity = (allow_capacity >= 0) ? allow_capacity : 0;

			if (cw_capacity < 1) {
				cw_quickstart(cw_bat);
				if_quickstart = 12;
				cw_capacity = 0;
			}
		} else if (if_quickstart <= 10)
			if_quickstart = if_quickstart+2;
		pr_debug("the cw201x voltage is less than SYSTEM_SHUTDOWN_VOLTAGE !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
	} else if ((cw_bat->charger_mode > 0) && (if_quickstart <= 12)) {
		if_quickstart = 0;
	}
#endif
	return cw_capacity;
}

static int cw_get_vol(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val[2];
	u16 value16, value16_1, value16_2, value16_3;
	int voltage;

	ret = cw_i2c_read(cw_bat->client, REG_VCELL, reg_val, 2);
	if (ret < 0)
		return ret;
	value16 = (reg_val[0] * 256) + reg_val[1];

	ret = cw_i2c_read(cw_bat->client, REG_VCELL, reg_val, 2);
	if (ret < 0)
		return ret;
	value16_1 = (reg_val[0] << 8) + reg_val[1];

	ret = cw_i2c_read(cw_bat->client, REG_VCELL, reg_val, 2);
	if (ret < 0)
		return ret;
	value16_2 = (reg_val[0] << 8) + reg_val[1];


	if (value16 > value16_1) {
		value16_3 = value16;
		value16 = value16_1;
		value16_1 = value16_3;
	}

	if (value16_1 > value16_2) {
		value16_3 = value16_1;
		value16_1 = value16_2;
		value16_2 = value16_3;
	}

	if (value16 > value16_1) {
		value16_3 = value16;
		value16 = value16_1;
		value16_1 = value16_3;
	}

	voltage = value16_1 * 312 / 1024;
	voltage = voltage * 1000;

	pr_debug("get cw_voltage : cw_voltage = %d\n", voltage);

	return voltage;
}

#ifdef BAT_LOW_INTERRUPT
static int cw_get_alt(struct cw_battery *cw_bat)
{
	int ret = 0;
	u8 reg_val;
	u8 value8 = 0;
	int alrt;

	ret = cw_i2c_read(cw_bat->client, REG_RRT_ALERT, &reg_val, 1);
	if (ret < 0)
		return ret;
	value8 = reg_val;
	alrt = value8 >> 7;


	value8 = value8&0x7f;
	reg_val = value8;
	ret = cw_i2c_write(cw_bat->client, REG_RRT_ALERT, &reg_val, 1);
	if (ret < 0) {
		dev_err(&cw_bat->client->dev, "Error clear ALRT\n");
		return ret;
	}
	return alrt;
}
#endif

static int cw_get_time_to_empty(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val;
	u16 value16;

	ret = cw_i2c_read(cw_bat->client, REG_RRT_ALERT, &reg_val, 1);
	if (ret < 0)
		return ret;

	value16 = reg_val;

	ret = cw_i2c_read(cw_bat->client, REG_RRT_ALERT + 1, &reg_val, 1);
	if (ret < 0)
		return ret;

	value16 = ((value16 << 8) + reg_val) & 0x1fff;
	return value16;
}

static void rk_bat_update_capacity(struct cw_battery *cw_bat)
{


	cw_capacity = cw_get_capacity(cw_bat);
	if ((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat->capacity != cw_capacity)) {
		cw_bat->capacity = cw_capacity;
		cw_bat->bat_change = 1;
		cw_update_time_member_capacity_change(cw_bat);

		if (cw_bat->capacity == 0)
			pr_debug("report battery capacity 0 and will shutdown if no changing");
	}
}


static void rk_bat_update_vol(struct cw_battery *cw_bat)
{
	int ret;
	ret = cw_get_vol(cw_bat);
	if ((ret >= 0) && (cw_bat->voltage != ret)) {
		cw_bat->voltage = ret;
		cw_bat->bat_change = 1;
	}
}



extern int power_supply_get_battery_charge_state(struct power_supply *psy);
static struct power_supply *charge_psy;
static u8 is_charger_plug;


static void rk_bat_update_status(struct cw_battery *cw_bat)
{
	int status;
	union power_supply_propval ret = {0,};




	if (!charge_psy) {
		charge_psy = power_supply_get_by_name("usb");
	} else{
		is_charger_plug = (u8)power_supply_get_battery_charge_state(charge_psy);
	}

	pr_debug("Chaman for test is_charger_plug %d\n", is_charger_plug);
	if (is_charger_plug == 0)
		cw_bat->charger_mode =  POWER_SUPPLY_TYPE_UNKNOWN;
	else
		cw_bat->charger_mode = USB_CHARGER_MODE;

	if (cw_bat->batt_psy == NULL)
		cw_bat->batt_psy = power_supply_get_by_name("battery");
	if (cw_bat->batt_psy) {
		/* if battery has been registered, use the status property */
		cw_bat->batt_psy->get_property(cw_bat->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
		status = ret.intval;
	} else{
		/* Default to false if the battery power supply is not registered. */
		pr_debug("battery power supply is not registered\n");
		status = POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (cw_bat->status != status) {
		cw_bat->status = status;
		cw_bat->bat_change = 1;
	}
}

static void rk_bat_update_time_to_empty(struct cw_battery *cw_bat)
{
	int ret;
	ret = cw_get_time_to_empty(cw_bat);
	if ((ret >= 0) && (cw_bat->time_to_empty != ret)) {
		cw_bat->time_to_empty = ret;
		cw_bat->bat_change = 1;
	}

}

static void cw_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;


	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	rk_bat_update_status(cw_bat);
	rk_bat_update_capacity(cw_bat);
	rk_bat_update_vol(cw_bat);
	rk_bat_update_time_to_empty(cw_bat);

	if (cw_bat->bat_change) {
		power_supply_changed(&cw_bat->rk_bat);
		cw_bat->bat_change = 0;
	}

	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10000));

	pr_debug("cw_bat->bat_change = %d, cw_bat->time_to_empty = %d, cw_bat->capacity = %d, cw_bat->voltage = %d\n", \
						cw_bat->bat_change, cw_bat->time_to_empty, cw_bat->capacity, cw_bat->voltage);
}

static int rk_battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;
	struct cw_battery *cw_bat;

	cw_bat = container_of(psy, struct cw_battery, rk_bat);
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = cw_bat->capacity;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = cw_bat->voltage <= 0 ? 0 : 1;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = cw_bat->voltage;
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = cw_bat->time_to_empty;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;

	default:
		break;
	}
	return ret;
}

static enum power_supply_property rk_battery_properties[] = {
	POWER_SUPPLY_PROP_CAPACITY,


	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

#ifdef BAT_LOW_INTERRUPT

#define WAKE_LOCK_TIMEOUT       (10 * HZ)
static struct wake_lock bat_low_wakelock;

static void bat_low_detect_do_wakeup(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, bat_low_wakeup_work);
	pr_debug("func: %s-------\n", __func__);
	cw_get_alt(cw_bat);

}

static irqreturn_t bat_low_detect_irq_handler(int irq, void *dev_id)
{
	struct cw_battery *cw_bat = dev_id;

	wake_lock_timeout(&bat_low_wakelock, WAKE_LOCK_TIMEOUT);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->bat_low_wakeup_work, msecs_to_jiffies(20));
	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	pr_debug("%s\n", __func__);
	cancel_delayed_work(&cw_bat->battery_delay_work);

		pr_debug("cw_bat->capacity:%d\n", cw_bat->capacity);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	pr_debug("%s\n", __func__);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1));
		pr_debug("cw_bat->capacity:%d\n", cw_bat->capacity);
		return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
		.suspend = cw_bat_suspend,
		.resume = cw_bat_resume,

};
#endif

#ifdef CONFIG_OF
static int cw_bat_parse_dt(struct device *dev, struct cw_bat_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->bat_low_pin = of_get_named_gpio_flags(np,
			"cw2015,irq-gpio", 0, &pdata->irq_flags);

	return 0;
}
#else
static int cw_bat_parse_dt(struct device *dev, struct cw_bat_platform_data *pdata)
{
	return 0;
}
#endif
#ifdef BAT_LOW_INTERRUPT
#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"
#define CW_I2C_VTG_MIN_UV	1800000
#define CW_I2C_VTG_MAX_UV	1800000
#define CW_VIO_LOAD_MAX_UA	10000
static int cw_bat_regulator_configure(struct cw_battery *cw_bat, bool on)
{
	int retval;

	if (on == false)
		goto hw_shutdown;

	cw_bat->vcc_i2c = regulator_get(&cw_bat->client->dev,
					"vcc_i2c");
	if (IS_ERR(cw_bat->vcc_i2c)) {
		dev_err(&cw_bat->client->dev,
				"%s: Failed to get i2c regulator\n",
				__func__);
		retval = PTR_ERR(cw_bat->vcc_i2c);
		goto hw_shutdown;
	}

	if (regulator_count_voltages(cw_bat->vcc_i2c) > 0) {
		retval = regulator_set_voltage(cw_bat->vcc_i2c,
			CW_I2C_VTG_MIN_UV, CW_I2C_VTG_MAX_UV);
		if (retval) {
			dev_err(&cw_bat->client->dev,
				"%s reg set i2c vtg failed retval =%d\n", __func__,
				retval);
		goto err_set_vtg_i2c;
		}
	}
	return 0;

err_set_vtg_i2c:
	regulator_put(cw_bat->vcc_i2c);

hw_shutdown:
	if (regulator_count_voltages(cw_bat->vcc_i2c) > 0)
		regulator_set_voltage(cw_bat->vcc_i2c, 0,
				CW_I2C_VTG_MAX_UV);
	regulator_put(cw_bat->vcc_i2c);

	return 0;
};

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int cw_bat_power_on(struct cw_battery *cw_bat,
					bool on) {
	int retval;

	if (on == false)
		goto power_off;

	retval = reg_set_optimum_mode_check(cw_bat->vcc_i2c, CW_VIO_LOAD_MAX_UA);
	if (retval < 0) {
		dev_err(&cw_bat->client->dev,
			"%s Regulator vcc_i2c set_opt failed rc=%d\n", __func__,
			retval);
		goto power_off;
	}

	retval = regulator_enable(cw_bat->vcc_i2c);
	if (retval) {
		dev_err(&cw_bat->client->dev,
			"%s Regulator vcc_i2c enable failed rc=%d\n", __func__,
			retval);
		goto error_reg_en_vcc_i2c;
	}

	msleep(200);
	return 0;

error_reg_en_vcc_i2c:
	reg_set_optimum_mode_check(cw_bat->vcc_i2c, 0);
	return retval;

power_off:
	reg_set_optimum_mode_check(cw_bat->vcc_i2c, 0);
	regulator_disable(cw_bat->vcc_i2c);

	msleep(100);
	return 0;
}

static int cw_bat_pinctrl_init(struct cw_battery *cw_bat)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	cw_bat->ts_pinctrl = devm_pinctrl_get(&(cw_bat->client->dev));
	if (IS_ERR_OR_NULL(cw_bat->ts_pinctrl)) {
		retval = PTR_ERR(cw_bat->ts_pinctrl);
		pr_debug(
			"%s Target does not use pinctrl  %d\n", __func__, retval);
		goto err_pinctrl_get;
	}

	cw_bat->pinctrl_state_active
		= pinctrl_lookup_state(cw_bat->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(cw_bat->pinctrl_state_active)) {
		retval = PTR_ERR(cw_bat->pinctrl_state_active);
		dev_err(&cw_bat->client->dev,
			"%s Can not lookup %s pinstate %d\n",
			__func__, PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	cw_bat->pinctrl_state_suspend
		= pinctrl_lookup_state(cw_bat->ts_pinctrl,
				PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(cw_bat->pinctrl_state_suspend)) {
		retval = PTR_ERR(cw_bat->pinctrl_state_suspend);
		dev_err(&cw_bat->client->dev,
			"%s Can not lookup %s pinstate %d\n",
			__func__, PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(cw_bat->ts_pinctrl);
err_pinctrl_get:
	cw_bat->ts_pinctrl = NULL;
	return retval;
}


static int cw_bat_pinctrl_select(struct cw_battery *cw_bat, bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? cw_bat->pinctrl_state_active
		: cw_bat->pinctrl_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(cw_bat->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(&cw_bat->client->dev,
				"%s can not set %s pins\n", __func__,
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else
		dev_err(&cw_bat->client->dev,
			"%s not a valid '%s' pinstate\n", __func__,
				on ? "pmx_ts_active" : "pmx_ts_suspend");

	return 0;
}


static int cw_bat_gpio_configure(struct cw_battery *cw_bat, bool on)
{
	int retval = 0;

	if (on) {
		if (gpio_is_valid(cw_bat->plat_data->bat_low_pin)) {
			/* configure  irq gpio */
			retval = gpio_request(cw_bat->plat_data->bat_low_pin,
				"rmi4_irq_gpio");
			if (retval) {
				dev_err(&cw_bat->client->dev,
					"%s unable to request gpio [%d]\n", __func__,
					cw_bat->plat_data->bat_low_pin);
				goto err_irq_gpio_req;
			}
			retval = gpio_direction_input(cw_bat->plat_data->bat_low_pin);
			if (retval) {
				dev_err(&cw_bat->client->dev,
					"%s unable to set direction for gpio " \
					"[%d]\n", __func__, cw_bat->plat_data->bat_low_pin);
				goto err_irq_gpio_dir;
			}
		} else {
			dev_err(&cw_bat->client->dev,
				"%s irq gpio not provided\n", __func__);
			goto err_irq_gpio_req;
		}

		return 0;
	} else {
		return 0;
	}

err_irq_gpio_dir:
	if (gpio_is_valid(cw_bat->plat_data->bat_low_pin))
		gpio_free(cw_bat->plat_data->bat_low_pin);
err_irq_gpio_req:
	return retval;
}
#endif

extern int battery_type_id ;
static int cw_bat_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cw_bat_platform_data *pdata = client->dev.platform_data;
	struct cw_battery *cw_bat;
	int ret;
	int loop = 0;

	pr_debug("\ncw2015/cw2013 driver v1.2 probe start, battery_type_id is %d\n", battery_type_id);

	cw_bat = kzalloc(sizeof(struct cw_battery), GFP_KERNEL);
	if (!cw_bat) {
		dev_err(&cw_bat->client->dev, "fail to allocate memory\n");
		return -ENOMEM;
	}
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
		sizeof(struct cw_bat_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev,
					"GTP Failed to allocate memory for pdata\n");
			return -ENOMEM;
		}
		ret = cw_bat_parse_dt(&client->dev, pdata);
		if (ret)
			return ret;
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}


	else if (battery_type_id == 1) {
		pdata->cw_bat_config_info  = config_info_feimaotui;
	} else if (battery_type_id == 2) {
		pdata->cw_bat_config_info  = config_info_xinwangda;
	} else {
		pdata->cw_bat_config_info  = config_info;
	}




	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	cw_bat->client = client;
	i2c_set_clientdata(client, cw_bat);

	cw_bat->plat_data = pdata;
	ret = cw_check_ic(cw_bat);

	while ((loop++ < 5) && (ret != 0)) {
		pr_debug(" check ret is %d, loop is %d \n" , ret, loop);
		ret = cw_check_ic(cw_bat);
	}


	if (ret != 0) {
		pr_debug(" wc_check_ic fail ,return  ENODEV \n");
		return -ENODEV;
	}

	ret = cw_init(cw_bat);
	while ((loop++ < 2000) && (ret != 0)) {
		ret = cw_init(cw_bat);
	}

	if (ret) {
			return ret;
	}

	cw_bat->rk_bat.name = "rk-bat";
	cw_bat->rk_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	cw_bat->rk_bat.properties = rk_battery_properties;
	cw_bat->rk_bat.num_properties = ARRAY_SIZE(rk_battery_properties);
	cw_bat->rk_bat.get_property = rk_battery_get_property;
	ret = power_supply_register(&client->dev, &cw_bat->rk_bat);
	if (ret < 0) {
		dev_err(&cw_bat->client->dev, "power supply register rk_bat error\n");
		pr_debug("rk_bat_register_fail\n");
		goto rk_bat_register_fail;
	}

	cw_bat->charger_mode = 0;
	cw_bat->capacity = 0;
	cw_bat->voltage = 0;
	cw_bat->status = 0;
	cw_bat->time_to_empty = 0;
	cw_bat->bat_change = 0;

	cw_update_time_member_capacity_change(cw_bat);
	cw_update_time_member_charge_start(cw_bat);

	cw_bat->battery_workqueue = create_singlethread_workqueue("rk_battery");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);

	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10));

#ifdef BAT_LOW_INTERRUPT
	ret = cw_bat_regulator_configure(cw_bat, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s Failed to configure regulators\n", __func__);
		goto err_reg_configure;
	}

	ret = cw_bat_power_on(cw_bat, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s Failed to power on\n", __func__);
		goto err_power_device;

	}

	ret = cw_bat_pinctrl_init(cw_bat);
	if (!ret && cw_bat->ts_pinctrl) {
		ret = pinctrl_select_state(cw_bat->ts_pinctrl,
					cw_bat->pinctrl_state_active);
		if (ret < 0)
			goto err_pinctrl_select;
	}

	ret = cw_bat_gpio_configure(cw_bat, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s Failed to configure gpios\n", __func__);
		goto err_gpio_config;
	}

	INIT_DELAYED_WORK(&cw_bat->bat_low_wakeup_work, bat_low_detect_do_wakeup);
	wake_lock_init(&bat_low_wakelock, WAKE_LOCK_SUSPEND, "bat_low_detect");
	cw_bat->client->irq = gpio_to_irq(pdata->bat_low_pin);
	ret = request_threaded_irq(client->irq, NULL,
			  bat_low_detect_irq_handler, pdata->irq_flags,
			  "bat_low_detect", cw_bat);
	if (ret) {
		dev_err(&client->dev, "request irq failed\n");
		gpio_free(cw_bat->plat_data->bat_low_pin);
	}
	/*Chaman add for charger detect*/
	charge_psy = power_supply_get_by_name("usb");

	err_gpio_config:
	if (cw_bat->ts_pinctrl) {
		ret = cw_bat_pinctrl_select(cw_bat, false);
		if (ret < 0)
			pr_err("Cannot get idle pinctrl state\n");
	}
	err_pinctrl_select:
	if (cw_bat->ts_pinctrl) {
	pinctrl_put(cw_bat->ts_pinctrl);
	}
	err_power_device:
	cw_bat_power_on(cw_bat, false);
	err_reg_configure:
	cw_bat_regulator_configure(cw_bat, false);
#endif

	pr_debug("\ncw2015/cw2013 driver v1.2 probe sucess\n");
	return 0;

rk_bat_register_fail:
	pr_debug("cw2015/cw2013 driver v1.2 probe error!!!!\n");
	return ret;
}

static int cw_bat_remove(struct i2c_client *client)
{
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	pr_debug("%s\n", __func__);
	cancel_delayed_work(&cw_bat->battery_delay_work);
	return 0;
}

static const struct i2c_device_id cw_id[] = {
	{ "cw201x", 0 },
};
MODULE_DEVICE_TABLE(i2c, cw_id);

static struct of_device_id cw2015_match_table[] = {
	{ .compatible = "cellwise,cw2015", },
	{ },
};
static struct i2c_driver cw_bat_driver = {
			.driver         = {
			.name   = "cw201x",

#ifdef CONFIG_PM
			.pm = &cw_bat_pm_ops,
#endif
			.of_match_table = cw2015_match_table,
	},
		.probe          = cw_bat_probe,
		.remove         = cw_bat_remove,
		.id_table   = cw_id,
};

static int __init cw_bat_init(void)
{
	return i2c_add_driver(&cw_bat_driver);
}

static void __exit cw_bat_exit(void)
{
	i2c_del_driver(&cw_bat_driver);
}

late_initcall(cw_bat_init);
module_exit(cw_bat_exit);

MODULE_AUTHOR("ben<ben.chen@cellwise-semi.com>");
MODULE_DESCRIPTION("cw2015/cw2013 battery driver");
MODULE_LICENSE("GPL");
