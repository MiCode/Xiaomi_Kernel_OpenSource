
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
#include <linux/debugfs.h>
#include <linux/kthread.h>

#include <asm/tlbflush.h>

#include <linux/ktrace.h>
#include <linux/rtmm.h>

#define MB_TO_PAGES(m)  (((m) << 20) >> PAGE_SHIFT)
#define PAGES_TO_MB(p) (((p) << PAGE_SHIFT) >> 20)

#define GLOBAL_RECLAIM_MAX_SIZE 200

enum reclaim_type {
	RECLAIM_NONE = 0,
	RECLAIM_KILL,
	RECLAIM_PROC,
	RECLAIM_GLOBAL,
};

enum page_type {
	RECLAIM_PAGE_NONE = 0,
	RECLAIM_PAGE_FILE,
	RECLAIM_PAGE_ANON,
	RECLAIM_PAGE_ALL,
};

#define CMD_STR_LEN 64

#define RECLAIM_CMD_NR 5
struct reclaim_cmd {

	int type;

	pid_t pid;

	int page_type;

	int order;

	int nr_to_reclaim;

	bool in_use;
	struct list_head list;
};

struct rtmm_reclaim {
	struct dentry *dir;

	struct task_struct *tsk;
	wait_queue_head_t waitqueue;

	spinlock_t lock;
	struct list_head cmd_todo;
	atomic_t cmd_nr;

	struct reclaim_cmd cmds[RECLAIM_CMD_NR];
};

struct rtmm_reclaim __reclaim;

static inline struct rtmm_reclaim *get_reclaim(void)
{
	return &__reclaim;
}

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

	return rss;
}

static struct task_struct *find_get_task_by_pid(pid_t pid)
{
	struct task_struct *p;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p)
		get_task_struct(p);

	rcu_read_unlock();

	return p;
}

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

	if (ret == -EPIPE)
		ret = 0;

	pr_info("process reclaim: pid %d, page_type %d, try to reclaim %d, reclaimed %d(scan %d)\n",
			pid, type, nr_to_reclaim, rp.nr_reclaimed, rp.nr_scanned);

	return ret;
}

static int mem_global_reclaim(unsigned long nr_to_reclaim)
{
#define RECLAIM_PAGES_PER_LOOP MB_TO_PAGES(1)

	unsigned long nr_reclaimed = 0;
	int loop = nr_to_reclaim / RECLAIM_PAGES_PER_LOOP;
	int remain = nr_to_reclaim % RECLAIM_PAGES_PER_LOOP;

	while (loop--) {
		nr_reclaimed += reclaim_global(RECLAIM_PAGES_PER_LOOP);

		if (nr_reclaimed >= nr_to_reclaim)
			break;
	}
	if (remain && (nr_reclaimed < nr_to_reclaim))
		nr_reclaimed += reclaim_global(remain);

	pr_info("global reclaim: try to reclaim %ld, reclaimed %ld\n",
			nr_to_reclaim, nr_reclaimed);

	return nr_reclaimed;
}

static void __init_reclaim_cmd(struct reclaim_cmd *cmd)
{
	memset(cmd, 0, sizeof(struct reclaim_cmd));

	cmd->type = RECLAIM_NONE;
	cmd->in_use = false;
	INIT_LIST_HEAD(&cmd->list);
}

static struct reclaim_cmd *__alloc_reclaim_cmd(struct rtmm_reclaim *reclaim)
{
	int i;
	struct reclaim_cmd *cmd;

	spin_lock(&reclaim->lock);

	for (i = 0; i < RECLAIM_CMD_NR; i++) {
		cmd = &reclaim->cmds[i];
		if (!cmd->in_use) {
			cmd->in_use = true;
			break;
		}
	}
	spin_unlock(&reclaim->lock);

	if (i == RECLAIM_CMD_NR)
		return NULL;
	else
		return cmd;
}

static void __release_reclaim_cmd(struct rtmm_reclaim *reclaim, struct reclaim_cmd *cmd)
{
	spin_lock(&reclaim->lock);

	__init_reclaim_cmd(cmd);

	spin_unlock(&reclaim->lock);
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

	sched_setscheduler(reclaim->tsk, SCHED_NORMAL, &param);

	while (true) {
		struct reclaim_cmd *cmd;

		wait_event_freezable(reclaim->waitqueue, (atomic_read(&reclaim->cmd_nr) > 0));

		while (atomic_read(&reclaim->cmd_nr) > 0) {
			cmd = __dequeue_reclaim_cmd(reclaim);
			if (!cmd)
				break;

			switch (cmd->type) {
			case RECLAIM_KILL:
				mem_process_kill(cmd->pid);
				break;

			case RECLAIM_PROC:
				mem_process_reclaim(cmd->pid, cmd->page_type, cmd->nr_to_reclaim);
				break;

			case RECLAIM_GLOBAL:
				mem_global_reclaim(cmd->nr_to_reclaim);
				break;

			default:
				pr_err("unknow type %d", cmd->type);
				break;
			}

			__release_reclaim_cmd(reclaim, cmd);
		}
	}

	return 0;
}

static int sys_kill_process(void *data, u64 val)
{
	struct rtmm_reclaim *reclaim = (struct rtmm_reclaim *)data;
	struct reclaim_cmd *cmd;
	pid_t pid = (pid_t)val;

	cmd = __alloc_reclaim_cmd(reclaim);

	if (!cmd) {
		pr_err("fail to get an empty slot\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_KILL;
	cmd->pid = pid;

	__enqueue_reclaim_cmd(reclaim, cmd);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(kill_ops, NULL, sys_kill_process, "%llu\n");

static int sys_proc_reclaim_show(struct seq_file *m, void *v)
{
	return 0;
}


static int sys_proc_reclaim_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = single_open(file, sys_proc_reclaim_show, inode->i_private);
	if (!ret) {
	}
	return ret;
}

static ssize_t sys_proc_reclaim_write(struct file *file, const char __user *user_buf,
		size_t size, loff_t *ppos)
{
	struct rtmm_reclaim *reclaim = ((struct seq_file *)file->private_data)->private;
	char buf[CMD_STR_LEN];
	int ret;
	pid_t pid;
	int page_type;
	int size_MB;
	struct reclaim_cmd *cmd;

	if (size >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, user_buf, size))
		return -EFAULT;

	buf[size] = 0;

	ret = sscanf(buf, "%d %d %d", &pid, &page_type, &size_MB);

	if (ret != 3) {
		pr_err("param err in proc reclaim cmd(%s)\n", buf);
		return -EINVAL;
	}

	cmd = __alloc_reclaim_cmd(reclaim);
	if (!cmd) {
		pr_err("fail to get an empty slot\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_PROC;
	cmd->pid = pid;
	cmd->page_type = page_type;
	cmd->nr_to_reclaim = MB_TO_PAGES(size_MB);

	__enqueue_reclaim_cmd(reclaim, cmd);

	return size;
}

static const struct file_operations proc_reclaim_fops = {
	.owner		= THIS_MODULE,
	.open		= sys_proc_reclaim_open,
	.read		= seq_read,
	.write		= sys_proc_reclaim_write,
	.llseek		= seq_lseek,
	.release		= seq_release,
};

static int sys_global_reclaim_show(struct seq_file *m, void *v)
{
	return 0;
}

static int sys_global_reclaim_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = single_open(file, sys_global_reclaim_show, inode->i_private);
	if (!ret) {
	}
	return ret;
}

static ssize_t sys_global_reclaim_write(struct file *file, const char __user *user_buf,
		size_t size, loff_t *ppos)
{
	struct rtmm_reclaim *reclaim = ((struct seq_file *)file->private_data)->private;
	char buf[CMD_STR_LEN];
	int ret;
	int size_MB;
	struct reclaim_cmd *cmd;

	if (size >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, user_buf, size))
		return -EFAULT;

	buf[size] = 0;

	ret = sscanf(buf, "%d", &size_MB);

	if (ret != 1) {
		pr_err("param err in global reclaim cmd(%s)\n", buf);
		return -EINVAL;
	}

	if (size_MB > GLOBAL_RECLAIM_MAX_SIZE) {
		pr_err("global reclaim: size %d MB to big, return\n", size_MB);
		return -EINVAL;
	}

	cmd = __alloc_reclaim_cmd(reclaim);
	if (!cmd) {
		pr_err("fail to get an empty slot\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_GLOBAL;
	cmd->nr_to_reclaim = MB_TO_PAGES(size_MB);

	__enqueue_reclaim_cmd(reclaim, cmd);

	return size;
}

static const struct file_operations global_reclaim_fops = {
	.owner		= THIS_MODULE,
	.open		= sys_global_reclaim_open,
	.read		= seq_read,
	.write		= sys_global_reclaim_write,
	.llseek		= seq_lseek,
	.release		= seq_release,
};


static int sys_auto_reclaim_show(struct seq_file *m, void *v)
{
	return 0;
}

static int sys_auto_reclaim_open(struct inode *inode, struct file *file)
{
	int ret;

	ret = single_open(file, sys_auto_reclaim_show, inode->i_private);
	if (!ret) {
	}
	return ret;
}

static ssize_t sys_auto_reclaim_write(struct file *file, const char __user *user_buf,
		size_t size, loff_t *ppos)
{
	struct rtmm_reclaim *reclaim = ((struct seq_file *)file->private_data)->private;
	char buf[CMD_STR_LEN];
	int ret;
	struct reclaim_cmd *cmd;
	int target_free_size;
	int cur_free = global_page_state(NR_FREE_PAGES);
	int cur_free_size = PAGES_TO_MB(cur_free);
	int reclaim_size;

	if (size >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, user_buf, size))
		return -EFAULT;

	buf[size] = 0;

	ret = sscanf(buf, "%d", &target_free_size);

	if (ret != 1) {
		pr_err("param err in auto reclaim cmd(%s)\n", buf);
		return -EINVAL;
	}

	if (target_free_size > GLOBAL_RECLAIM_MAX_SIZE) {
		pr_err("auto reclaim: size %d MB to big, return\n", target_free_size);
		return -EINVAL;
	}

	reclaim_size = target_free_size - cur_free_size;
	if (reclaim_size <= 0)
		return size;

	cmd = __alloc_reclaim_cmd(reclaim);
	if (!cmd) {
		pr_err("fail to get an empty slot in auto reclaim\n");
		return -ENOMEM;
	}

	cmd->type = RECLAIM_GLOBAL;
	cmd->nr_to_reclaim = MB_TO_PAGES(reclaim_size);
	__enqueue_reclaim_cmd(reclaim, cmd);

	return size;
}

static const struct file_operations auto_reclaim_fops = {
	.owner		= THIS_MODULE,
	.open		= sys_auto_reclaim_open,
	.read		= seq_read,
	.write		= sys_auto_reclaim_write,
	.llseek		= seq_lseek,
	.release		= seq_release,
};

static void __init reclaim_init(struct rtmm_reclaim *reclaim)
{
	int i;

	spin_lock_init(&reclaim->lock);
	init_waitqueue_head(&reclaim->waitqueue);

	for (i = 0; i < RECLAIM_CMD_NR; i++) {
		__init_reclaim_cmd(&reclaim->cmds[i]);
	}

	atomic_set(&reclaim->cmd_nr, 0);
	INIT_LIST_HEAD(&reclaim->cmd_todo);

	if (reclaim->dir) {
		debugfs_create_file("kill",
				S_IFREG | S_IRUGO | S_IWUSR,
				reclaim->dir,
				reclaim,
				&kill_ops);

		debugfs_create_file("proc_reclaim",
				S_IFREG | S_IRUGO | S_IWUSR,
				reclaim->dir,
				reclaim,
				&proc_reclaim_fops);

		debugfs_create_file("global_reclaim",
				S_IFREG | S_IRUGO | S_IWUSR,
				reclaim->dir,
				reclaim,
				&global_reclaim_fops);

		debugfs_create_file("auto_reclaim",
				S_IFREG | S_IRUGO | S_IWUSR,
				reclaim->dir,
				reclaim,
				&auto_reclaim_fops);
	}
}

int __init rtmm_reclaim_init(struct dentry *dir)
{
	struct rtmm_reclaim *reclaim = get_reclaim();

	memset(reclaim, 0, sizeof(struct rtmm_reclaim));

	reclaim->dir = debugfs_create_dir("reclaim", dir);
	if (!reclaim->dir) {
		pr_err("fail to create debugfs dir\n");
	}

	reclaim_init(reclaim);

	reclaim->tsk = kthread_run(mem_reclaim_thread, reclaim, "rtmm_reclaim");
	if (IS_ERR(reclaim->tsk)) {
		pr_err("%s: creating thread for rtmm reclaim failed\n",
		       __func__);
		return PTR_ERR_OR_ZERO(reclaim->tsk);
	}

	pr_info("rtmm reclaim init OK\n");

	return 0;
}


