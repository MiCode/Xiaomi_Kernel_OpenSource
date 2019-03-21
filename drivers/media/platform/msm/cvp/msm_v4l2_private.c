// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_v4l2_private.h"
#include "cvp_hfi_api.h"

static int _get_pkt_hdr_from_user(struct cvp_kmd_arg __user *up,
		struct cvp_hal_session_cmd_pkt *pkt_hdr)
{
	struct cvp_kmd_hfi_packet *u;

	u = &up->data.hfi_pkt;

	if (get_user(pkt_hdr->size, &u->pkt_data[0]))
		return -EFAULT;

	if (get_user(pkt_hdr->packet_type, &u->pkt_data[1]))
		return -EFAULT;

	if (get_pkt_index(pkt_hdr) < 0) {
		dprintk(CVP_DBG, "user mode provides incorrect hfi\n");
		goto set_default_pkt_hdr;
	}

	if (pkt_hdr->size > MAX_HFI_PKT_SIZE*sizeof(unsigned int)) {
		dprintk(CVP_ERR, "user HFI packet too large %x\n",
				pkt_hdr->size);
		return -EINVAL;
	}

	return 0;

set_default_pkt_hdr:
	pkt_hdr->size = sizeof(struct hfi_msg_session_hdr);
	return 0;
}

static int _get_fence_pkt_hdr_from_user(struct cvp_kmd_arg __user *up,
		struct cvp_hal_session_cmd_pkt *pkt_hdr)
{
	struct cvp_kmd_hfi_fence_packet *u;

	u = &up->data.hfi_fence_pkt;

	if (get_user(pkt_hdr->packet_type, &u->pkt_data[1]))
		return -EFAULT;

	pkt_hdr->size = (MAX_HFI_FENCE_OFFSET + MAX_HFI_FENCE_SIZE)
			* sizeof(unsigned int);

	if (pkt_hdr->size > (MAX_HFI_PKT_SIZE*sizeof(unsigned int)))
		return -EINVAL;

	return 0;
}

/* Size is in unit of u32 */
static int _copy_pkt_from_user(struct cvp_kmd_arg *kp,
		struct cvp_kmd_arg __user *up,
		unsigned int size)
{
	struct cvp_kmd_hfi_packet *k, *u;
	int i;

	k = &kp->data.hfi_pkt;
	u = &up->data.hfi_pkt;
	for (i = 0; i < size; i++)
		if (get_user(k->pkt_data[i], &u->pkt_data[i]))
			return -EFAULT;

	return 0;
}

/* Size is in unit of u32 */
static int _copy_fence_pkt_from_user(struct cvp_kmd_arg *kp,
		struct cvp_kmd_arg __user *up,
		unsigned int size)
{
	struct cvp_kmd_hfi_fence_packet *k, *u;
	int i;

	k = &kp->data.hfi_fence_pkt;
	u = &up->data.hfi_fence_pkt;
	for (i = 0; i < MAX_HFI_FENCE_OFFSET; i++) {
		if (get_user(k->pkt_data[i], &u->pkt_data[i]))
			return -EFAULT;
	}
	for (i = 0; i < MAX_HFI_FENCE_SIZE; i++) {
		if (get_user(k->fence_data[i], &u->fence_data[i]))
			return -EFAULT;
	}
	return 0;
}

static int _copy_pkt_to_user(struct cvp_kmd_arg *kp,
		struct cvp_kmd_arg __user *up,
		unsigned int size)
{
	struct cvp_kmd_hfi_packet *k, *u;
	int i;

	k = &kp->data.hfi_pkt;
	u = &up->data.hfi_pkt;
	for (i = 0; i < size; i++)
		if (put_user(k->pkt_data[i], &u->pkt_data[i]))
			return -EFAULT;

	return 0;
}

static int _copy_fence_pkt_to_user(struct cvp_kmd_arg *kp,
		struct cvp_kmd_arg __user *up,
		unsigned int size)
{
	struct cvp_kmd_hfi_fence_packet *k, *u;
	int i;

	k = &kp->data.hfi_fence_pkt;
	u = &up->data.hfi_fence_pkt;
	for (i = 0; i < MAX_HFI_FENCE_OFFSET; i++) {
		if (put_user(k->pkt_data[i], &u->pkt_data[i]))
			return -EFAULT;
	}
	for (i = 0; i < MAX_HFI_FENCE_SIZE; i++) {
		if (put_user(k->fence_data[i], &u->fence_data[i]))
			return -EFAULT;
	}
	return 0;
}

static int convert_from_user(struct cvp_kmd_arg *kp, unsigned long arg)
{
	int rc = 0;
	int i;
	struct cvp_kmd_arg __user *up = compat_ptr(arg);
	struct cvp_hal_session_cmd_pkt pkt_hdr;

	if (!kp || !up) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (get_user(kp->type, &up->type))
		return -EFAULT;

	switch (kp->type) {
	case CVP_KMD_GET_SESSION_INFO:
	{
		struct cvp_kmd_session_info *k, *u;

		k = &kp->data.session;
		u = &up->data.session;
		if (get_user(k->session_id, &u->session_id))
			return -EFAULT;
		for (i = 0; i < 10; i++)
			if (get_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case CVP_KMD_REQUEST_POWER:
	{
		struct cvp_kmd_request_power *k, *u;

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
	case CVP_KMD_REGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *k, *u;

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
	case CVP_KMD_UNREGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *k, *u;

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
	case CVP_KMD_HFI_SEND_CMD:
	{
		struct cvp_kmd_send_cmd *k, *u;

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
	case CVP_KMD_SEND_CMD_PKT:
	case CVP_KMD_HFI_DFS_CONFIG_CMD:
	case CVP_KMD_HFI_DFS_FRAME_CMD:
	case CVP_KMD_HFI_DME_CONFIG_CMD:
	case CVP_KMD_HFI_DME_FRAME_CMD:
	case CVP_KMD_HFI_PERSIST_CMD:
	{
		if (_get_pkt_hdr_from_user(up, &pkt_hdr)) {
			dprintk(CVP_ERR, "Invalid syscall: %x, %x, %x\n",
				kp->type, pkt_hdr.size, pkt_hdr.packet_type);
			return -EFAULT;
		}

		dprintk(CVP_DBG, "system call cmd pkt: %d 0x%x\n",
				pkt_hdr.size, pkt_hdr.packet_type);
		rc = _copy_pkt_from_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		if (_get_fence_pkt_hdr_from_user(up, &pkt_hdr)) {
			dprintk(CVP_ERR, "Invalid syscall: %x, %x, %x\n",
				kp->type, pkt_hdr.size, pkt_hdr.packet_type);
			return -EFAULT;
		}

		dprintk(CVP_DBG, "system call cmd pkt: %d 0x%x\n",
				pkt_hdr.size, pkt_hdr.packet_type);
		rc = _copy_fence_pkt_from_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_HFI_DFS_FRAME_CMD_RESPONSE:
	case CVP_KMD_HFI_DME_FRAME_CMD_RESPONSE:
	case CVP_KMD_HFI_PERSIST_CMD_RESPONSE:
	case CVP_KMD_RECEIVE_MSG_PKT:
		break;
	default:
		dprintk(CVP_ERR, "%s: unknown cmd type 0x%x\n",
			__func__, kp->type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int convert_to_user(struct cvp_kmd_arg *kp, unsigned long arg)
{
	int rc = 0;
	int i, size = sizeof(struct hfi_msg_session_hdr) >> 2;
	struct cvp_kmd_arg __user *up = compat_ptr(arg);
	struct cvp_hal_session_cmd_pkt pkt_hdr;

	if (!kp || !up) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (put_user(kp->type, &up->type))
		return -EFAULT;

	switch (kp->type) {
	case CVP_KMD_RECEIVE_MSG_PKT:
	{
		struct cvp_kmd_hfi_packet *k, *u;

		k = &kp->data.hfi_pkt;
		u = &up->data.hfi_pkt;
		for (i = 0; i < size; i++)
			if (put_user(k->pkt_data[i], &u->pkt_data[i]))
				return -EFAULT;
		break;
	}
	case CVP_KMD_GET_SESSION_INFO:
	{
		struct cvp_kmd_session_info *k, *u;

		k = &kp->data.session;
		u = &up->data.session;
		if (put_user(k->session_id, &u->session_id))
			return -EFAULT;
		for (i = 0; i < 10; i++)
			if (put_user(k->reserved[i], &u->reserved[i]))
				return -EFAULT;
		break;
	}
	case CVP_KMD_REQUEST_POWER:
	{
		struct cvp_kmd_request_power *k, *u;

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
	case CVP_KMD_REGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *k, *u;

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
	case CVP_KMD_UNREGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *k, *u;

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
	case CVP_KMD_HFI_SEND_CMD:
	{
		struct cvp_kmd_send_cmd *k, *u;

		dprintk(CVP_DBG, "%s: CVP_KMD_HFI_SEND_CMD\n",
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
	case CVP_KMD_SEND_CMD_PKT:
	case CVP_KMD_HFI_DFS_CONFIG_CMD:
	case CVP_KMD_HFI_DFS_FRAME_CMD:
	case CVP_KMD_HFI_DFS_FRAME_CMD_RESPONSE:
	case CVP_KMD_HFI_DME_CONFIG_CMD:
	case CVP_KMD_HFI_DME_FRAME_CMD:
	case CVP_KMD_HFI_DME_FRAME_CMD_RESPONSE:
	case CVP_KMD_HFI_PERSIST_CMD:
	case CVP_KMD_HFI_PERSIST_CMD_RESPONSE:
	{
		if (_get_pkt_hdr_from_user(up, &pkt_hdr))
			return -EFAULT;

		dprintk(CVP_DBG, "Send user cmd pkt: %d %d\n",
				pkt_hdr.size, pkt_hdr.packet_type);
		rc = _copy_pkt_to_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		if (_get_fence_pkt_hdr_from_user(up, &pkt_hdr))
			return -EFAULT;

		dprintk(CVP_DBG, "Send user cmd pkt: %d %d\n",
				pkt_hdr.size, pkt_hdr.packet_type);
		rc = _copy_fence_pkt_to_user(kp, up, (pkt_hdr.size >> 2));
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
	struct cvp_kmd_arg karg;

	if (!filp || !filp->private_data) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	inst = container_of(filp->private_data, struct msm_cvp_inst,
			event_handler);
	memset(&karg, 0, sizeof(struct cvp_kmd_arg));

	/*
	 * the arg points to user space memory and needs
	 * to be converted to kernel space before using it.
	 * Check do_video_ioctl() for more details.
	 */
	if (convert_from_user(&karg, arg)) {
		dprintk(CVP_ERR, "%s: failed to get from user cmd %x\n",
			__func__, karg.type);
		return -EFAULT;
	}

	rc = msm_cvp_private((void *)inst, cmd, &karg);
	if (rc) {
		dprintk(CVP_ERR, "%s: failed cmd type %x\n",
			__func__, karg.type);
		return -EINVAL;
	}

	if (convert_to_user(&karg, arg)) {
		dprintk(CVP_ERR, "%s: failed to copy to user cmd %x\n",
			__func__, karg.type);
		return -EFAULT;
	}

	return rc;
}
