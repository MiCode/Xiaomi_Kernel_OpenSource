// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include "tzbatt_initcfg.h"
#include <linux/power_supply.h>


/* ************************************ */
/* Function prototype*/
/* ************************************ */
static void tsbattery_exit(void);

/* ************************************ */
/* Weak functions */
/* ************************************ */
int __attribute__ ((weak))
read_tbat_value(void)
{
	pr_notice("[Thermal] E_WF: %s doesn't exist\n", __func__);
	return 30;
}

signed int __attribute__ ((weak))
battery_get_bat_temperature(void)
{
	int i;

	for (i = 0; i < 5; i++)
		pr_notice("[Thermal] E_WF: %s doesn't exist\n", __func__);

	tsbattery_exit();
	return -127000;
}
/* ************************************ */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

#if TZBATT_SET_INIT_CFG == 1
static unsigned int interval = TZBATT_INITCFG_INTERVAL;
static int trip_temp[10] = {
	TZBATT_INITCFG_TRIP_0_TEMP,
	TZBATT_INITCFG_TRIP_1_TEMP,
	TZBATT_INITCFG_TRIP_2_TEMP,
	TZBATT_INITCFG_TRIP_3_TEMP,
	TZBATT_INITCFG_TRIP_4_TEMP,
	TZBATT_INITCFG_TRIP_5_TEMP,
	TZBATT_INITCFG_TRIP_6_TEMP,
	TZBATT_INITCFG_TRIP_7_TEMP,
	TZBATT_INITCFG_TRIP_8_TEMP,
	TZBATT_INITCFG_TRIP_9_TEMP };
#else
static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };
#endif
/* static unsigned int cl_dev_dis_charge_state = 0; */
static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;
/* static struct thermal_cooling_device *cl_dev_dis_charge; */
static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktsbattery_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#if TZBATT_SET_INIT_CFG == 1
static int num_trip = TZBATT_INITCFG_NUM_TRIPS;
static char g_bind0[20] = TZBATT_INITCFG_TRIP_0_COOLER;
static char g_bind1[20] = TZBATT_INITCFG_TRIP_1_COOLER;
static char g_bind2[20] = TZBATT_INITCFG_TRIP_2_COOLER;
static char g_bind3[20] = TZBATT_INITCFG_TRIP_3_COOLER;
static char g_bind4[20] = TZBATT_INITCFG_TRIP_4_COOLER;
static char g_bind5[20] = TZBATT_INITCFG_TRIP_5_COOLER;
static char g_bind6[20] = TZBATT_INITCFG_TRIP_6_COOLER;
static char g_bind7[20] = TZBATT_INITCFG_TRIP_7_COOLER;
static char g_bind8[20] = TZBATT_INITCFG_TRIP_8_COOLER;
static char g_bind9[20] = TZBATT_INITCFG_TRIP_9_COOLER;
#else
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
#endif

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *	use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5000;
static int polling_factor2 = 10000;

/* static int battery_write_flag=0; */

#define mtktsbattery_TEMP_CRIT 60000	/* 60.000 degree Celsius */

#define mtktsbattery_dprintk(fmt, args...)   \
do {                                    \
	if (mtktsbattery_debug_log) {                \
		pr_debug("[Thermal/TZ/BATTERY]" fmt, ##args); \
	}                                   \
} while (0)

#define mtktsbattery_printk(fmt, args...)   \
pr_debug("[Thermal/TZ/BATTERY]" fmt, ##args)
/*
 * kernel fopen/fclose
 */
/*
 *static mm_segment_t oldfs;
 *
 *static void my_close(int fd)
 *{
 *	set_fs(oldfs);
 *	sys_close(fd);
 *}
 *
 *static int my_open(char *fname, int flag)
 *{
 *	oldfs = get_fs();
 *   set_fs(KERNEL_DS);
 *    return sys_open(fname, flag, 0);
 *}
 */
static int get_hw_battery_temp(void)
{
	union power_supply_propval prop;
	struct power_supply *psy;
	int ret = 0;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -1270;
	ret = power_supply_get_property(psy,
		POWER_SUPPLY_PROP_TEMP, &prop);
		mtktsbattery_dprintk("%s %d\n", __func__, prop.intval);
	if (ret != 0)
		return -1270;

	return prop.intval;
}

static DEFINE_MUTEX(Battery_lock);
int ts_battery_at_boot_time = 1;
static int mtktsbattery_get_hw_temp(void)
{
	int t_ret = 0;
	static int battery[60] = { 0 };
	static int counter = 0, first_time;


	if (ts_battery_at_boot_time == 1) {
		ts_battery_at_boot_time = 0;
		mtktsbattery_dprintk(
			"[%s] at boot time, return 25000 as default\n",
			__func__);
		battery[counter] = 25000;
		counter++;
		return 25000;
	}

	mutex_lock(&Battery_lock);

	/* get HW battery temp (TSBATTERY) */
	/* cat /sys/class/power_supply/battery/batt_temp */
	t_ret = get_hw_battery_temp();
	t_ret = t_ret * 100;

	mutex_unlock(&Battery_lock);

	if (t_ret)

		mtktsbattery_dprintk(
				"[%s] counter=%d, first_time =%d\n", __func__,
				counter, first_time);

	mtktsbattery_dprintk("[%s] T_Battery, %d\n", __func__,
									t_ret);
	return t_ret;
}

static int mtktsbattery_get_temp(struct thermal_zone_device *thermal, int *t)
{

	*t = mtktsbattery_get_hw_temp();

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtktsbattery_bind(struct thermal_zone_device *thermal,
			     struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktsbattery_dprintk(
			"[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktsbattery_dprintk("[%s] binding OK, %d\n", __func__, table_val);
	return 0;
}

static int mtktsbattery_unbind(struct thermal_zone_device *thermal,
			       struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktsbattery_dprintk("[%s] %s\n", __func__, cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktsbattery_dprintk(
				"[%s] error unbinding cooling dev\n", __func__);

		return -EINVAL;
	}

	mtktsbattery_dprintk("[%s] unbinding OK\n", __func__);
	return 0;
}

static int mtktsbattery_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktsbattery_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktsbattery_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktsbattery_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktsbattery_get_crit_temp(struct thermal_zone_device *thermal,
				      int *temperature)
{
	*temperature = mtktsbattery_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktsbattery_dev_ops = {
	.bind = mtktsbattery_bind,
	.unbind = mtktsbattery_unbind,
	.get_temp = mtktsbattery_get_temp,
	.get_mode = mtktsbattery_get_mode,
	.set_mode = mtktsbattery_set_mode,
	.get_trip_type = mtktsbattery_get_trip_type,
	.get_trip_temp = mtktsbattery_get_trip_temp,
	.get_crit_temp = mtktsbattery_get_crit_temp,
};

/*
 *static int dis_charge_get_max_state(struct thermal_cooling_device *cdev,
 *				 int *state)
 *{
 *		*state = 1;
 *		return 0;
 *}
 *static int dis_charge_get_cur_state(struct thermal_cooling_device *cdev,
 *				 int *state)
 *{
 *		*state = cl_dev_dis_charge_state;
 *		return 0;
 *}
 *static int dis_charge_set_cur_state(struct thermal_cooling_device *cdev,
 *				 int state)
 *{
 *    cl_dev_dis_charge_state = state;
 *    if(cl_dev_dis_charge_state == 1) {
 *	mtktsbattery_dprintk("[dis_charge_set_cur_state] disable charging\n");
 *    }
 *    return 0;
 *}
 */

static int tsbat_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int tsbat_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int tsbat_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{

	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_debug("Power/battery_Thermal: reset, reset, reset!!!");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_debug("*****************************************");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		//BUG();
	}
	return 0;
}

/*
 *static struct thermal_cooling_device_ops
 * mtktsbattery_cooling_dis_charge_ops = {
 *	.get_max_state = dis_charge_get_max_state,
 *	.get_cur_state = dis_charge_get_cur_state,
 *	.set_cur_state = dis_charge_set_cur_state,
 *};
 */
static struct thermal_cooling_device_ops mtktsbattery_cooling_sysrst_ops = {
	.get_max_state = tsbat_sysrst_get_max_state,
	.get_cur_state = tsbat_sysrst_get_cur_state,
	.set_cur_state = tsbat_sysrst_set_cur_state,
};


static int mtktsbattery_read(struct seq_file *m, void *v)
/* static int mtktsbattery_read(
 * char *buf, char **start, off_t off, int count, int *eof, void *data)
 */
{

	seq_printf(m,
		"[mtktsbattery_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

	seq_printf(m,
		"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[4], trip_temp[5], trip_temp[6],
		trip_temp[7], trip_temp[8], trip_temp[9]);

	seq_printf(m,
		"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

	seq_printf(m,
		"g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
		g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

	seq_printf(m,
		"g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
		g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

	seq_printf(m,
		"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

	seq_printf(m,
		"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtktsbattery_register_thermal(void);
static void mtktsbattery_unregister_thermal(void);

static ssize_t mtktsbattery_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
/* static ssize_t mtktsbattery_write(
 * struct file *file, const char *buffer, int count, void *data)
 */
{
	int len = 0, i;
	struct mtktsbattery_data {
		int trip[10];
		int t_type[10];
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
	char desc[512];
	};

	struct mtktsbattery_data *ptr_mtktsbattery_data = kmalloc(
				sizeof(*ptr_mtktsbattery_data), GFP_KERNEL);

	if (ptr_mtktsbattery_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktsbattery_data->desc) - 1)) ?
			count : (sizeof(ptr_mtktsbattery_data->desc) - 1);

	if (copy_from_user(ptr_mtktsbattery_data->desc, buffer, len)) {
		kfree(ptr_mtktsbattery_data);
		return 0;
	}

	ptr_mtktsbattery_data->desc[len] = '\0';

	if (sscanf
	    (ptr_mtktsbattery_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktsbattery_data->trip[0],
		&ptr_mtktsbattery_data->t_type[0],
		ptr_mtktsbattery_data->bind0,
		&ptr_mtktsbattery_data->trip[1],
		&ptr_mtktsbattery_data->t_type[1],
		ptr_mtktsbattery_data->bind1,
		&ptr_mtktsbattery_data->trip[2],
		&ptr_mtktsbattery_data->t_type[2],
		ptr_mtktsbattery_data->bind2,
		&ptr_mtktsbattery_data->trip[3],
		&ptr_mtktsbattery_data->t_type[3],
		ptr_mtktsbattery_data->bind3,
		&ptr_mtktsbattery_data->trip[4],
		&ptr_mtktsbattery_data->t_type[4],
		ptr_mtktsbattery_data->bind4,
		&ptr_mtktsbattery_data->trip[5],
		&ptr_mtktsbattery_data->t_type[5],
		ptr_mtktsbattery_data->bind5,
		&ptr_mtktsbattery_data->trip[6],
		&ptr_mtktsbattery_data->t_type[6],
		ptr_mtktsbattery_data->bind6,
		&ptr_mtktsbattery_data->trip[7],
		&ptr_mtktsbattery_data->t_type[7],
		ptr_mtktsbattery_data->bind7,
		&ptr_mtktsbattery_data->trip[8],
		&ptr_mtktsbattery_data->t_type[8],
		ptr_mtktsbattery_data->bind8,
		&ptr_mtktsbattery_data->trip[9],
		&ptr_mtktsbattery_data->t_type[9],
		ptr_mtktsbattery_data->bind9,
		&ptr_mtktsbattery_data->time_msec) == 32) {

		down(&sem_mutex);
		mtktsbattery_dprintk(
				"[%s] mtktsbattery_unregister_thermal\n",
				__func__);

		mtktsbattery_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtktsbattery_write",
					"Bad argument");
			#endif
			mtktsbattery_dprintk(
					"[%s] bad argument\n", __func__);

			kfree(ptr_mtktsbattery_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktsbattery_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0] = g_bind7[0]
			= g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktsbattery_data->bind0[i];
			g_bind1[i] = ptr_mtktsbattery_data->bind1[i];
			g_bind2[i] = ptr_mtktsbattery_data->bind2[i];
			g_bind3[i] = ptr_mtktsbattery_data->bind3[i];
			g_bind4[i] = ptr_mtktsbattery_data->bind4[i];
			g_bind5[i] = ptr_mtktsbattery_data->bind5[i];
			g_bind6[i] = ptr_mtktsbattery_data->bind6[i];
			g_bind7[i] = ptr_mtktsbattery_data->bind7[i];
			g_bind8[i] = ptr_mtktsbattery_data->bind8[i];
			g_bind9[i] = ptr_mtktsbattery_data->bind9[i];
		}

		mtktsbattery_dprintk(
			"[%s] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1]);

		mtktsbattery_dprintk(
			"g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,",
			g_THERMAL_TRIP[2], g_THERMAL_TRIP[3],
			g_THERMAL_TRIP[4]);

		mtktsbattery_dprintk(
			"g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6],
			g_THERMAL_TRIP[7]);

		mtktsbattery_dprintk(
			"g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

		mtktsbattery_dprintk(
			"[%s] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3);

		mtktsbattery_dprintk(
			"cooldev4=%s,cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind4, g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktsbattery_data->trip[i];

		interval = ptr_mtktsbattery_data->time_msec / 1000;

		mtktsbattery_dprintk(
			"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2]);

		mtktsbattery_dprintk(
			"trip_3_temp=%d,trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,",
			trip_temp[3], trip_temp[4], trip_temp[5],
			trip_temp[6], trip_temp[7]);

		mtktsbattery_dprintk(
			"trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
			trip_temp[8], trip_temp[9], interval * 1000);

		mtktsbattery_dprintk(
			"[%s] mtktsbattery_register_thermal\n", __func__);

		mtktsbattery_register_thermal();
		up(&sem_mutex);

		kfree(ptr_mtktsbattery_data);
		/* battery_write_flag=1; */
		return count;
	}

	mtktsbattery_dprintk("[%s] bad argument\n", __func__);
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
					"mtktsbattery_write", "Bad argument");
    #endif
	kfree(ptr_mtktsbattery_data);
	return -EINVAL;
}

#if 0
static void mtkts_battery_cancel_thermal_timer(void)
{
	/* cancel timer */
	/*pr_debug("mtkts_battery_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */
	/* For charging current throttling during deep idle,
	 *   this delayed work cannot be canceled.
	 *if (thz_dev)
	 *	cancel_delayed_work(&(thz_dev->poll_queue));
	 *
	 *return;
	 */
}

static void mtkts_battery_start_thermal_timer(void)
{
	/*pr_debug("mtkts_battery_start_thermal_timer\n"); */
	/* resume thermal framework polling when leaving deep idle */
	/* For charging current throttling during deep idle,
	 *   this delayed work cannot be canceled.
	 *if (thz_dev != NULL && interval != 0)
	 *	mod_delayed_work(system_freezable_power_efficient_wq,
	 *	&(thz_dev->poll_queue),
	 *   round_jiffies(msecs_to_jiffies(3000)));
	 *
	 *return;
	 */
}
#endif

int mtktsbattery_register_cooler(void)
{
	/* cooling devices */
	cl_dev_sysrst = mtk_thermal_cooling_device_register(
					"mtktsbattery-sysrst", NULL,
					&mtktsbattery_cooling_sysrst_ops);
	return 0;
}

static int mtktsbattery_register_thermal(void)
{
	mtktsbattery_dprintk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsbattery", num_trip,
						NULL, &mtktsbattery_dev_ops,
						0, 0, 0, interval * 1000);

	return 0;
}

void mtktsbattery_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktsbattery_unregister_thermal(void)
{
	mtktsbattery_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static void tsbattery_exit(void)
{
	mtktsbattery_dprintk("[%s]\n", __func__);
	mtktsbattery_unregister_thermal();
	mtktsbattery_unregister_cooler();
}

static int mtkts_battery_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktsbattery_read, NULL);
}

static const struct file_operations mtkts_battery_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_battery_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktsbattery_write,
	.release = single_release,
};

static int __init mtktsbattery_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktsbattery_dir = NULL;

	mtktsbattery_dprintk("[%s]\n", __func__);

	err = mtktsbattery_register_cooler();
	if (err)
		return err;

	err = mtktsbattery_register_thermal();
	if (err)
		goto err_unreg;

	mtktsbattery_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktsbattery_dir) {
		mtktsbattery_dprintk("%s mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzbattery", 0664, mtktsbattery_dir,
							&mtkts_battery_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
#if 0
	mtkTTimer_register("mtktsbattery", mtkts_battery_start_thermal_timer,
					mtkts_battery_cancel_thermal_timer);
#endif
	return 0;

err_unreg:
	mtktsbattery_unregister_cooler();
	return err;
}

static void __exit mtktsbattery_exit(void)
{
	mtktsbattery_dprintk("[%s]\n", __func__);
	mtktsbattery_unregister_thermal();
	mtktsbattery_unregister_cooler();
#if 0
	mtkTTimer_unregister("mtktsbattery");
#endif
}
module_init(mtktsbattery_init);
module_exit(mtktsbattery_exit);
