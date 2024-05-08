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

#ifndef __XGBE_H__
#define __XGBE_H__

#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <linux/bitops.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/timecounter.h>
#include <linux/net_tstamp.h>
#include <net/dcbnl.h>
#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/dcache.h>
#include <linux/ethtool.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include "xgbe-usr-opt.h"

#if XGBE_SRIOV_PF
#define XGBE_DRV_NAME		"brcm_xgbe_pci"
#else
#define XGBE_DRV_NAME		"brcm-xgbe-vf"
#endif

#define XGBE_DRV_VERSION	"1.2.1"
#define XGBE_DRV_DESC		"Broadcom 10 Gigabit Ethernet Driver"

#if XGBE_SRIOV_PF
#define MAC_USERVER        0x21

#define BCM8956X_A0_PF_ID	0xa005
#define BCM8956X_PF_ID	0xa006
#define BCM8957X_PF_ID	0xa008
#define BCM8989X_PF_ID	0xa00b

#else
#define BCM8956X_VF_ID	0xa007
#define BCM8957X_VF_ID	0xa009
#define BCM8989X_VF_ID	0xa00c

#endif
#define TEST_MSG 0

#if TEST_MSG 
#define TEST_PRNT(x...) printk(x)
#else
#define TEST_PRNT(x...) do {}while(0)
#endif

#ifdef CONFIG_PREEMPTION
#define XGBE_USLEEP_RANGE(x, y)  udelay((y))
#define XGBE_MSLEEP(x)  mdelay((x))
#else
#define XGBE_USLEEP_RANGE(x, y)  usleep_range((x) , (y))
#define XGBE_MSLEEP(x)  msleep((x))
#endif

#if XGBE_SRIOV_PF

#define ELI_MSG	0

#if ELI_MSG
#define ELI_PRNT(x...) printk(x)
#else
#define ELI_PRNT(x...) do {}while(0)
#endif

#define ELI_FUNC_PF       0
#define ELI_FUNC_VF0      1
#define ELI_FUNC_VF1      2
#define ELI_FUNC_VF2      3
#define ELI_FUNC_VF3      4
#define ELI_FUNC_VF4      5
#define ELI_FUNC_VF5      6
#define ELI_FUNC_VF6      7
#define ELI_FUNC_BC       0
#define ELI_FUNC_MC       9
#define ELI_TCAM_ENTRY_EN 3
#define ELI_TCAM_RAM                1
#define ELI_TCAM_FUNC_LOOKUP_RAM    2
#define ELI_TCAM_DMA_RAM            4
#define ELI_OP_READ     1
#define ELI_OP_WRITE    2
#define ELI_OP_SEARCH   4

#define ELI_LOOKUP_PRI_IN   0
#define ELI_LOOKUP_PRI_OUT  1

#define ELI_MAC_INDEX   		63
#define ELI_MAX_MAC_INDEX		64
#define ELI_MAX_DMAP_INDEX      512
#define MAC_INX_VLAN				0
#define	MAC_INX_OTHER				1

#define VLAN_ID_LIMIT		3

#define UC_MAC				0
#define IPV4_MC_MAC		1
#define IPV6_MC_MAC		2

#define GET_LIMIT(n)	(ELI_MAC_INDEX - n - ((n + 1) * VLAN_ID_LIMIT))

#define ELI_FUNC_MC_IPV4(n)		((GET_LIMIT(n)) - 2)      	 
#define ELI_FUNC_MC_IPV6(n)		((GET_LIMIT(n)) - 1)		

#define PREP_MASK(mask, p, n, max)						\
	do{																					\
		u32 mask_temp = 0, i = 0, val = 0;				\
		for(i = 0; i < max; i++){									\
			val = (n - 1) + (n * i);								\
			if(val == p)														\
				continue;															\
			mask_temp |= (1 << val);								\
		}																					\
		mask_temp = ~mask_temp;										\
		mask = (~((~(~0 << n)) << (p - n + 1)));	\
		mask = mask & mask_temp ;					\
	}while(0)

#define SRIOV_MAX_VF  		7
#define SRIOV_VF_PF_INT_EN (((1 << SRIOV_MAX_VF) - 1) & 0x7F)

#define ERRVF   1
#define XGBE_VLAN_REQ_FROM_PF -1

#else

#define ENABLE                      0x1
#define DISABLE                     0x0

#endif

#define MAX_MC_VF   9

/* Descriptor related defines */
#define XGBE_TX_DESC_CNT		4096
#define XGBE_TX_DESC_MIN_FREE	(XGBE_TX_DESC_CNT >> 3)
#define XGBE_TX_DESC_MAX_PROC	(XGBE_TX_DESC_CNT >> 1)
#define XGBE_RX_DESC_CNT	4096

#define XGBE_TX_DESC_CNT_MIN	64
#define XGBE_TX_DESC_CNT_MAX	4096
#define XGBE_RX_DESC_CNT_MIN	64
#define XGBE_RX_DESC_CNT_MAX	4096

#define XGBE_TX_MAX_BUF_SIZE	(0x3fff & ~(64 - 1))
#define XGBE_TX_DECS_PREFETCH	3
#define XGBE_RX_DECS_PREFETCH	3

#define XGBE_MIN_DESC	4096
#define XGBE_MAX_DESC	8192

/* Descriptors required for maximum contiguous TSO/GSO packet */
#define XGBE_TX_MAX_SPLIT	((GSO_MAX_SIZE / XGBE_TX_MAX_BUF_SIZE) + 1)

/* Maximum possible descriptors needed for an SKB:
 * - Maximum number of SKB frags
 * - Maximum descriptors for contiguous TSO/GSO packet
 * - Possible context descriptor
 * - Possible TSO header descriptor
 */
#define XGBE_TX_MAX_DESCS	(MAX_SKB_FRAGS + XGBE_TX_MAX_SPLIT + 2)

#define XGBE_RX_MIN_BUF_SIZE	(ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)
#define XGBE_RX_BUF_ALIGN	64
#define XGBE_SKB_ALLOC_SIZE	256
#define XGBE_SPH_HDSMS_SIZE	2	/* Keep in sync with SKB_ALLOC_SIZE */

#define XGBE_MAX_DMA_CHANNELS	16
#define XGBE_MAX_QUEUES		16
#define XGBE_PRIORITY_QUEUES	8
#define XGBE_DMA_STOP_TIMEOUT	1

/* DMA cache settings - Outer sharable, write-back, write-allocate */
#define XGBE_DMA_OS_ARCR	0x002b2b2b
#define XGBE_DMA_OS_AWCR	0x2f2f2f2f

/* DMA cache settings - System, no caches used */
#define XGBE_DMA_SYS_ARCR	0x00303030
#define XGBE_DMA_SYS_AWCR	0x30303030

/* DMA cache settings - PCI device */
#define XGBE_DMA_PCI_ARCR	0x00000000
#define XGBE_DMA_PCI_AWCR	0x00000000
#define XGBE_DMA_PCI_AWARCR	0x00000000
#define XGBE_DMA_PCI_COHERENT	0x01
#define XGBE_IO_TIMEOUT		1000

/* DMA channel interrupt modes */
#define XGBE_IRQ_MODE_EDGE	0
#define XGBE_IRQ_MODE_LEVEL	1

#define XGMAC_MIN_PACKET	60
#define XGMAC_STD_PACKET_MTU	1500
#define XGMAC_MAX_STD_PACKET	1518
#define XGMAC_JUMBO_PACKET_MTU	9000
#define XGMAC_MAX_JUMBO_PACKET	9018
#define XGMAC_ETH_PREAMBLE	(12 + 8)	/* Inter-frame gap + preamble */

#define XGMAC_PFC_DATA_LEN	46
#define XGMAC_PFC_DELAYS	14000

#define XGMAC_PRIO_QUEUES(_cnt)					\
	min_t(unsigned int, IEEE_8021QAZ_MAX_TCS, (_cnt))

/* DMA widths */
#define DMA_WIDTH_32 32
#define DMA_WIDTH_40 40
#define DMA_WIDTH_48 48

/* Hash table sizes */
#define HASH_TABLE_SIZE_64 64
#define HASH_TABLE_SIZE_128 128
#define HASH_TABLE_SIZE_256 256

/* Common property names */
#define XGBE_MAC_ADDR_PROPERTY	"mac-address"
#define XGBE_PHY_MODE_PROPERTY	"phy-mode"
#define XGBE_DMA_IRQS_PROPERTY	"brcm,per-channel-interrupt"
#define XGBE_SPEEDSET_PROPERTY	"brcm,speed-set"

/* Device-tree clock names */
#define XGBE_DMA_CLOCK		"dma_clk"
#define XGBE_PTP_CLOCK		"ptp_clk"

/* ACPI property names */
#define XGBE_ACPI_DMA_FREQ	"brcm,dma-freq"
#define XGBE_ACPI_PTP_FREQ	"brcm,ptp-freq"

/* PCI BAR mapping */
#define XGBE_XGMAC_BAR		0
#define XGBE_XPCS_BAR		1
#define XGBE_PHY_BAR		2
#define XGBE_MAC_PROP_OFFSET	0x1d000
#define XGBE_I2C_CTRL_OFFSET	0x1e000

/* PCI MSI/MSIx support */
#if XGBE_SRIOV_PF
/* 3 VF MailBox, 1 error interrupt, 1 Device to Host */
#define XGBE_MSI_ERR_DEV2HOST 2
#define XGBE_MSI_BASE_COUNT	(SRIOV_MAX_VF + XGBE_MSI_ERR_DEV2HOST)
#else
#define XGBE_MSI_BASE_COUNT	1
#endif
#define XGBE_MSI_MIN_COUNT	(XGBE_MSI_BASE_COUNT + 1)

#if XGBE_SRIOV_PF
#define XGMAC_MAP_VF_MBOX_INT(_i) (_i+(XGBE_MSI_BASE_COUNT-SRIOV_MAX_VF))
#endif

	/* PCI clock frequencies */
#define BCM8989X_V2_DMA_CLOCK_FREQ	250000000	/* 250 MHz */
#define BCM895XX_V2_DMA_CLOCK_FREQ	125000000	/* 125 MHz */

#define XGBE_V2_PTP_CLOCK_FREQ	125000000	/* 125 MHz */

/* Timestamp support - values based on PTP clock
 *   50MHz => 20 nsec(Time Period)
 *   25MHz => 40 nsec(Time period)
 */
#define XGBE_TSTAMP_SSINC	20
#define XGBE_TSTAMP_SNSINC	0

/* Driver PMT macros */
#define XGMAC_DRIVER_CONTEXT	1
#define XGMAC_IOCTL_CONTEXT	2

#define XGMAC_FIFO_MIN_ALLOC	2048
#define XGMAC_FIFO_UNIT		256
#define XGMAC_FIFO_ALIGN(_x)				\
	(((_x) + XGMAC_FIFO_UNIT - 1) & ~(XGMAC_FIFO_UNIT - 1))
#define XGMAC_FIFO_FC_OFF	2048
#define XGMAC_FIFO_FC_MIN	4096

#define XGBE_TC_MIN_QUANTUM	10

/* Helper macro for descriptor handling
 *  Always use XGBE_GET_DESC_DATA to access the descriptor data
 *  since the index is free-running and needs to be and-ed
 *  with the descriptor count value of the ring to index to
 *  the proper descriptor data.
 */
#define XGBE_GET_DESC_DATA(_ring, _idx)				\
	((_ring)->rdata +					\
	 ((_idx) & ((_ring)->rdesc_count - 1)))

/* Default coalescing parameters */
#define XGMAC_INIT_DMA_TX_USECS		1000
#define XGMAC_INIT_DMA_TX_FRAMES	25

#define XGMAC_MAX_DMA_RIWT		0xff
#define XGMAC_INIT_DMA_RX_USECS		30
#define XGMAC_INIT_DMA_RX_FRAMES	25

/* Flow control queue count */
#define XGMAC_MAX_FLOW_CONTROL_QUEUES	8

/* Flow control threshold units */
#define XGMAC_FLOW_CONTROL_UNIT		512
#define XGMAC_FLOW_CONTROL_ALIGN(_x)				\
	(((_x) + XGMAC_FLOW_CONTROL_UNIT - 1) & ~(XGMAC_FLOW_CONTROL_UNIT - 1))
#define XGMAC_FLOW_CONTROL_VALUE(_x)				\
	(((_x) < 1024) ? 0 : ((_x) / XGMAC_FLOW_CONTROL_UNIT) - 2)
#define XGMAC_FLOW_CONTROL_MAX		33280

/* Maximum MAC address hash table size (256 bits = 8 bytes) */
#define XGBE_MAC_HASH_TABLE_SIZE	8

/* Receive Side Scaling */
#define XGBE_RSS_HASH_KEY_SIZE		40
#define XGBE_RSS_MAX_TABLE_SIZE		256
#define XGBE_RSS_LOOKUP_TABLE_TYPE	0
#define XGBE_RSS_HASH_KEY_TYPE		1

/* Auto-negotiation */
#define XGBE_AN_MS_TIMEOUT		500
#define XGBE_LINK_TIMEOUT		5

#define XGBE_SGMII_AN_LINK_STATUS	BIT(1)
#define XGBE_SGMII_AN_LINK_SPEED	(BIT(2) | BIT(3))
#define XGBE_SGMII_AN_LINK_SPEED_100	0x04
#define XGBE_SGMII_AN_LINK_SPEED_1000	0x08
#define XGBE_SGMII_AN_LINK_DUPLEX	BIT(4)

/* ECC correctable error notification window (seconds) */
#define XGBE_ECC_LIMIT			60

/* MDIO port types */
#define XGMAC_MAX_C22_PORT		3

/* Link mode bit operations */
#define XGBE_ZERO_SUP(_ls)		\
	ethtool_link_ksettings_zero_link_mode((_ls), supported)

#define XGBE_SET_SUP(_ls, _mode)	\
	ethtool_link_ksettings_add_link_mode((_ls), supported, _mode)

#define XGBE_CLR_SUP(_ls, _mode)	\
	ethtool_link_ksettings_del_link_mode((_ls), supported, _mode)

#define XGBE_IS_SUP(_ls, _mode)	\
	ethtool_link_ksettings_test_link_mode((_ls), supported, _mode)

#define XGBE_ZERO_ADV(_ls)		\
	ethtool_link_ksettings_zero_link_mode((_ls), advertising)

#define XGBE_SET_ADV(_ls, _mode)	\
	ethtool_link_ksettings_add_link_mode((_ls), advertising, _mode)

#define XGBE_CLR_ADV(_ls, _mode)	\
	ethtool_link_ksettings_del_link_mode((_ls), advertising, _mode)

#define XGBE_ADV(_ls, _mode)		\
	ethtool_link_ksettings_test_link_mode((_ls), advertising, _mode)

#define XGBE_ZERO_LP_ADV(_ls)		\
	ethtool_link_ksettings_zero_link_mode((_ls), lp_advertising)

#define XGBE_SET_LP_ADV(_ls, _mode)	\
	ethtool_link_ksettings_add_link_mode((_ls), lp_advertising, _mode)

#define XGBE_CLR_LP_ADV(_ls, _mode)	\
	ethtool_link_ksettings_del_link_mode((_ls), lp_advertising, _mode)

#define XGBE_LP_ADV(_ls, _mode)		\
	ethtool_link_ksettings_test_link_mode((_ls), lp_advertising, _mode)

#define XGBE_LM_COPY(_dst, _dname, _src, _sname)	\
	bitmap_copy((_dst)->link_modes._dname,		\
		    (_src)->link_modes._sname,		\
		    __ETHTOOL_LINK_MODE_MASK_NBITS)

#define XGBE_MAX_VFS_DEFAULT 3
#define XGBE_MAX_VFS_DRV_LIMIT 7
#define XGBE_MAX_VF_MC_ENTRIES 30

/* BASE Values for str to int conversion */
#define OCTAL_BASE 8
#define DEC_BASE 10
#define HEX_BASE 16

#if XGBE_SRIOV_PF
/* MMC Register values */
#define MMC_RX64OCTETS_GB_LO_VAL 0x00000001
#define MMC_RX64OCTETS_GB_HI_VAL 0x88000000

/* MSIX Register values */
#define XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_LO_VAL 0x00000000
#define XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_HI_VAL 0x88000000
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_SBD_ALL_VAL 0x00
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST_DOORBELL_VAL 0x01
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST0_VAL 0x01
#define XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST1_VAL 0x01
#define XGMAC_PCIE_MISC_MII_CTRL_VAL 0x7
#endif

struct xgbe_prv_data;
struct vf_data_storage;

struct xgbe_packet_data {
	struct sk_buff *skb;

	unsigned int attributes;

	unsigned int errors;

	unsigned int rdesc_count;
	unsigned int length;

	unsigned int header_len;
	unsigned int tcp_header_len;
	unsigned int tcp_payload_len;
	unsigned short mss;

	unsigned short vlan_ctag;

	u64 rx_tstamp;

	u32 rss_hash;
	enum pkt_hash_types rss_hash_type;

	unsigned int tx_packets;
	unsigned int tx_bytes;
};

/* Common Rx and Tx descriptor mapping */
struct xgbe_ring_desc {
	__le32 desc0;
	__le32 desc1;
	__le32 desc2;
	__le32 desc3;
};

/* Page allocation related values */
struct xgbe_page_alloc {
	struct page *pages;
	unsigned int pages_len;
	unsigned int pages_offset;

	dma_addr_t pages_dma;
};

/* Ring entry buffer data */
struct xgbe_buffer_data {
	struct xgbe_page_alloc pa;
	struct xgbe_page_alloc pa_unmap;

	dma_addr_t dma_base;
	unsigned long dma_off;
	unsigned int dma_len;
};

/* Tx-related ring data */
struct xgbe_tx_ring_data {
	unsigned int packets;		/* BQL packet count */
	unsigned int bytes;		/* BQL byte count */
};

/* Rx-related ring data */
struct xgbe_rx_ring_data {
	struct xgbe_buffer_data hdr;	/* Header locations */
	struct xgbe_buffer_data buf;	/* Payload locations */

	unsigned short hdr_len;		/* Length of received header */
	unsigned short len;		/* Length of received packet */
};

/* Structure used to hold information related to the descriptor
 * and the packet associated with the descriptor (always use
 * use the XGBE_GET_DESC_DATA macro to access this data from the ring)
 */
struct xgbe_ring_data {
	struct xgbe_ring_desc *rdesc;	/* Virtual address of descriptor */
	dma_addr_t rdesc_dma;		/* DMA address of descriptor */

	struct sk_buff *skb;		/* Virtual address of SKB */
	dma_addr_t skb_dma;		/* DMA address of SKB data */
	unsigned int skb_dma_len;	/* Length of SKB DMA area */

	struct xgbe_tx_ring_data tx;	/* Tx-related data */
	struct xgbe_rx_ring_data rx;	/* Rx-related data */

	unsigned int mapped_as_page;

	/* Incomplete receive save location.  If the budget is exhausted
	 * or the last descriptor (last normal descriptor or a following
	 * context descriptor) has not been DMA'd yet the current state
	 * of the receive processing needs to be saved.
	 */
	unsigned int state_saved;
	struct {
		struct sk_buff *skb;
		unsigned int len;
		unsigned int error;
	} state;
};

struct xgbe_ring {
	/* Ring lock - used just for TX rings at the moment */
	spinlock_t lock;

	/* Per packet related information */
	struct xgbe_packet_data packet_data;

	/* Virtual/DMA addresses and count of allocated descriptor memory */
	struct xgbe_ring_desc *rdesc;
	dma_addr_t rdesc_dma;
	unsigned int rdesc_count;

	/* Array of descriptor data corresponding the descriptor memory
	 * (always use the XGBE_GET_DESC_DATA macro to access this data)
	 */
	struct xgbe_ring_data *rdata;

	/* Page allocation for RX buffers */
	struct xgbe_page_alloc rx_hdr_pa;
	struct xgbe_page_alloc rx_buf_pa;
	int node;

	/* Ring index values
	 *  cur   - Tx: index of descriptor to be used for current transfer
	 *          Rx: index of descriptor to check for packet availability
	 *  dirty - Tx: index of descriptor to check for transfer complete
	 *          Rx: index of descriptor to check for buffer reallocation
	 */
	unsigned int cur;
	unsigned int dirty;

	/* Coalesce frame count used for interrupt bit setting */
	unsigned int coalesce_count;

	union {
		struct {
			unsigned int queue_stopped;
			unsigned int xmit_more;
			unsigned short cur_mss;
			unsigned short cur_vlan_ctag;
		} tx;
	};
} ____cacheline_aligned;

/* Structure used to describe the descriptor rings associated with
 * a DMA channel.
 */
struct xgbe_channel {
	char name[16];

	/* Address of private data area for device */
	struct xgbe_prv_data *pdata;

	/* Queue index and base address of queue's DMA registers */
	unsigned int queue_index;
	void __iomem *dma_regs;

	/* Per channel interrupt irq number */
	int dma_irq;
	char dma_irq_name[IFNAMSIZ + 32];

	/* Netdev related settings */
	struct napi_struct napi;

	/* Per channel interrupt enablement tracker */
	unsigned int curr_ier;
	unsigned int saved_ier;

	unsigned int tx_timer_active;
	struct timer_list tx_timer;

	struct xgbe_ring *tx_ring;
	struct xgbe_ring *rx_ring;

	int node;
	cpumask_t affinity_mask;
} ____cacheline_aligned;

enum xgbe_state {
	XGBE_DOWN,
	XGBE_LINK_INIT,
	XGBE_LINK_ERR,
	XGBE_STOPPED,
};

enum xgbe_int {
	XGMAC_INT_DMA_CH_SR_TI,
	XGMAC_INT_DMA_CH_SR_TPS,
	XGMAC_INT_DMA_CH_SR_TBU,
	XGMAC_INT_DMA_CH_SR_RI,
	XGMAC_INT_DMA_CH_SR_RBU,
	XGMAC_INT_DMA_CH_SR_RPS,
	XGMAC_INT_DMA_CH_SR_TI_RI,
	XGMAC_INT_DMA_CH_SR_FBE,
	XGMAC_INT_DMA_ALL,
};

enum xgbe_int_state {
	XGMAC_INT_STATE_SAVE,
	XGMAC_INT_STATE_RESTORE,
};

enum xgbe_ecc_sec {
	XGBE_ECC_SEC_TX,
	XGBE_ECC_SEC_RX,
	XGBE_ECC_SEC_DESC,
};

enum xgbe_speed {
	XGBE_SPEED_1000 = 0,
	XGBE_SPEED_2500,
	XGBE_SPEED_10000,
	XGBE_SPEEDS,
};

enum xgbe_xpcs_access {
	XGBE_XPCS_ACCESS_V1 = 0,
	XGBE_XPCS_ACCESS_V2,
};

enum xgbe_an_mode {
	XGBE_AN_MODE_CL73 = 0,
	XGBE_AN_MODE_CL73_REDRV,
	XGBE_AN_MODE_CL37,
	XGBE_AN_MODE_CL37_SGMII,
	XGBE_AN_MODE_NONE,
};

enum xgbe_an {
	XGBE_AN_READY = 0,
	XGBE_AN_PAGE_RECEIVED,
	XGBE_AN_INCOMPAT_LINK,
	XGBE_AN_COMPLETE,
	XGBE_AN_NO_LINK,
	XGBE_AN_ERROR,
};

enum xgbe_rx {
	XGBE_RX_BPA = 0,
	XGBE_RX_XNP,
	XGBE_RX_COMPLETE,
	XGBE_RX_ERROR,
};

enum xgbe_mode {
	XGBE_MODE_KX_1000 = 0,
	XGBE_MODE_KX_2500,
	XGBE_MODE_KR,
	XGBE_MODE_X,
	XGBE_MODE_SGMII_100,
	XGBE_MODE_SGMII_1000,
	XGBE_MODE_SFI,
	XGBE_MODE_UNKNOWN,
};

enum xgbe_speedset {
	XGBE_SPEEDSET_1000_10000 = 0,
	XGBE_SPEEDSET_2500_10000,
};

enum xgbe_mdio_mode {
	XGBE_MDIO_MODE_NONE = 0,
	XGBE_MDIO_MODE_CL22,
	XGBE_MDIO_MODE_CL45,
};

enum l3_l4_registers {
        MAC_L3_L4_CONTROL = 0,
        MAC_L4_ADDRESS,
        RESERVED1,
        RESERVED2,
        MAC_L3_ADDRESS0,
        MAC_L3_ADDRESS1,
        MAC_L3_ADDRESS2,
        MAC_L3_ADDRESS3
};


struct xgbe_phy {
	struct ethtool_link_ksettings lks;

	int address;

	int autoneg;
	int speed;
	int duplex;

	int link;

	int pause_autoneg;
	int tx_pause;
	int rx_pause;
};

#if XGBE_SRIOV_PF
struct mbox_msg {
	unsigned int cmd;
	unsigned int data[15];
};

struct mbox_msg_mc {
	unsigned int count;
	unsigned short sub_cmd;
	unsigned char mc_addrs[54];
};
#endif

struct xgbe_mmc_stats {
	/* Tx Stats */
	u64 txoctetcount_gb;
	u64 txframecount_gb;
	u64 txbroadcastframes_g;
	u64 txmulticastframes_g;
	u64 tx64octets_gb;
	u64 tx65to127octets_gb;
	u64 tx128to255octets_gb;
	u64 tx256to511octets_gb;
	u64 tx512to1023octets_gb;
	u64 tx1024tomaxoctets_gb;
	u64 txunicastframes_gb;
	u64 txmulticastframes_gb;
	u64 txbroadcastframes_gb;
	u64 txunderflowerror;
	u64 txoctetcount_g;
	u64 txframecount_g;
	u64 txpauseframes;
	u64 txvlanframes_g;

	/* Rx Stats */
	u64 rxframecount_gb;
	u64 rxoctetcount_gb;
	u64 rxoctetcount_g;
	u64 rxbroadcastframes_g;
	u64 rxmulticastframes_g;
	u64 rxcrcerror;
	u64 rxrunterror;
	u64 rxjabbererror;
	u64 rxundersize_g;
	u64 rxoversize_g;
	u64 rx64octets_gb;
	u64 rx65to127octets_gb;
	u64 rx128to255octets_gb;
	u64 rx256to511octets_gb;
	u64 rx512to1023octets_gb;
	u64 rx1024tomaxoctets_gb;
	u64 rxunicastframes_g;
	u64 rxlengtherror;
	u64 rxoutofrangetype;
	u64 rxpauseframes;
	u64 rxfifooverflow;
	u64 rxvlanframes_gb;
	u64 rxwatchdogerror;
};

struct xgbe_ext_stats {
	u64 tx_tso_packets;
	u64 rx_split_header_packets;
	u64 rx_buffer_unavailable;

	u64 txq_packets[XGBE_MAX_DMA_CHANNELS];
	u64 txq_bytes[XGBE_MAX_DMA_CHANNELS];
	u64 rxq_packets[XGBE_MAX_DMA_CHANNELS];
	u64 rxq_bytes[XGBE_MAX_DMA_CHANNELS];

	u64 tx_vxlan_packets;
	u64 rx_vxlan_packets;
	u64 rx_csum_errors;
	u64 rx_vxlan_csum_errors;
};

struct xgbe_hw_if {
	int (*tx_complete)(struct xgbe_ring_desc *);

	int (*set_mac_address)(struct xgbe_prv_data *, u8 *addr);

#if XGBE_SRIOV_PF
	int (*set_mac_address_sriov)(struct xgbe_prv_data *, u8 *addr, int);
	int (*clear_mac_address_sriov)(struct xgbe_prv_data *, int);
	int (*set_promiscuous_mode)(struct xgbe_prv_data *, unsigned int);
	void (*enable_jumbo_frame)(struct xgbe_prv_data *, unsigned int func, unsigned int enable);
#if !ELI_ENABLE
    int (*add_vlan)(struct xgbe_prv_data *, int, int);
	int (*kill_vlan)(struct xgbe_prv_data *, int, int);
#endif
    int (*update_vlan_hash_table_vf)(struct xgbe_prv_data *, int);

	int (*flush_tx_queues_vf)(struct xgbe_prv_data *, int);
	int (*enable_vf_tx_queue)(struct xgbe_prv_data *, int);
	void (*set_mac_hash_table_vf)(struct xgbe_prv_data *);	
    
	int (*read_mmd_regs)(struct xgbe_prv_data *, int, int);
	void (*write_mmd_regs)(struct xgbe_prv_data *, int, int, int);
	int (*set_ext_mii_mode)(struct xgbe_prv_data *, unsigned int,
				enum xgbe_mdio_mode);
	int (*read_ext_mii_regs)(struct xgbe_prv_data *, int, int);
	int (*write_ext_mii_regs)(struct xgbe_prv_data *, int, int, u16);

	int (*set_gpio)(struct xgbe_prv_data *, unsigned int);
	int (*clr_gpio)(struct xgbe_prv_data *, unsigned int);
	void (*powerup_tx)(struct xgbe_prv_data *);
	void (*powerdown_tx)(struct xgbe_prv_data *);
	void (*powerup_rx)(struct xgbe_prv_data *);
	void (*powerdown_rx)(struct xgbe_prv_data *);
#endif

	int (*config_rx_mode)(struct xgbe_prv_data *);

	int (*enable_rx_csum)(struct xgbe_prv_data *);
	int (*disable_rx_csum)(struct xgbe_prv_data *);

	int (*enable_rx_vlan_stripping)(struct xgbe_prv_data *);
	int (*disable_rx_vlan_stripping)(struct xgbe_prv_data *);
	int (*enable_rx_vlan_filtering)(struct xgbe_prv_data *);
	int (*disable_rx_vlan_filtering)(struct xgbe_prv_data *);
	int (*update_vlan_hash_table)(struct xgbe_prv_data *);

	int (*set_speed)(struct xgbe_prv_data *, int);

	void (*enable_tx)(struct xgbe_prv_data *);
	void (*disable_tx)(struct xgbe_prv_data *);
	void (*enable_rx)(struct xgbe_prv_data *);
	void (*disable_rx)(struct xgbe_prv_data *);

	int (*init)(struct xgbe_prv_data *);
	int (*exit)(struct xgbe_prv_data *);

	int (*enable_int)(struct xgbe_channel *, enum xgbe_int);
	int (*disable_int)(struct xgbe_channel *, enum xgbe_int);
	void (*dev_xmit)(struct xgbe_channel *);
	int (*dev_read)(struct xgbe_channel *);
	void (*tx_desc_init)(struct xgbe_channel *);
	void (*rx_desc_init)(struct xgbe_channel *);
	void (*tx_desc_reset)(struct xgbe_ring_data *);
	void (*rx_desc_reset)(struct xgbe_prv_data *, struct xgbe_ring_data *, unsigned int);
	int (*is_last_desc)(struct xgbe_ring_desc *);
	int (*is_context_desc)(struct xgbe_ring_desc *);
	void (*tx_start_xmit)(struct xgbe_channel *, struct xgbe_ring *);

	/* For RX coalescing */
	int (*config_rx_coalesce)(struct xgbe_prv_data *);
	unsigned int (*usec_to_riwt)(struct xgbe_prv_data *, unsigned int);
	unsigned int (*riwt_to_usec)(struct xgbe_prv_data *, unsigned int);

#if XGBE_SRIOV_PF
	/* For FLOW ctrl */
	int (*config_tx_flow_control)(struct xgbe_prv_data *);
	int (*config_rx_flow_control)(struct xgbe_prv_data *);


	/* For RX and TX threshold config */
	int (*config_rx_threshold)(struct xgbe_prv_data *, unsigned int);
	int (*config_tx_threshold)(struct xgbe_prv_data *, unsigned int);

	/* For RX and TX Store and Forward Mode config */
	int (*config_rsf_mode)(struct xgbe_prv_data *, unsigned int);
	int (*config_tsf_mode)(struct xgbe_prv_data *, unsigned int);

	/* For MMC statistics */
	void (*rx_mmc_int)(struct xgbe_prv_data *);
	void (*tx_mmc_int)(struct xgbe_prv_data *);
	void (*read_mmc_stats)(struct xgbe_prv_data *);
#endif

	/* For TX DMA Operate on Second Frame config */
	int (*config_osp_mode)(struct xgbe_prv_data *);

	/* For Timestamp config */
#if XGBE_SRIOV_PF
	int (*config_tstamp)(struct xgbe_prv_data *, unsigned int);
	void (*update_tstamp_addend)(struct xgbe_prv_data *, unsigned int);
	void (*set_tstamp_time)(struct xgbe_prv_data *, unsigned int sec,
				unsigned int nsec);
#else
	int (*config_tstamp)(struct xgbe_prv_data *, unsigned int, unsigned int);
#endif

	u64 (*get_tstamp_time)(struct xgbe_prv_data *);
	u64 (*get_tx_tstamp)(struct xgbe_prv_data *);

#if XGBE_SRIOV_PF
	/* For Data Center Bridging config */
	void (*config_tc)(struct xgbe_prv_data *);
	void (*config_dcb_tc)(struct xgbe_prv_data *);
	void (*config_dcb_pfc)(struct xgbe_prv_data *);
#if !ELI_ENABLE
    /* For Receive Side Scaling */
	int (*enable_rss)(struct xgbe_prv_data *);
	int (*disable_rss)(struct xgbe_prv_data *);
	int (*set_rss_hash_key)(struct xgbe_prv_data *, const u8 *);
	int (*set_rss_lookup_table)(struct xgbe_prv_data *, const u32 *);
#endif
#endif

	/* For VXLAN */
	void (*enable_vxlan)(struct xgbe_prv_data *);
	void (*disable_vxlan)(struct xgbe_prv_data *);
	void (*set_vxlan_id)(struct xgbe_prv_data *);

#if BRCM_FH
	/* For flexible header configuration */
	void (*flex_hdr_set)(struct xgbe_prv_data *, unsigned int, unsigned int);
	void (*set_flex_hdr_length)(struct xgbe_prv_data *, unsigned int);
	void (*set_flex_hdr_position)(struct xgbe_prv_data *, unsigned int);
	void (*enable_link_layer_flex_hdr)(struct xgbe_prv_data *, unsigned int);
	void (*enable_tx_path_flex_hdr)(struct xgbe_prv_data *, unsigned int);
	void (*enable_rx_path_flex_hdr)(struct xgbe_prv_data *, unsigned int);
#endif
};

#if XGBE_SRIOV_VF
/* mailbox options */
struct xgbe_mbx {
	int (*request_irq)(struct xgbe_prv_data *);
	int (*free_irq)(struct xgbe_prv_data *);

	void (*set_mac_addr)(struct xgbe_prv_data *, unsigned char *);
	void (*set_mc_mac_addr)(struct xgbe_prv_data *, unsigned char *);
	void (*clr_mac_addr)(struct xgbe_prv_data *);
	void (*vf_flr_notice)(struct xgbe_prv_data *);
	void (*get_hw_features)(struct xgbe_prv_data *);
	void (*set_promiscuous_mode)(struct xgbe_prv_data *);
	void (*clear_promiscuous_mode)(struct xgbe_prv_data *);
	void (*enable_rx_vlan_stripping)(struct xgbe_prv_data *);
	void (*disable_rx_vlan_stripping)(struct xgbe_prv_data *);
	void (*enable_rx_vlan_filtering)(struct xgbe_prv_data *);
	void (*disable_rx_vlan_filtering)(struct xgbe_prv_data *);
	int (*rx_add_vlan_id)(struct xgbe_prv_data *, unsigned int);
	int (*rx_kill_vlan_id)(struct xgbe_prv_data *, unsigned int);
	int (*config_tstamp)(struct xgbe_prv_data *, u32, u32);
	void (*update_tstamp_addend)(struct xgbe_prv_data *, u32);
	void (*set_tstamp_time)(struct xgbe_prv_data *, unsigned int, unsigned int);
	u64 (*get_tstamp_time)(struct xgbe_prv_data *);

	int (*check_status)(struct xgbe_prv_data *);
    	void (*xgbe_flush_vf_tx_queues) (struct xgbe_prv_data *);
    	void (*enable_vf_tx_queue) (struct xgbe_prv_data *);
	void (*enable_disable_jumbo_frame)(struct xgbe_prv_data *, unsigned);
	void (*enable_disable_rx_csum)(struct xgbe_prv_data *, unsigned);
	void (*set_mc_addrs)(struct xgbe_prv_data *);
	void (*get_phy_link_status)(struct xgbe_prv_data *);
	void (*get_phy_link_speed)(struct xgbe_prv_data *);
};
#endif

struct xgbe_desc_if {
	int (*alloc_ring_resources)(struct xgbe_prv_data *);
	void (*free_ring_resources)(struct xgbe_prv_data *);
	int (*map_tx_skb)(struct xgbe_channel *, struct sk_buff *);
	int (*map_rx_buffer)(struct xgbe_prv_data *, struct xgbe_ring *, struct xgbe_ring_data *);
	void (*unmap_rdata)(struct xgbe_prv_data *, struct xgbe_ring_data *);
	void (*wrapper_tx_desc_init)(struct xgbe_prv_data *);
	void (*wrapper_rx_desc_init)(struct xgbe_prv_data *);
};

#if XGBE_SRIOV_PF
/* This structure represents implementation specific routines for an
 * implementation of a PHY. All routines are required unless noted below.
 *   Optional routines:
 *     kr_training_pre, kr_training_post
 */
struct xgbe_phy_impl_if {
	/* Perform Setup/teardown actions */
	int (*init)(struct xgbe_prv_data *);
	void (*exit)(struct xgbe_prv_data *);

	/* Perform start/stop specific actions */
	int (*reset)(struct xgbe_prv_data *);
	int (*start)(struct xgbe_prv_data *);
	void (*stop)(struct xgbe_prv_data *);

	/* Return the link status */
	int (*link_status)(struct xgbe_prv_data *, int *);

	/* Indicate if a particular speed is valid */
	bool (*valid_speed)(struct xgbe_prv_data *, int);

	/* Check if the specified mode can/should be used */
	bool (*use_mode)(struct xgbe_prv_data *, enum xgbe_mode);
	/* Switch the PHY into various modes */
	void (*set_mode)(struct xgbe_prv_data *, enum xgbe_mode);
	/* Retrieve mode needed for a specific speed */
	enum xgbe_mode (*get_mode)(struct xgbe_prv_data *, int);
	/* Retrieve new/next mode when trying to auto-negotiate */
	enum xgbe_mode (*switch_mode)(struct xgbe_prv_data *);
	/* Retrieve current mode */
	enum xgbe_mode (*cur_mode)(struct xgbe_prv_data *);

	/* Retrieve current auto-negotiation mode */
	enum xgbe_an_mode (*an_mode)(struct xgbe_prv_data *);

	/* Configure auto-negotiation settings */
	int (*an_config)(struct xgbe_prv_data *);

	/* Set/override auto-negotiation advertisement settings */
	void (*an_advertising)(struct xgbe_prv_data *,
			       struct ethtool_link_ksettings *);

	/* Process results of auto-negotiation */
	enum xgbe_mode (*an_outcome)(struct xgbe_prv_data *);

	/* Pre/Post KR training enablement support */
	void (*kr_training_pre)(struct xgbe_prv_data *);
	void (*kr_training_post)(struct xgbe_prv_data *);

	/* Flash Firmware */
	void (*flash_firmware)(struct xgbe_prv_data *, int region, 
			       char* buffer, int size);
	int (*reg_read)(struct xgbe_prv_data* pdata, unsigned int addr, int len, unsigned int* val);
	int (*reg_write)(struct xgbe_prv_data* pdata, unsigned int addr, unsigned int val, int len);

};

struct xgbe_phy_if {
	/* For PHY setup/teardown */
	int (*phy_init)(struct xgbe_prv_data *);
	void (*phy_exit)(struct xgbe_prv_data *);

	/* For PHY support when setting device up/down */
	int (*phy_reset)(struct xgbe_prv_data *);
	int (*phy_start)(struct xgbe_prv_data *);
	void (*phy_stop)(struct xgbe_prv_data *);

	/* For PHY support while device is up */
	void (*phy_status)(struct xgbe_prv_data *);
	int (*phy_config_aneg)(struct xgbe_prv_data *);

	/* For PHY settings validation */
	bool (*phy_valid_speed)(struct xgbe_prv_data *, int);

	/* For single interrupt support */
	irqreturn_t (*an_isr)(struct xgbe_prv_data *);

	/* PHY implementation specific services */
	struct xgbe_phy_impl_if phy_impl;
};

struct xgbe_mbx_if {
	int (*read) (struct xgbe_prv_data *, struct mbox_msg *, unsigned int);
	int (*write) (struct xgbe_prv_data *, struct mbox_msg *, unsigned int, unsigned int);
	int (*prep_data) (struct mbox_msg *, unsigned int *, unsigned int);
    int (*ptp_int_pf_vf) (struct xgbe_prv_data *, u64, unsigned int);    
};

struct xgbe_eli_if {
    int (*mac_add) (struct xgbe_prv_data *, int, unsigned char *);
    int (*mac_del) (struct xgbe_prv_data *, int);
    int (*add_vlan) (struct xgbe_prv_data *, int, int);
    int (*kill_vlan) (struct xgbe_prv_data *, int, int);
    int (*set_promiscuous_mode) (struct xgbe_prv_data *, unsigned int, unsigned int);
    int (*mc_mac_add) (struct xgbe_prv_data *, unsigned char *, unsigned int); 
	int (*mc_mac_del) (struct xgbe_prv_data *, int, int);
	void (*set_all_multicast_mode) (struct xgbe_prv_data *, int, int);    
	int (*eli_init) (struct xgbe_prv_data *);    
	int (*dmach_ram_read) (struct xgbe_prv_data *, int);
	int (*func_ram_read) (struct xgbe_prv_data *, int);
	int (*tcam_ram_read) (struct xgbe_prv_data *, int, char*, int *,int *);
};
#endif

/* This structure contains flags that indicate what hardware features
 * or configurations are present in the device.
 */
struct xgbe_hw_features {
	/* HW Version */
	unsigned int version;

	/* HW Feature Register0 */
	unsigned int gmii;		/* 1000 Mbps support */
	unsigned int vlhash;		/* VLAN Hash Filter */
	unsigned int sma;		/* SMA(MDIO) Interface */
	unsigned int rwk;		/* PMT remote wake-up packet */
	unsigned int mgk;		/* PMT magic packet */
	unsigned int mmc;		/* RMON module */
	unsigned int aoe;		/* ARP Offload */
	unsigned int ts;		/* IEEE 1588-2008 Advanced Timestamp */
	unsigned int eee;		/* Energy Efficient Ethernet */
	unsigned int tx_coe;		/* Tx Checksum Offload */
	unsigned int rx_coe;		/* Rx Checksum Offload */
	unsigned int addn_mac;		/* Additional MAC Addresses */
	unsigned int ts_src;		/* Timestamp Source */
	unsigned int sa_vlan_ins;	/* Source Address or VLAN Insertion */
	unsigned int vxn;		/* VXLAN/NVGRE */

	/* HW Feature Register1 */
	unsigned int rx_fifo_size;	/* MTL Receive FIFO Size */
	unsigned int tx_fifo_size;	/* MTL Transmit FIFO Size */
	unsigned int adv_ts_hi;		/* Advance Timestamping High Word */
	unsigned int dma_width;		/* DMA width */
	unsigned int dcb;		/* DCB Feature */
	unsigned int sph;		/* Split Header Feature */
	unsigned int tso;		/* TCP Segmentation Offload */
	unsigned int dma_debug;		/* DMA Debug Registers */
	unsigned int rss;		/* Receive Side Scaling */
	unsigned int tc_cnt;		/* Number of Traffic Classes */
	unsigned int hash_table_size;	/* Hash Table Size */
	unsigned int l3l4_filter_num;	/* Number of L3-L4 Filters */

	/* HW Feature Register2 */
	unsigned int rx_q_cnt;		/* Number of MTL Receive Queues */
	unsigned int tx_q_cnt;		/* Number of MTL Transmit Queues */
	unsigned int rx_ch_cnt;		/* Number of DMA Receive Channels */
	unsigned int tx_ch_cnt;		/* Number of DMA Transmit Channels */
	unsigned int pps_out_num;	/* Number of PPS outputs */
	unsigned int aux_snap_num;	/* Number of Aux snapshot inputs */
};

struct xgbe_version_data {
#if XGBE_SRIOV_PF
	void (*init_function_ptrs_phy_impl)(struct xgbe_phy_if *);
	unsigned int mmc_64bit;
	unsigned int tx_tstamp_workaround;    
	unsigned int an_cdr_workaround;
#endif
	unsigned int tx_desc_prefetch;
	unsigned int rx_desc_prefetch;
};

struct xgbe_vxlan_data {
	struct list_head list;
	sa_family_t sa_family;
	__be16 port;
};

#if XGBE_SRIOV_VF
struct mbox_msg {
	unsigned int cmd;
	unsigned int data[15];
};

struct mbox_msg_mc {
	unsigned int count;
	unsigned short sub_cmd;
	unsigned char mc_addrs[54];
};
#endif

struct xgbe_prv_data {
	struct net_device *netdev;
	struct pci_dev *pcidev;
#if XGBE_SRIOV_VF
	struct platform_device *platdev;
	struct platform_device *phy_platdev;
#endif
	struct acpi_device *adev;
	struct device *dev;

	struct device *phy_dev;
#if  BRCM_BCMUTIL
    struct miscdevice miscdev;
#endif

	/* L3L4 related */
	struct hlist_head l3l4_filter_list;
	int l3l4_filter_count;

	/* Version related data */
	struct xgbe_version_data *vdata;

	/* ACPI or DT flag */
	unsigned int use_acpi;

	/* XGMAC/XPCS related mmio registers */
	void __iomem *xgmac_regs;	/* XGMAC CSRs */
#if XGBE_SRIOV_VF
	void __iomem *mbox_regs;	/* XGMAC CSRs */
	void __iomem *misc_regs;	/* XGMAC CSRs */
#endif
	void __iomem *xpcs_regs;	/* XPCS MMD registers */
	void __iomem *rxtx_regs;	/* SerDes Rx/Tx CSRs */
	void __iomem *sir0_regs;	/* SerDes integration registers (1/2) */
	void __iomem *sir1_regs;	/* SerDes integration registers (2/2) */
#if XGBE_SRIOV_PF
	void __iomem *xprop_regs;	/* XGBE property registers */
	void __iomem *eli_regs;
	void __iomem *phy_regs;		/* XGMAC PHY Regs base */
#endif

	/* Port property registers */
	unsigned int pp0;
	unsigned int pp1;
	unsigned int pp2;
	unsigned int pp3;
	unsigned int pp4;

	/* Overall device lock */
	spinlock_t lock;

	/* XPCS indirect addressing lock */
	spinlock_t xpcs_lock;
	unsigned int xpcs_window_def_reg;
	unsigned int xpcs_window_sel_reg;
	unsigned int xpcs_window;
	unsigned int xpcs_window_size;
	unsigned int xpcs_window_mask;

	/* RSS addressing mutex */
	struct mutex rss_mutex;

	/* Flags representing xgbe_state */
	unsigned long dev_state;

#if XGBE_SRIOV_VF
	/* ECC support */
	unsigned long tx_sec_period;
	unsigned long tx_ded_period;
	unsigned long rx_sec_period;
	unsigned long rx_ded_period;
	unsigned long desc_sec_period;
	unsigned long desc_ded_period;

	unsigned int tx_sec_count;
	unsigned int tx_ded_count;
	unsigned int rx_sec_count;
	unsigned int rx_ded_count;
	unsigned int desc_ded_count;
	unsigned int desc_sec_count;

	char ecc_name[IFNAMSIZ + 32];

	int mbx_irq;
#endif

	int dev_id;
	int dev_irq;

	int channel_irq[XGBE_MAX_DMA_CHANNELS];

	unsigned int per_channel_irq;
	unsigned int irq_count;
	unsigned int channel_irq_count;
	unsigned int channel_irq_mode;

	struct xgbe_hw_if hw_if;
	struct xgbe_desc_if desc_if;
	
#if XGBE_SRIOV_PF
	struct xgbe_mbx_if mbx_if;
	struct xgbe_phy_if phy_if;
	struct xgbe_eli_if eli_if;
#else
	struct xgbe_mbx mbx;
#endif

    unsigned int pause_quanta;
	/* AXI DMA settings */
	unsigned int coherent;
	unsigned int arcr;
	unsigned int awcr;
	unsigned int awarcr;

	/* Service routine support */
	struct workqueue_struct *dev_workqueue;
	struct work_struct service_work;
	struct timer_list service_timer;

	/* Rings for Tx/Rx on a DMA channel */
	struct xgbe_channel *channel[XGBE_MAX_DMA_CHANNELS];
	unsigned int tx_max_channel_count;
	unsigned int rx_max_channel_count;
	unsigned int channel_count;
	unsigned int tx_ring_count;
	unsigned int tx_desc_count;
	unsigned int rx_ring_count;
	unsigned int rx_desc_count;

	unsigned int new_tx_ring_count;
	unsigned int new_rx_ring_count;

	unsigned int tx_max_q_count;
	unsigned int rx_max_q_count;
	unsigned int tx_q_count;
	unsigned int rx_q_count;

	/* Tx/Rx common settings */
	unsigned int blen;
	unsigned int pbl;
	unsigned int aal;
	unsigned int rd_osr_limit;
	unsigned int wr_osr_limit;

	/* Tx settings */
	unsigned int tx_sf_mode;
	unsigned int tx_threshold;
	unsigned int tx_osp_mode;
	unsigned int tx_max_fifo_size;

	/* Rx settings */
	unsigned int rx_sf_mode;
	unsigned int rx_threshold;
	unsigned int rx_max_fifo_size;

	/* Tx coalescing settings */
	unsigned int tx_usecs;
	unsigned int tx_frames;

	/* Rx coalescing settings */
	unsigned int rx_riwt;
	unsigned int rx_usecs;
	unsigned int rx_frames;

	/* Current Rx buffer size */
	unsigned int rx_buf_size;

	/* Flow control settings */
	unsigned int pause_autoneg;
	unsigned int tx_pause;
	unsigned int rx_pause;
	unsigned int rx_rfa[XGBE_MAX_QUEUES];
	unsigned int rx_rfd[XGBE_MAX_QUEUES];

    unsigned int rq_size[XGBE_MAX_QUEUES];
	spinlock_t phy_access_lock;

	/* Receive Side Scaling settings */
	u8 rss_key[XGBE_RSS_HASH_KEY_SIZE];
	u32 rss_table[XGBE_RSS_MAX_TABLE_SIZE];
	u32 rss_options;

	/* VXLAN settings */
	unsigned int vxlan_port_set;
	unsigned int vxlan_offloads_set;
	unsigned int vxlan_force_disable;
	unsigned int vxlan_port_count;
	struct list_head vxlan_ports;
	u16 vxlan_port;
	netdev_features_t vxlan_features;

	/* Netdev related settings */
	unsigned char mac_addr[ETH_ALEN];
	netdev_features_t netdev_features;
	struct napi_struct napi;
	struct xgbe_mmc_stats mmc_stats;
	struct xgbe_ext_stats ext_stats;

	/* Filtering support */
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];

#if XGBE_SRIOV_PF
	unsigned long active_vf_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif

	/* Device clocks */
	struct clk *sysclk;
	unsigned long sysclk_rate;
	struct clk *ptpclk;
	unsigned long ptpclk_rate;

	/* Timestamp support */
	spinlock_t tstamp_lock;
	struct ptp_clock_info ptp_clock_info;
	struct ptp_clock *ptp_clock;
	struct hwtstamp_config tstamp_config;
	struct cyclecounter tstamp_cc;
	struct timecounter tstamp_tc;
	unsigned int tstamp_addend;
	struct work_struct tx_tstamp_work;
	struct sk_buff *tx_tstamp_skb;
	u64 tx_tstamp;
	
#if XGBE_SRIOV_VF
    unsigned int tx_tstamp_lo;
    unsigned int tx_tstamp_hi;
#endif

	/* DCB support */
	struct ieee_ets *ets;
	struct ieee_pfc *pfc;
	unsigned int q2tc_map[XGBE_MAX_QUEUES];
	unsigned int prio2q_map[IEEE_8021QAZ_MAX_TCS];
	unsigned int pfcq[XGBE_MAX_QUEUES];
	unsigned int pfc_rfa;
	u8 num_tcs;

	/* Hardware features of the device */
	struct xgbe_hw_features hw_feat;

	/* Device work structures */
	struct work_struct restart_work;
	struct work_struct stopdev_work;

	/* Keeps track of power mode */
	unsigned int power_down;

	/* Network interface message level setting */
	u32 msg_enable;

	/* Current PHY settings */
	phy_interface_t phy_mode;
	int phy_link;
	int phy_speed;

	/* MDIO/PHY related settings */
	unsigned int phy_started;
	void *phy_data;
	struct xgbe_phy phy;
	int mdio_mmd;
	unsigned long link_check;
	struct completion mdio_complete;

	unsigned int kr_redrv;

	char an_name[IFNAMSIZ + 32];
	struct workqueue_struct *an_workqueue;

	int an_irq;
	struct work_struct an_irq_work;

	/* Auto-negotiation state machine support */
	unsigned int an_int;
	unsigned int an_status;
	struct mutex an_mutex;
	enum xgbe_an an_result;
	enum xgbe_an an_state;
	enum xgbe_rx kr_state;
	enum xgbe_rx kx_state;
	struct work_struct an_work;
	unsigned int an_again;
	unsigned int an_supported;
	unsigned int parallel_detect;
	unsigned int fec_ability;
	unsigned long an_start;
	enum xgbe_an_mode an_mode;

	unsigned int lpm_ctrl;		/* CTRL1 for resume */

	unsigned int isr_as_tasklet;
	struct tasklet_struct tasklet_dev;
	struct tasklet_struct tasklet_an;

	struct dentry *xgbe_debugfs;

	unsigned int debugfs_xgmac_reg;
	unsigned int debugfs_misc_reg;

	bool debugfs_an_cdr_workaround;
	bool debugfs_an_cdr_track_early;

	/* SR-IOV */
	unsigned int num_vfs;
	unsigned int flags;

    unsigned int bar2_size;
#if XGBE_SRIOV_PF
	unsigned int debugfs_xpcs_mmd;
	unsigned int debugfs_xpcs_reg;
	unsigned int debugfs_xprop_reg;
	void __iomem *mbox_regs;	/* MBOX */
	void __iomem *misc_regs;	/* MISC_cfg */
	int dev_2_host_irq;
	int vf_mbox_irq[SRIOV_MAX_VF];
	struct vf_data_storage *vfinfo;

	unsigned int max_DMA_pf;
	/* fifo size for all vfs */
	unsigned int tx_fifo_size_vfs;
	unsigned int rx_fifo_size_vfs;
	unsigned int vlan_filter_ofs;
	unsigned short last_used_dma;
	unsigned short vlan_filters_used;
	unsigned short ofs_2_vlan_map[32];
	unsigned int rx_csum_semaphore;
    unsigned short eli_pf_vf_bc_ch;
    unsigned int eli_mc_addrs;
    unsigned int occupied_eli[2];
    bool promisc;
	unsigned long mc_indices[BITS_TO_LONGS(ELI_MAC_INDEX + 1)];
	bool pm_mode;
    unsigned int jumbo_frame_enable_cache;
#else
	bool mc_mutex;
	bool uc_mutex;
	struct tasklet_struct tasklet_ecc;
	DECLARE_BITMAP(active_vfs, XGBE_MAX_VFS_DEFAULT);
    unsigned int mac_hfr0;
    unsigned int mac_hfr1;
    unsigned int mac_hfr2;
    unsigned int mac_ver;
#endif

#define XGBE_FLAG_MSI_ENABLED			BIT(1)
#define XGBE_FLAG_MSIX_ENABLED			BIT(3)
#define XGBE_FLAG_RX_1BUF_CAPABLE		BIT(4)
#define XGBE_FLAG_RX_PS_CAPABLE		BIT(5)
#define XGBE_FLAG_RX_PS_ENABLED		BIT(6)
#define XGBE_FLAG_DCA_ENABLED			BIT(8)
#define XGBE_FLAG_DCA_CAPABLE			BIT(9)
#define XGBE_FLAG_IMIR_ENABLED			BIT(10)
#define XGBE_FLAG_MQ_CAPABLE			BIT(11)
#define XGBE_FLAG_DCB_ENABLED			BIT(12)
#define XGBE_FLAG_VMDQ_CAPABLE			BIT(13)
#define XGBE_FLAG_VMDQ_ENABLED			BIT(14)
#define XGBE_FLAG_FAN_FAIL_CAPABLE		BIT(15)
#define XGBE_FLAG_NEED_LINK_UPDATE		BIT(16)
#define XGBE_FLAG_NEED_LINK_CONFIG		BIT(17)
#define XGBE_FLAG_FDIR_HASH_CAPABLE		BIT(18)
#define XGBE_FLAG_FDIR_PERFECT_CAPABLE		BIT(19)
#define XGBE_FLAG_FCOE_CAPABLE			BIT(20)
#define XGBE_FLAG_FCOE_ENABLED			BIT(21)
#define XGBE_FLAG_SRIOV_CAPABLE		BIT(22)
#define XGBE_FLAG_SRIOV_ENABLED		BIT(23)
#define XGBE_FLAG_VXLAN_OFFLOAD_CAPABLE	BIT(24)
#define XGBE_FLAG_RX_HWTSTAMP_ENABLED		BIT(25)
#define XGBE_FLAG_RX_HWTSTAMP_IN_REGISTER	BIT(26)
#define XGBE_FLAG_DCB_CAPABLE			BIT(27)
#define XGBE_FLAG_GENEVE_OFFLOAD_CAPABLE	BIT(28)

};
#if XGBE_SRIOV_PF
struct vf_data_storage {
	struct pci_dev *vfdev;
	unsigned char vf_mac_addresses[ETH_ALEN];
	unsigned int vf_api;
	unsigned short tx_fifo_size;
	unsigned short num_tx_queues;
	unsigned short num_dma_channels;
	unsigned char vf_num;
	unsigned int mac_address_offset;
	unsigned short dma_channel_start_num;
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	unsigned char vlan_strip_filter;
	unsigned int netdev_features;
	unsigned short last_used_dma;
	unsigned short vlan_filters_used;
    bool enabled;
    bool promisc;
	bool pm_mode;
    bool ptp_enabled; 
	unsigned long mc_indices[BITS_TO_LONGS(ELI_MAC_INDEX + 1)];
    unsigned int hash_table[XGBE_MAC_HASH_TABLE_SIZE]; 
};
#endif

/* Function prototypes*/
struct xgbe_prv_data *xgbe_alloc_pdata(struct device *);
void xgbe_free_pdata(struct xgbe_prv_data *);
void xgbe_set_counts(struct xgbe_prv_data *);
int xgbe_config_netdev(struct xgbe_prv_data *);
void xgbe_deconfig_netdev(struct xgbe_prv_data *);

#if XGBE_SRIOV_VF
int xgbe_platform_init(void);
void xgbe_platform_exit(void);
#endif

#ifdef CONFIG_PCI
int xgbe_pci_init(void);
void xgbe_pci_exit(void);
#else
static inline int xgbe_pci_init(void) { return 0; }
static inline void xgbe_pci_exit(void) { }
#endif

void xgbe_init_function_ptrs_dev(struct xgbe_hw_if *);
void xgbe_init_function_ptrs_desc(struct xgbe_desc_if *);

#if XGBE_SRIOV_PF
void xgbe_init_function_ptrs_mbx(struct xgbe_mbx_if *);
void xgbe_init_function_ptrs_phy(struct xgbe_phy_if *);
#if ELI_ENABLE
void xgbe_init_function_ptrs_eli(struct xgbe_eli_if *);
#endif
#else
void xgbe_init_function_ptrs_mbx(struct xgbe_mbx *);
#endif

const struct net_device_ops *xgbe_get_netdev_ops(void);
const struct ethtool_ops *xgbe_get_ethtool_ops(void);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
const struct udp_tunnel_nic_info *xgbe_get_udp_tunnel_info(void);
#endif

#ifdef CONFIG_BCM_XGBE_DCB
const struct dcbnl_rtnl_ops *xgbe_get_dcbnl_ops(void);
#endif

void xgbe_ptp_register(struct xgbe_prv_data *);
void xgbe_ptp_unregister(struct xgbe_prv_data *);
void xgbe_dump_tx_desc(struct xgbe_prv_data *, struct xgbe_ring *,
		       unsigned int, unsigned int, unsigned int);
void xgbe_dump_rx_desc(struct xgbe_prv_data *, struct xgbe_ring *,
		       unsigned int);
void xgbe_print_pkt(struct net_device *, struct sk_buff *, bool);
void xgbe_get_all_hw_features(struct xgbe_prv_data *);
int xgbe_powerup(struct net_device *, unsigned int);
int xgbe_powerdown(struct net_device *, unsigned int);
void xgbe_init_rx_coalesce(struct xgbe_prv_data *);
void xgbe_init_tx_coalesce(struct xgbe_prv_data *);
void xgbe_restart_dev(struct xgbe_prv_data *pdata);
void xgbe_full_restart_dev(struct xgbe_prv_data *pdata);

#ifdef CONFIG_DEBUG_FS
void xgbe_debugfs_init(struct xgbe_prv_data *);
void xgbe_debugfs_exit(struct xgbe_prv_data *);
void xgbe_debugfs_rename(struct xgbe_prv_data *pdata);
#else
static inline void xgbe_debugfs_init(struct xgbe_prv_data *pdata) {}
static inline void xgbe_debugfs_exit(struct xgbe_prv_data *pdata) {}
static inline void xgbe_debugfs_rename(struct xgbe_prv_data *pdata) {}
#endif /* CONFIG_DEBUG_FS */

int xgbe_disable_sriov(struct xgbe_prv_data *pdata);
#if XGBE_SRIOV_PF
int mbox_request_irqs(struct xgbe_prv_data *pdata);
void mbox_free_irqs(struct xgbe_prv_data *pdata);

int xgbe_enable_sriov(struct xgbe_prv_data *pdata, unsigned int max_vfs);
int xgbe_pci_sriov_configure(struct pci_dev *dev, int num_vfs);
#endif

#if BRCM_FH
void xgbe_set_flex_header(struct xgbe_prv_data *, unsigned int, unsigned int);
void xgbe_set_flex_header_length(struct xgbe_prv_data *, unsigned int);
void xgbe_set_flex_header_position(struct xgbe_prv_data *, unsigned int);
void xgbe_enable_link_layer_flex_header(struct xgbe_prv_data *, unsigned int);
void xgbe_enable_tx_path_flex_header(struct xgbe_prv_data *, unsigned int);
void xgbe_enable_rx_path_flex_header(struct xgbe_prv_data *, unsigned int);
#endif

struct l3l4_filter_data {
	__u32 flow_type;
	__be32 ip_src;
	__be32 ip_dst;
	__be16 psrc;
	__be16 pdst;
	__u64 queue;
};

struct l3l4_filter {
	struct hlist_node l3l4_node;
	struct l3l4_filter_data filter;
	struct l3l4_filter_data mask;
	u16 sw_idx;
	u64 action;
};
#define MASK_WIDTH 			31

#define MAC_L3_L4_CONTROL_LHSBM_INDEX 	6
#define MAC_L3_L4_CONTROL_LHDBM_INDEX 	11

#define SRC_IP_FILTER_EN 		0x4
#define DST_IP_FILTER_EN 		0x10
#define SRC_PORT_FILTER_EN 		0x040000
#define DST_PORT_FILTER_EN 		0x0100000
#define TCP_FILTER_EN 			0x000000
#define UDP_FILTER_EN 			0x010000

#define SRC_IP_FILTER_INV_EN    0x8
#define DST_IP_FILTER_INV_EN    0x20
#define SRC_PORT_FILTER_INV_EN  0x080000
#define DST_PORT_FILTER_INV_EN  0x0200000

#define PORT_MASK_INVAL 		65535
#define IP_MASK_NOT_APPLICABLE 		0XFFFFFFFF
#define L3L4_DMA_CH_EN         0x80000000
/* NOTE: Uncomment for function trace log messages in KERNEL LOG */
#define KERNEL_LOG 0

#if KERNEL_LOG
#define YDEBUG
#define YDEBUG_MDIO
#endif

/* For debug prints */
#ifdef YDEBUG
#define DBGPR(x...) pr_alert(x)
#else
#define DBGPR(x...) do { } while (0)
#endif

#ifdef YDEBUG_MDIO
#define DBGPR_MDIO(x...) pr_alert(x)
#else
#define DBGPR_MDIO(x...) do { } while (0)
#endif

#endif
