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

#include <media/v4l2-event.h>

#include "vpu_ioctl_internal.h"
#include "vpu_configuration.h"
#include "vpu_translate.h"
#include "vpu_channel.h"
#include "vpu_ipc.h"

#ifdef CONFIG_MSM_VPU_IN_VCAP
extern int vpu_init_port_vcap(struct vpu_dev_session *session,
			struct vpu_port_info *port_info);
#else
#define vpu_init_port_vcap(session, port_info)		0
#endif

#ifdef CONFIG_MSM_VPU_OUT_MDSS
extern int vpu_init_port_mdss(struct vpu_dev_session *session,
			struct vpu_port_info *port_info);
#else
#define vpu_init_port_mdss(session, port_info)		0
#endif

/* tunneling port operation call */
#define call_port_op(session, port, op) ( \
		    (session)->port_info[port].port_ops.op ? \
		    (session)->port_info[port].port_ops.op(session, port, \
		    (session)->port_info[port].port_ops.priv) : 0)
/*
 * Events/Callbacks handling
 */
static void __prepare_v4l2_event(struct v4l2_event *event,
		u32 type, u8 *data, u32 size)
{
	event->type = type;
	if (data && size > sizeof(event->u.data)) {
		pr_debug("Payload size (%d) is too large\n", size);
		size = sizeof(event->u.data);
	}
	if (data)
		memcpy(event->u.data, data, size);
}

void notify_vpu_event_client(struct vpu_client *client,
		u32 type, u8 *data, u32 size)
{
	struct v4l2_event event = {0};
	if (!client)
		return;

	__prepare_v4l2_event(&event, type, data, size);
	v4l2_event_queue_fh(&client->vfh, &event);
}

void notify_vpu_event_session(struct vpu_dev_session *session,
		u32 type, u8 *data, u32 size)
{
	struct v4l2_event event = {0};
	struct vpu_client *clnt, *n;
	if (!session)
		return;

	__prepare_v4l2_event(&event, type, data, size);
	list_for_each_entry_safe(clnt, n,
				      &session->clients_list, clients_entry)
		v4l2_event_queue_fh(&clnt->vfh, &event);
}

/* __system_callback_handler
 *
 * event:	see enum vpu_chan_event defined in vpu_channel.h
 */
static void __sys_event_callback_handler(u32 sid, u32 event,
		u32 data, void *pcore)
{
	struct vpu_dev_core *core = (struct vpu_dev_core *)pcore;
	int i;

	if (sid < VPU_NUM_SESSIONS) {
		/* session event */
		struct vpu_dev_session *session = core->sessions[sid];

		if (event == VPU_HW_EVENT_IPC_ERROR) {
			pr_debug("error in session %d (%d, %d)!\n",
					session->id, event, data);

			notify_vpu_event_session(session, VPU_EVENT_HW_ERROR,
					(u8 *) &data, sizeof(data));
		}

		if (event == VPU_HW_EVENT_ACTIVE_REGION_CHANGED) {
			struct v4l2_rect roi_result, *cache;
			struct rect *roi_rect = (struct rect *)data;

			translate_roi_rect_to_api(roi_rect, &roi_result);

			cache = get_control(session->controller,
					VPU_CTRL_ACTIVE_REGION_RESULT);
			if (cache)
				memcpy(cache, &roi_result, sizeof(roi_result));

			notify_vpu_event_session(session,
					VPU_EVENT_ACTIVE_REGION_CHANGED,
					(u8 *)&roi_result, sizeof(roi_result));
		}
	} else {
		/* system event */
		if ((event == VPU_HW_EVENT_WATCHDOG_BITE) ||
			(event == VPU_HW_EVENT_IPC_ERROR)) {

			pr_debug("VPU HW error (%d)\n", event);

			/* notify all sessions */
			for (i = 0; i < VPU_NUM_SESSIONS; i++)
				notify_vpu_event_session(core->sessions[i],
						VPU_EVENT_HW_ERROR, NULL, 0);

		}
	}
}

static void __sys_buffer_callback_handler(u32 sid, struct vpu_buffer *pbuf,
				    u32 data, void *client_data)
{
	int port, i;
	struct vpu_dev_session *session;
	struct vpu_port_info *port_info;
	if (!pbuf)
		return;

	if (!pbuf->vb.vb2_queue) {
		pr_err("ERROR: Null pointer\n");
		return;
	}

	port = get_port_number(pbuf->vb.vb2_queue->type);
	session = vb2_get_drv_priv(pbuf->vb.vb2_queue);

	if (data) {
		pr_debug("%s (%d) buffer callback session %d port %d buff %d\n",
			data == VPU_STS_EFRAMEUNPROCESSED ?
					"Unprocessed" : "Error",
			data, session->id, port, pbuf->vb.v4l2_buf.index);

		vb2_buffer_done(&pbuf->vb, VB2_BUF_STATE_ERROR);
	} else {
		pr_debug("Good buffer callback session %d port %d buff %d\n",
			session->id, port, pbuf->vb.v4l2_buf.index);

		/* update bytesused */
		port_info = &session->port_info[port];
		for (i = 0; i < pbuf->vb.num_planes; i++)
			pbuf->vb.v4l2_planes[i].bytesused =
				port_info->format.plane_fmt[i].sizeimage;

		vb2_buffer_done(&pbuf->vb, VB2_BUF_STATE_DONE);
	}
}

/*
 * Session/Client management
 */

/*
 * Dynamic switch (on/off) of VPU hardware on first/last global client.
 * Function must be called with core->lock mutex held.
 */
static int __vpu_hw_switch(struct vpu_dev_core *core, int on)
{
	int ret = 0;

	if (on) {
		if (core->global_client_count++ == 0) {

			pr_debug("Starting up VPU hardware\n");
			ret = vpu_hw_sys_start(__sys_event_callback_handler,
					__sys_buffer_callback_handler,
					(void *)core);
			if (ret) {
				pr_err("failed to start VPU hardware\n");
				core->global_client_count--;
			}
		}

	} else { /* off */
		if (--core->global_client_count == 0) {
			pr_debug("Shutting down VPU hardware\n");
			vpu_hw_sys_stop(0);
		}
	}

	return ret;
}

static void __vpu_streamoff_port(struct vpu_dev_session *session, int port)
{
	/* Stop end-to-end session streaming on first streamoff */
	if (session->streaming_state == ALL_STREAMING) {
		if (vpu_hw_session_stop(session->id))
			pr_err("Session stop failed\n");
		pr_debug("Session streaming stopped\n");
		session->commit_state = 0;
	}
	session->streaming_state &= ~(0x1 << port);
}

static struct vpu_client *__create_client(struct vpu_dev_core *core,
					struct file *file)
{
	struct vpu_client *client;
	int ret = 0;

	mutex_lock(&core->lock);

	if (core->global_client_count >= VPU_MAX_CLIENTS) {
		ret = -EBUSY;
		goto err_client_create;
	}

	/* create client struct */
	client = devm_kzalloc(core->dev, sizeof(*client), GFP_KERNEL);
	if (!client) {
		ret = -ENOMEM;
		goto err_client_create;
	}
	client->core = core;
	client->type = file ? VPU_USERSPACE_CLIENT : VPU_KERNEL_SUBDEV_CLIENT;
	INIT_LIST_HEAD(&client->clients_entry);

	/* Initialize HFI on first client open */
	ret = __vpu_hw_switch(core, 1);
	if (ret) {
		devm_kfree(core->dev, client);
		goto err_client_create;
	}

	list_add_tail(&client->clients_entry, &core->unattached_list);
	mutex_unlock(&core->lock);

	/* initialize v4l2_fh */
	v4l2_fh_init(&client->vfh, &core->vdev);
	if (file)
		file->private_data = &client->vfh;
	v4l2_fh_add(&client->vfh);

	pr_debug("Client creation successful\n");
	return client;

err_client_create:
	mutex_unlock(&core->lock);
	return ERR_PTR(ret);
}

int vpu_open_user_client(struct file *file)
{
	struct vpu_dev_core *core = video_drvdata(file);
	struct vpu_client *client;

	pr_debug("New userspace client\n");

	client = __create_client(core, file);
	if (IS_ERR_OR_NULL(client))
		return PTR_ERR(client);

	return 0;
}

struct vpu_client *vpu_open_kernel_client(struct vpu_dev_core *core)
{
	struct vpu_client *client;
	if (!core)
		return NULL;

	pr_debug("New kernel client\n");

	client = __create_client(core, NULL);
	if (IS_ERR_OR_NULL(client))
		return NULL;

	return client;
}

int vpu_close_client(struct vpu_client *client)
{
	if (!client)
		return 0; /* should never happen */

	pr_debug("Enter function\n");

	if (client->session)
		vpu_detach_client(client);

	mutex_lock(&client->core->lock);
	__vpu_hw_switch(client->core, 0);
	list_del_init(&client->clients_entry); /* remove from unattached list */
	mutex_unlock(&client->core->lock);

	v4l2_fh_del(&client->vfh);
	v4l2_fh_exit(&client->vfh);
	devm_kfree(client->core->dev, client);
	return 0;
}

int vpu_attach_client(struct vpu_client *client, int session_num)
{
	struct vpu_dev_core *core;
	struct vpu_dev_session *session;
	int ret = 0;
	if (!client || session_num < 0 || session_num >= VPU_NUM_SESSIONS) {
		pr_err("invalid session attach # %d\n", session_num);
		return -EINVAL;
	}

	core = client->core;
	session = core->sessions[session_num];

	pr_debug("Attach client %p to session %d\n", client, session_num);

	if (client->session == session) {
		return 0; /* client already attached to this session */
	} else if (client->session) {
		pr_err("Client already attached to a session\n");
		return -EINVAL;
	}

	if (session->client_count >= VPU_MAX_CLIENTS_PER_SESSION)
		return -EBUSY;

	mutex_lock(&session->lock);

	if (session->client_count++ == 0) {

		/* Initialize session controller */
		session->controller =
				init_vpu_controller(&session->core->resources);
		if (!session->controller) {
			pr_err("init_vpu_controller failed\n");
			goto err_dec_count;
		}

		/* Open hw session & IPC channel */
		ret = vpu_hw_session_open(session->id, 0);
		if (ret) {
			pr_err("could not open IPC channel\n");
			goto err_deinit_controller;
		}
	}

	/* remove from unattached_list and add to session's clients list */
	list_del_init(&client->clients_entry);
	list_add_tail(&client->clients_entry, &session->clients_list);
	client->session = session;
	mutex_unlock(&session->lock);

	pr_debug("Attach to session %d successful\n", session_num);
	return 0;

err_deinit_controller:
	deinit_vpu_controller(session->controller);
	session->controller = NULL;
err_dec_count:
	session->client_count--;
	mutex_unlock(&session->lock);
	return ret;
}

void vpu_detach_client(struct vpu_client *client)
{
	struct vpu_dev_session *session;
	int port = 0;
	if (!client || !client->session)
		return;

	session = client->session;
	pr_debug("Enter function\n");

	mutex_lock(&session->lock);
	for (port = 0; port < NUM_VPU_PORTS; ++port) {
		mutex_lock(&session->que_lock[port]);
		/* Stream off and return buffers if client was doing io ops */
		if (client == session->io_client[port]) {
			__vpu_streamoff_port(session, port);
			vb2_queue_release(&session->vbqueue[port]);
			session->io_client[port] = NULL;
		}
		mutex_unlock(&session->que_lock[port]);
	}

	if (--session->client_count == 0) {
		/* close hw session on last detach */
		vpu_hw_session_close(session->id);

		/* detach tunneling ports */
		for (port = 0; port < NUM_VPU_PORTS; ++port)
			call_port_op(session, port, detach);

		/* reset session state and configuration data */
		deinit_vpu_controller(session->controller);
		session->controller = NULL;

		memset(&session->port_info[INPUT_PORT], 0,
				sizeof(session->port_info[INPUT_PORT]));
		memset(&session->port_info[OUTPUT_PORT], 0,
				sizeof(session->port_info[OUTPUT_PORT]));

		session->streaming_state = 0;
		session->commit_state = 0;
	}

	list_del_init(&client->clients_entry); /* remove from attached list */
	client->session = NULL;
	mutex_unlock(&session->lock);

	/* add back to global unattached list */
	list_add_tail(&client->clients_entry, &client->core->unattached_list);
}

/*
 * Format/parameters configuration
 */
int vpu_enum_fmt(struct v4l2_fmtdesc *f)
{
	const struct vpu_format_desc *fmt;
	int port = get_port_number(f->type);
	if (port < 0)
		return -EINVAL;

	fmt = query_supported_formats(f->index);
	if (!fmt)
		return -EINVAL;

	memset(f->reserved, 0 , sizeof(f->reserved));
	strlcpy(f->description, fmt->description, sizeof(f->description));
	f->pixelformat = fmt->fourcc;

	return 0;
}

int vpu_get_fmt(struct vpu_client *client, struct v4l2_format *f)
{
	struct vpu_prop_session_input    in_param;
	struct vpu_prop_session_output   out_param;
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;

	if (!session)
		return -EPERM;

	port = get_port_number(f->type);
	if (port < 0)
		return -EINVAL;

	if (port == INPUT_PORT) {
		ret = vpu_hw_session_g_input_params(session->id, &in_param);
		translate_input_format_to_api(&in_param, f);
		f->fmt.pix_mp.colorspace =
			session->port_info[INPUT_PORT].format.colorspace;

	} else {
		ret = vpu_hw_session_g_output_params(session->id, &out_param);
		translate_output_format_to_api(&out_param, f);
		f->fmt.pix_mp.colorspace =
			session->port_info[OUTPUT_PORT].format.colorspace;
	}

	return ret;
}

int vpu_try_fmt(struct vpu_client *client, struct v4l2_format *f)
{
	const struct vpu_format_desc *vpu_format;
	u32 hfi_pixelformat, bytesperline;
	int i;

	hfi_pixelformat =
		translate_pixelformat_to_hfi(f->fmt.pix_mp.pixelformat);
	vpu_format = query_supported_formats(hfi_pixelformat);
	if (!vpu_format)
		return -EINVAL;

	pr_debug("width = %d, height = %d\n",
			f->fmt.pix_mp.width, f->fmt.pix_mp.height);
	if (!is_format_valid(f))
		return -EINVAL;

	f->fmt.pix_mp.num_planes = vpu_format->num_planes;

	for (i = 0; i < vpu_format->num_planes; i++) {
		bytesperline = get_bytesperline(f->fmt.pix_mp.width,
				vpu_format->plane[i].bitsperpixel,
				f->fmt.pix_mp.plane_fmt[i].bytesperline,
				f->fmt.pix_mp.pixelformat);
		if (!bytesperline) {
			pr_err("Invalid plane %d bytesperline\n", i);
			return -EINVAL;
		}
		f->fmt.pix_mp.plane_fmt[i].bytesperline = bytesperline;

		f->fmt.pix_mp.plane_fmt[i].sizeimage =
			get_sizeimage(bytesperline, f->fmt.pix_mp.height,
				vpu_format->plane[i].heightfactor);
	}

	return 0;
}

int vpu_set_fmt(struct vpu_client *client, struct v4l2_format *f)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0;
	int port;
	struct v4l2_rect def_roi;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;

	if (!session) {
		pr_err("invalid session\n");
		return -EPERM;
	}
	port = get_port_number(f->type);
	if (port < 0) {
		pr_err("invalid port (%d)\n", port);
		return -EINVAL;
	}

	/* check validity of format configuration */
	ret = vpu_try_fmt(client, f);
	if (ret < 0) {
		pr_err("try_fmt failed (err %d)\n", ret);
		return -EINVAL;
	}

	/* by default, ROI is the full frame */
	def_roi.left = 0;
	def_roi.top = 0;
	def_roi.width = pix_mp->width;
	def_roi.height = pix_mp->height;

	mutex_lock(&session->lock);
	memcpy(&session->port_info[port].format, pix_mp, sizeof(*pix_mp));
	memcpy(&session->port_info[port].roi, &def_roi, sizeof(def_roi));
	if (port == INPUT_PORT)
		session->port_info[INPUT_PORT].scan_mode =
				translate_v4l2_scan_mode(pix_mp->field);

	ret = configure_colorspace(session, port);
	if (ret)
		goto exit;

	ret = commit_port_config(session, port, 1);
exit:
	mutex_unlock(&session->lock);
	return ret;
}

static int __vpu_get_framerate(struct vpu_client *client, u32 *framerate,
		int port)
{
	struct vpu_prop_session_input    in_param;
	struct vpu_prop_session_output   out_param;
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0;

	if (port == INPUT_PORT) {
		ret = vpu_hw_session_g_input_params(session->id, &in_param);
		*framerate = in_param.frame_rate;
	} else {
		ret = vpu_hw_session_g_output_params(session->id, &out_param);
		*framerate = out_param.frame_rate;
	}

	return ret;
}

static int __vpu_set_framerate(struct vpu_client *client, u32 framerate,
		int port)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0;

	mutex_lock(&session->lock);
	session->port_info[port].framerate = framerate;
	ret = commit_port_config(session, port, 1);
	mutex_unlock(&session->lock);

	return ret;
}

int vpu_get_region_of_intereset(struct vpu_client *client,
		struct v4l2_crop *crop)
{
	struct vpu_prop_session_input    in_param;
	struct vpu_prop_session_output   out_param;
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;

	if (!session)
		return -EPERM;

	port = get_port_number(crop->type);
	if (port < 0)
		return -EINVAL;

	if (port == INPUT_PORT) {
		ret = vpu_hw_session_g_input_params(session->id, &in_param);
		translate_roi_rect_to_api(&in_param.region_interest,
				&crop->c);
	} else {
		ret = vpu_hw_session_g_output_params(session->id, &out_param);
		translate_roi_rect_to_api(&out_param.dest_rect,
				&crop->c);
	}

	return ret;
}

int vpu_set_region_of_intereset(struct vpu_client *client,
		const struct v4l2_crop *crop)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port = get_port_number(crop->type);

	if (!session)
		return -EPERM;
	if (port < 0)
		return -EINVAL;

	mutex_lock(&session->lock);
	memcpy(&session->port_info[port].roi, &crop->c, sizeof(crop->c));
	ret = commit_port_config(session, port, 0);

	mutex_unlock(&session->lock);

	return ret;
}

int vpu_get_input(struct vpu_client *client, unsigned int *i)
{
	if (!client || !client->session)
		return -EPERM;
	*i = client->session->port_info[INPUT_PORT].source;

	return 0;
}

int vpu_set_input(struct vpu_client *client, unsigned int i)
{
	int ret = 0;
	struct vpu_dev_session *session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	/* Changing input/output only allowed if port is not streaming */
	mutex_lock(&session->lock);
	if (session->streaming_state & (0x1 << INPUT_PORT)) {
		ret = -EBUSY;
		goto exit_s_input;
	}

	session->port_info[INPUT_PORT].source = i;

	/* detach previous input tunnel port, if existing */
	call_port_op(session, INPUT_PORT, detach);

	/* initiate and attach input tunnel port, if needed */
	if (i != VPU_INPUT_TYPE_HOST) {
		ret = vpu_init_port_vcap(session,
					&session->port_info[INPUT_PORT]);
		if (ret)
			goto exit_s_input;

		ret = call_port_op(session, INPUT_PORT, attach);
		if (ret)
			goto exit_s_input;

	}

	ret = commit_port_config(session, INPUT_PORT, 1);

exit_s_input:
	mutex_unlock(&session->lock);
	return ret;
}

int vpu_get_output(struct vpu_client *client, unsigned int *i)
{
	if (!client || !client->session)
		return -EPERM;
	*i = client->session->port_info[OUTPUT_PORT].destination;

	return 0;
}

int vpu_set_output(struct vpu_client *client, unsigned int i)
{
	int ret = 0;
	struct vpu_dev_session *session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	mutex_lock(&session->lock);
	if (session->streaming_state & (0x1 << OUTPUT_PORT)) {
		ret = -EBUSY;
		goto exit_s_output;
	}

	session->port_info[OUTPUT_PORT].destination = i;

	/* detach previous output tunnel port, if existing */
	call_port_op(session, OUTPUT_PORT, detach);

	/* initiate and attach input tunnel port, if needed */
	if (i != VPU_OUTPUT_TYPE_HOST) {
		ret = vpu_init_port_mdss(session,
					&session->port_info[OUTPUT_PORT]);
		if (ret)
			goto exit_s_output;

		ret = call_port_op(session, OUTPUT_PORT, attach);
		if (ret)
			goto exit_s_output;
	}

	ret = commit_port_config(session, OUTPUT_PORT, 1);

exit_s_output:
	mutex_unlock(&session->lock);
	return ret;
}

int vpu_get_control(struct vpu_client *client, struct vpu_control *control)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	return apply_vpu_control(session, 0, control);
}

int vpu_set_control(struct vpu_client *client, struct vpu_control *control)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	return apply_vpu_control(session, 1, control);
}

int vpu_get_control_extended(struct vpu_client *client,
		struct vpu_control_extended *control)
{
	/* Don't check client->session, since ioctl can address system */
	return apply_vpu_control_extended(client, 0, control);
}

int vpu_set_control_extended(struct vpu_client *client,
		struct vpu_control_extended *control)
{
	/* Don't check client->session, since ioctl can address system */
	return apply_vpu_control_extended(client, 1, control);
}

int vpu_get_control_port(struct vpu_client *client,
		struct vpu_control_port *control)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int port = control->port;

	if (!session)
		return -EPERM;
	if (port < 0 || port > NUM_VPU_PORTS)
		return -EINVAL;

	if (control->control_id == VPU_CTRL_FPS)
		return __vpu_get_framerate(client,
				&control->data.framerate, port);
	else
		return -EINVAL;
}

int vpu_set_control_port(struct vpu_client *client,
		struct vpu_control_port *control)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int port = control->port;

	if (!session)
		return -EPERM;
	if (port < 0 || port > NUM_VPU_PORTS)
		return -EINVAL;

	if (control->control_id == VPU_CTRL_FPS)
		return __vpu_set_framerate(client,
				control->data.framerate, port);
	else
		return -EINVAL;
}

int vpu_commit_configuration(struct vpu_client *client)
{
	int ret;
	struct vpu_dev_session *session = client ? client->session : 0;
	if (!session)
		return -EPERM;

	mutex_lock(&session->lock);
	ret = commit_initial_config(session);
	mutex_unlock(&session->lock);

	return ret;
}

/*
 * Streaming I/O operations
 */
static inline int __is_tunneling(struct vpu_dev_session *session,
		int port)
{
	/* Checks if the port is set to tunneling */
	return (session->port_info[port].source != 0);
}

int vpu_reqbufs(struct vpu_client *client, struct v4l2_requestbuffers *req)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;
	if (!session)
		return -EPERM;

	port = get_port_number(req->type);
	if (port < 0) {
		pr_err("Invalid buffer type %d\n", req->type);
		return -EINVAL;
	}

	pr_debug("count = %d\n", req->count);

	mutex_lock(&session->que_lock[port]);
	if (session->io_client[port] != client && session->io_client[port]) {
		ret = -EBUSY;
	} else if (__is_tunneling(session, port)) {
		pr_err("tunneling client can't request buffers\n");
		ret = -EINVAL;
	} else {
		ret = vb2_reqbufs(&session->vbqueue[port], req);
		if (!ret) {
			if (req->count == 0)
				session->io_client[port] = NULL;
			else
				session->io_client[port] = client;
		} else {
			pr_err("reqbufs returned error code %d\n", ret);
		}
	}

	mutex_unlock(&session->que_lock[port]);

	return ret;
}

static int __check_user_planes(struct vb2_queue *vbq, struct v4l2_buffer *b)
{
	/*
	 * check content of user planes array is mapped.
	 * If not then set flag for buf_init callback to map.
	 */
	struct vpu_buffer *vpu_buf;
	struct v4l2_plane *plane;
	int i;

	if (b->m.planes &&
	    b->type == vbq->type &&
	    b->index < vbq->num_buffers &&
	    b->length == vbq->bufs[b->index]->num_planes) {

		vpu_buf = to_vpu_buffer(vbq->bufs[b->index]);
		if (vpu_buf->vb.state != VB2_BUF_STATE_DEQUEUED)
			return -EINVAL;

		for (i = 0; i < b->length; i++) {

			plane = &b->m.planes[i];
			if (plane->m.fd == vpu_buf->planes[i].fd &&
				plane->length == vpu_buf->planes[i].length &&
				plane->reserved[0] ==
					vpu_buf->planes[i].data_offset) {
				/* plane info is unchanged */
				continue;
			} else {
				vpu_buf->planes[i].new_plane = 1;
				vpu_buf->planes[i].fd = plane->m.fd;
				vpu_buf->planes[i].length = plane->length;
				vpu_buf->planes[i].data_offset =
						plane->reserved[0];
			}
		}
		return 0;
	}

	pr_err("Invalid planes parameters\n");
	return -EINVAL;
}

int vpu_qbuf(struct vpu_client *client, struct v4l2_buffer *b)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;
	if (!session)
		return -EPERM;

	port = get_port_number(b->type);
	if (port < 0) {
		pr_err("Invalid type %d\n", b->type);
		return -EINVAL;
	}

	pr_debug("queuing buffer #%d on port %d\n", b->index, port);

	mutex_lock(&session->que_lock[port]);
	if (session->io_client[port] != client ||
			__is_tunneling(session, port)) {
		ret = -EINVAL;
	} else {
		ret = __check_user_planes(&session->vbqueue[port], b);
		if (!ret) {
			struct vpu_buffer *vpu_buf = to_vpu_buffer(
				session->vbqueue[port].bufs[b->index]);
			vpu_buf->sequence = b->sequence;

			ret = vb2_qbuf(&session->vbqueue[port], b);
		}
	}
	mutex_unlock(&session->que_lock[port]);

	return ret;
}

int vpu_dqbuf(struct vpu_client *client, struct v4l2_buffer *b,
		bool nonblocking)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;
	if (!session)
		return -EPERM;

	port = get_port_number(b->type);
	if (port < 0) {
		pr_err("Invalid type %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&session->que_lock[port]);
	if (session->io_client[port] != client ||
			__is_tunneling(session, port)) {
		ret = -EINVAL;
	} else {
		ret = vb2_dqbuf(&session->vbqueue[port], b, nonblocking);
		if (!ret && b->m.planes) {
			int i;
			struct vpu_buffer *vpu_buf = to_vpu_buffer(
				session->vbqueue[port].bufs[b->index]);
			b->sequence = vpu_buf->sequence;

			for (i = 0; i < b->length; i++)
				b->m.planes[i].reserved[0] =
					vpu_buf->planes[i].data_offset;
		}
	}
	mutex_unlock(&session->que_lock[port]);

	if (!ret)
		pr_debug("buffer #%d dequeued on port %d\n", b->index , port);

	return ret;
}

static int __queue_pending_buffers(struct vpu_dev_session *session)
{
	int port, ret = 0;
	struct vpu_buffer *buff, *n;

	BUG_ON(session->streaming_state != ALL_STREAMING);

	for (port = 0; port < NUM_VPU_PORTS; port++) {
		mutex_lock(&session->que_lock[port]);
		list_for_each_entry_safe(buff, n,
			&session->pending_list[port], buffers_entry)
		{
			if (port == INPUT_PORT)
				ret = vpu_hw_session_empty_buffer(session->id,
						buff);
			else
				ret = vpu_hw_session_fill_buffer(session->id,
						buff);

			if (ret) {
				pr_err("returning buffer\n");
				vb2_buffer_done(&buff->vb, VB2_BUF_STATE_ERROR);
			}
			list_del(&buff->buffers_entry);
		}
		mutex_unlock(&session->que_lock[port]);
	}

	return 0;
}

int vpu_flush_bufs(struct vpu_client *client, enum v4l2_buf_type type)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;
	if (!session)
		return -EPERM;

	port = get_port_number(type);
	if (port < 0) {
		pr_err("Invalid type %d\n", type);
		return -EINVAL;
	}

	mutex_lock(&session->lock);

	if (session->io_client[port] != client ||
			__is_tunneling(session, port)) {
		ret = -EINVAL;
		goto exit_flush;
	} else {
		if (!(session->streaming_state & (0x1 << port))) {
			/* Can't flush if port is not streaming */
			ret = -EINVAL;
			goto exit_flush;
		}

		mutex_lock(&session->que_lock[port]);

		ret = vpu_vb2_flush_buffers(session, port);
		if (ret)
			pr_err("Flush failed\n");
		else
			notify_vpu_event_client(client, VPU_EVENT_FLUSH_DONE,
					(u8 *)&type, sizeof(type));

		mutex_unlock(&session->que_lock[port]);
	}

exit_flush:
	mutex_unlock(&session->lock);

	return ret;
}

int vpu_trigger_stream(struct vpu_dev_session *session)
{
	int ret = 0;

	ret = vpu_hw_session_start(session->id);
	if (ret) {
		pr_err("channel start failed\n");
		return ret;
	}

	session->streaming_state = ALL_STREAMING;
	__queue_pending_buffers(session);

	return ret;
}

int vpu_streamon(struct vpu_client *client, enum v4l2_buf_type type)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;
	u32 temp_streaming = 0;
	if (!session)
		return -EPERM;

	port = get_port_number(type);
	if (port < 0) {
		pr_err("Invalid type %d\n", type);
		return -EINVAL;
	}

	mutex_lock(&session->lock); /* needed to sync streamon from two ports */

	mutex_lock(&session->que_lock[port]);
	if (session->io_client[port] != client &&
		(session->io_client[port] || !__is_tunneling(session, port))) {
		ret = -EBUSY;
		goto early_exit_streamon;
	}
	pr_debug("Port %d streaming on\n", port);
	temp_streaming = session->streaming_state;

	ret = vb2_streamon(&session->vbqueue[port], type);
	if (ret)
		goto early_exit_streamon;

	if (temp_streaming & (0x1 << port)) {
		goto early_exit_streamon; /* This port already streaming */
	} else {
		temp_streaming |= (0x1 << port);
		/* lock port if tunneling */
		if (__is_tunneling(session, port))
			session->io_client[port] = client;
	}

	if (temp_streaming != ALL_STREAMING)
		goto early_exit_streamon; /* wait for other port to streamon */

	mutex_unlock(&session->que_lock[port]);

	/* both ports are ready. commit configuration */
	ret = commit_initial_config(session);
	if (ret)
		goto late_exit_streamon;

	ret = call_port_op(session, OUTPUT_PORT, streamon);
	if (ret) {
		pr_err("streamon failed on output port\n");
		goto late_exit_streamon;
	}

	ret = call_port_op(session, INPUT_PORT, streamon);
	if (ret == -EAGAIN) {
		pr_debug("streamon delayed on input port\n");
		ret = 0;
		goto delay_streamon;
	} else if (ret) {
		pr_err("streamon failed on input port\n");
		goto late_exit_streamon;
	}

	/* Start end-to-end session streaming */
	ret = vpu_trigger_stream(session);

	pr_debug("Session streaming started successfully\n");

late_exit_streamon:
	if (ret) {
		/* TODO: How do we notify the streamed on sessions? */
		if (__is_tunneling(session, port))
			session->io_client[port] = NULL;
	}
delay_streamon:
	mutex_unlock(&session->lock);

	return ret;

early_exit_streamon:
	session->streaming_state = temp_streaming;
	mutex_unlock(&session->que_lock[port]);
	mutex_unlock(&session->lock);

	return ret;
}

int vpu_streamoff(struct vpu_client *client, enum v4l2_buf_type type)
{
	struct vpu_dev_session *session = client ? client->session : 0;
	int ret = 0, port;
	if (!session)
		return -EPERM;

	port = get_port_number(type);
	if (port < 0) {
		pr_err("Invalid type %d\n", type);
		return -EINVAL;
	}

	/* session lock needed to protect actions inside vb2_stream_off */
	mutex_lock(&session->lock);

	mutex_lock(&session->que_lock[port]);
	if (session->io_client[port] != client) {
		ret = -EINVAL;
		goto exit_stream_off;
	}
	pr_debug("Port %d streaming off\n", port);

	/* Send streamoff to FW */
	__vpu_streamoff_port(session, port);

	if (vb2_is_streaming(&session->vbqueue[port])) {
		ret = vb2_streamoff(&session->vbqueue[port],
				session->vbqueue[port].type);
		if (ret)
			pr_err("vb2_streamoff failed\n");
	}

	call_port_op(session, port, streamoff);

	/* unlock port if tunneling client */
	if (__is_tunneling(session, port))
		session->io_client[port] = NULL;

exit_stream_off:
	mutex_unlock(&session->que_lock[port]);
	mutex_unlock(&session->lock);

	return ret;
}
