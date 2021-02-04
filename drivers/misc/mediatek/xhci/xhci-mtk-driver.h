/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _XHCI_MTK_H
#define _XHCI_MTK_H

#include <linux/usb.h>
#include "mtk-phy.h"

#define K_ALET	(1<<6)
#define K_CRIT	(1<<5)
#define K_ERR	(1<<4)
#define K_WARNIN	(1<<3)
#define K_NOTICE	(1<<2)
#define K_INFO		(1<<1)
#define K_DEBUG	(1<<0)

/*Set the debug level for xhci driver*/
extern int xhci_debug_level;

extern struct xhci_hcd *mtk_xhci;

#ifdef CONFIG_USB_VBUS_GPIO
extern struct platform_device *g_pdev;
#endif
#define mtk_xhci_mtk_printk(level, fmt, args...) do { \
		if (xhci_debug_level & level) { \
			pr_debug("[XHCI]" fmt, ## args); \
		} \
	} while (0)

extern int xhci_mtk_register_plat(void);
extern void xhci_mtk_unregister_plat(void);
extern int mtk_xhci_driver_load(bool vbus_on);
extern void mtk_xhci_driver_unload(bool vbus_off);
extern void mtk_xhci_disable_vbus(void);
extern void mtk_xhci_enable_vbus(void);
#ifdef CONFIG_DUAL_ROLE_USB_INTF
extern void mt_usb_dual_role_to_none(void);
extern void mt_usb_dual_role_to_host(void);
#endif

extern bool mtk_is_host_mode(void);
bool mtk_is_charger_4_vol(void);

#if CONFIG_MTK_GAUGE_VERSION == 30
extern void enable_boost_polling(bool poll_en);
#endif

#endif
