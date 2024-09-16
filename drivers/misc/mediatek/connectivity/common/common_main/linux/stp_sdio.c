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

/*! \file   "stp_sdio.c"
 * \brief
 *
 * detailed description
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

#define STP_SDIO_TXPERFDBG 1	/* depends on STP_SDIO_DBG_SUPPORT */
#define STP_SDIO_OWNBACKDBG 1	/* depends on STP_SDIO_DBG_SUPPORT */
#define STP_SDIO_NEW_IRQ_HANDLER 1

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/time.h>

#include "osal.h"
#include "stp_exp.h"
#include "hif_sdio.h"
#include "stp_sdio.h"
#include "stp_dbg.h"
#include "wmt_lib.h"
#include "wmt_exp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[STP SDIO]"

#define STPSDIO_LOG_LOUD    5
#define STPSDIO_LOG_DBG     4
#define STPSDIO_LOG_HINT    3
#define STPSDIO_LOG_INFO    2
#define STPSDIO_LOG_WARN    1
#define STPSDIO_LOG_ERR     0

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_RXDBG
#define STP_SDIO_RXDBG_COUNT (0x10UL)
#define STP_SDIO_RXDBG_COUNT_MASK (STP_SDIO_RXDBG_COUNT - 1)
struct stp_sdio_rxdbg {
	UINT32 ts;
	UINT32 bus_rxlen;
	UINT32 chisr_rxlen;
	UINT8 rx_pkt_buf[STP_SDIO_RX_BUF_SIZE];
};
#endif

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
#define STP_SDIO_TXDBG_COUNT (0x10UL)
#define STP_SDIO_TXDBG_COUNT_MASK (STP_SDIO_TXDBG_COUNT - 1)
struct stp_sdio_txdbg {
	UINT32 ts;
	UINT32 bus_txlen;
	UINT64 l_sec;
	ULONG l_nsec;
	UINT32 four_byte_align_len;
	UINT8 tx_pkt_buf[STP_SDIO_TX_ENTRY_SIZE];
};
#define STP_SDIO_TXDBG_MAX_SIZE (0x20)
#endif
OSAL_SLEEPABLE_LOCK fake_coredump_lock;
/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static INT32 stp_sdio_func0_reg_rw(INT32 direction,  UINT32 offset, UINT32 value);
static INT32 stp_sdio_func_reg_rw(INT32 direction,  UINT32 offset, UINT32 value);

static INT32 stp_sdio_irq(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx);
static INT32 stp_sdio_probe(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx,
		const MTK_WCN_HIF_SDIO_FUNCINFO *sdio_func_infop);
static INT32 stp_sdio_remove(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx);

static VOID stp_sdio_rx_wkr(struct work_struct *work);
static VOID stp_sdio_tx_wkr(struct work_struct *work);
#if STP_SDIO_OWN_THREAD
static INT32 stp_sdio_wait_for_msg(PVOID pvData);
static VOID stp_sdio_tx_rx_handling(PVOID pData);
#endif
static _osal_inline_ INT32 stp_sdio_host_info_deinit(PPUINT8 ppTxBuf, PPUINT8 ppRxBuf);
static _osal_inline_ INT32 stp_sdio_host_info_init(PPUINT8 ppTxBuf, PPUINT8 ppRxBuf);
static _osal_inline_ INT32 stp_sdio_host_info_op(INT32 opId);
static _osal_inline_ SDIO_PS_OP stp_sdio_get_own_state(VOID);
static _osal_inline_ INT32 stp_sdio_do_own_set(MTK_WCN_STP_SDIO_HIF_INFO *p_info);
static _osal_inline_ INT32 stp_sdio_do_own_clr(INT32 wait);
static _osal_inline_ VOID stp_sdio_check_tx_sanity(const MTK_WCN_STP_SDIO_HIF_INFO *p_info, const UINT32 id);
static _osal_inline_ VOID stp_sdio_tx_wkr_comp(MTK_WCN_STP_SDIO_HIF_INFO * const p_info);
static _osal_inline_ INT32 stp_sdio_ownback_poll(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx, UINT32 retry,
		UINT32 delay_us);
static _osal_inline_ VOID stp_sdio_txperf_dump(VOID);
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* Supported SDIO device table */
static const MTK_WCN_HIF_SDIO_FUNCINFO mtk_stp_sdio_id_tbl[] = {
	/* MT6618 *//* Not an SDIO standard class device */
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x018B, 1, STP_SDIO_BLK_SIZE)},
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x018C, 1, STP_SDIO_BLK_SIZE)},	/* 2-function */

	/* MT6619 *//* Not an SDIO standard class device */
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x6619, 1, STP_SDIO_BLK_SIZE)},

	/* MT6620 *//* Not an SDIO standard class device */
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x020B, 1, STP_SDIO_BLK_SIZE)},
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x020C, 1, STP_SDIO_BLK_SIZE)},	/* 2-function */

	/* MT6628 *//* Not an SDIO standard class device */
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x6628, 2, STP_SDIO_BLK_SIZE)},	/* 2-function */

	/* MT6630 *//* Not an SDIO standard class device */
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x6630, 2, STP_SDIO_BLK_SIZE)},	/* 2-function */

	/* MT6632 *//* Not an SDIO standard class device */
	{MTK_WCN_HIF_SDIO_FUNC(0x037A, 0x6632, 2, STP_SDIO_BLK_SIZE)},	/* 2-function */
	{ /* end: all zeroes */ },
};

wait_queue_head_t g_ownback_done;
INT32 is_wait_ownback;
/* static void (*cmb_bgf_eirq_cb)(void) = NULL; */

/* STP SDIO client information for hif sdio driver registration */
const MTK_WCN_HIF_SDIO_CLTINFO g_stp_sdio_cltinfo = {
	.func_tbl = mtk_stp_sdio_id_tbl,
	.func_tbl_size = (sizeof(mtk_stp_sdio_id_tbl) / sizeof(MTK_WCN_HIF_SDIO_FUNCINFO) - 1),
	.hif_clt_irq = stp_sdio_irq,
	.hif_clt_probe = stp_sdio_probe,
	.hif_clt_remove = stp_sdio_remove,
};

/* STP SDIO host array for multiple hosts maintenance */
MTK_WCN_STP_SDIO_HIF_INFO g_stp_sdio_host_info;	/*[STP_SDIO_HOST_COUNT]; */
/* STP SDIO host information pointer (for stp if_tx() function) */
MTK_WCN_STP_SDIO_HIF_INFO *const gp_info = &g_stp_sdio_host_info;

/* STP-SDIO probe count (not support multiple probe and hosts) */
UINT32 g_stp_sdio_host_count;

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_RXDBG
#define STP_SDIO_RXDBG_PROCNAME "driver/stp_sdio_rxdbg"
static struct proc_dir_entry *gStpSdioRxDbgEntry;
static INT32 stp_sdio_rxdbg_cnt;
static struct stp_sdio_rxdbg stp_sdio_rxdbg_buffer[STP_SDIO_RXDBG_COUNT];
static struct timeval old = {0};
#define TX_NO_ACK_TIMEOUT_ASSERT 5 /* tx no ack timeout assert, unit:second*/

static ssize_t stp_sdio_rxdbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t stp_sdio_rxdbg_write(struct file *filp, const char __user *buf, size_t count,
			     loff_t *f_pos);
static const struct file_operations stp_sdio_rxdbg_fops = {
	.read = stp_sdio_rxdbg_read,
	.write = stp_sdio_rxdbg_write,
};

#endif

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_OWNBACKDBG
#define STP_SDIO_OWNDBG_PROCNAME "driver/stp_sdio_own"
static struct proc_dir_entry *gStpSdioOwnEntry;
static ssize_t stp_sdio_own_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t stp_sdio_own_write(struct file *filp, const char __user *buf, size_t count,
			   loff_t *f_pos);
static const struct file_operations stp_sdio_own_fops = {
	.read = stp_sdio_own_read,
	.write = stp_sdio_own_write,
};

#endif

#if STP_SDIO_DBG_SUPPORT && (STP_SDIO_TXDBG || STP_SDIO_TXPERFDBG)
#define STP_SDIO_TXDBG_PROCNAME "driver/stp_sdio_txdbg"
static struct proc_dir_entry *gStpSdioTxDbgEntry;

static ssize_t stp_sdio_txdbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t stp_sdio_txdbg_write(struct file *filp, const char __user *buf, size_t count,
			     loff_t *f_pos);
static const struct file_operations stp_sdio_txdbg_fops = {
	.read = stp_sdio_txdbg_read,
	.write = stp_sdio_txdbg_write,
};

#if STP_SDIO_TXDBG
static INT32 stp_sdio_txdbg_cnt;
static struct stp_sdio_txdbg stp_sdio_txdbg_buffer[STP_SDIO_TXDBG_COUNT];
#endif

#if STP_SDIO_TXPERFDBG
/* a record for tx worker loop counter */
static UINT32 stp_sdio_txperf_worker_cnt;

/* a record for left fifo size in hw when tx wait */
static UINT32 stp_sdio_txperf_fifo_left;
/* a record for data length when tx wait */
static UINT32 stp_sdio_txperf_to_send;
/* a record for tx wait fifo limit counter */
static UINT32 stp_sdio_txperf_fifo_lmt_cnt;

/* a record for left txed pkt number in tx worker */
static UINT32 stp_sdio_txperf_txed_pkt_num;
/* a record for tx wait pkt_num limit counter */
static UINT32 stp_sdio_txperf_pkt_num_lmt_cnt;
#endif
#endif

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc WCN_SE_CS3");
MODULE_DESCRIPTION("Read-Copy Update tracing for hierarchical implementation");

INT32 gStpSdioDbgLvl = STPSDIO_LOG_INFO;
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define STPSDIO_PR_LOUD(fmt, arg...) \
do { \
	if (gStpSdioDbgLvl >= STPSDIO_LOG_LOUD) \
		pr_info(DFT_TAG "[L]%s:"  fmt, __func__, ##arg); \
} while (0)

#define STPSDIO_PR_DBG(fmt, arg...) \
do { \
	if (gStpSdioDbgLvl >= STPSDIO_LOG_DBG) \
		pr_info(DFT_TAG "[D]%s:"  fmt, __func__, ##arg); \
} while (0)

#define STPSDIO_PR_HINT(fmt, arg...) \
do { \
	if (gStpSdioDbgLvl >= STPSDIO_LOG_HINT) \
		pr_info(DFT_TAG "[I]%s:"	fmt, __func__, ##arg); \
} while (0)

#define STPSDIO_PR_INFO(fmt, arg...) \
do { \
	if (gStpSdioDbgLvl >= STPSDIO_LOG_INFO) \
		pr_info(DFT_TAG "[I]%s:"  fmt, __func__, ##arg); \
} while (0)

#define STPSDIO_PR_WARN(fmt, arg...)	\
do { \
	if (gStpSdioDbgLvl >= STPSDIO_LOG_WARN) \
		pr_warn(DFT_TAG "[W]%s:"  fmt, __func__, ##arg); \
} while (0)

#define STPSDIO_PR_ERR(fmt, arg...)	\
do { \
	if (gStpSdioDbgLvl >= STPSDIO_LOG_ERR) \
		pr_err(DFT_TAG "[E]%s(%d):"  fmt, __func__, __LINE__, ##arg); \
} while (0)

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
INT32 stp_sdio_deep_sleep_flag_set(MTK_WCN_BOOL flag)
{
	return mtk_wcn_hif_sdio_deep_sleep_flag_set(flag);
}
#endif

VOID stp_sdio_retry_flag_ctrl(INT32 flag)
{
	gp_info->retry_enable_flag = flag == 0 ? 0 : 1;
}

INT32 stp_sdio_retry_flag_get(VOID)
{
	return gp_info->retry_enable_flag;
}

static _osal_inline_ INT32 stp_sdio_host_info_op(INT32 opId)
{
	INT32 iRet = 0;
#if KMALLOC_UPDATE
	static PUINT8 p_tx_buffer;
	static PUINT8 p_rx_buffer;

	if (p_tx_buffer == NULL) {
		p_tx_buffer = kmalloc(STP_SDIO_TX_BUF_CNT * STP_SDIO_TX_ENTRY_SIZE, GFP_ATOMIC);
		if (p_tx_buffer == NULL) {
			STPSDIO_PR_ERR
			    ("memory allocate for g_stp_sdio_host_info.pkt_buf.tx_buf fail!\n");
			iRet = -1;
		} else {
			STPSDIO_PR_DBG
			    ("memory allocate for g_stp_sdio_host_info.pkt_buf.tx_buf succeed!\n");
			iRet = 0;
		}
	} else {
		STPSDIO_PR_DBG
		    ("memory already allocated for g_stp_sdio_host_info.pkt_buf.tx_buf!\n");
		iRet = 0;
	}
	if (p_rx_buffer == NULL) {
		p_rx_buffer = kmalloc(STP_SDIO_RX_BUF_SIZE, GFP_ATOMIC);
		if (p_rx_buffer == NULL) {
			STPSDIO_PR_ERR
			    ("memory allocate for g_stp_sdio_host_info.pkt_buf.rx_buf fail!\n");
			iRet = -1;
		} else {
			STPSDIO_PR_DBG
			    ("memory allocate for g_stp_sdio_host_info.pkt_buf.rx_buf succeed!\n");
			iRet = 0;
		}
	} else {
		STPSDIO_PR_DBG
		    ("memory already allocated for g_stp_sdio_host_info.pkt_buf.rx_buf!\n");
		iRet = 0;
	}
#endif /* KMALLOC_UPDATE */

	if (iRet == 0 && opId == 0)
		iRet = stp_sdio_host_info_deinit(&p_tx_buffer, &p_rx_buffer);
	else if (iRet == 0)
		iRet = stp_sdio_host_info_init(&p_tx_buffer, &p_rx_buffer);
	else
		STPSDIO_PR_ERR("iRet (%d)!\n", iRet);

	return iRet;
}

static _osal_inline_ INT32 stp_sdio_host_info_init(PPUINT8 ppTxBuf, PPUINT8 ppRxBuf)
{
	/* Init host count */
	memset(&g_stp_sdio_host_info, 0, sizeof(g_stp_sdio_host_info));

#if KMALLOC_UPDATE
	g_stp_sdio_host_info.pkt_buf.tx_buf = *ppTxBuf;
	g_stp_sdio_host_info.pkt_buf.rx_buf = *ppRxBuf;
#endif

	return 0;
}

static _osal_inline_ INT32 stp_sdio_host_info_deinit(PPUINT8 ppTxBuf, PPUINT8 ppRxBuf)
{
#if KMALLOC_UPDATE
	if (*ppTxBuf != NULL) {
		kfree(*ppTxBuf);
		*ppTxBuf = NULL;
	}
	if (*ppRxBuf != NULL) {
		kfree(*ppRxBuf);
		*ppRxBuf = NULL;
	}
#endif

	return 0;
}

INT32 stp_sdio_issue_fake_coredump(UINT8 *str)
{
#define MAX_STRING_LENGTH 140
	UINT8 AssertStr[4 + MAX_STRING_LENGTH + 1 + 2] = { 0 };
	UINT32 length = strlen(str) >= MAX_STRING_LENGTH ? MAX_STRING_LENGTH : strlen(str);
	/*pack str into STP SDIO packet format */
	osal_lock_sleepable_lock(&fake_coredump_lock);
	/*STP header */
	AssertStr[0] = 0x80;
	AssertStr[1] = 0x50;
	AssertStr[2] = (length + 1) & 0xff;
	AssertStr[3] = 0x0;
	/*STP content */
	memcpy(&AssertStr[4], str, length);
	/*string end character */
	AssertStr[4 + length] = '\n';
	/*STP CRC */
	AssertStr[4 + length + 1] = 0x0;
	AssertStr[4 + length + 2] = 0x0;
	/*send to STP layer coredump content */
	mtk_wcn_stp_parser_data(&AssertStr[0], 4 + length + 1 + 2);

	/*send coredump end content */
	length = strlen("coredump end");
	AssertStr[0] = 0x80;
	AssertStr[1] = 0x50;
	AssertStr[2] = (length + 2) & 0xff;
	AssertStr[3] = 0x0;
	memcpy(&AssertStr[4], "coredump end", length);
	/*string end character */
	AssertStr[4 + length] = '\r';
	AssertStr[4 + length + 1] = '\n';
	/*STP CRC */
	AssertStr[4 + length + 2] = 0x0;
	AssertStr[4 + length + 3] = 0x0;
	mtk_wcn_stp_parser_data(&AssertStr[0], 4 + length + 2 + 2);
	osal_unlock_sleepable_lock(&fake_coredump_lock);

	STPSDIO_PR_ERR("trigger fake coredump with str:[%s] finished\n", str);

	return 0;
}

/*!
 * \brief
 *
 * \details
 *
 * \retval 0 success
 * \retval !=0 fail
 */
static _osal_inline_ SDIO_PS_OP stp_sdio_get_own_state(VOID)
{
	SDIO_PS_OP ret = OWN_SET;
	INT32 i_ret = 0;
	UINT32 chlcpr_value = 0x0;
	MTK_WCN_HIF_SDIO_CLTCTX clt_ctx;

	clt_ctx = gp_info->sdio_cltctx;
	i_ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR, &chlcpr_value,
			0);
	if (i_ret) {
		ret = OWN_SET;
		STPSDIO_PR_ERR("read CHLPCR fail(%d), return\n", ret);
		return ret;
	}
	if ((chlcpr_value & C_FW_COM_DRV_OWN) == C_FW_COM_DRV_OWN)
		ret = OWN_CLR;
	else
		ret = OWN_SET;

	return ret;
}

static _osal_inline_ INT32 stp_sdio_do_own_set(MTK_WCN_STP_SDIO_HIF_INFO *p_info)
{
	UINT32 value = 0;
	UINT32 iRet = 0;

	/* [COHEC_00006052] SW work-around solution:
	 * using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR
	 */
#if COHEC_00006052
	value = (C_FW_OWN_REQ_SET >> 8);
	iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx,
			(UINT32)(CHLPCR + 0x01), &value, 0);
#else
	value = C_FW_OWN_REQ_SET;
	iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx, CHLPCR, &value,
			0);
#endif /* COHEC_00006052 */
	if (iRet) {
		STPSDIO_PR_ERR("write CHLPCR, set firmware own! (sleeping) fail\n");
	} else {
		iRet = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx, CHLPCR,
				&value, 0);
		if (iRet)
			STPSDIO_PR_ERR("get firmware own! (sleeping) fail iRet:%d\n", iRet);
		else {
			if (!(value & C_FW_COM_DRV_OWN)) {
				STPSDIO_PR_DBG("set firmware own! (sleeping) ok\n");
				osal_ftrace_print("set F own okay, sleep_flag(%d), wakeup_flag(%d), awake_falg(%d)",
						p_info->sleep_flag, p_info->wakeup_flag, p_info->awake_flag);
				p_info->awake_flag = 0;
				p_info->sleep_flag = 0;
				osal_raise_signal(&p_info->isr_check_complete);
				osal_ftrace_print("set F own okay and raise signal done");
			} else {
				STPSDIO_PR_WARN("set firmware own fail!, set CLR BACK\n");
				osal_ftrace_print("set F own fail, set CLR BACK\n");
				/* if set firmware own not successful(possibly pending interrupts),
				 * indicate an own clear event
				 */
				/* [COHEC_00006052] SW work-around solution:
				 * using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR
				 */
#if COHEC_00006052
				value = (C_FW_OWN_REQ_CLR >> 8);
				iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT,
						p_info->sdio_cltctx, (UINT32)(CHLPCR + 0x01), &value, 0);
#else
				value = C_FW_OWN_REQ_CLR;
				iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT,
						p_info->sdio_cltctx, CHLPCR, &value, 0);
#endif /* COHEC_00006052 */
			}
		}
	}

	return 0;
}

/*!
 * \brief
 *
 * \details
 *
 * \retval 0 success
 * \retval !=0 fail
 */
static _osal_inline_ INT32 stp_sdio_do_own_clr(INT32 wait)
{
#define CLR_OWN_RETRY 50
	INT32 ret = -1;
	UINT32 time_out;
	INT32 retry;
	UINT32 chlcpr_value = 0x0;
	UINT32 write_value = 0x0;
	UINT32 delay_us = 500;
	MTK_WCN_HIF_SDIO_CLTCTX clt_ctx;

	clt_ctx = gp_info->sdio_cltctx;
	retry = 40;

	/* <1> request firmware-own back */
	STPSDIO_PR_DBG("Do FW-Own back!(Wakeup)\n");
	if (wait) {
		/* TODO:[FixMe][George] why use both polling & interrupt methods here??? */
		init_waitqueue_head(&g_ownback_done);

		is_wait_ownback = 1;
	}
/* need to wait for the ownback completion */
/* [COHEC_00006052] SW work-around solution:
 * using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR
 */
	while (retry-- > 0) {
#if COHEC_00006052
		write_value = (C_FW_OWN_REQ_CLR>>8);
		ret = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, clt_ctx,
				(UINT32)(CHLPCR+0x1), &write_value, 0);
#else
		write_value = C_FW_OWN_REQ_CLR;
		ret = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
				&write_value, 0);
#endif /* COHEC_00006052 */
		if (ret) {
			STPSDIO_PR_ERR("request firmware own back fail(%d)\n", ret);
			osal_dbg_assert_aee("<HIF_SDIO> sdio_write ERROR",
					"write CHLPCR by SDIO report error");
			if (retry == 0)
				goto out;
		} else
			break;
	}
	retry = 1200;/*wait for 1200*500 = 600 000 us*/
	do {
		ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
				&chlcpr_value, 0);
		if (ret) {
			/* 4 <1.1> get CHISR Rx error handling */
			STPSDIO_PR_ERR("get CHLPCR information rx error!(%d)\n", ret);
			goto out;
		}

		if ((chlcpr_value & C_FW_COM_DRV_OWN) == C_FW_COM_DRV_OWN) {
			/* 4 <2> handle ownership back interrupt */
			STPSDIO_PR_DBG("firmware ownback is polled!(%d)\n",
					 CLR_OWN_RETRY - retry);
			/*osal_usleep_range(2000, 2000);*/
			break;
		}
		STPSDIO_PR_DBG("firmware ownback is no polled, wait for (%d us) and retry\n", delay_us);
		osal_usleep_range(delay_us, 2*delay_us);
		if (0 == (retry - 1)%40)
			STPSDIO_PR_ERR("own back failed in %d us, something might goes wrong\n",
					40*delay_us);

		if (0 == (retry - 1)%200) {
#if COHEC_00006052
			write_value = (C_FW_OWN_REQ_CLR>>8);
			ret = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, clt_ctx,
					(UINT32)(CHLPCR+0x1), &write_value, 0);
#else
			write_value = C_FW_OWN_REQ_CLR;
			ret = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
					&write_value, 0);
#endif /* COHEC_00006052 */
			if (ret)
				STPSDIO_PR_ERR("request firmware own back fail(%d)\n", ret);
			STPSDIO_PR_ERR("own back failed in %d us, write again\n", 200*delay_us);
			stp_dbg_poll_cpupcr(5, 1, 1);
		}
	} while (retry-- > 0);
	if (wait) {
		/* <2> wait own_back bit is set. */
		time_out = wait_event_timeout(g_ownback_done,
					      is_wait_ownback == 0, (1000 / (1000 / HZ)));

		if (time_out != 0) {
			STPSDIO_PR_INFO("Do FW-Own back!(Wakeup) succeed\n");
			ret = 0;
		} else {
			STPSDIO_PR_ERR("Do FW-Own back!(Wakeup) fail\n");
			ret = -1;
		}
	} else
		ret = retry > 0 ? 0 : -1;

out:
	return ret;
}

/*!
 * \brief
 *
 * \details
 *
 * \param[IN] op code for SDIO PS and OWN control
 *
 * \retval 0 success
 * \retval !=0 fail
 */
INT32 stp_sdio_own_ctrl(SDIO_PS_OP op)
{
	INT32 ret = -1;
	P_OSAL_SIGNAL pOsalSignal = NULL;

	if (g_stp_sdio_host_count == 0) {
		STPSDIO_PR_WARN("g_stp_sdio_host_count is 0, do nothing!\n");
		return 0;
	}
	pOsalSignal = &gp_info->isr_check_complete;
	switch (op) {
	case OWN_SET:
		gp_info->wakeup_flag = 0;
		gp_info->sleep_flag = 1;
		osal_trigger_event(&gp_info->tx_rx_event);
		STPSDIO_PR_LOUD("before op(%d)\n", op);
		osal_ftrace_print("own set start, wakeup_flag(%d), sleep_flag(%d), wait signal",
				gp_info->wakeup_flag, gp_info->sleep_flag);
		ret = osal_wait_for_signal_timeout(pOsalSignal, NULL);
		if (!ret)
			STPSDIO_PR_ERR("OWN_SET fail, done(%d), sleep(%d), wakeup(%d)\n",
				pOsalSignal->comp.done, gp_info->sleep_flag, gp_info->wakeup_flag);
		STPSDIO_PR_LOUD("after op(%d)\n", op);
		ret = 0;
		break;

	case OWN_CLR:
		/* ret = stp_sdio_do_own_clr(1); */
		gp_info->sleep_flag = 0;
		gp_info->wakeup_flag = 1;
		osal_trigger_event(&gp_info->tx_rx_event);
		osal_ftrace_print("own clear start, wakeup_flag(%d), sleep_flag(%d), wait signal",
				gp_info->wakeup_flag, gp_info->sleep_flag);
		STPSDIO_PR_LOUD("before op(%d)\n", op);
		ret = osal_wait_for_signal_timeout(pOsalSignal, NULL);
		if (!ret)
			STPSDIO_PR_ERR("OWN_CLER fail, done(%d), sleep(%d), wakeup(%d)\n",
				pOsalSignal->comp.done, gp_info->sleep_flag, gp_info->wakeup_flag);
		STPSDIO_PR_LOUD("after op(%d)\n", op);
		ret = 0;
		break;
	case OWN_STATE:
		/* ret = stp_sdio_get_own_state(); */
		STPSDIO_PR_INFO("omit op(%d)\n", op);
		ret = 0;
		break;
	default:
		STPSDIO_PR_WARN("omit op(%d)\n", op);
		ret = -1;
		break;
	}
	osal_ftrace_print("own control end, wakeup_flag(%d), sleep_flag(%d), signal done",
			gp_info->wakeup_flag, gp_info->sleep_flag);
	return ret;
}

#if STP_SDIO_OWN_THREAD

static INT32 stp_sdio_wait_for_msg(PVOID pvData)
{
	MTK_WCN_STP_SDIO_HIF_INFO *pInfo = (MTK_WCN_STP_SDIO_HIF_INFO *) pvData;
#if STP_SDIO_NEW_TXRING
	STPSDIO_PR_LOUD
	    ("len(%u), wr_cnt(%u), rd_cnt(%u), irq_pending(%u), sleep(%u), wakeup(%u)\n",
	     pInfo->rx_pkt_len, atomic_read(&pInfo->pkt_buf.wr_cnt), atomic_read(&pInfo->pkt_buf.rd_cnt),
	     pInfo->irq_pending, pInfo->sleep_flag, pInfo->wakeup_flag);
	osal_ftrace_print("len(%u), txwkr(%u), irq(%u), sleep(%u), wakeup(%u), tx_pkt_num(%u))\n",
	     pInfo->rx_pkt_len, pInfo->txwkr_flag, pInfo->irq_pending, pInfo->sleep_flag, pInfo->wakeup_flag,
	     pInfo->firmware_info.tx_packet_num);
	return (pInfo->rx_pkt_len != 0)
	    || (pInfo->txwkr_flag != 0)
	    || (pInfo->irq_pending != 0)
	    || (pInfo->sleep_flag != 0)
	    || (pInfo->wakeup_flag != 0)
	    || (osal_thread_should_stop(&pInfo->tx_rx_thread));
#else
	STPSDIO_PR_LOUD
	    ("len(%u), rd_idx(%u), wr_idx(%u), irq_pending(%u), sleep(%u), wakeup(%u)\n",
	     pInfo->rx_pkt_len, atomic_read(&pInfo->pkt_buf.rd_idx), atomic_read(&pInfo->pkt_buf.wr_idx),
	     pInfo->irq_pending, pInfo->sleep_flag, pInfo->wakeup_flag);
	osal_ftrace_print("len(%u), txwkr(%u), irq(%u), sleep(%u), wakeup(%u), tx_pkt_num(%u)\n",
	     pInfo->rx_pkt_len, pInfo->txwkr_flag, pInfo->irq_pending, pInfo->sleep_flag, pInfo->wakeup_flag,
	     pInfo->firmware_info.tx_packet_num);
	return (pInfo->rx_pkt_len != 0)
	    || (pInfo->txwkr_flag != 0)
	    || (pInfo->irq_pending != 0)
	    || (pInfo->sleep_flag != 0)
	    || (pInfo->wakeup_flag != 0)
	    || (osal_thread_should_stop(&pInfo->tx_rx_thread));
#endif /* STP_SDIO_NEW_TXRING */
}

static VOID stp_sdio_tx_rx_handling(PVOID pData)
{
	MTK_WCN_STP_SDIO_HIF_INFO *pInfo = (MTK_WCN_STP_SDIO_HIF_INFO *) pData;
	UINT32 iRet = 0;
	UINT32 chisr = 0;
	UINT32 chlcpr_value = 0;
	UINT32 write_value = 0;
	UINT32 tx_comp = 0;
	MTK_WCN_HIF_SDIO_CLTCTX clt_ctx;
	UINT32 while_loop_counter = 0;
	UINT32 own_fail_counter = 0;
	UINT32 val = 0;
	UINT32 delay_us = 10000;
	UINT32 chisr_error_count = 3;

	STPSDIO_PR_INFO("stp_tx_rx_thread start running...\n");
	if (pInfo == NULL) {
		STPSDIO_PR_WARN("sanity check fail, (pInfo == NULL)\n");
		return;
	}
	clt_ctx = pInfo->sdio_cltctx;

	while (!osal_thread_should_stop(&pInfo->tx_rx_thread)) {
		while_loop_counter++;
		osal_ftrace_print("loop_count:%d\n", while_loop_counter);
		/* <0> get CHLPCR information */
		if (pInfo->awake_flag == 0) {
			if (CLTCTX_CID(clt_ctx) == 0x6632)
				stp_sdio_wake_up_ctrl(clt_ctx);
			if (stp_sdio_do_own_clr(0) == 0) {
				STPSDIO_PR_DBG("set OWN to driver side ok!\n");
				pInfo->awake_flag = 1;
				osal_ftrace_print("set OWN D ok!, sleep_flag(%d), wakeup_flag(%d), awake_falg(%d)",
						pInfo->sleep_flag, pInfo->wakeup_flag, pInfo->awake_flag);
				own_fail_counter = 0;
			} else {
				if ((pInfo->sleep_flag != 0) || (pInfo->wakeup_flag != 0)) {
					pInfo->wakeup_flag = 0;
					pInfo->sleep_flag = 0;
					STPSDIO_PR_WARN("clr fw own fail,signal for sleep/wakeup to return\n");
					osal_raise_signal(&pInfo->isr_check_complete);
				}
				if ((own_fail_counter % 50) == 0) {
					STPSDIO_PR_ERR("set OWN to driver side error!\n");
					/*trigger whole chip reset by send fake coredump content */
					if (own_fail_counter == 0)
						stp_sdio_issue_fake_coredump
						    ("ABT: STP-SDIO clear f/w own failed");
				}
				own_fail_counter++;
			}
		} else {
			STPSDIO_PR_DBG("OWN on driver side!\n");
			osal_ftrace_print("OWN D , sleep_flag(%d), wakeup_flag(%d), awake_falg(%d)",
						pInfo->sleep_flag, pInfo->wakeup_flag, pInfo->awake_flag);
		}

		if ((pInfo->wakeup_flag != 0) && (pInfo->awake_flag != 0)) {
			while_loop_counter = 0;
			STPSDIO_PR_DBG("clr firmware own! (wakeup) ok\n");
			pInfo->wakeup_flag = 0;
			osal_raise_signal(&pInfo->isr_check_complete);
			osal_ftrace_print("wakeup done\n");
		}

		if ((pInfo->irq_pending != 0) && (pInfo->awake_flag == 1)) {
			while_loop_counter = 0;

			/* <1> get CHISR information */
			iRet = mtk_wcn_hif_sdio_readl(clt_ctx, CHISR, &chisr);
			if (iRet) {
				/* 4       <1.1> get CHISR Rx error handling */
				/* TODO: error handling! */
				STPSDIO_PR_ERR("get CHISR information rx error, ret:%d\n", iRet);
				if (-5 == iRet) {
					iRet = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_MAX_RETRY_NUM,
							clt_ctx, CSR, &chisr, 0);
					pInfo->irq_pending = 0;
				}
			} else {
				pInfo->irq_pending = 0;
				osal_ftrace_print("CHISR(0x%08x)\n", chisr);
			}

			if (pInfo->dump_flag != 0)
				STPSDIO_PR_ERR("CHISR(0x%08x)\n", chisr);
			else
				STPSDIO_PR_DBG("CHISR(0x%08x)\n", chisr);


			if (chisr == 0x0) {
				STPSDIO_PR_ERR("******CHISR == 0*****\n");
				while (chisr_error_count) {
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CCIR, &val, 0);
					STPSDIO_PR_ERR("******CCIR == 0x%x*****\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CHLPCR, &val, 0);
					STPSDIO_PR_ERR("******CHLPCR == 0x%x*****\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CSDIOCSR, &val, 0);
					STPSDIO_PR_ERR("******CSDIOCSR == 0x%x*****\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CHCR, &val, 0);
					STPSDIO_PR_ERR("******CHCR == 0x%x*****\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CHISR, &chisr, 0);
					STPSDIO_PR_ERR("******CHISR == 0x%x*****\n", chisr);
					if (chisr)
						break;
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CHIER, &val, 0);
					STPSDIO_PR_ERR("******CHIER == 0x%x*****\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CTFSR, &val, 0);
					STPSDIO_PR_ERR("******CTFSR == 0x%x*****\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							  clt_ctx, CRPLR, &val, 0);
					STPSDIO_PR_ERR("******CRPLR == 0x%x*****\n", val);
					chisr_error_count--;
					osal_usleep_range(delay_us, 2 * delay_us);
				}
			}

			if (chisr & FW_OWN_BACK_INT)
				STPSDIO_PR_HINT("FW_OWN_BACK_INT\n");

			/* <4> handle Tx interrupt */
			if ((chisr & TX_EMPTY) || (chisr & TX_UNDER_THOLD) || (chisr & TX_RETRY)) {
				while_loop_counter = 0;
				STPSDIO_PR_DBG("Tx interrupt\n");
				/* get complete count */
				tx_comp = (chisr & TX_COMPLETE_COUNT) >> 4;

				tx_comp =
				    atomic_add_return(tx_comp, &pInfo->firmware_info.tx_comp_num);
				if (tx_comp > STP_SDIO_TX_PKT_MAX_CNT)
					STPSDIO_PR_ERR
					    ("Abnormal accumulated comp count(%d) chisr(0x%x)\n",
					     tx_comp, chisr);
				if (chisr & TX_RETRY)
					pInfo->tx_retry_flag = STP_SDIO_RETRY_INT;
			}
			if (pInfo->awake_flag == 1 && pInfo->tx_retry_flag != STP_SDIO_RETRY_CRC_ERROR)
				stp_sdio_tx_wkr(&pInfo->tx_work);

			if (chisr & RX_DONE) {
				/* STPSDIO_PR_INFO("RX_DONE_INT\n"); */
				if (pInfo->rx_pkt_len)
					STPSDIO_PR_ERR("rx worker is not finished yet!\n");

				/* get Rx packet length */
				pInfo->rx_pkt_len = (chisr & RX_PKT_LEN) >> 16;
				STPSDIO_PR_HINT("rx_pkt_len(%d)\n", pInfo->rx_pkt_len);
				/* sanity check */
				if ((pInfo->rx_pkt_len == 0) ||
						(pInfo->rx_pkt_len > STP_SDIO_RX_FIFO_SIZE)) {
					STPSDIO_PR_ERR
					    ("abnormal rx_pkt_len(%d) in CHISR(0x%08x) skip rx_worker\n",
					     pInfo->rx_pkt_len, chisr);
					pInfo->rx_pkt_len = 0;
				} else {
					/* Before host driver read all rx data, chip/fw will not send more
					 * data to host. No need to mask rx interrupt. schedule rx worker to
					 * get data back and handle it.
					 */
					if (pInfo->rx_pkt_len & 0x3)
						STPSDIO_PR_WARN("rx len not 4 bytes, CHISR(0x%08x), rx len (%d).\n",
								chisr, pInfo->rx_pkt_len);
				}
				/*Rx job */
				stp_sdio_rx_wkr(&pInfo->rx_work);
				mtk_wcn_stp_wmt_sdio_host_awake();
			}

		}

		/* We schedule Tx job here without condition */
		/*Tx job */
		if (pInfo->awake_flag == 1 && pInfo->tx_retry_flag != STP_SDIO_RETRY_CRC_ERROR)
			stp_sdio_tx_wkr(&pInfo->tx_work);

		/*Enable IRQ */
		/*Disable Common interrupt output in CHLPCR */
		STPSDIO_PR_DBG("enable COM IRQ\n");
		osal_ftrace_print("enable COM IRQ\n");
/* need to wait for the ownback completion */
/* [COHEC_00006052] SW work-around solution:
 * using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR
 */
#if COHEC_00006052
		write_value = C_FW_INT_EN_SET;
		iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
				&write_value, 0);
#else
		write_value = C_FW_INT_EN_SET;
		iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
				&write_value, 0);
#endif /* COHEC_00006052 */
		if (iRet) {
			STPSDIO_PR_ERR("enable IRQ fail. iRet:%d\n", iRet);
			osal_ftrace_print("enable COM IRQ fail, iRet:%d\n", iRet);
		} else {
			STPSDIO_PR_HINT("enable COM IRQ\n");
			iRet = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
					&chlcpr_value, 0);
			if (iRet) {
				STPSDIO_PR_ERR("read CHLPCR fail. iRet:%d\n", iRet);
			} else {
				if (chlcpr_value & C_FW_INT_EN_SET) {
					STPSDIO_PR_HINT("enable COM IRQ okay (0x%x)\n",
							  chlcpr_value);
					osal_ftrace_print("enable COM IRQ done\n");

					/* INTR_STATUS CHECK */
#if 0
					write_value = 0x00000002;
					iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT,
							clt_ctx, CHCR, &write_value, 0);

					iRet = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT,
							clt_ctx, CHISR, &chisr, 0);
					STPSDIO_PR_INFO("Query CHISR(0x%08x)\n", chisr);

					write_value = 0x00000000;
					iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT,
							clt_ctx, CHCR, &write_value, 0);
#endif
				}
			}

		}

		if ((pInfo->sleep_flag != 0)
			&& (pInfo->wakeup_flag == 0)
			&& (pInfo->irq_pending == 0)
			&& (pInfo->rx_pkt_len == 0)
			&& mtk_wcn_stp_is_ready()
			&& !mtk_wcn_stp_coredump_start_get()	/* f/w assert disable sdio sleep */
			) {
			if (pInfo->firmware_info.tx_packet_num == 0) {
				/* pInfo->awake_flag = 0; */
				/* STPSDIO_PR_INFO("set firmware own! (sleeping)\n"); */
				while_loop_counter = 0;
				osal_ftrace_print("|S|set F own\n");
				stp_sdio_do_own_set(pInfo);
				osal_ftrace_print("|E|set F own, sleep_flag(%d), wakeup_flag(%d), awake_falg(%d)",
						pInfo->sleep_flag, pInfo->wakeup_flag, pInfo->awake_flag);
			} else {
#if 0
				/* debug code */
				if (while_loop_counter > 150) {
					osal_ftrace_print("assert, w_l_c > %u\n", while_loop_counter);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx,
							CHLPCR, &val, 0);
					osal_ftrace_print("CHLPCR(0x%x)\n", val);
					stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx,
							CHISR, &val, 0);
					osal_ftrace_print("CHISR(0x%x)\n", val);
					iRet = wmt_core_trigger_stp_assert();
					if (!iRet)
						STPSDIO_PR_INFO("trigger assert fail\n");
				}
#endif
				/*Avoid competition for SDIO operations with ksdioirqd/mmc2 thread*/
				osal_usleep_range(300, 500);
			}
		}
		if (while_loop_counter > 1000) {
			while_loop_counter = 0;
			pInfo->dump_flag = 1;
			STPSDIO_PR_ERR("stp_sdio_tx_rx thread while_loop_counter over1000\n");
			stp_sdio_dump_info(pInfo);
			osal_ftrace_print("stp(%d)irq(%d)tx_pkt_num(%d)rx_pkt_len(%d)",
					mtk_wcn_stp_is_ready(), pInfo->irq_pending,
					pInfo->firmware_info.tx_packet_num, pInfo->rx_pkt_len);
			/*make fake irq flag to dump CHISR information */
			pInfo->irq_pending = 1;

			if ((pInfo->sleep_flag != 0) || (pInfo->wakeup_flag != 0)) {
				osal_ftrace_print("loop_count > 1000, sleep_flag(%d), wakeup_flag(%d), awake_flag(%d)",
							pInfo->sleep_flag, pInfo->wakeup_flag, pInfo->awake_flag);
				/*clear wakeup/sleep pending flag, wakeup wmtd thread */
				pInfo->wakeup_flag = 0;
				pInfo->sleep_flag = 0;
				osal_raise_signal(&pInfo->isr_check_complete);
				if (pInfo->firmware_info.tx_packet_num == STP_SDIO_TX_PKT_MAX_CNT) {
					STPSDIO_PR_ERR("STP-SDIO can't handle tx request. tx_packet_num:%d\n",
							pInfo->firmware_info.tx_packet_num);
					osal_sleep_ms(200);
				}
			}
		} else {
			pInfo->dump_flag = 0;
		}
		osal_wait_for_event(&pInfo->tx_rx_event, stp_sdio_wait_for_msg, (PVOID) pInfo);
		/* Read Packet from Firmware until firmware rx packet empty */
		STPSDIO_PR_DBG("stp_tx_rx_thread receive signal\n");
	}
	/*make sure thread who is waiting for STP-SDIO's sleep/wakeup completion event */
	if ((pInfo->sleep_flag != 0) || (pInfo->wakeup_flag != 0)) {
		osal_ftrace_print("wait for sleep/wakeup event, sleep_flag(%d), wakeup_flag(%d), awake_falg(%d)",
				pInfo->sleep_flag, pInfo->wakeup_flag, pInfo->awake_flag);
		pInfo->wakeup_flag = 0;
		pInfo->sleep_flag = 0;
		STPSDIO_PR_WARN("someone is wait for sleep/wakeup event, signal it to return\n");
		osal_raise_signal(&pInfo->isr_check_complete);
	}
	STPSDIO_PR_INFO("stp_tx_rx_thread exit\n");
}
#endif /* STP_SDIO_OWN_THREAD */

/*!
 * \brief Tx callback function for STP-CORE module
 *
 * \details A function registered to STP-CORE to provide STP-SDIO tx method.
 *      Multiple STP packet may be aggregated when available.
 *
 * \param[IN] data STP packet buffer to be sent through STP-SDIO
 * \param[IN] size STP packet length to be sent
 * \param[OUT] written_size Accepted buffer length by STP-SDIO. Shall be $size
 *      if success.
 *
 *\note Tx may do aggregation to previous entry with lock protection. If no
 *      aggregation is done, protection is NOT required.
 *
 * \retval 0 success
 * \retval -1 invalid input parameters
 * \todo return !0 when fail case (TBD)
 */
#if STP_SDIO_NEW_TXRING
INT32 stp_sdio_tx(const PUINT8 data, const UINT32 size, PUINT32 written_size)
{
	PUINT8 pkt_bufp;
	UINT32 prev_wr_idx;
	UINT32 prev_size;
	MTK_WCN_STP_SDIO_PKT_BUF *pb = NULL;
	UINT32 idx;

	osal_ftrace_print("%s|S|L|%d\n", __func__, size);
	if (written_size)
		*written_size = 0;

	/* do sanity check */
	if ((size > STP_SDIO_TX_FIFO_SIZE)
	    || (!data)) {
		STPSDIO_PR_LOUD("invalid size(%ld) > fifo(%d) or data(0x%p)\n",
				  size, STP_SDIO_TX_FIFO_SIZE, data);
		return -1;
	}

	pb = &gp_info->pkt_buf;
	/* 4 <1> enqueue the stp Tx packet */

	spin_lock_irqsave(&pb->rd_cnt_lock, pb->rd_irq_flag);
	/* Case 1: buffer is empty (No aggregation). Full flag is useless in this
	 * condition.
	 */
	if (atomic_read(&pb->rd_cnt) == atomic_read(&pb->wr_cnt)) {
		spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);
		/* Set the size in SDIO packet header later in tx_worker, not here */
		idx = atomic_read(&pb->wr_cnt) & STP_SDIO_TX_BUF_CNT_MASK;
		pb->tx_buf_ts[idx] = jiffies;
		pb->tx_buf_sz[idx] = size + STP_SDIO_HDR_SIZE;
#if KMALLOC_UPDATE
		pkt_bufp = pb->tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + STP_SDIO_HDR_SIZE;
#else
		pkt_bufp = &pb->tx_buf[idx][STP_SDIO_HDR_SIZE];
#endif
		memcpy(pkt_bufp, data, size);
		gp_info->txwkr_flag = 1;
		atomic_inc(&pb->wr_cnt);

		if (written_size)
			*written_size = size;
		STPSDIO_PR_DBG("(Empty) Enqueue done\n");
	}
	/* Case 2: buffer is neither empty(w != r) nor full (w-r < s) */
	else if ((atomic_read(&pb->wr_cnt) - atomic_read(&pb->rd_cnt)) < STP_SDIO_TX_BUF_CNT) {
		prev_wr_idx = (atomic_read(&pb->wr_cnt) - 1) & STP_SDIO_TX_BUF_CNT_MASK;
		prev_size = pb->tx_buf_sz[prev_wr_idx];

		/* Case 2.1 Aggregate if rd_cnt+1 != wr_cnt */
		/* George: do length check using add instead of sub operation. Compare
		 * to FIFO size instead of sw entry size.
		 */
		if (((atomic_read(&pb->rd_cnt) + 1) != atomic_read(&pb->wr_cnt))
		    && ((size + prev_size) <= STP_SDIO_TX_FIFO_SIZE)) {
#if KMALLOC_UPDATE
			pkt_bufp = pb->tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + prev_size;
#else
			pkt_bufp = &pb->tx_buf[prev_wr_idx][prev_size];
#endif
			pb->tx_buf_sz[prev_wr_idx] += size;

			memcpy(pkt_bufp, data, size);
			spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);

			STPSDIO_PR_DBG("(Not empty-aggre) Enqueue done\n");

			if (written_size)
				*written_size = size;
		}
		/* Case 2.2 Use next entry w/o aggregation  */
		else {
			/* Check the ring buf is full or not */
			if ((atomic_read(&pb->wr_cnt) - atomic_read(&pb->rd_cnt)) == 1)
				pb->full_flag = MTK_WCN_BOOL_TRUE;
			spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);

			/* George: if tx_wkr preempts here and pop out all buffered entries,
			 * ie increase rd_idx utill wr_idx, tx_wkr will encounter buffer
			 * empty condition and stop, then being scheduled before end of this
			 * function. It's safe!
			 */
			idx = atomic_read(&pb->wr_cnt) & STP_SDIO_TX_BUF_CNT_MASK;
			pb->tx_buf_ts[idx] = jiffies;
			pb->tx_buf_sz[idx] = size + STP_SDIO_HDR_SIZE;
#if KMALLOC_UPDATE
			pkt_bufp = pb->tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + STP_SDIO_HDR_SIZE;
#else
			pkt_bufp = &pb->tx_buf[idx][STP_SDIO_HDR_SIZE];
#endif
			memcpy(pkt_bufp, data, size);
			gp_info->txwkr_flag = 1;
			atomic_inc(&pb->wr_cnt);

			STPSDIO_PR_DBG("(Not empty-no aggre) Enqueue done\n");
			if (written_size)
				*written_size = size;
		}
	}
	/* Case 3: buffer is full (w-r >= s), (try aggregation) */
	else {
		if (!((atomic_read(&pb->wr_cnt) - atomic_read(&pb->rd_cnt)) >= STP_SDIO_TX_BUF_CNT)) {
			STPSDIO_PR_ERR
			    ("abnormal condition and flow, wr_cnt(0x%x), rd_cnt(0x%x)\n",
			     atomic_read(&pb->wr_cnt), atomic_read(&pb->rd_cnt));
		}
		prev_wr_idx = (atomic_read(&pb->wr_cnt) - 1) & STP_SDIO_TX_BUF_CNT_MASK;
		prev_size = pb->tx_buf_sz[prev_wr_idx];

		/* George: do length check using add instead of sub operation. Compare
		 * to FIFO size instead of sw entry size. (buf_allocation != 0) shall be
		 * an assert true condition, not a if () condition......
		 */
		/* Case 3.1 Aggregation */
		if ((size + prev_size) <= STP_SDIO_TX_FIFO_SIZE) {
			if (prev_size != 0)
				STPSDIO_PR_ERR("abnormal, wr_cnt(0x%x), rd_cnt(0x%x), prev(%d), prev_size(%d)\n",
						atomic_read(&pb->wr_cnt), atomic_read(&pb->rd_cnt),
						prev_wr_idx, prev_size);
#if KMALLOC_UPDATE
			pkt_bufp = pb->tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + prev_size;
#else
			pkt_bufp = &pb->tx_buf[prev_wr_idx][prev_size];
#endif
			pb->tx_buf_sz[prev_wr_idx] += size;

			/* unlock after copying data to ring buffer done so that rd_cnt can
			 * move forward and may overlap to prev_wr_idx.
			 */
			memcpy(pkt_bufp, data, size);
			spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);

			STPSDIO_PR_DBG("(full-aggre) Enqueue done\n");

			if (written_size)
				*written_size = size;
		}
		/* Case 3.2 Buffer is full */
		else {
			spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);
			STPSDIO_PR_WARN("Local Tx buffer is full !\n");

			/* Wait for tx ring buffer is not full */
			/* TODO:[FixMe][George] This wait() call IS a problem if caller runs
			 * in interrupt context (sw or hw) !!
			 */
			/* TODO:[FixMe][George] should use timeout version,
			 * not interruptible version. Return error when timeout!
			 */
			wait_event_interruptible(pb->fullwait_q,
						 (pb->full_flag == MTK_WCN_BOOL_FALSE));
			STPSDIO_PR_INFO("wait event return\n");

			spin_lock_irqsave(&pb->rd_cnt_lock, pb->rd_irq_flag);
			/* Check if the local buf is free enough */
			if ((atomic_read(&pb->wr_cnt) - atomic_read(&pb->rd_cnt)) == 1)
				pb->full_flag = MTK_WCN_BOOL_TRUE;
			spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);

			/* George: use this new entry w/o protection */
			idx = atomic(&pb->wr_cnt) & STP_SDIO_TX_BUF_CNT_MASK;
			pb->tx_buf_ts[idx] = jiffies;
			pb->tx_buf_sz[idx] = size + STP_SDIO_HDR_SIZE;
#if KMALLOC_UPDATE
			pkt_bufp = pb->tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + STP_SDIO_HDR_SIZE;
#else
			pkt_bufp = &pb->tx_buf[idx][STP_SDIO_HDR_SIZE];
#endif
			/* Copy data to ring buffer */
			memcpy(pkt_bufp, data, size);
			gp_info->txwkr_flag = 1;
			atomic_inc(&pb->wr_cnt);

			if (written_size)
				*written_size = size;
		}
	}

	/* <2> schedule for Tx worker tasklet */
	osal_ftrace_print("%s|E|signal stp_sdio_tx_rx\n", __func__);
#if STP_SDIO_OWN_THREAD
	/* tasklet_schedule(&gp_info->tx_rx_job); */
	STPSDIO_PR_DBG("osal_trigger_event gp_info->tx_rx_event\n");
	osal_trigger_event(&gp_info->tx_rx_event);
#else
	schedule_work(&gp_info->tx_work);
#endif
	osal_ftrace_print("%s|E|L|%d\n", __func__, size);

	return 0;
}

#else
INT32 stp_sdio_tx(const PUINT8 data, const UINT32 size, PUINT32 written_size)
{
	PUINT8 pkt_bufp;
	UINT32 prev_wr_idx;
	UINT32 buf_allocation;
	UINT32 room;
	UINT64 ts;
	ULONG nsec;

	osal_ftrace_print("%s|S|L|%d\n", __func__, size);

	/* 4 <1> enqueue the stp Tx packet */
	*written_size = 0;

	spin_lock_irqsave(&gp_info->pkt_buf.rd_idx_lock, gp_info->pkt_buf.rd_irq_flag);
	/* Case 1: buffer is empty (Not aggregation) */
	if ((atomic_read(&gp_info->pkt_buf.rd_idx) == atomic_read(&gp_info->pkt_buf.wr_idx))
	    && (gp_info->pkt_buf.full_flag == MTK_WCN_BOOL_FALSE)) {
		spin_unlock_irqrestore(&gp_info->pkt_buf.rd_idx_lock, gp_info->pkt_buf.rd_irq_flag);
		/* set the size in SDIO packet header */
#if KMALLOC_UPDATE
		*(gp_info->pkt_buf.tx_buf + atomic_read(&gp_info->pkt_buf.wr_idx) *
				STP_SDIO_TX_ENTRY_SIZE + 0) = (UINT8)((size + STP_SDIO_HDR_SIZE) & 0xff);
		*(gp_info->pkt_buf.tx_buf + atomic_read(&gp_info->pkt_buf.wr_idx) *
				STP_SDIO_TX_ENTRY_SIZE + 1) = (UINT8)((size + STP_SDIO_HDR_SIZE) >> 8);
#else
		gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][0] =
		    (UINT8) ((size + STP_SDIO_HDR_SIZE) & 0xff);
		gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][1] =
		    (UINT8) ((size + STP_SDIO_HDR_SIZE) >> 8);
#endif
		gp_info->pkt_buf.tx_buf_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = jiffies;
		osal_get_local_time(&ts, &nsec);
		gp_info->pkt_buf.tx_buf_local_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = ts;
		gp_info->pkt_buf.tx_buf_local_nsec[atomic_read(&gp_info->pkt_buf.wr_idx)] = nsec;

		STPSDIO_PR_DBG("(Empty) Enqueue done\n");
#if KMALLOC_UPDATE
		pkt_bufp = gp_info->pkt_buf.tx_buf + atomic_read(&gp_info->pkt_buf.wr_idx) *
			STP_SDIO_TX_ENTRY_SIZE + STP_SDIO_HDR_SIZE;
#else
		pkt_bufp = &gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][STP_SDIO_HDR_SIZE];
#endif
		memcpy(pkt_bufp, data, size);
		*written_size = size;
		gp_info->txwkr_flag = 1;
		atomic_set(&gp_info->pkt_buf.wr_idx,
				(atomic_read(&gp_info->pkt_buf.wr_idx) + 1) % STP_SDIO_TX_BUF_CNT);
	}
	/* Case 2: buffer is not empty */
	else if (atomic_read(&gp_info->pkt_buf.rd_idx) != atomic_read(&gp_info->pkt_buf.wr_idx)) {
		prev_wr_idx = (atomic_read(&gp_info->pkt_buf.wr_idx) - 1 + STP_SDIO_TX_BUF_CNT) %
			STP_SDIO_TX_BUF_CNT;
		/* set the packet size form previous SDIO packet header */
#if KMALLOC_UPDATE
		buf_allocation =
		    *(gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 1);
		buf_allocation =
		    (buf_allocation << 8) | *(gp_info->pkt_buf.tx_buf +
					      prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 0);
#else
		buf_allocation = gp_info->pkt_buf.tx_buf[prev_wr_idx][1];
		buf_allocation = (buf_allocation << 8) | gp_info->pkt_buf.tx_buf[prev_wr_idx][0];
#endif
		/* Case 2.1 Aggregation */
		/* George: do length check using add instead of sub operation. Compare
		 * to FIFO size instead of sw entry size.
		 */
		if ((prev_wr_idx != atomic_read(&gp_info->pkt_buf.rd_idx))
		    && ((size + buf_allocation) <= STP_SDIO_TX_FIFO_SIZE)) {
#if KMALLOC_UPDATE
			pkt_bufp =
			    gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE +
			    buf_allocation;

			buf_allocation += size;
			*(gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 0) =
			    (UINT8) (buf_allocation & 0xff);
			*(gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 1) =
			    (UINT8) (buf_allocation >> 8);
#else
			pkt_bufp = &gp_info->pkt_buf.tx_buf[prev_wr_idx][buf_allocation];

			buf_allocation += size;
			gp_info->pkt_buf.tx_buf[prev_wr_idx][0] = (UINT8) (buf_allocation & 0xff);
			gp_info->pkt_buf.tx_buf[prev_wr_idx][1] = (UINT8) (buf_allocation >> 8);
#endif
			memcpy(pkt_bufp, data, size);
			spin_unlock_irqrestore(&gp_info->pkt_buf.rd_idx_lock,
					       gp_info->pkt_buf.rd_irq_flag);

			STPSDIO_PR_DBG("(Not empty-aggre) Enqueue done\n");

			*written_size = size;
		}
		/* Case 2.2 Not aggregation */
		else {
			/* Check the ring buf is full or not */
			room = (atomic_read(&gp_info->pkt_buf.wr_idx) >=
				atomic_read(&gp_info->pkt_buf.rd_idx)) ? (STP_SDIO_TX_BUF_CNT -
				(atomic_read(&gp_info->pkt_buf.wr_idx) -
				 atomic_read(&gp_info->pkt_buf.rd_idx))) :
				(atomic_read(&gp_info->pkt_buf.rd_idx) -
				 atomic_read(&gp_info->pkt_buf.wr_idx));
			if (room == 1)
				gp_info->pkt_buf.full_flag = MTK_WCN_BOOL_TRUE;
			spin_unlock_irqrestore(&gp_info->pkt_buf.rd_idx_lock,
					       gp_info->pkt_buf.rd_irq_flag);

			/* George: if tx_wkr preempts here and pop out all buffered entries,
			 * ie increase rd_idx utill wr_idx, tx_wkr will encounter buffer
			 * empty condition and stop, then being scheduled before end of this
			 * function. It's safe!
			 */

			/* set the size in SDIO packet header */
#if KMALLOC_UPDATE
			*(gp_info->pkt_buf.tx_buf +
			  atomic_read(&gp_info->pkt_buf.wr_idx) * STP_SDIO_TX_ENTRY_SIZE + 0) =
				(UINT8) ((size + STP_SDIO_HDR_SIZE) & 0xff);
			*(gp_info->pkt_buf.tx_buf +
			atomic_read(&gp_info->pkt_buf.wr_idx) * STP_SDIO_TX_ENTRY_SIZE + 1) =
				(UINT8) ((size + STP_SDIO_HDR_SIZE) >> 8);
			gp_info->pkt_buf.tx_buf_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = jiffies;
			osal_get_local_time(&ts, &nsec);
			gp_info->pkt_buf.tx_buf_local_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = ts;
			gp_info->pkt_buf.tx_buf_local_nsec[atomic_read(&gp_info->pkt_buf.wr_idx)] = nsec;

			pkt_bufp =
			    gp_info->pkt_buf.tx_buf +
			    atomic_read(&gp_info->pkt_buf.wr_idx) * STP_SDIO_TX_ENTRY_SIZE + STP_SDIO_HDR_SIZE;
#else
			gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][0] =
			    (UINT8) ((size + STP_SDIO_HDR_SIZE) & 0xff);
			gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][1] =
			    (UINT8) ((size + STP_SDIO_HDR_SIZE) >> 8);
			gp_info->pkt_buf.tx_buf_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = jiffies;

			pkt_bufp = &gp_info->pkt_buf.
				tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][STP_SDIO_HDR_SIZE];
#endif
			memcpy(pkt_bufp, data, size);

			STPSDIO_PR_DBG("(Not empty-no aggre) Enqueue done\n");

			*written_size = size;
			gp_info->txwkr_flag = 1;
			atomic_set(&gp_info->pkt_buf.wr_idx,
					(atomic_read(&gp_info->pkt_buf.wr_idx) + 1) % STP_SDIO_TX_BUF_CNT);
		}
	}
	/* Case 3: buffer is full (Aggregation) */
	else if (gp_info->pkt_buf.full_flag != MTK_WCN_BOOL_FALSE) {
		prev_wr_idx = (atomic_read(&gp_info->pkt_buf.wr_idx) - 1 + STP_SDIO_TX_BUF_CNT) %
			STP_SDIO_TX_BUF_CNT;
#if KMALLOC_UPDATE
		buf_allocation =
		    *(gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 1);
		buf_allocation =
		    (buf_allocation << 8) | *(gp_info->pkt_buf.tx_buf +
					      prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 0);
#else
		buf_allocation = gp_info->pkt_buf.tx_buf[prev_wr_idx][1];
		buf_allocation = (buf_allocation << 8) | gp_info->pkt_buf.tx_buf[prev_wr_idx][0];
#endif
		/* Case 3.1 Aggregation */
		/* George: do length check using add instead of sub operation. Compare
		 * to FIFO size instead of sw entry size. (buf_allocation != 0) shall be
		 * an assert true condition, not a if () condition......
		 */
		if ((buf_allocation != 0)
		    && ((size + buf_allocation) <= STP_SDIO_TX_FIFO_SIZE)) {
#if KMALLOC_UPDATE
			pkt_bufp =
			    gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE +
			    buf_allocation;
#else
			pkt_bufp = &gp_info->pkt_buf.tx_buf[prev_wr_idx][buf_allocation];
#endif
			buf_allocation += size;
#if KMALLOC_UPDATE
			*(gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 0) =
			    (UINT8) (buf_allocation & 0xff);
			*(gp_info->pkt_buf.tx_buf + prev_wr_idx * STP_SDIO_TX_ENTRY_SIZE + 1) =
			    (UINT8) (buf_allocation >> 8);
#else
			gp_info->pkt_buf.tx_buf[prev_wr_idx][0] = (UINT8) (buf_allocation & 0xff);
			gp_info->pkt_buf.tx_buf[prev_wr_idx][1] = (UINT8) (buf_allocation >> 8);
#endif
			/* Copy data to ring buffer */
			memcpy(pkt_bufp, data, size);
			spin_unlock_irqrestore(&gp_info->pkt_buf.rd_idx_lock,
					       gp_info->pkt_buf.rd_irq_flag);

			STPSDIO_PR_DBG("(full-aggre) Enqueue done\n");

			*written_size = size;
		}
		/* Case 3.2 Buffer is full */
		else {
			spin_unlock_irqrestore(&gp_info->pkt_buf.rd_idx_lock,
					       gp_info->pkt_buf.rd_irq_flag);
			STPSDIO_PR_WARN("Local Tx buffer is full !!!!!\n");

			/* Wait for tx ring buffer is not full
			 * TODO:[FixMe][George] This wait() call IS a problem
			 * if caller runs in interrupt context (sw or hw) !!
			 * TODO:[FixMe][George] should use timeout version,
			 * not interruptible version. Return error when timeout!
			 */
			wait_event_interruptible(gp_info->pkt_buf.fullwait_q,
						 (!gp_info->pkt_buf.full_flag));
			STPSDIO_PR_INFO("wait event return\n");

			spin_lock_irqsave(&gp_info->pkt_buf.rd_idx_lock,
					  gp_info->pkt_buf.rd_irq_flag);
			/* Check if the local buf is free enough */
			room = (atomic_read(&gp_info->pkt_buf.wr_idx) >=
				atomic_read(&gp_info->pkt_buf.rd_idx)) ?
				(STP_SDIO_TX_BUF_CNT - (atomic_read(&gp_info->pkt_buf.wr_idx) -
				atomic_read(&gp_info->pkt_buf.rd_idx))) :
				(atomic_read(&gp_info->pkt_buf.rd_idx) -
				 atomic_read(&gp_info->pkt_buf.wr_idx));
			if (room == 1)
				gp_info->pkt_buf.full_flag = MTK_WCN_BOOL_TRUE;
			spin_unlock_irqrestore(&gp_info->pkt_buf.rd_idx_lock,
					       gp_info->pkt_buf.rd_irq_flag);

			/* George: use this new entry w/o protection */
#if KMALLOC_UPDATE
			*(gp_info->pkt_buf.tx_buf + atomic_read(&gp_info->pkt_buf.wr_idx) *
					STP_SDIO_TX_ENTRY_SIZE + 0) =
				(UINT8) ((size + STP_SDIO_HDR_SIZE) & 0xff);
			*(gp_info->pkt_buf.tx_buf + atomic_read(&gp_info->pkt_buf.wr_idx) *
					STP_SDIO_TX_ENTRY_SIZE + 1) =
				(UINT8) ((size + STP_SDIO_HDR_SIZE) >> 8);
			gp_info->pkt_buf.tx_buf_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = jiffies;
			osal_get_local_time(&ts, &nsec);
			gp_info->pkt_buf.tx_buf_local_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = ts;
			gp_info->pkt_buf.tx_buf_local_nsec[atomic_read(&gp_info->pkt_buf.wr_idx)] = nsec;

			pkt_bufp = gp_info->pkt_buf.tx_buf + atomic_read(&gp_info->pkt_buf.wr_idx)
				* STP_SDIO_TX_ENTRY_SIZE + STP_SDIO_HDR_SIZE;
#else
			gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][0] =
			    (UINT8) ((size + STP_SDIO_HDR_SIZE) & 0xff);
			gp_info->pkt_buf.tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][1] =
			    (UINT8) ((size + STP_SDIO_HDR_SIZE) >> 8);
			gp_info->pkt_buf.tx_buf_ts[atomic_read(&gp_info->pkt_buf.wr_idx)] = jiffies;

			pkt_bufp = &gp_info->pkt_buf.
				tx_buf[atomic_read(&gp_info->pkt_buf.wr_idx)][STP_SDIO_HDR_SIZE];
#endif
			/* Copy data to ring buffer */
			memcpy(pkt_bufp, data, size);
			*written_size = size;
			gp_info->txwkr_flag = 1;
			atomic_set(&gp_info->pkt_buf.wr_idx,
			    (atomic_read(&gp_info->pkt_buf.wr_idx) + 1) % STP_SDIO_TX_BUF_CNT);
		}
	}
	/* <2> schedule for Tx worker tasklet */
	osal_ftrace_print("%s|E|signal stp_sdio_tx_rx\n", __func__);
#if STP_SDIO_OWN_THREAD
	/* tasklet_schedule(&gp_info->tx_rx_job); */
	STPSDIO_PR_DBG("osal_trigger_event gp_info->tx_rx_event\n");
	osal_trigger_event(&gp_info->tx_rx_event);
#else
	schedule_work(&gp_info->tx_work);
#endif
	osal_ftrace_print("%s|E|L|%d\n", __func__, size);

	return 0;
}
#endif /* end of !STP_SDIO_NEW_TXRING */

/*!
 * \brief Do STP-SDIO tx status, counters, debug information sanity check
 *
 * \details A function doing sanity checks on STP-SDIO Tx-related status,
 *      counters, debugging information. Used in tx_worker before and after bus
 *      write to check if any abnormal status happened.
 *
 * \param[IN] p_info The STP-SDIO HIF information structure pointer
 * \param[IN] id The sanity check location ID, assigned by caller
 *
 * \retval none.
 */
static _osal_inline_ VOID stp_sdio_check_tx_sanity(const MTK_WCN_STP_SDIO_HIF_INFO *p_info, const UINT32 id)
{
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
	if ((p_info) && (p_info->firmware_info.tx_packet_num == 0)) {
		if (!(p_info->tx_pkt_list.pkt_rd_cnt == p_info->tx_pkt_list.pkt_wr_cnt)) {
			STPSDIO_PR_ERR
				("abnormal fifo_size(%d) pkt_num(%d) pkt_rd(0x%x, %ld) pkt_wr(0x%x, %ld)!(%d)\n",
				 p_info->firmware_info.tx_fifo_size,
				 p_info->firmware_info.tx_packet_num, p_info->tx_pkt_list.pkt_rd_cnt,
				 STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_rd_cnt),
				 p_info->tx_pkt_list.pkt_wr_cnt,
				 STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_wr_cnt), id);
			/* stp_sdio_dump_txdbg(); */
		}
		if (p_info->firmware_info.tx_fifo_size != STP_SDIO_TX_FIFO_SIZE) {
			STPSDIO_PR_ERR
				("abnormal fifo_size(%d) pkt_num(%d) pkt_rd(0x%x, %ld) pkt_wr(0x%x, %ld)!(%d)\n",
				 p_info->firmware_info.tx_fifo_size,
				 p_info->firmware_info.tx_packet_num, p_info->tx_pkt_list.pkt_rd_cnt,
				 STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_rd_cnt),
				 p_info->tx_pkt_list.pkt_wr_cnt,
				 STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_wr_cnt), id);
			/* stp_sdio_dump_txdbg(); */
		}
	} else {
		if ((p_info) && (p_info->tx_pkt_list.pkt_rd_cnt == p_info->tx_pkt_list.pkt_wr_cnt)) {
			STPSDIO_PR_ERR
				("abnormal fifo_size(%d) pkt_num(%d) pkt_rd(0x%x, %ld) pkt_wr(0x%x, %ld)!(%d)\n",
				 p_info->firmware_info.tx_fifo_size,
				 p_info->firmware_info.tx_packet_num, p_info->tx_pkt_list.pkt_rd_cnt,
				 STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_rd_cnt),
				 p_info->tx_pkt_list.pkt_wr_cnt,
				 STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_wr_cnt), id);
			/* stp_sdio_dump_txdbg(); */
		}
	}
#endif
}

/*!
 * \brief Handle STP-SDIO TX IRQ BH part and complete count
 *
 * \details Handle STP-SDIO TX IRQ bottom half part and reported tx complete
 *      coount. This function is used in tx_worker ONLY to avoid race condition.
 *
 * \note tx_comp_num in firmware_info structure shall be handled atomically.
 *      It is added in STP-SDIO Tx IRQ top half handler with the number reported
 *      in CHISR. It is deducted in this function.
 *
 * \note tx_fifo_size is deducted in tx_worker when writing data to bus and
 *      added back in this function when tx complete.
 *
 * \param[IN] p_info The STP-SDIO HIF information structure pointer
 *
 * \retval none.
 */
static VOID stp_sdio_tx_wkr_comp(MTK_WCN_STP_SDIO_HIF_INFO * const p_info)
{
	INT32 comp_count;
	UINT32 idx;
	INT32 i;
	INT32 max;
	INT32 ret;
	UINT32 value = 0;

	comp_count = atomic_read(&p_info->firmware_info.tx_comp_num);
	atomic_sub(comp_count, &p_info->firmware_info.tx_comp_num);

	/* update tx to firemware information */
	if (p_info->firmware_info.tx_packet_num >= comp_count) {
		STPSDIO_PR_DBG("tx_pack_num(%d), comp_count(%d),tx_comp_num(%d), retry flag(%d)\n",
				 p_info->firmware_info.tx_packet_num, comp_count,
				 atomic_read(&p_info->firmware_info.tx_comp_num),
				 p_info->tx_retry_flag);
		p_info->firmware_info.tx_packet_num -= comp_count;
	} else {
		STPSDIO_PR_INFO("abnormal complete count(%d), tx_packet_num(%d), retry flag(%d)!\n",
				 comp_count, p_info->firmware_info.tx_packet_num, p_info->tx_retry_flag);
		/* TODO: [FixMe][George] Add error handling or bug report!! */
		STPSDIO_PR_INFO("tx_fifo_size(%d), tx_packet_num(%d)\n",
				p_info->firmware_info.tx_fifo_size,
				p_info->firmware_info.tx_packet_num);
	}

	while (comp_count > 0) {
		if (p_info->tx_pkt_list.pkt_rd_cnt == p_info->tx_pkt_list.pkt_wr_cnt) {
			STPSDIO_PR_ERR("tx complete count(%d) but tx_pkt_list empty\n",
					comp_count);
			STPSDIO_PR_ERR("rd_cnt(%u, idx:%lx), wr_cnt(%u, idx:%lx)!\n",
					p_info->tx_pkt_list.pkt_rd_cnt,
					p_info->tx_pkt_list.pkt_rd_cnt & STP_SDIO_TX_PKT_LIST_SIZE_MASK,
					p_info->tx_pkt_list.pkt_wr_cnt,
					p_info->tx_pkt_list.pkt_wr_cnt & STP_SDIO_TX_PKT_LIST_SIZE_MASK);
			break;
		}

		idx = p_info->tx_pkt_list.pkt_rd_cnt++ & STP_SDIO_TX_PKT_LIST_SIZE_MASK;
		p_info->firmware_info.tx_fifo_size += p_info->tx_pkt_list.pkt_size_list[idx];
		p_info->tx_pkt_list.out_ts[idx] = jiffies;
		--comp_count;
	}
	if (p_info->retry_enable_flag) {
		if (p_info->tx_retry_flag == STP_SDIO_RETRY_INT) {
			for (i = 0; i < 100; i++) {
				ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx,
						CTMDPCR0, &value, 0);
				if (value & 1)
					break;
				osal_sleep_ms(10);
			}
			if (!(value & 1)) {
				stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx,
						CTMDPCR0, &value, 0);
				STPSDIO_PR_ERR("tx retry: firmware rx buffer is disable CTMDPCR0 value(%x)!\n",
						value);
				stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx,
						CTMDPCR1, &value, 0);
				STPSDIO_PR_ERR("tx retry: firmware rx buffer is disable CTMDPCR1 value(%x)!\n",
						value);
				stp_sdio_issue_fake_coredump
					("ABT: <HIF_SDIO> tx retry ERROR: firmware rx buffer is disable");
			}
			idx = p_info->tx_pkt_list.pkt_rd_cnt & STP_SDIO_TX_PKT_LIST_SIZE_MASK;
			max = p_info->firmware_info.tx_packet_num;
			STPSDIO_PR_ERR("tx retry idx(%d) tx retry max(%d)\n", idx, max);
			for (i = 0; i < max; i++) {
				/* sdio crc error tx retry */
				ret = stp_sdio_rw_retry(HIF_TYPE_WRITE_BUF, STP_SDIO_RETRY_LIMIT,
						p_info->sdio_cltctx, CTDR,
						(PUINT32)(p_info->pkt_buf.tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + 0),
						p_info->tx_pkt_list.pkt_size_list[idx]);
				if (ret) {
					STPSDIO_PR_ERR("sdio tx retry get CTDR information Tx error(%d)!\n",
							ret);
					/* TODO: error handling! */
					p_info->tx_retry_count++;
					if (p_info->tx_retry_count > STP_SDIO_RETRY_LIMIT) {
						STPSDIO_PR_INFO("sdio tx retry fail complete count(%d)\n",
								comp_count);
						STPSDIO_PR_INFO("tx_packet_num(%d), tx_fifo_size(%d)\n",
								p_info->firmware_info.tx_packet_num,
								p_info->firmware_info.tx_fifo_size);
						STPSDIO_PR_INFO("retry flag(%d)!\n", p_info->tx_retry_flag);
						/* TODO: error handling! */
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
						stp_sdio_txdbg_dump();
#endif
						p_info->tx_retry_count = 0;
						stp_sdio_issue_fake_coredump
							("ABT: <HIF_SDIO> write_readsb retry ERROR");
					}
				} else
					p_info->tx_retry_count = 0;
				idx++;
				idx = idx & STP_SDIO_TX_PKT_LIST_SIZE_MASK;
			}
			p_info->tx_retry_flag = STP_SDIO_RETRY_NONE;
		}
	}
}

/*!
 * \brief Handle STP-SDIO Tx buffer and send to bus
 *
 * \details Handle STP-SDIO Tx buffer and send SDIO packet to bus if everything
 *      is checked ok.
 *
 * \note Tx count to FIFO is counted on a 4-byte aligned base. 1~3 bytes padding
 *      are also sent into HW FIFO and SHALL be trimmed off by firmware.
 *      tx_fifo_size is deducted in this bus when writing data to bus and added
 *      back in handle_tx_comp() function.
 *
 * \note Data length written to bus shall be 4-byte aligned AND block_size
 *      aligned if length > block_size. Padding bytes added for block_size is
 *      removed by HW.
 *
 * \note Max accumulated Tx size to FIFO is limited by STP_SDIO_TX_FIFO_SIZE and
 *      it is (2080) for MT6620. It is NOT limited to 256*5=1280 bytes.
 *
 * \note Max outstanding Tx packet count is limited by STP_SDIO_TX_PKT_MAX_CNT
 *      and it is (7) for MT6620.
 *
 * \param[IN] work Tx work struct work_struct pointer used by STP-SDIO
 *
 * \retval none.
 */
#if STP_SDIO_NEW_TXRING
static VOID stp_sdio_tx_wkr(struct work_struct *work)
{
	MTK_WCN_STP_SDIO_HIF_INFO *p_info;
	UINT32 bus_txlen;
	UINT32 four_byte_align_len;
	PUINT8 buf_tx;
	INT32 ret;
	UINT32 idx;
	MTK_WCN_STP_SDIO_PKT_BUF *pb;
	struct timeval now;
	UINT64 ts;
	ULONG nsec;

	p_info = container_of(work, MTK_WCN_STP_SDIO_HIF_INFO, tx_work);
	ret = HIF_SDIO_ERR_SUCCESS;
	pb = &p_info->pkt_buf;

	/* 4 <0> Tx worker has been scheduled to send data */
	do {
		/* handle tx complete count if any */
		stp_sdio_tx_wkr_comp(p_info);
		/* check sanity of local tx information */
		stp_sdio_check_tx_sanity(p_info, 1);

		/* check if Tx ring buffer is empty */
		if (atomic_read(&p_info->pkt_buf.wr_cnt) == atomic_read(&p_info->pkt_buf.rd_cnt)) {
			/* full flag is use less in this condition */
			STPSDIO_PR_DBG("Tx entry ring buffer empty\n");
			break;
		}
		p_info->txwkr_flag = 0;
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXPERFDBG
		++stp_sdio_txperf_worker_cnt;
#endif

		/* George: check txed packet number < limit(7)
		 * tx_packet_num is maintained only in tw_worker, no protection.
		 */
		if (p_info->firmware_info.tx_packet_num >= STP_SDIO_TX_PKT_MAX_CNT) {
			STPSDIO_PR_DBG
			    ("tx_packet_num(%ld) limit,  tx_fifo_size(%ld), four_byte_align_len(%ld)\n",
			     p_info->firmware_info.tx_packet_num,
			     p_info->firmware_info.tx_fifo_size, four_byte_align_len);
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXPERFDBG
			++stp_sdio_txperf_pkt_num_lmt_cnt;
#endif
			break;
		}
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXPERFDBG
		stp_sdio_txperf_txed_pkt_num += p_info->firmware_info.tx_packet_num;
#endif

		/* Access content in rd_cnt is safe because it will not be aggregated
		 * anymore in sdio_tx(). Check current tx condition with info in rd_cnt.
		 */
		idx = atomic(&pb->rd_cnt) & STP_SDIO_TX_BUF_CNT_MASK;

		/* Get Tx packet size from Tx size ring buf */
		bus_txlen = pb->tx_buf_sz[idx];
		/* Update packet length in Tx entry */
#if KMALLOC_UPDATE
		buf_tx = gp_info->pkt_buf.tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + 0;
#else
		buf_tx = &pb->tx_buf[idx][0];
#endif
		buf_tx[0] = (UINT8) (bus_txlen & 0xff);
		buf_tx[1] = (UINT8) ((bus_txlen >> 8) & 0xff);

		/* George: hw always count fifo in 4-byte aligned length */
		bus_txlen += 0x3;
		bus_txlen &= ~(0x3UL);
		four_byte_align_len = bus_txlen;

		/* Sanity check: 4-byte aligned length shall not exceed HW FIFO Size */
		if (four_byte_align_len > STP_SDIO_TX_FIFO_SIZE) {
			STPSDIO_PR_ERR("abnormal four_byte_align_len(%d) > TX_FIFO_SIZE(%ld)!!\n",
					 four_byte_align_len, STP_SDIO_TX_FIFO_SIZE);
		}

		/* George: check if tx FIFO space is enough for 4-byte aligned length.
		 * If enough, tx this entry and increase rd_cnt.
		 */
		if (p_info->firmware_info.tx_fifo_size >= four_byte_align_len) {
			/* George: refine block_size alignment with the assumption: block_size is 2^*x */
			if (bus_txlen > STP_SDIO_BLK_SIZE) {
				bus_txlen += (STP_SDIO_BLK_SIZE - 1);
				bus_txlen &= ~((UINT32) STP_SDIO_BLK_SIZE - 1);
			}

			/* Sanity check: bus_txlen shall not exceed SW entry size */
			if (bus_txlen > STP_SDIO_TX_ENTRY_SIZE) {
				STPSDIO_PR_ERR
				    ("abnormal bus_txlen(%d) > STP_SDIO_TX_ENTRY_SIZE(%ld)!!\n",
				     bus_txlen, STP_SDIO_TX_ENTRY_SIZE);
			}

			++(p_info->firmware_info.tx_packet_num);
			/* decrease Tx FIFO size: using 4-byte aligned length! */
			p_info->firmware_info.tx_fifo_size -= four_byte_align_len;
			/* record the SDIO packet size in packet size list: using 4-byte aligned length! */
			idx = p_info->tx_pkt_list.pkt_wr_cnt++ & STP_SDIO_TX_PKT_LIST_SIZE_MASK;
			p_info->tx_pkt_list.pkt_size_list[idx] = four_byte_align_len;
			p_info->tx_pkt_list.in_ts[idx] = jiffies;
			p_info->tx_pkt_list.out_ts[idx] = 0;

			STPSDIO_PR_DBG("wr(0x%x, %ld) rd(0x%x, %ld), tx fifo(size:%d), pkt_num(%d)done\n",
					p_info->tx_pkt_list.pkt_wr_cnt,
					STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_wr_cnt),
					p_info->tx_pkt_list.pkt_rd_cnt,
					STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_rd_cnt),
					p_info->firmware_info.tx_fifo_size,
					p_info->firmware_info.tx_packet_num);

			/* port write the packet to CTDR */
			ret = stp_sdio_rw_retry(HIF_TYPE_WRITE_BUF, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx,
					CTDR, (PUINT32) buf_tx, bus_txlen);
			STPSDIO_PR_DBG("write to CTDR done\n");

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
			do {
				osal_get_local_time(&ts, &nsec);
				idx = stp_sdio_txdbg_cnt++ & STP_SDIO_TXDBG_COUNT_MASK;
				/* skip clear buf */
				stp_sdio_txdbg_buffer[idx].ts = jiffies;
				stp_sdio_txdbg_buffer[idx].l_sec = ts;
				stp_sdio_txdbg_buffer[idx].l_nsec = nsec;
				stp_sdio_txdbg_buffer[idx].bus_txlen = bus_txlen;
				stp_sdio_txdbg_buffer[idx].four_byte_align_len =
				    four_byte_align_len;
				/* store content */
				if (bus_txlen <= STP_SDIO_TX_ENTRY_SIZE) {
					memcpy(&stp_sdio_txdbg_buffer[idx].tx_pkt_buf[0], buf_tx,
					       bus_txlen);
				} else {
					memcpy(&stp_sdio_txdbg_buffer[idx].tx_pkt_buf[0], buf_tx,
					       STP_SDIO_TX_ENTRY_SIZE);
					STPSDIO_PR_ERR("abnormal bus_txlen (%d)!\n", bus_txlen);
				}
			} while (0);
#endif
			if (ret) {
				STPSDIO_PR_ERR("get CTDR information Tx error(%d)!\n", ret);
				if (p_info->retry_enable_flag) {
					if (ret == -EIO || ret == -EILSEQ || ret == -EBUSY)
						p_info->tx_retry_flag = STP_SDIO_RETRY_CRC_ERROR;
					else {
						p_info->tx_retry_count = 0;
						p_info->tx_retry_flag = STP_SDIO_RETRY_NONE;
						stp_sdio_issue_fake_coredump
							("ABT: <HIF_SDIO> write_readsb retry ERROR");
					}
				} else
					stp_sdio_issue_fake_coredump("ABT: <HIF_SDIO> write_readsb retry ERROR");
			}

			/* clear rd index entry of Tx ring buffer */
			/*memset(buf_tx, 0, STP_SDIO_TX_ENTRY_SIZE); */
			/* George: clear STP-SDIO header only for debugging. */
			/*memset(buf_tx, 0, 4); */
			/* need clear??? skip it for debugging */

			spin_lock_irqsave(&pb->rd_cnt_lock, pb->rd_irq_flag);
			atomic_inc(&pb->rd_cnt);
			/* TODO: [FixMe][George] check if full_flag needed? */
			if (pb->full_flag != MTK_WCN_BOOL_FALSE) {
				pb->full_flag = MTK_WCN_BOOL_FALSE;
				wake_up_interruptible(&pb->fullwait_q);
			}
			spin_unlock_irqrestore(&pb->rd_cnt_lock, pb->rd_irq_flag);
			osal_do_gettimeofday(&old);
		} else {
			/* tx FIFO free space < packet size, wait next time */
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXPERFDBG
			stp_sdio_txperf_fifo_left += p_info->firmware_info.tx_fifo_size;
			stp_sdio_txperf_to_send += four_byte_align_len;
			++stp_sdio_txperf_fifo_lmt_cnt;
#endif

			osal_do_gettimeofday(&now);
			if ((now.tv_sec - old.tv_sec) > TX_NO_ACK_TIMEOUT_ASSERT) {
				STPSDIO_PR_INFO("tx_fifo_size(%d), four_byte_align_len(%d), tx_packet_num(%d)\n",
						p_info->firmware_info.tx_fifo_size, four_byte_align_len,
						p_info->firmware_info.tx_packet_num);
				STPSDIO_PR_INFO("No ack trigger assert, tx %d seconds later\n",
						TX_NO_ACK_TIMEOUT_ASSERT);
				stp_dbg_poll_cpupcr(5, 1, 1);
				p_info->firmware_info.tx_fifo_size = STP_SDIO_TX_FIFO_SIZE;
				p_info->firmware_info.tx_packet_num = 0;
				if (pb->full_flag != MTK_WCN_BOOL_FALSE) {
					pb->full_flag = MTK_WCN_BOOL_FALSE;
					wake_up_interruptible(&pb->fullwait_q);
				}
				ret = mtk_wcn_wmt_assert_timeout(WMTDRV_TYPE_STP, 33, 0);
				if (!ret)
					STPSDIO_PR_INFO("trigger assert fail\n");
			}
			break;
		}

		stp_sdio_check_tx_sanity(p_info, 2);
	} while (1);
}

#else
static VOID stp_sdio_tx_wkr(struct work_struct *work)
{
	MTK_WCN_STP_SDIO_HIF_INFO *p_info;
	UINT32 bus_txlen;
	UINT32 four_byte_align_len;
	PUINT8 buf_tx;
	INT32 ret;
	UINT32 idx;
	MTK_WCN_STP_SDIO_PKT_BUF *pb;
	struct timeval now;
	UINT64 ts;
	ULONG nsec;

	p_info = container_of(work, MTK_WCN_STP_SDIO_HIF_INFO, tx_work);
	ret = HIF_SDIO_ERR_SUCCESS;
	pb = &p_info->pkt_buf;

	/* 4 <0> Tx worker has been scheduled to send data */
	do {
		/* handle tx complete count if any */
		stp_sdio_tx_wkr_comp(p_info);
		stp_sdio_check_tx_sanity(p_info, 1);

		/* check if Tx ring buffer is empty */
		if ((atomic_read(&p_info->pkt_buf.wr_idx) == atomic_read(&p_info->pkt_buf.rd_idx))
		    && (p_info->pkt_buf.full_flag == MTK_WCN_BOOL_FALSE)) {
			STPSDIO_PR_DBG("Tx ring buffer is empty\n");
			break;
		}
		p_info->txwkr_flag = 0;

		/* George: no race condition here! Updating rd_idx content will not be
		 * put into more data by stp_sdio_tx
		 */
		/* Get Tx packet size from Tx ring buf */
#if KMALLOC_UPDATE
		buf_tx = gp_info->pkt_buf.tx_buf +
			atomic_read(&p_info->pkt_buf.rd_idx) * STP_SDIO_TX_ENTRY_SIZE + 0;
#else
		buf_tx = &p_info->pkt_buf.tx_buf[atomic_read(&p_info->pkt_buf.rd_idx)][0];
#endif
		bus_txlen = buf_tx[1];
		bus_txlen = (bus_txlen << 8) | buf_tx[0];

		/* George: hw always count fifo in 4-byte aligned length */
		bus_txlen += 0x3;
		bus_txlen &= ~(0x3UL);
		four_byte_align_len = bus_txlen;
		/* Sanity check: 4-byte aligned length shall not exceed HW FIFO Size */
		if (four_byte_align_len > STP_SDIO_TX_FIFO_SIZE) {
			STPSDIO_PR_ERR("abnormal four_byte_align_len(%d) > TX_FIFO_SIZE(%ld)!!\n",
					 four_byte_align_len, STP_SDIO_TX_FIFO_SIZE);
		}

		osal_ftrace_print("%s|S|bus_txlen(%d)\n", __func__, four_byte_align_len);
		/* George: check if
		 * 1. tx FIFO free space is enough using 4-byte aligned length
		 * 2. tx max pkt count is not reached
		 */
		if ((p_info->firmware_info.tx_fifo_size >= four_byte_align_len)
		    && (p_info->firmware_info.tx_packet_num < STP_SDIO_TX_PKT_MAX_CNT)) {
			/* George: refine block_size alignment with the assumption: block_size is 2^*x */
			if (bus_txlen > STP_SDIO_BLK_SIZE) {
				bus_txlen += (STP_SDIO_BLK_SIZE - 1);
				bus_txlen &= ~((UINT32) STP_SDIO_BLK_SIZE - 1);
			}

			/* Sanity check: bus_txlen shall not exceed SW entry size */
			if (bus_txlen > STP_SDIO_TX_ENTRY_SIZE) {
				STPSDIO_PR_ERR
				    ("abnormal bus_txlen(%d) > STP_SDIO_TX_ENTRY_SIZE(%ld)!!\n",
				     bus_txlen, STP_SDIO_TX_ENTRY_SIZE);
			}

			++(p_info->firmware_info.tx_packet_num);
			/* decrease Tx FIFO size: using 4-byte aligned length! */
			p_info->firmware_info.tx_fifo_size -= four_byte_align_len;
			/* record the SDIO packet size in packet size list: using 4-byte aligned length! */
			idx = p_info->tx_pkt_list.pkt_wr_cnt++ & STP_SDIO_TX_PKT_LIST_SIZE_MASK;
			p_info->tx_pkt_list.pkt_size_list[idx] = four_byte_align_len;
			p_info->tx_pkt_list.in_ts[idx] = jiffies;
			p_info->tx_pkt_list.out_ts[idx] = 0;

			STPSDIO_PR_DBG("wr(0x%x, %ld) rd(0x%x, %ld), tx fifo(size:%d), pkt_num(%d)done\n",
					p_info->tx_pkt_list.pkt_wr_cnt,
					STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_wr_cnt),
					p_info->tx_pkt_list.pkt_rd_cnt,
					STP_SDIO_GET_PKT_AR_IDX(p_info->tx_pkt_list.pkt_rd_cnt),
					p_info->firmware_info.tx_fifo_size,
					p_info->firmware_info.tx_packet_num);

			/* port write the packet to CTDR */
			ret = stp_sdio_rw_retry(HIF_TYPE_WRITE_BUF, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx,
					CTDR, (PUINT32) buf_tx, bus_txlen);
			STPSDIO_PR_DBG("write to CTDR done\n");

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
			do {
				osal_get_local_time(&ts, &nsec);
				idx = stp_sdio_txdbg_cnt++ & STP_SDIO_TXDBG_COUNT_MASK;
				/* skip clear buf */
				stp_sdio_txdbg_buffer[idx].ts = jiffies;
				stp_sdio_txdbg_buffer[idx].l_sec = ts;
				stp_sdio_txdbg_buffer[idx].l_nsec = nsec;
				stp_sdio_txdbg_buffer[idx].bus_txlen = bus_txlen;
				stp_sdio_txdbg_buffer[idx].four_byte_align_len =
				    four_byte_align_len;
				/* store content */
				if (bus_txlen <= STP_SDIO_TX_ENTRY_SIZE) {
					memcpy(&stp_sdio_txdbg_buffer[idx].tx_pkt_buf[0], buf_tx,
					       bus_txlen);
				} else {
					memcpy(&stp_sdio_txdbg_buffer[idx].tx_pkt_buf[0], buf_tx,
					       STP_SDIO_TX_ENTRY_SIZE);
					STPSDIO_PR_ERR("abnormal bus_txlen(%d)!\n", bus_txlen);
				}
			} while (0);
#endif

			if (ret) {
				STPSDIO_PR_ERR("get CTDR information Tx error(%d)!\n", ret);
				if (p_info->retry_enable_flag) {
					if (ret == -EIO || ret == -EILSEQ || ret == -EBUSY)
						p_info->tx_retry_flag = STP_SDIO_RETRY_CRC_ERROR;
					else {
						p_info->tx_retry_count = 0;
						p_info->tx_retry_flag = STP_SDIO_RETRY_NONE;
						stp_sdio_issue_fake_coredump
							("ABT: <HIF_SDIO> write_readsb retry ERROR");
					}
				} else
					stp_sdio_issue_fake_coredump("ABT: <HIF_SDIO> write_readsb retry ERROR");
			}

			/* clear rd index entry of Tx ring buffer */
			/*memset(buf_tx, 0, STP_SDIO_TX_ENTRY_SIZE); */
			/* George: clear STP-SDIO header only for debugging. */
			/*memset(buf_tx, 0, 4); */
			/* need clear??? skip it for debugging */

			spin_lock_irqsave(&p_info->pkt_buf.rd_idx_lock,
					  p_info->pkt_buf.rd_irq_flag);
			/* release tx ring buffer */
			atomic_set(&p_info->pkt_buf.rd_idx,
					(atomic_read(&p_info->pkt_buf.rd_idx) + 1) % STP_SDIO_TX_BUF_CNT);
			/* Set Tx ring buffer is not full */
			if (p_info->pkt_buf.full_flag != MTK_WCN_BOOL_FALSE) {
				p_info->pkt_buf.full_flag = MTK_WCN_BOOL_FALSE;
				wake_up_interruptible(&p_info->pkt_buf.fullwait_q);
			}
			spin_unlock_irqrestore(&p_info->pkt_buf.rd_idx_lock,
					       p_info->pkt_buf.rd_irq_flag);
			osal_do_gettimeofday(&old);
		} else {
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXPERFDBG
			stp_sdio_txperf_fifo_left += p_info->firmware_info.tx_fifo_size;
			stp_sdio_txperf_to_send += four_byte_align_len;
			++stp_sdio_txperf_fifo_lmt_cnt;
#endif
			/* (tx FIFO free space < packet size) or (the number of tx packets >= 7) */
			osal_do_gettimeofday(&now);
			if ((now.tv_sec - old.tv_sec) > TX_NO_ACK_TIMEOUT_ASSERT) {
				STPSDIO_PR_INFO("tx_fifo_size(%d), four_byte_align_len(%d), tx_packet_num(%d)\n",
						p_info->firmware_info.tx_fifo_size, four_byte_align_len,
						p_info->firmware_info.tx_packet_num);
				STPSDIO_PR_INFO("No ack trigger assert, tx %d seconds later\n",
						TX_NO_ACK_TIMEOUT_ASSERT);
				stp_dbg_poll_cpupcr(5, 1, 1);
				osal_ftrace_print("tx_fifo_size:%d, four_byte_align_len:%d, tx_packet_num(%d)\n",
						p_info->firmware_info.tx_fifo_size, four_byte_align_len,
						p_info->firmware_info.tx_packet_num);
				p_info->firmware_info.tx_fifo_size = STP_SDIO_TX_FIFO_SIZE;
				p_info->firmware_info.tx_packet_num = 0;
				if (pb->full_flag != MTK_WCN_BOOL_FALSE) {
					pb->full_flag = MTK_WCN_BOOL_FALSE;
					wake_up_interruptible(&pb->fullwait_q);
				}
				ret = mtk_wcn_wmt_assert_timeout(WMTDRV_TYPE_STP, 33, 0);
				if (!ret)
					STPSDIO_PR_INFO("trigger assert fail\n");
			}
			break;
		}

		stp_sdio_check_tx_sanity(p_info, 2);
	} while (1);

	osal_ftrace_print("%s|E|\n", __func__);
}
#endif /* end of stp_sdio_tx_wkr and STP_SDIO_NEW_TXRING */

/*!
 * \brief Handle STP-SDIO Rx IRQ BH and read data from bus
 *
 * \details Handle STP-SDIO Rx IRQ buttom half and read data from bus according
 *      to the length read in Rx IRQ top half (stp_sdio_irq()) from CHISR
 *
 * \note rx_pkt_len read in stp_sdio_irq() from CHISR. No Rx IRQ would be
 *      triggered by FW before all Rx FIFO data is read by host driver. Do
 *      sanity check for this condition.
 *
 * \note HW Rx FIFO size is (2304 = 256*9) for MT6620
 *
 * \param[IN] work Rx work struct work_struct pointer used by STP-SDIO
 *
 * \retval none.
 */
static VOID stp_sdio_rx_wkr(struct work_struct *work)
{
	PUINT8 bufp;
	UINT32 bus_rxlen;
	UINT32 chisr_rxlen;
	INT32 ret;
	INT32 ret_1;
	UINT8 cccr_value = 0;
	MTK_WCN_STP_SDIO_HIF_INFO *p_info;

	p_info = container_of(work, MTK_WCN_STP_SDIO_HIF_INFO, rx_work);

	/* 4 <0> receive data from CRDR */
	/* George: refine 4-byte alignment */
	chisr_rxlen = p_info->rx_pkt_len;


	if (chisr_rxlen > STP_SDIO_RX_FIFO_SIZE) {
		/* TODO: error handling! */
		STPSDIO_PR_ERR("abnormal chisr_rxlen(%d) rx_worker stop\n", chisr_rxlen);
		return;
	}

	bus_rxlen = chisr_rxlen;
	bus_rxlen += 0x3;
	bus_rxlen &= ~(0x3UL);

	/* George: refine block_size alignment with the assumption: BLK_SIZE is 2^x. */
	if (bus_rxlen > STP_SDIO_BLK_SIZE) {
		bus_rxlen += (STP_SDIO_BLK_SIZE - 1);
		bus_rxlen &= ~((UINT32) STP_SDIO_BLK_SIZE - 1);
	}

	ret = stp_sdio_rw_retry(HIF_TYPE_READ_BUF, STP_SDIO_RETRY_LIMIT, p_info->sdio_cltctx, CRDR,
			(PUINT32)(&(p_info->pkt_buf.rx_buf[0])), bus_rxlen);
	if (p_info->retry_enable_flag) {
		if (ret) {
			if (ret == -EIO || ret == -EILSEQ || ret == -EBUSY) {
				cccr_value = 0;
				ret_1 = mtk_wcn_hif_sdio_f0_readb(p_info->sdio_cltctx, CCCR_F0, &cccr_value);
				if (ret_1)
					STPSDIO_PR_ERR("read CCCR_F0 fail(%d)\n", ret_1);
				STPSDIO_PR_ERR("read CCCR_F0: (0x%x)\n", cccr_value);
				cccr_value |= CCCR_F0_RX_CRC;
				cccr_value |= CCCR_F0_RX_INT;
				ret_1 = mtk_wcn_hif_sdio_f0_writeb(p_info->sdio_cltctx, CCCR_F0, cccr_value);
				if (ret_1)
					STPSDIO_PR_ERR("write CCCR_F0 fail(%d)\n", ret_1);
				STPSDIO_PR_ERR("write CCCR_F0: (0x%x)\n", cccr_value);
				cccr_value = 0;
				ret_1 = mtk_wcn_hif_sdio_f0_readb(p_info->sdio_cltctx, CCCR_F0, &cccr_value);
				if (ret_1)
					STPSDIO_PR_ERR("read CCCR_F0 fail(%d)\n", ret_1);
				STPSDIO_PR_ERR("read CCCR_F0: (0x%x)\n", cccr_value);
				STPSDIO_PR_ERR("sdio read buffer happen crc error will be retry(%d)\n", ret);
				p_info->rx_retry_count++;
				if (p_info->rx_retry_count > STP_SDIO_RETRY_LIMIT) {
					p_info->rx_retry_count = 0;
					stp_sdio_issue_fake_coredump("ABT: <HIF_SDIO> sdio_readsb retry ERROR");
				}
			} else {
				p_info->rx_retry_count = 0;
				stp_sdio_issue_fake_coredump("ABT: <HIF_SDIO> sdio_readsb retry ERROR");
			}
		} else {
			cccr_value = 0;
			ret_1 = mtk_wcn_hif_sdio_f0_readb(p_info->sdio_cltctx, CCCR_F0, &cccr_value);
			if (ret_1)
				STPSDIO_PR_ERR("read CCCR_F0 fail(%d)\n", ret_1);
			STPSDIO_PR_DBG("read CCCR_F0: (0x%x)\n", cccr_value);
			cccr_value &= ~CCCR_F0_RX_CRC;
			cccr_value |= CCCR_F0_RX_INT;
			ret_1 = mtk_wcn_hif_sdio_f0_writeb(p_info->sdio_cltctx, CCCR_F0, cccr_value);
			if (ret_1)
				STPSDIO_PR_ERR("write CCCR_F0 fail(%d)\n", ret_1);
			STPSDIO_PR_DBG("write CCCR_F0: (0x%x)\n", cccr_value);
			cccr_value = 0;
			ret_1 = mtk_wcn_hif_sdio_f0_readb(p_info->sdio_cltctx, CCCR_F0, &cccr_value);
			if (ret_1)
				STPSDIO_PR_ERR("read CCCR_F0 fail(%d)\n", ret_1);
			STPSDIO_PR_DBG("read CCCR_F0: (0x%x)\n", cccr_value);
			STPSDIO_PR_DBG("sdio read buffer success(%d)\n", ret);

			p_info->rx_retry_count = 0;
		}
	} else {
		if (ret) {
			STPSDIO_PR_HINT("set to p_info->rx_pkt_len 0\n");
			stp_sdio_issue_fake_coredump("ABT: <HIF_SDIO> sdio_readsb retry ERROR");
		}
	}
	if (ret) {
		/* TODO: error handling! */
		STPSDIO_PR_ERR("read CRDR len(%d) rx error!(%d)\n", bus_rxlen, ret);
		p_info->rx_pkt_len = 0;
		return;
	}
	p_info->rx_pkt_len = 0;
	STPSDIO_PR_HINT("set to p_info->rx_pkt_len 0\n");
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_RXDBG
	do {
		UINT32 idx = stp_sdio_rxdbg_cnt++ & (STP_SDIO_RXDBG_COUNT - 1);
		/* skip clear buf */
		stp_sdio_rxdbg_buffer[idx].ts = jiffies;
		stp_sdio_rxdbg_buffer[idx].chisr_rxlen = chisr_rxlen;
		stp_sdio_rxdbg_buffer[idx].bus_rxlen = bus_rxlen;
		/* store content */
		memcpy(&stp_sdio_rxdbg_buffer[idx].rx_pkt_buf[0], &p_info->pkt_buf.rx_buf[0],
		       bus_rxlen);
	} while (0);
#endif

	bufp = &p_info->pkt_buf.rx_buf[4];

	/* Notice: len = SDIO_HDR(4) + (STP Packet + padding)*N */
	/* George: refine sanity check */
	bus_rxlen = p_info->pkt_buf.rx_buf[1];
	bus_rxlen = (bus_rxlen << 8) | p_info->pkt_buf.rx_buf[0];
	STPSDIO_PR_DBG("bus_rxlen(%d) rx_len in chisr(%d)\n", bus_rxlen, chisr_rxlen);
	if (bus_rxlen != chisr_rxlen) {
		STPSDIO_PR_ERR("abnormal bus_rxlen(%d) in SDIO packet header!in chisr(%d)\n",
				 bus_rxlen, chisr_rxlen);
		return;
	}
	if (p_info->pkt_buf.rx_buf[2] || p_info->pkt_buf.rx_buf[3]) {
		STPSDIO_PR_ERR("abnormal p_info->pkt_buf.rx_buf[2](0x%02x) [3](0x%02x)\n",
				 p_info->pkt_buf.rx_buf[2], p_info->pkt_buf.rx_buf[3]);
		return;
	}

	if (bus_rxlen > STP_SDIO_HDR_SIZE) {
		bus_rxlen -= STP_SDIO_HDR_SIZE;
		/* transmit data to stp core driver */
		osal_ftrace_print("%s|B|parser|L|%d\n", __func__, bus_rxlen);
		ret = mtk_wcn_stp_parser_data(bufp, bus_rxlen);
		osal_ftrace_print("%s|A|parser|L|%d\n", __func__, bus_rxlen);
#if STP_SDIO_DBG_SUPPORT && (STP_SDIO_TXDBG || STP_SDIO_TXPERFDBG)
		if (ret && (p_info->tx_dbg_dump_flag == 0)) {
			p_info->tx_dbg_dump_flag = 1;
			stp_sdio_txdbg_dump();
		}
#endif
	} else {
		STPSDIO_PR_ERR("abnormal rx length(%d, %d)\n", bus_rxlen, chisr_rxlen);
	}

	/* [George]: no need to mask/unmask rx interrupt. chip/fw assert next rx int
	 * if and only if host reads all rx data.
	 */
}

#if STP_SDIO_NEW_IRQ_HANDLER
static INT32 stp_sdio_irq(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx)
{
	INT32 iRet = 0;
	UINT32 chlcpr_value = 0;
	UINT32 write_value = 0;
	MTK_WCN_STP_SDIO_HIF_INFO *p_info = gp_info;

	STPSDIO_PR_HINT("disable IRQ\n");
	/*Disable Common interrupt output in CHLPCR */
/* [COHEC_00006052] SW work-around solution:
 * using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR
 */
#if COHEC_00006052
	write_value = C_FW_INT_EN_CLR;
	iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
			&write_value, 0);
#else
	write_value = C_FW_INT_EN_CLR;
	iRet = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
			&write_value, 0);
#endif				/* COHEC_00006052 */
	if (iRet)
		STPSDIO_PR_ERR("disalbe IRQ fail\n");
	else {
		iRet = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
				&chlcpr_value, 0);
		if (iRet)
			STPSDIO_PR_ERR("read CHLPCR fail. iRet(%d)\n", iRet);
		else {
			if (!(chlcpr_value & C_FW_INT_EN_SET)) {
				STPSDIO_PR_DBG("disable COM IRQ okay (0x%x)\n", chlcpr_value);
				p_info->irq_pending = 1;

				/*inform stp_sdio thread to to rx/tx job */
				STPSDIO_PR_DBG("signal stp_tx_rx\n");
				osal_trigger_event(&gp_info->tx_rx_event);
				osal_ftrace_print("%s|stp_tx_rx\n", __func__);
			} else
				STPSDIO_PR_ERR
				    ("**********disable COM IRQ fail, don't signal stp-sdio thread******\n");
		}

	}
	return 0;

}

#else
/*!
 * \brief Handle STP-SDIO interrupt
 *
 * \details Top half interrupt handler of STP-SDIO. Most of Tx/Rx jobs are put
 *      to bottom half workers respectively.
 *
 * \note Rx ok interrupt shall be asserted by hw ONLY after last data are all
 *      read by driver. Do sanity check on rx_pkt_len and should be 0: rx BH
 *      finished.
 *
 * \note Tx complete count shall be handled atomically TH here and BH in
 *      tx_worker.
 *
 * \param[IN] clt_ctx A HIF-SDIO client context
 *
 * \retval 0 success
 * \retval !=0 fail
 */
static INT32 stp_sdio_irq(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx)
{
	/*MTK_WCN_STP_SDIO_PRIVATE_INFO *p_priv; */
	MTK_WCN_STP_SDIO_HIF_INFO *p_info = NULL;
	UINT32 chisr = 0;
	UINT32 tx_comp;
	INT32 ret;

#if 0
	   p_priv = mtk_wcn_hif_sdio_get_drvdata(clt_ctx);
TODO:[FixMe][George] do sanity check !
	   p_info = &g_stp_sdio_host_info[p_priv->stp_sdio_host_idx];
#endif
	p_info = gp_info;
	ret = HIF_SDIO_ERR_SUCCESS;
	/* 4 <0> get CHLPCR information */
	if (stp_sdio_get_own_state() == OWN_CLR)
		STPSDIO_PR_DBG("OWN on driver side!\n");
	else {
		STPSDIO_PR_DBG("OWN on fw side!\n");
		if (stp_sdio_do_own_clr(0) == 0)
			STPSDIO_PR_DBG("set OWN to driver side ok!\n");
		else {
			STPSDIO_PR_ERR("set OWN to driver side error!\n");
			return -1;
		}

	}
retry:
	/* 4 <1> get CHISR information */
	ret = mtk_wcn_hif_sdio_readl(clt_ctx, CHISR, &chisr);
	if (ret) {
		/* 4 <1.1> get CHISR Rx error handling */
		/* TODO: error handling! */
		STPSDIO_PR_ERR("get CHISR information rx error,ret:%d\n", ret);
		if (ret == -5) {
			STPSDIO_PR_ERR("get CHISR DAT CRC error, retry.\n");
			ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_MAX_RETRY_NUM, clt_ctx, CSR, &chisr,
					0);
		} else
			goto retry;
	}
	STPSDIO_PR_HINT("CHISR(0x%08x)\n", chisr);
	if (chisr == 0x0) {
		STPSDIO_PR_ERR("******CHISR == 0*****\n");
		return 0;
	}
	/* 4 <2> handle ownership back interrupt */
	if (chisr & FW_OWN_BACK_INT) {
		STPSDIO_PR_INFO("FW_OWN_BACK_INT\n");

		if (is_wait_ownback) {
			is_wait_ownback = 0;
			wake_up(&g_ownback_done);
		} else {
			mtk_wcn_stp_wmt_sdio_host_awake();
			/* if (cmb_bgf_eirq_cb) { */
			/* (*cmb_bgf_eirq_cb)(); */
			/* } */
		}
	}
	/* 4 <3> handle Rx interrupt */
	if (chisr & RX_DONE) {
		/* STPSDIO_PR_INFO("RX_DONE_INT\n"); */

		/* TODO: [FixMe][George] testing... */
		if (p_info->rx_pkt_len)
			STPSDIO_PR_ERR("rx worker is not finished yet!!!(%d)\n",
					 p_info->rx_pkt_len);

		/* get Rx packet length */
		p_info->rx_pkt_len = (chisr & RX_PKT_LEN) >> 16;
		STPSDIO_PR_HINT("rx_pkt_len(%d)\n", p_info->rx_pkt_len);
		/* sanity check */
		if ((p_info->rx_pkt_len == 0)
		    || (p_info->rx_pkt_len > STP_SDIO_RX_FIFO_SIZE)) {
			STPSDIO_PR_ERR
			    ("abnormal rx_pkt_len(%d) in CHISR(0x%08x) skip rx_worker\n",
			     p_info->rx_pkt_len, chisr);
			p_info->rx_pkt_len = 0;
		} else {
			/* Before host driver read all rx data, chip/fw will not send more data
			 * to host. No need to mask rx interrupt. schedule rx worker to get data
			 * back and handle it.
			 */
			if (p_info->rx_pkt_len & 0x3) {
				STPSDIO_PR_WARN
				    ("rx data len is not 4 bytes allignment, CHISR(0x%08x), rx len (%d).\n",
				     chisr, p_info->rx_pkt_len);
			}
#if STP_SDIO_OWN_THREAD
			/* tasklet_schedule(&p_info->tx_rx_job); */
			STPSDIO_PR_DBG("osal_trigger_event gp_info->tx_rx_event\n");
			osal_trigger_event(&gp_info->tx_rx_event);
#else
			schedule_work(&p_info->rx_work);
#endif
		}
	}
	/* 4 <4> handle Tx interrupt */
	if ((chisr & TX_EMPTY) || (chisr & TX_UNDER_THOLD)) {
		STPSDIO_PR_DBG("Tx interrupt\n");
		/* get complete count */
		tx_comp = (chisr & TX_COMPLETE_COUNT) >> 4;
#if 0
		atomic_add(tx_comp, &p_info->firmware_info.tx_comp_num);
		/* TODO:[FixMe][George]: debug and to be removed... */
		tx_comp = atomic_read(&p_info->firmware_info.tx_comp_num);
#else
		tx_comp = atomic_add_return(tx_comp, &p_info->firmware_info.tx_comp_num);
#endif
		if (tx_comp > STP_SDIO_TX_PKT_MAX_CNT) {
			STPSDIO_PR_ERR("Abnormal accumulated comp count(%d) chisr(0x%x)\n",
					 tx_comp, chisr);
		}

		/* move most of tx jobs to tx_worker */
		/* schedule tx worker for tx complete count and following tx data */
#if STP_SDIO_OWN_THREAD
		/* tasklet_schedule(&p_info->tx_rx_job); */
		STPSDIO_PR_DBG("osal_trigger_event gp_info->tx_rx_event\n");
		osal_trigger_event(&gp_info->tx_rx_event);
#else
		schedule_work(&p_info->tx_work);
#endif
	}

	return ret;
}
#endif /* STP_SDIO_NEW_IRQ_HANDLER */

#if STP_SDIO_POLL_OWNBACK_INTR
/*****************************************************************************
 * FUNCTION
 *  stp_sdio_ownback_poll
 * DESCRIPTION
 *   Poll ownback interrupt
 * PARAMETERS
 *  1. *func    [IN]    sdio driver function pointer
 *  2. retryp    [IN]    polling retry times
 *  3. delay_us    [IN]    polling delay (unit: us)
 * RETURNS
 *  ret:    Probe result
 *****************************************************************************/
static INT32 stp_sdio_ownback_poll(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx, UINT32 retry, UINT32 delay_us)
{
	INT32 ret;
	UINT32 chlpcr = 0;

	do {
#if 0
		ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHISR, &chisr_value,
				0);
#endif
		/* 20111020: change to poll CHLPCR instead of read-cleared CHISR */
		ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR, &chlpcr, 0);
		if (ret) {
			/* 4 <1.1> get CHISR Rx error handling */
			STPSDIO_PR_ERR("get CHLPCR information rx error!(%d)\n", ret);
			return ret;
		}

		/*if (chisr_value & FW_OWN_BACK_INT) { */
		if (chlpcr & C_FW_COM_DRV_OWN) {
			/* 4 <2> handle ownership back interrupt */
			STPSDIO_PR_DBG("Driver own is polled!(%d)\n", retry);
			gp_info->awake_flag = 1;
			break;
		}
		osal_usleep_range(delay_us, delay_us);
	} while (retry-- > 0);

	/*return (chisr_value & FW_OWN_BACK_INT) ? 0 : -HIF_SDIO_ERR_FAIL; */
	return (chlpcr & C_FW_COM_DRV_OWN) ? 0 : -HIF_SDIO_ERR_FAIL;
}
#endif /* STP_SDIO_POLL_OWNBACK_INTR */

/*****************************************************************************
 * FUNCTION
 *  stp_sdio_probe
 * DESCRIPTION
 *   Probe function of SDIO driver
 * PARAMETERS
 *  1. *func    [IN]    sdio driver function pointer
 *  2. *id      [IN]    sdio function id
 * RETURNS
 *  ret:    Probe result
 *****************************************************************************/
/* typedef INT32 (*MTK_WCN_HIF_SDIO_PROBE)(MTK_WCN_HIF_SDIO_CLTCTX, const MTK_WCN_HIF_SDIO_FUNCINFO *); */
static INT32 stp_sdio_probe(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx,
		const MTK_WCN_HIF_SDIO_FUNCINFO *sdio_func_infop)
{
	INT32 ret = HIF_SDIO_ERR_SUCCESS;
	UINT32 i = 0, chlcpr_value = 0;
	UINT32 write_value = 0;

	STPSDIO_PR_DBG("sdio_cltctx: 0x%08x\n", clt_ctx);

	if (g_stp_sdio_host_count) {
		STPSDIO_PR_ERR("g_stp_sdio_host_count(%d) probed already!\n",
				 g_stp_sdio_host_count);
		return -1;
	}
	/* 4 <1> check if input pointer is valid */
	if (g_stp_sdio_host_info.sdio_cltctx == clt_ctx) {
		STPSDIO_PR_WARN("sdio_cltctx(%d) already probed!\n", clt_ctx);
		return 0;
	}
	/* 4 <2> allocate host inform structure and initialize private variables */
	/* init host info private variables */
	g_stp_sdio_host_info.sdio_cltctx = clt_ctx;

	/* init Tx packet ring buffer */
	for (i = 0; i < STP_SDIO_TX_BUF_CNT; ++i) {
#if KMALLOC_UPDATE
		UINT8 *pData = g_stp_sdio_host_info.pkt_buf.tx_buf + i * STP_SDIO_TX_ENTRY_SIZE + 0;

		memset(pData, 0, STP_SDIO_TX_ENTRY_SIZE);
#else
		memset(&g_stp_sdio_host_info.pkt_buf.tx_buf[i][0], 0,
		       sizeof(g_stp_sdio_host_info.pkt_buf.tx_buf[i]));
#endif
	}
	osal_sleepable_lock_init(&fake_coredump_lock);

#if STP_SDIO_NEW_TXRING
	spin_lock_init(&g_stp_sdio_host_info.pkt_buf.rd_cnt_lock);
	atomic_set(&g_stp_sdio_host_info.pkt_buf.wr_cnt, 0);
	atomic_set(&g_stp_sdio_host_info.pkt_buf.rd_cnt, 0);
#else
	/*g_stp_sdio_host_info.pkt_buf.rd_idx_lock = SPIN_LOCK_UNLOCKED; */
	spin_lock_init(&g_stp_sdio_host_info.pkt_buf.rd_idx_lock);
	atomic_set(&g_stp_sdio_host_info.pkt_buf.wr_idx, 0);
	atomic_set(&g_stp_sdio_host_info.pkt_buf.rd_idx, 0);
#endif
	g_stp_sdio_host_info.pkt_buf.full_flag = MTK_WCN_BOOL_FALSE;

	/* init wait queue head for Tx ring buf full */
	init_waitqueue_head(&g_stp_sdio_host_info.pkt_buf.fullwait_q);

	/* init Tx packet size list information */
	memset(&g_stp_sdio_host_info.tx_pkt_list.pkt_size_list[0], 0,
	       sizeof(g_stp_sdio_host_info.tx_pkt_list.pkt_size_list));
	g_stp_sdio_host_info.tx_pkt_list.pkt_rd_cnt = 0;
	g_stp_sdio_host_info.tx_pkt_list.pkt_wr_cnt = 0;

	/* init Rx interrupt mask */
	/* g_stp_sdio_host_info.rx_intr_mask = MTK_WCN_BOOL_FALSE; */

	/* init firmware related information */
	g_stp_sdio_host_info.firmware_info.tx_fifo_size = STP_SDIO_TX_FIFO_SIZE;
	g_stp_sdio_host_info.firmware_info.tx_packet_num = 0;
	atomic_set(&g_stp_sdio_host_info.firmware_info.tx_comp_num, 0);

	/* init SDIO data path retry flag */
	g_stp_sdio_host_info.retry_enable_flag = 0;
	g_stp_sdio_host_info.tx_retry_flag = STP_SDIO_RETRY_NONE;

	g_stp_sdio_host_info.isr_check_complete.timeoutValue = 1000;
	osal_signal_init(&g_stp_sdio_host_info.isr_check_complete);

#if STP_SDIO_OWN_THREAD
	/* tasklet_init(&g_stp_sdio_host_info.tx_rx_job, stp_sdio_tx_rx_handling, */
	/*	(unsigned long) &g_stp_sdio_host_info); */
	/* Create stp_sdio_tx_rx_thread, in suspend state */
	g_stp_sdio_host_info.irq_pending = 0;
	g_stp_sdio_host_info.sleep_flag = 0;
	g_stp_sdio_host_info.wakeup_flag = 0;
	osal_event_init(&g_stp_sdio_host_info.tx_rx_event);
	g_stp_sdio_host_info.tx_rx_thread.pThreadFunc = stp_sdio_tx_rx_handling;
	g_stp_sdio_host_info.tx_rx_thread.pThreadData = (PVOID) &g_stp_sdio_host_info;
	g_stp_sdio_host_info.tx_dbg_dump_flag = 0;
	osal_strncpy((PINT8)&g_stp_sdio_host_info.tx_rx_thread.threadName,
		     "stp_sdio_tx_rx", osal_sizeof(g_stp_sdio_host_info.tx_rx_thread.threadName));
	ret = osal_thread_create(&g_stp_sdio_host_info.tx_rx_thread);
	if (ret < 0) {
		STPSDIO_PR_ERR("osal_thread_create fail...: %p\n",
				g_stp_sdio_host_info.tx_rx_thread.pThread);
		return ret;
	}
#else
	/* init tx_tasklet and rx work_queue */
	INIT_WORK(&g_stp_sdio_host_info.tx_work, stp_sdio_tx_wkr);
	INIT_WORK(&g_stp_sdio_host_info.rx_work, stp_sdio_rx_wkr);
#endif
	/* init stp sdio host private information *//* TODO: [FixMe][George] Still need this? */
	g_stp_sdio_host_info.private_info.stp_sdio_host_idx = g_stp_sdio_host_count;
	mtk_wcn_hif_sdio_set_drvdata(clt_ctx, &g_stp_sdio_host_info.private_info);

	++g_stp_sdio_host_count;

	/* 4 <3> request firmware-own back */
/* [COHEC_00006052] SW work-around solution: */
/* using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR */
#if COHEC_00006052
	write_value = (C_FW_OWN_REQ_CLR >> 8);
	ret = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, clt_ctx, (UINT32)(CHLPCR + 0x1),
			&write_value, 0);
#else
	write_value = C_FW_OWN_REQ_CLR;
	ret = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR,
			&write_value, 0);
#endif				/* COHEC_00006052 */
	if (ret) {
		STPSDIO_PR_ERR("request FW-Own back fail!(%d)\n", ret);
		goto out;
	}
	STPSDIO_PR_DBG("request FW-Own back done\n");

#if STP_SDIO_POLL_OWNBACK_INTR
	/* 4 <3.1> polling own back bit */
	ret = stp_sdio_ownback_poll(clt_ctx, 10, 100);
	if (ret) {
		STPSDIO_PR_ERR("poll FW-Own back fail!(%d)\n", ret);
		goto out;
	}
#endif
	/* 4 <4.0> enable irq flag in HIF-SDIO */
	mtk_wcn_hif_sdio_enable_irq(clt_ctx, MTK_WCN_BOOL_TRUE);
	/* 4 <4> enabling all host interrupt except abnormal ones */
	/*write_value = (CHISR_EN_15_7 | CHISR_EN_3_0);*/ /* enable CHISR interrupt output */
	/*ret = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHIER, &write_value, 0);*/
	write_value = (FIRMWARE_INT | TX_FIFO_OVERFLOW | FW_INT_IND_INDICATOR | TX_COMPLETE_COUNT
			| TX_UNDER_THOLD | TX_EMPTY | RX_DONE);
	ret = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHIER,
			&write_value, 0);
	if (ret) {
		STPSDIO_PR_ERR("set interrupt output fail!(%d)\n", ret);
		goto out;
	}
	STPSDIO_PR_DBG("set interrupt output done\n");
/* [COHEC_00006052] SW work-around solution: */
/* using CMD52 write instead of CMD53 write for CCIR, CHLPCR, CSDIOCSR */
#if COHEC_00006052
	write_value = C_FW_INT_EN_SET;
	ret = stp_sdio_rw_retry(HIF_TYPE_WRITEB, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR, &write_value,
			0);	/* enable interrupt */
#else
	write_value = C_FW_INT_EN_SET;
	ret = stp_sdio_rw_retry(HIF_TYPE_WRITEL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR, &write_value,
			0);	/* enable interrupt */
#endif				/* COHEC_00006052 */
	if (ret) {
		STPSDIO_PR_ERR("enable interrupt fail!(%d)\n", ret);
		goto out;
	}
	ret = stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR, &chlcpr_value,
			0);
	if (ret) {
		STPSDIO_PR_ERR("Read CHLPCR fail!(%d)\n", ret);
		goto out;
	} else {
		if (chlcpr_value & C_FW_INT_EN_SET)
			STPSDIO_PR_DBG("enable interrupt okay (0x%x)\n", chlcpr_value);
	}

	STPSDIO_PR_DBG("enable interrupt done\n");

#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
	stp_sdio_deep_sleep_flag_set(MTK_WCN_BOOL_FALSE);
#endif

#if STP_SDIO_OWN_THREAD
	ret = osal_thread_run(&g_stp_sdio_host_info.tx_rx_thread);
	if (ret < 0) {
		STPSDIO_PR_ERR("osal_thread_run fail...\n");
		goto out;
	}
#endif

	/* 4 <5> register mtk_wcn_stp_if_tx() to stp driver */
	mtk_wcn_stp_register_if_tx(STP_SDIO_IF_TX, (MTK_WCN_STP_IF_TX) stp_sdio_tx);

#if 0				/* controlled by 6620_launcher & WMT */
	/*set STP sdio mode */
	pr_warn(DFT_TAG "%s: set stp sdio mode\n", __func__);
	mtk_wcn_stp_set_sdio_mode(1);

	/*indicate the stp the sdio ready */
	pr_warn(DFT_TAG "%s: stp enable\n", __func__);
	mtk_wcn_stp_enable(1);
#endif

out:
	/* 4 <6> error handling */
	/* TODO: error handling */
	if (ret) {
#if STP_SDIO_OWN_THREAD
		osal_thread_destroy(&g_stp_sdio_host_info.tx_rx_thread);
#endif
		if (g_stp_sdio_host_count > 0)
			--g_stp_sdio_host_count;
	}

	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  stp_sdio_probe
 * DESCRIPTION
 *   SDIO hardware remove function.
 * PARAMETERS
 *  *func    [IN]    SDIO driver handler pointer.
 * RETURNS
 *  none.
 *****************************************************************************/
static INT32 stp_sdio_remove(const MTK_WCN_HIF_SDIO_CLTCTX clt_ctx)
{
#if 0
	MTK_WCN_STP_SDIO_PRIVATE_INFO *p_priv;

	p_priv = mtk_wcn_hif_sdio_get_drvdata(clt_ctx);
#endif

	if (g_stp_sdio_host_info.sdio_cltctx == clt_ctx)
		STPSDIO_PR_DBG("sdio_cltctx(%d) found\n", clt_ctx);
	else {
		STPSDIO_PR_ERR("sdio_cltctx(%d) not found\n", clt_ctx);
		return -1;
	}
	osal_sleepable_lock_deinit(&fake_coredump_lock);
	if (g_stp_sdio_host_count > 0)
		--g_stp_sdio_host_count;
	/* 4 <0> disable irq flag in HIF-SDIO */
	mtk_wcn_hif_sdio_enable_irq(clt_ctx, MTK_WCN_BOOL_FALSE);
	/* 4 <1> unregister if_tx() function */
	mtk_wcn_stp_register_if_tx(STP_SDIO_IF_TX, NULL);

	/* <2> stop Tx tasklet/Rx work queue of the host */
#if STP_SDIO_OWN_THREAD
	/* tasklet_kill(&g_stp_sdio_host_info.tx_rx_job); */
	/* STPSDIO_PR_INFO("kill tasklet finished\n"); */
	osal_thread_destroy(&g_stp_sdio_host_info.tx_rx_thread);
	osal_event_deinit(&g_stp_sdio_host_info.tx_rx_event);
	osal_signal_deinit(&g_stp_sdio_host_info.isr_check_complete);
	STPSDIO_PR_DBG("destroy STP-SDIO tx_rx_thread\n");
#else
	flush_scheduled_work();
	STPSDIO_PR_INFO("flush scheduled work end\n");
#endif

	/* 4 <3> return ownership to firmware of the host */
	/* TODO: check set which register ! */

	/* 4 <4> clear the host struct list */
	stp_sdio_host_info_op(1);


	STPSDIO_PR_DBG("clear g_stp_sdio_host_info[p_priv->stp_sdio_host_idx] done\n");

	return 0;
}

INT32 stp_sdio_rw_retry(ENUM_STP_SDIO_HIF_TYPE_T type, UINT32 retry_limit,
		MTK_WCN_HIF_SDIO_CLTCTX clt_ctx, UINT32 offset, PUINT32 pData, UINT32 len)
{
	INT32 ret = -1;
	INT32 ret_1 = -1;
	UINT32 value = 0;
	INT32 retry_flag = 0;

	UINT32 card_id = CLTCTX_CID(clt_ctx);

	if (card_id != 0x6630 && card_id != 0x6632) {
		STPSDIO_PR_LOUD("card_id is :0x%x, does not support CSR (Common Snapshot Register)\n",
				card_id);
		retry_limit = 1;
	}
	STPSDIO_PR_LOUD("clt_ctx:0x%x, offset:0x%x, retry_limit:%d\n", clt_ctx, offset, retry_limit);

	retry_limit = retry_limit == 0 ? 1 : retry_limit;
	retry_limit = retry_limit > STP_SDIO_MAX_RETRY_NUM ? STP_SDIO_MAX_RETRY_NUM : retry_limit;

	while (retry_limit > 0) {
		switch (type) {
		case HIF_TYPE_READB:
			if (retry_flag)
				ret = mtk_wcn_hif_sdio_readb(clt_ctx, CSR, (PUINT8)pData);
			else
				ret = mtk_wcn_hif_sdio_readb(clt_ctx, offset, (PUINT8)pData);
			break;
		case HIF_TYPE_READL:
			if (retry_flag)
				ret = mtk_wcn_hif_sdio_readl(clt_ctx, CSR, pData);
			else
				ret = mtk_wcn_hif_sdio_readl(clt_ctx, offset, pData);
			break;
		case HIF_TYPE_READ_BUF:
			ret = mtk_wcn_hif_sdio_read_buf(clt_ctx, offset, pData, len);
			break;
		case HIF_TYPE_WRITEB:
			ret = mtk_wcn_hif_sdio_writeb(clt_ctx, offset, (UINT8)(*pData));
			break;
		case HIF_TYPE_WRITEL:
			ret = mtk_wcn_hif_sdio_writel(clt_ctx, offset, *pData);
			break;
		case HIF_TYPE_WRITE_BUF:
			ret = mtk_wcn_hif_sdio_write_buf(clt_ctx, offset, pData, len);
			break;
		default:
			STPSDIO_PR_ERR("unknown hif sdio type!\n");
			goto exit;
		}

		if (ret) {
			STPSDIO_PR_ERR("sdio read or write failed, ret:%d\n", ret);
			if (ret == -ETIMEDOUT) {
				ret_1 = mtk_wcn_hif_sdio_readl(clt_ctx, CCIR, &value);
				STPSDIO_PR_ERR("sdio read or write timeout, ret:%d ret_1:%d, read chip id:%x\n",
						ret, ret_1, value);
				stp_sdio_dump_register();
			}
			/* sdio CRC error read CSR */
			if (type == HIF_TYPE_READ_BUF || type == HIF_TYPE_WRITE_BUF) {
				if (ret == -EIO || ret == -EILSEQ || ret == -EBUSY) {
					ret_1 = mtk_wcn_hif_sdio_abort(clt_ctx);
					if (ret_1)
						STPSDIO_PR_ERR("sdio crc error send abort fail, ret_1:%d\n",
								ret_1);
					else
						STPSDIO_PR_ERR("sdio crc error send abort success, ret_1:%d\n",
								ret_1);
					goto exit;
				}
			} else
				retry_flag = 1;
		} else {
			STPSDIO_PR_LOUD("CR:0x:%x value:0x%x\n", offset, *pData);
			break;
		}
		retry_limit--;
	}

exit:
	return ret;
}


#if STP_SDIO_DBG_SUPPORT && STP_SDIO_RXDBG

/*!
 * \brief /proc debug read interface and dump rx dbg information
 *
 * \details Dump all rx debug information to console.
 *
 * \retval 0 success
 */
ssize_t stp_sdio_rxdbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT32 idx;
	UINT32 i;
	UINT32 j;
	PUINT8 pbuf;
	UINT32 len;

	if (*f_pos > 0)
		return 0;

	for (i = 0; i < STP_SDIO_RXDBG_COUNT; ++i) {
		idx = (stp_sdio_rxdbg_cnt - 1 - i) & STP_SDIO_TXDBG_COUNT_MASK;
		len = stp_sdio_rxdbg_buffer[idx].bus_rxlen;
		if (len == 0) {
			pr_warn(DFT_TAG "idx(0x%x) 0 == len dump skip\n",
				stp_sdio_rxdbg_cnt);
		}
		pr_warn(DFT_TAG "idx(0x%x) chisr_rxlen(%d) bus_rxlen(%d) ts(%d)\n",
			stp_sdio_rxdbg_cnt, stp_sdio_rxdbg_buffer[idx].chisr_rxlen, len,
			stp_sdio_rxdbg_buffer[idx].ts);
		for (j = 0; j < STP_SDIO_RX_BUF_SIZE && j < len; j += 16) {
			pbuf = &stp_sdio_rxdbg_buffer[idx].rx_pkt_buf[j];
			pr_warn(DFT_TAG "[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ",
					pbuf[0], pbuf[1], pbuf[2], pbuf[3], pbuf[4], pbuf[5], pbuf[6],
					pbuf[7]);
			pr_warn(DFT_TAG "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
				pbuf[8], pbuf[9], pbuf[10], pbuf[11], pbuf[12], pbuf[13],
				pbuf[14], pbuf[15]);
			msleep(20);
		}
		pr_warn(DFT_TAG "dump ok\n");
	}

	return 0;
}

/*!
 * \brief /proc debug write interface. do nothing.
 *
 * \details
 *
 * \retval 0 success
 */
ssize_t stp_sdio_rxdbg_write(struct file *filp, const char __user *buf, size_t count,
			     loff_t *f_pos)
{
	ULONG len = count;

	pr_warn(DFT_TAG "write parameter len = %lu\n\r", len);

	return len;
}

/*!
 * \brief /proc initial procedures. Initialize global debugging information.
 *
 * \details Setup entry for /proc debugging for rx
 *
 * \retval 0 success
 */
INT32 stp_sdio_rxdbg_setup(VOID)
{
	stp_sdio_rxdbg_cnt = 0;

	gStpSdioRxDbgEntry = proc_create(STP_SDIO_RXDBG_PROCNAME, 0644, NULL, &stp_sdio_rxdbg_fops);
	if (gStpSdioRxDbgEntry == NULL) {
		pr_warn(DFT_TAG "Unable to create /proc entry\n\r");
		return -1;
	}

	return 0;
}

/*!
 * \brief /proc de-init procedures.
 *
 * \details remove entry for /proc debugging for rx
 *
 * \retval 0 success
 */
INT32 stp_sdio_rxdbg_remove(VOID)
{
	if (gStpSdioRxDbgEntry != NULL)
		proc_remove(gStpSdioRxDbgEntry);

	return 0;
}
#endif

#if STP_SDIO_DBG_SUPPORT && (STP_SDIO_TXDBG || STP_SDIO_TXPERFDBG)

static VOID stp_sdio_txperf_dump(VOID)
{
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXPERFDBG
	UINT32 cnt;
	UINT32 fifo;
	UINT32 data;
	UINT32 wkr;
	UINT32 pkt_num;
	UINT32 lmt_cnt;

	/* get debug counter snapshot */
	cnt = stp_sdio_txperf_fifo_lmt_cnt;
	fifo = stp_sdio_txperf_fifo_left;
	data = stp_sdio_txperf_to_send;

	wkr = stp_sdio_txperf_worker_cnt;
	pkt_num = stp_sdio_txperf_txed_pkt_num;
	lmt_cnt = stp_sdio_txperf_pkt_num_lmt_cnt;

	pr_warn(DFT_TAG "txwait_fifo_left(%d), txwait_to_send(%d), txwait_count(%d)\n",
		fifo, data, cnt);
	if (cnt)
		pr_warn(DFT_TAG "avg left(%d), to_send(%d)\n", (fifo / cnt), (data / cnt));
	pr_warn(DFT_TAG "tx_worker_cnt(%d), pkt_num(%d), pkt_num_lmt_cnt(%d)\n",
		wkr, pkt_num, lmt_cnt);

#endif
}

VOID stp_sdio_dump_info(MTK_WCN_STP_SDIO_HIF_INFO *p_info)
{
	STPSDIO_PR_INFO("stp_is_ready(%d) irq_pending(%d) tx_packet_num(%d) rx_pkt_len(%d)\n",
			mtk_wcn_stp_is_ready(), p_info->irq_pending,
			p_info->firmware_info.tx_packet_num, p_info->rx_pkt_len);
	STPSDIO_PR_INFO("sleep_flag(%d) wakeup_flag(%d) awake_flag(%d) txwkr_flag(%d)\n",
			p_info->sleep_flag, p_info->wakeup_flag, p_info->awake_flag, p_info->txwkr_flag);
	STPSDIO_PR_INFO("wr_idx(%d), rd_idx(%d), full_flag(%d), tx_fifo_size(%d)\n",
			atomic_read(&p_info->pkt_buf.wr_idx), atomic_read(&p_info->pkt_buf.rd_idx),
			p_info->pkt_buf.full_flag, p_info->firmware_info.tx_fifo_size);
}

VOID stp_sdio_txdbg_dump(VOID)
{
#if STP_SDIO_TXDBG
	UINT32 idx;
	UINT32 i;
	UINT32 j;
	PUINT8 pbuf;
	UINT32 len;

	for (i = 0; i < STP_SDIO_TXDBG_COUNT; ++i) {
		idx = (stp_sdio_txdbg_cnt - 1 - i) & STP_SDIO_TXDBG_COUNT_MASK;
		len = stp_sdio_txdbg_buffer[idx].bus_txlen;
		if (len == 0) {
			STPSDIO_PR_INFO("idx(%x) 0 == len dump skip\n", idx);
			continue;
		}

		len = len > STP_SDIO_TXDBG_MAX_SIZE ? STP_SDIO_TXDBG_MAX_SIZE : len;
		STPSDIO_PR_INFO(
				"stp_sdio_txdbg_buffer idx(%x) bus_txlen(0x%x, %d), time[%llu.%06lu]\n",
				idx, len, len, stp_sdio_txdbg_buffer[idx].l_sec, stp_sdio_txdbg_buffer[idx].l_nsec);
		for (j = 0; j < STP_SDIO_TX_ENTRY_SIZE && j < len; j += 16) {
			pbuf = &stp_sdio_txdbg_buffer[idx].tx_pkt_buf[j];
			STPSDIO_PR_INFO("[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
				pbuf[0], pbuf[1], pbuf[2], pbuf[3], pbuf[4], pbuf[5], pbuf[6], pbuf[7]);
			STPSDIO_PR_INFO("[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
				pbuf[8], pbuf[9], pbuf[10], pbuf[11], pbuf[12], pbuf[13], pbuf[14],
				pbuf[15]);
			msleep(20);
		}
		STPSDIO_PR_INFO("stp_sdio_txdbg_buffer dump ok\n");
	}
#if STP_TXDBG
	for (i = 0; i < STP_SDIO_TXDBG_COUNT; ++i) {
		idx = (stp_sdio_txdbg_cnt - 1 - i) & STP_SDIO_TXDBG_COUNT_MASK;
		len = stp_sdio_txdbg_buffer[idx].bus_txlen;
		STPSDIO_PR_INFO(
			"stp_sdio_txdbg_buffer idx(%x) bus_txlen(0x%x, %d) ts(%d)\n", idx, len, len,
			stp_sdio_txdbg_buffer[idx].ts);
	}
	STPSDIO_PR_INFO(
			"Dump tx info: pkt_num(%d) fifo(%d) pkt_list.rd(0x%x, %ld) pkt_list.wr(0x%x, %ld)\n",
			gp_info->firmware_info.tx_packet_num, gp_info->firmware_info.tx_fifo_size,
			gp_info->tx_pkt_list.pkt_rd_cnt,
			STP_SDIO_GET_PKT_AR_IDX(gp_info->tx_pkt_list.pkt_rd_cnt),
			gp_info->tx_pkt_list.pkt_wr_cnt,
			STP_SDIO_GET_PKT_AR_IDX(gp_info->tx_pkt_list.pkt_wr_cnt));

	for (i = 0; i < STP_SDIO_TX_PKT_LIST_SIZE; ++i) {
		idx = STP_SDIO_GET_PKT_AR_IDX(gp_info->tx_pkt_list.pkt_wr_cnt - 1 - i);
		STPSDIO_PR_INFO(
				"tx_pkt_list idx(0x%x, %d) size(0x%x, %d), in_ts(%d), out_ts(%d)\n",
				(gp_info->tx_pkt_list.pkt_wr_cnt - 1 - i), idx,
				gp_info->tx_pkt_list.pkt_size_list[idx],
				gp_info->tx_pkt_list.pkt_size_list[idx], gp_info->tx_pkt_list.in_ts[idx],
				gp_info->tx_pkt_list.out_ts[idx]);
	}

#if STP_SDIO_NEW_TXRING
	STPSDIO_PR_INFO("\n\ndump pkt_buf.tx_buf: rd(%d) wr(%d) full(%d)\n",
			atomic_read(&gp_info->pkt_buf.rd_cnt), atomic_read(&gp_info->pkt_buf.wr_cnt),
			gp_info->pkt_buf.full_flag);
#else
	STPSDIO_PR_INFO("\n\ndump pkt_buf.tx_buf: rdi(%d) wri(%d) full(%d)\n",
			atomic_read(&gp_info->pkt_buf.rd_idx), atomic_read(&gp_info->pkt_buf.wr_idx),
			gp_info->pkt_buf.full_flag);
#endif

	for (i = 0; i < STP_SDIO_TX_BUF_CNT; ++i) {
#if STP_SDIO_NEW_TXRING
		idx = (atomic_read(&gp_info->pkt_buf.wr_cnt) - 1 - i) & STP_SDIO_TX_BUF_CNT_MASK;
		len = gp_info->pkt_buf.tx_buf_sz[idx];
#else
		idx = (atomic_read(&gp_info->pkt_buf.wr_idx) - 1 - i + STP_SDIO_TX_BUF_CNT) %
			STP_SDIO_TX_BUF_CNT;
#if KMALLOC_UPDATE
		len = *(gp_info->pkt_buf.tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + 1);
		len = (len << 8) | *(gp_info->pkt_buf.tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + 0);
#else
		len = gp_info->pkt_buf.tx_buf[idx][1];
		len = (len << 8) | gp_info->pkt_buf.tx_buf[idx][0];
#endif

#endif
		STPSDIO_PR_INFO("pkt_buf.tx_buf idx(%x) ts(%d) len(%d), time[%llu.%06lu]\n",
				idx, gp_info->pkt_buf.tx_buf_ts[idx], len,
				gp_info->pkt_buf.tx_buf_local_ts[idx], gp_info->pkt_buf.tx_buf_local_nsec[idx]);
		if (len == 0) {
			STPSDIO_PR_INFO("idx(%x) 0 == len dump skip\n", idx);
			continue;
		}
		len = len > STP_SDIO_TXDBG_MAX_SIZE ? STP_SDIO_TXDBG_MAX_SIZE : len;
		for (j = 0; j < STP_SDIO_TX_ENTRY_SIZE && j < len; j += 16) {
#if KMALLOC_UPDATE
			pbuf = gp_info->pkt_buf.tx_buf + idx * STP_SDIO_TX_ENTRY_SIZE + j;
#else
			pbuf = &gp_info->pkt_buf.tx_buf[idx][j];
#endif
			STPSDIO_PR_INFO("[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
					pbuf[0], pbuf[1], pbuf[2], pbuf[3], pbuf[4], pbuf[5], pbuf[6],
					pbuf[7]);
			STPSDIO_PR_INFO("[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n",
					pbuf[8], pbuf[9], pbuf[10], pbuf[11], pbuf[12], pbuf[13], pbuf[14],
					pbuf[15]);
			msleep(20);
		}
		STPSDIO_PR_INFO("pkt_buf.tx_buf dump ok\n");
	}
#endif				/*end of STP_TXDBG*/
#endif				/* end of STP_SDIO_TXDBG */
}

/*!
 * \brief /proc debug read interface and dump tx dbg information
 *
 * \details Dump all tx debug information to console.
 *
 * \retval 0 success
 */
ssize_t stp_sdio_txdbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (*f_pos > 0)
		return 0;

	stp_sdio_txdbg_dump();
	stp_sdio_txperf_dump();

	return 0;
}

/*!
 * \brief /proc debug write interface. do nothing.
 *
 * \details
 *
 * \retval 0 success
 */
ssize_t stp_sdio_txdbg_write(struct file *filp, const char __user *buf, size_t count,
			     loff_t *f_pos)
{
	ULONG len = count;

	pr_warn(DFT_TAG "write parameter len = %lu\n\r", len);

	return len;
}

/*!
 * \brief /proc debug read interface and dump tx dbg information
 *
 * \details Dump all tx debug information to console.
 *
 * \retval 0 success
 */
ssize_t stp_sdio_own_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (*f_pos > 0)
		return 0;

	return 0;
}

/*!
 * \brief /proc debug write interface. do nothing.
 *
 * \details
 *
 * \retval 0 success
 */
ssize_t stp_sdio_own_write(struct file *filp, const char __user *buffer, size_t count,
			   loff_t *f_pos)
{
	ULONG len = count;
	PINT8 pBuf = NULL;
	PINT8 pToken = NULL;
	PINT8 pDelimiter = " \t";
	INT32 x = 0;
	INT8 buf[128] = { 0 };
	LONG res = 0;

	if (len >= osal_sizeof(buf)) {
		STPSDIO_PR_ERR("input handling fail!\n");
		len = osal_sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len)) {
		STPSDIO_PR_ERR("copy_from_user error.\n");
		return -EFAULT;
	}

	buf[len] = '\0';
	pBuf = buf;
	pToken = osal_strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x = (INT32)res;
	} else {
		x = 0;
	}

	if (x == 0) {
		STPSDIO_PR_INFO("stp_sdio_own_ctrl(OWN_CLR)\n\r");
		stp_sdio_own_ctrl(OWN_CLR);
	} else if (x == 2) {
		STPSDIO_PR_INFO("stp_sdio_own_ctrl(OWN_SET) -->Sleep\n\r");
		stp_sdio_own_ctrl(OWN_SET);
	} else if (x == 3) {
		gStpSdioDbgLvl = STPSDIO_LOG_WARN;
		STPSDIO_PR_WARN("set STP-SDIO LogLevel to STPSDIO_LOG_WARN\n\r");
	} else if (x == 4) {
		gStpSdioDbgLvl = STPSDIO_LOG_INFO;
		STPSDIO_PR_INFO("set STP-SDIO LogLevel to STPSDIO_LOG_INFO\n\r");
	} else if (x == 5) {
		gStpSdioDbgLvl = STPSDIO_LOG_HINT;
		STPSDIO_PR_INFO("set STP-SDIO LogLevel to STPSDIO_LOG_HINT\n\r");
	} else if (x == 6) {
		gStpSdioDbgLvl = STPSDIO_LOG_DBG;
		STPSDIO_PR_INFO("set STP-SDIO LogLevel to STPSDIO_LOG_DBG\n\r");
	} else if (x == 7) {
		gStpSdioDbgLvl = STPSDIO_LOG_LOUD;
		STPSDIO_PR_INFO("set STP-SDIO LogLevel to STPSDIO_LOG_LOUD\n\r");
	}

	return len;
}


/*!
 * \brief /proc initial procedures. Initialize global debugging information.
 *
 * \details Setup entry for /proc debugging for tx
 *
 * \retval 0 success
 */
INT32 stp_sdio_txdbg_setup(VOID)
{
	gStpSdioTxDbgEntry = proc_create(STP_SDIO_TXDBG_PROCNAME, 0644, NULL, &stp_sdio_txdbg_fops);
	if (gStpSdioTxDbgEntry == NULL) {
		pr_warn(DFT_TAG "Unable to create /proc entry\n\r");
		return -1;
	}

#if STP_SDIO_TXPERFDBG
	stp_sdio_txperf_worker_cnt = 0;

	stp_sdio_txperf_fifo_left = 0;
	stp_sdio_txperf_to_send = 0;
	stp_sdio_txperf_fifo_lmt_cnt = 0;

	stp_sdio_txperf_txed_pkt_num = 0;
	stp_sdio_txperf_pkt_num_lmt_cnt = 0;
#endif

#if STP_SDIO_TXDBG
	stp_sdio_txdbg_cnt = 0;
#endif

	return 0;
}

/*!
 * \brief /proc de-init procedures.
 *
 * \details remove entry for /proc debugging for tx
 *
 * \retval 0 success
 */
INT32 stp_sdio_txdbg_remove(VOID)
{
	if (gStpSdioTxDbgEntry != NULL)
		proc_remove(gStpSdioTxDbgEntry);

	return 0;
}

#endif				/* end of STP_SDIO_DBG_SUPPORT && (STP_SDIO_TXDBG || STP_SDIO_TXPERFDBG) */

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_OWNBACKDBG

/*!
 * \brief /proc initial procedures. Initialize global debugging information.
 *
 * \details Setup entry for /proc debugging for tx
 *
 * \retval 0 success
 */
INT32 stp_sdio_owndbg_setup(VOID)
{
	gStpSdioOwnEntry = proc_create(STP_SDIO_OWNDBG_PROCNAME, 0644, NULL, &stp_sdio_own_fops);
	if (gStpSdioOwnEntry == NULL) {
		pr_warn(DFT_TAG "Unable to create /proc entry\n\r");
		return -1;
	}

	return 0;
}



/*!
 * \brief /proc de-init procedures.
 *
 * \details remove entry for /proc debugging for tx
 *
 * \retval 0 success
 */
INT32 stp_sdio_owndbg_remove(VOID)
{
	if (gStpSdioOwnEntry != NULL)
		proc_remove(gStpSdioOwnEntry);

	return 0;
}
#endif

/*!
 * \brief hif_sdio init function
 *
 * detailed descriptions
 *
 * \retval
 */
static INT32 stp_sdio_init(VOID)
{
	INT32 ret;
	INT32 i;

	/* 4 <1> initialize all private variables */
	stp_sdio_host_info_op(1);
	g_stp_sdio_host_count = 0;
	/* Init stp sdio client info */
#if 0				/* George: chage to be a constant struct */
	g_stp_sdio_cltinfo.func_tbl = mtk_stp_sdio_id_tbl;
	g_stp_sdio_cltinfo.func_tbl_size =
	    sizeof(mtk_stp_sdio_id_tbl) / sizeof(MTK_WCN_HIF_SDIO_FUNCINFO) - 1;
	g_stp_sdio_cltinfo.hif_clt_irq = stp_sdio_irq;
	g_stp_sdio_cltinfo.hif_clt_probe = stp_sdio_probe;
	g_stp_sdio_cltinfo.hif_clt_remove = stp_sdio_remove;
#endif

	STPSDIO_PR_DBG("cltinfo func table size:%d\n", g_stp_sdio_cltinfo.func_tbl_size);
	for (i = 0; i < g_stp_sdio_cltinfo.func_tbl_size; i++) {
		STPSDIO_PR_DBG("manf_id:0x%x, card_id:0x%x, func_num:%d, blk_size:%d\n",
				  mtk_stp_sdio_id_tbl[i].manf_id, mtk_stp_sdio_id_tbl[i].card_id,
				  mtk_stp_sdio_id_tbl[i].func_num, mtk_stp_sdio_id_tbl[i].blk_sz);
	}

	/* 4 <2> register supported functions from sdio id table to hif sdio driver */
	ret = mtk_wcn_hif_sdio_client_reg(&g_stp_sdio_cltinfo);
	if (ret)
		STPSDIO_PR_ERR("mtk_wcn_hif_sdio_client_reg fail(%d)!\n", ret);

	ret = mtk_wcn_stp_wmt_sdio_op_reg(stp_sdio_own_ctrl);
	if (ret)
		STPSDIO_PR_ERR
		    ("mtk_wcn_stp_wmt_sdio_op_reg(mtk_wcn_stp_sdio_own_ctrl) fail(%d)!\n", ret);
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
	mtk_wcn_wmt_sdio_deep_sleep_flag_cb_reg(stp_sdio_deep_sleep_flag_set);
#endif
	mtk_wcn_wmt_sdio_rw_cb_reg(stp_sdio_reg_rw);

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_RXDBG
	stp_sdio_rxdbg_setup();
#endif

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
	stp_sdio_txdbg_setup();
#endif

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_OWNBACKDBG
	stp_sdio_owndbg_setup();
#endif

	STPSDIO_PR_DBG
	    ("blk_size(%ld), tx_buf_cnt(%ld), fifo tx(%ld) rx(%ld), buf tx(%ld) rx(%ld)\n",
	     STP_SDIO_BLK_SIZE, STP_SDIO_TX_BUF_CNT, STP_SDIO_TX_FIFO_SIZE, STP_SDIO_RX_FIFO_SIZE,
	     STP_SDIO_TX_ENTRY_SIZE, STP_SDIO_TX_ENTRY_SIZE);

	return ret;
}

INT32 stp_sdio_reg_rw(INT32 func_num, INT32 direction,  UINT32 offset, UINT32 value)
{

	if (func_num == 0)
		return stp_sdio_func0_reg_rw(direction, offset, value);
	else if (func_num == 2)
		return stp_sdio_func_reg_rw(direction, offset, value);

	STPSDIO_PR_ERR("func_num(%d) is not support!\n", func_num);
	return -1;
}

INT32 stp_sdio_func0_reg_rw(INT32 direction,  UINT32 offset, UINT32 value)
{
	INT32 ret = -1;
	UINT8 val = 0x00;

	val = (UINT8) value;
	switch (direction) {
	case 0:
		ret = mtk_wcn_hif_sdio_f0_readb(g_stp_sdio_host_info.sdio_cltctx, offset, &val);
		STPSDIO_PR_INFO("read func0 CR(0x%x), value(0x%x)\n", offset, val);
		break;
	case 1:
		ret = mtk_wcn_hif_sdio_f0_writeb(g_stp_sdio_host_info.sdio_cltctx, offset, val);
		STPSDIO_PR_INFO("write func0 CR(0x%x), value(0x%x)\n", offset, val);
		break;
	default:
		break;
	}
	return ret;

}

INT32 stp_sdio_func_reg_rw(INT32 direction,  UINT32 offset, UINT32 value)
{
	INT32 ret = -1;
	UINT32 val = 0x00;

	val = (UINT32) value;
	switch (direction) {
	case 0:
		ret = mtk_wcn_hif_sdio_readl(g_stp_sdio_host_info.sdio_cltctx, offset, &val);
		STPSDIO_PR_INFO("read sdio CR(0x%x), value(0x%x)\n", offset, val);
		break;
	case 1:
		ret = mtk_wcn_hif_sdio_writel(g_stp_sdio_host_info.sdio_cltctx, offset, val);
		STPSDIO_PR_INFO("write sdio CR(0x%x), value(0x%x)\n", offset, val);
		break;
	default:
		break;
	}
	return ret;
}

INT32 stp_sdio_wake_up_ctrl(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	INT32 ret;

	ret = hif_sdio_wake_up_ctrl(ctx);
	if (ret == -11) {
		STPSDIO_PR_ERR("wake up fail, polling [GPIO_CHIP_DEEP_SLEEP_PIN] low over 30ms\n");
		ret = stp_sdio_issue_fake_coredump
			("<ASSERT> wake up fail, polling [GPIO_CHIP_DEEP_SLEEP_PIN] low over 30ms # -");
	} else if (ret == -2 || ret == -3)
		STPSDIO_PR_ERR("get wake up, sleep pin error\n");

	return ret;
}

VOID stp_sdio_dump_register(VOID)
{
	UINT32 count = 3;
	MTK_WCN_HIF_SDIO_CLTCTX clt_ctx;
	UINT32 val = 0;
	UINT32 delay_us = 10000;

	clt_ctx = gp_info->sdio_cltctx;
	while (count) {
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CCIR, &val, 0);
		STPSDIO_PR_ERR("******CCIR == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHLPCR, &val, 0);
		STPSDIO_PR_ERR("******CHLPCR == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CSDIOCSR, &val, 0);
		STPSDIO_PR_ERR("******CSDIOCSR == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHCR, &val, 0);
		STPSDIO_PR_ERR("******CHCR == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHISR, &val, 0);
		STPSDIO_PR_ERR("******CHISR == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CHIER, &val, 0);
		STPSDIO_PR_ERR("******CHIER == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CTFSR, &val, 0);
		STPSDIO_PR_ERR("******CTFSR == 0x%x*****\n", val);
		stp_sdio_rw_retry(HIF_TYPE_READL, STP_SDIO_RETRY_LIMIT, clt_ctx, CRPLR, &val, 0);
		STPSDIO_PR_ERR("******CRPLR == 0x%x*****\n", val);
		count--;
		osal_usleep_range(delay_us, 2 * delay_us);
	}

}

/*!
 * \brief hif_sdio init function
 *
 * detailed descriptions
 *
 * \retval
 */
static VOID stp_sdio_exit(VOID)
{
#if STP_SDIO_DBG_SUPPORT && STP_SDIO_TXDBG
	stp_sdio_txdbg_remove();
#endif

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_RXDBG
	stp_sdio_rxdbg_remove();
#endif

#if STP_SDIO_DBG_SUPPORT && STP_SDIO_OWNBACKDBG
	stp_sdio_owndbg_remove();
#endif

	/* 4 <0> unregister if_tx() function */
	mtk_wcn_stp_register_if_tx(STP_SDIO_IF_TX, 0x0);

	/* 4 <1> for all functions that have not been unregistered */
	/* 4 <1.1> unregister stp sdio func of the host */
	mtk_wcn_hif_sdio_client_unreg(&g_stp_sdio_cltinfo);

	/* 4 <1.2> stop Tx tasklet/Rx work queue of the host */
	flush_scheduled_work();
	STPSDIO_PR_DBG("flush scheduled work end\n");

	/* 4 <1.3> return ownership to firmware of the host */
	/* TODO: check set which register ! */

	/* 4 <1.4> clear the host struct list and free the memory allocation of the host */
	g_stp_sdio_host_count = 0;
	stp_sdio_host_info_op(0);
	/* 4 <1.5> Notice: while rmmod client driver, the stp_sdio_remove() */
	/* will not be called after stp_sdio_exit()  ! */
}

INT32 mtk_wcn_stp_sdio_drv_init(VOID)
{
	return stp_sdio_init();

}
EXPORT_SYMBOL(mtk_wcn_stp_sdio_drv_init);

VOID mtk_wcn_stp_sdio_drv_exit(VOID)
{
	return stp_sdio_exit();
}
EXPORT_SYMBOL(mtk_wcn_stp_sdio_drv_exit);
