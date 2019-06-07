/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>

#include "dp_usbpd.h"

/* DP specific VDM commands */
#define DP_USBPD_VDM_STATUS	0x10
#define DP_USBPD_VDM_CONFIGURE	0x11

/* USBPD-TypeC specific Macros */
#define VDM_VERSION		0x0
#define USB_C_DP_SID		0xFF01

enum dp_usbpd_pin_assignment {
	DP_USBPD_PIN_A,
	DP_USBPD_PIN_B,
	DP_USBPD_PIN_C,
	DP_USBPD_PIN_D,
	DP_USBPD_PIN_E,
	DP_USBPD_PIN_F,
	DP_USBPD_PIN_MAX,
};

enum dp_usbpd_events {
	DP_USBPD_EVT_DISCOVER,
	DP_USBPD_EVT_ENTER,
	DP_USBPD_EVT_STATUS,
	DP_USBPD_EVT_CONFIGURE,
	DP_USBPD_EVT_CC_PIN_POLARITY,
	DP_USBPD_EVT_EXIT,
	DP_USBPD_EVT_ATTENTION,
};

enum dp_usbpd_alt_mode {
	DP_USBPD_ALT_MODE_NONE	    = 0,
	DP_USBPD_ALT_MODE_INIT	    = BIT(0),
	DP_USBPD_ALT_MODE_DISCOVER  = BIT(1),
	DP_USBPD_ALT_MODE_ENTER	    = BIT(2),
	DP_USBPD_ALT_MODE_STATUS    = BIT(3),
	DP_USBPD_ALT_MODE_CONFIGURE = BIT(4),
};

struct dp_usbpd_capabilities {
	enum dp_usbpd_port port;
	bool receptacle_state;
	u8 ulink_pin_config;
	u8 dlink_pin_config;
};

struct dp_usbpd_private {
	bool forced_disconnect;
	u32 vdo;
	struct device *dev;
	struct usbpd *pd;
	struct usbpd_svid_handler svid_handler;
	struct dp_hpd_cb *dp_cb;
	struct dp_usbpd_capabilities cap;
	struct dp_usbpd dp_usbpd;
	enum dp_usbpd_alt_mode alt_mode;
	u32 dp_usbpd_config;
};

static const char *dp_usbpd_pin_name(u8 pin)
{
	switch (pin) {
	case DP_USBPD_PIN_A: return "DP_USBPD_PIN_ASSIGNMENT_A";
	case DP_USBPD_PIN_B: return "DP_USBPD_PIN_ASSIGNMENT_B";
	case DP_USBPD_PIN_C: return "DP_USBPD_PIN_ASSIGNMENT_C";
	case DP_USBPD_PIN_D: return "DP_USBPD_PIN_ASSIGNMENT_D";
	case DP_USBPD_PIN_E: return "DP_USBPD_PIN_ASSIGNMENT_E";
	case DP_USBPD_PIN_F: return "DP_USBPD_PIN_ASSIGNMENT_F";
	default: return "UNKNOWN";
	}
}

static const char *dp_usbpd_port_name(enum dp_usbpd_port port)
{
	switch (port) {
	case DP_USBPD_PORT_NONE: return "DP_USBPD_PORT_NONE";
	case DP_USBPD_PORT_UFP_D: return "DP_USBPD_PORT_UFP_D";
	case DP_USBPD_PORT_DFP_D: return "DP_USBPD_PORT_DFP_D";
	case DP_USBPD_PORT_D_UFP_D: return "DP_USBPD_PORT_D_UFP_D";
	default: return "DP_USBPD_PORT_NONE";
	}
}

static const char *dp_usbpd_cmd_name(u8 cmd)
{
	switch (cmd) {
	case USBPD_SVDM_DISCOVER_MODES: return "USBPD_SVDM_DISCOVER_MODES";
	case USBPD_SVDM_ENTER_MODE: return "USBPD_SVDM_ENTER_MODE";
	case USBPD_SVDM_ATTENTION: return "USBPD_SVDM_ATTENTION";
	case DP_USBPD_VDM_STATUS: return "DP_USBPD_VDM_STATUS";
	case DP_USBPD_VDM_CONFIGURE: return "DP_USBPD_VDM_CONFIGURE";
	default: return "DP_USBPD_VDM_ERROR";
	}
}

static void dp_usbpd_init_port(enum dp_usbpd_port *port, u32 in_port)
{
	switch (in_port) {
	case 0:
		*port = DP_USBPD_PORT_NONE;
		break;
	case 1:
		*port = DP_USBPD_PORT_UFP_D;
		break;
	case 2:
		*port = DP_USBPD_PORT_DFP_D;
		break;
	case 3:
		*port = DP_USBPD_PORT_D_UFP_D;
		break;
	default:
		*port = DP_USBPD_PORT_NONE;
	}
	pr_debug("port:%s\n", dp_usbpd_port_name(*port));
}

static void dp_usbpd_get_capabilities(struct dp_usbpd_private *pd)
{
	struct dp_usbpd_capabilities *cap = &pd->cap;
	u32 buf = pd->vdo;
	int port = buf & 0x3;

	cap->receptacle_state = (buf & BIT(6)) ? true : false;
	cap->dlink_pin_config = (buf >> 8) & 0xff;
	cap->ulink_pin_config = (buf >> 16) & 0xff;

	dp_usbpd_init_port(&cap->port, port);
}

static void dp_usbpd_get_status(struct dp_usbpd_private *pd)
{
	struct dp_usbpd *status = &pd->dp_usbpd;
	u32 buf = pd->vdo;
	int port = buf & 0x3;

	status->low_pow_st     = (buf & BIT(2)) ? true : false;
	status->adaptor_dp_en  = (buf & BIT(3)) ? true : false;
	status->base.multi_func = (buf & BIT(4)) ? true : false;
	status->usb_config_req = (buf & BIT(5)) ? true : false;
	status->exit_dp_mode   = (buf & BIT(6)) ? true : false;
	status->base.hpd_high  = (buf & BIT(7)) ? true : false;
	status->base.hpd_irq   = (buf & BIT(8)) ? true : false;

	pr_debug("low_pow_st = %d, adaptor_dp_en = %d, multi_func = %d\n",
			status->low_pow_st, status->adaptor_dp_en,
			status->base.multi_func);
	pr_debug("usb_config_req = %d, exit_dp_mode = %d, hpd_high =%d\n",
			status->usb_config_req,
			status->exit_dp_mode, status->base.hpd_high);
	pr_debug("hpd_irq = %d\n", status->base.hpd_irq);

	dp_usbpd_init_port(&status->port, port);
}

static u32 dp_usbpd_gen_config_pkt(struct dp_usbpd_private *pd)
{
	u8 pin_cfg, pin;
	u32 config = 0;
	const u32 ufp_d_config = 0x2, dp_ver = 0x1;

	if (pd->cap.receptacle_state)
		pin_cfg = pd->cap.ulink_pin_config;
	else
		pin_cfg = pd->cap.dlink_pin_config;

	for (pin = DP_USBPD_PIN_A; pin < DP_USBPD_PIN_MAX; pin++) {
		if (pin_cfg & BIT(pin)) {
			if (pd->dp_usbpd.base.multi_func) {
				if (pin == DP_USBPD_PIN_D)
					break;
			} else {
				break;
			}
		}
	}

	if (pin == DP_USBPD_PIN_MAX)
		pin = DP_USBPD_PIN_C;

	pr_debug("pin assignment: %s\n", dp_usbpd_pin_name(pin));

	config |= BIT(pin) << 8;

	config |= (dp_ver << 2);
	config |= ufp_d_config;

	pr_debug("config = 0x%x\n", config);
	return config;
}

static void dp_usbpd_send_event(struct dp_usbpd_private *pd,
		enum dp_usbpd_events event)
{
	u32 config;

	switch (event) {
	case DP_USBPD_EVT_DISCOVER:
		usbpd_send_svdm(pd->pd, USB_C_DP_SID,
			USBPD_SVDM_DISCOVER_MODES,
			SVDM_CMD_TYPE_INITIATOR, 0x0, 0x0, 0x0);
		break;
	case DP_USBPD_EVT_ENTER:
		usbpd_send_svdm(pd->pd, USB_C_DP_SID,
			USBPD_SVDM_ENTER_MODE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	case DP_USBPD_EVT_EXIT:
		usbpd_send_svdm(pd->pd, USB_C_DP_SID,
			USBPD_SVDM_EXIT_MODE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	case DP_USBPD_EVT_STATUS:
		config = 0x1; /* DFP_D connected */
		usbpd_send_svdm(pd->pd, USB_C_DP_SID, DP_USBPD_VDM_STATUS,
			SVDM_CMD_TYPE_INITIATOR, 0x1, &config, 0x1);
		break;
	case DP_USBPD_EVT_CONFIGURE:
		config = dp_usbpd_gen_config_pkt(pd);
		usbpd_send_svdm(pd->pd, USB_C_DP_SID, DP_USBPD_VDM_CONFIGURE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, &config, 0x1);
		break;
	default:
		pr_err("unknown event:%d\n", event);
	}
}

static void dp_usbpd_connect_cb(struct usbpd_svid_handler *hdlr,
		bool peer_usb_comm)
{
	struct dp_usbpd_private *pd;

	pd = container_of(hdlr, struct dp_usbpd_private, svid_handler);
	if (!pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pr_debug("peer_usb_comm: %d\n", peer_usb_comm);
	pd->dp_usbpd.base.peer_usb_comm = peer_usb_comm;
	dp_usbpd_send_event(pd, DP_USBPD_EVT_DISCOVER);
}

static void dp_usbpd_disconnect_cb(struct usbpd_svid_handler *hdlr)
{
	struct dp_usbpd_private *pd;

	pd = container_of(hdlr, struct dp_usbpd_private, svid_handler);
	if (!pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pd->alt_mode = DP_USBPD_ALT_MODE_NONE;
	pd->dp_usbpd.base.alt_mode_cfg_done = false;
	pr_debug("\n");

	if (pd->dp_cb && pd->dp_cb->disconnect)
		pd->dp_cb->disconnect(pd->dev);
}

static int dp_usbpd_validate_callback(u8 cmd,
	enum usbpd_svdm_cmd_type cmd_type, int num_vdos)
{
	int ret = 0;

	if (cmd_type == SVDM_CMD_TYPE_RESP_NAK) {
		pr_err("error: NACK\n");
		ret = -EINVAL;
		goto end;
	}

	if (cmd_type == SVDM_CMD_TYPE_RESP_BUSY) {
		pr_err("error: BUSY\n");
		ret = -EBUSY;
		goto end;
	}

	if (cmd == USBPD_SVDM_ATTENTION) {
		if (cmd_type != SVDM_CMD_TYPE_INITIATOR) {
			pr_err("error: invalid cmd type for attention\n");
			ret = -EINVAL;
			goto end;
		}

		if (!num_vdos) {
			pr_err("error: no vdo provided\n");
			ret = -EINVAL;
			goto end;
		}
	} else {
		if (cmd_type != SVDM_CMD_TYPE_RESP_ACK) {
			pr_err("error: invalid cmd type\n");
			ret = -EINVAL;
		}
	}
end:
	return ret;
}


static int dp_usbpd_get_ss_lanes(struct dp_usbpd_private *pd)
{
	int rc = 0;
	int timeout = 250;

	/*
	 * By default, USB reserves two lanes for Super Speed.
	 * Which means DP has remaining two lanes to operate on.
	 * If multi-function is not supported, request USB to
	 * release the Super Speed lanes so that DP can use
	 * all four lanes in case DPCD indicates support for
	 * four lanes.
	 */
	if (!pd->dp_usbpd.base.multi_func) {
		while (timeout) {
			rc = pd->svid_handler.request_usb_ss_lane(
					pd->pd, &pd->svid_handler);
			if (rc != -EBUSY)
				break;

			pr_warn("USB busy, retry\n");

			/* wait for hw recommended delay for usb */
			msleep(20);
			timeout--;
		}
	}

	return rc;
}

static void dp_usbpd_response_cb(struct usbpd_svid_handler *hdlr, u8 cmd,
				enum usbpd_svdm_cmd_type cmd_type,
				const u32 *vdos, int num_vdos)
{
	struct dp_usbpd_private *pd;
	int rc = 0;

	pd = container_of(hdlr, struct dp_usbpd_private, svid_handler);

	pr_debug("callback -> cmd: %s, *vdos = 0x%x, num_vdos = %d\n",
				dp_usbpd_cmd_name(cmd), *vdos, num_vdos);

	if (dp_usbpd_validate_callback(cmd, cmd_type, num_vdos)) {
		pr_debug("invalid callback received\n");
		return;
	}

	switch (cmd) {
	case USBPD_SVDM_DISCOVER_MODES:
		pd->vdo = *vdos;
		dp_usbpd_get_capabilities(pd);

		pd->alt_mode |= DP_USBPD_ALT_MODE_DISCOVER;

		if (pd->cap.port & BIT(0))
			dp_usbpd_send_event(pd, DP_USBPD_EVT_ENTER);
		break;
	case USBPD_SVDM_ENTER_MODE:
		pd->alt_mode |= DP_USBPD_ALT_MODE_ENTER;

		dp_usbpd_send_event(pd, DP_USBPD_EVT_STATUS);
		break;
	case USBPD_SVDM_ATTENTION:
		if (pd->forced_disconnect)
			break;

		pd->vdo = *vdos;
		dp_usbpd_get_status(pd);

		if (!pd->dp_usbpd.base.alt_mode_cfg_done) {
			if (pd->dp_usbpd.port & BIT(1))
				dp_usbpd_send_event(pd, DP_USBPD_EVT_CONFIGURE);
			break;
		}

		if (pd->dp_cb && pd->dp_cb->attention)
			pd->dp_cb->attention(pd->dev);

		break;
	case DP_USBPD_VDM_STATUS:
		pd->vdo = *vdos;
		dp_usbpd_get_status(pd);

		if (!(pd->alt_mode & DP_USBPD_ALT_MODE_CONFIGURE)) {
			pd->alt_mode |= DP_USBPD_ALT_MODE_STATUS;

			if (pd->dp_usbpd.port & BIT(1))
				dp_usbpd_send_event(pd, DP_USBPD_EVT_CONFIGURE);
		}
		break;
	case DP_USBPD_VDM_CONFIGURE:
		pd->alt_mode |= DP_USBPD_ALT_MODE_CONFIGURE;
		pd->dp_usbpd.base.alt_mode_cfg_done = true;
		dp_usbpd_get_status(pd);

		pd->dp_usbpd.base.orientation =
			usbpd_get_plug_orientation(pd->pd);

		rc = dp_usbpd_get_ss_lanes(pd);
		if (rc) {
			pr_err("failed to get SuperSpeed lanes\n");
			break;
		}

		if (pd->dp_cb && pd->dp_cb->configure)
			pd->dp_cb->configure(pd->dev);
		break;
	default:
		pr_err("unknown cmd: %d\n", cmd);
		break;
	}
}

static int dp_usbpd_simulate_connect(struct dp_hpd *dp_hpd, bool hpd)
{
	int rc = 0;
	struct dp_usbpd *dp_usbpd;
	struct dp_usbpd_private *pd;

	if (!dp_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp_usbpd = container_of(dp_hpd, struct dp_usbpd, base);
	pd = container_of(dp_usbpd, struct dp_usbpd_private, dp_usbpd);

	dp_usbpd->base.hpd_high = hpd;
	pd->forced_disconnect = !hpd;
	pd->dp_usbpd.base.alt_mode_cfg_done = hpd;

	pr_debug("hpd_high=%d, forced_disconnect=%d, orientation=%d\n",
			dp_usbpd->base.hpd_high, pd->forced_disconnect,
			pd->dp_usbpd.base.orientation);
	if (hpd)
		pd->dp_cb->configure(pd->dev);
	else
		pd->dp_cb->disconnect(pd->dev);

error:
	return rc;
}

static int dp_usbpd_simulate_attention(struct dp_hpd *dp_hpd, int vdo)
{
	int rc = 0;
	struct dp_usbpd *dp_usbpd;
	struct dp_usbpd_private *pd;

	dp_usbpd = container_of(dp_hpd, struct dp_usbpd, base);
	if (!dp_usbpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	pd = container_of(dp_usbpd, struct dp_usbpd_private, dp_usbpd);

	pd->vdo = vdo;
	dp_usbpd_get_status(pd);

	if (pd->dp_cb && pd->dp_cb->attention)
		pd->dp_cb->attention(pd->dev);
error:
	return rc;
}

int dp_usbpd_register(struct dp_hpd *dp_hpd)
{
	struct dp_usbpd *dp_usbpd;
	struct dp_usbpd_private *usbpd;
	int rc = 0;

	if (!dp_hpd)
		return -EINVAL;

	dp_usbpd = container_of(dp_hpd, struct dp_usbpd, base);

	usbpd = container_of(dp_usbpd, struct dp_usbpd_private, dp_usbpd);

	rc = usbpd_register_svid(usbpd->pd, &usbpd->svid_handler);
	if (rc)
		pr_err("pd registration failed\n");

	return rc;
}

static void dp_usbpd_wakeup_phy(struct dp_hpd *dp_hpd, bool wakeup)
{
	struct dp_usbpd *dp_usbpd;
	struct dp_usbpd_private *usbpd;

	dp_usbpd = container_of(dp_hpd, struct dp_usbpd, base);
	usbpd = container_of(dp_usbpd, struct dp_usbpd_private, dp_usbpd);

	if (!usbpd->pd) {
		pr_err("usbpd pointer invalid");
		return;
	}

	usbpd_vdm_in_suspend(usbpd->pd, wakeup);
}

struct dp_hpd *dp_usbpd_get(struct device *dev, struct dp_hpd_cb *cb)
{
	int rc = 0;
	const char *pd_phandle = "qcom,dp-usbpd-detection";
	struct usbpd *pd = NULL;
	struct dp_usbpd_private *usbpd;
	struct dp_usbpd *dp_usbpd;
	struct usbpd_svid_handler svid_handler = {
		.svid		= USB_C_DP_SID,
		.vdm_received	= NULL,
		.connect	= &dp_usbpd_connect_cb,
		.svdm_received	= &dp_usbpd_response_cb,
		.disconnect	= &dp_usbpd_disconnect_cb,
	};

	if (!cb) {
		pr_err("invalid cb data\n");
		rc = -EINVAL;
		goto error;
	}

	pd = devm_usbpd_get_by_phandle(dev, pd_phandle);
	if (IS_ERR(pd)) {
		pr_err("usbpd phandle failed (%ld)\n", PTR_ERR(pd));
		rc = PTR_ERR(pd);
		goto error;
	}

	usbpd = devm_kzalloc(dev, sizeof(*usbpd), GFP_KERNEL);
	if (!usbpd) {
		rc = -ENOMEM;
		goto error;
	}

	usbpd->dev = dev;
	usbpd->pd = pd;
	usbpd->svid_handler = svid_handler;
	usbpd->dp_cb = cb;

	dp_usbpd = &usbpd->dp_usbpd;
	dp_usbpd->base.simulate_connect = dp_usbpd_simulate_connect;
	dp_usbpd->base.simulate_attention = dp_usbpd_simulate_attention;
	dp_usbpd->base.register_hpd = dp_usbpd_register;
	dp_usbpd->base.wakeup_phy = dp_usbpd_wakeup_phy;

	return &dp_usbpd->base;
error:
	return ERR_PTR(rc);
}

void dp_usbpd_put(struct dp_hpd *dp_hpd)
{
	struct dp_usbpd *dp_usbpd;
	struct dp_usbpd_private *usbpd;

	dp_usbpd = container_of(dp_hpd, struct dp_usbpd, base);
	if (!dp_usbpd)
		return;

	usbpd = container_of(dp_usbpd, struct dp_usbpd_private, dp_usbpd);

	usbpd_unregister_svid(usbpd->pd, &usbpd->svid_handler);

	devm_kfree(usbpd->dev, usbpd);
}
