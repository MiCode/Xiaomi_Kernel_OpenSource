// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#define pr_fmt(fmt) "ispv4 ramlog: " fmt

#include "ispv4_rproc.h"
#include <linux/mfd/ispv4_defs.h>
#include <linux/dma-map-ops.h>

#define MAX_LOG_SIZE (4096 * 16)
#define SINGLE_LOG_MAX_LEN 1024

static void log_mbox_callback(struct mbox_client *cl, void *mssg)
{
	static u8 buffer[MAX_LOG_SIZE + 1];
	u32 *data = (void *)((u8 *)mssg + 1);
	uintptr_t off = data[0];
	u32 len = data[1];
	static int last_tail = 0;
	int tail;
	int start = 0;
	bool cut = false;
	struct xm_ispv4_rproc *rp =
		container_of(cl, struct xm_ispv4_rproc, mbox_log);

	/* NOTE: Do not recv log */
	//return;

	pr_info("data offset: %lx, len = %d, lasttail=%d", off, len, last_tail);
	if (len > MAX_LOG_SIZE - last_tail || off >= rp->ramlog_buf_size) {
		pr_err("too long log or illegal offset max size=%d off=%lx\n",
		       MAX_LOG_SIZE - last_tail, rp->ramlog_buf_size);
		return;
	}

	tail = len + last_tail;
	memcpy_fromio(buffer + last_tail, (u8 *)rp->ramlog_dma + off, len);

	/* Append a string end flag */
	buffer[tail] = 0;

	/* An log max len == 1024 */
	while (start < tail) {
		/* Get next string len */
		int nlen = strnlen(buffer + start, SINGLE_LOG_MAX_LEN);
		if (start + nlen > tail) {
			pr_info("parse met error, %d %d %d\n", start, nlen,
				tail);
			break;
		}
		if (unlikely(start + nlen == tail)) {
			/* The data has been cut. */
			memcpy(buffer, buffer + start, nlen);
			last_tail = nlen;
			cut = true;
			break;
		}

		/* Print current log */
		printk("!{misp}: %s", buffer + start);
		/* The data has been cut. */
		start += nlen + 1;
	}

	if (!cut)
		last_tail = 0;
}

int ispv4_ramlog_init(struct xm_ispv4_rproc *xm_rp)
{
	xm_rp->mbox_log.dev = xm_rp->dev;
	xm_rp->mbox_log.tx_block = true;
	xm_rp->mbox_log.tx_tout = 1000;
	xm_rp->mbox_log.knows_txdone = false;
	xm_rp->mbox_log.rx_callback = log_mbox_callback;

	return 0;
}

void ispv4_ramlog_deinit(struct xm_ispv4_rproc *xm_rp)
{
}

int ispv4_ramlog_boot(struct xm_ispv4_rproc *xm_rp)
{
	dma_addr_t da;

	if (RP_SPI(xm_rp) || RP_FAKE(xm_rp))
		return 0;

	pr_info("pcie dma-coherent = %d\n",
		dev_is_dma_coherent(xm_rp->dev->parent));
	if (xm_rp->ramlog_buf != NULL) {
		xm_rp->ramlog_dma = dma_alloc_coherent(xm_rp->dev->parent,
						       xm_rp->ramlog_buf_size,
						       &da, GFP_KERNEL);
		if (IS_ERR_OR_NULL(xm_rp->ramlog_dma)) {
			return PTR_ERR(xm_rp->ramlog_dma);
		}
		pr_info("ramlog address da=%x\n", da);
		xm_rp->ramlog_dma_da = da;
	}

	xm_rp->mbox_log_chan = mbox_request_channel(&xm_rp->mbox_log, 2);
	if (IS_ERR_OR_NULL(xm_rp->mbox_log_chan)) {
		dev_err(xm_rp->dev, "request log mbox chan boot failed\n");
		dma_free_coherent(xm_rp->dev->parent, xm_rp->ramlog_buf_size,
				  xm_rp->ramlog_dma, xm_rp->ramlog_dma_da);
		return PTR_ERR(xm_rp->mbox_log_chan);
	}

	return 0;
}

void ispv4_ramlog_deboot(struct xm_ispv4_rproc *xm_rp)
{
	if (RP_SPI(xm_rp) || RP_FAKE(xm_rp))
		return;

	if (!IS_ERR_OR_NULL(xm_rp->mbox_log_chan))
		mbox_free_channel(xm_rp->mbox_log_chan);

	if (!IS_ERR_OR_NULL(xm_rp->ramlog_dma))
		dma_free_coherent(xm_rp->dev->parent, xm_rp->ramlog_buf_size,
				  xm_rp->ramlog_dma, xm_rp->ramlog_dma_da);
}
