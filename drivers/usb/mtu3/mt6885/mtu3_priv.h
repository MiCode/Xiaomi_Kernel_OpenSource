/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

