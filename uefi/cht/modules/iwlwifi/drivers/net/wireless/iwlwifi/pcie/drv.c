/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci-aspm.h>
#include <linux/acpi.h>

#ifdef CPTCFG_IWLWIFI_PLATFORM_DATA
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/platform_data/iwlwifi.h>
#endif /* CPTCFG_IWLWIFI_PLATFORM_DATA */
#include "iwl-trans.h"
#include "iwl-drv.h"
#include "internal.h"

#define IWL_PCI_DEVICE(dev, subdev, cfg) \
	.vendor = PCI_VENDOR_ID_INTEL,  .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = (subdev), \
	.driver_data = (kernel_ulong_t)&(cfg)

/* Hardware specific file defines the PCI IDs table for that hardware module */
static const struct pci_device_id iwl_hw_card_ids[] = {

#if IS_ENABLED(CPTCFG_IWLMVM)
/* 7260 Series */
	{IWL_PCI_DEVICE(0x08B1, 0x4070, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4072, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4170, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4C60, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4C70, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4060, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x406A, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4160, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4062, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4162, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4270, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4272, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4260, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x426A, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4262, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4470, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4472, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4460, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x446A, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4462, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4870, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x486E, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4A70, iwl7260_2ac_cfg_high_temp)},
	{IWL_PCI_DEVICE(0x08B1, 0x4A6E, iwl7260_2ac_cfg_high_temp)},
	{IWL_PCI_DEVICE(0x08B1, 0x4A6C, iwl7260_2ac_cfg_high_temp)},
	{IWL_PCI_DEVICE(0x08B1, 0x4570, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4560, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4370, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4360, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x5070, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x5072, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x5170, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x5770, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4020, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x402A, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0x4220, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0x4420, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC070, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC072, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC170, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC060, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC06A, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC160, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC062, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC162, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC770, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC760, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC270, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xCC70, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xCC60, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC272, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC260, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC26A, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC262, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC470, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC472, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC460, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC462, iwl7260_n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC570, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC560, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC370, iwl7260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC360, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC020, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC02A, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B2, 0xC220, iwl7260_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B1, 0xC420, iwl7260_2n_cfg)},

/* 3160 Series */
	{IWL_PCI_DEVICE(0x08B3, 0x0070, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0072, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0170, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0172, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0060, iwl3160_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0062, iwl3160_n_cfg)},
	{IWL_PCI_DEVICE(0x08B4, 0x0270, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B4, 0x0272, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0470, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x0472, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B4, 0x0370, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8070, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8072, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8170, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8172, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8060, iwl3160_2n_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8062, iwl3160_n_cfg)},
	{IWL_PCI_DEVICE(0x08B4, 0x8270, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B4, 0x8370, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B4, 0x8272, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8470, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x8570, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x1070, iwl3160_2ac_cfg)},
	{IWL_PCI_DEVICE(0x08B3, 0x1170, iwl3160_2ac_cfg)},

/* 3165 Series */
	{IWL_PCI_DEVICE(0x3165, 0x4010, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3165, 0x4012, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3165, 0x4410, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3165, 0x4510, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3165, 0x4110, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3166, 0x4310, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3166, 0x4210, iwl3165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x3165, 0x8010, iwl3165_2ac_cfg)},

/* 7265 Series */
	{IWL_PCI_DEVICE(0x095A, 0x5010, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5110, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5100, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x5310, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x5302, iwl7265_n_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x5210, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5012, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5412, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5410, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5510, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5400, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x1010, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5000, iwl7265_2n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x500A, iwl7265_2n_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x5200, iwl7265_2n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5002, iwl7265_n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5102, iwl7265_n_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x5202, iwl7265_n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9010, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9012, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x900A, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9110, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9112, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9210, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x9200, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9510, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9310, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x9410, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5020, iwl7265_2n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x502A, iwl7265_2n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5420, iwl7265_2n_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5090, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5190, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5590, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095B, 0x5290, iwl7265_2ac_cfg)},
	{IWL_PCI_DEVICE(0x095A, 0x5490, iwl7265_2ac_cfg)},

/* 8000 Series */
	{IWL_PCI_DEVICE(0x24F3, 0x0000, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0010, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x1010, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0110, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x1110, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0050, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0250, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x1050, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0150, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0x0030, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0x1130, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0x1030, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0xC010, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0xD010, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0xC030, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0xD030, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0xC050, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0xD050, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x8010, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x9010, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0x8030, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F4, 0x9030, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x8050, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x9050, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0004, iwl8260_2n_cfg)},
	{IWL_PCI_DEVICE(0x24F5, 0x0010, iwl4165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F6, 0x0030, iwl4165_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0810, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0910, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0850, iwl8260_2ac_cfg)},
	{IWL_PCI_DEVICE(0x24F3, 0x0950, iwl8260_2ac_cfg)},
#endif /* CPTCFG_IWLMVM */

	{0}
};
MODULE_DEVICE_TABLE(pci, iwl_hw_card_ids);

#ifdef CONFIG_ACPI
#define SPL_METHOD		"SPLC"
#define SPL_DOMAINTYPE_MODULE	BIT(0)
#define SPL_DOMAINTYPE_WIFI	BIT(1)
#define SPL_DOMAINTYPE_WIGIG	BIT(2)
#define SPL_DOMAINTYPE_RFEM	BIT(3)

static u64 splx_get_pwr_limit(struct iwl_trans *trans, union acpi_object *splx)
{
	union acpi_object *limits, *domain_type, *power_limit;

	if (splx->type != ACPI_TYPE_PACKAGE ||
	    splx->package.count != 2 ||
	    splx->package.elements[0].type != ACPI_TYPE_INTEGER ||
	    splx->package.elements[0].integer.value != 0) {
		IWL_ERR(trans, "Unsupported splx structure\n");
		return 0;
	}

	limits = &splx->package.elements[1];
	if (limits->type != ACPI_TYPE_PACKAGE ||
	    limits->package.count < 2 ||
	    limits->package.elements[0].type != ACPI_TYPE_INTEGER ||
	    limits->package.elements[1].type != ACPI_TYPE_INTEGER) {
		IWL_ERR(trans, "Invalid limits element\n");
		return 0;
	}

	domain_type = &limits->package.elements[0];
	power_limit = &limits->package.elements[1];
	if (!(domain_type->integer.value & SPL_DOMAINTYPE_WIFI)) {
		IWL_DEBUG_INFO(trans, "WiFi power is not limited\n");
		return 0;
	}

	return power_limit->integer.value;
}

static void set_dflt_pwr_limit(struct iwl_trans *trans, struct pci_dev *pdev)
{
	acpi_handle pxsx_handle;
	acpi_handle handle;
	struct acpi_buffer splx = {ACPI_ALLOCATE_BUFFER, NULL};
	acpi_status status;

	pxsx_handle = ACPI_HANDLE(&pdev->dev);
	if (!pxsx_handle) {
		IWL_DEBUG_INFO(trans,
			       "Could not retrieve root port ACPI handle\n");
		return;
	}

	/* Get the method's handle */
	status = acpi_get_handle(pxsx_handle, (acpi_string)SPL_METHOD, &handle);
	if (ACPI_FAILURE(status)) {
		IWL_DEBUG_INFO(trans, "SPL method not found\n");
		return;
	}

	/* Call SPLC with no arguments */
	status = acpi_evaluate_object(handle, NULL, NULL, &splx);
	if (ACPI_FAILURE(status)) {
		IWL_ERR(trans, "SPLC invocation failed (0x%x)\n", status);
		return;
	}

	trans->dflt_pwr_limit = splx_get_pwr_limit(trans, splx.pointer);
	IWL_DEBUG_INFO(trans, "Default power limit set to %lld\n",
		       trans->dflt_pwr_limit);
	kfree(splx.pointer);
}

#else /* CONFIG_ACPI */
static void set_dflt_pwr_limit(struct iwl_trans *trans, struct pci_dev *pdev) {}
#endif

/* PCI registers */
#define PCI_CFG_RETRY_TIMEOUT	0x041

static int iwl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	const struct iwl_cfg *cfg = (struct iwl_cfg *)(ent->driver_data);
	const struct iwl_cfg *cfg_7265d __maybe_unused = NULL;
	struct iwl_trans *iwl_trans;
	struct iwl_trans_pcie *trans_pcie;
	int ret;
#ifdef CPTCFG_IWLWIFI_PLATFORM_DATA
	struct device *dev;
	struct platform_device *plat_dev;
#endif

	iwl_trans = iwl_trans_pcie_alloc(pdev, ent, cfg);
	if (IS_ERR(iwl_trans))
		return PTR_ERR(iwl_trans);

#if IS_ENABLED(CPTCFG_IWLMVM)
	/*
	 * special-case 7265D, it has the same PCI IDs.
	 *
	 * Note that because we already pass the cfg to the transport above,
	 * all the parameters that the transport uses must, until that is
	 * changed, be identical to the ones in the 7265D configuration.
	 */
	if (cfg == &iwl7265_2ac_cfg)
		cfg_7265d = &iwl7265d_2ac_cfg;
	else if (cfg == &iwl7265_2n_cfg)
		cfg_7265d = &iwl7265d_2n_cfg;
	else if (cfg == &iwl7265_n_cfg)
		cfg_7265d = &iwl7265d_n_cfg;
	if (cfg_7265d &&
	    (iwl_trans->hw_rev & CSR_HW_REV_TYPE_MSK) == CSR_HW_REV_TYPE_7265D) {
		cfg = cfg_7265d;
		iwl_trans->cfg = cfg_7265d;
	}
#endif

	pci_set_drvdata(pdev, iwl_trans);

	trans_pcie = IWL_TRANS_GET_PCIE_TRANS(iwl_trans);
	trans_pcie->drv = iwl_drv_start(iwl_trans, cfg);

	if (IS_ERR(trans_pcie->drv)) {
		ret = PTR_ERR(trans_pcie->drv);
		goto out_free_trans;
	}

	set_dflt_pwr_limit(iwl_trans, pdev);

	/* register transport layer debugfs here */
	ret = iwl_trans_dbgfs_register(iwl_trans, iwl_trans->dbgfs_dir);
	if (ret)
		goto out_free_drv;

#ifdef CPTCFG_IWLWIFI_PLATFORM_DATA
	dev = bus_find_device_by_name(&platform_bus_type, NULL,
				      IWLWIFI_PLATFORM_NAME);
	if (dev) {
		plat_dev = to_platform_device(dev);
		if (plat_dev && plat_dev->dev.platform_data) {
			IWL_DEBUG_INFO(iwl_trans,
				       "Platform device found\n");
			trans_pcie->platform_ops = plat_dev->dev.platform_data;
		}
	}
#endif /* CPTCFG_IWLWIFI_PLATFORM_DATA */
	return 0;

out_free_drv:
	iwl_drv_stop(trans_pcie->drv);
out_free_trans:
	iwl_trans_pcie_free(iwl_trans);
	return ret;
}

static void iwl_pci_remove(struct pci_dev *pdev)
{
	struct iwl_trans *trans = pci_get_drvdata(pdev);
	struct iwl_trans_pcie *trans_pcie = IWL_TRANS_GET_PCIE_TRANS(trans);

	iwl_drv_stop(trans_pcie->drv);
	iwl_trans_pcie_free(trans);
}

#ifdef CONFIG_PM_SLEEP

static int iwl_pci_suspend(struct device *device)
{
	/* Before you put code here, think about WoWLAN. You cannot check here
	 * whether WoWLAN is enabled or not, and your code will run even if
	 * WoWLAN is enabled - don't kill the NIC, someone may need it in Sx.
	 */
#ifdef CPTCFG_IWLWIFI_PCIE_SUSPEND_RESUME
	struct pci_dev *pdev = to_pci_dev(device);

	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);
#endif

	return 0;
}

static int iwl_pci_resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct iwl_trans *trans = pci_get_drvdata(pdev);
	bool hw_rfkill;

#ifdef CPTCFG_IWLWIFI_PCIE_SUSPEND_RESUME
	int r;

	pci_set_power_state(pdev, PCI_D0);
	r = pci_enable_device(pdev);
	if (r)
		return r;
	pci_restore_state(pdev);
#endif
	/* Before you put code here, think about WoWLAN. You cannot check here
	 * whether WoWLAN is enabled or not, and your code will run even if
	 * WoWLAN is enabled - the NIC may be alive.
	 */

	/*
	 * We disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state.
	 */
	pci_write_config_byte(pdev, PCI_CFG_RETRY_TIMEOUT, 0x00);

	if (!trans->op_mode)
		return 0;

	iwl_enable_rfkill_int(trans);

	hw_rfkill = iwl_is_rfkill_set(trans);
	iwl_trans_pcie_rf_kill(trans, hw_rfkill);

	return 0;
}

static SIMPLE_DEV_PM_OPS(iwl_dev_pm_ops, iwl_pci_suspend, iwl_pci_resume);

#define IWL_PM_OPS	(&iwl_dev_pm_ops)

#else

#define IWL_PM_OPS	NULL

#endif

static struct pci_driver iwl_pci_driver = {
	.name = DRV_NAME,
	.id_table = iwl_hw_card_ids,
	.probe = iwl_pci_probe,
	.remove = iwl_pci_remove,
	.driver.pm = IWL_PM_OPS,
};

int __must_check iwl_pci_register_driver(void)
{
	int ret;
	ret = pci_register_driver(&iwl_pci_driver);
	if (ret)
		pr_err("Unable to initialize PCI module\n");

	return ret;
}

void iwl_pci_unregister_driver(void)
{
	pci_unregister_driver(&iwl_pci_driver);
}
