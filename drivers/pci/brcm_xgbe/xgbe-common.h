/*
 * Broadcom BCM8956X / BCM8957X / BCM8989X 10Gb Ethernet driver
 *
 * Copyright (c) 2021 Broadcom. The term "Broadcom" refers solely to the 
 * Broadcom Inc. subsidiary that distributes the Licensed Product, as defined 
 * below.
 *
 * The following copyright statements and licenses apply to open source software 
 * ("OSS") distributed with the BCM8956X / BCM8957X / BCM8989X product (the "Licensed Product").
 * The Licensed Product does not necessarily use all the OSS referred to below and 
 * may also only use portions of a given OSS component. 
 *
 * To the extent required under an applicable open source license, Broadcom 
 * will make source code available for applicable OSS upon request. Please send 
 * an inquiry to opensource@broadcom.com including your name, address, the 
 * product name and version, operating system, and the place of purchase.   
 *
 * To the extent the Licensed Product includes OSS, the OSS is typically not 
 * owned by Broadcom. THE OSS IS PROVIDED AS IS WITHOUT WARRANTY OR CONDITION 
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  
 * To the full extent permitted under applicable law, Broadcom disclaims all 
 * warranties and liability arising from or related to any use of the OSS.
 *
 * To the extent the Licensed Product includes OSS licensed under the GNU 
 * General Public License ("GPL") or the GNU Lesser General Public License 
 * ("LGPL"), the use, copying, distribution and modification of the GPL OSS or 
 * LGPL OSS is governed, respectively, by the GPL or LGPL.  A copy of the GPL 
 * or LGPL license may be found with the applicable OSS.  Additionally, a copy 
 * of the GPL License or LGPL License can be found at 
 * https://www.gnu.org/licenses or obtained by writing to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 * 
 * This file is available to you under your choice of the following two 
 * licenses:
 *
 * License 1: GPLv2 License
 *
 * Copyright (c) 2021 Broadcom
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * License 2: Modified BSD License
 * 
 * Copyright (c) 2021 Broadcom
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * AMD 10Gb Ethernet driver
 *
 * This file is available to you under your choice of the following two
 * licenses:
 *
 * License 1: GPLv2
 *
 * Copyright (c) 2014-2016 Advanced Micro Devices, Inc.
 *
 * This file is free software; you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * License 2: Modified BSD
 *
 * Copyright (c) 2014-2016 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef __XGBE_COMMON_H__
#define __XGBE_COMMON_H__

#include "xgbe-usr-opt.h"

/*PCI register offsets */

#define PCI_DEV_ID_OFFSET 0x2
#define MBOX_REGS_OFFSET 0x1000
#define XGMAC_REGS_OFFSET 0x3000
#if XGBE_SRIOV_PF
#define XGMAC_ELI_REGS_OFFSET 0x9000
#define XGMAC_PHY_REGS_OFFSET_FROM_END 0x100
#define XGMAC_PHY_LINK_STATUS_REG 0xF23D024C
#define XGMAC_PHY_SPEED_ADVERTISE_REG0 0xF20E0404
#define XGMAC_PHY_SPEED_ADVERTISE_REG 0xF20E0406
#define XGMAC_PHY_AN_CTRL_REG 0xF20E0400
#define XGMAC_PHY_T1_BASE_SEL_REG 0xF2021068
#define XGMAC_PHY_IEEE_CTL1 0xF2020000
#endif
#define XGMAC_BAR_MASK 0x15

#if XGBE_SRIOV_PF
/* MISC CFG register offsets */
#define XGMAC_MISC_FUNC_RESOURCES_PF0 		0x804
#define XGMAC_MISC_FUNC_RESOURCES_VFn_BASE 	0x904
#define XGMAC_MISC_FUNC_RESOURCES_OFFSET 	0x100

#define XGMAC_PCIE_MISC_MAILBOX_INT_STA_PF0 	0x820
#define XGMAC_MISC_MAILBOX_INT_EN_PF0 		0x824

#define XGMAC_MISC_MSIX_VECTOR_MAP_MB_VF7   	0x814
#define XGMAC_MISC_MSIX_VECTOR_MAP_MB_BASE 	0x828
#define XGMAC_MISC_MSIX_VECTOR_MAP_RX_PF0_BASE 	0x840
#define XGMAC_MISC_MSIX_VECTOR_MAP_TX_PF0_BASE 	0x890
#define XGMAC_MISC_MSIX_VECTOR_MAP_OFFSET 	0x4

#define XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_LO 			0x90
#define XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_HI 			0x94
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_SBD_ALL 		0x740
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST0 		0x700
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST1 		0x704
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST_DOORBELL 	0x728
#define XGMAC_PCIE_MISC_EP2HST_DOORBELL_INTR_STA        	0x724
#define XGMAC_PCIE_MISC_MII_CTRL 				0x4
#define XGMAC_PCIE_MISC_SS_DBG_BUS_SEL 				0x20
#define XGMAC_PCIE_MISC_POWER_STATE 				0x30

#define XGMAC_REG1 0x200
#define XGMAC_REG2 0x300
#define XGMAC_REG3 0x400

#define XGMAC_PCIE_MISC_POWER_STATE_VF_DSTATE_INDEX		28
#define XGMAC_PCIE_MISC_POWER_STATE_VF_DSTATE_WIDTH		8
#define XGMAC_PCIE_MISC_POWER_STATE_NUM_FUNCTIONS_INDEX		3
#define XGMAC_PCIE_MISC_POWER_STATE_NUM_FUNCTIONS_WIDTH		4
#define XGMAC_PCIE_MISC_POWER_STATE_PF_DSTATE_INDEX		0
#define XGMAC_PCIE_MISC_POWER_STATE_PF_DSTATE_WIDTH		3

#endif

/* DMA register offsets */
#define DMA_MR				0x3000
#define DMA_SBMR			0x3004
#define DMA_ISR				0x3008
#define DMA_AXIARCR			0x3010
#define DMA_AXIAWCR			0x3018
#define DMA_AXIAWARCR			0x301c
#define DMA_DSR0			0x3020
#define DMA_DSR1			0x3024
#define DMA_TXEDMACR			0x3040
#define DMA_RXEDMACR			0x3044

/* DMA register entry bit positions and sizes */
#define DMA_ISR_MACIS_INDEX		17
#define DMA_ISR_MACIS_WIDTH		1
#define DMA_ISR_MTLIS_INDEX		16
#define DMA_ISR_MTLIS_WIDTH		1
#define DMA_MR_INTM_INDEX		12
#define DMA_MR_INTM_WIDTH		2
#define DMA_MR_SWR_INDEX		0
#define DMA_MR_SWR_WIDTH		1
#define DMA_RXEDMACR_RDPS_INDEX		0
#define DMA_RXEDMACR_RDPS_WIDTH		3
#define DMA_SBMR_AAL_INDEX		12
#define DMA_SBMR_AAL_WIDTH		1
#define DMA_SBMR_EAME_INDEX		11
#define DMA_SBMR_EAME_WIDTH		1
#define DMA_SBMR_BLEN_INDEX		1
#define DMA_SBMR_BLEN_WIDTH		7
#define DMA_SBMR_RD_OSR_LMT_INDEX	16
#define DMA_SBMR_RD_OSR_LMT_WIDTH	6
#define DMA_SBMR_UNDEF_INDEX		0
#define DMA_SBMR_UNDEF_WIDTH		1
#define DMA_SBMR_WR_OSR_LMT_INDEX	24
#define DMA_SBMR_WR_OSR_LMT_WIDTH	6
#define DMA_TXEDMACR_TDPS_INDEX		0
#define DMA_TXEDMACR_TDPS_WIDTH		3

/* DMA register values */
#define DMA_SBMR_BLEN_256		256
#define DMA_SBMR_BLEN_128		128
#define DMA_SBMR_BLEN_64		64
#define DMA_SBMR_BLEN_32		32
#define DMA_SBMR_BLEN_16		16
#define DMA_SBMR_BLEN_8			8
#define DMA_SBMR_BLEN_4			4
#define DMA_DSR_RPS_WIDTH		4
#define DMA_DSR_TPS_WIDTH		4
#define DMA_DSR_Q_WIDTH			(DMA_DSR_RPS_WIDTH + DMA_DSR_TPS_WIDTH)
#define DMA_DSR0_RPS_START		8
#define DMA_DSR0_TPS_START		12
#define DMA_DSRX_FIRST_QUEUE		3
#define DMA_DSRX_INC			4
#define DMA_DSRX_QPR			4
#define DMA_DSRX_RPS_START		0
#define DMA_DSRX_TPS_START		4
#define DMA_TPS_STOPPED			0x00
#define DMA_TPS_SUSPENDED		0x06

/* DMA channel register offsets
 *   Multiple channels can be active.  The first channel has registers
 *   that begin at 0x3100.  Each subsequent channel has registers that
 *   are accessed using an offset of 0x80 from the previous channel.
 */
#define DMA_CH_BASE			0x3100
#define DMA_CH_INC			0x80

#define DMA_CH_CR			0x00
#define DMA_CH_TCR			0x04
#define DMA_CH_RCR			0x08
#define DMA_CH_TDLR_HI			0x10
#define DMA_CH_TDLR_LO			0x14
#define DMA_CH_RDLR_HI			0x18
#define DMA_CH_RDLR_LO			0x1c
#define DMA_CH_TDTR_LO			0x24
#define DMA_CH_RDTR_LO			0x2c
#define DMA_CH_TDRLR			0x30
#define DMA_CH_RDRLR			0x34
#define DMA_CH_IER			0x38
#define DMA_CH_RIWT			0x3c
#define DMA_CH_CATDR_LO			0x44
#define DMA_CH_CARDR_LO			0x4c
#define DMA_CH_CATBR_HI			0x50
#define DMA_CH_CATBR_LO			0x54
#define DMA_CH_CARBR_HI			0x58
#define DMA_CH_CARBR_LO			0x5c
#define DMA_CH_SR			0x60

/* DMA channel register entry bit positions and sizes */
#define DMA_CH_CR_PBLX8_INDEX		16
#define DMA_CH_CR_PBLX8_WIDTH		1
#define DMA_CH_CR_SPH_INDEX		24
#define DMA_CH_CR_SPH_WIDTH		1
#define DMA_CH_IER_AIE20_INDEX		15
#define DMA_CH_IER_AIE20_WIDTH		1
#define DMA_CH_IER_AIE_INDEX		14
#define DMA_CH_IER_AIE_WIDTH		1
#define DMA_CH_IER_FBEE_INDEX		12
#define DMA_CH_IER_FBEE_WIDTH		1
#define DMA_CH_IER_NIE20_INDEX		16
#define DMA_CH_IER_NIE20_WIDTH		1
#define DMA_CH_IER_NIE_INDEX		15
#define DMA_CH_IER_NIE_WIDTH		1
#define DMA_CH_IER_RBUE_INDEX		7
#define DMA_CH_IER_RBUE_WIDTH		1
#define DMA_CH_IER_RIE_INDEX		6
#define DMA_CH_IER_RIE_WIDTH		1
#define DMA_CH_IER_RSE_INDEX		8
#define DMA_CH_IER_RSE_WIDTH		1
#define DMA_CH_IER_TBUE_INDEX		2
#define DMA_CH_IER_TBUE_WIDTH		1
#define DMA_CH_IER_TIE_INDEX		0
#define DMA_CH_IER_TIE_WIDTH		1
#define DMA_CH_IER_TXSE_INDEX		1
#define DMA_CH_IER_TXSE_WIDTH		1
#define DMA_CH_RCR_PBL_INDEX		16
#define DMA_CH_RCR_PBL_WIDTH		6
#define DMA_CH_RCR_RBSZ_INDEX		1
#define DMA_CH_RCR_RBSZ_WIDTH		14
#define DMA_CH_RCR_SR_INDEX		0
#define DMA_CH_RCR_SR_WIDTH		1
#define DMA_CH_RCR_RPF_INDEX		31
#define DMA_CH_RCR_RPF_WIDTH		1
#define DMA_CH_RIWT_RWT_INDEX		0
#define DMA_CH_RIWT_RWT_WIDTH		8
#define DMA_CH_RIWT_RWTU_INDEX		12
#define DMA_CH_RIWT_RWTU_WIDTH		2
#define DMA_CH_RIWT_RBCT_INDEX		16
#define DMA_CH_RIWT_RBCT_WIDTH		10
#define DMA_CH_RIWT_PSEL_INDEX		31
#define DMA_CH_RIWT_PSEL_WIDTH		1
#define DMA_CH_SR_FBE_INDEX		12
#define DMA_CH_SR_FBE_WIDTH		1
#define DMA_CH_SR_RBU_INDEX		7
#define DMA_CH_SR_RBU_WIDTH		1
#define DMA_CH_SR_RI_INDEX		6
#define DMA_CH_SR_RI_WIDTH		1
#define DMA_CH_SR_RPS_INDEX		8
#define DMA_CH_SR_RPS_WIDTH		1
#define DMA_CH_SR_TBU_INDEX		2
#define DMA_CH_SR_TBU_WIDTH		1
#define DMA_CH_SR_TI_INDEX		0
#define DMA_CH_SR_TI_WIDTH		1
#define DMA_CH_SR_TPS_INDEX		1
#define DMA_CH_SR_TPS_WIDTH		1
#define DMA_CH_TCR_OSP_INDEX		4
#define DMA_CH_TCR_OSP_WIDTH		1
#define DMA_CH_TCR_PBL_INDEX		16
#define DMA_CH_TCR_PBL_WIDTH		6
#define DMA_CH_TCR_ST_INDEX		0
#define DMA_CH_TCR_ST_WIDTH		1
#define DMA_CH_TCR_TSE_INDEX		12
#define DMA_CH_TCR_TSE_WIDTH		1

/* DMA channel register values */
#define DMA_OSP_DISABLE			0x00
#define DMA_OSP_ENABLE			0x01
#define DMA_PBL_1			1
#define DMA_PBL_2			2
#define DMA_PBL_4			4
#define DMA_PBL_8			8
#define DMA_PBL_16			16
#define DMA_PBL_32			32
#define DMA_PBL_64			64      /* 8 x 8 */
#define DMA_PBL_128			128     /* 8 x 16 */
#define DMA_PBL_256			256     /* 8 x 32 */
#define DMA_PBL_X8_DISABLE		0x00
#define DMA_PBL_X8_ENABLE		0x01

/* OSR limits */
#define RD_OSR_LIMIT_MIN		0
#define RD_OSR_LIMIT_MAX		64
#define RD_OSR_LIMIT_DEFAULT		12
#define WR_OSR_LIMIT_MIN		0
#define WR_OSR_LIMIT_MAX		64
#define WR_OSR_LIMIT_DEFAULT		3

/* Auto-negotiation */
#define AUTO_NEG_ENABLE 		1
#define AUTO_NEG_DISABLE 		0

/* MAC register offsets */
#define MAC_TCR				0x0000
#define MAC_RCR				0x0004
#define MAC_PFR				0x0008
#define MAC_WTR				0x000c
#define MAC_HTR0			0x0010
#define MAC_VLANTR			0x0050
#if XGBE_SRIOV_PF
#define MAC_VLANTDR			0x0054
#endif
#define MAC_VLANHTR			0x0058
#define MAC_VLANIR			0x0060
#define MAC_IVLANIR			0x0064
#define MAC_RETMR			0x006c
#define MAC_Q0TFCR			0x0070
#define MAC_RFCR			0x0090
#if XGBE_SRIOV_PF
#define MAC_RQC4R			0x0094
#define MAC_RQC5R			0x0098
#endif
#define MAC_RQC0R			0x00a0
#define MAC_RQC1R			0x00a4
#define MAC_RQC2R			0x00a8
#define MAC_RQC3R			0x00ac
#define MAC_ISR				0x00b0
#define MAC_IER				0x00b4
#define MAC_RTSR			0x00b8
#define MAC_PMTCSR			0x00c0
#define MAC_RWKPFR			0x00c4
#define MAC_LPICSR			0x00d0
#define MAC_LPITCR			0x00d4
#define MAC_TIR				0x00e0
#define MAC_VR				0x0110
#define MAC_DR				0x0114
#define MAC_HWF0R			0x011c
#define MAC_HWF1R			0x0120
#define MAC_HWF2R			0x0124
#if XGBE_SRIOV_PF
#define MAC_EXT_CFG			0x0140
#endif
#define MAC_MDIOSCAR			0x0200
#define MAC_MDIOSCCDR			0x0204
#define MAC_MDIOISR			0x0214
#define MAC_MDIOIER			0x0218
#define MAC_MDIOCL22R			0x0220
#define MAC_GPIOCR			0x0278
#define MAC_GPIOSR			0x027c
#define MAC_MACA0HR			0x0300
#define MAC_MACA0LR			0x0304
#define MAC_MACA1HR			0x0308
#define MAC_MACA1LR			0x030c
#define MAC_RSSCR			0x0c80
#define MAC_RSSAR			0x0c88
#define MAC_RSSDR			0x0c8c
#define MAC_TSCR			0x0d00
#define MAC_SSIR			0x0d04
#define MAC_STSR			0x0d08
#define MAC_STNR			0x0d0c
#define MAC_STSUR			0x0d10
#define MAC_STNUR			0x0d14
#define MAC_TSAR			0x0d18
#define MAC_TSSR			0x0d20
#define MAC_TXSNR			0x0d30
#if XGBE_SRIOV_VF
#define MAC_TXSSR			0x0d34
#endif
#define MAC_TXSSR			0x0d34
#define MAC_PTO			    	0x0dc0

#define MAC_L3L4ACTL                    0x0c00
#define MAC_L3L4WRR                     0x0c04


#if BRCM_FH
/* Flexible Header Registers offsets */
#define MAC_FLEX_HDR_CFG		0x0194
#define MAC_FLEX_HDR_LOW		0x0198
#define MAC_FLEX_HDR_HIGH		0x019C

#define BRCM_FH_PKT_PREFIX_LEN		8
#define BRCM_FH_LEN			4
#endif

#define MAC_QTFCR_INC			4
#define MAC_MACA_INC			4
#define MAC_HTR_INC			4

#define MAC_RQC2_INC			4
#define MAC_RQC2_Q_PER_REG		4

/* MAC register entry bit positions and sizes */
#define MAC_PFR_IPFE_INDEX		20
#define MAC_PFR_IPFE_WIDTH		1
#define MAC_HWF0R_ADDMACADRSEL_INDEX	18
#define MAC_HWF0R_ADDMACADRSEL_WIDTH	5
#define MAC_HWF0R_ARPOFFSEL_INDEX	9
#define MAC_HWF0R_ARPOFFSEL_WIDTH	1
#define MAC_HWF0R_EEESEL_INDEX		13
#define MAC_HWF0R_EEESEL_WIDTH		1
#define MAC_HWF0R_GMIISEL_INDEX		1
#define MAC_HWF0R_GMIISEL_WIDTH		1
#define MAC_HWF0R_MGKSEL_INDEX		7
#define MAC_HWF0R_MGKSEL_WIDTH		1
#define MAC_HWF0R_MMCSEL_INDEX		8
#define MAC_HWF0R_MMCSEL_WIDTH		1
#define MAC_HWF0R_RWKSEL_INDEX		6
#define MAC_HWF0R_RWKSEL_WIDTH		1
#define MAC_HWF0R_RXCOESEL_INDEX	16
#define MAC_HWF0R_RXCOESEL_WIDTH	1
#define MAC_HWF0R_SAVLANINS_INDEX	27
#define MAC_HWF0R_SAVLANINS_WIDTH	1
#define MAC_HWF0R_SMASEL_INDEX		5
#define MAC_HWF0R_SMASEL_WIDTH		1
#define MAC_HWF0R_TSSEL_INDEX		12
#define MAC_HWF0R_TSSEL_WIDTH		1
#define MAC_HWF0R_TSSTSSEL_INDEX	25
#define MAC_HWF0R_TSSTSSEL_WIDTH	2
#define MAC_HWF0R_TXCOESEL_INDEX	14
#define MAC_HWF0R_TXCOESEL_WIDTH	1
#define MAC_HWF0R_VLHASH_INDEX		4
#define MAC_HWF0R_VLHASH_WIDTH		1
#define MAC_HWF0R_VXN_INDEX		29
#define MAC_HWF0R_VXN_WIDTH		1
#define MAC_HWF1R_ADDR64_INDEX		14
#define MAC_HWF1R_ADDR64_WIDTH		2
#define MAC_HWF1R_ADVTHWORD_INDEX	13
#define MAC_HWF1R_ADVTHWORD_WIDTH	1
#define MAC_HWF1R_DBGMEMA_INDEX		19
#define MAC_HWF1R_DBGMEMA_WIDTH		1
#define MAC_HWF1R_DCBEN_INDEX		16
#define MAC_HWF1R_DCBEN_WIDTH		1
#define MAC_HWF1R_HASHTBLSZ_INDEX	24
#define MAC_HWF1R_HASHTBLSZ_WIDTH	3
#define MAC_HWF1R_L3L4FNUM_INDEX	27
#define MAC_HWF1R_L3L4FNUM_WIDTH	4
#define MAC_HWF1R_NUMTC_INDEX		21
#define MAC_HWF1R_NUMTC_WIDTH		3
#define MAC_HWF1R_RSSEN_INDEX		20
#define MAC_HWF1R_RSSEN_WIDTH		1
#define MAC_HWF1R_RXFIFOSIZE_INDEX	0
#define MAC_HWF1R_RXFIFOSIZE_WIDTH	5
#define MAC_HWF1R_SPHEN_INDEX		17
#define MAC_HWF1R_SPHEN_WIDTH		1
#define MAC_HWF1R_TSOEN_INDEX		18
#define MAC_HWF1R_TSOEN_WIDTH		1
#define MAC_HWF1R_TXFIFOSIZE_INDEX	6
#define MAC_HWF1R_TXFIFOSIZE_WIDTH	5
#define MAC_HWF2R_AUXSNAPNUM_INDEX	28
#define MAC_HWF2R_AUXSNAPNUM_WIDTH	3
#define MAC_HWF2R_PPSOUTNUM_INDEX	24
#define MAC_HWF2R_PPSOUTNUM_WIDTH	3
#define MAC_HWF2R_RXCHCNT_INDEX		12
#define MAC_HWF2R_RXCHCNT_WIDTH		4
#define MAC_HWF2R_RXQCNT_INDEX		0
#define MAC_HWF2R_RXQCNT_WIDTH		4
#define MAC_HWF2R_TXCHCNT_INDEX		18
#define MAC_HWF2R_TXCHCNT_WIDTH		4
#define MAC_HWF2R_TXQCNT_INDEX		6
#define MAC_HWF2R_TXQCNT_WIDTH		4
#define MAC_IER_TSIE_INDEX		12
#define MAC_IER_TSIE_WIDTH		1
#define MAC_ISR_MMCRXIS_INDEX		9
#define MAC_ISR_MMCRXIS_WIDTH		1
#define MAC_ISR_MMCTXIS_INDEX		10
#define MAC_ISR_MMCTXIS_WIDTH		1
#define MAC_ISR_PMTIS_INDEX		4
#define MAC_ISR_PMTIS_WIDTH		1
#define MAC_ISR_SMI_INDEX		1
#define MAC_ISR_SMI_WIDTH		1
#define MAC_ISR_TSIS_INDEX		12
#define MAC_ISR_TSIS_WIDTH		1
#define MAC_MACA1HR_AE_INDEX		31
#define MAC_MACA1HR_AE_WIDTH		1
#if XGBE_SRIOV_PF
#define MAC_MACA1HR_DCS_INDEX   	16
#define MAC_MACA1HR_DCS_WIDTH   	4
#endif
#define MAC_MDIOIER_SNGLCOMPIE_INDEX	12
#define MAC_MDIOIER_SNGLCOMPIE_WIDTH	1
#define MAC_MDIOISR_SNGLCOMPINT_INDEX	12
#define MAC_MDIOISR_SNGLCOMPINT_WIDTH	1
#define MAC_MDIOSCAR_DA_INDEX		21
#define MAC_MDIOSCAR_DA_WIDTH		5
#define MAC_MDIOSCAR_PA_INDEX		16
#define MAC_MDIOSCAR_PA_WIDTH		5
#define MAC_MDIOSCAR_RA_INDEX		0
#define MAC_MDIOSCAR_RA_WIDTH		16
#define MAC_MDIOSCAR_REG_INDEX		0
#define MAC_MDIOSCAR_REG_WIDTH		21
#define MAC_MDIOSCCDR_BUSY_INDEX	22
#define MAC_MDIOSCCDR_BUSY_WIDTH	1
#define MAC_MDIOSCCDR_CMD_INDEX		16
#define MAC_MDIOSCCDR_CMD_WIDTH		2
#define MAC_MDIOSCCDR_CR_INDEX		19
#define MAC_MDIOSCCDR_CR_WIDTH		3
#define MAC_MDIOSCCDR_DATA_INDEX	0
#define MAC_MDIOSCCDR_DATA_WIDTH	16
#define MAC_MDIOSCCDR_SADDR_INDEX	18
#define MAC_MDIOSCCDR_SADDR_WIDTH	1
#if XGBE_SRIOV_PF
#define MAC_PFR_DBF_INDEX		5
#define MAC_PFR_DBF_WIDTH		1
#endif
#define MAC_PFR_HMC_INDEX		2
#define MAC_PFR_HMC_WIDTH		1
#define MAC_PFR_HPF_INDEX		10
#define MAC_PFR_HPF_WIDTH		1
#define MAC_PFR_HUC_INDEX		1
#define MAC_PFR_HUC_WIDTH		1
#define MAC_PFR_PM_INDEX		4
#define MAC_PFR_PM_WIDTH		1
#define MAC_PFR_PR_INDEX		0
#define MAC_PFR_PR_WIDTH		1
#if XGBE_SRIOV_PF
#define MAC_PFR_RA_INDEX        	31
#define MAC_PFR_RA_WIDTH        	1
#endif
#define MAC_PFR_VTFE_INDEX		16
#define MAC_PFR_VTFE_WIDTH		1
#define MAC_PFR_VUCC_INDEX		22
#define MAC_PFR_VUCC_WIDTH		1
#define MAC_PMTCSR_MGKPKTEN_INDEX	1
#define MAC_PMTCSR_MGKPKTEN_WIDTH	1
#define MAC_PMTCSR_PWRDWN_INDEX		0
#define MAC_PMTCSR_PWRDWN_WIDTH		1
#define MAC_PMTCSR_RWKFILTRST_INDEX	31
#define MAC_PMTCSR_RWKFILTRST_WIDTH	1
#define MAC_PMTCSR_RWKPKTEN_INDEX	2
#define MAC_PMTCSR_RWKPKTEN_WIDTH	1
#define MAC_Q0TFCR_PT_INDEX		16
#define MAC_Q0TFCR_PT_WIDTH		16
#define MAC_Q0TFCR_TFE_INDEX		1
#define MAC_Q0TFCR_TFE_WIDTH		1
#define MAC_RCR_ACS_INDEX		1
#define MAC_RCR_ACS_WIDTH		1
#define MAC_RCR_CST_INDEX		2
#define MAC_RCR_CST_WIDTH		1
#define MAC_RCR_DCRCC_INDEX		3
#define MAC_RCR_DCRCC_WIDTH		1
#if XGBE_SRIOV_PF
#define MAC_RCR_ELEN_INDEX      	30
#define MAC_RCR_ELEN_WIDTH      	1
#endif
#define MAC_RCR_HDSMS_INDEX		12
#define MAC_RCR_HDSMS_WIDTH		3
#define MAC_RCR_IPC_INDEX		9
#define MAC_RCR_IPC_WIDTH		1
#define MAC_RCR_JE_INDEX		8
#define MAC_RCR_JE_WIDTH		1
#define MAC_RCR_LM_INDEX		10
#define MAC_RCR_LM_WIDTH		1
#define MAC_RCR_RE_INDEX		0
#define MAC_RCR_RE_WIDTH		1
#define MAC_RFCR_PFCE_INDEX		8
#define MAC_RFCR_PFCE_WIDTH		1
#define MAC_RFCR_RFE_INDEX		0
#define MAC_RFCR_RFE_WIDTH		1
#define MAC_RFCR_UP_INDEX		1
#define MAC_RFCR_UP_WIDTH		1
#define MAC_RQC0R_RXQ0EN_INDEX		0
#define MAC_RQC0R_RXQ0EN_WIDTH		2
#if XGBE_SRIOV_PF
#define MAC_RQC1R_MCBCQ_INDEX    	8
#define MAC_RQC1R_MCBCQ_WIDTH    	4
#define MAC_RQC1R_MCBCQEN_INDEX  	15
#define MAC_RQC1R_MCBCQEN_WIDTH  	1
#define MAC_RQC1R_UPQ_INDEX     	0
#define MAC_RQC1R_UPQ_WIDTH     	4
#define MAC_RQC1R_DCBCPQ_INDEX		16
#define MAC_RQC1R_DCBCPQ_WIDTH		4
#define MAC_RQC1R_TACPQE_INDEX		23
#define MAC_RQC1R_TACPQE_WIDTH		1
#define MAC_RQC1R_PTPQ_INDEX		24
#define MAC_RQC1R_PTPQ_WIDTH		4
#define MAC_RQC1R_AVCPQ_INDEX		28
#define MAC_RQC1R_AVCPQ_WIDTH		4
#define MAC_RQC1R_TPQC_INDEX		21
#define MAC_RQC1R_TPQC_WIDTH		1
#define MAC_RQC5R_PRQSO_INDEX		0
#define MAC_RQC5R_PRQSO_WIDTH		4 
#endif
#define MAC_RSSAR_ADDRT_INDEX		2
#define MAC_RSSAR_ADDRT_WIDTH		1
#define MAC_RSSAR_CT_INDEX		1
#define MAC_RSSAR_CT_WIDTH		1
#define MAC_RSSAR_OB_INDEX		0
#define MAC_RSSAR_OB_WIDTH		1
#define MAC_RSSAR_RSSIA_INDEX		8
#define MAC_RSSAR_RSSIA_WIDTH		8
#define MAC_RSSCR_IP2TE_INDEX		1
#define MAC_RSSCR_IP2TE_WIDTH		1
#define MAC_RSSCR_RSSE_INDEX		0
#define MAC_RSSCR_RSSE_WIDTH		1
#define MAC_RSSCR_TCP4TE_INDEX		2
#define MAC_RSSCR_TCP4TE_WIDTH		1
#define MAC_RSSCR_UDP4TE_INDEX		3
#define MAC_RSSCR_UDP4TE_WIDTH		1
#define MAC_RSSDR_DMCH_INDEX		0
#define MAC_RSSDR_DMCH_WIDTH		4
#define MAC_SSIR_SNSINC_INDEX		8
#define MAC_SSIR_SNSINC_WIDTH		8
#define MAC_SSIR_SSINC_INDEX		16
#define MAC_SSIR_SSINC_WIDTH		8
#define MAC_TCR_SS_INDEX		29
#define MAC_TCR_SS_WIDTH		3
#define MAC_TCR_TE_INDEX		0
#define MAC_TCR_TE_WIDTH		1
#define MAC_TCR_VNE_INDEX		24
#define MAC_TCR_VNE_WIDTH		1
#define MAC_TCR_VNM_INDEX		25
#define MAC_TCR_VNM_WIDTH		1
#define MAC_TIR_TNID_INDEX		0
#define MAC_TIR_TNID_WIDTH		16
#define MAC_TSCR_AV8021ASMEN_INDEX	28
#define MAC_TSCR_AV8021ASMEN_WIDTH	1
#define MAC_TSCR_SNAPTYPSEL_INDEX	16
#define MAC_TSCR_SNAPTYPSEL_WIDTH	2
#define MAC_TSCR_TSADDREG_INDEX		5
#define MAC_TSCR_TSADDREG_WIDTH		1
#define MAC_TSCR_TSCFUPDT_INDEX		1
#define MAC_TSCR_TSCFUPDT_WIDTH		1
#define MAC_TSCR_TSCTRLSSR_INDEX	9
#define MAC_TSCR_TSCTRLSSR_WIDTH	1
#define MAC_TSCR_TSENA_INDEX		0
#define MAC_TSCR_TSENA_WIDTH		1
#define MAC_TSCR_TSENALL_INDEX		8
#define MAC_TSCR_TSENALL_WIDTH		1
#define MAC_TSCR_TSEVNTENA_INDEX	14
#define MAC_TSCR_TSEVNTENA_WIDTH	1
#define MAC_TSCR_TSINIT_INDEX		2
#define MAC_TSCR_TSINIT_WIDTH		1
#define MAC_TSCR_TSIPENA_INDEX		11
#define MAC_TSCR_TSIPENA_WIDTH		1
#define MAC_TSCR_TSIPV4ENA_INDEX	13
#define MAC_TSCR_TSIPV4ENA_WIDTH	1
#define MAC_TSCR_TSIPV6ENA_INDEX	12
#define MAC_TSCR_TSIPV6ENA_WIDTH	1
#define MAC_TSCR_TSMSTRENA_INDEX	15
#define MAC_TSCR_TSMSTRENA_WIDTH	1
#define MAC_TSCR_TSVER2ENA_INDEX	10
#define MAC_TSCR_TSVER2ENA_WIDTH	1
#define MAC_TSCR_TXTSSTSM_INDEX		24
#define MAC_TSCR_TXTSSTSM_WIDTH		1
#define MAC_TSSR_TXTSC_INDEX		15
#define MAC_TSSR_TXTSC_WIDTH		1
#define MAC_TXSNR_TXTSSTSMIS_INDEX	31
#define MAC_TXSNR_TXTSSTSMIS_WIDTH	1
#define MAC_VLANHTR_VLHT_INDEX		0
#define MAC_VLANHTR_VLHT_WIDTH		16
#define MAC_VLANIR_VLTI_INDEX		20
#define MAC_VLANIR_VLTI_WIDTH		1
#define MAC_VLANIR_CSVL_INDEX		19
#define MAC_VLANIR_CSVL_WIDTH		1
#define MAC_VLANTR_DOVLTC_INDEX		20
#define MAC_VLANTR_DOVLTC_WIDTH		1
#define MAC_VLANTR_ERSVLM_INDEX		19
#define MAC_VLANTR_ERSVLM_WIDTH		1
#define MAC_VLANTR_ESVL_INDEX		18
#define MAC_VLANTR_ESVL_WIDTH		1
#define MAC_VLANTR_ETV_INDEX		16
#define MAC_VLANTR_ETV_WIDTH		1
#define MAC_VLANTR_EVLS_INDEX		21
#define MAC_VLANTR_EVLS_WIDTH		2
#define MAC_VLANTR_EVLRXS_INDEX		24
#define MAC_VLANTR_EVLRXS_WIDTH		1
#if XGBE_SRIOV_PF
#define MAC_VLANTR_OFS_INDEX  		2
#define MAC_VLANTR_OFS_WIDTH  		5
#define MAC_VLANTR_CT_INDEX 		1
#define MAC_VLANTR_CT_WIDTH 		1
#define MAC_VLANTR_OB_INDEX 		0
#define MAC_VLANTR_OB_WIDTH 		1
#endif
#define MAC_VLANTR_VL_INDEX		0
#define MAC_VLANTR_VL_WIDTH		16
#define MAC_VLANTR_VTHM_INDEX		25
#define MAC_VLANTR_VTHM_WIDTH		1
#define MAC_VLANTR_VTIM_INDEX		17
#define MAC_VLANTR_VTIM_WIDTH		1
#if XGBE_SRIOV_PF
#define MAC_VLANTDR_DMACHN_INDEX 	25
#define MAC_VLANTDR_DMACHN_WIDTH 	4
#define MAC_VLANTDR_DMACHEN_INDEX 	24
#define MAC_VLANTDR_DMACHEN_WIDTH 	1
#define MAC_VLANTDR_DOVLTC_INDEX  	18
#define MAC_VLANTDR_DOVLTC_WIDTH  	1
#define MAC_VLANTDR_ERIVLT_INDEX 	20
#define MAC_VLANTDR_ERIVLT_WIDTH 	1
#define MAC_VLANTDR_ERSVLM_INDEX  	19
#define MAC_VLANTDR_ERSVLM_WIDTH  	1
#define MAC_VLANTDR_ETV_INDEX 		17
#define MAC_VLANTDR_ETV_WIDTH 		1
#define MAC_VLANTDR_VEN_INDEX 		16
#define MAC_VLANTDR_VEN_WIDTH 		1
#define MAC_VLANTDR_VID_INDEX 		0
#define MAC_VLANTDR_VID_WIDTH 		16
#endif
#define MAC_VR_DEVID_INDEX		8
#define MAC_VR_DEVID_WIDTH		8
#define MAC_VR_SNPSVER_INDEX		0
#define MAC_VR_SNPSVER_WIDTH		8
#define MAC_VR_USERVER_INDEX		16
#define MAC_VR_USERVER_WIDTH		8

#if BRCM_FH
#define MAC_FLEX_HDR_CFG_FHRX_INDEX	0
#define MAC_FLEX_HDR_CFG_FHRX_WIDTH	1
#define MAC_FLEX_HDR_CFG_FHTX_INDEX	1
#define MAC_FLEX_HDR_CFG_FHTX_WIDTH	1
#define MAC_FLEX_HDR_CFG_EFLL_INDEX	2
#define MAC_FLEX_HDR_CFG_EFLL_WIDTH	1
#define MAC_FLEX_HDR_CFG_FHSP_INDEX	8
#define MAC_FLEX_HDR_CFG_FHSP_WIDTH	5
#define MAC_FLEX_HDR_CFG_FHL_INDEX	24
#define MAC_FLEX_HDR_CFG_FHL_WIDTH	3
#define MAC_FLEX_HDR_LOW_SFHL_INDEX	0
#define MAC_FLEX_HDR_LOW_SFHL_WIDTH	32
#define MAC_FLEX_HDR_HIGH_SFHU_INDEX	0
#define MAC_FLEX_HDR_HIGH_SFHU_WIDTH	32
#endif

/*pto reg fields */
#define MAC_PTO_PTOEN_WIDTH     	1
#define MAC_PTO_PTOEN_INDEX	    	0
#define MAC_PTO_ASYNCEN_WIDTH		1
#define MAC_PTO_ASYNCEN_INDEX		1
#define MAC_PTO_APDREQEN_WIDTH		1
#define MAC_PTO_APDREQEN_INDEX     	2
#define MAC_PTO_ASYNCTRIG_WIDTH		1
#define MAC_PTO_ASYNCTRIG_INDEX		4
#define MAC_PTO_APDREQTRIG_WIDTH	1
#define MAC_PTO_APDREQTRIG_INDEX	5
#define MAC_PTO_DRRDIS_WIDTH		1
#define MAC_PTO_DRRDIS_INDEX		6
#define MAC_PTO_DN_WIDTH		8
#define MAC_PTO_DN_INDEX		8

/* L3L4 register index and positions */
#define MAC_L3L4ACTL_IDDR_NUM_WIDTH             4
#define MAC_L3L4ACTL_IDDR_NUM_INDEX             12
#define MAC_L3L4ACTL_IDDR_REGSEL_WIDTH          4
#define MAC_L3L4ACTL_IDDR_REGSEL_INDEX          8
#define MAC_L3L4ACTL_TT_WIDTH           	1
#define MAC_L3L4ACTL_TT_INDEX           	1
#define MAC_L3L4ACTL_XB_WIDTH           	1
#define MAC_L3L4ACTL_XB_INDEX           	0


/* MMC register offsets */
#define MMC_CR				0x0800
#define MMC_RISR			0x0804
#define MMC_TISR			0x0808
#define MMC_RIER			0x080c
#define MMC_TIER			0x0810
#define MMC_TXOCTETCOUNT_GB_LO		0x0814
#define MMC_TXOCTETCOUNT_GB_HI		0x0818
#define MMC_TXFRAMECOUNT_GB_LO		0x081c
#define MMC_TXFRAMECOUNT_GB_HI		0x0820
#define MMC_TXBROADCASTFRAMES_G_LO	0x0824
#define MMC_TXBROADCASTFRAMES_G_HI	0x0828
#define MMC_TXMULTICASTFRAMES_G_LO	0x082c
#define MMC_TXMULTICASTFRAMES_G_HI	0x0830
#define MMC_TX64OCTETS_GB_LO		0x0834
#define MMC_TX64OCTETS_GB_HI		0x0838
#define MMC_TX65TO127OCTETS_GB_LO	0x083c
#define MMC_TX65TO127OCTETS_GB_HI	0x0840
#define MMC_TX128TO255OCTETS_GB_LO	0x0844
#define MMC_TX128TO255OCTETS_GB_HI	0x0848
#define MMC_TX256TO511OCTETS_GB_LO	0x084c
#define MMC_TX256TO511OCTETS_GB_HI	0x0850
#define MMC_TX512TO1023OCTETS_GB_LO	0x0854
#define MMC_TX512TO1023OCTETS_GB_HI	0x0858
#define MMC_TX1024TOMAXOCTETS_GB_LO	0x085c
#define MMC_TX1024TOMAXOCTETS_GB_HI	0x0860
#define MMC_TXUNICASTFRAMES_GB_LO	0x0864
#define MMC_TXUNICASTFRAMES_GB_HI	0x0868
#define MMC_TXMULTICASTFRAMES_GB_LO	0x086c
#define MMC_TXMULTICASTFRAMES_GB_HI	0x0870
#define MMC_TXBROADCASTFRAMES_GB_LO	0x0874
#define MMC_TXBROADCASTFRAMES_GB_HI	0x0878
#define MMC_TXUNDERFLOWERROR_LO		0x087c
#define MMC_TXUNDERFLOWERROR_HI		0x0880
#define MMC_TXOCTETCOUNT_G_LO		0x0884
#define MMC_TXOCTETCOUNT_G_HI		0x0888
#define MMC_TXFRAMECOUNT_G_LO		0x088c
#define MMC_TXFRAMECOUNT_G_HI		0x0890
#define MMC_TXPAUSEFRAMES_LO		0x0894
#define MMC_TXPAUSEFRAMES_HI		0x0898
#define MMC_TXVLANFRAMES_G_LO		0x089c
#define MMC_TXVLANFRAMES_G_HI		0x08a0
#define MMC_RXFRAMECOUNT_GB_LO		0x0900
#define MMC_RXFRAMECOUNT_GB_HI		0x0904
#define MMC_RXOCTETCOUNT_GB_LO		0x0908
#define MMC_RXOCTETCOUNT_GB_HI		0x090c
#define MMC_RXOCTETCOUNT_G_LO		0x0910
#define MMC_RXOCTETCOUNT_G_HI		0x0914
#define MMC_RXBROADCASTFRAMES_G_LO	0x0918
#define MMC_RXBROADCASTFRAMES_G_HI	0x091c
#define MMC_RXMULTICASTFRAMES_G_LO	0x0920
#define MMC_RXMULTICASTFRAMES_G_HI	0x0924
#define MMC_RXCRCERROR_LO		0x0928
#define MMC_RXCRCERROR_HI		0x092c
#define MMC_RXRUNTERROR			0x0930
#define MMC_RXJABBERERROR		0x0934
#define MMC_RXUNDERSIZE_G		0x0938
#define MMC_RXOVERSIZE_G		0x093c
#define MMC_RX64OCTETS_GB_LO		0x0940
#define MMC_RX64OCTETS_GB_HI		0x0944
#define MMC_RX65TO127OCTETS_GB_LO	0x0948
#define MMC_RX65TO127OCTETS_GB_HI	0x094c
#define MMC_RX128TO255OCTETS_GB_LO	0x0950
#define MMC_RX128TO255OCTETS_GB_HI	0x0954
#define MMC_RX256TO511OCTETS_GB_LO	0x0958
#define MMC_RX256TO511OCTETS_GB_HI	0x095c
#define MMC_RX512TO1023OCTETS_GB_LO	0x0960
#define MMC_RX512TO1023OCTETS_GB_HI	0x0964
#define MMC_RX1024TOMAXOCTETS_GB_LO	0x0968
#define MMC_RX1024TOMAXOCTETS_GB_HI	0x096c
#define MMC_RXUNICASTFRAMES_G_LO	0x0970
#define MMC_RXUNICASTFRAMES_G_HI	0x0974
#define MMC_RXLENGTHERROR_LO		0x0978
#define MMC_RXLENGTHERROR_HI		0x097c
#define MMC_RXOUTOFRANGETYPE_LO		0x0980
#define MMC_RXOUTOFRANGETYPE_HI		0x0984
#define MMC_RXPAUSEFRAMES_LO		0x0988
#define MMC_RXPAUSEFRAMES_HI		0x098c
#define MMC_RXFIFOOVERFLOW_LO		0x0990
#define MMC_RXFIFOOVERFLOW_HI		0x0994
#define MMC_RXVLANFRAMES_GB_LO		0x0998
#define MMC_RXVLANFRAMES_GB_HI		0x099c
#define MMC_RXWATCHDOGERROR		0x09a0

/* MMC register entry bit positions and sizes */
#define MMC_CR_CR_INDEX				0
#define MMC_CR_CR_WIDTH				1
#define MMC_CR_CSR_INDEX			1
#define MMC_CR_CSR_WIDTH			1
#define MMC_CR_ROR_INDEX			2
#define MMC_CR_ROR_WIDTH			1
#define MMC_CR_MCF_INDEX			3
#define MMC_CR_MCF_WIDTH			1
#define MMC_CR_MCT_INDEX			4
#define MMC_CR_MCT_WIDTH			2
#define MMC_RIER_ALL_INTERRUPTS_INDEX		0
#define MMC_RIER_ALL_INTERRUPTS_WIDTH		23
#define MMC_RISR_RXFRAMECOUNT_GB_INDEX		0
#define MMC_RISR_RXFRAMECOUNT_GB_WIDTH		1
#define MMC_RISR_RXOCTETCOUNT_GB_INDEX		1
#define MMC_RISR_RXOCTETCOUNT_GB_WIDTH		1
#define MMC_RISR_RXOCTETCOUNT_G_INDEX		2
#define MMC_RISR_RXOCTETCOUNT_G_WIDTH		1
#define MMC_RISR_RXBROADCASTFRAMES_G_INDEX	3
#define MMC_RISR_RXBROADCASTFRAMES_G_WIDTH	1
#define MMC_RISR_RXMULTICASTFRAMES_G_INDEX	4
#define MMC_RISR_RXMULTICASTFRAMES_G_WIDTH	1
#define MMC_RISR_RXCRCERROR_INDEX		5
#define MMC_RISR_RXCRCERROR_WIDTH		1
#define MMC_RISR_RXRUNTERROR_INDEX		6
#define MMC_RISR_RXRUNTERROR_WIDTH		1
#define MMC_RISR_RXJABBERERROR_INDEX		7
#define MMC_RISR_RXJABBERERROR_WIDTH		1
#define MMC_RISR_RXUNDERSIZE_G_INDEX		8
#define MMC_RISR_RXUNDERSIZE_G_WIDTH		1
#define MMC_RISR_RXOVERSIZE_G_INDEX		9
#define MMC_RISR_RXOVERSIZE_G_WIDTH		1
#define MMC_RISR_RX64OCTETS_GB_INDEX		10
#define MMC_RISR_RX64OCTETS_GB_WIDTH		1
#define MMC_RISR_RX65TO127OCTETS_GB_INDEX	11
#define MMC_RISR_RX65TO127OCTETS_GB_WIDTH	1
#define MMC_RISR_RX128TO255OCTETS_GB_INDEX	12
#define MMC_RISR_RX128TO255OCTETS_GB_WIDTH	1
#define MMC_RISR_RX256TO511OCTETS_GB_INDEX	13
#define MMC_RISR_RX256TO511OCTETS_GB_WIDTH	1
#define MMC_RISR_RX512TO1023OCTETS_GB_INDEX	14
#define MMC_RISR_RX512TO1023OCTETS_GB_WIDTH	1
#define MMC_RISR_RX1024TOMAXOCTETS_GB_INDEX	15
#define MMC_RISR_RX1024TOMAXOCTETS_GB_WIDTH	1
#define MMC_RISR_RXUNICASTFRAMES_G_INDEX	16
#define MMC_RISR_RXUNICASTFRAMES_G_WIDTH	1
#define MMC_RISR_RXLENGTHERROR_INDEX		17
#define MMC_RISR_RXLENGTHERROR_WIDTH		1
#define MMC_RISR_RXOUTOFRANGETYPE_INDEX		18
#define MMC_RISR_RXOUTOFRANGETYPE_WIDTH		1
#define MMC_RISR_RXPAUSEFRAMES_INDEX		19
#define MMC_RISR_RXPAUSEFRAMES_WIDTH		1
#define MMC_RISR_RXFIFOOVERFLOW_INDEX		20
#define MMC_RISR_RXFIFOOVERFLOW_WIDTH		1
#define MMC_RISR_RXVLANFRAMES_GB_INDEX		21
#define MMC_RISR_RXVLANFRAMES_GB_WIDTH		1
#define MMC_RISR_RXWATCHDOGERROR_INDEX		22
#define MMC_RISR_RXWATCHDOGERROR_WIDTH		1
#define MMC_TIER_ALL_INTERRUPTS_INDEX		0
#define MMC_TIER_ALL_INTERRUPTS_WIDTH		18
#define MMC_TISR_TXOCTETCOUNT_GB_INDEX		0
#define MMC_TISR_TXOCTETCOUNT_GB_WIDTH		1
#define MMC_TISR_TXFRAMECOUNT_GB_INDEX		1
#define MMC_TISR_TXFRAMECOUNT_GB_WIDTH		1
#define MMC_TISR_TXBROADCASTFRAMES_G_INDEX	2
#define MMC_TISR_TXBROADCASTFRAMES_G_WIDTH	1
#define MMC_TISR_TXMULTICASTFRAMES_G_INDEX	3
#define MMC_TISR_TXMULTICASTFRAMES_G_WIDTH	1
#define MMC_TISR_TX64OCTETS_GB_INDEX		4
#define MMC_TISR_TX64OCTETS_GB_WIDTH		1
#define MMC_TISR_TX65TO127OCTETS_GB_INDEX	5
#define MMC_TISR_TX65TO127OCTETS_GB_WIDTH	1
#define MMC_TISR_TX128TO255OCTETS_GB_INDEX	6
#define MMC_TISR_TX128TO255OCTETS_GB_WIDTH	1
#define MMC_TISR_TX256TO511OCTETS_GB_INDEX	7
#define MMC_TISR_TX256TO511OCTETS_GB_WIDTH	1
#define MMC_TISR_TX512TO1023OCTETS_GB_INDEX	8
#define MMC_TISR_TX512TO1023OCTETS_GB_WIDTH	1
#define MMC_TISR_TX1024TOMAXOCTETS_GB_INDEX	9
#define MMC_TISR_TX1024TOMAXOCTETS_GB_WIDTH	1
#define MMC_TISR_TXUNICASTFRAMES_GB_INDEX	10
#define MMC_TISR_TXUNICASTFRAMES_GB_WIDTH	1
#define MMC_TISR_TXMULTICASTFRAMES_GB_INDEX	11
#define MMC_TISR_TXMULTICASTFRAMES_GB_WIDTH	1
#define MMC_TISR_TXBROADCASTFRAMES_GB_INDEX	12
#define MMC_TISR_TXBROADCASTFRAMES_GB_WIDTH	1
#define MMC_TISR_TXUNDERFLOWERROR_INDEX		13
#define MMC_TISR_TXUNDERFLOWERROR_WIDTH		1
#define MMC_TISR_TXOCTETCOUNT_G_INDEX		14
#define MMC_TISR_TXOCTETCOUNT_G_WIDTH		1
#define MMC_TISR_TXFRAMECOUNT_G_INDEX		15
#define MMC_TISR_TXFRAMECOUNT_G_WIDTH		1
#define MMC_TISR_TXPAUSEFRAMES_INDEX		16
#define MMC_TISR_TXPAUSEFRAMES_WIDTH		1
#define MMC_TISR_TXVLANFRAMES_G_INDEX		17
#define MMC_TISR_TXVLANFRAMES_G_WIDTH		1

/* MTL register offsets */
#define MTL_OMR				0x1000
#define MTL_FDCR			0x1008
#define MTL_FDSR			0x100c
#define MTL_FDDR			0x1010
#define MTL_ISR				0x1020
#define MTL_RQDCM0R			0x1030
#define MTL_TCPM0R			0x1040
#define MTL_TCPM1R			0x1044

#define MTL_RQDCM_INC			4
#define MTL_RQDCM_Q_PER_REG		4
#define MTL_TCPM_INC			4
#define MTL_TCPM_TC_PER_REG		4

/* MTL register entry bit positions and sizes */
#define MTL_OMR_ETSALG_INDEX		5
#define MTL_OMR_ETSALG_WIDTH		2
#define MTL_OMR_RAA_INDEX		2
#define MTL_OMR_RAA_WIDTH		1

/* MTL queue register offsets
 *   Multiple queues can be active.  The first queue has registers
 *   that begin at 0x1100.  Each subsequent queue has registers that
 *   are accessed using an offset of 0x80 from the previous queue.
 */
#define MTL_Q_BASE			0x1100
#define MTL_Q_INC			0x80

#define MTL_Q_TQOMR			0x00
#define MTL_Q_TQUR			0x04
#define MTL_Q_TQDR			0x08
#define MTL_Q_RQOMR			0x40
#define MTL_Q_RQMPOCR			0x44
#define MTL_Q_RQDR			0x48
#define MTL_Q_RQFCR			0x50
#define MTL_Q_IER			0x70
#define MTL_Q_ISR			0x74

/* MTL queue register entry bit positions and sizes */
#define MTL_Q_RQDR_PRXQ_INDEX		16
#define MTL_Q_RQDR_PRXQ_WIDTH		14
#define MTL_Q_RQDR_RXQSTS_INDEX		4
#define MTL_Q_RQDR_RXQSTS_WIDTH		2
#define MTL_Q_RQFCR_RFA_INDEX		1
#define MTL_Q_RQFCR_RFA_WIDTH		6
#define MTL_Q_RQFCR_RFD_INDEX		17
#define MTL_Q_RQFCR_RFD_WIDTH		6
#define MTL_Q_RQOMR_EHFC_INDEX		7
#define MTL_Q_RQOMR_EHFC_WIDTH		1
#define MTL_Q_RQOMR_RQS_INDEX		16
#define MTL_Q_RQOMR_RQS_WIDTH		9
#define MTL_Q_RQOMR_RSF_INDEX		5
#define MTL_Q_RQOMR_RSF_WIDTH		1
#define MTL_Q_RQOMR_RTC_INDEX		0
#define MTL_Q_RQOMR_RTC_WIDTH		2
#define MTL_Q_TQDR_TRCSTS_INDEX		1
#define MTL_Q_TQDR_TRCSTS_WIDTH		2
#define MTL_Q_TQDR_TXQSTS_INDEX		4
#define MTL_Q_TQDR_TXQSTS_WIDTH		1
#define MTL_Q_TQOMR_FTQ_INDEX		0
#define MTL_Q_TQOMR_FTQ_WIDTH		1
#define MTL_Q_TQOMR_Q2TCMAP_INDEX	8
#define MTL_Q_TQOMR_Q2TCMAP_WIDTH	3
#define MTL_Q_TQOMR_TQS_INDEX		16
#define MTL_Q_TQOMR_TQS_WIDTH		10
#define MTL_Q_TQOMR_TSF_INDEX		1
#define MTL_Q_TQOMR_TSF_WIDTH		1
#define MTL_Q_TQOMR_TTC_INDEX		4
#define MTL_Q_TQOMR_TTC_WIDTH		3
#define MTL_Q_TQOMR_TXQEN_INDEX		2
#define MTL_Q_TQOMR_TXQEN_WIDTH		2

/* MTL queue register value */
#define MTL_RSF_DISABLE			0x00
#define MTL_RSF_ENABLE			0x01
#define MTL_TSF_DISABLE			0x00
#define MTL_TSF_ENABLE			0x01

#define MTL_RX_THRESHOLD_64		0x00
#define MTL_RX_THRESHOLD_96		0x02
#define MTL_RX_THRESHOLD_128		0x03
#define MTL_TX_THRESHOLD_32		0x01
#define MTL_TX_THRESHOLD_64		0x00
#define MTL_TX_THRESHOLD_96		0x02
#define MTL_TX_THRESHOLD_128		0x03
#define MTL_TX_THRESHOLD_192		0x04
#define MTL_TX_THRESHOLD_256		0x05
#define MTL_TX_THRESHOLD_384		0x06
#define MTL_TX_THRESHOLD_512		0x07

#define MTL_ETSALG_WRR			0x00
#define MTL_ETSALG_WFQ			0x01
#define MTL_ETSALG_DWRR			0x02
#define MTL_RAA_SP			0x00
#define MTL_RAA_WSP			0x01

#define MTL_Q_DISABLED			0x00
#define MTL_Q_ENABLED			0x02

/* MTL traffic class register offsets
 *   Multiple traffic classes can be active.  The first class has registers
 *   that begin at 0x1100.  Each subsequent queue has registers that
 *   are accessed using an offset of 0x80 from the previous queue.
 */
#define MTL_TC_BASE			MTL_Q_BASE
#define MTL_TC_INC			MTL_Q_INC

#define MTL_TC_ETSCR			0x10
#define MTL_TC_ETSSR			0x14
#define MTL_TC_QWR			0x18

/* MTL traffic class register entry bit positions and sizes */
#define MTL_TC_ETSCR_TSA_INDEX		0
#define MTL_TC_ETSCR_TSA_WIDTH		2
#define MTL_TC_QWR_QW_INDEX		0
#define MTL_TC_QWR_QW_WIDTH		21

/* MTL traffic class register value */
#define MTL_TSA_SP			0x00
#define MTL_TSA_ETS			0x02

#if XGBE_SRIOV_VF
/* MISC register offsets */
#define XGMAC_PCIE_MISC_HOST_ADDR_63_48_VF 0x800
#define XGMAC_PCIE_MISC_FUNC_RESOURCES_VF 0x804
#define XGMAC_PCIE_MISC_MAILBOX_INT_STA_FRM_PF0_VF 0x81C
#define XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF 0x820
#define XGMAC_PCIE_MISC_MAILBOX_INT_EN_VF 0x824
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_MB_PF0_to_VF 0x828
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_RX_VF 0x840
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_TX_VF 0x890

#define MSIC_INC 4
#endif
/* PCS register offsets */
#define PCS_V1_WINDOW_SELECT		0x03fc
#define PCS_V2_WINDOW_DEF		0x9060
#define PCS_V2_WINDOW_SELECT		0x9064
#define PCS_V2_RV_WINDOW_DEF		0x1060
#define PCS_V2_RV_WINDOW_SELECT		0x1064

/* PCS register entry bit positions and sizes */
#define PCS_V2_WINDOW_DEF_OFFSET_INDEX	6
#define PCS_V2_WINDOW_DEF_OFFSET_WIDTH	14
#define PCS_V2_WINDOW_DEF_SIZE_INDEX	2
#define PCS_V2_WINDOW_DEF_SIZE_WIDTH	4

/* SerDes integration register offsets */
#define SIR0_KR_RT_1			0x002c
#define SIR0_STATUS			0x0040
#define SIR1_SPEED			0x0000

/* SerDes integration register entry bit positions and sizes */
#define SIR0_KR_RT_1_RESET_INDEX	11
#define SIR0_KR_RT_1_RESET_WIDTH	1
#define SIR0_STATUS_RX_READY_INDEX	0
#define SIR0_STATUS_RX_READY_WIDTH	1
#define SIR0_STATUS_TX_READY_INDEX	8
#define SIR0_STATUS_TX_READY_WIDTH	1
#define SIR1_SPEED_CDR_RATE_INDEX	12
#define SIR1_SPEED_CDR_RATE_WIDTH	4
#define SIR1_SPEED_DATARATE_INDEX	4
#define SIR1_SPEED_DATARATE_WIDTH	2
#define SIR1_SPEED_PLLSEL_INDEX		3
#define SIR1_SPEED_PLLSEL_WIDTH		1
#define SIR1_SPEED_RATECHANGE_INDEX	6
#define SIR1_SPEED_RATECHANGE_WIDTH	1
#define SIR1_SPEED_TXAMP_INDEX		8
#define SIR1_SPEED_TXAMP_WIDTH		4
#define SIR1_SPEED_WORDMODE_INDEX	0
#define SIR1_SPEED_WORDMODE_WIDTH	3

/* SerDes RxTx register offsets */
#define RXTX_REG6			0x0018
#define RXTX_REG20			0x0050
#define RXTX_REG22			0x0058
#define RXTX_REG114			0x01c8
#define RXTX_REG129			0x0204

/* SerDes RxTx register entry bit positions and sizes */
#define RXTX_REG6_RESETB_RXD_INDEX	8
#define RXTX_REG6_RESETB_RXD_WIDTH	1
#define RXTX_REG20_BLWC_ENA_INDEX	2
#define RXTX_REG20_BLWC_ENA_WIDTH	1
#define RXTX_REG114_PQ_REG_INDEX	9
#define RXTX_REG114_PQ_REG_WIDTH	7
#define RXTX_REG129_RXDFE_CONFIG_INDEX	14
#define RXTX_REG129_RXDFE_CONFIG_WIDTH	2

/* Descriptor/Packet entry bit positions and sizes */
#define RX_PACKET_ERRORS_CRC_INDEX		2
#define RX_PACKET_ERRORS_CRC_WIDTH		1
#define RX_PACKET_ERRORS_FRAME_INDEX		3
#define RX_PACKET_ERRORS_FRAME_WIDTH		1
#define RX_PACKET_ERRORS_LENGTH_INDEX		0
#define RX_PACKET_ERRORS_LENGTH_WIDTH		1
#define RX_PACKET_ERRORS_OVERRUN_INDEX		1
#define RX_PACKET_ERRORS_OVERRUN_WIDTH		1

#define RX_PACKET_ATTRIBUTES_CSUM_DONE_INDEX	0
#define RX_PACKET_ATTRIBUTES_CSUM_DONE_WIDTH	1
#define RX_PACKET_ATTRIBUTES_VLAN_CTAG_INDEX	1
#define RX_PACKET_ATTRIBUTES_VLAN_CTAG_WIDTH	1
#define RX_PACKET_ATTRIBUTES_LAST_INDEX		2
#define RX_PACKET_ATTRIBUTES_LAST_WIDTH		1
#define RX_PACKET_ATTRIBUTES_CONTEXT_NEXT_INDEX	3
#define RX_PACKET_ATTRIBUTES_CONTEXT_NEXT_WIDTH	1
#define RX_PACKET_ATTRIBUTES_CONTEXT_INDEX	4
#define RX_PACKET_ATTRIBUTES_CONTEXT_WIDTH	1
#define RX_PACKET_ATTRIBUTES_RX_TSTAMP_INDEX	5
#define RX_PACKET_ATTRIBUTES_RX_TSTAMP_WIDTH	1
#define RX_PACKET_ATTRIBUTES_RSS_HASH_INDEX	6
#define RX_PACKET_ATTRIBUTES_RSS_HASH_WIDTH	1
#define RX_PACKET_ATTRIBUTES_FIRST_INDEX	7
#define RX_PACKET_ATTRIBUTES_FIRST_WIDTH	1
#define RX_PACKET_ATTRIBUTES_TNP_INDEX		8
#define RX_PACKET_ATTRIBUTES_TNP_WIDTH		1
#define RX_PACKET_ATTRIBUTES_TNPCSUM_DONE_INDEX	9
#define RX_PACKET_ATTRIBUTES_TNPCSUM_DONE_WIDTH	1

#define RX_NORMAL_DESC0_OVT_INDEX		0
#define RX_NORMAL_DESC0_OVT_WIDTH		16
#define RX_NORMAL_DESC2_HL_INDEX		0
#define RX_NORMAL_DESC2_HL_WIDTH		10
#define RX_NORMAL_DESC2_TNP_INDEX		11
#define RX_NORMAL_DESC2_TNP_WIDTH		1
#define RX_NORMAL_DESC3_CDA_INDEX		27
#define RX_NORMAL_DESC3_CDA_WIDTH		1
#define RX_NORMAL_DESC3_CTXT_INDEX		30
#define RX_NORMAL_DESC3_CTXT_WIDTH		1
#define RX_NORMAL_DESC3_ES_INDEX		15
#define RX_NORMAL_DESC3_ES_WIDTH		1
#define RX_NORMAL_DESC3_ETLT_INDEX		16
#define RX_NORMAL_DESC3_ETLT_WIDTH		4
#define RX_NORMAL_DESC3_FD_INDEX		29
#define RX_NORMAL_DESC3_FD_WIDTH		1
#define RX_NORMAL_DESC3_INTE_INDEX		30
#define RX_NORMAL_DESC3_INTE_WIDTH		1
#define RX_NORMAL_DESC3_L34T_INDEX		20
#define RX_NORMAL_DESC3_L34T_WIDTH		4
#define RX_NORMAL_DESC3_LD_INDEX		28
#define RX_NORMAL_DESC3_LD_WIDTH		1
#define RX_NORMAL_DESC3_OWN_INDEX		31
#define RX_NORMAL_DESC3_OWN_WIDTH		1
#define RX_NORMAL_DESC3_PL_INDEX		0
#define RX_NORMAL_DESC3_PL_WIDTH		14
#define RX_NORMAL_DESC3_RSV_INDEX		26
#define RX_NORMAL_DESC3_RSV_WIDTH		1

#define RX_DESC3_L34T_IPV4_TCP			1
#define RX_DESC3_L34T_IPV4_UDP			2
#define RX_DESC3_L34T_IPV4_ICMP			3
#define RX_DESC3_L34T_IPV4_UNKNOWN		7
#define RX_DESC3_L34T_IPV6_TCP			9
#define RX_DESC3_L34T_IPV6_UDP			10
#define RX_DESC3_L34T_IPV6_ICMP			11
#define RX_DESC3_L34T_IPV6_UNKNOWN		15

#define RX_CONTEXT_DESC3_TSA_INDEX		4
#define RX_CONTEXT_DESC3_TSA_WIDTH		1
#define RX_CONTEXT_DESC3_TSD_INDEX		6
#define RX_CONTEXT_DESC3_TSD_WIDTH		1

#define TX_PACKET_ATTRIBUTES_CSUM_ENABLE_INDEX	0
#define TX_PACKET_ATTRIBUTES_CSUM_ENABLE_WIDTH	1
#define TX_PACKET_ATTRIBUTES_TSO_ENABLE_INDEX	1
#define TX_PACKET_ATTRIBUTES_TSO_ENABLE_WIDTH	1
#define TX_PACKET_ATTRIBUTES_VLAN_CTAG_INDEX	2
#define TX_PACKET_ATTRIBUTES_VLAN_CTAG_WIDTH	1
#define TX_PACKET_ATTRIBUTES_PTP_INDEX		3
#define TX_PACKET_ATTRIBUTES_PTP_WIDTH		1
#define TX_PACKET_ATTRIBUTES_VXLAN_INDEX	4
#define TX_PACKET_ATTRIBUTES_VXLAN_WIDTH	1

#if XGBE_SRIOV_VF
#define TX_CONTEXT_DESC0_TTSL_INDEX		0
#define TX_CONTEXT_DESC0_TTSL_WIDTH		32
#define TX_CONTEXT_DESC1_TTSH_INDEX		0
#define TX_CONTEXT_DESC1_TTSH_WIDTH		32
#endif
#define TX_CONTEXT_DESC2_MSS_INDEX		0
#define TX_CONTEXT_DESC2_MSS_WIDTH		15
#define TX_CONTEXT_DESC3_CTXT_INDEX		30
#define TX_CONTEXT_DESC3_CTXT_WIDTH		1
#define TX_CONTEXT_DESC3_TCMSSV_INDEX		26
#define TX_CONTEXT_DESC3_TCMSSV_WIDTH		1
#define TX_CONTEXT_DESC3_VLTV_INDEX		16
#define TX_CONTEXT_DESC3_VLTV_WIDTH		1
#define TX_CONTEXT_DESC3_VT_INDEX		0
#define TX_CONTEXT_DESC3_VT_WIDTH		16

#define TX_NORMAL_DESC2_HL_B1L_INDEX		0
#define TX_NORMAL_DESC2_HL_B1L_WIDTH		14
#define TX_NORMAL_DESC2_IC_INDEX		31
#define TX_NORMAL_DESC2_IC_WIDTH		1
#define TX_NORMAL_DESC2_TTSE_INDEX		30
#define TX_NORMAL_DESC2_TTSE_WIDTH		1
#define TX_NORMAL_DESC2_VTIR_INDEX		14
#define TX_NORMAL_DESC2_VTIR_WIDTH		2
#define TX_NORMAL_DESC3_CIC_INDEX		16
#define TX_NORMAL_DESC3_CIC_WIDTH		2
#define TX_NORMAL_DESC3_CPC_INDEX		26
#define TX_NORMAL_DESC3_CPC_WIDTH		2
#define TX_NORMAL_DESC3_CTXT_INDEX		30
#define TX_NORMAL_DESC3_CTXT_WIDTH		1
#define TX_NORMAL_DESC3_FD_INDEX		29
#define TX_NORMAL_DESC3_FD_WIDTH		1
#define TX_NORMAL_DESC3_FL_INDEX		0
#define TX_NORMAL_DESC3_FL_WIDTH		15
#define TX_NORMAL_DESC3_LD_INDEX		28
#define TX_NORMAL_DESC3_LD_WIDTH		1
#define TX_NORMAL_DESC3_OWN_INDEX		31
#define TX_NORMAL_DESC3_OWN_WIDTH		1
#define TX_NORMAL_DESC3_TCPHDRLEN_INDEX		19
#define TX_NORMAL_DESC3_TCPHDRLEN_WIDTH		4
#define TX_NORMAL_DESC3_TCPPL_INDEX		0
#define TX_NORMAL_DESC3_TCPPL_WIDTH		18
#define TX_NORMAL_DESC3_TSE_INDEX		18
#define TX_NORMAL_DESC3_TSE_WIDTH		1
#define TX_NORMAL_DESC3_VNP_INDEX		23
#define TX_NORMAL_DESC3_VNP_WIDTH		3

#define TX_NORMAL_DESC2_VLAN_INSERT		0x2
#define TX_NORMAL_DESC3_VXLAN_PACKET		0x3

/* MDIO undefined or vendor specific registers */
#ifndef MDIO_PMA_10GBR_PMD_CTRL
#define MDIO_PMA_10GBR_PMD_CTRL		0x0096
#endif

#ifndef MDIO_PMA_10GBR_FECCTRL
#define MDIO_PMA_10GBR_FECCTRL		0x00ab
#endif

#ifndef MDIO_PCS_DIG_CTRL
#define MDIO_PCS_DIG_CTRL		0x8000
#endif

#ifndef MDIO_AN_XNP
#define MDIO_AN_XNP			0x0016
#endif

#ifndef MDIO_AN_LPX
#define MDIO_AN_LPX			0x0019
#endif

#ifndef MDIO_AN_COMP_STAT
#define MDIO_AN_COMP_STAT		0x0030
#endif

#ifndef MDIO_AN_INTMASK
#define MDIO_AN_INTMASK			0x8001
#endif

#ifndef MDIO_AN_INT
#define MDIO_AN_INT			0x8002
#endif

#ifndef MDIO_VEND2_AN_ADVERTISE
#define MDIO_VEND2_AN_ADVERTISE		0x0004
#endif

#ifndef MDIO_VEND2_AN_LP_ABILITY
#define MDIO_VEND2_AN_LP_ABILITY	0x0005
#endif

#ifndef MDIO_VEND2_AN_CTRL
#define MDIO_VEND2_AN_CTRL		0x8001
#endif

#ifndef MDIO_VEND2_AN_STAT
#define MDIO_VEND2_AN_STAT		0x8002
#endif

#ifndef MDIO_VEND2_PMA_CDR_CONTROL
#define MDIO_VEND2_PMA_CDR_CONTROL	0x8056
#endif

#ifndef MDIO_CTRL1_SPEED1G
#define MDIO_CTRL1_SPEED1G		(MDIO_CTRL1_SPEED10G & ~BMCR_SPEED100)
#endif

#ifndef MDIO_VEND2_CTRL1_AN_ENABLE
#define MDIO_VEND2_CTRL1_AN_ENABLE	BIT(12)
#endif

#ifndef MDIO_VEND2_CTRL1_AN_RESTART
#define MDIO_VEND2_CTRL1_AN_RESTART	BIT(9)
#endif

#ifndef MDIO_VEND2_CTRL1_SS6
#define MDIO_VEND2_CTRL1_SS6		BIT(6)
#endif

#ifndef MDIO_VEND2_CTRL1_SS13
#define MDIO_VEND2_CTRL1_SS13		BIT(13)
#endif

/* MDIO mask values */
#define XGBE_AN_CL73_INT_CMPLT		BIT(0)
#define XGBE_AN_CL73_INC_LINK		BIT(1)
#define XGBE_AN_CL73_PG_RCV		BIT(2)
#define XGBE_AN_CL73_INT_MASK		0x07

#define XGBE_XNP_MCF_NULL_MESSAGE	0x001
#define XGBE_XNP_ACK_PROCESSED		BIT(12)
#define XGBE_XNP_MP_FORMATTED		BIT(13)
#define XGBE_XNP_NP_EXCHANGE		BIT(15)

#define XGBE_KR_TRAINING_START		BIT(0)
#define XGBE_KR_TRAINING_ENABLE		BIT(1)

#define XGBE_PCS_CL37_BP		BIT(12)

#define XGBE_AN_CL37_INT_CMPLT		BIT(0)
#define XGBE_AN_CL37_INT_MASK		0x01

#define XGBE_AN_CL37_HD_MASK		0x40
#define XGBE_AN_CL37_FD_MASK		0x20

#define XGBE_AN_CL37_PCS_MODE_MASK	0x06
#define XGBE_AN_CL37_PCS_MODE_BASEX	0x00
#define XGBE_AN_CL37_PCS_MODE_SGMII	0x04
#define XGBE_AN_CL37_TX_CONFIG_MASK	0x08
#define XGBE_AN_CL37_MII_CTRL_8BIT	0x0100

#define XGBE_PMA_CDR_TRACK_EN_MASK	0x01
#define XGBE_PMA_CDR_TRACK_EN_OFF	0x00
#define XGBE_PMA_CDR_TRACK_EN_ON	0x01

#if XGBE_SRIOV_PF
/* ELI register offsets */
#define TCAM_ACC                0x0000
#define TCAM_DATA0              0x0010
#define TCAM_DATA1              0x0014
#define TCAM_DATA2              0x0018
#define TCAM_MASK0              0x0020
#define TCAM_MASK1              0x0024
#define TCAM_MASK2              0x0028
#define TCAM_FUNC_PRIO_LOOKUP   0x0030
#define TCAM_DMACH_LOOKUP       0x0040
#define TCAM_ECC_CHKSUM_CTRL    0x0080
#define TCAM_DMACH_CTRL         0x0084
#define TCAM_UNTAGGED_PRI       0x0088

/* ELI registers entry bit positions and sizes */
#define TCAM_ACC_OP_STR_DONE_INDEX          0
#define TCAM_ACC_OP_STR_DONE_WIDTH          1
#define TCAM_ACC_OP_SEL_INDEX               1
#define TCAM_ACC_OP_SEL_WIDTH               3
#define TCAM_ACC_RAM_CLEAR_INDEX            4
#define TCAM_ACC_RAM_CLEAR_WIDTH            1
#define TCAM_ACC_KEY_RAW_ENC_INDEX          5
#define TCAM_ACC_KEY_RAW_ENC_WIDTH          1
#define TCAM_ACC_RAM_SEL_INDEX              10
#define TCAM_ACC_RAM_SEL_WIDTH              5
#define TCAM_ACC_TCAM_RST_INDEX             15
#define TCAM_ACC_TCAM_RST_WIDTH             1
#define TCAM_ACC_XCESS_ADDR_INDEX           16
#define TCAM_ACC_XCESS_ADDR_WIDTH           9
#define TCAM_ACC_SEARCH_STS_INDEX           27
#define TCAM_ACC_SEARCH_STS_WIDTH           1
#define TCAM_ACC_RD_STS_INDEX               28
#define TCAM_ACC_RD_STS_WIDTH               4
#define TCAM_DATA0_DA_ENABLE_INDEX          0
#define TCAM_DATA0_DA_ENABLE_WIDTH          2
#define TCAM_DATA0_DA_INDEX                 2
#define TCAM_DATA0_DA_WIDTH                 30
#define TCAM_DATA1_DA_INDEX                 0
#define TCAM_DATA1_DA_WIDTH                 18
#define TCAM_DATA1_IVT_INDEX                18
#define TCAM_DATA1_IVT_WIDTH                12
#define TCAM_DATA1_OVT_INDEX                30
#define TCAM_DATA1_OVT_WIDTH                2
#define TCAM_DATA2_OVT_INDEX                0
#define TCAM_DATA2_OVT_WIDTH                10
#define TCAM_FUNC_PRIO_LOOKUP_PRI_INDEX     7
#define TCAM_FUNC_PRIO_LOOKUP_PRI_WIDTH     1
#define TCAM_FUNC_PRIO_LOOKUP_FUNC_INDEX    0
#define TCAM_FUNC_PRIO_LOOKUP_FUNC_WIDTH    6
#define TCAM_UNTAGGED_PRI_MC_PRI_INDEX      29
#define TCAM_UNTAGGED_PRI_MC_PRI_WIDTH      3
#define TCAM_UNTAGGED_PRI_UC_PF_PRI_INDEX   0
#define TCAM_UNTAGGED_PRI_UC_PF_PRI_WIDTH   3
#define TCAM_UNTAGGED_PRI_UC_VF0_PRI_INDEX  3
#define TCAM_UNTAGGED_PRI_UC_VF0_PRI_WIDTH  3
#define TCAM_UNTAGGED_PRI_UC_VF1_PRI_INDEX  6
#define TCAM_UNTAGGED_PRI_UC_VF1_PRI_WIDTH  3
#define TCAM_UNTAGGED_PRI_UC_VF2_PRI_INDEX  9
#define TCAM_UNTAGGED_PRI_UC_VF2_PRI_WIDTH  3
#define TCAM_UNTAGGED_PRI_UC_VF3_PRI_INDEX  12
#define TCAM_UNTAGGED_PRI_UC_VF3_PRI_WIDTH  3
#define TCAM_UNTAGGED_PRI_UC_VF4_PRI_INDEX  15
#define TCAM_UNTAGGED_PRI_UC_VF4_PRI_WIDTH  3
#define TCAM_UNTAGGED_PRI_UC_VF5_PRI_INDEX  18
#define TCAM_UNTAGGED_PRI_UC_VF5_PRI_WIDTH  3
#define TCAM_UNTAGGED_PRI_UC_VF6_PRI_INDEX  21
#define TCAM_UNTAGGED_PRI_UC_VF6_PRI_WIDTH  3
/*ELI masks */
#define TCAM_DATA0_DA_MASK    0xFFFFFFFC
#define TCAM_DATA1_DA_MASK    0x3FFFF
#endif

/* Bit setting and getting macros
 *  The get macro will extract the current bit field value from within
 *  the variable
 *
 *  The set macro will clear the current bit field value within the
 *  variable and then set the bit field of the variable to the
 *  specified value
 */
#define GET_BITS(_var, _index, _width)					\
	(((_var) >> (_index)) & ((0x1 << (_width)) - 1))

#define SET_BITS(_var, _index, _width, _val)				\
do {									\
	(_var) &= ~(((0x1 << (_width)) - 1) << (_index));		\
	(_var) |= (((_val) & ((0x1 << (_width)) - 1)) << (_index));	\
} while (0)

#define GET_BITS_LE(_var, _index, _width)				\
	((le32_to_cpu((_var)) >> (_index)) & ((0x1 << (_width)) - 1))

#define SET_BITS_LE(_var, _index, _width, _val)				\
do {									\
	(_var) &= cpu_to_le32(~(((0x1 << (_width)) - 1) << (_index)));	\
	(_var) |= cpu_to_le32((((_val) &				\
			      ((0x1 << (_width)) - 1)) << (_index)));	\
} while (0)

/* Bit setting and getting macros based on register fields
 *  The get macro uses the bit field definitions formed using the input
 *  names to extract the current bit field value from within the
 *  variable
 *
 *  The set macro uses the bit field definitions formed using the input
 *  names to set the bit field of the variable to the specified value
 */
#define XGMAC_GET_BITS(_var, _prefix, _field)				\
	GET_BITS((_var),						\
		 _prefix##_##_field##_INDEX,				\
		 _prefix##_##_field##_WIDTH)

#define XGMAC_SET_BITS(_var, _prefix, _field, _val)			\
	SET_BITS((_var),						\
		 _prefix##_##_field##_INDEX,				\
		 _prefix##_##_field##_WIDTH, (_val))

#define XGMAC_GET_BITS_LE(_var, _prefix, _field)			\
	GET_BITS_LE((_var),						\
		 _prefix##_##_field##_INDEX,				\
		 _prefix##_##_field##_WIDTH)

#define XGMAC_SET_BITS_LE(_var, _prefix, _field, _val)			\
	SET_BITS_LE((_var),						\
		 _prefix##_##_field##_INDEX,				\
		 _prefix##_##_field##_WIDTH, (_val))

/* Macros for reading or writing MISC registers
 *  The ioread macros will get bit fields or full values using the
 *  register definitions formed using the input names
 *
 *  The iowrite macros will set bit fields or full values using the
 *  register definitions formed using the input names
 */
#define MISC_IOREAD(_pdata, _reg)					\
	ioread32((_pdata)->misc_regs + _reg)
#if XGBE_SRIOV_PF
#define MISC_IOREAD_BITS(_pdata, _reg, _field)				\
   GET_BITS(MISC_IOREAD((_pdata), _reg),				\
       _reg##_##_field##_INDEX,				\
       _reg##_##_field##_WIDTH)
#endif
#define MISC_IOWRITE(_pdata, _reg, _val)				\
	iowrite32((_val), (_pdata)->misc_regs + _reg)
#if XGBE_SRIOV_PF
#define MISC_IOWRITE_OFFSET(_pdata, _reg, _offset, _val)  \
	iowrite32((_val), (_pdata)->misc_regs + _reg + _offset)
#define MISC_IOREAD_OFFSET(_pdata, _reg, _offset)  \
	ioread32((_pdata)->misc_regs + _reg + _offset)
/* Macros for reading or writing ELI registers
 *  The ioread macros will get bit fields or full values using the
 *  register definitions formed using the input names
 *
 *  The iowrite macros will set bit fields or full values using the
 *  register definitions formed using the input names
 */
#define ELI_GET_BITS(_var, _prefix, _field)				\
	GET_BITS((_var),						\
		 _prefix##_##_field##_INDEX,				\
		 _prefix##_##_field##_WIDTH)

#define ELI_SET_BITS(_var, _prefix, _field, _val)			\
	SET_BITS((_var),						\
		 _prefix##_##_field##_INDEX,				\
		 _prefix##_##_field##_WIDTH, (_val))

#define ELI_IOREAD(_pdata, _reg)					\
   ioread32((_pdata)->eli_regs + _reg)

#define ELI_IOREAD_BITS(_pdata, _reg, _field)				\
   GET_BITS(ELI_IOREAD((_pdata), _reg),				\
       _reg##_##_field##_INDEX,				\
       _reg##_##_field##_WIDTH)

#define ELI_IOWRITE(_pdata, _reg, _val)				\
   iowrite32((_val), (_pdata)->eli_regs + (_reg))

#define ELI_IOWRITE_BITS(_pdata, _reg, _field, _val)			\
   do {									\
     u32 reg_val = ELI_IOREAD((_pdata), _reg);			\
     SET_BITS(reg_val,						\
         _reg##_##_field##_INDEX,				\
         _reg##_##_field##_WIDTH, (_val));			\
     ELI_IOWRITE((_pdata), _reg, reg_val);				\
   } while (0)
#endif

#define XGMAC_IOREAD(_pdata, _reg)					\
	ioread32((_pdata)->xgmac_regs + _reg)

#define XGMAC_IOREAD_BITS(_pdata, _reg, _field)				\
	GET_BITS(XGMAC_IOREAD((_pdata), _reg),				\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XGMAC_IOWRITE(_pdata, _reg, _val)				\
	iowrite32((_val), (_pdata)->xgmac_regs + (_reg))

#define XGMAC_IOWRITE_BITS(_pdata, _reg, _field, _val)			\
do {									\
	u32 reg_val = XGMAC_IOREAD((_pdata), _reg);			\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XGMAC_IOWRITE((_pdata), _reg, reg_val);				\
} while (0)

/* Macros for reading or writing MTL queue or traffic class registers
 *  Similar to the standard read and write macros except that the
 *  base register value is calculated by the queue or traffic class number
 */
#define XGMAC_MTL_IOREAD(_pdata, _n, _reg)				\
	ioread32((_pdata)->xgmac_regs +					\
		 MTL_Q_BASE + ((_n) * MTL_Q_INC) + _reg)

#define XGMAC_MTL_IOREAD_BITS(_pdata, _n, _reg, _field)			\
	GET_BITS(XGMAC_MTL_IOREAD((_pdata), (_n), _reg),		\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XGMAC_MTL_IOWRITE(_pdata, _n, _reg, _val)			\
	iowrite32((_val), (_pdata)->xgmac_regs +			\
		  MTL_Q_BASE + ((_n) * MTL_Q_INC) + _reg)

#define XGMAC_MTL_IOWRITE_BITS(_pdata, _n, _reg, _field, _val)		\
do {									\
	u32 reg_val = XGMAC_MTL_IOREAD((_pdata), (_n), _reg);		\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XGMAC_MTL_IOWRITE((_pdata), (_n), _reg, reg_val);		\
} while (0)

/* Macros for reading or writing DMA channel registers
 *  Similar to the standard read and write macros except that the
 *  base register value is obtained from the ring
 */
#define XGMAC_DMA_IOREAD(_channel, _reg)				\
	ioread32((_channel)->dma_regs + _reg)

#define XGMAC_DMA_IOREAD_BITS(_channel, _reg, _field)			\
	GET_BITS(XGMAC_DMA_IOREAD((_channel), _reg),			\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XGMAC_DMA_IOWRITE(_channel, _reg, _val)				\
	iowrite32((_val), (_channel)->dma_regs + _reg)

#define XGMAC_DMA_IOWRITE_BITS(_channel, _reg, _field, _val)		\
do {									\
	u32 reg_val = XGMAC_DMA_IOREAD((_channel), _reg);		\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XGMAC_DMA_IOWRITE((_channel), _reg, reg_val);			\
} while (0)

#if XGBE_SRIOV_VF
/* Macros for building, reading or writing register values or bits
 * within the register values of SerDes integration registers.
 */
#define XSIR_GET_BITS(_var, _prefix, _field)                            \
	GET_BITS((_var),                                                \
		 _prefix##_##_field##_INDEX,                            \
		 _prefix##_##_field##_WIDTH)

#define XSIR_SET_BITS(_var, _prefix, _field, _val)                      \
	SET_BITS((_var),                                                \
		 _prefix##_##_field##_INDEX,                            \
		 _prefix##_##_field##_WIDTH, (_val))

#define XSIR0_IOREAD(_pdata, _reg)					\
	ioread16((_pdata)->sir0_regs + _reg)

#define XSIR0_IOREAD_BITS(_pdata, _reg, _field)				\
	GET_BITS(XSIR0_IOREAD((_pdata), _reg),				\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XSIR0_IOWRITE(_pdata, _reg, _val)				\
	iowrite16((_val), (_pdata)->sir0_regs + _reg)

#define XSIR0_IOWRITE_BITS(_pdata, _reg, _field, _val)			\
do {									\
	u16 reg_val = XSIR0_IOREAD((_pdata), _reg);			\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XSIR0_IOWRITE((_pdata), _reg, reg_val);				\
} while (0)

#define XSIR1_IOREAD(_pdata, _reg)					\
	ioread16((_pdata)->sir1_regs + _reg)

#define XSIR1_IOREAD_BITS(_pdata, _reg, _field)				\
	GET_BITS(XSIR1_IOREAD((_pdata), _reg),				\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XSIR1_IOWRITE(_pdata, _reg, _val)				\
	iowrite16((_val), (_pdata)->sir1_regs + _reg)

#define XSIR1_IOWRITE_BITS(_pdata, _reg, _field, _val)			\
do {									\
	u16 reg_val = XSIR1_IOREAD((_pdata), _reg);			\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XSIR1_IOWRITE((_pdata), _reg, reg_val);				\
} while (0)

/* Macros for building, reading or writing register values or bits
 * within the register values of SerDes RxTx registers.
 */
#define XRXTX_IOREAD(_pdata, _reg)					\
	ioread16((_pdata)->rxtx_regs + _reg)

#define XRXTX_IOREAD_BITS(_pdata, _reg, _field)				\
	GET_BITS(XRXTX_IOREAD((_pdata), _reg),				\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XRXTX_IOWRITE(_pdata, _reg, _val)				\
	iowrite16((_val), (_pdata)->rxtx_regs + _reg)

#define XRXTX_IOWRITE_BITS(_pdata, _reg, _field, _val)			\
do {									\
	u16 reg_val = XRXTX_IOREAD((_pdata), _reg);			\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XRXTX_IOWRITE((_pdata), _reg, reg_val);				\
} while (0)
#endif

/* Macros for building, reading or writing register values or bits
 * using MDIO.  Different from above because of the use of standardized
 * Linux include values.  No shifting is performed with the bit
 * operations, everything works on mask values.
 */
#define XMDIO_READ(_pdata, _mmd, _reg)					\
	((_pdata)->hw_if.read_mmd_regs((_pdata), 0,			\
		MII_ADDR_C45 | (_mmd << 16) | ((_reg) & 0xffff)))

#define XMDIO_READ_BITS(_pdata, _mmd, _reg, _mask)			\
	(XMDIO_READ((_pdata), _mmd, _reg) & _mask)

#define XMDIO_WRITE(_pdata, _mmd, _reg, _val)				\
	((_pdata)->hw_if.write_mmd_regs((_pdata), 0,			\
		MII_ADDR_C45 | (_mmd << 16) | ((_reg) & 0xffff), (_val)))

#define XMDIO_WRITE_BITS(_pdata, _mmd, _reg, _mask, _val)		\
do {									\
	u32 mmd_val = XMDIO_READ((_pdata), _mmd, _reg);			\
	mmd_val &= ~_mask;						\
	mmd_val |= (_val);						\
	XMDIO_WRITE((_pdata), _mmd, _reg, mmd_val);			\
} while (0)

#endif
