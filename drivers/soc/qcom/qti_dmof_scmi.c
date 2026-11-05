// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
#include <linux/cpu_phys_log_map.h>

#define DMOF_ALGO_STR	(0x444D4F46) /* DMOF (Disable Memcpy Optimization Feature) ASCII */

enum dmof_param_ids {
	PARAM_DISABLE_DMOF = 1,
};

enum command {
	COMMAND_INIT = -1,
	COMMAND_SHOW,
	COMMAND_STORE,
};

struct qcom_dmof_dd {
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
	struct device *dev;
	u32 val;
	u32 req_val;
	int ret;
};

static DEFINE_PER_CPU(bool, cpu_is_on);
static DEFINE_PER_CPU(bool, need_ack);
static struct qcom_dmof_dd *qcom_dmof_dd;


static ssize_t disable_memcpy_optimization_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qcom_dmof_dd *fds = qcom_dmof_dd;
	int ret;
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
		wait_event_killable(fds->waitq[cpu], !per_cpu(need_ack, cpu));
		if (fds->ret < 0)
			goto cleanup;
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
		wait_event_killable(fds->waitq[i], !per_cpu(need_ack, i));
		if (fds->ret < 0) {
			dev_err(fds->dev, "dmof broken now:cpu:%d\n", i);
			WARN_ON(1);
			break;
		}

		fds->curr_val[cpu] = fds->req_val;
	}

unlock_cpu_readlock:
	fds->cmd = COMMAND_INIT;
	mutex_unlock(&fds->lock);
	cpus_read_unlock();

	return ((ret < 0) ? ret : count);
}

static ssize_t disable_memcpy_optimization_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qcom_dmof_dd *fds = qcom_dmof_dd;
	int val = 1;
	int cpu;

	cpus_read_lock();
	mutex_lock(&fds->lock);
	fds->cmd = COMMAND_SHOW;
	for_each_possible_cpu(cpu) {
		if (!per_cpu(cpu_is_on, cpu))
			continue;

		per_cpu(need_ack, cpu) = true;
		wake_up(&fds->waitq[cpu]);
		wait_event_killable(fds->waitq[cpu], !per_cpu(need_ack, cpu));
		if (fds->ret < 0)
			goto cleanup;
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

	if (fds->ret < 0)
		return fds->ret;
	return scnprintf(buf, PAGE_SIZE, "%u\n", le32_to_cpu(fds->val));
}

static DEVICE_ATTR_RW(disable_memcpy_optimization);

static const struct device_attribute *dmof_attrs[] = {
	&dev_attr_disable_memcpy_optimization,
	NULL,
};

static int qcom_dmof_kthread_fn(void *data)
{
	struct qcom_dmof_dd *fds = qcom_dmof_dd;
	u32 cpu = *(u32 *)data;
	/* [0] is for cpu id and [1] is for enable/disable dmof */
	u32 buf[2];

	while (!(kthread_should_stop())) {
		if (kthread_should_park()) {
			kthread_parkme();
			continue;
		}

repeat:
		wait_event_killable(fds->waitq[cpu], per_cpu(need_ack, cpu));
		if (fds->ret < 0)
			break;

		BUG_ON(get_cpu() != cpu);
		put_cpu();

		buf[0] = cpu_logical_to_phys(cpu);
		switch (fds->cmd) {
		case COMMAND_INIT:
			break;
		case COMMAND_STORE:
			__pm_stay_awake(fds->ws[cpu]);
			buf[1] = cpu_to_le32(fds->req_val);
			fds->ret = fds->ops->set_param(fds->ph, &buf, DMOF_ALGO_STR,
						       PARAM_DISABLE_DMOF, sizeof(buf));
			if (fds->ret < 0)
				dev_err(fds->dev, "Failed to set param for cpu:%u\n", cpu);

			per_cpu(need_ack, cpu) = false;
			wake_up(&fds->waitq[cpu]);
			__pm_relax(fds->ws[cpu]);
			goto repeat;
		case COMMAND_SHOW:
			__pm_stay_awake(fds->ws[cpu]);
			fds->ret = fds->ops->get_param(fds->ph, &buf, DMOF_ALGO_STR,
					       PARAM_DISABLE_DMOF, sizeof(u32), sizeof(u32));
			if (fds->ret < 0)
				dev_err(fds->dev, "Failed to get param for cpu:%u\n", cpu);
			per_cpu(need_ack, cpu) = false;
			fds->get_val[cpu] = le32_to_cpu(buf[0]);
			wake_up(&fds->waitq[cpu]);
			__pm_relax(fds->ws[cpu]);
			break;
		}
	}

	return 0;
}

static void smp_destroy_threads(struct qcom_dmof_dd *fds)
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

static int smp_create_thread(struct qcom_dmof_dd *fds, u32 cpu)
{
	struct task_struct *tsk;
	u32 *td;

	td = devm_kzalloc(fds->dev, sizeof(*td), GFP_KERNEL);
	if (!td)
		return -ENOMEM;

	*td = cpu;
	tsk = kthread_create_on_cpu(qcom_dmof_kthread_fn, td, cpu, fds->thread_comm);
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
	struct qcom_dmof_dd *fds = qcom_dmof_dd;

	mutex_lock(&fds->lock);
	per_cpu(cpu_is_on, cpu) = false;
	per_cpu(need_ack, cpu) = false;
	mutex_unlock(&fds->lock);

	return 0;
}

static int cpu_up_notifier(unsigned int cpu)
{
	struct qcom_dmof_dd *fds = qcom_dmof_dd;
	struct task_struct *tsk;

	mutex_lock(&fds->lock);
	tsk = fds->store[cpu];
	kthread_bind_mask(tsk, cpumask_of(cpu));

	if (fds->curr_val[cpu] == fds->val)
		goto cpu_on;

	fds->cmd = COMMAND_STORE;
	fds->ret = 0;
	per_cpu(need_ack, cpu) = true;
	wake_up(&fds->waitq[cpu]);

	wait_event_killable(fds->waitq[cpu], !per_cpu(need_ack, cpu));
	if (fds->ret >= 0)
		fds->curr_val[cpu] = fds->val;

cpu_on:
	per_cpu(need_ack, cpu) = false;
	per_cpu(cpu_is_on, cpu) = true;
	mutex_unlock(&fds->lock);

	return 0;
}

static int qcom_dmof_probe(struct platform_device *pdev)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	const struct qcom_scmi_vendor_ops *ops;
	struct scmi_protocol_handle *ph;
	struct scmi_device *scmi_dev;
	struct device *dev_root;
	struct task_struct *tsk;
	u32 cpu;
	int ret;

	qcom_dmof_dd = devm_kzalloc(&pdev->dev, sizeof(*qcom_dmof_dd), GFP_KERNEL);
	if (!qcom_dmof_dd)
		return -ENOMEM;

	dev_root = bus_get_dev_root(&cpu_subsys);
	if (!dev_root)
		return -EPROBE_DEFER;

	scmi_dev = get_qcom_scmi_device();
	if (IS_ERR(scmi_dev))
		return dev_err_probe(&pdev->dev, PTR_ERR(scmi_dev), "unable to get scmi_dev");

	ops = scmi_dev->handle->devm_protocol_get(scmi_dev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops)) {
		ret = PTR_ERR(ops);
		return ret;
	}

	mutex_init(&qcom_dmof_dd->lock);
	qcom_dmof_dd->dev = &pdev->dev;
	qcom_dmof_dd->ops = ops;
	qcom_dmof_dd->ph = ph;
	qcom_dmof_dd->thread_comm = "cpudmof/%u";
	qcom_dmof_dd->cmd = COMMAND_INIT;
	qcom_dmof_dd->ret = 0;
	qcom_dmof_dd->val = 1;

	qcom_dmof_dd->waitq = devm_kcalloc(&pdev->dev, num_possible_cpus(),
					   sizeof(wait_queue_head_t), GFP_KERNEL);
	if (!qcom_dmof_dd->waitq)
		return -ENOMEM;

	qcom_dmof_dd->ws = devm_kcalloc(&pdev->dev, num_possible_cpus(),
					sizeof(struct wakeup_source *), GFP_KERNEL);
	if (!qcom_dmof_dd->ws)
		return -ENOMEM;

	qcom_dmof_dd->store = devm_kcalloc(&pdev->dev, num_possible_cpus(),
					   sizeof(struct task_struct *), GFP_KERNEL);
	if (!qcom_dmof_dd->store)
		return -ENOMEM;

	qcom_dmof_dd->curr_val = devm_kcalloc(&pdev->dev,
					      num_possible_cpus(), sizeof(u32),
					      GFP_KERNEL);
	if (!qcom_dmof_dd->curr_val)
		return -ENOMEM;

	qcom_dmof_dd->get_val = devm_kcalloc(&pdev->dev,
					      num_possible_cpus(), sizeof(u32),
					      GFP_KERNEL);
	if (!qcom_dmof_dd->get_val)
		return -ENOMEM;

	cpus_read_lock();
	for_each_possible_cpu(cpu) {
		if (cpu_online(cpu))
			per_cpu(cpu_is_on, cpu) = true;

		init_waitqueue_head(&qcom_dmof_dd->waitq[cpu]);
		per_cpu(need_ack, cpu) = false;
		qcom_dmof_dd->ws[cpu] = wakeup_source_register(NULL, "dmof_ws");
		/* Earlier writes should be visible after this */
		smp_wmb();
		ret = smp_create_thread(qcom_dmof_dd, cpu);
		if (ret) {
			dev_err(&pdev->dev, "error during smp_create_thread ret = %d\n", ret);
			smp_destroy_threads(qcom_dmof_dd);
			cpus_read_unlock();
			return ret;
		}

		tsk = qcom_dmof_dd->store[cpu];
		if (tsk) {
			sched_setscheduler_nocheck(tsk, SCHED_FIFO, &param);
			kthread_unpark(tsk);
		}
	}

	ret = cpuhp_setup_state_nocalls_cpuslocked(CPUHP_AP_ONLINE_DYN, "dmof_cpu_hotplug",
						   cpu_up_notifier, cpu_down_notifier);
	cpus_read_unlock();

	ret = device_create_file(&pdev->dev, dmof_attrs[0]);

	put_device(dev_root);

	return 0;
}


static const struct of_device_id qcom_dmof_match[] = {
	{ .compatible = "qcom,dmof"},
	{}
};

static struct platform_driver qcom_dmof_driver = {
	.probe = qcom_dmof_probe,
	.driver         = {
		.name   = "qcom-dmof",
		.of_match_table = qcom_dmof_match,
	},
};



module_platform_driver(qcom_dmof_driver);

MODULE_SOFTDEP("pre: qcom_scmi_client");
MODULE_DESCRIPTION("QTI DMOF SCMI driver");
MODULE_LICENSE("GPL");
