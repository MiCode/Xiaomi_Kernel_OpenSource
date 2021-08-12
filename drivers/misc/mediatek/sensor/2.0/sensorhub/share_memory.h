/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SHARE_MEMORY_H_
#define _SHARE_MEMORY_H_

#include "scp.h"

enum share_mem_payload_type {
	SHARE_MEM_DATA_PAYLOAD_TYPE,
	SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE,
	SHARE_MEM_LIST_PAYLOAD_TYPE,
	SHARE_MEM_DEBUG_PAYLOAD_TYPE,
	SHARE_MEM_CUSTOM_W_PAYLOAD_TYPE,
	SHARE_MEM_CUSTOM_R_PAYLOAD_TYPE,
	MAX_SHARE_MEM_PAYLOAD_TYPE,
};

struct share_mem_data {
	uint8_t sensor_type;
	uint8_t action;
	uint8_t accurancy;
	uint8_t padding[1];
	int64_t timestamp;
	int32_t value[6] __aligned(4);
} __packed __aligned(4);

struct share_mem_super_data {
	uint8_t sensor_type;
	uint8_t action;
	uint8_t accurancy;
	uint8_t padding[1];
	int64_t timestamp;
	int32_t value[16] __aligned(4);
} __packed __aligned(4);

struct share_mem_debug {
	uint8_t sensor_type;
	uint8_t padding[3];
	uint32_t written;
	uint8_t buffer[4032] __aligned(4); //2048+1024+512+256+128+64
} __packed __aligned(4);

struct share_mem_info {
	uint8_t sensor_type;
	uint8_t padding[3];
	uint32_t gain;
	uint8_t name[16];
	uint8_t vendor[16];
} __packed __aligned(4);

struct share_mem_cmd {
	uint8_t command;
	uint8_t tx_len;
	uint8_t rx_len;
	uint8_t padding[1];
	int32_t data[15] __aligned(4);
} __packed __aligned(4);

struct share_mem_base {
	uint32_t rp;
	uint32_t wp;
	uint32_t buffer_size;
	uint32_t item_size;
	uint8_t data[0] __aligned(4);
} __packed __aligned(4);

struct share_mem {
	struct share_mem_base *base;

	struct mutex lock;
	uint32_t item_size;

	uint32_t write_position;
	uint32_t last_write_position;

	bool buffer_full_detect;
	uint8_t buffer_full_cmd;
	uint32_t buffer_full_written;
	uint32_t buffer_full_threshold;

	char *name;
};

struct share_mem_notify {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t notify_cmd;
};

struct share_mem_config {
	uint8_t payload_type;
	struct share_mem_base *base;
	uint32_t buffer_size;
};

int share_mem_seek(struct share_mem *shm, uint32_t write_position);
int share_mem_read_reset(struct share_mem *shm);
int share_mem_write_reset(struct share_mem *shm);
int share_mem_read(struct share_mem *shm, void *buf, uint32_t count);
int share_mem_write(struct share_mem *shm, void *buf, uint32_t count);
int share_mem_flush(struct share_mem *shm, struct share_mem_notify *notify);
int share_mem_init(struct share_mem *shm, struct share_mem_config *cfg);
int share_mem_config(void);
void share_mem_config_handler_register(uint8_t payload_type,
	int (*f)(struct share_mem_config *cfg, void *private_data),
	void *private_data);
void share_mem_config_handler_unregister(uint8_t payload_type);

#endif
