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
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <mt_ccci_common.h>
#include <linux/uidgid.h>
#include <mt_cooler_setting.h>
#include <linux/debugfs.h>

/* extern unsigned long ccci_get_md_boot_count(int md_id); */

#if FEATURE_THERMAL_DIAG
/* signal */
/* #define MAX_LEN	256 */
static unsigned int tmd_pid;
static unsigned int tmd_input_pid;
static struct task_struct tmd_task;
static struct task_struct *ptmd_task = &tmd_task;
#endif

#if FEATURE_MUTT_V2
/* signal */
#define MAX_LEN	128
static unsigned int tm_pid;
static unsigned int tm_input_pid;
static struct task_struct g_task;
static struct task_struct *pg_task = &g_task;

/* mdoff cooler */
static struct thermal_cooling_device *cl_dev_mdoff;
static unsigned int cl_dev_mdoff_state;

/* noIMS cooler */
static struct thermal_cooling_device *cl_dev_noIMS;
static unsigned int cl_dev_noIMS_state;
#endif

#if FEATURE_ADAPTIVE_MUTT
/* adp mutt cooler */
static struct thermal_cooling_device *cl_dev_adp_mutt;
static unsigned int cl_dev_adp_mutt_state;
static unsigned int cl_dev_adp_mutt_limit;
static int curr_adp_mutt_level = -1;

#define init_MD_tput_limit 0

/* Interal usage */
/* parameter from adb shell */
/* this is in milli degree C */
static int MD_target_t = 58000;
static int MD_TARGET_T_HIGH;
static int MD_TARGET_T_LOW;
static int t_stable_range = 1000;
static int tt_MD_high = 50;	/* initial value: assume 1 degreeC for temp. <=> 20% for MD_tput_limit(0~100) */
static int tt_MD_low = 50;
static int triggered;
#endif

unsigned long __attribute__ ((weak))
ccci_get_md_boot_count(int md_id)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

int __attribute__ ((weak))
exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return -316;
}

#define mtk_cooler_mutt_dprintk_always(fmt, args...) \
pr_debug("thermal/cooler/mutt" fmt, ##args)

#define mtk_cooler_mutt_dprintk(fmt, args...) \
do { \
	if (1 == cl_mutt_klog_on) \
		pr_debug("[thermal/cooler/mutt]" fmt, ##args); \
} while (0)

/* State of "MD off & noIMS" are not included. */
#define MAX_NUM_INSTANCE_MTK_COOLER_MUTT  4

#define MTK_CL_MUTT_GET_LIMIT(limit, state) \
{ (limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_MUTT_SET_LIMIT(limit, state) \
{ state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_MUTT_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_MUTT_SET_CURR_STATE(curr_state, state) \
do { \
	if (0 == curr_state) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

#if FEATURE_MUTT_V2
/**
 * No UL data(except IMS): active = 1; suspend = 255; bit0 in reserved =0;
 * No UL data(no IMS): active = 1; suspend = 255; bit0 in reserved =1;
*/
#define BIT_MD_CL_NO_IMS	0x01000000
#define MD_CL_NO_UL_DATA	0x00FF0101
#endif

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int cl_mutt_klog_on;
static struct thermal_cooling_device *cl_mutt_dev[MAX_NUM_INSTANCE_MTK_COOLER_MUTT] = { 0 };
static unsigned int cl_mutt_param[MAX_NUM_INSTANCE_MTK_COOLER_MUTT] = { 0 };
static unsigned long cl_mutt_state[MAX_NUM_INSTANCE_MTK_COOLER_MUTT] = { 0 };

static unsigned int cl_mutt_cur_limit;

static unsigned long last_md_boot_cnt;

#if FEATURE_THERMAL_DIAG
/*
 * use "si_code" for Action identify
 * for tmd_pid (/system/bin/thermald)
 */
enum {
/*	TMD_Alert_ShutDown = 1, */
	TMD_Alert_ULdataBack = 2,
	TMD_Alert_NOULdata = 3
};

static int clmutt_send_tmd_signal(int level)
{
	int ret = 0;
	static int warning_state = TMD_Alert_ULdataBack;

	if (warning_state == level)
		return ret;

	if (tmd_input_pid == 0) {
		mtk_cooler_mutt_dprintk("%s pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_mutt_dprintk_always(" %s pid is %d, %d; MD_Alert: %d\n", __func__,
							tmd_pid, tmd_input_pid, level);

	if (ret == 0 && tmd_input_pid != tmd_pid) {
		tmd_pid = tmd_input_pid;
		ptmd_task = get_pid_task(find_vpid(tmd_pid), PIDTYPE_PID);
	}

	if (ret == 0 && ptmd_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 0;
		info.si_code = level;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, ptmd_task);
	}

	if (ret != 0)
		mtk_cooler_mutt_dprintk_always(" %s ret=%d\n", __func__, ret);
	else {
		if (TMD_Alert_ULdataBack == level)
			warning_state = TMD_Alert_ULdataBack;
		else if (TMD_Alert_NOULdata == level)
			warning_state = TMD_Alert_NOULdata;
	}

	return ret;
}

static ssize_t clmutt_tmd_pid_write(struct file *filp, const char __user *buf, size_t count,
				    loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };
	int len = 0;

	len = (count < (MAX_LEN - 1)) ? count : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &tmd_input_pid);
	if (ret)
		WARN_ON(1);

	mtk_cooler_mutt_dprintk("%s %s = %d\n", __func__, tmp, tmd_input_pid);

	return len;
}

static int clmutt_tmd_pid_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", tmd_input_pid);
	mtk_cooler_mutt_dprintk("%s %d\n", __func__, tmd_input_pid);

	return 0;
}

static int clmutt_tmd_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, clmutt_tmd_pid_read, PDE_DATA(inode));
}

static const struct file_operations clmutt_tmd_pid_fops = {
	.owner = THIS_MODULE,
	.open = clmutt_tmd_pid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clmutt_tmd_pid_write,
	.release = single_release,
};
#endif

#if FEATURE_MUTT_V2
/*
 * use "si_errno" for client identify
 * for tm_pid (/system/bin/thermal)
 */
enum {
/*	TM_CLIENT_clwmt = 0,
	TM_CLIENT_mdulthro =1,
	TM_CLIENT_mddlthro =2,	*/
	TM_CLIENT_clmutt = 3
};
static int clmutt_send_tm_signal(int level)
{
	int ret = 0, j = 0;

	if (cl_dev_mdoff_state == level)
		return ret;

	if (tm_input_pid == 0) {
		mtk_cooler_mutt_dprintk("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_mutt_dprintk_always("[%s] pid is %d, %d; MD off: %d\n", __func__,
								tm_pid, tm_input_pid, level);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = TM_CLIENT_clmutt;
		info.si_code = level; /* Toggle MD ON: 0 OFF: 1*/
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_cooler_mutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
	else {
		if (1 == level) {
			cl_dev_mdoff_state = level;
			cl_dev_noIMS_state = 0;
			cl_mutt_cur_limit = 0;
			for (; j < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; j++)
				MTK_CL_MUTT_SET_CURR_STATE(0, cl_mutt_state[j]);
		} else
			cl_dev_mdoff_state = 0;
	}
	return ret;
}

static ssize_t clmutt_tm_pid_write(struct file *filp, const char __user *buf, size_t count, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = {0};
	int len = 0;

	len = (count < (MAX_LEN - 1)) ? count : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &tm_input_pid);
	if (ret)
		WARN_ON(1);

	mtk_cooler_mutt_dprintk("[%s] %s = %d\n", __func__, tmp, tm_input_pid);

	return len;
}

static int clmutt_tm_pid_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", tm_input_pid);

	mtk_cooler_mutt_dprintk("[%s] %d\n", __func__, tm_input_pid);

	return 0;
}

static int clmutt_tm_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, clmutt_tm_pid_read, PDE_DATA(inode));
}

static const struct file_operations clmutt_tm_pid_fops = {
	.owner = THIS_MODULE,
	.open = clmutt_tm_pid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clmutt_tm_pid_write,
	.release = single_release,
};

/*
 * cooling device callback functions (mtk_cl_mdoff_ops)
 * 1 : True and 0 : False
 */
static int mtk_cl_mdoff_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("mtk_cl_mdoff_get_max_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_mdoff_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = cl_dev_mdoff_state;
	mtk_cooler_mutt_dprintk("mtk_cl_mdoff_get_max_state() %s %lu (0: md on;  1: md off)\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_mdoff_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	if ((state >= 0) && (state <= 1))
		mtk_cooler_mutt_dprintk("mtk_cl_mdoff_set_cur_state() %s %lu (0: md on;  1: md off)\n",
								cdev->type, state);
	else {
		mtk_cooler_mutt_dprintk("mtk_cl_mdoff_set_cur_state(): Invalid input (0:md on;	 1: md off)\n");
		return 0;
	}

	clmutt_send_tm_signal(state);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_mdoff_ops = {
	.get_max_state = mtk_cl_mdoff_get_max_state,
	.get_cur_state = mtk_cl_mdoff_get_cur_state,
	.set_cur_state = mtk_cl_mdoff_set_cur_state,
};

static void mtk_cl_mutt_set_onIMS(int level)
{
	int ret = 0;
	unsigned int cl_mutt_param_noIMS = 0;

	if (cl_dev_noIMS_state == level)
		return;

	if (level)
		cl_mutt_param_noIMS = (MD_CL_NO_UL_DATA | BIT_MD_CL_NO_IMS);
	else
		cl_mutt_param_noIMS = MD_CL_NO_UL_DATA;

	if (cl_mutt_param_noIMS != cl_mutt_cur_limit) {
		cl_mutt_cur_limit = cl_mutt_param_noIMS;
		last_md_boot_cnt = ccci_get_md_boot_count(MD_SYS1);
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG, (char *) &cl_mutt_cur_limit, 4);
		mtk_cooler_mutt_dprintk_always("[%s] ret %d param %x bcnt %lul\n",
								__func__, ret, cl_mutt_cur_limit, last_md_boot_cnt);
	} else if (cl_mutt_param_noIMS != 0) {
		unsigned long cur_md_bcnt = ccci_get_md_boot_count(MD_SYS1);

		if (last_md_boot_cnt != cur_md_bcnt) {
			last_md_boot_cnt = cur_md_bcnt;
			ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG, (char *) &cl_mutt_cur_limit, 4);
			mtk_cooler_mutt_dprintk_always("[%s] mdrb ret %d param %x bcnt %lul\n",
								__func__, ret, cl_mutt_cur_limit, last_md_boot_cnt);
		}
	}

	if (ret != 0)
		mtk_cooler_mutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
	else {
		if (1 == level)
			cl_dev_noIMS_state = level;
		else
			cl_dev_noIMS_state = 0;
	}
}

/*
 * cooling device callback functions (mtk_cl_noIMS_ops)
 * 1 : True and 0 : False
 */
static int mtk_cl_noIMS_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("mtk_cl_noIMS_get_max_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_noIMS_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = cl_dev_noIMS_state;
	mtk_cooler_mutt_dprintk("mtk_cl_noIMS_get_max_state() %s %lu (0: md IMS OK;  1: md no IMS)\n",
							cdev->type, *state);
	return 0;
}

static int mtk_cl_noIMS_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{

	if (1 == cl_dev_mdoff_state) {
		mtk_cooler_mutt_dprintk("mtk_cl_noIMS_set_cur_state():  MD STILL OFF!!\n");
		return 0;
	}

	if ((state >= 0) && (state <= 1))
		mtk_cooler_mutt_dprintk("mtk_cl_noIMS_set_cur_state() %s %lu (0: md IMS OK;	1: md no IMS)\n",
							cdev->type, state);
	else {
		mtk_cooler_mutt_dprintk("mtk_cl_noIMS_set_cur_state(): Invalid input(0: md IMS OK; 1: md no IMS)\n");
		return 0;
	}

	mtk_cl_mutt_set_onIMS(state);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_noIMS_ops = {
	.get_max_state = mtk_cl_noIMS_get_max_state,
	.get_cur_state = mtk_cl_noIMS_get_cur_state,
	.set_cur_state = mtk_cl_noIMS_set_cur_state,
};
#endif

static void mtk_cl_mutt_set_mutt_limit(void)
{
	/* TODO: optimize */
	int i = 0, j = 0, ret = 0;
	int min_limit = 255;
	unsigned int min_param = 0;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i++) {
		unsigned long curr_state;

		MTK_CL_MUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);

		if (1 == curr_state) {
			unsigned int active;
			unsigned int suspend;
			int limit = 0;

			active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
			suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

			/* a cooler with 0 active or 0 suspend is not allowed */
			if (active == 0 || suspend == 0)
				goto err_unreg;

			/* compare the active/suspend ratio */
			if (active >= suspend)
				limit = active / suspend;
			else
				limit = (0 - suspend) / active;

			if (limit <= min_limit) {
				min_limit = limit;
				min_param = cl_mutt_param[i];
			}
		}
	}

#if FEATURE_ADAPTIVE_MUTT
		if (cl_dev_adp_mutt_limit > min_param)
			min_param = cl_dev_adp_mutt_limit;
#endif

	if (min_param != cl_mutt_cur_limit) {
		cl_mutt_cur_limit = min_param;
		last_md_boot_cnt = ccci_get_md_boot_count(MD_SYS1);
		ret =
		    exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
						 (char *)&cl_mutt_cur_limit, 4);
		mtk_cooler_mutt_dprintk_always("[%s] ret %d param %x bcnt %lul\n", __func__, ret,
					       cl_mutt_cur_limit, last_md_boot_cnt);
	} else if (min_param != 0) {
		unsigned long cur_md_bcnt = ccci_get_md_boot_count(MD_SYS1);

		if (last_md_boot_cnt != cur_md_bcnt) {
			last_md_boot_cnt = cur_md_bcnt;
			ret =
			    exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
							 (char *)&cl_mutt_cur_limit, 4);
			mtk_cooler_mutt_dprintk_always("[%s] mdrb ret %d param %x bcnt %lul\n",
						       __func__, ret, cl_mutt_cur_limit,
						       last_md_boot_cnt);
		}
	} else
		return;

	if (ret != 0) {
			cl_mutt_cur_limit = 0;
			for (; j < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; j++)
				MTK_CL_MUTT_SET_CURR_STATE(0, cl_mutt_state[j]);

			mtk_cooler_mutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
	}
#if FEATURE_THERMAL_DIAG
	else {
		if (cl_mutt_cur_limit == MD_CL_NO_UL_DATA)
			clmutt_send_tmd_signal(TMD_Alert_NOULdata);
		else
			clmutt_send_tmd_signal(TMD_Alert_ULdataBack);
	}
#endif

err_unreg:
	return;

}

static int mtk_cl_mutt_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("mtk_cl_mutt_get_max_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_mutt_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_MUTT_GET_CURR_STATE(*state, *((unsigned long *)cdev->devdata));
	mtk_cooler_mutt_dprintk("mtk_cl_mutt_get_cur_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_mutt_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
#if FEATURE_MUTT_V2
	if ((1 == cl_dev_mdoff_state) || (1 == cl_dev_noIMS_state)) {
		mtk_cooler_mutt_dprintk("mtk_cl_mutt_set_cur_state():  MD OFF or noIMS!!\n");
		return 0;
	}
#endif

	mtk_cooler_mutt_dprintk("mtk_cl_mutt_set_cur_state() %s %lu\n", cdev->type, state);
	MTK_CL_MUTT_SET_CURR_STATE(state, *((unsigned long *)cdev->devdata));
	mtk_cl_mutt_set_mutt_limit();

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_mutt_ops = {
	.get_max_state = mtk_cl_mutt_get_max_state,
	.get_cur_state = mtk_cl_mutt_get_cur_state,
	.set_cur_state = mtk_cl_mutt_set_cur_state,
};

#if FEATURE_ADAPTIVE_MUTT
/* decrease by one level */
static void decrease_mutt_limit(void)
{
	if (curr_adp_mutt_level >= 0)
		curr_adp_mutt_level--;

	if (-1 == curr_adp_mutt_level)
		cl_dev_adp_mutt_limit = 0;
	else if (0 != cl_mutt_param[curr_adp_mutt_level])
		cl_dev_adp_mutt_limit = cl_mutt_param[curr_adp_mutt_level];
}

/* increase by one level */
static void increase_mutt_limit(void)
{
	if (curr_adp_mutt_level < (MAX_NUM_INSTANCE_MTK_COOLER_MUTT - 1))
		curr_adp_mutt_level++;

	if (0 != cl_mutt_param[curr_adp_mutt_level])
		cl_dev_adp_mutt_limit = cl_mutt_param[curr_adp_mutt_level];
}

static void unlimit_mutt_limit(void)
{
	curr_adp_mutt_level = -1;
	cl_dev_adp_mutt_limit = 0;
}

static int adaptive_tput_limit(long curr_temp)
{
	static int MD_tput_limit = init_MD_tput_limit;

	MD_TARGET_T_HIGH = MD_target_t + t_stable_range;
	MD_TARGET_T_LOW = MD_target_t - t_stable_range;

	/* mtk_cooler_mutt_dprintk("%s : active= %d tirgger= %d curr_temp= %ld\n", __func__,
		cl_dev_adp_mutt_state, triggered, curr_temp); */

	if (cl_dev_adp_mutt_state == 1) {
		int tt_MD = MD_target_t - curr_temp;	/* unit: mC */

		/* Check if it is triggered */
		if (!triggered) {
			if (curr_temp < MD_target_t)
				return 0;

			triggered = 1;
		}

		/* Adjust total power budget if necessary */
		if (curr_temp >= MD_TARGET_T_HIGH)
			MD_tput_limit += (tt_MD / tt_MD_high);
		else if (curr_temp <= MD_TARGET_T_LOW)
			MD_tput_limit += (tt_MD / tt_MD_low);

		/* mtk_cooler_mutt_dprintk("%s MD T %d Tc %ld, MD_tput_limit %d\n",
			       __func__, MD_target_t, curr_temp, MD_tput_limit); */

		/* Adjust MUTT level  */
		{
			if (MD_tput_limit >= 100) {
				decrease_mutt_limit();
				MD_tput_limit = 0;
			} else if (MD_tput_limit <= -100) {
				increase_mutt_limit();
				MD_tput_limit = 0;
			}
		}
	} else {
		if (triggered) {
			triggered = 0;
			MD_tput_limit = 0;
			unlimit_mutt_limit();
		}
	}

	return 0;
}

/*
 * cooling device callback functions (mtk_cl_adp_mutt_ops)
 * 1 : True and 0 : False
 */
static int mtk_cl_adp_mutt_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("[%s] %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_adp_mutt_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = cl_dev_adp_mutt_state;
	mtk_cooler_mutt_dprintk("[%s] %s %lu (0:adp mutt off; 1:adp mutt on)\n",
							__func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_adp_mutt_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
#if FEATURE_MUTT_V2
		if ((1 == cl_dev_mdoff_state) || (1 == cl_dev_noIMS_state)) {
			mtk_cooler_mutt_dprintk("[%s]  MD OFF or noIMS!!\n", __func__);
			return 0;
		}
#endif

	if ((state != 0) && (state != 1)) {
		mtk_cooler_mutt_dprintk("[%s] Invalid input(0:adp mutt off; 1:adp mutt on)\n", __func__);
		return 0;
	}

	mtk_cooler_mutt_dprintk("[%s] %s %lu (0:adp mutt off; 1:adp mutt on)\n",
						__func__, cdev->type, state);
	cl_dev_adp_mutt_state = state;

	adaptive_tput_limit(mtk_thermal_get_temp(MTK_THERMAL_SENSOR_MD_PA));

	mtk_cl_mutt_set_mutt_limit();

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_adp_mutt_ops = {
	.get_max_state = mtk_cl_adp_mutt_get_max_state,
	.get_cur_state = mtk_cl_adp_mutt_get_cur_state,
	.set_cur_state = mtk_cl_adp_mutt_set_cur_state,
};

/* =======================
#define debugfs_entry(name) \
do { \
		dentry_f = debugfs_create_u32(#name, S_IWUSR | S_IRUGO, _d, &name); \
		if (IS_ERR_OR_NULL(dentry_f)) {	\
			pr_warn("Unable to create debugfsfile: " #name "\n"); \
			return; \
		} \
} while (0)

static void create_debugfs_entries(void)
{
	struct dentry *dentry_f;
	struct dentry *_d;

	_d = debugfs_create_dir("cl_adp_mutt", NULL);
	if (IS_ERR_OR_NULL(_d)) {
		pr_info("unable to create debugfs directory\n");
		return;
	}

	debugfs_entry(MD_target_t);
	debugfs_entry(t_stable_range);
	debugfs_entry(tt_MD_high);
	debugfs_entry(tt_MD_low);
}

#undef debugfs_entry
========================== */
#endif

static int mtk_cooler_mutt_register_ltf(void)
{
	int i;

	mtk_cooler_mutt_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-mutt%02d", i);
		/* put mutt state to cooler devdata */
		cl_mutt_dev[i] = mtk_thermal_cooling_device_register(temp, (void *)&cl_mutt_state[i],
								     &mtk_cl_mutt_ops);
	}

#if FEATURE_MUTT_V2
	cl_dev_noIMS = mtk_thermal_cooling_device_register("mtk-cl-noIMS", NULL,
										&mtk_cl_noIMS_ops);

	cl_dev_mdoff = mtk_thermal_cooling_device_register("mtk-cl-mdoff", NULL,
										&mtk_cl_mdoff_ops);
#endif

#if FEATURE_ADAPTIVE_MUTT
		cl_dev_adp_mutt = mtk_thermal_cooling_device_register("mtk-cl-adp-mutt", NULL,
											&mtk_cl_adp_mutt_ops);
#endif

	return 0;
}

static void mtk_cooler_mutt_unregister_ltf(void)
{
	int i;

	mtk_cooler_mutt_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i-- > 0;) {
		if (cl_mutt_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_mutt_dev[i]);
			cl_mutt_dev[i] = NULL;
			cl_mutt_state[i] = 0;
		}
	}
#if FEATURE_MUTT_V2
	if (cl_dev_noIMS) {
		mtk_thermal_cooling_device_unregister(cl_dev_noIMS);
		cl_dev_noIMS = NULL;
	}

	if (cl_dev_mdoff) {
		mtk_thermal_cooling_device_unregister(cl_dev_mdoff);
		cl_dev_mdoff = NULL;
	}
#endif

#if FEATURE_ADAPTIVE_MUTT
	if (cl_dev_adp_mutt) {
		mtk_thermal_cooling_device_unregister(cl_dev_adp_mutt);
		cl_dev_adp_mutt = NULL;
	}
#endif

}

static int _mtk_cl_mutt_proc_read(struct seq_file *m, void *v)
{
    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-mutt<ID>> <active (ms)> <suspend (ms)> <param> <state>
     *  ..
     */
	{
		int i = 0;

		seq_printf(m, "klog %d\n", cl_mutt_klog_on);
#if FEATURE_MUTT_V2
		seq_printf(m, "curr_limit %x, noIMS: %d, mdoff: %d\n", cl_mutt_cur_limit,
					cl_dev_noIMS_state, cl_dev_mdoff_state);
#else
		seq_printf(m, "curr_limit %x\n", cl_mutt_cur_limit);
#endif

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i++) {
			unsigned int active;
			unsigned int suspend;
			unsigned long curr_state;

			active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
			suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

			MTK_CL_MUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);

			seq_printf(m, "mtk-cl-mutt%02d %u %u %x, state %lu\n", i, active, suspend,
				   cl_mutt_param[i], curr_state);
		}

#if FEATURE_ADAPTIVE_MUTT
		seq_printf(m, "amutt_target_temp %d\n", MD_target_t);
#endif

	}

	return 0;
}

static ssize_t _mtk_cl_mutt_proc_write(struct file *filp, const char __user *buffer, size_t count,
				       loff_t *data)
{
	int len = 0;
	char desc[128];
	int klog_on, mutt0_a, mutt0_s, mutt1_a, mutt1_s, mutt2_a, mutt2_s, mutt3_a, mutt3_s, amutt_target_temp;
	int scan_count = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

    /**
     * sscanf format <klog_on> <mtk-cl-mutt00 active (ms)> <mtk-cl-mutt00 suspended (ms)> <mtk-cl-mutt01 active (ms)> <mtk-cl-mutt01 suspended (ms)> <mtk-cl-mutt02 active (ms)> <mtk-cl-mutt02 suspended (ms)>...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-mutt* active/suspended (ms) > can only be positive integer or 0 to denote no limit
     */

	if (NULL == data) {
		mtk_cooler_mutt_dprintk("[%s] null data\n", __func__);
		return -EINVAL;
	}
	/* WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 4 */
#if (4 == MAX_NUM_INSTANCE_MTK_COOLER_MUTT)
	/* cl_mutt_param[0] = 0; */
	/* cl_mutt_param[1] = 0; */
	/* cl_mutt_param[2] = 0; */

	scan_count = sscanf(desc, "%d %d %d %d %d %d %d %d %d %d",
			&klog_on, &mutt0_a, &mutt0_s, &mutt1_a, &mutt1_s, &mutt2_a, &mutt2_s,
			&mutt3_a, &mutt3_s, &amutt_target_temp);

	if (1 <= scan_count) {
		if (klog_on == 0 || klog_on == 1)
			cl_mutt_klog_on = klog_on;

		if (mutt0_a == 0)
			cl_mutt_param[0] = 0;
		else if (mutt0_a >= 100 && mutt0_a <= 25500 && mutt0_s >= 100 && mutt0_s <= 25500)
			cl_mutt_param[0] = ((mutt0_s / 100) << 16) | ((mutt0_a / 100) << 8) | 1;

		if (mutt1_a == 0)
			cl_mutt_param[1] = 0;
		else if (mutt1_a >= 100 && mutt1_a <= 25500 && mutt1_s >= 100 && mutt1_s <= 25500)
			cl_mutt_param[1] = ((mutt1_s / 100) << 16) | ((mutt1_a / 100) << 8) | 1;

		if (mutt2_a == 0)
			cl_mutt_param[2] = 0;
		else if (mutt2_a >= 100 && mutt2_a <= 25500 && mutt2_s >= 100 && mutt2_s <= 25500)
			cl_mutt_param[2] = ((mutt2_s / 100) << 16) | ((mutt2_a / 100) << 8) | 1;

		if (mutt3_a == 0)
			cl_mutt_param[3] = 0;
		else if (mutt3_a >= 100 && mutt3_a <= 25500 && mutt3_s >= 100 && mutt3_s <= 25500)
			cl_mutt_param[3] = ((mutt3_s / 100) << 16) | ((mutt3_a / 100) << 8) | 1;

#if FEATURE_ADAPTIVE_MUTT
		if (scan_count > 1+MAX_NUM_INSTANCE_MTK_COOLER_MUTT*2)
			MD_target_t = amutt_target_temp;
#endif
		return count;
	}
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_MUTT!"
#endif
	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);
	return -EINVAL;
}

static int _mtk_cl_mutt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtk_cl_mutt_proc_read, NULL);
}

static const struct file_operations cl_mutt_fops = {
	.owner = THIS_MODULE,
	.open = _mtk_cl_mutt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtk_cl_mutt_proc_write,
	.release = single_release,
};

static int __init mtk_cooler_mutt_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i-- > 0;) {
		cl_mutt_dev[i] = NULL;
		cl_mutt_state[i] = 0;
	}

	mtk_cooler_mutt_dprintk("init\n");

	err = mtk_cooler_mutt_register_ltf();
	if (err)
		goto err_unreg;

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry) {
			mtk_cooler_mutt_dprintk_always("[%s]: mkdir /proc/driver/thermal failed\n",
						       __func__);
		} else {
			entry =
			    proc_create("clmutt", S_IRUGO | S_IWUSR | S_IWGRP, dir_entry,
					&cl_mutt_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
#if FEATURE_MUTT_V2
			entry = proc_create("clmutt_tm_pid", S_IRUGO | S_IWUSR | S_IWGRP,
								dir_entry, &clmutt_tm_pid_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
#endif
#if FEATURE_THERMAL_DIAG
			entry = proc_create("clmutt_tmd_pid", S_IRUGO | S_IWUSR | S_IWGRP,
								dir_entry, &clmutt_tmd_pid_fops);
			if (entry)
				proc_set_user(entry, uid, gid);
#endif
		}
	}

#if FEATURE_ADAPTIVE_MUTT
	/* create_debugfs_entries(); */
#endif

	return 0;

err_unreg:
	mtk_cooler_mutt_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_mutt_exit(void)
{
	mtk_cooler_mutt_dprintk("exit\n");

	/* remove the proc file */
	remove_proc_entry("clmutt", NULL);

	mtk_cooler_mutt_unregister_ltf();
}
module_init(mtk_cooler_mutt_init);
module_exit(mtk_cooler_mutt_exit);
