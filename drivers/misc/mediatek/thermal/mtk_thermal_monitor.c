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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/bug.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <mt-plat/mtk_thermal_monitor.h>
#include <mt-plat/mtk_thermal_platform.h>
#include <linux/uidgid.h>

/* ************************************ */
/* Definition */
/* ************************************ */

/**
 * \def MTK_THERMAL_MONITOR_MEASURE_GET_TEMP_OVERHEAD
 * 1 to enable
 * 0 to disable
 */
#define MTK_THERMAL_MONITOR_MEASURE_GET_TEMP_OVERHEAD (0)

#define MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS (3)

#define MTK_THERMAL_MONITOR_CONDITIONAL_COOLING (1)

/**
 *  \def MTK_MAX_STEP_SMA_LEN
 *  If not defined as 1, multi-step temperature SMA len is supported.
 *  For example, MTK_MAX_STEP_SMA_LEN is defined as 4.
 *  Users can set 4 different SMA len for a thermal zone and assign
 *  a high threshold for each.
 *  SMA len in the next step is applied if temp of the TZ reaches high
 *  threshold.
 *  Represent this in a simple figure as below:
 *   -infinite HT(0)|<- sma_len(0) ->|HT(1)|<- sma_len(1) ->|HT(2)|<- sma_len(2)
 *   ->|HT(3)|<- sma_len(3)-> |+infinite HT(4)
 *  In temp range between HT(i) and HT(i+1), sma_len(i) is applied.
 *  HT(i) < HT(i+1), eq is not allowed since meaningless
 *  sma_len(i) in [1, 60]
 */
#define MAX_STEP_MA_LEN (4)

#define MSMA_MAX_HT     (1000000)
#define MSMA_MIN_HT     (-275000)

struct mtk_thermal_cooler_data {
	struct thermal_zone_device *tz;
	struct thermal_cooling_device_ops *ops;
	struct thermal_cooling_device_ops_extra *ops_ext;
	void *devdata;
	int trip;

	char conditions
		[MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS]
		[THERMAL_NAME_LENGTH];

	int *condition_last_value
		[MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS];

	int threshold[MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS];
	int exit_threshold;
	int id;
};

struct mtk_thermal_tz_data {
	struct thermal_zone_device_ops *ops;
	unsigned int ma_len;	/* max 60 */
	unsigned int ma_counter;
	long ma[60];
#if (MAX_STEP_MA_LEN > 1)
	unsigned int curr_idx_ma_len;
	unsigned int ma_lens[MAX_STEP_MA_LEN];
	long msma_ht[MAX_STEP_MA_LEN];
	/**< multi-step moving avg. high threshold array. */
#endif
	long fake_temp;
	/* to store the Tfake, range from -275000 to MAX positive of int...
	 *-275000 is a special number to turn off Tfake
	 */
	struct mutex ma_lock;	/* protect moving avg. vars... */
};

struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void);

static DEFINE_MUTEX(MTM_GET_TEMP_LOCK);
static int *tz_last_values[MTK_THERMAL_SENSOR_COUNT] = { NULL };

/* ************************************ */
/* Global Variable */
/* ************************************ */
struct thermal_zone_device_ops *g_SysinfoAttachOps;
static bool enable_ThermalMonitor;
static bool enable_ThermalMonitorXlog;
static int g_nStartRealTime;

/* lock by MTM_COOLER_PROC_DIR_LOCK */
static struct proc_dir_entry *proc_cooler_dir_entry;

/* lock by MTK_TZ_PROC_DIR_LOCK */
static struct proc_dir_entry *proc_tz_dir_entry;

static struct proc_dir_entry *proc_drv_therm_dir_entry;
/**
 *  write to nBattCurrentCnsmpt, nCPU0_usage,
 *  and nCPU1_usage are locked by MTM_SYSINFO_LOCK
 */



/* static int nModem_TxPower = -127;  ///< Indicate invalid value */

/* For enabling time based thermal protection
 * under phone call+AP suspend scenario.
 */
static int g_mtm_phone_call_ongoing;

static DEFINE_MUTEX(MTM_COOLER_LOCK);
static DEFINE_MUTEX(MTM_SYSINFO_LOCK);
static DEFINE_MUTEX(MTM_COOLER_PROC_DIR_LOCK);
static DEFINE_MUTEX(MTM_TZ_PROC_DIR_LOCK);
static DEFINE_MUTEX(MTM_DRV_THERM_PROC_DIR_LOCK);

static struct delayed_work _mtm_sysinfo_poll_queue;
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

/* ************************************ */
/* Macro */
/* ************************************ */
#ifdef CONFIG_MTK_MT_LOGGER
#define THRML_STORAGE_LOG(msg_id, func_name, ...) \
	do { \
		if (unlikely(is_dump_mthermal()) && enable_ThermalMonitor) { \
			AddThrmlTrace(msg_id, func_name, __VA_ARGS__); \
		} \
	} while (0)
#else
#define THRML_STORAGE_LOG(msg_id, func_name, ...)
#endif


#define THRML_LOG(fmt, args...) \
	do { \
		if (unlikely(enable_ThermalMonitorXlog)) { \
			pr_notice("THERMAL/MONITOR " fmt, ##args); \
		} \
	} while (0)


#define THRML_ERROR_LOG(fmt, args...) pr_notice("THERMAL/MONITOR " fmt, ##args)

/* ************************************ */
/* Define */
/* ************************************ */
/* thermal_zone_device * sysinfo_monitor_register(int nPollingTime); */
/* int sysinfo_monitor_unregister(void); */
#define SYSINFO_ATTACH_DEV_NAME "mtktscpu"

/* ************************************ */
/* Thermal Monitor API */
/* ************************************ */
#if MTK_THERMAL_MONITOR_MEASURE_GET_TEMP_OVERHEAD
static long int _get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}
#endif

static int mtk_thermal_get_tz_idx(char *type)
{
	if (strncmp(type, "mtktscpu", 8) == 0)
		return MTK_THERMAL_SENSOR_CPU;
	else if (strncmp(type, "mtktsabb", 8) == 0)
		return MTK_THERMAL_SENSOR_ABB;
	else if (strncmp(type, "mtktspmic", 9) == 0)
		return MTK_THERMAL_SENSOR_PMIC;
	else if (strncmp(type, "mtktsbattery2", 13) == 0)
		return MTK_THERMAL_SENSOR_BATTERY2;
	else if (strncmp(type, "mtktsbattery", 12) == 0)
		return MTK_THERMAL_SENSOR_BATTERY;
	else if (strncmp(type, "mtktspa", 7) == 0)
		return MTK_THERMAL_SENSOR_MD1;
	else if (strncmp(type, "mtktstdpa", 9) == 0)
		return MTK_THERMAL_SENSOR_MD2;
	else if (strncmp(type, "mtktswmt", 8) == 0)
		return MTK_THERMAL_SENSOR_WIFI;
	else if (strncmp(type, "mtktsbuck", 9) == 0)
		return MTK_THERMAL_SENSOR_BUCK;
	else if (strncmp(type, "mtktsAP", 7) == 0)
		return MTK_THERMAL_SENSOR_AP;
	else if (strncmp(type, "mtktspcb1", 9) == 0)
		return MTK_THERMAL_SENSOR_PCB1;
	else if (strncmp(type, "mtktspcb2", 9) == 0)
		return MTK_THERMAL_SENSOR_PCB2;
	else if (strncmp(type, "mtktsskin", 9) == 0)
		return MTK_THERMAL_SENSOR_SKIN;
	else if (strncmp(type, "mtktsxtal", 9) == 0)
		return MTK_THERMAL_SENSOR_XTAL;
	else if (strncmp(type, "mtktsbtsmdpa", 12) == 0)
		return MTK_THERMAL_SENSOR_MD_PA;
	else if (strncmp(type, "mtktsbtsnrpa", 12) == 0)
		return MTK_THERMAL_SENSOR_NR_PA;
	else if (strncmp(type, "mtktsdctm", 9) == 0)
		return MTK_THERMAL_SENSOR_DCTM;
	else if (strncmp(type, "mtktscharger", 12) == 0)
		return MTK_THERMAL_SENSOR_CHARGER;

	return -1;
}

static struct proc_dir_entry *_get_proc_cooler_dir_entry(void)
{
	mutex_lock(&MTM_COOLER_PROC_DIR_LOCK);
	if (proc_cooler_dir_entry == NULL) {
		proc_cooler_dir_entry = proc_mkdir("mtkcooler", NULL);
		if (proc_cooler_dir_entry == NULL)
			THRML_ERROR_LOG("%s mkdir /proc/mtkcooler failed\n",
								__func__);
	}
	mutex_unlock(&MTM_COOLER_PROC_DIR_LOCK);
	return proc_cooler_dir_entry;
}

static struct proc_dir_entry *_get_proc_tz_dir_entry(void)
{
	mutex_lock(&MTM_TZ_PROC_DIR_LOCK);
	if (proc_tz_dir_entry == NULL) {
		proc_tz_dir_entry = proc_mkdir("mtktz", NULL);
		if (proc_tz_dir_entry == NULL)
			THRML_ERROR_LOG("%s mkdir /proc/mtktz failed\n",
								__func__);
	}
	mutex_unlock(&MTM_TZ_PROC_DIR_LOCK);
	return proc_tz_dir_entry;
}

static struct thermal_cooling_device_ops *recoveryClientCooler
(struct thermal_cooling_device *cdev, struct mtk_thermal_cooler_data **mcdata)
{
	*mcdata = cdev->devdata;
	cdev->devdata = (*mcdata)->devdata;

	return (*mcdata)->ops;
}

/* Lookup List to get Client's Thermal Zone OPS */
static struct thermal_zone_device_ops *getClientZoneOps
(struct thermal_zone_device *zdev)
{
	struct thermal_zone_device_ops *ret = NULL;
	struct mtk_thermal_tz_data *tzdata;

	if ((zdev == NULL) || (zdev->devdata == NULL)) {
		WARN_ON_ONCE(1);
		return NULL;
	}

	tzdata = zdev->devdata;
	mutex_lock(&tzdata->ma_lock);
	ret = tzdata->ops;
	mutex_unlock(&tzdata->ma_lock);
	return ret;
}

#define CPU_USAGE_CURRENT_FIELD (0)
#define CPU_USAGE_SAVE_FIELD    (1)
#define CPU_USAGE_FRAME_FIELD   (2)

struct cpu_index_st {
	unsigned long u[3];
	unsigned long s[3];
	unsigned long n[3];
	unsigned long i[3];
	unsigned long w[3];
	unsigned long q[3];
	unsigned long sq[3];
	unsigned long tot_frme;
	unsigned long tz;
	int usage;
	int freq;
};

struct gpu_index_st {
	int usage;
	int freq;
};

static struct cpu_index_st cpu_index_list[8];	/* /< 8-Core is maximum */


#define SEEK_BUFF(x, c) \
	do { \
		while (*x != c)\
			x++; \
		x++; \
	} while (0)

#define TRIMz_ex(tz, x)   ((tz = (unsigned long long)(x)) < 0 ? 0 : tz)


static int _mtm_interval;

static void _mtm_update_sysinfo(struct work_struct *work)
{

	cancel_delayed_work(&_mtm_sysinfo_poll_queue);

	if (_mtm_interval != 0)
		queue_delayed_work(system_freezable_power_efficient_wq,
					&_mtm_sysinfo_poll_queue,
					msecs_to_jiffies(_mtm_interval));
}

static void _mtm_decide_new_delay(void)
{
	int new_interval = 0;

	if (true == enable_ThermalMonitor)
		new_interval = 1000;

	if (_mtm_interval == 0 && new_interval != 0) {
		_mtm_interval = new_interval;
		_mtm_update_sysinfo(NULL);
	} else {
		_mtm_interval = new_interval;
	}
}

/* ************************************ */
/* Thermal Host Driver Interface */
/* ************************************ */

/* Read */
static int mtkthermal_read(struct seq_file *m, void *v)
{
	seq_puts(m, "\r\n[Thermal Monitor debug flag]\r\n");
	seq_puts(m, "=========================================\r\n");
	seq_printf(m, "enable_ThermalMonitor = %d\r\n", enable_ThermalMonitor);
	seq_printf(m, "enable_ThermalMonitorXlog = %d\r\n",
					enable_ThermalMonitorXlog);

	seq_printf(m, "g_nStartRealTime = %d\r\n", g_nStartRealTime);

	THRML_LOG("%s enable_ThermalMonitor:%d\n", __func__,
					enable_ThermalMonitor);

	return 0;
}

/* Write */
static ssize_t mtkthermal_write
(struct file *file, const char __user *buffer, size_t count,
		loff_t *data)
{
	int len = 0, nCtrlCmd = 0, nReadTime = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &nCtrlCmd, &nReadTime) == 2) {
		/* Bit 0; Enable Thermal Monitor. */
		if ((nCtrlCmd >> 0) & 0x01) {
			/* Reset Global CPU Info Variable */
			memset(&cpu_index_list, 0x00, sizeof(cpu_index_list));

			enable_ThermalMonitor = true;
		} else {
			enable_ThermalMonitor = false;
		}

		_mtm_decide_new_delay();

		/* Bit 1: Enable Thermal Monitor xlog */
		enable_ThermalMonitorXlog =
				((nCtrlCmd >> 1) & 0x01) ? true : false;

		/*
		 * Get Real Time from user input
		 * Format: hhmmss 113901=> 11:39:01
		 */
		g_nStartRealTime = nReadTime;

		THRML_STORAGE_LOG(THRML_LOGGER_MSG_DEC_NUM, get_real_time,
				"[realtime]", g_nStartRealTime);

		THRML_ERROR_LOG(
		"%s nCtrlCmd=%d enable_ThermalMonitor=%d g_nStartRealTime=%d\n",
				__func__, nCtrlCmd, (int)enable_ThermalMonitor,
				g_nStartRealTime);

		return count;
	}

	if (kstrtoint(desc, 10, &nCtrlCmd) == 0) {
		/* Bit 0; Enable Thermal Monitor. */
		if ((nCtrlCmd >> 0) & 0x01) {
			/* Reset Global CPU Info Variable */
			memset(&cpu_index_list, 0x00, sizeof(cpu_index_list));

			enable_ThermalMonitor = true;
		} else {
			enable_ThermalMonitor = false;
		}

		_mtm_decide_new_delay();

		/* Bit 1: Enable Thermal Monitor xlog */
		enable_ThermalMonitorXlog =
				((nCtrlCmd >> 1) & 0x01) ? true : false;

		THRML_ERROR_LOG("%s nCtrlCmd=%d enable_ThermalMonitor=%d\n",
				__func__, nCtrlCmd, (int)enable_ThermalMonitor);

		return count;
	}

	THRML_LOG("%s bad arg\n", __func__);

	return -EINVAL;
}

static int mtkthermal_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkthermal_read, NULL);
}

static const struct file_operations mtkthermal_fops = {
	.owner = THIS_MODULE,
	.open = mtkthermal_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkthermal_write,
	.release = single_release,
};

static int _mtkthermal_check_cooler_conditions
(struct mtk_thermal_cooler_data *cldata)
{
	int ret = 0;

	if (cldata != NULL) {
		int i = 0;

		for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS;
			i++){
			if (cldata->condition_last_value[i] == NULL) {
				if (cldata->conditions[i][0] == 0x0)
					ret++;
				else if (strncmp(
					cldata->conditions[i], "EXIT", 4) == 0)
					ret++;
			} else {
#if 1 /* [FIX ME] Special case for "condifion of MOBILE"  */
				if (strncmp(cldata->conditions[i], "MOBILE", 5)
					== 0)  {
					if (*cldata->condition_last_value[i]
					< cldata->threshold[i]){
						THRML_LOG(
						"%s MOBILE Condition=%s, last_value=%d, threshold=%d\n",
						__func__, cldata->conditions[i],
						(*cldata
						->condition_last_value[i]),
						cldata->threshold[i]);
						ret++;
					}
				} else if (*cldata->condition_last_value[i]
							> cldata->threshold[i])
					ret++;
#else
				if (*cldata->condition_last_value[i]
							> cldata->threshold[i])
					ret++;
#endif
			}
		}
	}

	return ret;
}

static void _mtkthermal_clear_cooler_conditions
(struct mtk_thermal_cooler_data *cldata)
{
	int i = 0;

	cldata->exit_threshold = 0;

	for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS; i++) {
		cldata->conditions[i][0] = 0x0;
		cldata->condition_last_value[i] = NULL;
		cldata->threshold[i] = 0;
	}


	_mtm_decide_new_delay();
}

static int _mtkthermal_is_cooler_conditions_valid(char *condition)
{
	if (condition[0] == 0x0
		|| mtk_thermal_get_tz_idx(condition) >= 0
		|| strncmp(condition, "EXIT", strlen("EXIT")) == 0
		|| strncmp(condition, "CPU0", strlen("CPU0")) == 0
		|| strncmp(condition, "Wifi", strlen("Wifi")) == 0
		|| strncmp(condition, "Mobile", strlen("Mobile")) == 0)
		return 1;

	return 0;
}

static int _mtkthermal_cooler_read(struct seq_file *m, void *v)
{
	struct mtk_thermal_cooler_data *mcdata;

	/**
	 * The format to print out
	 * <condition_name_1> <condition_value_1> <thershold_1> <state_1>
	 * ..
	 * <condition_name_n> <condition_value_n> <thershold_n> <state_n>
	 * PS: n is MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS
	 */
	if (m->private == NULL) {
		THRML_ERROR_LOG("%s null data\n", __func__);
	} else {
		int i = 0;

		/* TODO: we may not need to lock here... */
		mutex_lock(&MTM_COOLER_LOCK);
		mcdata = (struct mtk_thermal_cooler_data *)m->private;
		mutex_unlock(&MTM_COOLER_LOCK);

		for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS;
		i++) {
			if (mcdata->conditions[i][0] == 0x0)
				continue;	/* no condition */

			/* TODO: consider the case that tz is unregistered... */
			seq_printf(m, "%s val=%d threshold=%d %s",
				mcdata->conditions[i],
				(NULL ==
				mcdata->condition_last_value[i]) ? 0 :
				*(mcdata->condition_last_value[i]),
				mcdata->threshold[i],
				(mcdata->condition_last_value[i] == NULL) ?
					"error\n" : "\n");
		}
	}

	return 0;
}

static ssize_t _mtkthermal_cooler_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[128];
	struct mtk_thermal_cooler_data *mcdata;
	char conditions
		[MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS]
		[THERMAL_NAME_LENGTH] = {{0} };
	int threshold[MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS] = {0};

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	/**
	 * sscanf format
	 * <condition_1> <threshold_1> ... <condition_n> <threshold_n>
	 * <condition_i> is string format
	 * <threshold_i> is integer format
	 * n is MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS
	 */

	/* TODO: we may not need to lock here... */
	mutex_lock(&MTM_COOLER_LOCK);
	mcdata = (struct mtk_thermal_cooler_data *)PDE_DATA(file_inode(file));
	mutex_unlock(&MTM_COOLER_LOCK);

	if (mcdata == NULL) {
		THRML_ERROR_LOG("%s null data\n", __func__);
		return -EINVAL;
	}
	/* WARNING: Modify here if
	 * MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS
	 * is changed to other than 3
	 */
#if (MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS == 3)
	if (sscanf(desc, "%19s %d %19s %d %19s %d",
		&conditions[0][0], &threshold[0],
		&conditions[1][0], &threshold[1],
		&conditions[2][0], &threshold[2]) >= 2){
		int i = 0;

		_mtkthermal_clear_cooler_conditions(mcdata);

		for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS;
		i++) {
			if (!_mtkthermal_is_cooler_conditions_valid(
				conditions[i])) {
				/* ignore invalid conditions */
				THRML_ERROR_LOG(
					"%s %s invalid condition: %s\n",
					__func__, mcdata->tz->type,
					conditions[i]);
				continue;
			}

			strncpy(mcdata->conditions[i], conditions[i],
				THERMAL_NAME_LENGTH);
			mcdata->conditions[i][THERMAL_NAME_LENGTH - 1] = '\0';
			mcdata->threshold[i] = threshold[i];

			if (strncmp(mcdata->conditions[i], "EXIT", 4) == 0) {
				mcdata->exit_threshold = mcdata->threshold[i];

			} else {
				/* normal thermal zones */
				mcdata->condition_last_value[i] = NULL;

			}

			THRML_LOG("%s %s %d: %s %d %p %d.\n", __func__,
				mcdata->tz->type, i,
				&mcdata->conditions[i][0],
				mcdata->conditions[i][0],
				mcdata->condition_last_value[i],
				mcdata->threshold[0]);
		}

		_mtm_decide_new_delay();

		return count;
	} else if (conditions[0][0] == 0x0) {
		/* No condition */
		_mtkthermal_clear_cooler_conditions(mcdata);
		THRML_LOG("%s %s No condition!\n",
			__func__, mcdata->tz->type);

		return count;
	}

#else
#error "Change correspondent part when changing MTK_THERMAL_MONITOR_ skip..."
/* Change correspondent part when changing
 * MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS!
 */
#endif
	THRML_ERROR_LOG("%s %s bad arg: %s\n",
		__func__, mcdata->tz->type, desc);

	return -EINVAL;
}

static int _mtkthermal_cooler_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtkthermal_cooler_read, PDE_DATA(inode));
}

static const struct file_operations _mtkthermal_cooler_fops = {
	.owner = THIS_MODULE,
	.open = _mtkthermal_cooler_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtkthermal_cooler_write,
	.release = single_release,
};

static int _mtkthermal_tz_read(struct seq_file *m, void *v)
{
	struct thermal_zone_device *tz = NULL;

	if (m->private == NULL) {
		THRML_ERROR_LOG("%s null data\n", __func__);
	} else {
		tz = (struct thermal_zone_device *)m->private;
		/* TODO: consider the case that tz is unregistered... */
		seq_printf(m, "%d\n", tz->temperature);
		{
			struct mtk_thermal_tz_data *tzdata = NULL;
			int ma_len = 0;
			int fake_temp = 0;

			tzdata = tz->devdata;
			if (!tzdata)
				WARN_ON_ONCE(1);

#if (MAX_STEP_MA_LEN > 1)
			mutex_lock(&tzdata->ma_lock);
			ma_len = tzdata->ma_len;
			fake_temp = tzdata->fake_temp;
			seq_printf(m, "ma_len=%d\n", ma_len);
			seq_printf(m, "%d ", tzdata->ma_lens[0]);
			{
				int i = 1;

				for (; i < MAX_STEP_MA_LEN; i++)
					seq_printf(m, "(%ld,%d) ",
							tzdata->msma_ht[i - 1],
							tzdata->ma_lens[i]);
			}
			mutex_unlock(&tzdata->ma_lock);
			seq_puts(m, "\n");
#else
			mutex_lock(&tzdata->ma_lock);
			ma_len = tzdata->ma_len;
			fake_temp = tzdata->fake_temp;
			mutex_unlock(&tzdata->ma_lock);
			seq_printf(m, "ma_len=%d\n", ma_len);
#endif
			if (-275000 < fake_temp) {
				/* print Tfake only when fake_temp > -275000 */
				seq_printf(m, "Tfake=%d\n", fake_temp);
			}
		}
	}

	return 0;
}

static ssize_t _mtkthermal_tz_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[128];
	char trailing[128] = { 0 };
	int check = 0;
	struct thermal_zone_device *tz;
	char arg_name[32] = { 0 };
	int arg_val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	tz = (struct thermal_zone_device *)PDE_DATA(file_inode(file));

	if (tz == NULL) {
		THRML_ERROR_LOG("%s null data\n", __func__);
		return -EINVAL;
	}

	if (sscanf(desc, "%31s %d %127s", arg_name, &arg_val, trailing) >= 2) {
		if ((strncmp(arg_name, "ma_len", 6) == 0)
			&& (arg_val >= 1) && (arg_val <= 60)) {

			struct mtk_thermal_tz_data *tzdata = NULL;

			tzdata = tz->devdata;
			if (!tzdata)
				WARN_ON_ONCE(1);

			/* THRML_ERROR_LOG(
			 * "%s trailing=%s\n", __func__, trailing);
			 */

			/* reset MA len and lock
			 */
#if (MAX_STEP_MA_LEN > 1)
			mutex_lock(&tzdata->ma_lock);
			tzdata->ma_len = arg_val;
			tzdata->ma_counter = 0;
			tzdata->curr_idx_ma_len = 0;
			tzdata->ma_lens[0] = arg_val;
			tzdata->msma_ht[0] = MSMA_MAX_HT;
			/* THRML_ERROR_LOG(
			 *	"%s %s ma_len=%d.\n", __func__, tz->type,
			 *				tzdata->ma_len);
			 */
#if (MAX_STEP_MA_LEN == 4)
			/* reset */
			tzdata->msma_ht[1] = tzdata->msma_ht[2] =
					tzdata->msma_ht[3] = MSMA_MAX_HT;

			tzdata->ma_lens[1] = tzdata->ma_lens[2] =
					tzdata->ma_lens[3] = 1;

			check = sscanf(trailing, "%ld,%d;%ld,%d;%ld,%d;",
				&tzdata->msma_ht[0], &tzdata->ma_lens[1],
				&tzdata->msma_ht[1], &tzdata->ma_lens[2],
				&tzdata->msma_ht[2], &tzdata->ma_lens[3]);

			/* THRML_ERROR_LOG(
			 * "%s %s (%ld, %d), (%ld, %d), (%ld, %d)\n", __func__,
			 * tz->type, tzdata->msma_ht[0], tzdata->ma_lens[1],
			 * tzdata->msma_ht[1], tzdata->ma_lens[2],
			 * tzdata->msma_ht[2], tzdata->ma_lens[3]);
			 */
#else
#error
#endif
			mutex_unlock(&tzdata->ma_lock);
#else
			mutex_lock(&tzdata->ma_lock);
			tzdata->ma_len = arg_val;
			tzdata->ma_counter = 0;
			mutex_unlock(&tzdata->ma_lock);
			THRML_ERROR_LOG("%s %s ma_len=%d.\n", __func__,
					tz->type, tzdata->ma_len);
#endif
		} else if ((strncmp(arg_name, "Tfake", 5) == 0)
			&& (arg_val >= -275000)) {
			/* only accept for[-275000, max positive value of int]*/
			struct mtk_thermal_tz_data *tzdata = NULL;

			tzdata = tz->devdata;
			if (!tzdata)
				WARN_ON_ONCE(1);

			mutex_lock(&tzdata->ma_lock);
			tzdata->fake_temp = (long)arg_val;
			mutex_unlock(&tzdata->ma_lock);
			THRML_ERROR_LOG("%s %s Tfake=%ld.\n", __func__,
						tz->type, tzdata->fake_temp);
		}

		return count;
	} else {
		return -EINVAL;
	}
}

static int _mtkthermal_tz_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtkthermal_tz_read, PDE_DATA(inode));
}

static const struct file_operations _mtkthermal_tz_fops = {
	.owner = THIS_MODULE,
	.open = _mtkthermal_tz_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtkthermal_tz_write,
	.release = single_release,
};

#define MIN(_a_, _b_) ((_a_) < (_b_) ? (_a_) : (_b_))

/* No parameter check in this internal function */
static long _mtkthermal_update_and_get_sma
(struct mtk_thermal_tz_data *tzdata, long latest_val)
{
	long ret = 0;

	if (tzdata == NULL) {
		WARN_ON_ONCE(1);
		return latest_val;
	}

	mutex_lock(&tzdata->ma_lock);
	/* Use Tfake if set... */
	latest_val =
		(-275000 < tzdata->fake_temp) ? tzdata->fake_temp : latest_val;

	if (tzdata->ma_len == 1) {
		ret = latest_val;
	} else if (tzdata->ma_len > 1) {
		int i = 0;

		tzdata->ma[(tzdata->ma_counter) %
					(tzdata->ma_len)] = latest_val;

		tzdata->ma_counter++;

		for (i = 0; i < MIN(tzdata->ma_counter, tzdata->ma_len); i++)
			ret += tzdata->ma[i];

		ret = ret / ((long)MIN(tzdata->ma_counter, tzdata->ma_len));
	}

#if (MAX_STEP_MA_LEN > 1)

	/*
	 *  2. Move to correct region if ma_counter == 1
	 *      a. For (i=0;SMA >= high_threshold[i];i++) ;
	 *      b. if (curr_idx_sma_len != i) {ma_counter = 0;
	 *		ma_len = sma_len[curr_idx_sma_len = i]; }
	 *  3. Check if need to change region if ma_counter > 1
	 *      a. if SMA >= high_threshold[curr_idx_sma_len]
	 *	{ Move upward: ma_counter = 0;
	 *		ma_len = sma_len[++curr_idx_sma_len]; }
	 *      b. else if curr_idx_sma_len >0
	 *		&& SMA < high_threshold[curr_idx_sma_len-1]
	 *	{ Move downward: ma_counter =0;
	 *		ma_len = sma_len[--curr_idx_sma_len]; }
	 */
	if (tzdata->ma_counter == 1) {
		int i = 0;

		for (; ret >= tzdata->msma_ht[i]; i++)
			;
		if (tzdata->curr_idx_ma_len != i) {
			tzdata->ma_counter = 0;
			tzdata->ma_len =
				tzdata->ma_lens[tzdata->curr_idx_ma_len = i];
			THRML_LOG("%s 2b ma_len: %d curr_idx_ma_len: %d\n",
			__func__, tzdata->ma_len, tzdata->curr_idx_ma_len);
		}
	} else {
		if (ret >= tzdata->msma_ht[tzdata->curr_idx_ma_len]) {
			tzdata->ma_counter = 0;
			tzdata->ma_len =
				tzdata->ma_lens[++(tzdata->curr_idx_ma_len)];
			THRML_LOG("%s 3a ma_len: %d curr_idx_ma_len: %d\n",
			__func__, tzdata->ma_len, tzdata->curr_idx_ma_len);

		} else if (tzdata->curr_idx_ma_len > 0
			&& ret < tzdata->msma_ht[tzdata->curr_idx_ma_len - 1]) {

			tzdata->ma_counter = 0;
			tzdata->ma_len =
				tzdata->ma_lens[--(tzdata->curr_idx_ma_len)];

			THRML_LOG("%s 3b ma_len: %d curr_idx_ma_len: %d\n",
			__func__, tzdata->ma_len, tzdata->curr_idx_ma_len);
		}
	}
#endif

	mutex_unlock(&tzdata->ma_lock);
	return ret;
}

/**
 *  0: means please do not show thermal limit in "Show CPU Usage" panel.
 *  1: means show thermal limit and CPU temp only
 *  2: means show all all tz temp besides thermal limit and CPU temp
 */
static unsigned int g_thermal_indicator_mode;

/**
 *  delay in milliseconds.
 */
static unsigned int g_thermal_indicator_delay;

/* Read */
static int _mtkthermal_indicator_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n%d\n", g_thermal_indicator_mode,
					g_thermal_indicator_delay);

	return 0;
}

/* Write */
static ssize_t _mtkthermal_indicator_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, thermal_indicator_mode = 0, thermal_indicator_delay = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &thermal_indicator_mode,
				&thermal_indicator_delay) == 2) {

		if ((thermal_indicator_mode >= 0)
			&& (thermal_indicator_mode <= 3))
			g_thermal_indicator_mode = thermal_indicator_mode;

		g_thermal_indicator_delay = thermal_indicator_delay;
		return count;
	} else {
		return 0;
	}
}

static int _mtkthermal_indicator_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtkthermal_indicator_read, NULL);
}

static const struct file_operations _mtkthermal_indicator_fops = {
	.owner = THIS_MODULE,
	.open = _mtkthermal_indicator_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtkthermal_indicator_write,
	.release = single_release,
};

/* Read */
static int _mtm_scen_call_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_mtm_phone_call_ongoing);

	return 0;
}

/* Write */
static ssize_t _mtm_scen_call_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, mtm_phone_call_ongoing = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &mtm_phone_call_ongoing) == 0) {
		if ((mtm_phone_call_ongoing == 0)
			|| (mtm_phone_call_ongoing == 1)) {

			g_mtm_phone_call_ongoing = mtm_phone_call_ongoing;

			if (mtm_phone_call_ongoing  == 1)
				mtk_thermal_set_user_scenarios(
						MTK_THERMAL_SCEN_CALL);

			else if (mtm_phone_call_ongoing == 0)
				mtk_thermal_clear_user_scenarios(
						MTK_THERMAL_SCEN_CALL);
		}
		return count;
	}

	return 0;
}

static int _mtm_scen_call_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtm_scen_call_read, NULL);
}

static const struct file_operations _mtm_scen_call_fops = {
	.owner = THIS_MODULE,
	.open = _mtm_scen_call_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtm_scen_call_write,
	.release = single_release,
};


/* Init */
static int __init mtkthermal_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry;
	struct proc_dir_entry *dir_entry =
			mtk_thermal_get_proc_drv_therm_dir_entry();

	THRML_LOG("%s\n", __func__);



	entry = proc_create("mtm_monitor",
		0664, dir_entry, &mtkthermal_fops);
	if (!entry)
		THRML_ERROR_LOG("%s Can not create mtm_monitor\n", __func__);
	else
		proc_set_user(entry, uid, gid);

	entry = proc_create("mtm_indicator",
		0644, dir_entry, &_mtkthermal_indicator_fops);

	if (!entry)
		THRML_ERROR_LOG("%s Can not create mtm_indicator\n", __func__);

	entry = proc_create("mtm_scen_call",
		0664, dir_entry, &_mtm_scen_call_fops);

	if (!entry)
		THRML_ERROR_LOG("%s Can not create mtm_scen_call\n", __func__);
	else
		proc_set_user(entry, uid, gid);

	/* create /proc/cooler folder */
	/* WARNING! This is not gauranteed to be invoked before
	 * mtk_ts_cpu's functions...
	 */
	proc_cooler_dir_entry =
		(proc_cooler_dir_entry == NULL) ?
			proc_mkdir("mtkcooler", NULL) : proc_cooler_dir_entry;

	if (proc_cooler_dir_entry == NULL)
		THRML_ERROR_LOG("%s mkdir /proc/mtkcooler failed\n", __func__);

	/* create /proc/tz folder */
	/* WARNING! This is not gauranteed to be invoked before
	 * mtk_ts_cpu's functions...
	 */
	proc_tz_dir_entry =
		(proc_tz_dir_entry == NULL) ?
			proc_mkdir("mtktz", NULL) : proc_tz_dir_entry;

	if (proc_tz_dir_entry == NULL)
		THRML_ERROR_LOG("%s mkdir /proc/mtktz failed\n", __func__);

	INIT_DELAYED_WORK(&_mtm_sysinfo_poll_queue, _mtm_update_sysinfo);
	_mtm_update_sysinfo(NULL);

	return err;
}

/* Exit */
static void __exit mtkthermal_exit(void)
{
	THRML_LOG("%s\n", __func__);
}

/* ************************************
 * thermal_zone_device_ops Wrapper
 * ************************************
 */

/*
 * .bind wrapper: bind the thermal zone device with a thermal cooling device.
 */
static int mtk_thermal_wrapper_bind
(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	/* WARNING! bind will invoke
	 * mtk_thermal_zone_bind_cooling_device_wrapper(),
	 * so don't rollback cooler's devdata in this bind...
	 */

#if MTK_THERMAL_MONITOR_CONDITIONAL_COOLING
	{
		int i = 0;
		struct mtk_thermal_cooler_data *cldata = NULL;

		mutex_lock(&MTM_COOLER_LOCK);
		cldata = cdev->devdata;
		mutex_unlock(&MTM_COOLER_LOCK);

		for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS;
		i++) {
			if ((cldata->conditions[i][0] != 0x0) &&
				(cldata->condition_last_value[i]) == NULL) {

				if (strncmp(cldata->conditions[i],
					thermal->type, 20) == 0) {

					cldata->condition_last_value[i] =
							&(thermal->temperature);

					THRML_LOG(
						"[.bind]condition+ tz: %s cdev: %s condition: %s\n",
						thermal->type, cdev->type,
						cldata->conditions[i]);
				}
			}
		}
	}
#endif

	/* Bind Relationship to StoreLogger */
	THRML_LOG("[.bind]+ tz: %s cdev: %s tz_data:%p cl_data:%p\n",
	thermal->type, cdev->type, thermal->devdata, cdev->devdata);

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG(
			"[.bind]E tz: %s unregistered.\n", thermal->type);

		return 1;
	}

	if (ops->bind)
		ops->bind(thermal, cdev);

	/* Bind Relationship to StoreLogger */
	THRML_LOG("[.bind]- tz: %s cdev: %s tz_data:%p cl_data:%p\n",
		thermal->type, cdev->type, thermal->devdata, cdev->devdata);

	/* Log in mtk_thermal_zone_bind_cooling_device_wrapper() */
	/* THRML_STORAGE_LOG(THRML_LOGGER_MSG_BIND, bind, thermal->type,
	 *							cdev->type);
	 */

	return ret;
}

/*
 *.unbind wrapper: unbind the thermal zone device with a thermal cooling device.
 */
static int mtk_thermal_wrapper_unbind
(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

#if MTK_THERMAL_MONITOR_CONDITIONAL_COOLING
	{
		int i = 0;
		struct mtk_thermal_cooler_data *cldata = NULL;

		mutex_lock(&MTM_COOLER_LOCK);
		cldata = cdev->devdata;
		mutex_unlock(&MTM_COOLER_LOCK);

		/* Clear cldata->tz */
		if (thermal == cldata->tz) {
			/* clear the state of cooler bounded first... */
			if (cdev->ops) {
				int tmp_exit_pt = cldata->exit_threshold;

				cldata->exit_threshold = 0;
				cdev->ops->set_cur_state(cdev, 0);
				cldata->exit_threshold = tmp_exit_pt;
			}

			cldata->tz = NULL;
			cldata->trip = 0;
		}

		for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS;
		i++) {
			if ((cldata->condition_last_value[i] != NULL) &&
				(&(thermal->temperature) ==
				cldata->condition_last_value[i])) {

				cldata->condition_last_value[i] = NULL;
				THRML_LOG(
					"[.unbind]condition- tz: %s cdev: %s condition: %s\n",
					thermal->type, cdev->type,
					cldata->conditions[i]);
			}
		}
	}
#endif

	THRML_LOG("[.unbind]+ tz: %s cdev: %s\n", thermal->type, cdev->type);

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.unbind]E tz: %s unregistered.\n",
							thermal->type);
		return 1;
	}

	if (ops->unbind)
		ret = ops->unbind(thermal, cdev);

	THRML_LOG("[.unbind]- tz: %s cdev: %s\n", thermal->type, cdev->type);

	return ret;
}

/*
 * .get_temp wrapper: get the current temperature of the thermal zone.
 */
static int mtk_thermal_wrapper_get_temp
(struct thermal_zone_device *thermal, int *temperature)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;
	int nTemperature;
	int raw_temp = 0;
#if MTK_THERMAL_MONITOR_MEASURE_GET_TEMP_OVERHEAD
	long int t = _get_current_time_us();
	long int dur = 0;
#endif

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.get_temp] tz: %s unregistered.\n",
							thermal->type);
		return 1;
	}
	if (ops->get_temp)
		ret = ops->get_temp(thermal, &raw_temp);

	nTemperature = (int)raw_temp;	/* /< Long cast to INT. */

	if (ret == 0) {
		*temperature =
		_mtkthermal_update_and_get_sma(thermal->devdata, raw_temp);
		/* No strong type cast... */
	} else {
		THRML_ERROR_LOG("[.get_temp] tz: %s invalid temp\n",
							thermal->type);
		*temperature = nTemperature;
	}

	/* Monitor Temperature to StoreLogger */
	THRML_STORAGE_LOG(THRML_LOGGER_MSG_ZONE_TEMP, get_temp,
				thermal->type, (int)*temperature);

	THRML_LOG("[.get_temp] tz: %s raw: %d sma: %ld\n",
						thermal->type, nTemperature,
						(long)*temperature);

#if MTK_THERMAL_MONITOR_MEASURE_GET_TEMP_OVERHEAD
	dur = _get_current_time_us() - t;
	if (dur > 10000)	/* over 10msec, log it */
		THRML_ERROR_LOG("[.get_temp] tz: %s dur: %ld\n",
						thermal->type, dur);
#endif

	return ret;
}

/*
 * .get_mode wrapper: get the current mode (user/kernel) of the thermal zone.
 *  - "kernel" means thermal management is done in kernel.
 *  - "user" will prevent kernel thermal driver actions upon trip points
 */
static int mtk_thermal_wrapper_get_mode
(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	THRML_LOG("[.get_mode] tz: %s mode: %d\n", thermal->type, *mode);

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.get_mode] tz: %s unregistered.\n",
							thermal->type);
		return 1;
	}

	if (ops->get_mode)
		ret = ops->get_mode(thermal, mode);

	return ret;
}

/*
 *  .set_mode wrapper: set the mode (user/kernel) of the thermal zone.
 */
static int mtk_thermal_wrapper_set_mode
(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	THRML_LOG("[.set_mode] tz: %s mode: %d\n", thermal->type, mode);

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.set_mode] tz: %s unregistered.\n",
							thermal->type);
		return 1;
	}

	if (ops->set_mode)
		ret = ops->set_mode(thermal, mode);

	return ret;
}

/*
 * .get_trip_type wrapper: get the type of certain trip point.
 */
static int mtk_thermal_wrapper_get_trip_type
(struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.get_trip_type] tz: %s unregistered.\n",
								thermal->type);
		return 1;
	}

	if (ops->get_trip_type)
		ret = ops->get_trip_type(thermal, trip, type);

	THRML_LOG("[.get_trip_type] tz: %s trip: %d type: %d\n",
						thermal->type, trip, *type);

	return ret;
}

/*
 * .get_trip_temp wrapper:get the temperature above which the certain trip point
 *  will be fired.
 */
static int mtk_thermal_wrapper_get_trip_temp
(struct thermal_zone_device *thermal, int trip, int *temperature)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.get_trip_temp] tz: %s unregistered.\n",
								thermal->type);
		return 1;
	}

	if (ops->get_trip_temp)
		ret = ops->get_trip_temp(thermal, trip, temperature);

	THRML_LOG("[.get_trip_temp] tz: %s trip: %d temp: %d\n",
					thermal->type, trip, (int)*temperature);

	THRML_STORAGE_LOG(THRML_LOGGER_MSG_TRIP_POINT, get_trip_temp,
					thermal->type, trip, *temperature);

	return ret;
}

/*
 * .get_crit_temp wrapper:
 */
static int mtk_thermal_wrapper_get_crit_temp
(struct thermal_zone_device *thermal, int *temperature)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.get_crit_temp] tz: %s unregistered.\n",
								thermal->type);
		return 1;
	}

	if (ops->get_crit_temp)
		ret = ops->get_crit_temp(thermal, temperature);

	THRML_LOG("[.get_crit_temp] tz: %s temp: %d\n", thermal->type,
							(int)*temperature);

	return ret;
}

static int mtk_thermal_wrapper_notify(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type type)
{
	int ret = 0;
	struct thermal_zone_device_ops *ops;

	ops = getClientZoneOps(thermal);

	if (!ops) {
		THRML_ERROR_LOG("[.notify] tz: %s unregistered.\n",
							thermal->type);
		return 1;
	}

	if (ops->notify)
		ret = ops->notify(thermal, trip, type);

	return ret;
}



/* *************************************** */
/* MTK thermal zone register/unregister */
/* *************************************** */

/* Wrapper callback OPS */
static struct thermal_zone_device_ops mtk_thermal_wrapper_dev_ops = {
	.bind = mtk_thermal_wrapper_bind,
	.unbind = mtk_thermal_wrapper_unbind,
	.get_temp = mtk_thermal_wrapper_get_temp,
	.get_mode = mtk_thermal_wrapper_get_mode,
	.set_mode = mtk_thermal_wrapper_set_mode,
	.get_trip_type = mtk_thermal_wrapper_get_trip_type,
	.get_trip_temp = mtk_thermal_wrapper_get_trip_temp,
	.get_crit_temp = mtk_thermal_wrapper_get_crit_temp,
	.notify = mtk_thermal_wrapper_notify,
};

/*mtk thermal zone register function */
struct thermal_zone_device *mtk_thermal_zone_device_register_wrapper(
char *type, int trips, void *devdata,
const struct thermal_zone_device_ops *ops,
int tc1, int tc2, int passive_delay, int polling_delay)
{

	struct thermal_zone_device *tz = NULL;
	struct mtk_thermal_tz_data *tzdata = NULL;
	int tzidx;

	THRML_LOG("%s tz: %s trips: %d passive_delay: %d polling_delay: %d\n",
			__func__, type, trips, passive_delay, polling_delay);

	if (strcmp(SYSINFO_ATTACH_DEV_NAME, type) == 0)
		g_SysinfoAttachOps = (struct thermal_zone_device_ops *)ops;

	tzdata = kzalloc(sizeof(struct mtk_thermal_tz_data), GFP_KERNEL);
	if (!tzdata) {
		THRML_ERROR_LOG("%s tzdata kzalloc fail.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&tzdata->ma_lock);
	mutex_lock(&tzdata->ma_lock);
	tzdata->ops = (struct thermal_zone_device_ops *)ops;
	tzdata->ma_len = 1;
	tzdata->ma_counter = 0;
	tzdata->fake_temp = -275000;	/* init to -275000 */
#if (MAX_STEP_MA_LEN > 1)
	tzdata->curr_idx_ma_len = 0;
	tzdata->ma_lens[0] = 1;
	tzdata->msma_ht[0] = MSMA_MAX_HT;
#endif
	mutex_unlock(&tzdata->ma_lock);

	tz = thermal_zone_device_register(type,
			trips,	/* /< total number of trip points */
			0,	/* /< mask */
			/* (void*)ops,     ///< invoker's ops pass to devdata */
			(void *)tzdata,
			&mtk_thermal_wrapper_dev_ops, /* /< use wrapper ops. */
			NULL,	/* /< tzp */
			passive_delay, polling_delay);

	tzidx = mtk_thermal_get_tz_idx(type);

	/* registered the last_temperature to local arra */
	mutex_lock(&MTM_GET_TEMP_LOCK);
	{
		if (tzidx >= 0 && tzidx < MTK_THERMAL_SENSOR_COUNT)
			tz_last_values[tzidx] = &(tz->temperature);
	}
	mutex_unlock(&MTM_GET_TEMP_LOCK);


	/* create a proc for this tz... */
	if (_get_proc_tz_dir_entry() != NULL) {
		struct proc_dir_entry *entry;

		entry = proc_create_data((const char *)type, 0664,
							proc_tz_dir_entry,
							&_mtkthermal_tz_fops,
							tz);

		if (!entry) {
			THRML_ERROR_LOG("%s proc file not created: %p\n",
								__func__, tz);

		} else {
			proc_set_user(entry, uid, gid);
			THRML_LOG("%s proc file created: %p\n", __func__, tz);
		}
	}

	/* This interface function adds a new thermal zone device */
	return tz;

}
EXPORT_SYMBOL(mtk_thermal_zone_device_register_wrapper);

/*mtk thermal zone unregister function */
void mtk_thermal_zone_device_unregister_wrapper(struct thermal_zone_device *tz)
{
	char type[32] = { 0 };
	struct mtk_thermal_tz_data *tzdata = NULL;
	int tzidx;

	strncpy(type, tz->type, 20);
	tzdata = (struct mtk_thermal_tz_data *)tz->devdata;

	/* delete the proc file entry from proc */
	if (proc_tz_dir_entry != NULL)
		remove_proc_entry((const char *)type, proc_tz_dir_entry);

	tzidx = mtk_thermal_get_tz_idx(tz->type);

	/* unregistered the last_temperature from local array */
	mutex_lock(&MTM_GET_TEMP_LOCK);
	{
		if (tzidx >= 0 && tzidx < MTK_THERMAL_SENSOR_COUNT)
			tz_last_values[tzidx] = NULL;
	}
	mutex_unlock(&MTM_GET_TEMP_LOCK);

	THRML_LOG("%s+ tz : %s\n", __func__, type);

	thermal_zone_device_unregister(tz);

	THRML_LOG("%s- tz: %s\n", __func__, type);

	/* free memory */
	if (tzdata != NULL) {
		mutex_lock(&tzdata->ma_lock);
		tzdata->ops = NULL;
		mutex_unlock(&tzdata->ma_lock);
		mutex_destroy(&tzdata->ma_lock);
		kfree(tzdata);
	}
}
EXPORT_SYMBOL(mtk_thermal_zone_device_unregister_wrapper);

int mtk_thermal_zone_bind_cooling_device_wrapper(
struct thermal_zone_device *thermal,
int trip, struct thermal_cooling_device *cdev)
{

	struct mtk_thermal_cooler_data *mcdata;
	int ret = 0;

	THRML_LOG("%s thermal_type:%s trip:%d cdev_type:%s  ret:%d\n", __func__,
			thermal->type, trip, cdev->type, ret);

	ret =
		thermal_zone_bind_cooling_device(thermal, trip, cdev,
			THERMAL_NO_LIMIT, THERMAL_NO_LIMIT,
			THERMAL_WEIGHT_DEFAULT);

	if (ret) {
		/* THRML_ERROR_LOG("thermal_zone_bind_cooling_device Fail.
		 * Code(%d)\n", ret);
		 */
	} else {
		/* TODO: think of a way don't do this here...
		 * Or cannot rollback devdata in bind ops...
		 */
		/* Init mtk Cooler Data */
		mcdata = cdev->devdata;
		mcdata->trip = trip;
		mcdata->tz = thermal;
	}

	THRML_LOG("%s thermal_type:%s trip:%d cdev_type:%s  ret:%d\n", __func__,
			thermal->type, trip, cdev->type, ret);

	THRML_STORAGE_LOG(THRML_LOGGER_MSG_BIND, bind,
				thermal->type, trip, cdev->type);

	return ret;
}
EXPORT_SYMBOL(mtk_thermal_zone_bind_cooling_device_wrapper);

/* ********************************************* */
/* MTK cooling dev register/unregister */
/* ********************************************* */

/* .get_max_state */
static int mtk_cooling_wrapper_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int ret = 0;
	struct thermal_cooling_device_ops *ops;
	struct mtk_thermal_cooler_data *mcdata;

	mutex_lock(&MTM_COOLER_LOCK);

	/* Recovery client's devdata */
	ops = recoveryClientCooler(cdev, &mcdata);

	if (ops->get_max_state)
		ret = ops->get_max_state(cdev, state);

	THRML_LOG("[.get_max_state] cdev_type:%s state:%lu\n",
						cdev->type, *state);

	cdev->devdata = mcdata;

	mutex_unlock(&MTM_COOLER_LOCK);

	return ret;
}

/* .get_cur_state */
static int mtk_cooling_wrapper_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int ret = 0;
	struct thermal_cooling_device_ops *ops;
	struct mtk_thermal_cooler_data *mcdata;

	mutex_lock(&MTM_COOLER_LOCK);

	/* Recovery client's devdata */
	ops = recoveryClientCooler(cdev, &mcdata);

	if (ops->get_cur_state)
		ret = ops->get_cur_state(cdev, state);

	THRML_LOG("[.get_cur_state] cdev_type:%s state:%lu\n",
						cdev->type, *state);

	/* reset devdata to mcdata */
	cdev->devdata = mcdata;

	mutex_unlock(&MTM_COOLER_LOCK);

	return ret;
}

/* set_cur_state */
static int mtk_cooling_wrapper_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct thermal_cooling_device_ops *ops;
	struct thermal_cooling_device_ops_extra *ops_ext;
	struct mtk_thermal_cooler_data *mcdata;
	int ret = 0;
	unsigned long cur_state = 0;

	mutex_lock(&MTM_COOLER_LOCK);

	/* Recovery client's devdata */
	ops = recoveryClientCooler(cdev, &mcdata);
	ops_ext = mcdata->ops_ext;

	if (ops == NULL) {
		THRML_ERROR_LOG("[.set_cur_state]E no cdev ops.\n");
		mutex_unlock(&MTM_COOLER_LOCK);
		return -1;
	}

	if (ops->get_cur_state)
		ret = ops->get_cur_state(cdev, &cur_state);

	/* check conditions */
#if MTK_THERMAL_MONITOR_CONDITIONAL_COOLING
	if (state != 0) {
		/* here check conditions for setting the cooler... */
		if (MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS ==
				_mtkthermal_check_cooler_conditions(mcdata)) {
			/* pass */
		} else {
			THRML_LOG(
				"[.set_cur_state]condition check failed tz_type:%s cdev_type:%s trip:%d state:%lu\n",
				mcdata->tz->type, cdev->type,
				mcdata->trip, state);

			state = 0;
		}
	}


	if (state == 0) {
		int last_temp = 0;
		int trip_temp = 0;
		struct thermal_zone_device_ops *tz_ops;

		if ((mcdata->exit_threshold > 0) && (mcdata->tz != NULL)) {
			/* if exit point is set and if this cooler
			 * is still bound...
			 */

			THRML_LOG("[.set_cur_state] cur_state:%lu\n",
								cur_state);

			if (cur_state > 0) {
				THRML_LOG("[.set_cur_state] tz:%p devdata:%p\n",
					mcdata->tz, mcdata->tz->devdata);

				if (mcdata->tz)
					last_temp = mcdata->tz->temperature;

				THRML_LOG("[.set_cur_state] last_temp:%d\n",
					last_temp);

				tz_ops = getClientZoneOps(mcdata->tz);
				if (tz_ops == NULL) {
					THRML_ERROR_LOG(
					"[.set_cur_state]tz_ops null\n");

					mutex_unlock(&MTM_COOLER_LOCK);
					return -1;
				}

				if (!ops) {
					THRML_ERROR_LOG(
					"[.set_cur_state]E tz unregistered.\n");
					/* WARN_ON_ONCE(1); */
					trip_temp = 120000;
				} else {
					if (tz_ops->get_trip_temp) {
						tz_ops->get_trip_temp(
							mcdata->tz, mcdata->trip
							, &trip_temp);

						THRML_LOG(
							"[.set_cur_state] trip_temp:%ld\n",
							(long)trip_temp);

					} else {
						WARN_ON_ONCE(1);
					}
				}

				if ((last_temp >= (int)trip_temp)
					|| (((int)trip_temp - last_temp)
					< mcdata->exit_threshold)) {

					THRML_LOG(
						"[.set_cur_state]not exit yet tz_type:%s cdev_type:%s trip:%d state:%lu\n",
						mcdata->tz->type, cdev->type,
						mcdata->trip, state);

					state = cur_state;
				}
			}
		}
	}
#endif

	THRML_LOG("[.set_cur_state] tz_type:%s cdev_type:%s trip:%d state:%lu\n"
			, mcdata->tz->type, cdev->type, mcdata->trip, state);

	THRML_STORAGE_LOG(THRML_LOGGER_MSG_COOL_STAE, set_cur_state,
			mcdata->tz->type, mcdata->trip, cdev->type, state);

	if (ops->set_cur_state)
		ret = ops->set_cur_state(cdev, state);

	if (ops_ext && ops_ext->set_cur_temp && mcdata->tz)
		ops_ext->set_cur_temp(cdev, mcdata->tz->temperature);

	/* reset devdata to mcdata */
	cdev->devdata = mcdata;

	mutex_unlock(&MTM_COOLER_LOCK);

	return ret;
}

/* Cooling callbacks OPS */
static struct thermal_cooling_device_ops mtk_cooling_wrapper_dev_ops = {
	.get_max_state = mtk_cooling_wrapper_get_max_state,
	.get_cur_state = mtk_cooling_wrapper_get_cur_state,
	.set_cur_state = mtk_cooling_wrapper_set_cur_state,
};

/*
 * MTK Cooling Register
 */
struct thermal_cooling_device *mtk_thermal_cooling_device_register_wrapper
(char *type, void *devdata, const struct thermal_cooling_device_ops *ops)
{
	struct mtk_thermal_cooler_data *mcdata = NULL;
	struct thermal_cooling_device *ret = NULL;
	int i = 0;

	THRML_LOG("%s type:%s\n", __func__, type);

	mcdata = kzalloc(sizeof(struct mtk_thermal_cooler_data), GFP_KERNEL);
	if (!mcdata) {
		THRML_ERROR_LOG("%s mcdata kzalloc fail.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	mcdata->ops = (struct thermal_cooling_device_ops *)ops;
	mcdata->ops_ext = NULL;
	mcdata->devdata = devdata;
	mcdata->exit_threshold = 0;

	for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS; i++) {
		mcdata->conditions[i][0] = 0x0;
		mcdata->condition_last_value[i] = NULL;
		mcdata->threshold[i] = 0;
	}

	/* create a proc for this cooler... */
	if (_get_proc_cooler_dir_entry() != NULL) {
		struct proc_dir_entry *entry;

		entry = proc_create_data((const char *)type, 0664,
						proc_cooler_dir_entry,
						&_mtkthermal_cooler_fops,
						mcdata);

		if (!entry) {
			THRML_ERROR_LOG("%s proc file not created: %p\n",
			__func__, mcdata);

		} else {
			proc_set_user(entry, uid, gid);
			THRML_LOG("%s proc file created: %p\n",
			__func__, mcdata);
		}
	}

	ret = thermal_cooling_device_register(type, mcdata,
				&mtk_cooling_wrapper_dev_ops);

	mcdata->id = ret->id;	/* Used for CPU usage flag... */
	return ret;
}
EXPORT_SYMBOL(mtk_thermal_cooling_device_register_wrapper);

struct thermal_cooling_device *mtk_thermal_cooling_device_register_wrapper_extra
(char *type, void *devdata, const struct thermal_cooling_device_ops *ops,
const struct thermal_cooling_device_ops_extra *ops_ext)
{

	struct mtk_thermal_cooler_data *mcdata = NULL;
	struct thermal_cooling_device *ret = NULL;
	int i = 0;

	THRML_LOG("%s type:%s\n", __func__, type);

	mcdata = kzalloc(sizeof(struct mtk_thermal_cooler_data), GFP_KERNEL);
	if (!mcdata) {
		THRML_ERROR_LOG("%s mcdata kzalloc fail.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	mcdata->ops = (struct thermal_cooling_device_ops *)ops;
	/* The only difference to mtk_thermal_cooling_device_register_wrapper */
	mcdata->ops_ext = (struct thermal_cooling_device_ops_extra *)ops_ext;
	mcdata->devdata = devdata;
	mcdata->exit_threshold = 0;

	for (; i < MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS; i++) {
		mcdata->conditions[i][0] = 0x0;
		mcdata->condition_last_value[i] = NULL;
		mcdata->threshold[i] = 0;
	}

	/* create a proc for this cooler... */
	if (_get_proc_cooler_dir_entry() != NULL) {
		struct proc_dir_entry *entry;

		entry = proc_create_data((const char *)type, 0664,
						proc_cooler_dir_entry,
						&_mtkthermal_cooler_fops,
						mcdata);

		if (!entry) {
			THRML_ERROR_LOG("%s proc file not created: %p\n",
							__func__, mcdata);

		} else {
			proc_set_user(entry, uid, gid);
			THRML_LOG("%s proc file created: %p\n",
							__func__, mcdata);
		}
	}

	ret = thermal_cooling_device_register(type, mcdata,
					&mtk_cooling_wrapper_dev_ops);

	mcdata->id = ret->id;	/* Used for CPU usage flag... */
	return ret;
}
EXPORT_SYMBOL(mtk_thermal_cooling_device_register_wrapper_extra);

int mtk_thermal_cooling_device_add_exit_point(
struct thermal_cooling_device *cdev, int exit_point)
{
	struct mtk_thermal_cooler_data *mcdata;

	if (!cdev)
		return -1;

	mutex_lock(&MTM_COOLER_LOCK);
	mcdata = cdev->devdata;

	if (!mcdata) {
		mutex_unlock(&MTM_COOLER_LOCK);
		return -1;
	}

	mcdata->exit_threshold = exit_point;
	mutex_unlock(&MTM_COOLER_LOCK);

	THRML_LOG("%s type:%s exit:%d\n", __func__, cdev->type, exit_point);
	return 0;
}
EXPORT_SYMBOL(mtk_thermal_cooling_device_add_exit_point);

/*
 * MTK Cooling Unregister
 */
void mtk_thermal_cooling_device_unregister_wrapper(
struct thermal_cooling_device *cdev)
{
	struct mtk_thermal_cooler_data *mcdata;
	char type[32] = { 0 };

	strncpy(type, cdev->type, 20);

	THRML_LOG("%s+ cdev:%p devdata:%p cdev:%s\n", __func__,
					cdev, cdev->devdata, type);

	/* delete the proc file entry from proc */
	if (proc_cooler_dir_entry != NULL)
		remove_proc_entry((const char *)type, proc_cooler_dir_entry);
	/* TODO: consider error handling... */

	mutex_lock(&MTM_COOLER_LOCK);

	/* free mtk cooler data */
	mcdata = cdev->devdata;

	mutex_unlock(&MTM_COOLER_LOCK);

	THRML_LOG("%s- mcdata:%p\n", __func__, mcdata);

	thermal_cooling_device_unregister(cdev);

	/* free mtk cooler data */
	kfree(mcdata);

	THRML_LOG("%s- cdev: %s\n", __func__, type);
}
EXPORT_SYMBOL(mtk_thermal_cooling_device_unregister_wrapper);

int mtk_thermal_zone_bind_trigger_trip(
struct thermal_zone_device *tz, int trip, int mode)
{
	THRML_LOG("%s trip %d\n", __func__, trip);
	schedule_delayed_work(&(tz->poll_queue), 0);
	return 0;
}
EXPORT_SYMBOL(mtk_thermal_zone_bind_trigger_trip);

int mtk_thermal_get_temp(enum mtk_thermal_sensor_id id)
{
	int ret = 0;

	if (id < 0 || id >= MTK_THERMAL_SENSOR_COUNT)
		return -127000;

	mutex_lock(&MTM_GET_TEMP_LOCK);
	if (tz_last_values[id] == NULL) {
		mutex_unlock(&MTM_GET_TEMP_LOCK);
		return -127000;
	}

	ret = *tz_last_values[id];
	mutex_unlock(&MTM_GET_TEMP_LOCK);
	return ret;
}
EXPORT_SYMBOL(mtk_thermal_get_temp);

struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void)
{
	mutex_lock(&MTM_DRV_THERM_PROC_DIR_LOCK);
	if (proc_drv_therm_dir_entry == NULL) {
		proc_drv_therm_dir_entry = proc_mkdir("driver/thermal", NULL);
		if (proc_drv_therm_dir_entry == NULL)
			THRML_ERROR_LOG(
				"[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
	}
	mutex_unlock(&MTM_DRV_THERM_PROC_DIR_LOCK);
	return proc_drv_therm_dir_entry;
}
EXPORT_SYMBOL(mtk_thermal_get_proc_drv_therm_dir_entry);

module_init(mtkthermal_init);
module_exit(mtkthermal_exit);
