/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
#include "mt-plat/mtk_thermal_monitor.h"

static int cl_debug_flag;

#define mtk_cooler_3Gmutt_dprintk_always(fmt, args...)pr_notice("[thermal/cooler/3Gmutt]" fmt, ##args)


#define mtk_cooler_3Gmutt_dprintk(fmt, args...) \
do { \
	if (1 == cl_mutt_klog_on) \
		pr_debug("[thermal/cooler/3Gmutt]" fmt, ##args); \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT  4

#define MTK_CL_3GMUTT_GET_LIMIT(limit, state) \
{ (limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_3GMUTT_SET_LIMIT(limit, state) \
{ state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_3GMUTT_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_3GMUTT_SET_CURR_STATE(curr_state, state) \
do { \
	if (0 == curr_state) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static int cl_mutt_klog_on;
static struct thermal_cooling_device *cl_mutt_dev[MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT] = { 0 };
static unsigned int cl_mutt_param[MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT] = { 0 };
static unsigned long cl_mutt_state[MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT] = { 0 };

static unsigned int cl_mutt_cur_limit;

static unsigned long last_md_boot_cnt;

static unsigned int tm_pid;
static unsigned int tm_input_pid_3Gtest;
static struct task_struct g_task;
static struct task_struct *pg_task = &g_task;
#define MAX_LEN	256
/* unsigned int tm_input_pid_3Gtest= 0; */

int mddulthro_pid_read(struct seq_file *m, void *v)
{
	/* int ret; */
	/* char tmp[MAX_LEN] = {0}; */

	seq_printf(m, "%d\n", tm_input_pid_3Gtest);
	/* ret = strlen(tmp); */

	/* memcpy(buf, tmp, ret*sizeof(char)); */

	mtk_cooler_3Gmutt_dprintk_always("[%s] %d\n", __func__, tm_input_pid_3Gtest);

	return 0;
}

ssize_t mddulthro_pid_write(struct file *file, const char __user *buf, size_t count, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };
	int len = 0;

	/* write data to the buffer */
	len = (count < (sizeof(tmp) - 1)) ? count : (sizeof(tmp) - 1);
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;


	ret = kstrtouint(tmp, 10, &tm_input_pid_3Gtest);
	if (ret)
		WARN_ON(1);

	mtk_cooler_3Gmutt_dprintk_always("[%s] %s = %d\n", __func__, tmp, tm_input_pid_3Gtest);

	return len;
}

static int mddulthro_pid_open(struct inode *inode, struct file *file)
{
	/* return single_open(file, mddulthro_pid_read, NULL); */
	return single_open(file, mddulthro_pid_read, PDE_DATA(inode));
}

static const struct file_operations mddulthro_pid_fops = {
	.owner = THIS_MODULE,
	.open = mddulthro_pid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mddulthro_pid_write,
	.release = single_release,
};


static int mutt3G_send_signal(int level, int level2)
{
	int ret = 0;
	int thro = level;
	int thro2 = level2;
/* g_limit_tput = level; */
	mtk_cooler_3Gmutt_dprintk_always("%s +++ ,level=%d,level2=%d\n", __func__, level, level2);

	if (tm_input_pid_3Gtest == 0) {
		mtk_cooler_3Gmutt_dprintk_always("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_3Gmutt_dprintk_always("[%s] pid is %d, %d, %d, %d\n", __func__, tm_pid,
					 tm_input_pid_3Gtest, thro, thro2);

	if (ret == 0 && tm_input_pid_3Gtest != tm_pid) {
		tm_pid = tm_input_pid_3Gtest;
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
		mtk_cooler_3Gmutt_dprintk_always("[%s] line %d\n", __func__, __LINE__);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 4;
		info.si_code = thro;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_cooler_3Gmutt_dprintk_always("[%s] ret=%d\n", __func__, ret);

	return ret;
}


static void mtk_cl_mutt_set_mutt_limit(void)
{
	/* TODO: optimize */
	int i = 0, ret = 0;
	int min_limit = 255;
	unsigned int min_param = 0;
	unsigned int md_active = 0;
	unsigned int md_suspend = 0;
	unsigned int md_final = 0;
	int limit = 0;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i++) {
		unsigned long curr_state;

		MTK_CL_3GMUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);
		mtk_cooler_3Gmutt_dprintk_always("[%s] curr_state = %lu\n", __func__, curr_state);

		if (1 == curr_state) {

			md_active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
			md_suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

			md_final = (cl_mutt_param[i] & 0x000FFFF0) >> 4;

			/* mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d ,active=0x%x,suspend=0x%x\n",
				__func__, i,md_active,md_suspend); */

			/* a cooler with 0 active or 0 suspend is not allowed */
			if (md_active == 0 || md_suspend == 0)
				goto err_unreg;

			/* compare the active/suspend ratio */
			if (md_active >= md_suspend)
				limit = md_active / md_suspend;
			else
				limit = (0 - md_suspend) / md_active;

			if (limit <= min_limit) {
				min_limit = limit;
				min_param = cl_mutt_param[i];
			}
		} else {
			/* mutt3G_send_signal(-1,-1); */
			/* mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d ,active=0x%x,suspend=0x%d\n",
				__func__, i,md_active,md_suspend); */
		}
	}
	mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d ,min_param=%x,cl_mutt_cur_limit=%x\n", __func__,
					 i, min_param, cl_mutt_cur_limit);
	mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d ,md_final=0x%x,active=0x%x,suspend=0x%x\n",
					 __func__, i, md_final, md_active, md_suspend);

	if (min_param != cl_mutt_cur_limit) {
		cl_mutt_cur_limit = min_param;
/* last_md_boot_cnt = ccci_get_md_boot_count(MD_SYS1); */
/* ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG, (char*) &cl_mutt_cur_limit, 4); */
		mutt3G_send_signal(md_final, cl_mutt_cur_limit);
		mtk_cooler_3Gmutt_dprintk_always("[%s] ret %d param %x bcnt %lul\n", __func__, ret,
						 cl_mutt_cur_limit, last_md_boot_cnt);
	}

err_unreg:
	return;

}

static int mtk_cl_mutt_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_3Gmutt_dprintk("mtk_cl_mutt_get_max_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_mutt_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_3GMUTT_GET_CURR_STATE(*state, *((unsigned long *)cdev->devdata));
	mtk_cooler_3Gmutt_dprintk("mtk_cl_mutt_get_cur_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_mutt_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mtk_cooler_3Gmutt_dprintk("mtk_cl_mutt_set_cur_state() %s %lu pid=%d\n", cdev->type, state,
				  tm_input_pid_3Gtest);

	MTK_CL_3GMUTT_SET_CURR_STATE(state, *((unsigned long *)cdev->devdata));
	mtk_cl_mutt_set_mutt_limit();

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_mutt_ops = {
	.get_max_state = mtk_cl_mutt_get_max_state,
	.get_cur_state = mtk_cl_mutt_get_cur_state,
	.set_cur_state = mtk_cl_mutt_set_cur_state,
};

static int mtk_cooler_mutt_register_ltf(void)
{
	int i;

	mtk_cooler_3Gmutt_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-3gmutt%02d", i);
		cl_mutt_dev[i] = mtk_thermal_cooling_device_register(temp,
			(void *)&cl_mutt_state[i],  /* put mutt state to cooler devdata */
			&mtk_cl_mutt_ops);
	}
	mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d\n", __func__, i);
	return 0;
}

static void mtk_cooler_mutt_unregister_ltf(void)
{
	int i;

	mtk_cooler_3Gmutt_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i-- > 0;) {
		if (cl_mutt_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_mutt_dev[i]);
			cl_mutt_dev[i] = NULL;
			cl_mutt_state[i] = 0;
		}
	}
	mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d\n", __func__, i);
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
		seq_printf(m, "curr_limit %x\n", cl_mutt_cur_limit);

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i++) {
			unsigned int active;
			unsigned int suspend;
			unsigned long curr_state;

			active = (cl_mutt_param[i] & 0x0000FF00) >> 8;
			suspend = (cl_mutt_param[i] & 0x00FF0000) >> 16;

			MTK_CL_3GMUTT_GET_CURR_STATE(curr_state, cl_mutt_state[i]);

			seq_printf(m, "mtk-cl-3gmutt%02d %u %u %x, state %lu\n", i, active, suspend,
				   cl_mutt_param[i], curr_state);
		}
		mtk_cooler_3Gmutt_dprintk_always("[%s]i= %d\n", __func__, i);

	}


	return 0;
}

static ssize_t _mtk_cl_mutt_proc_write(struct file *filp, const char __user *buffer, size_t count,
				       loff_t *data)
{
	int len = 0;
	char desc[128];
	int klog_on, mutt0_a, mutt0_s, mutt1_a, mutt1_s, mutt2_a, mutt2_s, mutt3_a, mutt3_s;

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
		mtk_cooler_3Gmutt_dprintk("[%s] null data\n", __func__);
		return -EINVAL;
	}
	/* WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 4 */
#if (4 == MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT)
	/* cl_mutt_param[0] = 0; */
	/* cl_mutt_param[1] = 0; */
	/* cl_mutt_param[2] = 0; */

	if (1 <= sscanf(desc, "%d %d %d %d %d %d %d %d %d",
			&klog_on, &mutt0_a, &mutt0_s, &mutt1_a, &mutt1_s, &mutt2_a, &mutt2_s, &mutt3_a, &mutt3_s)) {

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

		return count;
	}
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_MUTT!"
#endif
	mtk_cooler_3Gmutt_dprintk("[%s] bad arg\n", __func__);
	return -EINVAL;
}

static int _mtk_cl_mutt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtk_cl_mutt_proc_read, NULL);
}

static const struct file_operations cl_3gmutt_fops = {
	.owner = THIS_MODULE,
	.open = _mtk_cl_mutt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtk_cl_mutt_proc_write,
	.release = single_release,
};

static int cl_debug_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[ cl_debug_read] cl_debug_flag = %d\n", cl_debug_flag);


	return 0;
}
static ssize_t cl_debug_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int cl_debug_flag_switch;
	int cl_debug_flag_switch1;
	int cl_debug_flag_switch2;
	int len = 0;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';




	if (sscanf(desc, "%d %d %d", &cl_debug_flag_switch, &cl_debug_flag_switch1, &cl_debug_flag_switch2) == 3) {
		cl_debug_flag = cl_debug_flag_switch;
		mtk_cooler_3Gmutt_dprintk("cl_debug_write cl_debug_flag=%d\n", cl_debug_flag);

		/*return count;*/
	} else {
		mtk_cooler_3Gmutt_dprintk("cl_debug_write bad argument\n");
	}
	return -EINVAL;

}



static int cl_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cl_debug_read, NULL);
}

static const struct file_operations cl_debug_fops = {
	.owner = THIS_MODULE,
	.open = cl_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = cl_debug_write,
	.release = single_release,
};

static int __init mtk_cooler_mutt_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_3GMUTT; i-- > 0;) {
		cl_mutt_dev[i] = NULL;
		cl_mutt_state[i] = 0;
	}

	mtk_cooler_3Gmutt_dprintk("init\n");

	err = mtk_cooler_mutt_register_ltf();
	if (err)
		goto err_unreg;

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry) {
			mtk_cooler_3Gmutt_dprintk_always
			    ("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
		} else {

			entry =
			    proc_create("cldebug", S_IRUGO | S_IWUSR | S_IWGRP, dir_entry,
					&cl_debug_fops);

			if (entry)
				proc_set_user(entry, uid, gid);


			entry =
			    proc_create("cl3gmutt", S_IRUGO | S_IWUSR | S_IWGRP, dir_entry,
					&cl_3gmutt_fops);
			if (entry)
				proc_set_user(entry, uid, gid);

			entry =
			    proc_create("tm_3Gpid", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, dir_entry,
					&mddulthro_pid_fops);
			if (entry)
				proc_set_user(entry, uid, gid);

		}
	}

	return 0;

err_unreg:
	mtk_cooler_mutt_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_mutt_exit(void)
{
	mtk_cooler_3Gmutt_dprintk("exit\n");

	/* remove the proc file */
	remove_proc_entry("clmutt", NULL);

	mtk_cooler_mutt_unregister_ltf();
}
module_init(mtk_cooler_mutt_init);
module_exit(mtk_cooler_mutt_exit);
