/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __MTK_CHARGER_TYPE_H__
#define __MTK_CHARGER_TYPE_H__

enum charger_type {
	CHARGER_UNKNOWN = 0,
	STANDARD_HOST,		/* USB : 450mA */
	CHARGING_HOST,
	NONSTANDARD_CHARGER,	/* AC : 450mA~1A */
	STANDARD_CHARGER,	/* AC : ~1A */
	APPLE_2_1A_CHARGER, /* 2.1A apple charger */
	APPLE_1_0A_CHARGER, /* 1A apple charger */
	APPLE_0_5A_CHARGER, /* 0.5A apple charger */
	WIRELESS_CHARGER,
};
#if defined(CONFIG_USB_MTK_HDRC) || defined(CONFIG_USB_MU3D_DRV) \
	|| defined(CONFIG_EXTCON_MTK_USB)
extern void mt_usb_connect_v1(void);
extern void mt_usb_disconnect_v1(void);
#else
#define mt_usb_connect() do { } while (0)
#define mt_usb_disconnect() do { } while (0)
#endif

extern enum charger_type mt_get_charger_type(void);
extern void mtk_charger_int_handler(void);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT)
extern int register_charger_det_callback(int (*func)(int));
#endif /*CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT*/

#if defined(CONFIG_MACH_MT6877)
extern bool is_usb_rdy(struct device *dev);
#else
extern bool is_usb_rdy(void);
#endif
extern bool mt_usb_is_device(void);
extern int is_otg_en(void);

#ifndef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
extern void mtk_pmic_enable_chr_type_det(bool en);
#endif

#endif /* __MTK_CHARGER_TYPE_H__ */
