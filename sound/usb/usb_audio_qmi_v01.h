/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#ifndef USB_QMI_V01_H
#define USB_QMI_V01_H

#define UAUDIO_STREAM_SERVICE_ID_V01 0x41C
#define UAUDIO_STREAM_SERVICE_VERS_V01 0x01

#define QMI_UAUDIO_STREAM_RESP_V01 0x0001
#define QMI_UAUDIO_STREAM_REQ_V01 0x0001
#define QMI_UADUIO_STREAM_IND_V01 0x0001


struct mem_info_v01 {
	uint64_t va;
	uint64_t pa;
	uint32_t size;
};

struct apps_mem_info_v01 {
	struct mem_info_v01 evt_ring;
	struct mem_info_v01 tr_data;
	struct mem_info_v01 tr_sync;
	struct mem_info_v01 xfer_buff;
	struct mem_info_v01 dcba;
};

struct usb_endpoint_descriptor_v01 {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
	uint8_t bRefresh;
	uint8_t bSynchAddress;
};

struct usb_interface_descriptor_v01 {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
};

struct qmi_uaudio_stream_req_msg_v01 {
	uint32_t priv_data;
	uint8_t enable;
	uint32_t usb_token;
	uint32_t audio_format;
	uint32_t number_of_ch;
	uint32_t bit_rate;
	uint32_t xfer_buff_size;
};
#define QMI_UAUDIO_STREAM_REQ_MSG_V01_MAX_MSG_LEN 46
extern struct elem_info qmi_uaudio_stream_req_msg_v01_ei[];

struct qmi_uaudio_stream_resp_msg_v01 {
	uint32_t priv_data;
	uint32_t status;
	uint32_t slot_id;
	uint8_t bSubslotSize;
	struct usb_interface_descriptor_v01 std_as_opr_intf_desc;
	struct usb_endpoint_descriptor_v01 std_as_data_ep_desc;
	struct usb_endpoint_descriptor_v01 std_as_sync_ep_desc;
	uint8_t bDelay;
	uint16_t bcdADC;
	struct apps_mem_info_v01 xhci_mem_info;
	uint8_t interrupter_num;
};
#define QMI_UAUDIO_STREAM_RESP_MSG_V01_MAX_MSG_LEN 177
extern struct elem_info qmi_uaudio_stream_resp_msg_v01_ei[];

struct qmi_uaudio_stream_ind_msg_v01 {
	uint32_t usb_token;
	uint32_t priv_data;
	uint32_t status;
	uint32_t slot_id;
	uint8_t bSubslotSize;
	struct usb_interface_descriptor_v01 std_as_opr_intf_desc;
	struct usb_endpoint_descriptor_v01 std_as_data_ep_desc;
	struct usb_endpoint_descriptor_v01 std_as_sync_ep_desc;
	struct apps_mem_info_v01 xhci_mem_info;
};
#define QMI_UAUDIO_STREAM_IND_MSG_V01_MAX_MSG_LEN 171
extern struct elem_info qmi_uaudio_stream_ind_msg_v01_ei[];

#endif
