#include "xm_power.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "PowerDet:(%d:%s:%d)" fmt,current->pid,__func__,__LINE__

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
#include "../../gpu/drm/mediatek/mediatek_v2/mi_disp/mi_disp_notifier.h"
#endif

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
struct screen_monitor {
	 struct notifier_block power_notifier;
	 int screen_state;
};
struct screen_monitor sm;
#endif

unsigned int  wakelock_print_period_s = 10;
struct delayed_work power_info_dump;


/*add for cpuinfo*/
#define PID_HASH_BITS	10
DECLARE_HASHTABLE(hash_table, PID_HASH_BITS);
static DEFINE_RT_MUTEX(pid_lock);

#define TASK_NAME_LEN			16

unsigned int  task_print_period_s = 20;

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
	char comm[TASK_NAME_LEN];
	char pcomm[TASK_NAME_LEN];
	u64 DeltaUtime;
	u64 DeltaStime;
	u64 totaltime;
	cpumask_t cpus_mask;
};

ktime_t PrevKtime = 0;

/*add for cpuinfo*/

/*add for control print period*/
struct kobject *xm_power_kobj;
bool reset_write_flag;
static ssize_t reset_show(struct kobject *kobj,struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,"%d\n",reset_write_flag);
}

static ssize_t reset_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	char *p;
	int len;

	p= memchr(buf,'\n',n);
	len=p?p-buf:n;
	if(len == strlen("reset") && !strncmp(buf,"reset",len)){
		wakelock_print_period_s=10;
		task_print_period_s=20;
		reset_write_flag=1;
	}else{
		reset_write_flag=0;
	}
	return n;
}
power_attr(reset);

static ssize_t task_print_period_show(struct kobject *kobj,struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", task_print_period_s);
}

static ssize_t task_print_period_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	task_print_period_s = val;
	return n;
}
power_attr(task_print_period);


static ssize_t wakelock_print_period_show(struct kobject *kobj,struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wakelock_print_period_s);
}

static ssize_t wakelock_print_period_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	wakelock_print_period_s = val;
	return n;
}
power_attr(wakelock_print_period);

static struct attribute * g[] = {
	&reset_attr.attr,
	&task_print_period_attr.attr,
	&wakelock_print_period_attr.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = g,
};
static const struct attribute_group *attr_groups[] = {
	&attr_group,
	NULL,
};
/*add for control print period*/

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
	pr_info("%s:%02d-%02d-%02d %02d:%02d:%02d.%03d(android time)\n",
		state,tm_android.tm_year + 1900,tm_android.tm_mon + 1,
		tm_android.tm_mday, tm_android.tm_hour,
		tm_android.tm_min, tm_android.tm_sec,
		(unsigned int)(tv_android.tv_nsec / 1000));
}


#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
static int screen_state_for_power_callback(struct notifier_block *nb, unsigned long val, void *v)
{
	struct mi_disp_notifier *evdata = v;
	struct screen_monitor *s_m = container_of(nb, struct screen_monitor, power_notifier);
	unsigned int blank;

	if (!(val == MI_DISP_DPMS_EARLY_EVENT || val == MI_DISP_DPMS_EVENT)) {
		pr_info("event(%lu) do not need process\n", val);
		return NOTIFY_OK;
	}
	if (evdata && evdata->data && s_m) {
		blank = *(int *)(evdata->data);
		if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_POWERDOWN )) {
			sm.screen_state = 0;
			get_current_timestamp("SCREEN_OFF");
		} else if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_ON)) {
			sm.screen_state = 1;
			get_current_timestamp("SCREEN_ON");
		} else if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_LP1)) {
			get_current_timestamp("SCREEN_AOD_ON");
		}
	} else {
		pr_info("MI_DISP can not get screen_state");
		return -1;
	}
	return NOTIFY_OK;
}
#endif

static void dump_active_wakeup_sources(void)
{
	struct wakeup_source *ws_debug;
	int srcuidx, active = 0;
	struct wakeup_source *last_activity_ws = NULL;
	struct wakeup_source *last_sec_activity_ws = NULL;
	ktime_t active_time;

	srcuidx = wakeup_sources_read_lock();
	ws_debug = wakeup_sources_walk_start();
	while (ws_debug != NULL) {
	 if (ws_debug->active) {
	  ktime_t now = ktime_get();
	  active_time = ktime_sub(now, ws_debug->last_time);
	  pr_info("active wake lock: %s, active_since: %lld ms , active_count: %lu , pending suspend count: %lu\n",
		ws_debug->name,ktime_to_ms(active_time), ws_debug->active_count, ws_debug->wakeup_count);
	  active = 1;
	 } else if (!active && (!last_activity_ws || ktime_to_ns(ws_debug->last_time) > ktime_to_ns(last_activity_ws->last_time))) {

	  if(last_activity_ws!=NULL)
		last_sec_activity_ws = last_activity_ws;
	  last_activity_ws = ws_debug;
	 }
	 ws_debug = wakeup_sources_walk_next(ws_debug);
	}

	if (!active && last_activity_ws)
		pr_info("last active wakeup source: %s, last_time:%lld ms, active_count: %lu ,pending suspend count %lu\n",
		  last_activity_ws->name,ktime_to_ms(last_activity_ws->last_time),
		  last_activity_ws->active_count,last_activity_ws->wakeup_count);
	if (!active && last_sec_activity_ws)
		pr_info("last sec active wakeup source: %s, last_time:%lld ms, active_count: %lu ,pending suspend count %lu\n",
		  last_sec_activity_ws->name,ktime_to_ms(last_sec_activity_ws->last_time),
		  last_sec_activity_ws->active_count,last_sec_activity_ws->wakeup_count);
	wakeup_sources_read_unlock(srcuidx);
}


/*Print CPU info*/

#ifdef CONFIG_VIRT_CPU_ACCOUNTING_GEN

void task_cputime(struct task_struct *t, u64 *utime, u64 *stime)
{
	struct vtime *vtime = &t->vtime;
	unsigned int seq;
	u64 delta;

	if (!vtime_accounting_enabled()) {
		*utime = t->utime;
		*stime = t->stime;
		return;
	}

	do {
		seq = read_seqcount_begin(&vtime->seqcount);

		*utime = t->utime;
		*stime = t->stime;

		/* Task is sleeping or idle, nothing to add */
		if (vtime->state < VTIME_SYS)
			continue;

		delta = vtime_delta(vtime);

		/*
		 * Task runs either in user (including guest) or kernel space,
		 * add pending nohz time to the right place.
		 */
		if (vtime->state == VTIME_SYS)
			*stime += vtime->stime + delta;
		else
			*utime += vtime->utime + delta;
	} while (read_seqcount_retry(&vtime->seqcount, seq));
}
#else/* ！CONFIG_VIRT_CPU_ACCOUNTING_GEN */

static inline void task_cputime(struct task_struct *t,
				u64 *utime, u64 *stime)
{
	*utime = t->utime;
	*stime = t->stime;
}

#endif /* CONFIG_VIRT_CPU_ACCOUNTING_GEN */

#ifdef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE

void task_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st)
{
	*ut = p->utime;
	*st = p->stime;
}

#else /* !CONFIG_VIRT_CPU_ACCOUNTING_NATIVE: */

void cputime_adjust(struct task_cputime *curr, struct prev_cputime *prev,
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

void task_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st)
{
	struct task_cputime cputime = {
		.sum_exec_runtime = p->se.sum_exec_runtime,
	};

	task_cputime(p, &cputime.utime, &cputime.stime);
	cputime_adjust(&cputime, &p->prev_cputime, ut, st);
}

#endif /* !CONFIG_VIRT_CPU_ACCOUNTING_NATIVE */

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

static void print_all_task_stack(void)
{
	struct task_struct *g, *p;
	u64 DeltaUtime,DeltaStime,utime,stime;
	ktime_t delta_time,now;
	struct pid_entry *pid_entry = NULL;
	struct top_cpu_info task[6];
	struct top_cpu_info temp;

	int UserAvgLoad,KernelAvgLoad,i,j;
	pr_info("CpuLoad: %lu.%02lu(1min), %lu.%02lu(5min), %lu.%02lu(15min)\n",
		LOAD_INT(avenrun[0]), LOAD_FRAC(avenrun[0]),
		LOAD_INT(avenrun[1]), LOAD_FRAC(avenrun[1]),
		LOAD_INT(avenrun[2]), LOAD_FRAC(avenrun[2]));

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
		do_each_thread(g, p)
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
		   #ifndef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
			task_cputime_adjusted(p, &utime, &stime);
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
		   #else
		   if (p->state == TASK_RUNNING){
			pr_info("running task, comm:%s, pid:%d, cpu_mask:%x\n",p->comm,p->pid,p->cpus_mask);
		   }
		   #endif
		 }
		}
		while_each_thread(g, p);
		get_current_timestamp("dump_top5_task");
		for(i=0;i<5;i++){
			UserAvgLoad = task[i].DeltaUtime*100/delta_time;
			KernelAvgLoad = task[i].DeltaStime*100/delta_time;
			pr_info("Cpu usage:%d%% user + %d%% kernel, comm:%s, pid:%d, cpu_mask:%x, pcomm:%s, ppid:%d\n",
				UserAvgLoad,KernelAvgLoad,task[i].comm,task[i].pid,
				task[i].cpus_mask,task[i].pcomm,task[i].ppid);
		}
	}
	rcu_read_unlock();
	PrevKtime=ktime_get();
	rt_mutex_unlock(&pid_lock);
}


static void dump_cpu_info(void)
{
	static ktime_t last;
	u64 ms;
	int load = LOAD_INT(avenrun[0]);

	if (load < 3)
		return;

	ms = ktime_to_ms(ktime_sub(ktime_get(), last));
	if (ms < task_print_period_s * 1000)
		return;
	last = ktime_get();
	print_all_task_stack();
}

/*Print CPU info*/



static void power_info_dump_func(struct work_struct *work)
{
	pr_info("start \n");
	dump_active_wakeup_sources();
	dump_cpu_info();
	schedule_delayed_work(&power_info_dump,round_jiffies_relative(msecs_to_jiffies(wakelock_print_period_s*1000)));
}


static int __init powerdet_init(void)
{
	int result,error;
	pr_info("start \n");
	hash_init(hash_table);

	#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	sm.power_notifier.notifier_call = screen_state_for_power_callback;
	result = mi_disp_register_client(&sm.power_notifier);
	if (result < 0) {
		printk(KERN_ERR"power: register screen state callback failed\n");
	}
	#endif

	/*Dump power info*/
	INIT_DELAYED_WORK(&power_info_dump,power_info_dump_func);
	schedule_delayed_work(&power_info_dump,round_jiffies_relative(msecs_to_jiffies(wakelock_print_period_s*1000)));
	/*Dump power info*/
	xm_power_kobj = kobject_create_and_add("xm_power", NULL);
	if (!xm_power_kobj)
		return -ENOMEM;
	error = sysfs_create_groups(xm_power_kobj, attr_groups);
	if (error)
		return error;

	return 0;
}

static void __exit powerdet_exit(void)
{
	pr_info("powerdet_exit\n");

	#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	mi_disp_unregister_client(&sm.power_notifier);
	#endif
	cancel_delayed_work(&power_info_dump);
}


module_init(powerdet_init);
module_exit(powerdet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("XiaoMI inc.");
MODULE_DESCRIPTION("The Power detector");
