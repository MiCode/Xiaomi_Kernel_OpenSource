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


#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/errno.h>

#include <linux/io.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/mfd/syscon.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#ifdef CONFIG_MTK_IOMMU
#include <linux/iommu.h>
#endif
#include <soc/mediatek/smi.h>
#include <asm/cacheflush.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>

#include <linux/regmap.h>

#include "mtk_ovl_util.h"
#include "mtk_wdma.h"


#define MTK_WDMA_MODULE_NAME            "mtk-wdma"
#define MTK_WDMA_CLK_CNT                2
#define MTK_WDMA_REG_BASE_COUNT         1

enum MTK_WDMA_PROBE_STATUS {
	PROBE_ST_DEV_ALLOC,
	PROBE_ST_MUTEX_INIT,
	PROBE_ST_DEV_LARB,
	#if SUPPORT_IOMMU_ATTACH
	PROBE_ST_IOMMU_ATTACH,
	#endif
	PROBE_ST_CLK_GET,
	PROBE_ST_CLK_ON,
	PROBE_ST_WORKQUEUE_CREATE,
	PROBE_ST_IRQ_REQ,
	PROBE_ST_REG_MAP,
	PROBE_ST_REG_MMSYS_MAP,
	PROBE_ST_REG_MUTEX_MAP,
	PROBE_ST_V4L2_DEV_REG,
	PROBE_ST_VIDEO_DEV_REG,
	PROBE_ST_DMA_PARAM_SET,
	PROBE_ST_DMA_CONTIG_INIT,
	PROBE_ST_PM_ENABLE
};

/*
 *  struct mtk_wdma_pix_max - picture pixel size limits in various IP config
 *  @org_w:				max  source pixel width
 *  @org_h:				max  source pixel height
 *  @target_w:				max  output pixel height
 *  @target_h:				max  output pixel height
 */
struct mtk_wdma_pix_max {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

/*
 *  struct mtk_wdma_pix_min - picture pixel size limits in various IP config
 *
 *  @org_w: minimum source pixel width
 *  @org_h: minimum source pixel height
 *  @target_w: minimum output pixel height
 *  @target_h: minimum output pixel height
 */
struct mtk_wdma_pix_min {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

struct mtk_wdma_pix_align {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

/*
 * struct mtk_ovl_variant - video quality variant information
 */
struct mtk_wdma_variant {
	struct mtk_wdma_pix_max          *pix_max;
	struct mtk_wdma_pix_min          *pix_min;
	struct mtk_wdma_pix_align        *pix_align;
	u16                              in_buf_cnt;
	u16                              out_buf_cnt;
};

struct mtk_wdma_crop {
	u32    top;
	u32    left;
	u32    width;
	u32    height;
};

struct mtk_wdma_hw_clks {
	struct clk *clk_mm_disp_wdma2;
};

/**
 * struct mtk_wdma_fmt - the driver's internal color format data
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
struct mtk_wdma_fmt {
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

struct mtk_wdma_dev {
	struct mutex              lock;
	/* struct mutex              wdmalock; */
	struct platform_device    *pdev;
	struct vb2_alloc_ctx      *alloc_ctx;
	struct video_device       vdev;
	struct v4l2_device        v4l2_dev;
	struct device             *larb[2];
	struct workqueue_struct   *workqueue;
	struct platform_device    *wdma_dev;
	struct clk                *clks[MTK_WDMA_CLK_CNT];

	wait_queue_head_t         irq_queue;
	unsigned long             state;
	int                       id;
	void                      *reg_base[MTK_WDMA_REG_BASE_COUNT];

	struct regmap	          *mmsys_regmap;
	struct regmap	          *mutex_regmap;

	struct device             *larb_dev;
};

/**
 * struct mtk_wdma_frame - source frame properties
 * @f_width:        SRC : SRCIMG_WIDTH, DST : OUTPUTDMA_WHOLE_IMG_WIDTH
 * @f_height:       SRC : SRCIMG_HEIGHT, DST : OUTPUTDMA_WHOLE_IMG_HEIGHT
 * @crop:           cropped(source)/scaled(destination) size
 * @payload:        image size in bytes (w x h x bpp)
 * @pitch:          bytes per line of image in memory
 * @fmt:            color format pointer
 * @colorspace:	    value indicating v4l2_colorspace
 * @alpha:          frame's alpha value
 */
struct mtk_wdma_frame {
	struct v4l2_rect            crop;
	u32                         f_width;
	u32                         f_height;
	unsigned long               payload[VIDEO_MAX_PLANES];
	unsigned int                pitch[VIDEO_MAX_PLANES];
	const struct mtk_wdma_fmt   *fmt;
	u32                         colorspace;
	u8                          alpha;
};

struct mtk_wdma_buffer {
	struct vb2_v4l2_buffer    vb;
	struct list_head          list;
};

/*
 * mtk_wdma_ctx - the device context data
 * @s_frame:         source frame properties
 * @wdma_dev:         the wdma device which this context applies to
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
struct mtk_wdma_ctx {
	struct vb2_queue        vb2_q;
	struct mtk_wdma_frame   s_frame;
	struct mtk_wdma_dev     *wdma_dev;
	struct v4l2_fh          fh;
	struct mutex            qlock;
	struct mutex            slock;
	#if LIST_OLD
	struct mtk_wdma_buffer  *curr_buf;
	#else
	struct vb2_v4l2_buffer  *curr_vb2_v4l2_buffer;
	#endif

	u32                     flags;
	int                     layer_idx;

	spinlock_t                buf_queue_lock; /* protect bufqueue */
	struct list_head          buf_queue_list;
	/* struct mtk_wdma_crop      crop; */
	struct mtk_wdma_variant   *variant;
};

static struct MTK_WDMA_HW_PARAM _mtk_wdma_hw_param = {0};

static char *_ap_mtk_wdma_clk_name[MTK_WDMA_CLK_CNT] = {
	"mm_disp_wdma2",
	"mm_mutex_32k"
};

static struct mtk_wdma_pix_max mtk_wdma_size_max = {
	.org_w			= 1920,
	.org_h			= 1080,
	.target_w		= 1920,
	.target_h		= 1080,
};

static struct mtk_wdma_pix_min mtk_wdma_size_min = {
	.org_w			= 16,
	.org_h			= 16,
	.target_w		= 16,
	.target_h		= 16,
};

static struct mtk_wdma_pix_align mtk_wdma_size_align = {
	.org_w			= 16,
	.org_h			= 16,
	.target_w		= 16,
	.target_h		= 16,
};

static struct mtk_wdma_variant mtk_wdma_default_variant = {
	.pix_max		= &mtk_wdma_size_max,
	.pix_min		= &mtk_wdma_size_min,
	.pix_align		= &mtk_wdma_size_align,
	.in_buf_cnt		= 100,
	.out_buf_cnt	= 100,
};

static const struct mtk_wdma_fmt mtk_wdma_formats[] = {
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

static atomic_t _EventWqIrqFlag = ATOMIC_INIT(0);
static wait_queue_head_t _EventWqIrqHandle;

static inline struct mtk_wdma_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_wdma_ctx, fh);
}

static enum MTK_WDMA_HW_FORMAT mtk_wdma_fmt_convert(unsigned int fmt)
{
	enum MTK_WDMA_HW_FORMAT wdma_fmt = MTK_WDMA_HW_FORMAT_UNKNOWN;

	switch (fmt) {
	case V4L2_PIX_FMT_RGB565:
		wdma_fmt = MTK_WDMA_HW_FORMAT_RGB565;
		break;
	case V4L2_PIX_FMT_RGB24:
		wdma_fmt = MTK_WDMA_HW_FORMAT_RGB888;
		break;
	case V4L2_PIX_FMT_BGR24:
		wdma_fmt = MTK_WDMA_HW_FORMAT_BGR888;
		break;
	case V4L2_PIX_FMT_ARGB32:
		wdma_fmt = MTK_WDMA_HW_FORMAT_ARGB8888;
		break;
	case V4L2_PIX_FMT_ABGR32:
		wdma_fmt = MTK_WDMA_HW_FORMAT_ABGR8888;
		break;
	/*case eYUY2:*/
	case V4L2_PIX_FMT_YUYV:
		wdma_fmt = MTK_WDMA_HW_FORMAT_YUYV;
		break;
	case V4L2_PIX_FMT_YVYU:
		wdma_fmt = MTK_WDMA_HW_FORMAT_YVYU;
		break;
	case V4L2_PIX_FMT_VYUY:
		wdma_fmt = MTK_WDMA_HW_FORMAT_VYUY;
		break;
	case V4L2_PIX_FMT_UYVY:
		wdma_fmt = MTK_WDMA_HW_FORMAT_UYVY;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M:
		wdma_fmt = MTK_WDMA_HW_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV21M:
		wdma_fmt = MTK_WDMA_HW_FORMAT_NV21;
		break;
	#if MTK_OVL_SUPPORT_FMT_ALL
	case eYV12:
		wdma_fmt = MTK_WDMA_HW_FORMAT_YV12;
		break;
	case eYUV_420_3P:
		wdma_fmt = MTK_WDMA_HW_FORMAT_IYUV;
		break;
	case eBGR565:
		wdma_fmt = MTK_WDMA_HW_FORMAT_BGR565;
		break;
	case eRGBA8888:
		wdma_fmt = MTK_WDMA_HW_FORMAT_RGBA8888;
		break;
	case eBGRA8888:
		wdma_fmt = MTK_WDMA_HW_FORMAT_BGRA8888;
		break;
	#endif
	default:
		log_err("[wdma] %s[%d] unsupport wdma output fmt=0x%x",
			__func__, __LINE__, fmt);
		break;
	}

	return wdma_fmt;
}

irqreturn_t mtk_wdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_wdma_dev *wdma_dev = dev_id;

	log_dbg("[wdma] irq occur");

	mtk_wdma_hw_irq_clear(wdma_dev->reg_base[0]);

	atomic_set(&_EventWqIrqFlag, 1);
	wake_up_interruptible(&_EventWqIrqHandle);

	return IRQ_HANDLED;
}

static int mtk_wdma_wait_irq(void *reg_base)
{
	int ret = RET_OK;

	if (wait_event_interruptible_timeout(
			_EventWqIrqHandle,
			atomic_read(&_EventWqIrqFlag),
			HZ / 10) == 0) {
		log_err("[wdma] fail to wait irq");
		ret = RET_ERR_EXCEPTION;
	} else {
		log_dbg("[wdma] ok to wait irq");
	}

	atomic_set(&_EventWqIrqFlag, 0);

	return ret;
}

static int mtk_wdma_clock_on(struct mtk_wdma_dev *wdma_dev)
{
	int i;
	int ret;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (wdma_dev->larb_dev) {
		ret = mtk_smi_larb_get(wdma_dev->larb_dev);
		if (ret) {
			/* just for checkpatch */
			log_err("[wdma] fail to get larb, ret %d\n", ret);
		} else {
			/* just for checkpatch */
			log_dbg("[wdma] ok to get larb\n");
		}
	}

	for (i = 0; i < MTK_WDMA_CLK_CNT; i++) {
		ret = clk_prepare_enable(wdma_dev->clks[i]);
		if (ret) {
			log_err("[wdma] fail to enable clk %s",
				_ap_mtk_wdma_clk_name[i]);
			ret = -ENXIO;
			for (i -= 1; i >= 0; i--)
				clk_disable_unprepare(wdma_dev->clks[i]);
		} else
			log_dbg("[wdma] ok to enable clk %s",
				_ap_mtk_wdma_clk_name[i]);
	}

	return ret;
}

static void mtk_wdma_clock_off(struct mtk_wdma_dev *wdma_dev)
{
	int i;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	for (i = 0; i < MTK_WDMA_CLK_CNT; i++)
		clk_disable_unprepare(wdma_dev->clks[i]);

	if (wdma_dev->larb_dev)
		mtk_smi_larb_put(wdma_dev->larb_dev);
}

#define DISP_OVL1_MOUT_EN        0x44
#define DISP_WDMA1_SEL_IN        0x9C
static void mtk_wdma_mmsys_path_connext(struct regmap *mmsys_regmap)
{
	unsigned int addr;
	unsigned int val;

	addr = DISP_OVL1_MOUT_EN;
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys org 0x%x = 0x%x", addr, val);
	val &= (unsigned int)(!(1 << 16));
	log_dbg("[wdma] mmsys set-1 0x%x = 0x%x", addr, val);
	val |= (unsigned int)(1 << 17);
	log_dbg("[wdma] mmsys set-2 0x%x = 0x%x", addr, val);
	regmap_write(mmsys_regmap, addr, val);
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);

	addr = DISP_WDMA1_SEL_IN;
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys org 0x%x = 0x%x", addr, val);
	val |= (unsigned int)(1 << 16);
	log_dbg("[wdma] mmsys set 0x%x = 0x%x", addr, val);
	regmap_write(mmsys_regmap, addr, val);
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);
}

static void mtk_wdma_mmsys_path_disconnext(struct regmap *mmsys_regmap)
{
	unsigned int addr;
	unsigned int val;

	addr = DISP_OVL1_MOUT_EN;
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys org 0x%x = 0x%x", addr, val);
	val &= (unsigned int)(!(1 << 16));
	log_dbg("[wdma] mmsys set-1 0x%x = 0x%x", addr, val);
	val &= (unsigned int)(!(1 << 17));
	log_dbg("[wdma] mmsys set-2 0x%x = 0x%x", addr, val);
	regmap_write(mmsys_regmap, addr, val);
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);

	addr = DISP_WDMA1_SEL_IN;
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys org 0x%x = 0x%x", addr, val);
	val &= (unsigned int)(!(1 << 16));
	log_dbg("[wdma] mmsys set 0x%x = 0x%x", addr, val);
	regmap_write(mmsys_regmap, addr, val);
	regmap_read(mmsys_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);
}

#define DISP_MUTEX_EN        0x20
#define DISP_MUTEX_MOD       0x2C
#define DISP_MUTEX_SOF       0x30
#define DISP_MUTEX_MOD2      0x34
static void mtk_wdma_mutex_enable(struct regmap *mutex_regmap)
{
	unsigned int addr;
	unsigned int val;

	addr = (DISP_MUTEX_MOD + 0x20 * DISP_MUTEX_IDX);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex org 0x%x = 0x%x", addr, val);
	val |= (unsigned int)((1 << 30) | (1 << 31));
	log_dbg("[wdma] mutex set 0x%x = 0x%x", addr, val);
	regmap_write(mutex_regmap, addr, val);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex new 0x%x = 0x%x", addr, val);

	addr = (DISP_MUTEX_SOF + 0x20 * DISP_MUTEX_IDX);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex org 0x%x = 0x%x", addr, val);
	val &= (!(7 << 0));
	log_dbg("[wdma] mutex set 0x%x = 0x%x", addr, val);
	regmap_write(mutex_regmap, addr, val);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex new 0x%x = 0x%x", addr, val);

	addr = (DISP_MUTEX_EN + 0x20 * DISP_MUTEX_IDX);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex org 0x%x = 0x%x", addr, val);
	val |= (unsigned int)(1 << 0);
	log_dbg("[wdma] mutex set 0x%x = 0x%x", addr, val);
	regmap_write(mutex_regmap, addr, val);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex new 0x%x = 0x%x", addr, val);
}

static void mtk_wdma_mutex_disable(struct regmap *mutex_regmap)
{
	unsigned int addr;
	unsigned int val;

	addr = (DISP_MUTEX_MOD + 0x20 * DISP_MUTEX_IDX);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex org 0x%x = 0x%x", addr, val);
	val &= (unsigned int)(!((1 << 30) | (1 << 31)));
	log_dbg("[wdma] mutex set 0x%x = 0x%x", addr, val);
	regmap_write(mutex_regmap, addr, val);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);

	addr = (DISP_MUTEX_SOF + 0x20 * DISP_MUTEX_IDX);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex org 0x%x = 0x%x", addr, val);
	val &= (!(7 << 0));
	log_dbg("[wdma] mutex set 0x%x = 0x%x", addr, val);
	regmap_write(mutex_regmap, addr, val);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);

	addr = (DISP_MUTEX_EN + 0x20 * DISP_MUTEX_IDX);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mutex org 0x%x = 0x%x", addr, val);
	val &= (unsigned int)(!(1 << 0));
	log_dbg("[wdma] mutex set 0x%x = 0x%x", addr, val);
	regmap_write(mutex_regmap, addr, val);
	regmap_read(mutex_regmap, addr, &val);
	log_dbg("[wdma] mmsys new 0x%x = 0x%x", addr, val);
}

static int mtk_wdma_param_store(struct mtk_wdma_ctx *ctx, struct vb2_buffer *vb)
{
	#if LIST_OLD
	unsigned long flags;
	#endif

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	#if LIST_OLD

	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	if (list_empty(&ctx->buf_queue_list)) {
		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
		log_dbg("[wdma] [err] %s[%d]", __func__, __LINE__);
		return NULL;
	}
	ctx->curr_buf = list_first_entry(
			&ctx->buf_queue_list,
			struct mtk_wdma_buffer,
			list);
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
	if (ctx->curr_buf == NULL) {
		log_err("[wdma] [err] %s[%d] get next buf NULL",
			__func__, __LINE__);
		return RET_ERR_EXCEPTION;
	}
	/* INIT_LIST_HEAD(&ctx->curr_buf->list); */
	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	list_add_tail(&ctx->curr_buf->list, &ctx->buf_queue_list);
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	#else

	ctx->curr_vb2_v4l2_buffer = to_vb2_v4l2_buffer(vb);
	log_dbg("[wdma] %s[%d] get curr_vb2_v4l2_buffer 0x%lx 0x%lx",
	__func__, __LINE__,
	(unsigned long)(ctx->curr_vb2_v4l2_buffer), (unsigned long)vb);

	#endif

	/*lq-check*/
	_mtk_wdma_hw_param.in_format = MTK_WDMA_HW_FORMAT_RGB888;
	_mtk_wdma_hw_param.out_format =
		mtk_wdma_fmt_convert(ctx->s_frame.fmt->pixelformat);
	_mtk_wdma_hw_param.src_width = ctx->s_frame.f_width;
	_mtk_wdma_hw_param.src_height = ctx->s_frame.f_height;
	_mtk_wdma_hw_param.clip_x = 0;
	_mtk_wdma_hw_param.clip_y = 0;
	_mtk_wdma_hw_param.clip_width = ctx->s_frame.f_width; /*lq-check*/
	_mtk_wdma_hw_param.clip_height = ctx->s_frame.f_height; /*lq-check*/
	_mtk_wdma_hw_param.addr_1st_plane =
		vb2_dma_contig_plane_dma_addr(vb, 0);
	_mtk_wdma_hw_param.addr_2nd_plane = 0;
	_mtk_wdma_hw_param.addr_3rd_plane = 0;
	_mtk_wdma_hw_param.use_specified_alpha = 0; /*lq-check*/
	_mtk_wdma_hw_param.alpha = 0; /*lq-check*/

	log_dbg("[wdma] store hw param :");
	log_dbg("[wdma] fmt[%d, %d]",
		_mtk_wdma_hw_param.in_format,
		_mtk_wdma_hw_param.out_format);
	log_dbg("[wdma] wh[%d, %d]",
		_mtk_wdma_hw_param.src_width,
		_mtk_wdma_hw_param.src_height);
	log_dbg("[wdma] clip[%d, %d, %d, %d]",
		_mtk_wdma_hw_param.clip_x,
		_mtk_wdma_hw_param.clip_y,
		_mtk_wdma_hw_param.clip_width,
		_mtk_wdma_hw_param.clip_height);
	log_dbg("[wdma] addr[0x%lx, 0x%lx, 0x%lx]",
		_mtk_wdma_hw_param.addr_1st_plane,
		_mtk_wdma_hw_param.addr_2nd_plane,
		_mtk_wdma_hw_param.addr_3rd_plane);
	log_dbg("[wdma] alpha[%d, %d]",
		_mtk_wdma_hw_param.use_specified_alpha,
		_mtk_wdma_hw_param.alpha);

	log_dbg("[wdma] %s[%d] end", __func__, __LINE__);

	return RET_OK;
}

static int mtk_wdma_prepare_hw(struct mtk_wdma_ctx *ctx)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	mtk_wdma_clock_on(ctx->wdma_dev);

	ret = mtk_wdma_hw_set(
		ctx->wdma_dev->reg_base[0], &_mtk_wdma_hw_param);

	mtk_wdma_mmsys_path_connext(ctx->wdma_dev->mmsys_regmap);
	mtk_wdma_mutex_enable(ctx->wdma_dev->mutex_regmap);

	log_dbg("[wdma] %s[%d] end", __func__, __LINE__);

	return ret;
}

static int mtk_wdma_unprepare_hw(struct mtk_wdma_ctx *ctx)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	mtk_wdma_mmsys_path_disconnext(ctx->wdma_dev->mmsys_regmap);
	mtk_wdma_mutex_disable(ctx->wdma_dev->mutex_regmap);

	mtk_wdma_hw_unset(ctx->wdma_dev->reg_base[0]);
	mtk_wdma_clock_off(ctx->wdma_dev);

	#if LIST_OLD
	vb2_buffer_done(&ctx->curr_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	#else
	vb2_buffer_done(
		&ctx->curr_vb2_v4l2_buffer->vb2_buf, VB2_BUF_STATE_DONE);
	#endif

	#if LIST_OLD
	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	list_del(&ctx->curr_buf->list);
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
	#else
	/* spin_lock_irqsave(&ctx->vb2_q.lock, flags); */ /*lq-check*/
	list_del(&ctx->vb2_q.queued_list);
	/* spin_unlock_irqrestore(&ctx->vb2_q.lock, flags); */ /*lq-check*/
	#endif

	log_dbg("[wdma] %s[%d] end", __func__, __LINE__);

	return ret;
}

static const struct mtk_wdma_fmt *mtk_wdma_get_format(int index)
{
	if (index >= ARRAY_SIZE(mtk_wdma_formats))
		return NULL;

	return (struct mtk_wdma_fmt *)&mtk_wdma_formats[index];
}

static const struct mtk_wdma_fmt *mtk_wdma_find_fmt(
	u32 *pixelformat, u32 *mbus_code, u32 index)
{
	const struct mtk_wdma_fmt *fmt, *def_fmt = NULL;
	unsigned int i;

	if (index >= ARRAY_SIZE(mtk_wdma_formats))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_wdma_formats); i++) {
		fmt = mtk_wdma_get_format(i);
		if ((pixelformat != NULL) && (fmt->pixelformat == *pixelformat))
			return fmt;
	}

	return def_fmt;
}

#if 0
static void mtk_wdma_bound_align_image(
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

static void mtk_wdma_set_frame_size(
	struct mtk_wdma_frame *frame, int width, int height)
{
	frame->f_width = width;
	frame->f_height = height;
	frame->crop.width = width;
	frame->crop.height = height;
	frame->crop.left = 0;
	frame->crop.top = 0;
}

/*
 * mtk_wdma_queue_setup()
 * This function allocates memory for the buffers
 */
static int mtk_wdma_queue_setup(
	struct vb2_queue *vq,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	struct mtk_wdma_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_wdma_frame *frame = &ctx->s_frame;
	int i;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (!frame->fmt)
		return -EINVAL;

	*num_planes = frame->fmt->num_planes;
	for (i = 0; i < frame->fmt->num_planes; i++) {
		sizes[i] = frame->payload[i];
		#if SUPPORT_ALLOC_CTX
		alloc_devs[i] = ctx->wdma_dev->alloc_ctx;
		#endif
	}

	return 0;
}

#if LIST_OLD
static int mtk_wdma_buffer_init(struct vb2_buffer *vb)
{
	struct mtk_wdma_buffer *buf =
		container_of(vb, struct mtk_wdma_buffer, vb);

	INIT_LIST_HEAD(&buf->list);
	return 0;
}
#endif

/*
 * mtk_wdma_buf_prepare()
 * This is the callback function called from vb2_qbuf() function
 * the buffer is prepared and user space virtual address is converted into
 * physical address
 */
static int mtk_wdma_buf_prepare(struct vb2_buffer *vb)
{
	int ret = RET_OK;
	struct mtk_wdma_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_wdma_frame *frame = &ctx->s_frame;
	int i;
	unsigned long addr;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	for (i = 0; i < frame->fmt->num_planes; i++)
		vb2_set_plane_payload(vb, i, frame->payload[i]);

	addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	if (!IS_ALIGNED(addr, 8)) {
		log_err("[wdma] addr[0x%lx] is not 8 align", addr);
		return RET_ERR_EXCEPTION;
	}

	mtk_wdma_param_store(ctx, vb);

	return ret;
}

static void mtk_wdma_buf_queue(struct vb2_buffer *vb)
{
	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);
}

static void mtk_wdma_ctx_lock(struct vb2_queue *vq)
{
	struct mtk_wdma_ctx *ctx = vb2_get_drv_priv(vq);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	mutex_lock(&ctx->qlock);
}

static void mtk_wdma_ctx_unlock(struct vb2_queue *vq)
{
	struct mtk_wdma_ctx *ctx = vb2_get_drv_priv(vq);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	mutex_unlock(&ctx->qlock);
}

static int mtk_wdma_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_wdma_ctx *ctx = q->drv_priv;
	int ret;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	ret = pm_runtime_get_sync(&ctx->wdma_dev->pdev->dev);
	return ret > 0 ? 0 : ret;
}

static void mtk_wdma_stop_streaming(struct vb2_queue *q)
{
	struct mtk_wdma_ctx *ctx = q->drv_priv;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (!vb2_is_streaming(q))
		return;

	/* release all active buffers */
	#if LIST_OLD
	spin_lock_irqsave(&ctx->buf_queue_lock, flags);
	while (!list_empty(&ctx->buf_queue_list)) {
		ctx->curr_buf = list_first_entry(
				&ctx->buf_queue_list,
				struct mtk_wdma_buffer,
				list);
		list_del(&ctx->curr_buf->list);
		vb2_buffer_done(
			&ctx->curr_buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
	#endif

	pm_runtime_put_sync(&ctx->wdma_dev->pdev->dev);
}

static struct vb2_ops mtk_wdma_vb2_qops = {
	.queue_setup     = mtk_wdma_queue_setup,
	#if LIST_OLD
	.buf_init        = mtk_wdma_buffer_init,
	#endif
	.buf_prepare     = mtk_wdma_buf_prepare,
	.buf_queue       = mtk_wdma_buf_queue,
	.wait_prepare    = mtk_wdma_ctx_unlock,
	.wait_finish     = mtk_wdma_ctx_lock,
	.stop_streaming  = mtk_wdma_stop_streaming,
	.start_streaming = mtk_wdma_start_streaming,
};

static int mtk_wdma_querycap(
	struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (cap == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	strlcpy(cap->driver, MTK_WDMA_MODULE_NAME, sizeof(cap->driver));
	strlcpy(cap->card, ctx->wdma_dev->pdev->name, sizeof(cap->card));
	strlcpy(cap->bus_info, "platform", sizeof(cap->bus_info));
	cap->device_caps =  V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	log_dbg("[wdma] %s end, driver[%s], card[%s], bus_info[%s], caps[0x%x, 0x%x]",
		__func__,
		cap->driver, cap->card, cap->bus_info,
		cap->device_caps, cap->capabilities);

	return RET_OK;
}

static int mtk_wdma_enum_fmt_mplane(
	struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const struct mtk_wdma_fmt *fmt;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (f == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[wdma] %s start, index[%d]", __func__, f->index);

	fmt = mtk_wdma_find_fmt(NULL, NULL, f->index);
	if (!fmt)
		return -EINVAL;

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->pixelformat;

	log_dbg("[wdma] %s end, description = %s, pixelformat = %d",
		__func__, f->description, f->pixelformat);

	return RET_OK;
}

static int mtk_wdma_g_fmt_mplane(
	struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);
	struct mtk_wdma_frame *frame = &(ctx->s_frame);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	int i;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (f == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
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
		pix_mp->plane_fmt[i].sizeimage    =
			pix_mp->plane_fmt[i].bytesperline * frame->f_height;
	}

	log_dbg("[wdma] %s end, WH[%d, %d], fld[%d], pixfmt[%d], col[%d], np[%d], p[%d, %d][%d, %d]",
		__func__,
		pix_mp->width, pix_mp->height,
		pix_mp->field, pix_mp->pixelformat,
		pix_mp->colorspace, pix_mp->num_planes,
		pix_mp->plane_fmt[0].bytesperline,
		pix_mp->plane_fmt[0].sizeimage,
		pix_mp->plane_fmt[1].bytesperline,
		pix_mp->plane_fmt[1].sizeimage);

	return RET_OK;
}

static int mtk_wdma_try_fmt_mplane(
	struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp;
	const struct mtk_wdma_fmt *fmt;
	#if 0
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);
	struct mtk_wdma_variant *variant = ctx->variant;
	u32 max_w, max_h, mod_x, mod_y;
	u32 min_w, min_h, tmp_w, tmp_h;
	#endif
	int i;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (f == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	pix_mp = &f->fmt.pix_mp;

	log_dbg("[wdma] %s start, pixfmt[0x%x], field[%d], WH[%d, %d]",
		__func__,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.field,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height);

	fmt = mtk_wdma_find_fmt(&pix_mp->pixelformat, NULL, 0);
	if (!fmt) {
		log_err("[wdma] %s, pixelformat 0x%x invalid",
			__func__, pix_mp->pixelformat);
		return RET_ERR_EXCEPTION;
	}

	#if 0
	max_w = variant->pix_max->target_w;
	max_h = variant->pix_max->target_h;

	mod_x = ffs(variant->pix_align->org_w) - 1;
	if (FMT_IS_420(fmt->color))
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

	mtk_wdma_bound_align_image(
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

	log_dbg("[wdma] %s end, WH[%d, %d], np[%d], col[%d], p[%d, %d][%d, %d]",
		__func__,
		pix_mp->width, pix_mp->height,
		pix_mp->num_planes, pix_mp->colorspace,
		pix_mp->plane_fmt[0].bytesperline,
		pix_mp->plane_fmt[0].sizeimage,
		pix_mp->plane_fmt[1].bytesperline,
		pix_mp->plane_fmt[1].sizeimage);

	return RET_OK;
}

static int mtk_wdma_s_fmt_mplane(
	struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);
	struct mtk_wdma_frame *frame = &ctx->s_frame;
	struct v4l2_pix_format_mplane *pix;
	int i, ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (f == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	pix = &f->fmt.pix_mp;

	log_dbg("[wdma] %s[%d] start, wh[%d, %d] fmt[0x%x, 0x%x] flag[0x%x] plane[%d, %d, %d]",
		__func__, __LINE__,
		pix->width,
		pix->height,
		pix->pixelformat,
		pix->colorspace,
		pix->flags,
		pix->num_planes,
		pix->plane_fmt[0].bytesperline,
		pix->plane_fmt[0].sizeimage);

	ret = mtk_wdma_try_fmt_mplane(file, fh, f);
	if (ret)
		return ret;

	frame->fmt = mtk_wdma_find_fmt(&pix->pixelformat, NULL, 0);
	frame->colorspace = pix->colorspace;
	if (!frame->fmt)
		return -EINVAL;

	for (i = 0; i < frame->fmt->num_planes; i++) {
		frame->payload[i] = pix->plane_fmt[i].sizeimage;
		frame->pitch[i] = pix->plane_fmt[i].bytesperline;
	}

	mtk_wdma_set_frame_size(frame, pix->width, pix->height);

	log_dbg("[wdma] %s end, WH[%d, %d], col[%d], p[%d, %ld][%d, %ld]",
		__func__,
		pix->width, pix->height,
		frame->colorspace,
		frame->pitch[0], frame->payload[0],
		frame->pitch[1], frame->payload[1]);

	return RET_OK;
}

static int mtk_wdma_reqbufs(
	struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs)
{
	int ret = RET_OK;
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);
	struct video_device *vfd = video_devdata(file);

	if (reqbufs == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[wdma] %s start, cnt[%d] mem[%d] type[%d]",
		__func__, reqbufs->count, reqbufs->memory, reqbufs->type);

	vfd->queue = &ctx->vb2_q;
	log_dbg("[wdma] save vb2_q [0x%lx, 0x%lx, 0x%lx]",
		(unsigned long)(&(ctx->vb2_q)),
		(unsigned long)vfd,
		(unsigned long)(vfd->queue));

	ret = vb2_ioctl_reqbufs(file, fh, reqbufs);

	log_dbg("[wdma] %s end, ret[%d]",
		__func__, ret);

	return ret;
}

static int mtk_wdma_expbuf(
	struct file *file, void *fh, struct v4l2_exportbuffer *eb)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (eb == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	ret = vb2_ioctl_expbuf(file, fh, eb);

	log_dbg("[wdma] %s end, ret[%d], fd[%d], flag[%d], idx[%d], plane[%d]",
		__func__, ret, eb->fd, eb->flags, eb->index, eb->plane);

	return ret;
}

static int mtk_wdma_querybuf(
	struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (buf == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[wdma] %s start, idx[%d], memtype[%d], np[%d]",
		__func__, buf->index, buf->memory, buf->length);

	ret = vb2_ioctl_querybuf(file, fh, buf);

	log_dbg("[wdma] %s end, ret[%d], length[%d, %d], bytes[%d, %d]",
		__func__, ret,
		buf->m.planes[0].length, buf->m.planes[1].length,
		buf->m.planes[0].bytesused, buf->m.planes[1].bytesused);

	return ret;
}

static int mtk_wdma_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = RET_OK;
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);

	if (buf == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	log_dbg("[wdma] %s[%d] start, idx[%d], size[%d, %d], type[%d, %d], plane[%d, %d, 0x%lx], time[%ld, %ld]",
		__func__, __LINE__,
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

	init_waitqueue_head(&_EventWqIrqHandle);
	ret = mtk_ovl_prepare_hw();
	ret = mtk_wdma_prepare_hw(ctx);
	ret = mtk_wdma_wait_irq(ctx->wdma_dev->reg_base[0]);
	ret = mtk_wdma_unprepare_hw(ctx);
	ret = mtk_ovl_unprepare_hw();

	log_dbg("[wdma] %s end, ret[%d]", __func__, ret);

	return ret;
}

static int mtk_wdma_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (buf == NULL) {
		log_err("[wdma] %s, param is NULL", __func__);
		return RET_ERR_PARAM;
	}

	ret = vb2_ioctl_dqbuf(file, fh, buf);

	log_dbg("[wdma] %s end, ret[%d]", __func__, ret);

	return ret;
}

static int mtk_wdma_streamon(
	struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	ret = vb2_ioctl_streamon(file, fh, type);

	log_dbg("[wdma] %s end, ret[%d]", __func__, ret);

	return ret;
}

static int mtk_wdma_streamoff(
	struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret = RET_OK;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	ret = vb2_ioctl_streamoff(file, fh, type);

	log_dbg("[wdma] %s end, ret[%d]", __func__, ret);

	return ret;
}

static int mtk_wdma_g_selection(
	struct file *file, void *fh, struct v4l2_selection *s)
{
	int ret = RET_OK;
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);
	struct mtk_wdma_frame *frame = &(ctx->s_frame);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

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
		log_err("[wdma] %s, unsupport param %d", __func__, s->target);
		ret = RET_ERR_PARAM;
		break;
	}

	log_dbg("[wdma] %s end, ret[%d], target[%d], [%d, %d, %d, %d]",
		__func__, ret,
		s->target, s->r.left, s->r.top, s->r.width, s->r.height);

	return ret;
}

static int mtk_wdma_s_selection(
	struct file *file, void *fh, struct v4l2_selection *s)
{
	int ret = RET_OK;
	struct mtk_wdma_ctx *ctx = fh_to_ctx(fh);
	struct mtk_wdma_frame *frame = &(ctx->s_frame);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

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
		ret = RET_ERR_PARAM;
		break;
	}

	log_dbg("[wdma] %s end, target[%d], [%d, %d], [%d, %d, %d, %d]",
		__func__,
		s->target,
		frame->f_width, frame->f_height,
		frame->crop.left, frame->crop.top,
		frame->crop.width, frame->crop.height);

	return ret;
}

static const struct v4l2_ioctl_ops mtk_wdma_ioctl_ops = {
	.vidioc_querycap                = mtk_wdma_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = mtk_wdma_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = mtk_wdma_g_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane  = mtk_wdma_try_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_wdma_s_fmt_mplane,
	.vidioc_reqbufs                 = mtk_wdma_reqbufs,
	.vidioc_expbuf                  = mtk_wdma_expbuf,
	.vidioc_querybuf                = mtk_wdma_querybuf,
	.vidioc_qbuf                    = mtk_wdma_qbuf,
	.vidioc_dqbuf                   = mtk_wdma_dqbuf,
	.vidioc_streamon                = mtk_wdma_streamon,
	.vidioc_streamoff               = mtk_wdma_streamoff,
	.vidioc_g_selection             = mtk_wdma_g_selection,
	.vidioc_s_selection             = mtk_wdma_s_selection
};

static int mtk_wdma_open(struct file *file)
{
	int ret;
	struct mtk_wdma_ctx *ctx = NULL;
	struct video_device *vfd = video_devdata(file);
	struct mtk_wdma_dev *wdma_dev = video_drvdata(file);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (mutex_lock_interruptible(&wdma_dev->lock))
		return -ERESTARTSYS;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		ret = -ENOMEM;

	mutex_init(&ctx->qlock);
	mutex_init(&ctx->slock);
	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;

	ctx->wdma_dev = wdma_dev;
	ctx->s_frame.fmt = mtk_wdma_get_format(0);
	ctx->flags = 0;

	memset(&(ctx->vb2_q), 0, sizeof(ctx->vb2_q));
	ctx->vb2_q.dev = &(wdma_dev->pdev->dev);
	ctx->vb2_q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; /*lq-check*/
	ctx->vb2_q.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vb2_q.drv_priv = ctx;
	ctx->vb2_q.ops = &mtk_wdma_vb2_qops;
	ctx->vb2_q.mem_ops = &vb2_dma_contig_memops;
	ctx->vb2_q.buf_struct_size = sizeof(struct mtk_wdma_buffer);
	ctx->vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&(ctx->vb2_q));

	INIT_LIST_HEAD(&ctx->buf_queue_list);

	ctx->variant = &mtk_wdma_default_variant;

	mutex_unlock(&wdma_dev->lock);

	log_dbg("[wdma] %s[%d] end", __func__, __LINE__);

	return ret;
}

static int mtk_wdma_release(struct file *file)
{
	struct mtk_wdma_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_wdma_dev *wdma_dev = ctx->wdma_dev;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	flush_workqueue(wdma_dev->workqueue);
	mutex_lock(&wdma_dev->lock);
	mutex_destroy(&ctx->qlock);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	mutex_unlock(&wdma_dev->lock);

	log_dbg("[wdma] %s[%d] end", __func__, __LINE__);

	return 0;
}

static unsigned int mtk_wdma_poll(
	struct file *file, struct poll_table_struct *wait)
{
	int ret;
	struct mtk_wdma_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_wdma_dev *wdma_dev = ctx->wdma_dev;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (mutex_lock_interruptible(&wdma_dev->lock))
		return -ERESTARTSYS;

	ret = vb2_fop_poll(file, wait);

	mutex_unlock(&wdma_dev->lock);

	return ret;
}

static int mtk_wdma_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct mtk_wdma_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_wdma_dev *wdma_dev = ctx->wdma_dev;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (mutex_lock_interruptible(&wdma_dev->lock))
		return -ERESTARTSYS;

	ret = vb2_fop_mmap(file, vma);
	if (ret)
		log_err("[wdma] fail to mmap, ret[%d]", ret);

	mutex_unlock(&wdma_dev->lock);

	return ret;
}

static const struct v4l2_file_operations mtk_wdma_fops = {
	.owner		= THIS_MODULE,
	.open		= mtk_wdma_open,
	.release	= mtk_wdma_release,
	.poll		= mtk_wdma_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= mtk_wdma_mmap,
};

static void mtk_wdma_probe_free(
	struct mtk_wdma_dev *wdma_dev, enum MTK_WDMA_PROBE_STATUS st)
{
	int i;
	struct resource *res;
	struct platform_device *pdev = wdma_dev->pdev;
	struct device *dev = &pdev->dev;

	if (st >= PROBE_ST_PM_ENABLE)
		pm_runtime_disable(dev);

	#if SUPPORT_ALLOC_CTX
	if (st >= PROBE_ST_DMA_CONTIG_INIT)
		vb2_dma_contig_cleanup_ctx(wdma_dev->alloc_ctx);
	#endif

	if (st >= PROBE_ST_DMA_PARAM_SET) {
		kfree(dev->dma_parms);
		dev->dma_parms = NULL;
	}

	if (st >= PROBE_ST_VIDEO_DEV_REG)
		video_unregister_device(&wdma_dev->vdev);

	if (st >= PROBE_ST_V4L2_DEV_REG)
		v4l2_device_unregister(&wdma_dev->v4l2_dev);

	if (st >= PROBE_ST_REG_MAP) {
		for (i = 0; i < MTK_WDMA_REG_BASE_COUNT; i++) {
			devm_iounmap(
				dev, (void __iomem *)(wdma_dev->reg_base[i]));
		}
	}

	if (st >= PROBE_ST_IRQ_REQ) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		if (!res)
			log_dbg("fail to get irq resource\n");
		else
			devm_free_irq(dev, res->start, wdma_dev);
	}

	if (st >= PROBE_ST_WORKQUEUE_CREATE) {
		flush_workqueue(wdma_dev->workqueue);
		destroy_workqueue(wdma_dev->workqueue);
	}

	if (st >= PROBE_ST_CLK_GET) {
		for (i = 0; i < MTK_WDMA_CLK_CNT; i++)
			devm_clk_put(dev, wdma_dev->clks[i]);
	}

	#if SUPPORT_PROBE_CLOCK_ON
	if (st >= PROBE_ST_CLK_ON)
		mtk_wdma_clock_off(wdma_dev);
	#endif

	#if SUPPORT_IOMMU_ATTACH
	if (st >= PROBE_ST_IOMMU_ATTACH)
		arm_iommu_detach_device(dev);
	#endif

	/* if (st >= PROBE_ST_DEV_LARB) */

	if (st >= PROBE_ST_MUTEX_INIT)
		mutex_destroy(&wdma_dev->lock);

	if (st >= PROBE_ST_DEV_ALLOC)
		devm_kfree(dev, wdma_dev);
}

static int mtk_wdma_register_video_device(struct mtk_wdma_dev *wdma_dev)
{
	int ret = RET_OK;
	struct device *dev = &wdma_dev->pdev->dev;

	wdma_dev->vdev.fops = &mtk_wdma_fops;
	wdma_dev->vdev.lock = &wdma_dev->lock;
	wdma_dev->vdev.vfl_dir = VFL_DIR_RX;
	wdma_dev->vdev.release = video_device_release_empty;
	wdma_dev->vdev.v4l2_dev = &wdma_dev->v4l2_dev;
	wdma_dev->vdev.ioctl_ops = &mtk_wdma_ioctl_ops;
	snprintf(wdma_dev->vdev.name, sizeof(wdma_dev->vdev.name),
		"%s", MTK_WDMA_MODULE_NAME);
	video_set_drvdata(&wdma_dev->vdev, wdma_dev);

	v4l2_disable_ioctl_locking(&wdma_dev->vdev, VIDIOC_QBUF);
	v4l2_disable_ioctl_locking(&wdma_dev->vdev, VIDIOC_DQBUF);
	v4l2_disable_ioctl_locking(&wdma_dev->vdev, VIDIOC_S_CTRL);

	ret = video_register_device(&wdma_dev->vdev, VFL_TYPE_GRABBER, 2);
	if (ret) {
		dev_err(dev, "fail to register video device\n");
		ret = RET_ERR_EXCEPTION;
	} else
		log_dbg("ok to register driver as /dev/video%d\n",
			wdma_dev->vdev.num);

	return ret;
}

static int mtk_wdma_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct mtk_wdma_dev *wdma_dev;
	struct device_node *node;
	enum MTK_WDMA_PROBE_STATUS probe_st;
	struct iommu_domain *iommu;
	struct platform_device *larb_pdev;

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	iommu = iommu_get_domain_for_dev(dev);
	if (!iommu) {
		log_dbg("Waiting iommu driver ready\n");
		return -EPROBE_DEFER;
	}

	wdma_dev = devm_kzalloc(dev, sizeof(struct mtk_wdma_dev), GFP_KERNEL);
	if (!wdma_dev)
		return -ENOMEM;
	probe_st = PROBE_ST_DEV_ALLOC;

	platform_set_drvdata(pdev, wdma_dev);

	wdma_dev->pdev = pdev;

	init_waitqueue_head(&wdma_dev->irq_queue); /*lq-check*/

	mutex_init(&wdma_dev->lock);
	probe_st = PROBE_ST_MUTEX_INIT;

	wdma_dev->larb_dev = NULL;
	node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
	if (!node) {
		dev_err(&pdev->dev, "fail to get larb node\n");
		ret = -EINVAL;
		goto err_handle;
	}
	larb_pdev = of_find_device_by_node(node);
	if (!larb_pdev) {
		log_dbg("defer to get larb device\n");
		of_node_put(node);
		ret = -EPROBE_DEFER;
		goto err_handle;
	}
	of_node_put(node);
	wdma_dev->larb_dev = &larb_pdev->dev;
	log_dbg("ok to get larb device 0x%lx",
		(unsigned long)(wdma_dev->larb_dev));
	probe_st = PROBE_ST_DEV_LARB;

	#if SUPPORT_IOMMU_ATTACH
	/* start to attach iommu */
	node = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!node) {
		log_dbg("fail to parse for iommus\n");
		ret = -ENXIO;
		goto err_handle;
	}
	pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (WARN_ON(!pdev)) {
		log_dbg("fail to find dev for iommus\n");
		ret = -ENXIO;
		goto err_handle;
	}
	arm_iommu_attach_device(dev, pdev->dev.archdata.iommu);
	probe_st = PROBE_ST_IOMMU_ATTACH;
	#endif

	for (i = 0; i < MTK_WDMA_CLK_CNT; i++) {
		wdma_dev->clks[i] = devm_clk_get(dev, _ap_mtk_wdma_clk_name[i]);
		if (IS_ERR(wdma_dev->clks[i])) {
			dev_err(dev, "fail to get clk %s\n",
				_ap_mtk_wdma_clk_name[i]);
			ret = -ENXIO;
			for (i -= 1; i >= 0; i--)
				devm_clk_put(dev, wdma_dev->clks[i]);
			goto err_handle;
		} else
			log_dbg("ok to get dev[0x%lx] clk[%s] as 0x%lx\n",
				(unsigned long)dev,
				_ap_mtk_wdma_clk_name[i],
				(unsigned long)(wdma_dev->clks[i]));
	}
	probe_st = PROBE_ST_CLK_GET;

	#if SUPPORT_PROBE_CLOCK_ON
	ret = mtk_wdma_clock_on(wdma_dev);
	if (ret)
		goto err_handle;
	probe_st = PROBE_ST_CLK_ON;
	#endif

	/*lq-check*/
	wdma_dev->workqueue =
		create_singlethread_workqueue(MTK_WDMA_MODULE_NAME);
	if (!wdma_dev->workqueue) {
		dev_err(dev, "fail to alloc for workqueue\n");
		ret = -ENOMEM;
		goto err_handle;
	}
	probe_st = PROBE_ST_WORKQUEUE_CREATE;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "fail to get irq resource\n");
		ret = -ENXIO;
		goto err_handle;
	}
	ret = devm_request_irq(
		dev, res->start, mtk_wdma_irq_handler, 0, pdev->name, wdma_dev);
	if (ret) {
		dev_err(dev, "fail to request irq, ret[%d]\n", ret);
		ret = -ENXIO;
		goto err_handle;
	}
	probe_st = PROBE_ST_IRQ_REQ;

	for (i = 0; i < MTK_WDMA_REG_BASE_COUNT; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			dev_err(dev, "fail to get memory resource\n");
			ret = -ENXIO;
			goto err_handle;
		}

		wdma_dev->reg_base[i] =
			(void *)devm_ioremap_resource(dev, res);
		if (IS_ERR((void *)(wdma_dev->reg_base[i]))) {
			dev_err(dev, "fail to do reg[%d] ioremap\n", i);
			ret = PTR_ERR(wdma_dev->reg_base);
			for (i -= 1; i >= 0; i--)
				devm_iounmap(dev,
					(void __iomem *)
						(wdma_dev->reg_base[i]));
			goto err_handle;
		}

		log_dbg("ok to map reg[%d] base=0x%lx",
			i, (unsigned long)(wdma_dev->reg_base[i]));
	}
	probe_st = PROBE_ST_REG_MAP;

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,mmsys-regmap", 0);
	if (node) {
		log_dbg("ok to get mmsys-regmap node 0x%lx",
			(unsigned long)node);
		wdma_dev->mmsys_regmap = syscon_node_to_regmap(node);
		if (IS_ERR(wdma_dev->mmsys_regmap)) {
			log_err("fail to get mmsys-regmap regmap 0x%lx",
				(unsigned long)wdma_dev->mmsys_regmap);
			ret = -EPROBE_DEFER;
			goto err_handle;
		}
		log_dbg("ok to get mmsys-regmap regmap 0x%lx",
			(unsigned long)wdma_dev->mmsys_regmap);
	} else {
		log_err("fail to get mmsys-regmap node 0x%lx",
			(unsigned long)node);
		dev_err(&pdev->dev,
			"wdma2 node has not [mediatek,mmsys-regmap]\n");
		ret = -EINVAL;
		goto err_handle;
	}
	probe_st = PROBE_ST_REG_MMSYS_MAP;

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,mutex-regmap", 0);
	if (node) {
		log_dbg("ok to get mutex-regmap node 0x%lx",
			(unsigned long)node);
		wdma_dev->mutex_regmap = syscon_node_to_regmap(node);
		if (IS_ERR(wdma_dev->mutex_regmap)) {
			log_err("fail to get mutex-regmap regmap 0x%lx",
				(unsigned long)wdma_dev->mutex_regmap);
			ret = -EPROBE_DEFER;
			goto err_handle;
		}
		log_dbg("ok to get mutex-regmap regmap 0x%lx",
			(unsigned long)wdma_dev->mutex_regmap);
	} else {
		log_err("fail to get mutex-regmap node 0x%lx",
			(unsigned long)node);
		dev_err(&pdev->dev,
			"wdma2 node has not [mediatek,mutex-regmap]\n");
		ret = -EINVAL;
		goto err_handle;
	}
	probe_st = PROBE_ST_REG_MUTEX_MAP;

	ret = v4l2_device_register(dev, &wdma_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "fail to register v4l2 dev\n");
		ret = -EINVAL;
		goto err_handle;
	}
	probe_st = PROBE_ST_V4L2_DEV_REG;

	ret = mtk_wdma_register_video_device(wdma_dev);
	if (ret) {
		log_dbg("fail to register video dev\n");
		ret = -EINVAL;
		goto err_handle;
	}
	probe_st = PROBE_ST_VIDEO_DEV_REG;

	/*
	 * if device has no max_seg_size set, we assume that there is no limit
	 * and force it to DMA_BIT_MASK(32) to always use contiguous mappings
	 * in DMA address space
	 */
	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms) {
			log_dbg("fail to alloc for dma_parms\n");
			ret = -ENOMEM;
			goto err_handle;
		}
	}
	if (dma_set_max_seg_size(dev, DMA_BIT_MASK(32)) != 0) {
		log_dbg("fail to set dma_set_max_seg_size\n");
		ret = -ENOMEM;
		goto err_handle;
	}
	probe_st = PROBE_ST_DMA_PARAM_SET;

	#if SUPPORT_ALLOC_CTX
	wdma_dev->alloc_ctx = vb2_dma_contig_init_ctx(dev);
	if (IS_ERR(wdma_dev->alloc_ctx)) {
		log_dbg("fail to do vb2_dma_contig_init_ctx\n");
		ret = PTR_ERR(wdma_dev->alloc_ctx);
		goto err_handle;
	}
	probe_st = PROBE_ST_DMA_CONTIG_INIT;
	#endif

	pm_runtime_enable(dev);
	probe_st = PROBE_ST_PM_ENABLE;

	log_dbg("ok to do wdma probe\n");
	return 0;

err_handle:
	mtk_wdma_probe_free(wdma_dev, probe_st);
	log_dbg("fail to do wdma probe\n");
	return ret;
}

static int mtk_wdma_remove(struct platform_device *pdev)
{
	struct mtk_wdma_dev *wdma_dev = platform_get_drvdata(pdev);

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	mtk_wdma_probe_free(wdma_dev, PROBE_ST_PM_ENABLE);

	log_dbg("%s driver remove ok\n", pdev->name);

	return 0;
}

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int mtk_wdma_pm_suspend(struct device *dev)
{
	#if SUPPORT_CLOCK_SUSPEND
	struct mtk_wdma_dev *wdma_dev = dev_get_drvdata(dev);
	#endif

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	#if SUPPORT_CLOCK_SUSPEND
	mtk_wdma_clock_off(wdma_dev);
	#endif

	return 0;
}

static int mtk_wdma_pm_resume(struct device *dev)
{
	#if SUPPORT_CLOCK_SUSPEND
	struct mtk_wdma_dev *wdma_dev = dev_get_drvdata(dev);
	#endif

	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	#if SUPPORT_CLOCK_SUSPEND
	return mtk_wdma_clock_on(wdma_dev);
	#endif

	return 0;
}
#endif /* CONFIG_PM_RUNTIME || CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_SLEEP
static int mtk_wdma_suspend(struct device *dev)
{
	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (pm_runtime_suspended(dev))
		return 0;

	#ifdef CONFIG_PM_RUNTIME
	return mtk_wdma_pm_suspend(dev);
	#endif

	return 0;
}

static int mtk_wdma_resume(struct device *dev)
{
	log_dbg("[wdma] %s[%d] start", __func__, __LINE__);

	if (pm_runtime_suspended(dev))
		return 0;

	#ifdef CONFIG_PM_RUNTIME
	return mtk_wdma_pm_resume(dev);
	#endif

	return 0;
}
#endif

static const struct dev_pm_ops mtk_wdma_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_wdma_suspend, mtk_wdma_resume)
	SET_RUNTIME_PM_OPS(mtk_wdma_pm_suspend, mtk_wdma_pm_resume, NULL)
};

static const struct of_device_id mtk_wdma_match[] = {
	{.compatible = "mediatek,mt2712-disp-wdma-2",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_wdma_match);

static struct platform_driver mtk_wdma_driver = {
	.probe  = mtk_wdma_probe,
	.remove	= mtk_wdma_remove,
	.driver	= {
		.name  = MTK_WDMA_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm    = &mtk_wdma_pm_ops,
		.of_match_table = mtk_wdma_match,
	},
};
module_platform_driver(mtk_wdma_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek wdma driver");
MODULE_AUTHOR("Qing Li <qing.li@mediatek.com>");

