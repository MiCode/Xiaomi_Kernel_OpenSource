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
#include "conn_power_throttling.h"

/* unit-test related */
#if IS_ENABLED(CONFIG_CONN_PWR_DEBUG)
static int conn_pwr_ut_send_msg(int par1, int par2, int par3);
static int conn_pwr_ut_thermal(int par1, int par2, int par3);
static int conn_pwr_ut_start(int par1, int par2, int par3);
static int conn_pwr_ut_stop(int par1, int par2, int par3);
static int conn_pwr_ut_notify(int par1, int par2, int par3);
static int conn_pwr_ut_get_temp(int par1, int par2, int par3);
static int conn_pwr_ut_get_plat_level(int par1, int par2, int par3);
static int conn_pwr_ut_set_customer_level(int par1, int par2, int par3);
static int conn_pwr_ut_set_battery_level(int par1, int par2, int par3);
static int conn_pwr_ut_set_max_temp(int par1, int par2, int par3);
static int conn_pwr_ut_update_thermal_status(int par1, int par2, int par3);
static int conn_pwr_ut_enable_throttling(int par1, int par2, int par3);
static int conn_pwr_ut_event_cb_bt(enum conn_pwr_event_type event, void *data);
static int conn_pwr_ut_event_cb_fm(enum conn_pwr_event_type event, void *data);
static int conn_pwr_ut_event_cb_gps(enum conn_pwr_event_type event, void *data);
static int conn_pwr_ut_event_cb_wifi(enum conn_pwr_event_type event, void *data);

typedef int(*CONN_PWR_TEST_FUNC) (int par1, int par2, int par3);

static const CONN_PWR_TEST_FUNC conn_pwr_test_func[] = {
	[0x0] = conn_pwr_ut_send_msg,
	[0x1] = conn_pwr_ut_thermal,
	[0x2] = conn_pwr_ut_start,
	[0x3] = conn_pwr_ut_stop,
	[0x4] = conn_pwr_ut_notify,
	[0x5] = conn_pwr_ut_get_temp,
	[0x6] = conn_pwr_ut_get_plat_level,
	[0x7] = conn_pwr_ut_set_customer_level,
	[0x8] = conn_pwr_ut_set_max_temp,
	[0x9] = conn_pwr_ut_set_battery_level,
	[0xA] = conn_pwr_ut_update_thermal_status,
	[0x10] = conn_pwr_ut_enable_throttling,
};

static const CONN_PWR_EVENT_CB ut_cb_tbl[] = {
	conn_pwr_ut_event_cb_bt,
	conn_pwr_ut_event_cb_fm,
	conn_pwr_ut_event_cb_gps,
	conn_pwr_ut_event_cb_wifi
};

static int ut_battery_level;
static int ut_max_temp = 100;
static int ut_recovery_temp = 60;

ssize_t conn_pwr_dev_write(struct file *filp, const char __user *buffer, size_t count,
					loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char *pBuf;
	char *pDelimiter = " \t";
	int x = 0, y = 0, z = 0;
	char *pToken = NULL;
	long res = 0;
	static int test_enabled = -1;

	pr_info("write parameter len = %d\n\r", (int) len);
	if (len >= sizeof(buf)) {
		pr_info("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		kstrtol(pToken, 16, &res);
		x = (int)res;
	} else {
		x = 0;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		kstrtol(pToken, 16, &res);
		y = (int)res;
		pr_info("y = 0x%08x\n\r", y);
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		kstrtol(pToken, 16, &res);
		z = (int)res;
	}

	pr_info("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	/* For eng and userdebug load, have to enable debug by
	 * writing 0xDB9DB9 to * "/proc/driver/wmt_dbg" to avoid
	 * some malicious use
	 */
	if (x == 0xDB9DB9) {
		test_enabled = 1;
		return len;
	}

	if (test_enabled < 0)
		return len;

	if (ARRAY_SIZE(conn_pwr_test_func) > x &&
		conn_pwr_test_func[x] != NULL)
		(*conn_pwr_test_func[x]) (x, y, z);
	else
		pr_info("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}

static int conn_pwr_ut_send_msg(int par1, int par2, int par3)
{
	conn_pwr_send_msg(par2, par3, NULL);
	return 0;
}

static int conn_pwr_ut_thermal(int par1, int par2, int par3)
{
	conn_pwr_set_max_temp(par2);
	return 0;
}

static void conn_pwr_ut_event_cb(const char *s, enum conn_pwr_event_type event, void *data)
{
	if (event == CONN_PWR_EVENT_LEVEL)
		pr_info("%s level = %d\n", s, *((int *)data));
	else if (event == CONN_PWR_EVENT_MAX_TEMP) {
		struct conn_pwr_event_max_temp *d = (struct conn_pwr_event_max_temp *)data;

		pr_info("%s m_temp = %d, r_temp = %d\n", s, d->max_temp, d->recovery_temp);
	}
}

static int conn_pwr_ut_event_cb_bt(enum conn_pwr_event_type event, void *data)
{
	conn_pwr_ut_event_cb(__func__, event, data);
	return 0;
}

static int conn_pwr_ut_event_cb_fm(enum conn_pwr_event_type event, void *data)
{
	conn_pwr_ut_event_cb(__func__, event, data);
	return 0;
}

static int conn_pwr_ut_event_cb_gps(enum conn_pwr_event_type event, void *data)
{
	conn_pwr_ut_event_cb(__func__, event, data);
	return 0;
}

static int conn_pwr_ut_event_cb_wifi(enum conn_pwr_event_type event, void *data)
{
	conn_pwr_ut_event_cb(__func__, event, data);
	return 0;
}

static int conn_pwr_ut_start(int par1, int par2, int par3)
{
	enum conn_pwr_low_battery_level level = CONN_PWR_THR_LV_0;
	int ret;

	pr_info("%s", __func__);
	if (par2 < 0 || par2 >= CONN_PWR_DRV_MAX) {
		pr_info("type %d is invalid", par2);
		return 0;
	}

	ret = conn_pwr_register_event_cb(par2, ut_cb_tbl[par2]);
	pr_info("%d register event cb, ret = %d", par2, ret);

	ret = conn_pwr_drv_pre_on(par2, &level);
	pr_info("type(%d) on, ret = %d, level = %d", par2, ret, level);

	return 0;
}

static int conn_pwr_ut_stop(int par1, int par2, int par3)
{
	int ret;

	ret = conn_pwr_drv_post_off(par2);
	pr_info("type(%d) off, ret = %d", par2, ret);

	return 0;
}

static int conn_pwr_ut_notify(int par1, int par2, int par3)
{
	struct conn_pwr_event_max_temp data;

	if (par3 == CONN_PWR_EVENT_LEVEL)
		conn_pwr_notify_event(par2, par3, &ut_battery_level);
	else if (par3 == CONN_PWR_EVENT_MAX_TEMP) {
		data.max_temp = ut_max_temp;
		data.recovery_temp = ut_recovery_temp;
		conn_pwr_notify_event(par2, par3, &data);
	}
	return 0;
}

static int conn_pwr_ut_get_temp(int par1, int par2, int par3)
{
	int temp = 0, ret;

	ret = conn_pwr_get_temp(&temp, par2);
	pr_info("%s ret= %d, temp = %d", __func__, ret, temp);

	return 0;
}

static int conn_pwr_ut_get_plat_level(int par1, int par2, int par3)
{
	int level = 0, ret;

	ret = conn_pwr_get_plat_level(par2, &level);
	pr_info("%s ret= %d, type = %d, level = %d", __func__, ret, par2, level);

	return 0;
}

static int conn_pwr_ut_set_customer_level(int par1, int par2, int par3)
{
	conn_pwr_set_customer_level(par2, par3);
	pr_info("%s type = %d, level = %d\n", __func__, par2, par3);

	return 0;
}

static int conn_pwr_ut_set_battery_level(int par1, int par2, int par3)
{
	ut_battery_level = par2;
	conn_pwr_set_battery_level(par2);
	return 0;
}

/* this is used to set temp to notfiy driver later */
static int conn_pwr_ut_set_max_temp(int par1, int par2, int par3)
{
	ut_max_temp = par2;
	ut_recovery_temp = par3;
	pr_info("%s max = %d, recovery = %d\n", __func__, par2, par3);
	return 0;
}

static int conn_pwr_ut_update_thermal_status(int par1, int par2, int par3)
{
	void *value;

	value = &par3;
	conn_pwr_send_msg(CONN_PWR_DRV_WIFI, par2, value);
	return 0;
}

static int conn_pwr_ut_enable_throttling(int par1, int par2, int par3)
{
	conn_pwr_enable(par2);
	return 0;
}

#endif
