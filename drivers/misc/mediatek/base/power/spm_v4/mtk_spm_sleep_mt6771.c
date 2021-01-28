/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/of_fdt.h>
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
/* #include <mtk_dramc.h> */

#include <mtk_spm_internal.h>
#include <mtk_spm_pmic_wrap.h>
#include "pmic_api_buck.h"
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_spm_vcore_dvfs.h>
#endif

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

/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_power_gs_api.h>
#endif

#ifdef CONFIG_MTK_ICCS_SUPPORT
#include <mtk_hps_internal.h>
#endif

#include <mtk_spm_sleep_internal.h>

void spm_set_sysclk_settle(void)
{
	u32 settle;

	/* SYSCLK settle = MD SYSCLK settle but set it again for MD PDN */
	settle = spm_read(SPM_CLK_SETTLE);

	/* md_settle is keyword for suspend status */
	spm_crit2("md_settle = %u, settle = %u\n", SPM_SYSCLK_SETTLE, settle);
}

#if SPM_PMIC_DEBUG
static void spm_dump_pmic_reg(void)
{
	unsigned int pmic_reg[] = {PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_ADDR};
	unsigned int val = 0;
	unsigned int ret = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(pmic_reg); i++) {
		ret = pmic_read_interface_nolock(pmic_reg[i], &val, 0xffff, 0);
		spm_crit2("#@# %s(%d) pmic reg(0x%x) = 0x%x\n",
			  __func__, __LINE__, pmic_reg[i], val);
	}
}
#endif /* SPM_PMIC_DEBUG */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
static int mt_power_gs_dump_suspend_count = 2;
#endif
#endif

void spm_suspend_pre_process(struct pwr_ctrl *pwrctrl)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

#ifdef SSPM_TIMESYNC_SUPPORT
	sspm_timesync_ts_get(&spm_d.u.suspend.sys_timestamp_h,
			     &spm_d.u.suspend.sys_timestamp_l);
	sspm_timesync_clk_get(&spm_d.u.suspend.sys_src_clk_h,
			      &spm_d.u.suspend.sys_src_clk_l);
#endif

	spm_opt |= spm_for_gps_flag ?  SPM_OPT_GPS_STAT     : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command(SPM_SUSPEND, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
#if SPM_PMIC_DEBUG
	spm_dump_pmic_reg();
#endif /* SPM_PMIC_DEBUG */
#else
#if SPM_PMIC_EN
	/* VCORE 0.55V */
	pmic_config_interface(PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_ADDR, 0x8,
			PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_MASK,
			PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_SHIFT);
#endif /* SPM_PMIC_EN */
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
/* TODO: fix */
#if !defined(SPM_K414_EARLY_PORTING)
	if (slp_dump_golden_setting || --mt_power_gs_dump_suspend_count >= 0)
		mt_power_gs_dump_suspend(slp_dump_golden_setting_type);
#endif
#endif

	/* dvfsrc_md_scenario_update(1); */

#if SPM_PMIC_DEBUG
	spm_dump_pmic_reg();
#endif /* SPM_PMIC_DEBUG */
}

void spm_suspend_post_process(struct pwr_ctrl *pwrctrl)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int ret;
	struct spm_data spm_d;

	/* dvfsrc_md_scenario_update(0); */

#if SPM_PMIC_DEBUG
	spm_dump_pmic_reg();
#endif /* SPM_PMIC_DEBUG */

	memset(&spm_d, 0, sizeof(struct spm_data));

#ifdef SSPM_TIMESYNC_SUPPORT
	sspm_timesync_ts_get(&spm_d.u.suspend.sys_timestamp_h,
			     &spm_d.u.suspend.sys_timestamp_l);
	sspm_timesync_clk_get(&spm_d.u.suspend.sys_src_clk_h,
			      &spm_d.u.suspend.sys_src_clk_l);
#endif

	ret = spm_to_sspm_command(SPM_RESUME, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
#else
	/* dvfsrc_md_scenario_update(0); */

#if SPM_PMIC_DEBUG
	spm_dump_pmic_reg();
#endif /* SPM_PMIC_DEBUG */

#if SPM_PMIC_EN
	/* VCORE 0.6V */
	pmic_config_interface(PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_ADDR, 0x10,
			PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_MASK,
			PMIC_RG_BUCK_VCORE_VOSEL_SLEEP_SHIFT);

#endif /* SPM_PMIC_EN */

#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
}

bool spm_is_md_sleep(void)
{
	return !((spm_read(PCM_REG13_DATA) & R13_MD1_SRCCLKENA) |
		 (spm_read(PCM_REG13_DATA) & R13_MD2_SRCCLKENA));
}
EXPORT_SYMBOL(spm_is_md_sleep);

bool spm_is_md1_sleep(void)
{
	return !(spm_read(PCM_REG13_DATA) & R13_MD1_SRCCLKENA);
}
EXPORT_SYMBOL(spm_is_md1_sleep);

bool spm_is_md2_sleep(void)
{
	return !(spm_read(PCM_REG13_DATA) & R13_MD2_SRCCLKENA);
}
EXPORT_SYMBOL(spm_is_md2_sleep);

bool spm_is_conn_sleep(void)
{
	return !(spm_read(PCM_REG13_DATA) & R13_CONN_SRCCLKENA);
}
EXPORT_SYMBOL(spm_is_conn_sleep);

void spm_ap_mdsrc_req(u8 set)
{
	unsigned long flags;
	u32 i = 0;
	u32 md_sleep = 0;

	if (set) {
		spin_lock_irqsave(&__spm_lock, flags);

		if (spm_ap_mdsrc_req_cnt < 0) {
			spm_crit2(
			"warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n",
			set,
			spm_ap_mdsrc_req_cnt);
			spin_unlock_irqrestore(&__spm_lock, flags);
		} else {
			spm_ap_mdsrc_req_cnt++;

			SMC_CALL(MTK_SIP_KERNEL_SPM_AP_MDSRC_REQ, 1, 0, 0);

			spin_unlock_irqrestore(&__spm_lock, flags);

			/* if md_apsrc_req = 1'b0, */
			/* wait 26M settling time (3ms) */
			if ((spm_read(PCM_REG13_DATA) &
			     R13_MD1_APSRC_REQ) == 0) {
				md_sleep = 1;
				mdelay(3);
			}

			/* Check ap_mdsrc_ack = 1'b1 */
			while ((spm_read(AP_MDSRC_REQ) &
				AP_MDSMSRC_ACK_LSB) == 0) {
				if (i++ < 10) {
					mdelay(1);
				} else {
					spm_crit2(
		"WARNING: MD SLEEP = %d, %s CAN NOT polling AP_MD1SRC_ACK\n",
		md_sleep, __func__);
					break;
				}
			}
		}
	} else {
		spin_lock_irqsave(&__spm_lock, flags);

		spm_ap_mdsrc_req_cnt--;

		if (spm_ap_mdsrc_req_cnt < 0) {
			spm_crit2(
			"warning: set = %d, spm_ap_mdsrc_req_cnt = %d\n",
			set,
			spm_ap_mdsrc_req_cnt);
		} else {
			if (spm_ap_mdsrc_req_cnt == 0)
				SMC_CALL(MTK_SIP_KERNEL_SPM_AP_MDSRC_REQ,
					 0, 0, 0);
		}

		spin_unlock_irqrestore(&__spm_lock, flags);
	}
}

