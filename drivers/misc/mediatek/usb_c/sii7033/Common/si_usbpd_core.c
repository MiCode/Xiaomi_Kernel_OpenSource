/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/

#include "si_usbpd_core.h"
#include "si_usbpd_main.h"

/*#include "si_platform_i2c.h"*/
#define TRANSMIT_DATA 1

void *context;
uint32_t temp_data;

static bool sii_pd_vdm_handler(struct sii_usbpd_protocol *pd, struct pd_inq *req);

void sii_rcv_usbpd_data(struct sii70xx_drv_context *drv_context)
{
	struct sii_usbpd_protocol *pUsbpd_prtlyr = (struct sii_usbpd_protocol *)
	    drv_context->pUsbpd_prot;
	uint8_t count = 0;

	count = (sii_platform_rd_reg8(REG_ADDR__PDRXBC)
		 & BIT_MSK__PDRXBC__RO_PDRXSIZE_B4_B0);

	sii_platform_block_read8(REG_ADDR__PDRXBUF0, pUsbpd_prtlyr->rx_msg->recv_msg, count);
	sii_platform_set_bit8(REG_ADDR__PDCTR0, BIT_MSK__PDCTR0__RI_PRL_RX_MSG_READ_DONE_WP);


	SII_PLATFORM_DEBUG_ASSERT(pUsbpd_prtlyr);
	SII_PLATFORM_DEBUG_ASSERT(pUsbpd_prtlyr->in_command);
	SII_PLATFORM_DEBUG_ASSERT(pUsbpd_prtlyr->in_command->msgdata);

	usbpd_process_msg(pUsbpd_prtlyr, pUsbpd_prtlyr->rx_msg->recv_msg, count);
}

static bool sii_submit_pdmsg(struct sii_usbpd_protocol *pd,
			     uint16_t pd_header, uint32_t *buf, uint8_t length)
{
	pd->cb_params->send_data[OFFSET_1] = pd_header & 0xFF;
	pd->cb_params->send_data[OFFSET_2] = (pd_header & 0xFF00) >> 8;

	if (!buf || length == 0) {
		pr_debug("Buffer is not valid for control commands...\n");
		length = sizeof(pd_header);
	} else {
		length = length * sizeof(*buf) + sizeof(pd_header);
		memcpy((pd->cb_params->send_data + sizeof(pd_header)), (uint8_t *) buf, length);
	}

	pd->cb_params->count = length;

	if (pd->evnt_notify_fn)
		pd->evnt_notify_fn(pd, pd->cb_params);
	memset(pd->cb_params, 0, sizeof(struct pd_cb_params));
	pr_debug("\nSubmit done\n");
	return true;
}

bool usbpd_xmit_ctrl_msg(struct sii_usbpd_protocol *pd, enum ctrl_msg type)
{
	uint16_t pd_header;
	bool res;

	/*pr_debug("%s: Enter...\n", __func__); */

	if (!pd) {
		pr_info("%s: Error\n", __func__);
		return -ENOMEM;
	}

	pd_header = SII_USBPD_HEADER(type, pd->pwr_role, pd->data_role, 0, 0);

	/*pr_info("%s: pd_header %X\n", __func__, pd_header); */

	res = sii_submit_pdmsg(pd, pd_header, NULL, 0);
	if (!res) {
		pr_info("Intf Error......\n");
		return false;
	}
	return true;
}

bool usbipd_send_soft_reset(struct sii70xx_drv_context *drv_context, enum ctrl_msg type)
{
	struct sii_usbpd_protocol *pUsbpd_prtlyr = (struct sii_usbpd_protocol *)
	    drv_context->pUsbpd_prot;
	usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, type);

	return true;
}

bool sii_usbpd_xmit_data_msg(struct sii_usbpd_protocol *pd,
			     enum data_msg type, uint32_t *src_pdo, uint8_t count)
{
	uint16_t pd_header;
	bool b_result;

	if (!pd || !src_pdo) {
		pr_info("%s: Error\n", __func__);
		return false;
	}

	pd_header = SII_USBPD_HEADER(type, pd->pwr_role, pd->data_role, 0, count);

	b_result = sii_submit_pdmsg(pd, pd_header, src_pdo, count);

	if (!b_result)
		return false;

	return b_result;
}

void parse_bist(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	pr_debug("Bist => %X\n", ((req->msgdata[0])));

	if ((req->msgdata[0] >> 28) == 0x5) {
		set_bit(BIST_RCVD, &pd->cb_params->sm_cmd_inputs);
		sii_platform_wr_reg8(REG_ADDR__PDCTR18, 0x15);
		sii_platform_wr_reg8(REG_ADDR__PDCTR1, 0x01);
		bist_timer_start(pd);
	}
}

bool sii_process_data_msg(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	if (!req) {
		pr_debug("%s: Enter\n", __func__);
		return false;
	}

	pr_info("Received Command : ");

	memset(pd->cb_params, 0, sizeof(struct pd_cb_params));

	pd->cb_params->svdm_sm_inputs = 0;
	pd->cb_params->sm_cmd_inputs = 0;
	switch (MSG_TYPE(req->command)) {
	case SRCCAP:
		pr_info("SRC CAP RCVD.\n");
		set_bit(SRC_CAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case REQ:
		pr_info("REQ RCVD.\n");
		set_bit(REQ_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case BIST:
		pr_info("BIST RCVD.\n");
		set_bit(BIST_RCVD, &pd->cb_params->sm_cmd_inputs);
		parse_bist(pd, req);
		return true;
	case SNKCAP:
		pr_info("SNK CAP RCVD.\n");
		set_bit(SNK_CAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case VDM:
		pr_info("VDM CAP RCVD.\n");
		sii_pd_vdm_handler(pd, req);
		break;
	default:
		pr_info("Incorrect data message.\n");
		return false;
	}

	if (pd->cb_params) {
		pd->cb_params->data = (uint8_t *) req->msgdata;
		pd->cb_params->count = req->count;
		sii_update_inf_params(pd, pd->cb_params);
	} else {
		pr_debug("%s: Error...\n", __func__);
		return false;
	}

	return true;
}

bool sii_process_ctrl_cmds(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	if (!req) {
		pr_debug("%s: Error\n", __func__);
		return false;
	}

	pr_info("Received Command : ");
	memset(pd->cb_params, 0, sizeof(struct pd_cb_params));
	switch (MSG_TYPE(req->command)) {
	case CTRL_MSG__GOODCRC:
		pr_info("GOODCRC\n");
		set_bit(GOOD_CRC_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__GO_TO_MIN:
		pr_info("GO_TO_MIN\n");
		set_bit(GOTOMIN_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__ACCEPT:
		pr_info("ACCEPT\n");
		set_bit(ACCEPT_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__REJECT:
		pr_info("REJECT\n");
		set_bit(REJECT_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__PING:
		pr_info("PING\n");
		set_bit(PING_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__PS_RDY:
		pr_info("PS_RDY\n");
		set_bit(PS_RDY_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__GET_SRC_CAP:
		pr_info("GET_SOURCE_CAP\n");
		set_bit(GET_SOURCE_CAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__GET_SINK_CAP:
		pr_info("GET_SINK_CAP\n");
		set_bit(GET_SINK_CAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__DR_SWAP:
		pr_info("DR_SWAP\n");
		set_bit(DR_SWAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__PR_SWAP:
		pr_info("PR_SWAP\n");
		set_bit(PR_SWAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__VCONN_SWAP:
		pr_info("VCONN_SWAP\n");
		set_bit(VCONN_SWAP_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__WAIT:
		pr_info("WAIT\n");
		set_bit(WAIT_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	case CTRL_MSG__SOFT_RESET:
		pr_info("SOFT_RESET\n");
		set_bit(SOFT_RESET_RCVD, &pd->cb_params->sm_cmd_inputs);
		break;
	default:
		pr_info("Unhandled ctrl message type\n");
		return false;
	}

	if (pd->cb_params) {
		pd->cb_params->count = req->count;
		pd->cb_params->data = (uint8_t *) req->msgdata;
		sii_update_inf_params(pd, pd->cb_params);
	} else {
		pr_debug("Data got corrupted...\n");
		return false;
	}
	return true;
}

static void parse_vdm_identity(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	bool is_usb_host, is_usb_device, is_modal_op_supp, result = false;
	uint16_t usb_vid, svid0;

	pr_debug("parsing in = %X\n", SII_USBPD_VDO_SVID(req->msgdata[0]));


	if (SII_USBPD_VDO_SVID(req->msgdata[0]) == 0xFF00)
		pr_debug("\n");
	svid0 = SII_USBPD_VDO_SVID(req->msgdata[0]);

	is_usb_host = PD_HOST(req->msgdata[VDO_I(IDH)]);
	is_usb_device = PD_DEVICE(req->msgdata[VDO_I(IDH)]);
	is_modal_op_supp = PD_MODAL(req->msgdata[VDO_I(IDH)]);
	usb_vid = PD_IDH_VID(req->msgdata[VDO_I(IDH)]);

	pr_info("usb host support : %s\n", is_usb_host ? "Yes" : "No");
	pr_info("usb device support : %s\n", is_usb_device ? "Yes" : "No");
	pr_info("Modal operation support : %s\n", is_modal_op_supp ? "Yes" : "No");
	pr_info("usb vid : %X\n", usb_vid);
	pr_info("usb svid0 : %X\n", svid0);

	switch (PD_IDH_PTYPE(req->msgdata[VDO_I(IDH)])) {
	case IDH_PTYPE_UNDEF:
		pr_info("disc identity : UNDEFINED\n");
		/*clear_bit(SVDM_DISC_IDEN_RCVD, &pd->cb_params->
		   svdm_sm_inputs); */
		if (svid0 != 0xff00) {
			result = usbpd_svdm_init_resp_nak(pd,
							  CMD_DISCOVER_IDENT, svid0, false, NULL);
			clear_bit(SVDM_DISC_IDEN_RCVD, &pd->cb_params->svdm_sm_inputs);
			pr_info("NACK SENT\n");
		}
		if (result)
			pr_debug("undefined done\n");
		else
			pr_debug("undefined not done\n");
		break;

	case IDH_PTYPE_HUB:
		pr_info("disc identity : IDH_PTYPE_HUB\n");
		pr_info("usb product id => %X\n ", ((req->msgdata[VDO_I(PRODUCT)] >> 16) & 0xFFFF));
		pr_info("usb bcd device id => %X\n ", ((req->msgdata[VDO_I(PRODUCT)]) & 0xFFFF));
		break;

	case IDH_PTYPE_PERIPH:
		pr_info("disc identity : IDH_PTYPE_PERIPH\n");
		pr_info("usb product id => %X\n ", ((req->msgdata[VDO_I(PRODUCT)] >> 16) & 0xFFFF));
		pr_info("usb bcd device id => %X\n ", ((req->msgdata[VDO_I(PRODUCT)]) & 0xFFFF));
		break;

	case IDH_PTYPE_PCABLE:
		pr_info("disc identity : IDH_PTYPE_PCABLE\n");
		pr_info("HW version => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 28) & 0xF));
		pr_info("FW version => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 24) & 0xF));
		pr_info("CABLE TYPE => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 18) & 0x3));
		pr_info("CABLE TYPE => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 17) & 0x1) ? "receptacle" : "plug");
		pr_info("Cable Latency => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 13) & 0xF));
		pr_info("Cable Termination type => %X\n",
			((req->msgdata[VDO_I(CABLE)] >> 11) & 0x3));
		pr_info("SSTX1 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 10) ? "configurable" : "fixed"));
		pr_info("SSTX2 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 9) ? "configurable" : "fixed"));
		pr_info("SSRX1 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 8) ? "configurable" : "fixed"));
		pr_info("SSRX2 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 7) ? "configurable" : "fixed"));
		pr_info("Vbus current handling => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 5)));
		pr_info("vbus through cable => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 4)) ? "Yes" : "No");
		pr_info("sop-2 controller present => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 3)) ? "Yes" : "No");
		pr_info("super speed signalling => %X\n", (req->msgdata[VDO_I(CABLE)] >> 0) & 0x7);
		break;

	case IDH_PTYPE_ACABLE:
		pr_info("disc identity : IDH_PTYPE_ACABLE\n");
		pr_info("HW version => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 28) & 0xF));
		pr_info("FW version => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 24) & 0xF));
		pr_info("CABLE TYPE => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 18) & 0x3));
		pr_info("CABLE TYPE => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 17) & 0x1) ? "receptacle" : "plug");
		pr_info("Cable Latency => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 13) & 0xF));
		pr_info("Cable Termination type => %X\n",
			((req->msgdata[VDO_I(CABLE)] >> 11) & 0x3));
		pr_info("SSTX1 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 10) ? "configurable" : "fixed"));
		pr_info("SSTX2 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 9) ? "configurable" : "fixed"));
		pr_info("SSRX1 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 8) ? "configurable" : "fixed"));
		pr_info("SSRX2 Directionality => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 7) ? "configurable" : "fixed"));
		pr_info("Vbus current handling => %X\n", ((req->msgdata[VDO_I(CABLE)] >> 5)));
		pr_info("vbus through cable => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 4)) ? "Yes" : "No");
		pr_info("sop-2 controller present => %s\n",
			((req->msgdata[VDO_I(CABLE)] >> 3)) ? "Yes" : "No");
		pr_info("super speed signalling => %X\n",
			((req->msgdata[VDO_I(CABLE)] >> 0) & 0x7));
		break;

	case IDH_PTYPE_AMA:
		pr_info("Disc Identity: IDH_PTYPE_AMA\n");
		pr_info("usb product id => %X\n ", ((req->msgdata[VDO_I(PRODUCT)] >> 16) & 0xFFFF));
		pr_info("usb bcd device id => %X\n ", ((req->msgdata[VDO_I(PRODUCT)]) & 0xFFFF));
		pd->pd_hw_ver = PD_VDO_AMA_HW_VER(req->msgdata[VDO_I(AMA)]);
		pr_info("HW Version => %X\n", pd->pd_hw_ver);
		pd->pd_fw_ver = PD_VDO_AMA_FW_VER(req->msgdata[VDO_I(AMA)]);
		pr_info("FW Version => %X\n", pd->pd_fw_ver);
		pd->ss_tx1_func = PD_VDO_AMA_SS_TX1(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->ss_tx1_func => %X\n", pd->ss_tx1_func);
		pd->ss_tx2_func = PD_VDO_AMA_SS_TX2(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->ss_tx2_func => %X\n", pd->ss_tx2_func);
		pd->ss_rx1_func = PD_VDO_AMA_SS_RX1(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->ss_rx1_func => %X\n", pd->ss_rx1_func);
		pd->ss_rx2_func = PD_VDO_AMA_SS_RX2(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->ss_rx2_func => %X\n", pd->ss_rx2_func);
		pd->vconn = PD_VDO_AMA_VCONN_PWR(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->vconn => %X\n", pd->vconn);
		pd->vconn_req = PD_VDO_AMA_VCONN_REQ(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->vconn_req => %X\n", pd->vconn_req);
		pd->vbus_req = PD_VDO_AMA_VBUS_REQ(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->vbus_req => %X\n", pd->vbus_req);
		pd->usb_ss_supp = PD_VDO_AMA_SS_SUPP(req->msgdata[VDO_I(AMA)]);
		pr_info("pd->usb_ss_supp => %X\n", pd->usb_ss_supp);
		break;

	default:
		break;
	};
}

static void parse_svid(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	int i;
	bool result = false;
	uint32_t *ptr = ((req->msgdata) + 0);
	uint16_t svid0, svid1;

	svid0 = PD_VDO_SVID_SVID0(*ptr);
	for (i = 0; i < (MSG_CNT(req->command) - 1); i++) {
		pr_debug("pasre svid in\n");
		if (i == SVID_DISCOVERY_MAX) {
			pr_debug("ERR:SVIDCNT\n");
			break;
		}

		svid0 = PD_VDO_SVID_SVID0(*ptr);
		if (!svid0) {
			pr_info("svid0 error\n");
			break;
		}
		if (svid0 == 0xFF01) {
			pr_info("DISPLAY PORT DETECTION\n");
			set_bit(DISPLAY_PORT_RCVD, &pd->cb_params->svdm_sm_inputs);
			break;
		}
		pr_info("svid0 => %X\n", svid0);

		svid1 = PD_VDO_SVID_SVID1(*ptr);
		if (!svid1) {
			pr_info("svid0 error\n");
			break;
		}
		pr_info("svid1 => %X\n", svid0);
		ptr++;
	}
	if (svid0 != 0xFF00) {
		result = usbpd_svdm_init_resp_nak(pd, CMD_DISCOVER_SVID, svid0, false, NULL);
		pr_info("NACK SENT\n");
	}
	if (result)
		pr_debug("undefined done\n");
	else
		pr_debug("undefined not done\n");

	if (i && ((i % 12) == 0))
		pr_debug("ERR:SVID+12\n");
}

static void parse_modes(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	int i, j = 0;
	uint32_t *ptr = ((req->msgdata) + 0);
	uint16_t svid0, svid1;
	bool result = false;

	svid0 = PD_VDO_SVID_SVID0(*ptr);

	for (i = 0, j = 0; i < (MSG_CNT(req->command) - 1) && j < SVID_DISCOVERY_MAX; i++, j++) {
		if (i == SVID_DISCOVERY_MAX) {
			pr_debug("ERR:SVIDCNT\n");
			break;
		}

		svid0 = PD_VDO_SVID_SVID0(*ptr);
		if (!svid0) {
			pr_info("svid0 error\n");
			break;
		}

		pd->pd_modes[j] = svid0;

		pr_info("mode0 => %X\n", svid0);

		svid1 = PD_VDO_SVID_SVID1(*ptr);
		if (!svid1) {
			pr_info("svid0 error\n");
			break;
		}
		pr_info("mode1 => %X\n", svid1);
		j = j + 1;
		pd->pd_modes[j] = svid1;
		ptr++;
	}

	if (svid0 != 0xFF02) {
		result = usbpd_svdm_init_resp_nak(pd, CMD_DISCOVER_MODES, svid0, false, NULL);
		pr_info("NACK SENT\n");
	}
	if (result)
		pr_debug("undefined done\n");
	else
		pr_debug("undefined not done\n");

	if (i && ((i % 12) == 0))
		pr_debug("ERR:SVID+12\n");
}

static void init_resp_ack(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	/*pr_info("req->msgdata[0] = %x, req->msgdata[1] = %x\n",
	   req->msgdata[0], req->msgdata[1]); */
	switch (SII_USBPD_VDO_CMD(req->msgdata[VDO_I(HDR)])) {
	case DISCOVER_IDENTITY:
		pr_info("SVDM_CMD_DISCOVER_IDENTITY ack\n");
		set_bit(SVDM_DISC_IDEN_ACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		/*parse_vdm_identity(pd, req); */
		break;
	case DISCOVER_SVID:
		pr_info("SVDM_CMD_DISCOVER_SVID ack\n");
		set_bit(DISCOVER_SVID_ACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_svid(pd, req);
		break;
	case DISCOVER_MODE:
		pr_info("SVDM_CMD_DISCOVER_MODE ack\n");
		set_bit(DISCOVER_MODE_ACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_modes(pd, req);
		break;
	case ENTER_MODE:
		pr_info("SVDM_CMD_ENTER_MODE ack\n");
		set_bit(ENTER_MODE_ACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case EXIT_MODE:
		pr_info("SVDM_CMD_EXIT_MODE ack\n");
		set_bit(EXIT_MODE_ACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case ATTENTION:
		pr_info("SVDM_CMD_ATTENTION ack\n");
		set_bit(VDM_ATTENTION_ACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	default:
		pr_info("init_resp_ack failed..\n");
		break;
	}
}

static void init_resp_nack(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	switch (SII_USBPD_VDO_CMD(req->msgdata[0])) {
	case DISCOVER_IDENTITY:
		pr_debug("SVDM_CMD_DISCOVER_IDENTITY nack\n");
		set_bit(SVDM_DISC_IDEN_NACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case DISCOVER_SVID:
		pr_debug("SVDM_CMD_DISCOVER_SVID nack\n");
		set_bit(DISCOVER_SVID_NACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case DISCOVER_MODE:
		pr_debug("SVDM_CMD_DISCOVER_MODE nack\n");
		set_bit(DISCOVER_MODE_NACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case ENTER_MODE:
		pr_debug("SVDM_CMD_ENTER_MODE nack\n");
		set_bit(ENTER_MODE_NACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case EXIT_MODE:
		pr_debug("SVDM_CMD_EXIT_MODE nack\n");
		set_bit(EXIT_MODE_NACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case ATTENTION:
		pr_debug("SVDM_CMD_ATTENTION nack\n");
		set_bit(VDM_ATTENTION_NACK_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	default:
		pr_debug("init_resp_nack failed.\n");
		break;
	}
}

static void init_busy(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	switch (SII_USBPD_VDO_CMD(req->msgdata[0])) {
	case DISCOVER_IDENTITY:
		pr_debug("SVDM_CMD_DISCOVER_IDENTITY busy\n");
		set_bit(SVDM_DISC_IDEN_BUSY_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case DISCOVER_SVID:
		pr_debug("SVDM_CMD_DISCOVER_SVID busy\n");
		set_bit(DISCOVER_SVID_BUSY_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case DISCOVER_MODE:
		pr_debug("SVDM_CMD_DISCOVER_MODE busy\n");
		set_bit(DISCOVER_MODE_BUSY_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case ENTER_MODE:
		pr_debug("SVDM_CMD_ENTER_MODE busy\n");
		set_bit(ENTER_MODE_BUSY_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case EXIT_MODE:
		pr_debug("SVDM_CMD_EXIT_MODE busy\n");
		set_bit(EXIT_MODE_BUSY_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	case ATTENTION:
		pr_debug("SVDM_CMD_ATTENTION busy\n");
		set_bit(VDM_ATTENTION_BUSY_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	default:
		pr_debug("init_busy failed busy\n");
		break;
	}
}

static void parse_enter_mode(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	uint16_t svids;
	uint8_t opos;
	bool result = false;

	pr_debug("Parsing Enter mode data:\n");

	pr_debug("Data : %X\n", req->msgdata[0]);

	svids = SII_USBPD_VDO_SVID(req->msgdata[0]);
	svids &= 0xFFFF;
	pr_debug("svid ==> %X\n", svids);
	opos = SII_USBPD_VDO_OBJPOSITION(req->msgdata[VDO_I(IDH)]);
	pr_debug("opos ==> %X\n", opos);

	/*if (svids == pd->pd_modes[opos]) { */
	if (svids == 0xFF02) {
		pr_debug("%X: MHL Mode...\n", pd->pd_modes[opos]);
		set_bit(MHL_ESTABLISHD, &pd->cb_params->svdm_sm_inputs);
	} else {
		pr_debug("%X: Bill Board Device Class ....\n", svids);
		set_bit(MHL_NOT_ESTABLISHD, &pd->cb_params->svdm_sm_inputs);
	}
	if (svids != 0xFF02) {
		result = usbpd_svdm_init_resp_nak(pd, CMD_ENTER_MODE, svids, false, NULL);
		pr_info("NACK SENT\n");
	}
	if (result)
		pr_debug("undefined done\n");
	else
		pr_debug("undefined not done\n");
}

static void parse_exit_mode(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	uint16_t svids;
	uint8_t opos;
	bool result = false;

	pr_debug("Parsing Exit mode data:\n");

	svids = SII_USBPD_VDO_SVID(req->msgdata[VDO_I(IDH)]);
	pr_debug("svid ==> %X\n", svids);
	opos = SII_USBPD_VDO_OBJPOSITION(req->msgdata[VDO_I(IDH)]);
	pr_debug("opos ==> %X\n", opos);

	if (svids == 0xFF02) {
		pr_debug("MHL Mode exited\n");
		set_bit(MHL_EXITED, &pd->cb_params->svdm_sm_inputs);
	} else {
		pr_debug("Undefined mode exited\n");
		set_bit(MHL_NOT_EXITED, &pd->cb_params->svdm_sm_inputs);
	}

	svids = SII_USBPD_VDO_SVID(req->msgdata[0]);

	if (svids != 0xFF02) {
		result = usbpd_svdm_init_resp_nak(pd, CMD_EXIT_MODE, svids, false, NULL);
		pr_info("NACK SENT\n");
	}
	if (result)
		pr_debug("undefined done\n");
	else
		pr_debug("undefined not done\n");
}

static void init_resp(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	switch (SII_USBPD_VDO_CMD(req->msgdata[0])) {
	case DISCOVER_IDENTITY:
		pr_info("DISCOVER_IDENTITY\n");
		set_bit(SVDM_DISC_IDEN_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_vdm_identity(pd, req);
		break;
	case DISCOVER_SVID:
		pr_info("DISCOVER_SVID\n");
		set_bit(DISCOVER_SVID_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_svid(pd, req);
		break;
	case DISCOVER_MODE:
		pr_info("DISCOVER_MODE\n");
		set_bit(DISCOVER_MODE_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_modes(pd, req);
		break;
	case ENTER_MODE:
		pr_info("ENTER_MODE\n");
		set_bit(ENTER_MODE_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_enter_mode(pd, req);
		break;
	case EXIT_MODE:
		pr_info("EXIT_MODE\n");
		set_bit(EXIT_MODE_RCVD, &pd->cb_params->svdm_sm_inputs);
		parse_exit_mode(pd, req);
		break;
	case ATTENTION:
		pr_info("ATTENTION\n");
		set_bit(VDM_ATTENTION_RCVD, &pd->cb_params->svdm_sm_inputs);
		break;
	default:
		pr_debug("UNDEFINED\n");
		break;
	}
}

static bool sii_pd_vdm_handler(struct sii_usbpd_protocol *pd, struct pd_inq *req)
{
	if (!SII_USBPD_VDO_SVDM(req->msgdata[0])) {
		pr_debug("UNSTRUCTURED VDM DETECTED\n");
		/*pr_debug(" value => %X\n", req->msgdata[0]); */
		set_bit(UVDM_RCVD, &pd->cb_params->uvdm_sm_inputs);
		if ((req->msgdata[0] >> 6) & 0x1)
			pr_debug("UVDM => ACK  CURRENT ==> %d00 mA\n", temp_data);
		return true;
	}
	switch (SII_USBPD_VDO_CMDT(req->msgdata[0])) {
	case INITIATOR:
		pr_debug("INITIATOR\n");
		init_resp(pd, req);
		break;
	case RESP_ACK:
		pr_info("RESP_ACK\n");
		init_resp_ack(pd, req);
		break;
	case RESP_NACK:
		pr_debug("RESP_NACK\n");
		init_resp_nack(pd, req);
		break;
	case RESP_BUSY:
		pr_debug("RESP_BUSY\n");
		init_busy(pd, req);
		break;
	default:
		pr_debug("UNDEFINED\n");
		break;
	}
	set_bit(VDM_MSG_RCVD, &pd->cb_params->svdm_sm_inputs);
	return true;
}

#define UVDM_HEADER(cmd, cmd_type, ob_pos, ver, type, vid) ((cmd) \
		| ((cmd_type & 0x3) << 6) \
		| ((ob_pos & 0x7) << 8) \
		| ((ver & 0x3) << 13) \
		| ((type & 0x1) << 15) \
		| ((vid & 0xFFFF) << 16))

bool send_custom_vdm_msg_resp(struct sii_usbpd_protocol *pd, enum data_msg msg_type)
{
	bool b_result;
	uint32_t pd_header = 0, vdm_header;

	vdm_header = UVDM_HEADER(1, 0, 1, 0, 0, 0x2AC1);

	pr_debug("vdm_header => %X\n", vdm_header);

	pd_header = SII_USBPD_HEADER(VDM, pd->pwr_role, pd->data_role, 0, 1);

	b_result = sii_submit_pdmsg(pd, pd_header, &vdm_header, 1);

	if (!b_result) {
		pr_info("sent failed.\n");
		return false;
	}

	return b_result;
}

bool send_custom_vdm_message(struct sii_usbpd_protocol *pd, enum data_msg msg_type, uint8_t type)
{
	bool b_result;
	uint32_t pd_header = 0, vdm_header[2];

	vdm_header[0] = UVDM_HEADER(1, 0, 1, 0, 0, 0x2AC1);
	/*pr_debug("vdm_header0 => %X\n", vdm_header[0]); */

	vdm_header[1] = type;
	/*pr_debug("vdm_header1 => %X\n", vdm_header[1]); */

	pd_header = SII_USBPD_HEADER(VDM, 0, 1, 0, 2);

	b_result = sii_submit_pdmsg(pd, pd_header, vdm_header, 2);

	if (!b_result) {
		pr_info("sent failed.\n");
		return false;
	}

	temp_data = type;

	pr_debug("UVDM => REQ  CURRENT ==>%d00 mA\n", type);

	return b_result;
}

unsigned int svdm_get_identity_info(struct sii_usbpd_protocol *pd, uint32_t *vdm_data, uint8_t cmd)
{
	return USBPD_OBJ_2;
}

bool usbpd_send_vdm_cmd(struct sii_usbpd_protocol *pd,
			enum data_msg msg_type, uint8_t type, uint8_t pdo)
{
	bool b_result;
	uint32_t pd_header = 0, vdm_header = 0;

	pr_info("Send Command : ");
	switch (type) {
	case DISCOVER_IDENTITY:
		vdm_header = SVDM_HEADER(0xFF00, 1, 0, 0, INITIATOR, DISCOVER_IDENTITY);
		pr_info("DISCOVER_IDENTITY\n ");
		break;

	case DISCOVER_SVID:
		if (pdo == 0x01)
			vdm_header = SVDM_HEADER(0xFF00, 1, 0, 0, INITIATOR, DISCOVER_SVID);
		else if (pdo == 0x02)
			vdm_header = SVDM_HEADER(0xFF01, 1, 0, 0, INITIATOR, DISCOVER_SVID);

		pr_info("DISCOVER_SVID\n ");
		break;

	case DISCOVER_MODE:
		if (pdo == 0x01)
			vdm_header = SVDM_HEADER(0xFF02, 1, 0, 0, INITIATOR, DISCOVER_MODE);
		else if (pdo == 0x02)
			vdm_header = SVDM_HEADER(0xFF01, 1, 0, 0, INITIATOR, DISCOVER_MODE);

		pr_info("DISCOVER_MODE\n ");
		break;

	case ENTER_MODE:
		if (pdo == 0x01)
			vdm_header = SVDM_HEADER(0xFF02, 1, 0, 1, INITIATOR, ENTER_MODE);
		else
			vdm_header = SVDM_HEADER(0xFF01, 1, 0, 1, INITIATOR, ENTER_MODE);

		pr_info("ENTER_MODE\n ");
		break;

	case EXIT_MODE:
		vdm_header = SVDM_HEADER(0xFF02, 1, 0, 1, INITIATOR, EXIT_MODE);
		pr_info("EXIT_MODE\n ");
		break;

	case ATTENTION:
		vdm_header = SVDM_HEADER(0xFF02, 1, 0, 1, INITIATOR, ATTENTION);
		pr_info("ATTENTION\n ");
		break;
	default:
		break;

	};

	pd_header = SII_USBPD_HEADER(VDM, pd->pwr_role, pd->data_role, 0, 1);

	pr_debug("pd header = %x\n", pd_header);
	pr_debug("vdm header = %x\n", vdm_header);
	b_result = sii_submit_pdmsg(pd, pd_header, &vdm_header, 1);

	if (!b_result) {
		pr_debug("sent failed.\n");
		return false;
	}

	return b_result;
}

bool usbpd_svdm_init_resp(struct sii_usbpd_protocol *pd, uint8_t cmd, bool is_rcvd, uint32_t *vdo)
{
	uint32_t no_of_objs = 0;
	uint32_t vdm_data[USBPD_MAX_OBJ];
	uint16_t pd_header;

	memset(vdm_data, 0, USBPD_MAX_OBJ * 4);

	if (!vdo) {
		switch (cmd) {
		case DISCOVER_IDENTITY:
			vdm_data[VDM_HDR] = SVDM_HEADER(0xFF00, 1, 0, 0, RESP_ACK, cmd);
			vdm_data[VDM_IDH]   = 0;
			/*SVDM_IDH(1, 1, 5, 0, 0x8976);*/
			vdm_data[VDM_CERT]  = 0;
			/*SII_PREPARE_CERT_STAT_HEADER(0, 0xFFFFF);*/
			vdm_data[VDM_PTYPE] = 0;
			/*SII_PREPARE_PRODUCT_VDO_HEADER(0xFFFF, 0xFFFF);*/
			vdm_data[VDM_CABLE] = 0;
			vdm_data[VDM_AMA]   = 0;
			/*;SVDM_AMA(0, 0, 0, 0, 0, 0,1, 0, 0, 0);*/
			no_of_objs = 5;
			break;
		case DISCOVER_SVID:
			vdm_data[VDM_HDR] = SVDM_HEADER(0xFF00, 1, 0, 0, RESP_ACK, cmd);
			vdm_data[USBPD_OBJ_1] = SVDM_SVIDS(0xFF02, 0x0000);
			no_of_objs = USBPD_OBJ_2;
			break;
		case DISCOVER_MODE:
			vdm_data[VDM_HDR] = SVDM_HEADER(0xFF02, 1, 0, 0, RESP_ACK, cmd);
			vdm_data[USBPD_OBJ_1] = 0xFF02;
			no_of_objs = USBPD_OBJ_2;
			break;
		case ENTER_MODE:
			vdm_data[VDM_HDR] = SVDM_HEADER(0xFF02, 1, 0, 0, RESP_ACK, cmd);
			no_of_objs = USBPD_OBJ_1;
			break;
		case EXIT_MODE:
			vdm_data[VDM_HDR] = SVDM_HEADER(0xFF02, 1, 0, 0, RESP_ACK, cmd);
			no_of_objs = USBPD_OBJ_1;
			break;
		case ATTENTION:
			vdm_data[VDM_HDR] = SVDM_HEADER(0xFF02, 1, 0, 0, RESP_ACK, cmd);
			no_of_objs = USBPD_OBJ_1;
			break;
		default:
			break;
		}
	} else {
		memcpy(vdm_data, vdo, MAX_PD_PAYLOAD);
	}

	pd_header = SII_USBPD_HEADER(VDM, pd->pwr_role, pd->data_role, 0, no_of_objs);


	return sii_submit_pdmsg(pd, pd_header, vdm_data, no_of_objs);
}

bool usbpd_svdm_init_resp_nak(struct sii_usbpd_protocol *pd,
			      uint8_t cmd, uint16_t svid0, bool is_rcvd, uint32_t *vdo)
{
	uint32_t no_of_objs = 0;
	uint32_t vdm_data[USBPD_MAX_OBJ];
	uint16_t pd_header;

	memset(vdm_data, 0, USBPD_MAX_OBJ * 4);


	switch (cmd) {
	case DISCOVER_IDENTITY:
		vdm_data[VDM_HDR] = SVDM_HEADER(svid0, 1, 0, 0, RESP_NACK, cmd);
		/*vdm_data[VDM_IDH]   = 0; */
		/*vdm_data[VDM_CERT]  = 0; */
		/*vdm_data[VDM_PTYPE] = 0; */
		/*vdm_data[VDM_CABLE] = 0; */
		/*vdm_data[VDM_AMA]   = SVDM_AMA(0, 0, 0, 0, 0, 0,
		   1, 0, 0, 0); */
		no_of_objs = 1;
		break;
	case DISCOVER_SVID:
		vdm_data[VDM_HDR] = SVDM_HEADER(svid0, 1, 0, 0, RESP_NACK, cmd);
		no_of_objs = USBPD_OBJ_1;
		break;
	case DISCOVER_MODE:
		vdm_data[VDM_HDR] = SVDM_HEADER(svid0, 1, 0, 0, RESP_NACK, cmd);
		no_of_objs = USBPD_OBJ_1;
		break;
	case ENTER_MODE:
		vdm_data[VDM_HDR] = SVDM_HEADER(svid0, 1, 0, 1, RESP_NACK, cmd);
		no_of_objs = USBPD_OBJ_1;
		break;
	case EXIT_MODE:
		vdm_data[VDM_HDR] = SVDM_HEADER(svid0, 1, 0, 1, RESP_NACK, cmd);
		no_of_objs = USBPD_OBJ_1;
		break;
	case ATTENTION:
		vdm_data[VDM_HDR] = SVDM_HEADER(svid0, 1, 0, 0, RESP_NACK, cmd);
		no_of_objs = USBPD_OBJ_1;
		break;
	default:
		break;
	}

	pd_header = SII_USBPD_HEADER(VDM, pd->pwr_role, pd->data_role, 0, no_of_objs);

	return sii_submit_pdmsg(pd, pd_header, vdm_data, no_of_objs);
}

bool clean_inq_req(struct sii_usbpd_protocol *prot_lyr)
{
	struct pd_inq *req = prot_lyr->in_command;

	if (!req) {
		pr_info("cleaning request is failed\n");
		return false;
	}

	req->command = 0;
	memset(req->msgdata, 0, USBPD_MAX_OBJ);

	return true;
}

static void sii_parse_pd_msg(unsigned char *rx_buf, char count, struct pd_inq *req)
{
	req->command = (rx_buf[1] << 8) | (rx_buf[0] << 0);
	memcpy(req->msgdata, (uint32_t *) (rx_buf + sizeof(req->command)), count);
	req->count = count;
}

bool usbpd_process_msg(struct sii_usbpd_protocol *pd, uint8_t *buf, uint8_t count)
{
	bool b_result = false;


	if (!buf || !pd) {
		pr_err("%s: buf Error\n", __func__);
		return false;
	}
	SII_PLATFORM_DEBUG_ASSERT(pd);
	SII_PLATFORM_DEBUG_ASSERT(pd->in_command);
	SII_PLATFORM_DEBUG_ASSERT(pd->in_command->msgdata);

	sii_parse_pd_msg(buf, count, pd->in_command);

	if (count == 2) {
		b_result = sii_process_ctrl_cmds(pd, pd->in_command);
		goto exit;
	} else {
		b_result = sii_process_data_msg(pd, pd->in_command);
		if (b_result)
			clean_inq_req(pd);
		goto exit;
	}
exit:	return b_result;
}

void usbpd_reset(struct sii_usbpd_protocol *pd)
{
	memset(pd->in_command, 0, sizeof(struct pd_inq));
	/*memset(pd->out_command, 0, sizeof(struct pd_outq)); */
}

static void usbpd_svdm_init(struct sii_usbpd_protocol *prot_lyr)
{
	prot_lyr->svdm_resp[VDM_AMA]
	    = SVDM_AMA(1, 1, 0, 0, 0, 0, 0, 0, 1, 0);
}

void update_data_role(void *context, uint8_t updated_role)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)context;
	struct sii_usbpd_protocol *pUsbpd_prtlyr = (struct sii_usbpd_protocol *)
	    drv_context->pUsbpd_prot;

	if (updated_role == USBPD_ROLE_UFP)
		pUsbpd_prtlyr->data_role = USBPD_ROLE_UFP;
	else
		pUsbpd_prtlyr->data_role = USBPD_ROLE_DFP;
}

void update_pwr_role(void *context, uint8_t updated_role)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)context;
	struct sii_usbpd_protocol *pUsbpd_prtlyr = (struct sii_usbpd_protocol *)
	    drv_context->pUsbpd_prot;

	if (updated_role == USBPD_ROLE_SINK)
		pUsbpd_prtlyr->pwr_role = USBPD_ROLE_SINK;
	else
		pUsbpd_prtlyr->pwr_role = USBPD_ROLE_SOURCE;
}

void *usbpd_core_init(void *context,
		      struct config_param *params,
		      bool (*event_notify_fn)(void *, struct pd_cb_params *))
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)context;
	struct sii_usbpd_protocol *prot_lyr;

	prot_lyr = kzalloc(sizeof(struct sii_usbpd_protocol), GFP_KERNEL);
	if (!prot_lyr)
		return NULL;

	prot_lyr->drv_context = drv_context;
	prot_lyr->evnt_notify_fn = event_notify_fn;

	prot_lyr->cb_params = kzalloc(sizeof(struct pd_cb_params), GFP_KERNEL);

	if (!prot_lyr->cb_params) {
		kfree(prot_lyr);
		return NULL;
	}

	usbpd_svdm_init(prot_lyr);

	prot_lyr->in_command = kzalloc(sizeof(struct pd_inq)
				       , GFP_KERNEL);

	if (!prot_lyr->in_command) {
		pr_debug("%s: Error in creating in-req\n", __func__);
		kfree(prot_lyr);
		kfree(prot_lyr->cb_params);
		return NULL;
	}

	prot_lyr->in_command->msgdata = kzalloc(MAX_PD_PAYLOAD, GFP_KERNEL);
	SII_PLATFORM_DEBUG_ASSERT(prot_lyr->in_command->msgdata);
	if (!prot_lyr->in_command->msgdata) {
		kfree(prot_lyr);
		kfree(prot_lyr->cb_params);
		kfree(prot_lyr->in_command);
		return NULL;
	}
	prot_lyr->rx_msg = kzalloc(sizeof(struct si_data_intf), GFP_KERNEL);

	if (!prot_lyr->rx_msg) {
		kfree(prot_lyr);
		kfree(prot_lyr->cb_params);
		kfree(prot_lyr->in_command);
		kfree(prot_lyr->in_command->msgdata);
		return NULL;
	}
	return prot_lyr;

}

void usbpd_core_exit(void *context)
{
	struct sii_usbpd_protocol *prot_lyr = (struct sii_usbpd_protocol *)
	    context;

	if (prot_lyr->rx_msg != NULL)
		kfree(prot_lyr->rx_msg);
	if (prot_lyr->in_command->msgdata != NULL)
		kfree(prot_lyr->in_command->msgdata);
	if (prot_lyr->in_command != NULL)
		kfree(prot_lyr->in_command);
	if (prot_lyr->cb_params != NULL)
		kfree(prot_lyr->cb_params);
	if (prot_lyr != NULL)
		kfree(prot_lyr);
	pr_info("kfree pd_Core\n");
}
