/*
 * Generic helpers for smp ipi calls
 *
 * (C) Jens Axboe <jens.axboe@oracle.com> 2008
 */
#include <linux/irq_work.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#ifdef CONFIG_PROFILE_CPU
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#endif

#include "smpboot.h"
#if defined(CONFIG_ARCH_MT6738)
#include "mt-plat/mt_devinfo.h"
#endif

enum {
	CSD_FLAG_LOCK		= 0x01,
	CSD_FLAG_WAIT		= 0x02,
};

struct call_function_data {
	struct call_single_data	__percpu *csd;
	cpumask_var_t		cpumask;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_function_data, cfd_data);

static DEFINE_PER_CPU_SHARED_ALIGNED(struct llist_head, call_single_queue);

static void flush_smp_call_function_queue(bool warn_cpu_offline);

static int
hotplug_cfd(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	struct call_function_data *cfd = &per_cpu(cfd_data, cpu);

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		if (!zalloc_cpumask_var_node(&cfd->cpumask, GFP_KERNEL,
				cpu_to_node(cpu)))
			return notifier_from_errno(-ENOMEM);
		cfd->csd = alloc_percpu(struct call_single_data);
		if (!cfd->csd) {
			free_cpumask_var(cfd->cpumask);
			return notifier_from_errno(-ENOMEM);
		}
		break;

#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		/* Fall-through to the CPU_DEAD[_FROZEN] case. */

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		free_cpumask_var(cfd->cpumask);
		free_percpu(cfd->csd);
		break;

	case CPU_DYING:
	case CPU_DYING_FROZEN:
		/*
		 * The IPIs for the smp-call-function callbacks queued by other
		 * CPUs might arrive late, either due to hardware latencies or
		 * because this CPU disabled interrupts (inside stop-machine)
		 * before the IPIs were sent. So flush out any pending callbacks
		 * explicitly (without waiting for the IPIs to arrive), to
		 * ensure that the outgoing CPU doesn't go offline with work
		 * still pending.
		 */
		flush_smp_call_function_queue(false);
		break;
#endif
	};

	return NOTIFY_OK;
}

static struct notifier_block hotplug_cfd_notifier = {
	.notifier_call		= hotplug_cfd,
};

void __init call_function_init(void)
{
	void *cpu = (void *)(long)smp_processor_id();
	int i;

	for_each_possible_cpu(i)
		init_llist_head(&per_cpu(call_single_queue, i));

	hotplug_cfd(&hotplug_cfd_notifier, CPU_UP_PREPARE, cpu);
	register_cpu_notifier(&hotplug_cfd_notifier);
}

/*
 * csd_lock/csd_unlock used to serialize access to per-cpu csd resources
 *
 * For non-synchronous ipi calls the csd can still be in use by the
 * previous function call. For multi-cpu calls its even more interesting
 * as we'll have to ensure no other cpu is observing our csd.
 */
static void csd_lock_wait(struct call_single_data *csd)
{
	while (csd->flags & CSD_FLAG_LOCK)
		cpu_relax();
}

static void csd_lock(struct call_single_data *csd)
{
	csd_lock_wait(csd);
	csd->flags |= CSD_FLAG_LOCK;

	/*
	 * prevent CPU from reordering the above assignment
	 * to ->flags with any subsequent assignments to other
	 * fields of the specified call_single_data structure:
	 */
	smp_mb();
}

static void csd_unlock(struct call_single_data *csd)
{
	WARN_ON((csd->flags & CSD_FLAG_WAIT) && !(csd->flags & CSD_FLAG_LOCK));

	/*
	 * ensure we're all done before releasing data:
	 */
	smp_mb();

	csd->flags &= ~CSD_FLAG_LOCK;
}

static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_single_data, csd_data);

/*
 * Insert a previously allocated call_single_data element
 * for execution on the given CPU. data must already have
 * ->func, ->info, and ->flags set.
 */
static int generic_exec_single(int cpu, struct call_single_data *csd,
			       smp_call_func_t func, void *info, int wait)
{
	struct call_single_data csd_stack = { .flags = 0 };
	unsigned long flags;


	if (cpu == smp_processor_id()) {
		local_irq_save(flags);
		func(info);
		local_irq_restore(flags);
		return 0;
	}


	if ((unsigned)cpu >= nr_cpu_ids || !cpu_online(cpu))
		return -ENXIO;


	if (!csd) {
		csd = &csd_stack;
		if (!wait)
			csd = this_cpu_ptr(&csd_data);
	}

	csd_lock(csd);

	csd->func = func;
	csd->info = info;

	if (wait)
		csd->flags |= CSD_FLAG_WAIT;

	/*
	 * The list addition should be visible before sending the IPI
	 * handler locks the list to pull the entry off it because of
	 * normal cache coherency rules implied by spinlocks.
	 *
	 * If IPIs can go out of order to the cache coherency protocol
	 * in an architecture, sufficient synchronisation should be added
	 * to arch code to make it appear to obey cache coherency WRT
	 * locking and barrier primitives. Generic code isn't really
	 * equipped to do the right thing...
	 */
	if (llist_add(&csd->llist, &per_cpu(call_single_queue, cpu)))
		arch_send_call_function_single_ipi(cpu);

	if (wait)
		csd_lock_wait(csd);

	return 0;
}

/**
 * generic_smp_call_function_single_interrupt - Execute SMP IPI callbacks
 *
 * Invoked by arch to handle an IPI for call function single.
 * Must be called with interrupts disabled.
 */
void generic_smp_call_function_single_interrupt(void)
{
	flush_smp_call_function_queue(true);
}

/**
 * flush_smp_call_function_queue - Flush pending smp-call-function callbacks
 *
 * @warn_cpu_offline: If set to 'true', warn if callbacks were queued on an
 *		      offline CPU. Skip this check if set to 'false'.
 *
 * Flush any pending smp-call-function callbacks queued on this CPU. This is
 * invoked by the generic IPI handler, as well as by a CPU about to go offline,
 * to ensure that all pending IPI callbacks are run before it goes completely
 * offline.
 *
 * Loop through the call_single_queue and run all the queued callbacks.
 * Must be called with interrupts disabled.
 */
static void flush_smp_call_function_queue(bool warn_cpu_offline)
{
	struct llist_head *head;
	struct llist_node *entry;
	struct call_single_data *csd, *csd_next;
	static bool warned;

	WARN_ON(!irqs_disabled());

	head = this_cpu_ptr(&call_single_queue);
	entry = llist_del_all(head);
	entry = llist_reverse_order(entry);

	/* There shouldn't be any pending callbacks on an offline CPU. */
	if (unlikely(warn_cpu_offline && !cpu_online(smp_processor_id()) &&
		     !warned && !llist_empty(head))) {
		warned = true;
		WARN(1, "IPI on offline CPU %d\n", smp_processor_id());

		/*
		 * We don't have to use the _safe() variant here
		 * because we are not invoking the IPI handlers yet.
		 */
		llist_for_each_entry(csd, entry, llist)
			pr_warn("IPI callback %pS sent to offline CPU\n",
				csd->func);
	}

	llist_for_each_entry_safe(csd, csd_next, entry, llist) {
		csd->func(csd->info);
		csd_unlock(csd);
	}

	/*
	 * Handle irq works queued remotely by irq_work_queue_on().
	 * Smp functions above are typically synchronous so they
	 * better run first since some other CPUs may be busy waiting
	 * for them.
	 */
	irq_work_run();
}

/*
 * smp_call_function_single - Run a function on a specific CPU
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait until function has completed on other CPUs.
 *
 * Returns 0 on success, else a negative status code.
 */
int smp_call_function_single(int cpu, smp_call_func_t func, void *info,
			     int wait)
{
	int this_cpu;
	int err;

	/*
	 * prevent preemption and reschedule on another processor,
	 * as well as CPU removal
	 */
	this_cpu = get_cpu();

	/*
	 * Can deadlock when called with interrupts disabled.
	 * We allow cpu's that are not yet online though, as no one else can
	 * send smp call function interrupt to this cpu and as such deadlocks
	 * can't happen.
	 */
	WARN_ON_ONCE(cpu_online(this_cpu) && irqs_disabled()
		     && !oops_in_progress);

	err = generic_exec_single(cpu, NULL, func, info, wait);

	put_cpu();

	return err;
}
EXPORT_SYMBOL(smp_call_function_single);

/**
 * smp_call_function_single_async(): Run an asynchronous function on a
 * 			         specific CPU.
 * @cpu: The CPU to run on.
 * @csd: Pre-allocated and setup data structure
 *
 * Like smp_call_function_single(), but the call is asynchonous and
 * can thus be done from contexts with disabled interrupts.
 *
 * The caller passes his own pre-allocated data structure
 * (ie: embedded in an object) and is responsible for synchronizing it
 * such that the IPIs performed on the @csd are strictly serialized.
 *
 * NOTE: Be careful, there is unfortunately no current debugging facility to
 * validate the correctness of this serialization.
 */
int smp_call_function_single_async(int cpu, struct call_single_data *csd)
{
	int err = 0;

	preempt_disable();
	err = generic_exec_single(cpu, csd, csd->func, csd->info, 0);
	preempt_enable();

	return err;
}
EXPORT_SYMBOL_GPL(smp_call_function_single_async);

/*
 * smp_call_function_any - Run a function on any of the given cpus
 * @mask: The mask of cpus it can run on.
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait until function has completed.
 *
 * Returns 0 on success, else a negative status code (if no cpus were online).
 *
 * Selection preference:
 *	1) current cpu if in @mask
 *	2) any cpu of current node if in @mask
 *	3) any other online cpu in @mask
 */
int smp_call_function_any(const struct cpumask *mask,
			  smp_call_func_t func, void *info, int wait)
{
	unsigned int cpu;
	const struct cpumask *nodemask;
	int ret;

	/* Try for same CPU (cheapest) */
	cpu = get_cpu();
	if (cpumask_test_cpu(cpu, mask))
		goto call;

	/* Try for same node. */
	nodemask = cpumask_of_node(cpu_to_node(cpu));
	for (cpu = cpumask_first_and(nodemask, mask); cpu < nr_cpu_ids;
	     cpu = cpumask_next_and(cpu, nodemask, mask)) {
		if (cpu_online(cpu))
			goto call;
	}

	/* Any online will do: smp_call_function_single handles nr_cpu_ids. */
	cpu = cpumask_any_and(mask, cpu_online_mask);
call:
	ret = smp_call_function_single(cpu, func, info, wait);
	put_cpu();
	return ret;
}
EXPORT_SYMBOL_GPL(smp_call_function_any);

/**
 * smp_call_function_many(): Run a function on a set of other CPUs.
 * @mask: The set of cpus to run on (only runs on online subset).
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait (atomically) until function has completed
 *        on other CPUs.
 *
 * If @wait is true, then returns once @func has returned.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler. Preemption
 * must be disabled when calling this function.
 */
void smp_call_function_many(const struct cpumask *mask,
			    smp_call_func_t func, void *info, bool wait)
{
	struct call_function_data *cfd;
	int cpu, next_cpu, this_cpu = smp_processor_id();

	/*
	 * Can deadlock when called with interrupts disabled.
	 * We allow cpu's that are not yet online though, as no one else can
	 * send smp call function interrupt to this cpu and as such deadlocks
	 * can't happen.
	 */
	WARN_ON_ONCE(cpu_online(this_cpu) && irqs_disabled()
		     && !oops_in_progress && !early_boot_irqs_disabled);

	/* Try to fastpath.  So, what's a CPU they want? Ignoring this one. */
	cpu = cpumask_first_and(mask, cpu_online_mask);
	if (cpu == this_cpu)
		cpu = cpumask_next_and(cpu, mask, cpu_online_mask);

	/* No online cpus?  We're done. */
	if (cpu >= nr_cpu_ids)
		return;

	/* Do we have another CPU which isn't us? */
	next_cpu = cpumask_next_and(cpu, mask, cpu_online_mask);
	if (next_cpu == this_cpu)
		next_cpu = cpumask_next_and(next_cpu, mask, cpu_online_mask);

	/* Fastpath: do that cpu by itself. */
	if (next_cpu >= nr_cpu_ids) {
		smp_call_function_single(cpu, func, info, wait);
		return;
	}

	cfd = this_cpu_ptr(&cfd_data);

	cpumask_and(cfd->cpumask, mask, cpu_online_mask);
	cpumask_clear_cpu(this_cpu, cfd->cpumask);

	/* Some callers race with other cpus changing the passed mask */
	if (unlikely(!cpumask_weight(cfd->cpumask)))
		return;

	for_each_cpu(cpu, cfd->cpumask) {
		struct call_single_data *csd = per_cpu_ptr(cfd->csd, cpu);

		csd_lock(csd);
		csd->func = func;
		csd->info = info;
		llist_add(&csd->llist, &per_cpu(call_single_queue, cpu));
	}

	/* Send a message to all CPUs in the map */
	arch_send_call_function_ipi_mask(cfd->cpumask);

	if (wait) {
		for_each_cpu(cpu, cfd->cpumask) {
			struct call_single_data *csd;

			csd = per_cpu_ptr(cfd->csd, cpu);
			csd_lock_wait(csd);
		}
	}
}
EXPORT_SYMBOL(smp_call_function_many);

/**
 * smp_call_function(): Run a function on all other CPUs.
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait (atomically) until function has completed
 *        on other CPUs.
 *
 * Returns 0.
 *
 * If @wait is true, then returns once @func has returned; otherwise
 * it returns just before the target cpu calls @func.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
int smp_call_function(smp_call_func_t func, void *info, int wait)
{
	preempt_disable();
	smp_call_function_many(cpu_online_mask, func, info, wait);
	preempt_enable();

	return 0;
}
EXPORT_SYMBOL(smp_call_function);

/* Setup configured maximum number of CPUs to activate */
unsigned int setup_max_cpus = NR_CPUS;
EXPORT_SYMBOL(setup_max_cpus);


/*
 * Setup routine for controlling SMP activation
 *
 * Command-line option of "nosmp" or "maxcpus=0" will disable SMP
 * activation entirely (the MPS table probe still happens, though).
 *
 * Command-line option of "maxcpus=<NUM>", where <NUM> is an integer
 * greater than 0, limits the maximum number of CPUs activated in
 * SMP mode to <NUM>.
 */

void __weak arch_disable_smp_support(void) { }

static int __init nosmp(char *str)
{
	setup_max_cpus = 0;
	arch_disable_smp_support();

	return 0;
}

early_param("nosmp", nosmp);

/* this is hard limit */
static int __init nrcpus(char *str)
{
	int nr_cpus;

	get_option(&str, &nr_cpus);
	if (nr_cpus > 0 && nr_cpus < nr_cpu_ids)
		nr_cpu_ids = nr_cpus;

	return 0;
}

early_param("nr_cpus", nrcpus);

static int __init maxcpus(char *str)
{
	get_option(&str, &setup_max_cpus);
	if (setup_max_cpus == 0)
		arch_disable_smp_support();

	return 0;
}

early_param("maxcpus", maxcpus);

/* Setup number of possible processor ids */
int nr_cpu_ids __read_mostly = NR_CPUS;
EXPORT_SYMBOL(nr_cpu_ids);

/* An arch may set nr_cpu_ids earlier if needed, so this would be redundant */
void __init setup_nr_cpu_ids(void)
{
	nr_cpu_ids = find_last_bit(cpumask_bits(cpu_possible_mask),NR_CPUS) + 1;
}

void __weak smp_announce(void)
{
	printk(KERN_INFO "Brought up %d CPUs\n", num_online_cpus());
}

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
struct timestamp_rec hotplug_ts_rec;
DEFINE_SPINLOCK(hotplug_timestamp_lock);
unsigned int timestamp_enable = 1;

long hotplug_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

static ssize_t timestamp_enable_proc_write(struct file *f, const char *data, size_t len, loff_t *offset)
{
	int r;
	unsigned int enable_val;

	r = kstrtouint_from_user(data, len, 0, &enable_val);

	if (r)
		return -EINVAL;

	timestamp_enable = enable_val;

	return len;
}
static int timestamp_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", timestamp_enable);
	return 0;
}

static int profile_timestamp_init(void)
{
	int ret = 0;
	int i = 0;
	spin_lock(&hotplug_timestamp_lock);
	for (i = 0; i < TIMESTAMP_REC_SIZE; i++) {
		hotplug_ts_rec.rec[i].func = NULL;
		hotplug_ts_rec.rec[i].line = 0;
		hotplug_ts_rec.rec[i].timestamp_us = 0;
		hotplug_ts_rec.rec[i].delta_us = 0;
		hotplug_ts_rec.rec[i].note1 = 0;
		hotplug_ts_rec.rec[i].note2 = 0;
		hotplug_ts_rec.rec[i].note3 = 0;
		hotplug_ts_rec.rec[i].note4 = 0;
	}

	hotplug_ts_rec.rec_idx = 0;
	hotplug_ts_rec.filter = 0;

	SET_TIMESTAMP_FILTER(hotplug_ts_rec, TIMESTAMP_FILTER);
	timestamp_enable = 0;
	spin_unlock(&hotplug_timestamp_lock);

	return ret;
}
#endif /* #ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3 */


#ifdef CONFIG_PROFILE_CPU
struct profile_cpu_stats *cpu_stats;
DEFINE_SPINLOCK(profile_cpu_stats_lock);

static int cpu_lat_proc_show(struct seq_file *m, void *v)
{
	unsigned int cpu;

	seq_puts(m, "cpu\t up_time      up_lat_sum.us   up_lat_avg");
	seq_puts(m, "      up_lat_max      up_lat_min\n");
	for (cpu = 0; cpu < CONFIG_NR_CPUS; cpu++) {
		seq_printf(m, "%d %14lld", cpu, cpu_stats[cpu].hotplug_up_time);
		seq_printf(m, "%16lld", cpu_stats[cpu].hotplug_up_lat_us);
		seq_printf(m, "%16lld", div64_ul(cpu_stats[cpu].hotplug_up_lat_us,
						cpu_stats[cpu].hotplug_up_time));
		seq_printf(m, "%16lld", cpu_stats[cpu].hotplug_up_lat_max);
		seq_printf(m, "%16lld\n", cpu_stats[cpu].hotplug_up_lat_min);
	}

	seq_puts(m, "cpu    down_time    down_lat_sum    down_lat_avg");
	seq_puts(m, "    down_lat_max    down_lat_min\n");
	for (cpu = 0; cpu < CONFIG_NR_CPUS; cpu++) {
		seq_printf(m, "%d %14lld", cpu, cpu_stats[cpu].hotplug_down_time);
		seq_printf(m, "%16lld", cpu_stats[cpu].hotplug_down_lat_us);
		seq_printf(m, "%16lld", div64_ul(cpu_stats[cpu].hotplug_down_lat_us,
						cpu_stats[cpu].hotplug_down_time));
		seq_printf(m, "%16lld", cpu_stats[cpu].hotplug_down_lat_max);
		seq_printf(m, "%16lld\n", cpu_stats[cpu].hotplug_down_lat_min);
	}

	for_each_possible_cpu(cpu) {
		cpu_stats[cpu].hotplug_up_time = 0;
		cpu_stats[cpu].hotplug_down_time = 0;
		cpu_stats[cpu].hotplug_up_lat_us = 0;
		cpu_stats[cpu].hotplug_down_lat_us = 0;
		cpu_stats[cpu].hotplug_up_lat_max = 0;
		cpu_stats[cpu].hotplug_down_lat_max = 0;
		cpu_stats[cpu].hotplug_up_lat_min = 0;
		cpu_stats[cpu].hotplug_down_lat_min = 0;
	}

	return 0;
}

static int profile_cpu_stats_init(void)
{
	unsigned int cpu;
	int ret = 0;
	cpu_stats = kzalloc(sizeof(*cpu_stats) * CONFIG_NR_CPUS, GFP_KERNEL);
	if (!cpu_stats)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		cpu_stats[cpu].hotplug_up_time = 0;
		cpu_stats[cpu].hotplug_down_time = 0;
		cpu_stats[cpu].hotplug_up_lat_us = 0;
		cpu_stats[cpu].hotplug_down_lat_us = 0;
		cpu_stats[cpu].hotplug_up_lat_max = 0;
		cpu_stats[cpu].hotplug_down_lat_max = 0;
		cpu_stats[cpu].hotplug_up_lat_min = 0;
		cpu_stats[cpu].hotplug_down_lat_min = 0;
	}

	return ret;
}
#endif /* ifdef CONFIG_PROFILE_CPU */

#define PROC_FOPS_RW(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
	.write          = name ## _proc_write,				\
}

#define PROC_FOPS_RO(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
PROC_FOPS_RW(timestamp_enable);
#endif
#ifdef CONFIG_PROFILE_CPU
PROC_FOPS_RO(cpu_lat);
#endif
static int create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

#ifdef CONFIG_PROFILE_CPU
	const struct pentry entries_ro[] = {
		PROC_ENTRY(cpu_lat),
	};
#endif

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
	const struct pentry entries_rw[] = {
		PROC_ENTRY(timestamp_enable),
	};
#endif

	dir = proc_mkdir("cpu_hotplug", NULL);

	if (!dir)
		return -ENOMEM;

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3

	for (i = 0; i < ARRAY_SIZE(entries_rw); i++) {
		if (!proc_create(entries_rw[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries_rw[i].fops))
			return -ENOMEM;
	}

#endif

#ifdef CONFIG_PROFILE_CPU

	for (i = 0; i < ARRAY_SIZE(entries_ro); i++) {
		if (!proc_create(entries_ro[i].name, 0, dir, entries_ro[i].fops))
			return -ENOMEM;
	}

#endif

	return 0;

}
/* Called by boot processor to activate the rest. */
void __init smp_init(void)
{
	unsigned int cpu;
#if defined(CONFIG_ARCH_MT6738)
	unsigned int segment, i;

	segment =  get_devinfo_with_index(21) & 0xFF;
	if ((segment == 0x43) || (segment == 0x4B)) {
			for (i = 1; i <= 3; i++) {
				set_cpu_present(i, false);
				set_cpu_active(i, false);
				set_cpu_online(i, false);
			}
	}
#endif
	idle_threads_init();

#if defined(CONFIG_PROFILE_CPU) || defined(CONFIG_MTK_CPU_HOTPLUG_DEBUG_3)
	create_procfs();
#endif

#ifdef CONFIG_PROFILE_CPU
	profile_cpu_stats_init();
#endif

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
	profile_timestamp_init();
#endif

	/* FIXME: This should be done in userspace --RR */
	for_each_present_cpu(cpu) {
		if (num_online_cpus() >= setup_max_cpus)
			break;
		if (!cpu_online(cpu))
			cpu_up(cpu);
	}

	/* Any cleanup work */
	smp_announce();
	smp_cpus_done(setup_max_cpus);
}

/*
 * Call a function on all processors.  May be used during early boot while
 * early_boot_irqs_disabled is set.  Use local_irq_save/restore() instead
 * of local_irq_disable/enable().
 */
int on_each_cpu(void (*func) (void *info), void *info, int wait)
{
	unsigned long flags;
	int ret = 0;

	preempt_disable();
	ret = smp_call_function(func, info, wait);
	local_irq_save(flags);
	func(info);
	local_irq_restore(flags);
	preempt_enable();
	return ret;
}
EXPORT_SYMBOL(on_each_cpu);

/**
 * on_each_cpu_mask(): Run a function on processors specified by
 * cpumask, which may include the local processor.
 * @mask: The set of cpus to run on (only runs on online subset).
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait (atomically) until function has completed
 *        on other CPUs.
 *
 * If @wait is true, then returns once @func has returned.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.  The
 * exception is that it may be used during early boot while
 * early_boot_irqs_disabled is set.
 */
void on_each_cpu_mask(const struct cpumask *mask, smp_call_func_t func,
			void *info, bool wait)
{
	int cpu = get_cpu();

	smp_call_function_many(mask, func, info, wait);
	if (cpumask_test_cpu(cpu, mask)) {
		unsigned long flags;
		local_irq_save(flags);
		func(info);
		local_irq_restore(flags);
	}
	put_cpu();
}
EXPORT_SYMBOL(on_each_cpu_mask);

/*
 * on_each_cpu_cond(): Call a function on each processor for which
 * the supplied function cond_func returns true, optionally waiting
 * for all the required CPUs to finish. This may include the local
 * processor.
 * @cond_func:	A callback function that is passed a cpu id and
 *		the the info parameter. The function is called
 *		with preemption disabled. The function should
 *		return a blooean value indicating whether to IPI
 *		the specified CPU.
 * @func:	The function to run on all applicable CPUs.
 *		This must be fast and non-blocking.
 * @info:	An arbitrary pointer to pass to both functions.
 * @wait:	If true, wait (atomically) until function has
 *		completed on other CPUs.
 * @gfp_flags:	GFP flags to use when allocating the cpumask
 *		used internally by the function.
 *
 * The function might sleep if the GFP flags indicates a non
 * atomic allocation is allowed.
 *
 * Preemption is disabled to protect against CPUs going offline but not online.
 * CPUs going online during the call will not be seen or sent an IPI.
 *
 * You must not call this function with disabled interrupts or
 * from a hardware interrupt handler or from a bottom half handler.
 */
void on_each_cpu_cond(bool (*cond_func)(int cpu, void *info),
			smp_call_func_t func, void *info, bool wait,
			gfp_t gfp_flags)
{
	cpumask_var_t cpus;
	int cpu, ret;

	might_sleep_if(gfp_flags & __GFP_WAIT);

	if (likely(zalloc_cpumask_var(&cpus, (gfp_flags|__GFP_NOWARN)))) {
		preempt_disable();
		for_each_online_cpu(cpu)
			if (cond_func(cpu, info))
				cpumask_set_cpu(cpu, cpus);
		on_each_cpu_mask(cpus, func, info, wait);
		preempt_enable();
		free_cpumask_var(cpus);
	} else {
		/*
		 * No free cpumask, bother. No matter, we'll
		 * just have to IPI them one by one.
		 */
		preempt_disable();
		for_each_online_cpu(cpu)
			if (cond_func(cpu, info)) {
				ret = smp_call_function_single(cpu, func,
								info, wait);
				WARN_ON_ONCE(ret);
			}
		preempt_enable();
	}
}
EXPORT_SYMBOL(on_each_cpu_cond);

static void do_nothing(void *unused)
{
}

/**
 * kick_all_cpus_sync - Force all cpus out of idle
 *
 * Used to synchronize the update of pm_idle function pointer. It's
 * called after the pointer is updated and returns after the dummy
 * callback function has been executed on all cpus. The execution of
 * the function can only happen on the remote cpus after they have
 * left the idle function which had been called via pm_idle function
 * pointer. So it's guaranteed that nothing uses the previous pointer
 * anymore.
 */
void kick_all_cpus_sync(void)
{
	/* Make sure the change is visible before we kick the cpus */
	smp_mb();
	smp_call_function(do_nothing, NULL, 1);
}
EXPORT_SYMBOL_GPL(kick_all_cpus_sync);

/**
 * wake_up_all_idle_cpus - break all cpus out of idle
 * wake_up_all_idle_cpus try to break all cpus which is in idle state even
 * including idle polling cpus, for non-idle cpus, we will do nothing
 * for them.
 */
void wake_up_all_idle_cpus(void)
{
	int cpu;

	preempt_disable();
	for_each_online_cpu(cpu) {
		if (cpu == smp_processor_id())
			continue;

		wake_up_if_idle(cpu);
	}
	preempt_enable();
}
EXPORT_SYMBOL_GPL(wake_up_all_idle_cpus);
