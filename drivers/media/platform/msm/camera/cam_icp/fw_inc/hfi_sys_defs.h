/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef _HFI_DEFS_H_
#define _HFI_DEFS_H_

#include <linux/types.h>

/*
 * Following base acts as common starting points
 * for all enumerations.
 */
#define HFI_COMMON_BASE                 0x0

/* HFI Domain base offset for commands and messages */
#define HFI_DOMAIN_SHFT                 (24)
#define HFI_DOMAIN_BMSK                 (0x7 << HFI_DOMAIN_SHFT)
#define HFI_DOMAIN_BASE_ICP             (0x0 << HFI_DOMAIN_SHFT)
#define HFI_DOMAIN_BASE_IPE_BPS         (0x1 << HFI_DOMAIN_SHFT)
#define HFI_DOMAIN_BASE_CDM             (0x2 << HFI_DOMAIN_SHFT)
#define HFI_DOMAIN_BASE_DBG             (0x3 << HFI_DOMAIN_SHFT)

/* Command base offset for commands */
#define HFI_CMD_START_OFFSET            0x10000

/* Command base offset for messages */
#define HFI_MSG_START_OFFSET            0x20000

/* System Level Error types */
#define HFI_ERR_SYS_NONE                (HFI_COMMON_BASE)
#define HFI_ERR_SYS_FATAL               (HFI_COMMON_BASE + 0x1)
#define HFI_ERR_SYS_VERSION_MISMATCH    (HFI_COMMON_BASE + 0x2)
#define HFI_ERR_SYS_UNSUPPORTED_DOMAIN  (HFI_COMMON_BASE + 0x3)
#define HFI_ERR_SYS_UNSUPPORT_CMD       (HFI_COMMON_BASE + 0x4)
#define HFI_ERR_SYS_CMDFAILED           (HFI_COMMON_BASE + 0x5)
#define HFI_ERR_SYS_CMDSIZE             (HFI_COMMON_BASE + 0x6)

/* System Level Event types */
#define HFI_EVENT_SYS_ERROR             (HFI_COMMON_BASE + 0x1)
#define HFI_EVENT_ICP_ERROR             (HFI_COMMON_BASE + 0x2)
#define HFI_EVENT_IPE_BPS_ERROR         (HFI_COMMON_BASE + 0x3)
#define HFI_EVENT_CDM_ERROR             (HFI_COMMON_BASE + 0x4)
#define HFI_EVENT_DBG_ERROR             (HFI_COMMON_BASE + 0x5)

/* Core level start Ranges for errors */
#define HFI_ERR_ICP_START               (HFI_COMMON_BASE + 0x64)
#define HFI_ERR_IPE_BPS_START           (HFI_ERR_ICP_START + 0x64)
#define HFI_ERR_CDM_START               (HFI_ERR_IPE_BPS_START + 0x64)
#define HFI_ERR_DBG_START               (HFI_ERR_CDM_START + 0x64)

/*ICP Core level  error messages */
#define HFI_ERR_NO_RES                  (HFI_ERR_ICP_START + 0x1)
#define HFI_ERR_UNSUPPORTED_RES         (HFI_ERR_ICP_START + 0x2)
#define HFI_ERR_UNSUPPORTED_PROP        (HFI_ERR_ICP_START + 0x3)
#define HFI_ERR_INIT_EXPECTED           (HFI_ERR_ICP_START + 0x4)
#define HFI_ERR_INIT_IGNORED            (HFI_ERR_ICP_START + 0x5)

/* System level commands */
#define HFI_CMD_COMMON_START \
		(HFI_DOMAIN_BASE_ICP + HFI_CMD_START_OFFSET + 0x0)
#define HFI_CMD_SYS_INIT               (HFI_CMD_COMMON_START + 0x1)
#define HFI_CMD_SYS_PC_PREP            (HFI_CMD_COMMON_START + 0x2)
#define HFI_CMD_SYS_SET_PROPERTY       (HFI_CMD_COMMON_START + 0x3)
#define HFI_CMD_SYS_GET_PROPERTY       (HFI_CMD_COMMON_START + 0x4)
#define HFI_CMD_SYS_PING               (HFI_CMD_COMMON_START + 0x5)
#define HFI_CMD_SYS_RESET              (HFI_CMD_COMMON_START + 0x6)

/* Core level commands */
/* IPE/BPS core Commands */
#define HFI_CMD_IPE_BPS_COMMON_START \
		(HFI_DOMAIN_BASE_IPE_BPS + HFI_CMD_START_OFFSET + 0x0)
#define HFI_CMD_IPEBPS_CREATE_HANDLE \
		(HFI_CMD_IPE_BPS_COMMON_START + 0x8)
#define HFI_CMD_IPEBPS_ASYNC_COMMAND_DIRECT \
		(HFI_CMD_IPE_BPS_COMMON_START + 0xa)
#define HFI_CMD_IPEBPS_ASYNC_COMMAND_INDIRECT \
		(HFI_CMD_IPE_BPS_COMMON_START + 0xe)

/* CDM core Commands */
#define HFI_CMD_CDM_COMMON_START \
		(HFI_DOMAIN_BASE_CDM + HFI_CMD_START_OFFSET + 0x0)
#define HFI_CMD_CDM_TEST_START (HFI_CMD_CDM_COMMON_START + 0x800)
#define HFI_CMD_CDM_END        (HFI_CMD_CDM_COMMON_START + 0xFFF)

/* Debug/Test Commands */
#define HFI_CMD_DBG_COMMON_START \
		(HFI_DOMAIN_BASE_DBG + HFI_CMD_START_OFFSET + 0x0)
#define HFI_CMD_DBG_TEST_START  (HFI_CMD_DBG_COMMON_START + 0x800)
#define HFI_CMD_DBG_END         (HFI_CMD_DBG_COMMON_START + 0xFFF)

/* System level messages */
#define HFI_MSG_ICP_COMMON_START \
		(HFI_DOMAIN_BASE_ICP + HFI_MSG_START_OFFSET + 0x0)
#define HFI_MSG_SYS_INIT_DONE           (HFI_MSG_ICP_COMMON_START + 0x1)
#define HFI_MSG_SYS_PC_PREP_DONE        (HFI_MSG_ICP_COMMON_START + 0x2)
#define HFI_MSG_SYS_DEBUG               (HFI_MSG_ICP_COMMON_START + 0x3)
#define HFI_MSG_SYS_IDLE                (HFI_MSG_ICP_COMMON_START + 0x4)
#define HFI_MSG_SYS_PROPERTY_INFO       (HFI_MSG_ICP_COMMON_START + 0x5)
#define HFI_MSG_SYS_PING_ACK            (HFI_MSG_ICP_COMMON_START + 0x6)
#define HFI_MSG_SYS_RESET_ACK           (HFI_MSG_ICP_COMMON_START + 0x7)
#define HFI_MSG_EVENT_NOTIFY            (HFI_MSG_ICP_COMMON_START + 0x8)

/* Core level Messages */
/* IPE/BPS core Messages */
#define HFI_MSG_IPE_BPS_COMMON_START \
		(HFI_DOMAIN_BASE_IPE_BPS + HFI_MSG_START_OFFSET + 0x0)
#define HFI_MSG_IPEBPS_CREATE_HANDLE_ACK \
		(HFI_MSG_IPE_BPS_COMMON_START + 0x08)
#define HFI_MSG_IPEBPS_ASYNC_COMMAND_DIRECT_ACK \
		(HFI_MSG_IPE_BPS_COMMON_START + 0x0a)
#define HFI_MSG_IPEBPS_ASYNC_COMMAND_INDIRECT_ACK \
		(HFI_MSG_IPE_BPS_COMMON_START + 0x0e)
#define HFI_MSG_IPE_BPS_TEST_START	\
		(HFI_MSG_IPE_BPS_COMMON_START + 0x800)
#define HFI_MSG_IPE_BPS_END \
		(HFI_MSG_IPE_BPS_COMMON_START + 0xFFF)

/* CDM core Messages */
#define HFI_MSG_CDM_COMMON_START \
		(HFI_DOMAIN_BASE_CDM + HFI_MSG_START_OFFSET + 0x0)
#define  HFI_MSG_PRI_CDM_PAYLOAD_ACK    (HFI_MSG_CDM_COMMON_START + 0xa)
#define  HFI_MSG_PRI_LLD_PAYLOAD_ACK    (HFI_MSG_CDM_COMMON_START + 0xb)
#define HFI_MSG_CDM_TEST_START          (HFI_MSG_CDM_COMMON_START + 0x800)
#define HFI_MSG_CDM_END                 (HFI_MSG_CDM_COMMON_START + 0xFFF)

/* core level test command ranges */
/* ICP core level test command range */
#define HFI_CMD_ICP_TEST_START          (HFI_CMD_ICP_COMMON_START + 0x800)
#define HFI_CMD_ICP_END                 (HFI_CMD_ICP_COMMON_START + 0xFFF)

/* IPE/BPS core level test command range */
#define HFI_CMD_IPE_BPS_TEST_START \
		(HFI_CMD_IPE_BPS_COMMON_START + 0x800)
#define HFI_CMD_IPE_BPS_END (HFI_CMD_IPE_BPS_COMMON_START + 0xFFF)

/* ICP core level test message range */
#define HFI_MSG_ICP_TEST_START  (HFI_MSG_ICP_COMMON_START + 0x800)
#define HFI_MSG_ICP_END         (HFI_MSG_ICP_COMMON_START + 0xFFF)

/* ICP core level Debug test message range */
#define HFI_MSG_DBG_COMMON_START \
		(HFI_DOMAIN_BASE_DBG + 0x0)
#define HFI_MSG_DBG_TEST_START  (HFI_MSG_DBG_COMMON_START + 0x800)
#define HFI_MSG_DBG_END         (HFI_MSG_DBG_COMMON_START + 0xFFF)

/* System  level property base offset */
#define HFI_PROPERTY_ICP_COMMON_START  (HFI_DOMAIN_BASE_ICP + 0x0)

#define HFI_PROP_SYS_DEBUG_CFG         (HFI_PROPERTY_ICP_COMMON_START + 0x1)
#define HFI_PROP_SYS_UBWC_CFG          (HFI_PROPERTY_ICP_COMMON_START + 0x2)
#define HFI_PROP_SYS_IMAGE_VER         (HFI_PROPERTY_ICP_COMMON_START + 0x3)
#define HFI_PROP_SYS_SUPPORTED         (HFI_PROPERTY_ICP_COMMON_START + 0x4)
#define HFI_PROP_SYS_IPEBPS_PC         (HFI_PROPERTY_ICP_COMMON_START + 0x5)

/* Capabilities reported at sys init */
#define HFI_CAPS_PLACEHOLDER_1         (HFI_COMMON_BASE + 0x1)
#define HFI_CAPS_PLACEHOLDER_2         (HFI_COMMON_BASE + 0x2)

/* Section describes different debug levels (HFI_DEBUG_MSG_X)
 * available for debug messages from FW
 */
#define  HFI_DEBUG_MSG_LOW      0x00000001
#define  HFI_DEBUG_MSG_MEDIUM   0x00000002
#define  HFI_DEBUG_MSG_HIGH     0x00000004
#define  HFI_DEBUG_MSG_ERROR    0x00000008
#define  HFI_DEBUG_MSG_FATAL    0x00000010
/* Messages containing performance data */
#define  HFI_DEBUG_MSG_PERF     0x00000020
/* Disable ARM9 WFI in low power mode. */
#define  HFI_DEBUG_CFG_WFI      0x01000000
/* Disable ARM9 watchdog. */
#define  HFI_DEBUG_CFG_ARM9WD   0x10000000

/* Debug Msg Communication types:
 * Section describes different modes (HFI_DEBUG_MODE_X)
 * available to communicate the debug messages
 */
 /* Debug message output through   the interface debug queue. */
#define HFI_DEBUG_MODE_QUEUE     0x00000001
 /* Debug message output through QDSS. */
#define HFI_DEBUG_MODE_QDSS      0x00000002


#define HFI_DEBUG_MSG_LOW        0x00000001
#define HFI_DEBUG_MSG_MEDIUM     0x00000002
#define HFI_DEBUG_MSG_HIGH       0x00000004
#define HFI_DEBUG_MSG_ERROR      0x00000008
#define HFI_DEBUG_MSG_FATAL      0x00000010
#define HFI_DEBUG_MSG_PERF       0x00000020
#define HFI_DEBUG_CFG_WFI        0x01000000
#define HFI_DEBUG_CFG_ARM9WD     0x10000000

#define HFI_DEBUG_MODE_QUEUE     0x00000001
#define HFI_DEBUG_MODE_QDSS      0x00000002

#define HFI_DEV_VERSION_MAX      0x5

/**
 * start of sys command packet types
 * These commands are used to get system level information
 * from firmware
 */

/**
 * struct hfi_caps_support
 * payload to report caps through HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED
 * @type: capability type
 * @min: minimum supported value for the capability
 * @max: maximum supported value for the capability
 * @step_size: supported steps between min-max
 */
struct hfi_caps_support {
	uint32_t type;
	uint32_t min;
	uint32_t max;
	uint32_t step_size;
} __packed;

/**
 * struct hfi_caps_support_info
 * capability report through HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED
 * @num_caps: number of capabilities listed
 * @caps_data: capabilities info array
 */
struct hfi_caps_support_info {
	uint32_t num_caps;
	struct hfi_caps_support caps_data[1];
} __packed;

/**
 * struct hfi_debug
 * payload structure to configure HFI_PROPERTY_SYS_DEBUG_CONFIG
 * @debug_config: it is a result of HFI_DEBUG_MSG_X values that
 *                are OR-ed together to specify the debug message types
 *                to otput
 * @debug_mode: debug message output through debug queue/qdss
 * @HFI_PROPERTY_SYS_DEBUG_CONFIG
 */
struct hfi_debug {
	uint32_t debug_config;
	uint32_t debug_mode;
} __packed;

/**
 * struct hfi_ipe_bps_pc
 * payload structure to configure HFI_PROPERTY_SYS_IPEBPS_PC
 * @enable: Flag to enable IPE, BPS interfrane power collapse
 * @core_info: Core information to firmware
 */
struct hfi_ipe_bps_pc {
	uint32_t enable;
	uint32_t core_info;
} __packed;

/**
 * struct hfi_cmd_ubwc_cfg
 * Payload structure to configure HFI_PROP_SYS_UBWC_CFG
 * @ubwc_fetch_cfg: UBWC configuration for fecth
 * @ubwc_write_cfg: UBWC configuration for write
 */
struct hfi_cmd_ubwc_cfg {
	uint32_t ubwc_fetch_cfg;
	uint32_t ubwc_write_cfg;
};

/**
 * struct hfi_cmd_sys_init
 * command to initialization of system session
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @HFI_CMD_SYS_INIT
 */
struct hfi_cmd_sys_init {
	uint32_t size;
	uint32_t pkt_type;
} __packed;

/**
 * struct hfi_cmd_pc_prep
 * command to firmware to prepare for power collapse
 * @eize: packet size in bytes
 * @pkt_type: opcode of a packet
 * @HFI_CMD_SYS_PC_PREP
 */
struct hfi_cmd_pc_prep {
	uint32_t size;
	uint32_t pkt_type;
} __packed;

/**
 * struct hfi_cmd_prop
 * command to get/set properties of firmware
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @num_prop: number of properties queried/set
 * @prop_data: array of property IDs being queried. size depends on num_prop
 *             array of property IDs and associated structure pairs in set
 * @HFI_CMD_SYS_GET_PROPERTY
 * @HFI_CMD_SYS_SET_PROPERTY
 */
struct hfi_cmd_prop {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t num_prop;
	uint32_t prop_data[1];
} __packed;

/**
 * struct hfi_cmd_ping_pkt
 * ping command pings the firmware to confirm whether
 * it is alive.
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @user_data: client data, firmware returns this data
 *             as part of HFI_MSG_SYS_PING_ACK
 * @HFI_CMD_SYS_PING
 */
struct hfi_cmd_ping_pkt {
	uint32_t size;
	uint32_t pkt_type;
	uint64_t user_data;
} __packed;

/**
 * struct hfi_cmd_sys_reset_pkt
 * sends the reset command to FW. FW responds in the same type
 * of packet. so can be used for reset_ack_pkt type also
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @user_data: client data, firmware returns this data
 *             as part of HFI_MSG_SYS_RESET_ACK
 * @HFI_CMD_SYS_RESET
 */

struct hfi_cmd_sys_reset_pkt {
	uint32_t size;
	uint32_t pkt_type;
	uint64_t user_data;
} __packed;

/* end of sys command packet types */

/* start of sys message packet types */

/**
 * struct hfi_prop
 * structure to report maximum supported features of firmware.
 */
struct hfi_sys_support {
	uint32_t place_holder;
} __packed;

/**
 * struct hfi_supported_prop
 * structure to report HFI_PROPERTY_PARAM_PROPERTIES_SUPPORTED
 * for a session
 * @num_prop: number of properties supported
 * @prop_data: array of supported property IDs
 */
struct hfi_supported_prop {
	uint32_t num_prop;
	uint32_t prop_data[1];
} __packed;

/**
 * struct hfi_image_version
 * system image version
 * @major: major version number
 * @minor: minor version number
 * @ver_name_size: size of version name
 * @ver_name: image version name
 */
struct hfi_image_version {
	uint32_t major;
	uint32_t minor;
	uint32_t ver_name_size;
	uint8_t  ver_name[1];
} __packed;

/**
 * struct hfi_msg_init_done_data
 * @api_ver:    Firmware API version
 * @dev_ver:    Device version
 * @num_icp_hw: Number of ICP hardware information
 * @dev_hw_ver: Supported hardware version information
 * @reserved:   Reserved field
 */
struct hfi_msg_init_done_data {
	uint32_t api_ver;
	uint32_t dev_ver;
	uint32_t num_icp_hw;
	uint32_t dev_hw_ver[HFI_DEV_VERSION_MAX];
	uint32_t reserved;
};

/**
 * struct hfi_msg_init_done
 * system init done message from firmware. Many system level properties
 * are returned with the packet
 * @size:      Packet size in bytes
 * @pkt_type:  Opcode of a packet
 * @err_type:  Error code associated with response
 * @num_prop:  Number of default capability info
 * @prop_data: Array of property ids and corresponding structure pairs
 */
struct hfi_msg_init_done {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t err_type;
	uint32_t num_prop;
	uint32_t prop_data[1];
} __packed;

/**
 * struct hfi_msg_pc_prep_done
 * system power collapse preperation done message
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @err_type: error code associated with the response
 */
struct hfi_msg_pc_prep_done {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t err_type;
} __packed;

/**
 * struct hfi_msg_prop
 * system property info from firmware
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @num_prop: number of property info structures
 * @prop_data: array of property IDs and associated structure pairs
 */
struct hfi_msg_prop {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t num_prop;
	uint32_t prop_data[1];
} __packed;

/**
 * struct hfi_msg_idle
 * system idle message from firmware
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 */
struct hfi_msg_idle {
	uint32_t size;
	uint32_t pkt_type;
} __packed;

/**
 * struct hfi_msg_ping_ack
 * system ping ack message
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @user_data: this data is sent as part of ping command from host
 */
struct hfi_msg_ping_ack {
	uint32_t size;
	uint32_t pkt_type;
	uint64_t user_data;
} __packed;

/**
 * struct hfi_msg_debug
 * system debug message defination
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @msg_type: debug message type
 * @msg_size: size of debug message in bytes
 * @timestamp_hi: most significant 32 bits of the 64 bit timestamp field.
 *                timestamp shall be interpreted as a signed 64-bit value
 *                representing microseconds.
 * @timestamp_lo: least significant 32 bits of the 64 bit timestamp field.
 *                timestamp shall be interpreted as a signed 64-bit value
 *                representing microseconds.
 * @msg_data: message data in string form
 */
struct hfi_msg_debug {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t msg_type;
	uint32_t msg_size;
	uint32_t timestamp_hi;
	uint32_t timestamp_lo;
	uint8_t  msg_data[1];
} __packed;
/**
 * struct hfi_msg_event_notify
 * event notify message
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @fw_handle: firmware session handle
 * @event_id: session event id
 * @event_data1: event data corresponding to event ID
 * @event_data2: event data corresponding to event ID
 * @ext_event_data: info array, interpreted based on event_data1
 * and event_data2
 */
struct hfi_msg_event_notify {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t fw_handle;
	uint32_t event_id;
	uint32_t event_data1;
	uint32_t event_data2;
	uint32_t ext_event_data[1];
} __packed;
/**
 * end of sys message packet types
 */

#endif /* _HFI_DEFS_H_ */
