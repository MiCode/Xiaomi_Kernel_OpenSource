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
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/uidgid.h>

#define CONFIG_SIGNAL_USER_SPACE    (1)

#if CONFIG_SIGNAL_USER_SPACE
#include <linux/pid.h>
#endif

#define cl_type_upper               "cl-amddulthro-u"
#define cl_type_lower               "cl-amddulthro-l"

#define mtk_cooler_amddulthro_dprintk_always(fmt, args...) \
pr_debug("thermal/cooler/amddulthro" fmt, ##args)

#define mtk_cooler_amddulthro_dprintk(fmt, args...) \
do { \
	if (1 == cl_amddulthro_klog_on) { \
		pr_debug("thermal/cooler/amddulthro" fmt, ##args);\
	} \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO  1

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int cl_amddulthro_klog_on;

/* over_up_time * polling interval > up_duration --> throttling */
static unsigned int over_up_time;	/* polling time */
static unsigned int up_duration = 30;	/* sec */
static unsigned int up_step = 1;	/* step */

/* below_low_time * polling interval > low_duration --> throttling */
static unsigned int below_low_time;	/* polling time */
static unsigned int low_duration = 20;	/* sec */
static unsigned int low_step = 1;	/* step */

static unsigned int low_rst_time;
static unsigned int low_rst_max = 3;

/* static unsigned int deepest_step = 0; */

static int polling_interval = 1;	/* second */

#define UNK_STAT -1
#define LOW_STAT 0
#define MID_STAT 1
#define HIGH_STAT 2

#define MAX_LEN	256
#define COOLER_STEPS 10

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

static adaptive_coolers amddulthro;

#if CONFIG_SIGNAL_USER_SPACE
static int wmt_send_signal(int level)
{
	int ret = 0;
	int thro = level;

	if (tm_input_pid == 0) {
		mtk_cooler_amddulthro_dprintk("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_amddulthro_dprintk_always("[%s] pid is %d, %d, %d\n", __func__, tm_pid,
					     tm_input_pid, thro);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 2;	/* for md dul throttling */
		info.si_code = thro;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_cooler_amddulthro_dprintk("[%s] ret=%d\n", __func__, ret);

	return ret;
}
#endif				/* CONFIG_SIGNAL_USER_SPACE */


int amddulthro_backoff(int level)
{
	int ret;

	if (level == 0) {
		/* no throttle */
		wmt_send_signal(10);
		mtk_cooler_amddulthro_dprintk_always("[%s] unlimit mddulthro\n", __func__);

	} else if (level >= 1 && level <= 9) {
		/* throttle */
		wmt_send_signal(COOLER_STEPS - level);
		mtk_cooler_amddulthro_dprintk_always("[%s] limit mddulthro %d\n", __func__,
						     COOLER_STEPS - level);
	} else {
		/* error... */
		ret = -1;
		mtk_cooler_amddulthro_dprintk_always("[%s] ouf of range\n", __func__);
	}

	return ret;

}
EXPORT_SYMBOL(amddulthro_backoff);

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

	mtk_cooler_amddulthro_dprintk("[%s]+ [0]=%d, [1]=%d || [%d] is %s\n", __func__, mail_box[0],
				      mail_box[1], index, (is_on == 1 ? "ON" : "OFF"));
	mail_box[index] = is_on;

	if (mail_box[0] >= 0 && mail_box[1] >= 0) {
		cur_wifi_stat = mail_box[0] + mail_box[1];

		switch (cur_wifi_stat) {
		case HIGH_STAT:
			if (throttling_pre_stat < HIGH_STAT) {
				/* 1st down throttle */
				int new_step = down_throttle(&amddulthro, up_step);

				mtk_cooler_amddulthro_dprintk_always("LOW/MID-->HIGH: step %d\n",
								     new_step);

				throttling_pre_stat = HIGH_STAT;
				over_up_time = 0;
			} else if (throttling_pre_stat == HIGH_STAT) {
				/* keep down throttle */
				over_up_time++;
				if ((over_up_time * interval) >= up_duration) {
					int new_step = down_throttle(&amddulthro, up_step);

					mtk_cooler_amddulthro_dprintk_always
					    ("HIGH-->HIGH: step %d\n", new_step);

					over_up_time = 0;
				}
			} else {
				mtk_cooler_amddulthro_dprintk("[%s] Error state1!!\n", __func__,
							      throttling_pre_stat);
			}
			mtk_cooler_amddulthro_dprintk_always("case2 time=%d\n", over_up_time);
			break;

		case MID_STAT:
			if (throttling_pre_stat == LOW_STAT) {
				below_low_time = 0;
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amddulthro_dprintk_always("[%s] Go up!!\n", __func__);
			} else if (throttling_pre_stat == HIGH_STAT) {
				over_up_time = 0;
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amddulthro_dprintk_always("[%s] Go down!!\n", __func__);
			} else {
				throttling_pre_stat = MID_STAT;
				mtk_cooler_amddulthro_dprintk("[%s] pre_stat=%d!!\n", __func__,
							      throttling_pre_stat);
			}
			break;

		case LOW_STAT:
			if (throttling_pre_stat > LOW_STAT) {
				/* 1st up throttle */
				int new_step = up_throttle(&amddulthro, low_step);

				mtk_cooler_amddulthro_dprintk_always("MID/HIGH-->LOW: step %d\n",
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
						rst_throttle(&amddulthro);

						mtk_cooler_amddulthro_dprintk_always
						    ("over rst time=%d\n", low_rst_time);

						low_rst_time = low_rst_max;
						is_reset = true;
					} else if (!is_reset) {
						/* keep up throttle */
						int new_step = up_throttle(&amddulthro, low_step);

						low_rst_time++;

						mtk_cooler_amddulthro_dprintk_always
						    ("LOW-->LOW: step %d\n", new_step);

						below_low_time = 0;
					} else {
						mtk_cooler_amddulthro_dprintk
						    ("Have reset, no control!!");
					}
				}
			} else {
				mtk_cooler_amddulthro_dprintk_always("[%s] Error state3 %d!!\n",
								     __func__, throttling_pre_stat);
			}
			mtk_cooler_amddulthro_dprintk("case0 time=%d, rst=%d %d\n", below_low_time,
						      low_rst_time, is_reset);
			break;

		default:
			mtk_cooler_amddulthro_dprintk_always("[%s] Error cur_wifi_stat=%d!!\n",
							     __func__, cur_wifi_stat);
			break;
		}

		mail_box[0] = UNK_STAT;
		mail_box[1] = UNK_STAT;
	} else {
		mtk_cooler_amddulthro_dprintk("[%s] dont get all info!!\n", __func__);
	}
	return 0;
}

/* +amddulthro_cooler_upper_ops+ */
static int amddulthro_cooler_upper_get_max_state(struct thermal_cooling_device *cool_dev,
						 unsigned long *pv)
{
	*pv = 1;
	mtk_cooler_amddulthro_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amddulthro_cooler_upper_get_cur_state(struct thermal_cooling_device *cool_dev,
						 unsigned long *pv)
{
	*pv = cl_upper_dev_state;
	mtk_cooler_amddulthro_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amddulthro_cooler_upper_set_cur_state(struct thermal_cooling_device *cool_dev,
						 unsigned long v)
{
	int ret = 0;

	mtk_cooler_amddulthro_dprintk("[%s] %d\n", __func__, v);

	cl_upper_dev_state = (unsigned int)v;

	if (cl_upper_dev_state == 1)
		ret = judge_throttling(1, 1, polling_interval);
	else
		ret = judge_throttling(1, 0, polling_interval);

	if (ret != 0)
		mtk_cooler_amddulthro_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops amddulthro_cooler_upper_ops = {
	.get_max_state = amddulthro_cooler_upper_get_max_state,
	.get_cur_state = amddulthro_cooler_upper_get_cur_state,
	.set_cur_state = amddulthro_cooler_upper_set_cur_state,
};

/* -amddulthro_cooler_upper_ops- */

/* +amddulthro_cooler_lower_ops+ */
static int amddulthro_cooler_lower_get_max_state(struct thermal_cooling_device *cool_dev,
						 unsigned long *pv)
{
	*pv = 1;
	mtk_cooler_amddulthro_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amddulthro_cooler_lower_get_cur_state(struct thermal_cooling_device *cool_dev,
						 unsigned long *pv)
{
	*pv = cl_lower_dev_state;
	mtk_cooler_amddulthro_dprintk("[%s] %d\n", __func__, *pv);
	return 0;
}

static int amddulthro_cooler_lower_set_cur_state(struct thermal_cooling_device *cool_dev,
						 unsigned long v)
{
	int ret = 0;

	mtk_cooler_amddulthro_dprintk("[%s] %d\n", __func__, v);

	cl_lower_dev_state = (unsigned int)v;

	if (cl_lower_dev_state == 1)
		ret = judge_throttling(0, 1, polling_interval);
	else
		ret = judge_throttling(0, 0, polling_interval);

	if (ret != 0)
		mtk_cooler_amddulthro_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops amddulthro_cooler_lower_ops = {
	.get_max_state = amddulthro_cooler_lower_get_max_state,
	.get_cur_state = amddulthro_cooler_lower_get_cur_state,
	.set_cur_state = amddulthro_cooler_lower_set_cur_state,
};

/* -amddulthro_cooler_lower_ops- */

static int mtk_cooler_amddulthro_register_ltf(void)
{
	mtk_cooler_amddulthro_dprintk("[%s]\n", __func__);

	cl_upper_dev = mtk_thermal_cooling_device_register("cl-amddulthro-upper", NULL,
							   &amddulthro_cooler_upper_ops);

	cl_lower_dev = mtk_thermal_cooling_device_register("cl-amddulthro-lower", NULL,
							   &amddulthro_cooler_lower_ops);

	return 0;
}

static void mtk_cooler_amddulthro_unregister_ltf(void)
{
	mtk_cooler_amddulthro_dprintk("[%s]\n", __func__);

	if (cl_upper_dev) {
		mtk_thermal_cooling_device_unregister(cl_upper_dev);
		cl_upper_dev = NULL;
	}

	if (cl_lower_dev) {
		mtk_thermal_cooling_device_unregister(cl_lower_dev);
		cl_lower_dev = NULL;
	}
}

int amddulthro_param_read(struct seq_file *m, void *v)
{
	seq_printf(m,
		   "[up]\t%3d(sec)\t%2d\n[low]\t%3d(sec)\t%2d\nrst=%2d\ninterval=%d\nmax_step=%d",
		   up_duration, up_step, low_duration, low_step, low_rst_max, polling_interval,
		   amddulthro.max_level);
#if 0
	int ret;
	char tmp[MAX_LEN] = { 0 };

	sprintf(tmp, "[up]\t%3d(sec)\t%2d\n[low]\t%3d(sec)\t%2d\nrst=%2d\ninterval=%d\nmax_step=%d",
		up_duration, up_step, low_duration, low_step, low_rst_max, polling_interval,
		amddulthro.max_level);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret * sizeof(char));
#endif
	mtk_cooler_amddulthro_dprintk_always
	    ("[%s] [up]%d %d, [low]%d %d, rst=%d, interval=%d, max_step=%d\n", __func__,
	     up_duration, up_step, low_duration, low_step, low_rst_max, polling_interval,
	     amddulthro.max_level);

	return 0;
}

ssize_t amddulthro_param_write(struct file *file, const char __user *buf, size_t len,
			       loff_t *data)
{
	char desc[MAX_LEN] = { 0 };

	unsigned int tmp_up_dur = 30;
	unsigned int tmp_up_step = 1;

	unsigned int tmp_low_dur = 20;
	unsigned int tmp_low_step = 1;

	unsigned int tmp_low_rst_max = 3;

	int tmp_polling_interval = 1;

	unsigned int tmp_deepest_step = COOLER_STEPS - 1;

	unsigned int tmp_log = 0;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (sscanf(desc, "%d %d %d %d %d %d %d", &tmp_up_dur, &tmp_up_step, &tmp_low_dur,
		   &tmp_low_step, &tmp_low_rst_max, &tmp_polling_interval,
		   &tmp_deepest_step) >= 6) {

		up_duration = tmp_up_dur;
		up_step = tmp_up_step;

		low_duration = tmp_low_dur;
		low_step = tmp_low_step;

		low_rst_max = tmp_low_rst_max;
		polling_interval = tmp_polling_interval;

		if (tmp_deepest_step > 0 && tmp_deepest_step < COOLER_STEPS)
			amddulthro.max_level = tmp_deepest_step;

		over_up_time = 0;
		below_low_time = 0;
		low_rst_time = 0;

		mtk_cooler_amddulthro_dprintk_always
		    ("[%s] %s [up]%d %d, [low]%d %d, rst=%d, interval=%d, max_step=%d\n", __func__,
		     desc, up_duration, up_step, low_duration, low_step, low_rst_max,
		     polling_interval, amddulthro.max_level);

		return len;
	} else if (sscanf(desc, "log=%d", &tmp_log) == 1) {
		if (tmp_log == 1)
			cl_amddulthro_klog_on = 1;
		else
			cl_amddulthro_klog_on = 0;

		return len;
	}
	mtk_cooler_amddulthro_dprintk_always("[%s] bad argument = %s\n", __func__, desc);
	return -EINVAL;
}

static int amddulthro_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, amddulthro_param_read, NULL);
}

static const struct file_operations amddulthro_param_fops = {
	.owner = THIS_MODULE,
	.open = amddulthro_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = amddulthro_param_write,
	.release = single_release,
};

#if CONFIG_SIGNAL_USER_SPACE
int amddulthro_pid_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", tm_input_pid);
#if 0
	int ret;
	char tmp[MAX_LEN] = { 0 };

	sprintf(tmp, "%d\n", tm_input_pid);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret * sizeof(char));
#endif
	mtk_cooler_amddulthro_dprintk_always("[%s] %d\n", __func__, tm_input_pid);

	return 0;
}

ssize_t amddulthro_pid_write(struct file *file, const char __user *buf, size_t count, loff_t *data)
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

	mtk_cooler_amddulthro_dprintk_always("[%s] %s = %d\n", __func__, tmp, tm_input_pid);

	return len;
}

static int amddulthro_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, amddulthro_pid_read, NULL);
}

static const struct file_operations amddulthro_pid_fops = {
	.owner = THIS_MODULE,
	.open = amddulthro_pid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = amddulthro_pid_write,
	.release = single_release,
};
#endif				/* CONFIG_SIGNAL_USER_SPACE */

int amddulthro_dbg_read(struct seq_file *m, void *v)
{
	seq_printf(m, "cur=%d max=%d\n", amddulthro.cur_level, amddulthro.max_level);
#if 0
	int ret;
	char tmp[MAX_LEN] = { 0 };

	sprintf(tmp, "cur=%d max=%d\n", amddulthro.cur_level, amddulthro.max_level);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret * sizeof(char));
#endif
	mtk_cooler_amddulthro_dprintk_always("[%s] cur=%d max=%d\n", __func__, amddulthro.cur_level,
					     amddulthro.max_level);

	return 0;
}

ssize_t amddulthro_dbg_write(struct file *file, const char __user *buf, size_t len, loff_t *data)
{
	char desc[MAX_LEN] = { 0 };

	int new_level = -1;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (kstrtoint(desc, 10, &new_level) == 0) {

		if (new_level >= 0 && new_level < COOLER_STEPS) {
			/* valid input */
			mtk_cooler_amddulthro_dprintk_always("[%s] new level %d\n", __func__,
							     new_level);
			amddulthro.cur_level = new_level;
			amddulthro.opp_func_array[new_level] (new_level);
		} else
			mtk_cooler_amddulthro_dprintk_always("[%s] invalid %d\n", __func__,
							     new_level);

		return len;
	}
	mtk_cooler_amddulthro_dprintk_always("[%s] bad argument = %s\n", __func__, desc);
	return -EINVAL;
}

static int amddulthro_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, amddulthro_dbg_read, NULL);
}

static const struct file_operations amddulthro_dbg_fops = {
	.owner = THIS_MODULE,
	.open = amddulthro_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = amddulthro_dbg_write,
	.release = single_release,
};

static int amddulthro_proc_register(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *amddulthro_proc_dir = NULL;

	mtk_cooler_amddulthro_dprintk("[%s]\n", __func__);

	amddulthro_proc_dir = proc_mkdir("amddulthro", NULL);
	if (!amddulthro_proc_dir) {
		mtk_cooler_amddulthro_dprintk("[%s] mkdir /proc/amddulthro failed\n", __func__);
	} else {
#if CONFIG_SIGNAL_USER_SPACE
		entry =
		    proc_create("tm_pid", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
				amddulthro_proc_dir, &amddulthro_pid_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("amddulthro_param", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
				amddulthro_proc_dir, &amddulthro_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("amddulthro_dbg", S_IRUSR | S_IWUSR, amddulthro_proc_dir,
				&amddulthro_dbg_fops);
	}
	return 0;
}

static int __init mtk_cooler_amddulthro_init(void)
{
	int err = 0, i = 0;

	mtk_cooler_amddulthro_dprintk("[%s]\n", __func__);

	for (; i < COOLER_STEPS; i++)
		opp_func[i] = amddulthro_backoff;

	amddulthro.cur_level = 0;
	amddulthro.max_level = COOLER_STEPS - 1;
	amddulthro.opp_func_array = &opp_func[0];

	err = amddulthro_proc_register();
	if (err)
		return err;

	err = mtk_cooler_amddulthro_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_amddulthro_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_amddulthro_exit(void)
{
	mtk_cooler_amddulthro_dprintk("[%s]\n", __func__);

	/* remove the proc file */
	/* remove_proc_entry("amddulthro", NULL); */

	mtk_cooler_amddulthro_unregister_ltf();
}
module_init(mtk_cooler_amddulthro_init);
module_exit(mtk_cooler_amddulthro_exit);
