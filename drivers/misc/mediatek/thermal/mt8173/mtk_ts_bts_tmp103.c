/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mt_thermal.h"
#include <linux/types.h>
#include "inc/tmp103_cooler.h"
#include "inc/thermal_framework.h"
#include "mt_cpufreq.h"

#define NUM_SENSORS 3
#define DMF 1000

#define UNLIMITED_POWER	100001

static LIST_HEAD(thermal_sensor_list);

struct vs_config_t {
	unsigned long temp;
	int offset;
	int alpha;
	int weight;
};
static struct vs_config_t vs_data[NUM_SENSORS] = {
	{0, 0.0 * DMF, 1.0 * DMF, 0.333 * DMF},
	{0, 0.0 * DMF, 1.0 * DMF, 0.333 * DMF},
	{0, 0.0 * DMF, 1.0 * DMF, 0.333 * DMF},
};
static int sk[NUM_SENSORS] = { 0, 0, 0 };

static char *vs_conf_names[] = {
	"offsets",
	"alphas",
	"weights",
};

static struct tmp103_cooler_pdev tmp103_cooler_devs[] = {
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-0"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-1"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-2"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-3"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-4"},
	{TMP103_COOLER_CPU, MAX_CPU_POWER, MAX_CPU_POWER, "tmp103-cpu-cool-5"},
};

struct tmp103_platform_cooler_ctx {
	struct list_head list;
};

struct tmp103_cooler_ctx {
	enum tmp103_cooler_type ctype;
	struct list_head entry;
	struct thermal_cooling_device *cdev;
	unsigned int state;
	int action;
	int clear;
};
static struct tmp103_platform_cooler_ctx *ctx;

static struct thermal_cooling_device_ops cooling_ops;

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };
static struct thermal_zone_device *thz_dev;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = { 0 };
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1, use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5000;
static int polling_factor2 = 10000;

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);


int bts_cur_temp = 0;

#define mtkts_AP_TEMP_CRIT 60000	/* 60.000 degree Celsius */

static int get_hw_AP_temp(void)
{
	int i = 0;
	int tsenv = 0;
	struct thermal_dev *tdev;

	list_for_each_entry(tdev, &thermal_sensor_list, node) {
		vs_data[i].temp = tdev->dev_ops->report_temp(tdev);
		if (sk[i] == 0)
			sk[i] = vs_data[i].temp - vs_data[i].offset;
		else {
			sk[i] = vs_data[i].alpha * (vs_data[i].temp - vs_data[i].offset) +
			    (DMF - vs_data[i].alpha) * sk[i];
			sk[i] /= DMF;
		}
		i++;
	}
	for (i = 0; i < NUM_SENSORS; i++)
		tsenv += (vs_data[i].weight * sk[i]);

	return tsenv / DMF;
}

static DEFINE_MUTEX(AP_lock);
int mtkts_AP_get_hw_temp(void)
{
	int t_ret = 0;

	mutex_lock(&AP_lock);

	t_ret = get_hw_AP_temp();

	mutex_unlock(&AP_lock);

	bts_cur_temp = t_ret;

	if (t_ret > 60000)	/* abnormal high temp */
		pr_err("[Power/AP_Thermal] T_AP=%d\n", t_ret);

	pr_debug("[mtkts_AP_get_hw_temp] T_AP, %d\n", t_ret);
	return t_ret;
}

static int mtkts_AP_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtkts_AP_get_hw_temp();

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtkts_AP_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		pr_debug("[mtkts_AP_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		pr_debug("[mtkts_AP_bind] error binding cooling dev\n");
		return -EINVAL;
	}

	return 0;
}

static int mtkts_AP_unbind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		pr_debug("[mtkts_AP_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		pr_debug("[mtkts_AP_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	}

	return 0;
}

static int mtkts_AP_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_AP_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_AP_get_trip_type(struct thermal_zone_device *thermal, int trip,
				  enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_AP_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				  unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_AP_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtkts_AP_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_AP_dev_ops = {
	.bind = mtkts_AP_bind,
	.unbind = mtkts_AP_unbind,
	.get_temp = mtkts_AP_get_temp,
	.get_mode = mtkts_AP_get_mode,
	.set_mode = mtkts_AP_set_mode,
	.get_trip_type = mtkts_AP_get_trip_type,
	.get_trip_temp = mtkts_AP_get_trip_temp,
	.get_crit_temp = mtkts_AP_get_crit_temp,
};

static int mtkts_AP_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[mtkts_AP_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3], trip_temp[4]);
	seq_printf(m, "trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8], trip_temp[9]);
	seq_printf(m, "g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);
	seq_printf(m, "g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n", g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
	seq_printf(m, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
	seq_printf(m, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}


int mtkts_AP_register_thermal(void)
{
	pr_debug("[mtkts_AP_register_thermal]\n");

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsAP", num_trip, NULL,
						   &mtkts_AP_dev_ops, 0, 0, 0, interval * 1000);

	return 0;
}

void mtkts_AP_unregister_thermal(void)
{
	pr_debug("[mtkts_AP_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static ssize_t mtkts_AP_write(struct file *file, const char __user *buffer, size_t count,
			      loff_t *data)
{
	int len = 0, time_msec = 0;
	int trip[10] = { 0 };
	int t_type[10] = { 0 };
	int i;
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
	char desc[512];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf
	    (desc,
	     "%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d",
	     &num_trip, &trip[0], &t_type[0], bind0, &trip[1], &t_type[1], bind1, &trip[2],
	     &t_type[2], bind2, &trip[3], &t_type[3], bind3, &trip[4], &t_type[4], bind4, &trip[5],
	     &t_type[5], bind5, &trip[6], &t_type[6], bind6, &trip[7], &t_type[7], bind7, &trip[8],
	     &t_type[8], bind8, &trip[9], &t_type[9], bind9, &time_msec) == 32) {

		if (num_trip < 0 || num_trip > 10) {
			pr_debug("[mtkts_AP_write] bad argument\n");
			return -EINVAL;
		}

		pr_debug("[mtkts_AP_write] mtkts_AP_unregister_thermal\n");
		mtkts_AP_unregister_thermal();

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = bind0[i];
			g_bind1[i] = bind1[i];
			g_bind2[i] = bind2[i];
			g_bind3[i] = bind3[i];
			g_bind4[i] = bind4[i];
			g_bind5[i] = bind5[i];
			g_bind6[i] = bind6[i];
			g_bind7[i] = bind7[i];
			g_bind8[i] = bind8[i];
			g_bind9[i] = bind9[i];
		}

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec / 1000;

		pr_debug("[mtkts_AP_write] mtkts_AP_register_thermal\n");

		mtkts_AP_register_thermal();

		/* AP_write_flag=1; */
		return count;
	}

	return -EINVAL;
}

static int mtkts_AP_param_read(struct seq_file *m, void *v)
{
	int i = 0;
	int d, d0, d1, d2;

	seq_puts(m, "offsets ");
	for (i = 0; i < NUM_SENSORS; i++) {
		d = (int)abs(vs_data[i].offset % DMF);
		d0 = (d < 100) ? 0 : d / 100;
		d = (d % 100);
		d1 = (d < 10) ? 0 : d / 10;
		d2 = (d % 10);
		seq_printf(m, "%d.%d%d%d ", vs_data[i].offset / DMF, d0, d1, d2);
	}
	seq_puts(m, "\nalphas  ");
	for (i = 0; i < NUM_SENSORS; i++) {
		d = (int)abs(vs_data[i].alpha % DMF);
		d0 = (d < 100) ? 0 : d / 100;
		d = (d % 100);
		d1 = (d < 10) ? 0 : d / 10;
		d2 = (d % 10);
		seq_printf(m, "%d.%d%d%d ", vs_data[i].alpha / DMF, d0, d1, d2);
	}
	seq_puts(m, "\nweights ");
	for (i = 0; i < NUM_SENSORS; i++) {
		d = (int)abs(vs_data[i].weight % DMF);
		d0 = (d < 100) ? 0 : d / 100;
		d = (d % 100);
		d1 = (d < 10) ? 0 : d / 10;
		d2 = (d % 10);
		seq_printf(m, "%d.%d%d%d ", vs_data[i].weight / DMF, d0, d1, d2);
	}

	return 0;
}

static int cmd_check_parsing(const char *buf, int *idx, int *x, int *y, int *z)
{
	int ret = 0;
	int index = -1;
	const char dot[] = ".";
	const char space[] = " ";
	char *token, *str;
	int r, d, i, j, k, fact;
	int conf[3];
	char dec[2];
	size_t len;
	int t[3] = { 0, 0, 0 };

	dec[1] = '\0';
	str = kstrdup(buf, GFP_KERNEL);
	token = strsep(&str, space);
	if (token == NULL) {
		pr_err("%s: Error NULL token => %s\n", __func__, token);
		return -1;
	}
	for (i = 0; i < NUM_SENSORS; i++) {
		if (!strcmp(token, vs_conf_names[i]))
			index = i;

	}
	if ((index < 0) || (index > 2))
		pr_err("%s: Error Invalid Index %d config %s\n", __func__, index, token);
	for (i = 0; i < NUM_SENSORS; i++) {
		d = 0;
		r = 0;
		token = strsep(&str, dot);
		if (token) {
			ret = kstrtoint(token, 10, &r);
			if (ret < 0) {
				pr_err("%s: Error Invalid Real Val%d Ret %d token => %s str =>%s\n",
				       __func__, i, ret, token, str);
				return ret;
			}
		} else
			return -1;
		token = strsep(&str, space);
		if (token) {
			fact = 100;
			len = strlen(token);
			len = (len < 3) ? len : 3;
			for (k = 0; k < 3; k++)
				t[k] = 0;
			for (j = 0; j < len; j++) {
				if (token[j] == '\n')
					break;
				dec[0] = token[j];
				ret = kstrtoint(dec, 10, &t[j]);
				if (ret < 0) {
					pr_err("%s: Error Invalid Dec Val%d Ret%d len:%d\n",
					       __func__, j, ret, (int)len);
					pr_err("%s: Error Invalid Dec = %s token = %s str = %s\n",
					       __func__, dec, token, str);
					return ret;
				}
				d += t[j] * fact;
				fact /= 10;
			}
		} else
			return -1;
		if (d < 0)
			return -1;
		if (r < 0)
			conf[i] = r * DMF - d;
		else
			conf[i] = r * DMF + d;
	}

	*idx = index;
	*x = conf[0];
	*y = conf[1];
	*z = conf[2];

	pr_debug("%s: Wrote Index%d off%d alp=%d wei%d\n",
		 __func__, index, conf[0], conf[1], conf[2]);

	return ret;
}

static ssize_t mtkts_AP_param_write(struct file *file, const char __user *buffer, size_t count,
				    loff_t *data)
{
	int len = 0;
	char desc[512];
	int ret, index;
	int i = 0;
	int d[3];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	pr_debug("[mtkts_AP_write]\n");

	ret = cmd_check_parsing(desc, &index, &d[0], &d[1], &d[2]);
	if ((ret < 0) || (index < 0) || (index > 2))
		return -EINVAL;
	for (i = 0; i < NUM_SENSORS; i++) {
		if (index == 0)
			vs_data[i].offset = d[i];
		if (index == 1)
			vs_data[i].alpha = d[i];
		if (index == 2)
			vs_data[i].weight = d[i];
	}

	return count;
}

static int mtkts_AP_cooler_read(struct seq_file *m, void *v)
{
	struct tmp103_cooler_ctx *cctx;
	int len = 0;

	list_for_each_entry(cctx, &ctx->list, entry) {
		len += seq_printf(m + len, "%s action=%d clear=%d state=%d\n",
				  cctx->cdev->type, cctx->action, cctx->clear, cctx->state);
	}

	return len;
}

static ssize_t mtkts_AP_cooler_write(struct file *file,
				     const char __user *buffer, size_t count, loff_t *data)
{
	struct tmp103_cooler_ctx *cctx;
	char cdev[THERMAL_NAME_LENGTH];
	int action, clear;

	if (sscanf(buffer, "%19s %d %d", cdev, &action, &clear) != 3)
		return -EINVAL;

	list_for_each_entry(cctx, &ctx->list, entry) {
		if (strcmp(cdev, cctx->cdev->type))
			continue;

		cctx->action = action;
		cctx->clear = clear;

		return count;
	}

	return -EINVAL;
}

static int cooler_update(struct class *class, struct tmp103_cooler_ctx *cctx, unsigned long state)
{
	int curr_thermal_limit;
	int calc_value;

	cctx->state = state;
	curr_thermal_limit = mt_cpufreq_get_thermal_limited_power();

	/* 0 means unlimited power */
	if (curr_thermal_limit == 0)
		curr_thermal_limit = UNLIMITED_POWER;

	calc_value = state ? min(curr_thermal_limit, cctx->action) :
	    max(curr_thermal_limit, cctx->clear);

	mt_cpufreq_thermal_protect(calc_value, 1);
	return 0;
}

static int mtkts_AP_cooler_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_AP_cooler_read, NULL);
}

static const struct file_operations cooler_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_AP_cooler_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_AP_cooler_write,
	.release = single_release,
};

static int cooler_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int cooler_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct tmp103_cooler_ctx *cctx = cdev->devdata;

	if (!cctx) {
		pr_err("%s: NULL %s device data\n", __func__, cdev->type);
		return -EINVAL;
	}

	if (cctx)
		*state = cctx->state;

	return 0;
}

static int cooler_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct tmp103_cooler_ctx *cctx = cdev->devdata;

	pr_debug("[cooler_set_cur_state] %ld\n", state);

	if (!cctx) {
		pr_debug("%s: NULL %s device data\n", __func__, cdev->type);
		return -EINVAL;
	}

	if (!state && !cctx->state) {
		cctx->state = state;
		return 0;
	}
	cooler_update(cdev->device.class, cctx, state);

	return 0;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = cooler_get_max_state,
	.get_cur_state = cooler_get_cur_state,
	.set_cur_state = cooler_set_cur_state,
};

int mtkts_AP_register_dvfs_cooler(void)
{
	int i;
	struct tmp103_cooler_ctx *cctx;

	pr_debug("[mtkts_AP_register_dvfs_cooler]\n");

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -EINVAL;

	INIT_LIST_HEAD(&ctx->list);

	for (i = 0; i < ARRAY_SIZE(tmp103_cooler_devs); i++) {
		cctx = kzalloc(sizeof(*cctx), GFP_KERNEL);
		if (!cctx)
			continue;
		INIT_LIST_HEAD(&cctx->entry);
		cctx->ctype = tmp103_cooler_devs[i].ctype;
		cctx->action = tmp103_cooler_devs[i].action;
		cctx->clear = tmp103_cooler_devs[i].clear;
		cctx->state = 0;
		cctx->cdev = thermal_cooling_device_register(tmp103_cooler_devs[i].name,
							     cctx, &cooling_ops);
		if (!cctx->cdev) {
			kfree(cctx);
			continue;
		}

		list_add_tail(&cctx->entry, &ctx->list);
	}
	return 0;
}

int mtkts_AP_unregister_dvfs_cooler(void)
{
	struct tmp103_cooler_ctx *cctx;
	struct tmp103_cooler_ctx *cctx_tmp;

	list_for_each_entry_safe(cctx, cctx_tmp, &ctx->list, entry) {
		if (cctx->cdev)
			thermal_cooling_device_unregister(cctx->cdev);
		list_del_init(&cctx->entry);
		kfree(cctx);
	}

	kfree(ctx);
	return 0;
}

int thermal_sensor_register(struct thermal_dev *tdev)
{
	if (unlikely(IS_ERR_OR_NULL(tdev))) {
		pr_err("%s: NULL sensor thermal device\n", __func__);
		return -ENODEV;
	}
	if (!tdev->dev_ops->report_temp) {
		pr_err("%s: Error getting report_temp()\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&AP_lock);
	list_add_tail(&tdev->node, &thermal_sensor_list);
	mutex_unlock(&AP_lock);
	return 0;
}
EXPORT_SYMBOL(thermal_sensor_register);

static int mtkts_AP_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_AP_read, NULL);
}

static const struct file_operations mtkts_AP_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_AP_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_AP_write,
	.release = single_release,
};

static int mtkts_AP_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_AP_param_read, NULL);
}

static const struct file_operations mtkts_AP_param_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_AP_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_AP_param_write,
	.release = single_release,
};

static int __init mtkts_AP_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_AP_dir = NULL;

	pr_debug("[mtkts_AP_init]\n");

	err = mtkts_AP_register_dvfs_cooler();
	if (err)
		return err;

	err = mtkts_AP_register_thermal();
	if (err)
		return err;

	mtkts_AP_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_AP_dir) {
		pr_debug("[mtkts_AP_init]: mkdir /proc/driver/thermal failed\n");
	} else {

		entry =
		    proc_create("tzbts", S_IRUGO | S_IWUSR | S_IWGRP, mtkts_AP_dir, &mtkts_AP_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("tzbts_param", S_IRUGO | S_IWUSR | S_IWGRP, mtkts_AP_dir,
				&mtkts_AP_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return err;
}

static void __exit mtkts_AP_exit(void)
{
	pr_debug("[mtkts_AP_exit]\n");
	mtkts_AP_unregister_thermal();
	mtkts_AP_unregister_dvfs_cooler();
}
module_init(mtkts_AP_init);
module_exit(mtkts_AP_exit);
