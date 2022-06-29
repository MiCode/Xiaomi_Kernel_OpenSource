// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>
#include <lpm_resource_constraint_v1.h>

#include <lpm_dbg_fs_common.h>

#include <lpm.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_lp_sysfs.h>
#include <lpm_timer.h>

#define HWCG_RES_NAME		(8)
#define LPM_HWREQ_NAME	"hwreq"
#define LPM_HWCG_NAME	"hw_cg"

#define lpm_hwreq_log(buf, sz, len, fmt, args...) ({\
	if (len < sz)\
		len += scnprintf(buf + len, sz - len,\
				 fmt, ##args); })


enum LPM_HWREQ_NODE_TYPE {
	LPM_HWREQ_NODE_HWCG_NODE,
	LPM_HWREQ_NODE_HWCG_STA,
	LPM_HWREQ_NODE_HWCG_SET,
	LPM_HWREQ_NODE_HWCG_CLR,
	LPM_HWREQ_NODE_MAX
};


#define LPM_HWREQ_GENERIC_OP(op, _priv) ({\
	op.fs_read = lpm_generic_hwreq_read;\
	op.fs_write = lpm_generic_hwreq_write;\
	op.priv = _priv; })


#define LPM_GENERIC_HWREQ_NODE_INIT(_n, _id, _type, _parent) ({\
	_n.type = _type;\
	_n.hwreq_id = _id;\
	_n.parent = _parent;\
	LPM_HWREQ_GENERIC_OP(_n.op, &_n); })


struct LPM_HWREQ_NODE {
	int hwreq_id;
	int type;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
	void *parent;
};

struct LPM_HWCG_HANDLES {
	unsigned int setting_num;
	struct mtk_lp_sysfs_handle handle;
	struct LPM_HWREQ_NODE hStatus;
	struct LPM_HWREQ_NODE hSet;
	struct LPM_HWREQ_NODE hClr;
	struct list_head list;
};

static struct mtk_lp_sysfs_handle lpm_entry_hwreq;
static struct mtk_lp_sysfs_handle *lpm_hwcg;
static LIST_HEAD(lpm_hwcgs);

static ssize_t lpm_generic_hwreq_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	ssize_t len = 0;
	struct LPM_HWREQ_NODE *node = (struct LPM_HWREQ_NODE *)priv;

	if (!node)
		return -EINVAL;

	switch (node->type) {
	case LPM_HWREQ_NODE_HWCG_STA:
		if (node->parent) {
			struct LPM_HWCG_HANDLES *p =
				(struct LPM_HWCG_HANDLES *)node->parent;

			if (p->setting_num > 0) {
				int i = 0;

				lpm_hwreq_log(ToUserBuf, sz, len,
						"Index  Block      Status     Setting/Default\n");

				for (i = 0; i < p->setting_num; ++i) {
					unsigned long sta, val;

					sta = (unsigned long)lpm_smc_spm_dbg(
							MT_SPM_DBG_SMC_HWCG_STATUS,
							MT_LPM_SMC_ACT_GET, node->hwreq_id, i);

					val = (unsigned long)lpm_smc_spm_dbg(
							MT_SPM_DBG_SMC_HWCG_SETTING,
							MT_LPM_SMC_ACT_GET, node->hwreq_id, i);

					lpm_hwreq_log(ToUserBuf, sz, len,
						"[%2d]   0x%08lx 0x%08lx 0x%08lx/0x%08lx\n",
						i, (sta & val), sta, val,
						(unsigned long)lpm_smc_spm_dbg(
							MT_SPM_DBG_SMC_HWCG_DEF_SETTING,
							MT_LPM_SMC_ACT_GET, node->hwreq_id, i));
				}
			}
		}
		break;
	case LPM_HWREQ_NODE_HWCG_SET:
	case LPM_HWREQ_NODE_HWCG_CLR:
		lpm_hwreq_log(ToUserBuf, sz, len, "echo [HWCG Index] [value (hex)]\n");
		break;
	default:
		break;
	}
	return len;
}

static ssize_t lpm_generic_hwreq_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct LPM_HWREQ_NODE *node = (struct LPM_HWREQ_NODE *)priv;

	if (!node)
		return -EINVAL;

	if ((node->type == LPM_HWREQ_NODE_HWCG_SET)
	    || (node->type == LPM_HWREQ_NODE_HWCG_CLR)) {
		unsigned int parm1, parm2, act;

		if (sscanf(FromUserBuf, "%u %x", &parm1, &parm2) == 2) {
			unsigned long arg1 = 0;

			act = (node->type == LPM_HWREQ_NODE_HWCG_SET) ?
				MT_LPM_SMC_ACT_SET :
				(node->type == LPM_HWREQ_NODE_HWCG_CLR) ?
				MT_LPM_SMC_ACT_CLR : 0;

			arg1 = node->hwreq_id;
			arg1 = (arg1 << 32) | parm1;
			if (act != 0)
				lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_SETTING,
						act, arg1, parm2);
		}
	}
	return sz;
}

static void lpm_hwcg_fs_release(void)
{
	struct LPM_HWCG_HANDLES *cur;
	struct LPM_HWCG_HANDLES *next;

	if (list_empty(&lpm_hwcgs)) {
		cur = list_first_entry(&lpm_hwcgs,
					struct LPM_HWCG_HANDLES,
					list);
		do {
			if (list_is_last(&cur->list, &lpm_hwcgs))
				next = NULL;
			else
				next = list_next_entry(cur, list);

			list_del(&cur->list);
			kfree(cur);
			cur = next;
		} while (cur);
		INIT_LIST_HEAD(&lpm_hwcgs);
	}
	kfree(lpm_hwcg);
}

static void lpm_hwcg_fs_create(const char *node_name,
				struct mtk_lp_sysfs_handle *parent)
{
	unsigned long hwcg_num = 0, val = 0;
	int i = 0, j = 0;
	char name[HWCG_RES_NAME];
	struct LPM_HWCG_HANDLES *node;

	if (!node_name)
		return;

	hwcg_num = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_NUM,
				    MT_LPM_SMC_ACT_GET, 0, 0);

	if (!hwcg_num) {
		pr_info("[name:mtk_lpm] Doesn't support hw cg mode\n");
		return;
	}

	if (lpm_hwcg) {
		pr_info("[name:mtk_lpm] hw cg node have been create brfore!!\n");
		return;
	}

	lpm_hwcg = kcalloc(1, sizeof(*lpm_hwcg), GFP_KERNEL);

	if (!lpm_hwcg)
		return;

	mtk_lpm_sysfs_root_entry_create();

	mtk_lpm_sysfs_sub_entry_add(node_name, 0644,
				parent, lpm_hwcg);

	for (i = 0; i < hwcg_num ; ++i) {
		node = kcalloc(1, sizeof(*node), GFP_KERNEL);

		if (!node)
			break;

		node->setting_num =
			(unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_NUM,
						    MT_LPM_SMC_ACT_GET, i, 1);

		val = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_RES_NAME,
					MT_LPM_SMC_ACT_GET, i, 0);

		for (j = 0; j < (HWCG_RES_NAME - 1); ++j)
			name[j] = ((val >> (j<<3)) & 0xFF);
		name[j] = '\0';

		list_add(&node->list, &lpm_hwcgs);
		mtk_lpm_sysfs_sub_entry_add(name, 0644, lpm_hwcg, &node->handle);

		LPM_GENERIC_HWREQ_NODE_INIT(node->hStatus, i, LPM_HWREQ_NODE_HWCG_STA, node);
		mtk_lpm_sysfs_sub_entry_node_add("status", 0644, &node->hStatus.op,
						&node->handle, &node->hStatus.handle);

		LPM_GENERIC_HWREQ_NODE_INIT(node->hSet, i, LPM_HWREQ_NODE_HWCG_SET, node);
		mtk_lpm_sysfs_sub_entry_node_add("set", 0644, &node->hSet.op,
						&node->handle, &node->hSet.handle);

		LPM_GENERIC_HWREQ_NODE_INIT(node->hClr, i, LPM_HWREQ_NODE_HWCG_CLR, node);
		mtk_lpm_sysfs_sub_entry_node_add("clr", 0644, &node->hClr.op,
						&node->handle, &node->hClr.handle);
	}
}

int lpm_hwreq_fs_init(void)
{
	struct device_node *parent = NULL;
	struct property *prop;
	const char *pro_name = NULL;

	parent = of_find_compatible_node(NULL, NULL,
					 MTK_LPM_DTS_COMPATIBLE);

	prop = of_find_property(parent, "hwreq", NULL);

	if (!prop)
		return 0;

	mtk_lpm_sysfs_sub_entry_add(LPM_HWREQ_NAME, 0644, NULL,
				    &lpm_entry_hwreq);

	do {
		pro_name = of_prop_next_string(prop, pro_name);

		if (!pro_name)
			break;

		if (!strcmp(pro_name, LPM_HWCG_NAME))
			lpm_hwcg_fs_create(pro_name, &lpm_entry_hwreq);
	} while (1);

	return 0;
}
EXPORT_SYMBOL(lpm_hwreq_fs_init);

int lpm_hwreq_fs_deinit(void)
{
	lpm_hwcg_fs_release();
	return 0;
}
EXPORT_SYMBOL(lpm_hwreq_fs_deinit);
