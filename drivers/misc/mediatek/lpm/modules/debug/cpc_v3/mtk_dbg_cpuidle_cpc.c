// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_cpuidle_cpc.h"
#include <mtk_cpuidle_sysfs.h>

#define MTK_CPUIDLE_CPC_OP(op, _priv) ({\
	op.fs_read = mtk_cpuidle_cpc_read;\
	op.fs_write = mtk_cpuidle_cpc_write;\
	op.priv = _priv; })

#define MTK_CPUIDLE_CPC_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	MTK_CPUIDLE_CPC_OP(_n.op, &_n); })

enum {
	TYPE_AUTO_OFF_MODE,
	TYPE_AUTO_OFF_THRES,

	NF_TYPE_CPC_MAX
};

struct mtk_lp_sysfs_handle mtk_entry_cpuidle_cpc;

struct MTK_CPUIDLE_NODE auto_off_mode;
struct MTK_CPUIDLE_NODE auto_off_thres;

static const char *node_name[NF_TYPE_CPC_MAX] = {
	[TYPE_AUTO_OFF_MODE]	= "auto_off",
	[TYPE_AUTO_OFF_THRES]	= "auto_off_thres",
};

static const char *node_op[NF_TYPE_CPC_MAX] = {
	[TYPE_AUTO_OFF_MODE]	= "[0|1]",
	[TYPE_AUTO_OFF_THRES]	= "[us]",
};

static ssize_t mtk_cpuidle_cpc_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct MTK_CPUIDLE_NODE *node =
			(struct MTK_CPUIDLE_NODE *)priv;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_AUTO_OFF_MODE:
		mtk_dbg_cpuidle_log("auto_off mode : %s\n",
			cpc_get_auto_off_sta() ?
			"Enabled" : "Disabled");
		break;

	case TYPE_AUTO_OFF_THRES:
		mtk_dbg_cpuidle_log("auto_off threshold = %u us\n",
			(unsigned int)cpc_get_auto_off_thres());
		break;

	default:
		mtk_dbg_cpuidle_log("unknown command\n");
		break;
	}

	mtk_dbg_cpuidle_log("\n======== Command Usage ========\n");
	mtk_dbg_cpuidle_log("echo %s > /proc/mtk_lpm/cpuidle/cpc/%s\n",
				node_op[node->type],
				node_name[node->type]);

	return p - ToUserBuf;
}

static ssize_t mtk_cpuidle_cpc_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct MTK_CPUIDLE_NODE *node =
				(struct MTK_CPUIDLE_NODE *)priv;
	unsigned int param = 0;
	unsigned long ret = 0;

	if (!FromUserBuf || !node)
		return -EINVAL;

	if (kstrtouint(FromUserBuf, 10, &param) != 0)
		return -EINVAL;

	switch (node->type) {
	case TYPE_AUTO_OFF_MODE:
		if (param)
			ret = cpc_auto_off_en();
		else
			ret = cpc_auto_off_dis();
		break;

	case TYPE_AUTO_OFF_THRES:
		ret = cpc_set_auto_off_thres(param);
		break;

	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

void mtk_cpuidle_cpc_init(void)
{

	mtk_cpuidle_sysfs_sub_entry_add("cpc", MTK_CPUIDLE_SYS_FS_MODE,
				NULL, &mtk_entry_cpuidle_cpc);

	MTK_CPUIDLE_CPC_NODE_INIT(auto_off_mode, "auto_off",
				    TYPE_AUTO_OFF_MODE);
	mtk_cpuidle_sysfs_sub_entry_node_add(auto_off_mode.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&auto_off_mode.op,
					&mtk_entry_cpuidle_cpc,
					&auto_off_mode.handle);

	MTK_CPUIDLE_CPC_NODE_INIT(auto_off_thres, "auto_off_thres",
				    TYPE_AUTO_OFF_THRES);
	mtk_cpuidle_sysfs_sub_entry_node_add(auto_off_thres.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&auto_off_thres.op,
					&mtk_entry_cpuidle_cpc,
					&auto_off_thres.handle);
}
