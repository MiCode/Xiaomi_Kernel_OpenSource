/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*! \file   "mt66xx_reg.h"
 *   \brief  The common register definition of MT6630
 *
 *   N/A
 */



#ifndef _MT66XX_REG_H
#define _MT66XX_REG_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#ifdef MT6632
extern struct mt66xx_hif_driver_data mt66xx_driver_data_mt6632;
#endif /* MT6632 */
#ifdef MT7668
extern struct mt66xx_hif_driver_data mt66xx_driver_data_mt7668;
#endif /* MT7668 */
#ifdef MT7663
extern struct mt66xx_hif_driver_data mt66xx_driver_data_mt7663;
#endif /* MT7663 */
#ifdef CONNAC
extern struct mt66xx_hif_driver_data mt66xx_driver_data_connac;
#endif /* CONNAC */
#ifdef CONNAC2X2
extern struct mt66xx_hif_driver_data mt66xx_driver_data_connac2x2;
#endif /* CONNAC2X2 */
#ifdef UT_TEST_MODE
extern struct mt66xx_hif_driver_data mt66xx_driver_data_ut;
#endif /* UT_TEST_MODE */
#ifdef MT7915
extern struct mt66xx_hif_driver_data mt66xx_driver_data_mt7915;
#endif /* MT7915 */
#ifdef SOC3_0
extern struct mt66xx_hif_driver_data mt66xx_driver_data_soc3_0;
#endif /* SOC3_0 */
#ifdef MT7961
extern struct mt66xx_hif_driver_data mt66xx_driver_data_mt7961;
#endif /* MT7961 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* 1 MT6630 MCR Definition */

/* 2 Host Interface */

/* #define TOP_CFG_BASE 0x0000 */
#define TOP_CFG_BASE 0x80020000
#define TOP_RGU_BASE 0x81020000
#define TOP_CFG_AON_base     0x81021000
#define MCU_CFG_BASE 0x80000000

#define TOP_HVR              (TOP_CFG_BASE + 0x1000)
#define HW_VER_MASK          (0xffff)
#define GET_HW_VER(p)        (((p) & HW_VER_MASK))
#define RESC_CID_MASK        (0xf << 28)
#define GET_RESC_CID(p)      (((p) & RESC_CID_MASK) >> 28)

#define TOP_FVR              (TOP_CFG_BASE + 0x1004)
#define FW_VER_MASK          (0xffff)
#define GET_FW_VER(p)        (((p) & FW_VER_MASK))

#define TOP_HCR              (TOP_CFG_BASE + 0x1008)
#define HW_CODE_MASK         (0xffff)
#define GET_HW_CODE(p)       (((p) & HW_CODE_MASK))

#define STRAP_STA            (TOP_CFG_BASE + 0x1010)
#define XTAL_SEL_MASK        (0x3)
#define GET_XTAL_SEL(p)      (((p) & XTAL_SEL_MASK))
#define EEPROM_SEL           (1 << 2)
#define GET_EEPROM_SEL(p)    (((p) & EEPROM_SEL) >> 2)
#define CO_CLOCK_SEL         (1 << 8)
#define GET_CO_CLOCK_SEL(p)  (((p) & CO_CLOCK_SEL) >> 8)
#define ONE_ANT              (1 << 24)
#define GET_ONE_ANT(p)       (((p) & ONE_ANT) >> 24)
#define USB_MODE             (1 << 25)
#define GET_USB_MODE(p)      (((p) & USB_MODE) >> 25)

#define TOP_MISC2            (TOP_CFG_BASE + 0x1134)

#define TOP_CKGEN2_CR_PMIC_CK_MANUAL             (TOP_CFG_AON_base + 0x00000108)
#define TOP_CKGEN2_CR_PMIC_CK_MANUAL_MASK        0x00080000

#define MTK_CHIP_REV                    0x00006632

#define WIFI_RGU_SW_SYNC0    (TOP_RGU_BASE + 0x1250)
#define WIFI_RGU_SYNC0_RDY_OFFSET (0)
#define MCU_CFG_PCIE_REMAP1  (MCU_CFG_BASE + 0x0500)
#define PCIE_REMAP1_OFFSET   (18)
#define PCIE_REMAP1_MASK     (BITS(18, 31))
#define PCIE_REMAP1_BUS_ADDR (0x40000)
#define MCU_CFG_PCIE_REMAP2  (MCU_CFG_BASE + 0x0504)
#define PCIE_REMAP2_OFFSET   (19)
#define PCIE_REMAP2_MASK     (BITS(19, 31))
#define PCIE_REMAP2_BUS_ADDR (0x80000)



/* UMAC Register */
#define UMAC_PLE_CR_CFG_BASE_ADDR       0x82060000
#define UMAC_PSE_CR_CFG_BASE_ADDR       0x82068000

#define UMAC_PSE_PLE_CR_ADDR_DIFF \
	(UMAC_PSE_CR_CFG_BASE_ADDR - UMAC_PLE_CR_CFG_BASE_ADDR)
#define UMAC_PSE_CR_BITMAP_OFFSET       15
#define UMAC_PSE_PLE_ADDR_DIFF_MAR(_x) \
	(_x << UMAC_PSE_CR_BITMAP_OFFSET)


#define UMAC_PLE_BASE_ADDRESS   (0xa << 28)

#define UMAC_PSE_BASE_ADDRESS   (0xb << 28)

#define UMAC_FID_SHIFT_16_BIT_VM_MAP    16

#define UMAC_BASE(_x) \
	(UMAC_PLE_CR_CFG_BASE_ADDR | (_x << UMAC_PSE_CR_BITMAP_OFFSET))

#define UMAC_RESET(_x)                  (UMAC_BASE(_x) + 0x00000000)

#define UMAC_INT_CR4_EN_MASK(_x)        (UMAC_BASE(_x) + 0x00000004)
#define UMAC_INT_CR4_STS(_x)            (UMAC_BASE(_x) + 0x00000008)
#define UMAC_INT_CR4_ERR_RES(_x)        (UMAC_BASE(_x) + 0x0000000C)
#define UMAC_INT_CR4_ERR_MASK(_x)       (UMAC_BASE(_x) + 0x00000010)



#define UMAC_PBUF_CTRL(_x)              (UMAC_BASE(_x) + 0x00000014)

#define UMAC_CHIP_ID_VER(_x)            (UMAC_BASE(_x) + 0x00000018)

#define UMAC_TIMER_CNF(_x)              (UMAC_BASE(_x) + 0x0000001C)

#define UMAC_INT_N9_EN_MASK(_x)         (UMAC_BASE(_x) + 0x00000020)
#define UMAC_INT_N9_STS(_x)             (UMAC_BASE(_x) + 0x00000024)
#define UMAC_INT_N9_ERR_STS(_x)         (UMAC_BASE(_x) + 0x00000028)
#define UMAC_INT_N9_ERR_MASK(_x)        (UMAC_BASE(_x) + 0x0000002C)
#define UMAC_IGNORE_BUSY_EN_MASK(_x)    (UMAC_BASE(_x) + 0x0000038C)

#define UMAC_RELEASE_CTRL(_x)           (UMAC_BASE(_x) + 0x00000030)

#define UMAC_HIF_REPROT(_x)             (UMAC_BASE(_x) + 0x00000034)


#define UMAC_C_GET_FID_0(_x)            (UMAC_BASE(_x) + 0x00000040)
#define UMAC_C_GET_FID_1(_x)            (UMAC_BASE(_x) + 0x00000044)

#define UMAC_C_EN_QUEUE_0(_x)           (UMAC_BASE(_x) + 0x00000060)
#define UMAC_C_EN_QUEUE_1(_x)           (UMAC_BASE(_x) + 0x00000064)
#define UMAC_C_EN_QUEUE_2(_x)           (UMAC_BASE(_x) + 0x00000068)


#define UMAC_C_DE_QUEUE_0(_x)           (UMAC_BASE(_x) + 0x00000080)
#define UMAC_C_DE_QUEUE_1(_x)           (UMAC_BASE(_x) + 0x00000084)
#define UMAC_C_DE_QUEUE_2(_x)           (UMAC_BASE(_x) + 0x00000088)
#define UMAC_C_DE_QUEUE_3(_x)           (UMAC_BASE(_x) + 0x0000008C)

#define UMAC_ALLOCATE_0(_x)             (UMAC_BASE(_x) + 0x000000A0)
#define UMAC_ALLOCATE_1(_x)             (UMAC_BASE(_x) + 0x000000A4)
#define UMAC_ALLOCATE_2(_x)             (UMAC_BASE(_x) + 0x000000A8)

#define UMAC_QUEUE_EMPTY(_x)            (UMAC_BASE(_x) + 0x000000B0)

/* 0x820680B4 QUEUE_EMPTY_MASK Queue empty status mask register,
 * 7615 E3 eco item only PSE with this setting, but ple
 */
#define UMAC_QUEUE_EMPTY_MASK \
	(UMAC_BASE(UMAC_PSE_CFG_POOL_INDEX) + 0x000000B4)

#define UMAC_TO_CR4_INT(_x)             (UMAC_BASE(_x) + 0x000000e0)
#define UMAC_TO_N9_INT(_x)              (UMAC_BASE(_x) + 0x000000f0)


#define UMAC_FREEPG_CNT(_x)             (UMAC_BASE(_x) + 0x00000100)

#define UMAC_FREEPG_HEAD_TAIL(_x)       (UMAC_BASE(_x) + 0x00000104)


#define UMAC_PG_HIF0_GROUP(_x)          (UMAC_BASE(_x) + 0x00000110)
#define UMAC_HIF0_PG_INFO(_x)           (UMAC_BASE(_x) + 0x00000114)

#define UMAC_PG_HIF1_GROUP(_x)          (UMAC_BASE(_x) + 0x00000118)
#define UMAC_HIF1_PG_INFO(_x)           (UMAC_BASE(_x) + 0x0000011C)

#define UMAC_PG_CPU_GROUP(_x)           (UMAC_BASE(_x) + 0x00000150)
#define UMAC_CPU_PG_INFO(_x)            (UMAC_BASE(_x) + 0x00000154)


#define UMAC_PG_LMAC0_GROUP(_x)         (UMAC_BASE(_x) + 0x00000170)
#define UMAC_LMAC0_PG_INFO(_x)          (UMAC_BASE(_x) + 0x00000174)

#define UMAC_PG_LMAC1_GROUP(_x)         (UMAC_BASE(_x) + 0x00000178)
#define UMAC_LMAC1_PG_INFO(_x)          (UMAC_BASE(_x) + 0x0000017C)


#define UMAC_PG_LMAC2_GROUP(_x)         (UMAC_BASE(_x) + 0x00000180)
#define UMAC_LMAC2_PG_INFO(_x)          (UMAC_BASE(_x) + 0x00000184)

#define UMAC_PG_PLE_GROUP(_x)           (UMAC_BASE(_x) + 0x00000190)
#define UMAC_PLE_PG_INFO(_x)            (UMAC_BASE(_x) + 0x00000194)


#define UMAC_RL_BUF_CTRL_0(_x)          (UMAC_BASE(_x) + 0x000001A0)
#define UMAC_RL_BUF_CTRL_1(_x)          (UMAC_BASE(_x) + 0x000001A4)

#define UMAC_FL_QUE_CTRL_0(_x)          (UMAC_BASE(_x) + 0x000001B0)
#define UMAC_FL_QUE_CTRL_1(_x)          (UMAC_BASE(_x) + 0x000001B4)
#define UMAC_FL_QUE_CTRL_2(_x)          (UMAC_BASE(_x) + 0x000001B8)
#define UMAC_FL_QUE_CTRL_3(_x)          (UMAC_BASE(_x) + 0x000001BC)

#define UMAC_HIF_ENQ_PKT_NUM(_x)        (UMAC_BASE(_x) + 0x000001F0)
#define UMAC_CPU_ENQ_PKT_NUM(_x)        (UMAC_BASE(_x) + 0x000001F4)
#define UMAC_RLS_MSDU_PKT_NUM(_x)       (UMAC_BASE(_x) + 0x000001F8)
#define UMAC_HOST_REPORT_NUM(_x)        (UMAC_BASE(_x) + 0x000001FC)

#define UMAC_PL_QUE_CTRL_0(_x)          (UMAC_BASE(_x) + 0x000001C0)

#define UMAC_TP_HIF_EN(_x)              (UMAC_BASE(_x) + 0x00000200)

#define UMAC_TP_HIF_Q_CTRL0(_x)         (UMAC_BASE(_x) + 0x00000204)
#define UMAC_TP_HIF_Q_CTRL1(_x)         (UMAC_BASE(_x) + 0x00000208)
#define UMAC_TP_HIF_Q_CTRL2(_x)         (UMAC_BASE(_x) + 0x0000020C)

#define UMAC_TP_HIF_ALLOC0(_x)          (UMAC_BASE(_x) + 0x00000210)
#define UMAC_TP_HIF_ALLOC1(_x)          (UMAC_BASE(_x) + 0x00000214)

#define UMAC_TP_HIF_OPER0(_x)           (UMAC_BASE(_x) + 0x00000218)
#define UMAC_TP_HIF_OPER1(_x)           (UMAC_BASE(_x) + 0x0000021C)


#define UMAC_TP_LMAC_EN(_x)             (UMAC_BASE(_x) + 0x00000220)

#define UMAC_TP_LMAC_Q_CTRL0(_x)        (UMAC_BASE(_x) + 0x00000224)
#define UMAC_TP_LMAC_Q_CTRL1(_x)        (UMAC_BASE(_x) + 0x00000228)
#define UMAC_TP_LMAC_Q_CTRL2(_x)        (UMAC_BASE(_x) + 0x0000022C)

#define UMAC_TP_LMAC_ALLOC0(_x)         (UMAC_BASE(_x) + 0x00000230)
#define UMAC_TP_LMAC_ALLOC1(_x)         (UMAC_BASE(_x) + 0x00000234)

#define UMAC_TP_LMAC_OPER0(_x)          (UMAC_BASE(_x) + 0x00000238)
#define UMAC_TP_LMAC_OPER1(_x)          (UMAC_BASE(_x) + 0x0000023C)

#define UMAC_STATE_IDLE_CTL(_x)         (UMAC_BASE(_x) + 0x0000024C)

#define UMAC_DIS_STA_MAP0(_x)           (UMAC_BASE(_x) + 0x00000260)
#define UMAC_DIS_STA_MAP1(_x)           (UMAC_BASE(_x) + 0x00000264)
#define UMAC_DIS_STA_MAP2(_x)           (UMAC_BASE(_x) + 0x00000268)
#define UMAC_DIS_STA_MAP3(_x)           (UMAC_BASE(_x) + 0x0000026c)

#define UMAC_AC0_QUEUE_EMPTY0(_x)       (UMAC_BASE(_x) + 0x00000300)
#define UMAC_AC0_QUEUE_EMPTY1(_x)       (UMAC_BASE(_x) + 0x00000304)
#define UMAC_AC0_QUEUE_EMPTY2(_x)       (UMAC_BASE(_x) + 0x00000308)
#define UMAC_AC0_QUEUE_EMPTY3(_x)       (UMAC_BASE(_x) + 0x0000030C)

#define UMAC_AC1_QUEUE_EMPTY0(_x)       (UMAC_BASE(_x) + 0x00000310)
#define UMAC_AC1_QUEUE_EMPTY1(_x)       (UMAC_BASE(_x) + 0x00000314)
#define UMAC_AC1_QUEUE_EMPTY2(_x)       (UMAC_BASE(_x) + 0x00000318)
#define UMAC_AC1_QUEUE_EMPTY3(_x)       (UMAC_BASE(_x) + 0x0000031C)

#define UMAC_AC2_QUEUE_EMPTY0(_x)       (UMAC_BASE(_x) + 0x00000320)
#define UMAC_AC2_QUEUE_EMPTY1(_x)       (UMAC_BASE(_x) + 0x00000324)
#define UMAC_AC2_QUEUE_EMPTY2(_x)       (UMAC_BASE(_x) + 0x00000328)
#define UMAC_AC2_QUEUE_EMPTY3(_x)       (UMAC_BASE(_x) + 0x0000032C)

#define UMAC_AC3_QUEUE_EMPTY0(_x)       (UMAC_BASE(_x) + 0x00000330)
#define UMAC_AC3_QUEUE_EMPTY1(_x)       (UMAC_BASE(_x) + 0x00000334)
#define UMAC_AC3_QUEUE_EMPTY2(_x)       (UMAC_BASE(_x) + 0x00000338)
#define UMAC_AC3_QUEUE_EMPTY3(_x)       (UMAC_BASE(_x) + 0x0000033C)

#define UMAC_QUEUE_EMPTY_MASK \
	(UMAC_BASE(UMAC_PSE_CFG_POOL_INDEX) + 0x000000B4)

/* BSS PS INT */
#define UMAC_N9_BSS_PS_INT_EN \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000000F4)
#define UMAC_N9_BSS_PS_INT_STS \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000000F8)

/* CR for VoW and BW Ctrl */

#define UMAC_DRR_TABLE_CTRL0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000388)

#define UMAC_DRR_TABLE_WDATA0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000340)
#define UMAC_DRR_TABLE_WDATA1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000344)
#define UMAC_DRR_TABLE_WDATA2 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000348)
#define UMAC_DRR_TABLE_WDATA3 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x0000034C)


#define UMAC_DRR_TABLE_RDATA0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000350)
#define UMAC_DRR_TABLE_RDATA1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000354)
#define UMAC_DRR_TABLE_RDATA2 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000358)
#define UMAC_DRR_TABLE_RDATA3 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x0000035C)


#define UMAC_STATION_PAUSE_0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000360)
#define UMAC_STATION_PAUSE_1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000364)
#define UMAC_STATION_PAUSE_2 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000368)
#define UMAC_STATION_PAUSE_3 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x0000036C)
#define UMAC_VOW_ENABLE \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000370)
#define UMAC_AIR_TIME_DRR_SIZE \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000374)
#define UMAC_CHECK_TIME_TOKEN \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000378)
#define UMAC_CHECK_LENGTH_TOKEN \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x0000037C)
#define UMAC_WDRR0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000380)
#define UMAC_WDRR1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x00000384)

#define UMAC_VOW_CTRL1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x0000038C)

#define UMAC_VOW_DBG_MUX \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003A0)
#define UMAC_AIRTIME_DBG_INFO0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003A4)
#define UMAC_AIRTIME_DBG_INFO1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003A8)

#define UMAC_BW_DBG_INFO \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003AC)

#define UMAC_BW_WDRR0 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003B0)
#define UMAC_BW_WDRR1 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003B4)
#define UMAC_BW_WDRR2 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003B8)
#define UMAC_BW_WDRR3 \
	(UMAC_BASE(UMAC_PLE_CFG_POOL_INDEX) + 0x000003BC)
/* UMAC Register */


#if defined(_HIF_USB)

#define WIFI_CFG_SW_SYNC0    (TOP_CFG_BASE + 0x1128)
#define WIFI_CFG_SYNC0_RDY_OFFSET (16)

/* UDMA register */
#define UDMA_BASE                         0x50029000
#define UDMA_TX_QSEL                      (UDMA_BASE + 0x8)
#define FW_DL_EN                          (1 << 3)

#define UDMA_RESET                        (UDMA_BASE + 0x14)

#define UDMA_WLCFG_1                      (UDMA_BASE + 0xc)
#define UDMA_WLCFG_1_TX_TMOUT_LMT_MASK    (0xfffff << 8)
#define UDMA_WLCFG_1_TX_TMOUT_LMT(p)      (((p) & 0xfffff) << 8)
#define UDMA_WLCFG_1_RX_AGG_PKT_LMT_MASK  (0xff << 0)
#define UDMA_WLCFG_1_RX_AGG_PKT_LMT(p)    (((p) & 0xff) << 0)

#define UDMA_WLCFG_0                      (UDMA_BASE + 0x18)
#define UDMA_WLCFG_0_TX_BT_SIZE_MASK      (0x07 << 27)
#define UDMA_WLCFG_0_TX_BT_SIZE(p)        (((p) & 0x07) << 27)
#define UDMA_WLCFG_0_RX_BT_SIZE_MASK      (0x07 << 24)
#define UDMA_WLCFG_0_RX_BT_SIZE(p)        (((p) & 0x07) << 24)
#define UDMA_WLCFG_0_TX_EN_MASK           (0x1 << 23)
#define UDMA_WLCFG_0_TX_EN(p)             (((p) & 0x1) << 23)
#define UDMA_WLCFG_0_RX_EN_MASK           (0x1 << 22)
#define UDMA_WLCFG_0_RX_EN(p)             (((p) & 0x1) << 22)
#define UDMA_WLCFG_0_RX_AGG_EN_MASK       (0x1 << 21)
#define UDMA_WLCFG_0_RX_AGG_EN(p)         (((p) & 0x1) << 21)
#define UDMA_WLCFG_0_LPK_EN_MASK          (0x1 << 20)
#define UDMA_WLCFG_0_LPK_EN(p)            (((p) & 0x1) << 20)
#define UDMA_WLCFG_0_RX_MPSZ_PAD0_MASK    (0x1 << 18)
#define UDMA_WLCFG_0_RX_MPSZ_PAD0(p)      (((p) & 0x1) << 18)
#define UDMA_WLCFG_0_RX_AGG_LMT_MASK      (0xff << 8)
#define UDMA_WLCFG_0_RX_AGG_LMT(p)        (((p) & 0xff) << 8)
#define UDMA_WLCFG_0_RX_AGG_TO_MASK       (0xff << 0)
#define UDMA_WLCFG_0_RX_AGG_TO(p)         (((p) & 0xff) << 0)

#define PP_R_RXCUTDISP0                   (0x8206C054)
#define PP_R_RXCUTDISP0_CT_EN_MASK        (0x1 << 0)
#define PP_R_RXCUTDISP0_CT_EN(p)          (((p) & 0x1) << 0)
#define PP_R_RXCUTDISP0_CR4_EN_MASK       (0x1 << 1)
#define PP_R_RXCUTDISP0_CR4_EN(p)         (((p) & 0x1) << 1)
#define PP_R_RXCUTDISP0_START_OFFSET_MASK (0x3fff << 2)
#define PP_R_RXCUTDISP0_START_OFFSET(p)   (((p) & 0x3fff) << 2)
#define PP_R_RXCUTDISP0_END_OFFSET_MASK   (0x3fff << 18)
#define PP_R_RXCUTDISP0_END_OFFSET(p)     (((p) & 0x3fff) << 18)


#elif defined(_HIF_PCIE) ||  defined(_HIF_AXI)
#define RTC_TOP_BASE					0x0000

#define RTC_TOP_MISC2					(RTC_TOP_BASE + 0x1128)

#define TOP_HIF_BASE					0x0000
#define TOP_HW_VERSION					0x1000
#define TOP_HW_CONTROL					0x1008

#define WIFI_CFG_SW_SYNC0				RTC_TOP_MISC2
#define WIFI_CFG_SYNC0_RDY_OFFSET		(16)

#define PCIE_HIF_BASE					0x4000
#define PCIE_NEW_HIF_BASE				0x7c030000

/* HIF Sys Revision */
#define HIF_SYS_REV		(PCIE_HIF_BASE + 0x0000)

/* Check Enter Slepp Mode Register */
#define CONN_DUMMY_CR		(PCIE_HIF_BASE + 0x00A8)

#define CONN_HIF_RST		(PCIE_HIF_BASE + 0x0100)

#define WPDMA_FIFO_TEST_MOD				(PCIE_HIF_BASE + 0x0140)

#define WPDMA_APSRC_ACK_LOCK_SLPPROT                    (PCIE_HIF_BASE + 0x0160)

/* HIF Low Power Control Host Register */
#define CFG_PCIE_LPCR_HOST				(PCIE_HIF_BASE + 0x01F0)

/* HIF Low Power Control Fw Register */
#define CFG_PCIE_LPCR_FW				(PCIE_HIF_BASE + 0x01F4)

/* Interrupt Status */
#define WPDMA_INT_STA					(PCIE_HIF_BASE + 0x0200)

/* Interrupt Mask */
#define WPDMA_INT_MSK					(PCIE_HIF_BASE + 0x0204)

/* Global Configuration */
#define WPDMA_GLO_CFG					(PCIE_HIF_BASE + 0x0208)

/* Pointer Reset */
#define WPDMA_RST_PTR					(PCIE_HIF_BASE + 0x020C)

/* Configuration for WPDMA Delayed Interrupt */
#define WPDMA_DELAY_INT_CFG				(PCIE_HIF_BASE + 0x0210)

#define WPDMA_PAUSE_RX_Q_TH10				(PCIE_HIF_BASE + 0x0260)
#define WPDMA_PAUSE_RX_Q_TH32				(PCIE_HIF_BASE + 0x0264)
#define WPDMA_PAUSE_RX_Q_TH0				1
#define WPDMA_PAUSE_RX_Q_TH1				1
#define WPDMA_PAUSE_RX_Q_TH2				1
#define WPDMA_PAUSE_RX_Q_TH3				1
#define WPDMA_PAUSE_RX_Q_TH0_MASK			0x00000FFF
#define WPDMA_PAUSE_RX_Q_TH1_MASK			0x0FFF0000
#define WPDMA_PAUSE_RX_Q_TH2_MASK			0x00000FFF
#define WPDMA_PAUSE_RX_Q_TH3_MASK			0x0FFF0000
#define WPDMA_PAUSE_RX_Q_TH0_SHFT			0
#define WPDMA_PAUSE_RX_Q_TH1_SHFT			16
#define WPDMA_PAUSE_RX_Q_TH2_SHFT			0
#define WPDMA_PAUSE_RX_Q_TH3_SHFT			16

/* TX Ring0 Control 0 */
#define WPDMA_TX_RING0_CTRL0			(PCIE_HIF_BASE + 0x0300)

/* TX Ring0 Control 1 */
#define WPDMA_TX_RING0_CTRL1			(PCIE_HIF_BASE + 0x0304)

/* TX Ring0 Control 2 */
#define WPDMA_TX_RING0_CTRL2			(PCIE_HIF_BASE + 0x0308)

/* TX Ring0 Control 3 */
#define WPDMA_TX_RING0_CTRL3			(PCIE_HIF_BASE + 0x030C)

/* RX Ring0 Control 0 */
#define WPDMA_RX_RING0_CTRL0			(PCIE_HIF_BASE + 0x0400)

/* RX Ring0 Control 1 */
#define WPDMA_RX_RING0_CTRL1			(PCIE_HIF_BASE + 0x0404)

/* RX Ring0 Control 2 */
#define WPDMA_RX_RING0_CTRL2			(PCIE_HIF_BASE + 0x0408)

/* RX Ring0 Control 3 */
#define WPDMA_RX_RING0_CTRL3			(PCIE_HIF_BASE + 0x040C)

/* TX RING more than 32bits extension */
#define WPDMA_TX_RING0_BASE_PTR_EXT		(PCIE_HIF_BASE + 0x0500)

/* RX RING more than 32bits extension */
#define WPDMA_RX_RING0_BASE_PTR_EXT		(PCIE_HIF_BASE + 0x0580)

#define MT_WPDMA_GLO_CFG_1              (PCIE_HIF_BASE + 0x0500)

#define MT_WPDMA_TX_PRE_CFG             (PCIE_HIF_BASE + 0x0510)

#define MT_WPDMA_RX_PRE_CFG             (PCIE_HIF_BASE + 0x0520)

#define MT_WPDMA_ABT_CFG                (PCIE_HIF_BASE + 0x0530)

#define MT_WPDMA_ABT_CFG1               (PCIE_HIF_BASE + 0x0534)

#define MT_PCIE_IRQ_ENABLE              (PCIE_NEW_HIF_BASE + 0x0188)

#define MD_INT_STA					(PCIE_HIF_BASE + 0x01C0)
#define MD_WPDMA_GLO_CFG				(PCIE_HIF_BASE + 0x01D0)
#define MD_INT_ENA					(PCIE_HIF_BASE + 0x01D4)
#define MD_WPDMA_DLY_INIT_CFG				(PCIE_HIF_BASE + 0x01D8)
#define MD_WPDMA_MISC					(PCIE_HIF_BASE + 0x01DC)

#define CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_ADDR (PCIE_HIF_BASE + 0x154)
#define CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_PDMA_AXI_SLPPROT_ENABLE_MASK BIT(0)
#define CONN_HIF_PDMA_CSR_PDMA_SLP_PROT_PDMA_AXI_SLPPROT_RDY_MASK BIT(16)

/* WPDMA_INT_STA */
union WPDMA_INT_STA_STRUCT {
	struct {
		uint32_t rx_done_0:1;
		uint32_t rx_done_1:1;
		uint32_t err_det_int_0:1;
		uint32_t err_det_int_1:1;
		uint32_t tx_done:16;
		uint32_t rx_coherent:1;
		uint32_t tx_coherent:1;
		uint32_t rx_dly_int:1;
		uint32_t tx_dly_int:1;
		uint32_t wf_mac_int_0:1;
		uint32_t wf_mac_int_1:1;
		uint32_t wf_mac_int_2:1;
		uint32_t wf_mac_int_3:1;
		uint32_t wf_mac_int_4:1;
		uint32_t wf_mac_int_5:1;
		uint32_t mcu_cmd_int:1;
		uint32_t fw_clr_own:1;
	} field;

	struct {
		uint32_t rx_done_0:1;
		uint32_t rx_done_1:1;
		uint32_t rx_done_2:1;
		uint32_t rx_done_3:1;
		uint32_t tx_done:16;
		uint32_t rx_coherent:1;
		uint32_t tx_coherent:1;
		uint32_t reserved:2;
		uint32_t wpdma2host_err_int_en:1;
		uint32_t tx_done_20:1;
		uint32_t tx_done_16:1;
		uint32_t tx_done_17:1;
		uint32_t subsys_int_en:1;
		uint32_t mcu2host_sw_int_en:1;
		uint32_t tx_done_18:1;
		uint32_t tx_done_19:1;
	} field_conn2x;

	struct {
		uint32_t wfdma1_rx_done_0:1;
		uint32_t wfdma1_rx_done_1:1;
		uint32_t wfdma1_rx_done_2:1;
		uint32_t wfdma1_rx_done_3:1;
		uint32_t wfdma1_tx_done_0:1;
		uint32_t wfdma1_tx_done_1:1;
		uint32_t wfdma1_tx_done_2:1;
		uint32_t wfdma1_tx_done_3:1;
		uint32_t wfdma1_tx_done_4:1;
		uint32_t wfdma1_tx_done_5:1;
		uint32_t wfdma1_tx_done_6:1;
		uint32_t wfdma1_tx_done_7:1;
		uint32_t wfdma1_tx_done_8:1;
		uint32_t reserved13:2;
		uint32_t wfdma1_tx_done_20:1;
		uint32_t wfdma0_rx_done_0:1;
		uint32_t wfdma0_rx_done_1:1;
		uint32_t wfdma0_rx_done_2:1;
		uint32_t wfdma0_rx_done_3:1;
		uint32_t wfdma1_rx_coherent:1;
		uint32_t wfdma1_tx_coherent:1;
		uint32_t wfdma0_rx_coherent:1;
		uint32_t wfdma0_tx_coherent:1;
		uint32_t wpdma2host1_err_int_en:1;
		uint32_t wpdma2host0_err_int_en:1;
		uint32_t wfdma1_tx_done_16:1;
		uint32_t wfdma1_tx_done_17:1;
		uint32_t wfdma1_subsys_int_en:1;
		uint32_t wfdma1_mcu2host_sw_int_en:1;
		uint32_t wfdma1_tx_done_18:1;
		uint32_t wfdma1_tx_done_19:1;
	} field_conn2x_ext;

	struct {
		uint32_t wfdma0_rx_done_0:1;
		uint32_t wfdma0_rx_done_1:1;
		uint32_t wfdma0_rx_done_2:1;
		uint32_t wfdma0_rx_done_3:1;
		uint32_t wfdma0_tx_done_0:1;
		uint32_t wfdma0_tx_done_1:1;
		uint32_t wfdma0_tx_done_2:1;
		uint32_t wfdma0_tx_done_3:1;
		uint32_t wfdma0_tx_done_4:1;
		uint32_t wfdma0_tx_done_5:1;
		uint32_t wfdma0_tx_done_6:1;
		uint32_t wfdma0_tx_done_7:1;
		uint32_t wfdma0_tx_done_8:1;
		uint32_t wfdma0_tx_done_9:1;
		uint32_t wfdma0_tx_done_10:1;
		uint32_t wfdma0_tx_done_11:1;
		uint32_t wfdma0_tx_done_12:1;
		uint32_t wfdma0_tx_done_13:1;
		uint32_t wfdma0_tx_done_14:1;
		uint32_t reserved19:1;
		uint32_t wfdma0_rx_coherent:1;
		uint32_t wfdma0_tx_coherent:1;
		uint32_t wfdma0_rx_done_4:1;
		uint32_t wfdma0_rx_done_5:1;
		uint32_t wpdma2host0_err_int_en:1;
		uint32_t reserved25:1;
		uint32_t wfdma0_tx_done_16:1;
		uint32_t wfdma0_tx_done_17:1;
		uint32_t wfdma0_subsys_int_en:1;
		uint32_t wfdma0_mcu2host_sw_int_en:1;
		uint32_t wfdma0_tx_done_18:1;
		uint32_t reserved31:1;
	} field_conn2x_single;

	uint32_t word;
};

/* WPDMA_INT_MSK */
union WPDMA_INT_MASK {
	struct {
		uint32_t rx_done_0:1;
		uint32_t rx_done_1:1;
		uint32_t err_det_int_0:1;
		uint32_t err_det_int_1:1;
		uint32_t tx_done:16;
		uint32_t rx_coherent:1;
		uint32_t tx_coherent:1;
		uint32_t rx_dly_int:1;
		uint32_t tx_dly_int:1;
		uint32_t wf_mac_int_0:1;
		uint32_t wf_mac_int_1:1;
		uint32_t wf_mac_int_2:1;
		uint32_t wf_mac_int_3:1;
		uint32_t wf_mac_int_4:1;
		uint32_t wf_mac_int_5:1;
		uint32_t mcu_cmd_int:1;
		uint32_t fw_clr_own:1;
	} field;

	struct {
		uint32_t rx_done_0:1;
		uint32_t rx_done_1:1;
		uint32_t err_det_int_0:1;
		uint32_t err_det_int_1:1;
		uint32_t tx_done:16;
		uint32_t rx_coherent:1;
		uint32_t tx_coherent:1;
		uint32_t rx_dly_int:1;
		uint32_t tx_dly_int:1;
		uint32_t wpdma2host_err_int_ena:1;
		uint32_t rsv_25_27:3;
		uint32_t subsys_int_ena:1;
		uint32_t mcu2host_sw_int_ena:1;
		uint32_t rsv_30_31:2;
	} field_conn;

	struct {
		uint32_t rx_done_0:1;
		uint32_t rx_done_1:1;
		uint32_t rx_done_2:1;
		uint32_t rx_done_3:1;
		uint32_t tx_done:16;
		uint32_t rx_coherent:1;
		uint32_t tx_coherent:1;
		uint32_t reserved:2;
		uint32_t wpdma2host_err_int_en:1;
		uint32_t tx_done_20:1;
		uint32_t tx_done_16:1;
		uint32_t tx_done_17:1;
		uint32_t subsys_int_en:1;
		uint32_t mcu2host_sw_int_en:1;
		uint32_t tx_done_18:1;
		uint32_t tx_done_19:1;
	} field_conn2x;

	struct {
		uint32_t wfdma1_rx_done_0:1;
		uint32_t wfdma1_rx_done_1:1;
		uint32_t wfdma1_rx_done_2:1;
		uint32_t wfdma1_rx_done_3:1;
		uint32_t wfdma1_tx_done_0:1;
		uint32_t wfdma1_tx_done_1:1;
		uint32_t wfdma1_tx_done_2:1;
		uint32_t wfdma1_tx_done_3:1;
		uint32_t wfdma1_tx_done_4:1;
		uint32_t wfdma1_tx_done_5:1;
		uint32_t wfdma1_tx_done_6:1;
		uint32_t wfdma1_tx_done_7:1;
		uint32_t wfdma1_tx_done_8:1;
		uint32_t reserved13:2;
		uint32_t wfdma1_tx_done_20:1;
		uint32_t wfdma0_rx_done_0:1;
		uint32_t wfdma0_rx_done_1:1;
		uint32_t wfdma0_rx_done_2:1;
		uint32_t wfdma0_rx_done_3:1;
		uint32_t wfdma1_rx_coherent:1;
		uint32_t wfdma1_tx_coherent:1;
		uint32_t wfdma0_rx_coherent:1;
		uint32_t wfdma0_tx_coherent:1;
		uint32_t wpdma2host1_err_int_en:1;
		uint32_t wpdma2host0_err_int_en:1;
		uint32_t wfdma1_tx_done_16:1;
		uint32_t wfdma1_tx_done_17:1;
		uint32_t wfdma1_subsys_int_en:1;
		uint32_t wfdma1_mcu2host_sw_int_en:1;
		uint32_t wfdma1_tx_done_18:1;
		uint32_t wfdma1_tx_done_19:1;
	} field_conn2x_ext;

	struct {
		uint32_t wfdma0_rx_done_0:1;
		uint32_t wfdma0_rx_done_1:1;
		uint32_t wfdma0_rx_done_2:1;
		uint32_t wfdma0_rx_done_3:1;
		uint32_t wfdma0_tx_done_0:1;
		uint32_t wfdma0_tx_done_1:1;
		uint32_t wfdma0_tx_done_2:1;
		uint32_t wfdma0_tx_done_3:1;
		uint32_t wfdma0_tx_done_4:1;
		uint32_t wfdma0_tx_done_5:1;
		uint32_t wfdma0_tx_done_6:1;
		uint32_t wfdma0_tx_done_7:1;
		uint32_t wfdma0_tx_done_8:1;
		uint32_t wfdma0_tx_done_9:1;
		uint32_t wfdma0_tx_done_10:1;
		uint32_t wfdma0_tx_done_11:1;
		uint32_t wfdma0_tx_done_12:1;
		uint32_t wfdma0_tx_done_13:1;
		uint32_t wfdma0_tx_done_14:1;
		uint32_t reserved19:1;
		uint32_t wfdma0_rx_coherent:1;
		uint32_t wfdma0_tx_coherent:1;
		uint32_t wfdma0_rx_done_4:1;
		uint32_t wfdma0_rx_done_5:1;
		uint32_t wpdma2host0_err_int_en:1;
		uint32_t reserved25:1;
		uint32_t wfdma0_tx_done_16:1;
		uint32_t wfdma0_tx_done_17:1;
		uint32_t wfdma0_subsys_int_en:1;
		uint32_t wfdma0_mcu2host_sw_int_en:1;
		uint32_t wfdma0_tx_done_18:1;
		uint32_t reserved31:1;
	} field_conn2x_single;
	uint32_t word;
};

/* WPDMA_GLO_CFG */
union WPDMA_GLO_CFG_STRUCT {
	struct {
		uint32_t EnableTxDMA:1;
		uint32_t TxDMABusy:1;
		uint32_t EnableRxDMA:1;
		uint32_t RxDMABusy:1;
		uint32_t WPDMABurstSIZE:2;
		uint32_t EnTXWriteBackDDONE:1;
		uint32_t BigEndian:1;
		uint32_t Desc32BEn:1;
		uint32_t share_fifo_en:1;
		uint32_t multi_dma_en:2;
		uint32_t fifo_little_endian:1;
		uint32_t mi_depth:3;
		uint32_t err_det_th:8;
		uint32_t sw_rst:1;
		uint32_t force_tx_eof:1;
		uint32_t rsv_26:1;
		uint32_t omit_rx_info:1;
		uint32_t omit_tx_info:1;
		uint32_t byte_swap:1;
		uint32_t clk_gate_dis:1;
		uint32_t rx_2b_offset:1;
	} field;

	struct {
		uint32_t EnableTxDMA:1;
		uint32_t TxDMABusy:1;
		uint32_t EnableRxDMA:1;
		uint32_t RxDMABusy:1;
		uint32_t WPDMABurstSIZE:2;
		uint32_t EnTXWriteBackDDONE:1;
		uint32_t BigEndian:1;
		uint32_t dis_bt_size_align:1;
		uint32_t tx_bt_size:1;
		uint32_t multi_dma_en:2;
		uint32_t fifo_little_endian:1;
		uint32_t mi_depth:3;
		uint32_t mi_depth_rd_3_5:3;
		uint32_t mi_depth_rd_8_6:3;
		uint32_t tx_bt_size_bit21:2;
		uint32_t sw_rst:1;
		uint32_t force_tx_eof:1;
		uint32_t first_token:1;
		uint32_t omit_rx_info:1;
		uint32_t omit_tx_info:1;
		uint32_t byte_swap:1;
		uint32_t reserve_30:1;
		uint32_t rx_2b_offset:1;
	} field_1;

	struct {
		uint32_t tx_dma_en:1;
		uint32_t tx_dma_busy:1;
		uint32_t rx_dma_en:1;
		uint32_t rx_dma_busy:1;
		uint32_t pdma_bt_size:2;
		uint32_t tx_wb_ddone:1;
		uint32_t big_endian:1;
		uint32_t dmad_32b_en:1;
		uint32_t bypass_dmashdl_txring3:1;
		uint32_t multi_dma_en:2;
		uint32_t fifo_little_endian:1;
		uint32_t mi_depth:3;
		uint32_t dfet_arb_mi_depth:3;
		uint32_t pfet_arb_mi_depth:3;
		uint32_t reserved22:3;
		uint32_t force_tx_eof:1;
		uint32_t pdma_addr_ext_en:1;
		uint32_t omit_rx_info:1;
		uint32_t omit_tx_info:1;
		uint32_t byte_swap:1;
		uint32_t clk_gate_dis:1;
		uint32_t rx_2b_offset:1;
	} field_conn;

	struct {
		uint32_t tx_dma_en:1;
		uint32_t tx_dma_busy:1;
		uint32_t rx_dma_en:1;
		uint32_t rx_dma_busy:1;
		uint32_t pdma_bt_size:2;
		uint32_t tx_wb_ddone:1;
		uint32_t big_endian:1;
		uint32_t dmad_32b_en:1;
		uint32_t bypass_dmashdl_txring:1;
		uint32_t reserved10:2;
		uint32_t fifo_little_endian:1;
		uint32_t csr_rx_wb_ddone:1;
		uint32_t csr_pp_hif_txp_active_en:1;
		uint32_t csr_disp_base_ptr_chain_en:1;
		uint32_t csr_lbk_rx_q_sel:4;
		uint32_t csr_lbk_rx_q_sel_en:1;
		/* define after Buzzard (rsv before) */
		uint32_t omit_rx_info_pfet2:1;
		uint32_t reserved22:2;
		uint32_t csr_sw_rst:1;
		uint32_t force_tx_eof:1;
		uint32_t pdma_addr_ext_en:1;
		uint32_t omit_rx_info:1;
		uint32_t omit_tx_info:1;
		uint32_t byte_swap:1;
		uint32_t clk_gate_dis:1;
		uint32_t rx_2b_offset:1;
	} field_conn2x;

	uint32_t word;
};

/* WPDMA_RST_PTR */
union WPDMA_RST_IDX_STRUCT {
	struct {
		uint32_t RST_DTX_IDX0:1;
		uint32_t RST_DTX_IDX1:1;
		uint32_t rsv_2_15:14;
		uint32_t RST_DRX_IDX0:1;
		uint32_t RST_DRX_IDX1:1;
		uint32_t rsv_18_31:14;
	} field;

	uint32_t word;
};

/* WPDMA_DELAY_INT_CFG */
union DELAY_INT_CFG_STRUCT {
	struct {
		uint32_t RXMAX_PTIME:8;
		uint32_t RXMAX_PINT:7;
		uint32_t RXDLY_INT_EN:1;
		uint32_t TXMAX_PTIME:8;
		uint32_t TXMAX_PINT:7;
		uint32_t TXDLY_INT_EN:1;
	} field;

	uint32_t word;
};

/* RTC_TOP_MISC2 */
#define TOP_MISC2_CR4_INIT_DONE			BIT(18)
#define TOP_MISC2_N9_INIT_DONE			BIT(17)
#define TOP_MISC2_FW_READY				BIT(16)
#define WLAN_READY_BITS					BITS(16, 18)

/* HIF_SYS_REV */
#define PCIE_HIF_SYS_PROJ				BITS(16, 31)
#define PCIE_HIF_SYS_REV				BITS(0, 15)

/* CFG_PCIE_LPCR_HOST */
#define PCIE_LPCR_AP_HOST_OWNER_STATE_SYNC	BIT(2)
#define PCIE_LPCR_HOST_CLR_OWN			BIT(1)
#define PCIE_LPCR_HOST_SET_OWN			BIT(0)

/* CFG_PCIE_LPCR_FW */
#define PCIE_LPCR_FW_CLR_OWN			BIT(0)

/* WPDMA_INT_STA */
#define WPDMA_FW_CLR_OWN_INT			BIT(31)
#define WPDMA_TX_DONE_INT15				BIT(19)
#define WPDMA_TX_DONE_INT3				BIT(7)
#define WPDMA_TX_DONE_INT2				BIT(6)
#define WPDMA_TX_DONE_INT1				BIT(5)
#define WPDMA_TX_DONE_INT0				BIT(4)
#define WPDMA_RX_DONE_INT3				BIT(3)
#define WPDMA_RX_DONE_INT2				BIT(2)
#define WPDMA_RX_DONE_INT1				BIT(1)
#define WPDMA_RX_DONE_INT0				BIT(0)

#else
#define WIFI_CFG_SW_SYNC0			    0
#define WIFI_CFG_SYNC0_RDY_OFFSET       0
#endif

/* 4 CHIP ID Register */
#define MCR_WCIR                            0x0000

/* 4 HIF Low Power Control  Register */
#define MCR_WHLPCR                          0x0004

/* 4 Control  Status Register */
#define MCR_WSDIOCSR                        0x0008

/* 4 HIF Control Register */
#define MCR_WHCR                            0x000C

/* 4 HIF Interrupt Status  Register */
#define MCR_WHISR                           0x0010

/* 4 HIF Interrupt Enable  Register */
#define MCR_WHIER                           0x0014

/* 4 Abnormal Status Register */
#define MCR_WASR                            0x0020

/* 4 WLAN Software Interrupt Control Register */
#define MCR_WSICR                           0x0024

/* 4 WLAN TX Data Register 1 */
#define MCR_WTDR1                           0x0034

/* 4 WLAN RX Data Register 0 */
#define MCR_WRDR0                           0x0050

/* 4 WLAN RX Data Register 1 */
#define MCR_WRDR1                           0x0054

/* 4 Host to Device Send Mailbox 0 Register */
#define MCR_H2DSM0R                         0x0070

/* 4 Host to Device Send Mailbox 1 Register */
#define MCR_H2DSM1R                         0x0074

/* 4 Device to Host Receive Mailbox 0 Register */
#define MCR_D2HRM0R                         0x0078

/* 4 Device to Host Receive Mailbox 1 Register */
#define MCR_D2HRM1R                         0x007c

/* 4 Device to Host Receive Mailbox 2 Register */
#define MCR_D2HRM2R                         0x0080

/* 4 WLAN RX Packet Length Register */
#define MCR_WRPLR                           0x0090

/* 4 Test Mode Data Port */
#define MCR_WTMDR                           0x00b0

/* 4 Test Mode Control Register */
#define MCR_WTMCR                           0x00b4

/* 4 Test Mode Data Pattern Control Register #0 */
#define MCR_WTMDPCR0                        0x00b8

/* 4 Test Mode Data Pattern Control Register #1 */
#define MCR_WTMDPCR1                        0x00bc

/* 4 WLAN Packet Length Report Control Register */
#define MCR_WPLRCR                          0x00d4

/* 4 WLAN Snapshot Register */
#define MCR_WSR                             0x00D8

/* 4 Clock Pad Macro IO Control Register */
#define MCR_CLKIOCR                         0x0100

/* 4 Command Pad Macro IO Control Register */
#define MCR_CMDIOCR                         0x0104

/* 4 Data 0 Pad Macro IO Control Register */
#define MCR_DAT0IOCR                        0x0108

/* 4 Data 1 Pad Macro IO Control Register */
#define MCR_DAT1IOCR                        0x010C

/* 4 Data 2 Pad Macro IO Control Register */
#define MCR_DAT2IOCR                        0x0110

/* 4 Data 3 Pad Macro IO Control Register */
#define MCR_DAT3IOCR                        0x0114

/* 4 Clock Pad Macro Delay Chain Control Register */
#define MCR_CLKDLYCR                        0x0118

/* 4 Command Pad Macro Delay Chain Control Register */
#define MCR_CMDDLYCR                        0x011C

/* 4 SDIO Output Data Delay Chain Control Register */
#define MCR_ODATDLYCR                       0x0120

/* 4 SDIO Input Data Delay Chain Control Register 1 */
#define MCR_IDATDLYCR1                      0x0124

/* 4 SDIO Input Data Delay Chain Control Register 2 */
#define MCR_IDATDLYCR2                      0x0128

/* 4 SDIO Input Data Latch Time Control Register */
#define MCR_ILCHCR                          0x012C

/* 4 WLAN TXQ Count Register 0 */
#define MCR_WTQCR0                          0x0130

/* 4 WLAN TXQ Count Register 1 */
#define MCR_WTQCR1                          0x0134

/* 4 WLAN TXQ Count Register 2 */
#define MCR_WTQCR2                          0x0138

/* 4 WLAN TXQ Count Register 3 */
#define MCR_WTQCR3                          0x013C

/* 4 WLAN TXQ Count Register 4 */
#define MCR_WTQCR4                          0x0140

/* 4 WLAN TXQ Count Register 5 */
#define MCR_WTQCR5                          0x0144

/* 4 WLAN TXQ Count Register 6 */
#define MCR_WTQCR6                          0x0148

/* 4 WLAN TXQ Count Register 7 */
#define MCR_WTQCR7                          0x014C

/* WLAN/Common PC value Debug registre */
#define MCR_SWPCDBGR				0x0154

/* #if CFG_SDIO_INTR_ENHANCE */
struct ENHANCE_MODE_DATA_STRUCT {
	uint32_t u4WHISR;
	union {
		struct {
			uint16_t u2TQ0Cnt;
			uint16_t u2TQ1Cnt;
			uint16_t u2TQ2Cnt;
			uint16_t u2TQ3Cnt;
			uint16_t u2TQ4Cnt;
			uint16_t u2TQ5Cnt;
			uint16_t u2TQ6Cnt;
			uint16_t u2TQ7Cnt;
			uint16_t u2TQ8Cnt;
			uint16_t u2TQ9Cnt;
			uint16_t u2TQ10Cnt;
			uint16_t u2TQ11Cnt;
			uint16_t u2TQ12Cnt;
			uint16_t u2TQ13Cnt;
			uint16_t u2TQ14Cnt;
			uint16_t u2TQ15Cnt;
		} u;
		uint32_t au4WTSR[8];
	} rTxInfo;
	union {
		struct {
			uint16_t u2NumValidRx0Len;
			uint16_t u2NumValidRx1Len;
			uint16_t au2Rx0Len[16];
			uint16_t au2Rx1Len[16];
		} u;
		uint32_t au4RxStatusRaw[17];
	} rRxInfo;
	uint32_t u4RcvMailbox0;
	uint32_t u4RcvMailbox1;
};
/* #endif *//* ENHANCE_MODE_DATA_STRUCT_T */

/* 2 Definition in each register */
/* 3 WCIR 0x0000 */
#define WCIR_WLAN_READY                 BIT(21)
#define WCIR_POR_INDICATOR              BIT(20)
#define WCIR_REVISION_ID                BITS(16, 19)
#define WCIR_CHIP_ID                    BITS(0, 15)

#define MTK_CHIP_REV                    0x00006632
#define MTK_CHIP_MP_REVERSION_ID        0x0

/* 3 WHLPCR 0x0004 */
#define WHLPCR_FW_OWN_REQ_CLR           BIT(9)
#define WHLPCR_FW_OWN_REQ_SET           BIT(8)
#define WHLPCR_IS_DRIVER_OWN            BIT(8)
#define WHLPCR_INT_EN_CLR               BIT(1)
#define WHLPCR_INT_EN_SET               BIT(0)

/* 3 WSDIOCSR 0x0008 */
#define WSDIOCSR_DB_CMD7_RESELECT_DIS   BIT(4)
#define WSDIOCSR_DB_WR_BUSY_EN          BIT(3)
#define WSDIOCSR_DB_RD_BUSY_EN          BIT(2)
#define WSDIOCSR_SDIO_INT_CTL           BIT(1)
#define WSDIOCSR_SDIO_RE_INIT_EN        BIT(0)

/* 3 WHCR 0x000C */
#define WHCR_RX_ENHANCE_MODE_EN         BIT(16)
#define WHCR_MAX_HIF_RX_LEN_NUM         BITS(8, 13)
#define WHCR_RPT_OWN_RX_PACKET_LEN      BIT(3)
#define WHCR_RECV_MAILBOX_RD_CLR_EN     BIT(2)
#define WHCR_W_INT_CLR_CTRL             BIT(1)
#define WHCR_MCU_DBG_EN                 BIT(0)
#define WHCR_OFFSET_MAX_HIF_RX_LEN_NUM  8

/* 3 WHISR 0x0010 */
#define WHISR_D2H_SW_INT                BITS(8, 31)
#define WHISR_D2H_SW_ASSERT_INFO_INT    BIT(31)
#define WHISR_D2H_WKUP_BY_RX_PACKET		BIT(30)
#define WHISR_D2H_SW_RD_MAILBOX_INT     BIT(29)
#define WHISR_FW_OWN_BACK_INT           BIT(7)
#define WHISR_ABNORMAL_INT              BIT(6)
#define WHISR_RX1_DONE_INT              BIT(2)
#define WHISR_RX0_DONE_INT              BIT(1)
#define WHISR_TX_DONE_INT               BIT(0)

/* 3 WHIER 0x0014 */
#define WHIER_D2H_SW_INT                BITS(8, 31)
#define WHIER_FW_OWN_BACK_INT_EN        BIT(7)
#define WHIER_ABNORMAL_INT_EN           BIT(6)
#define WHIER_RX1_DONE_INT_EN           BIT(2)
#define WHIER_RX0_DONE_INT_EN           BIT(1)
#define WHIER_TX_DONE_INT_EN            BIT(0)
#define WHIER_DEFAULT                   (WHIER_RX0_DONE_INT_EN    | \
					 WHIER_RX1_DONE_INT_EN    | \
					 WHIER_TX_DONE_INT_EN     | \
					 WHIER_ABNORMAL_INT_EN    | \
					 WHIER_D2H_SW_INT           \
					 )

/* 3 WASR 0x0020 */
#define WASR_FW_OWN_INVALID_ACCESS      BIT(16)
#define WASR_RX1_UNDER_FLOW             BIT(9)
#define WASR_RX0_UNDER_FLOW             BIT(8)
#define WASR_TX1_OVER_FLOW              BIT(1)

/* 3 WSICR 0x0024 */
#define WSICR_H2D_SW_INT_SET            BITS(16, 31)

/* 3 WRPLR 0x0090 */
#define WRPLR_RX1_PACKET_LENGTH         BITS(16, 31)
#define WRPLR_RX0_PACKET_LENGTH         BITS(0, 15)

/* 3 WTMCR 0x00b4 */
#define WMTCR_TEST_MODE_FW_OWN          BIT(24)
#define WMTCR_PRBS_INIT_VAL             BITS(16, 23)
#define WMTCR_TEST_MODE_STATUS          BIT(8)
#define WMTCR_TEST_MODE_SELECT          BITS(0, 1)



/* Support features */
/* Options for VLAN-over-ethernet pkt to/from 802.11 LLC VLAN pkt.
 * This should depend on the configurations of HW-header-translation.
 */
#define FEAT_BITS_LLC_VLAN_TX           BIT(0)
#define FEAT_BITS_LLC_VLAN_RX           BIT(1)

/* Support features API */
#define FEAT_SUP_LLC_VLAN_TX(__chip_info) \
	((__chip_info)->features & FEAT_BITS_LLC_VLAN_TX)
#define FEAT_SUP_LLC_VLAN_RX(__chip_info) \
	((__chip_info)->features & FEAT_BITS_LLC_VLAN_RX)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

enum ENUM_WIFI_FUNC {
	WIFI_FUNC_INIT_DONE = BIT(0),
	WIFI_FUNC_N9_DONE = BIT(1),
	WIFI_FUNC_NO_CR4_READY_BITS = BITS(0, 1),
	WIFI_FUNC_CR4_READY = BIT(2),
	WIFI_FUNC_READY_BITS = BITS(0, 2),
	WIFI_FUNC_DUMMY_REQ = BIT(3)
};

enum enum_mt66xx_chip {
	MT66XX_CHIP_6632 = 0,
	MT66XX_CHIP_7666,
	MT66XX_CHIP_7668,
	MT66XX_CHIP_NUM
};

enum enum_workAround {
	WORKAROUND_MT7663_BRINGUP_20171205 = 0,
	WORKAROUND_NUM
};

struct mt66xx_chip_info {
	struct BUS_INFO *bus_info;
	struct FWDL_OPS_T *fw_dl_ops;
	struct TX_DESC_OPS_T *prTxDescOps;
	struct RX_DESC_OPS_T *prRxDescOps;
#if CFG_SUPPORT_QA_TOOL
	struct ATE_OPS_T *prAteOps;
#endif
	struct CHIP_DBG_OPS *prDebugOps;

	const unsigned int chip_id;	/* chip id */
	const unsigned int should_verify_chip_id;	/* verify chip id */
	const unsigned int sw_sync0;	/* sw_sync0 address */
	const unsigned int sw_ready_bits;	/* sw_sync0 ready bits */
	const unsigned int sw_ready_bit_offset;	/* sw_sync0 ready bit offset */
	const unsigned int patch_addr;	/* patch download start address */
	const unsigned int is_support_cr4;	/* support CR4 */
	const unsigned int is_support_wacpu;	/* support WA-CPU */
	const unsigned int txd_append_size;	/* hw mac txd append */
	const unsigned int rxd_size;	        /* hw mac rxd size */
	const unsigned int init_evt_rxd_size;	/* init event rxd size */
	const unsigned int pse_header_length;	/* NIC_TX_PSE_HEADER_LENGTH */
	const unsigned int init_event_size;     /* init event w/o rxd size */
	const unsigned int event_hdr_size;      /* event w/o rxd size */
	const unsigned int isNicCapV1;
	const unsigned int is_support_efuse; /* efuse support */
	const unsigned int top_hcr; /* TOP_HCR */
	const unsigned int top_hvr; /* TOP_HVR */
	const unsigned int top_fvr; /* TOP_FVR */
#if (CFG_SUPPORT_802_11AX == 1)
	const unsigned int arb_ac_mode_addr;
#endif /* CFG_SUPPORT_802_11AX == 1 */
	const unsigned int custom_oid_interface_version;
	const unsigned int em_interface_version;
	const unsigned int cmd_max_pkt_size;

	const struct ECO_INFO *eco_info;	/* chip version table */
	uint8_t eco_ver;	/* chip version */
	uint8_t ucPacketFormat;

	uint16_t u2TxInitCmdPort;
	uint16_t u2TxFwDlPort;
	uint16_t u2HifTxdSize;
	uint16_t u2CmdTxHdrSize;
	uint16_t u2RxSwPktBitMap;
	uint16_t u2RxSwPktEvent;
	uint16_t u2RxSwPktFrame;

	/* Extra TXD Size for TX Byte Count field (in unit of Byte) */
	uint32_t u4ExtraTxByteCount;
	uint32_t u4HifDmaShdlBaseAddr;
	/* chip ip version from FW */
	uint32_t u4ChipIpVersion;
	uint32_t u4ChipIpConfig;
	uint16_t u2ADieChipVersion;

	void (*asicCapInit)(IN struct ADAPTER *prAdapter);
	void (*asicEnableFWDownload)(IN struct ADAPTER *prAdapter,
		IN u_int8_t fgEnable);
	void (*asicFillInitCmdTxd)(IN struct ADAPTER *prAdapter,
		IN struct WIFI_CMD_INFO *prCmdInfo,
		OUT uint16_t *pu2BufInfoLen, OUT uint8_t *pucSeqNum,
		OUT void **pCmdBuf);
	void (*asicFillCmdTxd)(IN struct ADAPTER *prAdapter,
		IN struct WIFI_CMD_INFO *prCmdInfo,
		OUT uint8_t *pucSeqNum, OUT void **pCmdBuf);
	uint32_t (*asicGetChipID)(IN struct ADAPTER *prAdapter);
	void (*fillHifTxDesc)(IN uint8_t **pDest, IN uint16_t *pInfoBufLen);
	uint32_t (*downloadBufferBin)(IN struct ADAPTER *prAdapter);
	void (*asicRxProcessRxvforMSP)(IN struct ADAPTER *prAdapter,
		IN OUT struct SW_RFB *prRetSwRfb);
	uint8_t (*asicRxGetRcpiValueFromRxv)(
		IN uint8_t ucRcpiMode,
		IN struct SW_RFB *prSwRfb);
	void (*asicRxPerfIndProcessRXV)(
		IN struct ADAPTER *prAdapter,
		IN struct SW_RFB *prSwRfb,
		IN uint8_t ucBssIndex);

	const uint32_t features;	/* feature bits */
	u_int8_t is_support_hw_amsdu;
	uint8_t ucMaxSwAmsduNum;
	uint8_t ucMaxSwapAntenna;
	uint32_t workAround;
	char *prTxPwrLimitFile;
	uint8_t ucTxPwrLimitBatchSize;
	u_int8_t is_support_asic_lp;
	u_int8_t is_support_wfdma1;
	u_int8_t is_support_dma_shdl;
	u_int8_t rx_event_port;
#if defined(_HIF_USB)
	void (*asicUsbInit)(IN struct ADAPTER *prAdapter,
			    IN struct mt66xx_chip_info *prChipInfo);
	void (*asicUsbInit_ic_specific)(IN struct ADAPTER *prAdapter,
			    IN struct mt66xx_chip_info *prChipInfo);
	uint32_t u4SerUsbMcuEventAddr;
	uint32_t u4SerUsbHostAckAddr;
#endif
	void (*asicDumpSerDummyCR)(IN struct ADAPTER *prAdapter);
	void (*asicWfdmaReInit)(IN struct ADAPTER *prAdapter);
	void (*asicWfdmaReInit_handshakeInit)(IN struct ADAPTER *prAdapter);
	void *pdev;
	uint32_t group5_size;
	void (*wlanCheckAsicCap)(IN struct ADAPTER *prAdapter);
#if (CFG_CHIP_RESET_SUPPORT == 1) && (CFG_WMT_RESET_API_SUPPORT == 0)
	u_int8_t (*rst_L0_notify_step2)(void);
#endif
	uint32_t u4LmacWtblDUAddr;
	uint32_t u4UmacWtblDUAddr;
	int (*wmmcupwron)(void);
	int (*wmmcupwroff)(void);
	uint32_t (*pwrondownload)(IN struct ADAPTER *prAdapter,
		IN uint8_t ucDownloadItem);
	int (*triggerfwassert)(void);
	int (*coexpccifon)(void);
	int (*coexpccifoff)(void);
	void (*coantSetWiFi)(void);
	void (*coantSetMD)(void);
	void (*coantVFE28En)(IN struct ADAPTER *prAdapter);
	void (*coantVFE28Dis)(void);
	int (*trigger_wholechiprst)(char *reason);
	void (*sw_interrupt_handler)(IN struct ADAPTER *prAdapter);
	void (*conninra_cb_register)(void);
	void (*dumpwfsyscpupcr)(IN struct ADAPTER *prAdapter);
	uint8_t* (*getCalResult)(OUT uint32_t *prCalSize);
	void (*calDebugCmd)(uint32_t cmd, uint32_t para);
	u_int8_t is_support_nvram_fragment;
	int (*checkbushang)(void *prAdapter,
		uint8_t ucWfResetEnable);
	void (*dumpBusHangCr)(IN struct ADAPTER *prAdapter);
};

struct mt66xx_hif_driver_data {
	struct mt66xx_chip_info *chip_info;
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#endif
