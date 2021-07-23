/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <mtk_dcm_internal.h>
#include <mtk_dcm_autogen.h>
#include <mtk_dcm.h>

DEFINE_MUTEX(dcm_lock);
short dcm_debug;
short dcm_initiated;

short __attribute__((weak)) is_dcm_bringup(void)
{
	return 0;
}

unsigned int __attribute__((weak)) dcm_get_chip_sw_ver(void)
{
	return 0;
}

void __attribute__((weak)) dcm_pre_init(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

short __attribute__((weak)) dcm_get_cpu_cluster_stat(void)
{
	dcm_pr_info("weak function of %s\n", __func__);

	return 0;
}

void __attribute__((weak)) dcm_infracfg_ao_emi_indiv(int on)
{
	dcm_pr_info("weak function of %s: on=%d\n", __func__, on);
}

int __attribute__((weak)) dcm_set_stall_wr_del_sel
				(unsigned int mp0, unsigned int mp1)
{
	dcm_pr_info("weak function of %s: mp0=%d, mp1=%d\n",
					__func__, mp0, mp1);

	return 0;
}

void __attribute__((weak)) dcm_set_fmem_fsel_dbc
				(unsigned int fsel, unsigned int dbc)
{
	dcm_pr_info("weak function of %s: fsel=%d, dbc=%d\n",
					__func__, fsel, dbc);
}

int __attribute__((weak)) dcm_smc_get_cnt(int type_id)
{
	dcm_pr_info("weak function of %s: dcm->type_id=%d\n",
					__func__, type_id);

	return 0;
}

void __attribute__((weak)) dcm_smc_msg_send(unsigned int msg)
{
	dcm_pr_info("weak function of %s: msg=%d\n", __func__, msg);
}

void __attribute__((weak)) dcm_set_hotplug_nb(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

int __attribute__((weak)) sync_dcm_set_cci_freq(unsigned int cci)
{
	dcm_pr_info_limit("weak function of %s: cci=%u\n", __func__, cci);

	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp0_freq(unsigned int mp0)
{
	dcm_pr_info_limit("weak function of %s: mp0=%u\n", __func__, mp0);

	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp1_freq(unsigned int mp1)
{
	dcm_pr_info_limit("weak function of %s: mp1=%u\n", __func__, mp1);

	return 0;
}

int __attribute__((weak)) sync_dcm_set_mp2_freq(unsigned int mp2)
{
	dcm_pr_info_limit("weak function of %s: mp2=%u\n", __func__, mp2);

	return 0;
}

void __attribute__((weak)) *mt_dramc_chn_base_get(int channel)
{
	dcm_pr_info("weak function of %s\n", __func__);
	return NULL;
}

void __attribute__((weak)) *mt_ddrphy_chn_base_get(int channel)
{
	dcm_pr_info("weak function of %s\n", __func__);
	return NULL;
}

void __attribute__((weak)) __iomem *mt_cen_emi_base_get(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
	return 0;
}

void __attribute__((weak)) __iomem *mt_chn_emi_base_get(int chn)
{
	dcm_pr_info("weak function of %s\n", __func__);
	return 0;
}

/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to recovery CURRENT_STATE before any power-off reset.
 * 2. dcm_set_default(type) to reset as cold-power-on init state.
 * 3. dcm_disable(type) to disable all dcm.
 * 4. dcm_set_state(type) to set dcm state.
 * 5. dcm_dump_state(type) to show CURRENT_STATE.
 * 6. /sys/power/dcm_state interface:
 *	'restore', 'disable', 'dump', 'set'. 4 commands.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/
void dcm_set_default(unsigned int type)
{
	int i;
	struct DCM *dcm;

#ifndef ENABLE_DCM_IN_LK
	dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x\n",
					__func__, type, init_dcm_type);
#else
	dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x, INIT_DCM_TYPE_BY_K=0x%x\n",
		 __func__, type, init_dcm_type, INIT_DCM_TYPE_BY_K);
#endif

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm->saved_state = dcm->default_state;
			dcm->current_state = dcm->default_state;
			dcm->disable_refcnt = 0;
#ifdef ENABLE_DCM_IN_LK
			if (INIT_DCM_TYPE_BY_K & dcm->typeid) {
#endif
				if (dcm->preset_func)
					dcm->preset_func();
				dcm->func(dcm->current_state);
#ifdef ENABLE_DCM_IN_LK
			}
#endif

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}

	dcm_smc_msg_send(init_dcm_type);

	mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
	int i;
	struct DCM *dcm;
	unsigned int init_dcm_type_pre = init_dcm_type;

	dcm_pr_info("[%s]type:0x%08x, set:%d, init_dcm_type_pre=0x%x\n",
		 __func__, type, state, init_dcm_type_pre);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->saved_state = state;
			if (dcm->disable_refcnt == 0) {
				if (state)
					init_dcm_type |= dcm->typeid;
				else
					init_dcm_type &= ~(dcm->typeid);

				dcm->current_state = state;
				dcm->func(dcm->current_state);
			}

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	if (init_dcm_type_pre != init_dcm_type) {
		dcm_pr_info("[%s]type:0x%08x, set:%d, init_dcm_type=0x%x->0x%x\n",
			__func__, type, state,
			init_dcm_type_pre,
			init_dcm_type);
		dcm_smc_msg_send(init_dcm_type);
	}

	mutex_unlock(&dcm_lock);
}

void dcm_disable(unsigned int type)
{
	int i;
	struct DCM *dcm;
	unsigned int init_dcm_type_pre = init_dcm_type;

	dcm_pr_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->current_state = DCM_OFF;
			if (dcm->disable_refcnt++ == 0)
				init_dcm_type &= ~(dcm->typeid);
			dcm->func(dcm->current_state);

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	if (init_dcm_type_pre != init_dcm_type) {
		dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x->0x%x\n",
			 __func__, type, init_dcm_type_pre, init_dcm_type);
		dcm_smc_msg_send(init_dcm_type);
	}

	mutex_unlock(&dcm_lock);

}

void dcm_restore(unsigned int type)
{
	int i;
	struct DCM *dcm;
	unsigned int init_dcm_type_pre = init_dcm_type;

	dcm_pr_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->disable_refcnt > 0)
				dcm->disable_refcnt--;
			if (dcm->disable_refcnt == 0) {
				if (dcm->saved_state)
					init_dcm_type |= dcm->typeid;
				else
					init_dcm_type &= ~(dcm->typeid);

				dcm->current_state = dcm->saved_state;
				dcm->func(dcm->current_state);
			}

			dcm_pr_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	if (init_dcm_type_pre != init_dcm_type) {
		dcm_pr_info("[%s]type:0x%08x, init_dcm_type=0x%x->0x%x\n",
			 __func__, type, init_dcm_type_pre, init_dcm_type);
		dcm_smc_msg_send(init_dcm_type);
	}

	mutex_unlock(&dcm_lock);
}


void dcm_dump_state(int type)
{
	int i;
	struct DCM *dcm;

	dcm_pr_info("\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm_pr_info("[%-16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}
}

void dcm_sync_hw_state(void)
{
	int i;
	struct DCM *dcm;

	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (dcm->func_is_on != NULL) {
			dcm->current_state = dcm->func_is_on();
			dcm_pr_info("[%-16s 0x%08x] sync hw state:%d (%d)\n",
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

	/* dcm_dump_state(all_dcm_type); */
	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++)
		len += snprintf(buf+len, PAGE_SIZE-len,
				"[%-16s 0x%08x] current state:%d (%d), atf_on_cnt:%u\n",
				dcm->name, dcm->typeid, dcm->current_state,
				dcm->disable_refcnt,
				dcm_smc_get_cnt(dcm->typeid));

	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n********** dcm_state help *********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"set:       echo set [mask] [mode] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"disable:   echo disable [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"restore:   echo restore [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"dump:      echo dump [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"debug:     echo debug [0/1] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mask] is hexl bit mask of dcm;\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mode] is type of DCM to set and retained\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"init_dcm_type=0x%x, all_dcm_type=0x%x, dcm_debug=%d, ",
			init_dcm_type, all_dcm_type, dcm_debug);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"dcm_cpu_cluster_stat=%d\n",
			dcm_get_cpu_cluster_stat());
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
	unsigned int val0, val1;
	int ret, mode;

	if (sscanf(buf, "%15s %x", cmd, &mask) == 2) {
		mask &= all_dcm_type;

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
			dcm_dump_regs();
		} else if (!strcmp(cmd, "debug")) {
			if (mask == 0)
				dcm_debug = 0;
			else if (mask == 1)
				dcm_debug = 1;
			else if (mask == 2)
				dcm_infracfg_ao_emi_indiv(0);
			else if (mask == 3)
				dcm_infracfg_ao_emi_indiv(1);
		} else if (!strcmp(cmd, "set_stall_sel")) {
			if (sscanf(buf, "%15s %x %x", cmd, &val0, &val1) == 3)
				dcm_set_stall_wr_del_sel(val0, val1);
		} else if (!strcmp(cmd, "set_fmem")) {
			if (sscanf(buf, "%15s %d %d", cmd, &val0, &val1) == 3)
				dcm_set_fmem_fsel_dbc(val0, val1);
		} else if (!strcmp(cmd, "set")) {
			if (sscanf(buf, "%15s %x %d", cmd, &mask, &mode) == 3) {
				mask &= all_dcm_type;

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
#endif /* #ifdef CONFIG_PM */

int __init mt_dcm_init(void)
{
	if (is_dcm_bringup())
		return 0;

	if (dcm_initiated)
		return 0;

	if (mt_dcm_dts_map()) {
		dcm_pr_err("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	dcm_pre_init();

#ifndef DCM_DEFAULT_ALL_OFF
	/** enable all dcm **/
	dcm_set_default(init_dcm_type);
#else /* DCM_DEFAULT_ALL_OFF */
	dcm_set_state(all_dcm_type, DCM_OFF);
#endif /* #ifndef DCM_DEFAULT_ALL_OFF */

	dcm_dump_regs();
	dcm_sync_hw_state();

#ifdef CONFIG_PM
	{
		int err = 0;

		err = sysfs_create_file(power_kobj, &dcm_state_attr.attr);
		if (err)
			dcm_pr_err("[%s]: fail to create sysfs\n", __func__);
	}

#ifdef DCM_DEBUG_MON
	{
		int err = 0;

		err = sysfs_create_file(power_kobj, &dcm_debug_mon_attr.attr);
		if (err)
			dcm_pr_err("[%s]: fail to create sysfs\n", __func__);
	}
#endif /* #ifdef DCM_DEBUG_MON */
#endif /* #ifdef CONFIG_PM */

	dcm_set_hotplug_nb();

	dcm_initiated = 1;

	return 0;
}
late_initcall(mt_dcm_init);

/**** public APIs *****/
void mt_dcm_disable(void)
{
	if (!dcm_initiated)
		return;

	dcm_disable(all_dcm_type);
}

void mt_dcm_restore(void)
{
	if (!dcm_initiated)
		return;

	dcm_restore(all_dcm_type);
}
