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
//add by lty
//#include <linux/pci.h>

#include <linux/platform_device.h>
#include <linux/clk.h>
//#include <plat/usb.h>
#include "xhci.h"
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include "mtk-test-lib.h"
#include "mtk-usb-hcd.h"

#include <linux/interrupt.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

/* Device for a quirk */
static const char hcd_name[] = "xhc_mtk";


/*-------------------------------------------------------------------------*/

/* called during probe() after chip reset completes */
static int xhci_mtk_setup(struct usb_hcd *hcd)
{
	struct xhci_hcd		*xhci;
//	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	int			retval;
	u32			temp;

	hcd->self.sg_tablesize = TRBS_PER_SEGMENT - 2;
		/* xHCI private pointer was set in xhci_pci_probe for the second
		 * registered roothub.
		 */
	xhci = hcd_to_xhci(hcd);
	xhci->cap_regs = hcd->regs;
	xhci->op_regs = hcd->regs +
		HC_LENGTH(xhci_readl(xhci, &xhci->cap_regs->hc_capbase));
	xhci->run_regs = hcd->regs +
		(xhci_readl(xhci, &xhci->cap_regs->run_regs_off) & RTSOFF_MASK);
	/* Cache read-only capability registers */
	xhci->hcs_params1 = xhci_readl(xhci, &xhci->cap_regs->hcs_params1);
	xhci->hcs_params2 = xhci_readl(xhci, &xhci->cap_regs->hcs_params2);
	xhci->hcs_params3 = xhci_readl(xhci, &xhci->cap_regs->hcs_params3);
	xhci->hcc_params = xhci_readl(xhci, &xhci->cap_regs->hc_capbase);
	xhci->hci_version = HC_VERSION(xhci->hcc_params);
	xhci->hcc_params = xhci_readl(xhci, &xhci->cap_regs->hcc_params);
	mtktest_xhci_print_registers(xhci);

	/* Make sure the HC is halted. */
	retval = mtktest_xhci_halt(xhci);
	if (retval)
		goto error;

	xhci_dbg(xhci, "Resetting HCD\n");
	/* Reset the internal HC memory state and registers. */
	retval = mtktest_xhci_reset(xhci);
	if (retval)
		goto error;
	xhci_dbg(xhci, "Reset complete\n");
	xhci_dbg(xhci, "Calling HCD init\n");

	mtktest_setInitialReg();

	/* Initialize HCD and host controller data structures. */
	retval = mtktest_xhci_init(hcd);
	if (retval)
		goto error;
	xhci_dbg(xhci, "Called HCD init\n");
	return retval;
error:
	kfree(xhci);
	return retval;
}


static const struct hc_driver xhci_versatile_hc_driver;
/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */

/**
 * ehci_hcd_omap_probe - initialize TI-based HCDs
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 */
static int usb_hcd_versatile_probe(struct platform_device *pdev)
{
		//struct ehci_hcd_omap_platform_data *pdata = pdev->dev.platform_data;
	//	struct xhci_hcd_versatile *vers;
	//	struct resource *res;
		struct usb_hcd *hcd;
		int i;
		int irq;
		char supply[7];
		struct device_node *node = NULL;
		struct resource res;

		int ret = -ENODEV;
		printk("hcd_versatile_probe is called\n");
		/* Chiachun: don't u$se platform_device API first */

		/* got related info from dtsi */
		node = of_find_compatible_node(NULL, NULL, USB_XHCI_COMPATIBLE_NAME);
		if(node == NULL){
			printk(KERN_ERR "xhci_test get node failed\n");
			return ret;
		}
		if(of_address_to_resource(node, 0, &res)){
			printk(KERN_ERR "xhci_test get res failed\n");
			return ret;
		}

		/* special resource get*/
		irq = xhci_usbif_nirq;		

		hcd = mtk_usb_create_hcd(&xhci_versatile_hc_driver, &pdev->dev,
				dev_name(&pdev->dev));
		if (!hcd) {
			dev_dbg(&pdev->dev, "failed to create hcd with err %d\n", ret);
			ret = -ENOMEM;
			goto err_create_hcd;
		}
		printk(KERN_ERR "Creat HCD success!\n");
		hcd->rsrc_start = res.start;
		hcd->rsrc_len = resource_size(&res);

        printk(KERN_ERR "====%s(%d)==== hcd->pool[0] = 0x%p!\n", __func__, __LINE__, hcd->pool[0]);

		printk(KERN_ERR "Create xHC Base address 0x%p!\n", (void *)(unsigned long)hcd->rsrc_start);
		/* special resource get*/
		hcd->regs = xhci_usbif_base;	


		if (!hcd->regs) {
			dev_err(&pdev->dev, "XHCI ioremap failed\n");
			ret = -ENOMEM;
			goto err_ioremap;
		}

		hcd->self.sg_tablesize = TRBS_PER_SEGMENT - 2;
		hcd->self.uses_dma = 1;

		/* we know this is the memory we want, no need to ioremap again */

		ret = mtk_usb_add_hcd(hcd, irq, IRQF_SHARED | IRQF_TRIGGER_LOW);
		my_hcd = hcd;
		if (ret) {
			dev_dbg(&pdev->dev, "failed to add hcd with err %d\n", ret);
			goto err_add_hcd;
		}
        printk(KERN_ERR "====%s(%d)==== hcd->pool[0] = 0x%p!\n", __func__, __LINE__, hcd->pool[0]);
		printk(KERN_INFO "usb_add_hcd success!\n");
		return 0;
	err_add_hcd:
	err_start:
	err_tll_ioremap:
	err_uhh_ioremap:
		iounmap(hcd->regs);
	err_ioremap:
		usb_put_hcd(hcd);
	err_create_hcd:
	err_disabled:
	err_pdata:
		return ret;

}


static int usb_hcd_versatile_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	printk("hcd_versatile_remove is called\n");
	hcd = platform_get_drvdata (pdev);
	mtk_usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	// USBIF , should we free hcd ?  or it will be clean by kernel cause usb_put_hcd?
//	kfree(hcd);
//	kfree(vers);
	printk("hcd_versatile_remove is completed\n");

	/* special resource release */	
	iounmap(xhci_usbif_base);
	iounmap(xhci_usbif_sif_base);
	return 0;
}

/* called during probe() after chip reset completes */
static int xhci_setup(struct usb_hcd *hcd)
{
	struct xhci_hcd		*xhci;
//	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	int			retval;
	u32			temp;

	hcd->self.sg_tablesize = TRBS_PER_SEGMENT - 2;
		/* xHCI private pointer was set in xhci_pci_probe for the second
		 * registered roothub.
		 */
		xhci = hcd_to_xhci(hcd);
	xhci->cap_regs = hcd->regs;
	xhci->op_regs = hcd->regs +
		HC_LENGTH(xhci_readl(xhci, &xhci->cap_regs->hc_capbase));
	xhci->run_regs = hcd->regs +
		(xhci_readl(xhci, &xhci->cap_regs->run_regs_off) & RTSOFF_MASK);
	/* Cache read-only capability registers */
	xhci->hcs_params1 = xhci_readl(xhci, &xhci->cap_regs->hcs_params1);
	xhci->hcs_params2 = xhci_readl(xhci, &xhci->cap_regs->hcs_params2);
	xhci->hcs_params3 = xhci_readl(xhci, &xhci->cap_regs->hcs_params3);
	xhci->hcc_params = xhci_readl(xhci, &xhci->cap_regs->hc_capbase);
	xhci->hci_version = HC_VERSION(xhci->hcc_params);
	xhci->hcc_params = xhci_readl(xhci, &xhci->cap_regs->hcc_params);
	mtktest_xhci_print_registers(xhci);
	/* Make sure the HC is halted. */
	retval = mtktest_xhci_halt(xhci);
	if (retval)
		goto error;

	xhci_dbg(xhci, "Resetting HCD\n");
	/* Reset the internal HC memory state and registers. */
	retval = mtktest_xhci_reset(xhci);
	if (retval)
		goto error;
	xhci_dbg(xhci, "Reset complete\n");
	xhci_dbg(xhci, "Calling HCD init\n");
	/* Initialize HCD and host controller data structures. */
	retval = mtktest_xhci_init(hcd);
	if (retval)
		goto error;
	xhci_dbg(xhci, "Called HCD init\n");
	return retval;
error:
	kfree(xhci);
	return retval;
}


static void usb_hcd_versatile_shutdown(struct platform_device *pdev)
{
	printk("hcd_versatile_shutdown is called\n");
	if (my_hcd->driver->shutdown)
		my_hcd->driver->shutdown(my_hcd);
	printk("hcd_versatile_shutdown is completed\n");
}
static const struct hc_driver xhci_versatile_hc_driver = {
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
	.reset =		xhci_mtk_setup,
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
static void xhci_hcd_release (struct device *dev)
{
	printk("xhci_hcd_release \n");
	dev->parent = NULL;
//    printk(KERN_INFO, "dev = 0x%08X.\n", (uint32_t)dev);

}

static struct platform_driver xhci_versatile_driver = {

	.probe =	usb_hcd_versatile_probe,
	.remove =	usb_hcd_versatile_remove,
	.shutdown = 	usb_hcd_versatile_shutdown,

	.driver = {
		.name =		(char *) hcd_name,
	}
};

