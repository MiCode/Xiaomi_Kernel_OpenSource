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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"

#include <mach/mt_pmic_wrap.h>
#include <mt-plat/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_boot_common.h>
/*#include <mach/system.h> TBD*/
#include <mt-plat/mt_gpt.h>
#endif

#if defined(CONFIG_MTK_SMART_BATTERY)
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mach/mt_battery_meter.h>
#endif
#include "include/pmic_throttling_dlpt.h"
#include <mach/mt_pmic.h>
#include <mt-plat/mt_reboot.h>
#include <mt-plat/aee.h>

/*#define VPA_OC*/
/*#include <mach/mt_ccci_common.h>*/

/*---IPI Mailbox define---*/
/*#define IPIMB*/

/*****************************************************************************
 * Global variable
 ******************************************************************************/
int g_pmic_irq;
unsigned int g_eint_pmic_num = 176;
unsigned int g_cust_eint_mt_pmic_debounce_cn = 1;
unsigned int g_cust_eint_mt_pmic_type = 4;
unsigned int g_cust_eint_mt_pmic_debounce_en = 1;

/*****************************************************************************
 * PMIC extern variable
 ******************************************************************************/
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static bool long_pwrkey_press;
static unsigned long timer_pre;
static unsigned long timer_pos;
#define LONG_PWRKEY_PRESS_TIME_UNIT     500     /*500ms */
#define LONG_PWRKEY_PRESS_TIME_US       1000000 /*500ms */
#endif

/*****************************************************************************
 * PMIC extern function
 ******************************************************************************/

#ifdef CONFIG_ARM /* fix kpd build error only */
void kpd_pwrkey_pmic_handler(unsigned long pressed)
{}
void kpd_pmic_rstkey_handler(unsigned long pressed)
{}
#endif

/*****************************************************************************
 * interrupt Setting
 ******************************************************************************/
static struct pmic_interrupt_bit interrupt_status0[] = {
	PMIC_S_INT_GEN(RG_INT_EN_PWRKEY),
	PMIC_S_INT_GEN(RG_INT_EN_HOMEKEY),
	PMIC_S_INT_GEN(RG_INT_EN_PWRKEY_R),
	PMIC_S_INT_GEN(RG_INT_EN_HOMEKEY_R),
	PMIC_S_INT_GEN(RG_INT_EN_NI_LBAT_INT),
	PMIC_S_INT_GEN(RG_INT_EN_CHRDET),
	PMIC_S_INT_GEN(RG_INT_EN_CHRDET_EDGE),
	PMIC_S_INT_GEN(RG_INT_EN_BATON_LV),
	PMIC_S_INT_GEN(RG_INT_EN_BATON_HV),
	PMIC_S_INT_GEN(RG_INT_EN_BATON_BAT_IN),
	PMIC_S_INT_GEN(RG_INT_EN_BATON_BAT_OUT),
	PMIC_S_INT_GEN(RG_INT_EN_RTC),
	PMIC_S_INT_GEN(RG_INT_EN_BIF),
	PMIC_S_INT_GEN(RG_INT_EN_VCDT_HV_DET),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),};

static struct pmic_interrupt_bit interrupt_status1[] = {
	PMIC_S_INT_GEN(RG_INT_EN_THR_H),
	PMIC_S_INT_GEN(RG_INT_EN_THR_L),
	PMIC_S_INT_GEN(RG_INT_EN_BAT_H),
	PMIC_S_INT_GEN(RG_INT_EN_BAT_L),
	PMIC_S_INT_GEN(RG_INT_EN_BAT2_H),
	PMIC_S_INT_GEN(RG_INT_EN_BAT2_L),
	PMIC_S_INT_GEN(RG_INT_EN_BAT_TEMP_H),
	PMIC_S_INT_GEN(RG_INT_EN_BAT_TEMP_L),
	PMIC_S_INT_GEN(RG_INT_EN_AUXADC_IMP),
	PMIC_S_INT_GEN(RG_INT_EN_NAG_C_DLTV),
	PMIC_S_INT_GEN(RG_INT_EN_JEITA_HOT),
	PMIC_S_INT_GEN(RG_INT_EN_JEITA_WARM),
	PMIC_S_INT_GEN(RG_INT_EN_JEITA_COOL),
	PMIC_S_INT_GEN(RG_INT_EN_JEITA_COLD),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
};

static struct pmic_interrupt_bit interrupt_status2[] = {
	PMIC_S_INT_GEN(RG_INT_EN_VCORE_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMD1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMODEM_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VS1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VS2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VDRAM_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VPA1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VPA2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCORE_PREOC),
	PMIC_S_INT_GEN(RG_INT_EN_VA10_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VA12_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VA18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VBIF28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMA1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMA2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMAF_OC),
};

static struct pmic_interrupt_bit interrupt_status3[] = {
	PMIC_S_INT_GEN(RG_INT_EN_VCAMD1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMD2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMIO_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCN18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCN28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCN33_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VEFUSE_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VEMC_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VFE28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VGP3_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VIBR_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VIO18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VIO28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMC_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMCH_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMIPI_OC),
};

static struct pmic_interrupt_bit interrupt_status4[] = {
	PMIC_S_INT_GEN(RG_INT_EN_VRF12_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VRF18_1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VRF18_2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSIM1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSIM2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_CORE_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_DVFS1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_DVFS2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_GPU_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_MD_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VUFS18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VUSB33_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VXO22_OC),
	PMIC_S_INT_GEN(NO_USE),/*RG_INT_EN_CON4*/
	PMIC_S_INT_GEN(NO_USE),/*RG_INT_EN_CON4*/
	PMIC_S_INT_GEN(NO_USE),/*RG_INT_EN_CON4*/
};

static struct pmic_interrupt_bit interrupt_status5[] = {
	PMIC_S_INT_GEN(RG_INT_EN_FG_BAT0_H),
	PMIC_S_INT_GEN(RG_INT_EN_FG_BAT0_L),
	PMIC_S_INT_GEN(RG_INT_EN_FG_CUR_H),
	PMIC_S_INT_GEN(RG_INT_EN_FG_CUR_L),
	PMIC_S_INT_GEN(RG_INT_EN_FG_ZCV),
	PMIC_S_INT_GEN(RG_INT_EN_FG_BAT1_H),
	PMIC_S_INT_GEN(RG_INT_EN_FG_BAT1_L),
	PMIC_S_INT_GEN(RG_INT_EN_FG_N_CHARGE_L),
	PMIC_S_INT_GEN(RG_INT_EN_FG_IAVG_H),
	PMIC_S_INT_GEN(RG_INT_EN_FG_IAVG_L),
	PMIC_S_INT_GEN(RG_INT_EN_FG_TIME_H),
	PMIC_S_INT_GEN(RG_INT_EN_FG_DISCHARGE),
	PMIC_S_INT_GEN(RG_INT_EN_FG_CHARGE),
	PMIC_S_INT_GEN(NO_USE),/*RG_INT_EN_CON5*/
	PMIC_S_INT_GEN(NO_USE),/*RG_INT_EN_CON5*/
	PMIC_S_INT_GEN(NO_USE),/*RG_INT_EN_CON5*/
};

struct pmic_interrupts interrupts[] = {
	PMIC_M_INTS_GEN(MT6335_INT_STATUS0, MT6335_INT_RAW_STATUS0,
			MT6335_INT_CON0, MT6335_INT_MASK_CON0, interrupt_status0),
	PMIC_M_INTS_GEN(MT6335_INT_STATUS1, MT6335_INT_RAW_STATUS1,
			MT6335_INT_CON1, MT6335_INT_MASK_CON1, interrupt_status1),
	PMIC_M_INTS_GEN(MT6335_INT_STATUS2, MT6335_INT_RAW_STATUS2,
			MT6335_INT_CON2, MT6335_INT_MASK_CON2, interrupt_status2),
	PMIC_M_INTS_GEN(MT6335_INT_STATUS3, MT6335_INT_RAW_STATUS3,
			MT6335_INT_CON3, MT6335_INT_MASK_CON3, interrupt_status3),
	PMIC_M_INTS_GEN(MT6335_INT_STATUS4, MT6335_INT_RAW_STATUS4,
			MT6335_INT_CON4, MT6335_INT_MASK_CON4, interrupt_status4),
	PMIC_M_INTS_GEN(MT6335_INT_STATUS5, MT6335_INT_RAW_STATUS5,
			MT6335_INT_CON5, MT6335_INT_MASK_CON5, interrupt_status5),
};

int interrupts_size = ARRAY_SIZE(interrupts);

/*****************************************************************************
 * PWRKEY Int Handler
 ******************************************************************************/
void pwrkey_int_handler(void)
{

	PMICLOG("[pwrkey_int_handler] Press pwrkey %d\n",
		pmic_get_register_value(PMIC_PWRKEY_DEB));

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT)
		timer_pre = sched_clock();
#endif
#if defined(CONFIG_FPGA_EARLY_PORTING)
#else
	kpd_pwrkey_pmic_handler(0x1);
#endif
}

void pwrkey_int_handler_r(void)
{
	PMICLOG("[pwrkey_int_handler_r] Release pwrkey %d\n",
		pmic_get_register_value(PMIC_PWRKEY_DEB));
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && timer_pre != 0) {
		timer_pos = sched_clock();
		if (timer_pos - timer_pre >=
		    LONG_PWRKEY_PRESS_TIME_UNIT * LONG_PWRKEY_PRESS_TIME_US) {
			long_pwrkey_press = true;
		}
		PMICLOG
		    ("timer_pos = %ld, timer_pre = %ld, timer_pos-timer_pre = %ld, long_pwrkey_press = %d\r\n",
		     timer_pos, timer_pre, timer_pos - timer_pre, long_pwrkey_press);
		if (long_pwrkey_press) {	/*500ms */
			PMICLOG
			    ("Power Key Pressed during kernel power off charging, reboot OS\r\n");
			arch_reset(0, NULL);
		}
	}
#endif
#if defined(CONFIG_FPGA_EARLY_PORTING)
#else
	kpd_pwrkey_pmic_handler(0x0);
#endif
}

/*****************************************************************************
 * Homekey Int Handler
 ******************************************************************************/
void homekey_int_handler(void)
{
	PMICLOG("[homekey_int_handler] Press homekey %d\n",
		pmic_get_register_value(PMIC_HOMEKEY_DEB));
#if defined(CONFIG_FPGA_EARLY_PORTING)
#else
	kpd_pmic_rstkey_handler(0x1);
#endif
}

void homekey_int_handler_r(void)
{
	PMICLOG("[homekey_int_handler_r] Release homekey %d\n",
		pmic_get_register_value(PMIC_HOMEKEY_DEB));
#if defined(CONFIG_FPGA_EARLY_PORTING)
#else
	kpd_pmic_rstkey_handler(0x0);
#endif
}

/*****************************************************************************
 * VPA OC Int Handler
 ******************************************************************************/
void buck_oc_detect(void)
{
#ifdef VPA_OC
	/*pmic_enable_interrupt(19, 1, "PMIC");*/
#endif
}

#ifdef VPA_OC
void vpa_oc_int_handler(void)
{
	#ifdef CONFIG_MTK_CCCI_DEVICES
	int data_int32 = 43690;
	#endif

	pr_debug("[PMIC][vpa_oc_int_handler]\n");
	#ifdef CONFIG_MTK_CCCI_DEVICES
	int ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR, (char *)&data_int32, 4);

	if (ret)
		pr_debug("[PMIC][vpa_oc_int_handler] - exec_ccci_kern_func_by_md_id - msg fail\n");

	pr_debug("[PMIC][vpa_oc_int_handler] - exec_ccci_kern_func_by_md_id - msg pass\n");
	#endif

	pmic_enable_interrupt(38, 0, "PMIC");
}
#endif

/*****************************************************************************
 * Chrdet Int Handler
 ******************************************************************************/
void chrdet_int_handler(void)
{
	PMICLOG("[chrdet_int_handler]CHRDET status = %d....\n",
		pmic_get_register_value(PMIC_RGS_CHRDET));

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (!upmu_get_rgs_chrdet()) {
		int boot_mode = 0;

		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			PMICLOG("[chrdet_int_handler] Unplug Charger/USB\n");
			mt_power_off();
		}
	}
#endif
	pmic_set_register_value(PMIC_RG_USBDL_RST, 1);
#if defined(CONFIG_MTK_SMART_BATTERY)
	do_chrdet_int_task();
#endif
}

/*****************************************************************************
 * Auxadc Int Handler
 ******************************************************************************/
void auxadc_imp_int_handler_r(void)
{
	PMICLOG("auxadc_imp_int_handler_r() =%d\n",
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

/*****************************************************************************
 * Ldo OC Int Handler
 ******************************************************************************/
void ldo_oc_int_handler(void)
{
#if 0
	if (pmic_get_register_value(PMIC_OC_STATUS_VMCH))
		msdc_sd_power_off();
#endif
}

void pmic_ldo_oc_int_register(void)
{
#if 0
	pmic_register_interrupt_callback(31, ldo_oc_int_handler);
	pmic_enable_interrupt(31, 1, "PMIC");
#endif
}
EXPORT_SYMBOL(pmic_ldo_oc_int_register);

/*****************************************************************************
 * General OC Int Handler
 ******************************************************************************/
void oc_int_handler(PMIC_IRQ_ENUM intNo, const char *int_name)
{
	PMICLOG("[general_oc_int_handler] int name=%s\n", int_name);
	switch (intNo) {
	default:
		/* issue AEE exception and disable OC interrupt */
		/* TBD: dump_exception_reg*/
		aee_kernel_warning("PMIC OC", "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s", int_name);
		pmic_enable_interrupt(intNo, 0, "PMIC");
		pr_err(PMICTAG "[PMIC_INT] disable OC interrupt: %s\n", int_name);
		break;
	}
}

/*****************************************************************************
 * PMIC Interrupt service
 ******************************************************************************/
static DEFINE_MUTEX(pmic_mutex);
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
		pr_err(PMICTAG "[%s] pmic_thread_handle not ready\n", __func__);
		return;
	}
}

irqreturn_t mt_pmic_eint_irq(int irq, void *desc)
{
	disable_irq_nosync(irq);
	PMICLOG("[mt_pmic_eint_irq] disable PMIC irq\n");
	wake_up_pmic();
	return IRQ_HANDLED;
}

void pmic_enable_interrupt(PMIC_IRQ_ENUM intNo, unsigned int en, char *str)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_err(PMICTAG "[pmic_enable_interrupt] fail intno=%d\r\n", intNo);
		return;
	}

	PMICLOG("[pmic_enable_interrupt] intno=%d en=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, en, str, shift, no, interrupts[shift].en,
		upmu_get_reg_value(interrupts[shift].en));

	if (en == 1)
		pmic_config_interface(interrupts[shift].set, 0x1, 0x1, no);
	else if (en == 0)
		pmic_config_interface(interrupts[shift].clear, 0x1, 0x1, no);

	PMICLOG("[pmic_enable_interrupt] after [0x%x]=0x%x\r\n",
		interrupts[shift].en, upmu_get_reg_value(interrupts[shift].en));

}

void pmic_mask_interrupt(PMIC_IRQ_ENUM intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_err(PMICTAG "[pmic_mask_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[pmic_mask_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, interrupts[shift].mask,
		upmu_get_reg_value(interrupts[shift].mask));

	pmic_config_interface(interrupts[shift].mask_set, 0x1, 0x1, no);

	PMICLOG("[pmic_mask_interrupt] after [0x%x]=0x%x\r\n",
		interrupts[shift].mask_set, upmu_get_reg_value(interrupts[shift].mask));

}

void pmic_unmask_interrupt(PMIC_IRQ_ENUM intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_err(PMICTAG "[pmic_unmask_interrupt] fail intno=%d \r\n", intNo);
		return;
	}

	PMICLOG("[pmic_unmask_interrupt] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n",
		intNo, str, shift, no, interrupts[shift].mask,
		upmu_get_reg_value(interrupts[shift].mask));

	pmic_config_interface(interrupts[shift].mask_clear, 0x1, 0x1, no);

	PMICLOG("[pmic_unmask_interrupt] after [0x%x]=0x%x\r\n",
		interrupts[shift].mask_set, upmu_get_reg_value(interrupts[shift].mask));

}

void pmic_register_interrupt_callback(PMIC_IRQ_ENUM intNo, void (EINT_FUNC_PTR) (void))
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_err(PMICTAG "[pmic_register_interrupt_callback] fail intno=%d\r\n", intNo);
		return;
	} else if (interrupts[shift].interrupts[no].callback != NULL) {
		pr_err(PMICTAG "[pmic_register_interrupt_callback] register callback conflict intno=%d\n",
			intNo);
		return;
	}

	PMICLOG("[pmic_register_interrupt_callback] intno=%d\r\n", intNo);

	interrupts[shift].interrupts[no].callback = EINT_FUNC_PTR;

}

#define ENABLE_ALL_OC_IRQ 0
/* register general oc interrupt handler */
void pmic_register_oc_interrupt_callback(PMIC_IRQ_ENUM intNo)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_err(PMICTAG "[pmic_register_oc_interrupt_callback] fail intno=%d\r\n", intNo);
		return;
	}
	PMICLOG("[pmic_register_oc_interrupt_callback] intno=%d\r\n", intNo);
	interrupts[shift].interrupts[no].oc_callback = oc_int_handler;
}

/* register and enable all oc interrupt */
void register_all_oc_interrupts(void)
{
	PMIC_IRQ_ENUM oc_interrupt = INT_VCORE_OC;

	for (; oc_interrupt <= INT_VXO22_OC; oc_interrupt++) {
		pmic_register_oc_interrupt_callback(oc_interrupt);
		pmic_enable_interrupt(oc_interrupt, 1, "PMIC");
	}
}

static void pmic_int_handler(void)
{
	unsigned char i, j;
	unsigned int ret;

	for (i = 0; i < interrupts_size; i++) {
		unsigned int int_status_val = 0;

		int_status_val = upmu_get_reg_value(interrupts[i].address);
		pr_err(PMICTAG "[PMIC_INT] addr[0x%x]=0x%x\n", interrupts[i].address, int_status_val);

		for (j = 0; j < PMIC_INT_WIDTH; j++) {
			if ((int_status_val) & (1 << j)) {
				PMICLOG("[PMIC_INT][%s]\n", interrupts[i].interrupts[j].name);
				interrupts[i].interrupts[j].times++;
				if (interrupts[i].interrupts[j].callback != NULL)
					interrupts[i].interrupts[j].callback();
				if (interrupts[i].interrupts[j].oc_callback != NULL) {
					interrupts[i].interrupts[j].oc_callback((i * PMIC_INT_WIDTH + j),
						interrupts[i].interrupts[j].name);
				}
				ret = pmic_config_interface(interrupts[i].address, 0x1, 0x1, j);
			}
		}
	}
}

int pmic_thread_kthread(void *x)
{
	unsigned int i;
	unsigned int int_status_val = 0;
#ifdef IPIMB
#else
	unsigned int pwrap_eint_status = 0;
#endif
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	PMICLOG("[PMIC_INT] enter\n");

	pmic_enable_charger_detection_int(0);

	/* Run on a process content */
	while (1) {
		mutex_lock(&pmic_mutex);
#ifdef IPIMB
#else
		pwrap_eint_status = pmic_wrap_eint_status();
		PMICLOG("[PMIC_INT] pwrap_eint_status=0x%x\n", pwrap_eint_status);
#endif

		pmic_int_handler();

#ifdef IPIMB
#else
		pmic_wrap_eint_clr(0x0);
#endif

		for (i = 0; i < interrupts_size; i++) {
			int_status_val = upmu_get_reg_value(interrupts[i].address);
			PMICLOG("[PMIC_INT] after ,int_status_val[0x%x]=0x%x\n",
				interrupts[i].address, int_status_val);
		}

		mdelay(1);

		mutex_unlock(&pmic_mutex);
		pmic_wake_unlock(&pmicThread_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		if (g_pmic_irq != 0)
			enable_irq(g_pmic_irq);
		schedule();
	}

	return 0;
}

void PMIC_EINT_SETTING(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = { 0, 0 };

	/* create pmic irq thread handler*/
	pmic_thread_handle = kthread_create(pmic_thread_kthread, (void *)NULL, "pmic_thread");
	if (IS_ERR(pmic_thread_handle)) {
		pmic_thread_handle = NULL;
		pr_err(PMICTAG "[pmic_thread_kthread] creation fails\n");
	} else {
		PMICLOG("[pmic_thread_kthread] kthread_create Done\n");
	}

	upmu_set_reg_value(MT6335_INT_CON0, 0);
	upmu_set_reg_value(MT6335_INT_CON1, 0);
	upmu_set_reg_value(MT6335_INT_CON2, 0);
	upmu_set_reg_value(MT6335_INT_CON3, 0);
	upmu_set_reg_value(MT6335_INT_CON4, 0);
	upmu_set_reg_value(MT6335_INT_CON5, 0);

	/* For all interrupt events, turn on interrupt module clock */
	pmic_set_register_value(PMIC_RG_INTRP_CK_PDN, 0);

#if 0
	/* For BUCK PREOC related interrupt, please turn on intrp_pre_oc_ck (1MHz) */
	/* This clock is default on */
	pmic_set_register_value(RG_INTRP_PRE_OC_CK_PDN, 0);
#endif

	pmic_register_interrupt_callback(INT_PWRKEY, pwrkey_int_handler);
	pmic_register_interrupt_callback(INT_HOMEKEY, homekey_int_handler);
	pmic_register_interrupt_callback(INT_PWRKEY_R, pwrkey_int_handler_r);
	pmic_register_interrupt_callback(INT_HOMEKEY_R, homekey_int_handler_r);

	pmic_register_interrupt_callback(INT_CHRDET_EDGE, chrdet_int_handler);
	pmic_register_interrupt_callback(INT_BATON_LV, bat_l_int_handler);
	pmic_register_interrupt_callback(INT_BATON_HV, bat_h_int_handler);

	pmic_register_interrupt_callback(INT_FG_CUR_H, fg_cur_h_int_handler);
	pmic_register_interrupt_callback(INT_FG_CUR_L, fg_cur_l_int_handler);

	pmic_enable_interrupt(INT_PWRKEY, 1, "PMIC");
	pmic_enable_interrupt(INT_HOMEKEY, 1, "PMIC");
	pmic_enable_interrupt(INT_PWRKEY_R, 1, "PMIC");
	pmic_enable_interrupt(INT_HOMEKEY_R, 1, "PMIC");

	pmic_enable_interrupt(INT_CHRDET_EDGE, 1, "PMIC");

#if ENABLE_ALL_OC_IRQ
	register_all_oc_interrupts();
#endif

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6335-pmic");
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		g_pmic_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(g_pmic_irq, (irq_handler_t)mt_pmic_eint_irq,
			IRQF_TRIGGER_NONE, "pmic-eint", NULL);
		if (ret > 0)
			pr_err(PMICTAG "EINT IRQ LINENNOT AVAILABLE\n");
		enable_irq_wake(g_pmic_irq);
	} else
		pr_err(PMICTAG "can't find compatible node\n");

	PMICLOG("[CUST_EINT] CUST_EINT_MT_PMIC_MT6335_NUM=%d\n", g_eint_pmic_num);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n", g_cust_eint_mt_pmic_debounce_cn);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n", g_cust_eint_mt_pmic_type);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n", g_cust_eint_mt_pmic_debounce_en);
}

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");
