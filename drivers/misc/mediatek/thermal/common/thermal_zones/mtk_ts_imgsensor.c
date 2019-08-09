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
#include "kd_camera_feature.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_api.h"

/*=============================================================
 * Weak function
 *=============================================================
 */
MUINT32 __attribute__ ((weak))
Get_Camera_Temperature(
enum CAMERA_DUAL_CAMERA_SENSOR_ENUM senDevId, MUINT8 *valid, MUINT32 *temp)
{
	pr_notice("[Thermal/TZ/IMGS] E_WF: %s doesn't exist\n", __func__);
	return 0;
}

/*=============================================================
 * Macro
 *=============================================================
 */

/* If RESERVED_TZS is changed,
 * please adjust corresponding proc macro FOPS and PROC_FOPS_RW in this file
 */
#define RESERVED_TZS (8)
#define MTK_IMGS_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define mtk_imgs_dprintk(fmt, args...)   \
	do {                                    \
		if (mtk_imgs_debug_log) {                \
			pr_notice("[Thermal/TZ/IMGS]" fmt, ##args);\
		}                                   \
	} while (0)

#define mtk_imgs_printk(fmt, args...)   \
	pr_notice("[Thermal/TZ/IMGS]" fmt, ##args)
/*=============================================================
 * Function prototype
 *=============================================================
 */
/*=============================================================
 * Local variable
 *=============================================================
 */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

struct thz_data {
	struct thermal_zone_device *thz_dev;
	int cTemp;		/* Current temp */
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

/* 0: disable debug logs, Not 0: enable debug logs */
static int mtk_imgs_debug_log;

struct cooler_data {
	struct thermal_cooling_device *cl_dev;
	char cl_name[20];
	unsigned int sysrst_state;
};

static struct cooler_data g_clData[RESERVED_TZS];
static int g_img_max; /* Number of image sensors in this platform */

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *	use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5;
static int polling_factor2 = 10;
/*=============================================================
 * Local function
 *=============================================================
 */

/* Return -1 if the index is out of array boundary */
static int mtk_imgs_get_num_imgs(int *num)
{
	int i = 0;

	while (DUAL_CAMERA_SENSOR_MAX > (1 << i))
		i++;

	/* Add one thermal zone, tzimgs0, to get the max temperatures
	 * of all image sensors
	 */
	i++;

	*num = i;

	if (i > RESERVED_TZS) {
		*num = RESERVED_TZS;
		return -1;
	}

	return 0;
}

/*=============================================================
 * Debug switch
 *=============================================================
 */
static ssize_t mtk_imgs_write_log(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int log_switch;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &log_switch) == 0) {
		mtk_imgs_debug_log = log_switch;
		return count;
	}

	mtk_imgs_printk("[%s] bad argument\n", __func__);
	return -EINVAL;
}

static int mtk_imgs_read_log(struct seq_file *m, void *v)
{
	seq_printf(m, "[%s] log = %d\n", __func__, mtk_imgs_debug_log);

	return 0;
}

static int mtk_imgs_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_imgs_read_log, NULL);
}

static const struct file_operations mtk_imgs_log_fops = {
	.owner = THIS_MODULE,
	.open = mtk_imgs_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_imgs_write_log,
	.release = single_release,
};
/*=============================================================
 * Image sensor on/off status
 *=============================================================
 */
static int sensor_on_off_bitmap; /* 0: power off, 1: power on*/
enum sensor_state {power_off = 0, power_on = 1};

static void set_image_sensor_state(int device, enum sensor_state status)
{
	int mask = 1;

	mask = mask << device;

	if (status == power_on)
		sensor_on_off_bitmap |= mask;
	else
		sensor_on_off_bitmap &= ~(mask);

}
/* It returns a bitmap to indicate power on/off status of all image
 * sensors. Each bit binds to physical image sensor except bit 0.
 * Bit 0 binds the pseudo image sensor reporting the max temperature
 * of all physical image sensors, so bit 0 is always 0.
 * For example:
 * If bit 1 is 1, it means the image sensor 1 is power on. Otherwise,
 * it is power off.
 */
int get_image_sensor_state(void)
{
	return sensor_on_off_bitmap;
}
/*=============================================================
 * Thermal Zone
 *=============================================================
 */

static int mtk_imgs_get_index(struct thermal_zone_device *thermal)
{
	/* ex: tzimgs0, tzimgs1, ..., and tzimgs10 */
	int index;

	index = thermal->type[6] - '0';

	if (thermal->type[7] != '\0')
		index = index * 10 + thermal->type[7] - '0';

	if (index < 0 || index >= TS_ENUM_MAX)
		index = 0;

	return index;
}

static int mtk_imgs_get_max_temp(void)
{
	int i, max = -127000;

	/* Skip tzimgs0 because it is not a real sensor */
	if (g_img_max >= 1)
		max = g_tsData[1].cTemp;

	for (i = 2; i < g_img_max; i++) {
		if (max < g_tsData[i].cTemp)
			max = g_tsData[i].cTemp;
	}

	return max;
}

static int mtk_imgs_get_temp(struct thermal_zone_device *thermal, int *t)
{
	int curr_temp, index, ret;
	MUINT8 invalid;

	index = mtk_imgs_get_index(thermal);

	if (index == 0) {
		/* tzimgs0 is a pseduo thermal zone.
		 * It reports the max temperature of all image sensors
		 */
		curr_temp = mtk_imgs_get_max_temp();
	} else {
		ret = Get_Camera_Temperature(
				1 << (index - 1), &invalid, &curr_temp);

		curr_temp *= 1000;

		if ((invalid & SENSOR_TEMPERATURE_UNKNOWN_STATUS) != 0) {
			mtk_imgs_dprintk(
				"Invalid 0x%x: Image sensor %d status unknown\n",
				invalid,  1 << (index - 1));
			/* In sensor init state
			 * report the invalid temp
			 */
			curr_temp = -127000;

			set_image_sensor_state(index, power_on);
		} else if ((invalid & SENSOR_TEMPERATURE_CANNOT_SEARCH_SENSOR)
		!= 0) {
			mtk_imgs_dprintk(
				"Invalid 0x%x: Cannot serach the image sensor %d\n",
				invalid,  1 << (index - 1));
			/* The sensor doesn't exist in this project,
			 * Assign the invalid temp, and stop polling
			 */
			curr_temp = -127000;
			g_tsData[index].cTemp = curr_temp;
			*t = curr_temp;
			thermal->polling_delay = 0;

			set_image_sensor_state(index, power_off);
			return 0;
		} else if ((invalid & SENSOR_TEMPERATURE_NOT_POWER_ON) != 0) {
			mtk_imgs_dprintk(
				"Invalid 0x%x: Image sensor %d not power on\n",
				invalid,  1 << (index - 1));
			/* The camera doesn't power on
			 * report the invalid temp
			 */
			curr_temp = -127000;
			set_image_sensor_state(index, power_off);
		} else if ((invalid & SENSOR_TEMPERATURE_NOT_POWER_ON) == 0) {
			if ((invalid & SENSOR_TEMPERATURE_NOT_SUPPORT_THERMAL)
			!= 0) {
				mtk_imgs_dprintk(
					"Invalid 0x%x: Image sensor %d doesn't support power meter\n",
					invalid,  1 << (index - 1));
				/* The camera doesn't support power meter
				 * report the invalid temp
				 */
				curr_temp = -127000;
			} else if ((invalid & SENSOR_TEMPERATURE_VALID) == 0) {
				mtk_imgs_dprintk(
					"Invalid 0x%x: Image sensor %d reports invalid temp temporarily\n",
					invalid,  1 << (index - 1));
				/* Report the previous temp */
				curr_temp = g_tsData[index].cTemp;
			}

			set_image_sensor_state(index, power_on);
		}

		if (ret != 0) {
			mtk_imgs_dprintk("Error (%d): Image sensor %d\n", ret,
				 1 << (index - 1));
			/* Report the current temperature as previous one */
			curr_temp = g_tsData[index].cTemp;
		}
	}

	mtk_imgs_dprintk("mtk_imgs_get_temp ts%d =%d\n", index, curr_temp);

	g_tsData[index].cTemp = curr_temp;
	*t = curr_temp;

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = g_tsData[index].interval;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = g_tsData[index].interval *
							polling_factor2;
	else
		thermal->polling_delay = g_tsData[index].interval *
							polling_factor1;

	return 0;
}

static int mtk_imgs_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = -1, index, i;

	index = mtk_imgs_get_index(thermal);
	mtk_imgs_dprintk("[mtk_imgs_bind ts%d]\n", index);

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_tsData[index].bind[i])) {
			table_val = i;
			break;
		}
	}

	if (table_val == -1)
		return 0;

	mtk_imgs_dprintk("[mtk_imgs_bind ts %d] %s\n", index, cdev->type);

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtk_imgs_dprintk(
			"[mtk_imgs_bind ts %d] error binding cooling dev\n",
			index);
		return -EINVAL;
	}

	mtk_imgs_dprintk("[mtk_imgs_bind ts %d] binding OK, %d\n",
						index, table_val);

	return 0;
}

static int mtk_imgs_unbind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = -1, index, i;

	index = mtk_imgs_get_index(thermal);
	mtk_imgs_dprintk("[mtk_imgs_unbind ts%d]\n", index);

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_tsData[index].bind[i])) {
			table_val = i;
			break;
		}
	}

	if (table_val == -1)
		return 0;

	mtk_imgs_dprintk("[mtk_imgs_unbind ts %d] %s\n", index, cdev->type);

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtk_imgs_dprintk(
			"[mtk_imgs_unbind ts %d] error unbinding cooling dev\n",
									index);
		return -EINVAL;
	}

	mtk_imgs_dprintk("[mtk_imgs_unbind ts %d] unbinding OK\n", index);
	return 0;
}

static int mtk_imgs_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	int index;

	index = mtk_imgs_get_index(thermal);

	*mode = (g_tsData[index].kernelmode) ?
			THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtk_imgs_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	int index;

	index = mtk_imgs_get_index(thermal);

	g_tsData[index].kernelmode = mode;
	return 0;
}

static int mtk_imgs_get_trip_type(struct thermal_zone_device *thermal, int trip,
		enum thermal_trip_type *type)
{
	int index;

	index = mtk_imgs_get_index(thermal);
	*type = g_tsData[index].trip_type[trip];
	return 0;
}

static int mtk_imgs_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	int index;

	index = mtk_imgs_get_index(thermal);
	*temp = g_tsData[index].trip_temp[trip];
	return 0;
}

static int mtk_imgs_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTK_IMGS_TEMP_CRIT;
	return 0;
}

static void mtkts_allts_cancel_timer(void)
{
	int i;

	for (i = 0; i < g_img_max; i++) {
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

	for (i = 0; i < g_img_max; i++) {
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
static struct thermal_zone_device_ops mtk_imgs_dev_ops = {
	.bind = mtk_imgs_bind,
	.unbind = mtk_imgs_unbind,
	.get_temp = mtk_imgs_get_temp,
	.get_mode = mtk_imgs_get_mode,
	.set_mode = mtk_imgs_set_mode,
	.get_trip_type = mtk_imgs_get_trip_type,
	.get_trip_temp = mtk_imgs_get_trip_temp,
	.get_crit_temp = mtk_imgs_get_crit_temp,
};

#define PROC_FOPS_RW(num)	\
static int tz ## num ## _proc_read(struct seq_file *m, void *v)	\
{	\
	int i;	\
\
	for (i = 0; i < 10; i++) {	\
		seq_printf(m, "Trip_%d_temp=%d ", i,	\
			g_tsData[num].trip_temp[i]);	\
		if ((i + 1) % 5 == 0)	\
			seq_printf(m, "\n");	\
	}	\
\
	for (i = 0; i < 10; i++) {	\
		seq_printf(m, "Trip_type%d=%d ", i,	\
			g_tsData[num].trip_type[i]);	\
		if ((i + 1) % 5 == 0)	\
			seq_printf(m, "\n");	\
	}	\
\
	for (i = 0; i < 10; i++) {	\
		seq_printf(m, "Cool_dev%d=%s ", i,	\
			g_tsData[num].bind[i]);	\
		if ((i + 1) % 5 == 0)	\
			seq_printf(m, "\n");	\
	}	\
	seq_printf(m, "Time_ms=%d\n", g_tsData[num].interval);	\
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
	mtk_imgs_printk("[mtk_imgs_write_"__stringify(num)"]\n");	\
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
"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d "	\
"%d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",\
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
		down(&g_tsData[num].sem_mutex);	\
		mtk_imgs_dprintk(	\
	"[mtk_imgs_write_"__stringify(num)"] thermal unregister\n");	\
\
		if (g_tsData[num].thz_dev) {	\
			mtk_thermal_zone_device_unregister(	\
			g_tsData[num].thz_dev);	\
			g_tsData[num].thz_dev = NULL;	\
		}	\
\
		if (pTempD->num_trip < 0 || pTempD->num_trip > 10) {	\
			aee_kernel_warning_api(__FILE__, __LINE__,	\
				DB_OPT_DEFAULT,	\
				"[mtk_imgs_write_"__stringify(num)"]",	\
				"Bad argument");	\
			mtk_imgs_dprintk(	\
				"[mtk_imgs_write1] bad argument\n");	\
			kfree(pTempD);	\
			up(&g_tsData[num].sem_mutex);	\
			return -EINVAL;	\
		}	\
\
		g_tsData[num].num_trip = pTempD->num_trip;	\
\
		for (i = 0; i < g_tsData[num].num_trip; i++) {	\
			g_tsData[num].trip_type[i] = pTempD->t_type[i];	\
			g_tsData[num].trip_temp[i] = pTempD->trip[i];	\
		}	\
\
		for (i = 0; i < 10; i++) {	\
			g_tsData[num].bind[i][0] = '\0';	\
			for (j = 0; j < 20; j++)	\
				g_tsData[num].bind[i][j] =	\
						pTempD->bind[i][j];	\
		}	\
\
		g_tsData[num].interval = pTempD->time_msec;	\
\
		if (mtk_imgs_debug_log) {	\
			for (i = 0; i < 10; i++) {	\
				mtk_imgs_printk("Trip_%d_temp=%d ",	\
					i, g_tsData[num].trip_temp[i]);	\
				if ((i + 1) % 5 == 0)	\
					mtk_imgs_printk("\n");	\
			}	\
\
			for (i = 0; i < 10; i++) {	\
				mtk_imgs_printk("Trip_type%d=%d ", i,	\
					g_tsData[num].trip_type[i]);	\
				if ((i + 1) % 5 == 0)	\
					mtk_imgs_printk("\n");	\
			}	\
\
			for (i = 0; i < 10; i++) {	\
				mtk_imgs_printk("Cool_dev%d=%s ", i,	\
					g_tsData[num].bind[i]);	\
				if ((i + 1) % 5 == 0)	\
					mtk_imgs_printk("\n");	\
			}	\
			mtk_imgs_printk("Time_ms=%d\n",	\
					g_tsData[num].interval);	\
		}	\
\
		mtk_imgs_dprintk(	\
	"[mtk_imgs_write_"__stringify(num)"] thermal register\n");	\
		if (g_tsData[num].thz_dev == NULL) {	\
			g_tsData[num].thz_dev =	\
			mtk_thermal_zone_device_register(	\
				g_tsData[num].thz_name,\
				g_tsData[num].num_trip, NULL,	\
				&mtk_imgs_dev_ops, 0,\
				0, 0, g_tsData[num].interval);	\
		}	\
		up(&g_tsData[num].sem_mutex);	\
\
		kfree(pTempD);	\
		return count;	\
	}	\
\
	mtk_imgs_dprintk(	\
		"[mtk_imgs_write_"__stringify(num)"] bad argument\n");	\
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,	\
			"[mtk_imgs_write_"__stringify(num)"]", \
			"Bad argument");	\
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

PROC_FOPS_RW(0);
PROC_FOPS_RW(1);
PROC_FOPS_RW(2);
PROC_FOPS_RW(3);
PROC_FOPS_RW(4);
PROC_FOPS_RW(5);
PROC_FOPS_RW(6);
PROC_FOPS_RW(7);

static const struct file_operations *thz_fops[RESERVED_TZS] = {
	FOPS(0),
	FOPS(1),
	FOPS(2),
	FOPS(3),
	FOPS(4),
	FOPS(5),
	FOPS(6),
	FOPS(7),
};
/*=============================================================
 * Cooler
 *=============================================================
 */

static int mtk_imgs_cooler_get_index(struct thermal_cooling_device *cdev)
{

	/* ex: tzimgs0-sysrst, tzimgs1-sysrst, ..., and tzimgs10-sysrst */
	int index;

	index = cdev->type[6] - '0';

	if (cdev->type[7] != '-')
		index = index * 10 + cdev->type[7] - '0';

	if (index < 0 || index >= TS_ENUM_MAX)
		index = 0;

	return index;
}

static int mtk_imgs_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int mtk_imgs_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	int index;

	index = mtk_imgs_cooler_get_index(cdev);

	*state = g_clData[index].sysrst_state;

	return 0;
}

static int mtk_imgs_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	int index;

	index = mtk_imgs_cooler_get_index(cdev);
	g_clData[index].sysrst_state = state;

	if (g_clData[index].sysrst_state == 1) {
		pr_debug("%s: reset, reset, reset!!!\n",
						g_clData[index].cl_name);

		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		pr_debug("*****************************************\n");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();
	}
	return 0;
}

static struct thermal_cooling_device_ops mtk_imgs_cooling_sysrst_ops = {
	.get_max_state = mtk_imgs_sysrst_get_max_state,
	.get_cur_state = mtk_imgs_sysrst_get_cur_state,
	.set_cur_state = mtk_imgs_sysrst_set_cur_state,
};

/*=============================================================*/
static int __init mtk_imgs_init(void)
{
	int i, j, ret;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtk_imgs_dir = NULL;

	mtk_imgs_printk("[%s]\n", __func__);

	ret = mtk_imgs_get_num_imgs(&g_img_max);

	if (ret != 0) {
		mtk_imgs_printk("[%s] Reserved tzs are not enough\n", __func__);
		mtk_imgs_printk("[%s] Reserved tzs are not enough\n", __func__);
		mtk_imgs_printk("[%s] Reserved tzs are not enough\n", __func__);
		return 0;
	}

	for (i = 0; i < g_img_max; i++) {
		g_tsData[i].thz_dev = NULL;
		g_tsData[i].cTemp = -127000;
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

	mtk_imgs_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!mtk_imgs_dir) {
		mtk_imgs_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
		return 0;
	}

	/* Initialize thermal zone data */
	for (i = 0; i < g_img_max; i++) {
		sprintf(g_tsData[i].thz_name, "tzimgs%d", i);
		/* Assign a default thermal policy */
		g_tsData[i].num_trip = 1;
		g_tsData[i].trip_temp[0] = 130000;
		sprintf(g_tsData[i].bind[0], "tzimgs%d-sysrst", i);
		g_tsData[i].interval = 1000;
	}

	/* Initialize cooler data */
	for (i = 0; i < g_img_max; i++) {
		sprintf(g_clData[i].cl_name, "tzimgs%d-sysrst", i);
		g_clData[i].sysrst_state = 0;
	}

	/* Create proc nodes */
	for (i = 0; i < g_img_max; i++) {
		entry = proc_create(g_tsData[i].thz_name, 0664, mtk_imgs_dir,
								thz_fops[i]);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	entry = proc_create("tzimgs_debug", 0644, mtk_imgs_dir,
							&mtk_imgs_log_fops);

	/* Register coolers */
	for (i = 0; i < g_img_max; i++) {
		g_clData[i].cl_dev = mtk_thermal_cooling_device_register(
						g_clData[i].cl_name, NULL,
						&mtk_imgs_cooling_sysrst_ops);
	}

	/* Register thermal zones */
	for (i = 0; i < g_img_max; i++) {
		g_tsData[i].thz_dev = mtk_thermal_zone_device_register(
						g_tsData[i].thz_name,
						g_tsData[i].num_trip, NULL,
						&mtk_imgs_dev_ops, 0, 0, 0,
						g_tsData[i].interval);
	}

	/* Register timers */
	mtkTTimer_register("mtk_imgs", mtkts_allts_start_timer,
						mtkts_allts_cancel_timer);
	return 0;
}

static void __exit mtk_imgs_exit(void)
{
	int i;

	mtk_imgs_printk("[%s]\n", __func__);
	/* Unregister thermal zones */
	for (i = 0; i < g_img_max; i++) {
		if (g_tsData[i].thz_dev) {
			mtk_thermal_zone_device_unregister(g_tsData[i].thz_dev);
			g_tsData[i].thz_dev = NULL;
		}
	}

	/* Unregister coolers */
	for (i = 0; i < g_img_max; i++) {
		if (g_clData[i].cl_dev) {
			mtk_thermal_cooling_device_unregister(
							g_clData[i].cl_dev);

			g_clData[i].cl_dev = NULL;
		}
	}

	/* Unregister timers */
	mtkTTimer_unregister("mtk_imgs");
}
module_init(mtk_imgs_init);
module_exit(mtk_imgs_exit);
