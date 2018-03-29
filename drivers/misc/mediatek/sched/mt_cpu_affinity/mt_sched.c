/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/cpu.h>
#include <linux/security.h>
#include <linux/cpuset.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include "mt_sched_drv.h"

#define SCHED_DEV_NAME "sched"

struct mt_task {
	pid_t pid;
	struct task_struct *p;
	struct cpumask mask;
	struct list_head list;
};

static struct mt_task mt_task_head;
static DEFINE_SPINLOCK(mt_sched_spinlock);

static int get_user_cpu_mask(unsigned long __user *user_mask_ptr, unsigned len,
			     struct cpumask *new_mask)
{
	if (len < cpumask_size())
		cpumask_clear(new_mask);
	else if (len > cpumask_size())
		len = cpumask_size();

	return copy_from_user(new_mask, user_mask_ptr, len) ? -EFAULT : 0;
}

/**
 * find_process_by_pid - find a process with a matching PID value.
 * @pid: the pid in question.
 */
static struct task_struct *find_process_by_pid(pid_t pid)
{
	return pid ? find_task_by_vpid(pid) : current;
}

/*
 * check the target process has a UID that matches the current process's
 */
static bool check_same_owner(struct task_struct *p)
{
	const struct cred *cred = current_cred(), *pcred;
	bool match;

	rcu_read_lock();
	pcred = __task_cred(p);
	match = (uid_eq(cred->euid, pcred->euid) || uid_eq(cred->euid, pcred->uid));
	rcu_read_unlock();
	return match;
}

/*
 * check the task link list. If the task is exit, delete the task.
 */
static void mt_sched_check_tasks(void)
{
	struct mt_task *tmp, *tmp2;
	unsigned long irq_flags;

	spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
	list_for_each_entry_safe(tmp, tmp2, &mt_task_head.list, list) {
		if (tmp->pid != tmp->p->pid) {
			list_del(&(tmp->list));
			kfree(tmp);
		}
	}
	spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);
}

static long __mt_sched_addaffinity(struct task_struct *p, const struct cpumask *new_mask)
{

#if MT_SCHED_AFFININTY_DEBUG
	pr_debug("MT_SCHED: %s pid[%d]\n", __func__, p->pid);
#endif
	struct mt_task *new;
	struct mt_task *tmp, *tmp2;
	unsigned long irq_flags;
	int find = 0;

	new = kmalloc(sizeof(struct mt_task), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	INIT_LIST_HEAD(&(new->list));
	new->pid = p->pid;
	new->p = p;
	cpumask_copy(&new->mask, new_mask);

	spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
	list_for_each_entry_safe(tmp, tmp2, &mt_task_head.list, list) {
		if (tmp->pid != tmp->p->pid) {
			list_del(&(tmp->list));
			kfree(tmp);
			continue;
		}
		if (!find && (tmp->p == p)) {
			find = 1;
			cpumask_copy(&tmp->mask, new_mask);
		}
	}

	if (!find)
		list_add(&(new->list), &(mt_task_head.list));

	spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);

	if (find)
		kfree(new);

	return 0;
}

static long __mt_sched_setaffinity(pid_t pid, const struct cpumask *in_mask)
{
	cpumask_var_t cpus_allowed, new_mask;
	struct task_struct *p;
	int retval;

#if MT_SCHED_AFFININTY_DEBUG
	pr_debug("MT_SCHED: %s pid[%d]\n", __func__, pid);
#endif

	rcu_read_lock();

	p = find_process_by_pid(pid);
	if (!p) {
		rcu_read_unlock();
		pr_debug("MT_SCHED: setaffinity find process %d fail\n", pid);
		return -ESRCH;
	}

	/* Prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	if (p->flags & PF_NO_SETAFFINITY) {
		retval = -EINVAL;
		pr_debug("MT_SCHED: setaffinity flags PF_NO_SETAFFINITY fail\n");
		goto out_put_task;
	}

	if (!alloc_cpumask_var(&cpus_allowed, GFP_KERNEL)) {
		retval = -ENOMEM;
		pr_debug("MT_SCHED: setaffinity allo_cpumask_var for cpus_allowed fail\n");
		goto out_put_task;
	}
	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL)) {
		retval = -ENOMEM;
		pr_debug("MT_SCHED: setaffinity allo_cpumask_var for new_mask fail\n");
		goto out_free_cpus_allowed;
	}
	retval = -EPERM;

	if (!check_same_owner(p)) {
		rcu_read_lock();
		if (!ns_capable(__task_cred(p)->user_ns, CAP_SYS_NICE)) {
			rcu_read_unlock();
			pr_debug("MT_SCHED: setaffinity check_same_owner and task_ns_capable fail\n");
			goto out_free_new_mask;
		}
		rcu_read_unlock();
	}
	retval = security_task_setscheduler(p);
	if (retval) {
		pr_debug("MT_SCHED: setaffinity security_task_setscheduler fail, status: %d\n",
		       retval);
		goto out_free_new_mask;
	}

	cpuset_cpus_allowed(p, cpus_allowed);
	cpumask_and(new_mask, in_mask, cpus_allowed);

	/*
	* Since bandwidth control happens on root_domain basis,
	* if admission test is enabled, we only admit -deadline
	* tasks allowed to run on all the CPUs in the task's
	* root_domain.
	*/

#ifdef CONFIG_SMP
		if (task_has_dl_policy(p) && dl_bandwidth_enabled()) {
			rcu_read_lock();
			if (!cpumask_subset(task_rq(p)->rd->span, new_mask)) {
				retval = -EBUSY;
				rcu_read_unlock();
				pr_debug("MT_SCHED: setaffinity deadline tasks fail, status: %d\n", retval);
				goto out_free_new_mask;
			}
			rcu_read_unlock();
		}
#endif
again:
	retval = set_cpus_allowed_ptr(p, new_mask);
	if (retval)
		pr_debug("MT_SCHED: set_cpus_allowed_ptr status %d\n", retval);

	if (!retval) {
		cpuset_cpus_allowed(p, cpus_allowed);
		if (!cpumask_subset(new_mask, cpus_allowed)) {
			/*
			 * We must have raced with a concurrent cpuset
			 * update. Just reset the cpus_allowed to the
			 * cpuset's cpus_allowed
			 */
			cpumask_copy(new_mask, cpus_allowed);
			goto again;
		}
	}

	/* modify for the mt_sched_setaffinity */
	if ((!retval) || (!cpumask_intersects(new_mask, cpu_active_mask)))
		retval = __mt_sched_addaffinity(p, new_mask);


out_free_new_mask:
	free_cpumask_var(new_mask);
out_free_cpus_allowed:
	free_cpumask_var(cpus_allowed);
out_put_task:
	put_task_struct(p);
	if (retval)
		pr_debug("MT_SCHED: setaffinity status %d\n", retval);

	return retval;
}

static long __mt_sched_getaffinity(pid_t pid, struct cpumask *mask, struct cpumask *mt_mask)
{
	struct task_struct *p;
	struct mt_task *tmp;
	unsigned long irq_flags;
	unsigned long flags;
	int retval;

	rcu_read_lock();

#if MT_SCHED_AFFININTY_DEBUG
	pr_debug("MT_SCHED: %s pid[%d]\n", __func__, pid);
#endif

	retval = -ESRCH;
	p = find_process_by_pid(pid);
	if (!p) {
		pr_debug("MT_SCHED: getaffinity find process %d fail\n", pid);
		goto out_unlock;
	}

	retval = security_task_getscheduler(p);
	if (retval) {
		pr_debug("MT_SCHED: getaffinity security_task_getscheduler fail, status: %d\n",
		       retval);
		goto out_unlock;
	}

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	cpumask_and(mask, &p->cpus_allowed, cpu_active_mask);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	/* add for the mt_mask */
	cpumask_clear(mt_mask);
	spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
	list_for_each_entry(tmp, &mt_task_head.list, list) {
		if ((p == tmp->p) && (p->pid == tmp->pid)) {
			cpumask_copy(mt_mask, &tmp->mask);
			break;
		}
	}
	spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);

 out_unlock:
	rcu_read_unlock();

	if (retval)
		pr_debug("MT_SCHED: getaffinity status %d\n", retval);

	return retval;
}

static long __mt_sched_exitaffinity(pid_t pid)
{
	struct mt_task *tmp, *tmp2;
	unsigned long irq_flags;
	int find = 0;

	if (0 == pid)
		pid = current->pid;

#if MT_SCHED_AFFININTY_DEBUG
	pr_debug("MT_SCHED: %s pid[%d]\n", __func__, pid);
#endif

	spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
	list_for_each_entry_safe(tmp, tmp2, &mt_task_head.list, list) {
		if (pid == tmp->pid) {
			list_del(&(tmp->list));
			kfree(tmp);
			find = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);
	if (!find) {
		pr_debug("MT_SCHED: exit affinity find process %d fail.\n", pid);
		return -ESRCH;
	}
	return 0;
}

static long sched_ioctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval;
	pid_t pid;
	struct ioctl_arg data;
	cpumask_var_t new_mask, mask, mt_mask;
	int len;

	memset(&data, 0, sizeof(data));
	switch (cmd) {
	case IOCTL_SETAFFINITY:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			goto done;
		}
#if MT_SCHED_AFFININTY_DEBUG
		pr_debug("MT_SCHED: %s [IOCTL_SETAFFINITY]\n", __func__);
#endif

		if (!alloc_cpumask_var(&new_mask, GFP_KERNEL))
			return -ENOMEM;

		retval = get_user_cpu_mask(data.mask, data.len, new_mask);
		if (retval == 0)
			retval = __mt_sched_setaffinity(data.pid, new_mask);
		if (retval)
			pr_debug("MT_SCHED: setaffinity status %d\n", retval);
		free_cpumask_var(new_mask);
		break;

	case IOCTL_GETAFFINITY:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			goto done;
		}
#if MT_SCHED_AFFININTY_DEBUG
		pr_debug("MT_SCHED: %s [IOCTL_GETAFFINITY]\n", __func__);
#endif

		len = data.len;

		if ((len * BITS_PER_BYTE) < nr_cpu_ids)
			return -EINVAL;
		if (len & (sizeof(unsigned int) - 1))
			return -EINVAL;

		if (!alloc_cpumask_var(&mask, GFP_KERNEL))
			return -ENOMEM;

		if (!alloc_cpumask_var(&mt_mask, GFP_KERNEL)) {
			goto getaffinity_free1;
			return -ENOMEM;
		}

		retval = __mt_sched_getaffinity(data.pid, mask, mt_mask);
		if (retval == 0) {
			size_t retlen = min_t(size_t, len, cpumask_size());

			if (copy_to_user((int __user *)data.mask, mask, retlen)) {
				retval = -EFAULT;
				goto getaffinity_free;
			} else {
				retval = retlen;
			}

			if (copy_to_user((int __user *)data.mt_mask, mt_mask, retlen))
				retval = -EFAULT;
			else
				retval = retlen;
		}
 getaffinity_free:
		free_cpumask_var(mt_mask);
 getaffinity_free1:
		free_cpumask_var(mask);
		break;

	case IOCTL_EXITAFFINITY:
#if MT_SCHED_AFFININTY_DEBUG
		pr_debug("MT_SCHED: %s [IOCTL_EXITAFFINITY]\n", __func__);
#endif
		if (copy_from_user(&pid, (int __user *)arg, sizeof(pid))) {
			retval = -EFAULT;
			goto done;
		}

		retval = __mt_sched_exitaffinity(pid);
		break;
	default:
		retval = -ENOTTY;
	}
 done:
	return retval;
}

const struct file_operations sched_ioctl_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = sched_ioctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sched_ioctl_compat,
#endif
};

static struct cdev *sched_ioctl_cdev;
static dev_t sched_dev_num;
struct class *sched_class;
static int __init sched_ioctl_init(void)
{
	int ret;
	struct device *class_dev = NULL;

	if (alloc_chrdev_region(&sched_dev_num, 0, 1, SCHED_DEV_NAME)) {
		pr_debug("MT_SCHED: Device major number allocation failed\n");
		return -EAGAIN;
	}

	sched_ioctl_cdev = cdev_alloc();
	if (NULL == sched_ioctl_cdev) {
		pr_debug("MT_SCHED: cdev_alloc failed\n");
		ret = -1;
		goto out_err2;
	}

	cdev_init(sched_ioctl_cdev, &sched_ioctl_fops);
	sched_ioctl_cdev->owner = THIS_MODULE;
	ret = cdev_add(sched_ioctl_cdev, sched_dev_num, 1);
	if (ret) {
		pr_debug("MT_SCHED: Char device add failed\n");
		goto out_err2;
	}

	sched_class = class_create(THIS_MODULE, "scheddrv");
	if (IS_ERR(sched_class)) {
		pr_debug("Unable to create class, err = %d\n", (int)PTR_ERR(sched_class));
		goto out_err1;
	}
	class_dev = device_create(sched_class, NULL, sched_dev_num, NULL, "mtk_sched");

	pr_alert("MT_SCHED: Init complete, device major number = %d\n", MAJOR(sched_dev_num));

	goto out;

	class_destroy(sched_class);
 out_err1:
	cdev_del(sched_ioctl_cdev);
 out_err2:
	unregister_chrdev_region(sched_dev_num, 1);
 out:
	return ret;
}

/**
 * /proc/mtk_sched/affinity_status
 */
static int sched_status_show(struct seq_file *seq, void *v);
static int sched_status_open(struct inode *inode, struct file *file);
static unsigned int sched_status_poll(struct file *file, poll_table *wait);
static const struct file_operations sched_status_fops = {
	.open = sched_status_open,
	.read = seq_read,
	.poll = sched_status_poll,
	.llseek = seq_lseek,
	.release = single_release
};

static int sched_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, sched_status_show, inode->i_private);
}

static int sched_status_show(struct seq_file *seq, void *v)
{
	struct mt_task *tmp;
	struct cpumask mask;
	unsigned long irq_flags;
	int i;

	for_each_online_cpu(i)
		seq_printf(seq, "CPU%d:\t\tonline\n", i);

	mt_sched_check_tasks();
	spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
	seq_puts(seq, "\n  PID  REAL BACKUP CMD\n");
	list_for_each_entry(tmp, &mt_task_head.list, list) {
		cpumask_and(&mask, &tmp->p->cpus_allowed, cpu_online_mask);
		seq_printf(seq, "%5d %4lu %4lu    %s\n", tmp->pid, *mask.bits, *tmp->mask.bits,
			   tmp->p->comm);
	}
	spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);

	return 0;
}

static unsigned int sched_status_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static int __init sched_proc_init(void)
{
	struct proc_dir_entry *pe;

	if (!proc_mkdir("mtk_sched", NULL))
		return -1;

	pe = proc_create("mtk_sched/affinity_status", 0444, NULL, &sched_status_fops);
	if (!pe)
		return -ENOMEM;

	return 0;
}

/**
 * sched_cpu_notify - sched cpu notifer callback function.
 */
static int sched_cpu_notify(struct notifier_block *self, unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	struct mt_task *tmp, *tmp2;
	unsigned long irq_flags;
	struct task_struct *p;

	switch (action) {
	case CPU_ONLINE:
		spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
		list_for_each_entry_safe(tmp, tmp2, &mt_task_head.list, list) {
			if (cpumask_test_cpu(cpu, &(tmp->mask))) {
				p = find_process_by_pid(tmp->pid);
				if (p) {
					if (p == tmp->p)
						cpumask_copy(&tmp->p->cpus_allowed, &(tmp->mask));
					else{
						list_del(&(tmp->list));
						kfree(tmp);
					}
				} else {
					list_del(&(tmp->list));
					kfree(tmp);
				}
			}
		}
		spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);

		break;
	case CPU_DOWN_FAILED:
		break;
	case CPU_DOWN_PREPARE:
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block sched_cpu_nb = {
	.notifier_call = sched_cpu_notify,
};

static int __init sched_module_init(void)
{
	int ret;
	unsigned long irq_flags;

	ret = sched_ioctl_init();
	if (ret)
		return ret;

	ret = sched_proc_init();
	if (ret)
		return ret;

	ret = register_cpu_notifier(&sched_cpu_nb);
	if (ret)
		return ret;

	spin_lock_irqsave(&mt_sched_spinlock, irq_flags);
	INIT_LIST_HEAD(&(mt_task_head.list));
	spin_unlock_irqrestore(&mt_sched_spinlock, irq_flags);
	return ret;
}

static void sched_module_exit(void)
{
	class_destroy(sched_class);
	cdev_del(sched_ioctl_cdev);
	unregister_chrdev_region(sched_dev_num, 1);
	pr_alert("MT_SCHED: driver removed.\n");
}
module_init(sched_module_init);
module_exit(sched_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YaTing Chang <yt.chang@mediatek.com>");
MODULE_DESCRIPTION("This is sched module.");

