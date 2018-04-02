/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"sde_dbg:[%s] " fmt, __func__

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>

#include "sde_dbg.h"
#include "sde_trace.h"

bool sde_evtlog_is_enabled(struct sde_dbg_evtlog *evtlog, u32 flag)
{
	if (!evtlog)
		return false;

	return (flag & evtlog->enable) ||
		(flag == SDE_EVTLOG_ALL && evtlog->enable);
}

void sde_evtlog_log(struct sde_dbg_evtlog *evtlog, const char *name, int line,
		int flag, ...)
{
	unsigned long flags;
	int i, val = 0;
	va_list args;
	struct sde_dbg_evtlog_log *log;

	if (!evtlog)
		return;

	if (!sde_evtlog_is_enabled(evtlog, flag))
		return;

	spin_lock_irqsave(&evtlog->spin_lock, flags);
	log = &evtlog->logs[evtlog->curr];
	log->time = ktime_to_us(ktime_get());
	log->name = name;
	log->line = line;
	log->data_cnt = 0;
	log->pid = current->pid;

	va_start(args, flag);
	for (i = 0; i < SDE_EVTLOG_MAX_DATA; i++) {

		val = va_arg(args, int);
		if (val == SDE_EVTLOG_DATA_LIMITER)
			break;

		log->data[i] = val;
	}
	va_end(args);
	log->data_cnt = i;
	evtlog->curr = (evtlog->curr + 1) % SDE_EVTLOG_ENTRY;
	evtlog->last++;

	trace_sde_evtlog(name, line, i > 0 ? log->data[0] : 0,
			i > 1 ? log->data[1] : 0);

	spin_unlock_irqrestore(&evtlog->spin_lock, flags);
}

/* always dump the last entries which are not dumped yet */
static bool _sde_evtlog_dump_calc_range(struct sde_dbg_evtlog *evtlog,
	bool update_last_entry)
{
	bool need_dump = true;
	unsigned long flags;

	if (!evtlog)
		return false;

	spin_lock_irqsave(&evtlog->spin_lock, flags);

	evtlog->first = evtlog->next;

	if (update_last_entry)
		evtlog->last_dump = evtlog->last;

	if (evtlog->last_dump == evtlog->first) {
		need_dump = false;
		goto dump_exit;
	}

	if (evtlog->last_dump < evtlog->first) {
		evtlog->first %= SDE_EVTLOG_ENTRY;
		if (evtlog->last_dump < evtlog->first)
			evtlog->last_dump += SDE_EVTLOG_ENTRY;
	}

	if ((evtlog->last_dump - evtlog->first) > SDE_EVTLOG_PRINT_ENTRY) {
		pr_info("evtlog skipping %d entries, last=%d\n",
			evtlog->last_dump - evtlog->first -
			SDE_EVTLOG_PRINT_ENTRY,
			evtlog->last_dump - 1);
		evtlog->first = evtlog->last_dump - SDE_EVTLOG_PRINT_ENTRY;
	}
	evtlog->next = evtlog->first + 1;

dump_exit:
	spin_unlock_irqrestore(&evtlog->spin_lock, flags);

	return need_dump;
}

ssize_t sde_evtlog_dump_to_buffer(struct sde_dbg_evtlog *evtlog,
		char *evtlog_buf, ssize_t evtlog_buf_size,
		bool update_last_entry)
{
	int i;
	ssize_t off = 0;
	struct sde_dbg_evtlog_log *log, *prev_log;
	unsigned long flags;

	if (!evtlog || !evtlog_buf)
		return 0;

	/* update markers, exit if nothing to print */
	if (!_sde_evtlog_dump_calc_range(evtlog, update_last_entry))
		return 0;

	spin_lock_irqsave(&evtlog->spin_lock, flags);

	log = &evtlog->logs[evtlog->first % SDE_EVTLOG_ENTRY];

	prev_log = &evtlog->logs[(evtlog->first - 1) %
		SDE_EVTLOG_ENTRY];

	off = snprintf((evtlog_buf + off), (evtlog_buf_size - off), "%s:%-4d",
		log->name, log->line);

	if (off < SDE_EVTLOG_BUF_ALIGN) {
		memset((evtlog_buf + off), 0x20, (SDE_EVTLOG_BUF_ALIGN - off));
		off = SDE_EVTLOG_BUF_ALIGN;
	}

	off += snprintf((evtlog_buf + off), (evtlog_buf_size - off),
		"=>[%-8d:%-11llu:%9llu][%-4d]:", evtlog->first,
		log->time, (log->time - prev_log->time), log->pid);

	for (i = 0; i < log->data_cnt; i++)
		off += snprintf((evtlog_buf + off), (evtlog_buf_size - off),
			"%x ", log->data[i]);

	off += snprintf((evtlog_buf + off), (evtlog_buf_size - off), "\n");

	spin_unlock_irqrestore(&evtlog->spin_lock, flags);

	return off;
}

void sde_evtlog_dump_all(struct sde_dbg_evtlog *evtlog)
{
	char buf[SDE_EVTLOG_BUF_MAX];
	bool update_last_entry = true;

	if (!evtlog)
		return;

	while (sde_evtlog_dump_to_buffer(evtlog, buf, sizeof(buf),
		update_last_entry)) {
		pr_info("%s", buf);
		update_last_entry = false;
	}
}

struct sde_dbg_evtlog *sde_evtlog_init(void)
{
	struct sde_dbg_evtlog *evtlog;

	evtlog = kzalloc(sizeof(*evtlog), GFP_KERNEL);
	if (!evtlog)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&evtlog->spin_lock);
	evtlog->enable = SDE_EVTLOG_DEFAULT_ENABLE;

	return evtlog;
}

void sde_evtlog_destroy(struct sde_dbg_evtlog *evtlog)
{
	kfree(evtlog);
}
