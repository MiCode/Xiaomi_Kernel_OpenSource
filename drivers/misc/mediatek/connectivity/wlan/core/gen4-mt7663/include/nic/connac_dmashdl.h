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
/*! \file   "connac_dmashdl.h"
 *  \brief  The common register definition of MT6630
 *
 *   N/A
 */



#ifndef _CONNAC_DMASHDL_H
#define _CONNAC_DMASHDL_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */


/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
#define PCIE_HIF_DMASHDL_BASE                  0x6000
#endif /* _HIF_PCIE */

#if defined(_HIF_USB)
#define USB_HIF_DMASHDL_BASE                   0x5000A000
#endif /* _HIF_USB */

#define CONN_HIF_DMASHDL_SW_CONTROL(_base)                (_base + 0x04)
#define CONN_HIF_DMASHDL_OPTIONAL_CONTROL(_base)          (_base + 0x08)

#define CONN_HIF_DMASHDL_PAGE_SETTING(_base)              (_base + 0x0C)
/* User program group sequence type control */
/* 0: user program group sequence order type,
 * 1: pre define each slot group strict order
 */
#define CONN_HIF_DMASHDL_GROUP_SEQ_ORDER_TYPE   BIT(16)

#define CONN_HIF_DMASHDL_REFILL_CONTROL(_base)            (_base + 0x10)
#define CONN_HIF_DMASHDL_TX_PACKET_SIZE_PAGE_MAP(_base)   (_base + 0x14)

#define CONN_HIF_DMASHDL_CONTROL_SIGNAL(_base)            (_base + 0x18)
/* enable to clear the flag of PLE TXD size greater than ple max. size */
#define CONN_HIF_DMASHDL_PLE_TXD_GT_MAX_SIZE_FLAG_CLR \
	BIT(0)
#define CONN_HIF_DMASHDL_HIF_ASK_SUB_ENA \
	BIT(16) /* enable packet in substration action from HIF ask period */
#define CONN_HIF_DMASHDL_PLE_SUB_ENA \
	BIT(17) /* enable packet in substration action from PLE */
/* enable terminate refill period when PLE release packet to do addition */
#define CONN_HIF_DMASHDL_PLE_ADD_INT_REFILL_ENA \
	BIT(29)
/* enable terminate refill period when packet in to do addition */
#define CONN_HIF_DMASHDL_PDMA_ADD_INT_REFILL_ENA \
	BIT(30)
/* enable terminate refill period when packet in to do substration */
#define CONN_HIF_DMASHDL_PKTIN_INT_REFILL_ENA \
	BIT(31)

#define CONN_HIF_DMASHDL_PACKET_MAX_SIZE(_base)           (_base + 0x1C)
#define CONN_HIF_DMASHDL_GROUP0_CONTROL(_base)            (_base + 0x20)
#define CONN_HIF_DMASHDL_REFILL_CTRL(_base)	              (_base + 0x10)
#define CONN_HIF_DMASHDL_GROUP0_CTRL(_base)               (_base + 0x20)
#define CONN_HIF_DMASHDL_GROUP1_CTRL(_base)               (_base + 0x24)
#define CONN_HIF_DMASHDL_GROUP2_CTRL(_base)               (_base + 0x28)
#define CONN_HIF_DMASHDL_GROUP3_CTRL(_base)               (_base + 0x2c)
#define CONN_HIF_DMASHDL_GROUP4_CTRL(_base)               (_base + 0x30)
#define CONN_HIF_DMASHDL_GROUP5_CTRL(_base)               (_base + 0x34)
#define CONN_HIF_DMASHDL_GROUP6_CTRL(_base)               (_base + 0x38)
#define CONN_HIF_DMASHDL_GROUP7_CTRL(_base)               (_base + 0x3c)
#define CONN_HIF_DMASHDL_GROUP8_CTRL(_base)               (_base + 0x40)
#define CONN_HIF_DMASHDL_GROUP9_CTRL(_base)               (_base + 0x44)
#define CONN_HIF_DMASHDL_GROUP10_CTRL(_base)              (_base + 0x48)
#define CONN_HIF_DMASHDL_GROUP11_CTRL(_base)              (_base + 0x4c)
#define CONN_HIF_DMASHDL_GROUP12_CTRL(_base)              (_base + 0x50)
#define CONN_HIF_DMASHDL_GROUP13_CTRL(_base)              (_base + 0x54)
#define CONN_HIF_DMASHDL_GROUP14_CTRL(_base)              (_base + 0x58)
#define CONN_HIF_DMASHDL_GROUP15_CTRL(_base)              (_base + 0x5c)
#define CONN_HIF_DMASHDL_SHDL_SET0(_base)                 (_base + 0xb0)
#define CONN_HIF_DMASHDL_SHDL_SET1(_base)                 (_base + 0xb4)
#define CONN_HIF_DMASHDL_SLOT_SET0(_base)                 (_base + 0xc4)
#define CONN_HIF_DMASHDL_SLOT_SET1(_base)                 (_base + 0xc8)
#define CONN_HIF_DMASHDL_Q_MAP0(_base)                    (_base + 0xd0)
#define CONN_HIF_DMASHDL_Q_MAP1(_base)                    (_base + 0xd4)
#define CONN_HIF_DMASHDL_Q_MAP2(_base)                    (_base + 0xd8)
#define CONN_HIF_DMASHDL_Q_MAP3(_base)                    (_base + 0xdc)

#define CONN_HIF_DMASHDL_ERROR_FLAG_CTRL(_base)           (_base + 0x9c)

#define CONN_HIF_DMASHDL_STATUS_RD(_base)                 (_base + 0x100)
#define CONN_HIF_DMASHDL_STATUS_RD_GP0(_base)             (_base + 0x140)
#define CONN_HIF_DMASHDL_STATUS_RD_GP1(_base)             (_base + 0x144)
#define CONN_HIF_DMASHDL_STATUS_RD_GP2(_base)             (_base + 0x148)
#define CONN_HIF_DMASHDL_STATUS_RD_GP3(_base)             (_base + 0x14c)
#define CONN_HIF_DMASHDL_STATUS_RD_GP4(_base)             (_base + 0x150)
#define CONN_HIF_DMASHDL_STATUS_RD_GP5(_base)             (_base + 0x154)
#define CONN_HIF_DMASHDL_STATUS_RD_GP6(_base)             (_base + 0x158)
#define CONN_HIF_DMASHDL_STATUS_RD_GP7(_base)             (_base + 0x15c)
#define CONN_HIF_DMASHDL_STATUS_RD_GP8(_base)             (_base + 0x160)
#define CONN_HIF_DMASHDL_STATUS_RD_GP9(_base)             (_base + 0x164)
#define CONN_HIF_DMASHDL_STATUS_RD_GP10(_base)            (_base + 0x168)
#define CONN_HIF_DMASHDL_STATUS_RD_GP11(_base)            (_base + 0x16c)
#define CONN_HIF_DMASHDL_STATUS_RD_GP12(_base)            (_base + 0x170)
#define CONN_HIF_DMASHDL_STATUS_RD_GP13(_base)            (_base + 0x174)
#define CONN_HIF_DMASHDL_STATUS_RD_GP14(_base)            (_base + 0x178)
#define CONN_HIF_DMASHDL_STATUS_RD_GP15(_base)            (_base + 0x17c)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_0(_base)            (_base + 0x180)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_1(_base)            (_base + 0x184)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_2(_base)            (_base + 0x188)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_3(_base)            (_base + 0x18c)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_4(_base)            (_base + 0x190)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_5(_base)            (_base + 0x194)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_6(_base)            (_base + 0x198)
#define CONN_HIF_DMASHDLRD_GP_PKT_CNT_7(_base)            (_base + 0x19c)

/* ============================================================================
 *
 *  ---REFILL_CONTROL (0x5000A000 + 0x10)---
 *
 *    GROUP0_REFILL_DISABLE[16]    - (RW) Group0 refill control
 *    GROUP1_REFILL_DISABLE[17]    - (RW) Group1 refill control
 *    GROUP2_REFILL_DISABLE[18]    - (RW) Group2 refill control
 *    GROUP3_REFILL_DISABLE[19]    - (RW) Group3 refill control
 *    GROUP4_REFILL_DISABLE[20]    - (RW) Group4 refill control
 *    GROUP5_REFILL_DISABLE[21]    - (RW) Group5 refill control
 *    GROUP6_REFILL_DISABLE[22]    - (RW) Group6 refill control
 *    GROUP7_REFILL_DISABLE[23]    - (RW) Group7 refill control
 *    GROUP8_REFILL_DISABLE[24]    - (RW) Group8 refill control
 *    GROUP9_REFILL_DISABLE[25]    - (RW) Group9 refill control
 *    GROUP10_REFILL_DISABLE[26]   - (RW) Group10 refill control
 *    GROUP11_REFILL_DISABLE[27]   - (RW) Group11 refill control
 *    GROUP12_REFILL_DISABLE[28]   - (RW) Group12 refill control
 *    GROUP13_REFILL_DISABLE[29]   - (RW) Group13 refill control
 *    GROUP14_REFILL_DISABLE[30]   - (RW) Group14 refill control
 *    GROUP15_REFILL_DISABLE[31]   - (RW) Group15 refill control
 *
 * ============================================================================
 */
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP15_REFILL_DISABLE_MASK \
	0x80000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP14_REFILL_DISABLE_MASK \
	0x40000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP13_REFILL_DISABLE_MASK \
	0x20000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP12_REFILL_DISABLE_MASK \
	0x10000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP11_REFILL_DISABLE_MASK \
	0x08000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP10_REFILL_DISABLE_MASK \
	0x04000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP9_REFILL_DISABLE_MASK \
	0x02000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP8_REFILL_DISABLE_MASK \
	0x01000000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP7_REFILL_DISABLE_MASK \
	0x00800000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP6_REFILL_DISABLE_MASK \
	0x00400000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP5_REFILL_DISABLE_MASK \
	0x00200000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP4_REFILL_DISABLE_MASK \
	0x00100000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP3_REFILL_DISABLE_MASK \
	0x00080000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP2_REFILL_DISABLE_MASK \
	0x00040000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP1_REFILL_DISABLE_MASK \
	0x00020000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP0_REFILL_DISABLE_MASK \
	0x00010000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP15_REFILL_PRIORITY_MASK \
	0x00008000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP14_REFILL_PRIORITY_MASK \
	0x00004000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP13_REFILL_PRIORITY_MASK \
	0x00002000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP12_REFILL_PRIORITY_MASK \
	0x00001000
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP11_REFILL_PRIORITY_MASK \
	0x00000800
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP10_REFILL_PRIORITY_MASK \
	0x00000400
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP9_REFILL_PRIORITY_MASK \
	0x00000200
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP8_REFILL_PRIORITY_MASK \
	0x00000100
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP7_REFILL_PRIORITY_MASK \
	0x00000080
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP6_REFILL_PRIORITY_MASK \
	0x00000040
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP5_REFILL_PRIORITY_MASK \
	0x00000020
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP4_REFILL_PRIORITY_MASK \
	0x00000010
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP3_REFILL_PRIORITY_MASK \
	0x00000008
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP2_REFILL_PRIORITY_MASK \
	0x00000004
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP1_REFILL_PRIORITY_MASK \
	0x00000002
#define CONN_HIF_DMASHDL_TOP_REFILL_CONTROL_GROUP0_REFILL_PRIORITY_MASK \
	0x00000001

/* ============================================================================
 *
 *  ---QUEUE_MAPPING0 (0x5000A000 + 0xd0)---
 *
 *    QUEUE0_MAPPING[3..0]         - (RW) queue 0 use which group ID
 *    QUEUE1_MAPPING[7..4]         - (RW) queue 1 use which group ID
 *    QUEUE2_MAPPING[11..8]        - (RW) queue 2 use which group ID
 *    QUEUE3_MAPPING[15..12]       - (RW) queue 3 use which group ID
 *    QUEUE4_MAPPING[19..16]       - (RW) queue 4 use which group ID
 *    QUEUE5_MAPPING[23..20]       - (RW) queue 5 use which group ID
 *    QUEUE6_MAPPING[27..24]       - (RW) queue 6 use which group ID
 *    QUEUE7_MAPPING[31..28]       - (RW) queue 7 use which group ID
 *
 * ============================================================================
 */
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE7_MAPPING 28
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE6_MAPPING 24
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE5_MAPPING 20
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE4_MAPPING 16
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE3_MAPPING 12
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE2_MAPPING 8
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE1_MAPPING 4
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING0_QUEUE0_MAPPING 0

/* ============================================================================
 *
 *  ---QUEUE_MAPPING1 (0x5000A000 + 0xd4)---
 *
 *    QUEUE8_MAPPING[3..0]         - (RW) queue 8 use which group ID
 *    QUEUE9_MAPPING[7..4]         - (RW) queue 9 use which group ID
 *    QUEUE10_MAPPING[11..8]       - (RW) queue 10 use which group ID
 *    QUEUE11_MAPPING[15..12]      - (RW) queue 11 use which group ID
 *    QUEUE12_MAPPING[19..16]      - (RW) queue 12 use which group ID
 *    QUEUE13_MAPPING[23..20]      - (RW) queue 13 use which group ID
 *    QUEUE14_MAPPING[27..24]      - (RW) queue 14 use which group ID
 *    QUEUE15_MAPPING[31..28]      - (RW) queue 15 use which group ID
 *
 * ============================================================================
 */
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE15_MAPPING 28
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE14_MAPPING 24
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE13_MAPPING 20
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE12_MAPPING 16
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE11_MAPPING 12
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE10_MAPPING 8
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE9_MAPPING 4
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING1_QUEUE8_MAPPING 0

/* ============================================================================
 *
 *  ---QUEUE_MAPPING2 (0x5000A000 + 0xd8)---
 *
 *    QUEUE16_MAPPING[3..0]        - (RW) queue 16 use which group ID
 *    QUEUE17_MAPPING[7..4]        - (RW) queue 17 use which group ID
 *    QUEUE18_MAPPING[11..8]       - (RW) queue 18 use which group ID
 *    QUEUE19_MAPPING[15..12]      - (RW) queue 19 use which group ID
 *    QUEUE20_MAPPING[19..16]      - (RW) queue 20 use which group ID
 *    QUEUE21_MAPPING[23..20]      - (RW) queue 21 use which group ID
 *    QUEUE22_MAPPING[27..24]      - (RW) queue 22 use which group ID
 *    QUEUE23_MAPPING[31..28]      - (RW) queue 23 use which group ID
 *
 * =============================================================================
 */
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE23_MAPPING 28
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE22_MAPPING 24
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE21_MAPPING 20
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE20_MAPPING 16
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE19_MAPPING 12
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE18_MAPPING 8
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE17_MAPPING 4
#define CONN_HIF_DMASHDL_TOP_QUEUE_MAPPING2_QUEUE16_MAPPING 0

#define DMASHDL_RSV_CNT_MASK           (0xfff << 16)
#define DMASHDL_SRC_CNT_MASK           (0xfff << 0)
#define DMASHDL_RSV_CNT_OFFSET         16
#define DMASHDL_SRC_CNT_OFFSET         0
#define DMASHDL_FREE_PG_CNT_MASK       (0xfff << 16)
#define DMASHDL_FFA_CNT_MASK           (0xfff << 0)
#define DMASHDL_FREE_PG_CNT_OFFSET     16
#define DMASHDL_FFA_CNT_OFFSET         0
#define DMASHDL_MAX_QUOTA_MASK         (0xfff << 16)
#define DMASHDL_MIN_QUOTA_MASK         (0xfff << 0)
#define DMASHDL_MAX_QUOTA_OFFSET       16
#define DMASHDL_MIN_QUOTA_OFFSET       0

#define DMASHDL_MIN_QUOTA_NUM(p) (((p) & 0xfff) << DMASHDL_MIN_QUOTA_OFFSET)
#define GET_DMASHDL_MIN_QUOTA_NUM(p) \
	(((p) & DMASHDL_MIN_QUOTA_MASK) >> DMASHDL_MIN_QUOTA_OFFSET)

#define DMASHDL_MAX_QUOTA_NUM(p) \
	(((p) & 0xfff) << DMASHDL_MAX_QUOTA_OFFSET)
#define GET_DMASHDL_MAX_QUOTA_NUM(p) \
	(((p) & DMASHDL_MAX_QUOTA_MASK) >> DMASHDL_MAX_QUOTA_OFFSET)

#define ODD_GROUP_ASK_CN_MASK (0xff << 16)
#define ODD_GROUP_ASK_CN_OFFSET 16
#define GET_ODD_GROUP_ASK_CNT(p) \
	(((p) & ODD_GROUP_ASK_CN_MASK) >> ODD_GROUP_ASK_CN_OFFSET)
#define ODD_GROUP_PKT_IN_CN_MASK (0xff << 24)
#define ODD_GROUP_PKT_IN_CN_OFFSET 24
#define GET_ODD_GROUP_PKT_IN_CNT(p) \
	(((p) & ODD_GROUP_PKT_IN_CN_MASK) >> ODD_GROUP_PKT_IN_CN_OFFSET)
#define EVEN_GROUP_ASK_CN_MASK (0xff << 0)
#define EVEN_GROUP_ASK_CN_OFFSET 0
#define GET_EVEN_GROUP_ASK_CNT(p) \
	(((p) & EVEN_GROUP_ASK_CN_MASK) >> EVEN_GROUP_ASK_CN_OFFSET)
#define EVEN_GROUP_PKT_IN_CN_MASK (0xff << 8)
#define EVEN_GROUP_PKT_IN_CN_OFFSET 8
#define GET_EVEN_GROUP_PKT_IN_CNT(p) \
	(((p) & EVEN_GROUP_PKT_IN_CN_MASK) >> EVEN_GROUP_PKT_IN_CN_OFFSET)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
/* DMASHDL_REFILL_CONTROL */
union _DMASHDL_REFILL_CONTROL {
	struct {
		uint32_t PRIORITY:16;
		uint32_t DISABLE:16;
	} field;

	uint32_t word;
};

/* DMASHDL_PACKET_MAX_SIZE */
union _DMASHDL_PACKET_MAX_SIZE {
	struct {
		uint32_t PLE_SIZE:12;
		uint32_t RSV_12_15:4;
		uint32_t PSE_SIZE:12;
		uint32_t RSV_28_31:4;
	} field;

	uint32_t word;
};

/* DMASHDL_GROUP_CONTROL */
union _DMASHDL_GROUP_CONTROL {
	struct {
		uint32_t MIN_QUOTAE:12;
		uint32_t RSV_12_15:4;
		uint32_t MAX_QUOTAE:12;
		uint32_t RSV_28_31:4;
	} field;

	uint32_t word;
};
#endif /* _HIF_PCIE */

/* Group index */
enum _ENUM_GROUP_INDEX_T {
	GROUP0_INDEX = 0,
	GROUP1_INDEX,
	GROUP2_INDEX,
	GROUP3_INDEX,
	GROUP4_INDEX,
	GROUP5_INDEX,
	GROUP6_INDEX,
	GROUP7_INDEX,
	GROUP8_INDEX,
	GROUP9_INDEX,
	GROUP10_INDEX,
	GROUP11_INDEX,
	GROUP12_INDEX,
	GROUP13_INDEX,
	GROUP14_INDEX,
	GROUP15_INDEX,
	MAX_GROUP_NUM
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
