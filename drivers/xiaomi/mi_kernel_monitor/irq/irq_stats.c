

#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timex.h>
#include <linux/tracepoint.h>
//#include <trace/events/irq.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <trace/events/napi.h>
#include <linux/rtc.h>
#include <linux/time.h>

#include "internal.h"
#include "pub/trace_point.h"

#include "uapi/irq_stats.h"

struct xm_irq_stats_settings irq_stats_settings;

static int irq_stats_alloced = 0;

static bool trace_enable = false;
static int irq_count_threshold = 10000;  //per 5 seconds 40000 irq
static int softirq_duration_threshold = 200;  //per 5 seconds 40000 irq

struct CPU_IRQ_STAT_INFO {
	struct s_hard_irq_info
	{
	    int count;
		int cpu[16];
	    int irq_cnt[16];
	    int max_irq_cnt[16];
		int max_irq[16];
		u64 max_irq_total[16];
		u64 irq_run_total[16];
	    u64 timestamp[16];
	}hard_irq_info;

	struct s_soft_irq_info
	{
	    int count;
		int cpu[16];
		int max_softirq[16];
	    int softirq_cnt[16];
		u64 softirq_run_total[16];
	    u64 timestamp[16];
	}soft_irq_info;	
};

static struct CPU_IRQ_STAT_INFO cpu_irq_stat_info = {0};

static DEFINE_MUTEX(irq_stats_mutex);

///////////////////////////////////////////////////////////
static void dump_data(void);
static struct timer_list irqstats_timer;
static u64 irq_stat_period = 5;

static void showirq_timer_handler(struct timer_list *timer)
{
	dump_data();
    mod_timer(timer, jiffies + msecs_to_jiffies(irq_stat_period*1000));
	
    return;
}

static void irqstats_start_timer(void)
{
	timer_setup(&irqstats_timer, showirq_timer_handler,
			    TIMER_PINNED | TIMER_IRQSAFE);

	irqstats_timer.expires = jiffies + msecs_to_jiffies(irq_stat_period*1000);
	add_timer(&irqstats_timer);
}

static void irqstats_cancel_timer(void)
{
    del_timer_sync(&irqstats_timer);
}
////////////////////////////////////////////////////////////

static void clean_data(void)
{
	struct irq_func_runtime *func_runtimes[NR_BATCH];
	struct irq_func_runtime *func_runtime;
	int nr_found;
	struct xm_percpu_context *context;
	int cpu;
	unsigned long pos = 0;
	int i;

	for_each_possible_cpu(cpu) {
		context = get_percpu_context_cpu(cpu);

		rcu_read_lock();

		do {
			nr_found = radix_tree_gang_lookup(&context->irq_stats.irq_tree,
					(void **)func_runtimes, pos, NR_BATCH);
			for (i = 0; i < nr_found; i++) {
				func_runtime = func_runtimes[i];
				radix_tree_delete(&context->irq_stats.irq_tree,
							(unsigned long)func_runtime->handler);
				pos = (unsigned long)func_runtime->handler + 1;
				kfree(func_runtime);
			}
		} while (nr_found > 0);

		rcu_read_unlock();

		memset(&context->irq_stats, 0, sizeof(context->irq_stats));
		INIT_RADIX_TREE(&context->irq_stats.irq_tree, GFP_ATOMIC);
	}
}

static void trace_irq_handler_entry_hit(void *ignore, int irq, struct irqaction *action)
{
	struct irq_runtime *runtime = &get_percpu_context()->irq_stats.irq_runtime;
	u64 now;

	if (hardirq_count() > (1 << HARDIRQ_SHIFT))
		return;

	now = sched_clock(); //ktime_to_ns(ktime_get());
	runtime->irq = irq;
	runtime->time = now;

	return;
}

static void trace_irq_handler_exit_hit(void *ignore, int irq, struct irqaction *action, int ret)
{
	struct irq_runtime *entry_runtime;
	struct irq_result *result;
	struct xm_timespec ts;
	struct xm_percpu_context *context;

	u64 now = sched_clock();
	u64 delta_ns;

	if (hardirq_count() > (1 << HARDIRQ_SHIFT))
		return;

	context = get_percpu_context();
	entry_runtime = &context->irq_stats.irq_runtime;
	result = &context->irq_stats.irq_result;

	if (ret && (entry_runtime->time > 0))
	{
		struct irq_func_runtime *func_runtime;

		delta_ns = now - entry_runtime->time;

		result->irq_cnt++;
		result->irq_run_total += delta_ns;

		if (delta_ns > result->max_irq.time)
		{
			do_xm_gettimeofday(&ts);

			sprintf(result->max_irq.timestamp,
					"%lu / %lu",
					ts.tv_sec,
					ts.tv_usec);

			result->max_irq.time = delta_ns;
			result->max_irq.irq = irq;
		}

		{
			func_runtime = radix_tree_lookup(&context->irq_stats.irq_tree,
									(unsigned long)action->handler);
			if (!func_runtime) {
				func_runtime = kmalloc(sizeof(struct irq_func_runtime), GFP_ATOMIC | __GFP_ZERO);
				if (func_runtime) {
					func_runtime->irq = irq;
					func_runtime->handler = action->handler;
					radix_tree_insert(&context->irq_stats.irq_tree,
									(unsigned long)action->handler, func_runtime);
				}
			}
			if (func_runtime) {
				func_runtime->irq_cnt++;
				func_runtime->irq_run_total += delta_ns;
			}
		}
	}
}

static void trace_softirq_entry_hit(void *ignore, unsigned int nr_sirq)
{
	u64 period;
	struct softirq_runtime *runtime;

	if (nr_sirq >= NR_SOFTIRQS)
		return;

	period = sched_clock(); //ktime_to_ns(ktime_get());

	runtime = &get_percpu_context()->irq_stats.softirq_runtime;
	runtime->time[nr_sirq] = period;

	return;
}


static void trace_softirq_exit_hit(void *ignore, unsigned int nr_sirq)
{
	u64 period;
	u64 delta_ns;
	struct softirq_runtime *entry_runtime;
	struct irq_result *result;

	if (nr_sirq >= NR_SOFTIRQS)
		return;

	period = sched_clock(); //ktime_to_ns(ktime_get());

	entry_runtime = &get_percpu_context()->irq_stats.softirq_runtime;
	result = &get_percpu_context()->irq_stats.irq_result;

	if (entry_runtime->time[nr_sirq] > 0)
	{
		delta_ns = period - entry_runtime->time[nr_sirq];

		result->softirq_cnt[nr_sirq]++;
		result->sortirq_run_total[nr_sirq] += delta_ns;

		if (delta_ns > result->max_softirq.time[nr_sirq])
			result->max_softirq.time[nr_sirq] = delta_ns;
	}

	return;
}

static int __activate_irq_stats(void)
{
	struct irq_result *result;
	int cpu;

	irq_stats_alloced = 1;
    
	for_each_possible_cpu(cpu)
	{
		result = &get_percpu_context_cpu(cpu)->irq_stats.irq_result;
		memset(result, 0, sizeof(struct irq_result));
		INIT_RADIX_TREE(&get_percpu_context_cpu(cpu)->irq_stats.irq_tree, GFP_ATOMIC);
	}

	hook_tracepoint("irq_handler_entry", trace_irq_handler_entry_hit, NULL);
	hook_tracepoint("irq_handler_exit", trace_irq_handler_exit_hit, NULL);
	hook_tracepoint("softirq_entry", trace_softirq_entry_hit, NULL);
    hook_tracepoint("softirq_exit", trace_softirq_exit_hit, NULL);

    irqstats_start_timer();
	return 1;
}

int activate_irq_stats(void)
{
	if (!irq_stats_settings.activated)
		irq_stats_settings.activated = __activate_irq_stats();
	memset(&cpu_irq_stat_info, 0, sizeof(struct CPU_IRQ_STAT_INFO));

	return irq_stats_settings.activated;
}

static void __deactivate_irq_stats(void)
{
	unhook_tracepoint("irq_handler_entry", trace_irq_handler_entry_hit, NULL);
	unhook_tracepoint("irq_handler_exit", trace_irq_handler_exit_hit, NULL);
	unhook_tracepoint("softirq_entry", trace_softirq_entry_hit, NULL);
	unhook_tracepoint("softirq_exit", trace_softirq_exit_hit, NULL);
	
	synchronize_sched();
	msleep(10);

	clean_data();
	irqstats_cancel_timer();
}

int deactivate_irq_stats(void)
{
	if (irq_stats_settings.activated)
		__deactivate_irq_stats();
	irq_stats_settings.activated = 0;

	return 0;
}

static void dump_data(void)
{
	int cpu;
	int softirq;
	struct irq_result *result;
	struct xm_percpu_context *context;
	struct irq_stats_irq_summary irq_summary;
	struct irq_stats_irq_detail irq_detail;
	struct irq_stats_softirq_summary softirq_summary;
	int is_irq_storm = 0;


	for_each_online_cpu(cpu)
	{
	    int cnt = cpu_irq_stat_info.hard_irq_info.count;  //检测到几次中断风暴
		if (cnt >= 16)
		{
		    goto soft_irq_label;
		}
		
		context = get_percpu_context_cpu(cpu);
		result = &context->irq_stats.irq_result;

		irq_summary.cpu = cpu;
		irq_summary.irq_cnt = result->irq_cnt;
		irq_summary.irq_run_total = result->irq_run_total;
		irq_summary.max_irq = result->max_irq.irq;
		irq_summary.max_irq_time = result->max_irq.time;
		if (irq_summary.irq_cnt > (irq_count_threshold * irq_stat_period))
		{
		    ////printk("xiaomi_km: hardirq_storm cpu[%d] irq_cnt=%lu, runtime=%lu, max_irq=%lu, max_irq_time=%lu\n", irq_summary.cpu, irq_summary.irq_cnt, irq_summary.irq_run_total, irq_summary.max_irq, irq_summary.max_irq_time);
			cpu_irq_stat_info.hard_irq_info.cpu[cnt] = cpu;
			cpu_irq_stat_info.hard_irq_info.irq_cnt[cnt] = irq_summary.irq_cnt;
			cpu_irq_stat_info.hard_irq_info.timestamp[cnt] = sched_clock();
			cpu_irq_stat_info.hard_irq_info.irq_run_total[cnt] = irq_summary.irq_run_total;
			cpu_irq_stat_info.hard_irq_info.count++;
			is_irq_storm = 1;
			break;
	    }
	}

    if (is_irq_storm == 0)
    {
        goto soft_irq_label;
    }
	
	////for_each_possible_cpu(cpu) 
	{
	    u64 max_count = 0;
		struct irq_func_runtime *func_runtime;
		struct irq_func_runtime *func_runtimes[NR_BATCH];
		int nr_found;
		unsigned long pos = 0;
		int i;

	    context = get_percpu_context_cpu(cpu);
		result = &context->irq_stats.irq_result;

	    rcu_read_lock();
		do 
		{
			nr_found = radix_tree_gang_lookup(&context->irq_stats.irq_tree,(void **)func_runtimes, pos, NR_BATCH);
			////rcu_read_unlock();
		    for (i = 0; i < nr_found; i++) {
				func_runtime = func_runtimes[i];
			    pos = (unsigned long)func_runtime->handler + 1;
				if (func_runtime->irq_cnt) 
				{
					irq_detail.cpu = cpu;
				    irq_detail.irq = func_runtime->irq;
					irq_detail.handler = func_runtime->handler;
				    irq_detail.irq_cnt = func_runtime->irq_cnt;
					irq_detail.irq_run_total = func_runtime->irq_run_total;
					if (irq_detail.irq_cnt > max_count)
					{
					    max_count = irq_detail.irq_cnt;
						cpu_irq_stat_info.hard_irq_info.max_irq[cpu_irq_stat_info.hard_irq_info.count - 1] = irq_detail.irq;
		                cpu_irq_stat_info.hard_irq_info.max_irq_cnt[cpu_irq_stat_info.hard_irq_info.count - 1] = irq_detail.irq_cnt;
	                    cpu_irq_stat_info.hard_irq_info.max_irq_total[cpu_irq_stat_info.hard_irq_info.count - 1] = irq_detail.irq_run_total;
						////printk("xiaomi_km: hardirq_storm cpu[%d] max_irq=%u, irq_cnt=%lu, irq_run_total=%lu\n", irq_detail.cpu, irq_detail.irq, irq_detail.irq_cnt, irq_detail.irq_run_total);
					}

				}
		    }
			
			////rcu_read_lock();
	    } 
		while (nr_found == NR_BATCH);
		rcu_read_unlock();
	}


soft_irq_label:	
	for_each_online_cpu(cpu)
	{
	    int cnt = cpu_irq_stat_info.soft_irq_info.count;
		if (cnt >= 16)
		{
		    break;
		}
		
		context = get_percpu_context_cpu(cpu);
		result = &context->irq_stats.irq_result;

		softirq_summary.cpu = cpu;
		for (softirq = HI_SOFTIRQ; softirq < XM_NR_SOFTIRQS; softirq++)
		{
			softirq_summary.softirq_cnt[softirq] = result->softirq_cnt[softirq];
			softirq_summary.sortirq_run_total[softirq] = result->sortirq_run_total[softirq];
			if (softirq_summary.sortirq_run_total[softirq] > (softirq_duration_threshold * irq_stat_period * 1000000))
			{
			    ////printk("xiaomi_km: softirq_storm  cpu[%d] softirq[%d], softirq_cnt=%lu,sortirq_run_total=%lu\n", cpu, softirq, softirq_summary.softirq_cnt[softirq], softirq_summary.sortirq_run_total[softirq]);
				cpu_irq_stat_info.soft_irq_info.cpu[cnt] = cpu;
				cpu_irq_stat_info.soft_irq_info.max_softirq[cnt] = softirq;
				cpu_irq_stat_info.soft_irq_info.softirq_cnt[cnt] = softirq_summary.softirq_cnt[softirq];
				cpu_irq_stat_info.soft_irq_info.softirq_run_total[cnt] = softirq_summary.sortirq_run_total[softirq];
				cpu_irq_stat_info.soft_irq_info.timestamp[cnt] = sched_clock();
			    cpu_irq_stat_info.soft_irq_info.count++;
			    break;
			}
		}
	}

	clean_data();
}

static int threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", irq_count_threshold);

	return 0;
}

static ssize_t threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	irq_count_threshold = val;

	return count;
}

static int enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%s\n", trace_enable ? "enabled" : "disabled");

	return 0;
}

static ssize_t enable_store(void *priv, const char __user *buf, size_t count)
{
	bool enable;

	if (kstrtobool_from_user(buf, count, &enable))
		return -EINVAL;

	if (!!enable == !!trace_enable)
		return count;

	if (enable) {
		activate_irq_stats();
	} else {
	    deactivate_irq_stats();
	}

	trace_enable = enable;

	return count;
}

static int irq_info_show(struct seq_file *m, void *ptr)
{
    int i = 0;
    for (i = 0; i < cpu_irq_stat_info.hard_irq_info.count; i++)
	{
	    seq_printf(m, "hardirq: cpu[%d] irq_cnt[%d] irq_run_total[%llu ms] max_irq[%d] max_irq_cnt[%d] max_irq_total[%llu ms] timestamp[%llu]\n", 
			          cpu_irq_stat_info.hard_irq_info.cpu[i], 
			          cpu_irq_stat_info.hard_irq_info.irq_cnt[i],
			          cpu_irq_stat_info.hard_irq_info.irq_run_total[i]/1000000,
			          cpu_irq_stat_info.hard_irq_info.max_irq[i],
			          cpu_irq_stat_info.hard_irq_info.max_irq_cnt[i],
			          cpu_irq_stat_info.hard_irq_info.max_irq_total[i]/1000000,
			          cpu_irq_stat_info.hard_irq_info.timestamp[i]);
	}
	
	for (i = 0; i < cpu_irq_stat_info.soft_irq_info.count; i++)
	{
	    seq_printf(m, "softirq: cpu[%d] max_softirq[%d] softirq_cnt[%d] softirq_run_total[%llu ms] timestamp[%llu]\n", 
			          cpu_irq_stat_info.soft_irq_info.cpu[i], 
			          cpu_irq_stat_info.soft_irq_info.max_softirq[i],
			          cpu_irq_stat_info.soft_irq_info.softirq_cnt[i],
			          cpu_irq_stat_info.soft_irq_info.softirq_run_total[i]/1000000,
			          cpu_irq_stat_info.soft_irq_info.timestamp[i]);	
	}
	
	return 0;
}

static ssize_t irq_info_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}

static int softirq_threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", softirq_duration_threshold);

	return 0;
}

static ssize_t softirq_threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	softirq_duration_threshold = val;

	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(threshold);
DEFINE_PROC_ATTRIBUTE_RW(softirq_threshold);			 
DEFINE_PROC_ATTRIBUTE_RW(enable);
DEFINE_PROC_ATTRIBUTE_RW(irq_info);

int xiaomi_irq_stats_init(void)
{
	int cpu;
	struct proc_dir_entry *parent_dir;

	for_each_possible_cpu(cpu)
	{
		INIT_RADIX_TREE(&get_percpu_context_cpu(cpu)->irq_stats.irq_tree, GFP_ATOMIC);
	}

	if (irq_stats_settings.activated)
		irq_stats_settings.activated = __activate_irq_stats();

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_IRQSTAT, NULL);

	//parent_dir = proc_mkdir(PROC_DIR_NAME, NULL);
	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("threshold", 0666, parent_dir, &threshold_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("softirq_threshold", 0666, parent_dir, &softirq_threshold_fops, NULL))
		goto remove_proc;	
		
	if (!proc_create_data("enable", 0666, parent_dir, &enable_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("irq_info", 0, parent_dir, &irq_info_fops, NULL))
		goto remove_proc;


	return 0;
		
	remove_proc:
		remove_proc_subtree(PROC_TRACE_IRQSTAT, NULL);
	free_buf:
		return -ENOMEM;

}

void xiaomi_irq_stats_exit(void)
{
	if (irq_stats_settings.activated)
		deactivate_irq_stats();
	irq_stats_settings.activated = 0;
	remove_proc_subtree(PROC_TRACE_IRQSTAT, NULL);
	////free_percpu(cpu_irq_stat_info);
}
