/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APU_TAGS_H__
#define __APU_TAGS_H__

#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/types.h>

#define APU_TAG_NAME_SZ 32

typedef int (*apu_tags_seq_f)(struct seq_file *s, void *tag, void *priv);

struct apu_tags {
	spinlock_t lock;
	char name[APU_TAG_NAME_SZ];
	int dat_sz; /* size of tag data */
	int ent_sz; /* size of tag entry (including tag info) */
	int cnt;    /* total count of tags */
	int idx;    /* current tag index */
	char *tags; /* container of tags, size = ent_sz * cnt */
	apu_tags_seq_f seq_tag;  /* seq print func. for each tag */
	apu_tags_seq_f seq_info;  /* seq print func. for private info */
	void *priv;  /* private data (ex: device data)*/
	unsigned long used_mem; /* size of allocated memory */
	struct list_head list;  /* link to apu tags list*/
	struct proc_dir_entry *proc;  /* allocated procfs entry */
};

#ifdef CONFIG_MTK_APUSYS_DEBUG
struct apu_tags *apu_tags_alloc(const char *name, int size, int cnt,
	apu_tags_seq_f seq_tag, apu_tags_seq_f seq_info, void *priv);
void apu_tag_add(struct apu_tags *at, void *tag);
void apu_tags_free(struct apu_tags *at);
void apu_tags_seq_time(struct seq_file *s, uint64_t time);
int apu_tags_seq(struct apu_tags *at, struct seq_file *s);

#else
static inline
struct apu_tags *apu_tags_alloc(const char *name, int size, int cnt,
	apu_tags_seq_f seq_tag, apu_tags_seq_f seq_info, void *priv)
{
	return NULL;
}

static inline
void apu_tags_free(struct apu_tags *at)
{
}

static inline
void apu_tag_add(struct apu_tags *at, void *tag)
{
}

static inline
void apu_tags_seq_time(struct seq_file *s, uint64_t time)
{
}

static inline
int apu_tags_seq(struct apu_tags *at, struct seq_file *s)
{
	return 0;
}
#endif

#endif
