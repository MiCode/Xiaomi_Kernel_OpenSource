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

#define HWREQ_RES_NAME		(8)
#define LPM_HWREQ_NAME	"hwreq"
#define LPM_HWCG_NAME	"hw_cg"
#define LPM_PERI_NAME	"peri_req"

#define lpm_hwreq_log(buf, sz, len, fmt, args...) ({\
	if (len < sz)\
		len += scnprintf(buf + len, sz - len,\
				 fmt, ##args); })


enum LPM_HWREQ_NODE_TYPE {
	LPM_HWREQ_NODE_STA,
	LPM_HWREQ_NODE_SET,
	LPM_HWREQ_NODE_CLR,
	LPM_HWREQ_NODE_MAX
};


#define LPM_HWREQ_GENERIC_OP(op, _priv) ({\
	op.fs_read = lpm_generic_hwreq_read;\
	op.fs_write = lpm_generic_hwreq_write;\
	op.priv = _priv; })


#define LPM_GENERIC_HWREQ_NODE_INIT(_n, _id, _type, _parent, _hwreq_fs) ({\
	_n.type = _type;\
	_n.hwreq_id = _id;\
	_n.parent = _parent;\
	_n.hwreq_fs = _hwreq_fs;\
	LPM_HWREQ_GENERIC_OP(_n.op, &_n); })

#define LPM_GENERIC_HWREQ_FS_INIT(_n, _head, _name, _smc_num, _smc_name, \
	_smc_sta, _smc_set, _smc_def_set, _smc_sta_raw, _sysfs_handle) ({\
	_n->head = _head;\
	_n->name = _name;\
	_n->smc_id_num = _smc_num;\
	_n->smc_id_name = _smc_name;\
	_n->smc_id_sta = _smc_sta;\
	_n->smc_id_set = _smc_set;\
	_n->smc_def_set = _smc_def_set;\
	_n->smc_id_raw = _smc_sta_raw;\
	_n->sysfs_handle = _sysfs_handle; })

struct LPM_HWREQ_NODE {
	struct LPM_HWREQ_FS *hwreq_fs;
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

struct LPM_HWREQ_FS {
	struct list_head *head;
	const char *name;
	unsigned int smc_id_num;
	unsigned int smc_id_name;
	unsigned int smc_id_sta;
	unsigned int smc_id_set;
	unsigned int smc_def_set;
	unsigned int smc_id_raw;
	struct mtk_lp_sysfs_handle *sysfs_handle;
};

static struct mtk_lp_sysfs_handle lpm_entry_hwreq;
static struct mtk_lp_sysfs_handle *lpm_hwcg;
static struct mtk_lp_sysfs_handle *lpm_peri_req;
static LIST_HEAD(lpm_hwcg_fs);
static LIST_HEAD(lpm_peri_req_fs);
struct LPM_HWREQ_FS *hwreq_fs_hwcg;
struct LPM_HWREQ_FS *hwreq_fs_peri_req;

static ssize_t lpm_generic_hwreq_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	ssize_t len = 0;
	struct LPM_HWREQ_NODE *node = (struct LPM_HWREQ_NODE *)priv;
	char name[HWREQ_RES_NAME];

	if (!node)
		return -EINVAL;

	switch (node->type) {
	case LPM_HWREQ_NODE_STA:
		if (node->parent) {
			struct LPM_HWCG_HANDLES *p =
				(struct LPM_HWCG_HANDLES *)node->parent;

			if (p->setting_num > 0) {
				int i = 0, j = 0, k = 0;

				lpm_hwreq_log(ToUserBuf, sz, len,
						"Index  Block      Status     Setting/Default\n");

				for (i = 0; i < p->setting_num; ++i) {
					unsigned long sta, val;

					sta = (unsigned long)lpm_smc_spm_dbg(
							(node->hwreq_fs)->smc_id_sta,
							MT_LPM_SMC_ACT_GET, node->hwreq_id, i);

					val = (unsigned long)lpm_smc_spm_dbg(
							(node->hwreq_fs)->smc_id_set,
							MT_LPM_SMC_ACT_GET, node->hwreq_id, i);

					lpm_hwreq_log(ToUserBuf, sz, len,
						"[%2d]   0x%08lx 0x%08lx 0x%08lx/0x%08lx\n",
						i, (sta & val), sta, val,
						(unsigned long)lpm_smc_spm_dbg(
							(node->hwreq_fs)->smc_def_set,
							MT_LPM_SMC_ACT_GET, node->hwreq_id, i));
					if ((node->hwreq_fs)->smc_id_raw > 0) {
						#define RAW_DATA_TYPE_SHIFT	8
						#define RAW_DATA_IDX_SHIFT	4
						#define RAW_DATA_NUM	(0 << RAW_DATA_IDX_SHIFT)
						#define RAW_DATA_NAME	(1 << RAW_DATA_IDX_SHIFT)
						#define RAW_DATA_STA	(2 << RAW_DATA_IDX_SHIFT)
						val = (unsigned long)lpm_smc_spm_dbg(
							(node->hwreq_fs)->smc_id_raw,
							MT_LPM_SMC_ACT_GET, node->hwreq_id,
							(i << RAW_DATA_TYPE_SHIFT) + RAW_DATA_NUM);
					for (j = 0; j < val; j++) {
						unsigned long name_val = lpm_smc_spm_dbg(
							(node->hwreq_fs)->smc_id_raw,
							MT_LPM_SMC_ACT_GET, node->hwreq_id,
							(i << RAW_DATA_TYPE_SHIFT) +
							RAW_DATA_NAME + j);
					for (k = 0; k < (HWREQ_RES_NAME - 1); ++k)
						name[k] = ((name_val >> (k<<3)) & 0xFF);
					name[k] = '\0';

					lpm_hwreq_log(ToUserBuf, sz, len,
					"%17s 0x%08lx (1: clock not gating; 0: clock gating)\n",
					name,
					(unsigned long)lpm_smc_spm_dbg(
					(node->hwreq_fs)->smc_id_raw, MT_LPM_SMC_ACT_GET,
					node->hwreq_id,
					(i << RAW_DATA_TYPE_SHIFT) + RAW_DATA_STA + j));
						}
					}
				}
			}
		}
		break;
	case LPM_HWREQ_NODE_SET:
	case LPM_HWREQ_NODE_CLR:
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

	if ((node->type == LPM_HWREQ_NODE_SET)
	    || (node->type == LPM_HWREQ_NODE_CLR)) {
		unsigned int parm1, parm2, act;

		if (sscanf(FromUserBuf, "%u %x", &parm1, &parm2) == 2) {
			unsigned long arg1 = 0;

			act = (node->type == LPM_HWREQ_NODE_SET) ?
				MT_LPM_SMC_ACT_SET :
				(node->type == LPM_HWREQ_NODE_CLR) ?
				MT_LPM_SMC_ACT_CLR : 0;

			arg1 = node->hwreq_id;
			arg1 = (arg1 << 32) | parm1;
			if (act != 0)
				lpm_smc_spm_dbg((node->hwreq_fs)->smc_id_set,
						act, arg1, parm2);
		}
	}
	return sz;
}

static void lpm_hwcg_fs_release(struct LPM_HWREQ_FS *hwreq_fs)
{
	struct LPM_HWCG_HANDLES *cur;
	struct LPM_HWCG_HANDLES *next;
	struct list_head *head = hwreq_fs->head;

	if (list_empty(head)) {
		cur = list_first_entry(head,
					struct LPM_HWCG_HANDLES,
					list);
		do {
			if (list_is_last(&cur->list, head))
				next = NULL;
			else
				next = list_next_entry(cur, list);

			list_del(&cur->list);
			kfree(cur);
			cur = next;
		} while (cur);
		INIT_LIST_HEAD(head);
	}

	kfree(hwreq_fs->sysfs_handle);

	kfree(hwreq_fs);
}

//static void lpm_hwreq_fs_create(const char *node_name,
static void lpm_hwreq_fs_create(struct LPM_HWREQ_FS *hwreq_fs,
				struct mtk_lp_sysfs_handle *parent)
{
	unsigned long resource_num = 0, val = 0;
	int i = 0, j = 0;
	char name[HWREQ_RES_NAME*2] = {0x20};
	struct LPM_HWCG_HANDLES *node;

	if (!hwreq_fs->name)
		return;

	resource_num = lpm_smc_spm_dbg(hwreq_fs->smc_id_num,
				    MT_LPM_SMC_ACT_GET, 0, 0);

	if (!resource_num) {
		pr_info("[name:mtk_lpm] Doesn't support %s mode\n", hwreq_fs->name);
		return;
	}

	if (hwreq_fs->sysfs_handle) {
		pr_info("[name:mtk_lpm] %s node have been create brfore!!\n", hwreq_fs->name);
		return;
	}

	hwreq_fs->sysfs_handle = kcalloc(1, sizeof(*hwreq_fs->sysfs_handle), GFP_KERNEL);

	if (!hwreq_fs->sysfs_handle)
		return;

	mtk_lpm_sysfs_root_entry_create();

	mtk_lpm_sysfs_sub_entry_add(hwreq_fs->name, 0644,
				parent, hwreq_fs->sysfs_handle);


	for (i = 0; i < resource_num ; ++i) {
		node = kcalloc(1, sizeof(*node), GFP_KERNEL);

		if (!node)
			break;

		node->setting_num =
			(unsigned int)lpm_smc_spm_dbg(hwreq_fs->smc_id_num,
						    MT_LPM_SMC_ACT_GET, i, 1);

		val = lpm_smc_spm_dbg(hwreq_fs->smc_id_name,
					MT_LPM_SMC_ACT_GET, i, 0);

		memset(name, 0x20, sizeof(name));
		for (j = 0; j < HWREQ_RES_NAME; ++j) {
			name[j] = ((val >> (j<<3)) & 0xFF);
			if (name[j] == '\0')
				break;
		}

		if (name[j] != '\0') {
			val = lpm_smc_spm_dbg(hwreq_fs->smc_id_name,
					MT_LPM_SMC_ACT_GET, i, 0);
			for (j = HWREQ_RES_NAME; j < (2*HWREQ_RES_NAME - 1); ++j) {
				name[j] = ((val >> ((j-HWREQ_RES_NAME)<<3)) & 0xFF);

				if (name[j] == '\0')
					break;
			}
		}
		name[j] = '\0';

		list_add(&node->list, hwreq_fs->head);
		mtk_lpm_sysfs_sub_entry_add(name, 0644, hwreq_fs->sysfs_handle, &node->handle);

		LPM_GENERIC_HWREQ_NODE_INIT(node->hStatus, i, LPM_HWREQ_NODE_STA, node, hwreq_fs);
		mtk_lpm_sysfs_sub_entry_node_add("status", 0644, &node->hStatus.op,
						&node->handle, &node->hStatus.handle);

		LPM_GENERIC_HWREQ_NODE_INIT(node->hSet, i, LPM_HWREQ_NODE_SET, node, hwreq_fs);
		mtk_lpm_sysfs_sub_entry_node_add("set", 0644, &node->hSet.op,
						&node->handle, &node->hSet.handle);

		LPM_GENERIC_HWREQ_NODE_INIT(node->hClr, i, LPM_HWREQ_NODE_CLR, node, hwreq_fs);
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

		if (!strcmp(pro_name, LPM_HWCG_NAME)) {
			hwreq_fs_hwcg = kcalloc(1, sizeof(*hwreq_fs_hwcg), GFP_KERNEL);
			LPM_GENERIC_HWREQ_FS_INIT(hwreq_fs_hwcg, &lpm_hwcg_fs, pro_name,
			MT_SPM_DBG_SMC_HWCG_NUM, MT_SPM_DBG_SMC_HWCG_RES_NAME,
			MT_SPM_DBG_SMC_HWCG_STATUS, MT_SPM_DBG_SMC_HWCG_SETTING,
			MT_SPM_DBG_SMC_HWCG_DEF_SETTING, 0,
			lpm_hwcg);
			lpm_hwreq_fs_create(hwreq_fs_hwcg, &lpm_entry_hwreq);
		} else if (!strcmp(pro_name, LPM_PERI_NAME)) {
			hwreq_fs_peri_req = kcalloc(1, sizeof(*hwreq_fs_peri_req), GFP_KERNEL);
			LPM_GENERIC_HWREQ_FS_INIT(hwreq_fs_peri_req, &lpm_peri_req_fs, pro_name,
			MT_SPM_DBG_SMC_PERI_REQ_NUM, MT_SPM_DBG_SMC_PERI_REQ_RES_NAME,
			MT_SPM_DBG_SMC_PERI_REQ_STATUS, MT_SPM_DBG_SMC_PERI_REQ_SETTING,
			MT_SPM_DBG_SMC_PERI_REQ_DEF_SETTING, MT_SPM_DBG_SMC_PERI_REQ_STATUS_RAW,
			lpm_peri_req);
			lpm_hwreq_fs_create(hwreq_fs_peri_req, &lpm_entry_hwreq);
		};
	} while (1);

	return 0;
}
EXPORT_SYMBOL(lpm_hwreq_fs_init);

int lpm_hwreq_fs_deinit(void)
{
	if (hwreq_fs_hwcg)
		lpm_hwcg_fs_release(hwreq_fs_hwcg);

	if (hwreq_fs_peri_req)
		lpm_hwcg_fs_release(hwreq_fs_peri_req);

	return 0;
}
EXPORT_SYMBOL(lpm_hwreq_fs_deinit);
