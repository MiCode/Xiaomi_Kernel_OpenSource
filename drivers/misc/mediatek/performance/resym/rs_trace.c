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

#include "rs_trace.h"

#define LOGSIZE 32

static unsigned long __read_mostly mark_addr;

static int rs_update_tracemark(void)
{
	if (mark_addr)
		return 1;

	mark_addr = kallsyms_lookup_name("tracing_mark_write");

	if (unlikely(!mark_addr))
		return 0;

	return 1;
}

void __rs_systrace_c(pid_t pid, int val, const char *fmt, ...)
{
	char log[LOGSIZE];
	va_list args;
	int len;

	if (unlikely(!rs_update_tracemark()))
		return;

	va_start(args, fmt);
	len = vsnprintf(log, LOGSIZE, fmt, args);
	va_end(args);

	if (len == LOGSIZE)
		log[LOGSIZE - 1] = '\0';

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%d\n", pid, log, val);
	preempt_enable();
}

void __rs_systrace_c_uint64(pid_t pid, uint64_t val, const char *fmt, ...)
{
	char log[LOGSIZE];
	va_list args;
	int len;

	if (unlikely(!rs_update_tracemark()))
		return;

	va_start(args, fmt);
	len = vsnprintf(log, LOGSIZE, fmt, args);
	va_end(args);

	if (len == LOGSIZE)
		log[LOGSIZE - 1] = '\0';

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%llu\n", pid, log, val);
	preempt_enable();
}

void __rs_systrace_b(pid_t tgid, const char *fmt, ...)
{
	char log[LOGSIZE];
	va_list args;
	int len;

	if (unlikely(!rs_update_tracemark()))
		return;

	va_start(args, fmt);
	len = vsnprintf(log, LOGSIZE, fmt, args);
	va_end(args);

	if (len == LOGSIZE)
		log[LOGSIZE - 1] = '\0';

	preempt_disable();
	event_trace_printk(mark_addr, "B|%d|%s\n", tgid, log);
	preempt_enable();
}

void __rs_systrace_e(void)
{
	if (unlikely(!rs_update_tracemark()))
		return;

	preempt_disable();
	event_trace_printk(mark_addr, "E\n");
	preempt_enable();
}

int rs_trace_init(void)
{
	if (!rs_update_tracemark())
		return -1;

	return 0;
}

