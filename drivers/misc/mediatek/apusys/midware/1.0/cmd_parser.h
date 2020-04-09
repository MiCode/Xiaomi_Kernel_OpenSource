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
#include <linux/time.h>
#include "apusys_drv.h"
#include "apusys_device.h"
#include "mdw_cmn.h"
#include "cmd_format.h"
#include "apusys_user.h"

struct apusys_subcmd;

enum {
	CMD_STATE_IDLE,
	CMD_STATE_READY,
	CMD_STATE_RUN,
	CMD_STATE_DONE,

	CMD_STATE_MAX,
};

enum {
	CMD_SCHED_NORMAL, // scheduler decide
	CMD_SCHED_FORCE_SINGLE,
	CMD_SCHED_FORCE_MULTI,
};

struct pack_collect {
	unsigned long *pack_status;
	struct list_head sc_list;
};

struct apusys_cmd {
	/* basic info */
	pid_t pid;
	pid_t tgid;
	int mem_fd;
	struct apusys_kmem *cmdbuf;
	uint64_t mem_hnd;
	uint64_t cmd_id;     // cmd unique id
	uint32_t cmdbuf_size;

	struct apusys_cmd_hdr *u_hdr;
	struct apusys_cmd_hdr *hdr;

	void *dp_entry;
	void *dp_cnt_entry;
	uint8_t power_save;  // power save flag, allow to downgrade opp
	int multicore_sched;

	int *pdr_cnt_list;

	/* flow control */
	/* apusys_subcmd */
	struct apusys_subcmd **sc_list;
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

	int cmd_ret;

	/* for thread sync */
	struct completion comp;  // for thread pool used
};

struct apusys_subcmd {
	/* basic information */
	int type;
	int idx;                  // subcmd idx

	struct apusys_sc_hdr_cmn *u_hdr; // u common header
	struct apusys_sc_hdr *c_hdr; // common header
	//void *d_hdr; // extend device header

	void *codebuf;
	unsigned int codebuf_iosize;
	int codebuf_fd;
	uint64_t codebuf_mem_hnd;

	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t bw;
	uint32_t tcm_real_usage;
	uint32_t boost_val;       // boost value
	uint32_t ctx_id;          // allocated from mem mgt
	uint32_t pack_id;

	struct apusys_cmd *par_cmd; // apusys_cmd ptr

	int scr_num;
	unsigned int *scr_list;

	//unsigned long *dp_status; // dependency status
	int state;
	uint32_t exec_core_num;
	uint64_t exec_core_bitmap;

	struct mutex mtx;

	/* control use */
	//struct list_head ce_list; // apusys cmd
	struct list_head q_list;  // priority queue
	struct list_head pc_list; // pack cmd
	struct rb_node node; // deadline queue
	uint64_t period;
	uint64_t deadline;
	uint64_t runtime;
	int cluster_size; // cluster size of preemption
};

/* general functions */
uint32_t get_time_diff_from_system(struct timeval *duration);
uint8_t get_cmdformat_version(void);
uint64_t get_cmdformat_magic(void);

/* apusys cmd parse functions */
int apusys_cmd_create(int mem_fd, uint32_t offset,
	struct apusys_cmd **icmd, struct apusys_user *u);
int apusys_cmd_delete(struct apusys_cmd *cmd);
uint64_t get_subcmd_by_idx(const struct apusys_cmd *cmd, int idx);
int check_sc_ready(const struct apusys_cmd *cmd, int idx);
int get_sc_tcm_usage(struct apusys_subcmd *sc);
int check_cmd_done(struct apusys_cmd *cmd);
void decrease_pdr_cnt(struct apusys_cmd *cmd, int idx);

/* subcmd parse functions */
int apusys_subcmd_create(int idx, struct apusys_cmd *cmd,
	struct apusys_subcmd **isc, unsigned int scr_ofs);
int apusys_subcmd_delete(struct apusys_subcmd *sc);

#endif
