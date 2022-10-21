
#define pr_fmt(fmt)  "rtmm : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/sort.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/vmpressure.h>
#include <linux/hugetlb.h>
#include <linux/huge_mm.h>
#include <linux/freezer.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <asm/tlbflush.h>

#include "rtmm.h"


#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <linux/sched/mm.h>
#include <linux/sched/types.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <uapi/linux/sched/types.h>
#include <linux/pagewalk.h>
#endif

#define MB_TO_PAGES(m)  ((m) << (20 - PAGE_SHIFT))
#define PAGES_TO_MB(p)   ((p) >> (20 -PAGE_SHIFT))

#define DEFAULT_AUTO_RECLAIM_MAX 500   /* MB */
#define DEFAULT_GLOBAL_RECLAIM_MAX 1024  /* MB */
#define DEFAULT_RECLAIM_SWAPPINESS 150

#define INVALID_RECLAIM_SWAPPINESS -1

enum reclaim_type {
	RECLAIM_NONE = 0,
	RECLAIM_KILL,
	RECLAIM_PROC,
	RECLAIM_GLOBAL, /* global lru*/
	RECLAIM_AUTO,
	RECLAIM_TYPE_NR,
};

enum page_type {
	RECLAIM_PAGE_NONE = 0,
	RECLAIM_PAGE_FILE,
	RECLAIM_PAGE_ANON,
	RECLAIM_PAGE_ALL, /* anon/file pages */
};

static const char *stat_str[RECLAIM_TYPE_NR] = {
	[RECLAIM_NONE] = "",
	[RECLAIM_KILL] = "kill",
	[RECLAIM_PROC] = "proc reclaim",
	[RECLAIM_GLOBAL] = "global reclaim",
	[RECLAIM_AUTO] = "auto reclaim",
};

#define CMD_STR_LEN 64

struct reclaim_cmd {
	/* proc reclaim; global reclaim ; kill */
	int type;

	/* proc reclaim ; kill*/
	pid_t pid;

	int page_type; /* anon/file */

	/* global reclaim */
	int order;
	int nr_to_reclaim;

	int swappiness;

	struct list_head list;
};

struct rtmm_reclaim {
	struct task_struct *tsk;
	wait_queue_head_t waitqueue;

	spinlock_t lock;
	struct list_head cmd_todo;
	atomic_t cmd_nr;

	/* we do not have limit of below parameters, be CAUTIOUS !!!!! */
	int auto_reclaim_max;
	int global_reclaim_max;
	int default_swappiness;

	int reclaim_swappiness;

	struct kobject kobj;

	struct {
		/* last reclaimed */
		pid_t pid;
		int last_reclaimed; /* pages */

		/* total reclaim */
		int total_reclaimed; /* pages */
		int cnt;
		int fail_cnt;
		int ignore_cnt;
	} stat[RECLAIM_TYPE_NR];
};

struct rtmm_reclaim __reclaim;

static inline struct rtmm_reclaim *get_reclaim(void)
{
	return &__reclaim;
}

/*
  * kill a process
  */
static int mem_process_kill(pid_t pid)
{
	struct task_struct *task;
	struct signal_struct *sig;
	struct mm_struct *mm;
	int oom_score_adj;
	unsigned long rss = 0;
	int ret = 0;

	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (!task) {
		ret = -ESRCH;
		goto out;
	}
	mm = task->mm;
	sig = task->signal;
	if (!mm || !sig) {
		ret = -ESRCH;
		goto out;
	}

	oom_score_adj = sig->oom_score_adj;
	if (oom_score_adj < 0) {
		pr_warn("KILL: odj %d, exit\n", oom_score_adj);
		ret = -EPERM;
		goto out;
	}

	rss = get_mm_rss(mm);

	pr_info("kill process %d(%s)\n", pid, task->comm);
	send_sig(SIGKILL, task, 0);
out:
	rcu_read_unlock();

	return (ret < 0) ? ret : rss;
}

#ifdef CONFIG_PROCESS_RECLAIM
static struct task_struct * find_get_task_by_pid(pid_t pid)
{
	struct task_struct *p;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p)
		get_task_struct(p);

	rcu_read_unlock();

	return p;
}

/*
  * reclaim anon/file pages of a process
  */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static int mem_process_reclaim(pid_t pid, int type, int nr_to_reclaim)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct rtmm_reclaim_proc rr;
	int ret = 0;
	const struct mm_walk_ops reclaim_walk_ops = {
		.pmd_entry = rtmm_reclaim_pte_range,
	};

	task = find_get_task_by_pid(pid);
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	if (!mm) {
		ret = -EINVAL;
		goto out;
	}

	down_read(&mm->mmap_sem);

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (is_vm_hugetlb_page(vma))
			continue;

		if (type == RECLAIM_PAGE_ANON && vma->vm_file)
			continue;

		if (type == RECLAIM_PAGE_FILE && !vma->vm_file)
			continue;

		rr.vma = vma;
		ret = walk_page_range(mm, vma->vm_start, vma->vm_end,
			&reclaim_walk_ops, &rr);
		if (ret)
			break;
	}

	flush_tlb_mm(mm);
	up_read(&mm->mmap_sem);

	mmput(mm);

out:
	put_task_struct(task);

	/*  -EPIPE is returned in reclaim_pte_range() to break the iteration,
	  *  it is a valid value, so we set the ret as 0 here.
	  */
	if (ret == -EPIPE)
		ret = 0;

	return rr.nr_reclaimed;
}
#else
static int mem_process_reclaim(pid_t pid, int type, int nr_to_reclaim)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct mm_walk reclaim_walk = {};
	struct reclaim_param rp;
	int ret = 0;

	task = find_get_task_by_pid(pid);
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	if (!mm) {
		ret = -EINVAL;
		goto out;
	}

	reclaim_walk.mm = mm;
	reclaim_walk.pmd_entry = reclaim_pte_range;

	rp.nr_scanned = 0;
	rp.nr_to_reclaim = nr_to_reclaim;
	rp.nr_reclaimed = 0;
	reclaim_walk.private = &rp;

	down_read(&mm->mmap_sem);

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (is_vm_hugetlb_page(vma))
			continue;

		if (type == RECLAIM_PAGE_ANON && vma->vm_file)
			continue;

		if (type == RECLAIM_PAGE_FILE && !vma->vm_file)
			continue;

		rp.vma = vma;
		ret = walk_page_range(vma->vm_start, vma->vm_end,
				&reclaim_walk);
		if (ret)
			break;
	}

	flush_tlb_mm(mm);
	up_read(&mm->mmap_sem);

	mmput(mm);

out:
	put_task_struct(task);

	/*  -EPIPE is returned in reclaim_pte_range() to break the iteration,
	 *  it is a valid value, so we set the ret as 0 here.
	 */
	if (ret == -EPIPE)
		ret = 0;

	pr_info("process reclaim: pid %d, page_type %d, try to reclaim %d, reclaimed %d(scan %d)\n",
			pid, type, nr_to_reclaim, rp.nr_reclaimed, rp.nr_scanned);

	return rp.nr_reclaimed;
}
#endif
#else
static int mem_process_reclaim(pid_t pid, int type, int nr_to_reclaim)
{
	return 0;
}
#endif /* CONFIG_PROCESS_RECLAIM */

static int mem_global_reclaim(unsigned long nr_to_reclaim, int swappiness)
{
#define RECLAIM_PAGES_PER_LOOP MB_TO_PAGES(1)

	unsigned long nr_reclaimed = 0;
	int loop = nr_to_reclaim / RECLAIM_PAGES_PER_LOOP;
	int remain = nr_to_reclaim % RECLAIM_PAGES_PER_LOOP;

	/* Currently we only consider loops instead of nr_to_reclaim */
	while (loop--) {
		nr_reclaimed += reclaim_global(RECLAIM_PAGES_PER_LOOP);

		if (nr_reclaimed >= nr_to_reclaim)
			break;
	}
	if (remain && (nr_reclaimed < nr_to_reclaim))
		nr_reclaimed += reclaim_global(remain);

	pr_info("global reclaim: try to reclaim %ld, reclaimed %ld, swappiness %d\n",
		nr_to_reclaim, nr_reclaimed, swappiness);

	return nr_reclaimed;
}

static struct reclaim_cmd *__alloc_reclaim_cmd(void)
{
	struct reclaim_cmd *cmd;

	cmd = kzalloc(sizeof(struct reclaim_cmd), GFP_KERNEL);
	if (!cmd) {
		pr_err("fail to get an empty slot\n");
		return NULL;
	}
	cmd->type = RECLAIM_NONE;
	INIT_LIST_HEAD(&cmd->list);

	return cmd;
}

static void __release_reclaim_cmd(struct reclaim_cmd *cmd)
{
	kfree(cmd);
}

static void __enqueue_reclaim_cmd(struct rtmm_reclaim *reclaim, struct reclaim_cmd *cmd)
{
	spin_lock(&reclaim->lock);
	list_add_tail(&cmd->list, &reclaim->cmd_todo);
	spin_unlock(&reclaim->lock);

	atomic_inc(&reclaim->cmd_nr);

	wake_up(&reclaim->waitqueue);

	return;
}

static struct reclaim_cmd *__dequeue_reclaim_cmd(struct rtmm_reclaim *reclaim)
{
	struct reclaim_cmd *cmd = NULL;

	spin_lock(&reclaim->lock);
	if (!list_empty(&reclaim->cmd_todo)) {
		cmd = list_first_entry(&reclaim->cmd_todo, struct reclaim_cmd,
					  list);
		list_del(&cmd->list);
	}
	spin_unlock(&reclaim->lock);

	atomic_dec(&reclaim->cmd_nr);

	return cmd;
}


static int mem_reclaim_thread(void *data)
{
	struct rtmm_reclaim *reclaim = data;
	struct sched_param param = { .sched_priority = 0 };
	int nr_reclaimed = 0;

	sched_setscheduler(current, SCHED_NORMAL, &param);

	while (true) {
		struct reclaim_cmd *cmd;

		wait_event_freezable(reclaim->waitqueue, (atomic_read(&reclaim->cmd_nr) > 0));

		while (atomic_read(&reclaim->cmd_nr) > 0) {
			cmd = __dequeue_reclaim_cmd(reclaim);
			if (!cmd && !(cmd->type > RECLAIM_NONE && cmd->type < RECLAIM_TYPE_NR))
				break;

			switch (cmd->type) {
			case RECLAIM_KILL:
				nr_reclaimed = mem_process_kill(cmd->pid);
				if(nr_reclaimed > 0)
					reclaim->stat[RECLAIM_KILL].pid = cmd->pid;
				break;

			case RECLAIM_PROC:
				nr_reclaimed = mem_process_reclaim(cmd->pid, cmd->page_type,
							cmd->nr_to_reclaim);
				if(nr_reclaimed > 0)
					reclaim->stat[RECLAIM_PROC].pid = cmd->pid;
				break;

			case RECLAIM_GLOBAL:
			case RECLAIM_AUTO:
				reclaim->reclaim_swappiness = cmd->swappiness;
				nr_reclaimed = mem_global_reclaim(cmd->nr_to_reclaim, cmd->swappiness);
				reclaim->reclaim_swappiness = INVALID_RECLAIM_SWAPPINESS;
				break;

			default:
				pr_err("unknown type %d", cmd->type);
				break;
			}

			if (nr_reclaimed < 0) {
				reclaim->stat[cmd->type].fail_cnt++;
			} else {
				reclaim->stat[cmd->type].last_reclaimed = nr_reclaimed;
				reclaim->stat[cmd->type].cnt++;
				reclaim->stat[cmd->type].total_reclaimed += nr_reclaimed;
			}

			__release_reclaim_cmd(cmd);
		}
	}

	return 0;
}


DEFINE_SYSFS_OPS(rtmm_reclaim, struct rtmm_reclaim);

#define RTMM_RECLAIM_ATTR_RO(_name) \
	static struct rtmm_reclaim_attribute _name##_attr = __ATTR_RO(_name)

#define RTMM_RECLAIM_ATTR_WO(_name) \
	static struct rtmm_reclaim_attribute _name##_attr = __ATTR_WO(_name)

#define RTMM_RECLAIM_ATTR_RW(_name) \
	static struct rtmm_reclaim_attribute _name##_attr = __ATTR_RW(_name)

static bool reclaim_input_parse(const char *buf, struct rtmm_reclaim *reclaim,
			int *target_free_size, int *swappiness)
{
	char *argbuf, *args, *arg;
	bool ret = false;

	args = argbuf = kstrndup(buf, 32, GFP_KERNEL);
	if (!args)
		return ret;

	arg = strsep(&args, ",");
	if (kstrtoint(arg, 10, target_free_size))
		goto err;

	arg = strsep(&args, ",");
	if (!arg)
		*swappiness = reclaim->default_swappiness;
	else {
		if (kstrtoint(arg, 10, swappiness))
			goto err;

		if (*swappiness < 0 || *swappiness > 200) {
			pr_err("auto reclaim: swappiness(%d) invalid, use default %d\n",
				*swappiness, reclaim->default_swappiness);
			*swappiness = reclaim->default_swappiness;
		}
	}

	ret = true;

err:
	kfree(argbuf);
	return ret;
}

static ssize_t auto_reclaim_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	struct reclaim_cmd *cmd;
	int target_free_size, swappiness;
	int cur_free, cur_free_size, reclaim_size;

	if (!reclaim_input_parse(buf, reclaim,
			&target_free_size, &swappiness)) {
		pr_err("rtmm auto reclaim: param err(%s)\n", buf);
		return -EINVAL;
	}

	if (target_free_size > reclaim->auto_reclaim_max) {
		pr_err("auto reclaim: size %d MB to big(> %d), exit\n",
			target_free_size, reclaim->auto_reclaim_max);
		return -EINVAL;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	cur_free = global_zone_page_state(NR_FREE_PAGES);
#else
	cur_free = global_page_state(NR_FREE_PAGES);
#endif

	cur_free_size = PAGES_TO_MB(cur_free);

	reclaim_size = target_free_size - cur_free_size;
	if (reclaim_size <= 0) {
		reclaim->stat[RECLAIM_AUTO].ignore_cnt++;
		return len;
	}

	/*
	pr_info("totalreserve_pages: %ld, cur_free %d(%d MB)\n",
			totalreserve_pages, cur_free, cur_free_size);
	*/

	cmd = __alloc_reclaim_cmd();
	if (!cmd) {
		pr_err("fail to get an empty slot in auto reclaim\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_AUTO;
	cmd->nr_to_reclaim = MB_TO_PAGES(reclaim_size);
	cmd->swappiness = swappiness;

	__enqueue_reclaim_cmd(reclaim, cmd);

	return len;
}
RTMM_RECLAIM_ATTR_WO(auto_reclaim);


static ssize_t auto_reclaim_max_show(struct rtmm_reclaim *reclaim, char *buf)
{
	return sprintf(buf, "%d\n", reclaim->auto_reclaim_max);
}

static ssize_t auto_reclaim_max_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	if (val < 0)
		return -EINVAL;

	reclaim->auto_reclaim_max = val;
	return len;
}
RTMM_RECLAIM_ATTR_RW(auto_reclaim_max);

static ssize_t default_reclaim_swappiness_show(struct rtmm_reclaim *reclaim, char *buf)
{
	return sprintf(buf, "%d\n", reclaim->default_swappiness);
}

static ssize_t default_reclaim_swappiness_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	if (val < 0 || val > 200)
		return -EINVAL;

	reclaim->default_swappiness = val;
	return len;
}
RTMM_RECLAIM_ATTR_RW(default_reclaim_swappiness);


static ssize_t global_reclaim_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	struct reclaim_cmd *cmd;
	int target_free_size, swappiness;

	if (!reclaim_input_parse(buf, reclaim,
			&target_free_size, &swappiness)) {
		pr_err("rtmm global reclaim: param err(%s)\n", buf);
		return -EINVAL;
	}

	if (target_free_size > reclaim->global_reclaim_max) {
		pr_err("global reclaim: size %d MB to big(> %d), exit\n",
			target_free_size, reclaim->global_reclaim_max);
		return -EINVAL;
	}

	cmd = __alloc_reclaim_cmd();
	if (!cmd) {
		pr_err("fail to get an empty slot in global reclaim\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_GLOBAL;
	cmd->nr_to_reclaim = MB_TO_PAGES(target_free_size);
	cmd->swappiness = swappiness;

	__enqueue_reclaim_cmd(reclaim, cmd);

	return len;
}
RTMM_RECLAIM_ATTR_WO(global_reclaim);


static ssize_t global_reclaim_max_show(struct rtmm_reclaim *reclaim, char *buf)
{
	return sprintf(buf, "%d\n", reclaim->global_reclaim_max);
}

static ssize_t global_reclaim_max_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	if (val < 0)
		return -EINVAL;

	reclaim->global_reclaim_max = val;
	return len;
}
RTMM_RECLAIM_ATTR_RW(global_reclaim_max);


static ssize_t kill_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	struct reclaim_cmd *cmd;
	int val, err;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;

	if (val < 0)
		return -EINVAL;

	cmd = __alloc_reclaim_cmd();
	if (!cmd) {
		pr_err("fail to get an empty slot in kill reclaim\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_KILL;
	cmd->pid = (pid_t)val;

	__enqueue_reclaim_cmd(reclaim, cmd);

	return len;
}
RTMM_RECLAIM_ATTR_WO(kill);


static ssize_t proc_reclaim_store(struct rtmm_reclaim *reclaim,
			const char *buf, size_t len)
{
	struct reclaim_cmd *cmd;
	int ret, page_type, size_MB;
	pid_t pid;

	ret = sscanf(buf, "%d %d %d", &pid, &page_type, &size_MB);
	if (ret != 3) {
		pr_err("param err in proc reclaim cmd(%s)\n", buf);
		return -EINVAL;
	}

	cmd = __alloc_reclaim_cmd();
	if (!cmd) {
		pr_err("fail to get an empty slot in proc reclaim\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_PROC;
	cmd->pid = pid;
	cmd->page_type = page_type;
	cmd->nr_to_reclaim = MB_TO_PAGES(size_MB);

	__enqueue_reclaim_cmd(reclaim, cmd);

	return len;
}
RTMM_RECLAIM_ATTR_WO(proc_reclaim);


static ssize_t stat_show(struct rtmm_reclaim *reclaim, char *buf)
{
	ssize_t ret = 0;
	int i;

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
		"  %-15s %11s %15s %5s %8s %12s %8s %14s %5s\n",
		"type", "total_count", "total_reclaimed", "(MB)",
		"fail_cnt", "ignore_count", "last_pid", "last_reclaimed", "(MB)");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "  ---------------\n");

	for (i = RECLAIM_KILL; i < RECLAIM_TYPE_NR; i++) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"  %-15s %11d %15d %5d %8d %12d %8d %14d %5d\n",
			stat_str[i],
			reclaim->stat[i].cnt, reclaim->stat[i].total_reclaimed,
			PAGES_TO_MB(reclaim->stat[i].total_reclaimed),
			reclaim->stat[i].fail_cnt, reclaim->stat[i].ignore_cnt,
			reclaim->stat[i].pid, reclaim->stat[i].last_reclaimed,
			PAGES_TO_MB(reclaim->stat[i].last_reclaimed));
	}

	return ret;
}
RTMM_RECLAIM_ATTR_RO(stat);

static struct attribute *rtmm_reclaim_attr[] = {
	&auto_reclaim_attr.attr,
	&auto_reclaim_max_attr.attr,
	&default_reclaim_swappiness_attr.attr,
	&global_reclaim_attr.attr,
	&global_reclaim_max_attr.attr,
	&kill_attr.attr,
	&proc_reclaim_attr.attr,
	&stat_attr.attr,
	NULL,
};

static struct kobj_type rtmm_reclaim_ktype = {
	.sysfs_ops = &rtmm_reclaim_sysfs_ops,
	.default_attrs = rtmm_reclaim_attr,
};

static void __init reclaim_init(struct rtmm_reclaim *reclaim)
{
	spin_lock_init(&reclaim->lock);
	init_waitqueue_head(&reclaim->waitqueue);

	atomic_set(&reclaim->cmd_nr, 0);
	INIT_LIST_HEAD(&reclaim->cmd_todo);

	reclaim->auto_reclaim_max = DEFAULT_AUTO_RECLAIM_MAX;
	reclaim->global_reclaim_max = DEFAULT_GLOBAL_RECLAIM_MAX;
	reclaim->default_swappiness = DEFAULT_RECLAIM_SWAPPINESS;
	reclaim->reclaim_swappiness = INVALID_RECLAIM_SWAPPINESS;
}

bool rtmm_reclaim(const char *name)
{
	return strncmp("rtmm_reclaim", name, strlen("rtmm_reclaim")) == 0;
}

int rtmm_reclaim_swappiness(void)
{
	struct rtmm_reclaim *reclaim = get_reclaim();
	int swappiness = reclaim->default_swappiness;

	if (reclaim->reclaim_swappiness != INVALID_RECLAIM_SWAPPINESS)
		swappiness = reclaim->reclaim_swappiness;

	return (int)swappiness;
}

int __init rtmm_reclaim_init(struct kobject *rtmm_kobj)
{
	struct rtmm_reclaim *reclaim = get_reclaim();
	int err;

	memset(reclaim, 0, sizeof(struct rtmm_reclaim));

	reclaim_init(reclaim);

	err = kobject_init_and_add(&reclaim->kobj, &rtmm_reclaim_ktype,
					rtmm_kobj, "reclaim");
	if (err) {
		kobject_put(&reclaim->kobj);
		pr_err("failed to initialize the sysfs interface of rtmm reclaim\n");
		return -ENOMEM;
	}

	reclaim->tsk = kthread_run(mem_reclaim_thread, reclaim, "rtmm_reclaim");
	if (IS_ERR(reclaim->tsk)) {
		pr_err("%s: creating thread for rtmm reclaim failed\n",
		       __func__);
		return PTR_ERR_OR_ZERO(reclaim->tsk);
	}

	pr_info("rtmm reclaim init OK\n");

	return 0;
}



