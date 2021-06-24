/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_MSG__
#define __APUSYS_MDW_MSG__

/* mdw queue cmd type */
enum {
	MDW_IPI_NONE,
	MDW_IPI_APU_CMD,
	MDW_IPI_HANDSHAKE,
	MDW_IPI_PARAM,
	MDW_IPI_USER,
	MDW_IPI_MAX = 0x20,
};

enum {
	MDW_IPI_HANDSHAKE_BASIC_INFO,
	MDW_IPI_HANDSHAKE_DEV_NUM,
};

enum {
	MDW_IPI_MSG_STATUS_OK,
	MDW_IPI_MSG_STATUS_BUSY,
	MDW_IPI_MSG_STATUS_ERR,
};

struct mdw_ipi_ucmd {
	uint32_t dev_type;
	uint32_t dev_idx;
	uint64_t iova;
	uint32_t size;
};

struct mdw_ipi_apu_cmd {
	uint64_t start_ts_ns; // cmd time
	uint64_t iova;
	uint32_t size;
	uint32_t status;
};

struct mdw_ipi_handshake {
	uint32_t h_id;
	union {
		struct {
			uint64_t magic;
			uint32_t version;
			uint64_t dev_bmp;
		} basic;
		struct {
			uint32_t type;
			uint32_t num;
			uint8_t meta[MDW_DEV_META_SIZE];
		} dev;
	};
};

struct mdw_ipi_param {
	uint32_t uplog;
	uint32_t preempt_policy;
	uint32_t sched_policy;
};

struct mdw_ipi_msg {
	uint64_t sync_id;
	uint32_t id; //ipi id
	int32_t ret;
	union {
		struct mdw_ipi_apu_cmd c;
		struct mdw_ipi_handshake h;
		struct mdw_ipi_param p;
		struct mdw_ipi_ucmd u;
	};
} __attribute__((__packed__));

struct mdw_ipi_msg_sync {
	struct mdw_ipi_msg msg;
	struct list_head ud_item;
	struct completion cmplt;
	void (*complete)(struct mdw_ipi_msg_sync *s_msg);
};

#endif
