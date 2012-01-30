/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/android_pmem.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/ion.h>
#include <linux/msm_kgsl.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_formats.h"

enum {
	MDP_INTR_VSYNC_INTF_0,
	MDP_INTR_VSYNC_INTF_1,
	MDP_INTR_VSYNC_INTF_2,
	MDP_INTR_VSYNC_INTF_3,
	MDP_INTR_PING_PONG_0,
	MDP_INTR_PING_PONG_1,
	MDP_INTR_PING_PONG_2,
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
	case MDSS_MDP_IRQ_INTF_VSYNC:
		index = MDP_INTR_VSYNC_INTF_0 + (intf_num - MDSS_MDP_INTF0);
		break;
	case MDSS_MDP_IRQ_PING_PONG_COMP:
		index = MDP_INTR_PING_PONG_0 + intf_num;
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
	int index, ret;

	index = mdss_mdp_intr2index(intr_type, intf_num);
	if (index < 0) {
		pr_warn("invalid intr type=%u intf_num=%u\n",
				intr_type, intf_num);
		return -EINVAL;
	}

	spin_lock_irqsave(&mdss_mdp_intr_lock, flags);
	if (!mdp_intr_cb[index].func) {
		mdp_intr_cb[index].func = fnc_ptr;
		mdp_intr_cb[index].arg = arg;
		ret = 0;
	} else {
		ret = -EBUSY;
	}
	spin_unlock_irqrestore(&mdss_mdp_intr_lock, flags);

	return ret;
}

static inline void mdss_mdp_intr_done(int index)
{
	void (*fnc)(void *);
	void *arg;

	spin_lock(&mdss_mdp_intr_lock);
	fnc = mdp_intr_cb[index].func;
	arg = mdp_intr_cb[index].arg;
	if (fnc != NULL)
		mdp_intr_cb[index].func = NULL;
	spin_unlock(&mdss_mdp_intr_lock);
	if (fnc)
		fnc(arg);
}

irqreturn_t mdss_mdp_isr(int irq, void *ptr)
{
	u32 isr, mask;


	isr = MDSS_MDP_REG_READ(MDSS_MDP_REG_INTR_STATUS);

	pr_debug("isr=%x\n", isr);

	if (isr == 0)
		goto done;

	mask = MDSS_MDP_REG_READ(MDSS_MDP_REG_INTR_EN);
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_CLEAR, isr);

	isr &= mask;
	if (isr == 0)
		goto done;

	if (isr & MDSS_MDP_INTR_PING_PONG_0_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_0);

	if (isr & MDSS_MDP_INTR_PING_PONG_1_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_1);

	if (isr & MDSS_MDP_INTR_PING_PONG_2_DONE)
		mdss_mdp_intr_done(MDP_INTR_PING_PONG_2);

	if (isr & MDSS_MDP_INTR_INTF_0_VSYNC)
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_0);

	if (isr & MDSS_MDP_INTR_INTF_1_VSYNC)
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_1);

	if (isr & MDSS_MDP_INTR_INTF_2_VSYNC)
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_2);

	if (isr & MDSS_MDP_INTR_INTF_3_VSYNC)
		mdss_mdp_intr_done(MDP_INTR_VSYNC_INTF_3);

	if (isr & MDSS_MDP_INTR_WB_0_DONE)
		mdss_mdp_intr_done(MDP_INTR_WB_0);

	if (isr & MDSS_MDP_INTR_WB_1_DONE)
		mdss_mdp_intr_done(MDP_INTR_WB_1);

	if (isr & MDSS_MDP_INTR_WB_2_DONE)
		mdss_mdp_intr_done(MDP_INTR_WB_2);

done:
	return IRQ_HANDLED;
}

struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format)
{
	struct mdss_mdp_format_params *fmt = NULL;
	if (format < MDP_IMGTYPE_LIMIT) {
		fmt = &mdss_mdp_format_map[format];
		if (fmt->format != format)
			fmt = NULL;
	}

	return fmt;
}

int mdss_mdp_get_plane_sizes(u32 format, u32 w, u32 h,
			     struct mdss_mdp_plane_sizes *ps)
{
	struct mdss_mdp_format_params *fmt;
	int i;

	if (ps == NULL)
		return -EINVAL;

	if ((w > MAX_IMG_WIDTH) || (h > MAX_IMG_HEIGHT))
		return -ERANGE;

	fmt = mdss_mdp_get_format_params(format);
	if (!fmt)
		return -EINVAL;

	memset(ps, 0, sizeof(struct mdss_mdp_plane_sizes));

	if (fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
		u32 bpp = fmt->bpp + 1;
		ps->num_planes = 1;
		ps->plane_size[0] = w * h * bpp;
		ps->ystride[0] = w * bpp;
	} else {
		u8 hmap[] = { 1, 2, 1, 2 };
		u8 vmap[] = { 1, 1, 2, 2 };
		u8 horiz, vert;

		horiz = hmap[fmt->chroma_sample];
		vert = vmap[fmt->chroma_sample];

		if (format == MDP_Y_CR_CB_GH2V2) {
			ps->plane_size[0] = ALIGN(w, 16) * h;
			ps->plane_size[1] = ALIGN(w / horiz, 16) * (h / vert);
			ps->ystride[0] = ALIGN(w, 16);
			ps->ystride[1] = ALIGN(w / horiz, 16);
		} else {
			ps->plane_size[0] = w * h;
			ps->plane_size[1] = (w / horiz) * (h / vert);
			ps->ystride[0] = w;
			ps->ystride[1] = (w / horiz);
		}

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

	for (i = 0; i < ps->num_planes; i++)
		ps->total_size += ps->plane_size[i];

	return 0;
}

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps)
{
	if (!ps)
		return 0;

	if (!data || data->num_planes == 0)
		return -ENOMEM;

	if (data->bwc_enabled) {
		return -EPERM; /* not supported */
	} else {
		struct mdss_mdp_img_data *prev, *curr;
		int i;

		pr_debug("srcp0=%x len=%u frame_size=%u\n", data->p[0].addr,
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
				pr_err("insufficient mem=%u p=%d len=%u\n",
				       curr->len, i, ps->plane_size[i]);
				return -ENOMEM;
			}
			pr_debug("plane[%d] addr=%x len=%u\n", i,
					curr->addr, curr->len);
		}
		data->num_planes = ps->num_planes;
	}

	return 0;
}

int mdss_mdp_put_img(struct mdss_mdp_img_data *data)
{
	/* only source may use frame buffer */
	if (data->flags & MDP_MEMORY_ID_TYPE_FB) {
		fput_light(data->srcp_file, data->p_need);
		return 0;
	}
	if (data->srcp_file) {
		put_pmem_file(data->srcp_file);
		data->srcp_file = NULL;
		return 0;
	}
	if (!IS_ERR_OR_NULL(data->srcp_ihdl)) {
		ion_free(data->iclient, data->srcp_ihdl);
		data->iclient = NULL;
		data->srcp_ihdl = NULL;
		return 0;
	}

	return -ENOMEM;
}

int mdss_mdp_get_img(struct ion_client *iclient, struct msmfb_data *img,
		     struct mdss_mdp_img_data *data)
{
	struct file *file;
	int ret = -EINVAL;
	int fb_num;
	unsigned long *start, *len;

	start = (unsigned long *) &data->addr;
	len = (unsigned long *) &data->len;
	data->flags = img->flags;
	data->p_need = 0;

	if (img->flags & MDP_BLIT_SRC_GEM) {
		data->srcp_file = NULL;
		ret = kgsl_gem_obj_addr(img->memory_id, (int) img->priv,
					start, len);
	} else if (img->flags & MDP_MEMORY_ID_TYPE_FB) {
		file = fget_light(img->memory_id, &data->p_need);
		if (file && FB_MAJOR ==
				MAJOR(file->f_dentry->d_inode->i_rdev)) {
			data->srcp_file = file;
			fb_num = MINOR(file->f_dentry->d_inode->i_rdev);
			ret = mdss_fb_get_phys_info(start, len, fb_num);
		}
	} else if (iclient) {
		data->iclient = iclient;
		data->srcp_ihdl = ion_import_dma_buf(iclient, img->memory_id);
		if (IS_ERR_OR_NULL(data->srcp_ihdl))
			return PTR_ERR(data->srcp_ihdl);
		ret = ion_phys(iclient, data->srcp_ihdl,
			       start, (size_t *) len);
	} else {
		unsigned long vstart;
		ret = get_pmem_file(img->memory_id, start, &vstart, len,
				    &data->srcp_file);
	}

	if (!ret && (img->offset < data->len)) {
		data->addr += img->offset;
		data->len -= img->offset;
	} else {
		mdss_mdp_put_img(data);
		ret = -EINVAL;
	}

	return ret;
}
