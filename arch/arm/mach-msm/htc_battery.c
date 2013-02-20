/* arch/arm/mach-msm/htc_battery.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <asm/gpio.h>
#include <mach/msm_rpcrouter.h>
#include <mach/board.h>

static struct wake_lock vbus_wake_lock;

#define TRACE_BATT 0

#if TRACE_BATT
#define BATT(x...) printk(KERN_INFO "[BATT] " x)
#else
#define BATT(x...) do {} while (0)
#endif

/* rpc related */
#define APP_BATT_PDEV_NAME		"rs30100001"
#define APP_BATT_PROG			0x30100001
#define APP_BATT_VER			0
#define HTC_PROCEDURE_BATTERY_NULL	0
#define HTC_PROCEDURE_GET_BATT_LEVEL	1
#define HTC_PROCEDURE_GET_BATT_INFO	2
#define HTC_PROCEDURE_GET_CABLE_STATUS	3
#define HTC_PROCEDURE_SET_BATT_DELTA	4

/* module debugger */
#define HTC_BATTERY_DEBUG		1
#define BATTERY_PREVENTION		1

/* Enable this will shut down if no battery */
#define ENABLE_BATTERY_DETECTION	0

#define GPIO_BATTERY_DETECTION		21
#define GPIO_BATTERY_CHARGER_EN		128

/* Charge current selection */
#define GPIO_BATTERY_CHARGER_CURRENT	129

typedef enum {
	DISABLE = 0,
	ENABLE_SLOW_CHG,
	ENABLE_FAST_CHG
} batt_ctl_t;

/* This order is the same as htc_power_supplies[]
 * And it's also the same as htc_cable_status_update()
 */
typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC
} charger_type_t;

struct battery_info_reply {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	u32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	u32 batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
};

struct htc_battery_info {
	int present;
	unsigned long update_time;

	/* lock to protect the battery info */
	struct mutex lock;

	/* lock held while calling the arm9 to query the battery info */
	struct mutex rpc_lock;
	struct battery_info_reply rep;
};

static struct msm_rpc_endpoint *endpoint;

static struct htc_battery_info htc_batt_info;

static unsigned int cache_time = 1000;

static int htc_battery_initial = 0;

static enum power_supply_property htc_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property htc_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

/* HTC dedicated attributes */
static ssize_t htc_battery_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);

static int htc_power_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static int htc_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static struct power_supply htc_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = htc_battery_properties,
		.num_properties = ARRAY_SIZE(htc_battery_properties),
		.get_property = htc_battery_get_property,
	},
	{
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = htc_power_properties,
		.num_properties = ARRAY_SIZE(htc_power_properties),
		.get_property = htc_power_get_property,
	},
};


/* -------------------------------------------------------------------------- */

#if defined(CONFIG_DEBUG_FS)
int htc_battery_set_charging(batt_ctl_t ctl);
static int batt_debug_set(void *data, u64 val)
{
	return htc_battery_set_charging((batt_ctl_t) val);
}

static int batt_debug_get(void *data, u64 *val)
{
	return -ENOSYS;
}

DEFINE_SIMPLE_ATTRIBUTE(batt_debug_fops, batt_debug_get, batt_debug_set, "%llu\n");
static int __init batt_debug_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("htc_battery", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("charger_state", 0644, dent, NULL, &batt_debug_fops);

	return 0;
}

device_initcall(batt_debug_init);
#endif

static int init_batt_gpio(void)
{
	if (gpio_request(GPIO_BATTERY_DETECTION, "batt_detect") < 0)
		goto gpio_failed;
	if (gpio_request(GPIO_BATTERY_CHARGER_EN, "charger_en") < 0)
		goto gpio_failed;
	if (gpio_request(GPIO_BATTERY_CHARGER_CURRENT, "charge_current") < 0)
		goto gpio_failed;

	return 0;

gpio_failed:	
	return -EINVAL;
	
}

/* 
 *	battery_charging_ctrl - battery charing control.
 * 	@ctl:			battery control command
 *
 */
static int battery_charging_ctrl(batt_ctl_t ctl)
{
	int result = 0;

	switch (ctl) {
	case DISABLE:
		BATT("charger OFF\n");
		/* 0 for enable; 1 disable */
		result = gpio_direction_output(GPIO_BATTERY_CHARGER_EN, 1);
		break;
	case ENABLE_SLOW_CHG:
		BATT("charger ON (SLOW)\n");
		result = gpio_direction_output(GPIO_BATTERY_CHARGER_CURRENT, 0);
		result = gpio_direction_output(GPIO_BATTERY_CHARGER_EN, 0);
		break;
	case ENABLE_FAST_CHG:
		BATT("charger ON (FAST)\n");
		result = gpio_direction_output(GPIO_BATTERY_CHARGER_CURRENT, 1);
		result = gpio_direction_output(GPIO_BATTERY_CHARGER_EN, 0);
		break;
	default:
		printk(KERN_ERR "Not supported battery ctr called.!\n");
		result = -EINVAL;
		break;
	}
	
	return result;
}

int htc_battery_set_charging(batt_ctl_t ctl)
{
	int rc;
	
	if ((rc = battery_charging_ctrl(ctl)) < 0)
		goto result;
	
	if (!htc_battery_initial) {
		htc_batt_info.rep.charging_enabled = ctl & 0x3;
	} else {
		mutex_lock(&htc_batt_info.lock);
		htc_batt_info.rep.charging_enabled = ctl & 0x3;
		mutex_unlock(&htc_batt_info.lock);
	}
result:	
	return rc;
}

int htc_battery_status_update(u32 curr_level)
{
	int notify;
	if (!htc_battery_initial)
		return 0;

	mutex_lock(&htc_batt_info.lock);
	notify = (htc_batt_info.rep.level != curr_level);
	htc_batt_info.rep.level = curr_level;
	mutex_unlock(&htc_batt_info.lock);

	if (notify)
		power_supply_changed(&htc_power_supplies[CHARGER_BATTERY]);
	return 0;
}

int htc_cable_status_update(int status)
{
	int rc = 0;
	unsigned source;

	if (!htc_battery_initial)
		return 0;
	
	mutex_lock(&htc_batt_info.lock);
	switch(status) {
	case CHARGER_BATTERY:
		BATT("cable NOT PRESENT\n");
		htc_batt_info.rep.charging_source = CHARGER_BATTERY;
		break;
	case CHARGER_USB:
		BATT("cable USB\n");
		htc_batt_info.rep.charging_source = CHARGER_USB;
		break;
	case CHARGER_AC:
		BATT("cable AC\n");
		htc_batt_info.rep.charging_source = CHARGER_AC;
		break;
	default:
		printk(KERN_ERR "%s: Not supported cable status received!\n",
				__FUNCTION__);
		rc = -EINVAL;
	}
	source = htc_batt_info.rep.charging_source;
	mutex_unlock(&htc_batt_info.lock);

	msm_hsusb_set_vbus_state(source == CHARGER_USB);
	if (source == CHARGER_USB) {
		wake_lock(&vbus_wake_lock);
	} else {
		/* give userspace some time to see the uevent and update
		 * LED state or whatnot...
		 */
		wake_lock_timeout(&vbus_wake_lock, HZ / 2);
	}

	/* if the power source changes, all power supplies may change state */
	power_supply_changed(&htc_power_supplies[CHARGER_BATTERY]);
	power_supply_changed(&htc_power_supplies[CHARGER_USB]);
	power_supply_changed(&htc_power_supplies[CHARGER_AC]);

	return rc;
}

static int htc_get_batt_info(struct battery_info_reply *buffer)
{
	struct rpc_request_hdr req;
	
	struct htc_get_batt_info_rep {
		struct rpc_reply_hdr hdr;
		struct battery_info_reply info;
	} rep;
	
	int rc;

	if (buffer == NULL) 
		return -EINVAL;

	rc = msm_rpc_call_reply(endpoint, HTC_PROCEDURE_GET_BATT_INFO,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	if ( rc < 0 ) 
		return rc;
	
	mutex_lock(&htc_batt_info.lock);
	buffer->batt_id 		= be32_to_cpu(rep.info.batt_id);
	buffer->batt_vol 		= be32_to_cpu(rep.info.batt_vol);
	buffer->batt_temp 		= be32_to_cpu(rep.info.batt_temp);
	buffer->batt_current 		= be32_to_cpu(rep.info.batt_current);
	buffer->level 			= be32_to_cpu(rep.info.level);
	buffer->charging_source 	= be32_to_cpu(rep.info.charging_source);
	buffer->charging_enabled 	= be32_to_cpu(rep.info.charging_enabled);
	buffer->full_bat 		= be32_to_cpu(rep.info.full_bat);
	mutex_unlock(&htc_batt_info.lock);

	return 0;
}

#if 0
static int htc_get_cable_status(void)
{
	
	struct rpc_request_hdr req;
	
	struct htc_get_cable_status_rep {
		struct rpc_reply_hdr hdr;
		int status;
	} rep;

	int rc;

	rc = msm_rpc_call_reply(endpoint, HTC_PROCEDURE_GET_CABLE_STATUS,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	if (rc < 0) 
		return rc;

	return be32_to_cpu(rep.status);
}
#endif

/* -------------------------------------------------------------------------- */
static int htc_power_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	charger_type_t charger;
	
	mutex_lock(&htc_batt_info.lock);
	charger = htc_batt_info.rep.charging_source;
	mutex_unlock(&htc_batt_info.lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (charger ==  CHARGER_AC ? 1 : 0);
		else if (psy->type == POWER_SUPPLY_TYPE_USB)
			val->intval = (charger ==  CHARGER_USB ? 1 : 0);
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static int htc_battery_get_charging_status(void)
{
	u32 level;
	charger_type_t charger;	
	int ret;
	
	mutex_lock(&htc_batt_info.lock);
	charger = htc_batt_info.rep.charging_source;
	
	switch (charger) {
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
		level = htc_batt_info.rep.level;
		if (level == 100)
			ret = POWER_SUPPLY_STATUS_FULL;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}
	mutex_unlock(&htc_batt_info.lock);
	return ret;
}

static int htc_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = htc_battery_get_charging_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = htc_batt_info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mutex_lock(&htc_batt_info.lock);
		val->intval = htc_batt_info.rep.level;
		mutex_unlock(&htc_batt_info.lock);
		break;
	default:		
		return -EINVAL;
	}
	
	return 0;
}

#define HTC_BATTERY_ATTR(_name)							\
{										\
	.attr = { .name = #_name, .mode = S_IRUGO, .owner = THIS_MODULE },	\
	.show = htc_battery_show_property,					\
	.store = NULL,								\
}

static struct device_attribute htc_battery_attrs[] = {
	HTC_BATTERY_ATTR(batt_id),
	HTC_BATTERY_ATTR(batt_vol),
	HTC_BATTERY_ATTR(batt_temp),
	HTC_BATTERY_ATTR(batt_current),
	HTC_BATTERY_ATTR(charging_source),
	HTC_BATTERY_ATTR(charging_enabled),
	HTC_BATTERY_ATTR(full_bat),
};

enum {
	BATT_ID = 0,
	BATT_VOL,
	BATT_TEMP,
	BATT_CURRENT,
	CHARGING_SOURCE,
	CHARGING_ENABLED,
	FULL_BAT,
};

static int htc_rpc_set_delta(unsigned delta)
{
	struct set_batt_delta_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	req.data = cpu_to_be32(delta);
	return msm_rpc_call(endpoint, HTC_PROCEDURE_SET_BATT_DELTA,
			    &req, sizeof(req), 5 * HZ);
}


static ssize_t htc_battery_set_delta(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int rc;
	unsigned long delta = 0;
	
	delta = simple_strtoul(buf, NULL, 10);

	if (delta > 100)
		return -EINVAL;

	mutex_lock(&htc_batt_info.rpc_lock);
	rc = htc_rpc_set_delta(delta);
	mutex_unlock(&htc_batt_info.rpc_lock);
	if (rc < 0)
		return rc;
	return count;
}

static struct device_attribute htc_set_delta_attrs[] = {
	__ATTR(delta, S_IWUSR | S_IWGRP, NULL, htc_battery_set_delta),
};

static int htc_battery_create_attrs(struct device * dev)
{
	int i, j, rc;
	
	for (i = 0; i < ARRAY_SIZE(htc_battery_attrs); i++) {
		rc = device_create_file(dev, &htc_battery_attrs[i]);
		if (rc)
			goto htc_attrs_failed;
	}

	for (j = 0; j < ARRAY_SIZE(htc_set_delta_attrs); j++) {
		rc = device_create_file(dev, &htc_set_delta_attrs[j]);
		if (rc)
			goto htc_delta_attrs_failed;
	}
	
	goto succeed;
	
htc_attrs_failed:
	while (i--)
		device_remove_file(dev, &htc_battery_attrs[i]);
htc_delta_attrs_failed:
	while (j--)
		device_remove_file(dev, &htc_set_delta_attrs[i]);
succeed:	
	return rc;
}

static ssize_t htc_battery_show_property(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - htc_battery_attrs;
	
	/* rpc lock is used to prevent two threads from calling
	 * into the get info rpc at the same time
	 */

	mutex_lock(&htc_batt_info.rpc_lock);
	/* check cache time to decide if we need to update */
	if (htc_batt_info.update_time &&
            time_before(jiffies, htc_batt_info.update_time +
                                msecs_to_jiffies(cache_time)))
                goto dont_need_update;
	
	if (htc_get_batt_info(&htc_batt_info.rep) < 0)
		printk(KERN_ERR "%s: rpc failed!!!\n", __FUNCTION__);
	else
		htc_batt_info.update_time = jiffies;
dont_need_update:
	mutex_unlock(&htc_batt_info.rpc_lock);

	mutex_lock(&htc_batt_info.lock);
	switch (off) {
	case BATT_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_id);
		break;
	case BATT_VOL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_vol);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_temp);
		break;
	case BATT_CURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.batt_current);
		break;
	case CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.charging_source);
		break;
	case CHARGING_ENABLED:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.charging_enabled);
		break;		
	case FULL_BAT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       htc_batt_info.rep.full_bat);
		break;
	default:
		i = -EINVAL;
	}	
	mutex_unlock(&htc_batt_info.lock);
	
	return i;
}

static int htc_battery_probe(struct platform_device *pdev)
{
	int i, rc;

	if (pdev->id != (APP_BATT_VER & RPC_VERSION_MAJOR_MASK))
		return -EINVAL;

	/* init battery gpio */
	if ((rc = init_batt_gpio()) < 0) {
		printk(KERN_ERR "%s: init battery gpio failed!\n", __FUNCTION__);
		return rc;
	}

	/* init structure data member */
	htc_batt_info.update_time 	= jiffies;
	htc_batt_info.present 		= gpio_get_value(GPIO_BATTERY_DETECTION);
	
	/* init rpc */
	endpoint = msm_rpc_connect(APP_BATT_PROG, APP_BATT_VER, 0);
	if (IS_ERR(endpoint)) {
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));
		return rc;
	}

	/* init power supplier framework */
	for (i = 0; i < ARRAY_SIZE(htc_power_supplies); i++) {
		rc = power_supply_register(&pdev->dev, &htc_power_supplies[i]);
		if (rc)
			printk(KERN_ERR "Failed to register power supply (%d)\n", rc);	
	}

	/* create htc detail attributes */
	htc_battery_create_attrs(htc_power_supplies[CHARGER_BATTERY].dev);

	/* After battery driver gets initialized, send rpc request to inquiry
	 * the battery status in case of we lost some info
	 */
	htc_battery_initial = 1;

	mutex_lock(&htc_batt_info.rpc_lock);
	if (htc_get_batt_info(&htc_batt_info.rep) < 0)
		printk(KERN_ERR "%s: get info failed\n", __FUNCTION__);

	htc_cable_status_update(htc_batt_info.rep.charging_source);
	battery_charging_ctrl(htc_batt_info.rep.charging_enabled ?
			      ENABLE_SLOW_CHG : DISABLE);

	if (htc_rpc_set_delta(1) < 0)
		printk(KERN_ERR "%s: set delta failed\n", __FUNCTION__);
	htc_batt_info.update_time = jiffies;
	mutex_unlock(&htc_batt_info.rpc_lock);

	if (htc_batt_info.rep.charging_enabled == 0)
		battery_charging_ctrl(DISABLE);
	
	return 0;
}

static struct platform_driver htc_battery_driver = {
	.probe	= htc_battery_probe,
	.driver	= {
		.name	= APP_BATT_PDEV_NAME,
		.owner	= THIS_MODULE,
	},
};

/* batt_mtoa server definitions */
#define BATT_MTOA_PROG				0x30100000
#define BATT_MTOA_VERS				0
#define RPC_BATT_MTOA_NULL			0
#define RPC_BATT_MTOA_SET_CHARGING_PROC		1
#define RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC	2
#define RPC_BATT_MTOA_LEVEL_UPDATE_PROC		3

struct rpc_batt_mtoa_set_charging_args {
	int enable;
};

struct rpc_batt_mtoa_cable_status_update_args {
	int status;
};

struct rpc_dem_battery_update_args {
	uint32_t level;
};

static int handle_battery_call(struct msm_rpc_server *server,
			       struct rpc_request_hdr *req, unsigned len)
{	
	switch (req->procedure) {
	case RPC_BATT_MTOA_NULL:
		return 0;

	case RPC_BATT_MTOA_SET_CHARGING_PROC: {
		struct rpc_batt_mtoa_set_charging_args *args;
		args = (struct rpc_batt_mtoa_set_charging_args *)(req + 1);
		args->enable = be32_to_cpu(args->enable);
		BATT("set_charging: enable=%d\n",args->enable);
		htc_battery_set_charging(args->enable);
		return 0;
	}
	case RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC: {
		struct rpc_batt_mtoa_cable_status_update_args *args;
		args = (struct rpc_batt_mtoa_cable_status_update_args *)(req + 1);
		args->status = be32_to_cpu(args->status);
		BATT("cable_status_update: status=%d\n",args->status);
		htc_cable_status_update(args->status);
		return 0;
	}
	case RPC_BATT_MTOA_LEVEL_UPDATE_PROC: {
		struct rpc_dem_battery_update_args *args;
		args = (struct rpc_dem_battery_update_args *)(req + 1);
		args->level = be32_to_cpu(args->level);
		BATT("dem_battery_update: level=%d\n",args->level);
		htc_battery_status_update(args->level);
		return 0;
	}
	default:
		printk(KERN_ERR "%s: program 0x%08x:%d: unknown procedure %d\n",
		       __FUNCTION__, req->prog, req->vers, req->procedure);
		return -ENODEV;
	}
}

static struct msm_rpc_server battery_server = {
	.prog = BATT_MTOA_PROG,
	.vers = BATT_MTOA_VERS,
	.rpc_call = handle_battery_call,
};

static int __init htc_battery_init(void)
{
	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	mutex_init(&htc_batt_info.lock);
	mutex_init(&htc_batt_info.rpc_lock);
	msm_rpc_create_server(&battery_server);
	platform_driver_register(&htc_battery_driver);
	return 0;
}

module_init(htc_battery_init);
MODULE_DESCRIPTION("HTC Battery Driver");
MODULE_LICENSE("GPL");

