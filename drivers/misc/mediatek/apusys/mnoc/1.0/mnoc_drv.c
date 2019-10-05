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

/* for Kernel Native SMC API */
#include <linux/arm-smccc.h>
#include <mtk_secure_api.h>


#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#endif

#include "mnoc_drv.h"
#include "mnoc_hw.h"
#include "mnoc_qos.h"
#include "mnoc_dbg.h"

enum APUSYS_MNOC_SMC_ID {
	MNOC_INFRA2APU_SRAM_EN,
	MNOC_INFRA2APU_SRAM_DIS,
	MNOC_APU2INFRA_BUS_PROTECT_EN,
	MNOC_APU2INFRA_BUS_PROTECT_DIS,

	NR_APUSYS_MNOC_SMC_ID
};


#if MNOC_INT_ENABLE
	unsigned int mnoc_irq_number = 0;
#endif

DEFINE_SPINLOCK(mnoc_spinlock);

void __iomem *mnoc_base;
void __iomem *mnoc_int_base;
void __iomem *mnoc_slp_prot_base1;
void __iomem *mnoc_slp_prot_base2;


/* After APUSYS top power on */
void infra2apu_sram_en(void)
{
	struct arm_smccc_res res;

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_INFRA2APU_SRAM_EN,
		0, 0, 0, 0, 0, 0, &res);
}

/* Before APUSYS top power off */
void infra2apu_sram_dis(void)
{
	struct arm_smccc_res res;

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_INFRA2APU_SRAM_DIS,
		0, 0, 0, 0, 0, 0, &res);
}

/* Before APUSYS reset */
void apu2infra_bus_protect_en(void)
{
	struct arm_smccc_res res;

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_APU2INFRA_BUS_PROTECT_EN,
		0, 0, 0, 0, 0, 0, &res);
}

/* After APUSYS reset */
void apu2infra_bus_protect_dis(void)
{
	struct arm_smccc_res res;

	/*
	 * arm_smccc_smc (unsigned long a0, unsigned long a1,
	 *	unsigned long a2, unsigned long a3, unsigned long a4,
	 *	unsigned long a5, unsigned long a6, unsigned long a7,
	 *	struct arm_smccc_res *res)
	 */
	arm_smccc_smc(MTK_SIP_APUSYS_MNOC_CONTROL,
		MNOC_APU2INFRA_BUS_PROTECT_DIS,
		0, 0, 0, 0, 0, 0, &res);
}

void mnoc_apusys_top_after_pwr_on(void)
{
	LOG_DEBUG("+\n");

	infra2apu_sram_en();
	mnoc_reg_init();
	mnoc_qos_reg_init();
	notify_sspm_apusys_on();

	LOG_DEBUG("-\n");
}

void mnoc_apusys_top_before_pwr_off(void)
{
	LOG_DEBUG("+\n");

	infra2apu_sram_dis();
	notify_sspm_apusys_off();

	LOG_DEBUG("-\n");
}

#if MNOC_INT_ENABLE
static irqreturn_t mnoc_isr(int irq, void *dev_id)
{
	bool mnoc_irq_triggered = false;

	LOG_DEBUG("+\n");

	mnoc_irq_triggered = mnoc_check_int_status();

	LOG_DEBUG("-\n");

	if (mnoc_irq_triggered) {
		LOG_ERR("INT triggered by mnoc\n");
		return IRQ_HANDLED;
	}

	LOG_ERR("INT NOT triggered by mnoc\n");

	return IRQ_NONE;
}
#endif


static int mnoc_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *node = NULL;

	LOG_DEBUG("+\n");

	apu_qos_counter_init();

	node = pdev->dev.of_node;
	if (!node) {
		LOG_ERR("get apusys_mnoc device node err\n");
		return -ENODEV;
	}

	/* Setup IO addresses */
	mnoc_base = of_iomap(node, 0);
	mnoc_int_base = of_iomap(node, 1);
	mnoc_slp_prot_base1 = of_iomap(node, 2);
	mnoc_slp_prot_base2 = of_iomap(node, 3);

#if MNOC_INT_ENABLE
	mnoc_irq_number = irq_of_parse_and_map(node, 0);
	LOG_DEBUG("mnoc_irq_number = %d\n", mnoc_irq_number);

	/* set mnoc IRQ(GIC SPI pin shared with axi-reviser) */
	ret = request_irq(mnoc_irq_number, mnoc_isr,
			IRQF_TRIGGER_LOW | IRQF_SHARED,
			APUSYS_MNOC_DEV_NAME, dev);
	if (ret)
		LOG_ERR("IRQ register failed (%d)\n", ret);

	LOG_DEBUG("Set IRQ OK.\n");
#endif

	LOG_DEBUG("-\n");

	return ret;
}

static int mnoc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mnoc_resume(struct platform_device *pdev)
{
	return 0;
}

static int mnoc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = NULL;

	LOG_DEBUG("+\n");

	apu_qos_counter_destroy();

	node = pdev->dev.of_node;
	if (!node) {
		LOG_ERR("get apusys_mnoc device node err\n");
		return -ENODEV;
	}

	remove_procfs();
#if MNOC_INT_ENABLE
	free_irq(mnoc_irq_number, dev);
#endif
	iounmap(mnoc_base);
	iounmap(mnoc_int_base);
	iounmap(mnoc_slp_prot_base1);
	iounmap(mnoc_slp_prot_base2);

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

	create_procfs();

	spin_lock_init(&mnoc_spinlock);

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
