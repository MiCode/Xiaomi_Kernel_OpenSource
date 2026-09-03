/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SPRD_MUSB_OFFLOAD_H
#define _SPRD_MUSB_OFFLOAD_H

#include <linux/usb.h>
#include <linux/usb/hcd.h>

extern void musb_set_utmi_60m_flag(bool flag);
extern void musb_set_offload_mode(struct usb_hcd *hcd, bool is_offload);
extern void musb_offload_config(struct usb_hcd *hcd, int ep_num, int mono,
				int is_pcm_24, int width, int rate, int offload_used);
#endif /* _SPRD_MUSB_OFFLOAD_H */
