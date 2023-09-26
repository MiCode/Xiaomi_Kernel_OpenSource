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

#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/kmemleak.h>
#include "mdw_usr.h"
#include "mdw_cmd.h"
#include "mdw_cmn.h"

extern struct mdw_usr_mgr u_mgr;
struct apu_poll_desc {
	struct mdw_apu_cmd *c;
	struct mdw_usr *u;
};

static int apu_file_release(struct inode *inode, struct file *file)
{
	struct apu_poll_desc *d = file->private_data;

	if (d) {
		file->private_data = NULL;
		kfree(d);
	}
	return 0;
}

static unsigned int apu_file_poll(struct file *file, poll_table *wait)
{
	struct apu_poll_desc *d = file->private_data;
	struct mdw_usr *u;
	struct mdw_apu_cmd *c;
	int id = 0;

	if (d == NULL)
		return POLLIN;

	mutex_lock(&u_mgr.mtx);
	u = d->u;
	if (mdw_user_check(u))
		mdw_usr_get(u);
	else {
		mutex_unlock(&u_mgr.mtx);
		return POLLIN;
	}
	mutex_unlock(&u_mgr.mtx);

	/* Check cmd */
	mutex_lock(&u->mtx);
	idr_for_each_entry(&u->cmds_idr, c, id) {
		mdw_flw_debug("poll cmd(0x%llx/0x%llx) matching...\n",
			(uint64_t)c, (uint64_t)d->c);

		if (c == d->c)
			break;
		c = NULL;
	}
	if (c)
		idr_remove(&u->cmds_idr, c->id);
	mutex_unlock(&u->mtx);

	if (c == NULL)
		goto out;

	mdw_wait_cmd(u, d->c);
out:
	mdw_usr_put(u);
	return POLLIN;
}

static const struct file_operations apu_sync_file_fops = {
	.release = apu_file_release,
	.poll = apu_file_poll,
};

int apu_sync_file_create(struct mdw_apu_cmd *c)
{
	struct apu_poll_desc *desc;
	int ret = 0;
	int fd = get_unused_fd_flags(O_CLOEXEC);

	if (fd < 0)
		return -EINVAL;

	desc = kzalloc(sizeof(struct apu_poll_desc), GFP_KERNEL);

	/* Ignore kmemleak false positive */
	kmemleak_ignore(desc);

	desc->c = c;
	desc->u = c->usr;
	c->file = anon_inode_getfile("apu_file", &apu_sync_file_fops, desc, 0);

	if (c->file == NULL) {
		put_unused_fd(fd);
		ret = -EINVAL;
	} else {
		fd_install(fd, c->file);
		c->uf_hdr->fd = fd;
	}
	return 0;
}
