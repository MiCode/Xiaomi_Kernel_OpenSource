/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

struct rmnet_map_control_command_s {
	u8  command_name;
	u8  cmd_type:2;
	u8  reserved:6;
	u16 reserved2;
	u32 transaction_id;
	union {
		u8  data[65528];
		struct {
			u16 ip_family:2;
			u16 reserved:14;
			u16 flow_control_seq_num;
			u32 qos_id;
		} flow_control;
	};
}  __aligned(1);

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

enum rmnet_map_commands_e {
	RMNET_MAP_COMMAND_NONE,
	RMNET_MAP_COMMAND_FLOW_DISABLE,
	RMNET_MAP_COMMAND_FLOW_ENABLE,
	/* These should always be the last 2 elements */
	RMNET_MAP_COMMAND_UNKNOWN,
	RMNET_MAP_COMMAND_ENUM_LENGTH
};

struct rmnet_map_header_s {
	u8  pad_len:6;
	u8  reserved_bit:1;
	u8  cd_bit:1;
	u8  mux_id;
	u16 pkt_len;
}  __aligned(1);

#define RMNET_MAP_GET_MUX_ID(Y) (((struct rmnet_map_header_s *) \
				 (Y)->data)->mux_id)
#define RMNET_MAP_GET_CD_BIT(Y) (((struct rmnet_map_header_s *) \
				(Y)->data)->cd_bit)
#define RMNET_MAP_GET_PAD(Y) (((struct rmnet_map_header_s *) \
				(Y)->data)->pad_len)
#define RMNET_MAP_GET_CMD_START(Y) ((struct rmnet_map_control_command_s *) \
				    ((Y)->data + \
				      sizeof(struct rmnet_map_header_s)))
#define RMNET_MAP_GET_LENGTH(Y) (ntohs(((struct rmnet_map_header_s *) \
					(Y)->data)->pkt_len))

#define RMNET_MAP_COMMAND_REQUEST     0
#define RMNET_MAP_COMMAND_ACK         1
#define RMNET_MAP_COMMAND_UNSUPPORTED 2
#define RMNET_MAP_COMMAND_INVALID     3

#define RMNET_MAP_NO_PAD_BYTES        0
#define RMNET_MAP_ADD_PAD_BYTES       1

u8 rmnet_map_demultiplex(struct sk_buff *skb);
struct sk_buff *rmnet_map_deaggregate(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config);

struct rmnet_map_header_s *rmnet_map_add_map_header(struct sk_buff *skb,
						    int hdrlen, int pad);
rx_handler_result_t rmnet_map_command(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config);

#endif /* _RMNET_MAP_H_ */
