/*
 * Copyright (C) 2017 MediaTek Inc.
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


#ifndef __MTU3_HAL__
#define __MTU3_HAL__

enum {
	USB_DPIDLE_ALLOWED = 0,
	USB_DPIDLE_FORBIDDEN,
	USB_DPIDLE_SRAM,
	USB_DPIDLE_TIMER,
	USB_DPIDLE_AUDIO,
};

enum USB_DEV_SPEED {
	DEV_SPEED_INACTIVE = 0,
	DEV_SPEED_FULL = 1,
	DEV_SPEED_HIGH = 3,
	DEV_SPEED_SUPER = 4,
};

extern int xhci_mtk_register_plat(void);
extern void xhci_mtk_unregister_plat(void);

extern int get_ssusb_ext_rscs(struct ssusb_mtk *ssusb);
extern int ssusb_dual_phy_power_on(struct ssusb_mtk *ssusb,
	bool host_mode);
extern void ssusb_dual_phy_power_off(struct ssusb_mtk *ssusb,
	bool host_mode);
extern int ssusb_clk_on(struct ssusb_mtk *ssusb, int host_mode);
extern int ssusb_clk_off(struct ssusb_mtk *ssusb, int host_mode);
extern int ssusb_ext_pwr_on(struct ssusb_mtk *ssusb, int mode);
extern int ssusb_ext_pwr_off(struct ssusb_mtk *ssusb, int mode);

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern void phy_hal_init(struct phy *phy);
extern void phy_hal_exit(struct phy *phy);
extern bool ssusb_u3loop_back_test(struct ssusb_mtk *ssusb);


extern void ssusb_wakeup_mode_enable(struct ssusb_mtk *ssusb);
extern void ssusb_wakeup_mode_disable(struct ssusb_mtk *ssusb);

extern void ssusb_dpidle_request(int mode);
extern void ssusb_set_phy_mode(int speed);

extern void ssusb_debugfs_init(struct ssusb_mtk *ssusb);
extern void ssusb_debugfs_exit(struct ssusb_mtk *ssusb);

#endif

