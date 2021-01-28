// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>

//#include <mt6779_dcm_internal.h>
//#include <mt6779_dcm_autogen.h>
#include "mtk_dcm_common.h"
#include <mtk_dcm.h>

DEFINE_MUTEX(dcm_lock);
static short dcm_debug;
static short dcm_initiated;
struct DCM *common_dcm_array;
struct DCM_OPS *common_dcm_ops;
unsigned int common_init_dcm_type;
unsigned int common_all_dcm_type;
unsigned int common_init_dcm_type_by_k;


unsigned int __attribute__((weak)) dcm_get_chip_sw_ver(void)
{
	return 0;
}

/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to recovery CURRENT_STATE before any power-off reset.
 * 2. dcm_set_default(type) to reset as cold-power-on init state.
 * 3. dcm_disable(type) to disable all dcm.
 * 4. dcm_set_state(type) to set dcm state.
 * 5. dcm_dump_state(type) to show CURRENT_STATE.
 * 6. /sys/dcm/dcm_state interface:
 *			'restore', 'disable', 'dump', 'set'. 4 commands.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/
void dcm_set_default(unsigned int type)
{
	int i;
	struct DCM *dcm;

	dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x, INIT_DCM_TYPE_BY_K=0x%x\n",
		 __func__, type, common_init_dcm_type,
		 common_init_dcm_type_by_k);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &common_dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm->saved_state = dcm->default_state;
			dcm->current_state = dcm->default_state;
			dcm->disable_refcnt = 0;
			if (common_init_dcm_type_by_k & dcm->typeid) {
				if (dcm->preset_func)
					dcm->preset_func();
				dcm->func(dcm->current_state);
			}

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}

	/*dcm_smc_msg_send(common_init_dcm_type);*/

	mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
	int i;
	struct DCM *dcm;
	unsigned int init_dcm_type_pre = common_init_dcm_type;

	dcm_pr_info("[%s]type:0x%08x, set:%d, init_dcm_type_pre=0x%x\n",
		 __func__, type, state, init_dcm_type_pre);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &common_dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->saved_state = state;
			if (dcm->disable_refcnt == 0) {
				if (state)
					common_init_dcm_type |= dcm->typeid;
				else
					common_init_dcm_type &= ~(dcm->typeid);

				dcm->current_state = state;
				dcm->func(dcm->current_state);
			}

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	if (init_dcm_type_pre != common_init_dcm_type) {
		dcm_pr_info("[%s]type:0x%08x, set:%d, init_dcm_type=0x%x->0x%x\n",
			__func__, type, state,
			init_dcm_type_pre,
			common_init_dcm_type);
		/*dcm_smc_msg_send(common_init_dcm_type);*/
	}

	mutex_unlock(&dcm_lock);
}

void dcm_disable(unsigned int type)
{
	int i;
	struct DCM *dcm;
	unsigned int init_dcm_type_pre = common_init_dcm_type;

	dcm_pr_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &common_dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->current_state = DCM_OFF;
			if (dcm->disable_refcnt++ == 0)
				common_init_dcm_type &= ~(dcm->typeid);
			dcm->func(dcm->current_state);

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	if (init_dcm_type_pre != common_init_dcm_type) {
		dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x->0x%x\n",
			 __func__, type, init_dcm_type_pre,
			 common_init_dcm_type);
		/*dcm_smc_msg_send(common_init_dcm_type);*/
	}

	mutex_unlock(&dcm_lock);

}

void dcm_restore(unsigned int type)
{
	int i;
	struct DCM *dcm;
	unsigned int init_dcm_type_pre = common_init_dcm_type;

	dcm_pr_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &common_dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->disable_refcnt > 0)
				dcm->disable_refcnt--;
			if (dcm->disable_refcnt == 0) {
				if (dcm->saved_state)
					common_init_dcm_type |= dcm->typeid;
				else
					common_init_dcm_type &= ~(dcm->typeid);

				dcm->current_state = dcm->saved_state;
				dcm->func(dcm->current_state);
			}

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	if (init_dcm_type_pre != common_init_dcm_type) {
		dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x->0x%x\n",
			 __func__, type, init_dcm_type_pre,
			 common_init_dcm_type);
		/*dcm_smc_msg_send(common_init_dcm_type);*/
	}

	mutex_unlock(&dcm_lock);
}


void dcm_dump_state(int type)
{
	int i;
	struct DCM *dcm;

	dcm_pr_info("\n******** dcm dump state *********\n");
	for (i = 0, dcm = &common_dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm_pr_info("[%-16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}
}

#ifdef CONFIG_PM
static ssize_t dcm_state_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	int len = 0;
	int i;
	struct DCM *dcm;

	/* dcm_dump_state(common_all_dcm_type); */
	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n******** dcm dump state *********\n");
	for (i = 0, dcm = &common_dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++)
		len += snprintf(buf+len, PAGE_SIZE-len,
				"[%-16s 0x%08x] current state:%d (%d)\n",
				dcm->name, dcm->typeid, dcm->current_state,
				dcm->disable_refcnt);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n********** dcm_state help *********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"set:       echo set [mask] [mode] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"disable:   echo disable [mask] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"restore:   echo restore [mask] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"dump:      echo dump [mask] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"debug:     echo debug [0/1] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mask] is hexl bit mask of dcm;\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mode] is type of DCM to set and retained\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"init_dcm_type=0x%x, all_dcm_type=0x%x, dcm_debug=%d, ",
			common_init_dcm_type, common_all_dcm_type, dcm_debug);
	len += snprintf(buf+len, PAGE_SIZE-len, "dcm_get_chip_sw_ver=0x%x\n",
			dcm_get_chip_sw_ver());

	return len;
}

static ssize_t dcm_state_store(struct kobject *kobj,
				   struct kobj_attribute *attr, const char *buf,
				   size_t n)
{
	char cmd[16];
	unsigned int mask;
	/*unsigned int val0, val1;*/
	int ret, mode;

	if (sscanf(buf, "%15s %x", cmd, &mask) == 2) {
		mask &= common_all_dcm_type;

		if (!strcmp(cmd, "restore")) {
			/* dcm_dump_regs(); */
			dcm_restore(mask);
			/* dcm_dump_regs(); */
		} else if (!strcmp(cmd, "disable")) {
			/* dcm_dump_regs(); */
			dcm_disable(mask);
			/* dcm_dump_regs(); */
		} else if (!strcmp(cmd, "dump")) {
			dcm_dump_state(mask);
			common_dcm_ops->dump_regs();
		} else if (!strcmp(cmd, "set")) {
			if (sscanf(buf, "%15s %x %d", cmd, &mask, &mode) == 3) {
				mask &= common_all_dcm_type;

				dcm_set_state(mask, mode);

				/*
				 * Log for stallDCM switching
				 * in Performance/Normal mode
				 */
				if (mask & STALL_DCM_TYPE) {
					if (mode)
						dcm_pr_info("stall dcm is enabled for Default(Normal) mode started\n");
					else
						dcm_pr_info("stall dcm is disabled for Performance(Sports) mode started\n");
				}
			}
		} else {
			dcm_pr_info("SORRY, do not support your command: %s\n",
				    cmd);
		}
		ret = n;
	} else {
		dcm_pr_info("SORRY, do not support your command.\n");
		ret = -EINVAL;
	}

	return ret;
}

static struct kobj_attribute dcm_state_attr = {
	.attr = {
		 .name = "dcm_state",
		 .mode = 0644,
		 },
	.show = dcm_state_show,
	.store = dcm_state_store,
};
static struct kobject *kobj;
#endif /* #ifdef CONFIG_PM */

int mt_dcm_common_init(void)
{

	unsigned int default_type;
	int default_state;
	int err = 0;
	/*dcm_pr_info("[%s]: dcm common init\n", __func__);*/
	if (common_dcm_ops == NULL) {
		dcm_pr_notice("[%s] dcm common ops null\n",
					__func__);
		return -1;
	}
	common_dcm_ops->dump_regs();
	common_dcm_ops->get_default(&default_type, &default_state);
	common_dcm_ops->get_init_type(&common_init_dcm_type);
	common_dcm_ops->get_all_type(&common_all_dcm_type);
	common_dcm_ops->get_init_by_k_type(&common_init_dcm_type_by_k);

	if (default_state == DCM_DEFAULT)
		dcm_set_default(default_type);
	else
		dcm_set_state(default_type, default_state);

#ifdef CONFIG_PM
	{
		kobj = kobject_create_and_add("dcm", NULL);
		if (!kobj)
			return -ENOMEM;

		err = sysfs_create_file(kobj, &dcm_state_attr.attr);
		if (err)
			dcm_pr_notice("[%s]: fail to create sysfs\n", __func__);
	}

#ifdef DCM_DEBUG_MON
	{
		err = sysfs_create_file(power_kobj, &dcm_debug_mon_attr.attr);
		if (err)
			dcm_pr_notice("[%s]: fail to create sysfs\n", __func__);
	}
#endif /* #ifdef DCM_DEBUG_MON */
#endif /* #ifdef CONFIG_PM */


	dcm_initiated = 1;

	return err;
}
EXPORT_SYMBOL(mt_dcm_common_init);

void mt_dcm_array_register(struct DCM *array, struct DCM_OPS *ops)
{
	common_dcm_array = array;
	common_dcm_ops = ops;
}
EXPORT_SYMBOL(mt_dcm_array_register);

/**** public APIs *****/
bool is_dcm_initialized(void)
{
	bool ret = true;

	if (!dcm_initiated)
		ret = false;
	return ret;
}
EXPORT_SYMBOL(is_dcm_initialized);

void mt_dcm_disable(void)
{
	if (!dcm_initiated)
		return;

	dcm_disable(common_all_dcm_type);
}

void mt_dcm_restore(void)
{
	if (!dcm_initiated)
		return;

	dcm_restore(common_all_dcm_type);
}

static int __init mtk_dcm_init(void)
{
	dcm_debug = 0;
	dcm_initiated = 0;
	return 0;
}
//arch_initcall(mt6779_dcm_init);

static void __init mtk_dcm_exit(void)
{
}
module_init(mtk_dcm_init);
module_exit(mtk_dcm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek DCM driver");
