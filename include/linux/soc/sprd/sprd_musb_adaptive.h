/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SPRD_MUSB_ADAPTIVE_H
#define _SPRD_MUSB_ADAPTIVE_H

#include <linux/usb.h>
#include <linux/usb/hcd.h>

extern void musb_set_adaptive_mode(struct usb_hcd *hcd, bool is_adaptive);
extern void musb_adaptive_config(struct usb_hcd *hcd, int ep_num, int mono,
		int is_pcm_24, int width, int rate, int adaptive_used);
#endif /* _SPRD_MUSB_ADAPTIVE_H */
