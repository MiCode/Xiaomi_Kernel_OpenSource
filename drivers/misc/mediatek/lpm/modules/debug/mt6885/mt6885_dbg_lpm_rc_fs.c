// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>

#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>
#include <mtk_resource_constraint_v1.h>

#include <mt6885_pwr_ctrl.h>
#include <mt6885_dbg_fs_common.h>
#include <mt6885_cond.h>
#include <mt6885_spm_comm.h>

#include <mtk_lpm_sysfs.h>
#include <mtk_lp_sysfs.h>
#include <mtk_lpm_timer.h>

#define SPM_RC_UPDATE_COND_ID_MASK	0xffff
#define SPM_RC_UPDATE_COND_RC_ID_MASK	0xffff
#define SPM_RC_UPDATE_COND_RC_ID_SHIFT	(16)

#define SPM_RC_UPDATE_COND_CTRL_ID(rc, cond)\
	(((rc & SPM_RC_UPDATE_COND_RC_ID_MASK)\
		<< SPM_RC_UPDATE_COND_RC_ID_SHIFT)\
	| (cond & SPM_RC_UPDATE_COND_ID_MASK))


#define MT6885_LPM_RC_RATIO_DEFAULT		10000
#define mt6885_rc_log(buf, sz, len, fmt, args...) ({\
	if (len < sz)\
		len += scnprintf(buf + len, sz - len,\
				 fmt, ##args); })

#define MT6885_DBG_SMC(_id, _act, _rc, _param) ({\
	(unsigned long)mtk_lpm_smc_spm_dbg(_id, _act, _rc, _param); })


enum MT6885_RC_RATIO_TYPE {
	MT6885_RC_RATIO_UNKNOWN,
	MT6885_RC_RATIO_ENABLE,
	MT6885_RC_RATIO_DISABLE,
	MT6885_RC_RATIO_ENABLE_INFO,
	MT6885_RC_RATIO_INFO,
};

enum MT6885_RC_NODE_TYPE {
	MT6885_RC_NODE_STATE,
	MT6885_RC_NODE_RC_ENABLE,
	MT6885_RC_NODE_RC_STATE_SIMPLE,
	MT6885_RC_NODE_RC_STATE,
	MT6885_RC_NODE_COND_ENABLE,
	MT6885_RC_NODE_COND_STATE,
	MT6885_RC_NODE_COND_SET,
	MT6885_RC_NODE_COND_CLR,
	MT6885_RC_NODE_RATIO_ENABLE,
	MT6885_RC_NODE_RATIO_INTERVAL,
	MT6885_RC_NODE_VALID_BBLPM,
	MT6885_RC_NODE_VALID_TRACE,
	MT6885_RC_NODE_MAX
};

struct MT6885_RC_NODE {
	const char *name;
	int rc_id;
	int type;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

struct MT6885_RC_ENTERY {
	const char *name;
	struct mtk_lp_sysfs_handle handle;
};

struct MT6885_RC_COND_HANDLES {
	struct MT6885_RC_ENTERY root;
	struct MT6885_RC_NODE hSet;
	struct MT6885_RC_NODE hClr;
	struct MT6885_RC_NODE hState;
	struct MT6885_RC_NODE hEnable;
};

struct MT6885_RC_VALID_HANDLES {
	struct MT6885_RC_ENTERY root;
	struct MT6885_RC_NODE hBblpm;
	struct MT6885_RC_NODE hTrace;
};

struct MT6885_RC_RATIO_HANDLES {
	struct MT6885_RC_ENTERY root;
	struct MT6885_RC_NODE hInterval;
	struct MT6885_RC_NODE hEnable;
};

struct MT6885_RC_HANDLE_BASIC {
	struct MT6885_RC_ENTERY root;
	struct MT6885_RC_NODE hEnable;
	struct MT6885_RC_NODE hState;
};

struct MT6885_RC_HANDLE {
	struct MT6885_RC_HANDLE_BASIC basic;
	struct MT6885_RC_COND_HANDLES hCond;
	struct MT6885_RC_VALID_HANDLES valid;
};


#define MT6885_CONSTRAINT_GENERIC_OP(op, _priv) ({\
	op.fs_read = mt6885_generic_rc_read;\
	op.fs_write = mt6885_generic_rc_write;\
	op.priv = _priv; })


#define MT6885_GENERIC_RC_NODE_INIT(_n, _name, _id, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	_n.rc_id = _id;\
	MT6885_CONSTRAINT_GENERIC_OP(_n.op, &_n); })


struct mtk_lp_sysfs_handle mt6885_entry_rc;
struct MT6885_RC_HANDLE rc_dram;
struct MT6885_RC_HANDLE rc_syspll;
struct MT6885_RC_HANDLE rc_bus26m;
struct MT6885_RC_HANDLE_BASIC rc_cpu_buckldo;
struct MT6885_RC_RATIO_HANDLES rc_Ratio;
struct MT6885_RC_NODE rc_state;

struct mt_lpm_timer rc_ratio_timer;

static char *mt6885_spm_cond_cg_str[PLAT_SPM_COND_MAX] = {
	[PLAT_SPM_COND_MTCMOS_0]	= "MTCMOS_0",
	[PLAT_SPM_COND_CG_INFRA_0]	= "INFRA_0",
	[PLAT_SPM_COND_CG_INFRA_1]	= "INFRA_1",
	[PLAT_SPM_COND_CG_INFRA_2]	= "INFRA_2",
	[PLAT_SPM_COND_CG_INFRA_3]	= "INFRA_3",
	[PLAT_SPM_COND_CG_MMSYS_0]	= "MMSYS_0",
	[PLAT_SPM_COND_CG_MMSYS_1]	= "MMSYS_1",
	[PLAT_SPM_COND_CG_MMSYS_2]	= "MMSYS_2",
};

static char *mt6885_spm_cond_pll_str[PLAT_SPM_COND_PLL_MAX] = {
	[PLAT_SPM_COND_UNIVPLL]	= "UNIVPLL",
	[PLAT_SPM_COND_MFGPLL]	= "MFGPLL",
	[PLAT_SPM_COND_MSDCPLL]	= "MSDCPLL",
	[PLAT_SPM_COND_TVPLL]	= "TVPLL",
	[PLAT_SPM_COND_MMPLL]	= "MMPLL",
};


int mt6885_lpm_rc_cond_ctrl(int rc_id, unsigned int act,
			unsigned int cond_id, unsigned int value)
{
	unsigned int cond_ctrl_id;
	int res = 0;

	cond_ctrl_id = SPM_RC_UPDATE_COND_CTRL_ID(rc_id, cond_id);

	if (cond_id < PLAT_SPM_COND_MAX)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_COND_CTRL,
				    act, cond_ctrl_id, value);
	else if ((cond_id - PLAT_SPM_COND_MAX) < PLAT_SPM_COND_PLL_MAX)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_COND_CTRL,
				    act, cond_ctrl_id, !!value);
	else
		pr_info("[%s:%d] - unknown cond id = %u\n",
			__func__, __LINE__, cond_id);

	return res;
}

int mt6885_lpm_rc_ratio_timer_func(unsigned long long dur, void *priv)
{
	/* spm tick to ms: tick/32 (ms) */
	#define RC_RESIDENCY_COVERT(x)	(x>>5)
	#define RC_COVERT_RATIO(dur, x)\
		((RC_RESIDENCY_COVERT(x) * 100)/dur)

	unsigned long long rc_dram_t;
	unsigned long long rc_syspll_t;
	unsigned long long rc_bus26m_t;

	rc_dram_t = (unsigned long long)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_INFO,
			(MT_LPM_SMC_ACT_GET | MT_LPM_SMC_ACT_CLR),
			MT_RM_CONSTRAINT_ID_DRAM, 0);
	rc_syspll_t = (unsigned long long)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_INFO,
			(MT_LPM_SMC_ACT_GET | MT_LPM_SMC_ACT_CLR),
			MT_RM_CONSTRAINT_ID_SYSPLL, 0);
	rc_bus26m_t = (unsigned long long)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_INFO,
			(MT_LPM_SMC_ACT_GET | MT_LPM_SMC_ACT_CLR),
			MT_RM_CONSTRAINT_ID_BUS26M, 0);

	pr_info("[name:spm&][RC] ratio, duration_ms:%llu, dram:%llu%%, syspll:%llu%%, bus26m:%llu%%\n",
			dur, RC_COVERT_RATIO(dur, rc_dram_t),
			RC_COVERT_RATIO(dur, rc_syspll_t),
			RC_COVERT_RATIO(dur, rc_bus26m_t));
	return 0;
}

static ssize_t mt6885_lpm_rc_block_info(int rc_id,
					      char *ToUserBuf,
					      size_t sz)
{
	uint32_t block, b;
	int i;
	ssize_t len = 0;

	block = (uint32_t)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_COND_BLOCK,
				MT_LPM_SMC_ACT_GET, rc_id, 0);
	b = (uint32_t)
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_COND_CHECK,
				MT_LPM_SMC_ACT_GET, rc_id, 0);

	mt6885_rc_log(ToUserBuf, sz, len,
			"blocked=%u, blocked_cond=0x%08x\n",
			b, block);

	for (i = 0, b = block >> SPM_COND_BLOCKED_CG_IDX;
	     i < PLAT_SPM_COND_MAX; i++)
		mt6885_rc_log(ToUserBuf, sz, len,
				"[%2d] %8s=0x%08lx\n", i,
				mt6885_spm_cond_cg_str[i],
				((b >> i) & 0x1) ?
		    MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL,
				   MT_LPM_SMC_ACT_GET, rc_id, i) : 0);
	for (i = 0, b = block >> SPM_COND_BLOCKED_PLL_IDX;
	     i < PLAT_SPM_COND_PLL_MAX; i++)
		mt6885_rc_log(ToUserBuf, sz, len,
				"[%2d] %8s=%d\n",
				(i + PLAT_SPM_COND_MAX),
				mt6885_spm_cond_pll_str[i],
				((b >> i) & 0x1));
	return len;
}

static ssize_t mt6885_rc_state(int rc_id, char *ToUserBuf, size_t sz)
{
	ssize_t len = 0;

	if (rc_id < 0)
		return 0;

	mt6885_rc_log(ToUserBuf, sz, len,
		"enable=%lu, count=%lu, rc-id=%d\n",
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				MT_LPM_SMC_ACT_GET, rc_id, 0)
				& MT_SPM_RC_VALID_SW,
		MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_CNT,
				MT_LPM_SMC_ACT_GET, rc_id, 0),
		rc_id);
	return len;
}

static ssize_t mt6885_generic_rc_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	ssize_t len = 0;
	struct MT6885_RC_NODE *node = (struct MT6885_RC_NODE *)priv;

	if (!node)
		return -EINVAL;

	switch (node->type) {
	case MT6885_RC_NODE_STATE:
		mt6885_rc_log(ToUserBuf, sz, len, "count:%lu\n",
			MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_IDLE_CNT,
				    MT_LPM_SMC_ACT_GET, DBG_CTRL_COUNT, 0));
		break;
	case MT6885_RC_NODE_RC_STATE_SIMPLE:
		len += mt6885_rc_state(node->rc_id, ToUserBuf, sz);
		break;
	case MT6885_RC_NODE_RC_STATE:
		len += mt6885_rc_state(node->rc_id, ToUserBuf, sz);
		len += mt6885_lpm_rc_block_info(node->rc_id,
					ToUserBuf + len, sz - len);
		break;
	case MT6885_RC_NODE_RC_ENABLE:
		mt6885_rc_log(ToUserBuf, sz, len, "%lu\n",
			MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case MT6885_RC_NODE_COND_ENABLE:
		mt6885_rc_log(ToUserBuf, sz, len, "%lu\n",
			MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_COND_CHECK,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case MT6885_RC_NODE_COND_STATE:
		len += mt6885_lpm_rc_block_info(node->rc_id, ToUserBuf, sz);
		break;
	case MT6885_RC_NODE_RATIO_ENABLE:
		mt6885_rc_log(ToUserBuf, sz, len, "%lu\n",
			MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case MT6885_RC_NODE_RATIO_INTERVAL:
		mt6885_rc_log(ToUserBuf, sz, len, "%lu\n",
			mtk_lpm_timer_interval(&rc_ratio_timer));
		break;
	case MT6885_RC_NODE_VALID_BBLPM:
		mt6885_rc_log(ToUserBuf, sz, len, "%lu\n",
			MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_BBLPM,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case MT6885_RC_NODE_VALID_TRACE:
		mt6885_rc_log(ToUserBuf, sz, len, "%lu\n",
			MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	}

	return len;
}

static ssize_t mt6885_generic_rc_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct MT6885_RC_NODE *node = (struct MT6885_RC_NODE *)priv;

	if (!node)
		return -EINVAL;

	if ((node->type == MT6885_RC_NODE_RC_ENABLE) ||
	    (node->type == MT6885_RC_NODE_COND_ENABLE) ||
	    (node->type == MT6885_RC_NODE_VALID_BBLPM) ||
	    (node->type == MT6885_RC_NODE_VALID_TRACE)) {
		unsigned int parm;
		int cmd;

		if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
			cmd = (node->type == MT6885_RC_NODE_RC_ENABLE) ?
				MT_SPM_DBG_SMC_UID_RC_SWITCH :
				(node->type == MT6885_RC_NODE_COND_ENABLE) ?
				MT_SPM_DBG_SMC_UID_COND_CHECK :
				(node->type == MT6885_RC_NODE_VALID_BBLPM) ?
				MT_SPM_DBG_SMC_UID_RC_BBLPM :
				(node->type == MT6885_RC_NODE_VALID_TRACE) ?
				MT_SPM_DBG_SMC_UID_RC_TRACE : -1;

			if (cmd < 0)
				return -EINVAL;

			if (!!parm)
				parm = MT_LPM_SMC_ACT_SET;
			else
				parm = MT_LPM_SMC_ACT_CLR;
			MT6885_DBG_SMC(cmd, parm, node->rc_id, 0);
		}
	} else if ((node->type == MT6885_RC_NODE_COND_SET)
		|| (node->type == MT6885_RC_NODE_COND_CLR)) {
		unsigned int parm1, parm2, act;

		if (sscanf(FromUserBuf, "%u %x", &parm1, &parm2) == 2) {
			act = (node->type == MT6885_RC_NODE_COND_SET) ?
				MT_LPM_SMC_ACT_SET :
				(node->type == MT6885_RC_NODE_COND_CLR) ?
				MT_LPM_SMC_ACT_CLR : 0;

			if (act != 0)
				mt6885_lpm_rc_cond_ctrl(node->rc_id, act,
							parm1, parm2);
		}
	} else if ((node->type == MT6885_RC_NODE_RATIO_ENABLE)
		|| (node->type == MT6885_RC_NODE_RATIO_INTERVAL)) {
		unsigned int parm;

		if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
			if (node->type == MT6885_RC_NODE_RATIO_ENABLE) {
				parm = (!!parm) ? MT_LPM_SMC_ACT_SET :
						MT_LPM_SMC_ACT_CLR;
				MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
					parm, MT_RM_CONSTRAINT_ID_DRAM, 0);
				MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
					parm, MT_RM_CONSTRAINT_ID_SYSPLL, 0);
				MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
					parm, MT_RM_CONSTRAINT_ID_BUS26M, 0);

				if (parm == MT_LPM_SMC_ACT_SET)
					mtk_lpm_timer_start(&rc_ratio_timer);
				else
					mtk_lpm_timer_stop(&rc_ratio_timer);
			} else if (node->type == MT6885_RC_NODE_RATIO_INTERVAL)
				mtk_lpm_timer_interval_update(&rc_ratio_timer,
						(unsigned long long)parm);
		}
	}
	return sz;
}

static int mt6885_lpm_rc_node_add(struct MT6885_RC_NODE *n,
				int mode, struct MT6885_RC_ENTERY *p)
{
	return mtk_lpm_sysfs_sub_entry_node_add(n->name, mode,
					&n->op, &p->handle, &n->handle);
}

static int mt6885_lpm_rc_entry_add(struct MT6885_RC_ENTERY *n,
					int mode, struct MT6885_RC_ENTERY *p)
{
	return mtk_lpm_sysfs_sub_entry_add(n->name, mode,
					   &p->handle, &n->handle);
}

static int mt6885_lpm_rc_valid_node_add(int rc_id,
					struct MT6885_RC_ENTERY *parent,
					struct MT6885_RC_VALID_HANDLES *valid)
{
	int bRet = 0;

	if (!valid || !parent)
		return -EINVAL;

	valid->root.name = "valid";
	bRet = mt6885_lpm_rc_entry_add(&valid->root, 0644, parent);

	if (!bRet) {
		MT6885_GENERIC_RC_NODE_INIT(valid->hBblpm, "bblpm",
				    rc_id, MT6885_RC_NODE_VALID_BBLPM);
		mt6885_lpm_rc_node_add(&valid->hBblpm, 0200, &valid->root);
		MT6885_GENERIC_RC_NODE_INIT(valid->hTrace, "trace",
				    rc_id, MT6885_RC_NODE_VALID_TRACE);
		mt6885_lpm_rc_node_add(&valid->hTrace, 0200, &valid->root);
	}
	return bRet;
}

static int mt6885_lpm_rc_ratio_entry(struct mtk_lp_sysfs_handle *parent,
					struct MT6885_RC_RATIO_HANDLES *ratio)
{
	int bRet = 0;

	if (!ratio || !parent)
		return -EINVAL;

	ratio->root.name = "ratio";
	bRet = mtk_lpm_sysfs_sub_entry_add(ratio->root.name, 0644,
					parent, &ratio->root.handle);
	if (!bRet) {
		MT6885_GENERIC_RC_NODE_INIT(ratio->hEnable, "enable",
				    0, MT6885_RC_NODE_RATIO_ENABLE);
		mt6885_lpm_rc_node_add(&ratio->hEnable, 0200, &ratio->root);
		MT6885_GENERIC_RC_NODE_INIT(ratio->hInterval, "interval",
				    0, MT6885_RC_NODE_RATIO_INTERVAL);
		mt6885_lpm_rc_node_add(&ratio->hInterval, 0200, &ratio->root);
	}
	return 0;
}

static int mt6885_lpm_rc_cond_node_add(int rc_id,
					struct MT6885_RC_ENTERY *parent,
					struct MT6885_RC_COND_HANDLES *cond)
{
	int bRet = 0;

	if (!cond || !parent)
		return -EINVAL;

	cond->root.name = "cond";
	bRet = mt6885_lpm_rc_entry_add(&cond->root, 0644, parent);

	if (!bRet) {
		MT6885_GENERIC_RC_NODE_INIT(cond->hSet, "set",
				    rc_id, MT6885_RC_NODE_COND_SET);
		mt6885_lpm_rc_node_add(&cond->hSet, 0200, &cond->root);

		MT6885_GENERIC_RC_NODE_INIT(cond->hClr, "clr",
				    rc_id, MT6885_RC_NODE_COND_CLR);
		mt6885_lpm_rc_node_add(&cond->hClr, 0200, &cond->root);

		MT6885_GENERIC_RC_NODE_INIT(cond->hEnable, "enable",
				    rc_id, MT6885_RC_NODE_COND_ENABLE);
		mt6885_lpm_rc_node_add(&cond->hEnable, 0644, &cond->root);

		MT6885_GENERIC_RC_NODE_INIT(cond->hState, "state",
				    rc_id, MT6885_RC_NODE_COND_STATE);
		mt6885_lpm_rc_node_add(&cond->hState, 0444, &cond->root);
	}
	return bRet;
}

static int mt6885_lpm_rc_entry_nodes_basic(int IsSimple,
					const char *name, int rc_id,
					struct mtk_lp_sysfs_handle *parent,
					struct MT6885_RC_HANDLE_BASIC *rc)
{
	int bRet = 0;

	if (!parent || !rc)
		return -EINVAL;

	rc->root.name = name;
	bRet = mtk_lpm_sysfs_sub_entry_add(rc->root.name, 0644,
					parent, &rc->root.handle);
	if (bRet)
		return -EINVAL;

	MT6885_GENERIC_RC_NODE_INIT(rc->hState, "state", rc_id,
				    (IsSimple) ? MT6885_RC_NODE_RC_STATE_SIMPLE
						: MT6885_RC_NODE_RC_STATE);
	mt6885_lpm_rc_node_add(&rc->hState, 0444, &rc->root);

	MT6885_GENERIC_RC_NODE_INIT(rc->hEnable, "enable", rc_id,
				    MT6885_RC_NODE_RC_ENABLE);
	mt6885_lpm_rc_node_add(&rc->hEnable, 0644, &rc->root);

	return bRet;
}

int mt6885_lpm_rc_entry_nodes(const char *name, int rc_id,
				    struct mtk_lp_sysfs_handle *parent,
				    struct MT6885_RC_HANDLE *rc)
{
	int bRet = 0;

	if (!parent || !rc)
		return -EINVAL;

	memset(rc, 0, sizeof(struct MT6885_RC_HANDLE));

	bRet = mt6885_lpm_rc_entry_nodes_basic(0, name, rc_id,
						parent, &rc->basic);

	bRet = mt6885_lpm_rc_cond_node_add(rc_id, &rc->basic.root,
					   &rc->hCond);

	bRet = mt6885_lpm_rc_valid_node_add(rc_id, &rc->basic.root,
					   &rc->valid);
	return 0;
}

int mt6885_dbg_lpm_fs_init(void)
{
	/* enable resource constraint condition block latch */
	mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_LATCH,
				    MT_LPM_SMC_ACT_SET, 0, 0);
	mtk_lpm_sysfs_root_entry_create();

	mtk_lpm_sysfs_sub_entry_add("rc", 0644, NULL,
				    &mt6885_entry_rc);

	MT6885_GENERIC_RC_NODE_INIT(rc_state, "state",
				    0, MT6885_RC_NODE_STATE);
	mtk_lpm_sysfs_sub_entry_node_add(rc_state.name, 0444,
					&rc_state.op, &mt6885_entry_rc,
					&rc_state.handle);

	mt6885_lpm_rc_ratio_entry(&mt6885_entry_rc, &rc_Ratio);

	mt6885_lpm_rc_entry_nodes_basic(1, "cpu-buck-ldo",
				MT_RM_CONSTRAINT_ID_CPU_BUCK_LDO,
				&mt6885_entry_rc, &rc_cpu_buckldo);
	mt6885_lpm_rc_entry_nodes("dram", MT_RM_CONSTRAINT_ID_DRAM,
				  &mt6885_entry_rc, &rc_dram);
	mt6885_lpm_rc_entry_nodes("syspll", MT_RM_CONSTRAINT_ID_SYSPLL,
				  &mt6885_entry_rc, &rc_syspll);
	mt6885_lpm_rc_entry_nodes("bus26m", MT_RM_CONSTRAINT_ID_BUS26M,
				  &mt6885_entry_rc, &rc_bus26m);

	rc_ratio_timer.timeout = mt6885_lpm_rc_ratio_timer_func;
	mtk_lpm_timer_init(&rc_ratio_timer, MTK_LPM_TIMER_REPEAT);
	mtk_lpm_timer_interval_update(&rc_ratio_timer,
				MT6885_LPM_RC_RATIO_DEFAULT);

	/* enable constraint tracing */
	MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_SET, MT_RM_CONSTRAINT_ID_BUS26M, 0);
	MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_SET, MT_RM_CONSTRAINT_ID_SYSPLL, 0);
	MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_SET, MT_RM_CONSTRAINT_ID_DRAM, 0);
	return 0;
}

int mt6885_dbg_lpm_fs_deinit(void)
{
	/* disable resource contraint condition block latch */
	mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_LATCH,
			    MT_LPM_SMC_ACT_CLR, 0, 0);

	/* disable constraint tracing */
	MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_CLR, MT_RM_CONSTRAINT_ID_BUS26M, 0);
	MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_CLR, MT_RM_CONSTRAINT_ID_SYSPLL, 0);
	MT6885_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_CLR, MT_RM_CONSTRAINT_ID_DRAM, 0);
	return 0;
}
