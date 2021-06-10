// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#define DRIVER_AUTHOR   "jlguo <jlguo@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Gadget - transport data from CP to Gadget"
#define DRIVER_VERSION  "1.0.3"

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/composite.h>
#include "viatel_rawbulk.h"

#include <linux/module.h>
#include <linux/configfs.h>
#include "configfs.h"

/* int setdtr, data_connect; */
/* struct work_struct flow_control; */
/* struct work_struct dtr_status; */

static struct usb_function_instance *via_pcv_alloc_inst(void)
{
	return alloc_inst_rawbulk(RAWBULK_TID_PCV);
}

static struct usb_function *via_pcv_alloc(struct usb_function_instance *fi)
{
	return rawbulk_alloc(fi, RAWBULK_TID_PCV);
}

DECLARE_USB_FUNCTION_INIT(via_pcv, via_pcv_alloc_inst, via_pcv_alloc);
MODULE_LICENSE("GPL");

