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
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/of_platform.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/soc_camera.h>
#include <media/drv-intf/soc_mediabus.h>
#include <media/v4l2-of.h>
#include <media/videobuf2-core.h>
#include <linux/videodev2.h>
#include <soc/mediatek/smi.h>

#define MTK_CAMDMA_NUM 4
#define MTK_CAMDMA_MAX_NUM 4
#define MAX_DES_LINK 4

#define mipicsi0_info(fmt, args...)	\
		pr_info("[mipicsi0][info] %s %d: " fmt "\n",\
				__func__, __LINE__, ##args)
#define mipicsi0_err(fmt, args...)	\
			pr_info("[mipicsi0][err] %s %d: " fmt "\n",\
				 __func__, __LINE__, ##args)


#define MTK_MIPICSI0_DRV_NAME "mtk-mipicsi0"
#define MAX_VIDEO_MEM 16

#define MAX_BUFFER_NUM			32
#define MAX_SUPPORT_WIDTH		2048
#define MAX_SUPPORT_HEIGHT		2048
#define SUPPORT_WIDTH		2560 /* 1280 *2 */
#define SUPPORT_HEIGHT		1024
#define CAB888_WIDTH 2560 /* 1280 *2 */
#define CAB888_HEIGHT 720

#define VID_LIMIT_BYTES			(100 * 1024 * 1024)
#define MIN_FRAME_RATE			15
#define FRAME_INTERVAL_MILLI_SEC	(1000 / MIN_FRAME_RATE)

/* Definition for mtk_platform_data */
#define mtk_DATAWIDTH_8					0x01
#define mtk_DATAWIDTH_10				0x02


#define SOCAM_BUS_FLAGS	(V4L2_MBUS_MASTER | \
	V4L2_MBUS_HSYNC_ACTIVE_HIGH | V4L2_MBUS_VSYNC_ACTIVE_HIGH | \
	V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_PCLK_SAMPLE_FALLING | \
	V4L2_MBUS_DATA_ACTIVE_HIGH)
/* MIPI RX ANA CSI0 reg and val*/
#define MTK_RX_ANA00_CSI				0x00
#define MTK_RX_ANA04_CSI				0x04
#define MTK_RX_ANA08_CSI				0x08
#define MTK_RX_ANA0c_CSI				0x0c
#define MTK_RX_ANA10_CSI				0x10
#define MTK_RX_ANA20_CSI				0x20
#define MTK_RX_ANA24_CSI				0x24
#define MTK_RX_ANA4c_CSI				0x4c
#define MTK_RX_ANA50_CSI				0x50

#define MTK_RX_ANA00_CSI_VAL				0x00000008
#define MTK_RX_ANA04_CSI_VAL				0x00000008
#define MTK_RX_ANA08_CSI_VAL				0x00000008
#define MTK_RX_ANA0c_CSI_VAL				0x00000008
#define MTK_RX_ANA10_CSI_VAL				0x00000008
#define MTK_RX_ANA20_CSI_VAL				0xFF030003
#define MTK_RX_ANA24_CSI_VAL				0x00000011
#define MTK_RX_ANA4c_CSI_VAL				0xFEFBEFBE
#define MTK_RX_ANA50_CSI_VAL				0xFEFBEFBE
#define MTK_RX_ANA01_CSI_VAL				0x00000001


/* Test Pattern Generator SENINF*/
/*seninf top REG offset*/
#define MTK_SENINF_TOP_CTRL				0x00
#define MTK_SENINF_TOP_CMODEL_PAR			0x04
#define MTK_SENINF_TOP_MUX_CTRL				0x08
/*seninf top REG value*/
#define MTK_SENINF_TOP_CTRL_VAL				0x00010C00
#define MTK_SENINF_TOP_CMODEL_PAR_VAL			0x00079871
/*Select VC*/
#define MTK_SENINF_TOP_MUX_CTRL_VAL			0x32110000
/* seninf REG offset*/
#define MTK_SENINF1_CTRL				0x100
#define MTK_SENINF2_CTRL				0x500
#define MTK_SENINF3_CTRL				0x900
#define MTK_SENINF4_CTRL				0xD00
#define MTK_SENINF_MUX_CTRL				0x20

/*seninf REG value*/
#define MTK_SENINF1_CTRL_VAL				0x00008001
#define MTK_SENINF1_CTRL_TP_VAL				0x00001001

#define MTK_SENINF1_MUX_CTRL_VAL			0x9EFF8180
#define MTK_SENINF1_MUX_CTRL_TP_VAL			0xA6DF1080

#define MTK_SENINF2_MUX_CTRL_VAL			0x9EFF9180
#define MTK_SENINF3_MUX_CTRL_VAL			0x9EFFA180
#define MTK_SENINF4_MUX_CTRL_VAL			0x9EFFB180
/* MIPI rx config*/
#define MTK_SENINF_MIPI_RX_CON24_CSI0			0x24
#define MTK_SENINF_MIPI_RX_CON38_CSI0			0x38
#define MTK_SENINF_MIPI_RX_CON3C_CSI0			0x3C

#define MTK_SENINF_MIPI_RX_CON24_CSI0_VAL		0xE4000000
#define MTK_SENINF_MIPI_RX_CON38_CSI0_VAL_1		0x00000001
#define MTK_SENINF_MIPI_RX_CON38_CSI0_VAL_5		0x00000005
#define MTK_SENINF_MIPI_RX_CON38_CSI0_VAL_4		0x00000004

#define MTK_SENINF_MIPI_RX_CON3C_CSI0_VAL		0x00051545
#define MTK_SENINF_MIPI_RX_CON3C_CSI0_VAL_0		0x00000000

/* SENINF1_NCSI2 REG offset*/
#define MTK_SENINF1_NCSI2_CTL				0xA0
#define MTK_SENINF1_NCSI2_LNRC_TIMING			0xA4
#define MTK_SENINF1_NCSI2_LNRD_TIMING			0xA8
#define MTK_SENINF1_NCSI2_DPCM				0xAC

#define MTK_SENINF1_NCSI2_INT_EN			0xB0
#define MTK_SENINF1_NCSI2_INT_STATUS			0xB4
#define MTK_SENINF1_NCSI2_DGB_SEL			0xB8
#define MTK_SENINF1_NCSI2_DGB_PORT			0xBC

#define MTK_SENINF1_NCSI2_SPARE0			0xC0
#define MTK_SENINF1_NCSI2_SPARE1			0xC4
#define MTK_SENINF1_NCSI2_LNRC_FSM			0xC8
#define MTK_SENINF1_NCSI2_LNRD_FSM			0xCC

#define MTK_SENINF1_NCSI2_HSR_X_DBG			0xD8
#define MTK_SENINF1_NCSI2_DI				0xDC


#define MTK_SENINF1_NCSI2_HS_TRAIL			0xE0
#define MTK_SENINF1_NCSI2_DI_CTRL			0xE4
#define MTK_SENINF1_NCSI2_DI_1				0xE8
#define MTK_SENINF1_NCSI2_DI_CTRL1			0xEC
/* SENINF1_NCSI2 REG value*/

#define MTK_SENINF1_NCSI2_CTL_VAL			0x018961FF
#define MTK_SENINF1_NCSI2_CTL_TP_VAL			0x1278617F

#define MTK_SENINF1_NCSI2_LNRC_TIMING_VAL		0x00000100
#define MTK_SENINF1_NCSI2_LNRD_TIMING_TP_VAL		0x00003A01
#define MTK_SENINF1_NCSI2_LNRD_TIMING_VAL		0x00003000
#define MTK_SENINF1_NCSI2_DPCM_VAL			0x00000000

#define MTK_SENINF1_NCSI2_INT_EN_VAL			0x7FFFFFFF
#define MTK_SENINF1_NCSI2_INT_EN_TP_VAL			0x80321FFF
#define MTK_SENINF1_NCSI2_INT_STATUS_TP_VAL		0x00000000
#define MTK_SENINF1_NCSI2_INT_STATUS_VAL		0x00007FFF
#define MTK_SENINF1_NCSI2_DGB_SEL_TP_VAL		0x000000b6

#define MTK_SENINF1_NCSI2_DGB_SEL_VAL_11		0x00000011
#define MTK_SENINF1_NCSI2_DGB_SEL_VAL_FFFFFF00		0xFFFFFF00
#define MTK_SENINF1_NCSI2_DGB_SEL_VAL_FFFFFF45		0xFFFFFF45
#define MTK_SENINF1_NCSI2_DGB_SEL_VAL_FFFFFFEF		0xFFFFFFEF

#define MTK_SENINF1_NCSI2_DGB_PORT_VAL			0x00000000

#define MTK_SENINF1_NCSI2_SPARE0_VAL			0x123649F1
#define MTK_SENINF1_NCSI2_SPARE1_VAL			0x62735C8C
#define MTK_SENINF1_NCSI2_LNRC_FSM_VAL			0x00000001
#define MTK_SENINF1_NCSI2_LNRD_FSM_VAL			0x01010101



#define MTK_SENINF1_NCSI2_HS_TRAIL_VAL			0x00000000
#define MTK_SENINF1_NCSI2_DI_CTRL_VAL			0x01010101
#define MTK_SENINF1_NCSI2_DI_1_VAL			0x00000000
#define MTK_SENINF1_NCSI2_DI_CTRL1_VAL			0x00000000
#define MTK_SENINF1_NCSI2_HSR_X_DBG_VAL			0xFFFFFFEF
#define MTK_SENINF1_NCSI2_DI_VAL			0x03020100
#define MTK_SENINF1_NCSI2_DI_TP_VAL			0x00000000

/* Seninf TG*/
#define MTK_SENINF1_TG1_SEN_CK				0x204
#define MTK_SENINF1_TG1_TM_CTL				0x208
#define MTK_SENINF1_TG1_TM_SIZE				0x20C


#define MTK_SENINF1_TG1_SEN_CK_VAL			0x00003005
#define MTK_SENINF1_TG1_TM_CTL_VAL			0x00300015
#define MTK_SENINF1_TG1_TM_SIZE_VAL			0x04000A00


/* CAMSV REG offset*/
#define MTK_CAMSV_MODULE_EN				0x00
#define MTK_CAMSV_FMT_SEL				0x04
#define MTK_CAMSV_INT_EN				0x08
#define MTK_CAMSV_INT_STATUS				0x0c
#define MTK_CAMSV_IMGO_FBC				0x1c
#define MTK_CAMSV_CLK_EN				0x20

/* CAMSV REG value*/
#define MTK_CAMSV_MODULE_EN_VAL				0x40000019
/*CamDMA module enable*/
#define MTK_CAMSV_FMT_SEL_VAL				0x00000003
/*CamDMA format select [0:2] -> 1:RAW10, 3:YUV422, 7:JPEG*/
#define MTK_CAMSV_INT_EN_VAL				0x80000000
/*write clean enable*/
#define MTK_CAMSV_INT_EN_STATUS_VAL			0x80000400
/*Interrupt function turn on [10]: pass1_done, [31]interrupt write clear*/
#define MTK_CAMSV_IMGO_FBC_VAL				0x00000001
/*Increase fbc count [0]:1*/
#define MTK_CAMSV_CLK_EN_VAL				0x00008005
/*CamDMA clock enable*/

/* CAMSV_TG REG offset*/
#define MTK_CAMSV_TG_OTG_SEN_MODEL			0x00
#define MTK_CAMSV_TG_OTG_VF_CON				0x04
#define MTK_CAMSV_TG_OTG_SEN_GRAB_PXL			0x08
#define MTK_CAMSV_TG_OTG_SEN_GRAB_LIN			0x0c
#define MTK_CAMSV_TG_OTG_PATH_CFG			0x10

/* CAMSV_TG REG Value*/
#define MTK_CAMSV_TG_OTG_SEN_MODEL_VAL_0		0x00000003
/*Set TG configure TG_SEN_MODE : RAW/YUV	mode*/
#define MTK_CAMSV_TG_OTG_SEN_MODEL_VAL_0_JPEG		0x0000210F
/*Set TG configure TG_SEN_MODE : JPEG  mode*/
#define MTK_CAMSV_TG_OTG_VF_CON_VAL_0			0x00000001
/*CAMSV_0_TG_VF_CON*/
#define MTK_CAMSV_TG_OTG_SEN_GRAB_PXL_VAL_0		0x0A000000
#define MTK_CAMSV_TG_OTG_SEN_GRAB_LIN_VAL_0		0x04000000
#define MTK_CAMSV_TG_OTG_PATH_CFG_VAL_0			0x1000
/*Turn on TG [12]:1*/
#define MTK_CAMSV_TG_OTG_PATH_CFG_VAL_0_JPEG	0x90
/*Turn on TG jpeg mode in jpeg pattern [4]:1 [7]:1*/


/*CAMSV_FBC REG offset */
#define MTK_CAMSV_FBC_OFBC_IMGO_CTL1			0x00
#define MTK_CAMSV_FBC_OFBC_IMGO_ENO_ADDR		0x08

/* CAMSV_FBC REG value*/
#define MTK_CAMSV_FBC_OFBC_IMGO_CTL1_0_VAL		0x00418000
#define MTK_CAMSV_FBC_OFBC_IMGO_ENO_ADDR_0_VAL

/* CAMSV_DMA REG offset*/
#define MTK_CAMSV_DMA_OIMAGO_XSIZE			0x30
#define MTK_CAMSV_DMA_OIMAGO_YSIZE			0x34
#define MTK_CAMSV_DMA_OIMAGO_STRIDE			0x38

/* CAMSV_DMA REG value*/
/*#define MTK_CAMSV_DMA_OCQOIBASE_ADDR_VAL_0		0x1*/
/*CAMSV_0_TG_VF_CON*/
#define MTK_CAMSV_DMA_OIMAGO_XSIZE_VAL_0		0x500
/* Image xsize 1280*2-1*/
#define MTK_CAMSV_DMA_OIMAGO_YSIZE_VAL_0		0x400
/*Image ysize 1024-1*/
#define MTK_CAMSV_DMA_OIMAGO_STRIDE_VAL_0		0x01810000
#define SUBDEV_LINK_REG 0x49
#define MTK_MIPICSI0_CTRLS_HINT 20

/* if intersil camera sensor, skip first 5 frames */
static int skipframe_num = 5;
MODULE_PARM_DESC(skipframe_num, "MTK Carema driver drop some frames during start");
module_param(skipframe_num, int, 0644);

enum mipicsi_subdev_type {
	DES,
	SER,
};

struct mtk_mipicsi0_platform_data {
	unsigned long	mclk_khz;
	unsigned long	flags;
};
struct vb_addr {
	dma_addr_t vb_dma_addr_phy;
};
struct mtk_mipicsi0_dma_desc {
	dma_addr_t vb_dma_addr_phy;
	int prepare_flag;
};

/* buffer for one video frame */
struct mtk_mipicsi0_buf {
	struct list_head		queue;
	struct vb2_buffer *vb;
	dma_addr_t vb_dma_addr_phy;
	int prepare_flag;
	int				code;
	int				inwork;
	int				sgcount;
	int				bytes_left;
	enum videobuf_state		result;
};

struct mtk_mipicsi0_dev {
	struct soc_camera_host	soc_host;
	/*
	 * mipicsi0 is supposed to handle 4 camera on its Quick Capture
	 * interface. If anyone ever builds hardware to enable more than
	 * one camera, they will have to modify this driver too
	 */
	struct clk		*clk;
	struct clk		*img_seninf_cam_clk;
	struct clk		*img_seninf_scam_clk;
	struct clk		*img_cam_sv_clk;
	struct clk		*img_cam_sv1_clk;
	struct v4l2_device	v4l2_dev;
	struct device *larb_pdev;
	unsigned int		irq[MTK_CAMDMA_MAX_NUM];
	void __iomem		*mipi_csi_ana_base;
	void __iomem		*mipi_seninf1_base;
	void __iomem		*mipi_seninf_ctrl_base;
	void __iomem		*seninf_top_mux_base;
	void __iomem		*mipi_csi_clk;
	void __iomem		*mipi_seninf_mux_base[MTK_CAMDMA_MAX_NUM];
	void __iomem		*mipi_camsv_base[MTK_CAMDMA_MAX_NUM];
	void __iomem		*mipi_camsv_fbc_base[MTK_CAMDMA_MAX_NUM];
	void __iomem		*mipi_camsv_dma_base[MTK_CAMDMA_MAX_NUM];
	void __iomem		*mipi_camsv_tg_base[MTK_CAMDMA_MAX_NUM];
	const struct soc_camera_format_xlate *current_fmt;
	struct mtk_mipicsi0_platform_data *pdata;
	unsigned long		mclk;
	unsigned long		pflags;
	unsigned long		sequence;
	u32			mclk_divisor;
	u16				width_flags;	/* max 12 bits */
	struct list_head	capture_list[MTK_CAMDMA_MAX_NUM];
	int cur_index[MTK_CAMDMA_MAX_NUM];
	spinlock_t		lock[MTK_CAMDMA_MAX_NUM];
	struct mtk_mipicsi0_buf	cam_buf[MAX_BUFFER_NUM];
	int			skip_frame_num;
	int			is_enable_irq[MTK_CAMDMA_MAX_NUM];
	unsigned int		buf_sequence;
	int streamon;
	unsigned long frame_cnt[MTK_CAMDMA_MAX_NUM];
	int link;
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_pix_format	current_pix;
	irq_handler_t mtk_mipicsi0_irq_handler[MTK_CAMDMA_MAX_NUM];
};

static inline struct mtk_mipicsi0_dev *ctrl_to_pcdev(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_mipicsi0_dev, ctrl_hdl);
}

static int mtk_mipicsi0_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_mipicsi0_dev *pcdev = ctrl_to_pcdev(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		ctrl->val = pcdev->link * 3;
		break;
	default:
		break;
	}
	return 0;
}

static const struct v4l2_ctrl_ops mtk_mipicsi0_ctrl_ops = {
	.g_volatile_ctrl = mtk_mipicsi0_g_v_ctrl,
};

int mtk_mipicsi0_ctrls_setup(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_dev *pcdev = ici->priv;
	struct v4l2_ctrl *ctrl = NULL;

	v4l2_ctrl_handler_init(&pcdev->ctrl_hdl, MTK_MIPICSI0_CTRLS_HINT);

	/* g_volatile_ctrl */
	ctrl = v4l2_ctrl_new_std(&pcdev->ctrl_hdl,
			&mtk_mipicsi0_ctrl_ops,
			V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
			pcdev->link * 2, 32, pcdev->link, pcdev->link * 2);
	if (!ctrl)
		return -1;
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (pcdev->ctrl_hdl.error) {
		mipicsi0_info("Adding control failed %d",
				pcdev->ctrl_hdl.error);
		return pcdev->ctrl_hdl.error;
	}

	v4l2_ctrl_handler_setup(&pcdev->ctrl_hdl);
	return 0;
}

static int get_des_register(struct soc_camera_device *icd,
	struct v4l2_dbg_register *reg)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	int ret = 0;

	reg->match.type = DES;
	ret = v4l2_subdev_call(sd, core, g_register, reg);
	if (ret != 2) {
		mipicsi0_err("get des register 0x%llx fail, ret=%d",
			reg->reg, ret);
		return -EIO;
	}
	mipicsi0_info("read DES [reg/val/ret] is [0x%llx/0x%llx/%d]",
		reg->reg, reg->val, ret);
	return ret;
}

static int get_des_link(struct soc_camera_device *icd, int *link)
{
	struct v4l2_dbg_register reg;
	int ret = 0;
	int index = 0;

	if (!link) {
		mipicsi0_err("link = %p", link);
		return -EINVAL;
	}
	memset(&reg, 0, sizeof(reg));
	/*get camera link number*/
	reg.reg = SUBDEV_LINK_REG;
	ret = get_des_register(icd, &reg);
	if (ret < 0)
		return ret;

	*link = 0;
	for (index = 0; index < MAX_DES_LINK; ++index) {
		*link += (reg.val & 0x1);
		reg.val >>= 1;
	}

	mipicsi0_info("%d camera linked to sub device", *link);
	return 0;
}

static int mtk_mipicsi0_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_dev *pcdev = ici->priv;

	get_des_link(icd, &pcdev->link);
	mtk_mipicsi0_ctrls_setup(icd);
	pm_runtime_get_sync(icd->parent);
	return 0;
}

static void mtk_mipicsi0_remove_device(struct soc_camera_device *icd)
{
	pm_runtime_put_sync(icd->parent);
}

static int mtk_mipicsi0_querycap(struct soc_camera_host *ici,
			       struct v4l2_capability *cap)
{
	/* cap->name is set by the firendly caller:-> */
	strlcpy(cap->card, "MTK Camera", sizeof(cap->card));
	strlcpy(cap->driver, "MTK Camera", sizeof(cap->driver));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int mtk_mipicsi0_clock_start(struct soc_camera_host *ici)
{
	struct mtk_mipicsi0_dev *pcdev = ici->priv;
	int err = 0;

	if (pcdev->larb_pdev) {
		err = mtk_smi_larb_get(pcdev->larb_pdev);
		if (err)
			mipicsi0_err("failed to get larb, err %d", err);
	}
	clk_prepare_enable(pcdev->clk);
	clk_prepare_enable(pcdev->img_seninf_cam_clk);
	clk_prepare_enable(pcdev->img_seninf_scam_clk);
	clk_prepare_enable(pcdev->img_cam_sv_clk);
	clk_prepare_enable(pcdev->img_cam_sv1_clk);

	return 0;
}

static void mtk_mipicsi0_clock_stop(struct soc_camera_host *ici)
{
	struct mtk_mipicsi0_dev *pcdev = ici->priv;

	clk_disable_unprepare(pcdev->clk);
	clk_disable_unprepare(pcdev->img_seninf_cam_clk);
	clk_disable_unprepare(pcdev->img_seninf_scam_clk);
	clk_disable_unprepare(pcdev->img_cam_sv_clk);
	clk_disable_unprepare(pcdev->img_cam_sv1_clk);
	if (pcdev->larb_pdev)
		mtk_smi_larb_put(pcdev->larb_pdev);
}

static bool is_supported(struct soc_camera_device *icd,
		const u32 pixformat)
{
	switch (pixformat) {
	/* YUV, including grey */
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
	/* RGB */
	case V4L2_PIX_FMT_RGB565:
		return true;
	default:
		return false;
	}
}

static const struct soc_mbus_lookup mtk_mipicsi0_formats[] = {
	{
		.code = MEDIA_BUS_FMT_UYVY8_2X8,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_NV16,
			.name			= "YYUV",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_LE,
			.layout			= SOC_MBUS_LAYOUT_PLANAR_Y_C,
		},
	},
	{
		.code = MEDIA_BUS_FMT_VYUY8_2X8,
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
		.code = MEDIA_BUS_FMT_YUYV8_2X8,
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
		.code = MEDIA_BUS_FMT_YVYU8_2X8,
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
		.code = MEDIA_BUS_FMT_YVYU8_2X8,
		.fmt = {
			.fourcc			= V4L2_PIX_FMT_YUYV,
			.name			= "YUYV",
			.bits_per_sample	= 8,
			.packing		= SOC_MBUS_PACKING_2X8_PADHI,
			.order			= SOC_MBUS_ORDER_LE,
			.layout			= SOC_MBUS_LAYOUT_PACKED,
		},
	},
	{
		.code = MEDIA_BUS_FMT_RGB555_2X8_PADHI_BE,
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
		.code = MEDIA_BUS_FMT_RGB555_2X8_PADHI_LE,
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
		.code = MEDIA_BUS_FMT_BGR565_2X8_BE,
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
		.code = MEDIA_BUS_FMT_BGR565_2X8_LE,
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

#define MTK_CAMERA_BUS_PARAM (V4L2_MBUS_MASTER |	\
		V4L2_MBUS_HSYNC_ACTIVE_HIGH |	\
		V4L2_MBUS_HSYNC_ACTIVE_LOW |	\
		V4L2_MBUS_VSYNC_ACTIVE_HIGH |	\
		V4L2_MBUS_VSYNC_ACTIVE_LOW |	\
		V4L2_MBUS_PCLK_SAMPLE_RISING |	\
		V4L2_MBUS_PCLK_SAMPLE_FALLING |	\
		V4L2_MBUS_DATA_ACTIVE_HIGH)

static int mtk_mipicsi0_try_bus_param(struct soc_camera_device *icd,
				    unsigned char buswidth)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_dev *pcdev = ici->priv;
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_CSI2,};
	unsigned long common_flags = 0;
	int ret = 0;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret) {
		common_flags = soc_mbus_config_compatible(&cfg,
				MTK_CAMERA_BUS_PARAM);
		if (!common_flags) {
			mipicsi0_err("Flags incompatible: camera 0x%x, host 0x%x",
				cfg.flags, MTK_CAMERA_BUS_PARAM);
			return -EINVAL;
		}
	} else if (ret != -ENOIOCTLCMD) {
		return ret;
	}

	if ((1 << (buswidth - 1)) & pcdev->width_flags)
		return 0;
	return -EINVAL;
}


/* This will be corrected as we get more formats */
static bool mtk_mipicsi0_packing_supported(const struct soc_mbus_pixelfmt *fmt)
{
	return	fmt->packing == SOC_MBUS_PACKING_NONE ||
		(fmt->bits_per_sample == 8 &&
		 fmt->packing == SOC_MBUS_PACKING_2X8_PADHI) ||
		(fmt->bits_per_sample > 8 &&
		 fmt->packing == SOC_MBUS_PACKING_EXTEND16);
}

static int mtk_mipicsi0_get_formats(struct soc_camera_device *icd,
		unsigned int idx, struct soc_camera_format_xlate *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	int formats = 0, ret, i, n;
	/* sensor format */
	struct v4l2_subdev_mbus_code_enum code = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.index = idx,
	};
	/* soc camera host format */
	const struct soc_mbus_pixelfmt *fmt;

	ret = v4l2_subdev_call(sd, pad, enum_mbus_code, NULL, &code);
	if (ret < 0)
		/* No more formats */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code.code);
	if (!fmt) {
		mipicsi0_err("Invalid format code #%u: %d", idx, code.code);
		return 0;
	}

	/* This also checks support for the requested bits-per-sample */
	ret = mtk_mipicsi0_try_bus_param(icd, fmt->bits_per_sample);
	if (ret < 0) {
		mipicsi0_err("Fail to try the bus parameters");
		return 0;
	}

	switch (code.code) {
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
		n = ARRAY_SIZE(mtk_mipicsi0_formats);
		formats += n;
		for (i = 0; xlate && i < n; i++, xlate++) {
			xlate->host_fmt	= &mtk_mipicsi0_formats[i].fmt;
			xlate->code	= code.code;
			dev_dbg(icd->parent, "Providing format %s using code %d\n",
				xlate->host_fmt->name, xlate->code);
		}
		break;
	default:
		if (!mtk_mipicsi0_packing_supported(fmt))
			return 0;
		if (xlate)
			dev_dbg(icd->parent,
				"Providing format %s in pass-through mode\n",
				fmt->name);
	}

	/* Generic pass-through */
	formats++;
	if (xlate) {
		xlate->host_fmt	= fmt;
		xlate->code	= code.code;
		xlate++;
	}

	return formats;
}


static int mtk_mipicsi0_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev_pad_config pad_cfg;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
	};
	struct v4l2_mbus_framefmt *mf = &format.format;
	u32 pixfmt = pix->pixelformat;
	int ret = 0;

	/* check with atmel-isi support format, if not support use YUYV */
	if (!is_supported(icd, pix->pixelformat))
		pix->pixelformat = V4L2_PIX_FMT_YUYV;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (pixfmt && !xlate) {
		mipicsi0_err("Format %x not found", pixfmt);
		return -EINVAL;
	}

	/* limit to  MTK hardware capabilities */
	if (pix->height > MAX_SUPPORT_HEIGHT)
		pix->height = MAX_SUPPORT_HEIGHT;
	if (pix->width > MAX_SUPPORT_WIDTH)
		pix->width = MAX_SUPPORT_WIDTH;

	/* limit to sensor capabilities */
	mf->width	= pix->width;
	mf->height	= pix->height;
	mf->field	= pix->field;
	mf->colorspace	= pix->colorspace;
	mf->code	= xlate->code;

	ret = v4l2_subdev_call(sd, pad, set_fmt, &pad_cfg, &format);
	if (ret < 0)
		return ret;

	pix->width	= mf->width;
	pix->height	= mf->height;
	pix->colorspace	= mf->colorspace;

	switch (mf->field) {
	case V4L2_FIELD_ANY:
		pix->field = V4L2_FIELD_NONE;
		break;
	case V4L2_FIELD_NONE:
		break;
	default:
		mipicsi0_err("Field type %d unsupported", mf->field);
		ret = -EINVAL;
	}

	return ret;
}

static int mtk_mipicsi0_set_fmt(struct soc_camera_device *icd,
				struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	struct v4l2_mbus_framefmt *mf = &format.format;
	int ret = 0;

	/* check with atmel-isi support format, if not support use YUYV */
	if (!is_supported(icd, pix->pixelformat))
		pix->pixelformat = V4L2_PIX_FMT_YUYV;

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		mipicsi0_err("Format %x not found", pix->pixelformat);
		return -EINVAL;
	}

	mf->width	= pix->width;
	mf->height	= pix->height;
	mf->field	= pix->field;
	mf->colorspace	= pix->colorspace;
	mf->code	= xlate->code;

	ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, &format);
	if (ret < 0)
		return ret;

	if (mf->code != xlate->code)
		return -EINVAL;

	pix->width		= mf->width;
	pix->height		= mf->height;
	pix->field		= mf->field;
	pix->colorspace		= mf->colorspace;
	icd->current_fmt	= xlate;

	return ret;
}

static int mtk_mipicsi0_set_bus_param(struct soc_camera_device *icd)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	unsigned long common_flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH;
	int ret = 0;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret) {
		common_flags = soc_mbus_config_compatible(&cfg,
				MTK_CAMERA_BUS_PARAM);
		if (!common_flags) {
			mipicsi0_err("Flags incompatible: camera 0x%x, host 0x%x",
				cfg.flags, MTK_CAMERA_BUS_PARAM);
			return -EINVAL;
		}
	} else if (ret != -ENOIOCTLCMD) {
		return ret;
	}
	common_flags = MTK_CAMERA_BUS_PARAM;

	dev_dbg(icd->parent, "Flags cam: 0x%x host: 0x%x common: 0x%lx\n",
		cfg.flags, MTK_CAMERA_BUS_PARAM, common_flags);

	cfg.flags = common_flags;
	ret = v4l2_subdev_call(sd, video, s_mbus_config, &cfg);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		dev_dbg(icd->parent, "camera s_mbus_config(0x%lx) returned %d\n",
			common_flags, ret);
		return ret;
	}

	return 0;
}

static int mtk_mipicsi0_set_param(struct soc_camera_device *icd,
	struct v4l2_streamparm *a)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);

	if (!ici->ops->get_parm)
		return ici->ops->get_parm(icd, a);

	return 0;
}

static int mtk_mipicsi0_get_parm(struct soc_camera_device *icd,
	struct v4l2_streamparm *a)
{
	int link = 0;
	int ret = 0;

	/*get camera link number*/
	ret = get_des_link(icd, &link);
	if (ret < 0)
		return ret;

	a->parm.capture.timeperframe.numerator = 1;
	a->parm.capture.timeperframe.denominator = link * 30;

	return 0;
}

static int mtk_mipi_csi_ana_init(void __iomem *base)
{
	writel(MTK_RX_ANA4c_CSI_VAL & readl(base + MTK_RX_ANA4c_CSI),
		(base + MTK_RX_ANA4c_CSI));
	writel(MTK_RX_ANA50_CSI_VAL & readl(base + MTK_RX_ANA50_CSI),
		(base + MTK_RX_ANA50_CSI));
	writel(MTK_RX_ANA00_CSI_VAL | readl(base + MTK_RX_ANA00_CSI),
		(base + MTK_RX_ANA00_CSI));
	writel(MTK_RX_ANA04_CSI_VAL | readl(base + MTK_RX_ANA04_CSI),
		(base + MTK_RX_ANA04_CSI));
	writel(MTK_RX_ANA08_CSI_VAL | readl(base + MTK_RX_ANA08_CSI),
		(base + MTK_RX_ANA08_CSI));
	writel(MTK_RX_ANA0c_CSI_VAL | readl(base + MTK_RX_ANA0c_CSI),
		(base + MTK_RX_ANA0c_CSI));
	writel(MTK_RX_ANA10_CSI_VAL | readl(base + MTK_RX_ANA10_CSI),
		(base + MTK_RX_ANA10_CSI));
	writel(MTK_RX_ANA24_CSI_VAL | readl(base + MTK_RX_ANA24_CSI),
		(base + MTK_RX_ANA24_CSI));
	mdelay(1);
	writel(MTK_RX_ANA20_CSI_VAL | readl(base + MTK_RX_ANA20_CSI),
		(base + MTK_RX_ANA20_CSI));
	mdelay(1);
	writel(MTK_RX_ANA01_CSI_VAL | readl(base + MTK_RX_ANA00_CSI),
		(base + MTK_RX_ANA00_CSI));
	writel(MTK_RX_ANA01_CSI_VAL | readl(base + MTK_RX_ANA04_CSI),
		(base + MTK_RX_ANA04_CSI));
	writel(MTK_RX_ANA01_CSI_VAL | readl(base + MTK_RX_ANA08_CSI),
		(base + MTK_RX_ANA08_CSI));
	writel(MTK_RX_ANA01_CSI_VAL | readl(base + MTK_RX_ANA0c_CSI),
		(base + MTK_RX_ANA0c_CSI));
	writel(MTK_RX_ANA01_CSI_VAL | readl(base + MTK_RX_ANA10_CSI),
		(base + MTK_RX_ANA10_CSI));

	return 0;
}

static int mtk_seninf1_init(void __iomem *base)
{

	writel(MTK_SENINF_MIPI_RX_CON38_CSI0_VAL_1,
		(base + MTK_SENINF_MIPI_RX_CON38_CSI0));
	writel(MTK_SENINF_MIPI_RX_CON3C_CSI0_VAL,
		(base + MTK_SENINF_MIPI_RX_CON3C_CSI0));
	writel(MTK_SENINF_MIPI_RX_CON38_CSI0_VAL_5,
		(base + MTK_SENINF_MIPI_RX_CON38_CSI0));
	mdelay(1);
	writel(MTK_SENINF_MIPI_RX_CON38_CSI0_VAL_4,
		(base + MTK_SENINF_MIPI_RX_CON38_CSI0));
	writel(MTK_SENINF_MIPI_RX_CON3C_CSI0_VAL_0,
		(base + MTK_SENINF_MIPI_RX_CON3C_CSI0));
	writel(MTK_SENINF1_NCSI2_CTL_VAL, (base + MTK_SENINF1_NCSI2_CTL));
	writel(~(1U<<27) & readl(base + MTK_SENINF1_NCSI2_CTL),
		(base + MTK_SENINF1_NCSI2_CTL));
	writel((1U<<27) | readl(base + MTK_SENINF1_NCSI2_CTL),
			(base + MTK_SENINF1_NCSI2_CTL));
	writel(MTK_SENINF1_NCSI2_LNRD_TIMING_VAL,
		(base + MTK_SENINF1_NCSI2_LNRD_TIMING));
	writel(MTK_SENINF1_NCSI2_INT_STATUS_VAL,
		(base + MTK_SENINF1_NCSI2_INT_STATUS));

	writel(MTK_SENINF1_NCSI2_INT_EN_VAL,
		(base + MTK_SENINF1_NCSI2_INT_EN));
	writel(MTK_SENINF_MIPI_RX_CON24_CSI0_VAL,
		(base + MTK_SENINF_MIPI_RX_CON24_CSI0));
	writel(MTK_SENINF1_NCSI2_DGB_SEL_VAL_FFFFFF00 &
		readl(base + MTK_SENINF1_NCSI2_DGB_SEL),
		base + MTK_SENINF1_NCSI2_DGB_SEL);
	writel(MTK_SENINF1_NCSI2_DGB_SEL_VAL_FFFFFF45 |
		readl(base + MTK_SENINF1_NCSI2_DGB_SEL),
		base + MTK_SENINF1_NCSI2_DGB_SEL);
	writel(MTK_SENINF1_NCSI2_DGB_SEL_VAL_FFFFFFEF &
		readl(base + MTK_SENINF1_NCSI2_HSR_X_DBG),
		base + MTK_SENINF1_NCSI2_HSR_X_DBG);
	writel(MTK_SENINF1_NCSI2_DI_CTRL_VAL,
		(base + MTK_SENINF1_NCSI2_DI_CTRL));
	writel(MTK_SENINF1_NCSI2_DI_VAL,
		(base + MTK_SENINF1_NCSI2_DI));


	return 0;
}

static int mtk_seninf_top_init(void __iomem *base)
{
#if MTK_CAMDMA_NUM == 4
	writel(MTK_SENINF_TOP_CTRL_VAL, base + MTK_SENINF_TOP_CTRL);
	writel(MTK_SENINF_TOP_CMODEL_PAR_VAL, base + MTK_SENINF_TOP_CMODEL_PAR);
	writel(MTK_SENINF_TOP_MUX_CTRL_VAL, base + MTK_SENINF_TOP_MUX_CTRL);
#endif
	return 0;
}

static int mtk_seninf_ctrl_init(void __iomem *base)
{
	writel(MTK_SENINF1_CTRL_VAL, base);
	return 0;

}

static int mtk_seninf_mux_init(void __iomem *base, int mux_index)
{
	u32 reg_val = 0;

	reg_val = (((0x9EFF8 + mux_index) << 12) | 0x180);
	writel(reg_val, base);
	return 0;
}

static int mtk_camsv_init(void __iomem *base)
{
	writel(MTK_CAMSV_MODULE_EN_VAL, base);
	writel(MTK_CAMSV_CLK_EN_VAL, (base + MTK_CAMSV_CLK_EN));
	writel(MTK_CAMSV_FMT_SEL_VAL, (base + MTK_CAMSV_FMT_SEL));
	writel(MTK_CAMSV_INT_EN_STATUS_VAL, (base + MTK_CAMSV_INT_EN));

	return 0;
}

static int mtk_camsv_tg_init(void __iomem *base, int enable_jpeg,
		u32 width, u32 height)
{
	writel(width << 16, base + MTK_CAMSV_TG_OTG_SEN_GRAB_PXL);
	writel(height << 16, (base + MTK_CAMSV_TG_OTG_SEN_GRAB_LIN));
	if (enable_jpeg) {
		writel(MTK_CAMSV_TG_OTG_PATH_CFG_VAL_0_JPEG,
			(base + MTK_CAMSV_TG_OTG_PATH_CFG));
		writel(MTK_CAMSV_TG_OTG_SEN_MODEL_VAL_0_JPEG,
			(base + MTK_CAMSV_TG_OTG_SEN_MODEL));
	} else {
		writel(MTK_CAMSV_TG_OTG_PATH_CFG_VAL_0,
			(base + MTK_CAMSV_TG_OTG_PATH_CFG));
		writel(MTK_CAMSV_TG_OTG_SEN_MODEL_VAL_0,
			(base + MTK_CAMSV_TG_OTG_SEN_MODEL));
	}

	return 0;
}

static int mtk_camsv_fbc_init(void __iomem *fbc_base, void __iomem *camsv_base,
			dma_addr_t dma_handle, int is_init)
{
	if (is_init) {
		writel(MTK_CAMSV_FBC_OFBC_IMGO_CTL1_0_VAL,
			fbc_base + MTK_CAMSV_FBC_OFBC_IMGO_CTL1);
	} else {
		writel(dma_handle, fbc_base + MTK_CAMSV_FBC_OFBC_IMGO_ENO_ADDR);
		writel(MTK_CAMSV_IMGO_FBC_VAL, camsv_base + MTK_CAMSV_IMGO_FBC);
	}

	return 0;
}

static int mtk_camsv_dma_init(void __iomem *base, u32 width, u32 height)
{
	writel(MTK_CAMSV_DMA_OIMAGO_STRIDE_VAL_0 | width,
		base + MTK_CAMSV_DMA_OIMAGO_STRIDE);
	writel(width - 1, (base + MTK_CAMSV_DMA_OIMAGO_XSIZE));
	writel(height - 1, (base + MTK_CAMSV_DMA_OIMAGO_YSIZE));

	return 0;
}

static int mtk_mipicsi0_vb2_queue_setup(struct vb2_queue *vq,
		unsigned int *nbufs,
		unsigned int *num_planes, unsigned int sizes[],
		struct device *alloc_devs[])
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vq);
	u32 sizeimage = icd->sizeimage;

	if (!*nbufs || *nbufs > MAX_BUFFER_NUM)
		*nbufs = MAX_BUFFER_NUM;
	if (sizeimage * *nbufs > VID_LIMIT_BYTES)
		*nbufs = VID_LIMIT_BYTES / sizeimage;

	/*
	 * Called from VIDIOC_REQBUFS or in compatibility mode For YUV422P
	 * format, even if there are 3 planes Y, U and V, we reply there is only
	 * one plane, containing Y, U and V data, one after the other.
	 */
	if (*num_planes)
		return sizes[0] < sizeimage ? -EINVAL : 0;
	sizes[0] = sizeimage;
	*num_planes = 1;
	return 0;
}

static int mtk_mipicsi0_vb2_init(struct vb2_buffer *vb)
{
	struct mtk_mipicsi0_dev *pcdev = vb2_get_drv_priv(vb->vb2_queue);

	pcdev->cam_buf[vb->index].prepare_flag = 0;

	return 0;
}

static int mtk_mipicsi0_vb2_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = NULL;
	struct soc_camera_host *ici = NULL;
	struct mtk_mipicsi0_dev *pcdev = NULL;
	u32 size = 0;

	/* notice that vb->vb2_queue addr equals to soc_camera_device->vb2_vidq.
	 *  It was handled in reqbufs
	 */
	icd = soc_camera_from_vb2q(vb->vb2_queue);
	ici = to_soc_camera_host(icd->parent);
	pcdev = ici->priv;
	size = icd->sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		mipicsi0_err("data will not fit into plane (%lu < %u)",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);

	if (!(pcdev->cam_buf[vb->index].prepare_flag)) {
		pcdev->cam_buf[vb->index].prepare_flag = 1;
		pcdev->cam_buf[vb->index].vb_dma_addr_phy =
			vb2_dma_contig_plane_dma_addr(vb, 0);
		pcdev->cam_buf[vb->index].vb = vb;
	}

	return 0;
}

static void mtk_mipicsi0_vb2_queue(struct vb2_buffer *vb)
{
	int index = 0;
	int link = 0;
	unsigned long flags = 0;
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_dev *pcdev = ici->priv;

	link = (pcdev->link > 0) ? pcdev->link : MTK_CAMDMA_NUM;
	index = (vb->index) % link;

	spin_lock_irqsave(&pcdev->lock[index], flags);
	list_add_tail(&(pcdev->cam_buf[vb->index].queue),
		&(pcdev->capture_list[index]));
	spin_unlock_irqrestore(&pcdev->lock[index], flags);

	if (pcdev->cur_index[index] < 0) {
		pcdev->cur_index[index] = vb->index;

		mtk_camsv_fbc_init(pcdev->mipi_camsv_fbc_base[index],
			pcdev->mipi_camsv_base[index],
			pcdev->cam_buf[vb->index].vb_dma_addr_phy, 0);
		pcdev->skip_frame_num = 0;

		if (!pcdev->is_enable_irq[index] && pcdev->streamon) {
			pcdev->is_enable_irq[index] = 1;
			enable_irq(pcdev->irq[index]);
		}
	}
}

static int mtk_mipicsi0_vb2_start_streaming(struct vb2_queue *vq,
		unsigned int count)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_dev *pcdev = ici->priv;
	int ret = 0;
	int index = 0;
	int link = 0;

	pm_runtime_get_sync(ici->v4l2_dev.dev);
	pcdev->buf_sequence = 0;
	icd->vdev->queue = vq;

	if (pcdev->larb_pdev) {
		ret = mtk_smi_larb_get(pcdev->larb_pdev);
		if (ret)
			mipicsi0_err("failed to get larb, err %d", ret);
	}

	link = (pcdev->link > 0) ? pcdev->link : MTK_CAMDMA_NUM;

	for (index = 0; index < link; ++index) {
		if ((pcdev->cur_index[index] >= 0) &&
			(!pcdev->is_enable_irq[index])) {
			pcdev->is_enable_irq[index] = 1;
			enable_irq(pcdev->irq[index]);
			pcdev->streamon = 1;
		}
	}

	return 0;
}

static void mtk_mipicsi0_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vq);
	struct mtk_mipicsi0_dev *pcdev = vb2_get_drv_priv(vq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_buf *buf = NULL;
	int i = 0;
	unsigned long flags = 0;
	int link = 0;

	link = pcdev->link > 0 ? pcdev->link : MTK_CAMDMA_NUM;

	for (i = 0; i < link; i++) {
		spin_lock_irqsave(&pcdev->lock[i], flags);

		if (pcdev->cur_index[i] >= 0) {
			pcdev->cur_index[i] = -1;
			pcdev->is_enable_irq[i] = 0;
			disable_irq_nosync(pcdev->irq[i]);
		}

		list_for_each_entry(buf, &(pcdev->capture_list[i]), queue) {
			if (buf->vb->state == VB2_BUF_STATE_ACTIVE)
				vb2_buffer_done(buf->vb, VB2_BUF_STATE_ERROR);
			buf->vb_dma_addr_phy = 0;
			buf->prepare_flag = 0;
		}
		INIT_LIST_HEAD(&(pcdev->capture_list[i]));
		spin_unlock_irqrestore(&pcdev->lock[i], flags);
	}

	if (pcdev->larb_pdev)
		mtk_smi_larb_put(pcdev->larb_pdev);

	pm_runtime_put_sync(ici->v4l2_dev.dev);
}

static struct vb2_ops mtk_vb2_ops = {
	.queue_setup		= mtk_mipicsi0_vb2_queue_setup,
	.buf_init			= mtk_mipicsi0_vb2_init,
	.buf_prepare		= mtk_mipicsi0_vb2_prepare,
	.buf_queue			= mtk_mipicsi0_vb2_queue,
	.start_streaming	= mtk_mipicsi0_vb2_start_streaming,
	.stop_streaming		= mtk_mipicsi0_vb2_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static int mtk_mipicsi0_init_videobuf2(struct vb2_queue *q,
			      struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mtk_mipicsi0_dev *pcdev = ici->priv;
	struct mutex *q_lock = NULL;

	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP;
	q->drv_priv = pcdev;
	q->buf_struct_size = sizeof(struct vb2_buffer);
	q->ops = &mtk_vb2_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->dev = ici->v4l2_dev.dev;
	q_lock = devm_kzalloc(pcdev->soc_host.v4l2_dev.dev,
			sizeof(*q_lock), GFP_KERNEL);
	q->lock = q_lock;
	mutex_init(q->lock);

	return vb2_queue_init(q);
}


static struct soc_camera_host_ops mtk_soc_camera_host_ops = {
	.owner			= THIS_MODULE,
	.add			= mtk_mipicsi0_add_device,
	.remove			= mtk_mipicsi0_remove_device,
	.clock_start		= mtk_mipicsi0_clock_start,
	.clock_stop		= mtk_mipicsi0_clock_stop,
	.get_formats		= mtk_mipicsi0_get_formats,
	.set_fmt		= mtk_mipicsi0_set_fmt,
	.try_fmt		= mtk_mipicsi0_try_fmt,
	.init_videobuf2	= mtk_mipicsi0_init_videobuf2,
	.poll			= vb2_fop_poll,
	.querycap		= mtk_mipicsi0_querycap,
	.set_bus_param		= mtk_mipicsi0_set_bus_param,
	.get_parm		= mtk_mipicsi0_get_parm,
	.set_parm		= mtk_mipicsi0_set_param,
};

static const struct of_device_id mtk_mipicsi0_of_match[] = {
	{ .compatible = "mediatek,mt2712-mipicsi0", },
	{},
};
static int mtk_mipicsi0_suspend(struct device *dev)
{
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mtk_mipicsi0_dev *pcdev = NULL;
	int ret = 0;

	if (pm_runtime_suspended(dev))
		return 0;

	pcdev = container_of(ici, struct mtk_mipicsi0_dev, soc_host);
	if (pcdev->soc_host.icd) {
		struct v4l2_subdev *sd =
			soc_camera_to_subdev(pcdev->soc_host.icd);

		ret = v4l2_subdev_call(sd, core, s_power, 0);
		if (ret == -ENOIOCTLCMD)
			ret = 0;
	}

	clk_disable_unprepare(pcdev->img_seninf_scam_clk);
	clk_disable_unprepare(pcdev->img_cam_sv_clk);
	clk_disable_unprepare(pcdev->img_cam_sv1_clk);
	clk_disable_unprepare(pcdev->img_seninf_cam_clk);
	clk_disable_unprepare(pcdev->clk);
	return ret;
}

static int mtk_mipicsi0_resume(struct device *dev)
{
	struct soc_camera_host *ici = to_soc_camera_host(dev);

	struct mtk_mipicsi0_dev *pcdev =
		container_of(ici, struct mtk_mipicsi0_dev, soc_host);
	int ret = 0;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = clk_prepare_enable(pcdev->clk);
	ret = clk_prepare_enable(pcdev->img_seninf_cam_clk);
	ret = clk_prepare_enable(pcdev->img_cam_sv_clk);
	ret = clk_prepare_enable(pcdev->img_cam_sv1_clk);
	ret = clk_prepare_enable(pcdev->img_seninf_scam_clk);

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

static const struct dev_pm_ops mtk_mipicsi0_pm = {
	SET_RUNTIME_PM_OPS(mtk_mipicsi0_suspend, mtk_mipicsi0_resume, NULL)
};

static irqreturn_t mtk_mipicsi0_irq_camdma0(int irq, void *data)
{
	struct mtk_mipicsi0_dev *pcdev = data;
	unsigned int index;
	enum vb2_buffer_state	state = VB2_BUF_STATE_ERROR;
	struct mtk_mipicsi0_buf *new_cam_buf = NULL;

	/* clear interrupt*/
	writel(1U << 10, pcdev->mipi_camsv_base[0] + MTK_CAMSV_INT_STATUS);
	index = pcdev->cur_index[0];

	if (index < 0)
		return IRQ_HANDLED;

	spin_lock(&pcdev->lock[0]);
	vb2_buffer_done(pcdev->cam_buf[index].vb, VB2_BUF_STATE_DONE);
	++(pcdev->frame_cnt[0]);
	/* delete current buffer from capture list and
	 * then find an unused buffer fill into fbc
	 */
	list_del_init(&(pcdev->cam_buf[index].queue));

	if (!list_empty(&(pcdev->capture_list[0]))) {
		list_for_each_entry(new_cam_buf,
			&(pcdev->capture_list[0]), queue) {
			index = new_cam_buf->vb->index % pcdev->link;
			state = new_cam_buf->vb->state;
			if ((index % pcdev->link == 0) &&
				(state == VB2_BUF_STATE_ACTIVE))
				break;
		}
		if ((index % pcdev->link == 0) &&
			(state == VB2_BUF_STATE_ACTIVE)) {
			pcdev->cur_index[0] = new_cam_buf->vb->index;
			mtk_camsv_fbc_init(pcdev->mipi_camsv_fbc_base[0],
			pcdev->mipi_camsv_base[0],
			pcdev->cam_buf[new_cam_buf->vb->index].vb_dma_addr_phy,
			0);
		} else {
			pcdev->cur_index[0] = -1;
			writel(0, pcdev->mipi_camsv_base[0] +
				MTK_CAMSV_IMGO_FBC);
			if (pcdev->is_enable_irq[0]) {
				disable_irq_nosync(pcdev->irq[0]);
				pcdev->is_enable_irq[0] = 0;
			}
		}
	} else {
		pcdev->cur_index[0] = -1;
		writel(0, pcdev->mipi_camsv_base[0]+MTK_CAMSV_IMGO_FBC);
		if (pcdev->is_enable_irq[0]) {
			disable_irq_nosync(pcdev->irq[0]);
			pcdev->is_enable_irq[0] = 0;
		}
	}

	spin_unlock(&pcdev->lock[0]);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_mipicsi0_irq_camdma1(int irq, void *data)
{
	struct mtk_mipicsi0_dev *pcdev = data;
	unsigned int index;
	enum vb2_buffer_state	state = VB2_BUF_STATE_ERROR;
	struct mtk_mipicsi0_buf *new_cam_buf = NULL;

	/* clear interrupt*/
	writel(1U << 10, pcdev->mipi_camsv_base[1] + MTK_CAMSV_INT_STATUS);
	index = pcdev->cur_index[1];

	if (index < 0)
		return IRQ_HANDLED;

	spin_lock(&pcdev->lock[1]);
	vb2_buffer_done(pcdev->cam_buf[index].vb, VB2_BUF_STATE_DONE);
	++(pcdev->frame_cnt[1]);

	/* delete current buffer from capture list and
	 * then find an unused buffer fill into fbc
	 */
	list_del_init(&(pcdev->cam_buf[index].queue));

	if (!list_empty(&pcdev->capture_list[1])) {
		list_for_each_entry(new_cam_buf,
			&pcdev->capture_list[1], queue) {
			index = new_cam_buf->vb->index % pcdev->link;
			state = new_cam_buf->vb->state;
			if ((index % pcdev->link == 1) &&
				(state == VB2_BUF_STATE_ACTIVE))
				break;
		}
		if ((index % pcdev->link == 1) &&
			(state == VB2_BUF_STATE_ACTIVE)) {
			pcdev->cur_index[1] = new_cam_buf->vb->index;
			mtk_camsv_fbc_init(pcdev->mipi_camsv_fbc_base[1],
			pcdev->mipi_camsv_base[1],
			pcdev->cam_buf[new_cam_buf->vb->index].vb_dma_addr_phy,
			0);
		} else {
			writel(0, pcdev->mipi_camsv_base[1] +
				MTK_CAMSV_IMGO_FBC);
			if (pcdev->is_enable_irq[1]) {
				disable_irq_nosync(pcdev->irq[1]);
				pcdev->is_enable_irq[1] = 0;
			}
		}
	} else {
		pcdev->cur_index[1] = -1;
		writel(0, pcdev->mipi_camsv_base[1] + MTK_CAMSV_IMGO_FBC);
		if (pcdev->is_enable_irq[1]) {
			disable_irq_nosync(pcdev->irq[1]);
			pcdev->is_enable_irq[1] = 0;
		}

	}

	spin_unlock(&pcdev->lock[1]);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_mipicsi0_irq_camdma2(int irq, void *data)
{
	struct mtk_mipicsi0_dev *pcdev = data;
	unsigned int index;
	enum vb2_buffer_state	state = VB2_BUF_STATE_ERROR;
	struct mtk_mipicsi0_buf *new_cam_buf = NULL;


	/* clear interrupt*/
	writel(1U << 10, pcdev->mipi_camsv_base[2] + MTK_CAMSV_INT_STATUS);
	index = pcdev->cur_index[2];
	if (index < 0)
		return IRQ_HANDLED;

	spin_lock(&pcdev->lock[2]);
	vb2_buffer_done(pcdev->cam_buf[index].vb, VB2_BUF_STATE_DONE);
	++(pcdev->frame_cnt[2]);

	/* delete current buffer from capture list and
	 * then find an unused buffer fill into fbc
	 */
	list_del_init(&(pcdev->cam_buf[index].queue));

	if (!list_empty(&pcdev->capture_list[2])) {
		list_for_each_entry(new_cam_buf,
			&pcdev->capture_list[2], queue) {
			index = new_cam_buf->vb->index % pcdev->link;
			state = new_cam_buf->vb->state;
			if ((index % pcdev->link == 2) &&
				(state == VB2_BUF_STATE_ACTIVE))
				break;
		}
		if ((index % pcdev->link == 2) &&
			(state == VB2_BUF_STATE_ACTIVE)) {
			pcdev->cur_index[2] = new_cam_buf->vb->index;
			mtk_camsv_fbc_init(pcdev->mipi_camsv_fbc_base[2],
			pcdev->mipi_camsv_base[2],
			pcdev->cam_buf[new_cam_buf->vb->index].vb_dma_addr_phy,
			0);
		} else {
			pcdev->cur_index[2] = -1;
			writel(0, pcdev->mipi_camsv_base[2] +
				MTK_CAMSV_IMGO_FBC);
			if (pcdev->is_enable_irq[2]) {
				disable_irq_nosync(pcdev->irq[2]);
				pcdev->is_enable_irq[2] = 0;
			}
		}
	} else {
		pcdev->cur_index[2] = -1;
		writel(0, pcdev->mipi_camsv_base[2] + MTK_CAMSV_IMGO_FBC);
		if (pcdev->is_enable_irq[2]) {
			disable_irq_nosync(pcdev->irq[2]);
			pcdev->is_enable_irq[2] = 0;
		}
	}

	spin_unlock(&pcdev->lock[2]);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_mipicsi0_irq_camdma3(int irq, void *data)
{
	struct mtk_mipicsi0_dev *pcdev = data;
	unsigned int index;
	enum vb2_buffer_state	state = VB2_BUF_STATE_ERROR;
	struct mtk_mipicsi0_buf *new_cam_buf = NULL;

	/* clear interrupt*/
	writel(1U << 10, pcdev->mipi_camsv_base[3] + MTK_CAMSV_INT_STATUS);
	index = pcdev->cur_index[3];
	if (index < 0)
		return IRQ_HANDLED;

	spin_lock(&pcdev->lock[3]);
	vb2_buffer_done(pcdev->cam_buf[index].vb, VB2_BUF_STATE_DONE);
	++(pcdev->frame_cnt[3]);

	/* delete current buffer from capture list and
	 * then find an unused buffer fill into fbc
	 */
	list_del_init(&(pcdev->cam_buf[index].queue));

	if (!list_empty(&pcdev->capture_list[3])) {
		list_for_each_entry(new_cam_buf,
			&pcdev->capture_list[3], queue) {
			index = new_cam_buf->vb->index % pcdev->link;
			state = new_cam_buf->vb->state;
			if ((index % pcdev->link == 3) &&
				(state == VB2_BUF_STATE_ACTIVE))
				break;
		}
		if ((index % pcdev->link == 3) &&
			(state == VB2_BUF_STATE_ACTIVE)) {
			pcdev->cur_index[3] = new_cam_buf->vb->index;
			mtk_camsv_fbc_init(pcdev->mipi_camsv_fbc_base[3],
			pcdev->mipi_camsv_base[3],
			pcdev->cam_buf[new_cam_buf->vb->index].vb_dma_addr_phy,
			0);
		} else {
			writel(0, pcdev->mipi_camsv_base[3] +
				MTK_CAMSV_IMGO_FBC);
			pcdev->cur_index[3] = -1;
			if (pcdev->is_enable_irq[3]) {
				disable_irq_nosync(pcdev->irq[3]);
				pcdev->is_enable_irq[3] = 0;
			}
		}
	} else {
		pcdev->cur_index[3] = -1;
		writel(0, pcdev->mipi_camsv_base[3] + MTK_CAMSV_IMGO_FBC);
		if (pcdev->is_enable_irq[3]) {
			disable_irq_nosync(pcdev->irq[3]);
			pcdev->is_enable_irq[3] = 0;
		}
	}

	spin_unlock(&pcdev->lock[3]);

	return IRQ_HANDLED;
}

static int mtk_mipicsi0_probe(struct platform_device *pdev)
{
	struct mtk_mipicsi0_dev *pcdev = NULL;
	struct resource *res = NULL;
	struct clk *univpll_d52 = NULL;
	struct iommu_domain *iommu = NULL;
	struct device_node *node = NULL;
	struct platform_device *larbpdev = NULL;
	int ret = 0;
	int i = 0;


	iommu = iommu_get_domain_for_dev(&pdev->dev);
	if (!iommu) {
		mipicsi0_err("Waiting iommu driver ready...");
		return -EPROBE_DEFER;
	}

	pcdev = devm_kzalloc(&pdev->dev, sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev)
		return -ENOMEM;

	pcdev->mtk_mipicsi0_irq_handler[0] = mtk_mipicsi0_irq_camdma0;
	pcdev->mtk_mipicsi0_irq_handler[1] = mtk_mipicsi0_irq_camdma1;
	pcdev->mtk_mipicsi0_irq_handler[2] = mtk_mipicsi0_irq_camdma2;
	pcdev->mtk_mipicsi0_irq_handler[3] = mtk_mipicsi0_irq_camdma3;

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!node) {
		mipicsi0_err("Missing mediadek,larb phandle in %s node",
			node->full_name);
		return -EINVAL;
	}

	larbpdev = of_find_device_by_node(node);
	if (!larbpdev) {
		mipicsi0_err("Waiting for larb device %s", node->full_name);
		of_node_put(node);
		return -EPROBE_DEFER;
	}
	of_node_put(node);

	pcdev->larb_pdev = NULL;
	pcdev->larb_pdev = &larbpdev->dev;

	/* request irq */
	for (i = 0; i < MTK_CAMDMA_NUM; i++) {
		pcdev->is_enable_irq[i] = 0;
		pcdev->irq[i] = platform_get_irq(pdev, i);
		if (pcdev->irq[i] < 0)
			return -ENODEV;

		ret = devm_request_irq(&pdev->dev, pcdev->irq[i],
				pcdev->mtk_mipicsi0_irq_handler[i], 0,
				MTK_MIPICSI0_DRV_NAME, pcdev);
		if (ret) {
			mipicsi0_err("Camera interrupt register failed");
			goto error;
		}
		disable_irq(pcdev->irq[i]);
	}

	pcdev->clk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(pcdev->clk)) {
		mipicsi0_err("Could not devm_clk_get");
		return PTR_ERR(pcdev->clk);
	}
	univpll_d52 = devm_clk_get(&pdev->dev, "TOP_UNIVPLL2_D52");
	clk_set_parent(pcdev->clk, univpll_d52);
	pcdev->pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct mtk_mipicsi0_platform_data),
			GFP_KERNEL);
	if (!(pcdev->pdata))
		return -ENOMEM;

	pcdev->pdata->mclk_khz = clk_get_rate(pcdev->clk) / 1000;
	pcdev->pflags = pcdev->pdata->flags;
	/*camera work master clock*/
	pcdev->mclk = pcdev->pdata->mclk_khz * 1000;

	pcdev->img_seninf_cam_clk =
		devm_clk_get(&pdev->dev, "IMG_SENINF_CAM_EN");
	if (IS_ERR(pcdev->img_seninf_cam_clk)) {
		mipicsi0_err("Could not IMG_SENINF_CAM_EN");
		return PTR_ERR(pcdev->clk);
	}

	pcdev->img_seninf_scam_clk =
		devm_clk_get(&pdev->dev, "IMG_SENINF_SCAM_EN");
	if (IS_ERR(pcdev->img_seninf_scam_clk)) {
		mipicsi0_err("Could not IMG_SENINF_SCAM_EN");
		return PTR_ERR(pcdev->clk);
	}

	pcdev->img_cam_sv_clk = devm_clk_get(&pdev->dev, "IMG_CAM_SV_EN");
	if (IS_ERR(pcdev->img_cam_sv_clk)) {
		mipicsi0_err("Could not IMG_CAM_SV_EN");
		return PTR_ERR(pcdev->clk);
	}

	pcdev->img_cam_sv1_clk = devm_clk_get(&pdev->dev, "IMG_CAM_SV1_EN");
	if (IS_ERR(pcdev->img_cam_sv1_clk)) {
		mipicsi0_err("Could not IMG_CAM_SV1_EN");
		return PTR_ERR(pcdev->clk);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 24);
	if (!res) {
		mipicsi0_err("get memory resource failed");
		ret = -ENXIO;
		goto error;
	}

	pcdev->mipi_csi_clk = devm_ioremap_resource(&pdev->dev, res);

	pcdev->soc_host.drv_name	= MTK_MIPICSI0_DRV_NAME;
	pcdev->soc_host.ops		= &mtk_soc_camera_host_ops;
	pcdev->soc_host.priv		= pcdev;
	pcdev->soc_host.v4l2_dev.dev	= &pdev->dev;
	pcdev->soc_host.nr		= pdev->id;
	pcdev->width_flags = mtk_DATAWIDTH_8 << 7;
	pcdev->streamon = 0;
	ret = soc_camera_host_register(&pcdev->soc_host);
	if (ret)
		goto reg_err;

	mipicsi0_info("0x15000000=%x", readl(pcdev->mipi_csi_clk));
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
	mipicsi0_info("0x15000000=%x", readl(pcdev->mipi_csi_clk));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		mipicsi0_err("get memory resource failed");
		ret = -ENXIO;
		goto error;
	}

	pcdev->mipi_csi_ana_base = devm_ioremap_resource(&pdev->dev, res);
	mtk_mipi_csi_ana_init(pcdev->mipi_csi_ana_base);
	res = platform_get_resource(pdev, IORESOURCE_MEM,  1);
	if (!res) {
		mipicsi0_err("get memory resource failed");
		ret = -ENXIO;
		goto error;
	}

	pcdev->mipi_seninf1_base = devm_ioremap_resource(&pdev->dev, res);
	mtk_seninf1_init(pcdev->mipi_seninf1_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM,  2);
	if (!res) {
		mipicsi0_err("get memory resource failed");
		ret = -ENXIO;
		goto error;
	}

	pcdev->seninf_top_mux_base = devm_ioremap_resource(&pdev->dev, res);
	mtk_seninf_top_init(pcdev->seninf_top_mux_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM,  3);
	if (!res) {
		mipicsi0_err("get memory resource failed");
		ret = -ENXIO;
		goto error;
	}

	pcdev->mipi_seninf_ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	mtk_seninf_ctrl_init(pcdev->mipi_seninf_ctrl_base);

	for (i = 0; i < MTK_CAMDMA_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 5 * i + 4);
		if (!res) {
			mipicsi0_err("get memory resource failed");
			ret = -ENXIO;
			goto error;
		}

		pcdev->mipi_seninf_mux_base[i] =
			devm_ioremap_resource(&pdev->dev, res);
		mtk_seninf_mux_init(pcdev->mipi_seninf_mux_base[i], i);

		res = platform_get_resource(pdev, IORESOURCE_MEM, 5 * i + 5);
		if (!res) {
			mipicsi0_err("get memory resource failed");
			ret = -ENXIO;
			goto error;
		}

		pcdev->mipi_camsv_base[i] = devm_ioremap_resource(&pdev->dev,
						res);
		mtk_camsv_init(pcdev->mipi_camsv_base[i]);

		res = platform_get_resource(pdev, IORESOURCE_MEM, 5 * i + 6);
		if (!res) {
			mipicsi0_err("get memory resource failed");
			ret = -ENXIO;
			goto error;
		}

		pcdev->mipi_camsv_fbc_base[i] =
			devm_ioremap_resource(&pdev->dev, res);

		res = platform_get_resource(pdev, IORESOURCE_MEM, 5 * i + 7);
		if (!res) {
			mipicsi0_err("get memory resource failed");
			ret = -ENXIO;
			goto error;
		}

		pcdev->mipi_camsv_dma_base[i] =
			devm_ioremap_resource(&pdev->dev, res);

		res = platform_get_resource(pdev, IORESOURCE_MEM, 5 * i + 8);
		if (!res) {
			mipicsi0_err("get memory resource failed");
			ret = -ENXIO;
			goto error;
		}

		pcdev->mipi_camsv_tg_base[i] =
			devm_ioremap_resource(&pdev->dev, res);

#ifdef TG_TESTPATTERN
		mtk_camsv_tg_init(pcdev->mipi_camsv_tg_base[i], 0,
			SUPPORT_WIDTH, SUPPORT_HEIGHT);
#else
		mtk_camsv_tg_init(pcdev->mipi_camsv_tg_base[i], 0,
			CAB888_WIDTH, CAB888_HEIGHT);
#endif
		mtk_camsv_fbc_init(pcdev->mipi_camsv_fbc_base[i],
			pcdev->mipi_camsv_base[i], 0, 1);

#ifdef TG_TESTPATTERN
		mtk_camsv_dma_init(pcdev->mipi_camsv_dma_base[i],
			SUPPORT_WIDTH, SUPPORT_HEIGHT);
#else
		mtk_camsv_dma_init(pcdev->mipi_camsv_dma_base[i],
			CAB888_WIDTH, CAB888_HEIGHT);
#endif
		writel(MTK_CAMSV_TG_OTG_VF_CON_VAL_0,
			(pcdev->mipi_camsv_tg_base[i] +
				MTK_CAMSV_TG_OTG_VF_CON));
		pcdev->cur_index[i] = -1;
		INIT_LIST_HEAD(&pcdev->capture_list[i]);
		spin_lock_init(&pcdev->lock[i]);
		pcdev->frame_cnt[i] = 0;
	}

	ret = vb2_dma_contig_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));
	return ret;

reg_err:
	mipicsi0_err("Register host fail, ret = %d", ret);
	return ret;
error:
	soc_camera_host_unregister(&pcdev->soc_host);
	return ret;
}

static int mtk_mipicsi0_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	soc_camera_host_unregister(soc_host);

	return 0;
}

static struct platform_driver mtk_mipicsi0_driver = {
	.driver		= {
		.name	= MTK_MIPICSI0_DRV_NAME,
		.pm	= &mtk_mipicsi0_pm,
		.of_match_table = of_match_ptr(mtk_mipicsi0_of_match),
	},
	.probe		= mtk_mipicsi0_probe,
	.remove		= mtk_mipicsi0_remove,
};

module_platform_driver(mtk_mipicsi0_driver);
MODULE_DESCRIPTION("MTK SoC Camera Host driver");
MODULE_AUTHOR("baoyin zhang <baoyin.zhang@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" MTK_MIPICSI0_DRV_NAME);

