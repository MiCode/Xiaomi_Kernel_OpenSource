/* drivers/soc/qcom/smd_init_dt.c
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>

#include "smd_private.h"

#define MODULE_NAME "msm_smd"
#define IPC_LOG(level, x...) do { \
	if (smd_log_ctx) \
		ipc_log_string(smd_log_ctx, x); \
	else \
		printk(level x); \
	} while (0)

#if defined(CONFIG_MSM_SMD_DEBUG)
#define SMD_DBG(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMD_DEBUG) \
			IPC_LOG(KERN_DEBUG, x);		\
	} while (0)

#define SMSM_DBG(x...) do {					\
		if (msm_smd_debug_mask & MSM_SMSM_DEBUG)	\
			IPC_LOG(KERN_DEBUG, x);		\
	} while (0)
#else
#define SMD_DBG(x...) do { } while (0)
#define SMSM_DBG(x...) do { } while (0)
#endif

static DEFINE_MUTEX(smd_probe_lock);
static int first_probe_done;

static int msm_smsm_probe(struct platform_device *pdev)
{
	uint32_t edge;
	char *key;
	int ret;
	uint32_t irq_offset;
	uint32_t irq_bitmask;
	uint32_t irq_line;
	struct interrupt_config_item *private_irq;
	struct device_node *node;
	void *irq_out_base;
	resource_size_t irq_out_size;
	struct platform_device *parent_pdev;
	struct resource *r;
	struct interrupt_config *private_intr_config;
	uint32_t remote_pid;

	node = pdev->dev.of_node;

	if (!pdev->dev.parent) {
		pr_err("%s: missing link to parent device\n", __func__);
		return -ENODEV;
	}

	parent_pdev = to_platform_device(pdev->dev.parent);

	key = "irq-reg-base";
	r = platform_get_resource_byname(parent_pdev, IORESOURCE_MEM, key);
	if (!r)
		goto missing_key;
	irq_out_size = resource_size(r);
	irq_out_base = ioremap_nocache(r->start, irq_out_size);
	if (!irq_out_base) {
		pr_err("%s: ioremap_nocache() of irq_out_base addr:%pr size:%pr\n",
				__func__, &r->start, &irq_out_size);
		return -ENOMEM;
	}
	SMSM_DBG("%s: %s = %p", __func__, key, irq_out_base);

	key = "qcom,smsm-edge";
	ret = of_property_read_u32(node, key, &edge);
	if (ret)
		goto missing_key;
	SMSM_DBG("%s: %s = %d", __func__, key, edge);

	key = "qcom,smsm-irq-offset";
	ret = of_property_read_u32(node, key, &irq_offset);
	if (ret)
		goto missing_key;
	SMSM_DBG("%s: %s = %x", __func__, key, irq_offset);

	key = "qcom,smsm-irq-bitmask";
	ret = of_property_read_u32(node, key, &irq_bitmask);
	if (ret)
		goto missing_key;
	SMSM_DBG("%s: %s = %x", __func__, key, irq_bitmask);

	key = "interrupts";
	irq_line = irq_of_parse_and_map(node, 0);
	if (!irq_line)
		goto missing_key;
	SMSM_DBG("%s: %s = %d", __func__, key, irq_line);

	private_intr_config = smd_get_intr_config(edge);
	if (!private_intr_config) {
		pr_err("%s: invalid edge\n", __func__);
		return -ENODEV;
	}
	private_irq = &private_intr_config->smsm;
	private_irq->out_bit_pos = irq_bitmask;
	private_irq->out_offset = irq_offset;
	private_irq->out_base = irq_out_base;
	private_irq->irq_id = irq_line;
	remote_pid = smd_edge_to_remote_pid(edge);
	interrupt_stats[remote_pid].smsm_interrupt_id = irq_line;

	ret = request_irq(irq_line,
				private_irq->irq_handler,
				IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
				node->name,
				NULL);
	if (ret < 0) {
		pr_err("%s: request_irq() failed on %d\n", __func__, irq_line);
		return ret;
	} else {
		ret = enable_irq_wake(irq_line);
		if (ret < 0)
			pr_err("%s: enable_irq_wake() failed on %d\n", __func__,
					irq_line);
	}

	ret = smsm_post_init();
	if (ret) {
		pr_err("smd_post_init() failed ret=%d\n", ret);
		return ret;
	}

	return 0;

missing_key:
	pr_err("%s: missing key: %s", __func__, key);
	return -ENODEV;
}

static int msm_smd_probe(struct platform_device *pdev)
{
	uint32_t edge;
	char *key;
	int ret;
	uint32_t irq_offset;
	uint32_t irq_bitmask;
	uint32_t irq_line;
	const char *subsys_name;
	struct interrupt_config_item *private_irq;
	struct device_node *node;
	void *irq_out_base;
	resource_size_t irq_out_size;
	struct platform_device *parent_pdev;
	struct resource *r;
	struct interrupt_config *private_intr_config;
	uint32_t remote_pid;
	bool skip_pil;

	node = pdev->dev.of_node;

	if (!pdev->dev.parent) {
		pr_err("%s: missing link to parent device\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&smd_probe_lock);
	if (!first_probe_done) {
		smd_reset_all_edge_subsys_name();
		first_probe_done = 1;
	}
	mutex_unlock(&smd_probe_lock);

	parent_pdev = to_platform_device(pdev->dev.parent);

	key = "irq-reg-base";
	r = platform_get_resource_byname(parent_pdev, IORESOURCE_MEM, key);
	if (!r)
		goto missing_key;
	irq_out_size = resource_size(r);
	irq_out_base = ioremap_nocache(r->start, irq_out_size);
	if (!irq_out_base) {
		pr_err("%s: ioremap_nocache() of irq_out_base addr:%pr size:%pr\n",
				__func__, &r->start, &irq_out_size);
		return -ENOMEM;
	}
	SMD_DBG("%s: %s = %p", __func__, key, irq_out_base);

	key = "qcom,smd-edge";
	ret = of_property_read_u32(node, key, &edge);
	if (ret)
		goto missing_key;
	SMD_DBG("%s: %s = %d", __func__, key, edge);

	key = "qcom,smd-irq-offset";
	ret = of_property_read_u32(node, key, &irq_offset);
	if (ret)
		goto missing_key;
	SMD_DBG("%s: %s = %x", __func__, key, irq_offset);

	key = "qcom,smd-irq-bitmask";
	ret = of_property_read_u32(node, key, &irq_bitmask);
	if (ret)
		goto missing_key;
	SMD_DBG("%s: %s = %x", __func__, key, irq_bitmask);

	key = "interrupts";
	irq_line = irq_of_parse_and_map(node, 0);
	if (!irq_line)
		goto missing_key;
	SMD_DBG("%s: %s = %d", __func__, key, irq_line);

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	SMD_DBG("%s: %s = %s", __func__, key, subsys_name);
	/*
	 * Backwards compatibility.  Although label is required, some DTs may
	 * still list the legacy pil-string.  Sanely handle pil-string.
	 */
	if (!subsys_name) {
		pr_warn("msm_smd: Missing required property - label. Using legacy parsing\n");
		key = "qcom,pil-string";
		subsys_name = of_get_property(node, key, NULL);
		SMD_DBG("%s: %s = %s", __func__, key, subsys_name);
		if (subsys_name)
			skip_pil = false;
		else
			skip_pil = true;
	} else {
		key = "qcom,not-loadable";
		skip_pil = of_property_read_bool(node, key);
		SMD_DBG("%s: %s = %d\n", __func__, key, skip_pil);
	}

	private_intr_config = smd_get_intr_config(edge);
	if (!private_intr_config) {
		pr_err("%s: invalid edge\n", __func__);
		return -ENODEV;
	}
	private_irq = &private_intr_config->smd;
	private_irq->out_bit_pos = irq_bitmask;
	private_irq->out_offset = irq_offset;
	private_irq->out_base = irq_out_base;
	private_irq->irq_id = irq_line;
	remote_pid = smd_edge_to_remote_pid(edge);
	interrupt_stats[remote_pid].smd_interrupt_id = irq_line;

	ret = request_irq(irq_line,
				private_irq->irq_handler,
				IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
				node->name,
				NULL);
	if (ret < 0) {
		pr_err("%s: request_irq() failed on %d\n", __func__, irq_line);
		return ret;
	} else {
		ret = enable_irq_wake(irq_line);
		if (ret < 0)
			pr_err("%s: enable_irq_wake() failed on %d\n", __func__,
					irq_line);
	}

	smd_set_edge_subsys_name(edge, subsys_name);
	smd_proc_set_skip_pil(smd_edge_to_remote_pid(edge), skip_pil);

	smd_set_edge_initialized(edge);
	smd_post_init(remote_pid);
	return 0;

missing_key:
	pr_err("%s: missing key: %s", __func__, key);
	return -ENODEV;
}

static struct of_device_id msm_smd_match_table[] = {
	{ .compatible = "qcom,smd" },
	{},
};

static struct platform_driver msm_smd_driver = {
	.probe = msm_smd_probe,
	.driver = {
		.name = MODULE_NAME ,
		.owner = THIS_MODULE,
		.of_match_table = msm_smd_match_table,
	},
};

static struct of_device_id msm_smsm_match_table[] = {
	{ .compatible = "qcom,smsm" },
	{},
};

static struct platform_driver msm_smsm_driver = {
	.probe = msm_smsm_probe,
	.driver = {
		.name = "msm_smsm",
		.owner = THIS_MODULE,
		.of_match_table = msm_smsm_match_table,
	},
};

int msm_smd_driver_register(void)
{
	int rc;

	rc = platform_driver_register(&msm_smd_driver);
	if (rc) {
		pr_err("%s: smd_driver register failed %d\n",
			__func__, rc);
		return rc;
	}

	rc = platform_driver_register(&msm_smsm_driver);
	if (rc) {
		pr_err("%s: msm_smsm_driver register failed %d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(msm_smd_driver_register);

MODULE_DESCRIPTION("MSM SMD Device Tree Init");
MODULE_LICENSE("GPL v2");
