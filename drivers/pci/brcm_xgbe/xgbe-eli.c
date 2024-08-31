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
#include <linux/delay.h>

#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/net_tstamp.h>

#include "xgbe.h"
#include "xgbe-common.h"

#if XGBE_SRIOV_PF
#if ELI_ENABLE
extern int max_vfs;

static int get_mac_index(struct xgbe_prv_data *pdata, int inx_type, int init_req)
{
	int i = 0, inx = 0, val = 0, limit = 0;

	if(inx_type == MAC_INX_VLAN){
		limit = (ELI_MAC_INDEX - max_vfs);
		i =	GET_LIMIT(max_vfs); 
	} else {
		limit = GET_LIMIT(max_vfs); 
		i = 0;
	}

	if(init_req != UC_MAC){
		if(init_req == IPV4_MC_MAC)
			limit = limit - 2;
		if(init_req == IPV6_MC_MAC)
			limit = limit - 1;

		return limit;	
	}
	if(inx_type == MAC_INX_OTHER)
		limit -= 2;

	for(;i < limit; i++){ 

		if(i >= 32)
			inx = 1; 

		val = (inx ? (i - 32) : i);
		if(!((pdata->occupied_eli[inx] >> val) & 0x1)){
			return i;
		}
	}

	return -1;
}

static int str_cmp(unsigned char *da, unsigned char *check_da, int len)
{
	int i;

	for(i = 0; i < len; i++){

		if(da[i] != check_da[i])
			return 1;
	}

	return 0;
}

static int eli_tcam_ram_read(struct xgbe_prv_data *pdata, int offset, char *da, int* ivt, int* ovt)
{
	unsigned int wdata = 0, data[3];
	unsigned long long mac1, mac2;
	int i = 0,vlan;
	unsigned char *ptr;

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_READ);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);
	ELI_IOWRITE(pdata, TCAM_ACC, wdata);

	while(!ELI_IOREAD_BITS(pdata, TCAM_ACC, RD_STS));

	data[0] = ELI_IOREAD(pdata, TCAM_DATA0);
	data[1] = ELI_IOREAD(pdata, TCAM_DATA1);
	data[2] = ELI_IOREAD(pdata, TCAM_DATA2);


	vlan = ((data[2] << 2) | ((data[1] >> 30) & 0x3));
	*ivt = vlan & 0xFFF;
	*ovt = ((vlan >> 12) & 0xFFF);

	mac1 = (data[0] & TCAM_DATA0_DA_MASK) >> 2;
	mac2 = (data[1] & TCAM_DATA1_DA_MASK);
	mac2 <<= 30;

	mac2 |= mac1;

	if(mac2){
		ptr = (unsigned char *)&mac2;

		for(i = 5; i >=0; i--){
			da[i] = ptr[5 - i];
		}

	}

	return 0;
}

static int eli_tcam_ram_compare(struct xgbe_prv_data *pdata, char *da, int offset)
{
	unsigned long wdata = 0, data[3];
	unsigned long long mac1, mac2;
	int i = 0;
	unsigned char *ptr;
	unsigned char check_da[6]; 

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_READ);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);
	ELI_IOWRITE(pdata, TCAM_ACC, wdata);

	while(!ELI_IOREAD_BITS(pdata, TCAM_ACC, RD_STS));

	data[0] = ELI_IOREAD(pdata, TCAM_DATA0);
	data[1] = ELI_IOREAD(pdata, TCAM_DATA1); 

	mac1 = (data[0] & TCAM_DATA0_DA_MASK) >> 2;
	mac2 = (data[1] & TCAM_DATA1_DA_MASK);
	mac2 <<= 30;

	mac2 |= mac1;

	if(mac2){
		ptr = (unsigned char *)&mac2;

		for(i = 5; i >=0; i--){
			check_da[i] = ptr[5 - i];
		}

		if(!(str_cmp(da, check_da, 6)))
			return 1;
	}

	return 0;
}

static int eli_tcam_search(struct xgbe_prv_data *pdata, unsigned char *da, int ivt, int ovt, int
		da_type)
{
	unsigned long data[3];
	unsigned long long mac1 = 0, mac2 = 0;
	int i, vlan, offset = 0;
	unsigned int wdata = 0; 
	unsigned char *ptr;
	unsigned char check_da[6];

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, 0);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_SEARCH);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);

	ELI_IOWRITE(pdata, TCAM_ACC, wdata);

	do {

		offset = ELI_IOREAD_BITS(pdata, TCAM_ACC, XCESS_ADDR);

		data[0] = ELI_IOREAD(pdata, TCAM_DATA0);
		data[1] = ELI_IOREAD(pdata, TCAM_DATA1); 

		mac1 = (data[0] & TCAM_DATA0_DA_MASK) >> 2;
		mac2 = (data[1] & TCAM_DATA1_DA_MASK);
		mac2 <<= 30;

		mac2 |= mac1;

		if(mac2){
			ptr = (unsigned char *)&mac2;

			for(i = 5; i >=0; i--){
				check_da[i] = ptr[5 - i];
			}
		}

		data[2] = ELI_IOREAD(pdata, TCAM_DATA2);
		vlan = ((data[2] << 2) | ((data[1] >> 30) & 0x3));

		if((!str_cmp(da, check_da, 6)) && (ovt == vlan)){
			return offset;
		}
	} while(ELI_IOREAD_BITS(pdata, TCAM_ACC, OP_STR_DONE));

	return -1;
} 

static int eli_tcam_reset(struct xgbe_prv_data *pdata, int ram_sel)
{
	unsigned long timeout=100; 

	if(ram_sel == ELI_TCAM_RAM) {
		ELI_IOWRITE_BITS(pdata, TCAM_ACC, TCAM_RST, 1);
		while(timeout && ELI_IOREAD_BITS(pdata, TCAM_ACC, TCAM_RST))
		{
			msleep_interruptible(1);
			timeout--;
		}
	}
	else if(ram_sel == ELI_TCAM_FUNC_LOOKUP_RAM || ram_sel == ELI_TCAM_DMA_RAM) {
		ELI_IOWRITE_BITS(pdata, TCAM_ACC, RAM_CLEAR, 1);
		while(timeout && ELI_IOREAD_BITS(pdata, TCAM_ACC, RAM_CLEAR))
		{
			msleep_interruptible(1);
			timeout--;
		}

	}
	else {
		ELI_PRNT("Invalid ram_sel");
	}

	if(timeout==0)
		ELI_PRNT("Trouble with TCAM reset");

	return 0;
}

static int eli_tcam_write(struct xgbe_prv_data *pdata, int offset, unsigned char da[6], int ivt, 
		int ovt, int type)
{
	unsigned long mask[3], data[3];
	unsigned int wdata = 0; 

	mask[0] = 0xffffffff;
	mask[1] = 0x0003ffff;
	mask[2] = 0x00000000;

	if(ovt) {
		mask[1] = 0xc003ffff;
		mask[2] = 0x3ff;
	}
	if(ivt) {
		mask[1] = 0xffffffff;
	}

	if(type != UC_MAC){
		mask[2] = 0x0;
		mask[1] = (1 << 10);

		mask[0] = 0x3;
	} 

	data[0] = da[5]|(da[4]<<8)|(da[3]<<16)|(da[2]<<24);
	data[0] <<= 2;

	data[0] |= 3;

	data[1] = da[1]|(da[0]<<8);
	data[1] <<= 2;
	data[1] |= (da[2]>>6);

	if(ivt)
		data[1] |= (ivt<<18);
	if(ovt)
		data[1] |= ((ovt & 0x3)<<30);

	data[2] = 0;
	if(ovt)
		data[2] = (ovt>>2);

	if((data[0] == 3) && (data[1] == 0) && (data[2] == 0)){
		data[0] = 0;
	}

	ELI_IOWRITE(pdata, TCAM_DATA0, data[0] );
	ELI_IOWRITE(pdata, TCAM_DATA1, data[1] );
	ELI_IOWRITE(pdata, TCAM_DATA2, data[2] );


	ELI_IOWRITE(pdata, TCAM_MASK0, mask[0] );
	ELI_IOWRITE(pdata, TCAM_MASK1, mask[1] );
	ELI_IOWRITE(pdata, TCAM_MASK2, mask[2] );

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_WRITE);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);

	ELI_IOWRITE(pdata, TCAM_ACC, wdata); 

	return 0;
}

static int eli_func_ram_write(struct xgbe_prv_data *pdata, int func_ram_offset, int func_num, char pri)
{
	unsigned long wdata = 0;

	ELI_SET_BITS(wdata, TCAM_FUNC_PRIO_LOOKUP, FUNC, func_num);
	ELI_SET_BITS(wdata, TCAM_FUNC_PRIO_LOOKUP, PRI, pri);
	ELI_IOWRITE(pdata, TCAM_FUNC_PRIO_LOOKUP, wdata);

	wdata = 0;

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_FUNC_LOOKUP_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, func_ram_offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_WRITE);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);

	ELI_IOWRITE(pdata, TCAM_ACC, wdata);

	wdata = 0;

	return 0;
}

int eli_func_ram_read(struct xgbe_prv_data *pdata, int func_ram_offset)
{
	unsigned long wdata = 0;
	int f_num_lkup = 0;

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_FUNC_LOOKUP_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, func_ram_offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_READ);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);
	ELI_IOWRITE(pdata, TCAM_ACC, wdata);

	while(!ELI_IOREAD_BITS(pdata, TCAM_ACC, RD_STS));

	f_num_lkup = ELI_IOREAD(pdata, TCAM_FUNC_PRIO_LOOKUP); 

	return f_num_lkup;
}

static int eli_dmach_ram_write(struct xgbe_prv_data *pdata, int dma_ram_offset, int dma_channel)
{
	unsigned long wdata = 0;

	wdata = dma_channel;
	ELI_IOWRITE(pdata, TCAM_DMACH_LOOKUP, wdata); 

	wdata = 0; 

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_DMA_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, dma_ram_offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_WRITE);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);

	ELI_IOWRITE(pdata, TCAM_ACC, wdata);    

	wdata = 0;

	return 0;
}

static int eli_dmach_ram_read(struct xgbe_prv_data *pdata, int dma_ram_offset)
{
	unsigned long wdata = 0;
	int dma_ch_lkup = 0;

	ELI_SET_BITS(wdata, TCAM_ACC, RAM_SEL, ELI_TCAM_DMA_RAM);
	ELI_SET_BITS(wdata, TCAM_ACC, XCESS_ADDR, dma_ram_offset);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_SEL, ELI_OP_READ);
	ELI_SET_BITS(wdata, TCAM_ACC, OP_STR_DONE, 1);
	ELI_IOWRITE(pdata, TCAM_ACC, wdata);

	while(!ELI_IOREAD_BITS(pdata, TCAM_ACC, RD_STS));

	dma_ch_lkup = ELI_IOREAD(pdata, TCAM_DMACH_LOOKUP);

	return dma_ch_lkup;
}

static void create_bc_entry(struct xgbe_prv_data *pdata)
{
	int eli_mac_index, f_num_lkup, dma_ch_lkup;
	unsigned char da_0[] = { 0,0,0,0,0,0 };
	int ivt=0, ovt=0, i;	

	eli_mac_index = get_mac_index(pdata, MAC_INX_OTHER, UC_MAC); 
	dma_ch_lkup = 0;

	pdata->eli_pf_vf_bc_ch = 0x2;
	dma_ch_lkup = pdata->eli_pf_vf_bc_ch;

	memset( da_0, 0xff, 6);
	eli_tcam_write(pdata, eli_mac_index, da_0, ivt, ovt, UC_MAC);

	f_num_lkup = eli_mac_index;
	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 1);

	for(i = 0; i < 8; i++){
		eli_dmach_ram_write(pdata, (f_num_lkup * 8) + i, dma_ch_lkup);
	}

	pdata->occupied_eli[0] |= (1 << eli_mac_index);
}

static void create_mc_entry(struct xgbe_prv_data *pdata)
{
	unsigned char ipv4_mc[] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x00};
	unsigned char ipv6_mc[] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x00};
	int eli_mac_index, f_num_lkup, inx = 0;

	eli_mac_index = get_mac_index(pdata, MAC_INX_OTHER, IPV4_MC_MAC);
	f_num_lkup = eli_mac_index;
	eli_tcam_write(pdata, eli_mac_index, ipv4_mc, 0, 0, IPV4_MC_MAC);
	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 1);

	if(eli_mac_index >= 32){
		eli_mac_index -= 32;
		inx = 1;
	}
	pdata->occupied_eli[inx] |= (1 << eli_mac_index);

	eli_mac_index = get_mac_index(pdata, MAC_INX_OTHER, IPV6_MC_MAC);
	f_num_lkup = eli_mac_index;
	eli_tcam_write(pdata, eli_mac_index, ipv6_mc, 0, 0, IPV6_MC_MAC); 
	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 1);

	if(eli_mac_index >= 32){
		eli_mac_index -= 32;
		inx = 1;
	}
	pdata->occupied_eli[inx] |= (1 << eli_mac_index);
}

static void xgbe_rx_queue_mapping(struct xgbe_prv_data *pdata)
{
	unsigned int prio_queues;
	unsigned int ppq, ppq_extra, prio;
	unsigned int mask;
	unsigned int i, j, reg, reg_val;

	
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC5R, PRQSO, 0);

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
			mask |= (1 << prio);
			pdata->prio2q_map[prio++] = i;
		}

		if (i < ppq_extra) {
			netif_dbg(pdata, drv, pdata->netdev,
					"PRIO%u mapped to RXq%u\n", prio, i);
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

}

static void xgbe_init_queues(struct xgbe_prv_data *pdata)
{
	xgbe_rx_queue_mapping(pdata);
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, DCBCPQ, (pdata->rx_q_count - 2));
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, PTPQ, (pdata->rx_q_count - 1));
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, AVCPQ, (pdata->rx_q_count - 5));
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, TACPQE, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, TPQC, 1);
}

static int eli_linux_init(struct xgbe_prv_data *pdata)
{
	int i; 
	unsigned char da_0[] = { 0,0,0,0,0,0 };

	eli_tcam_reset(pdata,1);

	eli_tcam_reset(pdata,2);

	for(i = 0; i < ELI_MAX_MAC_INDEX; i++){
		eli_tcam_write(pdata, i, da_0, 0, 0, UC_MAC);
		eli_func_ram_write(pdata, i, 0, 0);
	}

	for(i = 0; i < ELI_MAX_DMAP_INDEX; i++){
		eli_dmach_ram_write(pdata, i, 0);
	}   

	create_bc_entry(pdata);
	create_mc_entry(pdata);

	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, RA, 0);

	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, MCBCQ, (pdata->rx_q_count - 1));
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, MCBCQEN, 1);
	XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, UPQ, (pdata->rx_q_count - 2));

	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, PM, 0);

	ELI_PRNT("0x00A0 : %x\n", XGMAC_IOREAD(pdata, MAC_RQC0R));
	ELI_PRNT("MAC_RQC1R : %x\n", XGMAC_IOREAD(pdata, MAC_RQC1R));
	ELI_PRNT("0x0094 : %x\n", XGMAC_IOREAD(pdata, MAC_RQC4R));
	ELI_PRNT("0x00ac : %x\n", XGMAC_IOREAD(pdata, MAC_RQC3R));
	ELI_PRNT("0x0140 : %x\n", XGMAC_IOREAD(pdata, MAC_EXT_CFG));

	XGMAC_IOWRITE(pdata, MAC_RQC4R, 0x80000B00);
	XGMAC_IOWRITE(pdata, MAC_RQC3R, 0x00000704);
	XGMAC_IOWRITE(pdata, MAC_EXT_CFG, 0x00000080);
	
	xgbe_init_queues(pdata);

	ELI_IOWRITE(pdata, TCAM_DMACH_CTRL, 0x0);

	ELI_IOWRITE(pdata, TCAM_UNTAGGED_PRI, 0);

	XGMAC_IOWRITE_BITS(pdata, MAC_RCR, ELEN, 1);

	return 0;
}

static int eli_set_promiscuous_mode(struct xgbe_prv_data *pdata, unsigned int fun_num, unsigned int enable)
{
	int dma_ch_lkup = 0, eli_mac_index = 0, f_num_lkup = 0, temp = 0;
	struct vf_data_storage *vfinfo = NULL;
	int i, j, inx = 0, val = 0, mask_new = 0;
	unsigned int rx_q_pri_lo = 0, rx_q_pri_hi = 0;
	u8 *da;
	bool promisc;

	if (fun_num == 0) {
		mask_new = (1 << (pdata->max_DMA_pf - 2)); 
		temp = (1 << (pdata->max_DMA_pf - 1));
		da = pdata->netdev->dev_addr;
		promisc = pdata->promisc;
	} else if (fun_num > 0 && fun_num < 8) {
		vfinfo = &pdata->vfinfo[fun_num - 1];
		mask_new = (1 << ((vfinfo->num_dma_channels * fun_num) + (pdata->max_DMA_pf - 2)));
		temp = (1 << ((vfinfo->num_dma_channels * fun_num) + (pdata->max_DMA_pf - 1)));
		da = vfinfo->vf_mac_addresses;
		promisc = vfinfo->promisc;
	} else {
		ELI_PRNT("INVALID Function number to enable Promiscuous mode\n");
		da = NULL;
		return -1;
	}

	for(i = 1; i < (ELI_MAX_MAC_INDEX); i++){

		if(i >= 32)
			inx = 1;
		val = (inx ? (i - 32) : i);

		if(((pdata->occupied_eli[inx] >> val) & 0x1)){
			eli_mac_index = i;

			if(eli_tcam_ram_compare(pdata, da, eli_mac_index))
				continue;

			f_num_lkup = eli_mac_index; 
			f_num_lkup &= 0x3F;

			if(eli_dmach_ram_read(pdata, (f_num_lkup * 8)) & mask_new)
				continue;

			for(j = 0; j < 8; j++){ 
				dma_ch_lkup = eli_dmach_ram_read(pdata, ((f_num_lkup * 8) + j));

				if(enable){ 
					dma_ch_lkup |= temp;
				}
				else{
					dma_ch_lkup &= (~temp);
				}
				eli_dmach_ram_write(pdata, ((f_num_lkup * 8) + j), dma_ch_lkup);
			}
		}
	}

	if(enable){
		temp |= ELI_IOREAD(pdata, TCAM_DMACH_CTRL);

		if(vfinfo) 
			vfinfo->promisc = 1;
		else
			pdata->promisc = 1;
	}
	else{
		temp = (ELI_IOREAD(pdata, TCAM_DMACH_CTRL) & (~temp));
		if(vfinfo)
			vfinfo->promisc = 0;
		else
			pdata->promisc = 0;
	}

	ELI_IOWRITE(pdata, TCAM_DMACH_CTRL, temp);

	if(temp){
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, RA, 1);
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, VTFE, 0);
	}else{
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, RA, 0);
		XGMAC_IOWRITE_BITS(pdata, MAC_PFR, VTFE, 1);
	}

	if(pdata->rx_q_count > 4){
		if(pdata->rx_q_count > 8){
			rx_q_pri_hi = (0xFF << 24);
			
			XGMAC_IOWRITE(pdata, MAC_RQC2R, 0);
			XGMAC_IOWRITE(pdata, MAC_RQC3R, 0);

			XGMAC_IOWRITE_BITS(pdata, MAC_RQC5R, PRQSO, (pdata->rx_q_count - 8));
		}else
			rx_q_pri_hi = (0xFF << ((pdata->rx_q_count - 5) * 8));
	}else
		rx_q_pri_lo = (0xFF << ((pdata->rx_q_count - 1) * 8));

	if(temp){ 		
		ELI_PRNT("Remapping VLAN packets with all priorities to highest RxQ\n");
		XGMAC_IOWRITE(pdata, MAC_RQC2R, rx_q_pri_lo);
		XGMAC_IOWRITE(pdata, MAC_RQC3R, rx_q_pri_hi);

		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, DCBCPQ, (pdata->rx_q_count - 1));
		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, PTPQ, (pdata->rx_q_count - 1));
		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, AVCPQ, (pdata->rx_q_count - 1));
		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, TACPQE, 1);
	}
	else{
		XGMAC_IOWRITE(pdata, MAC_RQC2R, 0);
		XGMAC_IOWRITE(pdata, MAC_RQC3R, 0);
		xgbe_rx_queue_mapping(pdata);

		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, DCBCPQ, (pdata->rx_q_count - 2));
		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, PTPQ, (pdata->rx_q_count - 1));
		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, AVCPQ, (pdata->rx_q_count - 5));
		XGMAC_IOWRITE_BITS(pdata, MAC_RQC1R, TACPQE, 0);
	}

	return 0;
}

static int eli_mac_add(struct xgbe_prv_data *pdata, int fun_num, unsigned char *da )
{

	int f_num_lkup, dma_ch_lkup, i, eli_mac_index;
	int ivt=0, ovt=0;
	int dma_max_cnt = MISC_IOREAD( pdata, XGMAC_MISC_FUNC_RESOURCES_PF0);

	eli_mac_index = ELI_MAC_INDEX - fun_num;
	eli_tcam_write( pdata, eli_mac_index, da, ivt, ovt, UC_MAC); 

	f_num_lkup = eli_mac_index;
	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 0);

	dma_ch_lkup = 1<<(fun_num*dma_max_cnt); 
	for(i = 0; i < 8; i++) {
		eli_dmach_ram_write(pdata, (f_num_lkup * 8) + i, dma_ch_lkup);
	}

	if (fun_num != 0) {
		pdata->eli_pf_vf_bc_ch |= (0x1 << ((dma_max_cnt * fun_num) + 1));
		for(i = 0; i < 8; i++){
			eli_dmach_ram_write(pdata, (ELI_FUNC_BC * 8) + i, pdata->eli_pf_vf_bc_ch);
		}
	}

	eli_mac_index -= 32;
	pdata->occupied_eli[1] |= (1 << eli_mac_index);

	return 0;
}

static void eli_set_all_multicast(struct xgbe_prv_data *pdata, int fun_num, int enable)
{
	int dma_max_cnt = MISC_IOREAD( pdata, XGMAC_MISC_FUNC_RESOURCES_PF0);
	int dma_ch_lkup, i, temp = 0;
	struct vf_data_storage *vfinfo = NULL;

	dma_ch_lkup = eli_dmach_ram_read(pdata, ELI_FUNC_MC_IPV4(max_vfs) * 8);

	temp = (dma_max_cnt * fun_num) + dma_max_cnt - 1;
	temp = temp - 1;
	temp = (1 << temp);

	if(fun_num > 0 && fun_num < 8)
		vfinfo = &pdata->vfinfo[fun_num - 1];

	if(enable){
		dma_ch_lkup |= temp;
		if(vfinfo)
			vfinfo->pm_mode = 1;
		else
			pdata->pm_mode = 1;
	} else {
		dma_ch_lkup &= (~(temp));
		if(vfinfo)
			vfinfo->pm_mode = 0;
		else
			pdata->pm_mode = 0;
	}

	for(i = 0; i < 8; i++){
		eli_dmach_ram_write(pdata, (ELI_FUNC_MC_IPV4(max_vfs) * 8) + i, dma_ch_lkup);
		eli_dmach_ram_write(pdata, (ELI_FUNC_MC_IPV6(max_vfs) * 8) + i, dma_ch_lkup);
	}
}

static int eli_mc_mac_add(struct xgbe_prv_data *pdata, unsigned char *addr,
		unsigned int fun_num)
{
	struct vf_data_storage *vfinfo = NULL;
	int f_num_lkup, dma_ch_lkup, i, eli_mac_index, temp, inx = 0;
	unsigned char da[6];
	int dma_max_cnt = MISC_IOREAD(pdata, XGMAC_MISC_FUNC_RESOURCES_PF0);

	if(!addr)
		return 0;

	if(fun_num > 0 && fun_num < 8)
		vfinfo = &pdata->vfinfo[fun_num - 1];

	temp = (dma_max_cnt * fun_num) + dma_max_cnt - 1;
	temp = temp - 1;
	temp = (1 << temp);

	for(i = 0; i < 6; i++)
		da[i] = addr[i];


	eli_mac_index = eli_tcam_search(pdata, addr, 0, 0, 0);
	if((eli_mac_index == ELI_FUNC_MC_IPV4(max_vfs)) || (eli_mac_index == ELI_FUNC_MC_IPV6(max_vfs)))
		goto check;
	if(eli_mac_index == -1){
check:
		eli_mac_index = get_mac_index(pdata, MAC_INX_OTHER, UC_MAC);
		if(eli_mac_index == -1){
			netdev_info(pdata->netdev, "No space available in TCAM ram\n");
			netdev_info(pdata->netdev, "Enabling wild card matching for some addresses\n");
			netdev_info(pdata->netdev, "Might not work as expected\n");
			return -1;
		}

		eli_tcam_write(pdata, eli_mac_index, addr, 0, 0, UC_MAC);
	}

	f_num_lkup = eli_mac_index;
	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 1);

	dma_ch_lkup = eli_dmach_ram_read(pdata, (f_num_lkup * 8));

	dma_ch_lkup |= temp;

	for(i = 0; i < 8; i++){
		eli_dmach_ram_write(pdata, (f_num_lkup * 8) + i, dma_ch_lkup);	
	}

	if(fun_num == 0) 
		set_bit(eli_mac_index, pdata->mc_indices);
	else
		if(vfinfo)
			set_bit(eli_mac_index, vfinfo->mc_indices);

	if(eli_mac_index >= 32){
		eli_mac_index -= 32;
		inx = 1;
	}
	pdata->occupied_eli[inx] |= (1 << eli_mac_index);

	return 0;
}

static int eli_mc_mac_del(struct xgbe_prv_data *pdata, int offset, int fun_num)
{
	struct vf_data_storage *vfinfo = NULL;
	int f_num_lkup, dma_ch_lkup, eli_mac_index, i, temp = 0;
	int dma_max_cnt = MISC_IOREAD(pdata, XGMAC_MISC_FUNC_RESOURCES_PF0); 
	unsigned char da[6];
	int p, n, mask = 0, inx = 0;

	if(offset == -1)
		return 0;

	if(fun_num > 0 && fun_num < 8)
		vfinfo = &pdata->vfinfo[fun_num - 1];

	p = (dma_max_cnt * fun_num) + (dma_max_cnt - 1);
	n = dma_max_cnt;
	PREP_MASK(mask, p, n, (max_vfs + 1));    

	if((offset == ELI_FUNC_MC_IPV4(max_vfs)) || (offset == ELI_FUNC_MC_IPV6(max_vfs)))
		return 0;

	eli_mac_index = offset;
	f_num_lkup = offset;
	dma_ch_lkup = eli_dmach_ram_read(pdata, (f_num_lkup * 8)); 

	temp = (dma_max_cnt * fun_num) + dma_max_cnt - 1;
	temp = temp - 1;
	temp = (1 << temp);

	if((dma_ch_lkup & mask)== 0){
		memset(da, 0x0, 6);
		eli_tcam_write(pdata, offset, da, 0, 0, UC_MAC);
		eli_func_ram_write(pdata, offset, 0, 0);
		dma_ch_lkup = 0;

		if(eli_mac_index >= 32){
			eli_mac_index -= 32;
			inx = 1;
		}
		pdata->occupied_eli[inx] &= (~(1 << eli_mac_index));
	} else {
		dma_ch_lkup &= (~temp);	
	}

	for(i = 0; i < 8; i++)
		eli_dmach_ram_write(pdata, (f_num_lkup * 8) + i, dma_ch_lkup);

	if(fun_num == 0)		
		clear_bit(offset, pdata->mc_indices); 
	else
		if(vfinfo)
			clear_bit(offset, vfinfo->mc_indices);

	return 0;
}

static int eli_mac_del(struct xgbe_prv_data *pdata, int fun_num)
{
	unsigned int eli_mac_index = ELI_MAC_INDEX - fun_num;
	int i, f_num_lkup = 0, dma_ch_lkup = 0;
	unsigned char da_0[] = {0, 0, 0, 0, 0, 0};
	int inx = 0;

	eli_tcam_write(pdata, eli_mac_index, da_0, 0, 0, UC_MAC);

	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 0);

	f_num_lkup = eli_mac_index;

	for(i = 0; i < 8; i++){
		eli_dmach_ram_write(pdata, (f_num_lkup * 8) + i, dma_ch_lkup);
	}

	if (fun_num != 0) {
		pdata->eli_pf_vf_bc_ch &= ~(0x1 << ((pdata->hw_feat.tx_ch_cnt * fun_num) + 1));
		for(i = 0; i < 8; i++){
			eli_dmach_ram_write(pdata, (ELI_FUNC_BC * 8) + i, pdata->eli_pf_vf_bc_ch);
		}
	}

	if(eli_mac_index >= 32){
		eli_mac_index -= 32;
		inx = 1;
	}
	pdata->occupied_eli[inx] &= (~(1 << eli_mac_index));

	return 0;
}

static int eli_add_vlan(struct xgbe_prv_data *pdata, int vid, int vf)
{
	int i, inx = 0;
	struct vf_data_storage *vfinfo;
	u8 *da;
	unsigned int eli_mac_index = 0, f_num_lkup = 0, dma_ch_lkup = 0;

	if (vid == 0)
		return 1; 

	if (vf == XGBE_VLAN_REQ_FROM_PF) {
		ELI_PRNT("Add VLAN (%d) request from PF \n", vid);

		if((pdata->vlan_filters_used) < VLAN_ID_LIMIT) {	
			eli_mac_index = get_mac_index(pdata, MAC_INX_VLAN, UC_MAC);
		} else{
			netdev_info(pdata->netdev, "Warning! Trying to assign more than %d VLAN IDs to PF\n", VLAN_ID_LIMIT);
			netdev_info(pdata->netdev, "Success depends on availability of entry in TCAM memory\n");
			eli_mac_index = get_mac_index(pdata, MAC_INX_OTHER, UC_MAC);
		}

		ELI_PRNT("Number of filters used by PF = %d\n", pdata->vlan_filters_used);

		if(eli_mac_index == -1){
			netdev_info(pdata->netdev, "No space available in TCAM RAM\n");
			netdev_info(pdata->netdev, "Try removing one of the VLAN interfaces or by closing applications that make use of Multicast\n");
			return -1; 
		}

		f_num_lkup = eli_mac_index;

		da = pdata->netdev->dev_addr;

		for(i = 0; i < 8; i++){
			dma_ch_lkup = (1 << (pdata->last_used_dma));
			eli_dmach_ram_write(pdata, ((f_num_lkup * 8) + i), dma_ch_lkup);
		}       

		pdata->last_used_dma++;

		if (pdata->last_used_dma == (pdata->max_DMA_pf)) {
			pdata->last_used_dma = 0;
		}
		pdata->vlan_filters_used++;    

	} else {
		ELI_PRNT("Add VLAN (%d) request from VF-%d\n", vid, vf);

		vfinfo = &pdata->vfinfo[vf];

		if ((vfinfo->vlan_filters_used) < VLAN_ID_LIMIT) {
			eli_mac_index = get_mac_index(pdata, MAC_INX_VLAN, UC_MAC);	
		} else {
			netdev_info(pdata->netdev, "Warning! Trying to assign more than %d VLAN IDs to VF%d\n", VLAN_ID_LIMIT, vf);
			netdev_info(pdata->netdev, "Success depends on availability of entry in TCAM memory\n");
			eli_mac_index = get_mac_index(pdata, MAC_INX_OTHER, UC_MAC);
		}

		ELI_PRNT("Number of filters used by VF%d = %d\n", vf, pdata->vlan_filters_used);

		if(eli_mac_index == -1){
			netdev_info(pdata->netdev, "No space available in TCAM RAM\n");
			netdev_info(pdata->netdev, "Try removing one of the VLAN interfaces or by closing applications that make use of Multicast\n");
			return -1; 
		}

		f_num_lkup = eli_mac_index;

		da = vfinfo->vf_mac_addresses; 

		for(i = 0; i < 8; i++){
			dma_ch_lkup = (1 << (vfinfo->last_used_dma));
			eli_dmach_ram_write(pdata, ((f_num_lkup * 8) + i), dma_ch_lkup);

		}

		vfinfo->last_used_dma++;
		if (vfinfo->last_used_dma == (vfinfo->dma_channel_start_num + vfinfo->num_dma_channels)) {
			vfinfo->last_used_dma = vfinfo->dma_channel_start_num;
		}
		vfinfo->vlan_filters_used++;
	}

	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTDR, ETV, 0);
	XGMAC_IOWRITE_BITS(pdata, MAC_VLANTDR, VEN, 1);

	eli_tcam_write(pdata, eli_mac_index, da, 0, vid, UC_MAC);
	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 1);

	if(eli_mac_index >= 32){
		eli_mac_index -= 32;
		inx = 1;
	}
	pdata->occupied_eli[inx] |= (1 << eli_mac_index);

	eli_tcam_search(pdata, da, 0, vid, 0);

	return 0;
}

static int eli_kill_vlan(struct xgbe_prv_data *pdata, int vid, int vf)
{
	unsigned int vlanid = 0;
	int i = 0, inx = 0;
	struct vf_data_storage *vfinfo;
	u8 *da;
	unsigned int eli_mac_index = 0, f_num_lkup = 0, dma_ch_lkup = 0;
	unsigned char da_0[] = {0, 0, 0, 0, 0, 0};

	if (vf == XGBE_VLAN_REQ_FROM_PF) {
		ELI_PRNT("Kill VLAN request from PF for VLAN ID : %d\n", vid);
		for_each_set_bit(vlanid, pdata->active_vlans, VLAN_N_VID) {
			if (vlanid == vid) {
				ELI_PRNT("VLAN ID %d is configured for PF in filter\n", vlanid);

				da = pdata->netdev->dev_addr;

				pdata->vlan_filters_used--;
				//if(pdata->last_used_dma != 0)
				//	pdata->last_used_dma--;

				goto success;
			}
		}
		ELI_PRNT("VlAN ID %d is not configured for PF\n", vid);
		goto failure;
	} else {
		vfinfo = &pdata->vfinfo[vf];

		ELI_PRNT("Kill VLAN request from VF%d for VLAN ID %d\n", vf, vid);
		for_each_set_bit (vlanid, vfinfo->active_vlans, VLAN_N_VID) {
			if (vlanid == vid) {
				ELI_PRNT("VLAN ID %d is configured for VF in filter\n", vlanid);

				da = vfinfo->vf_mac_addresses;

				vfinfo->vlan_filters_used--;
				//if (vfinfo->last_used_dma != 0)
				//	vfinfo->last_used_dma--;

				goto success;
			}
		}
		ELI_PRNT("VLAN ID %d is not confiured for VF%d\n", vid, vf);
		goto failure;
	}

success:

	eli_mac_index = eli_tcam_search(pdata, da, 0, vid, 0);
	if(eli_mac_index == -1){
		ELI_PRNT("No matching entry in TCAM\n");
		return -1;
	}

	eli_tcam_write(pdata, eli_mac_index, da_0, 0, 0, UC_MAC);

	eli_func_ram_write(pdata, eli_mac_index, f_num_lkup, 0);

	f_num_lkup = eli_mac_index;

	for(i = 0; i < 8; i++){
		eli_dmach_ram_write(pdata, (f_num_lkup * 8) + i, dma_ch_lkup);
	}

	if(eli_mac_index >= 32){
		eli_mac_index -= 32;
		inx = 1;
	}
	pdata->occupied_eli[inx] &= (~(1 << eli_mac_index));

	return 0;

failure:

	return -1;
}

void xgbe_init_function_ptrs_eli(struct xgbe_eli_if *eli_if)
{   
	eli_if->mac_add = eli_mac_add;
	eli_if->mac_del = eli_mac_del;
	eli_if->add_vlan = eli_add_vlan;
	eli_if->kill_vlan = eli_kill_vlan;
	eli_if->set_promiscuous_mode = eli_set_promiscuous_mode;
	eli_if->mc_mac_add = eli_mc_mac_add;
	eli_if->mc_mac_del = eli_mc_mac_del;
	eli_if->set_all_multicast_mode = eli_set_all_multicast;
	eli_if->eli_init = eli_linux_init;
	eli_if->dmach_ram_read = eli_dmach_ram_read;
	eli_if->func_ram_read = eli_func_ram_read;
	eli_if->tcam_ram_read =eli_tcam_ram_read;
}
#endif
#endif
