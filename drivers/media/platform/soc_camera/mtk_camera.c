/*
 * V4L2 Driver for MTK camera host
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf-dma-contig.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/v4l2-of.h>

#include <linux/videodev2.h>

#define MTK_CAM_DRV_NAME "mtk-camera"
#define MAX_VIDEO_MEM 16

#define SOCAM_BUS_FLAGS	(V4L2_MBUS_MASTER | \
	V4L2_MBUS_HSYNC_ACTIVE_HIGH | V4L2_MBUS_VSYNC_ACTIVE_HIGH | \
	V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_PCLK_SAMPLE_FALLING | \
	V4L2_MBUS_DATA_ACTIVE_HIGH)

/* vdoin supported width of resolution*/
#define MTK_VDOIN_SUPPORTED_WIDTH_720P_STANDARD			0x2d0
#define MTK_VDOIN_SUPPORTED_WIDTH_720P				0x2e0
#define MTK_VDOIN_SUPPORTED_HEIGHT_4CHANNEL			0x3bf
#define MTK_VDOIN_SUPPORTED_HEIGHT_1CHANNEL			0xef
/*vdoin register offset*/
#define MTK_VDOIN_EN_REG					0x00
#define MTK_VDOIN_MODE_REG					0x04
#define MTK_VDOIN_YBUF_ADDR_REG					0x08
#define MTK_VDOIN_ACT_LINE_REG					0x0c
#define MTK_VDOIN_CBUF_ADDR_REG					0x10
#define MTK_VDOIN_DW_NEED_REG					0x14
#define MTK_VDOIN_HPIXEL_REG					0x18
#define MTK_VDOIN_HBLACK_REG					0x1c
#define MTK_VDOIN_INPUT_CTRL_REG				0x20
#define MTK_VDOIN_TOP_BOT_SLINE_REG				0x24
#define MTK_VDOIN_WRAP_3D_REG					0x28
#define MTK_VDOIN_WRAP_3D_VSYNC_REG				0x2c
#define MTK_VDOIN_WRAP_3D_SET_REG				0x30
#define MTK_VDOIN_HCNT_SET_REG					0x34
#define MTK_VDOIN_HCNT_SET1_REG					0x38
#define MTK_VDOIN_VSCALE_REG					0x3c
#define MTK_VDOIN_HSCALE_REG					0x40
#define MTK_VDOIN_INDEBUG_STATUS_REG				0x50
#define MTK_VDOIN_PIC_REG					0x54
#define MTK_VDOIN_DEBUG4_REG					0x58
#define MTK_VDOIN_DEBUG5_REG					0x5c

/* VDOIN  Reset Register */
#define MTK_VDOIN_SOFTWARE_RESET				0x138
#define MTK_VDOIN_SOFTWARE_RESET_REG				(1 << 10)
#define MTK_VDOIN_ENABLE_REG_BIT				0x1

/*cmd*/
#define MTK_VDOIN_MODE_CMD					0x30328408
/* Y ACTLINE [27:16] & U ACTLINE [11:0] 239 */
#define MTK_VDOIN_DW_NEED_CMD_1CHANNEL				0x00ef00ef
#define MTK_VDOIN_DW_NEED_CMD_4CHANNEL				0x03bf03bf
#define MTK_VDOIN_HPIXEL_CMD					0x0
#define MTK_VDOIN_HBLACK_CMD					0x280
/* miss 1 byte */
#define MTK_VDOIN_INPUT_CTRL_CMD				0x80142024
#define MTK_VDOIN_TOP_BOT_SLINE_CMD				0x0
#define MTK_VDOIN_WRAP_3D_CMD					0x00070007
#define MTK_VDOIN_WRAP_3D_VSYNC_CMD				0x00ef00ef
/* HACTCNT & HCNT 1440(720) (1440)*/
#define MTK_VDOIN_HCNT_SET_CMD_720				0x05a005a0
/* HACTCNT & HCNT 1472(736) (1472)*/
#define MTK_VDOIN_HCNT_SET_CMD_736				0x05c005c0
/* UV HACTCNT/16-1 & Y HACTCNT/16-1  44*/
#define MTK_VDOIN_HCNT_SET1_CMD_720				0x002c002c
/* UV HACTCNT/16-1 & Y HACTCNT/16-1  45*/
#define MTK_VDOIN_HCNT_SET1_CMD_736				0x002d002d
#define MTK_VDOIN_HSCALE_CMD					0x80808080
/* ACTLINE [11:0]  239*/
#define MTK_VDOIN_ACT_LINE_CMD_1CHANNEL				0x000040ef
#define MTK_VDOIN_ACT_LINE_CMD_4CHANNEL				0x000043bf
/* horizntal dram cunt [15:16] 45   [14]frame interrupt [5:7]0  [0:2]1*/
#define MTK_VDOIN_VSCALE_CMD_720				0x002d5007
/* horizntal dram cunt [15:16] 46   [14]frame interrupt [5:7]0  [0:2]1*/
#define MTK_VDOIN_VSCALE_CMD_736				0x002e5007
#define MTK_VDOIN_PIC_CMD					0x0
#define MTK_VDOIN_DEBUG4_CMD					0x00000100
#define MTK_VDOIN_DEBUG5_CMD					0x00001000
#define MTK_VDOIN_WRAP_3D_SET_CMD				0x0020a080
/* interlace setting 3 bit & no YC mix  ,  invert field*/
#define MTK_VDOIN_EN_CMD					0x22105C90

/* IRQ */
#define MTK_VDOIN_IRQ_CFG					(1<<6)
#define MTK_VDOIN_IRQ_CLR					(1<<6)
#define MTK_VDOIN_IRQ_CFG_REG					0x4
/*VDOIN DRAM CFG*/
#define MTK_VDOIN_DRAM_CAMERA_INPUT_CLOCK_PATH			0x38
#define MTK_VDOIN_DRAM_CLK_CFG					0x18
#define MTK_VDOIN_DRAM_DATA_CFG					0x2C
#define MTK_VDOIN_DRAM_CLK_REG					0x20
#define MTK_VDOIN_DRAM_DATA_REG					0x4
#define MTK_VDOIN_DRAM_MODE					0x60000000

struct mtk_camera_platform_data {
	unsigned long	mclk_khz;
	unsigned long	flags;
};

struct mtk_camera_dev {
	struct soc_camera_host	soc_host;
	/*
	 * mtk_camera is only supposed to handle one camera on its Quick Capture
	 * interface. If anyone ever builds hardware to enable more than
	 * one camera, they will have to modify this driver too
	 */
	struct clk		*clk;

	unsigned int		irq;

	void __iomem		*reg_vdoin_base;
	void __iomem		*reg_interrupt_base;
	void __iomem		*reg_caminputclk_base;

	struct mtk_camera_platform_data *pdata;
	unsigned long		mclk;
	unsigned long		pflags;
	u32			mclk_divisor;

	struct list_head	capture;

	spinlock_t		lock;

	struct mtk_camera_buf	*active;
	int			enable_vdoin;
};

static int mtk_camera_add_device(struct soc_camera_device *icd)
{
	return 0;
}

static void mtk_camera_remove_device(struct soc_camera_device *icd)
{

}

static int mtk_camera_querycap(struct soc_camera_host *ici,
			       struct v4l2_capability *cap)
{
	/* cap->name is set by the firendly caller:-> */
	strlcpy(cap->card, "MTK Camera", sizeof(cap->card));
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static int mtk_camera_clock_start(struct soc_camera_host *ici)
{
	struct mtk_camera_dev *pcdev = ici->priv;

	clk_enable(pcdev->clk);
	return 0;
}

static void mtk_camera_clock_stop(struct soc_camera_host *ici)
{
	struct mtk_camera_dev *pcdev = ici->priv;

	clk_disable(pcdev->clk);
}

static int mtk_camera_check_frame(u32 width, u32 height)
{
	/* limit to hardware capabilities */
	return height < 32 || height > 2048 || width < 48 || width > 2048 ||
		(width & 0x01);
}

static int mtk_camera_set_crop(struct soc_camera_device *icd,
			       const struct v4l2_crop *a)
{
	const struct v4l2_rect *rect = &a->c;
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mtk_camera_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_sense sense = {
		.master_clock = pcdev->mclk,
		.pixel_clock_max = pcdev->pdata->mclk_khz * 1000,
	};
	struct v4l2_mbus_framefmt mf;
	u32 fourcc = icd->current_fmt->host_fmt->fourcc;
	int ret;

	/* If PCLK is used to latch data from the sensor, check sense */
	icd->sense = &sense;
	ret = v4l2_subdev_call(sd, video, s_crop, a);
	icd->sense = NULL;

	if (ret < 0) {
		dev_warn(dev, "Failed to crop to %ux%u@%u:%u\n",
			 rect->width, rect->height, rect->left, rect->top);
		return ret;
	}

	ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	if (mtk_camera_check_frame(mf.width, mf.height)) {
		/*
		 * Camera cropping produced a frame beyond our capabilities.
		 * FIXME: just extract a subframe, that we can process.
		 */
		v4l_bound_align_image(&mf.width, 48, 2048, 1,
			&mf.height, 32, 2048, 0,
			fourcc == V4L2_PIX_FMT_YUV422P ? 4 : 0);
		ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
		if (ret < 0)
			return ret;

		if (mtk_camera_check_frame(mf.width, mf.height)) {
			dev_warn(icd->parent,
				 "Inconsistent state. Use S_FMT to repair\n");
			return -EINVAL;
		}
	}

	icd->user_width		= mf.width;
	icd->user_height	= mf.height;

	return ret;
}

static const struct soc_mbus_lookup mtk_camera_formats[] = {
	{
		.code = V4L2_MBUS_FMT_UYVY8_2X8,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_NV16,
			.name			= "YUYV",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_LE,
			.layout			= SOC_MBUS_LAYOUT_PLANAR_Y_C,
		},
	},
	{
		.code = V4L2_MBUS_FMT_VYUY8_2X8,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_YVYU,
			.name			= "YVYU",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = V4L2_MBUS_FMT_YUYV8_2X8,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_UYVY,
			.name			= "UYVY",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = V4L2_MBUS_FMT_YVYU8_2X8,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_VYUY,
			.name			= "VYUY",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = V4L2_MBUS_FMT_RGB555_2X8_PADHI_BE,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_RGB555,
			.name			= "RGB555",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_RGB555X,
			.name			= "RGB555X",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = V4L2_MBUS_FMT_RGB565_2X8_BE,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_RGB565,
			.name			= "RGB565",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = V4L2_MBUS_FMT_RGB565_2X8_LE,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_RGB565X,
			.name			= "RGB565X",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_BE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
};

static int mtk_camera_get_formats(struct soc_camera_device *icd,
		unsigned int idx, struct soc_camera_format_xlate *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->parent;
	int formats = 0, ret;
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt *fmt;

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret < 0)
		/* No more formats */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code);
	if (!fmt) {
		dev_warn(dev, "%s: unsupported format code #%d: %d\n", __func__,
				idx, code);
		return 0;
	}

	/* Check support for the requested bits-per-sample */
	if (fmt->bits_per_sample != 8)
		return 0;

	switch (code) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_RGB565_2X8_BE:
	case V4L2_MBUS_FMT_RGB565_2X8_LE:
		formats++;
		if (xlate) {
			xlate->host_fmt	= soc_mbus_find_fmtdesc(code,
						mtk_camera_formats,
						ARRAY_SIZE(mtk_camera_formats));
			xlate->code	= code;

			dev_dbg(dev,
				"%s: providing format %s as byte swapped code #%d\n",
				__func__, xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	default:
		if (xlate)
			dev_dbg(dev,
				"%s: providing format %s in pass-through mode\n",
				__func__, fmt->name);
	}
	return formats;
}

static int mtk_camera_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int ret;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->parent, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	/*
	 * Limit to hardware capabilities.  YUV422P planar format requires
	 * images size to be a multiple of 16 bytes.  If not, zeros will be
	 * inserted between Y and U planes, and U and V planes, which violates
	 * the YUV422P standard.
	 */
	v4l_bound_align_image(&pix->width, 48, 2048, 1,
			      &pix->height, 32, 2048, 0,
			      pixfmt == V4L2_PIX_FMT_YUV422P ? 4 : 0);

	/* limit to sensor capabilities */
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	pix->width	= mf.width;
	pix->height	= mf.height;
	pix->field	= mf.field;
	pix->colorspace	= mf.colorspace;

	return ret;
}

static int mtk_camera_set_fmt(struct soc_camera_device *icd,
				struct v4l2_format *f)
{
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mtk_camera_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate = NULL;
	struct soc_camera_sense sense = {
		.master_clock = pcdev->mclk,
		.pixel_clock_max = pcdev->pdata->mclk_khz * 1000,
	};
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret;

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_err(dev, "Format %x not found\n", pix->pixelformat);
		return -EINVAL;
	}

	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	icd->sense = &sense;
	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
	icd->sense = NULL;
	if (mf.code != xlate->code)
		return -EINVAL;
	if (ret < 0) {
		dev_err(dev, "Failed to configure for format %x\n",
			 pix->pixelformat);
		return ret;
	} else if (mtk_camera_check_frame(mf.width, mf.height)) {
		dev_err(dev,
			 "Camera driver produced an unsupported frame %dx%d\n",
			 mf.width, mf.height);
		return -EINVAL;
	} else if (sense.flags & SOCAM_SENSE_PCLK_CHANGED) {
		if (sense.pixel_clock > sense.pixel_clock_max) {
			dev_err(dev,
				"pixel clock %lu set by the camera too high!",
				sense.pixel_clock);
			return -EIO;
		}
	}

	pix->width		= mf.width;
	pix->height		= mf.height;
	pix->field		= mf.field;
	pix->colorspace		= mf.colorspace;
	icd->current_fmt	= xlate;

	/*if the width is 736, it need to reset vdoin Horizontal count and VSCALE REG */
	if (mf.width == MTK_VDOIN_SUPPORTED_WIDTH_720P) {
		writel(MTK_VDOIN_HCNT_SET_CMD_736, (pcdev->reg_vdoin_base + MTK_VDOIN_HCNT_SET_REG));
		writel(MTK_VDOIN_HCNT_SET1_CMD_736, (pcdev->reg_vdoin_base + MTK_VDOIN_HCNT_SET1_REG));
		writel(MTK_VDOIN_VSCALE_CMD_736, (pcdev->reg_vdoin_base + MTK_VDOIN_VSCALE_REG));
	} else if (mf.width == MTK_VDOIN_SUPPORTED_WIDTH_720P_STANDARD) {
		writel(MTK_VDOIN_HCNT_SET_CMD_720, (pcdev->reg_vdoin_base + MTK_VDOIN_HCNT_SET_REG));
		writel(MTK_VDOIN_HCNT_SET1_CMD_720, (pcdev->reg_vdoin_base + MTK_VDOIN_HCNT_SET1_REG));
		writel(MTK_VDOIN_VSCALE_CMD_720, (pcdev->reg_vdoin_base + MTK_VDOIN_VSCALE_REG));
	}
	if (((mf.height >> 1) - 1) == MTK_VDOIN_SUPPORTED_HEIGHT_4CHANNEL) {
		writel(MTK_VDOIN_ACT_LINE_CMD_4CHANNEL, (pcdev->reg_vdoin_base + MTK_VDOIN_ACT_LINE_REG));
		writel(MTK_VDOIN_DW_NEED_CMD_4CHANNEL, (pcdev->reg_vdoin_base + MTK_VDOIN_DW_NEED_REG));
	} else if (((mf.height >> 1) - 1) == MTK_VDOIN_SUPPORTED_HEIGHT_1CHANNEL) {
		writel(MTK_VDOIN_ACT_LINE_CMD_1CHANNEL, (pcdev->reg_vdoin_base + MTK_VDOIN_ACT_LINE_REG));
		writel(MTK_VDOIN_DW_NEED_CMD_1CHANNEL, (pcdev->reg_vdoin_base + MTK_VDOIN_DW_NEED_REG));
	}
	return ret;
}

static int mtk_camera_set_bus_param(struct soc_camera_device *icd)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->parent;
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	unsigned long common_flags;
	int ret;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret) {
		common_flags = soc_mbus_config_compatible(&cfg, SOCAM_BUS_FLAGS);
		if (!common_flags) {
			dev_err(dev,
				"Flags incompatible: camera 0x%x, host 0x%x\n",
				cfg.flags, SOCAM_BUS_FLAGS);
			return -EINVAL;
		}
	}  else {
		if (ret != -ENOIOCTLCMD)
			return ret;

		common_flags = SOCAM_BUS_FLAGS;
	}

	/* Make choices, possibly based on platform configuration */

	cfg.flags = common_flags;
	ret = v4l2_subdev_call(sd, video, s_mbus_config, &cfg);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		dev_err(dev, "camera s_mbus_config(0x%lx) returned %d\n",
			common_flags, ret);
		return ret;
	}
	return 0;
}

/* buffer for one video frame */
struct mtk_camera_buf {
	struct videobuf_buffer		vb;
	enum v4l2_mbus_pixelcode	code;
	int				inwork;
	struct scatterlist		*sgbuf;
	int				sgcount;
	int				bytes_left;
	enum videobuf_state		result;
};

static unsigned int mtk_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;
	struct mtk_camera_buf *buf;

	buf = list_entry(icd->vb_vidq.stream.next, struct mtk_camera_buf,
			 vb.stream);

	poll_wait(file, &buf->vb.done, pt);

	if (buf->vb.state == VIDEOBUF_DONE || buf->vb.state == VIDEOBUF_ERROR)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int mtk_camera_reqbufs(struct soc_camera_device *icd,
			      struct v4l2_requestbuffers *p)
{
	int i;

	/*
	 * This is for locking debugging only. I removed spinlocks and now I
	 * check whether .prepare is ever called on a linked buffer, or whether
	 * a dma IRQ can occur for an in-work or unlinked buffer. Until now
	 * it hadn't triggered
	 */
	for (i = 0; i < p->count; i++) {
		struct mtk_camera_buf *buf = container_of(icd->vb_vidq.bufs[i],
					      struct mtk_camera_buf, vb);
		buf->inwork = 0;
		INIT_LIST_HEAD(&buf->vb.queue);
	}

	return 0;
}

static int mtk_videobuf_setup(struct videobuf_queue *vq, unsigned int *count,
			      unsigned int *size)
{
	struct soc_camera_device *icd = vq->priv_data;

	dev_dbg(icd->parent, "count=%d, size=%d\n", *count, *size);

	*size = icd->sizeimage;

	if (0 == *count)
		*count = 32;
	if (*size * *count > MAX_VIDEO_MEM * 1024 * 1024)
		*count = (MAX_VIDEO_MEM * 1024 * 1024) / *size;

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct mtk_camera_buf *buf)
{
	struct videobuf_buffer *vb = &buf->vb;

	BUG_ON(in_interrupt());

	videobuf_waiton(vq, vb, 0, 0);
	vb->state = VIDEOBUF_NEEDS_INIT;
}

static int mtk_videobuf_prepare(struct videobuf_queue *vq,
		struct videobuf_buffer *vb, enum v4l2_field field)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct mtk_camera_buf *buf = container_of(vb,
		struct mtk_camera_buf, vb);
	int ret;

	WARN_ON(!list_empty(&vb->queue));

	BUG_ON(NULL == icd->current_fmt);

	buf->inwork = 1;

	if (buf->code != icd->current_fmt->code || vb->field != field ||
			vb->width  != icd->user_width ||
			vb->height != icd->user_height) {
		buf->code  = icd->current_fmt->code;
		vb->width  = icd->user_width;
		vb->height = icd->user_height;
		vb->field  = field;
		vb->state  = VIDEOBUF_NEEDS_INIT;
	}

	vb->size = icd->sizeimage;

	if (vb->baddr && vb->bsize < vb->size) {
		ret = -EINVAL;
		goto out;
	}

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		ret = videobuf_iolock(vq, vb, NULL);
		if (ret)
			goto fail;

		vb->state = VIDEOBUF_PREPARED;
	}
	buf->inwork = 0;

	return 0;
fail:
	free_buffer(vq, buf);
out:
	buf->inwork = 0;
	return ret;
}

struct videobuf_dma_contig_memory {
	u32 magic;
	void *vaddr;
	dma_addr_t dma_handle;
	unsigned long size;
};

static irqreturn_t mtk_camera_irq(int irq, void *data)
{
	struct mtk_camera_dev *pcdev = data;
	unsigned long flags;
	struct videobuf_buffer *vb;
	struct videobuf_dma_contig_memory *mem;

	writel((readl(pcdev->reg_interrupt_base + MTK_VDOIN_IRQ_CFG_REG) | MTK_VDOIN_IRQ_CFG),
		   pcdev->reg_interrupt_base + MTK_VDOIN_IRQ_CFG_REG);
	writel((readl(pcdev->reg_interrupt_base + MTK_VDOIN_IRQ_CFG_REG) & ~MTK_VDOIN_IRQ_CLR),
		   pcdev->reg_interrupt_base + MTK_VDOIN_IRQ_CFG_REG);

	if (pcdev->active) {
		spin_lock_irqsave(&pcdev->lock, flags);
		vb = &pcdev->active->vb;
		if (vb != NULL && vb->state == VIDEOBUF_ACTIVE) {
			mem = (struct videobuf_dma_contig_memory *) vb->priv;

			writel((unsigned int)mem->dma_handle>>2,
				pcdev->reg_vdoin_base + MTK_VDOIN_YBUF_ADDR_REG);
			/* Cbuffer address is base NV16 */
			writel((unsigned int)(mem->dma_handle + (vb->size / 2))>>2,
				pcdev->reg_vdoin_base + MTK_VDOIN_CBUF_ADDR_REG);
			writel((readl(pcdev->reg_vdoin_base) & (~(1<<27|1<<28))),
				pcdev->reg_vdoin_base);

			vb->state = VIDEOBUF_DONE;
			v4l2_get_timestamp(&vb->ts);

			list_del_init(&vb->queue);
			wake_up(&vb->done);

			if (!list_empty(&pcdev->capture)) {
				pcdev->active = list_entry(pcdev->capture.next,
					struct mtk_camera_buf, vb.queue);
				pcdev->active->vb.state = VIDEOBUF_ACTIVE;
			} else{
				pcdev->active = NULL;
			}
		}
		spin_unlock_irqrestore(&pcdev->lock, flags);
	}
	return IRQ_HANDLED;
}

static void mtk_videobuf_queue(struct videobuf_queue *vq,
			       struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_camera_dev *pcdev = ici->priv;

	list_add_tail(&vb->queue, &pcdev->capture);
	vb->state = VIDEOBUF_QUEUED;

	if (pcdev->active)
		return;

	pcdev->active = list_entry(pcdev->capture.next,
				   struct mtk_camera_buf, vb.queue);
	pcdev->active->vb.state = VIDEOBUF_ACTIVE;

	if (pcdev->enable_vdoin == 0) {
		pcdev->enable_vdoin = 1;
		enable_irq(pcdev->irq);
		writel((readl(pcdev->reg_vdoin_base) | (MTK_VDOIN_ENABLE_REG_BIT)), (pcdev->reg_vdoin_base));
	}
}

static void mtk_videobuf_release(struct videobuf_queue *vq,
				 struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mtk_camera_dev *pcdev = ici->priv;

	BUG_ON(in_interrupt());
	videobuf_waiton(vq, vb, 0, 0);

	if (pcdev->active == NULL && vb->state == VIDEOBUF_DONE && pcdev->enable_vdoin == 1) {
		pcdev->enable_vdoin = 0;
		disable_irq(pcdev->irq);
		writel((readl(pcdev->reg_vdoin_base) | (MTK_VDOIN_ENABLE_REG_BIT)), (pcdev->reg_vdoin_base));
	}
	vb->state = VIDEOBUF_NEEDS_INIT;
}

static const struct videobuf_queue_ops mtk_videobuf_ops = {
	.buf_setup      = mtk_videobuf_setup,
	.buf_prepare    = mtk_videobuf_prepare,
	.buf_queue      = mtk_videobuf_queue,
	.buf_release    = mtk_videobuf_release,
};

static void mtk_camera_init_videobuf(struct videobuf_queue *q,
			      struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_camera_dev *pcdev = ici->priv;
	/*
	 * We must pass NULL as dev pointer, then all pci_* dma operations
	 * transform to normal dma_* ones.
	 */
	videobuf_queue_dma_contig_init(q, &mtk_videobuf_ops,
				icd->parent, &pcdev->lock,
				V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_FIELD_NONE,
				sizeof(struct mtk_camera_buf), icd,
				&ici->host_lock);
	INIT_LIST_HEAD(&pcdev->capture);
}

static struct soc_camera_host_ops mtk_soc_camera_host_ops = {
	.owner			= THIS_MODULE,
	.add			= mtk_camera_add_device,
	.remove			= mtk_camera_remove_device,
	.clock_start		= mtk_camera_clock_start,
	.clock_stop		= mtk_camera_clock_stop,
	.set_crop		= mtk_camera_set_crop,
	.get_formats		= mtk_camera_get_formats,
	.set_fmt		= mtk_camera_set_fmt,
	.try_fmt		= mtk_camera_try_fmt,
	.init_videobuf		= mtk_camera_init_videobuf,
	.reqbufs		= mtk_camera_reqbufs,
	.poll			= mtk_camera_poll,
	.querycap		= mtk_camera_querycap,
	.set_bus_param		= mtk_camera_set_bus_param,
};

static const struct of_device_id mtk_camera_of_match[] = {
	{ .compatible = "mediatek,mt2701-vdoin", },
	{},
};
static int mtk_camera_suspend(struct device *dev)
{
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mtk_camera_dev *pcdev = ici->priv;
	int ret = 0;

	if (pcdev->soc_host.icd) {
		struct v4l2_subdev *sd =
			soc_camera_to_subdev(pcdev->soc_host.icd);

		ret = v4l2_subdev_call(sd, core, s_power, 0);
		if (ret == -ENOIOCTLCMD)
			ret = 0;
	}
	return ret;
}

static int mtk_camera_resume(struct device *dev)
{
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mtk_camera_dev *pcdev = ici->priv;
	int ret = 0;

	if (pcdev->soc_host.icd) {
		struct v4l2_subdev *sd =
			soc_camera_to_subdev(pcdev->soc_host.icd);

		ret = v4l2_subdev_call(sd, core, s_power, 1);
		if (ret == -ENOIOCTLCMD)
			ret = 0;
	}
	/* Restart frame capture if active buffer exists */
	return ret;
}

static const struct dev_pm_ops mtk_camera_pm = {
	.suspend	= mtk_camera_suspend,
	.resume		= mtk_camera_resume,
};

static int mtk_vdoin_init(void __iomem *base)
{
	writel(MTK_VDOIN_MODE_CMD, (base + MTK_VDOIN_MODE_REG));
	writel(MTK_VDOIN_DW_NEED_CMD_1CHANNEL, (base + MTK_VDOIN_DW_NEED_REG));
	writel(MTK_VDOIN_HPIXEL_CMD, (base + MTK_VDOIN_HPIXEL_REG));
	writel(MTK_VDOIN_HBLACK_CMD, (base + MTK_VDOIN_HBLACK_REG));
	writel(MTK_VDOIN_INPUT_CTRL_CMD, (base + MTK_VDOIN_INPUT_CTRL_REG));
	writel(MTK_VDOIN_TOP_BOT_SLINE_CMD, (base + MTK_VDOIN_TOP_BOT_SLINE_REG));
	writel(MTK_VDOIN_WRAP_3D_CMD, (base + MTK_VDOIN_WRAP_3D_REG));
	writel(MTK_VDOIN_WRAP_3D_VSYNC_CMD, (base + MTK_VDOIN_WRAP_3D_VSYNC_REG));
	writel(MTK_VDOIN_HCNT_SET_CMD_720, (base + MTK_VDOIN_HCNT_SET_REG));
	writel(MTK_VDOIN_HCNT_SET1_CMD_720, (base + MTK_VDOIN_HCNT_SET1_REG));
	writel(MTK_VDOIN_HSCALE_CMD, (base + MTK_VDOIN_HSCALE_REG));
	writel(MTK_VDOIN_ACT_LINE_CMD_1CHANNEL, (base + MTK_VDOIN_ACT_LINE_REG));
	writel(MTK_VDOIN_VSCALE_CMD_720, (base + MTK_VDOIN_VSCALE_REG));
	writel(MTK_VDOIN_PIC_CMD, (base + MTK_VDOIN_PIC_REG));
	writel(MTK_VDOIN_DEBUG4_CMD, (base + MTK_VDOIN_DEBUG4_REG));
	writel(MTK_VDOIN_DEBUG5_CMD, (base + MTK_VDOIN_DEBUG5_REG));
	writel(MTK_VDOIN_WRAP_3D_SET_CMD, (base + MTK_VDOIN_WRAP_3D_SET_REG));
	writel(MTK_VDOIN_EN_CMD, (base + MTK_VDOIN_EN_REG));
	return 0;
}

static int mtk_camera_probe(struct platform_device *pdev)
{
	struct mtk_camera_dev *pcdev;
	struct resource *res;
	int ret, irq;
	static struct clk *padmclk, *univpll_d52;

	pcdev = devm_kzalloc(&pdev->dev, sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev)
		return -ENOMEM;

	/*
	 * Request the regions.
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "get memory resource failed.\n");
		ret = -ENXIO;
		goto error;
	}
	pcdev->reg_interrupt_base = devm_ioremap_resource(&pdev->dev, res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL) {
		dev_err(&pdev->dev, "get memory resource failed.\n");
		ret = -ENXIO;
		goto error;
	}
	pcdev->reg_vdoin_base = devm_ioremap_resource(&pdev->dev, res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res == NULL) {
		dev_err(&pdev->dev, "get memory resource failed.\n");
		ret = -ENXIO;
		goto error;
	}
	pcdev->reg_caminputclk_base = devm_ioremap_resource(&pdev->dev, res);

	irq = platform_get_irq(pdev, 0);
	if (!res || irq < 0)
		return -ENODEV;

	/* VDOIN DRAM SELECTION*/
	writel((readl(pcdev->reg_interrupt_base + MTK_VDOIN_DRAM_CLK_CFG)
		| MTK_VDOIN_DRAM_CLK_REG),
		pcdev->reg_interrupt_base + MTK_VDOIN_DRAM_CLK_CFG);

	writel(MTK_VDOIN_DRAM_DATA_REG,
		pcdev->reg_interrupt_base + MTK_VDOIN_DRAM_DATA_CFG);

	writel((readl(pcdev->reg_caminputclk_base + MTK_VDOIN_DRAM_CAMERA_INPUT_CLOCK_PATH)
		| MTK_VDOIN_DRAM_MODE),
		pcdev->reg_caminputclk_base + MTK_VDOIN_DRAM_CAMERA_INPUT_CLOCK_PATH);
	/*reset vdoin*/
	writel(readl(pcdev->reg_interrupt_base + MTK_VDOIN_SOFTWARE_RESET)
		& ~(MTK_VDOIN_SOFTWARE_RESET_REG),
		pcdev->reg_interrupt_base + MTK_VDOIN_SOFTWARE_RESET);
	writel(readl(pcdev->reg_interrupt_base + MTK_VDOIN_SOFTWARE_RESET)
		| MTK_VDOIN_SOFTWARE_RESET_REG,
		pcdev->reg_interrupt_base + MTK_VDOIN_SOFTWARE_RESET);
	mtk_vdoin_init(pcdev->reg_vdoin_base);

	pcdev->clk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(pcdev->clk)) {
		dev_err(&pdev->dev, "Could not devm_clk_get\n");
		return PTR_ERR(pcdev->clk);
	}
	univpll_d52 = devm_clk_get(&pdev->dev, "TOP_UNIVPLL2_D52");
	clk_set_parent(padmclk, univpll_d52);
	pcdev->irq = irq;
	pcdev->pdata = devm_kzalloc(&pdev->dev, sizeof(struct mtk_camera_platform_data), GFP_KERNEL);
	pcdev->pdata->mclk_khz = clk_get_rate(pcdev->clk) / 1000;
	if (pcdev->pdata) {
		pcdev->pflags = pcdev->pdata->flags;
		/*camera work master clock*/
		pcdev->mclk = pcdev->pdata->mclk_khz * 1000;
	}

	INIT_LIST_HEAD(&pcdev->capture);
	spin_lock_init(&pcdev->lock);

	/* request irq */
	ret = devm_request_irq(&pdev->dev, pcdev->irq, mtk_camera_irq, 0,
			       MTK_CAM_DRV_NAME, pcdev);
	if (ret) {
		dev_err(&pdev->dev, "Camera interrupt register failed\n");
		goto error;
	}
	disable_irq(pcdev->irq);

	pcdev->soc_host.drv_name	= MTK_CAM_DRV_NAME;
	pcdev->soc_host.ops		= &mtk_soc_camera_host_ops;
	pcdev->soc_host.priv		= pcdev;
	pcdev->soc_host.v4l2_dev.dev	= &pdev->dev;
	pcdev->soc_host.nr		= pdev->id;
	ret = soc_camera_host_register(&pcdev->soc_host);
	if (ret)
		goto error;
	return 0;

error:
	dev_err(&pdev->dev, "Register host fail, ret = %d\n", ret);
	return ret;
}

static int mtk_camera_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);

	soc_camera_host_unregister(soc_host);
	return 0;
}

static struct platform_driver mtk_camera_driver = {
	.driver		= {
		.name	= MTK_CAM_DRV_NAME,
		.pm	= &mtk_camera_pm,
		.of_match_table = of_match_ptr(mtk_camera_of_match),
	},
	.probe		= mtk_camera_probe,
	.remove		= mtk_camera_remove,
};

module_platform_driver(mtk_camera_driver);

MODULE_DESCRIPTION("MTK SoC Camera Host driver");
MODULE_AUTHOR("wz <wz@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" MTK_CAM_DRV_NAME);
