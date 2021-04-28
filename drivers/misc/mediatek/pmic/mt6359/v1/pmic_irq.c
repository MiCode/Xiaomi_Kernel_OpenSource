/*
 * Copyright (C) 2018 MediaTek Inc.
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
	int ret;
	unsigned int irq;
	const char *name;
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
/* General OC Int Handler */
static void oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int times;
#if defined(CONFIG_MTK_AEE_FEATURE)
	char oc_str[30] = "";
#endif

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	times = sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].times;

	IRQLOG("[%s] int name=%s\n", __func__, int_name);
	switch (intNo) {
	case INT_VBIF28_OC:
	case INT_VIO28_OC:
		pmic_enable_interrupt(intNo, 0, "PMIC");
		g_oc_work.oc_intNo = intNo;
		g_oc_work.name = int_name;
		schedule_work(&g_oc_work.work);
		break;
	case INT_VCN33_1_OC:
	case INT_VCN33_2_OC:
	case INT_VA12_OC:
	case INT_VUSB_OC:
		/* keep OC interrupt and keep tracking */
		pr_notice(PMICTAG "[PMIC_INT] PMIC OC: %s\n", int_name);
		if (times >= 10) {
			pmic_enable_interrupt(intNo, 0, "PMIC");
			pr_notice("disable OC interrupt: %s\n", int_name);
		}
		break;
	case INT_VIO18_OC:
		pr_notice("VIO18_PG_DEB=%d,RGS_VIO18_PG_STATUS=%d\n",
			pmic_get_register_value(PMIC_VIO18_PG_DEB),
			pmic_get_register_value(PMIC_RGS_VIO18_PG_STATUS));
		pr_notice("RG_INT_EN_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_EN_VIO18_OC));
		pr_notice("RG_INT_MASK_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_MASK_VIO18_OC));
		pr_notice("RG_INT_STATUS_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_STATUS_VIO18_OC));
		pr_notice("RG_INT_RAW_STATUS_VIO18_OC=0x%x\n",
			pmic_get_register_value(
				PMIC_RG_INT_RAW_STATUS_VIO18_OC));
		pr_notice("LDO_VIO18_CON0=0x%x,LDO_VIO18_MON=0x%x\n",
			upmu_get_reg_value(MT6359_LDO_VIO18_CON0),
			upmu_get_reg_value(MT6359_LDO_VIO18_MON));
		pr_notice("LDO_VIO18_OP_EN=0x%x,LDO_VIO18_OP_CFG=0x%x\n",
			upmu_get_reg_value(MT6359_LDO_VIO18_OP_EN),
			upmu_get_reg_value(MT6359_LDO_VIO18_OP_CFG));
		pr_notice("VIO18_ANA_CON0=0x%x,VIO18_ANA_CON1=0x%x\n",
			upmu_get_reg_value(MT6359_VIO18_ANA_CON0),
			upmu_get_reg_value(MT6359_VIO18_ANA_CON1));
		pr_notice("XO_FPM_ISEL_M=0x%x\n",
			pmic_get_register_value(PMIC_XO_FPM_ISEL_M));
		if (times >= 2) {
#if defined(CONFIG_MTK_AEE_FEATURE)
			snprintf(oc_str, 30, "PMIC OC:%s", int_name);
			aee_kernel_warning(
				oc_str,
				"\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				int_name);
#endif
			pmic_enable_interrupt(intNo, 0, "PMIC");
			pr_notice("disable OC interrupt: %s\n", int_name);
		}
		break;
	default:
		/* issue AEE exception and disable OC interrupt */
		if (times >= 3) {
			kernel_dump_exception_reg();
#if defined(CONFIG_MTK_AEE_FEATURE)
			snprintf(oc_str, 30, "PMIC OC:%s", int_name);
			aee_kernel_warning(
				oc_str,
				"\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				int_name);
#endif
			pmic_enable_interrupt(intNo, 0, "PMIC");
			pr_notice("disable OC interrupt: %s\n", int_name);
		}
		break;
	}
}

static void md_oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	int ret = 0;
	int data_int32 = 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
#if defined(CONFIG_MTK_AEE_FEATURE)
	char oc_str[30] = "";
#endif
#endif
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int times;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	times = sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].times;
	switch (intNo) {
	case INT_VPA_OC:
		data_int32 = 1 << 0;
		break;
	case INT_VFE28_OC:
		data_int32 = 1 << 1;
		pr_notice("Reg[0x1B8A]=0x%x,Reg[0x1B88]=0x%x,Reg[0x1B8C]=0x%x,Reg[0x1B92]=0x%x\n"
			, upmu_get_reg_value(0x1B8A),
			upmu_get_reg_value(0x1B88),
			upmu_get_reg_value(0x1B8C),
			upmu_get_reg_value(0x1B92));
		if (times >= 10) {
			pmic_enable_interrupt(intNo, 0, "PMIC");
			pr_notice("disable OC interrupt: %s\n", int_name);
		}
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
#ifdef CONFIG_MTK_CCCI_DEVICES
#if defined(CONFIG_MTK_AEE_FEATURE)
	snprintf(oc_str, 30, "PMIC OC:%s", int_name);
	aee_kernel_warning(oc_str, "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s"
			, int_name);
#endif
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
	for (oc_int = INT_VPU_OC; oc_int <= INT_VPA_OC; oc_int++) {
		pmic_register_oc_interrupt_callback(oc_int);
		pmic_enable_interrupt(oc_int, 1, "PMIC");
	}
	/* LDO OC */
	for (oc_int = INT_VFE28_OC; oc_int <= INT_VUFS_OC; oc_int++) {
		switch (oc_int) {
		case INT_VSIM1_OC:
		case INT_VSIM2_OC:
		case INT_VCAMIO_OC:
			IRQLOG("[PMIC_INT] non-enabled OC: %d\n", oc_int);
			break;
		default:
			pmic_register_oc_interrupt_callback(oc_int);
			pmic_enable_interrupt(oc_int, 1, "PMIC");
			break;
		}
	}
}
#endif

static void vio18_oc_int_handler(void)
{
	static unsigned int times;
	int len = 0;
#if defined(CONFIG_MTK_AEE_FEATURE)
	char oc_str[30] = "";
#endif
	pr_info("[%s]\n", __func__);

	pr_notice("VIO18_PG_DEB=%d,RGS_VIO18_PG_STATUS=%d\n",
		pmic_get_register_value(PMIC_VIO18_PG_DEB),
		pmic_get_register_value(PMIC_RGS_VIO18_PG_STATUS));
	pr_notice("RG_INT_EN_VIO18_OC=0x%x\n",
		pmic_get_register_value(PMIC_RG_INT_EN_VIO18_OC));
	pr_notice("RG_INT_MASK_VIO18_OC=0x%x\n",
		pmic_get_register_value(PMIC_RG_INT_MASK_VIO18_OC));
	pr_notice("RG_INT_STATUS_VIO18_OC=0x%x\n",
		pmic_get_register_value(PMIC_RG_INT_STATUS_VIO18_OC));
	pr_notice("RG_INT_RAW_STATUS_VIO18_OC=0x%x\n",
		pmic_get_register_value(
			PMIC_RG_INT_RAW_STATUS_VIO18_OC));
	pr_notice("LDO_VIO18_CON0=0x%x,LDO_VIO18_MON=0x%x\n",
		upmu_get_reg_value(MT6359_LDO_VIO18_CON0),
		upmu_get_reg_value(MT6359_LDO_VIO18_MON));
	pr_notice("LDO_VIO18_OP_EN=0x%x,LDO_VIO18_OP_CFG=0x%x\n",
		upmu_get_reg_value(MT6359_LDO_VIO18_OP_EN),
		upmu_get_reg_value(MT6359_LDO_VIO18_OP_CFG));
	pr_notice("VIO18_ANA_CON0=0x%x,VIO18_ANA_CON1=0x%x\n",
		upmu_get_reg_value(MT6359_VIO18_ANA_CON0),
		upmu_get_reg_value(MT6359_VIO18_ANA_CON1));
	pr_notice("XO_FPM_ISEL_M=0x%x\n",
		pmic_get_register_value(PMIC_XO_FPM_ISEL_M));
#if defined(CONFIG_MTK_AEE_FEATURE)
	len = snprintf(oc_str, 30, "PMIC OC:%s", "INT_VIO18_OC");
	if (len < 0)
		pr_err("[%s] error: snprintf return len < 0\n", __func__);

	aee_kernel_warning(oc_str,
			   "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
			   "INT_VIO18_OC");
#endif
	if (times >= 3)
		pmic_enable_interrupt(INT_VIO18_OC, 0, "PMIC");
	times++;
	pr_notice("disable OC interrupt: INT_VIO18_OC\n");
}


static void register_vio18_oc_interrupts(void)
{
	pmic_register_interrupt_callback(INT_VIO18_OC, vio18_oc_int_handler);
	pmic_enable_interrupt(INT_VIO18_OC, 1, "PMIC");
}

void PMIC_EINT_SETTING(struct platform_device *pdev)
{
	int ret = 0;

	/* MT6359 disable VIO18_PG/OC to debug VIO18 OC, must check!! */
	pmic_set_register_value(PMIC_RG_LDO_VIO18_OCFB_EN, 0x0);
	pmic_set_register_value(PMIC_RG_STRUP_VIO18_PG_ENB, 0x1);
	pmic_set_register_value(PMIC_RG_STRUP_VIO18_OC_ENB, 0x1);

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

	register_vio18_oc_interrupts();
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");

