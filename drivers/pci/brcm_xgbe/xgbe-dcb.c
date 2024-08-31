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

#include <linux/netdevice.h>
#include <net/dcbnl.h>

#include "xgbe.h"
#include "xgbe-common.h"

static int xgbe_dcb_ieee_getets(struct net_device *netdev,
		struct ieee_ets *ets)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	/* Set number of supported traffic classes */
	ets->ets_cap = pdata->hw_feat.tc_cnt;

	if (pdata->ets) {
		ets->cbs = pdata->ets->cbs;
		memcpy(ets->tc_tx_bw, pdata->ets->tc_tx_bw,
				sizeof(ets->tc_tx_bw));
		memcpy(ets->tc_tsa, pdata->ets->tc_tsa,
				sizeof(ets->tc_tsa));
		memcpy(ets->prio_tc, pdata->ets->prio_tc,
				sizeof(ets->prio_tc));
	}

	return 0;
}

static int xgbe_dcb_ieee_setets(struct net_device *netdev,
		struct ieee_ets *ets)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	unsigned int i, tc_ets, tc_ets_weight;
	u8 max_tc = 0;

	tc_ets = 0;
	tc_ets_weight = 0;
	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		netif_dbg(pdata, drv, netdev,
				"TC%u: tx_bw=%hhu, rx_bw=%hhu, tsa=%hhu\n", i,
				ets->tc_tx_bw[i], ets->tc_rx_bw[i],
				ets->tc_tsa[i]);
		netif_dbg(pdata, drv, netdev, "PRIO%u: TC=%hhu\n", i,
				ets->prio_tc[i]);

		max_tc = max_t(u8, max_tc, ets->prio_tc[i]);
		if ((ets->tc_tx_bw[i] || ets->tc_tsa[i]))
			max_tc = max_t(u8, max_tc, i);

		switch (ets->tc_tsa[i]) {
			case IEEE_8021QAZ_TSA_STRICT:
				break;
			case IEEE_8021QAZ_TSA_ETS:
				tc_ets = 1;
				tc_ets_weight += ets->tc_tx_bw[i];
				break;
			default:
				netif_err(pdata, drv, netdev,
						"unsupported TSA algorithm (%hhu)\n",
						ets->tc_tsa[i]);
				return -EINVAL;
		}
	}

	/* Check maximum traffic class requested */
	if (max_tc >= pdata->hw_feat.tc_cnt) {
		netif_err(pdata, drv, netdev,
				"exceeded number of supported traffic classes\n");
		return -EINVAL;
	}

	/* Weights must add up to 100% */
	if (tc_ets && (tc_ets_weight != 100)) {
		netif_err(pdata, drv, netdev,
				"sum of ETS algorithm weights is not 100 (%u)\n",
				tc_ets_weight);
		return -EINVAL;
	}

	if (!pdata->ets) {
		pdata->ets = devm_kzalloc(pdata->dev, sizeof(*pdata->ets),
				GFP_KERNEL);
		if (!pdata->ets)
			return -ENOMEM;
	}

	pdata->num_tcs = max_tc + 1;
	memcpy(pdata->ets, ets, sizeof(*pdata->ets));
#if XGBE_SRIOV_PF
	pdata->hw_if.config_dcb_tc(pdata);
#endif
	return 0;
}

static int xgbe_dcb_ieee_getpfc(struct net_device *netdev,
		struct ieee_pfc *pfc)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	/* Set number of supported PFC traffic classes */
	pfc->pfc_cap = pdata->hw_feat.tc_cnt;

	if (pdata->pfc) {
		pfc->pfc_en = pdata->pfc->pfc_en;
		pfc->mbc = pdata->pfc->mbc;
		pfc->delay = pdata->pfc->delay;
	}

	return 0;
}

static int xgbe_dcb_ieee_setpfc(struct net_device *netdev,
		struct ieee_pfc *pfc)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	netif_dbg(pdata, drv, netdev,
			"cap=%hhu, en=%#hhx, mbc=%hhu, delay=%hhu\n",
			pfc->pfc_cap, pfc->pfc_en, pfc->mbc, pfc->delay);

	/* Check PFC for supported number of traffic classes */
	if (pfc->pfc_en & ~((1 << pdata->hw_feat.tc_cnt) - 1)) {
		netif_err(pdata, drv, netdev,
				"PFC requested for unsupported traffic class\n");
		return -EINVAL;
	}

	if (!pdata->pfc) {
		pdata->pfc = devm_kzalloc(pdata->dev, sizeof(*pdata->pfc),
				GFP_KERNEL);
		if (!pdata->pfc)
			return -ENOMEM;
	}

	memcpy(pdata->pfc, pfc, sizeof(*pdata->pfc));
#if XGBE_SRIOV_PF
	pdata->hw_if.config_dcb_pfc(pdata);
#endif
	return 0;
}

static u8 xgbe_dcb_getdcbx(struct net_device *netdev)
{
	return DCB_CAP_DCBX_HOST | DCB_CAP_DCBX_VER_IEEE;
}

static u8 xgbe_dcb_setdcbx(struct net_device *netdev, u8 dcbx)
{
	struct xgbe_prv_data *pdata = netdev_priv(netdev);
	u8 support = xgbe_dcb_getdcbx(netdev);

	netif_dbg(pdata, drv, netdev, "DCBX=%#hhx\n", dcbx);

	if (dcbx & ~support)
		return 1;

	if ((dcbx & support) != support)
		return 1;

	return 0;
}

static const struct dcbnl_rtnl_ops xgbe_dcbnl_ops = {
	/* IEEE 802.1Qaz std */
	.ieee_getets = xgbe_dcb_ieee_getets,
	.ieee_setets = xgbe_dcb_ieee_setets,
	.ieee_getpfc = xgbe_dcb_ieee_getpfc,
	.ieee_setpfc = xgbe_dcb_ieee_setpfc,

	/* DCBX configuration */
	.getdcbx     = xgbe_dcb_getdcbx,
	.setdcbx     = xgbe_dcb_setdcbx,
};

const struct dcbnl_rtnl_ops *xgbe_get_dcbnl_ops(void)
{
	return &xgbe_dcbnl_ops;
}
