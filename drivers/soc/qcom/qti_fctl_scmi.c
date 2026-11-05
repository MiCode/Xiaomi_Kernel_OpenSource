// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <uapi/linux/sched/types.h>

#define FEAT_CTRL_ALGO_STR	(0x4643544C) /* FCTL (Feature control) ASCII */
#define UNIQUE_NAME(str, cpu)	"fctl_"#str#cpu

struct qcom_fctl_attr {
	struct attribute		attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};

enum fctl_param_ids {
	PARAM_ENABLE_NTWB = 1,
};

#define to_fctl_attr(_attr) \
	container_of(_attr, struct qcom_fctl_attr, attr)
#define FEAT_CTRL_ATTR_RW(_name)						\
static struct qcom_fctl_attr _name =					\
	__ATTR(_name, 0644, show_##_name, store_##_name)			\

enum command {
	COMMAND_INIT = -1,
	COMMAND_SHOW,
	COMMAND_STORE,
};

struct qcom_fctl_dd {
	struct platform_device *pdev;
	struct scmi_protocol_handle *ph;
	const struct qcom_scmi_vendor_ops *ops;
	const char *thread_comm;
	struct mutex lock;
	struct kobject kobj;
	wait_queue_head_t *waitq;
	struct wakeup_source **ws;
	struct task_struct **store;
	u32 *curr_val;
	u32 *get_val;
	enum command cmd;
	int (*thread_fn)(void *data);
	struct device *dev;
	u32 val;
	u32 req_val;
	int ret;
};

static DEFINE_PER_CPU(bool, cpu_is_on);
static DEFINE_PER_CPU(bool, need_ack);
static struct qcom_fctl_dd *qcom_fctl_dd;
static struct platform_device *qcom_fctl_pdev;

static ssize_t store_enable_force_ntwb(struct kobject *kobj, struct attribute *attr,
				       const char *buf, size_t count)
{
	struct qcom_fctl_dd *fds = qcom_fctl_dd;
	int ret, err;
	bool val;
	int cpu, i;

	ret = kstrtobool(buf, &val);
	if (ret < 0)
		return ret;

	cpus_read_lock();
	mutex_lock(&fds->lock);
	fds->req_val = val ? 1 : 0;
	if (fds->req_val == fds->val)
		goto unlock_cpu_readlock;

	fds->cmd = COMMAND_STORE;
	for_each_possible_cpu(cpu) {
		if (!per_cpu(cpu_is_on, cpu))
			continue;

		per_cpu(need_ack, cpu) = true;
		wake_up(&fds->waitq[cpu]);
		do {
			err = wait_event_interruptible(fds->waitq[cpu],
						       !per_cpu(need_ack, cpu));
			if (fds->ret < 0)
				goto cleanup;

		} while (err != 0);

		fds->curr_val[cpu] = fds->req_val;
	}

	fds->val = fds->req_val;
	goto unlock_cpu_readlock;

cleanup:
	fds->req_val = !fds->req_val;
	for (i = cpu - 1; i >= 0; i--) {
		if (!per_cpu(cpu_is_on, i))
			continue;

		fds->ret = 0;
		per_cpu(need_ack, i) = true;
		wake_up(&fds->waitq[i]);
		do {
			err = wait_event_interruptible(fds->waitq[i],
						       !per_cpu(need_ack, i));
			if (fds->ret < 0) {
				dev_err(fds->dev, "fctl broken now:cpu:%d\n", i);
				WARN_ON(1);
				break;
			}
		} while (err != 0);

		fds->curr_val[cpu] = fds->req_val;
	}

unlock_cpu_readlock:
	fds->cmd = COMMAND_INIT;
	mutex_unlock(&fds->lock);
	cpus_read_unlock();

	return ((ret < 0) ? ret : count);
}

static ssize_t show_enable_force_ntwb(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	struct qcom_fctl_dd *fds = qcom_fctl_dd;
	int val = 1;
	int cpu;
	int err;

	cpus_read_lock();
	mutex_lock(&fds->lock);
	fds->cmd = COMMAND_SHOW;
	for_each_possible_cpu(cpu) {
		if (!per_cpu(cpu_is_on, cpu))
			continue;

		per_cpu(need_ack, cpu) = true;
		wake_up(&fds->waitq[cpu]);
		do {
			err = wait_event_interruptible(fds->waitq[cpu],
						       !per_cpu(need_ack, cpu));
			if (fds->ret < 0)
				goto cleanup;

		} while (err != 0);
	}

	for_each_possible_cpu(cpu) {
		if (!per_cpu(cpu_is_on, cpu))
			continue;

		val &= fds->get_val[cpu];
	}

	fds->val = val ? 1 : 0;
	fds->cmd = COMMAND_INIT;

cleanup:
	mutex_unlock(&fds->lock);
	cpus_read_unlock();
	return scnprintf(buf, PAGE_SIZE, "%u\n", le32_to_cpu(fds->val));
}

FEAT_CTRL_ATTR_RW(enable_force_ntwb);

static struct attribute *fctl_settings_attrs[] = {
	&enable_force_ntwb.attr,
	NULL,
};
ATTRIBUTE_GROUPS(fctl_settings);

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
			 char *buf)
{
	struct qcom_fctl_attr *fctl_attr = to_fctl_attr(attr);
	ssize_t ret = -EIO;

	if (fctl_attr->show)
		ret = fctl_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
			  const char *buf, size_t count)
{
	struct qcom_fctl_attr *fctl_attr = to_fctl_attr(attr);
	ssize_t ret = -EIO;

	if (fctl_attr->store)
		ret = fctl_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops fctl_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static const struct kobj_type fctl_kobj_ktype = {
	.sysfs_ops		= &fctl_sysfs_ops,
	.default_groups		= fctl_settings_groups,
};

static int qcom_fctl_kthread_fn(void *data)
{
	struct qcom_fctl_dd *fds = qcom_fctl_dd;
	u32 cpu;
	/* [0] is for cpu id and [1] is for enable/disable fctl */
	u32 buf[2];
	int ret;

	while (!(kthread_should_stop())) {
		if (kthread_should_park()) {
			kthread_parkme();
			continue;
		}

		preempt_disable();
		cpu = smp_processor_id();
		preempt_enable();
repeat:
		do {
			ret = wait_event_interruptible(fds->waitq[cpu],
						       per_cpu(need_ack, cpu));
		} while (ret != 0);

		BUG_ON(cpu != *(int *)data);

		__set_current_state(TASK_RUNNING);

		buf[0] = cpu;
		switch (fds->cmd) {
		case COMMAND_INIT:
			break;
		case COMMAND_STORE:
			__pm_stay_awake(fds->ws[cpu]);
			buf[1] = cpu_to_le32(fds->req_val);
			fds->ret = fds->ops->set_param(fds->ph, &buf, FEAT_CTRL_ALGO_STR,
						       PARAM_ENABLE_NTWB, sizeof(buf));
			if (fds->ret < 0)
				dev_err(fds->dev, "Failed to set param for cpu:%u\n", cpu);

			per_cpu(need_ack, cpu) = false;
			wake_up(&fds->waitq[cpu]);
			__pm_relax(fds->ws[cpu]);
			goto repeat;
		case COMMAND_SHOW:
			__pm_stay_awake(fds->ws[cpu]);
			fds->ret = fds->ops->get_param(fds->ph, &buf, FEAT_CTRL_ALGO_STR,
						       PARAM_ENABLE_NTWB, sizeof(u32), sizeof(u32));
			if (fds->ret < 0)
				dev_err(fds->dev, "Failed to get param for cpu:%u\n", cpu);
			per_cpu(need_ack, cpu) = false;
			fds->get_val[cpu] = le32_to_cpu(buf[0]);
			wake_up(&fds->waitq[cpu]);
			__pm_relax(fds->ws[cpu]);
			break;
		}
	}

	return ret;
}

static void smp_destroy_threads(struct qcom_fctl_dd *fds)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		struct task_struct *tsk = fds->store[cpu];

		if (tsk) {
			kthread_stop(tsk);
			put_task_struct(tsk);
		}
	}
}

static int smp_create_thread(struct qcom_fctl_dd *fds, u32 cpu)
{
	struct task_struct *tsk;
	u32 *td;

	td = devm_kzalloc(fds->dev, sizeof(*td), GFP_KERNEL);
	if (!td)
		return -ENOMEM;

	*td = cpu;
	tsk = kthread_create_on_cpu(fds->thread_fn, td, cpu, fds->thread_comm);
	if (IS_ERR(tsk))
		return PTR_ERR(tsk);

	kthread_set_per_cpu(tsk, cpu);
	kthread_park(tsk);
	get_task_struct(tsk);
	fds->store[cpu] = tsk;

	return 0;
}

static int cpu_down_notifier(unsigned int cpu)
{
	struct qcom_fctl_dd *fds = qcom_fctl_dd;

	mutex_lock(&fds->lock);
	per_cpu(cpu_is_on, cpu) = false;
	per_cpu(need_ack, cpu) = false;
	mutex_unlock(&fds->lock);

	return 0;
}

static int cpu_up_notifier(unsigned int cpu)
{
	struct qcom_fctl_dd *fds = qcom_fctl_dd;
	int err;

	mutex_lock(&fds->lock);
	if (fds->curr_val[cpu] == fds->val)
		goto cpu_on;

	fds->cmd = COMMAND_STORE;
	fds->ret = 0;
	per_cpu(need_ack, cpu) = true;
	wake_up(&fds->waitq[cpu]);
	do {
		err = wait_event_interruptible(fds->waitq[cpu],
					       !per_cpu(need_ack, cpu));
		if (fds->ret < 0)
			break;

	} while (err != 0);

	fds->curr_val[cpu] = fds->val;

cpu_on:
	per_cpu(need_ack, cpu) = false;
	per_cpu(cpu_is_on, cpu) = true;
	mutex_unlock(&fds->lock);

	return 0;
}

static int qcom_fctl_probe(struct platform_device *pdev)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	const struct qcom_scmi_vendor_ops *ops;
	struct scmi_protocol_handle *ph;
	struct scmi_device *scmi_dev;
	struct device *dev_root;
	struct task_struct *tsk;
	u32 cpu;
	int ret;

	qcom_fctl_dd = devm_kzalloc(&pdev->dev, sizeof(*qcom_fctl_dd), GFP_KERNEL);
	if (!qcom_fctl_dd)
		return -ENOMEM;

	dev_root = bus_get_dev_root(&cpu_subsys);
	if (!dev_root)
		return -EPROBE_DEFER;

	scmi_dev = get_qcom_scmi_device();
	if (IS_ERR(scmi_dev)) {
		ret = PTR_ERR(scmi_dev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Error getting scmi_dev ret = %d\n", ret);
		return ret;
	}

	ops = scmi_dev->handle->devm_protocol_get(scmi_dev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops)) {
		ret = PTR_ERR(ops);
		ops = NULL;
		return ret;
	}

	mutex_init(&qcom_fctl_dd->lock);
	qcom_fctl_dd->dev = &pdev->dev;
	qcom_fctl_dd->ops = ops;
	qcom_fctl_dd->ph = ph;
	qcom_fctl_dd->thread_fn = qcom_fctl_kthread_fn;
	qcom_fctl_dd->thread_comm = "cpufctl/%u";
	qcom_fctl_dd->cmd = COMMAND_INIT;
	qcom_fctl_dd->ret = 0;

	qcom_fctl_dd->waitq = devm_kcalloc(&pdev->dev, num_possible_cpus(),
					   sizeof(wait_queue_head_t), GFP_KERNEL);
	if (!qcom_fctl_dd->waitq)
		return -ENOMEM;

	qcom_fctl_dd->ws = devm_kcalloc(&pdev->dev, num_possible_cpus(),
					sizeof(struct wakeup_source *), GFP_KERNEL);
	if (!qcom_fctl_dd->ws)
		return -ENOMEM;

	qcom_fctl_dd->store = devm_kcalloc(&pdev->dev, num_possible_cpus(),
					   sizeof(struct task_struct *), GFP_KERNEL);
	if (!qcom_fctl_dd->store)
		return -ENOMEM;

	qcom_fctl_dd->curr_val = devm_kcalloc(&pdev->dev,
					      num_possible_cpus(), sizeof(u32),
					      GFP_KERNEL);
	if (!qcom_fctl_dd->curr_val)
		return -ENOMEM;

	qcom_fctl_dd->get_val = devm_kcalloc(&pdev->dev,
					      num_possible_cpus(), sizeof(u32),
					      GFP_KERNEL);
	if (!qcom_fctl_dd->get_val)
		return -ENOMEM;

	cpus_read_lock();
	for_each_possible_cpu(cpu) {
		if (cpu_online(cpu))
			per_cpu(cpu_is_on, cpu) = true;

		init_waitqueue_head(&qcom_fctl_dd->waitq[cpu]);
		per_cpu(need_ack, cpu) = false;
		qcom_fctl_dd->ws[cpu] = wakeup_source_register(NULL, "fctl_ws");
		/* Earlier writes should be visible after this */
		smp_wmb();
		ret = smp_create_thread(qcom_fctl_dd, cpu);
		if (ret) {
			dev_err(&pdev->dev, "error during smp_create_thread ret = %d\n", ret);
			smp_destroy_threads(qcom_fctl_dd);
			cpus_read_unlock();
			return ret;
		}

		tsk = qcom_fctl_dd->store[cpu];
		if (tsk) {
			sched_setscheduler_nocheck(tsk, SCHED_FIFO, &param);
			kthread_unpark(tsk);
		}
	}

	ret = cpuhp_setup_state_nocalls_cpuslocked(CPUHP_AP_ONLINE_DYN, "fctl_cpu_hotplug",
						   cpu_up_notifier, cpu_down_notifier);
	cpus_read_unlock();

	ret = kobject_init_and_add(&qcom_fctl_dd->kobj, &fctl_kobj_ktype,
				   &dev_root->kobj, "fctl");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init fctl kobj: %d\n", ret);
		smp_destroy_threads(qcom_fctl_dd);
		kobject_put(&qcom_fctl_dd->kobj);
	}

	put_device(dev_root);

	return 0;
}

static int qcom_fctl_remove(struct platform_device *pdev)
{
	kobject_put(&qcom_fctl_dd->kobj);

	return 0;
}

static struct platform_driver qcom_fctl_driver = {
	.driver = {
		.name = "qcom-fctl",
	},
	.probe = qcom_fctl_probe,
	.remove = qcom_fctl_remove,
};

static int __init qcom_fctl_scmi_driver_init(void)
{
	int err;

	err = platform_driver_register(&qcom_fctl_driver);
	if (err)
		return err;

	qcom_fctl_pdev = platform_device_register_data(NULL, "qcom-fctl",
						       PLATFORM_DEVID_NONE, NULL, 0);
	if (IS_ERR(qcom_fctl_pdev)) {
		pr_err("failed to register qcom-fctl platform device\n");
		platform_driver_unregister(&qcom_fctl_driver);
		return PTR_ERR(qcom_fctl_pdev);
	}

	return 0;
}

module_init(qcom_fctl_scmi_driver_init)

static void __exit qcom_fctl_scmi_driver_exit(void)
{
	platform_device_unregister(qcom_fctl_pdev);
	platform_driver_unregister(&qcom_fctl_driver);
}
module_exit(qcom_fctl_scmi_driver_exit)

MODULE_SOFTDEP("pre: qcom_scmi_client");
MODULE_DESCRIPTION("Feature Control SCMI driver");
MODULE_LICENSE("GPL");
