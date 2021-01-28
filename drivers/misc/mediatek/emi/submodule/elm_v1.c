// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/printk.h>

#include <mt_emi.h>
#include "elm_v1.h"

static bool elm_enabled;
static DEFINE_SPINLOCK(elm_lock);

bool is_elm_enabled(void)
{
	return elm_enabled;
}
EXPORT_SYMBOL(is_elm_enabled);

void enable_elm(void)
{
	unsigned long flags;
	int emi_dcm_status;

	spin_lock_irqsave(&elm_lock, flags);
	emi_dcm_status = disable_emi_dcm();

	turn_on_elm();
	elm_enabled = true;

	restore_emi_dcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);
}
EXPORT_SYMBOL(enable_elm);

void disable_elm(void)
{
	unsigned long flags;
	int emi_dcm_status;

	spin_lock_irqsave(&elm_lock, flags);
	emi_dcm_status = disable_emi_dcm();

	turn_off_elm();
	elm_enabled = false;

	restore_emi_dcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);
}
EXPORT_SYMBOL(disable_elm);

static irqreturn_t elm_isr(int irq, void *dev_id)
{
	unsigned long flags;
	int emi_dcm_status;

	pr_info("[ELM] latency violation\n");

	spin_lock_irqsave(&elm_lock, flags);
	save_debug_reg();

	emi_dcm_status = disable_emi_dcm();
	reset_elm();
	restore_emi_dcm(emi_dcm_status);
	spin_unlock_irqrestore(&elm_lock, flags);

	return IRQ_HANDLED;
}

/* the interface of CCCI for MD EE */
void dump_last_bm(char *buf, unsigned int leng)
{
	unsigned long flags;

	spin_lock_irqsave(&elm_lock, flags);
	dump_elm(buf, leng);
	spin_unlock_irqrestore(&elm_lock, flags);
}

static ssize_t elm_ctrl_show(struct device_driver *driver, char *buf)
{
	return sprintf(buf, "ELM enabled: %d\n", is_elm_enabled());
}

static ssize_t elm_ctrl_store(struct device_driver *driver,
	const char *buf, size_t count)
{
	if (!strncmp(buf, "ON", strlen("ON")))
		enable_elm();
	else if (!strncmp(buf, "OFF", strlen("OFF")))
		disable_elm();

	return count;
}

DRIVER_ATTR(elm_ctrl, 0644, elm_ctrl_show, elm_ctrl_store);

void elm_init(struct platform_driver *emi_ctrl, struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	unsigned int elm_irq;
	int ret;

	pr_info("[ELM] initialize EMI ELM\n");

	mt_elm_init(mt_cen_emi_base_get());

	if (node) {
		elm_irq = irq_of_parse_and_map(node, ELM_IRQ_INDEX);
		pr_info("[ELM] get ELM IRQ: %d\n", elm_irq);

		ret = request_irq(elm_irq, (irq_handler_t)elm_isr,
			IRQF_TRIGGER_NONE, "elm", emi_ctrl);
		if (ret != 0) {
			pr_info("[ELM] fail to request IRQ (%d)\n", ret);
			return;
		}

		enable_elm();
	}

	ret = driver_create_file(&emi_ctrl->driver, &driver_attr_elm_ctrl);
	if (ret)
		pr_info("[ELM] fail to create elm_ctrl\n");
}

void suspend_elm(void)
{
	int emi_dcm_status;

	if (is_elm_enabled()) {
		emi_dcm_status = disable_emi_dcm();
		turn_off_elm();
		restore_emi_dcm(emi_dcm_status);
	}
}

void resume_elm(void)
{
	if (is_elm_enabled())
		enable_elm();
}

