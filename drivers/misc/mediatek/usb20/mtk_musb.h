/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __MUSB_MTK_MUSB_H__
#define __MUSB_MTK_MUSB_H__

#ifdef CONFIG_OF
extern struct musb *mtk_musb;

#define USBPHY_READ8(offset) \
	readb((void __iomem *)\
		(((unsigned long)\
		mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_WRITE8(offset, value)  writeb(value, (void __iomem *)\
		(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_SET8(offset, mask) \
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask) \
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~(mask)))

#define USBPHY_READ32(offset) \
	readl((void __iomem *)(((unsigned long)\
		mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_WRITE32(offset, value) \
	writel(value, (void __iomem *)\
		(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_SET32(offset, mask) \
	USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) | (mask))
#define USBPHY_CLR32(offset, mask) \
	USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) & (~(mask)))

#endif /* End of CONFIG_OF define */

struct musb;

enum usb_state_enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
};

/* USB phy and clock */
extern bool usb_pre_clock(bool enable);
#ifdef CONFIG_MTK_UART_USB_SWITCH
extern void usb_phy_context_restore(void);
extern void usb_phy_context_save(void);
#endif

/* general USB */
extern bool mt_usb_is_device(void);
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
extern void mt_usb_reconnect(void);
extern bool usb_cable_connected(struct musb *musb);
extern void musb_sync_with_bat(struct musb *musb, int usb_state);

bool is_saving_mode(void);

/* host and otg */
extern void mt_usb_init_drvvbus(void);
extern void mt_usb_iddig_int(struct musb *musb);
extern void switch_int_to_device(struct musb *musb);
extern void switch_int_to_host(struct musb *musb);
extern void switch_int_to_host_and_mask(struct musb *musb);
extern void musb_session_restart(struct musb *musb);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
extern int mt_usb_dual_role_init(struct musb *musb);
extern int mt_usb_dual_role_changed(struct musb *musb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */
extern bool is_usb_rdy(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
#endif
