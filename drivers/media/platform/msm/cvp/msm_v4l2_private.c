// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_v4l2_private.h"

static int convert_from_user(struct msm_cvp_arg *kp, unsigned long arg)
{
	int rc = 0;
	int i;
	struct msm_cvp_arg __user *up = compat_ptr(arg);

	if (!kp || !up) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (get_user(kp->type, &up->type))
		return -EFAULT;

	switch (kp->type) {
	case MSM_CVP_GET_SESSION_INFO:
	{
		struct msm_cvp_session_info *k, *u;

		k = &kp->data.session;
		u = &up->data.session;
		if (get_user(k->session_id, &u->session_id))
			return -EFAULT;
		for (i = 0; i < 10; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_REQUEST_POWER:
	{
		struct msm_cvp_request_power *k, *u;

		k = &kp->data.req_power;
		u = &up->data.req_power;
		if (get_user(k->clock_cycles_a, &u->clock_cycles_a) ||
			get_user(k->clock_cycles_b, &u->clock_cycles_b) ||
			get_user(k->ddr_bw, &u->ddr_bw) ||
			get_user(k->sys_cache_bw, &u->sys_cache_bw))
			return -EFAULT;
		for (i = 0; i < 8; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_REGISTER_BUFFER:
	{
		struct msm_cvp_buffer *k, *u;

		k = &kp->data.regbuf;
		u = &up->data.regbuf;
		if (get_user(k->type, &u->type) ||
			get_user(k->index, &u->index) ||
			get_user(k->fd, &u->fd) ||
			get_user(k->size, &u->size) ||
			get_user(k->offset, &u->offset) ||
			get_user(k->pixelformat, &u->pixelformat) ||
			get_user(k->flags, &u->flags))
			return -EFAULT;
		for (i = 0; i < 5; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_UNREGISTER_BUFFER:
	{
		struct msm_cvp_buffer *k, *u;

		k = &kp->data.unregbuf;
		u = &up->data.unregbuf;
		if (get_user(k->type, &u->type) ||
			get_user(k->index, &u->index) ||
			get_user(k->fd, &u->fd) ||
			get_user(k->size, &u->size) ||
			get_user(k->offset, &u->offset) ||
			get_user(k->pixelformat, &u->pixelformat) ||
			get_user(k->flags, &u->flags))
			return -EFAULT;
		for (i = 0; i < 5; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_SEND_CMD:
	{
		struct msm_cvp_send_cmd *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_SEND_CMD\n",
				__func__);
		k = &kp->data.send_cmd;
		u = &up->data.send_cmd;
		if (get_user(k->cmd_address_fd, &u->cmd_address_fd) ||
			get_user(k->cmd_size, &u->cmd_size))
			return -EFAULT;
		for (i = 0; i < 10; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DFS_CONFIG_CMD:
	{
		struct msm_cvp_dfsconfig *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_DFS_CONFIG_CMD\n",
				__func__);
		k = &kp->data.dfsconfig;
		u = &up->data.dfsconfig;
		if (get_user(k->cmd_address, &u->cmd_address) ||
			get_user(k->cmd_size, &u->cmd_size) ||
			get_user(k->packet_type, &u->packet_type) ||
			get_user(k->session_id, &u->session_id) ||
			get_user(k->srcbuffer_format, &u->srcbuffer_format) ||
			get_user(
			k->left_plane_info.stride[HFI_MAX_PLANES - 1],
			&u->left_plane_info.stride[HFI_MAX_PLANES - 1]) ||
			get_user(
			k->left_plane_info.buf_size[HFI_MAX_PLANES - 1],
			&u->left_plane_info.buf_size[HFI_MAX_PLANES - 1]) ||
			get_user(
			k->right_plane_info.stride[HFI_MAX_PLANES - 1],
			&u->right_plane_info.stride[HFI_MAX_PLANES - 1]) ||
			get_user(
			k->right_plane_info.buf_size[HFI_MAX_PLANES - 1],
			&u->right_plane_info.buf_size[HFI_MAX_PLANES - 1]) ||
			get_user(k->width, &u->width) ||
			get_user(k->height, &u->height) ||
			get_user(k->occlusionmask_enable,
				&u->occlusionmask_enable) ||
			get_user(k->occlusioncost, &u->occlusioncost) ||
			get_user(k->occlusionshift, &u->occlusionshift) ||
			get_user(k->maxdisparity, &u->maxdisparity) ||
			get_user(k->disparityoffset, &u->disparityoffset) ||
			get_user(k->medianfilter_enable,
				&u->medianfilter_enable) ||
			get_user(k->occlusionbound, &u->occlusionbound) ||
			get_user(k->occlusionfilling_enable,
				&u->occlusionfilling_enable) ||
			get_user(k->occlusionmaskdump,
				&u->occlusionmaskdump) ||
			get_user(k->clientdata.transactionid,
				&u->clientdata.transactionid) ||
			get_user(k->clientdata.client_data1,
				&u->clientdata.client_data1) ||
			get_user(k->clientdata.client_data2,
				&u->clientdata.client_data2))
			return -EFAULT;
		for (i = 0; i < MAX_DFS_HFI_PARAMS; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD:
	case MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dfsframe *k, *u;

		dprintk(CVP_DBG, "%s: Type =%d\n",
							__func__, kp->type);
		k = &kp->data.dfsframe;
		u = &up->data.dfsframe;
		if (get_user(k->cmd_address, &u->cmd_address) ||
			get_user(k->cmd_size, &u->cmd_size) ||
			get_user(k->packet_type, &u->packet_type) ||
			get_user(k->session_id, &u->session_id) ||
			get_user(k->left_buffer_index,
				&u->left_buffer_index) ||
			get_user(k->right_buffer_index,
				&u->right_buffer_index) ||
			get_user(k->disparitymap_buffer_idx,
				&u->disparitymap_buffer_idx) ||
			get_user(k->occlusionmask_buffer_idx,
				&u->occlusionmask_buffer_idx) ||
			get_user(k->clientdata.transactionid,
				&u->clientdata.transactionid) ||
			get_user(k->clientdata.client_data1,
				&u->clientdata.client_data1) ||
			get_user(k->clientdata.client_data2,
				&u->clientdata.client_data2))
			return -EFAULT;

		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown cmd type 0x%x\n",
			__func__, kp->type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int convert_to_user(struct msm_cvp_arg *kp, unsigned long arg)
{
	int rc = 0;
	int i;
	struct msm_cvp_arg __user *up = compat_ptr(arg);

	if (!kp || !up) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (put_user(kp->type, &up->type))
		return -EFAULT;

	switch (kp->type) {
	case MSM_CVP_GET_SESSION_INFO:
	{
		struct msm_cvp_session_info *k, *u;

		k = &kp->data.session;
		u = &up->data.session;
		if (put_user(k->session_id, &u->session_id))
			return -EFAULT;
		for (i = 0; i < 10; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_REQUEST_POWER:
	{
		struct msm_cvp_request_power *k, *u;

		k = &kp->data.req_power;
		u = &up->data.req_power;
		if (put_user(k->clock_cycles_a, &u->clock_cycles_a) ||
			put_user(k->clock_cycles_b, &u->clock_cycles_b) ||
			put_user(k->ddr_bw, &u->ddr_bw) ||
			put_user(k->sys_cache_bw, &u->sys_cache_bw))
			return -EFAULT;
		for (i = 0; i < 8; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_REGISTER_BUFFER:
	{
		struct msm_cvp_buffer *k, *u;

		k = &kp->data.regbuf;
		u = &up->data.regbuf;
		if (put_user(k->type, &u->type) ||
			put_user(k->index, &u->index) ||
			put_user(k->fd, &u->fd) ||
			put_user(k->size, &u->size) ||
			put_user(k->offset, &u->offset) ||
			put_user(k->pixelformat, &u->pixelformat) ||
			put_user(k->flags, &u->flags))
			return -EFAULT;
		for (i = 0; i < 5; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_UNREGISTER_BUFFER:
	{
		struct msm_cvp_buffer *k, *u;

		k = &kp->data.unregbuf;
		u = &up->data.unregbuf;
		if (put_user(k->type, &u->type) ||
			put_user(k->index, &u->index) ||
			put_user(k->fd, &u->fd) ||
			put_user(k->size, &u->size) ||
			put_user(k->offset, &u->offset) ||
			put_user(k->pixelformat, &u->pixelformat) ||
			put_user(k->flags, &u->flags))
			return -EFAULT;
		for (i = 0; i < 5; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_SEND_CMD:
	{
		struct msm_cvp_send_cmd *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_SEND_CMD\n",
					__func__);

		k = &kp->data.send_cmd;
		u = &up->data.send_cmd;
		if (put_user(k->cmd_address_fd, &u->cmd_address_fd) ||
			put_user(k->cmd_size, &u->cmd_size))
			return -EFAULT;
		for (i = 0; i < 10; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DFS_CONFIG_CMD:
	{
		struct msm_cvp_dfsconfig *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_DFS_CONFIG_CMD\n",
					__func__);

		k = &kp->data.dfsconfig;
		u = &up->data.dfsconfig;
		if (put_user(k->cmd_address, &u->cmd_address) ||
			put_user(k->cmd_size, &u->cmd_size) ||
			put_user(k->packet_type, &u->packet_type) ||
			put_user(k->session_id, &u->session_id) ||
			put_user(k->srcbuffer_format, &u->srcbuffer_format) ||
			put_user(
			k->left_plane_info.stride[HFI_MAX_PLANES - 1],
			&u->left_plane_info.stride[HFI_MAX_PLANES - 1]) ||
			put_user(
			k->left_plane_info.buf_size[HFI_MAX_PLANES - 1],
			&u->left_plane_info.buf_size[HFI_MAX_PLANES - 1]) ||
			put_user(
			k->right_plane_info.stride[HFI_MAX_PLANES - 1],
			&u->right_plane_info.stride[HFI_MAX_PLANES - 1]) ||
			put_user(
			k->right_plane_info.buf_size[HFI_MAX_PLANES - 1],
			&u->right_plane_info.buf_size[HFI_MAX_PLANES - 1])
			|| put_user(k->width, &u->width) ||
			put_user(k->height, &u->height) ||
			put_user(k->occlusionmask_enable,
				&u->occlusionmask_enable) ||
			put_user(k->occlusioncost, &u->occlusioncost) ||
			put_user(k->occlusionshift, &u->occlusionshift) ||
			put_user(k->maxdisparity, &u->maxdisparity) ||
			put_user(
				k->disparityoffset, &u->disparityoffset) ||
			put_user(k->medianfilter_enable,
				&u->medianfilter_enable) ||
			put_user(k->occlusionbound, &u->occlusionbound) ||
			put_user(k->occlusionfilling_enable,
				&u->occlusionfilling_enable) ||
			put_user(k->occlusionmaskdump,
				&u->occlusionmaskdump) ||
			put_user(k->clientdata.transactionid,
				&u->clientdata.transactionid) ||
			put_user(k->clientdata.client_data1,
				&u->clientdata.client_data1) ||
			put_user(k->clientdata.client_data2,
				&u->clientdata.client_data2))
			return -EFAULT;
		for (i = 0; i < MAX_DFS_HFI_PARAMS; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD:
	case MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dfsframe *k, *u;

		dprintk(CVP_DBG, "%s: type = %d\n",
					__func__, kp->type);
		k = &kp->data.dfsframe;
		u = &up->data.dfsframe;

		if (put_user(k->cmd_address, &u->cmd_address) ||
			put_user(k->cmd_size, &u->cmd_size) ||
			put_user(k->packet_type, &u->packet_type) ||
			put_user(k->session_id, &u->session_id) ||
			put_user(k->left_buffer_index,
				&u->left_buffer_index) ||
			put_user(k->right_buffer_index,
				&u->right_buffer_index) ||
			put_user(k->disparitymap_buffer_idx,
				&u->disparitymap_buffer_idx) ||
			put_user(k->occlusionmask_buffer_idx,
				&u->occlusionmask_buffer_idx) ||
			put_user(k->clientdata.transactionid,
				&u->clientdata.transactionid) ||
			put_user(k->clientdata.client_data1,
				&u->clientdata.client_data1) ||
			put_user(k->clientdata.client_data2,
				&u->clientdata.client_data2))
			return -EFAULT;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown cmd type 0x%x\n",
			__func__, kp->type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

long msm_cvp_v4l2_private(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int rc;
	struct msm_cvp_inst *inst;
	struct msm_cvp_arg karg;

	if (!filp || !filp->private_data) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	inst = container_of(filp->private_data, struct msm_cvp_inst,
			event_handler);
	memset(&karg, 0, sizeof(struct msm_cvp_arg));

	/*
	 * the arg points to user space memory and needs
	 * to be converted to kernel space before using it.
	 * Check do_video_ioctl() for more details.
	 */
	if (convert_from_user(&karg, arg))
		return -EFAULT;

	rc = msm_cvp_private((void *)inst, cmd, &karg);
	if (rc) {
		dprintk(CVP_ERR, "%s: failed cmd type %x\n",
			__func__, karg.type);
		return -EINVAL;
	}

	if (convert_to_user(&karg, arg))
		return -EFAULT;

	return rc;
}
