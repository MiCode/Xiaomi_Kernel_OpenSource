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

/* May be removed */
#if 0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wakelock.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif

/* May be used */
#if 0
#include <mt-plat/mtk_rtc.h>
#include <mach/mtk_spm_mtcmos.h>
#endif

#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/sched.h>

#if defined(CONFIG_MTK_SELINUX_AEE_WARNING)
#include <mt-plat/aee.h>
#endif
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_reboot.h>
#if defined(CONFIG_MTK_PMIC_WRAP_HAL)
#include <mach/mtk_pmic_wrap.h>
#endif
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/mtk_pmic_common.h"

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mtk_boot_common.h>
/*#include <mt-plat/mtk_boot.h> TBD*/
/*#include <mt-plat/mtk_gpt.h> TBD*/
#endif

#if defined(CONFIG_MTK_SMART_BATTERY)
#include <mt-plat/battery_common.h>
/*#include <mach/mtk_battery_meter.h> TBD*/
/*#include <mt-plat/battery_meter.h> TBD*/
#endif

/*---IPI Mailbox define---*/
/*#define IPIMB*/

void __attribute__((weak)) arch_reset(char mode, const char *cmd)
{
	pr_info("arch_reset is not ready\n");
}

/* Global variable */
int g_pmic_irq;
unsigned int g_eint_pmic_num = 176;	/* TBD */
unsigned int g_cust_eint_mt_pmic_debounce_cn = 1;
unsigned int g_cust_eint_mt_pmic_type = 4;
unsigned int g_cust_eint_mt_pmic_debounce_en = 1;

/* PMIC extern variable */
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static bool long_pwrkey_press;
static unsigned long timer_pre;
static unsigned long timer_pos;
#define LONG_PWRKEY_PRESS_TIME_UNIT     500     /*500ms */
#define LONG_PWRKEY_PRESS_TIME_US       1000000 /*500ms */
#endif

/* Interrupt Setting */
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
	PMIC_S_INT_GEN(RG_INT_EN_RTC_NSEC),
	PMIC_S_INT_GEN(RG_INT_EN_BIF),
	PMIC_S_INT_GEN(RG_INT_EN_VCDT_HV_DET),
	PMIC_S_INT_GEN(NO_USE),
};

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
	PMIC_S_INT_GEN(RG_INT_EN_VPROC11_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VPROC12_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCORE_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VGPU_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VDRAM1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VDRAM2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMODEM_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VS1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VS2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VPA_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCORE_PREOC),
	PMIC_S_INT_GEN(RG_INT_EN_VA10_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VA12_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VA18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VBIF28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMA1_OC),
};

static struct pmic_interrupt_bit interrupt_status3[] = {
	PMIC_S_INT_GEN(RG_INT_EN_VCAMA2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VXO18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMD1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMD2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCAMIO_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCN18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCN28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VCN33_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VTCXO24_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VEMC_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VFE28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VGP_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VLDO28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VIO18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VIO28_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMC_OC),
};

static struct pmic_interrupt_bit interrupt_status4[] = {
	PMIC_S_INT_GEN(RG_INT_EN_VMCH_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VMIPI_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VRF12_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VRF18_1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VRF18_2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSIM1_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSIM2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VGP2_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_CORE_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_PROC_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_GPU_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VSRAM_MD_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VUFS18_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VUSB33_OC),
	PMIC_S_INT_GEN(RG_INT_EN_VXO22_OC),
	PMIC_S_INT_GEN(NO_USE),
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
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
};

static struct pmic_interrupt_bit interrupt_status6[] = {
	PMIC_S_INT_GEN(RG_INT_EN_AUDIO),
	PMIC_S_INT_GEN(RG_INT_EN_MAD),
	PMIC_S_INT_GEN(RG_INT_EN_EINT_RTC32K_1V8_1),
	PMIC_S_INT_GEN(RG_INT_EN_EINT_AUD_CLK),
	PMIC_S_INT_GEN(RG_INT_EN_EINT_AUD_DAT_MOSI),
	PMIC_S_INT_GEN(RG_INT_EN_EINT_AUD_DAT_MISO),
	PMIC_S_INT_GEN(RG_INT_EN_EINT_VOW_CLK_MISO),
	PMIC_S_INT_GEN(RG_INT_EN_ACCDET),
	PMIC_S_INT_GEN(RG_INT_EN_ACCDET_EINT),
	PMIC_S_INT_GEN(RG_INT_EN_SPI_CMD_ALERT),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
	PMIC_S_INT_GEN(NO_USE),
};

struct pmic_interrupts interrupts[] = {
	PMIC_M_INTS_GEN(MT6355_INT_STATUS0, MT6355_INT_RAW_STATUS0,
			MT6355_INT_CON0, MT6355_INT_MASK_CON0,
			interrupt_status0),
	PMIC_M_INTS_GEN(MT6355_INT_STATUS1, MT6355_INT_RAW_STATUS1,
			MT6355_INT_CON1, MT6355_INT_MASK_CON1,
			interrupt_status1),
	PMIC_M_INTS_GEN(MT6355_INT_STATUS2, MT6355_INT_RAW_STATUS2,
			MT6355_INT_CON2, MT6355_INT_MASK_CON2,
			interrupt_status2),
	PMIC_M_INTS_GEN(MT6355_INT_STATUS3, MT6355_INT_RAW_STATUS3,
			MT6355_INT_CON3, MT6355_INT_MASK_CON3,
			interrupt_status3),
	PMIC_M_INTS_GEN(MT6355_INT_STATUS4, MT6355_INT_RAW_STATUS4,
			MT6355_INT_CON4, MT6355_INT_MASK_CON4,
			interrupt_status4),
	PMIC_M_INTS_GEN(MT6355_INT_STATUS5, MT6355_INT_RAW_STATUS5,
			MT6355_INT_CON5, MT6355_INT_MASK_CON5,
			interrupt_status5),
	PMIC_M_INTS_GEN(MT6355_INT_STATUS6, MT6355_INT_RAW_STATUS6,
			MT6355_INT_CON6, MT6355_INT_MASK_CON6,
			interrupt_status6),
};

int interrupts_size = ARRAY_SIZE(interrupts);

/* PWRKEY Int Handler */
void pwrkey_int_handler(void)
{
	PMICLOG("[%s] Press pwrkey %d\n"
		, __func__
		, pmic_get_register_value(PMIC_PWRKEY_DEB));
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT)
		timer_pre = sched_clock();
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	kpd_pwrkey_pmic_handler(0x1);
#endif
}

void pwrkey_int_handler_r(void)
{
	PMICLOG("[%s] Release pwrkey %d\n"
		, __func__
		, pmic_get_register_value(PMIC_PWRKEY_DEB));
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT &&
		timer_pre != 0) {
		timer_pos = sched_clock();
		if (timer_pos - timer_pre >=
		    LONG_PWRKEY_PRESS_TIME_UNIT * LONG_PWRKEY_PRESS_TIME_US)
			long_pwrkey_press = true;
		PMICLOG("timer_pos = %ld, timer_pre = %ld\r\n"
			, timer_pos
			, timer_pre);
		PMICLOG("timer_pos-timer_pre = %ld, long_pwrkey_press = %d\r\n"
			, timer_pos - timer_pre
			, long_pwrkey_press);
		if (long_pwrkey_press) {	/*500ms */
			PMICLOG("Power Key Pressed during KPOC, reboot OS\r\n"
				);
			arch_reset(0, NULL);
		}
	}
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	kpd_pwrkey_pmic_handler(0x0);
#endif
}

/* Homekey Int Handler */
void homekey_int_handler(void)
{
	PMICLOG("[%s] Press homekey %d\n"
		, __func__
		, pmic_get_register_value(PMIC_HOMEKEY_DEB));
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	kpd_pmic_rstkey_handler(0x1);
#endif
}

void homekey_int_handler_r(void)
{
	PMICLOG("[%s] Release homekey %d\n"
		, __func__
		, pmic_get_register_value(PMIC_HOMEKEY_DEB));
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	kpd_pmic_rstkey_handler(0x0);
#endif
}

/* Chrdet Int Handler */
#if defined(CONFIG_MTK_SMART_BATTERY)
#if (CONFIG_MTK_GAUGE_VERSION != 30)
void chrdet_int_handler(void)
{
	PMICLOG("[%s]CHRDET status = %d....\n"
		, __func__
		, pmic_get_register_value(PMIC_RGS_CHRDET));
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (!upmu_get_rgs_chrdet()) {
		int boot_mode = 0;

		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		|| boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			PMICLOG("[%s] Unplug Charger/USB\n", __func__);
			mt_power_off();
		}
	}
#endif
	/*pmic_set_register_value(PMIC_RG_USBDL_RST, 1); MT6355 TBD by Jeter*/
#if defined(CONFIG_MTK_SMART_BATTERY)
	do_chrdet_int_task();
#endif
}
#endif
#endif

/* VPA OC Int Handler is not used in MT6355 */

/* Ldo OC Int Handler is not used in MT6355*/

/* May be removed(TBD) */
/* Auxadc Int Handler */
void auxadc_imp_int_handler_r(void)
{
	PMICLOG("%s =%d\n"
		, __func__
		, pmic_get_register_value(PMIC_AUXADC_ADC_OUT_IMP));
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
	case INT_VPA_OC:
		/* keep OC interrupt and keep tracking */
		pr_notice(PMICTAG "[PMIC_INT] PMIC OC: %s\n", int_name);
		break;
	default:
		/* issue AEE exception and disable OC interrupt */
		/* TBD: dump_exception_reg */
		snprintf(oc_str, 30, "PMIC OC:%s", int_name);
#if defined(CONFIG_MTK_SELINUX_AEE_WARNING)
		aee_kernel_warning(oc_str
			, "\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s"
			, int_name);
#endif
		pmic_enable_interrupt(intNo, 0, "PMIC");
		pr_notice(PMICTAG "[PMIC_INT] disable OC interrupt: %s\n"
			  , int_name);
		break;
	}
}

/*
 * PMIC Interrupt service
 */
static DEFINE_MUTEX(pmic_mutex);
struct task_struct *pmic_thread_handle;

struct wakeup_source pmicThread_lock;

void wake_up_pmic(void)
{
	PMICLOG("[%s]\n", __func__);
	if (pmic_thread_handle != NULL) {
		pmic_wake_lock(&pmicThread_lock);
		wake_up_process(pmic_thread_handle);
	} else {
		pr_info(PMICTAG "[%s] pmic_thread_handle not ready\n", __func__)
			;
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

void pmic_enable_interrupt(enum PMIC_IRQ_ENUM intNo, unsigned int en, char *str)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_info(PMICTAG "[%s] fail intno=%d\r\n", __func__, intNo);
		return;
	} else if (en != 0 && en != 1) {
		pr_info(PMICTAG "[%s] error argument en=%d\n", __func__, en);
		return;
	}
	PMICLOG("[%s] intno=%d en=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n"
		, __func__
		, intNo, en, str, shift, no, interrupts[shift].en
		, upmu_get_reg_value(interrupts[shift].en));

	if (en == 1)
		pmic_config_interface(interrupts[shift].set, 0x1, 0x1, no);
	else if (en == 0)
		pmic_config_interface(interrupts[shift].clear, 0x1, 0x1, no);

	PMICLOG("[%s] after [0x%x]=0x%x\r\n"
		, __func__
		, interrupts[shift].en
		, upmu_get_reg_value(interrupts[shift].en));
}

void pmic_mask_interrupt(enum PMIC_IRQ_ENUM intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_info(PMICTAG "[%s] fail intno=%d\r\n", __func__, intNo);
		return;
	}
	PMICLOG("[%s] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n"
		, __func__
		, intNo, str, shift, no, interrupts[shift].mask,
		upmu_get_reg_value(interrupts[shift].mask));

	pmic_config_interface(interrupts[shift].mask_set, 0x1, 0x1, no);

	PMICLOG("[%s] after [0x%x]=0x%x\r\n"
		, __func__
		, interrupts[shift].mask_set
		, upmu_get_reg_value(interrupts[shift].mask));
}

void pmic_unmask_interrupt(enum PMIC_IRQ_ENUM intNo, char *str)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_info(PMICTAG "[%s] fail intno=%d\r\n", __func__, intNo);
		return;
	}
	PMICLOG("[%s] intno=%d str=%s shf=%d no=%d [0x%x]=0x%x\r\n"
		, __func__
		, intNo, str, shift, no, interrupts[shift].mask,
		upmu_get_reg_value(interrupts[shift].mask));

	pmic_config_interface(interrupts[shift].mask_clear, 0x1, 0x1, no);

	PMICLOG("[%s] after [0x%x]=0x%x\r\n"
		, __func__
		, interrupts[shift].mask_set
		, upmu_get_reg_value(interrupts[shift].mask));
}

void pmic_register_interrupt_callback(enum PMIC_IRQ_ENUM intNo
				      , void (EINT_FUNC_PTR) (void))
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_info(PMICTAG "[%s] fail intno=%d\r\n", __func__, intNo);
		return;
	} else if (interrupts[shift].interrupts[no].callback != NULL) {
		pr_info(PMICTAG "[%s] register callback conflict intno=%d\n"
			, __func__, intNo);
		return;
	}
	interrupts[shift].interrupts[no].callback = EINT_FUNC_PTR;

	PMICLOG("[%s] intno=%d\r\n", __func__, intNo);
}

/*#define ENABLE_ALL_OC_IRQ 0*/
/* register general oc interrupt handler */
void pmic_register_oc_interrupt_callback(enum PMIC_IRQ_ENUM intNo)
{
	unsigned int shift, no;

	shift = intNo / PMIC_INT_WIDTH;
	no = intNo % PMIC_INT_WIDTH;

	if (shift >= interrupts_size) {
		pr_info(PMICTAG "[%s] fail intno=%d\r\n", __func__, intNo);
		return;
	}
	interrupts[shift].interrupts[no].oc_callback = oc_int_handler;

	PMICLOG("[%s] intno=%d\r\n", __func__, intNo);
}

/* register and enable all oc interrupt */
void register_all_oc_interrupts(void)
{
	enum PMIC_IRQ_ENUM oc_interrupt = INT_VCORE_OC;

	for (; oc_interrupt <= INT_VXO22_OC; oc_interrupt++) {
		/* ignore pre_oc */
		if (oc_interrupt == INT_VCORE_PREOC)
			continue;
		/* ignore SIM card oc */
		if (oc_interrupt == INT_VSIM1_OC ||
		    oc_interrupt == INT_VSIM2_OC)
			continue;
		pmic_register_oc_interrupt_callback(oc_interrupt);
			if (oc_interrupt == INT_VCAMA1_OC ||
			    oc_interrupt == INT_VCAMA2_OC ||
			    oc_interrupt == INT_VCAMD1_OC ||
			    oc_interrupt == INT_VCAMD2_OC)
				pmic_enable_interrupt(oc_interrupt, 0, "PMIC");
			else
				pmic_enable_interrupt(oc_interrupt, 1, "PMIC");
	}
}

static void pmic_int_handler(void)
{
	unsigned char i, j;

	for (i = 0; i < interrupts_size; i++) {
		unsigned int int_status_val = 0;

		int_status_val = upmu_get_reg_value(interrupts[i].address);
		if (int_status_val) {
			pr_info(PMICTAG "[PMIC_INT] addr[0x%x]=0x%x\n"
				, interrupts[i].address, int_status_val);
			upmu_set_reg_value(interrupts[i].address
					   , int_status_val);
		}

		for (j = 0; j < PMIC_INT_WIDTH; j++) {
			if ((int_status_val) & (1 << j)) {
				PMICLOG("[PMIC_INT][%s]\n"
					, interrupts[i].interrupts[j].name);
				interrupts[i].interrupts[j].times++;
				if (interrupts[i].interrupts[j].callback !=
						NULL)
					interrupts[i].interrupts[j].callback();
				if (interrupts[i].interrupts[j].oc_callback !=
						NULL) {
					interrupts[i].interrupts[j].oc_callback(
					(i * PMIC_INT_WIDTH + j),
					interrupts[i].interrupts[j].name);
				}
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

	/*pmic_enable_charger_detection_int(0);*/

	/* Run on a process content */
	while (1) {
		set_current_state(TASK_RUNNING);
		mutex_lock(&pmic_mutex);
#ifdef IPIMB
#else
#if defined(CONFIG_MTK_PMIC_WRAP_HAL)
		pwrap_eint_status = pmic_wrap_eint_status();
#endif
		PMICLOG("[PMIC_INT] pwrap_eint_status=0x%x\n"
			, pwrap_eint_status);
#endif

		pmic_int_handler();

#ifdef IPIMB
#else
#if defined(CONFIG_MTK_PMIC_WRAP_HAL)
		pmic_wrap_eint_clr(0x0);
#endif
#endif

		for (i = 0; i < interrupts_size; i++) {
			int_status_val =
				upmu_get_reg_value(interrupts[i].address);
			PMICLOG("[PMIC_INT] after ,int_status_val[0x%x]=0x%x\n",
				interrupts[i].address, int_status_val);
		}

		/*mdelay(1); TBD*/

		mutex_unlock(&pmic_mutex);
		pmic_wake_unlock(&pmicThread_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		enable_irq(g_pmic_irq);
		schedule();
	}

	return 0;
}

static void irq_thread_init(void)
{
	/* create pmic irq thread handler*/
	pmic_thread_handle = kthread_create(pmic_thread_kthread,
					    (void *)NULL, "pmic_thread");
	if (IS_ERR(pmic_thread_handle)) {
		pmic_thread_handle = NULL;
		pr_info(PMICTAG "[pmic_thread_kthread] creation fails\n");
	} else {
		PMICLOG("[pmic_thread_kthread] kthread_create Done\n");
	}
}

static void register_irq_handlers(void)
{
	pmic_register_interrupt_callback(INT_PWRKEY, pwrkey_int_handler);
	pmic_register_interrupt_callback(INT_HOMEKEY, homekey_int_handler);
	pmic_register_interrupt_callback(INT_PWRKEY_R, pwrkey_int_handler_r);
	pmic_register_interrupt_callback(INT_HOMEKEY_R, homekey_int_handler_r);
#if defined(CONFIG_MTK_SMART_BATTERY)
#if (CONFIG_MTK_GAUGE_VERSION != 30)
	pmic_register_interrupt_callback(INT_CHRDET_EDGE, chrdet_int_handler);
#endif
#endif

	pmic_register_interrupt_callback(INT_BAT_L, bat_l_int_handler);
	pmic_register_interrupt_callback(INT_BAT_H, bat_h_int_handler);
	pmic_register_interrupt_callback(INT_FG_CUR_H, fg_cur_h_int_handler);
	pmic_register_interrupt_callback(INT_FG_CUR_L, fg_cur_l_int_handler);

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

#if defined(CONFIG_MTK_SMART_BATTERY)
#if (CONFIG_MTK_GAUGE_VERSION != 30)
	pmic_enable_interrupt(INT_CHRDET_EDGE, 1, "PMIC");
#endif
#endif
}

void PMIC_EINT_SETTING(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	/* create pmic irq thread handler*/
	irq_thread_init();

	/* Disable all interrupt for initializing */
	upmu_set_reg_value(MT6355_INT_CON0, 0);
	upmu_set_reg_value(MT6355_INT_CON1, 0);
	upmu_set_reg_value(MT6355_INT_CON2, 0);
	upmu_set_reg_value(MT6355_INT_CON3, 0);
	upmu_set_reg_value(MT6355_INT_CON4, 0);
	upmu_set_reg_value(MT6355_INT_CON5, 0);
	upmu_set_reg_value(MT6355_INT_CON6, 0);

	/* For all interrupt events, turn on interrupt module clock */
	pmic_set_register_value(PMIC_RG_INTRP_CK_PDN, 0);
	/* For BUCK PREOC rel. interrupt, pls turn on intrp_pre_oc_ck (1MHz)*/
	/* This clock is default on */
	/*pmic_set_register_value(RG_INTRP_PRE_OC_CK_PDN, 0); TBD*/

	register_irq_handlers();
	enable_pmic_irqs();

#if defined(CONFIG_MACH_MT6758)
	node = of_find_compatible_node(NULL, NULL, "mediatek, mt6355_pmic");
#else
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6355-pmic");
#endif
	if (node) {
		/* no debounce setting */
		g_pmic_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(g_pmic_irq, (irq_handler_t)mt_pmic_eint_irq,
			IRQF_TRIGGER_NONE, "pmic-eint", NULL);
		if (ret > 0)
			pr_info(PMICTAG "EINT IRQ NOT AVAILABLE\n");
		enable_irq_wake(g_pmic_irq);
	} else
		pr_info(PMICTAG "can't find compatible node\n");

	PMICLOG("[CUST_EINT] CUST_EINT_MT_PMIC_MT6355_NUM=%d\n"
		, g_eint_pmic_num);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_CN=%d\n"
		, g_cust_eint_mt_pmic_debounce_cn);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_TYPE=%d\n"
		, g_cust_eint_mt_pmic_type);
	PMICLOG("[CUST_EINT] CUST_EINT_PMIC_DEBOUNCE_EN=%d\n"
		, g_cust_eint_mt_pmic_debounce_en);
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");
