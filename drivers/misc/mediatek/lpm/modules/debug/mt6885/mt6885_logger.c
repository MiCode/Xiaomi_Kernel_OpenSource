// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>

#include <aee.h>
#include <mtk_lpm.h>

#include <mt6885_spm_comm.h>
#include <mt6885_spm_reg.h>
#include <mt6885_pwr_ctrl.h>
#include <mt6885_pcm_def.h>
#include <mtk_dbg_common_v1.h>
#include <mtk_power_gs_api.h>
#include <mt-plat/mtk_ccci_common.h>

static struct mt6885_spm_wake_status mt6885_wake;
void __iomem *mt6885_spm_base;

const char *mt6885_wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_RESERVED_DEBUG_B",
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
	[26] = " R12_AP2AP_PEER_WAKEUPEVENT_B",
	[27] = " R12_SEJ_EVENT_B",
	[28] = " R12_SPM_CPU_WAKEUPEVENT_B",
	[29] = " R12_APUSYS",
	[30] = " R12_NOT_USED1",
	[31] = " R12_NOT_USED2",
};

struct spm_wakesrc_irq_list mt6885_spm_wakesrc_irqs[] = {
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
	/* mt6358-irq */
	{ WAKE_SRC_STA1_EINT_EVENT_B, "mediatek,apirq", 0, 0},
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

#define WORLD_CLK_CNTCV_L        (0x10017008)
#define WORLD_CLK_CNTCV_H        (0x1001700C)
#define plat_mmio_read(offset)	__raw_readl(mt6885_spm_base + offset)

#define IRQ_NUMBER	\
(sizeof(mt6885_spm_wakesrc_irqs)/sizeof(struct spm_wakesrc_irq_list))
static void mt6885_get_spm_wakesrc_irq(void)
{
	int i;
	struct device_node *node;

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (mt6885_spm_wakesrc_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
			mt6885_spm_wakesrc_irqs[i].name);
		if (!node) {
			pr_info("[name:spm&][SPM] find '%s' node failed\n",
				mt6885_spm_wakesrc_irqs[i].name);
			continue;
		}

		mt6885_spm_wakesrc_irqs[i].irq_no =
			irq_of_parse_and_map(node,
				mt6885_spm_wakesrc_irqs[i].order);

		if (!mt6885_spm_wakesrc_irqs[i].irq_no) {
			pr_info("[name:spm&][SPM] get '%s' failed\n",
				mt6885_spm_wakesrc_irqs[i].name);
		}
	}
}

int mt6885_get_wakeup_status(struct mt6885_spm_wake_status *wakesta)
{
	if (!wakesta || !mt6885_spm_base)
		return -EINVAL;

	wakesta->r12 = plat_mmio_read(SPM_BK_WAKE_EVENT);
	wakesta->r12_ext = plat_mmio_read(SPM_WAKEUP_STA);
	wakesta->raw_sta = plat_mmio_read(SPM_WAKEUP_STA);
	wakesta->raw_ext_sta = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	wakesta->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_STA);
	wakesta->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	wakesta->src_req = plat_mmio_read(SPM_SRC_REQ);

	/* backup of SPM_WAKEUP_MISC */
	wakesta->wake_misc = plat_mmio_read(SPM_BK_WAKE_MISC);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	wakesta->timer_out = plat_mmio_read(SPM_BK_PCM_TIMER);

	/* get other SYS and co-clock status */
	wakesta->r13 = plat_mmio_read(PCM_REG13_DATA);
	wakesta->idle_sta = plat_mmio_read(SUBSYS_IDLE_STA);
	wakesta->req_sta0 = plat_mmio_read(SRC_REQ_STA_0);
	wakesta->req_sta1 = plat_mmio_read(SRC_REQ_STA_1);
	wakesta->req_sta2 = plat_mmio_read(SRC_REQ_STA_2);
	wakesta->req_sta3 = plat_mmio_read(SRC_REQ_STA_3);
	wakesta->req_sta4 = plat_mmio_read(SRC_REQ_STA_4);

	/* get HW CG check status */
	wakesta->cg_check_sta = plat_mmio_read(SPM_CG_CHECK_STA);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = plat_mmio_read(PCM_WDT_LATCH_SPARE_0);
	wakesta->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1);

	/* get backup SW flag status */
	wakesta->b_sw_flag0 = plat_mmio_read(SPM_SW_RSV_7);
	wakesta->b_sw_flag1 = plat_mmio_read(SPM_SW_RSV_8);

	/* get ISR status */
	wakesta->isr = plat_mmio_read(SPM_IRQ_STA);

	/* get SW flag status */
	wakesta->sw_flag0 = plat_mmio_read(SPM_SW_FLAG_0);
	wakesta->sw_flag1 = plat_mmio_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	wakesta->clk_settle = plat_mmio_read(SPM_CLK_SETTLE);
	/* check abort */
	wakesta->is_abort = wakesta->debug_flag & DEBUG_ABORT_MASK;
	wakesta->is_abort |= wakesta->debug_flag1 & DEBUG_ABORT_MASK_1;

	return 0;
}

void mt6885_show_detailed_wakeup_reason(struct mt6885_spm_wake_status *wakesta)
{
	int i;
	unsigned int irq_no;


#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_CCCI_DEVICES
	exec_ccci_kern_func_by_md_id(0, ID_DUMP_MD_SLEEP_MODE,
		NULL, 0);
#endif

#ifdef CONFIG_MTK_ECCCI_DRIVER
	if (wakesta->r12 & R12_CLDMA_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & R12_MD2AP_PEER_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & R12_CCIF0_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & R12_CCIF1_EVENT_B)
		exec_ccci_kern_func_by_md_id(2, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
#endif
#endif
	/* Check if pending irq happen and log them */
	for (i = 0; i < IRQ_NUMBER; i++) {
		if (mt6885_spm_wakesrc_irqs[i].name == NULL ||
			!mt6885_spm_wakesrc_irqs[i].irq_no)
			continue;
		if (mt6885_spm_wakesrc_irqs[i].wakesrc == wakesta->r12) {
			irq_no = mt6885_spm_wakesrc_irqs[i].irq_no;
			if (mt_irq_get_pending(irq_no))
				log_wakeup_reason(irq_no);
		}
	}
}

int mt6885_show_log_message(int type, const char *prefix, void *data)
{
	#define LOG_BUF_SIZE		256
	#define LOG_BUF_OUT_SZ		768
	#define IS_WAKE_MISC(x)	(mt6885_wake.wake_misc & x)
	#define IS_LOGBUF(ptr, newstr) \
		((strlen(ptr) + strlen(newstr)) < LOG_BUF_SIZE)

	int i;
	unsigned int spm_26M_off_pct = 0;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	char *local_ptr;
	int log_size = 0;
	unsigned int wr = WR_UNKNOWN;
	const char *scenario = prefix ?: "UNKNOWN";

	mt6885_get_wakeup_status(&mt6885_wake);

	/* Disable rcu lock checking */
	rcu_irq_enter_irqson();

	if (mt6885_wake.is_abort != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("SPM ABORT (%s), r13 = 0x%x, ",
			scenario, mt6885_wake.r13);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"[SPM] ABORT (%s), r13 = 0x%x, ",
			scenario, mt6885_wake.r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x",
			mt6885_wake.debug_flag, mt6885_wake.debug_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" debug_flag = 0x%x 0x%x",
			mt6885_wake.debug_flag, mt6885_wake.debug_flag1);

		aee_sram_printk(" sw_flag = 0x%x 0x%x",
			mt6885_wake.sw_flag0, mt6885_wake.sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" sw_flag = 0x%x 0x%x\n",
			mt6885_wake.sw_flag0, mt6885_wake.sw_flag1);

		aee_sram_printk(" b_sw_flag = 0x%x 0x%x",
			mt6885_wake.b_sw_flag0, mt6885_wake.b_sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" b_sw_flag = 0x%x 0x%x",
			mt6885_wake.b_sw_flag0, mt6885_wake.b_sw_flag1);

		wr =  WR_ABORT;
	} else {
		if (mt6885_wake.r12 & R12_PCM_TIMER) {
			if (mt6885_wake.wake_misc & WAKE_MISC_PCM_TIMER_EVENT) {
				local_ptr = " PCM_TIMER";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PCM_TIMER;
			}
		}

		if (mt6885_wake.r12 & R12_TWAM_IRQ_B) {
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
		}
		for (i = 1; i < 32; i++) {
			if (mt6885_wake.r12 & (1U << i)) {
				if (IS_LOGBUF(buf, mt6885_wakesrc_str[i]))
					strncat(buf, mt6885_wakesrc_str[i],
						strlen(mt6885_wakesrc_str[i]));

				wr = WR_WAKE_SRC;
			}
		}
		WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, ",
			scenario, buf, mt6885_wake.timer_out, mt6885_wake.r13,
			mt6885_wake.debug_flag, mt6885_wake.debug_flag1);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x 0x%x 0x%x, idle_sta = 0x%x, ",
			mt6885_wake.r12, mt6885_wake.r12_ext,
			mt6885_wake.raw_sta,
			mt6885_wake.md32pcm_wakeup_sta,
			mt6885_wake.md32pcm_event_sta,
			mt6885_wake.idle_sta);

		log_size += scnprintf(log_buf + log_size,
			  LOG_BUF_OUT_SZ - log_size,
			  " req_sta =  0x%x 0x%x 0x%x 0x%x 0x%x, cg_check_sta =0x%x, isr = 0x%x, ",
			  mt6885_wake.req_sta0, mt6885_wake.req_sta1,
			  mt6885_wake.req_sta2, mt6885_wake.req_sta3,
			  mt6885_wake.req_sta4, mt6885_wake.cg_check_sta,
			  mt6885_wake.isr);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x 0x%x 0x%x, req = 0x%x, ",
				mt6885_wake.raw_ext_sta,
				mt6885_wake.wake_misc,
				mt6885_wake.sw_flag0,
				mt6885_wake.sw_flag1, mt6885_wake.b_sw_flag0,
				mt6885_wake.b_sw_flag0,
				mt6885_wake.src_req);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				" clk_settle = 0x%x, ", mt6885_wake.clk_settle);

		if (!strcmp(scenario, "suspend")) {
			/* calculate 26M off percentage in suspend period */
			if (mt6885_wake.timer_out != 0) {
				spm_26M_off_pct =
					(100 * plat_mmio_read(SPM_BK_VTCXO_DUR))
							/ mt6885_wake.timer_out;
			}

			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d\n",
				_golden_read_reg(WORLD_CLK_CNTCV_L),
				_golden_read_reg(WORLD_CLK_CNTCV_H),
				spm_26M_off_pct);
		}
	}
	WARN_ON(log_size >= LOG_BUF_OUT_SZ);

	printk_deferred("[name:spm&][SPM] %s", log_buf);

	if (!strcmp(scenario, "suspend"))
		mt6885_show_detailed_wakeup_reason(&mt6885_wake);

	/* Eable rcu lock checking */
	rcu_irq_exit_irqson();

	return wr;
}


struct mtk_lpm_issuer mt6885_issuer = {
	.log = mt6885_show_log_message,
};

int __init mt6885_logger_init(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		mt6885_spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	if (mt6885_spm_base)
		mtk_lp_issuer_register(&mt6885_issuer);
	else
		pr_info("[name:mtk_lpm][P] - Don't register the issue by error! (%s:%d)\n",
			__func__, __LINE__);

	mt6885_get_spm_wakesrc_irq();

	return 0;
}
late_initcall_sync(mt6885_logger_init);

