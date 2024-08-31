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
#include <linux/pci.h>
#include <linux/log2.h>
#include <linux/bitrev.h>
#include <linux/crc32.h>
#include "xgbe.h"
#include "xgbe-common.h"
#include "xgbe-mbx.h"

#if XGBE_SRIOV_PF

extern unsigned int max_vfs;
#ifdef CONFIG_PCI_IOV
char vf_irq_name[16];
static int xgbe_get_vfs(struct xgbe_prv_data *pdata)
{
	struct pci_dev *pdev = pdata->pcidev;
	u16 vendor = pdev->vendor;
	struct pci_dev *vfdev;
	int vf = 0;
	u16 vf_id;
	int pos;
	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos)
		return 1;
	pci_read_config_word(pdev, pos + PCI_SRIOV_VF_DID, &vf_id);

	vfdev = pci_get_device(vendor, vf_id, NULL);
	for (; vfdev; vfdev = pci_get_device(vendor, vf_id, vfdev)) {
		if (!vfdev->is_virtfn)
			continue;
		if (vfdev->physfn != pdev)
			continue;
		if (vf >= pdata->num_vfs)
			continue;
		pci_dev_get(vfdev);
		pdata->vfinfo[vf].vfdev = vfdev;
		++vf;
	}

	return 0;
}

static int __xgbe_enable_sriov(struct xgbe_prv_data *pdata,
		unsigned int num_vfs)
{
	struct pci_dev *pdev = pdata->pcidev;
	struct device *dev = &pdev->dev;
	struct vf_data_storage *vfinfo;
	int i;

	/* Enable VMDq flag so device will be set in VM mode */
	pdata->flags |= XGBE_FLAG_SRIOV_ENABLED |
		XGBE_FLAG_VMDQ_ENABLED;

	/* Allocate memory for per VF control structures */
	pdata->vfinfo = kcalloc(num_vfs, sizeof(struct vf_data_storage),
			GFP_KERNEL);
	if (!pdata->vfinfo)
		return -ENOMEM;

	vfinfo = pdata->vfinfo;

	pdata->num_vfs = num_vfs;


	for (i = 0; i < num_vfs; i++) {
		vfinfo[i].num_dma_channels = MISC_IOREAD(pdata, XGMAC_MISC_FUNC_RESOURCES_PF0);
		vfinfo[i].num_dma_channels &= 0xF;
		vfinfo[i].num_tx_queues = vfinfo[i].num_dma_channels;
		vfinfo[i].tx_fifo_size = pdata->tx_fifo_size_vfs / max_vfs;
		vfinfo[i].vf_num = i + 1;
		eth_zero_addr(vfinfo[i].vf_mac_addresses);
		vfinfo[i].mac_address_offset =  MAC_MACA0HR + (i + 1) * 8 * vfinfo[i].num_dma_channels;
		vfinfo[i].dma_channel_start_num = (i + 1) * vfinfo[i].num_dma_channels;
		vfinfo[i].last_used_dma = vfinfo[i].dma_channel_start_num;
		vfinfo[i].vlan_filters_used = 0;
		vfinfo[i].promisc = 0;
		vfinfo[i].pm_mode = 0;
		vfinfo[i].enabled = 0;
		vfinfo[i].ptp_enabled = 1;
	}

	dev_dbg(dev, "SR-IOV enabled with %d VFs\n", num_vfs);
	return 0;
}

int xgbe_enable_sriov(struct xgbe_prv_data *pdata, unsigned int max_vfs)
{
	int pre_existing_vfs = 0;
	unsigned int num_vfs;
	struct pci_dev *pdev = pdata->pcidev;
	struct device *dev = &pdev->dev;

	pre_existing_vfs = pci_num_vf(pdev);
	if (!pre_existing_vfs && !max_vfs)
		return 1;

	/* If there are pre-existing VFs then we have to force
	 * use of that many - over ride any module parameter value.
	 * This may result from the user unloading the PF driver
	 * while VFs were assigned to guest VMs or because the VFs
	 * have been created via the new PCI SR-IOV sysfs interface.
	 */
	if (pre_existing_vfs) {
		num_vfs = pre_existing_vfs;
		dev_err(dev,
				"Virtual Functions already enabled for this device - Please reload all VF drivers to avoid spoofed packet errors\n");
	} else {
		int err;
		num_vfs = min_t(unsigned int, max_vfs, XGBE_MAX_VFS_DRV_LIMIT);
		err = pci_enable_sriov(pdev, num_vfs);
		if (err) {
			dev_err(dev, "Failed to enable PCI sriov: %d\n", err);
			return 1;
		}
	}

	if (!__xgbe_enable_sriov(pdata, num_vfs)) {
		if(xgbe_get_vfs(pdata)) {
			return 1;
		}
		return 0;
	}

	/* If we have gotten to this point then there is no memory available
	 * to manage the VF devices - print message and bail.
	 */
	dev_err(dev, "Unable to allocate memory for VF Data Storage - "
			"SRIOV disabled\n");
	xgbe_disable_sriov(pdata);
	return 0;
}
#endif /* #ifdef CONFIG_PCI_IOV */

int xgbe_disable_sriov(struct xgbe_prv_data *pdata)
{
	unsigned int num_vfs = pdata->num_vfs, vf;
#ifdef CONFIG_PCI_IOV
	struct pci_dev *pdev = pdata->pcidev;
	struct device *dev = &pdev->dev;
#endif
	/* set num VFs to 0 to prevent access to vfinfo */
	pdata->num_vfs = 0;

	/* put the reference to all of the vf devices */
	for (vf = 0; vf < num_vfs; ++vf) {
		struct pci_dev *vfdev = pdata->vfinfo[vf].vfdev;

		if (!vfdev)
			continue;
		pdata->vfinfo[vf].vfdev = NULL;
		pci_dev_put(vfdev);
	}

	/* free VF control structures */
	kfree(pdata->vfinfo);
	pdata->vfinfo = NULL;

	/* if SR-IOV is already disabled then there is nothing to do */
	if (!(pdata->flags & XGBE_FLAG_SRIOV_ENABLED))
		return 0;

#ifdef CONFIG_PCI_IOV
	/*
	 * If our VFs are assigned we cannot shut down SR-IOV
	 * without causing issues, so just leave the hardware
	 * available but disabled
	 */
	if (pci_vfs_assigned(pdata->pcidev)) {
		dev_dbg(dev, "Unloading driver while VFs are assigned - VFs will not be deallocated\n");
		return -EPERM;
	}
	/* disable iov and allow time for transactions to clear */
	pci_disable_sriov(pdata->pcidev);
#endif

	pdata->flags &= (~(XGBE_FLAG_SRIOV_ENABLED));

	/* take a breather then clean up driver data */
	XGBE_MSLEEP(100);
	return 0;
}

int xgbe_vf_configuration(struct pci_dev *pdev, unsigned int event_mask)
{
	struct xgbe_prv_data *pdata = pci_get_drvdata(pdev);
	unsigned int vfn = (event_mask & 0x3);
	bool enable = ((event_mask & 0x10000000U) != 0);

	if (enable)
		memset(pdata->vfinfo[vfn].vf_mac_addresses, vfn, 6);

	return 0;
}

static int xgbe_pci_sriov_enable(struct pci_dev *pcidev, int num_vfs)
{
#ifdef CONFIG_PCI_IOV
	struct xgbe_prv_data *pdata = pci_get_drvdata(pcidev);
	int pre_existing_vfs = pci_num_vf(pcidev);
	int err = 0, i;
	struct device *dev = &pcidev->dev;

	if (pre_existing_vfs && pre_existing_vfs != num_vfs)
		err = xgbe_disable_sriov(pdata);
	else if (pre_existing_vfs && pre_existing_vfs == num_vfs)
		return num_vfs;

	if (err)
		return err;

	err = __xgbe_enable_sriov(pdata, num_vfs);
	if (err)
		return  err;

	for (i = 0; i < num_vfs; i++)
		xgbe_vf_configuration(pcidev, (i | 0x10000000));

	err = pci_enable_sriov(pcidev, num_vfs);
	if (err) {
		dev_err(dev, "Failed to enable PCI sriov: %d\n", err);
		return err;
	}
	xgbe_get_vfs(pdata);

	return num_vfs;
#else
	return 0;
#endif
}

static int xgbe_pci_sriov_disable(struct pci_dev *dev)
{
	struct xgbe_prv_data *pdata = pci_get_drvdata(dev);
	int err;

	err = xgbe_disable_sriov(pdata);

	return err;
}

int xgbe_pci_sriov_configure(struct pci_dev *dev, int num_vfs)
{
	if (num_vfs == 0)
		return xgbe_pci_sriov_disable(dev);
	else
		return xgbe_pci_sriov_enable(dev, num_vfs);
}

#ifdef CONFIG_PCI_IOV
static int set_vf_mac(struct xgbe_prv_data *pdata, u8 *new_mac, int vf)
{
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf];

	hw_if->set_mac_address_sriov(pdata, new_mac, vf);
	eli_if->mac_add(pdata, vf + 1, new_mac);

	vfinfo->enabled = 1;

	return 0;
}

static int set_vf_mac_address(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf_number];
	u8 *new_mac = ((u8 *) (&msg->data[0]));

	if (!is_valid_ether_addr(new_mac))
		return -1;

	memcpy((unsigned char *)vfinfo->vf_mac_addresses, (unsigned char *)new_mac, ETH_ALEN); 

	return set_vf_mac(pdata, new_mac, vf_number);
}

static int set_vf_promiscuous_mode(struct xgbe_prv_data *pdata, int vf_number)
{
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct vf_data_storage *vfinfo = NULL;
	int i;

	eli_if->set_promiscuous_mode(pdata, (vf_number + 1), 1);

	eli_if->set_promiscuous_mode(pdata, ELI_FUNC_PF, pdata->promisc);

	for(i = 0; i < max_vfs; i++){
		if(i == vf_number)
			continue;

		vfinfo = &pdata->vfinfo[i];
		if(!vfinfo)
			continue;

		eli_if->set_promiscuous_mode(pdata, (i + 1), vfinfo->promisc);
	}
	return 0;
}

static int clear_vf_promiscuous_mode(struct xgbe_prv_data *pdata, int vf_number)
{
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct vf_data_storage *vfinfo = NULL;
	int i;

	eli_if->set_promiscuous_mode(pdata, (vf_number + 1), 0);

	eli_if->set_promiscuous_mode(pdata, ELI_FUNC_PF, pdata->promisc);

	for(i = 0; i < max_vfs; i++){
		if(i == vf_number)
			continue;

		vfinfo = &pdata->vfinfo[i];
		if(!vfinfo)
			continue;

		eli_if->set_promiscuous_mode(pdata, (i + 1), vfinfo->promisc);
	}

	return 0;
}

static int set_vf_mc_mac_address(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{

	struct mbox_msg_mc *mc_data = (struct mbox_msg_mc *)msg->data;
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf_number];
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	unsigned int recvd_count = mc_data->count, i, offset = -1, failed_count = 0;
	unsigned int hash_table_shift, hash_table_count;
	unsigned char da[6] = {0, 0, 0, 0, 0, 0};
	u32 crc;

	if(!recvd_count){
		return 0;
	}

	if(recvd_count > MAX_MC_VF){
		ELI_PRNT("Invalid number of addresses received\n");
		return -1;
	}

	hash_table_shift = 26 - (pdata->hw_feat.hash_table_size >> 7);
	hash_table_count = pdata->hw_feat.hash_table_size / 32;

	if(mc_data->sub_cmd == MBX_MULTICAST_SUB_NC) {
		if(vfinfo->pm_mode)
			eli_if->set_all_multicast_mode(pdata, (vf_number + 1), 0);

		for_each_set_bit(offset, vfinfo->mc_indices, (ELI_MAC_INDEX + 1)){
			eli_if->mc_mac_del(pdata, offset, (vf_number + 1));
		}

		memset(vfinfo->hash_table, 0, sizeof(vfinfo->hash_table));    
	}


	for (i = 0; i < 54;) {
		crc = bitrev32(~crc32_le(~0, &mc_data->mc_addrs[i], ETH_ALEN));
		crc >>= hash_table_shift;
		vfinfo->hash_table[crc >> 5] |= (1 << (crc & 0x1f));

        if(!strcmp(da, &mc_data->mc_addrs[i]))
            goto increment;

		if(eli_if->mc_mac_add(pdata, &mc_data->mc_addrs[i], (vf_number + 1)) == -1)
			failed_count++;
increment:
		i += 6;
	}

	if(failed_count)
		eli_if->set_all_multicast_mode(pdata, (vf_number + 1), 1);

	hw_if->set_mac_hash_table_vf(pdata);

	return 0;
}

static int reset_vf(struct xgbe_prv_data *pdata, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	struct vf_data_storage *vfinfo = &pdata->vfinfo[vf_number];
	unsigned int vid, offset = -1;

	for_each_set_bit (vid, vfinfo->active_vlans, VLAN_N_VID) {
		if (!eli_if->kill_vlan(pdata, vid, vf_number)) {
			clear_bit(vid, pdata->active_vf_vlans);
			clear_bit(vid, pdata->vfinfo[vf_number].active_vlans);
			hw_if->update_vlan_hash_table(pdata);    
		}
	}

	if(vfinfo->pm_mode)
		eli_if->set_all_multicast_mode(pdata, (vf_number + 1), vfinfo->pm_mode);

	for_each_set_bit(offset, vfinfo->mc_indices, (ELI_MAC_INDEX + 1)){
		eli_if->mc_mac_del(pdata, offset, (vf_number + 1));
	}
	hw_if->clear_mac_address_sriov(pdata, vf_number);

	vfinfo->enabled = 0;

	return 0;
}

static int clear_vf_mac_address(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	eli_if->mac_del(pdata, vf_number + 1);
	return 0;
}

static int add_vf_vlan(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	unsigned int vid;

	vid = msg->data[0];

	if (!eli_if->add_vlan(pdata, vid, vf_number)) {
		set_bit(vid, pdata->active_vf_vlans);
		set_bit(vid, pdata->vfinfo[vf_number].active_vlans);
		hw_if->update_vlan_hash_table(pdata);
		return 0;
	}

	return 1;
}

static int kill_vf_vlan(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	struct xgbe_eli_if *eli_if = &pdata->eli_if;
	unsigned int vid;

	vid = msg->data[0];
	if (!eli_if->kill_vlan(pdata, vid, vf_number)) {
		clear_bit(vid, pdata->active_vf_vlans);
		clear_bit(vid, pdata->vfinfo[vf_number].active_vlans);
		hw_if->update_vlan_hash_table(pdata);
		return 0;
	}

	return 1;
}

static int enable_vf_vlan_stripping(struct xgbe_prv_data *pdata, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	pdata->vfinfo[vf_number].netdev_features |= NETIF_F_HW_VLAN_CTAG_RX;

	hw_if->enable_rx_vlan_stripping(pdata);

	return 0;
}

static int disable_vf_vlan_stripping(struct xgbe_prv_data *pdata, int vf_number)
{
	int i;

	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	if (pdata->netdev->features & NETIF_F_HW_VLAN_CTAG_RX) {
		TEST_PRNT("PF is making use of VLAN stripping functionality\n");
		pdata->vfinfo[vf_number].netdev_features &= ~(NETIF_F_HW_VLAN_CTAG_RX);
		return 0;
	}
	for (i = 0; i < max_vfs; i++) {
		if (pdata->vfinfo[vf_number].netdev_features & NETIF_F_HW_VLAN_CTAG_RX) {
			TEST_PRNT("One or more VFs making use of VLAN stripping functionality\n");
			pdata->vfinfo[vf_number].netdev_features &= ~(NETIF_F_HW_VLAN_CTAG_RX);
			return 0;
		}
	}

	hw_if->disable_rx_vlan_stripping(pdata);

	return 0;
}

static int enable_vf_vlan_filtering(struct xgbe_prv_data *pdata, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	pdata->vfinfo[vf_number].netdev_features |= NETIF_F_HW_VLAN_CTAG_FILTER;

	hw_if->enable_rx_vlan_filtering(pdata);

	return 0;
}

static int disable_vf_vlan_filtering(struct xgbe_prv_data *pdata, int vf_number)
{
	int i;

	struct xgbe_hw_if *hw_if = &pdata->hw_if;


	if (pdata->netdev->features & NETIF_F_HW_VLAN_CTAG_FILTER) {
		TEST_PRNT("PF is making use of VLAN filtering functionality\n");
		pdata->vfinfo[vf_number].netdev_features &= ~(NETIF_F_HW_VLAN_CTAG_FILTER);
		return 0;
	}
	for (i = 0; i < max_vfs; i++) {
		if (pdata->vfinfo[vf_number].netdev_features & NETIF_F_HW_VLAN_CTAG_FILTER) {
			TEST_PRNT("One or more VFs making use of VLAN filtering functionality\n");
			pdata->vfinfo[vf_number].netdev_features &= ~(NETIF_F_HW_VLAN_CTAG_FILTER);
			return 0;
		}
	}

	hw_if->disable_rx_vlan_filtering(pdata);

	return 0;
}

static int config_jumbo_enable_vf(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	int enable;
	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	enable = msg->data[0];
	hw_if->enable_jumbo_frame(pdata, (vf_number+1), enable);

	return 0;
}

static int config_rx_csum(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	int enable;
	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	enable = msg->data[0];

	if(enable){
		hw_if->enable_rx_csum(pdata);
		pdata->rx_csum_semaphore++;
	}else{
		pdata->rx_csum_semaphore--;
		hw_if->disable_rx_csum(pdata);
	}
	return 0;
}

static int flush_vf_tx_queues(struct xgbe_prv_data *pdata, struct mbox_msg *msg, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	hw_if->flush_tx_queues_vf(pdata, vf_number);

	return 0;
}

static int enable_vf_tx_queues(struct xgbe_prv_data *pdata, int vf_number)
{
	struct xgbe_hw_if *hw_if = &pdata->hw_if;

	hw_if->enable_vf_tx_queue(pdata, vf_number);

	return 0;
}

static int vf_mbox_task(int vf_id, void *data)
{
	struct xgbe_prv_data *pdata = data;
	struct xgbe_mbx_if *mbx_if = &pdata->mbx_if;
	unsigned int data_reply[MBX_DATA_SIZE];
	int ret;
	u64 nsec;
	struct mbox_msg msg, msg_reply;

	ret = mbx_if->read(pdata, &msg, vf_id);
	if (ret)
		return ret;

	switch (msg.cmd) {
		case MBX_SET_MAC_ADDR:
			ret = set_vf_mac_address(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_CLR_MAC_ADDR:
			clear_vf_mac_address(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_VF_FLR:
			reset_vf(pdata, vf_id);
			break;

		case MBX_SET_PROMISCUOUS_MODE:
			set_vf_promiscuous_mode(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_CLEAR_PROMISCUOUS_MODE:
			clear_vf_promiscuous_mode(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_SET_MULTICAST_MODE:
			ret = set_vf_mc_mac_address(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_GET_HARDWARE_FEATURES:
			data_reply[0] = XGMAC_IOREAD(pdata, MAC_HWF0R);
			data_reply[1] = XGMAC_IOREAD(pdata, MAC_HWF1R);
			data_reply[2] = XGMAC_IOREAD(pdata, MAC_HWF2R);
			data_reply[3] = XGMAC_IOREAD(pdata, MAC_VR);
			mbx_if->prep_data(&msg_reply, data_reply, 3);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 4, vf_id);
			break;

		case MBX_ADD_VF_VLAN_ID:
			ret = add_vf_vlan(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			msg_reply.data[0] = ret;
			mbx_if->write(pdata, &msg_reply, 1, vf_id);
			break;

		case MBX_KILL_VF_VLAN_ID:
			ret = kill_vf_vlan(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			msg_reply.data[0] = ret;
			mbx_if->write(pdata, &msg_reply, 1, vf_id);
			break;

		case MBX_ENABLE_VLAN_STRIPPING:
			enable_vf_vlan_stripping(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_DISABLE_VLAN_STRIPPING:
			disable_vf_vlan_stripping(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_ENABLE_VLAN_FILTERING:
			enable_vf_vlan_filtering(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_DISABLE_VLAN_FILTERING:
			disable_vf_vlan_filtering(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_GET_TSTAMP_TIME:
			nsec = pdata->hw_if.get_tstamp_time(pdata);
			memcpy((unsigned char *)msg_reply.data, (unsigned char *)&nsec, 8);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 2, vf_id);
			break;

        case MBX_CONFIG_TSTAMP:
			pdata->hw_if.config_tstamp(pdata, (u32)(msg.data[0]));
            /* Enabling PTP offloading*/
	        XGMAC_IOWRITE(pdata, MAC_PTO, (u32)(msg.data[0])); 
            pdata->vfinfo[vf_id].ptp_enabled  = 1;
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_UPDATE_TSTAMP_ADDEND:
			pdata->hw_if.update_tstamp_addend(pdata, (u32)(msg.data[0]));
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_SET_TSTAMP_TIME:
			pdata->hw_if.set_tstamp_time(pdata, (u32)msg.data[0], (u32)msg.data[1]);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_FLUSH_VF_TX_QUEUES:
			flush_vf_tx_queues(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_EN_VF_TX_QUEUES:
			enable_vf_tx_queues(pdata, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_EN_DIS_JUMBO_FRAME:
			config_jumbo_enable_vf(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id); 
			break;

		case MBX_EN_DIS_RX_CSUM:
			config_rx_csum(pdata, &msg, vf_id);
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 0, vf_id);
			break;

		case MBX_GET_LINK_STATUS:
			//pdata->phy_if.phy_impl.link_status(pdata, NULL);
			msg_reply.data[0] = pdata->phy.link;
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 1, vf_id);
			break;

		case MBX_GET_LINK_SPEED:
			//msg_replay.data[0] = pdata->phy_if.phy_impl.get_mode(pdata, 0);
			msg_reply.data[0] =  pdata->phy.speed;
			msg_reply.cmd = msg.cmd;
			mbx_if->write(pdata, &msg_reply, 1, vf_id);
			break;

		default:
			ret = MBX_ERR;
			break;
	}
	return ret;
}

static irqreturn_t vf_mbox_isr(int irq, void *data)
{
	unsigned int stat, i;
	struct xgbe_prv_data *pdata = data;

	stat = MISC_IOREAD(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_PF0);
	for (i = 0; i < max_vfs; i++) {
		if (irq == pdata->vf_mbox_irq[i]) {
			MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_STA_PF0, (1 << i));
			break;
		}
	}

	if (0 == (stat & (1 << i))) {
		return IRQ_HANDLED;
	}

	if (i < max_vfs) {
		if (vf_mbox_task(i, data)) {
			dev_err(pdata->dev, "Error while handling mailbox message from VF%d\n", i);
		}
	}

	return IRQ_HANDLED;
}

int mbox_request_irqs(struct xgbe_prv_data *pdata)
{
	int ret, i;
	struct net_device *netdev = pdata->netdev;

	for (i = 0; i < max_vfs; i++) {
		snprintf(vf_irq_name, 15, "VF%1d mbox intr", i);
		ret = devm_request_irq(pdata->dev, pdata->vf_mbox_irq[i], vf_mbox_isr,
				0, vf_irq_name, pdata);
		if (ret) {
			netdev_alert(netdev, "error requesting vf1 mbox irq %d\n",
					pdata->vf_mbox_irq[i]);
		}
	}
	return 0;
}
#endif
void mbox_free_irqs(struct xgbe_prv_data *pdata)
{
	int i;

	for (i = 0; i < max_vfs; i++) {
		devm_free_irq(pdata->dev, pdata->vf_mbox_irq[i], pdata);
	}
}
#endif
