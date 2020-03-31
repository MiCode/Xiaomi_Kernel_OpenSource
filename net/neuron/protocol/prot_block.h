/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

#ifndef __H_PROT_BLOCK_COMMON_H
#define __H_PROT_BLOCK_COMMON_H

/* Block Server Advertisement message */
struct neuron_block_advertise {
	u32 logical_block_size;
	u32 physical_block_size;
	u64 num_device_sectors;
	u32 flags;
	u32 discard_granularity;
	u64 discard_max_hw_sectors;
	u64 discard_max_sectors;
	u16 alignment_offset;
	bool wc_flag;
	bool fua_flag;
	u8  uuid[16];
	u8 label[];
} __packed;

/* Request message */
struct neuron_block_req {
	u32 req_id;
	u16 req_type;
	u16 flags;
	u64 start_sector;
	u32 sectors;
} __packed;

/* Response message */
struct neuron_block_resp {
	u32 resp_id;
	u16 resp_status;
} __packed;

/* Bit position definition for flags field in struct neuron_block_req */
enum neuron_block_param_flags_bit {
	NEURON_BLOCK_PARAM_FLAG_READONLY = 0,
	NEURON_BLOCK_PARAM_FLAG_DISCARD_ZEROES,
};

#define NEURON_BLOCK_READONLY BIT(NEURON_BLOCK_PARAM_FLAG_READONLY)
#define NEURON_BLOCK_DISCARD_ZEROES BIT(NEURON_BLOCK_PARAM_FLAG_DISCARD_ZEROES)

#endif
