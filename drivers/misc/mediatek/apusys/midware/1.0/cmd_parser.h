/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_CMD_PARSER_H__
#define __APUSYS_CMD_PARSER_H__

#include <linux/mutex.h>
#include <linux/completion.h>
#include "apusys_drv.h"
#include "apusys_device.h"

enum {
	CMD_STATE_IDLE,
	CMD_STATE_READY,
	CMD_STATE_RUN,
	CMD_STATE_DONE,

	CMD_STATE_MAX,
};

struct pack_mgt {
	uint16_t *pack_ref;
	void **sc_list;
};

struct pack_collect {
	unsigned long *pack_status;
	struct list_head sc_list;
};

struct apusys_cmd {
	/* basic info */
	int mem_fd;
	uint64_t mem_hnd;

	void *kva;           // apusys cmd kernel va entry
	uint64_t cmd_uid;    // cmd id from user
	uint64_t cmd_id;      // cmd unique id
	uint32_t size;        // total apusys cmd size
	uint32_t sc_num;      // number of subcmds
	void *sc_list_entry; // subcmd list entry
	void *dp_entry;      // dependency list bitmp [BITS_TO_LONGS(sc_num)]
	uint8_t priority;     // cmd priority

	uint32_t target_time;   // deadline
	uint32_t estimate_time; // execution time from trail run

	/* flow control */
	/* apusys_subcmd */
	struct mutex sc_mtx;
	/* apusys_subcmd */
	struct list_head sc_list;
	/* subcmd status bitmap [BITS_TO_LONGS(sc_num)] */
	unsigned long *sc_status;

	/* pack cmd id control */
	/* pack_status list bitmp [BITS_TO_LONGS(sc_num)] */
	struct pack_collect pc_col;

	/* ctx ref count */
	uint32_t *ctx_ref;
	uint32_t *ctx_list;

	int state;       // cmd state

	struct list_head u_list; // apusys user list
	struct mutex mtx;

	uint32_t start_ms;
	uint32_t time_budget;

	int cmd_ret;

	/* for thread sync */
	struct completion comp;  // for thread pool used
};

struct apusys_subcmd {
	/* basic information */
	int type;                  // device type
	void *entry;              // subcmd info entry
	void *parent_cmd;         // apusys_cmd ptr
	int idx;                   // subcmd idx
	unsigned long *dp_status; // dependency status
	uint64_t d_time; // driver time
	uint64_t u_time; // up time

	/* execute information */
	uint32_t boost_val;
	unsigned long exec_ms;
	int state;

	uint32_t pack_idx;
	uint32_t ctx_group; // from user
	uint32_t ctx_id;  // allocated from mem mgt

	uint64_t bw;

	struct mutex mtx;

	/* control use */
	struct list_head ce_list; // apusys cmd
	struct list_head q_list;  // priority queue
	struct list_head pc_list; // pack cmd
};

uint64_t get_time_from_system(void);
int set_utime_to_subcmd(void *sc_entry, uint64_t us);
int set_dtime_to_subcmd(void *sc_entry, uint64_t us);

inline uint8_t get_cmdformat_version(void);
inline uint64_t get_cmdformat_magic(void);

uint64_t get_subcmd_by_idx(struct apusys_cmd *cmd, int idx);
uint32_t get_type_from_subcmd(void *sc_entry);
uint32_t get_ctxid_from_subcmd(void *sc_entry);
uint32_t get_size_from_subcmd(void *sc_entry);
uint64_t get_addr_from_subcmd(void *sc_entry);
uint32_t get_packid_from_subcmd(void *sc_entry, int type);

int apusys_subcmd_create(void *sc_entry,
	struct apusys_cmd *cmd, struct apusys_subcmd **isc);
int apusys_subcmd_delete(struct apusys_subcmd *sc);

int apusys_cmd_create(int mem_fd, uint32_t offset,
	struct apusys_cmd **icmd);
int apusys_cmd_delete(struct apusys_cmd *cmd);

#endif
