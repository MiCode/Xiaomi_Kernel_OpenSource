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
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mt_dramc.h"
#include <linux/uidgid.h>
#include <linux/slab.h>

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);
/*
 *The thermal policy is meaningless here.
 *We only use it to make this thermal zone work.
 */
static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 125000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktsdram_debug_log;

static int kernelmode;

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;

static char g_bind0[20] = "mtktsdram-sysrst";
static char g_bind1[20] = "cpu01";
static char g_bind2[20] = "";
static char g_bind3[20] = "";
static char g_bind4[20] = "";
static char g_bind5[20] = "";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";

#define mtktsdram_TEMP_CRIT 150000	/* 150.000 degree Celsius */

#define mtktsdram_dprintk(fmt, args...)			\
do {								\
	if (mtktsdram_debug_log)					\
		pr_debug("[Thermal/TZ/DRAM]" fmt, ##args);	\
} while (0)

static int mtktsdram_get_temp(
struct thermal_zone_device *thermal, unsigned long *t)
{
	unsigned char t1, t2;
/*
 *The value getting from the read_dram_temperature api is only from 0 to 7.
 *000B: SDRAM Low temperature operating limit exceeded
 *001B: 4x tREFI, 4x tREFIpb, 4x tREFW
 *010B: 2x tREFI, 2x tREFIpb, 2x tREFW
 *011B: 1x tREFI, 1x tREFIpb, 1x tREFW (<=85°C)
 *100B: 0.5x tREFI, 0.5x tREFIpb, 0.5x tREFW, do not de-rate SDRAM AC timing
 *101B: 0.25x tREFI, 0.25x tREFIpb, 0.25x tREFW, do not de-rate SDRAM AC timing
 *110B: 0.25x tREFI, 0.25x tREFIpb, 0.25x tREFW, de-rate SDRAM
 */
	mtktsdram_dprintk("[%s]\n", __func__);
	t1 = read_dram_temperature(CHANNEL_A);
	t2 = read_dram_temperature(CHANNEL_B);

	*t = (t1 > t2) ? t1 : t2;

	mtktsdram_dprintk("temp =%lu\n", *t);
	return 0;
}

static int mtktsdram_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0))
		table_val = 0;
	else if (!strcmp(cdev->type, g_bind1))
		table_val = 1;
	else if (!strcmp(cdev->type, g_bind2))
		table_val = 2;
	else if (!strcmp(cdev->type, g_bind3))
		table_val = 3;
	else if (!strcmp(cdev->type, g_bind4))
		table_val = 4;
	else if (!strcmp(cdev->type, g_bind5))
		table_val = 5;
	else if (!strcmp(cdev->type, g_bind6))
		table_val = 6;
	else if (!strcmp(cdev->type, g_bind7))
		table_val = 7;
	else if (!strcmp(cdev->type, g_bind8))
		table_val = 8;
	else if (!strcmp(cdev->type, g_bind9))
		table_val = 9;
	else
		return 0;

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktsdram_dprintk(
			"[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktsdram_dprintk("[%s] binding OK, %d\n", __func__, table_val);
	return 0;
}

static int mtktsdram_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0))
		table_val = 0;
	else if (!strcmp(cdev->type, g_bind1))
		table_val = 1;
	else if (!strcmp(cdev->type, g_bind2))
		table_val = 2;
	else if (!strcmp(cdev->type, g_bind3))
		table_val = 3;
	else if (!strcmp(cdev->type, g_bind4))
		table_val = 4;
	else if (!strcmp(cdev->type, g_bind5))
		table_val = 5;
	else if (!strcmp(cdev->type, g_bind6))
		table_val = 6;
	else if (!strcmp(cdev->type, g_bind7))
		table_val = 7;
	else if (!strcmp(cdev->type, g_bind8))
		table_val = 8;
	else if (!strcmp(cdev->type, g_bind9))
		table_val = 9;
	else
		return 0;

	//if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
	//	mtktsdram_dprintk(
	//		"[%s] error unbinding cooling dev\n", __func__);
	//	return -EINVAL;
	//}

	mtktsdram_dprintk("[%s] unbinding OK\n", __func__);
	return 0;
}

static int mtktsdram_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktsdram_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktsdram_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktsdram_get_trip_temp(
struct thermal_zone_device *thermal, int trip, unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktsdram_get_crit_temp(
struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtktsdram_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktsdram_dev_ops = {
	.bind = mtktsdram_bind,
	.unbind = mtktsdram_unbind,
	.get_temp = mtktsdram_get_temp,
	.get_mode = mtktsdram_get_mode,
	.set_mode = mtktsdram_set_mode,
	.get_trip_type = mtktsdram_get_trip_type,
	.get_trip_temp = mtktsdram_get_trip_temp,
	.get_crit_temp = mtktsdram_get_crit_temp,
};

static int mtktsdram_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktsdram_dprintk("%s!!!\n", __func__);
	*state = 1;
	return 0;
}

static int mtktsdram_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktsdram_dprintk("%s = %d\n", __func__,
						cl_dev_sysrst_state);
	*state = cl_dev_sysrst_state;
	return 0;
}

static int mtktsdram_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	mtktsdram_dprintk("%s = %d\n", __func__,
						cl_dev_sysrst_state);
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_debug("Power/dram_Thermal: reset, reset, reset!!!");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_debug("*****************************************");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG_ON(1);

	}
	return 0;
}

static struct thermal_cooling_device_ops mtktsdram_cooling_sysrst_ops = {
	.get_max_state = mtktsdram_sysrst_get_max_state,
	.get_cur_state = mtktsdram_sysrst_get_cur_state,
	.set_cur_state = mtktsdram_sysrst_set_cur_state,
};

int mtktsdram_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktsdram-sysrst",
						NULL,
						&mtktsdram_cooling_sysrst_ops);
	return 0;
}

static int mtktsdram_read(struct seq_file *m, void *v)
{

	seq_printf(m,
		"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n",
		__func__,
		trip_temp[0], trip_temp[1], trip_temp[2],
		trip_temp[3], trip_temp[4]);

	seq_printf(m,
		"trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[5], trip_temp[6], trip_temp[7],
		trip_temp[8], trip_temp[9]);

	seq_printf(m,
		"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

	seq_printf(m,
		"g_THERMAL_TRIP_4=%d, g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
		g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
					g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

	seq_printf(m,
		"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

	seq_printf(m,
		"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtktsdram_register_thermal(void);
static void mtktsdram_unregister_thermal(void);

static ssize_t mtktsdram_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i;
	struct mtktsdram_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktsdram_data *ptr_mtktsdram_data = kmalloc(
				sizeof(*ptr_mtktsdram_data), GFP_KERNEL);

	if (ptr_mtktsdram_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktsdram_data->desc) - 1)) ?
				count : (sizeof(ptr_mtktsdram_data->desc) - 1);

	if (copy_from_user(ptr_mtktsdram_data->desc, buffer, len)) {
		kfree(ptr_mtktsdram_data);
		return 0;
	}

	ptr_mtktsdram_data->desc[len] = '\0';

	if (sscanf(ptr_mtktsdram_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktsdram_data->trip[0], &ptr_mtktsdram_data->t_type[0],
		ptr_mtktsdram_data->bind0,
		&ptr_mtktsdram_data->trip[1], &ptr_mtktsdram_data->t_type[1],
		ptr_mtktsdram_data->bind1,
		&ptr_mtktsdram_data->trip[2], &ptr_mtktsdram_data->t_type[2],
		ptr_mtktsdram_data->bind2,
		&ptr_mtktsdram_data->trip[3], &ptr_mtktsdram_data->t_type[3],
		ptr_mtktsdram_data->bind3,
		&ptr_mtktsdram_data->trip[4], &ptr_mtktsdram_data->t_type[4],
		ptr_mtktsdram_data->bind4,
		&ptr_mtktsdram_data->trip[5], &ptr_mtktsdram_data->t_type[5],
		ptr_mtktsdram_data->bind5,
		&ptr_mtktsdram_data->trip[6], &ptr_mtktsdram_data->t_type[6],
		ptr_mtktsdram_data->bind6,
		&ptr_mtktsdram_data->trip[7], &ptr_mtktsdram_data->t_type[7],
		ptr_mtktsdram_data->bind7,
		&ptr_mtktsdram_data->trip[8], &ptr_mtktsdram_data->t_type[8],
		ptr_mtktsdram_data->bind8,
		&ptr_mtktsdram_data->trip[9], &ptr_mtktsdram_data->t_type[9],
		ptr_mtktsdram_data->bind9,
		&ptr_mtktsdram_data->time_msec) == 32) {

		mtktsdram_dprintk(
			"[%s] mtktsdram_unregister_thermal\n", __func__);

		down(&sem_mutex);
		mtktsdram_unregister_thermal();

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktsdram_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktsdram_data->bind0[i];
			g_bind1[i] = ptr_mtktsdram_data->bind1[i];
			g_bind2[i] = ptr_mtktsdram_data->bind2[i];
			g_bind3[i] = ptr_mtktsdram_data->bind3[i];
			g_bind4[i] = ptr_mtktsdram_data->bind4[i];
			g_bind5[i] = ptr_mtktsdram_data->bind5[i];
			g_bind6[i] = ptr_mtktsdram_data->bind6[i];
			g_bind7[i] = ptr_mtktsdram_data->bind7[i];
			g_bind8[i] = ptr_mtktsdram_data->bind8[i];
			g_bind9[i] = ptr_mtktsdram_data->bind9[i];
		}

		mtktsdram_dprintk(
			"[%s] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtktsdram_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtktsdram_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtktsdram_dprintk(
			"[%s] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		mtktsdram_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktsdram_data->trip[i];

		interval = ptr_mtktsdram_data->time_msec / 1000;

		mtktsdram_dprintk(
			"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtktsdram_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtktsdram_dprintk("trip_9_temp=%d,time_ms=%d\n",
						trip_temp[9], interval * 1000);

		mtktsdram_dprintk(
			"[%s] mtktsdram_register_thermal\n", __func__);

		mtktsdram_register_thermal();
		up(&sem_mutex);

		kfree(ptr_mtktsdram_data);
		return count;
	}

	mtktsdram_dprintk("[%s] bad argument\n", __func__);
	kfree(ptr_mtktsdram_data);
	return -EINVAL;
}

static int mtktsdram_register_thermal(void)
{
	mtktsdram_dprintk("[%s]\n", __func__);

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktsdram", num_trip, NULL,
				&mtktsdram_dev_ops, 0, 0, 0, interval * 1000);

	return 0;
}

void mtktsdram_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktsdram_unregister_thermal(void)
{
	mtktsdram_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktsdram_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktsdram_read, NULL);
}

static const struct file_operations mtktsdram_fops = {
	.owner = THIS_MODULE,
	.open = mtktsdram_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktsdram_write,
	.release = single_release,
};

static int __init mtktsdram_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktsdram_dir = NULL;

	mtktsdram_dprintk("%s: Start\n", __func__);

	err = mtktsdram_register_cooler();
	if (err)
		return err;

	err = mtktsdram_register_thermal();
	if (err)
		goto err_unreg;

	mtktsdram_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktsdram_dir) {
		mtktsdram_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzdram", 0664, mtktsdram_dir,
							&mtktsdram_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;

err_unreg:

	mtktsdram_unregister_cooler();

	return err;
}

static void __exit mtktsdram_exit(void)
{
	mtktsdram_dprintk("[%s]\n", __func__);
	mtktsdram_unregister_thermal();
	mtktsdram_unregister_cooler();
}
late_initcall(mtktsdram_init);
module_exit(mtktsdram_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");

