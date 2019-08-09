/*
 * mddp_usb_def.h -- Data structure of MD USB module.
 *
 * Copyright (C) 2018 MediaTek Inc.
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
#ifndef __MDDP_USB_DEF_H
#define __MDDP_USB_DEF_H

enum ufpm_func_mode_e {
	UFPM_FUNC_MODE_TETHER = 0,
	UFPM_FUNC_MODE_LOG,
	UFPM_FUNC_MODE_MAX_NUM
};

enum ufpm_bus_state_e {
	UFPM_BUS_STATE_SUSPEND = 0,
	UFPM_BUS_STATE_RESUME,
	UFPM_BUS_STATE_RESET,
	UFPM_BUS_STATE_NONE
};

enum usbc_usb_speed_e {
	USBC_USB_SPEED_MIN   = 0,
	USBC_USB_SPEED_USB11 = 1,
	USBC_USB_SPEED_USB20 = 2,
	USBC_USB_SPEED_USB30 = 3,
	USBC_USB_SPEED_MAX   = 4
};

struct ufpm_usb_mapping_t {
	u8		type;
	u8		map;
	u16		maxPktSize;    /* Max. packet size of the endpoint */
	u32		queue_config;
} __packed;

struct hifusb_setup_packet_t {
	 u8           bmRequestType;
	 u8           bRequest;
	 u16          wValue;
	 u16          wIndex;
	 u16          wLength;
} __packed;

struct tethering_net_stats_t {
	u64 rx_packets;        /* Total packets received */
	u64 tx_packets;        /* Total packets transmitted */
	u64 rx_errors;         /* Bad packets received */
	u64 tx_errors;         /* Packet transmit problems */
	u64 rx_dropped;        /* No space in system */
	u64 tx_dropped;        /* No space available in system */
	u64 rx_frame_errors;   /* Recvˇd frame alignment error */
} __packed;

struct tethering_activate_meta_info_t {
	u32 init_msg_max_transfer_size;
	u32 init_cmplt_max_packets_per_transfer;
	u32 init_cmplt_max_transfer_size;
	u32 init_cmplt_packet_alignment_factor;
	u8 host_mac_addr[6];
	u8 reserved_a[2];
	u8 device_mac_addr[6];
	u8 reserved_b[2];
	struct tethering_net_stats_t net_stats;
} __packed;

struct tethering_deactivate_meta_info_t {
	struct tethering_net_stats_t net_stats;
} __packed;

struct logging_activate_meta_info_t {
	u8 reserved1[8];
} __packed;

struct logging_deactivate_meta_info_t {
	u8 reserved1[8];
} __packed;

struct _ufpm_notify_md_bus_event_req_t {
	u8 state;        /* ufpm_bus_state_e */
	u8  reserved[3];
} __packed;

struct ufpm_send_md_ep0_msg_t {
	u8 mode;          /* ufpm_func_mode_e */
	u8 reserved[3];
	u8 pBuffer[1024]; /* hifusb_setup_packet_t */
} __packed;

struct ufpm_send_ap_ep0_msg_t {
	u8 mode;         /* ufpm_func_mode_e */
	u8  reserved[3];
	u32 ep0_data_len;
	u8 ep0Buffer[1024];
	u32 int_data_len;
	u8 intBuffer[64];
} __packed;

struct ufpm_mpu_info_t {
	u8 apUsbDomainId;		/* AP USB MPU domain ID */
	u8 mdCldmaDomainId;		/* MD CCMNI MPU domain ID */
	u8 reserved[6];
	u64 memBank0BaseAddr;	/* Memory bank0 base address */
	u64 memBank0Size;		/* Memory bank0 size */
	u64 memBank4BaseAddr;	/* Memory bank4 base address */
	u64 memBank4Size;		/* Memory bank4 size */
} __packed;

struct ufpm_enable_md_func_req_t {
	u8 mode;                /* ufpm_func_mode_e */
	u8 version;
	u8 reserved[2];
	struct ufpm_mpu_info_t mpuInfo;
} __packed;

struct ufpm_enable_md_func_rsp_t {
	u8 mode;                /* ufpm_func_mode_e */
	u8 result;
	u8 version;
	u8 reserved;
} __packed;

struct ufpm_activate_md_func_req_t {
	u8 mode;
	u8 address;
	u8 configuration;
	u8 speed;
	struct ufpm_usb_mapping_t ap_usb_map[8];
	struct tethering_activate_meta_info_t tethering_meta_info;
	struct logging_activate_meta_info_t logging_meta_info;
} __packed;

struct ufpm_deactivate_md_func_rsp_t {
	u8 mode;
	u8 result;
	u8 reserved[2];
	struct tethering_deactivate_meta_info_t tethering_meta_info;
	struct logging_deactivate_meta_info_t logging_meta_info;
} __packed;

struct ufpm_md_fast_path_common_req_t {
	u8 mode;        /* ufpm_func_mode_e */
	u8 reserved[3];
} __packed;

struct ufpm_md_fast_path_common_rsp_t {
	u8 mode;          /* ufpm_func_mode_e */
	u8 result;	         /* bool */
	u8 reserved[2];
} __packed;

#ifdef CONFIG_MTK_MD_DIRECT_LOGGING_SUPPORT
#define USB_ACM_IOCTL_CMD_DIRECT_ON			0x01
#define USB_ACM_IOCTL_CMD_DIRECT_OFF		0x02
#define USB_ACM_IOCTL_CMD_QUERY_STATE		0x03
#define USB_ACM_IOCTL_CMD_QUERY_TX_EMPTY	0x04

enum usb_acm_ioctl_direct_state_e {
	USB_ACM_IOCTL_DIRECT_ACTIVATING = 1,
	USB_ACM_IOCTL_DIRECT_ACTIVATED,
	USB_ACM_IOCTL_DIRECT_DEACTIVATING,
	USB_ACM_IOCTL_DIRECT_DEACTIVATED
};

enum usb_acm_ioctl_tx_status_e {
	USB_ACM_IOCTL_TX_EMPTY = 1,
	USB_ACM_IOCTL_TX_NON_EMPTY
};

extern void acm_enable_direct_feature(struct usb_function *f);
extern bool acm_activate_direct_logging(struct usb_function *f, bool suspend);
extern int acm_deactivate_direct_logging(struct usb_function *f);
extern int acm_set_direct_logging(struct usb_function *f, bool direct);
extern int acm_get_direct_logging_state(struct usb_function *f);
extern u8 acm_get_direct_state(struct usb_function *f);
extern bool acm_check_tx_fifo_empty(struct usb_function *f);
#endif

#endif /* __MDDP_USB_DEF_H */
