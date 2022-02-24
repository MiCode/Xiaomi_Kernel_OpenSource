// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/of_fdt.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <mt-plat/mtk_secure_api.h>

#include <mtk_spm_early_porting.h>

#ifdef CONFIG_ARM64
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <linux/irqchip/mtk-gic-extend.h>
#endif
#endif
#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif
/* #include <mach/mtk_clkmgr.h> */
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_cpuidle.h>
#endif
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
#include <mach/wd_api.h>
#endif
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif
#include <mtk_spm_misc.h>
#include <mtk_spm_sleep.h>
#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#include <mtk_spm_internal.h>
#if !defined(CONFIG_FPGA_EARLY_PORTING)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_spm_pmic_wrap.h>
#include <pmic_api_buck.h>
#endif
#endif /* CONFIG_FPGA_EARLY_PORTING */

#ifdef CONFIG_MTK_CCCI_DEVICES
#include <mt-plat/mtk_ccci_common.h>
#endif

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
#include <mt-plat/mtk_usb2jtag.h>
#endif

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_define.h>
#include <v1/sspm_timesync.h>
#endif

#ifdef CONFIG_MTK_ICCS_SUPPORT
#include <mtk_hps_internal.h>
#endif

#include <mtk_spm_sleep_internal.h>

static int spm_dormant_sta;
int spm_ap_mdsrc_req_cnt;

struct wake_status suspend_info[20];
u32 log_wakesta_cnt;
u32 log_wakesta_index;
u8 spm_snapshot_golden_setting;

struct wake_status spm_wakesta; /* record last wakesta */
static unsigned int spm_sleep_count;

int __attribute__ ((weak)) mtk_enter_idle_state(int idx)
{
	spm_crit("[name:spm&]NO %s !!!\n", __func__);
	return -1;
}

void __attribute__((weak)) mt_cirq_clone_gic(void)
{
	spm_crit("[name:spm&]NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_cirq_enable(void)
{
	spm_crit("[name:spm&]NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_cirq_flush(void)
{
	spm_crit("[name:spm&]NO %s !!!\n", __func__);
}

void __attribute__((weak)) mt_cirq_disable(void)
{
	spm_crit("[name:spm&]NO %s !!!\n", __func__);
}

int  __attribute__ ((weak)) vcorefs_get_curr_vcore(void)
{
	spm_crit("[name:spm&]NO %s !!!\n", __func__);
	return -1;
}

void __attribute__ ((weak)) mtk8250_backup_dev(void)
{
	//pr_debug("NO %s !!!\n", __func__);
}

void __attribute__ ((weak)) mtk8250_restore_dev(void)
{
	//pr_debug("NO %s !!!\n", __func__);
}

int __attribute__ ((weak)) mtk8250_request_to_wakeup(void)
{
	//pr_debug("NO %s !!!\n", __func__);
	return 0;
}

int __attribute__ ((weak)) mtk8250_request_to_sleep(void)
{
	//pr_debug("NO %s !!!\n", __func__);
	return 0;
}

static inline void spm_suspend_footprint(enum spm_suspend_step step)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_spm_suspend_val(aee_rr_curr_spm_suspend_val() |
				   step |
				   (smp_processor_id() <<
				    CPU_FOOTPRINT_SHIFT));
#endif
}

static inline void spm_suspend_reset_footprint(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_spm_suspend_val(0);
#endif
}

static struct pwr_ctrl suspend_ctrl;

struct spm_lp_scen __spm_suspend = {
	.pwrctrl = &suspend_ctrl,
	.wakestatus = &suspend_info[0],
};

static void spm_trigger_wfi_for_sleep(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
		spm_dormant_sta = mtk_enter_idle_state(MTK_SUSPEND_MODE);
#else
		;
#endif
	} else {
		/* need to comment out all cmd in CPU_PM_ENTER case, */
		/* at gic_cpu_pm_notifier() @ drivers/irqchip/irq-gic-v3.c */
		SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS, SPM_ARGS_SUSPEND, 0, 0);
		SMC_CALL(MTK_SIP_KERNEL_SPM_LEGACY_SLEEP, 0, 0, 0);
		SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS,
			 SPM_ARGS_SUSPEND_FINISH,
			 0, 0);
	}

	if (spm_dormant_sta < 0)
		spm_crit2("spm_dormant_sta %d", spm_dormant_sta);

	if (is_infra_pdn(pwrctrl->pcm_flags))
#if defined(CONFIG_MACH_MT6739)
		mtk_uart_restore();
#else
		mtk8250_restore_dev();
#endif
}

static void spm_suspend_pcm_setup_before_wfi(u32 cpu,
		struct pwr_ctrl *pwrctrl)
{
#ifdef CONFIG_MTK_ICCS_SUPPORT
	iccs_enter_low_power_state();
#endif

	spm_suspend_pre_process(pwrctrl);

	spm_set_sysclk_settle();
	__spm_sync_pcm_flags(pwrctrl);
	pwrctrl->timer_val = __spm_get_pcm_timer_val(pwrctrl);

	SMC_CALL(MTK_SIP_KERNEL_SPM_SUSPEND_ARGS,
		 pwrctrl->pcm_flags,
		 pwrctrl->pcm_flags1,
		 pwrctrl->timer_val);
}

static void spm_suspend_pcm_setup_after_wfi(u32 cpu,
					    struct pwr_ctrl *pwrctrl)
{
	spm_suspend_post_process(pwrctrl);
}

int sleep_ddr_status;
int sleep_vcore_status;

static unsigned int spm_output_wake_reason(struct wake_status *wakesta)
{
	unsigned int wr;
	int ddr_status = 0;
	int vcore_status = 0;

	if (spm_sleep_count >= 0xfffffff0)
		spm_sleep_count = 0;
	else
		spm_sleep_count++;

	wr = __spm_output_wake_reason(wakesta, NULL, true, "suspend");

	memcpy(&suspend_info[log_wakesta_cnt],
	       wakesta,
	       sizeof(struct wake_status));
	suspend_info[log_wakesta_cnt].log_index = log_wakesta_index;

	if (log_wakesta_cnt >= 10) {
		log_wakesta_cnt = 0;
		spm_snapshot_golden_setting = 0;
	} else {
		log_wakesta_cnt++;
		log_wakesta_index++;
	}

	if (log_wakesta_index >= 0xFFFFFFF0)
		log_wakesta_index = 0;

	ddr_status = vcorefs_get_curr_ddr();

	/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	vcore_status = vcorefs_get_curr_vcore();
#endif

	spm_crit2(
	"dormant = %d, s_ddr = %d, s_vcore = %d, ddr = %d, vcore = %d, sleep_count = %d\n",
		  spm_dormant_sta, sleep_ddr_status, sleep_vcore_status,
		  ddr_status, vcore_status, spm_sleep_count);
	if (spm_ap_mdsrc_req_cnt != 0)
		spm_crit2(
		"warning: spm_ap_mdsrc_req_cnt = %d, r7[ap_mdsrc_req] = 0x%x\n",
			  spm_ap_mdsrc_req_cnt,
			  spm_read(SPM_POWER_ON_VAL1) & (1 << 17));

#if defined(CONFIG_MTK_EIC) || defined(CONFIG_PINCTRL_MTK_COMMON)
	if (wakesta->r12 & WAKE_SRC_R12_EINT_EVENT_B)
		mt_eint_print_status();
#endif

#ifdef CONFIG_MTK_CCCI_DEVICES
	/* if (wakesta->r13 & 0x18) { */
		/* spm_crit2("dump ID_DUMP_MD_SLEEP_MODE"); */
		exec_ccci_kern_func_by_md_id(0, ID_DUMP_MD_SLEEP_MODE, NULL, 0);
	/* } */
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_ECCCI_DRIVER
	if (wakesta->r12 & WAKE_SRC_R12_CLDMA_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
	if (wakesta->r12 & WAKE_SRC_R12_MD2AP_PEER_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
	if (wakesta->r12 & WAKE_SRC_R12_CCIF0_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
	if (wakesta->r12 & WAKE_SRC_R12_CCIF1_EVENT_B)
		exec_ccci_kern_func_by_md_id(2, ID_GET_MD_WAKEUP_SRC, NULL, 0);
#endif
#endif
	return wr;
}

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	unsigned long flags;

	if (spm_is_wakesrc_invalid(wakesrc))
		return -EINVAL;

	spin_lock_irqsave(&__spm_lock, flags);
	if (enable) {
		if (replace)
			__spm_suspend.pwrctrl->wake_src = wakesrc;
		else
			__spm_suspend.pwrctrl->wake_src |= wakesrc;
	} else {
		if (replace)
			__spm_suspend.pwrctrl->wake_src = 0;
		else
			__spm_suspend.pwrctrl->wake_src &= ~wakesrc;
	}
	SMC_CALL(MTK_SIP_KERNEL_SPM_PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND,
			PW_WAKE_SRC, __spm_suspend.pwrctrl->wake_src);
	spin_unlock_irqrestore(&__spm_lock, flags);

	return 0;
}

/*
 * wakesrc: WAKE_SRC_XXX
 */
u32 spm_get_sleep_wakesrc(void)
{
	return SMC_CALL(MTK_SIP_KERNEL_SPM_GET_PWR_CTRL_ARGS,
			SPM_PWR_CTRL_SUSPEND, PW_WAKE_SRC, 0);
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
/* #include <cust_pmic.h> */
#if !defined(DISABLE_DLPT_FEATURE)
/* extern int get_dlpt_imix_spm(void); */
int __attribute__((weak)) get_dlpt_imix_spm(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
	return 0;
}
#endif
#endif
#endif

unsigned int spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
	u32 sec = 2;
	unsigned long flags;
#if defined(CONFIG_MTK_GIC_V3_EXT)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	struct mtk_irq_mask mask;
#endif
#endif
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	struct wd_api *wd_api;
	int wd_ret;
#endif
	static unsigned int last_wr = WR_NONE;
	struct pwr_ctrl *pwrctrl;
	u32 cpu = 0;
	u32 spm_flags1 = spm_data;

	spm_suspend_footprint(SPM_SUSPEND_ENTER);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#if !defined(DISABLE_DLPT_FEATURE)
	get_dlpt_imix_spm();
#endif
#endif
#endif

	pwrctrl = __spm_suspend.pwrctrl;
	pwrctrl->wake_src = SMC_CALL(MTK_SIP_KERNEL_SPM_GET_PWR_CTRL_ARGS,
			SPM_PWR_CTRL_SUSPEND, PW_WAKE_SRC, 0);

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__sync_big_buck_ctrl_pcm_flag(&spm_flags1);
	set_pwrctrl_pcm_flags1(pwrctrl, spm_flags1);


#if SPM_PWAKE_EN
	sec = _spm_get_wake_period(-1, last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

	SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS, SPM_ARGS_PCM_WDT, 1, 30);

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
		wd_api->wd_suspend_notify();
	} else
		spm_crit2("FAILED TO GET WD API\n");
#endif

	lockdep_off();
	spin_lock_irqsave(&__spm_lock, flags);

#if defined(CONFIG_MTK_GIC_V3_EXT)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep_ex(SPM_IRQ0_ID);
#endif
#if defined(CONFIG_MACH_MT6763)
	unmask_edge_trig_irqs_for_cirq();
#endif
#endif

#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif

	spm_crit2("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags),
		  is_infra_pdn(pwrctrl->pcm_flags));

	spm_suspend_pcm_setup_before_wfi(cpu, pwrctrl);

	spm_suspend_footprint(SPM_SUSPEND_ENTER_UART_SLEEP);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MACH_MT6739)
	if (request_uart_to_sleep()) {
#else
	if (mtk8250_request_to_sleep()) {
#endif
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}
#endif

	spm_suspend_footprint(SPM_SUSPEND_ENTER_WFI);

	spm_trigger_wfi_for_sleep(pwrctrl);

	spm_suspend_footprint(SPM_SUSPEND_LEAVE_WFI);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MACH_MT6739)
	request_uart_to_wakeup();
#else
	mtk8250_request_to_wakeup();
#endif
RESTORE_IRQ:
#endif

	/* record last wakesta */
	__spm_get_wakeup_status(&spm_wakesta);

	spm_suspend_footprint(SPM_SUSPEND_ENTER_UART_AWAKE);

	spm_suspend_pcm_setup_after_wfi(0, pwrctrl);

	/* record last wakesta */
	last_wr = spm_output_wake_reason(&spm_wakesta);
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif

#if defined(CONFIG_MTK_GIC_V3_EXT)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	mt_irq_mask_restore(&mask);
#endif
#endif

	spin_unlock_irqrestore(&__spm_lock, flags);
	lockdep_on();

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	if (!wd_ret) {
		if (!pwrctrl->wdt_disable)
			wd_api->wd_resume_notify();
		else
			spm_crit2("pwrctrl->wdt_disable %d\n",
				  pwrctrl->wdt_disable);
		wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);
	}
#endif

	SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS, SPM_ARGS_PCM_WDT, 0, 0);

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
	if (usb2jtag_mode())
		mtk_usb2jtag_resume();
#endif
	spm_suspend_reset_footprint();

	if (last_wr == WR_PCM_ASSERT)
		rekick_vcorefs_scenario();

/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	if (pwrctrl->wakelock_timer_val) {
		spm_crit2("#@# %s(%d) calling spm_pm_stay_awake()\n",
			  __func__, __LINE__);
		spm_pm_stay_awake(pwrctrl->wakelock_timer_val);
	}
#endif

	return last_wr;
}

ssize_t get_spm_sleep_count(char *ToUserBuf, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz, "0x%x\n", spm_sleep_count);

	return (bLen > sz) ? sz : bLen;
}

ssize_t get_spm_last_wakeup_src(char *ToUserBuf, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz, "0x%x\n", spm_wakesta.r12);

	return (bLen > sz) ? sz : bLen;
}

void spm_output_sleep_option(void)
{
	spm_notice("PWAKE_EN:%d, PCMWDT_EN:%d, BYPASS_SYSPWREQ:%d\n",
		   SPM_PWAKE_EN, SPM_PCMWDT_EN, SPM_BYPASS_SYSPWREQ);
}

/* record last wakesta */
u32 spm_get_last_wakeup_src(void)
{
	return spm_wakesta.r12;
}
EXPORT_SYMBOL(spm_get_last_wakeup_src);

u32 spm_get_last_wakeup_misc(void)
{
	return spm_wakesta.wake_misc;
}
EXPORT_SYMBOL(spm_get_last_wakeup_misc);
MODULE_DESCRIPTION("SPM-Sleep Driver v0.1");
