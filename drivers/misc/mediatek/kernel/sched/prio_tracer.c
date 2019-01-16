/*
 * abbreviation of "pts_" stands for Priority TracerS.
 */
#define PRIORITY_TRACER "v0.1"

#ifdef CONFIG_MT_PRIO_TRACER

#include <linux/types.h>
#include <linux/prio_tracer.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/uaccess.h>

DEFINE_SPINLOCK(pts_lock);

static struct rb_root priority_tracers;
static struct dentry *pts_debugfs_dir_root;
static struct dentry *pts_debugfs_dir_proc;
static const struct file_operations pts_proc_fops;
static unsigned long pts_enable;

struct prio_set {
	int prio;
	int policy;
};

struct prio_tracer {
	pid_t tid;
	struct rb_node rb_node;
	struct dentry *debugfs_entry;

	unsigned int count_usr;
	unsigned int count_ker;
	unsigned int change_usr;
	unsigned int change_ker;
	struct prio_set ps[4];
	int prio_binder;
};

/*
 * copy from kernel/kernel/sched/core.c
 *
 * __normal_prio - return the priority that is based on the static prio
 */
static inline int __normal_prio(struct task_struct *p)
{
	return p->static_prio;
}

/**
 * select_set -
 *     tracer is designed to record the latest 2 priority setting from
 * syscall and kernel respectively. the record will be saved like the
 * followings. syscall uses the first 2, while kernel takes the rest 2.
 *
 *     MUST hold pts_lock.
 *
 * count:    1   2   3   4   5 ...
 *   set:   t1  t0  t1  t0  t1 ...
 *
 *       | next | latest
 *  ---------------------
 *   odd |  t0  |   t1
 *  even |  t1  |   t0
 *
 * @next:	2 meanings while set. one is to find the next to overwrite.
 *		and, the other is to point to the second latest.
 * @kernel:	0 for user, 1 for kernel. 2 for binder, but here we treat it
 *		as kernel does .
 */
static struct prio_set *select_set(struct prio_tracer *pt, int next, int kernel)
{
	int count = kernel ? pt->count_ker : pt->count_usr;
	int i;

	i = (((count % 2) == (!!next)) ? 0 : 1) + (kernel ? 2 : 0);
	return &pt->ps[i];
}

/**
 * query_prio_tracer -
 * note: called under @pts_lock protected
 */
static struct prio_tracer *query_prio_tracer(pid_t tid)
{
	struct rb_node *n = priority_tracers.rb_node;
	struct prio_tracer *pt;

	while (n) {
		pt = rb_entry(n, struct prio_tracer, rb_node);

		if (tid < pt->tid)
			n = n->rb_left;
		else if (tid > pt->tid)
			n = n->rb_right;
		else {
			return pt;
		}
	}
	return NULL;
}

/**
 * update_prio_tracer -
 * @prio:	equivalent to prio of task structure.
 */
void update_prio_tracer(pid_t tid, int prio, int policy, int kernel)
{
	struct prio_tracer *pt;
	struct prio_set *ps, *ps_latest;
	unsigned long flags;

	if (!pts_enable)
		return;

	spin_lock_irqsave(&pts_lock, flags);
	pt = query_prio_tracer(tid);
	if (!pt) {
		spin_unlock_irqrestore(&pts_lock, flags);
		return;
	}

	ps = select_set(pt, 1, kernel);
	ps->prio = prio;
	ps->policy = policy;

	ps_latest = select_set(pt, 0, kernel);
	if (ps_latest->prio != prio || ps_latest->policy != policy)
		kernel ? pt->change_ker++ : pt->change_usr++;

	kernel ? pt->count_ker++ : pt->count_usr++;

	/* binder priority inherit */
	if (kernel == PTS_BNDR)
		pt->prio_binder = prio;
	spin_unlock_irqrestore(&pts_lock, flags);
}

void create_prio_tracer(pid_t tid)
{
	struct rb_node **p = &priority_tracers.rb_node;
	struct rb_node *parent = NULL;
	struct prio_tracer *new_pt, *pt;
	struct dentry *d = NULL;
	unsigned long flags;
	int i;

	new_pt = kzalloc(sizeof(struct prio_tracer), GFP_KERNEL);
	if (!new_pt) {
		pr_err("%s: alloc failed\n", __func__);
		return;
	}

	if (pts_debugfs_dir_proc) {
		char strbuf[11];
		snprintf(strbuf, sizeof(strbuf), "%u", tid);
		/* debugfs involves mutex... */
		d = debugfs_create_file(strbuf,
					S_IRUGO, pts_debugfs_dir_proc, new_pt, &pts_proc_fops);
	}

	spin_lock_irqsave(&pts_lock, flags);
	while (*p) {
		parent = *p;
		pt = rb_entry(parent, struct prio_tracer, rb_node);

		if (tid < pt->tid)
			p = &(*p)->rb_left;
		else if (tid > pt->tid)
			p = &(*p)->rb_right;
		else {
			spin_unlock_irqrestore(&pts_lock, flags);
			debugfs_remove(d);
			kfree(new_pt);
			pr_debug("%s: find same pid\n", __func__);
			return;
		}
	}

	new_pt->tid = tid;
	for (i = 0; i < 4; i++)
		new_pt->ps[i].policy = -1;
	new_pt->prio_binder = PTS_DEFAULT_PRIO;
	new_pt->debugfs_entry = d;

	rb_link_node(&new_pt->rb_node, parent, p);
	rb_insert_color(&new_pt->rb_node, &priority_tracers);
	spin_unlock_irqrestore(&pts_lock, flags);
}

void delete_prio_tracer(pid_t tid)
{
	struct prio_tracer *pt;
	struct dentry *d;
	unsigned long flags;

	spin_lock_irqsave(&pts_lock, flags);
	pt = query_prio_tracer(tid);
	if (!pt) {
		spin_unlock_irqrestore(&pts_lock, flags);
		return;
	}
	d = pt->debugfs_entry;
	spin_unlock_irqrestore(&pts_lock, flags);

	/* debugfs involves mutex... */
	debugfs_remove(d);

	spin_lock_irqsave(&pts_lock, flags);
	rb_erase(&pt->rb_node, &priority_tracers);
	kfree(pt);
	spin_unlock_irqrestore(&pts_lock, flags);
}

void set_user_nice_syscall(struct task_struct *p, long nice)
{
	set_user_nice_core(p, nice);
	update_prio_tracer(task_pid_nr(p), NICE_TO_PRIO(nice), 0, PTS_USER);
}

void set_user_nice_binder(struct task_struct *p, long nice)
{
	set_user_nice_core(p, nice);
	update_prio_tracer(task_pid_nr(p), NICE_TO_PRIO(nice), 0, PTS_BNDR);
}

int sched_setscheduler_syscall(struct task_struct *p, int policy, const struct sched_param *param)
{
	int retval;

	retval = sched_setscheduler_core(p, policy, param);
	if (!retval) {
		int prio = param->sched_priority & ~MT_ALLOW_RT_PRIO_BIT;
		if (!rt_policy(policy))
			prio = __normal_prio(p);
		else
			prio = MAX_RT_PRIO - 1 - prio;
		update_prio_tracer(task_pid_nr(p), prio, policy, PTS_USER);
	}
	return retval;
}

int sched_setscheduler_nocheck_binder(struct task_struct *p, int policy,
				      const struct sched_param *param)
{
	int retval;

	retval = sched_setscheduler_nocheck_core(p, policy, param);
	if (!retval) {
		int prio = param->sched_priority & ~MT_ALLOW_RT_PRIO_BIT;
		if (!rt_policy(policy))
			prio = __normal_prio(p);
		else
			prio = MAX_RT_PRIO - 1 - prio;
		update_prio_tracer(task_pid_nr(p), prio, policy, PTS_BNDR);
	}
	return retval;
}

static void pts_proc_print(struct seq_file *m, struct prio_set *ps)
{
	int prio = ps->prio;

	if (ps->policy == -1) {
		seq_puts(m, "0 0 0 -1 ");
		return;
	}

	if (rt_prio(prio))
		seq_printf(m, "%d %d %d", (prio - MAX_RT_PRIO), 0, (MAX_RT_PRIO - 1 - prio));
	else
		seq_printf(m, "%d %d %d", USER_PRIO(prio), PRIO_TO_NICE(prio), 0);
	seq_printf(m, " %d ", ps->policy);
}

static int pts_proc_show(struct seq_file *m, void *unused)
{
	struct prio_tracer *pt = m->private;
	struct prio_set ps_copy[4];
	unsigned int count_usr, count_ker;
	unsigned int change_usr, change_ker;
	int prio_binder;
	int i, j;
	unsigned long flags;

	spin_lock_irqsave(&pts_lock, flags);
	count_usr = pt->count_usr;
	count_ker = pt->count_ker;
	change_usr = pt->change_usr;
	change_ker = pt->change_ker;
	prio_binder = pt->prio_binder;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			memcpy(&ps_copy[((i * 2) + j)], select_set(pt, j, i),
			       sizeof(struct prio_set));
		}
	}
	spin_unlock_irqrestore(&pts_lock, flags);

	seq_printf(m, "%u %u ", count_usr, change_usr);
	for (i = 0; i < 2; i++)
		pts_proc_print(m, &ps_copy[i]);

	seq_printf(m, " %u %u ", count_ker, change_ker);
	for (i = 2; i < 4; i++)
		pts_proc_print(m, &ps_copy[i]);

	if (prio_binder != PTS_DEFAULT_PRIO) {
		int tmp = prio_binder;
		prio_binder = rt_prio(tmp) ? (tmp - MAX_RT_PRIO) : USER_PRIO(tmp);
	}
	seq_printf(m, " %d\n", prio_binder);
	return 0;
}

static int pts_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pts_proc_show, inode->i_private);
}

static const struct file_operations pts_proc_fops = {
	.owner = THIS_MODULE,
	.open = pts_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pts_enable_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "%lu\n", pts_enable);
	return 0;
}

static int pts_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, pts_enable_show, inode->i_private);
}

static ssize_t pts_enable_write(struct file *flip, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[32];
	size_t copy_size = cnt;
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		copy_size = 32 - 1;
	buf[copy_size] = '\0';

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;

	ret = strict_strtoul(buf, 10, &val);
	pts_enable = !!val;
	pr_debug("%s: set %s", __func__, pts_enable ? "enable" : "disable");
	return cnt;
}

static const struct file_operations pts_enable_fops = {
	.owner = THIS_MODULE,
	.open = pts_enable_open,
	.read = seq_read,
	.write = pts_enable_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pts_utest_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "usage: echo $type $prio $tid\n"
		   "       $type 0 user / 1 kernel / 2 binder\n"
		   "       $prio\n"
		   "              H             L\n"
		   "         rt   |<----------->|\n"
		   "              0            98\n"
		   "       ((RT) 99             1)\n\n"
		   "                              H          L\n"
		   "      normal                  |<-------->|\n"
		   "                            100        139\n"
		   "                    ((nice) -20         19)\n");
	return 0;
}

static int pts_utest_open(struct inode *inode, struct file *file)
{
	return single_open(file, pts_utest_show, inode->i_private);
}

/* # echo @ut_type @ut_prio @ut_tid > utest */
static ssize_t pts_utest_write(struct file *flip, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[32];
	size_t copy_size = cnt;
	unsigned long val;
	int ut_type, ut_tid, ut_prio;
	int ret, i = 0, j;
	struct task_struct *p;


	if (cnt >= sizeof(buf))
		copy_size = 32 - 1;
	buf[copy_size] = '\0';

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;

	do {
	} while (buf[i++] != ' ');
	buf[(i - 1)] = '\0';
	ret = strict_strtoul(buf, 10, &val);
	ut_type = (int)val;

	j = i;
	do {
	} while (buf[i++] != ' ');
	buf[(i - 1)] = '\0';
	ret = strict_strtoul((const char *)(&buf[j]), 10, &val);
	ut_prio = (int)val;

	ret = strict_strtoul((const char *)(&buf[i]), 10, &val);
	ut_tid = (int)val;

	pr_debug("%s: unit test %s tid %d prio %d j %d i %d", __func__,
		 (ut_type == PTS_USER) ? "user" :
		 ((ut_type == PTS_KRNL) ? "kernel" :
		  ((ut_type == PTS_BNDR) ? "binder" : "unknown")), ut_tid, ut_prio, j, i);


	/* start to test api */
	p = find_task_by_vpid(ut_tid);
	if (!p)
		goto utest_out;

	if ((ut_prio >= 0) && (ut_prio < MAX_RT_PRIO)) {
		struct sched_param param;

		/* sched_priority is rt priority rather than effective one */
		ut_prio = MAX_RT_PRIO - 1 - ut_prio;
		param.sched_priority = ut_prio | MT_ALLOW_RT_PRIO_BIT;

		switch (ut_type) {
		case PTS_USER:
			sched_setscheduler_syscall(p, SCHED_RR, &param);
			break;
		case PTS_KRNL:
			sched_setscheduler_nocheck(p, SCHED_RR, &param);
			break;
		case PTS_BNDR:
			sched_setscheduler_nocheck_binder(p, SCHED_RR, &param);
			break;
		default:
			break;
		}
	} else {		/* assume normal */
		switch (ut_type) {
		case PTS_USER:
			set_user_nice_syscall(p, PRIO_TO_NICE(ut_prio));
			break;
		case PTS_KRNL:
			set_user_nice(p, PRIO_TO_NICE(ut_prio));
			break;
		case PTS_BNDR:
			set_user_nice_binder(p, PRIO_TO_NICE(ut_prio));
			break;
		default:
			break;
		}
	}

 utest_out:
	return cnt;
}

static const struct file_operations pts_utest_fops = {
	.owner = THIS_MODULE,
	.open = pts_utest_open,
	.read = seq_read,
	.write = pts_utest_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init prio_tracer_init(void)
{
	pts_debugfs_dir_root = debugfs_create_dir("prio_tracer", NULL);

	if (pts_debugfs_dir_root) {
		pts_debugfs_dir_proc = debugfs_create_dir("proc", pts_debugfs_dir_root);

		debugfs_create_file("enable",
				    (S_IRUGO | S_IWUSR | S_IWGRP),
				    pts_debugfs_dir_root, NULL, &pts_enable_fops);

		debugfs_create_file("utest",
				    (S_IRUGO | S_IWUSR | S_IWGRP),
				    pts_debugfs_dir_root, NULL, &pts_utest_fops);
	}

	/* if built-in, default on */
	pts_enable = 1;
	return 0;
}
device_initcall(prio_tracer_init);

MODULE_LICENSE("GPL v2");

#endif
