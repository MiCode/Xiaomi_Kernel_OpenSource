// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#define MTK_WARP_NAME "mtk-warp"

struct mtk_wpe_dma_regs {
	u32	base_addr;
	u32	reserved1[3];
	u32	xsize;
	u32	ysize;
	u32	stride;
	u32	reserved2[5];
};

struct mtk_wpe_regs {
	u32	wpe_start;
	u32	reserved1[4];
	u32	ctl_fmt_sel;
	u32	ctl_int_en;
	u32	reserved2;
	u32	ctl_int_status;
	u32	reserved3[23];
	u32	ctl_jump_step;
	u32	reserved4[16];
	u32	vgen_in_img;
	u32	vgen_out_img;
	u32	vgen_h_step;
	u32	vgen_v_step;
	u32	reserved5[8];
	u32	vgen_max_vec;
	u32	reserved6[63];
	u32	psp2_ctl;
	u32	reserved7[51];
	struct {
		u32	base_addr;
		u32	reserved1;
		u32	stride;
		u32	reserved2[9];
	} addr_gen[4];
	u32	reserved8[27];
	struct mtk_wpe_dma_regs	wpeo;
	u32	reserved9[12];
	struct mtk_wpe_dma_regs	veci;
	struct mtk_wpe_dma_regs	vec2i;
};

struct mtk_warp_dev {
	void __iomem		*reg;
	int			irq;
	struct clk		*clk;
	struct device		*larb;
	struct mutex		lock;
	struct completion	comp;
	struct workqueue_struct	*workqueue;

	struct device		*dev;
	struct v4l2_device	v4l2_dev;
	struct v4l2_m2m_dev	*m2m_dev;
	struct video_device	vdev;
};

struct mtk_warp_fmt {
	u32 fourcc;
	u32 depth;
	u32 align;
	u32 fmt_sel;
};

struct mtk_warp_ctx;

struct mtk_warp_q_data {
	u32				w;
	u32				h;
	u32				bytesperline;
	u32				sizeimage;
	const struct mtk_warp_fmt	*fmt;
	struct mtk_warp_ctx		*ctx;
};

struct mtk_warp_map_buf {
	size_t		size;
	void		*cpu_addr;
	dma_addr_t	dma_addr;
};

struct mtk_warp_map_data {
	u32			w;
	u32			h;
	struct mtk_warp_map_buf x;
	struct mtk_warp_map_buf y;
};

struct mtk_warp_ctx {
	struct v4l2_fh			fh;
	struct mtk_warp_dev		*warp;

	struct mtk_warp_q_data		cap_q;
	struct mtk_warp_q_data		out_q;

	struct v4l2_ctrl_handler	ctrl_handler;
	struct mtk_warp_map_data	map[2];
	struct mtk_warp_map_data	*cur_map;
	struct mtk_warp_map_data	*new_map;

	struct work_struct		work;
};

static const struct mtk_warp_fmt formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_GREY,
		.depth		= 8,
		.align		= 1,
		.fmt_sel	= 0x00,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUYV,
		.depth		= 16,
		.align		= 4,
		.fmt_sel	= 0x05,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YVYU,
		.depth		= 16,
		.align		= 4,
		.fmt_sel	= 0x15,
	},
	{
		.fourcc		= V4L2_PIX_FMT_UYVY,
		.depth		= 16,
		.align		= 4,
		.fmt_sel	= 0x25,
	},
	{
		.fourcc		= V4L2_PIX_FMT_VYUY,
		.depth		= 16,
		.align		= 4,
		.fmt_sel	= 0x35,
	},
};

#define NUM_FORMATS ARRAY_SIZE(formats)

static irqreturn_t irq_handler(int irq, void *priv)
{
	struct mtk_warp_dev *warp = priv;
	struct mtk_wpe_regs *wpe = warp->reg;

	readl(&wpe->ctl_int_status);
	complete(&warp->comp);
	return IRQ_HANDLED;
}

#define TIMEOUT_MS 100
#define WPE_STEP(x, y, shift) \
	DIV_ROUND_UP_ULL(((u64)((x) - 1)) << (shift), (y) - 1)
static void worker(struct work_struct *work)
{
	struct mtk_warp_ctx *ctx =
		 container_of(work, struct mtk_warp_ctx, work);
	struct vb2_buffer *src_buf, *dst_buf;
	dma_addr_t src_dma, dst_dma;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_DONE;
	struct mtk_wpe_regs *wpe = ctx->warp->reg;
	size_t i;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	src_dma = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	dst_dma = vb2_dma_contig_plane_dma_addr(dst_buf, 0);

	writel(ctx->out_q.fmt->fmt_sel, &wpe->ctl_fmt_sel);
	writel((ctx->out_q.fmt->fmt_sel == 0x00) ? 0 : 3, &wpe->psp2_ctl);

	writel(ctx->cur_map->h << 16 | ctx->cur_map->w, &wpe->vgen_in_img);
	writel(ctx->cap_q.h << 16 | ctx->cap_q.w, &wpe->vgen_out_img);
	writel(ctx->out_q.h << 16 | ctx->out_q.w, &wpe->vgen_max_vec);

	writel(WPE_STEP(ctx->out_q.h, ctx->cap_q.h, 16), &wpe->ctl_jump_step);
	writel(WPE_STEP(ctx->cur_map->w, ctx->cap_q.w, 24), &wpe->vgen_h_step);
	writel(WPE_STEP(ctx->cur_map->h, ctx->cap_q.h, 24), &wpe->vgen_v_step);

	for (i = 0; i < 4; i++) {
		writel(src_dma, &wpe->addr_gen[i].base_addr);
		writel(ctx->out_q.bytesperline, &wpe->addr_gen[i].stride);
	}

	writel(dst_dma, &wpe->wpeo.base_addr);
	writel(ctx->cap_q.bytesperline - 1, &wpe->wpeo.xsize);
	writel(ctx->cap_q.h - 1, &wpe->wpeo.ysize);
	writel(ctx->cap_q.bytesperline, &wpe->wpeo.stride);

	writel(ctx->cur_map->x.dma_addr, &wpe->veci.base_addr);
	writel(ctx->cur_map->w * 4 - 1, &wpe->veci.xsize);
	writel(ctx->cur_map->h - 1, &wpe->veci.ysize);
	writel(ctx->cur_map->w * 4, &wpe->veci.stride);

	writel(ctx->cur_map->y.dma_addr, &wpe->vec2i.base_addr);
	writel(ctx->cur_map->w * 4 - 1, &wpe->vec2i.xsize);
	writel(ctx->cur_map->h - 1, &wpe->vec2i.ysize);
	writel(ctx->cur_map->w * 4, &wpe->vec2i.stride);

	writel(1, &wpe->ctl_int_en);
	writel(1, &wpe->wpe_start);

	if (wait_for_completion_timeout(&ctx->warp->comp,
					msecs_to_jiffies(TIMEOUT_MS)) == 0) {
		v4l2_err(&ctx->warp->v4l2_dev, "timeout\n");
		buf_state = VB2_BUF_STATE_ERROR;
	}

	writel(0, &wpe->ctl_int_en);

	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf), buf_state);
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), buf_state);
	v4l2_m2m_job_finish(ctx->warp->m2m_dev, ctx->fh.m2m_ctx);
}

static void device_run(void *priv)
{
	struct mtk_warp_ctx *ctx = priv;

	queue_work(ctx->warp->workqueue, &ctx->work);
}

static int job_ready(void *priv)
{
	struct mtk_warp_ctx *ctx = priv;

	if (ctx->cap_q.fmt != ctx->out_q.fmt) {
		v4l2_err(&ctx->warp->v4l2_dev,
			 "format of out_q is not the same as cap_q\n");
		return 0;
	}

	if (ctx->cur_map->w == 0 || ctx->cur_map->h == 0 ||
	    !ctx->cur_map->x.cpu_addr || !ctx->cur_map->y.cpu_addr) {
		v4l2_err(&ctx->warp->v4l2_dev,
			 "no warp map\n");
		return 0;
	}

	return 1;
}

static void job_abort(void *priv)
{
}

static const struct v4l2_m2m_ops m2m_ops = {
	.device_run	= device_run,
	.job_ready	= job_ready,
	.job_abort	= job_abort,
};

static struct mtk_warp_ctx *fh2ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_warp_ctx, fh);
}

static int mtk_warp_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers,
				unsigned int *num_planes,
				unsigned int sizes[],
				struct device *alloc_ctxs[])
{
	struct mtk_warp_q_data *q_data = vb2_get_drv_priv(q);

	*num_planes = 1;
	sizes[0] = q_data->sizeimage;

	return 0;
}

static int mtk_warp_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_warp_q_data *q_data = vb2_get_drv_priv(vb->vb2_queue);

	vb2_set_plane_payload(vb, 0, q_data->sizeimage);

	return 0;
}

static void mtk_warp_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_warp_q_data *q_data = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_m2m_buf_queue(q_data->ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static int mtk_warp_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_warp_q_data *q_data = vb2_get_drv_priv(q);
	int ret;

	ret = pm_runtime_get_sync(q_data->ctx->warp->dev);
	if (ret < 0)
		return ret;

	return 0;
}

static void *buf_remove(struct mtk_warp_ctx *ctx, enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static void mtk_warp_stop_streaming(struct vb2_queue *q)
{
	struct mtk_warp_q_data *q_data = vb2_get_drv_priv(q);
	struct vb2_buffer *vb;

	while ((vb = buf_remove(q_data->ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);

	pm_runtime_put_sync(q_data->ctx->warp->dev);
}

static const struct vb2_ops mtk_warp_qops = {
	.queue_setup		= mtk_warp_queue_setup,
	.buf_prepare		= mtk_warp_buf_prepare,
	.buf_queue		= mtk_warp_buf_queue,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
	.stop_streaming		= mtk_warp_stop_streaming,
	.start_streaming	= mtk_warp_start_streaming,
};

static int queue_init(void *priv, struct vb2_queue *src_vq,
		      struct vb2_queue *dst_vq)
{
	struct mtk_warp_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv = &ctx->out_q;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &mtk_warp_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->warp->lock;
	src_vq->dev = ctx->warp->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	dst_vq->drv_priv = &ctx->cap_q;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_warp_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->warp->lock;
	dst_vq->dev = ctx->warp->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

#define V4L2_CID_WARP_MAP_WIDTH		(V4L2_CID_USER_MTK_WARP_BASE + 0)
#define V4L2_CID_WARP_MAP_HEIGHT	(V4L2_CID_USER_MTK_WARP_BASE + 1)
#define V4L2_CID_WARP_MAP_X		(V4L2_CID_USER_MTK_WARP_BASE + 2)
#define V4L2_CID_WARP_MAP_Y		(V4L2_CID_USER_MTK_WARP_BASE + 3)
#define V4L2_CID_WARP_MAP_COMMIT	(V4L2_CID_USER_MTK_WARP_BASE + 4)
#define MTK_WARP_MAX_CTRL_NUM		5

static int mtk_warp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_warp_ctx *ctx =
		container_of(ctrl->handler, struct mtk_warp_ctx, ctrl_handler);
	struct mtk_warp_map_buf *map = NULL;

	switch (ctrl->id) {
	case V4L2_CID_WARP_MAP_WIDTH:
		ctx->new_map->w = ctrl->val;
		break;
	case V4L2_CID_WARP_MAP_HEIGHT:
		ctx->new_map->h = ctrl->val;
		break;
	case V4L2_CID_WARP_MAP_X:
	case V4L2_CID_WARP_MAP_Y:
		map = (ctrl->id == V4L2_CID_WARP_MAP_X) ? &ctx->new_map->x :
							  &ctx->new_map->y;

		if (ctx->new_map->w == 0 || ctx->new_map->h == 0)
			return -EINVAL;

		if (map->cpu_addr)
			dma_free_coherent(ctx->warp->dev, map->size,
					  map->cpu_addr, map->dma_addr);

		map->size = ctx->new_map->w * ctx->new_map->h * 4;
		map->cpu_addr = dma_alloc_coherent(ctx->warp->dev, map->size,
						   &map->dma_addr, GFP_KERNEL);

		if (copy_from_user(map->cpu_addr,
				   (void *)(long)*ctrl->p_new.p_s64,
				   map->size)) {
			dma_free_coherent(ctx->warp->dev, map->size,
					  map->cpu_addr, map->dma_addr);
			map->cpu_addr = NULL;
			return -EINVAL;
		}

		break;
	case V4L2_CID_WARP_MAP_COMMIT:
		if (ctrl->val == 0)
			return 0;

		if (ctx->new_map->w == 0 || ctx->new_map->h == 0 ||
		    !ctx->new_map->x.cpu_addr || !ctx->new_map->y.cpu_addr)
			return -EINVAL;

		swap(ctx->new_map, ctx->cur_map);

		ctx->new_map->w = 0;
		ctx->new_map->h = 0;
		if (ctx->new_map->x.cpu_addr)
			dma_free_coherent(ctx->warp->dev, ctx->new_map->x.size,
					  ctx->new_map->x.cpu_addr,
					  ctx->new_map->x.dma_addr);
		if (ctx->new_map->y.cpu_addr)
			dma_free_coherent(ctx->warp->dev, ctx->new_map->y.size,
					  ctx->new_map->y.cpu_addr,
					  ctx->new_map->y.dma_addr);
		ctx->new_map->x.cpu_addr = NULL;
		ctx->new_map->y.cpu_addr = NULL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = mtk_warp_s_ctrl,
};

static const struct v4l2_ctrl_config ctrl_map_width = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_WARP_MAP_WIDTH,
	.name = "WARP MAP WIDTH",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 2,
	.max = 640,
	.step = 1,
	.def = 2,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
};

static const struct v4l2_ctrl_config ctrl_map_height = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_WARP_MAP_HEIGHT,
	.name = "WARP MAP HEIGHT",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 2,
	.max = 480,
	.step = 1,
	.def = 2,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
};

static const struct v4l2_ctrl_config ctrl_map_x = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_WARP_MAP_X,
	.name = "WARP MAP X",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = S64_MIN,
	.max = S64_MAX,
	.step = 4,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
};

static const struct v4l2_ctrl_config ctrl_map_y = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_WARP_MAP_Y,
	.name = "WARP MAP Y",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = S64_MIN,
	.max = S64_MAX,
	.step = 4,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
};

static const struct v4l2_ctrl_config ctrl_map_commit = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_WARP_MAP_COMMIT,
	.name = "WARP MAP COMMIT",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
};

static int mtk_warp_open(struct file *file)
{
	struct mtk_warp_dev *warp = video_drvdata(file);
	struct video_device *vdev = video_devdata(file);
	struct mtk_warp_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, MTK_WARP_MAX_CTRL_NUM);
	v4l2_ctrl_new_custom(&ctx->ctrl_handler, &ctrl_map_width, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrl_handler, &ctrl_map_height, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrl_handler, &ctrl_map_x, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrl_handler, &ctrl_map_y, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrl_handler, &ctrl_map_commit, NULL);
	if (ctx->ctrl_handler.error) {
		ret = ctx->ctrl_handler.error;
		goto err_ctrl;
	}

	v4l2_fh_init(&ctx->fh, vdev);
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;

	ctx->warp = warp;
	ctx->cap_q.fmt = &formats[0];
	ctx->out_q.fmt = &formats[0];
	ctx->cap_q.ctx = ctx;
	ctx->out_q.ctx = ctx;
	ctx->cur_map = &ctx->map[0];
	ctx->new_map = &ctx->map[1];

	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(warp->m2m_dev, ctx, &queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto err_ctx;
	}

	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	INIT_WORK(&ctx->work, worker);

	return 0;

err_ctx:
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
err_ctrl:
	kfree(ctx);
	return ret;
}

static int mtk_warp_release(struct file *file)
{
	struct mtk_warp_ctx *ctx = fh2ctx(file->private_data);

	v4l2_fh_del(&ctx->fh);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	kfree(ctx);
	return 0;
}

static const struct v4l2_file_operations mtk_warp_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_warp_open,
	.release	= mtk_warp_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static int mtk_warp_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	struct mtk_warp_dev *warp = video_drvdata(file);

	strncpy(cap->driver, MTK_WARP_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, MTK_WARP_NAME, sizeof(cap->card) - 1);
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(warp->dev));
	cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int mtk_warp_enum_fmt(struct file *file, void *priv,
			     struct v4l2_fmtdesc *f)
{
	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	f->pixelformat = formats[f->index].fourcc;
	return 0;
}

static const struct mtk_warp_fmt *find_format(u32 fourcc)
{
	size_t i;

	for (i = 0; i < NUM_FORMATS; i++)
		if (formats[i].fourcc == fourcc)
			return &formats[i];

	return NULL;
}

#define IMG_MIN_WIDTH		16U
#define IMG_MAX_WIDTH		3840U
#define IMG_MIN_HEIGHT		16U
#define IMG_MAX_HEIGHT		2160U
#define WARP_MIN_STRIDE		16U
#define WARP_MAX_STRIDE		32768U
#define IMG_STRIDE_ALIGN	64U

static int mtk_warp_try_fmt(struct v4l2_format *f, struct mtk_warp_ctx *ctx)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;
	const struct mtk_warp_fmt *fmt;
	u32 bytesperline;

	pix->width = clamp(pix->width, IMG_MIN_WIDTH, IMG_MAX_WIDTH);
	pix->height = clamp(pix->height, IMG_MIN_HEIGHT, IMG_MAX_HEIGHT);

	fmt = find_format(pix->pixelformat);
	if (!fmt) {
		fmt = ctx->out_q.fmt;
		pix->pixelformat = fmt->fourcc;
	}

	pix->field = V4L2_FIELD_NONE;

	bytesperline = ALIGN(pix->width * fmt->depth >> 3, fmt->align);
	bytesperline = max(bytesperline, pix->bytesperline);
	bytesperline = ALIGN(bytesperline, IMG_STRIDE_ALIGN);
	bytesperline = clamp(bytesperline, WARP_MIN_STRIDE, WARP_MAX_STRIDE);
	pix->bytesperline = bytesperline;
	pix->sizeimage = max(pix->sizeimage, pix->bytesperline * pix->height);

	return 0;
}

static int mtk_warp_try_fmt_vid_cap(struct file *file, void *priv,
				    struct v4l2_format *f)
{
	struct mtk_warp_ctx *ctx = fh2ctx(priv);

	f->fmt.pix.pixelformat = 0;
	return mtk_warp_try_fmt(f, ctx);
}

static int mtk_warp_try_fmt_vid_out(struct file *file, void *priv,
				    struct v4l2_format *f)
{
	return mtk_warp_try_fmt(f, fh2ctx(priv));
}

static int mtk_warp_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vb2_queue *q;
	struct mtk_warp_q_data *q_data;
	struct mtk_warp_ctx *ctx = fh2ctx(priv);
	struct v4l2_pix_format *pix = &f->fmt.pix;

	q = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!q)
		return -EINVAL;

	q_data = vb2_get_drv_priv(q);

	pix->width = q_data->w;
	pix->height = q_data->h;
	pix->pixelformat = q_data->fmt->fourcc;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = q_data->bytesperline;
	pix->sizeimage = q_data->sizeimage;

	return 0;
}

static int mtk_warp_s_fmt(struct mtk_warp_ctx *ctx, struct v4l2_format *f)
{
	struct vb2_queue *q;
	struct mtk_warp_q_data *q_data;
	struct v4l2_pix_format *pix = &f->fmt.pix;

	q = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!q)
		return -EINVAL;

	if (vb2_is_busy(q)) {
		v4l2_err(&ctx->warp->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	q_data = vb2_get_drv_priv(q);

	q_data->w = pix->width;
	q_data->h = pix->height;
	q_data->fmt = find_format(pix->pixelformat);
	q_data->bytesperline = pix->bytesperline;
	q_data->sizeimage = pix->sizeimage;

	return 0;
}

static int mtk_warp_s_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int ret;

	ret = mtk_warp_try_fmt_vid_cap(file, priv, f);
	if (ret)
		return ret;

	return mtk_warp_s_fmt(fh2ctx(priv), f);
}

static int mtk_warp_s_fmt_vid_out(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int ret;

	ret = mtk_warp_try_fmt_vid_out(file, priv, f);
	if (ret)
		return ret;

	return mtk_warp_s_fmt(fh2ctx(priv), f);
}

static const struct v4l2_ioctl_ops mtk_warp_ioctl_ops = {
	.vidioc_querycap		= mtk_warp_querycap,
	.vidioc_enum_fmt_vid_cap	= mtk_warp_enum_fmt,
	.vidioc_enum_fmt_vid_out	= mtk_warp_enum_fmt,
	.vidioc_try_fmt_vid_cap		= mtk_warp_try_fmt_vid_cap,
	.vidioc_try_fmt_vid_out		= mtk_warp_try_fmt_vid_out,
	.vidioc_g_fmt_vid_cap		= mtk_warp_g_fmt,
	.vidioc_g_fmt_vid_out		= mtk_warp_g_fmt,
	.vidioc_s_fmt_vid_cap		= mtk_warp_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out		= mtk_warp_s_fmt_vid_out,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs			= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf			= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf			= v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon		= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff		= v4l2_m2m_ioctl_streamoff,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event       = v4l2_event_unsubscribe,
};

static int mtk_warp_probe(struct platform_device *pdev)
{
	struct mtk_warp_dev *warp;
	struct resource *res;
	int ret;
	struct device_node *larb_node;
	struct platform_device *larb_pdev;

	warp = devm_kzalloc(&pdev->dev, sizeof(*warp), GFP_KERNEL);
	if (!warp)
		return -ENOMEM;

	mutex_init(&warp->lock);
	warp->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	warp->reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(warp->reg)) {
		dev_err(&pdev->dev, "Fail to ioremap resource\n");
		return PTR_ERR(warp->reg);
	}

	warp->irq = platform_get_irq(pdev, 0);
	if (warp->irq < 0) {
		dev_err(&pdev->dev, "Fail to get irq %d\n", warp->irq);
		return warp->irq;
	}

	init_completion(&warp->comp);
	ret = devm_request_irq(&pdev->dev, warp->irq, irq_handler, 0,
			       MTK_WARP_NAME, warp);
	if (ret) {
		dev_err(&pdev->dev, "Fail to request irq %d (%d)\n",
			warp->irq, ret);
		return ret;
	}

	warp->clk = devm_clk_get(&pdev->dev, "wpe");
	if (IS_ERR(warp->clk)) {
		dev_err(&pdev->dev, "Fail to get main clk\n");
		return PTR_ERR(warp->clk);
	}

	larb_node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!larb_node) {
		dev_err(&pdev->dev, "Fail to get medkiatek,larb\n");
		return -EINVAL;
	}

	larb_pdev = of_find_device_by_node(larb_node);
	if (!larb_pdev) {
		dev_warn(&pdev->dev, "Waiting for larb devcie %s\n",
			 larb_node->full_name);
		of_node_put(larb_node);
		return -EPROBE_DEFER;
	}
	of_node_put(larb_node);
	warp->larb = &larb_pdev->dev;

	warp->workqueue = create_singlethread_workqueue(MTK_WARP_NAME);
	if (!warp->workqueue) {
		dev_err(&pdev->dev, "Fail to create workqueue\n");
		return -ENOMEM;
	}

	ret = v4l2_device_register(&pdev->dev, &warp->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Fail to register v4l2 device\n");
		goto err_v4l2_dev_register;
	}

	warp->m2m_dev = v4l2_m2m_init(&m2m_ops);
	if (IS_ERR(warp->m2m_dev)) {
		v4l2_err(&warp->v4l2_dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(warp->m2m_dev);
		goto err_m2m;
	}

	snprintf(warp->vdev.name, sizeof(warp->vdev.name), "%s-m2m",
		 MTK_WARP_NAME);
	warp->vdev.fops = &mtk_warp_fops;
	warp->vdev.ioctl_ops = &mtk_warp_ioctl_ops;
	warp->vdev.minor = -1;
	warp->vdev.release = video_device_release_empty;
	warp->vdev.lock = &warp->lock;
	warp->vdev.v4l2_dev = &warp->v4l2_dev;
	warp->vdev.vfl_dir = VFL_DIR_M2M;
	ret = video_register_device(&warp->vdev, VFL_TYPE_GRABBER, 0);
	if (ret) {
		v4l2_err(&warp->v4l2_dev, "Failed to register video device\n");
		goto err_vdev_register;
	}

	video_set_drvdata(&warp->vdev, warp);
	v4l2_info(&warp->v4l2_dev,
		  "device registered as /dev/video%d (%d,%d)\n",
		  warp->vdev.num, VIDEO_MAJOR, warp->vdev.minor);

	pm_runtime_enable(&pdev->dev);
	platform_set_drvdata(pdev, warp);

	return 0;

err_vdev_register:
	v4l2_m2m_release(warp->m2m_dev);
err_m2m:
	v4l2_device_unregister(&warp->v4l2_dev);
err_v4l2_dev_register:
	destroy_workqueue(warp->workqueue);
	return ret;
}

static int mtk_warp_remove(struct platform_device *pdev)
{
	struct mtk_warp_dev *warp = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	video_unregister_device(&warp->vdev);
	v4l2_m2m_release(warp->m2m_dev);
	v4l2_device_unregister(&warp->v4l2_dev);
	flush_workqueue(warp->workqueue);
	destroy_workqueue(warp->workqueue);

	return 0;
}

static const struct of_device_id mtk_warp_match[] = {
	{ .compatible = "mediatek,wpe" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_warp_match);

static void mtk_warp_clock_on(struct mtk_warp_dev *warp)
{
	int err;

	err = pm_runtime_get_sync(warp->larb);
	if (err)
		dev_err(warp->dev, "Fail to get larb, err %d\n", err);
	clk_prepare_enable(warp->clk);
}

static void mtk_warp_clock_off(struct mtk_warp_dev *warp)
{
	clk_disable_unprepare(warp->clk);
	pm_runtime_put_sync(warp->larb);
}

static int __maybe_unused mtk_warp_pm_suspend(struct device *dev)
{
	struct mtk_warp_dev *warp = dev_get_drvdata(dev);

	mtk_warp_clock_off(warp);
	return 0;
}

static int __maybe_unused mtk_warp_pm_resume(struct device *dev)
{
	struct mtk_warp_dev *warp = dev_get_drvdata(dev);

	mtk_warp_clock_on(warp);
	return 0;
}

static __maybe_unused int mtk_warp_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_warp_pm_suspend(dev);
}

static int __maybe_unused mtk_warp_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mtk_warp_pm_resume(dev);
}

static const struct dev_pm_ops mtk_warp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_warp_suspend, mtk_warp_resume)
	SET_RUNTIME_PM_OPS(mtk_warp_pm_suspend, mtk_warp_pm_resume, NULL)
};

static struct platform_driver mtk_warp_driver = {
	.probe = mtk_warp_probe,
	.remove = mtk_warp_remove,
	.driver = {
		.name		= MTK_WARP_NAME,
		.of_match_table	= mtk_warp_match,
		.pm		= &mtk_warp_pm_ops,
	},
};

module_platform_driver(mtk_warp_driver);

MODULE_DESCRIPTION("MediaTek WARP driver");
MODULE_LICENSE("GPL v2");
