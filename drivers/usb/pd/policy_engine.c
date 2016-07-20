/* Copyright (c) 2016, Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/ipc_logging.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/extcon.h>
#include "usbpd.h"

enum usbpd_state {
	PE_UNKNOWN,
	PE_ERROR_RECOVERY,
	PE_SRC_DISABLED,
	PE_SRC_STARTUP,
	PE_SRC_SEND_CAPABILITIES,
	PE_SRC_SEND_CAPABILITIES_WAIT, /* substate to wait for Request */
	PE_SRC_NEGOTIATE_CAPABILITY,
	PE_SRC_TRANSITION_SUPPLY,
	PE_SRC_READY,
	PE_SRC_HARD_RESET,
	PE_SRC_SOFT_RESET,
	PE_SRC_SEND_SOFT_RESET,
	PE_SRC_DISCOVERY,
	PE_SRC_TRANSITION_TO_DEFAULT,
	PE_SNK_STARTUP,
	PE_SNK_DISCOVERY,
	PE_SNK_WAIT_FOR_CAPABILITIES,
	PE_SNK_EVALUATE_CAPABILITY,
	PE_SNK_SELECT_CAPABILITY,
	PE_SNK_TRANSITION_SINK,
	PE_SNK_READY,
	PE_SNK_HARD_RESET,
	PE_SNK_SOFT_RESET,
	PE_SNK_SEND_SOFT_RESET,
	PE_SNK_TRANSITION_TO_DEFAULT,
	PE_DRS_SEND_DR_SWAP,
	PE_PRS_SNK_SRC_SEND_SWAP,
	PE_PRS_SNK_SRC_TRANSITION_TO_OFF,
	PE_PRS_SNK_SRC_SOURCE_ON,
	PE_PRS_SRC_SNK_SEND_SWAP,
	PE_PRS_SRC_SNK_TRANSITION_TO_OFF,
	PE_PRS_SRC_SNK_WAIT_SOURCE_ON,
};

static const char * const usbpd_state_strings[] = {
	"UNKNOWN",
	"ERROR_RECOVERY",
	"SRC_Disabled",
	"SRC_Startup",
	"SRC_Send_Capabilities",
	"SRC_Send_Capabilities (Wait for Request)",
	"SRC_Negotiate_Capability",
	"SRC_Transition_Supply",
	"SRC_Ready",
	"SRC_Hard_Reset",
	"SRC_Soft_Reset",
	"SRC_Send_Soft_Reset",
	"SRC_Discovery",
	"SRC_Transition_to_default",
	"SNK_Startup",
	"SNK_Discovery",
	"SNK_Wait_for_Capabilities",
	"SNK_Evaluate_Capability",
	"SNK_Select_Capability",
	"SNK_Transition_Sink",
	"SNK_Ready",
	"SNK_Hard_Reset",
	"SNK_Soft_Reset",
	"SNK_Send_Soft_Reset",
	"SNK_Transition_to_default",
	"DRS_Send_DR_Swap",
	"PRS_SNK_SRC_Send_Swap",
	"PRS_SNK_SRC_Transition_to_off",
	"PRS_SNK_SRC_Source_on",
	"PRS_SRC_SNK_Send_Swap",
	"PRS_SRC_SNK_Transition_to_off",
	"PRS_SRC_SNK_Wait_Source_on",
};

enum usbpd_control_msg_type {
	MSG_RESERVED = 0,
	MSG_GOODCRC,
	MSG_GOTOMIN,
	MSG_ACCEPT,
	MSG_REJECT,
	MSG_PING,
	MSG_PS_RDY,
	MSG_GET_SOURCE_CAP,
	MSG_GET_SINK_CAP,
	MSG_DR_SWAP,
	MSG_PR_SWAP,
	MSG_VCONN_SWAP,
	MSG_WAIT,
	MSG_SOFT_RESET,
};

enum usbpd_data_msg_type {
	MSG_SOURCE_CAPABILITIES = 1,
	MSG_REQUEST,
	MSG_BIST,
	MSG_SINK_CAPABILITIES,
	MSG_VDM = 0xF,
};

enum plug_orientation {
	ORIENTATION_NONE,
	ORIENTATION_CC1,
	ORIENTATION_CC2,
};

static void *usbpd_ipc_log;
#define usbpd_dbg(dev, fmt, ...) do { \
	ipc_log_string(usbpd_ipc_log, "%s: %s: " fmt, dev_name(dev), __func__, \
			##__VA_ARGS__); \
	dev_dbg(dev, fmt, ##__VA_ARGS__); \
	} while (0)

#define usbpd_info(dev, fmt, ...) do { \
	ipc_log_string(usbpd_ipc_log, "%s: %s: " fmt, dev_name(dev), __func__, \
			##__VA_ARGS__); \
	dev_info(dev, fmt, ##__VA_ARGS__); \
	} while (0)

#define usbpd_warn(dev, fmt, ...) do { \
	ipc_log_string(usbpd_ipc_log, "%s: %s: " fmt, dev_name(dev), __func__, \
			##__VA_ARGS__); \
	dev_warn(dev, fmt, ##__VA_ARGS__); \
	} while (0)

#define usbpd_err(dev, fmt, ...) do { \
	ipc_log_string(usbpd_ipc_log, "%s: %s: " fmt, dev_name(dev), __func__, \
			##__VA_ARGS__); \
	dev_err(dev, fmt, ##__VA_ARGS__); \
	} while (0)

#define NUM_LOG_PAGES		10

/* Timeouts (in ms) */
#define ERROR_RECOVERY_TIME	25
#define SENDER_RESPONSE_TIME	30
#define SINK_WAIT_CAP_TIME	620
#define PS_TRANSITION_TIME	550
#define SRC_CAP_TIME		120
#define SRC_TRANSITION_TIME	25
#define PS_HARD_RESET_TIME	35
#define PS_SOURCE_ON		400
#define PS_SOURCE_OFF		900

#define PD_CAPS_COUNT		50

#define PD_MAX_MSG_ID		7

#define PD_MSG_HDR(type, dr, pr, id, cnt) \
	(((type) & 0xF) | ((dr) << 5) | (1 << 6) | \
	 ((pr) << 8) | ((id) << 9) | ((cnt) << 12))
#define PD_MSG_HDR_COUNT(hdr) (((hdr) >> 12) & 7)
#define PD_MSG_HDR_TYPE(hdr) ((hdr) & 0xF)
#define PD_MSG_HDR_ID(hdr) (((hdr) >> 9) & 7)

#define PD_RDO_FIXED(obj, gb, mismatch, usb_comm, no_usb_susp, curr1, curr2) \
		(((obj) << 28) | ((gb) << 27) | ((mismatch) << 26) | \
		 ((usb_comm) << 25) | ((no_usb_susp) << 24) | \
		 ((curr1) << 10) | (curr2))

#define PD_RDO_OBJ_POS(rdo)		((rdo) >> 28 & 7)
#define PD_RDO_GIVEBACK(rdo)		((rdo) >> 27 & 1)
#define PD_RDO_MISMATCH(rdo)		((rdo) >> 26 & 1)
#define PD_RDO_USB_COMM(rdo)		((rdo) >> 25 & 1)
#define PD_RDO_NO_USB_SUSP(rdo)		((rdo) >> 24 & 1)
#define PD_RDO_FIXED_CURR(rdo)		((rdo) >> 19 & 0x3FF)
#define PD_RDO_FIXED_CURR_MINMAX(rdo)	((rdo) & 0x3FF)

#define PD_SRC_PDO_TYPE(pdo)		(((pdo) >> 30) & 3)
#define PD_SRC_PDO_TYPE_FIXED		0
#define PD_SRC_PDO_TYPE_BATTERY		1
#define PD_SRC_PDO_TYPE_VARIABLE	2

#define PD_SRC_PDO_FIXED_PR_SWAP(pdo)		(((pdo) >> 29) & 1)
#define PD_SRC_PDO_FIXED_USB_SUSP(pdo)		(((pdo) >> 28) & 1)
#define PD_SRC_PDO_FIXED_EXT_POWERED(pdo)	(((pdo) >> 27) & 1)
#define PD_SRC_PDO_FIXED_USB_COMM(pdo)		(((pdo) >> 26) & 1)
#define PD_SRC_PDO_FIXED_DR_SWAP(pdo)		(((pdo) >> 25) & 1)
#define PD_SRC_PDO_FIXED_PEAK_CURR(pdo)		(((pdo) >> 20) & 3)
#define PD_SRC_PDO_FIXED_VOLTAGE(pdo)		(((pdo) >> 10) & 0x3FF)
#define PD_SRC_PDO_FIXED_MAX_CURR(pdo)		((pdo) & 0x3FF)

#define PD_SRC_PDO_VAR_BATT_MAX_VOLT(pdo)	(((pdo) >> 20) & 0x3FF)
#define PD_SRC_PDO_VAR_BATT_MIN_VOLT(pdo)	(((pdo) >> 10) & 0x3FF)
#define PD_SRC_PDO_VAR_BATT_MAX(pdo)		((pdo) & 0x3FF)

static int min_sink_current = 900;
module_param(min_sink_current, int, S_IRUSR | S_IWUSR);

static int max_sink_current = 3000;
module_param(max_sink_current, int, S_IRUSR | S_IWUSR);

static const u32 default_src_caps[] = { 0x36019096 };	/* VSafe5V @ 1.5A */

static const u32 default_snk_caps[] = { 0x2601905A,	/* 5V @ 900mA */
					0x0002D096,	/* 9V @ 1.5A */
					0x0003C064 };	/* 12V @ 1A */

struct usbpd {
	struct device		dev;
	struct workqueue_struct	*wq;
	struct delayed_work	sm_work;

	struct extcon_dev	*extcon;

	enum usbpd_state	current_state;
	bool			hard_reset;
	u8			rx_msg_type;
	u8			rx_msg_len;
	u32			rx_payload[7];

	u32			received_pdos[7];
	int			src_cap_id;
	u8			selected_pdo;
	u8			requested_pdo;
	u32			rdo;	/* can be either source or sink */
	int			current_voltage;	/* uV */
	int			requested_voltage;	/* uV */
	int			requested_current;	/* mA */
	bool			pd_connected;
	bool			in_explicit_contract;
	bool			peer_usb_comm;
	bool			peer_pr_swap;
	bool			peer_dr_swap;

	struct power_supply	*usb_psy;
	struct notifier_block	psy_nb;

	enum power_supply_typec_mode typec_mode;
	enum power_supply_type	psy_type;
	bool			vbus_present;

	enum data_role		current_dr;
	enum power_role		current_pr;
	bool			in_pr_swap;
	bool			pd_phy_opened;

	struct regulator	*vbus;
	struct regulator	*vconn;
	bool			vconn_enabled;

	u8			tx_msgid;
	u8			rx_msgid;
	int			caps_count;
	int			hard_reset_count;

	struct list_head	instance;
};

static LIST_HEAD(_usbpd);	/* useful for debugging */

static const unsigned int usbpd_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_USB_CC,
	EXTCON_NONE,
};

/* EXTCON_USB and EXTCON_USB_HOST are mutually exclusive */
static const u32 usbpd_extcon_exclusive[] = {0x3, 0};

static enum plug_orientation usbpd_get_plug_orientation(struct usbpd *pd)
{
	int ret;
	union power_supply_propval val;

	ret = power_supply_get_property(pd->usb_psy,
		POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, &val);
	if (ret)
		return ORIENTATION_NONE;

	return val.intval;
}

static bool is_cable_flipped(struct usbpd *pd)
{
	enum plug_orientation cc;

	cc = usbpd_get_plug_orientation(pd);
	if (cc == ORIENTATION_CC2)
		return true;

	/*
	 * ORIENTATION_CC1 or ORIENTATION_NONE.
	 * Return value for ORIENTATION_NONE is
	 * "dont care" as disconnect handles it.
	 */
	return false;
}

static int set_power_role(struct usbpd *pd, enum power_role pr)
{
	union power_supply_propval val = {0};

	switch (pr) {
	case PR_NONE:
		val.intval = POWER_SUPPLY_TYPEC_PR_NONE;
		break;
	case PR_SINK:
		val.intval = POWER_SUPPLY_TYPEC_PR_SINK;
		break;
	case PR_SRC:
		val.intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
		break;
	}

	return power_supply_set_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &val);
}

static int pd_send_msg(struct usbpd *pd, u8 hdr_type, const u32 *data,
		size_t num_data, enum pd_msg_type type)
{
	int ret;
	u16 hdr;

	hdr = PD_MSG_HDR(hdr_type, pd->current_dr, pd->current_pr,
			pd->tx_msgid, num_data);
	ret = pd_phy_write(hdr, (u8 *)data, num_data * sizeof(u32), type, 15);
	/* TODO figure out timeout. based on tReceive=1.1ms x nRetryCount? */

	/* MessageID incremented regardless of Tx error */
	pd->tx_msgid = (pd->tx_msgid + 1) & PD_MAX_MSG_ID;

	if (ret != num_data * sizeof(u32))
		return -EIO;
	return 0;
}

static int pd_select_pdo(struct usbpd *pd, int pdo_pos)
{
	int curr = min_sink_current;
	int max_current = max_sink_current;
	bool mismatch = false;
	u32 pdo = pd->received_pdos[pdo_pos - 1];

	/* TODO: handle variable/battery types */
	if (PD_SRC_PDO_TYPE(pdo) != PD_SRC_PDO_TYPE_FIXED) {
		usbpd_err(&pd->dev, "Non-fixed PDOs currently unsupported\n");
		return -ENOTSUPP;
	}

	/*
	 * Check if the PDO has enough current, otherwise set the
	 * Capability Mismatch flag
	 */
	if ((PD_SRC_PDO_FIXED_MAX_CURR(pdo) * 10) < curr) {
		mismatch = true;
		max_current = curr;
		curr = PD_SRC_PDO_FIXED_MAX_CURR(pdo) * 10;
	}

	pd->requested_voltage = PD_SRC_PDO_FIXED_VOLTAGE(pdo) * 50 * 1000;
	pd->requested_current = max_current;
	pd->requested_pdo = pdo_pos;
	pd->rdo = PD_RDO_FIXED(pdo_pos, 0, mismatch, 1, 1, curr / 10,
			max_current / 10);

	return 0;
}

static int pd_eval_src_caps(struct usbpd *pd, const u32 *src_caps)
{
	u32 first_pdo = src_caps[0];

	/* save the PDOs so userspace can further evaluate */
	memcpy(&pd->received_pdos, src_caps, sizeof(pd->received_pdos));
	pd->src_cap_id++;

	if (PD_SRC_PDO_TYPE(first_pdo) != PD_SRC_PDO_TYPE_FIXED) {
		usbpd_err(&pd->dev, "First src_cap invalid! %08x\n", first_pdo);
		return -EINVAL;
	}

	pd->peer_usb_comm = PD_SRC_PDO_FIXED_USB_COMM(first_pdo);
	pd->peer_pr_swap = PD_SRC_PDO_FIXED_PR_SWAP(first_pdo);
	pd->peer_dr_swap = PD_SRC_PDO_FIXED_DR_SWAP(first_pdo);

	/* Select the first PDO (vSafe5V) immediately. */
	pd_select_pdo(pd, 1);

	return 0;
}

static void pd_send_hard_reset(struct usbpd *pd)
{
	int ret;

	usbpd_dbg(&pd->dev, "send hard reset");

	/* Force CC logic to source/sink to keep Rp/Rd unchanged */
	set_power_role(pd, pd->current_pr);
	pd->hard_reset_count++;
	ret = pd_phy_signal(HARD_RESET_SIG, 5); /* tHardResetComplete */
	if (!ret)
		pd->hard_reset = true;
}

static void phy_sig_received(struct usbpd *pd, enum pd_sig_type type)
{
	if (type != HARD_RESET_SIG) {
		usbpd_err(&pd->dev, "invalid signal (%d) received\n", type);
		return;
	}

	usbpd_dbg(&pd->dev, "hard reset received\n");

	/* Force CC logic to source/sink to keep Rp/Rd unchanged */
	set_power_role(pd, pd->current_pr);
	pd->hard_reset = true;
	mod_delayed_work(pd->wq, &pd->sm_work, 0);
}

static void phy_msg_received(struct usbpd *pd, enum pd_msg_type type,
		u8 *buf, size_t len)
{
	u16 header;

	if (type != SOP_MSG) {
		usbpd_err(&pd->dev, "invalid msg type (%d) received; only SOP supported\n",
				type);
		return;
	}

	if (len < 2) {
		usbpd_err(&pd->dev, "invalid message received, len=%ld\n", len);
		return;
	}

	header = *((u16 *)buf);
	buf += sizeof(u16);
	len -= sizeof(u16);

	if (len % 4 != 0) {
		usbpd_err(&pd->dev, "len=%ld not multiple of 4\n", len);
		return;
	}

	/* if MSGID already seen, discard */
	if (PD_MSG_HDR_ID(header) == pd->rx_msgid &&
			PD_MSG_HDR_TYPE(header) != MSG_SOFT_RESET) {
		usbpd_dbg(&pd->dev, "MessageID already seen, discarding\n");
		return;
	}

	pd->rx_msgid = PD_MSG_HDR_ID(header);

	/* check header's count field to see if it matches len */
	if (PD_MSG_HDR_COUNT(header) != (len / 4)) {
		usbpd_err(&pd->dev, "header count (%d) mismatch, len=%ld\n",
				PD_MSG_HDR_COUNT(header), len);
		return;
	}

	pd->rx_msg_type = PD_MSG_HDR_TYPE(header);
	pd->rx_msg_len = PD_MSG_HDR_COUNT(header);
	memcpy(&pd->rx_payload, buf, len);

	mod_delayed_work(pd->wq, &pd->sm_work, 0);
}

static void phy_shutdown(struct usbpd *pd)
{
	usbpd_dbg(&pd->dev, "shutdown");
}

/* Enters new state and executes actions on entry */
static void usbpd_set_state(struct usbpd *pd, enum usbpd_state next_state)
{
	struct pd_phy_params phy_params = {
		.signal_cb		= phy_sig_received,
		.msg_rx_cb		= phy_msg_received,
		.shutdown_cb		= phy_shutdown,
		.frame_filter_val	= FRAME_FILTER_EN_SOP |
					  FRAME_FILTER_EN_HARD_RESET
	};
	union power_supply_propval val = {0};
	int ret;

	usbpd_dbg(&pd->dev, "%s -> %s\n",
			usbpd_state_strings[pd->current_state],
			usbpd_state_strings[next_state]);

	pd->current_state = next_state;

	switch (next_state) {
	case PE_ERROR_RECOVERY: /* perform hard disconnect/reconnect */
		pd->in_pr_swap = false;
		set_power_role(pd, PR_NONE);
		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(ERROR_RECOVERY_TIME));
		break;

	/* Source states */
	case PE_SRC_STARTUP:
		if (pd->current_dr == DR_NONE) {
			pd->current_dr = DR_DFP;
			/* Defer starting USB host mode until after PD */
		}

		pd->rx_msg_len = 0;
		pd->rx_msg_type = 0;
		pd->rx_msgid = -1;

		if (!pd->in_pr_swap) {
			if (pd->pd_phy_opened) {
				pd_phy_close();
				pd->pd_phy_opened = false;
			}

			phy_params.data_role = pd->current_dr;
			phy_params.power_role = pd->current_pr;

			ret = pd_phy_open(&phy_params);
			if (ret) {
				WARN_ON_ONCE(1);
				usbpd_err(&pd->dev, "error opening PD PHY %d\n",
						ret);
				pd->current_state = PE_UNKNOWN;
				return;
			}

			pd->pd_phy_opened = true;
		}

		val.intval = 1;
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_PD_ACTIVE, &val);

		pd->in_pr_swap = false;
		pd->current_state = PE_SRC_SEND_CAPABILITIES;
		usbpd_dbg(&pd->dev, "Enter %s\n",
				usbpd_state_strings[pd->current_state]);
		/* fall-through */

	case PE_SRC_SEND_CAPABILITIES:
		queue_delayed_work(pd->wq, &pd->sm_work, 0);
		break;

	case PE_SRC_NEGOTIATE_CAPABILITY:
		if (PD_RDO_OBJ_POS(pd->rdo) != 1) {
			/* send Reject */
			ret = pd_send_msg(pd, MSG_REJECT, NULL, 0, SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending Reject\n");
				usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
				break;
			}

			usbpd_err(&pd->dev, "Invalid request: %08x\n", pd->rdo);

			if (pd->in_explicit_contract)
				usbpd_set_state(pd, PE_SRC_READY);
			else
				/*
				 * bypass PE_SRC_Capability_Response and
				 * PE_SRC_Wait_New_Capabilities in this
				 * implementation for simplicity.
				 */
				usbpd_set_state(pd, PE_SRC_SEND_CAPABILITIES);
			break;
		}

		/*
		 * we only support VSafe5V so Aceept right away as there is
		 * nothing more to prepare from the power supply
		 */
		pd->current_state = PE_SRC_TRANSITION_SUPPLY;
		usbpd_dbg(&pd->dev, "Enter %s\n",
				usbpd_state_strings[pd->current_state]);
		/* fall-through */

	case PE_SRC_TRANSITION_SUPPLY:
		ret = pd_send_msg(pd, MSG_ACCEPT, NULL, 0, SOP_MSG);
		if (ret) {
			usbpd_err(&pd->dev, "Error sending Accept\n");
			usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
			break;
		}

		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(SRC_TRANSITION_TIME));
		break;

	case PE_SRC_READY:
		if (pd->current_dr == DR_DFP)
			extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 1);
		pd->in_explicit_contract = true;
		kobject_uevent(&pd->dev.kobj, KOBJ_CHANGE);
		break;

	case PE_SRC_TRANSITION_TO_DEFAULT:
		if (pd->vconn_enabled)
			regulator_disable(pd->vconn);
		regulator_disable(pd->vbus);

		if (pd->current_dr != DR_DFP) {
			extcon_set_cable_state_(pd->extcon, EXTCON_USB, 0);
			pd->current_dr = DR_DFP;
			pd_phy_update_roles(pd->current_dr, pd->current_pr);
		}

		msleep(1000);	/* tSrcRecover */

		ret = regulator_enable(pd->vbus);
		if (ret)
			usbpd_err(&pd->dev, "Unable to enable vbus\n");

		if (pd->vconn_enabled) {
			ret = regulator_enable(pd->vconn);
			if (ret) {
				usbpd_err(&pd->dev, "Unable to enable vconn\n");
				pd->vconn_enabled = false;
			}
		}

		usbpd_set_state(pd, PE_SRC_STARTUP);
		break;

	case PE_SRC_HARD_RESET:
	case PE_SNK_HARD_RESET:
		/* hard reset may sleep; handle it in the workqueue */
		queue_delayed_work(pd->wq, &pd->sm_work, 0);
		break;

	case PE_SRC_SEND_SOFT_RESET:
	case PE_SNK_SEND_SOFT_RESET:
		/* Reset protocol layer */
		pd->tx_msgid = 0;
		pd->rx_msgid = -1;

		ret = pd_send_msg(pd, MSG_SOFT_RESET, NULL, 0, SOP_MSG);
		if (ret) {
			usbpd_err(&pd->dev, "Error sending Soft Reset, do Hard Reset\n");
			usbpd_set_state(pd, pd->current_pr == PR_SRC ?
					PE_SRC_HARD_RESET : PE_SNK_HARD_RESET);
			break;
		}

		/* wait for ACCEPT */
		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(SENDER_RESPONSE_TIME * 3));
		break;

	/* Sink states */
	case PE_SNK_STARTUP:
		if (pd->current_dr == DR_NONE || pd->current_dr == DR_UFP) {
			pd->current_dr = DR_UFP;

			if (pd->psy_type == POWER_SUPPLY_TYPE_USB ||
				pd->psy_type == POWER_SUPPLY_TYPE_USB_CDP) {
				extcon_set_cable_state_(pd->extcon,
						EXTCON_USB_CC,
						is_cable_flipped(pd));
				extcon_set_cable_state_(pd->extcon,
						EXTCON_USB, 1);
			}
		}

		pd->rx_msg_len = 0;
		pd->rx_msg_type = 0;
		pd->rx_msgid = -1;

		if (!pd->in_pr_swap) {
			if (pd->pd_phy_opened) {
				pd_phy_close();
				pd->pd_phy_opened = false;
			}

			phy_params.data_role = pd->current_dr;
			phy_params.power_role = pd->current_pr;

			ret = pd_phy_open(&phy_params);
			if (ret) {
				WARN_ON_ONCE(1);
				usbpd_err(&pd->dev, "error opening PD PHY %d\n",
						ret);
				pd->current_state = PE_UNKNOWN;
				return;
			}

			pd->pd_phy_opened = true;
		}

		pd->in_pr_swap = false;
		pd->current_voltage = 5000000;

		if (!pd->vbus_present) {
			/* can get here during a hard reset and we lost vbus */
			pd->current_state = PE_SNK_DISCOVERY;
			queue_delayed_work(pd->wq, &pd->sm_work,
					msecs_to_jiffies(2000));
			break;
		}

		/*
		 * If VBUS is already present go and skip ahead to
		 * PE_SNK_WAIT_FOR_CAPABILITIES.
		 */
		pd->current_state = PE_SNK_WAIT_FOR_CAPABILITIES;
		/* fall-through */

	case PE_SNK_WAIT_FOR_CAPABILITIES:
		if (pd->rx_msg_len && pd->rx_msg_type)
			queue_delayed_work(pd->wq, &pd->sm_work, 0);
		else
			queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(SINK_WAIT_CAP_TIME));
		break;

	case PE_SNK_EVALUATE_CAPABILITY:
		pd->pd_connected = true; /* we know peer is PD capable */
		pd->hard_reset_count = 0;

		/* evaluate PDOs and select one */
		ret = pd_eval_src_caps(pd, pd->rx_payload);
		if (ret < 0) {
			usbpd_err(&pd->dev, "Invalid src_caps received. Skipping request\n");
			break;
		}
		pd->current_state = PE_SNK_SELECT_CAPABILITY;
		/* fall-through */

	case PE_SNK_SELECT_CAPABILITY:
		ret = pd_send_msg(pd, MSG_REQUEST, &pd->rdo, 1, SOP_MSG);
		if (ret)
			usbpd_err(&pd->dev, "Error sending Request\n");

		/* wait for ACCEPT */
		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(SENDER_RESPONSE_TIME * 3));
		break;

	case PE_SNK_TRANSITION_SINK:
		/* wait for PS_RDY */
		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(PS_TRANSITION_TIME));
		break;

	case PE_SNK_READY:
		pd->in_explicit_contract = true;
		kobject_uevent(&pd->dev.kobj, KOBJ_CHANGE);
		break;

	case PE_SNK_TRANSITION_TO_DEFAULT:
		if (pd->current_dr != DR_UFP) {
			extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 0);

			pd->current_dr = DR_UFP;
			extcon_set_cable_state_(pd->extcon, EXTCON_USB_CC,
					is_cable_flipped(pd));
			extcon_set_cable_state_(pd->extcon, EXTCON_USB, 1);
			pd_phy_update_roles(pd->current_dr, pd->current_pr);
		}
		if (pd->vconn_enabled) {
			regulator_disable(pd->vconn);
			pd->vconn_enabled = false;
		}

		pd->tx_msgid = 0;

		val.intval = pd->requested_voltage; /* set range back to 5V */
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
		pd->current_voltage = pd->requested_voltage;

		val.intval = pd->requested_current * 1000; /* mA->uA */
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &val);

		/* recursive call; go back to beginning state */
		usbpd_set_state(pd, PE_SNK_STARTUP);
		break;

	default:
		usbpd_dbg(&pd->dev, "No action for state %s\n",
				usbpd_state_strings[pd->current_state]);
		break;
	}
}

static void dr_swap(struct usbpd *pd)
{
	if (pd->current_dr == DR_DFP) {
		extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 0);
		extcon_set_cable_state_(pd->extcon, EXTCON_USB_CC,
				is_cable_flipped(pd));
		extcon_set_cable_state_(pd->extcon, EXTCON_USB, 1);
		pd->current_dr = DR_UFP;
	} else if (pd->current_dr == DR_UFP) {
		extcon_set_cable_state_(pd->extcon, EXTCON_USB, 0);
		extcon_set_cable_state_(pd->extcon, EXTCON_USB_CC,
				is_cable_flipped(pd));
		extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 1);
		pd->current_dr = DR_DFP;
	}

	pd_phy_update_roles(pd->current_dr, pd->current_pr);
}

/* Handles current state and determines transitions */
static void usbpd_sm(struct work_struct *w)
{
	struct usbpd *pd = container_of(w, struct usbpd, sm_work.work);
	union power_supply_propval val = {0};
	int ret;
	enum usbpd_control_msg_type ctrl_recvd = 0;
	enum usbpd_data_msg_type data_recvd = 0;

	usbpd_dbg(&pd->dev, "handle state %s\n",
			usbpd_state_strings[pd->current_state]);

	if (pd->rx_msg_len)
		data_recvd = pd->rx_msg_type;
	else
		ctrl_recvd = pd->rx_msg_type;

	/* Disconnect? */
	if (pd->typec_mode == POWER_SUPPLY_TYPEC_NONE) {
		if (pd->current_state == PE_UNKNOWN)
			return;

		usbpd_info(&pd->dev, "USB PD disconnect\n");

		if (pd->pd_phy_opened) {
			pd_phy_close();
			pd->pd_phy_opened = false;
		}

		pd->in_pr_swap = false;
		pd->pd_connected = false;
		pd->in_explicit_contract = false;
		pd->hard_reset = false;
		pd->caps_count = 0;
		pd->hard_reset_count = 0;
		pd->src_cap_id = 0;
		memset(&pd->received_pdos, 0, sizeof(pd->received_pdos));

		val.intval = 0;
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_PD_ACTIVE, &val);

		if (pd->current_pr == PR_SRC) {
			regulator_disable(pd->vconn);
			regulator_disable(pd->vbus);
		}

		if (pd->current_dr == DR_UFP)
			extcon_set_cable_state_(pd->extcon, EXTCON_USB, 0);
		else if (pd->current_dr == DR_DFP)
			extcon_set_cable_state_(pd->extcon, EXTCON_USB_HOST, 0);

		pd->current_pr = PR_NONE;
		pd->current_dr = DR_NONE;

		/* Set CC back to DRP toggle */
		val.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &val);

		pd->current_state = PE_UNKNOWN;
		return;
	}

	/* Hard reset? */
	if (pd->hard_reset) {
		if (pd->current_pr == PR_SINK)
			usbpd_set_state(pd, PE_SNK_TRANSITION_TO_DEFAULT);
		else
			usbpd_set_state(pd, PE_SRC_TRANSITION_TO_DEFAULT);
		pd->hard_reset = false;
	}

	/* Soft reset? */
	if (ctrl_recvd == MSG_SOFT_RESET) {
		usbpd_dbg(&pd->dev, "Handle soft reset\n");

		if (pd->current_pr == PR_SRC)
			pd->current_state = PE_SRC_SOFT_RESET;
		else if (pd->current_pr == PR_SINK)
			pd->current_state = PE_SNK_SOFT_RESET;
	}

	switch (pd->current_state) {
	case PE_UNKNOWN:
		if (pd->current_pr == PR_SINK) {
			usbpd_set_state(pd, PE_SNK_STARTUP);
		} else if (pd->current_pr == PR_SRC) {
			ret = regulator_enable(pd->vbus);
			if (ret)
				usbpd_err(&pd->dev, "Unable to enable vbus\n");

			if (!pd->vconn_enabled &&
					pd->typec_mode ==
					POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE) {
				ret = regulator_enable(pd->vconn);
				if (ret)
					usbpd_err(&pd->dev, "Unable to enable vconn\n");
				else
					pd->vconn_enabled = true;
			}

			usbpd_set_state(pd, PE_SRC_STARTUP);
		}
		break;

	case PE_SRC_SEND_CAPABILITIES:
		ret = pd_send_msg(pd, MSG_SOURCE_CAPABILITIES, default_src_caps,
				ARRAY_SIZE(default_src_caps), SOP_MSG);
		if (ret) {
			pd->caps_count++;

			if (pd->caps_count == 5 && pd->current_dr == DR_DFP) {
				/* Likely not PD-capable, start host now */
				extcon_set_cable_state_(pd->extcon,
					EXTCON_USB_CC, is_cable_flipped(pd));
				extcon_set_cable_state_(pd->extcon,
						EXTCON_USB_HOST, 1);
			} else if (pd->caps_count >= PD_CAPS_COUNT) {
				usbpd_dbg(&pd->dev, "Src CapsCounter exceeded, disabling PD\n");
				usbpd_set_state(pd, PE_SRC_DISABLED);

				val.intval = 0;
				power_supply_set_property(pd->usb_psy,
						POWER_SUPPLY_PROP_PD_ACTIVE,
						&val);
				break;
			}

			queue_delayed_work(pd->wq, &pd->sm_work,
					msecs_to_jiffies(SRC_CAP_TIME));
			break;
		}

		/* transmit was successful if GoodCRC was received */
		pd->caps_count = 0;
		pd->hard_reset_count = 0;
		pd->pd_connected = true; /* we know peer is PD capable */

		val.intval = POWER_SUPPLY_TYPE_USB_PD;
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &val);

		/* wait for REQUEST */
		pd->current_state = PE_SRC_SEND_CAPABILITIES_WAIT;
		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(SENDER_RESPONSE_TIME * 3));
		break;

	case PE_SRC_SEND_CAPABILITIES_WAIT:
		if (data_recvd == MSG_REQUEST) {
			pd->rdo = pd->rx_payload[0];
			usbpd_set_state(pd, PE_SRC_NEGOTIATE_CAPABILITY);
		} else {
			usbpd_set_state(pd, PE_SRC_HARD_RESET);
		}
		break;

	case PE_SRC_TRANSITION_SUPPLY:
		ret = pd_send_msg(pd, MSG_PS_RDY, NULL, 0, SOP_MSG);
		if (ret) {
			usbpd_err(&pd->dev, "Error sending PS_RDY\n");
			usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
			break;
		}

		usbpd_set_state(pd, PE_SRC_READY);
		break;

	case PE_SRC_READY:
		if (ctrl_recvd == MSG_GET_SOURCE_CAP) {
			ret = pd_send_msg(pd, MSG_SOURCE_CAPABILITIES,
					default_src_caps,
					ARRAY_SIZE(default_src_caps), SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending SRC CAPs\n");
				usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
				break;
			}
		} else if (data_recvd == MSG_REQUEST) {
			pd->rdo = pd->rx_payload[0];
			usbpd_set_state(pd, PE_SRC_NEGOTIATE_CAPABILITY);
		} else if (ctrl_recvd == MSG_DR_SWAP) {
			ret = pd_send_msg(pd, MSG_ACCEPT, NULL, 0, SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending Accept\n");
				usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
				break;
			}

			dr_swap(pd);
			kobject_uevent(&pd->dev.kobj, KOBJ_CHANGE);
		} else if (ctrl_recvd == MSG_PR_SWAP) {
			/* we'll happily accept Src->Sink requests anytime */
			ret = pd_send_msg(pd, MSG_ACCEPT, NULL, 0, SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending Accept\n");
				usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
				break;
			}

			pd->current_state = PE_PRS_SRC_SNK_TRANSITION_TO_OFF;
			queue_delayed_work(pd->wq, &pd->sm_work, 0);
			break;
		}
		break;

	case PE_SRC_HARD_RESET:
		pd_send_hard_reset(pd);
		pd->in_explicit_contract = false;

		msleep(PS_HARD_RESET_TIME);
		usbpd_set_state(pd, PE_SRC_TRANSITION_TO_DEFAULT);
		break;

	case PE_SNK_DISCOVERY:
		if (!pd->vbus_present) {
			/* Hard reset and VBUS didn't come back? */
			power_supply_get_property(pd->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &val);
			if (val.intval == POWER_SUPPLY_TYPEC_NONE) {
				pd->typec_mode = POWER_SUPPLY_TYPEC_NONE;
				queue_delayed_work(pd->wq, &pd->sm_work, 0);
			}
			break;
		}

		usbpd_set_state(pd, PE_SNK_WAIT_FOR_CAPABILITIES);
		break;

	case PE_SNK_WAIT_FOR_CAPABILITIES:
		if (data_recvd == MSG_SOURCE_CAPABILITIES) {
			val.intval = 1;
			power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_PD_ACTIVE, &val);

			val.intval = POWER_SUPPLY_TYPE_USB_PD;
			power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &val);

			usbpd_set_state(pd, PE_SNK_EVALUATE_CAPABILITY);
		} else if (pd->hard_reset_count < 3) {
			usbpd_set_state(pd, PE_SNK_HARD_RESET);
		} else if (pd->pd_connected) {
			usbpd_info(&pd->dev, "Sink hard reset count exceeded, forcing reconnect\n");
			usbpd_set_state(pd, PE_ERROR_RECOVERY);
		} else {
			usbpd_dbg(&pd->dev, "Sink hard reset count exceeded, disabling PD\n");
			val.intval = 0;
			power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_PD_ACTIVE, &val);
		}
		break;

	case PE_SNK_SELECT_CAPABILITY:
		if (ctrl_recvd == MSG_ACCEPT) {
			/* prepare for voltage increase/decrease */
			val.intval = pd->requested_voltage;
			power_supply_set_property(pd->usb_psy,
				pd->requested_voltage >= pd->current_voltage ?
					POWER_SUPPLY_PROP_VOLTAGE_MAX :
					POWER_SUPPLY_PROP_VOLTAGE_MIN,
					&val);

			val.intval = 0; /* suspend charging */
			power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &val);

			pd->selected_pdo = pd->requested_pdo;
			usbpd_set_state(pd, PE_SNK_TRANSITION_SINK);
		} else if (ctrl_recvd == MSG_REJECT || ctrl_recvd == MSG_WAIT) {
			if (pd->in_explicit_contract)
				usbpd_set_state(pd, PE_SNK_READY);
			else
				usbpd_set_state(pd,
						PE_SNK_WAIT_FOR_CAPABILITIES);
		} else {
			/* timed out; go to hard reset */
			usbpd_set_state(pd, PE_SNK_HARD_RESET);
		}
		break;

	case PE_SNK_TRANSITION_SINK:
		if (ctrl_recvd == MSG_PS_RDY) {
			val.intval = pd->requested_voltage;
			power_supply_set_property(pd->usb_psy,
				pd->requested_voltage >= pd->current_voltage ?
					POWER_SUPPLY_PROP_VOLTAGE_MIN :
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
			pd->current_voltage = pd->requested_voltage;

			/* resume charging */
			val.intval = pd->requested_current * 1000; /* mA->uA */
			power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &val);

			usbpd_set_state(pd, PE_SNK_READY);
		} else {
			/* timed out; go to hard reset */
			usbpd_set_state(pd, PE_SNK_HARD_RESET);
		}
		break;

	case PE_SNK_READY:
		if (data_recvd == MSG_SOURCE_CAPABILITIES)
			usbpd_set_state(pd, PE_SNK_EVALUATE_CAPABILITY);
		else if (ctrl_recvd == MSG_GET_SINK_CAP) {
			ret = pd_send_msg(pd, MSG_SINK_CAPABILITIES,
					default_snk_caps,
					ARRAY_SIZE(default_snk_caps), SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending Sink Caps\n");
				usbpd_set_state(pd, PE_SNK_SEND_SOFT_RESET);
			}
		} else if (ctrl_recvd == MSG_DR_SWAP) {
			ret = pd_send_msg(pd, MSG_ACCEPT, NULL, 0, SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending Accept\n");
				usbpd_set_state(pd, PE_SRC_SEND_SOFT_RESET);
				break;
			}

			dr_swap(pd);
			kobject_uevent(&pd->dev.kobj, KOBJ_CHANGE);
		} else if (ctrl_recvd == MSG_PR_SWAP) {
			/* TODO: should we Reject in certain circumstances? */
			ret = pd_send_msg(pd, MSG_ACCEPT, NULL, 0, SOP_MSG);
			if (ret) {
				usbpd_err(&pd->dev, "Error sending Accept\n");
				usbpd_set_state(pd, PE_SNK_SEND_SOFT_RESET);
				break;
			}

			pd->in_pr_swap = true;
			pd->current_state = PE_PRS_SNK_SRC_TRANSITION_TO_OFF;
			/* turn off sink */
			pd->in_explicit_contract = false;

			/*
			 * need to update PR bit in message header so that
			 * proper GoodCRC is sent when receiving next PS_RDY
			 */
			pd->current_pr = PR_SRC;
			pd_phy_update_roles(pd->current_dr, pd->current_pr);

			queue_delayed_work(pd->wq, &pd->sm_work,
					msecs_to_jiffies(PS_SOURCE_OFF));
			break;
		}
		break;

	case PE_SRC_SOFT_RESET:
	case PE_SNK_SOFT_RESET:
		/* Reset protocol layer */
		pd->tx_msgid = 0;
		pd->rx_msgid = -1;

		ret = pd_send_msg(pd, MSG_ACCEPT, NULL, 0, SOP_MSG);
		if (ret) {
			usbpd_err(&pd->dev, "%s: Error sending Accept, do Hard Reset\n",
					usbpd_state_strings[pd->current_state]);
			usbpd_set_state(pd, pd->current_pr == PR_SRC ?
					PE_SRC_HARD_RESET : PE_SNK_HARD_RESET);
			break;
		}

		usbpd_set_state(pd, pd->current_pr == PR_SRC ?
				PE_SRC_SEND_CAPABILITIES :
				PE_SNK_WAIT_FOR_CAPABILITIES);
		break;

	case PE_SRC_SEND_SOFT_RESET:
	case PE_SNK_SEND_SOFT_RESET:
		if (ctrl_recvd == MSG_ACCEPT) {
			usbpd_set_state(pd, pd->current_pr == PR_SRC ?
					PE_SRC_SEND_CAPABILITIES :
					PE_SNK_WAIT_FOR_CAPABILITIES);
		} else {
			usbpd_err(&pd->dev, "%s: Did not see Accept, do Hard Reset\n",
					usbpd_state_strings[pd->current_state]);
			usbpd_set_state(pd, pd->current_pr == PR_SRC ?
					PE_SRC_HARD_RESET : PE_SNK_HARD_RESET);
		}
		break;

	case PE_SNK_HARD_RESET:
		/* prepare charger for VBUS change */
		val.intval = 1;
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_PD_ACTIVE, &val);

		pd->requested_voltage = 5000000;
		pd->requested_current = max_sink_current;

		val.intval = pd->requested_voltage;
		power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MIN, &val);

		pd_send_hard_reset(pd);
		pd->in_explicit_contract = false;
		usbpd_set_state(pd, PE_SNK_TRANSITION_TO_DEFAULT);
		break;

	case PE_DRS_SEND_DR_SWAP:
		if (ctrl_recvd == MSG_ACCEPT)
			dr_swap(pd);

		usbpd_set_state(pd, pd->current_pr == PR_SRC ?
				PE_SRC_READY : PE_SNK_READY);
		break;

	case PE_PRS_SRC_SNK_SEND_SWAP:
		if (ctrl_recvd != MSG_ACCEPT) {
			pd->current_state = PE_SRC_READY;
			break;
		}

		pd->current_state = PE_PRS_SRC_SNK_TRANSITION_TO_OFF;
		/* fall-through */
	case PE_PRS_SRC_SNK_TRANSITION_TO_OFF:
		pd->in_pr_swap = true;
		pd->in_explicit_contract = false;

		regulator_disable(pd->vbus);
		set_power_role(pd, PR_SINK); /* switch Rp->Rd */
		pd->current_pr = PR_SINK;
		pd_phy_update_roles(pd->current_dr, pd->current_pr);

		ret = pd_send_msg(pd, MSG_PS_RDY, NULL, 0, SOP_MSG);
		if (ret) {
			usbpd_err(&pd->dev, "Error sending PS_RDY\n");
			usbpd_set_state(pd, PE_ERROR_RECOVERY);
			break;
		}

		pd->current_state = PE_PRS_SRC_SNK_WAIT_SOURCE_ON;
		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(PS_SOURCE_ON));
		break;

	case PE_PRS_SRC_SNK_WAIT_SOURCE_ON:
		if (ctrl_recvd == MSG_PS_RDY)
			usbpd_set_state(pd, PE_SNK_STARTUP);
		else
			usbpd_set_state(pd, PE_ERROR_RECOVERY);
		break;

	case PE_PRS_SNK_SRC_SEND_SWAP:
		if (ctrl_recvd != MSG_ACCEPT) {
			pd->current_state = PE_SNK_READY;
			break;
		}

		pd->in_pr_swap = true;
		pd->current_state = PE_PRS_SNK_SRC_TRANSITION_TO_OFF;
		/* turn off sink */
		pd->in_explicit_contract = false;

		/*
		 * need to update PR bit in message header so that
		 * proper GoodCRC is sent when receiving next PS_RDY
		 */
		pd->current_pr = PR_SRC;
		pd_phy_update_roles(pd->current_dr, pd->current_pr);

		queue_delayed_work(pd->wq, &pd->sm_work,
				msecs_to_jiffies(PS_SOURCE_OFF));
		break;

	case PE_PRS_SNK_SRC_TRANSITION_TO_OFF:
		if (ctrl_recvd != MSG_PS_RDY) {
			usbpd_set_state(pd, PE_ERROR_RECOVERY);
			break;
		}

		pd->current_state = PE_PRS_SNK_SRC_SOURCE_ON;
		/* fall-through */
	case PE_PRS_SNK_SRC_SOURCE_ON:
		set_power_role(pd, PR_SRC);
		ret = regulator_enable(pd->vbus);
		if (ret)
			usbpd_err(&pd->dev, "Unable to enable vbus\n");

		ret = pd_send_msg(pd, MSG_PS_RDY, NULL, 0, SOP_MSG);
		if (ret) {
			usbpd_err(&pd->dev, "Error sending PS_RDY\n");
			usbpd_set_state(pd, PE_ERROR_RECOVERY);
			break;
		}

		usbpd_set_state(pd, PE_SRC_STARTUP);
		break;

	default:
		usbpd_err(&pd->dev, "Unhandled state %s\n",
				usbpd_state_strings[pd->current_state]);
		break;
	}

	/* Rx message should have been consumed now */
	pd->rx_msg_type = pd->rx_msg_len = 0;
}

static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr)
{
	struct usbpd *pd = container_of(nb, struct usbpd, psy_nb);
	union power_supply_propval val;
	bool pd_allowed;
	enum power_supply_typec_mode typec_mode;
	enum power_supply_type psy_type;
	int ret;

	if (ptr != pd->usb_psy || evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_PD_ALLOWED, &val);
	if (ret) {
		usbpd_err(&pd->dev, "Unable to read USB PROP_PD_ALLOWED: %d\n",
				ret);
		return ret;
	}

	pd_allowed = val.intval;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret) {
		usbpd_err(&pd->dev, "Unable to read USB PRESENT: %d\n", ret);
		return ret;
	}

	pd->vbus_present = val.intval;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	if (ret) {
		usbpd_err(&pd->dev, "Unable to read USB TYPEC_MODE: %d\n", ret);
		return ret;
	}

	typec_mode = val.intval;

	/*
	 * Don't proceed if cable is connected but PD_ALLOWED is false.
	 * It means the PMIC may still be in the middle of performing
	 * charger type detection.
	 */
	if (!pd_allowed && typec_mode != POWER_SUPPLY_TYPEC_NONE)
		return 0;

	/*
	 * Workaround for PMIC HW bug.
	 *
	 * During hard reset or PR swap (sink to source) when VBUS goes to 0
	 * the CC logic will report this as a disconnection. In those cases it
	 * can be ignored, however the downside is that pd->hard_reset can be
	 * momentarily true even when a non-PD capable source is attached, and
	 * can't be distinguished from a physical disconnect. In that case,
	 * allow for the common case of disconnecting from an SDP.
	 *
	 * The less common case is a PD-capable SDP which will result in a
	 * hard reset getting treated like a disconnect. We can live with this
	 * until the HW bug is fixed: in which disconnection won't be reported
	 * on VBUS loss alone unless pullup is also removed from CC.
	 */
	if ((pd->hard_reset || pd->in_pr_swap) &&
			typec_mode == POWER_SUPPLY_TYPEC_NONE &&
			pd->psy_type != POWER_SUPPLY_TYPE_USB) {
		usbpd_dbg(&pd->dev, "Ignoring disconnect due to %s\n",
				pd->hard_reset ? "hard reset" : "PR swap");
		return 0;
	}

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &val);
	if (ret) {
		usbpd_err(&pd->dev, "Unable to read USB TYPE: %d\n", ret);
		return ret;
	}

	psy_type = val.intval;

	usbpd_dbg(&pd->dev, "typec mode:%d present:%d type:%d\n", typec_mode,
			pd->vbus_present, psy_type);

	/* any change? */
	if (pd->typec_mode == typec_mode && pd->psy_type == psy_type)
		return 0;

	pd->typec_mode = typec_mode;
	pd->psy_type = psy_type;

	switch (typec_mode) {
	/* Disconnect */
	case POWER_SUPPLY_TYPEC_NONE:
		queue_delayed_work(pd->wq, &pd->sm_work, 0);
		break;

	/* Sink states */
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		usbpd_info(&pd->dev, "Type-C Source connected\n");
		if (pd->current_pr != PR_SINK) {
			pd->current_pr = PR_SINK;
			queue_delayed_work(pd->wq, &pd->sm_work, 0);
		}
		break;

	/* Source states */
	case POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE:
	case POWER_SUPPLY_TYPEC_SINK:
		usbpd_info(&pd->dev, "Type-C Sink connected\n");
		if (pd->current_pr != PR_SRC) {
			pd->current_pr = PR_SRC;
			queue_delayed_work(pd->wq, &pd->sm_work, 0);
		}
		break;

	case POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY:
		usbpd_info(&pd->dev, "Type-C Debug Accessory connected\n");
		break;
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		usbpd_info(&pd->dev, "Type-C Analog Audio Adapter connected\n");
		break;
	default:
		usbpd_warn(&pd->dev, "Unsupported typec mode:%d\n", typec_mode);
		break;
	}

	return 0;
}

static int usbpd_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	int i;

	add_uevent_var(env, "DATA_ROLE=%s", pd->current_dr == DR_DFP ?
			"dfp" : "ufp");

	if (pd->current_pr == PR_SINK) {
		add_uevent_var(env, "POWER_ROLE=sink");
		add_uevent_var(env, "SRC_CAP_ID=%d", pd->src_cap_id);

		for (i = 0; i < ARRAY_SIZE(pd->received_pdos); i++)
			add_uevent_var(env, "PDO%d=%08x", i,
					pd->received_pdos[i]);

		add_uevent_var(env, "REQUESTED_PDO=%d", pd->requested_pdo);
		add_uevent_var(env, "SELECTED_PDO=%d", pd->selected_pdo);
	} else {
		add_uevent_var(env, "POWER_ROLE=source");
		for (i = 0; i < ARRAY_SIZE(default_src_caps); i++)
			add_uevent_var(env, "PDO%d=%08x", i,
					default_src_caps[i]);
	}

	add_uevent_var(env, "RDO=%08x", pd->rdo);
	add_uevent_var(env, "CONTRACT=%s", pd->in_explicit_contract ?
				"explicit" : "implicit");

	return 0;
}

static ssize_t contract_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			pd->in_explicit_contract ?  "explicit" : "implicit");
}
static DEVICE_ATTR_RO(contract);

static ssize_t current_pr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	const char *pr = "none";

	if (pd->current_pr == PR_SINK)
		pr = "sink";
	else if (pd->current_pr == PR_SRC)
		pr = "source";

	return snprintf(buf, PAGE_SIZE, "%s\n", pr);
}
static DEVICE_ATTR_RO(current_pr);

static ssize_t initial_pr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	const char *pr = "none";

	if (pd->typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT)
		pr = "sink";
	else if (pd->typec_mode >= POWER_SUPPLY_TYPEC_SINK)
		pr = "source";

	return snprintf(buf, PAGE_SIZE, "%s\n", pr);
}
static DEVICE_ATTR_RO(initial_pr);

static ssize_t current_dr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	const char *dr = "none";

	if (pd->current_dr == DR_UFP)
		dr = "ufp";
	else if (pd->current_dr == DR_DFP)
		dr = "dfp";

	return snprintf(buf, PAGE_SIZE, "%s\n", dr);
}
static DEVICE_ATTR_RO(current_dr);

static ssize_t initial_dr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	const char *dr = "none";

	if (pd->typec_mode >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT)
		dr = "ufp";
	else if (pd->typec_mode >= POWER_SUPPLY_TYPEC_SINK)
		dr = "dfp";

	return snprintf(buf, PAGE_SIZE, "%s\n", dr);
}
static DEVICE_ATTR_RO(initial_dr);

static ssize_t src_cap_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pd->src_cap_id);
}
static DEVICE_ATTR_RO(src_cap_id);

/* Dump received source PDOs in human-readable format */
static ssize_t pdo_h_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	int i;
	ssize_t cnt = 0;

	for (i = 0; i < ARRAY_SIZE(pd->received_pdos); i++) {
		u32 pdo = pd->received_pdos[i];

		if (pdo == 0)
			break;

		cnt += scnprintf(&buf[cnt], PAGE_SIZE - cnt, "PDO %d\n", i + 1);

		if (PD_SRC_PDO_TYPE(pdo) == PD_SRC_PDO_TYPE_FIXED) {
			cnt += scnprintf(&buf[cnt], PAGE_SIZE - cnt,
					"\tFixed supply\n"
					"\tDual-Role Power:%d\n"
					"\tUSB Suspend Supported:%d\n"
					"\tExternally Powered:%d\n"
					"\tUSB Communications Capable:%d\n"
					"\tData Role Swap:%d\n"
					"\tPeak Current:%d\n"
					"\tVoltage:%d (mV)\n"
					"\tMax Current:%d (mA)\n",
					PD_SRC_PDO_FIXED_PR_SWAP(pdo),
					PD_SRC_PDO_FIXED_USB_SUSP(pdo),
					PD_SRC_PDO_FIXED_EXT_POWERED(pdo),
					PD_SRC_PDO_FIXED_USB_COMM(pdo),
					PD_SRC_PDO_FIXED_DR_SWAP(pdo),
					PD_SRC_PDO_FIXED_PEAK_CURR(pdo),
					PD_SRC_PDO_FIXED_VOLTAGE(pdo) * 50,
					PD_SRC_PDO_FIXED_MAX_CURR(pdo) * 10);
		} else if (PD_SRC_PDO_TYPE(pdo) == PD_SRC_PDO_TYPE_BATTERY) {
			cnt += scnprintf(&buf[cnt], PAGE_SIZE - cnt,
					"\tBattery supply\n"
					"\tMax Voltage:%d (mV)\n"
					"\tMin Voltage:%d (mV)\n"
					"\tMax Power:%d (mW)\n",
					PD_SRC_PDO_VAR_BATT_MAX_VOLT(pdo),
					PD_SRC_PDO_VAR_BATT_MIN_VOLT(pdo),
					PD_SRC_PDO_VAR_BATT_MAX(pdo));
		} else if (PD_SRC_PDO_TYPE(pdo) == PD_SRC_PDO_TYPE_VARIABLE) {
			cnt += scnprintf(&buf[cnt], PAGE_SIZE - cnt,
					"\tVariable supply\n"
					"\tMax Voltage:%d (mV)\n"
					"\tMin Voltage:%d (mV)\n"
					"\tMax Current:%d (mA)\n",
					PD_SRC_PDO_VAR_BATT_MAX_VOLT(pdo),
					PD_SRC_PDO_VAR_BATT_MIN_VOLT(pdo),
					PD_SRC_PDO_VAR_BATT_MAX(pdo));
		} else {
			cnt += scnprintf(&buf[cnt], PAGE_SIZE - cnt,
					"Invalid PDO\n");
		}

		buf[cnt++] = '\n';
	}

	return cnt;
}
static DEVICE_ATTR_RO(pdo_h);

static ssize_t pdo_n_show(struct device *dev, struct device_attribute *attr,
		char *buf);

#define PDO_ATTR(n) {					\
	.attr	= { .name = __stringify(pdo##n), .mode = S_IRUGO },	\
	.show	= pdo_n_show,				\
}
static struct device_attribute dev_attr_pdos[] = {
	PDO_ATTR(1),
	PDO_ATTR(2),
	PDO_ATTR(3),
	PDO_ATTR(4),
	PDO_ATTR(5),
	PDO_ATTR(6),
	PDO_ATTR(7),
};

static ssize_t pdo_n_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(dev_attr_pdos); i++)
		if (attr == &dev_attr_pdos[i])
			/* dump the PDO as a hex string */
			return snprintf(buf, PAGE_SIZE, "%08x\n",
					pd->received_pdos[i]);

	usbpd_err(&pd->dev, "Invalid PDO index\n");
	return -EINVAL;
}

static ssize_t select_pdo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	int src_cap_id;
	int pdo;
	int ret;

	/* Only allowed if we are already in explicit sink contract */
	if (pd->current_state != PE_SNK_READY) {
		usbpd_err(&pd->dev, "select_pdo: Cannot select new PDO yet\n");
		return -EBUSY;
	}

	if (sscanf(buf, "%d %d\n", &src_cap_id, &pdo) != 2) {
		usbpd_err(&pd->dev, "select_pdo: Must specify <src cap id> <PDO>\n");
		return -EINVAL;
	}

	if (src_cap_id != pd->src_cap_id) {
		usbpd_err(&pd->dev, "select_pdo: src_cap_id mismatch.  Requested:%d, current:%d\n",
				src_cap_id, pd->src_cap_id);
		return -EINVAL;
	}

	if (pdo < 1 || pdo > 7) {
		usbpd_err(&pd->dev, "select_pdo: invalid PDO:%d\n", pdo);
		return -EINVAL;
	}

	ret = pd_select_pdo(pd, pdo);
	if (ret)
		return ret;

	usbpd_set_state(pd, PE_SNK_SELECT_CAPABILITY);

	return size;
}

static ssize_t select_pdo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pd->selected_pdo);
}
static DEVICE_ATTR_RW(select_pdo);

static ssize_t rdo_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usbpd *pd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "Request Data Object: %08x\n\n"
			"Obj Pos:%d\n"
			"Giveback:%d\n"
			"Capability Mismatch:%d\n"
			"USB Communications Capable:%d\n"
			"No USB Suspend:%d\n"
			"Operating Current/Power:%d (mA) / %d (mW)\n"
			"%s Current/Power:%d (mA) / %d (mW)\n",
			pd->rdo,
			PD_RDO_OBJ_POS(pd->rdo),
			PD_RDO_GIVEBACK(pd->rdo),
			PD_RDO_MISMATCH(pd->rdo),
			PD_RDO_USB_COMM(pd->rdo),
			PD_RDO_NO_USB_SUSP(pd->rdo),
			PD_RDO_FIXED_CURR(pd->rdo) * 10,
			PD_RDO_FIXED_CURR(pd->rdo) * 250,
			PD_RDO_GIVEBACK(pd->rdo) ? "Min" : "Max",
			PD_RDO_FIXED_CURR_MINMAX(pd->rdo) * 10,
			PD_RDO_FIXED_CURR_MINMAX(pd->rdo) * 250);
}
static DEVICE_ATTR_RO(rdo);

static ssize_t hard_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct usbpd *pd = dev_get_drvdata(dev);
	int val = 0;

	if (sscanf(buf, "%d\n", &val) != 1)
		return -EINVAL;

	if (val)
		usbpd_set_state(pd, pd->current_pr == PR_SRC ?
				PE_SRC_HARD_RESET : PE_SNK_HARD_RESET);

	return size;
}
static DEVICE_ATTR_WO(hard_reset);

static struct attribute *usbpd_attrs[] = {
	&dev_attr_contract.attr,
	&dev_attr_initial_pr.attr,
	&dev_attr_current_pr.attr,
	&dev_attr_initial_dr.attr,
	&dev_attr_current_dr.attr,
	&dev_attr_src_cap_id.attr,
	&dev_attr_pdo_h.attr,
	&dev_attr_pdos[0].attr,
	&dev_attr_pdos[1].attr,
	&dev_attr_pdos[2].attr,
	&dev_attr_pdos[3].attr,
	&dev_attr_pdos[4].attr,
	&dev_attr_pdos[5].attr,
	&dev_attr_pdos[6].attr,
	&dev_attr_select_pdo.attr,
	&dev_attr_rdo.attr,
	&dev_attr_hard_reset.attr,
	NULL,
};
ATTRIBUTE_GROUPS(usbpd);

static struct class usbpd_class = {
	.name = "usbpd",
	.owner = THIS_MODULE,
	.dev_uevent = usbpd_uevent,
	.dev_groups = usbpd_groups,
};

static int num_pd_instances;

/**
 * usbpd_create - Create a new instance of USB PD protocol/policy engine
 * @parent - parent device to associate with
 *
 * This creates a new usbpd class device which manages the state of a
 * USB PD-capable port. The parent device that is passed in should be
 * associated with the physical device port, e.g. a PD PHY.
 *
 * Return: struct usbpd pointer, or an ERR_PTR value
 */
struct usbpd *usbpd_create(struct device *parent)
{
	int ret;
	struct usbpd *pd;

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	device_initialize(&pd->dev);
	pd->dev.class = &usbpd_class;
	pd->dev.parent = parent;
	dev_set_drvdata(&pd->dev, pd);

	ret = dev_set_name(&pd->dev, "usbpd%d", num_pd_instances++);
	if (ret)
		goto free_pd;

	ret = device_add(&pd->dev);
	if (ret)
		goto free_pd;

	pd->wq = alloc_ordered_workqueue("usbpd_wq", WQ_FREEZABLE);
	if (!pd->wq) {
		ret = -ENOMEM;
		goto del_pd;
	}
	INIT_DELAYED_WORK(&pd->sm_work, usbpd_sm);

	pd->usb_psy = power_supply_get_by_name("usb");
	if (!pd->usb_psy) {
		usbpd_dbg(&pd->dev, "Could not get USB power_supply, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto destroy_wq;
	}

	pd->psy_nb.notifier_call = psy_changed;
	ret = power_supply_reg_notifier(&pd->psy_nb);
	if (ret)
		goto put_psy;

	/*
	 * associate extcon with the parent dev as it could have a DT
	 * node which will be useful for extcon_get_edev_by_phandle()
	 */
	pd->extcon = devm_extcon_dev_allocate(parent, usbpd_extcon_cable);
	if (IS_ERR(pd->extcon)) {
		usbpd_err(&pd->dev, "failed to allocate extcon device\n");
		ret = PTR_ERR(pd->extcon);
		goto unreg_psy;
	}

	pd->extcon->mutually_exclusive = usbpd_extcon_exclusive;
	ret = devm_extcon_dev_register(parent, pd->extcon);
	if (ret) {
		usbpd_err(&pd->dev, "failed to register extcon device\n");
		goto unreg_psy;
	}

	pd->vbus = devm_regulator_get(parent, "vbus");
	if (IS_ERR(pd->vbus)) {
		ret = PTR_ERR(pd->vbus);
		goto unreg_psy;
	}

	pd->vconn = devm_regulator_get(parent, "vconn");
	if (IS_ERR(pd->vconn)) {
		ret = PTR_ERR(pd->vconn);
		goto unreg_psy;
	}

	pd->current_pr = PR_NONE;
	pd->current_dr = DR_NONE;
	list_add_tail(&pd->instance, &_usbpd);

	/* force read initial power_supply values */
	psy_changed(&pd->psy_nb, PSY_EVENT_PROP_CHANGED, pd->usb_psy);

	return pd;

unreg_psy:
	power_supply_unreg_notifier(&pd->psy_nb);
put_psy:
	power_supply_put(pd->usb_psy);
destroy_wq:
	destroy_workqueue(pd->wq);
del_pd:
	device_del(&pd->dev);
free_pd:
	num_pd_instances--;
	kfree(pd);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(usbpd_create);

/**
 * usbpd_destroy - Removes and frees a usbpd instance
 * @pd: the instance to destroy
 */
void usbpd_destroy(struct usbpd *pd)
{
	if (!pd)
		return;

	list_del(&pd->instance);
	power_supply_unreg_notifier(&pd->psy_nb);
	power_supply_put(pd->usb_psy);
	destroy_workqueue(pd->wq);
	device_del(&pd->dev);
	kfree(pd);
}
EXPORT_SYMBOL(usbpd_destroy);

static int __init usbpd_init(void)
{
	usbpd_ipc_log = ipc_log_context_create(NUM_LOG_PAGES, "usb_pd", 0);
	return class_register(&usbpd_class);
}
module_init(usbpd_init);

static void __exit usbpd_exit(void)
{
	class_unregister(&usbpd_class);
}
module_exit(usbpd_exit);

MODULE_DESCRIPTION("USB Power Delivery Policy Engine");
MODULE_LICENSE("GPL v2");
