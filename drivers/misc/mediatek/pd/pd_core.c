/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/pd_core.c
 * Power Delvery Core Driver
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/slab.h>

#include "inc/tcpci.h"
#include "inc/pd_core.h"
#include "inc/d_dpm_core.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci_event.h"
#include "inc/pd_policy_engine.h"

#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include <linux/usb/class-dual-role.h>
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

/* From DTS */


static int pd_parse_pdata(pd_port_t *pd_port)
{
	struct device_node *np;
	int ret = 0, i;

	pr_info("%s\n", __func__);
	np = of_find_node_by_name(pd_port->tcpc_dev->dev.of_node, "pd-data");

	if (np) {
		ret = of_property_read_u32(np, "pd,source-pdo-size",
				(u32 *)&pd_port->local_src_cap_default.nr);
		if (ret < 0)
			pr_err("%s get source pdo size fail\n", __func__);

		ret = of_property_read_u32_array(np, "pd,source-pdo-data",
			(u32 *)pd_port->local_src_cap_default.pdos,
			pd_port->local_src_cap_default.nr);
		if (ret < 0)
			pr_err("%s get source pdo data fail\n", __func__);

		pr_info("%s src pdo data =\n", __func__);
		for (i = 0; i < pd_port->local_src_cap_default.nr; i++) {
			pr_info("%s %d: 0x%08x\n", __func__, i,
				pd_port->local_src_cap_default.pdos[i]);
		}

		ret = of_property_read_u32(np, "pd,sink-pdo-size",
					(u32 *)&pd_port->local_snk_cap.nr);
		if (ret < 0)
			pr_err("%s get sink pdo size fail\n", __func__);

		ret = of_property_read_u32_array(np, "pd,sink-pdo-data",
			(u32 *)pd_port->local_snk_cap.pdos,
				pd_port->local_snk_cap.nr);
		if (ret < 0)
			pr_err("%s get sink pdo data fail\n", __func__);

		pr_info("%s snk pdo data =\n", __func__);
		for (i = 0; i < pd_port->local_snk_cap.nr; i++) {
			pr_info("%s %d: 0x%08x\n", __func__, i,
				pd_port->local_snk_cap.pdos[i]);
		}

		ret = of_property_read_u32(np, "pd,id-vdo-size",
					(u32 *)&pd_port->id_vdo_nr);
		if (ret < 0)
			pr_err("%s get id vdo size fail\n", __func__);
		ret = of_property_read_u32_array(np, "pd,id-vdo-data",
			(u32 *)pd_port->id_vdos, pd_port->id_vdo_nr);
		if (ret < 0)
			pr_err("%s get id vdo data fail\n", __func__);

		pr_info("%s id vdos data =\n", __func__);
		for (i = 0; i < pd_port->id_vdo_nr; i++)
			pr_info("%s %d: 0x%08x\n", __func__, i,
			pd_port->id_vdos[i]);
	}

	return 0;
}

#define DEFAULT_DP_ROLE_CAP				(MODE_DP_SRC)
#define DEFAULT_DP_FIRST_CONNECTED		(DPSTS_DFP_D_CONNECTED)
#define DEFAULT_DP_SECOND_CONNECTED		(DPSTS_DFP_D_CONNECTED)

#ifdef CONFIG_USB_PD_ALT_MODE
static const struct {
	const char *prop_name;
	uint32_t mode;
} supported_dp_pin_modes[] = {
	{ "pin_assignment,mode_a", MODE_DP_PIN_A },
	{ "pin_assignment,mode_b", MODE_DP_PIN_B },
	{ "pin_assignment,mode_c", MODE_DP_PIN_C },
	{ "pin_assignment,mode_d", MODE_DP_PIN_D },
	{ "pin_assignment,mode_e", MODE_DP_PIN_E },
	{ "pin_assignment,mode_f", MODE_DP_PIN_F },
};

static const struct {
	const char *conn_mode;
	uint32_t val;
} dp_connect_mode[] = {
	{"both", DPSTS_BOTH_CONNECTED},
	{"dfp_d", DPSTS_DFP_D_CONNECTED},
	{"ufp_d", DPSTS_UFP_D_CONNECTED},
};

static int dpm_alt_mode_parse_svid_data(
	pd_port_t *pd_port, svdm_svid_data_t *svid_data)
{
	struct device_node *np, *ufp_np, *dfp_np;
	const char *connection;
	uint32_t ufp_d_pin_cap = 0;
	uint32_t dfp_d_pin_cap = 0;
	uint32_t signal = MODE_DP_V13;
	uint32_t receptacle = 1;
	uint32_t usb2 = 0;
	int i = 0;

	np = of_find_node_by_name(
		pd_port->tcpc_dev->dev.of_node, "displayport");
	if (np == NULL) {
		pd_port->svid_data_cnt = 0;
		pr_err("%s get displayport data fail\n", __func__);
		return -1;
	}

	pr_info("dp, svid\r\n");
	svid_data->svid = USB_SID_DISPLAYPORT;
	ufp_np = of_find_node_by_name(np, "ufp_d");
	dfp_np = of_find_node_by_name(np, "dfp_d");

	if (ufp_np) {
		pr_info("dp, ufp_np\n");
		for (i = 0; i < ARRAY_SIZE(supported_dp_pin_modes) - 1; i++) {
			if (of_property_read_bool(ufp_np,
				supported_dp_pin_modes[i].prop_name))
				ufp_d_pin_cap |=
					supported_dp_pin_modes[i].mode;
		}
	}

	if (dfp_np) {
		pr_info("dp, dfp_np\n");
		for (i = 0; i < ARRAY_SIZE(supported_dp_pin_modes); i++) {
			if (of_property_read_bool(dfp_np,
				supported_dp_pin_modes[i].prop_name))
				dfp_d_pin_cap |=
					supported_dp_pin_modes[i].mode;
		}
	}

	if (of_property_read_bool(np, "signal,dp_v13"))
		signal |= MODE_DP_V13;
	if (of_property_read_bool(np, "signal,dp_gen2"))
		signal |= MODE_DP_GEN2;
	if (of_property_read_bool(np, "usbr20_not_used"))
		usb2 = 1;
	if (of_property_read_bool(np, "typec,receptacle"))
		receptacle = 1;

	svid_data->local_mode.mode_cnt = 1;
	svid_data->local_mode.mode_vdo[0] = VDO_MODE_DP(
		ufp_d_pin_cap, dfp_d_pin_cap,
		usb2, receptacle, signal, (ufp_d_pin_cap ? MODE_DP_SNK : 0)
		| (dfp_d_pin_cap ? MODE_DP_SRC : 0));

	pd_port->dp_first_connected = DEFAULT_DP_FIRST_CONNECTED;
	pd_port->dp_second_connected = DEFAULT_DP_SECOND_CONNECTED;

	if (of_property_read_string(np, "1st_connection", &connection) == 0) {
		pr_info("dp, 1st_connection\n");
		for (i = 0; i < ARRAY_SIZE(dp_connect_mode); i++) {
			if (strcasecmp(connection,
				dp_connect_mode[i].conn_mode) == 0) {
				pd_port->dp_first_connected =
					dp_connect_mode[i].val;
				break;
			}
		}
	}

	if (of_property_read_string(np, "2nd_connection", &connection) == 0) {
		pr_info("dp, 2nd_connection\n");
		for (i = 0; i < ARRAY_SIZE(dp_connect_mode); i++) {
			if (strcasecmp(connection,
				dp_connect_mode[i].conn_mode) == 0) {
				pd_port->dp_second_connected =
					dp_connect_mode[i].val;
				break;
			}
		}
	}
	/* 2nd connection must not be BOTH */
	BUG_ON(pd_port->dp_second_connected == DPSTS_BOTH_CONNECTED);
	/* UFP or DFP can't both be invalid */
	BUG_ON(ufp_d_pin_cap == 0 && dfp_d_pin_cap == 0);
	if (pd_port->dp_first_connected == DPSTS_BOTH_CONNECTED) {
		BUG_ON(ufp_d_pin_cap == 0);
		BUG_ON(dfp_d_pin_cap == 0);
	}

	return 0;
}

#endif /* CONFIG_USB_PD_ALT_MODE*/

static void pd_core_parse_svid_data(pd_port_t *pd_port)
{
	int i = 0;

	/* TODO: dynamic allocate svid_data from DTS */

#ifdef CONFIG_USB_PD_ALT_MODE
	dpm_alt_mode_parse_svid_data(pd_port, &pd_port->svid_data[i++]);
#endif

	pd_port->svid_data_cnt = i;
}

static const struct {
	const char *prop_name;
	uint32_t val;
} supported_dpm_caps[] = {
	{"local_dr_power", DPM_CAP_LOCAL_DR_POWER},
	{"local_dr_data", DPM_CAP_LOCAL_DR_DATA},
	{"local_ext_power", DPM_CAP_LOCAL_EXT_POWER},
	{"local_usb_comm", DPM_CAP_LOCAL_USB_COMM},
	{"local_usb_suspend", DPM_CAP_LOCAL_USB_SUSPEND},
	{"local_high_cap", DPM_CAP_LOCAL_HIGH_CAP},
	{"local_give_back", DPM_CAP_LOCAL_GIVE_BACK},
	{"local_no_suspend", DPM_CAP_LOCAL_NO_SUSPEND},
	{"local_vconn_supply", DPM_CAP_LOCAL_VCONN_SUPPLY},

	{"attemp_discover_cable_dfp", DPM_CAP_ATTEMP_DISCOVER_CABLE_DFP},
	{"attemp_enter_dp_mode", DPM_CAP_ATTEMP_ENTER_DP_MODE},
	{"attemp_discover_cable", DPM_CAP_ATTEMP_DISCOVER_CABLE},
	{"attemp_discover_id", DPM_CAP_ATTEMP_DISCOVER_ID},

	{"pr_reject_as_source", DPM_CAP_PR_SWAP_REJECT_AS_SRC},
	{"pr_reject_as_sink", DPM_CAP_PR_SWAP_REJECT_AS_SNK},
	{"pr_check_gp_source", DPM_CAP_PR_SWAP_CHECK_GP_SRC},
	{"pr_check_gp_sink", DPM_CAP_PR_SWAP_CHECK_GP_SNK},

	{"dr_reject_as_dfp", DPM_CAP_DR_SWAP_REJECT_AS_DFP},
	{"dr_reject_as_ufp", DPM_CAP_DR_SWAP_REJECT_AS_UFP},

	{"snk_prefer_low_voltage", DPM_CAP_SNK_PREFER_LOW_VOLTAGE},
	{"snk_ignore_mismatch_current", DPM_CAP_SNK_IGNORE_MISMATCH_CURRENT},
};

static void pd_core_power_flags_init(pd_port_t *pd_port)
{
	uint32_t src_flag, snk_flag, val;
	struct device_node *np;
	int i;
	pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	pd_port_power_caps *src_cap = &pd_port->local_src_cap_default;

	np = of_find_node_by_name(pd_port->tcpc_dev->dev.of_node, "dpm_caps");

	for (i = 0; i < ARRAY_SIZE(supported_dpm_caps); i++) {
		if (of_property_read_bool(np,
			supported_dpm_caps[i].prop_name))
			pd_port->dpm_caps |=
				supported_dpm_caps[i].val;
			pr_info("dpm_caps: %s\n",
				supported_dpm_caps[i].prop_name);
	}

	if (of_property_read_u32(np, "pr_check", &val) == 0)
		pd_port->dpm_caps |= DPM_CAP_PR_CHECK_PROP(val);
	else
		pr_err("%s get pr_check data fail\n", __func__);

	if (of_property_read_u32(np, "dr_check", &val) == 0)
		pd_port->dpm_caps |= DPM_CAP_DR_CHECK_PROP(val);
	else
		pr_err("%s get dr_check data fail\n", __func__);

	pr_info("dpm_caps = 0x%08x\n", pd_port->dpm_caps);

	src_flag = 0;
	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER)
		src_flag |= PDO_FIXED_DUAL_ROLE;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_DATA)
		src_flag |= PDO_FIXED_DATA_SWAP;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_EXT_POWER)
		src_flag |= PDO_FIXED_EXTERNAL;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_USB_COMM)
		src_flag |= PDO_FIXED_COMM_CAP;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_USB_SUSPEND)
		src_flag |= PDO_FIXED_SUSPEND;

	snk_flag = src_flag;
	if (pd_port->dpm_caps & DPM_CAP_LOCAL_HIGH_CAP)
		snk_flag |= PDO_FIXED_HIGH_CAP;

	snk_cap->pdos[0] |= snk_flag;
	src_cap->pdos[0] |= src_flag;
}

int pd_core_init(struct tcpc_device *tcpc_dev)
{
	int ret;
	pd_port_t *pd_port = &tcpc_dev->pd_port;

	mutex_init(&pd_port->pd_lock);
	pd_port->tcpc_dev = tcpc_dev;

	pd_port->pe_pd_state = PE_IDLE;
	pd_port->pe_vdm_state = PE_IDLE;

	pd_port->pd_connect_state = PD_CONNECT_NONE;

	ret = pd_parse_pdata(pd_port);
	if (ret)
		return ret;

	pd_core_parse_svid_data(pd_port);
	pd_core_power_flags_init(pd_port);

	pd_dpm_core_init(pd_port);

	PE_INFO("pd_core_init\r\n");
	return 0;
}

void pd_extract_rdo_power(uint32_t rdo, uint32_t pdo,
			uint32_t *op_curr, uint32_t *max_curr)
{
	uint32_t op_power, max_power, vmin;


	switch (pdo & PDO_TYPE_MASK) {
	case PDO_TYPE_FIXED:
	case PDO_TYPE_VARIABLE:
		*op_curr = RDO_FIXED_VAR_EXTRACT_OP_CURR(rdo);
		*max_curr = RDO_FIXED_VAR_EXTRACT_MAX_CURR(rdo);
		break;

	case PDO_TYPE_BATTERY: /* TODO: check it later !! */
		vmin = PDO_BATT_EXTRACT_MIN_VOLT(pdo);
		op_power = RDO_BATT_EXTRACT_OP_POWER(rdo);
		max_power = RDO_BATT_EXTRACT_MAX_POWER(rdo);

		*op_curr = op_power / vmin;
		*max_curr = max_power / vmin;
		break;

	default:
		*op_curr = *max_curr = 0;
		break;
	}
}

uint32_t pd_reset_pdo_power(uint32_t pdo, uint32_t imax)
{
	uint32_t ioper;

	switch (pdo & PDO_TYPE_MASK) {
	case PDO_TYPE_FIXED:
		ioper = PDO_FIXED_EXTRACT_CURR(pdo);
		if (ioper > imax)
			return PDO_FIXED_RESET_CURR(pdo, imax);
		break;

	case PDO_TYPE_VARIABLE:
		ioper = PDO_VAR_EXTRACT_CURR(pdo);
		if (ioper > imax)
			return PDO_VAR_RESET_CURR(pdo, imax);
		break;

	case PDO_TYPE_BATTERY:
		/* TODO: check it later !! */
		PD_ERR("No Support\r\n");
		break;
	}
	return pdo;
}

void pd_extract_pdo_power(uint32_t pdo,
		uint32_t *vmin, uint32_t *vmax, uint32_t *ioper)
{
	uint32_t pwatt;

	switch (pdo & PDO_TYPE_MASK) {
	case PDO_TYPE_FIXED:
		*ioper = PDO_FIXED_EXTRACT_CURR(pdo);
		*vmin = *vmax = PDO_FIXED_EXTRACT_VOLT(pdo);
		break;

	case PDO_TYPE_VARIABLE:
		*ioper = PDO_VAR_EXTRACT_CURR(pdo);
		*vmin = PDO_VAR_EXTRACT_MIN_VOLT(pdo);
		*vmax = PDO_VAR_EXTRACT_MAX_VOLT(pdo);
		break;

	case PDO_TYPE_BATTERY:	/* TODO: check it later !! */
		*vmin = PDO_BATT_EXTRACT_MIN_VOLT(pdo);
		*vmax = PDO_BATT_EXTRACT_MAX_VOLT(pdo);
		pwatt = PDO_BATT_EXTRACT_OP_POWER(pdo);
		*ioper = pwatt / *vmin;
		break;

	default:
		*vmin = *vmax = *ioper = 0;
	}
}


uint32_t pd_extract_cable_curr(uint32_t vdo)
{
	uint32_t cable_curr;

	switch (PD_VDO_CABLE_CURR(vdo)) {
	case CABLE_CURR_1A5:
		cable_curr = 1500;
		break;
	case CABLE_CURR_5A:
		cable_curr = 5000;
		break;
	default:
	case CABLE_CURR_3A:
		cable_curr = 3000;
		break;
	}

	return cable_curr;
}

void pd_reset_svid_data(pd_port_t *pd_port)
{
	uint8_t i;
	svdm_svid_data_t *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		svid_data->exist = false;
		svid_data->remote_mode.mode_cnt = 0;
		svid_data->active_mode = 0;
	}
}

int pd_reset_protocol_layer(pd_port_t *pd_port)
{
	int i = 0;

	pd_notify_pe_reset_protocol(pd_port);

	pd_port->explicit_contract = 0;
	pd_port->local_selected_cap = 0;
	pd_port->remote_selected_cap = 0;
	pd_port->during_swap = 0;
	pd_port->dpm_ack_immediately = 0;

	for (i = 0; i < PD_SOP_NR; i++) {
		pd_port->msg_id_tx[i] = 0;
		pd_port->msg_id_rx[i] = 0;
		pd_port->msg_id_rx_init[i] = false;
	}

	return 0;
}

int pd_set_rx_enable(pd_port_t *pd_port, uint8_t enable)
{
	return tcpci_set_rx_enable(pd_port->tcpc_dev, enable);
}

int pd_enable_vbus_valid_detection(pd_port_t *pd_port, bool wait_valid)
{
	PE_DBG("WaitVBUS=%d\r\n", wait_valid);
	pd_notify_pe_wait_vbus_once(pd_port,
		wait_valid ? PD_WAIT_VBUS_VALID_ONCE :
					PD_WAIT_VBUS_INVALID_ONCE);
	return 0;
}

int pd_enable_vbus_safe0v_detection(pd_port_t *pd_port)
{
	PE_DBG("WaitVSafe0V\r\n");
	pd_notify_pe_wait_vbus_once(pd_port, PD_WAIT_VBUS_SAFE0V_ONCE);
	return 0;
}

int pd_enable_vbus_stable_detection(pd_port_t *pd_port)
{
	PE_DBG("WaitVStable\r\n");
	pd_notify_pe_wait_vbus_once(pd_port, PD_WAIT_VBUS_STABLE_ONCE);
	return 0;
}

int pd_set_data_role(pd_port_t *pd_port, uint8_t dr)
{
	pd_port->data_role = dr;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	/* dual role usb--> 0:ufp, 1:dfp */
	pd_port->tcpc_dev->dual_role_mode = pd_port->data_role;
	/* dual role usb --> 0: Device, 1: Host */
	pd_port->tcpc_dev->dual_role_dr = !(pd_port->data_role);
	dual_role_instance_changed(pd_port->tcpc_dev->dr_usb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

	tcpci_notify_role_swap(pd_port->tcpc_dev, TCP_NOTIFY_DR_SWAP, dr);
	return tcpci_set_msg_header(pd_port->tcpc_dev,
			pd_port->power_role, pd_port->data_role);
}

int pd_set_power_role(pd_port_t *pd_port, uint8_t pr)
{
	int ret;

	pd_port->power_role = pr;
	ret = tcpci_set_msg_header(pd_port->tcpc_dev,
			pd_port->power_role, pd_port->data_role);
	if (ret)
		return ret;

	pd_notify_pe_pr_changed(pd_port);

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	/* 0:sink, 1: source */
	pd_port->tcpc_dev->dual_role_pr = !(pd_port->power_role);
	dual_role_instance_changed(pd_port->tcpc_dev->dr_usb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

	tcpci_notify_role_swap(pd_port->tcpc_dev, TCP_NOTIFY_PR_SWAP, pr);
	return ret;
}

int pd_init_role(pd_port_t *pd_port, uint8_t pr, uint8_t dr, bool vr)
{
	pd_port->power_role = pr;
	pd_port->data_role = dr;
	pd_port->vconn_source = vr;

	return tcpci_set_msg_header(pd_port->tcpc_dev,
			pd_port->power_role, pd_port->data_role);
}

int pd_set_cc_res(pd_port_t *pd_port, int pull)
{
	return tcpci_set_cc(pd_port->tcpc_dev, pull);
}

int pd_set_vconn(pd_port_t *pd_port, int enable)
{
	pd_port->vconn_source = enable;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	pd_port->tcpc_dev->dual_role_vconn = pd_port->vconn_source;
	dual_role_instance_changed(pd_port->tcpc_dev->dr_usb);
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

	tcpci_notify_role_swap(pd_port->tcpc_dev,
				TCP_NOTIFY_VCONN_SWAP, enable);
	return tcpci_set_vconn(pd_port->tcpc_dev, enable);
}

static inline int pd_reset_modal_operation(pd_port_t *pd_port)
{
	uint8_t i;
	svdm_svid_data_t *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];

		if (svid_data->active_mode) {
			svid_data->active_mode = 0;
			tcpci_exit_mode(pd_port->tcpc_dev, svid_data->svid);
		}
	}

	pd_port->modal_operation = false;
	return 0;
}

int pd_reset_local_hw(pd_port_t *pd_port)
{
	pd_notify_pe_transit_to_default(pd_port);
	pd_unlock_msg_output(pd_port);

	pd_reset_pe_timer(pd_port);
	pd_set_rx_enable(pd_port, PD_RX_CAP_PE_HARDRESET);

	pd_port->explicit_contract = false;
	pd_port->pd_connected  = false;
	pd_port->pe_ready = false;
	pd_port->dpm_ack_immediately = false;

	pd_reset_modal_operation(pd_port);

	pd_set_vconn(pd_port, false);

	if (pd_port->power_role == PD_ROLE_SINK) {
		pd_port->state_machine = PE_STATE_MACHINE_SINK;
		pd_set_data_role(pd_port, PD_ROLE_UFP);
	} else {
		pd_port->state_machine = PE_STATE_MACHINE_SOURCE;
		pd_set_data_role(pd_port, PD_ROLE_DFP);
	}

	PE_DBG("reset_local_hw\r\n");

	return 0;
}

int pd_enable_bist_test_mode(pd_port_t *pd_port, bool en)
{
	PE_DBG("bist_test_mode=%d\r\n", en);
	return tcpci_set_bist_test_mode(pd_port->tcpc_dev, en);
}

/* ---- Send PD Message ----*/

static int pd_send_message(pd_port_t *pd_port, uint8_t sop_type,
			uint8_t msg, uint16_t count, const uint32_t *data)
{
	int ret;
	uint16_t msg_hdr;
	uint8_t type = PD_TX_STATE_WAIT_CRC_PD;
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	if (tcpc_dev->typec_attach_old == 0) {
		PE_DBG("[SendMsg] Unattached\r\n");
		return 0;
	}

	if (tcpc_dev->pd_hard_reset_event_pending) {
		PE_DBG("[SendMsg] HardReset Pending");
		return 0;
	}

	if (sop_type == TCPC_TX_SOP) {
		msg_hdr = PD_HEADER_SOP(msg, pd_port->power_role,
			pd_port->data_role,
			pd_port->msg_id_tx[sop_type], count);
	} else {
		msg_hdr = PD_HEADER_SOP_PRIME(msg,
			0, pd_port->msg_id_tx[sop_type], count);
	}

	if ((count > 0) && (msg == PD_DATA_VENDOR_DEF))
		type = PD_TX_STATE_WAIT_CRC_VDM;

	pd_port->msg_id_tx[sop_type] = (pd_port->msg_id_tx[sop_type] + 1) % 8;

	pd_notify_pe_transmit_msg(pd_port, type);
	ret = tcpci_transmit(pd_port->tcpc_dev, sop_type, msg_hdr, data);
	if (ret < 0)
		PD_ERR("[SendMsg] Failed, %d\r\n", ret);

	ret = pd_wait_tx_finished_event(pd_port);
	return ret;
}

#ifdef CONFIG_RT7207_ADAPTER
int rt7207_vdm_send_message(pd_port_t *pd_port, uint8_t sop_type,
			uint8_t msg, uint16_t count, const uint32_t *data)
{
	return pd_send_message(pd_port, sop_type, msg, count, data);
}
#endif /* CONFIG_RT7207_ADAPTER */

int pd_send_ctrl_msg(pd_port_t *pd_port, uint8_t sop_type, uint8_t msg)
{
	return pd_send_message(pd_port, sop_type, msg, 0, NULL);
}

int pd_send_data_msg(pd_port_t *pd_port,
		uint8_t sop_type, uint8_t msg, uint8_t cnt, uint32_t *payload)
{
	return pd_send_message(pd_port, sop_type, msg, cnt, payload);
}

int pd_send_hard_reset(pd_port_t *pd_port)
{
	int ret;

	PE_DBG("Send HARD Reset++\r\n");

	pd_notify_pe_send_hard_reset_start(pd_port);
	ret = tcpci_transmit(pd_port->tcpc_dev, TCPC_TX_HARD_RESET, 0, NULL);
	if (ret)
		return ret;
	ret = pd_wait_tx_finished_event(pd_port);

	pd_port->hard_reset_counter++;
	pd_notify_pe_send_hard_reset_done(pd_port);

	PE_DBG("Send HARD Reset--\r\n");
	return 0;
}

int pd_send_bist_mode2(pd_port_t *pd_port)
{
	int ret = 0;

#ifdef CONFIG_USB_PD_TRANSMIT_BIST2
	TCPC_DBG("BIST_MODE_2\r\n");
	ret = tcpci_transmit(
		pd_port->tcpc_dev, TCPC_TX_BIST_MODE_2, 0, NULL);
#else
	ret = tcpci_set_bist_carrier_mode(
		pd_port->tcpc_dev, 1 << 2);
#endif

	return ret;
}

int pd_disable_bist_mode2(pd_port_t *pd_port)
{
	int ret = 0;

#ifndef CONFIG_USB_PD_TRANSMIT_BIST2
	ret = tcpci_set_bist_carrier_mode(
		pd_port->tcpc_dev, 0);
#endif

	return 0;
}

/* ---- Send / Reply VDM Command ----*/

int pd_send_svdm_request(pd_port_t *pd_port,
		uint8_t sop_type, uint16_t svid, uint8_t vdm_cmd,
		uint8_t obj_pos, uint8_t cnt, uint32_t *data_obj)
{
	int ret;
	uint32_t payload[VDO_MAX_SIZE];

	BUG_ON(cnt >= (VDO_MAX_SIZE-1));

	payload[0] = VDO_S(svid, CMDT_INIT, vdm_cmd, obj_pos);
	memcpy(&payload[1], data_obj, sizeof(uint32_t) * cnt);

	ret = pd_send_data_msg(
			pd_port, sop_type, PD_DATA_VENDOR_DEF, 1+cnt, payload);

	if (ret == 0 && (vdm_cmd != CMD_ATTENTION))
		pd_enable_timer(pd_port, PD_TIMER_VDM_RESPONSE);

	return ret;
}

int pd_reply_svdm_request(pd_port_t *pd_port, pd_event_t *pd_event,
				uint8_t reply, uint8_t cnt, uint32_t *data_obj)
{
	uint32_t vdo;
	uint32_t payload[VDO_MAX_SIZE];

	BUG_ON(cnt >= (VDO_MAX_SIZE-1));
	BUG_ON(pd_event->pd_msg == NULL);

	vdo = pd_event->pd_msg->payload[0];
	payload[0] = VDO_S(
		PD_VDO_VID(vdo), reply, PD_VDO_CMD(vdo), PD_VDO_OPOS(vdo));

	if (cnt > 0) {
		BUG_ON(data_obj == NULL);
		memcpy(&payload[1], data_obj, sizeof(uint32_t) * cnt);
	}

	return pd_send_data_msg(pd_port,
			TCPC_TX_SOP, PD_DATA_VENDOR_DEF, 1+cnt, payload);
}

void pd_lock_msg_output(pd_port_t *pd_port)
{
	if (pd_port->msg_output_lock)
		return;
	pd_port->msg_output_lock = true;

	pd_dbg_info_lock();
}

void pd_unlock_msg_output(pd_port_t *pd_port)
{
	if (!pd_port->msg_output_lock)
		return;
	pd_port->msg_output_lock = false;

	pd_dbg_info_unlock();
}

int pd_update_connect_state(pd_port_t *pd_port, uint8_t state)
{
	if (pd_port->pd_connect_state == state)
		return 0;

	switch (state) {
	case PD_CONNECT_TYPEC_ONLY:
		if (pd_port->power_role == PD_ROLE_SOURCE)
			state = PD_CONNECT_TYPEC_ONLY_SRC;
		else {
			switch (pd_port->tcpc_dev->typec_remote_rp_level) {
			case TYPEC_CC_VOLT_SNK_DFT:
				state = PD_CONNECT_TYPEC_ONLY_SNK_DFT;
				break;

			case TYPEC_CC_VOLT_SNK_1_5:
			case TYPEC_CC_VOLT_SNK_3_0:
				state = PD_CONNECT_TYPEC_ONLY_SNK;
				break;
			}
		}
		break;

	case PD_CONNECT_PE_READY:
		state = pd_port->power_role == PD_ROLE_SOURCE ?
			PD_CONNECT_PE_READY_SRC : PD_CONNECT_PE_READY_SNK;
		break;

	case PD_CONNECT_NONE:
		break;
	}

	pd_port->pd_connect_state = state;
	return tcpci_notify_pd_state(pd_port->tcpc_dev, state);
}
