// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>
#include <lpm_resource_constraint_v1.h>

#include <lpm_dbg_fs_common.h>

#include <lpm.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_lp_sysfs.h>
#include <lpm_timer.h>

#define SPM_RC_UPDATE_COND_ID_MASK	0xffff
#define SPM_RC_UPDATE_COND_RC_ID_MASK	0xffff
#define SPM_RC_UPDATE_COND_RC_ID_SHIFT	(16)

#define SPM_RC_UPDATE_COND_CTRL_ID(rc, cond)\
	(((rc & SPM_RC_UPDATE_COND_RC_ID_MASK)\
		<< SPM_RC_UPDATE_COND_RC_ID_SHIFT)\
	| (cond & SPM_RC_UPDATE_COND_ID_MASK))


#define LPM_RC_RATIO_DEFAULT		10000
#define lpm_rc_log(buf, sz, len, fmt, args...) ({\
	if (len < sz)\
		len += scnprintf(buf + len, sz - len,\
				 fmt, ##args); })

#define LPM_DBG_SMC(_id, _act, _rc, _param) ({\
	(unsigned long)lpm_smc_spm_dbg(_id, _act, _rc, _param); })


enum LPM_RC_RATIO_TYPE {
	LPM_RC_RATIO_UNKNOWN,
	LPM_RC_RATIO_ENABLE,
	LPM_RC_RATIO_DISABLE,
	LPM_RC_RATIO_ENABLE_INFO,
	LPM_RC_RATIO_INFO,
};

enum LPM_RC_NODE_TYPE {
	LPM_RC_NODE_STATE,
	LPM_RC_NODE_RC_ENABLE,
	LPM_RC_NODE_RC_STATE_SIMPLE,
	LPM_RC_NODE_RC_STATE,
	LPM_RC_NODE_COND_ENABLE,
	LPM_RC_NODE_COND_STATE,
	LPM_RC_NODE_COND_SET,
	LPM_RC_NODE_COND_CLR,
	LPM_RC_NODE_RATIO_ENABLE,
	LPM_RC_NODE_RATIO_INTERVAL,
	LPM_RC_NODE_VALID_BBLPM,
	LPM_RC_NODE_VALID_TRACE,
	LPM_RC_NODE_MAX
};

struct LPM_RC_NODE {
	const char *name;
	int rc_id;
	int type;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

struct LPM_RC_ENTERY {
	const char *name;
	struct mtk_lp_sysfs_handle handle;
};

struct LPM_RC_COND_HANDLES {
	struct LPM_RC_ENTERY root;
	struct LPM_RC_NODE hSet;
	struct LPM_RC_NODE hClr;
	struct LPM_RC_NODE hState;
	struct LPM_RC_NODE hEnable;
};

struct LPM_RC_VALID_HANDLES {
	struct LPM_RC_ENTERY root;
	struct LPM_RC_NODE hBblpm;
	struct LPM_RC_NODE hTrace;
};

struct LPM_RC_RATIO_HANDLES {
	struct LPM_RC_ENTERY root;
	struct LPM_RC_NODE hInterval;
	struct LPM_RC_NODE hEnable;
};

struct LPM_RC_HANDLE_BASIC {
	struct LPM_RC_ENTERY root;
	struct LPM_RC_NODE hEnable;
	struct LPM_RC_NODE hState;
};

struct LPM_RC_HANDLE {
	struct LPM_RC_HANDLE_BASIC basic;
	struct LPM_RC_COND_HANDLES hCond;
	struct LPM_RC_VALID_HANDLES valid;
};


#define LPM_CONSTRAINT_GENERIC_OP(op, _priv) ({\
	op.fs_read = lpm_generic_rc_read;\
	op.fs_write = lpm_generic_rc_write;\
	op.priv = _priv; })


#define LPM_GENERIC_RC_NODE_INIT(_n, _name, _id, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	_n.rc_id = _id;\
	LPM_CONSTRAINT_GENERIC_OP(_n.op, &_n); })


struct mtk_lp_sysfs_handle lpm_entry_rc;
struct LPM_RC_RATIO_HANDLES rc_Ratio;
struct LPM_RC_NODE rc_state;

struct lpm_timer rc_ratio_timer;

struct rc_trace_struct {
	char **rc_name;
	unsigned long bitmap;
};
static struct rc_trace_struct rc_trace;
struct spm_condition spm_cond;
int lpm_rc_cond_ctrl(int rc_id, unsigned int act,
			unsigned int cond_id, unsigned int value)
{
	unsigned int cond_ctrl_id;
	int res = 0;

	cond_ctrl_id = SPM_RC_UPDATE_COND_CTRL_ID(rc_id, cond_id);

	if (cond_id < spm_cond.cg_cnt)
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_COND_CTRL,
				    act, cond_ctrl_id, value);
	else if ((cond_id - spm_cond.cg_cnt) < spm_cond.pll_cnt)
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_COND_CTRL,
				    act, cond_ctrl_id, !!value);
	else
		pr_info("[%s:%d] - unknown cond id = %u\n",
			__func__, __LINE__, cond_id);

	return res;
}

int lpm_rc_ratio_timer_func(unsigned long long dur, void *priv)
{
	/* spm tick to ms: tick/32 (ms) */
	#define RC_RESIDENCY_COVERT(x)	(x>>5)
	#define RC_COVERT_RATIO(dur, x)\
		((RC_RESIDENCY_COVERT(x) * 100)/dur)
	#define LOG_BUF_OUT_SZ		256

	unsigned long long rc_state;

	int rc_id = 0, idx = 0;
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	int log_size = 0;

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"[name:spm&][RC] ratio, duration_ms:%llu",
		dur);

	for_each_set_bit(rc_id, &rc_trace.bitmap, 32) {
		rc_state = (unsigned long long)
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_INFO,
				(MT_LPM_SMC_ACT_GET | MT_LPM_SMC_ACT_CLR),
				rc_id, 0);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			", %s:%llu%%", rc_trace.rc_name[idx++],
			RC_COVERT_RATIO(dur, rc_state));
	}

	pr_info("%s", log_buf);

	return 0;
}

static ssize_t lpm_rc_block_info(int rc_id,
					      char *ToUserBuf,
					      size_t sz)
{
	uint32_t block, b;
	int i;
	ssize_t len = 0;

	block = (uint32_t)
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_COND_BLOCK,
				MT_LPM_SMC_ACT_GET, rc_id, 0);
	b = (uint32_t)
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_COND_CHECK,
				MT_LPM_SMC_ACT_GET, rc_id, 0);

	lpm_rc_log(ToUserBuf, sz, len,
			"blocked=%u, blocked_cond=0x%08x\n",
			b, block);

	if (spm_cond.shift_config != 1) {
		lpm_rc_log(ToUserBuf, sz, len,
			"cg-shift & pll-shift not configured in dts\n");
		return len;
	}

	for (i = 0, b = block >> spm_cond.cg_shift;
	     i < spm_cond.cg_cnt; i++)
		lpm_rc_log(ToUserBuf, sz, len,
				"[%2d] %8s=0x%08lx\n", i,
				spm_cond.cg_str[i],
				((b >> i) & 0x1) ?
		    LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL,
				   MT_LPM_SMC_ACT_GET, rc_id, i) : 0);
	for (i = 0, b = block >> spm_cond.pll_shift;
	     i < spm_cond.pll_cnt; i++)
		lpm_rc_log(ToUserBuf, sz, len,
				"[%2d] %8s=%d\n",
				(i + spm_cond.cg_cnt),
				spm_cond.pll_str[i],
				((b >> i) & 0x1));
	return len;
}

static ssize_t lpm_rc_state(int rc_id, char *ToUserBuf, size_t sz)
{
	ssize_t len = 0;

	if (rc_id < 0)
		return 0;

	lpm_rc_log(ToUserBuf, sz, len,
		"enable=%lu, count=%lu, rc-id=%d\n",
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				MT_LPM_SMC_ACT_GET, rc_id, 0)
				& MT_SPM_RC_VALID_SW,
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_CNT,
				MT_LPM_SMC_ACT_GET, rc_id, 0),
		rc_id);
	return len;
}

static ssize_t lpm_generic_rc_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	ssize_t len = 0;
	struct LPM_RC_NODE *node = (struct LPM_RC_NODE *)priv;

	if (!node)
		return -EINVAL;

	switch (node->type) {
	case LPM_RC_NODE_STATE:
		lpm_rc_log(ToUserBuf, sz, len, "count:%lu\n",
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_IDLE_CNT,
				    MT_LPM_SMC_ACT_GET, DBG_CTRL_COUNT, 0));
		break;
	case LPM_RC_NODE_RC_STATE_SIMPLE:
		len += lpm_rc_state(node->rc_id, ToUserBuf, sz);
		break;
	case LPM_RC_NODE_RC_STATE:
		len += lpm_rc_state(node->rc_id, ToUserBuf, sz);
		len += lpm_rc_block_info(node->rc_id,
					ToUserBuf + len, sz - len);
		break;
	case LPM_RC_NODE_RC_ENABLE:
		lpm_rc_log(ToUserBuf, sz, len, "%lu\n",
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case LPM_RC_NODE_COND_ENABLE:
		lpm_rc_log(ToUserBuf, sz, len, "%lu\n",
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_COND_CHECK,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case LPM_RC_NODE_COND_STATE:
		len += lpm_rc_block_info(node->rc_id, ToUserBuf, sz);
		break;
	case LPM_RC_NODE_RATIO_ENABLE:
		lpm_rc_log(ToUserBuf, sz, len, "%lu\n",
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case LPM_RC_NODE_RATIO_INTERVAL:
		lpm_rc_log(ToUserBuf, sz, len, "%lu\n",
			lpm_timer_interval(&rc_ratio_timer));
		break;

	case LPM_RC_NODE_VALID_BBLPM:
		lpm_rc_log(ToUserBuf, sz, len, "%lu\n",
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_BBLPM,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	case LPM_RC_NODE_VALID_TRACE:
		lpm_rc_log(ToUserBuf, sz, len, "%lu\n",
			LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
				    MT_LPM_SMC_ACT_GET, node->rc_id, 0));
		break;
	}

	return len;
}

static ssize_t lpm_generic_rc_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct LPM_RC_NODE *node = (struct LPM_RC_NODE *)priv;

	if (!node)
		return -EINVAL;

	if ((node->type == LPM_RC_NODE_RC_ENABLE) ||
		(node->type == LPM_RC_NODE_COND_ENABLE) ||
		(node->type == LPM_RC_NODE_VALID_BBLPM) ||
		(node->type == LPM_RC_NODE_VALID_TRACE)) {
		unsigned int parm;
		int cmd;

		if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
			cmd = (node->type == LPM_RC_NODE_RC_ENABLE) ?
				MT_SPM_DBG_SMC_UID_RC_SWITCH :
				(node->type == LPM_RC_NODE_COND_ENABLE) ?
				MT_SPM_DBG_SMC_UID_COND_CHECK :
				(node->type == LPM_RC_NODE_VALID_BBLPM) ?
				MT_SPM_DBG_SMC_UID_RC_BBLPM :
				(node->type == LPM_RC_NODE_VALID_TRACE) ?
				MT_SPM_DBG_SMC_UID_RC_TRACE : -1;

			if (cmd < 0)
				return -EINVAL;

			if (!!parm)
				parm = MT_LPM_SMC_ACT_SET;
			else
				parm = MT_LPM_SMC_ACT_CLR;
			LPM_DBG_SMC(cmd, parm, node->rc_id, 0);
		}
	} else if ((node->type == LPM_RC_NODE_COND_SET)
		|| (node->type == LPM_RC_NODE_COND_CLR)) {
		unsigned int parm1, parm2, act;

		if (sscanf(FromUserBuf, "%u %x", &parm1, &parm2) == 2) {
			act = (node->type == LPM_RC_NODE_COND_SET) ?
				MT_LPM_SMC_ACT_SET :
				(node->type == LPM_RC_NODE_COND_CLR) ?
				MT_LPM_SMC_ACT_CLR : 0;

			if (act != 0)
				lpm_rc_cond_ctrl(node->rc_id, act,
							parm1, parm2);
		}
	} else if ((node->type == LPM_RC_NODE_RATIO_ENABLE)
		|| (node->type == LPM_RC_NODE_RATIO_INTERVAL)) {
		unsigned int parm;

		if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
			if (node->type == LPM_RC_NODE_RATIO_ENABLE) {
				parm = (!!parm) ? MT_LPM_SMC_ACT_SET :
						MT_LPM_SMC_ACT_CLR;
				LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
					parm, MT_RM_CONSTRAINT_ID_DRAM, 0);
				LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
					parm, MT_RM_CONSTRAINT_ID_SYSPLL, 0);
				LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_RES_CTRL,
					parm, MT_RM_CONSTRAINT_ID_BUS26M, 0);

				if (parm == MT_LPM_SMC_ACT_SET)
					lpm_timer_start(&rc_ratio_timer);
				else
					lpm_timer_stop(&rc_ratio_timer);
			} else if (node->type == LPM_RC_NODE_RATIO_INTERVAL)
				lpm_timer_interval_update(&rc_ratio_timer,
						(unsigned long long)parm);
		}
	}
	return sz;
}

static int lpm_rc_node_add(struct LPM_RC_NODE *n,
				int mode, struct LPM_RC_ENTERY *p)
{
	return mtk_lpm_sysfs_sub_entry_node_add(n->name, mode,
					&n->op, &p->handle, &n->handle);
}

static int lpm_rc_entry_add(struct LPM_RC_ENTERY *n,
					int mode, struct LPM_RC_ENTERY *p)
{
	return mtk_lpm_sysfs_sub_entry_add(n->name, mode,
					   &p->handle, &n->handle);
}

static int lpm_rc_valid_node_add(int rc_id,
					struct LPM_RC_ENTERY *parent,
					struct LPM_RC_VALID_HANDLES *valid)
{
	int bRet = 0;

	if (!valid || !parent)
		return -EINVAL;

	valid->root.name = "valid";
	bRet = lpm_rc_entry_add(&valid->root, 0644, parent);

	if (!bRet) {
		LPM_GENERIC_RC_NODE_INIT(valid->hBblpm, "bblpm",
				    rc_id, LPM_RC_NODE_VALID_BBLPM);
		lpm_rc_node_add(&valid->hBblpm, 0200, &valid->root);
		LPM_GENERIC_RC_NODE_INIT(valid->hTrace, "trace",
				    rc_id, LPM_RC_NODE_VALID_TRACE);
		lpm_rc_node_add(&valid->hTrace, 0200, &valid->root);
	}
	return bRet;
}

static int lpm_rc_ratio_entry(struct mtk_lp_sysfs_handle *parent,
					struct LPM_RC_RATIO_HANDLES *ratio)
{
	int bRet = 0;

	if (!ratio || !parent)
		return -EINVAL;

	ratio->root.name = "ratio";
	bRet = mtk_lpm_sysfs_sub_entry_add(ratio->root.name, 0644,
					parent, &ratio->root.handle);
	if (!bRet) {
		LPM_GENERIC_RC_NODE_INIT(ratio->hEnable, "enable",
				    0, LPM_RC_NODE_RATIO_ENABLE);
		lpm_rc_node_add(&ratio->hEnable, 0200, &ratio->root);
		LPM_GENERIC_RC_NODE_INIT(ratio->hInterval, "interval",
				    0, LPM_RC_NODE_RATIO_INTERVAL);
		lpm_rc_node_add(&ratio->hInterval, 0200, &ratio->root);
	}
	return 0;
}

static int lpm_rc_cond_node_add(int rc_id,
					struct LPM_RC_ENTERY *parent,
					struct LPM_RC_COND_HANDLES *cond)
{
	int bRet = 0;

	if (!cond || !parent)
		return -EINVAL;

	cond->root.name = "cond";
	bRet = lpm_rc_entry_add(&cond->root, 0644, parent);

	if (!bRet) {
		LPM_GENERIC_RC_NODE_INIT(cond->hSet, "set",
				    rc_id, LPM_RC_NODE_COND_SET);
		lpm_rc_node_add(&cond->hSet, 0200, &cond->root);

		LPM_GENERIC_RC_NODE_INIT(cond->hClr, "clr",
				    rc_id, LPM_RC_NODE_COND_CLR);
		lpm_rc_node_add(&cond->hClr, 0200, &cond->root);

		LPM_GENERIC_RC_NODE_INIT(cond->hEnable, "enable",
				    rc_id, LPM_RC_NODE_COND_ENABLE);
		lpm_rc_node_add(&cond->hEnable, 0644, &cond->root);

		LPM_GENERIC_RC_NODE_INIT(cond->hState, "state",
				    rc_id, LPM_RC_NODE_COND_STATE);
		lpm_rc_node_add(&cond->hState, 0444, &cond->root);
	}
	return bRet;
}


static int lpm_rc_entry_nodes_basic(int IsSimple,
					const char *name, int rc_id,
					struct mtk_lp_sysfs_handle *parent,
					struct LPM_RC_HANDLE_BASIC *rc)
{
	int bRet = 0;

	if (!parent || !rc)
		return -EINVAL;

	rc->root.name = name;
	bRet = mtk_lpm_sysfs_sub_entry_add(rc->root.name, 0644,
					parent, &rc->root.handle);
	if (bRet)
		return -EINVAL;

	LPM_GENERIC_RC_NODE_INIT(rc->hState, "state", rc_id,
				    (IsSimple) ? LPM_RC_NODE_RC_STATE_SIMPLE
						: LPM_RC_NODE_RC_STATE);
	lpm_rc_node_add(&rc->hState, 0444, &rc->root);

	LPM_GENERIC_RC_NODE_INIT(rc->hEnable, "enable", rc_id,
				    LPM_RC_NODE_RC_ENABLE);
	lpm_rc_node_add(&rc->hEnable, 0644, &rc->root);

	return bRet;
}

struct LPM_RC_HANDLE rc_handle[5];

int spm_cond_init(void)
{
	struct device_node *devnp = NULL;
	struct device_node *np = NULL;
	int idx = 0, constraint_cnt = 0;
	const char *rc_name = NULL;
	u32 rc_id = 0, cond_info = 0, ret = 0;
	struct property *prop;

	const char *cg_name = NULL, *pll_name = NULL;
	int cg_idx = 0, pll_idx = 0, cg_cnt = 0, pll_cnt = 0;

	if (spm_cond.init)
		return 0;

	devnp = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	if (devnp) {
		constraint_cnt = of_count_phandle_with_args(
					devnp, "constraints",
					NULL);
		rc_trace.rc_name = kcalloc(1, sizeof(char *)*constraint_cnt,
						GFP_KERNEL);

		if (!rc_trace.rc_name)
			return -ENOMEM;

		while ((np = of_parse_phandle(devnp, "constraints", idx))) {
			cond_info = 0;
			ret = of_property_read_string(np, "rc-name", &rc_name);
			if (ret == 0)
				ret = of_property_read_u32(np, "id", &rc_id);
			if (ret == 0)
				ret = of_property_read_u32(np, "cond-info",
						&cond_info);
			of_node_put(np);

			if (ret != 0 || rc_id == -1 || rc_name == NULL) {
				pr_info("[%s:%d] - abnormal rc\n",
					__func__, __LINE__);
				idx++;
				continue;
			}

			if (!!cond_info) {
				lpm_rc_entry_nodes_basic(0, rc_name,
						rc_id, &lpm_entry_rc,
						&rc_handle[idx].basic);

				lpm_rc_cond_node_add(rc_id,
						&rc_handle[idx].basic.root,
						&rc_handle[idx].hCond);

				lpm_rc_valid_node_add(rc_id,
						&rc_handle[idx].basic.root,
						&rc_handle[idx].valid);

				rc_trace.bitmap |= (1 << rc_id);

				rc_trace.rc_name[idx] = kcalloc(1,
					sizeof(char)*(strlen(rc_name) + 1),
					GFP_KERNEL);

				if (!rc_trace.rc_name[idx]) {
					kfree(rc_trace.rc_name);
					return -ENOMEM;
				}

				strncat(rc_trace.rc_name[idx++], rc_name,
					strlen(rc_name));
			} else {
				lpm_rc_entry_nodes_basic(1, rc_name,
						rc_id, &lpm_entry_rc,
						&rc_handle[idx++].basic);
			}
		}

		idx = 0;
		while ((np = of_parse_phandle(devnp, "spm-cond", idx))) {
			idx++;
			cg_cnt = of_property_count_strings(np, "cg-name");
			if (cg_cnt >= 1) {
				spm_cond.cg_cnt = cg_cnt;
				spm_cond.cg_str =
				kcalloc(1, sizeof(char *)*(spm_cond.cg_cnt),
					GFP_KERNEL);

				if (!spm_cond.cg_str)
					return -ENOMEM;

				of_property_for_each_string(np, "cg-name",
						prop, cg_name) {

					spm_cond.cg_str[cg_idx] =
					kcalloc(1, sizeof(char)*
						(strlen(cg_name) + 1),
						GFP_KERNEL);

					if (!spm_cond.cg_str[cg_idx]) {
						kfree(spm_cond.cg_str);
						return -ENOMEM;
					}

					strncat(spm_cond.cg_str[cg_idx++],
						cg_name, strlen(cg_name));
				}
			}

			pll_cnt = of_property_count_strings(np, "pll-name");
			if (pll_cnt >= 1) {
				spm_cond.pll_cnt = pll_cnt;
				spm_cond.pll_str =
				kcalloc(1, sizeof(char *)*(spm_cond.pll_cnt),
						GFP_KERNEL);

				if (!spm_cond.pll_str)
					return -ENOMEM;

				of_property_for_each_string(np, "pll-name",
					prop, pll_name) {
					spm_cond.pll_str[pll_idx] =
					kcalloc(1,
						sizeof(char) *
						(strlen(pll_name) + 1),
						GFP_KERNEL);

					if (!spm_cond.pll_str[pll_idx]) {
						kfree(spm_cond.pll_str);
						return -ENOMEM;
					}

					strncat(spm_cond.pll_str[pll_idx++],
						pll_name, strlen(pll_name));
				}
			}
			of_node_put(np);
		}

		ret = of_property_read_u32(devnp, "cg-shift",
					&spm_cond.cg_shift);
		if (ret == 0)
			ret = of_property_read_u32(devnp, "pll-shift",
					&spm_cond.pll_shift);
		if (ret == 0)
			spm_cond.shift_config = 1;

		of_node_put(devnp);
	}
	spm_cond.init = 1;

	return 0;
}
EXPORT_SYMBOL(spm_cond_init);

void spm_cond_deinit(void)
{
	struct device_node *devnp = NULL;
	int idx = 0, constraint_cnt = 0;

	if (!spm_cond.init)
		return;

	spm_cond.init = 0;

	devnp = of_find_compatible_node(NULL, NULL,
					MTK_LPM_DTS_COMPATIBLE);

	/* free allocated memory */
	if (devnp) {
		constraint_cnt = of_count_phandle_with_args(
						devnp, "constraints",
						NULL);
		for (idx = 0; idx < constraint_cnt; idx++)
			kfree(rc_trace.rc_name[idx]);
		kfree(rc_trace.rc_name);

		if (spm_cond.cg_cnt >= 1) {
			for (idx = 0; idx < spm_cond.cg_cnt; idx++)
				kfree(spm_cond.cg_str[idx]);
			kfree(spm_cond.cg_str);
		}

		if (spm_cond.pll_cnt >= 1) {
			for (idx = 0; idx < spm_cond.pll_cnt; idx++)
				kfree(spm_cond.pll_str[idx]);
			kfree(spm_cond.pll_str);
		}

		of_node_put(devnp);
	}
}
EXPORT_SYMBOL(spm_cond_deinit);

int lpm_rc_fs_init(void)
{
	int idx = 0, ret = 0;

	/* enable resource constraint condition block latch */
	lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_LATCH,
				    MT_LPM_SMC_ACT_SET, 0, 0);

	mtk_lpm_sysfs_root_entry_create();

	mtk_lpm_sysfs_sub_entry_add("rc", 0644, NULL,
				    &lpm_entry_rc);

	LPM_GENERIC_RC_NODE_INIT(rc_state, "state",
				    0, LPM_RC_NODE_STATE);
	mtk_lpm_sysfs_sub_entry_node_add(rc_state.name, 0444,
					&rc_state.op, &lpm_entry_rc,
					&rc_state.handle);

	lpm_rc_ratio_entry(&lpm_entry_rc, &rc_Ratio);

	ret = spm_cond_init();
	if (ret)
		pr_info("[%s:%d] - spm_cond_init failed\n",
			__func__, __LINE__);

	rc_ratio_timer.timeout = lpm_rc_ratio_timer_func;
	lpm_timer_init(&rc_ratio_timer, LPM_TIMER_REPEAT);
	lpm_timer_interval_update(&rc_ratio_timer,
					LPM_RC_RATIO_DEFAULT);

	/* enable constraint tracing */
	for_each_set_bit(idx, &rc_trace.bitmap, 32) {
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
				MT_LPM_SMC_ACT_SET, idx, 0);
	}

	return 0;
}
EXPORT_SYMBOL(lpm_rc_fs_init);

int lpm_rc_fs_deinit(void)
{
	int idx = 0;

	/* disable resource contraint condition block latch */
	lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_LATCH,
			    MT_LPM_SMC_ACT_CLR, 0, 0);

	/* disable constraint tracing */
	for_each_set_bit(idx, &rc_trace.bitmap, 32) {
		LPM_DBG_SMC(MT_SPM_DBG_SMC_UID_RC_TRACE,
			MT_LPM_SMC_ACT_CLR, idx, 0);
	}

	spm_cond_deinit();
	return 0;

}
EXPORT_SYMBOL(lpm_rc_fs_deinit);
