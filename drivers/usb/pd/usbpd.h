/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _USBPD_H
#define _USBPD_H

#include <linux/device.h>

struct usbpd;

#if IS_ENABLED(CONFIG_USB_PD_POLICY)
struct usbpd *usbpd_create(struct device *parent);
void usbpd_destroy(struct usbpd *pd);
#else
static inline struct usbpd *usbpd_create(struct device *parent)
{
	return ERR_PTR(-ENODEV);
}
static inline void usbpd_destroy(struct usbpd *pd) { }
#endif

enum data_role {
	DR_NONE = -1,
	DR_UFP = 0,
	DR_DFP = 1,
};

enum power_role {
	PR_NONE = -1,
	PR_SINK = 0,
	PR_SRC = 1,
};

enum pd_sig_type {
	HARD_RESET_SIG = 0,
	CABLE_RESET_SIG,
};

enum pd_sop_type {
	SOP_MSG = 0,
	SOPI_MSG,
	SOPII_MSG,
};

enum pd_spec_rev {
	USBPD_REV_20 = 1,
	USBPD_REV_30 = 2,
};

/* enable msg and signal to be received by phy */
#define FRAME_FILTER_EN_SOP		BIT(0)
#define FRAME_FILTER_EN_SOPI		BIT(1)
#define FRAME_FILTER_EN_HARD_RESET	BIT(5)

struct pd_phy_params {
	void		(*signal_cb)(struct usbpd *pd, enum pd_sig_type sig);
	void		(*msg_rx_cb)(struct usbpd *pd, enum pd_sop_type sop,
					u8 *buf, size_t len);
	void		(*shutdown_cb)(struct usbpd *pd);
	enum data_role	data_role;
	enum power_role power_role;
	u8		frame_filter_val;
};


struct usbpd_pdo {
	bool pps;
	int type;
	int max_volt_mv;
	int min_volt_mv;
	int curr_ma;
	int pos;
};

int usbpd_get_pps_status(struct usbpd *pd, u32 *status);
int usbpd_fetch_pdo(struct usbpd *pd, struct usbpd_pdo *pdos);

#if IS_ENABLED(CONFIG_QPNP_USB_PDPHY)
int pd_phy_open(struct pd_phy_params *params);
int pd_phy_signal(enum pd_sig_type sig);
int pd_phy_write(u16 hdr, const u8 *data, size_t data_len,
		enum pd_sop_type sop);
int pd_phy_update_roles(enum data_role dr, enum power_role pr);
int pd_phy_update_frame_filter(u8 frame_filter_val);
void pd_phy_close(void);
#else
static inline int pd_phy_open(struct pd_phy_params *params)
{
	return -ENODEV;
}

static inline int pd_phy_signal(enum pd_sig_type type)
{
	return -ENODEV;
}

static inline int pd_phy_write(u16 hdr, const u8 *data, size_t data_len,
		enum pd_sop_type sop)
{
	return -ENODEV;
}

static inline int pd_phy_update_roles(enum data_role dr, enum power_role pr)
{
	return -ENODEV;
}

static inline int pd_phy_update_frame_filter(u8 frame_filter_val)
{
	return -ENODEV;
}

static inline void pd_phy_close(void)
{
}
#endif

enum uvdm_state {
	USBPD_UVDM_DISCONNECT,
	USBPD_UVDM_CHARGER_VERSION,
	USBPD_UVDM_CHARGER_VOLTAGE,
	USBPD_UVDM_CHARGER_TEMP,
	USBPD_UVDM_SESSION_SEED,
	USBPD_UVDM_AUTHENTICATION,
	USBPD_UVDM_VERIFIED,
	USBPD_UVDM_REMOVE_COMPENSATION,
	USBPD_UVDM_CONNECT,
	USBPD_UVDM_NAN_ACK,
};

#define USB_PD_MI_SVID			0x2717
#define USBPD_UVDM_SS_LEN		4
#define USBPD_UVDM_VERIFIED_LEN		1

#define VDM_HDR(svid, cmd0, cmd1) \
       (((svid) << 16) | (0 << 15) | ((cmd0) << 8) \
       | (cmd1))
#define UVDM_HDR_CMD(hdr)	((hdr) & 0xFF)

#define USBPD_VDM_RANDOM_NUM		4
#define USBPD_VDM_REQUEST		0x1
#define USBPD_ACK			0x2

struct usbpd_vdm_data {
	int ta_version;
	int ta_temp;
	int ta_voltage;
	unsigned long s_secert[USBPD_UVDM_SS_LEN];
	unsigned long digest[USBPD_UVDM_SS_LEN];
};

#define USBPD_WEAK_PPS_POWER		18000000
#define USBPD_WAKK_PPS_CURR_LIMIT	1500000

#endif /* _USBPD_H */
