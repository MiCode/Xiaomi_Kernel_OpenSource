/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
#ifndef HAB_PIPE_H
#define HAB_PIPE_H

struct hab_shared_buf {
	uint32_t rd_count; /* volatile cannot be used here */
	uint32_t wr_count; /* volatile cannot be used here */
	uint32_t size;
	unsigned char data[]; /* volatile cannot be used here */
};

/* debug only */
struct dbg_item {
	uint32_t rd_cnt;
	uint32_t wr_cnt;
	void *va; /* local for read or write */
	uint32_t index; /* local */
	uint32_t sz; /* size in */
	uint32_t ret; /* actual bytes read */
};

#define DBG_ITEM_SIZE 20

struct dbg_items {
	struct dbg_item it[DBG_ITEM_SIZE];
	int idx;
};

struct hab_pipe_endpoint {
	struct {
		uint32_t wr_count;
		uint32_t index;
		struct hab_shared_buf *sh_buf;
	} tx_info;
	struct {
		uint32_t index;
		struct hab_shared_buf *sh_buf;
	} rx_info;
};

struct hab_pipe {
	struct hab_pipe_endpoint top;
	struct hab_pipe_endpoint bottom;

	/* For debugging only */
	struct hab_shared_buf *buf_a; /* top TX, bottom RX */
	struct hab_shared_buf *buf_b; /* top RX, bottom TX */
	size_t total_size;

	unsigned char buf_base[];
};

size_t hab_pipe_calc_required_bytes(uint32_t shared_buf_size);

struct hab_pipe_endpoint *hab_pipe_init(struct hab_pipe *pipe,
		uint32_t shared_buf_size, int top);

uint32_t hab_pipe_write(struct hab_pipe_endpoint *ep,
		unsigned char *p, uint32_t num_bytes);

void hab_pipe_write_commit(struct hab_pipe_endpoint *ep);

uint32_t hab_pipe_read(struct hab_pipe_endpoint *ep,
		unsigned char *p, uint32_t size, uint32_t clear);

/* debug only */
void hab_pipe_rxinfo(struct hab_pipe_endpoint *ep, uint32_t *rd_cnt,
		uint32_t *wr_cnt, uint32_t *idx);

#endif /* HAB_PIPE_H */
