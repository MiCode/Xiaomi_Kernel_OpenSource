/**
 * dwc3_otg.h - DesignWare USB3 DRD Controller OTG
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

#ifndef __LINUX_USB_DWC3_OTG_H
#define __LINUX_USB_DWC3_OTG_H

#include <linux/workqueue.h>

#include <linux/usb/otg.h>

struct dwc3_charger;

/**
 * struct dwc3_otg: OTG driver data. Shared by HCD and DCD.
 * @otg: USB OTG Transceiver structure.
 * @irq: IRQ number assigned for HSUSB controller.
 * @regs: ioremapped register base address.
 * @sm_work: OTG state machine work.
 * @charger: DWC3 external charger detector
 * @inputs: OTG state machine inputs
 */
struct dwc3_otg {
	struct usb_otg otg;
	int irq;
	void __iomem *regs;
	struct work_struct	sm_work;
	struct dwc3_charger	*charger;
	struct dwc3_ext_xceiv	*ext_xceiv;
#define ID		0
#define B_SESS_VLD	1
	unsigned long inputs;
};

/**
 * USB charger types
 *
 * DWC3_INVALID_CHARGER	Invalid USB charger.
 * DWC3_SDP_CHARGER	Standard downstream port. Refers to a downstream port
 *                      on USB compliant host/hub.
 * DWC3_DCP_CHARGER	Dedicated charger port (AC charger/ Wall charger).
 * DWC3_CDP_CHARGER	Charging downstream port. Enumeration can happen and
 *                      IDEV_CHG_MAX can be drawn irrespective of USB state.
 */
enum dwc3_chg_type {
	DWC3_INVALID_CHARGER = 0,
	DWC3_SDP_CHARGER,
	DWC3_DCP_CHARGER,
	DWC3_CDP_CHARGER,
};

struct dwc3_charger {
	enum dwc3_chg_type	chg_type;

	/* start/stop charger detection, provided by external charger module */
	void	(*start_detection)(struct dwc3_charger *charger, bool start);

	/* to notify OTG about charger detection completion, provided by OTG */
	void	(*notify_detection_complete)(struct usb_otg *otg,
						struct dwc3_charger *charger);
};

/* for external charger driver */
extern int dwc3_set_charger(struct usb_otg *otg, struct dwc3_charger *charger);

enum dwc3_ext_events {
	DWC3_EVENT_NONE = 0,		/* no change event */
	DWC3_EVENT_PHY_RESUME,		/* PHY has come out of LPM */
	DWC3_EVENT_XCEIV_STATE,		/* XCEIV state (id/bsv) has changed */
};

enum dwc3_id_state {
	DWC3_ID_GROUND = 0,
	DWC3_ID_FLOAT,
};

/* external transceiver that can perform connect/disconnect monitoring in LPM */
struct dwc3_ext_xceiv {
	enum dwc3_id_state	id;
	bool			bsv;

	/* to notify OTG about LPM exit event, provided by OTG */
	void	(*notify_ext_events)(struct usb_otg *otg,
					enum dwc3_ext_events ext_event);
};

/* for external transceiver driver */
extern int dwc3_set_ext_xceiv(struct usb_otg *otg,
				struct dwc3_ext_xceiv *ext_xceiv);

#endif /* __LINUX_USB_DWC3_OTG_H */
