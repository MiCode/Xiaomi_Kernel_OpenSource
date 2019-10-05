/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DPMAIF_REG_H__
#define __DPMAIF_REG_H__

#include <mt-plat/sync_write.h>

/* #define DPMAIF_NOT_ACCESS_HW  */
#ifndef DPMAIF_NOT_ACCESS_HW
extern struct hif_dpmaif_ctrl *dpmaif_ctrl;
#endif

/* INFRA */
#define INFRA_RST0_REG_PD (0x0150)/* reset dpmaif reg */
#define INFRA_RST1_REG_PD (0x0154)/* clear dpmaif reset reg */
#define DPMAIF_PD_RST_MASK (1 << 2)
#define INFRA_RST0_REG_AO (0x0140)
#define INFRA_RST1_REG_AO (0x0144)
#define DPMAIF_AO_RST_MASK (1 << 6)
#define INFRA_DPMAIF_CTRL_REG  (0xC00)
#define DPMAIF_IP_BUSY_MASK   (0x3 << 12)
/***********************************************************************
 *  DPMAIF AO/PD register define macro
 *
 ***********************************************************************/
 #ifdef DVT_DEFINITION
#define DPMAIF_PD_BASE                     0x1022D000
#define DPMAIF_AO_BASE                     0x10014000
#define DPMAIF_PD_MD_MISC_BASE       0x1022C000

/*DPMAIF PD domain group*/
#define DPMAIF_UL_CFG_OFFSET               0
#define DPMAIF_DL_CFG_OFFSET               0x100
#define DPMAIF_AP_MISC_OFFSET              0x400

#define DPMAIF_PD_UL_CFG_BASE              (DPMAIF_PD_BASE + 0)
#define DPMAIF_PD_DL_CFG_BASE              (DPMAIF_PD_BASE + 0x100)
#define DPMAIF_PD_AP_MISC_BASE             (DPMAIF_PD_BASE + 0x400)

/*DPMAIF AO domain group*/
#define DPMAIF_AO_UL_OFFSET                0
#define DPMAIF_AO_DL_OFFSET                0x400

#define DPMAIF_AO_UL_CFG_BASE              (DPMAIF_AO_BASE + 0)
#define DPMAIF_AO_DL_CFG_BASE              (DPMAIF_AO_BASE + 0x400)
#else
#define DPMAIF_PD_UL_CFG_BASE              0
#define DPMAIF_PD_DL_CFG_BASE              0
#define DPMAIF_PD_AP_MISC_BASE             0

#define DPMAIF_AO_UL_CFG_BASE              0
#define DPMAIF_AO_DL_CFG_BASE              0
#endif

/*DPMAIF AO UL CONFIG: 0x10014000 / ao + 0*/
#define DPMAIF_AO_UL_CHNL0_STA            (DPMAIF_AO_UL_CFG_BASE + 0x0000)
#define DPMAIF_AO_UL_CHNL1_STA            (DPMAIF_AO_UL_CFG_BASE + 0x0004)
#define DPMAIF_AO_UL_CHNL2_STA            (DPMAIF_AO_UL_CFG_BASE + 0x0008)
#define DPMAIF_AO_UL_CHNL3_STA            (DPMAIF_AO_UL_CFG_BASE + 0x000C)


/*DPMAIF AO DL CONFIG: 0x10014400 / ao + 0x400*/
#define DPMAIF_AO_DL_PKTINFO_CONO         (DPMAIF_AO_DL_CFG_BASE + 0x0000)
#define DPMAIF_AO_DL_PKTINFO_CON1         (DPMAIF_AO_DL_CFG_BASE + 0x0004)
#define DPMAIF_AO_DL_PKTINFO_CON2         (DPMAIF_AO_DL_CFG_BASE + 0x0008)

#define DPMAIF_AO_DL_RDY_CHK_THRES        (DPMAIF_AO_DL_CFG_BASE + 0x000C)

#define DPMAIF_AO_DL_BAT_STA0             (DPMAIF_AO_DL_CFG_BASE + 0x0010)
#define DPMAIF_AO_DL_BAT_STA1             (DPMAIF_AO_DL_CFG_BASE + 0x0014)
#define DPMAIF_AO_DL_BAT_STA2             (DPMAIF_AO_DL_CFG_BASE + 0x0018)

#define DPMAIF_AO_DL_PIT_STA0             (DPMAIF_AO_DL_CFG_BASE + 0x0020)
#define DPMAIF_AO_DL_PIT_STA1             (DPMAIF_AO_DL_CFG_BASE + 0x0024)
#define DPMAIF_AO_DL_PIT_STA2             (DPMAIF_AO_DL_CFG_BASE + 0x0028)
#define DPMAIF_AO_DL_PIT_STA3             (DPMAIF_AO_DL_CFG_BASE + 0x002C)

/*DPMAIF PD UL CONFIG: 0x1022D000 /pd+0*/
#define DPMAIF_PD_UL_ADD_DESC             (DPMAIF_PD_UL_CFG_BASE + 0x0000)
#define DPMAIF_PD_UL_RESTORE_RIDX         (DPMAIF_PD_UL_CFG_BASE + 0x0004)

#define DPMAIF_PD_UL_MD_RDY_CNT_TH        (DPMAIF_PD_UL_CFG_BASE + 0x0008)
#define DPMAIF_PD_UL_CHNL_ARB0            (DPMAIF_PD_UL_CFG_BASE + 0x0010)
#define DPMAIF_PD_UL_CHNL_ARB1            (DPMAIF_PD_UL_CFG_BASE + 0x0014)

#define DPMAIF_PD_UL_CHNL0_CON0           (DPMAIF_PD_UL_CFG_BASE + 0x0020)
#define DPMAIF_PD_UL_CHNL0_CON1           (DPMAIF_PD_UL_CFG_BASE + 0x0024)
#define DPMAIF_PD_UL_CHNL0_STA0           (DPMAIF_PD_UL_CFG_BASE + 0x0060)

#define DPMAIF_PD_UL_CHNL1_CON0           (DPMAIF_PD_UL_CFG_BASE + 0x0030)
#define DPMAIF_PD_UL_CHNL1_CON1           (DPMAIF_PD_UL_CFG_BASE + 0x0034)
#define DPMAIF_PD_UL_CHNL1_STA0           (DPMAIF_PD_UL_CFG_BASE + 0x0064)

#define DPMAIF_PD_UL_CHNL2_CON0           (DPMAIF_PD_UL_CFG_BASE + 0x0040)
#define DPMAIF_PD_UL_CHNL2_CON1           (DPMAIF_PD_UL_CFG_BASE + 0x0044)
#define DPMAIF_PD_UL_CHNL2_STA0           (DPMAIF_PD_UL_CFG_BASE + 0x0068)

#define DPMAIF_PD_UL_CHNL3_CON0           (DPMAIF_PD_UL_CFG_BASE + 0x0050)
#define DPMAIF_PD_UL_CHNL3_CON1           (DPMAIF_PD_UL_CFG_BASE + 0x0054)
#define DPMAIF_PD_UL_CHNL3_STA0           (DPMAIF_PD_UL_CFG_BASE + 0x006C)

#define DPMAIF_PD_UL_CACHE_CON0           (DPMAIF_PD_UL_CFG_BASE + 0x0070)

#define DPMAIF_PD_UL_ADD_DESC_CH          (DPMAIF_PD_UL_CFG_BASE + 0x00B0)

#define DPMAIF_PD_UL_DBG_STA0             (DPMAIF_PD_UL_CFG_BASE + 0x0080)
#define DPMAIF_PD_UL_DBG_STA1             (DPMAIF_PD_UL_CFG_BASE + 0x0084)
#define DPMAIF_PD_UL_DBG_STA2             (DPMAIF_PD_UL_CFG_BASE + 0x0088)
#define DPMAIF_PD_UL_DBG_STA3             (DPMAIF_PD_UL_CFG_BASE + 0x008C)
#define DPMAIF_PD_UL_DBG_STA4             (DPMAIF_PD_UL_CFG_BASE + 0x0090)
#define DPMAIF_PD_UL_DBG_STA5             (DPMAIF_PD_UL_CFG_BASE + 0x0094)
#define DPMAIF_PD_UL_DBG_STA6             (DPMAIF_PD_UL_CFG_BASE + 0x0098)
#define DPMAIF_PD_UL_DBG_STA7             (DPMAIF_PD_UL_CFG_BASE + 0x009C)
#define DPMAIF_PD_UL_DBG_STA8             (DPMAIF_PD_UL_CFG_BASE + 0x00A0)
#define DPMAIF_PD_UL_DBG_STA9             (DPMAIF_PD_UL_CFG_BASE + 0x00A4)

/*DPMAIF PD DL CONFIG: 0x1022D100 /pd+0x100*/
#define DPMAIF_PD_DL_BAT_INIT             (DPMAIF_PD_DL_CFG_BASE + 0x0000)
#define DPMAIF_PD_DL_BAT_ADD              (DPMAIF_PD_DL_CFG_BASE + 0x0004)

#define DPMAIF_PD_DL_BAT_INIT_CON0        (DPMAIF_PD_DL_CFG_BASE + 0x0008)
#define DPMAIF_PD_DL_BAT_INIT_CON1        (DPMAIF_PD_DL_CFG_BASE + 0x000C)
#define DPMAIF_PD_DL_BAT_INIT_CON2        (DPMAIF_PD_DL_CFG_BASE + 0x0010)

#define DPMAIF_PD_DL_PIT_INIT             (DPMAIF_PD_DL_CFG_BASE + 0x0020)
#define DPMAIF_PD_DL_PIT_ADD              (DPMAIF_PD_DL_CFG_BASE + 0x0024)

#define DPMAIF_PD_DL_PIT_INIT_CON0        (DPMAIF_PD_DL_CFG_BASE + 0x0028)
#define DPMAIF_PD_DL_PIT_INIT_CON1        (DPMAIF_PD_DL_CFG_BASE + 0x002C)
#define DPMAIF_PD_DL_PIT_INIT_CON2        (DPMAIF_PD_DL_CFG_BASE + 0x0030)
#define DPMAIF_PD_DL_PIT_INIT_CON3        (DPMAIF_PD_DL_CFG_BASE + 0x0034)

#define DPMAIF_PD_DL_MISC_CON0            (DPMAIF_PD_DL_CFG_BASE + 0x0040)

#define DPMAIF_PD_DL_STA0                 (DPMAIF_PD_DL_CFG_BASE + 0x0080)
#define DPMAIF_PD_DL_STA8                 (DPMAIF_PD_DL_CFG_BASE + 0x00A0)

#define DPMAIF_PD_DL_DBG_STA0             (DPMAIF_PD_DL_CFG_BASE + 0x00B0)
#define DPMAIF_PD_DL_DBG_STA1             (DPMAIF_PD_DL_CFG_BASE + 0x00B4)
#define DPMAIF_PD_DL_DBG_STA7             (DPMAIF_PD_DL_CFG_BASE + 0x00CC)

#define DPMAIF_PD_DL_DBG_STA14             (DPMAIF_PD_DL_CFG_BASE + 0x00FC)

#define DPMAIF_PD_DL_EMI_ENH1             (DPMAIF_PD_DL_CFG_BASE + 0x0210)
#define DPMAIF_PD_DL_EMI_ENH2             (DPMAIF_PD_DL_CFG_BASE + 0x0214)

/*DPMAIF PD AP MSIC CONFIG: 0x1022D400 /pd+0x400*/
#define DPMAIF_PD_AP_UL_L2TISAR0          (DPMAIF_PD_AP_MISC_BASE + 0x0000)
#define DPMAIF_PD_AP_UL_L2TIMR0           (DPMAIF_PD_AP_MISC_BASE + 0x0004)
#define DPMAIF_PD_AP_UL_L2TICR0           (DPMAIF_PD_AP_MISC_BASE + 0x0008)
#define DPMAIF_PD_AP_UL_L2TISR0           (DPMAIF_PD_AP_MISC_BASE + 0x000C)

#define DPMAIF_PD_AP_L1TISAR0             (DPMAIF_PD_AP_MISC_BASE + 0x0010)
#define DPMAIF_PD_AP_L1TIMR0              (DPMAIF_PD_AP_MISC_BASE + 0x0014)

#define DPMAIF_PD_BUS_CONFIG0             (DPMAIF_PD_AP_MISC_BASE + 0x0020)
#define DPMAIF_PD_TOP_AP_CFG              (DPMAIF_PD_AP_MISC_BASE + 0x0024)
#define DPMAIF_PD_BUS_STATUS0             (DPMAIF_PD_AP_MISC_BASE + 0x0030)
#define DPMAIF_PD_AP_DMA_ERR_STA          (DPMAIF_PD_AP_MISC_BASE + 0x0040)

#define DPMAIF_PD_AP_DL_L2TISAR0          (DPMAIF_PD_AP_MISC_BASE + 0x0050)
#define DPMAIF_PD_AP_DL_L2TIMR0           (DPMAIF_PD_AP_MISC_BASE + 0x0054)
#define DPMAIF_PD_AP_DL_L2TIMCR0           (DPMAIF_PD_AP_MISC_BASE + 0x0058)
#define DPMAIF_PD_AP_DL_L2TIMSR0           (DPMAIF_PD_AP_MISC_BASE + 0x005C)

#define DPMAIF_PD_AP_IP_BUSY              (DPMAIF_PD_AP_MISC_BASE + 0x0060)
#define DPMAIF_PD_AP_DLUL_IP_BUSY_MASK    (DPMAIF_PD_AP_MISC_BASE + 0x0064)

#define DPMAIF_PD_AP_CG_EN               (DPMAIF_PD_AP_MISC_BASE + 0x0068)
#define DPMAIF_PD_AP_CODA_VER            (DPMAIF_PD_AP_MISC_BASE + 0x006C)

/*DPMAIF PD MD MISC CONFIG: 0x1022C000 */
#define DPMAIF_PD_MD_IP_BUSY              (DPMAIF_PD_MD_MISC_BASE + 0x0000)
#define DPMAIF_PD_MD_IP_BUSY_WAKE         (DPMAIF_PD_MD_MISC_BASE + 0x0004)


#define DPMAIF_PD_MD_L1TISAR0             (DPMAIF_PD_MD_MISC_BASE + 0x0010)
#define DPMAIF_PD_MD_L1TIMR0              (DPMAIF_PD_MD_MISC_BASE + 0x0014)
#define DPMAIF_PD_MD_L1TICR0              (DPMAIF_PD_MD_MISC_BASE + 0x0018)
#define DPMAIF_PD_MD_L1TISR0              (DPMAIF_PD_MD_MISC_BASE + 0x001C)


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
#define dpmaif_write32(b, a, v)	mt_reg_sync_writel(v, (b)+(a))
#define dpmaif_write16(b, a, v)	mt_reg_sync_writew(v, (b)+(a))
#define dpmaif_write8(b, a, v)		mt_reg_sync_writeb(v, (b)+(a))

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
	dpmaif_write32(dpmaif_ctrl->dpmaif_ao_dl_base, (a), v)

#define DPMA_READ_AO_DL(a) \
	dpmaif_read32(dpmaif_ctrl->dpmaif_ao_dl_base, (a))
#endif

/* DL */
/* DPMAIF_PD_DL_BAT/PIT_ADD */
#define DPMAIF_DL_ADD_UPDATE                (1 << 31)
#define DPMAIF_DL_ADD_NOT_READY             (1 << 31)

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

#define DPMAIF_PIT_EN_MSK              0x01
#define DPMAIF_PIT_SIZE_MSK            0xFFFF
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

#define DPMAIF_DL_PIT_WRIDX_MSK        0xFFFF
#define DPMAIF_DL_BAT_WRIDX_MSK        0xFFFF

#define DPMAIF_BAT_CHECK_THRES_MSK     (0x3F << 16)
#define DPMAIF_FRG_CHECK_THRES_MSK     (0xFF)
#define DPMAIF_AO_DL_ISR_MSK           (0x7F)

/*DPMAIF_PD_DL_DBG_STA1*/
#define DPMAIF_DL_IDLE_STS             (1 << 25)

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

/* ======== UL ========= */

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
#define DPMAIF_ULQ_STA0_n(q_num)            \
	((DPMAIF_PD_UL_CHNL0_STA0) + (0x04 * (q_num)))

/*DPMAIF_PD_UL_DBG_STA2*/
#define DPMAIF_UL_STS_CUR_SHIFT        (26)
#define DPMAIF_UL_IDLE_STS_MSK         (0x3F)
#define DPMAIF_UL_IDLE_STS             (0x01)

/* === tx interrupt mask === */
#define UL_INT_DONE_OFFSET          0
#define UL_INT_EMPTY_OFFSET         4
#define UL_INT_MD_NOTRDY_OFFSET     8
#define UL_INT_PWR_NOTRDY_OFFSET    12
#define UL_INT_LEN_ERR_OFFSET       16

#define DPMAIF_UL_INT_DONE(q_num)            (1 << (q_num+UL_INT_DONE_OFFSET))
#define DPMAIF_UL_INT_EMPTY(q_num)          (1 << (q_num+UL_INT_EMPTY_OFFSET))
#define DPMAIF_UL_INT_MD_NOTRDY(q_num)           \
	(1 << (q_num+UL_INT_MD_NOTRDY_OFFSET))
#define DPMAIF_UL_INT_PWR_NOTRDY(q_num)         \
	(1 << (q_num+UL_INT_PWR_NOTRDY_OFFSET))
#define DPMAIF_UL_INT_LEN_ERR(q_num)                 \
	(1 << (q_num+UL_INT_LEN_ERR_OFFSET))

#define DPMAIF_UL_INT_QDONE_MSK	(0x0F << UL_INT_DONE_OFFSET)
#define DPMAIF_UL_INT_EMPTY_MSK	(0x0F << UL_INT_EMPTY_OFFSET)
#define DPMAIF_UL_INT_MD_NOTREADY_MSK	(0x0F << UL_INT_MD_NOTRDY_OFFSET)
#define DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK	\
	(0x0F << UL_INT_PWR_NOTRDY_OFFSET)
#define DPMAIF_UL_INT_ERR_MSK		(0x0F << UL_INT_LEN_ERR_OFFSET)

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
#ifdef _E1_SB_SW_WORKAROUND_
#define AP_DL_L2INTR_ERR_En_Msk \
	(DPMAIF_DL_INT_SKB_LEN_ERR(0) | DPMAIF_DL_INT_PITCNT_LEN_ERR(0) | \
	DPMAIF_DL_INT_MTU_ERR_MSK | DPMAIF_DL_INT_BATCNT_LEN_ERR(0))
/* DPMAIF_DL_INT_EMPTY_MSK | */
#define AP_DL_L2INTR_En_Msk \
	(AP_DL_L2INTR_ERR_En_Msk | \
	DPMAIF_DL_INT_QDONE_MSK)
#else
#define AP_DL_L2INTR_ERR_En_Msk \
	(DPMAIF_DL_INT_SKB_LEN_ERR(0) | \
	DPMAIF_DL_INT_MTU_ERR_MSK)
/* DPMAIF_DL_INT_EMPTY_MSK | */
#define AP_DL_L2INTR_En_Msk \
	(AP_DL_L2INTR_ERR_En_Msk | \
	DPMAIF_DL_INT_QDONE_MSK)
#endif
#endif				/* __DPMAIF_REG_H__ */
