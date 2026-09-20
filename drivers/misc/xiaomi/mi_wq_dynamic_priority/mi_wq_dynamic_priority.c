#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/blk-mq.h>
#include <trace/hooks/wqlockup.h>
#include <trace/hooks/blk.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cgroup.h>

#include "mi_wq_dynamic_priority.h"

/* The bit offset is 14. It is necessary to check whether this bit conflicts after kernel version upgrades. */
#define WQ_MI_HIPRI    (1 << 14)

#define VIRTUAL_KWORKER_NICE (-1000)

static bool mikblockd_enabled = true;

#define DATA_LEN 64

static struct workqueue_struct *mi_kblockd_workqueue;

#if IS_ENABLED(CONFIG_METIS)
extern bool mi_vip_task(struct task_struct *tsk);
extern bool mi_link_vip_task(struct task_struct *tsk);
extern bool mi_ui_task(struct task_struct *p);
extern bool mi_render_task(struct task_struct *p);

static bool is_fore_task(struct task_struct *tsk)
{
	struct cgroup_subsys_state *css;
	bool is_fore = false;

	rcu_read_lock();
	css = task_css(tsk, cpuset_cgrp_id);
	if (css && css->cgroup && css->cgroup->kn)
		is_fore = strncmp(css->cgroup->kn->name, "background", 11) != 0;
	rcu_read_unlock();

	return is_fore;
}

static bool is_vip_task(struct task_struct *tsk)
{
	if (((mi_vip_task(tsk) || mi_link_vip_task(tsk) ||
		mi_ui_task(tsk) || mi_render_task(tsk)) && is_fore_task(tsk)) ||
		rt_task(tsk))
		return true;
	return false;
}
#else
static bool is_vip_task(struct task_struct *tsk)
{
	if (rt_task(tsk))
		return true;
	return false;
}
#endif

static void sfi_delay_run_hw_queue(void *data, int cpu, struct blk_mq_hw_ctx *hctx,
		unsigned long jif_delay, bool *skip)
{
	if (READ_ONCE(mikblockd_enabled) && mi_kblockd_workqueue && !jif_delay &&
			in_task() && is_vip_task(current)) {
		*skip = queue_work_on(WORK_CPU_UNBOUND, mi_kblockd_workqueue, &hctx->run_work.work);
	} else {
		*skip = false;
	}
}

static void sfi_kick_requeue_list(void *data, struct request_queue *q,
		unsigned long jif_delay, bool *skip)
{
	if (READ_ONCE(mikblockd_enabled) && mi_kblockd_workqueue && !jif_delay &&
			in_task() && is_vip_task(current)) {
		*skip = queue_work_on(WORK_CPU_UNBOUND, mi_kblockd_workqueue, &q->requeue_work.work);
	} else {
		*skip = false;
	}
}

static void sfi_create_worker(void *data, struct task_struct *task,
		struct workqueue_attrs *attrs)
{
	if (attrs->nice == VIRTUAL_KWORKER_NICE) {
		sched_set_fifo_low(task);
		set_user_nice(task, MIN_NICE);
	}
}

static void sfi_alloc_and_link_pwqs(void *data, struct workqueue_struct *wq,
		int *ret, bool *skip)
{
	if (wq && (wq->flags & WQ_MI_HIPRI)) {
		struct workqueue_attrs *new_attrs;

		new_attrs = alloc_workqueue_attrs();

		if (!new_attrs) {
			*skip = false;
			pr_err("sfi_v3: alloc_workqueue_attrs failed.\n");
			return;
		}

		new_attrs->nice = VIRTUAL_KWORKER_NICE;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)
		*ret = apply_workqueue_attrs_locked(wq, new_attrs);
#else
		cpus_read_lock();
		*ret = apply_workqueue_attrs(wq, new_attrs);
		cpus_read_unlock();
#endif
		*skip = true;
		free_workqueue_attrs(new_attrs);
	} else
		*skip = false;
}

struct config_wq_flags {
	char *target_str;
	unsigned int new_flags;
};

static struct config_wq_flags mi_wq_config[] = {
	{ "kverityd", WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_MI_HIPRI | WQ_UNBOUND },
	{ "loop", WQ_UNBOUND | WQ_FREEZABLE | WQ_MI_HIPRI},
	{ NULL, 0 } /* Terminate array with NULL */
};

static int handler_alloc_workqueue_pre(struct kprobe *p, struct pt_regs *regs)
{
	const char *fmt = (const char *)regs->regs[0];
	unsigned int flags = (unsigned int)regs->regs[1];

	struct config_wq_flags *item = mi_wq_config;
	if(fmt) {
		while (item->target_str) {
			if (!strncmp(fmt, item->target_str, strlen(item->target_str)) && (item->new_flags != flags)) {
				regs->regs[1] = item->new_flags;
				break;
			}
			item++;
		}
	}
	return 0;
}

static struct kprobe mi_alloc_workqueue_kp = {
	.symbol_name = "alloc_workqueue",
	.pre_handler = handler_alloc_workqueue_pre,
};

static int register_mi_wq_dynprio_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_blk_mq_delay_run_hw_queue failed! ret=%d\n", ret);
		goto out;
	}

	ret = register_trace_android_vh_blk_mq_kick_requeue_list(sfi_kick_requeue_list, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_blk_mq_kick_requeue_list failed! ret=%d\n", ret);
		unregister_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
		goto out;
	}

	ret = register_trace_android_rvh_create_worker(sfi_create_worker, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_rvh_create_worker failed! ret=%d\n", ret);
		unregister_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
		unregister_trace_android_vh_blk_mq_kick_requeue_list(sfi_kick_requeue_list, NULL);
		goto out;
	}

	ret = register_trace_android_rvh_alloc_and_link_pwqs(sfi_alloc_and_link_pwqs, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_rvh_alloc_and_link_pwqs failed! ret=%d\n", ret);
		unregister_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
		unregister_trace_android_vh_blk_mq_kick_requeue_list(sfi_kick_requeue_list, NULL);
		goto out;
	}

out:
	return ret;
}

static void unregister_mi_wq_dynprio_vendor_hooks(void)
{
	unregister_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
	unregister_trace_android_vh_blk_mq_kick_requeue_list(sfi_kick_requeue_list, NULL);
	/* rvh hook don't have unregister */
	return;
}

static ssize_t
mikblockd_enable_write(struct file *filp, const char *ubuf, size_t len, loff_t *data)
{
	int ret = 0;
	u64 value = 0;
	char buffer[DATA_LEN] = {0};

	len = (len >= DATA_LEN) ? DATA_LEN -1 : len;
	if (copy_from_user(buffer, ubuf, len)) {
		return -EFAULT;
	}

	buffer[len] = '\0';
	ret = kstrtou64(buffer, 10, &value);

	if (ret)
		return ret;

	if (value == 1) {
		WRITE_ONCE(mikblockd_enabled, true);
		pr_err("mikblockd_enabled is  true.\n");
	} else {
		WRITE_ONCE(mikblockd_enabled, false);
		pr_err("mikblockd_enabled is false.\n");
	}

	return len;
}

static int mikblockd_enable_show(struct seq_file *m, void *v)
{
	if (!m) {
		pr_err("seq_file is Null.\n");
		return -EFAULT;
	}

	seq_printf(m, "%d\n", READ_ONCE(mikblockd_enabled));
	return 0;
}

static int mikblockd_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, mikblockd_enable_show, inode->i_private);
}

static const struct proc_ops mikblockd_enable_fops = {
	.proc_open = mikblockd_enable_open,
	.proc_write = mikblockd_enable_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init mi_wq_dynprio_init(void)
{
	struct proc_dir_entry *pe;
	int ret;

	/* register hook must be front of alloc_workqueue */
	ret = register_mi_wq_dynprio_vendor_hooks();
	if (ret !=0 ) {
		WRITE_ONCE(mikblockd_enabled, false);
		pr_err("register_mi_wq_dynprio_vendor_hooks failed.\n");
		return 0;
	}

	pe = proc_create("mikblockd_enable", 0664, NULL, &mikblockd_enable_fops);
	if (!pe) {
		WRITE_ONCE(mikblockd_enabled, false);
		pr_err("fail to create file node\n");
		unregister_mi_wq_dynprio_vendor_hooks();
		return 0;
	}

	/* WQ_UNBOUND must be set, only unbound wq will contact to the workqueue_attrs */
	mi_kblockd_workqueue = alloc_workqueue("mikblockd", WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_MI_HIPRI | WQ_UNBOUND, 0);

	if (!mi_kblockd_workqueue) {
		WRITE_ONCE(mikblockd_enabled, false);
		pr_err("alloc  mi_kblockd_workqueue fail!\n");
		remove_proc_entry("mikblockd_enable", NULL);
		unregister_mi_wq_dynprio_vendor_hooks();
		return 0;
	}

	ret = register_kprobe(&mi_alloc_workqueue_kp);
	if (ret < 0) {
		WRITE_ONCE(mikblockd_enabled, false);
		pr_err("register_kprobe alloc_workqueue failed, returned %d\n", ret);
		remove_proc_entry("mikblockd_enable", NULL);
		unregister_mi_wq_dynprio_vendor_hooks();
		destroy_workqueue(mi_kblockd_workqueue);
		return 0;
	}

	/* rvh hooks cannot be unregistered; pin the module to prevent rmmod UAF */
	try_module_get(THIS_MODULE);

	return 0;
}

static void __exit mi_wq_dynprio_exit(void)
{
	WRITE_ONCE(mikblockd_enabled, false);
	unregister_mi_wq_dynprio_vendor_hooks();
	unregister_kprobe(&mi_alloc_workqueue_kp);
	synchronize_rcu();
	remove_proc_entry("mikblockd_enable", NULL);
	if (mi_kblockd_workqueue)
		destroy_workqueue(mi_kblockd_workqueue);
}

module_init(mi_wq_dynprio_init);
module_exit(mi_wq_dynprio_exit);

MODULE_LICENSE("GPL");
