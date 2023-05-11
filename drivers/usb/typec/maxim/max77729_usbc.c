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

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77729.h>
#include <linux/mfd/max77729-private.h>
#include <linux/usb/typec/maxim/max77729-muic.h>
#include <linux/usb/typec/maxim/max77729_usbc.h>
#include <linux/usb/typec/maxim/max77729_alternate.h>
#include <linux/firmware.h>

#define DRIVER_VER		"1.0VER"

#define MAX77729_MAX_APDCMD_TIME (2*HZ)

#define MAX77729_PMIC_REG_INTSRC_MASK 0x23
#define MAX77729_PMIC_REG_INTSRC 0x22

#define MAX77729_IRQSRC_CHG	(1 << 0)
#define MAX77729_IRQSRC_FG      (1 << 2)
#define MAX77729_IRQSRC_MUIC	(1 << 3)

static const unsigned int extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

struct max77729_usbc_platform_data *g_usbc_data;
static struct class *max_adapter_class;
int vdm_count;

#ifdef MAX77729_SYS_FW_UPDATE
#define MAXIM_DEFAULT_FW		"secure_max77729.bin"
#define MAXIM_SPU_FW			"/maxim/pdic_fw.bin"

struct pdic_fw_update {
	char id[10];
	char path[50];
	int need_verfy;
	int enforce_do;
};
#endif

static void max77729_usbc_mask_irq(struct max77729_usbc_platform_data *usbc_data);
static void max77729_usbc_umask_irq(struct max77729_usbc_platform_data *usbc_data);
static void max77729_get_version_info(struct max77729_usbc_platform_data *usbc_data);
void max77729_usbc_dequeue_queue(struct max77729_usbc_platform_data *usbc_data);
bool is_empty_usbc_cmd_queue(usbc_cmd_queue_t *usbc_cmd_queue);

static void set_msgheader(void *data, int msg_type, int obj_num)
{
	/* Common : Fill the VDM OpCode MSGHeader */
	SEND_VDM_BYTE_DATA *MSG_HDR;
	uint8_t *SendMSG = (uint8_t *)data;

	MSG_HDR = (SEND_VDM_BYTE_DATA *)&SendMSG[0];
	MSG_HDR->BITS.Num_Of_VDO = obj_num;
	if (msg_type == NAK)
		MSG_HDR->BITS.Reserved = 7;
	MSG_HDR->BITS.Cmd_Type = msg_type;
}

void set_uvdmheader_cmd(void *data, int vendor_id, int vdm_type, unsigned char cmd)
{
	/*Common : Fill the UVDMHeader*/
	UND_UNSTRUCTURED_VDM_HEADER_Type *UVDM_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;
	UVDM_HEADER = (UND_UNSTRUCTURED_VDM_HEADER_Type *)&SendMSG[4];
	UVDM_HEADER->BITS.USB_Vendor_ID = vendor_id;
	UVDM_HEADER->BITS.VDM_TYPE = vdm_type;
	UVDM_HEADER->BITS.VENDOR_DEFINED_MESSAGE = SEC_UVDM_UNSTRUCTURED_VDM | cmd | 0x100;
	return;
}

static int max77729_send_vdm_write_message(void *data)
{
	struct SS_UNSTRUCTURED_VDM_MSG vdm_opcode_msg;
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	uint8_t *SendMSG = (uint8_t *)data;
	usbc_cmd_data write_data;
	int len = sizeof(struct SS_UNSTRUCTURED_VDM_MSG);

	memset(&vdm_opcode_msg, 0, len);
	/* Common : MSGHeader */
	memcpy(&vdm_opcode_msg.byte_data, &SendMSG[0], 1);
	/* 8. Copy the data from SendMSG buffer */
	memcpy(&vdm_opcode_msg.VDO_MSG.VDO[0], &SendMSG[4], 28);
	/* 9. Write SendMSG buffer via I2C */
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_VDM_DISCOVER_SET_VDM_REQ;
	memcpy(write_data.write_data, &vdm_opcode_msg, sizeof(vdm_opcode_msg));
	write_data.write_length = len;
	write_data.read_length = len;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
	msg_maxim("opcode is sent");
	return 0;
}


static int usbpd_send_vdm(void *data, unsigned char cmd, uint8_t *buf, size_t size)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	uint8_t SendMSG[32] = {0,};
	int time_left;
    int obj_num;
    int i=0;
	u8 usbc_status2 = 0;

	/* if (buf == NULL) { */
		/* msg_maxim("given data is not valid !"); */
		/* return -EINVAL; */
	/* } */

	msg_maxim("adapter_svid : %x", usbpd_data->adapter_svid);
	if ((usbpd_data->adapter_svid != USB_PD_MI_SVID)) {
		msg_maxim("Not support the UVDM except MI_SVID!");
		return -ENXIO;
	}

	obj_num = size + 1; //uvdmheader(1)+VDO(size)
	set_msgheader(SendMSG, ACK, obj_num);
	set_uvdmheader_cmd(SendMSG, USB_PD_MI_SVID, 0, cmd); //0x27170201~6 message
	if(size != 0){
		for(i = 0; i < size; i++)
			SendMSG[i+8] = buf[i];
	}

	switch (cmd) {
		case 1:
		case 2:
		case 3:
		       SendMSG[0] = 0x9;
		       break;
		case 4:
		case 5:
		       SendMSG[0] = 0xD;
		       break;
		case 6:
		       SendMSG[0] = 0xA;
		       break;
		default:
		       break;
	}

	max77729_send_vdm_write_message(SendMSG);
	reinit_completion(&usbpd_data->uvdm_longpacket_out_wait);
	/* Wait Response*/
	time_left =
		wait_for_completion_interruptible_timeout(&usbpd_data->uvdm_longpacket_out_wait,
					  msecs_to_jiffies(2000));

	if (time_left <= 0) {
		//to add the protection code to avoid the PD collision. 
		max77729_read_reg(usbpd_data->muic, REG_USBC_STATUS2, &usbc_status2);
		msg_maxim("Need to check the reason sysmsg : %d", usbc_status2);		
		max77729_usbc_clear_queue(usbpd_data);	
		max77729_send_vdm_write_message(SendMSG);
		msleep(300);
		return -ETIME;
	}

	msg_maxim("exit : short data transfer complete!");
	return size;
}

static int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while (cnt < (tmplen / 2)) {
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p++;
		cnt++;
	}
	if (tmplen % 2 != 0)
		out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if (outlen != NULL)
		*outlen = tmplen / 2 + tmplen % 2;

	return tmplen / 2 + tmplen % 2;
}

#define BSWAP_32(x) \
	(u32)((((u32)(x) & 0xff000000) >> 24) | \
			(((u32)(x) & 0x00ff0000) >> 8) | \
			(((u32)(x) & 0x0000ff00) << 8) | \
			(((u32)(x) & 0x000000ff) << 24))

static void usbpd_sha256_bitswap32(unsigned int *array, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		array[i] = BSWAP_32(array[i]);
	}
}

void charTointMax(char *str, int input_len, unsigned int *out, unsigned int *outlen)
{
	int i;

	if (outlen != NULL)
		*outlen = 0;
	for (i = 0; i < (input_len / 4 + 1); i++) {
		out[i] = ((str[i*4 + 3] * 0x1000000) |
				(str[i*4 + 2] * 0x10000) |
				(str[i*4 + 1] * 0x100) |
				str[i*4]);
		*outlen = *outlen + 1;
	}

	msg_maxim("%s: outlen = %d\n", __func__, *outlen);
	/*for (i = 0; i < *outlen; i++)
		msg_maxim("%s: out[%d] = %08x\n", __func__, i, out[i]);*/
	msg_maxim("%s: char to int done.\n", __func__);
}

static int usbpd_request_vdm_cmd(enum uvdm_state cmd, unsigned char *data)
{
	int rc = 0;
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	uint8_t vdm_data[32];
	unsigned int *int_data;
	unsigned int outlen;
	int i;

	if (in_interrupt()) {
		int_data = kmalloc(40, GFP_ATOMIC);
		msg_maxim("%s: kmalloc atomic ok.\n", __func__);
	} else {
		int_data = kmalloc(40, GFP_KERNEL);
		msg_maxim("%s: kmalloc kernel ok.\n", __func__);
	}
	memset(int_data, 0, 40);

	charTointMax(data, vdm_count, int_data, &outlen);

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
	case USBPD_UVDM_CHARGER_TEMP:
	case USBPD_UVDM_CHARGER_VOLTAGE:
		rc = usbpd_send_vdm(usbpd_data, cmd, data, 0);
		if (rc < 0) {
    		msg_maxim("failed to send %d\n", cmd);
			return rc;
		}
		break;
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
		rc = usbpd_send_vdm(usbpd_data, cmd ,data, 1);
		if (rc < 0) {
    		msg_maxim("failed to send %d\n", cmd);
			return rc;
		}
		break;
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_AUTHENTICATION:
// it need to add or not based on the taget plaform(Xiaomi)
		usbpd_sha256_bitswap32(int_data, 4);
		/*for (i = 0; i < 4; i++) {
			msg_maxim("%08x\n", int_data[i]);
		}*/

		for (i = 0; i < 4; i++) {
			vdm_data[i * 4 + 0] = (int_data[i] & 0xFF) >> 0;
			vdm_data[i * 4 + 1] = (int_data[i] & 0xFF00) >> 8;
			vdm_data[i * 4 + 2] = (int_data[i] & 0XFF0000) >> 16;
			vdm_data[i * 4 + 3] = (int_data[i] & 0XFF000000) >> 24;
		}

		/*for (i = 0; i < 16; i++) {
			msg_maxim("i:%d  %02x\n", i, vdm_data[i]);
		}*/

		rc = usbpd_send_vdm(usbpd_data, cmd, vdm_data, 16);
		if (rc < 0) {
    		msg_maxim("failed to send %d\n", cmd);
			return rc;
		}
		break;
	default:
    	msg_maxim("cmd:%d is not support\n", cmd);
		break;
	}

	return rc;
}

void usbpd_mi_vdm_received_cb(struct max77729_usbc_platform_data *usbpd_data,
		char *opcode_data, int len)
{
 	uint8_t ReadMSG[32] = {0,};
	int cmd;

	memcpy(ReadMSG, opcode_data, OPCODE_DATA_LENGTH);
	cmd = UVDM_HDR_CMD(ReadMSG[2]);

    /*for(i=0; i<10;i++){
       	msg_maxim("%x", ReadMSG[i]);
    }
    msg_maxim("\n");

    for(i=10; i<20;i++){
       	msg_maxim("%x", ReadMSG[i]);
    }
    msg_maxim("\n");*/
        //need to add the calutation for current and volate and etc.
	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		//usbpd_data->vdm_data.ta_version = 0x00030001;
		usbpd_data->vdm_data.ta_version = (ReadMSG[9]<<24)|(ReadMSG[8]<<16)|(ReadMSG[7]<<8)|ReadMSG[6];
		msg_maxim("ta_version : %x",usbpd_data->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		//usbpd_data->vdm_data.ta_voltage = 0x33 * 100;  /* mV */
		usbpd_data->vdm_data.ta_voltage = ((ReadMSG[9]<<24)|(ReadMSG[8]<<16)|(ReadMSG[7]<<8)|ReadMSG[6])*100;
		msg_maxim("ta_voltage : %d", usbpd_data->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		//usbpd_data->vdm_data.ta_temp = 0x00;
		usbpd_data->vdm_data.ta_temp = (ReadMSG[9]<<24)|(ReadMSG[8]<<16)|(ReadMSG[7]<<8)|ReadMSG[6];
		msg_maxim("ta_temp : %d", usbpd_data->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_SESSION_SEED:
		usbpd_data->vdm_data.s_secert [0] = (ReadMSG[9]<<24)|(ReadMSG[8]<<16)|(ReadMSG[7]<<8)|ReadMSG[6];
		usbpd_data->vdm_data.s_secert [1] = 0x00000000;
		usbpd_data->vdm_data.s_secert [2] = 0x00000000;
		usbpd_data->vdm_data.s_secert [3] = 0x00000000;
		msg_maxim("s_secert");
		break;
	case USBPD_UVDM_AUTHENTICATION:
		usbpd_data->vdm_data.digest [0] = ((ReadMSG[9]<<24)|(ReadMSG[8]<<16)|(ReadMSG[7]<<8)|ReadMSG[6]) & 0xFFFFFFFF;//0x850a0b71;
		usbpd_data->vdm_data.digest [1] = ((ReadMSG[13]<<24)|(ReadMSG[12]<<16)|(ReadMSG[11]<<8)|ReadMSG[10]) & 0xFFFFFFFF;//0x58479a1c;
		usbpd_data->vdm_data.digest [2] = ((ReadMSG[17]<<24)|(ReadMSG[16]<<16)|(ReadMSG[15]<<8)|ReadMSG[14]) & 0xFFFFFFFF;//0x04a54634;
		usbpd_data->vdm_data.digest [3] = ((ReadMSG[21]<<24)|(ReadMSG[20]<<16)|(ReadMSG[19]<<8)|ReadMSG[18]) & 0xFFFFFFFF;//0x1875206b;
		/*msg_maxim("digest 0: %08lx\n",usbpd_data->vdm_data.digest[0]);
		msg_maxim("digest 1: %08lx\n",usbpd_data->vdm_data.digest[1]);
		msg_maxim("digest 2: %08lx\n",usbpd_data->vdm_data.digest[2]);
		msg_maxim("digest 3: %08lx\n",usbpd_data->vdm_data.digest[3]);*/
		msg_maxim("digest");
		break;
	case USBPD_UVDM_VERIFIED:
		msg_maxim("verified");
		break;
	default:
		break;
	}
	usbpd_data->uvdm_state = cmd;
    complete(&usbpd_data->uvdm_longpacket_out_wait);
}

static void max77729_send_role_swap_message(struct max77729_usbc_platform_data *usbpd_data, u8 mode)
{
	usbc_cmd_data write_data;

	//max77729_usbc_clear_queue(usbpd_data);
	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x37;
	/* 0x1 : DR_SWAP, 0x2 : PR_SWAP, 0x4: Manual Role Swap */
	write_data.write_data[0] = mode;
	write_data.write_length = 0x1;
	write_data.read_length = 0x1;
	max77729_usbc_opcode_write_immediately(usbpd_data, &write_data);
}

void max77729_power_role_change(struct max77729_usbc_platform_data *usbpd_data, int power_role)
{
	msg_maxim("power_role = 0x%x", power_role);

	switch (power_role) {
	case TYPE_C_ATTACH_SRC:
	case TYPE_C_ATTACH_SNK:
		max77729_send_role_swap_message(usbpd_data, POWER_ROLE_SWAP);
		break;
	};
}
void max77729_rprd_mode_change(struct max77729_usbc_platform_data *usbpd_data, u8 mode)
{
	msg_maxim("mode = 0x%x", mode);

	switch (mode) {
	case TYPE_C_ATTACH_DFP:
	case TYPE_C_ATTACH_UFP:
		max77729_send_role_swap_message(usbpd_data, MANUAL_ROLE_SWAP);
		msleep(1000);
		break;
	default:
		break;
	};
}

void max77729_data_role_change(struct max77729_usbc_platform_data *usbpd_data, int data_role)
{
	msg_maxim("data_role = 0x%x", data_role);
	msleep(300);

	switch (data_role) {
	case TYPE_C_ATTACH_DFP:
	case TYPE_C_ATTACH_UFP:
		max77729_send_role_swap_message(usbpd_data, DATA_ROLE_SWAP);
		break;
	};
}

static int max77729_pr_set(const struct typec_capability *cap, enum typec_role role)
{
	struct max77729_usbc_platform_data *usbpd_data = container_of(cap, struct max77729_usbc_platform_data, typec_cap);

	if (!usbpd_data)
		return -EINVAL;

	msg_maxim("typec_power_role=%d, typec_data_role=%d, role=%d",
		usbpd_data->typec_power_role, usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_power_role != TYPEC_SINK
	    && usbpd_data->typec_power_role != TYPEC_SOURCE)
		return -EPERM;
	else if (usbpd_data->typec_power_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_SINK) {
		msg_maxim("try reversing, from Source to Sink");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		max77729_power_role_change(usbpd_data, TYPE_C_ATTACH_SNK);
	} else if (role == TYPEC_SOURCE) {
		msg_maxim("try reversing, from Sink to Source");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		max77729_power_role_change(usbpd_data, TYPE_C_ATTACH_SRC);
	} else {
		msg_maxim("invalid typec_role");
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		if (usbpd_data->typec_power_role != role)
		return -ETIMEDOUT;
	}
	return 0;
}

static int max77729_dr_set(const struct typec_capability *cap, enum typec_data_role role)
{
	/* struct max77729_usbc_platform_data *usbpd_data = g_usbc_data; */
	struct max77729_usbc_platform_data *usbpd_data = container_of(cap, struct max77729_usbc_platform_data, typec_cap);

    //need to add the proection code to check SINK or Source or No_connection.
    /*
	if (usbpd_data->typec_data_role != TYPEC_DEVICE
		&& usbpd_data->typec_data_role != TYPEC_HOST)
		return -EPERM;
	else if (usbpd_data->typec_data_role == role)
		return -EPERM;
    */

/*     usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR; */

	/* reinit_completion(&usbpd_data->typec_reverse_completion); */
	/* if (role == TYPEC_DEVICE) { */
		/* msg_maxim("try reversing, from DFP to UFP"); */
		/* max77729_data_role_change(usbpd_data, TYPE_C_ATTACH_UFP); */
	/* } else if (role == TYPEC_HOST) { */
		/* msg_maxim("try reversing, from UFP to DFP"); */
		/* max77729_data_role_change(usbpd_data, TYPE_C_ATTACH_DFP); */
	/* } else { */
		/* msg_maxim("invalid typec_role"); */
		/* return -EIO; */
	/* } */
	/* if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion, */
				/* msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) { */
		/* usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE; */
		/* return -ETIMEDOUT; */
	/* } */

	if (!usbpd_data)
		return -EINVAL;
	msg_maxim("typec_power_role=%d, typec_data_role=%d, role=%d",
			usbpd_data->typec_power_role, usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_data_role != TYPEC_DEVICE
			&& usbpd_data->typec_data_role != TYPEC_HOST)
		return -EPERM;
	else if (usbpd_data->typec_data_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_DEVICE) {
		msg_maxim("try reversing, from DFP to UFP");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		max77729_data_role_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else if (role == TYPEC_HOST) {
		msg_maxim("try reversing, from UFP to DFP");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		max77729_data_role_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else {
		msg_maxim("invalid typec_role");
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		return -ETIMEDOUT;
	}
	return 0;
}

static void max77729_get_srccap_ext_message(struct max77729_usbc_platform_data *usbpd_data)
{
	usbc_cmd_data write_data;
	max77729_usbc_clear_queue(usbpd_data);
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_SEND_GET_REQUEST;
	write_data.write_data[0] = 0;
	write_data.write_data[1] = 0;
	write_data.write_data[2] = 0;
	write_data.write_length = 0x3;
	write_data.read_length = 28;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}
#if 0
void max77729_get_srccap(struct max77729_usbc_platform_data *usbpd_data, int idx)
{
	usbc_cmd_data write_data;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x65;
	write_data.write_data[0] = 0;

	write_data.write_length = 2;
	write_data.read_length = 30;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}
#endif

void max77729_rerun_chgdet(struct max77729_usbc_platform_data *usbpd_data)
{
	usbc_cmd_data write_data;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x02;
	write_data.write_data[0] = 0x93;

	write_data.write_length = 2;
	write_data.read_length = 2;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}
void max77729_send_new_srccap(struct max77729_usbc_platform_data *usbpd_data, int idx)
{
	usbc_cmd_data write_data;
	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_SET_SRCCAP;
	write_data.write_data[0] = 1;
	if (idx) {
		write_data.write_data[1] = 0xC8;
		write_data.write_data[2] = 0xD0;
		write_data.write_data[3] = 0x02;
		write_data.write_data[4] = 0x36;
	} else {
		write_data.write_data[1] = 0x96;
		write_data.write_data[2] = 0x90;
		write_data.write_data[3] = 0x01;
		write_data.write_data[4] = 0x28;
	}

	write_data.write_length = 0x6;
	write_data.read_length = 2;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}

static int max77729_pd_get_svid(struct max77729_usbc_platform_data *usbc_data)
{

	if (usbc_data->adapter_svid != 0)
		return 0;

	reinit_completion(&usbc_data->pps_in_wait);
	usbc_data->typec_try_pps_enable = TRY_PPS_ENABLE;
	max77729_get_srccap_ext_message(usbc_data);
	if (!wait_for_completion_timeout(&usbc_data->pps_in_wait,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbc_data->typec_try_pps_enable = TRY_PPS_NONE;
		return -ETIMEDOUT;
	}
	return 0;
}


static void max77729_request_response(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_READ_RESPONSE_FOR_GET_REQUEST;
	value.write_data[0]= 0x00;
	value.write_length = 0x01;
	value.read_length = 31;
	//adding the delay.
	msleep(30);
	max77729_usbc_opcode_push(usbc_data, &value);
	pr_err("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);
}

void max77729_extend_msg_process(struct max77729_usbc_platform_data *usbc_data, unsigned char *data,
		unsigned char len)
{
	unsigned short vid=0x0;
	unsigned short pid=0x0;
	unsigned short xid=0x0;
	vid = *(unsigned short *)(data + 2);
	pid = *(unsigned short *)(data + 4);
	xid = *(unsigned int *)(data + 6);
	if (vid == 0x2717) {
		usbc_data->adapter_svid	= vid;
		usbc_data->adapter_id	= pid;
	}
	usbc_data->xid	= xid;
	msg_maxim("%s, %04x, %04x, %08x",
		__func__, vid, pid, xid);
	if (vid == 0x2717) {	//&&(pid == 0x741b)
		msg_maxim("Xiaomi PPS TA");
		max77729_vdm_process_set_identity_req_push(usbc_data);
	}
}

void max77729_read_response(struct max77729_usbc_platform_data *usbc_data, unsigned char *data)
{
	//u8 pd_status0;

	switch (data[1] >> 5) {
	case OPCODE_GET_SRC_CAP_EXT:
		max77729_extend_msg_process(usbc_data, data+2, data[1] & 0x1F);
		break;
	case OPCODE_GET_STATUS:
	case OPCODE_GET_BAT_CAP:
	case OPCODE_GET_BAT_STS:
		break;
	default:
		//max77729_read_reg(usbc_data->muic, REG_PD_STATUS0, &pd_status0);
		//if(pd_status0 == Not_Supported_Received){
		//Kernel driver should send VDM message if SINK && UFP.
		max77729_vdm_process_set_identity_req_push(usbc_data);
		//}
		//msg_maxim("%s, Err : %d", __func__, data[1]);
		break;
	}
	//complete(&usbc_data->uvdm_longpacket_in_wait);
	if (usbc_data->typec_try_pps_enable == TRY_PPS_ENABLE){
			usbc_data->typec_try_pps_enable = TRY_PPS_NONE;
			complete(&usbc_data->pps_in_wait);
	}
}

static void max77729_get_srccap_message(struct max77729_usbc_platform_data *usbpd_data)
{
	usbc_cmd_data write_data;
	max77729_usbc_clear_queue(usbpd_data);
	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x31;
	write_data.write_data[0] = 0;
	write_data.write_length = 0x1;
	write_data.read_length = 28;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}

/* void max77729_set_enable_pps(bool enable, int ppsVol, int ppsCur) */
/* { */
	/* usbc_cmd_data value; */

	/* init_usbc_cmd_data(&value); */
	/* value.opcode = 0x3C; */
	/* if (enable) { */
		/* value.write_data[0] = 0x1; //PPS_ON On */
		/* value.write_data[1] = (ppsVol / 20) & 0xFF; //Default Output Voltage (Low), 20mV */
		/* value.write_data[2] = ((ppsVol / 20) >> 8) & 0xFF; //Default Output Voltage (High), 20mV */
		/* value.write_data[3] = (ppsCur / 50) & 0x7F; //Default Operating Current, 50mA */
		/* value.write_length = 4; */
		/* value.read_length = 1; */
		/* pr_info("%s : PPS_On (Vol:%dmV, Cur:%dmA)\n", __func__, ppsVol, ppsCur); */
	/* } else { */
		/* value.write_data[0] = 0x0; //PPS_ON Off */
		/* value.write_length = 1; */
		/* value.read_length = 1; */
		/* pr_info("%s : PPS_Off\n", __func__); */
	/* } */
	/* max77729_usbc_opcode_write(g_usbc_data, &value); */
/* } */


/* int max77729_select_pps(int num, int ppsVol, int ppsCur) */
/* { */
	/* struct max77729_usbc_platform_data *pusbpd = g_usbc_data; */
	/* usbc_cmd_data value; */

	/* init_usbc_cmd_data(&value); */

	/* value.opcode = 0x3A; */
	/* value.write_data[0] = (num & 0xFF); [> APDO Position <] */
	/* value.write_data[1] = (ppsVol / 20) & 0xFF; [> Output Voltage(Low) <] */
	/* value.write_data[2] = ((ppsVol / 20) >> 8) & 0xFF; [> Output Voltage(High) <] */
	/* value.write_data[3] = (ppsCur / 50) & 0x7F; [> Operating Current <] */
	/* value.write_length = 4; */
	/* value.read_length = 1; [> Result <] */
	/* max77729_usbc_opcode_write(pusbpd, &value); */

/* [dchg] TODO: add return value */
	/* return 0; */
/* } */

int max77729_current_pr_state(struct max77729_usbc_platform_data *usbc_data)
{
	int current_pr = usbc_data->cc_data->current_pr;
	return current_pr;

}

void blocking_auto_vbus_control(int enable)
{
	int current_pr = 0;

	msg_maxim("disable : %d", enable);

	if (enable) {
		current_pr = max77729_current_pr_state(g_usbc_data);
		switch (current_pr) {
		case SRC:
			/* turn off the vbus */
			max77729_vbus_turn_on_ctrl(g_usbc_data, OFF, false);
			break;
		default:
			break;
		}
		g_usbc_data->mpsm_mode = MPSM_ON;
	} else {
		current_pr = max77729_current_pr_state(g_usbc_data);
		switch (current_pr) {
		case SRC:
			max77729_vbus_turn_on_ctrl(g_usbc_data, ON, false);
			break;
		default:
			break;

		}
		g_usbc_data->mpsm_mode = MPSM_OFF;
	}
	msg_maxim("current_pr : %x disable : %x", current_pr, enable);
}
EXPORT_SYMBOL(blocking_auto_vbus_control);

static void vbus_control_hard_reset(struct work_struct *work)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	msg_maxim("current_pr=%d", usbpd_data->cc_data->current_pr);

	if (usbpd_data->cc_data->current_pr == SRC)
		max77729_vbus_turn_on_ctrl(usbpd_data, ON, false);
}


void max77729_usbc_enable_audio(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data write_data;

	/* we need new function for BIT_CCDbgEn */
	usbc_data->op_ctrl1_w |= (BIT_CCDbgEn | BIT_CCAudEn);

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_CCCTRL1_W;
	write_data.write_data[0] = usbc_data->op_ctrl1_w;
	write_data.write_length = 0x1;
	write_data.read_length = 0x1;
	max77729_usbc_opcode_write(usbc_data, &write_data);
	msg_maxim("Enable Audio Detect");
}

#if 0
static void max77729_send_role_swap_message(struct max77729_usbc_platform_data *usbpd_data, u8 mode)
{
	usbc_cmd_data write_data;

	max77729_usbc_clear_queue(usbpd_data);
	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x37;
	/* 0x1 : DR_SWAP, 0x2 : PR_SWAP, 0x4: Manual Role Swap */
	write_data.write_data[0] = mode;
	write_data.write_length = 0x1;
	write_data.read_length = 0x1;
	max77729_usbc_opcode_write(usbpd_data, &write_data);
}
#endif



/* void max77729_data_role_change(struct max77729_usbc_platform_data *usbpd_data, int data_role) */
/* { */
	/* msg_maxim("data_role = 0x%x", data_role); */

	/* switch (data_role) { */
	/* case TYPE_C_ATTACH_DFP: */
	/* case TYPE_C_ATTACH_UFP: */
		/* max77729_send_role_swap_message(usbpd_data, DATA_ROLE_SWAP); */
		/* break; */
	/* }; */
/* } */

#if 0 //Brandon : need to enable this feature if customer want ot use PS_SWAP or DR_SWAP
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int max77729_dr_set(const struct typec_capability *cap, enum typec_data_role role)
#else
static int max77729_dr_set(struct typec_port *port, enum typec_data_role role)
#endif
{
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
	struct max77729_usbc_platform_data *usbpd_data = container_of(cap, struct max77729_usbc_platform_data, typec_cap);
#else
	struct max77729_usbc_platform_data *usbpd_data = typec_get_drvdata(port);
#endif
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif /* CONFIG_USB_HW_PARAM */

	if (!usbpd_data)
		return -EINVAL;
	msg_maxim("typec_power_role=%d, typec_data_role=%d, role=%d",
		usbpd_data->typec_power_role, usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_data_role != TYPEC_DEVICE
		&& usbpd_data->typec_data_role != TYPEC_HOST)
		return -EPERM;
	else if (usbpd_data->typec_data_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_DEVICE) {
		msg_maxim("try reversing, from DFP to UFP");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		max77729_data_role_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else if (role == TYPEC_HOST) {
		msg_maxim("try reversing, from UFP to DFP");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		max77729_data_role_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else {
		msg_maxim("invalid typec_role");
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		return -ETIMEDOUT;
	}
#if defined(CONFIG_USB_HW_PARAM)
	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_DR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */
	return 0;
}

#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int max77729_pr_set(const struct typec_capability *cap, enum typec_role role)
#else
static int max77729_pr_set(struct typec_port *port, enum typec_role role)
#endif
{
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
	struct max77729_usbc_platform_data *usbpd_data = container_of(cap, struct max77729_usbc_platform_data, typec_cap);
#else
	struct max77729_usbc_platform_data *usbpd_data = typec_get_drvdata(port);
#endif
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif /* CONFIG_USB_HW_PARAM */

	if (!usbpd_data)
		return -EINVAL;

	msg_maxim("typec_power_role=%d, typec_data_role=%d, role=%d",
		usbpd_data->typec_power_role, usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_power_role != TYPEC_SINK
	    && usbpd_data->typec_power_role != TYPEC_SOURCE)
		return -EPERM;
	else if (usbpd_data->typec_power_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_SINK) {
		msg_maxim("try reversing, from Source to Sink");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		max77729_power_role_change(usbpd_data, TYPE_C_ATTACH_SNK);
	} else if (role == TYPEC_SOURCE) {
		msg_maxim("try reversing, from Sink to Source");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		max77729_power_role_change(usbpd_data, TYPE_C_ATTACH_SRC);
	} else {
		msg_maxim("invalid typec_role");
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		if (usbpd_data->typec_power_role != role)
		return -ETIMEDOUT;
	}
#if defined(CONFIG_USB_HW_PARAM)
	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_PR_SWAP_COUNT);
#endif
	return 0;
}


#if defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static const struct typec_operations max77729_ops = {
	.dr_set = max77729_dr_set,
	.pr_set = max77729_pr_set,
};
#endif
#endif

int max77729_get_pd_support(struct max77729_usbc_platform_data *usbc_data)
{
	bool support_pd_role_swap = false;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, "maxim,max77729_pdic");

	if (np)
		support_pd_role_swap = of_property_read_bool(np, "support_pd_role_swap");
	else
		msg_maxim("np is null");

	msg_maxim("TYPEC_CLASS: support_pd_role_swap is %d, usbc_data->pd_support : %d",
		support_pd_role_swap, usbc_data->pd_support);

	if (support_pd_role_swap && usbc_data->pd_support)
		return TYPEC_PWR_MODE_PD;

	return usbc_data->pwr_opmode;
}

static int max77729_firmware_update_sys(struct max77729_usbc_platform_data *data, int fw_dir)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	max77729_fw_header *fw_header;
	const struct firmware *fw_entry;
	int fw_size, ret = 0;

	if (!usbc_data) {
		msg_maxim("usbc_data is null!!");
		return -ENODEV;
	}

	ret = request_firmware(&fw_entry, MAXIM_SPU_FW, usbc_data->dev);
	if (ret) {
		pr_info("%s: firmware is not available %d\n", __func__, ret);
		return ret;
	}

	fw_size = (int)fw_entry->size;
	fw_header = (max77729_fw_header *)fw_entry->data;
	ret = max77729_usbc_fw_update(usbc_data->max77729, MAXIM_SPU_FW,
				fw_size, 1);
	release_firmware(fw_entry);
	return ret;
}


#if defined(MAX77729_SYS_FW_UPDATE)
static int max77729_firmware_update_sysfs(struct max77729_usbc_platform_data *usbpd_data, int fw_dir)
{
	int ret = 0;
	usbpd_data->fw_update = 1;
	max77729_usbc_mask_irq(usbpd_data);
	max77729_write_reg(usbpd_data->muic, REG_PD_INT_M, 0xFF);
	max77729_write_reg(usbpd_data->muic, REG_CC_INT_M, 0xFF);
	max77729_write_reg(usbpd_data->muic, REG_UIC_INT_M, 0xFF);
	max77729_write_reg(usbpd_data->muic, REG_VDM_INT_M, 0xFF);
	ret = max77729_firmware_update_sys(usbpd_data, fw_dir);
	max77729_write_reg(usbpd_data->muic, REG_UIC_INT_M, REG_UIC_INT_M_INIT);
	max77729_write_reg(usbpd_data->muic, REG_CC_INT_M, REG_CC_INT_M_INIT);
	max77729_write_reg(usbpd_data->muic, REG_PD_INT_M, REG_PD_INT_M_INIT);
	max77729_write_reg(usbpd_data->muic, REG_VDM_INT_M, REG_VDM_INT_M_INIT);
	max77729_set_enable_alternate_mode(ALTERNATE_MODE_START);
	max77729_usbc_umask_irq(usbpd_data);
	if (ret)
		usbpd_data->fw_update = 2;
	else
		usbpd_data->fw_update = 0;
	return ret;
}
#endif

#if 0
static unsigned long max77729_get_firmware_size(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data
		= (struct max77729_usbc_platform_data *)data;
	unsigned long ret = 0;

	ret = usbpd_data->max77729->fw_size;

	return ret;
}
#endif

#if defined(MAX77729_SYS_FW_UPDATE)
static void max77729_firmware_update_sysfs_work(struct work_struct *work)
{
	struct max77729_usbc_platform_data *usbpd_data = container_of(work,
			struct max77729_usbc_platform_data, fw_update_work);

	max77729_firmware_update_sysfs(usbpd_data, 1);
}
#endif

/*
 * assume that 1 HMD device has name(14),vid(4),pid(4) each, then
 * max 32 HMD devices(name,vid,pid) need 806 bytes including TAG, NUM, comba
 */
#define MAX_HMD_POWER_STORE_LEN	1024
enum {
	HMD_POWER_MON = 0,	/* monitor name field */
	HMD_POWER_VID,		/* vid field */
	HMD_POWER_PID,		/* pid field */
	HMD_POWER_FIELD_MAX,
};

#if 0
/* convert VID/PID string to uint in hexadecimal */
static int _max77729_strtoint(char *tok, uint *result)
{
	int  ret = 0;

	if (!tok || !result) {
		msg_maxim("invalid arg!");
		ret = -EINVAL;
		goto end;
	}

	if (strlen(tok) == 5 && tok[4] == 0xa/*LF*/) {
		/* continue since it's ended with line feed */
	} else if (strlen(tok) != 4) {
		msg_maxim("%s should have 4 len, but %lu!", tok, strlen(tok));
		ret = -EINVAL;
		goto end;
	}

	ret = kstrtouint(tok, 16, result);
	if (ret) {
		msg_maxim("fail to convert %s! ret:%d", tok, ret);
		goto end;
	}
end:
	return ret;
}
#endif

static int pd_set_pd_verify_process(struct device *dev, int verify_in_process)
{
	int ret = 0;
	//union power_supply_propval val = {0,};
	//struct power_supply *usb_psy = NULL;

	dev_err(dev, "[%s] pd verify in process:%d\n",
		__func__, verify_in_process);
/*
	usb_psy = power_supply_get_by_name("usb");

	if (usb_psy) {
		val.intval = verify_in_process;
		ret = power_supply_set_property(usb_psy,
			POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS, &val);
	} else {
		adapter_err("[%s] usb psy not found!\n", __func__);
	}
*/
	return ret;
}

static ssize_t max77729_fw_update(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int start_fw_update = 0;
	unsigned char *test_buf;

	if (kstrtou32(buf, 0, &start_fw_update)) {
		dev_err(dev,
			"%s: Failed converting from str to u32.", __func__);
	}

	msg_maxim("start_fw_update %d", start_fw_update);
	switch (start_fw_update) {
	case 1:
		max77729_firmware_update_sysfs(g_usbc_data, 1);
		break;
    case 17:
        max77729_dr_set(&g_usbc_data->typec_cap, TYPEC_HOST);
        break;

/*maxim test xiaomi adaptor case as below*/

    case 18:
/*
# Send Ustruct VDM 0x27170101
host.I2CWr(0x4a, 0x21, 0x48)
host.I2CWr(0x4a, 0x22, 0x09)
host.I2CWr(0x4a, 0x23, 0x01)
host.I2CWr(0x4a, 0x24, 0x01)
host.I2CWr(0x4a, 0x25, 0x17)
host.I2CWr(0x4a, 0x26, 0x27)
host.I2CWr(0x4a, 0x41, 0x00)
*/
        usbpd_request_vdm_cmd(USBPD_UVDM_CHARGER_VERSION,test_buf);
        break;
    case 19:
/*
# Send Ustruct VDM 0x27170102
host.I2CWr(0x4a, 0x21, 0x48)
host.I2CWr(0x4a, 0x22, 0x09)
host.I2CWr(0x4a, 0x23, 0x02)
host.I2CWr(0x4a, 0x24, 0x01)
host.I2CWr(0x4a, 0x25, 0x17)
host.I2CWr(0x4a, 0x26, 0x27)
host.I2CWr(0x4a, 0x41, 0x00)
*/
        usbpd_request_vdm_cmd(USBPD_UVDM_CHARGER_VOLTAGE,test_buf);
        break;
    case 20:
/*
# Send Ustruct VDM 0x27170103
host.I2CWr(0x4a, 0x21, 0x48)
host.I2CWr(0x4a, 0x22, 0x09)
host.I2CWr(0x4a, 0x23, 0x03)
host.I2CWr(0x4a, 0x24, 0x01)
host.I2CWr(0x4a, 0x25, 0x17)
host.I2CWr(0x4a, 0x26, 0x27)
host.I2CWr(0x4a, 0x41, 0x00)
*/
        usbpd_request_vdm_cmd(USBPD_UVDM_CHARGER_TEMP,test_buf);
        break;
    case 21:
/*
# Send Ustruct VDM 0x27170104 0XA601934C 0x1403BFD9 0x55EBD77F 0x509BA17F
    host.I2CWr(0x4a, 0x21, 0x48)
    host.I2CWr(0x4a, 0x22, 0x0D)
#0x27170104
    host.I2CWr(0x4a, 0x23, 0x04)
    host.I2CWr(0x4a, 0x24, 0x01)
    host.I2CWr(0x4a, 0x25, 0x17)
    host.I2CWr(0x4a, 0x26, 0x27)
#0XA601934C
    host.I2CWr(0x4a, 0x27, 0x4C)
    host.I2CWr(0x4a, 0x28, 0x93)
    host.I2CWr(0x4a, 0x29, 0x01)
    host.I2CWr(0x4a, 0x2A, 0xA6)
#0x1403BFD9
    host.I2CWr(0x4a, 0x2B, 0xD9)
    host.I2CWr(0x4a, 0x2C, 0xBF)
    host.I2CWr(0x4a, 0x2D, 0x03)
    host.I2CWr(0x4a, 0x2E, 0x14)
#0x55EBD77F
    host.I2CWr(0x4a, 0x2F, 0x7F)
    host.I2CWr(0x4a, 0x30, 0xD7)
    host.I2CWr(0x4a, 0x31, 0xED)
    host.I2CWr(0x4a, 0x32, 0x55)
#0x509BA17F
    host.I2CWr(0x4a, 0x33, 0x7F)
    host.I2CWr(0x4a, 0x34, 0xA1)
    host.I2CWr(0x4a, 0x35, 0x9B)
    host.I2CWr(0x4a, 0x36, 0x50)
    host.I2CWr(0x4a, 0x41, 0x00)
*/
        test_buf[0] = 0x4C;
        test_buf[1] = 0x93;
        test_buf[2] = 0x01;
        test_buf[3] = 0xA6;

        test_buf[4] = 0xD9;
        test_buf[5] = 0xBF;
        test_buf[6] = 0x03;
        test_buf[7] = 0x14;

        test_buf[8] = 0x7F;
        test_buf[9] = 0xD7;
        test_buf[10] = 0xED;
        test_buf[11] = 0x55;

        test_buf[12] = 0x7F;
        test_buf[13] = 0xA1;
        test_buf[14] = 0x9B;
        test_buf[15] = 0x50;
        usbpd_request_vdm_cmd(USBPD_UVDM_SESSION_SEED,test_buf);
        break;
    case 22:
/*
# Send Ustruct VDM 0x27170105 0XBF9B01F9 0x8F353B1F 0x30737687 0x578B2E7B
host.I2CWr(0x4a, 0x21, 0x48)
host.I2CWr(0x4a, 0x22, 0x0D)
#0x27170105
host.I2CWr(0x4a, 0x23, 0x05)
host.I2CWr(0x4a, 0x24, 0x01)
host.I2CWr(0x4a, 0x25, 0x17)
host.I2CWr(0x4a, 0x26, 0x27)
#0XBF9B01F9
host.I2CWr(0x4a, 0x27, 0xF9)
host.I2CWr(0x4a, 0x28, 0x01)
host.I2CWr(0x4a, 0x29, 0x9B)
host.I2CWr(0x4a, 0x2A, 0xBF)
#0x8F353B1F
host.I2CWr(0x4a, 0x2B, 0x1F)
host.I2CWr(0x4a, 0x2C, 0x3B)
host.I2CWr(0x4a, 0x2D, 0x35)
host.I2CWr(0x4a, 0x2E, 0x8F)
#0x30737687
host.I2CWr(0x4a, 0x2F, 0x87)
host.I2CWr(0x4a, 0x30, 0x76)
host.I2CWr(0x4a, 0x31, 0x73)
host.I2CWr(0x4a, 0x32, 0x30)
#0x578B2E7B
host.I2CWr(0x4a, 0x33, 0x7B)
host.I2CWr(0x4a, 0x34, 0x2E)
host.I2CWr(0x4a, 0x35, 0x8B)
host.I2CWr(0x4a, 0x36, 0x57)
host.I2CWr(0x4a, 0x41, 0x00)

*/
        test_buf[0] = 0x1F;
        test_buf[1] = 0x3B;
        test_buf[2] = 0x35;
        test_buf[3] = 0x8F;

        test_buf[4] = 0x1F;
        test_buf[5] = 0x3B;
        test_buf[6] = 0x35;
        test_buf[7] = 0x8F;

        test_buf[8] = 0x87;
        test_buf[9] = 0x76;
        test_buf[10] = 0x73;
        test_buf[11] = 0x30;

        test_buf[12] = 0x7B;
        test_buf[13] = 0x2E;
        test_buf[14] = 0x8B;
        test_buf[15] = 0x57;
        usbpd_request_vdm_cmd(USBPD_UVDM_AUTHENTICATION,test_buf);
        break;
    case 23:
/*
# Send Ustruct VDM 0x27170106
host.I2CWr(0x4a, 0x21, 0x48)
host.I2CWr(0x4a, 0x22, 0x0A)
host.I2CWr(0x4a, 0x23, 0x06)
host.I2CWr(0x4a, 0x24, 0x01)
host.I2CWr(0x4a, 0x25, 0x17)
host.I2CWr(0x4a, 0x26, 0x27)
#0X00000001
host.I2CWr(0x4a, 0x27, 0x01)
host.I2CWr(0x4a, 0x28, 0x00)
host.I2CWr(0x4a, 0x29, 0x00)
host.I2CWr(0x4a, 0x2A, 0x00)
host.I2CWr(0x4a, 0x41, 0x00)
*/
        test_buf[0] = 0x01;
        test_buf[1] = 0x00;
        test_buf[2] = 0x00;
        test_buf[3] = 0x00;
        usbpd_request_vdm_cmd(USBPD_UVDM_VERIFIED,test_buf);
        break;
    case 24:
/*
# Send GET_Source_Cap
host.I2CWr(0x4a, 0x21, 0x31)
host.I2CWr(0x4a, 0x22, 0x00)
host.I2CWr(0x4a, 0x41, 0x00)
*/
        max77729_get_srccap_message(g_usbc_data);
        break;
   case 25:
  //enable the PPS.
        //max77729_set_enable_pps(1,5000,1000);
        break;
   case 26:
        //max77729_select_pps(5,9000,1000);
        break;
   case 27:
        max77729_pd_get_svid(g_usbc_data);
	}
	return size;
}
static DEVICE_ATTR(fw_update, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, max77729_fw_update);

static ssize_t request_vdm_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int cmd, ret;
	unsigned char buffer[64];
	unsigned char *data;
	int count;

	if (in_interrupt()) {
		data = kmalloc(40, GFP_ATOMIC);
		msg_maxim("%s: kmalloc atomic ok.\n", __func__);
	} else {
		data = kmalloc(40, GFP_KERNEL);
		msg_maxim("%s: kmalloc kernel ok.\n", __func__);
	}
	memset(data, 0, 40);

	ret = sscanf(buf, "%d,%s\n", &cmd, buffer);
	msg_maxim("%s:cmd:%d, buffer:%s\n", __func__, cmd, buffer);

	StringToHex(buffer, data, &count);
	msg_maxim("%s:count = %d\n", __func__, count);

	/*for (i = 0; i < count; i++)
		msg_maxim("%02x", data[i]);*/

	vdm_count = count;
	usbpd_request_vdm_cmd(cmd, (unsigned char *)data);
	kfree(data);

	return size;
}

static ssize_t request_vdm_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	int i;
	char data[16], str_buf[128] = {0};

	dev_err(dev, "request_vdm_cmd_show: uvdm_state: %d\n", usbpd_data->uvdm_state);
	switch (usbpd_data->uvdm_state) {
	case USBPD_UVDM_CHARGER_VERSION:
		return snprintf(buf, PAGE_SIZE, "%d,%x", usbpd_data->uvdm_state, usbpd_data->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		return snprintf(buf, PAGE_SIZE, "%d,%d", usbpd_data->uvdm_state, usbpd_data->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		return snprintf(buf, PAGE_SIZE, "%d,%d", usbpd_data->uvdm_state, usbpd_data->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
	case USBPD_UVDM_CONNECT:
	case USBPD_UVDM_DISCONNECT:
	case USBPD_UVDM_VERIFIED:
	case USBPD_UVDM_REMOVE_COMPENSATION:
	case USBPD_UVDM_NAN_ACK:
		return snprintf(buf, PAGE_SIZE, "%d,Null", usbpd_data->uvdm_state);
		break;
	/* case USBPD_UVDM_REVERSE_AUTHEN: */
		/* return snprintf(buf, PAGE_SIZE, "%d,%d", usbpd_data->uvdm_state, pd->vdm_data.reauth); */
		/* break; */
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < 4; i++) {
			memset(data, 0, sizeof(data));
			snprintf(data, sizeof(data), "%08lx", usbpd_data->vdm_data.digest[i]);
			strlcat(str_buf, data, sizeof(str_buf));
		}

		dev_err(dev, "str_buf: %s\n",str_buf);
		return snprintf(buf, PAGE_SIZE, "%d,%s", usbpd_data->uvdm_state, str_buf);
		break;
	default:
		/* usbpd_err(&pd->dev, "feedbak cmd:%d is not support\n", cmd); */
		break;
	}
	return snprintf(buf, PAGE_SIZE, "%d,%s", usbpd_data->uvdm_state, str_buf);

}
static DEVICE_ATTR_RW(request_vdm_cmd);

static ssize_t current_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	dev_err(dev, "%s: current_state is %d\n", __func__, usbpd_data->pd_state);

	if (usbpd_data->sink_Ready) {
		dev_err(dev, "%s: %s\n", __func__, "SNK_Ready");
		return snprintf(buf, PAGE_SIZE, "%s\n", "SNK_Ready");
	} else if (usbpd_data->source_Ready) {
		dev_err(dev, "%s: %s\n", __func__, "SRC_Ready");
		return snprintf(buf, PAGE_SIZE, "%s\n", "SRC_Ready");
	}

	return 0;
}
static DEVICE_ATTR_RO(current_state);

static ssize_t adapter_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	max77729_pd_get_svid(usbpd_data);
	dev_err(dev, "%s: adapter_id is %08x\n", __func__, usbpd_data->adapter_id);

	return snprintf(buf, PAGE_SIZE, "%08x\n", usbpd_data->adapter_id);
}
static DEVICE_ATTR_RO(adapter_id);

static ssize_t adapter_svid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	max77729_pd_get_svid(usbpd_data);
	dev_err(dev, "%s: adapter_svid is %04x\n", __func__, usbpd_data->adapter_svid);

	return snprintf(buf, PAGE_SIZE, "%04x\n", usbpd_data->adapter_svid);
}
static DEVICE_ATTR_RO(adapter_svid);

static ssize_t verify_process_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	int val;

	if (sscanf(buf, "%d\n", &val) != 1) {
		usbpd_data->verify_process = 0;
		return -EINVAL;
	}

	usbpd_data->verify_process = !!val;
	dev_err(dev, "%s: batterysecret verify process :%d\n",
		__func__, usbpd_data->verify_process);

	pd_set_pd_verify_process(dev, usbpd_data->verify_process);

	return size;
}

static ssize_t verify_process_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", usbpd_data->verify_process);
}
static DEVICE_ATTR_RW(verify_process);

static ssize_t usbpd_verifed_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	int val = 0;

	if (sscanf(buf, "%d\n", &val) != 1) {
		usbpd_data->verifed = 0;
		return -EINVAL;
	}

	dev_err(dev, "%s: batteryd set usbpd verifyed :%d\n", __func__, val);
	usbpd_data->verifed = !!val;

	if (usbpd_data->verifed)
		max77729_get_srccap_message(usbpd_data);

	return size;
}

static ssize_t usbpd_verifed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", usbpd_data->verifed);
}
static DEVICE_ATTR_RW(usbpd_verifed);

static ssize_t current_pr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	const char *pr = "none";

	dev_err(dev, "%s: current_pr is %d\n", __func__, usbpd_data->typec_power_role);

	usbpd_data->typec_power_role;

	if (usbpd_data->typec_power_role == TYPEC_SINK)
		pr = "sink";
	else if (usbpd_data->typec_power_role == TYPEC_SOURCE)
		pr = "source";

	return snprintf(buf, PAGE_SIZE, "%s\n", pr);
}
static DEVICE_ATTR_RO(current_pr);

static ssize_t pdo_n_show(struct device *dev, struct device_attribute *attr,
		char *buf);

#define PDO_ATTR(n) {					\
	.attr	= { .name = __stringify(pdo##n), .mode = 0444 },	\
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
static ssize_t pdo_n_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	int i;

	for (i = 0; i < ARRAY_SIZE(dev_attr_pdos); i++) {
		if (attr == &dev_attr_pdos[i])
			/* dump the PDO as a hex string */
			return snprintf(buf, PAGE_SIZE, "%08x\n",
				usbpd_data->received_pdos[i]);
	}

	dev_err(dev, "%s: Invalid PDO index\n", __func__);
	return -EINVAL;
}

static struct attribute *max77729_attr[] = {
	/* &dev_attr_contract.attr, */
	/* &dev_attr_initial_pr.attr, */
	/* &dev_attr_initial_dr.attr, */
	/* &dev_attr_current_dr.attr, */
	/* &dev_attr_src_cap_id.attr, */
	/* &dev_attr_pdo_h.attr, */
	/* &dev_attr_select_pdo.attr, */
	/* &dev_attr_rdo.attr, */
	/* &dev_attr_rdo_h.attr, */
	/* &dev_attr_hard_reset.attr, */
	/* &dev_attr_get_src_cap_ext.attr, */
	/* &dev_attr_get_status.attr, */
	/* &dev_attr_get_pps_status.attr, */
	/* &dev_attr_get_battery_cap.attr, */
	/* &dev_attr_get_battery_status.attr, */
	/* &dev_attr_adapter_version.attr, */

	&dev_attr_fw_update.attr,
	&dev_attr_request_vdm_cmd.attr,
	&dev_attr_current_state.attr,
	&dev_attr_adapter_id.attr,
	&dev_attr_adapter_svid.attr,
	&dev_attr_verify_process.attr,
	&dev_attr_usbpd_verifed.attr,
	&dev_attr_current_pr.attr,
	&dev_attr_pdos[0].attr,
	&dev_attr_pdos[1].attr,
	&dev_attr_pdos[2].attr,
	&dev_attr_pdos[3].attr,
	&dev_attr_pdos[4].attr,
	&dev_attr_pdos[5].attr,
	&dev_attr_pdos[6].attr,
	NULL,
};

static const struct attribute_group max77729_group = {
	.attrs = max77729_attr,
};

static const struct attribute_group *max77729_groups[] = {
	&max77729_group,
	NULL,
};

static void max77729_get_version_info(struct max77729_usbc_platform_data *usbc_data)
{
	u8 hw_rev[4] = {0, };
	u8 sw_main[3] = {0, };

	max77729_read_reg(usbc_data->muic, REG_UIC_HW_REV, &hw_rev[0]);
	max77729_read_reg(usbc_data->muic, REG_UIC_FW_MINOR, &sw_main[1]);
	max77729_read_reg(usbc_data->muic, REG_UIC_FW_REV, &sw_main[0]);

	usbc_data->HW_Revision = hw_rev[0];
	usbc_data->FW_Minor_Revision = sw_main[1] & MINOR_VERSION_MASK;
	usbc_data->FW_Revision = sw_main[0];

	/* H/W, Minor, Major, Boot */
	msg_maxim("HW rev is %02Xh, FW rev is %02X.%02X!",
			usbc_data->HW_Revision, usbc_data->FW_Revision, usbc_data->FW_Minor_Revision);

}

void max77729_usbc_disable_auto_vbus(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data write_data;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = 0x54;
	write_data.write_data[0] = 0x0;
	write_data.write_length = 0x1;
	write_data.read_length = 0x1;
	max77729_usbc_opcode_write(usbc_data, &write_data);
	msg_maxim("TURN OFF THE AUTO VBUS");
	usbc_data->auto_vbus_en = false;
}

static void max77729_init_opcode
		(struct max77729_usbc_platform_data *usbc_data, int reset)
{
	struct max77729_platform_data *pdata = usbc_data->max77729_data;

	max77729_usbc_disable_auto_vbus(usbc_data);

	if (pdata && pdata->support_audio)
		max77729_usbc_enable_audio(usbc_data);
	if (reset)
		max77729_set_enable_alternate_mode(ALTERNATE_MODE_START | ALTERNATE_MODE_READY);
}

static bool max77729_check_recover_opcode(u8 opcode)
{
	bool ret = false;

	switch (opcode) {
	case OPCODE_SET_ALTERNATEMODE:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}
	return ret;
}

static void max77729_recover_opcode
		(struct max77729_usbc_platform_data *usbc_data, bool opcode_list[])
{
	int i;

	for (i = 0; i < OPCODE_NONE; i++) {
		if (opcode_list[i]) {
			msg_maxim("opcode = 0x%02x", i);
			switch (i) {
			case OPCODE_SET_ALTERNATEMODE:
				max77729_set_enable_alternate_mode
					(usbc_data->set_altmode);
				break;
			default:
				break;
			}
			opcode_list[i] = false;
		}
	}
}

void init_usbc_cmd_data(usbc_cmd_data *cmd_data)
{
	cmd_data->opcode = OPCODE_NONE;
	cmd_data->prev_opcode = OPCODE_NONE;
	cmd_data->response = OPCODE_NONE;
	cmd_data->val = REG_NONE;
	cmd_data->mask = REG_NONE;
	cmd_data->reg = REG_NONE;
	cmd_data->noti_cmd = OPCODE_NOTI_NONE;
	cmd_data->write_length = 0;
	cmd_data->read_length = 0;
	cmd_data->seq = 0;
	cmd_data->is_uvdm = 0;
	memset(cmd_data->write_data, REG_NONE, OPCODE_DATA_LENGTH);
	memset(cmd_data->read_data, REG_NONE, OPCODE_DATA_LENGTH);
}

static void init_usbc_cmd_node(usbc_cmd_node *usbc_cmd_node)
{
	usbc_cmd_data *cmd_data = &(usbc_cmd_node->cmd_data);

	pr_debug("%s:%s\n", "MAX77729", __func__);

	usbc_cmd_node->next = NULL;

	init_usbc_cmd_data(cmd_data);
}

static void copy_usbc_cmd_data(usbc_cmd_data *from, usbc_cmd_data *to)
{
	to->opcode = from->opcode;
	to->response = from->response;
	memcpy(to->read_data, from->read_data, OPCODE_DATA_LENGTH);
	memcpy(to->write_data, from->write_data, OPCODE_DATA_LENGTH);
	to->reg = from->reg;
	to->mask = from->mask;
	to->val = from->val;
	to->seq = from->seq;
	to->read_length = from->read_length;
	to->write_length = from->write_length;
	to->prev_opcode = from->prev_opcode;
	to->is_uvdm = from->is_uvdm;
}

bool is_empty_usbc_cmd_queue(usbc_cmd_queue_t *usbc_cmd_queue)
{
	bool ret = false;

	if (usbc_cmd_queue->front == NULL)
		ret = true;

	if (ret)
		msg_maxim("usbc_cmd_queue Empty(%c)", ret ? 'T' : 'F');

	return ret;
}

void enqueue_usbc_cmd(usbc_cmd_queue_t *usbc_cmd_queue, usbc_cmd_data *cmd_data)
{
	usbc_cmd_node	*temp_node = kzalloc(sizeof(usbc_cmd_node), GFP_KERNEL);

	if (!temp_node) {
		msg_maxim("failed to allocate usbc command queue");
		return;
	}

	init_usbc_cmd_node(temp_node);

	copy_usbc_cmd_data(cmd_data, &(temp_node->cmd_data));

	if (is_empty_usbc_cmd_queue(usbc_cmd_queue)) {
		usbc_cmd_queue->front = temp_node;
		usbc_cmd_queue->rear = temp_node;
	} else {
		usbc_cmd_queue->rear->next = temp_node;
		usbc_cmd_queue->rear = temp_node;
	}

	if (g_usbc_data && g_usbc_data->max77729)
		g_usbc_data->max77729->is_usbc_queue = 1;
}

void enqueue_front_usbc_cmd(usbc_cmd_queue_t *usbc_cmd_queue, usbc_cmd_data *cmd_data)
{
	usbc_cmd_node	*temp_node = kzalloc(sizeof(usbc_cmd_node), GFP_KERNEL);

	if (!temp_node) {
		msg_maxim("failed to allocate usbc command queue");
		return;
	}

	init_usbc_cmd_node(temp_node);

	copy_usbc_cmd_data(cmd_data, &(temp_node->cmd_data));

	if (is_empty_usbc_cmd_queue(usbc_cmd_queue)) {
		usbc_cmd_queue->front = temp_node;
		usbc_cmd_queue->rear = temp_node;
	} else {
		temp_node->next = usbc_cmd_queue->front;
		usbc_cmd_queue->front = temp_node;
	}

	if (g_usbc_data && g_usbc_data->max77729)
		g_usbc_data->max77729->is_usbc_queue = 1;
}

static void dequeue_usbc_cmd
	(usbc_cmd_queue_t *usbc_cmd_queue, usbc_cmd_data *cmd_data)
{
	usbc_cmd_node *temp_node;

	if (is_empty_usbc_cmd_queue(usbc_cmd_queue)) {
		msg_maxim("Queue, Empty!");
		return;
	}

	temp_node = usbc_cmd_queue->front;
	copy_usbc_cmd_data(&(temp_node->cmd_data), cmd_data);

	msg_maxim("Opcode(0x%02x) Response(0x%02x)", cmd_data->opcode, cmd_data->response);

	if (usbc_cmd_queue->front->next == NULL) {
		msg_maxim("front->next = NULL");
		usbc_cmd_queue->front = NULL;
	} else
		usbc_cmd_queue->front = usbc_cmd_queue->front->next;

	if (is_empty_usbc_cmd_queue(usbc_cmd_queue))
		usbc_cmd_queue->rear = NULL;

	kfree(temp_node);
}

static bool front_usbc_cmd
	(usbc_cmd_queue_t *cmd_queue, usbc_cmd_data *cmd_data)
{
	if (is_empty_usbc_cmd_queue(cmd_queue)) {
		msg_maxim("Queue, Empty!");
		return false;
	}

	copy_usbc_cmd_data(&(cmd_queue->front->cmd_data), cmd_data);
	msg_maxim("Opcode(0x%02x)", cmd_data->opcode);
	return true;
}

static bool is_usbc_notifier_opcode(u8 opcode)
{
	bool noti = false;

	return noti;
}

bool check_usbc_opcode_queue(void)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	usbc_cmd_queue_t *cmd_queue = NULL;
	bool ret = true;

	if (usbpd_data == NULL)
		goto err;

	cmd_queue = &(usbpd_data->usbc_cmd_queue);

	if (cmd_queue == NULL)
		goto err;

	ret = is_empty_usbc_cmd_queue(cmd_queue);

err:
	return ret;
}
EXPORT_SYMBOL(check_usbc_opcode_queue);

/*
 * max77729_i2c_opcode_write - SMBus "opcode write" protocol
 * @chip: max77729 platform data
 * @command: OPcode
 * @values: Byte array into which data will be read; big enough to hold
 *	the data returned by the slave.
 *
 * This executes the SMBus "opcode read" protocol, returning negative errno
 * else the number of data bytes in the slave's response.
 */
int max77729_i2c_opcode_write(struct max77729_usbc_platform_data *usbc_data,
		u8 opcode, u8 length, u8 *values)
{
	u8 write_values[OPCODE_MAX_LENGTH] = { 0, };
	int ret = 0;

	if (length > OPCODE_DATA_LENGTH)
		return -EMSGSIZE;

	write_values[0] = opcode;
	if (length)
		memcpy(&write_values[1], values, length);


	msg_maxim("opcode 0x%x, write_length %d",
			opcode, length + OPCODE_SIZE);
	print_hex_dump(KERN_ERR, "max77729: opcode_write: ",
			DUMP_PREFIX_OFFSET, 16, 1, write_values,
			length + OPCODE_SIZE, false);
	/* Write opcode and data */
	ret = max77729_bulk_write(usbc_data->muic, OPCODE_WRITE,
			length + OPCODE_SIZE, write_values);
	/* Write end of data by 0x00 */
	if (length < OPCODE_DATA_LENGTH)
		max77729_write_reg(usbc_data->muic, OPCODE_WRITE_END, 0x00);

	if (opcode == OPCODE_SET_ALTERNATEMODE)
		usbc_data->set_altmode_error = ret;

	if (ret == 0)
		usbc_data->opcode_stamp = jiffies;

	return ret;
}

/**
 * max77729_i2c_opcode_read - SMBus "opcode read" protocol
 * @chip: max77729 platform data
 * @command: OPcode
 * @values: Byte array into which data will be read; big enough to hold
 *	the data returned by the slave.
 *
 * This executes the SMBus "opcode read" protocol, returning negative errno
 * else the number of data bytes in the slave's response.
 */
int max77729_i2c_opcode_read(struct max77729_usbc_platform_data *usbc_data,
		u8 opcode, u8 length, u8 *values)
{
	int size = 0;

	if (length > OPCODE_DATA_LENGTH)
		return -EMSGSIZE;

	/*
	 * We don't need to use opcode to get any feedback
	 */

	/* Read opcode data */
	size = max77729_bulk_read(usbc_data->muic, OPCODE_READ,
			length + OPCODE_SIZE, values);


	msg_maxim("opcode 0x%x, read_length %d, ret_error %d",
			opcode, length + OPCODE_SIZE, size);
	print_hex_dump(KERN_ERR, "max77729: opcode_read: ",
			DUMP_PREFIX_OFFSET, 16, 1, values,
			length + OPCODE_SIZE, false);
	return size;
}

static void max77729_notify_execute(struct max77729_usbc_platform_data *usbc_data,
		const usbc_cmd_data *cmd_data)
{
		/* to do  */
}

static void max77729_handle_update_opcode(struct max77729_usbc_platform_data *usbc_data,
		const usbc_cmd_data *cmd_data, unsigned char *data)
{
	usbc_cmd_data write_data;
	u8 read_value = data[1];
	u8 write_value = (read_value & (~cmd_data->mask)) | (cmd_data->val & cmd_data->mask);
	u8 opcode = cmd_data->response + 1; /* write opcode = read opocde + 1 */

	pr_info("%s: value update [0x%x]->[0x%x] at OPCODE(0x%x)\n", __func__,
			read_value, write_value, opcode);

	init_usbc_cmd_data(&write_data);
	write_data.opcode = opcode;
	write_data.write_length = 1;
	write_data.write_data[0] = write_value;
	write_data.read_length = 0;

	max77729_usbc_opcode_push(usbc_data, &write_data);
}

#define UNKNOWN_VID 0xFFFF
void max77729_send_get_request(struct max77729_usbc_platform_data *usbc_data, unsigned char *data)
{
	enum {
		SENT_REQ_MSG = 0,
		ERR_SNK_RDY = 5,
		ERR_PD20,
		ERR_SNKTXNG,
	};
	SEC_PD_SINK_STATUS *snk_sts = &usbc_data->pd_data->pd_noti.sink_status;

	if (data[1] == SENT_REQ_MSG) {
		max77729_request_response(usbc_data);
	} else { /* ERROR case */
		/* Mark Error in xid */
		snk_sts->xid = (UNKNOWN_VID << 16) | (data[1] << 8);
		msg_maxim("%s, Err : %d", __func__, data[1]);
	}
}

void max77729_handle_qc_result(struct max77729_muic_data *muic_data, unsigned char *data)
{
	int result = data[1];
	union power_supply_propval pvalue ={0,};
	pr_info("%s:%s result:0x%x vbadc:0x%x\n", MUIC_DEV_NAME,
			__func__, data[1], data[2]);

	switch (result) {
	case 0:
		pr_info("%s:%s QC2.0 Success\n", MUIC_DEV_NAME, __func__);
		g_usbc_data->is_hvdcp = true;
		pvalue.intval = POWER_SUPPLY_TYPE_USB_HVDCP;
		psy_do_property("usb", set, POWER_SUPPLY_PROP_REAL_TYPE, pvalue);
		break;
	case 1:
		pr_info("%s:%s No CHGIN\n", MUIC_DEV_NAME, __func__);
		break;
	case 2:
		pr_info("%s:%s Not High Voltage DCP\n",
				MUIC_DEV_NAME, __func__);
		break;
	case 3:
		pr_info("%s:%s Not DCP\n", MUIC_DEV_NAME, __func__);
		break;
	case 6:
		pr_info("%s:%s Vbus is not changed with 3 continuous ping\n",
				MUIC_DEV_NAME, __func__);
		break;
	case 7:
		pr_info("%s:%s Vbus is not changed in 1 sec\n",
				MUIC_DEV_NAME, __func__);
		break;
	default:
		pr_info("%s:%s QC2.0 error(%d)\n", MUIC_DEV_NAME, __func__, result);
		break;
	}
}

static void max77729_irq_execute(struct max77729_usbc_platform_data *usbc_data,
		const usbc_cmd_data *cmd_data)
{
	int len = cmd_data->read_length;
	unsigned char data[OPCODE_DATA_LENGTH] = {0,};
	u8 response = 0xff;
	u8 vdm_opcode_header = 0x0;
	UND_DATA_MSG_VDM_HEADER_Type vdm_header;
	u8 vdm_command = 0x0;
	u8 vdm_type = 0x0;
	u8 vdm_response = 0x0;
	u8 reqd_vdm_command = 0;
	uint8_t W_DATA = 0x0;
	u8 result = 0x0;

	memset(&vdm_header, 0, sizeof(UND_DATA_MSG_VDM_HEADER_Type));
	max77729_i2c_opcode_read(usbc_data, cmd_data->opcode,
			len, data);

	/* opcode identifying the messsage type. (0x51)*/
	response = data[0];

	if (response != cmd_data->response) {
		msg_maxim("Response [0x%02x] != [0x%02x]",
			response, cmd_data->response);
#if !defined (MAX77729_GRL_ENABLE)
		if (cmd_data->response == OPCODE_FW_OPCODE_CLEAR) {
			msg_maxim("Response after FW opcode cleared, just return");
			return;
		}
#endif
	}

	/* to do(read switch case) */
	switch (response) {
	case OPCODE_BCCTRL1_R:
	case OPCODE_BCCTRL2_R:
	case OPCODE_CTRL1_R:
	case OPCODE_CTRL2_R:
	case OPCODE_CTRL3_R:
	case OPCODE_CCCTRL1_R:
	case OPCODE_CCCTRL2_R:
	case OPCODE_CCCTRL3_R:
	case OPCODE_HVCTRL_R:
	case OPCODE_OPCODE_VCONN_ILIM_R:
	case OPCODE_CHGIN_ILIM_R:
	case OPCODE_CHGIN_ILIM2_R:
		if (cmd_data->seq == OPCODE_UPDATE_SEQ)
			max77729_handle_update_opcode(usbc_data, cmd_data, data);
		break;
	case OPCODE_CURRENT_SRCCAP:
		max77729_current_pdo(usbc_data, data);
		break;
	case OPCODE_GET_SRCCAP:
		max77729_pdo_list(usbc_data, data);
		break;
	case OPCODE_READ_RESPONSE_FOR_GET_REQUEST:
		max77729_read_response(usbc_data, data);
		break;
	case OPCODE_SEND_GET_REQUEST:
		result = data[1];
		switch(result){
			case SENT_REQ_MSG:
			    msg_maxim("sucess to send");
				max77729_request_response(usbc_data);
				break;
			case ERR_SNK_RDY:
			    msg_maxim(" Not in Snk Ready");
				break;
			case ERR_PD20:
				msg_maxim(" PD 2.0");
				break;
			case ERR_SNKTXNG:
				msg_maxim(" SinkTxNG");
				break;
			default:
				msg_maxim("OPCODE_SEND_GET_REQUEST = [%x]",result);
				break;
		}
		break;

	case OPCODE_SRCCAP_REQUEST:
		/*
		 * If response of Source_Capablities message is SinkTxNg(0xFE) or Not in Ready State(0xFF)
		 * It means that the message can not be sent to Port Partner.
		 * After Attaching Rp 3.0A, send again the message.
		 */
		if (data[1] == 0xfe || data[1] == 0xff){
			usbc_data->srcccap_request_retry = true;
			pr_info("%s : srcccap_request_retry is set\n", __func__);
		}
		break;
	case OPCODE_SET_SRCCAP:
		if (data[1] == 0xff && usbc_data->source_Ready){
			max77729_send_new_srccap(usbc_data, 0);
		}
		break;
	case 0x65:
		if (data[1] != 0xff && !usbc_data->source_Ready){
			pr_info("%s : harry otg srccap %x %x\n", __func__, data[1], data[2]);
			max77729_current_pdo(usbc_data, data);
		}
		break;
	case OPCODE_APDO_SRCCAP_REQUEST:
		max77729_response_apdo_request(usbc_data, data);
		break;
	case OPCODE_SET_PPS:
		max77729_response_set_pps(usbc_data, data);
		break;
	case OPCODE_READ_MESSAGE:
		pr_info("@TA_ALERT: %s : OPCODE[%x] Data[1] = 0x%x Data[7] = 0x%x Data[9] = 0x%x\n",
			__func__, OPCODE_READ_MESSAGE, data[1], data[7], data[9]);
#if defined(CONFIG_DIRECT_CHARGING)
		if ((data[0] == 0x5D) &&
			/* OCP would be set to Alert or Status message */
			((data[1] == 0x01 && data[7] == 0x04) || (data[1] == 0x02 && (data[9] & 0x02)))) {
			union power_supply_propval value = {0,};
			value.intval = true;
			psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
		}
#endif
		break;
	case OPCODE_VDM_DISCOVER_GET_VDM_RESP:
		max77729_vdm_message_handler(usbc_data, data, len + OPCODE_SIZE);
		break;
	case OPCODE_VDM_DISCOVER_SET_VDM_REQ:
		vdm_opcode_header = data[1];
		switch (vdm_opcode_header) {
		case 0xFF:
			msg_maxim("This isn't invalid response(OPCODE : 0x48, HEADER : 0xFF)");
			break;
		default:
			memcpy(&vdm_header, &data[2], sizeof(vdm_header));
			vdm_type = vdm_header.BITS.VDM_Type;
			vdm_command = vdm_header.BITS.VDM_command;
			vdm_response = vdm_header.BITS.VDM_command_type;
			msg_maxim("vdm_type[%x], vdm_command[%x], vdm_response[%x]",
				vdm_type, vdm_command, vdm_response);
			switch (vdm_type) {
			case STRUCTURED_VDM:
				if ((vdm_response == SEC_UVDM_RESPONDER_ACK) ||
					((vdm_response == SEC_UVDM_RESPONDER_NAK) &&
					(vdm_command == Discover_Identity))) {
					switch (vdm_command) {
					case Discover_Identity:
						msg_maxim("ignore Discover_Identity");
						usbc_data->uvdm_state = USBPD_UVDM_CONNECT;
						break;
					case Discover_SVIDs:
						msg_maxim("ignore Discover_SVIDs");
						break;
					case Discover_Modes:
							msg_maxim("ignore Discover_Modes");
						break;
					case Enter_Mode:
							msg_maxim("ignore Enter_Mode");
						break;
					case Exit_Mode:
						msg_maxim("ignore Exit_Mode");
						break;
					case Attention:
						msg_maxim("ignore Attention");
						break;
					case Configure:
						break;
					default:
						msg_maxim("vdm_command isn't valid[%x]", vdm_command);
						break;
					};
				} else if (vdm_response == SEC_UVDM_ININIATOR) {
					switch (vdm_command) {
					case Attention:
						/* Attention message is not able to be received via 0x48 OPCode */
						/* Check requested vdm command and responded vdm command */
						{
							/* Read requested vdm command */
							max77729_read_reg(usbc_data->muic, 0x23, &reqd_vdm_command);
							reqd_vdm_command &= 0x1F; /* Command bit, b4...0 */

							if (reqd_vdm_command == Configure) {
								W_DATA = 1 << (usbc_data->dp_selected_pin - 1);
								/* Retry Configure message */
								msg_maxim("Retry Configure message, W_DATA = %x, dp_selected_pin = %d",
										W_DATA, usbc_data->dp_selected_pin);
								max77729_vdm_process_set_DP_configure_mode_req(usbc_data, W_DATA);
							}
						}
						break;
					case Discover_Identity:
					case Discover_SVIDs:
					case Discover_Modes:
					case Enter_Mode:
					case Configure:
					default:
						/* Nothing */
						break;
					};
				} else
					msg_maxim("vdm_response is error value[%x]", vdm_response);
				break;
            case SEC_UVDM_UNSTRUCTURED_VDM :
                msg_maxim("adapter_svid : %x", usbc_data->adapter_svid);
                if (usbc_data->adapter_svid == USB_PD_MI_SVID) {
                    msg_maxim("SEC_UVDM_UNSTRUCTURED_VDM !!!");
                    usbpd_mi_vdm_received_cb(usbc_data, data, len+OPCODE_SIZE);
                }
                break;
			default:
				msg_maxim("vdm_type isn't valid error");
				break;
			};
			break;
		};
		break;
	case OPCODE_SET_ALTERNATEMODE:
		usbc_data->max77729->set_altmode_en = 1;
		msg_maxim("set altmode en to 1");
		break;
	case OPCODE_QC_2_0_SET:
		max77729_handle_qc_result(usbc_data->muic_data, data);
		break;

 	case OPCODE_FW_OPCODE_CLEAR:
		msg_maxim("Cleared FW OPCODE");
	default:
		break;
	}
}

void max77729_usbc_dequeue_queue(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data cmd_data;
	usbc_cmd_queue_t *cmd_queue = NULL;

	cmd_queue = &(usbc_data->usbc_cmd_queue);

	init_usbc_cmd_data(&cmd_data);

	if (is_empty_usbc_cmd_queue(cmd_queue)) {
		msg_maxim("Queue, Empty");
		return;
	}

	dequeue_usbc_cmd(cmd_queue, &cmd_data);
	msg_maxim("!! Dequeue queue : opcode : %x, 1st data : %x. 2st data : %x",
		cmd_data.write_data[0],
		cmd_data.read_data[0],
		cmd_data.val);
}

static void max77729_usbc_clear_fw_queue(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data write_data;

	msg_maxim("called");

	init_usbc_cmd_data(&write_data);
	write_data.opcode = OPCODE_FW_OPCODE_CLEAR;
	max77729_usbc_opcode_write(usbc_data, &write_data);
}

void max77729_usbc_clear_queue(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_data cmd_data;
	usbc_cmd_queue_t *cmd_queue = NULL;

	mutex_lock(&usbc_data->op_lock);
	msg_maxim("IN");
	cmd_queue = &(usbc_data->usbc_cmd_queue);

	while (!is_empty_usbc_cmd_queue(cmd_queue)) {
		init_usbc_cmd_data(&cmd_data);
		dequeue_usbc_cmd(cmd_queue, &cmd_data);
		if (max77729_check_recover_opcode(cmd_data.opcode)){
			usbc_data->recover_opcode_list[cmd_data.opcode] = true;
			usbc_data->need_recover = true;
		}
	}
	usbc_data->opcode_stamp = 0;
	msg_maxim("OUT");
	mutex_unlock(&usbc_data->op_lock);
	/* also clear fw opcode queue to sync with driver */
	max77729_usbc_clear_fw_queue(usbc_data);
}

static void max77729_usbc_cmd_run(struct max77729_usbc_platform_data *usbc_data)
{
	usbc_cmd_queue_t *cmd_queue = NULL;
	usbc_cmd_node *run_node;
	usbc_cmd_data cmd_data;
	int ret = 0;

	cmd_queue = &(usbc_data->usbc_cmd_queue);


	run_node = kzalloc(sizeof(usbc_cmd_node), GFP_KERNEL);
	if (!run_node) {
		msg_maxim("failed to allocate muic command queue");
		return;
	}

	init_usbc_cmd_node(run_node);

	init_usbc_cmd_data(&cmd_data);

	if (is_empty_usbc_cmd_queue(cmd_queue)) {
		msg_maxim("Queue, Empty");
		kfree(run_node);
		return;
	}

	dequeue_usbc_cmd(cmd_queue, &cmd_data);

	if (is_usbc_notifier_opcode(cmd_data.opcode)) {
		max77729_notify_execute(usbc_data, &cmd_data);
		max77729_usbc_cmd_run(usbc_data);
	} else if (cmd_data.opcode == OPCODE_NONE) {/* Apcmdres isr */
		msg_maxim("Apcmdres ISR !!!");
		max77729_irq_execute(usbc_data, &cmd_data);
		usbc_data->opcode_stamp = 0;
		max77729_usbc_cmd_run(usbc_data);
	} else { /* No ISR */
		msg_maxim("No ISR");
		copy_usbc_cmd_data(&cmd_data, &(usbc_data->last_opcode));
		ret = max77729_i2c_opcode_write(usbc_data, cmd_data.opcode,
				cmd_data.write_length, cmd_data.write_data);
		if (ret < 0) {
			msg_maxim("i2c write fail. dequeue opcode");
			max77729_usbc_dequeue_queue(usbc_data);
		}
	}
	kfree(run_node);
}

void max77729_usbc_opcode_write(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *write_op)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	usbc_cmd_data execute_cmd_data;
	usbc_cmd_data current_cmd;

	mutex_lock(&usbc_data->op_lock);
	init_usbc_cmd_data(&current_cmd);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = write_op->opcode;
	execute_cmd_data.write_length = write_op->write_length;
	execute_cmd_data.is_uvdm = write_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, write_op->write_data, OPCODE_DATA_LENGTH);
	execute_cmd_data.seq = OPCODE_WRITE_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = write_op->opcode;
	execute_cmd_data.read_length = write_op->read_length;
	execute_cmd_data.is_uvdm = write_op->is_uvdm;
	execute_cmd_data.seq = OPCODE_WRITE_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	msg_maxim("W->W opcode[0x%02x] write_length[%d] read_length[%d]",
		write_op->opcode, write_op->write_length, write_op->read_length);

	front_usbc_cmd(cmd_queue, &current_cmd);
	if (current_cmd.opcode == write_op->opcode)
		max77729_usbc_cmd_run(usbc_data);
	else {
		msg_maxim("!!!current_cmd.opcode [0x%02x][0x%02x], read_op->opcode[0x%02x]",
			current_cmd.opcode, current_cmd.response, write_op->opcode);
		if (usbc_data->opcode_stamp != 0 && current_cmd.opcode == OPCODE_NONE) {
			if (time_after(jiffies,
					usbc_data->opcode_stamp + MAX77729_MAX_APDCMD_TIME)) {
				usbc_data->opcode_stamp = 0;
				msg_maxim("error. we will dequeue response data");
				max77729_usbc_dequeue_queue(usbc_data);
				max77729_usbc_cmd_run(usbc_data);
			}
		}
	}
	mutex_unlock(&usbc_data->op_lock);
}


void max77729_usbc_opcode_write_immediately(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *write_op)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	usbc_cmd_data execute_cmd_data;
	usbc_cmd_data current_cmd;
	int wait_response_flag = 0;

	mutex_lock(&usbc_data->op_lock);
	init_usbc_cmd_data(&current_cmd);


	if(front_usbc_cmd(cmd_queue, &current_cmd))
	{
		if (usbc_data->opcode_stamp != 0 && current_cmd.opcode == OPCODE_NONE)
		{
			usbc_data->opcode_stamp = 0;
			msg_maxim("before enqueue to front, dequeue response data");
			max77729_usbc_dequeue_queue(usbc_data);
			wait_response_flag = 1;
		}
	}

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = write_op->opcode;
	execute_cmd_data.read_length = write_op->read_length;
	execute_cmd_data.is_uvdm = write_op->is_uvdm;
	execute_cmd_data.seq = OPCODE_WRITE_SEQ;
	enqueue_front_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = write_op->opcode;
	execute_cmd_data.write_length = write_op->write_length;
	execute_cmd_data.is_uvdm = write_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, write_op->write_data, OPCODE_DATA_LENGTH);
	execute_cmd_data.seq = OPCODE_WRITE_SEQ;
	enqueue_front_usbc_cmd(cmd_queue, &execute_cmd_data);

	msg_maxim("W->W opcode[0x%02x] write_length[%d] read_length[%d]",
		write_op->opcode, write_op->write_length, write_op->read_length);

	/* add back the dequeue response opcode. */
	if(1 == wait_response_flag)
	{
		msg_maxim("wait_response_flag = 1, add back the response opcode to the front of the queue, don't send opcode 0x37 immediately");
		enqueue_front_usbc_cmd(cmd_queue, &current_cmd);
	} else {
		max77729_usbc_cmd_run(usbc_data);
		msg_maxim("wait_response_flag = 0, no response opcode in queue, send opcode 0x37 immediately");
	}


	mutex_unlock(&usbc_data->op_lock);
}


void max77729_usbc_opcode_read(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *read_op)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	usbc_cmd_data execute_cmd_data;
	usbc_cmd_data current_cmd;

	mutex_lock(&usbc_data->op_lock);
	init_usbc_cmd_data(&current_cmd);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = read_op->opcode;
	execute_cmd_data.write_length = read_op->write_length;
	execute_cmd_data.is_uvdm = read_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, read_op->write_data, read_op->write_length);
	execute_cmd_data.seq = OPCODE_READ_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = read_op->opcode;
	execute_cmd_data.read_length = read_op->read_length;
	execute_cmd_data.is_uvdm = read_op->is_uvdm;
	execute_cmd_data.seq = OPCODE_READ_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	msg_maxim("R->R opcode[0x%02x] write_length[%d] read_length[%d]",
		read_op->opcode, read_op->write_length, read_op->read_length);

	front_usbc_cmd(cmd_queue, &current_cmd);
	if (current_cmd.opcode == read_op->opcode)
		max77729_usbc_cmd_run(usbc_data);
	else {
		msg_maxim("!!!current_cmd.opcode [0x%02x][0x%02x], read_op->opcode[0x%02x]",
			current_cmd.opcode, current_cmd.response, read_op->opcode);
		if (usbc_data->opcode_stamp != 0 && current_cmd.opcode == OPCODE_NONE) {
			if (time_after(jiffies,
					usbc_data->opcode_stamp + MAX77729_MAX_APDCMD_TIME)) {
				usbc_data->opcode_stamp = 0;
				msg_maxim("error. we will dequeue response data");
				max77729_usbc_dequeue_queue(usbc_data);
				max77729_usbc_cmd_run(usbc_data);
			}
		}
	}

	mutex_unlock(&usbc_data->op_lock);
}

void max77729_usbc_opcode_update(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *update_op)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	usbc_cmd_data execute_cmd_data;
	usbc_cmd_data current_cmd;

	switch (update_op->opcode) {
	case OPCODE_BCCTRL1_R:
	case OPCODE_BCCTRL2_R:
	case OPCODE_CTRL1_R:
	case OPCODE_CTRL2_R:
	case OPCODE_CTRL3_R:
	case OPCODE_CCCTRL1_R:
	case OPCODE_CCCTRL2_R:
	case OPCODE_CCCTRL3_R:
	case OPCODE_HVCTRL_R:
	case OPCODE_OPCODE_VCONN_ILIM_R:
	case OPCODE_CHGIN_ILIM_R:
	case OPCODE_CHGIN_ILIM2_R:
		break;
	default:
		pr_err("%s: invalid usage(0x%x), return\n", __func__, update_op->opcode);
		return;
	}

	mutex_lock(&usbc_data->op_lock);
	init_usbc_cmd_data(&current_cmd);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = update_op->opcode;
	execute_cmd_data.write_length = 0;
	execute_cmd_data.is_uvdm = update_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, update_op->write_data, update_op->write_length);
	execute_cmd_data.seq = OPCODE_UPDATE_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = update_op->opcode;
	execute_cmd_data.read_length = 1;
	execute_cmd_data.seq = OPCODE_UPDATE_SEQ;
	execute_cmd_data.val = update_op->val;
	execute_cmd_data.mask = update_op->mask;
	execute_cmd_data.is_uvdm = update_op->is_uvdm;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	msg_maxim("U->U opcode[0x%02x] write_length[%d] read_length[%d]",
		update_op->opcode, update_op->write_length, update_op->read_length);

	front_usbc_cmd(cmd_queue, &current_cmd);
	if (current_cmd.opcode == update_op->opcode)
		max77729_usbc_cmd_run(usbc_data);
	else {
		msg_maxim("!!! current_cmd.opcode [0x%02x], update_op->opcode[0x%02x]",
			current_cmd.opcode, update_op->opcode);
		if (usbc_data->opcode_stamp != 0 && current_cmd.opcode == OPCODE_NONE) {
			if (time_after(jiffies,
					usbc_data->opcode_stamp + MAX77729_MAX_APDCMD_TIME)) {
				usbc_data->opcode_stamp = 0;
				msg_maxim("error. we will dequeue response data");
				max77729_usbc_dequeue_queue(usbc_data);
				max77729_usbc_cmd_run(usbc_data);
			}
		}
	}

	mutex_unlock(&usbc_data->op_lock);
}

void max77729_usbc_opcode_push(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *read_op)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	usbc_cmd_data execute_cmd_data;
	usbc_cmd_data current_cmd;

	init_usbc_cmd_data(&current_cmd);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = read_op->opcode;
	execute_cmd_data.write_length = read_op->write_length;
	execute_cmd_data.is_uvdm = read_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, read_op->write_data, read_op->write_length);
	execute_cmd_data.seq = OPCODE_PUSH_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = read_op->opcode;
	execute_cmd_data.read_length = read_op->read_length;
	execute_cmd_data.is_uvdm = read_op->is_uvdm;
	execute_cmd_data.seq = OPCODE_PUSH_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	msg_maxim("P->P opcode[0x%02x] write_length[%d] read_length[%d]",
		read_op->opcode, read_op->write_length, read_op->read_length);
}

void max77729_usbc_opcode_rw(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *read_op, usbc_cmd_data *write_op)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	usbc_cmd_data execute_cmd_data;
	usbc_cmd_data current_cmd;

	mutex_lock(&usbc_data->op_lock);
	init_usbc_cmd_data(&current_cmd);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = read_op->opcode;
	execute_cmd_data.write_length = read_op->write_length;
	execute_cmd_data.is_uvdm = read_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, read_op->write_data, read_op->write_length);
	execute_cmd_data.seq = OPCODE_RW_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = read_op->opcode;
	execute_cmd_data.read_length = read_op->read_length;
	execute_cmd_data.is_uvdm = read_op->is_uvdm;
	execute_cmd_data.seq = OPCODE_RW_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages sent to USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.opcode = write_op->opcode;
	execute_cmd_data.write_length = write_op->write_length;
	execute_cmd_data.is_uvdm = write_op->is_uvdm;
	memcpy(execute_cmd_data.write_data, write_op->write_data, OPCODE_DATA_LENGTH);
	execute_cmd_data.seq = OPCODE_RW_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	/* the messages recevied From USBC. */
	init_usbc_cmd_data(&execute_cmd_data);
	execute_cmd_data.response = write_op->opcode;
	execute_cmd_data.read_length = write_op->read_length;
	execute_cmd_data.is_uvdm = write_op->is_uvdm;
	execute_cmd_data.seq = OPCODE_RW_SEQ;
	enqueue_usbc_cmd(cmd_queue, &execute_cmd_data);

	msg_maxim("RW->R opcode[0x%02x] write_length[%d] read_length[%d]",
		read_op->opcode, read_op->write_length, read_op->read_length);
	msg_maxim("RW->W opcode[0x%02x] write_length[%d] read_length[%d]",
		write_op->opcode, write_op->write_length, write_op->read_length);

	front_usbc_cmd(cmd_queue, &current_cmd);
	if (current_cmd.opcode == read_op->opcode)
		max77729_usbc_cmd_run(usbc_data);
	else {
		msg_maxim("!!! current_cmd.opcode [0x%02x], read_op->opcode[0x%02x]",
			current_cmd.opcode, read_op->opcode);
		if (usbc_data->opcode_stamp != 0 && current_cmd.opcode == OPCODE_NONE) {
			if (time_after(jiffies,
					usbc_data->opcode_stamp + MAX77729_MAX_APDCMD_TIME)) {
				usbc_data->opcode_stamp = 0;
				msg_maxim("error. we will dequeue response data");
				max77729_usbc_dequeue_queue(usbc_data);
				max77729_usbc_cmd_run(usbc_data);
			}
		}
	}

	mutex_unlock(&usbc_data->op_lock);
}


static void max77729_reset_ic(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_dev *max77729 = usbc_data->max77729;

	//gurantee to block i2c trasaction during ccic reset
	mutex_lock(&max77729->i2c_lock);
	max77729_write_reg_nolock(usbc_data->muic, 0x80, 0x0F);
	msleep(100); /* need 100ms delay */
	mutex_unlock(&max77729->i2c_lock);
}

void max77729_usbc_check_sysmsg(struct max77729_usbc_platform_data *usbc_data, u8 sysmsg)
{
	usbc_cmd_queue_t *cmd_queue = &(usbc_data->usbc_cmd_queue);
	bool is_empty_queue = is_empty_usbc_cmd_queue(cmd_queue);
	usbc_cmd_data cmd_data;
	usbc_cmd_data next_cmd_data;
	u8 next_opcode = 0xFF;
	u8 interrupt;

	int ret = 0;

	if (usbc_data->shut_down) {
		msg_maxim("IGNORE SYSTEM_MSG IN SHUTDOWN MODE!!");
		return;
	}

	switch (sysmsg) {
	case SYSERROR_NONE:
		break;
	case SYSERROR_BOOT_WDT:
		usbc_data->watchdog_count++;
		msg_maxim("SYSERROR_BOOT_WDT: %d", usbc_data->watchdog_count);
		max77729_usbc_mask_irq(usbc_data);
		max77729_write_reg(usbc_data->muic, REG_UIC_INT_M, REG_UIC_INT_M_INIT);
		max77729_write_reg(usbc_data->muic, REG_CC_INT_M, REG_CC_INT_M_INIT);
		max77729_write_reg(usbc_data->muic, REG_PD_INT_M, REG_PD_INT_M_INIT);
		max77729_write_reg(usbc_data->muic, REG_VDM_INT_M, REG_VDM_INT_M_INIT);
		/* clear UIC_INT to prevent infinite sysmsg irq*/
		g_usbc_data->max77729->enable_nested_irq = 1;
		max77729_read_reg(usbc_data->muic, MAX77729_USBC_REG_UIC_INT, &interrupt);
		g_usbc_data->max77729->usbc_irq = interrupt & 0xBF; //clear the USBC SYSTEM IRQ
		max77729_usbc_clear_queue(usbc_data);
		usbc_data->is_first_booting = 1;
		max77729_init_opcode(usbc_data, 1);
		max77729_usbc_umask_irq(usbc_data);
		break;
	case SYSERROR_BOOT_SWRSTREQ:
		break;
	case SYSMSG_BOOT_POR:
		usbc_data->por_count++;
		max77729_usbc_mask_irq(usbc_data);
		max77729_reset_ic(usbc_data);
		max77729_write_reg(usbc_data->muic, REG_UIC_INT_M, REG_UIC_INT_M_INIT);
		max77729_write_reg(usbc_data->muic, REG_CC_INT_M, REG_CC_INT_M_INIT);
		max77729_write_reg(usbc_data->muic, REG_PD_INT_M, REG_PD_INT_M_INIT);
		max77729_write_reg(usbc_data->muic, REG_VDM_INT_M, REG_VDM_INT_M_INIT);
		/* clear UIC_INT to prevent infinite sysmsg irq*/
	        g_usbc_data->max77729->enable_nested_irq = 1;
		max77729_read_reg(usbc_data->muic, MAX77729_USBC_REG_UIC_INT, &interrupt);
		g_usbc_data->max77729->usbc_irq = interrupt & 0xBF; //clear the USBC SYSTEM IRQ
		msg_maxim("SYSERROR_BOOT_POR: %d, UIC_INT:0x%02x", usbc_data->por_count, interrupt);
		max77729_usbc_clear_queue(usbc_data);
		usbc_data->is_first_booting = 1;
		max77729_init_opcode(usbc_data, 1);
		max77729_usbc_umask_irq(usbc_data);
		break;
	case SYSERROR_APCMD_UNKNOWN:
		break;
	case SYSERROR_APCMD_INPROGRESS:
		break;
	case SYSERROR_APCMD_FAIL:

		init_usbc_cmd_data(&cmd_data);
		init_usbc_cmd_data(&next_cmd_data);

		if (front_usbc_cmd(cmd_queue, &next_cmd_data))
			next_opcode = next_cmd_data.response;

		if (!is_empty_queue) {
			copy_usbc_cmd_data(&(usbc_data->last_opcode), &cmd_data);

 			if (next_opcode == OPCODE_VDM_DISCOVER_SET_VDM_REQ) {
				usbc_data->opcode_stamp = 0;
				max77729_usbc_dequeue_queue(usbc_data);
				cmd_data.opcode = OPCODE_NONE;
			}

			if ((cmd_data.opcode != OPCODE_NONE) && (cmd_data.opcode == next_opcode)) {
				if (next_opcode != OPCODE_VDM_DISCOVER_SET_VDM_REQ) {
					ret = max77729_i2c_opcode_write(usbc_data,
						cmd_data.opcode,
						cmd_data.write_length,
						cmd_data.write_data);
					if (ret) {
						msg_maxim("i2c write fail. dequeue opcode");
						max77729_usbc_dequeue_queue(usbc_data);
					} else
						msg_maxim("RETRY SUCCESS : %x, %x", cmd_data.opcode, next_opcode);
				} else
					msg_maxim("IGNORE COMMAND : %x, %x", cmd_data.opcode, next_opcode);
			} else {
				msg_maxim("RETRY FAILED : %x, %x", cmd_data.opcode, next_opcode);
			}

		}

		break;
	default:
		break;
	}
}


static irqreturn_t max77729_apcmd_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	u8 sysmsg = 0;

	msg_maxim("IRQ(%d)_IN", irq);
	max77729_read_reg(usbc_data->muic, REG_USBC_STATUS2, &usbc_data->usbc_status2);
	sysmsg = usbc_data->usbc_status2;
	msg_maxim(" [IN] sysmsg : %d", sysmsg);

	mutex_lock(&usbc_data->op_lock);
	max77729_usbc_cmd_run(usbc_data);
	mutex_unlock(&usbc_data->op_lock);

	if (usbc_data->need_recover) {
		max77729_recover_opcode(usbc_data,
			usbc_data->recover_opcode_list);
		usbc_data->need_recover = false;
	}

	msg_maxim("IRQ(%d)_OUT", irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77729_sysmsg_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	u8 sysmsg = 0;
	u8 i = 0;
	u8 raw_data[3] = {0, };
	u8 usbc_status2 = 0;
	u8 dump_reg[10] = {0, };

	for (i = 0; i < 3; i++) {
		usbc_status2 = 0;
		max77729_read_reg(usbc_data->muic, REG_USBC_STATUS2, &usbc_status2);
		raw_data[i] = usbc_status2;
	}
	if((raw_data[0] == raw_data[1]) && (raw_data[0] == raw_data[2])){
		sysmsg = raw_data[0];
	} else {
		max77729_bulk_read(usbc_data->muic, REG_USBC_STATUS1,
				8, dump_reg);
		msg_maxim("[ERROR ]sys_reg, %x, %x, %x", raw_data[0], raw_data[1],raw_data[2]);
		msg_maxim("[ERROR ]dump_reg, %x, %x, %x, %x, %x, %x, %x, %x\n", dump_reg[0], dump_reg[1],
			dump_reg[2], dump_reg[3], dump_reg[4], dump_reg[5], dump_reg[6], dump_reg[7]);
		sysmsg = 0x6D;
	}
	msg_maxim("IRQ(%d)_IN sysmsg: %x", irq, sysmsg);
	max77729_usbc_check_sysmsg(usbc_data, sysmsg);
	usbc_data->sysmsg = sysmsg;
	msg_maxim("IRQ(%d)_OUT sysmsg: %x", irq, sysmsg);

	return IRQ_HANDLED;
}


static irqreturn_t max77729_vdm_identity_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_Discover_ID = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77729_vdm_svids_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_Discover_SVIDs = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vdm_discover_mode_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_Discover_MODEs = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vdm_enter_mode_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_Enter_Mode = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vdm_dp_status_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_DP_Status_Update = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vdm_dp_configure_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_DP_Configure = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vdm_attention_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	MAX77729_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;

	memset(&VDM_MSG_IRQ_State, 0, sizeof(VDM_MSG_IRQ_State));
	msg_maxim("IRQ(%d)_IN", irq);
	VDM_MSG_IRQ_State.BITS.Vdm_Flag_Attention = 1;
	max77729_receive_alternate_message(usbc_data, &VDM_MSG_IRQ_State);
	msg_maxim("IRQ(%d)_OUT", irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vir_altmode_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;

	msg_maxim("max77729_vir_altmode_irq");

	if (usbc_data->shut_down) {
		msg_maxim("%s doing shutdown. skip set alternate mode", __func__);
		goto skip;
	}

	max77729_set_enable_alternate_mode
		(usbc_data->set_altmode);

skip:
	return IRQ_HANDLED;
}

int max77729_init_irq_handler(struct max77729_usbc_platform_data *usbc_data)
{
	int ret = 0;
	usbc_data->irq_apcmd = usbc_data->irq_base + MAX77729_USBC_IRQ_APC_INT;
	if (usbc_data->irq_apcmd) {
		ret = request_threaded_irq(usbc_data->irq_apcmd,
			   NULL, max77729_apcmd_irq,
			   0,
			   "usbc-apcmd-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_sysmsg = usbc_data->irq_base + MAX77729_USBC_IRQ_SYSM_INT;
	if (usbc_data->irq_sysmsg) {
		ret = request_threaded_irq(usbc_data->irq_sysmsg,
			   NULL, max77729_sysmsg_irq,
			   0,
			   "usbc-sysmsg-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm0 = usbc_data->irq_base + MAX77729_IRQ_VDM_DISCOVER_ID_INT;
	if (usbc_data->irq_vdm0) {
		ret = request_threaded_irq(usbc_data->irq_vdm0,
			   NULL, max77729_vdm_identity_irq,
			   0,
			   "usbc-vdm0-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm1 = usbc_data->irq_base + MAX77729_IRQ_VDM_DISCOVER_SVIDS_INT;
	if (usbc_data->irq_vdm1) {
		ret = request_threaded_irq(usbc_data->irq_vdm1,
			   NULL, max77729_vdm_svids_irq,
			   0,
			   "usbc-vdm1-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm2 = usbc_data->irq_base + MAX77729_IRQ_VDM_DISCOVER_MODES_INT;
	if (usbc_data->irq_vdm2) {
		ret = request_threaded_irq(usbc_data->irq_vdm2,
			   NULL, max77729_vdm_discover_mode_irq,
			   0,
			   "usbc-vdm2-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm3 = usbc_data->irq_base + MAX77729_IRQ_VDM_ENTER_MODE_INT;
	if (usbc_data->irq_vdm3) {
		ret = request_threaded_irq(usbc_data->irq_vdm3,
			   NULL, max77729_vdm_enter_mode_irq,
			   0,
			   "usbc-vdm3-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm4 = usbc_data->irq_base + MAX77729_IRQ_VDM_DP_STATUS_UPDATE_INT;
	if (usbc_data->irq_vdm4) {
		ret = request_threaded_irq(usbc_data->irq_vdm4,
			   NULL, max77729_vdm_dp_status_irq,
			   0,
			   "usbc-vdm4-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm5 = usbc_data->irq_base + MAX77729_IRQ_VDM_DP_CONFIGURE_INT;
	if (usbc_data->irq_vdm5) {
		ret = request_threaded_irq(usbc_data->irq_vdm5,
			   NULL, max77729_vdm_dp_configure_irq,
			   0,
			   "usbc-vdm5-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vdm6 = usbc_data->irq_base + MAX77729_IRQ_VDM_ATTENTION_INT;
	if (usbc_data->irq_vdm6) {
		ret = request_threaded_irq(usbc_data->irq_vdm6,
			   NULL, max77729_vdm_attention_irq,
			   0,
			   "usbc-vdm6-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	usbc_data->irq_vir0 = usbc_data->irq_base + MAX77729_VIR_IRQ_ALTERROR_INT;
	if (usbc_data->irq_vir0) {
		ret = request_threaded_irq(usbc_data->irq_vir0,
			   NULL, max77729_vir_altmode_irq,
			   0,
			   "usbc-vir0-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			return ret;
		}
	}

	return ret;
}

static void max77729_usbc_umask_irq(struct max77729_usbc_platform_data *usbc_data)
{
	int ret = 0;
	u8 i2c_data = 0;
	/* Unmask max77729 interrupt */
	ret = max77729_read_reg(usbc_data->i2c, 0x23,
			  &i2c_data);
	if (ret) {
		pr_err("%s fail to read muic reg\n", __func__);
		return;
	}

	i2c_data &= ~((1 << 3));	/* Unmask muic interrupt */
	max77729_write_reg(usbc_data->i2c, 0x23,
			   i2c_data);
}
static void max77729_usbc_mask_irq(struct max77729_usbc_platform_data *usbc_data)
{
	int ret = 0;
	u8 i2c_data = 0;
	/* Unmask max77729 interrupt */
	ret = max77729_read_reg(usbc_data->i2c, 0x23,
			  &i2c_data);
	if (ret) {
		pr_err("%s fail to read muic reg\n", __func__);
		return;
	}

	i2c_data |= ((1 << 3));	/* Unmask muic interrupt */
	max77729_write_reg(usbc_data->i2c, 0x23,
			   i2c_data);
}

#if 0 //Brandon Need to optimaze based on customer kernel if customer want to use VDM
static int pdic_handle_usb_external_notifier_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;
	int ret = 0;
	int enable = *(int *)data;

	pr_info("%s : action=%lu , enable=%d\n", __func__, action, enable);
	switch (action) {
	case EXTERNAL_NOTIFY_HOSTBLOCK_PRE:
		if (enable) {
			max77729_set_enable_alternate_mode(ALTERNATE_MODE_STOP);
			if (usbpd_data->dp_is_connect)
				max77729_dp_detach(usbpd_data);
		} else {
			if (usbpd_data->dp_is_connect)
				max77729_dp_detach(usbpd_data);
		}
		break;
	case EXTERNAL_NOTIFY_HOSTBLOCK_POST:
		if (enable) {
		} else {
			max77729_set_enable_alternate_mode(ALTERNATE_MODE_START);
		}
		break;
	case EXTERNAL_NOTIFY_DEVICEADD:
		if (enable) {
			usbpd_data->device_add = 1;
			wake_up_interruptible(&usbpd_data->device_add_wait_q);
		}
		break;
	case EXTERNAL_NOTIFY_MDMBLOCK_PRE:
		if (enable && usbpd_data->dp_is_connect) {
			usbpd_data->mdm_block = 1;
			max77729_dp_detach(usbpd_data);
		}
		break;
	default:
		break;
	}

	return ret;
}

static void delayed_external_notifier_init(struct work_struct *work)
{
	int ret = 0;
	static int retry_count = 1;
	int max_retry_count = 5;
	struct max77729_usbc_platform_data *usbpd_data = g_usbc_data;

	pr_info("%s : %d = times!\n", __func__, retry_count);

	/* Register ccic handler to ccic notifier block list */
	ret = usb_external_notify_register(&usbpd_data->usb_external_notifier_nb,
		pdic_handle_usb_external_notifier_notification, EXTERNAL_NOTIFY_DEV_PDIC);
	if (ret < 0) {
		pr_err("Manager notifier init time is %d.\n", retry_count);
		if (retry_count++ != max_retry_count)
			schedule_delayed_work(&usbpd_data->usb_external_notifier_register_work, msecs_to_jiffies(2000));
		else
			pr_err("fail to init external notifier\n");
	} else
		pr_info("%s : external notifier register done!\n", __func__);
}
#endif

static int max77729_port_type_set(const struct typec_capability *cap, enum typec_port_type port_type)
{
	struct max77729_usbc_platform_data *usbpd_data = container_of(cap, struct max77729_usbc_platform_data, typec_cap);

	if (!usbpd_data)
		return -EINVAL;

	msg_maxim("typec_power_role=%d, typec_data_role=%d, port_type=%d",
		usbpd_data->typec_power_role, usbpd_data->typec_data_role, port_type);

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (port_type == TYPEC_PORT_SRC) {
		msg_maxim("try reversing, from UFP(Sink) to DFP(Source)");
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		max77729_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else if (port_type == TYPEC_PORT_SNK) {
		msg_maxim("try reversing, from DFP(Source) to UFP(Sink)");

		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		max77729_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else {
		msg_maxim("invalid typec_role");
		return 0;
	}

	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 * max_adapter_device_register - Register a adapter_device
 * @parent: Parent device
 * @cap: Description of the port
 *
 * Registers a device for adapter_device described in @cap.
 *
 * Returns handle to the port on success or ERR_PTR on failure.
 */
struct max_adapter_device *max_adapter_device_register(struct device *parent)
{
	struct max_adapter_device *adapter_dev = NULL;
	int ret;

	dev_err(parent, "max_adapter_device_register\n");
	adapter_dev = kzalloc(sizeof(*adapter_dev), GFP_KERNEL);
	if (!adapter_dev)
		return ERR_PTR(-ENOMEM);

	adapter_dev->dev.class = max_adapter_class;
	adapter_dev->dev.parent = parent;
	dev_set_name(&adapter_dev->dev, "%s", "pd_adapter");

	ret = device_register(&adapter_dev->dev);
	if (ret) {
		dev_err(parent, "failed to register cable (%d)\n", ret);
		kfree(adapter_dev);
		return ERR_PTR(ret);
	}

	return adapter_dev;
}
EXPORT_SYMBOL_GPL(max_adapter_device_register);

/**
 * max_adapter_device_unregister - Unregister a  max_adapter_device
 * @port: The  max_adapter_device to be unregistered
 *
 * Unregister device created with max_adapter_device_register().
 */
void max_adapter_device_unregister(struct max_adapter_device *adapter_dev)
{
	if (!adapter_dev)
		return;

	device_unregister(&adapter_dev->dev);
}
EXPORT_SYMBOL_GPL(max_adapter_device_unregister);

static int max77729_usbc_probe(struct platform_device *pdev)
{
	struct max77729_dev *max77729 = dev_get_drvdata(pdev->dev.parent);
	struct max77729_platform_data *pdata = dev_get_platdata(max77729->dev);
	struct max77729_usbc_platform_data *usbc_data = NULL;
	int ret;

	msg_maxim("Probing : %d", max77729->irq);
	usbc_data =  kzalloc(sizeof(struct max77729_usbc_platform_data), GFP_KERNEL);
	if (!usbc_data)
		return -ENOMEM;

	max77729->check_usbc_opcode_queue = check_usbc_opcode_queue;
	usbc_data->dev = pdev->dev.parent;
	usbc_data->max77729 = max77729;
	usbc_data->muic = max77729->muic;
	usbc_data->charger = max77729->charger;
	usbc_data->i2c = max77729->i2c;
	usbc_data->max77729_data = pdata;
	usbc_data->irq_base = pdata->irq_base;

	usbc_data->pd_data = kzalloc(sizeof(struct max77729_pd_data), GFP_KERNEL);
	if (!usbc_data->pd_data)
		return -ENOMEM;

	usbc_data->cc_data = kzalloc(sizeof(struct max77729_cc_data), GFP_KERNEL);
	if (!usbc_data->cc_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, usbc_data);

	usbc_data->HW_Revision = 0x0;
	usbc_data->FW_Revision = 0x0;
	usbc_data->plug_attach_done = 0x0;
	usbc_data->cc_data->current_pr = 0xFF;
	usbc_data->pd_data->current_dr = 0xFF;
	usbc_data->cc_data->current_vcon = 0xFF;
	usbc_data->op_code_done = 0x0;
	usbc_data->usbc_cmd_queue.front = NULL;
	usbc_data->usbc_cmd_queue.rear = NULL;
	usbc_data->opcode_stamp = 0;
	mutex_init(&usbc_data->op_lock);
	usbc_data->vconn_en = 1;
	usbc_data->cc_pin_status = NO_DETERMINATION;

	/* pd->typec_caps.type = TYPEC_PORT_DRP; */
	/* pd->typec_caps.data = TYPEC_PORT_DRD; */
	/* pd->typec_caps.revision = 0x0130; */
	/* pd->typec_caps.pd_revision = 0x0300; */
	/* pd->typec_caps.dr_set = usbpd_typec_dr_set; */
	/* pd->typec_caps.pr_set = usbpd_typec_pr_set; */
	/* pd->typec_caps.port_type_set = usbpd_typec_port_type_set; */
	/* pd->partner_desc.identity = &pd->partner_identity; */

 	usbc_data->typec_cap.revision = 0x0130;
	usbc_data->typec_cap.pd_revision = 0x300;
	usbc_data->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;

	usbc_data->typec_cap.pr_set = max77729_pr_set;
	usbc_data->typec_cap.dr_set = max77729_dr_set;
	usbc_data->typec_cap.port_type_set = max77729_port_type_set;

	usbc_data->typec_cap.type = TYPEC_PORT_DRP;
	usbc_data->typec_cap.data = TYPEC_PORT_DRD;

	usbc_data->typec_power_role = TYPEC_SINK;
	usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
	usbc_data->typec_try_pps_enable = TRY_PPS_NONE;

	usbc_data->port = typec_register_port(usbc_data->dev, &usbc_data->typec_cap);
	if (IS_ERR(usbc_data->port))
		pr_err("unable to register typec_register_port\n");
	else
		msg_maxim("success typec_register_port port=%pK", usbc_data->port);

	max_adapter_class = class_create(THIS_MODULE, "Charging_Adapter");
	if (IS_ERR(max_adapter_class)) {
		return PTR_ERR(max_adapter_class);
	}
	max_adapter_class->dev_groups = max77729_groups;
	usbc_data->adapter_dev = max_adapter_device_register(usbc_data->dev);
	if (IS_ERR_OR_NULL(usbc_data->adapter_dev)) {
		ret = PTR_ERR(usbc_data->adapter_dev);
		goto err_register_max_adapter_dev;
	}

	init_completion(&usbc_data->typec_reverse_completion);

	usbc_data->auto_vbus_en = false;
	usbc_data->is_first_booting = 1;
	usbc_data->pd_support = false;
	usbc_data->ccrp_state = 0;
	usbc_data->set_altmode = 0;
	usbc_data->set_altmode_error = 0;
	usbc_data->need_recover = false;
	usbc_data->op_ctrl1_w = (BIT_CCSrcSnk | BIT_CCSnkSrc | BIT_CCDetEn);
	usbc_data->srcccap_request_retry = false;
	init_completion(&usbc_data->op_completion);
	init_completion(&usbc_data->ccic_sysfs_completion);
	init_completion(&usbc_data->psrdy_wait);
	init_completion(&usbc_data->pps_in_wait);
	init_completion(&usbc_data->uvdm_longpacket_out_wait);
	INIT_WORK(&usbc_data->fw_update_work,
			max77729_firmware_update_sysfs_work);


	g_usbc_data = usbc_data;
 	/*
	 * associate extcon with the parent dev as it could have a DT
	 * node which will be useful for extcon_get_edev_by_phandle()
	 */
	usbc_data->extcon = devm_extcon_dev_allocate(usbc_data->dev, extcon_cable);
	if (IS_ERR(usbc_data->extcon)) {
		ret = PTR_ERR(usbc_data->extcon);
		/* goto put_psy; */
	}

	ret = devm_extcon_dev_register(usbc_data->dev, usbc_data->extcon);
	if (ret) {
		/* goto put_psy; */
	}

	/* Support reporting polarity and speed via properties */
	extcon_set_property_capability(usbc_data->extcon, EXTCON_USB,
			EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(usbc_data->extcon, EXTCON_USB,
			EXTCON_PROP_USB_SS);
	extcon_set_property_capability(usbc_data->extcon, EXTCON_USB,
			EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT);
	extcon_set_property_capability(usbc_data->extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(usbc_data->extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_SS);

	max77729_get_version_info(usbc_data);
	max77729_init_irq_handler(usbc_data);
	max77729_bc12_probe(usbc_data);
	max77729_cc_init(usbc_data);
	max77729_pd_init(usbc_data);
	max77729_write_reg(usbc_data->muic, REG_PD_INT_M, 0x1C);
	max77729_write_reg(usbc_data->muic, REG_VDM_INT_M, 0xFF);
	max77729_init_opcode(usbc_data, 1);                   //harry change it for alt mode enable
	INIT_DELAYED_WORK(&usbc_data->vbus_hard_reset_work,
				vbus_control_hard_reset);
	/* turn on the VBUS automatically. */
	max77729->cc_booting_complete = 1;
	max77729_usbc_umask_irq(usbc_data);
	init_waitqueue_head(&usbc_data->host_turn_on_wait_q);
	init_waitqueue_head(&usbc_data->device_add_wait_q);
	usbc_data->host_turn_on_wait_time = 3;

	usbc_data->cc_open_req = 1;

//	schedule_delayed_work(&usbc_data->fw_update_work,
//			msecs_to_jiffies(10000));
	msg_maxim("probing Complete..");

	return 0;

err_register_max_adapter_dev:
	max_adapter_device_unregister(usbc_data->adapter_dev);

	return ret;
}

static int max77729_usbc_remove(struct platform_device *pdev)
{
	struct max77729_usbc_platform_data *usbc_data = platform_get_drvdata(pdev);

	class_destroy(max_adapter_class);
	kfree(usbc_data->hmd_list);
	usbc_data->hmd_list = NULL;
	mutex_destroy(&usbc_data->hmd_power_lock);
	mutex_destroy(&usbc_data->op_lock);
	free_irq(usbc_data->irq_apcmd, usbc_data);
	free_irq(usbc_data->irq_sysmsg, usbc_data);
	free_irq(usbc_data->irq_vdm0, usbc_data);
	free_irq(usbc_data->irq_vdm1, usbc_data);
	free_irq(usbc_data->irq_vdm2, usbc_data);
	free_irq(usbc_data->irq_vdm3, usbc_data);
	free_irq(usbc_data->irq_vdm4, usbc_data);
	free_irq(usbc_data->irq_vdm5, usbc_data);
	free_irq(usbc_data->irq_vdm6, usbc_data);
	free_irq(usbc_data->irq_vdm7, usbc_data);
	free_irq(usbc_data->pd_data->irq_pdmsg, usbc_data);
	free_irq(usbc_data->pd_data->irq_datarole, usbc_data);
	free_irq(usbc_data->pd_data->irq_ssacc, usbc_data);
	free_irq(usbc_data->pd_data->irq_fct_id, usbc_data);
	free_irq(usbc_data->cc_data->irq_vconncop, usbc_data);
	free_irq(usbc_data->cc_data->irq_vsafe0v, usbc_data);
	free_irq(usbc_data->cc_data->irq_detabrt, usbc_data);
	free_irq(usbc_data->cc_data->irq_vconnsc, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccpinstat, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccistat, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccvcnstat, usbc_data);
	free_irq(usbc_data->cc_data->irq_ccstat, usbc_data);
	kfree(usbc_data->cc_data);
	kfree(usbc_data->pd_data);
	kfree(usbc_data);
	return 0;
}

#if defined CONFIG_PM
static int max77729_usbc_suspend(struct device *dev)
{
	struct max77729_usbc_platform_data *usbc_data =
		dev_get_drvdata(dev);

	max77729_muic_suspend(usbc_data);

	return 0;
}

static int max77729_usbc_resume(struct device *dev)
{
	struct max77729_usbc_platform_data *usbc_data =
		dev_get_drvdata(dev);

	max77729_muic_resume(usbc_data);
	if (usbc_data->set_altmode_error) {
		msg_maxim("set alternate mode");
		max77729_set_enable_alternate_mode
			(usbc_data->set_altmode);
	}

	return 0;
}
#else
#define max77729_usbc_suspend NULL
#define max77729_usbc_resume NULL
#endif

static void max77729_usbc_disable_irq(struct max77729_usbc_platform_data *usbc_data)
{
	disable_irq(usbc_data->irq_apcmd);
	disable_irq(usbc_data->irq_sysmsg);
	disable_irq(usbc_data->irq_vdm0);
	disable_irq(usbc_data->irq_vdm1);
	disable_irq(usbc_data->irq_vdm2);
	disable_irq(usbc_data->irq_vdm3);
	disable_irq(usbc_data->irq_vdm4);
	disable_irq(usbc_data->irq_vdm5);
	disable_irq(usbc_data->irq_vdm6);
	disable_irq(usbc_data->irq_vir0);
	disable_irq(usbc_data->pd_data->irq_pdmsg);
	disable_irq(usbc_data->pd_data->irq_psrdy);
	disable_irq(usbc_data->pd_data->irq_datarole);
	disable_irq(usbc_data->pd_data->irq_ssacc);
	disable_irq(usbc_data->pd_data->irq_fct_id);
	disable_irq(usbc_data->cc_data->irq_vconncop);
	disable_irq(usbc_data->cc_data->irq_vsafe0v);
	disable_irq(usbc_data->cc_data->irq_vconnsc);
	disable_irq(usbc_data->cc_data->irq_ccpinstat);
	disable_irq(usbc_data->cc_data->irq_ccistat);
	disable_irq(usbc_data->cc_data->irq_ccvcnstat);
	disable_irq(usbc_data->cc_data->irq_ccstat);
}

static void max77729_usbc_shutdown(struct platform_device *pdev)
{
	struct max77729_usbc_platform_data *usbc_data =
		platform_get_drvdata(pdev);
	/* struct device_node *np; */
	/* int gpio_dp_sw_oe; */
	u8 uic_int = 0;
	u8 uid = 0;

	msg_maxim("max77729 usbc driver shutdown++++");
	if (!usbc_data->muic) {
		msg_maxim("no max77729 i2c client");
		return;
	}
	usbc_data->shut_down = 1;
	max77729_usbc_mask_irq(usbc_data);
	/* unmask */
	max77729_write_reg(usbc_data->muic, REG_PD_INT_M, 0xFF);
	max77729_write_reg(usbc_data->muic, REG_CC_INT_M, 0xFF);
	max77729_write_reg(usbc_data->muic, REG_UIC_INT_M, 0xFF);
	max77729_write_reg(usbc_data->muic, REG_VDM_INT_M, 0xFF);

	max77729_usbc_disable_irq(usbc_data);

	max77729_read_reg(usbc_data->muic, REG_USBC_STATUS1, &uid);
	uid = (uid & BIT_UIDADC) >> FFS(BIT_UIDADC);
    /* send the reset command */

	max77729_reset_ic(usbc_data);
	max77729_write_reg(usbc_data->muic, REG_PD_INT_M, 0xFF);
	max77729_write_reg(usbc_data->muic, REG_CC_INT_M, 0xFF);
	max77729_write_reg(usbc_data->muic, REG_UIC_INT_M, 0xFF);
	max77729_write_reg(usbc_data->muic, REG_VDM_INT_M, 0xFF);
	max77729_read_reg(usbc_data->muic,
			MAX77729_USBC_REG_UIC_INT, &uic_int);
	msg_maxim("max77729 usbc driver shutdown----");
}

static SIMPLE_DEV_PM_OPS(max77729_usbc_pm_ops, max77729_usbc_suspend,
			 max77729_usbc_resume);

static struct platform_driver max77729_usbc_driver = {
	.driver = {
		.name = "max77729-usbc",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max77729_usbc_pm_ops,
#endif
	},
	.shutdown = max77729_usbc_shutdown,
	.probe = max77729_usbc_probe,
	.remove = max77729_usbc_remove,
};

static int __init max77729_usbc_init(void)
{
	msg_maxim("init");
	return platform_driver_register(&max77729_usbc_driver);
}
device_initcall(max77729_usbc_init);

static void __exit max77729_usbc_exit(void)
{
	platform_driver_unregister(&max77729_usbc_driver);
}
module_exit(max77729_usbc_exit);

MODULE_DESCRIPTION("max77729 USBPD driver");
MODULE_LICENSE("GPL");
