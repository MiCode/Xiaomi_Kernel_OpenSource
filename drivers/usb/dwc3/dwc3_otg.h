/**
 * dwc3_otg.h - DesignWare USB3 DRD Controller OTG
 *
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include <linux/usb/otg.h>

/**
 * struct dwc3_otg: OTG driver data. Shared by HCD and DCD.
 * @otg: USB OTG Transceiver structure.
 */
struct dwc3_otg {
	struct usb_otg		otg;
	struct dwc3		*dwc;
};


#endif /* __LINUX_USB_DWC3_OTG_H */
