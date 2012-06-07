/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#ifndef __DAL_VOICE_H__
#define __DAL_VOICE_H__

#define VOICE_DAL_DEVICE 0x02000075
#define VOICE_DAL_PORT "DAL_AM_AUD"
#define VOICE_DAL_VERSION 0x00010000

#define APR_PKTV1_TYPE_EVENT_V 0
#define APR_UNDEFINED -1
#define APR_PKTV1_TYPE_MASK 0x00000010
#define APR_PKTV1_TYPE_SHFT 4

#define APR_SET_BITMASK(mask, shift, value) \
	(((value) << (shift)) & (mask))

#define APR_SET_FIELD(field, value) \
	APR_SET_BITMASK((field##_MASK), (field##_SHFT), (value))


enum {
	VOICE_OP_INIT = DAL_OP_FIRST_DEVICE_API,
	VOICE_OP_CONTROL,
};

struct apr_command_pkt {
	uint32_t size;
	uint32_t header;
	uint16_t reserved1;
	uint16_t src_addr;
	uint16_t dst_addr;
	uint16_t ret_addr;
	uint32_t src_token;
	uint32_t dst_token;
	uint32_t ret_token;
	uint32_t context;
	uint32_t opcode;
} __attribute__ ((packed));


#define APR_IBASIC_RSP_RESULT 0x00010000

#define APR_OP_CMD_CREATE 0x0001001B

#define APR_OP_CMD_DESTROY 0x0001001C

#define VOICE_OP_CMD_BRINGUP 0x0001001E

#define VOICE_OP_CMD_TEARDOWN 0x0001001F

#define VOICE_OP_CMD_SET_NETWORK 0x0001001D

#define VOICE_OP_CMD_STREAM_SETUP 0x00010027

#define VOICE_OP_CMD_STREAM_TEARDOWN 0x00010028

#endif
