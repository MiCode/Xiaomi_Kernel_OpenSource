/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __MMC_RING_BUFFER__
#define __MMC_RING_BUFFER__

#include <linux/mmc/card.h>
#include <linux/smp.h>

#include "core.h"

#define MMC_TRACE_RBUF_SZ_ORDER	2	/* 2^2 pages */
#define MMC_TRACE_RBUF_SZ	(PAGE_SIZE * (1 << MMC_TRACE_RBUF_SZ_ORDER))
#define MMC_TRACE_EVENT_SZ	256
#define MMC_TRACE_RBUF_NUM_EVENTS	(MMC_TRACE_RBUF_SZ / MMC_TRACE_EVENT_SZ)

struct mmc_host;
struct mmc_trace_buffer {
	int	wr_idx;
	bool stop_tracing;
	spinlock_t trace_lock;
	char *data;
};

#ifdef CONFIG_MMC_RING_BUFFER
void mmc_stop_tracing(struct mmc_host *mmc);
void mmc_trace_write(struct mmc_host *mmc, const char *fmt, ...);
void mmc_trace_init(struct mmc_host *mmc);
void mmc_trace_free(struct mmc_host *mmc);
void mmc_dump_trace_buffer(struct mmc_host *mmc, struct seq_file *s);
#else
static inline void mmc_stop_tracing(struct mmc_host *mmc) {}
static inline void mmc_trace_write(struct mmc_host *mmc,
		const char *fmt, ...) {}
static inline void mmc_trace_init(struct mmc_host *mmc) {}
static inline void mmc_trace_free(struct mmc_host *mmc) {}
static inline void mmc_dump_trace_buffer(struct mmc_host *mmc,
		struct seq_file *s) {}
#endif

#define MMC_TRACE(mmc, fmt, ...) \
		mmc_trace_write(mmc, fmt, ##__VA_ARGS__)

#endif /* __MMC_RING_BUFFER__ */
