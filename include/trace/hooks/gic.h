/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM gic

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_GIC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_GIC_H

#include <trace/hooks/vendor_hooks.h>

/* struct cpumask */
#include <linux/cpumask.h>
/* struct irq_data */
#include <linux/irq.h>
/* struct irq_domain */
#include <linux/irqdomain.h>

DECLARE_HOOK(android_vh_gic_v2_resume,
	TP_PROTO(struct irq_domain *domain, void __iomem *dist_base),
	TP_ARGS(domain, dist_base));

DECLARE_HOOK(android_vh_gic_set_affinity,
	TP_PROTO(struct irq_data *d, const struct cpumask *mask_val,
		 bool force, u8 *gic_cpu_map, void __iomem *reg),
	TP_ARGS(d, mask_val, force, gic_cpu_map, reg));

#endif /* _TRACE_HOOK_GIC_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
