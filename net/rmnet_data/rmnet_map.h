/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/spinlock.h>

#ifndef _RMNET_MAP_H_
#define _RMNET_MAP_H_

#define RMNET_MAP_MAX_FLOWS 8

struct rmnet_map_header_s {
#ifndef RMNET_USE_BIG_ENDIAN_STRUCTS
	uint8_t  pad_len:6;
	uint8_t  reserved_bit:1;
	uint8_t  cd_bit:1;
#else
	uint8_t  cd_bit:1;
	uint8_t  reserved_bit:1;
	uint8_t  pad_len:6;
#endif /* RMNET_USE_BIG_ENDIAN_STRUCTS */
	uint8_t  mux_id;
	uint16_t pkt_len;
}  __aligned(1);

struct rmnet_map_control_command_s {
	uint8_t command_name;
#ifndef RMNET_USE_BIG_ENDIAN_STRUCTS
	uint8_t  cmd_type:2;
	uint8_t  reserved:6;
#else
	uint8_t  reserved:6;
	uint8_t  cmd_type:2;
#endif /* RMNET_USE_BIG_ENDIAN_STRUCTS */
	uint16_t reserved2;
	uint32_t   transaction_id;
	union {
		uint8_t  data[65528];
		struct {
#ifndef RMNET_USE_BIG_ENDIAN_STRUCTS
			uint16_t  ip_family:2;
			uint16_t  reserved:14;
#else
			uint16_t  reserved:14;
			uint16_t  ip_family:2;
#endif /* RMNET_USE_BIG_ENDIAN_STRUCTS */
			uint16_t  flow_control_seq_num;
			uint32_t  qos_id;
		} flow_control;
	};
}  __aligned(1);

struct rmnet_map_flow_mapping_s {
	uint32_t flow_id;
	uint32_t tc_handle;
	uint64_t v4_seq;
	uint64_t v6_seq;
};

struct rmnet_map_flow_control_s {
	rwlock_t flow_map_lock;
	uint32_t default_tc_handle;
	uint64_t default_v4_seq;
	uint64_t default_v6_seq;
	struct rmnet_map_flow_mapping_s flowmap[RMNET_MAP_MAX_FLOWS];
};

enum rmnet_map_results_e {
	RMNET_MAP_SUCCESS,
	RMNET_MAP_CONSUMED,
	RMNET_MAP_GENERAL_FAILURE,
	RMNET_MAP_NOT_ENABLED,
	RMNET_MAP_FAILED_AGGREGATION,
	RMNET_MAP_FAILED_MUX
};

enum rmnet_map_mux_errors_e {
	RMNET_MAP_MUX_SUCCESS,
	RMNET_MAP_MUX_INVALID_MUX_ID,
	RMNET_MAP_MUX_INVALID_PAD_LENGTH,
	RMNET_MAP_MUX_INVALID_PKT_LENGTH,
	/* This should always be the last element */
	RMNET_MAP_MUX_ENUM_LENGTH
};

enum rmnet_map_checksum_errors_e {
	RMNET_MAP_CHECKSUM_OK,
	RMNET_MAP_CHECKSUM_VALID_FLAG_NOT_SET,
	RMNET_MAP_CHECKSUM_VALIDATION_FAILED,
	RMNET_MAP_CHECKSUM_ERROR_UNKOWN,
	RMNET_MAP_CHECKSUM_ERROR_NOT_DATA_PACKET,
	RMNET_MAP_CHECKSUM_ERROR_BAD_BUFFER,
	RMNET_MAP_CHECKSUM_ERROR_UNKNOWN_IP_VERSION,
	RMNET_MAP_CHECKSUM_ERROR_UNKNOWN_TRANSPORT,
	/* This should always be the last element */
	RMNET_MAP_CHECKSUM_ENUM_LENGTH
};

enum rmnet_map_commands_e {
	RMNET_MAP_COMMAND_NONE,
	RMNET_MAP_COMMAND_FLOW_DISABLE,
	RMNET_MAP_COMMAND_FLOW_ENABLE,
	/* These should always be the last 2 elements */
	RMNET_MAP_COMMAND_UNKNOWN,
	RMNET_MAP_COMMAND_ENUM_LENGTH
};

enum rmnet_map_agg_state_e {
	RMNET_MAP_AGG_IDLE,
	RMNET_MAP_TXFER_SCHEDULED
};

#define RMNET_MAP_P_ICMP4  0x01
#define RMNET_MAP_P_TCP    0x06
#define RMNET_MAP_P_UDP    0x11
#define RMNET_MAP_P_ICMP6  0x3a

#define RMNET_MAP_COMMAND_REQUEST     0
#define RMNET_MAP_COMMAND_ACK         1
#define RMNET_MAP_COMMAND_UNSUPPORTED 2
#define RMNET_MAP_COMMAND_INVALID     3

uint8_t rmnet_map_demultiplex(struct sk_buff *skb);
struct sk_buff *rmnet_map_deaggregate(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config);

#define RMNET_MAP_GET_MUX_ID(Y) (((struct rmnet_map_header_s *)Y->data)->mux_id)
#define RMNET_MAP_GET_CD_BIT(Y) (((struct rmnet_map_header_s *)Y->data)->cd_bit)
#define RMNET_MAP_GET_PAD(Y) (((struct rmnet_map_header_s *)Y->data)->pad_len)
#define RMNET_MAP_GET_CMD_START(Y) ((struct rmnet_map_control_command_s *) \
				    Y->data + sizeof(struct rmnet_map_header_s))
#define RMNET_MAP_GET_LENGTH(Y) (ntohs( \
			       ((struct rmnet_map_header_s *)Y->data)->pkt_len))

struct rmnet_map_header_s *rmnet_map_add_map_header(struct sk_buff *skb,
						    int hdrlen);
rx_handler_result_t rmnet_map_command(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config);
void rmnet_map_aggregate(struct sk_buff *skb,
			 struct rmnet_phys_ep_conf_s *config);

#endif /* _RMNET_MAP_H_ */
