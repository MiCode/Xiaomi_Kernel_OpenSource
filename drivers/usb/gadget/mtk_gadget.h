/*
 * Copyright (C) 2015 MediaTek Inc.
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
#ifndef MTK_GADGET_H
#define MTK_GADGET_H
#include "rndis.h"

extern bool usb_cable_connected(void);
extern void composite_setup_complete(struct usb_ep *ep, struct usb_request *req);

#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
typedef enum {
	DIRECT_STATE_NONE = 0,
	DIRECT_STATE_ENABLING,
	DIRECT_STATE_DEACTIVATED,
	DIRECT_STATE_ACTIVATING,
	DIRECT_STATE_ACTIVATED,
	DIRECT_STATE_DEACTIVATING,
	DIRECT_STATE_DISABLING
} direct_state_enum;

typedef enum _ufpm_func_mode {
	UFPM_FUNC_MODE_TETHER = 0,
	UFPM_FUNC_MODE_LOG,
	UFPM_FUNC_MODE_MAX_NUM
} ufpm_func_mode_e;

typedef enum _ufpm_bus_state {
	UFPM_BUS_STATE_SUSPEND = 0,
	UFPM_BUS_STATE_RESUME,
	UFPM_BUS_STATE_RESET,
	UFPM_BUS_STATE_NONE
} ufpm_bus_state_e;

typedef enum _usbc_usb_speed {
	USBC_USB_SPEED_MIN   = 0,
	USBC_USB_SPEED_USB11 = 1,
	USBC_USB_SPEED_USB20 = 2,
	USBC_USB_SPEED_USB30 = 3,
	USBC_USB_SPEED_MAX   = 4
} usbc_usb_speed_e;

#define USB_MAP_TYPE_ENDPOINT	0x40
#define USB_MAP_TYPE_INTERFACE	0x60
#define ENDPOINT_USED			0x80
#define ENDPOINT_UNUSED			0x00
#define ENDPOINT_FIFO_SINGLE	0x00
#define ENDPOINT_FIFO_DOUBLE	0x10
#define ENDPOINT_CONTROL		0x00
#define ENDPOINT_ISOCHRONOUS	0x04
#define ENDPOINT_BULK			0x08
#define ENDPOINT_INTERRUPT		0x0C
#define ENDPOINT_DIR_IN			0x00
#define ENDPOINT_DIR_OUT		0x01

typedef struct _ufpm_usb_mapping_t {
	u8		type;
	u8		map;
	u16		maxPktSize;					/* Max. packet size of the endpoint */
	u32		queue_config;
} __packed ufpm_usb_mapping_t;

typedef struct _hifusb_setup_packet {
	 u8           bmRequestType;
	 u8           bRequest;
	 u16          wValue;
	 u16          wIndex;
	 u16          wLength;
} __packed hifusb_setup_packet_t;

typedef struct _tethering_net_stats {
	u64 rx_packets;                /* Total packets received */
	u64 tx_packets;                /* Total packets transmitted */
	u64 rx_errors;                 /* Bad packets received */
	u64 tx_errors;                 /* Packet transmit problems */
	u64 rx_dropped;                /* No space in system */
	u64 tx_dropped;                /* No space available in system */
	u64 rx_frame_errors;           /* Recvˇd frame alignment error */
} __packed tethering_net_stats_t;

typedef struct _tethering_meta_info {
	u32 init_msg_max_transfer_size;
	u32 init_cmplt_max_packets_per_transfer;
	u32 init_cmplt_max_transfer_size;
	u32 init_cmplt_packet_alignment_factor;
	u8 host_mac_addr[6];
	u8 reserved_a[2];
	u8 device_mac_addr[6];
	u8 reserved_b[2];
	tethering_net_stats_t net_stats;
} __packed tethering_activate_meta_info_t;

typedef struct _tethering_deactivate_meta_info_t {
	tethering_net_stats_t net_stats;
} __packed tethering_deactivate_meta_info_t;

typedef struct _logging_activate_meta_info_t {
	u8 reserved1[8];
} __packed logging_activate_meta_info_t;

typedef struct _logging_deactivate_meta_info_t {
	u8 reserved1[8];
} __packed logging_deactivate_meta_info_t;

typedef struct _ufpm_notify_md_bus_event_req {
	u8 state;        /* ufpm_bus_state_e */
	u8  reserved[3];
} __packed ufpm_notify_md_bus_event_req_t;

typedef struct _ufpm_send_md_ep0_msg {
	u8 mode;          /* ufpm_func_mode_e */
	u8 reserved[3];
	u8 pBuffer[1024]; /* hifusb_setup_packet_t */
} __packed ufpm_send_md_ep0_msg_t;

typedef struct _ufpm_send_ap_ep0_msg {
	u8 mode;         /* ufpm_func_mode_e */
	u8  reserved[3];
	u32 ep0_data_len;
	u8 ep0Buffer[1024];
	u32 int_data_len;
	u8 intBuffer[64];
} __packed ufpm_send_ap_ep0_msg_t;

typedef struct _ufpm_mpu_info {
	u8 apUsbDomainId;		/* AP USB MPU domain ID */
	u8 mdCldmaDomainId;		/* MD CCMNI MPU domain ID */
	u8 reserved[6];
	u64 memBank0BaseAddr;	/* Memory bank0 base address */
	u64 memBank0Size;		/* Memory bank0 size */
	u64 memBank4BaseAddr;	/* Memory bank4 base address */
	u64 memBank4Size;		/* Memory bank4 size */
} __packed ufpm_mpu_info_t;

typedef struct _ufpm_enable_md_func_req {
	u8 mode;                /* ufpm_func_mode_e */
	u8 reserved[3];
	ufpm_mpu_info_t mpuInfo;
} __packed ufpm_enable_md_func_req_t;

typedef struct _ufpm_activate_md_func_req {
	u8 mode;								/* ufpm_func_mode_e */
	u8 address;								/* USB address */
	u8 configuration;						/* USB configuration number */
	u8 speed;								/* usbc_usb_speed_e */
	ufpm_usb_mapping_t ap_usb_map[8];		/* AP released IF/EP */
	tethering_activate_meta_info_t tethering_meta_info;		/* tethering meta data */
	logging_activate_meta_info_t logging_meta_info;			/* logging meta data */
} __packed ufpm_activate_md_func_req_t;

typedef struct _ufpm_deactivate_md_func_rsp {
	u8 mode;			/* ufpm_func_mode_e */
	u8 result;			/* bool */
	u8 reserved[2];
	tethering_deactivate_meta_info_t tethering_meta_info;   /* tethering meta data */
	logging_deactivate_meta_info_t logging_meta_info;          /* logging meta data */
} __packed ufpm_deactivate_md_func_rsp_t;

typedef struct _ufpm_md_fast_path_common_req {
	u8 mode;        /* ufpm_func_mode_e */
	u8 reserved[3];
} __packed ufpm_md_fast_path_common_req_t;

typedef struct _ufpm_md_fast_path_common_rsp {
	u8 mode;          /* ufpm_func_mode_e */
	u8 result;	         /* bool */
	u8 reserved[2];
} __packed ufpm_md_fast_path_common_rsp_t;

typedef enum {
	RNDIS_NETWORK_TYPE_NONE = 0,
	RNDIS_NETWORK_TYPE_MOBILE,
	RNDIS_NETWORK_TYPE_NON_MOBILE
} rndis_network_type_enum;

extern int rndis_get_direct_tethering_state(struct usb_function *f);
extern rndis_resp_t *rndis_add_md_response(struct rndis_params *params,
					u32 length);
extern void rndis_get_pkt_info(struct rndis_params *params,
		u32 *maxPacketsPerTransfer, u32 *maxTransferSize);
extern void rndis_get_net_stats(struct rndis_params *params, tethering_net_stats_t *net_stats);
extern void rndis_set_net_stats(struct rndis_params *params, tethering_net_stats_t *net_stats);
#endif

#endif
