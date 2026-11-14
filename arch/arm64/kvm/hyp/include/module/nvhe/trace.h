/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ARM64_KVM_HYP_NVHE_TRACE_H
#define __ARM64_KVM_HYP_NVHE_TRACE_H
#include <asm/kvm_hyptrace.h>
#include <asm/kvm_hypevents_defs.h>

#ifdef CONFIG_TRACING
void *tracing_reserve_entry(unsigned long length);
void tracing_commit_entry(void);
int register_hyp_mod_events(void *event_ids, size_t nr_events,
			    void *funcs, void *funcs_end,
			    void *tramp,
			    unsigned long kern_hyp_offset);

#define HYP_EVENT(__name, __proto, __struct, __assign, __printk)		\
	HYP_EVENT_FORMAT(__name, __struct);					\
	extern atomic_t __name##_enabled;					\
	extern struct hyp_event_id hyp_event_id_##__name;			\
	static inline void trace_##__name(__proto)				\
	{									\
		size_t length = sizeof(struct trace_hyp_format_##__name);	\
		struct trace_hyp_format_##__name *__entry;			\
										\
		if (!atomic_read(&__name##_enabled))				\
			return;							\
		__entry = tracing_reserve_entry(length);			\
		if (!__entry)							\
			return;							\
		__entry->hdr.id = hyp_event_id_##__name.id;			\
		__assign							\
		tracing_commit_entry();						\
	}

void __pkvm_update_clock_tracing(u32 mult, u32 shift, u64 epoch_ns, u64 epoch_cyc);
int __pkvm_load_tracing(unsigned long desc_va, size_t desc_size);
void __pkvm_teardown_tracing(void);
int __pkvm_enable_tracing(bool enable);
int __pkvm_reset_tracing(unsigned int cpu);
int __pkvm_swap_reader_tracing(unsigned int cpu);
void __pkvm_panic_tracing(void);
int __pkvm_enable_event(unsigned short id, bool enable);

extern struct hyp_printk_fmt __hyp_printk_fmts_start[];

#ifdef MODULE
#define hyp_printk_fmt_to_id(fmt)					\
({									\
	static u8 fmt_id_offset __section(".hyp.printk_fmt_offset") __used;	\
	(struct hyp_printk_fmt *)fmt - __hyp_printk_fmts_start + fmt_id_offset; \
})
#else
static inline u8 hyp_printk_fmt_to_id(const char *fmt)
{
	return (struct hyp_printk_fmt *)fmt - __hyp_printk_fmts_start;
}
#endif

#define __trace_hyp_printk(__fmt, a, b, c, d)		\
do {							\
	static struct hyp_printk_fmt __used		\
			__section(".hyp.printk_fmts")	\
			ht_fmt = {			\
				.fmt = __fmt		\
	};						\
	trace___hyp_printk(hyp_printk_fmt_to_id(ht_fmt.fmt), a, b, c, d); \
} while (0)

#define __trace_hyp_printk_0(fmt, arg)		\
	__trace_hyp_printk(fmt, 0, 0, 0, 0)
#define __trace_hyp_printk_1(fmt, a)		\
	__trace_hyp_printk(fmt, a, 0, 0, 0)
#define __trace_hyp_printk_2(fmt, a, b)		\
	__trace_hyp_printk(fmt, a, b, 0, 0)
#define __trace_hyp_printk_3(fmt, a, b, c)	\
	__trace_hyp_printk(fmt, a, b, c, 0)
#define __trace_hyp_printk_4(fmt, a, b, c, d) \
	__trace_hyp_printk(fmt, a, b, c, d)

#define __trace_hyp_printk_N(fmt, ...) \
	CONCATENATE(__trace_hyp_printk_, COUNT_ARGS(__VA_ARGS__))(fmt, ##__VA_ARGS__)

#define trace_hyp_printk(fmt, ...) \
	__trace_hyp_printk_N(fmt, __VA_ARGS__)

#ifdef CONFIG_PKVM_FTRACE
void hyp_ftrace_setup_core(void);
unsigned long *hyp_ftrace_find_host_func(unsigned long host_func,
					 unsigned long *funcs,
					 unsigned long *funcs_end,
					 unsigned long offset_idx);
void *hyp_ftrace_sync(unsigned long *func_pg, unsigned long *funcs,
		      unsigned long *funcs_end, unsigned long offset_idx,
		      void *tramp);
int hyp_ftrace_setup(unsigned long *funcs, unsigned long *funcs_end,
		     unsigned long hyp_kern_offset, void *tramp);
void hyp_ftrace_ret_flush(void);
void hyp_ftrace_disable(unsigned long *funcs, unsigned long *funcs_end);
int __pkvm_sync_ftrace(unsigned long host_func_pg);
int __pkvm_disable_ftrace(void);
#else
static inline void hyp_ftrace_setup_core(void) { }
static inline void hyp_ftrace_ret_flush(void) { }
static inline int hyp_ftrace_setup(unsigned long *funcs, unsigned long *funcs_end,
				   unsigned long hyp_kern_offset, void *tramp) { return 0; }
static inline void hyp_ftrace_enable(unsigned long *funcs, unsigned long *funcs_end,
		       bool enable, void *tramp) { }
static inline int __pkvm_sync_ftrace(unsigned long host_func_pg) { return -EOPNOTSUPP; }
static inline int __pkvm_disable_ftrace(void) { return -EOPNOTSUPP; }
#endif /* CONFIG_PKVM_FTRACE */
#else /* CONFIG_TRACING */
static inline int
register_hyp_mod_events(void *event_ids, size_t nr_events, void *funcs, void *funcs_end,
			void *tramp, unsigned long kern_hyp_offset) { return 0; }
static inline void *tracing_reserve_entry(unsigned long length) { return NULL; }
static inline void tracing_commit_entry(void) { }
static inline int register_hyp_event_ids(void *event_ids, size_t nr_events)
{
	return -ENODEV;
}

#define HYP_EVENT(__name, __proto, __struct, __assign, __printk)      \
	static inline void trace_##__name(__proto) {}

static inline
void __pkvm_update_clock_tracing(u32 mult, u32 shift, u64 epoch_ns, u64 epoch_cyc) { }
static inline int __pkvm_load_tracing(unsigned long desc_va, size_t desc_size) { return -ENODEV; }
static inline void __pkvm_teardown_tracing(void) { }
static inline int __pkvm_enable_tracing(bool enable) { return -ENODEV; }
static inline int __pkvm_reset_tracing(unsigned int cpu) { return -ENODEV; }
static inline int __pkvm_swap_reader_tracing(unsigned int cpu) { return -ENODEV; }
static inline void __pkvm_panic_tracing(void) { }
static inline int __pkvm_enable_event(unsigned short id, bool enable)  { return -ENODEV; }
#define trace_hyp_printk(fmt, ...)

static inline void hyp_ftrace_setup_core(void) { }
static inline void hyp_ftrace_ret_flush(void) { }
static inline int __pkvm_sync_ftrace(unsigned long host_func_pg) { return -EOPNOTSUPP; }
static inline int __pkvm_disable_ftrace(void) { return -EOPNOTSUPP; }
#endif
#endif
