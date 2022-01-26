/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APUSYS_MDW_CMD_H__
#define __APUSYS_MDW_CMD_H__

#include <linux/file.h>
#include <linux/kref.h>
#include "apu_cmd.h"
#include "mdw_usr.h"
#include "apusys_device.h"

#define MDW_CMD_SC_MAX 64
#define MDW_CMD_PRIO_MAX 32

enum {
	MDW_CMD_STATE_NORMAL,
	MDW_CMD_STATE_ABORT,
};

struct mdw_apu_cmd {
	struct apu_cmd_hdr *u_hdr; // from user
	struct apu_cmd_hdr *hdr; // copied
	struct apu_fence_hdr *uf_hdr; // from user
	struct file *file; // Fence sync file
	uint32_t size;
	int id;
	uint64_t kid;
	struct apusys_kmem *cmdbuf;

	pid_t pid;
	pid_t tgid;

	uint8_t multi;

	uint64_t sc_status_bmp; // bitmap of sc status
	struct list_head sc_list; // non-finished subcmd list, to mdw_apu_sc
	struct kref ref;
	uint32_t parsed_sc_num; // to record parsed sc num

	uint8_t pdr_cnt[MDW_CMD_SC_MAX]; // predecessor num
	uint8_t ctx_cnt[MDW_CMD_SC_MAX]; // ctx count
	uint8_t ctx_repo[MDW_CMD_SC_MAX]; // ctx tmp storage
	uint8_t pack_cnt[MDW_CMD_SC_MAX]; // pack count
	struct list_head di_list; //for dispr item

	int state;

	struct mdw_usr *usr; // usr
	struct mutex mtx;
	struct completion cmplt;

	/* perf info */
	struct timespec ts_create;
	struct timespec ts_delete;
};

struct mdw_apu_sc {
	struct apu_sc_hdr_cmn *u_hdr; // from user
	struct apu_sc_hdr_cmn *hdr; // copied
	void *d_hdr;
	struct mdw_apu_cmd *parent;
	uint64_t kva;
	uint32_t size;
	uint32_t type;
	struct apusys_kmem buf;

	int idx;

	struct list_head cmd_item; // to mdw_apu_cmd
	uint32_t pdr_num;
	uint64_t scr_bmp;
	int state;
	unsigned long ctx;
	uint32_t real_tcm_usage;
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t bw;
	uint32_t boost;
	int status;

	/* multi */
	uint8_t multi_total;
	uint64_t multi_bmp;
	struct kref multi_ref;

	struct list_head ds_item; // to done sc q
	struct list_head di_item; // to dispr item

	struct mutex mtx;

	/* norm used */
	struct list_head q_item; // to pid's priority q

	/* deadline used */
	struct rb_node node; // deadline queue
	uint64_t period;
	uint64_t deadline;
	uint64_t runtime;
	int cluster_size; // cluster size of preemption

	/* perf info */
	struct timespec ts_create;
	struct timespec ts_enque;
	struct timespec ts_deque;
	struct timespec ts_start;
	struct timespec ts_end;
	struct timespec ts_delete;
};

struct mdw_cmd_parser {
	struct mdw_apu_cmd *(*create_cmd)(int fd, uint32_t size, uint32_t ofs,
			struct mdw_usr *c);
	int (*delete_cmd)(struct mdw_apu_cmd *c);
	int (*abort_cmd)(struct mdw_apu_cmd *c);
	int (*parse_cmd)(struct mdw_apu_cmd *c, struct mdw_apu_sc **out);
	int (*end_sc)(struct mdw_apu_sc *in, struct mdw_apu_sc **out);
	int (*get_ctx)(struct mdw_apu_sc *sc);
	void (*put_ctx)(struct mdw_apu_sc *sc);
	int (*exec_core_num)(struct mdw_apu_sc *sc);
	int (*set_hnd)(struct mdw_apu_sc *sc, int d_idx, void *h);
	void (*clr_hnd)(struct mdw_apu_sc *sc, void *h);
	bool (*is_deadline)(struct mdw_apu_sc *sc);
};
struct mdw_cmd_parser *mdw_cmd_get_parser(void);

uint64_t mdw_cmd_get_magic(void);
uint32_t mdw_cmd_get_ver(void);
int mdw_wait_cmd(struct mdw_usr *u, struct mdw_apu_cmd *c);

#endif
