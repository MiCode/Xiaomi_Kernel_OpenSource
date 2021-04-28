/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MUSB_MTK_MUSB_H__
#define __MUSB_MTK_MUSB_H__

#ifdef CONFIG_OF
extern struct musb *mtk_musb;

#ifdef USB2_PHY_V2
#define USB_PHY_OFFSET 0x300
#else
#define USB_PHY_OFFSET 0x800
#endif

#define USBPHY_READ8(offset) \
	readb((void __iomem *)\
		(((unsigned long)\
		mtk_musb->xceiv->io_priv)+USB_PHY_OFFSET+offset))
#define USBPHY_WRITE8(offset, value)  writeb(value, (void __iomem *)\
		(((unsigned long)mtk_musb->xceiv->io_priv)+USB_PHY_OFFSET+offset))
#define USBPHY_SET8(offset, mask) \
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask) \
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~(mask)))
#define USBPHY_READ32(offset) \
	readl((void __iomem *)(((unsigned long)\
		mtk_musb->xceiv->io_priv)+USB_PHY_OFFSET+offset))
#define USBPHY_WRITE32(offset, value) \
	writel(value, (void __iomem *)\
		(((unsigned long)mtk_musb->xceiv->io_priv)+USB_PHY_OFFSET+offset))
#define USBPHY_SET32(offset, mask) \
	USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) | (mask))
#define USBPHY_CLR32(offset, mask) \
	USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) & (~(mask)))

#ifdef MTK_UART_USB_SWITCH
#define UART2_BASE 0x11003000
#endif

#else

#include <mach/mt_reg_base.h>

#define USBPHY_READ8(offset) \
		readb((void __iomem *)(USB_SIF_BASE+USB_PHY_OFFSET+offset))
#define USBPHY_WRITE8(offset, value) \
		writeb(value, (void __iomem *)(USB_SIF_BASE+USB_PHY_OFFSET+offset))
#define USBPHY_SET8(offset, mask) \
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask) \
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~mask))

#define USBPHY_READ32(offset) \
		readl((void __iomem *)(USB_SIF_BASE+USB_PHY_OFFSET+offset))
#define USBPHY_WRITE32(offset, value) \
		writel(value, (void __iomem *)(USB_SIF_BASE+USB_PHY_OFFSET+offset))
#define USBPHY_SET32(offset, mask) \
		USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) | (mask))
#define USBPHY_CLR32(offset, mask) \
		USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) & (~mask))

#endif
struct musb;

enum usb_state_enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
};

/* USB phy and clock */
extern bool usb_pre_clock(bool enable);
extern void usb_phy_poweron(void);
extern void usb_phy_recover(void);
extern void usb_phy_savecurrent(void);
extern void usb_phy_context_restore(void);
extern void usb_phy_context_save(void);
extern bool usb_enable_clock(bool enable);
extern void usb_rev6_setting(int value);

/* general USB */
extern bool mt_usb_is_device(void);
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
/* ALPS00775710 */
/* extern bool usb_iddig_state(void); */
/* ALPS00775710 */
extern bool usb_cable_connected(void);
extern void pmic_chrdet_int_en(int is_on);
extern void musb_platform_reset(struct musb *musb);
extern void musb_sync_with_bat(struct musb *musb, int usb_state);

extern bool is_saving_mode(void);

/* USB switch charger */
extern bool is_switch_charger(void);

/* host and otg */
extern void mt_usb_otg_init(struct musb *musb);
extern void mt_usb_otg_exit(struct musb *musb);
extern void mt_usb_init_drvvbus(void);
extern void mt_usb_set_vbus(struct musb *musb, int is_on);
extern int mt_usb_get_vbus_status(struct musb *musb);
extern void mt_usb_iddig_int(struct musb *musb);
extern void switch_int_to_device(struct musb *musb);
extern void switch_int_to_host(struct musb *musb);
extern void switch_int_to_host_and_mask(struct musb *musb);
extern void musb_disable_host(struct musb *musb);
extern void musb_enable_host(struct musb *musb);
extern void musb_session_restart(struct musb *musb);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
extern int mt_usb_dual_role_init(struct musb *musb);
extern int mt_usb_dual_role_changed(struct musb *musb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */
#endif
