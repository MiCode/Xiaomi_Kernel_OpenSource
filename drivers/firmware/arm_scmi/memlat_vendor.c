// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "common.h"

#define MAX_MAP_ENTRIES 14
#define MAX_PMU_ENTRIES 24

#define SCMI_VENDOR_MSG_START   (3)
#define SCMI_VENDOR_MSG_MODULE_START   (16)
#define SCMI_MAX_RX_SIZE	128
#define SCMI_MAX_GET_DATA_SIZE	124

enum scmi_memlat_protocol_cmd {
	MEMLAT_SET_LOG_LEVEL = SCMI_VENDOR_MSG_START,
	MEMLAT_SET_CPU_GROUP = SCMI_VENDOR_MSG_MODULE_START,
	MEMLAT_SET_MONITOR,
	MEMLAT_COMMON_PMU_MAP,
	MEMLAT_MON_PMU_MAP,
	MEMLAT_RATIO_CEIL,
	MEMLAT_STALL_FLOOR,
	MEMLAT_L3_L2WB_PCT,
	MEMLAT_L3_IPM_FILTER,
	MEMLAT_SAMPLE_MS,
	MEMLAT_MON_FREQ_MAP,
	MEMLAT_SET_MIN_FREQ,
	MEMLAT_SET_MAX_FREQ,
	MEMLAT_START_MONITOR,
	MEMLAT_STOP_MONITOR,
	MEMLAT_GET_DATA = 0xFF,
	MEMLAT_MAX_MSG
};

struct node_msg {
	uint32_t cpumask;
	uint32_t mon_type;
};

struct scalar_param_msg {
	uint32_t cpumask;
	uint32_t mon_type;
	uint32_t val;
};

struct map_table {
	uint32_t v1;
	uint32_t v2;
};

struct map_param_msg {
	uint32_t cpumask;
	uint32_t mon_type;
	uint32_t nr_rows;
	struct map_table tbl[MAX_MAP_ENTRIES];
};

struct pmu_map_msg {
	uint32_t cpumask;
	uint32_t mon_type;
	uint32_t nr_entries;
	uint32_t pmu[MAX_PMU_ENTRIES];
};

static int scmi_set_cpugrp_mon(const struct scmi_handle *handle,
			u32 cpus_mpidr, u32 mon_type, u32 msg_id)
{
	int ret = 0;
	struct scmi_xfer *t;
	struct node_msg *msg;

	ret = scmi_xfer_get_init(handle, msg_id,
				SCMI_PROTOCOL_MEMLAT,
				sizeof(*msg), sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	msg->cpumask = cpu_to_le32(cpus_mpidr);
	msg->mon_type = cpu_to_le32(mon_type);
	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);

	return ret;
}

static int scmi_set_mon(const struct scmi_handle *handle,
			u32 cpus_mpidr, u32 mon_type)
{
	return scmi_set_cpugrp_mon(handle, cpus_mpidr,
				mon_type, MEMLAT_SET_MONITOR);
}

static int scmi_set_cpu_grp(const struct scmi_handle *handle,
			u32 cpus_mpidr, u32 mon_type)
{
	return scmi_set_cpugrp_mon(handle, cpus_mpidr,
				mon_type, MEMLAT_SET_CPU_GROUP);
}

static int scmi_send_pmu_map_command(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type, u32 nr_entries,
				void *buf, u32 msg_id)
{
	int ret, i = 0;
	struct scmi_xfer *t;
	struct pmu_map_msg *msg;
	u32 *dst;
	struct map_table *src = buf;

	if (nr_entries > MAX_PMU_ENTRIES)
		return -EINVAL;

	ret = scmi_xfer_get_init(handle, msg_id,
				SCMI_PROTOCOL_MEMLAT,
				sizeof(*msg), sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	msg->cpumask = cpu_to_le32(cpus_mpidr);
	msg->mon_type = cpu_to_le32(mon_type);
	msg->nr_entries = cpu_to_le32(nr_entries);
	dst = msg->pmu;

	for (i = 0; i < nr_entries; i++)
		dst[i] = cpu_to_le32(src[i].v2);

	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);
	return ret;
}

static int scmi_common_pmu_map(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type,
				u32 nr_entries, void *buf)
{
	return scmi_send_pmu_map_command(handle, cpus_mpidr,
					mon_type, nr_entries, buf,
					MEMLAT_COMMON_PMU_MAP);
}

static int scmi_mon_pmu_map(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type,
				u32 nr_entries, void *buf)
{
	return scmi_send_pmu_map_command(handle, cpus_mpidr,
					mon_type, nr_entries, buf,
					MEMLAT_MON_PMU_MAP);
}

static int scmi_freq_map(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type,
				u32 nr_rows, void *buf)
{
	int ret, i = 0;
	struct scmi_xfer *t;
	struct map_param_msg *msg;
	struct map_table *tbl, *src = buf;

	if (nr_rows > MAX_MAP_ENTRIES)
		return -EINVAL;

	ret = scmi_xfer_get_init(handle, MEMLAT_MON_FREQ_MAP,
				SCMI_PROTOCOL_MEMLAT,
				sizeof(*msg), sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	msg->cpumask = cpu_to_le32(cpus_mpidr);
	msg->mon_type = cpu_to_le32(mon_type);
	msg->nr_rows = cpu_to_le32(nr_rows);
	tbl = msg->tbl;

	for (i = 0; i < nr_rows; i++) {
		tbl[i].v1 = cpu_to_le32(src[i].v1);
		tbl[i].v2 = cpu_to_le32(src[i].v2);
	}
	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);
	return ret;
}

#define scmi_send_cmd(name, _msg_id)					\
static int scmi_##name(const struct scmi_handle *handle,		\
				u32 cpus_mpidr, u32 mon_type, u32 val)	\
{									\
	int ret = 0;							\
	struct scmi_xfer *t;						\
	struct scalar_param_msg *msg;					\
	ret = scmi_xfer_get_init(handle, _msg_id,			\
				SCMI_PROTOCOL_MEMLAT,			\
				sizeof(*msg), sizeof(*msg), &t);	\
	if (ret)							\
		return ret;						\
	msg = t->tx.buf;						\
	msg->cpumask = cpu_to_le32(cpus_mpidr);				\
	msg->mon_type = cpu_to_le32(mon_type);				\
	msg->val = cpu_to_le32(val);					\
	ret = scmi_do_xfer(handle, t);					\
	scmi_xfer_put(handle, t);					\
	return ret;							\
}									\

scmi_send_cmd(ratio_ceil, MEMLAT_RATIO_CEIL);
scmi_send_cmd(stall_floor, MEMLAT_STALL_FLOOR);
scmi_send_cmd(l2wb_pct, MEMLAT_L3_L2WB_PCT);
scmi_send_cmd(l2wb_filter, MEMLAT_L3_IPM_FILTER);
scmi_send_cmd(sample_ms, MEMLAT_SAMPLE_MS);
scmi_send_cmd(min_freq, MEMLAT_SET_MIN_FREQ);
scmi_send_cmd(max_freq, MEMLAT_SET_MAX_FREQ);

static int scmi_send_start_stop(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type, u32 msg_id)
{
	int ret = 0;
	struct scmi_xfer *t;
	struct scalar_param_msg *msg;

	ret = scmi_xfer_get_init(handle, msg_id,
				SCMI_PROTOCOL_MEMLAT,
				sizeof(*msg), sizeof(*msg), &t);
	if (ret)
		return ret;
	msg = t->tx.buf;
	msg->cpumask = cpu_to_le32(cpus_mpidr);
	msg->mon_type = cpu_to_le32(mon_type);
	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);

	return ret;
}

static int scmi_stop_mon(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type)
{
	return scmi_send_start_stop(handle, cpus_mpidr,
				mon_type, MEMLAT_STOP_MONITOR);
}

static int scmi_start_mon(const struct scmi_handle *handle,
				u32 cpus_mpidr, u32 mon_type)
{
	return scmi_send_start_stop(handle, cpus_mpidr,
				mon_type, MEMLAT_START_MONITOR);
}

static int scmi_get_data(const struct scmi_handle *handle, u8 *buf)
{
	int ret = 0;
	struct scmi_xfer *t;
	u32 prev_cnt = 0;
	struct scalar_param_msg *msg;

	ret = scmi_xfer_get_init(handle, MEMLAT_GET_DATA,
				 SCMI_PROTOCOL_MEMLAT, sizeof(*msg),
				 SCMI_MAX_RX_SIZE, &t);
	if (ret)
		return ret;
	do {
		ret = scmi_do_xfer(handle, t);
		if (ret == -ETIMEDOUT)
			ret = scmi_do_xfer(handle, t);
		if (ret < 0)
			break;

		memcpy((buf + prev_cnt), t->rx.buf, t->rx.len);
		prev_cnt += t->rx.len;
	} while (t->rx.len >= SCMI_MAX_GET_DATA_SIZE);

	scmi_xfer_put(handle, t);

	return ret;
}

static int scmi_set_log_level(const struct scmi_handle *handle, u32 val)
{
	int ret = 0;
	struct scmi_xfer *t;
	u32 *ptr;

	ret = scmi_xfer_get_init(handle, MEMLAT_SET_LOG_LEVEL,
				SCMI_PROTOCOL_MEMLAT, sizeof(u32),
				sizeof(u32), &t);
	if (ret)
		return ret;
	ptr = (u32 *)t->tx.buf;
	*ptr = cpu_to_le32(val);
	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);

	return ret;
}

static struct scmi_memlat_vendor_ops memlat_ops = {
	.set_cpu_grp = scmi_set_cpu_grp,
	.freq_map = scmi_freq_map,
	.set_mon = scmi_set_mon,
	.common_pmu_map = scmi_common_pmu_map,
	.mon_pmu_map = scmi_mon_pmu_map,
	.ratio_ceil = scmi_ratio_ceil,
	.stall_floor = scmi_stall_floor,
	.sample_ms = scmi_sample_ms,
	.l2wb_filter = scmi_l2wb_filter,
	.l2wb_pct = scmi_l2wb_pct,
	.min_freq = scmi_min_freq,
	.max_freq = scmi_max_freq,
	.start_monitor = scmi_start_mon,
	.stop_monitor = scmi_stop_mon,
	.set_log_level = scmi_set_log_level,
	.get_data = scmi_get_data,
};

static int scmi_memlat_vendor_protocol_init(struct scmi_handle *handle)
{
	u32 version;

	scmi_version_get(handle, SCMI_PROTOCOL_MEMLAT, &version);

	dev_dbg(handle->dev, "memlat version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	handle->memlat_ops = &memlat_ops;

	return 0;
}

static int __init scmi_memlat_init(void)
{
	return scmi_protocol_register(SCMI_PROTOCOL_MEMLAT,
				      &scmi_memlat_vendor_protocol_init);
}
subsys_initcall(scmi_memlat_init);
