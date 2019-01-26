/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <soc/qcom/early_domain.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <asm/barrier.h>

static struct early_domain_core *ed_core_data;
static bool early_domain_enabled;

bool get_early_service_status(enum service_id sid)
{
	struct early_domain_core *domain;
	unsigned long *status;

	if (!early_domain_enabled)
		return false;
	domain = ed_core_data;
	status = &domain->pdata->early_domain_status;
	cpu_relax();
	return test_bit(sid, status);
}
EXPORT_SYMBOL(get_early_service_status);

static void active_early_services(void)
{
	enum service_id core;
	int i;
	bool active;

	core = EARLY_DOMAIN_CORE;
	i = (int)core;
	while (i < NUM_SERVICES) {
		active = get_early_service_status((enum service_id)i);
		if (active)
			pr_info("Early service_id:%d active\n", i);
		i++;
	}
}

void request_early_service_shutdown(enum service_id sid)
{
	struct early_domain_core *domain = ed_core_data;
	unsigned long *request;

	if (!early_domain_enabled)
		return;

	request = &domain->pdata->early_domain_request;
	set_bit(sid, request);
}
EXPORT_SYMBOL(request_early_service_shutdown);

void *get_service_shared_mem_start(enum service_id sid)
{
	void *service_shared_mem_start;
	void *early_domain_shm_start;

	if (!early_domain_enabled || sid < EARLY_DISPLAY || sid > NUM_SERVICES)
		return NULL;

	early_domain_shm_start = ed_core_data->pdata;
	service_shared_mem_start = early_domain_shm_start +
					sizeof(struct early_domain_header) +
					(SERVICE_SHARED_MEM_SIZE * (sid - 1));
	return service_shared_mem_start;
}
EXPORT_SYMBOL(get_service_shared_mem_start);

static void free_reserved_lk_mem(phys_addr_t paddr, size_t size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	memblock_free(paddr, size);
	pfn_start = paddr >> PAGE_SHIFT;
	pfn_end = (paddr + size) >> PAGE_SHIFT;
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));
}

static void early_domain_work(struct work_struct *work)
{
	struct early_domain_core *core_data;
	struct device *cpu_device;
	unsigned long *status;
	int cpu;
	int delay;
	int max_delay;

	core_data = ed_core_data;
	status = &core_data->pdata->early_domain_status;
	delay = 40;
	max_delay = 1000;

	/* Poll on status which will be updated by early-domain running in LK */
	while (*status) {
		cpu_relax();
		msleep(delay);
		delay = (delay > max_delay ? max_delay : delay + 20);
	}

	/* An instruction barrier to ensure that execution pipline is flushed
	 * and instrutions ahead are not executed out of order, leading to
	 * an unwanted situation where we free resources with a non-zero status.
	 */

	isb();

	/* Once the status is zero, hot add reserved cpu cores and
	 * free reserved memory
	 */
	unregister_cpu_notifier(&core_data->ed_notifier);

	/* Take attempts to hot add the cpu(s) reserved for early domain
	 * with delays
	 */
	delay = 20;
	max_delay = 200;
	while (!cpumask_empty(&core_data->cpumask)) {
		for_each_cpu(cpu, &core_data->cpumask) {
			cpu_device = get_cpu_device(cpu);
			if (!cpu_device) {
				pr_err("cpu:%d absent in cpu_possible_mask\n"
					, cpu);
				cpumask_clear_cpu(cpu, &core_data->cpumask);
				continue;
			}
			if (!device_online(cpu_device))
				cpumask_clear_cpu(cpu, &core_data->cpumask);
		}
		msleep(delay);
		delay = (delay > max_delay ? max_delay : delay + 20);
	}
	early_domain_enabled = false;
	free_reserved_lk_mem(core_data->lk_pool_paddr, core_data->lk_pool_size);
	free_reserved_lk_mem(core_data->early_domain_shm,
				core_data->early_domain_shm_size);

	/* Remove qos and wake locks */

	pm_qos_remove_request(&core_data->ed_qos_request);
	__pm_relax(&core_data->ed_wake_lock);
}

static int early_domain_cpu_notifier(struct notifier_block *self,
			unsigned long action, void *hcpu)
{
	struct early_domain_core *core_data = container_of(self,
					struct early_domain_core, ed_notifier);
	unsigned int notifier;
	unsigned int cpu;

	notifier = NOTIFY_BAD;
	cpu = (long)hcpu;
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
		if (cpumask_test_cpu(cpu, &core_data->cpumask)) {
			pr_err("Early domain services are running on cpu%d\n"
				, cpu);
			break;
		}
	default:
		notifier = NOTIFY_OK;
		break;
	}
	return notifier;
}

static int init_early_domain_data(struct early_domain_core *core_data)
{
	int cpu;
	unsigned long cpumask;
	int ret;

	cpumask_clear(&core_data->cpumask);
	cpumask = (unsigned long)core_data->pdata->cpumask;
	for_each_set_bit(cpu, &cpumask, sizeof(cpumask))
		cpumask_set_cpu(cpu, &core_data->cpumask);

	memset(&core_data->ed_qos_request, 0,
		sizeof(core_data->ed_qos_request));
	core_data->ed_qos_request.type = PM_QOS_REQ_AFFINE_CORES;
	core_data->ed_qos_request.cpus_affine = core_data->cpumask;
	pm_qos_add_request(&core_data->ed_qos_request,
		PM_QOS_CPU_DMA_LATENCY, PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE);
	wakeup_source_init(&core_data->ed_wake_lock, "early_domain");
	__pm_stay_awake(&core_data->ed_wake_lock);
	core_data->ed_notifier = (struct notifier_block) {
		.notifier_call = early_domain_cpu_notifier,
		};
	ret = register_cpu_notifier(&core_data->ed_notifier);
	if (ret) {
		dev_err(&core_data->pdev->dev, "Could not register cpu notifier\n");
		return ret;
	}
	INIT_WORK(&core_data->early_domain_work, early_domain_work);
	if (!schedule_work(&core_data->early_domain_work)) {
		dev_err(&core_data->pdev->dev, "Could not schedule work for handoff\n");
		unregister_cpu_notifier(&core_data->ed_notifier);
		ret = -ENOMEM;
	}
	return ret;
}

/* It is expected that probe is being called before the initialization of
 * multimedia drivers like display/camera/audio, so they get correct status
 * of any active early service.
 */

static int early_domain_probe(struct platform_device *pdev)
{
	int ret;
	struct early_domain_core *core_data;
	struct resource res_shm, res_lk;
	struct device_node *parent, *node;

	ret = 0;
	parent = of_find_node_by_path("/reserved-memory");
	if (!parent) {
		dev_err(&pdev->dev, "Could not find reserved-memory node\n");
		return -EINVAL;
	}
	/* early_domain_shm reserved memory node will be added by bootloader
	 * dynamically if earlydomain was enabled.
	 */

	node = of_find_node_by_name(parent, "early_domain_shm");
	if (!node) {
		dev_err(&pdev->dev, "Could not find early_domain_shm\n");
		of_node_put(parent);
		return -EINVAL;
	}
	ret = of_address_to_resource(node, 0, &res_shm);
	if (ret) {
		dev_err(&pdev->dev, "No memory address assigned to the region\n");
		of_node_put(node);
		of_node_put(parent);
		return ret;
	}
	of_node_put(node);
	/* lk_pool reserved memory node will be added by bootloader
	 * dynamically if earlydomain was enabled.
	 */

	node = of_find_node_by_name(parent, "lk_pool");
	if (!node) {
		dev_err(&pdev->dev, "Could not find lk_pool\n");
		of_node_put(parent);
		return -EINVAL;
	}
	of_node_put(parent);
	ret = of_address_to_resource(node, 0, &res_lk);
	if (ret) {
		dev_err(&pdev->dev, "No memory address assigned to lk_pool\n");
		of_node_put(node);
		return ret;
	}
	of_node_put(node);
	core_data = kzalloc(sizeof(*core_data), GFP_KERNEL);
	if (!core_data)
		return -ENOMEM;
	ed_core_data = core_data;
	core_data->pdata = (struct early_domain_header *)
			ioremap_nocache(res_shm.start, resource_size(&res_shm));
	if (!core_data->pdata) {
		dev_err(&pdev->dev, "%s cannot map reserved early domain space\n"
			, __func__);
		ret = -ENOMEM;
		goto err;
	}
	core_data->early_domain_shm = (dma_addr_t)res_shm.start;
	core_data->early_domain_shm_size = res_shm.end - res_shm.start;
	core_data->lk_pool_paddr = (dma_addr_t)res_lk.start;
	core_data->lk_pool_size = res_lk.end - res_lk.start;
	ret = memcmp(core_data->pdata->magic, EARLY_DOMAIN_MAGIC,
			MAGIC_SIZE);
	if (ret) {
		dev_err(&pdev->dev, "Early domain reserved page has been corrupted\n");
		ret = -EINVAL;
		goto err;
	}
	core_data->pdev = pdev;
	ret = init_early_domain_data(core_data);
	if (ret)
		goto err;
	platform_set_drvdata(pdev, core_data);
	early_domain_enabled = true;
	active_early_services();
	return ret;
err:
	if (pm_qos_request_active(&core_data->ed_qos_request))
		pm_qos_remove_request(&core_data->ed_qos_request);
	if (core_data->ed_wake_lock.active)
		wakeup_source_trash(&core_data->ed_wake_lock);
	kfree(core_data);
	return ret;
}

static int early_domain_remove(struct platform_device *pdev)
{
	struct early_domain_core *core_data;

	core_data = platform_get_drvdata(pdev);
	pm_qos_remove_request(&core_data->ed_qos_request);
	__pm_relax(&core_data->ed_wake_lock);
	unregister_cpu_notifier(&core_data->ed_notifier);
	early_domain_enabled = false;
	kfree(core_data);
	return 0;
}

static const struct of_device_id early_domain_table[] = {
	{ .compatible = "qcom,early_domain" },
	{}
};

static struct platform_driver early_domain_driver = {
	.probe = early_domain_probe,
	.remove = early_domain_remove,
	.driver = {
		.name = "early_domain",
		.owner = THIS_MODULE,
		.of_match_table = early_domain_table,
	},
};

static int __init earlydom_init(void)
{
	return platform_driver_register(&early_domain_driver);
}

fs_initcall(earlydom_init);

static void __exit earlydom_exit(void)
{
	platform_driver_unregister(&early_domain_driver);
}

module_exit(earlydom_exit);
MODULE_DESCRIPTION("QCOM EARLYDOMAIN DRIVER");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, early_domain_table);
