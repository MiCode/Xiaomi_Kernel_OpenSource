// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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


#if IS_ENABLED(CONFIG_OF)
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#endif

#include "apusys_core.h"
#include "apusys_power.h"
//#include "apusys_power_cust.h"
#include "apusys_debug_api.h"

#include "mnoc_drv.h"
#include "mnoc_qos.h"
#include "mnoc_qos_sys.h"
#include "mnoc_dbg.h"
#include "mnoc_pmu.h"
#include "mnoc_util.h"
#include "mnoc_api.h"

#ifdef MNOC_TAG_TP
#include "mnoc_tag.h"
#endif

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

struct mnoc_plat_drv mnoc_drv;

struct apusys_core_info *mnoc_core_info;


phys_addr_t get_apu_iommu_tfrp(unsigned int id)
{
	return mnoc_drv.get_apu_iommu_tfrp(id);
}
EXPORT_SYMBOL(get_apu_iommu_tfrp);


void mnoc_set_mni_pre_ultra(int dev_type, int dev_core, bool endis)
{
	mnoc_drv.set_mni_pre_ultra(dev_type, dev_core, endis);
}

void mnoc_set_lt_guardian_pre_ultra(int dev_type, int dev_core, bool endis)
{
	mnoc_drv.set_lt_guardian_pre_ultra(dev_type, dev_core, endis);
}

static void mnoc_isr_work_func(struct work_struct *work)
{
	LOG_DEBUG("+\n");
#if MNOC_AEE_WARN_ENABLE
	mutex_lock(&mnoc_pwr_mtx);
	if (mnoc_pwr_is_on)
		apusys_reg_dump("apusys-mnoc", false);
	mutex_unlock(&mnoc_pwr_mtx);
	mnoc_aee_warn("MNOC", "MNOC Exception");
#endif

	mnoc_drv.print_int_sta(NULL);

	LOG_DEBUG("-\n");
}
#endif

static void mnoc_apusys_top_after_pwr_on(void *para)
{
	unsigned long flags;

	LOG_DEBUG("+\n");

	mnoc_pmu_reg_init();
	infra2apu_sram_en();
	mnoc_drv.hw_reinit();
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

	mnoc_drv.int_endis(true);

	LOG_DEBUG("-\n");
}

static void mnoc_apusys_top_before_pwr_off(void *para)
{
	unsigned long flags;

	LOG_DEBUG("+\n");

	mnoc_drv.int_endis(false);

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

	mnoc_irq_triggered = mnoc_drv.chk_int_status();

	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	if (mnoc_irq_triggered != 0) {
		LOG_DEBUG("INT triggered by mnoc\n");

		if (mnoc_irq_triggered == 2)
			LOG_DEBUG("mnoc timeout\n");

		if (mnoc_irq_triggered == 3 && is_first_isr_after_pwr_on) {

			LOG_ERR("check int status case 3 !!\n");
			is_first_isr_after_pwr_on = false;

			mutex_lock(&mnoc_pwr_mtx);
			if (mnoc_pwr_is_on)
				apusys_reg_dump("apusys-mnoc", false);
			mutex_unlock(&mnoc_pwr_mtx);
			mnoc_aee_warn("MNOC", "MNOC Exception");
		}

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
	struct device_node *node;
	struct apu_mnoc *p_mnoc = NULL;

	mnoc_reg_valid = false;
	mnoc_log_level = 0;

	LOG_DEBUG("+\n");

	p_mnoc = kmalloc(sizeof(struct apu_mnoc), GFP_KERNEL);
	if (!p_mnoc)
		return -ENOMEM;

	p_mnoc->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, p_mnoc);

#if MNOC_APU_PWR_CHK
	if (!apusys_power_check())
		return -1;
#endif
	mnoc_drv = *(struct mnoc_plat_drv *)of_device_get_match_data(&pdev->dev);


	mutex_init(&mnoc_pwr_mtx);
	mnoc_pwr_is_on = false;

	create_debugfs(mnoc_core_info->dbg_root);
	mnoc_qos_create_sys(&pdev->dev);
	spin_lock_init(&mnoc_spinlock);
	apu_qos_counter_init(&pdev->dev);
	mnoc_pmu_init();
	mnoc_drv.init();

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
			//IRQF_TRIGGER_HIGH | IRQF_SHARED,
			irq_get_trigger_type(mnoc_irq_number) | IRQF_SHARED,
			APUSYS_MNOC_DEV_NAME, &pdev->dev);
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

#ifdef MNOC_TAG_TP
	mnoc_init_drv_tags();
#endif

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

	node = pdev->dev.of_node;
	if (!node) {
		LOG_ERR("get apusys_mnoc device node err\n");
		return -ENODEV;
	}

#if MNOC_INT_ENABLE
	free_irq(mnoc_irq_number, &pdev->dev);
	cancel_work_sync(&mnoc_isr_work);
#endif
	iounmap(mnoc_base);
	iounmap(mnoc_int_base);
	iounmap(mnoc_apu_conn_base);
	iounmap(mnoc_slp_prot_base1);
	iounmap(mnoc_slp_prot_base2);

	p_mnoc = dev_get_drvdata(&pdev->dev);
	kfree(p_mnoc);

#ifdef MNOC_TAG_TP
	mnoc_exit_drv_tags();
#endif

	LOG_DEBUG("-\n");

	return 0;
}

static struct platform_driver mnoc_driver = {
	.probe		= mnoc_probe,
	.remove		= mnoc_remove,
	.suspend	= mnoc_suspend,
	.resume		= mnoc_resume,
	.driver		= {
		.name   = APUSYS_MNOC_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

int mnoc_init(struct apusys_core_info *info)
{
	LOG_DEBUG("mnoc driver init start\n");

	mnoc_core_info = info;

	memset(&mnoc_drv, 0, sizeof(struct mnoc_plat_drv));

	mnoc_driver.driver.of_match_table = mnoc_util_get_device_id();

	mnoc_rv_setup(info);

	if (platform_driver_register(&mnoc_driver)) {
		LOG_ERR("failed to register %s driver", APUSYS_MNOC_DEV_NAME);
		return -ENODEV;
	}

	return 0;
}

void mnoc_exit(void)
{
	LOG_DEBUG("de-initialization\n");
	platform_driver_unregister(&mnoc_driver);
}
