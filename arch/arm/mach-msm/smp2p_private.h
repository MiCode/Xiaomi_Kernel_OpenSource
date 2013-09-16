/* arch/arm/mach-msm/smp2p_private.h
 *
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
#ifndef _ARCH_ARM_MACH_MSM_MSM_SMP2P_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_MSM_SMP2P_PRIVATE_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <mach/msm_ipc_logging.h>
#include "smp2p_private_api.h"

#define SMP2P_MAX_ENTRY 16
#define SMP2P_FEATURE_SSR_ACK 0x1

/* SMEM Item Header Macros */
#define SMP2P_MAGIC 0x504D5324
#define SMP2P_LOCAL_PID_MASK 0x0000ffff
#define SMP2P_LOCAL_PID_BIT 0
#define SMP2P_REMOTE_PID_MASK 0xffff0000
#define SMP2P_REMOTE_PID_BIT 16
#define SMP2P_VERSION_MASK 0x000000ff
#define SMP2P_VERSION_BIT 0
#define SMP2P_FEATURE_MASK 0xffffff00
#define SMP2P_FEATURE_BIT 8
#define SMP2P_ENT_TOTAL_MASK 0x0000ffff
#define SMP2P_ENT_TOTAL_BIT 0
#define SMP2P_ENT_VALID_MASK 0xffff0000
#define SMP2P_ENT_VALID_BIT 16
#define SMP2P_FLAGS_RESTART_DONE_BIT 0
#define SMP2P_FLAGS_RESTART_DONE_MASK 0x1
#define SMP2P_FLAGS_RESTART_ACK_BIT 1
#define SMP2P_FLAGS_RESTART_ACK_MASK 0x2

#define SMP2P_GET_BITS(hdr_val, mask, bit) \
	(((hdr_val) & (mask)) >> (bit))
#define SMP2P_SET_BITS(hdr_val, mask, bit, new_value) \
	do {\
		hdr_val = (hdr_val & ~(mask)) \
		| (((new_value) << (bit)) & (mask)); \
	} while (0)

#define SMP2P_GET_LOCAL_PID(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_LOCAL_PID_MASK, SMP2P_LOCAL_PID_BIT)
#define SMP2P_SET_LOCAL_PID(hdr, pid) \
	SMP2P_SET_BITS(hdr, SMP2P_LOCAL_PID_MASK, SMP2P_LOCAL_PID_BIT, pid)

#define SMP2P_GET_REMOTE_PID(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_REMOTE_PID_MASK, SMP2P_REMOTE_PID_BIT)
#define SMP2P_SET_REMOTE_PID(hdr, pid) \
	SMP2P_SET_BITS(hdr, SMP2P_REMOTE_PID_MASK, SMP2P_REMOTE_PID_BIT, pid)

#define SMP2P_GET_VERSION(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_VERSION_MASK, SMP2P_VERSION_BIT)
#define SMP2P_SET_VERSION(hdr, version) \
	SMP2P_SET_BITS(hdr, SMP2P_VERSION_MASK, SMP2P_VERSION_BIT, version)

#define SMP2P_GET_FEATURES(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_FEATURE_MASK, SMP2P_FEATURE_BIT)
#define SMP2P_SET_FEATURES(hdr, features) \
	SMP2P_SET_BITS(hdr, SMP2P_FEATURE_MASK, SMP2P_FEATURE_BIT, features)

#define SMP2P_GET_ENT_TOTAL(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_ENT_TOTAL_MASK, SMP2P_ENT_TOTAL_BIT)
#define SMP2P_SET_ENT_TOTAL(hdr, entries) \
	SMP2P_SET_BITS(hdr, SMP2P_ENT_TOTAL_MASK, SMP2P_ENT_TOTAL_BIT, entries)

#define SMP2P_GET_ENT_VALID(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_ENT_VALID_MASK, SMP2P_ENT_VALID_BIT)
#define SMP2P_SET_ENT_VALID(hdr, entries) \
	SMP2P_SET_BITS(hdr,  SMP2P_ENT_VALID_MASK, SMP2P_ENT_VALID_BIT,\
		entries)

#define SMP2P_GET_RESTART_DONE(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_FLAGS_RESTART_DONE_MASK, \
			SMP2P_FLAGS_RESTART_DONE_BIT)
#define SMP2P_SET_RESTART_DONE(hdr, value) \
	SMP2P_SET_BITS(hdr, SMP2P_FLAGS_RESTART_DONE_MASK, \
			SMP2P_FLAGS_RESTART_DONE_BIT, value)

#define SMP2P_GET_RESTART_ACK(hdr) \
	SMP2P_GET_BITS(hdr, SMP2P_FLAGS_RESTART_ACK_MASK, \
			SMP2P_FLAGS_RESTART_ACK_BIT)
#define SMP2P_SET_RESTART_ACK(hdr, value) \
	SMP2P_SET_BITS(hdr, SMP2P_FLAGS_RESTART_ACK_MASK, \
			SMP2P_FLAGS_RESTART_ACK_BIT, value)

/* Loopback Command Macros */
#define SMP2P_RMT_CMD_TYPE_MASK 0x80000000
#define SMP2P_RMT_CMD_TYPE_BIT 31
#define SMP2P_RMT_IGNORE_MASK 0x40000000
#define SMP2P_RMT_IGNORE_BIT 30
#define SMP2P_RMT_CMD_MASK 0x3f000000
#define SMP2P_RMT_CMD_BIT 24
#define SMP2P_RMT_DATA_MASK 0x00ffffff
#define SMP2P_RMT_DATA_BIT 0

#define SMP2P_GET_RMT_CMD_TYPE(val) \
	SMP2P_GET_BITS(val, SMP2P_RMT_CMD_TYPE_MASK, SMP2P_RMT_CMD_TYPE_BIT)
#define SMP2P_GET_RMT_CMD(val) \
	SMP2P_GET_BITS(val, SMP2P_RMT_CMD_MASK, SMP2P_RMT_CMD_BIT)

#define SMP2P_GET_RMT_DATA(val) \
	SMP2P_GET_BITS(val, SMP2P_RMT_DATA_MASK, SMP2P_RMT_DATA_BIT)

#define SMP2P_SET_RMT_CMD_TYPE(val, cmd_type) \
	SMP2P_SET_BITS(val, SMP2P_RMT_CMD_TYPE_MASK, SMP2P_RMT_CMD_TYPE_BIT, \
		cmd_type)
#define SMP2P_SET_RMT_CMD_TYPE_REQ(val) \
	SMP2P_SET_RMT_CMD_TYPE(val, 1)
#define SMP2P_SET_RMT_CMD_TYPE_RESP(val) \
	SMP2P_SET_RMT_CMD_TYPE(val, 0)

#define SMP2P_SET_RMT_CMD(val, cmd) \
	SMP2P_SET_BITS(val, SMP2P_RMT_CMD_MASK, SMP2P_RMT_CMD_BIT, \
		cmd)
#define SMP2P_SET_RMT_DATA(val, data) \
	SMP2P_SET_BITS(val, SMP2P_RMT_DATA_MASK, SMP2P_RMT_DATA_BIT, data)

enum {
	SMP2P_LB_CMD_NOOP = 0x0,
	SMP2P_LB_CMD_ECHO,
	SMP2P_LB_CMD_CLEARALL,
	SMP2P_LB_CMD_PINGPONG,
	SMP2P_LB_CMD_RSPIN_START,
	SMP2P_LB_CMD_RSPIN_LOCKED,
	SMP2P_LB_CMD_RSPIN_UNLOCKED,
	SMP2P_LB_CMD_RSPIN_END,
};
#define SMP2P_RLPB_IGNORE 0x40
#define SMP2P_RLPB_ENTRY_NAME "smp2p"

/* Debug Logging Macros */
enum {
	MSM_SMP2P_INFO = 1U << 0,
	MSM_SMP2P_DEBUG = 1U << 1,
	MSM_SMP2P_GPIO = 1U << 2,
};

#define SMP2P_IPC_LOG_STR(x...) do { \
	if (smp2p_get_log_ctx()) \
		ipc_log_string(smp2p_get_log_ctx(), x); \
} while (0)

#define SMP2P_DBG(x...) do {                              \
	if (smp2p_get_debug_mask() & MSM_SMP2P_DEBUG) \
			SMP2P_IPC_LOG_STR(x);  \
} while (0)

#define SMP2P_INFO(x...) do {                              \
	if (smp2p_get_debug_mask() & MSM_SMP2P_INFO) \
			SMP2P_IPC_LOG_STR(x);  \
} while (0)

#define SMP2P_ERR(x...) do {                              \
	pr_err(x); \
	SMP2P_IPC_LOG_STR(x);  \
} while (0)

#define SMP2P_GPIO(x...) do {                              \
	if (smp2p_get_debug_mask() & MSM_SMP2P_GPIO) \
			SMP2P_IPC_LOG_STR(x);  \
} while (0)


enum msm_smp2p_edge_state {
	SMP2P_EDGE_STATE_CLOSED,
	SMP2P_EDGE_STATE_OPENING,
	SMP2P_EDGE_STATE_OPENED,
	SMP2P_EDGE_STATE_FAILED = 0xff,
};

/**
 * struct smp2p_smem - SMP2P SMEM Item Header
 *
 * @magic:  Set to "$SMP" -- used for identification / debug purposes
 * @feature_version:  Feature and version fields
 * @rem_loc_proc_id:  Remote (31:16) and Local (15:0) processor IDs
 * @valid_total_ent:  Valid (31:16) and total (15:0) entries
 * @flags:  Flags (bits 31:2 reserved)
 */
struct smp2p_smem {
	uint32_t magic;
	uint32_t feature_version;
	uint32_t rem_loc_proc_id;
	uint32_t valid_total_ent;
	uint32_t flags;
};

struct smp2p_entry_v1 {
	char name[SMP2P_MAX_ENTRY_NAME];
	uint32_t entry;
};

struct smp2p_smem_item {
	struct smp2p_smem header;
	struct smp2p_entry_v1 entries[SMP2P_MAX_ENTRY];
};

/* Mock object for internal loopback testing. */
struct msm_smp2p_remote_mock {
	struct smp2p_smem_item remote_item;
	int rx_interrupt_count;
	int (*rx_interrupt)(void);
	void (*tx_interrupt)(void);

	bool item_exists;
	bool initialized;
	struct completion cb_completion;
};

void smp2p_init_header(struct smp2p_smem *header_ptr, int local_pid,
		int remote_pid, uint32_t features, uint32_t version);
void *msm_smp2p_get_remote_mock(void);
int smp2p_remote_mock_rx_interrupt(void);
int smp2p_reset_mock_edge(void);
void msm_smp2p_interrupt_handler(int);
void msm_smp2p_set_remote_mock_exists(bool item_exists);
void *msm_smp2p_get_remote_mock_smem_item(uint32_t *size);
void *msm_smp2p_init_rmt_lpb_proc(int remote_pid);
int msm_smp2p_deinit_rmt_lpb_proc(int remote_pid);
void *smp2p_get_log_ctx(void);
int smp2p_get_debug_mask(void);

/* Inbound / outbound Interrupt configuration. */
struct smp2p_interrupt_config {
	bool is_configured;
	uint32_t *out_int_ptr;
	uint32_t out_int_mask;
	int in_int_id;
	const char *name;

	/* interrupt stats */
	unsigned in_interrupt_count;
	unsigned out_interrupt_count;
};

struct smp2p_interrupt_config *smp2p_get_interrupt_config(void);
const char *smp2p_pid_to_name(int remote_pid);
struct smp2p_smem *smp2p_get_in_item(int remote_pid);
struct smp2p_smem *smp2p_get_out_item(int remote_pid, int *state);
void smp2p_gpio_open_test_entry(const char *name, int remote_pid, bool do_open);
#endif
