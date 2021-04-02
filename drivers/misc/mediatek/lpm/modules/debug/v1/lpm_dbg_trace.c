// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>
#include <lpm_resource_constraint_v1.h>

#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_syssram_v1.h>

#include <mtk_lpm_sysfs.h>
#include <mtk_lp_sysfs.h>
#include <lpm_plat_common.h>

#define LPM_GENERIC_TRACE_NODE_INIT(_n, _r, _w) ({\
		_n.op.fs_read = _r;\
		_n.op.fs_write = _w;\
		_n.op.priv = &_n; })


#define lpm_dbg_trace_log(buf, sz, len, fmt, args...) ({\
	if (len < sz)\
		len += scnprintf(buf + len, sz - len,\
				 fmt, ##args); })

/* Common section */
#define LPM_DGB_TRACE_COMMON_BASE		(0x0)
#define LPM_DGB_TRACE_COMMON_SZ		(0x20)

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
#define LPM_DGB_TRACE_SUSPEND_BASE \
	(LPM_DGB_TRACE_COMMON_BASE + LPM_DGB_TRACE_COMMON_SZ)
#define LPM_DGB_TRACE_SUSPEND_SZ	(0xe0)


/* Low power section */
#define LPM_DGB_TRACE_LP_BASE \
	(LPM_DGB_TRACE_SUSPEND_BASE + LPM_DGB_TRACE_SUSPEND_SZ)
#define LPM_DGB_TRACE_LP_SZ		(0x200)


struct LPM_TRACE_NODE {
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

struct mtk_lp_sysfs_handle lpm_entry_trace;

struct LPM_TRACE_NODE lpm_trace_node_comm;
struct LPM_TRACE_NODE lpm_trace_node_lp;
struct LPM_TRACE_NODE lpm_trace_node_suspend;

struct LPM_TRACE_COMM {
	unsigned int magic;
	unsigned int common_fp;
	unsigned int rc_timestamp_h;
	unsigned int rc_timestamp_l;
	unsigned int rc_info;
	unsigned int rc_fp;
	unsigned int rc_valid;
};

static ssize_t lpm_dbg_trace_suspend_show(char *ToUserBuf,
							size_t sz, void *priv)
{
	struct LPM_PLAT_TRACE trace;
	unsigned int dump[4], offset = 0;
	size_t len = 0;
	int ret = 0;

	ret = lpm_platform_trace_get(LPM_PLAT_TRACE_SYSRAM, &trace);

	if (ret || !trace.read)
		return -ENOENT;

	lpm_dbg_trace_log(ToUserBuf, sz, len, "[SUSPEND]\n");
	while (((offset + sizeof(dump)) < LPM_DGB_TRACE_SUSPEND_SZ)) {
		dump[0] = dump[1] = dump[2] = dump[3] = 0;
		trace.read((LPM_DGB_TRACE_SUSPEND_BASE + offset),
			   dump, sizeof(dump));
		lpm_dbg_trace_log(ToUserBuf, sz, len,
				"%03xh: %08x %08x %08x %08x\n", offset,
				dump[0], dump[1], dump[2], dump[3]);
		offset += sizeof(dump);
	}

	return len;
}

static ssize_t lpm_dbg_trace_lp_show(char *ToUserBuf,
						      size_t sz, void *priv)
{
	struct LPM_PLAT_TRACE trace;
	unsigned int dump[4], offset = 0;
	size_t len = 0;
	int ret = 0;

	ret = lpm_platform_trace_get(LPM_PLAT_TRACE_SYSRAM, &trace);

	if (ret || !trace.read)
		return -ENOENT;

	lpm_dbg_trace_log(ToUserBuf, sz, len, "[LP]\n");
	while (((offset + sizeof(dump)) < LPM_DGB_TRACE_LP_SZ)) {
		dump[0] = dump[1] = dump[2] = dump[3] = 0;
		trace.read((LPM_DGB_TRACE_LP_BASE + offset),
			   dump, sizeof(dump));
		lpm_dbg_trace_log(ToUserBuf, sz, len,
				"%03xh: %08x %08x %08x %08x\n", offset,
				dump[0], dump[1], dump[2], dump[3]);
		offset += sizeof(dump);
	}

	return len;
}

static ssize_t lpm_dbg_trace_comm_show(char *ToUserBuf,
							size_t sz, void *priv)
{
	struct LPM_PLAT_TRACE trace;
	struct LPM_TRACE_COMM *data = NULL;
	size_t len = 0, rSz = 0;
	int ret = 0;

	ret = lpm_platform_trace_get(LPM_PLAT_TRACE_SYSRAM, &trace);

	if (ret || !trace.read)
		return -ENOENT;

	data = kzalloc(sizeof(struct LPM_TRACE_COMM),
			GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	rSz = (sizeof(struct LPM_TRACE_COMM) > LPM_DGB_TRACE_COMMON_SZ) ?
		LPM_DGB_TRACE_COMMON_SZ : sizeof(struct LPM_TRACE_COMM);

	len = trace.read(LPM_DGB_TRACE_COMMON_BASE, data, rSz);

	if (len > 0) {
		unsigned long long timestamp;

		len = 0;
		timestamp = data->rc_timestamp_h;
		timestamp = (timestamp << 32) | data->rc_timestamp_l;

		lpm_dbg_trace_log(ToUserBuf, sz, len,
				"magic: 0x%x\n", data->magic);

		lpm_dbg_trace_log(ToUserBuf, sz, len,
				"common_footfprint: 0x%x\n", data->common_fp);

		lpm_dbg_trace_log(ToUserBuf, sz, len,
				"timestamp: %llu.%03llu\n",
				timestamp/1000000000,
				timestamp%1000000000);
		lpm_dbg_trace_log(ToUserBuf, sz, len,
				"last constriant\n - cpu:%d\n - rc_id:%d\n - valid:0x%x\n",
				TRACE_DBG_LAST_RC_INFO_CPU(data->rc_info),
				TRACE_DBG_LAST_RC_INFO_RC_ID(data->rc_info),
				data->rc_valid);
	}

	kfree(data);
	return len;
}

int lpm_dbg_init(void)
{
	mtk_lpm_sysfs_root_entry_create();
	mtk_lpm_sysfs_sub_entry_add("trace", 0644, NULL,
				    &lpm_entry_trace);

	LPM_GENERIC_TRACE_NODE_INIT(lpm_trace_node_comm,
				lpm_dbg_trace_comm_show,
				NULL);

	mtk_lpm_sysfs_sub_entry_node_add("common", 0444,
				&lpm_trace_node_comm.op,
				&lpm_entry_trace,
				&lpm_trace_node_comm.handle);

	LPM_GENERIC_TRACE_NODE_INIT(lpm_trace_node_lp,
				lpm_dbg_trace_lp_show,
				NULL);

	mtk_lpm_sysfs_sub_entry_node_add("lp", 0444,
				&lpm_trace_node_lp.op,
				&lpm_entry_trace,
				&lpm_trace_node_lp.handle);

	LPM_GENERIC_TRACE_NODE_INIT(lpm_trace_node_suspend,
				lpm_dbg_trace_suspend_show,
				NULL);

	mtk_lpm_sysfs_sub_entry_node_add("suspend", 0444,
				&lpm_trace_node_suspend.op,
				&lpm_entry_trace,
				&lpm_trace_node_suspend.handle);
	return 0;
}
EXPORT_SYMBOL(lpm_dbg_init);

int lpm_dbg_deinit(void)
{
	mtk_lpm_sysfs_entry_node_remove(&lpm_trace_node_suspend.handle);
	mtk_lpm_sysfs_entry_node_remove(&lpm_trace_node_lp.handle);
	mtk_lpm_sysfs_entry_node_remove(&lpm_trace_node_comm.handle);
	mtk_lpm_sysfs_entry_node_remove(&lpm_entry_trace);
	return 0;
}
EXPORT_SYMBOL(lpm_dbg_deinit);
