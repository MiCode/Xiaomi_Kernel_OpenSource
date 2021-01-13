// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/of_device.h>
#include <linux/suspend.h>
#include <linux/cpumask.h>

enum thermal_pause_levels {
	THERMAL_NO_CPU_PAUSE,
	THERMAL_GROUP_CPU_PAUSE,

	/* define new pause levels above this line */
	MAX_THERMAL_PAUSE_LEVEL
};

struct thermal_pause_cdev {
	struct list_head		node;
	unsigned long			cpu_mask;
	bool				thermal_pause_level;
	struct thermal_cooling_device	*cdev;
	struct device_node		*np;
	struct work_struct		reg_work;
};

static DEFINE_MUTEX(cpus_pause_lock);
static LIST_HEAD(thermal_pause_cdev_list);
static atomic_t in_suspend;
static struct cpumask cpus_paused_by_thermal;
static struct cpumask cpus_in_max_cooling_level;

static BLOCKING_NOTIFIER_HEAD(multi_max_cooling_level_notifer);

void cpu_cooling_multi_max_level_notifier_register(struct notifier_block *n)
{
	blocking_notifier_chain_register(&multi_max_cooling_level_notifer, n);
}

void cpu_cooling_multi_max_level_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&multi_max_cooling_level_notifer, n);
}

const struct cpumask *cpu_cooling_multi_get_max_level_cpumask(void)
{
	return &cpus_in_max_cooling_level;
}

static int thermal_pause_pm_notify_suspend(void)
{
	struct thermal_pause_cdev *thermal_pause_cdev;
	unsigned int cpu;
	cpumask_var_t cpus_paused;

	if (!zalloc_cpumask_var(&cpus_paused, GFP_KERNEL))
		return -ENOMEM;

	mutex_lock(&cpus_pause_lock);

	list_for_each_entry(thermal_pause_cdev, &thermal_pause_cdev_list,
			    node) {

		if (thermal_pause_cdev->thermal_pause_level) {

			for_each_cpu(cpu, to_cpumask(
				     &thermal_pause_cdev->cpu_mask)) {

				if (cpu_online(cpu) &&
				    !cpumask_test_and_set_cpu(cpu,
				      &cpus_paused_by_thermal)) {

					mutex_unlock(&cpus_pause_lock);

					/* need a variable mask, pause_cpus can write */
					cpumask_set_cpu(cpu, cpus_paused);

					if (pause_cpus(cpus_paused)) {
						cpumask_clear_cpu(cpu,
						  &cpus_paused_by_thermal);
					}

					cpumask_clear(cpus_paused);

					mutex_lock(&cpus_pause_lock);
				}
			}
		}
	}

	mutex_unlock(&cpus_pause_lock);
	atomic_set(&in_suspend, 0);
	free_cpumask_var(cpus_paused);

	return 0;
}

static int thermal_pause_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	int err = 0;

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		atomic_set(&in_suspend, 1);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		err = thermal_pause_pm_notify_suspend();
		break;
	default:
		break;
	}

	return err;
}

static struct notifier_block thermal_pause_pm_nb = {
	.notifier_call = thermal_pause_pm_notify,
};

static int thermal_pause_hp_offline(unsigned int offline_cpu)
{
	struct thermal_pause_cdev *thermal_pause_cdev;

	mutex_lock(&cpus_pause_lock);

	list_for_each_entry(thermal_pause_cdev, &thermal_pause_cdev_list, node) {
		if (!thermal_pause_cdev->cdev)
			break;

		if (cpumask_test_cpu(offline_cpu,
				     to_cpumask(&thermal_pause_cdev->cpu_mask)))

			cpumask_clear_cpu(offline_cpu,
					  &cpus_paused_by_thermal);
		break;
	}

	mutex_unlock(&cpus_pause_lock);

	return 0;
}

static int thermal_pause_hp_online(unsigned int online_cpu)
{
	struct thermal_pause_cdev *thermal_pause_cdev;
	int ret = 0;

	if (atomic_read(&in_suspend))
		return 0;

	mutex_lock(&cpus_pause_lock);

	list_for_each_entry(thermal_pause_cdev, &thermal_pause_cdev_list, node) {
		if (thermal_pause_cdev->cdev) {
			if (thermal_pause_cdev->thermal_pause_level)
				ret = NOTIFY_BAD;
		} else {
			queue_work(system_highpri_wq,
				   &thermal_pause_cdev->reg_work);
		}
		break;
	}

	mutex_unlock(&cpus_pause_lock);

	return ret;
}

/**
 * thermal_pause_cpus_locked - function to pause a group of cpus at
 *                         the specified level.
 *
 * @thermal_pause_cdev: the pause device
 *
 * function to handle setting the current cpus paused by
 * this driver for the mask specified in the device.
 * it assumes the mutex is locked.
 *
 * Returns 0 if CPUs were paused, error otherwise
 */
static int thermal_pause_cpus_locked(struct thermal_pause_cdev *thermal_pause_cdev)
{
	int cpu = 0;
	int ret = -ENODEV;
	cpumask_var_t cpus_paused;

	if (!zalloc_cpumask_var(&cpus_paused, GFP_KERNEL))
		return -ENOMEM;

	cpumask_andnot(cpus_paused, to_cpumask(&thermal_pause_cdev->cpu_mask),
		       &cpus_paused_by_thermal);

	if (cpumask_any(cpus_paused) < nr_cpu_ids) {

		mutex_unlock(&cpus_pause_lock);

		if (pause_cpus(cpus_paused) == 0) {
			cpumask_or(&cpus_paused_by_thermal,
				   &cpus_paused_by_thermal, cpus_paused);
			ret = 0;
		}

		mutex_lock(&cpus_pause_lock);

		for_each_cpu(cpu, cpus_paused)
			blocking_notifier_call_chain(
				&multi_max_cooling_level_notifer,
				1, (void *)(long)cpu);
	}

	cpumask_copy(&cpus_in_max_cooling_level, &cpus_paused_by_thermal);

	free_cpumask_var(cpus_paused);

	return ret;
}

/**
 * cpu_unpause_cpus_locked - function to unpause a
 *       group of cpus in the mask for this cdev
 *
 * @thermal_pause_cdev: the pause device
 *
 * function to handle enabling the group of cpus in the cdev
 *
 * Returns 0 if CPUs were unpaused,
 */
static int cpu_unpause_cpus_locked(struct thermal_pause_cdev *thermal_pause_cdev)
{
	int cpu = 0;
	int ret = -ENODEV;

	cpumask_var_t cpus_unpaused;

	if (!zalloc_cpumask_var(&cpus_unpaused, GFP_KERNEL))
		return -ENOMEM;

	cpumask_and(cpus_unpaused, &cpus_paused_by_thermal,
		    to_cpumask(&thermal_pause_cdev->cpu_mask));

	if (cpumask_any(cpus_unpaused) < nr_cpu_ids) {
		mutex_unlock(&cpus_pause_lock);
		ret = resume_cpus(cpus_unpaused);
		mutex_lock(&cpus_pause_lock);
	}

	for_each_cpu(cpu, &cpus_paused_by_thermal)
		blocking_notifier_call_chain(
			&multi_max_cooling_level_notifer,
			0, (void *)(long)cpu);

	cpumask_andnot(&cpus_paused_by_thermal, &cpus_paused_by_thermal,
		       cpus_unpaused);

	cpumask_clear(&cpus_in_max_cooling_level);

	free_cpumask_var(cpus_unpaused);

	return ret;
}

/**
 * thermal_pause_set_cur_state - callback function to set the current cooling
 *				level.
 * @cdev: thermal cooling device pointer.
 * @level: set this variable to the current cooling level.
 *
 * Callback for the thermal cooling device to change the cpu pause
 * current cooling level.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int thermal_pause_set_cur_state(struct thermal_cooling_device *cdev,
				   unsigned long level)
{
	struct thermal_pause_cdev *thermal_pause_cdev = cdev->devdata;
	int ret = 0;

	if (level > MAX_THERMAL_PAUSE_LEVEL - 1)
		return -EINVAL;

	if (thermal_pause_cdev->thermal_pause_level == level)
		return 0;

	mutex_lock(&cpus_pause_lock);

	thermal_pause_cdev->thermal_pause_level = level;

	if (level > THERMAL_NO_CPU_PAUSE && level <= (MAX_THERMAL_PAUSE_LEVEL - 1))
		ret = thermal_pause_cpus_locked(thermal_pause_cdev);
	else
		ret = cpu_unpause_cpus_locked(thermal_pause_cdev);

	mutex_unlock(&cpus_pause_lock);

	return ret;
}

/**
 * thermal_pause_get_cur_state - callback function to get the current cooling
 *				state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the cpu pause
 * current cooling level
 *
 * Return: 0 on success, an error code otherwise.
 */
static int thermal_pause_get_cur_state(struct thermal_cooling_device *cdev,
				   unsigned long *level)
{
	struct thermal_pause_cdev *thermal_pause_cdev = cdev->devdata;

	*level = thermal_pause_cdev->thermal_pause_level;

	return 0;
}

/**
 * thermal_pause_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @level: fill this variable with the max cooling level
 *
 * Callback for the thermal cooling device to return the cpu
 * pause max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int thermal_pause_get_max_state(struct thermal_cooling_device *cdev,
				   unsigned long *level)
{
	*level = MAX_THERMAL_PAUSE_LEVEL - 1;
	return 0;
}

static struct thermal_cooling_device_ops thermal_pause_cooling_ops = {
	.get_max_state = thermal_pause_get_max_state,
	.get_cur_state = thermal_pause_get_cur_state,
	.set_cur_state = thermal_pause_set_cur_state,
};

static void thermal_pause_register_cdev(struct work_struct *work)
{
	struct thermal_pause_cdev *thermal_pause_cdev =
			container_of(work, struct thermal_pause_cdev, reg_work);
	char cdev_name[THERMAL_NAME_LENGTH] = "";
	int ret = 0;

	snprintf(cdev_name, THERMAL_NAME_LENGTH, "thermal-pause%X",
			thermal_pause_cdev->cpu_mask);

	thermal_pause_cdev->cdev = thermal_of_cooling_device_register(
					thermal_pause_cdev->np,
					cdev_name,
					thermal_pause_cdev,
					&thermal_pause_cooling_ops);

	if (IS_ERR(thermal_pause_cdev->cdev)) {
		ret = PTR_ERR(thermal_pause_cdev->cdev);
		pr_err("Cooling register failed for %s, ret:%d\n",
			cdev_name, ret);
		thermal_pause_cdev->cdev = NULL;
		return;
	}

	pr_debug("Cooling device [%s] registered.\n", cdev_name);
}

static int thermal_pause_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *subsys_np = NULL;
	struct thermal_pause_cdev *thermal_pause_cdev = NULL;
	struct device_node *np = pdev->dev.of_node;
	u32 cpu_mask;
	char cdev_name[THERMAL_NAME_LENGTH] = "";

	INIT_LIST_HEAD(&thermal_pause_cdev_list);

	for_each_available_child_of_node(np, subsys_np) {

		/* get the mask of cpus for this cdev */
		if (of_property_read_u32(subsys_np, "qcom,mask", &cpu_mask))
			continue;

		thermal_pause_cdev = devm_kzalloc(&pdev->dev,
				sizeof(*thermal_pause_cdev), GFP_KERNEL);

		if (!thermal_pause_cdev) {
			of_node_put(subsys_np);
			return -ENOMEM;
		}

		thermal_pause_cdev->thermal_pause_level = false;
		thermal_pause_cdev->cdev = NULL;
		thermal_pause_cdev->np = subsys_np;
		thermal_pause_cdev->cpu_mask = cpu_mask;

		snprintf(cdev_name, THERMAL_NAME_LENGTH, "thermal-pause%X",
			 thermal_pause_cdev->cpu_mask);

		thermal_pause_cdev->cdev = thermal_of_cooling_device_register(
			thermal_pause_cdev->np,
			cdev_name,
			thermal_pause_cdev,
			&thermal_pause_cooling_ops);

		INIT_WORK(&thermal_pause_cdev->reg_work,
				thermal_pause_register_cdev);
		list_add(&thermal_pause_cdev->node, &thermal_pause_cdev_list);
	}

	atomic_set(&in_suspend, 0);
	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "thermal-pause/cdev:online",
				thermal_pause_hp_online, thermal_pause_hp_offline);
	if (ret < 0)
		return ret;
	register_pm_notifier(&thermal_pause_pm_nb);
	cpumask_clear(&cpus_in_max_cooling_level);

	return 0;
}

static const struct of_device_id thermal_pause_match[] = {
	{ .compatible = "qcom,thermal-pause", },
	{},
};

static struct platform_driver thermal_pause_driver = {
	.probe		= thermal_pause_probe,
	.driver		= {
		.name		= KBUILD_MODNAME,
		.of_match_table = thermal_pause_match,
	},
};

module_platform_driver(thermal_pause_driver);
MODULE_LICENSE("GPL v2");
