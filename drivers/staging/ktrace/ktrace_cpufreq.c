#define pr_fmt(fmt)  "ktrace : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/preempt.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/ktrace.h>
#include <linux/hashtable.h>
#include <linux/list.h>
#include <linux/sched.h>

#define PROC_HASH_BITS	10
DECLARE_HASHTABLE(proc_hash, PROC_HASH_BITS);

#define CPUFREQ_ITEMS_READ_MAX DEFAULT_ITEMS_PER_READ
#define KTRACE_CPUFREQ_MITIGATED_NR 2

struct cpufreq_entry {
	s64 time_stamp_ns;
	unsigned char type;
	pid_t pid;
	unsigned int cpu;
	unsigned int target_freq;
	unsigned int max;
};

struct cpufreq_mitigated {
	bool ismitigated;
	unsigned int cpu;
	unsigned int max;
	char comm[TASK_COMM_LEN];
};

struct proc_entry {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	unsigned long long load;
	struct hlist_node hash;
};

struct ktrace_cpufreq {
	struct ktrace_queue *q;
	struct dentry *dir;

	void *buf;
	int entry_size;
	int max;
	struct cpufreq_mitigated mitigated[KTRACE_CPUFREQ_MITIGATED_NR];
};

static struct ktrace_cpufreq __cf;

static const char const *cpufreq_event_name[KTRACE_CPUFREQ_TYPE_NR] = {
	"sam_1",
	"sam_2",
	"sam_3",
	"sam_4",
	"sam_5",
	"sam_6",
	"sam_7",
	"sam_8",
	"sam_9",
	"sam_10",

	"mitigation",
	"unpluged",
};

static int cpufreq_show_entry(struct seq_file *m, void *entry, bool debug)
{
	struct cpufreq_entry *e = (struct cpufreq_entry *)entry;
	int ret;

	if (e->type == KTRACE_CPUFREQ_TYPE_MITIGATION) {
		if (debug) {
			ret = seq_printf(m, "%lld %5d <%s> %d ms %u %u %u\n",
					e->time_stamp_ns, e->pid,
					cpufreq_event_name[e->type], -1,
					e->cpu, e->target_freq, e->max);
		} else {
			ret = seq_printf(m, "%lld %5d <%s> %d ms %u %u %u\n",
					e->time_stamp_ns, e->pid,
					cpufreq_event_name[e->type], -1,
					e->cpu, e->target_freq, e->max);
		}
	} else {
		ret = seq_printf(m, "%lld %5d <%s> %d ms %u\n",
				e->time_stamp_ns, e->pid,
				cpufreq_event_name[e->type], -1,
				e->cpu);
	}

	return ret;
}

KTRACE_QUEUE_RO_SINGLE(cpufreq);

void ktrace_add_cpufreq_event(unsigned char type, pid_t pid, u64 time_stamp,
		unsigned int cpu, unsigned int target_freq, unsigned int max)
{
	struct ktrace_cpufreq *cpufreq = &__cf;
	struct ktrace_queue *q = cpufreq->q;
	struct cpufreq_entry e;

	if (unlikely(!cpufreq->buf))
		return;

	if (!ktrace_sched_match_pid(pid))
		return;

	e.time_stamp_ns = time_stamp;
	e.type = type;
	e.pid = pid;
	e.cpu = cpu;
	e.target_freq = target_freq;
	e.max = max;

	ktrace_tryadd_queue(q, &e);
}

void ktrace_cpufreq_set_mitigated(char *comm, unsigned int cpu,
		const struct cpumask *related_cpus, unsigned int max)
{
	struct ktrace_cpufreq *cpufreq = &__cf;
	int i;

	for (i = 0; i < KTRACE_CPUFREQ_MITIGATED_NR; i++) {
		struct cpufreq_mitigated *cm = &cpufreq->mitigated[i];

		if (cm->cpu == -1  || cpumask_test_cpu(cm->cpu, related_cpus) != 0) {
			if (max == 0) {

				cm->ismitigated = false;
				cm->max = UINT_MAX;
			} else if (max < cm->max) {


				cm->ismitigated = true;
				cm->cpu = cpu;
				cm->max = max;
				memcpy(cm->comm, comm, TASK_COMM_LEN);
			}
			break;
		}
	}
}

static int cpufreq_mitigated_show(struct seq_file *m, void *v)
{
	struct ktrace_cpufreq *cpufreq = (struct ktrace_cpufreq *)m->private;
	int i;

	for (i = 0; i < KTRACE_CPUFREQ_MITIGATED_NR; i++) {
		struct cpufreq_mitigated *cm = &cpufreq->mitigated[i];
		if (cm->ismitigated) {
			seq_printf(m, "%u %u <%s>\n",
					cm->cpu, cm->max, cm->comm);
		}
	}

	return 0;
}

KTRACE_ENTRY_RO_SINGLE(cpufreq_mitigated);

static void __init cpufreq_mitigated_init(struct ktrace_cpufreq *cpufreq)
{
	int i;

	for (i = 0; i < KTRACE_CPUFREQ_MITIGATED_NR; i++) {
		struct cpufreq_mitigated *cm = &cpufreq->mitigated[i];
		cm->ismitigated = false;
		cm->cpu = -1;
		cm->max = UINT_MAX;
	}
}

void ktrace_cpufreq_update_history(struct task_struct *p, u32 runtime, int samples, u32 scale_runtime)
{
	u32 load = runtime;

	if (samples)
		load += samples * scale_runtime;

	p->ravg.proc_load += load;
}

static inline struct proc_entry *find_proc_entry(pid_t pid)
{
	struct proc_entry *proc_entry;
	hash_for_each_possible(proc_hash, proc_entry, hash, pid) {
		if (proc_entry->pid == pid)
			return proc_entry;
	}
	return NULL;
}

static inline struct proc_entry *find_or_register_proc(pid_t pid, char *comm)
{
	struct proc_entry *proc_entry;

	proc_entry = find_proc_entry(pid);
	if (proc_entry)
		return proc_entry;

	proc_entry = kzalloc(sizeof(struct proc_entry), GFP_ATOMIC);
	if (!proc_entry)
		return NULL;

	proc_entry->pid = pid;
	memcpy(proc_entry->comm, comm, TASK_COMM_LEN);
	proc_entry->load = 0;

	hash_add(proc_hash, &proc_entry->hash, pid);

	return proc_entry;
}

static int cpufreq_procload_show(struct seq_file *m, void *v)
{
	struct proc_entry *proc_entry;
	struct task_struct *task, *temp;
	struct hlist_node *tmp;
	unsigned long bkt;

	read_lock(&tasklist_lock);
	do_each_thread(temp, task) {
		proc_entry = find_or_register_proc(task->tgid, task->group_leader->comm);

		if (!proc_entry) {
			pr_err("%s: failed to find the proc_entry for pid %d\n",
					__func__, task->tgid);
			read_unlock(&tasklist_lock);
			return -ENOMEM;
		}

		proc_entry->load += task->ravg.proc_load;

	} while_each_thread(temp, task);
	read_unlock(&tasklist_lock);

	hash_for_each_safe(proc_hash, bkt, tmp, proc_entry, hash) {
		if (proc_entry->load > 0) {
			seq_printf(m, "%d %s %llu\n", proc_entry->pid,
					proc_entry->comm, proc_entry->load);
		}
		hash_del(&proc_entry->hash);
		kfree(proc_entry);
	}

	return 0;

}

KTRACE_ENTRY_RO_SINGLE(cpufreq_procload);

int __init ktrace_cpufreq_init(struct dentry *dir, struct ktrace_queue *q)
{
	struct ktrace_cpufreq *cpufreq = &__cf;
	void *buf;

	hash_init(proc_hash);

	BUILD_BUG_ON(sizeof(cpufreq_event_name) / sizeof(cpufreq_event_name[0])
			!= KTRACE_CPUFREQ_TYPE_NR);

	buf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!buf)
		return -ENOMEM;
	cpufreq->q = q;

	cpufreq->entry_size = sizeof(struct cpufreq_entry);
	cpufreq->max = (PAGE_SIZE << 1) / cpufreq->entry_size;

	cpufreq_mitigated_init(cpufreq);

	cpufreq->dir = debugfs_create_dir("cpufreq", dir);

	ktrace_init_queue(q, cpufreq->dir, cpufreq, buf, cpufreq->entry_size, cpufreq->max,
			CPUFREQ_ITEMS_READ_MAX,
			cpufreq_show_entry);

	if (cpufreq->dir) {
		debugfs_create_file("trace",
				S_IFREG | S_IRUGO,
				cpufreq->dir,
				q,
				&cpufreq_fops);

		debugfs_create_file("mitigated",
				S_IFREG | S_IRUGO,
				cpufreq->dir,
				cpufreq,
				&cpufreq_mitigated_fops);

		debugfs_create_file("procload",
				S_IFREG | S_IRUGO,
				cpufreq->dir,
				cpufreq,
				&cpufreq_procload_fops);
	}

	smp_mb();
	cpufreq->buf = buf;

	pr_info("cpufreq ktrace init OK\n");

	return 0;
}
