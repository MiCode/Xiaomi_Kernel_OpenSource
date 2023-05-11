/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * PD Device Policy Manager for Direct Charge
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

#include <linux/random.h>
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "pd_dpm_prv.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_ALT_MODE_RTDC

#define RTDC_UVDM_EN_UNLOCK		0x2024
#define RTDC_UVDM_RECV_EN_UNLOCK	0x4024
#define RTDC_SVDM_PPS_AUTHORIZATION	0x10

#define RTDC_VALID_MODE				0x01
#define RTDC_UVDM_EN_UNLOCK_SUCCESS		0x01

void crcbits(uint32_t data, uint32_t *crc, uint32_t *ppolynomial)
{
	uint32_t i, newbit, newword, rl_crc;

	for (i = 0; i < 32; i++) {
		newbit = ((*crc >> 31) ^ ((data >> i) & 1)) & 1;
		if (newbit)
			newword = *ppolynomial;
		else
			newword = 0;
		rl_crc = (*crc << 1) | newbit;
		*crc = rl_crc ^ newword;
	}
}

uint32_t crcwrap(uint32_t V)
{
	uint32_t   ret = 0, i, j, bit;

	V = ~V;
	for (i = 0; i < 32; i++) {
		j = 31 - i;
		bit = (V >> i) & 1;
		ret |= bit << j;
	}

	return ret;
}

static uint32_t dc_get_random_code(void)
{
	uint32_t num;

	get_random_bytes(&num, sizeof(num));
	return num;
}

static uint32_t dc_get_authorization_code(uint32_t data)
{
	uint32_t dwpolynomial = 0x04C11DB6, dwCrc = 0xFFFFFFFF;

	crcbits(data, &dwCrc, &dwpolynomial);
	dwCrc = crcwrap(dwCrc);
	return dwCrc;
}

static inline bool dc_dfp_send_en_unlock(struct pd_port *pd_port,
		uint32_t cmd, uint32_t data0, uint32_t data1)
{
	pd_port->uvdm_cnt = 3;
	pd_port->uvdm_wait_resp = true;

	pd_port->uvdm_data[0] = PD_UVDM_HDR(USB_VID_DIRECTCHARGE, cmd);
	pd_port->uvdm_data[1] = data0;
	pd_port->uvdm_data[2] = data1;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->pe_data.dc_pps_mode) {
		pd_port->uvdm_data[0] = VDO_S(
			USB_VID_DIRECTCHARGE,
			SVDM_REV20,
			CMDT_INIT,
			RTDC_SVDM_PPS_AUTHORIZATION,
			0);
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	return pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_UVDM);
}

enum pd_dc_dfp_state {
	DC_DFP_NONE = 0,
	DC_DFP_DISCOVER_ID,
	DC_DFP_DISCOVER_SVIDS,
	DC_DFP_DISCOVER_MODES,
	DC_DFP_ENTER_MODE,
	DC_DFP_EN_UNLOCK1,
	DC_DFP_EN_UNLOCK2,
	DC_DFP_OPERATION,

#ifdef RTDC_TA_EMULATE
	DC_UFP_T0,
	DC_UFP_T1,
	DC_UFP_T2,
#endif

	DC_DFP_STATE_NR,

	DC_DFP_ERR = 0X10,

	DC_DFP_ERR_DISCOVER_ID_TYPE,
	DC_DFP_ERR_DISCOVER_ID_NAK_TIMEOUT,

	DC_DFP_ERR_DISCOVER_SVID_DC_SID,
	DC_DFP_ERR_DISCOVER_SVID_NAK_TIMEOUT,

	DC_DFP_ERR_DISCOVER_CABLE,

	DC_DFP_ERR_DISCOVER_MODE_DC_SID,
	DC_DFP_ERR_DISCOVER_MODE_CAP,
	DC_DFP_ERR_DISCOVER_MODE_NAK_TIMEROUT,

	DC_DFP_ERR_ENTER_MODE_DC_SID,
	DC_DFP_ERR_ENTER_MODE_NAK_TIMEOUT,

	DC_DFP_ERR_EXIT_MODE_DC_SID,
	DC_DFP_ERR_EXIT_MODE_NAK_TIMEOUT,

	DC_DFP_ERR_EN_UNLOCK1_FAILED,
	DC_DFP_ERR_EN_UNLOCK1_NAK_TIMEOUT,

	DC_DFP_ERR_EN_UNLOCK2_FAILED,
	DC_DFP_ERR_EN_UNLOCK2_NAK_TIMEOUT,

	DC_DFP_ERR_PD_REV30,
};

#if DC_DBG_ENABLE
static const char * const dc_dfp_state_name[] = {
	"dc_dfp_none",
	"dc_dfp_discover_id",
	"dc_dfp_discover_svids",
	"dc_dfp_discover_modes",
	"dc_dfp_enter_mode",
	"dc_dfp_en_unlock1",
	"dc_dfp_en_unlock2",
	"dc_dfp_operation",

#ifdef RTDC_TA_EMULATE
	"dc1",
	"dc2",
	"dc3",
#endif

};
#endif /* DC_DBG_ENABLE */

void dc_dfp_set_state(struct pd_port *pd_port, uint8_t state)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	pd_port->dc_dfp_state = state;

	if (pd_port->dc_dfp_state < DC_DFP_STATE_NR)
		DC_DBG("%s\n", dc_dfp_state_name[state]);
	else
		DC_DBG("dc_dfp_stop (%d)\n", state);
}

bool dc_dfp_start_en_unlock1(struct pd_port *pd_port)
{
	uint32_t rn_code[2];
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	rn_code[0] = dc_get_random_code();
	rn_code[1] = dc_get_random_code();
	pd_port->dc_pass_code = dc_get_authorization_code(
			(rn_code[0] & 0xffff) | (rn_code[1] & 0xffff0000));

	DC_DBG("en_unlock1: 0x%x, 0x%x\n", rn_code[0], rn_code[1]);

	dc_dfp_send_en_unlock(
			pd_port, RTDC_UVDM_EN_UNLOCK, rn_code[0], rn_code[1]);

	dc_dfp_set_state(pd_port, DC_DFP_EN_UNLOCK1);

	return true;
}

#define SVDM_CMD_STATE_MASK(raw)		(raw & (0x80df))
#define SVDM_CMD_STATE(cmd, cmd_type)	\
	((1 << 15) | (cmd & 0x1f) | ((cmd_type & 0x03) << 6))

bool dc_dfp_verify_en_unlock1(struct pd_port *pd_port)
{
	uint32_t resp_cmd, expect_resp;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	expect_resp = RTDC_UVDM_RECV_EN_UNLOCK;
	resp_cmd = PD_UVDM_HDR_CMD(pd_port->uvdm_data[0]);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->pe_data.dc_pps_mode) {
		resp_cmd = SVDM_CMD_STATE_MASK(pd_port->uvdm_data[0]);
		expect_resp = SVDM_CMD_STATE(
			RTDC_SVDM_PPS_AUTHORIZATION, CMDT_RSP_ACK);
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (resp_cmd != expect_resp) {
		DC_INFO("en_unlock1: unexpect resp (0x%x)\n", resp_cmd);
		dc_dfp_set_state(pd_port, DC_DFP_ERR_EN_UNLOCK1_FAILED);
		return false;
	}

	if (pd_port->dc_pass_code != pd_port->uvdm_data[1]) {
		DC_INFO("en_unlock1: pass wrong 0x%x 0x%x\n",
				pd_port->dc_pass_code, pd_port->uvdm_data[1]);
		dc_dfp_set_state(pd_port, DC_DFP_ERR_EN_UNLOCK1_FAILED);
		return false;
	}

	return true;
}


bool dc_dfp_start_en_unlock2(struct pd_port *pd_port)
{
	uint32_t rn_code = dc_get_random_code();
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	pd_port->dc_pass_code =
		dc_get_authorization_code(pd_port->uvdm_data[2]);

	DC_DBG("en_unlock2: 0x%x, 0x%x\n", pd_port->dc_pass_code, rn_code);

	dc_dfp_send_en_unlock(pd_port, RTDC_UVDM_EN_UNLOCK,
			pd_port->dc_pass_code, rn_code);

	dc_dfp_set_state(pd_port, DC_DFP_EN_UNLOCK2);

	return true;
}

bool dc_dfp_verify_en_unlock2(struct pd_port *pd_port)
{
	uint32_t resp_cmd, expect_resp;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	expect_resp = RTDC_UVDM_RECV_EN_UNLOCK;
	resp_cmd = PD_UVDM_HDR_CMD(pd_port->uvdm_data[0]);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->pe_data.dc_pps_mode) {
		resp_cmd = SVDM_CMD_STATE_MASK(pd_port->uvdm_data[0]);
		expect_resp = SVDM_CMD_STATE(
			RTDC_SVDM_PPS_AUTHORIZATION, CMDT_RSP_ACK);
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (resp_cmd != expect_resp) {
		DC_INFO("en_unlock2: unexpect resp (0x%x)\n", resp_cmd);
		dc_dfp_set_state(pd_port, DC_DFP_ERR_EN_UNLOCK2_FAILED);
		return false;
	}

	if (pd_port->uvdm_data[1] != RTDC_UVDM_EN_UNLOCK_SUCCESS) {
		DC_INFO("en_unlock2: failed\n");
		dc_dfp_set_state(pd_port, DC_DFP_ERR_EN_UNLOCK2_FAILED);
		return false;
	}

	return true;
}

bool dc_dfp_notify_pe_startup(
		struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	if (!(pd_port->id_vdos[0] & PD_IDH_MODAL_SUPPORT))
		return false;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMP_ENTER_DC_MODE)
		dc_dfp_set_state(pd_port, DC_DFP_DISCOVER_ID);

#ifdef RTDC_TA_EMULATE
	dc_dfp_set_state(pd_port, DC_UFP_T0);
#endif

	return true;
}

int dc_dfp_notify_pe_ready(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data)
{
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#ifdef RTDC_TA_EMULATE
	if (pd_port->data_role == PD_ROLE_DFP && svid_data->exist) {
		pd_put_tcp_pd_event(pd_port, TCP_DPM_EVT_DR_SWAP_AS_UFP);
		return 1;
	} else {
		return 0;
	}
#endif

	if (pd_port->data_role != PD_ROLE_DFP)
		return 0;

	if (pd_port->dc_dfp_state != DC_DFP_DISCOVER_MODES)
		return 0;

#ifdef CONFIG_USB_PD_RTDC_CHECK_CABLE
	if (!pd_port->pe_data.power_cable_present) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_CABLE);
		return 0;
	}

	if (pd_get_cable_curr_lvl(pd_port) != CABLE_CURR_5A) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_CABLE);
		return 0;
	}
#endif	/* CONFIG_USB_PD_RTDC_CHECK_CABLE */

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	/* If TA support pd revision30, using standard PPS flow */
	if (pd_check_rev30(pd_port)) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_PD_REV30);
		return 0;
	}

	if (pd_is_source_support_apdo(pd_port)) {
		DC_INFO("pps_mode\n");
		pd_port->pe_data.dc_pps_mode = true;
		return dc_dfp_start_en_unlock1(pd_port);
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	pd_port->mode_svid = USB_VID_DIRECTCHARGE;
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DISCOVER_MODES);
	return 1;
}

bool dc_dfp_notify_discover_id(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack)
{
	if (pd_port->dc_dfp_state != DC_DFP_DISCOVER_ID)
		return true;

	if (!ack) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_ID_NAK_TIMEOUT);
		return true;
	}

	/* TODO: Check device ID or Type */

	dc_dfp_set_state(pd_port, DC_DFP_DISCOVER_SVIDS);
	return true;
}

bool dc_dfp_notify_discover_svid(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, bool ack)
{
	if (pd_port->dc_dfp_state != DC_DFP_DISCOVER_SVIDS)
		return false;

	if (!ack) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_SVID_NAK_TIMEOUT);
		return false;
	}

	if (!svid_data->exist) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_SVID_DC_SID);
		return false;
	}

	dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_CABLE_FLOW);
	dc_dfp_set_state(pd_port, DC_DFP_DISCOVER_MODES);
	return true;
}

bool dc_dfp_notify_discover_modes(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, bool ack)
{
	if (pd_port->dc_dfp_state != DC_DFP_DISCOVER_MODES)
		return false;

	if (!ack) {
		dc_dfp_set_state(pd_port,
			DC_DFP_ERR_DISCOVER_MODE_NAK_TIMEROUT);
		return false;
	}

	if (svid_data->remote_mode.mode_cnt == 0) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_MODE_DC_SID);
		return false;
	}

	if (svid_data->remote_mode.mode_vdo[0] != RTDC_VALID_MODE) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_DISCOVER_MODE_CAP);
		return false;
	}

	pd_port->mode_obj_pos = 1;
	dc_dfp_set_state(pd_port, DC_DFP_ENTER_MODE);
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_ENTER_MODE);
	return true;
}

bool dc_dfp_notify_enter_mode(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, uint8_t ops, bool ack)
{
	if (pd_port->dc_dfp_state != DC_DFP_ENTER_MODE)
		return true;

	if (!ack) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_ENTER_MODE_NAK_TIMEOUT);
		return true;
	}

	if (svid_data->active_mode == 0) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_ENTER_MODE_DC_SID);
		return false;
	}

	return dc_dfp_start_en_unlock1(pd_port);
}

bool dc_dfp_notify_exit_mode(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, uint8_t ops)
{
	if (pd_port->dc_dfp_state <= DC_DFP_ENTER_MODE)
		return false;

	if (svid_data->svid != USB_VID_DIRECTCHARGE)
		return false;

	dc_dfp_set_state(pd_port, DC_DFP_NONE);
	return true;
}

static inline bool dc_dfp_notify_en_unlock1(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack)
{
	if (pd_port->dc_dfp_state != DC_DFP_EN_UNLOCK1)
		return false;

	if (!ack) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_EN_UNLOCK1_NAK_TIMEOUT);
		return false;
	}

	if (!dc_dfp_verify_en_unlock1(pd_port))
		return false;

	return dc_dfp_start_en_unlock2(pd_port);
}

static inline bool dc_dfp_notify_en_unlock2(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack)
{
	if (pd_port->dc_dfp_state != DC_DFP_EN_UNLOCK2)
		return false;

	if (!ack) {
		dc_dfp_set_state(pd_port, DC_DFP_ERR_EN_UNLOCK2_NAK_TIMEOUT);
		return false;
	}

	if (!dc_dfp_verify_en_unlock2(pd_port))
		return false;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
#ifdef CONFIG_USB_PD_REV30_SYNC_SPEC_REV
	if (pd_port->pe_data.dc_pps_mode)
		pd_port->pd_revision[0] = PD_REV30;
#endif	/* CONFIG_USB_PD_REV30_SYNC_SPEC_REV */
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	dc_dfp_set_state(pd_port, DC_DFP_OPERATION);

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	/* PPS shoult not use en_unlock to notify system */
	if (pd_port->pe_data.dc_pps_mode)
		return true;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	tcpci_dc_notify_en_unlock(pd_port->tcpc);
	return true;
}

bool dc_dfp_notify_uvdm(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack)
{
	switch (pd_port->dc_dfp_state) {
	case DC_DFP_EN_UNLOCK1:
		dc_dfp_notify_en_unlock1(pd_port, svid_data, ack);
		break;

	case DC_DFP_EN_UNLOCK2:
		dc_dfp_notify_en_unlock2(pd_port, svid_data, ack);
		break;
	}

	return true;
}

bool dc_ufp_notify_uvdm(struct pd_port *pd_port,
				struct svdm_svid_data *svid_data)
{
#ifdef RTDC_TA_EMULATE
	uint32_t reply_cmd[3];
	uint32_t recv_code[2], rn_code, pass_code;

	reply_cmd[0] = PD_UVDM_HDR(0x29cf, RTDC_UVDM_EN_UNLOCK);
	uint32_t cmd = PD_UVDM_HDR_CMD(pd_port->uvdm_data[0]);

	if (cmd != RTDC_UVDM_EN_UNLOCK) {
		DC_INFO("What!?");
		return true;
	}

	switch (pd_port->dc_dfp_state) {
	case DC_UFP_T0:
		{
			recv_code[0] = pd_port->uvdm_data[1];
			recv_code[1] = pd_port->uvdm_data[2];
			DC_INFO("T0: recv_code: 0x%x, 0x%x\n",
						recv_code[0], recv_code[1]);

			pass_code = dc_get_authorization_code(
					(recv_code[0] & 0xffff) |
					(recv_code[1] & 0xffff0000));

			rn_code = dc_get_random_code();

			pd_port->dc_pass_code =
				dc_get_authorization_code(rn_code);

			DC_INFO("T0: reply: 0x%x, 0x%x\n",
				rn_code, pd_port->dc_pass_code);

			reply_cmd[1] = pass_code;
			reply_cmd[2] = rn_code;

			pd_reply_custom_vdm(pd_port, TCPC_TX_SOP, 3, reply_cmd);
			dc_dfp_set_state(pd_port, DC_UFP_T1);
		}
		break;

	case DC_UFP_T1:
		{
			recv_code[0] = pd_port->uvdm_data[1];
			recv_code[1] = pd_port->uvdm_data[2];
			DC_INFO("T1: recv_code: 0x%x, 0x%x\n",
				recv_code[0], recv_code[1]);

			if (recv_code[0] != pd_port->dc_pass_code) {
				DC_INFO("T1: pass_code error\n");
				reply_cmd[1] = 0;
			} else {
				DC_INFO("T1: pass_code success\n");
				reply_cmd[1] = 1;
			}

			reply_cmd[2] = dc_get_random_code();
			pd_reply_custom_vdm(pd_port, TCPC_TX_SOP, 3, reply_cmd);
			dc_dfp_set_state(pd_port, DC_UFP_T2);
		}
		break;
	}
#endif	/* RTDC_TA_EMULATE */
	return true;
}

bool dc_reset_state(struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	dc_dfp_set_state(pd_port, DC_DFP_NONE);
	return true;
}

bool dc_parse_svid_data(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data)
{
	svid_data->local_mode.mode_cnt = 1;
	svid_data->local_mode.mode_vdo[0] = 0x00;
	pd_port->dpm_caps |= DPM_CAP_ATTEMP_ENTER_DC_MODE;
	return true;
}
#endif	/* CONFIG_USB_PD_ALT_MODE_RTDC */
#endif	/* CONFIG_USB_POWER_DELIVERY */
