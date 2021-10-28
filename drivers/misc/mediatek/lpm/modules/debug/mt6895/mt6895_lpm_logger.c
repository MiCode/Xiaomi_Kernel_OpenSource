// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <mt6895_spm_reg.h>
#include <mt6895_pwr_ctrl.h>
#include <mt-plat/mtk_ccci_common.h>
#include <lpm_timer.h>
#include <mtk_lpm_sysfs.h>
#include <mt6895_cond.h>

#define MT6895_LOG_DEFAULT_MS		5000

#define PCM_32K_TICKS_PER_SEC		(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)

#define aee_sram_printk pr_info

static char *mt6895_spm_cond_cg_str[PLAT_SPM_COND_MAX] = {
	[PLAT_SPM_COND_MTCMOS_0]	= "MTCMOS_0",
	[PLAT_SPM_COND_CG_INFRA_0]	= "INFRA_0",
	[PLAT_SPM_COND_CG_INFRA_1]	= "INFRA_1",
	[PLAT_SPM_COND_CG_INFRA_2]	= "INFRA_2",
	[PLAT_SPM_COND_CG_INFRA_3]	= "INFRA_3",
	[PLAT_SPM_COND_CG_INFRA_4]	= "INFRA_4",
	[PLAT_SPM_COND_CG_INFRA_5]	= "INFRA_5",
	[PLAT_SPM_COND_CG_PERI_0]	= "PERI_0",
	[PLAT_SPM_COND_CG_PERI_1]	= "PERI_1",
	[PLAT_SPM_COND_CG_PERI_2]	= "PERI_2",
	[PLAT_SPM_COND_CG_MMSYS_0]	= "MMSYS_0",
	[PLAT_SPM_COND_CG_MMSYS_1]	= "MMSYS_1",
	[PLAT_SPM_COND_CG_MMSYS_2]	= "MMSYS_2",
	[PLAT_SPM_COND_CG_MMSYS_3]	= "MMSYS_3",
	[PLAT_SPM_COND_CG_MMSYS_4]	= "MMSYS_4",
	[PLAT_SPM_COND_CG_MMSYS_5]	= "MMSYS_5",
	[PLAT_SPM_COND_CG_MMSYS_6]	= "MMSYS_6",
};

/*FIXME*/
const char *mt6895_wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_TWAM_PMSR_DVFSRC_IRQ",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_SC_SSPM2SPM_WAKEUP_B",
	[11] = " R12_SC_SCP2SPM_WAKEUP_B",
	[12] = " R12_SC_ADSP2SPM_WAKEUP_B",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USB_CDSC_B",
	[15] = " R12_USB_POWERDWN_B",
	[16] = " R12_RESERVED_BIT",
	[17] = " R12_RESERVED_BIT",
	[18] = " R12_SYSTIMER",
	[19] = " R12_EINT_SECURED",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERM_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_MD2AP_PEER_EVENT_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_AP2AP_PEER_WAKEUPEVENT_B",
	[27] = " R12_SEJ_EVENT_B",
	[28] = " R12_SPM_CPU_WAKEUPEVENT_B",
	[29] = " R12_APUSYS",
	[30] = " R12_PCIE",
	[31] = " R12_MSDC",
};
/*FIXME*/
struct spm_wakesrc_irq_list mt6895_spm_wakesrc_irqs[] = {
	/* mtk-kpd */
	{ WAKE_SRC_STA1_KP_IRQ_B, "mediatek,kp", 0, 0},
	/* mt_wdt */
	{ WAKE_SRC_STA1_APWDT_EVENT_B, "mediatek,toprgu", 0, 0},
	/* BTCVSD_ISR_Handle */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mtk-btcvsd-snd", 0, 0},
	/* BTIF_WAKEUP_IRQ */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,bt", 0, 0},
	/* BGF_SW_IRQ */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,bt", 1, 0},
	/* wlan0 */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,wifi", 0, 0},
	/* wlan0 */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,wifi", 1, 0},
	/* fm */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,fm", 0, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 0, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 1, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 2, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 3, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 4, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 5, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 6, 0},
	/* CCIF_AP_DATA */
	{ WAKE_SRC_STA1_CCIF0_EVENT_B, "mediatek,ap_ccif0", 0, 0},
	/* SCP IPC0 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 0, 0},
	/* SCP IPC1 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 1, 0},
	/* MBOX_ISR0 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 2, 0},
	/* MBOX_ISR1 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 3, 0},
	/* MBOX_ISR2 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 4, 0},
	/* MBOX_ISR3 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 5, 0},
	/* MBOX_ISR4 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 6, 0},
	/* ADSP_A_AUD */
	{ WAKE_SRC_STA1_SC_ADSP2SPM_WAKEUP_B, "mediatek,adsp_core_0", 2, 0},
	/* ADSP_B_AUD */
	{ WAKE_SRC_STA1_SC_ADSP2SPM_WAKEUP_B, "mediatek,adsp_core_1", 2, 0},
	/* CCIF0_AP */
	{ WAKE_SRC_STA1_MD1_WDT_B, "mediatek,mddriver", 2, 0},
	/* DPMAIF_AP */
	{ WAKE_SRC_STA1_AP2AP_PEER_WAKEUPEVENT_B, "mediatek,dpmaif", 0, 0},
};

#define plat_mmio_read(offset)	__raw_readl(lpm_spm_base + offset)
u64 ap_pd_count;
u64 ap_slp_duration;
u64 spm_26M_off_count;
u64 spm_26M_off_duration;
u32 before_ap_slp_duration;

struct mt6895_logger_timer {
	struct lpm_timer tm;
	unsigned int fired;
};
#define	STATE_NUM	10
#define	STATE_NAME_SIZE	15
struct mt6895_logger_fired_info {
	unsigned int fired;
	unsigned int state_index;
	char state_name[STATE_NUM][STATE_NAME_SIZE];
	int fired_index;
};

static struct lpm_spm_wake_status mt6895_wakesrc;

static struct lpm_log_helper mt6895_log_help = {
	.wakesrc = &mt6895_wakesrc,
	.cur = 0,
	.prev = 0,
};


#define IRQ_NUMBER	\
	(sizeof(mt6895_spm_wakesrc_irqs)/sizeof(struct spm_wakesrc_irq_list))
static void lpm_get_spm_wakesrc_irq(void)
{
	int i;
	struct device_node *node = NULL;

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (mt6895_spm_wakesrc_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
			mt6895_spm_wakesrc_irqs[i].name);
		if (!node) {
			pr_info("[name:spm&][SPM] find '%s' node failed\n",
				mt6895_spm_wakesrc_irqs[i].name);
			continue;
		}

		mt6895_spm_wakesrc_irqs[i].irq_no =
			irq_of_parse_and_map(node,
				mt6895_spm_wakesrc_irqs[i].order);

		if (!mt6895_spm_wakesrc_irqs[i].irq_no) {
			pr_info("[name:spm&][SPM] get '%s' failed\n",
				mt6895_spm_wakesrc_irqs[i].name);
		}
	}
}

static int lpm_get_wakeup_status(void)
{
	struct lpm_log_helper *help = &mt6895_log_help;

	if (!help->wakesrc || !lpm_spm_base)
		return -EINVAL;

	help->wakesrc->r12 = plat_mmio_read(SPM_BK_WAKE_EVENT);
	help->wakesrc->r12_ext = plat_mmio_read(SPM_WAKEUP_STA);
	help->wakesrc->raw_sta = plat_mmio_read(SPM_WAKEUP_STA);
	help->wakesrc->raw_ext_sta = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	help->wakesrc->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_STA);
	help->wakesrc->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	help->wakesrc->src_req = plat_mmio_read(SPM_SRC_REQ);

	/* backup of SPM_WAKEUP_MISC */
	help->wakesrc->wake_misc = plat_mmio_read(SPM_BK_WAKE_MISC);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	help->wakesrc->timer_out = plat_mmio_read(SPM_BK_PCM_TIMER);

	/* get other SYS and co-clock status */
	help->wakesrc->r13 = plat_mmio_read(MD32PCM_SCU_STA0);
	help->wakesrc->req_sta0 = plat_mmio_read(SPM_REQ_STA_0);
	help->wakesrc->req_sta1 = plat_mmio_read(SPM_REQ_STA_1);
	help->wakesrc->req_sta2 = plat_mmio_read(SPM_REQ_STA_2);
	help->wakesrc->req_sta3 = plat_mmio_read(SPM_REQ_STA_3);
	help->wakesrc->req_sta4 = plat_mmio_read(SPM_REQ_STA_4);
	help->wakesrc->req_sta5 = plat_mmio_read(SPM_REQ_STA_5);
	help->wakesrc->req_sta6 = plat_mmio_read(SPM_REQ_STA_6);

	/* get HW CG check status */
	help->wakesrc->cg_check_sta = plat_mmio_read(SPM_REQ_STA_2);

	/* get debug flag for PCM execution check */
	help->wakesrc->debug_flag = plat_mmio_read(PCM_WDT_LATCH_SPARE_0);
	help->wakesrc->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1);

	/* get backup SW flag status */
	help->wakesrc->b_sw_flag0 = plat_mmio_read(SPM_SW_RSV_7);
	help->wakesrc->b_sw_flag1 = plat_mmio_read(SPM_SW_RSV_8);

	help->wakesrc->rt_req_sta0 = plat_mmio_read(SPM_SW_RSV_2);
	help->wakesrc->rt_req_sta1 = plat_mmio_read(SPM_SW_RSV_3);
	help->wakesrc->rt_req_sta2 = plat_mmio_read(SPM_SW_RSV_4);
	help->wakesrc->rt_req_sta3 = plat_mmio_read(SPM_SW_RSV_5);
	help->wakesrc->rt_req_sta4 = plat_mmio_read(SPM_SW_RSV_6);
	/* get ISR status */
	help->wakesrc->isr = plat_mmio_read(SPM_IRQ_STA);

	/* get SW flag status */
	help->wakesrc->sw_flag0 = plat_mmio_read(SPM_SW_FLAG_0);
	help->wakesrc->sw_flag1 = plat_mmio_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	help->wakesrc->clk_settle = plat_mmio_read(SPM_CLK_SETTLE);
	/* check abort */

	help->cur += 1;
	return 0;
}

static void lpm_save_sleep_info(void)
{
#define AVOID_OVERFLOW (0xFFFFFFFF00000000)
	u32 off_26M_duration;
	u32 slp_duration;

	if (!lpm_spm_base)
		return;

	slp_duration = plat_mmio_read(SPM_BK_PCM_TIMER);
	if (slp_duration == before_ap_slp_duration)
		return;

	/* Save ap off counter and duration */
	if (ap_pd_count >= AVOID_OVERFLOW)
		ap_pd_count = 0;
	else
		ap_pd_count++;

	if (ap_slp_duration >= AVOID_OVERFLOW)
		ap_slp_duration = 0;
	else {
		ap_slp_duration = ap_slp_duration + slp_duration;
		before_ap_slp_duration = slp_duration;
	}

	/* Save 26M's off counter and duration */
	if (spm_26M_off_duration >= AVOID_OVERFLOW)
		spm_26M_off_duration = 0;
	else {
		off_26M_duration = plat_mmio_read(SPM_BK_VTCXO_DUR);
		if (off_26M_duration == 0)
			return;

		spm_26M_off_duration = spm_26M_off_duration +
			off_26M_duration;
	}

	if (spm_26M_off_count >= AVOID_OVERFLOW)
		spm_26M_off_count = 0;
	else
		spm_26M_off_count = (plat_mmio_read(SPM_SRCCLKENA_EVENT_COUNT_STA)
					& 0xffff)
			+ spm_26M_off_count;
}

static void mt6895_suspend_show_detailed_wakeup_reason
	(struct lpm_spm_wake_status *wakesta)
{
}
static void dump_lp_cond(void)
{
#define MT6885_DBG_SMC(_id, _act, _rc, _param) ({\
	(u32) mtk_lpm_smc_spm_dbg(_id, _act, _rc, _param); })

	int i;
	u32 blkcg;

	for (i = 1 ; i < PLAT_SPM_COND_MAX ; i++) {
		blkcg = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL, MT_LPM_SMC_ACT_GET, 0, i);
		if (blkcg != 0)
			pr_info("suspend warning: CG: %6s = 0x%08lx\n"
				, mt6895_spm_cond_cg_str[i], blkcg);

	}
}
static void mt6895_suspend_spm_rsc_req_check
	(struct lpm_spm_wake_status *wakesta)
{
#define LOG_BUF_SIZE		        256
#define IS_BLOCKED_OVER_TIMES		10
#undef AVOID_OVERFLOW
#define AVOID_OVERFLOW (0xF0000000)
static u32 is_blocked_cnt;
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;
	u32 is_no_blocked = 0;
	u32 req_sta_0, req_sta_1, req_sta_2;
	u32 req_sta_3, req_sta_4, req_sta_5;
	u32 req_sta_6;
	u32 src_req;

	if (is_blocked_cnt >= AVOID_OVERFLOW)
		is_blocked_cnt = 0;

	/* Check if ever enter deepest System LPM */
	is_no_blocked = wakesta->debug_flag & 0x2;

	/* Check if System LPM ever is blocked over 10 times */
	if (!is_no_blocked)
		is_blocked_cnt++;
	else
		is_blocked_cnt = 0;

	if (is_blocked_cnt < IS_BLOCKED_OVER_TIMES)
		return;

	if (!lpm_spm_base)
		return;

	/* Show who is blocking system LPM */
	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"suspend warning:(OneShot) System LPM is blocked by ");

	req_sta_0 = plat_mmio_read(SPM_REQ_STA_0);
	req_sta_1 = plat_mmio_read(SPM_REQ_STA_1);
	req_sta_2 = plat_mmio_read(SPM_REQ_STA_2);
	req_sta_3 = plat_mmio_read(SPM_REQ_STA_3);
	req_sta_4 = plat_mmio_read(SPM_REQ_STA_4);
	req_sta_5 = plat_mmio_read(SPM_REQ_STA_5);
	req_sta_6 = plat_mmio_read(SPM_REQ_STA_6);
	if (req_sta_4 & 0x1F)
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "md ");
	if ((req_sta_2 & 0xF) || (req_sta_1 & (0x7 << 29)))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "conn ");
	if (req_sta_2 & (0xF << 9))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "disp ");

	if (req_sta_5 & (0x1F << 3))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "scp ");

	if (req_sta_0 & (0x1F << 10))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "adsp ");

	if (req_sta_5 & (0x1F << 27))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "ufs ");

	if (req_sta_4 & (0x3FF << 15))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "msdc ");

	if (req_sta_0 & (0x1F << 5))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "apu ");

	if (req_sta_3 & 0x7)
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "gce ");


	src_req = plat_mmio_read(SPM_SRC_REQ);
	if (src_req & 0x63E) {
		dump_lp_cond();
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "spm ");
	}
	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
}

static int lpm_show_message(int type, const char *prefix, void *data)
{
	struct lpm_spm_wake_status *wakesrc = mt6895_log_help.wakesrc;

#undef LOG_BUF_SIZE
	#define LOG_BUF_SIZE		256
	#define LOG_BUF_OUT_SZ		768
	#define IS_WAKE_MISC(x)	(wakesrc->wake_misc & x)
	#define IS_LOGBUF(ptr, newstr) \
		((strlen(ptr) + strlen(newstr)) < LOG_BUF_SIZE)

	unsigned int spm_26M_off_pct = 0;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	char *local_ptr = NULL;
	int i = 0, log_size = 0, log_type = 0;
	unsigned int wr = WR_UNKNOWN;
	const char *scenario = prefix ?: "UNKNOWN";

	log_type = ((struct lpm_issuer *)data)->log_type;

	if (log_type < LOG_SUCCEESS) {
		aee_sram_printk("[name:spm&][SPM] %s didn't enter MCUSYS off, cstate enter func ret = %d\n",
					prefix, log_type);
		wr =  WR_ABORT;

		goto end;
	}

	if (log_type == LOG_MCUSYS_NOT_OFF) {
		aee_sram_printk("[name:spm&][SPM] %s didn't enter MCUSYS off, MCUSYS cnt is no update\n",
					prefix);
		wr =  WR_ABORT;

		goto end;
	}

	if (wakesrc->is_abort != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("SPM ABORT (%s), r13 = 0x%x, ",
			scenario, wakesrc->r13);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"[SPM] ABORT (%s), r13 = 0x%x, ",
			scenario, wakesrc->r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x",
			wakesrc->debug_flag, wakesrc->debug_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" debug_flag = 0x%x 0x%x",
			wakesrc->debug_flag, wakesrc->debug_flag1);

		aee_sram_printk(" sw_flag = 0x%x 0x%x",
			wakesrc->sw_flag0, wakesrc->sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" sw_flag = 0x%x 0x%x\n",
			wakesrc->sw_flag0, wakesrc->sw_flag1);

		aee_sram_printk(" b_sw_flag = 0x%x 0x%x",
			wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" b_sw_flag = 0x%x 0x%x",
			wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);

		wr =  WR_ABORT;
	} else {
		if (wakesrc->r12 & R12_PCM_TIMER_B) {
			if (wakesrc->wake_misc & WAKE_MISC_PCM_TIMER_EVENT) {
				local_ptr = " PCM_TIMER";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PCM_TIMER;
			}
		}

		if (wakesrc->r12 & R12_SPM_DEBUG_B) {
			if (IS_WAKE_MISC(WAKE_MISC_DVFSRC_IRQ)) {
				local_ptr = " DVFSRC";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_DVFSRC;
			}
			if (IS_WAKE_MISC(WAKE_MISC_TWAM_IRQ_B)) {
				local_ptr = " TWAM";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_TWAM;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMSR_IRQ_B_SET0)) {
				local_ptr = " PMSR";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PMSR;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMSR_IRQ_B_SET1)) {
				local_ptr = " PMSR";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PMSR;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_0)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_1)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_2)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_3)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SRCLKEN_RC_ERR_INT)) {
				local_ptr = " WAKE_MISC_SRCLKEN_RC_ERR_INT";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_VLP_BUS_TIMEOUT_IRQ)) {
				local_ptr = " WAKE_MISC_VLP_BUS_TIMEOUT_IRQ";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMIC_EINT_OUT)) {
				local_ptr = " WAKE_MISC_PMIC_EINT_OUT";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMIC_IRQ_ACK)) {
				local_ptr = " WAKE_MISC_PMIC_IRQ_ACK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMIC_SCP_IRQ)) {
				local_ptr = " WAKE_MISC_PMIC_SCP_IRQ";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
		}
		for (i = 1; i < 32; i++) {
			if (wakesrc->r12 & (1U << i)) {
				if (IS_LOGBUF(buf, mt6895_wakesrc_str[i]))
					strncat(buf, mt6895_wakesrc_str[i],
						strlen(mt6895_wakesrc_str[i]));

				wr = WR_WAKE_SRC;
			}
		}
		WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, ",
			scenario, buf, wakesrc->timer_out, wakesrc->r13,
			wakesrc->debug_flag, wakesrc->debug_flag1);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x 0x%x 0x%x, idle_sta = 0x%x, ",
			wakesrc->r12, wakesrc->r12_ext,
			wakesrc->raw_sta,
			wakesrc->md32pcm_wakeup_sta,
			wakesrc->md32pcm_event_sta,
			wakesrc->idle_sta);

		log_size += scnprintf(log_buf + log_size,
			  LOG_BUF_OUT_SZ - log_size,
			  "req_sta =  0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x, cg_check_sta =0x%x, isr = 0x%x, rt_req_sta0 = 0x%x rt_req_sta1 = 0x%x rt_req_sta2 = 0x%x rt_req_sta3 = 0x%x dram_sw_con_3 = 0x%x, ",
			  wakesrc->req_sta0, wakesrc->req_sta1,
			  wakesrc->req_sta2, wakesrc->req_sta3,
			  wakesrc->req_sta4, wakesrc->req_sta5, wakesrc->req_sta6,
			  wakesrc->cg_check_sta,
			  wakesrc->isr, wakesrc->rt_req_sta0,
			  wakesrc->rt_req_sta1, wakesrc->rt_req_sta2,
			  wakesrc->rt_req_sta3, wakesrc->rt_req_sta4);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x 0x%x 0x%x, req = 0x%x, ",
				wakesrc->raw_ext_sta,
				wakesrc->wake_misc,
				wakesrc->sw_flag0,
				wakesrc->sw_flag1, wakesrc->b_sw_flag0,
				wakesrc->b_sw_flag0,
				wakesrc->src_req);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				" clk_settle = 0x%x, ", wakesrc->clk_settle);

		if (type == LPM_ISSUER_SUSPEND && lpm_spm_base) {
			/* calculate 26M off percentage in suspend period */
			if (wakesrc->timer_out != 0) {
				spm_26M_off_pct =
					(100 * plat_mmio_read(SPM_BK_VTCXO_DUR))
							/ wakesrc->timer_out;
			}
			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d\n",
				plat_mmio_read(SYS_TIMER_VALUE_L),
				plat_mmio_read(SYS_TIMER_VALUE_H),
				spm_26M_off_pct);
		}
	}
	WARN_ON(log_size >= LOG_BUF_OUT_SZ);

	if (type == LPM_ISSUER_SUSPEND) {
		pr_info("[name:spm&][SPM] %s", log_buf);
		mt6895_suspend_show_detailed_wakeup_reason(wakesrc);
		mt6895_suspend_spm_rsc_req_check(wakesrc);
		pr_info("[name:spm&][SPM] Suspended for %d.%03d seconds",
			PCM_TICK_TO_SEC(wakesrc->timer_out),
			PCM_TICK_TO_SEC((wakesrc->timer_out %
				PCM_32K_TICKS_PER_SEC)
			* 1000));

		/* Eable rcu lock checking */
//		rcu_irq_exit_irqson();
	} else
		pr_info("[name:spm&][SPM] %s", log_buf);

end:
	return wr;
}


static struct lpm_dbg_plat_ops mt6895_dbg_ops = {
	.lpm_show_message = lpm_show_message,
	.lpm_save_sleep_info = lpm_save_sleep_info,
	.lpm_get_spm_wakesrc_irq = lpm_get_spm_wakesrc_irq,
	.lpm_get_wakeup_status = lpm_get_wakeup_status,
};

int mt6895_dbg_ops_register(void)
{
	int ret;

	ret = lpm_dbg_plat_ops_register(&mt6895_dbg_ops);

	return ret;
}
