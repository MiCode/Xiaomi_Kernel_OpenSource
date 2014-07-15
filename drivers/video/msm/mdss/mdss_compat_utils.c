/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

static unsigned int __do_compat_ioctl_nr(unsigned int cmd32)
{
	unsigned int cmd;

	switch (cmd32) {
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
	default:
		cmd = cmd32;
		break;
	}

	return cmd;
}

static int mdss_fb_compat_buf_sync(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	struct mdp_buf_sync32 __user *buf_sync32;
	struct mdp_buf_sync __user *buf_sync;
	u32 data;
	int ret;

	buf_sync = compat_alloc_user_space(sizeof(*buf_sync));
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

	ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) buf_sync);
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

static int mdss_fb_compat_set_lut(struct fb_info *info, unsigned long arg)
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

	ret = mdss_fb_do_ioctl(info, MSMFB_SET_LUT, (unsigned long) cmap);
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

static int __from_user_pcc_cfg_data(
			struct mdp_pcc_cfg_data32 __user *pcc_cfg32,
			struct mdp_pcc_cfg_data __user *pcc_cfg)
{
	if (copy_in_user(&pcc_cfg->block,
			&pcc_cfg32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_cfg->ops,
			&pcc_cfg32->ops,
			sizeof(uint32_t)))
		return -EFAULT;

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

	return 0;
}

static int __to_user_pcc_cfg_data(
			struct mdp_pcc_cfg_data32 __user *pcc_cfg32,
			struct mdp_pcc_cfg_data __user *pcc_cfg)
{
	if (copy_in_user(&pcc_cfg32->block,
			&pcc_cfg->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&pcc_cfg32->ops,
			&pcc_cfg->ops,
			sizeof(uint32_t)))
		return -EFAULT;

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

static int __from_user_igc_lut_data(
		struct mdp_igc_lut_data32 __user *igc_lut32,
		struct mdp_igc_lut_data __user *igc_lut)
{
	uint32_t data;

	if (copy_in_user(&igc_lut->block,
			&igc_lut32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut->len,
			&igc_lut32->len,
			sizeof(uint32_t)) ||
	    copy_in_user(&igc_lut->ops,
			&igc_lut32->ops,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, &igc_lut32->c0_c1_data) ||
	    put_user(compat_ptr(data), &igc_lut->c0_c1_data) ||
	    get_user(data, &igc_lut32->c2_data) ||
	    put_user(compat_ptr(data), &igc_lut->c2_data))
		return -EFAULT;

	return 0;
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

static int __from_user_pgc_lut_data(
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
			sizeof(uint8_t)))
		return -EFAULT;

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

static int __from_user_hist_lut_data(
			struct mdp_hist_lut_data32 __user *hist_lut32,
			struct mdp_hist_lut_data __user *hist_lut)
{
	uint32_t data;

	if (copy_in_user(&hist_lut->block,
			&hist_lut32->block,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut->ops,
			&hist_lut32->ops,
			sizeof(uint32_t)) ||
	    copy_in_user(&hist_lut->len,
			&hist_lut32->len,
			sizeof(uint32_t)))
		return -EFAULT;

	if (get_user(data, &hist_lut32->data) ||
	    put_user(compat_ptr(data), &hist_lut->data))
		return -EFAULT;

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
	int ret;

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

static int __from_user_pa_v2_cfg_data(
			struct mdp_pa_v2_cfg_data32 __user *pa_v2_cfg32,
			struct mdp_pa_v2_cfg_data __user *pa_v2_cfg)
{
	if (copy_in_user(&pa_v2_cfg->block,
			&pa_v2_cfg32->block,
			sizeof(uint32_t)))
		return -EFAULT;

	if (__from_user_pa_v2_data(
			compat_ptr((uintptr_t)&pa_v2_cfg32->pa_v2_data),
			&pa_v2_cfg->pa_v2_data))
		return -EFAULT;

	return 0;
}

static int __to_user_pa_v2_cfg_data(
			struct mdp_pa_v2_cfg_data32 __user *pa_v2_cfg32,
			struct mdp_pa_v2_cfg_data __user *pa_v2_cfg)
{
	if (copy_in_user(&pa_v2_cfg32->block,
			&pa_v2_cfg->block,
			sizeof(uint32_t)))
		return -EFAULT;

	if (__to_user_pa_v2_data(
			compat_ptr((uintptr_t)&pa_v2_cfg32->pa_v2_data),
			&pa_v2_cfg->pa_v2_data))
		return -EFAULT;

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

static int __from_user_gamut_cfg_data(
			struct mdp_gamut_cfg_data32 __user *gamut_cfg32,
			struct mdp_gamut_cfg_data __user *gamut_cfg)
{
	uint32_t data;
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
			MDP_GAMUT_TABLE_NUM * sizeof(uint32_t)))
		return 0;

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

static int __pp_compat_alloc(struct msmfb_mdp_pp32 __user *pp32,
					struct msmfb_mdp_pp __user **pp,
					uint32_t op)
{
	uint32_t alloc_size = 0, lut_type, r_size, g_size, b_size;
	struct mdp_pgc_lut_data32 __user *pgc_data32;
	uint8_t num_r_stages, num_g_stages, num_b_stages;

	alloc_size = sizeof(struct msmfb_mdp_pp);

	if (op == mdp_op_lut_cfg) {
		if (copy_from_user(&lut_type,
			&pp32->data.lut_cfg_data.lut_type,
			sizeof(uint32_t)))
			return -EFAULT;

		if (lut_type == mdp_lut_pgc) {
			pgc_data32 = compat_ptr((uintptr_t)
				&pp32->data.lut_cfg_data.data.pgc_lut_data);
			if (copy_from_user(&num_r_stages,
				&pgc_data32->num_r_stages,
				sizeof(uint8_t)) ||
			    copy_from_user(&num_g_stages,
				&pgc_data32->num_g_stages,
				sizeof(uint8_t)) ||
			    copy_from_user(&num_b_stages,
				&pgc_data32->num_b_stages,
				sizeof(uint8_t)))
				return -EFAULT;

			if (num_r_stages > GC_LUT_SEGMENTS ||
			    num_g_stages > GC_LUT_SEGMENTS ||
			    num_b_stages > GC_LUT_SEGMENTS ||
			    num_r_stages <= 0 ||
			    num_g_stages <= 0 ||
			    num_b_stages <= 0)
				return -EINVAL;

			r_size = num_r_stages *
				sizeof(struct mdp_ar_gc_lut_data);
			g_size = num_g_stages *
				sizeof(struct mdp_ar_gc_lut_data);
			b_size = num_b_stages *
				sizeof(struct mdp_ar_gc_lut_data);

			alloc_size += r_size + g_size + b_size;

			*pp = compat_alloc_user_space(alloc_size);
			if (NULL == pp)
				return -ENOMEM;
			memset(*pp, 0, alloc_size);

			(*pp)->data.lut_cfg_data.data.pgc_lut_data.r_data =
					(struct mdp_ar_gc_lut_data *)
					((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp));
			(*pp)->data.lut_cfg_data.data.pgc_lut_data.g_data =
					(struct mdp_ar_gc_lut_data *)
					((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp) + r_size);
			(*pp)->data.lut_cfg_data.data.pgc_lut_data.b_data =
					(struct mdp_ar_gc_lut_data *)
					((unsigned long) *pp +
					sizeof(struct msmfb_mdp_pp) +
					r_size + g_size);
		} else {
			*pp = compat_alloc_user_space(alloc_size);
			if (NULL == *pp)
				return -ENOMEM;
			memset(*pp, 0, alloc_size);
		}
	} else {
		*pp = compat_alloc_user_space(alloc_size);
		if (NULL == *pp)
			return -ENOMEM;
		memset(*pp, 0, alloc_size);
	}

	return 0;
}

static int mdss_compat_pp_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
		break;
	case mdp_op_pa_cfg:
		ret = __from_user_pa_cfg_data(
			compat_ptr((uintptr_t)&pp32->data.pa_cfg_data),
			&pp->data.pa_cfg_data);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
		break;
	case mdp_op_ad_input:
		ret = __from_user_ad_input(
			compat_ptr((uintptr_t)&pp32->data.ad_input),
			&pp->data.ad_input);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
		break;
	case mdp_op_calib_buffer:
		ret = __from_user_calib_config_buffer(
			compat_ptr((uintptr_t)&pp32->data.calib_buffer),
			&pp->data.calib_buffer);
		if (ret)
			goto pp_compat_exit;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) pp);
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
			unsigned long arg)
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
		memset(hist_req, 0, sizeof(struct mdp_histogram_start_req));
		ret = __from_user_hist_start_req(hist_req32, hist_req);
		if (ret)
			goto histo_compat_err;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) hist_req);
		break;
	case MSMFB_HISTOGRAM_STOP:
		ret = mdss_fb_do_ioctl(info, cmd, arg);
		break;
	case MSMFB_HISTOGRAM:
		hist32 = compat_ptr(arg);
		hist = compat_alloc_user_space(
				sizeof(struct mdp_histogram_data));
		memset(hist, 0, sizeof(struct mdp_histogram_data));
		ret = __from_user_hist_data(hist32, hist);
		if (ret)
			goto histo_compat_err;
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) hist);
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

int mdss_compat_overlay_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	struct mdp_overlay *ov, **layers_head;
	struct mdp_overlay32 *ov32;
	struct mdp_overlay_list __user *ovlist;
	struct mdp_overlay_list32 __user *ovlist32;
	size_t layers_refs_sz, layers_sz, prepare_sz;
	void __user *total_mem_chunk;
	uint32_t num_overlays;
	int ret;

	if (!info || !info->par)
		return -EINVAL;


	switch (cmd) {
	case MSMFB_MDP_PP:
		ret = mdss_compat_pp_ioctl(info, cmd, arg);
		break;
	case MSMFB_HISTOGRAM_START:
	case MSMFB_HISTOGRAM_STOP:
	case MSMFB_HISTOGRAM:
		ret = mdss_histo_compat_ioctl(info, cmd, arg);
		break;
	case MSMFB_OVERLAY_GET:
		ov = compat_alloc_user_space(sizeof(*ov));
		ov32 = compat_ptr(arg);
		ret = __from_user_mdp_overlay(ov, ov32);
		if (ret)
			pr_err("%s: compat mdp overlay failed\n", __func__);
		else
			ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) ov);
		ret = __to_user_mdp_overlay(ov32, ov);
		break;
	case MSMFB_OVERLAY_SET:
		ov = compat_alloc_user_space(sizeof(*ov));
		ov32 = compat_ptr(arg);
		ret = __from_user_mdp_overlay(ov, ov32);
		if (ret) {
			pr_err("%s: compat mdp overlay failed\n", __func__);
		} else {
			ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) ov);
			ret = __to_user_mdp_overlay(ov32, ov);
		}
		break;
	case MSMFB_OVERLAY_PREPARE:
		ovlist32 = compat_ptr(arg);
		if (get_user(num_overlays, &ovlist32->num_overlays)) {
			pr_err("compat mdp prepare failed: invalid arg\n");
			return -EFAULT;
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
						(unsigned long) ovlist);
			if (!ret)
				ret = __to_user_mdp_overlaylist(ovlist32,
							 ovlist, layers_head);
		}
		break;
	case MSMFB_OVERLAY_UNSET:
	case MSMFB_OVERLAY_PLAY_ENABLE:
	case MSMFB_OVERLAY_PLAY:
	case MSMFB_OVERLAY_PLAY_WAIT:
	case MSMFB_VSYNC_CTRL:
	case MSMFB_OVERLAY_VSYNC_CTRL:
	case MSMFB_OVERLAY_COMMIT:
	case MSMFB_METADATA_SET:
	case MSMFB_METADATA_GET:
	default:
		pr_debug("%s: overlay ioctl cmd=[%u]\n", __func__, cmd);
		ret = mdss_fb_do_ioctl(info, cmd, (unsigned long) arg);
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
			 unsigned long arg)
{
	int ret;

	if (!info || !info->par)
		return -EINVAL;

	cmd = __do_compat_ioctl_nr(cmd);
	switch (cmd) {
	case MSMFB_CURSOR:
		pr_debug("%s: MSMFB_CURSOR not supported\n", __func__);
		ret = -ENOSYS;
		break;
	case MSMFB_SET_LUT:
		ret = mdss_fb_compat_set_lut(info, arg);
		break;
	case MSMFB_BUFFER_SYNC:
		ret = mdss_fb_compat_buf_sync(info, cmd, arg);
		break;
	case MSMFB_MDP_PP:
	case MSMFB_HISTOGRAM_START:
	case MSMFB_HISTOGRAM_STOP:
	case MSMFB_HISTOGRAM:
	case MSMFB_OVERLAY_GET:
	case MSMFB_OVERLAY_SET:
	case MSMFB_OVERLAY_UNSET:
	case MSMFB_OVERLAY_PLAY_ENABLE:
	case MSMFB_OVERLAY_PLAY:
	case MSMFB_OVERLAY_PLAY_WAIT:
	case MSMFB_VSYNC_CTRL:
	case MSMFB_OVERLAY_VSYNC_CTRL:
	case MSMFB_OVERLAY_COMMIT:
	case MSMFB_METADATA_SET:
	case MSMFB_METADATA_GET:
	case MSMFB_OVERLAY_PREPARE:
		ret = mdss_compat_overlay_ioctl(info, cmd, arg);
		break;
	case MSMFB_NOTIFY_UPDATE:
	case MSMFB_DISPLAY_COMMIT:
	default:
		ret = mdss_fb_do_ioctl(info, cmd, arg);
		break;
	}

	if (ret == -ENOSYS)
		pr_err("%s: unsupported ioctl\n", __func__);
	else if (ret)
		pr_debug("%s: ioctl err cmd=%u ret=%d\n", __func__, cmd, ret);

	return ret;
}
EXPORT_SYMBOL(mdss_fb_compat_ioctl);
