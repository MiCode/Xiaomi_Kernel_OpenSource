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

#include "xgbe-usr-opt.h"

#if XGBE_SRIOV_PF
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/tcp.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <net/busy_poll.h>
#include <linux/clk.h>
#include <linux/if_ether.h>
#include <linux/net_tstamp.h>
#include <linux/phy.h>
#include <net/vxlan.h>

#include "xgbe.h"
#include "xgbe-common.h"
#endif
#include "xgbe-mbx.h"

#if XGBE_SRIOV_VF
static int check_status(struct xgbe_prv_data *pdata)
{
	volatile int stat = 1;
	unsigned int timeout = 1000;

	stat = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_FRM_PF0_VF);

	while (stat && timeout) {
		stat = MISC_IOREAD(pdata,
			XGMAC_PCIE_MISC_MAILBOX_INT_STA_FRM_PF0_VF);
		timeout--;
	}

	if (!timeout)
		TEST_PRNT("Previous Mailbox Cmd NOT completed !!!\n");

	return timeout;

}
#endif

#if XGBE_SRIOV_PF
static int xgbe_mbx_prep_data(struct mbox_msg *msg, unsigned int *data, unsigned int size)
{
	int i;
	for (i = 0; i < size; i++) {
		msg->data[i] = data[i];
	}
	return 0;
}

static int xgbe_mbx_read_pf(struct xgbe_prv_data *pdata, struct mbox_msg *msg, unsigned int vf_number)
{
	struct mbox_msg *recvd_msg = pdata->mbox_regs + (vf_number * MBX_READ_OFFSET);
	int i;

	if (recvd_msg->cmd == MBX_NO_CMD)
		return MBX_ERR;

	msg->cmd = recvd_msg->cmd;
	for (i = 0; i < MBX_DATA_SIZE; i++)
		msg->data[i] = recvd_msg->data[i];
	return 0;
}

static int xgbe_mbx_write_pf(struct xgbe_prv_data *pdata, struct mbox_msg *msg, unsigned int size, unsigned int vf_number)
{
	struct mbox_msg *reply_msg = pdata->mbox_regs + MBX_WRITE_START_OFFSET + (vf_number * MBX_WRITE_OFFSET);
	int i;
	
	for (i = 0; i < size; i++)
		reply_msg->data[i] = msg->data[i];

	reply_msg->cmd = msg->cmd;

	return 0;
}

static int xgbe_mbx_ptp_int_pf_vf(struct xgbe_prv_data *pdata, u64 tx_tstamp, unsigned int vf_number)
{
	struct mbox_msg *reply_msg = pdata->mbox_regs + MBX_WRITE_START_OFFSET + (vf_number * MBX_WRITE_OFFSET);
    reply_msg->data[0] = (tx_tstamp & 0xFFFFFFFF);
    reply_msg->data[1] = ((tx_tstamp >> 32) & 0xFFFFFFFF);
	reply_msg->cmd = MBX_PTP_PF_VF_INT;

	return 0;
}

void xgbe_init_function_ptrs_mbx(struct xgbe_mbx_if *mbx_if)
{
	mbx_if->read = xgbe_mbx_read_pf;
	mbx_if->write = xgbe_mbx_write_pf;
	mbx_if->prep_data = xgbe_mbx_prep_data;
    mbx_if->ptp_int_pf_vf = xgbe_mbx_ptp_int_pf_vf;
}
#endif

#if XGBE_SRIOV_VF

static void mbx_set_mac_addr(struct xgbe_prv_data *pdata, unsigned char *addr)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("---> set mac address mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* data */
	memcpy((unsigned char *)msg->data, addr, 6);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_SET_MAC_ADDR;

	TEST_PRNT("<--- set mac address mailbox option\n");

}

static void mbx_set_mc_mac_addr(struct xgbe_prv_data *pdata, unsigned char *addr)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("---> set mac address mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	if(addr)
		/* data */
		memcpy((unsigned char *)msg->data, addr, 6);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_SET_MULTICAST_MODE;

	TEST_PRNT("<--- set mac address mailbox option\n");

}

static void mbx_clr_mac_addr(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("---> clear mac address function\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_CLR_MAC_ADDR;
	TEST_PRNT("<--- clear mac address function\n");

}

static void mbx_vf_flr_notice(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_VF_FLR;
	TEST_PRNT("<--- FLR mailbox option\n");

}

irqreturn_t pf_mbx_isr(int irq, void *data)
{
	struct xgbe_prv_data *pdata = data;
	struct mbox_msg *msg;

	TEST_PRNT("---> pf_mbox_isr, Interrupt occured, irq_num = %d\n", irq);

	msg = (struct mbox_msg *)(pdata->mbox_regs+0x100);

	TEST_PRNT("PF to VF cmd = %x", msg->cmd);
	if(msg->cmd == MBX_PTP_PF_VF_INT) {
		pdata->tx_tstamp = msg->data[0];
		pdata->tx_tstamp |= ((u64)(msg->data[1]) << 32);
		queue_work(pdata->dev_workqueue, &pdata->tx_tstamp_work);
		TEST_PRNT("PF to VF PTP interrupt handled\n");
	}

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);

	TEST_PRNT("<--- pf_mbox_isr");
	return IRQ_HANDLED;
}

static int mbx_request_irq(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	int ret = 0;

	TEST_PRNT("---> mbox request irq\n");
	ret = devm_request_irq(pdata->dev, pdata->mbx_irq, pf_mbx_isr, 0,
			"MailBox", pdata);
	TEST_PRNT("mbx interrupt = %d", pdata->mbx_irq);
	if (ret)
		netdev_alert(netdev, "error requesting irq %d\n",
				pdata->mbx_irq);

	TEST_PRNT("<--- mbox request irq\n");
	return ret;
}

static int mbx_free_irq(struct xgbe_prv_data *pdata)
{
	TEST_PRNT("mbox free irq");
	devm_free_irq(pdata->dev, pdata->mbx_irq, pdata);
	TEST_PRNT(" freed %d irq", pdata->mbx_irq);
	return 0;
}


static void mbx_set_promiscuous_mode(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("---> set promiscuous mode mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_SET_PROMISCUOUS_MODE;

	TEST_PRNT("<--- set promiscuous mode mailbox option\n");
}

static void mbx_clear_promiscuous_mode(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("---> clear promiscuous mode mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_CLEAR_PROMISCUOUS_MODE;

	TEST_PRNT("<--- clear promiscuous mode mailbox option\n");
}

static void mbx_disable_rx_vlan_stripping(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("--->In disable RX vlan stripping mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_DISABLE_VLAN_STRIPPING;

	TEST_PRNT("<--- Disable RX vlan stripping mailbox option\n");
}

static void mbx_enable_rx_vlan_stripping(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("--->In enable RX vlan stripping mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_ENABLE_VLAN_STRIPPING;

	TEST_PRNT("<--- Enable RX vlan stripping mailbox option\n");

}

static void mbx_disable_rx_vlan_filtering(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("--->In disable RX vlan filtering mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_DISABLE_VLAN_FILTERING;

	TEST_PRNT("<--- Disable RX vlan filtering mailbox option\n");
}

static void mbx_enable_rx_vlan_filtering(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	TEST_PRNT("--->In enable RX vlan filtering mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_ENABLE_VLAN_FILTERING;

	TEST_PRNT("<--- Enable RX vlan filtering mailbox option\n");

}

static int mbx_rx_add_vlan_id(struct xgbe_prv_data *pdata, unsigned int vid)
{
	struct mbox_msg *msg;
	struct mbox_msg *recvd_msg = (struct mbox_msg *)(pdata->mbox_regs + 0x100);
	unsigned int timeout = 1000;
	int int_stat = 0, ret = 0;

	msg = (struct mbox_msg *)(pdata->mbox_regs);
	
	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_EN_VF, 0x0);	

	TEST_PRNT("---> Config RX VLAN ID mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* data */
	msg->data[0] = vid;

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_ADD_VF_VLAN_ID;

	while(int_stat != 1 && timeout != 0){
		int_stat = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 0x1;
		timeout--;
	}
	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);

	
	ret = recvd_msg->data[0];
	if(ret)
		ret = -1;
	
	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_EN_VF, 0x1);
	
	TEST_PRNT("<--- Config RX VLAN ID mailbox option\n");

	return ret;
}

static int mbx_rx_kill_vlan_id(struct xgbe_prv_data *pdata, unsigned int vid)
{
	struct mbox_msg *msg;
	struct mbox_msg *recvd_msg = (struct mbox_msg *)(pdata->mbox_regs + 0x100);
	unsigned int timeout = 1000;
	int int_stat = 0, ret = 0;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_EN_VF, 0x0);

	TEST_PRNT("---> Config RX VLAN ID mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* data */
	msg->data[0] = vid;

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_KILL_VF_VLAN_ID;

	while(int_stat != 1 && timeout != 0){
		int_stat = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 0x1;
		timeout--;
	}
	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);

	ret = recvd_msg->data[0];
	if(ret)
		ret = -1;

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_EN_VF, 0x1);

	TEST_PRNT("<--- Config RX VLAN ID mailbox option\n");
	return ret;

}

static void mbx_set_mc_addrs(struct xgbe_prv_data *pdata)
{
	struct net_device *netdev = pdata->netdev;
	struct netdev_hw_addr *ha;
	struct mbox_msg *msg = (struct mbox_msg *)(pdata->mbox_regs);
	struct mbox_msg_mc mc_msg;
	unsigned int count = 0;
	unsigned char msg_data[60];
	unsigned int timeout = 10000, int_stat = 0;
	unsigned int i = 0;

	pdata->mc_mutex = 1;

	memset((void *)&msg_data, 0, 60);
	memset((void *)&mc_msg, 0, sizeof(struct mbox_msg_mc));
	memset((void *)&msg->data[0], 0, (15 * sizeof(unsigned int)));
	
	check_status(pdata);

	mc_msg.sub_cmd = MBX_MULTICAST_SUB_NC;

	netdev_for_each_mc_addr(ha, netdev){
		if(ha)
			if(ha->addr){
				memcpy(&mc_msg.mc_addrs[i], ha->addr, ETH_ALEN);
		        count++;
            }
		i += 6;
		if(count == MAX_MC_VF){
			mc_msg.count = count;
			memcpy((void *)&msg_data[0], &mc_msg.count, 4);
			memcpy((void *)&msg_data[4], &mc_msg.sub_cmd, 2);
			memcpy((void *)&msg_data[6], &mc_msg.mc_addrs, 54);
			memcpy((void *)&msg->data[0], msg_data, 60);
			msg->cmd = MBX_SET_MULTICAST_MODE;

			while(int_stat != 1 && timeout != 0){
				int_stat = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 0x1;
				timeout--;
			}
			MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);
			mc_msg.sub_cmd = MBX_MULTICAST_SUB_C;
			count = 0;
			i = 0;
		}	
	}

    if(count != 0) {
		mc_msg.count = count;
		memcpy((void *)&msg_data[0], &mc_msg.count, 4);
		memcpy((void *)&msg_data[4], &mc_msg.sub_cmd, 2);
		memcpy((void *)&msg_data[6], &mc_msg.mc_addrs, 54);
		memcpy((void *)&msg->data[0], msg_data, 60);
		msg->cmd = MBX_SET_MULTICAST_MODE;

		while(int_stat != 1 && timeout != 0){
			int_stat = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 0x1;
			timeout--;
		}
		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);
	    }
	pdata->mc_mutex = 0;
}

static void mbx_get_hw_features(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg, *msg_vf;
	unsigned int timeout = 1000;
	unsigned int int_stat = 0;

	msg_vf = (struct mbox_msg *)(pdata->mbox_regs);
	msg = (struct mbox_msg *)(pdata->mbox_regs + 0x100);

	TEST_PRNT("---> Get hardware features mailbox option\n");

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg_vf->cmd = MBX_GET_HARDWARE_FEATURES;
	while (int_stat != 1 && timeout != 0) {
		int_stat = MISC_IOREAD(pdata,
			XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 1;
		timeout--;
	}

	/* check status of current mailbox request */
	if (int_stat) {
        pdata->mac_hfr0 = msg->data[0];
        pdata->mac_hfr1 = msg->data[1];
        pdata->mac_hfr2 = msg->data[2];
        pdata->mac_ver  = msg->data[3];
		xgbe_get_all_hw_features(pdata);
	}

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);
	TEST_PRNT("<--- Get hardware features mailbox option\n");
}

static void mbx_get_phy_link_status(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg, *msg_vf;
	unsigned int timeout = 1000;
	unsigned int int_stat = 0;

	msg_vf = (struct mbox_msg *)(pdata->mbox_regs);
	msg = (struct mbox_msg *)(pdata->mbox_regs + 0x100);

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg_vf->cmd = MBX_GET_LINK_STATUS;
	while (int_stat != 1 && timeout != 0) {
		int_stat = MISC_IOREAD(pdata,
			XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 1;
		timeout--;
	}

	/* check status of current mailbox request */
	if (int_stat) {
		pdata->phy_link = msg->data[0];
		pdata->phy.link = pdata->phy_link;
		if(pdata->phy_link){
			netif_carrier_on(pdata->netdev);
		} else {
			netif_carrier_off(pdata->netdev);
		}
	}

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);
}

static void mbx_get_phy_link_speed(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg, *msg_vf;
	unsigned int timeout = 1000;
	unsigned int int_stat = 0;
	unsigned int speed_status;

	msg_vf = (struct mbox_msg *)(pdata->mbox_regs);
	msg = (struct mbox_msg *)(pdata->mbox_regs + 0x100);

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg_vf->cmd = MBX_GET_LINK_SPEED;
	while (int_stat != 1 && timeout != 0) {
		int_stat = MISC_IOREAD(pdata,
			XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 1;
		timeout--;
	}

	/* check status of current mailbox request */
	if (int_stat) {
		speed_status = msg->data[0];
		if(pdata->dev_id == BCM8989X_VF_ID) {
			switch(speed_status)
			{
				case 0:	pdata->phy_speed = SPEED_10000;
					break;
				case 1:	pdata->phy_speed = SPEED_5000;
					break;
				case 2:	pdata->phy_speed = SPEED_2500;
					break;
				default:pdata->phy_speed = SPEED_UNKNOWN;
					break;
			}
			pdata->phy.speed = pdata->phy_speed;
		} else {
			pdata->phy_speed = SPEED_1000;
			pdata->phy.speed = speed_status;
		}
	}

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);
}

static void mbx_update_tstamp_addend(struct xgbe_prv_data *pdata, u32 addend)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	/* check status of pervious mailbox request */
	check_status(pdata);

	msg->data[0] = addend;

	TEST_PRNT("addend = %d\n", addend);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_UPDATE_TSTAMP_ADDEND;
}

static int mbx_config_tstamp(struct xgbe_prv_data *pdata, u32 mac_tscr, u32 mac_pto)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	/* check status of pervious mailbox request */
	check_status(pdata);

	msg->data[0] = mac_tscr;
	msg->data[1] = mac_pto;

	TEST_PRNT("mac_tscr= 0x%x\n", mac_tscr);
	TEST_PRNT("mac_pto = 0x%x\n", mac_pto);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_CONFIG_TSTAMP;
    return 0;
}

static void mbx_set_tstamp_time(struct xgbe_prv_data *pdata, unsigned int sec,
				 unsigned int nsec)
{
	struct mbox_msg *msg;

	msg = (struct mbox_msg *)(pdata->mbox_regs);

	/* check status of pervious mailbox request */
	check_status(pdata);

	msg->data[0] = sec;
	msg->data[1] = nsec;

	TEST_PRNT("nsec = %d\t sec = %d\n", sec, nsec);
	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_SET_TSTAMP_TIME;

}

static u64 mbx_get_tstamp_time(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg, *msg_reply;
	unsigned int timeout = 1000;
	unsigned int int_stat = 0;
	u64 nsec = 0;

	msg = (struct mbox_msg *)(pdata->mbox_regs);
	msg_reply = (struct mbox_msg *)(pdata->mbox_regs + 0x100);

	/* check status of pervious mailbox request */
	check_status(pdata);

	/* command, writing here causes Interrupt to PF */
	msg->cmd = MBX_GET_TSTAMP_TIME;
	while (int_stat != 1 && timeout != 0) {
		int_stat = MISC_IOREAD(pdata,
				XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF) & 1;
		timeout--;
	}

	/* check status of current mailbox request */
	if (int_stat) {
		memcpy(&nsec, msg_reply->data, 8);
		TEST_PRNT("nsec = %llx\n", nsec);
		return nsec;
	}

	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_VF, 1);
	return -1;
}

static void mbx_flush_vf_tx_queues(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;
	msg = (struct mbox_msg *)(pdata->mbox_regs);
	TEST_PRNT("---> flush vf tx queue mailbox option\n");
	check_status(pdata);
	msg->cmd = MBX_FLUSH_VF_TX_QUEUES;
	TEST_PRNT("<--- flush vf tx queue mailbox option\n");
}
static void mbx_enable_disable_jumbo_frame(struct xgbe_prv_data *pdata,
        unsigned int enable)
{
	struct mbox_msg *msg;
	msg = (struct mbox_msg *)(pdata->mbox_regs);
	TEST_PRNT("---> enable or disable jumbo frame mailbox option\n");
	check_status(pdata);
    msg->data[0] = enable;
	msg->cmd = MBX_EN_DIS_JUMBO_FRAME;
	TEST_PRNT("<--- enable or disable jumbo frame mailbox option\n");
}
static void mbx_enable_disable_rx_csum(struct xgbe_prv_data *pdata,
        unsigned int enable)
{
	struct mbox_msg *msg;
	msg = (struct mbox_msg *)(pdata->mbox_regs);
	TEST_PRNT("---> enable or disable rx checksum mailbox option\n");
	check_status(pdata);
    msg->data[0] = enable;
	msg->cmd = MBX_EN_DIS_RX_CSUM;
	TEST_PRNT("<--- enable or disable rx checksum frame mailbox option\n");
}
static void mbx_enable_vf_tx_queue(struct xgbe_prv_data *pdata)
{
	struct mbox_msg *msg;
	msg = (struct mbox_msg *)(pdata->mbox_regs);
	TEST_PRNT("--->In enable RX vlan stripping mailbox option\n");
	check_status(pdata);
	msg->cmd = MBX_EN_VF_TX_QUEUES;
	TEST_PRNT("<--- Enable RX vlan stripping mailbox option\n");
}
void xgbe_init_function_ptrs_mbx(struct xgbe_mbx *mbx)
{

	mbx->request_irq = mbx_request_irq;
	mbx->free_irq = mbx_free_irq;
	mbx->set_mac_addr = mbx_set_mac_addr;
	mbx->set_mc_mac_addr = mbx_set_mc_mac_addr;
	mbx->clr_mac_addr = mbx_clr_mac_addr;
	mbx->vf_flr_notice = mbx_vf_flr_notice;
	mbx->get_hw_features = mbx_get_hw_features;
	mbx->set_promiscuous_mode = mbx_set_promiscuous_mode;
	mbx->clear_promiscuous_mode = mbx_clear_promiscuous_mode;
	mbx->enable_rx_vlan_stripping = mbx_enable_rx_vlan_stripping;
	mbx->disable_rx_vlan_stripping = mbx_disable_rx_vlan_stripping;
	mbx->enable_rx_vlan_filtering = mbx_enable_rx_vlan_filtering;
	mbx->disable_rx_vlan_filtering = mbx_disable_rx_vlan_filtering;
	mbx->rx_add_vlan_id = mbx_rx_add_vlan_id;
	mbx->rx_kill_vlan_id = mbx_rx_kill_vlan_id;
	mbx->check_status = check_status;
	mbx->config_tstamp = mbx_config_tstamp;
	mbx->update_tstamp_addend = mbx_update_tstamp_addend;
	mbx->set_tstamp_time = mbx_set_tstamp_time;
	mbx->get_tstamp_time = mbx_get_tstamp_time;
	mbx->xgbe_flush_vf_tx_queues    = mbx_flush_vf_tx_queues;
	mbx->enable_disable_jumbo_frame = mbx_enable_disable_jumbo_frame;
	mbx->enable_disable_rx_csum     = mbx_enable_disable_rx_csum;
	mbx->enable_vf_tx_queue         = mbx_enable_vf_tx_queue; 
	mbx->set_mc_addrs    = mbx_set_mc_addrs;
	mbx->get_phy_link_status = mbx_get_phy_link_status;
	mbx->get_phy_link_speed = mbx_get_phy_link_speed;

}
#endif
