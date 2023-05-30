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
#include <linux/suspend.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <lpm_dbg_common_v1.h>
#include <lpm_logger.h>
#include <spm_reg.h>
#include <pwr_ctrl.h>
#include <mt-plat/mtk_ccci_common.h>
#include <lpm_timer.h>
#include <mtk_lpm_sysfs.h>
#include <cond.h>

#define LOG_DEFAULT_MS		5000

#define PCM_32K_TICKS_PER_SEC		(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)

#define aee_sram_printk pr_info

enum spm_req_sta_idx {
	SPM_REQ_STA_IDX_0 = 0,
	SPM_REQ_STA_IDX_1,
	SPM_REQ_STA_IDX_2,
	SPM_REQ_STA_IDX_3,
	SPM_REQ_STA_IDX_4,
	SPM_REQ_STA_IDX_5,
	SPM_REQ_STA_IDX_6,
	SPM_REQ_STA_IDX_MAX,
	SPM_REQ_STA_IDX_ALL = 0xff,
};

static char *spm_cond_cg_str[PLAT_SPM_COND_MAX] = {
	[PLAT_SPM_COND_MTCMOS_0]	= "MTCMOS_0",
	[PLAT_SPM_COND_MTCMOS_1]	= "MTCMOS_1",
	[PLAT_SPM_COND_CG_INFRA_0]	= "INFRA_0",
	[PLAT_SPM_COND_CG_INFRA_1]	= "INFRA_1",
	[PLAT_SPM_COND_CG_PERI_0]	= "PERI_0",
	[PLAT_SPM_COND_CG_PERI_1]	= "PERI_1",
	[PLAT_SPM_COND_CG_MMSYS_0]	= "MMSYS_0",
	[PLAT_SPM_COND_CG_MMSYS_1]	= "MMSYS_1",
	[PLAT_SPM_COND_CG_MDPSYS_0]	= "MDPSYS_0",
};

const char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_TWAM_PMSR_DVFSRC_IRQ",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_CCIF1_EVENT_B",
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

#define plat_mmio_read(offset)	__raw_readl(lpm_spm_base + offset)
u64 ap_pd_count;
u64 ap_slp_duration;
u64 spm_26M_off_count;
u64 spm_26M_off_duration;
u32 before_ap_slp_duration;

static struct spm_req_sta_item req_sta_item[] = {
	{"afe(ssusb0)", SPM_REQ_STA_IDX_0, 0x1F << 0, 0},
	{"cam", SPM_REQ_STA_IDX_0, 0x3 << 15, 0},
	{"ccif", SPM_REQ_STA_IDX_0, 0xFFF << 17, 0},
	{"ccif", SPM_REQ_STA_IDX_1, 0xFFFFFF << 0, 0},
	{"conn", SPM_REQ_STA_IDX_1, 0x7 << 29, 0},
	{"conn", SPM_REQ_STA_IDX_2, 0x9 << 0, 0},
	{"disp0", SPM_REQ_STA_IDX_2, 0x3 << 9, 0},
	{"dpm", SPM_REQ_STA_IDX_2, 0x11 << 17, 0},
	{"dpmaif", SPM_REQ_STA_IDX_2, 0x1F << 25, 0},
	{"gce", SPM_REQ_STA_IDX_2, 0x1 << 31, 0},
	{"gce", SPM_REQ_STA_IDX_3, 0x7 << 0, 0},
	{"img", SPM_REQ_STA_IDX_3, 0x3 << 8, 0},
	{"infrasys", SPM_REQ_STA_IDX_3, 0x1 << 11, 0},
	{"ipic", SPM_REQ_STA_IDX_3, 0x3 << 12, 0},
	{"md", SPM_REQ_STA_IDX_3, 0x3 << 30, 0},
	{"md", SPM_REQ_STA_IDX_4, 0x17 << 0, 0},
	{"mm_proc", SPM_REQ_STA_IDX_4, 0x1F << 7, 0},
	{"scp", SPM_REQ_STA_IDX_5, 0x1F << 3, 0},
	{"srcclkeni", SPM_REQ_STA_IDX_5, 0x5 << 8, 0},
	{"ufs", SPM_REQ_STA_IDX_5, 0x1F << 27, 0},
	{"vdec", SPM_REQ_STA_IDX_6, 0x3 << 0, 0},
	{"venc", SPM_REQ_STA_IDX_6, 0x3 << 2, 0},
};

static struct rtc_time suspend_tm;

static struct spm_req_sta_list req_sta_list = {
	.spm_req = req_sta_item,
	.spm_req_num = sizeof(req_sta_item)/sizeof(struct spm_req_sta_item),
	.is_blocked = 0,
	.suspend_tm = &suspend_tm,
};

struct spm_req_sta_list *spm_get_req_sta_list(void)
{
	return &req_sta_list;
}

struct logger_timer {
	struct lpm_timer tm;
	unsigned int fired;
};

#define	STATE_NUM	10
#define	STATE_NAME_SIZE	15
struct logger_fired_info {
	unsigned int fired;
	unsigned int state_index;
	char state_name[STATE_NUM][STATE_NAME_SIZE];
	int fired_index;
};

static struct lpm_spm_wake_status wakesrc;

static struct lpm_log_helper log_help = {
	.wakesrc = &wakesrc,
	.cur = 0,
	.prev = 0,
};

static int lpm_get_wakeup_status(void)
{
	struct lpm_log_helper *help = &log_help;

	if (!help->wakesrc || !lpm_spm_base)
		return -EINVAL;

	help->wakesrc->r12 = plat_mmio_read(SPM_BK_WAKE_EVENT);
	help->wakesrc->r12_ext = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	help->wakesrc->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_STA);
	help->wakesrc->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	/* backup of SPM_WAKEUP_MISC */
	help->wakesrc->wake_misc = plat_mmio_read(SPM_BK_WAKE_MISC);

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

	help->wakesrc->src_req = plat_mmio_read(SPM_SRC_REQ);

	/* get debug flag for PCM execution check */
	help->wakesrc->debug_flag = plat_mmio_read(PCM_WDT_LATCH_SPARE_0);
	help->wakesrc->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1);

	/* get ISR status */
	help->wakesrc->isr = plat_mmio_read(SPM_IRQ_STA);

	/* get SW flag status */
	help->wakesrc->sw_flag0 = plat_mmio_read(SPM_SW_FLAG_0);
	help->wakesrc->sw_flag1 = plat_mmio_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	help->wakesrc->clk_settle = plat_mmio_read(SPM_CLK_SETTLE);

	help->wakesrc->sw_rsv0 = plat_mmio_read(SPM_SW_RSV_0);
	help->wakesrc->sw_rsv1 = plat_mmio_read(SPM_SW_RSV_1);
	help->wakesrc->sw_rsv2 = plat_mmio_read(SPM_SW_RSV_2);
	help->wakesrc->sw_rsv3 = plat_mmio_read(SPM_SW_RSV_3);
	help->wakesrc->sw_rsv4 = plat_mmio_read(SPM_SW_RSV_4);
	help->wakesrc->sw_rsv5 = plat_mmio_read(SPM_SW_RSV_5);
	help->wakesrc->sw_rsv6 = plat_mmio_read(SPM_SW_RSV_6);
	help->wakesrc->sw_rsv7 = plat_mmio_read(SPM_SW_RSV_7);
	help->wakesrc->sw_rsv8 = plat_mmio_read(SPM_SW_RSV_8);

	help->wakesrc->debug_spare2 = plat_mmio_read(PCM_WDT_LATCH_SPARE_2);
	help->wakesrc->debug_spare3 = plat_mmio_read(PCM_WDT_LATCH_SPARE_3);
	help->wakesrc->debug_spare4 = plat_mmio_read(PCM_WDT_LATCH_SPARE_4);
	help->wakesrc->debug_spare5 = plat_mmio_read(PCM_WDT_LATCH_SPARE_5);
	help->wakesrc->debug_spare6 = plat_mmio_read(PCM_WDT_LATCH_SPARE_6);

	help->cur += 1;
	return 0;
}

static char *spm_resource_str[MT_SPM_RES_MAX] = {
	[MT_SPM_RES_XO_FPM] = "XO_FPM",
	[MT_SPM_RES_CK_26M] = "CK_26M",
	[MT_SPM_RES_INFRA] = "INFRA",
	[MT_SPM_RES_SYSPLL] = "SYSPLL",
	[MT_SPM_RES_DRAM_S0] = "DRAM_S0",
	[MT_SPM_RES_DRAM_S1] = "DRAM_S1",
};

static char *spm_scenario_str[NUM_SPM_SCENE] = {
	[MT_SPM_AUDIO_AFE] = "AUDIO_AFE",
	[MT_SPM_AUDIO_DSP] = "AUDIO_DSP",
	[MT_SPM_USB_HEADSET] = "USB_HEADSET",
};

char *get_spm_scenario_str(unsigned int index)
{
	if (index >= NUM_SPM_SCENE)
		return NULL;
	return spm_scenario_str[index];
}

static void dump_lp_sw_request(void)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	(128)
	char log_buf[LOG_BUF_SIZE] = { 0 };
	unsigned int log_size = 0;
	unsigned int rnum, rusage, per_usage, unum, sta;
	unsigned int unamei, unamet;
	char uname[MT_LP_RQ_USER_NAME_LEN+1];
	int i, j, s, u;
	struct spm_req_sta_list *sta_list;

	sta_list = spm_get_req_sta_list();

	/* dump spm request by SW */
	rnum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_NUM,
		MT_LPM_SMC_ACT_GET, 0, 0);

	rusage = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
		MT_LPM_SMC_ACT_GET,
		MT_LP_RQ_ID_ALL_USAGE, 0);

	unum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NUM,
		MT_LPM_SMC_ACT_GET, 0, 0);

	for (i = 0; i < rnum; i++) {
		if ((1U<<i) & rusage) {

			per_usage = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
				MT_LPM_SMC_ACT_GET, i, 0);

			log_size += scnprintf(log_buf + log_size,
				 LOG_BUF_SIZE - log_size,
				"%s request:", spm_resource_str[i]);

			for (j = 0; j < unum; j++) {
				if (per_usage & (1U << j)) {
					unamei = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NAME,
						MT_LPM_SMC_ACT_GET, j, 0);
					/* convert user name */
					for (s = 0, u = 0; s < MT_LP_RQ_USER_NAME_LEN;
						s++, u += MT_LP_RQ_USER_CHAR_U) {
						unamet = ((unamei >> u) & MT_LP_RQ_USER_CHAR_MASK);
						uname[s] = (unamet) ? (char)unamet : ' ';
					}
					uname[s] = '\0';
					log_size += scnprintf(log_buf + log_size,
						 LOG_BUF_SIZE - log_size,
						"%s ", uname);
				}
			}
			pr_info("suspend warning: %s\n", log_buf);
			log_size = 0;
			memset(log_buf, 0, sizeof(log_buf));
		}
	}

	/* dump LP request by scenario (Audio/USB) */
	sta = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_LP_REQ_STAT,
		 MT_LPM_SMC_ACT_GET, 0, 0);
	if (sta) {
		sta_list->lp_scenario_sta = sta;
		log_size = 0;
		memset(log_buf, 0, sizeof(log_buf));

		log_size += scnprintf(log_buf + log_size,
			 LOG_BUF_SIZE - log_size,
			"scenario:");

		for (i = 0; i < NUM_SPM_SCENE; i++)
			if (sta & (0x1 << i))
				log_size += scnprintf(log_buf + log_size,
					LOG_BUF_SIZE - log_size,
					"%s ", spm_scenario_str[i]);

		pr_info("suspend warning: %s\n", log_buf);
	}
}

static void lpm_save_sleep_info(void)
{
}

static void suspend_show_detailed_wakeup_reason
	(struct lpm_spm_wake_status *wakesta)
{
}

static void dump_lp_cond(void)
{
	int i;
	u32 blkcg;

	for (i = 1 ; i < PLAT_SPM_COND_MAX ; i++) {
		blkcg = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL, MT_LPM_SMC_ACT_GET, 0, i);
		if (blkcg != 0)
			pr_info("suspend warning: CG: %6s = 0x%08x\n"
				, spm_cond_cg_str[i], blkcg);

	}
}

unsigned int is_lp_blocked_threshold;
static void suspend_spm_rsc_req_check
	(struct lpm_spm_wake_status *wakesta)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	256
#undef AVOID_OVERFLOW
#define AVOID_OVERFLOW (0xF0000000)
	static u32 is_blocked_cnt;
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0, i;
	u32 is_no_blocked = 0;
	u32 req_sta[SPM_REQ_STA_IDX_MAX];
	u32 req_sta_mix = 0, temp;
	u32 src_req;
	struct spm_req_sta_list *sta_list;

	sta_list = spm_get_req_sta_list();

	if (is_blocked_cnt >= AVOID_OVERFLOW)
		is_blocked_cnt = 0;

	/* Check if ever enter deepest System LPM */
	/* All resources turned off */
	is_no_blocked = (wakesta->debug_flag & (SPM_DBG_DEBUG_IDX_26M_SLEEP))
			&& is_infra_pdn(wakesta->sw_flag0);

	/* Check if System LPM ever is blocked over is_lp_blocked_threshold times */
	if (!is_no_blocked) {
		is_blocked_cnt++;
	} else {
		is_blocked_cnt = 0;
		sta_list->is_blocked = 0;
	}

	if (is_blocked_cnt < is_lp_blocked_threshold)
		return;

	if (!lpm_spm_base)
		return;

	if (!sta_list)
		return;

	sta_list->is_blocked = 1;

	/* Show who is blocking system LPM */
	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"suspend warning:(OneShot) System LPM is blocked by ");


	for (i = 0; i < SPM_REQ_STA_IDX_MAX; i++) {
		req_sta[i] = plat_mmio_read(SPM_REQ_STA_0 + (i * 4));
		req_sta_mix |= req_sta[i];
	}

	for (i = 0; i < sta_list->spm_req_num; i++) {
		sta_list->spm_req[i].on = 0;
		if (sta_list->spm_req[i].req_sta_num == SPM_REQ_STA_IDX_ALL)
			temp = req_sta_mix & sta_list->spm_req[i].mask;
		else
			temp = req_sta[sta_list->spm_req[i].req_sta_num]
				& sta_list->spm_req[i].mask;
		if (temp) {
			log_size += scnprintf(log_buf + log_size,
				 LOG_BUF_SIZE - log_size, "%s ",  sta_list->spm_req[i].name);
			sta_list->spm_req[i].on = 1;
		}
	}

	src_req = plat_mmio_read(SPM_SRC_REQ);
	if (src_req & 0x43E) {
		dump_lp_cond();
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "spm ");
	}

	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);
	dump_lp_sw_request();
}

static int lpm_show_message(int type, const char *prefix, void *data)
{
	struct lpm_spm_wake_status *wakesrc = log_help.wakesrc;

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
			wakesrc->debug_spare3, wakesrc->debug_spare5);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" b_sw_flag = 0x%x 0x%x",
			wakesrc->debug_spare3, wakesrc->debug_spare5);

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
				if (IS_LOGBUF(buf, wakesrc_str[i]))
					strncat(buf, wakesrc_str[i],
						strlen(wakesrc_str[i]));

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
			"r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x 0x%x, ",
			wakesrc->r12, wakesrc->r12_ext,
			wakesrc->md32pcm_wakeup_sta,
			wakesrc->md32pcm_event_sta);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"req_sta =  0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x, req = 0x%x, ",
			wakesrc->req_sta0, wakesrc->req_sta1,
			wakesrc->req_sta2, wakesrc->req_sta3,
			wakesrc->req_sta4, wakesrc->req_sta5,
			wakesrc->req_sta6, wakesrc->src_req);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"pcm_flag = 0x%x 0x%x 0x%x 0x%x, ",
			wakesrc->sw_flag0, wakesrc->sw_flag1,
			wakesrc->debug_spare3, wakesrc->debug_spare5);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"rsv = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x, ",
			wakesrc->sw_rsv0, wakesrc->sw_rsv1,
			wakesrc->sw_rsv2, wakesrc->sw_rsv3,
			wakesrc->sw_rsv4, wakesrc->sw_rsv5,
			wakesrc->sw_rsv6, wakesrc->sw_rsv7,
			wakesrc->sw_rsv8);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"debug = 0x%x 0x%x 0x%x 0x%x 0x%x, ",
			wakesrc->debug_spare2, wakesrc->debug_spare3,
			wakesrc->debug_spare4, wakesrc->debug_spare5,
			wakesrc->debug_spare6);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"wake_misc = 0x%x, isr = 0x%x, clk_settle = 0x%x, ",
			wakesrc->wake_misc,
			wakesrc->isr,
			wakesrc->clk_settle);

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
		suspend_show_detailed_wakeup_reason(wakesrc);
		suspend_spm_rsc_req_check(wakesrc);
		pr_info("[name:spm&][SPM] Suspended for %d.%03d seconds",
			PCM_TICK_TO_SEC(wakesrc->timer_out),
			PCM_TICK_TO_SEC((wakesrc->timer_out %
				PCM_32K_TICKS_PER_SEC)
			* 1000));
		log_md_sleep_info();
		/* Eable rcu lock checking */
//		rcu_irq_exit_irqson();
	} else
		pr_info("[name:spm&][SPM] %s", log_buf);

end:
	return wr;
}

static struct lpm_dbg_plat_ops dbg_ops = {
	.lpm_show_message = lpm_show_message,
	.lpm_save_sleep_info = lpm_save_sleep_info,
	.lpm_get_spm_wakesrc_irq = NULL,
	.lpm_get_wakeup_status = lpm_get_wakeup_status,
};

static int lpm_dbg_logger_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec64 tv = { 0 };

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		ktime_get_real_ts64(&tv);
		rtc_time64_to_tm(tv.tv_sec, &suspend_tm);
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		return NOTIFY_DONE;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static struct notifier_block lpm_dbg_logger_notifier_func = {
	.notifier_call = lpm_dbg_logger_event,
	.priority = 0,
};

int dbg_ops_register(void)
{
	int ret;

	ret = lpm_dbg_plat_ops_register(&dbg_ops);
	if (ret)
		pr_info("[name:spm&][SPM] Failed to register dbg plat ops.\n");

	ret = register_pm_notifier(&lpm_dbg_logger_notifier_func);
	if (ret)
		pr_info("[name:spm&][SPM] Failed to register dbg pm notifier.\n");

	is_lp_blocked_threshold = 10;

	return 0;
}
