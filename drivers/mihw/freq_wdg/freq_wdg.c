#define pr_fmt(fmt) "fqos_wdg: " fmt

#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/export.h>
#include <trace/hooks/power.h>
#include "freq_wdg.h"

struct mi_freq_qos_wdg {
	int index;
	int pid;
	int uid;
	int last_state;
	bool valid;
	int count;
	unsigned int type;
	unsigned long last_update;
	unsigned long bite_jiffies;
	unsigned long enqueue_time;
	int default_value;
	char owner[TASK_COMM_LEN];
	struct delayed_work dwork;
	struct freq_qos_request *req;
};

static struct mi_freq_qos_wdg mi_fqos_dogs[MAX_FREQ_QOS_REQ];
static struct proc_dir_entry *fqos_entry;
static struct workqueue_struct *fqos_wdg_wq;
static int fqos_enable;
module_param(fqos_enable, uint, 0644);
static bool fqos_init_suc = false;
static int mi_fqos_wdgs;
static unsigned int fqos_wdg_timeout = 10000;
module_param(fqos_wdg_timeout, uint, 0644);
static int fqos_debug;
module_param(fqos_debug, uint, 0644);
static DEFINE_SPINLOCK(mi_fqos_lock);

static int find_mi_freq_qos_wdg(struct freq_qos_request *req)
{
	int i;

	for (i = 0; i < mi_fqos_wdgs; i++)
		if (mi_fqos_dogs[i].req == req)
			return i;

	return MAX_FREQ_QOS_REQ;
}

static int get_reqcpu_freq(struct freq_constraints *con,
					enum freq_qos_req_type type)
{
	struct cpufreq_policy *policy;
	int value;

	policy = container_of(con, struct cpufreq_policy, constraints);
	if (unlikely(!policy))
		return GET_FREQ_ERR;

	rcu_read_lock();
	switch (type){
		case FREQ_QOS_MIN:
			value = policy->cpuinfo.min_freq;
			break;
		case FREQ_QOS_MAX:
			value = policy->cpuinfo.max_freq;
			break;
		default:
			break;
	}
	rcu_read_unlock();

	return value;
}

static void do_update_timeout_request(struct work_struct *work)
{
	struct delayed_work *d_work = to_delayed_work(work);
	struct mi_freq_qos_wdg *mi_wdg = container_of(d_work,
					struct mi_freq_qos_wdg, dwork);
	unsigned long curjiff = jiffies;
	unsigned long timeout;

	if (unlikely(fqos_debug))
		pr_info("index : %d, last_update : %ld, enqueue_time : %ld",
				mi_wdg->index, mi_wdg->last_update,
						mi_wdg->enqueue_time);

	timeout = fqos_wdg_timeout - (curjiff - mi_wdg->last_update);
	if (!(mi_wdg->last_update == mi_wdg->enqueue_time)){
		if (mi_wdg->last_state){
			if(!delayed_work_pending(&mi_wdg->dwork)){
				mi_wdg->enqueue_time = mi_wdg->last_update;
				queue_delayed_work(fqos_wdg_wq,
							&(mi_wdg->dwork),
								timeout);
			}
		}
	}

	mi_wdg->count ++;

	if (unlikely(fqos_debug))
		pr_info("mi_wdg->count : %d", mi_wdg->count);
}

static int insert_mi_fqos_dogs(struct freq_qos_request *req, int value)
{
	int i, uid;
	struct mi_freq_qos_wdg *mi_wdg;

	i = find_mi_freq_qos_wdg(req);
	if (i < MAX_FREQ_QOS_REQ)
		return i;

	if (unlikely(mi_fqos_wdgs >= MAX_FREQ_QOS_REQ)) {
		pr_err("too many fqos wdgs\n");
		return MAX_FREQ_QOS_REQ;
	}

	for (i = 0; i < mi_fqos_wdgs; i++) {
		if (!mi_fqos_dogs[i].valid)
			goto find_suc;
	}

	i = mi_fqos_wdgs;
	mi_fqos_wdgs++;

find_suc:
	mi_wdg = &mi_fqos_dogs[i];
	mi_wdg->valid = true;
	mi_wdg->req = req;
	mi_wdg->index = i;
	mi_wdg->pid = current->pid;
	uid = from_kuid(&init_user_ns, current_uid());
	uid %= MAX_UID_VALUE;
	mi_wdg->uid = uid;
	mi_wdg->last_state = 0;
	mi_wdg->default_value = value;
	mi_wdg->count = 0;
	mi_wdg->last_update = 0;
	mi_wdg->enqueue_time = 0;
	INIT_DELAYED_WORK(&(mi_wdg->dwork),
				do_update_timeout_request);
	memset(mi_wdg->owner, 0, TASK_COMM_LEN);

	if (unlikely(fqos_debug))
		pr_info("insert_mi_fqos_dogs->i : %d,", i);

	return i;
}

static void remove_mi_fqos_dogs(struct freq_qos_request *req)
{
	int index = find_mi_freq_qos_wdg(req);

	if (index < MAX_FREQ_QOS_REQ) {
		struct mi_freq_qos_wdg *mi_wdg;
		mi_wdg= &mi_fqos_dogs[index];
		mi_wdg->valid = false;
		if(delayed_work_pending(&mi_wdg->dwork))
			cancel_delayed_work(&mi_wdg->dwork);

		mi_wdg->req = NULL;
		mi_wdg->default_value = 0;
		memset(mi_wdg->owner, 0, TASK_COMM_LEN);
	}

	if (index == mi_fqos_wdgs - 1)
		mi_fqos_wdgs --;
}

static void print_mi_req_owner(struct freq_qos_request *req)
{
	struct task_struct *owner;
	struct mi_freq_qos_wdg *mi_wdg;

	if (!req)
		return;

	mi_wdg = container_of(&req, struct mi_freq_qos_wdg, req);
	if (unlikely(!mi_wdg))
		return;

	rcu_read_lock();
	owner = find_task_by_vpid(mi_wdg->pid);
	if (owner)
		pr_info("\n comm %s | ", owner->comm);
	else
		pr_info("\n comm %s | ", "Deadtask");
	rcu_read_unlock();

	pr_info("pid: %d \n", mi_wdg->pid);
	pr_info("uid: %d\n", mi_wdg->uid);
	pr_info("value: %d\n", (req->pnode).prio);
	pr_info("type: %s\n", (mi_wdg->type ? "max" : "min"));
	pr_info("update_time: %d\n", mi_wdg->last_update);
}

static inline void resched_fqos_work(struct mi_freq_qos_wdg *mi_wdg,
		unsigned long tm)
{
	/* request high freq again before timeoutms */
	if (delayed_work_pending(&mi_wdg->dwork))
		cancel_delayed_work(&(mi_wdg->dwork));

	mi_wdg->enqueue_time = tm;
	queue_delayed_work(fqos_wdg_wq,
			&(mi_wdg->dwork),
			fqos_wdg_timeout);
}

static inline void sched_fqos_work(struct mi_freq_qos_wdg *mi_wdg,
		unsigned long tm)
{
	if (!delayed_work_pending(&mi_wdg->dwork)) {
		mi_wdg->enqueue_time = tm;
		queue_delayed_work(fqos_wdg_wq, &(mi_wdg->dwork),
				fqos_wdg_timeout);
	}
}

static inline void cancel_fqos_work(struct mi_freq_qos_wdg *mi_wdg)
{
	if (delayed_work_pending(&mi_wdg->dwork))
		cancel_delayed_work(&(mi_wdg->dwork));
}

static void mi_freq_qos_update_req(void *nouse, struct freq_qos_request *req,
		int value)
{
	int uid, index, default_value = 0;
	struct mi_freq_qos_wdg *mi_wdg;
	unsigned long flags, delta, cur_jiffies;
	struct mi_freq_qos *fqos = (struct mi_freq_qos *)req->android_oem_data1;

	 if (unlikely(!fqos_init_suc) || !fqos_enable)
		return;

	/* req not exist in mi_freq_wdg */
	if (unlikely (!(req->android_oem_data1[0] == USED_MI_FREQ_QOS))) {
		if (unlikely(fqos_debug))
			pr_info("pid : %d first update", current->pid);

		default_value = get_reqcpu_freq(req->qos, req->type);
		if (default_value == GET_FREQ_ERR)
			return;

		spin_lock_irqsave(&mi_fqos_lock, flags);
		index= insert_mi_fqos_dogs(req, default_value);
		fqos->wdg = index;
		req->android_oem_data1[0] = USED_MI_FREQ_QOS;
		spin_unlock_irqrestore(&mi_fqos_lock, flags);

		if (index < MAX_FREQ_QOS_REQ) {
			mi_wdg = &mi_fqos_dogs[index];
			mi_wdg->type = !(req->type == FREQ_QOS_MIN);
			memcpy(mi_wdg->owner, current->comm, TASK_COMM_LEN);
		}
	}

	index = find_mi_freq_qos_wdg(req);
	if (unlikely (index >= MAX_FREQ_QOS_REQ))
		return;

	mi_wdg = &mi_fqos_dogs[index];
	if (unlikely(mi_wdg->pid != current->pid)) {
		mi_wdg->pid = current->pid;
		memcpy(mi_wdg->owner, current->comm, TASK_COMM_LEN);
		uid = from_kuid(&init_user_ns, current_uid());
		uid %= MAX_UID_VALUE;
		mi_wdg->uid = uid;
	}

	cur_jiffies = jiffies;
	switch (req->type) {
		case FREQ_QOS_MIN:
			/* The first time to update */
			if (!mi_wdg->last_update) {
				mi_wdg->last_update = cur_jiffies;
				mi_wdg->last_state =
					(value > default_value) ? 1 : 0;

				if (mi_wdg->last_state){
					mi_wdg->enqueue_time = cur_jiffies;
					queue_delayed_work(fqos_wdg_wq,
							&(mi_wdg->dwork),
							fqos_wdg_timeout);
				}
				break;
			}

			delta = cur_jiffies - mi_wdg->last_update;
			/* request high freq and the last_state
			 * is high freq too*/
			if (value > default_value) {
				if (mi_wdg->last_state &&
						(delta < fqos_wdg_timeout))
					resched_fqos_work(mi_wdg, cur_jiffies);

				/* request high freq and the last_state
				 * is reset freq */
				else
					sched_fqos_work(mi_wdg, cur_jiffies);
			} else {
				/* reset the freq */
				if (delta < fqos_wdg_timeout
						&& mi_wdg->last_state)
					/* reset by the requestor itself*/
					cancel_fqos_work(mi_wdg);
			}
			mi_wdg->last_update = cur_jiffies;
			mi_wdg->last_state = (value > default_value) ? 1:0;
			break;

		case FREQ_QOS_MAX:
			break;

		default:
			break;
	}
}

static void mi_freq_qos_add_req(void *nouse, struct freq_constraints *qos,
		struct freq_qos_request *req,
		enum freq_qos_req_type type, int value, int ret_ok)
{
	int index;
	unsigned long flags;
	struct mi_freq_qos *fqos = (struct mi_freq_qos *)req->android_oem_data1;

        if (unlikely(!fqos_init_suc) || !fqos_enable || (ret_ok < 0))
                return;

	value = get_reqcpu_freq(req->qos, req->type);

	spin_lock_irqsave(&mi_fqos_lock, flags);
	index= insert_mi_fqos_dogs(req, value);
	fqos->wdg = index;
	req->android_oem_data1[0] = USED_MI_FREQ_QOS;
	spin_unlock_irqrestore(&mi_fqos_lock, flags);

	if (index < MAX_FREQ_QOS_REQ) {
		mi_fqos_dogs[index].type = !(type == FREQ_QOS_MIN);
		memcpy(mi_fqos_dogs[index].owner,
				current->comm, TASK_COMM_LEN);
	}
}

static void mi_freq_qos_remove_req(void *nouse, struct freq_qos_request *req)
{
	unsigned long flags;
	struct mi_freq_qos *fqos = (struct mi_freq_qos *)req->android_oem_data1;

	if (unlikely(!fqos_init_suc) || !fqos_enable)
		return;

	spin_lock_irqsave(&mi_fqos_lock, flags);
	remove_mi_fqos_dogs(req);
	fqos->wdg = MAX_FREQ_QOS_REQ;
	req->android_oem_data1[0] = 0;
	spin_unlock_irqrestore(&mi_fqos_lock, flags);
}

/* unlocked internal variant */
static inline int freq_qos_get_value(struct pm_qos_constraints *c)
{
	if (plist_head_empty(&c->list))
		return c->no_constraint_value;

	switch (c->type) {
		case PM_QOS_MIN:
			return plist_first(&c->list)->prio;
		case PM_QOS_MAX:
			return plist_last(&c->list)->prio;
		default:
			/* runtime check for not using enum */
			BUG();
			return PM_QOS_DEFAULT_VALUE;
	}
}

static int freq_qos_debug_show(struct seq_file *s, void *unused)
{
#define MAX_PM_QOS_OBJECT_PER_POLICY	2
	struct pm_qos_constraints *min_freq, *max_freq, *c;
	struct pm_qos_constraints *array[MAX_PM_QOS_OBJECT_PER_POLICY];
	struct freq_qos_request *req = NULL;
	struct mi_freq_qos_wdg *mi_wdg;
	char *type;
	unsigned long flags;
	int tot_reqs = 0;
	int active_reqs = 0;
	int cpu = 0, i, cluster = 0;
	int index;
	struct cpufreq_policy *policy;
	struct cpumask visit_cpumask;
	struct freq_constraints *qos;
	char *cluster_name[MAX_CLUSTER] = {"lit", "mid", "big"};
	struct mi_freq_qos *fqos;

	cpumask_clear(&visit_cpumask);
	cpus_read_lock();
	cpumask_copy(&visit_cpumask, cpu_online_mask);
	seq_printf(s, "id:      value: stat            ");
	seq_printf(s, "pid[   process name]");
	seq_printf(s, "    \t uid time");
	seq_printf(s, "       out times\n");
	for_each_cpu(cpu, &visit_cpumask) {
		policy = cpufreq_cpu_get(cpu);
		if (unlikely(!policy))
			continue;

		cpumask_andnot(&visit_cpumask,
			&visit_cpumask, policy->related_cpus);
		qos = &policy->constraints;
		if (IS_ERR_OR_NULL(qos)) {
			pr_err("%s: bad qos param!\n", __func__);
			cpufreq_cpu_put(policy);
			cpus_read_unlock();
			return -EINVAL;
		}

		min_freq = &qos->min_freq;
		max_freq = &qos->max_freq;
		if (IS_ERR_OR_NULL(min_freq)
				|| IS_ERR_OR_NULL(max_freq)) {
			pr_err("%s: Bad constraints on qos?\n", __func__);
			cpufreq_cpu_put(policy);
			cpus_read_unlock();
			return -EINVAL;
		}

		if (plist_head_empty(&min_freq->list)
				|| plist_head_empty(&max_freq->list)) {
			seq_puts(s, "Empty!\n");
			cpufreq_cpu_put(policy);
			continue;
		}

		/* Lock to ensure we have a snapshot */
		spin_lock_irqsave(&mi_fqos_lock, flags);
		array[0] = min_freq;
		array[1] = max_freq;
		seq_printf(s, "---------------%s------------\n",
				cluster_name[cluster]);
		cluster ++;
		cluster %= MAX_CLUSTER;
		for (i = 0; i < MAX_PM_QOS_OBJECT_PER_POLICY; i++) {
			c = array[i];
			switch (c->type) {
			case PM_QOS_MIN:
				type = "Max_limit";
				break;
			case PM_QOS_MAX:
				type = "Min_limit";
				break;
			default:
				type = "Unknown";
			}
			active_reqs = 0;
			tot_reqs = 0;
			plist_for_each_entry(req, &c->list, pnode) {
				char *state = "Def";
				bool print_owner = false;
				fqos = (struct mi_freq_qos *)req->android_oem_data1;

				index = fqos->wdg;
				if (index < MAX_FREQ_QOS_REQ)
					print_owner = true;
				else
					continue;

				if (unlikely (!(req->android_oem_data1[0]
							== USED_MI_FREQ_QOS)))
					continue;

				mi_wdg = &mi_fqos_dogs[index];

				tot_reqs++;
				if ((req->pnode).prio == c->default_value)
					goto print;

				active_reqs++;
				state = "Act";
print:				seq_printf(s, "%2d: %10d: %3s %15d[%15s] %15d %10lu %8d\n",
					tot_reqs, (req->pnode).prio,
					state, mi_wdg->pid,
					(print_owner ? mi_wdg->owner :
					 "Unknow"),
					mi_wdg->uid,
					(jiffies - mi_wdg->last_update) / HZ,
					mi_wdg->count);
			}
			seq_printf(s, "Type=%s, Value=%d, Requests: active=%d / total=%d\n\n",
					type, freq_qos_get_value(c), active_reqs, tot_reqs);
		}
		spin_unlock_irqrestore(&mi_fqos_lock, flags);
		cpufreq_cpu_put(policy);
	}
	cpus_read_unlock();
	return 0;
}
DEFINE_PROC_SHOW_ATTRIBUTE(freq_qos_debug);

void fqos_wdg_update_bite_timeout(struct freq_qos_request *req, unsigned long new_time)
{
	int index;
	if (new_time <= 0)
		new_time = 0;
	index = find_mi_freq_qos_wdg(req);
	if (likely(index < MAX_FREQ_QOS_REQ))
		mi_fqos_dogs[index].bite_jiffies = new_time;
	else
		return;
	pr_info("%s\n");
	print_mi_req_owner(req);
	pr_info("change new_time to %lu\n", new_time);
}
EXPORT_SYMBOL_GPL(fqos_wdg_update_bite_timeout);

static int __init freq_qos_init(void)
{
	pr_info("wdg is start");
	fqos_wdg_wq = alloc_workqueue("fqos_wdg_wq", WQ_HIGHPRI, 0);

	if (!fqos_wdg_wq){
		return -EFAULT;
	}

	register_trace_android_vh_freq_qos_add_request(mi_freq_qos_add_req, NULL);
	register_trace_android_vh_freq_qos_update_request(mi_freq_qos_update_req, NULL);
	register_trace_android_vh_freq_qos_remove_request(mi_freq_qos_remove_req, NULL);

	fqos_entry = proc_create("show_mi_freq_qos", 0664,
		NULL, &freq_qos_debug_proc_ops);

	fqos_init_suc = true;

	return 0;
}

static void __exit freq_qos_exit(void)
{
	fqos_init_suc = false;
	if (fqos_entry)
		proc_remove(fqos_entry);
	fqos_entry = NULL;
	printk(KERN_ERR "in %s\n", __func__);
}
module_init(freq_qos_init);
module_exit(freq_qos_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Freq-qos watchdog developed by David");
