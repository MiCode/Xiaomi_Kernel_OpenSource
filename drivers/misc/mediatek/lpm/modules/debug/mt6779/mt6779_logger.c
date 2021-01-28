// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <aee.h>
#include <mtk_lpm.h>

#include <mt6779_spm_comm.h>
#include <mt6779_spm_reg.h>
#include <mt6779_pwr_ctrl.h>
#include <mt6779_pcm_def.h>
#include <mtk_dbg_common_v1.h>

#define BYPASS_LOG

#ifndef BYPASS_LOG
static struct mt6779_spm_wake_status mt6779_wake;
#endif
void __iomem *mt6779_spm_base;

const char *mt6779_wakesrc_str[32] = {
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


#define plat_mmio_read(offset)	__raw_readl(mt6779_spm_base + offset)
int mt6779_get_wakeup_status(struct mt6779_spm_wake_status *wakesta)
{
	if (!wakesta || !mt6779_spm_base)
		return -EINVAL;

	wakesta->r12 = plat_mmio_read(SPM_SW_RSV_0);
	wakesta->r12_ext = plat_mmio_read(SPM_WAKEUP_STA);
	wakesta->raw_sta = plat_mmio_read(SPM_WAKEUP_STA);
	wakesta->raw_ext_sta = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	wakesta->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_STA);
	wakesta->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	wakesta->src_req = plat_mmio_read(SPM_SRC_REQ);

	/* backup of SPM_WAKEUP_MISC */
	wakesta->wake_misc = plat_mmio_read(SPM_SW_RSV_5);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	wakesta->timer_out = plat_mmio_read(SPM_SW_RSV_6);

	/* get other SYS and co-clock status */
	wakesta->r13 = plat_mmio_read(PCM_REG13_DATA);
	wakesta->idle_sta = plat_mmio_read(SUBSYS_IDLE_STA);
	wakesta->req_sta0 = plat_mmio_read(SRC_REQ_STA_0);
	wakesta->req_sta1 = plat_mmio_read(SRC_REQ_STA_1);
	wakesta->req_sta2 = plat_mmio_read(SRC_REQ_STA_2);
	wakesta->req_sta3 = plat_mmio_read(SRC_REQ_STA_3);
	wakesta->req_sta4 = plat_mmio_read(SRC_REQ_STA_4);

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

#ifdef BYPASS_LOG

int mt6779_show_log_message(int type, const char *prefix, void *data)
{
	return 0;
}

#else

int mt6779_show_log_message(int type, const char *prefix, void *data)
{
	#define LOG_BUF_SIZE        256
	#define LOG_BUF_OUT_SZ	768
	int i;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	char *local_ptr;
	int log_size = 0;
	unsigned int wr = WR_UNKNOWN;
	const char *scenario = prefix ?: "UNKNOWN";

	mt6779_get_wakeup_status(&mt6779_wake);

	/* Disable rcu lock checking */
	rcu_irq_enter_irqson();

	if (mt6779_wake.is_abort != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("SPM ABORT (%s), r13 = 0x%x, ",
			scenario, mt6779_wake.r13);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"[SPM] ABORT (%s), r13 = 0x%x, ",
			scenario, mt6779_wake.r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x\n",
			mt6779_wake.debug_flag, mt6779_wake.debug_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" debug_flag = 0x%x 0x%x\n",
			mt6779_wake.debug_flag, mt6779_wake.debug_flag1);

		aee_sram_printk(" sw_flag = 0x%x 0x%x\n",
			mt6779_wake.sw_flag0, mt6779_wake.sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" sw_flag = 0x%x 0x%x\n",
			mt6779_wake.sw_flag0, mt6779_wake.sw_flag1);

		aee_sram_printk(" b_sw_flag = 0x%x 0x%x\n",
			mt6779_wake.b_sw_flag0, mt6779_wake.b_sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" b_sw_flag = 0x%x 0x%x\n",
			mt6779_wake.b_sw_flag0, mt6779_wake.b_sw_flag1);

		wr =  WR_ABORT;
	}

	if (mt6779_wake.r12 & R12_PCM_TIMER_EVENT) {
		if (mt6779_wake.wake_misc & WAKE_MISC_PCM_TIMER_EVENT) {
			local_ptr = " PCM_TIMER";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PCM_TIMER;
		}
	}

	if (mt6779_wake.r12 & R12_SPM_TWAM_IRQ_B) {
		if (mt6779_wake.wake_misc & WAKE_MISC_DVFSRC_IRQ) {
			local_ptr = " DVFSRC";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_DVFSRC;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_TWAM_IRQ_B) {
			local_ptr = " TWAM";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_TWAM;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_PMSR_IRQ_B_SET0) {
			local_ptr = " PMSR";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PMSR;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_PMSR_IRQ_B_SET1) {
			local_ptr = " PMSR";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_PMSR;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_0) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_1) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_2) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_3) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (mt6779_wake.wake_misc & WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL) {
			local_ptr = " SPM_ACK_CHK";
			if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
				strncat(buf, local_ptr, strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
	}
	for (i = 1; i < 32; i++) {
		if (mt6779_wake.r12 & (1U << i)) {
			if ((strlen(buf) + strlen(mt6779_wakesrc_str[i])) <
				LOG_BUF_SIZE)
				strncat(buf, mt6779_wakesrc_str[i],
					strlen(mt6779_wakesrc_str[i]));

			wr = WR_WAKE_SRC;
		}
	}
	WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

	log_size += scnprintf(log_buf + log_size, LOG_BUF_OUT_SZ - log_size,
		"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, ",
		scenario, buf, mt6779_wake.timer_out, mt6779_wake.r13,
		mt6779_wake.debug_flag, mt6779_wake.debug_flag1);

	log_size += scnprintf(log_buf + log_size, LOG_BUF_OUT_SZ - log_size,
		  "r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x 0x%x 0x%x, idle_sta = 0x%x, ",
		  mt6779_wake.r12, mt6779_wake.r12_ext, mt6779_wake.raw_sta,
		  mt6779_wake.md32pcm_wakeup_sta, mt6779_wake.md32pcm_event_sta,
		  mt6779_wake.idle_sta);

	log_size += scnprintf(log_buf + log_size, LOG_BUF_OUT_SZ - log_size,
		  " req_sta =  0x%x 0x%x 0x%x 0x%x 0x%x, isr = 0x%x, ",
		  mt6779_wake.req_sta0, mt6779_wake.req_sta1,
		  mt6779_wake.req_sta2, mt6779_wake.req_sta3,
		  mt6779_wake.req_sta4, mt6779_wake.isr);

	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x 0x%x 0x%x, req = 0x%x, ",
			mt6779_wake.raw_ext_sta,
			mt6779_wake.wake_misc,
			mt6779_wake.sw_flag0,
			mt6779_wake.sw_flag1, mt6779_wake.b_sw_flag0,
			mt6779_wake.b_sw_flag0,
			mt6779_wake.src_req);

	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" clk_settle = 0x%x, ", mt6779_wake.clk_settle);

	WARN_ON(log_size >= LOG_BUF_OUT_SZ);

	pr_info("[name:spm&][SPM] %s", log_buf);

	/* Eable rcu lock checking */
	rcu_irq_exit_irqson();

	return wr;
}
#endif

struct mtk_lpm_issuer mt6779_issuer = {
	.log = mt6779_show_log_message,
};

int mt6779_logger_init(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,spm");

	if (node) {
		mt6779_spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	if (mt6779_spm_base)
		mtk_lp_issuer_register(&mt6779_issuer);
	else
		pr_info("[name:mtk_lpm][P] - Don't register the issue by error! (%s:%d)\n",
			__func__, __LINE__);
	return 0;
}
EXPORT_SYMBOL(mt6779_logger_init);

