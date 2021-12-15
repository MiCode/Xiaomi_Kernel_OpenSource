// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */
/*
 * This is interrupt driver
 *
 * GZ does not support virtual interrupt, interrupt forwarding driver is
 * need for passing GZ and hypervisor-TEE interrupts
 */

#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/sm_err.h>
#include <gz-trusty/trusty.h>
#include <linux/irqdomain.h>
#include <linux/of_irq.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

#define ENABLE_IRQ_DEBUG_LOG (0)
#if ENABLE_IRQ_DEBUG_LOG
#define IRQ_DEBUG_LOG_EN
#endif

#define enable_code 0 /*replace #if 0*/

struct trusty_irq {
	struct trusty_irq_state *is;
	struct hlist_node node;
	unsigned int irq;
	bool percpu;
	bool enable;
	struct trusty_irq __percpu *percpu_ptr;
};

struct trusty_irq_irqset {
	struct hlist_head pending;
	struct hlist_head inactive;
};

struct trusty_irq_state {
	struct device *dev;
	struct device *trusty_dev;
	struct trusty_irq_irqset normal_irqs;
	spinlock_t normal_irqs_lock;
	struct trusty_irq_irqset __percpu *percpu_irqs;
	struct notifier_block trusty_call_notifier;
	struct hlist_node cpuhp_node;
	enum tee_id_t tee_id;
};

static int trusty_irq_cpuhp_slot = -1;

static struct device_node *spi_node;
static struct device_node *ppi_node;
static struct trusty_irq __percpu *trusty_ipi_data[16];
static int trusty_ipi_init[16];

static void trusty_irq_enable_pending_irqs(struct trusty_irq_state *is,
					   struct trusty_irq_irqset *irqset,
					   bool percpu)
{
	struct hlist_node *n;
	struct trusty_irq *trusty_irq;

	hlist_for_each_entry_safe(trusty_irq, n, &irqset->pending, node) {
#ifdef IRQ_DEBUG_LOG_EN
		dev_dbg(is->dev,
			"%s: enable pending irq %d, percpu %d, cpu %d\n",
			__func__, trusty_irq->irq, percpu, smp_processor_id());
#endif
		if (percpu)
			enable_percpu_irq(trusty_irq->irq, 0);
		else
			enable_irq(trusty_irq->irq);

		hlist_del(&trusty_irq->node);
		hlist_add_head(&trusty_irq->node, &irqset->inactive);
	}
}

static void trusty_irq_enable_irqset(struct trusty_irq_state *is,
				     struct trusty_irq_irqset *irqset)
{
	struct trusty_irq *trusty_irq;

	hlist_for_each_entry(trusty_irq, &irqset->inactive, node) {
		if (trusty_irq->enable) {
			dev_info(is->dev,
				 "%s: percpu irq %d already enabled, cpu %d\n",
				 __func__, trusty_irq->irq, smp_processor_id());
			continue;
		}
#ifdef IRQ_DEBUG_LOG_EN
		dev_dbg(is->dev, "%s: enable percpu irq %d, cpu %d\n",
			__func__, trusty_irq->irq, smp_processor_id());
#endif
		enable_percpu_irq(trusty_irq->irq, 0);
		trusty_irq->enable = true;
	}
}

static void trusty_irq_disable_irqset(struct trusty_irq_state *is,
				      struct trusty_irq_irqset *irqset)
{
	struct hlist_node *n;
	struct trusty_irq *trusty_irq;

	hlist_for_each_entry(trusty_irq, &irqset->inactive, node) {
		if (!trusty_irq->enable) {
			dev_info(is->dev,
				 "irq %d already disabled, percpu %d, cpu %d\n",
				 trusty_irq->irq, trusty_irq->percpu,
				 smp_processor_id());
			continue;
		}
#ifdef IRQ_DEBUG_LOG_EN
		dev_dbg(is->dev, "%s: disable irq %d, percpu %d, cpu %d\n",
			__func__, trusty_irq->irq, trusty_irq->percpu,
			smp_processor_id());
#endif
		trusty_irq->enable = false;
		if (trusty_irq->percpu)
			disable_percpu_irq(trusty_irq->irq);
		else
			disable_irq_nosync(trusty_irq->irq);
	}
	hlist_for_each_entry_safe(trusty_irq, n, &irqset->pending, node) {
		if (!trusty_irq->enable) {
			dev_info(is->dev,
				 "pending irq %d already disabled, percpu %d, cpu %d\n",
				 trusty_irq->irq, trusty_irq->percpu,
				 smp_processor_id());
		}
#ifdef IRQ_DEBUG_LOG_EN
		dev_dbg(is->dev,
			"%s: disable pending irq %d, percpu %d, cpu %d\n",
			__func__, trusty_irq->irq, trusty_irq->percpu,
			smp_processor_id());
#endif
		trusty_irq->enable = false;
		hlist_del(&trusty_irq->node);
		hlist_add_head(&trusty_irq->node, &irqset->inactive);
	}
}

static int trusty_irq_call_notify(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct trusty_irq_state *is;

	WARN_ON(!irqs_disabled());

	if (action != TRUSTY_CALL_PREPARE)
		return NOTIFY_DONE;

	is = container_of(nb, struct trusty_irq_state, trusty_call_notifier);

	spin_lock(&is->normal_irqs_lock);
	trusty_irq_enable_pending_irqs(is, &is->normal_irqs, false);
	spin_unlock(&is->normal_irqs_lock);
	trusty_irq_enable_pending_irqs(is, this_cpu_ptr(is->percpu_irqs), true);

	return NOTIFY_OK;
}

irqreturn_t trusty_irq_handler(int irq, void *data)
{
	struct trusty_irq *trusty_irq = data;
	struct trusty_irq_state *is = trusty_irq->is;
	struct trusty_irq_irqset *irqset;

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: irq %d, percpu %d, cpu %d, enable %d\n",
		__func__, irq, trusty_irq->irq, smp_processor_id(),
		trusty_irq->enable);
#endif

	if (trusty_irq->percpu) {
		disable_percpu_irq(irq);
		irqset = this_cpu_ptr(is->percpu_irqs);
	} else {
		disable_irq_nosync(irq);
		irqset = &is->normal_irqs;
	}

	spin_lock(&is->normal_irqs_lock);
	if (trusty_irq->enable) {
		hlist_del(&trusty_irq->node);
		hlist_add_head(&trusty_irq->node, &irqset->pending);
	}
	spin_unlock(&is->normal_irqs_lock);

	trusty_enqueue_nop(is->trusty_dev, NULL, smp_processor_id());

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: irq %d done\n", __func__, irq);
#endif

	return IRQ_HANDLED;
}

void handle_trusty_ipi(int ipinr)
{
	if (ipinr < 0 || ipinr >= 16)
		return;

	if (trusty_ipi_init[ipinr] == 0)
		return;

	//irq_enter() and irq_exit() are not supported when compiled as .ko
	/*for kernel-4.19*/
	//irq_enter();
	//trusty_irq_handler(ipinr, this_cpu_ptr(trusty_ipi_data[ipinr]));
	//irq_exit();

	pr_info("%s not supported.\n", __func__);
}

static int trusty_irq_cpu_up(unsigned int cpu, struct hlist_node *node)
{
	unsigned long irq_flags;
	struct trusty_irq_state *is;

	is = container_of(node, struct trusty_irq_state, cpuhp_node);
#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: cpu %d\n", __func__, cpu);
#endif

	local_irq_save(irq_flags);
	trusty_irq_enable_irqset(is, this_cpu_ptr(is->percpu_irqs));
	local_irq_restore(irq_flags);

	return 0;
}

static int trusty_irq_cpu_down(unsigned int cpu, struct hlist_node *node)
{
	unsigned long irq_flags;
	struct trusty_irq_state *is;

	is = container_of(node, struct trusty_irq_state, cpuhp_node);
#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: cpu %d\n", __func__, cpu);
#endif

	local_irq_save(irq_flags);
	trusty_irq_disable_irqset(is, this_cpu_ptr(is->percpu_irqs));
	local_irq_restore(irq_flags);

	return 0;
}

#if enable_code /*#if 0*/
static int trusty_irq_create_irq_mapping(struct trusty_irq_state *is, int irq)
{
	int ret;
	int index;
	u32 irq_pos;
	u32 templ_idx;
	u32 range_base;
	u32 range_end;
	struct of_phandle_args oirq;

	/* check if "interrupt-ranges" property is present */
	if (!of_find_property(is->dev->of_node, "interrupt-ranges", NULL)) {
		/* fallback to old behavior to be backward compatible with
		 * systems that do not need IRQ domains.
		 */
		return irq;
	}

	/* find irq range */
	for (index = 0;; index += 3) {
		ret = of_property_read_u32_index(is->dev->of_node,
						 "interrupt-ranges",
						 index, &range_base);
		if (ret)
			return ret;

		ret = of_property_read_u32_index(is->dev->of_node,
						 "interrupt-ranges",
						 index + 1, &range_end);
		if (ret)
			return ret;

		if (irq >= range_base && irq <= range_end)
			break;
	}

	/*  read the rest of range entry: template index and irq_pos */
	ret = of_property_read_u32_index(is->dev->of_node,
					 "interrupt-ranges",
					 index + 2, &templ_idx);
	if (ret)
		return ret;

	/* read irq template */
	ret = of_parse_phandle_with_args(is->dev->of_node,
					 "interrupt-templates",
					 "#interrupt-cells", templ_idx, &oirq);
	if (ret)
		return ret;

	WARN_ON(!oirq.np);
	WARN_ON(!oirq.args_count);

	/*
	 * An IRQ template is a non empty array of u32 values describing group
	 * of interrupts having common properties. The u32 entry with index
	 * zero contains the position of irq_id in interrupt specifier array
	 * followed by data representing interrupt specifier array with irq id
	 * field omitted, so to convert irq template to interrupt specifier
	 * array we have to move down one slot the first irq_pos entries and
	 * replace the resulting gap with real irq id.
	 */
	irq_pos = oirq.args[0];

	if (irq_pos >= oirq.args_count) {
		dev_info(is->dev, "irq pos is out of range: %d\n", irq_pos);
		return -EINVAL;
	}

	for (index = 1; index <= irq_pos; index++)
		oirq.args[index - 1] = oirq.args[index];

	oirq.args[irq_pos] = irq - range_base;

	ret = irq_create_of_mapping(&oirq);

	return (!ret) ? -EINVAL : ret;
}
#endif


static int trusty_irq_init_normal_irq(struct trusty_irq_state *is, int tirq)
{
	int ret;
	int irq = tirq;
	unsigned long irq_flags;
	struct trusty_irq *trusty_irq;

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: irq %d\n", __func__, tirq);
#endif

#if enable_code /*#if 0*/
	irq = trusty_irq_create_irq_mapping(is, tirq);
	if (irq < 0) {
		dev_info(is->dev,
			 "trusty_irq_create_irq_mapping failed (%d)\n", irq);
		return irq;
	}
#endif

	trusty_irq = kzalloc(sizeof(*trusty_irq), GFP_KERNEL);
	if (!trusty_irq)
		return -ENOMEM;

	if (spi_node) {
		struct of_phandle_args oirq;

		if (irq < 32) {
			ret = -EINVAL;
			dev_info(is->dev, "SPI only, no %d\n", irq);
			goto err_request_irq;
		}

		oirq.np = spi_node;
		oirq.args_count = 3;
		oirq.args[0] = GIC_SPI;
		oirq.args[1] = irq - 32;
		oirq.args[2] = 0;

		irq = irq_create_of_mapping(&oirq);
		if (irq == 0) {
			ret = -EINVAL;
			goto err_request_irq;
		}
	}

	trusty_irq->is = is;
	trusty_irq->irq = irq;
	trusty_irq->enable = true;

	spin_lock_irqsave(&is->normal_irqs_lock, irq_flags);
	hlist_add_head(&trusty_irq->node, &is->normal_irqs.inactive);
	spin_unlock_irqrestore(&is->normal_irqs_lock, irq_flags);

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: tirq=%d, virq=%d\n", __func__, tirq, irq);
#endif

	ret = request_irq(irq, trusty_irq_handler, IRQF_NO_THREAD | IRQF_SHARED | IRQF_PROBE_SHARED,
			  "trusty", trusty_irq);
	if (ret) {
		dev_info(is->dev, "request_irq failed %d\n", ret);
		goto err_request_irq;
	}
	return 0;

err_request_irq:
	spin_lock_irqsave(&is->normal_irqs_lock, irq_flags);
	hlist_del(&trusty_irq->node);
	spin_unlock_irqrestore(&is->normal_irqs_lock, irq_flags);
	kfree(trusty_irq);
	return ret;
}


static int trusty_irq_init_per_cpu_irq(struct trusty_irq_state *is, int tirq)
{
	int ret;
	int irq = tirq;
	unsigned int cpu;
	struct trusty_irq __percpu *trusty_irq_handler_data;

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: irq %d\n", __func__, tirq);
#endif
#if enable_code /*#if 0*/
	irq = trusty_irq_create_irq_mapping(is, tirq);
	if (irq <= 0) {
		dev_info(is->dev,
			 "trusty_irq_create_irq_mapping failed (%d)\n", irq);
		return irq;
	}
#endif

	trusty_irq_handler_data = alloc_percpu(struct trusty_irq);
	if (!trusty_irq_handler_data)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		struct trusty_irq *trusty_irq;
		struct trusty_irq_irqset *irqset;

		trusty_irq = per_cpu_ptr(trusty_irq_handler_data, cpu);
		irqset = per_cpu_ptr(is->percpu_irqs, cpu);

		trusty_irq->is = is;
		hlist_add_head(&trusty_irq->node, &irqset->inactive);
		trusty_irq->irq = irq;
		trusty_irq->percpu = true;
		trusty_irq->percpu_ptr = trusty_irq_handler_data;
	}

	if (irq >= 0 && irq < 16) {		/* IPI (SGI) */
		trusty_ipi_data[irq] = trusty_irq_handler_data;
		trusty_ipi_init[irq] = 1;
		return 0;
	}

	if (ppi_node) {
		struct of_phandle_args oirq;

		if (irq >= 32) {
			ret = -EINVAL;
			dev_info(is->dev, "Not support SPI %d\n", irq);
			goto err_request_percpu_irq;
		}

		oirq.np = ppi_node;
		oirq.args_count = 3;
		oirq.args[0] = GIC_PPI;
		oirq.args[1] = irq - 16;
		oirq.args[2] = 0;

		irq = irq_create_of_mapping(&oirq);
		if (irq == 0) {
			ret = -EINVAL;
			goto err_request_percpu_irq;
		}

		for_each_possible_cpu(cpu)
			per_cpu_ptr(trusty_irq_handler_data, cpu)->irq = irq;
	}

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(is->dev, "%s: tirq=%d, virq=%d\n", __func__, tirq, irq);
#endif

	ret = request_percpu_irq(irq, trusty_irq_handler, "trusty",
				 trusty_irq_handler_data);
	if (ret) {
		dev_info(is->dev, "request_percpu_irq failed %d\n", ret);
		goto err_request_percpu_irq;
	}

	return 0;

err_request_percpu_irq:
	for_each_possible_cpu(cpu) {
		struct trusty_irq *trusty_irq;

		trusty_irq = per_cpu_ptr(trusty_irq_handler_data, cpu);
		hlist_del(&trusty_irq->node);
	}

	free_percpu(trusty_irq_handler_data);
	return ret;
}


static int trusty_smc_get_next_irq(struct trusty_irq_state *is,
				   unsigned long min_irq, bool per_cpu)
{
	return trusty_fast_call32(is->trusty_dev,
			MTEE_SMCNR(SMCF_FC_GET_NEXT_IRQ, is->trusty_dev),
			min_irq, per_cpu, is->tee_id);
}

static int trusty_irq_init_one(struct trusty_irq_state *is,
			       int irq, bool per_cpu)
{
	int ret;

	irq = trusty_smc_get_next_irq(is, irq, per_cpu);

	if (irq < 0)
		return irq;

	if (per_cpu)
		ret = trusty_irq_init_per_cpu_irq(is, irq);
	else
		ret = trusty_irq_init_normal_irq(is, irq);

	if (ret)
		dev_info(is->dev, "init irq %d failed, ignored irq\n", irq);

	return irq + 1;
}

static void trusty_irq_free_irqs(struct trusty_irq_state *is)
{
	struct trusty_irq *irq;
	struct hlist_node *n;
	unsigned int cpu;

	hlist_for_each_entry_safe(irq, n, &is->normal_irqs.inactive, node) {
#ifdef IRQ_DEBUG_LOG_EN
		dev_dbg(is->dev, "%s: irq %d\n", __func__, irq->irq);
#endif
		free_irq(irq->irq, irq);
		hlist_del(&irq->node);
		kfree(irq);
	}
	hlist_for_each_entry_safe(irq, n,
				  &this_cpu_ptr(is->percpu_irqs)->inactive,
				  node) {
		struct trusty_irq __percpu *trusty_irq_handler_data;

#ifdef IRQ_DEBUG_LOG_EN
		dev_dbg(is->dev, "%s: percpu irq %d\n", __func__, irq->irq);
#endif
		trusty_irq_handler_data = irq->percpu_ptr;
		free_percpu_irq(irq->irq, trusty_irq_handler_data);
		for_each_possible_cpu(cpu) {
			struct trusty_irq *irq_tmp;

			irq_tmp = per_cpu_ptr(trusty_irq_handler_data, cpu);
			hlist_del(&irq_tmp->node);
		}
		free_percpu(trusty_irq_handler_data);
	}
}

static void init_irq_node(struct device_node *node)
{
	struct device_node *spi;
	struct device_node *ppi;

	if (!node)
		return;

	spi = of_irq_find_parent(node);
	if (!spi)
		return;

	ppi = of_parse_phandle(node, "ppi-interrupt-parent", 0);
	if (!ppi)
		ppi = of_irq_find_parent(spi);

	if (!ppi)
		return;

	spi_node = spi;
	ppi_node = ppi;
}

static int trusty_irq_probe(struct platform_device *pdev)
{
	int ret, irq, tee_id = -1;
	unsigned long irq_flags;
	struct trusty_irq_state *is;
	struct device_node *node = pdev->dev.of_node,
			   *pnode = pdev->dev.parent->of_node;

	if (!node || !pnode) {
		dev_info(&pdev->dev, "of_node required\n");
		return -EINVAL;
	}

	init_irq_node(node);

	ret = of_property_read_u32(pnode, "tee-id", &tee_id);
	if (ret != 0 || !is_tee_id(tee_id)) {
		dev_info(&pdev->dev, "tee_id is not set on device tree\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "--- init trusty-irq for MTEE %d ---\n", tee_id);

	is = kzalloc(sizeof(struct trusty_irq_state), GFP_KERNEL);
	if (!is) {
		ret = -ENOMEM;
		goto err_alloc_is;
	}

	is->tee_id = tee_id;
	is->dev = &pdev->dev;
	is->trusty_dev = is->dev->parent;
	spin_lock_init(&is->normal_irqs_lock);
	is->percpu_irqs = alloc_percpu(struct trusty_irq_irqset);
	if (!is->percpu_irqs) {
		ret = -ENOMEM;
		goto err_alloc_pending_percpu_irqs;
	}

	is->trusty_call_notifier.notifier_call = trusty_irq_call_notify;
	ret = trusty_call_notifier_register(is->trusty_dev,
					    &is->trusty_call_notifier);
	if (ret) {
		dev_info(&pdev->dev,
			 "failed to register trusty call notifier\n");
		goto err_trusty_call_notifier_register;
	}

	for (irq = 0; irq >= 0;)
		irq = trusty_irq_init_one(is, irq, true);
	for (irq = 0; irq >= 0;)
		irq = trusty_irq_init_one(is, irq, false);

	ret = cpuhp_state_add_instance(trusty_irq_cpuhp_slot,
				       &is->cpuhp_node);
	if (ret < 0) {
		dev_info(&pdev->dev,
			 "cpuhp_state_add_instance failed %d\n", ret);
		goto err_add_cpuhp_instance;
	}

	platform_set_drvdata(pdev, is);

	return 0;

err_add_cpuhp_instance:
	spin_lock_irqsave(&is->normal_irqs_lock, irq_flags);
	trusty_irq_disable_irqset(is, &is->normal_irqs);
	spin_unlock_irqrestore(&is->normal_irqs_lock, irq_flags);
	trusty_irq_free_irqs(is);
	trusty_call_notifier_unregister(is->trusty_dev,
					&is->trusty_call_notifier);
err_trusty_call_notifier_register:
	free_percpu(is->percpu_irqs);
err_alloc_pending_percpu_irqs:
	kfree(is);
err_alloc_is:
	return ret;
}

static int trusty_irq_remove(struct platform_device *pdev)
{
	int ret;
	unsigned long irq_flags;
	struct trusty_irq_state *is = platform_get_drvdata(pdev);

#ifdef IRQ_DEBUG_LOG_EN
	dev_dbg(&pdev->dev, "%s\n", __func__);
#endif

	ret = cpuhp_state_remove_instance(trusty_irq_cpuhp_slot,
					  &is->cpuhp_node);
	if (WARN_ON(ret))
		return ret;

	spin_lock_irqsave(&is->normal_irqs_lock, irq_flags);
	trusty_irq_disable_irqset(is, &is->normal_irqs);
	spin_unlock_irqrestore(&is->normal_irqs_lock, irq_flags);

	trusty_irq_free_irqs(is);

	trusty_call_notifier_unregister(is->trusty_dev,
					&is->trusty_call_notifier);
	free_percpu(is->percpu_irqs);

	kfree(is);

	return 0;
}

/* for trusty */
static const struct of_device_id trusty_irq_of_match[] = {
	{ .compatible = "android,trusty-irq-v1", },
	{},
};

static struct platform_driver trusty_irq_driver = {
	.probe = trusty_irq_probe,
	.remove = trusty_irq_remove,
	.driver = {
		   .name = "trusty-irq",
		   .owner = THIS_MODULE,
		   .of_match_table = trusty_irq_of_match,
		   },
};

/* for nebula */
static const struct of_device_id nebula_irq_of_match[] = {
	{ .compatible = "android,nebula-irq-v1", },
	{},
};

static struct platform_driver nebula_irq_driver = {
	.probe = trusty_irq_probe,
	.remove = trusty_irq_remove,
	.driver = {
		   .name = "nebula-irq",
		   .owner = THIS_MODULE,
		   .of_match_table = nebula_irq_of_match,
		   },
};

static int __init trusty_irq_driver_init(void)
{
	int ret;

	/* allocate dynamic cpuhp state slot */
	ret = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
				      "trusty-irq:cpu:online",
				      trusty_irq_cpu_up, trusty_irq_cpu_down);
	if (ret < 0)
		return ret;
	trusty_irq_cpuhp_slot = ret;

	ret = platform_driver_register(&trusty_irq_driver);
	if (ret < 0)
		goto err_trusty_register;

	ret = platform_driver_register(&nebula_irq_driver);
	if (ret < 0)
		goto err_nebula_register;

	return ret;

err_nebula_register:
	platform_driver_unregister(&trusty_irq_driver);
err_trusty_register:
	/* undo cpuhp slot allocation */
	cpuhp_remove_multi_state(trusty_irq_cpuhp_slot);
	trusty_irq_cpuhp_slot = -1;

	return ret;
}

static void __exit trusty_irq_driver_exit(void)
{
	platform_driver_unregister(&nebula_irq_driver);
	platform_driver_unregister(&trusty_irq_driver);

	cpuhp_remove_multi_state(trusty_irq_cpuhp_slot);
	trusty_irq_cpuhp_slot = -1;
}

module_init(trusty_irq_driver_init);
module_exit(trusty_irq_driver_exit);
MODULE_LICENSE("GPL");

