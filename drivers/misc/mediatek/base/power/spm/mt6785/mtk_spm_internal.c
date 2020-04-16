/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#define CONFIG_LPM_STATE_RECORDS
#ifdef CONFIG_LPM_STATE_RECORDS
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#endif

#define WORLD_CLK_CNTCV_L        (0x10017008)
#define WORLD_CLK_CNTCV_H        (0x1001700C)

#define CONN_SLEEP_MASK          (0xe010)
#define MODEM_SLEEP_MASK         (0x110c)
#define SUBSYS_SLEEP_MASK        (0xff)

#ifdef CONFIG_LPM_STATE_RECORDS
#define MAX_LSR_RECORDS		1024
#define MAX_LWR_RECORDS		1024
#define MAX_LPM_SHORT_TIME	(30*32*1024)

static DEFINE_SPINLOCK(lsr_lock);
static DEFINE_SPINLOCK(lwr_lock);

struct lpm_state_records_buff {
	char msg[256];
	struct timespec key_time;
};

struct lpm_wakeup_records_buff {
	char msg[256];
	struct timespec key_time;
};

struct lpm_state_records_buff lsr_buff[MAX_LSR_RECORDS];
struct lpm_wakeup_records_buff lwr_buff[MAX_LWR_RECORDS];

static u32 lsr_num;
static u32 index_head;
static u32 index_tail;
static u32 lwr_num;
static u32 lwr_index_head;
static u32 lwr_index_tail;

static void lsr_record(const struct wake_status *wakesta, unsigned int spm_26M_off_pct);
static void lwr_record(char *buf);
#endif

static u32 pcm_timer_ramp_max_sec_loop = 1;
u64 ap_pd_count;
u64 ap_slp_duration;

u64 spm_26M_off_count;
u64 spm_26M_off_duration;

char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER_EVENT",
	[1] = " R12_SPM_TWAM_IRQ_B",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_SC_SSPM2SPM_WAKEUP",
	[11] = " R12_SC_SCP2SPM_WAKEUP",
	[12] = " R12_SC_ADSP2SPM_WAKEUP",
	[13] = " R12_PCM_WDT_EVENT_B",
	[14] = " R12_USBX_CDSC_B",
	[15] = " R12_USBX_POWERDWN_B",
	[16] = " R12_SYS_TIMER_EVENT_B",
	[17] = " R12_EINT_EVENT_SECURE_B",
	[18] = " R12_CCIF1_EVENT_B",
	[19] = " R12_UART0_IRQ_B",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERMAL_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_MD2AP_PEER_WAKEUP_EVENT",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_AP2AP_PEER_WAKEUP_EVENT",
	[27] = " R12_SEJ_EVENT_B",
	[28] = " R12_SPM_CPU_WAKEUP_EVENT",
	[29] = " R12_CPU_IRQOUT",
	[30] = " R12_CPU_WFI",
	[31] = " R12_MCUSYS_IDLE_TO_EMI_ALL",
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
	/* get wakeup event */
	/* backup of PCM_REG12_DATA */
	wakesta->r12 = spm_read(SPM_SW_RSV_0);
	wakesta->r12_ext = spm_read(SPM_WAKEUP_STA);
	wakesta->raw_sta = spm_read(SPM_WAKEUP_STA);
	wakesta->raw_ext_sta = spm_read(SPM_WAKEUP_EXT_STA);
	wakesta->md32pcm_wakeup_sta = spm_read(MD32PCM_WAKEUP_STA);
	wakesta->md32pcm_event_sta = spm_read(MD32PCM_EVENT_STA);
	/* backup of SPM_WAKEUP_MISC */
	wakesta->wake_misc = spm_read(SPM_SW_RSV_5);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	wakesta->timer_out = spm_read(SPM_SW_RSV_6);

	/* get other SYS and co-clock status */
	wakesta->r13 = spm_read(PCM_REG13_DATA);
	wakesta->idle_sta = spm_read(SUBSYS_IDLE_STA);
	wakesta->req_sta0 = spm_read(SRC_REQ_STA_0);
	wakesta->req_sta1 = spm_read(SRC_REQ_STA_1);
	wakesta->req_sta2 = spm_read(SRC_REQ_STA_2);
	wakesta->req_sta3 = spm_read(SRC_REQ_STA_3);
	wakesta->req_sta4 = spm_read(SRC_REQ_STA_4);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = spm_read(PCM_WDT_LATCH_SPARE_0);
	wakesta->debug_flag1 = spm_read(PCM_WDT_LATCH_SPARE_1);

	/* get backup SW flag status */
	wakesta->b_sw_flag0 = spm_read(SPM_SW_RSV_7);   /* SPM_SW_RSV_7 */
	wakesta->b_sw_flag1 = spm_read(SPM_SW_RSV_8);   /* SPM_SW_RSV_8 */

	/* get ISR status */
	wakesta->isr = spm_read(SPM_IRQ_STA);

	/* get SW flag status */
	wakesta->sw_flag0 = spm_read(SPM_SW_FLAG_0);
	wakesta->sw_flag1 = spm_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	wakesta->clk_settle = spm_read(SPM_CLK_SETTLE); /* SPM_CLK_SETTLE */
	/* check abort */
	wakesta->is_abort = wakesta->debug_flag & DEBUG_ABORT_MASK;
	wakesta->is_abort |= wakesta->debug_flag1 & DEBUG_ABORT_MASK_1;
}

#define AVOID_OVERFLOW (0xFFFFFFFF00000000)
void __spm_save_ap_sleep_info(struct wake_status *wakesta)
{
	if (ap_pd_count >= AVOID_OVERFLOW)
		ap_pd_count = 0;
	else
		ap_pd_count++;

	if (ap_slp_duration >= AVOID_OVERFLOW)
		ap_slp_duration = 0;
	else
		ap_slp_duration = ap_slp_duration + wakesta->timer_out;
}

void __spm_save_26m_sleep_info(void)
{
	if (spm_26M_off_count >= AVOID_OVERFLOW)
		spm_26M_off_count = 0;
	else
		spm_26M_off_count = (spm_read(SPM_26M_COUNT) & 0xffff)
			+ spm_26M_off_count;

	if (spm_26M_off_duration >= AVOID_OVERFLOW)
		spm_26M_off_duration = 0;
	else
		spm_26M_off_duration = spm_26M_off_duration +
			spm_read(SPM_SW_RSV_4);
}

void rekick_vcorefs_scenario(void)
{
/* FIXME: */
}

unsigned int __spm_output_wake_reason(
	const struct wake_status *wakesta, bool suspend, const char *scenario)
{
	int i;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[1024] = { 0 };
	char *local_ptr;
	int log_size = 0;
	unsigned int wr = WR_UNKNOWN;
	unsigned int spm_26M_off_pct = 0;

	if (wakesta->is_abort != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("SPM ABORT (%s), r13 = 0x%x, ",
			scenario, wakesta->r13);
		printk_deferred("[name:spm&][SPM] ABORT (%s), r13 = 0x%x, ",
			scenario, wakesta->r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x\n",
			wakesta->debug_flag, wakesta->debug_flag1);
		printk_deferred("[name:spm&][SPM] debug_flag = 0x%x 0x%x\n",
			wakesta->debug_flag, wakesta->debug_flag1);

		aee_sram_printk(" sw_flag = 0x%x 0x%x\n",
			wakesta->sw_flag0, wakesta->sw_flag1);
		printk_deferred("[name:spm&][SPM] sw_flag = 0x%x 0x%x\n",
			wakesta->sw_flag0, wakesta->sw_flag1);

		aee_sram_printk(" b_sw_flag = 0x%x 0x%x\n",
			wakesta->b_sw_flag0, wakesta->b_sw_flag1);
		printk_deferred("[name:spm&][SPM] b_sw_flag = 0x%x 0x%x\n",
			wakesta->b_sw_flag0, wakesta->b_sw_flag1);

		wr =  WR_ABORT;
	}


	if (wakesta->r12 & R12_PCM_TIMER_EVENT) {

		if (wakesta->wake_misc & WAKE_MISC_PCM_TIMER_EVENT) {
			local_ptr = wakesrc_str[0];
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PCM_TIMER;
		}
	}

	if (wakesta->r12 & R12_SPM_TWAM_IRQ_B) {

		if (wakesta->wake_misc & WAKE_MISC_DVFSRC_IRQ) {
			local_ptr = " DVFSRC";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_DVFSRC;
		}

		if (wakesta->wake_misc & WAKE_MISC_TWAM_IRQ_B) {
			local_ptr = " TWAM";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_TWAM;
		}

		if (wakesta->wake_misc & WAKE_MISC_PMSR_IRQ_B_SET0) {
			local_ptr = " PMSR";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PMSR;
		}

		if (wakesta->wake_misc & WAKE_MISC_PMSR_IRQ_B_SET1) {
			local_ptr = " PMSR";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PMSR;
		}

		if (wakesta->wake_misc & WAKE_MISC_PMSR_IRQ_B_SET2) {
			local_ptr = " PMSR";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PMSR;
		}

		if (wakesta->wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_0) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}

		if (wakesta->wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_1) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}

		if (wakesta->wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_2) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}

		if (wakesta->wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_3) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}

		if (wakesta->wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
	}

	for (i = 2; i < 32; i++) {
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
		"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, %s, %s, %s, ",
		scenario, buf, wakesta->timer_out, wakesta->r13,
		wakesta->debug_flag, wakesta->debug_flag1,
		(wakesta->r13 & CONN_SLEEP_MASK) ? "connectivity not sleep" : "connectivity sleep",
		(wakesta->r13 & MODEM_SLEEP_MASK) ? "modem not sleep" : "modem sleep",
		(wakesta->debug_flag & SUBSYS_SLEEP_MASK) == SUBSYS_SLEEP_MASK ? "subsys sleep" : "subsys not sleep");

	log_size += sprintf(log_buf + log_size,
		  "r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x, 0x%x, 0x%x 0x%x, ",
		  wakesta->r12, wakesta->r12_ext,
		  wakesta->raw_sta, wakesta->idle_sta,
		  wakesta->md32pcm_wakeup_sta,
		  wakesta->md32pcm_event_sta);

	log_size += sprintf(log_buf + log_size,
		  "req_sta =  0x%x 0x%x 0x%x 0x%x 0x%x, isr = 0x%x, ",
		  wakesta->req_sta0, wakesta->req_sta1, wakesta->req_sta2,
		  wakesta->req_sta3, wakesta->req_sta4, wakesta->isr);

	log_size += sprintf(log_buf + log_size,
		"raw_ext_sta = 0x%x, wake_misc = 0x%x, sw_flag = 0x%x 0x%x 0x%x 0x%x, req = 0x%x,",
		wakesta->raw_ext_sta,
		wakesta->wake_misc,
		wakesta->sw_flag0,
		wakesta->sw_flag1,
		wakesta->b_sw_flag0,
		wakesta->b_sw_flag1,
		spm_read(SPM_SRC_REQ));

	if (!strcmp(scenario, "suspend")) {
		/* calculate 26M off percentage in suspend period */
		if (wakesta->timer_out != 0) {
			spm_26M_off_pct = 100 * spm_read(SPM_SW_RSV_4)
						/ wakesta->timer_out;
		}

		log_size += sprintf(log_buf + log_size,
			" clk_settle = 0x%x, ",
			wakesta->clk_settle);

		log_size += sprintf(log_buf + log_size,
			"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d\n",
			_golden_read_reg(WORLD_CLK_CNTCV_L),
			_golden_read_reg(WORLD_CLK_CNTCV_H),
			spm_26M_off_pct);
	} else
		log_size += sprintf(log_buf + log_size,
			" clk_settle = 0x%x\n",
			wakesta->clk_settle);

	WARN_ON(log_size >= 1024);

	if (!suspend)
		printk_deferred("[name:spm&][SPM] %s", log_buf);
	else {
		aee_sram_printk("%s", log_buf);
		printk_deferred("[name:spm&][SPM] %s", log_buf);
	}

#ifdef CONFIG_LPM_STATE_RECORDS
	if (suspend) {
		lsr_record(wakesta, spm_26M_off_pct);
		lwr_record(buf);
	}
#endif

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
// FIXME
}

int __attribute__ ((weak)) get_dynamic_period(
	int first_use, int first_wakeup_time, int battery_capacity_level)
{
	/* printk_deferred("[name:spm&]NO %s !!!\n", __func__); */
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
			printk_deferred("[name:spm&][SPM] CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		aee_sram_printk("pwake = %d\n", pwake_time);
		printk_deferred("[name:spm&][SPM] pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
		period = 36 * 3600;

	return period;
}

#ifdef CONFIG_LPM_STATE_RECORDS
static void lsr_record(const struct wake_status *wakesta, unsigned int spm_26M_off_pct)
{
	static int m;
	int conn_sleep = 0;
	int modem_sleep = 0;
	int subsys_sleep = 0;

	if (!wakesta)
		return;

	if (!spin_trylock(&lsr_lock))
		return;

	if (wakesta->timer_out > MAX_LPM_SHORT_TIME) {
		conn_sleep = (wakesta->r13 & CONN_SLEEP_MASK) ? 0 : 1;
		modem_sleep = (wakesta->r13 & MODEM_SLEEP_MASK) ? 0 : 1;
		subsys_sleep = (wakesta->debug_flag & SUBSYS_SLEEP_MASK) == SUBSYS_SLEEP_MASK ? 1 : 0;

		if (!conn_sleep || !modem_sleep || !subsys_sleep) {
			index_tail = m;
			getnstimeofday(&lsr_buff[m].key_time);
			sprintf(lsr_buff[m++].msg, "%u, %s, %s, %s, req_sta = 0x%x 0x%x 0x%x 0x%x 0x%x, %u", wakesta->timer_out,
					conn_sleep ? "connectivity sleep" : "connectivity not sleep",
					modem_sleep ? "modem sleep" : "modem not sleep",
					subsys_sleep ? "subsys sleep" : "subsys not sleep",
					wakesta->req_sta0, wakesta->req_sta1, wakesta->req_sta2,
					wakesta->req_sta3, wakesta->req_sta4, spm_26M_off_pct);

			if (m >= MAX_LSR_RECORDS)
				m = 0;

			lsr_num++;
			if (lsr_num >= MAX_LSR_RECORDS) {
				lsr_num = MAX_LSR_RECORDS;
				index_head = index_tail + 1;
				if (index_head >= MAX_LSR_RECORDS)
					index_head = 0;
			}
		}
	}

	spin_unlock(&lsr_lock);
}

static int lsr_seq_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&lsr_lock);
	if (lsr_num < MAX_LSR_RECORDS) {
		for (i = 0; i < lsr_num; i++) {
			rtc_time_to_tm(lsr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_state { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lsr_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_LSR_RECORDS; i++) {
			rtc_time_to_tm(lsr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_state { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lsr_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time_to_tm(lsr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_state { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						lsr_buff[i].msg);
			}
		}
	}
	spin_unlock(&lsr_lock);

	return 0;
}

static int lsr_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, lsr_seq_show, NULL);
}

static const struct file_operations lpm_state_records_ops = {
	.open           = lsr_record_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void lwr_record(char *buf)
{
	static int m;

	if (!buf || (strlen(buf) > LOG_BUF_SIZE))
		return;

	if (!spin_trylock(&lwr_lock))
		return;

	lwr_index_tail = m;
	getnstimeofday(&lwr_buff[m].key_time);
	sprintf(lwr_buff[m++].msg, "%s", buf);

	if (m >= MAX_LWR_RECORDS)
		m = 0;

	lwr_num++;
	if (lwr_num >= MAX_LWR_RECORDS) {
		lwr_num = MAX_LWR_RECORDS;
		lwr_index_head = lwr_index_tail + 1;
		if (lwr_index_head >= MAX_LWR_RECORDS)
			lwr_index_head = 0;
	}

	spin_unlock(&lwr_lock);
}

static int lwr_seq_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&lwr_lock);
	if (lwr_num < MAX_LWR_RECORDS) {
		for (i = 0; i < lwr_num; i++) {
			rtc_time_to_tm(lwr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_wakeup { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lwr_buff[i].msg);
		}
	} else {
		for (i = lwr_index_head; i < MAX_LWR_RECORDS; i++) {
			rtc_time_to_tm(lwr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_wakeup { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lwr_buff[i].msg);
		}

		if (lwr_index_head > lwr_index_tail) {
			for (i = 0; i <= lwr_index_tail; i++) {
				rtc_time_to_tm(lwr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_wakeup { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						lwr_buff[i].msg);
			}
		}
	}
	spin_unlock(&lwr_lock);

	return 0;
}

static int lwr_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, lwr_seq_show, NULL);
}

static const struct file_operations lpm_wakeup_records_ops = {
	.open           = lwr_record_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __init lpm_state_records(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *wp_entry;

	entry = proc_create("lpm_state_records", 0444, NULL, &lpm_state_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create proc lpm_state_records node failed\n", __func__);

	wp_entry = proc_create("lpm_wakeup_records", 0444, NULL, &lpm_wakeup_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create proc node lpm_wakeup_records failed\n", __func__);

	return 0;
}

late_initcall(lpm_state_records);
#endif

MODULE_DESCRIPTION("SPM-Internal Driver v0.1");
