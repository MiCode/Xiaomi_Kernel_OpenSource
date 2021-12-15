/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */



#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>

#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
#include <mach/mtk_pmic_wrap.h>
#endif
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
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned int hwirq;

	if (desc)
		hwirq = irqd_to_hwirq(&desc->irq_data);
	else
		return IRQ_HANDLED;
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
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
	default:
		IRQLOG("Unsupported key irq %d\n", hwirq);
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
	struct irq_desc *desc;

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

#if ENABLE_ALL_OC_IRQ
static unsigned int vio18_oc_times;

/* General OC Int Handler */
static void oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	char oc_str[30] = "";
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int times;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	times = sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].times;

	IRQLOG("[%s] int name=%s\n", __func__, int_name);
	switch (intNo) {
	case INT_VCN33_OC:
		/* keep OC interrupt and keep tracking */
		pr_notice(PMICTAG "[PMIC_INT] PMIC OC: %s\n", int_name);
		break;
	case INT_VIO18_OC:
		pr_notice("LDO_DEGTD_SEL=0x%x\n",
			pmic_get_register_value(PMIC_LDO_DEGTD_SEL));
		pr_notice("RG_INT_EN_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_EN_VIO18_OC));
		pr_notice("RG_INT_MASK_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_MASK_VIO18_OC));
		pr_notice("RG_INT_STATUS_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_STATUS_VIO18_OC));
		pr_notice("RG_INT_RAW_STATUS_VIO18_OC=0x%x\n",
			pmic_get_register_value(
				PMIC_RG_INT_RAW_STATUS_VIO18_OC));
		pr_notice("DA_VIO18_OCFB_EN=0x%x\n",
			pmic_get_register_value(PMIC_DA_VIO18_OCFB_EN));
		pr_notice("RG_LDO_VIO18_OCFB_EN=0x%x\n",
			pmic_get_register_value(PMIC_RG_LDO_VIO18_OCFB_EN));
		vio18_oc_times++;
		if (vio18_oc_times >= 2) {
			snprintf(oc_str, 30, "PMIC OC:%s", int_name);
			aee_kernel_warning(
				oc_str,
				"\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				int_name);
			pmic_enable_interrupt(intNo, 0, "PMIC");
			pr_notice("disable OC interrupt: %s\n", int_name);
		}
		break;
	default:
		/* issue AEE exception and disable OC interrupt */
		kernel_dump_exception_reg();
		snprintf(oc_str, 30, "PMIC OC:%s", int_name);
		aee_kernel_warning(oc_str,
			"\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
			int_name);
		pmic_enable_interrupt(intNo, 0, "PMIC");
		pr_notice(PMICTAG "[PMIC_INT] disable OC interrupt: %s\n"
			, int_name);
		break;
	}
}

static void md_oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	int ret = 0;
	int data_int32 = 0;
	char oc_str[30] = "";

	switch (intNo) {
	case INT_VPA_OC:
		data_int32 = 1 << 0;
		break;
	case INT_VFE28_OC:
		data_int32 = 1 << 1;
		break;
	case INT_VRF12_OC:
		data_int32 = 1 << 2;
		break;
	case INT_VRF18_OC:
		data_int32 = 1 << 3;
		break;
	default:
		break;
	}
	snprintf(oc_str, 30, "PMIC OC:%s", int_name);
#ifdef CONFIG_MTK_CCCI_DEVICES
	aee_kernel_warning(oc_str, "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s"
			, int_name);
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR,
					(char *)&data_int32, 4);
#endif
	if (ret)
		pr_notice("[%s] - exec_ccci_kern_func_by_md_id - msg fail\n"
			  , __func__);
	pr_info("[%s]Send msg pass\n", __func__);
}

/* register general oc interrupt handler */
void pmic_register_oc_interrupt_callback(enum PMIC_IRQ_ENUM intNo)
{
	unsigned int spNo, sp_conNo, sp_irqNo;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	IRQLOG("[%s] intNo=%d\n", __func__, intNo);
	switch (intNo) {
	case INT_VPA_OC:
	case INT_VFE28_OC:
	case INT_VRF12_OC:
	case INT_VRF18_OC:
		sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].oc_callback =
							md_oc_int_handler;
		break;
	default:
		sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].oc_callback =
							oc_int_handler;
		break;
	}
}

/* register and enable all oc interrupt */
void register_all_oc_interrupts(void)
{
	enum PMIC_IRQ_ENUM oc_int;

	/* BUCK OC */
	for (oc_int = INT_VPROC11_OC; oc_int <= INT_VPA_OC; oc_int++) {
		pmic_register_oc_interrupt_callback(oc_int);
		pmic_enable_interrupt(oc_int, 1, "PMIC");
	}
	/* LDO OC */
	for (oc_int = INT_VFE28_OC; oc_int <= INT_VBIF28_OC; oc_int++) {
		switch (oc_int) {
		case INT_VSIM1_OC:
		case INT_VSIM2_OC:
		case INT_VMCH_OC:
		case INT_VCAMA1_OC:
		case INT_VCAMA2_OC:
		case INT_VCAMD_OC:
		case INT_VCAMIO_OC:
			IRQLOG("[PMIC_INT] non-enabled OC: %d\n", oc_int);
			break;
#if 0
		case INT_VCAMA_OC:
			IRQLOG("[PMIC_INT] OC:%d enabled after power on\n"
				, oc_int);
			pmic_register_oc_interrupt_callback(oc_int);
			break;
#endif
		default:
			pmic_register_oc_interrupt_callback(oc_int);
			pmic_enable_interrupt(oc_int, 1, "PMIC");
			break;
		}
	}
}
#endif

void PMIC_EINT_SETTING(struct platform_device *pdev)
{
	int ret = 0;

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

