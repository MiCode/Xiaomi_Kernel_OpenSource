/**
 * dwc3_otg.c - DesignWare USB3 DRD Controller OTG
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/platform_device.h>

#include "core.h"
#include "dwc3_otg.h"
#include "io.h"
#include "xhci.h"


/**
 * dwc3_otg_set_host_regs - reset dwc3 otg registers to host operation.
 *
 * This function sets the OTG registers to work in A-Device host mode.
 * This function should be called just before entering to A-Device mode.
 *
 * @w: Pointer to the dwc3 otg workqueue.
 */
static void dwc3_otg_set_host_regs(struct dwc3_otg *dotg)
{
	u32 octl;

	/* Set OCTL[6](PeriMode) to 0 (host) */
	octl = dwc3_readl(dotg->regs, DWC3_OCTL);
	octl &= ~DWC3_OTG_OCTL_PERIMODE;
	dwc3_writel(dotg->regs, DWC3_OCTL, octl);

	/*
	 * TODO: add more OTG registers writes for HOST mode here,
	 * see figure 12-10 A-device flow in dwc3 Synopsis spec
	 */
}

/**
 * dwc3_otg_set_peripheral_regs - reset dwc3 otg registers to peripheral operation.
 *
 * This function sets the OTG registers to work in B-Device peripheral mode.
 * This function should be called just before entering to B-Device mode.
 *
 * @w: Pointer to the dwc3 otg workqueue.
 */
static void dwc3_otg_set_peripheral_regs(struct dwc3_otg *dotg)
{
	u32 octl;

	/* Set OCTL[6](PeriMode) to 1 (peripheral) */
	octl = dwc3_readl(dotg->regs, DWC3_OCTL);
	octl |= DWC3_OTG_OCTL_PERIMODE;
	dwc3_writel(dotg->regs, DWC3_OCTL, octl);

	/*
	 * TODO: add more OTG registers writes for PERIPHERAL mode here,
	 * see figure 12-19 B-device flow in dwc3 Synopsis spec
	 */
}

/**
 * dwc3_otg_start_host -  helper function for starting/stoping the host controller driver.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @on: start / stop the host controller driver.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_host(struct usb_otg *otg, int on)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);
	struct usb_hcd *hcd;
	struct xhci_hcd *xhci;
	int ret = 0;

	if (!otg->host)
		return -EINVAL;

	hcd = bus_to_hcd(otg->host);
	xhci = hcd_to_xhci(hcd);
	if (on) {
		dev_dbg(otg->phy->dev, "%s: turn on host %s\n",
					__func__, otg->host->bus_name);
		dwc3_otg_set_host_regs(dotg);

		/*
		 * This should be revisited for more testing post-silicon.
		 * In worst case we may need to disconnect the root hub
		 * before stopping the controller so that it does not
		 * interfere with runtime pm/system pm.
		 * We can also consider registering and unregistering xhci
		 * platform device. It is almost similar to add_hcd and
		 * remove_hcd, But we may not use standard set_host method
		 * anymore.
		 */
		ret = hcd->driver->start(hcd);
		if (ret) {
			dev_err(otg->phy->dev,
				"%s: failed to start primary hcd, ret=%d\n",
				__func__, ret);
			return ret;
		}

		ret = xhci->shared_hcd->driver->start(xhci->shared_hcd);
		if (ret) {
			dev_err(otg->phy->dev,
				"%s: failed to start secondary hcd, ret=%d\n",
				__func__, ret);
			return ret;
		}
	} else {
		dev_dbg(otg->phy->dev, "%s: turn off host %s\n",
					__func__, otg->host->bus_name);
		hcd->driver->stop(hcd);
	}

	return 0;
}

/**
 * dwc3_otg_set_host -  bind/unbind the host controller driver.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @host: Pointer to the usb_bus structure.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	if (host) {
		dev_dbg(otg->phy->dev, "%s: set host %s\n",
					__func__, host->bus_name);
		otg->host = host;

		/*
		 * Only after both peripheral and host are set then check
		 * OTG sm. This prevents unnecessary activation of the sm
		 * in case the ID is high.
		 */
		if (otg->gadget)
			schedule_work(&dotg->sm_work);
	} else {
		if (otg->phy->state == OTG_STATE_A_HOST) {
			dwc3_otg_start_host(otg, 0);
			otg->host = NULL;
			otg->phy->state = OTG_STATE_UNDEFINED;
			schedule_work(&dotg->sm_work);
		} else {
			otg->host = NULL;
		}
	}

	return 0;
}

/**
 * dwc3_otg_start_peripheral -  bind/unbind the peripheral controller.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @gadget: pointer to the usb_gadget structure.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_peripheral(struct usb_otg *otg, int on)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	if (!otg->gadget)
		return -EINVAL;

	if (on) {
		dev_dbg(otg->phy->dev, "%s: turn on gadget %s\n",
					__func__, otg->gadget->name);
		dwc3_otg_set_peripheral_regs(dotg);
		usb_gadget_vbus_connect(otg->gadget);
	} else {
		dev_dbg(otg->phy->dev, "%s: turn off gadget %s\n",
					__func__, otg->gadget->name);
		usb_gadget_vbus_disconnect(otg->gadget);
	}

	return 0;
}

/**
 * dwc3_otg_set_peripheral -  bind/unbind the peripheral controller driver.
 *
 * @otg: Pointer to the otg_transceiver structure.
 * @gadget: pointer to the usb_gadget structure.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_set_peripheral(struct usb_otg *otg,
				struct usb_gadget *gadget)
{
	struct dwc3_otg *dotg = container_of(otg, struct dwc3_otg, otg);

	if (gadget) {
		dev_dbg(otg->phy->dev, "%s: set gadget %s\n",
					__func__, gadget->name);
		otg->gadget = gadget;

		/*
		 * Only after both peripheral and host are set then check
		 * OTG sm. This prevents unnecessary activation of the sm
		 * in case the ID is grounded.
		 */
		if (otg->host)
			schedule_work(&dotg->sm_work);
	} else {
		if (otg->phy->state == OTG_STATE_B_PERIPHERAL) {
			dwc3_otg_start_peripheral(otg, 0);
			otg->gadget = NULL;
			otg->phy->state = OTG_STATE_UNDEFINED;
			schedule_work(&dotg->sm_work);
		} else {
			otg->gadget = NULL;
		}
	}

	return 0;
}

/**
 * dwc3_otg_interrupt - interrupt handler for dwc3 otg events.
 * @_dotg: Pointer to out controller context structure
 *
 * Returns IRQ_HANDLED on success otherwise IRQ_NONE.
 */
static irqreturn_t dwc3_otg_interrupt(int irq, void *_dotg)
{
	struct dwc3_otg *dotg = (struct dwc3_otg *)_dotg;
	u32 oevt_reg;
	int ret = IRQ_NONE;
	int handled_irqs = 0;

	oevt_reg = dwc3_readl(dotg->regs, DWC3_OEVT);

	if (oevt_reg & DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT) {
		/*
		 * ID sts has changed, read it and later, in the workqueue
		 * function, switch from A to B or from B to A.
		 */
		dotg->osts = dwc3_readl(dotg->regs, DWC3_OSTS);
		if ((dotg->otg.phy->state == OTG_STATE_B_IDLE) ||
		    (dotg->otg.phy->state == OTG_STATE_A_IDLE)) {

			/*
			 * OTG state is ABOUT to change to A or B device, but
			 * since ID sts was chnaged, then we return the state
			 * machine to the start point.
			 */
			 dotg->otg.phy->state = OTG_STATE_UNDEFINED;
		}
		schedule_work(&dotg->sm_work);

		handled_irqs |= DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT;
		ret = IRQ_HANDLED;
	}

	/*
	 * Clear the interrupts we handled.
	 */
	if (ret == IRQ_HANDLED)
		dwc3_writel(dotg->regs, DWC3_OEVT, handled_irqs);

	return ret;
}

/**
 * dwc3_otg_sm_work - workqueue function.
 *
 * @w: Pointer to the dwc3 otg workqueue
 *
 * NOTE: After any change in phy->state,
 * we must reschdule the state machine.
 */
static void dwc3_otg_sm_work(struct work_struct *w)
{
	struct dwc3_otg *dotg = container_of(w, struct dwc3_otg, sm_work);
	struct usb_phy *phy = dotg->otg.phy;

	dev_dbg(phy->dev, "%s state\n", otg_state_string(phy->state));

	/* Check OTG state */
	switch (phy->state) {
	case OTG_STATE_UNDEFINED:
		/* Switch to A or B-Device according to IDSTS */
		if (dotg->osts & DWC3_OTG_OSTS_CONIDSTS)
			phy->state = OTG_STATE_B_IDLE;
		else
			phy->state = OTG_STATE_A_IDLE;

		schedule_work(&dotg->sm_work);
		break;
	case OTG_STATE_B_IDLE:
		if (dwc3_otg_start_peripheral(&dotg->otg, 1)) {
			/*
			 * Probably set_peripheral was not called yet.
			 * We will re-try as soon as it will be called
			 */
			dev_err(phy->dev,
				"unable to start B-device\n");
			phy->state = OTG_STATE_UNDEFINED;
		} else
			phy->state = OTG_STATE_B_PERIPHERAL;

		schedule_work(&dotg->sm_work);
		break;
	case OTG_STATE_B_PERIPHERAL:
		if (!(dotg->osts & DWC3_OTG_OSTS_CONIDSTS)) {
			dwc3_otg_start_peripheral(&dotg->otg, 0);
			phy->state = OTG_STATE_A_IDLE;
			schedule_work(&dotg->sm_work);
		}
		break;
	case OTG_STATE_A_IDLE:
		/* Switch to A-Device*/
		if (dwc3_otg_start_host(&dotg->otg, 1)) {
			/*
			 * Probably set_host was not called yet.
			 * We will re-try as soon as it will be called
			 */
			dev_err(phy->dev,
				"unable to start A-device\n");
			phy->state = OTG_STATE_UNDEFINED;
		} else
			phy->state = OTG_STATE_A_HOST;

		schedule_work(&dotg->sm_work);
		break;
	case OTG_STATE_A_HOST:
		if (dotg->osts & DWC3_OTG_OSTS_CONIDSTS) {
			dwc3_otg_start_host(&dotg->otg, 0);
			phy->state = OTG_STATE_B_IDLE;
			schedule_work(&dotg->sm_work);
		}
		break;
	default:
		dev_err(phy->dev, "%s: invalid otg-state\n", __func__);

	}
}


/**
 * dwc3_otg_reset - reset dwc3 otg registers.
 *
 * @w: Pointer to the dwc3 otg workqueue
 */
static void dwc3_otg_reset(struct dwc3_otg *dotg)
{
	/*
	 * OCFG[2] - OTG-Version = 1
	 * OCFG[1] - HNPCap = 0
	 * OCFG[0] - SRPCap = 0
	 */
	dwc3_writel(dotg->regs, DWC3_OCFG, 0x4);

	/*
	 * OCTL[6] - PeriMode = 1
	 * OCTL[5] - PrtPwrCtl = 0
	 * OCTL[4] - HNPReq = 0
	 * OCTL[3] - SesReq = 0
	 * OCTL[2] - TermSelDLPulse = 0
	 * OCTL[1] - DevSetHNPEn = 0
	 * OCTL[0] - HstSetHNPEn = 0
	 */
	dwc3_writel(dotg->regs, DWC3_OCTL, 0x40);

	/* Clear all otg events (interrupts) indications  */
	dwc3_writel(dotg->regs, DWC3_OEVT, 0xFFFF);

	/* Enable only the ConIDStsChngEn event*/
	dwc3_writel(dotg->regs, DWC3_OEVTEN,
				DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT);

	/* Read OSTS  */
	dotg->osts = dwc3_readl(dotg->regs, DWC3_OSTS);

}

/**
 * dwc3_otg_init - Initializes otg related registers
 * @dwc: Pointer to out controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_otg_init(struct dwc3 *dwc)
{
	u32	reg;
	int ret = 0;
	struct dwc3_otg *dotg;

	dev_dbg(dwc->dev, "dwc3_otg_init\n");

	/*
	 * GHWPARAMS6[10] bit is SRPSupport.
	 * This bit also reflects DWC_USB3_EN_OTG
	 */
	reg = dwc3_readl(dwc->regs, DWC3_GHWPARAMS6);
	if (!(reg & DWC3_GHWPARAMS6_SRP_SUPPORT)) {
		/*
		 * No OTG support in the HW core.
		 * We return 0 to indicate no error, since this is acceptable
		 * situation, just continue probe the dwc3 driver without otg.
		 */
		dev_dbg(dwc->dev, "dwc3_otg address space is not supported\n");
		return 0;
	}

	/* Allocate and init otg instance */
	dotg = kzalloc(sizeof(struct dwc3_otg), GFP_KERNEL);
	if (!dotg) {
		dev_err(dwc->dev, "unable to allocate dwc3_otg\n");
		return -ENOMEM;
	}

	dotg->irq = platform_get_irq(to_platform_device(dwc->dev), 0);
	if (dotg->irq < 0) {
		dev_err(dwc->dev, "%s: missing IRQ\n", __func__);
		ret = -ENODEV;
		goto err1;
	}

	dotg->regs = dwc->regs;

	dotg->otg.set_peripheral = dwc3_otg_set_peripheral;
	dotg->otg.set_host = dwc3_otg_set_host;

	/* This reference is used by dwc3 modules for checking otg existance */
	dwc->dotg = dotg;

	dotg->otg.phy = kzalloc(sizeof(struct usb_phy), GFP_KERNEL);
	if (!dotg->otg.phy) {
		dev_err(dwc->dev, "unable to allocate dwc3_otg.phy\n");
		ret = -ENOMEM;
		goto err1;
	}

	dotg->otg.phy->otg = &dotg->otg;
	dotg->otg.phy->dev = dwc->dev;

	ret = usb_set_transceiver(dotg->otg.phy);
	if (ret) {
		dev_err(dotg->otg.phy->dev,
			"%s: failed to set transceiver, already exists\n",
			__func__);
		goto err2;
	}

	dwc3_otg_reset(dotg);

	dotg->otg.phy->state = OTG_STATE_UNDEFINED;

	INIT_WORK(&dotg->sm_work, dwc3_otg_sm_work);

	ret = request_irq(dotg->irq, dwc3_otg_interrupt, IRQF_SHARED,
				"dwc3_otg", dotg);
	if (ret) {
		dev_err(dotg->otg.phy->dev, "failed to request irq #%d --> %d\n",
				dotg->irq, ret);
		goto err3;
	}

	return 0;

err3:
	cancel_work_sync(&dotg->sm_work);
	usb_set_transceiver(NULL);
err2:
	kfree(dotg->otg.phy);
err1:
	dwc->dotg = NULL;
	kfree(dotg);

	return ret;
}

/**
 * dwc3_otg_exit
 * @dwc: Pointer to out controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
void dwc3_otg_exit(struct dwc3 *dwc)
{
	struct dwc3_otg *dotg = dwc->dotg;

	/* dotg is null when GHWPARAMS6[10]=SRPSupport=0, see dwc3_otg_init */
	if (dotg) {
		cancel_work_sync(&dotg->sm_work);
		usb_set_transceiver(NULL);
		free_irq(dotg->irq, dotg);
		kfree(dotg->otg.phy);
		kfree(dotg);
		dwc->dotg = NULL;
	}
}
