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

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "xgbe.h"
#include "xgbe-common.h"

static ssize_t xgbe_common_read(char __user *buffer, size_t count,
				loff_t *ppos, unsigned int value)
{
	char *buf;
	ssize_t len;

	if (*ppos != 0)
		return 0;

	buf = kasprintf(GFP_KERNEL, "0x%08x\n", value);
	if (!buf)
		return -ENOMEM;

	if (count < strlen(buf)) {
		kfree(buf);
		return -ENOSPC;
	}

	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
	kfree(buf);

	return len;
}

static ssize_t xgbe_common_write(const char __user *buffer, size_t count,
				 loff_t *ppos, unsigned int *value)
{
	char workarea[32];
	ssize_t len;
	int ret;

	if (*ppos != 0)
		return -EINVAL;

	if (count >= sizeof(workarea))
		return -ENOSPC;

	len = simple_write_to_buffer(workarea, sizeof(workarea) - 1, ppos, buffer, count);
	if (len < 0)
		return len;

	workarea[len] = '\0';
	ret = kstrtouint(workarea, 16, value);
	if (ret)
		return -EIO;

	return len;
}

static ssize_t xgmac_reg_addr_read(struct file *filp, char __user *buffer,
				   size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_read(buffer, count, ppos, pdata->debugfs_xgmac_reg);
}

static ssize_t xgmac_reg_addr_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_write(buffer, count, ppos, &pdata->debugfs_xgmac_reg);
}

static ssize_t xgmac_reg_value_read(struct file *filp, char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	unsigned int value;

	value = XGMAC_IOREAD(pdata, pdata->debugfs_xgmac_reg);

	return xgbe_common_read(buffer, count, ppos, value);
}

static ssize_t xgmac_reg_value_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	unsigned int value;
	ssize_t len;

	len = xgbe_common_write(buffer, count, ppos, &value);
	if (len < 0)
		return len;

	XGMAC_IOWRITE(pdata, pdata->debugfs_xgmac_reg, value);

	return len;
}
static const struct file_operations xgmac_reg_addr_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  xgmac_reg_addr_read,
	.write = xgmac_reg_addr_write,
};

static const struct file_operations xgmac_reg_value_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  xgmac_reg_value_read,
	.write = xgmac_reg_value_write,
};

static ssize_t misc_reg_addr_read(struct file *filp, char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_read(buffer, count, ppos, pdata->debugfs_misc_reg);
}

static ssize_t misc_reg_addr_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_write(buffer, count, ppos, &pdata->debugfs_misc_reg);
}

static ssize_t misc_reg_value_read(struct file *filp, char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	unsigned int value;

	value = MISC_IOREAD(pdata, pdata->debugfs_misc_reg);

	return xgbe_common_read(buffer, count, ppos, value);
}

static ssize_t misc_reg_value_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	unsigned int value;
	ssize_t len;

	len = xgbe_common_write(buffer, count, ppos, &value);
	if (len < 0)
		return len;

	MISC_IOWRITE(pdata, pdata->debugfs_misc_reg, value);

	return len;
}

static const struct file_operations misc_reg_addr_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  misc_reg_addr_read,
	.write = misc_reg_addr_write,
};

static const struct file_operations misc_reg_value_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  misc_reg_value_read,
	.write = misc_reg_value_write,
};

#if XGBE_SRIOV_PF
static ssize_t eli_dump_read(struct file *filp, char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	char *buf;
	ssize_t len;
	int i;
	char da[6];
	int ovt;
	int ivt;
	int func;
	int dma_ch;
	int offset;
	int index;

	if (*ppos != 0)
		return 0;

	buf = kzalloc(4096, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (count < strlen(buf)) {
		kfree(buf);
		return -ENOSPC;
	}
	len = 0;
	len += snprintf(&buf[len], 4096, "IDX\t     DEST-MAC     \t OVT \t IVT \tFUNC RAM\tDMA-CH-MAP\n");
	for(i=0; i<ELI_MAX_MAC_INDEX; i++)
	{
		pdata->eli_if.tcam_ram_read(pdata, i, &da[0], &ivt, &ovt);
		func = pdata->eli_if.func_ram_read(pdata, i);
		dma_ch = pdata->eli_if.dmach_ram_read(pdata, i*8);

		if(i >= 32){
			offset = (i - 32);
			index = 1;
		} else {
			offset = i;
			index = 0;
		}

		if(pdata->occupied_eli[index] & (1 << offset)) {
			len += snprintf(&buf[len], 4096, "%03d\t%02X:%02X:%02X:%02X:%02X:%02X\t%04d\t%04d\t%08d\t%04x\n",
				i,da[0]&0xff,da[1]&0xff,da[2]&0xff,da[3]&0xff,da[4]&0xff,da[5]&0xff,ovt,ivt,ELI_GET_BITS(func, TCAM_FUNC_PRIO_LOOKUP, FUNC), dma_ch);
		}
	}
	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
	kfree(buf);

	return len;
}


static const struct file_operations eli_dump_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  eli_dump_read,
};

static ssize_t xpcs_mmd_read(struct file *filp, char __user *buffer, size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_read(buffer, count, ppos, pdata->debugfs_xpcs_mmd);
}

static ssize_t xpcs_mmd_write(struct file *filp, const char __user *buffer,
			      size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_write(buffer, count, ppos,
				 &pdata->debugfs_xpcs_mmd);
}

static ssize_t xpcs_reg_addr_read(struct file *filp, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_read(buffer, count, ppos, pdata->debugfs_xpcs_reg);
}

static ssize_t xpcs_reg_addr_write(struct file *filp, const char __user *buffer,
				   size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;

	return xgbe_common_write(buffer, count, ppos,
				 &pdata->debugfs_xpcs_reg);
}

static ssize_t xpcs_reg_value_read(struct file *filp, char __user *buffer,
				   size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	unsigned int value = 0;

	if(pdata->hw_if.read_mmd_regs)
	  value = pdata->hw_if.read_mmd_regs(pdata,  pdata->debugfs_xpcs_mmd,  pdata->debugfs_xpcs_reg);

	return xgbe_common_read(buffer, count, ppos, value);
}

static ssize_t xpcs_reg_value_write(struct file *filp,
				    const char __user *buffer,
				    size_t count, loff_t *ppos)
{
	struct xgbe_prv_data *pdata = filp->private_data;
	unsigned int value;
	ssize_t len;

	len = xgbe_common_write(buffer, count, ppos, &value);
	if (len < 0)
		return len;

	if(pdata->hw_if.write_mmd_regs)
		pdata->hw_if.write_mmd_regs(pdata,pdata->debugfs_xpcs_mmd, pdata->debugfs_xpcs_reg, value);

	return len;
}

static const struct file_operations xpcs_mmd_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  xpcs_mmd_read,
	.write = xpcs_mmd_write,
};

static const struct file_operations xpcs_reg_addr_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  xpcs_reg_addr_read,
	.write = xpcs_reg_addr_write,
};

static const struct file_operations xpcs_reg_value_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  xpcs_reg_value_read,
	.write = xpcs_reg_value_write,
};
#endif
void xgbe_debugfs_init(struct xgbe_prv_data *pdata)
{
	char *buf;

	/* Set defaults */
	pdata->debugfs_xgmac_reg = 0;
	pdata->debugfs_misc_reg = 0;

#if XGBE_SRIOV_PF
	pdata->debugfs_xpcs_mmd = 1;
	pdata->debugfs_xpcs_reg = 0;
#endif
	buf = kasprintf(GFP_KERNEL, "brcm-xgbe-%s", pdata->netdev->name);
	if (!buf)
		return;

	pdata->xgbe_debugfs = debugfs_create_dir(buf, NULL);

	debugfs_create_file("xgmac_register", 0600, pdata->xgbe_debugfs, pdata, &xgmac_reg_addr_fops);

	debugfs_create_file("xgmac_register_value", 0600, pdata->xgbe_debugfs, pdata, &xgmac_reg_value_fops);

	debugfs_create_file("misc_register", 0600, pdata->xgbe_debugfs, pdata, &misc_reg_addr_fops);

	debugfs_create_file("misc_register_value", 0600, pdata->xgbe_debugfs, pdata, &misc_reg_value_fops);

#if XGBE_SRIOV_PF
	if(pdata->dev_id == BCM8989X_PF_ID) {
		debugfs_create_file("phy_devad", 0600, pdata->xgbe_debugfs, pdata, &xpcs_mmd_fops);

		debugfs_create_file("phy_mdio_addr", 0600, pdata->xgbe_debugfs, pdata, &xpcs_reg_addr_fops);

		debugfs_create_file("phy_mdio_addr_value", 0600, pdata->xgbe_debugfs, pdata, &xpcs_reg_value_fops);
	}
#if	ELI_ENABLE
	debugfs_create_file("eli_dump", 0400, pdata->xgbe_debugfs, pdata, &eli_dump_fops);
#endif
#endif
	kfree(buf);
}

void xgbe_debugfs_exit(struct xgbe_prv_data *pdata)
{
	debugfs_remove_recursive(pdata->xgbe_debugfs);
	pdata->xgbe_debugfs = NULL;
}

void xgbe_debugfs_rename(struct xgbe_prv_data *pdata)
{
	char *buf;

	if (!pdata->xgbe_debugfs)
		return;

	buf = kasprintf(GFP_KERNEL, "brcm-xgbe-%s", pdata->netdev->name);
	if (!buf)
		return;

	if (!strcmp(pdata->xgbe_debugfs->d_name.name, buf))
		goto out;

	debugfs_rename(pdata->xgbe_debugfs->d_parent, pdata->xgbe_debugfs, pdata->xgbe_debugfs->d_parent, buf);

out:
	kfree(buf);
}
