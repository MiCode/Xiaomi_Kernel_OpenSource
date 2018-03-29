/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include "compat_mtk_disp_mgr.h"

#include "disp_drv_log.h"
#include "debug.h"
#include "primary_display.h"
#include "display_recorder.h"
#include "mtkfb_fence.h"
#include "disp_drv_platform.h"


#ifdef CONFIG_COMPAT
static int compat_get_disp_output_config(compat_disp_output_config __user *data32,
					disp_output_config __user *data)
{
	compat_uint_t u;
	compat_uptr_t p;
	int err = 0;

	err |= get_user(p, (unsigned long *)&(data32->va));
	err |= put_user(p, (unsigned long *)&(data->va));

	err |= get_user(p, (unsigned long *)&(data32->pa));
	err |= put_user(p, (unsigned long *)&(data->pa));

	err |= get_user(u, &(data32->fmt));
	err |= put_user(u, &(data->fmt));

	err |= get_user(u, &(data32->x));
	err |= put_user(u, &(data->x));

	err |= get_user(u, &(data32->y));
	err |= put_user(u, &(data->y));

	err |= get_user(u, &(data32->width));
	err |= put_user(u, &(data->width));

	err |= get_user(u, &(data32->height));
	err |= put_user(u, &(data->height));

	err |= get_user(u, &(data32->pitch));
	err |= put_user(u, &(data->pitch));

	err |= get_user(u, &(data32->pitchUV));
	err |= put_user(u, &(data->pitchUV));

	err |= get_user(u, &(data32->security));
	err |= put_user(u, &(data->security));

	err |= get_user(u, &(data32->buff_idx));
	err |= put_user(u, &(data->buff_idx));

	err |= get_user(u, &(data32->interface_idx));
	err |= put_user(u, &(data->interface_idx));

	err |= get_user(u, &(data32->frm_sequence));
	err |= put_user(u, &(data->frm_sequence));

	return err;
}

static int compat_get_disp_session_output_config(compat_disp_session_output_config __user *data32,
						 disp_session_output_config __user *data)
{
	compat_uint_t u;
	int err = 0;

	err = get_user(u, &(data32->session_id));
	err |= put_user(u, &(data->session_id));

	err |= compat_get_disp_output_config(&data32->config, &data->config);

	return err;
}

static int compat_get_disp_input_config(compat_disp_input_config __user *data32,
					disp_input_config __user *data)
{
	compat_uint_t u;
	compat_int_t i;
	compat_uptr_t p;
	int err = 0;

	err |= get_user(u, &(data32->layer_id));
	err |= put_user(u, &(data->layer_id));

	err |= get_user(u, &(data32->layer_enable));
	err |= put_user(u, &(data->layer_enable));

	err |= get_user(u, &(data32->buffer_source));
	err |= put_user(u, &(data->buffer_source));

	err |= get_user(p, (unsigned long *)&(data32->src_base_addr));
	err |= put_user(p, (unsigned long *)&(data->src_base_addr));

	err |= get_user(p, (unsigned long *)&(data32->src_phy_addr));
	err |= put_user(p, (unsigned long *)&(data->src_phy_addr));

	err |= get_user(u, &(data32->src_direct_link));
	err |= put_user(u, &(data->src_direct_link));

	err |= get_user(u, &(data32->src_fmt));
	err |= put_user(u, &(data->src_fmt));

	err |= get_user(u, &(data32->src_use_color_key));
	err |= put_user(u, &(data->src_use_color_key));

	err |= get_user(u, &(data32->src_pitch));
	err |= put_user(u, &(data->src_pitch));

	err |= get_user(u, &(data32->src_offset_x));
	err |= put_user(u, &(data->src_offset_x));

	err |= get_user(u, &(data32->src_offset_y));
	err |= put_user(u, &(data->src_offset_y));

	err |= get_user(u, &(data32->src_width));
	err |= put_user(u, &(data->src_width));

	err |= get_user(u, &(data32->src_height));
	err |= put_user(u, &(data->src_height));

	err |= get_user(u, &(data32->tgt_offset_x));
	err |= put_user(u, &(data->tgt_offset_x));

	err |= get_user(u, &(data32->tgt_offset_y));
	err |= put_user(u, &(data->tgt_offset_y));

	err |= get_user(u, &(data32->tgt_width));
	err |= put_user(u, &(data->tgt_width));

	err |= get_user(u, &(data32->tgt_height));
	err |= put_user(u, &(data->tgt_height));

	err |= get_user(u, &(data32->layer_rotation));
	err |= put_user(u, &(data->layer_rotation));

	err |= get_user(u, &(data32->layer_type));
	err |= put_user(u, &(data->layer_type));

	err |= get_user(u, &(data32->video_rotation));
	err |= put_user(u, &(data->video_rotation));

	err |= get_user(u, &(data32->isTdshp));
	err |= put_user(u, &(data->isTdshp));

	err |= get_user(u, &(data32->next_buff_idx));
	err |= put_user(u, &(data->next_buff_idx));

	err |= get_user(i, &(data32->identity));
	err |= put_user(i, &(data->identity));

	err |= get_user(i, &(data32->connected_type));
	err |= put_user(i, &(data->connected_type));

	err |= get_user(u, &(data32->security));
	err |= put_user(u, &(data->security));

	err |= get_user(u, &(data32->alpha_enable));
	err |= put_user(u, &(data->alpha_enable));

	err |= get_user(u, &(data32->alpha));
	err |= put_user(u, &(data->alpha));

	err |= get_user(u, &(data32->sur_aen));
	err |= put_user(u, &(data->sur_aen));

	err |= get_user(u, &(data32->src_alpha));
	err |= put_user(u, &(data->src_alpha));

	err |= get_user(u, &(data32->dst_alpha));
	err |= put_user(u, &(data->dst_alpha));

	err |= get_user(i, &(data32->frm_sequence));
	err |= put_user(i, &(data->frm_sequence));

	err |= get_user(u, &(data32->yuv_range));
	err |= put_user(u, &(data->yuv_range));

	err |= get_user(u, &(data32->fps));
	err |= put_user(u, &(data->fps));

	err |= get_user(u, &(data32->timestamp));
	err |= put_user(u, &(data->timestamp));

	return err;
}

static int compat_get_disp_session_input_config(compat_disp_session_input_config __user *data32,
						disp_session_input_config __user *data)
{
	compat_uint_t u;
	compat_long_t l;
	int err = 0;
	int j;

	err |= get_user(u, &(data32->session_id));
	err |= put_user(u, &(data->session_id));

	err |= get_user(l, &(data32->config_layer_num));
	err |= put_user(l, &(data->config_layer_num));

	for (j = 0; j < ARRAY_SIZE(data32->config); j++)
		err |= compat_get_disp_input_config(&data32->config[j], &data->config[j]);

	return err;
}

int _compat_ioctl_set_input_buffer(struct file *file, unsigned long arg)
{
	int ret = 0;
	int err = 0;

	compat_disp_session_input_config __user *data32;

	disp_session_input_config __user *data;

	DISPDBG("COMPAT_DISP_IOCTL_SET_INPUT_BUFFER\n");
	data32 = compat_ptr(arg);
	data = compat_alloc_user_space(sizeof(disp_session_input_config));

	if (data == NULL) {
		DISPERR("compat_alloc_user_space fail!\n");
		return -EFAULT;
	}

	err = compat_get_disp_session_input_config(data32, data);
	if (err) {
		DISPERR("compat_get_disp_session_input_config fail!\n");
		return err;
	}

	ret = file->f_op->unlocked_ioctl(file, DISP_IOCTL_SET_INPUT_BUFFER, (unsigned long)data);
	return ret;
}

int _compat_ioctl_set_output_buffer(struct file *file, unsigned long arg)
{
	int ret = 0;
	int err = 0;

	compat_disp_session_output_config __user *data32;

	disp_session_output_config __user *data;

	DISPDBG("COMPAT_DISP_IOCTL_SET_OUTPUT_BUFFER\n");
	data32 = compat_ptr(arg);
	data = compat_alloc_user_space(sizeof(disp_session_output_config));

	if (data == NULL) {
		DISPERR("compat_alloc_user_space fail!\n");
		return -EFAULT;
	}

	err = compat_get_disp_session_output_config(data32, data);
	if (err) {
		DISPERR("compat_get_disp__session_output_config fail!\n");
		return err;
	}

	ret = file->f_op->unlocked_ioctl(file, DISP_IOCTL_SET_OUTPUT_BUFFER, (unsigned long)data);

	return ret;
}

#endif
