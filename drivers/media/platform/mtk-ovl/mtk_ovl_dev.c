/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Qing Li <qing.li@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include "mtk_ovl_util.h"
#include "mtk_ovl.h"


#define MTK_OVL_MODULE_NAME             "mtk-ovl"
#define OVL_MAP_HW_REG_NUM
#define MTK_OVL_CLK_CNT                 1
#define MTK_OVL_REG_BASE_COUNT          1

#define PROBE_ST_DEV_ALLOC              (1 << 0)
#define PROBE_ST_MUTEX_INIT             (1 << 1)
#define PROBE_ST_DEV_LARB               (1 << 2)
#if SUPPORT_IOMMU_ATTACH
#define PROBE_ST_IOMMU_ATTACH           (1 << 3)
#endif
#define PROBE_ST_CLK_GET                (1 << 4)
#define PROBE_ST_CLK_ON                 (1 << 5)
#define PROBE_ST_WORKQUEUE_CREATE       (1 << 6)
#define PROBE_ST_IRQ_REQ                (1 << 7)
#define PROBE_ST_REG_MAP                (1 << 8)
#define PROBE_ST_V4L2_DEV_REG           (1 << 9)
#define PROBE_ST_VIDEO_DEV_REG          (1 << 10)
#define PROBE_ST_DMA_PARAM_SET          (1 << 11)
#define PROBE_ST_DMA_CONTIG_INIT        (1 << 12)
#define PROBE_ST_PM_ENABLE              (1 << 13)
#define PROBE_ST_ALL                    (0xffffffff)

/*
 *  struct mtk_ovl_pix_max - picture pixel size limits in various IP config
 *  @org_w:    max  source pixel width
 *  @org_h:    max  source pixel height
 *  @target_w: max  output pixel height
 *  @target_h: max  output pixel height
 */
struct mtk_ovl_pix_max {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

/*
 *  struct mtk_ovl_pix_min - picture pixel size limits in various IP config
 *
 *  @org_w:    minimum source pixel width
 *  @org_h:    minimum source pixel height
 *  @target_w: minimum output pixel height
 *  @target_h: minimum output pixel height
 */
struct mtk_ovl_pix_min {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

struct mtk_ovl_pix_align {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

/*
 * struct mtk_ovl_variant - video quality variant information
 */
struct mtk_ovl_variant {
	struct mtk_ovl_pix_max       *pix_max;
	struct mtk_ovl_pix_min       *pix_min;
	struct mtk_ovl_pix_align     *pix_align;
	u16                          in_buf_cnt;
	u16                          out_buf_cnt;
};

struct mtk_ovl_crop {
	u32    top;
	u32    left;
	u32    width;
	u32    height;
};

struct mtk_ovl_hw_clks {
	struct clk *clk_mm_disp_ovl2;
};

struct mtk_ovl_dev {
	struct mutex              lock;
	/* struct mutex              ovllock; */
	struct platform_device    *pdev;
	struct vb2_alloc_ctx      *alloc_ctx;
	struct video_device       vdev;
	struct v4l2_device        v4l2_dev;
	struct device             *larb[2];
	struct workqueue_struct   *workqueue;
	struct platform_device    *platform_dev;
	struct clk                *clks[MTK_OVL_CLK_CNT];

	wait_queue_head_t         irq_queue;
	unsigned long             state;
	int                       id;
	void                      *reg_base[MTK_OVL_REG_BASE_COUNT];
	struct device             *larb_dev;
};

/**
 * struct mtk_ovl_fmt - the driver's internal color format data
 * @mbus_code:        Media Bus pixel code, -1 if not applicable
 * @name:             format description
 * @pixelformat:      the fourcc code for this format, 0 if not applicable
 * @yorder:           Y/C order
 * @corder:           Chrominance order control
 * @num_planes:       number of physically non-contiguous data planes
 * @nr_comp:          number of physically contiguous data planes
 * @depth:            per plane driver's private 'number of bits per pixel'
 * @flags:            flags indicating which operation mode format applies to
 */
struct mtk_ovl_fmt {
	/* enum v4l2_mbus_pixelcode        mbus_code; */
	char                            *name;
	u32                             pixelformat;
	u32                             color;
	u32                             yorder;
	u32                             corder;
	u16                             num_planes;
	u16                             num_comp;
	u8                              depth[VIDEO_MAX_PLANES];
	u32                             flags;
};

/**
 * struct mtk_ovl_frame - source frame properties
 * @f_width:        SRC : SRCIMG_WIDTH, DST : OUTPUTDMA_WHOLE_IMG_WIDTH
 * @f_height:       SRC : SRCIMG_HEIGHT, DST : OUTPUTDMA_WHOLE_IMG_HEIGHT
 * @crop:           cropped(source)/scaled(destination) size
 * @payload:        image size in bytes (w x h x bpp)
 * @pitch:          bytes per line of image in memory
 * @fmt:            color format pointer
 * @colorspace:	    value indicating v4l2_colorspace
 * @alpha:          frame's alpha value
 */
struct mtk_ovl_frame {
	struct v4l2_rect            crop;
	u32                         f_width;
	u32                         f_height;
	unsigned long               payload[VIDEO_MAX_PLANES];
	unsigned int                pitch[VIDEO_MAX_PLANES];
	const struct mtk_ovl_fmt    *fmt;
	u32                         colorspace;
	u8                          alpha;
};

struct mtk_ovl_buffer {
	struct vb2_v4l2_buffer    vb;
	struct list_head          list;
};

/*
 * mtk_ovl_ctx - the device context data
 * @s_frame:         source frame properties
 * @ovl_dev:         the ovl device which this context applies to
 * @fh:              v4l2 file handle
 * @ctrl_handler:    v4l2 controls handler
 * @ctrls            image processor control set
 * @qlock:           vb2 queue lock
 * @slock:           the mutex protecting this data structure
 * @work:            worker for image processing
 * @s_buf:           source buffer information record
 * @s_idx_for_next:  source buffer idx for next
 * @flags:           additional flags for image conversion
 * @state:           flags to keep track of user configuration
 * @ctrls_rdy:       true if the control handler is initialized
 */
struct mtk_ovl_ctx {
	struct vb2_queue        vb2_q;
	struct mtk_ovl_frame    s_frame;
	struct mtk_ovl_dev      *ovl_dev;
	struct v4l2_fh          fh;
	struct mutex            qlock;
	struct mutex            slock;
	#if LIST_OLD
	struct mtk_ovl_buffer   *curr_buf;
	#else
	struct vb2_v4l2_buffer  *curr_vb2_v4l2_buffer;
	#endif
	u32                     flags;

	spinlock_t              buf_queue_lock; /* protect bufqueue */
	struct list_head        buf_queue_list;
	/* struct mtk_ovl_crop   crop; */
	struct mtk_ovl_variant  *variant;
};

static struct mtk_ovl_ctx *_mtk_ovl_ctx[OVL_LAYER_NUM] = {0};

static struct MTK_OVL_HW_PARAM_ALL _mtk_ovl_hw_param;

static char *_ap_mtk_ovl_clk_name[MTK_OVL_CLK_CNT] = {
	"mm_disp_ovl2",
};

static struct mtk_ovl_pix_max mtk_ovl_size_max = {
	.org_w			= 1920,
	.org_h			= 1080,
	.target_w		= 1920,
	.target_h		= 1080,
};

static struct mtk_ovl_pix_min mtk_ovl_size_min = {
	.org_w			= 16,
	.org_h			= 16,
	.target_w		= 16,
	.target_h		= 16,
};

static struct mtk_ovl_pix_align mtk_ovl_size_align = {
	.org_w			= 16,
	.org_h			= 16,
	.target_w		= 16,
	.target_h		= 16,
};

static struct mtk_ovl_variant mtk_ovl_default_variant = {
	.pix_max		= &mtk_ovl_size_max,
	.pix_min		= &mtk_ovl_size_min,
	.pix_align		= &mtk_ovl_size_align,
	.in_buf_cnt		= 100,
	.out_buf_cnt		= 100,
};

static const struct mtk_ovl_fmt mtk_ovl_formats[] = {
	{
		.name		= "RGB 5:6:5. 1p, RGB",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.depth		= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "RGB 8:8:8. 1p, RGB",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.depth		= { 24 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "BGR 8:8:8. 1p, BGR",
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.depth		= { 24 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "ARGB 8:8:8:8. 1p, ARGB",
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.depth		= { 43 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "ABGR 8:8:8:8. 1p, ABGR",
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.depth		= { 43 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "YUV 4:2:2. 1p, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.depth		= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "YUV 4:2:2. 1p, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.depth		= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "YUV 4:2:2. 1p, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.depth		= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "YUV 4:2:2. 1p, CrYCbY",
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.depth		= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "YUV420 non-contig. 2p, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.depth		= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
	}, {
		.name		= "YUV420 contig. 1p, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.depth		= { 8, 4 },
		.num_planes	= 1,
		.num_comp	= 2,
	}, {
		.name		= "YUV420 non-contig. 3p, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.depth		= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
	}, {
		.name		= "YUV 4:2:2 non-contig. 3p, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.depth		= { 8, 4, 4 },
		.num_planes	= 3,
		.num_comp	= 3,
	}, {
		.name		= "YUV 4:2:2. 1p, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.depth		= { 8, 8 },
		.num_planes	= 1,
		.num_comp	= 2,
	}, {
		.name		= "YUV 4:2:2 non-contig. 2p, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.depth		= { 8, 8 },
		.num_planes	= 2,
		.num_comp	= 2,
	}
};

static inline struct mtk_ovl_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_ovl_ctx, fh);
}

static enum OVL_INPUT_FORMAT mtk_ovl_input_fmt_convert(unsigned int color)
{/*lq-check*/
	enum OVL_INPUT_FORMAT ovl_fmt = OVL_INPUT_FORMAT_UNKNOWN;

	switch (color) {
	case V4L2_PIX_FMT_RGB565:
		ovl_fmt = OVL_INPUT_FORMAT_RGB565;
		break;
	case V4L2_PIX_FMT_RGB24:
		ovl_fmt = OVL_INPUT_FORMAT_RGB888;
		break;
	case V4L2_PIX_FMT_BGR24:
		ovl_fmt = OVL_INPUT_FORMAT_BGR888;
		break;
	case V4L2_PIX_FMT_ABGR32:
		ovl_fmt = OVL_INPUT_FORMAT_BGRA8888;
		break;
	case V4L2_PIX_FMT_ARGB32:
		ovl_fmt = OVL_INPUT_FORMAT_ARGB8888;
		break;
	case V4L2_PIX_FMT_VYUY:
		ovl_fmt = OVL_INPUT_FORMAT_VYUY;
		break;
	case V4L2_PIX_FMT_YVYU:
		ovl_fmt = OVL_INPUT_FORMAT_YVYU;
		break;
	case V4L2_PIX_FMT_UYVY:
		ovl_fmt = OVL_INPUT_FORMAT_UYVY;
		break;
	case V4L2_PIX_FMT_YUYV:
		ovl_fmt = OVL_INPUT_FORMAT_YUYV;
		break;
	default:
		log_err("unsupport color[0x%x] map to fmt", color);
		ovl_fmt = OVL_INPUT_FORMAT_BGR565;
		break;
	}

	return ovl_fmt;
}

static const struct mtk_ovl_fmt *mtk_ovl_get_format(int index)
{
	if (index >= ARRAY_SIZE(mtk_ovl_formats))
		return NULL;

	return (struct mtk_ovl_fmt *)&mtk_ovl_formats[index];
}

static const struct mtk_ovl_fmt *mtk_ovl_find_fmt(
	u32 *pixelformat, u32 *mbus_code, u32 index)
{
	const struct mtk_ovl_fmt *fmt, *def_fmt = NULL;
	unsigned int i;

	if (index >= ARRAY_SIZE(mtk_ovl_formats))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_ovl_formats); i++) {
		fmt = mtk_ovl_get_format(i);
		if ((pixelformat != NULL) && (fmt->pixelformat == *pixelformat))
			return fmt;
	}

	return def_fmt;
}

#if 0
static void mtk_ovl_bound_align_image(
	u32 *w, unsigned int wmin, unsigned int wmax, unsigned int walign,
	u32 *h, unsigned int hmin, unsigned int hmax, unsigned int halign)
{
	int width, height, w_step, h_step;

	width = *w;
	height = *h;
	w_step = 1 << walign;
	h_step = 1 << halign;

	v4l_bound_align_image(w, wmin, wmax, walign, h, hmin, hmax, halign, 0);
	if (*w < width && (*w + w_step) <= wmax)
		*w += w_step;
	if (*h < height && (*h + h_step) <= hmax)
		*h += h_step;
}
#endif

static void mtk_ovl_set_frame_size(
	struct mtk_ovl_frame *frame, int width, int height)
{
	frame->f_width = width;
	frame->f_height = height;
	frame->crop.width = width;
	frame->crop.height = height;
	frame->crop.left = 0;
	frame->crop.top = 0;
}

static int mtk_ovl_param_store(struct mtk_ovl_ctx *ctx, struct vb2_buffer *vb)
{
	#if LIST_OLD
	unsigned long flags;
	#endif
	struct MTK_OVL_HW_PARAM_ALL *hw = &_mtk_ovl_hw_param;
	struct MTK_OVL_HW_PARAM_LAYER *layer = NULL;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	#if LIST_OLD
	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	if (list_empty(&ctx->buf_queue_list)) {
		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
		log_err("[ovl%d] [err] %s[%d]",
			ctx->ovl_dev->id, __func__, __LINE__);
		return NULL;
	}
	ctx->curr_buf = list_first_entry(
			&ctx->buf_queue_list,
			struct mtk_ovl_buffer,
			list);
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	if (ctx->curr_buf == NULL) {
		log_err("[ovl%d] [err] %s[%d] get next buf NULL",
			ctx->ovl_dev->id, __func__, __LINE__);
		return RET_ERR_EXCEPTION;
	}
	/* INIT_LIST_HEAD(&ctx->curr_buf->list); */
	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	list_add_tail(&ctx->curr_buf->list, &ctx->buf_queue_list);
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	#else

	ctx->curr_vb2_v4l2_buffer = to_vb2_v4l2_buffer(vb);

	#endif

	/*lq-check*/
	hw->dst_w = ctx->s_frame.f_width;
	hw->dst_h = ctx->s_frame.f_height;
	layer = &(hw->ovl_config[ctx->ovl_dev->id]);
	layer->layer_en = 1;
	layer->source = OVL_LAYER_SOURCE_MEM;
	layer->fmt = mtk_ovl_input_fmt_convert(ctx->s_frame.fmt->pixelformat);
	layer->addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	layer->src_x = 0;
	layer->src_y = 0;
	layer->src_pitch = ctx->s_frame.pitch[0];
	layer->dst_x = 0;
	layer->dst_y = 0;
	layer->dst_w = ctx->s_frame.f_width;
	layer->dst_h = ctx->s_frame.f_height;
	layer->keyEn = 0;
	layer->key = 0xFF020100;
	layer->aen = 0;
	layer->alpha = 0;
	layer->sur_aen = 0;
	layer->src_alpha = 0;
	layer->dst_alpha = 0;
	layer->yuv_range = 0;

	log_dbg("[ovl%d] store hw param :",
		ctx->ovl_dev->id);
	log_dbg("[ovl%d] dst wh[%d, %d]",
		ctx->ovl_dev->id,
		hw->dst_w,
		hw->dst_h);
	log_dbg("[ovl%d] layer[%d] en[%d] src[%d] fmt[%d] addr[0x%lx]",
		ctx->ovl_dev->id,
		ctx->ovl_dev->id,
		layer->layer_en,
		layer->source,
		layer->fmt,
		layer->addr);
	log_dbg("[ovl%d] src xy[%d, %d] pitch[%d]",
		ctx->ovl_dev->id,
		layer->src_x,
		layer->src_y,
		layer->src_pitch);
	log_dbg("[ovl%d] dst xy[%d, %d] wh[%d, %d]",
		ctx->ovl_dev->id,
		layer->dst_x,
		layer->dst_y,
		layer->dst_w,
		layer->dst_h);
	log_dbg("[ovl%d] key[%d, %d]",
		ctx->ovl_dev->id,
		layer->keyEn,
		layer->key);
	log_dbg("[ovl%d] alpha[%d, %d, %d, %d, %d]",
		ctx->ovl_dev->id,
		layer->aen,
		layer->alpha,
		layer->sur_aen,
		layer->src_alpha,
		layer->dst_alpha);
	log_dbg("[ovl%d] yuv_range[%d]",
		ctx->ovl_dev->id,
		layer->yuv_range);

	log_dbg("[ovl%d] %s[%d] end", ctx->ovl_dev->id, __func__, __LINE__);

	return RET_OK;
}

#if LIST_OLD
static int mtk_ovl_buffer_init(struct vb2_buffer *vb)
{
	struct mtk_ovl_buffer *buf =
		container_of(vb, struct mtk_ovl_buffer, vb);

	INIT_LIST_HEAD(&buf->list);
	return 0;
}
#endif

/*
 * mtk_ovl_queue_setup()
 * This function allocates memory for the buffers
 */
static int mtk_ovl_queue_setup(
	struct vb2_queue *vq,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	struct mtk_ovl_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_ovl_frame *frame = &ctx->s_frame;
	int i;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (!frame->fmt)
		return -EINVAL;

	*num_planes = frame->fmt->num_planes;
	for (i = 0; i < frame->fmt->num_planes; i++) {
		sizes[i] = frame->payload[i];
		#if SUPPORT_ALLOC_CTX
		alloc_devs[i] = ctx->ovl_dev->alloc_ctx;
		#endif
	}

	return 0;
}

/*
 * mtk_ovl_buf_prepare()
 * This is the callback function called from vb2_qbuf() function
 * the buffer is prepared and user space virtual address is converted into
 * physical address
 */
static int mtk_ovl_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_ovl_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_ovl_frame *frame = &ctx->s_frame;
	int i;
	unsigned long addr;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	for (i = 0; i < frame->fmt->num_planes; i++)
		vb2_set_plane_payload(vb, i, frame->payload[i]);

	addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	if (!IS_ALIGNED(addr, 8)) {
		log_err("[ovl%d] addr[0x%lx] is not 8 align",
			ctx->ovl_dev->id, addr);
		return RET_ERR_EXCEPTION;
	}

	mtk_ovl_param_store(ctx, vb);

	return 0;
}

static void mtk_ovl_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_ovl_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);
}

static void mtk_ovl_ctx_lock(struct vb2_queue *vq)
{
	struct mtk_ovl_ctx *ctx = vb2_get_drv_priv(vq);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	mutex_lock(&ctx->qlock);
}

static void mtk_ovl_ctx_unlock(struct vb2_queue *vq)
{
	struct mtk_ovl_ctx *ctx = vb2_get_drv_priv(vq);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	mutex_unlock(&ctx->qlock);
}

static int mtk_ovl_start_streaming(struct vb2_queue *q, unsigned int count)
{
	int ret = 0;
	struct mtk_ovl_ctx *ctx = q->drv_priv;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (ctx->ovl_dev->id == MTK_OVL_MAIN_ID)
		ret = pm_runtime_get_sync(&ctx->ovl_dev->pdev->dev);

	return ret > 0 ? 0 : ret;
}

static void mtk_ovl_stop_streaming(struct vb2_queue *q)
{
	#if LIST_OLD
	unsigned long flags;
	struct vb2_buffer *vb;
	#endif
	struct mtk_ovl_ctx *ctx = q->drv_priv;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (!vb2_is_streaming(q))
		return;

	/* release all active buffers */
	#if LIST_OLD
	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	while (!list_empty(&ctx->buf_queue_list)) {
		ctx->curr_buf = list_first_entry(
				&ctx->buf_queue_list,
				struct mtk_ovl_buffer,
				list);
		list_del(&ctx->curr_buf->list);
		vb2_buffer_done(
			&ctx->curr_buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
	#endif

	if (ctx->ovl_dev->id == MTK_OVL_MAIN_ID)
		pm_runtime_put_sync(&ctx->ovl_dev->pdev->dev);
}

static struct vb2_ops mtk_ovl_vb2_qops = {
	.queue_setup     = mtk_ovl_queue_setup,
	#if LIST_OLD
	.buf_init        = mtk_ovl_buffer_init,
	#endif
	.buf_prepare     = mtk_ovl_buf_prepare,
	.buf_queue       = mtk_ovl_buf_queue,
	.wait_prepare    = mtk_ovl_ctx_unlock,
	.wait_finish     = mtk_ovl_ctx_lock,
	.stop_streaming  = mtk_ovl_stop_streaming,
	.start_streaming = mtk_ovl_start_streaming,
};

static int mtk_ovl_querycap(
	struct file *file, void *fh, struct v4l2_capability *cap)
{
	char str_dev_id[3];
	char str_dev_name[20];
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (cap == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	memset(str_dev_name, 0, sizeof(str_dev_name));
	memcpy(str_dev_name, MTK_OVL_MODULE_NAME, sizeof(MTK_OVL_MODULE_NAME));
	strcat(str_dev_name, "-");
	sprintf(str_dev_id, "%d", ctx->ovl_dev->id);
	strcat(str_dev_name, str_dev_id);

	strlcpy(cap->driver, str_dev_name, sizeof(cap->driver));
	strlcpy(cap->card, ctx->ovl_dev->pdev->name, sizeof(cap->card));
	strlcpy(cap->bus_info, "platform", sizeof(cap->bus_info));
	cap->device_caps =  V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	log_dbg("[ovl%d] %s end, driver[%s], card[%s], bus_info[%s], caps[0x%x, 0x%x]",
		ctx->ovl_dev->id, __func__,
		cap->driver, cap->card, cap->bus_info,
		cap->device_caps, cap->capabilities);

	return RET_OK;
}

static int mtk_ovl_enum_fmt_mplane(
	struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const struct mtk_ovl_fmt *fmt;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (f == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[ovl%d] %s start, index[%d]",
		ctx->ovl_dev->id, __func__, f->index);

	fmt = mtk_ovl_find_fmt(NULL, NULL, f->index);
	if (!fmt)
		return -EINVAL;

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->pixelformat;

	log_dbg("[ovl%d] %s end, description = %s, pixelformat = %d",
		ctx->ovl_dev->id, __func__, f->description, f->pixelformat);

	return RET_OK;
}

static int mtk_ovl_g_fmt_mplane(
	struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);
	struct mtk_ovl_frame *frame = &(ctx->s_frame);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	int i;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (f == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	pix_mp->width       = frame->f_width;
	pix_mp->height      = frame->f_height;
	pix_mp->pixelformat = frame->fmt->pixelformat;
	pix_mp->colorspace  = V4L2_COLORSPACE_REC709;
	pix_mp->num_planes  = frame->fmt->num_planes;

	for (i = 0; i < pix_mp->num_planes; ++i) {
		pix_mp->plane_fmt[i].bytesperline =
			(frame->f_width * frame->fmt->depth[i]) / 8;
		pix_mp->plane_fmt[i].sizeimage =
			pix_mp->plane_fmt[i].bytesperline * frame->f_height;
	}

	log_dbg("[ovl%d] %s end, WH[%d, %d], fld[%d], pixfmt[%d], col[%d], np[%d], p[%d, %d][%d, %d]",
		ctx->ovl_dev->id, __func__,
		pix_mp->width, pix_mp->height,
		pix_mp->field, pix_mp->pixelformat,
		pix_mp->colorspace, pix_mp->num_planes,
		pix_mp->plane_fmt[0].bytesperline,
		pix_mp->plane_fmt[0].sizeimage,
		pix_mp->plane_fmt[1].bytesperline,
		pix_mp->plane_fmt[1].sizeimage);

	return RET_OK;
}

static int mtk_ovl_try_fmt_mplane(
	struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);
	struct v4l2_pix_format_mplane *pix_mp;
	const struct mtk_ovl_fmt *fmt;
	#if 0
	struct mtk_ovl_variant *variant = ctx->variant;
	u32 max_w, max_h, mod_x, mod_y;
	u32 min_w, min_h, tmp_w, tmp_h;
	#endif
	int i;

	if (f == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	pix_mp = &f->fmt.pix_mp;

	log_dbg("[ovl%d] %s start, pixfmt[0x%x], field[%d], WH[%d, %d]",
		ctx->ovl_dev->id, __func__,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.field,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height);

	fmt = mtk_ovl_find_fmt(&pix_mp->pixelformat, NULL, 0);
	if (!fmt) {
		log_err("[ovl%d] %s, pixelformat 0x%x invalid",
			ctx->ovl_dev->id, __func__, pix_mp->pixelformat);
		return RET_ERR_EXCEPTION;
	}

	#if 0
	max_w = variant->pix_max->target_w;
	max_h = variant->pix_max->target_h;

	mod_x = ffs(variant->pix_align->org_w) - 1;
	if (FMT_IS_420(fmt->color)) /*lq-check*/
		mod_y = ffs(variant->pix_align->org_h) - 1;
	else
		mod_y = ffs(variant->pix_align->org_h) - 2;

	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		min_w = variant->pix_min->org_w;
		min_h = variant->pix_min->org_h;
	} else {
		min_w = variant->pix_min->target_w;
		min_h = variant->pix_min->target_h;
	}

	tmp_w = pix_mp->width;
	tmp_h = pix_mp->height;

	mtk_ovl_bound_align_image(
	    &pix_mp->width, min_w, max_w, mod_x,
		&pix_mp->height, min_h, max_h, mod_y);
	#endif

	pix_mp->num_planes = fmt->num_planes;

	if (pix_mp->width >= 1280) /* HD */
		pix_mp->colorspace = V4L2_COLORSPACE_REC709;
	else /* SD */
		pix_mp->colorspace = V4L2_COLORSPACE_SMPTE170M;

	for (i = 0; i < pix_mp->num_planes; ++i) {
		int bpl = (pix_mp->width * fmt->depth[i]) >> 3;
		int sizeimage = bpl * pix_mp->height;

		pix_mp->plane_fmt[i].bytesperline = bpl;

		if (pix_mp->plane_fmt[i].sizeimage < sizeimage)
			pix_mp->plane_fmt[i].sizeimage = sizeimage;
	}

	log_dbg("[ovl%d] %s end, WH[%d, %d], np[%d], col[%d], p[%d, %d][%d, %d]",
		ctx->ovl_dev->id, __func__,
		pix_mp->width, pix_mp->height,
		pix_mp->num_planes, pix_mp->colorspace,
		pix_mp->plane_fmt[0].bytesperline,
		pix_mp->plane_fmt[0].sizeimage,
		pix_mp->plane_fmt[1].bytesperline,
		pix_mp->plane_fmt[1].sizeimage);

	return RET_OK;
}

static int mtk_ovl_s_fmt_mplane(
	struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);
	struct mtk_ovl_frame *frame = &ctx->s_frame;
	struct v4l2_pix_format_mplane *pix;
	int i, ret = RET_OK;

	if (f == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	pix = &f->fmt.pix_mp;

	log_dbg("[ovl%d] %s[%d] start, wh[%d, %d] fmt[0x%x, 0x%x] flag[0x%x] plane[%d, %d, %d]",
		ctx->ovl_dev->id,
		__func__, __LINE__,
		pix->width,
		pix->height,
		pix->pixelformat,
		pix->colorspace,
		pix->flags,
		pix->num_planes,
		pix->plane_fmt[0].bytesperline,
		pix->plane_fmt[0].sizeimage);

	ret = mtk_ovl_try_fmt_mplane(file, fh, f);
	if (ret)
		return ret;

	frame->fmt = mtk_ovl_find_fmt(&pix->pixelformat, NULL, 0);
	frame->colorspace = pix->colorspace;
	if (!frame->fmt)
		return -EINVAL;

	for (i = 0; i < frame->fmt->num_planes; i++) {
		frame->payload[i] = pix->plane_fmt[i].sizeimage;
		frame->pitch[i] = pix->plane_fmt[i].bytesperline;
	}

	mtk_ovl_set_frame_size(frame, pix->width, pix->height);

	log_dbg("[ovl%d] %s end, WH[%d, %d], col[%d], p[%d, %ld][%d, %ld]",
		ctx->ovl_dev->id, __func__,
		pix->width, pix->height,
		frame->colorspace,
		frame->pitch[0], frame->payload[0],
		frame->pitch[1], frame->payload[1]);

	return RET_OK;
}

static int mtk_ovl_reqbufs(
	struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);
	struct video_device *vfd = video_devdata(file);

	if (reqbufs == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[ovl%d] %s start, cnt[%d] mem[%d] type[%d, %d] point[0x%lx]",
		ctx->ovl_dev->id, __func__,
		reqbufs->count,
		reqbufs->memory,
		reqbufs->type,
		ctx->vb2_q.type,
		(unsigned long)(&(ctx->vb2_q)));

	vfd->queue = &ctx->vb2_q;
	log_dbg("[ovl%d] save vb2_q [0x%lx, 0x%lx, 0x%lx]",
		ctx->ovl_dev->id, (unsigned long)(&(ctx->vb2_q)),
		(unsigned long)vfd, (unsigned long)(vfd->queue));

	ret = vb2_ioctl_reqbufs(file, fh, reqbufs);

	log_dbg("[ovl%d] %s end, ret[%d]",
		ctx->ovl_dev->id, __func__, ret);

	return ret;
}

static int mtk_ovl_expbuf(
	struct file *file, void *fh, struct v4l2_exportbuffer *eb)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (eb == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	ret = vb2_ioctl_expbuf(file, fh, eb);

	log_dbg("[ovl%d] %s end, ret[%d], fd[%d], flag[%d], idx[%d], plane[%d]",
		ctx->ovl_dev->id, __func__,
		ret, eb->fd, eb->flags, eb->index, eb->plane);

	return ret;
}

static int mtk_ovl_querybuf(
	struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start",
		ctx->ovl_dev->id, __func__, __LINE__);

	if (buf == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[ovl%d] %s start, idx[%d], memtype[%d], np[%d]",
		ctx->ovl_dev->id, __func__,
		buf->index, buf->memory, buf->length);

	ret = vb2_ioctl_querybuf(file, fh, buf);

	log_dbg("[ovl%d] %s end, ret[%d], length[%d, %d], bytes[%d, %d]",
		ctx->ovl_dev->id, __func__, ret,
		buf->m.planes[0].length, buf->m.planes[1].length,
		buf->m.planes[0].bytesused, buf->m.planes[1].bytesused);

	return ret;
}

static int mtk_ovl_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	if (buf == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[ovl%d] %s[%d] start, idx[%d], size[%d, %d], type[%d, %d], plane[%d, %d, 0x%lx], time[%ld, %ld]",
		ctx->ovl_dev->id, __func__, __LINE__,
		buf->index,
		buf->bytesused,
		buf->length,
		buf->memory,
		buf->type,
		buf->m.planes[0].length,
		buf->m.planes[0].bytesused,
		(unsigned long)(buf->m.planes[0].m.userptr),
		(unsigned long)(buf->timestamp.tv_sec),
		(unsigned long)(buf->timestamp.tv_usec));

	ret = vb2_ioctl_qbuf(file, fh, buf);

	log_dbg("[ovl%d] %s end, ret[%d]", ctx->ovl_dev->id, __func__, ret);

	return ret;
}

static int mtk_ovl_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (buf == NULL) {
		log_err("[ovl%d] %s, param is NULL",
			ctx->ovl_dev->id, __func__);
		return RET_ERR_PARAM;
	}

	ret = vb2_ioctl_dqbuf(file, fh, buf);

	log_dbg("[ovl%d] %s end, ret[%d]", ctx->ovl_dev->id, __func__, ret);

	return ret;
}

static int mtk_ovl_streamon(
	struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	ret = vb2_ioctl_streamon(file, fh, type);

	_mtk_ovl_ctx[ctx->ovl_dev->id] = ctx;
	log_dbg("[ovl%d] save ovl ctx[0x%lx] to id[%d]",
		ctx->ovl_dev->id, (unsigned long)ctx, ctx->ovl_dev->id);

	log_dbg("[ovl%d] %s end, ret[%d]", ctx->ovl_dev->id, __func__, ret);

	return ret;
}

static int mtk_ovl_streamoff(
	struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	ret = vb2_ioctl_streamoff(file, fh, type);

	log_dbg("[ovl%d] remove ovl ctx[0x%lx]",
		ctx->ovl_dev->id,
		(unsigned long)(_mtk_ovl_ctx[ctx->ovl_dev->id]));
	_mtk_ovl_ctx[ctx->ovl_dev->id] = NULL;

	log_dbg("[ovl%d] %s end, ret[%d]", ctx->ovl_dev->id, __func__, ret);

	return ret;
}

static int mtk_ovl_g_selection(
	struct file *file, void *fh, struct v4l2_selection *s)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);
	struct mtk_ovl_frame *frame = &(ctx->s_frame);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = frame->f_width;
		s->r.height = frame->f_height;
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_CROP:
		s->r.left = frame->crop.left;
		s->r.top = frame->crop.top;
		s->r.width = frame->crop.width;
		s->r.height = frame->crop.height;
		break;
	default:
		log_err("[ovl%d] %s, unsupport param %d",
			ctx->ovl_dev->id, __func__, s->target);
		ret = RET_ERR_PARAM;
		break;
	}

	log_dbg("[ovl%d] %s end, ret[%d], target[%d], [%d, %d, %d, %d]",
		ctx->ovl_dev->id, __func__, ret,
		s->target, s->r.left, s->r.top, s->r.width, s->r.height);

	return ret;
}

static int mtk_ovl_s_selection(
	struct file *file, void *fh, struct v4l2_selection *s)
{
	int ret = RET_OK;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(fh);
	struct mtk_ovl_frame *frame = &(ctx->s_frame);

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE:
		frame->f_width = s->r.width;
		frame->f_height = s->r.height;
		break;
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		frame->crop.left = s->r.left;
		frame->crop.top = s->r.top;
		frame->crop.width = s->r.width;
		frame->crop.height = s->r.height;
		break;
	default:
		log_err("[ovl%d] %s, unsupport param %d",
			ctx->ovl_dev->id, __func__, s->target);
		ret = RET_ERR_PARAM;
		break;
	}

	log_dbg("[ovl%d] %s end, target[%d], [%d, %d], [%d, %d, %d, %d]",
		ctx->ovl_dev->id, __func__,
		s->target,
		frame->f_width, frame->f_height,
		frame->crop.left, frame->crop.top,
		frame->crop.width, frame->crop.height);

	return ret;
}

static const struct v4l2_ioctl_ops mtk_ovl_ioctl_ops = {
	.vidioc_querycap                = mtk_ovl_querycap,
	.vidioc_enum_fmt_vid_out_mplane = mtk_ovl_enum_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane    = mtk_ovl_g_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane  = mtk_ovl_try_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane    = mtk_ovl_s_fmt_mplane,
	.vidioc_reqbufs                 = mtk_ovl_reqbufs,
	.vidioc_expbuf                  = mtk_ovl_expbuf,
	.vidioc_querybuf                = mtk_ovl_querybuf,
	.vidioc_qbuf                    = mtk_ovl_qbuf,
	.vidioc_dqbuf                   = mtk_ovl_dqbuf,
	.vidioc_streamon                = mtk_ovl_streamon,
	.vidioc_streamoff               = mtk_ovl_streamoff,
	.vidioc_g_selection             = mtk_ovl_g_selection,
	.vidioc_s_selection             = mtk_ovl_s_selection
};

static int mtk_ovl_open(struct file *file)
{
	int ret;
	struct mtk_ovl_ctx *ctx = NULL;
	struct video_device *vfd = video_devdata(file);
	struct mtk_ovl_dev *ovl_dev = video_drvdata(file);

	if (mutex_lock_interruptible(&ovl_dev->lock))
		return -ERESTARTSYS;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		ret = -ENOMEM;

	ctx->ovl_dev = ovl_dev;
	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	mutex_init(&ctx->qlock);
	mutex_init(&ctx->slock);
	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;

	ctx->s_frame.fmt = mtk_ovl_get_format(0);
	ctx->flags = 0;

	memset(&(ctx->vb2_q), 0, sizeof(ctx->vb2_q));
	ctx->vb2_q.dev = &(ovl_dev->pdev->dev);
	ctx->vb2_q.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; /*lq-check*/
	ctx->vb2_q.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vb2_q.drv_priv = ctx;
	ctx->vb2_q.ops = &mtk_ovl_vb2_qops;
	ctx->vb2_q.mem_ops = &vb2_dma_contig_memops;
	ctx->vb2_q.buf_struct_size = sizeof(struct mtk_ovl_buffer);
	ctx->vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&(ctx->vb2_q));

	INIT_LIST_HEAD(&ctx->buf_queue_list);

	ctx->variant = &mtk_ovl_default_variant;

	mutex_unlock(&ovl_dev->lock);

	return ret;
}

static int mtk_ovl_release(struct file *file)
{
	unsigned int ovl_id;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_ovl_dev *ovl_dev = ctx->ovl_dev;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	ovl_id = ctx->ovl_dev->id;

	flush_workqueue(ovl_dev->workqueue);
	mutex_lock(&ovl_dev->lock);
	mutex_destroy(&ctx->qlock);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	mutex_unlock(&ovl_dev->lock);

	log_dbg("[ovl%d] %s[%d] end", ovl_id, __func__, __LINE__);

	return 0;
}

static unsigned int mtk_ovl_poll(
	struct file *file, struct poll_table_struct *wait)
{
	int ret;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_ovl_dev *ovl_dev = ctx->ovl_dev;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (mutex_lock_interruptible(&ovl_dev->lock))
		return -ERESTARTSYS;

	ret = vb2_fop_poll(file, wait);

	mutex_unlock(&ovl_dev->lock);

	return ret;
}

static int mtk_ovl_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct mtk_ovl_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_ovl_dev *ovl_dev = ctx->ovl_dev;

	log_dbg("[ovl%d] %s[%d] start", ctx->ovl_dev->id, __func__, __LINE__);

	if (mutex_lock_interruptible(&ovl_dev->lock))
		return -ERESTARTSYS;

	ret = vb2_fop_mmap(file, vma);
	if (ret)
		log_err("[ovl%d] fail to mmap, ret[%d]", ctx->ovl_dev->id, ret);

	mutex_unlock(&ovl_dev->lock);

	return ret;
}

static const struct v4l2_file_operations mtk_ovl_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_ovl_open,
	.release	= mtk_ovl_release,
	.poll		= mtk_ovl_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= mtk_ovl_mmap,
};

static int mtk_ovl_clock_on(struct mtk_ovl_dev *ovl_dev)
{
	int i;
	int ret;

	log_dbg("[ovl%d] %s[%d] start", ovl_dev->id, __func__, __LINE__);

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		if (ovl_dev->larb_dev) {
			ret = mtk_smi_larb_get(ovl_dev->larb_dev);
			if (ret) {
				log_err("[ovl%d] fail to get larb, ret %d\n",
					ovl_dev->id, ret);
			} else {
				log_dbg("[ovl%d] ok to get larb\n",
					ovl_dev->id);
			}
		}
	}

	for (i = 0; i < MTK_OVL_CLK_CNT; i++) {
		ret = clk_prepare_enable(ovl_dev->clks[i]);
		if (ret) {
			log_err("[ovl%d] fail to enable clk %s",
				ovl_dev->id, _ap_mtk_ovl_clk_name[i]);
			ret = -ENXIO;
			for (i -= 1; i >= 0; i--)
				clk_disable_unprepare(ovl_dev->clks[i]);
		} else
			log_dbg("[ovl%d] ok to enable clk %s",
				ovl_dev->id, _ap_mtk_ovl_clk_name[i]);
	}

	return ret;
}

static void mtk_ovl_clock_off(struct mtk_ovl_dev *ovl_dev)
{
	int i;

	log_dbg("[ovl%d] %s[%d] start", ovl_dev->id, __func__, __LINE__);

	for (i = 0; i < MTK_OVL_CLK_CNT; i++)
		clk_disable_unprepare(ovl_dev->clks[i]);

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		if (ovl_dev->larb_dev)
			mtk_smi_larb_put(ovl_dev->larb_dev);
	}
}

static void mtk_ovl_probe_free(
	struct mtk_ovl_dev *ovl_dev, unsigned int st)
{
	int i;
	struct platform_device *pdev = ovl_dev->pdev;
	struct device *dev = &pdev->dev;

	log_dbg("will free probe st[0x%x]", st);

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		if (st & PROBE_ST_PM_ENABLE)
			pm_runtime_disable(dev);
	}

	#if SUPPORT_ALLOC_CTX
	if (st & PROBE_ST_DMA_CONTIG_INIT)
		vb2_dma_contig_cleanup_ctx(ovl_dev->alloc_ctx);
	#endif

	if (st & PROBE_ST_DMA_PARAM_SET) {
		kfree(dev->dma_parms);
		dev->dma_parms = NULL;
	}

	if (st & PROBE_ST_VIDEO_DEV_REG)
		video_unregister_device(&ovl_dev->vdev);

	if (st & PROBE_ST_V4L2_DEV_REG)
		v4l2_device_unregister(&ovl_dev->v4l2_dev);

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		if (st & PROBE_ST_REG_MAP) {
			for (i = 0; i < MTK_OVL_REG_BASE_COUNT; i++)
				devm_iounmap(
					dev,
					(void __iomem *)(ovl_dev->reg_base[i]));
		}
	}

	if (st & PROBE_ST_WORKQUEUE_CREATE) {
		flush_workqueue(ovl_dev->workqueue);
		destroy_workqueue(ovl_dev->workqueue);
	}

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		if (st & PROBE_ST_CLK_GET) {
			for (i = 0; i < MTK_OVL_CLK_CNT; i++)
				devm_clk_put(dev, ovl_dev->clks[i]);
		}
	}

	#if SUPPORT_PROBE_CLOCK_ON
	if (st & PROBE_ST_CLK_ON)
		mtk_ovl_clock_off(ovl_dev);
	#endif

	#if SUPPORT_IOMMU_ATTACH
	if (st & PROBE_ST_IOMMU_ATTACH)
		arm_iommu_detach_device(dev);
	#endif

	#if 0
	if (ovl_dev->id == MTK_OVL_MAIN_ID)
		if (st & PROBE_ST_DEV_LARB)
			log_dbg("maybe need to do something", ovl_dev);
	#endif

	if (st & PROBE_ST_MUTEX_INIT)
		mutex_destroy(&ovl_dev->lock);

	if (st & PROBE_ST_DEV_ALLOC) {
		log_dbg("will to do devm_kfree for addr[0x%lx]\n",
			(unsigned long)ovl_dev);
		devm_kfree(dev, ovl_dev);
	}
}

static int mtk_ovl_register_video_device(struct mtk_ovl_dev *ovl_dev)
{
	int ret = RET_OK;
	struct device *dev = &ovl_dev->pdev->dev;

	ovl_dev->vdev.fops = &mtk_ovl_fops;
	ovl_dev->vdev.lock = &ovl_dev->lock;
	ovl_dev->vdev.vfl_dir = VFL_DIR_TX;
	ovl_dev->vdev.release = video_device_release_empty;
	ovl_dev->vdev.v4l2_dev = &ovl_dev->v4l2_dev;
	ovl_dev->vdev.ioctl_ops = &mtk_ovl_ioctl_ops;
	snprintf(ovl_dev->vdev.name, sizeof(ovl_dev->vdev.name),
		"%s", MTK_OVL_MODULE_NAME);
	video_set_drvdata(&ovl_dev->vdev, ovl_dev);

	v4l2_disable_ioctl_locking(&ovl_dev->vdev, VIDIOC_QBUF);
	v4l2_disable_ioctl_locking(&ovl_dev->vdev, VIDIOC_DQBUF);
	v4l2_disable_ioctl_locking(&ovl_dev->vdev, VIDIOC_S_CTRL);

	ret = video_register_device(&ovl_dev->vdev, VFL_TYPE_GRABBER, 2);
	if (ret) {
		dev_err(dev, "fail to register video device\n");
		ret = RET_ERR_EXCEPTION;
	} else
		log_dbg("ok to register driver as /dev/video%d\n",
			ovl_dev->vdev.num);

	return ret;
}

static int mtk_ovl_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct mtk_ovl_dev *ovl_dev;
	struct device_node *node;
	unsigned int probe_st = 0;
	struct iommu_domain *iommu;
	struct platform_device *larb_pdev;

	log_dbg("%s[%d] start, %s", __func__, __LINE__, pdev->name);

	iommu = iommu_get_domain_for_dev(dev);
	if (!iommu) {
		log_err("Waiting iommu driver ready\n");
		return -EPROBE_DEFER;
	}

	ovl_dev = devm_kzalloc(dev, sizeof(struct mtk_ovl_dev), GFP_KERNEL);
	if (!ovl_dev)
		return -ENOMEM;
	probe_st |= PROBE_ST_DEV_ALLOC;

	platform_set_drvdata(pdev, ovl_dev);

	ret = of_property_read_u32(
		dev->of_node, "mediatek,ovlid", &ovl_dev->id);
	if (ret) {
		dev_err(dev, "fail to get id for %s\n", "mediatek,ovlid");
		goto err_handle;
	} else
		log_dbg("ok to get id[%d] for %s\n",
			ovl_dev->id, "mediatek,ovlid");

	log_dbg("[ovl%d] %s[%d] probe", ovl_dev->id, __func__, __LINE__);

	ovl_dev->pdev = pdev;

	init_waitqueue_head(&ovl_dev->irq_queue); /*lq-check*/

	mutex_init(&ovl_dev->lock);
	probe_st |= PROBE_ST_MUTEX_INIT;

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		ovl_dev->larb_dev = NULL;
		node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
		if (!node) {
			dev_err(&pdev->dev,
				"fail to get larb node\n");
			ret = -EINVAL;
			goto err_handle;
		}
		larb_pdev = of_find_device_by_node(node);
		if (!larb_pdev) {
			log_err("defer to get larb device\n");
			of_node_put(node);
			ret = -EPROBE_DEFER;
			goto err_handle;
		}
		of_node_put(node);
		ovl_dev->larb_dev = &larb_pdev->dev;
		log_dbg("ok to get larb device 0x%lx",
			(unsigned long)(ovl_dev->larb_dev));
		probe_st |= PROBE_ST_DEV_LARB;
	}

	#if SUPPORT_IOMMU_ATTACH
	/* start to attach iommu */
	node = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!node) {
		log_err("fail to parse for iommus\n");
		ret = -ENXIO;
		goto err_handle;
	}
	pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (WARN_ON(!pdev)) {
		log_err("fail to find dev for iommus\n");
		ret = -ENXIO;
		goto err_handle;
	}
	arm_iommu_attach_device(dev, pdev->dev.archdata.iommu);
	probe_st |= PROBE_ST_IOMMU_ATTACH;
	#endif

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		for (i = 0; i < MTK_OVL_CLK_CNT; i++) {
			ovl_dev->clks[i] =
				devm_clk_get(dev, _ap_mtk_ovl_clk_name[i]);
			if (IS_ERR(ovl_dev->clks[i])) {
				dev_err(dev,
					"fail to get dev[0x%lx] clk[%s] as 0x%lx\n",
					(unsigned long)dev,
					_ap_mtk_ovl_clk_name[i],
					(unsigned long)(ovl_dev->clks[i]));
				ret = -ENXIO;
				for (i -= 1; i >= 0; i--) {
					log_err("will put clk[%d][0x%lx]\n",
					i, (unsigned long)(ovl_dev->clks[i]));
					devm_clk_put(dev, ovl_dev->clks[i]);
				}
				goto err_handle;
			} else
				log_dbg("ok to get dev[0x%lx] clk[%s] as 0x%lx\n",
					(unsigned long)dev,
					_ap_mtk_ovl_clk_name[i],
					(unsigned long)(ovl_dev->clks[i]));
		}

		probe_st |= PROBE_ST_CLK_GET;
	}

	#if SUPPORT_PROBE_CLOCK_ON
	ret = mtk_ovl_clock_on(ovl_dev);
	if (ret)
		goto err_handle;
	probe_st |= PROBE_ST_CLK_ON;
	#endif

	/*lq-check*/
	ovl_dev->workqueue = create_singlethread_workqueue(MTK_OVL_MODULE_NAME);
	if (!ovl_dev->workqueue) {
		dev_err(dev, "fail to alloc for workqueue\n");
		ret = -ENOMEM;
		goto err_handle;
	}
	probe_st |= PROBE_ST_WORKQUEUE_CREATE;

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		for (i = 0; i < MTK_OVL_REG_BASE_COUNT; i++) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, i);
			if (res == NULL) {
				dev_err(dev, "fail to get memory resource\n");
				ret = -ENXIO;
				goto err_handle;
			}

			ovl_dev->reg_base[i] =
				(void *)devm_ioremap_resource(dev, res);
			if (IS_ERR((void *)(ovl_dev->reg_base[i]))) {
				dev_err(dev,
					"fail to do reg[%d] ioremap\n", i);
				ret = PTR_ERR(ovl_dev->reg_base);
				for (i -= 1; i >= 0; i--)
					devm_iounmap(dev,
						(void __iomem *)
							(ovl_dev->reg_base[i]));
				goto err_handle;
			}

			log_dbg("ok to map reg[%d] base=0x%lx",
				i, (unsigned long)(ovl_dev->reg_base[i]));
		}
		probe_st |= PROBE_ST_REG_MAP;
	}

	ret = v4l2_device_register(dev, &ovl_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "fail to register v4l2 dev\n");
		ret = -EINVAL;
		goto err_handle;
	}
	probe_st |= PROBE_ST_V4L2_DEV_REG;

	ret = mtk_ovl_register_video_device(ovl_dev);
	if (ret) {
		log_err("fail to register video dev\n");
		ret = -EINVAL;
		goto err_handle;
	}
	probe_st |= PROBE_ST_VIDEO_DEV_REG;

	/*
	 * if device has no max_seg_size set, we assume that there is no limit
	 * and force it to DMA_BIT_MASK(32) to always use contiguous mappings
	 * in DMA address space
	 */
	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms) {
			log_err("fail to alloc for dma_parms\n");
			ret = -ENOMEM;
			goto err_handle;
		}
	}
	if (dma_set_max_seg_size(dev, DMA_BIT_MASK(32)) != 0) {
		log_err("fail to set dma_set_max_seg_size\n");
		ret = -ENOMEM;
		goto err_handle;
	}
	probe_st |= PROBE_ST_DMA_PARAM_SET;

	#if SUPPORT_ALLOC_CTX
	ovl_dev->alloc_ctx = vb2_dma_contig_init_ctx(dev);
	if (IS_ERR(ovl_dev->alloc_ctx)) {
		log_err("fail to do vb2_dma_contig_init_ctx\n");
		ret = PTR_ERR(ovl_dev->alloc_ctx);
		goto err_handle;
	}
	probe_st |= PROBE_ST_DMA_CONTIG_INIT;
	#endif

	if (ovl_dev->id == MTK_OVL_MAIN_ID) {
		pm_runtime_enable(dev);
		probe_st |= PROBE_ST_PM_ENABLE;
	}

	log_dbg("ok to do ovl-%d probe\n", ovl_dev->id);
	return 0;

err_handle:
	mtk_ovl_probe_free(ovl_dev, probe_st);
	log_err("fail to do ovl-%d probe\n", ovl_dev->id);
	return ret;
}

static int mtk_ovl_remove(struct platform_device *pdev)
{
	struct mtk_ovl_dev *ovl_dev = platform_get_drvdata(pdev);

	mtk_ovl_probe_free(ovl_dev, PROBE_ST_ALL);

	log_dbg("%s driver remove ok\n", pdev->name);

	return 0;
}

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int mtk_ovl_pm_suspend(struct device *dev)
{
	#if SUPPORT_CLOCK_SUSPEND
	struct mtk_ovl_dev *ovl_dev = dev_get_drvdata(dev);
	#endif

	#if SUPPORT_CLOCK_SUSPEND
	mtk_ovl_clock_off(ovl_dev);
	#endif

	return 0;
}

static int mtk_ovl_pm_resume(struct device *dev)
{
	#if SUPPORT_CLOCK_SUSPEND
	struct mtk_ovl_dev *ovl_dev = dev_get_drvdata(dev);
	#endif

	#if SUPPORT_CLOCK_SUSPEND
	return mtk_ovl_clock_on(ovl_dev);
	#endif

	return 0;
}
#endif /* CONFIG_PM_RUNTIME || CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_SLEEP
static int mtk_ovl_suspend(struct device *dev)
{
	log_dbg("[ovl] %s[%d] start", __func__, __LINE__);

	if (pm_runtime_suspended(dev))
		return 0;

	#ifdef CONFIG_PM_RUNTIME
	return mtk_ovl_pm_suspend(dev);
	#endif

	return 0;
}

static int mtk_ovl_resume(struct device *dev)
{
	log_dbg("[ovl] %s[%d] start", __func__, __LINE__);

	if (pm_runtime_suspended(dev))
		return 0;

	#ifdef CONFIG_PM_RUNTIME
	return mtk_ovl_pm_resume(dev);
	#endif

	return 0;
}
#endif

static const struct dev_pm_ops mtk_ovl_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ovl_suspend, mtk_ovl_resume)
	SET_RUNTIME_PM_OPS(mtk_ovl_pm_suspend, mtk_ovl_pm_resume, NULL)
};

static const struct of_device_id mtk_ovl_match[] = {
	{.compatible = "mediatek,mt2712-disp-ovl-2",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ovl_match);

static struct platform_driver mtk_ovl_driver = {
	.probe  = mtk_ovl_probe,
	.remove	= mtk_ovl_remove,
	.driver	= {
		.name  = MTK_OVL_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm    = &mtk_ovl_pm_ops,
		.of_match_table = mtk_ovl_match,
	},
};
module_platform_driver(mtk_ovl_driver);

int mtk_ovl_prepare_hw(void)
{
	int ret = RET_OK;

	log_dbg("[ovl] %s[%d] start", __func__, __LINE__);

	ret = mtk_ovl_clock_on(_mtk_ovl_ctx[0]->ovl_dev);
	ret = mtk_ovl_hw_set(
		_mtk_ovl_ctx[0]->ovl_dev->reg_base[0], &_mtk_ovl_hw_param);

	log_dbg("[ovl] %s[%d] end", __func__, __LINE__);

	return ret;
}

int mtk_ovl_unprepare_hw(void)
{
	int i = 0;
	int ret = RET_OK;

	log_dbg("[ovl] %s[%d] start", __func__, __LINE__);

	mtk_ovl_hw_unset(_mtk_ovl_ctx[0]->ovl_dev->reg_base[0]);
	mtk_ovl_clock_off(_mtk_ovl_ctx[0]->ovl_dev);

	for (i = 0; i < OVL_LAYER_NUM; i++) {
		struct mtk_ovl_ctx *ctx = _mtk_ovl_ctx[i];

		#if LIST_OLD
		vb2_buffer_done(&ctx->curr_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
		#else
		vb2_buffer_done(
			&ctx->curr_vb2_v4l2_buffer->vb2_buf,
			VB2_BUF_STATE_DONE);
		#endif

		#if LIST_OLD
		spin_lock_irqsave(&ctx->buf_queue_lock, flags);
		list_del(&ctx->curr_buf->list);
		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
		#else
		/* spin_lock_irqsave(&ctx->vb2_q.lock, flags); lq-check*/
		list_del(&ctx->vb2_q.queued_list);
		/* spin_unlock_irqrestore(&ctx->vb2_q.lock, flags); lq-check*/
		#endif

		_mtk_ovl_hw_param.ovl_config[i].layer_en = 0;
	}

	log_dbg("[ovl] %s[%d] end", __func__, __LINE__);

	return ret;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek overlay driver");
MODULE_AUTHOR("Qing Li <qing.li@mediatek.com>");

