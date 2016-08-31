/*
 * Copyright (c) 2013-2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>

#include "ote_protocol.h"

#define TE_SHMEM_FNAME_SZ	SZ_64
#define TE_SHMEM_DATA_SZ	SZ_128K

struct te_file_req_shmem {
	char	file_name[TE_SHMEM_FNAME_SZ];
	char	file_data[TE_SHMEM_DATA_SZ];
};

struct te_file_req_node {
	struct list_head node;
	struct te_file_req *req;
};

static struct list_head req_list;
static DECLARE_COMPLETION(req_ready);
static DECLARE_COMPLETION(req_complete);
static unsigned long secure_error;

static void indicate_complete(unsigned long ret)
{
	tlk_generic_smc(TE_SMC_FS_OP_DONE, ret, 0);
}

int te_handle_fs_ioctl(struct file *file, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	struct te_file_req new_req, *ptr_user_req = NULL;
	struct te_file_req_node *req_node;

	switch (ioctl_num) {
	case TE_IOCTL_FILE_NEW_REQ: /* new request */

		ptr_user_req = (struct te_file_req *)ioctl_param;

		/* wait for a new request */
		if (wait_for_completion_interruptible(&req_ready)) {
			return -ENODATA;
		}

		/* dequeue new request from the secure world */
		req_node = list_first_entry(&req_list, struct te_file_req_node,
				node);

		/* populate request for the non-secure client */
		if (req_node) {
			if (copy_to_user(ptr_user_req, req_node->req,
				sizeof(struct te_file_req))) {
				pr_err("copy_to_user failed for new request\n");
				return -EFAULT;
			}

			list_del(&req_node->node);
			kfree(req_node);
		} else {
			pr_err("no request available\n");
			return -ENOMEM;
		}

		break;

	case TE_IOCTL_FILE_FILL_BUF: /* pass data to be written to the file */

		if (copy_from_user(&new_req, (void __user *)ioctl_param,
			sizeof(struct te_file_req))) {
			pr_err("copy_from_user failed for request\n");
			return -EFAULT;
		}

		if (new_req.type != OTE_FILE_REQ_WRITE)
			return -EINVAL;

		if (!new_req.kern_data_buf || !new_req.user_data_buf)
			return -EINVAL;

		if (copy_to_user(new_req.user_data_buf, new_req.kern_data_buf,
			new_req.data_len)) {
			pr_err("copy_to_user failed for fill buffer\n");
			return -EFAULT;
		}
		break;

	case TE_IOCTL_FILE_REQ_COMPLETE: /* request complete */

		if (copy_from_user(&new_req, (void __user *)ioctl_param,
			sizeof(struct te_file_req))) {
			pr_err("copy_from_user failed for request\n");
			return -EFAULT;
		}

		if (new_req.type == OTE_FILE_REQ_READ && !new_req.error) {
			if (copy_from_user(new_req.kern_data_buf,
				(void __user *)new_req.user_data_buf,
				new_req.data_len)) {
				pr_err("copy_from_user failed for request\n");
				return -EFAULT;
			}
		}

		/* get error code */
		secure_error = (new_req.error) ? OTE_ERROR_NO_DATA
					       : new_req.result;

		/* signal the producer */
		complete(&req_complete);
		break;
	}

	return 0;
}

static void _te_fs_file_operation(const char *name, void *buf, int len,
		enum te_file_req_type type)
{
	struct te_file_req *new_req;
	struct te_file_req_node *req_node;

	BUG_ON(!name);

	if (type == OTE_FILE_REQ_READ || type == OTE_FILE_REQ_WRITE)
		BUG_ON(!buf);

	/* allocate te_file_req structure */
	new_req = kzalloc(sizeof(struct te_file_req), GFP_KERNEL);
	BUG_ON(!new_req);

	/* prepare a new request */
	strncpy(new_req->name, name, strlen(name));
	new_req->type = type;
	new_req->data_len = len;
	new_req->result = 0;
	new_req->kern_data_buf = buf;
	new_req->error = 0;

	req_node = kzalloc(sizeof(struct te_file_req_node), GFP_KERNEL);
	BUG_ON(!req_node);

	req_node->req = new_req;
	INIT_LIST_HEAD(&req_node->node);

	/* add it to the pending queue and signal the consumer */
	list_add_tail(&req_list, &req_node->node);
	complete(&req_ready);

	/* wait for the consumer's signal */
	wait_for_completion(&req_complete);

	kfree(new_req);

	/* signal completion to the secure world */
	indicate_complete(secure_error);
}

void tlk_fread(const char *name, void *buf, int len)
{
	if (!buf)
		_te_fs_file_operation(name, buf, len, OTE_FILE_REQ_SIZE);
	else
		_te_fs_file_operation(name, buf, len, OTE_FILE_REQ_READ);
}

void tlk_fwrite(const char *name, void *buf, int len)
{
	_te_fs_file_operation(name, buf, len, OTE_FILE_REQ_WRITE);
}

void tlk_fdelete(const char *name)
{
	_te_fs_file_operation(name, NULL, 0, OTE_FILE_REQ_DELETE);
}

static int __init tlk_fs_register_handlers(void)
{
	struct te_file_req_shmem *shmem_ptr;
	uint32_t smc_args[MAX_EXT_SMC_ARGS];
	dma_addr_t shmem_dma;

	shmem_ptr = dma_alloc_coherent(NULL, sizeof(struct te_file_req_shmem),
			&shmem_dma, GFP_KERNEL);
	if (!shmem_ptr) {
		pr_err("%s: no memory available for fs operations\n", __func__);
		return -ENOMEM;
	}

	memset(shmem_ptr, 0, sizeof(struct te_file_req_shmem));

	INIT_LIST_HEAD(&req_list);
	init_completion(&req_ready);
	init_completion(&req_complete);

	smc_args[0] = TE_SMC_REGISTER_FS_HANDLERS;
	smc_args[1] = (uint32_t)tlk_fread;
	smc_args[2] = (uint32_t)tlk_fwrite;
	smc_args[3] = (uint32_t)tlk_fdelete;
	smc_args[4] = (uint32_t)shmem_ptr->file_name;
	smc_args[5] = (uint32_t)shmem_ptr->file_data;

	tlk_extended_smc(smc_args);

	return 0;
}

arch_initcall(tlk_fs_register_handlers);
