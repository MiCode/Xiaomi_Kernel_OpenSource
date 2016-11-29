/*
 * HECI bus layer messages handling
 *
 * Copyright (c) 2003-2015, Intel Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _HECI_HBM_H_
#define _HECI_HBM_H_

#include <linux/uuid.h>

struct heci_device;
struct heci_msg_hdr;
struct heci_cl;

/*
 * Timeouts in Seconds
 */
#define HECI_INTEROP_TIMEOUT         7  /* Timeout on ready message */
#define HECI_CONNECT_TIMEOUT         3  /* HPS: at least 2 seconds */

#define HECI_CL_CONNECT_TIMEOUT     15  /* HPS: Client Connect Timeout */
#define HECI_CLIENTS_INIT_TIMEOUT   15  /* HPS: Clients Enumeration Timeout */

#if 0
#define HECI_IAMTHIF_STALL_TIMER    12  /* HPS */
#define HECI_IAMTHIF_READ_TIMER     10  /* HPS */
#endif


/*
 * HECI Version
 */
#define HBM_MINOR_VERSION                   0
#define HBM_MAJOR_VERSION                   1
#define HBM_TIMEOUT                         1	/* 1 second */

/* Host bus message command opcode */
#define HECI_HBM_CMD_OP_MSK                  0x7f
/* Host bus message command RESPONSE */
#define HECI_HBM_CMD_RES_MSK                 0x80

/*
 * HECI Bus Message Command IDs
 */
#define HOST_START_REQ_CMD                  0x01
#define HOST_START_RES_CMD                  0x81

#define HOST_STOP_REQ_CMD                   0x02
#define HOST_STOP_RES_CMD                   0x82

#define ME_STOP_REQ_CMD                     0x03

#define HOST_ENUM_REQ_CMD                   0x04
#define HOST_ENUM_RES_CMD                   0x84

#define HOST_CLIENT_PROPERTIES_REQ_CMD      0x05
#define HOST_CLIENT_PROPERTIES_RES_CMD      0x85

#define CLIENT_CONNECT_REQ_CMD              0x06
#define CLIENT_CONNECT_RES_CMD              0x86

#define CLIENT_DISCONNECT_REQ_CMD           0x07
#define CLIENT_DISCONNECT_RES_CMD           0x87

#define HECI_FLOW_CONTROL_CMD                0x08

#define CLIENT_DMA_REQ_CMD		0x10
#define CLIENT_DMA_RES_CMD		0x90

/*
 * HECI Stop Reason
 * used by hbm_host_stop_request.reason
 */
enum heci_stop_reason_types {
	DRIVER_STOP_REQUEST = 0x00,
	DEVICE_D1_ENTRY = 0x01,
	DEVICE_D2_ENTRY = 0x02,
	DEVICE_D3_ENTRY = 0x03,
	SYSTEM_S1_ENTRY = 0x04,
	SYSTEM_S2_ENTRY = 0x05,
	SYSTEM_S3_ENTRY = 0x06,
	SYSTEM_S4_ENTRY = 0x07,
	SYSTEM_S5_ENTRY = 0x08
};

/*
 * Client Connect Status
 * used by hbm_client_connect_response.status
 */
enum client_connect_status_types {
	CCS_SUCCESS = 0x00,
	CCS_NOT_FOUND = 0x01,
	CCS_ALREADY_STARTED = 0x02,
	CCS_OUT_OF_RESOURCES = 0x03,
	CCS_MESSAGE_SMALL = 0x04
};

/*
 * Client Disconnect Status
 */
enum client_disconnect_status_types {
	CDS_SUCCESS = 0x00
};

/*
 *  HECI BUS Interface Section
 */
struct heci_msg_hdr {
	u32 me_addr:8;
	u32 host_addr:8;
	u32 length:9;
	u32 reserved:6;
	u32 msg_complete:1;
} __packed;


struct heci_bus_message {
	u8 hbm_cmd;
	u8 data[0];
} __packed;

/**
 * struct hbm_cl_cmd - client specific host bus command
 *	CONNECT, DISCONNECT, and FlOW CONTROL
 *
 * @hbm_cmd - bus message command header
 * @me_addr - address of the client in ME
 * @host_addr - address of the client in the driver
 * @data
 */
struct heci_hbm_cl_cmd {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 data;
};

struct hbm_version {
	u8 minor_version;
	u8 major_version;
} __packed;

struct hbm_host_version_request {
	u8 hbm_cmd;
	u8 reserved;
	struct hbm_version host_version;
} __packed;

struct hbm_host_version_response {
	u8 hbm_cmd;
	u8 host_version_supported;
	struct hbm_version me_max_version;
} __packed;

struct hbm_host_stop_request {
	u8 hbm_cmd;
	u8 reason;
	u8 reserved[2];
} __packed;

struct hbm_host_stop_response {
	u8 hbm_cmd;
	u8 reserved[3];
} __packed;

struct hbm_me_stop_request {
	u8 hbm_cmd;
	u8 reason;
	u8 reserved[2];
} __packed;

struct hbm_host_enum_request {
	u8 hbm_cmd;
	u8 reserved[3];
} __packed;

struct hbm_host_enum_response {
	u8 hbm_cmd;
	u8 reserved[3];
	u8 valid_addresses[32];
} __packed;

struct heci_client_properties {
	uuid_le protocol_name;
	u8 protocol_version;
	u8 max_number_of_connections;
	u8 fixed_address;
	u8 single_recv_buf;
	u32 max_msg_length;
	u8 dma_hdr_len;
#define	HECI_CLIENT_DMA_ENABLED	0x80
	u8 reserved4;
	u8 reserved5;
	u8 reserved6;
} __packed;

struct hbm_props_request {
	u8 hbm_cmd;
	u8 address;
	u8 reserved[2];
} __packed;


struct hbm_props_response {
	u8 hbm_cmd;
	u8 address;
	u8 status;
	u8 reserved[1];
	struct heci_client_properties client_properties;
} __packed;

/**
 * struct hbm_client_connect_request - connect/disconnect request
 *
 * @hbm_cmd - bus message command header
 * @me_addr - address of the client in ME
 * @host_addr - address of the client in the driver
 * @reserved
 */
struct hbm_client_connect_request {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 reserved;
} __packed;

/**
 * struct hbm_client_connect_response - connect/disconnect response
 *
 * @hbm_cmd - bus message command header
 * @me_addr - address of the client in ME
 * @host_addr - address of the client in the driver
 * @status - status of the request
 */
struct hbm_client_connect_response {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 status;
} __packed;


#define HECI_FC_MESSAGE_RESERVED_LENGTH           5

struct hbm_flow_control {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 reserved[HECI_FC_MESSAGE_RESERVED_LENGTH];
} __packed;

struct hbm_client_dma_request {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 reserved;
	u64 msg_addr;
	u32 msg_len;
	u16 reserved2;
	u16 msg_preview_len;
	u8 msg_preview[12];
} __packed;

struct hbm_client_dma_response {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 status;
	u64 msg_addr;
	u32 msg_len;
} __packed;

/**
 * enum heci_hbm_state - host bus message protocol state
 *
 * @HECI_HBM_IDLE : protocol not started
 * @HECI_HBM_START : start request message was sent
 * @HECI_HBM_ENUM_CLIENTS : enumeration request was sent
 * @HECI_HBM_CLIENT_PROPERTIES : acquiring clients properties
 */
enum heci_hbm_state {
	HECI_HBM_IDLE = 0,
	HECI_HBM_START,
	HECI_HBM_STARTED,
	HECI_HBM_ENUM_CLIENTS,
	HECI_HBM_CLIENT_PROPERTIES,
	HECI_HBM_WORKING,
	HECI_HBM_STOPPED,
};

#if 0
void heci_hbm_dispatch(struct heci_device *dev, struct heci_msg_hdr *hdr);
#else
void heci_hbm_dispatch(struct heci_device *dev, struct heci_bus_message *hdr);
#endif

static inline void heci_hbm_hdr(struct heci_msg_hdr *hdr, size_t length)
{
	hdr->host_addr = 0;
	hdr->me_addr = 0;
	hdr->length = length;
	hdr->msg_complete = 1;
	hdr->reserved = 0;
}

int heci_hbm_start_req(struct heci_device *dev);
int heci_hbm_start_wait(struct heci_device *dev);
int heci_hbm_cl_flow_control_req(struct heci_device *dev, struct heci_cl *cl);
int heci_hbm_cl_disconnect_req(struct heci_device *dev, struct heci_cl *cl);
int heci_hbm_cl_connect_req(struct heci_device *dev, struct heci_cl *cl);
void heci_hbm_enum_clients_req(struct heci_device *dev);
void	recv_hbm(struct heci_device *dev, struct heci_msg_hdr *heci_hdr);

/* System state */
#define HECI_SYSTEM_STATE_CLIENT_ADDR 13

#define SYSTEM_STATE_SUBSCRIBE                  0x1
#define SYSTEM_STATE_STATUS                     0x2
#define SYSTEM_STATE_QUERY_SUBSCRIBERS          0x3
#define SYSTEM_STATE_STATE_CHANGE_REQ		0x4

#define SUSPEND_STATE_BIT       (1<<1) /*indicates suspend and resume states*/

#define ANDROID_EVENT_MASK	0xff000000

struct ish_system_states_header {
	u32 cmd;
	u32 cmd_status;  /*responses will have this set*/
} __packed;

struct ish_system_states_subscribe {
	struct ish_system_states_header hdr;
	u32 states;
} __packed;

struct ish_system_states_status {
	struct ish_system_states_header hdr;
	u32 supported_states;
	u32 states_status;
} __packed;

struct ish_system_states_query_subscribers {
	struct ish_system_states_header hdr;
} __packed;

struct ish_system_states_state_change_req {
	struct ish_system_states_header hdr;
	u32 requested_states;
	u32 states_status;
} __packed;

void send_suspend(struct heci_device *dev);
void send_resume(struct heci_device *dev);
void query_subscribers(struct heci_device *dev);

void recv_fixed_cl_msg(struct heci_device *dev, struct heci_msg_hdr *heci_hdr);
void heci_hbm_dispatch(struct heci_device *dev, struct heci_bus_message *hdr);
#endif /* _HECI_HBM_H_ */

