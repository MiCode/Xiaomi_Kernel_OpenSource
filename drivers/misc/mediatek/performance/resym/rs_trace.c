/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

#include "rs_trace.h"

#define LOGSIZE 32

static unsigned long __read_mostly mark_addr;

void __rs_systrace_c(pid_t pid, int val, const char *fmt, ...)
{
	char log[LOGSIZE];
	va_list args;

	if (unlikely(!mark_addr))
		return;

	va_start(args, fmt);
	vsnprintf(log, LOGSIZE, fmt, args);
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%d\n", pid, log, val);
	preempt_enable();
}

void __rs_systrace_c_uint64(pid_t pid, uint64_t val, const char *fmt, ...)
{
	char log[LOGSIZE];
	va_list args;

	if (unlikely(!mark_addr))
		return;

	va_start(args, fmt);
	vsnprintf(log, LOGSIZE, fmt, args);
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%llu\n", pid, log, val);
	preempt_enable();
}

void __rs_systrace_b(pid_t tgid, const char *fmt, ...)
{
	char log[LOGSIZE];
	va_list args;

	if (unlikely(!mark_addr))
		return;

	va_start(args, fmt);
	vsnprintf(log, LOGSIZE, fmt, args);
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "B|%d|%s\n", tgid, log);
	preempt_enable();
}

void __rs_systrace_e(void)
{
	if (unlikely(!mark_addr))
		return;

	preempt_disable();
	event_trace_printk(mark_addr, "E\n");
	preempt_enable();
}

int rs_init_trace(struct dentry *rs_debugfs_dir)
{
	mark_addr = kallsyms_lookup_name("tracing_mark_write");

	return 0;
}

