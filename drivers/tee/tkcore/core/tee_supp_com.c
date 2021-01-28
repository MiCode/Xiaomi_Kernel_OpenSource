// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/anon_inodes.h>
#include <linux/semaphore.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/device.h>

#include "tee_shm.h"
#include "tee_core.h"
#include "tee_supp_com.h"

enum teec_rpc_result tee_supp_cmd(struct tee *tee,
			uint32_t id, void *data, size_t datalen)
{
	struct tee_rpc *rpc = tee->rpc;
	enum teec_rpc_result res = TEEC_RPC_FAIL;
	size_t size;
	struct task_struct *task = current;

	(void) task;

	if (id == TEE_RPC_ICMD_INVOKE) {
		if (atomic_read(&rpc->used) == 0) {
			pr_err("teed not ready. id=0x%x\n", id);
			goto out;
		}
	}

	switch (id) {
	case TEE_RPC_ICMD_ALLOCATE:
		{
			struct tee_rpc_alloc *alloc;
			struct tee_shm *shmint;

			alloc = (struct tee_rpc_alloc *)data;
			size = alloc->size;
			memset(alloc, 0, sizeof(struct tee_rpc_alloc));
			shmint = tee_shm_alloc_from_rpc(tee, size, 0U);
			if (IS_ERR_OR_NULL(shmint))
				break;

			alloc->size = size;
			alloc->data = (void *) (unsigned long)
				shmint->resv.paddr;
			alloc->shm = shmint;
			res = TEEC_RPC_OK;

			break;
		}
	case TEE_RPC_ICMD_FREE:
		{
			struct tee_rpc_free *free;

			free = (struct tee_rpc_free *)data;
			tee_shm_free_from_rpc(free->shm);
			res = TEEC_RPC_OK;
			break;
		}
	case TEE_RPC_ICMD_INVOKE:
		{
			if (sizeof(rpc->commToUser) < datalen)
				break;

			/*
			 * Other threads blocks here until we've copied our
			 * answer from the teed
			 */
			mutex_lock(&rpc->thrd_mutex);

			mutex_lock(&rpc->outsync);
			rpc->res = 0;
			memcpy(&rpc->commToUser, data, datalen);
			mutex_unlock(&rpc->outsync);

			up(&rpc->datatouser);

			down(&rpc->datafromuser);

			mutex_lock(&rpc->insync);
			memcpy(data, &rpc->commFromUser, datalen);
			mutex_unlock(&rpc->insync);

			res = rpc->res;
			mutex_unlock(&rpc->thrd_mutex);

			break;
		}
	default:
		/* not supported */
		break;
	}

out:

	return res;
}
EXPORT_SYMBOL(tee_supp_cmd);

ssize_t tee_supp_read(struct file *filp, char __user *buffer,
		size_t length, loff_t *offset)
{
	struct tee_context *ctx = (struct tee_context *)(filp->private_data);
	struct tee *tee;
	struct tee_rpc *rpc;
	struct task_struct *task = current;
	int ret;

	(void) task;

	if (ctx == NULL ||
		ctx->tee == NULL ||
		ctx->tee->dev == NULL ||
		ctx->tee->rpc == NULL) {
		pr_err("invalid context\n");
		return -EINVAL;
	}

	tee = ctx->tee;

	rpc = tee->rpc;

	if (atomic_read(&rpc->used) == 0) {
		pr_err("teed application not ready\n");
		ret = -EPERM;
		goto out;
	}

	if (down_interruptible(&rpc->datatouser))
		return -ERESTARTSYS;

	mutex_lock(&rpc->outsync);

	ret =
		sizeof(rpc->commToUser) - sizeof(rpc->commToUser.cmds) +
		sizeof(rpc->commToUser.cmds[0]) * rpc->commToUser.nbr_bf;
	if (length < ret) {
		ret = -EINVAL;
	} else {
		if (copy_to_user(buffer, &rpc->commToUser, ret)) {
			pr_err("copy_to_user(comm) failed!\n");
			ret = -EINVAL;
		}
	}

	mutex_unlock(&rpc->outsync);

out:
	return ret;
}

ssize_t tee_supp_write(struct file *filp, const char __user *buffer,
		size_t length, loff_t *offset)
{
	int ret = 0;
	uint32_t i;
	unsigned long r;

	struct tee_context *ctx = (struct tee_context *) (filp->private_data);
	struct tee *tee;
	struct tee_rpc *rpc;
	struct task_struct *task = current;

	(void) task;

	if (ctx == NULL || ctx->tee == NULL || ctx->tee->rpc == NULL) {
		pr_err("Invalid ctx\n");
		return -EINVAL;
	}

	if (length == 0)
		return 0;

	tee = ctx->tee;
	rpc = tee->rpc;

	if (atomic_read(&rpc->used) == 0) {
		pr_err("teed not ready\n");
		goto out;
	}

	if (length > sizeof(rpc->commFromUser)) {
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&rpc->insync);

	r = copy_from_user(&rpc->commFromUser, buffer, length);
	if (r) {
		pr_err("copy_from_user(comm) failed: %lu\n", r);
		rpc->res = -EINVAL;
		mutex_unlock(&rpc->insync);
		up(&rpc->datafromuser);

		ret = -EINVAL;
		goto out;
	}

	if (rpc->commFromUser.nbr_bf > TEE_RPC_BUFFER_NUMBER) {
		rpc->res = -EINVAL;
		mutex_unlock(&rpc->insync);
		up(&rpc->datafromuser);

		ret = -EINVAL;
		goto out;
	}

	/* Translate virtual address of caller into physical address */
	for (i = 0; i < rpc->commFromUser.nbr_bf; i++) {
		uint32_t type = rpc->commFromUser.cmds[i].type;
		void *buffer = rpc->commFromUser.cmds[i].buffer;

		if (type != TEE_RPC_BUFFER || buffer == NULL)
			continue;

		if (type & TEE_RPC_BUFFER_NONSECURE) {
		} else {
			struct tee_shm *shm;
			struct vm_area_struct *vma = find_vma(
					current->mm, (unsigned long) buffer);

			if (vma == NULL)
				continue;

			shm = vma->vm_private_data;

			if (shm == NULL) {
				pr_err(
						"Invalid vma->vm_private_data [%s:%d:%d]\n",
						current->comm,
						current->tgid,
						current->pid);

				rpc->res = -EINVAL;
				mutex_unlock(&rpc->insync);
				up(&rpc->datafromuser);

				ret = -EINVAL;
				goto out;
			}

			rpc->commFromUser.cmds[i].buffer =
				(void *) (unsigned long)
				shm->resv.paddr;
		}
	}

	rpc->res = 0;
	mutex_unlock(&rpc->insync);
	up(&rpc->datafromuser);
	ret = length;

out:
	return ret;
}

int tee_supp_init(struct tee *tee)
{
	struct tee_rpc *rpc;

	rpc = devm_kzalloc(tee->dev, sizeof(struct tee_rpc), GFP_KERNEL);
	if (rpc == NULL)
		return -ENOMEM;

	rpc->datafromuser = (struct semaphore)
		__SEMAPHORE_INITIALIZER(rpc->datafromuser, 0);
	rpc->datatouser = (struct semaphore)
		__SEMAPHORE_INITIALIZER(rpc->datatouser, 0);
	mutex_init(&rpc->thrd_mutex);
	mutex_init(&rpc->outsync);
	mutex_init(&rpc->insync);
	atomic_set(&rpc->used, 0);
	tee->rpc = rpc;

	return 0;
}

void tee_supp_deinit(struct tee *tee)
{
	devm_kfree(tee->dev, tee->rpc);
	tee->rpc = NULL;
}
