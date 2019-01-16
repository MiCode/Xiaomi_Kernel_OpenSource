/*
 * xHCI host controller driver PCI Bus Glue.
 *
 * Copyright (C) 2008 Intel Corp.
 *
 * Author: Sarah Sharp
 * Some code borrowed from the Linux EHCI driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/pci.h>
#include "xhci.h"
#include "mtk-usb-hcd.h"
#include "mtk-test.h"

static const char hcd_name[] = "xhci_hcd";

/*-------------------------------------------------------------------------*/

static const struct hc_driver xhci_pci_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"xHCI MTK Test Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			mtktest_xhci_mtk_irq,
	.flags =		HCD_MEMORY | HCD_USB3,

	/*
	 * basic lifecycle operations
	 */
	.reset =		xhci_mtk_pci_setup,
	.start =		mtktest_xhci_mtk_run,
	/* suspend and resume implemented later */
	.stop =			mtktest_xhci_mtk_stop,
	.shutdown =		mtktest_xhci_mtk_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		mtktest_xhci_mtk_urb_enqueue,
	.urb_dequeue =		mtktest_xhci_mtk_urb_dequeue,
	.alloc_dev =		mtktest_xhci_mtk_alloc_dev,
	.free_dev =		mtktest_xhci_mtk_free_dev,
	.alloc_streams =	mtktest_xhci_mtk_alloc_streams,
	.free_streams =		mtktest_xhci_mtk_free_streams,
	.add_endpoint =		mtktest_xhci_mtk_add_endpoint,
	.drop_endpoint =	mtktest_xhci_mtk_drop_endpoint,
	.endpoint_reset =	mtktest_xhci_mtk_endpoint_reset,
	.check_bandwidth =	mtktest_xhci_mtk_check_bandwidth,
	.reset_bandwidth =	mtktest_xhci_mtk_reset_bandwidth,
	.address_device =	mtktest_xhci_mtk_address_device,
	.update_hub_device =	mtktest_xhci_mtk_update_hub_device,
	.reset_device =		mtktest_xhci_mtk_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	mtktest_xhci_mtk_get_frame,

	/* Root hub support */
	.hub_control =		mtktest_xhci_mtk_hub_control,
	.hub_status_data =	mtktest_xhci_mtk_hub_status_data,
};

/*-------------------------------------------------------------------------*/

/* PCI driver selection metadata; PCI hotplugging uses this */
static const struct pci_device_id pci_ids[] = { {
	/* handle any USB 3.0 xHCI controller */
	PCI_DEVICE_CLASS(PCI_CLASS_SERIAL_USB_XHCI, ~0),
	.driver_data =	(unsigned long) &xhci_pci_hc_driver,
	},
	{ /* end: all zeroes */ }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver xhci_pci_driver = {
	.name =		(char *) hcd_name,
	.id_table =	pci_ids,

	.probe =	mtk_usb_hcd_pci_probe,
	.remove =	mtk_usb_hcd_pci_remove,
	/* suspend and resume implemented later */

	.shutdown = 	mtk_usb_hcd_pci_shutdown,
};

int xhci_register_pci(void)
{
	return pci_register_driver(&xhci_pci_driver);
}

void xhci_unregister_pci(void)
{
	pci_unregister_driver(&xhci_pci_driver);
}
