/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_gadget.h
 *
 * Copyright (c) 2015 MediaTek Inc.
 *
 */
#ifndef MTK_GADGET_H
#define MTK_GADGET_H

/* duplicate declaration tag_bootmode in charger */
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

extern int meta_dt_get_mboot_params(void);
#ifdef CONFIG_USB_CONFIGFS_MTK_FASTMETA
#include <linux/of.h>
extern int meta_dt_get_mboot_params(void);
#else
extern int meta_dt_get_mboot_params(void) {return 0; };
#endif

#ifdef CONFIG_USB_CONFIGFS_MTK_FASTMETA
extern void composite_setup_complete(struct usb_ep *ep,
		struct usb_request *req);
#endif
#endif
