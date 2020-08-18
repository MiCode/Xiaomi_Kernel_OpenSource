// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>

#include <mtk_lp_plat_apmcu_mbox.h>
#include <mtk_cpuidle_sysfs.h>

#include "mtk_cpuidle_status.h"

#define MT6873_CPUIDLE_CONTROL_OP(op, _priv) ({\
	op.fs_read = mt6873_cpuidle_control_read;\
	op.fs_write = mt6873_cpuidle_control_write;\
	op.priv = _priv; })

#define MT6873_CPUIDLE_CONTROL_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	MT6873_CPUIDLE_CONTROL_OP(_n.op, &_n); })

enum {
	TYPE_ARMPLL_MODE,
	TYPE_BUCK_MODE,
	TYPE_LOG_EN,
	TYPE_NOTIFY_CM,
	TYPE_STRESS_EN,
	TYPE_STRESS_TIME,
	TYPE_STRESS_TIMER,

	NF_TYPE_CONTROL_MAX
};

struct mtk_lp_sysfs_handle mt6873_entry_cpuidle_control;

struct MT6873_CPUIDLE_NODE armpll_mode;
struct MT6873_CPUIDLE_NODE buck_mode;
struct MT6873_CPUIDLE_NODE log_enable;
struct MT6873_CPUIDLE_NODE notify_cm;
struct MT6873_CPUIDLE_NODE stress_enable;
struct MT6873_CPUIDLE_NODE stress_time;
struct MT6873_CPUIDLE_NODE stress_timer;

static const char *node_stress_name[NF_TYPE_CONTROL_MAX] = {
	[TYPE_ARMPLL_MODE]	= "armpll_mode",
	[TYPE_BUCK_MODE]	= "buck_mode",
	[TYPE_LOG_EN]		= "log",
	[TYPE_STRESS_EN]	= "stress",
	[TYPE_STRESS_TIME]	= "stress_time",
	[TYPE_STRESS_TIMER]	= "timer",
};

static const char *node_stress_param[NF_TYPE_CONTROL_MAX] = {
	[TYPE_ARMPLL_MODE]	= "[mode]",
	[TYPE_BUCK_MODE]	= "[mode]",
	[TYPE_LOG_EN]		= "[0|1]",
	[TYPE_STRESS_EN]	= "[0|1]",
	[TYPE_STRESS_TIME]	= "[us]",
	[TYPE_STRESS_TIMER]	= "[0|1]",
};

static ssize_t mt6873_cpuidle_control_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct MT6873_CPUIDLE_NODE *node =
			(struct MT6873_CPUIDLE_NODE *)priv;
	int mode;

	struct mode_param {
		int id;
		char *str;
	};

	const struct mode_param armpll_table[] = {
		{MCUPM_ARMPLL_ON, "on"},
		{MCUPM_ARMPLL_GATING, "gating"},
		{MCUPM_ARMPLL_OFF, "off"},
		{NF_MCUPM_ARMPLL_MODE, "unknown"},
	};

	const struct mode_param buck_table[] = {
		{MCUPM_BUCK_NORMAL_MODE, "on"},
		{MCUPM_BUCK_LP_MODE, "low power"},
		{MCUPM_BUCK_OFF_MODE, "off"},
		{NF_MCUPM_BUCK_MODE, "unknown"},
	};

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_ARMPLL_MODE:
		/* PLAT_MODULE */
		mode = mtk_get_mcupm_pll_mode();

		if (mode < 0 || mode > NF_MCUPM_ARMPLL_MODE)
			mode = NF_MCUPM_ARMPLL_MODE;

		mtk_dbg_cpuidle_log("armpll mode : %s\n",
			armpll_table[mode].str);
		break;

	case TYPE_BUCK_MODE:
		/* PLAT_MODULE */
		mode = mtk_get_mcupm_buck_mode();

		if (mode < 0 || mode > NF_MCUPM_BUCK_MODE)
			mode = NF_MCUPM_BUCK_MODE;

		mtk_dbg_cpuidle_log("Vproc/Vproc_sram buck mode : %s\n",
			buck_table[mode].str);
		break;

	case TYPE_LOG_EN:
		mtk_dbg_cpuidle_log("CPU idle log : %s\n",
			mtk_cpuidle_ctrl_log_sta_get() ?
			"Enable" : "Disable");
		break;

	case TYPE_NOTIFY_CM:
		/* PLAT_MODULE */
		mtk_dbg_cpuidle_log("mcupm cm mgr : %s\n",
			mtk_mcupm_cm_is_notified() ? "Enable" : "Disable");
		break;

	case TYPE_STRESS_EN:
		mtk_dbg_cpuidle_log("CPU idle stress : %s\n",
			mtk_cpuidle_get_stress_status() ?
			"Enable" : "Disable");
		break;

	case TYPE_STRESS_TIME:
		mtk_dbg_cpuidle_log("CPU idle stress interval time : %u\n",
			mtk_cpuidle_get_stress_time());
		break;

	case TYPE_STRESS_TIMER:
		mtk_dbg_cpuidle_log("Limit sleep timer : %s\n",
			mtk_cpuidle_ctrl_timer_sta_get() ?
			"Enable" : "Disable");
		break;

	default:
		mtk_dbg_cpuidle_log("unknown command\n");
		break;
	}

	mtk_dbg_cpuidle_log("\n======== Command Usage ========\n");

	if (node->type == TYPE_NOTIFY_CM)
		mtk_dbg_cpuidle_log("Read Only : Not support dynamic control\n");
	else
		mtk_dbg_cpuidle_log("echo %s > /proc/mtk_lpm/cpuidle/control/%s\n",
				node_stress_param[node->type],
				node_stress_name[node->type]);

	if (node->type == TYPE_ARMPLL_MODE) {
		mtk_dbg_cpuidle_log("mode:\n");
		for (mode = 0; mode < NF_MCUPM_ARMPLL_MODE; mode++)
			mtk_dbg_cpuidle_log("\t%d: %s\n",
					armpll_table[mode].id,
					armpll_table[mode].str);
	}

	if (node->type == TYPE_BUCK_MODE) {
		mtk_dbg_cpuidle_log("mode:\n");
		for (mode = 0; mode < NF_MCUPM_BUCK_MODE; mode++)
			mtk_dbg_cpuidle_log("\t%d: %s\n",
					buck_table[mode].id,
					buck_table[mode].str);
	}

	return p - ToUserBuf;
}

static ssize_t mt6873_cpuidle_control_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct MT6873_CPUIDLE_NODE *node =
				(struct MT6873_CPUIDLE_NODE *)priv;
	unsigned int parm = 0;

	if (!FromUserBuf || !node)
		return -EINVAL;

	if (kstrtouint(FromUserBuf, 10, &parm) != 0)
		return -EINVAL;

	switch (node->type) {
	case TYPE_ARMPLL_MODE:
		if (parm < NF_MCUPM_ARMPLL_MODE)
			/* PLAT_MODULE */
			mtk_set_mcupm_pll_mode(parm);
		break;

	case TYPE_BUCK_MODE:
		if (parm < NF_MCUPM_BUCK_MODE)
			/* PLAT_MODULE */
			mtk_set_mcupm_buck_mode(parm);
		break;

	case TYPE_LOG_EN:
		mtk_cpuidle_ctrl_log_en(!!parm);
		break;

	case TYPE_NOTIFY_CM:
		pr_info("Read Only : Not support dynamic control\n");
		break;

	case TYPE_STRESS_EN:
		mtk_cpuidle_set_stress_test(!!parm);
		break;

	case TYPE_STRESS_TIME:
		mtk_cpuidle_set_stress_time(parm);
		break;

	case TYPE_STRESS_TIMER:
		mtk_cpuidle_ctrl_timer_en(!!parm);
		break;

	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

void mtk_cpuidle_control_init(void)
{

	mtk_cpuidle_sysfs_sub_entry_add("control", MTK_CPUIDLE_SYS_FS_MODE,
				NULL, &mt6873_entry_cpuidle_control);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(armpll_mode, "armpll_mode",
				    TYPE_ARMPLL_MODE);
	mtk_cpuidle_sysfs_sub_entry_node_add(armpll_mode.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&armpll_mode.op,
					&mt6873_entry_cpuidle_control,
					&armpll_mode.handle);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(buck_mode, "buck_mode",
				    TYPE_BUCK_MODE);
	mtk_cpuidle_sysfs_sub_entry_node_add(buck_mode.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&buck_mode.op,
					&mt6873_entry_cpuidle_control,
					&buck_mode.handle);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(log_enable, "log",
				    TYPE_LOG_EN);
	mtk_cpuidle_sysfs_sub_entry_node_add(log_enable.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&log_enable.op,
					&mt6873_entry_cpuidle_control,
					&log_enable.handle);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(notify_cm, "notify_cm",
				    TYPE_NOTIFY_CM);
	mtk_cpuidle_sysfs_sub_entry_node_add(notify_cm.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&notify_cm.op,
					&mt6873_entry_cpuidle_control,
					&notify_cm.handle);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(stress_enable, "stress",
				    TYPE_STRESS_EN);
	mtk_cpuidle_sysfs_sub_entry_node_add(stress_enable.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&stress_enable.op,
					&mt6873_entry_cpuidle_control,
					&stress_enable.handle);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(stress_time, "stress_time",
				    TYPE_STRESS_TIME);
	mtk_cpuidle_sysfs_sub_entry_node_add(stress_time.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&stress_time.op,
					&mt6873_entry_cpuidle_control,
					&stress_time.handle);

	MT6873_CPUIDLE_CONTROL_NODE_INIT(stress_timer, "timer",
				    TYPE_STRESS_TIMER);
	mtk_cpuidle_sysfs_sub_entry_node_add(stress_timer.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&stress_timer.op,
					&mt6873_entry_cpuidle_control,
					&stress_timer.handle);

}
