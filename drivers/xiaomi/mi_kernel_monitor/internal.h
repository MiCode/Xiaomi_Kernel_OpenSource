
#ifndef __XM_INTERNAL_H
#define __XM_INTERNAL_H

#include <linux/interrupt.h>
#include <linux/kprobes.h>
#include <linux/version.h>
#include <linux/cpu.h>
#include <linux/radix-tree.h>
#include <linux/syscalls.h>

#include <linux/uio.h>

#include <linux/sched.h>
#include <linux/binfmts.h>
#include <asm/syscall.h>
#include <linux/sched/clock.h>
#include <linux/percpu-defs.h>

#include "symbol.h"
#include "uapi/kernelmonitor.h"
#include "uapi/sys_delay.h"
////#include "uapi/kprobe.h"
#include "uapi/sys_cost.h"
#include "uapi/rw_sem.h"
#include "pub/stack.h"
#include "uapi/pmu.h"
#include <linux/sched/cputime.h>
#include <linux/mmzone.h>
#include <linux/mi_sched.h>
#include "event.h"

#if IS_ENABLED(CONFIG_MTK_PLATFORM)
#include "../../../../../kernel-6.6/kernel/sched/sched.h"
#else
#include "../../../../common/kernel/sched/sched.h"
#endif

#define MAX_ORDER  MAX_PAGE_ORDER

#define LOOKUP_SYMS(name) do {					\
				orig_##name = (void *)xm_kallsyms_lookup_name(#name); 	\
				if (!orig_##name) { 					\
					pr_err("xiaomi xm_kallsyms_lookup_name: %s\n", #name);		\
					return -EINVAL; 					\
				}								\
			} while (0)

#define LOOKUP_SYMS_NORET(name) do {							\
		orig_##name = (void *)xm_kallsyms_lookup_name(#name);		\
		if (!orig_##name)						\
			pr_err("kallsyms_lookup_name: %s\n", #name);		\
	} while (0)

#define STACK_IS_END(v) ((v) == 0 || (v) == ULONG_MAX)
extern int xm_timer_period;
struct stack_trace {
	unsigned int nr_entries, max_entries;
	unsigned long *entries;
	int skip;	/* input argument: How many entries to skip */
};

struct xm_percpu_context;
struct task_struct;

enum xm_printk_type {
	TRACE_PRINTK,
	TRACE_BUFFER_PRINTK,
	TRACE_BUFFER_PRINTK_NOLOCK,
	TRACE_FILE_PRINTK,
	TRACE_FILE_PRINTK_NOLOCK,
};

enum group_type {
	GRP_VIP,
	GRP_RT,
	GRP_OT,
// #ifdef CONFIG_XM_INTERNAL_VERSION
// 	GRP_BG,
// 	GRP_FG,
// 	GRP_TA,
// #endif
	GRP_TYPES,
};

enum thres_type
{
    LOW_THRES=0,
    HIGH_THRES,
    FATAL_THRES,
    MAX_THRES,
};

int xiaomi_kernel_init(void);
void xiaomi_kernel_exit(void);

extern int xiaomi_huqh_test_init(void);
extern void xiaomi_huqh_test_exit(void);

extern void kern_task_runs_timer(struct xm_percpu_context *);
extern void syscall_timer(struct xm_percpu_context *);
extern void xiaomi_load_timer(struct xm_percpu_context *);
extern int need_dump(int delay_threshold_ms, u64 *max_delay_ms, u64 base);
extern void cpus_read_unlock(void);
extern void cpus_read_lock(void);

#define NR_vm_run (NR_syscalls + 1)
#define NR_page_fault (NR_vm_run + 1)
#define NR_syscalls_virt (NR_page_fault + 1)

struct irq_func_runtime {
	unsigned int		irq;
	irq_handler_t		handler;
	u64 irq_cnt;
	u64 irq_run_total;
};

struct xm_percpu_context {
	unsigned long trace_buf[BACKTRACE_DEPTH];

	struct {
		u64 syscall_start_time;
		u64 sys_delay_max_ms;
		int sys_delay_in_kvm;
		long sys_id;
	} sys_delay;

	struct {
		enum {
			_XM_TIMER_SILENT,
			_XM_TIMER_RUNNING,
		} timer_state;
		int timer_started;
		struct hrtimer timer;
		u64 timer_expected_time;
	} timer_info;

	struct {
		u64 max_irq_delay_ms;
	} irq_delay;

	struct {
		struct irq_runtime {
			int irq;
			s64 time;
			char timestamp[26];
		} irq_runtime;
		
		struct softirq_runtime {
			s64 time[XM_NR_SOFTIRQS];
		} softirq_runtime;


		struct irq_result {
				u64 irq_cnt;
				u64 softirq_cnt[XM_NR_SOFTIRQS];
				////u64 softirq_cnt_d[XM_NR_SOFTIRQS];

				u64 irq_run_total;
				u64 sortirq_run_total[XM_NR_SOFTIRQS];
				////u64 sortirq_run_total_d[XM_NR_SOFTIRQS];

				struct irq_runtime max_irq;
				struct softirq_runtime max_softirq;

		} irq_result;

		struct radix_tree_root irq_tree;
	} irq_stats;

	struct xm_irq_trace {
		struct {
			int irq;
			s64 start_time;
		} irq;
		
		struct {
			int sirq;
			s64 start_time;
		} softirq;

		struct {
			s64 start_time;
		} timer;

		struct {
			unsigned long irq_count;
			unsigned long irq_runs;
			unsigned long sirq_count[XM_NR_SOFTIRQS];
			unsigned long sirq_runs[XM_NR_SOFTIRQS];
			unsigned long timer_count;
			unsigned long timer_runs;
		} sum;
	} irq_trace;

	struct sys_delay_detail sys_delay_detail;

	struct {
		u64 start_time;
		/////unsigned long count[NR_syscalls_virt];
		/////unsigned long cost[NR_syscalls_virt];
		////struct sys_cost_detail detail;
	} sys_cost;

	////struct {
	////	struct kprobe_detail kprobe_detail;
	////	struct kprobe_raw_stack_detail kprobe_raw_stack_detail;
	////	unsigned int sample_step;
	////} kprobe;
};

struct xm_trace_settings {
	unsigned int activated;
	int threshold_ms;
};

extern struct xm_percpu_context *xm_percpu_context[NR_CPUS];

static inline struct xm_percpu_context * get_percpu_context_cpu(int cpu)
{
	if (cpu >= num_possible_cpus())
		return NULL;
	
	return xm_percpu_context[cpu];
}

static inline struct xm_percpu_context * get_percpu_context(void)
{
	return get_percpu_context_cpu(smp_processor_id());
}

struct alive_task_desc {
	struct task_struct *task;
	ktime_t sched_in;
	ktime_t sched_out;
	unsigned long id;
	struct rb_node rb_node;
};

////////////////////////////////////////////////////////////
#define MAX_TRACE_ENTRIES		    (512)
#define OVER_FLOW_LEN               32
#define MAX_STACE_TRACE_ENTRIES		(32)

struct stack_entry {
	unsigned int nr_entries;
	unsigned long *entries;
};

struct stack_entry_task {
	unsigned int nr_entries;
	unsigned long entries[BACKTRACE_DEPTH];  //BACKTRACE_DEPTH=30
};

struct xm_stack_trace {
	////u64 last_timestamp;
	////struct task_struct *skip;

	unsigned int nr_stack_entries;
	unsigned int nr_entries;
	struct stack_entry stack_entries[MAX_STACE_TRACE_ENTRIES];
	unsigned long entries[MAX_TRACE_ENTRIES + OVER_FLOW_LEN];

	char curr_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	char parent_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	pid_t pids[MAX_STACE_TRACE_ENTRIES];
	int duration[MAX_STACE_TRACE_ENTRIES];
	int prio[MAX_STACE_TRACE_ENTRIES];
	////bool flag[MAX_STACE_TRACE_ENTRIES];
	u8 event_type[MAX_STACE_TRACE_ENTRIES];
	u8 grp_type[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];
};

struct xm_stack_trace_task {
	unsigned int nr_stack_entries;
	unsigned int nr_tasks;
	struct stack_entry_task stack_entries[MAX_STACE_TRACE_ENTRIES];

	char curr_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	char parent_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	pid_t pids[MAX_STACE_TRACE_ENTRIES];
	int duration[MAX_STACE_TRACE_ENTRIES];
	int prio[MAX_STACE_TRACE_ENTRIES];

	u8 event_type[MAX_STACE_TRACE_ENTRIES];
	u8 grp_type[MAX_STACE_TRACE_ENTRIES];

	////bool flag[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];
};

//////////////////////////////////////////////////////////////


struct proc_dir_entry;
extern struct proc_dir_entry *xiaomi_proc_mkdir(const char *name,
		struct proc_dir_entry *parent);

DECLARE_PER_CPU(struct softirq_runtime, softirq_runtime);

#define MAX_TEST_ORDER  4
extern int sysctl_alloc_cost[MAX_TEST_ORDER + 1];

extern void read_lock_alive_tasks(void);
extern void read_unlock_alive_tasks(void);
extern void write_lock_alive_tasks(void);
extern void write_unlock_alive_tasks(void);
extern struct alive_task_desc *find_alive_task(struct task_struct *task);
extern struct alive_task_desc *find_alive_task_alloc(struct task_struct *task);
extern struct alive_task_desc *takeout_alive_task(struct task_struct *task);
extern void inter_alive_tasks(int (*cb)(struct alive_task_desc *desc));
extern void cleanup_alive_tasks(void);

extern void xiaomi_print_process_chain_cmdline(int pre, struct task_struct *tsk);
extern unsigned int sysctl_debug_trace_printk;

#define debug_trace_printk(fmt, ...)		\
	do {							\
		if (sysctl_debug_trace_printk)			\
			xiaomi_trace_printk(fmt, ##__VA_ARGS__);	\
	} while (0)
											\

extern int sysctl_force_printk;

#define xiaomi_trace_printk(fmt, ...)      \
({                                              \
	if (sysctl_force_printk) {		\
		printk(KERN_DEBUG fmt, ##__VA_ARGS__);	\
	} else {									\
		printk(fmt, ##__VA_ARGS__);  		\
	}											\
})

#define hash_shift 8
#define hash_size (1 << hash_shift)
#define hash_mask (hash_size - 1)

#ifndef DECLARE_HASHTABLE
#define DECLARE_HASHTABLE(name, bits)                                   	\
	struct hlist_head name[1 << (bits)]
#endif

struct xm_stack_desc {
        atomic64_t hit_count;
		int alloc_count_orders[MAX_ORDER];
		unsigned long trace_buf[BACKTRACE_DEPTH];
        struct rb_node rb_node;
		struct list_head list;
};

struct xiaomi_stack_trace {
        struct rb_root stack_tree;
        spinlock_t tree_lock;
		struct list_head list;
};

void dump_cgroups(int pre);
void dump_cgroups_tsk(int pre, struct task_struct *tsk);
void xiaomi_cgroup_name(struct task_struct *tsk, char *buf, unsigned int count, int cgroup);
void xiaomi_comm_name(struct task_struct *tsk, char *buf, unsigned int count);
int xiaomi_get_task_type(struct task_struct *tsk);

struct xm_stack_desc *xiaomi_stack_desc_find_alloc(struct xiaomi_stack_trace *trace);
int xiaomi_dump_trace_stack(struct xiaomi_stack_trace *trace);
int xiaomi_printk_trace_stack(struct xiaomi_stack_trace *trace);
void xiaomi_init_trace_stack(struct xiaomi_stack_trace *trace);
void xiaomi_cleanup_trace_stack(struct xiaomi_stack_trace *trace);

////void irq_delay_timer(struct xm_percpu_context *context);
////void perf_timer(struct xm_percpu_context *context);

void xiaomi_hook_sys_enter(void);
void xiaomi_unhook_sys_enter(void);


#define NR_BATCH 5
static inline ktime_t __ktime_add_ms(const ktime_t kt, const u64 msec)
{
	return ktime_add_ns(kt, msec * NSEC_PER_MSEC);
}

static inline ktime_t __ktime_add_us(const ktime_t kt, const u64 msec)
{
	return ktime_add_ns(kt, msec * NSEC_PER_USEC);
}

static inline ktime_t __ms_to_ktime(u64 ms)
{
	return ms * NSEC_PER_MSEC;
}

static inline ktime_t __us_to_ktime(u64 ms)
{
	return ms * NSEC_PER_USEC;
}

DECLARE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);
#define xm_raw_rq()    raw_cpu_ptr(&runqueues)

static inline bool xm_single_task_running_by_cpu(int cpu)
{
    return cpu_rq(cpu)->nr_running == 1;
}

static inline bool xm_single_task_running(void)
{
    return xm_raw_rq()->nr_running == 1;
    ////return false;
}

static inline int xm_rq_running_tasks(void)
{
    return xm_raw_rq()->nr_running;
}

extern struct pt_regs *xm_get_irq_regs(void);
struct xm_percpu_context *get_percpu_context_cpu(int cpu);
struct xm_percpu_context *get_percpu_context(void);

extern int hook_vcpu_run_init(void);
extern void hook_vcpu_run_exit(void);

unsigned int ipstr2int(const char *ipstr);
char *int2ipstr(const unsigned int ip, char *ipstr, const unsigned int ip_str_len);
char *mac2str(const unsigned char *mac, char *mac_str, const unsigned int mac_str_len);
extern bool xm_get_task_cgrp(struct task_struct *task);

#define ORIG_PARAM1(__regs) ((__regs)->regs[0])
#define ORIG_PARAM2(__regs) ((__regs)->regs[1])
#define ORIG_PARAM3(__regs) ((__regs)->regs[2])
#define ORIG_PARAM4(__regs) ((__regs)->regs[3])
#define ORIG_PARAM5(__regs) ((__regs)->regs[4])
#define ORIG_PARAM6(__regs) ((__regs)->regs[5])
//#define RETURN_REG(__regs) ((__regs)->regs[0])

#define SYSCALL_PARAM1 ORIG_PARAM2
#define SYSCALL_PARAM2 ORIG_PARAM3
#define SYSCALL_PARAM3 ORIG_PARAM4
#define SYSCALL_PARAM4 ORIG_PARAM5
#define SYSCALL_PARAM5 ORIG_PARAM6

#define SYSCALL_NO(regs) ((regs)->syscallno)

#ifndef XIAOMI_LINUX_PATH
#define XIAOMI_PROC_TRACE       "mi_kernel"
#define PROC_TRACE_CPU_UTIL     "mi_kernel/trace_cpu_util"
#define PROC_TRACE_IRQ_OFF     "mi_kernel/trace_irqoff"
#define PROC_TRACE_IRQSTAT     "mi_kernel/trace_irqstat"
#define PROC_TRACE_IRQ_HANDLER   "mi_kernel/trace_irqhandler"
#define PROC_TRACE_MUTEX        "mi_kernel/trace_mutex"
#define PROC_TRACE_PREEMPT_NAME  "mi_kernel/trace_preemptoff"
#define PROC_TRACE_RT_NAME	 "mi_kernel/trace_rt"
#define PROC_TRACE_RWSEM      "mi_kernel/trace_rwsem"
#define PROC_TRACE_SYS_COST   "mi_kernel/trace_sys_cost"
#define PROC_TRACE_SYS_DELAY   "mi_kernel/trace_sys_delay"
#define XM_LOCKING_DIRNAME		"mi_kernel/trace_locking"
#define PROC_TRACE_MEM  "mi_kernel/trace_mem"
#define PROC_TRACE_SCHED  "mi_kernel/trace_sched"
#define PROC_TRACE_BINDER  "mi_kernel/trace_binder"
#define PROC_TRACE_TEST_DIR   "mi_kernel/test_dir"
#define PROC_TRACE_EVENT   "mi_kernel/trace_event"
#define PROC_TRACE_IO  "mi_kernel/trace_io"
#else
#endif

#include <linux/atomic.h>

#define DEFINE_PROC_ATTRIBUTE(name, __write)				\
	static int name##_open(struct inode *inode, struct file *file)	\
	{								\
		return single_open(file, name##_show, inode->i_private/*PDE_DATA(inode)*/);	\
	}								\
									\
	static const struct proc_ops name##_fops = {		\
		.proc_open		= name##_open,				\
		.proc_read		= seq_read,				\
		.proc_write		= name##_write,				\
		.proc_lseek		= seq_lseek,				\
		.proc_release	= single_release,			\
	}

#define DEFINE_PROC_ATTRIBUTE_RW(name)					\
	static ssize_t name##_write(struct file *file,			\
				    const char __user *buf,		\
				    size_t count, loff_t *ppos)		\
	{								\
		return name##_store(file_inode(file)->i_private/*PDE_DATA(file_inode(file))*/, buf,	\
				    count);				\
	}								\
	DEFINE_PROC_ATTRIBUTE(name, name##_write)

#define DEFINE_PROC_ATTRIBUTE_RO(name)	\
	DEFINE_PROC_ATTRIBUTE(name, NULL)

int xiaomi_copy_stack_frame(struct task_struct *tsk,
	const void __user *fp,
	void *frame,
	unsigned int size);

#define synchronize_sched synchronize_rcu
static inline void do_xm_gettimeofday(struct xm_timespec *tv)
{
	struct timespec64 ts;
	ktime_get_real_ts64(&ts);

	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
}

void xiaomi_task_brief(struct task_struct *tsk, struct xm_task_detail *detail);
void printk_task_brief(struct xm_task_detail *detail);
void xm_store_stack_trace(struct pt_regs *regs, struct stack_entry *stack_entry, unsigned long *entries, unsigned int max_entries, int skip);
bool xm_stack_trace_record_for_task(struct task_struct* tsk, struct xm_stack_trace_task *stack_trace, struct pt_regs *regs, int duration);
void get_kernel_bt(struct task_struct *tsk, int delta, char* name);

void xiaomi_task_kern_stack(struct task_struct *tsk, struct xm_kern_stack_detail *detail);
void xiaomi_task_user_stack(struct task_struct *tsk, struct xm_user_stack_detail *detail);
void printk_task_user_stack(struct xm_user_stack_detail *detail);
void xiaomi_task_raw_stack(struct task_struct *tsk, struct xm_raw_stack_detail *detail);

void cb_sys_enter_run_trace(void *__data, struct pt_regs *regs, long id);
void cb_sys_enter_sys_delay(void *__data, struct pt_regs *regs, long id);
void cb_sys_enter_sys_cost(void *__data, struct pt_regs *regs, long id);

int str_to_cpumask(char *cpus, struct cpumask *cpumask);
void cpumask_to_str(struct cpumask *cpumask, char *buf, int len);
int str_to_bitmaps(char *bits, unsigned long *bitmap, int nr);
void bitmap_to_str(unsigned long *bitmap, int nr, char *buf, int len);

int xiaomi_sys_cost_init(void);
void xiaomi_sys_cost_exit(void);
void record_dump_cmd(char *module);
int activate_rw_sem(void);
int deactivate_rw_sem(void);
int xiaomi_rw_sem_init(void);
void xiaomi_rw_sem_exit(void);
bool stack_trace_record_event(u8 grp_type, u8 event_type, int delta, struct task_struct *tsk);

#if IS_ENABLED(CONFIG_METIS)
extern bool mi_vip_task(struct task_struct *p);
extern bool mi_link_vip_task(struct task_struct *tsk);
#else
static inline bool mi_vip_task(struct task_struct *p) { return false; }
static inline bool mi_link_vip_task(struct task_struct *tsk) { return false; }
#endif

static inline bool is_mi_vip_task(struct task_struct* p)
{
    if (mi_vip_task(p) || mi_link_vip_task(p))
    {
        return true;
    }
    return false;
}

static inline bool mi_rt_task(struct task_struct *tsk)
{
	return (tsk->prio < MAX_RT_PRIO);
}

static inline bool is_mi_ux_task(struct task_struct* p)
{
    if (mi_vip_task(p) || mi_link_vip_task(p) || mi_rt_task(p))
    {
        return true;
    }
    return false;
}

static inline bool mi_normal_task(struct task_struct *tsk)
{
	return (tsk->prio >= MAX_RT_PRIO);
}
#endif /* __XM_INTERNAL_H */

