// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/haven/hh_msgq.h>
#include <linux/kthread.h>
#include "sde_kms.h"
#include "sde_vm.h"

static void _sde_vm_msgq_process_msg(struct kthread_work *work)
{
	struct sde_vm_msg_work *vm_work =
		container_of(work, struct sde_vm_msg_work, work);
	struct sde_vm_ops *vm_ops = &vm_work->sde_vm->vm_ops;

	if (vm_ops->vm_msg_recv_cb)
		vm_ops->vm_msg_recv_cb(vm_work->sde_vm, vm_work->msg_buf,
				vm_work->msg_size);

	kfree(vm_work->msg_buf);
}

static int _sde_vm_msgq_listener(void *data)
{
	struct sde_vm *sde_vm = (struct sde_vm *)data;
	struct sde_kms *sde_kms = sde_vm->sde_kms;
	struct sde_vm_msg_work *vm_work;
	struct msm_drm_private *priv;
	struct msm_drm_thread *event_thread;
	void *buf;
	size_t size;
	int ret = 0;

	priv = sde_kms->dev->dev_private;
	event_thread = &priv->event_thread[0];
	vm_work = &sde_vm->vm_work;

	while (true) {
		buf = kzalloc(HH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		ret = hh_msgq_recv(sde_vm->msgq_handle, buf,
				HH_MSGQ_MAX_MSG_SIZE_BYTES, &size, 0);
		if (ret < 0) {
			kfree(buf);
			SDE_ERROR("hh_msgq_recv failed, rc=%d\n", ret);
			return -EINVAL;
		}

		vm_work->msg_buf = buf;
		vm_work->msg_size = size;
		vm_work->sde_vm = sde_vm;

		kthread_queue_work(&event_thread->worker, &vm_work->work);
	}

	return 0;
}

int sde_vm_msgq_send(struct sde_vm *sde_vm, void *msg, size_t msg_size)
{
	if (!sde_vm->msgq_handle) {
		SDE_ERROR("Failed to send msg, invalid msgq handle\n");
		return -EINVAL;
	}

	if (msg_size > HH_MSGQ_MAX_MSG_SIZE_BYTES) {
		SDE_ERROR("msg size unsupported for msgq: %d > %d\n", msg_size,
				HH_MSGQ_MAX_MSG_SIZE_BYTES);
		return -E2BIG;
	}

	return hh_msgq_send(sde_vm->msgq_handle, msg, msg_size, HH_MSGQ_TX_PUSH);
}

int sde_vm_msgq_init(struct sde_vm *sde_vm)
{
	struct sde_vm_ops *vm_ops = &sde_vm->vm_ops;
	void *msgq_handle = NULL;
	struct task_struct *msgq_listener_thread = NULL;
	int rc = 0;

	msgq_handle = hh_msgq_register(HH_MSGQ_LABEL_DISPLAY);
	if (IS_ERR(msgq_handle)) {
		SDE_ERROR("hh_msgq_register failed, hdl=%p\n", msgq_handle);
		return -EINVAL;
	}

	sde_vm->msgq_handle = msgq_handle;

	if (!vm_ops->vm_msg_recv_cb)
		goto done;

	msgq_listener_thread = kthread_run(_sde_vm_msgq_listener,
			(void *)sde_vm, "disp_msgq_listener");
	if (IS_ERR(msgq_listener_thread)) {
		SDE_ERROR("kthread creation failed for msgq, hdl: %p\n",
				msgq_listener_thread);
		rc = -EINVAL;
		goto kthread_create_fail;
	}

	kthread_init_work(&sde_vm->vm_work.work, _sde_vm_msgq_process_msg);

	sde_vm->msgq_listener_thread = msgq_listener_thread;

	return 0;

kthread_create_fail:
	hh_msgq_unregister(msgq_handle);
	sde_vm->msgq_handle = NULL;
done:
	return rc;
}

void sde_vm_msgq_deinit(struct sde_vm *sde_vm)
{
	if (sde_vm->msgq_listener_thread)
		kthread_stop(sde_vm->msgq_listener_thread);

	if (sde_vm->msgq_handle)
		hh_msgq_unregister(sde_vm->msgq_handle);
}
