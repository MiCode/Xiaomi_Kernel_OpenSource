// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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

	/* debug only */
	struct dbg_items *its = kzalloc(sizeof(struct dbg_items), GFP_KERNEL);

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

	pipe->buf_a = (struct hab_shared_buf *)its;
	return ep;
}

uint32_t hab_pipe_write(struct hab_pipe_endpoint *ep,
		unsigned char *p, uint32_t num_bytes)
{
	struct hab_shared_buf *sh_buf = ep->tx_info.sh_buf;
	uint32_t space =
		(sh_buf->size - (ep->tx_info.wr_count - sh_buf->rd_count));
	uint32_t count1, count2;

	if (!p || num_bytes > space || num_bytes == 0) {
		pr_err("****can not write to pipe p %pK to-write %d space available %d\n",
			p, num_bytes, space);
		return 0;
	}

	asm volatile("dmb ish" ::: "memory");

	count1 = (num_bytes <= (sh_buf->size - ep->tx_info.index)) ? num_bytes :
		(sh_buf->size - ep->tx_info.index);
	count2 = num_bytes - count1;

	if (count1 > 0) {
		memcpy((void *)&sh_buf->data[ep->tx_info.index], p, count1);
		ep->tx_info.wr_count += count1;
		ep->tx_info.index += count1;
		if (ep->tx_info.index >= sh_buf->size)
			ep->tx_info.index = 0;
	}
	if (count2 > 0) {/* handle buffer wrapping */
		memcpy((void *)&sh_buf->data[ep->tx_info.index],
			p + count1, count2);
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

	/* Must commit data before incrementing count */
	asm volatile("dmb ishst" ::: "memory");
	sh_buf->wr_count = ep->tx_info.wr_count;
}

#define HAB_HEAD_CLEAR     0xCC

uint32_t hab_pipe_read(struct hab_pipe_endpoint *ep,
		unsigned char *p, uint32_t size, uint32_t clear)
{
	struct hab_shared_buf *sh_buf = ep->rx_info.sh_buf;
	/* mb to guarantee wr_count is updated after contents are written */
	uint32_t avail = sh_buf->wr_count - sh_buf->rd_count;
	uint32_t count1, count2, to_read;
	uint32_t index_saved = ep->rx_info.index; /* store original for retry */

	if (!p || avail == 0 || size == 0)
		return 0;

	asm volatile("dmb ishld" ::: "memory");
	/* error if available is less than size and available is not zero */
	to_read = (avail < size) ? avail : size;

	if (to_read < size) /* only provide exact read size, not less */
		pr_err("less data available %d than requested %d\n",
			avail, size);

	count1 = (to_read <= (sh_buf->size - ep->rx_info.index)) ? to_read :
		(sh_buf->size - ep->rx_info.index);
	count2 = to_read - count1;

	if (count1 > 0) {
		memcpy(p, (void *)&sh_buf->data[ep->rx_info.index], count1);
		ep->rx_info.index += count1;
		if (ep->rx_info.index >= sh_buf->size)
			ep->rx_info.index = 0;
	}
	if (count2 > 0) { /* handle buffer wrapping */
		memcpy(p + count1, (void *)&sh_buf->data[ep->rx_info.index],
			count2);
		ep->rx_info.index += count2;
	}


	if (count1 + count2) {
		struct hab_header *head = (struct hab_header *)p;
		int retry_cnt = 0;

		if (clear && (size == sizeof(*head))) {
retry:

			if (unlikely(head->signature != 0xBEE1BEE1)) {
				pr_debug("hab head corruption detected at %pK buf %pK %08X %08X %08X %08X rd %d wr %d index %X saved %X retry %d\n",
					head, &sh_buf->data[0],
					head->id_type_size, head->session_id,
					head->signature, head->sequence,
					sh_buf->rd_count, sh_buf->wr_count,
					ep->rx_info.index, index_saved,
					retry_cnt);
				if (retry_cnt++ <= 1000) {
					memcpy(p, &sh_buf->data[index_saved],
						count1);
					if (count2)
						memcpy(&p[count1],
				&sh_buf->data[ep->rx_info.index - count2],
						count2);
					goto retry;
				} else
					pr_err("quit retry after %d time may fail %X %X %X %X rd %d wr %d index %X\n",
						retry_cnt, head->id_type_size,
						head->session_id,
						head->signature,
						head->sequence,
						sh_buf->rd_count,
						sh_buf->wr_count,
						ep->rx_info.index);
			}
		}

		/*Must commit data before incremeting count*/
		asm volatile("dmb ish" ::: "memory");
		sh_buf->rd_count += count1 + count2;
	}
	return to_read;
}

void hab_pipe_rxinfo(struct hab_pipe_endpoint *ep, uint32_t *rd_cnt,
					uint32_t *wr_cnt, uint32_t *idx)
{
	struct hab_shared_buf *sh_buf = ep->rx_info.sh_buf;

	*idx = ep->rx_info.index;
	*rd_cnt = sh_buf->rd_count;
	*wr_cnt = sh_buf->wr_count;
}
