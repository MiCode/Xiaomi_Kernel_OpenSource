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
#include <linux/iommu.h>
#include <linux/msm_kgsl.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <mach/iommu_domains.h>

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
		u32 bpp = fmt->bpp;
		ps->num_planes = 1;
		ps->plane_size[0] = w * h * bpp;
		ps->ystride[0] = w * bpp;
	} else {
		u8 hmap[] = { 1, 2, 1, 2 };
		u8 vmap[] = { 1, 1, 2, 2 };
		u8 horiz, vert, stride_align;

		horiz = hmap[fmt->chroma_sample];
		vert = vmap[fmt->chroma_sample];

		switch (format) {
		case MDP_Y_CR_CB_GH2V2:
			stride_align = 16;
			break;
		case MDP_Y_CBCR_H2V2_VENUS:
			stride_align = 32;
			break;
		default:
			stride_align = 1;
			break;
		}

		ps->ystride[0] = ALIGN(w, stride_align);
		ps->ystride[1] = ALIGN(w / horiz, stride_align);
		ps->plane_size[0] = ps->ystride[0] * h;
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
	struct ion_client *iclient = mdss_get_ionclient();
	if (data->flags & MDP_MEMORY_ID_TYPE_FB) {
		pr_debug("fb mem buf=0x%x\n", data->addr);
		fput_light(data->srcp_file, data->p_need);
		data->srcp_file = NULL;
	} else if (data->srcp_file) {
		pr_debug("pmem buf=0x%x\n", data->addr);
		put_pmem_file(data->srcp_file);
		data->srcp_file = NULL;
	} else if (!IS_ERR_OR_NULL(data->srcp_ihdl)) {
		pr_debug("ion hdl=%p buf=0x%x\n", data->srcp_ihdl, data->addr);

		if (is_mdss_iommu_attached())
			ion_unmap_iommu(iclient, data->srcp_ihdl,
					mdss_get_iommu_domain(), 0);

		ion_free(iclient, data->srcp_ihdl);
		data->srcp_ihdl = NULL;
	} else {
		return -ENOMEM;
	}

	return 0;
}

int mdss_mdp_get_img(struct msmfb_data *img, struct mdss_mdp_img_data *data)
{
	struct file *file;
	int ret = -EINVAL;
	int fb_num;
	unsigned long *start, *len;
	struct ion_client *iclient = mdss_get_ionclient();

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

		if (is_mdss_iommu_attached()) {
			ret = ion_map_iommu(iclient, data->srcp_ihdl,
					    mdss_get_iommu_domain(),
					    0, SZ_4K, 0, start, len, 0,
					    ION_IOMMU_UNMAP_DELAYED);
		} else {
			ret = ion_phys(iclient, data->srcp_ihdl, start,
				       (size_t *) len);
		}

		if (IS_ERR_VALUE(ret)) {
			ion_free(iclient, data->srcp_ihdl);
			pr_err("failed to map ion handle (%d)\n", ret);
			return ret;
		}
	} else {
		unsigned long vstart;
		ret = get_pmem_file(img->memory_id, start, &vstart, len,
				    &data->srcp_file);
	}

	if (!ret && (img->offset < data->len)) {
		data->addr += img->offset;
		data->len -= img->offset;

		pr_debug("mem=%d ihdl=%p buf=0x%x len=0x%x\n", img->memory_id,
			 data->srcp_ihdl, data->addr, data->len);
	} else {
		return -EINVAL;
	}

	return ret;
}
