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
	    put_user(compat_ptr(data), &buf_sync->rel_fen_fd))
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
			sizeof(int)))
		return -EFAULT;

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

static int __to_user_pp_params(struct mdp_overlay_pp_params32 *ppp32,
				   struct mdp_overlay_pp_params *ppp)
{
	return 0;
}

static int __from_user_pp_params32(struct mdp_overlay_pp_params *ppp,
				   struct mdp_overlay_pp_params32 *ppp32)
{
	__u32 data;

	if (get_user(data, &ppp32->config_ops) ||
	    put_user(data, &ppp->config_ops))
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

	if (get_user(data, &igc_lut->c0_c1_data) ||
	    put_user((compat_caddr_t) data, &igc_lut32->c0_c1_data) ||
	    get_user(data, &igc_lut->c2_data) ||
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

	if (get_user(data, &hist_lut->data) ||
	    put_user((compat_caddr_t) data, &hist_lut32->data))
		return -EFAULT;

	return 0;
}

static int __from_user_lut_cfg_data(
			struct mdp_lut_cfg_data32 __user *lut_cfg32,
			struct mdp_lut_cfg_data __user *lut_cfg)
{
	uint32_t lut_type;
	int ret;

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

	if (get_user(data, &qseed_data->data) ||
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
	default:
		break;
	}

pp_compat_exit:
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

	ret = __to_user_pp_params(&ov32->overlay_pp_cfg, &ov->overlay_pp_cfg);
	if (ret)
		return -EFAULT;

	ret = copy_in_user(&ov32->scale, &ov->scale,
			   sizeof(struct mdp_scale_data));
	if (ret)
		return -EFAULT;
	return 0;
}


static int __from_user_mdp_overlay(struct mdp_overlay *ov,
				   struct mdp_overlay32 *ov32)
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
	    get_user(data, &ov32->id) ||
	    put_user(data, &ov->id))
		return -EFAULT;

	if (copy_in_user(&ov->user_data, &ov32->user_data,
			 sizeof(ov32->user_data)))
		return -EFAULT;

	if (get_user(data, &ov32->horz_deci) ||
	    put_user(data, &ov->horz_deci) ||
	    get_user(data, &ov32->vert_deci) ||
	    put_user(data, &ov->vert_deci))
		return -EFAULT;

	if (__from_user_pp_params32(&ov->overlay_pp_cfg,
				    &ov32->overlay_pp_cfg))
		return -EFAULT;

	if (copy_in_user(&ov->scale, &ov32->scale,
			 sizeof(struct mdp_scale_data)))
		return -EFAULT;

	return 0;
}

int mdss_compat_overlay_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	struct mdp_overlay *ov;
	struct mdp_overlay32 *ov32;
	int ret;

	if (!info || !info->par)
		return -EINVAL;


	switch (cmd) {
	case MSMFB_MDP_PP:
		ret = mdss_compat_pp_ioctl(info, cmd, arg);
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
		pr_err("%s: ioctl err cmd=%u ret=%d\n", __func__, cmd, ret);

	return ret;
}
EXPORT_SYMBOL(mdss_fb_compat_ioctl);
