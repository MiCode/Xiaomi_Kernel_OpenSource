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
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/gpio/machine.h>
#include <linux/of_gpio.h>

#include "xgbe.h"
#include "xgbe-common.h"

#if BRCM_BCMUTIL
#include "xgbe-bcmutil.h"
#endif

#if XGBE_SRIOV_PF
#if ((defined CONFIG_PCI_IOV) && (ELI_ENABLE == 1))
unsigned int max_vfs = XGBE_MAX_VFS_DEFAULT;
 module_param(max_vfs, uint, 0);
 MODULE_PARM_DESC(max_vfs,
		 "Maximum number of virtual functions to allocate per physical function - default is zero and maximum value is 7. (Deprecated)");
#else
unsigned int max_vfs = 0;
#endif /* CONFIG_PCI_IOV */
#endif

static int g_interrupt_gpio = -1;
static struct pci_dev* g_device = NULL;

static int xgbe_config_multi_msi(struct xgbe_prv_data *pdata)
{
	unsigned int vector_count;
	unsigned int i, j = 0;
	int ret;

#if XGBE_SRIOV_VF
	int rx_reg = XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_RX_VF;
	int tx_reg = XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_TX_VF;
#endif

	vector_count = XGBE_MSI_BASE_COUNT;
	vector_count += max(pdata->rx_ring_count,
			pdata->tx_ring_count);

	ret = pci_alloc_irq_vectors(pdata->pcidev, XGBE_MSI_MIN_COUNT,
			vector_count, PCI_IRQ_MSI | PCI_IRQ_MSIX);

	if (ret < 0) {
		dev_info(pdata->dev, "multi MSI/MSI-X enablement failed\n");
		return ret;
	} else {
		dev_info(pdata->dev, "multi MSI/MSI-X enablement passed\n");
	}

	pdata->isr_as_tasklet = 1;
	pdata->irq_count = ret;

#if XGBE_SRIOV_PF
	pdata->dev_irq = pci_irq_vector(pdata->pcidev, 0);
	pdata->dev_2_host_irq = pci_irq_vector(pdata->pcidev, 1);
#ifdef CONFIG_PCI_IOV
	for (i = 0; i < max_vfs; i++)
		pdata->vf_mbox_irq[i] = pci_irq_vector(pdata->pcidev, i + (XGBE_MSI_BASE_COUNT - SRIOV_MAX_VF));

	if (i)
		mbox_request_irqs(pdata);
#endif
	for (i = XGBE_MSI_BASE_COUNT, j = 0; i < ret; i++, j++)
		pdata->channel_irq[j] = pci_irq_vector(pdata->pcidev, i);
#endif

#if XGBE_SRIOV_VF
	pdata->mbx_irq = pci_irq_vector(pdata->pcidev, 0);
	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_MB_PF0_to_VF, 0x00);

	TEST_PRNT("Total MSI num = %d\n", ret);
	TEST_PRNT("mbx_irq = %d\n", pdata->mbx_irq);

	if (ret-1 == pdata->tx_ring_count)	{
		for (i = XGBE_MSI_BASE_COUNT, j = 0; i < ret; i++, j++) {
			pdata->channel_irq[j] =
					pci_irq_vector(pdata->pcidev, i);
			/*assiging vector numbers and storing in reg */
			MISC_IOWRITE(pdata, rx_reg, i);
			rx_reg += MSIC_INC;
			MISC_IOWRITE(pdata, tx_reg, i);
			tx_reg += MSIC_INC;
			TEST_PRNT("channel_num = %d\n vector_num = %d\n",
					j, pdata->channel_irq[j]);
		}
	}
#endif

	pdata->channel_irq_count = j;
	pdata->per_channel_irq = 1;
	pdata->channel_irq_mode = XGBE_IRQ_MODE_LEVEL;
	if (netif_msg_probe(pdata))
		dev_dbg(pdata->dev, "multi %s interrupts enabled\n",
				pdata->pcidev->msix_enabled ? "MSI-X" : "MSI");

	return 0;
}

static int xgbe_config_irqs(struct xgbe_prv_data *pdata)
{
	int ret;

	ret = xgbe_config_multi_msi(pdata);
	if (!ret)
		goto out;

	ret = pci_alloc_irq_vectors(pdata->pcidev, 1, 1,
			PCI_IRQ_LEGACY | PCI_IRQ_MSI);
	if (ret < 0) {
		dev_info(pdata->dev, "single IRQ enablement failed\n");
		return ret;
	}

	pdata->isr_as_tasklet = pdata->pcidev->msi_enabled ? 1 : 0;
	pdata->irq_count = 1;
	pdata->channel_irq_count = 1;

	pdata->dev_irq = pci_irq_vector(pdata->pcidev, 0);
	if (netif_msg_probe(pdata))
		dev_dbg(pdata->dev, "single %s interrupt enabled\n",
				pdata->pcidev->msi_enabled ?  "MSI" : "legacy");

out:
	if (netif_msg_probe(pdata)) {
		unsigned int i;
#if XGBE_SRIOV_PF
		dev_dbg(pdata->dev, " dev irq=%d\n", pdata->dev_irq);
#endif
#if XGBE_SRIOV_VF
		dev_dbg(pdata->dev, " mbx irq = %d\n", pdata->mbx_irq);
#endif
		for (i = 0; i < pdata->channel_irq_count; i++)
			dev_dbg(pdata->dev, " dma%u irq = %d\n",
					i, pdata->channel_irq[i]);
	}

	return 0;
}

#if XGBE_SRIOV_PF
static int xgbe_dma_soft_reset(struct xgbe_prv_data *pdata)
{
    unsigned int count = 2000;

    DBGPR("-->dma_soft_reset entry\n");

    /* Issue a software reset */
    XGMAC_IOWRITE_BITS(pdata, DMA_MR, SWR, 1);
    XGBE_USLEEP_RANGE(10, 15);

    /* Poll Until Poll Condition */
    while (--count && XGMAC_IOREAD_BITS(pdata, DMA_MR, SWR))
        XGBE_USLEEP_RANGE(500, 600);

    if (!count)
        return -EBUSY;

    DBGPR("<--dma_soft_reset exit\n");

    return 0;
}
#endif

static int xgbe_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct xgbe_prv_data *pdata;
	struct device *dev = &pdev->dev;
	void __iomem * const *iomap_table;
	int bar_mask;
	int ret;
	unsigned short device;

#if XGBE_SRIOV_PF
	int i;
	unsigned int num_pf_dma, num_vf_funs;
    struct resource *res;
#endif
	g_device = pdev;

	pdata = xgbe_alloc_pdata(dev);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto err_alloc;
	}

	pdata->pcidev = pdev;
	pci_set_drvdata(pdev, pdata);

	/* Get the version data */
	pdata->vdata = (struct xgbe_version_data *)id->driver_data;

	pci_read_config_word(pdev, PCI_DEV_ID_OFFSET, &device);

	ret = pcim_enable_device(pdev);
	if (ret) {
		dev_err(dev, "pcim_enable_device failed\n");
		goto err_pci_enable;
	}

	/* Obtain the mmio areas for the device */
	bar_mask = pci_select_bars(pdev, IORESOURCE_MEM);

	pdata->dev_id = device;
#if XGBE_SRIOV_PF
	if(device != BCM8956X_A0_PF_ID)
#endif
		bar_mask &= XGMAC_BAR_MASK;

	ret = pcim_iomap_regions(pdev, bar_mask, XGBE_DRV_NAME);
	if (ret) {
		dev_err(dev, "pcim_iomap_regions failed\n");
		goto err_pci_enable;
	}


	iomap_table = pcim_iomap_table(pdev);
	if (!iomap_table) {
		dev_err(dev, "pcim_iomap_table failed\n");
		ret = -ENOMEM;
		goto err_pci_enable;
	}
#if BRCM_BCMUTIL
    pdata->xgmac_regs = iomap_table[XGBE_XGMAC_BAR];
    xgbe_bcmutil_fixup(pdata);
#endif

#if XGBE_SRIOV_PF
	/* Initialize Phy register base address */
	res = &pdev->resource[XGBE_PHY_BAR];
	pdata->bar2_size = resource_size(res);
	pdata->phy_regs = iomap_table[XGBE_PHY_BAR] + (pdata->bar2_size - XGMAC_PHY_REGS_OFFSET_FROM_END);
#endif
	pdata->xgmac_regs = iomap_table[XGBE_XGMAC_BAR];
	pdata->misc_regs = pdata->xgmac_regs;
	pdata->mbox_regs = pdata->xgmac_regs + MBOX_REGS_OFFSET;

#if XGBE_SRIOV_PF
	pdata->eli_regs = pdata->xgmac_regs + XGMAC_ELI_REGS_OFFSET;
	pdata->xgmac_regs += XGMAC_REGS_OFFSET; /* XGMAC offset from BAR 0 */
	num_pf_dma = (MISC_IOREAD(pdata, XGMAC_MISC_FUNC_RESOURCES_PF0));
    
    /* total number of virtual functions */
    num_vf_funs = ((MISC_IOREAD_BITS(pdata, XGMAC_PCIE_MISC_POWER_STATE,
                NUM_FUNCTIONS)) - 1);  
	pdata->xgmac_regs -= XGMAC_REGS_OFFSET; /* XGMAC offset from BAR 0 */

    /* Errorouting if max_vfs is greater than supported vfs */
	if (max_vfs > num_vf_funs) {
		dev_err(dev, "ERROR: INVALID Entry for VFs count %d is NOT SUPPORTED,\
				Provide valid VF count\n", max_vfs);
		dev_err(dev, "Maximum Number of VFs supported for current \
				configuration are %d\n", num_vf_funs);
		dev_err(dev, "MODULE IS NOT INSERTED PROPERLY, \
				PLEASE REMOVE AND REINSERT WITH VALID INPUT\n");
		ret  = -ERRVF;
		goto err_alloc;
	}

	if(1){
		pci_write_config_dword(pdev, MMC_RX64OCTETS_GB_LO, MMC_RX64OCTETS_GB_LO_VAL);
		pci_write_config_dword(pdev, MMC_RX64OCTETS_GB_HI, MMC_RX64OCTETS_GB_HI_VAL);

		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_LO, XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_LO_VAL);
		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_HI, XGMAC_PCIE_MISC_MSIX_ADDR_MATCH_HI_VAL);


#if SRIOV_MAX_VF > 0
		MISC_IOWRITE(pdata, XGMAC_MISC_MAILBOX_INT_EN_PF0, SRIOV_VF_PF_INT_EN);

		for (i = 0; i < max_vfs; i++) {
			if (i == (SRIOV_MAX_VF - 1))
				MISC_IOWRITE(pdata, XGMAC_MISC_MSIX_VECTOR_MAP_MB_VF7, XGMAC_MAP_VF_MBOX_INT(i));
			else
				MISC_IOWRITE_OFFSET(pdata, XGMAC_MISC_MSIX_VECTOR_MAP_MB_BASE,
						i * XGMAC_MISC_MSIX_VECTOR_MAP_OFFSET, XGMAC_MAP_VF_MBOX_INT(i));
		}
#endif

		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_SBD_ALL, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_SBD_ALL_VAL);

		/* EP to Host DB intr 0 */
		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST_DOORBELL, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST_DOORBELL_VAL);
		/* EP to Host HW interrupts */
		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST0, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST0_VAL);
		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST1, XGMAC_PCIE_MISC_MSIX_VECTOR_MAP_EP2HOST1_VAL);

		for (i = 0; i < num_pf_dma; i++) {
			MISC_IOWRITE_OFFSET(pdata, XGMAC_MISC_MSIX_VECTOR_MAP_RX_PF0_BASE,
					i * XGMAC_MISC_MSIX_VECTOR_MAP_OFFSET, XGBE_MSI_BASE_COUNT + i);
			MISC_IOWRITE_OFFSET(pdata, XGMAC_MISC_MSIX_VECTOR_MAP_TX_PF0_BASE,
					i * XGMAC_MISC_MSIX_VECTOR_MAP_OFFSET, XGBE_MSI_BASE_COUNT + i);
		}
		/* switch enable */
		MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MII_CTRL, XGMAC_PCIE_MISC_MII_CTRL_VAL);
		if(device == BCM8989X_PF_ID) {
		   MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MII_CTRL, XGMAC_PCIE_MISC_MII_CTRL_VAL);
		   i = MISC_IOREAD(pdata, 0x8);
		   i &= ~(0x100);
		   i |= 0x200;
		   MISC_IOWRITE(pdata, 0x8, i);
		}
	}

	pdata->xgmac_regs = iomap_table[XGBE_XGMAC_BAR];
#endif

#if XGBE_SRIOV_VF
	MISC_IOWRITE(pdata, XGMAC_PCIE_MISC_MAILBOX_INT_EN_VF, 0x01);
#endif
	pdata->xgmac_regs += XGMAC_REGS_OFFSET; /* XGMAC offset from BAR 0 */

	if (!pdata->xgmac_regs) {
		dev_err(dev, "xgmac ioremap failed\n");
		ret = -ENOMEM;
		goto err_pci_enable;
	} else
		dev_info(dev, "xgmac ioremap done\n");


	pci_set_master(pdev);

	/* Clock settings */
#if XGBE_SRIOV_PF
	if(pdata->dev_id == BCM8989X_PF_ID)
		pdata->sysclk_rate = BCM8989X_V2_DMA_CLOCK_FREQ;
	else
		pdata->sysclk_rate = BCM895XX_V2_DMA_CLOCK_FREQ;
#endif

#if XGBE_SRIOV_VF
	if(pdata->dev_id == BCM8989X_VF_ID)
		pdata->sysclk_rate = BCM8989X_V2_DMA_CLOCK_FREQ;
	else
		pdata->sysclk_rate = BCM895XX_V2_DMA_CLOCK_FREQ;
#endif

	pdata->ptpclk_rate = XGBE_V2_PTP_CLOCK_FREQ;

	/* Set the DMA coherency values */
	pdata->coherent = XGBE_DMA_PCI_COHERENT;
	pdata->arcr = XGBE_DMA_PCI_ARCR;
	pdata->awcr = XGBE_DMA_PCI_AWCR;
	pdata->awarcr = XGBE_DMA_PCI_AWARCR;

	/* Set the hardware channel and queue counts */
	xgbe_set_counts(pdata);

	/* Configure interrupt support */
	ret = xgbe_config_irqs(pdata);
	if (ret)
		goto err_pci_enable;

#if XGBE_SRIOV_PF
#ifdef CONFIG_PCI_IOV
	if(1) {
		if (max_vfs > 0) {
			pci_sriov_set_totalvfs(pdev, XGBE_MAX_VFS_DRV_LIMIT);
			if (xgbe_enable_sriov(pdata, max_vfs))
				goto err_irq_vectors;
		}
	}
#endif
#endif

#if XGBE_SRIOV_VF
	if(1) {
		pdata->mbx.request_irq(pdata);
		TEST_PRNT("mbx irq done\n");
	}

	pdata->mbx.get_hw_features(pdata);
#endif

#if BRCM_BCMUTIL
    ret = xgbe_bcmutil_misc_driver_register(pdata);
    if (ret) {
        printk("%s: Can't register misc device\n", __func__);
        goto err_irq_vectors;
    }
#endif

	/* Configure the netdev resource */
	ret = xgbe_config_netdev(pdata);
	if (ret)
		goto err_irq_vectors;

#if XGBE_SRIOV_PF
    xgbe_dma_soft_reset(pdata);
#if ELI_ENABLE 
    /* Initializing the ELI BLOCK */
    pdata->eli_if.eli_init(pdata);
#else
    XGMAC_IOWRITE(pdata, MAC_RQC1R, 0x7B2A8B0A);
#endif
#endif

	netdev_notice(pdata->netdev, "net device enabled\n");

	dev_info(dev, "xgbe_config_netdev done\n");

	return 0;

err_irq_vectors:
	pci_free_irq_vectors(pdata->pcidev);

err_pci_enable:
	xgbe_free_pdata(pdata);

err_alloc:
	dev_notice(dev, "net device not enabled\n");

	return ret;
}

#if XGBE_SRIOV_PF
static void dma_reset_tx(struct xgbe_prv_data *pdata, int ch_start_num, int no_of_ch)
{
	unsigned int i, total = ch_start_num + no_of_ch;
	unsigned int reg;
	unsigned int val;

	for(i = ch_start_num; i < total; i++){
		reg = DMA_CH_BASE + (i * DMA_CH_INC);  
		val = XGMAC_IOREAD(pdata, (reg + DMA_CH_TCR)); 
		XGMAC_SET_BITS(val, DMA_CH_TCR, ST, 0);
		XGMAC_IOWRITE(pdata, (reg + DMA_CH_TCR), val);
	}
}

static void dma_reset_rx(struct xgbe_prv_data *pdata, int ch_start_num, int no_of_ch)
{
	unsigned int i, total = ch_start_num + no_of_ch;
	unsigned int timeout = XGBE_IO_TIMEOUT, stat = 0, reg;
	unsigned int val;

	for(i = ch_start_num; i < total; i++){
		reg = DMA_CH_BASE + (i * DMA_CH_INC); 

		val = XGMAC_IOREAD(pdata, (reg + DMA_CH_RCR)); 
		XGMAC_SET_BITS(val, DMA_CH_RCR, SR, 0);
		XGMAC_IOWRITE(pdata, (reg + DMA_CH_RCR), val);

		val = XGMAC_IOREAD(pdata, (reg + DMA_CH_SR));
		stat = XGMAC_GET_BITS(val, DMA_CH_SR, RPS);
		while((!stat) && (timeout != 0)){
			timeout--;
			val = XGMAC_IOREAD(pdata, (reg + DMA_CH_SR));
			stat = XGMAC_GET_BITS(val, DMA_CH_SR, RPS);
		}
		if(timeout == 0)
			TEST_PRNT("Timed out waiting for Rx DMA channel %d to stop\n", i);

		val = XGMAC_IOREAD(pdata, (reg + DMA_CH_RCR));
		XGMAC_SET_BITS(val, DMA_CH_RCR, RPF, 1);
		XGMAC_IOWRITE(pdata, (reg + DMA_CH_RCR), val);

		timeout = XGBE_IO_TIMEOUT;
	}
}

static void rewrite_l3l4_registers(struct xgbe_prv_data *pdata, int register_number, int register_type, __be32 register_value)
{
        XGMAC_IOWRITE(pdata, MAC_L3L4WRR, register_value);
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, IDDR_REGSEL, register_type);
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, IDDR_NUM, register_number);
        // Set TT field to 0
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, TT, 0);

        // Set XB field to 1
        XGMAC_IOWRITE_BITS(pdata, MAC_L3L4ACTL, XB, 1);

        while(XGMAC_IOREAD_BITS(pdata, MAC_L3L4ACTL, XB));
        //printk("xb: %x\n", XGMAC_IOREAD_BITS(pdata, MAC_L3L4ACTL, XB));

}

static void xgbe_l3l4_filter_exit(struct xgbe_prv_data *pdata)
{
        struct hlist_node *node2;
        struct l3l4_filter *filter;

        spin_lock(&pdata->lock);

        hlist_for_each_entry_safe(filter, node2,
                                  &pdata->l3l4_filter_list, l3l4_node) {
                hlist_del(&filter->l3l4_node);
                kfree(filter);
        }
        pdata->l3l4_filter_count = 0;

        spin_unlock(&pdata->lock);
}
#endif

static void xgbe_pci_remove(struct pci_dev *pdev)
{
	struct xgbe_prv_data *pdata = pci_get_drvdata(pdev);
#ifdef CONFIG_PCI_IOV
	unsigned short device;
#endif
#if XGBE_SRIOV_PF
	int j = 0;
#endif
#if XGBE_SRIOV_VF
	unsigned short temp, pos;
	unsigned char stat;
	unsigned int timeout = 1000;	
#endif

#if XGBE_SRIOV_PF
	int i;
	struct vf_data_storage *vfinfo = NULL;

	if(pdata->vfinfo && (pdata->flags & XGBE_FLAG_SRIOV_ENABLED)){
		for(i = 0; i < pdata->num_vfs; i++){
			vfinfo = &pdata->vfinfo[i];

			if(!vfinfo)
				continue;

			if(!vfinfo->enabled)
				continue;

			dma_reset_tx(pdata, vfinfo->dma_channel_start_num, vfinfo->num_dma_channels);
			dma_reset_rx(pdata, vfinfo->dma_channel_start_num, vfinfo->num_dma_channels);
		}
	}

#ifdef CONFIG_PCI_IOV
	pci_read_config_word(pdev, 0x2, &device);
	if(1) {
		mbox_free_irqs(pdata);
		xgbe_disable_sriov(pdata);
	}
#endif
#if BRCM_BCMUTIL
    xgbe_bcmutil_misc_driver_deregister(pdata);
#endif

	xgbe_deconfig_netdev(pdata);

	pci_free_irq_vectors(pdata->pcidev);

	xgbe_free_pdata(pdata);
		/* Clearing all filters */
	if(pdata->phy.link) {
		for (j  = 7; j > -1; j--) {
			rewrite_l3l4_registers(pdata, j, MAC_L3_L4_CONTROL, 0);
			rewrite_l3l4_registers(pdata, j, MAC_L4_ADDRESS	, 0);
			rewrite_l3l4_registers(pdata, j, MAC_L3_ADDRESS0	, 0);
			rewrite_l3l4_registers(pdata, j, MAC_L3_ADDRESS1	, 0);
		}
	}
	/* Freeing Memory consumed if any */
	xgbe_l3l4_filter_exit(pdata);
	/* Disabling the register */
	XGMAC_IOWRITE_BITS(pdata, MAC_PFR, IPFE, 0);
#endif
#if XGBE_SRIOV_VF
	pci_read_config_word(pdev, 0x2, &device);

	pdata->mbx.vf_flr_notice(pdata);
	pdata->mbx.free_irq(pdata);

	xgbe_deconfig_netdev(pdata);	

	pci_free_irq_vectors(pdata->pcidev);

	pci_write_config_word(pdev, 0x4, 0);

	pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);

	
	pci_read_config_byte(pdev, (pos + 0xA), &stat);
	printk("stat %x\n", stat);
	
	while(((stat >> 5) & 0x1) && (timeout != 0)){
		pci_read_config_byte(pdev, (pos + 0xA), &stat);
		printk("stat %x\n", stat);				
		timeout--;
	}
	
	/* FLR */
	XGBE_MSLEEP(1);
	pdata->mbx.clr_mac_addr(pdata);
	TEST_PRNT("Initiate FLR");
	pci_read_config_word(pdev, (pos + 0x8), &temp);
	temp |=  0x8000;
	pci_write_config_word(pdev, (pos + 0x8), temp);

	/* kernel sleep for 100ms */
	XGBE_MSLEEP(100);
	printk("here after msleep\n");

	printk("FLR Done\n");

	xgbe_free_pdata(pdata);
#endif
}

#if 0
static int xgbe_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int ret = 0;

#if XGBE_SRIOV_PF
	struct xgbe_prv_data *pdata = pci_get_drvdata(pdev);
	struct net_device *netdev = pdata->netdev;

	if (netif_running(netdev))
		ret = xgbe_powerdown(netdev, XGMAC_DRIVER_CONTEXT);

	pdata->lpm_ctrl = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_CTRL1);
	pdata->lpm_ctrl |= MDIO_CTRL1_LPOWER;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_CTRL1, pdata->lpm_ctrl);
#endif

	return ret;
}

static int xgbe_pci_resume(struct pci_dev *pdev)
{
	int ret = 0;

#if XGBE_SRIOV_PF
	struct xgbe_prv_data *pdata = pci_get_drvdata(pdev);
	struct net_device *netdev = pdata->netdev;

	/* Re-initializing registers reset by PERST */
	pci_write_config_dword(pdev, MMC_RX64OCTETS_GB_LO, MMC_RX64OCTETS_GB_LO_VAL);
	pci_write_config_dword(pdev, MMC_RX64OCTETS_GB_HI, MMC_RX64OCTETS_GB_HI_VAL);

	pdata->lpm_ctrl &= ~MDIO_CTRL1_LPOWER;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_CTRL1, pdata->lpm_ctrl);

	if (netif_running(netdev)) {
		ret = xgbe_powerup(netdev, XGMAC_DRIVER_CONTEXT);

		/* Schedule a restart in case the link or phy state changed
		 * while we were powered down.
		 */
		schedule_work(&pdata->restart_work);
	}
#endif

	return ret;
}
#endif /* CONFIG_PM */

static const struct xgbe_version_data xgbe_v2a = {
#if XGBE_SRIOV_PF
	.init_function_ptrs_phy_impl = xgbe_init_function_ptrs_phy,
	.mmc_64bit              = 1,
	.tx_tstamp_workaround	= 1,
#endif
	.tx_desc_prefetch		= XGBE_TX_DECS_PREFETCH,
	.rx_desc_prefetch		= XGBE_RX_DECS_PREFETCH,
};

static const struct pci_device_id xgbe_pci_table[] = {
#if XGBE_SRIOV_PF
	{ PCI_VDEVICE(BROADCOM, 0xa005),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
	{ PCI_VDEVICE(BROADCOM, 0xa006),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
	{ PCI_VDEVICE(BROADCOM, 0xa008),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
	{ PCI_VDEVICE(BROADCOM, 0xa00b),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
#endif

#if XGBE_SRIOV_VF
	{ PCI_VDEVICE(BROADCOM, 0xa007),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
	{ PCI_VDEVICE(BROADCOM, 0xa009),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
	{ PCI_VDEVICE(BROADCOM, 0xa00c),
		.driver_data = (kernel_ulong_t)&xgbe_v2a },
#endif
	/* Last entry must be zero */
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, xgbe_pci_table);

static struct pci_driver xgbe_driver = {
	.name = XGBE_DRV_NAME,
	.id_table = xgbe_pci_table,
	.probe = xgbe_pci_probe,
	.remove = xgbe_pci_remove,
#if 0
	.suspend = xgbe_pci_suspend,
	.resume = xgbe_pci_resume,
#endif
};

static ssize_t phy_bcm89272_reset_gpio_state_show(struct device_driver *drv, char *buf)
{
	int value = -1;

	value = gpio_get_value(480);
	if (value >= 0) {
		return scnprintf(buf, PAGE_SIZE, "gpio state: %d\n", value);
	} else {
		return scnprintf(buf, PAGE_SIZE, "error, gpio179: 480, state: %d\n", value);
	}
}

static ssize_t phy_bcm89272_reset_gpio_state_store(struct device_driver *drv, const char *buf, size_t count)
{
	if (buf[0] == '1') {
		gpio_set_value(480, 1);
	} else if (buf[0] == '0') {
		gpio_set_value(480, 0);
	}
	msleep(25);
	return count;
}
static DRIVER_ATTR_RW(phy_bcm89272_reset_gpio_state);

static ssize_t phy_bcm89272_interrupt_gpio_state_show(struct device_driver *drv, char *buf)
{
	int value = -1;

	if (g_device != NULL) {
		g_interrupt_gpio = of_get_named_gpio(g_device->dev.of_node, "interrupt-gpio", 0);
	}
	if (g_interrupt_gpio < 0) {
		g_interrupt_gpio = 378;
		printk(KERN_ERR "set interrupt_gpio = 378\n");
	}
	gpio_request(g_interrupt_gpio, "interrupt-gpio");
	gpio_direction_input(g_interrupt_gpio);

	value = gpio_get_value(g_interrupt_gpio);
	gpio_free(g_interrupt_gpio);
	if (value >= 0) {
		return scnprintf(buf, PAGE_SIZE, "interrupt gpio state: %d\n", value);
	} else {
		return scnprintf(buf, PAGE_SIZE, "error, gpio: %d, state: %d\n", g_interrupt_gpio, value);
	}
}
static DRIVER_ATTR_RO(phy_bcm89272_interrupt_gpio_state);

static void diagnosis_sysfs_init(void) {
	int status;

	status = driver_create_file(&(xgbe_driver.driver), &driver_attr_phy_bcm89272_reset_gpio_state);
	if (status < 0) {
		status = -ENOENT;
	}
	status = driver_create_file(&(xgbe_driver.driver), &driver_attr_phy_bcm89272_interrupt_gpio_state);
	if (status < 0) {
		status = -ENOENT;
	}
}
extern int msm_pcie_enable_rc(u32 rc_idx);
extern int msm_pcie_enumerate(u32 rc_idx);

int xgbe_pci_init(void)
{
	int ret = 0;
  	int resetgpionum = 480;

	gpio_request(resetgpionum, "reset-gpio");
	gpio_direction_output(resetgpionum, 0);
	printk(KERN_ERR "%s: reset-gpio = 480 \n", __func__);
	gpio_set_value(resetgpionum, 1);
	usleep_range(25000, 25005);
	gpio_set_value(resetgpionum, 0);
	usleep_range(2000000, 2000005);
	msm_pcie_enable_rc(1);
  	/* enumerate it on PCIE */
        ret = msm_pcie_enumerate(1);
        if (ret < 0) {
		printk(KERN_ERR "here msm pcie enumerate brcm_xgbe error.\n");
        }
	ret = pci_register_driver(&xgbe_driver);
  
	diagnosis_sysfs_init();

	return ret;
}

void xgbe_pci_exit(void)
{
	pci_unregister_driver(&xgbe_driver);
}
