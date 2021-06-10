/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>


#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#endif

#include "apusys_power.h"
#include "apusys_power_cust.h"
#include "apusys_dbg.h"

#include "mnoc_drv.h"
#include "mnoc_hw.h"
#include "mnoc_qos.h"
#include "mnoc_qos_sys.h"
#include "mnoc_dbg.h"
#include "mnoc_pmu.h"
#include "mnoc_option.h"


DEFINE_SPINLOCK(mnoc_spinlock);

void __iomem *mnoc_base;
void __iomem *mnoc_int_base;
void __iomem *mnoc_apu_conn_base;
void __iomem *mnoc_slp_prot_base1;
void __iomem *mnoc_slp_prot_base2;

bool mnoc_reg_valid;
int mnoc_log_level;

struct mutex mnoc_pwr_mtx;
bool mnoc_pwr_is_on;

#if MNOC_INT_ENABLE
unsigned int mnoc_irq_number;
bool is_first_isr_after_pwr_on;
static struct work_struct mnoc_isr_work;

static void mnoc_isr_work_func(struct work_struct *work)
{
	LOG_DEBUG("+\n");
#if MNOC_AEE_WARN_ENABLE
	mutex_lock(&mnoc_pwr_mtx);
	if (mnoc_pwr_is_on)
		apusys_reg_dump();
	mutex_unlock(&mnoc_pwr_mtx);
	mnoc_aee_warn("MNOC", "MNOC Exception");
#endif
	print_int_sta(NULL);
	LOG_DEBUG("-\n");
}
#endif

static void mnoc_apusys_top_after_pwr_on(void *para)
{
	unsigned long flags;

	LOG_DEBUG("+\n");

	mnoc_pmu_reg_init();
	infra2apu_sram_en();
	mnoc_hw_reinit();
	apu_qos_on();

	spin_lock_irqsave(&mnoc_spinlock, flags);
#if MNOC_INT_ENABLE
	is_first_isr_after_pwr_on = true;
#endif
	mnoc_reg_valid = true;
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	mutex_lock(&mnoc_pwr_mtx);
	mnoc_pwr_is_on = true;
	mutex_unlock(&mnoc_pwr_mtx);

	mnoc_int_endis(true);

	LOG_DEBUG("-\n");
}

static void mnoc_apusys_top_before_pwr_off(void *para)
{
	unsigned long flags;

	LOG_DEBUG("+\n");

	mnoc_int_endis(false);

	spin_lock_irqsave(&mnoc_spinlock, flags);
	mnoc_reg_valid = false;
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	infra2apu_sram_dis();
	apu_qos_off();

	mutex_lock(&mnoc_pwr_mtx);
	mnoc_pwr_is_on = false;
	mutex_unlock(&mnoc_pwr_mtx);

	LOG_DEBUG("-\n");
}

#if MNOC_INT_ENABLE
/*
 * GIC SPI IRQ 406 is shared, need to return IRQ_NONE
 * if not triggered by mnoc
 */
static irqreturn_t mnoc_isr(int irq, void *dev_id)
{
	unsigned long flags;
	int mnoc_irq_triggered = 0;

	LOG_DEBUG("+\n");

	spin_lock_irqsave(&mnoc_spinlock, flags);

	/* prevent register access if apusys power off */
	if (!mnoc_reg_valid) {
		spin_unlock_irqrestore(&mnoc_spinlock, flags);
		LOG_DEBUG("ISR can't access mnoc reg when APUSYS off\n");
		return IRQ_NONE;
	}

	mnoc_irq_triggered = mnoc_check_int_status();

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	if (mnoc_irq_triggered != 0) {
		LOG_DEBUG("INT triggered by mnoc\n");
		/* Prevent overwhelming interrupts paralyzing system */
		if (mnoc_irq_triggered == 1 && is_first_isr_after_pwr_on) {
			is_first_isr_after_pwr_on = false;
			schedule_work(&mnoc_isr_work);
		}
		return IRQ_HANDLED;
	}


	LOG_DEBUG("INT NOT triggered by mnoc\n");

	LOG_DEBUG("-\n");

	return IRQ_NONE;
}
#endif

static int mnoc_probe(struct platform_device *pdev)
{
	int ret = 0;
#if MNOC_INT_ENABLE
	struct device *dev = &pdev->dev;
#endif
	struct device_node *node, *sub_node;
	struct platform_device *sub_pdev;
	struct apu_mnoc *p_mnoc = NULL;

	mnoc_reg_valid = false;
	mnoc_log_level = 0;

	LOG_DEBUG("+\n");

	p_mnoc = kmalloc(sizeof(*p_mnoc), GFP_KERNEL);
	if (!p_mnoc)
		return -ENOMEM;

	p_mnoc->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, p_mnoc);
#if MNOC_APU_PWR_CHK
	if (!apusys_power_check())
		return 0;
#endif

	/* make sure apusys_power driver initiallized before
	 * calling apu_power_callback_device_register
	 */
	sub_node = of_find_compatible_node(NULL, NULL, "mediatek,apusys_power");
	if (!sub_node) {
		LOG_ERR("DT,mediatek,apusys_power not found\n");
		return -EINVAL;
	}

	sub_pdev = of_find_device_by_node(sub_node);

	if (!sub_pdev || !sub_pdev->dev.driver) {
		LOG_DEBUG("Waiting for %s\n",
			 sub_node->full_name);
		return -EPROBE_DEFER;
	}

	mutex_init(&mnoc_pwr_mtx);
	mnoc_pwr_is_on = false;
	create_debugfs();
	mnoc_qos_create_sys(&pdev->dev);
	spin_lock_init(&mnoc_spinlock);
	apu_qos_counter_init(&pdev->dev);
	mnoc_pmu_init();
	mnoc_hw_init();

	node = pdev->dev.of_node;
	if (!node) {
		LOG_ERR("get apusys_mnoc device node err\n");
		return -ENODEV;
	}

	/* Setup IO addresses */
	mnoc_base = of_iomap(node, 0);
	mnoc_int_base = of_iomap(node, 1);
	mnoc_apu_conn_base = of_iomap(node, 2);
	mnoc_slp_prot_base1 = of_iomap(node, 3);
	mnoc_slp_prot_base2 = of_iomap(node, 4);

#if MNOC_INT_ENABLE
	INIT_WORK(&mnoc_isr_work, &mnoc_isr_work_func);
	mnoc_irq_number = irq_of_parse_and_map(node, 0);
	LOG_DEBUG("mnoc_irq_number = %d\n", mnoc_irq_number);

	/* set mnoc IRQ(GIC SPI pin shared with axi-reviser/devapc) */
	ret = request_irq(mnoc_irq_number, mnoc_isr,
			IRQF_TRIGGER_HIGH | IRQF_SHARED,
			APUSYS_MNOC_DEV_NAME, dev);
	if (ret)
		LOG_ERR("IRQ register failed (%d)\n", ret);
	else
		LOG_DEBUG("Set IRQ OK.\n");
#endif

	ret = apu_power_callback_device_register(MNOC,
		mnoc_apusys_top_after_pwr_on, mnoc_apusys_top_before_pwr_off);
	if (ret) {
		LOG_ERR("apu_power_callback_device_register return error(%d)\n",
			ret);
		return -EINVAL;
	}

	LOG_DEBUG("-\n");

	return ret;
}

static int mnoc_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* apu_qos_suspend(); */
	/* mnoc_pmu_suspend(); */
	return 0;
}

static int mnoc_resume(struct platform_device *pdev)
{
	/* apu_qos_resume(); */
	/* mnoc_pmu_resume(); */
	return 0;
}

static int mnoc_remove(struct platform_device *pdev)
{
#if MNOC_INT_ENABLE
	struct device *dev = &pdev->dev;
#endif
	struct device_node *node = NULL;
	struct apu_mnoc *p_mnoc = NULL;

	LOG_DEBUG("+\n");

#if MNOC_APU_PWR_CHK
	if (!apusys_power_check())
		return 0;
#endif

	apu_power_callback_device_unregister(MNOC);

	remove_debugfs();
	mnoc_qos_remove_sys(&pdev->dev);
	apu_qos_counter_destroy(&pdev->dev);
	mnoc_pmu_exit();
	mnoc_hw_exit();

	node = pdev->dev.of_node;
	if (!node) {
		LOG_ERR("get apusys_mnoc device node err\n");
		return -ENODEV;
	}

#if MNOC_INT_ENABLE
	free_irq(mnoc_irq_number, dev);
	cancel_work_sync(&mnoc_isr_work);
#endif
	iounmap(mnoc_base);
	iounmap(mnoc_int_base);
	iounmap(mnoc_apu_conn_base);
	iounmap(mnoc_slp_prot_base1);
	iounmap(mnoc_slp_prot_base2);

	p_mnoc = dev_get_drvdata(&pdev->dev);
	kfree(p_mnoc);

	LOG_DEBUG("-\n");

	return 0;
}

static const struct of_device_id apusys_mnoc_of_match[] = {
	{.compatible = "mediatek,apusys_mnoc",},
	{/* end of list */},
};

static struct platform_driver mnoc_driver = {
	.probe		= mnoc_probe,
	.remove		= mnoc_remove,
	.suspend	= mnoc_suspend,
	.resume		= mnoc_resume,
	.driver		= {
		.name   = APUSYS_MNOC_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = apusys_mnoc_of_match,
	},
};

static int __init mnoc_init(void)
{
	LOG_DEBUG("driver init start\n");

	if (platform_driver_register(&mnoc_driver)) {
		LOG_ERR("failed to register %s driver", APUSYS_MNOC_DEV_NAME);
		return -ENODEV;
	}

	return 0;
}

static void __exit mnoc_exit(void)
{
	LOG_DEBUG("de-initialization\n");
	platform_driver_unregister(&mnoc_driver);
}

module_init(mnoc_init);
module_exit(mnoc_exit);
MODULE_DESCRIPTION("MTK APUSYS MNoC Driver");
MODULE_AUTHOR("SPT1/SS5");
MODULE_LICENSE("GPL");
