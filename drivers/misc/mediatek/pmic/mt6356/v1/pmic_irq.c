/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/sched.h>

#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_reboot.h>
#include <mach/mtk_pmic_wrap.h>
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_debugfs.h"

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mtk_boot_common.h>
/*#include <mt-plat/mtk_boot.h> TBD*/
/*#include <mt-plat/mtk_gpt.h> TBD*/
#endif

/*---IPI Mailbox define---*/
/*#define IPIMB*/

/* Global variable */
int g_pmic_irq;
unsigned int g_eint_pmic_num = 176; /* TBD */
unsigned int g_cust_eint_mt_pmic_debounce_cn = 1;
unsigned int g_cust_eint_mt_pmic_type = 4;
unsigned int g_cust_eint_mt_pmic_debounce_en = 1;

/* PMIC extern variable */

#define IRQ_HANDLER_READY 1

/* Interrupt Setting */
static struct pmic_sp_irq psc_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_PWRKEY), PMIC_SP_IRQ_GEN(1, INT_HOMEKEY),
	PMIC_SP_IRQ_GEN(1, INT_PWRKEY_R), PMIC_SP_IRQ_GEN(1, INT_HOMEKEY_R),
	PMIC_SP_IRQ_GEN(1, INT_NI_LBAT_INT), PMIC_SP_IRQ_GEN(1, INT_CHRDET),
	PMIC_SP_IRQ_GEN(1, INT_CHRDET_EDGE),
	PMIC_SP_IRQ_GEN(1, INT_VCDT_HV_DET),
	PMIC_SP_IRQ_GEN(1, INT_PCHR_CM_VINC),
	PMIC_SP_IRQ_GEN(1, INT_PCHR_CM_VDEC), PMIC_SP_IRQ_GEN(1, INT_WATCHDOG),
	PMIC_SP_IRQ_GEN(1, INT_VBATON_UNDET),
	PMIC_SP_IRQ_GEN(1, INT_BVALID_DET), PMIC_SP_IRQ_GEN(1, INT_OV),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq bm_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_FG_BAT0_H), PMIC_SP_IRQ_GEN(1, INT_FG_BAT0_L),
	PMIC_SP_IRQ_GEN(1, INT_FG_CUR_H), PMIC_SP_IRQ_GEN(1, INT_FG_CUR_L),
	PMIC_SP_IRQ_GEN(1, INT_FG_ZCV), PMIC_SP_IRQ_GEN(1, INT_FG_BAT1_H),
	PMIC_SP_IRQ_GEN(1, INT_FG_BAT1_L),
	PMIC_SP_IRQ_GEN(1, INT_FG_N_CHARGE_L),
	PMIC_SP_IRQ_GEN(1, INT_FG_IAVG_H), PMIC_SP_IRQ_GEN(1, INT_FG_IAVG_L),
	PMIC_SP_IRQ_GEN(1, INT_FG_TIME_H),
	PMIC_SP_IRQ_GEN(1, INT_FG_DISCHARGE),
	PMIC_SP_IRQ_GEN(1, INT_FG_CHARGE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	},
	{
	PMIC_SP_IRQ_GEN(1, INT_BATON_LV), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(1, INT_BATON_BAT_IN),
	PMIC_SP_IRQ_GEN(1, INT_BATON_BAT_OUT), PMIC_SP_IRQ_GEN(1, INT_BIF),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq sck_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_RTC), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq hk_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_THR_H), PMIC_SP_IRQ_GEN(1, INT_THR_L),
	PMIC_SP_IRQ_GEN(1, INT_BAT_H), PMIC_SP_IRQ_GEN(1, INT_BAT_L),
	PMIC_SP_IRQ_GEN(1, INT_BAT2_H), PMIC_SP_IRQ_GEN(1, INT_BAT2_L),
	PMIC_SP_IRQ_GEN(1, INT_BAT_TEMP_H), PMIC_SP_IRQ_GEN(1, INT_BAT_TEMP_L),
	PMIC_SP_IRQ_GEN(1, INT_AUXADC_IMP), PMIC_SP_IRQ_GEN(1, INT_NAG_C_DLTV),
	PMIC_SP_IRQ_GEN(1, INT_JEITA_HOT), PMIC_SP_IRQ_GEN(1, INT_JEITA_WARM),
	PMIC_SP_IRQ_GEN(1, INT_JEITA_COOL), PMIC_SP_IRQ_GEN(1, INT_JEITA_COLD),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	},
	{
	PMIC_SP_IRQ_GEN(1, INT_TYPEC_H_MAX),
	PMIC_SP_IRQ_GEN(1, INT_TYPEC_H_MIN),
	PMIC_SP_IRQ_GEN(1, INT_TYPEC_L_MAX),
	PMIC_SP_IRQ_GEN(1, INT_TYPEC_L_MIN), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq xpp_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_TYPE_C_SINK), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq buck_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_VPROC_OC), PMIC_SP_IRQ_GEN(1, INT_VCORE_OC),
	PMIC_SP_IRQ_GEN(1, INT_VMODEM_OC), PMIC_SP_IRQ_GEN(1, INT_VS1_OC),
	PMIC_SP_IRQ_GEN(1, INT_VS2_OC), PMIC_SP_IRQ_GEN(1, INT_VPA_OC),
	PMIC_SP_IRQ_GEN(1, INT_VCORE_PREOC), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq ldo_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_VFE28_OC), PMIC_SP_IRQ_GEN(1, INT_VXO22_OC),
	PMIC_SP_IRQ_GEN(1, INT_VRF18_OC), PMIC_SP_IRQ_GEN(1, INT_VRF12_OC),
	PMIC_SP_IRQ_GEN(1, INT_VMIPI_OC), PMIC_SP_IRQ_GEN(1, INT_VCN33_OC),
	PMIC_SP_IRQ_GEN(1, INT_VCN28_OC), PMIC_SP_IRQ_GEN(1, INT_VCN18_OC),
	PMIC_SP_IRQ_GEN(1, INT_VCAMA_OC), PMIC_SP_IRQ_GEN(1, INT_VCAMD_OC),
	PMIC_SP_IRQ_GEN(1, INT_VCAMIO_OC), PMIC_SP_IRQ_GEN(1, INT_VLDO28_OC),
	PMIC_SP_IRQ_GEN(1, INT_VA12_OC), PMIC_SP_IRQ_GEN(1, INT_VAUX18_OC),
	PMIC_SP_IRQ_GEN(1, INT_VAUD28_OC), PMIC_SP_IRQ_GEN(1, INT_VIO28_OC),
	},
	{
	PMIC_SP_IRQ_GEN(1, INT_VIO18_OC),
	PMIC_SP_IRQ_GEN(1, INT_VSRAM_PROC_OC),
	PMIC_SP_IRQ_GEN(1, INT_VSRAM_OTHERS_OC),
	PMIC_SP_IRQ_GEN(1, INT_VSRAM_GPU_OC), PMIC_SP_IRQ_GEN(1, INT_VDRAM_OC),
	PMIC_SP_IRQ_GEN(1, INT_VMC_OC), PMIC_SP_IRQ_GEN(1, INT_VMCH_OC),
	PMIC_SP_IRQ_GEN(1, INT_VEMC_OC), PMIC_SP_IRQ_GEN(1, INT_VSIM1_OC),
	PMIC_SP_IRQ_GEN(1, INT_VSIM2_OC), PMIC_SP_IRQ_GEN(1, INT_VIBR_OC),
	PMIC_SP_IRQ_GEN(1, INT_VUSB_OC), PMIC_SP_IRQ_GEN(1, INT_VBIF28_OC),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq aud_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_AUDIO), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(1, INT_ACCDET),
	PMIC_SP_IRQ_GEN(1, INT_ACCDET_EINT),
	PMIC_SP_IRQ_GEN(1, INT_ACCDET_EINT1), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq misc_irqs[][PMIC_INT_WIDTH] = {
	{
	PMIC_SP_IRQ_GEN(1, INT_EINT_RTC32K_1V8_1),
	PMIC_SP_IRQ_GEN(1, INT_SPI_CMD_ALERT), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE), PMIC_SP_IRQ_GEN(0, NO_USE),
	PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

struct pmic_sp_interrupt sp_interrupts[] = {
	PMIC_SP_INTS_GEN(PSC_TOP, 1, psc_irqs, 2),
	PMIC_SP_INTS_GEN(BM_TOP, 2, bm_irqs, 4),
	PMIC_SP_INTS_GEN(SCK_TOP, 1, sck_irqs, 3),
	PMIC_SP_INTS_GEN(HK_TOP, 2, hk_irqs, 5),
	PMIC_SP_INTS_GEN(XPP_TOP, 1, xpp_irqs, 6),
	PMIC_SP_INTS_GEN(BUCK_TOP, 1, buck_irqs, 0),
	PMIC_SP_INTS_GEN(LDO_TOP, 2, ldo_irqs, 1),
	PMIC_SP_INTS_GEN(AUD_TOP, 1, aud_irqs, 7),
	PMIC_SP_INTS_GEN(MISC_TOP, 1, misc_irqs, 8),
};

unsigned int sp_interrupt_size = ARRAY_SIZE(sp_interrupts);

#if IRQ_HANDLER_READY
/* PWRKEY Int Handler */
void pwrkey_int_handler(void)
{
	PMICLOG("[%s] Press pwrkey %d\n", __func__,
		pmic_get_register_value(PMIC_PWRKEY_DEB));
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	kpd_pwrkey_pmic_handler(0x1);
#endif
}

void pwrkey_int_handler_r(void)
{
	PMICLOG("[%s] Release pwrkey %d\n", __func__,
		pmic_get_register_value(PMIC_PWRKEY_DEB));
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	kpd_pwrkey_pmic_handler(0x0);
#endif
}

/* Homekey Int Handler */
void homekey_int_handler(void)
{
	PMICLOG("[%s] Press homekey %d\n", __func__,
		pmic_get_register_value(PMIC_HOMEKEY_DEB));
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	kpd_pmic_rstkey_handler(0x1);
#endif
}

void homekey_int_handler_r(void)
{
	PMICLOG("[%s] Release homekey %d\n", __func__,
		pmic_get_register_value(PMIC_HOMEKEY_DEB));
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	kpd_pmic_rstkey_handler(0x0);
#endif
}

/* Chrdet Int Handler */
#if (CONFIG_MTK_GAUGE_VERSION != 30)
void chrdet_int_handler(void)
{
	PMICLOG("[%s]CHRDET status = %d....\n", __func__,
		pmic_get_register_value(PMIC_RGS_CHRDET));
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (!upmu_get_rgs_chrdet()) {
		int boot_mode = 0;

		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			PMICLOG("[%s] Unplug Charger/USB\n", __func__);
#ifdef CONFIG_MTK_RTC
			mt_power_off();
#endif
		}
	}
#endif
	pmic_set_register_value(PMIC_RG_USBDL_RST, 1);
#if defined(CONFIG_MTK_SMART_BATTERY)
	do_chrdet_int_task();
#endif
}
#endif /* CONFIG_MTK_GAUGE_VERSION != 30 */
#endif /* IRQ_HANDLER_READY */

/* May be removed(TBD) */
/* Auxadc Int Handler */
void auxadc_imp_int_handler_r(void)
{
	PMICLOG("%s() =%d\n", __func__,
		pmic_get_register_value(PMIC_AUXADC_ADC_OUT_IMP));
	/*clear IRQ */
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 1);
	/*restore to initial state */
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 0);
	/*turn off interrupt */
	pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP, 0);
}

/* General OC Int Handler */
void oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	char oc_str[30] = "";

	PMICLOG("[%s] int name=%s\n", __func__, int_name);
	switch (intNo) {
	case INT_VLDO28_OC:
		/* keep OC interrupt and keep tracking */
		pr_notice(PMICTAG "[PMIC_INT] PMIC OC: %s\n", int_name);
		break;
	default:
		/* issue AEE exception and disable OC interrupt */
		kernel_dump_exception_reg();
		snprintf(oc_str, 30, "PMIC OC:%s", int_name);
		aee_kernel_warning(
		    oc_str, "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
		    int_name);
		pmic_enable_interrupt(intNo, 0, "PMIC");
		pr_notice(PMICTAG "[PMIC_INT] disable OC interrupt: %s\n",
			  int_name);
		break;
	}
}

/*
 * PMIC Interrupt service
 */
struct task_struct *pmic_thread_handle;

#if !defined CONFIG_HAS_WAKELOCKS
struct wakeup_source pmicThread_lock;
#else
struct wake_lock pmicThread_lock;
#endif

void wake_up_pmic(void)
{
	PMICLOG("[%s]\n", __func__);
	if (pmic_thread_handle != NULL) {
		pmic_wake_lock(&pmicThread_lock);
		wake_up_process(pmic_thread_handle);
	} else {
		pr_debug(PMICTAG "[%s] pmic_thread_handle not ready\n",
		       __func__);
		return;
	}
}

irqreturn_t mt_pmic_eint_irq(int irq, void *desc)
{
	disable_irq_nosync(irq);
	PMICLOG("[%s] disable PMIC irq\n", __func__);
	wake_up_pmic();
	return IRQ_HANDLED;
}

static unsigned int get_spNo(enum PMIC_IRQ_ENUM intNo)
{
	if (intNo >= SP_PSC_TOP_START && intNo < SP_BM_TOP_START)
		return 0; /* SP_PSC_TOP */
	else if (intNo >= SP_BM_TOP_START && intNo < SP_SCK_TOP_START)
		return 1; /* SP_BM_TOP */
	else if (intNo >= SP_SCK_TOP_START && intNo < SP_HK_TOP_START)
		return 2; /* SP_SCK_TOP */
	else if (intNo >= SP_HK_TOP_START && intNo < SP_XPP_TOP_START)
		return 3; /* SP_HK_TOP */
	else if (intNo >= SP_XPP_TOP_START && intNo < SP_BUCK_TOP_START)
		return 4; /* SP_XPP_TOP */
	else if (intNo >= SP_BUCK_TOP_START && intNo < SP_LDO_TOP_START)
		return 5; /* SP_BUCK_TOP */
	else if (intNo >= SP_LDO_TOP_START && intNo < SP_AUD_TOP_START)
		return 6; /* SP_LDO_TOP */
	else if (intNo >= SP_AUD_TOP_START && intNo < SP_MISC_TOP_START)
		return 7; /* SP_AUD_TOP */
	else if (intNo >= SP_MISC_TOP_START && intNo < INT_ENUM_MAX)
		return 8; /* SP_MISC_TOP */
	return 99;
}

static unsigned int pmic_check_intNo(enum PMIC_IRQ_ENUM intNo,
				     unsigned int *spNo,
				     unsigned int *sp_conNo,
				     unsigned int *sp_irqNo)
{
	if (intNo >= INT_ENUM_MAX)
		return 1; /* fail intNo */

	*spNo = get_spNo(intNo);
	*sp_conNo = (intNo - sp_interrupts[*spNo].int_offset) / PMIC_INT_WIDTH;
	*sp_irqNo = intNo % PMIC_INT_WIDTH;
	return 0;
}

void pmic_enable_interrupt(enum PMIC_IRQ_ENUM intNo, unsigned int en,
			   char *str)
{
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int enable_reg;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_debug(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	enable_reg = sp_interrupts[spNo].enable + 0x6 * sp_conNo;
	PMICLOG(
		"[%s] intNo=%d en=%d str=%s spNo=%d sp_conNo=%d sp_irqNo=%d, Reg[0x%x]=0x%x\n",
		__func__, intNo, en, str, spNo, sp_conNo, sp_irqNo, enable_reg,
		upmu_get_reg_value(enable_reg));
	if (en == 1)
		pmic_config_interface(enable_reg + 0x2, 0x1, 0x1, sp_irqNo);
	else if (en == 0)
		pmic_config_interface(enable_reg + 0x4, 0x1, 0x1, sp_irqNo);
	PMICLOG("[%s] after, Reg[0x%x]=0x%x\n", __func__, enable_reg,
		upmu_get_reg_value(enable_reg));
}

void pmic_mask_interrupt(enum PMIC_IRQ_ENUM intNo, char *str)
{
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int mask_reg;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_debug(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	mask_reg = sp_interrupts[spNo].mask + 0x6 * sp_conNo;
	PMICLOG(
		"[%s] intNo=%d str=%s spNo=%d sp_conNo=%d sp_irqNo=%d, Reg[0x%x]=0x%x\n",
		__func__, intNo, str, spNo, sp_conNo, sp_irqNo, mask_reg,
		upmu_get_reg_value(mask_reg));
	/* MASK_SET */
	pmic_config_interface(mask_reg + 0x2, 0x1, 0x1, sp_irqNo);
	PMICLOG("[%s] after, Reg[0x%x]=0x%x\n", __func__, mask_reg,
		upmu_get_reg_value(mask_reg));
}

void pmic_unmask_interrupt(enum PMIC_IRQ_ENUM intNo, char *str)
{
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int mask_reg;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_debug(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	mask_reg = sp_interrupts[spNo].mask + 0x6 * sp_conNo;
	PMICLOG(
		"[%s] intNo=%d str=%s spNo=%d sp_conNo=%d sp_irqNo=%d, Reg[0x%x]=0x%x\n",
		__func__, intNo, str, spNo, sp_conNo, sp_irqNo, mask_reg,
		upmu_get_reg_value(mask_reg));
	/* MASK_CLR */
	pmic_config_interface(mask_reg + 0x4, 0x1, 0x1, sp_irqNo);
	PMICLOG("[%s] after, Reg[0x%x]=0x%x\n", __func__, mask_reg,
		upmu_get_reg_value(mask_reg));
}

void pmic_register_interrupt_callback(enum PMIC_IRQ_ENUM intNo,
				      void(EINT_FUNC_PTR)(void))
{
	unsigned int spNo, sp_conNo, sp_irqNo;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_debug(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	} else if (sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].callback !=
		   NULL) {
		pr_debug(PMICTAG "[%s] register callback conflict intNo=%d\n",
		       __func__, intNo);
		return;
	}

	PMICLOG("[%s] intNo=%d\n", __func__, intNo);
	sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].callback =
	    EINT_FUNC_PTR;
}

#if ENABLE_ALL_OC_IRQ
/* register general oc interrupt handler */
void pmic_register_oc_interrupt_callback(enum PMIC_IRQ_ENUM intNo)
{
	unsigned int spNo, sp_conNo, sp_irqNo;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_debug(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	PMICLOG("[%s] intNo=%d\n", __func__, intNo);
	sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].oc_callback =
	    oc_int_handler;
}

/* register and enable all oc interrupt */
void register_all_oc_interrupts(void)
{
	enum PMIC_IRQ_ENUM oc_interrupt;

	/* BUCK OC */
	for (oc_interrupt = INT_VPROC_OC; oc_interrupt <= INT_VPA_OC;
	     oc_interrupt++) {
		pmic_register_oc_interrupt_callback(oc_interrupt);
		pmic_enable_interrupt(oc_interrupt, 1, "PMIC");
	}
	/* LDO OC */
	for (oc_interrupt = INT_VFE28_OC; oc_interrupt <= INT_VBIF28_OC;
	     oc_interrupt++) {
		switch (oc_interrupt) {
		case INT_VBIF28_OC:
		case INT_VSIM1_OC:
		case INT_VSIM2_OC:
		case INT_VMCH_OC:
		case INT_VCAMA_OC:
		case INT_VCAMD_OC:
		case INT_VCAMIO_OC:
		case INT_VIBR_OC:
			PMICLOG("[PMIC_INT] non-enabled OC: %d\n",
				oc_interrupt);
			break;
#if 0
		case INT_VCAMA_OC:
		case INT_VCAMD_OC:
			PMICLOG(
				"[PMIC_INT] OC:%d should be enabled after power on\n",
				oc_interrupt);
			pmic_register_oc_interrupt_callback(oc_interrupt);
			break;
#endif
		default:
			pmic_register_oc_interrupt_callback(oc_interrupt);
			pmic_enable_interrupt(oc_interrupt, 1, "PMIC");
			break;
		}
	}
}
#endif

static void pmic_sp_irq_handler(unsigned int spNo, unsigned int sp_conNo,
				unsigned int sp_int_status)
{
	unsigned int i;
	struct pmic_sp_irq *sp_irq;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

	if (sp_int_status == 0)
		return; /* this subpack control has no interrupt triggered */

	if ((sp_interrupts[spNo].status + 0x6 * sp_conNo) ==
		PMIC_RG_INT_STATUS_BAT_H_ADDR &&
	    (sp_int_status == 0x4 || sp_int_status == 0x8 ||
	     sp_int_status == 0xC)) {
		if (__ratelimit(&ratelimit)) {
			pr_notice(
			    PMICTAG "[PMIC_INT] Reg[0x%x]=0x%x\n",
			    (sp_interrupts[spNo].status + 0x6 * sp_conNo),
			    sp_int_status);
		}
	} else {
		pr_notice(PMICTAG "[PMIC_INT] Reg[0x%x]=0x%x\n",
			  (sp_interrupts[spNo].status + 0x6 * sp_conNo),
			  sp_int_status);
	}
#if 0
	/* clear interrupt status in this subpack control */
	upmu_set_reg_value((sp_interrupts[spNo].status + 0x6 * sp_conNo),
	    sp_int_status);
#else /* prevent from MT6356 glitch problem */
	/* clear interrupt status by CLR enable register */
	upmu_set_reg_value((sp_interrupts[spNo].enable + 0x6 * sp_conNo) + 0x4,
			   sp_int_status);
	/* delay 3T~4T 32K clock (96us~128us) */
	udelay(150);
	/* restore enable register */
	upmu_set_reg_value((sp_interrupts[spNo].enable + 0x6 * sp_conNo) + 0x2,
			   sp_int_status);
#endif
	for (i = 0; i < PMIC_INT_WIDTH; i++) {
		if (sp_int_status & (1 << i)) {
			sp_irq = &(sp_interrupts[spNo].sp_irqs[sp_conNo][i]);
			PMICLOG("[PMIC_INT][%s]\n", sp_irq->name);
			sp_irq->times++;
			if (sp_irq->callback != NULL)
				sp_irq->callback();
			if (sp_irq->oc_callback != NULL) {
				sp_irq->oc_callback(
				    (sp_interrupts[spNo].int_offset +
				     sp_conNo * PMIC_INT_WIDTH + i),
				    sp_irq->name);
			}
		}
	}
}

static void pmic_int_handler(void)
{
	unsigned int spNo, sp_conNo;
	unsigned int status_reg;
	unsigned int top_int_status = 0, sp_int_status = 0;

	top_int_status = upmu_get_reg_value(MT6356_TOP_INT_STATUS0);

	for (spNo = 0; spNo < sp_interrupt_size; spNo++) {
		if (!(top_int_status & (1 << sp_interrupts[spNo].top_int_bit)))
			continue; /* this subpack has no interrupt triggered */
		for (sp_conNo = 0; sp_conNo < sp_interrupts[spNo].con_len;
		     sp_conNo++) {
			status_reg =
			    sp_interrupts[spNo].status + 0x6 * sp_conNo;
			sp_int_status = upmu_get_reg_value(status_reg);
			pmic_sp_irq_handler(spNo, sp_conNo, sp_int_status);
		}
	}
}

int pmic_thread_kthread(void *x)
{
	unsigned int spNo, sp_conNo;
	unsigned int status_reg;
	unsigned int sp_int_status = 0;
#ifdef IPIMB
#else
	unsigned int pwrap_eint_status = 0;
#endif

#if 0
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
#else
	/* try to modify pmic irq priority to NICE = -19 */
	set_user_nice(current, (MIN_NICE + 1));
#endif

	PMICLOG("[PMIC_INT] enter\n");

	/* Run on a process content */
	while (1) {
#ifdef IPIMB
#else
		pwrap_eint_status = pmic_wrap_eint_status();
		PMICLOG("[PMIC_INT] pwrap_eint_status=0x%x\n",
			pwrap_eint_status);
#endif
		pmic_int_handler();
#ifdef IPIMB
#else
		pmic_wrap_eint_clr(0x0);
#endif
		for (spNo = 0; spNo < sp_interrupt_size; spNo++) {
			for (sp_conNo = 0;
			     sp_conNo < sp_interrupts[spNo].con_len;
			     sp_conNo++) {
				status_reg = sp_interrupts[spNo].status +
					     0x6 * sp_conNo;
				sp_int_status = upmu_get_reg_value(status_reg);
				PMICLOG("[PMIC_INT] after, Reg[0x%x]=0x%x\n",
					status_reg, sp_int_status);
			}
		}
		pmic_wake_unlock(&pmicThread_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		enable_irq(g_pmic_irq);
		schedule();
		if (g_pmic_irq < 0)
			break;
	}

	return 0;
}

static void irq_thread_init(void)
{
	/* create pmic irq thread handler*/
	pmic_thread_handle =
	    kthread_create(pmic_thread_kthread, (void *)NULL, "pmic_thread");
	if (IS_ERR(pmic_thread_handle)) {
		pmic_thread_handle = NULL;
		pr_debug(PMICTAG "[pmic_thread_kthread] creation fails\n");
	} else {
		PMICLOG("[pmic_thread_kthread] kthread_create Done\n");
	}
}

static void register_irq_handlers(void)
{
#if IRQ_HANDLER_READY
	pmic_register_interrupt_callback(INT_PWRKEY, pwrkey_int_handler);
	pmic_register_interrupt_callback(INT_HOMEKEY, homekey_int_handler);
	pmic_register_interrupt_callback(INT_PWRKEY_R, pwrkey_int_handler_r);
	pmic_register_interrupt_callback(INT_HOMEKEY_R, homekey_int_handler_r);

#if (CONFIG_MTK_GAUGE_VERSION != 30)
	pmic_register_interrupt_callback(INT_CHRDET_EDGE, chrdet_int_handler);
#endif
	pmic_register_interrupt_callback(INT_BAT_L, bat_l_int_handler);
	pmic_register_interrupt_callback(INT_BAT_H, bat_h_int_handler);

	pmic_register_interrupt_callback(INT_FG_CUR_H, fg_cur_h_int_handler);
	pmic_register_interrupt_callback(INT_FG_CUR_L, fg_cur_l_int_handler);
#endif
#if ENABLE_ALL_OC_IRQ
	register_all_oc_interrupts();
#endif
}

static void enable_pmic_irqs(void)
{
	pmic_enable_interrupt(INT_PWRKEY, 1, "PMIC");
	pmic_enable_interrupt(INT_HOMEKEY, 1, "PMIC");
	pmic_enable_interrupt(INT_PWRKEY_R, 1, "PMIC");
	pmic_enable_interrupt(INT_HOMEKEY_R, 1, "PMIC");

	pmic_enable_interrupt(INT_CHRDET_EDGE, 1, "PMIC");
}

void PMIC_EINT_SETTING(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int spNo, sp_conNo;
	unsigned int enable_reg;
	struct device_node *node = pdev->dev.of_node;

	/* unmask PMIC TOP interrupt */
	pmic_set_register_value(PMIC_TOP_INT_MASK_CON0_CLR, 0x1FF);

	/* create pmic irq thread handler*/
	irq_thread_init();

	/* Disable all interrupt for initializing */
	for (spNo = 0; spNo < sp_interrupt_size; spNo++) {
		for (sp_conNo = 0; sp_conNo < sp_interrupts[spNo].con_len;
		     sp_conNo++) {
			enable_reg =
			    sp_interrupts[spNo].enable + 0x6 * sp_conNo;
			upmu_set_reg_value(enable_reg, 0);
		}
	}

	/* For all interrupt events, turn on interrupt module clock */
	pmic_set_register_value(PMIC_RG_INTRP_CK_PDN, 0);
	/* For BUCK PREOC related interrupt, please turn on intrp_pre_oc_ck
	 * (1MHz)
	 */
	/* This clock is default on */
	/*pmic_set_register_value(RG_INTRP_PRE_OC_CK_PDN, 0); TBD*/

	register_irq_handlers();
	enable_pmic_irqs();

	if (node) {
		/* no debounce setting */
		g_pmic_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(g_pmic_irq, (irq_handler_t)mt_pmic_eint_irq,
				  IRQF_TRIGGER_NONE, "pmic-eint", NULL);
		if (ret > 0)
			pr_debug(PMICTAG "EINT IRQ NOT AVAILABLE\n");
		enable_irq_wake(g_pmic_irq);
	} else
		pr_debug(PMICTAG "can't find compatible node\n");

	PMICLOG("[CUST_EINT] CUST_EINT_MT_PMIC_MT6356_NUM=%d\n",
		g_eint_pmic_num);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n",
		g_cust_eint_mt_pmic_debounce_cn);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n",
		g_cust_eint_mt_pmic_type);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n",
		g_cust_eint_mt_pmic_debounce_en);
}

/*****************************************************************************
 * PMIC Interrupt debugfs
 ******************************************************************************/
struct pmic_irq_dbg_st dbg_data[4];

enum { PMIC_IRQ_DBG_LIST,
	   PMIC_IRQ_DBG_LIST_ENABLED,
	   PMIC_IRQ_DBG_ENABLE,
	   PMIC_IRQ_DBG_MASK,
	   PMIC_IRQ_DBG_MAX,
};

static int list_pmic_irq(struct seq_file *s)
{
	unsigned int i;
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int en;
	unsigned int mask;
	void *callback;
	struct pmic_sp_irq *sp_irq;

	seq_printf(s, "Num: %20s, %8s, event times\n", "INT Name", "Status");
	for (i = 0; i < INT_ENUM_MAX; i++) {
		pmic_check_intNo(i, &spNo, &sp_conNo, &sp_irqNo);
		en = upmu_get_reg_value(sp_interrupts[spNo].enable +
					0x6 * sp_conNo);
		mask = upmu_get_reg_value(sp_interrupts[spNo].mask +
					  0x6 * sp_conNo);
		sp_irq = &(sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo]);
		if (sp_irq->used == 0) {
			seq_printf(s, "%3d: NO_USE\n", i);
			continue;
		}
		if (sp_irq->callback)
			callback = sp_irq->callback;
		else
			callback = sp_irq->oc_callback;
		seq_printf(s, "%3d: %20s, %8s%s, %d times, callback=%pf\n", i,
			   sp_irq->name,
			   en & (1 << sp_irqNo) ? "enabled" : "disabled",
			   mask & (1 << sp_irqNo) ? "(m)" : "", sp_irq->times,
			   callback);
	}
	return 0;
}

static int list_enabled_pmic_irq(struct seq_file *s)
{
	unsigned int i;
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int en;
	unsigned int mask;
	void *callback;
	struct pmic_sp_irq *sp_irq;

	seq_printf(s, "Num: %20s, %8s, event times\n", "INT Name", "Status");
	for (i = 0; i < INT_ENUM_MAX; i++) {
		pmic_check_intNo(i, &spNo, &sp_conNo, &sp_irqNo);
		en = upmu_get_reg_value(sp_interrupts[spNo].enable +
					0x6 * sp_conNo);
		mask = upmu_get_reg_value(sp_interrupts[spNo].mask +
					  0x6 * sp_conNo);
		if (!(en & (1 << sp_irqNo)))
			continue;
		sp_irq = &(sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo]);
		if (sp_irq->callback)
			callback = sp_irq->callback;
		else
			callback = sp_irq->oc_callback;
		seq_printf(s, "%3d: %20s, %8s%s, %d times, callback=%pf\n", i,
			   sp_irq->name, "enabled",
			   mask & (1 << sp_irqNo) ? "(m)" : "", sp_irq->times,
			   callback);
	}
	return 0;
}

static int pmic_irq_dbg_show(struct seq_file *s, void *unused)
{
	struct pmic_irq_dbg_st *dbg_st = s->private;

	switch (dbg_st->dbg_id) {
	case PMIC_IRQ_DBG_LIST:
		list_pmic_irq(s);
		break;
	case PMIC_IRQ_DBG_LIST_ENABLED:
		list_enabled_pmic_irq(s);
		break;
	default:
		break;
	}
	return 0;
}

static int pmic_irq_dbg_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_open(file, pmic_irq_dbg_show, inode->i_private);
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t pmic_irq_dbg_write(struct file *file,
				  const char __user *user_buffer, size_t count,
				  loff_t *position)
{
	struct pmic_irq_dbg_st *dbg_st = file->private_data;
	char buf[10] = {0};
	char *buf_ptr = NULL;
	char *s_intNo;
	char *s_state;
	unsigned int intNo = 999, state = 2; /* initialize as invalid value */
	int ret = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, position,
				     user_buffer, count);
	if (ret < 0)
		return ret;
	buf_ptr = (char *)buf;
	s_intNo = strsep(&buf_ptr, " ");
	s_state = strsep(&buf_ptr, " ");
	if (s_intNo)
		ret = kstrtou32(s_intNo, 10, (unsigned int *)&intNo);
	if (s_state)
		ret = kstrtou32(s_state, 10, (unsigned int *)&state);

	switch (dbg_st->dbg_id) {
	case PMIC_IRQ_DBG_ENABLE:
		pmic_enable_interrupt(intNo, state, "pmic_irq_dbg");
		break;
	case PMIC_IRQ_DBG_MASK:
		if (state == 1)
			pmic_mask_interrupt(intNo, "pmic_irq_dbg");
		else if (state == 0)
			pmic_unmask_interrupt(intNo, "pmic_irq_dbg");
		break;
	default:
		break;
	}

	return count;
}

static int pmic_irq_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_release(inode, file);
	return 0;
}

static const struct file_operations pmic_irq_dbg_fops = {
	.open = pmic_irq_dbg_open,
	.read = seq_read,
	.write = pmic_irq_dbg_write,
	.llseek = seq_lseek,
	.release = pmic_irq_release,
};

int pmic_irq_debug_init(struct dentry *debug_dir)
{
	struct dentry *pmic_irq_dir;

	if (IS_ERR(debug_dir) || !debug_dir) {
		pr_debug(PMICTAG "dir mtk_pmic does not exist\n");
		return -1;
	}
	pmic_irq_dir = debugfs_create_dir("pmic_irq", debug_dir);
	if (IS_ERR(pmic_irq_dir) || !pmic_irq_dir) {
		pr_debug(PMICTAG
		       "fail to mkdir /sys/kernel/debug/mtk_pmic/pmic_irq\n");
		return -1;
	}
	/* PMIC irq debug init */
	dbg_data[0].dbg_id = PMIC_IRQ_DBG_LIST;
	debugfs_create_file("list_pmic_irq", (S_IFREG | 0444), pmic_irq_dir,
			    (void *)&dbg_data[0], &pmic_irq_dbg_fops);

	dbg_data[1].dbg_id = PMIC_IRQ_DBG_LIST_ENABLED;
	debugfs_create_file("list_enabled_pmic_irq", (S_IFREG | 0444),
			    pmic_irq_dir, (void *)&dbg_data[1],
			    &pmic_irq_dbg_fops);

	dbg_data[2].dbg_id = PMIC_IRQ_DBG_ENABLE;
	debugfs_create_file("enable_pmic_irq", (S_IFREG | 0444),
			    pmic_irq_dir, (void *)&dbg_data[2],
			    &pmic_irq_dbg_fops);

	dbg_data[3].dbg_id = PMIC_IRQ_DBG_MASK;
	debugfs_create_file("mask_pmic_irq", (S_IFREG | 0444), pmic_irq_dir,
			    (void *)&dbg_data[3], &pmic_irq_dbg_fops);

	return 0;
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");
