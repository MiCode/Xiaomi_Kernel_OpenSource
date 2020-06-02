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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include "apu_tags.h"

#define APU_TAGS_PROC_FS_NAME "aputag"

static struct proc_dir_entry *proot;
static DEFINE_MUTEX(apu_tags_list_lock);
static LIST_HEAD(apu_tags_list);

static int apu_tags_alloc_procfs(struct apu_tags *at);
static void apu_tags_free_procfs(struct apu_tags *at);
static int apu_tags_init_procfs(void);
static void apu_tags_exit_procfs(void);

struct apu_tag_info_t {
	uint64_t time;
	unsigned short pid;
};

static struct apu_tags *apu_tags_find(const char *name)
{
	struct apu_tags *a, *n;
	struct apu_tags *ret = NULL;

	mutex_lock(&apu_tags_list_lock);
	list_for_each_entry_safe(a, n, &apu_tags_list, list) {
		if (!strncmp(a->name, name, APU_TAG_NAME_SZ-1)) {
			ret = a;
			break;
		}
	}
	mutex_unlock(&apu_tags_list_lock);
	return ret;
}

static void apu_tags_free_tags(struct apu_tags *at)
{
	unsigned long flags;

	spin_lock_irqsave(&at->lock, flags);
	kfree(at->tags);
	at->tags = NULL;
	at->cnt = 0;
	at->used_mem = 0;
	spin_unlock_irqrestore(&at->lock, flags);
}

/**
 * apu_tags_alloc() - Allocate apu_tags
 *
 * @name: name of the tags
 * @size: size of a tag data
 * @cnt: number of tags
 * @seq_tag: seq print function for each tag
 * @seq_info: seq print funcction for private info
 * @priv: private data (ex: device data)
 *
 * Returns allocated apu_tags, or NULL on fail.
 * It also creates procfs entry at /proc/aputag/"name"
 */
struct apu_tags *apu_tags_alloc(const char *name, int size, int cnt,
	apu_tags_seq_f seq_tag, apu_tags_seq_f seq_info, void *priv)
{
	struct apu_tags *at;

	if (!name || !cnt || !size || !seq_tag)
		return NULL;

	at = apu_tags_find(name);
	if (at) {
		pr_notice("%s: apu tags %s is already exist\n",
			__func__, name);
		return NULL;
	}

	at = kzalloc(sizeof(struct apu_tags), GFP_NOFS);
	if (!at)
		return NULL;

	at->seq_tag = seq_tag;
	at->seq_info = seq_info;
	at->dat_sz = size;
	at->ent_sz = size + sizeof(struct apu_tag_info_t);
	at->used_mem = sizeof(struct apu_tags) + (at->ent_sz * cnt);
	at->idx = 0;
	at->cnt = cnt;
	spin_lock_init(&at->lock);
	at->tags = kmalloc_array(cnt, at->ent_sz, GFP_NOFS);

	if (!at->tags) {
		kfree(at);
		return NULL;
	}

	memset(at->tags, 0, at->ent_sz * at->cnt);
	strncpy(at->name, name, APU_TAG_NAME_SZ-1);

	/* proc dentries */
	if (apu_tags_alloc_procfs(at)) {
		apu_tags_free(at);
		return NULL;
	}

	return at;
}

/**
 * apu_tags_free() - Free apu_tags, and removes its procfs entry
 *
 * @at: pointer to allocated apu_tags
 */
void apu_tags_free(struct apu_tags *at)
{
	if (!at)
		return;

	apu_tags_free_procfs(at);
	apu_tags_free_tags(at);
	kfree(at);
}

static void apu_tags_clear(struct apu_tags *at)
{
	unsigned long flags;

	spin_lock_irqsave(&at->lock, flags);
	memset(at->tags, 0, at->ent_sz * at->cnt);
	at->idx = 0;
	spin_unlock_irqrestore(&at->lock, flags);
}

static struct apu_tag_info_t *apu_tag_at(struct apu_tags *at, int idx)
{
	if (!at->tags)
		return NULL;

	return (struct apu_tag_info_t *)(at->tags + (at->ent_sz * idx));
}

static struct apu_tag_info_t *apu_tag_curr(struct apu_tags *at)
{
	return apu_tag_at(at, at->idx);
}

static void *apu_tag_data(struct apu_tag_info_t *ti)
{
	if (!ti)
		return NULL;

	return ((char *)ti) + sizeof(struct apu_tag_info_t);
}

/**
 * apu_tag_add() - Add an entry to the ring tag buffer of apu_tags
 *
 * @at: pointer to allocated apu_tags
 * @tag: tag entry to be added
 *
 * Copyies the content of the given tag entry to apu_tags, and
 * increments the index of apu_tags.
 */
void apu_tag_add(struct apu_tags *at, void *tag)
{
	struct apu_tag_info_t *ti;
	unsigned long flags;

	if (!at->tags)
		return;

	spin_lock_irqsave(&at->lock, flags);
	ti = apu_tag_curr(at);
	if (ti)	{
		ti->time = sched_clock();
		ti->pid = current->pid;
		memcpy(apu_tag_data(ti), tag, at->dat_sz);
		at->idx++;
		if (at->idx >= at->cnt)
			at->idx = 0;
	}
	spin_unlock_irqrestore(&at->lock, flags);
}

/**
 * apu_tags_seq_time() - print time to seq_file
 *
 * @time: schedule clock time got from sched_clock()
 */
void apu_tags_seq_time(struct seq_file *s, uint64_t time)
{
	uint32_t nsec;

	nsec = do_div(time, 1000000000);
	seq_printf(s, "%lu.%06lu", (unsigned long)time,
		(unsigned long)nsec/1000);
}

static void
apu_tags_seq_prefix(struct seq_file *s, uint64_t t, unsigned short pid)
{
	uint32_t nsec;

	nsec = do_div(t, 1000000000);
	seq_printf(s, "[%5lu.%06lu] %d:", (unsigned long)t,
		(unsigned long)nsec/1000, pid);
}

/**
 * apu_tags_seq() - print tags to seq_file
 *
 * @at: given apu_tags
 * @s: target seq_file
 *
 * Note: The content of /proc/aputag/"name", you
 * may call this function to show tags at other debug nodes
 */
int apu_tags_seq(struct apu_tags *at, struct seq_file *s)
{
	unsigned long flags;
	int i, end, ret;

	if (!at || !at->seq_tag)
		return 0;

	if (at->idx >= at->cnt || at->idx < 0)
		at->idx = 0;

	spin_lock_irqsave(&at->lock, flags);
	end = (at->idx > 0) ? at->idx - 1 : at->cnt - 1;

	for (i = at->idx;; ) {
		struct apu_tag_info_t *ti;

		ti = apu_tag_at(at, i);
		if (!ti)
			break;
		if (!ti->time)
			goto next;
		apu_tags_seq_prefix(s, ti->time, ti->pid);
		ret = at->seq_tag(s, apu_tag_data(ti), at->priv);
		if (ret) {
			seq_puts(s, "\n");
			break;
		}
next:
		if (i == end)
			break;
		i = (i >= at->cnt - 1) ? 0 : i + 1;
	}
	spin_unlock_irqrestore(&at->lock, flags);

	if (at->seq_info)
		at->seq_info(s, NULL, at->priv);

	return 0;
}

#define APU_TAG_CMD_SZ 16

static ssize_t apu_tags_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	struct apu_tags *at;
	char b[APU_TAG_CMD_SZ];
	char *cmd, *cur;

	at = PDE_DATA(file->f_inode);
	if (!at)
		return -EINVAL;

	if (count == 0 || count >= APU_TAG_CMD_SZ)
		return -EINVAL;

	if (copy_from_user(b, buf, count))
		return -EINVAL;

	b[APU_TAG_CMD_SZ - 1] = 0;
	cur = &b[0];
	cmd = strsep(&cur, " \t\n");

	if (!strncmp(cmd, "clear", APU_TAG_CMD_SZ))
		apu_tags_clear(at);
	else if (!strncmp(cmd, "free", APU_TAG_CMD_SZ))
		apu_tags_free_tags(at);

	return count;
}

static int apu_tags_proc_show(struct seq_file *s, void *v)
{
	return apu_tags_seq((struct apu_tags *)s->private, s);
}

static int apu_tags_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_tags_proc_show, PDE_DATA(inode));
}

static const struct file_operations apu_tags_proc_fops = {
	.open = apu_tags_proc_open,
	.write = apu_tags_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int apu_tags_info_show(struct seq_file *s, void *v)
{
	struct apu_tags *a, *n;
	unsigned long total_mem = 0;

	seq_puts(s, "Name\tCount\tEntry Size\tUsed Memory\n");
	mutex_lock(&apu_tags_list_lock);
	list_for_each_entry_safe(a, n, &apu_tags_list, list) {
		seq_printf(s, "%s\t%5d\t%10d\t%11ld\n",
			a->name, a->cnt, a->ent_sz, a->used_mem);
		total_mem += a->used_mem;
	}
	mutex_unlock(&apu_tags_list_lock);
	seq_printf(s, "Total Used Memory: %ld bytes\n", total_mem);

	return 0;
}

static int apu_tags_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_tags_info_show, NULL);
}

static const struct file_operations apu_tags_info_fops = {
	.open = apu_tags_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int apu_tags_init_procfs(void)
{
	if (proot)
		return 0;

	proot = proc_mkdir(APU_TAGS_PROC_FS_NAME, NULL);
	if (!proot) {
		pr_info("%s: unable to create\n", __func__);
		return -ENOMEM;
	}
	proc_create("info", 0440, proot, &apu_tags_info_fops);

	return 0;
}

static void apu_tags_exit_procfs(void)
{
	remove_proc_subtree(APU_TAGS_PROC_FS_NAME, NULL);
	proot = NULL;
}

#if !defined(USER_BUILD_KERNEL) && defined(CONFIG_MTK_ENG_BUILD)
#define APU_TAG_PROC_MDOE		0660
#else
#define APU_TAG_PROC_MDOE		0440
#endif

static int apu_tags_alloc_procfs(struct apu_tags *at)
{
	int ret = 0;

	mutex_lock(&apu_tags_list_lock);
	list_add(&at->list, &apu_tags_list);
	ret = apu_tags_init_procfs();
	if (ret)
		goto out;

	at->proc = proc_create_data(at->name, APU_TAG_PROC_MDOE,
		proot, &apu_tags_proc_fops, at);
	if (!at->proc) {
		pr_info("%s: unable to create %s\n", __func__, at->name);
		ret = -ENOMEM;
	}
out:
	mutex_unlock(&apu_tags_list_lock);
	return ret;
}

static void apu_tags_free_procfs(struct apu_tags *at)
{
	mutex_lock(&apu_tags_list_lock);
	list_del(&at->list);
	if (at->proc && proot) {
		remove_proc_subtree(at->name, proot);
		at->proc = NULL;
	}
	if (list_empty(&apu_tags_list))
		apu_tags_exit_procfs();
	mutex_unlock(&apu_tags_list_lock);
}

