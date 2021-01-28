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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <mtk_ccci_common.h>
#include <linux/uidgid.h>
#include <mtk_cooler_setting.h>
#include <linux/debugfs.h>


/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/
#ifndef MAX
#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#endif

/* TMC interface */
#define MUTT_ACTIVATED_OFFSET		(16)
#define MUTT_SUSPEND_OFFSET		(24)
#define MUTT_ACTIVATED_FILTER		(0x00FF0000)
#define MUTT_SUSPEND_FILTER		(0xFF000000)
#define MUTT_LEVEL_CTRL_CMD_FILTER	(0x0000FFFF)
#define TMC_IMS_ONLY_LEVEL		(TMC_COOLER_LV7)
#define TMC_NO_IMS_LEVEL		(TMC_COOLER_LV8)
#define TMC_MD_OFF_LEVEL		(0xFF)
#define TMC_CA_CTRL_CA_ON \
	(TMC_CTRL_CMD_CA_CTRL | TMC_CA_ON << 8)
#define TMC_CA_CTRL_CA_OFF \
	(TMC_CTRL_CMD_CA_CTRL | TMC_CA_OFF << 8)
#define TMC_PA_CTRL_PA_ALL_ON \
	(TMC_CTRL_CMD_PA_CTRL | TMC_PA_ALL_ON << 8)
#define TMC_PA_CTRL_PA_OFF_1PA \
	(TMC_CTRL_CMD_PA_CTRL | TMC_PA_OFF_1PA << 8)
#define TMC_THROTTLING_THROT_DISABLE \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROT_DISABLE << 8)
#define MUTT_THROTTLING_IMS_ENABLE \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROT_ENABLE_IMS_ENABLE << 8)
#define MUTT_THROTTLING_IMS_DISABLE \
	(TMC_CTRL_CMD_THROTTLING | TMC_THROT_ENABLE_IMS_DISABLE << 8)
#define MUTT_TMC_COOLER_LV_ENABLE \
	(TMC_CTRL_CMD_COOLER_LV | TMC_COOLER_LV_ENABLE << 8)
#define MUTT_TMC_COOLER_LV_DISABLE \
	(TMC_CTRL_CMD_COOLER_LV | TMC_COOLER_LV_DISABLE << 8)
#define TMC_COOLER_LV_CTRL00 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV0 << 16)
#define TMC_COOLER_LV_CTRL01 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV1 << 16)
#define TMC_COOLER_LV_CTRL02 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV2 << 16)
#define TMC_COOLER_LV_CTRL03 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV3 << 16)
#define TMC_COOLER_LV_CTRL04 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV4 << 16)
#define TMC_COOLER_LV_CTRL05 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV5 << 16)
#define TMC_COOLER_LV_CTRL06 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV6 << 16)
#define TMC_COOLER_LV_CTRL07 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV7 << 16)
#define TMC_COOLER_LV_CTRL08 (MUTT_TMC_COOLER_LV_ENABLE | TMC_COOLER_LV8 << 16)
#define TMC_COOLER_LV_RAT_LTE	(TMC_OVERHEATED_LTE << 24)
#define TMC_COOLER_LV_RAT_NR	(TMC_OVERHEATED_NR << 24)
#define TMC_REDUCE_OTHER_MAX_TX_POWER(pwr) \
	(TMC_CTRL_CMD_TX_POWER \
	| (TMC_TW_PWR_REDUCE_OTHER_MAX_TX_EVENT << 16)	\
	| (pwr << 24))
#define TMC_REDUCE_NR_MAX_TX_POWER(pwr) \
	(TMC_CTRL_CMD_TX_POWER \
	| (TMC_TW_PWR_REDUCE_NR_MAX_TX_EVENT << 16)	\
	| (pwr << 24))

#if 0
/*
 * No UL data(except IMS): active = 1; suspend = 255; bit0 in reserved =0;
 * No UL data(no IMS): active = 1; suspend = 255; bit0 in reserved =1;
 */
#define BIT_MD_CL_NO_IMS MUTT_ENABLE_IMS_DISABLE /*IMS disable*/
#define MD_CL_NO_UL_DATA (0xFF010000 | MUTT_ENABLE_IMS_ENABLE) /*IMS only*/
#endif

/* State of "MD off & noIMS" are not included. */
#define MAX_NUM_INSTANCE_MTK_COOLER_MUTT  8
#define MAX_NUM_TX_PWR_LV  3

#define MTK_CL_MUTT_GET_LIMIT(limit, state) \
{ (limit) = (short) (((unsigned long) (state))>>16); }
#define MTK_CL_MUTT_SET_LIMIT(limit, state) \
{ state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }
#define MTK_CL_MUTT_GET_CURR_STATE(curr_state, state) \
{ curr_state = (((unsigned long) (state))&0xFFFF); }
#define MTK_CL_MUTT_SET_CURR_STATE(curr_state, state) \
do { \
	if (curr_state == 0) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

#define IS_MD_OFF(lv)			(lv == (int)TMC_MD_OFF_LEVEL)
#define IS_MD_OFF_OR_NO_IMS(lv)		(lv >= (int)TMC_NO_IMS_LEVEL)
#define IS_IMS_ONLY(lv)			(lv == (int)TMC_IMS_ONLY_LEVEL)
#define IS_MUTT_TYPE_VALID(type)	((unsigned int)type < NR_MUTT_TYPE)

#define for_each_mutt_type(i)	for (i = 0; i < NR_MUTT_TYPE; i++)
#define for_each_mutt_cooler_instance(i) \
	for (i = 0; i < MAX_NUM_INSTANCE_MTK_COOLER_MUTT; i++)
#define for_each_tx_pwr_lv(i)  for (i = 0; i < MAX_NUM_TX_PWR_LV; i++)

/* LOG */
#define mtk_cooler_mutt_dprintk_always(fmt, args...) \
pr_debug("[Thermal/TC/mutt]" fmt, ##args)

#define mtk_cooler_mutt_dprintk(fmt, args...) \
do { \
	if (clmutt_data.klog_on == 1) \
		pr_debug("[Thermal/TC/mutt]" fmt, ##args); \
} while (0)

/* PROCFS */
#define PROC_BUFFER_LEN		128
#define PROC_ENTRY(name) {__stringify(name), &name ## _proc_fops}
#define PROC_FOPS_RW(name)                                                    \
static int clmutt_ ## name ## _proc_open(                                     \
	struct inode *inode, struct file *file)                               \
{                                                                             \
	return single_open(file, clmutt_ ## name ## _proc_read,               \
		PDE_DATA(inode));                                             \
}                                                                             \
static const struct file_operations clmutt_ ## name ## _proc_fops = {         \
	.owner	= THIS_MODULE,                                                \
	.open	= clmutt_ ## name ## _proc_open,                              \
	.read	= seq_read,                                                   \
	.llseek	= seq_lseek,                                                  \
	.release	= single_release,                                     \
	.write	= clmutt_ ## name ## _proc_write,                             \
}

enum mutt_type {
	MUTT_LTE,
	MUTT_NR,

	NR_MUTT_TYPE,
};

/*enum mapping must be align with MD site*/
enum tmc_ctrl_cmd_enum {
	TMC_CTRL_CMD_THROTTLING = 0,
	TMC_CTRL_CMD_CA_CTRL,
	TMC_CTRL_CMD_PA_CTRL,
	TMC_CTRL_CMD_COOLER_LV,
	/* MD internal use start */
	TMC_CTRL_CMD_CELL,            /* refer as del_cell */
	TMC_CTRL_CMD_BAND,            /* refer as del_band */
	TMC_CTRL_CMD_INTER_BAND_OFF,  /* similar to PA_OFF on Gen95 */
	TMC_CTRL_CMD_CA_OFF,          /* similar to CA_OFF on Gen95 */
	/* MD internal use end */
	TMC_CTRL_CMD_SCG_OFF,         /* Fall back to 4G */
	TMC_CTRL_CMD_SCG_ON,          /* Enabled 5G */
	TMC_CTRL_CMD_TX_POWER,
	TMC_CTRL_CMD_DEFAULT,
};

enum tmc_throt_ctrl_enum {
	TMC_THROT_ENABLE_IMS_ENABLE = 0,
	TMC_THROT_ENABLE_IMS_DISABLE,
	TMC_THROT_DISABLE,
};

enum tmc_ca_ctrl_enum {
	TMC_CA_ON = 0, /* leave thermal control*/
	TMC_CA_OFF,
};

enum tmc_pa_ctrl_enum {
	TMC_PA_ALL_ON = 0, /* leave thermal control*/
	TMC_PA_OFF_1PA,
};

enum tmc_cooler_lv_ctrl_enum {
	TMC_COOLER_LV_ENABLE = 0,
	TMC_COOLER_LV_DISABLE
};

enum tmc_cooler_lv_enum {
	TMC_COOLER_LV0 = 0,
	TMC_COOLER_LV1,
	TMC_COOLER_LV2,
	TMC_COOLER_LV3,
	TMC_COOLER_LV4,
	TMC_COOLER_LV5,
	TMC_COOLER_LV6,
	TMC_COOLER_LV7,
	TMC_COOLER_LV8,
};

enum tmc_overheated_rat_enum {
	TMC_OVERHEATED_LTE = 0,
	TMC_OVERHEATED_NR,
};

enum tmc_tx_pwr_event_enum {
	TMC_TW_PWR_VOLTAGE_LOW_EVENT = 0,
	TMC_TW_PWR_LOW_BATTERY_EVENT,
	TMC_TW_PWR_OVER_CURRENT_EVENT,
	/* reserved for reduce 2G/3G/4G/C2K max TX power for certain value */
	TMC_TW_PWR_REDUCE_OTHER_MAX_TX_EVENT,
	/* reserved for reduce 5G max TX power for certain value */
	TMC_TW_PWR_REDUCE_NR_MAX_TX_EVENT,
	TMC_TW_PWR_EVENT_MAX_NUM,
};

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
#endif

/*
 * use "si_errno" for client identify
 * for tm_pid (/system/bin/thermal)
 */
enum {
	/*	TM_CLIENT_clwmt = 0,
	 *	TM_CLIENT_mdulthro =1,
	 *	TM_CLIENT_mddlthro =2,
	 */
	TM_CLIENT_clmutt = 3
};


/****************************************************************************
 *  Type Definitions
 ****************************************************************************/
#if FEATURE_ADAPTIVE_MUTT
struct clmutt_adaptive_algo_param {
	int tput_limit;
	int target_t;
	int target_t_range;
	int tt_high;
	int tt_low;
	int is_triggered;
};
#endif

struct clmutt_cooler {
	enum mutt_type type;
	char *name;
	int target_level;
	int target_tx_pwr_level;
#if FEATURE_ADAPTIVE_MUTT
	int adp_level;
	struct clmutt_adaptive_algo_param adp_param;
	struct thermal_cooling_device *adp_dev;
	unsigned long adp_state;
#endif
	/* mdoff cooler */
	struct thermal_cooling_device *mdoff_dev;
	unsigned long mdoff_state;
	/* noIMS cooler */
	struct thermal_cooling_device *noIMS_dev;
	unsigned long noIMS_state;
	/* normal cooler */
	struct thermal_cooling_device *dev[MAX_NUM_INSTANCE_MTK_COOLER_MUTT];
	unsigned long state[MAX_NUM_INSTANCE_MTK_COOLER_MUTT];
	/* To identify cooler type and level */
	unsigned int id[MAX_NUM_INSTANCE_MTK_COOLER_MUTT];
	/* TX power cooler */
	struct thermal_cooling_device *tx_pwr_dev[MAX_NUM_TX_PWR_LV];
	unsigned long tx_pwr_state[MAX_NUM_TX_PWR_LV];
	unsigned int tx_pwr_db[MAX_NUM_TX_PWR_LV];
	/* To identify cooler type and reduce tx power level */
	unsigned int id_tx_pwr[MAX_NUM_TX_PWR_LV];
};

struct clmutt_param {
	int klog_on;
	/* latest limit to MD */
	unsigned int cur_limit;
	int cur_level;
	/* record last MD boot count to recover previous limit */
	unsigned long last_md_boot_cnt;
	/* signal related */
#if FEATURE_THERMAL_DIAG
	unsigned int tmd_pid;
	unsigned int tmd_input_pid;
	struct task_struct *ptmd_task;
#endif
	unsigned int tm_pid;
	unsigned int tm_input_pid;
	struct task_struct *pg_task;
	struct clmutt_cooler cooler_param[NR_MUTT_TYPE];
	struct mutex lock;
	struct thermal_cooling_device *scg_off_dev;
	unsigned long scg_off_state;
	/* cache command input */
	int cooler_lv_ctrl[NR_MUTT_TYPE];
	unsigned int ca_ctrl; /* LTE only */
	unsigned int pa_ctrl; /* LTE only */
	unsigned int no_ims; /* LTE only */
	unsigned int active_period_100ms; /* LTE only, 0 is disable */
	unsigned int suspend_period_100ms; /* LTE only, 0 is disable */
	unsigned int scg_off; /* NR only */
	unsigned int reduce_tx_pwr[NR_MUTT_TYPE];
};

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static struct clmutt_param clmutt_data = {
	.cur_limit = MUTT_TMC_COOLER_LV_DISABLE,
	.cur_level = -1,
	.cooler_param = {
		[MUTT_LTE] = {
			.type = MUTT_LTE,
			.name = __stringify(MUTT_LTE),
			.target_level = -1,
			.target_tx_pwr_level = -1,
#if FEATURE_ADAPTIVE_MUTT
			.adp_level = -1,
			.adp_param = {0, 58000, 1000, 50, 50, 0},
#endif
			.dev = { 0 },
			.state = { 0 },
			.tx_pwr_dev = { 0 },
			.tx_pwr_state = { 0 },
			.tx_pwr_db = {1, 2, 3},
		},
		[MUTT_NR] = {
			.type = MUTT_NR,
			.name = __stringify(MUTT_NR),
			.target_level = -1,
			.target_tx_pwr_level = -1,
#if FEATURE_ADAPTIVE_MUTT
			.adp_level = -1,
			.adp_param = {0, 58000, 1000, 50, 50, 0},
#endif
			.dev = { 0 },
			.state = { 0 },
			.tx_pwr_dev = { 0 },
			.tx_pwr_state = { 0 },
			.tx_pwr_db = {1, 2, 3},
		},
	},
	.lock = __MUTEX_INITIALIZER(clmutt_data.lock),
	.cooler_lv_ctrl = {-1, -1},
};

/****************************************************************************
 *  Weak Function
 ****************************************************************************/
unsigned long __attribute__ ((weak))
ccci_get_md_boot_count(int md_id)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

int __attribute__ ((weak))
exec_ccci_kern_func_by_md_id(
int md_id, unsigned int id, char *buf, unsigned int len)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return -316;
}

/****************************************************************************
 *  Static Function
 ****************************************************************************/
static int clmutt_decide_final_level(enum mutt_type type, int lv)
{
	int i, final_lv = -1;

	for_each_mutt_type(i) {
		if (i == (int)type)
			final_lv = MAX(final_lv, lv);
		else
			final_lv = MAX(final_lv,
				clmutt_data.cooler_param[i].target_level);
	}

	return final_lv;
}

static void clmutt_cooler_param_reset(unsigned long mdoff_state)
{
	int i, j;

	clmutt_data.cur_limit = 0;
	for_each_mutt_type(i) {
		clmutt_data.cooler_param[i].noIMS_state = 0;
#if FEATURE_ADAPTIVE_MUTT
		/* clmutt_data.cooler_param[i].adp_level = -1; */
		clmutt_data.cooler_param[i].adp_state = 0;
#endif
		for_each_mutt_cooler_instance(j)
			MTK_CL_MUTT_SET_CURR_STATE(
			0, clmutt_data.cooler_param[i].state[j]);
	}
}

static unsigned int clmutt_level_selection(int lv, unsigned int type)
{
	unsigned int ctrl_lv = 0;

	switch (lv) {
	case 0:
		ctrl_lv = TMC_COOLER_LV_CTRL00;
		break;
	case 1:
		ctrl_lv = TMC_COOLER_LV_CTRL01;
		break;
	case 2:
		ctrl_lv = TMC_COOLER_LV_CTRL02;
		break;
	case 3:
		ctrl_lv = TMC_COOLER_LV_CTRL03;
		break;
	case 4:
		ctrl_lv = TMC_COOLER_LV_CTRL04;
		break;
	case 5:
		ctrl_lv = TMC_COOLER_LV_CTRL05;
		break;
	case 6:
		ctrl_lv = TMC_COOLER_LV_CTRL06;
		break;
	case 7:
		ctrl_lv = TMC_COOLER_LV_CTRL07;
		break;
	case 8:
		ctrl_lv = TMC_COOLER_LV_CTRL08;
		break;
	default:
		ctrl_lv = MUTT_TMC_COOLER_LV_DISABLE;
		break;
	}

	ctrl_lv = (type == MUTT_NR)
		? ctrl_lv | TMC_COOLER_LV_RAT_NR
		: ctrl_lv | TMC_COOLER_LV_RAT_LTE;

	mtk_cooler_mutt_dprintk(
		"[%s] type(%d) lv(%d):ctrl_lv: 0x%08x\n",
		__func__, type, lv, ctrl_lv);

	return ctrl_lv;
}

static int clmutt_send_tmc_cmd(unsigned int cmd)
{
	int ret = 0;

	if (cmd != clmutt_data.cur_limit) {
		clmutt_data.cur_limit = cmd;
		clmutt_data.last_md_boot_cnt = ccci_get_md_boot_count(MD_SYS1);
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
			ID_THROTTLING_CFG, (char *) &cmd, 4);

		mtk_cooler_mutt_dprintk_always(
			"[%s] ret %d param 0x%08x bcnt %lu\n", __func__,
			ret, cmd, clmutt_data.last_md_boot_cnt);

	} else if (cmd != MUTT_TMC_COOLER_LV_DISABLE) {
		unsigned long cur_md_bcnt = ccci_get_md_boot_count(MD_SYS1);

		if (clmutt_data.last_md_boot_cnt != cur_md_bcnt) {
			clmutt_data.last_md_boot_cnt = cur_md_bcnt;
			ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
				ID_THROTTLING_CFG, (char *) &cmd, 4);

			mtk_cooler_mutt_dprintk_always(
				"[%s] mdrb ret %d param 0x%08x bcnt %lu\n",
				__func__, ret, cmd,
				clmutt_data.last_md_boot_cnt);
		}
	}

	return ret;
}

#if FEATURE_THERMAL_DIAG
static int clmutt_send_tmd_signal(int level)
{
	int ret = 0;
	static int warning_state = TMD_Alert_ULdataBack;

	if (warning_state == level)
		return ret;

	if (clmutt_data.tmd_input_pid == 0) {
		mtk_cooler_mutt_dprintk("%s pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_mutt_dprintk_always(" %s pid is %d, %d; MD_Alert: %d\n",
				__func__, clmutt_data.tmd_pid,
				clmutt_data.tmd_input_pid, level);

	if (ret == 0 && clmutt_data.tmd_input_pid != clmutt_data.tmd_pid) {
		clmutt_data.tmd_pid = clmutt_data.tmd_input_pid;

		if (clmutt_data.ptmd_task != NULL)
			put_task_struct(clmutt_data.ptmd_task);
		clmutt_data.ptmd_task = get_pid_task(
			find_vpid(clmutt_data.tmd_pid), PIDTYPE_PID);
	}

	if (ret == 0 && clmutt_data.ptmd_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = 0;
		info.si_code = level;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, clmutt_data.ptmd_task);
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
#endif

static int clmutt_send_tm_signal(enum mutt_type type, unsigned long state)
{
	int ret = 0;
	int target_lv;

	if (!IS_MUTT_TYPE_VALID(type))
		return ret;

	if (clmutt_data.cooler_param[type].mdoff_state == state)
		return ret;

	target_lv = (state) ? TMC_MD_OFF_LEVEL : TMC_NO_IMS_LEVEL;
	if (!state && clmutt_decide_final_level(type, target_lv)
		== TMC_MD_OFF_LEVEL) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s:MD off by others, skip!\n", __func__,
			clmutt_data.cooler_param[type].name);
		clmutt_data.cooler_param[type].mdoff_state = state;
		clmutt_data.cooler_param[type].target_level = target_lv;
		return ret;
	} else if (target_lv == clmutt_data.cur_level) {
		clmutt_data.cooler_param[type].mdoff_state = state;
		clmutt_data.cooler_param[type].target_level = target_lv;
		return ret;
	}

	if (clmutt_data.tm_input_pid == 0) {
		mtk_cooler_mutt_dprintk("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_mutt_dprintk_always("[%s] %s:pid is %d, %d; MD off=%d\n",
		__func__, clmutt_data.cooler_param[type].name,
		clmutt_data.tm_pid, clmutt_data.tm_input_pid, state);

	if (ret == 0 && clmutt_data.tm_input_pid != clmutt_data.tm_pid) {
		clmutt_data.tm_pid = clmutt_data.tm_input_pid;

		if (clmutt_data.pg_task != NULL)
			put_task_struct(clmutt_data.pg_task);
		clmutt_data.pg_task = get_pid_task(
			find_vpid(clmutt_data.tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && clmutt_data.pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = TM_CLIENT_clmutt;
		info.si_code = state; /* Toggle MD ON: 0 OFF: 1*/
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, clmutt_data.pg_task);
	}

	if (ret != 0)
		mtk_cooler_mutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
	else {
		clmutt_data.cooler_param[type].mdoff_state = state;
		clmutt_data.cooler_param[type].target_level = target_lv;
		if (state == 1) {
			clmutt_cooler_param_reset(state);
			clmutt_data.cur_level = target_lv;
			mtk_cooler_mutt_dprintk_always(
				"[%s] MD off by %s!\n", __func__,
				clmutt_data.cooler_param[type].name);
		} else {
			clmutt_data.cooler_param[type].mdoff_state = 0;
			clmutt_data.cooler_param[type].target_level =
				target_lv;
			clmutt_data.cur_level = -1;
			clmutt_data.cur_limit =
				MUTT_TMC_COOLER_LV_DISABLE;
			mtk_cooler_mutt_dprintk_always(
				"[%s] MD on by %s!\n", __func__,
				clmutt_data.cooler_param[type].name);
		}
	}
	return ret;
}

static int clmutt_disable_cooler_lv_ctrl(void)
{
	int ret;

	if (clmutt_data.cur_level == -1)
		return 0;

	ret = clmutt_send_tmc_cmd(MUTT_TMC_COOLER_LV_DISABLE);
	if (!ret) {
		clmutt_data.cur_limit = MUTT_TMC_COOLER_LV_DISABLE;
		clmutt_data.cur_level = -1;
		mtk_cooler_mutt_dprintk("[%s] disable lv ctrl done\n",
			__func__);
	} else {
		clmutt_data.cur_limit = 0;
		mtk_cooler_mutt_dprintk_always(
			"[%s] disable lv ctrl failed, ret:%d\n",
			__func__, ret);
	}

	return ret;
}

static int clmutt_resend_limit(void)
{
	int i;
	unsigned int cmd;

	if (clmutt_data.cur_limit) {
		cmd = clmutt_data.cur_limit;
	} else {
		/* cur_limit = 0 due to previous command sent failed */
		for_each_mutt_type(i) {
			if (clmutt_data.cur_level ==
				clmutt_data.cooler_param[i].target_level)
				break;
		}

		if (i == NR_MUTT_TYPE)
			cmd = MUTT_TMC_COOLER_LV_DISABLE;
		else
			cmd = clmutt_level_selection(clmutt_data.cur_level,
				(enum mutt_type)i);
	}

	return clmutt_send_tmc_cmd(cmd);
}

static int clmutt_send_scg_off_cmd(int onoff)
{
	int ret = -1;
	unsigned int limit;

	if (onoff)
		limit = TMC_CTRL_CMD_SCG_OFF;
	else
		limit = TMC_CTRL_CMD_SCG_ON;

	ret = clmutt_send_tmc_cmd(limit);
	mtk_cooler_mutt_dprintk_always(
		"[%s] set SCG off:%d(0x%08x). ret:%d, bcnt:%lu\n",
		__func__, onoff, limit,
		ret, clmutt_data.last_md_boot_cnt);

	return ret;
}

static int clmutt_send_reduce_tx_pwr_cmd(
	enum mutt_type type, unsigned int pwr)
{
	int ret = -1;
	unsigned int limit;

	if (pwr < 0)
		pwr = 0;

	limit = (type == MUTT_NR)
		? TMC_REDUCE_NR_MAX_TX_POWER(pwr)
		: TMC_REDUCE_OTHER_MAX_TX_POWER(pwr);
	ret = clmutt_send_tmc_cmd(limit);

	mtk_cooler_mutt_dprintk_always(
		"[%s] set %s tx_pwr:%d(0x%08x). ret:%d, bcnt:%lu\n",
		__func__, clmutt_data.cooler_param[type].name, pwr,
		limit, ret, clmutt_data.last_md_boot_cnt);

	return ret;
}

/*
 * cooling device callback functions (mtk_cl_mdoff_ops)
 * 1 : True and 0 : False
 */
static int mtk_cl_mdoff_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
				cdev->type, *state);

	return 0;
}

static int mtk_cl_mdoff_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	unsigned int type = *(unsigned int *)cdev->devdata;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	*state = clmutt_data.cooler_param[type].mdoff_state;
	mtk_cooler_mutt_dprintk(
			"[%s] %s %lu (0: md on;  1: md off)\n", __func__,
			cdev->type, *state);
	return 0;
}

static int mtk_cl_mdoff_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	unsigned int type = *(unsigned int *)cdev->devdata;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if ((state >= 0) && (state <= 1))
		mtk_cooler_mutt_dprintk(
			"[%s] %s %lu (0: md on;  1: md off)\n", __func__,
			cdev->type, state);
	else {
		mtk_cooler_mutt_dprintk(
		"[%s]: Invalid input (0:md on;	 1: md off)\n", __func__);

		return 0;
	}

	mutex_lock(&clmutt_data.lock);

	clmutt_send_tm_signal(type, state);

	mutex_unlock(&clmutt_data.lock);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_mdoff_ops = {
	.get_max_state = mtk_cl_mdoff_get_max_state,
	.get_cur_state = mtk_cl_mdoff_get_cur_state,
	.set_cur_state = mtk_cl_mdoff_set_cur_state,
};

static void mtk_cl_mutt_set_onIMS(enum mutt_type type, unsigned long state)
{
	int ret = 0, target_lv;

	if (!IS_MUTT_TYPE_VALID(type))
		return;

	if (clmutt_data.cooler_param[type].noIMS_state == state)
		return;

	target_lv = (state) ? TMC_NO_IMS_LEVEL : TMC_IMS_ONLY_LEVEL;
	if (!state && clmutt_decide_final_level(type, target_lv)
		== TMC_NO_IMS_LEVEL) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s:IMS off by others, skip!\n", __func__,
			clmutt_data.cooler_param[type].name);
		clmutt_data.cooler_param[type].noIMS_state = state;
		clmutt_data.cooler_param[type].target_level = target_lv;
		return;
	}

	if (target_lv != clmutt_data.cur_level)
		/* update lv to MD */
		ret = clmutt_send_tmc_cmd(
			clmutt_level_selection(target_lv, type));
	else
		ret = clmutt_resend_limit();

	if (ret != 0) {
		clmutt_data.cur_limit = 0;
		mtk_cooler_mutt_dprintk_always("[%s] %s:ret=%d\n", __func__,
			clmutt_data.cooler_param[type].name, ret);
	} else {
		clmutt_data.cooler_param[type].noIMS_state = state;
		clmutt_data.cooler_param[type].target_level = target_lv;
		clmutt_data.cur_level = target_lv;
		mtk_cooler_mutt_dprintk_always("[%s] %s:set noIMS state=%d\n",
			__func__, clmutt_data.cooler_param[type].name, state);
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
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
							cdev->type, *state);

	return 0;
}

static int mtk_cl_noIMS_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	unsigned int type = *(unsigned int *)cdev->devdata;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	*state = clmutt_data.cooler_param[type].noIMS_state;
	mtk_cooler_mutt_dprintk(
			"%s() %s %lu (0: md IMS OK;  1: md no IMS)\n", __func__,
			cdev->type, *state);

	return 0;
}

static int mtk_cl_noIMS_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	unsigned int type = *(unsigned int *)cdev->devdata;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if ((state >= 0) && (state <= 1))
		mtk_cooler_mutt_dprintk(
			"%s() %s %lu (0: md IMS OK;	1: md no IMS)\n",
			__func__,
			cdev->type, state);
	else {
		mtk_cooler_mutt_dprintk_always(
			"%s(): Invalid input(0: md IMS OK; 1: md no IMS)\n",
			__func__);
		return 0;
	}

	mutex_lock(&clmutt_data.lock);

	if (IS_MD_OFF(clmutt_data.cur_level)) {
		mtk_cooler_mutt_dprintk(
			"%s(): MD STILL OFF!!\n", __func__);
		goto end;
	}

	mtk_cl_mutt_set_onIMS(type, state);

end:
	mutex_unlock(&clmutt_data.lock);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_noIMS_ops = {
	.get_max_state = mtk_cl_noIMS_get_max_state,
	.get_cur_state = mtk_cl_noIMS_get_cur_state,
	.set_cur_state = mtk_cl_noIMS_set_cur_state,
};

/* MUST in lock! */
static void mtk_cl_mutt_set_mutt_limit(enum mutt_type type)
{
	int i, final_lv, ret = 0;
	int target_lv = -1;

	if (!IS_MUTT_TYPE_VALID(type))
		return;

	for_each_mutt_cooler_instance(i) {
		unsigned long curr_state;

		MTK_CL_MUTT_GET_CURR_STATE(curr_state,
			clmutt_data.cooler_param[type].state[i]);

		if (curr_state == 1)
			target_lv = i;
	}

#if FEATURE_ADAPTIVE_MUTT
	if (clmutt_data.cooler_param[type].adp_level > target_lv)
		target_lv = clmutt_data.cooler_param[type].adp_level;
#endif

	final_lv = clmutt_decide_final_level(type, target_lv);
	if (final_lv != -1 && target_lv < final_lv) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s:target_lv(%d) < final_lv(%d), skip...\n",
			__func__, clmutt_data.cooler_param[type].name,
			target_lv, final_lv);
		clmutt_data.cooler_param[type].target_level = target_lv;
		return;
	}

	if (target_lv != clmutt_data.cur_level) {
		/* update lv to MD */
		ret = clmutt_send_tmc_cmd(
			clmutt_level_selection(target_lv, type));
		mtk_cooler_mutt_dprintk_always("[%s]set %s lv to %d, ret=%d\n",
			__func__, clmutt_data.cooler_param[type].name,
			target_lv, ret);
	} else {
		ret = clmutt_resend_limit();
	}

	if (ret != 0) {
		/* force to send again at next time */
		clmutt_data.cur_limit = 0;
		for_each_mutt_cooler_instance(i) {
			MTK_CL_MUTT_SET_CURR_STATE(0,
				clmutt_data.cooler_param[type].state[i]);
			mtk_cooler_mutt_dprintk_always(
			"[%s]cl_mutt_state[%d] %ld\n",
			__func__, i, clmutt_data.cooler_param[type].state[i]);

		}
		mtk_cooler_mutt_dprintk_always("[%s] ret=%d\n", __func__, ret);
		return;
	}

	clmutt_data.cur_level = target_lv;
	clmutt_data.cooler_param[type].target_level = target_lv;
#if FEATURE_THERMAL_DIAG
	if (IS_IMS_ONLY(clmutt_data.cur_level))
		clmutt_send_tmd_signal(TMD_Alert_NOULdata);
	else
		clmutt_send_tmd_signal(TMD_Alert_ULdataBack);
#endif
}

static int mtk_cl_mutt_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
							cdev->type, *state);

	return 0;
}

static int mtk_cl_mutt_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	unsigned int id = *(unsigned int *)cdev->devdata;
	unsigned int type = id / MAX_NUM_INSTANCE_MTK_COOLER_MUTT;
	unsigned int cooler_id = id % MAX_NUM_INSTANCE_MTK_COOLER_MUTT;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if (cooler_id >= MAX_NUM_INSTANCE_MTK_COOLER_MUTT) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt cooler_id %d\n",
			__func__, cdev->type, cooler_id);
		return 0;
	}

	MTK_CL_MUTT_GET_CURR_STATE(*state,
		clmutt_data.cooler_param[type].state[cooler_id]);
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
							cdev->type, *state);
	return 0;
}

static int mtk_cl_mutt_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	unsigned int id = *(unsigned int *)cdev->devdata;
	unsigned int type = id / MAX_NUM_INSTANCE_MTK_COOLER_MUTT;
	unsigned int cooler_id = id % MAX_NUM_INSTANCE_MTK_COOLER_MUTT;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if (cooler_id >= MAX_NUM_INSTANCE_MTK_COOLER_MUTT) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt cooler_id %d\n",
			__func__, cdev->type, cooler_id);
		return 0;
	}

	if ((state != 0) && (state != 1)) {
		mtk_cooler_mutt_dprintk_always(
		"[%s] %s: Invalid input(0:mutt off; 1:mutt on)\n",
			__func__, cdev->type);
		return 0;
	}

	mutex_lock(&clmutt_data.lock);

	if (IS_MD_OFF_OR_NO_IMS(clmutt_data.cur_level)) {
		mtk_cooler_mutt_dprintk_always("[%s] %s: MD OFF or noIMS!!\n",
						__func__, cdev->type);
		goto end;
	}

	mtk_cooler_mutt_dprintk("[%s] %s id%d cid%d %lu\n",
		__func__, cdev->type, id, cooler_id, state);

	MTK_CL_MUTT_SET_CURR_STATE(state,
		clmutt_data.cooler_param[type].state[cooler_id]);
	mtk_cl_mutt_set_mutt_limit(type);

end:
	mutex_unlock(&clmutt_data.lock);

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
static void decrease_mutt_limit(unsigned int type)
{
	if (!IS_MUTT_TYPE_VALID(type))
		return;

	if (clmutt_data.cooler_param[type].adp_level >= 0)
		clmutt_data.cooler_param[type].adp_level--;

	mtk_cooler_mutt_dprintk("%s : type %d level = 0x%x\n", __func__,
		type, clmutt_data.cooler_param[type].adp_level);
}

/* increase by one level */
static void increase_mutt_limit(unsigned int type)
{
	if (!IS_MUTT_TYPE_VALID(type))
		return;

	if (clmutt_data.cooler_param[type].adp_level
		< (MAX_NUM_INSTANCE_MTK_COOLER_MUTT - 1))
		clmutt_data.cooler_param[type].adp_level++;
	mtk_cooler_mutt_dprintk("%s : type %d level = 0x%x\n", __func__,
		type, clmutt_data.cooler_param[type].adp_level);
}

static void unlimit_mutt_limit(unsigned int type)
{
	if (!IS_MUTT_TYPE_VALID(type))
		return;

	clmutt_data.cooler_param[type].adp_level = -1;
	mtk_cooler_mutt_dprintk("%s\n", __func__);
}

static int adaptive_tput_limit(unsigned int type, long curr_temp)
{
	struct clmutt_adaptive_algo_param *param;
	int target_t_h, target_t_l;

	if (!IS_MUTT_TYPE_VALID(type))
		return 0;

	param = &clmutt_data.cooler_param[type].adp_param;

	target_t_h = param->target_t + param->target_t_range;
	target_t_l = param->target_t - param->target_t_range;

	if (clmutt_data.cooler_param[type].adp_state == 1) {
		int tt_MD = param->target_t - curr_temp;	/* unit: mC */

		/* Check if it is triggered */
		if (!param->is_triggered) {
			if (curr_temp < param->target_t)
				return 0;

			param->is_triggered = 1;
		}

		/* Adjust total power budget if necessary */
		if (curr_temp >= target_t_h)
			param->tput_limit += (tt_MD / param->tt_high);
		else if (curr_temp <= target_t_l)
			param->tput_limit += (tt_MD / param->tt_low);

		/* Adjust MUTT level  */
		if (param->tput_limit >= 100) {
			decrease_mutt_limit(type);
			param->tput_limit = 0;
		} else if (param->tput_limit <= -100) {
			increase_mutt_limit(type);
			param->tput_limit = 0;
		}
	} else {
		if (param->is_triggered) {
			param->is_triggered = 0;
			param->tput_limit = 0;
			unlimit_mutt_limit(type);
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
	unsigned int type = *(unsigned long *)cdev->devdata;

	if (IS_MUTT_TYPE_VALID(type))
		*state = clmutt_data.cooler_param[type].adp_state;
	else
		*state = 0;

	mtk_cooler_mutt_dprintk("[%s] %s %lu (0:adp mutt off; 1:adp mutt on)\n",
						__func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_adp_mutt_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	unsigned int type = *(unsigned int *)cdev->devdata;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if ((state != 0) && (state != 1)) {
		mtk_cooler_mutt_dprintk_always(
		"[%s] %s: Invalid input(0:adp mutt off; 1:adp mutt on)\n",
			__func__, cdev->type);
		return 0;
	}

	mutex_lock(&clmutt_data.lock);

	if (IS_MD_OFF_OR_NO_IMS(clmutt_data.cur_level)) {
		mtk_cooler_mutt_dprintk_always("[%s] %s: MD OFF or noIMS!!\n",
						__func__, cdev->type);
		goto end;
	}

	mtk_cooler_mutt_dprintk("[%s] %s %lu (0:adp mutt off; 1:adp mutt on)\n",
						__func__, cdev->type, state);
	clmutt_data.cooler_param[type].adp_state = state;

	if (type == (unsigned int)MUTT_NR)
		adaptive_tput_limit((unsigned int)MUTT_NR,
			mtk_thermal_get_temp(MTK_THERMAL_SENSOR_NR_PA));
	else
		adaptive_tput_limit((unsigned int)MUTT_LTE,
			mtk_thermal_get_temp(MTK_THERMAL_SENSOR_MD_PA));

	mtk_cl_mutt_set_mutt_limit(type);

end:
	mutex_unlock(&clmutt_data.lock);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_adp_mutt_ops = {
	.get_max_state = mtk_cl_adp_mutt_get_max_state,
	.get_cur_state = mtk_cl_adp_mutt_get_cur_state,
	.set_cur_state = mtk_cl_adp_mutt_set_cur_state,
};
#endif

static int mtk_cl_scg_off_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
				cdev->type, *state);

	return 0;
}

static int mtk_cl_scg_off_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = clmutt_data.scg_off_state;
	mtk_cooler_mutt_dprintk(
		"[%s] %lu (0: SCG on;  1: SCG off)\n", __func__, *state);
	return 0;
}

static int mtk_cl_scg_off_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	if ((state >= 0) && (state <= 1))
		mtk_cooler_mutt_dprintk(
			"[%s] %lu (0: SCG on; 1: SCG off)\n", __func__, state);
	else {
		mtk_cooler_mutt_dprintk(
		"[%s]: Invalid input (0:SCG on; 1: SCG off)\n", __func__);

		return 0;
	}

	mutex_lock(&clmutt_data.lock);

	if (IS_MD_OFF_OR_NO_IMS(clmutt_data.cur_level)) {
		mtk_cooler_mutt_dprintk_always("[%s] MD OFF or noIMS!!\n",
						__func__);
		goto end;
	}

	mtk_cooler_mutt_dprintk("[%s] %lu\n", __func__, state);

	if (clmutt_data.scg_off_state == state)
		goto end;

	if (clmutt_send_scg_off_cmd((int)state))
		clmutt_data.cur_limit = 0;
	else
		clmutt_data.scg_off_state = state;

end:
	mutex_unlock(&clmutt_data.lock);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_scg_off_ops = {
	.get_max_state = mtk_cl_scg_off_get_max_state,
	.get_cur_state = mtk_cl_scg_off_get_cur_state,
	.set_cur_state = mtk_cl_scg_off_set_cur_state,
};

static int mtk_cl_tx_pwr_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
				cdev->type, *state);

	return 0;
}

static int mtk_cl_tx_pwr_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	unsigned int id = *(unsigned int *)cdev->devdata;
	unsigned int type = id / MAX_NUM_TX_PWR_LV;
	unsigned int cid = id % MAX_NUM_TX_PWR_LV;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if (cid >= MAX_NUM_TX_PWR_LV) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid tx pwr id %d\n",
			__func__, cdev->type, cid);
		return 0;
	}

	*state = clmutt_data.cooler_param[type].tx_pwr_state[cid];
	mtk_cooler_mutt_dprintk("%s() %s %lu\n", __func__,
							cdev->type, *state);
	return 0;
}

static int mtk_cl_tx_pwr_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	unsigned int id = *(unsigned int *)cdev->devdata;
	unsigned int type = id / MAX_NUM_TX_PWR_LV;
	unsigned int cid = id % MAX_NUM_TX_PWR_LV;
	int i, target_lv = -1, ret = -1;

	if (!IS_MUTT_TYPE_VALID(type)) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid mutt type %d\n",
			__func__, cdev->type, type);
		return 0;
	}

	if (cid >= MAX_NUM_TX_PWR_LV) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] %s: Invalid tx pwr id %d\n",
			__func__, cdev->type, cid);
		return 0;
	}

	if ((state != 0) && (state != 1)) {
		mtk_cooler_mutt_dprintk_always(
		"[%s] %s: Invalid input(0:tx pwr off; 1:tx pwr on)\n",
			__func__, cdev->type);
		return 0;
	}

	mutex_lock(&clmutt_data.lock);

	if (IS_MD_OFF_OR_NO_IMS(clmutt_data.cur_level)) {
		mtk_cooler_mutt_dprintk_always("[%s] %s: MD OFF or noIMS!!\n",
						__func__, cdev->type);
		goto end;
	}

	mtk_cooler_mutt_dprintk("[%s] %s %d %lu\n", __func__,
		cdev->type, cid, state);

	clmutt_data.cooler_param[type].tx_pwr_state[cid] = state;
	for_each_tx_pwr_lv(i) {
		if (clmutt_data.cooler_param[type].tx_pwr_state[i] == 1)
			target_lv = i;
	}

	if (target_lv == clmutt_data.cooler_param[type].target_tx_pwr_level)
		goto end;
	else if (target_lv < 0)
		ret = clmutt_send_reduce_tx_pwr_cmd(type, 0); /* cancel */
	else if (target_lv < MAX_NUM_TX_PWR_LV)
		ret = clmutt_send_reduce_tx_pwr_cmd(type,
			clmutt_data.cooler_param[type].tx_pwr_db[target_lv]);

	if (ret != 0) {
		clmutt_data.cur_limit = 0;
	} else {
		clmutt_data.cooler_param[type].target_tx_pwr_level = target_lv;
		if (target_lv < 0)
			mtk_cooler_mutt_dprintk_always(
			"[%s] %s:cancel reduce tx pwr!\n",
			__func__, clmutt_data.cooler_param[type].name);
		else
			mtk_cooler_mutt_dprintk_always(
			"[%s] %s:reduce tx %d dBm\n",
			__func__, clmutt_data.cooler_param[type].name,
			clmutt_data.cooler_param[type].tx_pwr_db[target_lv]);
	}

end:
	mutex_unlock(&clmutt_data.lock);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_tx_pwr_ops = {
	.get_max_state = mtk_cl_tx_pwr_get_max_state,
	.get_cur_state = mtk_cl_tx_pwr_get_cur_state,
	.set_cur_state = mtk_cl_tx_pwr_set_cur_state,
};

static int mtk_cooler_mutt_register_ltf(void)
{
	struct clmutt_cooler *p_cooler;
	int i, j, id = 0, id_tx_pwr = 0;

	mtk_cooler_mutt_dprintk("register ltf\n");

	clmutt_data.scg_off_dev = mtk_thermal_cooling_device_register(
		"mtk-cl-scg-off", NULL, &mtk_cl_scg_off_ops);

	for_each_mutt_type(i) {
		char postfix[4] = {"\0"};
		char temp[20] = { 0 };

		if (i == MUTT_NR)
			strncpy(postfix, "-nr", sizeof("-nr"));

		p_cooler = &clmutt_data.cooler_param[i];
		for_each_mutt_cooler_instance(j) {
			p_cooler->id[j] = id;
			snprintf(temp, sizeof(temp), "mtk-cl-mutt%02d%s",
				j, postfix);
			/* put mutt state to cooler devdata */
			p_cooler->dev[j] =
				mtk_thermal_cooling_device_register(temp,
					(void *)&p_cooler->id[j],
					&mtk_cl_mutt_ops);
			id++;
		}
		for_each_tx_pwr_lv(j) {
			p_cooler->id_tx_pwr[j] = id_tx_pwr;
			snprintf(temp, sizeof(temp), "mtk-cl-tx-pwr%02d%s",
				j, postfix);
			/* put mutt state to cooler devdata */
			p_cooler->tx_pwr_dev[j] =
				mtk_thermal_cooling_device_register(temp,
					(void *)&p_cooler->id_tx_pwr[j],
					&mtk_cl_tx_pwr_ops);
			id_tx_pwr++;
		}

		snprintf(temp, sizeof(temp), "mtk-cl-noIMS%s", postfix);
		p_cooler->noIMS_dev = mtk_thermal_cooling_device_register(
			temp, (void *)&p_cooler->type,
			&mtk_cl_noIMS_ops);
		snprintf(temp, sizeof(temp), "mtk-cl-mdoff%s", postfix);
		p_cooler->mdoff_dev = mtk_thermal_cooling_device_register(
			temp, (void *)&p_cooler->type,
			&mtk_cl_mdoff_ops);
#if FEATURE_ADAPTIVE_MUTT
		snprintf(temp, sizeof(temp), "mtk-cl-adp-mutt%s", postfix);
		p_cooler->adp_dev = mtk_thermal_cooling_device_register(
			temp, (void *)&p_cooler->type,
			&mtk_cl_adp_mutt_ops);

#endif
	}

	return 0;
}

static void mtk_cooler_mutt_unregister_ltf(void)
{
	struct clmutt_cooler *p_cooler;
	int i, j;

	mtk_cooler_mutt_dprintk("unregister ltf\n");

	if (clmutt_data.scg_off_dev) {
		mtk_thermal_cooling_device_unregister(clmutt_data.scg_off_dev);
		clmutt_data.scg_off_dev = NULL;
		clmutt_data.scg_off_state = 0;
	}

	for_each_mutt_type(i) {
		p_cooler = &clmutt_data.cooler_param[i];

		for_each_mutt_cooler_instance(j) {
			if (p_cooler->dev[j]) {
				mtk_thermal_cooling_device_unregister(
					p_cooler->dev[j]);
				p_cooler->dev[j] = NULL;
				p_cooler->state[j] = 0;
			}
		}
		for_each_tx_pwr_lv(j) {
			if (p_cooler->tx_pwr_dev[j]) {
				mtk_thermal_cooling_device_unregister(
					p_cooler->tx_pwr_dev[j]);
				p_cooler->tx_pwr_dev[j] = NULL;
				p_cooler->tx_pwr_state[j] = 0;
			}
		}
		if (p_cooler->noIMS_dev) {
			mtk_thermal_cooling_device_unregister(
				p_cooler->noIMS_dev);
			p_cooler->noIMS_dev = NULL;
			p_cooler->noIMS_state = 0;
		}
		if (p_cooler->mdoff_dev) {
			mtk_thermal_cooling_device_unregister(
				p_cooler->mdoff_dev);
			p_cooler->mdoff_dev = NULL;
			p_cooler->mdoff_state = 0;
		}
#if FEATURE_ADAPTIVE_MUTT
		if (p_cooler->adp_dev) {
			mtk_thermal_cooling_device_unregister(
				p_cooler->adp_dev);
			p_cooler->adp_dev = NULL;
			p_cooler->adp_state = 0;
		}
#endif
	}
}

static int copy_proc_data(const char __user *buffer, size_t count,
	char *data)
{
	int len = 0;

	len = (count < (PROC_BUFFER_LEN - 1)) ? count : (PROC_BUFFER_LEN - 1);
	if (copy_from_user(data, buffer, len))
		return -EFAULT;

	data[len] = '\0';

	return len;
}

static int clmutt_setting_proc_read(struct seq_file *m, void *v)
{
	int i, j;

	seq_printf(m, "cur_limit: 0x%08x, cur_level: %d\n",
		clmutt_data.cur_limit, clmutt_data.cur_level);
	seq_printf(m, "\tscgoff_state:%lu\n", clmutt_data.scg_off_state);

	for_each_mutt_type(i) {
		seq_printf(m, "[%d][%s]\n",
			clmutt_data.cooler_param[i].type,
			clmutt_data.cooler_param[i].name);
		seq_printf(m, "\ttarget_level:%d\n",
			clmutt_data.cooler_param[i].target_level);
		seq_printf(m, "\ttarget_tx_pwr_level:%d\n",
			clmutt_data.cooler_param[i].target_tx_pwr_level);
#if FEATURE_ADAPTIVE_MUTT
		seq_printf(m, "\ttarget_t:%d\n",
			clmutt_data.cooler_param[i].adp_param.target_t);
		seq_printf(m, "\tadp_level:%d\n",
			clmutt_data.cooler_param[i].adp_level);
		seq_printf(m, "\tadp_state:%lu\n",
			clmutt_data.cooler_param[i].adp_state);
#endif
		seq_printf(m, "\tmdoff_state:%lu\n",
			clmutt_data.cooler_param[i].mdoff_state);
		seq_printf(m, "\tnoIMS_state:%lu\n",
			clmutt_data.cooler_param[i].noIMS_state);

		for_each_mutt_cooler_instance(j) {
			unsigned long state;

			MTK_CL_MUTT_GET_CURR_STATE(state,
				clmutt_data.cooler_param[i].state[j]);
			seq_printf(m, "\tstate[%d]:%lu\n", j, state);
		}
		for_each_tx_pwr_lv(j) {
			seq_printf(m, "\ttx_pwr_state[%d]:%lu, pwr:%d dBm\n",
				j,
				clmutt_data.cooler_param[i].tx_pwr_state[j],
				clmutt_data.cooler_param[i].tx_pwr_db[j]);
		}
	}

	return 0;
}

static ssize_t clmutt_setting_proc_write(
struct file *filp, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN];
	int len = 0, i, klog_on, target_t_nr, target_t;
	int tx_pwr[MAX_NUM_TX_PWR_LV];
	int tx_pwr_nr[MAX_NUM_TX_PWR_LV];
	int scan_count = 0;

	len = copy_proc_data(buffer, count, desc);

	scan_count = sscanf(desc, "%d %d %d %d %d %d %d %d %d",
		&klog_on, &target_t_nr, &target_t, &tx_pwr_nr[0],
		&tx_pwr_nr[1], &tx_pwr_nr[2], &tx_pwr[0],
		&tx_pwr[1], &tx_pwr[2]);
	if (scan_count == 3 + NR_MUTT_TYPE * MAX_NUM_TX_PWR_LV) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] klog_on:%d, target_t_nr:%d, target_t:%d\n",
			__func__, klog_on, target_t_nr, target_t);

		mutex_lock(&clmutt_data.lock);
		clmutt_data.klog_on = klog_on;
		clmutt_data.cooler_param[MUTT_NR].adp_param.target_t
			= target_t_nr;
		clmutt_data.cooler_param[MUTT_LTE].adp_param.target_t
			= target_t;
		for_each_tx_pwr_lv(i) {
			clmutt_data.cooler_param[MUTT_NR].tx_pwr_db[i]
				= tx_pwr_nr[i];
			clmutt_data.cooler_param[MUTT_LTE].tx_pwr_db[i]
				= tx_pwr[i];
		}
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_tm_pid_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", clmutt_data.tm_input_pid);

	mtk_cooler_mutt_dprintk("[%s] %d\n",
		__func__, clmutt_data.tm_input_pid);

	return 0;
}

static ssize_t clmutt_tm_pid_proc_write(
struct file *filp, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN] = {0};
	int ret = 0, len = 0;

	len = copy_proc_data(buffer, count, desc);

	ret = kstrtouint(desc, 10, &clmutt_data.tm_input_pid);
	if (ret)
		WARN_ON_ONCE(1);

	mtk_cooler_mutt_dprintk("[%s] %s = %d\n",
		__func__, desc, clmutt_data.tm_input_pid);

	return len;
}

#if FEATURE_THERMAL_DIAG
static int clmutt_tmd_pid_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", clmutt_data.tmd_input_pid);
	mtk_cooler_mutt_dprintk("%s %d\n",
		__func__, clmutt_data.tmd_input_pid);

	return 0;
}

static ssize_t clmutt_tmd_pid_proc_write(
struct file *filp, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN] = {0};
	int ret = 0, len = 0;

	len = copy_proc_data(buffer, count, desc);

	ret = kstrtouint(desc, 10, &clmutt_data.tmd_input_pid);
	if (ret)
		WARN_ON_ONCE(1);

	mtk_cooler_mutt_dprintk("%s %s = %d\n",
		__func__, desc, clmutt_data.tmd_input_pid);

	return len;
}
#endif

static int clmutt_klog_on_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "klog_on: %d\n", clmutt_data.klog_on);

	return 0;
}

static ssize_t clmutt_klog_on_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN] = {0};
	int len = 0, klog_on;

	len = copy_proc_data(buffer, count, desc);

	if (kstrtoint(desc, 10, &klog_on) == 0) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] klog_on:%d\n", __func__, klog_on);

		mutex_lock(&clmutt_data.lock);
		clmutt_data.klog_on = klog_on;
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_duty_ctrl_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "no IMS: %d\n", clmutt_data.no_ims);
	seq_printf(m, "active/suspend: %d/%d\n",
		clmutt_data.active_period_100ms,
		clmutt_data.suspend_period_100ms);

	return 0;
}

static ssize_t clmutt_duty_ctrl_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN];
	int ret = 0, len = 0, no_ims, active, suspend;
	int scan_count = 0;
	unsigned int limit;

	len = copy_proc_data(buffer, count, desc);

	scan_count = sscanf(desc, "%d %d %d", &no_ims, &active, &suspend);
	if (scan_count == 3) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] no_ims:%d, active/suspend:%d/%d\n",
			__func__, no_ims, active, suspend);

		mutex_lock(&clmutt_data.lock);

		ret = clmutt_disable_cooler_lv_ctrl();
		if (ret)
			goto end;

		if (no_ims)
			/* set active/suspend=1/255 for no IMS case */
			limit = (1 << MUTT_ACTIVATED_OFFSET)
				| (255 << MUTT_SUSPEND_OFFSET)
				| MUTT_THROTTLING_IMS_DISABLE;
		else if (active >= 1 && active <= 255
			&& suspend >= 1 && suspend <= 255)
			limit = (active << MUTT_ACTIVATED_OFFSET)
				| (suspend << MUTT_SUSPEND_OFFSET)
				| MUTT_THROTTLING_IMS_ENABLE;
		else
			limit = TMC_THROTTLING_THROT_DISABLE;
		ret = clmutt_send_tmc_cmd(limit);

		mtk_cooler_mutt_dprintk_always(
			"[%s] noims/a/s:%d/%d/%d(0x%08x). ret:%d, bcnt:%lu\n",
			__func__, no_ims, active, suspend, limit,
			ret, clmutt_data.last_md_boot_cnt);

		if (ret) {
			clmutt_data.cur_limit = 0;
		} else {
			clmutt_data.no_ims = no_ims;
			clmutt_data.active_period_100ms = active;
			clmutt_data.suspend_period_100ms = suspend;
		}

end:
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_ca_ctrl_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "CA ctrl: %d\n", clmutt_data.ca_ctrl);

	return 0;
}

static ssize_t clmutt_ca_ctrl_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN] = {0};
	int ret = 0, len = 0, ca_ctrl;
	unsigned int limit;

	len = copy_proc_data(buffer, count, desc);

	if (kstrtoint(desc, 10, &ca_ctrl) == 0) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] ca_ctrl:%d\n", __func__, ca_ctrl);

		mutex_lock(&clmutt_data.lock);

		if (ca_ctrl == clmutt_data.ca_ctrl)
			goto end;

		ret = clmutt_disable_cooler_lv_ctrl();
		if (ret)
			goto end;

		limit = (ca_ctrl) ? TMC_CA_CTRL_CA_OFF : TMC_CA_CTRL_CA_ON;
		ret = clmutt_send_tmc_cmd(limit);

		mtk_cooler_mutt_dprintk_always(
			"[%s] set CA ctrl:%d(0x%08x). ret:%d, bcnt:%lu\n",
			__func__, ca_ctrl, limit,
			ret, clmutt_data.last_md_boot_cnt);

		if (ret)
			clmutt_data.cur_limit = 0;
		else
			clmutt_data.ca_ctrl = ca_ctrl;

end:
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_pa_ctrl_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "PA ctrl: %d\n", clmutt_data.pa_ctrl);

	return 0;
}

static ssize_t clmutt_pa_ctrl_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN] = {0};
	int ret = 0, len = 0, pa_ctrl;
	unsigned int limit;

	len = copy_proc_data(buffer, count, desc);

	if (kstrtoint(desc, 10, &pa_ctrl) == 0) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] pa_ctrl:%d\n", __func__, pa_ctrl);

		mutex_lock(&clmutt_data.lock);

		if (pa_ctrl == clmutt_data.pa_ctrl)
			goto end;

		ret = clmutt_disable_cooler_lv_ctrl();
		if (ret)
			goto end;

		limit = (pa_ctrl) ? TMC_PA_CTRL_PA_OFF_1PA
			: TMC_PA_CTRL_PA_ALL_ON;
		ret = clmutt_send_tmc_cmd(limit);

		mtk_cooler_mutt_dprintk_always(
			"[%s] set PA ctrl:%d(0x%08x). ret:%d, bcnt:%lu\n",
			__func__, pa_ctrl, limit,
			ret, clmutt_data.last_md_boot_cnt);

		if (ret)
			clmutt_data.cur_limit = 0;
		else
			clmutt_data.pa_ctrl = pa_ctrl;

end:
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_cooler_lv_proc_read(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "cooler lv setting:\n");

	for_each_mutt_type(i)
		seq_printf(m, "[%s] = %d\n",
			clmutt_data.cooler_param[i].name,
			clmutt_data.cooler_lv_ctrl[i]);

	return 0;
}

static ssize_t clmutt_cooler_lv_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN];
	int ret = 0, len = 0, type, lv;
	int scan_count = 0;
	unsigned int limit;

	len = copy_proc_data(buffer, count, desc);

	scan_count = sscanf(desc, "%d %d", &type, &lv);
	if (scan_count == 2) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] type:%d lv:%d\n", __func__, type, lv);

		if (!IS_MUTT_TYPE_VALID(type)) {
			mtk_cooler_mutt_dprintk_always(
				"[%s] Invalid mutt type %d\n", __func__, type);
			return -EINVAL;
		}

		mutex_lock(&clmutt_data.lock);

		/* not support MD off by setting cooler LV */
		if (lv < 0 || lv > MAX_NUM_INSTANCE_MTK_COOLER_MUTT) {
			limit = MUTT_TMC_COOLER_LV_DISABLE;
			ret = clmutt_disable_cooler_lv_ctrl();
			lv = -1;
		} else {
			limit = clmutt_level_selection(lv, type);
			ret = clmutt_send_tmc_cmd(limit);
		}

		mtk_cooler_mutt_dprintk_always(
			"[%s] set %s cool_lv:%d(0x%08x). ret:%d, bcnt:%lu\n",
			__func__, clmutt_data.cooler_param[type].name, lv,
			limit, ret, clmutt_data.last_md_boot_cnt);

		if (ret) {
			clmutt_data.cur_limit = 0;
		} else {
			clmutt_data.cooler_lv_ctrl[type] = lv;
			clmutt_data.cur_level = lv;
		}

		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_scg_off_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "scg off: %d\n", clmutt_data.scg_off);

	return 0;
}

static ssize_t clmutt_scg_off_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN] = {0};
	int ret = 0, len = 0, off;

	len = copy_proc_data(buffer, count, desc);

	if (kstrtoint(desc, 10, &off) == 0) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] off:%d\n", __func__, off);

		mutex_lock(&clmutt_data.lock);

		if (off == clmutt_data.scg_off)
			goto end;

		ret = clmutt_disable_cooler_lv_ctrl();
		if (ret)
			goto end;

		ret = clmutt_send_scg_off_cmd(off);
		if (ret)
			clmutt_data.cur_limit = 0;
		else
			clmutt_data.scg_off = off;

end:
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

static int clmutt_tx_pwr_proc_read(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "Reduce tx power: (unit: db)\n");

	for_each_mutt_type(i)
		seq_printf(m, "[%s] = %d\n",
			clmutt_data.cooler_param[i].name,
			clmutt_data.reduce_tx_pwr[i]);

	return 0;
}

static ssize_t clmutt_tx_pwr_proc_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[PROC_BUFFER_LEN];
	int ret = 0, len = 0, type, tx_pwr;
	int scan_count = 0;

	len = copy_proc_data(buffer, count, desc);

	scan_count = sscanf(desc, "%d %d", &type, &tx_pwr);
	if (scan_count == 2) {
		mtk_cooler_mutt_dprintk_always(
			"[%s] type:%d tx_pwr:%d\n", __func__, type, tx_pwr);

		if (!IS_MUTT_TYPE_VALID(type)) {
			mtk_cooler_mutt_dprintk_always(
				"[%s] Invalid mutt type %d\n", __func__, type);
			return -EINVAL;
		}

		mutex_lock(&clmutt_data.lock);

		if (tx_pwr == clmutt_data.reduce_tx_pwr[type])
			goto end;

		ret = clmutt_send_reduce_tx_pwr_cmd(type, tx_pwr);
		if (ret)
			clmutt_data.cur_limit = 0;
		else
			clmutt_data.reduce_tx_pwr[type] = tx_pwr;

end:
		mutex_unlock(&clmutt_data.lock);

		return len;
	}

	mtk_cooler_mutt_dprintk("[%s] bad arg\n", __func__);

	return -EINVAL;
}

PROC_FOPS_RW(setting);
PROC_FOPS_RW(tm_pid);
#if FEATURE_THERMAL_DIAG
PROC_FOPS_RW(tmd_pid);
#endif
PROC_FOPS_RW(klog_on);
PROC_FOPS_RW(duty_ctrl);
PROC_FOPS_RW(ca_ctrl);
PROC_FOPS_RW(pa_ctrl);
PROC_FOPS_RW(cooler_lv);
PROC_FOPS_RW(scg_off);
PROC_FOPS_RW(tx_pwr);

static int __init mtk_cooler_mutt_init(void)
{
	int err = 0;

	mtk_cooler_mutt_dprintk("init\n");

	err = mtk_cooler_mutt_register_ltf();
	if (err)
		goto err_unreg;

	/* create a proc file */
	{
		struct proc_dir_entry *dir_entry = NULL;
		struct pentry {
			const char *name;
			const struct file_operations *fops;
		};

		const struct pentry entries[] = {
			PROC_ENTRY(clmutt_setting),
			PROC_ENTRY(clmutt_tm_pid),
#if FEATURE_THERMAL_DIAG
			PROC_ENTRY(clmutt_tmd_pid),
#endif
			PROC_ENTRY(clmutt_klog_on),
			PROC_ENTRY(clmutt_duty_ctrl),
			PROC_ENTRY(clmutt_ca_ctrl),
			PROC_ENTRY(clmutt_pa_ctrl),
			PROC_ENTRY(clmutt_cooler_lv),
			PROC_ENTRY(clmutt_scg_off),
			PROC_ENTRY(clmutt_tx_pwr),
		};

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry) {
			mtk_cooler_mutt_dprintk_always(
				"[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
		} else {
			struct proc_dir_entry *entry = NULL;
			kuid_t uid = KUIDT_INIT(0);
			kgid_t gid = KGIDT_INIT(1000);
			int i;

			for (i = 0; i < ARRAY_SIZE(entries); i++) {
				entry = proc_create(entries[i].name, 0664,
					dir_entry, entries[i].fops);
				if (entry)
					proc_set_user(entry, uid, gid);
			}
		}
	}

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
