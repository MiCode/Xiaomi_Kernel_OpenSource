/* SPDX-License-Identifier: GPL-2.0 */
/*
 * musb_dr.h - dual role switch and host glue layer header
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Author: Macpaul Lin <macpaul.lin@mediatek.com>
 */

#ifndef _MUSB_DR_H_
#define _MUSB_DR_H_

#include <musb.h>
#include <musb_core.h>
#include <mtk_musb.h>
#include <usb20.h>

#if IS_ENABLED(CONFIG_USB_MTK_OTG) || IS_ENABLED(CONFIG_MTK_MUSB_DUAL_ROLE)
int musb_host_init(struct musb *musb, struct device_node *parent_dn);
void musb_host_exit(struct musb *musb);
int musb_wakeup_of_property_parse(struct musb *musb,
				struct device_node *dn);
int musb_host_enable(struct musb *musb);
int musb_host_disable(struct musb *musb, bool suspend);
void musb_wakeup_set(struct musb *musb, bool enable);
#else
static inline int musb_host_init(struct musb *musb,

	struct device_node *parent_dn)
{
	return 0;
}

static inline void musb_host_exit(struct musb *musb)
{}

static inline int musb_wakeup_of_property_parse(
	struct musb *musb, struct device_node *dn)
{
	return 0;
}

static inline int musb_host_enable(struct musb *musb)
{
	return 0;
}

static inline int musb_host_disable(struct musb *musb, bool suspend)
{
	return 0;
}

static inline void musb_wakeup_set(struct musb *musb, bool enable)
{}

#endif

#if IS_ENABLED(CONFIG_USB_MTK_HDRC) || IS_ENABLED(CONFIG_MTK_MUSB_DUAL_ROLE)
int musb_gadget_init(struct musb *musb);
void musb_gadget_exit(struct musb *musb);
#else
static inline int musb_gadget_init(struct musb *musb)
{
	return 0;
}

static inline void musb_gadget_exit(struct musb *musb)
{}
#endif

#if IS_ENABLED(CONFIG_MTK_MUSB_DUAL_ROLE)
int mt_usb_otg_switch_init(struct mt_usb_glue *glue);
void mt_usb_otg_switch_exit(struct mt_usb_glue *glue);
extern void mt_usb_mode_switch(struct musb *musb, int to_host);
extern int mt_usb_set_vbus(struct otg_switch_mtk *otg_sx, int is_on);
void mt_usb_set_force_mode(struct musb *musb,
			  enum mt_usb_dr_force_mode mode);

#else
static inline int mt_usb_otg_switch_init(struct mt_usb_glue *glue)
{
	return 0;
}

static inline void mt_usb_otg_switch_exit(struct mt_usb_glue *glue)
{}

static inline void mt_usb_mode_switch(struct musb *musb, int to_host)
{}

static inline int mt_usb_set_vbus(struct otg_switch_mtk *otg_sx, int is_on)
{
	return 0;
}

static inline void
mt_usb_set_force_mode(struct musb *musb, enum mt_usb_dr_force_mode mode)
{}
#endif

#endif		/* _MUSB_DR_H_ */
