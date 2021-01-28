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
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/power_supply.h>

#define mtktscharger2_TEMP_CRIT (150000) /* 150.000 degree Celsius */

#define mtktscharger2_dprintk(fmt, args...) \
do { \
	if (mtktscharger2_debug_log) \
		pr_notice("[Thermal/tzcharger2]" fmt, ##args); \
} while (0)

#define mtktscharger2_dprintk_always(fmt, args...) \
	pr_notice("[Thermal/tzcharger2]" fmt, ##args)

#define mtktscharger2_pr_notice(fmt, args...) \
	pr_notice("[Thermal/tzcharger2]" fmt, ##args)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static int kernelmode;
static unsigned int interval; /* seconds, 0 : no auto polling */
static int num_trip = 1;
static int trip_temp[10] = { 125000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static char g_bind0[20] = "mtktscharger2-rst";
static char g_bind1[20] = "";
static char g_bind2[20] = "";
static char g_bind3[20] = "";
static char g_bind4[20] = "";
static char g_bind5[20] = "";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";
static char *g_bind_a[10] = {&g_bind0[0], &g_bind1[0], &g_bind2[0]
	, &g_bind3[0], &g_bind4[0], &g_bind5[0], &g_bind6[0], &g_bind7[0]
	, &g_bind8[0], &g_bind9[0]};
static struct thermal_zone_device *thz_dev;

static unsigned int cl_dev_sysrst_state;
static struct thermal_cooling_device *cl_dev_sysrst;

static int mtktscharger2_debug_log;

/* This is to preserve last temperature readings from charger driver.
 * In case mtk_ts_charger.c fails to read temperature.
 */
static unsigned long prev_temp = 30000;


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
struct charger_consumer __attribute__ ((weak))
*charger_manager_get_by_name(struct device *dev,
	const char *supply_name)
{
	mtktscharger2_dprintk_always("%s not found.\n", __func__);
	return NULL;
}

int __attribute__ ((weak))
charger_manager_get_charger_temperature(struct charger_consumer *consumer,
	int idx, int *tchg_min, int *tchg_max)
{
	mtktscharger2_dprintk_always("%s not found.\n", __func__);
	return -ENODEV;
}
static struct power_supply *get_charger2_handle(void)
{
	static struct power_supply *chg_psy_slave;
	static struct power_supply *chg_psy_master;

	/*check if support slave charger*/
	if (chg_psy_slave == NULL)
		chg_psy_slave = power_supply_get_by_name("mtk-slave-charger");
	if (chg_psy_master == NULL)
		chg_psy_master = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy_slave && chg_psy_master)
		return chg_psy_master;

	pr_notice("%s is not dual charger project\n",
				__func__);
	return NULL;
}

#define MAIN_CHARGER 0
/**
 * Use new GM30 API to get charger temperatures.
 * When nothing is defined, main charger temperature is read.
 * When PEP30 is defined, direct charger temperature is read.
 * When Dual Charging is defined, slave charger temperature is read.
 * If main charger is not PMIC, it is necessary to create another TZ
 * for main charger
 * in both PEP30 and dual charging cases.
 */
static int mtktscharger2_get_hw_temp(void)
{
	int ret = -1;
	int t = -127000;
	union power_supply_propval prop;
	struct power_supply *chg_psy;


	chg_psy = get_charger2_handle();
	if (chg_psy == NULL)
		return t;
	ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_TEMP, &prop);
	if (ret == 0) {
		t = 1000 * prop.intval;
		prev_temp = t;
	} else
		t = prev_temp;

	mtktscharger2_dprintk_always("%s t=%d ret=%d\n", __func__,
							t, ret);

	return t;
}

static int mtktscharger2_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtktscharger2_get_hw_temp();

	mtktscharger2_dprintk("%s %d\n", __func__, *t);

	if (*t >= 85000)
		mtktscharger2_dprintk_always("HT %d\n", *t);

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtktscharger2_bind(
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
		mtktscharger2_dprintk("%s error binding %s\n", __func__,
								cdev->type);
		return -EINVAL;
	}

	mtktscharger2_dprintk("%s binding %s at %d\n", __func__, cdev->type,
								table_val);
	return 0;
}

static int mtktscharger2_unbind(struct thermal_zone_device *thermal,
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

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktscharger2_dprintk("%s error unbinding %s\n", __func__,
								cdev->type);
		return -EINVAL;
	}

	mtktscharger2_dprintk("%s unbinding OK\n", __func__);
	return 0;
}

static int mtktscharger2_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktscharger2_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktscharger2_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktscharger2_get_trip_temp(
struct thermal_zone_device *thermal, int trip,
	int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktscharger2_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = mtktscharger2_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktscharger2_dev_ops = {
	.bind = mtktscharger2_bind,
	.unbind = mtktscharger2_unbind,
	.get_temp = mtktscharger2_get_temp,
	.get_mode = mtktscharger2_get_mode,
	.set_mode = mtktscharger2_set_mode,
	.get_trip_type = mtktscharger2_get_trip_type,
	.get_trip_temp = mtktscharger2_get_trip_temp,
	.get_crit_temp = mtktscharger2_get_crit_temp,
};

static int mtktscharger2_register_thermal(void)
{
	mtktscharger2_dprintk("%s\n", __func__);

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktscharger2", num_trip,
				NULL, /* name: mtktscharger2 ??? */
				&mtktscharger2_dev_ops, 0, 0, 0,
				interval * 1000);

	return 0;
}

static void mtktscharger2_unregister_thermal(void)
{
	mtktscharger2_dprintk("%s\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktscharger2_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int mtktscharger2_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int mtktscharger2_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		mtktscharger2_pr_notice(
			"[Thermal/mtktscharger2_sysrst] reset, reset, reset!!!\n");


		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();
	}

	return 0;
}

static struct thermal_cooling_device_ops mtktscharger2_cooling_sysrst_ops = {
	.get_max_state = mtktscharger2_sysrst_get_max_state,
	.get_cur_state = mtktscharger2_sysrst_get_cur_state,
	.set_cur_state = mtktscharger2_sysrst_set_cur_state,
};

int mtktscharger2_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktscharger2-rst",
				NULL, &mtktscharger2_cooling_sysrst_ops);
	return 0;
}

void mtktscharger2_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static int mtktscharger2_read(struct seq_file *m, void *v)
{
	seq_printf(m, "log=%d\n", mtktscharger2_debug_log);
	seq_printf(m, "polling delay=%d\n", interval * 1000);
	seq_printf(m, "no of trips=%d\n", num_trip);
	{
		int i = 0;

		for (; i < 10; i++)
			seq_printf(m, "%02d\t%d\t%d\t%s\n"
				, i, trip_temp[i], g_THERMAL_TRIP[i],
				g_bind_a[i]);
	}

	return 0;
}

static ssize_t mtktscharger2_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i;
	struct mtktscharger2_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktscharger2_data *ptr_mtktscharger2_data = kmalloc(
				sizeof(*ptr_mtktscharger2_data), GFP_KERNEL);

	if (ptr_mtktscharger2_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktscharger2_data->desc) - 1)) ?
			count : (sizeof(ptr_mtktscharger2_data->desc) - 1);

	if (copy_from_user(ptr_mtktscharger2_data->desc, buffer, len)) {
		kfree(ptr_mtktscharger2_data);
		return 0;
	}

	ptr_mtktscharger2_data->desc[len] = '\0';

	/* TODO: Add a switch of mtktscharger2_debug_log. */

	if (sscanf(ptr_mtktscharger2_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktscharger2_data->trip[0],
		&ptr_mtktscharger2_data->t_type[0],
		ptr_mtktscharger2_data->bind0,
		&ptr_mtktscharger2_data->trip[1],
		&ptr_mtktscharger2_data->t_type[1],
		ptr_mtktscharger2_data->bind1,
		&ptr_mtktscharger2_data->trip[2],
		&ptr_mtktscharger2_data->t_type[2],
		ptr_mtktscharger2_data->bind2,
		&ptr_mtktscharger2_data->trip[3],
		&ptr_mtktscharger2_data->t_type[3],
		ptr_mtktscharger2_data->bind3,
		&ptr_mtktscharger2_data->trip[4],
		&ptr_mtktscharger2_data->t_type[4],
		ptr_mtktscharger2_data->bind4,
		&ptr_mtktscharger2_data->trip[5],
		&ptr_mtktscharger2_data->t_type[5],
		ptr_mtktscharger2_data->bind5,
		&ptr_mtktscharger2_data->trip[6],
		&ptr_mtktscharger2_data->t_type[6],
		ptr_mtktscharger2_data->bind6,
		&ptr_mtktscharger2_data->trip[7],
		&ptr_mtktscharger2_data->t_type[7],
		ptr_mtktscharger2_data->bind7,
		&ptr_mtktscharger2_data->trip[8],
		&ptr_mtktscharger2_data->t_type[8],
		ptr_mtktscharger2_data->bind8,
		&ptr_mtktscharger2_data->trip[9],
		&ptr_mtktscharger2_data->t_type[9],
		ptr_mtktscharger2_data->bind9,
		&ptr_mtktscharger2_data->time_msec) == 32) {
		down(&sem_mutex);
		mtktscharger2_dprintk("mtktscharger2_unregister_thermal\n");
		mtktscharger2_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			mtktscharger2_dprintk_always("%s bad argument\n",
								__func__);

			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtktscharger2_write",
					"Bad argument");
			kfree(ptr_mtktscharger2_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktscharger2_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktscharger2_data->bind0[i];
			g_bind1[i] = ptr_mtktscharger2_data->bind1[i];
			g_bind2[i] = ptr_mtktscharger2_data->bind2[i];
			g_bind3[i] = ptr_mtktscharger2_data->bind3[i];
			g_bind4[i] = ptr_mtktscharger2_data->bind4[i];
			g_bind5[i] = ptr_mtktscharger2_data->bind5[i];
			g_bind6[i] = ptr_mtktscharger2_data->bind6[i];
			g_bind7[i] = ptr_mtktscharger2_data->bind7[i];
			g_bind8[i] = ptr_mtktscharger2_data->bind8[i];
			g_bind9[i] = ptr_mtktscharger2_data->bind9[i];
		}

		mtktscharger2_dprintk("%s g_THERMAL_TRIP_0=%d,", __func__,
							g_THERMAL_TRIP[0]);

		mtktscharger2_dprintk(
			"g_THERMAL_TRIP_1=%d g_THERMAL_TRIP_2=%d ",
			g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);

		mtktscharger2_dprintk(
			"g_THERMAL_TRIP_3=%d g_THERMAL_TRIP_4=%d ",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4]);

		mtktscharger2_dprintk(
			"g_THERMAL_TRIP_5=%d g_THERMAL_TRIP_6=%d ",
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtktscharger2_dprintk(
			"g_THERMAL_TRIP_7=%d g_THERMAL_TRIP_8=%d g_THERMAL_TRIP_9=%d\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtktscharger2_dprintk("cooldev0=%s cooldev1=%s cooldev2=%s ",
			g_bind0, g_bind1, g_bind2);

		mtktscharger2_dprintk("cooldev3=%s cooldev4=%s ",
			g_bind3, g_bind4);

		mtktscharger2_dprintk(
			"cooldev5=%s cooldev6=%s cooldev7=%s cooldev8=%s cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktscharger2_data->trip[i];

		interval = ptr_mtktscharger2_data->time_msec / 1000;

		mtktscharger2_dprintk("%s trip_0_temp=%d trip_1_temp=%d ",
			__func__, trip_temp[0], trip_temp[1]);

		mtktscharger2_dprintk("trip_2_temp=%d trip_3_temp=%d ",
			trip_temp[2], trip_temp[3]);

		mtktscharger2_dprintk(
			"trip_4_temp=%d trip_5_temp=%d trip_6_temp=%d ",
			trip_temp[4], trip_temp[5], trip_temp[6]);

		mtktscharger2_dprintk("trip_7_temp=%d trip_8_temp=%d ",
			trip_temp[7], trip_temp[8]);

		mtktscharger2_dprintk("trip_9_temp=%d time_ms=%d\n",
			trip_temp[9], interval * 1000);

		mtktscharger2_dprintk("mtktscharger_register_thermal\n");
		mtktscharger2_register_thermal();
		up(&sem_mutex);

		kfree(ptr_mtktscharger2_data);
		return count;
	}

	mtktscharger2_dprintk("%s bad argument\n", __func__);
	kfree(ptr_mtktscharger2_data);

	return -EINVAL;
}

static int mtktscharger2_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscharger2_read, NULL);
}

static const struct file_operations mtktscharger2_fops = {
	.owner = THIS_MODULE,
	.open = mtktscharger2_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktscharger2_write,
	.release = single_release,
};

static int mtktscharger2_pdrv_probe(struct platform_device *pdev)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscharger2_dir = NULL;

	mtktscharger2_dprintk_always("%s\n", __func__);
	err = mtktscharger2_register_thermal();
	if (err)
		goto err_unreg;

	mtktscharger2_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscharger2_dir) {
		mtktscharger2_pr_notice("%s get /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzcharger2", 0664, mtktscharger2_dir,
							&mtktscharger2_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;

err_unreg:
	mtktscharger2_unregister_cooler();

	return 0;
}

static int mtktscharger2_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_device mtktscharger2_device = {
	.name = "mtktscharger2",
	.id = -1,
};

static struct platform_driver mtktscharger2_driver = {
	.probe = mtktscharger2_pdrv_probe,
	.remove = mtktscharger2_pdrv_remove,
	.driver = {
		   .name = "mtktscharger2",
		   .owner  = THIS_MODULE,
		   },
};

static int __init mtktscharger2_init(void)
{
	int err = 0;
	/* Move this segment to probe function
	 * in case mtktscharger2 reads temperature
	 * before mtk_charger allows it.
	 */


	err = mtktscharger2_register_cooler();
	if (err)
		return err;

	/* Move this segment to probe function
	 * in case mtktscharger2 reads temperature
	 * before mtk_charger allows it.
	 */


	/* TODO: consider not to register a charger thermal zone
	 * if not PEP30 or dual charger.
	 */
	/* register platform device/driver
	 */
	err = platform_device_register(&mtktscharger2_device);
	if (err) {
		mtktscharger2_dprintk("%s fail to reg device\n", __func__);
		goto err_unreg;
	}

	err = platform_driver_register(&mtktscharger2_driver);
	if (err) {
		mtktscharger2_dprintk("%s fail to reg driver\n", __func__);
		goto reg_platform_driver_fail;
	}


	return 0;

reg_platform_driver_fail:
	platform_device_unregister(&mtktscharger2_device);


err_unreg:

	mtktscharger2_unregister_cooler();

	return err;
}

static void __exit mtktscharger2_exit(void)
{
	mtktscharger2_dprintk("%s\n", __func__);
	mtktscharger2_unregister_thermal();
	mtktscharger2_unregister_cooler();
}

late_initcall(mtktscharger2_init);
module_exit(mtktscharger2_exit);
