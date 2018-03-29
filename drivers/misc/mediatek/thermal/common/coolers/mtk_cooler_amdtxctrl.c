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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <mach/mtk_eemcs_helper.h>
#include "mt-plat/mtk_thermal_monitor.h"

#define CONFIG_SIGNAL_USER_SPACE    (0)

#if CONFIG_SIGNAL_USER_SPACE
#include <linux/pid.h>
#endif

#define cl_type_upper               "cl-amdtxctrl-u"
#define cl_type_lower               "cl-amdtxctrl-l"

#define mtk_cooler_amdtxctrl_dprintk_always(fmt, args...) \
pr_debug("thermal/cooler/amdtxctrl" fmt, ##args)

#define mtk_cooler_amdtxctrl_dprintk(fmt, args...) \
do { \
	if (1 == cl_amdtxctrl_klog_on) { \
		pr_debug("thermal/cooler/amdtxctrl" fmt, ##args); \
	} \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO  1

static int cl_amdtxctrl_klog_on;

/* over_up_time * polling interval > up_duration --> throttling */
static unsigned int over_up_time;	/* polling time */
static unsigned int up_duration = 30;	/* sec */
static unsigned int up_step = 1;	/* step */

/* below_low_time * polling interval > low_duration --> throttling */
static unsigned int below_low_time;	/* polling time */
static unsigned int low_duration = 10;	/* sec */
static unsigned int low_step = 1;	/* step */

static unsigned int low_rst_time;
static unsigned int low_rst_max = 3;

static int polling_interval = 1;	/* second */

#define UNK_STAT -1
#define LOW_STAT 0
#define MID_STAT 1
#define HIGH_STAT 2

#define MAX_LEN	256
#define COOLER_STEPS 5

#if CONFIG_SIGNAL_USER_SPACE
static unsigned int tm_pid;
static unsigned int tm_input_pid;
static struct task_struct g_task;
static struct task_struct *pg_task = &g_task;
#endif

static unsigned int cl_upper_dev_state;
static unsigned int cl_lower_dev_state;

static struct thermal_cooling_device *cl_upper_dev;
static struct thermal_cooling_device *cl_lower_dev;

typedef int (*activate_cooler_opp_func) (int level);

static activate_cooler_opp_func opp_func[COOLER_STEPS] = { 0 };

typedef struct adaptive_cooler {
	int cur_level;
	int max_level;
	activate_cooler_opp_func *opp_func_array;
} adaptive_coolers;

static adaptive_coolers amdtxctrl;

static int amdtxpwr_backoff(int level)
{
	int ret;

	if (level == 0) {
		/* no throttle */
		/* ret = eemcs_notify_md_by_sys_msg(MD_SYS5, EXT_MD_DTX_REQ, 8); */
		ret = eemcs_notify_md_by_sys_msg(MD_SYS5, EXT_MD_TX_PWR_REDU_REQ, 23);	/* TODO: 30db as unlimit... */
		mtk_cooler_amdtxctrl_dprintk_always("[%s] unlimit DTX and TX\n", __func__);

	}
#if 0
	else if (level >= 1 && level <= 7) {
		/* only DTX */
		ret = eemcs_notify_md_by_sys_msg(MD_SYS5, EXT_MD_DTX_REQ, 8 - level);
		ret = eemcs_notify_md_by_sys_msg(MD_SYS5, EXT_MD_TX_PWR_REDU_REQ, 23);	/* TODO: 30db as unlimit... */
		mtk_cooler_amdtxctrl_dprintk_always("[%s] limit DTX %d and unlimit TX\n", __func__,
						    8 - level);
	}
#endif
	else if (level >= 1 && level <= COOLER_STEPS - 1) {
		/* DTX 1/8 + Tx power back off */
		/* ret = eemcs_notify_md_by_sys_msg(MD_SYS5, EXT_MD_DTX_REQ, 1); */
		/* TODO: 30db as unlimit... */
		ret = eemcs_notify_md_by_sys_msg(MD_SYS5, EXT_MD_TX_PWR_REDU_REQ, (23 - (level) * 3));
		mtk_cooler_amdtxctrl_dprintk_always("[%s] limit DTX 1 and limit TX %d\n", __func__,
						    (23 - (level) * 3));
	} else {
		/* error... */
		ret = -1;
		mtk_cooler_amdtxctrl_dprintk_always("[%s] ouf of range\n", __func__);
	}

	return ret;

}



static int down_throttle(adaptive_coolers *p, int step)
{
	if (NULL == p)
		return -1;
	if (step <= 0)
		return p->cur_level;

	if (p->cur_level + step > p->max_level) {
		p->cur_level = p->max_level;
		p->opp_func_array[p->cur_level] (p->cur_level);
		return p->cur_level;
	}
	p->cur_level += step;
	p->opp_func_array[p->cur_level] (p->cur_level);
	return p->cur_level;
}

static int up_throttle(adaptive_coolers *p, int step)
{
	if (NULL == p)
		return -1;
	if (step <= 0)
		return p->cur_level;

	if (p->cur_level - step < 0) {
		p->cur_level = 0;
		p->opp_func_array[p->cur_level] (p->cur_level);
		return p->cur_level;
	}
	p->cur_level -= step;
	p->opp_func_array[p->cur_level] (p->cur_level);
	return p->cur_level;
}

static int rst_throttle(adaptive_coolers *p)
{
	if (NULL == p)
		return -1;

	p->cur_level = 0;
	p->opp_func_array[p->cur_level] (p->cur_level);
	return p->cur_level;
}


#if CONFIG_SIGNAL_USER_SPACE
static int wmt_send_signal(int level)
{
	int ret = 0;
	int thro = level;

	if (tm_input_pid == 0) {
		mtk_cooler_amdtxctrl_dprintk("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_amdtxctrl_dprintk_always("[%s] pid is %d, %d, %d\n", __func__, tm_pid,
					    tm_input_pid, thro);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 1;	/* for md ul throttling */
		info.si_code = thro;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_cooler_amdtxctrl_dprintk("[%s] ret=%d\n", __func__, ret);

	return ret;
}
#endif				/* CONFIG_SIGNAL_USER_SPACE */


/* index --> 0, lower; 1, upper */
/* is_on --> 0, off; 1, on */
static int judge_throttling(int index, int is_on, int interval)
{
	/*
	 *     throttling_stat
	 *        2 ( upper=1,lower=1 )
	 * UPPER ----
	 *        1 ( upper=0,lower=1 )
	 * LOWER ----
	 *        0 ( upper=0,lower=0 )
	 */
	static unsigned int throttling_pre_stat;
	static int mail_box[2] = { -1, -1 };

	static bool is_reset;

	/* unsigned long cur_thro = tx_throughput; */
	/* static unsigned long thro_constraint = 99 * 1000; */

	int cur_wifi_stat = 0;

	mtk_cooler_amdtxctrl_dprintk("[%s]+ [0]=%d, [1]=%d || [%d] is %s\n", __func__, mail_box[0],
				     mail_box[1], index, (is_on == 1 ? "ON" : "OFF"));
	mail_box[index] = is_on;

	if (mail_box[0] >= 0 && mail_box[1] >= 0) {
		cur_wifi_stat = mail_box[0] + mail_box[1];

		switch (cur_wifi_stat) {
		case HIGH_STAT:
			if (throttling_pre_stat < HIGH_STAT) {
				/* 1st down throttle */
				int new_step = down_throttle(&amdtxctrl, up_step);

				mtk_cooler_amdtxctrl_dprintk_always("LOW/MID-->HIGH: step %d\n",
								    new_step);

				throttling_pre_stat = HIGH_STAT;
				over_up_time = 0;
			} else if (throttling_pre_stat == HIGH_STAT) {
				/* keep down throttle */
				over_up_time++;
				if ((over_up_time * interval) >= up_duration) {
					int new_step = down_throttle(&amdtxctrl, up_step);

					mtk_cooler_amdtxctrl_dprintk_always
					    ("HIGH-->HIGH: step %d\n", new_step);

					over_up_time = 0;
				}
			} else {
				mtk_cooler_amdtxctrl_dprintk("[%s] Error state1!!\n", __func__,
							     throttling_pre_stat);
			}
			mtk_cooler_amdtxctrl_dprintk_always("case2 time=%d\n", over_up_time);
			break;

		case MID_STAT:
			if (throttling_pre_stat == LOW_STAT) {
				below_low_time = 0;
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amdtxctrl_dprintk_always("[%s] Go up!!\n", __func__);
			} else if (throttling_pre_stat == HIGH_STAT) {
				over_up_time = 0;
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amdtxctrl_dprintk_always("[%s] Go down!!\n", __func__);
			} else {
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amdtxctrl_dprintk("[%s] pre_stat=%d!!\n", __func__,
							     throttling_pre_stat);
			}
			break;

		case LOW_STAT:
			if (throttling_pre_stat > LOW_STAT) {
				/* 1st up throttle */
				int new_step = up_throttle(&amdtxctrl, low_step);

				mtk_cooler_amdtxctrl_dprintk_always("MID/HIGH-->LOW: step %d\n",
								    new_step);
				throttling_pre_stat = LOW_STAT;
				below_low_time = 0;
				low_rst_time = 0;
				is_reset = false;
			} else if (throttling_pre_stat == LOW_STAT) {
				below_low_time++;
				if ((below_low_time * interval) >= low_duration) {
					if (low_rst_time >= low_rst_max && !is_reset) {
						/* rst */
						rst_throttle(&amdtxctrl);

						mtk_cooler_amdtxctrl_dprintk_always
						    ("over rst time=%d\n", low_rst_time);

						low_rst_time = low_rst_max;
						is_reset = true;
					} else if (!is_reset) {
						/* keep up throttle */
						int new_step = up_throttle(&amdtxctrl, low_step);

						low_rst_time++;

						mtk_cooler_amdtxctrl_dprintk_always
						    ("LOW-->LOW: step %d\n", new_step);

						below_low_time = 0;
					} else {
						mtk_cooler_amdtxctrl_dprintk
						    ("Have reset, no control!!");
					}
				}
			} else {
				mtk_cooler_amdtxctrl_dprintk_always("[%s] Error state3 %d!!\n",
								    __func__, throttling_pre_stat);
			}
			mtk_cooler_amdtxctrl_dprintk("case0 time=%d, rst=%d %d\n", below_low_time,
						     low_rst_time, is_reset);
			break;

		default:
			mtk_cooler_amdtxctrl_dprintk_always("[%s] Error cur_wifi_stat=%d!!\n",
							    __func__, cur_wifi_stat);
			break;
		}

		mail_box[0] = UNK_STAT;
		mail_box[1] = UNK_STAT;
	} else {
		mtk_cooler_amdtxctrl_dprintk("[%s] dont get all info!!\n", __func__);
	}
	return 0;
}

/* +amdtxctrl_cooler_upper_ops+ */
static int amdtxctrl_cooler_upper_get_max_state(struct thermal_cooling_device *cool_dev,
						unsigned long *pv)
{
	*pv = 1;
	mtk_cooler_amdtxctrl_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amdtxctrl_cooler_upper_get_cur_state(struct thermal_cooling_device *cool_dev,
						unsigned long *pv)
{
	*pv = cl_upper_dev_state;
	mtk_cooler_amdtxctrl_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amdtxctrl_cooler_upper_set_cur_state(struct thermal_cooling_device *cool_dev,
						unsigned long v)
{
	int ret = 0;

	mtk_cooler_amdtxctrl_dprintk("[%s] %d\n", __func__, v);

	cl_upper_dev_state = (unsigned int)v;

	if (cl_upper_dev_state == 1)
		ret = judge_throttling(1, 1, polling_interval);
	else
		ret = judge_throttling(1, 0, polling_interval);

	if (ret != 0)
		mtk_cooler_amdtxctrl_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops amdtxctrl_cooler_upper_ops = {
	.get_max_state = amdtxctrl_cooler_upper_get_max_state,
	.get_cur_state = amdtxctrl_cooler_upper_get_cur_state,
	.set_cur_state = amdtxctrl_cooler_upper_set_cur_state,
};

/* -amdtxctrl_cooler_upper_ops- */

/* +amdtxctrl_cooler_lower_ops+ */
static int amdtxctrl_cooler_lower_get_max_state(struct thermal_cooling_device *cool_dev,
						unsigned long *pv)
{
	*pv = 1;
	mtk_cooler_amdtxctrl_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amdtxctrl_cooler_lower_get_cur_state(struct thermal_cooling_device *cool_dev,
						unsigned long *pv)
{
	*pv = cl_lower_dev_state;
	mtk_cooler_amdtxctrl_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amdtxctrl_cooler_lower_set_cur_state(struct thermal_cooling_device *cool_dev,
						unsigned long v)
{
	int ret = 0;

	mtk_cooler_amdtxctrl_dprintk("[%s] %d\n", __func__, v);

	cl_lower_dev_state = (unsigned int)v;

	if (cl_lower_dev_state == 1)
		ret = judge_throttling(0, 1, polling_interval);
	else
		ret = judge_throttling(0, 0, polling_interval);

	if (ret != 0)
		mtk_cooler_amdtxctrl_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops amdtxctrl_cooler_lower_ops = {
	.get_max_state = amdtxctrl_cooler_lower_get_max_state,
	.get_cur_state = amdtxctrl_cooler_lower_get_cur_state,
	.set_cur_state = amdtxctrl_cooler_lower_set_cur_state,
};

/* -amdtxctrl_cooler_lower_ops- */

static int mtk_cooler_amdtxctrl_register_ltf(void)
{
	mtk_cooler_amdtxctrl_dprintk("[%s]\n", __func__);

	cl_upper_dev = mtk_thermal_cooling_device_register("cl-amdtxctrl-upper", NULL,
							   &amdtxctrl_cooler_upper_ops);

	cl_lower_dev = mtk_thermal_cooling_device_register("cl-amdtxctrl-lower", NULL,
							   &amdtxctrl_cooler_lower_ops);

	return 0;
}

static void mtk_cooler_amdtxctrl_unregister_ltf(void)
{
	mtk_cooler_amdtxctrl_dprintk("[%s]\n", __func__);

	if (cl_upper_dev) {
		mtk_thermal_cooling_device_unregister(cl_upper_dev);
		cl_upper_dev = NULL;
	}

	if (cl_lower_dev) {
		mtk_thermal_cooling_device_unregister(cl_lower_dev);
		cl_lower_dev = NULL;
	}
}

/*New Wifi throttling Algo+*/
int amdtxctrl_param_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret;
	char tmp[MAX_LEN] = { 0 };

	sprintf(tmp, "[up]\t%3d(sec)\t%2d\n[low]\t%3d(sec)\t%2d\nrst=%2d\ninterval=%d\n",
		up_duration, up_step, low_duration, low_step, low_rst_max, polling_interval);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret * sizeof(char));

	mtk_cooler_amdtxctrl_dprintk_always("[%s] [up]%d %d, [low]%d %d, rst=%d, interval=%d\n",
					    __func__, up_duration, up_step, low_duration, low_step,
					    low_rst_max, polling_interval);

	return ret;
}

ssize_t amdtxctrl_param_write(struct file *filp, const char __user *buf, unsigned long len,
			      void *data)
{
	char desc[MAX_LEN] = { 0 };

	unsigned int tmp_up_dur = 10;
	unsigned int tmp_up_step = 1;

	unsigned int tmp_low_dur = 10;
	unsigned int tmp_low_step = 1;

	unsigned int tmp_low_rst_max = 6;

	int tmp_polling_interval = 1;

	unsigned int tmp_log = 0;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (sscanf(desc, "%d %d %d %d %d %d", &tmp_up_dur, &tmp_up_step, &tmp_low_dur,
		   &tmp_low_step, &tmp_low_rst_max, &tmp_polling_interval) == 6) {

		up_duration = tmp_up_dur;
		up_step = tmp_up_step;

		low_duration = tmp_low_dur;
		low_step = tmp_low_step;

		low_rst_max = tmp_low_rst_max;
		polling_interval = tmp_polling_interval;

		over_up_time = 0;
		below_low_time = 0;
		low_rst_time = 0;

		mtk_cooler_amdtxctrl_dprintk_always
		    ("[%s] %s [up]%d %d, [low]%d %d, rst=%d, interval=%d\n", __func__, desc,
		     up_duration, up_step, low_duration, low_step, low_rst_max, polling_interval);

		return len;
	}

	if (sscanf(desc, "log=%d", &tmp_log) == 1) {
		if (tmp_log == 1)
			cl_amdtxctrl_klog_on = 1;
		else
			cl_amdtxctrl_klog_on = 0;

		return len;
	}
	mtk_cooler_amdtxctrl_dprintk_always("[%s] bad argument = %s\n", __func__, desc);
	return -EINVAL;
}

/*New Wifi throttling Algo-*/

#if CONFIG_SIGNAL_USER_SPACE
int amdtxctrl_pid_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret;
	char tmp[MAX_LEN] = { 0 };

	sprintf(tmp, "%d\n", tm_input_pid);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret * sizeof(char));

	mtk_cooler_amdtxctrl_dprintk_always("[%s] %s = %d\n", __func__, buf, tm_input_pid);

	return ret;
}

ssize_t amdtxctrl_pid_write(struct file *filp, const char __user *buf, unsigned long count,
			    void *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };
	int len = 0;

	/* write data to the buffer */
	len = (count < (sizeof(tmp) - 1)) ? count : (sizeof(tmp) - 1);
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &tm_input_pid);
	if (ret)
		WARN_ON(1);

	mtk_cooler_amdtxctrl_dprintk_always("[%s] %s = %d\n", __func__, tmp, tm_input_pid);

	return len;
}
#endif				/* CONFIG_SIGNAL_USER_SPACE */

static int amdtxctrl_proc_register(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *amdtxctrl_proc_dir = NULL;

	mtk_cooler_amdtxctrl_dprintk("[%s]\n", __func__);

	amdtxctrl_proc_dir = proc_mkdir("amdtxctrl", NULL);
	if (!amdtxctrl_proc_dir) {
		mtk_cooler_amdtxctrl_dprintk("[%s] mkdir /proc/amdtxctrl failed\n", __func__);
	} else {
#if CONFIG_SIGNAL_USER_SPACE
		entry =
		    create_proc_entry("tm_pid", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
				      amdtxctrl_proc_dir);
		if (entry) {
			entry->read_proc = amdtxctrl_pid_read;
			entry->write_proc = amdtxctrl_pid_write;
			entry->gid = 1000;	/* allow system process to write this proc */
		}
#endif				/* CONFIG_SIGNAL_USER_SPACE */

		entry =
		    create_proc_entry("amdtxctrl_param", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
				      amdtxctrl_proc_dir);
		if (entry) {
			entry->read_proc = amdtxctrl_param_read;
			entry->write_proc = amdtxctrl_param_write;
			entry->gid = 1000;	/* allow system process to write this proc */
		}
	}
	return 0;
}

static int __init mtk_cooler_amdtxctrl_init(void)
{
	int err = 0, i = 0;

	mtk_cooler_amdtxctrl_dprintk("[%s]\n", __func__);

	for (; i < COOLER_STEPS; i++)
		opp_func[i] = amdtxpwr_backoff;

	amdtxctrl.cur_level = 0;
	amdtxctrl.max_level = COOLER_STEPS - 1;
	amdtxctrl.opp_func_array = &opp_func[0];

	err = amdtxctrl_proc_register();
	if (err)
		return err;

	err = mtk_cooler_amdtxctrl_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_amdtxctrl_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_amdtxctrl_exit(void)
{
	mtk_cooler_amdtxctrl_dprintk("[%s]\n", __func__);

	/* remove the proc file */
	/* remove_proc_entry("amdtxctrl", NULL); */

	mtk_cooler_amdtxctrl_unregister_ltf();
}
module_init(mtk_cooler_amdtxctrl_init);
module_exit(mtk_cooler_amdtxctrl_exit);
