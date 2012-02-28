/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/android_pmem.h>
#include <linux/clk.h>
#include <media/msm/vidc_type.h>
#include <media/msm/vcd_api.h>
#include <media/msm/vidc_init.h>

#include "venc_internal.h"
#include "vcd_res_tracker_api.h"

#define VID_ENC_NAME	"msm_vidc_enc"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define INFO(x...) printk(KERN_INFO x)
#define ERR(x...) printk(KERN_ERR x)

static struct vid_enc_dev *vid_enc_device_p;
static dev_t vid_enc_dev_num;
static struct class *vid_enc_class;
static long vid_enc_ioctl(struct file *file,
	unsigned cmd, unsigned long arg);
static int stop_cmd;

static s32 vid_enc_get_empty_client_index(void)
{
	u32 i;
	u32 found = false;

	for (i = 0; i < VIDC_MAX_NUM_CLIENTS; i++) {
		if (!vid_enc_device_p->venc_clients[i].vcd_handle) {
			found = true;
			break;
		}
	}
	if (!found) {
		ERR("%s():ERROR No space for new client\n",
			__func__);
		return -ENOMEM;
	} else {
		DBG("%s(): available client index = %u\n",
			__func__, i);
		return i;
	}
}


u32 vid_enc_get_status(u32 status)
{
	u32 venc_status;

	switch (status) {
	case VCD_S_SUCCESS:
		venc_status = VEN_S_SUCCESS;
		break;
	case VCD_ERR_FAIL:
		venc_status = VEN_S_EFAIL;
		break;
	case VCD_ERR_ALLOC_FAIL:
		venc_status = VEN_S_ENOSWRES;
		break;
	case VCD_ERR_ILLEGAL_OP:
		venc_status = VEN_S_EINVALCMD;
		break;
	case VCD_ERR_ILLEGAL_PARM:
		venc_status = VEN_S_EBADPARAM;
		break;
	case VCD_ERR_BAD_POINTER:
	case VCD_ERR_BAD_HANDLE:
		venc_status = VEN_S_EFATAL;
		break;
	case VCD_ERR_NOT_SUPPORTED:
		venc_status = VEN_S_ENOTSUPP;
		break;
	case VCD_ERR_BAD_STATE:
		venc_status = VEN_S_EINVALSTATE;
		break;
	case VCD_ERR_MAX_CLIENT:
		venc_status = VEN_S_ENOHWRES;
		break;
	default:
		venc_status = VEN_S_EFAIL;
		break;
	}
	return venc_status;
}

static void vid_enc_notify_client(struct video_client_ctx *client_ctx)
{
	if (client_ctx)
		complete(&client_ctx->event);
}

void vid_enc_vcd_open_done(struct video_client_ctx *client_ctx,
	struct vcd_handle_container *handle_container)
{
	DBG("vid_enc_vcd_open_done\n");

	if (client_ctx) {
		if (handle_container)
			client_ctx->vcd_handle = handle_container->handle;
		else
		ERR("%s(): ERROR. handle_container is NULL\n",
		__func__);
		vid_enc_notify_client(client_ctx);
	} else
		ERR("%s(): ERROR. client_ctx is NULL\n",
			__func__);
}

static void vid_enc_input_frame_done(struct video_client_ctx *client_ctx,
		u32 event, u32 status,
		struct vcd_frame_data *vcd_frame_data)
{
	struct vid_enc_msg *venc_msg;

	if (!client_ctx || !vcd_frame_data) {
		ERR("vid_enc_input_frame_done() NULL pointer\n");
		return;
	}

	venc_msg = kzalloc(sizeof(struct vid_enc_msg),
					    GFP_KERNEL);
	if (!venc_msg) {
		ERR("vid_enc_input_frame_done(): cannot allocate vid_enc_msg "
		" buffer\n");
		return;
	}

	venc_msg->venc_msg_info.statuscode = vid_enc_get_status(status);

	venc_msg->venc_msg_info.msgcode = VEN_MSG_INPUT_BUFFER_DONE;

	switch (event) {
	case VCD_EVT_RESP_INPUT_DONE:
	   DBG("Send INPUT_DON message to client = %p\n",
			client_ctx);
	   break;
	case VCD_EVT_RESP_INPUT_FLUSHED:
		DBG("Send INPUT_FLUSHED message to client = %p\n",
			client_ctx);
	   break;
	default:
		ERR("vid_enc_input_frame_done(): invalid event type: "
			"%d\n", event);
		venc_msg->venc_msg_info.statuscode = VEN_S_EFATAL;
	   break;
	}

	venc_msg->venc_msg_info.buf.clientdata =
		(void *)vcd_frame_data->frm_clnt_data;
	venc_msg->venc_msg_info.msgdata_size =
		sizeof(struct vid_enc_msg);

	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&venc_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}

static void vid_enc_output_frame_done(struct video_client_ctx *client_ctx,
		u32 event, u32 status,
		struct vcd_frame_data *vcd_frame_data)
{
	struct vid_enc_msg *venc_msg;
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 ion_flag = 0;
	struct ion_handle *buff_handle = NULL;

	if (!client_ctx || !vcd_frame_data) {
		ERR("vid_enc_input_frame_done() NULL pointer\n");
		return;
	}

	venc_msg = kzalloc(sizeof(struct vid_enc_msg),
					   GFP_KERNEL);
	if (!venc_msg) {
		ERR("vid_enc_input_frame_done(): cannot allocate vid_enc_msg "
		" buffer\n");
		return;
	}

	venc_msg->venc_msg_info.statuscode = vid_enc_get_status(status);
	venc_msg->venc_msg_info.msgcode = VEN_MSG_OUTPUT_BUFFER_DONE;

	switch (event) {
	case VCD_EVT_RESP_OUTPUT_DONE:
	   DBG("Send INPUT_DON message to client = %p\n",
			client_ctx);
	   break;
	case VCD_EVT_RESP_OUTPUT_FLUSHED:
	   DBG("Send INPUT_FLUSHED message to client = %p\n",
		   client_ctx);
	   break;
	default:
	   ERR("QVD: vid_enc_output_frame_done invalid cmd type: %d\n", event);
	   venc_msg->venc_msg_info.statuscode = VEN_S_EFATAL;
	   break;
	}

	kernel_vaddr =
		(unsigned long)vcd_frame_data->virtual;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
		false, &user_vaddr, &kernel_vaddr,
		&phy_addr, &pmem_fd, &file,
		&buffer_index)) {

		/* Buffer address in user space */
		venc_msg->venc_msg_info.buf.ptrbuffer =	(u8 *) user_vaddr;
		/* Buffer address in user space */
		venc_msg->venc_msg_info.buf.clientdata = (void *)
		vcd_frame_data->frm_clnt_data;
		/* Data length */
		venc_msg->venc_msg_info.buf.len =
			vcd_frame_data->data_len;
		venc_msg->venc_msg_info.buf.flags =
			vcd_frame_data->flags;
		/* Timestamp pass-through from input frame */
		venc_msg->venc_msg_info.buf.timestamp =
			vcd_frame_data->time_stamp;

		/* Decoded picture width and height */
		venc_msg->venc_msg_info.msgdata_size =
			sizeof(struct venc_buffer);
	} else {
		ERR("vid_enc_output_frame_done UVA can not be found\n");
		venc_msg->venc_msg_info.statuscode =
			VEN_S_EFATAL;
	}
	if (venc_msg->venc_msg_info.buf.len > 0) {
		ion_flag = vidc_get_fd_info(client_ctx, BUFFER_TYPE_OUTPUT,
					pmem_fd, kernel_vaddr, buffer_index,
					&buff_handle);
		if (ion_flag == CACHED) {
			msm_ion_do_cache_op(client_ctx->user_ion_client,
				buff_handle,
				(unsigned long *) kernel_vaddr,
				(unsigned long)venc_msg->venc_msg_info.buf.len,
				ION_IOC_CLEAN_INV_CACHES);
		}
	}
	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&venc_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}

static void vid_enc_lean_event(struct video_client_ctx *client_ctx,
	u32 event, u32 status)
{
	struct vid_enc_msg *venc_msg;
	if (!client_ctx) {
		ERR("%s(): !client_ctx pointer\n",
			__func__);
		return;
	}

	venc_msg = kzalloc(sizeof(struct vid_enc_msg),
			GFP_KERNEL);
	if (!venc_msg) {
		ERR("%s(): cannot allocate vid_enc_msg buffer\n",
			__func__);
		return;
	}

	venc_msg->venc_msg_info.statuscode =
		vid_enc_get_status(status);

	switch (event) {
	case VCD_EVT_RESP_FLUSH_INPUT_DONE:
		INFO("\n msm_vidc_enc: Sending VCD_EVT_RESP_FLUSH_INPUT_DONE"
			 " to client");
		venc_msg->venc_msg_info.msgcode =
			VEN_MSG_FLUSH_INPUT_DONE;
		break;
	case VCD_EVT_RESP_FLUSH_OUTPUT_DONE:
		INFO("\n msm_vidc_enc: Sending VCD_EVT_RESP_FLUSH_OUTPUT_DONE"
			 " to client");
		venc_msg->venc_msg_info.msgcode =
			VEN_MSG_FLUSH_OUPUT_DONE;
		break;

	case VCD_EVT_RESP_START:
		INFO("\n msm_vidc_enc: Sending VCD_EVT_RESP_START"
			 " to client");
		venc_msg->venc_msg_info.msgcode =
			VEN_MSG_START;
		break;

	case VCD_EVT_RESP_STOP:
		INFO("\n msm_vidc_enc: Sending VCD_EVT_RESP_STOP"
			 " to client");
		venc_msg->venc_msg_info.msgcode =
			VEN_MSG_STOP;
		break;

	case VCD_EVT_RESP_PAUSE:
		INFO("\n msm_vidc_enc: Sending VCD_EVT_RESP_PAUSE"
			 " to client");
		venc_msg->venc_msg_info.msgcode =
			VEN_MSG_PAUSE;
		break;

	default:
		ERR("%s() : unknown event type %u\n",
			__func__, event);
		break;
	}

	venc_msg->venc_msg_info.msgdata_size = 0;

	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&venc_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}


void vid_enc_vcd_cb(u32 event, u32 status,
	void *info, size_t sz, void *handle,
	void *const client_data)
{
	struct video_client_ctx *client_ctx =
		(struct video_client_ctx *)client_data;

	DBG("Entering %s()\n", __func__);

	if (!client_ctx) {
		ERR("%s(): client_ctx is NULL\n", __func__);
		return;
	}

	client_ctx->event_status = status;

	switch (event) {
	case VCD_EVT_RESP_OPEN:
		vid_enc_vcd_open_done(client_ctx,
		(struct vcd_handle_container *)info);
		break;

	case VCD_EVT_RESP_INPUT_DONE:
	case VCD_EVT_RESP_INPUT_FLUSHED:
		vid_enc_input_frame_done(client_ctx, event,
		status, (struct vcd_frame_data *)info);
		break;

	case VCD_EVT_RESP_OUTPUT_DONE:
	case VCD_EVT_RESP_OUTPUT_FLUSHED:
		vid_enc_output_frame_done(client_ctx, event, status,
		(struct vcd_frame_data *)info);
		break;

	case VCD_EVT_RESP_PAUSE:
	case VCD_EVT_RESP_START:
	case VCD_EVT_RESP_STOP:
	case VCD_EVT_RESP_FLUSH_INPUT_DONE:
	case VCD_EVT_RESP_FLUSH_OUTPUT_DONE:
	case VCD_EVT_IND_OUTPUT_RECONFIG:
	case VCD_EVT_IND_HWERRFATAL:
	case VCD_EVT_IND_RESOURCES_LOST:
		vid_enc_lean_event(client_ctx, event, status);
		break;

	default:
		ERR("%s() :  Error - Invalid event type =%u\n",
		__func__, event);
		break;
	}
}

static u32 vid_enc_msg_pending(struct video_client_ctx *client_ctx)
{
	u32 islist_empty = 0;

	mutex_lock(&client_ctx->msg_queue_lock);
	islist_empty = list_empty(&client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);

	if (islist_empty) {
		DBG("%s(): vid_enc msg queue empty\n",
			__func__);
		if (client_ctx->stop_msg) {
			DBG("%s(): List empty and Stop Msg set\n",
				__func__);
			return client_ctx->stop_msg;
		}
	} else
		DBG("%s(): vid_enc msg queue Not empty\n",
			__func__);

	return !islist_empty;
}

static int vid_enc_get_next_msg(struct video_client_ctx *client_ctx,
		struct venc_msg *venc_msg_info)
{
	int rc;
	struct vid_enc_msg *vid_enc_msg = NULL;

	if (!client_ctx)
		return -EIO;

	rc = wait_event_interruptible(client_ctx->msg_wait,
		vid_enc_msg_pending(client_ctx));

	if (rc < 0) {
		DBG("rc = %d,stop_msg= %u\n", rc, client_ctx->stop_msg);
		return rc;
	} else if (client_ctx->stop_msg) {
		DBG("stopped stop_msg = %u\n", client_ctx->stop_msg);
		return -EIO;
	}

	mutex_lock(&client_ctx->msg_queue_lock);

	if (!list_empty(&client_ctx->msg_queue)) {
		DBG("%s(): After Wait\n", __func__);
		vid_enc_msg = list_first_entry(&client_ctx->msg_queue,
					struct vid_enc_msg, list);
		list_del(&vid_enc_msg->list);
		memcpy(venc_msg_info, &vid_enc_msg->venc_msg_info,
		sizeof(struct venc_msg));
		kfree(vid_enc_msg);
	}
	mutex_unlock(&client_ctx->msg_queue_lock);
	return 0;
}

static u32 vid_enc_close_client(struct video_client_ctx *client_ctx)
{
	struct vid_enc_msg *vid_enc_msg = NULL;
	u32 vcd_status;
	int rc;

	INFO("\n msm_vidc_enc: Inside %s()", __func__);
	if (!client_ctx || (!client_ctx->vcd_handle)) {
		ERR("\n %s(): Invalid client_ctx", __func__);
		return false;
	}

	mutex_lock(&vid_enc_device_p->lock);

	if (!stop_cmd) {
		vcd_status = vcd_stop(client_ctx->vcd_handle);
		DBG("Waiting for VCD_STOP: Before Timeout\n");
		if (!vcd_status) {
			rc = wait_for_completion_timeout(&client_ctx->event,
				5 * HZ);
			if (!rc) {
				ERR("%s:ERROR vcd_stop time out"
				"rc = %d\n", __func__, rc);
			}

			if (client_ctx->event_status) {
				ERR("%s:ERROR "
				"vcd_stop Not successs\n", __func__);
			}
		}
	}
	DBG("VCD_STOPPED: After Timeout, calling VCD_CLOSE\n");
	mutex_lock(&client_ctx->msg_queue_lock);
	while (!list_empty(&client_ctx->msg_queue)) {
		DBG("%s(): Delete remaining entries\n", __func__);
		vid_enc_msg = list_first_entry(&client_ctx->msg_queue,
					struct vid_enc_msg, list);
		list_del(&vid_enc_msg->list);
		kfree(vid_enc_msg);
	}
	mutex_unlock(&client_ctx->msg_queue_lock);
	vcd_status = vcd_close(client_ctx->vcd_handle);

	if (vcd_status) {
		mutex_unlock(&vid_enc_device_p->lock);
		return false;
	}
	memset((void *)client_ctx, 0,
		sizeof(struct video_client_ctx));

	vid_enc_device_p->num_clients--;
	stop_cmd = 0;
	mutex_unlock(&vid_enc_device_p->lock);
	return true;
}


static int vid_enc_open(struct inode *inode, struct file *file)
{
	s32 client_index;
	struct video_client_ctx *client_ctx;
	u32 vcd_status = VCD_ERR_FAIL;
	u8 client_count = 0;

	INFO("\n msm_vidc_enc: Inside %s()", __func__);

	mutex_lock(&vid_enc_device_p->lock);

	stop_cmd = 0;
	client_count = vcd_get_num_of_clients();
	if (client_count == VIDC_MAX_NUM_CLIENTS ||
		res_trk_check_for_sec_session()) {
		ERR("ERROR : vid_enc_open() max number of clients"
		    "limit reached or secure session is open\n");
		mutex_unlock(&vid_enc_device_p->lock);
		return -ENODEV;
	}

	DBG(" Virtual Address of ioremap is %p\n", vid_enc_device_p->virt_base);
	if (!vid_enc_device_p->num_clients) {
		if (!vidc_load_firmware())
			return -ENODEV;
	}

	client_index = vid_enc_get_empty_client_index();

	if (client_index == -1) {
		ERR("%s() : No free clients client_index == -1\n",
			__func__);
		return -ENODEV;
	}

	client_ctx =
		&vid_enc_device_p->venc_clients[client_index];
	vid_enc_device_p->num_clients++;

	init_completion(&client_ctx->event);
	mutex_init(&client_ctx->msg_queue_lock);
	mutex_init(&client_ctx->enrty_queue_lock);
	INIT_LIST_HEAD(&client_ctx->msg_queue);
	init_waitqueue_head(&client_ctx->msg_wait);
	if (vcd_get_ion_status()) {
		client_ctx->user_ion_client = vcd_get_ion_client();
		if (!client_ctx->user_ion_client) {
			ERR("vcd_open ion get client failed");
			return -EFAULT;
		}
	}
	vcd_status = vcd_open(vid_enc_device_p->device_handle, false,
		vid_enc_vcd_cb, client_ctx);
	client_ctx->stop_msg = 0;

	if (!vcd_status) {
		wait_for_completion(&client_ctx->event);
		if (client_ctx->event_status) {
			ERR("callback for vcd_open returned error: %u",
				client_ctx->event_status);
			mutex_unlock(&vid_enc_device_p->lock);
			return -EFAULT;
		}
	} else {
		ERR("vcd_open returned error: %u", vcd_status);
		mutex_unlock(&vid_enc_device_p->lock);
		return -EFAULT;
	}
	file->private_data = client_ctx;
	mutex_unlock(&vid_enc_device_p->lock);
	return 0;
}

static int vid_enc_release(struct inode *inode, struct file *file)
{
	struct video_client_ctx *client_ctx = file->private_data;
	INFO("\n msm_vidc_enc: Inside %s()", __func__);
	vid_enc_close_client(client_ctx);
	vidc_release_firmware();
#ifndef USE_RES_TRACKER
	vidc_disable_clk();
#endif
	INFO("\n msm_vidc_enc: Return from %s()", __func__);
	return 0;
}

static const struct file_operations vid_enc_fops = {
	.owner = THIS_MODULE,
	.open = vid_enc_open,
	.release = vid_enc_release,
	.unlocked_ioctl = vid_enc_ioctl,
};

void vid_enc_interrupt_deregister(void)
{
}

void vid_enc_interrupt_register(void *device_name)
{
}

void vid_enc_interrupt_clear(void)
{
}

void *vid_enc_map_dev_base_addr(void *device_name)
{
	return vid_enc_device_p->virt_base;
}

static int vid_enc_vcd_init(void)
{
	int rc;
	struct vcd_init_config vcd_init_config;
	u32 i;

	INFO("\n msm_vidc_enc: Inside %s()", __func__);
	vid_enc_device_p->num_clients = 0;

	for (i = 0; i < VIDC_MAX_NUM_CLIENTS; i++) {
		memset((void *)&vid_enc_device_p->venc_clients[i], 0,
		sizeof(vid_enc_device_p->venc_clients[i]));
	}

	mutex_init(&vid_enc_device_p->lock);
	vid_enc_device_p->virt_base = vidc_get_ioaddr();

	if (!vid_enc_device_p->virt_base) {
		ERR("%s() : ioremap failed\n", __func__);
		return -ENOMEM;
	}

	vcd_init_config.device_name = "VIDC";
	vcd_init_config.map_dev_base_addr =
		vid_enc_map_dev_base_addr;
	vcd_init_config.interrupt_clr =
		vid_enc_interrupt_clear;
	vcd_init_config.register_isr =
		vid_enc_interrupt_register;
	vcd_init_config.deregister_isr =
		vid_enc_interrupt_deregister;

	rc = vcd_init(&vcd_init_config,
		&vid_enc_device_p->device_handle);

	if (rc) {
		ERR("%s() : vcd_init failed\n",
			__func__);
		return -ENODEV;
	}
	return 0;
}

static int __init vid_enc_init(void)
{
	int rc = 0;
	struct device *class_devp;

	INFO("\n msm_vidc_enc: Inside %s()", __func__);
	vid_enc_device_p = kzalloc(sizeof(struct vid_enc_dev),
					 GFP_KERNEL);
	if (!vid_enc_device_p) {
		ERR("%s Unable to allocate memory for vid_enc_dev\n",
			__func__);
		return -ENOMEM;
	}

	rc = alloc_chrdev_region(&vid_enc_dev_num, 0, 1, VID_ENC_NAME);
	if (rc < 0) {
		ERR("%s: alloc_chrdev_region Failed rc = %d\n",
			__func__, rc);
		goto error_vid_enc_alloc_chrdev_region;
	}

	vid_enc_class = class_create(THIS_MODULE, VID_ENC_NAME);
	if (IS_ERR(vid_enc_class)) {
		rc = PTR_ERR(vid_enc_class);
		ERR("%s: couldn't create vid_enc_class rc = %d\n",
			__func__, rc);
		goto error_vid_enc_class_create;
	}

	class_devp = device_create(vid_enc_class, NULL,
				vid_enc_dev_num, NULL, VID_ENC_NAME);

	if (IS_ERR(class_devp)) {
		rc = PTR_ERR(class_devp);
		ERR("%s: class device_create failed %d\n",
		__func__, rc);
		goto error_vid_enc_class_device_create;
	}

	vid_enc_device_p->device = class_devp;

	cdev_init(&vid_enc_device_p->cdev, &vid_enc_fops);
	vid_enc_device_p->cdev.owner = THIS_MODULE;
	rc = cdev_add(&(vid_enc_device_p->cdev), vid_enc_dev_num, 1);

	if (rc < 0) {
		ERR("%s: cdev_add failed %d\n",
		__func__, rc);
		goto error_vid_enc_cdev_add;
	}
	vid_enc_vcd_init();
	return 0;

error_vid_enc_cdev_add:
	device_destroy(vid_enc_class, vid_enc_dev_num);
error_vid_enc_class_device_create:
	class_destroy(vid_enc_class);
error_vid_enc_class_create:
	unregister_chrdev_region(vid_enc_dev_num, 1);
error_vid_enc_alloc_chrdev_region:
	kfree(vid_enc_device_p);

	return rc;
}

static void __exit vid_enc_exit(void)
{
	INFO("\n msm_vidc_enc: Inside %s()", __func__);
	cdev_del(&(vid_enc_device_p->cdev));
	device_destroy(vid_enc_class, vid_enc_dev_num);
	class_destroy(vid_enc_class);
	unregister_chrdev_region(vid_enc_dev_num, 1);
	kfree(vid_enc_device_p);
	INFO("\n msm_vidc_enc: Return from %s()", __func__);
}
static long vid_enc_ioctl(struct file *file,
		unsigned cmd, unsigned long u_arg)
{
	struct video_client_ctx *client_ctx = NULL;
	struct venc_ioctl_msg venc_msg;
	void __user *arg = (void __user *)u_arg;
	u32 result = true;
	int result_read = -1;

	DBG("%s\n", __func__);

	client_ctx = (struct video_client_ctx *)file->private_data;
	if (!client_ctx) {
		ERR("!client_ctx. Cannot attach to device handle\n");
		return -ENODEV;
	}

	switch (cmd) {
	case VEN_IOCTL_CMD_READ_NEXT_MSG:
	{
		struct venc_msg cb_msg;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_CMD_READ_NEXT_MSG\n");
		result_read = vid_enc_get_next_msg(client_ctx, &cb_msg);
		if (result_read < 0)
			return result_read;
		if (copy_to_user(venc_msg.out, &cb_msg, sizeof(cb_msg)))
			return -EFAULT;
		break;
	}
	case VEN_IOCTL_CMD_STOP_READ_MSG:
	{
		DBG("VEN_IOCTL_CMD_STOP_READ_MSG\n");
		client_ctx->stop_msg = 1;
		wake_up(&client_ctx->msg_wait);
		break;
	}
	case VEN_IOCTL_CMD_ENCODE_FRAME:
	case VEN_IOCTL_CMD_FILL_OUTPUT_BUFFER:
	{
		struct venc_buffer enc_buffer;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_CMD_ENCODE_FRAME"
			"/VEN_IOCTL_CMD_FILL_OUTPUT_BUFFER\n");
		if (copy_from_user(&enc_buffer, venc_msg.in,
						   sizeof(enc_buffer)))
			return -EFAULT;
		if (cmd == VEN_IOCTL_CMD_ENCODE_FRAME)
			result = vid_enc_encode_frame(client_ctx,
					&enc_buffer);
		else
			result = vid_enc_fill_output_buffer(client_ctx,
					&enc_buffer);
		if (!result) {
			DBG("\n VEN_IOCTL_CMD_ENCODE_FRAME/"
				"VEN_IOCTL_CMD_FILL_OUTPUT_BUFFER failed");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_INPUT_BUFFER:
	case VEN_IOCTL_SET_OUTPUT_BUFFER:
	{
		enum venc_buffer_dir buffer_dir;
		struct venc_bufferpayload buffer_info;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_SET_INPUT_BUFFER/VEN_IOCTL_SET_OUTPUT_BUFFER\n");
		if (copy_from_user(&buffer_info, venc_msg.in,
			sizeof(buffer_info)))
			return -EFAULT;
		buffer_dir = VEN_BUFFER_TYPE_INPUT;
		if (cmd == VEN_IOCTL_SET_OUTPUT_BUFFER)
			buffer_dir = VEN_BUFFER_TYPE_OUTPUT;
		result = vid_enc_set_buffer(client_ctx, &buffer_info,
				buffer_dir);
		if (!result) {
			DBG("\n VEN_IOCTL_SET_INPUT_BUFFER"
				"/VEN_IOCTL_SET_OUTPUT_BUFFER failed");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_CMD_FREE_INPUT_BUFFER:
	case VEN_IOCTL_CMD_FREE_OUTPUT_BUFFER:
	{
		enum venc_buffer_dir buffer_dir;
		struct venc_bufferpayload buffer_info;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_CMD_FREE_INPUT_BUFFER/"
			"VEN_IOCTL_CMD_FREE_OUTPUT_BUFFER\n");

		if (copy_from_user(&buffer_info, venc_msg.in,
			sizeof(buffer_info)))
			return -EFAULT;

		buffer_dir = VEN_BUFFER_TYPE_INPUT;
		if (cmd == VEN_IOCTL_CMD_FREE_OUTPUT_BUFFER)
			buffer_dir = VEN_BUFFER_TYPE_OUTPUT;

		result = vid_enc_free_buffer(client_ctx, &buffer_info,
				buffer_dir);
		if (!result) {
			DBG("\n VEN_IOCTL_CMD_FREE_OUTPUT_BUFFER"
				"/VEN_IOCTL_CMD_FREE_OUTPUT_BUFFER failed");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_INPUT_BUFFER_REQ:
	case VEN_IOCTL_SET_OUTPUT_BUFFER_REQ:
	{
		struct venc_allocatorproperty allocatorproperty;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_SET_INPUT_BUFFER_REQ"
			"/VEN_IOCTL_SET_OUTPUT_BUFFER_REQ\n");

		if (copy_from_user(&allocatorproperty, venc_msg.in,
			sizeof(allocatorproperty)))
			return -EFAULT;

		if (cmd == VEN_IOCTL_SET_OUTPUT_BUFFER_REQ)
				result = vid_enc_set_buffer_req(client_ctx,
						&allocatorproperty, false);
		else
			result = vid_enc_set_buffer_req(client_ctx,
					&allocatorproperty, true);
		if (!result) {
			DBG("setting VEN_IOCTL_SET_OUTPUT_BUFFER_REQ/"
			"VEN_IOCTL_SET_INPUT_BUFFER_REQ failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_GET_INPUT_BUFFER_REQ:
	case VEN_IOCTL_GET_OUTPUT_BUFFER_REQ:
	{
		struct venc_allocatorproperty allocatorproperty;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_GET_INPUT_BUFFER_REQ/"
			"VEN_IOCTL_GET_OUTPUT_BUFFER_REQ\n");

		if (cmd == VEN_IOCTL_GET_OUTPUT_BUFFER_REQ)
			result = vid_enc_get_buffer_req(client_ctx,
					&allocatorproperty, false);
		else
			result = vid_enc_get_buffer_req(client_ctx,
					&allocatorproperty, true);
		if (!result)
			return -EIO;
		if (copy_to_user(venc_msg.out, &allocatorproperty,
				sizeof(allocatorproperty)))
			return -EFAULT;
		break;
	}
	case VEN_IOCTL_CMD_FLUSH:
	{
		struct venc_bufferflush bufferflush;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_CMD_FLUSH\n");
		if (copy_from_user(&bufferflush, venc_msg.in,
			sizeof(bufferflush)))
			return -EFAULT;
		INFO("\n %s(): Calling vid_enc_flush with mode = %lu",
			 __func__, bufferflush.flush_mode);
		result = vid_enc_flush(client_ctx, &bufferflush);

		if (!result) {
			ERR("setting VEN_IOCTL_CMD_FLUSH failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_CMD_START:
	{
		INFO("\n %s(): Executing VEN_IOCTL_CMD_START", __func__);
		result = vid_enc_start_stop(client_ctx, true);
		if (!result) {
			ERR("setting VEN_IOCTL_CMD_START failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_CMD_STOP:
	{
		INFO("\n %s(): Executing VEN_IOCTL_CMD_STOP", __func__);
		result = vid_enc_start_stop(client_ctx, false);
		if (!result) {
			ERR("setting VEN_IOCTL_CMD_STOP failed\n");
			return -EIO;
		}
		stop_cmd = 1;
		break;
	}
	case VEN_IOCTL_CMD_PAUSE:
	{
		INFO("\n %s(): Executing VEN_IOCTL_CMD_PAUSE", __func__);
		result = vid_enc_pause_resume(client_ctx, true);
		if (!result) {
			ERR("setting VEN_IOCTL_CMD_PAUSE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_CMD_RESUME:
	{
		INFO("\n %s(): Executing VEN_IOCTL_CMD_RESUME", __func__);
		result = vid_enc_pause_resume(client_ctx, false);
		if (!result) {
			ERR("setting VEN_IOCTL_CMD_RESUME failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_RECON_BUFFER:
	{
		struct venc_recon_addr venc_recon;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_SET_RECON_BUFFER\n");
		if (copy_from_user(&venc_recon, venc_msg.in,
				sizeof(venc_recon)))
				return -EFAULT;
		result = vid_enc_set_recon_buffers(client_ctx,
					&venc_recon);
		if (!result) {
			ERR("setting VEN_IOCTL_SET_RECON_BUFFER failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_FREE_RECON_BUFFER:
	{
		struct venc_recon_addr venc_recon;
		DBG("VEN_IOCTL_FREE_RECON_BUFFER\n");
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		if (copy_from_user(&venc_recon, venc_msg.in,
				sizeof(venc_recon)))
				return -EFAULT;
		result = vid_enc_free_recon_buffers(client_ctx,
				&venc_recon);
		if (!result) {
			ERR("VEN_IOCTL_FREE_RECON_BUFFER failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_GET_RECON_BUFFER_SIZE:
	{
		struct venc_recon_buff_size venc_recon_size;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_GET_RECON_BUFFER_SIZE\n");
		if (copy_from_user(&venc_recon_size, venc_msg.out,
						   sizeof(venc_recon_size)))
				return -EFAULT;
		result = vid_enc_get_recon_buffer_size(client_ctx,
					&venc_recon_size);
		if (result) {
				if (copy_to_user(venc_msg.out, &venc_recon_size,
					sizeof(venc_recon_size)))
					return -EFAULT;
			} else {
				ERR("setting VEN_IOCTL_GET_RECON_BUFFER_SIZE"
					"failed\n");
				return -EIO;
			}
		break;
	}
	case VEN_IOCTL_SET_QP_RANGE:
	case VEN_IOCTL_GET_QP_RANGE:
	{
		struct venc_qprange qprange;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_G(S)ET_QP_RANGE\n");
		if (cmd == VEN_IOCTL_SET_QP_RANGE) {
			if (copy_from_user(&qprange, venc_msg.in,
				sizeof(qprange)))
				return -EFAULT;
			result = vid_enc_set_get_qprange(client_ctx,
					&qprange, true);
		} else {
			result = vid_enc_set_get_qprange(client_ctx,
					&qprange, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &qprange,
					sizeof(qprange)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_G(S)ET_QP_RANGE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_HEC:
	case VEN_IOCTL_GET_HEC:
	{
		struct venc_headerextension headerextension;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_HEC\n");
		if (cmd == VEN_IOCTL_SET_HEC) {
			if (copy_from_user(&headerextension, venc_msg.in,
				sizeof(headerextension)))
				return -EFAULT;

			result = vid_enc_set_get_headerextension(client_ctx,
					&headerextension, true);
		} else {
			result = vid_enc_set_get_headerextension(client_ctx,
					&headerextension, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &headerextension,
				sizeof(headerextension)))
					return -EFAULT;
			}
		}

		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_HEC failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_TARGET_BITRATE:
	case VEN_IOCTL_GET_TARGET_BITRATE:
	{
		struct venc_targetbitrate targetbitrate;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_TARGET_BITRATE\n");
		if (cmd == VEN_IOCTL_SET_TARGET_BITRATE) {
			if (copy_from_user(&targetbitrate, venc_msg.in,
				sizeof(targetbitrate)))
				return -EFAULT;

			result = vid_enc_set_get_bitrate(client_ctx,
					&targetbitrate, true);
		} else {
			result = vid_enc_set_get_bitrate(client_ctx,
					&targetbitrate, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &targetbitrate,
					sizeof(targetbitrate)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_TARGET_BITRATE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_FRAME_RATE:
	case VEN_IOCTL_GET_FRAME_RATE:
	{
		struct venc_framerate framerate;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_FRAME_RATE\n");
		if (cmd == VEN_IOCTL_SET_FRAME_RATE) {
			if (copy_from_user(&framerate, venc_msg.in,
				sizeof(framerate)))
				return -EFAULT;
			result = vid_enc_set_get_framerate(client_ctx,
					&framerate, true);
		} else {
			result = vid_enc_set_get_framerate(client_ctx,
					&framerate,	false);
			if (result) {
				if (copy_to_user(venc_msg.out, &framerate,
					sizeof(framerate)))
					return -EFAULT;
			}
		}

		if (!result) {
			ERR("VEN_IOCTL_(G)SET_FRAME_RATE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_VOP_TIMING_CFG:
	case VEN_IOCTL_GET_VOP_TIMING_CFG:
	{
		struct venc_voptimingcfg voptimingcfg;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_(G)SET_VOP_TIMING_CFG\n");
		if (cmd == VEN_IOCTL_SET_VOP_TIMING_CFG) {
			if (copy_from_user(&voptimingcfg, venc_msg.in,
				sizeof(voptimingcfg)))
				return -EFAULT;
			result = vid_enc_set_get_voptimingcfg(client_ctx,
					&voptimingcfg, true);
		} else {
			result = vid_enc_set_get_voptimingcfg(client_ctx,
					&voptimingcfg, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &voptimingcfg,
					sizeof(voptimingcfg)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("VEN_IOCTL_(G)SET_VOP_TIMING_CFG failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_RATE_CTRL_CFG:
	case VEN_IOCTL_GET_RATE_CTRL_CFG:
	{
		struct venc_ratectrlcfg ratectrlcfg;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_RATE_CTRL_CFG\n");
		if (cmd == VEN_IOCTL_SET_RATE_CTRL_CFG) {
			if (copy_from_user(&ratectrlcfg, venc_msg.in,
				sizeof(ratectrlcfg)))
				return -EFAULT;

			result = vid_enc_set_get_ratectrlcfg(client_ctx,
					&ratectrlcfg, true);
		} else {
			result = vid_enc_set_get_ratectrlcfg(client_ctx,
					&ratectrlcfg, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &ratectrlcfg,
					sizeof(ratectrlcfg)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_RATE_CTRL_CFG failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_MULTI_SLICE_CFG:
	case VEN_IOCTL_GET_MULTI_SLICE_CFG:
	{
		struct venc_multiclicecfg multiclicecfg;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_MULTI_SLICE_CFG\n");
		if (cmd == VEN_IOCTL_SET_MULTI_SLICE_CFG) {
			if (copy_from_user(&multiclicecfg, venc_msg.in,
				sizeof(multiclicecfg)))
				return -EFAULT;

			result = vid_enc_set_get_multiclicecfg(client_ctx,
					&multiclicecfg, true);
		} else {
			result = vid_enc_set_get_multiclicecfg(client_ctx,
					&multiclicecfg, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &multiclicecfg,
					sizeof(multiclicecfg)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("VEN_IOCTL_(G)SET_MULTI_SLICE_CFG failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_INTRA_REFRESH:
	case VEN_IOCTL_GET_INTRA_REFRESH:
	{
		struct venc_intrarefresh intrarefresh;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_INTRA_REFRESH\n");
		if (cmd == VEN_IOCTL_SET_INTRA_REFRESH) {
			if (copy_from_user(&intrarefresh, venc_msg.in,
				sizeof(intrarefresh)))
				return -EFAULT;
			result = vid_enc_set_get_intrarefresh(client_ctx,
					&intrarefresh, true);
		} else {
			result = vid_enc_set_get_intrarefresh(client_ctx,
					&intrarefresh, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &intrarefresh,
					sizeof(intrarefresh)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_SET_INTRA_REFRESH failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_DEBLOCKING_CFG:
	case VEN_IOCTL_GET_DEBLOCKING_CFG:
	{
		struct venc_dbcfg dbcfg;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_(G)SET_DEBLOCKING_CFG\n");
		if (cmd == VEN_IOCTL_SET_DEBLOCKING_CFG) {
			if (copy_from_user(&dbcfg, venc_msg.in,
				sizeof(dbcfg)))
				return -EFAULT;
			result = vid_enc_set_get_dbcfg(client_ctx,
					&dbcfg, true);
		} else {
			result = vid_enc_set_get_dbcfg(client_ctx,
					&dbcfg, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &dbcfg,
				sizeof(dbcfg)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_SET_DEBLOCKING_CFG failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_ENTROPY_CFG:
	case VEN_IOCTL_GET_ENTROPY_CFG:
	{
		struct venc_entropycfg entropy_cfg;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_ENTROPY_CFG\n");
		if (cmd == VEN_IOCTL_SET_ENTROPY_CFG) {
			if (copy_from_user(&entropy_cfg, venc_msg.in,
				sizeof(entropy_cfg)))
				return -EFAULT;
			result = vid_enc_set_get_entropy_cfg(client_ctx,
					&entropy_cfg, true);
		} else {
			result = vid_enc_set_get_entropy_cfg(client_ctx,
					&entropy_cfg, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &entropy_cfg,
				sizeof(entropy_cfg)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_ENTROPY_CFG failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_GET_SEQUENCE_HDR:
	{
		struct venc_seqheader seq_header, seq_header_user;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_GET_SEQUENCE_HDR\n");
		if (copy_from_user(&seq_header_user, venc_msg.in,
			sizeof(seq_header_user)))
			return -EFAULT;
		seq_header.hdrbufptr = NULL;
		result = vid_enc_get_sequence_header(client_ctx,
				&seq_header);
		if (result && ((copy_to_user(seq_header_user.hdrbufptr,
			seq_header.hdrbufptr, seq_header.hdrlen)) ||
			(copy_to_user(&seq_header_user.hdrlen,
			&seq_header.hdrlen,
			sizeof(seq_header.hdrlen)))))
				result = false;
		kfree(seq_header.hdrbufptr);
		if (!result)
			return -EIO;
		break;
	}
	case VEN_IOCTL_CMD_REQUEST_IFRAME:
	{
		result = vid_enc_request_iframe(client_ctx);
		if (!result) {
			ERR("setting VEN_IOCTL_CMD_REQUEST_IFRAME failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_INTRA_PERIOD:
	case VEN_IOCTL_GET_INTRA_PERIOD:
	{
		struct venc_intraperiod intraperiod;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_INTRA_PERIOD\n");
		if (cmd == VEN_IOCTL_SET_INTRA_PERIOD) {
			if (copy_from_user(&intraperiod, venc_msg.in,
				sizeof(intraperiod)))
				return -EFAULT;
			result = vid_enc_set_get_intraperiod(client_ctx,
					&intraperiod, true);
		} else {
			result = vid_enc_set_get_intraperiod(client_ctx,
					&intraperiod, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &intraperiod,
					sizeof(intraperiod)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_INTRA_PERIOD failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_SESSION_QP:
	case VEN_IOCTL_GET_SESSION_QP:
	{
		struct venc_sessionqp session_qp;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("VEN_IOCTL_(G)SET_SESSION_QP\n");
		if (cmd == VEN_IOCTL_SET_SESSION_QP) {
			if (copy_from_user(&session_qp,	venc_msg.in,
				sizeof(session_qp)))
				return -EFAULT;
			result = vid_enc_set_get_session_qp(client_ctx,
					&session_qp, true);
		} else {
			result = vid_enc_set_get_session_qp(client_ctx,
					&session_qp, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &session_qp,
					sizeof(session_qp)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_SESSION_QP failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_PROFILE_LEVEL:
	case VEN_IOCTL_GET_PROFILE_LEVEL:
	{
		struct ven_profilelevel profile_level;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_(G)SET_PROFILE_LEVEL\n");
		if (cmd == VEN_IOCTL_SET_PROFILE_LEVEL) {
			if (copy_from_user(&profile_level, venc_msg.in,
				sizeof(profile_level)))
				return -EFAULT;
			result = vid_enc_set_get_profile_level(client_ctx,
					&profile_level, true);
		} else {
			result = vid_enc_set_get_profile_level(client_ctx,
					&profile_level, false);
			if (result) {
				if (copy_to_user(venc_msg.out,
				&profile_level,	sizeof(profile_level)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_SET_PROFILE_LEVEL failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_CODEC_PROFILE:
	case VEN_IOCTL_GET_CODEC_PROFILE:
	{
		struct venc_profile profile;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("VEN_IOCTL_(G)SET_CODEC_PROFILE\n");
		if (cmd == VEN_IOCTL_SET_CODEC_PROFILE) {
			if (copy_from_user(&profile, venc_msg.in,
					sizeof(profile)))
				return -EFAULT;
			result = vid_enc_set_get_profile(client_ctx,
					&profile, true);
		} else {
			result = vid_enc_set_get_profile(client_ctx,
					&profile, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &profile,
						sizeof(profile)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_SET_CODEC_PROFILE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_SHORT_HDR:
	case VEN_IOCTL_GET_SHORT_HDR:
	{
		struct venc_switch encoder_switch;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		DBG("Getting VEN_IOCTL_(G)SET_SHORT_HDR\n");
		if (cmd == VEN_IOCTL_SET_SHORT_HDR) {
			if (copy_from_user(&encoder_switch,	venc_msg.in,
				sizeof(encoder_switch)))
				return -EFAULT;

			result = vid_enc_set_get_short_header(client_ctx,
					&encoder_switch, true);
		} else {
			result = vid_enc_set_get_short_header(client_ctx,
					&encoder_switch, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &encoder_switch,
					sizeof(encoder_switch)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_SHORT_HDR failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_BASE_CFG:
	case VEN_IOCTL_GET_BASE_CFG:
	{
		struct venc_basecfg base_config;
		DBG("VEN_IOCTL_SET_BASE_CFG\n");
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		if (cmd == VEN_IOCTL_SET_BASE_CFG) {
			if (copy_from_user(&base_config, venc_msg.in,
				sizeof(base_config)))
				return -EFAULT;
			result = vid_enc_set_get_base_cfg(client_ctx,
					&base_config, true);
		} else {
			result = vid_enc_set_get_base_cfg(client_ctx,
					&base_config, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &base_config,
					sizeof(base_config)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_SET_BASE_CFG failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_LIVE_MODE:
	case VEN_IOCTL_GET_LIVE_MODE:
	{
		struct venc_switch encoder_switch;
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;

		DBG("Getting VEN_IOCTL_(G)SET_LIVE_MODE\n");
		if (cmd == VEN_IOCTL_SET_LIVE_MODE) {
			if (copy_from_user(&encoder_switch,	venc_msg.in,
				sizeof(encoder_switch)))
				return -EFAULT;
			result = vid_enc_set_get_live_mode(client_ctx,
					&encoder_switch, true);
		} else {
			result = vid_enc_set_get_live_mode(client_ctx,
					&encoder_switch, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &encoder_switch,
					sizeof(encoder_switch)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_LIVE_MODE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_GET_NUMBER_INSTANCES:
	{
		DBG("VEN_IOCTL_GET_NUMBER_INSTANCES\n");
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		if (copy_to_user(venc_msg.out,
			&vid_enc_device_p->num_clients, sizeof(u32)))
			return -EFAULT;
		break;
	}
	case VEN_IOCTL_SET_METABUFFER_MODE:
	{
		u32 metabuffer_mode, vcd_status;
		struct vcd_property_hdr vcd_property_hdr;
		struct vcd_property_live live_mode;

		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		if (copy_from_user(&metabuffer_mode, venc_msg.in,
			sizeof(metabuffer_mode)))
			return -EFAULT;
		vcd_property_hdr.prop_id = VCD_I_META_BUFFER_MODE;
		vcd_property_hdr.sz =
			sizeof(struct vcd_property_live);
		live_mode.live = metabuffer_mode;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &live_mode);
		if (vcd_status) {
			pr_err(" Setting metabuffer mode failed");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_EXTRADATA:
	case VEN_IOCTL_GET_EXTRADATA:
	{
		u32 extradata_flag;
		DBG("VEN_IOCTL_(G)SET_EXTRADATA\n");
		if (copy_from_user(&venc_msg, arg, sizeof(venc_msg)))
			return -EFAULT;
		if (cmd == VEN_IOCTL_SET_EXTRADATA) {
			if (copy_from_user(&extradata_flag, venc_msg.in,
					sizeof(u32)))
				return -EFAULT;
			result = vid_enc_set_get_extradata(client_ctx,
					&extradata_flag, true);
		} else {
			result = vid_enc_set_get_extradata(client_ctx,
					&extradata_flag, false);
			if (result) {
				if (copy_to_user(venc_msg.out, &extradata_flag,
						sizeof(u32)))
					return -EFAULT;
			}
		}
		if (!result) {
			ERR("setting VEN_IOCTL_(G)SET_LIVE_MODE failed\n");
			return -EIO;
		}
		break;
	}
	case VEN_IOCTL_SET_AC_PREDICTION:
	case VEN_IOCTL_GET_AC_PREDICTION:
	case VEN_IOCTL_SET_RVLC:
	case VEN_IOCTL_GET_RVLC:
	case VEN_IOCTL_SET_ROTATION:
	case VEN_IOCTL_GET_ROTATION:
	case VEN_IOCTL_SET_DATA_PARTITION:
	case VEN_IOCTL_GET_DATA_PARTITION:
	case VEN_IOCTL_GET_CAPABILITY:
	default:
		ERR("%s(): Unsupported ioctl %d\n", __func__, cmd);
		return -ENOTTY;

		break;
	}
	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Video encoder driver");
MODULE_VERSION("1.0");

module_init(vid_enc_init);
module_exit(vid_enc_exit);
