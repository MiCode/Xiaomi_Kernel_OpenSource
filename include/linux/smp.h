#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H

/*
 *	Generic SMP support
 *		Alan Cox. <alan@redhat.com>
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/llist.h>

typedef void (*smp_call_func_t)(void *info);
struct call_single_data {
	struct llist_node llist;
	smp_call_func_t func;
	void *info;
	u16 flags;
};

/* total number of cpus in this system (may exceed NR_CPUS) */
extern unsigned int total_cpus;

int smp_call_function_single(int cpuid, smp_call_func_t func, void *info,
			     int wait);

/*
 * Call a function on all processors
 */
int on_each_cpu(smp_call_func_t func, void *info, int wait);

/*
 * Call a function on processors specified by mask, which might include
 * the local one.
 */
void on_each_cpu_mask(const struct cpumask *mask, smp_call_func_t func,
		void *info, bool wait);

/*
 * Call a function on each processor for which the supplied function
 * cond_func returns a positive value. This may include the local
 * processor.
 */
void on_each_cpu_cond(bool (*cond_func)(int cpu, void *info),
		smp_call_func_t func, void *info, bool wait,
		gfp_t gfp_flags);

int smp_call_function_single_async(int cpu, struct call_single_data *csd);

#ifdef CONFIG_SMP

#include <linux/preempt.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/thread_info.h>
#include <asm/smp.h>
#include <linux/spinlock.h>

#ifdef CONFIG_PROFILE_CPU
struct profile_cpu_stats {
	u64 hotplug_up_time;
	u64 hotplug_down_time;
	u64 hotplug_up_lat_us;
	u64 hotplug_down_lat_us;
	u64 hotplug_up_lat_max;
	u64 hotplug_down_lat_max;
	u64 hotplug_up_lat_min;
	u64 hotplug_down_lat_min;
};

extern struct profile_cpu_stats *cpu_stats;
#endif


/* for Hotplug timestamp profiling */
#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
#define TIMESTAMP_REC_SIZE 300
#define TIMESTAMP_FILTER 1
struct timestamp_rec {
	struct {
		const char *func;
		int line;
		unsigned long long timestamp_us;
		unsigned long long delta_us;
		unsigned long note1;	/* cpu id */
		unsigned long note2;	/* callback event */
		unsigned long note3;	/* callback index */
		unsigned long note4;	/* reservation */
	} rec[TIMESTAMP_REC_SIZE];

	unsigned int rec_idx;
	unsigned int filter;
};
extern struct timestamp_rec hotplug_ts_rec;
extern spinlock_t hotplug_timestamp_lock;
extern unsigned int timestamp_enable;
long hotplug_get_current_time_us(void);

#define SET_TIMESTAMP_FILTER(ts, f) \
do { \
	if (timestamp_enable) {\
		ts.filter = (f);\
	} \
} while (0)

#define BEGIN_TIMESTAMP_REC(ts, f, cpu, event, index, n4) \
do { \
	if (timestamp_enable) {\
		spin_lock(&hotplug_timestamp_lock);\
		if (ts.filter & (f)) { \
				ts.rec[0].func = __func__; \
				ts.rec[0].line = __LINE__; \
				ts.rec[0].timestamp_us = hotplug_get_current_time_us(); \
				ts.rec[0].delta_us = 0; \
				ts.rec[0].note1 = (cpu); \
				ts.rec[0].note2 = (event); \
				ts.rec[0].note3 = (index); \
				ts.rec[0].note4 = (n4); \
				ts.rec_idx = 1; \
		} \
		spin_unlock(&hotplug_timestamp_lock);\
	} \
} while (0)

#define TIMESTAMP_REC(ts, f, cpu, event, index, n4) \
do { \
	if (timestamp_enable) {\
		spin_lock(&hotplug_timestamp_lock);\
		if (ts.rec_idx < TIMESTAMP_REC_SIZE && (ts.filter & (f))) { \
			ts.rec[ts.rec_idx].func = __func__; \
			ts.rec[ts.rec_idx].line = __LINE__; \
			ts.rec[ts.rec_idx].timestamp_us = hotplug_get_current_time_us(); \
			ts.rec[ts.rec_idx].delta_us = ts.rec[ts.rec_idx].timestamp_us - \
			ts.rec[ts.rec_idx-1].timestamp_us; \
			ts.rec[ts.rec_idx].note1 = (cpu); \
			ts.rec[ts.rec_idx].note2 = (event); \
			ts.rec[ts.rec_idx].note3 = (index); \
			ts.rec[ts.rec_idx].note4 = (n4); \
			ts.rec_idx++; \
		} \
		spin_unlock(&hotplug_timestamp_lock);\
	} \
} while (0)

#define END_TIMESTAMP_REC(ts, f, cpu, event, index, n4) \
do { \
	int i; \
	if (timestamp_enable) {\
		spin_lock(&hotplug_timestamp_lock);\
		if (ts.rec_idx < TIMESTAMP_REC_SIZE && (ts.filter & (f))) { \
			ts.rec[ts.rec_idx].func = __func__; \
			ts.rec[ts.rec_idx].line = __LINE__; \
			ts.rec[ts.rec_idx].timestamp_us = hotplug_get_current_time_us(); \
			ts.rec[ts.rec_idx].delta_us = ts.rec[ts.rec_idx].timestamp_us - \
			ts.rec[ts.rec_idx-1].timestamp_us; \
			ts.rec[ts.rec_idx].note1 = (cpu); \
			ts.rec[ts.rec_idx].note2 = (event); \
			ts.rec[ts.rec_idx].note3 = (index); \
			ts.rec[ts.rec_idx].note4 = (n4); \
			ts.rec_idx++; \
		} \
		pr_warn("hotplug timestamp:\ttime\t\tposition\tdelta\tcpu\tevent\tindex\tn4\n"); \
		for (i = 0; i < ts.rec_idx; i++) { \
			pr_warn("hotplug timestamp: %lld\t%s():%d\t%lld\t%ld\t%ld\t%ld\t%p\n",\
			 ts.rec[i].timestamp_us, ts.rec[i].func, ts.rec[i].line, ts.rec[i].delta_us, \
			 ts.rec[i].note1, ts.rec[i].note2, ts.rec[i].note3, (void *)ts.rec[i].note4); \
		} \
		spin_unlock(&hotplug_timestamp_lock);\
	} \
} while (0)

#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_cpu_caller(u32 val);
extern void aee_rr_rec_cpu_callee(u32 val);
extern void aee_rr_rec_cpu_up_prepare_ktime(u64 val);
extern void aee_rr_rec_cpu_starting_ktime(u64 val);
extern void aee_rr_rec_cpu_online_ktime(u64 val);
extern void aee_rr_rec_cpu_down_prepare_ktime(u64 val);
extern void aee_rr_rec_cpu_dying_ktime(u64 val);
extern void aee_rr_rec_cpu_dead_ktime(u64 val);
extern void aee_rr_rec_cpu_post_dead_ktime(u64 val);
#endif

/*
 * main cross-CPU interfaces, handles INIT, TLB flush, STOP, etc.
 * (defined in asm header):
 */

/*
 * stops all CPUs but the current one:
 */
extern void smp_send_stop(void);

/*
 * sends a 'reschedule' event to another CPU:
 */
extern void smp_send_reschedule(int cpu);


/*
 * Prepare machine for booting other CPUs.
 */
extern void smp_prepare_cpus(unsigned int max_cpus);

/*
 * Bring a CPU up
 */
extern int __cpu_up(unsigned int cpunum, struct task_struct *tidle);

/*
 * Final polishing of CPUs
 */
extern void smp_cpus_done(unsigned int max_cpus);

/*
 * Call a function on all other processors
 */
int smp_call_function(smp_call_func_t func, void *info, int wait);
void smp_call_function_many(const struct cpumask *mask,
			    smp_call_func_t func, void *info, bool wait);

int smp_call_function_any(const struct cpumask *mask,
			  smp_call_func_t func, void *info, int wait);

void kick_all_cpus_sync(void);
void wake_up_all_idle_cpus(void);

/*
 * Generic and arch helpers
 */
void __init call_function_init(void);
void generic_smp_call_function_single_interrupt(void);
#define generic_smp_call_function_interrupt \
	generic_smp_call_function_single_interrupt

/*
 * Mark the boot cpu "online" so that it can call console drivers in
 * printk() and can access its per-cpu storage.
 */
void smp_prepare_boot_cpu(void);

extern unsigned int setup_max_cpus;
extern void __init setup_nr_cpu_ids(void);
extern void __init smp_init(void);

#else /* !SMP */

static inline void smp_send_stop(void) { }

/*
 *	These macros fold the SMP functionality into a single CPU system
 */
#define raw_smp_processor_id()			0
static inline int up_smp_call_function(smp_call_func_t func, void *info)
{
	return 0;
}
#define smp_call_function(func, info, wait) \
			(up_smp_call_function(func, info))

static inline void smp_send_reschedule(int cpu) { }
#define smp_prepare_boot_cpu()			do {} while (0)
#define smp_call_function_many(mask, func, info, wait) \
			(up_smp_call_function(func, info))
static inline void call_function_init(void) { }

static inline int
smp_call_function_any(const struct cpumask *mask, smp_call_func_t func,
		      void *info, int wait)
{
	return smp_call_function_single(0, func, info, wait);
}

static inline void kick_all_cpus_sync(void) {  }
static inline void wake_up_all_idle_cpus(void) {  }

#endif /* !SMP */

/*
 * smp_processor_id(): get the current CPU ID.
 *
 * if DEBUG_PREEMPT is enabled then we check whether it is
 * used in a preemption-safe way. (smp_processor_id() is safe
 * if it's used in a preemption-off critical section, or in
 * a thread that is bound to the current CPU.)
 *
 * NOTE: raw_smp_processor_id() is for internal use only
 * (smp_processor_id() is the preferred variant), but in rare
 * instances it might also be used to turn off false positives
 * (i.e. smp_processor_id() use that the debugging code reports but
 * which use for some reason is legal). Don't use this to hack around
 * the warning message, as your code might not work under PREEMPT.
 */
#ifdef CONFIG_DEBUG_PREEMPT
extern unsigned int debug_smp_processor_id(void);
# define smp_processor_id() debug_smp_processor_id()
#else
# define smp_processor_id() raw_smp_processor_id()
#endif

#define get_cpu()		({ preempt_disable(); smp_processor_id(); })
#define put_cpu()		preempt_enable()

/*
 * Callback to arch code if there's nosmp or maxcpus=0 on the
 * boot command line:
 */
extern void arch_disable_smp_support(void);

extern void arch_enable_nonboot_cpus_begin(void);
extern void arch_enable_nonboot_cpus_end(void);

void smp_setup_processor_id(void);

#endif /* __LINUX_SMP_H */
