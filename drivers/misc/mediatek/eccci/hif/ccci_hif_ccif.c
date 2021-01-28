// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h> /* for clk_prepare/un* */
#include <linux/syscore_ops.h>

#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "ccci_hif_ccif.h"
#include "md_sys1_platform.h"
#include "ccci_debug_info.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#define TAG "cif"
/* struct md_ccif_ctrl *ccif_ctrl; */
unsigned int devapc_check_flag = 1;

/* this table maybe can be set array when multi, or else. */
static struct ccci_clk_node ccif_clk_table[] = {
	{ NULL, "infra-ccif-ap"},
	{ NULL, "infra-ccif-md"},
	{ NULL, "infra-ccif1-ap"},
	{ NULL, "infra-ccif1-md"},
	{ NULL, "infra-ccif2-ap"},
	{ NULL, "infra-ccif2-md"},
	{ NULL, "infra-ccif4-md"},
};
#define IS_PASS_SKB(per_md_data, qno)	\
	(!per_md_data->data_usb_bypass && (per_md_data->is_in_ee_dump == 0) \
	 && ((1<<qno) & NET_RX_QUEUE_MASK))

/* #define RUN_WQ_BY_CHECKING_RINGBUF */

struct c2k_port {
	enum c2k_channel ch;
	enum c2k_channel excp_ch;
	enum CCCI_CH tx_ch_mapping;
	enum CCCI_CH rx_ch_mapping;
};

static struct c2k_port c2k_ports[] = {
	/* c2k control channel mapping to 2 pairs of CCCI channels,
	 * please mind the order in this array,
	 * make sure CCCI_CONTROL_TX/RX be first.
	 */
	/*control channel */
	{CTRL_CH_C2K, CTRL_CH_C2K_EXCP, CCCI_CONTROL_TX, CCCI_CONTROL_RX,},
	/*control channel */
	{CTRL_CH_C2K, CTRL_CH_C2K, CCCI_STATUS_TX, CCCI_STATUS_RX,},
	/*audio channel */
	{AUDIO_CH_C2K, AUDIO_CH_C2K, CCCI_PCM_TX, CCCI_PCM_RX,},
	/*network channel for CCMNI1 */
	{NET1_CH_C2K, NET1_CH_C2K, CCCI_CCMNI1_TX, CCCI_CCMNI1_RX,},
	/*network channel for CCMNI1 */
	{NET1_CH_C2K, NET1_CH_C2K, CCCI_CCMNI1_DL_ACK, CCCI_CCMNI1_DL_ACK,},
	/*network channel for CCMNI2 */
	{NET2_CH_C2K, NET2_CH_C2K, CCCI_CCMNI2_TX, CCCI_CCMNI2_RX,},
	/*network channel for CCMNI2 */
	{NET2_CH_C2K, NET2_CH_C2K, CCCI_CCMNI2_DL_ACK, CCCI_CCMNI2_DL_ACK,},
	/*network channel for CCMNI3 */
	{NET3_CH_C2K, NET3_CH_C2K, CCCI_CCMNI3_TX, CCCI_CCMNI3_RX,},
	/*network channel for CCMNI3 */
	{NET3_CH_C2K, NET3_CH_C2K, CCCI_CCMNI3_DL_ACK, CCCI_CCMNI3_DL_ACK,},
	/*network channel for CCMNI4 */
	{NET4_CH_C2K, NET4_CH_C2K, CCCI_CCMNI4_TX, CCCI_CCMNI4_RX,},
	/*network channel for CCMNI5 */
	{NET5_CH_C2K, NET5_CH_C2K, CCCI_CCMNI5_TX, CCCI_CCMNI5_RX,},
	/*network channel for CCMNI6 */
	{NET6_CH_C2K, NET6_CH_C2K, CCCI_CCMNI6_TX, CCCI_CCMNI6_RX,},
	/*network channel for CCMNI7 */
	{NET7_CH_C2K, NET7_CH_C2K, CCCI_CCMNI7_TX, CCCI_CCMNI7_RX,},
	/*network channel for CCMNI8 */
	{NET8_CH_C2K, NET8_CH_C2K, CCCI_CCMNI8_TX, CCCI_CCMNI8_RX,},
	{NET10_CH_C2K, NET10_CH_C2K, CCCI_CCMNI10_TX, CCCI_CCMNI10_RX,},
	{NET11_CH_C2K, NET11_CH_C2K, CCCI_CCMNI11_TX, CCCI_CCMNI11_RX,},
	{NET12_CH_C2K, NET12_CH_C2K, CCCI_CCMNI12_TX, CCCI_CCMNI12_RX,},
	{NET13_CH_C2K, NET13_CH_C2K, CCCI_CCMNI13_TX, CCCI_CCMNI13_RX,},
	{NET14_CH_C2K, NET14_CH_C2K, CCCI_CCMNI14_TX, CCCI_CCMNI14_RX,},
	{NET15_CH_C2K, NET15_CH_C2K, CCCI_CCMNI15_TX, CCCI_CCMNI15_RX,},
	{NET16_CH_C2K, NET16_CH_C2K, CCCI_CCMNI16_TX, CCCI_CCMNI16_RX,},
	{NET17_CH_C2K, NET17_CH_C2K, CCCI_CCMNI17_TX, CCCI_CCMNI17_RX,},
	{NET18_CH_C2K, NET18_CH_C2K, CCCI_CCMNI18_TX, CCCI_CCMNI18_RX,},
	{NET19_CH_C2K, NET19_CH_C2K, CCCI_CCMNI19_TX, CCCI_CCMNI19_RX,},
	{NET20_CH_C2K, NET20_CH_C2K, CCCI_CCMNI20_TX, CCCI_CCMNI20_RX,},
	{NET21_CH_C2K, NET21_CH_C2K, CCCI_CCMNI21_TX, CCCI_CCMNI21_RX,},
	/*mdlogger ctrl channel */
	{MDLOG_CTRL_CH_C2K, MDLOG_CTRL_CH_C2K, CCCI_UART1_TX, CCCI_UART1_RX,},
	/*mdlogger data channel */
	{MDLOG_CH_C2K, MDLOG_CH_C2K, CCCI_MD_LOG_TX, CCCI_MD_LOG_RX,},
	/*flashless channel, new */
	{FS_CH_C2K, FS_CH_C2K, CCCI_FS_TX, CCCI_FS_RX,},
	/*ppp channel, for usb bypass */
	{DATA_PPP_CH_C2K, DATA_PPP_CH_C2K,
	CCCI_C2K_PPP_DATA, CCCI_C2K_PPP_DATA,},
	/*AT for rild, new */
	{AT_CH_C2K, AT_CH_C2K, CCCI_C2K_AT, CCCI_C2K_AT,},
	/*AT2 for rild, new */
	{AT2_CH_C2K, AT2_CH_C2K, CCCI_C2K_AT2, CCCI_C2K_AT2,},
	/*AT3 for rild, new */
	{AT3_CH_C2K, AT3_CH_C2K, CCCI_C2K_AT3, CCCI_C2K_AT3,},
	/*AT4 for rild, new */
	{AT4_CH_C2K, AT4_CH_C2K, CCCI_C2K_AT4, CCCI_C2K_AT4,},
	/*AT5 for rild, new */
	{AT5_CH_C2K, AT5_CH_C2K, CCCI_C2K_AT5, CCCI_C2K_AT5,},
	/*AT6 for rild, new */
	{AT6_CH_C2K, AT6_CH_C2K, CCCI_C2K_AT6, CCCI_C2K_AT6,},
	/*AT7 for rild, new */
	{AT7_CH_C2K, AT7_CH_C2K, CCCI_C2K_AT7, CCCI_C2K_AT7,},
	/*AT8 for rild, new */
	{AT8_CH_C2K, AT8_CH_C2K, CCCI_C2K_AT8, CCCI_C2K_AT8,},
	/*agps channel */
	{AGPS_CH_C2K, AGPS_CH_C2K, CCCI_IPC_UART_TX, CCCI_IPC_UART_RX,},
	{MD2AP_LOOPBACK_C2K, MD2AP_LOOPBACK_C2K, CCCI_C2K_LB_DL,
	CCCI_C2K_LB_DL,},
	{LOOPBACK_C2K, LOOPBACK_C2K, CCCI_LB_IT_TX, CCCI_LB_IT_RX,},
	{STATUS_CH_C2K, STATUS_CH_C2K, CCCI_CONTROL_TX, CCCI_CONTROL_RX,},
};

/*always keep this in mind:
 * what if there are more than 1 modems using CLDMA...
 */

/*ccif share memory setting*/
/*need confirm with md. haow*/


static int rx_queue_buffer_size_up_95[QUEUE_NUM] = { 80 * 1024, 80 * 1024,
	40 * 1024, 80 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

static int tx_queue_buffer_size_up_95[QUEUE_NUM] = { 128 * 1024, 40 * 1024,
	8 * 1024, 40 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};
static int rx_exp_buffer_size_up_95[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

static int tx_exp_buffer_size_up_95[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

static int rx_queue_buffer_size[QUEUE_NUM] = { 80 * 1024, 80 * 1024,
	40 * 1024, 80 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
};

static int tx_queue_buffer_size[QUEUE_NUM] = { 128 * 1024, 40 * 1024,
	8 * 1024, 40 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
};
static int rx_exp_buffer_size[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
};

static int tx_exp_buffer_size[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
};

#ifdef CCCI_KMODULE_ENABLE
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;
//void __iomem *infra_ao_base;
#endif

static void md_ccif_dump(unsigned char *title, unsigned char hif_id)
{
	int idx;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "%s: %s\n", __func__, title);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "AP_CON(%p)=0x%x\n",
			md_ctrl->ccif_ap_base + APCCIF_CON,
			ccif_read32(md_ctrl->ccif_ap_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "AP_BUSY(%p)=0x%x\n",
			md_ctrl->ccif_ap_base + APCCIF_BUSY,
			ccif_read32(md_ctrl->ccif_ap_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "AP_START(%p)=0x%x\n",
			md_ctrl->ccif_ap_base + APCCIF_START,
			ccif_read32(md_ctrl->ccif_ap_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "AP_TCHNUM(%p)=0x%x\n",
			md_ctrl->ccif_ap_base + APCCIF_TCHNUM,
			ccif_read32(md_ctrl->ccif_ap_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "AP_RCHNUM(%p)=0x%x\n",
			md_ctrl->ccif_ap_base + APCCIF_RCHNUM,
			ccif_read32(md_ctrl->ccif_ap_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "AP_ACK(%p)=0x%x\n",
			md_ctrl->ccif_ap_base + APCCIF_ACK,
			ccif_read32(md_ctrl->ccif_ap_base, APCCIF_ACK));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "MD_CON(%p)=0x%x\n",
			md_ctrl->ccif_md_base + APCCIF_CON,
			ccif_read32(md_ctrl->ccif_md_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "MD_BUSY(%p)=0x%x\n",
			md_ctrl->ccif_md_base + APCCIF_BUSY,
			ccif_read32(md_ctrl->ccif_md_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "MD_START(%p)=0x%x\n",
			md_ctrl->ccif_md_base + APCCIF_START,
			ccif_read32(md_ctrl->ccif_md_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "MD_TCHNUM(%p)=0x%x\n",
			md_ctrl->ccif_md_base + APCCIF_TCHNUM,
			ccif_read32(md_ctrl->ccif_md_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "MD_RCHNUM(%p)=0x%x\n",
			md_ctrl->ccif_md_base + APCCIF_RCHNUM,
			ccif_read32(md_ctrl->ccif_md_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG, "MD_ACK(%p)=0x%x\n",
			md_ctrl->ccif_md_base + APCCIF_ACK,
			ccif_read32(md_ctrl->ccif_md_base, APCCIF_ACK));

	for (idx = 0;
		 idx < md_ctrl->sram_size / sizeof(u32);
		 idx += 4) {
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
				 "CHDATA(%p): %08X %08X %08X %08X\n",
				 md_ctrl->ccif_ap_base + APCCIF_CHDATA +
				 idx * sizeof(u32),
				 ccif_read32(md_ctrl->ccif_ap_base +
						APCCIF_CHDATA,
						(idx + 0) * sizeof(u32)),
				 ccif_read32(md_ctrl->ccif_ap_base +
						APCCIF_CHDATA,
						(idx + 1) * sizeof(u32)),
				 ccif_read32(md_ctrl->ccif_ap_base +
						APCCIF_CHDATA,
						(idx + 2) * sizeof(u32)),
				 ccif_read32(md_ctrl->ccif_ap_base +
						APCCIF_CHDATA,
						(idx + 3) * sizeof(u32)));
	}

}

static void md_ccif_queue_dump(unsigned char hif_id)
{
	int idx;
	unsigned long long ts = 0;
	unsigned long nsec_rem = 0;

	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (!md_ctrl || !md_ctrl->rxq[0].ringbuf)
		return;

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Dump md_ctrl->channel_id 0x%lx\n",
		md_ctrl->channel_id);
	ts = md_ctrl->traffic_info.latest_isr_time;
	nsec_rem = do_div(ts, NSEC_PER_SEC);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Dump CCIF latest isr %5llu.%06lu\n", ts,
		nsec_rem / 1000);
#ifdef DEBUG_FOR_CCB
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Dump CCIF latest r_ch: 0x%x\n",
		md_ctrl->traffic_info.last_ccif_r_ch);
	ts = md_ctrl->traffic_info.latest_ccb_isr_time;
	nsec_rem = do_div(ts, NSEC_PER_SEC);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Dump CCIF latest ccb_isr %5llu.%06lu\n", ts,
		nsec_rem / 1000);
#endif

	for (idx = 0; idx < QUEUE_NUM; idx++) {
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Q%d TX: w=%d, r=%d, len=%d, %p\n",
		idx, md_ctrl->txq[idx].ringbuf->tx_control.write,
		md_ctrl->txq[idx].ringbuf->tx_control.read,
		md_ctrl->txq[idx].ringbuf->tx_control.length,
		md_ctrl->txq[idx].ringbuf);
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Q%d RX: w=%d, r=%d, len=%d\n",
		idx, md_ctrl->rxq[idx].ringbuf->rx_control.write,
		md_ctrl->rxq[idx].ringbuf->rx_control.read,
		md_ctrl->rxq[idx].ringbuf->rx_control.length);
		ts = md_ctrl->traffic_info.latest_q_rx_isr_time[idx];
		nsec_rem = do_div(ts, NSEC_PER_SEC);
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Q%d RX: last isr %5llu.%06lu\n", idx, ts,
		nsec_rem / 1000);
		ts = md_ctrl->traffic_info.latest_q_rx_time[idx];
		nsec_rem = do_div(ts, NSEC_PER_SEC);
		CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Q%d RX: last wq  %5llu.%06lu\n", idx, ts,
		nsec_rem / 1000);
	}
	ccci_md_dump_log_history(md_ctrl->md_id,
		&md_ctrl->traffic_info, 1, QUEUE_NUM, QUEUE_NUM);
}

static void md_ccif_dump_queue_history(unsigned char hif_id, unsigned int qno)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (!md_ctrl || !md_ctrl->rxq[qno].ringbuf)
		return;

	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Dump md_ctrl->channel_id 0x%lx\n", md_ctrl->channel_id);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Dump CCIF Queue%d Control\n", qno);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Q%d TX: w=%d, r=%d, len=%d\n",
		qno, md_ctrl->txq[qno].ringbuf->tx_control.write,
		md_ctrl->txq[qno].ringbuf->tx_control.read,
		md_ctrl->txq[qno].ringbuf->tx_control.length);
	CCCI_MEM_LOG_TAG(md_ctrl->md_id, TAG,
		"Q%d RX: w=%d, r=%d, len=%d\n",
		qno, md_ctrl->rxq[qno].ringbuf->rx_control.write,
		md_ctrl->rxq[qno].ringbuf->rx_control.read,
		md_ctrl->rxq[qno].ringbuf->rx_control.length);
	ccci_md_dump_log_history(md_ctrl->md_id,
		&md_ctrl->traffic_info, 0, qno, qno);
}

static void md_cd_dump_ccif_reg(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	int idx;

	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_PWROFF
		|| ccif_ctrl->ccif_state == HIFCCIF_STATE_MIN) {
		CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG,
			"CCIF not power on, skip dump\n");
		return;
	}

	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "AP_CON(%p)=%x\n",
		ccif_ctrl->ccif_ap_base + APCCIF_CON,
		ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "AP_BUSY(%p)=%x\n",
		ccif_ctrl->ccif_ap_base + APCCIF_BUSY,
		ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "AP_START(%p)=%x\n",
		ccif_ctrl->ccif_ap_base + APCCIF_START,
		ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "AP_TCHNUM(%p)=%x\n",
		ccif_ctrl->ccif_ap_base + APCCIF_TCHNUM,
		ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "AP_RCHNUM(%p)=%x\n",
		ccif_ctrl->ccif_ap_base + APCCIF_RCHNUM,
		ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "AP_ACK(%p)=%x\n",
		ccif_ctrl->ccif_ap_base + APCCIF_ACK,
		ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_ACK));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "MD_CON(%p)=%x\n",
		ccif_ctrl->ccif_md_base + APCCIF_CON,
		ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "MD_BUSY(%p)=%x\n",
		ccif_ctrl->ccif_md_base + APCCIF_BUSY,
		ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "MD_START(%p)=%x\n",
		ccif_ctrl->ccif_md_base + APCCIF_START,
		ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "MD_TCHNUM(%p)=%x\n",
		ccif_ctrl->ccif_md_base + APCCIF_TCHNUM,
		ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "MD_RCHNUM(%p)=%x\n",
		ccif_ctrl->ccif_md_base + APCCIF_RCHNUM,
		ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG, "MD_ACK(%p)=%x\n",
		ccif_ctrl->ccif_md_base + APCCIF_ACK,
		ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_ACK));

	for (idx = 0; idx < ccif_ctrl->sram_size / sizeof(u32);
		idx += 4) {
		CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG,
			"CHDATA(%p): %08X %08X %08X %08X\n",
			ccif_ctrl->ccif_ap_base + APCCIF_CHDATA +
			idx * sizeof(u32),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
				(idx + 0) * sizeof(u32)),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
				(idx + 1) * sizeof(u32)),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
				(idx + 2) * sizeof(u32)),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
				(idx + 3) * sizeof(u32)));
	}
}

static int ccif_debug_dump_data(unsigned int hif_id, int *buff, int length)
{
	int i;
	unsigned int *dest_buff = NULL;
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	int sram_size = ccif_ctrl->sram_size;

	if (!buff || length < 0 || length > sram_size)
		return 0;

	dest_buff = (unsigned int *)buff;

	for (i = 0; i < length / sizeof(unsigned int); i++) {
		*(dest_buff + i) = ccif_read32(ccif_ctrl->ccif_ap_base,
			APCCIF_CHDATA + (sram_size - length) +
			i * sizeof(unsigned int));
	}
	CCCI_MEM_LOG_TAG(ccif_ctrl->md_id, TAG,
		"Dump CCIF SRAM (last %d bytes)\n", length);
	ccci_util_mem_dump(ccif_ctrl->md_id,
		CCCI_DUMP_MEM_DUMP, dest_buff, length);

	return 0;
}

static int md_ccif_op_dump_status(unsigned char hif_id,
	enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (!ccif_ctrl)
		return -1;

	/*runtime data, boot, long time no response EE */
	if (flag & DUMP_FLAG_CCIF) {
		ccif_debug_dump_data(hif_id, buff, length);
		md_ccif_dump("Dump CCIF SRAM\n", hif_id);
		md_ccif_queue_dump(hif_id);
	}
	if (flag & DUMP_FLAG_IRQ_STATUS) {
		CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG,
		"Dump AP CCIF IRQ status not support\n");
		/* mt_irq_dump_status(md_ctrl->ccif_irq_id);*/
	}
	if (flag & DUMP_FLAG_QUEUE_0)
		md_ccif_dump_queue_history(hif_id, 0);
	if (flag & DUMP_FLAG_QUEUE_0_1) {
		md_ccif_dump_queue_history(hif_id, 0);
		md_ccif_dump_queue_history(hif_id, 1);
	}
	if (flag & (DUMP_FLAG_CCIF_REG | DUMP_FLAG_REG))
		md_cd_dump_ccif_reg(hif_id);

	return 0;
}

/*direction: 1: tx; 0: rx*/
static int c2k_ch_to_ccci_ch(int c2k_ch, int direction)
{
	u16 c2k_channel_id;
	int i = 0;

	c2k_channel_id = (u16) c2k_ch;
	for (i = 0; i < (sizeof(c2k_ports) / sizeof(struct c2k_port)); i++) {
		if (c2k_channel_id == c2k_ports[i].ch) {
			CCCI_DEBUG_LOG(MD_SYS3, TAG,
				"%s:channel(%d)-->(T%d R%d)\n",
				(direction == OUT) ? "TX" : "RX", c2k_ch,
				c2k_ports[i].tx_ch_mapping,
				c2k_ports[i].rx_ch_mapping);
			return (direction == OUT) ? c2k_ports[i].tx_ch_mapping :
				c2k_ports[i].rx_ch_mapping;
		}
	}

	CCCI_ERROR_LOG(MD_SYS3, TAG,
		"%s:ERR cannot find mapped c2k ch ID(%d)\n",
		direction ? "TX" : "RX", c2k_ch);
	return CCCI_OVER_MAX_CH;
}

static int ccci_ch_to_c2k_ch(int md_state, int ccci_ch, int direction)
{
	u16 ccci_channel_id;
	u16 channel_map;
	int i = 0;

	ccci_channel_id = (u16) ccci_ch;
	for (i = 0; i < (sizeof(c2k_ports) / sizeof(struct c2k_port)); i++) {
		channel_map = (direction == OUT) ? c2k_ports[i].tx_ch_mapping :
			c2k_ports[i].rx_ch_mapping;

		if (ccci_channel_id == channel_map) {
			CCCI_DEBUG_LOG(MD_SYS3, TAG, "%s:channel(%d)-->(%d)\n",
					(direction == OUT) ? "TX" : "RX",
					ccci_channel_id, c2k_ports[i].ch);
			return (md_state != EXCEPTION) ? c2k_ports[i].ch :
				c2k_ports[i].excp_ch;
		}
	}

	CCCI_ERROR_LOG(MD_SYS3, TAG,
		"%s:ERR cannot find mapped ccci ch ID(%d)\n",
		direction ? "TX" : "RX", ccci_ch);
	return C2K_OVER_MAX_CH;
}

static inline void ccci_md_check_rx_seq_num(unsigned char md_id,
	struct ccci_hif_traffic *traffic_info,
	struct ccci_header *ccci_h, int qno)
{
	u16 channel, seq_num, assert_bit;
	unsigned int param[3] = {0};

	channel = ccci_h->channel;
	seq_num = ccci_h->seq_num;
	assert_bit = ccci_h->assert_bit;

	if (assert_bit && traffic_info->seq_nums[IN][channel] != 0
		&& ((seq_num - traffic_info->seq_nums[IN][channel])
		& 0x7FFF) != 1) {
		CCCI_ERROR_LOG(md_id, CORE,
			"channel %d seq number out-of-order %d->%d (data: %X, %X)\n",
			channel, seq_num, traffic_info->seq_nums[IN][channel],
			ccci_h->data[0], ccci_h->data[1]);
		md_ccif_op_dump_status(CCIF_HIF_ID, DUMP_FLAG_CCIF, NULL, qno);
		param[0] = channel;
		param[1] = traffic_info->seq_nums[IN][channel];
		param[2] = seq_num;
		ccci_md_force_assert(md_id, MD_FORCE_ASSERT_BY_MD_SEQ_ERROR,
			(char *)param, sizeof(param));

	} else {
		traffic_info->seq_nums[IN][channel] = seq_num;
	}
}

static void md_ccif_sram_rx_work(struct work_struct *work)
{
	struct md_ccif_ctrl *md_ctrl =
		container_of(work, struct md_ccif_ctrl, ccif_sram_work);
	struct ccci_header *dl_pkg =
		&md_ctrl->ccif_sram_layout->dl_header;
	struct ccci_header *ccci_h;
	struct ccci_header ccci_hdr;
	struct sk_buff *skb = NULL;
	int pkg_size, ret = 0, retry_cnt = 0;
	int c2k_to_ccci_ch = 0;

	u32 i = 0;
	u8 *md_feature = (u8 *)(&md_ctrl->ccif_sram_layout->md_rt_data);

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
		"%s:dk_pkg=%p, md_featrue=%p\n", __func__,
		dl_pkg, md_feature);
	pkg_size =
		sizeof(struct ccci_header) + sizeof(struct md_query_ap_feature);

	skb = ccci_alloc_skb(pkg_size, 1, 1);
	if (skb == NULL) {
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"%s: alloc skb size=%d, failed\n", __func__,
			pkg_size);
		return;
	}
	skb_put(skb, pkg_size);
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->data[0] = ccif_read32(&dl_pkg->data[0], 0);
	ccci_h->data[1] = ccif_read32(&dl_pkg->data[1], 0);
	/*ccci_h->channel = ccif_read32(&dl_pkg->channel,0); */
	*(((u32 *) ccci_h) + 2) = ccif_read32((((u32 *) dl_pkg) + 2), 0);
	if (md_ctrl->md_id == MD_SYS3) {
		c2k_to_ccci_ch = c2k_ch_to_ccci_ch(ccci_h->channel, IN);
		ccci_h->channel = (u16) c2k_to_ccci_ch;
	}
	ccci_h->reserved = ccif_read32(&dl_pkg->reserved, 0);

	/*warning: make sure struct md_query_ap_feature is 4 bypes align */
	while (i < sizeof(struct md_query_ap_feature)) {
		*((u32 *) (skb->data + sizeof(struct ccci_header) + i))
		= ccif_read32(md_feature, i);
		i += 4;
	}

	if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1) {
		md_ctrl->wakeup_count++;
		CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
			"CCIF_MD wakeup source:(SRX_IDX/%d)(%u)\n",
			ccci_h->channel, md_ctrl->wakeup_count);
	}
	ccci_hdr = *ccci_h;
	ccci_md_check_rx_seq_num(md_ctrl->md_id,
		&md_ctrl->traffic_info, &ccci_hdr, 0);

 RETRY:
	ret = ccci_md_recv_skb(md_ctrl->md_id, md_ctrl->hif_id, skb);
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "Rx msg %x %x %x %x ret=%d\n",
		ccci_hdr.data[0], ccci_hdr.data[1],
		*(((u32 *)&ccci_hdr) + 2), ccci_hdr.reserved, ret);
	if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
		CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
			"%s:ccci_port_recv_skb ret=%d\n", __func__,
			ret);
	} else {
		if (retry_cnt < 20) {
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"%s:ccci_md_recv_skb ret=%d,retry=%d\n", __func__,
			ret, retry_cnt);
			udelay(5);
			retry_cnt++;
			goto RETRY;
		}
		ccci_free_skb(skb);
		CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
		"%s:ccci_port_recv_skb ret=%d\n", __func__, ret);
	}
}

static void c2k_mem_dump(void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _16_fix_num = len / 16;
	int tail_num = len % 16;
	char buf[16];
	int i, j;

	if (curr_p == NULL) {
		CCCI_ERROR_LOG(MD_SYS3, TAG, "[C2K-DUMP]NULL point to dump!\n");
		return;
	}
	if (len == 0) {
		CCCI_ERROR_LOG(MD_SYS3, TAG, "[C2K-DUMP]Not need to dump\n");
		return;
	}

	CCCI_DEBUG_LOG(MD_SYS3, TAG, "[C2K-DUMP]Base: 0x%lx, len: %d\n",
		(unsigned long)start_addr, len);
	/*Fix section */
	for (i = 0; i < _16_fix_num; i++) {
		CCCI_MEM_LOG(MD_SYS3, TAG,
			"[C2K-DUMP]%03X: %08X %08X %08X %08X\n",
			i * 16, *curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3));
		curr_p += 4;
	}

	/*Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		CCCI_MEM_LOG(MD_SYS3, TAG,
			"[C2K-DUMP]%03X: %08X %08X %08X %08X\n",
			i * 16, *curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3));
	}
}

static int ccif_check_flow_ctrl(struct md_ccif_ctrl *md_ctrl,
	struct md_ccif_queue *queue, struct ccci_ringbuf *rx_buf)
{
	int is_busy, buf_size = 0;
	int ret = 0;

	is_busy = ccif_is_md_queue_busy(md_ctrl, queue->index);
	if (is_busy < 0) {
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"ccif flow ctrl: check modem return %d\n",
			is_busy);
		return 0;
	}
	if (is_busy > 0) {
		buf_size = rx_buf->rx_control.write - rx_buf->rx_control.read;
		if (buf_size < 0)
			buf_size += rx_buf->rx_control.length;
		if (queue->resume_cnt < FLOW_CTRL_THRESHOLD &&
			buf_size <= rx_buf->rx_control.length /
			(2 << (queue->resume_cnt * 2))) {
			if (ccci_fsm_get_md_state(md_ctrl->md_id) == READY) {
				ret = ccci_port_send_msg_to_md(md_ctrl->md_id,
						CCCI_CONTROL_TX,
						C2K_FLOW_CTRL_MSG,
						queue->index, 0);
				if (ret < 0)
					CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
						"fail to resume md Q%d, ret0x%x\n",
						queue->index, ret);
				else {
					queue->resume_cnt++;
					CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
						"flow ctrl: resume Q%d, buf %d, cnt %d\n",
						queue->index, buf_size,
						queue->resume_cnt);
				}
			}
		}
	} else
		queue->resume_cnt = 0;

	return ret;
}

static void md_ccif_traffic_work_func(struct work_struct *work)
{
	struct ccci_hif_traffic *traffic_inf =
		container_of(work, struct ccci_hif_traffic,
			traffic_work_struct);
	struct md_ccif_ctrl *md_ctrl =
		container_of(traffic_inf, struct md_ccif_ctrl, traffic_info);

	ccci_port_dump_status(md_ctrl->md_id);
	ccci_channel_dump_packet_counter(md_ctrl->md_id,
		&md_ctrl->traffic_info);
	/*pre_cnt for tx, pkt_cont for rx*/
	if (md_ctrl->md_id == MD_SYS3) {
		CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
		"traffic(AT): tx:[%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld\n",
		CCCI_C2K_AT,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT],
		CCCI_C2K_AT2,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT2],
		CCCI_C2K_AT3,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT3],
		CCCI_C2K_AT4,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT4],
		CCCI_C2K_AT5,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT5],
		CCCI_C2K_AT6,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT6],
		CCCI_C2K_AT7,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT7],
		CCCI_C2K_AT8,
		md_ctrl->traffic_info.logic_ch_pkt_pre_cnt[CCCI_C2K_AT8]);
		CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
		"traffic(AT): rx:[%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld, [%d]%ld\n",
		CCCI_C2K_AT,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT],
		CCCI_C2K_AT2,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT2],
		CCCI_C2K_AT3,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT3],
		CCCI_C2K_AT4,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT4],
		CCCI_C2K_AT5,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT5],
		CCCI_C2K_AT6,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT6],
		CCCI_C2K_AT7,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT7],
		CCCI_C2K_AT8,
		md_ctrl->traffic_info.logic_ch_pkt_cnt[CCCI_C2K_AT8]);
	}
	mod_timer(&md_ctrl->traffic_monitor,
		jiffies + CCIF_TRAFFIC_MONITOR_INTERVAL * HZ);
}

static void md_ccif_traffic_monitor_func(struct timer_list *t)
{
	struct md_ccif_ctrl *md_ctrl = from_timer(md_ctrl, t, traffic_monitor);

	schedule_work(&md_ctrl->traffic_info.traffic_work_struct);
}

atomic_t lb_dl_q;
/*this function may be called from both workqueue and softirq (NAPI)*/
static unsigned long rx_data_cnt;
static unsigned int pkg_num;
static int ccif_rx_collect(struct md_ccif_queue *queue, int budget,
	int blocking, int *result)
{

	struct ccci_ringbuf *rx_buf = queue->ringbuf;
	unsigned char *data_ptr;
	int ret = 0, count = 0, pkg_size;
	unsigned long flags;
	int qno = queue->index;
	struct ccci_header *ccci_h = NULL;
	struct ccci_header ccci_hdr;
	struct sk_buff *skb;
	int c2k_to_ccci_ch = 0;
	unsigned char from_pool;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(queue->hif_id);
	struct ccci_per_md *per_md_data =
		ccci_get_per_md_data(md_ctrl->md_id);

	if (atomic_read(&queue->rx_on_going)) {
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"Q%d rx is on-going(%d)1\n",
			queue->index, atomic_read(&queue->rx_on_going));
		*result = 0;
		return -1;
	}
	atomic_set(&queue->rx_on_going, 1);

	if (IS_PASS_SKB(per_md_data, qno))
		from_pool = 0;
	else
		from_pool = 1;

	while (1) {
		md_ctrl->traffic_info.latest_q_rx_time[qno] = local_clock();
		spin_lock_irqsave(&queue->rx_lock, flags);
		pkg_size = ccci_ringbuf_readable(md_ctrl->md_id, rx_buf);
		spin_unlock_irqrestore(&queue->rx_lock, flags);
		if (pkg_size < 0) {
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"Q%d Rx:rbf readable ret=%d\n",
				queue->index, pkg_size);
			ret = 0;
			goto OUT;
		}

		skb = ccci_alloc_skb(pkg_size, from_pool, blocking);

		if (skb == NULL) {
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
				       "Q%d Rx:ccci_alloc_skb pkg_size=%d failed,count=%d\n",
				       queue->index, pkg_size, count);
			ret = -ENOMEM;
			goto OUT;
		}

		data_ptr = (unsigned char *)skb_put(skb, pkg_size);
		/*copy data into skb */
		spin_lock_irqsave(&queue->rx_lock, flags);
		ret = ccci_ringbuf_read(md_ctrl->md_id, rx_buf,
				data_ptr, pkg_size);
		spin_unlock_irqrestore(&queue->rx_lock, flags);
		if (unlikely(ret < 0)) {
			ccci_free_skb(skb);
			goto OUT;
		}
		ccci_h = (struct ccci_header *)skb->data;
		if (md_ctrl->md_id == MD_SYS3) {
			/* md3(c2k) logical channel number is not
			 * the same as other modems,
			 * so we need use mapping table to
			 * convert channel id here.
			 */
			c2k_to_ccci_ch = c2k_ch_to_ccci_ch(ccci_h->channel, IN);
			ccci_h->channel = (u16) c2k_to_ccci_ch;

			/* heart beat msg from c2k control channel,
			 * but handled by ECCCI status channel handler,
			 * we hack the channel ID here.
			 */
			if (ccci_h->channel == CCCI_C2K_LB_DL) {
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					"Q%d Rx lb_dl\n", queue->index);
				c2k_mem_dump(data_ptr, pkg_size);
			}
		}
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1) {
			md_ctrl->wakeup_count++;
			CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
				"CCIF_MD wakeup source:(%d/%d/%x)(%u)\n",
				queue->index, ccci_h->channel,
				ccci_h->reserved, md_ctrl->wakeup_count);
		}
		if (ccci_h->channel == CCCI_C2K_LB_DL)
			atomic_set(&lb_dl_q, queue->index);

		ccci_hdr = *ccci_h;

		ret = ccci_port_recv_skb(md_ctrl->md_id,
			queue->hif_id, skb, NORMAL_DATA);

		if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
			count++;
			ccci_md_check_rx_seq_num(md_ctrl->md_id,
				&md_ctrl->traffic_info,
				&ccci_hdr, queue->index);
			ccci_md_add_log_history(&md_ctrl->traffic_info, IN,
				(int)queue->index, &ccci_hdr,
				(ret >= 0 ? 0 : 1));
			ccci_channel_update_packet_counter(
				md_ctrl->traffic_info.logic_ch_pkt_cnt,
				&ccci_hdr);

			if (queue->debug_id) {
				CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
					"Q%d Rx recv req ret=%d\n",
					queue->index, ret);
				queue->debug_id = 0;
			}
			spin_lock_irqsave(&queue->rx_lock, flags);
			ccci_ringbuf_move_rpointer(md_ctrl->md_id,
				rx_buf, pkg_size);
			spin_unlock_irqrestore(&queue->rx_lock, flags);
			if (likely(ccci_md_get_cap_by_id(md_ctrl->md_id)
					& MODEM_CAP_TXBUSY_STOP))
				ccif_check_flow_ctrl(md_ctrl, queue, rx_buf);

			if (ccci_hdr.channel == CCCI_MD_LOG_RX) {
				rx_data_cnt += pkg_size - 16;
				pkg_num++;
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					    "Q%d Rx buf read=%d, write=%d, pkg_size=%d, log_cnt=%ld, pkg_num=%d\n",
					    queue->index,
					    rx_buf->rx_control.read,
					    rx_buf->rx_control.write, pkg_size,
						rx_data_cnt, pkg_num);
			}
			ret = 0;
		} else {
			/*leave package into share memory,
			 * and waiting ccci to receive
			 */
			ccci_free_skb(skb);

			if (queue->debug_id == 0) {
				queue->debug_id = 1;
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"Q%d Rx err, ret = 0x%x\n",
					queue->index, ret);
			}

			goto OUT;
		}
		if (count > budget) {
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"Q%d count > budget, exit now\n",
				queue->index);
			goto OUT;
		}
	}

 OUT:
	atomic_set(&queue->rx_on_going, 0);
	*result = count;
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"Q%d rx %d pkg,ret=%d\n",
		queue->index, count, ret);
	spin_lock_irqsave(&queue->rx_lock, flags);
	if (ret != -CCCI_ERR_PORT_RX_FULL
		&& ret != -EAGAIN) {
		pkg_size = ccci_ringbuf_readable(md_ctrl->md_id, rx_buf);
		if (pkg_size > 0)
			ret = -EAGAIN;
	}
	spin_unlock_irqrestore(&queue->rx_lock, flags);
	return ret;
}

static void ccif_rx_work(struct work_struct *work)
{
	int result = 0, ret = 0;
	struct md_ccif_queue *queue =
		container_of(work, struct md_ccif_queue, qwork);
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(queue->hif_id);

	ret = ccif_rx_collect(queue, queue->budget, 1, &result);
	if (ret == -EAGAIN) {
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"Q%u queue again\n", queue->index);
		queue_work(queue->worker, &queue->qwork);
	} else {
		ccci_port_queue_status_notify(md_ctrl->md_id, queue->hif_id,
			queue->index, IN, RX_FLUSH);
	}
}

void ccif_polling_ready(unsigned char hif_id, int step)
{
	int cnt = 500; /*MD timeout is 10s*/
	int time_once = 10;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

#ifdef CCCI_EE_HS_POLLING_TIME
	cnt = CCCI_EE_HS_POLLING_TIME / time_once;
#endif
	while (cnt > 0) {
		if (md_ctrl->channel_id & (1 << step)) {
			clear_bit(step, &md_ctrl->channel_id);
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"poll RCHNUM %ld\n", md_ctrl->channel_id);
			return;
		}
		msleep(time_once);
		cnt--;
	}
	CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
		"poll EE HS timeout, RCHNUM %ld\n",
		md_ctrl->channel_id);
}

static int md_ccif_send(unsigned char hif_id, int channel_id)
{
	int busy = 0;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	busy = ccif_read32(md_ctrl->ccif_ap_base, APCCIF_BUSY);
	if (busy & (1 << channel_id)) {
		CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
			"CCIF channel %d busy\n", channel_id);
	} else {
		ccif_write32(md_ctrl->ccif_ap_base,
			APCCIF_BUSY, 1 << channel_id);
		ccif_write32(md_ctrl->ccif_ap_base,
			APCCIF_TCHNUM, channel_id);
		CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
			"CCIF start=0x%x\n",
			ccif_read32(md_ctrl->ccif_ap_base,
				APCCIF_START));
	}
	return 0;
}

static int md_ccif_send_data(unsigned char hif_id, int channel_id)
{
	switch (channel_id) {
	case H2D_EXCEPTION_CLEARQ_ACK:
		md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_EXP);
		md_ccif_reset_queue(CCIF_HIF_ID, 0);
		break;
	case H2D_SRAM:
		break;
	default:
		break;
	}
	return md_ccif_send(hif_id, channel_id);
}

void md_ccif_sram_reset(unsigned char hif_id)
{
	int idx = 0;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
		"%s\n", __func__);
	for (idx = 0; idx < md_ctrl->sram_size / sizeof(u32);
		 idx += 1)
		ccif_write32(md_ctrl->ccif_ap_base + APCCIF_CHDATA,
			idx * sizeof(u32), 0);
	ccci_reset_seq_num(&md_ctrl->traffic_info);

}

void md_ccif_reset_queue(unsigned char hif_id, unsigned char for_start)
{
	int i;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	unsigned long flags;
	int ccif_id = md_ctrl->md_id ==
		MD_SYS1 ? AP_MD1_CCIF : AP_MD3_CCIF;

	if (for_start) {
		mod_timer(&md_ctrl->traffic_monitor,
			jiffies + CCIF_TRAFFIC_MONITOR_INTERVAL * HZ);
	} else {
		del_timer(&md_ctrl->traffic_monitor);
		ccci_reset_ccif_hw(md_ctrl->md_id,
			ccif_id, md_ctrl->ccif_ap_base,
			md_ctrl->ccif_md_base, md_ctrl);
	}

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG, "%s\n", __func__);
	for (i = 0; i < QUEUE_NUM; ++i) {
		flush_work(&md_ctrl->rxq[i].qwork);
		spin_lock_irqsave(&md_ctrl->rxq[i].rx_lock, flags);
		ccci_ringbuf_reset(md_ctrl->md_id,
			md_ctrl->rxq[i].ringbuf, 0);
		spin_unlock_irqrestore(&md_ctrl->rxq[i].rx_lock, flags);
		md_ctrl->rxq[i].resume_cnt = 0;

		spin_lock_irqsave(&md_ctrl->txq[i].tx_lock, flags);
		ccci_ringbuf_reset(md_ctrl->md_id,
			md_ctrl->txq[i].ringbuf, 1);
		spin_unlock_irqrestore(&md_ctrl->txq[i].tx_lock, flags);

		ccif_wake_up_tx_queue(md_ctrl, i);
		md_ctrl->txq[i].wakeup = 0;
	}
	ccif_reset_busy_queue(md_ctrl);
	ccci_reset_seq_num(&md_ctrl->traffic_info);

}

void md_ccif_switch_ringbuf(unsigned char hif_id, enum ringbuf_id rb_id)
{
	int i;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	unsigned long flags;

	CCCI_NORMAL_LOG(md_ctrl->md_id, TAG, "%s\n", __func__);
	for (i = 0; i < QUEUE_NUM; ++i) {
		spin_lock_irqsave(&md_ctrl->rxq[i].rx_lock, flags);
		md_ctrl->rxq[i].ringbuf = md_ctrl->rxq[i].ringbuf_bak[rb_id];
		spin_unlock_irqrestore(&md_ctrl->rxq[i].rx_lock, flags);

		spin_lock_irqsave(&md_ctrl->txq[i].tx_lock, flags);
		md_ctrl->txq[i].ringbuf = md_ctrl->txq[i].ringbuf_bak[rb_id];
		spin_unlock_irqrestore(&md_ctrl->txq[i].tx_lock, flags);
	}
}

static void md_ccif_check_ringbuf(struct md_ccif_ctrl *md_ctrl, int qno)
{
#ifdef RUN_WQ_BY_CHECKING_RINGBUF
	unsigned long flags;
	int data_to_read;

	if (atomic_read(&md_ctrl->rxq[qno].rx_on_going)) {
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "Q%d rx is on-going(%d)3\n",
			     md_ctrl->rxq[qno].index,
			     atomic_read(&md_ctrl->rxq[qno].rx_on_going));
		return;
	}
	spin_lock_irqsave(&md_ctrl->rxq[qno].rx_lock, flags);
	data_to_read =
		ccci_ringbuf_readable(md_ctrl->md_id,
			md_ctrl->rxq[qno].ringbuf);
	spin_unlock_irqrestore(&md_ctrl->rxq[qno].rx_lock, flags);
	if (unlikely(data_to_read > 0)
		&& ccci_md_napi_check_and_notice(md, qno) == 0
		&& ccci_fsm_get_md_state(md_ctrl->md_id) != EXCEPTION) {
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
			"%d data remain in q%d\n", data_to_read, qno);
		queue_work(md_ctrl->rxq[qno].worker,
			&md_ctrl->rxq[qno].qwork);
	}
#endif
}

/*exception and SRAM channel handler*/
static void md_ccif_handle_exception(struct md_ccif_ctrl *md_ctrl)
{
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"ccif_irq_tasklet1: ch %lx\n", md_ctrl->channel_id);
	if (md_ctrl->channel_id & CCIF_HW_CH_RX_RESERVED) {
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"Interrupt from reserved ccif ch(%ld)\n",
			md_ctrl->channel_id);
		md_ctrl->channel_id &= ~CCIF_HW_CH_RX_RESERVED;
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"After cleared reserved ccif ch(%ld)\n",
			md_ctrl->channel_id);
	}
	if (md_ctrl->channel_id & (1 << D2H_EXCEPTION_INIT)) {
		clear_bit(D2H_EXCEPTION_INIT, &md_ctrl->channel_id);
		md_fsm_exp_info(md_ctrl->md_id, (1 << D2H_EXCEPTION_INIT));
	}

	if (md_ctrl->channel_id & (1 << AP_MD_SEQ_ERROR)) {
		clear_bit(AP_MD_SEQ_ERROR, &md_ctrl->channel_id);
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"MD check seq fail\n");
		ccci_md_dump_info(md_ctrl->md_id,
			DUMP_FLAG_CCIF, NULL, 0);
	}
	if (md_ctrl->channel_id & (1 << (D2H_SRAM))) {
		clear_bit(D2H_SRAM, &md_ctrl->channel_id);
		schedule_work(&md_ctrl->ccif_sram_work);
	}
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"ccif_irq_tasklet2: ch %ld\n", md_ctrl->channel_id);
}

static void md_ccif_launch_work(struct md_ccif_ctrl *md_ctrl)
{
	int i;

	if (md_ctrl->channel_id & (1 << (D2H_SRAM))) {
		clear_bit(D2H_SRAM, &md_ctrl->channel_id);
		schedule_work(&md_ctrl->ccif_sram_work);
	}

	if (md_ctrl->channel_id & (1 << AP_MD_CCB_WAKEUP)) {
		clear_bit(AP_MD_CCB_WAKEUP, &md_ctrl->channel_id);
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "CCB wakeup\n");
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1) {
			md_ctrl->wakeup_count++;
			CCCI_NOTICE_LOG(md_ctrl->md_id, TAG,
			"CCIF_MD wakeup source:(CCB)(%u)\n",
			md_ctrl->wakeup_count);
		}
#ifdef DEBUG_FOR_CCB
		md_ctrl->traffic_info.latest_ccb_isr_time
			= local_clock();
#endif
		ccci_port_queue_status_notify(md_ctrl->md_id, CCIF_HIF_ID,
			AP_MD_CCB_WAKEUP, -1, RX_IRQ);
	}
	for (i = 0; i < QUEUE_NUM; i++) {
		if (md_ctrl->channel_id & (1 << (i + D2H_RINGQ0))) {
			md_ctrl->traffic_info.latest_q_rx_isr_time[i]
				= local_clock();
			clear_bit(i + D2H_RINGQ0, &md_ctrl->channel_id);
			if (atomic_read(&md_ctrl->rxq[i].rx_on_going)) {
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"Q%d rx is on-going(%d)2\n",
				md_ctrl->rxq[i].index,
				atomic_read(&md_ctrl->rxq[i].rx_on_going));
				continue;
			}
			queue_work(md_ctrl->rxq[i].worker,
				&md_ctrl->rxq[i].qwork);
		} else
			md_ccif_check_ringbuf(md_ctrl, i);
	}
}

static irqreturn_t md_ccif_isr(int irq, void *data)
{
	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)data;
	unsigned int ch_id, i;
	u64 cur_time = local_clock();

	/*disable_irq_nosync(md_ctrl->ccif_irq_id); */
	/*must ack first, otherwise IRQ will rush in */
	ch_id = ccif_read32(md_ctrl->ccif_ap_base, APCCIF_RCHNUM);

	for (i = 0; i < CCIF_CH_NUM; i++)
		if (ch_id & 0x1 << i) {
			set_bit(i, &md_ctrl->channel_id);
			ccif_debug_save_irq(i, cur_time);
		}
	/* for 91/92, HIF CCIF is for C2K, only 16 CH;
	 * for 93, only lower 16 CH is for data
	 */
	ccif_write32(md_ctrl->ccif_ap_base,
		APCCIF_ACK, ch_id & 0xFFFF);
	CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
		"%s ch_id = 0x%lX\n", __func__, md_ctrl->channel_id);
	/* igore exception queue */
	if (ch_id >> RINGQ_BASE) {
		md_ctrl->traffic_info.latest_isr_time
			= local_clock();
#ifdef DEBUG_FOR_CCB
	/* infactly, maybe md_ctrl->channel_id is, which maybe cleared */
		md_ctrl->traffic_info.last_ccif_r_ch = ch_id;
#endif
		md_ccif_launch_work(md_ctrl);
	} else
		md_ccif_handle_exception(md_ctrl);

	return IRQ_HANDLED;
}

static inline void md_ccif_queue_struct_init(struct md_ccif_queue *queue,
	unsigned char hif_id, enum DIRECTION dir, unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->hif_id = hif_id;
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->rx_lock);
	spin_lock_init(&queue->tx_lock);
	atomic_set(&queue->rx_on_going, 0);
	queue->debug_id = 0;
	queue->wakeup = 0;
	queue->resume_cnt = 0;
	queue->budget = RX_BUGDET;

	ccif_debug_info_init();
}

static int md_ccif_op_write_room(unsigned char hif_id, unsigned char qno)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (qno >= QUEUE_NUM)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return ccci_ringbuf_writeable(md_ctrl->md_id,
				md_ctrl->txq[qno].ringbuf, 0);
}

static int md_ccif_op_send_skb(unsigned char hif_id, int qno,
	struct sk_buff *skb, int skb_from_pool, int blocking)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	struct md_ccif_queue *queue = NULL;
	/* struct ccci_header *ccci_h =
	 * (struct ccci_header *)req->skb->data;
	 */
	int ret;
	/* struct ccci_header *ccci_h; */
	unsigned long flags;
	int ccci_to_c2k_ch = 0;
	int md_flow_ctrl = 0;
	struct ccci_header *ccci_h;
	int md_cap = ccci_md_get_cap_by_id(md_ctrl->md_id);
	struct ccci_per_md *per_md_data =
		ccci_get_per_md_data(md_ctrl->md_id);

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	queue = &md_ctrl->txq[qno];

	ccci_h = (struct ccci_header *)skb->data;

	if (ccci_h->channel == CCCI_C2K_LB_DL)
		qno = atomic_read(&lb_dl_q);
	if (md_ctrl->plat_val.md_gen < 6295) {
		if (qno > 7) {
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
				"qno error (%d)\n", qno);
			return -CCCI_ERR_INVALID_QUEUE_INDEX;
		}
	}
	queue = &md_ctrl->txq[qno];
 retry:
	/* we use irqsave as network require a lock in softirq,
	 * cause a potential deadlock
	 */
	spin_lock_irqsave(&queue->tx_lock, flags);

	if (ccci_ringbuf_writeable(md_ctrl->md_id,
			queue->ringbuf, skb->len) > 0) {
		if (ccci_h->channel == CCCI_C2K_LB_DL) {
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"Q%d Tx lb_dl\n", queue->index);
			c2k_mem_dump(skb->data, skb->len);
		}
		ccci_md_inc_tx_seq_num(md_ctrl->md_id,
			&md_ctrl->traffic_info, ccci_h);

		ccci_channel_update_packet_counter(
			md_ctrl->traffic_info.logic_ch_pkt_cnt,
			ccci_h);

		if (md_ctrl->md_id == MD_SYS3) {
			/* heart beat msg is sent from status channel in ECCCI,
			 * but from control channel in C2K,
			 * no status channel in C2K
			 */
			if (ccci_h->channel == CCCI_STATUS_TX) {
				ccci_h->channel = CCCI_CONTROL_TX;
				ccci_h->data[1] = C2K_HB_MSG;
				ccci_h->reserved = md_ctrl->heart_beat_counter;
				md_ctrl->heart_beat_counter++;
				ccci_md_inc_tx_seq_num(md_ctrl->md_id,
					&md_ctrl->traffic_info, ccci_h);
			}

			/* md3(c2k) logical channel number is not
			 * the same as other modems,
			 * so we need to use mapping table to
			 * convert channel id here.
			 */
			ccci_to_c2k_ch =
			ccci_ch_to_c2k_ch(ccci_fsm_get_md_state(md_ctrl->md_id),
				ccci_h->channel, OUT);
			if (ccci_to_c2k_ch >= 0
				&& ccci_to_c2k_ch < C2K_OVER_MAX_CH)
				ccci_h->channel = (u16) ccci_to_c2k_ch;
			else {
				ret = -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"channel num error (%d)\n",
					ccci_to_c2k_ch);
				spin_unlock_irqrestore(&queue->tx_lock, flags);
				return ret;
			}
			if (ccci_h->data[1] == C2K_HB_MSG)
				CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
					"hb: 0x%x\n", ccci_h->channel);
		}
		/* copy skb to ringbuf */
		ret = ccci_ringbuf_write(md_ctrl->md_id,
				queue->ringbuf, skb->data, skb->len);
		if (ret != skb->len)
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
				"TX:ERR rbf write: ret(%d)!=req(%d)\n",
				ret, skb->len);
		ccci_md_add_log_history(&md_ctrl->traffic_info, OUT,
			(int)queue->index, ccci_h, 0);
		/* free request */
		ccci_free_skb(skb);

		/* send ccif request */
		md_ccif_send(hif_id, queue->ccif_ch);
		spin_unlock_irqrestore(&queue->tx_lock, flags);
	} else {
		md_flow_ctrl = ccif_is_md_flow_ctrl_supported(md_ctrl);
		if (likely(md_cap & MODEM_CAP_TXBUSY_STOP)
			&& md_flow_ctrl > 0) {
			ccif_set_busy_queue(md_ctrl, qno);
			/* double check tx buffer after set busy bit.
			 * it is to avoid tx buffer is empty now
			 */
			if (unlikely(ccci_ringbuf_writeable(md_ctrl->md_id,
					queue->ringbuf, skb->len) > 0)) {
				ccif_clear_busy_queue(md_ctrl, qno);
				spin_unlock_irqrestore(&queue->tx_lock, flags);
				goto retry;
			} else {
				CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
					"flow ctrl: TX busy on Q%d\n",
					queue->index);
				ccci_port_queue_status_notify(md_ctrl->md_id,
					hif_id, queue->index, OUT, TX_FULL);
			}
		} else
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"flow ctrl is invalid, cap = %d, md_flow_ctrl = %d\n",
				md_cap, md_flow_ctrl);

		spin_unlock_irqrestore(&queue->tx_lock, flags);

		if (blocking) {
			if (md_flow_ctrl > 0) {
				CCCI_NORMAL_LOG(md_ctrl->md_id, TAG,
					"flow ctrl: Q%d is blocking, skb->len = %d\n",
					queue->index, skb->len);
				ret = wait_event_interruptible_exclusive(
					queue->req_wq,
					(queue->wakeup != 0));
				queue->wakeup = 0;
				if (ret == -ERESTARTSYS)
					return -EINTR;
			}
			CCCI_REPEAT_LOG(md_ctrl->md_id, TAG,
				"tx retry for Q%d, ch %d\n",
				queue->index, ccci_h->channel);
				goto retry;
		} else {
			if (per_md_data->data_usb_bypass)
				return -ENOMEM;
			else
				return -EBUSY;
			CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
				"tx fail on q%d\n", qno);
		}
	}
	return 0;
}

static int md_ccif_op_give_more(unsigned char hif_id, unsigned char qno)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (!md_ctrl)
		return -CCCI_ERR_HIF_NOT_POWER_ON;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	queue_work(md_ctrl->rxq[qno].worker,
		&md_ctrl->rxq[qno].qwork);
	return 0;
}

static int md_ccif_stop_queue(unsigned char hif_id,
	unsigned char qno, enum DIRECTION dir)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (dir == OUT)
		ccif_set_busy_queue(md_ctrl, qno);

	return 0;
}

static int md_ccif_start_queue(unsigned char hif_id,
	unsigned char qno, enum DIRECTION dir)
{
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	struct md_ccif_queue *queue = NULL;
	unsigned long flags;

	if (dir == OUT
		&& likely(ccci_md_get_cap_by_id(md_ctrl->md_id)
		& MODEM_CAP_TXBUSY_STOP
		&& (qno < QUEUE_NUM))) {
		queue = &md_ctrl->txq[qno];
		spin_lock_irqsave(&queue->tx_lock, flags);
		ccif_wake_up_tx_queue(md_ctrl, qno);
		/*special for net queue*/
		ccci_hif_queue_status_notify(md_ctrl->md_id,
			hif_id, qno, OUT, TX_IRQ);
		spin_unlock_irqrestore(&queue->tx_lock, flags);
	}
	return 0;
}

int md_ccif_exp_ring_buf_init(struct md_ccif_ctrl *md_ctrl)
{
	int i = 0;
	unsigned char *buf;
	int bufsize = 0;
	struct ccci_ringbuf *ringbuf;
	struct ccci_smem_region *ccism;

	ccism = ccci_md_get_smem_by_user_id(md_ctrl->md_id,
		SMEM_USER_CCISM_MCU_EXP);
	if (ccism->size)
		memset_io(ccism->base_ap_view_vir, 0, ccism->size);

	buf = (unsigned char *)ccism->base_ap_view_vir;

	for (i = 0; i < QUEUE_NUM; i++) {

		if (md_ctrl->plat_val.md_gen >= 6295) {
			bufsize = CCCI_RINGBUF_CTL_LEN +
			rx_exp_buffer_size_up_95[i]
			+ tx_exp_buffer_size_up_95[i];
			ringbuf =
		    ccci_create_ringbuf(md_ctrl->md_id, buf, bufsize,
				rx_exp_buffer_size_up_95[i],
				tx_exp_buffer_size_up_95[i]);
			if (ringbuf == NULL) {
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"ccci_create_ringbuf %d failed\n", i);
				return -1;
			}

		} else {
			bufsize = CCCI_RINGBUF_CTL_LEN + rx_exp_buffer_size[i]
				+ tx_exp_buffer_size[i];
			ringbuf =
			    ccci_create_ringbuf(md_ctrl->md_id, buf, bufsize,
					rx_exp_buffer_size[i],
					tx_exp_buffer_size[i]);
			if (ringbuf == NULL) {
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"ccci_create_ringbuf %d failed\n", i);
				return -1;
			}
		}
		/*rx */
		md_ctrl->rxq[i].ringbuf_bak[RB_EXP] = ringbuf;
		md_ctrl->rxq[i].ccif_ch = D2H_RINGQ0 + i;
		/*tx */
		md_ctrl->txq[i].ringbuf_bak[RB_EXP] = ringbuf;
		md_ctrl->txq[i].ccif_ch = H2D_RINGQ0 + i;
		buf += bufsize;
	}

	return 0;
}

int md_ccif_ring_buf_init(unsigned char hif_id)
{
	int i = 0;
	unsigned char *buf;
	int bufsize = 0;
	struct md_ccif_ctrl *md_ctrl;
	struct ccci_ringbuf *ringbuf;
	struct ccci_smem_region *ccism;

	md_ctrl = (struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	ccism = ccci_md_get_smem_by_user_id(md_ctrl->md_id,
		SMEM_USER_CCISM_MCU);
	if (ccism->size)
		memset_io(ccism->base_ap_view_vir, 0, ccism->size);
	md_ctrl->total_smem_size = 0;
	/*CCIF_MD_SMEM_RESERVE; */
	buf = (unsigned char *)ccism->base_ap_view_vir;

	for (i = 0; i < QUEUE_NUM; i++) {
		if (md_ctrl->plat_val.md_gen >= 6295) {
			bufsize = CCCI_RINGBUF_CTL_LEN
			+ rx_queue_buffer_size_up_95[i]
			+ tx_queue_buffer_size_up_95[i];

			if (md_ctrl->total_smem_size + bufsize > ccism->size) {
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"share memory too small,please check configure,smem_size=%d\n",
					ccism->size);
				return -1;
			}
			ringbuf =
			    ccci_create_ringbuf(md_ctrl->md_id, buf, bufsize,
					rx_queue_buffer_size_up_95[i],
					tx_queue_buffer_size_up_95[i]);
		} else {
			bufsize = CCCI_RINGBUF_CTL_LEN + rx_queue_buffer_size[i]
				+ tx_queue_buffer_size[i];
			if (md_ctrl->total_smem_size + bufsize > ccism->size) {
				CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
					"share memory too small,please check configure,smem_size=%d\n",
					ccism->size);
				return -1;
			}
			ringbuf =
			    ccci_create_ringbuf(md_ctrl->md_id, buf, bufsize,
					rx_queue_buffer_size[i],
					tx_queue_buffer_size[i]);
		}

		if (ringbuf == NULL) {
			CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
				"ccci_create_ringbuf %d failed\n", i);
			return -1;
		}
		/*rx */
		md_ctrl->rxq[i].ringbuf_bak[RB_NORMAL] = ringbuf;
		md_ctrl->rxq[i].ringbuf = ringbuf;
		md_ctrl->rxq[i].ccif_ch = D2H_RINGQ0 + i;
		if (i != C2K_MD_LOG_RX_Q)
			md_ctrl->rxq[i].worker =
				alloc_workqueue("rx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM
					| WQ_HIGHPRI,
					1, i);
		else
			md_ctrl->rxq[i].worker =
				alloc_workqueue("rx%d_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM, 1, i);
		INIT_WORK(&md_ctrl->rxq[i].qwork, ccif_rx_work);
		/*tx */
		md_ctrl->txq[i].ringbuf_bak[RB_NORMAL] = ringbuf;
		md_ctrl->txq[i].ringbuf = ringbuf;
		md_ctrl->txq[i].ccif_ch = H2D_RINGQ0 + i;
		buf += bufsize;
		md_ctrl->total_smem_size += bufsize;
	}

	md_ccif_exp_ring_buf_init(md_ctrl);

	/*flow control zone is behind ring buffer zone*/
#ifdef FLOW_CTRL_ENABLE
	if (ccci_md_get_cap_by_id(md_ctrl->md_id) & MODEM_CAP_TXBUSY_STOP) {
		md_ctrl->flow_ctrl =
		(struct ccif_flow_control *)(ccism->base_ap_view_vir
		+ md_ctrl->total_smem_size);
		md_ctrl->total_smem_size += sizeof(struct ccif_flow_control);
	} else {
		md_ctrl->flow_ctrl = NULL;
		CCCI_INIT_LOG(md_ctrl->md_id, TAG, "No flow control for AP\n");
	}
#else
	md_ctrl->flow_ctrl = NULL;
	CCCI_INIT_LOG(md_ctrl->md_id, TAG, "flow control is disabled\n");
#endif
	ccism->size = md_ctrl->total_smem_size;
	return 0;
}

#define PCCIF_BUSY (0x4)
#define PCCIF_TCHNUM (0xC)
#define PCCIF_ACK (0x14)
#define PCCIF_CHDATA (0x100)
#define PCCIF_SRAM_SIZE (512)
void ccci_reset_ccif_hw(unsigned char md_id,
	int ccif_id, void __iomem *baseA,
	void __iomem *baseB, struct md_ccif_ctrl *md_ctrl)
{
	int i;
	struct ccci_smem_region *region;

	{
		int reset_bit = -1;

		switch (ccif_id) {
		case AP_MD1_CCIF:
			reset_bit = 8;
			break;
		}

		if (reset_bit == -1)
			return;

		/*
		 *this reset bit will clear
		 *CCIF's busy/wch/irq, but not SRAM
		 */
		/*set reset bit*/
		regmap_write(md_ctrl->plat_val.infra_ao_base,
			0x150, 1 << reset_bit);
		/*clear reset bit*/
		regmap_write(md_ctrl->plat_val.infra_ao_base,
			0x154, 1 << reset_bit);
	}

	/* clear SRAM */
	for (i = 0; i < PCCIF_SRAM_SIZE/sizeof(unsigned int); i++) {
		ccif_write32(baseA, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
		ccif_write32(baseB, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
	}

	/* extend from 36bytes to 72bytes in CCIF SRAM */
	/* 0~60bytes for bootup trace,
	 *last 12bytes for magic pattern,smem address and size
	 */
	region = ccci_md_get_smem_by_user_id(md_id,
		SMEM_USER_RAW_MDSS_DBG);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 3 * sizeof(u32),
		0x7274626E);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 2 * sizeof(u32),
		region->base_md_view_phy);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - sizeof(u32),
		region->size);
}
EXPORT_SYMBOL(ccci_reset_ccif_hw);

static int ccif_debug(unsigned char hif_id,
		enum ccci_hif_debug_flg flag, int *para)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	int ret = -1;

	switch (flag) {
	case CCCI_HIF_DEBUG_SET_WAKEUP:
		ret = atomic_set(&ccif_ctrl->wakeup_src, para[0]);
		break;
	case CCCI_HIF_DEBUG_RESET:
		ccci_reset_ccif_hw(ccif_ctrl->md_id, AP_MD1_CCIF,
			ccif_ctrl->ccif_ap_base,
			ccif_ctrl->ccif_md_base, ccif_ctrl);
		break;
	default:
		break;
	}
	return ret;
}
static irqreturn_t md_cd_ccif_isr(int irq, void *data)
{
	struct md_ccif_ctrl *ccif_ctrl = (struct md_ccif_ctrl *)data;
	int channel_id;

	/* must ack first, otherwise IRQ will rush in */
	channel_id = ccif_read32(ccif_ctrl->ccif_ap_base,
		APCCIF_RCHNUM);
	CCCI_DEBUG_LOG(ccif_ctrl->md_id, TAG,
		"MD CCIF IRQ 0x%X\n", channel_id);
	/*don't ack data queue to avoid missing rx intr*/
	ccif_write32(ccif_ctrl->ccif_ap_base, APCCIF_ACK,
		channel_id & (0xFFFF << RINGQ_EXP_BASE));

	md_fsm_exp_info(ccif_ctrl->md_id, channel_id);

	return IRQ_HANDLED;
}

static int ccif_late_init(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	int ret = 0;

	CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG, "%s\n", __func__);

	/* IRQ is enabled after requested, so call enable_irq after
	 * request_irq will get a unbalance warning
	 */
	ret = request_irq(ccif_ctrl->ap_ccif_irq1_id, md_cd_ccif_isr,
			ccif_ctrl->ap_ccif_irq1_flags, "CCIF_AP_DATA",
			ccif_ctrl);
	if (ret) {
		CCCI_ERROR_LOG(ccif_ctrl->md_id, TAG,
			"request CCIF_AP_DATA IRQ1(%d) error %d\n",
			ccif_ctrl->ap_ccif_irq1_id, ret);
		return -1;
	}
	md_ccif_ring_buf_init(CCIF_HIF_ID);

	return 0;
}

static void ccif_set_clk_cg(unsigned char hif_id, unsigned int on)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);
	int idx = 0;
	int ret = 0;

	CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG, "%s: on=%d\n", __func__, on);

	/* Clean MD_PCCIF4_SW_READY and MD_PCCIF4_PWR_ON */

	if (!on)
		regmap_write(ccif_ctrl->plat_val.infra_ao_base,
		0x22C, 0x0);

	for (idx = 0; idx < ARRAY_SIZE(ccif_clk_table); idx++) {
		if (ccif_clk_table[idx].clk_ref == NULL)
			continue;
		if (on) {
			ret = clk_prepare_enable(ccif_clk_table[idx].clk_ref);
			if (ret)
				CCCI_ERROR_LOG(ccif_ctrl->md_id, TAG,
					"%s: on=%d,ret=%d\n",
					__func__, on, ret);
			devapc_check_flag = 1;
		} else {
			if (strcmp(ccif_clk_table[idx].clk_name,
				"infra-ccif4-md") == 0) {
				udelay(1000);
				CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG,
					"ccif4 %s: after 1ms, set 0x%p + 0x14 = 0xFF\n",
					__func__, ccif_ctrl->md_ccif4_base);
				ccci_write32(ccif_ctrl->md_ccif4_base, 0x14,
					0xFF); /* special use ccci_write32 */
			}
			devapc_check_flag = 0;
			clk_disable_unprepare(ccif_clk_table[idx].clk_ref);
		}
	}
	/* Set MD_PCCIF4_PWR_ON */
	if (on) {
		CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG,
			"ccif4 %s:  set 0x%px + 0x22C = 0x1\n",
			__func__,
			ccif_ctrl->plat_val.infra_ao_base);
		regmap_write(ccif_ctrl->plat_val.infra_ao_base,
			0x22C, 0x1);
	}
}

static int ccif_start(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_PWRON)
		return 0;
	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_MIN)
		ccif_late_init(hif_id);
	if (hif_id != CCIF_HIF_ID)
		CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG, "%s but %d\n",
			__func__, hif_id);
	ccif_set_clk_cg(hif_id, 1);
	md_ccif_sram_reset(CCIF_HIF_ID);
	md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_EXP);
	md_ccif_reset_queue(CCIF_HIF_ID, 1);
	md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_NORMAL);
	md_ccif_reset_queue(CCIF_HIF_ID, 1);

	/* clear all ccif irq before enable it.*/
	ccci_reset_ccif_hw(ccif_ctrl->md_id, AP_MD1_CCIF,
		ccif_ctrl->ccif_ap_base,
		ccif_ctrl->ccif_md_base, ccif_ctrl);
	ccif_ctrl->ccif_state = HIFCCIF_STATE_PWRON;
	CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG, "%s\n", __func__);
	return 0;
}

static int ccif_stop(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_PWROFF
		|| ccif_ctrl->ccif_state == HIFCCIF_STATE_MIN)
		return 0;
	/* ACK CCIF for MD. while entering flight mode,
	 * we may send something after MD slept
	 */
	ccif_ctrl->ccif_state = HIFCCIF_STATE_PWROFF;
	ccci_reset_ccif_hw(ccif_ctrl->md_id, AP_MD1_CCIF,
		ccif_ctrl->ccif_ap_base, ccif_ctrl->ccif_md_base, ccif_ctrl);
	/*disable ccif clk*/
	ccif_set_clk_cg(hif_id, 0);
	CCCI_NORMAL_LOG(ccif_ctrl->md_id, TAG, "%s\n", __func__);
	return 0;
}

/*return ap_rt_data pointer after filling header*/
static void *ccif_hif_fill_rt_header(unsigned char hif_id, int packet_size,
	unsigned int tx_ch, unsigned int txqno)
{
	struct ccci_header *ccci_h;
	struct ccci_header ccci_h_bk;
	struct md_ccif_ctrl *md_ctrl =
		(struct md_ccif_ctrl *)ccci_hif_get_by_id(hif_id);

	ccci_h =
		(struct ccci_header *)&md_ctrl->ccif_sram_layout->up_header;
	/*header */
	ccif_write32(&ccci_h->data[0], 0, 0x00);
	ccif_write32(&ccci_h->data[1], 0, packet_size);
	ccif_write32(&ccci_h->reserved, 0, MD_INIT_CHK_ID);
	/*ccif_write32(&ccci_h->channel,0,CCCI_CONTROL_TX); */
	/*as Runtime data always be the first packet
	 * we send on control channel
	 */
	ccif_write32((u32 *) ccci_h + 2, 0, tx_ch);
	/*ccci_header need backup for log history*/
	ccci_h_bk.data[0] = ccif_read32(&ccci_h->data[0], 0);
	ccci_h_bk.data[1] = ccif_read32(&ccci_h->data[1], 0);
	*((u32 *)&ccci_h_bk + 2) = ccif_read32((u32 *) ccci_h + 2, 0);
	ccci_h_bk.reserved = ccif_read32(&ccci_h->reserved, 0);
	ccci_md_add_log_history(&md_ctrl->traffic_info, OUT,
		(int)txqno, &ccci_h_bk, 0);

	return (void *)&md_ctrl->ccif_sram_layout->ap_rt_data;
}

static struct ccci_hif_ops ccci_hif_ccif_ops = {
	.send_skb = &md_ccif_op_send_skb,
	.give_more = &md_ccif_op_give_more,
	.write_room = &md_ccif_op_write_room,
	.stop_queue = &md_ccif_stop_queue,
	.start_queue = &md_ccif_start_queue,
	.dump_status = &md_ccif_op_dump_status,

	.start = &ccif_start,
	.stop = &ccif_stop,
	.debug = &ccif_debug,
	.send_data = &md_ccif_send_data,
	.fill_rt_header = &ccif_hif_fill_rt_header,
};

static u64 ccif_dmamask = DMA_BIT_MASK(36);
static int ccif_hif_hw_init(struct device *dev, struct md_ccif_ctrl *md_ctrl)
{
	struct device_node *node = NULL;
	int idx = 0;
	int ret;

	if (!dev) {
		CCCI_ERROR_LOG(-1, TAG, "No ccif driver in dtsi\n");
		ret = -3;
		return ret;
	}

	if (!md_ctrl->plat_val.infra_ao_base) {
		CCCI_ERROR_LOG(-1, TAG, "No infra_ao register in dtsi\n");
		ret = -4;
		return ret;
	}

	node = dev->of_node;
	if (!node) {
		CCCI_ERROR_LOG(-1, TAG, "No ccif node in dtsi\n");
		ret = -3;
		return ret;
	}
	md_ctrl->ccif_ap_base = of_iomap(node, 0);
	md_ctrl->ccif_md_base = of_iomap(node, 1);

	md_ctrl->ap_ccif_irq0_id = irq_of_parse_and_map(node, 0);
	md_ctrl->ap_ccif_irq1_id = irq_of_parse_and_map(node, 1);

	md_ctrl->md_pcore_pccif_base =
		ioremap_nocache(MD_PCORE_PCCIF_BASE, 0x20);
	CCCI_BOOTUP_LOG(-1, TAG, "pccif:%x\n", MD_PCORE_PCCIF_BASE);

	/* Device tree using none flag to register irq,
	 * sensitivity has set at "irq_of_parse_and_map"
	 */
	md_ctrl->ap_ccif_irq0_flags = IRQF_TRIGGER_NONE;
	md_ctrl->ap_ccif_irq1_flags = IRQF_TRIGGER_NONE;
	ret = of_property_read_u32(dev->of_node,
		"mediatek,sram_size", &md_ctrl->sram_size);
	if (ret < 0)
		md_ctrl->sram_size = CCIF_SRAM_SIZE;
	md_ctrl->ccif_sram_layout =
		(struct ccif_sram_layout *)(md_ctrl->ccif_ap_base
		+ APCCIF_CHDATA);
	for (idx = 0; idx < ARRAY_SIZE(ccif_clk_table); idx++) {
		ccif_clk_table[idx].clk_ref = devm_clk_get(dev,
			ccif_clk_table[idx].clk_name);
		if (IS_ERR(ccif_clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(-1, TAG,
				 "ccif get %s failed\n",
					ccif_clk_table[idx].clk_name);
			ccif_clk_table[idx].clk_ref = NULL;
		}
	}
	dev->dma_mask = &ccif_dmamask;
	dev->coherent_dma_mask = ccif_dmamask;
	dev->platform_data = md_ctrl;
	node = of_find_compatible_node(NULL, NULL,
		"mediatek,md_ccif4");
	if (node) {
		md_ctrl->md_ccif4_base = of_iomap(node, 0);
		if (!md_ctrl->md_ccif4_base) {
			CCCI_ERROR_LOG(-1, TAG,
				"ccif4_base fail: 0x%p!\n",
				md_ctrl->md_ccif4_base);
			return -2;
		}
	}

	if (md_ctrl->ccif_ap_base == 0 ||
		md_ctrl->ccif_md_base == 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"ap_ccif_base:0x%p, ccif_md_base:0x%p\n",
			md_ctrl->ccif_ap_base,
			md_ctrl->ccif_md_base);
		return -2;
	}
	if (md_ctrl->ap_ccif_irq0_id == 0 ||
		md_ctrl->ap_ccif_irq1_id == 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"ccif_irq0:%d,ccif_irq1:%d\n",
			md_ctrl->ap_ccif_irq0_id, md_ctrl->ap_ccif_irq1_id);
		return -2;
	}

	CCCI_DEBUG_LOG(-1, TAG,
		"ap_ccif_base:0x%p, ccif_md_base:0x%p\n",
		md_ctrl->ccif_ap_base,
		md_ctrl->ccif_md_base);
	CCCI_DEBUG_LOG(-1, TAG, "ccif_irq0:%d,ccif_irq1:%d\n",
		md_ctrl->ap_ccif_irq0_id, md_ctrl->ap_ccif_irq1_id);
	ret = request_irq(md_ctrl->ap_ccif_irq0_id, md_ccif_isr,
			md_ctrl->ap_ccif_irq0_flags, "CCIF_AP_DATA", md_ctrl);
	if (ret) {
		CCCI_ERROR_LOG(md_ctrl->md_id, TAG,
			"request CCIF_AP_DATA IRQ0(%d) error %d\n",
			md_ctrl->ap_ccif_irq0_id, ret);
		return -1;
	}
	return 0;

}

static int ccci_ccif_syssuspend(void)
{

	return 0;
}

static void ccci_ccif_sysresume(void)
{
	struct ccci_modem *md;
	struct md_sys1_info *md_info;

	md = ccci_md_get_modem_by_id(0);
	if (md) {
		md_info = (struct md_sys1_info *)md->private_data;
		ccif_write32(md_info->ap_ccif_base, APCCIF_CON, 0x01);

	} else
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: get modem1 failed.", __func__);
}

static struct syscore_ops ccci_ccif_sysops = {
	.suspend = ccci_ccif_syssuspend,
	.resume = ccci_ccif_sysresume,
};

int ccci_ccif_hif_init(struct platform_device *pdev,
	unsigned char hif_id, unsigned char md_id)
{
	int i, ret;
	struct device_node *node_md;
	struct md_ccif_ctrl *md_ctrl;

	md_ctrl = kzalloc(sizeof(struct md_ccif_ctrl), GFP_KERNEL);
	if (!md_ctrl) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc hif_ctrl fail\n", __func__);
		return -1;
	}
	/* ccif_ctrl = md_ctrl; */
	INIT_WORK(&md_ctrl->ccif_sram_work, md_ccif_sram_rx_work);

	timer_setup(&md_ctrl->traffic_monitor, md_ccif_traffic_monitor_func, 0);
	md_ctrl->heart_beat_counter = 0;
	INIT_WORK(&md_ctrl->traffic_info.traffic_work_struct,
		md_ccif_traffic_work_func);

	md_ctrl->channel_id = 0;
	md_ctrl->md_id = md_id;
	md_ctrl->hif_id = hif_id;
	node_md = of_find_compatible_node(NULL, NULL,
		"mediatek,mddriver");
	of_property_read_u32(node_md,
		"mediatek,md_generation", &md_ctrl->plat_val.md_gen);
	md_ctrl->plat_val.infra_ao_base =
		syscon_regmap_lookup_by_phandle(node_md,
		"ccci-infracfg");
	atomic_set(&md_ctrl->reset_on_going, 1);
	atomic_set(&md_ctrl->wakeup_src, 0);
	atomic_set(&md_ctrl->ccif_irq_enabled, 1);
	atomic_set(&md_ctrl->ccif_irq1_enabled, 1);
	ccci_reset_seq_num(&md_ctrl->traffic_info);

	/*init queue */
	for (i = 0; i < QUEUE_NUM; i++) {
		md_ccif_queue_struct_init(&md_ctrl->txq[i],
			md_ctrl->hif_id, OUT, i);
		md_ccif_queue_struct_init(&md_ctrl->rxq[i],
			md_ctrl->hif_id, IN, i);
	}

	md_ctrl->ops = &ccci_hif_ccif_ops;
	md_ctrl->plat_dev = pdev;
	ret = ccif_hif_hw_init(&pdev->dev, md_ctrl);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci ccif hw init fail");
		return ret;
	}

	ccci_hif_register(md_ctrl->hif_id, (void *)md_ctrl, &ccci_hif_ccif_ops);

	/* register SYS CORE suspend resume call back */
	register_syscore_ops(&ccci_ccif_sysops);

	return 0;
}

int ccci_hif_ccif_probe(struct platform_device *pdev)
{
	int ret;

	ret = ccci_ccif_hif_init(pdev, CCIF_HIF_ID, MD_SYS1);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci ccif init fail");
		return ret;
	}

	return 0;
}

static const struct of_device_id ccci_ccif_of_ids[] = {
	{.compatible = "mediatek,ccci_ccif"},
	{}
};

static struct platform_driver ccci_hif_ccif_driver = {

	.driver = {
		.name = "ccci_hif_ccif",
		.of_match_table = ccci_ccif_of_ids,
	},

	.probe = ccci_hif_ccif_probe,
};

static int __init ccci_hif_ccif_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_hif_ccif_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci hif_ccif driver init fail %d",
			ret);
		return ret;
	}
	return 0;
}

static void __exit ccci_hif_ccif_exit(void)
{
}

module_init(ccci_hif_ccif_init);
module_exit(ccci_hif_ccif_exit);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci hif ccif driver");
MODULE_LICENSE("GPL");
