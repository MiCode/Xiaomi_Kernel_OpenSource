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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pstore_ram.h>
#include <linux/pstore.h>
#include "ipanic.h"

static struct ipanic_header *aee_poweroff_logs;
static unsigned int aee_active_index;
#define AEE_POWEROFF_NR	4
#define AEE_POWEROFF_OFFSET 0x00700000
#define AEE_POWEROFF_MAGIC 0x43474244	/* DBGC */

struct ipanic_data_zone {
	struct ipanic_data_header *header;

};

void ipanic_kmsg_hdr(struct ipanic_data_header *hdr, unsigned int id, unsigned int offset)
{
	struct timespec timestamp;

	hdr->id = id;
	hdr->valid = 1;
	hdr->offset = offset;
	hdr->total = __LOG_BUF_LEN;
	hdr->encrypt = 0;
	if (__getnstimeofday(&timestamp)) {
		timestamp.tv_sec = 0;
		timestamp.tv_nsec = 0;
	}
	snprintf(hdr->name, 32, "kmsg_%lu", (long)timestamp.tv_sec);
}

void ipanic_kmsg_init(void)
{
	aee_poweroff_logs = ipanic_header_from_sd(AEE_POWEROFF_OFFSET, AEE_POWEROFF_MAGIC);
	if (!aee_poweroff_logs) {
		aee_poweroff_logs = kzalloc(sizeof(struct ipanic_header), GFP_KERNEL);
		if (ipanic_msdc_info(aee_poweroff_logs)) {
			LOGE("aee poweroff logs init msdc fail");
			kfree(aee_poweroff_logs);
			aee_poweroff_logs = NULL;
			return;
		}
		aee_poweroff_logs->magic = AEE_POWEROFF_MAGIC;
		aee_poweroff_logs->size = sizeof(struct ipanic_header);
	}
}

void ipanic_kmsg_hdr_write(void)
{
	ipanic_mem_write(aee_poweroff_logs, AEE_POWEROFF_OFFSET, sizeof(struct ipanic_header), 0);
}

void ipanic_kmsg_next(void)
{
	int i;
	struct ipanic_data_header *hdr;
	int last_id = 0;

	if (!aee_poweroff_logs)
		return;
	aee_active_index = 0;

	for (i = 0; i < AEE_POWEROFF_NR; i++) {
		if (aee_poweroff_logs->data_hdr[i].id <
		    aee_poweroff_logs->data_hdr[aee_active_index].id)
			aee_active_index = i;
		if (aee_poweroff_logs->data_hdr[i].id > aee_poweroff_logs->data_hdr[last_id].id)
			last_id = i;
	}
	hdr = &aee_poweroff_logs->data_hdr[aee_active_index];
	ipanic_kmsg_hdr(hdr, aee_poweroff_logs->data_hdr[last_id].id + 1,
			AEE_POWEROFF_OFFSET + ALIGN(sizeof(struct ipanic_header),
						    aee_poweroff_logs->blksize) +
			aee_active_index * __LOG_BUF_LEN);
	hdr->used = hdr->total;
	ipanic_kmsg_hdr_write();
	hdr->used = 0;
}

int ipanic_kmsg_write(unsigned int part, const char *buf, size_t size)
{
	struct ipanic_data_header *hdr;
	int offset;

	if (part == 1)
		ipanic_kmsg_next();
	hdr = &aee_poweroff_logs->data_hdr[aee_active_index];
	offset = hdr->offset + hdr->total - hdr->used - ALIGN(size, aee_poweroff_logs->blksize);
	if (offset < hdr->offset)
		return -EINVAL;
	hdr->used += ALIGN(size, aee_poweroff_logs->blksize);
	ipanic_mem_write((void *)buf, offset, size, hdr->encrypt);
	return 0;
}

int ipanic_kmsg_get_next(int *count, u64 *id, enum pstore_type_id *type, struct timespec *time,
			 char **buf, struct pstore_info *psi)
{
	char *data;
	int i, size = 0;
	struct ipanic_data_header *hdr;

	if (*count == 0)
		ipanic_kmsg_init();
	for (i = *count; i < AEE_POWEROFF_NR; i++) {
		hdr = &aee_poweroff_logs->data_hdr[i];
		if (!hdr->valid)
			continue;
		data = ipanic_data_from_sd(hdr, 0);
		if (data) {
			*buf = data;
			size = hdr->used;
			*type = PSTORE_TYPE_DMESG;
			*id = hdr->id;
			i++;
			break;
		}
	}
	LOGD("ipanic_kmsg_get_next: count %d, buf %p, size %x\n", *count, *buf, size);
	*count = i;
	return size;
}
