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

#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/clk.h>
#include <linux/bitrev.h>
#include <linux/crc32.h>
#include <linux/version.h>

#include "xgbe.h"
#include "xgbe-common.h"

#if XGBE_SRIOV_PF
#include <linux/pci.h>

extern unsigned int max_vfs;
#endif

#if BRCM_FH
static void xgbe_flex_hdr_set(struct xgbe_prv_data *pdata, 
		unsigned int fhdr_high, unsigned int fhdr_low)
{
	XGMAC_IOWRITE(pdata, MAC_FLEX_HDR_HIGH, fhdr_high);
	XGMAC_IOWRITE(pdata, MAC_FLEX_HDR_LOW, fhdr_low);
}

static void xgbe_set_flex_hdr_length(struct xgbe_prv_data *pdata, 
				unsigned int fhdr_len)
{
	XGMAC_IOWRITE_BITS(pdata, MAC_FLEX_HDR_CFG, FHL, fhdr_len);	
}

static void xgbe_set_flex_hdr_position(struct xgbe_prv_data *pdata, 
				unsigned int fhdr_pos)

{
	XGMAC_IOWRITE_BITS(pdata, MAC_FLEX_HDR_CFG, FHSP, fhdr_pos);
}

static void xgbe_enable_link_layer_flex_hdr(struct xgbe_prv_data *pdata, 
				unsigned int fhdr_en_ll)
{
	XGMAC_IOWRITE_BITS(pdata, MAC_FLEX_HDR_CFG, EFLL, fhdr_en_ll);
}

static void xgbe_enable_tx_path_flex_hdr(struct xgbe_prv_data *pdata, 
				unsigned int fhdr_en_tx)
{
	XGMAC_IOWRITE_BITS(pdata, MAC_FLEX_HDR_CFG, FHTX, fhdr_en_tx);
}

static void xgbe_enable_rx_path_flex_hdr(struct xgbe_prv_data *pdata, 
				unsigned int fhdr_en_rx)
{
	XGMAC_IOWRITE_BITS(pdata, MAC_FLEX_HDR_CFG, FHRX, fhdr_en_rx);
}
#endif

static inline unsigned int xgbe_get_max_frame(struct xgbe_prv_data *pdata)
{
	return pdata->netdev->mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
}

static unsigned int xgbe_usec_to_riwt(struct xgbe_prv_data *pdata,
		unsigned int usec)
{
	unsigned long rate;
	unsigned int ret;

	DBGPR("--> %s\n", __func__);

	rate = pdata->sysclk_rate;

	/*
	 * Convert the input usec value to the watchdog timer value. Each
	 * watchdog timer value is equivalent to 256 clock cycles.
	 * Calculate the required value as:
	 * (usec * (system_clock_mhz / 10^6) / 256
	 */
	ret = (usec * (rate / 1000000)) / 256;

	DBGPR("<-- %s\n", __func__);

	return ret;
}

static unsigned int xgbe_riwt_to_usec(struct xgbe_prv_data *pdata,
		unsigned int riwt)
{
	unsigned long rate;
	unsigned int ret;

	DBGPR("--> %s\n", __func__);

	rate = pdata->sysclk_rate;

	/*
	 * Convert the input watchdog timer value to the usec value. Each
	 * watchdog timer value is equivalent to 256 clock cycles.
	 * Calculate the required value as:
	 *	 (riwt * 256) / (system_clock_mhz / 10^6)
	 */
	ret = (riwt * 256) / (rate / 1000000);

	DBGPR("<-- %s\n", __func__);

	return ret;
}

static int xgbe_config_pbl_val(struct xgbe_prv_data *pdata)
{
	unsigned int pblx8, pbl;
	unsigned int i;

	pblx8 = DMA_PBL_X8_DISABLE;
	pbl = pdata->pbl;

	if (pdata->pbl > 32) {
		pblx8 = DMA_PBL_X8_ENABLE;
		pbl >>= 3;
	}

	for (i = 0; i < pdata->channel_count; i++) {
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_CR, PBLX8,
				pblx8);

		if (pdata->channel[i]->tx_ring)
			XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR,
					PBL, pbl);

		if (pdata->channel[i]->rx_ring)
			XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR,
					PBL, pbl);
	}

	return 0;
}

static int xgbe_config_osp_mode(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->tx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR, OSP,
				pdata->tx_osp_mode);
	}

	return 0;
}

#if XGBE_SRIOV_PF
static int xgbe_config_rsf_mode(struct xgbe_prv_data *pdata, unsigned int val)
{
	unsigned int i;

	for (i = 0; i < pdata->rx_q_count; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, RSF, val);

	return 0;
}

static int xgbe_config_tsf_mode(struct xgbe_prv_data *pdata, unsigned int val)
{
	unsigned int i;

	for (i = 0; i < XGBE_MAX_QUEUES; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TSF, val);

	return 0;
}

static int xgbe_config_rx_threshold(struct xgbe_prv_data *pdata,
		unsigned int val)
{
	unsigned int i;

	for (i = 0; i < pdata->rx_q_count; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, RTC, val);

	return 0;
}

static int xgbe_config_tx_threshold(struct xgbe_prv_data *pdata,
		unsigned int val)
{
	unsigned int i;

	for (i = 0; i < pdata->tx_q_count; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TTC, val);

	return 0;
}
#endif

static int xgbe_config_rx_coalesce(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RIWT, RWT,
				(pdata->rx_riwt));
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RIWT, RWTU,
				2);
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RIWT, PSEL,
				1);
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RIWT, RBCT,
				(pdata->rx_frames));
	}

	return 0;
}

static void xgbe_config_rx_buffer_size(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, RBSZ,
				pdata->rx_buf_size);
	}
}

static void xgbe_config_tso_mode(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->tx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR, TSE, 1);
	}
}

static void xgbe_config_sph_mode(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_CR, SPH, 1);
	}
#if XGBE_SRIOV_PF
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, HDSMS, XGBE_SPH_HDSMS_SIZE);
#endif
}

#if XGBE_SRIOV_PF
#if !ELI_ENABLE
static int xgbe_write_rss_reg(struct xgbe_prv_data *pdata, unsigned int type,
		unsigned int index, unsigned int val)
{
	unsigned int wait;
	int ret = 0;

	mutex_lock(&pdata->rss_mutex);

	if (XGMAC_IOREAD_BITS(pdata, MAC_RSSAR, OB)) {
		ret = -EBUSY;
		goto unlock;
	}

	XGMAC_IOWRITE(pdata, MAC_RSSDR, val);

	XGMAC_IOWRITE_BITS(pdata, MAC_RSSAR, RSSIA, index);
	XGMAC_IOWRITE_BITS(pdata, MAC_RSSAR, ADDRT, type);
	XGMAC_IOWRITE_BITS(pdata, MAC_RSSAR, CT, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_RSSAR, OB, 1);

	wait = 1000;
	while (wait--) {
		if (!XGMAC_IOREAD_BITS(pdata, MAC_RSSAR, OB))
			goto unlock;

		XGBE_USLEEP_RANGE(1000, 1500);
	}

	ret = -EBUSY;

unlock:
	mutex_unlock(&pdata->rss_mutex);

	return ret;
}

static int xgbe_write_rss_hash_key(struct xgbe_prv_data *pdata)
{
	unsigned int key_regs = sizeof(pdata->rss_key) / sizeof(u32);
	unsigned int *key = (unsigned int *)&pdata->rss_key;
	int ret;

	while (key_regs--) {
		ret = xgbe_write_rss_reg(pdata, XGBE_RSS_HASH_KEY_TYPE,
				key_regs, *key++);
		if (ret)
			return ret;
	}

	return 0;
}

static int xgbe_write_rss_lookup_table(struct xgbe_prv_data *pdata)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(pdata->rss_table); i++) {
		ret = xgbe_write_rss_reg(pdata,
				XGBE_RSS_LOOKUP_TABLE_TYPE, i,
				pdata->rss_table[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int xgbe_set_rss_hash_key(struct xgbe_prv_data *pdata, const u8 *key)
{
	memcpy((u8 *)pdata->rss_key, key, sizeof(pdata->rss_key));

	return xgbe_write_rss_hash_key(pdata);
}

static int xgbe_set_rss_lookup_table(struct xgbe_prv_data *pdata,
		const u32 *table)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pdata->rss_table); i++)
		XGMAC_SET_BITS(pdata->rss_table[i], MAC_RSSDR, DMCH, table[i]);

	return xgbe_write_rss_lookup_table(pdata);
}

static int xgbe_enable_rss(struct xgbe_prv_data *pdata)
{
	int ret;

	if (!pdata->hw_feat.rss)
		return -EOPNOTSUPP;

	/* Program the hash key */
	ret = xgbe_write_rss_hash_key(pdata);
	if (ret)
		return ret;

	/* Program the lookup table */
	ret = xgbe_write_rss_lookup_table(pdata);
	if (ret)
		return ret;

	/* Set the RSS options */
	XGMAC_IOWRITE(pdata, MAC_RSSCR, pdata->rss_options);

	/* Enable RSS */
	XGMAC_IOWRITE_BITS(pdata, MAC_RSSCR, RSSE, 1);

	return 0;
}

static int xgbe_disable_rss(struct xgbe_prv_data *pdata)
{
	if (!pdata->hw_feat.rss)
		return -EOPNOTSUPP;

	XGMAC_IOWRITE_BITS(pdata, MAC_RSSCR, RSSE, 0);

	return 0;
}

static void xgbe_config_rss(struct xgbe_prv_data *pdata)
{
	int ret;

	if (!pdata->hw_feat.rss)
		return;

	if (pdata->netdev->features & NETIF_F_RXHASH)
		ret = xgbe_enable_rss(pdata);
	else
		ret = xgbe_disable_rss(pdata);

	if (ret)
		netdev_err(pdata->netdev,
				"error configuring RSS, RSS disabled\n");
}
#endif

static bool xgbe_is_pfc_queue(struct xgbe_prv_data *pdata,
		unsigned int queue)
{
	unsigned int prio, tc;

	for (prio = 0; prio < IEEE_8021QAZ_MAX_TCS; prio++) {
		/* Does this queue handle the priority? */
		if (pdata->prio2q_map[prio] != queue)
			continue;

		/* Get the Traffic Class for this priority */
		tc = pdata->ets->prio_tc[prio];

		/* Check if PFC is enabled for this traffic class */
		if (pdata->pfc->pfc_en & (1 << tc))
			return true;
	}

	return false;
}
#endif

static void xgbe_set_vxlan_id(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	/* Program the VXLAN port */
	XGMAC_IOWRITE_BITS(pdata, MAC_TIR, TNID, pdata->vxlan_port);
#endif
	netif_dbg(pdata, drv, pdata->netdev, "VXLAN tunnel id set to %hx\n",
			pdata->vxlan_port);
}

static void xgbe_enable_vxlan(struct xgbe_prv_data *pdata)
{
	if (!pdata->hw_feat.vxn)
		return;

	/* Program the VXLAN port */
	xgbe_set_vxlan_id(pdata);

#if XGBE_SRIOV_PF
	/* Allow for IPv6/UDP zero-checksum VXLAN packets */
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, VUCC, 1);

	/* Enable VXLAN tunneling mode */
	XGMAC_IOWRITE_BITS(pdata, MAC_TCR, VNM, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_TCR, VNE, 1);
#endif

	netif_dbg(pdata, drv, pdata->netdev, "VXLAN acceleration enabled\n");
}

static void xgbe_disable_vxlan(struct xgbe_prv_data *pdata)
{
	if (!pdata->hw_feat.vxn)
		return;
		
#if XGBE_SRIOV_PF
	/* Disable tunneling mode */
	XGMAC_IOWRITE_BITS(pdata, MAC_TCR, VNE, 0);

	/* Clear IPv6/UDP zero-checksum VXLAN packets setting */
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, VUCC, 0);

	/* Clear the VXLAN port */
	XGMAC_IOWRITE_BITS(pdata, MAC_TIR, TNID, 0);
#endif

	netif_dbg(pdata, drv, pdata->netdev, "VXLAN acceleration disabled\n");
}

#if XGBE_SRIOV_PF
static int xgbe_disable_tx_flow_control(struct xgbe_prv_data *pdata)
{
	unsigned int max_q_count, q_count;
	unsigned int reg, reg_val;
	unsigned int i;

	/* Clear MTL flow control */
	for (i = 0; i < pdata->rx_q_count; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, EHFC, 0);

	/* Clear MAC flow control */
	max_q_count = XGMAC_MAX_FLOW_CONTROL_QUEUES;
	q_count = min_t(unsigned int, pdata->tx_q_count, max_q_count);
	reg = MAC_Q0TFCR;
	for (i = 0; i < q_count; i++) {
		reg_val = XGMAC_IOREAD(pdata, reg);
		XGMAC_SET_BITS(reg_val, MAC_Q0TFCR, TFE, 0);
		XGMAC_IOWRITE(pdata, reg, reg_val);

		reg += MAC_QTFCR_INC;
	}

	return 0;
}

static int xgbe_enable_tx_flow_control(struct xgbe_prv_data *pdata)
{
	struct ieee_pfc *pfc = pdata->pfc;
	struct ieee_ets *ets = pdata->ets;
	unsigned int max_q_count, q_count;
	unsigned int reg, reg_val;
	unsigned int i;

	/* Set MTL flow control */
	for (i = 0; i < pdata->rx_q_count; i++) {
		unsigned int ehfc = 0;

		if (pdata->rx_rfd[i]) {
			/* Flow control thresholds are established */
			if (pfc && ets) {
				if (xgbe_is_pfc_queue(pdata, i))
					ehfc = 1;
			} else {
				ehfc = 1;
			}
		}

		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, EHFC, ehfc);

		netif_dbg(pdata, drv, pdata->netdev,
				"flow control %s for RXq%u\n",
				ehfc ? "enabled" : "disabled", i);
	}

	/* Set MAC flow control */
	max_q_count = XGMAC_MAX_FLOW_CONTROL_QUEUES;
	q_count = min_t(unsigned int, pdata->tx_q_count, max_q_count);
	reg = MAC_Q0TFCR;
	for (i = 0; i < q_count; i++) {
		reg_val = XGMAC_IOREAD(pdata, reg);

		/* Enable transmit flow control */
		XGMAC_SET_BITS(reg_val, MAC_Q0TFCR, TFE, 1);
		/* Set pause time */
		XGMAC_SET_BITS(reg_val, MAC_Q0TFCR, PT, pdata->pause_quanta);

		XGMAC_IOWRITE(pdata, reg, reg_val);

		reg += MAC_QTFCR_INC;
	}

	return 0;
}

static int xgbe_disable_rx_flow_control(struct xgbe_prv_data *pdata)
{
	XGMAC_IOWRITE_BITS(pdata, MAC_RFCR, RFE, 0);

	return 0;
}

static int xgbe_enable_rx_flow_control(struct xgbe_prv_data *pdata)
{
	XGMAC_IOWRITE_BITS(pdata, MAC_RFCR, RFE, 1);

	return 0;
}

static int xgbe_config_tx_flow_control(struct xgbe_prv_data *pdata)
{
	struct ieee_pfc *pfc = pdata->pfc;

	if (pdata->tx_pause || (pfc && pfc->pfc_en))
		xgbe_enable_tx_flow_control(pdata);
	else
		xgbe_disable_tx_flow_control(pdata);

	return 0;
}

static int xgbe_config_rx_flow_control(struct xgbe_prv_data *pdata)
{
	struct ieee_pfc *pfc = pdata->pfc;

	if (pdata->rx_pause || (pfc && pfc->pfc_en))
		xgbe_enable_rx_flow_control(pdata);
	else
		xgbe_disable_rx_flow_control(pdata);

	return 0;
}

static void xgbe_config_flow_control(struct xgbe_prv_data *pdata)
{
	struct ieee_pfc *pfc = pdata->pfc;

	xgbe_config_tx_flow_control(pdata);
	xgbe_config_rx_flow_control(pdata);

	XGMAC_IOWRITE_BITS(pdata, MAC_RFCR, PFCE,
			(pfc && pfc->pfc_en) ? 1 : 0);
}
#endif

static void xgbe_enable_dma_interrupts(struct xgbe_prv_data *pdata)
{
	struct xgbe_channel *channel;
	unsigned int i, ver;

	/* Set the interrupt mode if supported */
#if XGBE_SRIOV_PF
	if (pdata->channel_irq_mode)
		XGMAC_IOWRITE_BITS(pdata, DMA_MR, INTM,
				pdata->channel_irq_mode);

	ver = XGMAC_GET_BITS(pdata->hw_feat.version, MAC_VR, SNPSVER);
#endif

#if XGBE_SRIOV_VF
	ver = 0x30;
#endif

	for (i = 0; i < pdata->channel_count; i++) {
		channel = pdata->channel[i];

		/* Clear all the interrupts which are set */
		XGMAC_DMA_IOWRITE(channel, DMA_CH_SR,
				XGMAC_DMA_IOREAD(channel, DMA_CH_SR));

		/* Clear all interrupt enable bits */
		channel->curr_ier = 0;

		/* Enable following interrupts
		 * NIE - Normal Interrupt Summary Enable
		 * AIE - Abnormal Interrupt Summary Enable
		 * FBEE - Fatal Bus Error Enable
		 */
		if (ver < 0x21) {
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, NIE20, 1);
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, AIE20, 1);
		} else {
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, NIE, 1);
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, AIE, 1);
		}
		XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, FBEE, 1);

		if (channel->tx_ring) {
			/* Enable the following Tx interrupts
			 * TIE - Transmit Interrupt Enable (unless using
			 * per channel interrupts in edge triggered
			 * mode)
			 */
			if (!pdata->per_channel_irq || pdata->channel_irq_mode)
				XGMAC_SET_BITS(channel->curr_ier,
						DMA_CH_IER, TIE, 1);
		}
		if (channel->rx_ring) {
			/* Enable following Rx interrupts
			 * RBUE - Receive Buffer Unavailable Enable
			 * RIE - Receive Interrupt Enable (unless using
			 * per channel interrupts in edge triggered
			 * mode)
			 */
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RBUE, 1);
			if (!pdata->per_channel_irq || pdata->channel_irq_mode)
				XGMAC_SET_BITS(channel->curr_ier,
						DMA_CH_IER, RIE, 1);
		}

		XGMAC_DMA_IOWRITE(channel, DMA_CH_IER, channel->curr_ier);
	}
}

#if XGBE_SRIOV_PF
static void xgbe_enable_mtl_interrupts(struct xgbe_prv_data *pdata)
{
	unsigned int mtl_q_isr;
	unsigned int q_count, i;

	q_count = max(pdata->hw_feat.tx_q_cnt, pdata->hw_feat.rx_q_cnt);
	for (i = 0; i < q_count; i++) {
		/* Clear all the interrupts which are set */
		mtl_q_isr = XGMAC_MTL_IOREAD(pdata, i, MTL_Q_ISR);
		XGMAC_MTL_IOWRITE(pdata, i, MTL_Q_ISR, mtl_q_isr);

		/* No MTL interrupts to be enabled */
		XGMAC_MTL_IOWRITE(pdata, i, MTL_Q_IER, 0);
	}
}

static void xgbe_enable_mac_interrupts(struct xgbe_prv_data *pdata)
{
	unsigned int mac_ier = 0;

	/* Enable Timestamp interrupt */
	XGMAC_SET_BITS(mac_ier, MAC_IER, TSIE, 1);

	XGMAC_IOWRITE(pdata, MAC_IER, mac_ier);

	/* Enable all counter interrupts */
	XGMAC_IOWRITE_BITS(pdata, MMC_RIER, ALL_INTERRUPTS, 0xffffffff);
	XGMAC_IOWRITE_BITS(pdata, MMC_TIER, ALL_INTERRUPTS, 0xffffffff);

	/* Enable MDIO single command completion interrupt */
	XGMAC_IOWRITE_BITS(pdata, MAC_MDIOIER, SNGLCOMPIE, 1);
}

static int xgbe_set_speed(struct xgbe_prv_data *pdata, int speed)
{
	unsigned int ss;

	switch (speed) {
		case SPEED_1000:
			ss = 0x03;
			break;
		case SPEED_2500:
			ss = 0x02;
			break;
		case SPEED_10000:
			ss = 0x00;
			break;
		default:
			return -EINVAL;
	}

	if (XGMAC_IOREAD_BITS(pdata, MAC_TCR, SS) != ss)
		XGMAC_IOWRITE_BITS(pdata, MAC_TCR, SS, ss);

	return 0;
}
#if !ELI_ENABLE
static int xgbe_kill_vlan(struct xgbe_prv_data *pdata, int vid, int vf)
{
	unsigned int reg_val = 0, vlan_data = 0, vlanid = 0, ofs = 0;
	int i = 0;
	struct vf_data_storage *vfinfo;

	reg_val = XGMAC_IOREAD(pdata, MAC_VLANTR);

	for (i = 0; i < 32; i++) {
		if (pdata->ofs_2_vlan_map[i] == vid) {
			ofs = i;
			break;
		}
	}

	if (i == 32) {
		TEST_PRNT("No matching VLAN filter found for VLAN ID %d\n", vid);
		return -1;
	}

	if (vf == XGBE_VLAN_REQ_FROM_PF) {
		TEST_PRNT("Kill VLAN request from PF for VLAN ID : %d\n", vid);
		for_each_set_bit(vlanid, pdata->active_vlans, VLAN_N_VID) {
			if (vlanid == vid) {
				TEST_PRNT("VLAN ID %d is configured for PF in filter %d\n", vlanid, ofs);

				pdata->vlan_filters_used--;

				goto SUCCESS;
			}
		}
		TEST_PRNT("VlAN ID %d is not configured for PF\n", vid);
		goto FAILURE;
	} else {
		vfinfo = &pdata->vfinfo[vf];

		TEST_PRNT("Kill VLAN request from VF%d for VLAN ID %d\n", vf, vid);
		for_each_set_bit (vlanid, vfinfo->active_vlans, VLAN_N_VID) {
			if (vlanid == vid) {
				TEST_PRNT("VLAN ID %d is configured for VF in filter %d\n", vlanid, ofs);

				vfinfo->vlan_filters_used--;

				goto SUCCESS;
			}
		}
		TEST_PRNT("VLAN ID %d is not confiured for VF%d\n", vid, vf);
		goto FAILURE;
	}

SUCCESS:

	pdata->ofs_2_vlan_map[ofs] = 0;
	XGMAC_SET_BITS(reg_val, MAC_VLANTR, OFS, ofs);
	XGMAC_SET_BITS(reg_val, MAC_VLANTR, CT, 0);

	XGMAC_IOWRITE(pdata, MAC_VLANTR, reg_val);
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTDR, VEN, 1);

	XGMAC_IOWRITE(pdata, MAC_VLANTDR, vlan_data);

	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, OB, 1);
	while (XGMAC_IOREAD_BITS(pdata, MAC_VLANTR, OB)) {
		TEST_PRNT("Write operation in progress\n");
	}

	return 0;

FAILURE:
	return -1;
}

static int xgbe_add_vlan(struct xgbe_prv_data *pdata, int vid, int vf)
{
	unsigned int reg_val = 0, vlan_data = 0, vlanid = 0, ofs = 0;
	int i;
	struct vf_data_storage *vfinfo;

	if (vid == 0)
		return 1;

	for_each_set_bit(vlanid, pdata->active_vlans, VLAN_N_VID) {
		if (vlanid == vid) {
			TEST_PRNT("VLAN ID %d is already mapped to a DMA channel for PF\n", vlanid);
			TEST_PRNT("HW doesn't allow mapping of single VLAN ID to multiple DMA channels\n");
			return -1;
		}
	}

	vlanid = 0;

	for_each_set_bit(vlanid, pdata->active_vf_vlans, VLAN_N_VID) {
		if (vlanid == vid) {
			TEST_PRNT("VLAN ID %d is already mapped to a DMA channel for a VF\n", vlanid);
			TEST_PRNT("HW doesn't allow mapping of single VLAN ID to multiple DMA channels\n");
			return -1;
		}
	}

	/* Check which filters are free and take the first one */
	for (i = 0; i < 32; i++) {
		if (pdata->ofs_2_vlan_map[i] == 0) {
			break;
		}
	}

	if (i == 32) {
		TEST_PRNT("All VLAN filters in use\n");
		return -1;
	}

	ofs = i;

	reg_val = XGMAC_IOREAD(pdata, MAC_VLANTR);
	XGMAC_SET_BITS(reg_val, MAC_VLANTR, VTHM, 0);

	TEST_PRNT("Configuring for vid %d\n", vid);

	TEST_PRNT("Configuring VLAN Filter %d\n", ofs);

	XGMAC_SET_BITS(reg_val, MAC_VLANTR, OFS, ofs);
	XGMAC_SET_BITS(reg_val, MAC_VLANTR, CT, 0);

	XGMAC_IOWRITE(pdata, MAC_VLANTR, reg_val);
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTDR, VEN, 1);

	/* -1 indicates request is coming from PF. */
	if (vf == XGBE_VLAN_REQ_FROM_PF) {
		TEST_PRNT("Add VLAN request from PF\n");
		if ((pdata->vlan_filters_used) == (32 / (max_vfs + 1))) {
			dev_err(pdata->dev, "PF has used the maximum number of VLAN filters allotted to it\n");
			dev_err(pdata->dev, "Number of filters used by PF = %d\n", pdata->vlan_filters_used);
			return -1;
		}

		if (pdata->last_used_dma == (pdata->max_DMA_pf - 1)) {
			pdata->last_used_dma = 0;
		}
		XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, DMACHN, (pdata->last_used_dma + 1));
		XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, DMACHEN, 1);

		TEST_PRNT("Mapped VLAN ID %d to DMA channel %d\n", vid, pdata->last_used_dma + 1);

		pdata->last_used_dma++;
		pdata->vlan_filters_used++;
	} else {
		TEST_PRNT("Add VLAN request from VF%d\n", vf);

		vfinfo = &pdata->vfinfo[vf];
		if ((vfinfo->vlan_filters_used) == (32 / (max_vfs + 1))) {
			TEST_PRNT("VF%d has used the maximum number of VLAN filters allotted to it\n", vf);
			TEST_PRNT("Number of filters used by VF%d = %d\n", vf, vfinfo->vlan_filters_used);
			return -1;
		}

		if (vfinfo->last_used_dma == (vfinfo->dma_channel_start_num + vfinfo->num_dma_channels - 1)) {
			vfinfo->last_used_dma = vfinfo->dma_channel_start_num;
		}

		XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, DMACHN, (vfinfo->last_used_dma + 1));
		XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, DMACHEN, 1);

		TEST_PRNT("Mapped VLAN ID %d to DMA channel %d\n", vid, vfinfo->last_used_dma + 1);

		vfinfo->last_used_dma++;
		vfinfo->vlan_filters_used++;
	}

	XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, ERIVLT, 0);
	XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, ERSVLM, 0);
	XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, DOVLTC, 1);
	XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, ETV, 1);
	XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, VEN, 1);
	XGMAC_SET_BITS(vlan_data, MAC_VLANTDR, VID, vid);

	XGMAC_IOWRITE(pdata, MAC_VLANTDR, vlan_data);

	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, OB, 1);
	while (XGMAC_IOREAD_BITS(pdata, MAC_VLANTR, OB)) {
		TEST_PRNT("Write operation in progress\n");
	}

	pdata->ofs_2_vlan_map[ofs] = vid;

	return 0;
}
#endif
#endif /* XGBE_SRIOV_PF */

static int xgbe_enable_rx_vlan_stripping(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	if (0x3 == XGMAC_IOREAD_BITS(pdata, MAC_VLANTR, EVLS)) {
		TEST_PRNT("VLAN stripping already enabled for HW\n");
		return 0;
	}
	/* Put the VLAN tag in the Rx descriptor */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, EVLRXS, 1);

	/* Don't check the VLAN type */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, DOVLTC, 1);

	/* Check only C-TAG (0x8100) packets */
	//XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, ERSVLM, 0);
	/* to enable hash table filtering */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, ERSVLM, 1);

	/* Don't consider an S-TAG (0x88A8) packet as a VLAN packet */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, ESVL, 0);

	/* Enable VLAN tag stripping */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, EVLS, 0x3);
#endif

#if XGBE_SRIOV_VF
	pdata->mbx.enable_rx_vlan_stripping(pdata);
#endif
	return 0;
}

static int xgbe_disable_rx_vlan_stripping(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, EVLS, 0);
#endif

#if XGBE_SRIOV_VF
	pdata->mbx.disable_rx_vlan_stripping(pdata);
#endif
	return 0;
}

static int xgbe_enable_rx_vlan_filtering(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	if (1 == XGMAC_IOREAD_BITS(pdata, MAC_PFR, VTFE)) {
		TEST_PRNT("VLAN filtering already enabled in HW\n");
		return 0;
	}
	/* Enable VLAN filtering */
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, VTFE, 1);

	/* Enable VLAN Hash Table filtering */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, VTHM, 1);

	/* Disable VLAN tag inverse matching */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, VTIM, 0);

	/* Only filter on the lower 12-bits of the VLAN tag */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, ETV, 1);

	/* In order for the VLAN Hash Table filtering to be effective,
	 * the VLAN tag identifier in the VLAN Tag Register must not
	 * be zero.  Set the VLAN tag identifier to "1" to enable the
	 * VLAN Hash Table filtering.  This implies that a VLAN tag of
	 * 1 will always pass filtering.
	 */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, VL, 1);
#endif

#if XGBE_SRIOV_VF
	pdata->mbx.enable_rx_vlan_filtering(pdata);
#endif
	return 0;
}

static int xgbe_disable_rx_vlan_filtering(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	/* Disable VLAN filtering */
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, VTFE, 0);
#endif
#if XGBE_SRIOV_VF
	pdata->mbx.disable_rx_vlan_filtering(pdata);
#endif
	return 0;
}

static u32 xgbe_vid_crc32_le(__le16 vid_le)
{
	u32 poly = 0xedb88320;	/* CRCPOLY_LE */
	u32 crc = ~0;
	u32 temp = 0;
	unsigned char *data = (unsigned char *)&vid_le;
	unsigned char data_byte = 0;
	int i, bits;

	bits = get_bitmask_order(VLAN_VID_MASK);
	for (i = 0; i < bits; i++) {
		if ((i % 8) == 0)
			data_byte = data[i / 8];

		temp = ((crc & 1) ^ data_byte) & 1;
		crc >>= 1;
		data_byte >>= 1;

		if (temp)
			crc ^= poly;
	}

	return crc;
}

#if XGBE_SRIOV_PF
#if ELI_ENABLE
static int xgbe_update_vlan_hash_table_vf(struct xgbe_prv_data *pdata, int vf)
{
	u32 crc;
	u16 vid;
	__le16 vid_le;
	u16 vlan_hash_table = 0;
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf];

	/* Generate the VLAN Hash Table value */
	for_each_set_bit(vid, pdata->active_vlans, VLAN_N_VID) {
		/* Get the CRC32 value of the VLAN ID */
		vid_le = cpu_to_le16(vid);
		crc = bitrev32(~xgbe_vid_crc32_le(vid_le)) >> 28;

		vlan_hash_table |= (1 << crc);
	}

	for_each_set_bit(vid, vfinfo->active_vlans, VLAN_N_VID) {
		vid_le = cpu_to_le16(vid);
		crc = bitrev32(~xgbe_vid_crc32_le(vid_le)) >> 28;

		vlan_hash_table |= (1 << crc);
	}

	/* Set the VLAN Hash Table filtering register */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANHTR, VLHT, vlan_hash_table);

	return 0;
}
#endif
#endif /* XGBE_SRIOV_PF */

static int xgbe_update_vlan_hash_table(struct xgbe_prv_data *pdata)
{
	u32 crc;
	u16 vid;
	__le16 vid_le;
	u16 vlan_hash_table = 0;

#if XGBE_SRIOV_PF
	struct vf_data_storage *vfinfo = NULL;
	unsigned int i;
#endif

	/* Generate the VLAN Hash Table value */
	for_each_set_bit(vid, pdata->active_vlans, VLAN_N_VID) {
		/* Get the CRC32 value of the VLAN ID */
		vid_le = cpu_to_le16(vid);
		crc = bitrev32(~xgbe_vid_crc32_le(vid_le)) >> 28;

		vlan_hash_table |= (1 << crc);
	}

#if XGBE_SRIOV_PF
	if(pdata->vfinfo && (pdata->flags & XGBE_FLAG_SRIOV_ENABLED)){
		for(i = 0; i < max_vfs; i++){
			vfinfo = &pdata->vfinfo[i];
			if(!vfinfo)
				continue;

			for_each_set_bit(vid, vfinfo->active_vlans, VLAN_N_VID) {
				vid_le = cpu_to_le16(vid);
				crc = bitrev32(~xgbe_vid_crc32_le(vid_le)) >> 28;

				vlan_hash_table |= (1 << crc);
			}        
		}
	}
	/* Set the VLAN Hash Table filtering register */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANHTR, VLHT, vlan_hash_table);
#endif

	return 0;
}

#if XGBE_SRIOV_PF
#if ELI_ENABLE
static int xgbe_set_promiscuous_mode(struct xgbe_prv_data *pdata,
		unsigned int enable)
{
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct vf_data_storage *vfinfo = NULL;
	int i;

	netif_dbg(pdata, drv, pdata->netdev, "%s promiscuous mode\n",
			enable ? "entering" : "leaving");
	eli_if->set_promiscuous_mode(pdata, ELI_FUNC_PF, enable);

	if(pdata->vfinfo && (pdata->flags & XGBE_FLAG_SRIOV_ENABLED))
		for(i = 0; i < max_vfs; i++){
			vfinfo = &pdata->vfinfo[i];
			if(!vfinfo)
				continue;

			eli_if->set_promiscuous_mode(pdata, (i + 1), vfinfo->promisc);
		}		

	return 0;
}
#else

static int xgbe_set_promiscuous_mode(struct xgbe_prv_data *pdata,
		unsigned int enable)
{
	unsigned int val = enable ? 1 : 0;
	if (XGMAC_IOREAD_BITS(pdata, MAC_PFR, PR) == val)
		return 0;

	netif_dbg(pdata, drv, pdata->netdev, "%s promiscuous mode\n",
			enable ? "entering" : "leaving");

	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, PR, val);


	/* Hardware will still perform VLAN filtering in promiscuous mode */
	if (enable) {
		xgbe_disable_rx_vlan_filtering(pdata);
	} else {
		/* Disabled Broadcast Packets */
		if (pdata->netdev->features & NETIF_F_HW_VLAN_CTAG_FILTER)
			xgbe_enable_rx_vlan_filtering(pdata);
	}

	return 0;
}
#endif
#endif /* XGBE_SRIOV_PF */

#if XGBE_SRIOV_VF
static int xgbe_set_promiscuous_mode(struct xgbe_prv_data *pdata,
				 unsigned int enable)
{

	if (enable) {
		pdata->mbx.set_promiscuous_mode(pdata);
		TEST_PRNT("Promiscuous Mode Enabled");
	}
	else{
		pdata->mbx.clear_promiscuous_mode(pdata);
		TEST_PRNT("Promiscuous mode disabled");
	}

	return 0;

}
#endif /* XGBE_SRIOV_VF */

static int xgbe_set_all_multicast_mode(struct xgbe_prv_data *pdata,
		unsigned int enable)
{
#if XGBE_SRIOV_PF
	unsigned int val = enable ? 1 : 0;

	if (XGMAC_IOREAD_BITS(pdata, MAC_PFR, PM) == val)
		return 0;

	netif_dbg(pdata, drv, pdata->netdev, "%s allmulti mode\n",
			enable ? "entering" : "leaving");
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, PM, val);
#endif
	return 0;
}

static void xgbe_set_mac_reg(struct xgbe_prv_data *pdata,
	struct netdev_hw_addr *ha, unsigned int *mac_reg)
{
#if XGBE_SRIOV_PF
	unsigned int mac_addr_hi, mac_addr_lo;
	u8 *mac_addr;

	mac_addr_lo = 0;
	mac_addr_hi = 0;

	if (ha) {
		mac_addr = (u8 *)&mac_addr_lo;
		mac_addr[0] = ha->addr[0];
		mac_addr[1] = ha->addr[1];
		mac_addr[2] = ha->addr[2];
		mac_addr[3] = ha->addr[3];
		mac_addr = (u8 *)&mac_addr_hi;
		mac_addr[0] = ha->addr[4];
		mac_addr[1] = ha->addr[5];

		netif_dbg(pdata, drv, pdata->netdev,
				"adding mac address %pM at %#x\n",
				ha->addr, *mac_reg);

		XGMAC_SET_BITS(mac_addr_hi, MAC_MACA1HR, AE, 1);
	}

	XGMAC_IOWRITE(pdata, *mac_reg, mac_addr_hi);
	*mac_reg += MAC_MACA_INC;
	XGMAC_IOWRITE(pdata, *mac_reg, mac_addr_lo);
	*mac_reg += MAC_MACA_INC;
#endif
}

#if ELI_ENABLE
static void xgbe_set_mac_addn_addrs(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	struct netdev_hw_addr *ha;
	unsigned int mac_reg;
	unsigned int addn_macs;
	struct xgbe_eli_if *eli_if = &pdata->eli_if;

	mac_reg = MAC_MACA1HR;
	addn_macs = pdata->hw_feat.addn_mac;

	if (netdev_uc_count(netdev) > addn_macs) {
		TEST_PRNT("promiscuous mode is called in xgbe_set_mac_addn_addrs api\n");
		xgbe_set_promiscuous_mode(pdata, 1);
	} else {
		netdev_for_each_uc_addr(ha, netdev) {
			xgbe_set_mac_reg(pdata, ha, &mac_reg);
			addn_macs--;
		}

		if (netdev_mc_count(netdev) > addn_macs) {
			xgbe_set_all_multicast_mode(pdata, 1);
		} else {
			netdev_for_each_mc_addr(ha, netdev) {
				xgbe_set_mac_reg(pdata, ha, &mac_reg);
				if (ha)
					eli_if->mc_mac_add(pdata, ha->addr, 0);
				else
					eli_if->mc_mac_add(pdata, NULL, 0);
				addn_macs--;
			}
		}
	}

	/* Clear remaining additional MAC address entries */
	while (addn_macs--)
		xgbe_set_mac_reg(pdata, NULL, &mac_reg);
}

static void xgbe_set_mac_hash_table_vf(struct xgbe_prv_data *pdata)
{

	struct net_device *netdev = pdata->netdev;
	struct netdev_hw_addr *ha;
	struct vf_data_storage *vfinfo = NULL;
	unsigned int hash_reg;
	unsigned int hash_table_shift, hash_table_count;
	u32 hash_table[XGBE_MAC_HASH_TABLE_SIZE];
	u32 crc;
	unsigned int i, j;

	hash_table_shift = 26 - (pdata->hw_feat.hash_table_size >> 7);
	hash_table_count = pdata->hw_feat.hash_table_size / 32;
	memset(hash_table, 0, sizeof(hash_table));

	netdev_for_each_mc_addr(ha, netdev) {
		crc = bitrev32(~crc32_le(~0, ha->addr, ETH_ALEN));
		crc >>= hash_table_shift;
		hash_table[crc >> 5] |= (1 << (crc & 0x1f)); 
	}

	for(i = 0; i < max_vfs; i++){
		vfinfo = &pdata->vfinfo[i];
		if(!vfinfo)
			continue;

		for(j = 0; j < hash_table_count; j++){
			if(vfinfo->hash_table[j])
				hash_table[j] |= vfinfo->hash_table[j];
		}
	}

	/* Set the MAC Hash Table registers */
	hash_reg = MAC_HTR0;
	for (i = 0; i < hash_table_count; i++) {
		XGMAC_IOWRITE(pdata, hash_reg, hash_table[i]);
		hash_reg += MAC_HTR_INC;
	}
}

static void xgbe_set_mac_hash_table(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	struct netdev_hw_addr *ha;
	unsigned int hash_reg;
	unsigned int hash_table_shift, hash_table_count;
	u32 hash_table[XGBE_MAC_HASH_TABLE_SIZE];
	u32 crc;
	unsigned int i, count = 0, offset = -1, j;
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct vf_data_storage *vfinfo = NULL;

	hash_table_shift = 26 - (pdata->hw_feat.hash_table_size >> 7);
	hash_table_count = pdata->hw_feat.hash_table_size / 32;
	memset(hash_table, 0, sizeof(hash_table));

	if(pdata->pm_mode)
		eli_if->set_all_multicast_mode(pdata, ELI_FUNC_PF, 0);

	for_each_set_bit(offset, pdata->mc_indices, (ELI_MAC_INDEX + 1)){
		eli_if->mc_mac_del(pdata, offset, ELI_FUNC_PF);
	}

	/* Build the MAC Hash Table register values */
	netdev_for_each_uc_addr(ha, netdev) {
		crc = bitrev32(~crc32_le(~0, ha->addr, ETH_ALEN));
		crc >>= hash_table_shift;
		hash_table[crc >> 5] |= (1 << (crc & 0x1f));
	}

	netdev_for_each_mc_addr(ha, netdev) {
		crc = bitrev32(~crc32_le(~0, ha->addr, ETH_ALEN));
		crc >>= hash_table_shift;
		hash_table[crc >> 5] |= (1 << (crc & 0x1f));

		if(ha)
			if( eli_if->mc_mac_add(pdata, ha->addr, 0) == -1)
				count++;
	}

	if(count)
		eli_if->set_all_multicast_mode(pdata, ELI_FUNC_PF, 1);

	if(pdata->vfinfo && (pdata->flags & XGBE_FLAG_SRIOV_ENABLED)){
		for(i = 0; i < max_vfs; i++){
			vfinfo = &pdata->vfinfo[i];
			if(!vfinfo)
				continue;
			for(j = 0; j < hash_table_count; j++){
				if(vfinfo->hash_table[j])
					hash_table[j] |= vfinfo->hash_table[j];
			}
		}
	}
	/* Set the MAC Hash Table registers */
	hash_reg = MAC_HTR0;
	for (i = 0; i < hash_table_count; i++) {
		XGMAC_IOWRITE(pdata, hash_reg, hash_table[i]);
		hash_reg += MAC_HTR_INC;
	}
}
#else
static void xgbe_set_mac_addn_addrs(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	struct netdev_hw_addr *ha;
	unsigned int mac_reg;
	unsigned int addn_macs;

	mac_reg = MAC_MACA1HR;
	addn_macs = pdata->hw_feat.addn_mac;

	if (netdev_uc_count(netdev) > addn_macs) {
		xgbe_set_promiscuous_mode(pdata, 1);
	} else {
		netdev_for_each_uc_addr(ha, netdev) {
			xgbe_set_mac_reg(pdata, ha, &mac_reg);
			addn_macs--;
		}

		if (netdev_mc_count(netdev) > addn_macs) {
			xgbe_set_all_multicast_mode(pdata, 1);
		} else {
			netdev_for_each_mc_addr(ha, netdev) {
				xgbe_set_mac_reg(pdata, ha, &mac_reg);
				addn_macs--;
			}
		}
	}

	/* Clear remaining additional MAC address entries */
	while (addn_macs--)
		xgbe_set_mac_reg(pdata, NULL, &mac_reg);
}

static void xgbe_set_mac_hash_table(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	struct net_device *netdev = pdata->netdev;
	struct netdev_hw_addr *ha;
	unsigned int hash_reg;
	unsigned int hash_table_shift, hash_table_count;
	u32 hash_table[XGBE_MAC_HASH_TABLE_SIZE];
	u32 crc;
	unsigned int i;

	hash_table_shift = 26 - (pdata->hw_feat.hash_table_size >> 7);
	hash_table_count = pdata->hw_feat.hash_table_size / 32;
	memset(hash_table, 0, sizeof(hash_table));

	/* Build the MAC Hash Table register values */
	netdev_for_each_uc_addr(ha, netdev) {
		crc = bitrev32(~crc32_le(~0, ha->addr, ETH_ALEN));
		crc >>= hash_table_shift;
		hash_table[crc >> 5] |= (1 << (crc & 0x1f));
	}

	netdev_for_each_mc_addr(ha, netdev) {
		crc = bitrev32(~crc32_le(~0, ha->addr, ETH_ALEN));
		crc >>= hash_table_shift;
		hash_table[crc >> 5] |= (1 << (crc & 0x1f));
	}

	/* Set the MAC Hash Table registers */
	hash_reg = MAC_HTR0;
	for (i = 0; i < hash_table_count; i++) {
		XGMAC_IOWRITE(pdata, hash_reg, hash_table[i]);
		hash_reg += MAC_HTR_INC;
	}
#endif

#if XGBE_SRIOV_VF
	if(!pdata->mc_mutex)
		pdata->mbx.set_mc_addrs(pdata);
#endif
}
#endif

static int xgbe_add_mac_addresses(struct xgbe_prv_data *pdata)
{
	if (pdata->hw_feat.hash_table_size)
		xgbe_set_mac_hash_table(pdata);
	else
		xgbe_set_mac_addn_addrs(pdata);

	return 0;
}

#if XGBE_SRIOV_PF
static int xgbe_clear_mac_address_sriov(struct xgbe_prv_data *pdata, int vf)
{
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf];
	unsigned int mac_reg;
	unsigned short num_dma;

	mac_reg = vfinfo->mac_address_offset;
	num_dma = vfinfo->num_dma_channels;

	XGMAC_IOWRITE(pdata, mac_reg, 0);
	mac_reg += MAC_MACA_INC;
	XGMAC_IOWRITE(pdata, mac_reg, 0); 

	return 0;
}

static int xgbe_set_mac_address_sriov(struct xgbe_prv_data *pdata, u8 *addr, int vf)
{
	unsigned int mac_addr_hi, mac_addr_lo, mac_reg;
	unsigned short dma_ch, num_dma;
	struct vf_data_storage *vfinfo  = &pdata->vfinfo[vf];
	mac_addr_hi = (((unsigned int)addr[5]) << 8) | (((unsigned int)addr[4]) << 0);
	mac_addr_lo = (((unsigned int)addr[3]) << 24) | (((unsigned int)addr[2]) << 16) |
		(((unsigned int)addr[1]) << 8) | (((unsigned int)addr[0]) << 0);

	mac_reg = vfinfo->mac_address_offset;
	dma_ch = vfinfo->dma_channel_start_num;
	num_dma = vfinfo->num_dma_channels;
	mac_addr_hi &= 0xFFFF;

	XGMAC_SET_BITS(mac_addr_hi, MAC_MACA1HR, AE, 1);

	XGMAC_SET_BITS(mac_addr_hi, MAC_MACA1HR, DCS, dma_ch);
	XGMAC_IOWRITE(pdata, mac_reg, mac_addr_hi);
	mac_reg += MAC_MACA_INC;
	XGMAC_IOWRITE(pdata, mac_reg, mac_addr_lo);

	return 0;
}
#endif

#if XGBE_SRIOV_PF
#if ELI_ENABLE
static int xgbe_set_mac_address(struct xgbe_prv_data *pdata, u8 *addr)
{
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	unsigned int mac_addr_hi, mac_addr_lo;
	mac_addr_hi = (((unsigned int)addr[5]) << 8) | (((unsigned int)addr[4]) << 0);
	mac_addr_lo = (((unsigned int)addr[3]) << 24) | (((unsigned int)addr[2]) << 16) |
		(((unsigned int)addr[1]) << 8) | (((unsigned int)addr[0]) << 0);

	XGMAC_IOWRITE(pdata, MAC_MACA0HR, mac_addr_hi);
	XGMAC_IOWRITE(pdata, MAC_MACA0LR, mac_addr_lo);
	eli_if->mac_add(pdata, 0, addr);
	return 0;
}
#else
static int xgbe_set_mac_address(struct xgbe_prv_data *pdata, u8 *addr)
{
	unsigned int mac_addr_hi, mac_addr_lo;
	unsigned int num_dma, dma_ch, mac_reg;
	int i;
	mac_addr_hi = (addr[5] <<  8) | (addr[4] <<  0);
	mac_addr_lo = (addr[3] << 24) | (addr[2] << 16) |
		(addr[1] <<  8) | (addr[0] <<  0);

	XGMAC_IOWRITE(pdata, MAC_MACA0HR, mac_addr_hi);
	XGMAC_IOWRITE(pdata, MAC_MACA0LR, mac_addr_lo);

	if (pdata->flags & XGBE_FLAG_SRIOV_ENABLED) {
		num_dma = 4;
		dma_ch = 1;
		mac_reg = MAC_MACA0LR + MAC_MACA_INC;
		XGMAC_SET_BITS(mac_addr_hi, MAC_MACA1HR, AE, 1);

		for (i = 0; i < (num_dma - 1); i++) {
			XGMAC_SET_BITS(mac_addr_hi, MAC_MACA1HR, DCS, (dma_ch + i));

			XGMAC_IOWRITE(pdata, mac_reg, mac_addr_hi);
			mac_reg += MAC_MACA_INC;
			XGMAC_IOWRITE(pdata, mac_reg, mac_addr_lo);
			mac_reg += MAC_MACA_INC;
		}
	}

	return 0;
}
#endif
#endif

#if XGBE_SRIOV_VF
static int xgbe_set_mac_address(struct xgbe_prv_data *pdata, u8 *addr)
{
	unsigned int mac_addr_hi, mac_addr_lo;

	mac_addr_hi = (((unsigned int)addr[5]) << 8) | (((unsigned int)addr[4]) << 0);
	mac_addr_lo = (((unsigned int)addr[3]) << 24) | (((unsigned int)addr[2]) << 16) |
		(((unsigned int)addr[1]) << 8) | (((unsigned int)addr[0]) << 0);


	if(!pdata->uc_mutex) {
		pdata->mbx.set_mac_addr(pdata, addr);
		pdata->uc_mutex = 1;
	}

	return 0;
}
#endif

static int xgbe_config_rx_mode(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	unsigned int pr_mode, am_mode;

	pr_mode = ((netdev->flags & IFF_PROMISC) != 0);
	am_mode = ((netdev->flags & IFF_ALLMULTI) != 0);

	xgbe_set_promiscuous_mode(pdata, pr_mode);
	xgbe_set_all_multicast_mode(pdata, am_mode);

	xgbe_add_mac_addresses(pdata);

	return 0;
}

#if XGBE_SRIOV_PF
static int xgbe_clr_gpio(struct xgbe_prv_data *pdata, unsigned int gpio)
{
	unsigned int reg;

	if (gpio > 15)
		return -EINVAL;

	reg = XGMAC_IOREAD(pdata, MAC_GPIOSR);

	reg &= ~(1 << (gpio + 16));
	XGMAC_IOWRITE(pdata, MAC_GPIOSR, reg);

	return 0;
}

static int xgbe_set_gpio(struct xgbe_prv_data *pdata, unsigned int gpio)
{
	unsigned int reg;

	if (gpio > 15)
		return -EINVAL;

	reg = XGMAC_IOREAD(pdata, MAC_GPIOSR);

	reg |= (1 << (gpio + 16));
	XGMAC_IOWRITE(pdata, MAC_GPIOSR, reg);

	return 0;
}

static int xgbe_read_mmd_regs(struct xgbe_prv_data *pdata, int prtad,
		int mmd_reg)
{
	if(pdata->dev_id == BCM8989X_PF_ID) {
		unsigned int reg = (0xF2000000 | (prtad << 17) | (mmd_reg << 1));
		unsigned int reg_value;
		spin_lock(&pdata->phy_access_lock);
		pdata->phy_if.phy_impl.reg_read(pdata, reg, 2, &reg_value);
		spin_unlock(&pdata->phy_access_lock);
		return reg_value;
	}
	return 0;
}

static void xgbe_write_mmd_regs(struct xgbe_prv_data *pdata, int prtad,
		int mmd_reg, int mmd_data)
{

	if(pdata->dev_id == BCM8989X_PF_ID) {
		unsigned int reg = (0xF2000000 | (prtad << 17) | (mmd_reg << 1));
		spin_lock(&pdata->phy_access_lock);
		pdata->phy_if.phy_impl.reg_write(pdata, reg, mmd_data, 2);
		spin_unlock(&pdata->phy_access_lock);
	}
}

static int xgbe_write_ext_mii_regs(struct xgbe_prv_data *pdata, int addr,
		int reg, u16 val)
{
	return 0;
}

static int xgbe_read_ext_mii_regs(struct xgbe_prv_data *pdata, int addr,
		int reg)
{
	return XGMAC_IOREAD_BITS(pdata, MAC_MDIOSCCDR, DATA);
}

static int xgbe_set_ext_mii_mode(struct xgbe_prv_data *pdata, unsigned int port,
		enum xgbe_mdio_mode mode)
{
	return 0;
}
#endif

static int xgbe_tx_complete(struct xgbe_ring_desc *rdesc)
{
	return !XGMAC_GET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN);
}

static int xgbe_disable_rx_csum(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	if(!(pdata->rx_csum_semaphore))
		XGMAC_IOWRITE_BITS(pdata, MAC_RCR, IPC, 0);
#endif

#if XGBE_SRIOV_VF
	pdata->mbx.enable_disable_rx_csum(pdata, DISABLE);
#endif
	return 0;
}

static int xgbe_enable_rx_csum(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	if(XGMAC_IOREAD_BITS(pdata, MAC_RCR, IPC))
		return 0;

	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, IPC, 1);
#endif

#if XGBE_SRIOV_VF
	pdata->mbx.enable_disable_rx_csum(pdata, ENABLE);
#endif
	return 0;
}

static void xgbe_tx_desc_reset(struct xgbe_ring_data *rdata)
{
	struct xgbe_ring_desc *rdesc = rdata->rdesc;

	/* Reset the Tx descriptor
	 * Set buffer 1 (lo) address to zero
	 * Set buffer 1 (hi) address to zero
	 * Reset all other control bits (IC, TTSE, B2L & B1L)
	 * Reset all other control bits (OWN, CTXT, FD, LD, CPC, CIC, etc)
	 */
	rdesc->desc0 = 0;
	rdesc->desc1 = 0;
	rdesc->desc2 = 0;
	rdesc->desc3 = 0;

	/* Make sure ownership is written to the descriptor */
	dma_wmb();
}

static void xgbe_tx_desc_init(struct xgbe_channel *channel)
{
	struct xgbe_ring *ring = channel->tx_ring;
	struct xgbe_ring_data *rdata;
	int i;
	int start_index = ring->cur;

	DBGPR("--> %s\n", __func__);

	/* Initialze all descriptors */
	for (i = 0; i < ring->rdesc_count; i++) {
		rdata = XGBE_GET_DESC_DATA(ring, i);

		/* Initialize Tx descriptor */
		xgbe_tx_desc_reset(rdata);
	}

	/* Update the total number of Tx descriptors */
	XGMAC_DMA_IOWRITE(channel, DMA_CH_TDRLR, ring->rdesc_count - 1);

	/* Update the starting address of descriptor ring */
	rdata = XGBE_GET_DESC_DATA(ring, start_index);
	XGMAC_DMA_IOWRITE(channel, DMA_CH_TDLR_HI,
			upper_32_bits(rdata->rdesc_dma));
	XGMAC_DMA_IOWRITE(channel, DMA_CH_TDLR_LO,
			lower_32_bits(rdata->rdesc_dma));
	TEST_PRNT("Tx Head Lo = %x\n", lower_32_bits(rdata->rdesc_dma));
	DBGPR("<-- %s\n", __func__);
}

static void xgbe_rx_desc_reset(struct xgbe_prv_data *pdata,
		struct xgbe_ring_data *rdata, unsigned int index)
{
	struct xgbe_ring_desc *rdesc = rdata->rdesc;
	unsigned int rx_usecs = pdata->rx_usecs;
	unsigned int rx_frames = pdata->rx_frames;
	unsigned int inte;
	dma_addr_t hdr_dma, buf_dma;

	if (!rx_usecs && !rx_frames) {
		/* No coalescing, interrupt for every descriptor */
		inte = 1;
	} else {
		/* Set interrupt based on Rx frame coalescing setting */
		if (rx_frames && !((index + 1) % rx_frames))
			inte = 1;
		else
			inte = 0;
	}

	/* Reset the Rx descriptor
	 * Set buffer 1 (lo) address to header dma address (lo)
	 * Set buffer 1 (hi) address to header dma address (hi)
	 * Set buffer 2 (lo) address to buffer dma address (lo)
	 * Set buffer 2 (hi) address to buffer dma address (hi) and
	 * set control bits OWN and INTE
	 */
	hdr_dma = rdata->rx.hdr.dma_base + rdata->rx.hdr.dma_off;
	buf_dma = rdata->rx.buf.dma_base + rdata->rx.buf.dma_off;
	rdesc->desc0 = cpu_to_le32(lower_32_bits(hdr_dma));
	rdesc->desc1 = cpu_to_le32(upper_32_bits(hdr_dma));
	rdesc->desc2 = cpu_to_le32(lower_32_bits(buf_dma));
	rdesc->desc3 = cpu_to_le32(upper_32_bits(buf_dma));

#if XGBE_SRIOV_PF
    if (BCM8956X_A0_PF_ID == pdata->dev_id) {
		XGMAC_SET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, INTE, 0);
	} else 
#endif
	{
		XGMAC_SET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, INTE, inte);
	}

	/* Since the Rx DMA engine is likely running, make sure everything
	 * is written to the descriptor(s) before setting the OWN bit
	 * for the descriptor
	 */
	dma_wmb();

	XGMAC_SET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, OWN, 1);

	/* Make sure ownership is written to the descriptor */
	dma_wmb();
}

static void xgbe_rx_desc_init(struct xgbe_channel *channel)
{
	struct xgbe_prv_data *pdata = channel->pdata;
	struct xgbe_ring *ring = channel->rx_ring;
	struct xgbe_ring_data *rdata;
	unsigned int start_index = ring->cur;
	unsigned int i;

	DBGPR("--> %s\n", __func__);

	/* Initialize all descriptors */
	for (i = 0; i < ring->rdesc_count; i++) {
		rdata = XGBE_GET_DESC_DATA(ring, i);

		/* Initialize Rx descriptor */
		xgbe_rx_desc_reset(pdata, rdata, i);
	}

	/* Update the total number of Rx descriptors */
	XGMAC_DMA_IOWRITE(channel, DMA_CH_RDRLR, ring->rdesc_count - 1);

	/* Update the starting address of descriptor ring */
	rdata = XGBE_GET_DESC_DATA(ring, start_index);
	XGMAC_DMA_IOWRITE(channel, DMA_CH_RDLR_HI,
			upper_32_bits(rdata->rdesc_dma));
	XGMAC_DMA_IOWRITE(channel, DMA_CH_RDLR_LO,
			lower_32_bits(rdata->rdesc_dma));
	TEST_PRNT("DMA_CH_RDLR_HI = %x\n",
		XGMAC_DMA_IOREAD(channel, DMA_CH_RDLR_HI));
	TEST_PRNT("DMA_CH_RDLR_LO = %x\n",
		XGMAC_DMA_IOREAD(channel, DMA_CH_RDLR_LO));

	/* Update the Rx Descriptor Tail Pointer */
	rdata = XGBE_GET_DESC_DATA(ring, start_index + ring->rdesc_count - 1);
	XGMAC_DMA_IOWRITE(channel, DMA_CH_RDTR_LO,
			lower_32_bits(rdata->rdesc_dma));

	DBGPR("<-- %s\n", __func__);
}

#if XGBE_SRIOV_PF
static void xgbe_update_tstamp_addend(struct xgbe_prv_data *pdata,
		unsigned int addend)
{
	unsigned int count = 10000;

	/* Set the addend register value and tell the device */
	XGMAC_IOWRITE(pdata, MAC_TSAR, addend);
	XGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSADDREG, 1);

	/* Wait for addend update to complete */
	while (--count && XGMAC_IOREAD_BITS(pdata, MAC_TSCR, TSADDREG))
		udelay(5);

	if (!count)
		netdev_err(pdata->netdev,
				"timed out updating timestamp addend register\n");
}

static void xgbe_set_tstamp_time(struct xgbe_prv_data *pdata, unsigned int sec,
		unsigned int nsec)
{
	unsigned int count = 10000;

	/* Set the time values and tell the device */
	XGMAC_IOWRITE(pdata, MAC_STSUR, sec);
	XGMAC_IOWRITE(pdata, MAC_STNUR, nsec);
	XGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSINIT, 1);

	/* Wait for time update to complete */
	while (--count && XGMAC_IOREAD_BITS(pdata, MAC_TSCR, TSINIT))
		udelay(5);

	if (!count)
		netdev_err(pdata->netdev, "timed out initializing timestamp\n");
}

static u64 xgbe_get_tstamp_time(struct xgbe_prv_data *pdata)
{
	u64 nsec;

	nsec = XGMAC_IOREAD(pdata, MAC_STSR);
	nsec *= NSEC_PER_SEC;
	nsec += XGMAC_IOREAD(pdata, MAC_STNR);

	return nsec;
}

static u64 xgbe_get_tx_tstamp(struct xgbe_prv_data *pdata)
{
	unsigned int tx_snr, tx_ssr;
	u64 nsec;

	if (pdata->vdata->tx_tstamp_workaround) {
		tx_snr = XGMAC_IOREAD(pdata, MAC_TXSNR);
		tx_ssr = XGMAC_IOREAD(pdata, MAC_TXSSR);
	} else {
		tx_ssr = XGMAC_IOREAD(pdata, MAC_TXSSR);
		tx_snr = XGMAC_IOREAD(pdata, MAC_TXSNR);
	}

	if (XGMAC_GET_BITS(tx_snr, MAC_TXSNR, TXTSSTSMIS))
		return 0;

	nsec = tx_ssr;
	nsec *= NSEC_PER_SEC;
	nsec += tx_snr;

	return nsec;
}
#endif

static void xgbe_get_rx_tstamp(struct xgbe_packet_data *packet,
		struct xgbe_ring_desc *rdesc)
{
	u64 nsec;

	if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_CONTEXT_DESC3, TSA) &&
		!XGMAC_GET_BITS_LE(rdesc->desc3, RX_CONTEXT_DESC3, TSD)) {
		nsec = le32_to_cpu(rdesc->desc1);
		nsec *= NSEC_PER_SEC;
		nsec += le32_to_cpu(rdesc->desc0);
		if (nsec != 0xffffffffffffffffULL) {
			packet->rx_tstamp = nsec;
			XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
					RX_TSTAMP, 1);
		}
	}
}

#if XGBE_SRIOV_VF
static u64 xgbe_get_tx_tstamp(struct xgbe_prv_data *pdata)
{
	unsigned int tx_snr, tx_ssr;
	u64 nsec;
	tx_ssr = pdata->tx_tstamp_hi;
	tx_snr = pdata->tx_tstamp_lo;

	if (XGMAC_GET_BITS(tx_snr, MAC_TXSNR, TXTSSTSMIS))
		return 0;

	nsec = tx_ssr;
	nsec *= NSEC_PER_SEC;
	nsec += tx_snr;

	return nsec;
}

static int xgbe_config_tstamp(struct xgbe_prv_data *pdata,
				 unsigned int mac_tscr, unsigned int mac_pto)
{
	TEST_PRNT("---> %s\n", __func__);
	pdata->mbx.config_tstamp(pdata, mac_tscr, mac_pto);

	/* Initialize the timecounter */
	timecounter_init(&pdata->tstamp_tc, &pdata->tstamp_cc,
			ktime_to_ns(ktime_get_real()));

	return 0;
}

#endif

#if XGBE_SRIOV_PF
static int xgbe_config_tstamp(struct xgbe_prv_data *pdata,
		unsigned int mac_tscr)
{
	/* Set one nano-second accuracy */
	XGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSCTRLSSR, 1);

	/* Set fine timestamp update */
	XGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSCFUPDT, 1);

	/* Overwrite earlier timestamps */
	XGMAC_SET_BITS(mac_tscr, MAC_TSCR, TXTSSTSM, 1);

	XGMAC_IOWRITE(pdata, MAC_TSCR, mac_tscr);

	/* Exit if timestamping is not enabled */
	if (!XGMAC_GET_BITS(mac_tscr, MAC_TSCR, TSENA))
		return 0;

	/* Initialize time registers */
	XGMAC_IOWRITE_BITS(pdata, MAC_SSIR, SSINC, XGBE_TSTAMP_SSINC);
	XGMAC_IOWRITE_BITS(pdata, MAC_SSIR, SNSINC, XGBE_TSTAMP_SNSINC);
	xgbe_update_tstamp_addend(pdata, pdata->tstamp_addend);
	xgbe_set_tstamp_time(pdata, 0, 0);

	/* Initialize the timecounter */
	timecounter_init(&pdata->tstamp_tc, &pdata->tstamp_cc,
			ktime_to_ns(ktime_get_real()));

	return 0;
}
#endif

static void xgbe_tx_start_xmit(struct xgbe_channel *channel,
		struct xgbe_ring *ring)
{
	struct xgbe_prv_data *pdata = channel->pdata;
	struct xgbe_ring_data *rdata;

	/* Make sure everything is written before the register write */
	wmb();

	/* Issue a poll command to Tx DMA by writing address
	 * of next immediate free descriptor
	 */

	rdata = XGBE_GET_DESC_DATA(ring, ring->cur);
	XGMAC_DMA_IOWRITE(channel, DMA_CH_TDTR_LO,
			lower_32_bits(rdata->rdesc_dma));
	TEST_PRNT("Tx Tail = %x\n", lower_32_bits(rdata->rdesc_dma));
	/* Start the Tx timer */
	if (pdata->tx_usecs && !channel->tx_timer_active) {
		channel->tx_timer_active = 1;
		mod_timer(&channel->tx_timer,
				jiffies + usecs_to_jiffies(pdata->tx_usecs));
	}

	ring->tx.xmit_more = 0;
}

static void xgbe_dev_xmit(struct xgbe_channel *channel)
{
	struct xgbe_prv_data *pdata = channel->pdata;
	struct xgbe_ring *ring = channel->tx_ring;
	struct xgbe_ring_data *rdata;
	struct xgbe_ring_desc *rdesc;
	struct xgbe_packet_data *packet = &ring->packet_data;
	unsigned int tx_packets, tx_bytes;
	unsigned int csum, tso, vlan, vxlan;
	unsigned int tso_context, vlan_context;
	unsigned int tx_set_ic;
	int start_index = ring->cur;
	int cur_index = ring->cur;
	int i;

	DBGPR("--> %s\n", __func__);

	tx_packets = packet->tx_packets;
	tx_bytes = packet->tx_bytes;

	csum = XGMAC_GET_BITS(packet->attributes, TX_PACKET_ATTRIBUTES,
			CSUM_ENABLE);
	tso = XGMAC_GET_BITS(packet->attributes, TX_PACKET_ATTRIBUTES,
			TSO_ENABLE);
	vlan = XGMAC_GET_BITS(packet->attributes, TX_PACKET_ATTRIBUTES,
			VLAN_CTAG);
	vxlan = XGMAC_GET_BITS(packet->attributes, TX_PACKET_ATTRIBUTES,
			VXLAN);

	if (tso && (packet->mss != ring->tx.cur_mss))
		tso_context = 1;
	else
		tso_context = 0;

	if (vlan && (packet->vlan_ctag != ring->tx.cur_vlan_ctag))
		vlan_context = 1;
	else
		vlan_context = 0;

	/* Determine if an interrupt should be generated for this Tx:
	 *   Interrupt:
	 *     - Tx frame count exceeds the frame count setting
	 *     - Addition of Tx frame count to the frame count since the
	 *       last interrupt was set exceeds the frame count setting
	 *   No interrupt:
	 *     - No frame count setting specified (ethtool -C ethX tx-frames 0)
	 *     - Addition of Tx frame count to the frame count since the
	 *       last interrupt was set does not exceed the frame count setting
	 */
	ring->coalesce_count += tx_packets;
	if (!pdata->tx_frames)
		tx_set_ic = 0;
	else if (tx_packets > pdata->tx_frames)
		tx_set_ic = 1;
	else if ((ring->coalesce_count % pdata->tx_frames) < tx_packets)
		tx_set_ic = 1;
	else
		tx_set_ic = 0;

	rdata = XGBE_GET_DESC_DATA(ring, cur_index);
	rdesc = rdata->rdesc;

	/* Create a context descriptor if this is a TSO packet */
	if (tso_context || vlan_context) {
		if (tso_context) {
			netif_dbg(pdata, tx_queued, pdata->netdev,
					"TSO context descriptor, mss=%u\n",
					packet->mss);

			/* Set the MSS size */
			XGMAC_SET_BITS_LE(rdesc->desc2, TX_CONTEXT_DESC2,
					MSS, packet->mss);

			/* Mark it as a CONTEXT descriptor */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3,
					CTXT, 1);

			/* Indicate this descriptor contains the MSS */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3,
					TCMSSV, 1);

			ring->tx.cur_mss = packet->mss;
		}

		if (vlan_context) {
			netif_dbg(pdata, tx_queued, pdata->netdev,
					"VLAN context descriptor, ctag=%u\n",
					packet->vlan_ctag);

			TEST_PRNT("CTAG : %d\n", packet->vlan_ctag);
			/* Mark it as a CONTEXT descriptor */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3,
					CTXT, 1);

			/* Set the VLAN tag */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3,
					VT, packet->vlan_ctag);

			/* Indicate this descriptor contains the VLAN tag */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3,
					VLTV, 1);

			ring->tx.cur_vlan_ctag = packet->vlan_ctag;
		}

		cur_index++;
		rdata = XGBE_GET_DESC_DATA(ring, cur_index);
		rdesc = rdata->rdesc;
	}

	/* Update buffer address (for TSO this is the header) */
	rdesc->desc0 =	cpu_to_le32(lower_32_bits(rdata->skb_dma));
	rdesc->desc1 =	cpu_to_le32(upper_32_bits(rdata->skb_dma));

#if BRCM_FH
	/* Update the buffer length */
	XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, HL_B1L,
			rdata->skb_dma_len + BRCM_FH_PKT_PREFIX_LEN);
#else
	/* Update the buffer length */
	XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, HL_B1L,
			rdata->skb_dma_len);
#endif

	/* VLAN tag insertion check */
	if (vlan)
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, VTIR,
				TX_NORMAL_DESC2_VLAN_INSERT);

	/* Timestamp enablement check */
	if (XGMAC_GET_BITS(packet->attributes, TX_PACKET_ATTRIBUTES, PTP))
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, TTSE, 1);

	/* Mark it as First Descriptor */
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, FD, 1);

	/* Mark it as a NORMAL descriptor */
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CTXT, 0);

	/* Set OWN bit if not the first descriptor */
	if (cur_index != start_index)
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN, 1);

	if (tso) {
		/* Enable TSO */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, TSE, 1);
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, TCPPL,
				packet->tcp_payload_len);
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, TCPHDRLEN,
				packet->tcp_header_len / 4);

		pdata->ext_stats.tx_tso_packets += tx_packets;
	} else {
		/* Enable CRC and Pad Insertion */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CPC, 0);

		/* Enable HW CSUM */
		if (csum)
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3,
					CIC, 0x3);

		/* Set the total length to be transmitted */
#if BRCM_FH
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, FL,
				packet->length + BRCM_FH_LEN);
#else
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, FL,
				packet->length);
#endif
	}

	if (vxlan) {
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, VNP,
				TX_NORMAL_DESC3_VXLAN_PACKET);

		pdata->ext_stats.tx_vxlan_packets += packet->tx_packets;
	}

	for (i = cur_index - start_index + 1; i < packet->rdesc_count; i++) {
		cur_index++;
		rdata = XGBE_GET_DESC_DATA(ring, cur_index);
		rdesc = rdata->rdesc;

		/* Update buffer address */
		rdesc->desc0 = cpu_to_le32(lower_32_bits(rdata->skb_dma));
		rdesc->desc1 = cpu_to_le32(upper_32_bits(rdata->skb_dma));

		/* Update the buffer length */
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, HL_B1L,
				rdata->skb_dma_len);

		/* Set OWN bit */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN, 1);

		/* Mark it as NORMAL descriptor */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CTXT, 0);

		/* Enable HW CSUM */
		if (csum)
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3,
					CIC, 0x3);
	}

	/* Set LAST bit for the last descriptor */
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, LD, 1);

	/* Set IC bit based on Tx coalescing settings */
	if (tx_set_ic)
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, IC, 1);

	/* Save the Tx info to report back during cleanup */
	rdata->tx.packets = tx_packets;
	rdata->tx.bytes = tx_bytes;

	pdata->ext_stats.txq_packets[channel->queue_index] += tx_packets;
	pdata->ext_stats.txq_bytes[channel->queue_index] += tx_bytes;

	/* In case the Tx DMA engine is running, make sure everything
	 * is written to the descriptor(s) before setting the OWN bit
	 * for the first descriptor
	 */
	dma_wmb();

	/* Set OWN bit for the first descriptor */
	rdata = XGBE_GET_DESC_DATA(ring, start_index);
	rdesc = rdata->rdesc;
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN, 1);

#if XGBE_SRIOV_VF
	/* vf hack to test MSI-X	IOC */
	XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, IC, 1);
#endif

	if (netif_msg_tx_queued(pdata))
		xgbe_dump_tx_desc(pdata, ring, start_index,
				packet->rdesc_count, 1);

	/* Make sure ownership is written to the descriptor */
	smp_wmb();

	ring->cur = cur_index + 1;
	
	/* Currently supported kernel versions are 4.15.x and 5.4.x */
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)))
	if (!packet->skb->xmit_more ||
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (!netdev_xmit_more() ||
#endif
			netif_xmit_stopped(netdev_get_tx_queue(pdata->netdev,
					channel->queue_index)))
		xgbe_tx_start_xmit(channel, ring);
	else
		ring->tx.xmit_more = 1;

	DBGPR("	%s: descriptors %u to %u written\n",
			channel->name, start_index & (ring->rdesc_count - 1),
			(ring->cur - 1) & (ring->rdesc_count - 1));

	DBGPR("<-- %s\n", __func__);
}

static int xgbe_dev_read(struct xgbe_channel *channel)
{
	struct xgbe_prv_data *pdata = channel->pdata;
	struct xgbe_ring *ring = channel->rx_ring;
	struct xgbe_ring_data *rdata;
	struct xgbe_ring_desc *rdesc;
	struct xgbe_packet_data *packet = &ring->packet_data;
	struct net_device *netdev = pdata->netdev;
	unsigned int err, etlt, l34t;

	DBGPR("--> %s: cur = %d\n", __func__, ring->cur);

	rdata = XGBE_GET_DESC_DATA(ring, ring->cur);
	rdesc = rdata->rdesc;

	/* Check for data availability */
	if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, OWN))
		return 1;

	/* Make sure descriptor fields are read after reading the OWN bit */
	dma_rmb();

	if (netif_msg_rx_status(pdata))
		xgbe_dump_rx_desc(pdata, ring, ring->cur);

	if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, CTXT)) {
		/* Timestamp Context Descriptor */
		xgbe_get_rx_tstamp(packet, rdesc);

		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				CONTEXT, 1);
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				CONTEXT_NEXT, 0);
		return 0;
	}

	/* Normal Descriptor, be sure Context Descriptor bit is off */
	XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, CONTEXT, 0);

	/* Indicate if a Context Descriptor is next */
	if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, CDA))
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				CONTEXT_NEXT, 1);

	/* Get the header length */
	if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, FD)) {
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				FIRST, 1);
		rdata->rx.hdr_len = XGMAC_GET_BITS_LE(rdesc->desc2,
				RX_NORMAL_DESC2, HL);
		if (rdata->rx.hdr_len)
			pdata->ext_stats.rx_split_header_packets++;
	} else {
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				FIRST, 0);
	}
	
#if XGBE_SRIOV_VF
	/* Get the RSS hash */
	if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, RSV)) {
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				RSS_HASH, 1);

		packet->rss_hash = le32_to_cpu(rdesc->desc1);

		l34t = XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, L34T);
		switch (l34t) {
		case RX_DESC3_L34T_IPV4_TCP:
		case RX_DESC3_L34T_IPV4_UDP:
		case RX_DESC3_L34T_IPV6_TCP:
		case RX_DESC3_L34T_IPV6_UDP:
			packet->rss_hash_type = PKT_HASH_TYPE_L4;
			break;
		default:
			packet->rss_hash_type = PKT_HASH_TYPE_L3;
		}
	}
#endif

	/* Not all the data has been transferred for this packet */
	if (!XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, LD))
		return 0;

	/* This is the last of the data for this packet */
	XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
			LAST, 1);

	/* Get the packet length */
	rdata->rx.len = XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, PL);

	/* Set checksum done indicator as appropriate */
	if (netdev->features & NETIF_F_RXCSUM) {
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				CSUM_DONE, 1);
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				TNPCSUM_DONE, 1);
	}

	/* Set the tunneled packet indicator */
	if (XGMAC_GET_BITS_LE(rdesc->desc2, RX_NORMAL_DESC2, TNP)) {
		XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
				TNP, 1);
		pdata->ext_stats.rx_vxlan_packets++;

		l34t = XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, L34T);
		switch (l34t) {
			case RX_DESC3_L34T_IPV4_UNKNOWN:
			case RX_DESC3_L34T_IPV6_UNKNOWN:
				XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
						TNPCSUM_DONE, 0);
				break;
		}
	}

	/* Check for errors (only valid in last descriptor) */
	err = XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, ES);
	etlt = XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, ETLT);
	netif_dbg(pdata, rx_status, netdev, "err=%u, etlt=%#x\n", err, etlt);

	if (!err || !etlt) {
		/* No error if err is 0 or etlt is 0 */
		if ((etlt == 0x09) &&
				(netdev->features & NETIF_F_HW_VLAN_CTAG_RX)) {
			XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
					VLAN_CTAG, 1);
			packet->vlan_ctag = XGMAC_GET_BITS_LE(rdesc->desc0,
					RX_NORMAL_DESC0,
					OVT);
			netif_dbg(pdata, rx_status, netdev, "vlan-ctag=%#06x\n",
					packet->vlan_ctag);
			TEST_PRNT("vlan_ctag : %d\n", packet->vlan_ctag);
		}
	} else {
		unsigned int tnp = XGMAC_GET_BITS(packet->attributes,
				RX_PACKET_ATTRIBUTES, TNP);

		if ((etlt == 0x05) || (etlt == 0x06)) {
			XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
					CSUM_DONE, 0);
			XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
					TNPCSUM_DONE, 0);
			pdata->ext_stats.rx_csum_errors++;
		} else if (tnp && ((etlt == 0x09) || (etlt == 0x0a))) {
			XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
					CSUM_DONE, 0);
			XGMAC_SET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
					TNPCSUM_DONE, 0);
			pdata->ext_stats.rx_vxlan_csum_errors++;
		} else {
			XGMAC_SET_BITS(packet->errors, RX_PACKET_ERRORS,
					FRAME, 1);
		}
	}

	pdata->ext_stats.rxq_packets[channel->queue_index]++;
	pdata->ext_stats.rxq_bytes[channel->queue_index] += rdata->rx.len;

	DBGPR("<-- %s: %s - descriptor=%u (cur=%d)\n", __func__,
		channel->name, ring->cur & (ring->rdesc_count - 1),
		ring->cur);

	TEST_PRNT("<-- %s: %s - descriptor=%u (cur=%d)\n", __func__,
		channel->name, ring->cur & (ring->rdesc_count - 1),
		ring->cur);

	return 0;
}

static int xgbe_is_context_desc(struct xgbe_ring_desc *rdesc)
{
	/* Rx and Tx share CTXT bit, so check TDES3.CTXT bit */
	return XGMAC_GET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CTXT);
}

static int xgbe_is_last_desc(struct xgbe_ring_desc *rdesc)
{
	/* Rx and Tx share LD bit, so check TDES3.LD bit */
	return XGMAC_GET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, LD);
}

static int xgbe_enable_int(struct xgbe_channel *channel,
		enum xgbe_int int_id)
{
	switch (int_id) {
		case XGMAC_INT_DMA_CH_SR_TI:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TIE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_TPS:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TXSE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_TBU:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TBUE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_RI:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RIE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_RBU:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RBUE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_RPS:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RSE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_TI_RI:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TIE, 1);
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RIE, 1);
			break;
		case XGMAC_INT_DMA_CH_SR_FBE:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, FBEE, 1);
			break;
		case XGMAC_INT_DMA_ALL:
			channel->curr_ier |= channel->saved_ier;
			break;
		default:
			return -1;
	}

	XGMAC_DMA_IOWRITE(channel, DMA_CH_IER, channel->curr_ier);

	return 0;
}

static int xgbe_disable_int(struct xgbe_channel *channel,
		enum xgbe_int int_id)
{
	switch (int_id) {
		case XGMAC_INT_DMA_CH_SR_TI:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TIE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_TPS:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TXSE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_TBU:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TBUE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_RI:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RIE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_RBU:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RBUE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_RPS:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RSE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_TI_RI:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, TIE, 0);
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, RIE, 0);
			break;
		case XGMAC_INT_DMA_CH_SR_FBE:
			XGMAC_SET_BITS(channel->curr_ier, DMA_CH_IER, FBEE, 0);
			break;
		case XGMAC_INT_DMA_ALL:
			channel->saved_ier = channel->curr_ier;
			channel->curr_ier = 0;
			break;
		default:
			return -1;
	}

	XGMAC_DMA_IOWRITE(channel, DMA_CH_IER, channel->curr_ier);

	return 0;
}

static int __xgbe_exit(struct xgbe_prv_data *pdata)
{

#if XGBE_SRIOV_PF
	unsigned int count = 2000;

	DBGPR("-->xgbe_exit\n");

	/* Issue a software reset */
	XGBE_USLEEP_RANGE(10, 15);

	/* Poll Until Poll Condition */
	while (--count && XGMAC_IOREAD_BITS(pdata, DMA_MR, SWR))
		XGBE_USLEEP_RANGE(500, 600);
	if (!count) {
		return -EBUSY;
	}

	DBGPR("<--xgbe_exit\n");
#endif

	return 0;
}

static int xgbe_exit(struct xgbe_prv_data *pdata)
{
	int ret;

	/* To guard against possible incorrectly generated interrupts,
	 * issue the software reset twice.
	 */
	ret = __xgbe_exit(pdata);
	if (ret)
		return ret;

	return __xgbe_exit(pdata);
}

#if XGBE_SRIOV_PF
static int xgbe_flush_tx_queues(struct xgbe_prv_data *pdata)
{
	unsigned int i, count;

	if (XGMAC_GET_BITS(pdata->hw_feat.version, MAC_VR, SNPSVER) < 0x21)
		return 0;

	for (i = 0; i < pdata->tx_q_count; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, FTQ, 1);

	/* Poll Until Poll Condition */
	for (i = 0; i < pdata->tx_q_count; i++) {
		count = 2000;
		while (--count && XGMAC_MTL_IOREAD_BITS(pdata, i,
					MTL_Q_TQOMR, FTQ))
			XGBE_USLEEP_RANGE(500, 600);
		if(!count)
			return -EBUSY;
	}

	return 0;
}

#if ELI_ENABLE
static int xgbe_flush_tx_queues_vf(struct xgbe_prv_data *pdata, int vf)
{
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf];
	unsigned int tx_q_start, total_tx_q, i, count;

	tx_q_start = vfinfo->dma_channel_start_num;
	total_tx_q = tx_q_start + (vfinfo->num_dma_channels);

	for (i = tx_q_start; i < total_tx_q; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, FTQ, 1);

	/* Poll Until Poll Condition */
	for (i = tx_q_start; i < total_tx_q; i++) {
		count = 2000;
		while (--count && XGMAC_MTL_IOREAD_BITS(pdata, i,
					MTL_Q_TQOMR, FTQ))
			XGBE_USLEEP_RANGE(500, 600);
		if (!count)
			return -EBUSY;
	}

	return 0;
}
#endif
#endif

#if XGBE_SRIOV_VF
static int xgbe_flush_tx_queues(struct xgbe_prv_data *pdata)
{
        pdata->mbx.xgbe_flush_vf_tx_queues(pdata);
	return 0;
}
#endif

#if XGBE_SRIOV_PF
static void xgbe_config_dma_bus(struct xgbe_prv_data *pdata)
{
	unsigned int sbmr;

	sbmr = XGMAC_IOREAD(pdata, DMA_SBMR);

	/* Set enhanced addressing mode */
	XGMAC_SET_BITS(sbmr, DMA_SBMR, EAME, 1);

	/* Set the System Bus mode */
	XGMAC_SET_BITS(sbmr, DMA_SBMR, UNDEF, 1);
	if(pdata->blen > 128)

		XGMAC_SET_BITS(sbmr, DMA_SBMR, BLEN, pdata->blen >> 5);

	else

		XGMAC_SET_BITS(sbmr, DMA_SBMR, BLEN, 4);
	XGMAC_SET_BITS(sbmr, DMA_SBMR, AAL, pdata->aal);
	XGMAC_SET_BITS(sbmr, DMA_SBMR, RD_OSR_LMT, pdata->rd_osr_limit - 1);
	XGMAC_SET_BITS(sbmr, DMA_SBMR, WR_OSR_LMT, pdata->wr_osr_limit - 1);

	printk("SBMR Programmed is 0x%x\n",sbmr);
	XGMAC_IOWRITE(pdata, DMA_SBMR, sbmr);

	/* Set descriptor fetching threshold */
	if (pdata->vdata->tx_desc_prefetch)
		XGMAC_IOWRITE_BITS(pdata, DMA_TXEDMACR, TDPS,
				pdata->vdata->tx_desc_prefetch);

	if (pdata->vdata->rx_desc_prefetch)
		XGMAC_IOWRITE_BITS(pdata, DMA_RXEDMACR, RDPS,
				pdata->vdata->rx_desc_prefetch);
}

static void xgbe_config_dma_cache(struct xgbe_prv_data *pdata)
{
	XGMAC_IOWRITE(pdata, DMA_AXIARCR, pdata->arcr);
	XGMAC_IOWRITE(pdata, DMA_AXIAWCR, pdata->awcr);
	if (pdata->awarcr)
		XGMAC_IOWRITE(pdata, DMA_AXIAWARCR, pdata->awarcr);
}

static void xgbe_config_mtl_mode(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	/* Set Tx to weighted round robin scheduling algorithm */
	XGMAC_IOWRITE_BITS(pdata, MTL_OMR, ETSALG, MTL_ETSALG_WRR);

	/* Set Tx traffic classes to use WRR algorithm with equal weights */
	for (i = 0; i < pdata->hw_feat.tc_cnt; i++) {
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_ETSCR, TSA,
				MTL_TSA_ETS);
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_QWR, QW, 1);
	}
	/* To Configure remaining TX queues MTL configuration */
	for (; i < (XGBE_MAX_DMA_CHANNELS - pdata->hw_feat.tc_cnt); i++) {
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_ETSCR, TSA,
				MTL_TSA_ETS);
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_QWR, QW, 1);
	}

	/* Set Rx to strict priority algorithm */
	XGMAC_IOWRITE_BITS(pdata, MTL_OMR, RAA, MTL_RAA_SP);
}

static void xgbe_queue_flow_control_threshold(struct xgbe_prv_data *pdata,
		unsigned int queue,
		unsigned int q_fifo_size)
{
	unsigned int frame_fifo_size;
	unsigned int rfa, rfd;

	frame_fifo_size = XGMAC_FLOW_CONTROL_ALIGN(xgbe_get_max_frame(pdata));

	if (pdata->pfcq[queue] && (q_fifo_size > pdata->pfc_rfa)) {
		/* PFC is active for this queue */
		rfa = pdata->pfc_rfa;
		rfd = rfa + frame_fifo_size;
		if (rfd > XGMAC_FLOW_CONTROL_MAX)
			rfd = XGMAC_FLOW_CONTROL_MAX;
		if (rfa >= XGMAC_FLOW_CONTROL_MAX)
			rfa = XGMAC_FLOW_CONTROL_MAX - XGMAC_FLOW_CONTROL_UNIT;
	} else {
		/* This path deals with just maximum frame sizes which are
		 * limited to a jumbo frame of 9,000 (plus headers, etc.)
		 * so we can never exceed the maximum allowable RFA/RFD
		 * values.
		 */
		if (q_fifo_size <= 2048) {
			/* rx_rfd to zero to signal no flow control */
			pdata->rx_rfa[queue] = 0;
			pdata->rx_rfd[queue] = 0;
			return;
		}

		if (q_fifo_size <= 4096) {
			/* Between 2048 and 4096 */
			pdata->rx_rfa[queue] = 0;	/* Full - 1024 bytes */
			pdata->rx_rfd[queue] = 1;	/* Full - 1536 bytes */
			return;
		}

		if (q_fifo_size <= frame_fifo_size) {
			/* Between 4096 and max-frame */
			pdata->rx_rfa[queue] = 2;	/* Full - 2048 bytes */
			pdata->rx_rfd[queue] = 5;	/* Full - 3584 bytes */
			return;
		}

		if (q_fifo_size <= (frame_fifo_size * 3)) {
			/* Between max-frame and 3 max-frames,
			 * trigger if we get just over a frame of data and
			 * resume when we have just under half a frame left.
			 */
			rfa = q_fifo_size - frame_fifo_size;
			rfd = rfa + (frame_fifo_size / 2);
		} else {
			/* Above 3 max-frames - trigger when just over
			 * 2 frames of space available
			 */
			rfa = frame_fifo_size * 2;
			rfa += XGMAC_FLOW_CONTROL_UNIT;
			rfd = rfa + frame_fifo_size;
		}
	}

	pdata->rx_rfa[queue] = XGMAC_FLOW_CONTROL_VALUE(rfa);
	pdata->rx_rfd[queue] = XGMAC_FLOW_CONTROL_VALUE(rfd);
}

static void xgbe_calculate_flow_control_threshold(struct xgbe_prv_data *pdata,
		unsigned int *fifo)
{
	unsigned int q_fifo_size;
	unsigned int i;

	for (i = 0; i < pdata->rx_q_count; i++) {
		q_fifo_size = (fifo[i] + 1) * XGMAC_FIFO_UNIT;

		xgbe_queue_flow_control_threshold(pdata, i, q_fifo_size);
	}
}

static void xgbe_config_flow_control_threshold(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < pdata->rx_q_count; i++) {
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQFCR, RFA,
				pdata->rx_rfa[i]);
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQFCR, RFD,
				pdata->rx_rfd[i]);
	}
}

static unsigned int xgbe_get_tx_fifo_size(struct xgbe_prv_data *pdata)
{
	/* The configured value may not be the actual amount of fifo RAM */
	return min_t(unsigned int, pdata->tx_max_fifo_size,
			pdata->hw_feat.tx_fifo_size);
}

static unsigned int xgbe_get_rx_fifo_size(struct xgbe_prv_data *pdata)
{
	/* The configured value may not be the actual amount of fifo RAM */
	return min_t(unsigned int, pdata->rx_max_fifo_size,
			pdata->hw_feat.rx_fifo_size);
}

static void xgbe_calculate_equal_fifo(unsigned int fifo_size,
		unsigned int queue_count,
		unsigned int *fifo)
{
	unsigned int q_fifo_size;
	unsigned int p_fifo;
	unsigned int i;

	q_fifo_size = fifo_size / queue_count;

	/* Calculate the fifo setting by dividing the queue's fifo size
	 * by the fifo allocation increment (with 0 representing the
	 * base allocation increment so decrement the result by 1).
	 */
	p_fifo = q_fifo_size / XGMAC_FIFO_UNIT;
	if (p_fifo)
		p_fifo--;

	/* Distribute the fifo equally amongst the queues */
	for (i = 0; i < queue_count; i++)
		fifo[i] = p_fifo;
}

static unsigned int xgbe_set_nonprio_fifos(unsigned int fifo_size,
		unsigned int queue_count,
		unsigned int *fifo)
{
	unsigned int i;

	BUILD_BUG_ON_NOT_POWER_OF_2(XGMAC_FIFO_MIN_ALLOC);

	if (queue_count <= IEEE_8021QAZ_MAX_TCS)
		return fifo_size;

	/* Rx queues 9 and up are for specialized packets,
	 * such as PTP or DCB control packets, etc. and
	 * don't require a large fifo
	 */
	for (i = IEEE_8021QAZ_MAX_TCS; i < queue_count; i++) {
        if ( (i == 10) || (i == 11)) {
            fifo[i] = (XGMAC_FIFO_MIN_ALLOC * 3/ XGMAC_FIFO_UNIT) - 1;
            fifo_size -= XGMAC_FIFO_MIN_ALLOC * 3;
        }
        else {
            fifo[i] = 0;
		}
	}

	return fifo_size;
}

static unsigned int xgbe_get_pfc_delay(struct xgbe_prv_data *pdata)
{
	unsigned int delay;

	/* If a delay has been provided, use that */
	if (pdata->pfc->delay)
		return pdata->pfc->delay / 8;

	/* Allow for two maximum size frames */
	delay = xgbe_get_max_frame(pdata);
	delay += XGMAC_ETH_PREAMBLE;
	delay *= 2;

	/* Allow for PFC frame */
	delay += XGMAC_PFC_DATA_LEN;
	delay += ETH_HLEN + ETH_FCS_LEN;
	delay += XGMAC_ETH_PREAMBLE;

	/* Allow for miscellaneous delays (LPI exit, cable, etc.) */
	delay += XGMAC_PFC_DELAYS;

	return delay;
}

static unsigned int xgbe_get_pfc_queues(struct xgbe_prv_data *pdata)
{
	unsigned int count, prio_queues;
	unsigned int i;

	if (!pdata->pfc->pfc_en)
		return 0;

	count = 0;
	prio_queues = XGMAC_PRIO_QUEUES(pdata->rx_q_count);
	for (i = 0; i < prio_queues; i++) {
		if (!xgbe_is_pfc_queue(pdata, i))
			continue;

		pdata->pfcq[i] = 1;
		count++;
	}

	return count;
}

static void xgbe_calculate_dcb_fifo(struct xgbe_prv_data *pdata,
		unsigned int fifo_size,
		unsigned int *fifo)
{
	unsigned int q_fifo_size, rem_fifo, addn_fifo;
	unsigned int prio_queues;
	unsigned int pfc_count;
	unsigned int i;

	q_fifo_size = XGMAC_FIFO_ALIGN(xgbe_get_max_frame(pdata));
	prio_queues = XGMAC_PRIO_QUEUES(pdata->rx_q_count);
	pfc_count = xgbe_get_pfc_queues(pdata);

	if (!pfc_count || ((q_fifo_size * prio_queues) > fifo_size)) {
		/* No traffic classes with PFC enabled or can't do lossless */
		xgbe_calculate_equal_fifo(fifo_size, prio_queues, fifo);
		return;
	}

	/* Calculate how much fifo we have to play with */
	rem_fifo = fifo_size - (q_fifo_size * prio_queues);

	/* Calculate how much more than base fifo PFC needs, which also
	 * becomes the threshold activation point (RFA)
	 */
	pdata->pfc_rfa = xgbe_get_pfc_delay(pdata);
	pdata->pfc_rfa = XGMAC_FLOW_CONTROL_ALIGN(pdata->pfc_rfa);

	if (pdata->pfc_rfa > q_fifo_size) {
		addn_fifo = pdata->pfc_rfa - q_fifo_size;
		addn_fifo = XGMAC_FIFO_ALIGN(addn_fifo);
	} else {
		addn_fifo = 0;
	}

	/* Calculate DCB fifo settings:
	 *   - distribute remaining fifo between the VLAN priority
	 *     queues based on traffic class PFC enablement and overall
	 *     priority (0 is lowest priority, so start at highest)
	 */
	i = prio_queues;
	while (i > 0) {
		i--;

		fifo[i] = (q_fifo_size / XGMAC_FIFO_UNIT) - 1;

		if (!pdata->pfcq[i] || !addn_fifo)
			continue;

		if (addn_fifo > rem_fifo) {
			netdev_warn(pdata->netdev,
					"RXq%u cannot set needed fifo size\n", i);
			if (!rem_fifo)
				continue;

			addn_fifo = rem_fifo;
		}

		fifo[i] += (addn_fifo / XGMAC_FIFO_UNIT);
		rem_fifo -= addn_fifo;
	}

	if (rem_fifo) {
		unsigned int inc_fifo = rem_fifo / prio_queues;

		/* Distribute remaining fifo across queues */
		for (i = 0; i < prio_queues; i++)
			fifo[i] += (inc_fifo / XGMAC_FIFO_UNIT);
	}
}

static void xgbe_config_tx_fifo_size(struct xgbe_prv_data *pdata)
{
	unsigned int fifo_size;
	unsigned int fifo[XGBE_MAX_QUEUES];
	unsigned int i;

	fifo_size = xgbe_get_tx_fifo_size(pdata);

	xgbe_calculate_equal_fifo(fifo_size, XGBE_MAX_QUEUES, fifo);

	for (i = 0; i < XGBE_MAX_QUEUES; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TQS, fifo[i]);

	netif_info(pdata, drv, pdata->netdev,
			"%d Tx hardware queues, %d byte fifo per queue\n",
			pdata->tx_q_count, ((fifo[0] + 1) * XGMAC_FIFO_UNIT));
}

static void xgbe_config_rx_fifo_size(struct xgbe_prv_data *pdata)
{
	unsigned int fifo_size;
	unsigned int fifo[XGBE_MAX_QUEUES];
	unsigned int prio_queues;
	unsigned int i;

	/* Clear any DCB related fifo/queue information */
	memset(pdata->pfcq, 0, sizeof(pdata->pfcq));
	pdata->pfc_rfa = 0;

	fifo_size = xgbe_get_rx_fifo_size(pdata);
	prio_queues = XGMAC_PRIO_QUEUES(pdata->rx_q_count);

	/* Assign a minimum fifo to the non-VLAN priority queues */
	fifo_size = xgbe_set_nonprio_fifos(fifo_size, pdata->rx_q_count, fifo);

	if (pdata->pfc && pdata->ets)
		xgbe_calculate_dcb_fifo(pdata, fifo_size, fifo);
	else
		xgbe_calculate_equal_fifo(fifo_size, prio_queues, fifo);

	if(pdata->dev_id == BCM8989X_PF_ID) {
		for (i = 0; i < pdata->rx_q_count; i++) {
			XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, RQS, pdata->rq_size[i]);
    	}
    } else {
		for (i = 0; i < pdata->rx_q_count; i++) {
    		if (BCM8956X_A0_PF_ID == pdata->dev_id) {
				XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, RQS, 0x3F);
			} else {
				XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_RQOMR, RQS, fifo[i]);
			}
    	}
		xgbe_calculate_flow_control_threshold(pdata, fifo);
	}
	xgbe_config_flow_control_threshold(pdata);

	if (pdata->pfc && pdata->ets && pdata->pfc->pfc_en) {
		netif_info(pdata, drv, pdata->netdev,
				"%u Rx hardware queues\n", pdata->rx_q_count);
		for (i = 0; i < pdata->rx_q_count; i++)
			netif_info(pdata, drv, pdata->netdev,
					"RxQ%u, %u byte fifo queue\n", i,
					((fifo[i] + 1) * XGMAC_FIFO_UNIT));
	} else {
		netif_info(pdata, drv, pdata->netdev,
				"%u Rx hardware queues, %u byte fifo per queue\n",
				pdata->rx_q_count,
				((fifo[0] + 1) * XGMAC_FIFO_UNIT));
	}
}

static void xgbe_config_queue_mapping(struct xgbe_prv_data *pdata)
{
	unsigned int qptc, qptc_extra, queue;
	unsigned int prio_queues;
	unsigned int ppq, ppq_extra, prio;
	unsigned int mask;
	unsigned int i, j, reg, reg_val;

	/* Map the MTL Tx Queues to Traffic Classes
	 *   Note: Tx Queues >= Traffic Classes
	 */
	qptc = pdata->tx_q_count / pdata->hw_feat.tc_cnt;
	qptc_extra = pdata->tx_q_count % pdata->hw_feat.tc_cnt;

	if (!(pdata->flags & XGBE_FLAG_SRIOV_ENABLED)) {
		for (i = 0, queue = 0; i < pdata->hw_feat.tc_cnt; i++) {
			for (j = 0; j < qptc; j++) {
				netif_dbg(pdata, drv, pdata->netdev,
						"TXq%u mapped to TC%u\n", queue, i);
				XGMAC_MTL_IOWRITE_BITS(pdata, queue, MTL_Q_TQOMR,
						Q2TCMAP, i);
				pdata->q2tc_map[queue++] = i;
			}

			if (i < qptc_extra) {
				netif_dbg(pdata, drv, pdata->netdev,
						"TXq%u mapped to TC%u\n", queue, i);
				XGMAC_MTL_IOWRITE_BITS(pdata, queue, MTL_Q_TQOMR,
						Q2TCMAP, i);
				pdata->q2tc_map[queue++] = i;
			}
		}
	} else {
		TEST_PRNT("for SRIOV queue to tc map\n");
		for (queue = 0; queue < XGBE_MAX_QUEUES;) {
			for (i = 0; i < pdata->hw_feat.tc_cnt; i++) {
				XGMAC_MTL_IOWRITE_BITS(pdata, queue, MTL_Q_TQOMR,
						Q2TCMAP, i);
				pdata->q2tc_map[queue++] = i;
			}
		}
		for (i = 0; i < 16; i++)
			TEST_PRNT("MTL_TxQ_%d = %x\n", i, XGMAC_MTL_IOREAD(pdata, i, MTL_Q_TQOMR));
	}
	/* Map the 8 VLAN priority values to available MTL Rx queues */
	prio_queues = XGMAC_PRIO_QUEUES(pdata->rx_q_count);
	ppq = IEEE_8021QAZ_MAX_TCS / prio_queues;
	ppq_extra = IEEE_8021QAZ_MAX_TCS % prio_queues;

	reg = MAC_RQC2R;
	reg_val = 0;
	for (i = 0, prio = 0; i < prio_queues;) {
		mask = 0;
		for (j = 0; j < ppq; j++) {
			netif_dbg(pdata, drv, pdata->netdev,
					"PRIO%u mapped to RXq%u\n", prio, i);
			TEST_PRNT("PRIO%u mapped to RXq%u\n", prio, i);
			mask |= (1 << prio);
			pdata->prio2q_map[prio++] = i;
		}

		if (i < ppq_extra) {
			netif_dbg(pdata, drv, pdata->netdev,
					"PRIO%u mapped to RXq%u\n", prio, i);
			TEST_PRNT("PRIO%u mapped to RXq%u\n", prio, i);
			mask |= (1 << prio);
			pdata->prio2q_map[prio++] = i;
		}

		reg_val |= (mask << ((i++ % MAC_RQC2_Q_PER_REG) << 3));

		if ((i % MAC_RQC2_Q_PER_REG) && (i != prio_queues))
			continue;

		XGMAC_IOWRITE(pdata, reg, reg_val);
		reg += MAC_RQC2_INC;
		reg_val = 0;
	}

	/* Select dynamic mapping of MTL Rx queue to DMA Rx channel */
	reg = MTL_RQDCM0R;
	reg_val = 0;
	for (i = 0; i < pdata->rx_q_count;) {
		reg_val |= (0x80 << ((i++ % MTL_RQDCM_Q_PER_REG) << 3));

		if ((i % MTL_RQDCM_Q_PER_REG) && (i != pdata->rx_q_count))
			continue;

		XGMAC_IOWRITE(pdata, reg, reg_val);

		reg += MTL_RQDCM_INC;
		reg_val = 0;
	}
}

static void xgbe_config_tc(struct xgbe_prv_data *pdata)
{
	unsigned int offset, queue, prio;
	u8 i;

	netdev_reset_tc(pdata->netdev);
	if (!pdata->num_tcs)
		return;

	netdev_set_num_tc(pdata->netdev, pdata->num_tcs);

	for (i = 0, queue = 0, offset = 0; i < pdata->num_tcs; i++) {
		while ((queue < pdata->tx_q_count) &&
				(pdata->q2tc_map[queue] == i))
			queue++;

		netif_dbg(pdata, drv, pdata->netdev, "TC%u using TXq%u-%u\n",
				i, offset, queue - 1);
		netdev_set_tc_queue(pdata->netdev, i, queue - offset, offset);
		offset = queue;
	}

	if (!pdata->ets)
		return;

	for (prio = 0; prio < IEEE_8021QAZ_MAX_TCS; prio++)
		netdev_set_prio_tc_map(pdata->netdev, prio,
				pdata->ets->prio_tc[prio]);
}

static void xgbe_config_dcb_tc(struct xgbe_prv_data *pdata)
{
	struct ieee_ets *ets = pdata->ets;
	unsigned int total_weight, min_weight, weight;
	unsigned int mask, reg, reg_val;
	unsigned int i, prio;

	if (!ets)
		return;

	/* Set Tx to deficit weighted round robin scheduling algorithm (when
	 * traffic class is using ETS algorithm)
	 */
	XGMAC_IOWRITE_BITS(pdata, MTL_OMR, ETSALG, MTL_ETSALG_DWRR);

	/* Set Traffic Class algorithms */
	total_weight = pdata->netdev->mtu * pdata->hw_feat.tc_cnt;
	min_weight = total_weight / 100;
	if (!min_weight)
		min_weight = 1;

	for (i = 0; i < pdata->hw_feat.tc_cnt; i++) {
		/* Map the priorities to the traffic class */
		mask = 0;
		for (prio = 0; prio < IEEE_8021QAZ_MAX_TCS; prio++) {
			if (ets->prio_tc[prio] == i)
				mask |= (1 << prio);
		}
		mask &= 0xff;

		netif_dbg(pdata, drv, pdata->netdev, "TC%u PRIO mask=%#x\n",
				i, mask);
		reg = MTL_TCPM0R + (MTL_TCPM_INC * (i / MTL_TCPM_TC_PER_REG));
		reg_val = XGMAC_IOREAD(pdata, reg);

		reg_val &= ~(0xff << ((i % MTL_TCPM_TC_PER_REG) << 3));
		reg_val |= (mask << ((i % MTL_TCPM_TC_PER_REG) << 3));

		XGMAC_IOWRITE(pdata, reg, reg_val);

		/* Set the traffic class algorithm */
		switch (ets->tc_tsa[i]) {
			case IEEE_8021QAZ_TSA_STRICT:
				netif_dbg(pdata, drv, pdata->netdev,
						"TC%u using SP\n", i);
				XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_ETSCR, TSA,
						MTL_TSA_SP);
				break;
			case IEEE_8021QAZ_TSA_ETS:
				weight = total_weight * ets->tc_tx_bw[i] / 100;
				weight = clamp(weight, min_weight, total_weight);

				netif_dbg(pdata, drv, pdata->netdev,
						"TC%u using DWRR (weight %u)\n", i, weight);
				XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_ETSCR, TSA,
						MTL_TSA_ETS);
				XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_TC_QWR, QW,
						weight);
				break;
		}
	}

	xgbe_config_tc(pdata);
}

static void xgbe_config_dcb_pfc(struct xgbe_prv_data *pdata)
{
	if (!test_bit(XGBE_DOWN, &pdata->dev_state)) {
		/* Just stop the Tx queues while Rx fifo is changed */
		netif_tx_stop_all_queues(pdata->netdev);

		/* Suspend Rx so that fifo's can be adjusted */
		pdata->hw_if.disable_rx(pdata);
	}

	xgbe_config_rx_fifo_size(pdata);
	xgbe_config_flow_control(pdata);

	if (!test_bit(XGBE_DOWN, &pdata->dev_state)) {
		/* Resume Rx */
		pdata->hw_if.enable_rx(pdata);

		/* Resume Tx queues */
		netif_tx_start_all_queues(pdata->netdev);
	}
}
#endif

#if XGBE_SRIOV_PF
static void xgbe_enable_jumbo_frame(struct xgbe_prv_data *pdata, unsigned int func, unsigned int enable)
{
       unsigned int temp = pdata->jumbo_frame_enable_cache;

       //First Clear the Bit, if enable then set the bit
       temp &= (unsigned int)(~(1 << func));
       if(enable)
               temp |= (1 << func);

       if((temp != 0) && (pdata->jumbo_frame_enable_cache == 0)) {
               //Enable Jumbo frames, as it is not enabled
               XGMAC_IOWRITE_BITS(pdata, MAC_RCR, JE, 1);
       }

       if((pdata->jumbo_frame_enable_cache != 0) && (temp == 0)) {
               //Disable Jumbo frames, as it is enabled
               XGMAC_IOWRITE_BITS(pdata, MAC_RCR, JE, 0);
       }
       pdata->jumbo_frame_enable_cache = temp;
}

static void xgbe_config_jumbo_enable(struct xgbe_prv_data *pdata)
{
	unsigned int val;

	val = (pdata->netdev->mtu > XGMAC_STD_PACKET_MTU) ? 1 : 0;
    //enable for PF Function
    xgbe_enable_jumbo_frame(pdata, 0, val);
}

#if ELI_ENABLE
static void xgbe_config_mac_address(struct xgbe_prv_data *pdata)
{
	xgbe_set_mac_address(pdata, pdata->netdev->dev_addr);


	/* Filtering is done using perfect filtering*/
	if (pdata->hw_feat.hash_table_size) {
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, HPF, 1);
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, HUC, 1);
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, HMC, 1);
	}
}

static void xgbe_config_checksum_offload(struct xgbe_prv_data *pdata)
{
	if (pdata->netdev->features & NETIF_F_RXCSUM)
		xgbe_enable_rx_csum(pdata);
	else
		xgbe_disable_rx_csum(pdata);
}

#else
static void xgbe_config_mac_address(struct xgbe_prv_data *pdata)
{
	xgbe_set_mac_address(pdata, (u8 *)pdata->netdev->dev_addr);


	/* Filtering is done using perfect filtering and hash filtering */
	if (pdata->hw_feat.hash_table_size) {
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, HPF, 1);
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, HUC, 1);
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, HMC, 1);
	}
}

static void xgbe_config_checksum_offload(struct xgbe_prv_data *pdata)
{
	if (pdata->netdev->features & NETIF_F_RXCSUM){
		xgbe_enable_rx_csum(pdata);
		pdata->rx_csum_semaphore++;
	}else{
		pdata->rx_csum_semaphore--;
		xgbe_disable_rx_csum(pdata);
	}
}
#endif

static void xgbe_config_mac_speed(struct xgbe_prv_data *pdata)
{
	xgbe_set_speed(pdata, pdata->phy_speed);
}
#endif

#if XGBE_SRIOV_VF
static void xgbe_config_mac_address(struct xgbe_prv_data *pdata)
{
	xgbe_set_mac_address(pdata, (u8 *)pdata->netdev->dev_addr);
}

static void xgbe_config_jumbo_enable(struct xgbe_prv_data *pdata)
{
	unsigned int val;

	val = (pdata->netdev->mtu > XGMAC_STD_PACKET_MTU) ? 1 : 0;

	/* XGMAC_IOWRITE_BITS(pdata, MAC_RCR, JE, val);*/
	if (val)
		pdata->mbx.enable_disable_jumbo_frame(pdata, ENABLE);
	else
		pdata->mbx.enable_disable_jumbo_frame(pdata, DISABLE);
}

static void xgbe_config_checksum_offload(struct xgbe_prv_data *pdata)
{
	if (pdata->netdev->features & NETIF_F_RXCSUM)
		xgbe_enable_rx_csum(pdata);
	else
		xgbe_disable_rx_csum(pdata);
}

#endif

static void xgbe_config_vlan_support(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	/* Indicate that VLAN Tx CTAGs come from context descriptors */
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANIR, CSVL, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANIR, VLTI, 1);

	/* Set the current VLAN Hash Table register value */
	xgbe_update_vlan_hash_table(pdata);
#endif

	if (pdata->netdev->features & NETIF_F_HW_VLAN_CTAG_FILTER)
		xgbe_enable_rx_vlan_filtering(pdata);
	else
		xgbe_disable_rx_vlan_filtering(pdata);

	if (pdata->netdev->features & NETIF_F_HW_VLAN_CTAG_RX)
		xgbe_enable_rx_vlan_stripping(pdata);
	else
		xgbe_disable_rx_vlan_stripping(pdata);
}

#if XGBE_SRIOV_PF
static u64 xgbe_mmc_read(struct xgbe_prv_data *pdata, unsigned int reg_lo)
{
	bool read_hi;
	u64 val;

	if (pdata->vdata->mmc_64bit) {
		switch (reg_lo) {
			/* These registers are always 32 bit */
			case MMC_RXRUNTERROR:
			case MMC_RXJABBERERROR:
			case MMC_RXUNDERSIZE_G:
			case MMC_RXOVERSIZE_G:
			case MMC_RXWATCHDOGERROR:
				read_hi = false;
				break;

			default:
				read_hi = true;
		}
	} else {
		switch (reg_lo) {
			/* These registers are always 64 bit */
			case MMC_TXOCTETCOUNT_GB_LO:
			case MMC_TXOCTETCOUNT_G_LO:
			case MMC_RXOCTETCOUNT_GB_LO:
			case MMC_RXOCTETCOUNT_G_LO:
				read_hi = true;
				break;

			default:
				read_hi = false;
		}
	}

	val = XGMAC_IOREAD(pdata, reg_lo);

	if (read_hi)
		val |= ((u64)XGMAC_IOREAD(pdata, reg_lo + 4) << 32);

	return val;
}

static void xgbe_tx_mmc_int(struct xgbe_prv_data *pdata)
{
	struct xgbe_mmc_stats *stats = &pdata->mmc_stats;
	unsigned int mmc_isr = XGMAC_IOREAD(pdata, MMC_TISR);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXOCTETCOUNT_GB))
		stats->txoctetcount_gb +=
			xgbe_mmc_read(pdata, MMC_TXOCTETCOUNT_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXFRAMECOUNT_GB))
		stats->txframecount_gb +=
			xgbe_mmc_read(pdata, MMC_TXFRAMECOUNT_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXBROADCASTFRAMES_G))
		stats->txbroadcastframes_g +=
			xgbe_mmc_read(pdata, MMC_TXBROADCASTFRAMES_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXMULTICASTFRAMES_G))
		stats->txmulticastframes_g +=
			xgbe_mmc_read(pdata, MMC_TXMULTICASTFRAMES_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TX64OCTETS_GB))
		stats->tx64octets_gb +=
			xgbe_mmc_read(pdata, MMC_TX64OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TX65TO127OCTETS_GB))
		stats->tx65to127octets_gb +=
			xgbe_mmc_read(pdata, MMC_TX65TO127OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TX128TO255OCTETS_GB))
		stats->tx128to255octets_gb +=
			xgbe_mmc_read(pdata, MMC_TX128TO255OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TX256TO511OCTETS_GB))
		stats->tx256to511octets_gb +=
			xgbe_mmc_read(pdata, MMC_TX256TO511OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TX512TO1023OCTETS_GB))
		stats->tx512to1023octets_gb +=
			xgbe_mmc_read(pdata, MMC_TX512TO1023OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TX1024TOMAXOCTETS_GB))
		stats->tx1024tomaxoctets_gb +=
			xgbe_mmc_read(pdata, MMC_TX1024TOMAXOCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXUNICASTFRAMES_GB))
		stats->txunicastframes_gb +=
			xgbe_mmc_read(pdata, MMC_TXUNICASTFRAMES_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXMULTICASTFRAMES_GB))
		stats->txmulticastframes_gb +=
			xgbe_mmc_read(pdata, MMC_TXMULTICASTFRAMES_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXBROADCASTFRAMES_GB))
		stats->txbroadcastframes_g +=
			xgbe_mmc_read(pdata, MMC_TXBROADCASTFRAMES_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXUNDERFLOWERROR))
		stats->txunderflowerror +=
			xgbe_mmc_read(pdata, MMC_TXUNDERFLOWERROR_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXOCTETCOUNT_G))
		stats->txoctetcount_g +=
			xgbe_mmc_read(pdata, MMC_TXOCTETCOUNT_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXFRAMECOUNT_G))
		stats->txframecount_g +=
			xgbe_mmc_read(pdata, MMC_TXFRAMECOUNT_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXPAUSEFRAMES))
		stats->txpauseframes +=
			xgbe_mmc_read(pdata, MMC_TXPAUSEFRAMES_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_TISR, TXVLANFRAMES_G))
		stats->txvlanframes_g +=
			xgbe_mmc_read(pdata, MMC_TXVLANFRAMES_G_LO);
}

static void xgbe_rx_mmc_int(struct xgbe_prv_data *pdata)
{
	struct xgbe_mmc_stats *stats = &pdata->mmc_stats;
	unsigned int mmc_isr = XGMAC_IOREAD(pdata, MMC_RISR);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXFRAMECOUNT_GB))
		stats->rxframecount_gb +=
			xgbe_mmc_read(pdata, MMC_RXFRAMECOUNT_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXOCTETCOUNT_GB))
		stats->rxoctetcount_gb +=
			xgbe_mmc_read(pdata, MMC_RXOCTETCOUNT_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXOCTETCOUNT_G))
		stats->rxoctetcount_g +=
			xgbe_mmc_read(pdata, MMC_RXOCTETCOUNT_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXBROADCASTFRAMES_G))
		stats->rxbroadcastframes_g +=
			xgbe_mmc_read(pdata, MMC_RXBROADCASTFRAMES_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXMULTICASTFRAMES_G))
		stats->rxmulticastframes_g +=
			xgbe_mmc_read(pdata, MMC_RXMULTICASTFRAMES_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXCRCERROR))
		stats->rxcrcerror +=
			xgbe_mmc_read(pdata, MMC_RXCRCERROR_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXRUNTERROR))
		stats->rxrunterror +=
			xgbe_mmc_read(pdata, MMC_RXRUNTERROR);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXJABBERERROR))
		stats->rxjabbererror +=
			xgbe_mmc_read(pdata, MMC_RXJABBERERROR);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXUNDERSIZE_G))
		stats->rxundersize_g +=
			xgbe_mmc_read(pdata, MMC_RXUNDERSIZE_G);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXOVERSIZE_G))
		stats->rxoversize_g +=
			xgbe_mmc_read(pdata, MMC_RXOVERSIZE_G);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RX64OCTETS_GB))
		stats->rx64octets_gb +=
			xgbe_mmc_read(pdata, MMC_RX64OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RX65TO127OCTETS_GB))
		stats->rx65to127octets_gb +=
			xgbe_mmc_read(pdata, MMC_RX65TO127OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RX128TO255OCTETS_GB))
		stats->rx128to255octets_gb +=
			xgbe_mmc_read(pdata, MMC_RX128TO255OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RX256TO511OCTETS_GB))
		stats->rx256to511octets_gb +=
			xgbe_mmc_read(pdata, MMC_RX256TO511OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RX512TO1023OCTETS_GB))
		stats->rx512to1023octets_gb +=
			xgbe_mmc_read(pdata, MMC_RX512TO1023OCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RX1024TOMAXOCTETS_GB))
		stats->rx1024tomaxoctets_gb +=
			xgbe_mmc_read(pdata, MMC_RX1024TOMAXOCTETS_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXUNICASTFRAMES_G))
		stats->rxunicastframes_g +=
			xgbe_mmc_read(pdata, MMC_RXUNICASTFRAMES_G_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXLENGTHERROR))
		stats->rxlengtherror +=
			xgbe_mmc_read(pdata, MMC_RXLENGTHERROR_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXOUTOFRANGETYPE))
		stats->rxoutofrangetype +=
			xgbe_mmc_read(pdata, MMC_RXOUTOFRANGETYPE_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXPAUSEFRAMES))
		stats->rxpauseframes +=
			xgbe_mmc_read(pdata, MMC_RXPAUSEFRAMES_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXFIFOOVERFLOW))
		stats->rxfifooverflow +=
			xgbe_mmc_read(pdata, MMC_RXFIFOOVERFLOW_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXVLANFRAMES_GB))
		stats->rxvlanframes_gb +=
			xgbe_mmc_read(pdata, MMC_RXVLANFRAMES_GB_LO);

	if (XGMAC_GET_BITS(mmc_isr, MMC_RISR, RXWATCHDOGERROR))
		stats->rxwatchdogerror +=
			xgbe_mmc_read(pdata, MMC_RXWATCHDOGERROR);
}

static void xgbe_read_mmc_stats(struct xgbe_prv_data *pdata)
{
	struct xgbe_mmc_stats *stats = &pdata->mmc_stats;

	/* Freeze counters */
	XGMAC_IOWRITE_BITS(pdata, MMC_CR, MCF, 1);

	stats->txoctetcount_gb +=
		xgbe_mmc_read(pdata, MMC_TXOCTETCOUNT_GB_LO);

	stats->txframecount_gb +=
		xgbe_mmc_read(pdata, MMC_TXFRAMECOUNT_GB_LO);

	stats->txbroadcastframes_g +=
		xgbe_mmc_read(pdata, MMC_TXBROADCASTFRAMES_G_LO);

	stats->txmulticastframes_g +=
		xgbe_mmc_read(pdata, MMC_TXMULTICASTFRAMES_G_LO);

	stats->tx64octets_gb +=
		xgbe_mmc_read(pdata, MMC_TX64OCTETS_GB_LO);

	stats->tx65to127octets_gb +=
		xgbe_mmc_read(pdata, MMC_TX65TO127OCTETS_GB_LO);

	stats->tx128to255octets_gb +=
		xgbe_mmc_read(pdata, MMC_TX128TO255OCTETS_GB_LO);

	stats->tx256to511octets_gb +=
		xgbe_mmc_read(pdata, MMC_TX256TO511OCTETS_GB_LO);

	stats->tx512to1023octets_gb +=
		xgbe_mmc_read(pdata, MMC_TX512TO1023OCTETS_GB_LO);

	stats->tx1024tomaxoctets_gb +=
		xgbe_mmc_read(pdata, MMC_TX1024TOMAXOCTETS_GB_LO);

	stats->txunicastframes_gb +=
		xgbe_mmc_read(pdata, MMC_TXUNICASTFRAMES_GB_LO);

	stats->txmulticastframes_gb +=
		xgbe_mmc_read(pdata, MMC_TXMULTICASTFRAMES_GB_LO);

	stats->txbroadcastframes_g +=
		xgbe_mmc_read(pdata, MMC_TXBROADCASTFRAMES_GB_LO);

	stats->txunderflowerror +=
		xgbe_mmc_read(pdata, MMC_TXUNDERFLOWERROR_LO);

	stats->txoctetcount_g +=
		xgbe_mmc_read(pdata, MMC_TXOCTETCOUNT_G_LO);

	stats->txframecount_g +=
		xgbe_mmc_read(pdata, MMC_TXFRAMECOUNT_G_LO);

	stats->txpauseframes +=
		xgbe_mmc_read(pdata, MMC_TXPAUSEFRAMES_LO);

	stats->txvlanframes_g +=
		xgbe_mmc_read(pdata, MMC_TXVLANFRAMES_G_LO);

	stats->rxframecount_gb +=
		xgbe_mmc_read(pdata, MMC_RXFRAMECOUNT_GB_LO);

	stats->rxoctetcount_gb +=
		xgbe_mmc_read(pdata, MMC_RXOCTETCOUNT_GB_LO);

	stats->rxoctetcount_g +=
		xgbe_mmc_read(pdata, MMC_RXOCTETCOUNT_G_LO);

	stats->rxbroadcastframes_g +=
		xgbe_mmc_read(pdata, MMC_RXBROADCASTFRAMES_G_LO);

	stats->rxmulticastframes_g +=
		xgbe_mmc_read(pdata, MMC_RXMULTICASTFRAMES_G_LO);

	stats->rxcrcerror +=
		xgbe_mmc_read(pdata, MMC_RXCRCERROR_LO);

	stats->rxrunterror +=
		xgbe_mmc_read(pdata, MMC_RXRUNTERROR);

	stats->rxjabbererror +=
		xgbe_mmc_read(pdata, MMC_RXJABBERERROR);

	stats->rxundersize_g +=
		xgbe_mmc_read(pdata, MMC_RXUNDERSIZE_G);

	stats->rxoversize_g +=
		xgbe_mmc_read(pdata, MMC_RXOVERSIZE_G);

	stats->rx64octets_gb +=
		xgbe_mmc_read(pdata, MMC_RX64OCTETS_GB_LO);

	stats->rx65to127octets_gb +=
		xgbe_mmc_read(pdata, MMC_RX65TO127OCTETS_GB_LO);

	stats->rx128to255octets_gb +=
		xgbe_mmc_read(pdata, MMC_RX128TO255OCTETS_GB_LO);

	stats->rx256to511octets_gb +=
		xgbe_mmc_read(pdata, MMC_RX256TO511OCTETS_GB_LO);

	stats->rx512to1023octets_gb +=
		xgbe_mmc_read(pdata, MMC_RX512TO1023OCTETS_GB_LO);

	stats->rx1024tomaxoctets_gb +=
		xgbe_mmc_read(pdata, MMC_RX1024TOMAXOCTETS_GB_LO);

	stats->rxunicastframes_g +=
		xgbe_mmc_read(pdata, MMC_RXUNICASTFRAMES_G_LO);

	stats->rxlengtherror +=
		xgbe_mmc_read(pdata, MMC_RXLENGTHERROR_LO);

	stats->rxoutofrangetype +=
		xgbe_mmc_read(pdata, MMC_RXOUTOFRANGETYPE_LO);

	stats->rxpauseframes +=
		xgbe_mmc_read(pdata, MMC_RXPAUSEFRAMES_LO);

	stats->rxfifooverflow +=
		xgbe_mmc_read(pdata, MMC_RXFIFOOVERFLOW_LO);

	stats->rxvlanframes_gb +=
		xgbe_mmc_read(pdata, MMC_RXVLANFRAMES_GB_LO);

	stats->rxwatchdogerror +=
		xgbe_mmc_read(pdata, MMC_RXWATCHDOGERROR);

	/* Un-freeze counters */
	XGMAC_IOWRITE_BITS(pdata, MMC_CR, MCF, 0);
}

static void xgbe_config_mmc(struct xgbe_prv_data *pdata)
{
	/* Set counters to reset on read */
	XGMAC_IOWRITE_BITS(pdata, MMC_CR, ROR, 1);

	/* Reset the counters */
	XGMAC_IOWRITE_BITS(pdata, MMC_CR, CR, 1);
}

static void xgbe_txq_prepare_tx_stop(struct xgbe_prv_data *pdata,
		unsigned int queue)
{
	unsigned int tx_status;
	unsigned long tx_timeout;

	/* The Tx engine cannot be stopped if it is actively processing
	 * packets. Wait for the Tx queue to empty the Tx fifo.  Don't
	 * wait forever though...
	 */
	tx_timeout = jiffies + (XGBE_DMA_STOP_TIMEOUT * HZ);
	while (time_before(jiffies, tx_timeout)) {
		tx_status = XGMAC_MTL_IOREAD(pdata, queue, MTL_Q_TQDR);
		if ((XGMAC_GET_BITS(tx_status, MTL_Q_TQDR, TRCSTS) != 1) &&
				(XGMAC_GET_BITS(tx_status, MTL_Q_TQDR, TXQSTS) == 0))
			break;

		XGBE_USLEEP_RANGE(500, 1000);
	}

	if (!time_before(jiffies, tx_timeout))
		netdev_info(pdata->netdev,
				"timed out waiting for Tx queue %u to empty\n",
				queue);
}

static void xgbe_prepare_tx_stop(struct xgbe_prv_data *pdata,
		unsigned int queue)
{
	unsigned int tx_dsr, tx_pos, tx_qidx;
	unsigned int tx_status;
	unsigned long tx_timeout;

	if (XGMAC_GET_BITS(pdata->hw_feat.version, MAC_VR, SNPSVER) > 0x20)
		return xgbe_txq_prepare_tx_stop(pdata, queue);

	/* Calculate the status register to read and the position within */
	if (queue < DMA_DSRX_FIRST_QUEUE) {
		tx_dsr = DMA_DSR0;
		tx_pos = (queue * DMA_DSR_Q_WIDTH) + DMA_DSR0_TPS_START;
	} else {
		tx_qidx = queue - DMA_DSRX_FIRST_QUEUE;

		tx_dsr = DMA_DSR1 + ((tx_qidx / DMA_DSRX_QPR) * DMA_DSRX_INC);
		tx_pos = ((tx_qidx % DMA_DSRX_QPR) * DMA_DSR_Q_WIDTH) +
			DMA_DSRX_TPS_START;
	}

	/* The Tx engine cannot be stopped if it is actively processing
	 * descriptors. Wait for the Tx engine to enter the stopped or
	 * suspended state.  Don't wait forever though...
	 */
	tx_timeout = jiffies + (XGBE_DMA_STOP_TIMEOUT * HZ);
	while (time_before(jiffies, tx_timeout)) {
		tx_status = XGMAC_IOREAD(pdata, tx_dsr);
		tx_status = GET_BITS(tx_status, tx_pos, DMA_DSR_TPS_WIDTH);
		if ((tx_status == DMA_TPS_STOPPED) ||
				(tx_status == DMA_TPS_SUSPENDED))
			break;

		XGBE_USLEEP_RANGE(500, 1000);
	}

	if (!time_before(jiffies, tx_timeout))
		netdev_info(pdata->netdev,
				"timed out waiting for Tx DMA channel %u to stop\n",
				queue);
}
#endif

static void xgbe_enable_tx(struct xgbe_prv_data *pdata)
{
	unsigned int i;

#if XGBE_SRIOV_PF
	/* Enable each Tx queue */
	for (i = 0; i < pdata->tx_q_count; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TXQEN,
				MTL_Q_ENABLED);
#endif

#if XGBE_SRIOV_VF
    pdata->mbx.enable_vf_tx_queue(pdata);
#endif

	/* Enable each Tx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->tx_ring)
			break;

		TEST_PRNT("Enabling DMA Tx channel %d\n", i);
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR, ST, 1);
	}

#if XGBE_SRIOV_PF
	/* Enable MAC Tx */
	XGMAC_IOWRITE_BITS(pdata, MAC_TCR, TE, 1);
#endif

}

#if XGBE_SRIOV_PF
int xgbe_enable_vf_tx_queue(struct xgbe_prv_data *pdata, int vf)
{
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf];
	unsigned int tx_q_start, total_tx_q, i;

	tx_q_start = vfinfo->dma_channel_start_num;
	total_tx_q = tx_q_start + (vfinfo->num_dma_channels);

	/* Enable each Tx queue */
	for (i = tx_q_start; i < total_tx_q; i++)
		XGMAC_MTL_IOWRITE_BITS(pdata, i, MTL_Q_TQOMR, TXQEN,
				MTL_Q_ENABLED);
	return 0;

}
#endif

static void xgbe_disable_tx(struct xgbe_prv_data *pdata)
{
	unsigned int i;

#if XGBE_SRIOV_PF
	/* Prepare for Tx DMA channel stop */
	for (i = 0; i < pdata->tx_q_count; i++)
		xgbe_prepare_tx_stop(pdata, i);
#endif

	/* Disable each Tx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->tx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR, ST, 0);
	}
}

#if XGBE_SRIOV_PF
static void xgbe_prepare_rx_stop(struct xgbe_prv_data *pdata,
		unsigned int queue)
{
	unsigned int rx_status;
	unsigned long rx_timeout;

	/* The Rx engine cannot be stopped if it is actively processing
	 * packets. Wait for the Rx queue to empty the Rx fifo.  Don't
	 * wait forever though...
	 */
	rx_timeout = jiffies + (XGBE_DMA_STOP_TIMEOUT * HZ);
	while (time_before(jiffies, rx_timeout)) {
		rx_status = XGMAC_MTL_IOREAD(pdata, queue, MTL_Q_RQDR);
		if ((XGMAC_GET_BITS(rx_status, MTL_Q_RQDR, PRXQ) == 0) &&
				(XGMAC_GET_BITS(rx_status, MTL_Q_RQDR, RXQSTS) == 0))
			break;

		XGBE_USLEEP_RANGE(500, 1000);
	}

	if (!time_before(jiffies, rx_timeout))
		netdev_info(pdata->netdev,
				"timed out waiting for Rx queue %u to empty\n",
				queue);
}
#endif

static void xgbe_enable_rx(struct xgbe_prv_data *pdata)
{
	unsigned int i;
#if XGBE_SRIOV_PF
	unsigned int reg_val;
#endif
	/* Enable each Rx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, SR, 1);
#if XGBE_SRIOV_VF
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, RPF, 0);
#endif
	}
#if XGBE_SRIOV_PF
	/* Enable each Rx queue */
	reg_val = 0;
	for (i = 0; i < pdata->rx_q_count; i++)
		reg_val |= (0x02 << (i << 1));
	XGMAC_IOWRITE(pdata, MAC_RQC0R, reg_val);
#if TEST_MSG
	XGMAC_IOWRITE(pdata, MAC_RQC0R, (reg_val & 0xffffffff));
	TEST_PRNT("RXQUEUE Enable reg = %x\n", XGMAC_IOREAD(pdata, MAC_RQC0R));
#endif
	/* Enable MAC Rx */
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, DCRCC, 1);
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, CST, 1);
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, RE, 1);
#endif
}

static void xgbe_disable_rx(struct xgbe_prv_data *pdata)
{
	unsigned int i;
#if XGBE_SRIOV_PF
	unsigned int timeout = XGBE_IO_TIMEOUT;

	/* Disable MAC Rx */
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, DCRCC, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, CST, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, ACS, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, RE, 0);

	/* Disable each Rx queue */
	XGMAC_IOWRITE(pdata, MAC_RQC0R, 0);

	/* Prepare for Rx DMA channel stop */
	for (i = 0; i < pdata->rx_q_count; i++)
		xgbe_prepare_rx_stop(pdata, i);
#endif

	/* Disable each Rx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, SR, 0);
#if XGBE_SRIOV_PF
		while((!(XGMAC_DMA_IOREAD_BITS(pdata->channel[i], DMA_CH_SR, RPS))) && (timeout != 0))
		{
			timeout--;
		}
		if(timeout == 0)
			TEST_PRNT("Timed out waiting for Rx DMA channel %d to stop\n", i);
#endif

#if XGBE_SRIOV_VF
		XGBE_MSLEEP(15);
#endif
		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, RPF, 1);
#if XGBE_SRIOV_PF
		timeout = XGBE_IO_TIMEOUT;
#endif
	}
}

#if XGBE_SRIOV_PF
static void xgbe_powerup_tx(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	/* Enable each Tx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->tx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR, ST, 1);
	}

	/* Enable MAC Tx */
	XGMAC_IOWRITE_BITS(pdata, MAC_TCR, TE, 1);
}

static void xgbe_powerdown_tx(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	/* Prepare for Tx DMA channel stop */
	for (i = 0; i < pdata->tx_q_count; i++)
		xgbe_prepare_tx_stop(pdata, i);

	/* Disable MAC Tx */
	XGMAC_IOWRITE_BITS(pdata, MAC_TCR, TE, 0);

	/* Disable each Tx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->tx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_TCR, ST, 0);
	}
}

static void xgbe_powerup_rx(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	/* Enable each Rx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, SR, 1);
	}
}

static void xgbe_powerdown_rx(struct xgbe_prv_data *pdata)
{
	unsigned int i;

	/* Disable each Rx DMA channel */
	for (i = 0; i < pdata->channel_count; i++) {
		if (!pdata->channel[i]->rx_ring)
			break;

		XGMAC_DMA_IOWRITE_BITS(pdata->channel[i], DMA_CH_RCR, SR, 0);
	}
}

#endif

static int xgbe_init(struct xgbe_prv_data *pdata)
{
	struct xgbe_desc_if *desc_if = &pdata->desc_if;
	int ret;

	DBGPR("--> %s\n", __func__);

	/* Flush Tx queues */
	ret = xgbe_flush_tx_queues(pdata);
	if (ret) {
		netdev_err(pdata->netdev, "error flushing TX queues\n");
		return ret;
	}

	/*
	 * Initialize DMA related features
	 */
#if XGBE_SRIOV_PF
	xgbe_config_dma_bus(pdata);
	xgbe_config_dma_cache(pdata);
#endif
	xgbe_config_osp_mode(pdata);
	xgbe_config_pbl_val(pdata);
	xgbe_config_rx_coalesce(pdata);
	xgbe_config_rx_buffer_size(pdata);
	xgbe_config_tso_mode(pdata);
	xgbe_config_sph_mode(pdata);
#if XGBE_SRIOV_PF
#if !ELI_ENABLE
	xgbe_config_rss(pdata);
#endif
#endif
	desc_if->wrapper_tx_desc_init(pdata);
	desc_if->wrapper_rx_desc_init(pdata);
	xgbe_enable_dma_interrupts(pdata);

#if XGBE_SRIOV_PF
	/*
	 * Initialize MTL related features
	 */
	xgbe_config_mtl_mode(pdata);
	xgbe_config_queue_mapping(pdata);
	xgbe_config_tsf_mode(pdata, pdata->tx_sf_mode);
	xgbe_config_rsf_mode(pdata, pdata->rx_sf_mode);
	xgbe_config_tx_threshold(pdata, pdata->tx_threshold);
	xgbe_config_rx_threshold(pdata, pdata->rx_threshold);
	xgbe_config_tx_fifo_size(pdata);
	xgbe_config_rx_fifo_size(pdata);
	/*TODO: Error Packet and undersized good Packet forwarding enable
		(FEP and FUP)
	 */
	xgbe_config_dcb_tc(pdata);
	xgbe_enable_mtl_interrupts(pdata);
#endif

	/*
	 * Initialize MAC related features
	 */
	xgbe_config_mac_address(pdata);
	xgbe_config_rx_mode(pdata);
	xgbe_config_jumbo_enable(pdata);
#if XGBE_SRIOV_PF
	xgbe_config_flow_control(pdata);
	xgbe_config_mac_speed(pdata);
#endif
	xgbe_config_checksum_offload(pdata);
	xgbe_config_vlan_support(pdata);
#if XGBE_SRIOV_PF
	xgbe_config_mmc(pdata);
	xgbe_enable_mac_interrupts(pdata);
#endif
	DBGPR("<-- %s\n", __func__);

	return 0;
}

#if XGBE_SRIOV_PF
#if ELI_ENABLE
void xgbe_init_function_ptrs_dev(struct xgbe_hw_if *hw_if)
{
	DBGPR("-->%s\n", __func__);

	hw_if->tx_complete = xgbe_tx_complete;

	hw_if->set_mac_address = xgbe_set_mac_address;
	hw_if->set_mac_address_sriov = xgbe_set_mac_address_sriov;
	hw_if->clear_mac_address_sriov = xgbe_clear_mac_address_sriov;
	hw_if->config_rx_mode = xgbe_config_rx_mode;
	hw_if->set_promiscuous_mode = xgbe_set_promiscuous_mode;
	hw_if->enable_rx_csum = xgbe_enable_rx_csum;
	hw_if->disable_rx_csum = xgbe_disable_rx_csum;
	hw_if->enable_jumbo_frame = xgbe_enable_jumbo_frame;
	hw_if->enable_rx_vlan_stripping = xgbe_enable_rx_vlan_stripping;
	hw_if->disable_rx_vlan_stripping = xgbe_disable_rx_vlan_stripping;
	hw_if->enable_rx_vlan_filtering = xgbe_enable_rx_vlan_filtering;
	hw_if->disable_rx_vlan_filtering = xgbe_disable_rx_vlan_filtering;
	hw_if->update_vlan_hash_table = xgbe_update_vlan_hash_table;
	hw_if->update_vlan_hash_table_vf = xgbe_update_vlan_hash_table_vf;
	hw_if->flush_tx_queues_vf = xgbe_flush_tx_queues_vf;
	hw_if->set_mac_hash_table_vf = xgbe_set_mac_hash_table_vf;

	hw_if->read_mmd_regs = xgbe_read_mmd_regs;
	hw_if->write_mmd_regs = xgbe_write_mmd_regs;

	hw_if->set_speed = xgbe_set_speed;

	hw_if->set_ext_mii_mode = xgbe_set_ext_mii_mode;
	hw_if->read_ext_mii_regs = xgbe_read_ext_mii_regs;
	hw_if->write_ext_mii_regs = xgbe_write_ext_mii_regs;

	hw_if->set_gpio = xgbe_set_gpio;
	hw_if->clr_gpio = xgbe_clr_gpio;

	hw_if->enable_tx = xgbe_enable_tx;
	hw_if->enable_vf_tx_queue = xgbe_enable_vf_tx_queue;    
	hw_if->disable_tx = xgbe_disable_tx;
	hw_if->enable_rx = xgbe_enable_rx;
	hw_if->disable_rx = xgbe_disable_rx;

	hw_if->powerup_tx = xgbe_powerup_tx;
	hw_if->powerdown_tx = xgbe_powerdown_tx;
	hw_if->powerup_rx = xgbe_powerup_rx;
	hw_if->powerdown_rx = xgbe_powerdown_rx;

	hw_if->dev_xmit = xgbe_dev_xmit;
	hw_if->dev_read = xgbe_dev_read;
	hw_if->enable_int = xgbe_enable_int;
	hw_if->disable_int = xgbe_disable_int;
	hw_if->init = xgbe_init;
	hw_if->exit = xgbe_exit;

#if BRCM_FH
	/* Flexible header related settings */
	hw_if->flex_hdr_set = xgbe_flex_hdr_set;
	hw_if->set_flex_hdr_length = xgbe_set_flex_hdr_length;
	hw_if->set_flex_hdr_position = xgbe_set_flex_hdr_position;
	hw_if->enable_link_layer_flex_hdr = xgbe_enable_link_layer_flex_hdr;
	hw_if->enable_tx_path_flex_hdr = xgbe_enable_tx_path_flex_hdr;
	hw_if->enable_rx_path_flex_hdr = xgbe_enable_rx_path_flex_hdr;
#endif

	/* Descriptor related Sequences have to be initialized here */
	hw_if->tx_desc_init = xgbe_tx_desc_init;
	hw_if->rx_desc_init = xgbe_rx_desc_init;
	hw_if->tx_desc_reset = xgbe_tx_desc_reset;
	hw_if->rx_desc_reset = xgbe_rx_desc_reset;
	hw_if->is_last_desc = xgbe_is_last_desc;
	hw_if->is_context_desc = xgbe_is_context_desc;
	hw_if->tx_start_xmit = xgbe_tx_start_xmit;

	/* For FLOW ctrl */
	hw_if->config_tx_flow_control = xgbe_config_tx_flow_control;
	hw_if->config_rx_flow_control = xgbe_config_rx_flow_control;

	/* For RX coalescing */
	hw_if->config_rx_coalesce = xgbe_config_rx_coalesce;
	hw_if->usec_to_riwt = xgbe_usec_to_riwt;
	hw_if->riwt_to_usec = xgbe_riwt_to_usec;

	/* For RX and TX threshold config */
	hw_if->config_rx_threshold = xgbe_config_rx_threshold;
	hw_if->config_tx_threshold = xgbe_config_tx_threshold;

	/* For RX and TX Store and Forward Mode config */
	hw_if->config_rsf_mode = xgbe_config_rsf_mode;
	hw_if->config_tsf_mode = xgbe_config_tsf_mode;

	/* For TX DMA Operating on Second Frame config */
	hw_if->config_osp_mode = xgbe_config_osp_mode;

	/* For MMC statistics support */
	hw_if->tx_mmc_int = xgbe_tx_mmc_int;
	hw_if->rx_mmc_int = xgbe_rx_mmc_int;
	hw_if->read_mmc_stats = xgbe_read_mmc_stats;

	/* For PTP config */
	hw_if->config_tstamp = xgbe_config_tstamp;
	hw_if->update_tstamp_addend = xgbe_update_tstamp_addend;
	hw_if->set_tstamp_time = xgbe_set_tstamp_time;
	hw_if->get_tstamp_time = xgbe_get_tstamp_time;
	hw_if->get_tx_tstamp = xgbe_get_tx_tstamp;

	/* For Data Center Bridging config */
	hw_if->config_tc = xgbe_config_tc;
	hw_if->config_dcb_tc = xgbe_config_dcb_tc;
	hw_if->config_dcb_pfc = xgbe_config_dcb_pfc;

	/* For VXLAN */
	hw_if->enable_vxlan = xgbe_enable_vxlan;
	hw_if->disable_vxlan = xgbe_disable_vxlan;
	hw_if->set_vxlan_id = xgbe_set_vxlan_id;

	DBGPR("<--xgbe_init_function_ptrs\n");
}
#else
void xgbe_init_function_ptrs_dev(struct xgbe_hw_if *hw_if)
{
	DBGPR("-->xgbe_init_function_ptrs\n");

	hw_if->tx_complete = xgbe_tx_complete;

	hw_if->set_mac_address = xgbe_set_mac_address;
	hw_if->set_mac_address_sriov = xgbe_set_mac_address_sriov;
	hw_if->clear_mac_address_sriov = xgbe_clear_mac_address_sriov;
	hw_if->config_rx_mode = xgbe_config_rx_mode;
	hw_if->set_promiscuous_mode = xgbe_set_promiscuous_mode;
	hw_if->enable_rx_csum = xgbe_enable_rx_csum;
	hw_if->disable_rx_csum = xgbe_disable_rx_csum;

	hw_if->enable_rx_vlan_stripping = xgbe_enable_rx_vlan_stripping;
	hw_if->disable_rx_vlan_stripping = xgbe_disable_rx_vlan_stripping;
	hw_if->enable_rx_vlan_filtering = xgbe_enable_rx_vlan_filtering;
	hw_if->disable_rx_vlan_filtering = xgbe_disable_rx_vlan_filtering;
	hw_if->update_vlan_hash_table = xgbe_update_vlan_hash_table;

	hw_if->read_mmd_regs = xgbe_read_mmd_regs;
	hw_if->write_mmd_regs = xgbe_write_mmd_regs;

	hw_if->set_speed = xgbe_set_speed;

	hw_if->set_ext_mii_mode = xgbe_set_ext_mii_mode;
	hw_if->read_ext_mii_regs = xgbe_read_ext_mii_regs;
	hw_if->write_ext_mii_regs = xgbe_write_ext_mii_regs;

	hw_if->set_gpio = xgbe_set_gpio;
	hw_if->clr_gpio = xgbe_clr_gpio;

	hw_if->enable_tx = xgbe_enable_tx;
	hw_if->disable_tx = xgbe_disable_tx;
	hw_if->enable_rx = xgbe_enable_rx;
	hw_if->disable_rx = xgbe_disable_rx;

	hw_if->powerup_tx = xgbe_powerup_tx;
	hw_if->powerdown_tx = xgbe_powerdown_tx;
	hw_if->powerup_rx = xgbe_powerup_rx;
	hw_if->powerdown_rx = xgbe_powerdown_rx;

	hw_if->dev_xmit = xgbe_dev_xmit;
	hw_if->dev_read = xgbe_dev_read;
	hw_if->enable_int = xgbe_enable_int;
	hw_if->disable_int = xgbe_disable_int;
	hw_if->init = xgbe_init;
	hw_if->exit = xgbe_exit;

#if BRCM_FH
	/* Flexible header related settings */
	hw_if->flex_hdr_set = xgbe_flex_hdr_set;
	hw_if->set_flex_hdr_length = xgbe_set_flex_hdr_length;
	hw_if->set_flex_hdr_position = xgbe_set_flex_hdr_position;
	hw_if->enable_link_layer_flex_hdr = xgbe_enable_link_layer_flex_hdr;
	hw_if->enable_tx_path_flex_hdr = xgbe_enable_tx_path_flex_hdr;
	hw_if->enable_rx_path_flex_hdr = xgbe_enable_rx_path_flex_hdr;
#endif

	/* Descriptor related Sequences have to be initialized here */
	hw_if->tx_desc_init = xgbe_tx_desc_init;
	hw_if->rx_desc_init = xgbe_rx_desc_init;
	hw_if->tx_desc_reset = xgbe_tx_desc_reset;
	hw_if->rx_desc_reset = xgbe_rx_desc_reset;
	hw_if->is_last_desc = xgbe_is_last_desc;
	hw_if->is_context_desc = xgbe_is_context_desc;
	hw_if->tx_start_xmit = xgbe_tx_start_xmit;

	/* For FLOW ctrl */
	hw_if->config_tx_flow_control = xgbe_config_tx_flow_control;
	hw_if->config_rx_flow_control = xgbe_config_rx_flow_control;

	/* For RX coalescing */
	hw_if->config_rx_coalesce = xgbe_config_rx_coalesce;
	hw_if->usec_to_riwt = xgbe_usec_to_riwt;
	hw_if->riwt_to_usec = xgbe_riwt_to_usec;

	/* For RX and TX threshold config */
	hw_if->config_rx_threshold = xgbe_config_rx_threshold;
	hw_if->config_tx_threshold = xgbe_config_tx_threshold;

	/* For RX and TX Store and Forward Mode config */
	hw_if->config_rsf_mode = xgbe_config_rsf_mode;
	hw_if->config_tsf_mode = xgbe_config_tsf_mode;

	/* For TX DMA Operating on Second Frame config */
	hw_if->config_osp_mode = xgbe_config_osp_mode;

	/* For MMC statistics support */
	hw_if->tx_mmc_int = xgbe_tx_mmc_int;
	hw_if->rx_mmc_int = xgbe_rx_mmc_int;
	hw_if->read_mmc_stats = xgbe_read_mmc_stats;

	/* For PTP config */
	hw_if->config_tstamp = xgbe_config_tstamp;
	hw_if->update_tstamp_addend = xgbe_update_tstamp_addend;
	hw_if->set_tstamp_time = xgbe_set_tstamp_time;
	hw_if->get_tstamp_time = xgbe_get_tstamp_time;
	hw_if->get_tx_tstamp = xgbe_get_tx_tstamp;

	/* For Data Center Bridging config */
	hw_if->config_tc = xgbe_config_tc;
	hw_if->config_dcb_tc = xgbe_config_dcb_tc;
	hw_if->config_dcb_pfc = xgbe_config_dcb_pfc;

	hw_if->add_vlan = xgbe_add_vlan;
	hw_if->kill_vlan = xgbe_kill_vlan;
	/* For Receive Side Scaling */
	hw_if->enable_rss = xgbe_enable_rss;
	hw_if->disable_rss = xgbe_disable_rss;
	hw_if->set_rss_hash_key = xgbe_set_rss_hash_key;
	hw_if->set_rss_lookup_table = xgbe_set_rss_lookup_table;

	/* For VXLAN */
	hw_if->enable_vxlan = xgbe_enable_vxlan;
	hw_if->disable_vxlan = xgbe_disable_vxlan;
	hw_if->set_vxlan_id = xgbe_set_vxlan_id;

	DBGPR("<--xgbe_init_function_ptrs\n");
}
#endif
#endif

#if XGBE_SRIOV_VF
void xgbe_init_function_ptrs_dev(struct xgbe_hw_if *hw_if)
{
	DBGPR("-->%s\n", __func__);

	hw_if->tx_complete = xgbe_tx_complete;

	hw_if->set_mac_address = xgbe_set_mac_address;
	hw_if->config_rx_mode = xgbe_config_rx_mode;

	hw_if->enable_rx_csum = xgbe_enable_rx_csum;
	hw_if->disable_rx_csum = xgbe_disable_rx_csum;

	hw_if->enable_rx_vlan_stripping = xgbe_enable_rx_vlan_stripping;
	hw_if->disable_rx_vlan_stripping = xgbe_disable_rx_vlan_stripping;
	hw_if->enable_rx_vlan_filtering = xgbe_enable_rx_vlan_filtering;
	hw_if->disable_rx_vlan_filtering = xgbe_disable_rx_vlan_filtering;
	hw_if->update_vlan_hash_table = xgbe_update_vlan_hash_table;

	hw_if->enable_tx = xgbe_enable_tx;
	hw_if->disable_tx = xgbe_disable_tx;
	hw_if->enable_rx = xgbe_enable_rx;
	hw_if->disable_rx = xgbe_disable_rx;

	hw_if->dev_xmit = xgbe_dev_xmit;
	hw_if->dev_read = xgbe_dev_read;
	hw_if->enable_int = xgbe_enable_int;
	hw_if->disable_int = xgbe_disable_int;
	hw_if->init = xgbe_init;
	hw_if->exit = xgbe_exit;

#if BRCM_FH
	/* Flexible header related settings */
	hw_if->flex_hdr_set = xgbe_flex_hdr_set;
	hw_if->set_flex_hdr_length = xgbe_set_flex_hdr_length;
	hw_if->set_flex_hdr_position = xgbe_set_flex_hdr_position;
	hw_if->enable_link_layer_flex_hdr = xgbe_enable_link_layer_flex_hdr;
	hw_if->enable_tx_path_flex_hdr = xgbe_enable_tx_path_flex_hdr;
	hw_if->enable_rx_path_flex_hdr = xgbe_enable_rx_path_flex_hdr;
#endif

	/* Descriptor related Sequences have to be initialized here */
	hw_if->tx_desc_init = xgbe_tx_desc_init;
	hw_if->rx_desc_init = xgbe_rx_desc_init;
	hw_if->tx_desc_reset = xgbe_tx_desc_reset;
	hw_if->rx_desc_reset = xgbe_rx_desc_reset;
	hw_if->is_last_desc = xgbe_is_last_desc;
	hw_if->is_context_desc = xgbe_is_context_desc;
	hw_if->tx_start_xmit = xgbe_tx_start_xmit;

	/* For RX coalescing */
	hw_if->config_rx_coalesce = xgbe_config_rx_coalesce;
	hw_if->usec_to_riwt = xgbe_usec_to_riwt;
	hw_if->riwt_to_usec = xgbe_riwt_to_usec;


	/* For TX DMA Operating on Second Frame config */
	hw_if->config_osp_mode = xgbe_config_osp_mode;

	/* For PTP config */
	hw_if->config_tstamp = xgbe_config_tstamp;
    hw_if->get_tx_tstamp =  xgbe_get_tx_tstamp;
	/* For VXLAN */
	hw_if->enable_vxlan = xgbe_enable_vxlan;
	hw_if->disable_vxlan = xgbe_disable_vxlan;
	hw_if->set_vxlan_id = xgbe_set_vxlan_id;

	DBGPR("<--%s\n", __func__);
}
#endif
