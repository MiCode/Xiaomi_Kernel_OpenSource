/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
	HVDCP_CHARGER,	/* QC2 */
	CHECK_HV,	/* check done */
};

#if defined(CONFIG_USB_MTK_HDRC) || defined(CONFIG_USB_MU3D_DRV) \
	|| defined(CONFIG_EXTCON_MTK_USB)
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
#else
#define mt_usb_connect() do { } while (0)
#define mt_usb_disconnect() do { } while (0)
#endif

extern enum charger_type mt_get_charger_type(void);
extern void mtk_charger_int_handler(void);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT)
extern int register_charger_det_callback(int (*func)(int));
#endif /*CONFIG_MTK_PUMP_EXPRESS_PLUS_30_SUPPORT*/

extern bool is_usb_rdy(void);
extern bool mt_usb_is_device(void);
extern int is_otg_en(void);

#ifndef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
extern void mtk_pmic_enable_chr_type_det(bool en);
#endif

#endif /* __MTK_CHARGER_TYPE_H__ */
