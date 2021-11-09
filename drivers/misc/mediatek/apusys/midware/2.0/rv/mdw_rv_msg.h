/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_RV_MSG__
#define __MTK_APU_MDW_RV_MSG__

#include "mdw.h"
/* mdw queue cmd type */
enum {
	MDW_IPI_NONE,
	MDW_IPI_APU_CMD,
	MDW_IPI_HANDSHAKE,
	MDW_IPI_PARAM,
	MDW_IPI_MAX = 0x20,
};

enum {
	MDW_IPI_HANDSHAKE_BASIC_INFO,
	MDW_IPI_HANDSHAKE_DEV_NUM,
	MDW_IPI_HANDSHAKE_TASK_NUM,
	MDW_IPI_HANDSHAKE_MEM_INFO,
};

enum {
	MDW_IPI_MSG_STATUS_OK,
	MDW_IPI_MSG_STATUS_BUSY,
	MDW_IPI_MSG_STATUS_ERR,
	MDW_IPI_MSG_STATUS_TIMEOUT,
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
} __attribute__((__packed__));

struct mdw_ipi_handshake {
	uint32_t h_id;
	union {
		struct {
			uint64_t magic;
			uint32_t version;
			uint64_t dev_bmp;
			uint64_t mem_bmp;
			uint64_t stat_iova;
			uint32_t stat_size;
		} basic;
		struct {
			uint32_t type;
			uint32_t num;
			uint8_t meta[MDW_DEV_META_SIZE];
		} dev;
		struct {
			uint32_t type;
			uint64_t start;
			uint32_t size;
		} mem;
		struct {
			uint32_t type;
			uint32_t norm_task_num;
			uint32_t deadline_task_num;
		} task;
	};
};

struct mdw_ipi_param {
	uint32_t type;
	uint32_t dir;
	uint32_t value;
};

struct mdw_stat {
	uint32_t task_num[APUSYS_DEVICE_LAST][MDW_QUEUE_MAX];
	uint32_t task_loading[APUSYS_DEVICE_LAST][MDW_QUEUE_MAX][2];
} __attribute__((__packed__));

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
