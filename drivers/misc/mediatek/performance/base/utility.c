// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include "mtk_perfmgr_internal.h"
#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

char *perfmgr_copy_from_user_for_proc(const char __user *buffer,
		size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

int check_proc_write(int *data, const char *ubuf, size_t cnt)
{

	char buf[128];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	if (kstrtoint(buf, 10, data))
		return -1;
	return 0;
}

int check_group_proc_write(int *cgroup, int *data,
		const char *ubuf, size_t cnt)
{
	char buf[128];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	if (sscanf(buf, "%d %d", cgroup, data) != 2)
		return -1;
	return 0;
}

#ifdef CONFIG_TRACING
static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}

void perfmgr_trace_count(int val, const char *fmt, ...)
{
	char log[32];
	va_list args;

#ifdef CONFIG_MTK_PPM
	if (powerhal_tid <= 0)
		return;
#endif

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	__mt_update_tracing_mark_write_addr();
	preempt_disable();

#ifdef CONFIG_MTK_PPM
	event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d\n",
		powerhal_tid, log, val);
#else
	event_trace_printk(tracing_mark_write_addr, "C|%s|%d\n",
		log, val);
#endif

	preempt_enable();
}

void perfmgr_trace_printk(char *module, char *string)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "%d [%s] %s\n",
			current->tgid, module, string);
	preempt_enable();
}

void perfmgr_trace_begin(char *name, int id, int a, int b)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "B|%d|%s|%d|%d|%d\n",
			current->tgid, name, id, a, b);
	preempt_enable();
}

void perfmgr_trace_end(void)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "E\n");
	preempt_enable();
}

void perfmgr_trace_log(char *module, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	perfmgr_trace_printk(module, log);
}

unsigned int perfmgr_cpufreq_get_freq_by_idx(int cluster, int opp)
{
	unsigned int freq = 0;

	if (cluster == 0) {
		switch (opp) {
		case 0:
			freq = 2000000;
			break;
		case 1:
			freq = 1933000;
			break;
		case 2:
			freq = 1866000;
			break;
		case 3:
			freq = 1800000;
			break;
		case 4:
			freq = 1733000;
			break;
		case 5:
			freq = 1666000;
			break;
		case 6:
			freq = 1548000;
			break;
		case 7:
			freq = 1475000;
			break;
		case 8:
			freq = 1375000;
			break;
		case 9:
			freq = 1275000;
			break;
		case 10:
			freq = 1175000;
			break;
		case 11:
			freq = 1075000;
			break;
		case 12:
			freq = 999000;
			break;
		case 13:
			freq = 925000;
			break;
		case 14:
			freq = 850000;
			break;
		case 15:
			freq = 774000;
			break;
		}
	} else if (cluster == 1) {
		switch (opp) {
		case 0:
			freq = 2200000;
			break;
		case 1:
			freq = 2133000;
			break;
		case 2:
			freq = 2066000;
			break;
		case 3:
			freq = 2000000;
			break;
		case 4:
			freq = 1933000;
			break;
		case 5:
			freq = 1866000;
			break;
		case 6:
			freq = 1800000;
			break;
		case 7:
			freq = 1651000;
			break;
		case 8:
			freq = 1503000;
			break;
		case 9:
			freq = 1414000;
			break;
		case 10:
			freq = 1295000;
			break;
		case 11:
			freq = 1176000;
			break;
		case 12:
			freq = 1087000;
			break;
		case 13:
			freq = 998000;
			break;
		case 14:
			freq = 909000;
			break;
		case 15:
			freq = 850000;
			break;
		}
	}
	return freq;
}

#endif
