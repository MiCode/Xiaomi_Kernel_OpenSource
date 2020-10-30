// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <mtk_cpuidle_sysfs.h>

#include "mtk_cpuidle_cpc.h"
#include "mtk_cpuidle_status.h"

#define LPM_CPUIDLE_PROFILE_OP(op, _priv) ({\
	op.fs_read = lpm_cpuidle_profile_read;\
	op.fs_write = lpm_cpuidle_profile_write;\
	op.priv = _priv; })

#define LPM_CPUIDLE_PROFILE_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	LPM_CPUIDLE_PROFILE_OP(_n.op, &_n); })

enum {
	TYPE_CPC,
	TYPE_RATIO,

	NF_TYPE_PROFILE_MAX
};

struct mtk_lp_sysfs_handle lpm_entry_cpuidle_profile;

struct MTK_CPUIDLE_NODE profile_cpc;
struct MTK_CPUIDLE_NODE profile_ratio;

static ssize_t lpm_cpuidle_profile_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct MTK_CPUIDLE_NODE *node =
			(struct MTK_CPUIDLE_NODE *)priv;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_CPC:
		mtk_cpc_prof_lat_dump(&p, &sz);
		break;

	case TYPE_RATIO:
		mtk_cpuidle_prof_ratio_dump(&p, &sz);
		break;

	default:
		mtk_dbg_cpuidle_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t lpm_cpuidle_profile_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct MTK_CPUIDLE_NODE *node =
				(struct MTK_CPUIDLE_NODE *)priv;
	unsigned int parm = 0;

	if (!FromUserBuf || !node)
		return -EINVAL;

	if (kstrtouint(FromUserBuf, 10, &parm) != 0)
		return -EINVAL;

	switch (node->type) {
	case TYPE_CPC:
		parm ? mtk_cpc_prof_start() : mtk_cpc_prof_stop();
		break;

	case TYPE_RATIO:
		parm ? mtk_cpuidle_prof_ratio_start() :
				mtk_cpuidle_prof_ratio_stop();
		break;

	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

void lpm_cpuidle_profile_init(void)
{
	mtk_cpuidle_sysfs_sub_entry_add("profile", MTK_CPUIDLE_SYS_FS_MODE,
				NULL, &lpm_entry_cpuidle_profile);

	LPM_CPUIDLE_PROFILE_NODE_INIT(profile_cpc, "cpc",
				    TYPE_CPC);
	mtk_cpuidle_sysfs_sub_entry_node_add(profile_cpc.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&profile_cpc.op,
					&lpm_entry_cpuidle_profile,
					&profile_cpc.handle);

	LPM_CPUIDLE_PROFILE_NODE_INIT(profile_ratio, "ratio",
				    TYPE_RATIO);
	mtk_cpuidle_sysfs_sub_entry_node_add(profile_ratio.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&profile_ratio.op,
					&lpm_entry_cpuidle_profile,
					&profile_ratio.handle);
}
