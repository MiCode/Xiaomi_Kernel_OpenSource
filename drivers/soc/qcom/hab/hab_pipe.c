/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "hab.h"
#include "hab_pipe.h"

size_t hab_pipe_calc_required_bytes(uint32_t shared_buf_size)
{
	return sizeof(struct hab_pipe)
		+ (2 * (sizeof(struct hab_shared_buf) + shared_buf_size));
}

struct hab_pipe_endpoint *hab_pipe_init(struct hab_pipe *pipe,
		uint32_t shared_buf_size, int top)
{
	struct hab_pipe_endpoint *ep = NULL;
	struct hab_shared_buf *buf_a;
	struct hab_shared_buf *buf_b;

	if (!pipe)
		return NULL;

	buf_a = (struct hab_shared_buf *) pipe->buf_base;
	buf_b = (struct hab_shared_buf *) (pipe->buf_base
		+ sizeof(struct hab_shared_buf) + shared_buf_size);

	if (top) {
		ep = &pipe->top;
		memset(ep, 0, sizeof(*ep));
		ep->tx_info.sh_buf = buf_a;
		ep->rx_info.sh_buf = buf_b;
	} else {
		ep = &pipe->bottom;
		memset(ep, 0, sizeof(*ep));
		ep->tx_info.sh_buf = buf_b;
		ep->rx_info.sh_buf = buf_a;
		memset(ep->tx_info.sh_buf, 0, sizeof(struct hab_shared_buf));
		memset(ep->rx_info.sh_buf, 0, sizeof(struct hab_shared_buf));
		ep->tx_info.sh_buf->size = shared_buf_size;
		ep->rx_info.sh_buf->size = shared_buf_size;

		pipe->buf_a = buf_a;
		pipe->buf_b = buf_b;
		pipe->total_size =
			hab_pipe_calc_required_bytes(shared_buf_size);
	}
	return ep;
}

uint32_t hab_pipe_write(struct hab_pipe_endpoint *ep,
		unsigned char *p, uint32_t num_bytes)
{
	struct hab_shared_buf *sh_buf = ep->tx_info.sh_buf;
	uint32_t space =
		(sh_buf->size - (ep->tx_info.wr_count - sh_buf->rd_count));
	uint32_t count1, count2;

	if (!p || num_bytes > space || num_bytes == 0)
		return 0;

	count1 = (num_bytes <= (sh_buf->size - ep->tx_info.index)) ? num_bytes :
		(sh_buf->size - ep->tx_info.index);
	count2 = num_bytes - count1;

	if (count1 > 0) {
		memcpy(&sh_buf->data[ep->tx_info.index], p, count1);
		ep->tx_info.wr_count += count1;
		ep->tx_info.index += count1;
		if (ep->tx_info.index >= sh_buf->size)
			ep->tx_info.index = 0;
	}
	if (count2 > 0) {/* handle buffer wrapping */
		memcpy(&sh_buf->data[ep->tx_info.index], p + count1, count2);
		ep->tx_info.wr_count += count2;
		ep->tx_info.index += count2;
		if (ep->tx_info.index >= sh_buf->size)
			ep->tx_info.index = 0;
	}
	return num_bytes;
}

/* Updates the write index which is shared with the other VM */
void hab_pipe_write_commit(struct hab_pipe_endpoint *ep)
{
	struct hab_shared_buf *sh_buf = ep->tx_info.sh_buf;

	mb(); /* Must commit data before incrementing count */
	sh_buf->wr_count = ep->tx_info.wr_count;
}

uint32_t hab_pipe_read(struct hab_pipe_endpoint *ep,
		unsigned char *p, uint32_t size)
{
	struct hab_shared_buf *sh_buf = ep->rx_info.sh_buf;
	uint32_t avail = sh_buf->wr_count - sh_buf->rd_count;
	uint32_t count1, count2, to_read;

	if (!p || avail == 0 || size == 0)
		return 0;

	to_read = (avail < size) ? avail : size;
	count1 = (to_read <= (sh_buf->size - ep->rx_info.index)) ? to_read :
		(sh_buf->size - ep->rx_info.index);
	count2 = to_read - count1;

	if (count1 > 0) {
		memcpy(p, &sh_buf->data[ep->rx_info.index], count1);
		ep->rx_info.index += count1;
		if (ep->rx_info.index >= sh_buf->size)
			ep->rx_info.index = 0;
		mb(); /*Must commit data before incremeting count*/
		sh_buf->rd_count += count1;
	}
	if (count2 > 0) { /* handle buffer wrapping */
		memcpy(p + count1, &sh_buf->data[ep->rx_info.index], count2);
		ep->rx_info.index += count2;
		mb(); /*Must commit data before incremeting count*/
		sh_buf->rd_count += count2;
	}

	return to_read;
}
