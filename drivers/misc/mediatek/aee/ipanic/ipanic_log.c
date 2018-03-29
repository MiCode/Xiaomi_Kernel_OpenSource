/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/module.h>
#include "ipanic.h"
#include "ipanic_version.h"

u8 *log_temp;
/*
 * The function is used to dump kernel log after *Linux 3.5*
 */
size_t log_rest;
u8 *log_idx;
int ipanic_kmsg_dump3(struct kmsg_dumper *dumper, char *buf, size_t len)
{
	size_t rc = 0;
	size_t sum = 0;
	/* if log_rest is no 0, it means that there some bytes left by previous log entry need to write to buf */
	if (log_rest != 0) {
		memcpy(buf, log_idx, log_rest);
		sum += log_rest;
		log_rest = 0;
	}
	while (kmsg_dump_get_line_nolock(dumper, false, log_temp, len, &rc)
	       && dumper->cur_seq <= dumper->next_seq) {
		if (sum + rc >= len) {
			memcpy(buf + sum, log_temp, len - sum);
			log_rest = rc - (len - sum);
			log_idx = log_temp + (len - sum);
			sum += (len - sum);
			return len;
		}
		memcpy(buf + sum, log_temp, rc);
		sum += rc;
	}
	return sum;
}
EXPORT_SYMBOL(ipanic_kmsg_dump3);

void ipanic_klog_region(struct kmsg_dumper *dumper)
{
	static struct ipanic_log_index next = { 0 };

	dumper->cur_idx = next.seq ? next.idx : log_first_idx;
	dumper->cur_seq = next.seq ? next.seq : log_first_seq;
	dumper->next_idx = log_next_idx;
	dumper->next_seq = log_next_seq;
	next.idx = log_next_idx;
	next.seq = log_next_seq;
	LOGD("kernel log region: [%x:%llx,%x:%llx]", dumper->cur_idx, dumper->cur_seq,
	     dumper->next_idx, dumper->next_seq);
}

int ipanic_klog_buffer(void *data, unsigned char *buffer, size_t sz_buf)
{
	int rc = 0;
	struct kmsg_dumper *dumper = (struct kmsg_dumper *)data;

	dumper->active = true;
	rc = ipanic_kmsg_dump3(dumper, buffer, sz_buf);
	if (rc < 0)
		rc = -1;
	return rc;
}

void ipanic_log_temp_init(void)
{
	log_temp = (u8 *) __get_free_page(GFP_KERNEL);
}

