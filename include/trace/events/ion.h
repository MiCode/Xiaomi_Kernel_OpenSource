/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ion

#if !defined(_TRACE_ION_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ION_H

#include <linux/types.h>
#include <linux/tracepoint.h>

#define DEV_NAME_NONE "None"

DECLARE_EVENT_CLASS(ion_dma_map_cmo_class,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir),

	TP_STRUCT__entry(
		__string(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE)
		__string(name, name)
		__field(bool, cached)
		__field(bool, hlos_accessible)
		__field(unsigned long, map_attrs)
		__field(enum dma_data_direction, dir)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE);
		__assign_str(name, name);
		__entry->cached = cached;
		__entry->hlos_accessible = hlos_accessible;
		__entry->map_attrs = map_attrs;
		__entry->dir = dir;
	),

	TP_printk("dev=%s name=%s cached=%d access=%d map_attrs=0x%lx dir=%d",
		__get_str(dev_name),
		__get_str(name),
		__entry->cached,
		__entry->hlos_accessible,
		__entry->map_attrs,
		__entry->dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_map_cmo_apply,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_map_cmo_skip,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_unmap_cmo_apply,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DEFINE_EVENT(ion_dma_map_cmo_class, ion_dma_unmap_cmo_skip,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, unsigned long map_attrs,
		 enum dma_data_direction dir),

	TP_ARGS(dev, name, cached, hlos_accessible, map_attrs, dir)
);

DECLARE_EVENT_CLASS(ion_access_cmo_class,

	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped),

	TP_STRUCT__entry(
		__string(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE)
		__string(name, name)
		__field(bool, cached)
		__field(bool, hlos_accessible)
		__field(enum dma_data_direction, dir)
		__field(bool, only_mapped)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev ? dev_name(dev) : DEV_NAME_NONE);
		__assign_str(name, name);
		__entry->cached = cached;
		__entry->hlos_accessible = hlos_accessible;
		__entry->dir = dir;
		__entry->only_mapped = only_mapped;
	),

	TP_printk("dev=%s name=%s cached=%d access=%d dir=%d, only_mapped=%d",
		  __get_str(dev_name),
		  __get_str(name),
		  __entry->cached,
		  __entry->hlos_accessible,
		  __entry->dir,
		  __entry->only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_begin_cpu_access_cmo_apply,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_begin_cpu_access_cmo_skip,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_begin_cpu_access_notmapped,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_end_cpu_access_cmo_apply,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_end_cpu_access_cmo_skip,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);

DEFINE_EVENT(ion_access_cmo_class, ion_end_cpu_access_notmapped,
	TP_PROTO(const struct device *dev, const char *name,
		 bool cached, bool hlos_accessible, enum dma_data_direction dir,
		 bool only_mapped),

	TP_ARGS(dev, name, cached, hlos_accessible, dir, only_mapped)
);
#endif /* _TRACE_ION_H */

#include <trace/define_trace.h>

