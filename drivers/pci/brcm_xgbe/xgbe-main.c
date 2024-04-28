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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/pci.h>

#include "xgbe.h"
#include "xgbe-common.h"

 MODULE_AUTHOR(" ");
 MODULE_LICENSE("Dual BSD/GPL");
 MODULE_VERSION(XGBE_DRV_VERSION);
 MODULE_DESCRIPTION(XGBE_DRV_DESC);

#if XGBE_SRIOV_PF
extern unsigned int max_vfs;
#endif

 static int debug = -1;
 module_param(debug, int, 0644);
 MODULE_PARM_DESC(debug, " Network interface message level setting");

 static const u32 default_msg_level = (NETIF_MSG_LINK | NETIF_MSG_IFDOWN |
		 NETIF_MSG_IFUP);

#if BRCM_FH
static unsigned int mp_fhdr_high = 0x00000000;
static unsigned int mp_fhdr_low =  0x00000000;
//static unsigned int mp_fhdr_len = 4;
//static unsigned int mp_fhdr_pos = 12;
//static unsigned int mp_fhdr_en_ll = 1;
static unsigned int mp_fhdr_en_tx = 1;
static unsigned int mp_fhdr_en_rx = 1;

static void xgbe_config_flex_header(struct xgbe_prv_data *pdata)
{
//	xgbe_set_flex_header_length(pdata, mp_fhdr_len);
//	xgbe_set_flex_header_position(pdata, mp_fhdr_pos);
//	xgbe_enable_link_layer_flex_header(pdata, mp_fhdr_en_ll);
	xgbe_enable_tx_path_flex_header(pdata, mp_fhdr_en_tx);
	xgbe_enable_rx_path_flex_header(pdata, mp_fhdr_en_rx);
	xgbe_set_flex_header(pdata, mp_fhdr_high, mp_fhdr_low);
}
#endif

static int cb_pause_quanta(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < 1 || n > 65355) {
		printk("Valid range for tx_pause_quanta is from 1 to 65535\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_pause_quanta = {
	.set	= cb_pause_quanta,
	.get	= param_get_int,
};
static int mp_pause_quanta = 0xffff;
module_param_cb(pause_quanta, &po_pause_quanta, &mp_pause_quanta, 0660);

static int cb_tx_desc_count(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < XGBE_MIN_DESC || n > XGBE_MAX_DESC) {
		printk("Valid range for tx_desc_count is from 4096 to 8192\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_tx_desc_count = {
	.set	= cb_tx_desc_count,
	.get	= param_get_int,
};
static int mp_tx_desc_count = XGBE_TX_DESC_CNT;
module_param_cb(tx_desc_count, &po_tx_desc_count, &mp_tx_desc_count, 0660);

static int cb_rx_desc_count(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < XGBE_MIN_DESC || n > XGBE_MAX_DESC) {
		printk("Valid range for rx_desc_count is from 4096 to 8192\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_rx_desc_count = {
	.set	= cb_rx_desc_count,
	.get	= param_get_int,
};
static int mp_rx_desc_count = XGBE_RX_DESC_CNT;
module_param_cb(rx_desc_count, &po_rx_desc_count, &mp_rx_desc_count, 0660);

static int cb_blen(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < DMA_SBMR_BLEN_4 || n > DMA_SBMR_BLEN_256) {
		printk("Valid range for blen is from 4 to 256\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_blen = {
	.set	= cb_blen,
	.get	= param_get_int,
};
static int mp_blen = -1;
module_param_cb(blen, &po_blen, &mp_blen, 0660);

static int cb_pbl(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < DMA_PBL_1 || n > DMA_PBL_256) {
		printk("Valid range for pbl is from 1 to 256\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_pbl = {
	.set	= cb_pbl,
	.get	= param_get_int,
};
static int mp_pbl = DMA_PBL_32;
module_param_cb(pbl, &po_pbl, &mp_pbl, 0660);

static int cb_rd_osr_limit(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < RD_OSR_LIMIT_MIN || n > RD_OSR_LIMIT_MAX) {
		printk("Valid range for rd_osr_limit is from 0 to 64\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_rd_osr_limit = {
	.set	= cb_rd_osr_limit,
	.get	= param_get_int,
};
static int mp_rd_osr_limit = -1;
module_param_cb(rd_osr_limit, &po_rd_osr_limit, &mp_rd_osr_limit, 0660);

static int cb_wr_osr_limit(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < WR_OSR_LIMIT_MIN || n > WR_OSR_LIMIT_MAX) {
		printk("Valid range for wr_osr_limit is from 0 to 64\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_wr_osr_limit = {
	.set	= cb_wr_osr_limit,
	.get	= param_get_int,
};
static int mp_wr_osr_limit = -1;
module_param_cb(wr_osr_limit, &po_wr_osr_limit, &mp_wr_osr_limit, 0660);

static int cb_tx_threshold(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < MTL_TX_THRESHOLD_64 || n > MTL_TX_THRESHOLD_512) {
		printk("Valid range for tx_threshold is from 0 to 7\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_tx_threshold = {
	.set	= cb_tx_threshold,
	.get	= param_get_int,
};
static int mp_tx_threshold = MTL_TX_THRESHOLD_64;
module_param_cb(tx_threshold, &po_tx_threshold, &mp_tx_threshold, 0660);

static int cb_rx_threshold(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < MTL_RX_THRESHOLD_64 || n > MTL_RX_THRESHOLD_128) {
		printk("Valid range for rx_threshold is from 0 to 3\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_rx_threshold = {
	.set	= cb_rx_threshold,
	.get	= param_get_int,
};
static int mp_rx_threshold = MTL_RX_THRESHOLD_64;
module_param_cb(rx_threshold, &po_rx_threshold, &mp_rx_threshold, 0660);

static int cb_pause_autoneg(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < AUTO_NEG_DISABLE || n > AUTO_NEG_ENABLE) {
		printk("Valid range for pause_autoneg is 0(disable) and 1(enable)\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_pause_autoneg = {
	.set	= cb_pause_autoneg,
	.get	= param_get_int,
};
static int mp_pause_autoneg = AUTO_NEG_ENABLE;
module_param_cb(pause_autoneg, &po_pause_autoneg, &mp_pause_autoneg, 0660);

static int cb_tx_sf_mode(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < MTL_TSF_DISABLE || n > MTL_TSF_ENABLE) {
		printk("Valid range for tx_sf_mode is 0(disable) and 1(enable)\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_tx_sf_mode = {
	.set	= cb_tx_sf_mode,
	.get	= param_get_int,
};
static int mp_tx_sf_mode = MTL_TSF_ENABLE;
module_param_cb(tx_sf_mode, &po_tx_sf_mode, &mp_tx_sf_mode, 0660);

static int cb_rx_sf_mode(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < MTL_RSF_DISABLE || n > MTL_RSF_ENABLE) {
		printk("Valid range for rx_sf_mode is 0(disable) and 1(enable)\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_rx_sf_mode = {
	.set	= cb_rx_sf_mode,
	.get	= param_get_int,
};
static int mp_rx_sf_mode = MTL_RSF_DISABLE;
module_param_cb(rx_sf_mode, &po_rx_sf_mode, &mp_rx_sf_mode, 0660);

static int cb_tx_osp_mode(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < DMA_OSP_DISABLE || n > DMA_OSP_ENABLE) {
		printk("Valid range for tx_osp_mode is 0(disable) and 1(enable)\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_tx_osp_mode = {
	.set	= cb_tx_osp_mode,
	.get	= param_get_int,
};
static int mp_tx_osp_mode = DMA_OSP_ENABLE;
module_param_cb(tx_osp_mode, &po_tx_osp_mode, &mp_tx_osp_mode, 0660);

static int cb_phy_speed(const char *val, const struct kernel_param *kp)
{
	int n = 0, ret;
 
	ret = kstrtoint(val, DEC_BASE, &n);
	if (ret != 0 || n < SPEED_1000 || n > SPEED_10000) {
		printk("Valid range for phy_speed is from 1000 to 10000\n");
		return -EINVAL;
	}
 
	return param_set_int(val, kp);
}
static const struct kernel_param_ops po_phy_speed = {
	.set	= cb_phy_speed,
	.get	= param_get_int,
};


static int mp_phy_speed = SPEED_UNKNOWN;

module_param_cb(phy_speed, &po_phy_speed, &mp_phy_speed, 0660);


static unsigned int rq_size[XGBE_MAX_QUEUES];
static int rq_count;
module_param_array(rq_size, int, &rq_count, 0660);

static int rq_fca_threshold[XGBE_MAX_QUEUES];
static int rq_fca_count;
module_param_array(rq_fca_threshold, int, &rq_fca_count, 0660);

static int rq_fcd_threshold[XGBE_MAX_QUEUES];
static int rq_fcd_count;
module_param_array(rq_fcd_threshold, int, &rq_fcd_count, 0660);

static void xgbe_default_config(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_PF
	int i;
#endif
	DBGPR("--> %s\n", __func__);

	if(mp_blen == -1) {
		u16 dev_ctl;
		pcie_capability_read_word(pdata->pcidev, PCI_EXP_DEVCTL, &dev_ctl);
		pdata->blen  = (1 << (((dev_ctl & PCI_EXP_DEVCTL_PAYLOAD) >> 5) + 7));

	} else {
		pdata->blen = mp_blen;
	}
	pdata->pbl = mp_pbl;
	pdata->aal = 0;
	if(mp_rd_osr_limit == -1) {
		pdata->rd_osr_limit = 64;
	} else {
		pdata->rd_osr_limit = mp_rd_osr_limit;
	}
	if(mp_wr_osr_limit == -1) {
		pdata->wr_osr_limit = 64;
	} else {
		pdata->wr_osr_limit = mp_wr_osr_limit;
	}
	pdata->tx_sf_mode = mp_tx_sf_mode;
	pdata->tx_threshold = mp_tx_threshold;
	pdata->tx_osp_mode = mp_tx_osp_mode;
	pdata->rx_sf_mode = mp_rx_sf_mode;
	pdata->rx_threshold = mp_rx_threshold;
	pdata->pause_autoneg = mp_pause_autoneg;
	pdata->tx_pause = 1;
	pdata->rx_pause = 1;
	pdata->phy_speed = mp_phy_speed;
#if XGBE_SRIOV_PF
	if(mp_phy_speed == SPEED_UNKNOWN) {
		if(pdata->dev_id == BCM8989X_PF_ID) 
			pdata->phy_speed = SPEED_10000;
		else {
			pdata->phy_speed = SPEED_1000;
		}
	} 
#endif
#if XGBE_SRIOV_VF
	if(mp_phy_speed == SPEED_UNKNOWN) {
		if(pdata->dev_id == BCM8989X_VF_ID) 
			pdata->phy_speed = SPEED_10000;
		else {
			pdata->phy_speed = SPEED_1000;
		}
	}
#endif

	pdata->power_down = 0;
#if XGBE_SRIOV_PF
	if(pdata->dev_id == BCM8989X_PF_ID) {

		if (rq_count != 0) {
			for (i = 0; i < XGBE_MAX_QUEUES; i++) {
				if (rq_size[i] > 1)
					pdata->rq_size[i] = rq_size[i]/256 - 1;
			}
		}
		else {
			for (i = 0; i < XGBE_MAX_QUEUES; i++) {
				if ( i < 8)
					pdata->rq_size[i] = (6656/256) - 1;
				if (i == 10 || i == 11)
					pdata->rq_size[i] = (6144/256) - 1;
			}

		}
		for (i = 0; i < XGBE_MAX_QUEUES; i ++) {
			if (rq_fca_count != 0)
				pdata->rx_rfa[i] = XGMAC_FLOW_CONTROL_VALUE(rq_fca_threshold[i]);
			else
				pdata->rx_rfa[i] = XGMAC_FLOW_CONTROL_VALUE(3584);

			if (rq_fcd_count != 0)
				pdata->rx_rfd[i] = XGMAC_FLOW_CONTROL_VALUE(rq_fcd_threshold[i]);
			else
				pdata->rx_rfd[i] = XGMAC_FLOW_CONTROL_VALUE(5120);
		}
	}
#endif
	DBGPR("<-- %s\n", __func__);
}

static void xgbe_init_all_fptrs(struct xgbe_prv_data *pdata)
{
	xgbe_init_function_ptrs_dev(&pdata->hw_if);
#if XGBE_SRIOV_VF
	xgbe_init_function_ptrs_mbx(&pdata->mbx);
#endif
	xgbe_init_function_ptrs_desc(&pdata->desc_if);

#if XGBE_SRIOV_PF
	xgbe_init_function_ptrs_mbx(&pdata->mbx_if);
	pdata->vdata->init_function_ptrs_phy_impl(&pdata->phy_if);

#if ELI_ENABLE
	xgbe_init_function_ptrs_eli(&pdata->eli_if);
#endif
#endif

}

struct xgbe_prv_data *xgbe_alloc_pdata(struct device *dev)
{
	struct xgbe_prv_data *pdata;
	struct net_device *netdev;

	netdev = alloc_etherdev_mq(sizeof(struct xgbe_prv_data),
			XGBE_MAX_DMA_CHANNELS);
	if (!netdev) {
		dev_err(dev, "alloc_etherdev_mq failed\n");
		return ERR_PTR(-ENOMEM);
	}
	SET_NETDEV_DEV(netdev, dev);
	pdata = netdev_priv(netdev);
	pdata->netdev = netdev;
	pdata->dev = dev;

	spin_lock_init(&pdata->lock);
	spin_lock_init(&pdata->xpcs_lock);
	mutex_init(&pdata->rss_mutex);
	spin_lock_init(&pdata->tstamp_lock);
	init_completion(&pdata->mdio_complete);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0))
	INIT_LIST_HEAD(&pdata->vxlan_ports);
#endif

	pdata->msg_enable = netif_msg_init(debug, default_msg_level);

	set_bit(XGBE_DOWN, &pdata->dev_state);
	set_bit(XGBE_STOPPED, &pdata->dev_state);

	pdata->l3l4_filter_count = 0;
	return pdata;
}

void xgbe_free_pdata(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	free_netdev(netdev);
}

void xgbe_set_counts(struct xgbe_prv_data *pdata)
{
#if XGBE_SRIOV_VF
	struct xgbe_hw_features *hw_feat = &pdata->hw_feat;
	unsigned int dma_ch_cnt;
#endif
	/* Set all the function pointers */
	xgbe_init_all_fptrs(pdata);
#if XGBE_SRIOV_VF
	memset(hw_feat, 0, sizeof(*hw_feat));
#endif
	/* Populate the hardware features */
#if XGBE_SRIOV_PF
	xgbe_get_all_hw_features(pdata);
#endif

#if XGBE_SRIOV_VF
	dma_ch_cnt = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_FUNC_RESOURCES_VF);
	TEST_PRNT("dma_ch_cnt = %d", dma_ch_cnt);
	hw_feat->tx_q_cnt     = dma_ch_cnt-1;
	hw_feat->rx_ch_cnt    = dma_ch_cnt-1;
	hw_feat->tx_ch_cnt    = dma_ch_cnt-1;

	/* The Queue, Channel and TC counts are zero based so increment them
	 * to get the actual number
	 */
	hw_feat->tx_q_cnt++;
	hw_feat->rx_ch_cnt++;
	hw_feat->tx_ch_cnt++;
#endif

	/* Set default max values if not provided */
	if (!pdata->tx_max_channel_count)
		pdata->tx_max_channel_count = pdata->hw_feat.tx_ch_cnt;
	if (!pdata->rx_max_channel_count)
		pdata->rx_max_channel_count = pdata->hw_feat.rx_ch_cnt;

	if (!pdata->tx_max_q_count)
		pdata->tx_max_q_count = pdata->hw_feat.tx_q_cnt;
	if (!pdata->rx_max_q_count)
		pdata->rx_max_q_count = pdata->hw_feat.rx_q_cnt;

	/* Calculate the number of Tx and Rx rings to be created
	 *  -Tx (DMA) Channels map 1-to-1 to Tx Queues so set
	 *   the number of Tx queues to the number of Tx channels
	 *   enabled
	 *  -Rx (DMA) Channels do not map 1-to-1 so use the actual
	 *   number of Rx queues or maximum allowed
	 */
#if XGBE_SRIOV_PF
	pdata->tx_ring_count = min_t(unsigned int, num_online_cpus(),
			pdata->hw_feat.tx_ch_cnt);
#endif

#if XGBE_SRIOV_VF
	pdata->tx_ring_count =	pdata->hw_feat.tx_ch_cnt;
#endif

	pdata->tx_ring_count = min_t(unsigned int, pdata->tx_ring_count,
			pdata->tx_max_channel_count);
	pdata->tx_ring_count = min_t(unsigned int, pdata->tx_ring_count,
			pdata->tx_max_q_count);

	pdata->tx_q_count = pdata->tx_ring_count;
#if XGBE_SRIOV_PF
	pdata->rx_ring_count = min_t(unsigned int, num_online_cpus(),
			pdata->hw_feat.rx_ch_cnt);
#endif

#if XGBE_SRIOV_VF
	pdata->rx_ring_count = pdata->hw_feat.rx_ch_cnt;
#endif
	pdata->rx_ring_count = min_t(unsigned int, pdata->rx_ring_count,
			pdata->rx_max_channel_count);

	pdata->rx_q_count = min_t(unsigned int, pdata->hw_feat.rx_q_cnt,
			pdata->rx_max_q_count);
#if XGBE_SRIOV_PF
	pdata->last_used_dma = 0;
	pdata->vlan_filters_used = 0;
	pdata->max_DMA_pf = pdata->tx_q_count;
	memset(pdata->occupied_eli, 0, 2 * sizeof(unsigned int));
	pdata->promisc = 0;
	pdata->pm_mode = 0;
#endif

#if XGBE_SRIOV_VF
	pdata->mc_mutex = 0;
    pdata->uc_mutex = 0;
#endif

	if (netif_msg_probe(pdata)) {
		dev_dbg(pdata->dev, "TX/RX DMA channel count = %u/%u\n",
				pdata->tx_ring_count, pdata->rx_ring_count);
		dev_dbg(pdata->dev, "TX/RX hardware queue count = %u/%u\n",
				pdata->tx_q_count, pdata->rx_q_count);
	}
}


int xgbe_config_netdev(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	struct device *dev = pdata->dev;
	int ret;

	netdev->irq = pdata->dev_irq;
	netdev->base_addr = (unsigned long)pdata->xgmac_regs;
	memcpy((unsigned char *)netdev->dev_addr, (unsigned char *)pdata->mac_addr, netdev->addr_len);

#if XGBE_SRIOV_PF
	/* Issue software reset to device */
	ret = pdata->hw_if.exit(pdata);
	if (ret) {
		dev_err(dev, "software reset failed\n");
		return ret;
	}
#endif

#if XGBE_SRIOV_VF
	/* Initialize ECC timestamps */
	pdata->tx_sec_period = jiffies;
	pdata->tx_ded_period = jiffies;
	pdata->rx_sec_period = jiffies;
	pdata->rx_ded_period = jiffies;
	pdata->desc_sec_period = jiffies;
	pdata->desc_ded_period = jiffies;
#endif

	/* Set default configuration data */
	xgbe_default_config(pdata);

    pdata->pause_quanta = mp_pause_quanta;

	/* Set the DMA mask */
	ret = dma_set_mask_and_coherent(dev,
			DMA_BIT_MASK(pdata->hw_feat.dma_width));
	if (ret) {
		dev_err(dev, "dma_set_mask_and_coherent failed\n");
		return ret;
	}

	/* Set default max values if not provided */
	if (!pdata->tx_max_fifo_size)
		pdata->tx_max_fifo_size = pdata->hw_feat.tx_fifo_size;
	if (!pdata->rx_max_fifo_size)
		pdata->rx_max_fifo_size = pdata->hw_feat.rx_fifo_size;

	/* Set and validate the number of descriptors for a ring */
	BUILD_BUG_ON_NOT_POWER_OF_2(XGBE_TX_DESC_CNT);
	pdata->tx_desc_count = mp_tx_desc_count;

	BUILD_BUG_ON_NOT_POWER_OF_2(XGBE_RX_DESC_CNT);
	pdata->rx_desc_count = mp_rx_desc_count;

	/* Adjust the number of queues based on interrupts assigned */
	if (pdata->channel_irq_count) {
		pdata->tx_ring_count = min_t(unsigned int, pdata->tx_ring_count,
				pdata->channel_irq_count);
		pdata->rx_ring_count = min_t(unsigned int, pdata->rx_ring_count,
				pdata->channel_irq_count);

		if (netif_msg_probe(pdata))
			dev_dbg(pdata->dev,
					"adjusted TX/RX DMA channel count = %u/%u\n",
					pdata->tx_ring_count, pdata->rx_ring_count);
	}

	/* Initialize RSS hash key */
	netdev_rss_key_fill(pdata->rss_key, sizeof(pdata->rss_key));

	XGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, IP2TE, 1);
	XGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, TCP4TE, 1);
	XGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, UDP4TE, 1);

#if XGBE_SRIOV_PF
	/* Call MDIO/PHY initialization routine */
	ret = pdata->phy_if.phy_impl.init(pdata);
	if (ret)
		return ret;
#endif

	/* Set device operations */
	netdev->netdev_ops = xgbe_get_netdev_ops();
	netdev->ethtool_ops = xgbe_get_ethtool_ops();
#ifdef CONFIG_BCM_XGBE_DCB
	netdev->dcbnl_ops = xgbe_get_dcbnl_ops();
#endif

	/* Set device features */
	netdev->hw_features = NETIF_F_SG |
		NETIF_F_IP_CSUM |
		NETIF_F_IPV6_CSUM |
		NETIF_F_RXCSUM |
		NETIF_F_TSO |
		NETIF_F_TSO6 |
		NETIF_F_GRO |
		NETIF_F_HW_VLAN_CTAG_RX |
		NETIF_F_HW_VLAN_CTAG_TX |
		NETIF_F_HW_VLAN_CTAG_FILTER;

#if XGBE_SRIOV_PF
        netdev->hw_features |= NETIF_F_NTUPLE;
#endif

	if (pdata->hw_feat.rss)
		netdev->hw_features |= NETIF_F_RXHASH;

	if (pdata->hw_feat.vxn) {
		netdev->hw_enc_features = NETIF_F_SG |
			NETIF_F_IP_CSUM |
			NETIF_F_IPV6_CSUM |
			NETIF_F_RXCSUM |
			NETIF_F_TSO |
			NETIF_F_TSO6 |
			NETIF_F_GRO |
			NETIF_F_GSO_UDP_TUNNEL |
			NETIF_F_GSO_UDP_TUNNEL_CSUM;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0))
		netdev->hw_enc_features |= NETIF_F_RX_UDP_TUNNEL_PORT;
#endif

		netdev->hw_features |= NETIF_F_GSO_UDP_TUNNEL |
			NETIF_F_GSO_UDP_TUNNEL_CSUM;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0))
		netdev->hw_features |= NETIF_F_RX_UDP_TUNNEL_PORT;
		pdata->vxlan_offloads_set = 1;
		pdata->vxlan_features = NETIF_F_GSO_UDP_TUNNEL |
			NETIF_F_GSO_UDP_TUNNEL_CSUM |
			NETIF_F_RX_UDP_TUNNEL_PORT;
#else
		netdev->udp_tunnel_nic_info = xgbe_get_udp_tunnel_info();
#endif
	}

	netdev->vlan_features |= NETIF_F_SG |
		NETIF_F_IP_CSUM |
		NETIF_F_IPV6_CSUM |
		NETIF_F_TSO |
		NETIF_F_TSO6;

	netdev->features |= netdev->hw_features;
	pdata->netdev_features = netdev->features;

	netdev->priv_flags |= IFF_UNICAST_FLT;
	netdev->min_mtu = 0;
	netdev->max_mtu = XGMAC_JUMBO_PACKET_MTU;

	/* Use default watchdog timeout */
	netdev->watchdog_timeo = 0;

	xgbe_init_rx_coalesce(pdata);
	xgbe_init_tx_coalesce(pdata);

#if XGBE_SRIOV_PF
	/* assign number of SR-IOV VFs */
	if (max_vfs > XGBE_MAX_VFS_DRV_LIMIT) {
		max_vfs = 0;
		dev_dbg(dev, "max_vfs parameter out of range. Not assigning any SR-IOV VFs\n");
	}
#endif

	netif_carrier_off(netdev);
	ret = register_netdev(netdev);
	if (ret) {
		dev_err(dev, "net device registration failed\n");
		return ret;
	}

	if (IS_REACHABLE(CONFIG_PTP_1588_CLOCK))
		xgbe_ptp_register(pdata);

#if BRCM_FH
	/* Configure flexible header */
	xgbe_config_flex_header(pdata);
#endif

	xgbe_debugfs_init(pdata);

	netif_dbg(pdata, drv, pdata->netdev, "%u Tx software queues\n",
			pdata->tx_ring_count);
	netif_dbg(pdata, drv, pdata->netdev, "%u Rx software queues\n",
			pdata->rx_ring_count);

	return 0;
}

void xgbe_deconfig_netdev(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;

	xgbe_debugfs_exit(pdata);

	if (IS_REACHABLE(CONFIG_PTP_1588_CLOCK))
		xgbe_ptp_unregister(pdata);

	unregister_netdev(netdev);
}

static int xgbe_netdev_event(struct notifier_block *nb, unsigned long event,
		void *data)
{
	struct net_device *netdev = netdev_notifier_info_to_dev(data);
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	if (netdev->netdev_ops != xgbe_get_netdev_ops())
		goto out;

	switch (event) {
		case NETDEV_CHANGENAME:
			xgbe_debugfs_rename(pdata);
			break;

		default:
			break;
	}

out:
	return NOTIFY_DONE;
}

static struct notifier_block xgbe_netdev_notifier = {
	.notifier_call = xgbe_netdev_event,
};

static int __init xgbe_mod_init(void)
{
	int ret;

	ret = register_netdevice_notifier(&xgbe_netdev_notifier);
	if (ret)
		return ret;

	ret = xgbe_pci_init();
	if (ret)
		return ret;

	return 0;
}

static void __exit xgbe_mod_exit(void)
{
	xgbe_pci_exit();

	unregister_netdevice_notifier(&xgbe_netdev_notifier);
}

module_init(xgbe_mod_init);
module_exit(xgbe_mod_exit);

