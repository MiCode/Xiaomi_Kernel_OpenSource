#ifndef __KTRACE_H__
#define __KTRACE_H__

#define KTRACE_SCHED_BLOCK_PRINT_NS 300000000

/*
 * sed -i 's/^\([^\[]*\[[0-9]\{3\}\] [^\s]\{4\} \s*[0-9]\+\.[0-9]\+:\) \([^:]*:\) \(tracing_mark_write: .*\)/\1 \3/g' <trace>.html
 */
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

int __init ktrace_sched_init(struct dentry *parent);
int ktrace_sched_match_pid(pid_t pid);
int ktrace_sched_is_critical_pid(pid_t pid);
int ktrace_is_camera_provider(pid_t tgid);

/* Public APIs */
void ktrace_event_add_slowpath(u64 time_stamp, int order, u64 time);

void ktrace_event_add_binder_transaction(u64 time_stamp, u64 time);

void ktrace_event_add_sched_block(u64 time_stamp,
											u64 elapse,
											pid_t pid,
											void *pc);
void ktrace_event_add_sched_preempt(u64 time_stamp,
											u64 elapse,
											pid_t pid);
void ktrace_event_add_sched_wait(u64 time_stamp,
											u64 elapse,
											pid_t pid);

void ktrace_event_add_cpufreq_mitigation(u64 time_stamp,
					pid_t pid, unsigned int cpu, unsigned int target_freq, unsigned int max);

void ktrace_event_add_cpufreq_unpluged(u64 time_stamp,
					pid_t pid, unsigned int cpu, unsigned int target_freq, unsigned int max);

void ktrace_set_cpufreq_mitigated(char *comm, unsigned int cpu,
				const struct cpumask *related_cpus, unsigned int max);

void ktrace_procload_update_history(struct task_struct *p, u32 runtime, int samples, u32 scale_runtime);

void ktrace_log(const char *fmt, ...);

static inline int ktrace_event_slowpath_threshold(void)
{
        return 50000000; /* ns */
}

#else

static inline int ktrace_sched_match_pid(pid_t pid) { return 0; }

static inline int ktrace_sched_is_critical_pid(pid_t pid) {return 0;}

static inline int ktrace_is_camera_provider(pid_t tgid) { return 0;}

static inline void ktrace_event_add_slowpath(u64 time_stamp, int order, u64 time) {}

static inline void ktrace_event_add_binder_transaction(u64 time_stamp, u64 time) {}

static inline void ktrace_event_add_sched_block(u64 time_stamp,
											u64 elapse,
											pid_t pid,
											void *pc) {}
static inline void ktrace_event_add_sched_preempt(u64 time_stamp,
											u64 elapse,
											pid_t pid) {}
static inline void ktrace_event_add_sched_wait(u64 time_stamp,
											u64 elapse,
											pid_t pid) {}

static inline void ktrace_event_add_cpufreq_mitigation(u64 time_stamp,
					pid_t pid, unsigned int cpu, unsigned int target_freq, unsigned int max) {}

static inline void ktrace_event_add_cpufreq_unpluged(u64 time_stamp,
					pid_t pid, unsigned int cpu, unsigned int target_freq, unsigned int max) {}

static inline void ktrace_set_cpufreq_mitigated(char *comm, unsigned int cpu,
				const struct cpumask *related_cpus, unsigned int max){}

static inline void ktrace_procload_update_history(struct task_struct *p, u32 runtime, int samples, u32 scale_runtime){}

static inline int ktrace_event_slowpath_threshold(void)
{
        return 50000000; /* ns */
}

#endif

#endif
