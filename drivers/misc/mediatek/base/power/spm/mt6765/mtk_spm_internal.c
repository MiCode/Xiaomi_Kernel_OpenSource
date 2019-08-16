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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <asm/setup.h>
#include <mtk_spm_internal.h>
#include <mtk_power_gs_api.h>

#define WORLD_CLK_CNTCV_L        (0x10017008)
#define WORLD_CLK_CNTCV_H        (0x1001700C)
static u32 pcm_timer_ramp_max_sec_loop = 1;

const char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_SSPM_WDT_EVENT_B",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_SSPM_SPM_IRQ_B",
	[11] = " R12_SCP_IPC_MD2SPM_B",
	[12] = " R12_SCP_WDT_EVENT_B",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USB_CDSC_B",
	[15] = " R12_USB_POWERDWN_B",
	[16] = " R12_SYS_TIMER_EVENT_B",
	[17] = " R12_EINT_EVENT_SECURE_B",
	[18] = " R12_CCIF1_EVENT_B",
	[19] = " R12_UART0_IRQ_B",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERM_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_MD2AP_PEER_EVENT_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_CLDMA_EVENT_B",
	[27] = " R12_SEJ_WDT_GPT_B",
	[28] = " R12_ALL_SSPM_WAKEUP_B",
	[29] = " R12_CPU_IRQ_B",
	[30] = " R12_CPU_WFI_AND_B",
	[31] = " R12_MCUSYS_IDLE_TO_EMI_ALL_B",
};

/**************************************
 * Function and API
 **************************************/
int __spm_get_pcm_timer_val(const struct pwr_ctrl *pwrctrl)
{
	u32 val;

	/* set PCM timer (set to max when disable) */
	if (pwrctrl->timer_val_ramp_en != 0) {
		u32 index;

		get_random_bytes(&index, sizeof(index));

		val = PCM_TIMER_RAMP_BASE_DPIDLE + (index & 0x000000FF);
	} else if (pwrctrl->timer_val_ramp_en_sec != 0) {
		u32 index;

		get_random_bytes(&index, sizeof(index));

		pcm_timer_ramp_max_sec_loop++;
		if (pcm_timer_ramp_max_sec_loop >= 50) {
			pcm_timer_ramp_max_sec_loop = 0;
			/* range 5min to 10min */
			val = PCM_TIMER_RAMP_BASE_SUSPEND_LONG +
				index % PCM_TIMER_RAMP_BASE_SUSPEND_LONG;
		} else {
			/* range 50ms to 16sec50ms */
			val = PCM_TIMER_RAMP_BASE_SUSPEND_50MS +
				index % PCM_TIMER_RAMP_BASE_SUSPEND_SHORT;
		}
	} else {
		if (pwrctrl->timer_val_cust == 0)
			val = pwrctrl->timer_val ? : PCM_TIMER_MAX;
		else
			val = pwrctrl->timer_val_cust;
	}

	return val;
}

void __spm_set_pwrctrl_pcm_flags(struct pwr_ctrl *pwrctrl, u32 flags)
{
	if (pwrctrl->pcm_flags_cust == 0)
		pwrctrl->pcm_flags = flags;
	else
		pwrctrl->pcm_flags = pwrctrl->pcm_flags_cust;
}

void __spm_set_pwrctrl_pcm_flags1(struct pwr_ctrl *pwrctrl, u32 flags)
{
	if (pwrctrl->pcm_flags1_cust == 0)
		pwrctrl->pcm_flags1 = flags;
	else
		pwrctrl->pcm_flags1 = pwrctrl->pcm_flags1_cust;
}

void __spm_sync_pcm_flags(struct pwr_ctrl *pwrctrl)
{
	/* set PCM flags and data */
	if (pwrctrl->pcm_flags_cust_clr != 0)
		pwrctrl->pcm_flags &= ~pwrctrl->pcm_flags_cust_clr;
	if (pwrctrl->pcm_flags_cust_set != 0)
		pwrctrl->pcm_flags |= pwrctrl->pcm_flags_cust_set;
	if (pwrctrl->pcm_flags1_cust_clr != 0)
		pwrctrl->pcm_flags1 &= ~pwrctrl->pcm_flags1_cust_clr;
	if (pwrctrl->pcm_flags1_cust_set != 0)
		pwrctrl->pcm_flags1 |= pwrctrl->pcm_flags1_cust_set;
}

void __spm_get_wakeup_status(struct wake_status *wakesta)
{
	/* get PC value if PCM assert (pause abort) */
	wakesta->assert_pc = spm_read(PCM_REG_DATA_INI);

	/* get wakeup event */
	/* backup of PCM_REG12_DATA */
	wakesta->r12 = spm_read(SPM_SW_RSV_0);
	wakesta->r12_ext = spm_read(PCM_REG12_EXT_DATA);
	wakesta->raw_sta = spm_read(SPM_WAKEUP_STA);
	wakesta->raw_ext_sta = spm_read(SPM_WAKEUP_EXT_STA);
	/* backup of SPM_WAKEUP_MISC */
	wakesta->wake_misc = spm_read(SPM_BSI_D0_SR);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	wakesta->timer_out = spm_read(SPM_BSI_D1_SR);

	/* get other SYS and co-clock status */
	wakesta->r13 = spm_read(PCM_REG13_DATA);
	wakesta->idle_sta = spm_read(SUBSYS_IDLE_STA);
	wakesta->req_sta = spm_read(SRC_REQ_STA);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = spm_read(SPM_SW_DEBUG);
	wakesta->debug_flag1 = spm_read(WDT_LATCH_SPARE0_FIX);

	/* get special pattern (0xf0000 or 0x10000) if sleep abort */
	/* PCM_EVENT_REG_STA */
	wakesta->event_reg = spm_read(SPM_BSI_D2_SR);

	/* get ISR status */
	wakesta->isr = spm_read(SPM_IRQ_STA);
}

unsigned int __spm_output_wake_reason(
	const struct wake_status *wakesta, bool suspend, const char *scenario)
{
	int i;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[768] = { 0 };
	char *local_ptr;
	int log_size = 0;
	unsigned int wr = WR_UNKNOWN;
	unsigned int spm_26M_off_pct = 0;

	if (wakesta->assert_pc != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("PCM ASSERT AT 0x%x (%s), r13 = 0x%x, ",
			  wakesta->assert_pc, scenario, wakesta->r13);
		pr_info("[SPM] PCM ASSERT AT 0x%x (%s), r13 = 0x%x, ",
			  wakesta->assert_pc, scenario, wakesta->r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x\n",
			  wakesta->debug_flag, wakesta->debug_flag1);
		pr_info(" debug_flag = 0x%x 0x%x\n",
			  wakesta->debug_flag, wakesta->debug_flag1);

		return WR_PCM_ASSERT;
	}

	if (wakesta->r12 & WAKE_SRC_R12_PCM_TIMER) {
		if (wakesta->wake_misc & WAKE_MISC_PCM_TIMER) {
			local_ptr = " PCM_TIMER";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PCM_TIMER;
		}
		if (wakesta->wake_misc & WAKE_MISC_TWAM) {
			local_ptr = " TWAM";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_WAKE_SRC;
		}
		if (wakesta->wake_misc & WAKE_MISC_CPU_WAKE) {
			local_ptr = " CPU";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_WAKE_SRC;
		}
	}
	for (i = 1; i < 32; i++) {
		if (wakesta->r12 & (1U << i)) {
			if ((strlen(buf) + strlen(wakesrc_str[i])) <
				LOG_BUF_SIZE)
				strncat(buf, wakesrc_str[i],
					strlen(wakesrc_str[i]));

			wr = WR_WAKE_SRC;
		}
	}
	WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

	log_size += sprintf(log_buf,
		"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, ",
		scenario, buf, wakesta->timer_out, wakesta->r13,
		wakesta->debug_flag, wakesta->debug_flag1);

	log_size += sprintf(log_buf + log_size,
		  "r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x, idle_sta = 0x%x, ",
		  wakesta->r12, wakesta->r12_ext, wakesta->raw_sta,
			wakesta->idle_sta);

	log_size += sprintf(log_buf + log_size,
		  " req_sta =  0x%x, event_reg = 0x%x, isr = 0x%x, ",
		  wakesta->req_sta, wakesta->event_reg, wakesta->isr);

	if (!strcmp(scenario, "suspend")) {
		/* calculate 26M off percentage in suspend period */
		if (wakesta->timer_out != 0) {
			spm_26M_off_pct = 100 * spm_read(SPM_PASR_DPD_0)
						/ wakesta->timer_out;
		}

		log_size += sprintf(log_buf + log_size,
			"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x, req = 0x%x, ",
			wakesta->raw_ext_sta,
			wakesta->wake_misc,
			spm_read(SPM_SW_FLAG),
			spm_read(SPM_SW_RSV_2),
			spm_read(SPM_SRC_REQ));

		log_size += sprintf(log_buf + log_size,
			"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d\n",
			_golden_read_reg(WORLD_CLK_CNTCV_L),
			_golden_read_reg(WORLD_CLK_CNTCV_H),
			spm_26M_off_pct);
	} else
		log_size += sprintf(log_buf + log_size,
			"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x, req = 0x%x\n",
			wakesta->raw_ext_sta,
			wakesta->wake_misc,
			spm_read(SPM_SW_FLAG),
			spm_read(SPM_SW_RSV_2),
			spm_read(SPM_SRC_REQ));

	WARN_ON(log_size >= 768);

	if (!suspend)
		pr_info("[SPM] %s", log_buf);
	else {
		aee_sram_printk("%s", log_buf);
		pr_info("[SPM] %s", log_buf);
	}

	return wr;
}

long int spm_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

void spm_set_dummy_read_addr(int debug)
{
	/* FIXME: no implementation in this chip ? */
}

int __attribute__ ((weak)) get_dynamic_period(
	int first_use, int first_wakeup_time, int battery_capacity_level)
{
	/* pr_err("NO %s !!!\n", __func__); */
	return 5401;
}

u32 __spm_get_wake_period(int pwake_time, unsigned int last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
		period = get_dynamic_period(last_wr != WR_PCM_TIMER
				? 1 : 0, SPM_WAKE_PERIOD, 1);
		if (period <= 0) {
			pr_info("[SPM] CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		aee_sram_printk("pwake = %d\n", pwake_time);
		pr_info("[SPM] pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
		period = 36 * 3600;

	return period;
}

static bool is_big_buck_ctrl_by_spm(void)
{
	return false;
}

void __sync_big_buck_ctrl_pcm_flag(u32 *flag)
{
	if (is_big_buck_ctrl_by_spm()) {
		*flag |= (SPM_FLAG1_BIG_BUCK_OFF_ENABLE |
				SPM_FLAG1_BIG_BUCK_ON_ENABLE);
	} else {
		*flag &= ~(SPM_FLAG1_BIG_BUCK_OFF_ENABLE |
				SPM_FLAG1_BIG_BUCK_ON_ENABLE);
	}
}

MODULE_DESCRIPTION("SPM-Internal Driver v0.1");
