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
		struct msm_cvp_dfs_config *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_DFS_CONFIG_CMD\n", __func__);
		k = &kp->data.dfs_config;
		u = &up->data.dfs_config;
		for (i = 0; i < CVP_DFS_CONFIG_CMD_SIZE; i++)
			if (get_user(k->cvp_dfs_config[i],
				&u->cvp_dfs_config[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD:
	case MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dfs_frame *k, *u;

		dprintk(CVP_DBG, "%s: Type =%d\n", __func__, kp->type);
		k = &kp->data.dfs_frame;
		u = &up->data.dfs_frame;
		for (i = 0; i < CVP_DFS_FRAME_CMD_SIZE; i++)
			if (get_user(k->frame_data[i], &u->frame_data[i]))
				return -EFAULT;

		break;
	}
	case MSM_CVP_HFI_DME_CONFIG_CMD:
	{
		struct msm_cvp_dme_config *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_DFS_CONFIG_CMD\n", __func__);
		k = &kp->data.dme_config;
		u = &up->data.dme_config;
		for (i = 0; i < CVP_DME_CONFIG_CMD_SIZE; i++)
			if (get_user(k->cvp_dme_config[i],
				&u->cvp_dme_config[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DME_FRAME_CMD:
	case MSM_CVP_HFI_DME_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dme_frame *k, *u;

		dprintk(CVP_DBG, "%s: type = %d\n",
					__func__, kp->type);
		k = &kp->data.dme_frame;
		u = &up->data.dme_frame;

		for (i = 0; i < CVP_DME_FRAME_CMD_SIZE; i++)
			if (get_user(k->frame_data[i], &u->frame_data[i]))
				return -EFAULT;

		break;
	}
	case MSM_CVP_HFI_PERSIST_CMD:
	case MSM_CVP_HFI_PERSIST_CMD_RESPONSE:
	{
		struct msm_cvp_persist_buf *k, *u;

		dprintk(CVP_DBG, "%s: type = %d\n",
					__func__, kp->type);
		k = &kp->data.pbuf_cmd;
		u = &up->data.pbuf_cmd;

		for (i = 0; i < CVP_PERSIST_CMD_SIZE; i++)
			if (get_user(k->persist_data[i], &u->persist_data[i]))
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
		struct msm_cvp_dfs_config *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_DFS_CONFIG_CMD\n",
					__func__);

		k = &kp->data.dfs_config;
		u = &up->data.dfs_config;
		for (i = 0; i < CVP_DFS_CONFIG_CMD_SIZE; i++)
			if (put_user(k->cvp_dfs_config[i],
				&u->cvp_dfs_config[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD:
	case MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dfs_frame *k, *u;

		dprintk(CVP_DBG, "%s: type = %d\n",
					__func__, kp->type);
		k = &kp->data.dfs_frame;
		u = &up->data.dfs_frame;

		for (i = 0; i < CVP_DFS_FRAME_CMD_SIZE; i++)
			if (put_user(k->frame_data[i], &u->frame_data[i]))
				return -EFAULT;

		break;
	}
	case MSM_CVP_HFI_DME_CONFIG_CMD:
	{
		struct msm_cvp_dme_config *k, *u;

		dprintk(CVP_DBG, "%s: MSM_CVP_HFI_DME_CONFIG_CMD\n", __func__);
		k = &kp->data.dme_config;
		u = &up->data.dme_config;
		for (i = 0; i < CVP_DME_CONFIG_CMD_SIZE; i++)
			if (put_user(k->cvp_dme_config[i],
				&u->cvp_dme_config[i]))
				return -EFAULT;
		break;
	}
	case MSM_CVP_HFI_DME_FRAME_CMD:
	case MSM_CVP_HFI_DME_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dme_frame *k, *u;

		dprintk(CVP_DBG, "%s: type = %d\n",
					__func__, kp->type);
		k = &kp->data.dme_frame;
		u = &up->data.dme_frame;

		for (i = 0; i < CVP_DME_FRAME_CMD_SIZE; i++)
			if (put_user(k->frame_data[i], &u->frame_data[i]))
				return -EFAULT;

		break;
	}
	case MSM_CVP_HFI_PERSIST_CMD:
	case MSM_CVP_HFI_PERSIST_CMD_RESPONSE:
	{
		struct msm_cvp_persist_buf *k, *u;

		dprintk(CVP_DBG, "%s: type = %d\n",
					__func__, kp->type);
		k = &kp->data.pbuf_cmd;
		u = &up->data.pbuf_cmd;

		for (i = 0; i < CVP_PERSIST_CMD_SIZE; i++)
			if (put_user(k->persist_data[i], &u->persist_data[i]))
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
