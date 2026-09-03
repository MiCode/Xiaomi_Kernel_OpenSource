// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd
 */

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/wait.h>
#include "unisoc_cpufreq_info.h"
#include "unisoc_cpuidle_info.h"
#include "unisoc_gpu_info.h"
#include "unisoc_pnp_common.h"

int sprd_pnp_log_force;
module_param(sprd_pnp_log_force, int, 0644);
MODULE_PARM_DESC(sprd_pnp_log_force, "sprd pnp log force out (default: 0)");

struct sprd_pnp_platform_info *sprd_pnp_data;
static char *envp[] = { NULL, NULL };

static const struct proc_ops cpu_freq_fops = {
	.proc_open		= sprd_cpu_freq_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static const struct proc_ops cpu_idle_fops = {
	.proc_open		= sprd_cpu_idle_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

#if ENABLE_PNP_GPU_INFO
static const struct proc_ops gpu_freq_fops = {
	.proc_open		= sprd_gpu_freq_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};
#endif

ssize_t common_en_read(bool *debug_en, struct file *file,
		char __user *in_buf, size_t count, loff_t *ppos)
{
	char out_buf[5];
	size_t len;

	len = sprintf(out_buf, "%d\n", *debug_en);
	return simple_read_from_buffer(in_buf, count, ppos, out_buf, len);
}

ssize_t common_en_write(bool *debug_en, struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	bool enable;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtobool_from_user(user_buf, count, &enable);
	if (ret)
		return -EINVAL;

	*debug_en = enable;

	return count;
}

static const struct proc_ops cpu_en_fops = {
	.proc_open	= simple_open,
	.proc_read	= cpu_en_read,
	.proc_write	= cpu_en_write,
	.proc_lseek	= default_llseek,
	.proc_poll	= cpu_en_poll,
};

#if ENABLE_PNP_GPU_INFO
static const struct proc_ops gpu_en_fops = {
	.proc_open	= simple_open,
	.proc_read	= gpu_en_read,
	.proc_write	= gpu_en_write,
	.proc_lseek	= default_llseek,
};
#endif

static int sprd_pnp_proc_init(void)
{
	struct proc_dir_entry *dir;
	struct proc_dir_entry *fle;
	int ret = 0;

	dir = proc_mkdir("sprd_pnp", NULL);
	if (!dir) {
		SPRD_PNP_ERR("Proc dir create failed\n");
		return -EINVAL;
	}

	fle = proc_create("cpu_debug_en", 0660, dir, &cpu_en_fops);
	if (!fle) {
		ret = -EINVAL;
		goto error_cpu_en;
	}

	fle = proc_create("cpu_freq_info", 0660, dir, &cpu_freq_fops);
	if (!fle) {
		ret = -EINVAL;
		goto error_cpu_freq;
	}

	fle = proc_create("cpu_idle_info", 0660, dir, &cpu_idle_fops);
	if (!fle) {
		ret = -EINVAL;
		goto error_cpu_idle;
	}
#if !ENABLE_PNP_GPU_INFO
	return ret;
#else
	fle = proc_create("gpu_debug_en", 0660, dir, &gpu_en_fops);
	if (!fle) {
		ret = -EINVAL;
		goto error_gpu_en;
	}

	fle = proc_create("gpu_freq_info", 0660, dir, &gpu_freq_fops);
	if (!fle) {
		ret = -EINVAL;
		goto error_gpu_freq;
	}
	return ret;
error_gpu_freq:
	remove_proc_entry("gpu_debug_en", dir);

error_gpu_en:
	remove_proc_entry("cpu_idle_info", dir);
#endif

error_cpu_idle:
	remove_proc_entry("cpu_freq_info", dir);

error_cpu_freq:
	remove_proc_entry("cpu_debug_en", dir);

error_cpu_en:
	remove_proc_entry("sprd_pnp", NULL);

	return ret;
}

static int notify_wait_thread(void *data)
{
	char pnp_state[100] = {0};

	envp[0] = pnp_state;

	while (!kthread_should_stop()) {

		wait_event_interruptible(sprd_pnp_data->notify_wq,
							sprd_pnp_data->wake_up_condition);

		sprd_pnp_data->wake_up_condition = 0;

		snprintf(pnp_state, ARRAY_SIZE(pnp_state),
		"EVENT=%s, VALUE=%d", sprd_pnp_data->event.name, sprd_pnp_data->event.value);

		kobject_uevent_env(&sprd_pnp_data->dev->kobj, KOBJ_CHANGE, envp);
		SPRD_PNP_INFO("%s,EVENT=%s, VALUE=%d\n",
			__func__, sprd_pnp_data->event.name, sprd_pnp_data->event.value);
	}
	return 0;
}

static int sprd_pnp_probe(struct platform_device *pdev)
{
	int ret;

	sprd_pnp_data = devm_kzalloc(&pdev->dev, sizeof(struct sprd_pnp_platform_info), GFP_KERNEL);
	if (!sprd_pnp_data)
		return -ENOMEM;
	init_waitqueue_head(&sprd_pnp_data->notify_wq);
	init_waitqueue_head(&sprd_pnp_data->poll_wq);

	sprd_pnp_data->dev = &pdev->dev;
	sprd_pnp_data->wait_thread_task = kthread_create(notify_wait_thread,
								NULL, "pnp_notify_wait");

	if (IS_ERR(sprd_pnp_data->wait_thread_task)) {
		SPRD_PNP_ERR("create wait thread failed\n");
		return PTR_ERR(sprd_pnp_data->wait_thread_task);
	}
	wake_up_process(sprd_pnp_data->wait_thread_task);

	ret = sprd_pnp_proc_init();
	if (ret) {
		SPRD_PNP_ERR("failed to init pnp proc\n");
		kthread_stop(sprd_pnp_data->wait_thread_task);
		return ret;
	}

	return 0;
}

/**
 * sprd_pnp_remove - remove the pnp driver
 */

static int sprd_pnp_remove(struct platform_device *pdev)
{
	int ret;

	ret = unregister_cpufreq_notify_info(sprd_pnp_data);
	if (ret) {
		SPRD_PNP_ERR("%s: Failed to unregister cpu freq trace info\n", __func__);
		return ret;
	}

	ret = unregister_cpuidle_trace_info(sprd_pnp_data);
	if (ret) {
		SPRD_PNP_ERR("%s: Failed to unregister cpu idle trace info\n", __func__);
		return ret;
	}
#if ENABLE_PNP_GPU_INFO
	ret = unregister_gpufreq_trace_info(sprd_pnp_data);
	if (ret) {
		SPRD_PNP_ERR("%s: Failed to unregister gpu freq trace info\n", __func__);
		return ret;
	}
#endif

	if (sprd_pnp_data->wait_thread_task) {
		ret = kthread_stop(sprd_pnp_data->wait_thread_task);
		if (ret) {
			SPRD_PNP_ERR("%s: kthread_stop failed!\n", __func__);
			return ret;
		}
		sprd_pnp_data->wait_thread_task = NULL;
	}

	remove_proc_entry("sprd_pnp", NULL);
	kfree(sprd_pnp_data);
	sprd_pnp_data = NULL;

	return 0;
}

static const struct of_device_id sprd_pnp_of_match[] = {
{
	.compatible = "sprd,pnp",
},
{},
};

static struct platform_driver sprd_pnp_driver = {
	.probe = sprd_pnp_probe,
	.remove = sprd_pnp_remove,
	.driver = {
		.name = "sprd-pnp",
		.of_match_table = sprd_pnp_of_match,
	},
};

module_platform_driver(sprd_pnp_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd pnp driver");
