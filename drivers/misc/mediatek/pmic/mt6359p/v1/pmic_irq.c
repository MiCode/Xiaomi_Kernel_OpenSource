/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>

#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic_wrap.h>
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"

#include <mt-plat/mtk_ccci_common.h>
#include <linux/mfd/mt6358/core.h>

struct legacy_pmic_callback {
	bool has_requested;
	void (*callback)(void);
};
static struct device *pmic_dev;
static struct legacy_pmic_callback pmic_cbs[300];

/* KEY Int Handler */
irqreturn_t key_int_handler(int irq, void *data)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned int hwirq;

	if (desc)
		hwirq = irqd_to_hwirq(&desc->irq_data);
	else
		return IRQ_HANDLED;

	switch (hwirq) {
	case INT_PWRKEY:
		IRQLOG("Press pwrkey %d\n",
			pmic_get_register_value(PMIC_PWRKEY_DEB));
		kpd_pwrkey_pmic_handler(0x1);
		break;
	case INT_PWRKEY_R:
		IRQLOG("Release pwrkey %d\n",
			pmic_get_register_value(PMIC_PWRKEY_DEB));
		kpd_pwrkey_pmic_handler(0x0);
		break;
	case INT_HOMEKEY:
		IRQLOG("Press homekey %d\n",
			pmic_get_register_value(PMIC_HOMEKEY_DEB));
		kpd_pmic_rstkey_handler(0x1);
		break;
	case INT_HOMEKEY_R:
		IRQLOG("Release homekey %d\n",
			pmic_get_register_value(PMIC_HOMEKEY_DEB));
		kpd_pmic_rstkey_handler(0x0);
		break;
	}
#endif
	return IRQ_HANDLED;
}

irqreturn_t legacy_pmic_int_handler(int irq, void *data)
{
	struct legacy_pmic_callback *pmic_cb = data;

	pmic_cb->callback();
	return IRQ_HANDLED;
}

/*
 * PMIC Interrupt service
 */
void pmic_enable_interrupt(enum PMIC_IRQ_ENUM intNo, unsigned int en, char *str)
{
	int ret = 0;
	unsigned int irq = 0;
	const char *name = NULL;
	struct legacy_pmic_callback *pmic_cb = &pmic_cbs[intNo];
	struct irq_desc *desc = NULL;

	if (intNo == INT_ENUM_MAX) {
		pr_notice(PMICTAG "[%s] disable intNo=%d\n", __func__, intNo);
		return;
	} else if (pmic_cb->callback == NULL) {
		pr_notice(PMICTAG "[%s] No callback at intNo=%d\n",
			__func__, intNo);
		return;
	}
	irq = mt6358_irq_get_virq(pmic_dev->parent, intNo);
	if (!irq) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}

	name = mt6358_irq_get_name(pmic_dev->parent, intNo);
	IRQLOG("mt6358_irq_get_name: %s............\n", name);

	if (name == NULL) {
		pr_notice(PMICTAG "[%s] no irq name at intNo=%d\n",
			__func__, intNo);
		return;
	}
	if (en == 1) {
		if (!(pmic_cb->has_requested)) {
			ret = devm_request_threaded_irq(pmic_dev, irq, NULL,
				legacy_pmic_int_handler, IRQF_TRIGGER_HIGH,
				name, pmic_cb);
			if (ret < 0)
				pr_notice(PMICTAG "[%s] request %s irq fail\n",
					  __func__, name);
			else
				pmic_cb->has_requested = true;
		} else
			enable_irq(irq);
	} else if (en == 0 && pmic_cb->has_requested)
		disable_irq_nosync(irq);
	desc = irq_to_desc(irq);
	IRQLOG("[%s] intNo=%d, en=%d, depth=%d\n",
		__func__, intNo, en, desc ? desc->depth : -1);
}

void pmic_register_interrupt_callback(enum PMIC_IRQ_ENUM intNo,
		void (EINT_FUNC_PTR) (void))
{
	struct legacy_pmic_callback *pmic_cb = &pmic_cbs[intNo];

	if (intNo == INT_ENUM_MAX) {
		pr_info(PMICTAG "[%s] disable intNo=%d\n", __func__, intNo);
		return;
	}
	pr_info("[%s] intNo=%d, callback=%pf\n",
		__func__, intNo, EINT_FUNC_PTR);
	pmic_cb->callback = EINT_FUNC_PTR;
}

void PMIC_EINT_SETTING(struct platform_device *pdev)
{
	int ret = 0;

	/* Keep VIO18_PG/OC original setting */

	/* MT6359 set VIO18 OC de-bounce to 120us */
	pmic_set_register_value(PMIC_RG_LDO_VIO18_OC_TSEL, 0x1);

	pmic_dev = &pdev->dev;
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "pwrkey"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"pwrkey", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request PWRKEY irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "pwrkey_r"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"pwrkey_r", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request PWRKEY_R irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "homekey"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"homekey", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request HOMEKEY irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "homekey_r"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"homekey_r", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request HOMEKEY_R irq fail\n");
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");

