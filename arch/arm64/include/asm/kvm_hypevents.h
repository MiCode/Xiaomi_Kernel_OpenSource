/* SPDX-License-Identifier: GPL-2.0 */

#if !defined(__ARM64_KVM_HYPEVENTS_H_) || defined(HYP_EVENT_MULTI_READ)
#define __ARM64_KVM_HYPEVENTS_H_

#ifdef __KVM_NVHE_HYPERVISOR__
#include <nvhe/trace.h>
#endif

/*
 * Hypervisor events definitions.
 */

#ifdef CONFIG_PKVM_FTRACE
HYP_EVENT(func,
	HE_PROTO(unsigned long ip, unsigned long parent),
	HE_STRUCT(
		he_field(unsigned long, ip)
		he_field(unsigned long, parent)
	),
	HE_ASSIGN(
		__entry->ip = ip;
		__entry->parent = parent;
	),
	HE_PRINTK("    %ps <- %ps", (void *)__entry->ip, (void *)__entry->parent)
);

HYP_EVENT(func_ret,
	HE_PROTO(unsigned long ip),
	HE_STRUCT(
		he_field(unsigned long, ip)
	),
	HE_ASSIGN(
		__entry->ip = ip;
	),
	HE_PRINTK("%ps", (void *)__entry->ip)
);
#endif

HYP_EVENT(hyp_enter,
	HE_PROTO(void),
	HE_STRUCT(
	),
	HE_ASSIGN(
	),
	HE_PRINTK(" ")
);

HYP_EVENT(hyp_exit,
	HE_PROTO(void),
	HE_STRUCT(
	),
	HE_ASSIGN(
	),
	HE_PRINTK(" ")
);

HYP_EVENT(host_hcall,
	HE_PROTO(unsigned int id, u8 invalid),
	HE_STRUCT(
		he_field(unsigned int, id)
		he_field(u8, invalid)
	),
	HE_ASSIGN(
		__entry->id = id;
		__entry->invalid = invalid;
	),
	HE_PRINTK("id=%u invalid=%u",
		  __entry->id, __entry->invalid)
);

HYP_EVENT(host_smc,
	HE_PROTO(u64 id, u8 forwarded),
	HE_STRUCT(
		he_field(u64, id)
		he_field(u8, forwarded)
	),
	HE_ASSIGN(
		__entry->id = id;
		__entry->forwarded = forwarded;
	),
	HE_PRINTK("id=%llu forwarded=%u",
		  __entry->id, __entry->forwarded)
);


HYP_EVENT(host_mem_abort,
	HE_PROTO(u64 esr, u64 addr),
	HE_STRUCT(
		he_field(u64, esr)
		he_field(u64, addr)
	),
	HE_ASSIGN(
		__entry->esr = esr;
		__entry->addr = addr;
	),
	HE_PRINTK("esr=0x%llx addr=0x%llx",
		  __entry->esr, __entry->addr)
);

HYP_EVENT(__hyp_printk,
	HE_PROTO(u8 fmt_id, u64 a, u64 b, u64 c, u64 d),
	HE_STRUCT(
		he_field(u8, fmt_id)
		he_field(u64, a)
		he_field(u64, b)
		he_field(u64, c)
		he_field(u64, d)
	),
	HE_ASSIGN(
		__entry->fmt_id = fmt_id;
		__entry->a = a;
		__entry->b = b;
		__entry->c = c;
		__entry->d = d;
	),
	HE_PRINTK_UNKNOWN_FMT(hyp_printk_fmt_from_id(__entry->fmt_id),
		__entry->a, __entry->b, __entry->c, __entry->d)
);

HYP_EVENT(psci_mem_protect,
	HE_PROTO(u64 count, u64 was),
	HE_STRUCT(
		he_field(u64, count)
		he_field(u64, was)
	),
	HE_ASSIGN(
		__entry->count = count;
		__entry->was = was;
	),
	HE_PRINTK("count=%llu was=%llu", __entry->count, __entry->was)
);

HYP_EVENT(vcpu_illegal_trap,
	HE_PROTO(u64 esr),
	HE_STRUCT(
		he_field(u64, esr)
	),
	HE_ASSIGN(
		__entry->esr = esr;
	),
	HE_PRINTK("esr_el2=%llx", __entry->esr)
);

#ifdef CONFIG_PKVM_SELFTESTS
HYP_EVENT(selftest,
	  HE_PROTO(void),
	  HE_STRUCT(),
	  HE_ASSIGN(),
	  HE_PRINTK(" ")
);
#endif

HYP_EVENT(iommu_idmap,
	HE_PROTO(u64 from, u64 to, int prot),
	HE_STRUCT(
		he_field(u64, from)
		he_field(u64, to)
		he_field(int, prot)
	),
	HE_ASSIGN(
		__entry->from = from;
		__entry->to = to;
		__entry->prot = prot;
	),
	HE_PRINTK("from=0x%llx to=0x%llx prot=0x%x", __entry->from, __entry->to, __entry->prot)
);

HYP_EVENT(iommu_idmap_complete,
	HE_PROTO(bool map),
	HE_STRUCT(
		he_field(bool, map)
	),
	HE_ASSIGN(
		__entry->map = map;
	),
	HE_PRINTK("map=%d", __entry->map)
);
#endif /* __ARM64_KVM_HYPEVENTS_H_ */
