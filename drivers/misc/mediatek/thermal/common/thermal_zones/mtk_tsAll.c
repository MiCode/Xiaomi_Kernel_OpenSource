/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <linux/slab.h>

#define RESERVED_TZS (21)
#define AUTO_GEN_COOLERS (1)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

struct thz_data {
	struct thermal_zone_device *thz_dev;
	char thz_name[20];
	int trip_temp[10];
	int trip_type[10];	/*ACTIVE, PASSIVE, HOT, and Critical*/
	char bind[10][20];
	int num_trip;
	unsigned int interval;	/* mseconds, 0 : no auto polling */
	int kernelmode;
	struct semaphore sem_mutex;
	int isTimerCancelled;
};

static struct thz_data g_tsData[RESERVED_TZS];

#if AUTO_GEN_COOLERS
static int tztsAll_polling_interval = 1000; /* mseconds, 0 : no auto polling */
static int tztsAll_enable_switch; /* 1: switch on, 0: switch off */
struct cooler_data {
	struct thermal_cooling_device *cooler_dev;
	int state; /* 0: inactivated, 1: activated */
};
/* Our purpose is to make all tsX report their temperatures
 * regularly without activating by a thermal policy,
 * so we have to create the same amount of coolers and bind
 * them together.
 */
static struct cooler_data g_coolerData[RESERVED_TZS];
#endif

static int tsallts_debug_log;

#define TSALLTS_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define tsallts_dprintk(fmt, args...)   \
	do {                                    \
		if (tsallts_debug_log) {                \
			pr_debug("[Thermal/TZ/CPUALL]" fmt, ##args);\
		}                                   \
	} while (0)

#if AUTO_GEN_COOLERS
#define clnothings_dprintk(fmt, args...)   \
	do {                                    \
		if (tsallts_debug_log) {                \
			pr_debug("[Thermal/TZ/CLNOTHINGS]" fmt, ##args);\
		}                                   \
	} while (0)
#endif

#define tsallts_printk(fmt, args...)   \
	pr_debug("[Thermal/TZ/CPUALL]" fmt, ##args)

static void tsX_register(int index);
static void tsX_unregister(int index);

static int tsallts_get_index(struct thermal_zone_device *thermal)
{
	/* ex: tzts1, tzts2, ..., and tzts10 */
	int index;

	index = thermal->type[4] - '0';

	if (thermal->type[5] != '\0')
		index = index * 10 + thermal->type[5] - '0';

	index = index - 1;

	if (index < 0 || index >= TS_ENUM_MAX)
		index = 0;

	return index;
}

static int tsallts_get_temp(struct thermal_zone_device *thermal, int *t)
{
	int curr_temp, index;

	index = tsallts_get_index(thermal);
	curr_temp = get_immediate_tsX[index]();
	tsallts_dprintk("%s ts%d =%d\n", __func__, index, curr_temp);

	*t = curr_temp;

#if AUTO_GEN_COOLERS
	thermal->polling_delay = g_tsData[index].interval;
#endif
	return 0;
}

static int tsallts_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = -1, index, i;

	index = tsallts_get_index(thermal);
	tsallts_dprintk("[%s ts%d]\n", __func__, index);

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_tsData[index].bind[i])) {
			table_val = i;
			break;
		}
	}

	if (table_val == -1)
		return 0;

	tsallts_dprintk("[%s ts %d] %s\n", __func__, index, cdev->type);

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		tsallts_dprintk(
			"[%s ts %d] error binding cooling dev\n", __func__,
			index);

		return -EINVAL;
	}

	tsallts_dprintk("[%s ts %d] binding OK, %d\n", __func__, index,
								table_val);
	return 0;
}

static int tsallts_unbind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = -1, index, i;

	index = tsallts_get_index(thermal);
	tsallts_dprintk("[%s ts%d]\n", __func__, index);

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_tsData[index].bind[i])) {
			table_val = i;
			break;
		}
	}

	if (table_val == -1)
		return 0;

	tsallts_dprintk("[%s ts %d] %s\n", __func__, index, cdev->type);

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		tsallts_dprintk(
			"[%s ts %d] error unbinding cooling dev\n", __func__,
			index);

		return -EINVAL;
	}

	tsallts_dprintk("[%s ts %d] unbinding OK\n", __func__, index);
	return 0;
}

static int tsallts_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	int index;

	index = tsallts_get_index(thermal);

	*mode = (g_tsData[index].kernelmode) ?
			THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int tsallts_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	int index;

	index = tsallts_get_index(thermal);

	g_tsData[index].kernelmode = mode;
	return 0;
}

static int tsallts_get_trip_type(struct thermal_zone_device *thermal, int trip,
		enum thermal_trip_type *type)
{
	int index;

	index = tsallts_get_index(thermal);
	*type = g_tsData[index].trip_type[trip];
	return 0;
}

static int tsallts_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	int index;

	index = tsallts_get_index(thermal);
	*temp = g_tsData[index].trip_temp[trip];
	return 0;
}

static int tsallts_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = TSALLTS_TEMP_CRIT;
	return 0;
}

static void mtkts_allts_cancel_timer(void)
{
	int i;

	for (i = 0; i < TS_ENUM_MAX; i++) {
		if (down_trylock(&g_tsData[i].sem_mutex))
			continue;

		if (g_tsData[i].thz_dev) {
			cancel_delayed_work(&(g_tsData[i].thz_dev->poll_queue));
			g_tsData[i].isTimerCancelled = 1;
		}
		up(&g_tsData[i].sem_mutex);
	}
}


static void mtkts_allts_start_timer(void)
{
	int i;

	for (i = 0; i < TS_ENUM_MAX; i++) {
		if (!g_tsData[i].isTimerCancelled)
			continue;


		if (down_trylock(&g_tsData[i].sem_mutex))
			continue;

		if (g_tsData[i].thz_dev != NULL && g_tsData[i].interval != 0) {
			mod_delayed_work(system_freezable_power_efficient_wq,
					&(g_tsData[i].thz_dev->poll_queue),
					round_jiffies(msecs_to_jiffies(1000)));
			g_tsData[i].isTimerCancelled = 0;
		}
		up(&g_tsData[i].sem_mutex);
		/*1000 = 1sec */
	}
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops tsallts_dev_ops = {
	.bind = tsallts_bind,
	.unbind = tsallts_unbind,
	.get_temp = tsallts_get_temp,
	.get_mode = tsallts_get_mode,
	.set_mode = tsallts_set_mode,
	.get_trip_type = tsallts_get_trip_type,
	.get_trip_temp = tsallts_get_trip_temp,
	.get_crit_temp = tsallts_get_crit_temp,
};

#define PROC_FOPS_RW(num)	\
static int tz ## num ## _proc_read(struct seq_file *m, void *v)	\
{	\
	int i;	\
\
	for (i = 0; i < 10; i++) {	\
		seq_printf(m, "Trip_%d_temp=%d ", i,	\
			g_tsData[(num - 1)].trip_temp[i]);	\
		if ((i + 1) % 5 == 0)	\
			seq_printf(m, "\n");	\
	}	\
\
	for (i = 0; i < 10; i++) {	\
		seq_printf(m, "Trip_type%d=%d ", i,	\
			g_tsData[(num - 1)].trip_type[i]);	\
		if ((i + 1) % 5 == 0)	\
			seq_printf(m, "\n");	\
	}	\
\
	for (i = 0; i < 10; i++) {	\
		seq_printf(m, "Cool_dev%d=%s ", i,	\
			g_tsData[(num - 1)].bind[i]);	\
		if ((i + 1) % 5 == 0)	\
			seq_printf(m, "\n");	\
	}	\
	seq_printf(m, "Time_ms=%d\n", g_tsData[(num - 1)].interval);	\
	return 0;	\
}	\
\
static ssize_t tz ## num ## _proc_write(	\
struct file *file, const char __user *buffer, size_t count,	\
			     loff_t *data)	\
{	\
	int len = 0, i, j;	\
	struct tempD {	\
		int num_trip;	\
		int time_msec;	\
		int trip[10];	\
		int t_type[10];	\
		char bind[10][20];	\
		char desc[512];	\
	};	\
\
	struct tempD *pTempD = kmalloc(sizeof(*pTempD), GFP_KERNEL);	\
\
	tsallts_printk("[tsallts_write_"__stringify(num)"]\n");	\
\
	if (pTempD == NULL)	\
		return -ENOMEM;	\
\
	len = (count < (sizeof(pTempD->desc) - 1)) ?	\
				count : (sizeof(pTempD->desc) - 1);	\
	if (copy_from_user(pTempD->desc, buffer, len)) {	\
		kfree(pTempD);	\
		return 0;	\
	}	\
\
	pTempD->desc[len] = '\0';\
\
	i = sscanf(pTempD->desc,	\
"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d"	\
"%19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",	\
		&pTempD->num_trip,	\
		&pTempD->trip[0], &pTempD->t_type[0], pTempD->bind[0],	\
		&pTempD->trip[1], &pTempD->t_type[1], pTempD->bind[1],	\
		&pTempD->trip[2], &pTempD->t_type[2], pTempD->bind[2],	\
		&pTempD->trip[3], &pTempD->t_type[3], pTempD->bind[3],	\
		&pTempD->trip[4], &pTempD->t_type[4], pTempD->bind[4],	\
		&pTempD->trip[5], &pTempD->t_type[5], pTempD->bind[5],	\
		&pTempD->trip[6], &pTempD->t_type[6], pTempD->bind[6],	\
		&pTempD->trip[7], &pTempD->t_type[7], pTempD->bind[7],	\
		&pTempD->trip[8], &pTempD->t_type[8], pTempD->bind[8],	\
		&pTempD->trip[9], &pTempD->t_type[9], pTempD->bind[9],	\
		&pTempD->time_msec);	\
\
	if (i == 32) {	\
		down(&g_tsData[(num - 1)].sem_mutex);	\
		tsallts_dprintk("[tsallts_write_"__stringify(num)	\
					"]thermal unregister\n");	\
\
		if (g_tsData[(num - 1)].thz_dev) {	\
			mtk_thermal_zone_device_unregister(	\
				g_tsData[(num - 1)].thz_dev);	\
				g_tsData[(num - 1)].thz_dev = NULL;	\
		}	\
\
		if (pTempD->num_trip < 0 || pTempD->num_trip > 10) {	\
			tsallts_dprintk(	\
				"[tsallts_write1] bad argument\n");	\
				kfree(pTempD);	\
				up(&g_tsData[(num - 1)].sem_mutex);	\
				return -EINVAL;	\
		}	\
\
		g_tsData[(num - 1)].num_trip = pTempD->num_trip;	\
\
		for (i = 0; i < g_tsData[(num - 1)].num_trip; i++) {	\
			g_tsData[(num - 1)].trip_type[i] =	\
						pTempD->t_type[i];	\
			g_tsData[(num - 1)].trip_temp[i] =	\
						pTempD->trip[i];	\
		}	\
\
		for (i = 0; i < 10; i++) {	\
			g_tsData[(num - 1)].bind[i][0] = '\0';	\
			for (j = 0; j < 20; j++)	\
				g_tsData[(num - 1)].bind[i][j] =	\
						pTempD->bind[i][j];	\
		}	\
\
		g_tsData[(num - 1)].interval = pTempD->time_msec;	\
\
		if (tsallts_debug_log) {	\
			for (i = 0; i < 10; i++) {	\
				tsallts_printk("Trip_%d_temp=%d ", i,	\
				g_tsData[(num - 1)].trip_temp[i]);	\
				if ((i + 1) % 5 == 0)	\
					tsallts_printk("\n");	\
			}	\
\
			for (i = 0; i < 10; i++) {	\
				tsallts_printk("Trip_type%d=%d ", i,	\
				g_tsData[(num - 1)].trip_type[i]);	\
				if ((i + 1) % 5 == 0)	\
					tsallts_printk("\n");	\
			}	\
\
			for (i = 0; i < 10; i++) {	\
				tsallts_printk("Cool_dev%d=%s ", i,	\
				g_tsData[(num - 1)].bind[i]);	\
				if ((i + 1) % 5 == 0)	\
					tsallts_printk("\n");	\
			}	\
			tsallts_printk("Time_ms=%d\n",	\
					g_tsData[(num - 1)].interval);	\
		}	\
\
		tsallts_dprintk("[tsallts_write_"__stringify(num)	\
					"] thermal register\n");	\
		if (g_tsData[(num - 1)].thz_dev == NULL) {	\
			g_tsData[(num - 1)].thz_dev =	\
				mtk_thermal_zone_device_register(	\
					g_tsData[(num - 1)].thz_name,\
					g_tsData[(num - 1)].num_trip,	\
					NULL, &tsallts_dev_ops, 0,	\
				0, 0, g_tsData[(num - 1)].interval);	\
		}	\
\
		up(&g_tsData[(num - 1)].sem_mutex);	\
		kfree(pTempD);	\
		return count;	\
	}	\
\
	tsallts_dprintk("[tsallts_write_"__stringify(num)	\
					"] bad argument\n");	\
	kfree(pTempD);	\
	return -EINVAL;	\
}	\
static int tz ## num ## _proc_open(	\
struct inode *inode, struct file *file)	\
{	\
	return single_open(file, tz ## num ## _proc_read,	\
			PDE_DATA(inode));	\
}	\
static const struct file_operations tz ## num ## _proc_fops = {	\
	.owner          = THIS_MODULE,	\
	.open           = tz ## num ## _proc_open,	\
	.read           = seq_read,	\
	.llseek         = seq_lseek,	\
	.release        = single_release,	\
	.write          = tz ## num ## _proc_write,	\
}

#define FOPS(num)	(&tz ## num ## _proc_fops)

PROC_FOPS_RW(1);
PROC_FOPS_RW(2);
PROC_FOPS_RW(3);
PROC_FOPS_RW(4);
PROC_FOPS_RW(5);
PROC_FOPS_RW(6);
PROC_FOPS_RW(7);
PROC_FOPS_RW(8);
PROC_FOPS_RW(9);
PROC_FOPS_RW(10);
PROC_FOPS_RW(11);
PROC_FOPS_RW(12);
PROC_FOPS_RW(13);
PROC_FOPS_RW(14);
PROC_FOPS_RW(15);
PROC_FOPS_RW(16);
PROC_FOPS_RW(17);
PROC_FOPS_RW(18);
PROC_FOPS_RW(19);
PROC_FOPS_RW(20);
PROC_FOPS_RW(21);

static const struct file_operations *thz_fops[RESERVED_TZS] = {
	FOPS(1),
	FOPS(2),
	FOPS(3),
	FOPS(4),
	FOPS(5),
	FOPS(6),
	FOPS(7),
	FOPS(8),
	FOPS(9),
	FOPS(10),
	FOPS(11),
	FOPS(12),
	FOPS(13),
	FOPS(14),
	FOPS(15),
	FOPS(16),
	FOPS(17),
	FOPS(18),
	FOPS(19),
	FOPS(20),
	FOPS(21)
};

#if AUTO_GEN_COOLERS
static int thz_enable_read(struct seq_file *m, void *v)
{
	seq_puts(m, "Current status:\n");
	seq_printf(m, "tztsAll_enable_switch: %d\n", tztsAll_enable_switch);
	seq_printf(m, "tztsAll_polling_interval: %d ms\n\n",
		tztsAll_polling_interval);
	seq_puts(m, "[Note]\n");
	seq_puts(m, "1. Enable tztsAll\n");
	seq_puts(m, "   echo switch 1 > /proc/driver/thermal/tztsAll_enable\n");
	seq_puts(m, "2. Disable tztsAll\n");
	seq_puts(m, "   echo switch 0 > /proc/driver/thermal/tztsAll_enable\n");
	seq_puts(m, "3. Change polling interval\n");
	seq_puts(m, "   The polling interval is in millisecond\n");
	seq_puts(m, "   For example: the polling interval is 1s\n");
	seq_puts(m, "   echo polling_interval 1000 > /proc/driver/thermal/tztsAll_enable\n");

	return 0;
}

static ssize_t thz_enable_write
(struct file *file, const char __user *buffer, size_t count,
		loff_t *data)
{
	char desc[32], arg_name[32];
	int i, len = 0, arg_val;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%31s %d", arg_name, &arg_val) == 2) {
		if ((strncmp(arg_name, "switch", 6) == 0)) {
			if (arg_val != 0 && arg_val != 1)
				goto THZ_ENABLE_WRITE_ERROR;

			tztsAll_enable_switch = arg_val;
			for (i = 0; i < TS_ENUM_MAX; i++) {
				if (tztsAll_enable_switch == 1) {
					down(&g_tsData[i].sem_mutex);
					g_tsData[i].interval =
						tztsAll_polling_interval;
					tsX_unregister(i);
					tsX_register(i);
					up(&g_tsData[i].sem_mutex);
				} else {
					g_tsData[i].interval = 0;
				}
			}
		} else if ((strncmp(arg_name, "polling_interval", 16) == 0)) {
			tztsAll_polling_interval = arg_val;
			for (i = 0; i < TS_ENUM_MAX; i++)
				g_tsData[i].interval = arg_val;
		} else {
			goto THZ_ENABLE_WRITE_ERROR;
		}
		return count;
	}

THZ_ENABLE_WRITE_ERROR:
	tsallts_printk("%s bad argument: %s\n", __func__, desc);
	return -EINVAL;
}

static int thz_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, thz_enable_read, NULL);
}
static const struct file_operations thz_enable_fops = {
	.owner = THIS_MODULE,
	.open = thz_enable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = thz_enable_write,
	.release = single_release,
};
static int clnothings_get_index(struct thermal_cooling_device *cdev)
{
	/* ex: clnothing1, clnothing2, ..., and clnothing10 */
	int index;

	index = cdev->type[9] - '0';

	if (cdev->type[10] != '\0')
		index = index * 10 + cdev->type[10] - '0';

	index = index - 1;

	if (index < 0 || index >= TS_ENUM_MAX)
		index = 0;

	return index;
}

static int clnothings_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int clnothings_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int i;

	i = clnothings_get_index(cdev);
	*state = g_coolerData[i].state;

	return 0;
}

static int clnothings_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	int i;

	i = clnothings_get_index(cdev);
	g_coolerData[i].state = state;

	if (state == 1) {
		/* Do nothing */
		clnothings_dprintk("%s triggered\n", cdev->type);
	}

	return 0;
}
static struct thermal_cooling_device_ops clnothings_ops = {
	.get_max_state = clnothings_get_max_state,
	.get_cur_state = clnothings_get_cur_state,
	.set_cur_state = clnothings_set_cur_state,
};
#endif

static int __init tsallts_init(void)
{
	int i, j;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *tsallts_dir = NULL;

#if AUTO_GEN_COOLERS
	char temp[20] = { 0 };
#endif

	tsallts_dprintk("[%s]\n", __func__);

	if (TS_ENUM_MAX > RESERVED_TZS) {
		tsallts_printk("Didn't reserve enough memory for all tsX\n");
		return -1;
	}

	for (i = 0; i < RESERVED_TZS; i++) {
		g_tsData[i].thz_dev = NULL;
		g_tsData[i].thz_name[0] = '\0';
		for (j = 0; j < 10; j++) {
			g_tsData[i].trip_temp[j] = 0;
			g_tsData[i].trip_type[j] = 0;
			g_tsData[i].bind[j][0] = '\0';
		}
		g_tsData[i].num_trip = 0;
		g_tsData[i].interval = 0;
		g_tsData[i].kernelmode = 0;
		sema_init(&g_tsData[i].sem_mutex, 1);
		g_tsData[i].isTimerCancelled = 0;
	}

#if AUTO_GEN_COOLERS
	for (i = 0; i < TS_ENUM_MAX; i++) {
		g_coolerData[i].cooler_dev = NULL;
		g_coolerData[i].state = 0;

		sprintf(temp, "clnothing%d", (i + 1));
		g_coolerData[i].cooler_dev =
				mtk_thermal_cooling_device_register(
				temp, (void *)&g_coolerData[i].state,
				&clnothings_ops);
	}
#endif

	tsallts_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!tsallts_dir) {
		tsallts_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		for (i = 0; i < TS_ENUM_MAX; i++)
			sprintf(g_tsData[i].thz_name, "tzts%d", (i + 1));

		for (i = 0; i < TS_ENUM_MAX; i++) {
			entry = proc_create(g_tsData[i].thz_name, 0664,
						tsallts_dir, thz_fops[i]);
			if (entry)
				proc_set_user(entry, uid, gid);
		}

#if AUTO_GEN_COOLERS
		entry = proc_create("tztsAll_enable", 0664,
				tsallts_dir, &thz_enable_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif
	}

#if AUTO_GEN_COOLERS
	for (i = 0; i < TS_ENUM_MAX; i++) {
		down(&g_tsData[i].sem_mutex);
		tsX_unregister(i);

		g_tsData[i].num_trip = 1;
		g_tsData[i].trip_type[0] =  0;
		g_tsData[i].trip_temp[0] = 150000;
		sprintf(g_tsData[i].bind[0], "clnothing%d", (i + 1));

		if (tztsAll_enable_switch == 1) {
			g_tsData[i].interval = tztsAll_polling_interval;
			tsX_register(i);
		} else {
			g_tsData[i].interval = 0;
		}

		up(&g_tsData[i].sem_mutex);
	}
#endif

	mtkTTimer_register("tztsAll", mtkts_allts_start_timer,
						mtkts_allts_cancel_timer);
	return 0;
}

static void tsX_register(int index)
{
	if (g_tsData[index].thz_dev == NULL) {
		g_tsData[index].thz_dev = mtk_thermal_zone_device_register(
			g_tsData[index].thz_name,
			g_tsData[index].num_trip,
			NULL, &tsallts_dev_ops, 0,
			0, 0, g_tsData[index].interval);
	}
}

static void tsX_unregister(int index)
{
	if (g_tsData[index].thz_dev != NULL) {
		mtk_thermal_zone_device_unregister(
				g_tsData[index].thz_dev);
		g_tsData[index].thz_dev = NULL;
	}

}

static void tsallts_unregister_thermal(void)
{
	int i;

	tsallts_dprintk("[%s]\n", __func__);

	for (i = 0; i < RESERVED_TZS; i++) {
		down(&g_tsData[i].sem_mutex);
		tsX_unregister(i);
		up(&g_tsData[i].sem_mutex);
	}
}

static void __exit tsallts_exit(void)
{
	tsallts_dprintk("[%s]\n", __func__);
	tsallts_unregister_thermal();
	mtkTTimer_unregister("tztsAll");
}
module_init(tsallts_init);
module_exit(tsallts_exit);
