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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <mach/mtk_eemcs_helper.h>

#define mtk_cooler_mdtxpwr_dprintk_always(fmt, args...) \
pr_debug("[Thermal/TC/mdtxpwr]" fmt, ##args)

#define mtk_cooler_mdtxpwr_dprintk(fmt, args...) \
do { \
	if (cl_mdtxpwr_klog_on == 1) \
		pr_debug("[Thermal/TC/mdtxpwr]" fmt, ##args); \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR  3

#define MTK_CL_MDTXPWR_GET_LIMIT(limit, state) \
{ (limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_MDTXPWR_SET_LIMIT(limit, state) \
{ state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_MDTXPWR_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_MDTXPWR_SET_CURR_STATE(curr_state, state) \
do { \
	if (curr_state == 0) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

static int cl_mdtxpwr_klog_on;
static struct thermal_cooling_device
		*cl_mdtxpwr_dev[MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR] = { 0 };

static unsigned long cl_mdtxpwr_state
				[MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR] = { 0 };

static int cl_mdtxpwr_cur_limit = 65535;

static void mtk_cl_mdtxpwr_set_mdtxpwr_limit(void)
{
	/* TODO: optimize */
	int i = 0;
	int min_limit = 65535;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR; i++) {
		unsigned long curr_state;

		MTK_CL_MDTXPWR_GET_CURR_STATE(curr_state, cl_mdtxpwr_state[i]);
		if (curr_state == 1) {
			int limit;

			MTK_CL_MDTXPWR_GET_LIMIT(limit, cl_mdtxpwr_state[i]);
			if ((min_limit > limit) && (limit > 0))
				min_limit = limit;
		}
	}

	if (min_limit != cl_mdtxpwr_cur_limit) {
		cl_mdtxpwr_cur_limit = min_limit;
#if 1
		if (cl_mdtxpwr_cur_limit >= 65535) {
			/* TODO: 30db as unlimit... */
			int ret = eemcs_notify_md_by_sys_msg(MD_SYS5,
						EXT_MD_TX_PWR_REDU_REQ, 30);

			mtk_cooler_mdtxpwr_dprintk_always(
					"mtk_cl_mdtxpwr_set_mdtxpwr_limit() ret %d limit=30\n",
					ret);
		} else {
			int ret =
			    eemcs_notify_md_by_sys_msg(MD_SYS5,
						EXT_MD_TX_PWR_REDU_REQ,
						cl_mdtxpwr_cur_limit);

			mtk_cooler_mdtxpwr_dprintk_always(
					"mtk_cl_mdtxpwr_set_mdtxpwr_limit() ret %d limit=%d\n",
					cl_mdtxpwr_cur_limit);
		}
#endif
	}
}

static int mtk_cl_mdtxpwr_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_mdtxpwr_dprintk("mtk_cl_mdtxpwr_get_max_state() %s %d\n",
							cdev->type, *state);
	return 0;
}

static int mtk_cl_mdtxpwr_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_MDTXPWR_GET_CURR_STATE(*state,
				*((unsigned long *)cdev->devdata));

	mtk_cooler_mdtxpwr_dprintk("mtk_cl_mdtxpwr_get_cur_state() %s %d\n",
							cdev->type, *state);

	return 0;
}

static int mtk_cl_mdtxpwr_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	mtk_cooler_mdtxpwr_dprintk("mtk_cl_mdtxpwr_set_cur_state() %s %d\n",
							cdev->type, state);

	MTK_CL_MDTXPWR_SET_CURR_STATE(state, *((unsigned long *)cdev->devdata));
	mtk_cl_mdtxpwr_set_mdtxpwr_limit();

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_mdtxpwr_ops = {
	.get_max_state = mtk_cl_mdtxpwr_get_max_state,
	.get_cur_state = mtk_cl_mdtxpwr_get_cur_state,
	.set_cur_state = mtk_cl_mdtxpwr_set_cur_state,
};

static int mtk_cooler_mdtxpwr_register_ltf(void)
{
	int i;

	mtk_cooler_mdtxpwr_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-mdtxpwr%02d", i);
		/* put mdtxpwr state to cooler devdata */
		cl_mdtxpwr_dev[i] = mtk_thermal_cooling_device_register(temp,
						(void *)&cl_mdtxpwr_state[i],
						&mtk_cl_mdtxpwr_ops);
	}

	return 0;
}

static void mtk_cooler_mdtxpwr_unregister_ltf(void)
{
	int i;

	mtk_cooler_mdtxpwr_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR; i-- > 0;) {
		if (cl_mdtxpwr_dev[i]) {
			mtk_thermal_cooling_device_unregister(
							cl_mdtxpwr_dev[i]);

			cl_mdtxpwr_dev[i] = NULL;
			cl_mdtxpwr_state[i] = 0;
		}
	}
}

static int _mtk_cl_mdtxpwr_proc_read(
char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	mtk_cooler_mdtxpwr_dprintk("[_mtk_cl_mdtxpwr_proc_read] invoked.\n");

    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-mdtxpwr<ID>> <bcc limit>
     *  ..
     */
	if (data == NULL) {
		mtk_cooler_mdtxpwr_dprintk(
				"[_mtk_cl_mdtxpwr_proc_read] null data\n");
	} else {
		int i = 0;

		p += sprintf(p, "klog %d\n", cl_mdtxpwr_klog_on);
		p += sprintf(p, "curr_limit %d\n", cl_mdtxpwr_cur_limit);

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR; i++) {
			int limit;
			unsigned int curr_state;

			MTK_CL_MDTXPWR_GET_LIMIT(limit, cl_mdtxpwr_state[i]);
			MTK_CL_MDTXPWR_GET_CURR_STATE(curr_state,
							cl_mdtxpwr_state[i]);

			p += sprintf(p, "mtk-cl-mdtxpwr%02d %d db, state %d\n",
							i, limit, curr_state);
		}
	}

	*start = buf + off;

	len = p - buf;
	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
}

static ssize_t _mtk_cl_mdtxpwr_proc_write(
struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len = 0;
	char desc[128];
	int klog_on, limit0, limit1, limit2;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	/**
	 * sscanf format
	 * <klog_on> <mtk-cl-mdtxpwr00 limit> <mtk-cl-mdtxpwr01 limit>
	 * <klog_on> can only be 0 or 1
	 * <mtk-cl-mdtxpwr00 limit> can only be positive integer
	 * or -1 to denote no limit
	 */

	if (data == NULL) {
		mtk_cooler_mdtxpwr_dprintk(
				"[_mtk_cl_mdtxpwr_proc_write] null data\n");

		return -EINVAL;
	}
	/* WARNING: Modify here if
	 * MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS
	 * is changed to other than 3
	 */
#if (3 == MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR)
	MTK_CL_MDTXPWR_SET_LIMIT(-1, cl_mdtxpwr_state[0]);
	MTK_CL_MDTXPWR_SET_LIMIT(-1, cl_mdtxpwr_state[1]);
	MTK_CL_MDTXPWR_SET_LIMIT(-1, cl_mdtxpwr_state[2]);

	if (sscanf(desc, "%d %d %d %d",
		&klog_on, &limit0, &limit1, &limit2) >= 1) {
		if (klog_on == 0 || klog_on == 1)
			cl_mdtxpwr_klog_on = klog_on;

		if (limit0 >= -1)
			MTK_CL_MDTXPWR_SET_LIMIT(limit0, cl_mdtxpwr_state[0]);
		if (limit1 >= -1)
			MTK_CL_MDTXPWR_SET_LIMIT(limit1, cl_mdtxpwr_state[1]);
		if (limit2 >= -1)
			MTK_CL_MDTXPWR_SET_LIMIT(limit2, cl_mdtxpwr_state[2]);

		return count;
	}
#else
#error	\
"Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR!"
#endif
	mtk_cooler_mdtxpwr_dprintk(
			"[_mtk_cl_mdtxpwr_proc_write] bad argument\n");

	return -EINVAL;
}

static int __init mtk_cooler_mdtxpwr_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_MDTXPWR; i-- > 0;) {
		cl_mdtxpwr_dev[i] = NULL;
		cl_mdtxpwr_state[i] = 0;
	}

	/* cl_mdtxpwr_dev = NULL; */

	mtk_cooler_mdtxpwr_dprintk("init\n");

	err = mtk_cooler_mdtxpwr_register_ltf();
	if (err)
		goto err_unreg;

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;

		entry = create_proc_entry("driver/mtk-cl-mdtxpwr", 0664, NULL);

		if (entry != NULL) {
			entry->read_proc = _mtk_cl_mdtxpwr_proc_read;
			entry->write_proc = _mtk_cl_mdtxpwr_proc_write;
			entry->data = cl_mdtxpwr_state;
			/* allow system process to write this proc */
			entry->gid = 1000;
		}
		mtk_cooler_mdtxpwr_dprintk(
				"[mtk_cooler_mdtxpwr_init] proc file created: %x\n",
				entry->data);
	}

	return 0;

err_unreg:
	mtk_cooler_mdtxpwr_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_mdtxpwr_exit(void)
{
	mtk_cooler_mdtxpwr_dprintk("exit\n");

	/* remove the proc file */
	remove_proc_entry("driver/mtk-cl-mdtxpwr", NULL);

	mtk_cooler_mdtxpwr_unregister_ltf();
}
module_init(mtk_cooler_mdtxpwr_init);
module_exit(mtk_cooler_mdtxpwr_exit);
