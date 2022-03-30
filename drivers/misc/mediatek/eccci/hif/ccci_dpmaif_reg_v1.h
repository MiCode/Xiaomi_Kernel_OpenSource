/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_DPMAIF_REG_V1_H__
#define __CCCI_DPMAIF_REG_V1_H__

#include <linux/io.h>

#include "ccci_dpmaif_reg_com.h"


#define DPMAIF_PD_UL_CFG_BASE              0
#define DPMAIF_PD_DL_CFG_BASE              0
#define DPMAIF_PD_AP_MISC_BASE             0

#define DPMAIF_AO_UL_CFG_BASE              0
#define DPMAIF_AO_DL_CFG_BASE              0


/*Default DPMAIF DL common setting*/
#define DPMAIF_HW_CHK_PIT_NUM      6
#define DPMAIF_HW_CHK_BAT_NUM      3
#define DPMAIF_HW_CHK_FRG_NUM      3


#define DPMAIF_DL_BAT_ENTRY_SIZE  1024 /* 128 */

/* 2048*/ /* 256, 100pkts*2*10ms=2000*12B=>24k */
#define DPMAIF_DL_PIT_ENTRY_SIZE  (DPMAIF_DL_BAT_ENTRY_SIZE * 2)
#define DPMAIF_DL_PIT_BYTE_SIZE   12

/*DPMAIF AO UL CONFIG: 0x10014000 / ao + 0*/
#define DPMAIF_AO_UL_CHNL0_STA            (DPMAIF_AO_UL_CFG_BASE + 0x0000)
#define DPMAIF_AO_UL_CHNL1_STA            (DPMAIF_AO_UL_CFG_BASE + 0x0004)
#define DPMAIF_AO_UL_CHNL2_STA            (DPMAIF_AO_UL_CFG_BASE + 0x0008)
#define DPMAIF_AO_UL_CHNL3_STA            (DPMAIF_AO_UL_CFG_BASE + 0x000C)

/*DPMAIF PD UL CONFIG: 0x1022D000 /pd+0*/
#define DPMAIF_PD_UL_ADD_DESC             NRL2_DPMAIF_UL_ADD_DESC
#define DPMAIF_PD_UL_ADD_DESC_CH          (DPMAIF_PD_UL_CFG_BASE + 0x00B0)

#define DPMAIF_PD_DL_MISC_CON0            (DPMAIF_PD_DL_CFG_BASE + 0x0040)
#define DPMAIF_PD_DL_STA0                 (DPMAIF_PD_DL_CFG_BASE + 0x0080)
#define DPMAIF_PD_DL_DBG_STA14             (DPMAIF_PD_DL_CFG_BASE + 0x00FC)

#define DPMAIF_AO_DL_PKTINFO_CONO         (DPMAIF_AO_DL_CFG_BASE + 0x0000)
#define DPMAIF_AO_DL_FRGBAT_STA2          (DPMAIF_AO_DL_CFG_BASE + 0x0048)

#define DPMAIF_PD_AP_CODA_VER             (DPMAIF_PD_AP_MISC_BASE + 0x006C)
#define DPMAIF_PD_AP_CG_EN                (DPMAIF_PD_AP_MISC_BASE + 0x0068)

/*DPMAIF_PD_DL_DBG_STA1*/
#define DPMAIF_DL_IDLE_STS             (1 << 25)

/*DPMAIF_PD_UL_DBG_STA2*/
#define DPMAIF_UL_STS_CUR_SHIFT        (26)
#define DPMAIF_UL_IDLE_STS_MSK         (0x3F)
#define DPMAIF_UL_IDLE_STS             (0x01)

/* === tx interrupt mask === */
#define UL_INT_EMPTY_OFFSET         4
#define UL_INT_MD_NOTRDY_OFFSET     8
#define UL_INT_PWR_NOTRDY_OFFSET    12
#define UL_INT_LEN_ERR_OFFSET       16

#define DPMAIF_UL_INT_DONE(q_num)            (1 << (q_num+UL_INT_DONE_OFFSET))
#define DPMAIF_UL_INT_EMPTY(q_num)           (1 << (q_num+UL_INT_EMPTY_OFFSET))
#define DPMAIF_UL_INT_MD_NOTRDY(q_num)       (1 << (q_num+UL_INT_MD_NOTRDY_OFFSET))
#define DPMAIF_UL_INT_PWR_NOTRDY(q_num)      (1 << (q_num+UL_INT_PWR_NOTRDY_OFFSET))
#define DPMAIF_UL_INT_LEN_ERR(q_num)         (1 << (q_num+UL_INT_LEN_ERR_OFFSET))

#define DPMAIF_UL_INT_QDONE_MSK              (0x0F << UL_INT_DONE_OFFSET)
#define DPMAIF_UL_INT_EMPTY_MSK              (0x0F << UL_INT_EMPTY_OFFSET)
#define DPMAIF_UL_INT_MD_NOTREADY_MSK        (0x0F << UL_INT_MD_NOTRDY_OFFSET)
#define DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK    (0x0F << UL_INT_PWR_NOTRDY_OFFSET)
#define DPMAIF_UL_INT_ERR_MSK                (0x0F << UL_INT_LEN_ERR_OFFSET)

#define AP_UL_L2INTR_ERR_En_Msk \
	(DPMAIF_UL_INT_ERR_MSK | DPMAIF_UL_INT_MD_NOTREADY_MSK | \
	DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK)

/* DPMAIF_UL_INT_EMPTY_MSK | */
#define AP_UL_L2INTR_En_Msk    (AP_UL_L2INTR_ERR_En_Msk | DPMAIF_UL_INT_QDONE_MSK)


#define DPMAIF_PD_UL_CHNL0_CON1           (DPMAIF_PD_UL_CFG_BASE + 0x0024)
#define DPMAIF_PD_UL_CHNL0_CON0           (DPMAIF_PD_UL_CFG_BASE + 0x0020)

#define DPMAIF_PD_UL_CHNL0_STA0           NRL2_DPMAIF_AO_UL_CHNL0_STA0


#define DPMAIF_ULQSAR_n(q_num)      \
	((DPMAIF_PD_UL_CHNL0_CON0) + (0x10 * (q_num)))

#define DPMAIF_UL_DRBSIZE_ADDRH_n(q_num)   \
	((DPMAIF_PD_UL_CHNL0_CON1) + (0x10 * (q_num)))

#define DPMAIF_ULQ_STA0_n(q_num)            \
	((DPMAIF_PD_UL_CHNL0_STA0) + (0x04 * (q_num)))

#define DPMAIF_PD_AP_UL_L2TICR0              (DPMAIF_PD_AP_MISC_BASE + 0x0008)
#define DPMAIF_PD_AP_UL_L2TISR0              (DPMAIF_PD_AP_MISC_BASE + 0x000C)
#define DPMAIF_PD_AP_UL_L2TIMR0              (DPMAIF_PD_AP_MISC_BASE + 0x0004)
#define DPMAIF_PD_AP_DL_L2TIMCR0             (DPMAIF_PD_AP_MISC_BASE + 0x0058)
#define DPMAIF_PD_AP_DL_L2TIMSR0             (DPMAIF_PD_AP_MISC_BASE + 0x005C)
#define DPMAIF_PD_AP_DL_L2TIMR0              (DPMAIF_PD_AP_MISC_BASE + 0x0054)

#define DPMAIF_PD_AP_DLUL_IP_BUSY_MASK       (DPMAIF_PD_AP_MISC_BASE + 0x0064)

#define NRL2_DPMAIF_AO_DL_RDY_CHK_FRG_THRES  (BASE_DPMAIF_AO_DL + 0x30)


#define DPMAIF_AO_DL_RDY_CHK_FRG_THRES       NRL2_DPMAIF_AO_DL_RDY_CHK_FRG_THRES

#define DPMAIF_AO_DL_PIT_STA2                (DPMAIF_AO_DL_CFG_BASE + 0x0028)


#define DPMAIF_AO_DL_PKTINFO_CON2         (DPMAIF_AO_DL_CFG_BASE + 0x0008)

#define DPMAIF_PD_UL_CHNL_ARB0            (DPMAIF_PD_UL_CFG_BASE + 0x0010)


#define DPMAIF_PIT_SIZE_MSK            0xFFFF
#define DPMAIF_DL_PIT_WRIDX_MSK        0xFFFF

#endif /* __CCCI_DPMAIF_REG_V1_H__ */
