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


#ifndef __MTU3_PRIV__
#define __MTU3_PRIV__

struct ssusb_priv {
	struct regulator *vusb10;
};

enum MTK_USB_SMC_CALL {
	MTK_USB_SMC_INFRA_REQUEST = 0,
	MTK_USB_SMC_INFRA_RELEASE,
	MTK_USB_SMC_NUM
};

extern void usb_audio_req(bool on);
extern int mtu3_phy_init_debugfs(struct phy *phy);
extern int mtu3_phy_exit_debugfs(void);

#endif

