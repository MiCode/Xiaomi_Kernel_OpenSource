// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/of_device.h>
#include <linux/suspend.h>

#define CPU_ISOLATE_LEVEL 1

struct cpu_isolate_cdev {
	struct list_head node;
	int cpu_id;
	bool cpu_isolate_state;
	struct thermal_cooling_device *cdev;
	struct device_node *np;
	struct work_struct reg_work;
};

static DEFINE_MUTEX(cpu_isolate_lock);
static LIST_HEAD(cpu_isolate_cdev_list);
static atomic_t in_suspend;
static struct cpumask cpus_pending_online;
static struct cpumask cpus_isolated_by_thermal;

static struct cpumask cpus_in_max_cooling_level;
static BLOCKING_NOTIFIER_HEAD(cpu_max_cooling_level_notifer);

void cpu_cooling_max_level_notifier_register(struct notifier_block *n)
{
	blocking_notifier_chain_register(&cpu_max_cooling_level_notifer, n);
}

void cpu_cooling_max_level_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&cpu_max_cooling_level_notifer, n);
}

const struct cpumask *cpu_cooling_get_max_level_cpumask(void)
{
	return &cpus_in_max_cooling_level;
}

static int cpu_isolate_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	struct cpu_isolate_cdev *cpu_isolate_cdev;
	unsigned int cpu;

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		atomic_set(&in_suspend, 1);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&cpu_isolate_lock);
		list_for_each_entry(cpu_isolate_cdev, &cpu_isolate_cdev_list,
					node) {
			if (cpu_isolate_cdev->cpu_id == -1)
				continue;
			if (cpu_isolate_cdev->cpu_isolate_state) {
				cpu = cpu_isolate_cdev->cpu_id;
				if (cpu_online(cpu) &&
					!cpumask_test_and_set_cpu(cpu,
					&cpus_isolated_by_thermal)) {
					mutex_unlock(&cpu_isolate_lock);
					if (sched_isolate_cpu(cpu))
						cpumask_clear_cpu(cpu,
						&cpus_isolated_by_thermal);
					mutex_lock(&cpu_isolate_lock);
				}
				continue;
			}
		}
		mutex_unlock(&cpu_isolate_lock);
		atomic_set(&in_suspend, 0);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block cpu_isolate_pm_nb = {
	.notifier_call = cpu_isolate_pm_notify,
};

static int cpu_isolate_hp_offline(unsigned int offline_cpu)
{
	struct cpu_isolate_cdev *cpu_isolate_cdev;

	mutex_lock(&cpu_isolate_lock);
	list_for_each_entry(cpu_isolate_cdev, &cpu_isolate_cdev_list, node) {
		if (offline_cpu != cpu_isolate_cdev->cpu_id)
			continue;

		if (!cpu_isolate_cdev->cdev)
			break;

		if ((cpu_isolate_cdev->cpu_isolate_state)
			&& (cpumask_test_and_clear_cpu(offline_cpu,
			&cpus_isolated_by_thermal)))
			sched_unisolate_cpu_unlocked(offline_cpu);
		break;
	}
	mutex_unlock(&cpu_isolate_lock);

	return 0;
}

static int cpu_isolate_hp_online(unsigned int online_cpu)
{
	struct cpu_isolate_cdev *cpu_isolate_cdev;
	int ret = 0;

	if (atomic_read(&in_suspend))
		return 0;

	mutex_lock(&cpu_isolate_lock);
	list_for_each_entry(cpu_isolate_cdev, &cpu_isolate_cdev_list, node) {
		if (online_cpu != cpu_isolate_cdev->cpu_id)
			continue;

		if (cpu_isolate_cdev->cdev) {
			if (cpu_isolate_cdev->cpu_isolate_state) {
				cpumask_set_cpu(online_cpu,
						&cpus_pending_online);
				ret = NOTIFY_BAD;
			}
		} else {
			queue_work(system_highpri_wq,
					&cpu_isolate_cdev->reg_work);
		}

		break;
	}
	mutex_unlock(&cpu_isolate_lock);

	return ret;
}

/**
 * cpu_isolate_set_cur_state - callback function to set the current cooling
 *				state.
 * @cdev: thermal cooling device pointer.
 * @state: set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the cpu isolation
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int cpu_isolate_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct cpu_isolate_cdev *cpu_isolate_cdev = cdev->devdata;
	struct device *cpu_dev;
	int ret = 0;
	int cpu = 0;

	if (cpu_isolate_cdev->cpu_id == -1)
		return -ENODEV;

	/* Request state should be less than max_level */
	if (state > CPU_ISOLATE_LEVEL)
		return -EINVAL;

	state = !!state;
	/* Check if the old cooling action is same as new cooling action */
	if (cpu_isolate_cdev->cpu_isolate_state == state)
		return 0;

	mutex_lock(&cpu_isolate_lock);
	cpu = cpu_isolate_cdev->cpu_id;
	cpu_isolate_cdev->cpu_isolate_state = state;
	if (state == CPU_ISOLATE_LEVEL) {
		if (cpu_online(cpu) &&
			(!cpumask_test_and_set_cpu(cpu,
			&cpus_isolated_by_thermal))) {
			mutex_unlock(&cpu_isolate_lock);
			if (sched_isolate_cpu(cpu))
				cpumask_clear_cpu(cpu,
					&cpus_isolated_by_thermal);
			mutex_lock(&cpu_isolate_lock);
		}
		cpumask_set_cpu(cpu, &cpus_in_max_cooling_level);
		blocking_notifier_call_chain(&cpu_max_cooling_level_notifer,
						1, (void *)(long)cpu);
	} else {
		if (cpumask_test_and_clear_cpu(cpu, &cpus_pending_online)) {
			cpu_dev = get_cpu_device(cpu);
			if (!cpu_dev) {
				pr_err("CPU:%d cpu dev error\n", cpu);
				mutex_unlock(&cpu_isolate_lock);
				return ret;
			}
			mutex_unlock(&cpu_isolate_lock);
			ret = device_online(cpu_dev);
			if (ret)
				pr_err("CPU:%d online error:%d\n", cpu, ret);
			return ret;
		} else if (cpumask_test_and_clear_cpu(cpu,
			&cpus_isolated_by_thermal)) {
			mutex_unlock(&cpu_isolate_lock);
			sched_unisolate_cpu(cpu);
			mutex_lock(&cpu_isolate_lock);
		}
		cpumask_clear_cpu(cpu, &cpus_in_max_cooling_level);
		blocking_notifier_call_chain(&cpu_max_cooling_level_notifer,
						0, (void *)(long)cpu);
	}
	mutex_unlock(&cpu_isolate_lock);

	return 0;
}

/**
 * cpu_isolate_get_cur_state - callback function to get the current cooling
 *				state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the cpu isolation
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int cpu_isolate_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpu_isolate_cdev *cpu_isolate_cdev = cdev->devdata;

	*state = (cpu_isolate_cdev->cpu_isolate_state) ?
			CPU_ISOLATE_LEVEL : 0;

	return 0;
}

/**
 * cpu_isolate_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the cpu
 * isolation max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int cpu_isolate_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = CPU_ISOLATE_LEVEL;
	return 0;
}

static struct thermal_cooling_device_ops cpu_isolate_cooling_ops = {
	.get_max_state = cpu_isolate_get_max_state,
	.get_cur_state = cpu_isolate_get_cur_state,
	.set_cur_state = cpu_isolate_set_cur_state,
};

static void cpu_isolate_register_cdev(struct work_struct *work)
{
	struct cpu_isolate_cdev *cpu_isolate_cdev =
			container_of(work, struct cpu_isolate_cdev, reg_work);
	char cdev_name[THERMAL_NAME_LENGTH] = "";
	int ret = 0;

	snprintf(cdev_name, THERMAL_NAME_LENGTH, "cpu-isolate%d",
			cpu_isolate_cdev->cpu_id);

	cpu_isolate_cdev->cdev = thermal_of_cooling_device_register(
					cpu_isolate_cdev->np,
					cdev_name,
					cpu_isolate_cdev,
					&cpu_isolate_cooling_ops);
	if (IS_ERR(cpu_isolate_cdev->cdev)) {
		ret = PTR_ERR(cpu_isolate_cdev->cdev);
		pr_err("Cooling register failed for %s, ret:%d\n",
			cdev_name, ret);
		cpu_isolate_cdev->cdev = NULL;
		return;
	}
	pr_debug("Cooling device [%s] registered.\n", cdev_name);
}

static int cpu_isolate_probe(struct platform_device *pdev)
{
	int ret = 0, cpu = 0;
	struct device_node *dev_phandle, *subsys_np;
	struct device *cpu_dev;
	struct cpu_isolate_cdev *cpu_isolate_cdev = NULL;
	struct device_node *np = pdev->dev.of_node;

	INIT_LIST_HEAD(&cpu_isolate_cdev_list);
	for_each_available_child_of_node(np, subsys_np) {
		cpu_isolate_cdev = devm_kzalloc(&pdev->dev,
				sizeof(*cpu_isolate_cdev), GFP_KERNEL);
		if (!cpu_isolate_cdev)
			return -ENOMEM;
		cpu_isolate_cdev->cpu_id = -1;
		cpu_isolate_cdev->cpu_isolate_state = false;
		cpu_isolate_cdev->cdev = NULL;
		cpu_isolate_cdev->np = subsys_np;

		dev_phandle = of_parse_phandle(subsys_np, "qcom,cpu", 0);
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpu_isolate_cdev->cpu_id = cpu;
				break;
			}
		}
		INIT_WORK(&cpu_isolate_cdev->reg_work,
				cpu_isolate_register_cdev);
		list_add(&cpu_isolate_cdev->node, &cpu_isolate_cdev_list);
	}

	atomic_set(&in_suspend, 0);
	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "cpu-isolate/cdev:online",
				cpu_isolate_hp_online, cpu_isolate_hp_offline);
	if (ret < 0)
		return ret;
	register_pm_notifier(&cpu_isolate_pm_nb);
	cpumask_clear(&cpus_in_max_cooling_level);
	ret = 0;

	return ret;
}

static const struct of_device_id cpu_isolate_match[] = {
	{ .compatible = "qcom,cpu-isolate", },
	{},
};

static struct platform_driver cpu_isolate_driver = {
	.probe		= cpu_isolate_probe,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = cpu_isolate_match,
	},
};
builtin_platform_driver(cpu_isolate_driver);
