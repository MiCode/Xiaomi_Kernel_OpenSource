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
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include "mdw_usr.h"
#include "mdw_cmd.h"


static int apu_file_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned int apu_file_poll(struct file *file, poll_table *wait)
{
	struct mdw_apu_cmd *c = file->private_data;

	if (c == NULL)
		return POLLIN;

	mdw_wait_cmd(c);
	return POLLIN;
}

static const struct file_operations apu_sync_file_fops = {
	.release = apu_file_release,
	.poll = apu_file_poll,
};


int apu_sync_file_create(struct mdw_apu_cmd *c)
{
	int ret = 0;
	int fd = get_unused_fd_flags(O_CLOEXEC);

	if (fd < 0)
		return -EINVAL;

	c->file = anon_inode_getfile("apu_file", &apu_sync_file_fops, c, 0);

	if (c->file == NULL) {
		put_unused_fd(fd);
		ret = -EINVAL;
	} else {
		fd_install(fd, c->file);
		c->uf_hdr->fd = fd;
	}
	return 0;
}
