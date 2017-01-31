/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mmc/ring_buffer.h>
#include <linux/mmc/host.h>

void mmc_stop_tracing(struct mmc_host *mmc)
{
	mmc->trace_buf.stop_tracing = true;
}

void mmc_trace_write(struct mmc_host *mmc,
			const char *fmt, ...)
{
	unsigned int idx;
	va_list args;
	char *event;
	unsigned long flags;
	char str[MMC_TRACE_EVENT_SZ];

	if (unlikely(!mmc->trace_buf.data) ||
			unlikely(mmc->trace_buf.stop_tracing))
		return;

	/*
	 * Here an increment and modulus is used to keep
	 * index within array bounds. The cast to unsigned is
	 * necessary so increment and rolover wraps to 0 correctly
	 */
	spin_lock_irqsave(&mmc->trace_buf.trace_lock, flags);
	mmc->trace_buf.wr_idx += 1;
	idx = ((unsigned int)mmc->trace_buf.wr_idx) &
			(MMC_TRACE_RBUF_NUM_EVENTS - 1);
	spin_unlock_irqrestore(&mmc->trace_buf.trace_lock, flags);

	/* Catch some unlikely machine specific wrap-around bug */
	if (unlikely(idx > (MMC_TRACE_RBUF_NUM_EVENTS - 1))) {
		pr_err("%s: %s: Invalid idx:%d for mmc trace, tracing stopped !\n",
			mmc_hostname(mmc), __func__, idx);
		mmc_stop_tracing(mmc);
		return;
	}

	event = &mmc->trace_buf.data[idx * MMC_TRACE_EVENT_SZ];
	va_start(args, fmt);
	snprintf(str, MMC_TRACE_EVENT_SZ, "<%d> %lld: %s: %s",
		raw_smp_processor_id(),
		ktime_to_ns(ktime_get()),
		mmc_hostname(mmc), fmt);
	memset(event, '\0', MMC_TRACE_EVENT_SZ);
	vscnprintf(event, MMC_TRACE_EVENT_SZ, str, args);
	va_end(args);
}

void mmc_trace_init(struct mmc_host *mmc)
{
	BUILD_BUG_ON_NOT_POWER_OF_2(MMC_TRACE_RBUF_NUM_EVENTS);

	mmc->trace_buf.data = (char *)
				__get_free_pages(GFP_KERNEL|__GFP_ZERO,
				MMC_TRACE_RBUF_SZ_ORDER);

	if (!mmc->trace_buf.data) {
		pr_err("%s: %s: Unable to allocate trace for mmc\n",
			__func__, mmc_hostname(mmc));
		return;
	}

	spin_lock_init(&mmc->trace_buf.trace_lock);
	mmc->trace_buf.wr_idx = -1;
}

void mmc_trace_free(struct mmc_host *mmc)
{
	if (mmc->trace_buf.data)
		free_pages((unsigned long)mmc->trace_buf.data,
			MMC_TRACE_RBUF_SZ_ORDER);
}

void mmc_dump_trace_buffer(struct mmc_host *mmc, struct seq_file *s)
{
	unsigned int idx, cur_idx;
	unsigned int N = MMC_TRACE_RBUF_NUM_EVENTS - 1;
	char *event;
	unsigned long flags;

	if (!mmc->trace_buf.data)
		return;

	spin_lock_irqsave(&mmc->trace_buf.trace_lock, flags);
	idx = ((unsigned int)mmc->trace_buf.wr_idx) & N;
	cur_idx = (idx + 1) & N;

	do {
		event = &mmc->trace_buf.data[cur_idx * MMC_TRACE_EVENT_SZ];
		if (s)
			seq_printf(s, "%s", (char *)event);
		else
			pr_err("%s", (char *)event);
		cur_idx = (cur_idx + 1) & N;
		if (cur_idx == idx) {
			event =
			  &mmc->trace_buf.data[cur_idx * MMC_TRACE_EVENT_SZ];
			if (s)
				seq_printf(s, "latest_event: %s",
					(char *)event);
			else
				pr_err("latest_event: %s", (char *)event);
			break;
		}
	} while (1);
	spin_unlock_irqrestore(&mmc->trace_buf.trace_lock, flags);
}
