#include <linux/proc_fs.h>
#include <linux/sched/task.h>
#include "cpu_util.h"
#include "internal.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt)  "CpuUtil:"fmt

#define do_each_thread(g, t) \
	for (g = t = &init_task ; (g = t = next_task(g)) != &init_task ; ) do
	
static bool trace_enable = false;
static int cpu_util_threshold = 80;
static int sampling_period = 15;  //10s

struct delayed_work cpu_util_dump;
static struct xm_stack_trace_task s_cpuutil_stack_trace = {0};


#define PID_HASH_BITS	10
static DECLARE_HASHTABLE(hash_table, PID_HASH_BITS);
static DEFINE_RT_MUTEX(pid_lock);

#define TASK_NAME_LEN			16

struct pid_entry {
	pid_t pid;
	char comm[TASK_NAME_LEN];
	u64 utime;
	u64 stime;
	struct hlist_node hash;
};

struct top_cpu_info {
	pid_t pid;
	pid_t ppid;
	struct task_struct* task;
	char comm[TASK_NAME_LEN];
	char pcomm[TASK_NAME_LEN];
	u64 DeltaUtime;
	u64 DeltaStime;
	u64 totaltime;
	cpumask_t cpus_mask;
};

ktime_t PrevKtime = 0;

void get_current_timestamp(char* state)
{
	struct rtc_time tm;
	struct timespec64 tv = { 0 };
	/* android time */
	struct rtc_time tm_android;
	struct timespec64 tv_android = { 0 };

	ktime_get_real_ts64(&tv);
	tv_android = tv;
	rtc_time64_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	pr_info("xiaomi %s:%02d-%02d-%02d %02d:%02d:%02d.%03d(android time)\n",
		state,tm_android.tm_year + 1900,tm_android.tm_mon + 1,
		tm_android.tm_mday, tm_android.tm_hour,
		tm_android.tm_min, tm_android.tm_sec,
		(unsigned int)(tv_android.tv_nsec / 1000));
}

static inline void xm_task_cputime(struct task_struct *t,
				u64 *utime, u64 *stime)
{
	*utime = t->utime;
	*stime = t->stime;
}

static void xm_cputime_adjust(struct task_cputime *curr, struct prev_cputime *prev,
		    u64 *ut, u64 *st)
{
	u64 rtime, stime, utime;
	unsigned long flags;

	raw_spin_lock_irqsave(&prev->lock, flags);
	rtime = curr->sum_exec_runtime;

	if (prev->stime + prev->utime >= rtime) 
		goto out;

	stime = curr->stime;
	utime = curr->utime;

	if (stime == 0) {
		utime = rtime;
		goto update;
	}

	if (utime == 0) {
		stime = rtime;
		goto update;
	}

	stime = mul_u64_u64_div_u64_bak(stime, rtime, stime + utime);

update:

	if (stime < prev->stime)
		stime = prev->stime;
	utime = rtime - stime;

	if (utime < prev->utime) {
		utime = prev->utime;
		stime = rtime - utime;
	}

	prev->stime = stime;
	prev->utime = utime;
out:
	*ut = prev->utime;
	*st = prev->stime;
	raw_spin_unlock_irqrestore(&prev->lock, flags);
}

static void xm_task_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st)
{
	struct task_cputime cputime = {
		.sum_exec_runtime = p->se.sum_exec_runtime,
	};

	xm_task_cputime(p, &cputime.utime, &cputime.stime);
	xm_cputime_adjust(&cputime, &p->prev_cputime, ut, st);
}

static struct pid_entry *find_pid_entry(pid_t pid)
{
	struct pid_entry *pid_entry;
	hash_for_each_possible(hash_table, pid_entry, hash, pid) {
		if (pid_entry->pid == pid)
			return pid_entry;
	}
	return NULL;
}

static struct pid_entry *find_or_register_pid(struct task_struct *p)
{
	struct pid_entry *pid_entry;

	pid_entry = find_pid_entry(p->pid);
	if (pid_entry)
		return pid_entry;

	pid_entry = kzalloc(sizeof(struct pid_entry), GFP_ATOMIC);
	if (!pid_entry)
		return NULL;

	pid_entry->pid = p->pid;
	strcpy(pid_entry->comm,p->comm);
	//pid_entry->utime = p->utime;
	//pid_entry->stime = p->stime;
	pid_entry->utime = 0;
	pid_entry->stime = 0;
	hash_add(hash_table, &pid_entry->hash, p->pid);

	return pid_entry;
}

static inline void seq_print_stack_trace(struct seq_file *m, struct stack_entry_task *entry)
{
	int i;

	for (i = 0; i < entry->nr_entries; i++)
		seq_printf(m, "%*c%pS\n", 5, ' ', (void *)entry->entries[i]);
}

static inline bool stack_trace_record(struct task_struct*tsk, struct xm_stack_trace_task *stack_trace, int delta)
{
	////if (unlikely(delta >= cpu_util_threshold))
		return xm_stack_trace_record_for_task(tsk, stack_trace, xm_get_irq_regs(), delta);

	return false;
}

static void print_all_task_stack(void)
{
#if 1
	struct task_struct *g = NULL, *p = NULL;
	u64 DeltaUtime,DeltaStime,utime,stime;
	ktime_t delta_time,now;
	struct pid_entry *pid_entry = NULL;
	struct top_cpu_info task[6];
	struct top_cpu_info temp;

	int UserAvgLoad = 0, KernelAvgLoad = 0, i = 0, j = 0;

	rt_mutex_lock(&pid_lock);
	for(j=0;j<6;j++){
		task[j].DeltaUtime=0;
		task[j].DeltaStime=0;
		task[j].totaltime=0;
	}
	now = ktime_get();
	delta_time = ktime_sub(now,PrevKtime);
	delta_time = ktime_to_ms(delta_time);
	rcu_read_lock();
	if (PrevKtime != 0){
		do_each_thread(g, p)   ////modify by huqinghe 20240828
		{
		 if (!pid_entry || pid_entry->pid != p->pid){
			pid_entry = find_or_register_pid(p);
		 }
		 if (!pid_entry) {
			rcu_read_unlock();
			rt_mutex_unlock(&pid_lock);
			pr_err("%s: failed to find the pid_entry for pid %d\n",__func__, p->pid);
			return;
		 }
		 if (!(p->flags & PF_EXITING)) {
			xm_task_cputime_adjusted(p, &utime, &stime);
			//DeltaUtime = (u64)jiffies64_to_msecs(DeltaUtime);
			//DeltaStime = (u64)jiffies64_to_msecs(DeltaStime);
			if (((utime > pid_entry->utime) && (stime >= pid_entry->stime))
			   ||((stime > pid_entry->stime) && (utime >= pid_entry->utime))){
				DeltaUtime=utime - pid_entry->utime;
				DeltaStime=stime - pid_entry->stime;
				DeltaUtime=ktime_to_ms(DeltaUtime);
				DeltaStime=ktime_to_ms(DeltaStime);
				task[5].DeltaStime=DeltaStime;
				task[5].DeltaUtime=DeltaUtime;
				task[5].totaltime=DeltaStime+DeltaUtime;
				task[5].task = p;
				task[5].pid=p->pid;
				task[5].ppid=p->group_leader->pid;
				task[5].cpus_mask=p->cpus_mask;
				strcpy(task[5].comm,p->comm);
				strcpy(task[5].pcomm,p->group_leader->comm);
				for(i=0;i<5;i++){
				   for(j=5;j>i;j--){
				     if(task[j].totaltime>task[i].totaltime){
					temp=task[j];
					task[j]=task[i];
					task[i]=temp;
				     }
				   }
				}
			}
			pid_entry->utime=utime;
			pid_entry->stime=stime;
			strcpy(pid_entry->comm,p->comm);
		 }
		}
		while_each_thread(g, p);
		get_current_timestamp("dump_top5_task");
		for(i=0;i<1;i++){
			UserAvgLoad = task[i].DeltaUtime*100/delta_time;
			KernelAvgLoad = task[i].DeltaStime*100/delta_time;
			/////pr_info("xiaomi Cpu usage:%d%% user + %d%% kernel, comm:%s, pid:%d, pcomm:%s, ppid:%d\n",
			////	UserAvgLoad,KernelAvgLoad,task[i].comm,task[i].pid,
			////	task[i].pcomm,task[i].ppid);
			if ((UserAvgLoad + KernelAvgLoad) > cpu_util_threshold)
			{
			    pr_info("xiaomi_km:[high] Cpu usage:%d%% user + %d%% kernel, comm:%s, pid:%d\n",UserAvgLoad,KernelAvgLoad,task[i].comm,task[i].pid);
			    stack_trace_record(task[i].task, &s_cpuutil_stack_trace, UserAvgLoad+KernelAvgLoad);
				////get_kernel_bt(current, UserAvgLoad+KernelAvgLoad, "trace_cpu_util");
			}
			else
			{
			    pr_info("xiaomi[low] Cpu usage:%d%% user + %d%% kernel, comm:%s, pid:%d\n",UserAvgLoad,KernelAvgLoad,task[i].comm,task[i].pid);
			}
		}
	}
	rcu_read_unlock();
	PrevKtime=ktime_get();
	rt_mutex_unlock(&pid_lock);
#endif	
}


static void dump_cpu_info(void)
{
	print_all_task_stack();
}

static int activate_cpu_util_monitor(void)
{
    printk("xiaomi activate_cpu_util_monitor start\n");
	memset(&s_cpuutil_stack_trace, 0, sizeof(struct xm_stack_trace_task));
	schedule_delayed_work(&cpu_util_dump, round_jiffies_relative(msecs_to_jiffies(sampling_period*1000)));

	return 0;
}

static int deactivate_cpu_util_monitor(void)
{
	printk("xiaomi deactivate_cpu_util_monitor cancel\n");
	cancel_delayed_work_sync(&cpu_util_dump);
	return 0;
}

/*Print CPU info*/
static void cup_util_dump_func(struct work_struct *work)
{
    if (!trace_enable)
    {
        pr_info("xiaomi cup_util_dump_func disabled!\n");
        return;
    }
	
	pr_info("xiaomi cup_util_dump_func start \n");
	dump_cpu_info();
	schedule_delayed_work(&cpu_util_dump,round_jiffies_relative(msecs_to_jiffies(sampling_period*1000)));
}

static int sampling_period_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", sampling_period);

	return 0;
}

static ssize_t sampling_period_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	sampling_period = val;

	return count;
}

static int threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", cpu_util_threshold);

	return 0;
}

static ssize_t threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	cpu_util_threshold = val;

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
		activate_cpu_util_monitor();
	} else {
	    deactivate_cpu_util_monitor();
	}

	trace_enable = enable;

	return count;
}

static int stack_trace_show(struct seq_file *m, void *ptr)
{
	////int cpu;
	struct xm_stack_trace_task *cpu_stack_trace = &s_cpuutil_stack_trace;

	////for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		nr = smp_load_acquire(&cpu_stack_trace->nr_tasks);
		if (!nr)
			return 0;

		////seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry_task *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d util:%d timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

	return 0;

}

static ssize_t stack_trace_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(sampling_period);
DEFINE_PROC_ATTRIBUTE_RW(threshold);
DEFINE_PROC_ATTRIBUTE_RW(enable);
DEFINE_PROC_ATTRIBUTE_RW(stack_trace);

int  xiaomi_cpu_util_init(void)
{
	struct proc_dir_entry *parent_dir;

	pr_info("xiaomi start \n");
	hash_init(hash_table);
	INIT_DELAYED_WORK(&cpu_util_dump, cup_util_dump_func);
	////schedule_delayed_work(&cpu_util_dump,round_jiffies_relative(msecs_to_jiffies(sampling_period*1000)));

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_CPU_UTIL, NULL);

	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("threshold", 0666, parent_dir, &threshold_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("sampling_period", 0666, parent_dir, &sampling_period_fops, NULL))
		goto remove_proc;
	
	if (!proc_create_data("enable", 0666, parent_dir, &enable_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("stack_trace", 0, parent_dir, &stack_trace_fops, NULL))
		goto remove_proc;
	
	return 0;
		
	remove_proc:
		remove_proc_subtree(PROC_TRACE_CPU_UTIL, NULL);
	free_buf:
		cancel_delayed_work(&cpu_util_dump);
		return -ENOMEM;	
}

void  xiaomi_cpu_util_exit(void)
{
	pr_info("powerdet_exit\n");
	cancel_delayed_work(&cpu_util_dump);
	remove_proc_subtree(PROC_TRACE_CPU_UTIL, NULL);
}


