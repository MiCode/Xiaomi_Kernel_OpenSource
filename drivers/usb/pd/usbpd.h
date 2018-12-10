/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#endif /* _USBPD_H */
