/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/wakeup_reason.h>
#include <asm/setup.h>

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
#include <mt-plat/mtk_wd_api.h>
#endif
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif
#include <mtk_spm_irq.h>

#include <mtk_sleep_internal.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_spm_resource_req.h>

#include <mt-plat/mtk_ccci_common.h>
#include <mtk_idle_module.h>

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
#include <mt-plat/mtk_usb2jtag.h>
#endif

#ifdef CONFIG_MTK_ICCS_SUPPORT
#include <mtk_hps_internal.h>
#endif

static int spm_dormant_sta;
int spm_ap_mdsrc_req_cnt;

struct wake_status suspend_info[20];
u32 log_wakesta_cnt;
u32 log_wakesta_index;
u8 spm_snapshot_golden_setting;

struct wake_status spm_wakesta; /* record last wakesta */
unsigned int spm_sleep_count;

int __attribute__ ((weak)) mtk_enter_idle_state(int idx)
{
	pr_info("NO %s !!!\n", __func__);
	return -1;
}

int __attribute__ ((weak)) vcorefs_get_curr_ddr(void)
{
	pr_info("NO %s !!!\n", __func__);
	return -1;
}

int  __attribute__ ((weak)) vcorefs_get_curr_vcore(void)
{
	pr_info("NO %s !!!\n", __func__);
	return -1;
}

static u32 suspend_pcm_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_SUSPEND_OPTION
};

static u32 suspend_pcm_flags1 = {
	0
};

static inline void spm_suspend_footprint(enum spm_suspend_step step)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_spm_suspend_val(step |
		(smp_processor_id() << CPU_FOOTPRINT_SHIFT));
#endif
}

static struct pwr_ctrl suspend_ctrl;

struct spm_lp_scen __spm_suspend = {
	.pwrctrl = &suspend_ctrl,
	.wakestatus = &suspend_info[0],
};

static void spm_trigger_wfi_for_sleep(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags))
		spm_dormant_sta = mtk_enter_idle_state(MTK_SUSPEND_MODE);
	else {
		/* need to comment out all cmd in CPU_PM_ENTER case, */
		/* at gic_cpu_pm_notifier() @ drivers/irqchip/irq-gic-v3.c */
		SMC_CALL(ARGS, SPM_ARGS_SUSPEND, 0, 0);
		SMC_CALL(LEGACY_SLEEP, 0, 0, 0);
		SMC_CALL(ARGS, SPM_ARGS_SUSPEND_FINISH, 0, 0);
	}

	if (spm_dormant_sta < 0) {
		aee_sram_printk("spm_dormant_sta %d", spm_dormant_sta);
		pr_info("[SPM] spm_dormant_sta %d", spm_dormant_sta);
	}

	if (is_infra_pdn(pwrctrl->pcm_flags))
		mtk8250_restore_dev();
}

static void spm_suspend_pcm_setup_before_wfi(u32 cpu,
		struct pwr_ctrl *pwrctrl)
{
	unsigned int resource_usage = 0;

#ifdef CONFIG_MTK_ICCS_SUPPORT
	iccs_enter_low_power_state();
#endif

	/* Only get resource usage from user SCP */
	resource_usage = spm_get_resource_usage_by_user(SPM_RESOURCE_USER_SCP);

	spm_suspend_pre_process(pwrctrl);

	spm_dump_world_clk_cntcv();
	spm_set_sysclk_settle();
	__spm_sync_pcm_flags(pwrctrl);
	pwrctrl->timer_val = __spm_get_pcm_timer_val(pwrctrl);

	mt_secure_call(MTK_SIP_KERNEL_SPM_SUSPEND_ARGS, pwrctrl->pcm_flags,
		pwrctrl->pcm_flags1, pwrctrl->timer_val, resource_usage);
}

static void spm_suspend_pcm_setup_after_wfi(u32 cpu, struct pwr_ctrl *pwrctrl)
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

	wr = __spm_output_wake_reason(wakesta, true, "suspend");

	memcpy(&suspend_info[log_wakesta_cnt], wakesta,
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
#if 0 // FIXME
	ddr_status = vcorefs_get_curr_ddr();
	vcore_status = vcorefs_get_curr_vcore();
#endif
	aee_sram_printk("dormant = %d, s_ddr = %d, s_vcore = %d, ",
		  spm_dormant_sta, sleep_ddr_status, sleep_vcore_status);
	pr_info("[SPM] dormant = %d, s_ddr = %d, s_vcore = %d, ",
		  spm_dormant_sta, sleep_ddr_status, sleep_vcore_status);
	aee_sram_printk("ddr = %d, vcore = %d, sleep_count = %d\n",
		  ddr_status, vcore_status, spm_sleep_count);
	pr_info("ddr = %d, vcore = %d, sleep_count = %d\n",
		  ddr_status, vcore_status, spm_sleep_count);
	if (spm_ap_mdsrc_req_cnt != 0) {
		aee_sram_printk("warning: spm_ap_mdsrc_req_cnt = %d, ",
			spm_ap_mdsrc_req_cnt);
		pr_info("[SPM ]warning: spm_ap_mdsrc_req_cnt = %d, ",
			spm_ap_mdsrc_req_cnt);
		aee_sram_printk("r7[ap_mdsrc_req] = 0x%x\n",
			spm_read(SPM_POWER_ON_VAL1) & (1 << 17));
		pr_info("r7[ap_mdsrc_req] = 0x%x\n",
			spm_read(SPM_POWER_ON_VAL1) & (1 << 17));
	}

#if defined(CONFIG_MTK_EIC) || defined(CONFIG_PINCTRL_MTK)
	if (wakesta->r12 & WAKE_SRC_R12_EINT_EVENT_B)
		mt_eint_print_status();
#endif

#ifdef CONFIG_MTK_CCCI_DEVICES
		exec_ccci_kern_func_by_md_id(0, ID_DUMP_MD_SLEEP_MODE,
			NULL, 0);
#endif

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (wakesta->r12 & R12_SCP_SPM_IRQ_B)
		mt_print_scp_ipi_id();
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_ECCCI_DRIVER
	if (wakesta->r12 & WAKE_SRC_R12_CLDMA_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
	if (wakesta->r12 & WAKE_SRC_R12_MD2AP_PEER_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & WAKE_SRC_R12_CCIF0_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & WAKE_SRC_R12_CCIF1_EVENT_B)
		exec_ccci_kern_func_by_md_id(2, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
#endif
#endif
	log_irq_wakeup_reason(mtk_spm_get_irq_0());

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
	SMC_CALL(PWR_CTRL_ARGS, SPM_PWR_CTRL_SUSPEND,
			PW_WAKE_SRC, __spm_suspend.pwrctrl->wake_src);
	spin_unlock_irqrestore(&__spm_lock, flags);

	return 0;
}

/*
 * wakesrc: WAKE_SRC_XXX
 */
u32 spm_get_sleep_wakesrc(void)
{
	return SMC_CALL(GET_PWR_CTRL_ARGS,
			SPM_PWR_CTRL_SUSPEND, PW_WAKE_SRC, 0);
}

bool spm_is_enable_sleep(void)
{
	return true;
}

bool spm_suspend_condition_check(void)
{
	if (is_infra_pdn(suspend_pcm_flags) && !is_cpu_pdn(suspend_pcm_flags)) {
		pr_info("[SLP] CANNOT SLEEP DUE TO INFRA PDN BUT CPU PDN\n");
		return false;
	}

	return true;
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
/* #include <cust_pmic.h> */
#if !defined(DISABLE_DLPT_FEATURE)
/* extern int get_dlpt_imix_spm(void); */
int __attribute__((weak)) get_dlpt_imix_spm(void)
{
	pr_info("NO %s !!!\n", __func__);
	return 0;
}
#endif
#endif
#endif
unsigned int spm_go_to_sleep_ex(unsigned int ex_flag)
{
	unsigned int bRet = 0;

	if ((ex_flag & SPM_SUSPEND_PLAT_SLP_DP) != 0) {
		mtk_idle_enter(IDLE_TYPE_DP, smp_processor_id(),
			MTK_IDLE_OPT_SLEEP_DPIDLE, 0);
		bRet = get_slp_dp_last_wr();
	} else
		bRet = spm_go_to_sleep();

	return bRet;
}
unsigned int spm_go_to_sleep(void)
{
	u32 sec = 2;
	unsigned long flags;
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	struct wd_api *wd_api;
	int wd_ret;
#endif
	static unsigned int last_wr = WR_NONE;
	struct pwr_ctrl *pwrctrl;
	u32 cpu = 0;
	u32 spm_flags = suspend_pcm_flags;
	u32 spm_flags1 = suspend_pcm_flags1;

	spm_suspend_footprint(SPM_SUSPEND_ENTER);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#if !defined(DISABLE_DLPT_FEATURE)
	get_dlpt_imix_spm();
#endif
#endif
#endif

	pwrctrl = __spm_suspend.pwrctrl;

	pwrctrl->wake_src = SMC_CALL(GET_PWR_CTRL_ARGS,
			SPM_PWR_CTRL_SUSPEND, PW_WAKE_SRC, 0);

	__spm_set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__sync_big_buck_ctrl_pcm_flag(&spm_flags1);
	__spm_set_pwrctrl_pcm_flags1(pwrctrl, spm_flags1);


#if SPM_PWAKE_EN
	sec = __spm_get_wake_period(-1, last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

	SMC_CALL(ARGS, SPM_ARGS_PCM_WDT, 1, 30);

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
		wd_api->wd_suspend_notify();
	} else
		pr_info("FAILED TO GET WD API\n");
#endif
	lockdep_off();
	spin_lock_irqsave(&__spm_lock, flags);
	lockdep_on();

	mtk_spm_irq_backup();

	aee_sram_printk("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags),
		  is_infra_pdn(pwrctrl->pcm_flags));
	pr_info("[SPM] sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags),
		  is_infra_pdn(pwrctrl->pcm_flags));

	spm_suspend_pcm_setup_before_wfi(cpu, pwrctrl);

	spm_suspend_footprint(SPM_SUSPEND_ENTER_UART_SLEEP);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (mtk8250_request_to_sleep()) {
		last_wr = WR_UART_BUSY;
		pr_info("Fail to request uart sleep\n");
		goto RESTORE_IRQ;
	}
#endif

	spm_suspend_footprint(SPM_SUSPEND_ENTER_WFI);

	spm_trigger_wfi_for_sleep(pwrctrl);

	spm_suspend_footprint(SPM_SUSPEND_LEAVE_WFI);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	mtk8250_request_to_wakeup();
RESTORE_IRQ:
#endif

	/* record last wakesta */
	__spm_get_wakeup_status(&spm_wakesta);

	spm_suspend_footprint(SPM_SUSPEND_ENTER_UART_AWAKE);

	spm_suspend_pcm_setup_after_wfi(0, pwrctrl);

	/* record last wakesta */
	last_wr = spm_output_wake_reason(&spm_wakesta);
	mtk_spm_irq_restore();

	lockdep_off();
	spin_unlock_irqrestore(&__spm_lock, flags);
	lockdep_on();

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	if (!wd_ret) {
		if (!pwrctrl->wdt_disable)
			wd_api->wd_resume_notify();
		else {
			aee_sram_printk("pwrctrl->wdt_disable %d\n",
				pwrctrl->wdt_disable);
			pr_info("[SPM] pwrctrl->wdt_disable %d\n",
				pwrctrl->wdt_disable);
		}
		wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);
	}
#endif

	SMC_CALL(ARGS, SPM_ARGS_PCM_WDT, 0, 0);

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
	if (usb2jtag_mode())
		mtk_usb2jtag_resume();
#endif
	spm_suspend_footprint(0);

	if (pwrctrl->wakelock_timer_val) {
		aee_sram_printk("#@# %s(%d) calling spm_pm_stay_awake()\n",
			__func__, __LINE__);
		pr_info("[SPM ]#@# %s(%d) calling spm_pm_stay_awake()\n",
			__func__, __LINE__);
		spm_pm_stay_awake(pwrctrl->wakelock_timer_val);
	}

	return last_wr;
}

MODULE_DESCRIPTION("SPM-Sleep Driver v0.1");
