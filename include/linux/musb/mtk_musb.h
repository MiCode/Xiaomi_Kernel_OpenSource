#ifndef __MUSB_MTK_MUSB_H__
#define __MUSB_MTK_MUSB_H__

#ifdef CONFIG_OF
extern struct musb *mtk_musb;
#define USBPHY_READ8(offset)          readb((void __iomem *)(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_WRITE8(offset, value)  writeb(value, (void __iomem *)(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_SET8(offset, mask)     USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask)     USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~mask))

#define USBPHY_READ16(offset)          readw((void __iomem *)(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_WRITE16(offset, value)  writew(value, (void __iomem *)(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_SET16(offset, mask)     USBPHY_WRITE16(offset, (USBPHY_READ16(offset)) | (mask))
#define USBPHY_CLR16(offset, mask)     USBPHY_WRITE16(offset, (USBPHY_READ16(offset)) & (~mask))

#define USBPHY_READ32(offset)          readl((void __iomem *)(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_WRITE32(offset, value)  writel(value, (void __iomem *)(((unsigned long)mtk_musb->xceiv->io_priv)+0x800+offset))
#define USBPHY_SET32(offset, mask)     USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) | (mask))
#define USBPHY_CLR32(offset, mask)     USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) & (~mask))

#ifdef MTK_UART_USB_SWITCH
#define UART2_BASE 0x11003000
#endif

#else

#include <mach/mt_reg_base.h>

#define USBPHY_READ8(offset)          readb((void __iomem *)(USB_SIF_BASE+0x800+offset))
#define USBPHY_WRITE8(offset, value)  writeb(value, (void __iomem *)(USB_SIF_BASE+0x800+offset))
#define USBPHY_SET8(offset, mask)     USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask)     USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~mask))

#define USBPHY_READ16(offset)          readw((void __iomem *)(USB_SIF_BASE+0x800+offset))
#define USBPHY_WRITE16(offset, value)  writew(value, (void __iomem *)(USB_SIF_BASE+0x800+offset))
#define USBPHY_SET16(offset, mask)     USBPHY_WRITE16(offset, (USBPHY_READ16(offset)) | (mask))
#define USBPHY_CLR16(offset, mask)     USBPHY_WRITE16(offset, (USBPHY_READ16(offset)) & (~mask))

#define USBPHY_READ32(offset)          readl((void __iomem *)(USB_SIF_BASE+0x800+offset))
#define USBPHY_WRITE32(offset, value)  writel(value, (void __iomem *)(USB_SIF_BASE+0x800+offset))
#define USBPHY_SET32(offset, mask)     USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) | (mask))
#define USBPHY_CLR32(offset, mask)     USBPHY_WRITE32(offset, (USBPHY_READ32(offset)) & (~mask))

#endif
struct musb;

typedef enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
} usb_state_enum;

/* USB phy and clock */
extern void usb_phy_poweron(void);
extern void usb_phy_recover(void);
extern void usb_phy_savecurrent(void);
extern void usb_phy_context_restore(void);
extern void usb_phy_context_save(void);
extern bool usb_enable_clock(bool enable);

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

extern bool is_saving_mode();

/* USB switch charger */
extern bool is_switch_charger(void);

/* host and otg */
extern void mt_usb_otg_init(struct musb *musb);
extern void mt_usb_init_drvvbus(void);
extern void mt_usb_set_vbus(struct musb *musb, int is_on);
extern int mt_usb_get_vbus_status(struct musb *musb);
extern void mt_usb_iddig_int(struct musb *musb);
extern void switch_int_to_device(struct musb *musb);
extern void switch_int_to_host(struct musb *musb);
extern void switch_int_to_host_and_mask(struct musb *musb);
extern void musb_session_restart(struct musb *musb);
#endif
