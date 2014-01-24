/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/module.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include "vpu_v4l2.h"
#include "vpu_ioctl_internal.h"
#include "vpu_configuration.h"
#include "vpu_channel.h"
#include "vpu_debug.h"

/*
 * V4l2 interface *
 */
static int v4l2_vpu_open(struct file *file)
{
	return vpu_open_user_client(file);
}

static int v4l2_vpu_close(struct file *file)
{
	struct vpu_client *client = get_vpu_client(file->private_data);

	return vpu_close_client(client);
}

static unsigned int __poll_vb2_queue(struct vpu_client *client, int port)
{
	struct vpu_dev_session *session = client->session;
	struct vb2_queue *q = &session->vbqueue[port];
	struct vb2_buffer *vb = NULL;
	unsigned int res = 0;
	unsigned long flags;

	spin_lock_irqsave(&q->done_lock, flags);
	if (!list_empty(&q->done_list))
		vb = list_first_entry(&q->done_list, struct vb2_buffer,
				done_entry);
	spin_unlock_irqrestore(&q->done_lock, flags);

	if (vb && (vb->state == VB2_BUF_STATE_DONE
			|| vb->state == VB2_BUF_STATE_ERROR))
		res |= (V4L2_TYPE_IS_OUTPUT(q->type)) ?
				(POLLOUT | POLLWRNORM) : (POLLIN | POLLRDNORM);

	return res;
}

static unsigned int v4l2_vpu_poll(struct file *file,
				struct poll_table_struct *wait)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	struct vpu_dev_session *session = client->session;
	unsigned long req_events = poll_requested_events(wait);
	unsigned int mask = 0;

	pr_debug("Enter function\n");

	/* poll for non buffer events */
	if (req_events & POLLPRI) {
		poll_wait(file, &(client->vfh.wait), wait);

		if (v4l2_event_pending(&client->vfh)) {
			pr_debug("event ready\n");
			mask |= POLLPRI;
		}
	}

	/* check if buffer events should be polled. Client must be attached */
	if (!(req_events & (POLLOUT | POLLWRNORM | POLLIN | POLLRDNORM)))
		return mask;
	else if (!session)
		return mask | POLLERR;

	/* poll input queue */
	if (req_events & (POLLOUT | POLLWRNORM)) {
		poll_wait(file, &session->vbqueue[INPUT_PORT].done_wq, wait);
		mask |= __poll_vb2_queue(client, INPUT_PORT);
	}

	/* poll output queue */
	if (req_events & (POLLIN | POLLRDNORM)) {
		poll_wait(file, &session->vbqueue[OUTPUT_PORT].done_wq, wait);
		mask |= __poll_vb2_queue(client, OUTPUT_PORT);
	}

	return mask;
}

static const struct v4l2_file_operations v4l2_vpu_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_vpu_open,
	.release = v4l2_vpu_close,
	.unlocked_ioctl = video_ioctl2,
	.poll = v4l2_vpu_poll,
};


/*
 * V4l2 IOCTLs
 */
static long v4l2_vpu_private_ioctls(struct file *file, void *fh,
		bool valid_prio, unsigned int cmd, void *arg)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	int session_num = 0, ret = 0;

	switch (cmd) {
	case VPU_QUERY_SESSIONS:
		pr_debug("Received ioctl VPU_QUERY_SESSIONS\n");
		ret = get_vpu_num_sessions((unsigned *)arg);
		break;

	case VPU_ATTACH_TO_SESSION:
		pr_debug("Received ioctl VPU_ATTACH_TO_SESSION\n");
		session_num = *((unsigned *)arg);
		ret = vpu_attach_client(client, session_num);
		break;

	case VPU_COMMIT_CONFIGURATION:
		pr_debug("Received ioctl VPU_COMMIT_CONFIGURATION\n");
		ret = vpu_commit_configuration(client);
		break;

	case VPU_FLUSH_BUFS:
		pr_debug("Received ioctl VPU_FLUSH_BUFS\n");
		ret = vpu_flush_bufs(client, *((enum v4l2_buf_type *) arg));
		break;

	case VPU_G_CONTROL:
		pr_debug("Received ioctl VPU_G_CONTROL\n");
		ret = vpu_get_control(client, (struct vpu_control *) arg);
		break;

	case VPU_S_CONTROL:
		pr_debug("Received ioctl VPU_S_CONTROL\n");
		ret = vpu_set_control(client, (struct vpu_control *) arg);
		break;

	case VPU_G_CONTROL_EXTENDED:
		pr_debug("Received ioctl VPU_G_CONTROL_EXTENDED\n");
		ret = vpu_get_control_extended(client,
				(struct vpu_control_extended *) arg);
		break;

	case VPU_S_CONTROL_EXTENDED:
		pr_debug("Received ioctl VPU_S_CONTROL_EXTENDED\n");
		ret = vpu_set_control_extended(client,
				(struct vpu_control_extended *) arg);
		break;

	case VPU_G_CONTROL_PORT:
		pr_debug("Received ioctl VPU_G_CONTROL_PORT\n");
		ret = vpu_get_control_port(client,
				(struct vpu_control_port *) arg);
		break;

	case VPU_S_CONTROL_PORT:
		pr_debug("Received ioctl VPU_S_CONTROL_PORT\n");
		ret = vpu_set_control_port(client,
				(struct vpu_control_port *) arg);
		break;

	default:
		pr_warn("Received unknown ioctl (%d)\n", cmd);
		return -ENOTTY;
		break;
	}
	return (long) ret;
}

static int v4l2_vpu_querycap(struct file *file, void *fh,
		struct v4l2_capability *cap)
{
	struct vpu_dev_core *core = video_drvdata(file);

	strlcpy(cap->driver, VPU_DRV_NAME, sizeof(cap->driver));
	snprintf(cap->card, 32, "%s0", VPU_DRV_NAME);
	strlcpy(cap->bus_info, core->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = KERNEL_VERSION(0, 0, 1);
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE |
		 V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;

	return 0;
}

static int v4l2_vpu_enum_fmt(struct file *file,
					void *fh, struct v4l2_fmtdesc *f)
{
	return vpu_enum_fmt(f);
}

static int v4l2_vpu_g_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct vpu_client *client = get_vpu_client(file->private_data);

	return vpu_get_fmt(client, f);
}

static int v4l2_vpu_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_S_FMT on port %d\n",
			get_port_number(f->type));

	return vpu_set_fmt(client, f);
}

static int v4l2_vpu_try_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_TRY_FMT on port %d\n",
				get_port_number(f->type));

	return vpu_try_fmt(client, f);
}

static int v4l2_vpu_g_crop(struct file *file, void *fh, struct v4l2_crop *c)
{
	struct vpu_client *client = get_vpu_client(file->private_data);

	return vpu_get_region_of_intereset(client, c);
}

static int v4l2_vpu_s_crop(struct file *file, void *fh,
			const struct v4l2_crop *c)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_S_CROP on port %d\n",
			get_port_number(c->type));

	return vpu_set_region_of_intereset(client, c);
}

static int v4l2_vpu_g_input(struct file *file, void *fh, unsigned int *i)
{
	struct vpu_client *client = get_vpu_client(file->private_data);

	return vpu_get_input(client, i);
}
static int v4l2_vpu_s_input(struct file *file, void *fh, unsigned int i)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_S_INPUT\n");

	return vpu_set_input(client, i);
}

static int v4l2_vpu_g_output(struct file *file, void *fh, unsigned int *i)
{
	struct vpu_client *client = get_vpu_client(file->private_data);

	return vpu_get_output(client, i);
}
static int v4l2_vpu_s_output(struct file *file, void *fh, unsigned int i)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_S_OUTPUT\n");

	return vpu_set_output(client, i);
}

static int v4l2_vpu_reqbufs(struct file *file, void *fh,
			struct v4l2_requestbuffers *rb)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	struct vpu_dev_session *session;
	struct vpu_port_ops *port_ops;
	int port;

	session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	port = get_port_number(rb->type);
	if (port < 0) {
		pr_err("Invalid type %d\n", rb->type);
		return -EINVAL;
	}

	pr_debug("Received ioctl VIDIOC_REQBUFS on port %d\n",
			get_port_number(rb->type));

	port_ops = &client->session->port_info[port].port_ops;
	if (port_ops->set_buf_num)
		return port_ops->set_buf_num(session, port,
				rb->count, port_ops->priv);
	else
		return vpu_reqbufs(client, rb);
}

static int v4l2_vpu_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	struct vpu_dev_session *session;
	struct vpu_port_ops *port_ops;
	int port;

	session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	port = get_port_number(b->type);
	if (port < 0) {
		pr_err("Invalid type %d\n", b->type);
		return -EINVAL;
	}

	pr_debug("Received ioctl VIDIOC_QBUF on port %d\n",
			get_port_number(b->type));

	port_ops = &client->session->port_info[port].port_ops;
	if (port_ops->set_buf)
		return port_ops->set_buf(session, port, b, port_ops->priv);
	else
		return vpu_qbuf(client, b);
}

static int v4l2_vpu_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_DQBUF on port %d\n",
			get_port_number(b->type));

	return vpu_dqbuf(client, b, file->f_flags & O_NONBLOCK);
}

static int v4l2_vpu_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_STREAMON on port %d\n",
			get_port_number(i));

	return vpu_streamon(client, i);
}

static int v4l2_vpu_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct vpu_client *client = get_vpu_client(file->private_data);
	pr_debug("Received ioctl VIDIOC_STREAMOFF on port %d\n",
			get_port_number(i));

	return vpu_streamoff(client, i);
}

static int v4l2_vpu_subscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	struct v4l2_event_subscription each_sub;
	int ret = 0;

	pr_debug("Received ioctl VIDIOC_SUBSCRIBE_EVENT, type %d\n", sub->type);
	memset(&each_sub, 0, sizeof(each_sub));

	if (sub->type == V4L2_EVENT_ALL) {

		each_sub.type = VPU_EVENT_START + 1;
		do {
			ret = v4l2_event_subscribe(fh, &each_sub,
					VPU_EVENT_Q_SIZE, NULL);
			if (ret < 0) {
				v4l2_event_unsubscribe_all(fh);
				break;
			}
			each_sub.type++;
		} while (each_sub.type < VPU_EVENT_END);

	} else if (sub->type <= VPU_EVENT_START || sub->type >= VPU_EVENT_END) {
		ret = -EINVAL;
	} else {
		each_sub.type = sub->type;
		ret = v4l2_event_subscribe(fh, &each_sub,
				VPU_EVENT_Q_SIZE, NULL);
	}

	if (ret)
		pr_err("event %d subscribe failed (ret %d)\n", sub->type, ret);

	return ret;
}

static const struct v4l2_ioctl_ops v4l2_vpu_ioctl_ops = {
	.vidioc_querycap = v4l2_vpu_querycap,

	/* video format setting */
	.vidioc_enum_fmt_vid_cap_mplane = v4l2_vpu_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = v4l2_vpu_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = v4l2_vpu_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = v4l2_vpu_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = v4l2_vpu_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = v4l2_vpu_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = v4l2_vpu_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = v4l2_vpu_try_fmt,

	/* Cropping ioctls (region of interest) */
	.vidioc_cropcap = NULL,
	.vidioc_g_crop = v4l2_vpu_g_crop,
	.vidioc_s_crop = v4l2_vpu_s_crop,

	/* ioctls to set/get session source and destination */
	.vidioc_g_input = v4l2_vpu_g_input,
	.vidioc_s_input = v4l2_vpu_s_input,
	.vidioc_g_output = v4l2_vpu_g_output,
	.vidioc_s_output = v4l2_vpu_s_output,

	/* streaming I/O ioctls */
	.vidioc_reqbufs = v4l2_vpu_reqbufs,
	.vidioc_prepare_buf = NULL,
	.vidioc_qbuf = v4l2_vpu_qbuf,
	.vidioc_dqbuf = v4l2_vpu_dqbuf,
	.vidioc_streamon = v4l2_vpu_streamon,
	.vidioc_streamoff = v4l2_vpu_streamoff,

	/* Subscribe/unsubscribe to events (use VIDIOC_DQEVENT to get events) */
	.vidioc_subscribe_event = v4l2_vpu_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe, /* generic unsub */

	/* Custom ioctls */
	.vidioc_default = v4l2_vpu_private_ioctls,
};


/*
 * Module init / Probe *
 */
static void vpu_video_device_release(struct video_device *vdev) {}

static void deinit_vpu_sessions(struct vpu_dev_core *core)
{
	int i;
	struct vpu_dev_session *session = 0;
	struct vpu_client *client, *n;

	for (i = 0; i < VPU_NUM_SESSIONS; i++) {
		session = core->sessions[i];
		if (!session)
			break;

		/* free all attached clients */
		list_for_each_entry_safe(client, n,
				&session->clients_list, clients_entry) {
			vpu_close_client(client);
		}

		devm_kfree(core->dev, session);
		core->sessions[i] = 0;
	}

	/* free any other unattached clients */
	list_for_each_entry_safe(client, n,
				&core->unattached_list, clients_entry) {
		vpu_close_client(client);
	}
}

static int init_vpu_sessions(struct vpu_dev_core *core)
{
	int ret = 0, i;
	struct vpu_dev_session *session = 0;

	for (i = 0; i < VPU_NUM_SESSIONS; i++) {
		session = devm_kzalloc(core->dev, sizeof(*session), GFP_KERNEL);
		if (!session) {
			ret = -ENOMEM;
			goto err_enomem;
		}
		session->id = i;
		session->core = core;

		mutex_init(&session->lock);
		INIT_LIST_HEAD(&session->clients_list);

		mutex_init(&session->que_lock[INPUT_PORT]);
		mutex_init(&session->que_lock[OUTPUT_PORT]);
		INIT_LIST_HEAD(&session->pending_list[INPUT_PORT]);
		INIT_LIST_HEAD(&session->pending_list[OUTPUT_PORT]);

		ret = vpu_vb2_queue_init(&session->vbqueue[INPUT_PORT],
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, session);
		ret |= vpu_vb2_queue_init(&session->vbqueue[OUTPUT_PORT],
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, session);
		if (ret)
			goto err_free_mem;

		core->sessions[i] = session;
	}

	return ret;

err_free_mem:
	devm_kfree(core->dev, session);
err_enomem:
	deinit_vpu_sessions(core); /* free successfully loaded sessions */
	pr_err("init_vpu_sessions failed\n");

	return ret;
}

static int vpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct vpu_dev_core *core = 0;
	pr_debug("Enter function\n");

	/* Allocate global VPU core struct */
	core = devm_kzalloc(&pdev->dev, sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	/* Initialize core's data */
	INIT_LIST_HEAD(&core->unattached_list);
	mutex_init(&core->lock);
	core->dev = &pdev->dev;
	platform_set_drvdata(pdev, core);

	/* read/get platform resources from Device Tree */
	ret = read_vpu_platform_resources(&core->resources, pdev);
	if (ret) {
		pr_err("Failed to read platform resources\n");
		goto err_free_core;
	}

	ret = register_vpu_iommu_domains(&core->resources);
	if (ret) {
		pr_err("Failed to register iommu domains\n");
		goto err_free_platform_resources;
	}

	/* init IPC system */
	ret = vpu_hw_sys_init(&core->resources);
	if (ret) {
		pr_err("VPU system init failed\n");
		goto err_unregister_iommu_domains;
	}

	/* Initialize VPU sessions */
	ret = init_vpu_sessions(core);
	if (ret)
		goto err_deinit_vpu_system;

	setup_vpu_controls();

	/* register VPU v4l2 device */
	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret)
		goto err_deinit_sessions;

	/* register video_device */
	strlcpy(core->vdev.name, VPU_DRV_NAME, sizeof(core->vdev.name));
	core->vdev.v4l2_dev = &core->v4l2_dev;
	core->vdev.fops = &v4l2_vpu_fops;
	core->vdev.ioctl_ops = &v4l2_vpu_ioctl_ops;
	core->vdev.release = vpu_video_device_release;
	/* mem2mem device: can transmit using the same video device. */
	core->vdev.vfl_dir = VFL_DIR_M2M;
	video_set_drvdata(&core->vdev, core);
	ret = video_register_device(&core->vdev, VFL_TYPE_GRABBER, -1);
	if (ret) {
		pr_err("Failed to register video_device\n");
		goto err_unregister_v4l2_device;
	}

	/* initialize debugfs */
	core->debugfs_root = init_vpu_debugfs(core);

	pr_info("Probe Successful\n");
	return 0;

err_unregister_v4l2_device:
	v4l2_device_unregister(&core->v4l2_dev);
err_deinit_sessions:
	deinit_vpu_sessions(core);
err_deinit_vpu_system:
	vpu_hw_sys_cleanup();
err_unregister_iommu_domains:
	unregister_vpu_iommu_domains(&core->resources);
err_free_platform_resources:
	free_vpu_platform_resources(&core->resources);
err_free_core:
	devm_kfree(&pdev->dev, core);
	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct vpu_dev_core *core = platform_get_drvdata(pdev);
	pr_debug("Enter function\n");

	cleanup_vpu_debugfs(core->debugfs_root);
	video_unregister_device(&core->vdev);
	v4l2_device_unregister(&core->v4l2_dev);
	deinit_vpu_sessions(core);
	vpu_hw_sys_cleanup();
	unregister_vpu_iommu_domains(&core->resources);
	free_vpu_platform_resources(&core->resources);
	devm_kfree(&pdev->dev, core);

	return 0;
}


static const struct of_device_id vpu_dt_match[] = {
	{.compatible = "qcom,vpu"},
	{}
};

static struct platform_driver vpu_platform_driver = {
	.driver   = {
		.name = VPU_DRV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = vpu_dt_match,
	},
	.probe    = vpu_probe,
	.remove   = vpu_remove,
};

static int __init vpu_init_module(void)
{
	pr_debug("Enter function\n");
	return platform_driver_register(&vpu_platform_driver);
}

static void __exit vpu_exit_module(void)
{
	pr_debug("Enter function\n");
	platform_driver_unregister(&vpu_platform_driver);
}

module_init(vpu_init_module);
module_exit(vpu_exit_module);
MODULE_DESCRIPTION("MSM VPU driver");
MODULE_LICENSE("GPL v2");

