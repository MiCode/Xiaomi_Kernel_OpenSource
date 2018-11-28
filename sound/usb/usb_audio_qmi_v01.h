/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef USB_QMI_V01_H
#define USB_QMI_V01_H

#define UAUDIO_STREAM_SERVICE_ID_V01 0x41D
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

enum usb_audio_stream_status_enum_v01 {
	USB_AUDIO_STREAM_STATUS_ENUM_MIN_VAL_V01 = INT_MIN,
	USB_AUDIO_STREAM_REQ_SUCCESS_V01 = 0,
	USB_AUDIO_STREAM_REQ_FAILURE_V01 = 1,
	USB_AUDIO_STREAM_REQ_FAILURE_NOT_FOUND_V01 = 2,
	USB_AUDIO_STREAM_REQ_FAILURE_INVALID_PARAM_V01 = 3,
	USB_AUDIO_STREAM_REQ_FAILURE_MEMALLOC_V01 = 4,
	USB_AUDIO_STREAM_STATUS_ENUM_MAX_VAL_V01 = INT_MAX,
};

enum usb_audio_device_indication_enum_v01 {
	USB_AUDIO_DEVICE_INDICATION_ENUM_MIN_VAL_V01 = INT_MIN,
	USB_AUDIO_DEV_CONNECT_V01 = 0,
	USB_AUDIO_DEV_DISCONNECT_V01 = 1,
	USB_AUDIO_DEV_SUSPEND_V01 = 2,
	USB_AUDIO_DEV_RESUME_V01 = 3,
	USB_AUDIO_DEVICE_INDICATION_ENUM_MAX_VAL_V01 = INT_MAX,
};

enum usb_audio_device_speed_enum_v01 {
	USB_AUDIO_DEVICE_SPEED_ENUM_MIN_VAL_V01 = INT_MIN,
	USB_AUDIO_DEVICE_SPEED_INVALID_V01 = 0,
	USB_AUDIO_DEVICE_SPEED_LOW_V01 = 1,
	USB_AUDIO_DEVICE_SPEED_FULL_V01 = 2,
	USB_AUDIO_DEVICE_SPEED_HIGH_V01 = 3,
	USB_AUDIO_DEVICE_SPEED_SUPER_V01 = 4,
	USB_AUDIO_DEVICE_SPEED_SUPER_PLUS_V01 = 5,
	USB_AUDIO_DEVICE_SPEED_ENUM_MAX_VAL_V01 = INT_MAX,
};

struct qmi_uaudio_stream_req_msg_v01 {
	uint8_t enable;
	uint32_t usb_token;
	uint8_t audio_format_valid;
	uint32_t audio_format;
	uint8_t number_of_ch_valid;
	uint32_t number_of_ch;
	uint8_t bit_rate_valid;
	uint32_t bit_rate;
	uint8_t xfer_buff_size_valid;
	uint32_t xfer_buff_size;
	uint8_t service_interval_valid;
	uint32_t service_interval;
};
#define QMI_UAUDIO_STREAM_REQ_MSG_V01_MAX_MSG_LEN 46
extern struct qmi_elem_info qmi_uaudio_stream_req_msg_v01_ei[];

struct qmi_uaudio_stream_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t status_valid;
	enum usb_audio_stream_status_enum_v01 status;
	uint8_t internal_status_valid;
	uint32_t internal_status;
	uint8_t slot_id_valid;
	uint32_t slot_id;
	uint8_t usb_token_valid;
	uint32_t usb_token;
	uint8_t std_as_opr_intf_desc_valid;
	struct usb_interface_descriptor_v01 std_as_opr_intf_desc;
	uint8_t std_as_data_ep_desc_valid;
	struct usb_endpoint_descriptor_v01 std_as_data_ep_desc;
	uint8_t std_as_sync_ep_desc_valid;
	struct usb_endpoint_descriptor_v01 std_as_sync_ep_desc;
	uint8_t usb_audio_spec_revision_valid;
	uint16_t usb_audio_spec_revision;
	uint8_t data_path_delay_valid;
	uint8_t data_path_delay;
	uint8_t usb_audio_subslot_size_valid;
	uint8_t usb_audio_subslot_size;
	uint8_t xhci_mem_info_valid;
	struct apps_mem_info_v01 xhci_mem_info;
	uint8_t interrupter_num_valid;
	uint8_t interrupter_num;
	uint8_t speed_info_valid;
	enum usb_audio_device_speed_enum_v01 speed_info;
	uint8_t controller_num_valid;
	uint8_t controller_num;
};
#define QMI_UAUDIO_STREAM_RESP_MSG_V01_MAX_MSG_LEN 202
extern struct qmi_elem_info qmi_uaudio_stream_resp_msg_v01_ei[];

struct qmi_uaudio_stream_ind_msg_v01 {
	enum usb_audio_device_indication_enum_v01 dev_event;
	uint32_t slot_id;
	uint8_t usb_token_valid;
	uint32_t usb_token;
	uint8_t std_as_opr_intf_desc_valid;
	struct usb_interface_descriptor_v01 std_as_opr_intf_desc;
	uint8_t std_as_data_ep_desc_valid;
	struct usb_endpoint_descriptor_v01 std_as_data_ep_desc;
	uint8_t std_as_sync_ep_desc_valid;
	struct usb_endpoint_descriptor_v01 std_as_sync_ep_desc;
	uint8_t usb_audio_spec_revision_valid;
	uint16_t usb_audio_spec_revision;
	uint8_t data_path_delay_valid;
	uint8_t data_path_delay;
	uint8_t usb_audio_subslot_size_valid;
	uint8_t usb_audio_subslot_size;
	uint8_t xhci_mem_info_valid;
	struct apps_mem_info_v01 xhci_mem_info;
	uint8_t interrupter_num_valid;
	uint8_t interrupter_num;
	uint8_t controller_num_valid;
	uint8_t controller_num;
};
#define QMI_UAUDIO_STREAM_IND_MSG_V01_MAX_MSG_LEN 181
extern struct qmi_elem_info qmi_uaudio_stream_ind_msg_v01_ei[];

#endif
