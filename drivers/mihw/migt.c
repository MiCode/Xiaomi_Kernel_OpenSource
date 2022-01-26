#define pr_fmt(fmt) "migt: " fmt
#include <linux/jiffies.h>
#include <linux/cred.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#define VIP_REQ_LIMIT  3
#define BOOST_MAXTIME_LIMIT (60 * HZ)
#define DEFAULT_BOOST_MINTIME 30
#define min_value(x, y)  (x < y ? x : y)
#define mi_time_after(a,b)	\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((b) - (a)) < 0))
#define mi_time_before(a,b) mi_time_after(b,a)

unsigned int mi_viptask[VIP_REQ_LIMIT];
static int migt_debug;
static int boost_task_pid;
static struct ctl_table_header *migt_sched_header;
static atomic_t mi_dynamic_vip_num = ATOMIC_INIT(0);

static void inline set_mi_vip_task(struct task_struct *p, unsigned int jiff)
{
        if (p) {
                p->pkg.migt.boost_end = jiffies + jiff;
                if (!(p->pkg.migt.flag & MASK_MI_VTASK))
                        atomic_inc(&mi_dynamic_vip_num);
                p->pkg.migt.flag |= MASK_MI_VTASK;
        }
}

static void inline clean_mi_vip_task(struct task_struct *p)
{
        unsigned long boost_end = p->pkg.migt.boost_end;
        if (mi_time_after(jiffies, boost_end)) {
                if (migt_debug)
                        pr_info("clean vip flag %d, %s, time %lu %lu to %lu\n",
                                p->pid, p->comm, jiffies, boost_end,
                                p->pkg.migt.boost_end);
                atomic_dec(&mi_dynamic_vip_num);
                p->pkg.migt.flag &= (~MASK_MI_VTASK);
        }
}

static int proc_boost_mi_task(struct ctl_table *table, int write,
                void __user *buffer, size_t *lenp, loff_t *ppos)
{
        int ret = proc_dointvec(table, write,
                        buffer, lenp, ppos);
        struct task_struct *target;
        rcu_read_lock();
        target = find_task_by_vpid(boost_task_pid);
        if (unlikely(!target)) {
                rcu_read_unlock();
                pr_err("Invalid input %d, no such process\n", boost_task_pid);
                return 0;
        }
        get_task_struct(target);
        pr_info("%d, %s set as mi vip task from %u", boost_task_pid,
                        target->comm, jiffies);
        rcu_read_unlock();
        set_mi_vip_task(target, HZ);
        put_task_struct(target);
        boost_task_pid = -1;
        return ret;
}

void mi_vip_task_req(int *pid, unsigned int nr, unsigned int jiff)
{
        int i, task_pid;
        struct task_struct *target;
#define MAX_MI_VIP_REQ 5
        if (nr > MAX_MI_VIP_REQ) {
                pr_err("req too many vip tasks\n");
                return;
        }
        if (!pid)
                return;
        jiff = min_value(BOOST_MAXTIME_LIMIT, jiff);
        if (unlikely(!jiff))
                jiff = DEFAULT_BOOST_MINTIME;
        for (i = 0; i < nr; i++) {
                rcu_read_lock();
                task_pid = pid[i];
                target = find_task_by_vpid(task_pid);
                if (unlikely(!target)) {
                        rcu_read_unlock();
                        pr_err("Invalid input %d, no such process\n", task_pid);
                        continue;
                }
                get_task_struct(target);
                pr_info("%d, %s set as mi vip task from %u to %u", task_pid,
                                target->comm, jiffies, jiffies + jiff);
                rcu_read_unlock();
                set_mi_vip_task(target, jiff);
                put_task_struct(target);
        }
}

int get_mi_dynamic_vip_num(void)
{
        return atomic_read(&mi_dynamic_vip_num);
}

static int set_mi_vip_task_req(const char *buf, const struct kernel_param *kp)
{
	int i, len, ntokens = 0;
	unsigned int val;
	int num = 0;
	unsigned int times = 0;
	const char *cp = buf;
	while ((cp = strpbrk(cp + 1, ":"))) {
		ntokens++;
	}
	len = strlen(buf);
	if (!ntokens) {
		if (sscanf(buf, "%u-%u\n", &val, &times) != 2)
			return -EINVAL;
		pr_info("val %d times %d\n", val, times);
		mi_vip_task_req(&val, 1, times);
		return 0;
	}
	cp = buf;
	for (i = 0; i < ntokens; i ++) {
		if (sscanf(cp, "%u", &val) != 1)
			return -EINVAL;
		mi_viptask[num++] = val;
		pr_info("arg %d val %d\n", num, val);
		cp = strpbrk(cp + 1, ":");
		cp ++;
		if ((cp >= buf + len))
			return 0;
		if (num >= VIP_REQ_LIMIT) {
			cp = strpbrk(cp + 1, "-");
			cp ++;
			if ((cp >= buf + len))
				 return 0;
			if (sscanf(cp, "%u", &times) != 1)
				return -EINVAL;
			pr_info("arg %d times %d\n", num, times);
			mi_vip_task_req(mi_viptask, num, times);
			return 0;
		}
	}
	if (cp < buf + len) {
		if (sscanf(cp, "%u-%u", &val, &times) != 2)
			return  -EINVAL;
		mi_viptask[num++] = val;
		pr_info("arg %d val = %d times = %d\n",
				num, val, times);
	}
	mi_vip_task_req(mi_viptask, num, times);
	return 0;
}
static int get_mi_viptask(char *buf, const struct kernel_param *kp)
{
	int i, cnt = 0;
	for (i = 0; i < VIP_REQ_LIMIT; i++)
		cnt += snprintf(buf + cnt,
			PAGE_SIZE - cnt, "%u:%d ",
			mi_viptask[i], get_mi_dynamic_vip_num());
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}
static const struct kernel_param_ops param_ops_mi_viptask = {
	.set = set_mi_vip_task_req,
	.get = get_mi_viptask,
};
module_param_cb(mi_viptask, &param_ops_mi_viptask, NULL, 0644);

void migt_monitor_init(struct task_struct *p)
{
	p->pkg.migt.flag        = MIGT_NORMAL_TASK;
	p->pkg.migt.boost_end = 0;
}
EXPORT_SYMBOL(migt_monitor_init);

static struct ctl_table migt_table[] = {
	{
		.procname       = "boost_pid",
		.data           = &boost_task_pid,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_boost_mi_task,
	},
	{
		.procname       = "migt_sched_debug",
		.data           = &migt_debug,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec,
	},
	{}
};

static struct ctl_table migt_ctl_root[] = {
	{
		.procname	= "migt",
		.mode		= 0555,
		.child	= migt_table,
	},
	{}
};

static int __init migt_sched_init(void)
{
	pr_info("migt_sched %s: inited!\n", __func__);
	migt_sched_header = register_sysctl_table(migt_ctl_root);
	return 0;
}

static void __exit migt_sched_exit(void)
{
	unregister_sysctl_table(migt_sched_header);
	migt_sched_header = NULL;
}

int game_vip_task(struct task_struct *p)
{
	unsigned long boost_end = p->pkg.migt.boost_end;
	if (mi_time_before(jiffies, boost_end)) {
		if (migt_debug)
			pr_info("%d %s is mi vip task %d\n",
				p->pid, p->comm,
				p->pkg.migt.flag & MASK_MI_VTASK);
		return p->pkg.migt.flag & MASK_MI_VTASK;
	}
	else if (p->pkg.migt.flag & MASK_MI_VTASK)
		clean_mi_vip_task(p);

	return 0;
}

module_init(migt_sched_init);
module_exit(migt_sched_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("smart sched by Mi");
