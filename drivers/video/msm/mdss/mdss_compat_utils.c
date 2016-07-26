/*
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 1994 Martin Schaller
 *
 * 2001 - Documented with DocBook
 * - Brad Douglas <brad@neruo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/compat.h>
#include <linux/fb.h>

#include <linux/uaccess.h>

#include "mdss_fb.h"
#include "mdss_compat_utils.h"
#include "mdss_mdp_hwio.h"
#include "mdss_mdp.h"

#define MSMFB_CURSOR32 _IOW(MSMFB_IOCTL_MAGIC, 130, struct fb_cursor32)
#define MSMFB_SET_LUT32 _IOW(MSMFB_IOCTL_MAGIC, 131, struct fb_cmap32)
#define MSMFB_HISTOGRAM32 _IOWR(MSMFB_IOCTL_MAGIC, 132,\
					struct mdp_histogram_data32)
#define MSMFB_GET_CCS_MATRIX32  _IOWR(MSMFB_IOCTL_MAGIC, 133, struct mdp_ccs32)
#define MSMFB_SET_CCS_MATRIX32  _IOW(MSMFB_IOCTL_MAGIC, 134, struct mdp_ccs32)
#define MSMFB_OVERLAY_SET32       _IOWR(MSMFB_IOCTL_MAGIC, 135,\
					struct mdp_overlay32)

#define MSMFB_OVERLAY_GET32      _IOR(MSMFB_IOCTL_MAGIC, 140,\
					struct mdp_overlay32)
#define MSMFB_OVERLAY_BLT32       _IOWR(MSMFB_IOCTL_MAGIC, 142,\
					struct msmfb_overlay_blt32)
#define MSMFB_HISTOGRAM_START32	_IOR(MSMFB_IOCTL_MAGIC, 144,\
					struct mdp_histogram_start_req32)

#define MSMFB_OVERLAY_3D32       _IOWR(MSMFB_IOCTL_MAGIC, 147,\
					struct msmfb_overlay_3d32)

#define MSMFB_MIXER_INFO32       _IOWR(MSMFB_IOCTL_MAGIC, 148,\
						struct msmfb_mixer_info_req32)
#define MSMFB_MDP_PP32 _IOWR(MSMFB_IOCTL_MAGIC, 156, struct msmfb_mdp_pp32)
#define MSMFB_BUFFER_SYNC32  _IOW(MSMFB_IOCTL_MAGIC, 162, struct mdp_buf_sync32)
#define MSMFB_OVERLAY_PREPARE32		_IOWR(MSMFB_IOCTL_MAGIC, 169, \
						struct mdp_overlay_list32)
#define MSMFB_ATOMIC_COMMIT32	_IOWR(MDP_IOCTL_MAGIC, 128, compat_caddr_t)

#define MSMFB_ASYNC_POSITION_UPDATE_32 _IOWR(MDP_IOCTL_MAGIC, 129, \
		struct mdp_position_update32)

static int __copy_layer_pp_info_params(struct mdp_input_layer *layer,
				struct mdp_input_layer32 *layer32);

static unsigned int __do_compat_ioctl_nr(unsigned int cmd32)
{
	unsigned int cmd;

	switch (cmd32) {
	case MSMFB_CURSOR32:
		cmd = MSMFB_CURSOR;
		break;
	case MSMFB_SET_LUT32:
		cmd = MSMFB_SET_LUT;
		break;
	case MSMFB_HISTOGRAM32:
		cmd = MSMFB_HISTOGRAM;
		break;
	case MSMFB_GET_CCS_MATRIX32:
		cmd = MSMFB_GET_CCS_MATRIX;
		break;
	case MSMFB_SET_CCS_MATRIX32:
		cmd = MSMFB_SET_CCS_MATRIX;
		break;
	case MSMFB_OVERLAY_SET32:
		cmd = MSMFB_OVERLAY_SET;
		break;
	case MSMFB_OVERLAY_GET32:
		cmd = MSMFB_OVERLAY_GET;
		break;
	case MSMFB_OVERLAY_BLT32:
		cmd = MSMFB_OVERLAY_BLT;
		break;
	case MSMFB_OVERLAY_3D32:
		cmd = MSMFB_OVERLAY_3D;
		break;
	case MSMFB_MIXER_INFO32:
		cmd = MSMFB_MIXER_INFO;
		break;
	case MSMFB_MDP_PP32:
		cmd = MSMFB_MDP_PP;
		break;
	case MSMFB_BUFFER_SYNC32:
		cmd = MSMFB_BUFFER_SYNC;
		break;
	case MSMFB_OVERLAY_PREPARE32:
		cmd = MSMFB_OVERLAY_PREPARE;
		break;
	case MSMFB_ATOMIC_COMMIT32:
		cmd = MSMFB_ATOMIC_COMMIT;
		break;
	case MSMFB_ASYNC_POSITION_UPDATE_32:
		cmd = MSMFB_ASYNC_POSITION_UPDATE;
		break;
	default:
		cmd = cmd32;
		break;
	}

	return cmd;
}

static void  __copy_atomic_commit_struct(struct mdp_layer_commit  *commit,
	struct mdp_layer_commit32 *commit32)
{
	commit->version = commit32->version;
	commit->commit_v1.flags = commit32->commit_v1.flags;
	commit->commit_v1.input_layer_cnt =
		commit32->commit_v1.input_layer_cnt;
	commit->commit_v1.left_roi = commit32->commit_v1.left_roi;
	commit->commit_v1.right_roi = commit32->commit_v1.right_roi;
	memcpy(&commit->commit_v1.reserved, &commit32->commit_v1.reserved,
		sizeof(commit32->commit_v1.reserved));
}

static struct mdp_input_layer32 *__create_layer_list32(
	struct mdp_layer_commit32 *commit32,
	u32 layer_count)
{
	u32 buffer_size32;
	struct mdp_input_layer32 *layer_list32;
	int ret;

	buffer_size32 = sizeof(struct mdp_input_layer32) * layer_count;

	layer_list32 = kmalloc(buffer_size32, GFP_KERNEL);
	if (!layer_list32) {
		pr_err("unable to allocate memory for layers32\n");
		layer_list32 = ERR_PTR(-ENOMEM);
		goto end;
	}

	ret = copy_from_user(layer_list32,
			compat_ptr(commit32->commit_v1.input_layers),
			sizeof(struct mdp_input_layer32) * layer_count);
	if (ret) {
		pr_err("layer list32 copy from user failed, ptr %p\n",
			compat_ptr(commit32->commit_v1.input_layers));
		kfree(layer_list32);
		ret = -EFAULT;
		layer_list32 = ERR_PTR(ret);
	}

end:
	return layer_list32;
}

static int __copy_scale_params(struct mdp_input_layer *layer,
	struct mdp_input_layer32 *layer32)
{
	struct mdp_scale_data *scale;
	int ret;

	if (!(layer->flags & MDP_LAYER_ENABLE_PIXEL_EXT))
		return 0;

	scale = kmalloc(sizeof(struct mdp_scale_data), GFP_KERNEL);
	if (!scale) {
		pr_err("unable to allocate memory for scale param\n");
		ret = -ENOMEM;
		goto end;
	}

	/* scale structure size is same for compat and 64bit version */
	ret = copy_from_user(scale, compat_ptr(layer32->scale),
			sizeof(struct mdp_scale_data));
	if (ret) {
		kfree(scale);
		pr_err("scale param copy from user failed, ptr %p\n",
			compat_ptr(layer32->scale));
		ret = -EFAULT;
	} else {
		layer->scale = scale;
	}
end:
	return ret;
}

static struct mdp_input_layer *__create_layer_list(
	struct mdp_layer_commit *commit,
	struct mdp_input_layer32 *layer_list32,
	u32 layer_count)
{
	int i, ret;
	u32 buffer_size;
	struct mdp_input_layer *layer, *layer_list;
	struct mdp_input_layer32 *layer32;

	buffer_size = sizeof(struct mdp_input_layer) * layer_count;

	layer_list = kmalloc(buffer_size, GFP_KERNEL);
	if (!layer_list) {
		pr_err("unable to allocate memory for layers32\n");
		layer_list = ERR_PTR(-ENOMEM);
		goto end;
	}

	commit->commit_v1.input_layers = layer_list;

	for (i = 0; i < layer_count; i++) {
		layer = &layer_list[i];
		layer32 = &layer_list32[i];

		layer->flags = layer32->flags;
		layer->pipe_ndx = layer32->pipe_ndx;
		layer->horz_deci = layer32->horz_deci;
		layer->vert_deci = layer32->vert_deci;
		layer->z_order = layer32->z_order;
		layer->transp_mask = layer32->transp_mask;
		layer->bg_color = layer32->bg_color;
		layer->blend_op = layer32->blend_op;
		layer->src_rect = layer32->src_rect;
		layer->dst_rect = layer32->dst_rect;
		layer->buffer = layer32->buffer;
		memcpy(&layer->reserved, &layer32->reserved,
			sizeof(layer->reserved));

		layer->scale = NULL;
		ret = __copy_scale_params(layer, layer32);
		if (ret)
			break;

		layer->pp_info = NULL;
		ret = __copy_layer_pp_info_params(layer, layer32);
		if (ret)
			break;
	}

	if (ret) {
		for (i--; i >= 0; i--) {
			kfree(layer_list[i].scale);
			mdss_mdp_free_layer_pp_info(&layer_list[i]);
		}
		kfree(layer_list);
		layer_list = ERR_PTR(ret);
	}

end:
	return layer_list;
}

static int __copy_to_user_atomic_commit(struct mdp_layer_commit  *commit,
	struct mdp_layer_commit32 *commit32,
	struct mdp_input_layer32 *layer_list32,
	unsigned long argp, u32 layer_count)
{
	int i, ret;
	struct mdp_input_layer *layer_list;

	layer_list = commit->commit_v1.input_layers;

	for (i = 0; i < layer_count; i++)
		layer_list32[i].error_code = layer_list[i].error_code;

	ret = copy_to_user(compat_ptr(commit32->commit_v1.input_layers),
		layer_list32,
		sizeof(struct mdp_input_layer32) * layer_count);
	if (ret)
		goto end;

	ret = copy_to_user(compat_ptr(commit32->commit_v1.output_layer),
		commit->commit_v1.output_layer,
		sizeof(struct mdp_output_layer));
	if (ret)
		goto end;

	commit32->commit_v1.release_fence =
		commit->commit_v1.release_fence;
	commit32->commit_v1.retire_fence =
		commit->commit_v1.retire_fence;

	ret = copy_to_user((void __user *)argp, commit32,
		sizeof(struct mdp_layer_commit32));

end:
	return ret;
}

static int __compat_atomic_commit(struct fb_info *info, unsigned int cmd,
			 unsigned long argp, struct file *file)
{
	int ret, i;
	struct mdp_layer_commit  commit;
	struct mdp_layer_commit32 commit32;
	u32 layer_count;
	struct mdp_input_layer *layer_list = NULL;
	struct mdp_input_layer32 *layer_list32 = NULL;
	struct mdp_output_layer *output_layer = NULL;
	struct mdp_frc_info *frc_info = NULL;

	/* copy top level memory from 32 bit structure to kernel memory */
	ret = copy_from_user(&commit32, (void __user *)argp,
		sizeof(struct mdp_layer_commit32));
	if (ret) {
		pr_err("%s:copy_from_user failed, ptr %p\n", __func__,
			(void __user *)argp);
		ret = -EFAULT;
		return ret;
	}
	__copy_atomic_commit_struct(&commit, &commit32);

	if (commit32.commit_v1.output_layer) {
		int buffer_size = sizeof(struct mdp_output_layer);
		output_layer = kzalloc(buffer_size, GFP_KERNEL);
		if (!output_layer) {
			pr_err("fail to allocate output layer\n");
			return -ENOMEM;
		}
		ret = copy_from_user(output_layer,
				compat_ptr(commit32.commit_v1.output_layer),
				buffer_size);
		if (ret) {
			pr_err("fail to copy output layer from user, ptr %p\n",
				compat_ptr(commit32.commit_v1.output_layer));
			ret = -EFAULT;
			goto layer_list_err;
		}

		commit.commit_v1.output_layer = output_layer;
	}

	layer_count = commit32.commit_v1.input_layer_cnt;
	if (layer_count > MAX_LAYER_COUNT) {
		ret = -EINVAL;
		goto layer_list_err;
	} else if (layer_count) {
		/*
		 * allocate memory for layer list in 32bit domain and copy it
		 * from user
		 */
		layer_list32 = __create_layer_list32(&commit32, layer_count);
		if (IS_ERR_OR_NULL(layer_list32)) {
			ret = PTR_ERR(layer_list32);
			goto layer_list_err;
		}

		/*
		 * allocate memory for layer list in kernel memory domain and
		 * copy layer info from 32bit structures to kernel memory
		 */
		layer_list = __create_layer_list(&commit, layer_list32,
			layer_count);
		if (IS_ERR_OR_NULL(layer_list)) {
			ret = PTR_ERR(layer_list);
			goto layer_list_err;
		}
	}

	if (commit32.commit_v1.frc_info) {
		int buffer_size = sizeof(struct mdp_frc_info);

		frc_info = kzalloc(buffer_size, GFP_KERNEL);
		if (!frc_info) {
			ret = -ENOMEM;
			goto frc_err;
		}

		ret = copy_from_user(frc_info,
				compat_ptr(commit32.commit_v1.frc_info),
				buffer_size);
		if (ret) {
			pr_err("fail to copy frc info from user, ptr %p\n",
				compat_ptr(commit32.commit_v1.frc_info));
			kfree(frc_info);
			ret = -EFAULT;
			goto frc_err;
		}

		commit.commit_v1.frc_info = frc_info;
	}

	ret = mdss_fb_atomic_commit(info, &commit, file);
	if (ret)
		pr_err("atomic commit failed ret:%d\n", ret);

	if (layer_count)
		__copy_to_user_atomic_commit(&commit, &commit32, layer_list32,
			argp, layer_count);

	for (i = 0; i < layer_count; i++) {
		kfree(layer_list[i].scale);
		mdss_mdp_free_layer_pp_info(&layer_list[i]);
	}

	kfree(frc_info);
frc_err:
	kfree(layer_list);
layer_list_err:
	kfree(layer_list32);
	kfree(output_layer);
	return ret;
}

static int __copy_to_user_async_position_update(
		struct mdp_position_update *update_pos,
		struct mdp_position_update32 *update_pos32,
		unsigned long argp, u32 layer_cnt)
{
	int ret;

	ret = copy_to_user(update_pos32->input_layers,
			update_pos->input_layers,
			sizeof(struct mdp_async_layer) * layer_cnt);
	if (ret)
		goto end;

	ret = copy_to_user((void __user *) argp, update_pos32,
			sizeof(struct mdp_position_update32));

end:
	return ret;
}

static struct mdp_async_layer *__create_async_layer_list(
	struct mdp_position_update32 *update_pos32, u32 layer_cnt)
{
	u32 buffer_size;
	struct mdp_async_layer *layer_list;
	int ret;

	buffer_size = sizeof(struct mdp_async_layer) * layer_cnt;

	layer_list = kmalloc(buffer_size, GFP_KERNEL);
	if (!layer_list) {
		layer_list = ERR_PTR(-ENOMEM);
		goto end;
	}

	ret = copy_from_user(layer_list,
			update_pos32->input_layers, buffer_size);
	if (ret) {
		pr_err("layer list32 copy from user failed\n");
		kfree(layer_list);
		layer_list = ERR_PTR(ret);
	}

end:
	return layer_list;
}

static int __compat_async_position_update(struct fb_info *info,
		unsigned int cmd, unsigned long argp)
{
	struct mdp_position_update update_pos;
	struct mdp_position_update32 update_pos32;
	struct mdp_async_layer *layer_list = NULL;
	u32 layer_cnt, ret;

	/* copy top level memory from 32 bit structure to kernel memory */
	ret = copy_from_user(&update_pos32, (void __user *)argp,
		sizeof(struct mdp_position_update32));
	if (ret) {
		pr_err("%s:copy_from_user failed\n", __func__);
		return ret;
	}

	update_pos.input_layer_cnt = update_pos32.input_layer_cnt;
	layer_cnt = update_pos32.input_layer_cnt;
	if ((!layer_cnt) || (layer_cnt > MAX_LAYER_COUNT)) {
		pr_err("invalid async layers :%d to update\n", layer_cnt);
		return -EINVAL;
	}

	layer_list = __create_async_layer_list(&update_pos32,
		layer_cnt);
	if (IS_ERR_OR_NULL(layer_list))
		return PTR_ERR(layer_list);

	update_pos.input_layers = layer_list;

	ret = mdss_fb_async_position_update(info, &update_pos);
	if (ret)
		pr_err("async position update failed ret:%d\n", ret);

	ret = __copy_to_user_async_position_update(&update_pos, &update_pos32,
			argp, layer_cnt);
	if (ret)
		pr_err("copy to user of async update position failed\n");

	kfree(layer_list);
	return ret;
}

static int mdss_fb_compat_buf_sync(struct fb_info *info, unsigned int cmd,
			 unsigned long arg, struct file *file)
{
	struct mdp_buf_sync32 __user *buf_sync32;
	struct mdp_buf_sync __user *buf_sync;
	u32 data;
	int ret;

	buf_sync = compat_alloc_user_space(sizeof(*buf_sync));
	if (!buf_sync) {
		pr_err("%s:%u: compat alloc error [%zu] bytes\n",
			 __func__, __LINE__, sizeof(*buf_sync));
		return -EINVAL;
	}
	buf_sync32 = compat_ptr(arg);

	if (copy_in_user(&buf_sync->flags, &buf_sync32->flags,
			 3 * sizeof(u32)))
		return -EFAULT;

	if (get_user(data, &buf_sync32->acq_fen_fd) ||
	    put_user(compat_ptr(data), &buf_sync->acq_fen_fd) ||
	    get_user(data, &buf_sync32->rel_fen_fd) ||
	    put_user(compat_ptr(data), &buf_sync->rel_fen_fd) ||
	    get_user(data, &buf_sync32->retire_fen_fd) ||
	    put_user(compat_ptr(data), &buf_sync->retire_fen_fd))
		return -EFAULT;

	ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) buf_sync, file);
	if (ret) {
		pr_err("%s: failed %d\n", __func__, ret);
		return ret;
	}

	if (copy_in_user(compat_ptr(buf_sync32->rel_fen_fd),
			buf_sync->rel_fen_fd,
			sizeof(int)))
		return -EFAULT;
	if (copy_in_user(compat_ptr(buf_sync32->retire_fen_fd),
			buf_sync->retire_fen_fd,
			sizeof(int))) {
		if (buf_sync->flags & MDP_BUF_SYNC_FLAG_RETIRE_FENCE)
			return -EFAULT;
		else
			pr_debug("%s: no retire fence fd for wb\n",
				__func__);
	}

	return ret;
}

static int __from_user_fb_cmap(struct fb_cmap __user *cmap,
				struct fb_cmap32 __user *cmap32)
{
	__u32 data;

	if (copy_in_user(&cmap->start, &cmap32->start, 2 * sizeof(__u32)))
		return -EFAULT;

	if (get_user(data, &cmap32->red) ||
	    put_user(compat_ptr(data), &cmap->red) ||
	    get_user(data, &cmap32->green) ||
	    put_user(compat_ptr(data), &cmap->green) ||
	    get_user(data, &cmap32->blue) ||
	    put_user(compat_ptr(data), &cmap->blue) ||
	    get_user(data, &cmap32->transp) ||
	    put_user(compat_ptr(data), &cmap->transp))
		return -EFAULT;

	return 0;
}

static int __to_user_fb_cmap(struct fb_cmap __user *cmap,
				struct fb_cmap32 __user *cmap32)
{
	unsigned long data;

	if (copy_in_user(&cmap32->start, &cmap->start, 2 * sizeof(__u32)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &cmap->red) ||
	    put_user((compat_caddr_t) data, &cmap32->red) ||
	    get_user(data, (unsigned long *) &cmap->green) ||
	    put_user((compat_caddr_t) data, &cmap32->green) ||
	    get_user(data, (unsigned long *) &cmap->blue) ||
	    put_user((compat_caddr_t) data, &cmap32->blue) ||
	    get_user(data, (unsigned long *) &cmap->transp) ||
	    put_user((compat_caddr_t) data, &cmap32->transp))
		return -EFAULT;

	return 0;
}

static int __from_user_fb_image(struct fb_image __user *image,
				struct fb_image32 __user *image32)
{
	__u32 data;

	if (copy_in_user(&image->dx, &image32->dx, 6 * sizeof(u32)) ||
		copy_in_user(&image->depth, &image32->depth, sizeof(u8)))
		return -EFAULT;

	if (get_user(data, &image32->data) ||
		put_user(compat_ptr(data), &image->data))
		return -EFAULT;

	if (__from_user_fb_cmap(&image->cmap, &image32->cmap))
		return -EFAULT;

	return 0;
}

static int mdss_fb_compat_cursor(struct fb_info *info, unsigned int cmd,
			unsigned long arg, struct file *file)
{
	struct fb_cursor32 __user *cursor32;
	struct fb_cursor __user *cursor;
	__u32 data;
	int ret;

	cursor = compat_alloc_user_space(sizeof(*cursor));
	if (!cursor) {
		pr_err("%s:%u: compat alloc error [%zu] bytes\n",
			 __func__, __LINE__, sizeof(*cursor));
		return -EINVAL;
	}
	cursor32 = compat_ptr(arg);

	if (copy_in_user(&cursor->set, &cursor32->set, 3 * sizeof(u16)))
		return -EFAULT;

	if (get_user(data, &cursor32->mask) ||
			put_user(compat_ptr(data), &cursor->mask))
		return -EFAULT;

	if (copy_in_user(&cursor->hot, &cursor32->hot, sizeof(struct fbcurpos)))
		return -EFAULT;

	if (__from_user_fb_image(&cursor->image, &cursor32->image))
		return -EFAULT;

	ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) cursor, file);
	return ret;
}

static int mdss_fb_compat_set_lut(struct fb_info *info, unsigned long arg,
	struct file *file)
{
	struct fb_cmap_user __user *cmap;
	struct fb_cmap32 __user *cmap32;
	__u32 data;
	int ret;

	cmap = compat_alloc_user_space(sizeof(*cmap));
	cmap32 = compat_ptr(arg);

	if (copy_in_user(&cmap->start, &cmap32->start, 2 * sizeof(__u32)))
		return -EFAULT;

	if (get_user(data, &cmap32->red) ||
	    put_user(compat_ptr(data), &cmap->red) ||
	    get_user(data, &cmap32->green) ||
	    put_user(compat_ptr(data), &cmap->green) ||
	    get_user(data, &cmap32->blue) ||
	    put_user(compat_ptr(data), &cmap->blue) ||
	    get_user(data, &cmap32->transp) ||
	    put_user(compat_ptr(data), &cmap->transp))
		return -EFAULT;

	ret = mdss_fb_do_ioctl(info, MSMFB_SET_LUT, (unsigned long) cmap, file);
	if (!ret)
		pr_debug("%s: compat ioctl successful\n", __func__);

	return ret;
}

static int __from_user_sharp_cfg(
			struct mdp_sharp_cfg32 __user *sharp_cfg32,
			struct mdp_sharp_cfg __user *sharp_cfg)
{
	if (copy_in_user(&sharp_cfg->flags,
			&sharp_cfg32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg->strength,
			&sharp_cfg32->strength,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg->edge_thr,
			&sharp_cfg32->edge_thr,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg->smooth_thr,
			&sharp_cfg32->smooth_thr,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg->noise_thr,
			&sharp_cfg32->noise_thr,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_sharp_cfg(
			struct mdp_sharp_cfg32 __user *sharp_cfg32,
			struct mdp_sharp_cfg __user *sharp_cfg)
{
	if (copy_in_user(&sharp_cfg32->flags,
			&sharp_cfg->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg32->strength,
			&sharp_cfg->strength,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg32->edge_thr,
			&sharp_cfg->edge_thr,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg32->smooth_thr,
			&sharp_cfg->smooth_thr,
			sizeof(uint32_t)) ||
	    copy_in_user(&sharp_cfg32->noise_thr,
			&sharp_cfg->noise_thr,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_histogram_cfg(
			struct mdp_histogram_cfg32 __user *hist_cfg32,
			struct mdp_histogram_cfg __user *hist_cfg)
{
	if (copy_in_user(&hist_cfg->ops,
			&hist_cfg32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_cfg->block,
			&hist_cfg32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_cfg->frame_cnt,
			&hist_cfg32->frame_cnt,
			sizeof(uint8_t)) ||
	    copy_in_user(&hist_cfg->bit_mask,
			&hist_cfg32->bit_mask,
			sizeof(uint8_t)) ||
	    copy_in_user(&hist_cfg->num_bins,
			&hist_cfg32->num_bins,
			sizeof(uint16_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_histogram_cfg(
			struct mdp_histogram_cfg32 __user *hist_cfg32,
			struct mdp_histogram_cfg __user *hist_cfg)
{
	if (copy_in_user(&hist_cfg32->ops,
			&hist_cfg->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_cfg32->block,
			&hist_cfg->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_cfg32->frame_cnt,
			&hist_cfg->frame_cnt,
			sizeof(uint8_t)) ||
	    copy_in_user(&hist_cfg32->bit_mask,
			&hist_cfg->bit_mask,
			sizeof(uint8_t)) ||
	    copy_in_user(&hist_cfg32->num_bins,
			&hist_cfg->num_bins,
			sizeof(uint16_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_pcc_coeff(
			struct mdp_pcc_coeff32 __user *pcc_coeff32,
			struct mdp_pcc_coeff __user *pcc_coeff)
{
	if (copy_in_user(&pcc_coeff->c,
			&pcc_coeff32->c,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->r,
			&pcc_coeff32->r,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->g,
			&pcc_coeff32->g,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->b,
			&pcc_coeff32->b,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->rr,
			&pcc_coeff32->rr,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->gg,
			&pcc_coeff32->gg,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->bb,
			&pcc_coeff32->bb,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->rg,
			&pcc_coeff32->rg,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->gb,
			&pcc_coeff32->gb,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->rb,
			&pcc_coeff32->rb,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->rgb_0,
			&pcc_coeff32->rgb_0,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff->rgb_1,
			&pcc_coeff32->rgb_1,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_pcc_coeff(
			struct mdp_pcc_coeff32 __user *pcc_coeff32,
			struct mdp_pcc_coeff __user *pcc_coeff)
{
	if (copy_in_user(&pcc_coeff32->c,
			&pcc_coeff->c,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->r,
			&pcc_coeff->r,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->g,
			&pcc_coeff->g,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->b,
			&pcc_coeff->b,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->rr,
			&pcc_coeff->rr,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->gg,
			&pcc_coeff->gg,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->bb,
			&pcc_coeff->bb,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->rg,
			&pcc_coeff->rg,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->gb,
			&pcc_coeff->gb,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->rb,
			&pcc_coeff->rb,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->rgb_0,
			&pcc_coeff->rgb_0,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_coeff32->rgb_1,
			&pcc_coeff->rgb_1,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_pcc_coeff_v17(
			struct mdp_pcc_cfg_data32 __user *pcc_cfg32,
			struct mdp_pcc_cfg_data __user *pcc_cfg)
{
	struct mdp_pcc_data_v1_7_32 pcc_cfg_payload32;
	struct mdp_pcc_data_v1_7 pcc_cfg_payload;

	if (copy_from_user(&pcc_cfg_payload32,
			   compat_ptr(pcc_cfg32->cfg_payload),
			   sizeof(struct mdp_pcc_data_v1_7_32))) {
		pr_err("failed to copy payload for pcc from user\n");
		return -EFAULT;
	}

	pcc_cfg_payload.r.b = pcc_cfg_payload32.r.b;
	pcc_cfg_payload.r.g = pcc_cfg_payload32.r.g;
	pcc_cfg_payload.r.c = pcc_cfg_payload32.r.c;
	pcc_cfg_payload.r.r = pcc_cfg_payload32.r.r;
	pcc_cfg_payload.r.gb = pcc_cfg_payload32.r.gb;
	pcc_cfg_payload.r.rb = pcc_cfg_payload32.r.rb;
	pcc_cfg_payload.r.rg = pcc_cfg_payload32.r.rg;
	pcc_cfg_payload.r.rgb = pcc_cfg_payload32.r.rgb;

	pcc_cfg_payload.g.b = pcc_cfg_payload32.g.b;
	pcc_cfg_payload.g.g = pcc_cfg_payload32.g.g;
	pcc_cfg_payload.g.c = pcc_cfg_payload32.g.c;
	pcc_cfg_payload.g.r = pcc_cfg_payload32.g.r;
	pcc_cfg_payload.g.gb = pcc_cfg_payload32.g.gb;
	pcc_cfg_payload.g.rb = pcc_cfg_payload32.g.rb;
	pcc_cfg_payload.g.rg = pcc_cfg_payload32.g.rg;
	pcc_cfg_payload.g.rgb = pcc_cfg_payload32.g.rgb;

	pcc_cfg_payload.b.b = pcc_cfg_payload32.b.b;
	pcc_cfg_payload.b.g = pcc_cfg_payload32.b.g;
	pcc_cfg_payload.b.c = pcc_cfg_payload32.b.c;
	pcc_cfg_payload.b.r = pcc_cfg_payload32.b.r;
	pcc_cfg_payload.b.gb = pcc_cfg_payload32.b.gb;
	pcc_cfg_payload.b.rb = pcc_cfg_payload32.b.rb;
	pcc_cfg_payload.b.rg = pcc_cfg_payload32.b.rg;
	pcc_cfg_payload.b.rgb = pcc_cfg_payload32.b.rgb;

	if (copy_to_user(pcc_cfg->cfg_payload, &pcc_cfg_payload,
			 sizeof(pcc_cfg_payload))) {
		pr_err("failed to copy payload for pcc to user\n");
		return -EFAULT;
	}
	return 0;
}

static int __from_user_pcc_cfg_data(
			struct mdp_pcc_cfg_data32 __user *pcc_cfg32,
			struct mdp_pcc_cfg_data __user *pcc_cfg)
{
	u32 version;

	if (copy_in_user(&pcc_cfg->block,
			&pcc_cfg32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_cfg->ops,
			&pcc_cfg32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_cfg->version,
			&pcc_cfg32->version,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_from_user(&version, &pcc_cfg32->version, sizeof(u32))) {
		pr_err("failed to copy version for pcc\n");
		return -EFAULT;
	}

	switch (version) {
	case mdp_pcc_v1_7:
		if (__from_user_pcc_coeff_v17(pcc_cfg32, pcc_cfg)) {
			pr_err("failed to copy pcc v17 data\n");
			return -EFAULT;
		}
		break;
	default:
		pr_debug("pcc version %d not supported use legacy\n", version);
		if (__from_user_pcc_coeff(
				compat_ptr((uintptr_t)&pcc_cfg32->r),
				&pcc_cfg->r) ||
		    __from_user_pcc_coeff(
				compat_ptr((uintptr_t)&pcc_cfg32->g),
				&pcc_cfg->g) ||
		    __from_user_pcc_coeff(
				compat_ptr((uintptr_t)&pcc_cfg32->b),
				&pcc_cfg->b))
			return -EFAULT;
		break;
	}
	return 0;
}

static int __to_user_pcc_coeff_v1_7(
			struct mdp_pcc_cfg_data32 __user *pcc_cfg32,
			struct mdp_pcc_cfg_data __user *pcc_cfg)
{
	struct mdp_pcc_data_v1_7_32 pcc_cfg_payload32;
	struct mdp_pcc_data_v1_7 pcc_cfg_payload;

	if (copy_from_user(&pcc_cfg_payload,
			   pcc_cfg->cfg_payload,
			   sizeof(struct mdp_pcc_data_v1_7))) {
		pr_err("failed to copy payload for pcc from user\n");
		return -EFAULT;
	}

	pcc_cfg_payload32.r.b = pcc_cfg_payload.r.b;
	pcc_cfg_payload32.r.g = pcc_cfg_payload.r.g;
	pcc_cfg_payload32.r.c = pcc_cfg_payload.r.c;
	pcc_cfg_payload32.r.r = pcc_cfg_payload.r.r;
	pcc_cfg_payload32.r.gb = pcc_cfg_payload.r.gb;
	pcc_cfg_payload32.r.rb = pcc_cfg_payload.r.rb;
	pcc_cfg_payload32.r.rg = pcc_cfg_payload.r.rg;
	pcc_cfg_payload32.r.rgb = pcc_cfg_payload.r.rgb;

	pcc_cfg_payload32.g.b = pcc_cfg_payload.g.b;
	pcc_cfg_payload32.g.g = pcc_cfg_payload.g.g;
	pcc_cfg_payload32.g.c = pcc_cfg_payload.g.c;
	pcc_cfg_payload32.g.r = pcc_cfg_payload.g.r;
	pcc_cfg_payload32.g.gb = pcc_cfg_payload.g.gb;
	pcc_cfg_payload32.g.rb = pcc_cfg_payload.g.rb;
	pcc_cfg_payload32.g.rg = pcc_cfg_payload.g.rg;
	pcc_cfg_payload32.g.rgb = pcc_cfg_payload.g.rgb;

	pcc_cfg_payload32.b.b = pcc_cfg_payload.b.b;
	pcc_cfg_payload32.b.g = pcc_cfg_payload.b.g;
	pcc_cfg_payload32.b.c = pcc_cfg_payload.b.c;
	pcc_cfg_payload32.b.r = pcc_cfg_payload.b.r;
	pcc_cfg_payload32.b.gb = pcc_cfg_payload.b.gb;
	pcc_cfg_payload32.b.rb = pcc_cfg_payload.b.rb;
	pcc_cfg_payload32.b.rg = pcc_cfg_payload.b.rg;
	pcc_cfg_payload32.b.rgb = pcc_cfg_payload.b.rgb;

	if (copy_to_user(compat_ptr(pcc_cfg32->cfg_payload),
			 &pcc_cfg_payload32,
			 sizeof(pcc_cfg_payload32))) {
		pr_err("failed to copy payload for pcc to user\n");
		return -EFAULT;
	}

	return 0;
}


static int __to_user_pcc_cfg_data(
			struct mdp_pcc_cfg_data32 __user *pcc_cfg32,
			struct mdp_pcc_cfg_data __user *pcc_cfg)
{
	u32 version;
	u32 ops;

	if (copy_from_user(&ops, &pcc_cfg->ops, sizeof(u32))) {
		pr_err("failed to copy op for pcc\n");
		return -EFAULT;
	}

	if (!(ops & MDP_PP_OPS_READ)) {
		pr_debug("Read op is not set. Skipping compat copyback\n");
		return 0;
	}

	if (copy_from_user(&version, &pcc_cfg->version, sizeof(u32))) {
		pr_err("failed to copy version for pcc\n");
		return -EFAULT;
	}

	switch (version) {
	case mdp_pcc_v1_7:
		if (__to_user_pcc_coeff_v1_7(pcc_cfg32, pcc_cfg)) {
			pr_err("failed to copy pcc v1_7 data\n");
			return -EFAULT;
		}
		break;
	default:
		pr_debug("version invalid, fallback to legacy\n");

		if (__to_user_pcc_coeff(
				compat_ptr((uintptr_t)&pcc_cfg32->r),
				&pcc_cfg->r) ||
		    __to_user_pcc_coeff(
				compat_ptr((uintptr_t)&pcc_cfg32->g),
				&pcc_cfg->g) ||
		    __to_user_pcc_coeff(
				compat_ptr((uintptr_t)&pcc_cfg32->b),
				&pcc_cfg->b))
			return -EFAULT;
		break;
	}

	return 0;
}

static int __from_user_csc_cfg(
			struct mdp_csc_cfg32 __user *csc_data32,
			struct mdp_csc_cfg __user *csc_data)
{
	if (copy_in_user(&csc_data->flags,
			&csc_data32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&csc_data->csc_mv[0],
			&csc_data32->csc_mv[0],
			9 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data->csc_pre_bv[0],
			&csc_data32->csc_pre_bv[0],
			3 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data->csc_post_bv[0],
			&csc_data32->csc_post_bv[0],
			3 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data->csc_pre_lv[0],
			&csc_data32->csc_pre_lv[0],
			6 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data->csc_post_lv[0],
			&csc_data32->csc_post_lv[0],
			6 * sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}
static int __to_user_csc_cfg(
			struct mdp_csc_cfg32 __user *csc_data32,
			struct mdp_csc_cfg __user *csc_data)
{
	if (copy_in_user(&csc_data32->flags,
			&csc_data->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&csc_data32->csc_mv[0],
			&csc_data->csc_mv[0],
			9 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data32->csc_pre_bv[0],
			&csc_data->csc_pre_bv[0],
			3 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data32->csc_post_bv[0],
			&csc_data->csc_post_bv[0],
			3 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data32->csc_pre_lv[0],
			&csc_data->csc_pre_lv[0],
			6 * sizeof(uint32_t)) ||
	    copy_in_user(&csc_data32->csc_post_lv[0],
			&csc_data->csc_post_lv[0],
			6 * sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_csc_cfg_data(
			struct mdp_csc_cfg_data32 __user *csc_cfg32,
			struct mdp_csc_cfg_data __user *csc_cfg)
{
	if (copy_in_user(&csc_cfg->block,
			&csc_cfg32->block,
			sizeof(uint32_t)))
		return -EFAULT;

	if (__from_user_csc_cfg(
			compat_ptr((uintptr_t)&csc_cfg32->csc_data),
			&csc_cfg->csc_data))
		return -EFAULT;

	return 0;
}

static int __to_user_csc_cfg_data(
			struct mdp_csc_cfg_data32 __user *csc_cfg32,
			struct mdp_csc_cfg_data __user *csc_cfg)
{
	if (copy_in_user(&csc_cfg32->block,
			&csc_cfg->block,
			sizeof(uint32_t)))
		return -EFAULT;

	if (__to_user_csc_cfg(
			compat_ptr((uintptr_t)&csc_cfg32->csc_data),
			&csc_cfg->csc_data))
		return -EFAULT;

	return 0;
}

static int __from_user_igc_lut_data_v17(
		struct mdp_igc_lut_data32 __user *igc_lut32,
		struct mdp_igc_lut_data __user *igc_lut)
{
	struct mdp_igc_lut_data_v1_7_32 igc_cfg_payload_32;
	struct mdp_igc_lut_data_v1_7 igc_cfg_payload;

	if (copy_from_user(&igc_cfg_payload_32,
			   compat_ptr(igc_lut32->cfg_payload),
			   sizeof(igc_cfg_payload_32))) {
		pr_err("failed to copy payload from user for igc\n");
		return -EFAULT;
	}
	igc_cfg_payload.c0_c1_data = compat_ptr(igc_cfg_payload_32.c0_c1_data);
	igc_cfg_payload.c2_data = compat_ptr(igc_cfg_payload_32.c2_data);
	igc_cfg_payload.len = igc_cfg_payload_32.len;
	igc_cfg_payload.table_fmt = igc_cfg_payload_32.table_fmt;
	if (copy_to_user(igc_lut->cfg_payload, &igc_cfg_payload,
			 sizeof(igc_cfg_payload))) {
		pr_err("failed to copy payload to user for igc\n");
		return -EFAULT;
	}
	return 0;
}

static int __from_user_igc_lut_data(
		struct mdp_igc_lut_data32 __user *igc_lut32,
		struct mdp_igc_lut_data __user *igc_lut)
{
	uint32_t data;
	uint32_t version = mdp_igc_vmax;
	int ret = 0;

	if (copy_in_user(&igc_lut->block,
			&igc_lut32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut->len,
			&igc_lut32->len,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut->ops,
			&igc_lut32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut->version,
			&igc_lut32->version,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(version, &igc_lut32->version)) {
		pr_err("failed to copy the version for IGC\n");
		return -EFAULT;
	}

	switch (version) {
	case mdp_igc_v1_7:
		ret = __from_user_igc_lut_data_v17(igc_lut32, igc_lut);
		if (ret)
			pr_err("failed to copy payload for igc version %d ret %d\n",
				version, ret);
		break;
	default:
		pr_debug("version not supported fallback to legacy %d\n",
			 version);
		if (get_user(data, &igc_lut32->c0_c1_data) ||
		    put_user(compat_ptr(data), &igc_lut->c0_c1_data) ||
		    get_user(data, &igc_lut32->c2_data) ||
		    put_user(compat_ptr(data), &igc_lut->c2_data))
			return -EFAULT;
		break;
	}
	return ret;
}

static int __to_user_igc_lut_data(
		struct mdp_igc_lut_data32 __user *igc_lut32,
		struct mdp_igc_lut_data __user *igc_lut)
{
	unsigned long data;

	if (copy_in_user(&igc_lut32->block,
			&igc_lut->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut32->len,
			&igc_lut->len,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut32->ops,
			&igc_lut->ops,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &igc_lut->c0_c1_data) ||
	    put_user((compat_caddr_t) data, &igc_lut32->c0_c1_data) ||
	    get_user(data, (unsigned long *) &igc_lut->c2_data) ||
	    put_user((compat_caddr_t) data, &igc_lut32->c2_data))
		return -EFAULT;

	return 0;
}

static int __from_user_ar_gc_lut_data(
			struct mdp_ar_gc_lut_data32 __user *ar_gc_data32,
			struct mdp_ar_gc_lut_data __user *ar_gc_data)
{
	if (copy_in_user(&ar_gc_data->x_start,
			&ar_gc_data32->x_start,
			sizeof(uint32_t)) ||
	    copy_in_user(&ar_gc_data->slope,
			&ar_gc_data32->slope,
			sizeof(uint32_t)) ||
	    copy_in_user(&ar_gc_data->offset,
			&ar_gc_data32->offset,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_ar_gc_lut_data(
			struct mdp_ar_gc_lut_data32 __user *ar_gc_data32,
			struct mdp_ar_gc_lut_data __user *ar_gc_data)
{
	if (copy_in_user(&ar_gc_data32->x_start,
			&ar_gc_data->x_start,
			sizeof(uint32_t)) ||
	    copy_in_user(&ar_gc_data32->slope,
			&ar_gc_data->slope,
			sizeof(uint32_t)) ||
	    copy_in_user(&ar_gc_data32->offset,
			&ar_gc_data->offset,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}


static int __from_user_pgc_lut_data_v1_7(
			struct mdp_pgc_lut_data32 __user *pgc_lut32,
			struct mdp_pgc_lut_data __user *pgc_lut)
{
	struct mdp_pgc_lut_data_v1_7_32 pgc_cfg_payload_32;
	struct mdp_pgc_lut_data_v1_7 pgc_cfg_payload;
	if (copy_from_user(&pgc_cfg_payload_32,
			   compat_ptr(pgc_lut32->cfg_payload),
			   sizeof(pgc_cfg_payload_32))) {
		pr_err("failed to copy from user the pgc32 payload\n");
		return -EFAULT;
	}
	pgc_cfg_payload.c0_data = compat_ptr(pgc_cfg_payload_32.c0_data);
	pgc_cfg_payload.c1_data = compat_ptr(pgc_cfg_payload_32.c1_data);
	pgc_cfg_payload.c2_data = compat_ptr(pgc_cfg_payload_32.c2_data);
	pgc_cfg_payload.len = pgc_cfg_payload_32.len;
	if (copy_to_user(pgc_lut->cfg_payload, &pgc_cfg_payload,
			 sizeof(pgc_cfg_payload))) {
		pr_err("failed to copy to user pgc payload\n");
		return -EFAULT;
	}
	return 0;
}

static int __from_user_pgc_lut_data_legacy(
			struct mdp_pgc_lut_data32 __user *pgc_lut32,
			struct mdp_pgc_lut_data __user *pgc_lut)
{
	struct mdp_ar_gc_lut_data32 __user *r_data_temp32;
	struct mdp_ar_gc_lut_data32 __user *g_data_temp32;
	struct mdp_ar_gc_lut_data32 __user *b_data_temp32;
	struct mdp_ar_gc_lut_data __user *r_data_temp;
	struct mdp_ar_gc_lut_data __user *g_data_temp;
	struct mdp_ar_gc_lut_data __user *b_data_temp;
	uint8_t num_r_stages, num_g_stages, num_b_stages;
	int i;

	if (copy_from_user(&num_r_stages,
			&pgc_lut32->num_r_stages,
			sizeof(uint8_t)) ||
	    copy_from_user(&num_g_stages,
			&pgc_lut32->num_g_stages,
			sizeof(uint8_t)) ||
	    copy_from_user(&num_b_stages,
			&pgc_lut32->num_b_stages,
			sizeof(uint8_t)))
		return -EFAULT;

	if (num_r_stages > GC_LUT_SEGMENTS || num_b_stages > GC_LUT_SEGMENTS
	    || num_r_stages > GC_LUT_SEGMENTS || !num_r_stages || !num_b_stages
	    || !num_g_stages) {
		pr_err("invalid number of stages r_stages %d b_stages %d g_stages %d\n",
		       num_r_stages, num_b_stages, num_r_stages);
		return -EFAULT;
	}

	r_data_temp32 = compat_ptr((uintptr_t)pgc_lut32->r_data);
	r_data_temp = pgc_lut->r_data;

	for (i = 0; i < num_r_stages; i++) {
		if (__from_user_ar_gc_lut_data(
				&r_data_temp32[i],
				&r_data_temp[i]))
			return -EFAULT;
	}

	g_data_temp32 = compat_ptr((uintptr_t)pgc_lut32->g_data);
	g_data_temp = pgc_lut->g_data;

	for (i = 0; i < num_g_stages; i++) {
		if (__from_user_ar_gc_lut_data(
				&g_data_temp32[i],
				&g_data_temp[i]))
			return -EFAULT;
	}

	b_data_temp32 = compat_ptr((uintptr_t)pgc_lut32->b_data);
	b_data_temp = pgc_lut->b_data;

	for (i = 0; i < num_b_stages; i++) {
		if (__from_user_ar_gc_lut_data(
				&b_data_temp32[i],
				&b_data_temp[i]))
			return -EFAULT;
	}
	return 0;
}

static int __from_user_pgc_lut_data(
			struct mdp_pgc_lut_data32 __user *pgc_lut32,
			struct mdp_pgc_lut_data __user *pgc_lut)
{
	u32 version = mdp_pgc_vmax;
	int ret = 0;

	if (copy_in_user(&pgc_lut->block,
			&pgc_lut32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&pgc_lut->flags,
			&pgc_lut32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&pgc_lut->num_r_stages,
			&pgc_lut32->num_r_stages,
			sizeof(uint8_t)) ||
	    copy_in_user(&pgc_lut->num_g_stages,
			&pgc_lut32->num_g_stages,
			sizeof(uint8_t)) ||
	    copy_in_user(&pgc_lut->num_b_stages,
			&pgc_lut32->num_b_stages,
			sizeof(uint8_t)) ||
	    copy_in_user(&pgc_lut->version,
			&pgc_lut32->version,
			sizeof(uint32_t)))
		return -EFAULT;
	if (copy_from_user(&version, &pgc_lut32->version, sizeof(u32))) {
		pr_err("version copying failed\n");
		return -EFAULT;
	}
	switch (version) {
	case mdp_pgc_v1_7:
		ret = __from_user_pgc_lut_data_v1_7(pgc_lut32, pgc_lut);
		if (ret)
			pr_err("failed to copy pgc v17\n");
		break;
	default:
		pr_debug("version %d not supported fallback to legacy\n",
			 version);
		ret = __from_user_pgc_lut_data_legacy(pgc_lut32, pgc_lut);
		if (ret)
			pr_err("copy from user pgc lut legacy failed ret %d\n",
				ret);
		break;
	}
	return ret;
}

static int __to_user_pgc_lut_data(
			struct mdp_pgc_lut_data32 __user *pgc_lut32,
			struct mdp_pgc_lut_data __user *pgc_lut)
{
	struct mdp_ar_gc_lut_data32 __user *r_data_temp32;
	struct mdp_ar_gc_lut_data32 __user *g_data_temp32;
	struct mdp_ar_gc_lut_data32 __user *b_data_temp32;
	struct mdp_ar_gc_lut_data __user *r_data_temp;
	struct mdp_ar_gc_lut_data __user *g_data_temp;
	struct mdp_ar_gc_lut_data __user *b_data_temp;
	uint8_t num_r_stages, num_g_stages, num_b_stages;
	int i;

	if (copy_in_user(&pgc_lut32->block,
			&pgc_lut->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&pgc_lut32->flags,
			&pgc_lut->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&pgc_lut32->num_r_stages,
			&pgc_lut->num_r_stages,
			sizeof(uint8_t)) ||
	    copy_in_user(&pgc_lut32->num_g_stages,
			&pgc_lut->num_g_stages,
			sizeof(uint8_t)) ||
	    copy_in_user(&pgc_lut32->num_b_stages,
			&pgc_lut->num_b_stages,
			sizeof(uint8_t)))
		return -EFAULT;

	if (copy_from_user(&num_r_stages,
			&pgc_lut->num_r_stages,
			sizeof(uint8_t)) ||
	    copy_from_user(&num_g_stages,
			&pgc_lut->num_g_stages,
			sizeof(uint8_t)) ||
	    copy_from_user(&num_b_stages,
			&pgc_lut->num_b_stages,
			sizeof(uint8_t)))
		return -EFAULT;

	r_data_temp32 = compat_ptr((uintptr_t)pgc_lut32->r_data);
	r_data_temp = pgc_lut->r_data;
	for (i = 0; i < num_r_stages; i++) {
		if (__to_user_ar_gc_lut_data(
				&r_data_temp32[i],
				&r_data_temp[i]))
			return -EFAULT;
	}

	g_data_temp32 = compat_ptr((uintptr_t)pgc_lut32->g_data);
	g_data_temp = pgc_lut->g_data;
	for (i = 0; i < num_g_stages; i++) {
		if (__to_user_ar_gc_lut_data(
				&g_data_temp32[i],
				&g_data_temp[i]))
			return -EFAULT;
	}

	b_data_temp32 = compat_ptr((uintptr_t)pgc_lut32->b_data);
	b_data_temp = pgc_lut->b_data;
	for (i = 0; i < num_b_stages; i++) {
		if (__to_user_ar_gc_lut_data(
				&b_data_temp32[i],
				&b_data_temp[i]))
			return -EFAULT;
	}

	return 0;
}

static int __from_user_hist_lut_data_v1_7(
			struct mdp_hist_lut_data32 __user *hist_lut32,
			struct mdp_hist_lut_data __user *hist_lut)
{
	struct mdp_hist_lut_data_v1_7_32 hist_lut_cfg_payload32;
	struct mdp_hist_lut_data_v1_7 hist_lut_cfg_payload;

	if (copy_from_user(&hist_lut_cfg_payload32,
			compat_ptr(hist_lut32->cfg_payload),
			sizeof(hist_lut_cfg_payload32))) {
		pr_err("failed to copy the Hist Lut payload from userspace\n");
		return -EFAULT;
	}

	hist_lut_cfg_payload.len = hist_lut_cfg_payload32.len;
	hist_lut_cfg_payload.data = compat_ptr(hist_lut_cfg_payload32.data);

	if (copy_to_user(hist_lut->cfg_payload,
			&hist_lut_cfg_payload,
			sizeof(hist_lut_cfg_payload))) {
		pr_err("Failed to copy to user hist lut cfg payload\n");
		return -EFAULT;
	}

	return 0;
}

static int __from_user_hist_lut_data(
			struct mdp_hist_lut_data32 __user *hist_lut32,
			struct mdp_hist_lut_data __user *hist_lut)
{
	uint32_t version = 0;
	uint32_t data;

	if (copy_in_user(&hist_lut->block,
			&hist_lut32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut->version,
			&hist_lut32->version,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut->hist_lut_first,
			&hist_lut32->hist_lut_first,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut->ops,
			&hist_lut32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut->len,
			&hist_lut32->len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_from_user(&version,
			&hist_lut32->version,
			sizeof(uint32_t))) {
		pr_err("failed to copy the version info\n");
		return -EFAULT;
	}

	switch (version) {
	case mdp_hist_lut_v1_7:
		if (__from_user_hist_lut_data_v1_7(hist_lut32, hist_lut)) {
			pr_err("failed to get hist lut data for version %d\n",
				version);
			return -EFAULT;
		}
		break;
	default:
		pr_debug("version invalid, fallback to legacy\n");
		if (get_user(data, &hist_lut32->data) ||
		    put_user(compat_ptr(data), &hist_lut->data))
			return -EFAULT;
		break;
	}

	return 0;
}

static int __to_user_hist_lut_data(
			struct mdp_hist_lut_data32 __user *hist_lut32,
			struct mdp_hist_lut_data __user *hist_lut)
{
	unsigned long data;

	if (copy_in_user(&hist_lut32->block,
			&hist_lut->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut32->ops,
			&hist_lut->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut32->len,
			&hist_lut->len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &hist_lut->data) ||
	    put_user((compat_caddr_t) data, &hist_lut32->data))
		return -EFAULT;

	return 0;
}

static int __from_user_rgb_lut_data(
				struct mdp_rgb_lut_data32 __user *rgb_lut32,
				struct mdp_rgb_lut_data __user *rgb_lut)
{
	if (copy_in_user(&rgb_lut->flags, &rgb_lut32->flags,
		sizeof(uint32_t)) ||
		copy_in_user(&rgb_lut->lut_type, &rgb_lut32->lut_type,
		sizeof(uint32_t)))
		return -EFAULT;

	return __from_user_fb_cmap(&rgb_lut->cmap, &rgb_lut32->cmap);
}

static int __to_user_rgb_lut_data(
			struct mdp_rgb_lut_data32 __user *rgb_lut32,
			struct mdp_rgb_lut_data __user *rgb_lut)
{
	if (copy_in_user(&rgb_lut32->flags, &rgb_lut->flags,
		sizeof(uint32_t)) ||
		copy_in_user(&rgb_lut32->lut_type, &rgb_lut->lut_type,
		sizeof(uint32_t)))
		return -EFAULT;

	return __to_user_fb_cmap(&rgb_lut->cmap, &rgb_lut32->cmap);
}

static int __from_user_lut_cfg_data(
			struct mdp_lut_cfg_data32 __user *lut_cfg32,
			struct mdp_lut_cfg_data __user *lut_cfg)
{
	uint32_t lut_type;
	int ret = 0;

	if (copy_from_user(&lut_type, &lut_cfg32->lut_type,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_in_user(&lut_cfg->lut_type,
			&lut_cfg32->lut_type,
			sizeof(uint32_t)))
		return -EFAULT;

	switch (lut_type) {
	case mdp_lut_igc:
		ret = __from_user_igc_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.igc_lut_data),
			&lut_cfg->data.igc_lut_data);
		break;
	case mdp_lut_pgc:
		ret = __from_user_pgc_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.pgc_lut_data),
			&lut_cfg->data.pgc_lut_data);
		break;
	case mdp_lut_hist:
		ret = __from_user_hist_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.hist_lut_data),
			&lut_cfg->data.hist_lut_data);
		break;
	case mdp_lut_rgb:
		ret = __from_user_rgb_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.rgb_lut_data),
			&lut_cfg->data.rgb_lut_data);
		break;
	default:
		break;
	}

	return ret;
}

static int __to_user_lut_cfg_data(
			struct mdp_lut_cfg_data32 __user *lut_cfg32,
			struct mdp_lut_cfg_data __user *lut_cfg)
{
	uint32_t lut_type;
	int ret = 0;

	if (copy_from_user(&lut_type, &lut_cfg->lut_type,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_in_user(&lut_cfg32->lut_type,
			&lut_cfg->lut_type,
			sizeof(uint32_t)))
		return -EFAULT;

	switch (lut_type) {
	case mdp_lut_igc:
		ret = __to_user_igc_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.igc_lut_data),
			&lut_cfg->data.igc_lut_data);
		break;
	case mdp_lut_pgc:
		ret = __to_user_pgc_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.pgc_lut_data),
			&lut_cfg->data.pgc_lut_data);
		break;
	case mdp_lut_hist:
		ret = __to_user_hist_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.hist_lut_data),
			&lut_cfg->data.hist_lut_data);
		break;
	case mdp_lut_rgb:
		ret = __to_user_rgb_lut_data(
			compat_ptr((uintptr_t)&lut_cfg32->data.rgb_lut_data),
			&lut_cfg->data.rgb_lut_data);
		break;
	default:
		break;
	}

	return ret;
}

static int __from_user_qseed_cfg(
			struct mdp_qseed_cfg32 __user *qseed_data32,
			struct mdp_qseed_cfg __user *qseed_data)
{
	uint32_t data;

	if (copy_in_user(&qseed_data->table_num,
			&qseed_data32->table_num,
			sizeof(uint32_t)) ||
	    copy_in_user(&qseed_data->ops,
			&qseed_data32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&qseed_data->len,
			&qseed_data32->len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, &qseed_data32->data) ||
	    put_user(compat_ptr(data), &qseed_data->data))
		return -EFAULT;

	return 0;
}

static int __to_user_qseed_cfg(
			struct mdp_qseed_cfg32 __user *qseed_data32,
			struct mdp_qseed_cfg __user *qseed_data)
{
	unsigned long data;

	if (copy_in_user(&qseed_data32->table_num,
			&qseed_data->table_num,
			sizeof(uint32_t)) ||
	    copy_in_user(&qseed_data32->ops,
			&qseed_data->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&qseed_data32->len,
			&qseed_data->len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &qseed_data->data) ||
	    put_user((compat_caddr_t) data, &qseed_data32->data))
		return -EFAULT;

	return 0;
}

static int __from_user_qseed_cfg_data(
			struct mdp_qseed_cfg_data32 __user *qseed_cfg32,
			struct mdp_qseed_cfg_data __user *qseed_cfg)
{
	if (copy_in_user(&qseed_cfg->block,
			&qseed_cfg32->block,
			sizeof(uint32_t)))
		return -EFAULT;

	if (__from_user_qseed_cfg(
			compat_ptr((uintptr_t)&qseed_cfg32->qseed_data),
			&qseed_cfg->qseed_data))
		return -EFAULT;

	return 0;
}

static int __to_user_qseed_cfg_data(
			struct mdp_qseed_cfg_data32 __user *qseed_cfg32,
			struct mdp_qseed_cfg_data __user *qseed_cfg)
{
	if (copy_in_user(&qseed_cfg32->block,
			&qseed_cfg->block,
			sizeof(uint32_t)))
		return -EFAULT;

	if (__to_user_qseed_cfg(
			compat_ptr((uintptr_t)&qseed_cfg32->qseed_data),
			&qseed_cfg->qseed_data))
		return -EFAULT;

	return 0;
}

static int __from_user_bl_scale_data(
			struct mdp_bl_scale_data32 __user *bl_scale32,
			struct mdp_bl_scale_data __user *bl_scale)
{
	if (copy_in_user(&bl_scale->min_lvl,
			&bl_scale32->min_lvl,
			sizeof(uint32_t)) ||
	    copy_in_user(&bl_scale->scale,
			&bl_scale32->scale,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_pa_cfg(
			struct mdp_pa_cfg32 __user *pa_data32,
			struct mdp_pa_cfg __user *pa_data)
{
	if (copy_in_user(&pa_data->flags,
			&pa_data32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data->hue_adj,
			&pa_data32->hue_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data->sat_adj,
			&pa_data32->sat_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data->val_adj,
			&pa_data32->val_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data->cont_adj,
			&pa_data32->cont_adj,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_pa_cfg(
			struct mdp_pa_cfg32 __user *pa_data32,
			struct mdp_pa_cfg __user *pa_data)
{
	if (copy_in_user(&pa_data32->flags,
			&pa_data->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data32->hue_adj,
			&pa_data->hue_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data32->sat_adj,
			&pa_data->sat_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data32->val_adj,
			&pa_data->val_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_data32->cont_adj,
			&pa_data->cont_adj,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_pa_cfg_data(
			struct mdp_pa_cfg_data32 __user *pa_cfg32,
			struct mdp_pa_cfg_data __user *pa_cfg)
{
	if (copy_in_user(&pa_cfg->block,
			&pa_cfg32->block,
			sizeof(uint32_t)))
		return -EFAULT;
	if (__from_user_pa_cfg(
			compat_ptr((uintptr_t)&pa_cfg32->pa_data),
			&pa_cfg->pa_data))
		return -EFAULT;

	return 0;
}

static int __to_user_pa_cfg_data(
			struct mdp_pa_cfg_data32 __user *pa_cfg32,
			struct mdp_pa_cfg_data __user *pa_cfg)
{
	if (copy_in_user(&pa_cfg32->block,
			&pa_cfg->block,
			sizeof(uint32_t)))
		return -EFAULT;
	if (__to_user_pa_cfg(
			compat_ptr((uintptr_t)&pa_cfg32->pa_data),
			&pa_cfg->pa_data))
		return -EFAULT;

	return 0;
}

static int __from_user_mem_col_cfg(
			struct mdp_pa_mem_col_cfg32 __user *mem_col_cfg32,
			struct mdp_pa_mem_col_cfg __user *mem_col_cfg)
{
	if (copy_in_user(&mem_col_cfg->color_adjust_p0,
			&mem_col_cfg32->color_adjust_p0,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg->color_adjust_p1,
			&mem_col_cfg32->color_adjust_p1,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg->hue_region,
			&mem_col_cfg32->hue_region,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg->sat_region,
			&mem_col_cfg32->sat_region,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg->val_region,
			&mem_col_cfg32->val_region,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_mem_col_cfg(
			struct mdp_pa_mem_col_cfg32 __user *mem_col_cfg32,
			struct mdp_pa_mem_col_cfg __user *mem_col_cfg)
{
	if (copy_in_user(&mem_col_cfg32->color_adjust_p0,
			&mem_col_cfg->color_adjust_p0,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg32->color_adjust_p1,
			&mem_col_cfg->color_adjust_p1,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg32->hue_region,
			&mem_col_cfg->hue_region,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg32->sat_region,
			&mem_col_cfg->sat_region,
			sizeof(uint32_t)) ||
	    copy_in_user(&mem_col_cfg32->val_region,
			&mem_col_cfg->val_region,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_pa_v2_data(
			struct mdp_pa_v2_data32 __user *pa_v2_data32,
			struct mdp_pa_v2_data __user *pa_v2_data)
{
	uint32_t data;

	if (copy_in_user(&pa_v2_data->flags,
			&pa_v2_data32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data->global_hue_adj,
			&pa_v2_data32->global_hue_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data->global_sat_adj,
			&pa_v2_data32->global_sat_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data->global_val_adj,
			&pa_v2_data32->global_val_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data->global_cont_adj,
			&pa_v2_data32->global_cont_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data->six_zone_thresh,
			&pa_v2_data32->six_zone_thresh,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data->six_zone_len,
			&pa_v2_data32->six_zone_len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, &pa_v2_data32->six_zone_curve_p0) ||
	    put_user(compat_ptr(data), &pa_v2_data->six_zone_curve_p0) ||
	    get_user(data, &pa_v2_data32->six_zone_curve_p1) ||
	    put_user(compat_ptr(data), &pa_v2_data->six_zone_curve_p1))
		return -EFAULT;

	if (__from_user_mem_col_cfg(
			compat_ptr((uintptr_t)&pa_v2_data32->skin_cfg),
			&pa_v2_data->skin_cfg) ||
	    __from_user_mem_col_cfg(
			compat_ptr((uintptr_t)&pa_v2_data32->sky_cfg),
			&pa_v2_data->sky_cfg) ||
	    __from_user_mem_col_cfg(
			compat_ptr((uintptr_t)&pa_v2_data32->fol_cfg),
			&pa_v2_data->fol_cfg))
		return -EFAULT;

	return 0;
}

static int __to_user_pa_v2_data(
			struct mdp_pa_v2_data32 __user *pa_v2_data32,
			struct mdp_pa_v2_data __user *pa_v2_data)
{
	unsigned long data;

	if (copy_in_user(&pa_v2_data32->flags,
			&pa_v2_data->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data32->global_hue_adj,
			&pa_v2_data->global_hue_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data32->global_sat_adj,
			&pa_v2_data->global_sat_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data32->global_val_adj,
			&pa_v2_data->global_val_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data32->global_cont_adj,
			&pa_v2_data->global_cont_adj,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data32->six_zone_thresh,
			&pa_v2_data->six_zone_thresh,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_data32->six_zone_len,
			&pa_v2_data->six_zone_len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &pa_v2_data->six_zone_curve_p0) ||
	    put_user((compat_caddr_t) data, &pa_v2_data32->six_zone_curve_p0) ||
	    get_user(data, (unsigned long *) &pa_v2_data->six_zone_curve_p1) ||
	    put_user((compat_caddr_t) data, &pa_v2_data32->six_zone_curve_p1))
		return -EFAULT;

	if (__to_user_mem_col_cfg(
			compat_ptr((uintptr_t)&pa_v2_data32->skin_cfg),
			&pa_v2_data->skin_cfg) ||
	    __to_user_mem_col_cfg(
			compat_ptr((uintptr_t)&pa_v2_data32->sky_cfg),
			&pa_v2_data->sky_cfg) ||
	    __to_user_mem_col_cfg(
			compat_ptr((uintptr_t)&pa_v2_data32->fol_cfg),
			&pa_v2_data->fol_cfg))
		return -EFAULT;

	return 0;
}

static inline void __from_user_pa_mem_col_data_v1_7(
			struct mdp_pa_mem_col_data_v1_7_32 *mem_col_data32,
			struct mdp_pa_mem_col_data_v1_7 *mem_col_data)
{
	mem_col_data->color_adjust_p0 = mem_col_data32->color_adjust_p0;
	mem_col_data->color_adjust_p1 = mem_col_data32->color_adjust_p1;
	mem_col_data->color_adjust_p2 = mem_col_data32->color_adjust_p2;
	mem_col_data->blend_gain = mem_col_data32->blend_gain;
	mem_col_data->sat_hold = mem_col_data32->sat_hold;
	mem_col_data->val_hold = mem_col_data32->val_hold;
	mem_col_data->hue_region = mem_col_data32->hue_region;
	mem_col_data->sat_region = mem_col_data32->sat_region;
	mem_col_data->val_region = mem_col_data32->val_region;
}


static int __from_user_pa_data_v1_7(
			struct mdp_pa_v2_cfg_data32 __user *pa_v2_cfg32,
			struct mdp_pa_v2_cfg_data __user *pa_v2_cfg)
{
	struct mdp_pa_data_v1_7_32 pa_cfg_payload32;
	struct mdp_pa_data_v1_7 pa_cfg_payload;

	if (copy_from_user(&pa_cfg_payload32,
			compat_ptr(pa_v2_cfg32->cfg_payload),
			sizeof(pa_cfg_payload32))) {
		pr_err("failed to copy the PA payload from userspace\n");
		return -EFAULT;
	}

	pa_cfg_payload.mode = pa_cfg_payload32.mode;
	pa_cfg_payload.global_hue_adj = pa_cfg_payload32.global_hue_adj;
	pa_cfg_payload.global_sat_adj = pa_cfg_payload32.global_sat_adj;
	pa_cfg_payload.global_val_adj = pa_cfg_payload32.global_val_adj;
	pa_cfg_payload.global_cont_adj = pa_cfg_payload32.global_cont_adj;

	__from_user_pa_mem_col_data_v1_7(&pa_cfg_payload32.skin_cfg,
					&pa_cfg_payload.skin_cfg);
	__from_user_pa_mem_col_data_v1_7(&pa_cfg_payload32.sky_cfg,
					&pa_cfg_payload.sky_cfg);
	__from_user_pa_mem_col_data_v1_7(&pa_cfg_payload32.fol_cfg,
					&pa_cfg_payload.fol_cfg);

	pa_cfg_payload.six_zone_thresh = pa_cfg_payload32.six_zone_thresh;
	pa_cfg_payload.six_zone_adj_p0 = pa_cfg_payload32.six_zone_adj_p0;
	pa_cfg_payload.six_zone_adj_p1 = pa_cfg_payload32.six_zone_adj_p1;
	pa_cfg_payload.six_zone_sat_hold = pa_cfg_payload32.six_zone_sat_hold;
	pa_cfg_payload.six_zone_val_hold = pa_cfg_payload32.six_zone_val_hold;
	pa_cfg_payload.six_zone_len = pa_cfg_payload32.six_zone_len;

	pa_cfg_payload.six_zone_curve_p0 =
		compat_ptr(pa_cfg_payload32.six_zone_curve_p0);
	pa_cfg_payload.six_zone_curve_p1 =
		compat_ptr(pa_cfg_payload32.six_zone_curve_p1);

	if (copy_to_user(pa_v2_cfg->cfg_payload, &pa_cfg_payload,
			sizeof(pa_cfg_payload))) {
		pr_err("Failed to copy to user pa cfg payload\n");
		return -EFAULT;
	}

	return 0;
}

static int __from_user_pa_v2_cfg_data(
			struct mdp_pa_v2_cfg_data32 __user *pa_v2_cfg32,
			struct mdp_pa_v2_cfg_data __user *pa_v2_cfg)
{
	uint32_t version;

	if (copy_in_user(&pa_v2_cfg->block,
			&pa_v2_cfg32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_cfg->version,
			&pa_v2_cfg32->version,
			sizeof(uint32_t)) ||
	    copy_in_user(&pa_v2_cfg->flags,
			&pa_v2_cfg32->flags,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_from_user(&version,
			&pa_v2_cfg32->version,
			sizeof(uint32_t))) {
		pr_err("failed to copy the version info\n");
		return -EFAULT;
	}

	switch (version) {
	case mdp_pa_v1_7:
		if (__from_user_pa_data_v1_7(pa_v2_cfg32, pa_v2_cfg)) {
			pr_err("failed to get pa data for version %d\n",
				version);
			return -EFAULT;
		}
		break;
	default:
		pr_debug("version invalid, fallback to legacy\n");
		if (__from_user_pa_v2_data(
				compat_ptr((uintptr_t)&pa_v2_cfg32->pa_v2_data),
				&pa_v2_cfg->pa_v2_data))
			return -EFAULT;
		break;
	}

	return 0;
}

static inline void __to_user_pa_mem_col_data_v1_7(
			struct mdp_pa_mem_col_data_v1_7_32 *mem_col_data32,
			struct mdp_pa_mem_col_data_v1_7 *mem_col_data)
{
	mem_col_data32->color_adjust_p0 = mem_col_data->color_adjust_p0;
	mem_col_data32->color_adjust_p1 = mem_col_data->color_adjust_p1;
	mem_col_data32->color_adjust_p2 = mem_col_data->color_adjust_p2;
	mem_col_data32->blend_gain = mem_col_data->blend_gain;
	mem_col_data32->sat_hold = mem_col_data->sat_hold;
	mem_col_data32->val_hold = mem_col_data->val_hold;
	mem_col_data32->hue_region = mem_col_data->hue_region;
	mem_col_data32->sat_region = mem_col_data->sat_region;
	mem_col_data32->val_region = mem_col_data->val_region;
}

static int __to_user_pa_data_v1_7(
			struct mdp_pa_v2_cfg_data32 __user *pa_v2_cfg32,
			struct mdp_pa_v2_cfg_data __user *pa_v2_cfg)
{
	struct mdp_pa_data_v1_7_32 pa_cfg_payload32;
	struct mdp_pa_data_v1_7 pa_cfg_payload;

	if (copy_from_user(&pa_cfg_payload,
			pa_v2_cfg->cfg_payload,
			sizeof(pa_cfg_payload))) {
		pr_err("failed to copy the PA payload from userspace\n");
		return -EFAULT;
	}

	pa_cfg_payload32.mode = pa_cfg_payload.mode;
	pa_cfg_payload32.global_hue_adj = pa_cfg_payload.global_hue_adj;
	pa_cfg_payload32.global_sat_adj = pa_cfg_payload.global_sat_adj;
	pa_cfg_payload32.global_val_adj = pa_cfg_payload.global_val_adj;
	pa_cfg_payload32.global_cont_adj = pa_cfg_payload.global_cont_adj;

	__to_user_pa_mem_col_data_v1_7(&pa_cfg_payload32.skin_cfg,
					&pa_cfg_payload.skin_cfg);
	__to_user_pa_mem_col_data_v1_7(&pa_cfg_payload32.sky_cfg,
					&pa_cfg_payload.sky_cfg);
	__to_user_pa_mem_col_data_v1_7(&pa_cfg_payload32.fol_cfg,
					&pa_cfg_payload.fol_cfg);

	pa_cfg_payload32.six_zone_thresh = pa_cfg_payload.six_zone_thresh;
	pa_cfg_payload32.six_zone_adj_p0 = pa_cfg_payload.six_zone_adj_p0;
	pa_cfg_payload32.six_zone_adj_p1 = pa_cfg_payload.six_zone_adj_p1;
	pa_cfg_payload32.six_zone_sat_hold = pa_cfg_payload.six_zone_sat_hold;
	pa_cfg_payload32.six_zone_val_hold = pa_cfg_payload.six_zone_val_hold;
	pa_cfg_payload32.six_zone_len = pa_cfg_payload.six_zone_len;

	if (copy_to_user(compat_ptr(pa_v2_cfg32->cfg_payload),
			&pa_cfg_payload32,
			sizeof(pa_cfg_payload32))) {
		pr_err("Failed to copy to user pa cfg payload\n");
		return -EFAULT;
	}

	return 0;
}

static int __to_user_pa_v2_cfg_data(
			struct mdp_pa_v2_cfg_data32 __user *pa_v2_cfg32,
			struct mdp_pa_v2_cfg_data __user *pa_v2_cfg)
{
	uint32_t version = 0;
	uint32_t flags = 0;

	if (copy_from_user(&version,
			&pa_v2_cfg32->version,
			sizeof(uint32_t)))
		return -EFAULT;

	switch (version) {
	case mdp_pa_v1_7:
		if (copy_from_user(&flags,
				&pa_v2_cfg32->flags,
				sizeof(uint32_t))) {
			pr_err("failed to get PA v1_7 flags\n");
			return -EFAULT;
		}

		if (!(flags & MDP_PP_OPS_READ)) {
			pr_debug("Read op not set. Skipping compat copyback\n");
			return 0;
		}

		if (__to_user_pa_data_v1_7(pa_v2_cfg32, pa_v2_cfg)) {
			pr_err("failed to set pa data for version %d\n",
				version);
			return -EFAULT;
		}
		break;
	default:
		pr_debug("version invalid, fallback to legacy\n");

		if (copy_from_user(&flags,
				&pa_v2_cfg32->pa_v2_data.flags,
				sizeof(uint32_t))) {
			pr_err("failed to get PAv2 flags\n");
			return -EFAULT;
		}

		if (!(flags & MDP_PP_OPS_READ)) {
			pr_debug("Read op not set. Skipping compat copyback\n");
			return 0;
		}

		if (__to_user_pa_v2_data(
				compat_ptr((uintptr_t)&pa_v2_cfg32->pa_v2_data),
				&pa_v2_cfg->pa_v2_data))
			return -EFAULT;
		break;
	}

	return 0;
}

static int __from_user_dither_cfg_data(
			struct mdp_dither_cfg_data32 __user *dither_cfg32,
			struct mdp_dither_cfg_data __user *dither_cfg)
{
	if (copy_in_user(&dither_cfg->block,
			&dither_cfg32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg->flags,
			&dither_cfg32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg->g_y_depth,
			&dither_cfg32->g_y_depth,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg->r_cr_depth,
			&dither_cfg32->r_cr_depth,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg->b_cb_depth,
			&dither_cfg32->b_cb_depth,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_dither_cfg_data(
			struct mdp_dither_cfg_data32 __user *dither_cfg32,
			struct mdp_dither_cfg_data __user *dither_cfg)
{
	if (copy_in_user(&dither_cfg32->block,
			&dither_cfg->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg32->flags,
			&dither_cfg->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg32->g_y_depth,
			&dither_cfg->g_y_depth,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg32->r_cr_depth,
			&dither_cfg->r_cr_depth,
			sizeof(uint32_t)) ||
	    copy_in_user(&dither_cfg32->b_cb_depth,
			&dither_cfg->b_cb_depth,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_gamut_cfg_data_v17(
			struct mdp_gamut_cfg_data32 __user *gamut_cfg32,
			struct mdp_gamut_cfg_data __user *gamut_cfg)
{
	struct mdp_gamut_data_v1_7 gamut_cfg_payload;
	struct mdp_gamut_data_v1_7_32 gamut_cfg_payload32;
	u32 i = 0;

	if (copy_from_user(&gamut_cfg_payload32,
			   compat_ptr(gamut_cfg32->cfg_payload),
			   sizeof(gamut_cfg_payload32))) {
		pr_err("failed to copy the gamut payload from userspace\n");
		return -EFAULT;
	}
	gamut_cfg_payload.mode = gamut_cfg_payload32.mode;
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		gamut_cfg_payload.tbl_size[i] =
			gamut_cfg_payload32.tbl_size[i];
		gamut_cfg_payload.c0_data[i] =
			compat_ptr(gamut_cfg_payload32.c0_data[i]);
		gamut_cfg_payload.c1_c2_data[i] =
			compat_ptr(gamut_cfg_payload32.c1_c2_data[i]);
	}
	for (i = 0; i < MDP_GAMUT_SCALE_OFF_TABLE_NUM; i++) {
		gamut_cfg_payload.tbl_scale_off_sz[i] =
			gamut_cfg_payload32.tbl_scale_off_sz[i];
		gamut_cfg_payload.scale_off_data[i] =
			compat_ptr(gamut_cfg_payload32.scale_off_data[i]);
	}
	if (copy_to_user(gamut_cfg->cfg_payload, &gamut_cfg_payload,
			 sizeof(gamut_cfg_payload))) {
		pr_err("failed to copy the gamut payload to userspace\n");
		return -EFAULT;
	}
	return 0;
}

static int __from_user_gamut_cfg_data(
			struct mdp_gamut_cfg_data32 __user *gamut_cfg32,
			struct mdp_gamut_cfg_data __user *gamut_cfg)
{
	uint32_t data, version;
	int i;

	if (copy_in_user(&gamut_cfg->block,
			&gamut_cfg32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg->flags,
			&gamut_cfg32->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg->gamut_first,
			&gamut_cfg32->gamut_first,
			sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg->tbl_size[0],
			&gamut_cfg32->tbl_size[0],
			MDP_GAMUT_TABLE_NUM * sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg->version,
			&gamut_cfg32->version,
			sizeof(uint32_t)))
		return 0;

	if (copy_from_user(&version, &gamut_cfg32->version, sizeof(u32))) {
		pr_err("failed to copy the version info\n");
		return -EFAULT;
	}

	switch (version) {
	case mdp_gamut_v1_7:
		if (__from_user_gamut_cfg_data_v17(gamut_cfg32, gamut_cfg)) {
			pr_err("failed to get the gamut data for version %d\n",
				version);
			return -EFAULT;
		}
		break;
	default:
		pr_debug("version invalid fallback to legacy\n");
	/* The Gamut LUT data contains 3 static arrays for R, G, and B
	 * gamut data. Each these arrays contains pointers dynamic arrays
	 * which hold the gamut LUTs for R, G, and B. Must copy the array of
	 * pointers from 32 bit to 64 bit addresses. */
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			if (get_user(data, &gamut_cfg32->r_tbl[i]) ||
			    put_user(compat_ptr(data), &gamut_cfg->r_tbl[i]))
				return -EFAULT;
		}

		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			if (get_user(data, &gamut_cfg32->g_tbl[i]) ||
			    put_user(compat_ptr(data), &gamut_cfg->g_tbl[i]))
				return -EFAULT;
		}

		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			if (get_user(data, &gamut_cfg32->b_tbl[i]) ||
			    put_user(compat_ptr(data), &gamut_cfg->b_tbl[i]))
				return -EFAULT;
		}
		break;
	}
	return 0;
}

static int __to_user_gamut_cfg_data(
			struct mdp_gamut_cfg_data32 __user *gamut_cfg32,
			struct mdp_gamut_cfg_data __user *gamut_cfg)
{
	unsigned long data;
	int i;

	if (copy_in_user(&gamut_cfg32->block,
			&gamut_cfg->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg32->flags,
			&gamut_cfg->flags,
			sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg32->gamut_first,
			&gamut_cfg->gamut_first,
			sizeof(uint32_t)) ||
	    copy_in_user(&gamut_cfg32->tbl_size[0],
			&gamut_cfg->tbl_size[0],
			MDP_GAMUT_TABLE_NUM * sizeof(uint32_t)))
		return 0;

	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
		if (get_user(data, (unsigned long *) &gamut_cfg->r_tbl[i]) ||
		    put_user((compat_caddr_t)data, &gamut_cfg32->r_tbl[i]))
			return -EFAULT;
	}

	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
		if (get_user(data, (unsigned long *) &gamut_cfg->g_tbl[i]) ||
		    put_user((compat_caddr_t)data, &gamut_cfg32->g_tbl[i]))
			return -EFAULT;
	}

	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
		if (get_user(data, (unsigned long *) &gamut_cfg->b_tbl[i]) ||
		    put_user((compat_caddr_t)data, &gamut_cfg32->g_tbl[i]))
			return -EFAULT;
	}

	return 0;
}

static int __from_user_calib_config_data(
			struct mdp_calib_config_data32 __user *calib_cfg32,
			struct mdp_calib_config_data __user *calib_cfg)
{
	if (copy_in_user(&calib_cfg->ops,
			&calib_cfg32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_cfg->addr,
			&calib_cfg32->addr,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_cfg->data,
			&calib_cfg32->data,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __to_user_calib_config_data(
			struct mdp_calib_config_data32 __user *calib_cfg32,
			struct mdp_calib_config_data __user *calib_cfg)
{
	if (copy_in_user(&calib_cfg32->ops,
			&calib_cfg->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_cfg32->addr,
			&calib_cfg->addr,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_cfg32->data,
			&calib_cfg->data,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_ad_init(
			struct mdss_ad_init32 __user *ad_init32,
			struct mdss_ad_init __user *ad_init)
{
	uint32_t data;

	if (copy_in_user(&ad_init->asym_lut[0],
			&ad_init32->asym_lut[0],
			33 * sizeof(uint32_t)) ||
	    copy_in_user(&ad_init->color_corr_lut[0],
			&ad_init32->color_corr_lut[0],
			33 * sizeof(uint32_t)) ||
	    copy_in_user(&ad_init->i_control[0],
			&ad_init32->i_control[0],
			2 * sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->black_lvl,
			&ad_init32->black_lvl,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_init->white_lvl,
			&ad_init32->white_lvl,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_init->var,
			&ad_init32->var,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->limit_ampl,
			&ad_init32->limit_ampl,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->i_dither,
			&ad_init32->i_dither,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->slope_max,
			&ad_init32->slope_max,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->slope_min,
			&ad_init32->slope_min,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->dither_ctl,
			&ad_init32->dither_ctl,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->format,
			&ad_init32->format,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->auto_size,
			&ad_init32->auto_size,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->frame_w,
			&ad_init32->frame_w,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_init->frame_h,
			&ad_init32->frame_h,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_init->logo_v,
			&ad_init32->logo_v,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->logo_h,
			&ad_init32->logo_h,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_init->alpha,
			&ad_init32->alpha,
			sizeof(uint32_t)) ||
	    copy_in_user(&ad_init->alpha_base,
			&ad_init32->alpha_base,
			sizeof(uint32_t)) ||
	    copy_in_user(&ad_init->bl_lin_len,
			&ad_init32->bl_lin_len,
			sizeof(uint32_t)) ||
	    copy_in_user(&ad_init->bl_att_len,
			&ad_init32->bl_att_len,
			sizeof(uint32_t)))
		return -EFAULT;


	if (get_user(data, &ad_init32->bl_lin) ||
	    put_user(compat_ptr(data), &ad_init->bl_lin) ||
	    get_user(data, &ad_init32->bl_lin_inv) ||
	    put_user(compat_ptr(data), &ad_init->bl_lin_inv) ||
	    get_user(data, &ad_init32->bl_att_lut) ||
	    put_user(compat_ptr(data), &ad_init->bl_att_lut))
		return -EFAULT;

	return 0;
}

static int __from_user_ad_cfg(
			struct mdss_ad_cfg32 __user *ad_cfg32,
			struct mdss_ad_cfg __user *ad_cfg)
{
	if (copy_in_user(&ad_cfg->mode,
			&ad_cfg32->mode,
			sizeof(uint32_t)) ||
	    copy_in_user(&ad_cfg->al_calib_lut[0],
			&ad_cfg32->al_calib_lut[0],
			33 * sizeof(uint32_t)) ||
	    copy_in_user(&ad_cfg->backlight_min,
			&ad_cfg32->backlight_min,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->backlight_max,
			&ad_cfg32->backlight_max,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->backlight_scale,
			&ad_cfg32->backlight_scale,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->amb_light_min,
			&ad_cfg32->amb_light_min,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->filter[0],
			&ad_cfg32->filter[0],
			2 * sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->calib[0],
			&ad_cfg32->calib[0],
			4 * sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->strength_limit,
			&ad_cfg32->strength_limit,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_cfg->t_filter_recursion,
			&ad_cfg32->t_filter_recursion,
			sizeof(uint8_t)) ||
	    copy_in_user(&ad_cfg->stab_itr,
			&ad_cfg32->stab_itr,
			sizeof(uint16_t)) ||
	    copy_in_user(&ad_cfg->bl_ctrl_mode,
			&ad_cfg32->bl_ctrl_mode,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_ad_init_cfg(
			struct mdss_ad_init_cfg32 __user *ad_info32,
			struct mdss_ad_init_cfg __user *ad_info)
{
	uint32_t op;

	if (copy_from_user(&op, &ad_info32->ops,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_in_user(&ad_info->ops,
			&ad_info32->ops,
			sizeof(uint32_t)))
		return -EFAULT;

	if (op & MDP_PP_AD_INIT) {
		if (__from_user_ad_init(
				compat_ptr((uintptr_t)&ad_info32->params.init),
				&ad_info->params.init))
			return -EFAULT;
	} else if (op & MDP_PP_AD_CFG) {
		if (__from_user_ad_cfg(
				compat_ptr((uintptr_t)&ad_info32->params.cfg),
				&ad_info->params.cfg))
			return -EFAULT;
	} else {
		pr_err("Invalid AD init/config operation\n");
		return -EINVAL;
	}

	return 0;
}

static int __from_user_ad_input(
			struct mdss_ad_input32 __user *ad_input32,
			struct mdss_ad_input __user *ad_input)
{
	int mode;

	if (copy_from_user(&mode,
			&ad_input32->mode,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_in_user(&ad_input->mode,
			&ad_input32->mode,
			sizeof(uint32_t)) ||
	    copy_in_user(&ad_input->output,
			&ad_input32->output,
			sizeof(uint32_t)))
		return -EFAULT;

	switch (mode) {
	case MDSS_AD_MODE_AUTO_BL:
	case MDSS_AD_MODE_AUTO_STR:
		if (copy_in_user(&ad_input->in.amb_light,
				&ad_input32->in.amb_light,
				sizeof(uint32_t)))
			return -EFAULT;
		break;
	case MDSS_AD_MODE_TARG_STR:
	case MDSS_AD_MODE_MAN_STR:
		if (copy_in_user(&ad_input->in.strength,
				&ad_input32->in.strength,
				sizeof(uint32_t)))
			return -EFAULT;
		break;
	case MDSS_AD_MODE_CALIB:
		if (copy_in_user(&ad_input->in.calib_bl,
				&ad_input32->in.calib_bl,
				sizeof(uint32_t)))
			return -EFAULT;
		break;
	}

	return 0;
}

static int __to_user_ad_input(
			struct mdss_ad_input32 __user *ad_input32,
			struct mdss_ad_input __user *ad_input)
{
	int mode;

	if (copy_from_user(&mode,
			&ad_input->mode,
			sizeof(uint32_t)))
		return -EFAULT;

	if (copy_in_user(&ad_input32->mode,
			&ad_input->mode,
			sizeof(uint32_t)) ||
	    copy_in_user(&ad_input32->output,
			&ad_input->output,
			sizeof(uint32_t)))
		return -EFAULT;

	switch (mode) {
	case MDSS_AD_MODE_AUTO_BL:
	case MDSS_AD_MODE_AUTO_STR:
		if (copy_in_user(&ad_input32->in.amb_light,
				&ad_input->in.amb_light,
				sizeof(uint32_t)))
			return -EFAULT;
		break;
	case MDSS_AD_MODE_TARG_STR:
	case MDSS_AD_MODE_MAN_STR:
		if (copy_in_user(&ad_input32->in.strength,
				&ad_input->in.strength,
				sizeof(uint32_t)))
			return -EFAULT;
		break;
	case MDSS_AD_MODE_CALIB:
		if (copy_in_user(&ad_input32->in.calib_bl,
				&ad_input->in.calib_bl,
				sizeof(uint32_t)))
			return -EFAULT;
		break;
	}

	return 0;
}

static int __from_user_calib_cfg(
			struct mdss_calib_cfg32 __user *calib_cfg32,
			struct mdss_calib_cfg __user *calib_cfg)
{
	if (copy_in_user(&calib_cfg->ops,
			&calib_cfg32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_cfg->calib_mask,
			&calib_cfg32->calib_mask,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_calib_config_buffer(
			struct mdp_calib_config_buffer32 __user *calib_buffer32,
			struct mdp_calib_config_buffer __user *calib_buffer)
{
	uint32_t data;

	if (copy_in_user(&calib_buffer->ops,
			&calib_buffer32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_buffer->size,
			&calib_buffer32->size,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, &calib_buffer32->buffer) ||
	    put_user(compat_ptr(data), &calib_buffer->buffer))
		return -EFAULT;

	return 0;
}

static int __to_user_calib_config_buffer(
			struct mdp_calib_config_buffer32 __user *calib_buffer32,
			struct mdp_calib_config_buffer __user *calib_buffer)
{
	unsigned long data;

	if (copy_in_user(&calib_buffer32->ops,
			&calib_buffer->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_buffer32->size,
			&calib_buffer->size,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &calib_buffer->buffer) ||
	    put_user((compat_caddr_t) data, &calib_buffer32->buffer))
		return -EFAULT;

	return 0;
}

static int __from_user_calib_dcm_state(
			struct mdp_calib_dcm_state32 __user *calib_dcm32,
			struct mdp_calib_dcm_state __user *calib_dcm)
{
	if (copy_in_user(&calib_dcm->ops,
			&calib_dcm32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&calib_dcm->dcm_state,
			&calib_dcm32->dcm_state,
			sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static u32 __pp_compat_size_igc(void)
{
	u32 alloc_size = 0;
	/* When we have mutiple versions pick largest struct size */
	alloc_size = sizeof(struct mdp_igc_lut_data_v1_7);
	return alloc_size;
}

static u32 __pp_compat_size_hist_lut(void)
{
	u32 alloc_size = 0;
	/* When we have mutiple versions pick largest struct size */
	alloc_size = sizeof(struct mdp_hist_lut_data_v1_7);
	return alloc_size;
}

static u32 __pp_compat_size_pgc(void)
{
	u32 tbl_sz_max = 0;
	tbl_sz_max =  3 * GC_LUT_SEGMENTS * sizeof(struct mdp_ar_gc_lut_data);
	tbl_sz_max += sizeof(struct mdp_pgc_lut_data_v1_7);
	return tbl_sz_max;
}

static u32 __pp_compat_size_pcc(void)
{
	/* if new version of PCC is added return max struct size */
	return sizeof(struct mdp_pcc_data_v1_7);
}

static u32 __pp_compat_size_pa(void)
{
	/* if new version of PA is added return max struct size */
	return sizeof(struct mdp_pa_data_v1_7);
}

static u32 __pp_compat_size_gamut(void)
{
	return sizeof(struct mdp_gamut_data_v1_7);
}

static int __pp_compat_alloc(struct msmfb_mdp_pp32 __user *pp32,
					struct msmfb_mdp_pp __user **pp,
					uint32_t op)
{
	uint32_t alloc_size = 0, lut_type, pgc_size = 0;

	alloc_size = sizeof(struct msmfb_mdp_pp);
	switch (op) {
	case  mdp_op_lut_cfg:
		if (copy_from_user(&lut_type,
			&pp32->data.lut_cfg_data.lut_type,
			sizeof(uint32_t)))
			return -EFAULT;

		switch (lut_type)  {
		case mdp_lut_pgc:

			pgc_size = GC_LUT_SEGMENTS *
				sizeof(struct mdp_ar_gc_lut_data);
			alloc_size += __pp_compat_size_pgc();

			*pp = compat_alloc_user_space(alloc_size);
			if (NULL == *pp)
				return -ENOMEM;
			memset(*pp, 0, alloc_size);

			(*pp)->data.lut_cfg_data.data.pgc_lut_data.r_data =
					(struct mdp_ar_gc_lut_data *)
					((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp));
			(*pp)->data.lut_cfg_data.data.pgc_lut_data.g_data =
					(struct mdp_ar_gc_lut_data *)
					((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp) +
					pgc_size);
			(*pp)->data.lut_cfg_data.data.pgc_lut_data.b_data =
					(struct mdp_ar_gc_lut_data *)
					((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp) +
					(2 * pgc_size));
			(*pp)->data.lut_cfg_data.data.pgc_lut_data.cfg_payload
					 = (void *)((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp) +
					(3 * pgc_size));
			break;
		case mdp_lut_igc:
			alloc_size += __pp_compat_size_igc();
			*pp = compat_alloc_user_space(alloc_size);
			if (NULL == *pp) {
				pr_err("failed to alloc from user size %d for igc\n",
					alloc_size);
				return -ENOMEM;
			}
			memset(*pp, 0, alloc_size);
			(*pp)->data.lut_cfg_data.data.igc_lut_data.cfg_payload
					= (void *)((unsigned long)(*pp) +
					   sizeof(struct msmfb_mdp_pp));
			break;
		case mdp_lut_hist:
			alloc_size += __pp_compat_size_hist_lut();
			*pp = compat_alloc_user_space(alloc_size);
			if (NULL == *pp) {
				pr_err("failed to alloc from user size %d for hist lut\n",
					alloc_size);
				return -ENOMEM;
			}
			memset(*pp, 0, alloc_size);
			(*pp)->data.lut_cfg_data.data.hist_lut_data.cfg_payload
					= (void *)((unsigned long)(*pp) +
					   sizeof(struct msmfb_mdp_pp));
			break;
		default:
			*pp = compat_alloc_user_space(alloc_size);
			if (NULL == *pp) {
				pr_err("failed to alloc from user size %d for lut_type %d\n",
					alloc_size, lut_type);
				return -ENOMEM;
			}
			memset(*pp, 0, alloc_size);
			break;
		}
		break;
	case mdp_op_pcc_cfg:
		alloc_size += __pp_compat_size_pcc();
		*pp = compat_alloc_user_space(alloc_size);
		if (NULL == *pp) {
			pr_err("alloc from user size %d for pcc fail\n",
				alloc_size);
			return -ENOMEM;
		}
		memset(*pp, 0, alloc_size);
		(*pp)->data.pcc_cfg_data.cfg_payload =
				(void *)((unsigned long)(*pp) +
				 sizeof(struct msmfb_mdp_pp));
		break;
	case mdp_op_gamut_cfg:
		alloc_size += __pp_compat_size_gamut();
		*pp = compat_alloc_user_space(alloc_size);
		if (NULL == *pp) {
			pr_err("alloc from user size %d for pcc fail\n",
				alloc_size);
			return -ENOMEM;
		}
		memset(*pp, 0, alloc_size);
		(*pp)->data.gamut_cfg_data.cfg_payload =
				(void *)((unsigned long)(*pp) +
				 sizeof(struct msmfb_mdp_pp));
		break;
	case mdp_op_pa_v2_cfg:
		alloc_size += __pp_compat_size_pa();
		*pp = compat_alloc_user_space(alloc_size);
		if (NULL == *pp) {
			pr_err("alloc from user size %d for pcc fail\n",
				alloc_size);
			return -ENOMEM;
		}
		memset(*pp, 0, alloc_size);
		(*pp)->data.pa_v2_cfg_data.cfg_payload =
				(void *)((unsigned long)(*pp) +
				sizeof(struct msmfb_mdp_pp));
		break;
	default:
		*pp = compat_alloc_user_space(alloc_size);
		if (NULL == *pp)
			return -ENOMEM;
		memset(*pp, 0, alloc_size);
		break;
	}
	return 0;
}

static int mdss_compat_pp_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg, struct file *file)
{
	uint32_t op;
	int ret = 0;
	struct msmfb_mdp_pp32 __user *pp32;
	struct msmfb_mdp_pp __user *pp;

	pp32 = compat_ptr(arg);
	if (copy_from_user(&op, &pp32->op, sizeof(uint32_t)))
		return -EFAULT;

	ret = __pp_compat_alloc(pp32, &pp, op);
	if (ret)
		return ret;

	if (copy_in_user(&pp->op, &pp32->op, sizeof(uint32_t)))
		return -EFAULT;

	switch (op) {
	case mdp_op_pcc_cfg:
		ret = __from_user_pcc_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pcc_cfg_data),
			&pp->data.pcc_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_pcc_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pcc_cfg_data),
			&pp->data.pcc_cfg_data);
		break;
	case mdp_op_csc_cfg:
		ret = __from_user_csc_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.csc_cfg_data),
			&pp->data.csc_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_csc_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.csc_cfg_data),
			&pp->data.csc_cfg_data);
		break;
	case mdp_op_lut_cfg:
		ret = __from_user_lut_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.lut_cfg_data),
			&pp->data.lut_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_lut_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.lut_cfg_data),
			&pp->data.lut_cfg_data);
		break;
	case mdp_op_qseed_cfg:
		ret = __from_user_qseed_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.qseed_cfg_data),
			&pp->data.qseed_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_qseed_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.qseed_cfg_data),
			&pp->data.qseed_cfg_data);
		break;
	case mdp_bl_scale_cfg:
		ret = __from_user_bl_scale_data(
			compat_ptr((uintptr_t)&pp32->data.bl_scale_data),
			&pp->data.bl_scale_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		break;
	case mdp_op_pa_cfg:
		ret = __from_user_pa_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pa_cfg_data),
			&pp->data.pa_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_pa_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pa_cfg_data),
			&pp->data.pa_cfg_data);
		break;
	case mdp_op_pa_v2_cfg:
		ret = __from_user_pa_v2_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pa_v2_cfg_data),
			&pp->data.pa_v2_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_pa_v2_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pa_v2_cfg_data),
			&pp->data.pa_v2_cfg_data);
		break;
	case mdp_op_dither_cfg:
		ret = __from_user_dither_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.dither_cfg_data),
			&pp->data.dither_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_dither_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.dither_cfg_data),
			&pp->data.dither_cfg_data);
		break;
	case mdp_op_gamut_cfg:
		ret = __from_user_gamut_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.gamut_cfg_data),
			&pp->data.gamut_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_gamut_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.gamut_cfg_data),
			&pp->data.gamut_cfg_data);
		break;
	case mdp_op_calib_cfg:
		ret = __from_user_calib_config_data(
			compat_ptr((uintptr_t)&pp32->data.calib_cfg),
			&pp->data.calib_cfg);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_calib_config_data(
			compat_ptr((uintptr_t)&pp32->data.calib_cfg),
			&pp->data.calib_cfg);
		break;
	case mdp_op_ad_cfg:
		ret = __from_user_ad_init_cfg(
			compat_ptr((uintptr_t)&pp32->data.ad_init_cfg),
			&pp->data.ad_init_cfg);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		break;
	case mdp_op_ad_input:
		ret = __from_user_ad_input(
			compat_ptr((uintptr_t)&pp32->data.ad_input),
			&pp->data.ad_input);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_ad_input(
			compat_ptr((uintptr_t)&pp32->data.ad_input),
			&pp->data.ad_input);
		break;
	case mdp_op_calib_mode:
		ret = __from_user_calib_cfg(
			compat_ptr((uintptr_t)&pp32->data.mdss_calib_cfg),
			&pp->data.mdss_calib_cfg);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		break;
	case mdp_op_calib_buffer:
		ret = __from_user_calib_config_buffer(
			compat_ptr((uintptr_t)&pp32->data.calib_buffer),
			&pp->data.calib_buffer);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		if (ret)
			goto pp_compat_exit;
		ret = __to_user_calib_config_buffer(
			compat_ptr((uintptr_t)&pp32->data.calib_buffer),
			&pp->data.calib_buffer);
		break;
	case mdp_op_calib_dcm_state:
		ret = __from_user_calib_dcm_state(
			compat_ptr((uintptr_t)&pp32->data.calib_dcm),
			&pp->data.calib_dcm);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp, file);
		break;
	default:
		break;
	}

pp_compat_exit:
	return ret;
}

static int __from_user_pp_params(struct mdp_overlay_pp_params32 *ppp32,
				struct mdp_overlay_pp_params *ppp)
{
	int ret = 0;

	if (copy_in_user(&ppp->config_ops,
			&ppp32->config_ops,
			sizeof(uint32_t)))
		return -EFAULT;

	ret = __from_user_csc_cfg(
			compat_ptr((uintptr_t)&ppp32->csc_cfg),
			&ppp->csc_cfg);
	if (ret)
		return ret;
	ret = __from_user_qseed_cfg(
			compat_ptr((uintptr_t)&ppp32->qseed_cfg[0]),
			&ppp->qseed_cfg[0]);
	if (ret)
		return ret;
	ret = __from_user_qseed_cfg(
			compat_ptr((uintptr_t)&ppp32->qseed_cfg[1]),
			&ppp->qseed_cfg[1]);
	if (ret)
		return ret;
	ret = __from_user_pa_cfg(
			compat_ptr((uintptr_t)&ppp32->pa_cfg),
			&ppp->pa_cfg);
	if (ret)
		return ret;
	ret = __from_user_igc_lut_data(
			compat_ptr((uintptr_t)&ppp32->igc_cfg),
			&ppp->igc_cfg);
	if (ret)
		return ret;
	ret = __from_user_sharp_cfg(
			compat_ptr((uintptr_t)&ppp32->sharp_cfg),
			&ppp->sharp_cfg);
	if (ret)
		return ret;
	ret = __from_user_histogram_cfg(
			compat_ptr((uintptr_t)&ppp32->hist_cfg),
			&ppp->hist_cfg);
	if (ret)
		return ret;
	ret = __from_user_hist_lut_data(
			compat_ptr((uintptr_t)&ppp32->hist_lut_cfg),
			&ppp->hist_lut_cfg);
	if (ret)
		return ret;
	ret = __from_user_pa_v2_data(
			compat_ptr((uintptr_t)&ppp32->pa_v2_cfg),
			&ppp->pa_v2_cfg);

	return ret;
}

static int __to_user_pp_params(struct mdp_overlay_pp_params *ppp,
				struct mdp_overlay_pp_params32 *ppp32)
{
	int ret = 0;

	if (copy_in_user(&ppp32->config_ops,
			&ppp->config_ops,
			sizeof(uint32_t)))
		return -EFAULT;

	ret = __to_user_csc_cfg(
			compat_ptr((uintptr_t)&ppp32->csc_cfg),
			&ppp->csc_cfg);
	if (ret)
		return ret;
	ret = __to_user_qseed_cfg(
			compat_ptr((uintptr_t)&ppp32->qseed_cfg[0]),
			&ppp->qseed_cfg[0]);
	if (ret)
		return ret;
	ret = __to_user_qseed_cfg(
			compat_ptr((uintptr_t)&ppp32->qseed_cfg[1]),
			&ppp->qseed_cfg[1]);
	if (ret)
		return ret;
	ret = __to_user_pa_cfg(
			compat_ptr((uintptr_t)&ppp32->pa_cfg),
			&ppp->pa_cfg);
	if (ret)
		return ret;
	ret = __to_user_igc_lut_data(
			compat_ptr((uintptr_t)&ppp32->igc_cfg),
			&ppp->igc_cfg);
	if (ret)
		return ret;
	ret = __to_user_sharp_cfg(
			compat_ptr((uintptr_t)&ppp32->sharp_cfg),
			&ppp->sharp_cfg);
	if (ret)
		return ret;
	ret = __to_user_histogram_cfg(
			compat_ptr((uintptr_t)&ppp32->hist_cfg),
			&ppp->hist_cfg);
	if (ret)
		return ret;
	ret = __to_user_hist_lut_data(
			compat_ptr((uintptr_t)&ppp32->hist_lut_cfg),
			&ppp->hist_lut_cfg);
	if (ret)
		return ret;
	ret = __to_user_pa_v2_data(
			compat_ptr((uintptr_t)&ppp32->pa_v2_cfg),
			&ppp->pa_v2_cfg);

	return ret;
}

static int __from_user_hist_start_req(
			struct mdp_histogram_start_req32 __user *hist_req32,
			struct mdp_histogram_start_req __user *hist_req)
{
	if (copy_in_user(&hist_req->block,
			&hist_req32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_req->frame_cnt,
			&hist_req32->frame_cnt,
			sizeof(uint8_t)) ||
	    copy_in_user(&hist_req->bit_mask,
			&hist_req32->bit_mask,
			sizeof(uint8_t)) ||
	    copy_in_user(&hist_req->num_bins,
			&hist_req32->num_bins,
			sizeof(uint16_t)))
		return -EFAULT;

	return 0;
}

static int __from_user_hist_data(
			struct mdp_histogram_data32 __user *hist_data32,
			struct mdp_histogram_data __user *hist_data)
{
	uint32_t data;

	if (copy_in_user(&hist_data->block,
			&hist_data32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_data->bin_cnt,
			&hist_data32->bin_cnt,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, &hist_data32->c0) ||
	    put_user(compat_ptr(data), &hist_data->c0) ||
	    get_user(data, &hist_data32->c1) ||
	    put_user(compat_ptr(data), &hist_data->c1) ||
	    get_user(data, &hist_data32->c2) ||
	    put_user(compat_ptr(data), &hist_data->c2) ||
	    get_user(data, &hist_data32->extra_info) ||
	    put_user(compat_ptr(data), &hist_data->extra_info))
		return -EFAULT;

	return 0;
}

static int __to_user_hist_data(
			struct mdp_histogram_data32 __user *hist_data32,
			struct mdp_histogram_data __user *hist_data)
{
	unsigned long data;

	if (copy_in_user(&hist_data32->block,
			&hist_data->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_data32->bin_cnt,
			&hist_data->bin_cnt,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, (unsigned long *) &hist_data->c0) ||
	    put_user((compat_caddr_t) data, &hist_data32->c0) ||
	    get_user(data, (unsigned long *) &hist_data->c1) ||
	    put_user((compat_caddr_t) data, &hist_data32->c1) ||
	    get_user(data, (unsigned long *) &hist_data->c2) ||
	    put_user((compat_caddr_t) data, &hist_data32->c2) ||
	    get_user(data, (unsigned long *) &hist_data->extra_info) ||
	    put_user((compat_caddr_t) data, &hist_data32->extra_info))
		return -EFAULT;

	return 0;
}

static int mdss_histo_compat_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg, struct file *file)
{
	struct mdp_histogram_data __user *hist;
	struct mdp_histogram_data32 __user *hist32;
	struct mdp_histogram_start_req __user *hist_req;
	struct mdp_histogram_start_req32 __user *hist_req32;
	int ret = 0;

	switch (cmd) {
	case MSMFB_HISTOGRAM_START:
		hist_req32 = compat_ptr(arg);
		hist_req = compat_alloc_user_space(
				sizeof(struct mdp_histogram_start_req));
		if (!hist_req) {
			pr_err("%s:%u: compat alloc error [%zu] bytes\n",
				 __func__, __LINE__,
				 sizeof(struct mdp_histogram_start_req));
			return -EINVAL;
		}
		memset(hist_req, 0, sizeof(struct mdp_histogram_start_req));
		ret = __from_user_hist_start_req(hist_req32, hist_req);
		if (ret)
			goto histo_compat_err;
		ret = mdss_fb_do_ioctl(info, cmd,
			(unsigned long) hist_req, file);
		break;
	case MSMFB_HISTOGRAM_STOP:
		ret = mdss_fb_do_ioctl(info, cmd, arg, file);
		break;
	case MSMFB_HISTOGRAM:
		hist32 = compat_ptr(arg);
		hist = compat_alloc_user_space(
				sizeof(struct mdp_histogram_data));
		if (!hist) {
			pr_err("%s:%u: compat alloc error [%zu] bytes\n",
				 __func__, __LINE__,
				 sizeof(struct mdp_histogram_data));
			return -EINVAL;
		}
		memset(hist, 0, sizeof(struct mdp_histogram_data));
		ret = __from_user_hist_data(hist32, hist);
		if (ret)
			goto histo_compat_err;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) hist, file);
		if (ret)
			goto histo_compat_err;
		ret = __to_user_hist_data(hist32, hist);
		break;
	default:
		break;
	}

histo_compat_err:
	return ret;
}

static int __copy_layer_pp_info_qseed_params(
			struct mdp_overlay_pp_params *pp_info,
			struct mdp_overlay_pp_params32 *pp_info32)
{
	pp_info->qseed_cfg[0].table_num = pp_info32->qseed_cfg[0].table_num;
	pp_info->qseed_cfg[0].ops = pp_info32->qseed_cfg[0].ops;
	pp_info->qseed_cfg[0].len = pp_info32->qseed_cfg[0].len;
	pp_info->qseed_cfg[0].data = compat_ptr(pp_info32->qseed_cfg[0].data);

	pp_info->qseed_cfg[1].table_num = pp_info32->qseed_cfg[1].table_num;
	pp_info->qseed_cfg[1].ops = pp_info32->qseed_cfg[1].ops;
	pp_info->qseed_cfg[1].len = pp_info32->qseed_cfg[1].len;
	pp_info->qseed_cfg[1].data = compat_ptr(pp_info32->qseed_cfg[1].data);

	return 0;
}

static int __copy_layer_igc_lut_data_v1_7(
			struct mdp_igc_lut_data_v1_7 *cfg_payload,
			struct mdp_igc_lut_data_v1_7_32 __user *cfg_payload32)
{
	struct mdp_igc_lut_data_v1_7_32 local_cfg_payload32;
	int ret = 0;

	ret = copy_from_user(&local_cfg_payload32,
			cfg_payload32,
			sizeof(struct mdp_igc_lut_data_v1_7_32));
	if (ret) {
		pr_err("copy from user failed, IGC cfg payload = %p\n",
			cfg_payload32);
		ret = -EFAULT;
		goto exit;
	}

	cfg_payload->table_fmt = local_cfg_payload32.table_fmt;
	cfg_payload->len = local_cfg_payload32.len;
	cfg_payload->c0_c1_data = compat_ptr(local_cfg_payload32.c0_c1_data);
	cfg_payload->c2_data = compat_ptr(local_cfg_payload32.c2_data);

exit:
	return ret;
}

static int __copy_layer_pp_info_igc_params(
			struct mdp_overlay_pp_params *pp_info,
			struct mdp_overlay_pp_params32 *pp_info32)
{
	void *cfg_payload = NULL;
	uint32_t payload_size = 0;
	int ret = 0;

	pp_info->igc_cfg.block = pp_info32->igc_cfg.block;
	pp_info->igc_cfg.version = pp_info32->igc_cfg.version;
	pp_info->igc_cfg.ops = pp_info32->igc_cfg.ops;

	if (pp_info->igc_cfg.version != 0) {
		payload_size = __pp_compat_size_igc();

		cfg_payload = kmalloc(payload_size, GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}
	}

	switch (pp_info->igc_cfg.version) {
	case mdp_igc_v1_7:
		ret = __copy_layer_igc_lut_data_v1_7(cfg_payload,
				compat_ptr(pp_info32->igc_cfg.cfg_payload));
		if (ret) {
			pr_err("compat copy of IGC cfg payload failed, ret %d\n",
				ret);
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("No version set, fallback to legacy IGC version\n");
		pp_info->igc_cfg.len = pp_info32->igc_cfg.len;
		pp_info->igc_cfg.c0_c1_data =
			compat_ptr(pp_info32->igc_cfg.c0_c1_data);
		pp_info->igc_cfg.c2_data =
			compat_ptr(pp_info32->igc_cfg.c2_data);
		cfg_payload = NULL;
		break;
	}
exit:
	pp_info->igc_cfg.cfg_payload = cfg_payload;
	return ret;
}

static int __copy_layer_hist_lut_data_v1_7(
			struct mdp_hist_lut_data_v1_7 *cfg_payload,
			struct mdp_hist_lut_data_v1_7_32 __user *cfg_payload32)
{
	struct mdp_hist_lut_data_v1_7_32 local_cfg_payload32;
	int ret = 0;

	ret = copy_from_user(&local_cfg_payload32,
			cfg_payload32,
			sizeof(struct mdp_hist_lut_data_v1_7_32));
	if (ret) {
		pr_err("copy from user failed, hist lut cfg_payload = %p\n",
			cfg_payload32);
		ret = -EFAULT;
		goto exit;
	}

	cfg_payload->len = local_cfg_payload32.len;
	cfg_payload->data = compat_ptr(local_cfg_payload32.data);
exit:
	return ret;
}

static int __copy_layer_pp_info_hist_lut_params(
			struct mdp_overlay_pp_params *pp_info,
			struct mdp_overlay_pp_params32 *pp_info32)
{
	void *cfg_payload = NULL;
	uint32_t payload_size = 0;
	int ret = 0;

	pp_info->hist_lut_cfg.block = pp_info32->hist_lut_cfg.block;
	pp_info->hist_lut_cfg.version = pp_info32->hist_lut_cfg.version;
	pp_info->hist_lut_cfg.ops = pp_info32->hist_lut_cfg.ops;
	pp_info->hist_lut_cfg.hist_lut_first =
			pp_info32->hist_lut_cfg.hist_lut_first;

	if (pp_info->hist_lut_cfg.version != 0) {
		payload_size = __pp_compat_size_hist_lut();

		cfg_payload = kmalloc(payload_size, GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}
	}

	switch (pp_info->hist_lut_cfg.version) {
	case mdp_hist_lut_v1_7:
		ret = __copy_layer_hist_lut_data_v1_7(cfg_payload,
			compat_ptr(pp_info32->hist_lut_cfg.cfg_payload));
		if (ret) {
			pr_err("compat copy of Hist LUT cfg payload failed, ret %d\n",
				ret);
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("version invalid, fallback to legacy\n");
		pp_info->hist_lut_cfg.len = pp_info32->hist_lut_cfg.len;
		pp_info->hist_lut_cfg.data =
				compat_ptr(pp_info32->hist_lut_cfg.data);
		cfg_payload = NULL;
		break;
	}
exit:
	pp_info->hist_lut_cfg.cfg_payload = cfg_payload;
	return ret;
}

static int __copy_layer_pa_data_v1_7(
			struct mdp_pa_data_v1_7 *cfg_payload,
			struct mdp_pa_data_v1_7_32 __user *cfg_payload32)
{
	struct mdp_pa_data_v1_7_32 local_cfg_payload32;
	int ret = 0;

	ret = copy_from_user(&local_cfg_payload32,
			cfg_payload32,
			sizeof(struct mdp_pa_data_v1_7_32));
	if (ret) {
		pr_err("copy from user failed, pa cfg_payload = %p\n",
			cfg_payload32);
		ret = -EFAULT;
		goto exit;
	}

	cfg_payload->mode = local_cfg_payload32.mode;
	cfg_payload->global_hue_adj = local_cfg_payload32.global_hue_adj;
	cfg_payload->global_sat_adj = local_cfg_payload32.global_sat_adj;
	cfg_payload->global_val_adj = local_cfg_payload32.global_val_adj;
	cfg_payload->global_cont_adj = local_cfg_payload32.global_cont_adj;

	memcpy(&cfg_payload->skin_cfg, &local_cfg_payload32.skin_cfg,
			sizeof(struct mdp_pa_mem_col_data_v1_7));
	memcpy(&cfg_payload->sky_cfg, &local_cfg_payload32.sky_cfg,
			sizeof(struct mdp_pa_mem_col_data_v1_7));
	memcpy(&cfg_payload->fol_cfg, &local_cfg_payload32.fol_cfg,
			sizeof(struct mdp_pa_mem_col_data_v1_7));

	cfg_payload->six_zone_thresh = local_cfg_payload32.six_zone_thresh;
	cfg_payload->six_zone_adj_p0 = local_cfg_payload32.six_zone_adj_p0;
	cfg_payload->six_zone_adj_p1 = local_cfg_payload32.six_zone_adj_p1;
	cfg_payload->six_zone_sat_hold = local_cfg_payload32.six_zone_sat_hold;
	cfg_payload->six_zone_val_hold = local_cfg_payload32.six_zone_val_hold;
	cfg_payload->six_zone_len = local_cfg_payload32.six_zone_len;

	cfg_payload->six_zone_curve_p0 =
			compat_ptr(local_cfg_payload32.six_zone_curve_p0);
	cfg_payload->six_zone_curve_p1 =
			compat_ptr(local_cfg_payload32.six_zone_curve_p1);
exit:
	return ret;
}

static int __copy_layer_pp_info_pa_v2_params(
			struct mdp_overlay_pp_params *pp_info,
			struct mdp_overlay_pp_params32 *pp_info32)
{
	void *cfg_payload = NULL;
	uint32_t payload_size = 0;
	int ret = 0;

	pp_info->pa_v2_cfg_data.block = pp_info32->pa_v2_cfg_data.block;
	pp_info->pa_v2_cfg_data.version = pp_info32->pa_v2_cfg_data.version;
	pp_info->pa_v2_cfg_data.flags = pp_info32->pa_v2_cfg_data.flags;

	if (pp_info->pa_v2_cfg_data.version != 0) {
		payload_size = __pp_compat_size_pa();

		cfg_payload = kmalloc(payload_size, GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}
	}

	switch (pp_info->pa_v2_cfg_data.version) {
	case mdp_pa_v1_7:
		ret = __copy_layer_pa_data_v1_7(cfg_payload,
			compat_ptr(pp_info32->pa_v2_cfg_data.cfg_payload));
		if (ret) {
			pr_err("compat copy of PA cfg payload failed, ret %d\n",
				ret);
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("version invalid\n");
		cfg_payload = NULL;
		break;
	}
exit:
	pp_info->pa_v2_cfg_data.cfg_payload = cfg_payload;
	return ret;
}

static int __copy_layer_pp_info_legacy_pa_v2_params(
			struct mdp_overlay_pp_params *pp_info,
			struct mdp_overlay_pp_params32 *pp_info32)
{
	pp_info->pa_v2_cfg.global_hue_adj =
		pp_info32->pa_v2_cfg.global_hue_adj;
	pp_info->pa_v2_cfg.global_sat_adj =
		pp_info32->pa_v2_cfg.global_sat_adj;
	pp_info->pa_v2_cfg.global_val_adj =
		pp_info32->pa_v2_cfg.global_val_adj;
	pp_info->pa_v2_cfg.global_cont_adj =
		pp_info32->pa_v2_cfg.global_cont_adj;

	memcpy(&pp_info->pa_v2_cfg.skin_cfg,
			&pp_info32->pa_v2_cfg.skin_cfg,
			sizeof(struct mdp_pa_mem_col_cfg));
	memcpy(&pp_info->pa_v2_cfg.sky_cfg,
			&pp_info32->pa_v2_cfg.sky_cfg,
			sizeof(struct mdp_pa_mem_col_cfg));
	memcpy(&pp_info->pa_v2_cfg.fol_cfg,
			&pp_info32->pa_v2_cfg.fol_cfg,
			sizeof(struct mdp_pa_mem_col_cfg));

	pp_info->pa_v2_cfg.six_zone_thresh =
		pp_info32->pa_v2_cfg.six_zone_thresh;
	pp_info->pa_v2_cfg.six_zone_len =
		pp_info32->pa_v2_cfg.six_zone_len;

	pp_info->pa_v2_cfg.six_zone_curve_p0 =
		compat_ptr(pp_info32->pa_v2_cfg.six_zone_curve_p0);
	pp_info->pa_v2_cfg.six_zone_curve_p1 =
		compat_ptr(pp_info32->pa_v2_cfg.six_zone_curve_p1);

	return 0;
}

static int __copy_layer_pp_info_pcc_params(
			struct mdp_overlay_pp_params *pp_info,
			struct mdp_overlay_pp_params32 *pp_info32)
{
	void *cfg_payload = NULL;
	uint32_t payload_size = 0;
	int ret = 0;

	pp_info->pcc_cfg_data.block = pp_info32->pcc_cfg_data.block;
	pp_info->pcc_cfg_data.version = pp_info32->pcc_cfg_data.version;
	pp_info->pcc_cfg_data.ops = pp_info32->pcc_cfg_data.ops;

	if (pp_info->pcc_cfg_data.version != 0) {
		payload_size = __pp_compat_size_pcc();

		cfg_payload = kmalloc(payload_size, GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}
	}

	switch (pp_info->pcc_cfg_data.version) {
	case mdp_pcc_v1_7:
		ret = copy_from_user(cfg_payload,
			compat_ptr(pp_info32->pcc_cfg_data.cfg_payload),
			sizeof(struct mdp_pcc_data_v1_7));
		if (ret) {
			pr_err("compat copy of PCC cfg payload failed, ptr %p\n",
				compat_ptr(
				pp_info32->pcc_cfg_data.cfg_payload));
			ret = -EFAULT;
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("version invalid, fallback to legacy\n");
		cfg_payload = NULL;
		break;
	}
exit:
	pp_info->pcc_cfg_data.cfg_payload = cfg_payload;
	return ret;
}


static int __copy_layer_pp_info_params(struct mdp_input_layer *layer,
				struct mdp_input_layer32 *layer32)
{
	struct mdp_overlay_pp_params *pp_info;
	struct mdp_overlay_pp_params32 pp_info32;
	int ret = 0;

	if (!(layer->flags & MDP_LAYER_PP))
		return 0;

	ret = copy_from_user(&pp_info32,
			compat_ptr(layer32->pp_info),
			sizeof(struct mdp_overlay_pp_params32));
	if (ret) {
		pr_err("pp info copy from user failed, pp_info %p\n",
			compat_ptr(layer32->pp_info));
		ret = -EFAULT;
		goto exit;
	}

	pp_info = kmalloc(sizeof(struct mdp_overlay_pp_params), GFP_KERNEL);
	if (!pp_info) {
		ret = -ENOMEM;
		goto exit;
	}
	memset(pp_info, 0, sizeof(struct mdp_overlay_pp_params));

	pp_info->config_ops = pp_info32.config_ops;

	memcpy(&pp_info->csc_cfg, &pp_info32.csc_cfg,
		sizeof(struct mdp_csc_cfg));
	memcpy(&pp_info->sharp_cfg, &pp_info32.sharp_cfg,
		sizeof(struct mdp_sharp_cfg));
	memcpy(&pp_info->hist_cfg, &pp_info32.hist_cfg,
		sizeof(struct mdp_histogram_cfg));
	memcpy(&pp_info->pa_cfg, &pp_info32.pa_cfg,
		sizeof(struct mdp_pa_cfg));

	ret = __copy_layer_pp_info_qseed_params(pp_info, &pp_info32);
	if (ret) {
		pr_err("compat copy pp_info QSEED params failed, ret %d\n",
			ret);
		goto exit_pp_info;
	}
	ret = __copy_layer_pp_info_legacy_pa_v2_params(pp_info, &pp_info32);
	if (ret) {
		pr_err("compat copy pp_info Legacy PAv2 params failed, ret %d\n",
			ret);
		goto exit_pp_info;
	}
	ret = __copy_layer_pp_info_igc_params(pp_info, &pp_info32);
	if (ret) {
		pr_err("compat copy pp_info IGC params failed, ret %d\n",
			ret);
		goto exit_pp_info;
	}
	ret = __copy_layer_pp_info_hist_lut_params(pp_info, &pp_info32);
	if (ret) {
		pr_err("compat copy pp_info Hist LUT params failed, ret %d\n",
			ret);
		goto exit_igc;
	}
	ret = __copy_layer_pp_info_pa_v2_params(pp_info, &pp_info32);
	if (ret) {
		pr_err("compat copy pp_info PAv2 params failed, ret %d\n",
			ret);
		goto exit_hist_lut;
	}
	ret = __copy_layer_pp_info_pcc_params(pp_info, &pp_info32);
	if (ret) {
		pr_err("compat copy pp_info PCC params failed, ret %d\n",
			ret);
		goto exit_pa;
	}

	layer->pp_info = pp_info;

	return ret;

exit_pa:
	kfree(pp_info->pa_v2_cfg_data.cfg_payload);
exit_hist_lut:
	kfree(pp_info->hist_lut_cfg.cfg_payload);
exit_igc:
	kfree(pp_info->igc_cfg.cfg_payload);
exit_pp_info:
	kfree(pp_info);
exit:
	return ret;
}


static int __to_user_mdp_overlay(struct mdp_overlay32 __user *ov32,
				 struct mdp_overlay __user *ov)
{
	int ret = 0;

	ret = copy_in_user(&ov32->src, &ov->src, sizeof(ov32->src)) ||
		copy_in_user(&ov32->src_rect,
			&ov->src_rect, sizeof(ov32->src_rect)) ||
		copy_in_user(&ov32->dst_rect,
			&ov->dst_rect, sizeof(ov32->dst_rect));
	if (ret)
		return -EFAULT;

	ret |= put_user(ov->z_order, &ov32->z_order);
	ret |= put_user(ov->is_fg, &ov32->is_fg);
	ret |= put_user(ov->alpha, &ov32->alpha);
	ret |= put_user(ov->blend_op, &ov32->blend_op);
	ret |= put_user(ov->transp_mask, &ov32->transp_mask);
	ret |= put_user(ov->flags, &ov32->flags);
	ret |= put_user(ov->id, &ov32->id);
	ret |= put_user(ov->priority, &ov32->priority);
	if (ret)
		return -EFAULT;

	ret = copy_in_user(&ov32->user_data, &ov->user_data,
		     sizeof(ov32->user_data));
	if (ret)
		return -EFAULT;

	ret |= put_user(ov->horz_deci, &ov32->horz_deci);
	ret |= put_user(ov->vert_deci, &ov32->vert_deci);
	if (ret)
		return -EFAULT;

	ret = __to_user_pp_params(
			&ov->overlay_pp_cfg,
			compat_ptr((uintptr_t) &ov32->overlay_pp_cfg));
	if (ret)
		return -EFAULT;

	ret = copy_in_user(&ov32->scale, &ov->scale,
			   sizeof(struct mdp_scale_data));
	if (ret)
		return -EFAULT;

	ret = put_user(ov->frame_rate, &ov32->frame_rate);
	if (ret)
		return -EFAULT;

	return 0;
}


static int __from_user_mdp_overlay(struct mdp_overlay *ov,
				   struct mdp_overlay32 __user *ov32)
{
	__u32 data;

	if (copy_in_user(&ov->src, &ov32->src,
			 sizeof(ov32->src)) ||
	    copy_in_user(&ov->src_rect, &ov32->src_rect,
			 sizeof(ov32->src_rect)) ||
	    copy_in_user(&ov->dst_rect, &ov32->dst_rect,
			 sizeof(ov32->dst_rect)))
		return -EFAULT;

	if (get_user(data, &ov32->z_order) ||
	    put_user(data, &ov->z_order) ||
	    get_user(data, &ov32->is_fg) ||
	    put_user(data, &ov->is_fg) ||
	    get_user(data, &ov32->alpha) ||
	    put_user(data, &ov->alpha) ||
	    get_user(data, &ov32->blend_op) ||
	    put_user(data, &ov->blend_op) ||
	    get_user(data, &ov32->transp_mask) ||
	    put_user(data, &ov->transp_mask) ||
	    get_user(data, &ov32->flags) ||
	    put_user(data, &ov->flags) ||
	    get_user(data, &ov32->pipe_type) ||
	    put_user(data, &ov->pipe_type) ||
	    get_user(data, &ov32->id) ||
	    put_user(data, &ov->id) ||
	    get_user(data, &ov32->priority) ||
	    put_user(data, &ov->priority))
		return -EFAULT;

	if (copy_in_user(&ov->user_data, &ov32->user_data,
			 sizeof(ov32->user_data)))
		return -EFAULT;

	if (get_user(data, &ov32->horz_deci) ||
	    put_user(data, &ov->horz_deci) ||
	    get_user(data, &ov32->vert_deci) ||
	    put_user(data, &ov->vert_deci))
		return -EFAULT;

	if (__from_user_pp_params(
			compat_ptr((uintptr_t) &ov32->overlay_pp_cfg),
			&ov->overlay_pp_cfg))
		return -EFAULT;

	if (copy_in_user(&ov->scale, &ov32->scale,
			 sizeof(struct mdp_scale_data)))
		return -EFAULT;

	if (get_user(data, &ov32->frame_rate) ||
	    put_user(data, &ov->frame_rate))
		return -EFAULT;

	return 0;
}

static int __from_user_mdp_overlaylist(struct mdp_overlay_list *ovlist,
				   struct mdp_overlay_list32 *ovlist32,
				   struct mdp_overlay **to_list_head)
{
	__u32 i, ret;
	unsigned long data, from_list_head;
	struct mdp_overlay32 *iter;

	if (!to_list_head || !ovlist32 || !ovlist) {
		pr_err("%s:%u: null error\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (copy_in_user(&ovlist->num_overlays, &ovlist32->num_overlays,
			 sizeof(ovlist32->num_overlays)))
		return -EFAULT;

	if (copy_in_user(&ovlist->flags, &ovlist32->flags,
			 sizeof(ovlist32->flags)))
		return -EFAULT;

	if (copy_in_user(&ovlist->processed_overlays,
			&ovlist32->processed_overlays,
			 sizeof(ovlist32->processed_overlays)))
		return -EFAULT;

	if (get_user(data, &ovlist32->overlay_list)) {
		ret = -EFAULT;
		goto validate_exit;
	}
	for (i = 0; i < ovlist32->num_overlays; i++) {
		if (get_user(from_list_head, (__u32 *)data + i)) {
			ret = -EFAULT;
			goto validate_exit;
		}

		iter = compat_ptr(from_list_head);
		if (__from_user_mdp_overlay(to_list_head[i],
			       (struct mdp_overlay32 *)(iter))) {
			ret = -EFAULT;
			goto validate_exit;
		}
	}
	ovlist->overlay_list = to_list_head;

	return 0;

validate_exit:
	pr_err("%s: %u: copy error\n", __func__, __LINE__);
	return -EFAULT;
}

static int __to_user_mdp_overlaylist(struct mdp_overlay_list32 *ovlist32,
				   struct mdp_overlay_list *ovlist,
				   struct mdp_overlay **l_ptr)
{
	__u32 i, ret;
	unsigned long data, data1;
	struct mdp_overlay32 *temp;
	struct mdp_overlay *l = l_ptr[0];

	if (copy_in_user(&ovlist32->num_overlays, &ovlist->num_overlays,
			 sizeof(ovlist32->num_overlays)))
		return -EFAULT;

	if (get_user(data, &ovlist32->overlay_list)) {
		ret = -EFAULT;
		pr_err("%s:%u: err\n", __func__, __LINE__);
		goto validate_exit;
	}

	for (i = 0; i < ovlist32->num_overlays; i++) {
		if (get_user(data1, (__u32 *)data + i)) {
			ret = -EFAULT;
			goto validate_exit;
		}
		temp = compat_ptr(data1);
		if (__to_user_mdp_overlay(
				(struct mdp_overlay32 *) temp,
				l + i)) {
			ret = -EFAULT;
			goto validate_exit;
		}
	}

	if (copy_in_user(&ovlist32->flags, &ovlist->flags,
				sizeof(ovlist32->flags)))
		return -EFAULT;

	if (copy_in_user(&ovlist32->processed_overlays,
			&ovlist->processed_overlays,
			sizeof(ovlist32->processed_overlays)))
		return -EFAULT;

	return 0;

validate_exit:
	pr_err("%s: %u: copy error\n", __func__, __LINE__);
	return -EFAULT;

}

void mdss_compat_align_list(void __user *total_mem_chunk,
		struct mdp_overlay __user **list_ptr, u32 num_ov)
{
	int i = 0;
	struct mdp_overlay __user *contig_overlays;

	contig_overlays = total_mem_chunk + sizeof(struct mdp_overlay_list) +
		 (num_ov * sizeof(struct mdp_overlay *));

	for (i = 0; i < num_ov; i++)
		list_ptr[i] = contig_overlays + i;
}

static u32 __pp_sspp_size(void)
{
	u32 size = 0;
	/* pick the largest of the revision when multiple revs are supported */
	size = sizeof(struct mdp_igc_lut_data_v1_7);
	size += sizeof(struct mdp_pa_data_v1_7);
	size += sizeof(struct mdp_pcc_data_v1_7);
	size += sizeof(struct mdp_hist_lut_data_v1_7);
	return size;
}

static int __pp_sspp_set_offsets(struct mdp_overlay *ov)
{
	if (!ov) {
		pr_err("invalid overlay pointer\n");
		return -EFAULT;
	}
	ov->overlay_pp_cfg.igc_cfg.cfg_payload = (void *)((unsigned long)ov +
				sizeof(struct mdp_overlay));
	ov->overlay_pp_cfg.pa_v2_cfg_data.cfg_payload =
		ov->overlay_pp_cfg.igc_cfg.cfg_payload +
		sizeof(struct mdp_igc_lut_data_v1_7);
	ov->overlay_pp_cfg.pcc_cfg_data.cfg_payload =
		ov->overlay_pp_cfg.pa_v2_cfg_data.cfg_payload +
		sizeof(struct mdp_pa_data_v1_7);
	ov->overlay_pp_cfg.hist_lut_cfg.cfg_payload =
		ov->overlay_pp_cfg.pcc_cfg_data.cfg_payload +
		sizeof(struct mdp_pcc_data_v1_7);
	return 0;
}

int mdss_compat_overlay_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg, struct file *file)
{
	struct mdp_overlay *ov, **layers_head;
	struct mdp_overlay32 *ov32;
	struct mdp_overlay_list __user *ovlist;
	struct mdp_overlay_list32 __user *ovlist32;
	size_t layers_refs_sz, layers_sz, prepare_sz;
	void __user *total_mem_chunk;
	uint32_t num_overlays;
	uint32_t alloc_size = 0;
	int ret;

	if (!info || !info->par)
		return -EINVAL;


	switch (cmd) {
	case MSMFB_MDP_PP:
		ret = mdss_compat_pp_ioctl(info, cmd, arg, file);
		break;
	case MSMFB_HISTOGRAM_START:
	case MSMFB_HISTOGRAM_STOP:
	case MSMFB_HISTOGRAM:
		ret = mdss_histo_compat_ioctl(info, cmd, arg, file);
		break;
	case MSMFB_OVERLAY_GET:
		alloc_size += sizeof(*ov) + __pp_sspp_size();
		ov = compat_alloc_user_space(alloc_size);
		if (!ov) {
			pr_err("%s:%u: compat alloc error [%zu] bytes\n",
				 __func__, __LINE__, sizeof(*ov));
			return -EINVAL;
		}
		ov32 = compat_ptr(arg);
		ret = __pp_sspp_set_offsets(ov);
		if (ret) {
			pr_err("setting the pp offsets failed ret %d\n", ret);
			return ret;
		}
		ret = __from_user_mdp_overlay(ov, ov32);
		if (ret)
			pr_err("%s: compat mdp overlay failed\n", __func__);
		else
			ret = mdss_fb_do_ioctl(info, cmd,
				(unsigned long) ov, file);
		ret = __to_user_mdp_overlay(ov32, ov);
		break;
	case MSMFB_OVERLAY_SET:
		alloc_size += sizeof(*ov) + __pp_sspp_size();
		ov = compat_alloc_user_space(alloc_size);
		if (!ov) {
			pr_err("%s:%u: compat alloc error [%zu] bytes\n",
				 __func__, __LINE__, sizeof(*ov));
			return -EINVAL;
		}
		ret = __pp_sspp_set_offsets(ov);
		if (ret) {
			pr_err("setting the pp offsets failed ret %d\n", ret);
			return ret;
		}
		ov32 = compat_ptr(arg);
		ret = __from_user_mdp_overlay(ov, ov32);
		if (ret) {
			pr_err("%s: compat mdp overlay failed\n", __func__);
		} else {
			ret = mdss_fb_do_ioctl(info, cmd,
				(unsigned long) ov, file);
			ret = __to_user_mdp_overlay(ov32, ov);
		}
		break;
	case MSMFB_OVERLAY_PREPARE:
		ovlist32 = compat_ptr(arg);
		if (get_user(num_overlays, &ovlist32->num_overlays)) {
			pr_err("compat mdp prepare failed: invalid arg\n");
			return -EFAULT;
		}

		if (num_overlays >= OVERLAY_MAX) {
			pr_err("%s: No: of overlays exceeds max\n", __func__);
			return -EINVAL;
		}

		layers_sz = num_overlays * sizeof(struct mdp_overlay);
		prepare_sz = sizeof(struct mdp_overlay_list);
		layers_refs_sz = num_overlays * sizeof(struct mdp_overlay *);

		total_mem_chunk = compat_alloc_user_space(
			prepare_sz + layers_refs_sz + layers_sz);
		if (!total_mem_chunk) {
			pr_err("%s:%u: compat alloc error [%zu] bytes\n",
				 __func__, __LINE__,
				 layers_refs_sz + layers_sz + prepare_sz);
			return -EINVAL;
		}

		layers_head = total_mem_chunk + prepare_sz;
		mdss_compat_align_list(total_mem_chunk, layers_head,
					num_overlays);
		ovlist = (struct mdp_overlay_list *)total_mem_chunk;

		ret = __from_user_mdp_overlaylist(ovlist, ovlist32,
					layers_head);
		if (ret) {
			pr_err("compat mdp overlaylist failed\n");
		} else {
			ret = mdss_fb_do_ioctl(info, cmd,
				(unsigned long) ovlist, file);
			if (!ret)
				ret = __to_user_mdp_overlaylist(ovlist32,
							 ovlist, layers_head);
		}
		break;
	case MSMFB_OVERLAY_UNSET:
	case MSMFB_OVERLAY_PLAY:
	case MSMFB_OVERLAY_VSYNC_CTRL:
	case MSMFB_METADATA_SET:
	case MSMFB_METADATA_GET:
	default:
		pr_debug("%s: overlay ioctl cmd=[%u]\n", __func__, cmd);
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) arg, file);
		break;
	}
	return ret;
}

/*
 * mdss_fb_compat_ioctl() - MDSS Framebuffer compat ioctl function
 * @info:	pointer to framebuffer info
 * @cmd:	ioctl command
 * @arg:	argument to ioctl
 *
 * This function adds the compat translation layer for framebuffer
 * ioctls to allow 32-bit userspace call ioctls on the mdss
 * framebuffer device driven in 64-bit kernel.
 */
int mdss_fb_compat_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg, struct file *file)
{
	int ret;

	if (!info || !info->par)
		return -EINVAL;

	cmd = __do_compat_ioctl_nr(cmd);
	switch (cmd) {
	case MSMFB_CURSOR:
		ret = mdss_fb_compat_cursor(info, cmd, arg, file);
		break;
	case MSMFB_SET_LUT:
		ret = mdss_fb_compat_set_lut(info, arg, file);
		break;
	case MSMFB_BUFFER_SYNC:
		ret = mdss_fb_compat_buf_sync(info, cmd, arg, file);
		break;
	case MSMFB_ATOMIC_COMMIT:
		ret = __compat_atomic_commit(info, cmd, arg, file);
		break;
	case MSMFB_ASYNC_POSITION_UPDATE:
		ret = __compat_async_position_update(info, cmd, arg);
		break;
	case MSMFB_MDP_PP:
	case MSMFB_HISTOGRAM_START:
	case MSMFB_HISTOGRAM_STOP:
	case MSMFB_HISTOGRAM:
	case MSMFB_OVERLAY_GET:
	case MSMFB_OVERLAY_SET:
	case MSMFB_OVERLAY_UNSET:
	case MSMFB_OVERLAY_PLAY:
	case MSMFB_OVERLAY_VSYNC_CTRL:
	case MSMFB_METADATA_SET:
	case MSMFB_METADATA_GET:
	case MSMFB_OVERLAY_PREPARE:
		ret = mdss_compat_overlay_ioctl(info, cmd, arg, file);
		break;
	case MSMFB_NOTIFY_UPDATE:
	case MSMFB_DISPLAY_COMMIT:
	default:
		ret = mdss_fb_do_ioctl(info, cmd, arg, file);
		break;
	}

	if (ret == -ENOSYS)
		pr_err("%s: unsupported ioctl\n", __func__);
	else if (ret)
		pr_debug("%s: ioctl err cmd=%u ret=%d\n", __func__, cmd, ret);

	return ret;
}
EXPORT_SYMBOL(mdss_fb_compat_ioctl);
