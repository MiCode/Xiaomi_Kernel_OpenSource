// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>
#include <mtk_resource_constraint_v1.h>

#include <mt6853_pwr_ctrl.h>
#include <mt6853_dbg_fs_common.h>
#include <mt6853_cond.h>
#include <mt6853_spm_comm.h>

#include <mtk_lpm_sysfs.h>
#include <mtk_lp_sysfs.h>
#include <mtk_lpm_platform.h>

#define MT6853_GENERIC_TRACE_NODE_INIT(_n, _r, _w) ({\
		_n.op.fs_read = _r;\
		_n.op.fs_write = _w;\
		_n.op.priv = &_n; })


#define mt6853_dbg_lpm_trace_log(buf, sz, len, fmt, args...) ({\
	if (len < sz)\
		len += scnprintf(buf + len, sz - len,\
				 fmt, ##args); })

/* Common section */
#define MT6853_DGB_TRACE_COMMON_BASE		(0x0)
#define MT6853_DGB_TRACE_COMMON_SZ		(0x20)

#define TRACE_DBG_LAST_RC_INFO_CPU_SHIFT	(28)
#define TRACE_DBG_LAST_RC_INFO_CPU_MASK		(0xf)

#define TRACE_DBG_LAST_RC_INFO_RCID_SHIFT	(0)
#define TRACE_DBG_LAST_RC_INFO_RCID_MASK	(0xffff)

#define TRACE_DBG_LAST_RC_INFO_CPU(info)\
	((info >> TRACE_DBG_LAST_RC_INFO_CPU_SHIFT)\
	& TRACE_DBG_LAST_RC_INFO_CPU_MASK)

#define TRACE_DBG_LAST_RC_INFO_RC_ID(info)\
	((info >> TRACE_DBG_LAST_RC_INFO_RCID_SHIFT)\
	& TRACE_DBG_LAST_RC_INFO_RCID_MASK)


/* Suspend section */
#define MT6853_DGB_TRACE_SUSPEND_BASE \
	(MT6853_DGB_TRACE_COMMON_BASE + MT6853_DGB_TRACE_COMMON_SZ)
#define MT6853_DGB_TRACE_SUSPEND_SZ	(0xe0)


/* Low power section */
#define MT6853_DGB_TRACE_LP_BASE \
	(MT6853_DGB_TRACE_SUSPEND_BASE + MT6853_DGB_TRACE_SUSPEND_SZ)
#define MT6853_DGB_TRACE_LP_SZ		(0x200)


struct MT6853_LPM_TRACE_NODE {
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

struct mtk_lp_sysfs_handle mt6853_entry_lpm_trace;

struct MT6853_LPM_TRACE_NODE mt6853_lpm_trace_node_comm;
struct MT6853_LPM_TRACE_NODE mt6853_lpm_trace_node_lp;
struct MT6853_LPM_TRACE_NODE mt6853_lpm_trace_node_suspend;

struct MT6853_TRACE_COMM {
	unsigned int magic;
	unsigned int common_fp;
	unsigned int rc_timestamp_h;
	unsigned int rc_timestamp_l;
	unsigned int rc_info;
	unsigned int rc_fp;
	unsigned int rc_valid;
};

static ssize_t mt6853_dbg_lpm_trace_suspend_show(char *ToUserBuf,
							size_t sz, void *priv)
{
	struct MTK_LPM_PLAT_TRACE trace;
	unsigned int dump[4], offset = 0;
	size_t len = 0;
	int ret = 0;

	ret = mtk_lpm_platform_trace_get(MT_LPM_PLAT_TRACE_SYSRAM, &trace);

	if (ret || !trace.read)
		return -ENOENT;

	while (((offset + sizeof(dump)) < MT6853_DGB_TRACE_SUSPEND_SZ)) {
		dump[0] = dump[1] = dump[2] = dump[3] = 0;
		trace.read((MT6853_DGB_TRACE_SUSPEND_BASE + offset),
			   dump, sizeof(dump));
		mt6853_dbg_lpm_trace_log(ToUserBuf, sz, len,
				"%03xh: %08x %08x %08x %08x\n", offset,
				dump[0], dump[1], dump[2], dump[3]);
		offset += sizeof(dump);
	}

	return len;
}

static ssize_t mt6853_dbg_lpm_trace_lp_show(char *ToUserBuf,
						      size_t sz, void *priv)
{
	struct MTK_LPM_PLAT_TRACE trace;
	unsigned int dump[4], offset = 0;
	size_t len = 0;
	int ret = 0;

	ret = mtk_lpm_platform_trace_get(MT_LPM_PLAT_TRACE_SYSRAM, &trace);

	if (ret || !trace.read)
		return -ENOENT;

	while (((offset + sizeof(dump)) <= MT6853_DGB_TRACE_LP_SZ)) {
		dump[0] = dump[1] = dump[2] = dump[3] = 0;
		trace.read((MT6853_DGB_TRACE_LP_BASE + offset),
			   dump, sizeof(dump));
		mt6853_dbg_lpm_trace_log(ToUserBuf, sz, len,
				"%03xh: %08x %08x %08x %08x\n", offset,
				dump[0], dump[1], dump[2], dump[3]);
		offset += sizeof(dump);
	}

	return len;
}

static ssize_t mt6853_dbg_lpm_trace_comm_show(char *ToUserBuf,
							size_t sz, void *priv)
{
	struct MTK_LPM_PLAT_TRACE trace;
	struct MT6853_TRACE_COMM *data = NULL;
	size_t len = 0, rSz = 0;
	int ret = 0;

	ret = mtk_lpm_platform_trace_get(MT_LPM_PLAT_TRACE_SYSRAM, &trace);

	if (ret || !trace.read)
		return -ENOENT;

	data = kzalloc(sizeof(struct MT6853_TRACE_COMM),
			GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	rSz = (sizeof(struct MT6853_TRACE_COMM) > MT6853_DGB_TRACE_COMMON_SZ) ?
		MT6853_DGB_TRACE_COMMON_SZ : sizeof(struct MT6853_TRACE_COMM);

	len = trace.read(MT6853_DGB_TRACE_COMMON_BASE, data, rSz);

	if (len > 0) {
		unsigned long long timestamp;

		len = 0;
		timestamp = data->rc_timestamp_h;
		timestamp = (timestamp << 32) | data->rc_timestamp_l;

		mt6853_dbg_lpm_trace_log(ToUserBuf, sz, len,
				"magic: 0x%x\n", data->magic);

		mt6853_dbg_lpm_trace_log(ToUserBuf, sz, len,
				"common_footfprint: 0x%x\n", data->common_fp);

		mt6853_dbg_lpm_trace_log(ToUserBuf, sz, len,
				"timestamp: %llu.%03llu\n",
				timestamp/1000000000,
				timestamp%1000000000);
		mt6853_dbg_lpm_trace_log(ToUserBuf, sz, len,
				"last constriant\n - cpu:%d\n - rc_id:%d\n - valid:0x%x\n",
				TRACE_DBG_LAST_RC_INFO_CPU(data->rc_info),
				TRACE_DBG_LAST_RC_INFO_RC_ID(data->rc_info),
				data->rc_valid);
	}

	kfree(data);
	return sz;
}

int mt6853_dbg_lpm_init(void)
{
	mtk_lpm_sysfs_root_entry_create();
	mtk_lpm_sysfs_sub_entry_add("trace", 0644, NULL,
				    &mt6853_entry_lpm_trace);

	MT6853_GENERIC_TRACE_NODE_INIT(mt6853_lpm_trace_node_comm,
				mt6853_dbg_lpm_trace_comm_show,
				NULL);

	mtk_lpm_sysfs_sub_entry_node_add("common", 0400,
				&mt6853_lpm_trace_node_comm.op,
				&mt6853_entry_lpm_trace,
				&mt6853_lpm_trace_node_comm.handle);

	MT6853_GENERIC_TRACE_NODE_INIT(mt6853_lpm_trace_node_lp,
				mt6853_dbg_lpm_trace_lp_show,
				NULL);

	mtk_lpm_sysfs_sub_entry_node_add("lp", 0400,
				&mt6853_lpm_trace_node_lp.op,
				&mt6853_entry_lpm_trace,
				&mt6853_lpm_trace_node_lp.handle);

	MT6853_GENERIC_TRACE_NODE_INIT(mt6853_lpm_trace_node_suspend,
				mt6853_dbg_lpm_trace_suspend_show,
				NULL);

	mtk_lpm_sysfs_sub_entry_node_add("suspend", 0400,
				&mt6853_lpm_trace_node_suspend.op,
				&mt6853_entry_lpm_trace,
				&mt6853_lpm_trace_node_suspend.handle);
	return 0;
}

int mt6853_dbg_lpm_deinit(void)
{
	mtk_lpm_sysfs_entry_node_remove(&mt6853_lpm_trace_node_suspend.handle);
	mtk_lpm_sysfs_entry_node_remove(&mt6853_lpm_trace_node_lp.handle);
	mtk_lpm_sysfs_entry_node_remove(&mt6853_lpm_trace_node_comm.handle);
	mtk_lpm_sysfs_entry_node_remove(&mt6853_entry_lpm_trace);
	return 0;
}
