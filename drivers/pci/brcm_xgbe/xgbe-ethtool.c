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

#include <linux/spinlock.h>
#include <linux/phy.h>
#include <linux/net_tstamp.h>
#include <linux/version.h>
#include <linux/firmware.h>

#include "xgbe.h"
#include "xgbe-common.h"

struct xgbe_stats {
	char stat_string[ETH_GSTRING_LEN];
	int stat_size;
	int stat_offset;
};
/* Currently supported kernel versions are 4.15.x and 5.4.x */
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)))

#define XGMAC_MMC_STAT(_string, _var)				\
	{ _string,						\
		FIELD_SIZEOF(struct xgbe_mmc_stats, _var),		\
		offsetof(struct xgbe_prv_data, mmc_stats._var),	\
	}

#define XGMAC_EXT_STAT(_string, _var)				\
	{ _string,						\
		FIELD_SIZEOF(struct xgbe_ext_stats, _var),		\
		offsetof(struct xgbe_prv_data, ext_stats._var),	\
	}
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(6, 16, 0)))
#define XGMAC_MMC_STAT(_string, _var)				\
	{ _string,						\
		sizeof_field(struct xgbe_mmc_stats, _var),	\
		offsetof(struct xgbe_prv_data, mmc_stats._var),	\
	}

#define XGMAC_EXT_STAT(_string, _var)				\
	{ _string,						\
		sizeof_field(struct xgbe_ext_stats, _var),	\
		offsetof(struct xgbe_prv_data, ext_stats._var),	\
	}
#endif

static const struct xgbe_stats xgbe_gstring_stats[] = {
	XGMAC_MMC_STAT("tx_bytes", txoctetcount_gb),
	XGMAC_MMC_STAT("tx_packets", txframecount_gb),
	XGMAC_MMC_STAT("tx_unicast_packets", txunicastframes_gb),
	XGMAC_MMC_STAT("tx_broadcast_packets", txbroadcastframes_gb),
	XGMAC_MMC_STAT("tx_multicast_packets", txmulticastframes_gb),
	XGMAC_MMC_STAT("tx_vlan_packets", txvlanframes_g),
	XGMAC_EXT_STAT("tx_vxlan_packets", tx_vxlan_packets),
	XGMAC_EXT_STAT("tx_tso_packets", tx_tso_packets),
	XGMAC_MMC_STAT("tx_64_byte_packets", tx64octets_gb),
	XGMAC_MMC_STAT("tx_65_to_127_byte_packets", tx65to127octets_gb),
	XGMAC_MMC_STAT("tx_128_to_255_byte_packets", tx128to255octets_gb),
	XGMAC_MMC_STAT("tx_256_to_511_byte_packets", tx256to511octets_gb),
	XGMAC_MMC_STAT("tx_512_to_1023_byte_packets", tx512to1023octets_gb),
	XGMAC_MMC_STAT("tx_1024_to_max_byte_packets", tx1024tomaxoctets_gb),
	XGMAC_MMC_STAT("tx_underflow_errors", txunderflowerror),
	XGMAC_MMC_STAT("tx_pause_frames", txpauseframes),

	XGMAC_MMC_STAT("rx_bytes", rxoctetcount_gb),
	XGMAC_MMC_STAT("rx_packets", rxframecount_gb),
	XGMAC_MMC_STAT("rx_unicast_packets", rxunicastframes_g),
	XGMAC_MMC_STAT("rx_broadcast_packets", rxbroadcastframes_g),
	XGMAC_MMC_STAT("rx_multicast_packets", rxmulticastframes_g),
	XGMAC_MMC_STAT("rx_vlan_packets", rxvlanframes_gb),
	XGMAC_EXT_STAT("rx_vxlan_packets", rx_vxlan_packets),
	XGMAC_MMC_STAT("rx_64_byte_packets", rx64octets_gb),
	XGMAC_MMC_STAT("rx_65_to_127_byte_packets", rx65to127octets_gb),
	XGMAC_MMC_STAT("rx_128_to_255_byte_packets", rx128to255octets_gb),
	XGMAC_MMC_STAT("rx_256_to_511_byte_packets", rx256to511octets_gb),
	XGMAC_MMC_STAT("rx_512_to_1023_byte_packets", rx512to1023octets_gb),
	XGMAC_MMC_STAT("rx_1024_to_max_byte_packets", rx1024tomaxoctets_gb),
	XGMAC_MMC_STAT("rx_undersize_packets", rxundersize_g),
	XGMAC_MMC_STAT("rx_oversize_packets", rxoversize_g),
	XGMAC_MMC_STAT("rx_crc_errors", rxcrcerror),
	XGMAC_MMC_STAT("rx_crc_errors_small_packets", rxrunterror),
	XGMAC_MMC_STAT("rx_crc_errors_giant_packets", rxjabbererror),
	XGMAC_MMC_STAT("rx_length_errors", rxlengtherror),
	XGMAC_MMC_STAT("rx_out_of_range_errors", rxoutofrangetype),
	XGMAC_MMC_STAT("rx_fifo_overflow_errors", rxfifooverflow),
	XGMAC_MMC_STAT("rx_watchdog_errors", rxwatchdogerror),
	XGMAC_EXT_STAT("rx_csum_errors", rx_csum_errors),
	XGMAC_EXT_STAT("rx_vxlan_csum_errors", rx_vxlan_csum_errors),
	XGMAC_MMC_STAT("rx_pause_frames", rxpauseframes),
	XGMAC_EXT_STAT("rx_split_header_packets", rx_split_header_packets),
	XGMAC_EXT_STAT("rx_buffer_unavailable", rx_buffer_unavailable),
};

#define XGBE_STATS_COUNT	ARRAY_SIZE(xgbe_gstring_stats)

static void xgbe_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	int i;

	switch (stringset) {
		case ETH_SS_STATS:
			for (i = 0; i < XGBE_STATS_COUNT; i++) {
				memcpy(data, xgbe_gstring_stats[i].stat_string,
						ETH_GSTRING_LEN);
				data += ETH_GSTRING_LEN;
			}
			for (i = 0; i < pdata->tx_ring_count; i++) {
				sprintf(data, "txq_%u_packets", i);
				data += ETH_GSTRING_LEN;
				sprintf(data, "txq_%u_bytes", i);
				data += ETH_GSTRING_LEN;
			}
			for (i = 0; i < pdata->rx_ring_count; i++) {
				sprintf(data, "rxq_%u_packets", i);
				data += ETH_GSTRING_LEN;
				sprintf(data, "rxq_%u_bytes", i);
				data += ETH_GSTRING_LEN;
			}
			break;
	}
}

static void xgbe_get_ethtool_stats(struct net_device *netdev,
		struct ethtool_stats *stats, u64 *data)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	u8 *stat;
	int i;
#if XGBE_SRIOV_PF
	pdata->hw_if.read_mmc_stats(pdata);
#endif
	for (i = 0; i < XGBE_STATS_COUNT; i++) {
		stat = (u8 *)pdata + xgbe_gstring_stats[i].stat_offset;
		*data++ = *(u64 *)stat;
	}
	for (i = 0; i < pdata->tx_ring_count; i++) {
		*data++ = pdata->ext_stats.txq_packets[i];
		*data++ = pdata->ext_stats.txq_bytes[i];
	}
	for (i = 0; i < pdata->rx_ring_count; i++) {
		*data++ = pdata->ext_stats.rxq_packets[i];
		*data++ = pdata->ext_stats.rxq_bytes[i];
	}
}

static int xgbe_get_sset_count(struct net_device *netdev, int stringset)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	int ret;

	switch (stringset) {
		case ETH_SS_STATS:
			ret = XGBE_STATS_COUNT +
				(pdata->tx_ring_count * 2) +
				(pdata->rx_ring_count * 2);
			break;

		default:
			ret = -EOPNOTSUPP;
	}

	return ret;
}

#if XGBE_SRIOV_PF
static void xgbe_get_pauseparam(struct net_device *netdev,
		struct ethtool_pauseparam *pause)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	pause->autoneg = pdata->phy.pause_autoneg;
	pause->tx_pause = pdata->tx_pause;
	pause->rx_pause = pdata->rx_pause;
}

static int xgbe_set_pauseparam(struct net_device *netdev,
		struct ethtool_pauseparam *pause)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	struct ethtool_link_ksettings *lks = &pdata->phy.lks;
	int ret = 0;

	if (pause->autoneg && (pdata->phy.autoneg != AUTONEG_ENABLE)) {
		netdev_err(netdev,
				"autoneg disabled, pause autoneg not available\n");
		return -EINVAL;
	}

	pdata->phy.pause_autoneg = pause->autoneg;
	if (pdata->tx_pause != pause->tx_pause) {
		pdata->tx_pause = pause->tx_pause;
		pdata->hw_if.config_tx_flow_control(pdata);
        }

        if (pdata->rx_pause != pause->rx_pause) {
                pdata->rx_pause = pause->rx_pause;
                pdata->hw_if.config_rx_flow_control(pdata);
        }

	XGBE_CLR_ADV(lks, Pause);
	XGBE_CLR_ADV(lks, Asym_Pause);

	if (pause->rx_pause) {
		XGBE_SET_ADV(lks, Pause);
		XGBE_SET_ADV(lks, Asym_Pause);
	}

	if (pause->tx_pause) {
		/* Equivalent to XOR of Asym_Pause */
		if (XGBE_ADV(lks, Asym_Pause))
			XGBE_CLR_ADV(lks, Asym_Pause);
		else
			XGBE_SET_ADV(lks, Asym_Pause);
	}

	return ret;
}
#endif

static int xgbe_get_link_ksettings(struct net_device *netdev,
		struct ethtool_link_ksettings *cmd)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	struct ethtool_link_ksettings *lks = &pdata->phy.lks;

	cmd->base.phy_address = pdata->phy.address;
	cmd->base.autoneg =  pdata->phy.autoneg ? AUTONEG_ENABLE : AUTONEG_DISABLE;

 #if XGBE_SRIOV_PF
	if(pdata->dev_id == BCM8989X_PF_ID) {
    	if(pdata->phy.link) {	
			switch(pdata->phy.speed)
   			{
   	  			case 0: cmd->base.speed = SPEED_10000; break;
   	  			case 1: cmd->base.speed = SPEED_5000; break;
				case 2: cmd->base.speed = SPEED_2500; break;
				default:cmd->base.speed = SPEED_UNKNOWN; break;
			}
   		} else {
			cmd->base.speed = SPEED_UNKNOWN;
		}
	} else {
   		cmd->base.speed = pdata->phy.speed;
	}
	
	cmd->base.duplex = pdata->phy.duplex;/* pdata->phy.duplex; */   		
 #endif
 
 #if XGBE_SRIOV_VF
 	pdata->mbx.get_phy_link_speed(pdata);
   	cmd->base.speed = pdata->phy.speed;
	cmd->base.duplex = DUPLEX_FULL;
 #endif

	cmd->base.port = PORT_MII;
	cmd->base.transceiver = XCVR_EXTERNAL;

	XGBE_LM_COPY(cmd, supported, lks, supported);
	XGBE_LM_COPY(cmd, advertising, lks, advertising);
	XGBE_LM_COPY(cmd, lp_advertising, lks, lp_advertising);

	return 0;
}

static int xgbe_set_link_ksettings(struct net_device *netdev,
		const struct ethtool_link_ksettings *cmd)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	struct ethtool_link_ksettings *lks = &pdata->phy.lks;
	__ETHTOOL_DECLARE_LINK_MODE_MASK(advertising);
	u32 speed;
	int ret;

	speed = cmd->base.speed;
	
	if (cmd->base.phy_address != pdata->phy.address) {
		netdev_err(netdev, "invalid phy address %hhu\n",
				cmd->base.phy_address);
		return -EINVAL;
	}

	if ((cmd->base.autoneg != AUTONEG_ENABLE) &&
			(cmd->base.autoneg != AUTONEG_DISABLE)) {
		netdev_err(netdev, "unsupported autoneg %hhu\n",
				cmd->base.autoneg);
		return -EINVAL;
	}

	if (cmd->base.autoneg == AUTONEG_DISABLE) {
#if XGBE_SRIOV_PF
		if (!pdata->phy_if.phy_impl.valid_speed(pdata, speed)) {
			netdev_err(netdev, "unsupported speed %u\n", speed);
			return -EINVAL;
		}
#endif
#if 0
		if (cmd->base.duplex != DUPLEX_FULL) {
			netdev_err(netdev, "unsupported duplex %hhu\n",
					cmd->base.duplex);
			return -EINVAL;
		}
#endif
	}

	netif_dbg(pdata, link, netdev,
			"requested advertisement 0x%*pb, phy supported 0x%*pb\n",
			__ETHTOOL_LINK_MODE_MASK_NBITS, cmd->link_modes.advertising,
			__ETHTOOL_LINK_MODE_MASK_NBITS, lks->link_modes.supported);

	bitmap_and(advertising,
			cmd->link_modes.advertising, lks->link_modes.supported,
			__ETHTOOL_LINK_MODE_MASK_NBITS);

	if ((cmd->base.autoneg == AUTONEG_ENABLE) &&
			bitmap_empty(advertising, __ETHTOOL_LINK_MODE_MASK_NBITS)) {
		netdev_err(netdev,
				"unsupported requested advertisement\n");
		return -EINVAL;
	}


	ret = 0;
	pdata->phy.autoneg = cmd->base.autoneg;
	pdata->phy.speed = speed;
	pdata->phy.duplex = cmd->base.duplex;

#if XGBE_SRIOV_PF
	if(pdata->dev_id == BCM8989X_PF_ID) {

		//Check if there is any change in advertising
		bitmap_xor(advertising, cmd->link_modes.advertising, 
			lks->link_modes.advertising, __ETHTOOL_LINK_MODE_MASK_NBITS);

		if(!bitmap_empty(advertising, __ETHTOOL_LINK_MODE_MASK_NBITS))
			pdata->phy_if.phy_impl.an_advertising(pdata, (struct ethtool_link_ksettings*)cmd);

		ret = pdata->phy_if.phy_impl.an_config(pdata); 
	} else {
		bitmap_copy(lks->link_modes.advertising, advertising,
			__ETHTOOL_LINK_MODE_MASK_NBITS);
	}
#endif

	if (cmd->base.autoneg == AUTONEG_ENABLE)
		XGBE_SET_ADV(lks, Autoneg);
	else
		XGBE_CLR_ADV(lks, Autoneg);

	return ret;
}

static void xgbe_get_drvinfo(struct net_device *netdev,
		struct ethtool_drvinfo *drvinfo)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	struct xgbe_hw_features *hw_feat = &pdata->hw_feat;

	strlcpy(drvinfo->driver, XGBE_DRV_NAME, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, XGBE_DRV_VERSION, sizeof(drvinfo->version));
	strlcpy(drvinfo->bus_info, dev_name(pdata->dev),
			sizeof(drvinfo->bus_info));
	snprintf(drvinfo->fw_version, sizeof(drvinfo->fw_version), "%d.%d.%d",
			XGMAC_GET_BITS(hw_feat->version, MAC_VR, USERVER),
			XGMAC_GET_BITS(hw_feat->version, MAC_VR, DEVID),
			XGMAC_GET_BITS(hw_feat->version, MAC_VR, SNPSVER));
}

static u32 xgbe_get_msglevel(struct net_device *netdev)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	return pdata->msg_enable;
}

static void xgbe_set_msglevel(struct net_device *netdev, u32 msglevel)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	pdata->msg_enable = msglevel;
}

static int xgbe_get_coalesce(struct net_device *netdev,
		struct ethtool_coalesce *ec)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	memset(ec, 0, sizeof(struct ethtool_coalesce));

	ec->rx_coalesce_usecs = pdata->rx_usecs;
	ec->rx_max_coalesced_frames = pdata->rx_frames;

	ec->tx_max_coalesced_frames = pdata->tx_frames;

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static int xgbe_get_coalesce_args(struct net_device *netdev,
			     struct ethtool_coalesce *ec,
			     struct kernel_ethtool_coalesce *kernel_coal,
			     struct netlink_ext_ack *extack)
{
	return xgbe_get_coalesce(netdev, ec);
}
#endif

static int xgbe_set_coalesce(struct net_device *netdev,
		struct ethtool_coalesce *ec)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	unsigned int rx_frames, rx_riwt, rx_usecs;
	unsigned int tx_frames;

	/* Check for not supported parameters  */
	if ((ec->rx_coalesce_usecs_irq) ||
			(ec->rx_max_coalesced_frames_irq) ||
			(ec->tx_coalesce_usecs) ||
			(ec->tx_coalesce_usecs_irq) ||
			(ec->tx_max_coalesced_frames_irq) ||
			(ec->stats_block_coalesce_usecs) ||
			(ec->use_adaptive_rx_coalesce) ||
			(ec->use_adaptive_tx_coalesce) ||
			(ec->pkt_rate_low) ||
			(ec->rx_coalesce_usecs_low) ||
			(ec->rx_max_coalesced_frames_low) ||
			(ec->tx_coalesce_usecs_low) ||
			(ec->tx_max_coalesced_frames_low) ||
			(ec->pkt_rate_high) ||
			(ec->rx_coalesce_usecs_high) ||
			(ec->rx_max_coalesced_frames_high) ||
			(ec->tx_coalesce_usecs_high) ||
			(ec->tx_max_coalesced_frames_high) ||
			(ec->rate_sample_interval)) {
		netdev_err(netdev, "unsupported coalescing parameter\n");
		return -EOPNOTSUPP;
	}

	rx_riwt = hw_if->usec_to_riwt(pdata, ec->rx_coalesce_usecs);
	rx_usecs = ec->rx_coalesce_usecs;
	rx_frames = ec->rx_max_coalesced_frames;

	/* Use smallest possible value if conversion resulted in zero */
	if (rx_usecs && !rx_riwt)
		rx_riwt = 1;

	/* Check the bounds of values for Rx */
	if (rx_riwt > XGMAC_MAX_DMA_RIWT) {
		netdev_err(netdev, "rx-usec is limited to %d usecs\n",
				hw_if->riwt_to_usec(pdata, XGMAC_MAX_DMA_RIWT));
		return -EINVAL;
	}
	if (rx_frames > pdata->rx_desc_count) {
		netdev_err(netdev, "rx-frames is limited to %d frames\n",
				pdata->rx_desc_count);
		return -EINVAL;
	}

	tx_frames = ec->tx_max_coalesced_frames;

	/* Check the bounds of values for Tx */
	if (tx_frames > pdata->tx_desc_count) {
		netdev_err(netdev, "tx-frames is limited to %d frames\n",
				pdata->tx_desc_count);
		return -EINVAL;
	}

	pdata->rx_riwt = rx_riwt;
	pdata->rx_usecs = rx_usecs;
	pdata->rx_frames = rx_frames;
	hw_if->config_rx_coalesce(pdata);

	pdata->tx_frames = tx_frames;
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static int xgbe_set_coalesce_args(struct net_device *netdev,
			     struct ethtool_coalesce *ec,
			     struct kernel_ethtool_coalesce *kernel_coal,
			     struct netlink_ext_ack *extack)
{
	return xgbe_set_coalesce(netdev, ec);
}
#endif

static int xgbe_l3l4_get_rule(struct xgbe_prv_data *pdata,struct ethtool_rxnfc *rxnfc)
{
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&rxnfc->fs;
	struct hlist_node *node2;
	struct l3l4_filter *rule = NULL;

	/* Report total number of rules*/
	rxnfc->data = XGMAC_IOREAD_BITS(pdata, MAC_HWF1R, L3L4FNUM); 

	hlist_for_each_entry_safe(rule, node2,
				&pdata->l3l4_filter_list, l3l4_node) {
		if (fsp->location <= rule->sw_idx)
			break;
	}

	if(!rule || fsp->location != rule->sw_idx)
		return -EINVAL;

	switch(rule->filter.flow_type) {
		case TCP_V4_FLOW:
			fsp->flow_type = TCP_V4_FLOW;
			fsp->h_u.tcp_ip4_spec.psrc   = htons(rule->filter.psrc);	
			fsp->h_u.tcp_ip4_spec.pdst   = htons(rule->filter.pdst);	
			fsp->h_u.tcp_ip4_spec.ip4src = htonl(rule->filter.ip_src);
			fsp->h_u.tcp_ip4_spec.ip4dst = htonl(rule->filter.ip_dst);
			fsp->m_u.tcp_ip4_spec.psrc   = htons(rule->mask.psrc);	
			fsp->m_u.tcp_ip4_spec.pdst   = htons(rule->mask.pdst);	
			fsp->m_u.tcp_ip4_spec.ip4src = htonl(rule->mask.ip_src);
			fsp->m_u.tcp_ip4_spec.ip4dst = htonl(rule->mask.ip_dst);
			fsp->ring_cookie 	     = rule->filter.queue;
			break;
		case UDP_V4_FLOW:
			fsp->flow_type = UDP_V4_FLOW;
			fsp->h_u.udp_ip4_spec.psrc   = 	htons(rule->filter.psrc);	
			fsp->h_u.udp_ip4_spec.pdst   = 	htons(rule->filter.pdst);	
			fsp->h_u.udp_ip4_spec.ip4src =  htonl(rule->filter.ip_src);	
			fsp->h_u.udp_ip4_spec.ip4dst =  htonl(rule->filter.ip_dst);	
			fsp->m_u.udp_ip4_spec.psrc   = 	htons(rule->mask.psrc);	
			fsp->m_u.udp_ip4_spec.pdst   = 	htons(rule->mask.pdst);	
			fsp->m_u.udp_ip4_spec.ip4src =  htonl(rule->mask.ip_src);	
			fsp->m_u.udp_ip4_spec.ip4dst =  htonl(rule->mask.ip_dst);	
			fsp->ring_cookie             =  rule->filter.queue;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}


void read_l3l4_registers(struct xgbe_prv_data *pdata, int register_number, int register_type)
{


        // Set TT field to 1
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, TT, 1);

        // Select fileter number
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, IDDR_NUM, register_number);

        // write register type into IDDR
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, IDDR_REGSEL, register_type);


        // Set XB field to 1
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, XB, 1);

        while(XGMAC_IOREAD_BITS(pdata, MAC_L3L4ACTL, XB));
}

static int xgbe_l3l4_get_all_rules(struct xgbe_prv_data *pdata, struct ethtool_rxnfc *rxnfc, u32 *rule_locs)
{

	struct hlist_node *node2;
	struct l3l4_filter *rule;
	int cnt = 0;

	/* Report total number of rules*/
	rxnfc->data = XGMAC_IOREAD_BITS(pdata, MAC_HWF1R, L3L4FNUM); 
	
	hlist_for_each_entry_safe(rule, node2,
			&pdata->l3l4_filter_list, l3l4_node) {
		if(cnt == rxnfc->rule_cnt) {
			return -EMSGSIZE;
		}
		rule_locs[cnt] = rule->sw_idx;
		cnt++;
	}
	rxnfc->rule_cnt = cnt;

	return 0;
}

static int xgbe_get_rxnfc(struct net_device *netdev,
		struct ethtool_rxnfc *rxnfc, u32 *rule_locs)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	int ret = -EOPNOTSUPP;

	switch (rxnfc->cmd) {
		case ETHTOOL_GRXRINGS:
			rxnfc->data = pdata->rx_ring_count;
			ret = 0;
			break;
		case ETHTOOL_GRXCLSRLCNT:
			rxnfc->rule_cnt = pdata->l3l4_filter_count;
			ret = 0;
			break;
		case ETHTOOL_GRXCLSRULE:
			ret = xgbe_l3l4_get_rule(pdata, rxnfc);
			break;
		case ETHTOOL_GRXCLSRLALL:
			ret = xgbe_l3l4_get_all_rules(pdata, rxnfc, rule_locs);
			break;
		default:
			break;
	}

	return ret;
}

static u32 xgbe_get_rxfh_key_size(struct net_device *netdev)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	return sizeof(pdata->rss_key);
}

static u32 xgbe_get_rxfh_indir_size(struct net_device *netdev)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	return ARRAY_SIZE(pdata->rss_table);
}

static int xgbe_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key,
		u8 *hfunc)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	unsigned int i;

	if (indir) {
		for (i = 0; i < ARRAY_SIZE(pdata->rss_table); i++)
			indir[i] = XGMAC_GET_BITS(pdata->rss_table[i],
					MAC_RSSDR, DMCH);
	}

	if (key)
		memcpy(key, pdata->rss_key, sizeof(pdata->rss_key));

	if (hfunc)
		*hfunc = ETH_RSS_HASH_TOP;

	return 0;
}

static int xgbe_set_rxfh(struct net_device *netdev, const u32 *indir,
		const u8 *key, const u8 hfunc)
{
	if (hfunc != ETH_RSS_HASH_NO_CHANGE && hfunc != ETH_RSS_HASH_TOP) {
		netdev_err(netdev, "unsupported hash function\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int xgbe_get_ts_info(struct net_device *netdev,
		struct ethtool_ts_info *ts_info)
{
#if XGBE_SRIOV_PF
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
#endif
	ts_info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
		SOF_TIMESTAMPING_RX_SOFTWARE |
		SOF_TIMESTAMPING_SOFTWARE |
		SOF_TIMESTAMPING_TX_HARDWARE |
		SOF_TIMESTAMPING_RX_HARDWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;

#if XGBE_SRIOV_PF
	if (pdata->ptp_clock)
		ts_info->phc_index = ptp_clock_index(pdata->ptp_clock);
	else
#endif
		ts_info->phc_index = -1;

	ts_info->tx_types = (1 << HWTSTAMP_TX_OFF) | (1 << HWTSTAMP_TX_ON);
	ts_info->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
		(1 << HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
		(1 << HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
		(1 << HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
		(1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
		(1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
		(1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
		(1 << HWTSTAMP_FILTER_PTP_V2_EVENT) |
		(1 << HWTSTAMP_FILTER_PTP_V2_SYNC) |
		(1 << HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
		(1 << HWTSTAMP_FILTER_ALL);

	return 0;
}

static int xgbe_get_module_info(struct net_device *netdev,
		struct ethtool_modinfo *modinfo)
{
	return 0;
}

static int xgbe_get_module_eeprom(struct net_device *netdev,
		struct ethtool_eeprom *eeprom, u8 *data)
{
	return 0;
}

static void xgbe_get_ringparam(struct net_device *netdev,
		struct ethtool_ringparam *ringparam,
	    struct kernel_ethtool_ringparam *kernel_ringparam,
		struct netlink_ext_ack *extack)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	ringparam->rx_max_pending = XGBE_RX_DESC_CNT_MAX;
	ringparam->tx_max_pending = XGBE_TX_DESC_CNT_MAX;
	ringparam->rx_pending = pdata->rx_desc_count;
	ringparam->tx_pending = pdata->tx_desc_count;
}

static int xgbe_set_ringparam(struct net_device *netdev,
		struct ethtool_ringparam *ringparam,
	    struct kernel_ethtool_ringparam *kernel_ringparam,
		struct netlink_ext_ack *extack)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	unsigned int rx, tx;

	if (ringparam->rx_mini_pending || ringparam->rx_jumbo_pending) {
		netdev_err(netdev, "unsupported ring parameter\n");
		return -EINVAL;
	}

	if ((ringparam->rx_pending < XGBE_RX_DESC_CNT_MIN) ||
			(ringparam->rx_pending > XGBE_RX_DESC_CNT_MAX)) {
		netdev_err(netdev,
				"rx ring parameter must be between %u and %u\n",
				XGBE_RX_DESC_CNT_MIN, XGBE_RX_DESC_CNT_MAX);
		return -EINVAL;
	}

	if ((ringparam->tx_pending < XGBE_TX_DESC_CNT_MIN) ||
			(ringparam->tx_pending > XGBE_TX_DESC_CNT_MAX)) {
		netdev_err(netdev,
				"tx ring parameter must be between %u and %u\n",
				XGBE_TX_DESC_CNT_MIN, XGBE_TX_DESC_CNT_MAX);
		return -EINVAL;
	}

	rx = __rounddown_pow_of_two(ringparam->rx_pending);
	if (rx != ringparam->rx_pending)
		netdev_notice(netdev,
				"rx ring parameter rounded to power of two: %u\n",
				rx);

	tx = __rounddown_pow_of_two(ringparam->tx_pending);
	if (tx != ringparam->tx_pending)
		netdev_notice(netdev,
				"tx ring parameter rounded to power of two: %u\n",
				tx);

	if ((rx == pdata->rx_desc_count) &&
			(tx == pdata->tx_desc_count))
		goto out;

	pdata->rx_desc_count = rx;
	pdata->tx_desc_count = tx;

	xgbe_restart_dev(pdata);

out:
	return 0;
}

static void xgbe_get_channels(struct net_device *netdev,
		struct ethtool_channels *channels)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	unsigned int rx, tx, combined;

	/* Calculate maximums allowed:
	 *   - Take into account the number of available IRQs
	 *   - Do not take into account the number of online CPUs so that
	 *     the user can over-subscribe if desired
	 *   - Tx is additionally limited by the number of hardware queues
	 */
	rx = min(pdata->hw_feat.rx_ch_cnt, pdata->rx_max_channel_count);
	rx = min(rx, pdata->channel_irq_count);
	tx = min(pdata->hw_feat.tx_ch_cnt, pdata->tx_max_channel_count);
	tx = min(tx, pdata->channel_irq_count);
	tx = min(tx, pdata->tx_max_q_count);

	combined = min(rx, tx);

	channels->max_combined = combined;
	channels->max_rx = rx ? rx - 1 : 0;
	channels->max_tx = tx ? tx - 1 : 0;

	/* Get current settings based on device state */
	rx = pdata->new_rx_ring_count ? : pdata->rx_ring_count;
	tx = pdata->new_tx_ring_count ? : pdata->tx_ring_count;

	combined = min(rx, tx);
	rx -= combined;
	tx -= combined;

	channels->combined_count = combined;
	channels->rx_count = rx;
	channels->tx_count = tx;
}

static void xgbe_print_set_channels_input(struct net_device *netdev,
		struct ethtool_channels *channels)
{
	netdev_err(netdev, "channel inputs: combined=%u, rx-only=%u, tx-only=%u\n",
			channels->combined_count, channels->rx_count,
			channels->tx_count);
}

static int xgbe_set_channels(struct net_device *netdev,
		struct ethtool_channels *channels)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	unsigned int rx, rx_curr, tx, tx_curr, combined;

	/* Calculate maximums allowed:
	 *   - Take into account the number of available IRQs
	 *   - Do not take into account the number of online CPUs so that
	 *     the user can over-subscribe if desired
	 *   - Tx is additionally limited by the number of hardware queues
	 */
	rx = min(pdata->hw_feat.rx_ch_cnt, pdata->rx_max_channel_count);
	rx = min(rx, pdata->channel_irq_count);
	tx = min(pdata->hw_feat.tx_ch_cnt, pdata->tx_max_channel_count);
	tx = min(tx, pdata->tx_max_q_count);
	tx = min(tx, pdata->channel_irq_count);

	combined = min(rx, tx);

	/* Should not be setting other count */
	if (channels->other_count) {
		netdev_err(netdev,
				"other channel count must be zero\n");
		return -EINVAL;
	}

	/* Require at least one Combined (Rx and Tx) channel */
	if (!channels->combined_count) {
		netdev_err(netdev,
				"at least one combined Rx/Tx channel is required\n");
		xgbe_print_set_channels_input(netdev, channels);
		return -EINVAL;
	}

	/* Check combined channels */
	if (channels->combined_count > combined) {
		netdev_err(netdev,
				"combined channel count cannot exceed %u\n",
				combined);
		xgbe_print_set_channels_input(netdev, channels);
		return -EINVAL;
	}

	/* Can have some Rx-only or Tx-only channels, but not both */
	if (channels->rx_count && channels->tx_count) {
		netdev_err(netdev,
				"cannot specify both Rx-only and Tx-only channels\n");
		xgbe_print_set_channels_input(netdev, channels);
		return -EINVAL;
	}

	/* Check that we don't exceed the maximum number of channels */
	if ((channels->combined_count + channels->rx_count) > rx) {
		netdev_err(netdev,
				"total Rx channels (%u) requested exceeds maximum available (%u)\n",
				channels->combined_count + channels->rx_count, rx);
		xgbe_print_set_channels_input(netdev, channels);
		return -EINVAL;
	}

	if ((channels->combined_count + channels->tx_count) > tx) {
		netdev_err(netdev,
				"total Tx channels (%u) requested exceeds maximum available (%u)\n",
				channels->combined_count + channels->tx_count, tx);
		xgbe_print_set_channels_input(netdev, channels);
		return -EINVAL;
	}

	rx = channels->combined_count + channels->rx_count;
	tx = channels->combined_count + channels->tx_count;

	rx_curr = pdata->new_rx_ring_count ? : pdata->rx_ring_count;
	tx_curr = pdata->new_tx_ring_count ? : pdata->tx_ring_count;

	if ((rx == rx_curr) && (tx == tx_curr))
		goto out;

	pdata->new_rx_ring_count = rx;
	pdata->new_tx_ring_count = tx;

	xgbe_full_restart_dev(pdata);

out:
	return 0;
}


#if XGBE_SRIOV_PF
void write_l3l4_registers(struct xgbe_prv_data *pdata, int register_number, int register_type, __be32 register_value)
{
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, TT, 1);
	/* Set the data to be written */
        XGMAC_IOWRITE(pdata, MAC_L3L4WRR, register_value);

	/* Select the register to be updated */	
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, IDDR_REGSEL, register_type);

	/* Select the filter number */
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, IDDR_NUM, register_number);

        // Set TT field to 0
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, TT, 0);

        // Set XB field to 1
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, XB, 1);

        while(XGMAC_IOREAD_BITS(pdata, MAC_L3L4ACTL, XB));

}

static int erase_l3l4_registers(struct xgbe_prv_data *pdata, struct l3l4_filter *rule)
{

	spin_lock(&pdata->lock);
	write_l3l4_registers(pdata, rule->sw_idx, MAC_L3_L4_CONTROL, 0);
	write_l3l4_registers(pdata, rule->sw_idx, MAC_L4_ADDRESS, 0);
	write_l3l4_registers(pdata, rule->sw_idx, MAC_L3_ADDRESS0, 0);
	write_l3l4_registers(pdata, rule->sw_idx, MAC_L3_ADDRESS1, 0);
	spin_unlock(&pdata->lock);


	return 0;

}

int xgbe_update_ethtool_entry(struct xgbe_prv_data *pdata, 
				struct l3l4_filter *input, u16 sw_idx)
{
	struct hlist_node *node2;
	struct l3l4_filter *parent, *rule;
	int err = -EINVAL;

	parent = NULL;
	rule = NULL;

	hlist_for_each_entry_safe(rule, node2, 
				&pdata->l3l4_filter_list, l3l4_node) {
		if(rule->sw_idx >= sw_idx)
			break;		
		parent = rule;
	}

	/* Case where an old rule is present */
	if (rule && (rule->sw_idx == sw_idx)) {
		if(!input) {
		err = erase_l3l4_registers(pdata, rule);	
		}
	hlist_del(&rule->l3l4_node);
	kfree(rule);
	pdata->l3l4_filter_count--;

	}

	if(!input)
		return err;

	INIT_HLIST_NODE(&input->l3l4_node);

	/* Add filter to the list */
	if(parent)
		hlist_add_behind(&input->l3l4_node, &parent->l3l4_node);
	else
		hlist_add_head(&input->l3l4_node, &pdata->l3l4_filter_list);

	pdata->l3l4_filter_count++;
	
	return 0;
}

static int xgbe_l3l4_del_rule(struct xgbe_prv_data *pdata, struct ethtool_rxnfc *rxnfc)
{
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&rxnfc->fs;
	int err = 0;

	err = xgbe_update_ethtool_entry(pdata, NULL, fsp->location);
	return err;

	
}

static int xgbe_update_l3l4_registers(struct xgbe_prv_data *pdata, struct l3l4_filter *input)
{
	int ret = 0;
	__be32 value = 0x0;
	__be32 src_mask = 0x0;
	__be32 dst_mask = 0x0;
	int register_number = input->sw_idx;
	__be32 src_mask_cnt = 0;
	__be32 dst_mask_cnt = 0;
	unsigned int i = 0;
    u32 ring = ethtool_get_flow_spec_ring(input->filter.queue);

	/* Ip source mask application */
	src_mask = input->mask.ip_src;
	if (!(src_mask == IP_MASK_NOT_APPLICABLE )){
		for (i = 0; i < MASK_WIDTH; i++) {
			if ((src_mask && 0x01)) {
				src_mask_cnt++;
				src_mask = src_mask >> 1;
			}
			else
				break;
		}	

		for (i = src_mask_cnt; i < MASK_WIDTH; i++) {
			if ((src_mask && 0x1))
				return -EINVAL;
			src_mask = src_mask >> 1;
		}		
		value |= src_mask_cnt << MAC_L3_L4_CONTROL_LHSBM_INDEX;
		
		/* Ip destination mask application */
		dst_mask = input->mask.ip_dst;
		for (i = 0; i < MASK_WIDTH; i++) {
			if ((dst_mask && 0x01)) {
				dst_mask_cnt++;
				dst_mask = dst_mask >> 1;
			}
			else
				break;
		}	

		for (i = dst_mask_cnt; i < MASK_WIDTH; i++) {
			if ((dst_mask && 0x1))
				return -EINVAL;
			dst_mask = dst_mask >> 1;
		}		
		value |= dst_mask_cnt << MAC_L3_L4_CONTROL_LHDBM_INDEX;

	}

	//if (input->filter.queue != -1)
	//	return -EINVAL;

	/* Enabling the register */
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, IPFE, 1);
	/* Program control register */
	if (input->filter.flow_type == UDP_V4_FLOW) {
		if (!input->filter.ip_src && !input->filter.ip_dst && !input->filter.psrc && !input->filter.pdst)
			return -EINVAL;

		value |= UDP_FILTER_EN; 
		if (input->filter.ip_src) {
			value |= SRC_IP_FILTER_EN;
            if(ring == -1) value |= SRC_IP_FILTER_INV_EN;
        }
		if (input->filter.ip_dst) {
			value |= DST_IP_FILTER_EN;
            if(ring == -1) value |= DST_IP_FILTER_INV_EN;
        }
		if (input->filter.psrc) {
			value |= SRC_PORT_FILTER_EN;
            if(ring == -1) value |= SRC_PORT_FILTER_INV_EN;
			if (!(input->mask.psrc == PORT_MASK_INVAL))
				return -EINVAL;
		}
		if (input->filter.pdst) {
			value |= DST_PORT_FILTER_EN;
            if(ring == -1) value |= DST_PORT_FILTER_INV_EN;
			if (!(input->mask.pdst == PORT_MASK_INVAL))
				return -EINVAL;
		}
	}
	else if (input->filter.flow_type == TCP_V4_FLOW) {
		if (!input->filter.ip_src && !input->filter.ip_dst && !input->filter.psrc && !input->filter.pdst)
			return -EINVAL;

		value |= TCP_FILTER_EN; 
		if (input->filter.ip_src) {
			value |= SRC_IP_FILTER_EN;
            if(ring == -1) value |= SRC_IP_FILTER_INV_EN;
        }
		if (input->filter.ip_dst) {
			value |= DST_IP_FILTER_EN;
            if(ring == -1) value |= DST_IP_FILTER_INV_EN;
        }
		if (input->filter.psrc) {
			value |= SRC_PORT_FILTER_EN;
            if(ring == -1) value |= SRC_PORT_FILTER_INV_EN;
			if (!(input->mask.psrc == PORT_MASK_INVAL))
				return -EINVAL;
		}
		if (input->filter.pdst) {
			value |= DST_PORT_FILTER_EN;
            if(ring == -1) value |= DST_PORT_FILTER_INV_EN;
			if (!(input->mask.pdst == PORT_MASK_INVAL))
				return -EINVAL;
		}
	}
    if(ring != -1) {
        value |= L3L4_DMA_CH_EN; //31st Bit 
        value |= ((ring & 3) << 24);
    }
	write_l3l4_registers(pdata, register_number, MAC_L3_L4_CONTROL, value);

	/* Add the IP source address */
	write_l3l4_registers(pdata, register_number, MAC_L3_ADDRESS0, input->filter.ip_src);

	/* Add the IP destination Address */
	write_l3l4_registers(pdata, register_number, MAC_L3_ADDRESS1, input->filter.ip_dst);

	/* Add the Port address */
	value = 0x0;
	value = input->filter.pdst ;
	value = value << 16;
	value |= input->filter.psrc;
	write_l3l4_registers(pdata, register_number, MAC_L4_ADDRESS, value);

	return ret;
}

static int xgbe_l3l4_add_rule(struct xgbe_prv_data *pdata, 
				struct ethtool_rxnfc *rxnfc)
{
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&rxnfc->fs;
	struct l3l4_filter *input;
	int err = 0;
	
	/* Check for invalid action parameter from ethtool */
	//if (fsp->ring_cookie != -1)
	//	return -EINVAL;

	input = kzalloc(sizeof(*input), GFP_KERNEL);
	if(!input)
		return -ENOMEM;

	/* Caputre the location of the rule in the table */
	input->sw_idx = fsp->location;

	/* Capture the user filter data */
	switch(fsp->flow_type) {
		case TCP_V4_FLOW:
			input->filter.flow_type = fsp->flow_type;
			input->filter.ip_src = ntohl(fsp->h_u.tcp_ip4_spec.ip4src);
			input->filter.ip_dst = ntohl(fsp->h_u.tcp_ip4_spec.ip4dst);
			input->filter.psrc = ntohs(fsp->h_u.tcp_ip4_spec.psrc);
			input->filter.pdst = ntohs(fsp->h_u.tcp_ip4_spec.pdst);
			input->mask.ip_src = ntohl(fsp->m_u.tcp_ip4_spec.ip4src);
			input->mask.ip_dst = ntohl(fsp->m_u.tcp_ip4_spec.ip4dst);
			input->mask.psrc = ntohs(fsp->m_u.tcp_ip4_spec.psrc);
			input->mask.pdst = ntohs(fsp->m_u.tcp_ip4_spec.pdst);
			input->filter.queue = fsp->ring_cookie;
			break;
		case UDP_V4_FLOW:
			input->filter.flow_type = fsp->flow_type;
			input->filter.ip_src = ntohl(fsp->h_u.udp_ip4_spec.ip4src);
			input->filter.ip_dst = ntohl(fsp->h_u.udp_ip4_spec.ip4dst);
			input->filter.psrc = ntohs(fsp->h_u.udp_ip4_spec.psrc);
			input->filter.pdst = ntohs(fsp->h_u.udp_ip4_spec.pdst);
			input->mask.ip_src = ntohl(fsp->m_u.udp_ip4_spec.ip4src);
			input->mask.ip_dst = ntohl(fsp->m_u.udp_ip4_spec.ip4dst);
			input->mask.psrc = ntohs(fsp->m_u.udp_ip4_spec.psrc);
			input->mask.pdst = ntohs(fsp->m_u.udp_ip4_spec.pdst);
			input->filter.queue = fsp->ring_cookie;
			break;
		default:
			return -EINVAL;
	}

	spin_lock(&pdata->lock);	

	err = xgbe_update_l3l4_registers(pdata, input);
	if(err)
		goto error_inval;

	xgbe_update_ethtool_entry(pdata, input, input->sw_idx);	

	//spin_unlock(&pdata->lock);	

error_inval:
	spin_unlock(&pdata->lock);	
	return err;
}

static int xgbe_set_rxnfc(struct net_device *netdev,
		struct ethtool_rxnfc *rxnfc)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	int ret = 0;

	switch(rxnfc->cmd) {
		case ETHTOOL_SRXCLSRLINS:
			ret = xgbe_l3l4_add_rule(pdata, rxnfc);
			break;
		case ETHTOOL_SRXCLSRLDEL:
			ret = xgbe_l3l4_del_rule(pdata, rxnfc);
			break;
		default:
			ret = -EOPNOTSUPP;

	}

	return ret;
}
#endif
static int xgbe_flash_firmware(struct net_device *netdev,
		struct ethtool_flash *flash)
{
	int ret = 0;
#if XGBE_SRIOV_PF
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	const struct firmware  *fw;
	if(pdata->dev_id == BCM8989X_PF_ID) {
		ret = request_firmware(&fw, flash->data, &netdev->dev);
		if (ret != 0) {
			netdev_err(netdev, "Error %d requesting firmware file: %s\n", ret, flash->data);
			return ret;
		}
		pdata->phy_if.phy_impl.flash_firmware(pdata, flash->region, (char *)fw->data, fw->size); 
	}
#endif
	return ret;
}

static const struct ethtool_ops xgbe_ethtool_ops = {
	.get_drvinfo = xgbe_get_drvinfo,
	.get_msglevel = xgbe_get_msglevel,
	.set_msglevel = xgbe_set_msglevel,
	.get_link = ethtool_op_get_link,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
	.supported_coalesce_params = ETHTOOL_COALESCE_RX_USECS |
				     ETHTOOL_COALESCE_MAX_FRAMES,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	.get_coalesce = xgbe_get_coalesce_args,
	.set_coalesce = xgbe_set_coalesce_args,
#else
	.get_coalesce = xgbe_get_coalesce,
	.set_coalesce = xgbe_set_coalesce,
#endif
#if XGBE_SRIOV_PF
	.get_pauseparam = xgbe_get_pauseparam,
	.set_pauseparam = xgbe_set_pauseparam,
#endif
	.get_strings = xgbe_get_strings,
	.get_ethtool_stats = xgbe_get_ethtool_stats,
	.get_sset_count = xgbe_get_sset_count,
	.get_rxnfc = xgbe_get_rxnfc,
#if XGBE_SRIOV_PF
	.set_rxnfc = xgbe_set_rxnfc,
#endif
	.get_rxfh_key_size = xgbe_get_rxfh_key_size,
	.get_rxfh_indir_size = xgbe_get_rxfh_indir_size,
	.get_rxfh = xgbe_get_rxfh,
	.set_rxfh = xgbe_set_rxfh,
	.get_ts_info = xgbe_get_ts_info,
	.get_link_ksettings = xgbe_get_link_ksettings,
	.set_link_ksettings = xgbe_set_link_ksettings,
	.get_module_info = xgbe_get_module_info,
	.get_module_eeprom = xgbe_get_module_eeprom,
	.get_ringparam = xgbe_get_ringparam,
	.set_ringparam = xgbe_set_ringparam,
	.get_channels = xgbe_get_channels,
	.set_channels = xgbe_set_channels,
	.flash_device = xgbe_flash_firmware,
};

const struct ethtool_ops *xgbe_get_ethtool_ops(void)
{
	return &xgbe_ethtool_ops;
}
