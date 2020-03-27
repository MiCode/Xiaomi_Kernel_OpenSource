// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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
		dprintk(CVP_ERR, "user mode provides incorrect hfi\n");
		goto set_default_pkt_hdr;
	}

	if (pkt_hdr->size > MAX_HFI_PKT_SIZE*sizeof(unsigned int)) {
		dprintk(CVP_ERR, "user HFI packet too large %x\n",
				pkt_hdr->size);
		return -EINVAL;
	}

	return 0;

set_default_pkt_hdr:
	pkt_hdr->size = get_msg_size();
	return 0;
}

static int _get_fence_pkt_hdr_from_user(struct cvp_kmd_arg __user *up,
		struct cvp_hal_session_cmd_pkt *pkt_hdr)
{
	struct cvp_kmd_hfi_fence_packet *u;

	u = &up->data.hfi_fence_pkt;

	if (get_user(pkt_hdr->size, &u->pkt_data[0]))
		return -EFAULT;

	if (get_user(pkt_hdr->packet_type, &u->pkt_data[1]))
		return -EFAULT;

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

static int _copy_sysprop_from_user(struct cvp_kmd_arg *kp,
		struct cvp_kmd_arg __user *up)
{
	struct cvp_kmd_sys_properties *k, *u;

	k = &kp->data.sys_properties;
	u = &up->data.sys_properties;

	if (get_user(k->prop_num, &u->prop_num))
		return -EFAULT;

	if (k->prop_num < 1 || k->prop_num > 32) {
		dprintk(CVP_ERR, "Num of prop out of range %d\n", k->prop_num);
		return -EFAULT;
	}

	return _copy_pkt_from_user(kp, up,
		(k->prop_num*((sizeof(struct cvp_kmd_sys_property)>>2)+1)));
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

static int _copy_sysprop_to_user(struct cvp_kmd_arg *kp,
		struct cvp_kmd_arg __user *up)
{
	struct cvp_kmd_sys_properties *k, *u;

	k = &kp->data.sys_properties;
	u = &up->data.sys_properties;

	if (put_user(k->prop_data.data, &u->prop_data.data))
		return -EFAULT;

	return 0;

}

static void _set_deprecate_bitmask(struct cvp_kmd_arg *kp,
			struct msm_cvp_inst *inst)
{
	dprintk(CVP_INFO, "%s: kp->type = %#x\n", __func__, kp->type);

	switch (kp->type) {
	case CVP_KMD_HFI_DFS_FRAME_CMD:
	{
		set_bit(DFS_BIT_OFFSET, &inst->deprecate_bitmask);
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_CMD:
	{
		set_bit(DME_BIT_OFFSET, &inst->deprecate_bitmask);
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		set_bit(DME_BIT_OFFSET, &inst->deprecate_bitmask);
		break;
	}
	case CVP_KMD_HFI_FD_FRAME_CMD:
	{
		set_bit(FD_BIT_OFFSET, &inst->deprecate_bitmask);
		break;
	}
	default:
		break;
	}
}

static void print_hfi_short(struct cvp_kmd_arg __user *up)
{
	struct cvp_kmd_hfi_packet *pkt;
	unsigned int words[5];

	pkt = &up->data.hfi_pkt;
	if (get_user(words[0], &up->type) ||
			get_user(words[1], &up->buf_offset) ||
			get_user(words[2], &up->buf_num) ||
			get_user(words[3], &pkt->pkt_data[0]) ||
			get_user(words[4], &pkt->pkt_data[1]))
		dprintk(CVP_ERR, "Failed to print ioctl cmd\n");

	dprintk(CVP_DBG, "IOCTL cmd type %#x, offset %d, num %d, pkt %d %#x\n",
			words[0], words[1], words[2], words[3], words[4]);
}

static int _copy_session_ctrl_to_user(
	struct cvp_kmd_session_control *k,
	struct cvp_kmd_session_control *u)
{
	int i;

	if (put_user(k->ctrl_type, &u->ctrl_type))
		return -EFAULT;
	for (i = 0; i < 8; i++)
		if (put_user(k->ctrl_data[i], &u->ctrl_data[i]))
			return -EFAULT;
	return 0;
}

static int _get_session_ctrl_from_user(
	struct cvp_kmd_session_control *k,
	struct cvp_kmd_session_control *u)
{
	int i;

	if (get_user(k->ctrl_type, &u->ctrl_type))
		return -EFAULT;

	for (i = 0; i < 8; i++)
		if (get_user(k->ctrl_data[i], &u->ctrl_data[i]))
			return -EFAULT;
	return 0;
}

static int convert_from_user(struct cvp_kmd_arg *kp,
		unsigned long arg,
		struct msm_cvp_inst *inst)
{
	int rc = 0;
	int i;
	struct cvp_kmd_arg __user *up = (struct cvp_kmd_arg *)arg;
	struct cvp_hal_session_cmd_pkt pkt_hdr;
	int pkt_idx;

	if (!kp || !up) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	print_hfi_short(up);

	if (get_user(kp->type, &up->type))
		return -EFAULT;

	_set_deprecate_bitmask(kp, inst);

	if (get_user(kp->buf_offset, &up->buf_offset) ||
		get_user(kp->buf_num, &up->buf_num))
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
	case CVP_KMD_HFI_FD_FRAME_CMD:
	{
		if (_get_pkt_hdr_from_user(up, &pkt_hdr)) {
			dprintk(CVP_ERR, "Invalid syscall: %x, %x, %x\n",
				kp->type, pkt_hdr.size, pkt_hdr.packet_type);
			return -EFAULT;
		}

		rc = _copy_pkt_from_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_SEND_FENCE_CMD_PKT:
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		if (_get_fence_pkt_hdr_from_user(up, &pkt_hdr)) {
			dprintk(CVP_ERR, "Invalid syscall: %x, %x, %x\n",
				kp->type, pkt_hdr.size, pkt_hdr.packet_type);
			return -EFAULT;
		}
		dprintk(CVP_DBG, "system call cmd pkt: %d 0x%x\n",
				pkt_hdr.size, pkt_hdr.packet_type);

		pkt_idx = get_pkt_index(&pkt_hdr);
		if (pkt_idx < 0) {
			dprintk(CVP_ERR, "%s incorrect packet %d, %x\n",
				__func__,
				pkt_hdr.size,
				pkt_hdr.packet_type);
			return -EFAULT;
		}

		set_feature_bitmask(pkt_idx, &inst->deprecate_bitmask);

		rc = _copy_fence_pkt_from_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_HFI_DFS_FRAME_CMD_RESPONSE:
	case CVP_KMD_HFI_DME_FRAME_CMD_RESPONSE:
	case CVP_KMD_HFI_PERSIST_CMD_RESPONSE:
	case CVP_KMD_RECEIVE_MSG_PKT:
		break;
	case CVP_KMD_SESSION_CONTROL:
	{
		struct cvp_kmd_session_control *k, *u;

		k = &kp->data.session_ctrl;
		u = &up->data.session_ctrl;

		rc = _get_session_ctrl_from_user(k, u);
		break;
	}
	case CVP_KMD_GET_SYS_PROPERTY:
	{
		if (_copy_sysprop_from_user(kp, up)) {
			dprintk(CVP_ERR, "Failed to get sysprop from user\n");
			return -EFAULT;
		}
		break;
	}
	case CVP_KMD_SET_SYS_PROPERTY:
	{
		if (_copy_sysprop_from_user(kp, up)) {
			dprintk(CVP_ERR, "Failed to set sysprop from user\n");
			return -EFAULT;
		}
		break;
	}
	case CVP_KMD_UPDATE_POWER:
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
	int i, size = get_msg_size() >> 2;
	struct cvp_kmd_arg __user *up = (struct cvp_kmd_arg *)arg;
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
	case CVP_KMD_HFI_FD_FRAME_CMD:
	{
		if (_get_pkt_hdr_from_user(up, &pkt_hdr))
			return -EFAULT;

		dprintk(CVP_DBG, "Send user cmd pkt: %d %d\n",
				pkt_hdr.size, pkt_hdr.packet_type);
		rc = _copy_pkt_to_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_SEND_FENCE_CMD_PKT:
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		if (_get_fence_pkt_hdr_from_user(up, &pkt_hdr))
			return -EFAULT;

		dprintk(CVP_DBG, "Send user cmd pkt: %d %d\n",
				pkt_hdr.size, pkt_hdr.packet_type);

		rc = _copy_fence_pkt_to_user(kp, up, (pkt_hdr.size >> 2));
		break;
	}
	case CVP_KMD_SESSION_CONTROL:
	{
		struct cvp_kmd_session_control *k, *u;

		k = &kp->data.session_ctrl;
		u = &up->data.session_ctrl;
		rc = _copy_session_ctrl_to_user(k, u);
		break;
	}
	case CVP_KMD_GET_SYS_PROPERTY:
	{
		if (_copy_sysprop_to_user(kp, up)) {
			dprintk(CVP_ERR, "Fail to copy sysprop to user\n");
			return -EFAULT;
		}
		break;
	}
	case CVP_KMD_SET_SYS_PROPERTY:
		break;
	case CVP_KMD_UPDATE_POWER:
		break;
	default:
		dprintk(CVP_ERR, "%s: unknown cmd type 0x%x\n",
			__func__, kp->type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static long cvp_ioctl(struct msm_cvp_inst *inst,
	unsigned int cmd, unsigned long arg)
{
	int rc;
	struct cvp_kmd_arg *karg;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	karg = kzalloc(sizeof(*karg), GFP_KERNEL);
	if (!karg)
		return -ENOMEM;

	if (convert_from_user(karg, arg, inst)) {
		dprintk(CVP_ERR, "%s: failed to get from user cmd %x\n",
			__func__, karg->type);
		kfree(karg);
		return -EFAULT;
	}

	rc = msm_cvp_private((void *)inst, cmd, karg);
	if (rc) {
		dprintk(CVP_ERR, "%s: failed cmd type %x %d\n",
			__func__, karg->type, rc);
		kfree(karg);
		return rc;
	}

	if (convert_to_user(karg, arg)) {
		dprintk(CVP_ERR, "%s: failed to copy to user cmd %x\n",
			__func__, karg->type);
		kfree(karg);
		return -EFAULT;
	}

	kfree(karg);
	return rc;
}

long cvp_unblocked_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct msm_cvp_inst *inst;

	if (!filp || !filp->private_data) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	inst = filp->private_data;
	return cvp_ioctl(inst, cmd, arg);
}

long cvp_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct msm_cvp_inst *inst;

	if (!filp || !filp->private_data) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	inst = filp->private_data;
	return cvp_ioctl(inst, cmd, (unsigned long)compat_ptr(arg));
}
