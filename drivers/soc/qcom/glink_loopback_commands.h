/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#ifndef _GLINK_LOOPBACK_COMMANDS_H_
#define _GLINK_LOOPBACK_COMMANDS_H_

#define MAX_NAME_LEN 32

enum request_type {
	OPEN = 1,
	CLOSE,
	QUEUE_RX_INTENT_CONFIG,
	TX_CONFIG,
	RX_DONE_CONFIG,
};

struct req_hdr {
	uint32_t req_id;
	uint32_t req_type;
	uint32_t req_size;
};

struct open_req {
	uint32_t delay_ms;
	uint32_t name_len;
	char ch_name[MAX_NAME_LEN];
};

struct close_req {
	uint32_t delay_ms;
	uint32_t name_len;
	char ch_name[MAX_NAME_LEN];
};

struct queue_rx_intent_config_req {
	uint32_t num_intents;
	uint32_t intent_size;
	uint32_t random_delay;
	uint32_t delay_ms;
	uint32_t name_len;
	char ch_name[MAX_NAME_LEN];
};

enum transform_type {
	NO_TRANSFORM = 0,
	PACKET_COUNT,
	CHECKSUM,
};

struct tx_config_req {
	uint32_t random_delay;
	uint32_t delay_ms;
	uint32_t echo_count;
	uint32_t transform_type;
	uint32_t name_len;
	char ch_name[MAX_NAME_LEN];
};

struct rx_done_config_req {
	uint32_t random_delay;
	uint32_t delay_ms;
	uint32_t name_len;
	char ch_name[MAX_NAME_LEN];
};

union req_payload {
	struct open_req open;
	struct close_req close;
	struct queue_rx_intent_config_req q_rx_int_conf;
	struct tx_config_req tx_conf;
	struct rx_done_config_req rx_done_conf;
};

struct req {
	struct req_hdr hdr;
	union req_payload payload;
};

struct resp {
	uint32_t req_id;
	uint32_t req_type;
	uint32_t response;
};

/*
 * Tracer Packet Event IDs for Loopback Client/Server.
 * This being a client of G-Link, the tracer packet events start
 * from 256.
 */
enum loopback_tracer_pkt_events {
	LOOPBACK_SRV_TX = 256,
	LOOPBACK_SRV_RX = 257,
	LOOPBACK_CLNT_TX = 258,
	LOOPBACK_CLNT_RX = 259,
};
#endif
