/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_DPMAIF_DRV_ARCH_V3_H__
#define __MODEM_DPMAIF_DRV_ARCH_V3_H__

#include <linux/io.h>
#include "ccci_config.h"

/* #define DPMAIF_NOT_ACCESS_HW  */
#ifndef DPMAIF_NOT_ACCESS_HW
extern struct hif_dpmaif_ctrl *dpmaif_ctrl_v3;
#define dpmaif_ctrl dpmaif_ctrl_v3
#endif

/***********************************************************************
 *  DPMAIF common marco
 *
 ***********************************************************************/
/*assistant macros*/
#define CLDMA_AP_TQSAR(i)  (CLDMA_AP_UL_START_ADDR_0   + (4 * (i)))
#define CLDMA_AP_TQCPR(i)  (CLDMA_AP_UL_CURRENT_ADDR_0 + (4 * (i)))
#define CLDMA_AP_RQSAR(i)  (CLDMA_AP_SO_START_ADDR_0   + (4 * (i)))
#define CLDMA_AP_RQCPR(i)  (CLDMA_AP_SO_CURRENT_ADDR_0 + (4 * (i)))
#define CLDMA_AP_TQSABAK(i)  (CLDMA_AP_UL_START_ADDR_BK_0 + (4 * (i)))
#define CLDMA_AP_TQCPBAK(i)  (CLDMA_AP_UL_CURRENT_ADDR_BK_0 + (4 * (i)))

#ifdef DPMAIF_NOT_ACCESS_HW
#define dpmaif_write32(b, a, v)
#define dpmaif_write16(b, a, v)
#define dpmaif_write8(b, a, v)

#define dpmaif_read32(b, a)			0
#define dpmaif_read16(b, a)			0
#define dpmaif_read8(b, a)			0

#define DPMA_READ_PD_MISC(a)	0
#define DPMA_READ_PD_UL(a)		0
#define DPMA_READ_PD_DL(a)		0

#define DPMA_READ_AO_DL(a)		0

#define DPMA_WRITE_PD_MISC(a, v)
#define DPMA_WRITE_PD_UL(a, v)
#define DPMA_WRITE_PD_DL(a, v)
#define DPMA_WRITE_AO_DL(a, v)

#else
#define dpmaif_write32(b, a, v)	\
do { \
	writel(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define dpmaif_write16(b, a, v)	\
do { \
	writew(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define dpmaif_write8(b, a, v) \
do { \
	writeb(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define dpmaif_read32(b, a)		ioread32((void __iomem *)((b)+(a)))
#define dpmaif_read16(b, a)		ioread16((void __iomem *)((b)+(a)))
#define dpmaif_read8(b, a)		ioread8((void __iomem *)((b)+(a)))

#define DPMA_READ_PD_MISC(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_pd_misc_base, (a))
#define DPMA_READ_PD_UL(a) dpmaif_read32(dpmaif_ctrl->dpmaif_pd_ul_base, (a))
#define DPMA_READ_PD_DL(a) dpmaif_read32(dpmaif_ctrl->dpmaif_pd_dl_base, (a))

#define DPMA_WRITE_PD_MISC(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_pd_misc_base, (a), v)
#define DPMA_WRITE_PD_UL(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_pd_ul_base, (a), v)
#define DPMA_WRITE_PD_DL(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_pd_dl_base, (a), v)

#define DPMA_WRITE_AO_DL(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_dl_sram_base, (a), v)  //xuxin-change

#define DPMA_READ_AO_DL(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_dl_sram_base, (a))  //xuxin-change

#define DPMA_WRITE_AO_DL_NOSRAM(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_dl_base, (a), v)  //xuxin-add, has use

#define DPMA_READ_AO_DL_NOSRAM(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_dl_base, (a))  //xuxin-add, no use

#define DPMA_WRITE_AO_UL_SRAM(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_ul_sram_base, (a), v)  //xuxin-add, has use

#define DPMA_READ_AO_UL_SRAM(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_ul_sram_base, (a))  //xuxin-add, has use

#define DPMA_WRITE_AO_MISC_SRAM(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_msic_sram_base, (a), v)  //xuxin-add, has use

#define DPMA_READ_AO_MISC_SRAM(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_msic_sram_base, (a))  //xuxin-add, has use

#define DPMA_WRITE_AO_UL(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_ul_base, (a), v)

#define DPMA_READ_AO_UL(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_ul_base, (a))

#define DPMA_WRITE_WDMA(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_pd_wdma_base, (a), v)

#define DPMA_READ_WDMA(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_pd_wdma_base, (a))


#define DPMA_WRITE_AO_MD_DL(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_md_dl_base, (a), v)

#define DPMA_READ_AO_MD_DL(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_md_dl_base, (a))

#define DPMA_WRITE_MD_MISC_DL(a, v) \
	dpmaif_write32(dpmaif_ctrl->dpmaif_pd_md_misc_base, (a), v)

#define DPMA_READ_MD_MISC_DL(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_pd_md_misc_base, (a))

#endif

/***********************************************************************
 *  DPMAIF DL common marco
 *
 ***********************************************************************/

/* DPMAIF_PD_DL_BAT/PIT_ADD */
#define DPMAIF_DL_ADD_UPDATE                (1 << 31)
#define DPMAIF_DL_ADD_NOT_READY             (1 << 31)
#define DPMAIF_DL_BAT_FRG_ADD               (1 << 16)

#define DPMAIF_DL_BAT_INIT_ALLSET           (1 << 0)
#define DPMAIF_DL_BAT_FRG_INIT              (1 << 16)
#define DPMAIF_DL_BAT_INIT_EN               (1 << 31)
#define DPMAIF_DL_BAT_INIT_NOT_READY        (1 << 31)
#define DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT  (0 << 0)

#define DPMAIF_DL_PIT_INIT_ALLSET           (1 << 0)
#define DPMAIF_DL_PIT_INIT_ONLY_ENABLE_BIT  (0 << 0)
#define DPMAIF_DL_PIT_INIT_EN               (1 << 31)
#define DPMAIF_DL_PIT_INIT_NOT_READY        (1 << 31)

#define DPMAIF_PKT_ALIGN64_MODE        0
#define DPMAIF_PKT_ALIGN128_MODE       1

#define DPMAIF_BAT_REMAIN_SZ_BASE      16
#define DPMAIF_BAT_BUFFER_SZ_BASE      128
#define DPMAIF_FRG_BAT_BUFFER_SZ_BASE  128

#define DPMAIF_PIT_EN_MSK              0x01

#define DPMAIF_PIT_SIZE_MSK            0x3FFFF  //xuxin-change

#define DPMAIF_PIT_ADDRH_MSK           0xFF000000

#define DPMAIF_BAT_EN_MSK              (1 << 16)
#define DPMAIF_BAT_SIZE_MSK            0xFFFF
#define DPMAIF_BAT_ADDRH_MSK           0xFF000000

#define DPMAIF_BAT_BID_MAXCNT_MSK      0xFFFF0000
#define DPMAIF_BAT_REMAIN_MINSZ_MSK    0x0000FF00
#define DPMAIF_PIT_CHK_NUM_MSK         0xFF000000
#define DPMAIF_BAT_BUF_SZ_MSK          0x0001FF00
#define DPMAIF_BAT_RSV_LEN_MSK         0x000000FF
#define DPMAIF_PKT_ALIGN_MSK           (0x3 << 22)

#define DPMAIF_BAT_CHECK_THRES_MSK     (0x3F << 16)

#define DPMAIF_PKT_ALIGN_EN            (1 << 23)

#define DPMAIF_DL_PIT_WRIDX_MSK        0x3FFFF  //xuxin-change
#define DPMAIF_DL_PIT_REMAIN_CNT_MSK   0x3FFFF

#define DPMAIF_DL_BAT_WRIDX_MSK        0xFFFF

#define DPMAIF_BAT_CHECK_THRES_MSK     (0x3F << 16)
#define DPMAIF_FRG_CHECK_THRES_MSK     (0xFF)
#define DPMAIF_AO_DL_ISR_MSK           (0x7F)

#define DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK   (1 << 28)
#define DPMAIF_FRG_BAT_BUF_FEATURE_EN       (1 << 28)
#define DPMAIF_FRG_BAT_BUF_SZ_MSK           (0xff << 8)
#define DPMAIF_CHKSUM_ON_MSK                (1 << 31)

#define DPMAIF_DL_IDLE_STS             (1 << 23)  //xuxin-change

/*DPMAIF_PD_DL_DBG_STA7*/
#define DPMAIF_DL_FIFO_PUSH_RIDX       (0x3F << 20)
#define DPMAIF_DL_FIFO_PUSH_SHIFT       20
#define DPMAIF_DL_FIFO_PUSH_MSK         0x3F

#define DPMAIF_DL_FIFO_PUSH_IDLE_STS   (1 << 16)

#define DPMAIF_DL_FIFO_POP_RIDX        (0x3F << 5)
#define DPMAIF_DL_FIFO_POP_SHIFT        5
#define DPMAIF_DL_FIFO_POP_MSK          0x3F

#define DPMAIF_DL_FIFO_POP_IDLE_STS    (1 << 0)

#define DPMAIF_DL_FIFO_IDLE_STS \
	(DPMAIF_DL_FIFO_POP_IDLE_STS|DPMAIF_DL_FIFO_PUSH_IDLE_STS)


#define DPMAIF_CHK_RB_PITNUM_MSK 0x000000FF
/*BASE_NADDR_NRL2_DPMAIF_WDMA*/
#define DPMAIF_DL_WDMA_CTRL_OSTD_OFST (28)
#define DPMAIF_DL_WDMA_CTRL_OSTD_MSK (0xF)
#define DPMAIF_DL_WDMA_CTRL_OSTD_VALUE (0xE)

#define DPMAIF_AWDOMAIN_BIT_MSK 0xF
#define DPMAIF_ARDOMAIN_BIT_MSK 0xF
#define DPMAIF_AWDOMAIN_BIT_OFT 0
#define DPMAIF_ARDOMAIN_BIT_OFT 8

#define DPMAIF_CACHE_BANK0_BIT_MSK 0x3F
#define DPMAIF_CACHE_BANK1_BIT_MSK 0x3F
#define DPMAIF_CACHE_BANK0_BIT_OFT 0
#define DPMAIF_CACHE_BANK1_BIT_OFT 8

#define DP_DOMAIN_ID 1
#define DP_BANK0_ID 6
#define DP_BANK1_ID 7

#define DPMAIF_MD_AO_REMAP_ENABLE (1 << 0)


#define DPMAIF_MEM_CLR_MASK             (1 << 0)  //xuxin-add, has use
#define DPMAIF_SRAM_SYNC_MASK           (1 << 0)  //xuxin-add, has use
#define DPMAIF_UL_INIT_DONE_MASK        (1 << 0)  //xuxin-add, has use
#define DPMAIF_DL_INIT_DONE_MASK        (1 << 0)  //xuxin-add, has use

/***********************************************************************
 *  DPMAIF UL common marco
 *
 ***********************************************************************/

/* DPMAIF_PD_UL_CHNL(*)_CON1 */
#define DPMAIF_DRB_ADDRH_MSK           0xFF000000
#define DPMAIF_DRB_SIZE_MSK            0x0000FFFF

#define DPMAIF_UL_ADD_NOT_READY             (1 << 31)

#define DPMAIF_UL_ALL_QUE_ARB_EN            (0xF << 8)

#define DPMAIF_UL_ADD_UPDATE                (1 << 31)
#define DPMAIF_ULQ_ADD_DESC_CH_n(q_num)     \
	((DPMAIF_PD_UL_ADD_DESC_CH) + (0x04 * (q_num)))

#define DPMAIF_ULQSAR_n(q_num)      \
	((DPMAIF_PD_UL_CHNL0_CON0) + (0x10 * (q_num)))
#define DPMAIF_UL_DRBSIZE_ADDRH_n(q_num)    \
	((DPMAIF_PD_UL_CHNL0_CON1) + (0x10 * (q_num)))

#define DPMAIF_ULQ_STA0_n(q_num)     \
	((NRL2_DPMAIF_AO_UL_CH0_STA) + (0x04 * (q_num)))

#define DPMAIF_UL_STS_CUR_SHIFT        (11)  //xuxin-change
#define DPMAIF_UL_IDLE_STS_MSK         (0x01)  //xuxin-change

#define DPMAIF_UL_IDLE_STS             (0x01)
/***********************************************************************
 *  DPMAIF interrupt common marco
 *
 ***********************************************************************/

/* === tx interrupt mask === */
#define UL_INT_DONE_OFFSET          0
#define UL_INT_EMPTY_OFFSET         5
#define UL_INT_MD_NOTRDY_OFFSET     10
#define UL_INT_PWR_NOTRDY_OFFSET    15
#define UL_INT_LEN_ERR_OFFSET       20

#define DPMAIF_UL_INT_DONE(q_num)            (1 << (q_num+UL_INT_DONE_OFFSET))
#define DPMAIF_UL_INT_EMPTY(q_num)          (1 << (q_num+UL_INT_EMPTY_OFFSET))
#define DPMAIF_UL_INT_MD_NOTRDY(q_num)           \
	(1 << (q_num+UL_INT_MD_NOTRDY_OFFSET))
#define DPMAIF_UL_INT_PWR_NOTRDY(q_num)         \
	(1 << (q_num+UL_INT_PWR_NOTRDY_OFFSET))
#define DPMAIF_UL_INT_LEN_ERR(q_num)                 \
	(1 << (q_num+UL_INT_LEN_ERR_OFFSET))

#define DPMAIF_UL_INT_QDONE_MSK	(0x1F << UL_INT_DONE_OFFSET)
#define DPMAIF_UL_INT_EMPTY_MSK	(0x1F << UL_INT_EMPTY_OFFSET)
#define DPMAIF_UL_INT_MD_NOTREADY_MSK	(0x1F << UL_INT_MD_NOTRDY_OFFSET)
#define DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK	\
	(0x1F << UL_INT_PWR_NOTRDY_OFFSET)
#define DPMAIF_UL_INT_ERR_MSK		(0x1F << UL_INT_LEN_ERR_OFFSET)

#define AP_UL_L2INTR_ERR_En_Msk \
	(DPMAIF_UL_INT_ERR_MSK | DPMAIF_UL_INT_MD_NOTREADY_MSK | \
	DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK)
/* DPMAIF_UL_INT_EMPTY_MSK | */
#define AP_UL_L2INTR_En_Msk \
	(AP_UL_L2INTR_ERR_En_Msk | \
	DPMAIF_UL_INT_QDONE_MSK)

/* === RX interrupt mask === */
#define DPMAIF_DL_INT_ERR_MSK                    (0x07 << 1)
#define DPMAIF_DL_INT_EMPTY_MSK                  (0x03 << 4)
#define DPMAIF_DL_INT_MTU_ERR_MSK                (0x01 << 6)
#define DPMAIF_DL_INT_QDONE_MSK                  (0x01 << 0)
#define DPMAIF_DL_INT_SKB_LEN_ERR(q_num)              (1 << 1)
#define DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num)           (1 << 2)
#define DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num)           (1 << 3)

#define AP_DL_L2INTR_ERR_En_Msk \
	(DPMAIF_DL_INT_SKB_LEN_ERR(0) | \
	DPMAIF_DL_INT_MTU_ERR_MSK)
/* DPMAIF_DL_INT_EMPTY_MSK | */
#define AP_DL_L2INTR_En_Msk \
	(AP_DL_L2INTR_ERR_En_Msk | \
	DPMAIF_DL_INT_QDONE_MSK)


#endif /*__MODEM_DPMAIF_DRV_ARCH_H__*/
