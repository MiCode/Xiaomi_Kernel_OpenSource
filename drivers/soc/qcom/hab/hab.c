/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "hab.h"

#define HAB_DEVICE_CNSTR(__name__, __id__, __num__) { \
	.name = __name__,\
	.id = __id__,\
	.pchannels = LIST_HEAD_INIT(hab_devices[__num__].pchannels),\
	.pchan_lock = __MUTEX_INITIALIZER(hab_devices[__num__].pchan_lock),\
	.openq_list = LIST_HEAD_INIT(hab_devices[__num__].openq_list),\
	.openlock = __SPIN_LOCK_UNLOCKED(&hab_devices[__num__].openlock)\
	}

/* the following has to match habmm definitions, order does not matter */
static struct hab_device hab_devices[] = {
	HAB_DEVICE_CNSTR(DEVICE_AUD1_NAME, MM_AUD_1, 0),
	HAB_DEVICE_CNSTR(DEVICE_AUD2_NAME, MM_AUD_2, 1),
	HAB_DEVICE_CNSTR(DEVICE_AUD3_NAME, MM_AUD_3, 2),
	HAB_DEVICE_CNSTR(DEVICE_AUD4_NAME, MM_AUD_4, 3),
	HAB_DEVICE_CNSTR(DEVICE_CAM_NAME, MM_CAM, 4),
	HAB_DEVICE_CNSTR(DEVICE_DISP1_NAME, MM_DISP_1, 5),
	HAB_DEVICE_CNSTR(DEVICE_DISP2_NAME, MM_DISP_2, 6),
	HAB_DEVICE_CNSTR(DEVICE_DISP3_NAME, MM_DISP_3, 7),
	HAB_DEVICE_CNSTR(DEVICE_DISP4_NAME, MM_DISP_4, 8),
	HAB_DEVICE_CNSTR(DEVICE_DISP5_NAME, MM_DISP_5, 9),
	HAB_DEVICE_CNSTR(DEVICE_GFX_NAME, MM_GFX, 10),
	HAB_DEVICE_CNSTR(DEVICE_VID_NAME, MM_VID, 11),
	HAB_DEVICE_CNSTR(DEVICE_MISC_NAME, MM_MISC, 12),
	HAB_DEVICE_CNSTR(DEVICE_QCPE1_NAME, MM_QCPE_VM1, 13),
	HAB_DEVICE_CNSTR(DEVICE_QCPE2_NAME, MM_QCPE_VM2, 14),
	HAB_DEVICE_CNSTR(DEVICE_QCPE3_NAME, MM_QCPE_VM3, 15),
	HAB_DEVICE_CNSTR(DEVICE_QCPE4_NAME, MM_QCPE_VM4, 16)
};

struct hab_driver hab_driver = {
	.ndevices = ARRAY_SIZE(hab_devices),
	.devp = hab_devices,
};

struct uhab_context *hab_ctx_alloc(int kernel)
{
	struct uhab_context *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->closing = 0;
	INIT_LIST_HEAD(&ctx->vchannels);
	INIT_LIST_HEAD(&ctx->exp_whse);
	INIT_LIST_HEAD(&ctx->imp_whse);

	INIT_LIST_HEAD(&ctx->exp_rxq);
	init_waitqueue_head(&ctx->exp_wq);
	spin_lock_init(&ctx->expq_lock);

	spin_lock_init(&ctx->imp_lock);
	rwlock_init(&ctx->exp_lock);
	rwlock_init(&ctx->ctx_lock);

	kref_init(&ctx->refcount);
	ctx->import_ctx = habmem_imp_hyp_open();
	if (!ctx->import_ctx) {
		kfree(ctx);
		return NULL;
	}
	ctx->kernel = kernel;

	return ctx;
}

void hab_ctx_free(struct kref *ref)
{
	struct uhab_context *ctx =
		container_of(ref, struct uhab_context, refcount);
	struct hab_export_ack_recvd *ack_recvd, *tmp;

	habmem_imp_hyp_close(ctx->import_ctx, ctx->kernel);

	list_for_each_entry_safe(ack_recvd, tmp, &ctx->exp_rxq, node) {
		list_del(&ack_recvd->node);
		kfree(ack_recvd);
	}

	kfree(ctx);
}

struct virtual_channel *hab_get_vchan_fromvcid(int32_t vcid,
		struct uhab_context *ctx)
{
	struct virtual_channel *vchan;

	read_lock(&ctx->ctx_lock);
	list_for_each_entry(vchan, &ctx->vchannels, node) {
		if (vcid == vchan->id) {
			kref_get(&vchan->refcount);
			read_unlock(&ctx->ctx_lock);
			return vchan;
		}
	}
	read_unlock(&ctx->ctx_lock);
	return NULL;
}

static struct hab_device *find_hab_device(unsigned int mm_id)
{
	int i;

	for (i = 0; i < hab_driver.ndevices; i++) {
		if (hab_driver.devp[i].id == HAB_MMID_GET_MAJOR(mm_id))
			return &hab_driver.devp[i];
	}

	pr_err("find_hab_device failed: id=%d\n", mm_id);
	return NULL;
}
/*
 *   open handshake in FE and BE

 *   frontend            backend
 *  send(INIT)          wait(INIT)
 *  wait(INIT_ACK)      send(INIT_ACK)
 *  send(ACK)           wait(ACK)

 */
struct virtual_channel *frontend_open(struct uhab_context *ctx,
		unsigned int mm_id,
		int dom_id)
{
	int ret, open_id = 0;
	struct physical_channel *pchan = NULL;
	struct hab_device *dev;
	struct virtual_channel *vchan = NULL;
	static atomic_t open_id_counter = ATOMIC_INIT(0);
	struct hab_open_request request;
	struct hab_open_request *recv_request;
	int sub_id = HAB_MMID_GET_MINOR(mm_id);

	dev = find_hab_device(mm_id);
	if (dev == NULL) {
		ret = -EINVAL;
		goto err;
	}

	pchan = hab_pchan_find_domid(dev, dom_id);
	if (!pchan) {
		pr_err("hab_pchan_find_domid failed: dom_id=%d\n", dom_id);
		ret = -EINVAL;
		goto err;
	}

	vchan = hab_vchan_alloc(ctx, pchan);
	if (!vchan) {
		ret = -ENOMEM;
		goto err;
	}

	/* Send Init sequence */
	open_id = atomic_inc_return(&open_id_counter);
	hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT, pchan,
		vchan->id, sub_id, open_id);
	ret = hab_open_request_send(&request);
	if (ret) {
		pr_err("hab_open_request_send failed: %d\n", ret);
		goto err;
	}

	/* Wait for Init-Ack sequence */
	hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT_ACK, pchan,
		0, sub_id, open_id);
	ret = hab_open_listen(ctx, dev, &request, &recv_request, 0);
	if (ret || !recv_request) {
		pr_err("hab_open_listen failed: %d\n", ret);
		goto err;
	}

	vchan->otherend_id = recv_request->vchan_id;
	hab_open_request_free(recv_request);

	/* Send Ack sequence */
	hab_open_request_init(&request, HAB_PAYLOAD_TYPE_ACK, pchan,
		0, sub_id, open_id);
	ret = hab_open_request_send(&request);
	if (ret)
		goto err;

	hab_pchan_put(pchan);

	return vchan;
err:
	if (vchan)
		hab_vchan_put(vchan);
	if (pchan)
		hab_pchan_put(pchan);

	return ERR_PTR(ret);
}

struct virtual_channel *backend_listen(struct uhab_context *ctx,
		unsigned int mm_id)
{
	int ret;
	int open_id;
	int sub_id = HAB_MMID_GET_MINOR(mm_id);
	struct physical_channel *pchan = NULL;
	struct hab_device *dev;
	struct virtual_channel *vchan = NULL;
	struct hab_open_request request;
	struct hab_open_request *recv_request;
	uint32_t otherend_vchan_id;

	dev = find_hab_device(mm_id);
	if (dev == NULL) {
		ret = -EINVAL;
		goto err;
	}

	while (1) {
		/* Wait for Init sequence */
		hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT,
			NULL, 0, sub_id, 0);
		ret = hab_open_listen(ctx, dev, &request, &recv_request, 0);
		if (ret || !recv_request) {
			pr_err("hab_open_listen failed: %d\n", ret);
			goto err;
		}

		otherend_vchan_id = recv_request->vchan_id;
		open_id = recv_request->open_id;
		pchan = recv_request->pchan;
		hab_pchan_get(pchan);
		hab_open_request_free(recv_request);

		vchan = hab_vchan_alloc(ctx, pchan);
		if (!vchan) {
			ret = -ENOMEM;
			goto err;
		}

		vchan->otherend_id = otherend_vchan_id;

		/* Send Init-Ack sequence */
		hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT_ACK,
				pchan, vchan->id, sub_id, open_id);
		ret = hab_open_request_send(&request);
		if (ret)
			goto err;

		/* Wait for Ack sequence */
		hab_open_request_init(&request, HAB_PAYLOAD_TYPE_ACK,
				pchan, 0, sub_id, open_id);
		ret = hab_open_listen(ctx, dev, &request, &recv_request, HZ);

		if (ret != -EAGAIN)
			break;

		hab_vchan_put(vchan);
		vchan = NULL;
		hab_pchan_put(pchan);
		pchan = NULL;
	}

	if (ret || !recv_request) {
		pr_err("backend_listen failed: %d\n", ret);
		ret = -EINVAL;
		goto err;
	}

	hab_open_request_free(recv_request);
	hab_pchan_put(pchan);
	return vchan;
err:
	if (vchan)
		hab_vchan_put(vchan);
	if (pchan)
		hab_pchan_put(pchan);
	return ERR_PTR(ret);
}

long hab_vchan_send(struct uhab_context *ctx,
		int vcid,
		size_t sizebytes,
		void *data,
		unsigned int flags)
{
	struct virtual_channel *vchan;
	int ret;
	struct hab_header header = HAB_HEADER_INITIALIZER;
	int nonblocking_flag = flags & HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING;

	if (sizebytes > HAB_MAX_MSG_SIZEBYTES) {
		pr_err("Message too large, %lu bytes\n", sizebytes);
		return -EINVAL;
	}

	vchan = hab_get_vchan_fromvcid(vcid, ctx);
	if (!vchan || vchan->otherend_closed)
		return -ENODEV;

	HAB_HEADER_SET_SIZE(header, sizebytes);
	HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_MSG);
	HAB_HEADER_SET_ID(header, vchan->otherend_id);

	while (1) {
		ret = physical_channel_send(vchan->pchan, &header, data);

		if (vchan->otherend_closed || nonblocking_flag ||
			ret != -EAGAIN)
			break;

		schedule();
	}

	hab_vchan_put(vchan);
	return ret;
}

struct hab_message *hab_vchan_recv(struct uhab_context *ctx,
				int vcid,
				unsigned int flags)
{
	struct virtual_channel *vchan;
	struct hab_message *message;
	int ret = 0;
	int nonblocking_flag = flags & HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING;

	vchan = hab_get_vchan_fromvcid(vcid, ctx);
	if (!vchan || vchan->otherend_closed)
		return ERR_PTR(-ENODEV);

	if (nonblocking_flag) {
		/*
		 * Try to pull data from the ring in this context instead of
		 * IRQ handler. Any available messages will be copied and queued
		 * internally, then fetched by hab_msg_dequeue()
		 */
		physical_channel_rx_dispatch((unsigned long) vchan->pchan);
	}

	message = hab_msg_dequeue(vchan, !nonblocking_flag);
	if (!message) {
		if (nonblocking_flag)
			ret = -EAGAIN;
		else
			ret = -EPIPE;
	}

	hab_vchan_put(vchan);
	return ret ? ERR_PTR(ret) : message;
}

bool hab_is_loopback(void)
{
	return hab_driver.b_loopback;
}

int hab_vchan_open(struct uhab_context *ctx,
		unsigned int mmid,
		int32_t *vcid,
		uint32_t flags)
{
	struct virtual_channel *vchan;

	if (!vcid)
		return -EINVAL;

	if (hab_is_loopback()) {
		if (!hab_driver.loopback_num) {
			hab_driver.loopback_num = 1;
			vchan = backend_listen(ctx, mmid);
		} else {
			hab_driver.loopback_num = 0;
			vchan = frontend_open(ctx, mmid, LOOPBACK_DOM);
		}
	} else {
		if (hab_driver.b_server_dom)
			vchan = backend_listen(ctx, mmid);
		else
			vchan = frontend_open(ctx, mmid, 0);
	}

	if (IS_ERR(vchan))
		return PTR_ERR(vchan);

	write_lock(&ctx->ctx_lock);
	list_add_tail(&vchan->node, &ctx->vchannels);
	write_unlock(&ctx->ctx_lock);

	*vcid = vchan->id;

	return 0;
}

void hab_send_close_msg(struct virtual_channel *vchan)
{
	struct hab_header header;

	if (vchan && !vchan->otherend_closed) {
		HAB_HEADER_SET_SIZE(header, 0);
		HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_CLOSE);
		HAB_HEADER_SET_ID(header, vchan->otherend_id);
		physical_channel_send(vchan->pchan, &header, NULL);
	}
}

static void hab_vchan_close_impl(struct kref *ref)
{
	struct virtual_channel *vchan =
		container_of(ref, struct virtual_channel, usagecnt);

	list_del(&vchan->node);
	hab_vchan_stop_notify(vchan);
	hab_vchan_put(vchan);
}


void hab_vchan_close(struct uhab_context *ctx, int32_t vcid)
{
	struct virtual_channel *vchan, *tmp;

	if (!ctx)
		return;

	write_lock(&ctx->ctx_lock);
	list_for_each_entry_safe(vchan, tmp, &ctx->vchannels, node) {
		if (vchan->id == vcid) {
			kref_put(&vchan->usagecnt, hab_vchan_close_impl);
			break;
		}
	}

	write_unlock(&ctx->ctx_lock);
}

static int hab_open(struct inode *inodep, struct file *filep)
{
	int result = 0;
	struct uhab_context *ctx;

	ctx = hab_ctx_alloc(0);

	if (!ctx) {
		pr_err("hab_ctx_alloc failed\n");
		filep->private_data = NULL;
		return -ENOMEM;
	}

	filep->private_data = ctx;

	return result;
}

static int hab_release(struct inode *inodep, struct file *filep)
{
	struct uhab_context *ctx = filep->private_data;
	struct virtual_channel *vchan, *tmp;

	if (!ctx)
		return 0;

	write_lock(&ctx->ctx_lock);

	list_for_each_entry_safe(vchan, tmp, &ctx->vchannels, node) {
		list_del(&vchan->node);
		hab_vchan_stop_notify(vchan);
		hab_vchan_put(vchan);
	}

	write_unlock(&ctx->ctx_lock);

	hab_ctx_put(ctx);
	filep->private_data = NULL;

	return 0;
}

static long hab_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct uhab_context *ctx = (struct uhab_context *)filep->private_data;
	struct hab_open *open_param;
	struct hab_close *close_param;
	struct hab_recv *recv_param;
	struct hab_send *send_param;
	struct hab_message *msg;
	void *send_data;
	unsigned char data[256] = { 0 };
	long ret = 0;

	if (_IOC_SIZE(cmd) && (cmd & IOC_IN)) {
		if (_IOC_SIZE(cmd) > sizeof(data))
			return -EINVAL;

		if (copy_from_user(data, (void __user *)arg, _IOC_SIZE(cmd))) {
			pr_err("copy_from_user failed cmd=%x size=%d\n",
				cmd, _IOC_SIZE(cmd));
			return -EFAULT;
		}
	}

	switch (cmd) {
	case IOCTL_HAB_VC_OPEN:
		open_param = (struct hab_open *)data;
		ret = hab_vchan_open(ctx, open_param->mmid,
			&open_param->vcid, open_param->flags);
		break;
	case IOCTL_HAB_VC_CLOSE:
		close_param = (struct hab_close *)data;
		hab_vchan_close(ctx, close_param->vcid);
		break;
	case IOCTL_HAB_SEND:
		send_param = (struct hab_send *)data;
		if (send_param->sizebytes > HAB_MAX_MSG_SIZEBYTES) {
			ret = -EINVAL;
			break;
		}

		send_data = kzalloc(send_param->sizebytes, GFP_TEMPORARY);
		if (!send_data) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(send_data, (void __user *)send_param->data,
				send_param->sizebytes)) {
			ret = -EFAULT;
		} else {
			ret = hab_vchan_send(ctx, send_param->vcid,
						send_param->sizebytes,
						send_data,
						send_param->flags);
		}
		kfree(send_data);
		break;
	case IOCTL_HAB_RECV:
		recv_param = (struct hab_recv *)data;
		if (!recv_param->data) {
			ret = -EINVAL;
			break;
		}

		msg = hab_vchan_recv(ctx, recv_param->vcid, recv_param->flags);

		if (IS_ERR(msg)) {
			recv_param->sizebytes = 0;
			ret = PTR_ERR(msg);
			break;
		}

		if (recv_param->sizebytes < msg->sizebytes) {
			recv_param->sizebytes = 0;
			ret = -EINVAL;
		} else if (copy_to_user((void __user *)recv_param->data,
					msg->data,
					msg->sizebytes)) {
			pr_err("copy_to_user failed: vc=%x size=%d\n",
				recv_param->vcid, (int)msg->sizebytes);
			recv_param->sizebytes = 0;
			ret = -EFAULT;
		} else {
			recv_param->sizebytes = msg->sizebytes;
		}

		hab_msg_free(msg);
		break;
	case IOCTL_HAB_VC_EXPORT:
		ret = hab_mem_export(ctx, (struct hab_export *)data, 0);
		break;
	case IOCTL_HAB_VC_IMPORT:
		ret = hab_mem_import(ctx, (struct hab_import *)data, 0);
		break;
	case IOCTL_HAB_VC_UNEXPORT:
		ret = hab_mem_unexport(ctx, (struct hab_unexport *)data, 0);
		break;
	case IOCTL_HAB_VC_UNIMPORT:
		ret = hab_mem_unimport(ctx, (struct hab_unimport *)data, 0);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	if (ret == 0 && _IOC_SIZE(cmd) && (cmd & IOC_OUT))
		if (copy_to_user((void __user *) arg, data, _IOC_SIZE(cmd))) {
			pr_err("copy_to_user failed: cmd=%x\n", cmd);
			ret = -EFAULT;
		}

	return ret;
}

static const struct file_operations hab_fops = {
	.owner = THIS_MODULE,
	.open = hab_open,
	.release = hab_release,
	.mmap = habmem_imp_hyp_mmap,
	.unlocked_ioctl = hab_ioctl
};

/*
 * These map sg functions are pass through because the memory backing the
 * sg list is already accessible to the kernel as they come from a the
 * dedicated shared vm pool
 */

static int hab_map_sg(struct device *dev, struct scatterlist *sgl,
	int nelems, enum dma_data_direction dir,
	struct dma_attrs *attrs)
{
	/* return nelems directly */
	return nelems;
}

static void hab_unmap_sg(struct device *dev,
	struct scatterlist *sgl, int nelems,
	enum dma_data_direction dir,
	struct dma_attrs *attrs)
{
	/*Do nothing */
}

static const struct dma_map_ops hab_dma_ops = {
	.map_sg		= hab_map_sg,
	.unmap_sg	= hab_unmap_sg,
};

static int __init hab_init(void)
{
	int result;
	int i;
	dev_t dev;
	struct hab_device *device;

	result = alloc_chrdev_region(&hab_driver.major, 0, 1, "hab");

	if (result < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", result);
		return result;
	}

	cdev_init(&hab_driver.cdev, &hab_fops);
	hab_driver.cdev.owner = THIS_MODULE;
	hab_driver.cdev.ops = &hab_fops;
	dev = MKDEV(MAJOR(hab_driver.major), 0);

	result = cdev_add(&hab_driver.cdev, dev, 1);

	if (result < 0) {
		unregister_chrdev_region(dev, 1);
		pr_err("cdev_add failed: %d\n", result);
		return result;
	}

	hab_driver.class = class_create(THIS_MODULE, "hab");

	if (IS_ERR(hab_driver.class)) {
		result = PTR_ERR(hab_driver.class);
		pr_err("class_create failed: %d\n", result);
		goto err;
	}

	hab_driver.dev = device_create(hab_driver.class, NULL,
					dev, &hab_driver, "hab");

	if (IS_ERR(hab_driver.dev)) {
		result = PTR_ERR(hab_driver.dev);
		pr_err("device_create failed: %d\n", result);
		goto err;
	}

	for (i = 0; i < hab_driver.ndevices; i++) {
		device = &hab_driver.devp[i];
		init_waitqueue_head(&device->openq);
	}

	hab_hypervisor_register();

	hab_driver.kctx = hab_ctx_alloc(1);
	if (!hab_driver.kctx) {
		pr_err("hab_ctx_alloc failed");
		result = -ENOMEM;
		hab_hypervisor_unregister();
		goto err;
	}

	set_dma_ops(hab_driver.dev, &hab_dma_ops);

	return result;

err:
	if (!IS_ERR_OR_NULL(hab_driver.dev))
		device_destroy(hab_driver.class, dev);
	if (!IS_ERR_OR_NULL(hab_driver.class))
		class_destroy(hab_driver.class);
	cdev_del(&hab_driver.cdev);
	unregister_chrdev_region(dev, 1);

	return result;
}

static void __exit hab_exit(void)
{
	dev_t dev;

	hab_hypervisor_unregister();
	hab_ctx_put(hab_driver.kctx);
	dev = MKDEV(MAJOR(hab_driver.major), 0);
	device_destroy(hab_driver.class, dev);
	class_destroy(hab_driver.class);
	cdev_del(&hab_driver.cdev);
	unregister_chrdev_region(dev, 1);
}

subsys_initcall(hab_init);
module_exit(hab_exit);

MODULE_DESCRIPTION("Hypervisor abstraction layer");
MODULE_LICENSE("GPL v2");
