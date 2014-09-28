/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/msm_ion.h>
#include <linux/qcom_iommu.h>
#include <linux/msm_kgsl.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/major.h>
#include <media/msm_media_info.h>

#include <linux/msm_iommu_domains.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_formats.h"
#include "mdss_debug.h"

enum {
	MDP_INTR_VSYNC_INTF_0,
	MDP_INTR_VSYNC_INTF_1,
	MDP_INTR_VSYNC_INTF_2,
	MDP_INTR_VSYNC_INTF_3,
	MDP_INTR_UNDERRUN_INTF_0,
	MDP_INTR_UNDERRUN_INTF_1,
	MDP_INTR_UNDERRUN_INTF_2,
	MDP_INTR_UNDERRUN_INTF_3,
	MDP_INTR_PING_PONG_0,
	MDP_INTR_PING_PONG_1,
	MDP_INTR_PING_PONG_2,
	MDP_INTR_PING_PONG_3,
	MDP_INTR_PING_PONG_0_RD_PTR,
	MDP_INTR_PING_PONG_1_RD_PTR,
	MDP_INTR_PING_PONG_2_RD_PTR,
	MDP_INTR_PING_PONG_3_RD_PTR,
	MDP_INTR_WB_0,
	MDP_INTR_WB_1,
	MDP_INTR_WB_2,
	MDP_INTR_MAX,
};

struct intr_callback {
	void (*func)(void *);
	void *arg;
};

struct intr_callback mdp_intr_cb[MDP_INTR_MAX];
static DEFINE_SPINLOCK(mdss_mdp_intr_lock);

static int mdss_mdp_intr2index(u32 intr_type, u32 intf_num)
{
	int index = -1;
	switch (intr_type) {
	case MDSS_MDP_IRQ_INTF_UNDER_RUN:
		index = MDP_INTR_UNDERRUN_INTF_0 + (intf_num - MDSS_MDP_INTF0);
		break;
	case MDSS_MDP_IRQ_INTF_VSYNC:
		index = MDP_INTR_VSYNC_INTF_0 + (intf_num - MDSS_MDP_INTF0);
		break;
	case MDSS_MDP_IRQ_PING_PONG_COMP:
		index = MDP_INTR_PING_PONG_0 + intf_num;
		break;
	case MDSS_MDP_IRQ_PING_PONG_RD_PTR:
		index = MDP_INTR_PING_PONG_0_RD_PTR + intf_num;
		break;
	case MDSS_MDP_IRQ_WB_ROT_COMP:
		index = MDP_INTR_WB_0 + intf_num;
		break;
	case MDSS_MDP_IRQ_WB_WFD:
		index = MDP_INTR_WB_2 + intf_num;
		break;
	}

	return index;
}

int mdss_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
			       void (*fnc_ptr)(void *), void *arg)
{
	unsigned long flags;
	int index;

	index = mdss_mdp_intr2index(intr_type, intf_num);
	if (index < 0) {
		pr_warn("invalid intr type=%u intf_num=%u\n",
				intr_type, intf_num);
		return -EINVAL;
	}

	spin_lock_irqsave(&mdss_mdp_intr_lock, flags);
	WARN(mdp_intr_cb[index].func && fnc_ptr,
		"replacing current intr callback for ndx=%d\n", index);
	mdp_intr_cb[index].func = fnc_ptr;
	mdp_intr_cb[index].arg = arg;
	spin_unlock_irqrestore(&mdss_mdp_intr_lock, flags);

	return 0;
}

static inline void mdss_mdp_intr_done(int index)
{
	void (*fnc)(void *);
	void *arg;

	spin_lock(&mdss_mdp_intr_lock);
	fnc = mdp_intr_cb[index].func;
	arg = mdp_intr_cb[index].arg;
	spin_unlock(&mdss_mdp_intr_lock);
	if (fnc)
		fnc(arg);
}

irqreturn_t mdss_mdp_isr(int irq, void *ptr)
{
	struct mdss_data_type *mdata = ptr;
	u32 isr, mask, hist_isr, hist_mask;


	isr = readl_relaxed(mdata->mdp_base + MDSS_MDP_REG_INTR_STATUS);

	if (isr == 0)
		goto mdp_isr_done;


	mask = readl_relaxed(mdata->mdp_base + MDSS_MDP_REG_INTR_EN);
	writel_relaxed(isr, mdata->mdp_base + MDSS_MDP_REG_INTR_CLEAR);

	pr_debug("%s: isr=%x mask=%x\n", __func__, isr, mask);

	isr &= mask;
	if (isr == 0)
		goto mdp_isr_done;

	if (isr & MDSS_MDP_INTR_INTF_0_UNDERRUN)
		mdss_mdp_intr_done(MDP_INTR_UNDERRUN_INTF_0);

	if (isr & MDSS_MDP_INTR_INTF_1_UNDERRUN)
		mdss_mdp_intr_done(MDP_INTR_UNDERRUN_INTF_1);

	if (isr & MDSS_MDP_INTR_INTF_2_UNDERRUN)
		mdss_mdp_intr_done(MDP_INTR_UNDERRUN_INTF_2);

	if (isr & MDSS_MDP_INTR_INTF_3_UNDERRUN)
		mdss_mdp_intr_done(MDP_INTR_UNDERRUN_INTF_3);

	if (isr & MDSS_MDP_INTR_PING_PONG_0_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_0);

	if (isr & MDSS_MDP_INTR_PING_PONG_1_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_1);

	if (isr & MDSS_MDP_INTR_PING_PONG_2_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_2);

	if (isr & MDSS_MDP_INTR_PING_PONG_3_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_3);

	if (isr & MDSS_MDP_INTR_PING_PONG_0_RD_PTR)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_0_RD_PTR);

	if (isr & MDSS_MDP_INTR_PING_PONG_1_RD_PTR)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_1_RD_PTR);

	if (isr & MDSS_MDP_INTR_PING_PONG_2_RD_PTR)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_2_RD_PTR);

	if (isr & MDSS_MDP_INTR_PING_PONG_3_RD_PTR)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_3_RD_PTR);

	if (isr & MDSS_MDP_INTR_INTF_0_VSYNC) {
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_0);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_EDP);
	}

	if (isr & MDSS_MDP_INTR_INTF_1_VSYNC) {
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_1);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_DSI0);
	}

	if (isr & MDSS_MDP_INTR_INTF_2_VSYNC) {
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_2);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_DSI1);
	}

	if (isr & MDSS_MDP_INTR_INTF_3_VSYNC) {
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_3);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_HDMI);
	}

	if (isr & MDSS_MDP_INTR_WB_0_DONE) {
		mdss_mdp_intr_done(MDP_INTR_WB_0);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_MDP);
	}

	if (isr & MDSS_MDP_INTR_WB_1_DONE) {
		mdss_mdp_intr_done(MDP_INTR_WB_1);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_MDP);
	}

	if (isr & ((mdata->mdp_rev == MDSS_MDP_HW_REV_108) ?
		MDSS_MDP_INTR_WB_2_DONE >> 2 : MDSS_MDP_INTR_WB_2_DONE)) {
		mdss_mdp_intr_done(MDP_INTR_WB_2);
		mdss_misr_crc_collect(mdata, DISPLAY_MISR_MDP);
	}

mdp_isr_done:
	hist_isr = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_REG_HIST_INTR_STATUS);
	if (hist_isr == 0)
		goto hist_isr_done;
	hist_mask = readl_relaxed(mdata->mdp_base +
			MDSS_MDP_REG_HIST_INTR_EN);
	writel_relaxed(hist_isr, mdata->mdp_base +
		MDSS_MDP_REG_HIST_INTR_CLEAR);
	hist_isr &= hist_mask;
	if (hist_isr == 0)
		goto hist_isr_done;
	mdss_mdp_hist_intr_done(hist_isr);
hist_isr_done:
	return IRQ_HANDLED;
}

struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format)
{
	if (format < MDP_IMGTYPE_LIMIT) {
		struct mdss_mdp_format_params *fmt = NULL;
		int i;
		for (i = 0; i < ARRAY_SIZE(mdss_mdp_format_map); i++) {
			fmt = &mdss_mdp_format_map[i];
			if (format == fmt->format)
				return fmt;
		}
	}
	return NULL;
}

void mdss_mdp_intersect_rect(struct mdss_rect *res_rect,
	const struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect)
{
	int l = max(dst_rect->x, sci_rect->x);
	int t = max(dst_rect->y, sci_rect->y);
	int r = min((dst_rect->x + dst_rect->w), (sci_rect->x + sci_rect->w));
	int b = min((dst_rect->y + dst_rect->h), (sci_rect->y + sci_rect->h));

	if (r < l || b < t)
		*res_rect = (struct mdss_rect){0, 0, 0, 0};
	else
		*res_rect = (struct mdss_rect){l, t, (r-l), (b-t)};
}

void mdss_mdp_crop_rect(struct mdss_rect *src_rect,
	struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect)
{
	struct mdss_rect res;
	mdss_mdp_intersect_rect(&res, dst_rect, sci_rect);

	if (res.w && res.h) {
		if ((res.w != dst_rect->w) || (res.h != dst_rect->h)) {
			src_rect->x = src_rect->x + (res.x - dst_rect->x);
			src_rect->y = src_rect->y + (res.y - dst_rect->y);
			src_rect->w = res.w;
			src_rect->h = res.h;
		}
		*dst_rect = (struct mdss_rect)
			{(res.x - sci_rect->x), (res.y - sci_rect->y),
			res.w, res.h};
	}
}

int mdss_mdp_get_rau_strides(u32 w, u32 h,
			       struct mdss_mdp_format_params *fmt,
			       struct mdss_mdp_plane_sizes *ps)
{
	if (fmt->is_yuv) {
		ps->rau_cnt = DIV_ROUND_UP(w, 64);
		ps->ystride[0] = 64 * 4;
		ps->rau_h[0] = 4;
		ps->rau_h[1] = 2;
		if (fmt->chroma_sample == MDSS_MDP_CHROMA_H1V2)
			ps->ystride[1] = 64 * 2;
		else if (fmt->chroma_sample == MDSS_MDP_CHROMA_H2V1) {
			ps->ystride[1] = 32 * 4;
			ps->rau_h[1] = 4;
		} else
			ps->ystride[1] = 32 * 2;

		/* account for both chroma components */
		ps->ystride[1] <<= 1;
	} else if (fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
		ps->rau_cnt = DIV_ROUND_UP(w, 32);
		ps->ystride[0] = 32 * 4 * fmt->bpp;
		ps->ystride[1] = 0;
		ps->rau_h[0] = 4;
		ps->rau_h[1] = 0;
	} else  {
		pr_err("Invalid format=%d\n", fmt->format);
		return -EINVAL;
	}

	ps->ystride[0] *= ps->rau_cnt;
	ps->ystride[1] *= ps->rau_cnt;
	ps->num_planes = 2;

	pr_debug("BWC rau_cnt=%d strides={%d,%d} heights={%d,%d}\n",
		ps->rau_cnt, ps->ystride[0], ps->ystride[1],
		ps->rau_h[0], ps->rau_h[1]);

	return 0;
}

int mdss_mdp_get_plane_sizes(u32 format, u32 w, u32 h,
	struct mdss_mdp_plane_sizes *ps, u32 bwc_mode, bool rotation)
{
	struct mdss_mdp_format_params *fmt;
	int i, rc;
	u32 bpp;
	if (ps == NULL)
		return -EINVAL;

	if ((w > MAX_IMG_WIDTH) || (h > MAX_IMG_HEIGHT))
		return -ERANGE;

	fmt = mdss_mdp_get_format_params(format);
	if (!fmt)
		return -EINVAL;

	bpp = fmt->bpp;
	memset(ps, 0, sizeof(struct mdss_mdp_plane_sizes));

	if (bwc_mode) {
		u32 height, meta_size;

		rc = mdss_mdp_get_rau_strides(w, h, fmt, ps);
		if (rc)
			return rc;

		height = DIV_ROUND_UP(h, ps->rau_h[0]);
		meta_size = DIV_ROUND_UP(ps->rau_cnt, 8);
		ps->ystride[1] += meta_size;
		ps->ystride[0] += ps->ystride[1] + meta_size;
		ps->plane_size[0] = ps->ystride[0] * height;

		ps->ystride[1] = 2;
		ps->plane_size[1] = 2 * ps->rau_cnt * height;

		pr_debug("BWC data stride=%d size=%d meta size=%d\n",
			ps->ystride[0], ps->plane_size[0], ps->plane_size[1]);
	} else {
		if (fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
			ps->num_planes = 1;
			ps->plane_size[0] = w * h * bpp;
			ps->ystride[0] = w * bpp;
		} else if (format == MDP_Y_CBCR_H2V2_VENUS) {
			int cf = COLOR_FMT_NV12;
			ps->num_planes = 2;
			ps->ystride[0] = VENUS_Y_STRIDE(cf, w);
			ps->ystride[1] = VENUS_UV_STRIDE(cf, w);
			ps->plane_size[0] = VENUS_Y_SCANLINES(cf, h) *
				ps->ystride[0];
			ps->plane_size[1] = VENUS_UV_SCANLINES(cf, h) *
				ps->ystride[1];
		} else {
			u8 hmap[] = { 1, 2, 1, 2 };
			u8 vmap[] = { 1, 1, 2, 2 };
			u8 horiz, vert, stride_align, height_align;
			u32 chroma_samp;

			chroma_samp = fmt->chroma_sample;

			if (rotation) {
				if (chroma_samp == MDSS_MDP_CHROMA_H2V1)
					chroma_samp = MDSS_MDP_CHROMA_H1V2;
				else if (chroma_samp == MDSS_MDP_CHROMA_H1V2)
					chroma_samp = MDSS_MDP_CHROMA_H2V1;
			}

			if (chroma_samp >= ARRAY_SIZE(hmap) ||
				chroma_samp >= ARRAY_SIZE(vmap)) {
				pr_err("%s: out of bounds error\n", __func__);
				return -ERANGE;
			}
			horiz = hmap[chroma_samp];
			vert = vmap[chroma_samp];

			switch (format) {
			case MDP_Y_CR_CB_GH2V2:
				stride_align = 16;
				height_align = 1;
				break;
			default:
				stride_align = 1;
				height_align = 1;
				break;
			}

			ps->ystride[0] = ALIGN(w, stride_align);
			ps->ystride[1] = ALIGN(w / horiz, stride_align);
			ps->plane_size[0] = ps->ystride[0] *
				ALIGN(h, height_align);
			ps->plane_size[1] = ps->ystride[1] * (h / vert);

			if (fmt->fetch_planes == MDSS_MDP_PLANE_PSEUDO_PLANAR) {
				ps->num_planes = 2;
				ps->plane_size[1] *= 2;
				ps->ystride[1] *= 2;
			} else { /* planar */
				ps->num_planes = 3;
				ps->plane_size[2] = ps->plane_size[1];
				ps->ystride[2] = ps->ystride[1];
			}
		}
	}
	for (i = 0; i < ps->num_planes; i++)
		ps->total_size += ps->plane_size[i];

	return 0;
}

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps)
{
	struct mdss_mdp_img_data *prev, *curr;
	int i;

	if (!ps)
		return 0;

	if (!data || data->num_planes == 0)
		return -ENOMEM;

	pr_debug("srcp0=%pa len=%lu frame_size=%u\n", &data->p[0].addr,
		data->p[0].len, ps->total_size);

	for (i = 0; i < ps->num_planes; i++) {
		curr = &data->p[i];
		if (i >= data->num_planes) {
			u32 psize = ps->plane_size[i-1];
			prev = &data->p[i-1];
			if (prev->len > psize) {
				curr->len = prev->len - psize;
				prev->len = psize;
			}
			curr->addr = prev->addr + psize;
		}
		if (curr->len < ps->plane_size[i]) {
			pr_err("insufficient mem=%lu p=%d len=%u\n",
			       curr->len, i, ps->plane_size[i]);
			return -ENOMEM;
		}
		pr_debug("plane[%d] addr=%pa len=%lu\n", i,
				&curr->addr, curr->len);
	}
	data->num_planes = ps->num_planes;

	return 0;
}

void mdss_mdp_data_calc_offset(struct mdss_mdp_data *data, u16 x, u16 y,
	struct mdss_mdp_plane_sizes *ps, struct mdss_mdp_format_params *fmt)
{
	if ((x == 0) && (y == 0))
		return;

	data->p[0].addr += y * ps->ystride[0];

	if (data->num_planes == 1) {
		data->p[0].addr += x * fmt->bpp;
	} else {
		u8 hmap[] = { 1, 2, 1, 2 };
		u8 vmap[] = { 1, 1, 2, 2 };
		u16 xoff = x / hmap[fmt->chroma_sample];
		u16 yoff = y / vmap[fmt->chroma_sample];

		data->p[0].addr += x;
		data->p[1].addr += xoff + (yoff * ps->ystride[1]);
		if (data->num_planes == 2) /* pseudo planar */
			data->p[1].addr += xoff;
		else /* planar */
			data->p[2].addr += xoff + (yoff * ps->ystride[2]);
	}
}

static int mdss_mdp_put_img(struct mdss_mdp_img_data *data)
{
	struct ion_client *iclient = mdss_get_ionclient();
	if (data->flags & MDP_MEMORY_ID_TYPE_FB) {
		pr_debug("fb mem buf=0x%pa\n", &data->addr);
		fput_light(data->srcp_file, data->p_need);
		data->srcp_file = NULL;
	} else if (data->srcp_file) {
		pr_debug("pmem buf=0x%pa\n", &data->addr);
		data->srcp_file = NULL;
	} else if (!IS_ERR_OR_NULL(data->srcp_ihdl)) {
		pr_debug("ion hdl=%p buf=0x%pa\n", data->srcp_ihdl,
							&data->addr);
		if (!iclient) {
			pr_err("invalid ion client\n");
			return -ENOMEM;
		} else {
			if (data->mapped) {
				int domain;
				if (data->flags & MDP_SECURE_OVERLAY_SESSION)
					domain = MDSS_IOMMU_DOMAIN_SECURE;
				else
					domain = MDSS_IOMMU_DOMAIN_UNSECURE;
				ion_unmap_iommu(iclient, data->srcp_ihdl,
					mdss_get_iommu_domain(domain), 0);

				data->mapped = false;
			}
			ion_free(iclient, data->srcp_ihdl);
			data->srcp_ihdl = NULL;
		}

	} else {
		return -ENOMEM;
	}

	return 0;
}

static int mdss_mdp_get_img(struct msmfb_data *img,
		struct mdss_mdp_img_data *data)
{
	struct file *file;
	int ret = -EINVAL;
	int fb_num;
	unsigned long *len;
	dma_addr_t *start;
	struct ion_client *iclient = mdss_get_ionclient();

	start = &data->addr;
	len = &data->len;
	data->flags |= img->flags;
	data->p_need = 0;
	data->offset = img->offset;

	if (img->flags & MDP_BLIT_SRC_GEM) {
		data->srcp_file = NULL;
		ret = kgsl_gem_obj_addr(img->memory_id, (int) img->priv,
					start, len);
	} else if (img->flags & MDP_MEMORY_ID_TYPE_FB) {
		file = fget_light(img->memory_id, &data->p_need);
		if (file == NULL) {
			pr_err("invalid framebuffer file (%d)\n",
					img->memory_id);
			return -EINVAL;
		}
		data->srcp_file = file;

		if (MAJOR(file->f_dentry->d_inode->i_rdev) == FB_MAJOR) {
			fb_num = MINOR(file->f_dentry->d_inode->i_rdev);
			ret = mdss_fb_get_phys_info(start, len, fb_num);
			if (ret)
				pr_err("mdss_fb_get_phys_info() failed\n");
		} else {
			pr_err("invalid FB_MAJOR\n");
			ret = -1;
		}
	} else if (iclient) {
		data->srcp_ihdl = ion_import_dma_buf(iclient, img->memory_id);
		if (IS_ERR_OR_NULL(data->srcp_ihdl)) {
			pr_err("error on ion_import_fd\n");
			ret = PTR_ERR(data->srcp_ihdl);
			data->srcp_ihdl = NULL;
			return ret;
		}
		data->addr = 0;
		data->len = 0;
		data->mapped = false;
		/* return early, mapping will be done later */

		return 0;
	}

	if (!*start) {
		pr_err("start address is zero!\n");
		mdss_mdp_put_img(data);
		return -ENOMEM;
	}

	if (!ret && (data->offset < data->len)) {
		data->addr += data->offset;
		data->len -= data->offset;

		pr_debug("mem=%d ihdl=%p buf=0x%pa len=0x%lu\n", img->memory_id,
			 data->srcp_ihdl, &data->addr, data->len);
	} else {
		mdss_mdp_put_img(data);
		return ret ? : -EOVERFLOW;
	}

	return ret;
}

static int mdss_mdp_map_buffer(struct mdss_mdp_img_data *data)
{
	int ret = -EINVAL;
	struct ion_client *iclient = mdss_get_ionclient();

	if (data->addr && data->len)
		return 0;

	if (!IS_ERR_OR_NULL(data->srcp_ihdl)) {
		if (mdss_res->mdss_util->iommu_attached()) {
			int domain;
			if (data->flags & MDP_SECURE_OVERLAY_SESSION)
				domain = MDSS_IOMMU_DOMAIN_SECURE;
			else
				domain = MDSS_IOMMU_DOMAIN_UNSECURE;

			ret = ion_map_iommu(iclient, data->srcp_ihdl,
						mdss_get_iommu_domain(domain),
						0, SZ_4K, 0, &data->addr,
						&data->len, 0, 0);
			data->mapped = true;
		} else {
			ret = ion_phys(iclient, data->srcp_ihdl,
					&data->addr, (size_t *) &data->len);
		}

		if (IS_ERR_VALUE(ret)) {
			ion_free(iclient, data->srcp_ihdl);
			pr_err("failed to map ion handle (%d)\n", ret);
			return ret;
		}
	}

	if (!data->addr) {
		pr_err("start address is zero!\n");
		mdss_mdp_put_img(data);
		return -ENOMEM;
	}

	if (!ret && (data->offset < data->len)) {
		data->addr += data->offset;
		data->len -= data->offset;

		pr_debug("ihdl=%p buf=0x%pa len=0x%lu\n",
			 data->srcp_ihdl, &data->addr, data->len);
	} else {
		mdss_mdp_put_img(data);
		return ret ? : -EOVERFLOW;
	}

	return ret;
}

int mdss_mdp_data_get(struct mdss_mdp_data *data, struct msmfb_data *planes,
		int num_planes, u32 flags)
{
	int i, rc = 0;

	if ((num_planes <= 0) || (num_planes > MAX_PLANES))
		return -EINVAL;

	for (i = 0; i < num_planes; i++) {
		data->p[i].flags = flags;
		rc = mdss_mdp_get_img(&planes[i], &data->p[i]);
		if (rc) {
			pr_err("failed to get buf p=%d flags=%x\n", i, flags);
			while (i > 0) {
				i--;
				mdss_mdp_put_img(&data->p[i]);
			}
			break;
		}
	}

	data->num_planes = i;

	return rc;
}

int mdss_mdp_data_map(struct mdss_mdp_data *data)
{
	int i, rc = 0;

	if (!data || !data->num_planes)
		return -EINVAL;

	for (i = 0; i < data->num_planes; i++) {
		rc = mdss_mdp_map_buffer(&data->p[i]);
		if (rc) {
			pr_err("failed to map buf p=%d\n", i);
			while (i > 0) {
				i--;
				mdss_mdp_put_img(&data->p[i]);
			}
			break;
		}
	}

	return rc;
}

void mdss_mdp_data_free(struct mdss_mdp_data *data)
{
	int i;

	for (i = 0; i < data->num_planes && data->p[i].len; i++)
		mdss_mdp_put_img(&data->p[i]);

	data->num_planes = 0;
}

int mdss_mdp_calc_phase_step(u32 src, u32 dst, u32 *out_phase)
{
	u32 unit, residue, result;

	if (src == 0 || dst == 0)
		return -EINVAL;

	unit = 1 << PHASE_STEP_SHIFT;
	*out_phase = mult_frac(unit, src, dst);

	/* check if overflow is possible */
	if (src > dst) {
		residue = *out_phase - unit;
		result = (residue * dst) + residue;

		while (result > (unit + (unit >> 1)))
			result -= unit;

		if ((result > residue) && (result < unit))
			return -EOVERFLOW;
	}

	return 0;
}
