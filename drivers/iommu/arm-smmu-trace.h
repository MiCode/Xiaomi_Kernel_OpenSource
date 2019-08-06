/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM arm_smmu

#if !defined(_TRACE_ARM_SMMU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ARM_SMMU_H

#include <linux/types.h>
#include <linux/tracepoint.h>

struct device;

DECLARE_EVENT_CLASS(iommu_tlbi,

	TP_PROTO(struct device *dev, u64 time),

	TP_ARGS(dev, time),

	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(u64, time)
	),

	TP_fast_assign(
		__assign_str(device, dev_name(dev));
		__entry->time = time;
	),

	TP_printk("IOMMU:%s %lld us",
			__get_str(device), __entry->time
	)
);

DEFINE_EVENT(iommu_tlbi, tlbi_start,

	TP_PROTO(struct device *dev, u64 time),

	TP_ARGS(dev, time)
);

DEFINE_EVENT(iommu_tlbi, tlbi_end,

	TP_PROTO(struct device *dev, u64 time),

	TP_ARGS(dev, time)
);

DEFINE_EVENT(iommu_tlbi, tlbsync_timeout,

	TP_PROTO(struct device *dev, u64 time),

	TP_ARGS(dev, time)
);

TRACE_EVENT(smmu_init,

	TP_PROTO(u64 time),

	TP_ARGS(time),

	TP_STRUCT__entry(
		__field(u64, time)
	),

	TP_fast_assign(
		__entry->time = time;
	),

	TP_printk("ARM SMMU init latency: %lld us", __entry->time)
);
#endif /* _TRACE_ARM_SMMU_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/iommu

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE arm-smmu-trace

/* This part must be outside protection */
#include <trace/define_trace.h>
