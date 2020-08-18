// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mtk_cpuidle_sysfs.h>

#include "mtk_cpuidle_cpc.h"
#include "mtk_cpuidle_status.h"

#define MT6873_CPUIDLE_PROFILE_OP(op, _priv) ({\
	op.fs_read = mt6873_cpuidle_profile_read;\
	op.fs_write = mt6873_cpuidle_profile_write;\
	op.priv = _priv; })

#define MT6873_CPUIDLE_PROFILE_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	MT6873_CPUIDLE_PROFILE_OP(_n.op, &_n); })

enum {
	TYPE_CPC,
	TYPE_KERNEL,
	TYPE_RATIO,
	TYPE_TARGET_STATE,

	NF_TYPE_PROFILE_MAX
};

struct mtk_lp_sysfs_handle mt6873_entry_cpuidle_profile;

struct MT6873_CPUIDLE_NODE profile_cpc;
struct MT6873_CPUIDLE_NODE profile_kernel;
struct MT6873_CPUIDLE_NODE profile_ratio;
struct MT6873_CPUIDLE_NODE profile_target_state;

static const char *node_name[NF_TYPE_PROFILE_MAX] = {
	[TYPE_CPC]		= "profile cpc",
	[TYPE_KERNEL]		= "profile kernel",
	[TYPE_RATIO]		= "profile ratio",
	[TYPE_TARGET_STATE]	= "profile target state",
};

static ssize_t mt6873_cpuidle_profile_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct MT6873_CPUIDLE_NODE *node =
			(struct MT6873_CPUIDLE_NODE *)priv;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_CPC:
		mtk_cpc_prof_lat_dump(&p, &sz);
		break;

	case TYPE_RATIO:
		mtk_cpuidle_prof_ratio_dump(&p, &sz);
		break;

	case TYPE_KERNEL:
	case TYPE_TARGET_STATE:
		mtk_dbg_cpuidle_log("%s: %s\n",
			__func__, node_name[node->type]);
		break;

	default:
		mtk_dbg_cpuidle_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t mt6873_cpuidle_profile_write(char *FromUserBuf,
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
	case TYPE_CPC:
		parm ? mtk_cpc_prof_start() : mtk_cpc_prof_stop();
		break;

	case TYPE_KERNEL:
		/* Start/Stop kernel profile */
		break;

	case TYPE_RATIO:
		parm ? mtk_cpuidle_prof_ratio_start() :
				mtk_cpuidle_prof_ratio_stop();
		break;

	case TYPE_TARGET_STATE:
		/* Set target state */
		break;

	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

void mtk_cpuidle_profile_init(void)
{

	mtk_cpuidle_sysfs_sub_entry_add("profile", MTK_CPUIDLE_SYS_FS_MODE,
				NULL, &mt6873_entry_cpuidle_profile);

	MT6873_CPUIDLE_PROFILE_NODE_INIT(profile_cpc, "cpc",
				    TYPE_CPC);
	mtk_cpuidle_sysfs_sub_entry_node_add(profile_cpc.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&profile_cpc.op,
					&mt6873_entry_cpuidle_profile,
					&profile_cpc.handle);

	MT6873_CPUIDLE_PROFILE_NODE_INIT(profile_kernel, "kernel",
				    TYPE_KERNEL);
	mtk_cpuidle_sysfs_sub_entry_node_add(profile_kernel.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&profile_kernel.op,
					&mt6873_entry_cpuidle_profile,
					&profile_kernel.handle);

	MT6873_CPUIDLE_PROFILE_NODE_INIT(profile_ratio, "ratio",
				    TYPE_RATIO);
	mtk_cpuidle_sysfs_sub_entry_node_add(profile_ratio.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&profile_ratio.op,
					&mt6873_entry_cpuidle_profile,
					&profile_ratio.handle);

	MT6873_CPUIDLE_PROFILE_NODE_INIT(profile_target_state, "target_state",
				    TYPE_TARGET_STATE);
	mtk_cpuidle_sysfs_sub_entry_node_add(profile_target_state.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&profile_target_state.op,
					&mt6873_entry_cpuidle_profile,
					&profile_target_state.handle);
}
