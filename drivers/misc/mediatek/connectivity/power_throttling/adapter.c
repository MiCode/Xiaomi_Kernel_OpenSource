// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif
#include "conn_power_throttling.h"
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif

/* termal related macro */
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
//#define CONN_PWR_LOW_BATTERY_ENABLE
#endif
#define CONN_PWR_INVALID_TEMP (-888)
#define CONN_PWR_MAX_TEMP_HIGH  110
#if IS_ENABLED(CONFIG_CONN_PWR_DEBUG)
#define CONN_PWR_MAX_TEMP_LOW    0
#else
#define CONN_PWR_MAX_TEMP_LOW    60
#endif

/* device node related macro */
#define CONN_PWR_DEV_NUM 1
#define CONN_PWR_DRVIER_NAME "conn_pwr_drv"
#define CONN_PWR_DEVICE_NAME "conn_pwr_dev"
#define CONN_PWR_DEV_MAJOR 155
#define CONN_PWR_DEV_IOC_MAGIC 0xc2
#define CONN_PWR_IOCTL_SET_MAX_TEMP  _IOW(CONN_PWR_DEV_IOC_MAGIC, 0, int)
#define CONN_PWR_IOCTL_GET_DRV_LEVEL _IOR(CONN_PWR_DEV_IOC_MAGIC, 1, int)
#define CONN_PWR_IOCTL_GET_PLAT_LEVEL _IOR(CONN_PWR_DEV_IOC_MAGIC, 2, int)
#define CONN_PWR_SWITCH_LEVEL_MIN_SEC 30
#define CONN_PWR_SWITCH_LEVEL_GPS_MIN_SEC 5
#define CONN_CUSTOMER_SET_LEVEL_SUCCESS 0
#define CONN_CUSTOMER_SET_LEVEL_FAILED -1

static struct conn_pwr_plat_info g_plat_info;
static CONN_PWR_EVENT_CB g_event_cb_tbl[CONN_PWR_DRV_MAX];
static int g_drv_status_tbl[CONN_PWR_DRV_MAX];
#ifdef CONN_PWR_LOW_BATTERY_ENABLE
static int g_low_battery_level = LOW_BATTERY_LEVEL_0;
#else
static int g_low_battery_level;
#endif
static int g_max_temp = CONN_PWR_MAX_TEMP_HIGH;
static int g_connsys_temp = CONN_PWR_INVALID_TEMP;
static unsigned int g_customer_level;
static unsigned long long g_radio_last_updated_time[CONN_PWR_DRV_MAX];

/* device node related */
static int gConnPwrMajor = CONN_PWR_DEV_MAJOR;
static struct class *pConnPwrClass;
static struct device *pConnPwrDev;
static struct cdev gConnPwrdev;
#if IS_ENABLED(CONFIG_CONN_PWR_DEBUG)
extern ssize_t conn_pwr_dev_write(struct file *filp, const char __user *buffer, size_t count,
					loff_t *f_pos);
#endif
static long conn_pwr_dev_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#if IS_ENABLED(CONFIG_COMPAT)
static long conn_pwr_dev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif
const struct file_operations gConnPwrDevFops = {
#if IS_ENABLED(CONFIG_CONN_PWR_DEBUG)
	.write = conn_pwr_dev_write,
#endif
	.unlocked_ioctl = conn_pwr_dev_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = conn_pwr_dev_compat_ioctl,
#endif
};

void conn_pwr_get_local_time(unsigned long long *sec, unsigned long *usec)
{
	if (sec != NULL && usec != NULL) {
		*sec = local_clock();
		*usec = do_div(*sec, 1000000000)/1000;
	} else
		pr_info("The input parameters error when get local time\n");
}

int conn_pwr_enable(int enable)
{
	conn_pwr_core_enable(enable);
	return 0;
}
EXPORT_SYMBOL(conn_pwr_enable);

int conn_pwr_send_msg(enum conn_pwr_drv_type drv, enum conn_pwr_msg_type msg, void *data)
{
	struct conn_pwr_update_info info = {0};

	if (drv < 0 || drv >= CONN_PWR_DRV_MAX || msg < 0 || msg > CONN_PWR_MSG_MAX) {
		pr_info("%s, invalid parameter. drv (%d), msg(%d)", __func__, drv, msg);
		return -1;
	}

	if (msg == CONN_PWR_MSG_TEMP_TOO_HIGH) {
		if (data != NULL) {
			g_connsys_temp = *((int *)data);
			pr_info("%s drv:%d, msg: %d, temp: %d\n", __func__, drv, msg,
				*((int *)data));
		}
		info.reason = CONN_PWR_ARB_TEMP_CHECK;
		info.drv = drv;
		conn_pwr_arbitrate(&info);
	} else if (msg == CONN_PWR_MSG_TEMP_RECOVERY) {
		if (data != NULL) {
			g_connsys_temp = *((int *)data);
			pr_info("%s drv:%d, msg: %d, temp: %d\n", __func__, drv, msg,
				*((int *)data));
		}
		info.reason = CONN_PWR_ARB_TEMP_CHECK;
		info.drv = drv;
		conn_pwr_arbitrate(&info);
	} else if (msg == CONN_PWR_MSG_GET_TEMP && data != NULL) {
		struct conn_pwr_event_max_temp *d = (struct conn_pwr_event_max_temp *)data;

		conn_pwr_get_thermal(d);
		pr_info("%s drv:%d, msg: %d, max: %d, recovery: %d\n", __func__, drv, msg,
			d->max_temp, d->recovery_temp);
	}

	if (data == NULL)
		pr_info("%s drv:%d, msg: %d\n", __func__, drv, msg);
	return 0;
}
EXPORT_SYMBOL(conn_pwr_send_msg);

int conn_pwr_set_max_temp(unsigned long arg)
{
	struct conn_pwr_update_info info;

	if (g_max_temp == arg || arg > CONN_PWR_MAX_TEMP_HIGH || arg < CONN_PWR_MAX_TEMP_LOW) {
		pr_info("%s, max temp is not updated. old(%d), new(%lu)", __func__,
			g_max_temp, arg);
		return 0;
	}

	pr_info("%s, max temp is adjusted to %lu from %d\n", __func__, arg, g_max_temp);
	g_max_temp = arg;

	info.reason = CONN_PWR_ARB_THERMAL;
	conn_pwr_arbitrate(&info);

	return 0;
}

int conn_pwr_set_battery_level(int level)
{
	struct conn_pwr_update_info info;

	pr_info("%s level = %d\n", __func__, level);
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	if (level < LOW_BATTERY_LEVEL_0 || level > LOW_BATTERY_LEVEL_2) {
		pr_info("invalid level %d\n", level);
		return -1;
	}
#endif
	g_low_battery_level = level;
	info.reason = CONN_PWR_ARB_LOW_BATTERY;
	conn_pwr_arbitrate(&info);
	return 0;
}

static long conn_pwr_dev_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	pr_info("[%s] cmd (%d),arg (%ld)\n", __func__, cmd, arg);

	switch (cmd) {
	case CONN_PWR_IOCTL_SET_MAX_TEMP:
		ret = conn_pwr_set_max_temp(arg);
		break;
	case CONN_PWR_IOCTL_GET_DRV_LEVEL:
		conn_pwr_get_drv_level(arg, (enum conn_pwr_low_battery_level *)&ret);
		break;
	case CONN_PWR_IOCTL_GET_PLAT_LEVEL:
		conn_pwr_get_platform_level(arg, (int *)&ret);
		break;
	default:
		break;
	}
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long conn_pwr_dev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	pr_info("[%s] cmd (%d)\n", __func__, cmd);
	return conn_pwr_dev_unlocked_ioctl(filp, cmd, arg);
}
#endif

#ifdef CONN_PWR_LOW_BATTERY_ENABLE
static void conn_pwr_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	conn_pwr_set_battery_level(level);
}
#endif

int conn_pwr_get_plat_level(enum conn_pwr_plat_type type, int *data)
{
	if (data == NULL) {
		pr_info("%s: data is NULL.\n", __func__);
		return -1;
	}

	if (type == CONN_PWR_PLAT_LOW_BATTERY)
		*data = g_low_battery_level;
	else if (type == CONN_PWR_PLAT_THERMAL)
		*data = g_max_temp;
	else if (type == CONN_PWR_PLAT_CUSTOMER)
		*data = g_customer_level;
	else {
		pr_info("type %d is out of range.\n", type);
		return -2;
	}
	pr_info("%s ,type = %d, ret = %d\n", __func__, type, *data);

	return 0;
}

int conn_pwr_set_customer_level(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level level)
{
	struct conn_pwr_update_info info = {0};
	int i;
	int updated = 0;
	unsigned long long sec;
	unsigned long usec;
	int ret = CONN_CUSTOMER_SET_LEVEL_SUCCESS;
	unsigned int inactive_time = 0;

	if (type < CONN_PRW_DRV_ALL || type >= CONN_PWR_DRV_MAX || level < CONN_PWR_THR_LV_0 ||
		level >= CONN_PWR_LOW_BATTERY_MAX) {
		pr_info("%s, invalid parameter, type = %d, level = %d\n", __func__, type, level);
		return CONN_CUSTOMER_SET_LEVEL_FAILED;
	}

	conn_pwr_get_local_time(&sec, &usec);

	if (type == CONN_PRW_DRV_ALL) {
		for (i = 0; i < CONN_PWR_DRV_MAX; i++) {
			if (i == CONN_PWR_DRV_GPS)
				inactive_time = CONN_PWR_SWITCH_LEVEL_GPS_MIN_SEC;
			else
				inactive_time = CONN_PWR_SWITCH_LEVEL_MIN_SEC;

			if (sec > (g_radio_last_updated_time[i] + inactive_time) ||
				g_radio_last_updated_time[i] > sec) {
				updated = 1;
			} else {
				updated = 0;
				break;
			}
		}

		if (updated) {
			CONN_PWR_SET_CUSTOMER_POWER_LEVEL(g_customer_level, CONN_PWR_DRV_BT, level);
			CONN_PWR_SET_CUSTOMER_POWER_LEVEL(g_customer_level, CONN_PWR_DRV_FM, level);
			CONN_PWR_SET_CUSTOMER_POWER_LEVEL(g_customer_level, CONN_PWR_DRV_GPS,
							  level);
			CONN_PWR_SET_CUSTOMER_POWER_LEVEL(g_customer_level, CONN_PWR_DRV_WIFI,
							  level);
			g_radio_last_updated_time[CONN_PWR_DRV_BT] = sec;
			g_radio_last_updated_time[CONN_PWR_DRV_FM] = sec;
			g_radio_last_updated_time[CONN_PWR_DRV_GPS] = sec;
			g_radio_last_updated_time[CONN_PWR_DRV_WIFI] = sec;
		}
	} else {
		if (type == CONN_PWR_DRV_GPS)
			inactive_time = CONN_PWR_SWITCH_LEVEL_GPS_MIN_SEC;
		else
			inactive_time = CONN_PWR_SWITCH_LEVEL_MIN_SEC;

		if (sec > (g_radio_last_updated_time[type] + inactive_time) ||
				g_radio_last_updated_time[type] > sec) {
			g_radio_last_updated_time[type] = sec;
			updated = 1;
		}

		if (updated)
			CONN_PWR_SET_CUSTOMER_POWER_LEVEL(g_customer_level, type, level);
	}

	if (updated) {
		info.reason = CONN_PWR_ARB_CUSTOMER;
		info.drv = type;
		conn_pwr_arbitrate(&info);
	}

	pr_info("%s, type = %d, level = %d, customer_level = %d, updated = %d\n", __func__,
		type, level, g_customer_level, updated);

	if (updated == 0) {
		pr_info("%s, Set Level failed within %d, %llu (%llu, %llu, %llu, %llu)\n", __func__,
			CONN_PWR_SWITCH_LEVEL_MIN_SEC, sec,
			g_radio_last_updated_time[CONN_PWR_DRV_BT],
			g_radio_last_updated_time[CONN_PWR_DRV_FM],
			g_radio_last_updated_time[CONN_PWR_DRV_GPS],
			g_radio_last_updated_time[CONN_PWR_DRV_WIFI]);
		ret = CONN_CUSTOMER_SET_LEVEL_FAILED;
	}

	return ret;
}
EXPORT_SYMBOL(conn_pwr_set_customer_level);

int conn_pwr_get_chip_id(void)
{
	return g_plat_info.chip_id;
}

int conn_pwr_get_adie_id(void)
{
	return g_plat_info.adie_id;
}

int conn_pwr_get_temp(int *temp, int cached)
{
	int ret = 0;

	if ((g_connsys_temp == CONN_PWR_INVALID_TEMP || cached == 0) && g_plat_info.get_temp) {
		ret = (*g_plat_info.get_temp)(temp);
		if (ret == 0)
			g_connsys_temp = *temp;
	} else if (cached > 0 && g_connsys_temp != CONN_PWR_INVALID_TEMP) {
		*temp = g_connsys_temp;
	} else {
		ret = -1;
		pr_info("%s, get_temp is NULL/n", __func__);
	}
	pr_info("%s, ret = %d, temp = %d, cached = %d\n", __func__, ret, *temp, cached);

	return ret;
}

int conn_pwr_notify_event(enum conn_pwr_drv_type drv, enum conn_pwr_event_type event, void *data)
{
	int ret;
	struct conn_pwr_event_max_temp *d;

	if (drv < 0 || drv >= CONN_PWR_DRV_MAX || event < 0 || event >= CONN_PWR_EVENT_MAX) {
		pr_info("drv %d or event %d is out of range.\n", drv, event);
		return -1;
	}

	if (g_event_cb_tbl[drv] == NULL) {
		pr_info("event cb is not registered.\n", drv);
		return -2;
	}

	ret = (*g_event_cb_tbl[drv])(event, data);
	if (event == CONN_PWR_EVENT_LEVEL)
		pr_info("%s, drv = %d, level = %d, ret = %d\n", __func__, drv, *((int *)data), ret);
	else if (event == CONN_PWR_EVENT_MAX_TEMP && data != NULL) {
		d = (struct conn_pwr_event_max_temp *)data;
		pr_info("%s, drv = %d, max_t = %d, rcv_t = %d, ret = %d\n", __func__,
			drv, d->max_temp, d->recovery_temp, ret);
	} else {
		pr_info("invalid. event = %d, data = %d\n", event, drv);
		return -3;
	}
	return ret;
}

int conn_pwr_get_drv_status(enum conn_pwr_drv_type type)
{
	if (type < 0 || type >= CONN_PWR_DRV_MAX) {
		pr_info("type %d is out of range.\n", type);
		return -1;
	}

	return g_drv_status_tbl[type];
}

static int conn_pwr_set_drv_status(enum conn_pwr_drv_type type, enum conn_pwr_drv_status status)
{
	struct conn_pwr_update_info info;

	pr_info("%s, type = %d, status = %x\n", __func__, type, status);

	if (type < 0 || type >= CONN_PWR_DRV_MAX) {
		pr_info("type %d is out of range.\n", type);
		return -1;
	}

	if (status != CONN_PWR_DRV_STATUS_OFF && status != CONN_PWR_DRV_STATUS_ON) {
		pr_info("status %d is invalid.\n", status);
		return -2;
	}

	if (g_drv_status_tbl[type] == status) {
		pr_info("status is not changed.\n");
		return 0;
	}

	g_drv_status_tbl[type] = status;
	info.reason = CONN_PWR_ARB_SUBSYS_ON_OFF;
	info.drv = type;
	info.status = status;

	conn_pwr_arbitrate(&info);

	return 0;
}

int conn_pwr_drv_pre_on(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level *level)
{
	int ret;

	ret = conn_pwr_set_drv_status(type, CONN_PWR_DRV_STATUS_ON);
	if (ret == 0)
		ret = conn_pwr_get_drv_level(type, level);
	pr_info("%s, ret = %d, type = %d, level = %d\n", __func__, ret, type, *level);

	return ret;
}
EXPORT_SYMBOL(conn_pwr_drv_pre_on);

int conn_pwr_drv_post_off(enum conn_pwr_drv_type type)
{
	pr_info("%s type = %d", __func__, type);
	return conn_pwr_set_drv_status(type, CONN_PWR_DRV_STATUS_OFF);
}
EXPORT_SYMBOL(conn_pwr_drv_post_off);

int conn_pwr_register_event_cb(enum conn_pwr_drv_type type,
				CONN_PWR_EVENT_CB cb)
{
	pr_info("%s, type = %d, cb = %p\n", __func__, type, cb);

	if (type < 0 || type >= CONN_PWR_DRV_MAX) {
		pr_info("type %d is out of range.\n", type);
		return -1;
	}
	g_event_cb_tbl[type] = cb;

	return 0;
}
EXPORT_SYMBOL(conn_pwr_register_event_cb);

static int conn_pwr_dev_init(void)
{
	dev_t dev_id = MKDEV(gConnPwrMajor, 0);
	int ret = 0;

	ret = register_chrdev_region(dev_id, CONN_PWR_DEV_NUM,
						CONN_PWR_DRVIER_NAME);
	if (ret) {
		pr_info("fail to register chrdev.(%d)\n", ret);
		return -1;
	}

	cdev_init(&gConnPwrdev, &gConnPwrDevFops);
	gConnPwrdev.owner = THIS_MODULE;

	ret = cdev_add(&gConnPwrdev, dev_id, CONN_PWR_DEV_NUM);
	if (ret) {
		pr_info("cdev_add() fails (%d)\n", ret);
		goto err1;
	}

	pConnPwrClass = class_create(THIS_MODULE, CONN_PWR_DEVICE_NAME);
	if (IS_ERR(pConnPwrClass)) {
		pr_info("class create fail, error code(%ld)\n",
						PTR_ERR(pConnPwrClass));
		goto err2;
	}

	pConnPwrDev = device_create(pConnPwrClass, NULL, dev_id,
						NULL, CONN_PWR_DEVICE_NAME);
	if (IS_ERR(pConnPwrDev)) {
		pr_info("device create fail, error code(%ld)\n",
						PTR_ERR(pConnPwrDev));
		goto err3;
	}

	return 0;
err3:

	pr_info("[%s] err3", __func__);
	if (pConnPwrClass) {
		class_destroy(pConnPwrClass);
		pConnPwrClass = NULL;
	}
err2:
	pr_info("[%s] err2", __func__);
	cdev_del(&gConnPwrdev);

err1:
	pr_info("[%s] err1", __func__);
	unregister_chrdev_region(dev_id, CONN_PWR_DEV_NUM);

	return -1;
}

static int conn_pwr_dev_deinit(void)
{
	dev_t dev_id = MKDEV(gConnPwrMajor, 0);

	if (pConnPwrDev) {
		device_destroy(pConnPwrClass, dev_id);
		pConnPwrDev = NULL;
	}

	if (pConnPwrClass) {
		class_destroy(pConnPwrClass);
		pConnPwrClass = NULL;
	}

	cdev_del(&gConnPwrdev);
	unregister_chrdev_region(dev_id, CONN_PWR_DEV_NUM);

	return 0;
}

int conn_pwr_init(struct conn_pwr_plat_info *data)
{
	pr_info("%s\n", __func__);
	if (data == NULL) {
		pr_info("data is NULL\n");
		return -1;
	}

	if (data->chip_id == 0 || data->get_temp == NULL) {
		pr_info("%s, init data is invalid: chip_id = %d, get_temp = %p\n",
			__func__, data->chip_id, data->get_temp);
		return -2;
	}

	memcpy(&g_plat_info, data, sizeof(struct conn_pwr_plat_info));
	memset(g_event_cb_tbl, 0, sizeof(g_event_cb_tbl));
	memset(g_drv_status_tbl, 0, sizeof(g_drv_status_tbl));

	conn_pwr_core_init();

#ifdef CONN_PWR_LOW_BATTERY_ENABLE
	register_low_battery_notify(&conn_pwr_low_battery_cb, LOW_BATTERY_PRIO_WIFI);
#endif
	conn_pwr_dev_init();

	return 0;
}
EXPORT_SYMBOL(conn_pwr_init);

int conn_pwr_deinit(void)
{
	pr_info("%s", __func__);
	conn_pwr_dev_deinit();
	return 0;
}
EXPORT_SYMBOL(conn_pwr_deinit);

int conn_pwr_resume(void)
{
	conn_pwr_core_resume();
	return 0;
}
EXPORT_SYMBOL(conn_pwr_resume);

int conn_pwr_suspend(void)
{
	conn_pwr_core_suspend();
	return 0;
}
EXPORT_SYMBOL(conn_pwr_suspend);
