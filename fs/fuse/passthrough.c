/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "fuse_passthrough.h"

#include <linux/aio.h>
#include <linux/fs_stack.h>

void fuse_setup_passthrough(struct fuse_conn *fc, struct fuse_req *req)
{
	int daemon_fd, fs_stack_depth;
	unsigned int open_out_index;
	struct file *passthrough_filp;
	struct inode *passthrough_inode;
	struct super_block *passthrough_sb;
	struct fuse_open_out *open_out;

	req->passthrough_filp = NULL;

	if (!(fc->passthrough))
		return;

	if ((req->in.h.opcode != FUSE_OPEN) &&
	    (req->in.h.opcode != FUSE_CREATE))
		return;

	open_out_index = req->in.numargs - 1;

	WARN_ON(open_out_index != 0 && open_out_index != 1);
	WARN_ON(req->out.args[open_out_index].size != sizeof(*open_out));

	open_out = req->out.args[open_out_index].value;

	daemon_fd = (int)open_out->passthrough_fd;
	if (daemon_fd < 0)
		return;

	passthrough_filp = fget_raw(daemon_fd);
	if (!passthrough_filp)
		return;

	passthrough_inode = file_inode(passthrough_filp);
	passthrough_sb = passthrough_inode->i_sb;
	fs_stack_depth = passthrough_sb->s_stack_depth + 1;

	/* If we reached the stacking limit go through regular io */
	if (fs_stack_depth > FILESYSTEM_MAX_STACK_DEPTH) {
		/* Release the passthrough file. */
		fput(passthrough_filp);
		pr_err("FUSE: maximum fs stacking depth exceeded, cannot use passthrough for this file\n");
		return;
	}
	req->passthrough_filp = passthrough_filp;
}


static ssize_t fuse_passthrough_read_write_iter(struct kiocb *iocb,
					    struct iov_iter *iter, int do_write)
{
	ssize_t ret_val;
	struct fuse_file *ff;
	struct file *fuse_file, *passthrough_filp;
	struct inode *fuse_inode, *passthrough_inode;
	struct fuse_conn *fc;

	ff = iocb->ki_filp->private_data;
	fuse_file = iocb->ki_filp;
	passthrough_filp = ff->passthrough_filp;
	fc = ff->fc;

	/* lock passthrough file to prevent it from being released */
	get_file(passthrough_filp);
	iocb->ki_filp = passthrough_filp;
	fuse_inode = fuse_file->f_path.dentry->d_inode;
	passthrough_inode = file_inode(passthrough_filp);

	if (do_write) {
		if (!passthrough_filp->f_op->write_iter)
			return -EIO;

		ret_val = passthrough_filp->f_op->write_iter(iocb, iter);

		if (ret_val >= 0 || ret_val == -EIOCBQUEUED) {
			spin_lock(&fc->lock);
			fsstack_copy_inode_size(fuse_inode, passthrough_inode);
			spin_unlock(&fc->lock);
			fsstack_copy_attr_times(fuse_inode, passthrough_inode);
		}
	} else {
		if (!passthrough_filp->f_op->read_iter)
			return -EIO;

		ret_val = passthrough_filp->f_op->read_iter(iocb, iter);
		if (ret_val >= 0 || ret_val == -EIOCBQUEUED)
			fsstack_copy_attr_atime(fuse_inode, passthrough_inode);
	}

	iocb->ki_filp = fuse_file;

	/* unlock passthrough file */
	fput(passthrough_filp);

	return ret_val;
}

ssize_t fuse_passthrough_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	return fuse_passthrough_read_write_iter(iocb, to, 0);
}

ssize_t fuse_passthrough_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	return fuse_passthrough_read_write_iter(iocb, from, 1);
}

void fuse_passthrough_release(struct fuse_file *ff)
{
	if (!(ff->passthrough_filp))
		return;

	/* Release the passthrough file. */
	fput(ff->passthrough_filp);
	ff->passthrough_filp = NULL;
}
