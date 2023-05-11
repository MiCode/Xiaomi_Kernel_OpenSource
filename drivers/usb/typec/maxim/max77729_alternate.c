/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/mfd/max77729-private.h>
#include <linux/completion.h>
#include <linux/usb/typec/maxim/max77729_usbc.h>
#include <linux/usb/typec/maxim/max77729_alternate.h>

#define UVDM_DEBUG (0)
#define SEC_UVDM_ALIGN		(4)
#define MAX_DATA_FIRST_UVDMSET	12
#define MAX_DATA_NORMAL_UVDMSET	16
#define CHECKSUM_DATA_COUNT		20
#define MAX_INPUT_DATA (255)

extern struct max77729_usbc_platform_data *g_usbc_data;

const struct DP_DP_DISCOVER_IDENTITY DP_DISCOVER_IDENTITY = {
	{
		.BITS.Num_Of_VDO = 1,
		.BITS.Cmd_Type = ACK,
		.BITS.Reserved = 0
	},

	{
		.BITS.VDM_command = Discover_Identity,
		.BITS.Rsvd2_VDM_header = 0,
		.BITS.VDM_command_type = REQ,
		.BITS.Object_Position = 0,
		.BITS.Rsvd_VDM_header = 0,
		.BITS.Structured_VDM_Version = Version_1_0,
		.BITS.VDM_Type = STRUCTURED_VDM,
		.BITS.Standard_Vendor_ID = 0xFF00
	}

};
const struct DP_DP_DISCOVER_ENTER_MODE DP_DISCOVER_ENTER_MODE = {
	{
		.BITS.Num_Of_VDO = 1,
		.BITS.Cmd_Type = ACK,
		.BITS.Reserved = 0
	},
	{
		.BITS.VDM_command = Enter_Mode,
		.BITS.Rsvd2_VDM_header = 0,
		.BITS.VDM_command_type = REQ,
		.BITS.Object_Position = 1,
		.BITS.Rsvd_VDM_header = 0,
		.BITS.Structured_VDM_Version = Version_1_0,
		.BITS.VDM_Type = STRUCTURED_VDM,
		.BITS.Standard_Vendor_ID = 0xFF01
	}
};

struct DP_DP_CONFIGURE DP_CONFIGURE = {
	{
		.BITS.Num_Of_VDO = 2,
		.BITS.Cmd_Type = ACK,
		.BITS.Reserved = 0
	},
	{
		.BITS.VDM_command = 17, /* SVID Specific Command */
		.BITS.Rsvd2_VDM_header = 0,
		.BITS.VDM_command_type = REQ,
		.BITS.Object_Position = 1,
		.BITS.Rsvd_VDM_header = 0,
		.BITS.Structured_VDM_Version = Version_1_0,
		.BITS.VDM_Type = STRUCTURED_VDM,
		.BITS.Standard_Vendor_ID = 0xFF01
	},
	{
		.BITS.SEL_Configuration = num_Cfg_UFP_U_as_UFP_D,
		.BITS.Select_DP_V1p3 = 1,
		.BITS.Select_USB_Gen2 = 0,
		.BITS.Select_Reserved_1 = 0,
		.BITS.Select_Reserved_2 = 0,
		.BITS.DFP_D_PIN_Assign_A = 0,
		.BITS.DFP_D_PIN_Assign_B = 0,
		.BITS.DFP_D_PIN_Assign_C = 0,
		.BITS.DFP_D_PIN_Assign_D = 1,
		.BITS.DFP_D_PIN_Assign_E = 0,
		.BITS.DFP_D_PIN_Assign_F = 0,
		.BITS.DFP_D_PIN_Reserved = 0,
		.BITS.UFP_D_PIN_Assign_A = 0,
		.BITS.UFP_D_PIN_Assign_B = 0,
		.BITS.UFP_D_PIN_Assign_C = 0,
		.BITS.UFP_D_PIN_Assign_D = 0,
		.BITS.UFP_D_PIN_Assign_E = 0,
		.BITS.UFP_D_PIN_Assign_F = 0,
		.BITS.UFP_D_PIN_Reserved = 0,
		.BITS.DP_MODE_Reserved = 0
	}
};

static char VDM_MSG_IRQ_State_Print[9][40] = {
	{"bFLAG_Vdm_Reserve_b0"},
	{"bFLAG_Vdm_Discover_ID"},
	{"bFLAG_Vdm_Discover_SVIDs"},
	{"bFLAG_Vdm_Discover_MODEs"},
	{"bFLAG_Vdm_Enter_Mode"},
	{"bFLAG_Vdm_Exit_Mode"},
	{"bFLAG_Vdm_Attention"},
	{"bFlag_Vdm_DP_Status_Update"},
	{"bFlag_Vdm_DP_Configure"},
};
/* static char DP_Pin_Assignment_Print[7][40] = { */
	/* {"DP_Pin_Assignment_None"}, */
	/* {"DP_Pin_Assignment_A"}, */
	/* {"DP_Pin_Assignment_B"}, */
	/* {"DP_Pin_Assignment_C"}, */
	/* {"DP_Pin_Assignment_D"}, */
	/* {"DP_Pin_Assignment_E"}, */
	/* {"DP_Pin_Assignment_F"}, */
/* }; */

static uint8_t DP_Pin_Assignment_Data[7] = {
	DP_PIN_ASSIGNMENT_NODE,
	DP_PIN_ASSIGNMENT_A,
	DP_PIN_ASSIGNMENT_B,
	DP_PIN_ASSIGNMENT_C,
	DP_PIN_ASSIGNMENT_D,
	DP_PIN_ASSIGNMENT_E,
	DP_PIN_ASSIGNMENT_F,
};

int max77729_process_check_accessory(void *data)
{
	return 1;
}

void max77729_vdm_process_printf(char *type, char *vdm_data, int len)
{
#if 0
	int i = 0;

	for (i = 2; i < len; i++)
		msg_maxim("[%s] , %d, [0x%x]", type, i, vdm_data[i]);
#endif
}

void max77729_vdm_process_set_identity_req_push(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;
	int len = sizeof(DP_DISCOVER_IDENTITY);
	int vdm_header_num = sizeof(UND_DATA_MSG_VDM_HEADER_Type);
	int vdo0_num = 0;
	usbpd_data->send_vdm_identity = 1;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_SET_VDM_REQ;
	memcpy(write_data.write_data, &DP_DISCOVER_IDENTITY, sizeof(DP_DISCOVER_IDENTITY));
	write_data.write_length = len;
	vdo0_num = DP_DISCOVER_IDENTITY.byte_data.BITS.Num_Of_VDO * 4;
	write_data.read_length = OPCODE_SIZE + OPCODE_HEADER_SIZE + vdm_header_num + vdo0_num;
	max77729_usbc_opcode_push(usbpd_data, &write_data);
}

void max77729_vdm_process_set_identity_req(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;
	int len = sizeof(DP_DISCOVER_IDENTITY);
	int vdm_header_num = sizeof(UND_DATA_MSG_VDM_HEADER_Type);
	int vdo0_num = 0;
	usbpd_data->send_vdm_identity = 1;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_SET_VDM_REQ;
	memcpy(write_data.write_data, &DP_DISCOVER_IDENTITY, sizeof(DP_DISCOVER_IDENTITY));
	write_data.write_length = len;
	vdo0_num = DP_DISCOVER_IDENTITY.byte_data.BITS.Num_Of_VDO * 4;
	write_data.read_length = OPCODE_SIZE + OPCODE_HEADER_SIZE + vdm_header_num + vdo0_num;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}

void max77729_vdm_process_set_DP_enter_mode_req(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	usbc_cmd_data write_data;
	int len = sizeof(DP_DISCOVER_ENTER_MODE);
	int vdm_header_num = sizeof(UND_DATA_MSG_VDM_HEADER_Type);
	int vdo0_num = 0;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_SET_VDM_REQ;
	memcpy(write_data.write_data, &DP_DISCOVER_ENTER_MODE, sizeof(DP_DISCOVER_ENTER_MODE));
	write_data.write_length = len;
	vdo0_num = DP_DISCOVER_ENTER_MODE.byte_data.BITS.Num_Of_VDO * 4;
	write_data.read_length = OPCODE_SIZE + OPCODE_HEADER_SIZE + vdm_header_num + vdo0_num;
	max77729_usbc_opcode_push(usbpd_data, &write_data);
}

void max77729_vdm_process_set_DP_configure_mode_req(void *data, uint8_t W_DATA)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	usbc_cmd_data write_data;
	int len = sizeof(DP_CONFIGURE);
	int vdm_header_num = sizeof(UND_DATA_MSG_VDM_HEADER_Type);
	int vdo0_num = 0;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_SET_VDM_REQ;
	memcpy(write_data.write_data, &DP_CONFIGURE, sizeof(DP_CONFIGURE));
	write_data.write_data[6] = W_DATA;
	write_data.write_length = len;
	vdo0_num = DP_CONFIGURE.byte_data.BITS.Num_Of_VDO * 4;
	write_data.read_length = OPCODE_SIZE + OPCODE_HEADER_SIZE + vdm_header_num + vdo0_num;
	max77729_usbc_opcode_push(usbpd_data, &write_data);
}


static int max77729_vdm_process_discover_svids(void *data, char *vdm_data, int len)
{
	struct max77729_usbc_platform_data *usbpd_data = data;

	uint16_t svid = 0;
	uint16_t i = 0;
	DIS_MODE_DP_CAPA_Type *pDP_DIS_MODE = (DIS_MODE_DP_CAPA_Type *)&vdm_data[0];
	/* Number_of_obj has msg_header & vdm_header, each vdo has 2 svids */
	/* This logic can work until Max VDOs 12 */
	int num_of_vdos = (pDP_DIS_MODE->MSG_HEADER.BITS.Number_of_obj - 2) * 2;
	UND_VDO1_Type  *DATA_MSG_VDO1 = (UND_VDO1_Type  *)&vdm_data[8];
	usbpd_data->SVID_DP = 0;
	usbpd_data->SVID_0 = DATA_MSG_VDO1->BITS.SVID_0;
	usbpd_data->SVID_1 = DATA_MSG_VDO1->BITS.SVID_1;

	for (i = 0; i < num_of_vdos; i++) {
		memcpy(&svid, &vdm_data[8 + i * 2], 2);
		if (svid == TypeC_DP_SUPPORT) {
			msg_maxim("svid_%d : 0x%X", i, svid);
			usbpd_data->SVID_DP = svid;
			break;
		}
	}

	if (usbpd_data->SVID_DP == TypeC_DP_SUPPORT) {
   		usbpd_data->dp_is_connect = 1;
		/* If you want to support USB SuperSpeed when you connect
		 * Display port dongle, You should change dp_hs_connect depend
		 * on Pin assignment.If DP use 4lane(Pin Assignment C,E,A),
		 * dp_hs_connect is 1. USB can support HS.If DP use 2lane(Pin Assigment B,D,F), dp_hs_connect is 0. USB
		 * can support SS
		 */
		 usbpd_data->dp_hs_connect = 1;
	}
	msg_maxim("SVID_0 : 0x%X, SVID_1 : 0x%X",
			usbpd_data->SVID_0, usbpd_data->SVID_1);
	return 0;
}

static int max77729_vdm_process_discover_mode(void *data, char *vdm_data, int len)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	DIS_MODE_DP_CAPA_Type *pDP_DIS_MODE = (DIS_MODE_DP_CAPA_Type *)&vdm_data[0];
	UND_DATA_MSG_VDM_HEADER_Type *DATA_MSG_VDM = (UND_DATA_MSG_VDM_HEADER_Type *)&vdm_data[4];

	msg_maxim("vendor_id = 0x%04x , svid_1 = 0x%04x", DATA_MSG_VDM->BITS.Standard_Vendor_ID, usbpd_data->SVID_1);
	if (DATA_MSG_VDM->BITS.Standard_Vendor_ID == TypeC_DP_SUPPORT && usbpd_data->SVID_DP == TypeC_DP_SUPPORT) {
		/*  pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS. */
		msg_maxim("pDP_DIS_MODE->MSG_HEADER.DATA = 0x%08X", pDP_DIS_MODE->MSG_HEADER.DATA);
		msg_maxim("pDP_DIS_MODE->DATA_MSG_VDM_HEADER.DATA = 0x%08X", pDP_DIS_MODE->DATA_MSG_VDM_HEADER.DATA);
		msg_maxim("pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.DATA = 0x%08X", pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.DATA);

		if (pDP_DIS_MODE->MSG_HEADER.BITS.Number_of_obj > 1) {
			if ((pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_UFP_D_Capable)
				&& (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_Receptacle)) {
				usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.UFP_D_Pin_Assignments;
				msg_maxim("1. support UFP_D 0x%08x", usbpd_data->pin_assignment);
			} else if (((pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_UFP_D_Capable)
				&& (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_PLUG))) {
				usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.DFP_D_Pin_Assignments;
				msg_maxim("2. support DFP_D 0x%08x", usbpd_data->pin_assignment);
			} else if (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_DFP_D_and_UFP_D_Capable) {
				if (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Receptacle_Indication == num_USB_TYPE_C_PLUG) {
					usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.DFP_D_Pin_Assignments;
					msg_maxim("3. support DFP_D 0x%08x", usbpd_data->pin_assignment);
				} else {
					usbpd_data->pin_assignment = pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.UFP_D_Pin_Assignments;
					msg_maxim("4. support UFP_D 0x%08x", usbpd_data->pin_assignment);
				}
			} else if (pDP_DIS_MODE->DATA_MSG_MODE_VDO_DP.BITS.Port_Capability == num_DFP_D_Capable) {
				usbpd_data->pin_assignment = DP_PIN_ASSIGNMENT_NODE;
				msg_maxim("do not support Port_Capability num_DFP_D_Capable!!!");
				return -EINVAL;
			} else {
				usbpd_data->pin_assignment = DP_PIN_ASSIGNMENT_NODE;
				msg_maxim("there is not valid object information!!!");
				return -EINVAL;
			}
		}
	}

    max77729_vdm_process_set_DP_enter_mode_req(usbpd_data);

	return 0;
}

static int max77729_vdm_process_enter_mode(void *data, char *vdm_data, int len)
{
	UND_DATA_MSG_VDM_HEADER_Type *DATA_MSG_VDM = (UND_DATA_MSG_VDM_HEADER_Type *)&vdm_data[4];

	if (DATA_MSG_VDM->BITS.VDM_command_type == 1) {
		msg_maxim("EnterMode ACK.");
	} else {
		msg_maxim("EnterMode NAK.");
	}
	return 0;
}

static int max77729_vdm_dp_select_pin(void *data, int multi)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	int pin_sel = 0;

	if (multi) {
		if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_D)
			pin_sel = DP_PIN_ASSIGNMENT_D;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_B)
			pin_sel = DP_PIN_ASSIGNMENT_B;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_F)
			pin_sel = DP_PIN_ASSIGNMENT_F;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_C)
			pin_sel = DP_PIN_ASSIGNMENT_C;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_E)
			pin_sel = DP_PIN_ASSIGNMENT_E;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_A)
			pin_sel = DP_PIN_ASSIGNMENT_A;
		else
			msg_maxim("wrong pin assignment value");
	} else {
		if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_C)
			pin_sel = DP_PIN_ASSIGNMENT_C;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_E)
			pin_sel = DP_PIN_ASSIGNMENT_E;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_A)
			pin_sel = DP_PIN_ASSIGNMENT_A;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_D)
			pin_sel = DP_PIN_ASSIGNMENT_D;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_B)
			pin_sel = DP_PIN_ASSIGNMENT_B;
		else if (usbpd_data->pin_assignment & DP_PIN_ASSIGNMENT_F)
			pin_sel = DP_PIN_ASSIGNMENT_F;
		else
			msg_maxim("wrong pin assignment value");
	}

	return pin_sel;
}

static int max77729_vdm_dp_status_update(void *data, char *vdm_data, int len)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	int i;
	uint8_t multi_func = 0;
	int pin_sel = 0;
	int hpd = 0;
	int hpdirq = 0;
	VDO_MESSAGE_Type *VDO_MSG;
	DP_STATUS_UPDATE_Type *DP_STATUS;
	uint8_t W_DATA = 0x0;

	if (usbpd_data->SVID_DP == TypeC_DP_SUPPORT) {
		DP_STATUS = (DP_STATUS_UPDATE_Type *)&vdm_data[0];

		msg_maxim("DP_STATUS_UPDATE = 0x%08X", DP_STATUS->DATA_DP_STATUS_UPDATE.DATA);

		if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.Port_Connected == 0x00) {
			msg_maxim("port disconnected!");
		} else {
			if (usbpd_data->is_sent_pin_configuration == 0) {
				multi_func = DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.Multi_Function_Preference;
				pin_sel = max77729_vdm_dp_select_pin(usbpd_data, multi_func);
				usbpd_data->dp_selected_pin = pin_sel;
				W_DATA = DP_Pin_Assignment_Data[pin_sel];

				/*msg_maxim("multi_func_preference %d,  %s, W_DATA : %d",
					multi_func, DP_Pin_Assignment_Print[pin_sel], W_DATA);*/

				max77729_vdm_process_set_DP_configure_mode_req(data, W_DATA);

				usbpd_data->is_sent_pin_configuration = 1;
			} /*else {
				msg_maxim("pin configuration is already sent as %s!",
					DP_Pin_Assignment_Print[usbpd_data->dp_selected_pin]);
			}*/
		}

		if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.HPD_State == 1)
			hpd = CCIC_NOTIFY_HIGH;
		else if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.HPD_State == 0)
			hpd = CCIC_NOTIFY_LOW;

		if (DP_STATUS->DATA_DP_STATUS_UPDATE.BITS.HPD_Interrupt == 1)
			hpdirq = CCIC_NOTIFY_IRQ;

	} else {
		/* need to check F/W code */
		VDO_MSG = (VDO_MESSAGE_Type *)&vdm_data[8];
		for (i = 0; i < 6; i++)
			msg_maxim("VDO_%d : %d", i+1, VDO_MSG->VDO[i]);
	}
	return 0;
}

static int max77729_vdm_dp_attention(void *data, char *vdm_data, int len)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	int i;
	int hpd = 0;
	int hpdirq = 0;
	uint8_t multi_func = 0;
	int pin_sel = 0;

	VDO_MESSAGE_Type *VDO_MSG;
	DIS_ATTENTION_MESSAGE_DP_STATUS_Type *DP_ATTENTION;
	uint8_t W_DATA = 0;

	if (usbpd_data->SVID_DP == TypeC_DP_SUPPORT) {
		DP_ATTENTION = (DIS_ATTENTION_MESSAGE_DP_STATUS_Type *)&vdm_data[0];

		msg_maxim("%s DP_ATTENTION = 0x%08X\n", __func__,
			DP_ATTENTION->DATA_MSG_DP_STATUS.DATA);
		if (usbpd_data->is_sent_pin_configuration == 0) {
			multi_func = DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.Multi_Function_Preference;
			pin_sel = max77729_vdm_dp_select_pin(usbpd_data, multi_func);
			usbpd_data->dp_selected_pin = pin_sel;
			W_DATA = DP_Pin_Assignment_Data[pin_sel];

			/*msg_maxim("multi_func_preference %d, %s, W_DATA : %d\n",
				 multi_func, DP_Pin_Assignment_Print[pin_sel], W_DATA);*/

			max77729_vdm_process_set_DP_configure_mode_req(data, W_DATA);
			usbpd_data->is_sent_pin_configuration = 1;
		} /*else {
			msg_maxim("%s : pin configuration is already sent as %s!\n", __func__,
				DP_Pin_Assignment_Print[usbpd_data->dp_selected_pin]);
		}*/
		if (DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.HPD_State == 1)
			hpd = CCIC_NOTIFY_HIGH;
		else if (DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.HPD_State == 0)
			hpd = CCIC_NOTIFY_LOW;

		if (DP_ATTENTION->DATA_MSG_DP_STATUS.BITS.HPD_Interrupt == 1)
			hpdirq = CCIC_NOTIFY_IRQ;

	} else {
	/* need to check the F/W code. */
		VDO_MSG = (VDO_MESSAGE_Type *)&vdm_data[8];

		for (i = 0; i < 6; i++)
			msg_maxim("%s : VDO_%d : %d\n", __func__,
				i+1, VDO_MSG->VDO[i]);
	}

	return 0;
}

static int max77729_vdm_dp_configure(void *data, char *vdm_data, int len)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	UND_DATA_MSG_VDM_HEADER_Type *DATA_MSG_VDM = (UND_DATA_MSG_VDM_HEADER_Type *)&vdm_data[4];


	pr_debug("vendor_id = 0x%04x , svid_1 = 0x%04x", DATA_MSG_VDM->BITS.Standard_Vendor_ID, usbpd_data->SVID_1);
	if (usbpd_data->SVID_DP == TypeC_DP_SUPPORT) {
 	}

 	return 0;
}

void max77729_vdm_message_handler(struct max77729_usbc_platform_data *usbpd_data,
	char *opcode_data, int len)
{
	unsigned char vdm_data[OPCODE_DATA_LENGTH] = {0,};
	UND_DATA_MSG_ID_HEADER_Type *DATA_MSG_ID = NULL;
	UND_PRODUCT_VDO_Type *DATA_MSG_PRODUCT = NULL;
	UND_DATA_MSG_VDM_HEADER_Type vdm_header;

	memset(&vdm_header, 0, sizeof(UND_DATA_MSG_VDM_HEADER_Type));
	memcpy(vdm_data, opcode_data, len);
	memcpy(&vdm_header, &vdm_data[4], sizeof(vdm_header));
	if ((vdm_header.BITS.VDM_command_type) == SEC_UVDM_RESPONDER_NAK) {
		msg_maxim("IGNORE THE NAK RESPONSE !!![%d]", vdm_data[1]);
		return;
	}

	switch (vdm_data[1]) {
	case OPCODE_ID_VDM_DISCOVER_IDENTITY:
		max77729_vdm_process_printf("VDM_DISCOVER_IDENTITY", vdm_data, len);
		/* Message Type Definition */
		DATA_MSG_ID = (UND_DATA_MSG_ID_HEADER_Type *)&vdm_data[8];
		DATA_MSG_PRODUCT = (UND_PRODUCT_VDO_Type *)&vdm_data[16];
		usbpd_data->is_sent_pin_configuration = 0;
		usbpd_data->Vendor_ID = DATA_MSG_ID->BITS.USB_Vendor_ID;
		usbpd_data->Product_ID = DATA_MSG_PRODUCT->BITS.Product_ID;
		usbpd_data->Device_Version = DATA_MSG_PRODUCT->BITS.Device_Version;
		if (usbpd_data->Vendor_ID == 0x2717) {
			usbpd_data->adapter_svid = usbpd_data->Vendor_ID;
			usbpd_data->adapter_id = usbpd_data->Device_Version;
		}
		msg_maxim("Vendor_ID : 0x%X, Product_ID : 0x%X Device Version 0x%X",
			usbpd_data->Vendor_ID, usbpd_data->Product_ID, usbpd_data->Device_Version);
		if (max77729_process_check_accessory(usbpd_data))
			msg_maxim("Samsung Accessory Connected.");
	break;
	case OPCODE_ID_VDM_DISCOVER_SVIDS:
		max77729_vdm_process_printf("VDM_DISCOVER_SVIDS", vdm_data, len);
		max77729_vdm_process_discover_svids(usbpd_data, vdm_data, len);
	break;
	case OPCODE_ID_VDM_DISCOVER_MODES:
		max77729_vdm_process_printf("VDM_DISCOVER_MODES", vdm_data, len);
		vdm_data[0] = vdm_data[2];
		vdm_data[1] = vdm_data[3];
		max77729_vdm_process_discover_mode(usbpd_data, vdm_data, len);
	break;
	case OPCODE_ID_VDM_ENTER_MODE:
		max77729_vdm_process_printf("VDM_ENTER_MODE", vdm_data, len);
		max77729_vdm_process_enter_mode(usbpd_data, vdm_data, len);
	break;
	case OPCODE_ID_VDM_SVID_DP_STATUS:
		max77729_vdm_process_printf("VDM_SVID_DP_STATUS", vdm_data, len);
		vdm_data[0] = vdm_data[2];
		vdm_data[1] = vdm_data[3];
		max77729_vdm_dp_status_update(usbpd_data, vdm_data, len);
	break;
	case OPCODE_ID_VDM_SVID_DP_CONFIGURE:
		max77729_vdm_process_printf("VDM_SVID_DP_CONFIGURE", vdm_data, len);
		max77729_vdm_dp_configure(usbpd_data, vdm_data, len);
	break;
	case OPCODE_ID_VDM_ATTENTION:
		max77729_vdm_process_printf("VDM_ATTENTION", vdm_data, len);
		vdm_data[0] = vdm_data[2];
		vdm_data[1] = vdm_data[3];
		max77729_vdm_dp_attention(usbpd_data, vdm_data, len);
	break;
	case OPCODE_ID_VDM_EXIT_MODE:

	break;

	default:
		break;
	}
}

static int max77729_process_discover_identity(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_DISCOVER_IDENTITY;
	write_data.write_length = 0x1;
	write_data.read_length = 31;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static int max77729_process_discover_svids(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_DISCOVER_SVIDS;
	write_data.write_length = 0x1;
	write_data.read_length = 31;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static int max77729_process_discover_modes(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_DISCOVER_MODES;
	write_data.write_length = 0x1;
	write_data.read_length = 11;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static int max77729_process_enter_mode(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_ENTER_MODE;
	write_data.write_length = 0x1;
	write_data.read_length = 7;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static int max77729_process_attention(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_ATTENTION;
	write_data.write_length = 0x1;
	write_data.read_length = 11;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static int max77729_process_dp_status_update(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_SVID_DP_STATUS;
	write_data.write_length = 0x1;
	write_data.read_length = 11;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static int max77729_process_dp_configure(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	/* send the opcode */
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_GET_VDM_RESP;
	write_data.write_data[0] = OPCODE_ID_VDM_SVID_DP_CONFIGURE;
	write_data.write_length = 0x1;
	write_data.read_length = 11;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	return 0;
}

static void max77729_process_alternate_mode(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	uint32_t mode = usbpd_data->alternate_state;
	int	ret = 0;

	if (mode) {
		msg_maxim("mode : 0x%x", mode);

		if (mode & VDM_DISCOVER_ID)
			ret = max77729_process_discover_identity(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DISCOVER_SVIDS)
			ret = max77729_process_discover_svids(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DISCOVER_MODES)
			ret = max77729_process_discover_modes(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_ENTER_MODE)
			ret = max77729_process_enter_mode(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DP_STATUS_UPDATE)
			ret = max77729_process_dp_status_update(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_DP_CONFIGURE)
			ret = max77729_process_dp_configure(usbpd_data);
		if (ret)
			goto process_error;
		if (mode & VDM_ATTENTION)
			ret = max77729_process_attention(usbpd_data);
process_error:
		usbpd_data->alternate_state = 0;
	}
}

void max77729_receive_alternate_message(struct max77729_usbc_platform_data *data, MAX77729_VDM_MSG_IRQ_STATUS_Type *VDM_MSG_IRQ_State)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	static int last_alternate = 0;

DISCOVER_ID:
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_ID) {
		pr_debug(": %s", &VDM_MSG_IRQ_State_Print[1][0]);
		usbpd_data->alternate_state |= VDM_DISCOVER_ID;
		last_alternate = VDM_DISCOVER_ID;
	}

DISCOVER_SVIDS:
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_SVIDs) {
		pr_debug(": %s", &VDM_MSG_IRQ_State_Print[2][0]);
		if (last_alternate != VDM_DISCOVER_ID) {
			msg_maxim("%s vdm miss\n", __func__);
			VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_ID = 1;
			goto DISCOVER_ID;
		}
		usbpd_data->alternate_state |= VDM_DISCOVER_SVIDS;
		last_alternate = VDM_DISCOVER_SVIDS;
	}

DISCOVER_MODES:
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_MODEs) {
		msg_maxim(": %s", &VDM_MSG_IRQ_State_Print[3][0]);
		if (last_alternate != VDM_DISCOVER_SVIDS &&
				last_alternate != VDM_DP_CONFIGURE) {
			msg_maxim("%s vdm miss\n", __func__);
			VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_SVIDs = 1;
			goto DISCOVER_SVIDS;
		}
		usbpd_data->alternate_state |= VDM_DISCOVER_MODES;
		last_alternate = VDM_DISCOVER_MODES;
	}

	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Enter_Mode) {
		msg_maxim(": %s", &VDM_MSG_IRQ_State_Print[4][0]);
		if (last_alternate != VDM_DISCOVER_MODES) {
			msg_maxim("%s vdm miss\n", __func__);
			VDM_MSG_IRQ_State->BITS.Vdm_Flag_Discover_MODEs = 1;
			goto DISCOVER_MODES;
		}
		usbpd_data->alternate_state |= VDM_ENTER_MODE;
		last_alternate = VDM_ENTER_MODE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Exit_Mode) {
		msg_maxim(": %s", &VDM_MSG_IRQ_State_Print[5][0]);
		usbpd_data->alternate_state |= VDM_EXIT_MODE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_Attention) {
		msg_maxim(": %s", &VDM_MSG_IRQ_State_Print[6][0]);
		usbpd_data->alternate_state |= VDM_ATTENTION;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_DP_Status_Update) {
		msg_maxim(": %s", &VDM_MSG_IRQ_State_Print[7][0]);
		usbpd_data->alternate_state |= VDM_DP_STATUS_UPDATE;
		last_alternate = VDM_DP_STATUS_UPDATE;
	}
	if (VDM_MSG_IRQ_State->BITS.Vdm_Flag_DP_Configure) {
		msg_maxim(": %s", &VDM_MSG_IRQ_State_Print[8][0]);
		usbpd_data->alternate_state |= VDM_DP_CONFIGURE;
		last_alternate = VDM_DP_CONFIGURE;
	}

	max77729_process_alternate_mode(usbpd_data);
}
void max77729_vdm_process_set_alternate_mode(void *data, int mode)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_SET_ALTERNATEMODE;
	write_data.write_data[0] = mode;
	write_data.write_length = 0x1;
	write_data.read_length = 0x1;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}

void max77729_set_enable_alternate_mode(int mode)
{
	struct max77729_usbc_platform_data *usbpd_data = NULL;
	static int check_is_driver_loaded;
	static int prev_alternate_mode;
	int is_first_booting = 0;
	struct max77729_pd_data *pd_data = NULL;
	u8 status[11] = {0, };

	usbpd_data = g_usbc_data;

	if (!usbpd_data)
		return;
	is_first_booting = usbpd_data->is_first_booting;
	pd_data = usbpd_data->pd_data;

	msg_maxim("is_first_booting  : %x mode %x",
			usbpd_data->is_first_booting, mode);

	usbpd_data->set_altmode = mode;

	if ((mode & ALTERNATE_MODE_NOT_READY) &&
	    (mode & ALTERNATE_MODE_READY)) {
		msg_maxim("mode is invalid!");
		return;
	}
	if ((mode & ALTERNATE_MODE_START) && (mode & ALTERNATE_MODE_STOP)) {
		msg_maxim("mode is invalid!");
		return;
	}
	if (mode & ALTERNATE_MODE_RESET) {
		msg_maxim("mode is reset! check_is_driver_loaded=%d, prev_alternate_mode=%d",
			check_is_driver_loaded, prev_alternate_mode);
		if (check_is_driver_loaded &&
		    (prev_alternate_mode == ALTERNATE_MODE_START)) {

			msg_maxim("[No process] alternate mode is reset as start!");
			prev_alternate_mode = ALTERNATE_MODE_START;
		} else if (check_is_driver_loaded &&
			   (prev_alternate_mode == ALTERNATE_MODE_STOP)) {
			msg_maxim("[No process] alternate mode is reset as stop!");
			prev_alternate_mode = ALTERNATE_MODE_STOP;
		} else {
			;
		}
	} else {
		if (mode & ALTERNATE_MODE_NOT_READY) {
			check_is_driver_loaded = 0;
			msg_maxim("alternate mode is not ready!");
		} else if (mode & ALTERNATE_MODE_READY) {
			check_is_driver_loaded = 1;
			msg_maxim("alternate mode is ready!");
		} else {
			;
		}

		if (check_is_driver_loaded) {
			switch (is_first_booting) {
			case 0: /*this routine is calling after complete a booting.*/
				if (mode & ALTERNATE_MODE_START) {
					max77729_vdm_process_set_alternate_mode(usbpd_data,
						MAXIM_ENABLE_ALTERNATE_SRC_VDM);
					msg_maxim("[NO BOOTING TIME] !!!alternate mode is started!");
					if (usbpd_data->cc_data->current_pr == SNK && (pd_data->current_dr == DFP)) {
						usbpd_data->send_vdm_identity = 1;
						max77729_vdm_process_set_identity_req(usbpd_data);
						msg_maxim("[NO BOOTING TIME] SEND THE PACKET (DEX HUB) ");
					}

				} else if (mode & ALTERNATE_MODE_STOP) {
					max77729_vdm_process_set_alternate_mode(usbpd_data,
						MAXIM_ENABLE_ALTERNATE_SRCCAP);
					msg_maxim("[NO BOOTING TIME] alternate mode is stopped!");
				}

				break;
			case 1:
				if (mode & ALTERNATE_MODE_START) {
					msg_maxim("[ON BOOTING TIME] !!!alternate mode is started!");
					prev_alternate_mode = ALTERNATE_MODE_START;
					max77729_vdm_process_set_alternate_mode(usbpd_data,
						MAXIM_ENABLE_ALTERNATE_SRC_VDM);
					msg_maxim("!![ON BOOTING TIME] SEND THE PACKET REGARDING IN CASE OF VR/DP ");
					/* FOR THE DEX FUNCTION. */
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_USBC_STATUS1, &status[0]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_USBC_STATUS2, &status[1]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_BC_STATUS, &status[2]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_CC_STATUS0, &status[3]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_CC_STATUS1, &status[4]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_PD_STATUS0, &status[5]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_PD_STATUS1, &status[6]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_UIC_INT_M, &status[7]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_CC_INT_M, &status[8]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_PD_INT_M, &status[9]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_VDM_INT_M, &status[10]);
					msg_maxim("USBC1:0x%02x, USBC2:0x%02x, BC:0x%02x",
						status[0], status[1], status[2]);
					msg_maxim("CC_STATUS0:0x%x, CC_STATUS1:0x%x, PD_STATUS0:0x%x, PD_STATUS1:0x%x",
						status[3], status[4], status[5], status[6]);
					msg_maxim("UIC_INT_M:0x%x, CC_INT_M:0x%x, PD_INT_M:0x%x, VDM_INT_M:0x%x",
						status[7], status[8], status[9], status[10]);
					if (usbpd_data->cc_data->current_pr == SNK && (pd_data->current_dr == DFP)
						&& usbpd_data->is_first_booting) {
						usbpd_data->send_vdm_identity = 1;
						max77729_vdm_process_set_identity_req(usbpd_data);
						msg_maxim("[ON BOOTING TIME] SEND THE PACKET (DEX HUB)  ");
					}
					max77729_write_reg(usbpd_data->muic, REG_VDM_INT_M, 0xF0);
					max77729_write_reg(usbpd_data->muic, REG_PD_INT_M, 0x0);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_PD_INT_M, &status[9]);
					max77729_read_reg(usbpd_data->muic, MAX77729_USBC_REG_VDM_INT_M, &status[10]);
					msg_maxim("UIC_INT_M:0x%x, CC_INT_M:0x%x, PD_INT_M:0x%x, VDM_INT_M:0x%x",
						status[7], status[8], status[9], status[10]);
					usbpd_data->is_first_booting = 0;
				} else if (mode & ALTERNATE_MODE_STOP) {
					msg_maxim("[ON BOOTING TIME] alternate mode is stopped!");
				}
				break;

			default:
				msg_maxim("Never calling");
				msg_maxim("[Never calling] is_first_booting [ %d]", is_first_booting);
				break;

			}
		}
	}
}
