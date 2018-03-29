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

/*
 * Id:
 * */

/*! \file   "stp_sdio.h"
    \brief


*/

/*
 * Log:
 * */

#ifndef _STP_SDIO_H
#define _STP_SDIO_H
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/


#define KMALLOC_UPDATE 1

#if 0				/* NO support for multiple STP-SDIO instances (multiple MT6620) on a single host */
#define STP_SDIO_HOST_COUNT (1)
#define STP_SDIO_ONLY_ONE_HOST (0)
#endif
#define STP_SDIO_POLL_OWNBACK_INTR (1)

#define STP_SDIO_NEW_TXRING (0)
/* George: Keep old (0) codes for debugging only!
 * Use new code (1) for SQC and MP!
 */

#define STP_SDIO_OWN_THREAD (1)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "osal.h"
#include "hif_sdio.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* Common HIF register address */
#define CCIR        (0x0000)
#define CHLPCR        (0x0004)
#define CSDIOCSR    (0x0008)
#define CHCR        (0x000c)
#define CHISR        (0x0010)
#define CHIER        (0x0014)
#define CTDR        (0x0018)
#define CRDR        (0x001c)
#define CTFSR        (0x0020)
#define CRPLR        (0x0024)
#define CSR        (0x00D8)	/* MT6630 & MT6632 only for the moment */
#define SWPCDBGR	(0x0154)

/* Common HIF register bit field address */
/* CHLPCR */
#define C_FW_OWN_REQ_CLR    (0x00000200)
#define C_FW_OWN_REQ_SET    (0x00000100)
#define C_FW_INT_EN_CLR     (0x00000002)
#define C_FW_INT_EN_SET     (0x00000001)
#define C_FW_COM_DRV_OWN    (0x00000100)

/* CHIER */
#define CHISR_EN_15_7       (0x0000ff80)
#define CHISR_EN_3_0        (0x0000000f)
/* CHISR */
#define RX_PKT_LEN          (0xffff0000)
#define FIRMWARE_INT        (0x0000fe00)
#define TX_FIFO_OVERFLOW    (0x00000100)
#define FW_INT_IND_INDICATOR (0x00000080)
#define TX_COMPLETE_COUNT   (0x00000070)
#define TX_UNDER_THOLD      (0x00000008)
#define TX_EMPTY            (0x00000004)
#define RX_DONE             (0x00000002)
#define FW_OWN_BACK_INT     (0x00000001)

/* hardware settings */
#define STP_SDIO_TX_FIFO_SIZE (2080UL)
#define STP_SDIO_RX_FIFO_SIZE (2304UL)	/* 256*9 */
#define STP_SDIO_TX_PKT_MAX_CNT (7)	/* Max outstanding tx pkt count, as defined in TX_COMPLETE_COUNT */
#define STP_SDIO_HDR_SIZE (4)	/* hw,fw,sw follow the same format: 2 bytes length + 2 bytes reserved */

/* sdio bus settings */
#define STP_SDIO_BLK_SIZE (512UL)

/* software driver settings */
#define STP_SDIO_TX_BUF_CNT (16UL)	/*(7) */
#define STP_SDIO_TX_BUF_CNT_MASK (STP_SDIO_TX_BUF_CNT - 1)
#define STP_SDIO_TX_PKT_LIST_SIZE (STP_SDIO_TX_BUF_CNT)	/* must be 2^x now... */
#define STP_SDIO_TX_PKT_LIST_SIZE_MASK (STP_SDIO_TX_PKT_LIST_SIZE - 1)

#define STP_SDIO_FW_CPUPCR_POLLING_CNT (5)

#define STP_SDIO_RETRY_LIMIT (5)
#define STP_SDIO_MAX_RETRY_NUM (100)

/* tx buffer size for a single entry */
/* George: SHALL BE a multiple of the used BLK_SIZE!! */
#if 1
/* round up: 512*5 = 2560 > 2080 */
#define STP_SDIO_TX_ENTRY_SIZE ((STP_SDIO_TX_FIFO_SIZE + (STP_SDIO_BLK_SIZE - 1)) & ~(STP_SDIO_BLK_SIZE - 1))
#else
/* round down: 512*4 = 2048 < 2080 */
#define STP_SDIO_TX_MAX_BLK_CNT (STP_SDIO_TX_FIFO_SIZE / STP_SDIO_BLK_SIZE)
#define STP_SDIO_TX_ENTRY_SIZE (STP_SDIO_TX_MAX_BLK_CNT * STP_SDIO_BLK_SIZE)
#endif

/*software rx buffer size */
/*#define STP_SDIO_RX_BUF_SIZE (STP_SDIO_RX_FIFO_SIZE)*/
/* George: SHALL BE a multiple of the used BLK_SIZE!! */
#if 1
/* round up: 512*5 = 2560 > 2304 */
#define STP_SDIO_RX_BUF_SIZE ((STP_SDIO_RX_FIFO_SIZE + (STP_SDIO_BLK_SIZE - 1)) & ~(STP_SDIO_BLK_SIZE - 1))
#else
/* round down: 512*4 = 2048 < 2304 */
#define STP_SDIO_RX_MAX_BLK_CNT (STP_SDIO_RX_FIFO_SIZE / STP_SDIO_BLK_SIZE)
#define STP_SDIO_RX_BUF_SIZE (STP_SDIO_RX_MAX_BLK_CNT * STP_SDIO_BLK_SIZE)
#endif

#define COHEC_00006052 (1)
/* #define COHEC_00006052 (0) */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_STP_SDIO_HIF_TYPE_T {
	HIF_TYPE_READB = 0,
	HIF_TYPE_READL = HIF_TYPE_READB + 1,
	HIF_TYPE_READ_BUF = HIF_TYPE_READL + 1,
	HIF_TYPE_WRITEB = HIF_TYPE_READ_BUF + 1,
	HIF_TYPE_WRITEL = HIF_TYPE_WRITEB + 1,
	HIF_TYPE_WRITE_BUF = HIF_TYPE_WRITEL + 1,
	HIF_TYPE_MAX
} ENUM_STP_SDIO_HIF_TYPE_T, *P_ENUM_STP_SDIO_HIF_TYPE_T;

/* HIF's local packet buffer variables for Tx/Rx */
typedef struct _MTK_WCN_STP_SDIO_PKT_BUF {
	/* Tx entry ring buffer. Entry size is aligned to SDIO block size. */
#if KMALLOC_UPDATE
	PUINT8 tx_buf;
#else
	UINT8 tx_buf[STP_SDIO_TX_BUF_CNT][STP_SDIO_TX_ENTRY_SIZE];
#endif

	/* Tx size ring buffer. Record valid data size in tx_buf. */
	UINT32 tx_buf_sz[STP_SDIO_TX_BUF_CNT];
	/* Tx debug timestamp: 1st time when the entry is filled with data */
	UINT32 tx_buf_ts[STP_SDIO_TX_BUF_CNT];
#if KMALLOC_UPDATE
	PUINT8 rx_buf;
#else
	UINT8 rx_buf[STP_SDIO_RX_BUF_SIZE];	/* Rx buffer (not ring) */
#endif
#if STP_SDIO_NEW_TXRING
	UINT32 wr_cnt;		/* Tx entry ring buffer write count */
	UINT32 rd_cnt;		/* Tx entry ring buffer read count */
	spinlock_t rd_cnt_lock;	/* Tx entry ring buffer read count spin lock */
#else
	UINT8 wr_idx;		/* Tx ring buffer write index *//*George: obsolete */
	UINT8 rd_idx;		/* Tx ring buffer read index *//*George: obsolete */
	spinlock_t rd_idx_lock;	/* spin lock for Tx ring buffer read index */
#endif
	MTK_WCN_BOOL full_flag;	/* Tx entry ring buffer full flag (TRUE: full, FALSE: not full) */
	/* save interrupt status flag for Tx entry ring buf spin lock */
	ULONG rd_irq_flag;
	/* wait queue head for Tx entry ring buf full case */
	wait_queue_head_t fullwait_q;
} MTK_WCN_STP_SDIO_PKT_BUF;

/* Tx packet list information */
typedef struct _MTK_WCN_STP_SDIO_Tx_Pkt_LIST {
	UINT32 pkt_rd_cnt;
	UINT32 pkt_wr_cnt;
	UINT16 pkt_size_list[STP_SDIO_TX_PKT_LIST_SIZE];	/*max length is FIFO Size */
	UINT32 out_ts[STP_SDIO_TX_PKT_LIST_SIZE];
	UINT32 in_ts[STP_SDIO_TX_PKT_LIST_SIZE];
} MTK_WCN_STP_SDIO_Tx_Pkt_LIST;

/* STP HIF firmware information */
typedef struct _MTK_WCN_STP_SDIO_FIRMWARE_INFO {
	UINT32 tx_fifo_size;	/* Current left tx FIFO size */
	UINT32 tx_packet_num;	/* Current outstanding tx packet (0~7) */
	atomic_t tx_comp_num;	/* Current total tx ok but fifo size not released packet count */
} MTK_WCN_STP_SDIO_FIRMWARE_INFO;

/* STP SDIO private information */
typedef struct _MTK_WCN_STP_SDIO_PRIVATE_INFO {
	UINT8 stp_sdio_host_idx;
} MTK_WCN_STP_SDIO_PRIVATE_INFO;

/* STP SDIO host information */
typedef struct _MTK_WCN_STP_SDIO_HIF_INFO {
	MTK_WCN_HIF_SDIO_CLTCTX sdio_cltctx;
	MTK_WCN_STP_SDIO_PKT_BUF pkt_buf;
	MTK_WCN_STP_SDIO_Tx_Pkt_LIST tx_pkt_list;
	UINT32 rx_pkt_len;	/* George: use 32-bit for efficiency. Correct name to pkt for packet */
	MTK_WCN_STP_SDIO_FIRMWARE_INFO firmware_info;
	MTK_WCN_STP_SDIO_PRIVATE_INFO private_info;
#if STP_SDIO_OWN_THREAD
	/* struct tasklet_struct tx_rx_job; */
	OSAL_THREAD tx_rx_thread;
	INT32 irq_pending;
	INT32 sleep_flag;
	INT32 wakeup_flag;
	INT32 awake_flag;
	OSAL_EVENT tx_rx_event;
	OSAL_SIGNAL isr_check_complete;
	INT32 dump_flag;
#endif
	INT32 tx_dbg_dump_flag;
	struct work_struct tx_work;
	struct work_struct rx_work;
} MTK_WCN_STP_SDIO_HIF_INFO;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
extern MTK_WCN_STP_SDIO_HIF_INFO g_stp_sdio_host_info;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
/* STP_SDIO_TX_PKT_LIST_SIZE must be 2^x */
#define STP_SDIO_GET_PKT_AR_IDX(idx) ((idx) & STP_SDIO_TX_PKT_LIST_SIZE_MASK)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*!
 * \brief MTK hif sdio client registration function
 *
 * Client uses this function to do hif sdio registration
 *
 * \param pinfo     a pointer of client's information
 *
 * \retval 0    register successfully
 * \retval < 0  error code
 */
extern INT32 mtk_wcn_hif_sdio_client_reg(const MTK_WCN_HIF_SDIO_CLTINFO *pinfo);

extern INT32 mtk_wcn_stp_sdio_do_own_clr(VOID);

/* extern INT32 */
/* mtk_wcn_stp_sdio_do_own_set (void); */

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 stp_sdio_rw_retry(ENUM_STP_SDIO_HIF_TYPE_T type, UINT32 retry_limit,
		MTK_WCN_HIF_SDIO_CLTCTX clt_ctx, UINT32 offset, PUINT32 pData, UINT32 len);
#endif				/* _STP_SDIO_H */
