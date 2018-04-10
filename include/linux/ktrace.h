#ifndef __KTRACE_H__
#define __KTRACE_H__

#include <linux/ktime.h>
#include <linux/cpumask.h>

#define US_TO_NS(usec)		((usec) * 1000)
#define MS_TO_US(msec)		((msec) * 1000)

#define QUEUE_NEXT(v, max) (((v) + 1) % (max))
#define QUEUE_ADD(v, a, max) (((v) + (a)) % (max))
#define QUEUE_EMPTY(tail, head) ((tail) == (head))
#define QUEUE_FULL(tail, head, max) ((((head) + 1) % (max)) == tail)

#define DEFAULT_ITEMS_PER_READ  INT_MAX

#define KTRACE_SCHED_BLOCK_PRINT_NS 5000000000
#define KTRACE_MM_SLOWPATH_THRESHOLD 30000000  /* 30 ms */

enum {
	KTRACE_MM_TYPE_SAMPLE1 = 0,
	KTRACE_MM_TYPE_SAMPLE2,
	KTRACE_MM_TYPE_SAMPLE3,
	KTRACE_MM_TYPE_SAMPLE4,
	KTRACE_MM_TYPE_SAMPLE5,
	KTRACE_MM_TYPE_SAMPLE6,
	KTRACE_MM_TYPE_SAMPLE7,
	KTRACE_MM_TYPE_SAMPLE8,
	KTRACE_MM_TYPE_SAMPLE9,
	KTRACE_MM_TYPE_SAMPLE10,

	KTRACE_MM_TYPE_SLOW_PATH,
	KTRACE_MM_TYPE_COMPACT,
	KTRACE_MM_TYPE_DIRECT_RECLAIM,
	KTRACE_MM_TYPE_NR,
};

enum {
	KTRACE_SCHED_TYPE_SAMPLE1 = 0,
	KTRACE_SCHED_TYPE_SAMPLE2,
	KTRACE_SCHED_TYPE_SAMPLE3,
	KTRACE_SCHED_TYPE_SAMPLE4,
	KTRACE_SCHED_TYPE_SAMPLE5,
	KTRACE_SCHED_TYPE_SAMPLE6,
	KTRACE_SCHED_TYPE_SAMPLE7,
	KTRACE_SCHED_TYPE_SAMPLE8,
	KTRACE_SCHED_TYPE_SAMPLE9,
	KTRACE_SCHED_TYPE_SAMPLE10,

	KTRACE_SCHED_TYPE_PREEMPT,
	KTRACE_SCHED_TYPE_WAIT,
	KTRACE_SCHED_TYPE_BLOCK,
	KTRACE_SCHED_TYPE_NR,
};

enum {
	KTRACE_BINDER_TYPE_SAMPLE1 = 0,
	KTRACE_BINDER_TYPE_SAMPLE2,
	KTRACE_BINDER_TYPE_SAMPLE3,
	KTRACE_BINDER_TYPE_SAMPLE4,
	KTRACE_BINDER_TYPE_SAMPLE5,
	KTRACE_BINDER_TYPE_SAMPLE6,
	KTRACE_BINDER_TYPE_SAMPLE7,
	KTRACE_BINDER_TYPE_SAMPLE8,
	KTRACE_BINDER_TYPE_SAMPLE9,
	KTRACE_BINDER_TYPE_SAMPLE10,

	KTRACE_BINDER_TYPE_TRANSACTION,
	KTRACE_BINDER_TYPE_NR,
};

enum {
	KTRACE_CPUFREQ_TYPE_SAMPLE1 = 0,
	KTRACE_CPUFREQ_TYPE_SAMPLE2,
	KTRACE_CPUFREQ_TYPE_SAMPLE3,
	KTRACE_CPUFREQ_TYPE_SAMPLE4,
	KTRACE_CPUFREQ_TYPE_SAMPLE5,
	KTRACE_CPUFREQ_TYPE_SAMPLE6,
	KTRACE_CPUFREQ_TYPE_SAMPLE7,
	KTRACE_CPUFREQ_TYPE_SAMPLE8,
	KTRACE_CPUFREQ_TYPE_SAMPLE9,
	KTRACE_CPUFREQ_TYPE_SAMPLE10,

	KTRACE_CPUFREQ_TYPE_MITIGATION,
	KTRACE_CPUFREQ_TYPE_UNPLUGED,
	KTRACE_CPUFREQ_TYPE_NR,
};

enum {
	QUEUE_MM = 0,
	QUEUE_STR,
	QUEUE_SCHED,
	QUEUE_BINDER,
	QUEUE_CPUFREQ,
	QUEUE_NR
};

enum {
	KTRACE_EVENT_1 = 0,
	KTRACE_EVENT_2,
	KTRACE_EVENT_3,
	KTRACE_EVENT_4,
	KTRACE_EVENT_5,
	KTRACE_EVENT_6,
	KTRACE_EVENT_7,
	KTRACE_EVENT_8,
	KTRACE_EVENT_9,
	KTRACE_EVENT_10,

	KTRACE_EVENT_11,
	KTRACE_EVENT_12,
	KTRACE_EVENT_13,
	KTRACE_EVENT_14,
	KTRACE_EVENT_15,
	KTRACE_EVENT_16,
	KTRACE_EVENT_17,
	KTRACE_EVENT_18,
	KTRACE_EVENT_19,
	KTRACE_EVENT_20,

	KTRACE_EVENT_NR
};

struct ktrace_queue {
	spinlock_t lock;

	void *buf;
	int entry_size;
	int max;
	int head;
	int tail;

	int read_cnt;
	int items_per_read;

	int enable_debug;

	int (*show_entry)(struct seq_file *m, void *entry, bool debug);

	void *priv;
};

struct ktrace {
	struct dentry *dir;

	struct ktrace_queue queue[QUEUE_NR];

	/* str ktrace */
	spinlock_t str_lock;
	int str_queue_head;
	int str_queue_tail;
	int str_queue_max;
	char *str_buf;

	int str_enable_debug;
};

/* use single_open() */
#define KTRACE_ENTRY_RO_SINGLE(name) \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, name##_show, inode->i_private); \
} \
\
static const struct file_operations name##_fops = { \
	.owner = THIS_MODULE, \
	.open = name##_open, \
	.read = seq_read, \
	.llseek = no_llseek, \
	.release = single_release, \
};

/* use single_open() */
#define KTRACE_QUEUE_RO_SINGLE(name) \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, ktrace_q_single_show, inode->i_private); \
} \
\
static const struct file_operations name##_fops = { \
	.owner = THIS_MODULE, \
	.open = name##_open, \
	.read = seq_read, \
	.llseek = no_llseek, \
	.release = single_release, \
};


/* use seq_open() */
#define KTRACE_QUEUE_RO(name) \
\
static const struct seq_operations name##_seq_ops = { \
	.start =  ktrace_q_start, \
	.stop =  ktrace_q_stop, \
	.next =  ktrace_q_next, \
	.show =  ktrace_q_show, \
}; \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	int ret; \
\
	ret = seq_open(file, &name##_seq_ops); \
	if (!ret) { \
		struct seq_file *m = file->private_data; \
		m->private = inode->i_private; \
	} \
	return ret; \
} \
\
static const struct file_operations name##_fops = { \
	.owner = THIS_MODULE, \
	.open = name##_open, \
	.read = seq_read, \
	.llseek = no_llseek, \
	.release = seq_release, \
};

#define KTRACE_BEGIN(name) trace_printk("tracing_mark_write: B|%d|%s\n", current->tgid, name)
#define KTRACE_END() trace_printk("tracing_mark_write: E\n")
#define KTRACE_FUNC() KTRACE_BEGIN(__func__)
#define KTRACE_INT(name, value) trace_printk("tracing_mark_write: C|%d|%s|%d\n", current->tgid, name, (int)(value))
#define KTRACE_BEGIN_MSG(fmt...) \
	do { \
		char buf[64]; \
		snprintf(buf, sizeof(buf) - 1, fmt); \
		KTRACE_BEGIN(buf); \
	} while (0);

#ifdef CONFIG_KTRACE

struct ktrace *get_ktrace(void);
int __init ktrace_mm_init(struct dentry *dir, struct ktrace_queue *q);
int __init ktrace_print_init(struct dentry *dir, struct ktrace_queue *q);
int __init ktrace_sched_init(struct dentry *dir, struct ktrace_queue *q);
int __init ktrace_binder_init(struct dentry *dir, struct ktrace_queue *q);
int __init ktrace_cpufreq_init(struct dentry *dir, struct ktrace_queue *q);
int __init ktrace_event_init(struct dentry *dir);

void ktrace_add_mm_event(unsigned char type, u64 time_stamp, u64 delta);
void ktrace_add_sched_event(unsigned char type, pid_t pid, u64 time_stamp, u64 delta, void *pc);
void ktrace_add_binder_event(unsigned char type, u64 time_stamp, u64 delta);
int ktrace_sched_match_pid(pid_t pid);
void ktrace_add_cpufreq_event(unsigned char type, pid_t pid, u64 time_stamp,
		unsigned int cpu, unsigned int target_freq, unsigned int max);
void ktrace_cpufreq_set_mitigated(char *comm, unsigned int cpu,
		const struct cpumask *related_cpus, unsigned int max);
void ktrace_cpufreq_update_history(struct task_struct *p, u32 runtime, int samples, u32 scale_runtime);
void ktrace_print(const char *fmt, ...);

void *ktrace_q_start(struct seq_file *m, loff_t *pos);
void *ktrace_q_next(struct seq_file *m, void *v, loff_t *pos);
void ktrace_q_stop(struct seq_file *m, void *v);
int ktrace_q_show(struct seq_file *m, void *v);
int ktrace_q_single_show(struct seq_file *m, void *v);
void ktrace_init_queue(struct ktrace_queue *q, struct dentry *dir,
		void *priv, void *buf, int entry_size, int max, int items_per_read,
		int(*show_entry)(struct seq_file *, void *, bool));
void ktrace_tryadd_queue(struct ktrace_queue *q, void *entry);
void ktrace_add_queue_multi(struct ktrace_queue *q, void *entry, int num);
void ktrace_reset_queue(struct ktrace_queue *q);

void ktrace_event_inc(unsigned char type);
void ktrace_event_dec(unsigned char type);
void ktrace_event_add(unsigned char type, int i);
void ktrace_event_sub(unsigned char type, int i);
#else
static inline void ktrace_add_mm_event(unsigned char type, u64 time_stamp, u64 delta) {}
static inline void ktrace_print(const char *fmt, ...) {}
static inline void ktrace_add_sched_event(unsigned char type, pid_t pid, u64 time_stamp, u64 delta, void *pc) {}
static inline void ktrace_add_binder_event(unsigned char type, u64 time_stamp, u64 delta) {}
static inline int ktrace_sched_match_pid(pid_t pid) { return 0; }
static inline void ktrace_add_cpufreq_event(unsigned char type, pid_t pid, u64 time_stamp,
		unsigned int cpu, unsigned int target_freq, unsigned int max) {}
static inline void ktrace_cpufreq_set_mitigated(char *comm, unsigned int cpu,
		const struct cpumask *related_cpus, unsigned int max){}
static inline void ktrace_cpufreq_update_history(struct task_struct *p, u32 runtime, int samples, u32 scale_runtime) {}



static inline void ktrace_event_inc(unsigned char type) {}
static inline void ktrace_event_dec(unsigned char type) {}
static inline void ktrace_event_add(unsigned char type, int i) {}
static inline void ktrace_event_sub(unsigned char type, int i) {}

#endif

#endif
