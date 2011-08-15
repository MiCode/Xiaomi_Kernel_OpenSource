/* Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/android_pmem.h>
#include <linux/msm_rotator.h>
#include <linux/io.h>
#include <mach/msm_rotator_imem.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/major.h>
#include <linux/regulator/consumer.h>
#include <linux/ion.h>
#ifdef CONFIG_MSM_BUS_SCALING
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#endif
#include <mach/msm_subsystem_map.h>
#include <mach/iommu_domains.h>

#define DRIVER_NAME "msm_rotator"

#define MSM_ROTATOR_BASE (msm_rotator_dev->io_base)
#define MSM_ROTATOR_INTR_ENABLE			(MSM_ROTATOR_BASE+0x0020)
#define MSM_ROTATOR_INTR_STATUS			(MSM_ROTATOR_BASE+0x0024)
#define MSM_ROTATOR_INTR_CLEAR			(MSM_ROTATOR_BASE+0x0028)
#define MSM_ROTATOR_START			(MSM_ROTATOR_BASE+0x0030)
#define MSM_ROTATOR_MAX_BURST_SIZE		(MSM_ROTATOR_BASE+0x0050)
#define MSM_ROTATOR_HW_VERSION			(MSM_ROTATOR_BASE+0x0070)
#define MSM_ROTATOR_SRC_SIZE			(MSM_ROTATOR_BASE+0x1108)
#define MSM_ROTATOR_SRCP0_ADDR			(MSM_ROTATOR_BASE+0x110c)
#define MSM_ROTATOR_SRCP1_ADDR			(MSM_ROTATOR_BASE+0x1110)
#define MSM_ROTATOR_SRCP2_ADDR			(MSM_ROTATOR_BASE+0x1114)
#define MSM_ROTATOR_SRC_YSTRIDE1		(MSM_ROTATOR_BASE+0x111c)
#define MSM_ROTATOR_SRC_YSTRIDE2		(MSM_ROTATOR_BASE+0x1120)
#define MSM_ROTATOR_SRC_FORMAT			(MSM_ROTATOR_BASE+0x1124)
#define MSM_ROTATOR_SRC_UNPACK_PATTERN1		(MSM_ROTATOR_BASE+0x1128)
#define MSM_ROTATOR_SUB_BLOCK_CFG		(MSM_ROTATOR_BASE+0x1138)
#define MSM_ROTATOR_OUT_PACK_PATTERN1		(MSM_ROTATOR_BASE+0x1154)
#define MSM_ROTATOR_OUTP0_ADDR			(MSM_ROTATOR_BASE+0x1168)
#define MSM_ROTATOR_OUTP1_ADDR			(MSM_ROTATOR_BASE+0x116c)
#define MSM_ROTATOR_OUTP2_ADDR			(MSM_ROTATOR_BASE+0x1170)
#define MSM_ROTATOR_OUT_YSTRIDE1		(MSM_ROTATOR_BASE+0x1178)
#define MSM_ROTATOR_OUT_YSTRIDE2		(MSM_ROTATOR_BASE+0x117c)
#define MSM_ROTATOR_SRC_XY			(MSM_ROTATOR_BASE+0x1200)
#define MSM_ROTATOR_SRC_IMAGE_SIZE		(MSM_ROTATOR_BASE+0x1208)

#define MSM_ROTATOR_MAX_ROT	0x07
#define MSM_ROTATOR_MAX_H	0x1fff
#define MSM_ROTATOR_MAX_W	0x1fff

/* from lsb to msb */
#define GET_PACK_PATTERN(a, x, y, z, bit) \
			(((a)<<((bit)*3))|((x)<<((bit)*2))|((y)<<(bit))|(z))
#define CLR_G 0x0
#define CLR_B 0x1
#define CLR_R 0x2
#define CLR_ALPHA 0x3

#define CLR_Y  CLR_G
#define CLR_CB CLR_B
#define CLR_CR CLR_R

#define ROTATIONS_TO_BITMASK(r) ((((r) & MDP_ROT_90) ? 1 : 0)  | \
				 (((r) & MDP_FLIP_LR) ? 2 : 0) | \
				 (((r) & MDP_FLIP_UD) ? 4 : 0))

#define IMEM_NO_OWNER -1;

#define MAX_SESSIONS 16
#define INVALID_SESSION -1
#define VERSION_KEY_MASK 0xFFFFFF00
#define MAX_DOWNSCALE_RATIO 3

#define ROTATOR_REVISION_V0		0
#define ROTATOR_REVISION_V1		1
#define ROTATOR_REVISION_V2		2
#define ROTATOR_REVISION_NONE	0xffffffff

uint32_t rotator_hw_revision;

/*
 * rotator_hw_revision:
 * 0 == 7x30
 * 1 == 8x60
 * 2 == 8960
 *
 */
struct tile_parm {
	unsigned int width;  /* tile's width */
	unsigned int height; /* tile's height */
	unsigned int row_tile_w; /* tiles per row's width */
	unsigned int row_tile_h; /* tiles per row's height */
};

struct msm_rotator_mem_planes {
	unsigned int num_planes;
	unsigned int plane_size[4];
	unsigned int total_size;
};

#define checkoffset(offset, size, max_size) \
	((size) > (max_size) || (offset) > ((max_size) - (size)))

struct msm_rotator_fd_info {
	int pid;
	int ref_cnt;
	struct list_head list;
};

struct msm_rotator_dev {
	void __iomem *io_base;
	int irq;
	struct msm_rotator_img_info *img_info[MAX_SESSIONS];
	struct clk *core_clk;
	struct msm_rotator_fd_info *fd_info[MAX_SESSIONS];
	struct list_head fd_list;
	struct clk *pclk;
	int rot_clk_state;
	struct regulator *regulator;
	struct delayed_work rot_clk_work;
	struct clk *imem_clk;
	int imem_clk_state;
	struct delayed_work imem_clk_work;
	struct platform_device *pdev;
	struct cdev cdev;
	struct device *device;
	struct class *class;
	dev_t dev_num;
	int processing;
	int last_session_idx;
	struct mutex rotator_lock;
	struct mutex imem_lock;
	int imem_owner;
	wait_queue_head_t wq;
	struct ion_client *client;
	#ifdef CONFIG_MSM_BUS_SCALING
	uint32_t bus_client_handle;
	#endif
};

#define COMPONENT_5BITS 1
#define COMPONENT_6BITS 2
#define COMPONENT_8BITS 3

static struct msm_rotator_dev *msm_rotator_dev;

enum {
	CLK_EN,
	CLK_DIS,
	CLK_SUSPEND,
};

int msm_rotator_iommu_map_buf(int mem_id, unsigned char src,
	unsigned long *start, unsigned long *len,
	struct ion_handle **pihdl)
{
	if (!msm_rotator_dev->client)
		return -EINVAL;

	*pihdl = ion_import_fd(msm_rotator_dev->client, mem_id);
	if (IS_ERR_OR_NULL(*pihdl)) {
		pr_err("ion_import_fd() failed\n");
		return PTR_ERR(*pihdl);
	}
	pr_debug("%s(): ion_hdl %p, ion_buf %p\n", __func__, *pihdl,
		ion_share(msm_rotator_dev->client, *pihdl));

	if (ion_map_iommu(msm_rotator_dev->client,
		*pihdl,	ROTATOR_DOMAIN, GEN_POOL,
		SZ_4K, 0, start, len, 0, ION_IOMMU_UNMAP_DELAYED)) {
		pr_err("ion_map_iommu() failed\n");
		return -EINVAL;
	}

	pr_debug("%s(): mem_id %d, start 0x%lx, len 0x%lx\n",
		__func__, mem_id, *start, *len);
	return 0;
}

int msm_rotator_imem_allocate(int requestor)
{
	int rc = 0;

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	switch (requestor) {
	case ROTATOR_REQUEST:
		if (mutex_trylock(&msm_rotator_dev->imem_lock)) {
			msm_rotator_dev->imem_owner = ROTATOR_REQUEST;
			rc = 1;
		} else
			rc = 0;
		break;
	case JPEG_REQUEST:
		mutex_lock(&msm_rotator_dev->imem_lock);
		msm_rotator_dev->imem_owner = JPEG_REQUEST;
		rc = 1;
		break;
	default:
		rc = 0;
	}
#else
	if (requestor == JPEG_REQUEST)
		rc = 1;
#endif
	if (rc == 1) {
		cancel_delayed_work(&msm_rotator_dev->imem_clk_work);
		if (msm_rotator_dev->imem_clk_state != CLK_EN
			&& msm_rotator_dev->imem_clk) {
			clk_prepare_enable(msm_rotator_dev->imem_clk);
			msm_rotator_dev->imem_clk_state = CLK_EN;
		}
	}

	return rc;
}
EXPORT_SYMBOL(msm_rotator_imem_allocate);

void msm_rotator_imem_free(int requestor)
{
#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (msm_rotator_dev->imem_owner == requestor) {
		schedule_delayed_work(&msm_rotator_dev->imem_clk_work, HZ);
		mutex_unlock(&msm_rotator_dev->imem_lock);
	}
#else
	if (requestor == JPEG_REQUEST)
		schedule_delayed_work(&msm_rotator_dev->imem_clk_work, HZ);
#endif
}
EXPORT_SYMBOL(msm_rotator_imem_free);

static void msm_rotator_imem_clk_work_f(struct work_struct *work)
{
#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (mutex_trylock(&msm_rotator_dev->imem_lock)) {
		if (msm_rotator_dev->imem_clk_state == CLK_EN
		     && msm_rotator_dev->imem_clk) {
			clk_disable_unprepare(msm_rotator_dev->imem_clk);
			msm_rotator_dev->imem_clk_state = CLK_DIS;
		} else if (msm_rotator_dev->imem_clk_state == CLK_SUSPEND)
			msm_rotator_dev->imem_clk_state = CLK_DIS;
		mutex_unlock(&msm_rotator_dev->imem_lock);
	}
#endif
}

/* enable clocks needed by rotator block */
static void enable_rot_clks(void)
{
	if (msm_rotator_dev->regulator)
		regulator_enable(msm_rotator_dev->regulator);
	if (msm_rotator_dev->core_clk != NULL)
		clk_prepare_enable(msm_rotator_dev->core_clk);
	if (msm_rotator_dev->pclk != NULL)
		clk_prepare_enable(msm_rotator_dev->pclk);
}

/* disable clocks needed by rotator block */
static void disable_rot_clks(void)
{
	if (msm_rotator_dev->core_clk != NULL)
		clk_disable_unprepare(msm_rotator_dev->core_clk);
	if (msm_rotator_dev->pclk != NULL)
		clk_disable_unprepare(msm_rotator_dev->pclk);
	if (msm_rotator_dev->regulator)
		regulator_disable(msm_rotator_dev->regulator);
}

static void msm_rotator_rot_clk_work_f(struct work_struct *work)
{
	if (mutex_trylock(&msm_rotator_dev->rotator_lock)) {
		if (msm_rotator_dev->rot_clk_state == CLK_EN) {
			disable_rot_clks();
			msm_rotator_dev->rot_clk_state = CLK_DIS;
		} else if (msm_rotator_dev->rot_clk_state == CLK_SUSPEND)
			msm_rotator_dev->rot_clk_state = CLK_DIS;
		mutex_unlock(&msm_rotator_dev->rotator_lock);
	}
}

static irqreturn_t msm_rotator_isr(int irq, void *dev_id)
{
	if (msm_rotator_dev->processing) {
		msm_rotator_dev->processing = 0;
		wake_up(&msm_rotator_dev->wq);
	} else
		printk(KERN_WARNING "%s: unexpected interrupt\n", DRIVER_NAME);

	return IRQ_HANDLED;
}

static unsigned int tile_size(unsigned int src_width,
		unsigned int src_height,
		const struct tile_parm *tp)
{
	unsigned int tile_w, tile_h;
	unsigned int row_num_w, row_num_h;
	tile_w = tp->width * tp->row_tile_w;
	tile_h = tp->height * tp->row_tile_h;
	row_num_w = (src_width + tile_w - 1) / tile_w;
	row_num_h = (src_height + tile_h - 1) / tile_h;
	return ((row_num_w * row_num_h * tile_w * tile_h) + 8191) & ~8191;
}

static int get_bpp(int format)
{
	switch (format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
		return 2;

	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
		return 4;

	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		return 1;

	case MDP_RGB_888:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		return 3;

	case MDP_YCRYCB_H2V1:
		return 2;/* YCrYCb interleave */

	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
		return 1;

	default:
		return -1;
	}

}

static int msm_rotator_get_plane_sizes(uint32_t format,	uint32_t w, uint32_t h,
				       struct msm_rotator_mem_planes *p)
{
	/*
	 * each row of samsung tile consists of two tiles in height
	 * and two tiles in width which means width should align to
	 * 64 x 2 bytes and height should align to 32 x 2 bytes.
	 * video decoder generate two tiles in width and one tile
	 * in height which ends up height align to 32 X 1 bytes.
	 */
	const struct tile_parm tile = {64, 32, 2, 1};
	int i;

	if (p == NULL)
		return -EINVAL;

	if ((w > MSM_ROTATOR_MAX_W) || (h > MSM_ROTATOR_MAX_H))
		return -ERANGE;

	memset(p, 0, sizeof(*p));

	switch (format) {
	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
	case MDP_RGB_888:
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_YCRYCB_H2V1:
		p->num_planes = 1;
		p->plane_size[0] = w * h * get_bpp(format);
		break;
	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
		p->num_planes = 2;
		p->plane_size[0] = w * h;
		p->plane_size[1] = w * h;
		break;
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
		p->num_planes = 2;
		p->plane_size[0] = w * h;
		p->plane_size[1] = w * h / 2;
		break;
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		p->num_planes = 2;
		p->plane_size[0] = tile_size(w, h, &tile);
		p->plane_size[1] = tile_size(w, h/2, &tile);
		break;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
		p->num_planes = 3;
		p->plane_size[0] = w * h;
		p->plane_size[1] = (w / 2) * (h / 2);
		p->plane_size[2] = (w / 2) * (h / 2);
		break;
	case MDP_Y_CR_CB_GH2V2:
		p->num_planes = 3;
		p->plane_size[0] = ALIGN(w, 16) * h;
		p->plane_size[1] = ALIGN(w / 2, 16) * (h / 2);
		p->plane_size[2] = ALIGN(w / 2, 16) * (h / 2);
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < p->num_planes; i++)
		p->total_size += p->plane_size[i];

	return 0;
}

static int msm_rotator_ycxcx_h2v1(struct msm_rotator_img_info *info,
				  unsigned int in_paddr,
				  unsigned int out_paddr,
				  unsigned int use_imem,
				  int new_session,
				  unsigned int in_chroma_paddr,
				  unsigned int out_chroma_paddr)
{
	int bpp;

	if (info->src.format != info->dst.format)
		return -EINVAL;

	bpp = get_bpp(info->src.format);
	if (bpp < 0)
		return -ENOTTY;

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(in_chroma_paddr, MSM_ROTATOR_SRCP1_ADDR);
	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);

	if (new_session) {
		iowrite32(info->src.width |
			  info->src.width << 16,
			  MSM_ROTATOR_SRC_YSTRIDE1);
		if (info->rotations & MDP_ROT_90)
			iowrite32(info->dst.width |
				  info->dst.width*2 << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);
		else
			iowrite32(info->dst.width |
				  info->dst.width << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);
		if (info->src.format == MDP_Y_CBCR_H2V1) {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		} else {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		}
		iowrite32((1  << 18) | 		/* chroma sampling 1=H2V1 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);
		iowrite32(0 << 29 | 		/* frame format 0 = linear */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  2 << 19 | 		/* fetch planes 2 = pseudo */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  1 << 13 | 		/* unpack count 0=1 component */
			  (bpp-1) << 9 |	/* src Bpp 0=1 byte ... */
			  0 << 8  | 		/* has alpha */
			  0 << 6  | 		/* alpha bits 3=8bits */
			  3 << 4  | 		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  | 		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,   		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}

	return 0;
}

static int msm_rotator_ycxcx_h2v2(struct msm_rotator_img_info *info,
				  unsigned int in_paddr,
				  unsigned int out_paddr,
				  unsigned int use_imem,
				  int new_session,
				  unsigned int in_chroma_paddr,
				  unsigned int out_chroma_paddr,
				  unsigned int in_chroma2_paddr)
{
	uint32_t dst_format;
	int is_tile = 0;

	switch (info->src.format) {
	case MDP_Y_CRCB_H2V2_TILE:
		is_tile = 1;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CRCB_H2V2:
		dst_format = MDP_Y_CRCB_H2V2;
		break;
	case MDP_Y_CBCR_H2V2_TILE:
		is_tile = 1;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CBCR_H2V2:
		dst_format = MDP_Y_CBCR_H2V2;
		break;
	default:
		return -EINVAL;
	}
	if (info->dst.format  != dst_format)
		return -EINVAL;

	/* rotator expects YCbCr for planar input format */
	if ((info->src.format == MDP_Y_CR_CB_H2V2 ||
	    info->src.format == MDP_Y_CR_CB_GH2V2) &&
	    rotator_hw_revision < ROTATOR_REVISION_V2)
		swap(in_chroma_paddr, in_chroma2_paddr);

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(in_chroma_paddr, MSM_ROTATOR_SRCP1_ADDR);
	iowrite32(in_chroma2_paddr, MSM_ROTATOR_SRCP2_ADDR);

	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			((info->dst_y * info->dst.width)/2 + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);

	if (new_session) {
		if (in_chroma2_paddr) {
			if (info->src.format == MDP_Y_CR_CB_GH2V2) {
				iowrite32(ALIGN(info->src.width, 16) |
					ALIGN((info->src.width / 2), 16) << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
				iowrite32(ALIGN((info->src.width / 2), 16),
					MSM_ROTATOR_SRC_YSTRIDE2);
			} else {
				iowrite32(info->src.width |
					(info->src.width / 2) << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
				iowrite32((info->src.width / 2),
					MSM_ROTATOR_SRC_YSTRIDE2);
			}
		} else {
			iowrite32(info->src.width |
					info->src.width << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
		}
		iowrite32(info->dst.width |
				info->dst.width << 16,
				MSM_ROTATOR_OUT_YSTRIDE1);

		if (dst_format == MDP_Y_CBCR_H2V2) {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		} else {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		}
		iowrite32((3  << 18) | 		/* chroma sampling 3=4:2:0 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);

		iowrite32((is_tile ? 2 : 0) << 29 |  /* frame format */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  (in_chroma2_paddr ? 1 : 2) << 19 | /* fetch planes */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  1 << 13 | 		/* unpack count 0=1 component */
			  0 << 9  |		/* src Bpp 0=1 byte ... */
			  0 << 8  | 		/* has alpha */
			  0 << 6  | 		/* alpha bits 3=8bits */
			  3 << 4  | 		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  | 		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,   		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}
	return 0;
}

static int msm_rotator_ycrycb(struct msm_rotator_img_info *info,
			      unsigned int in_paddr,
			      unsigned int out_paddr,
			      unsigned int use_imem,
			      int new_session,
			      unsigned int out_chroma_paddr)
{
	int bpp;
	uint32_t dst_format;

	if (info->src.format == MDP_YCRYCB_H2V1)
		dst_format = MDP_Y_CRCB_H2V1;
	else
		return -EINVAL;

	if (info->dst.format != dst_format)
		return -EINVAL;

	bpp = get_bpp(info->src.format);
	if (bpp < 0)
		return -ENOTTY;

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			((info->dst_y * info->dst.width)/2 + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);

	if (new_session) {
		iowrite32(info->src.width * bpp,
			  MSM_ROTATOR_SRC_YSTRIDE1);
		if (info->rotations & MDP_ROT_90)
			iowrite32(info->dst.width |
				  (info->dst.width*2) << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);
		else
			iowrite32(info->dst.width |
				  (info->dst.width) << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);

		iowrite32(GET_PACK_PATTERN(CLR_Y, CLR_CR, CLR_Y, CLR_CB, 8),
			  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
		iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
			  MSM_ROTATOR_OUT_PACK_PATTERN1);
		iowrite32((1  << 18) | 		/* chroma sampling 1=H2V1 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);
		iowrite32(0 << 29 | 		/* frame format 0 = linear */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  0 << 19 | 		/* fetch planes 0=interleaved */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  3 << 13 | 		/* unpack count 0=1 component */
			  (bpp-1) << 9 |	/* src Bpp 0=1 byte ... */
			  0 << 8  | 		/* has alpha */
			  0 << 6  | 		/* alpha bits 3=8bits */
			  3 << 4  | 		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  | 		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,   		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}

	return 0;
}

static int msm_rotator_rgb_types(struct msm_rotator_img_info *info,
				 unsigned int in_paddr,
				 unsigned int out_paddr,
				 unsigned int use_imem,
				 int new_session)
{
	int bpp, abits, rbits, gbits, bbits;

	if (info->src.format != info->dst.format)
		return -EINVAL;

	bpp = get_bpp(info->src.format);
	if (bpp < 0)
		return -ENOTTY;

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x) * bpp,
		  MSM_ROTATOR_OUTP0_ADDR);

	if (new_session) {
		iowrite32(info->src.width * bpp, MSM_ROTATOR_SRC_YSTRIDE1);
		iowrite32(info->dst.width * bpp, MSM_ROTATOR_OUT_YSTRIDE1);
		iowrite32((0  << 18) | 		/* chroma sampling 0=rgb */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);
		switch (info->src.format) {
		case MDP_RGB_565:
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = 0;
			rbits = COMPONENT_5BITS;
			gbits = COMPONENT_6BITS;
			bbits = COMPONENT_5BITS;
			break;

		case MDP_BGR_565:
			iowrite32(GET_PACK_PATTERN(0, CLR_B, CLR_G, CLR_R, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, CLR_B, CLR_G, CLR_R, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = 0;
			rbits = COMPONENT_5BITS;
			gbits = COMPONENT_6BITS;
			bbits = COMPONENT_5BITS;
			break;

		case MDP_RGB_888:
		case MDP_YCBCR_H1V1:
		case MDP_YCRCB_H1V1:
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = 0;
			rbits = COMPONENT_8BITS;
			gbits = COMPONENT_8BITS;
			bbits = COMPONENT_8BITS;
			break;

		case MDP_ARGB_8888:
		case MDP_RGBA_8888:
		case MDP_XRGB_8888:
		case MDP_RGBX_8888:
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_R, CLR_G,
						   CLR_B, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_R, CLR_G,
						   CLR_B, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = COMPONENT_8BITS;
			rbits = COMPONENT_8BITS;
			gbits = COMPONENT_8BITS;
			bbits = COMPONENT_8BITS;
			break;

		case MDP_BGRA_8888:
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_B, CLR_G,
						   CLR_R, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_B, CLR_G,
						   CLR_R, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = COMPONENT_8BITS;
			rbits = COMPONENT_8BITS;
			gbits = COMPONENT_8BITS;
			bbits = COMPONENT_8BITS;
			break;

		default:
			return -EINVAL;
		}
		iowrite32(0 << 29 | 		/* frame format 0 = linear */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  0 << 19 | 		/* fetch planes 0=interleaved */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  (abits ? 3 : 2) << 13 | /* unpack count 0=1 comp */
			  (bpp-1) << 9 | 	/* src Bpp 0=1 byte ... */
			  (abits ? 1 : 0) << 8  | /* has alpha */
			  abits << 6  | 	/* alpha bits 3=8bits */
			  rbits << 4  | 	/* R/Cr bits 1=5 2=6 3=8 */
			  bbits << 2  | 	/* B/Cb bits 1=5 2=6 3=8 */
			  gbits << 0,   	/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}

	return 0;
}

static int get_img(struct msmfb_data *fbd, unsigned char src,
	unsigned long *start, unsigned long *len, struct file **p_file,
	int *p_need, struct ion_handle **p_ihdl)
{
	int ret = 0;
#ifdef CONFIG_FB
	struct file *file = NULL;
	int put_needed, fb_num;
#endif
#ifdef CONFIG_ANDROID_PMEM
	unsigned long vstart;
#endif

	*p_need = 0;

#ifdef CONFIG_FB
	if (fbd->flags & MDP_MEMORY_ID_TYPE_FB) {
		file = fget_light(fbd->memory_id, &put_needed);
		if (file == NULL) {
			pr_err("fget_light returned NULL\n");
			return -EINVAL;
		}

		if (MAJOR(file->f_dentry->d_inode->i_rdev) == FB_MAJOR) {
			fb_num = MINOR(file->f_dentry->d_inode->i_rdev);
			if (get_fb_phys_info(start, len, fb_num,
				ROTATOR_SUBSYSTEM_ID)) {
				pr_err("get_fb_phys_info() failed\n");
				ret = -1;
			} else {
				*p_file = file;
				*p_need = put_needed;
			}
		} else {
			pr_err("invalid FB_MAJOR failed\n");
			ret = -1;
		}
		if (ret)
			fput_light(file, put_needed);
		return ret;
	}
#endif

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	return msm_rotator_iommu_map_buf(fbd->memory_id, src, start,
		len, p_ihdl);
#endif
#ifdef CONFIG_ANDROID_PMEM
	if (!get_pmem_file(fbd->memory_id, start, &vstart, len, p_file))
		return 0;
	else
		return -ENOMEM;
#endif

}

static void put_img(struct file *p_file, struct ion_handle *p_ihdl)
{
#ifdef CONFIG_ANDROID_PMEM
	if (p_file != NULL)
		put_pmem_file(p_file);
#endif
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	if (!IS_ERR_OR_NULL(p_ihdl)) {
		pr_debug("%s(): p_ihdl %p\n", __func__, p_ihdl);
		ion_unmap_iommu(msm_rotator_dev->client,
			p_ihdl, ROTATOR_DOMAIN, GEN_POOL);

		ion_free(msm_rotator_dev->client, p_ihdl);
	}
#endif
}
static int msm_rotator_do_rotate(unsigned long arg)
{
	unsigned int status, format;
	struct msm_rotator_data_info info;
	unsigned int in_paddr, out_paddr;
	unsigned long src_len, dst_len;
	int use_imem = 0, rc = 0, s;
	struct file *srcp0_file = NULL, *dstp0_file = NULL;
	struct file *srcp1_file = NULL, *dstp1_file = NULL;
	struct ion_handle *srcp0_ihdl = NULL, *dstp0_ihdl = NULL;
	struct ion_handle *srcp1_ihdl = NULL, *dstp1_ihdl = NULL;
	int ps0_need, p_need;
	unsigned int in_chroma_paddr = 0, out_chroma_paddr = 0;
	unsigned int in_chroma2_paddr = 0;
	struct msm_rotator_img_info *img_info;
	struct msm_rotator_mem_planes src_planes, dst_planes;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	for (s = 0; s < MAX_SESSIONS; s++)
		if ((msm_rotator_dev->img_info[s] != NULL) &&
			(info.session_id ==
			(unsigned int)msm_rotator_dev->img_info[s]
			))
			break;

	if (s == MAX_SESSIONS) {
		dev_dbg(msm_rotator_dev->device,
			"%s() : Attempt to use invalid session_id %d\n",
			__func__, s);
		rc = -EINVAL;
		goto do_rotate_unlock_mutex;
	}

	if (msm_rotator_dev->img_info[s]->enable == 0) {
		dev_dbg(msm_rotator_dev->device,
			"%s() : Session_id %d not enabled \n",
			__func__, s);
		rc = -EINVAL;
		goto do_rotate_unlock_mutex;
	}

	img_info = msm_rotator_dev->img_info[s];
	if (msm_rotator_get_plane_sizes(img_info->src.format,
					img_info->src.width,
					img_info->src.height,
					&src_planes)) {
		pr_err("%s: invalid src format\n", __func__);
		rc = -EINVAL;
		goto do_rotate_unlock_mutex;
	}
	if (msm_rotator_get_plane_sizes(img_info->dst.format,
					img_info->dst.width,
					img_info->dst.height,
					&dst_planes)) {
		pr_err("%s: invalid dst format\n", __func__);
		rc = -EINVAL;
		goto do_rotate_unlock_mutex;
	}

	rc = get_img(&info.src, 1, (unsigned long *)&in_paddr,
			(unsigned long *)&src_len, &srcp0_file, &ps0_need,
			&srcp0_ihdl);
	if (rc) {
		pr_err("%s: in get_img() failed id=0x%08x\n",
			DRIVER_NAME, info.src.memory_id);
		goto do_rotate_unlock_mutex;
	}

	rc = get_img(&info.dst, 0, (unsigned long *)&out_paddr,
			(unsigned long *)&dst_len, &dstp0_file, &p_need,
			&dstp0_ihdl);
	if (rc) {
		pr_err("%s: out get_img() failed id=0x%08x\n",
		       DRIVER_NAME, info.dst.memory_id);
		goto do_rotate_unlock_mutex;
	}

	format = msm_rotator_dev->img_info[s]->src.format;
	if (((info.version_key & VERSION_KEY_MASK) == 0xA5B4C300) &&
			((info.version_key & ~VERSION_KEY_MASK) > 0) &&
			(src_planes.num_planes == 2)) {
		if (checkoffset(info.src.offset,
				src_planes.plane_size[0],
				src_len)) {
			pr_err("%s: invalid src buffer (len=%lu offset=%x)\n",
			       __func__, src_len, info.src.offset);
			rc = -ERANGE;
			goto do_rotate_unlock_mutex;
		}
		if (checkoffset(info.dst.offset,
				dst_planes.plane_size[0],
				dst_len)) {
			pr_err("%s: invalid dst buffer (len=%lu offset=%x)\n",
			       __func__, dst_len, info.dst.offset);
			rc = -ERANGE;
			goto do_rotate_unlock_mutex;
		}

		rc = get_img(&info.src_chroma, 1,
				(unsigned long *)&in_chroma_paddr,
				(unsigned long *)&src_len, &srcp1_file, &p_need,
				&srcp1_ihdl);
		if (rc) {
			pr_err("%s: in chroma get_img() failed id=0x%08x\n",
				DRIVER_NAME, info.src_chroma.memory_id);
			goto do_rotate_unlock_mutex;
		}

		rc = get_img(&info.dst_chroma, 0,
				(unsigned long *)&out_chroma_paddr,
				(unsigned long *)&dst_len, &dstp1_file, &p_need,
				&dstp1_ihdl);
		if (rc) {
			pr_err("%s: out chroma get_img() failed id=0x%08x\n",
				DRIVER_NAME, info.dst_chroma.memory_id);
			goto do_rotate_unlock_mutex;
		}

		if (checkoffset(info.src_chroma.offset,
				src_planes.plane_size[1],
				src_len)) {
			pr_err("%s: invalid chr src buf len=%lu offset=%x\n",
			       __func__, src_len, info.src_chroma.offset);
			rc = -ERANGE;
			goto do_rotate_unlock_mutex;
		}

		if (checkoffset(info.dst_chroma.offset,
				src_planes.plane_size[1],
				dst_len)) {
			pr_err("%s: invalid chr dst buf len=%lu offset=%x\n",
			       __func__, dst_len, info.dst_chroma.offset);
			rc = -ERANGE;
			goto do_rotate_unlock_mutex;
		}

		in_chroma_paddr += info.src_chroma.offset;
		out_chroma_paddr += info.dst_chroma.offset;
	} else {
		if (checkoffset(info.src.offset,
				src_planes.total_size,
				src_len)) {
			pr_err("%s: invalid src buffer (len=%lu offset=%x)\n",
			       __func__, src_len, info.src.offset);
			rc = -ERANGE;
			goto do_rotate_unlock_mutex;
		}
		if (checkoffset(info.dst.offset,
				dst_planes.total_size,
				dst_len)) {
			pr_err("%s: invalid dst buffer (len=%lu offset=%x)\n",
			       __func__, dst_len, info.dst.offset);
			rc = -ERANGE;
			goto do_rotate_unlock_mutex;
		}
	}

	in_paddr += info.src.offset;
	out_paddr += info.dst.offset;

	if (!in_chroma_paddr && src_planes.num_planes >= 2)
		in_chroma_paddr = in_paddr + src_planes.plane_size[0];
	if (!out_chroma_paddr && dst_planes.num_planes >= 2)
		out_chroma_paddr = out_paddr + dst_planes.plane_size[0];
	if (src_planes.num_planes >= 3)
		in_chroma2_paddr = in_chroma_paddr + src_planes.plane_size[1];

	cancel_delayed_work(&msm_rotator_dev->rot_clk_work);
	if (msm_rotator_dev->rot_clk_state != CLK_EN) {
		enable_rot_clks();
		msm_rotator_dev->rot_clk_state = CLK_EN;
	}
	enable_irq(msm_rotator_dev->irq);

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	use_imem = msm_rotator_imem_allocate(ROTATOR_REQUEST);
#else
	use_imem = 0;
#endif
	/*
	 * workaround for a hardware bug. rotator hardware hangs when we
	 * use write burst beat size 16 on 128X128 tile fetch mode. As a
	 * temporary fix use 0x42 for BURST_SIZE when imem used.
	 */
	if (use_imem)
		iowrite32(0x42, MSM_ROTATOR_MAX_BURST_SIZE);

	iowrite32(((msm_rotator_dev->img_info[s]->src_rect.h & 0x1fff)
				<< 16) |
		  (msm_rotator_dev->img_info[s]->src_rect.w & 0x1fff),
		  MSM_ROTATOR_SRC_SIZE);
	iowrite32(((msm_rotator_dev->img_info[s]->src_rect.y & 0x1fff)
				<< 16) |
		  (msm_rotator_dev->img_info[s]->src_rect.x & 0x1fff),
		  MSM_ROTATOR_SRC_XY);
	iowrite32(((msm_rotator_dev->img_info[s]->src.height & 0x1fff)
				<< 16) |
		  (msm_rotator_dev->img_info[s]->src.width & 0x1fff),
		  MSM_ROTATOR_SRC_IMAGE_SIZE);

	switch (format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_RGB_888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_XRGB_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		rc = msm_rotator_rgb_types(msm_rotator_dev->img_info[s],
					   in_paddr, out_paddr,
					   use_imem,
					   msm_rotator_dev->last_session_idx
								!= s);
		break;
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		rc = msm_rotator_ycxcx_h2v2(msm_rotator_dev->img_info[s],
					    in_paddr, out_paddr, use_imem,
					    msm_rotator_dev->last_session_idx
								!= s,
					    in_chroma_paddr,
					    out_chroma_paddr,
					    in_chroma2_paddr);
		break;
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
		rc = msm_rotator_ycxcx_h2v1(msm_rotator_dev->img_info[s],
					    in_paddr, out_paddr, use_imem,
					    msm_rotator_dev->last_session_idx
								!= s,
					    in_chroma_paddr,
					    out_chroma_paddr);
		break;
	case MDP_YCRYCB_H2V1:
		rc = msm_rotator_ycrycb(msm_rotator_dev->img_info[s],
				in_paddr, out_paddr, use_imem,
				msm_rotator_dev->last_session_idx != s,
				out_chroma_paddr);
		break;
	default:
		rc = -EINVAL;
		goto do_rotate_exit;
	}

	if (rc != 0) {
		msm_rotator_dev->last_session_idx = INVALID_SESSION;
		goto do_rotate_exit;
	}

	iowrite32(3, MSM_ROTATOR_INTR_ENABLE);

	msm_rotator_dev->processing = 1;
	iowrite32(0x1, MSM_ROTATOR_START);

	wait_event(msm_rotator_dev->wq,
		   (msm_rotator_dev->processing == 0));
	status = (unsigned char)ioread32(MSM_ROTATOR_INTR_STATUS);
	if ((status & 0x03) != 0x01)
		rc = -EFAULT;
	iowrite32(0, MSM_ROTATOR_INTR_ENABLE);
	iowrite32(3, MSM_ROTATOR_INTR_CLEAR);

do_rotate_exit:
	disable_irq(msm_rotator_dev->irq);
#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	msm_rotator_imem_free(ROTATOR_REQUEST);
#endif
	schedule_delayed_work(&msm_rotator_dev->rot_clk_work, HZ);
do_rotate_unlock_mutex:
	put_img(dstp1_file, dstp1_ihdl);
	put_img(srcp1_file, srcp1_ihdl);
	put_img(dstp0_file, dstp0_ihdl);

	/* only source may use frame buffer */
	if (info.src.flags & MDP_MEMORY_ID_TYPE_FB)
		fput_light(srcp0_file, ps0_need);
	else
		put_img(srcp0_file, srcp0_ihdl);
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	dev_dbg(msm_rotator_dev->device, "%s() returning rc = %d\n",
		__func__, rc);
	return rc;
}

static void msm_rotator_set_perf_level(u32 wh, u32 is_rgb)
{
	u32 perf_level;

	if (is_rgb)
		perf_level = 1;
	else if (wh <= (640 * 480))
		perf_level = 2;
	else if (wh <= (736 * 1280))
		perf_level = 3;
	else
		perf_level = 4;

#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_scale_client_update_request(msm_rotator_dev->bus_client_handle,
		perf_level);
#endif

}

static int msm_rotator_start(unsigned long arg,
			     struct msm_rotator_fd_info *fd_info)
{
	struct msm_rotator_img_info info;
	int rc = 0;
	int s, is_rgb = 0;
	int first_free_index = INVALID_SESSION;
	unsigned int dst_w, dst_h;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;

	if ((info.rotations > MSM_ROTATOR_MAX_ROT) ||
	    (info.src.height > MSM_ROTATOR_MAX_H) ||
	    (info.src.width > MSM_ROTATOR_MAX_W) ||
	    (info.dst.height > MSM_ROTATOR_MAX_H) ||
	    (info.dst.width > MSM_ROTATOR_MAX_W) ||
	    (info.downscale_ratio > MAX_DOWNSCALE_RATIO)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (info.rotations & MDP_ROT_90) {
		dst_w = info.src_rect.h >> info.downscale_ratio;
		dst_h = info.src_rect.w >> info.downscale_ratio;
	} else {
		dst_w = info.src_rect.w >> info.downscale_ratio;
		dst_h = info.src_rect.h >> info.downscale_ratio;
	}

	if (checkoffset(info.src_rect.x, info.src_rect.w, info.src.width) ||
	    checkoffset(info.src_rect.y, info.src_rect.h, info.src.height) ||
	    checkoffset(info.dst_x, dst_w, info.dst.width) ||
	    checkoffset(info.dst_y, dst_h, info.dst.height)) {
		pr_err("%s: Invalid src or dst rect\n", __func__);
		return -ERANGE;
	}

	switch (info.src.format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_RGB_888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_XRGB_8888:
	case MDP_RGBX_8888:
	case MDP_BGRA_8888:
		is_rgb = 1;
		info.dst.format = info.src.format;
		break;
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		info.dst.format = info.src.format;
		break;
	case MDP_YCRYCB_H2V1:
		info.dst.format = MDP_Y_CRCB_H2V1;
		break;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CBCR_H2V2_TILE:
		info.dst.format = MDP_Y_CBCR_H2V2;
		break;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CRCB_H2V2_TILE:
		info.dst.format = MDP_Y_CRCB_H2V2;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&msm_rotator_dev->rotator_lock);

	msm_rotator_set_perf_level((info.src.width*info.src.height), is_rgb);

	for (s = 0; s < MAX_SESSIONS; s++) {
		if ((msm_rotator_dev->img_info[s] != NULL) &&
			(info.session_id ==
			(unsigned int)msm_rotator_dev->img_info[s]
			)) {
			*(msm_rotator_dev->img_info[s]) = info;
			msm_rotator_dev->fd_info[s] = fd_info;

			if (msm_rotator_dev->last_session_idx == s)
				msm_rotator_dev->last_session_idx =
				INVALID_SESSION;
			break;
		}

		if ((msm_rotator_dev->img_info[s] == NULL) &&
			(first_free_index ==
			INVALID_SESSION))
			first_free_index = s;
	}

	if ((s == MAX_SESSIONS) && (first_free_index != INVALID_SESSION)) {
		/* allocate a session id */
		msm_rotator_dev->img_info[first_free_index] =
			kzalloc(sizeof(struct msm_rotator_img_info),
					GFP_KERNEL);
		if (!msm_rotator_dev->img_info[first_free_index]) {
			printk(KERN_ERR "%s : unable to alloc mem\n",
					__func__);
			rc = -ENOMEM;
			goto rotator_start_exit;
		}
		info.session_id = (unsigned int)
			msm_rotator_dev->img_info[first_free_index];
		*(msm_rotator_dev->img_info[first_free_index]) = info;
		msm_rotator_dev->fd_info[first_free_index] = fd_info;

		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			rc = -EFAULT;
	} else if (s == MAX_SESSIONS) {
		dev_dbg(msm_rotator_dev->device, "%s: all sessions in use\n",
			__func__);
		rc = -EBUSY;
	}

rotator_start_exit:
	mutex_unlock(&msm_rotator_dev->rotator_lock);

	return rc;
}

static int msm_rotator_finish(unsigned long arg)
{
	int rc = 0;
	int s;
	unsigned int session_id;

	if (copy_from_user(&session_id, (void __user *)arg, sizeof(s)))
		return -EFAULT;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	for (s = 0; s < MAX_SESSIONS; s++) {
		if ((msm_rotator_dev->img_info[s] != NULL) &&
			(session_id ==
			(unsigned int)msm_rotator_dev->img_info[s])) {
			if (msm_rotator_dev->last_session_idx == s)
				msm_rotator_dev->last_session_idx =
					INVALID_SESSION;
			kfree(msm_rotator_dev->img_info[s]);
			msm_rotator_dev->img_info[s] = NULL;
			msm_rotator_dev->fd_info[s] = NULL;
			break;
		}
	}

	if (s == MAX_SESSIONS)
		rc = -EINVAL;
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_scale_client_update_request(msm_rotator_dev->bus_client_handle,
		0);
#endif
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return rc;
}

static int
msm_rotator_open(struct inode *inode, struct file *filp)
{
	struct msm_rotator_fd_info *tmp, *fd_info = NULL;
	int i;

	if (filp->private_data)
		return -EBUSY;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	for (i = 0; i < MAX_SESSIONS; i++) {
		if (msm_rotator_dev->fd_info[i] == NULL)
			break;
	}

	if (i == MAX_SESSIONS) {
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return -EBUSY;
	}

	list_for_each_entry(tmp, &msm_rotator_dev->fd_list, list) {
		if (tmp->pid == current->pid) {
			fd_info = tmp;
			break;
		}
	}

	if (!fd_info) {
		fd_info = kzalloc(sizeof(*fd_info), GFP_KERNEL);
		if (!fd_info) {
			mutex_unlock(&msm_rotator_dev->rotator_lock);
			pr_err("%s: insufficient memory to alloc resources\n",
			       __func__);
			return -ENOMEM;
		}
		list_add(&fd_info->list, &msm_rotator_dev->fd_list);
		fd_info->pid = current->pid;
	}
	fd_info->ref_cnt++;
	mutex_unlock(&msm_rotator_dev->rotator_lock);

	filp->private_data = fd_info;

	return 0;
}

static int
msm_rotator_close(struct inode *inode, struct file *filp)
{
	struct msm_rotator_fd_info *fd_info;
	int s;

	fd_info = (struct msm_rotator_fd_info *)filp->private_data;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	if (--fd_info->ref_cnt > 0) {
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return 0;
	}

	for (s = 0; s < MAX_SESSIONS; s++) {
		if (msm_rotator_dev->img_info[s] != NULL &&
			msm_rotator_dev->fd_info[s] == fd_info) {
			pr_debug("%s: freeing rotator session %p (pid %d)\n",
				 __func__, msm_rotator_dev->img_info[s],
				 fd_info->pid);
			kfree(msm_rotator_dev->img_info[s]);
			msm_rotator_dev->img_info[s] = NULL;
			msm_rotator_dev->fd_info[s] = NULL;
			if (msm_rotator_dev->last_session_idx == s)
				msm_rotator_dev->last_session_idx =
					INVALID_SESSION;
		}
	}
	list_del(&fd_info->list);
	kfree(fd_info);
	mutex_unlock(&msm_rotator_dev->rotator_lock);

	return 0;
}

static long msm_rotator_ioctl(struct file *file, unsigned cmd,
						 unsigned long arg)
{
	struct msm_rotator_fd_info *fd_info;

	if (_IOC_TYPE(cmd) != MSM_ROTATOR_IOCTL_MAGIC)
		return -ENOTTY;

	fd_info = (struct msm_rotator_fd_info *)file->private_data;

	switch (cmd) {
	case MSM_ROTATOR_IOCTL_START:
		return msm_rotator_start(arg, fd_info);
	case MSM_ROTATOR_IOCTL_ROTATE:
		return msm_rotator_do_rotate(arg);
	case MSM_ROTATOR_IOCTL_FINISH:
		return msm_rotator_finish(arg);

	default:
		dev_dbg(msm_rotator_dev->device,
			"unexpected IOCTL %d\n", cmd);
		return -ENOTTY;
	}
}

static const struct file_operations msm_rotator_fops = {
	.owner = THIS_MODULE,
	.open = msm_rotator_open,
	.release = msm_rotator_close,
	.unlocked_ioctl = msm_rotator_ioctl,
};

static int __devinit msm_rotator_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	struct msm_rotator_platform_data *pdata = NULL;
	int i, number_of_clks;
	uint32_t ver;

	msm_rotator_dev = kzalloc(sizeof(struct msm_rotator_dev), GFP_KERNEL);
	if (!msm_rotator_dev) {
		printk(KERN_ERR "%s Unable to allocate memory for struct\n",
		       __func__);
		return -ENOMEM;
	}
	for (i = 0; i < MAX_SESSIONS; i++)
		msm_rotator_dev->img_info[i] = NULL;
	msm_rotator_dev->last_session_idx = INVALID_SESSION;

	pdata = pdev->dev.platform_data;
	number_of_clks = pdata->number_of_clocks;

	msm_rotator_dev->imem_owner = IMEM_NO_OWNER;
	mutex_init(&msm_rotator_dev->imem_lock);
	INIT_LIST_HEAD(&msm_rotator_dev->fd_list);
	msm_rotator_dev->imem_clk_state = CLK_DIS;
	INIT_DELAYED_WORK(&msm_rotator_dev->imem_clk_work,
			  msm_rotator_imem_clk_work_f);
	msm_rotator_dev->imem_clk = NULL;
	msm_rotator_dev->pdev = pdev;

	msm_rotator_dev->core_clk = NULL;
	msm_rotator_dev->pclk = NULL;

#ifdef CONFIG_MSM_BUS_SCALING
	if (!msm_rotator_dev->bus_client_handle && pdata &&
		pdata->bus_scale_table) {
		msm_rotator_dev->bus_client_handle =
			msm_bus_scale_register_client(
				pdata->bus_scale_table);
		if (!msm_rotator_dev->bus_client_handle) {
			pr_err("%s not able to get bus scale handle\n",
				__func__);
		}
	}
#endif

	for (i = 0; i < number_of_clks; i++) {
		if (pdata->rotator_clks[i].clk_type == ROTATOR_IMEM_CLK) {
			msm_rotator_dev->imem_clk =
			clk_get(&msm_rotator_dev->pdev->dev,
				pdata->rotator_clks[i].clk_name);
			if (IS_ERR(msm_rotator_dev->imem_clk)) {
				rc = PTR_ERR(msm_rotator_dev->imem_clk);
				msm_rotator_dev->imem_clk = NULL;
				printk(KERN_ERR "%s: cannot get imem_clk "
					"rc=%d\n", DRIVER_NAME, rc);
				goto error_imem_clk;
			}
			if (pdata->rotator_clks[i].clk_rate)
				clk_set_rate(msm_rotator_dev->imem_clk,
					pdata->rotator_clks[i].clk_rate);
		}
		if (pdata->rotator_clks[i].clk_type == ROTATOR_PCLK) {
			msm_rotator_dev->pclk =
			clk_get(&msm_rotator_dev->pdev->dev,
				pdata->rotator_clks[i].clk_name);
			if (IS_ERR(msm_rotator_dev->pclk)) {
				rc = PTR_ERR(msm_rotator_dev->pclk);
				msm_rotator_dev->pclk = NULL;
				printk(KERN_ERR "%s: cannot get pclk rc=%d\n",
					DRIVER_NAME, rc);
				goto error_pclk;
			}

			if (pdata->rotator_clks[i].clk_rate)
				clk_set_rate(msm_rotator_dev->pclk,
					pdata->rotator_clks[i].clk_rate);
		}

		if (pdata->rotator_clks[i].clk_type == ROTATOR_CORE_CLK) {
			msm_rotator_dev->core_clk =
			clk_get(&msm_rotator_dev->pdev->dev,
				pdata->rotator_clks[i].clk_name);
			if (IS_ERR(msm_rotator_dev->core_clk)) {
				rc = PTR_ERR(msm_rotator_dev->core_clk);
				msm_rotator_dev->core_clk = NULL;
				printk(KERN_ERR "%s: cannot get core clk "
					"rc=%d\n", DRIVER_NAME, rc);
			goto error_core_clk;
			}

			if (pdata->rotator_clks[i].clk_rate)
				clk_set_rate(msm_rotator_dev->core_clk,
					pdata->rotator_clks[i].clk_rate);
		}
	}

	msm_rotator_dev->regulator = regulator_get(NULL, pdata->regulator_name);
	if (IS_ERR(msm_rotator_dev->regulator))
		msm_rotator_dev->regulator = NULL;

	msm_rotator_dev->rot_clk_state = CLK_DIS;
	INIT_DELAYED_WORK(&msm_rotator_dev->rot_clk_work,
			  msm_rotator_rot_clk_work_f);

	mutex_init(&msm_rotator_dev->rotator_lock);
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	msm_rotator_dev->client = msm_ion_client_create(-1, pdev->name);
#endif
	platform_set_drvdata(pdev, msm_rotator_dev);


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk(KERN_ALERT
		       "%s: could not get IORESOURCE_MEM\n", DRIVER_NAME);
		rc = -ENODEV;
		goto error_get_resource;
	}
	msm_rotator_dev->io_base = ioremap(res->start,
					   resource_size(res));

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (msm_rotator_dev->imem_clk)
		clk_prepare_enable(msm_rotator_dev->imem_clk);
#endif
	enable_rot_clks();
	ver = ioread32(MSM_ROTATOR_HW_VERSION);
	disable_rot_clks();

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (msm_rotator_dev->imem_clk)
		clk_disable_unprepare(msm_rotator_dev->imem_clk);
#endif
	if (ver != pdata->hardware_version_number)
		pr_debug("%s: invalid HW version ver 0x%x\n",
			DRIVER_NAME, ver);

	rotator_hw_revision = ver;
	rotator_hw_revision >>= 16;     /* bit 31:16 */
	rotator_hw_revision &= 0xff;

	pr_info("%s: rotator_hw_revision=%x\n",
		__func__, rotator_hw_revision);

	msm_rotator_dev->irq = platform_get_irq(pdev, 0);
	if (msm_rotator_dev->irq < 0) {
		printk(KERN_ALERT "%s: could not get IORESOURCE_IRQ\n",
		       DRIVER_NAME);
		rc = -ENODEV;
		goto error_get_irq;
	}
	rc = request_irq(msm_rotator_dev->irq, msm_rotator_isr,
			 IRQF_TRIGGER_RISING, DRIVER_NAME, NULL);
	if (rc) {
		printk(KERN_ERR "%s: request_irq() failed\n", DRIVER_NAME);
		goto error_get_irq;
	}
	/* we enable the IRQ when we need it in the ioctl */
	disable_irq(msm_rotator_dev->irq);

	rc = alloc_chrdev_region(&msm_rotator_dev->dev_num, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		printk(KERN_ERR "%s: alloc_chrdev_region Failed rc = %d\n",
		       __func__, rc);
		goto error_get_irq;
	}

	msm_rotator_dev->class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(msm_rotator_dev->class)) {
		rc = PTR_ERR(msm_rotator_dev->class);
		printk(KERN_ERR "%s: couldn't create class rc = %d\n",
		       DRIVER_NAME, rc);
		goto error_class_create;
	}

	msm_rotator_dev->device = device_create(msm_rotator_dev->class, NULL,
						msm_rotator_dev->dev_num, NULL,
						DRIVER_NAME);
	if (IS_ERR(msm_rotator_dev->device)) {
		rc = PTR_ERR(msm_rotator_dev->device);
		printk(KERN_ERR "%s: device_create failed %d\n",
		       DRIVER_NAME, rc);
		goto error_class_device_create;
	}

	cdev_init(&msm_rotator_dev->cdev, &msm_rotator_fops);
	rc = cdev_add(&msm_rotator_dev->cdev,
		      MKDEV(MAJOR(msm_rotator_dev->dev_num), 0),
		      1);
	if (rc < 0) {
		printk(KERN_ERR "%s: cdev_add failed %d\n", __func__, rc);
		goto error_cdev_add;
	}

	init_waitqueue_head(&msm_rotator_dev->wq);

	dev_dbg(msm_rotator_dev->device, "probe successful\n");
	return rc;

error_cdev_add:
	device_destroy(msm_rotator_dev->class, msm_rotator_dev->dev_num);
error_class_device_create:
	class_destroy(msm_rotator_dev->class);
error_class_create:
	unregister_chrdev_region(msm_rotator_dev->dev_num, 1);
error_get_irq:
	iounmap(msm_rotator_dev->io_base);
error_get_resource:
	mutex_destroy(&msm_rotator_dev->rotator_lock);
	if (msm_rotator_dev->regulator)
		regulator_put(msm_rotator_dev->regulator);
	clk_put(msm_rotator_dev->core_clk);
error_core_clk:
	clk_put(msm_rotator_dev->pclk);
error_pclk:
	if (msm_rotator_dev->imem_clk)
		clk_put(msm_rotator_dev->imem_clk);
error_imem_clk:
	mutex_destroy(&msm_rotator_dev->imem_lock);
	kfree(msm_rotator_dev);
	return rc;
}

static int __devexit msm_rotator_remove(struct platform_device *plat_dev)
{
	int i;

#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_scale_unregister_client(msm_rotator_dev->bus_client_handle);
#endif
	free_irq(msm_rotator_dev->irq, NULL);
	mutex_destroy(&msm_rotator_dev->rotator_lock);
	cdev_del(&msm_rotator_dev->cdev);
	device_destroy(msm_rotator_dev->class, msm_rotator_dev->dev_num);
	class_destroy(msm_rotator_dev->class);
	unregister_chrdev_region(msm_rotator_dev->dev_num, 1);
	iounmap(msm_rotator_dev->io_base);
	if (msm_rotator_dev->imem_clk) {
		if (msm_rotator_dev->imem_clk_state == CLK_EN)
			clk_disable_unprepare(msm_rotator_dev->imem_clk);
		clk_put(msm_rotator_dev->imem_clk);
		msm_rotator_dev->imem_clk = NULL;
	}
	if (msm_rotator_dev->rot_clk_state == CLK_EN)
		disable_rot_clks();
	clk_put(msm_rotator_dev->core_clk);
	clk_put(msm_rotator_dev->pclk);
	if (msm_rotator_dev->regulator)
		regulator_put(msm_rotator_dev->regulator);
	msm_rotator_dev->core_clk = NULL;
	msm_rotator_dev->pclk = NULL;
	mutex_destroy(&msm_rotator_dev->imem_lock);
	for (i = 0; i < MAX_SESSIONS; i++)
		if (msm_rotator_dev->img_info[i] != NULL)
			kfree(msm_rotator_dev->img_info[i]);
	kfree(msm_rotator_dev);
	return 0;
}

#ifdef CONFIG_PM
static int msm_rotator_suspend(struct platform_device *dev, pm_message_t state)
{
	mutex_lock(&msm_rotator_dev->imem_lock);
	if (msm_rotator_dev->imem_clk_state == CLK_EN
		&& msm_rotator_dev->imem_clk) {
		clk_disable_unprepare(msm_rotator_dev->imem_clk);
		msm_rotator_dev->imem_clk_state = CLK_SUSPEND;
	}
	mutex_unlock(&msm_rotator_dev->imem_lock);
	mutex_lock(&msm_rotator_dev->rotator_lock);
	if (msm_rotator_dev->rot_clk_state == CLK_EN) {
		disable_rot_clks();
		msm_rotator_dev->rot_clk_state = CLK_SUSPEND;
	}
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return 0;
}

static int msm_rotator_resume(struct platform_device *dev)
{
	mutex_lock(&msm_rotator_dev->imem_lock);
	if (msm_rotator_dev->imem_clk_state == CLK_SUSPEND
		&& msm_rotator_dev->imem_clk) {
		clk_prepare_enable(msm_rotator_dev->imem_clk);
		msm_rotator_dev->imem_clk_state = CLK_EN;
	}
	mutex_unlock(&msm_rotator_dev->imem_lock);
	mutex_lock(&msm_rotator_dev->rotator_lock);
	if (msm_rotator_dev->rot_clk_state == CLK_SUSPEND) {
		enable_rot_clks();
		msm_rotator_dev->rot_clk_state = CLK_EN;
	}
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return 0;
}
#endif

static struct platform_driver msm_rotator_platform_driver = {
	.probe = msm_rotator_probe,
	.remove = __devexit_p(msm_rotator_remove),
#ifdef CONFIG_PM
	.suspend = msm_rotator_suspend,
	.resume = msm_rotator_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME
	}
};

static int __init msm_rotator_init(void)
{
	return platform_driver_register(&msm_rotator_platform_driver);
}

static void __exit msm_rotator_exit(void)
{
	return platform_driver_unregister(&msm_rotator_platform_driver);
}

module_init(msm_rotator_init);
module_exit(msm_rotator_exit);

MODULE_DESCRIPTION("MSM Offline Image Rotator driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
