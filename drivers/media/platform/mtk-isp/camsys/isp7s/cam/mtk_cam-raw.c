// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <linux/suspend.h>
#include <linux/rtc.h>
#include <mtk_printk_ctrl.h>

#include <soc/mediatek/smi.h>

#include "mtk_cam.h"
#include "mtk_cam-ctrl.h"
#include "mtk_cam-dvfs_qos_raw.h"
#include "mtk_cam-feature.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-dmadbg.h"
#include "mtk_cam-raw_debug.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-trace.h"

static int debug_dump_fbc;
module_param(debug_dump_fbc, int, 0644);
MODULE_PARM_DESC(debug_dump_fbc, "debug: dump fbc");

#define MTK_RAW_STOP_HW_TIMEOUT			(33)

#define KERNEL_LOG_MAX	                400

void trigger_rawi(struct mtk_raw_device *dev, signed int hw_scene)
{
#define TRIGGER_RAWI_R6 0x10
#define TRIGGER_RAWI_R2 0x01

	u32 cmd = 0;

	if (hw_scene == MTKCAM_IPI_HW_PATH_OFFLINE_M2M)
		cmd = TRIGGER_RAWI_R2;
	else if (hw_scene == MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER)
		cmd = TRIGGER_RAWI_R6;

	dev_dbg(dev->dev, "m2m %s, cmd:%d\n", __func__, cmd);

	writel_relaxed(cmd, dev->base + REG_CTL_RAWI_TRIG);
	wmb(); /* TBC */
}

/*stagger case seamless switch case*/
void dbload_force(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CTL_MISC);
	writel_relaxed(val | CTL_DB_LOAD_FORCE, dev->base + REG_CTL_MISC);
	writel_relaxed(val | CTL_DB_LOAD_FORCE, dev->base_inner + REG_CTL_MISC);
	wmb(); /* TBC */
	dev_info(dev->dev, "%s: 0x%x->0x%x\n", __func__,
		val, val | CTL_DB_LOAD_FORCE);
}

void apply_cq(struct mtk_raw_device *dev,
	      int initial, dma_addr_t cq_addr,
	      unsigned int cq_size, unsigned int cq_offset,
	      unsigned int sub_cq_size, unsigned int sub_cq_offset)
{
	/* note: legacy CQ just trigger 1st frame */
	int trigger = USINGSCQ || initial;
	dma_addr_t main, sub;

	MTK_CAM_TRACE_FUNC_BEGIN(BASIC);

	if (initial)
		dev_info(dev->dev,
			"apply 1st raw%d cq - addr:0x%llx ,size:%d/%d,offset:%d,cq_en(0x%x),period(%d)\n",
			dev->id, cq_addr, cq_size, sub_cq_size, sub_cq_offset,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_SCQ_START_PERIOD));
	else
		dev_dbg(dev->dev,
			"apply raw%d cq - addr:0x%llx ,size:%d/%d,offset:%d\n",
			dev->id, cq_addr, cq_size, sub_cq_size, sub_cq_offset);

	if (!dev->enable_hsf) {
		main = cq_addr + cq_offset;
		sub = cq_addr + sub_cq_offset;

		writel_relaxed(dmaaddr_lsb(main),
			       dev->base + REG_CQ_THR0_BASEADDR);
		writel_relaxed(dmaaddr_msb(main),
			       dev->base + REG_CQ_THR0_BASEADDR_MSB);
		writel_relaxed(cq_size,
			       dev->base + REG_CQ_THR0_DESC_SIZE);

		writel_relaxed(dmaaddr_lsb(sub),
			       dev->base + REG_CQ_SUB_THR0_BASEADDR_2);
		writel_relaxed(dmaaddr_msb(sub),
			       dev->base + REG_CQ_SUB_THR0_BASEADDR_MSB_2);
		writel_relaxed(sub_cq_size,
			       dev->base + REG_CQ_SUB_THR0_DESC_SIZE_2);

		if (trigger)
			writel(CTL_CQ_THR0_START, dev->base + REG_CTL_START);

		wmb(); /* make sure committed */
	} else {
#ifdef NOT_READY
		ccu_apply_cq(dev, cq_addr, cq_size, initial,
			     cq_offset, sub_cq_size, sub_cq_offset);
#endif
	}

	MTK_CAM_TRACE_END(BASIC);
}

/* sw check again for rawi dcif case */
bool is_dma_idle(struct mtk_raw_device *dev)
{
	bool ret = false;
	int chasing_stat;
	int raw_rst_stat = readl(dev->base + REG_DMA_SOFT_RST_STAT);
	int raw_rst_stat2 = readl(dev->base + REG_DMA_SOFT_RST_STAT2);
	int yuv_rst_stat = readl(dev->yuv_base + REG_DMA_SOFT_RST_STAT);

	if (raw_rst_stat2 != 0x7 || yuv_rst_stat != 0xfffffff)
		return false;

	/* check beside rawi_r2/r3/r5*/
	if (~raw_rst_stat & 0x7fffffda)
		return false;

	if (~raw_rst_stat & RST_STAT_RAWI_R2) { /* RAWI_R2 */
		chasing_stat = readl(dev->base + REG_DMA_DBG_CHASING_STATUS);
		ret = ((chasing_stat & RAWI_R2_SMI_REQ_ST) == 0 &&
		 (readl(dev->base + REG_RAWI_R2_BASE + DMA_OFFSET_SPECIAL_DCIF)
			& DC_CAMSV_STAGER_EN) &&
		 (readl(dev->base + REG_CTL_MOD6_EN) & 0x1))
			? true:false;
		dev_info(dev->dev, "%s: chasing_stat: 0x%llx ret=%d\n",
				__func__, chasing_stat, ret);
	}
	if (~raw_rst_stat & RST_STAT_RAWI_R3) {
		chasing_stat = readl(dev->base + REG_DMA_DBG_CHASING_STATUS);
		ret = ((chasing_stat & RAWI_R3_SMI_REQ_ST) == 0 &&
		 (readl(dev->base + REG_RAWI_R3_BASE + DMA_OFFSET_SPECIAL_DCIF)
			& DC_CAMSV_STAGER_EN) &&
		 (readl(dev->base + REG_CTL_MOD6_EN) & 0x80))
			? true:false;
		dev_info(dev->dev, "%s: chasing_stat: 0x%llx, ret=%d\n",
				__func__, chasing_stat, ret);
	}
	if (~raw_rst_stat & RST_STAT_RAWI_R5) {
		chasing_stat = readl(dev->base + REG_DMA_DBG_CHASING_STATUS2);
		ret = ((chasing_stat & RAWI_R5_SMI_REQ_ST) == 0 &&
		 (readl(dev->base + REG_RAWI_R5_BASE + DMA_OFFSET_SPECIAL_DCIF)
			& DC_CAMSV_STAGER_EN) &&
		 (readl(dev->base + REG_CTL_MOD6_EN) & 0x1000))
			? true:false;
		dev_info(dev->dev, "%s: chasing_stat: 0x%llx, ret=%d\n",
				__func__, chasing_stat, ret);
	}

	return ret;
}

void reset(struct mtk_raw_device *dev)
{
	int sw_ctl;
	int ret;

	//dev_dbg(dev->dev, "%s\n", __func__);

	/* Disable all DMA DCM before reset */
	writel(0x00000fff, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
	writel(0x0007ffff, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
	writel(0xffffffff, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);

	/* enable CQI_R1 ~ R4 before reset and make sure loaded to inner */
	writel(readl(dev->base + REG_CTL_MOD6_EN) | 0x78000,
	       dev->base + REG_CTL_MOD6_EN);
	toggle_db(dev);

	writel(0, dev->base + REG_CTL_SW_CTL);
	writel(1, dev->base + REG_CTL_SW_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, dev->base + REG_CTL_SW_CTL, sw_ctl,
				 sw_ctl & 0x2,
				 1 /* delay, us */,
				 5000 /* timeout, us */);

	if (ret < 0 && !is_dma_idle(dev)) {
		dev_info(dev->dev,
			 "%s: timeout: tg_sen_mode: 0x%x, ctl_en: 0x%x, mod6_en: 0x%x, ctl_sw_ctl:0x%x, frame_no:0x%x,rst_stat:0x%x,rst_stat2:0x%x,yuv_rst_stat:0x%x,cg_con:0x%x,sw_rst:0x%x\n",
			 __func__,
			 readl(dev->base + REG_TG_SEN_MODE),
			 readl(dev->base + REG_CTL_EN),
			 readl(dev->base + REG_CTL_MOD6_EN),
			 readl(dev->base + REG_CTL_SW_CTL),
			 readl(dev->base + REG_FRAME_SEQ_NUM),
			 readl(dev->base + REG_DMA_SOFT_RST_STAT),
			 readl(dev->base + REG_DMA_SOFT_RST_STAT2),
			 readl(dev->yuv_base + REG_DMA_SOFT_RST_STAT),
			 readl(dev->cam->base + REG_CAMSYS_CG_CON),
			 readl(dev->cam->base + REG_CAMSYS_SW_RST));

		/* check dma cmd cnt */
		mtk_cam_sw_reset_check(dev->dev, dev->base + CAMDMATOP_BASE,
			dbg_ulc_cmd_cnt, ARRAY_SIZE(dbg_ulc_cmd_cnt));
		mtk_cam_sw_reset_check(dev->dev, dev->base + CAMDMATOP_BASE,
			dbg_ori_cmd_cnt, ARRAY_SIZE(dbg_ori_cmd_cnt));

		mtk_smi_dbg_hang_detect("camsys");

		goto RESET_FAILURE;
	}

	/* do hw rst */
	writel(4, dev->base + REG_CTL_SW_CTL);
	writel(0, dev->base + REG_CTL_SW_CTL);

RESET_FAILURE:
	/* Enable all DMA DCM back */
	writel(0x0, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
	writel(0x0, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
	writel(0x0, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);

	wmb(); /* make sure committed */
}

static void reset_reg(struct mtk_raw_device *dev)
{
	int cq_en, sw_done, sw_sub_ctl;

	dev_dbg(dev->dev,
			 "[%s++] CQ_EN/SW_SUB_CTL/SW_DONE [in] 0x%x/0x%x/0x%x [out] 0x%x/0x%x/0x%x\n",
			 __func__,
			 readl_relaxed(dev->base_inner + REG_CQ_EN),
			 readl_relaxed(dev->base_inner + REG_CTL_SW_SUB_CTL),
			 readl_relaxed(dev->base_inner + REG_CTL_SW_PASS1_DONE),
			 readl_relaxed(dev->base + REG_CQ_EN),
			 readl_relaxed(dev->base + REG_CTL_SW_SUB_CTL),
			 readl_relaxed(dev->base + REG_CTL_SW_PASS1_DONE));

	cq_en = readl_relaxed(dev->base_inner + REG_CQ_EN);
	sw_done = readl_relaxed(dev->base_inner + REG_CTL_SW_PASS1_DONE);
	sw_sub_ctl = readl_relaxed(dev->base_inner + REG_CTL_SW_SUB_CTL);

	cq_en = cq_en & (~SCQ_SUBSAMPLE_EN) & (~SCQ_STAGGER_MODE);
	writel(cq_en, dev->base_inner + REG_CQ_EN);
	writel(cq_en, dev->base + REG_CQ_EN);

	dev_dbg(dev->dev, "[--] try to disable SCQ_STAGGER_MODE: CQ_EN(0x%x)\n",
		cq_en);

	writel(sw_done & (~SW_DONE_SAMPLE_EN), dev->base_inner + REG_CTL_SW_PASS1_DONE);
	writel(sw_done & (~SW_DONE_SAMPLE_EN), dev->base + REG_CTL_SW_PASS1_DONE);

	writel(0, dev->base_inner + REG_CTL_SW_SUB_CTL);
	writel(0, dev->base + REG_CTL_SW_SUB_CTL);

	wmb(); /* make sure committed */

	dev_dbg(dev->dev,
			 "[%s--] CQ_EN/SW_SUB_CTL/SW_DONE [in] 0x%x/0x%x/0x%x [out] 0x%x/0x%x/0x%x\n",
			 __func__,
			 readl_relaxed(dev->base_inner + REG_CQ_EN),
			 readl_relaxed(dev->base_inner + REG_CTL_SW_SUB_CTL),
			 readl_relaxed(dev->base_inner + REG_CTL_SW_PASS1_DONE),
			 readl_relaxed(dev->base + REG_CQ_EN),
			 readl_relaxed(dev->base + REG_CTL_SW_SUB_CTL),
			 readl_relaxed(dev->base + REG_CTL_SW_PASS1_DONE));
}

void dump_aa_info(struct mtk_cam_ctx *ctx, struct mtk_ae_debug_data *ae_info)
{
	// TODO
}

static int reset_msgfifo(struct mtk_raw_device *dev)
{
	atomic_set(&dev->is_fifo_overflow, 0);
	return kfifo_init(&dev->msg_fifo, dev->msg_buffer, dev->fifo_size);
}

static int push_msgfifo(struct mtk_raw_device *dev,
			struct mtk_camsys_irq_info *info)
{
	int len;

	if (unlikely(kfifo_avail(&dev->msg_fifo) < sizeof(*info))) {
		atomic_set(&dev->is_fifo_overflow, 1);
		return -1;
	}

	len = kfifo_in(&dev->msg_fifo, info, sizeof(*info));
	WARN_ON(len != sizeof(*info));

	return 0;
}

#define FIFO_THRESHOLD(FIFO_SIZE, HEIGHT_RATIO, LOW_RATIO) \
	(((FIFO_SIZE * HEIGHT_RATIO) & 0xFFF) << 16 | \
	((FIFO_SIZE * LOW_RATIO) & 0xFFF))

void set_fifo_threshold(void __iomem *dma_base)
{
	int fifo_size = 0;

	fifo_size = readl_relaxed(dma_base + DMA_OFFSET_CON0) & 0xFFF;

	writel_relaxed((0x1 << 28) | FIFO_THRESHOLD(fifo_size, 1/4, 1/8),
			dma_base + DMA_OFFSET_CON1);
	writel_relaxed((0x1 << 28) | FIFO_THRESHOLD(fifo_size, 1/2, 3/8),
			dma_base + DMA_OFFSET_CON2);
	writel_relaxed((0x1 << 31) | FIFO_THRESHOLD(fifo_size, 2/3, 13/24),
			dma_base + DMA_OFFSET_CON3);
	writel_relaxed((0x1 << 31) | FIFO_THRESHOLD(fifo_size, 3/8, 1/4),
			dma_base + DMA_OFFSET_CON4);
}

void toggle_db(struct mtk_raw_device *dev)
{
	int value;
	u32 val_cfg, val_dcif_ctl, val_sen;

	value = readl_relaxed(dev->base + REG_CTL_MISC);
	val_cfg = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
	val_dcif_ctl = readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL);
	val_sen = readl_relaxed(dev->base_inner + REG_TG_SEN_MODE);
	writel_relaxed(value & ~CTL_DB_EN, dev->base + REG_CTL_MISC);
	wmb(); /* TBC */
	writel_relaxed(value | CTL_DB_EN, dev->base + REG_CTL_MISC);
	wmb(); /* TBC */
	dev_info(dev->dev, "%s,  read inner AsIs->ToBe TG_PATH_CFG:0x%x->0x%x, TG_DCIF:0x%x->0x%x, TG_SEN:0x%x->0x%x\n",
		__func__,
			val_cfg,
			readl_relaxed(dev->base_inner + REG_TG_PATH_CFG),
			val_dcif_ctl,
			readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL),
			val_sen,
			readl_relaxed(dev->base_inner + REG_TG_SEN_MODE));
}

void enable_tg_db(struct mtk_raw_device *dev, int en)
{
	int value;
	u32 val_cfg, val_dcif_ctl, val_sen;

	value = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
	if (en == 0) { /*disable tg db*/
		val_cfg = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
		val_dcif_ctl = readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL);
		val_sen = readl_relaxed(dev->base_inner + REG_TG_SEN_MODE);
		writel_relaxed(value | TG_DB_LOAD_DIS, dev->base + REG_TG_PATH_CFG);
		wmb(); /* TBC */
		dev_dbg(dev->dev, "%s disable,  TG_PATH_CFG:0x%x->0x%x, TG_DCIF:0x%x->0x%x, TG_SEN:0x%x->0x%x\n",
		__func__,
			val_cfg,
			readl_relaxed(dev->base_inner + REG_TG_PATH_CFG),
			val_dcif_ctl,
			readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL),
			val_sen,
			readl_relaxed(dev->base_inner + REG_TG_SEN_MODE));
	} else { /*enable tg db*/
		writel_relaxed(value & ~TG_DB_LOAD_DIS, dev->base + REG_TG_PATH_CFG);
		wmb(); /* TBC */
		dev_dbg(dev->dev, "%s enable, TG_PATH_CFG:0x%x\n", __func__, value);
	}
}

static void init_dma_threshold(struct mtk_raw_device *dev)
{
	struct mtk_cam_device *cam_dev;
	struct mtk_yuv_device *yuv_dev = NULL; //get_yuv_dev(dev);
	bool is_srt = 0; //TODO: mtk_cam_is_srt(dev->pipeline->hw_mode);
	unsigned int raw_urgent, yuv_urgent;

	cam_dev = dev->cam;

	dev_info(dev->dev, "%s: SRT:%d\n", __func__, is_srt);

	set_fifo_threshold(dev->base + REG_IMGO_R1_BASE);
	set_fifo_threshold(dev->base + REG_FHO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAHO_R1_BASE);
	set_fifo_threshold(dev->base + REG_PDO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AFO_R1_BASE);
	set_fifo_threshold(dev->base + REG_LTMSO_R1_BASE);
	set_fifo_threshold(dev->base + REG_TSFSO_R1_BASE);
	set_fifo_threshold(dev->base + REG_TSFSO_R2_BASE);
	set_fifo_threshold(dev->base + REG_FLKO_R1_BASE);
	set_fifo_threshold(dev->base + REG_UFEO_R1_BASE);

	set_fifo_threshold(dev->yuv_base + REG_YUVO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVCO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVCO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R4_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R4_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R5_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R5_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TBO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_ACTSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSHO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSYO_R1_BASE);

	set_fifo_threshold(dev->base + REG_RAWI_R2_BASE);
	set_fifo_threshold(dev->base + REG_UFDI_R2_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R3_BASE);
	set_fifo_threshold(dev->base + REG_UFDI_R3_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R1_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R2_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R3_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R4_BASE);
	set_fifo_threshold(dev->base + REG_LSCI_R1_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R1_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R2_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R3_BASE);
	set_fifo_threshold(dev->base + REG_PDI_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAI_R1_BASE);
	set_fifo_threshold(dev->base + REG_CACI_R1_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R5_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R6_BASE);

	// TODO: move HALT1,2 to camsv
	writel_relaxed(CAMSV_1_WDMA_PORT, cam_dev->base + REG_HALT1_EN);
	writel_relaxed(CAMSV_2_WDMA_PORT, cam_dev->base + REG_HALT2_EN);

	switch (dev->id) {
	case MTKCAM_SUBDEV_RAW_0:
		raw_urgent = REG_HALT5_EN;
		yuv_urgent = REG_HALT6_EN;
		break;
	case MTKCAM_SUBDEV_RAW_1:
		raw_urgent = REG_HALT7_EN;
		yuv_urgent = REG_HALT8_EN;
		break;
	case MTKCAM_SUBDEV_RAW_2:
		raw_urgent = REG_HALT9_EN;
		yuv_urgent = REG_HALT10_EN;
		break;
	default:
		dev_info(dev->dev, "%s: unknown raw id %d\n", __func__, dev->id);
		return;
	}

	if (is_srt) {
		writel_relaxed(0x0, cam_dev->base + raw_urgent);
		writel_relaxed(0x0, cam_dev->base + yuv_urgent);

		mtk_smi_larb_ultra_dis(&dev->larb_pdev->dev, true);
		mtk_smi_larb_ultra_dis(&yuv_dev->larb_pdev->dev, true);
	} else {
		writel_relaxed(RAW_WDMA_PORT, cam_dev->base + raw_urgent);
		writel_relaxed(YUV_WDMA_PORT, cam_dev->base + yuv_urgent);

		//mtk_smi_larb_ultra_dis(&dev->larb_pdev->dev, false);
		//mtk_smi_larb_ultra_dis(&yuv_dev->larb_pdev->dev, false);
	}
	dev_info(dev->dev, "%s: end %d\n", __func__, dev->id);
}

/* TODO: check this */
int get_fps_ratio(int fps)
{
	int fps_ratio;

	return (fps + 30) / 30;
	if (fps <= 30)
		fps_ratio = 1;
	else if (fps <= 60)
		fps_ratio = 2;
	else
		fps_ratio = 1;

	return fps_ratio;
}

void stagger_enable(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_STAGGER_MODE, dev->base + REG_CQ_EN);
	wmb(); /* TBC */
	dev->stagger_en = 1;
	dev_dbg(dev->dev, "%s - CQ_EN:0x%x->0x%x, TG_PATH_CFG:0x%x, TG_DCIF:0x%x, TG_SEN:0x%x\n",
		__func__, val,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_TG_PATH_CFG),
			readl_relaxed(dev->base + REG_TG_DCIF_CTL),
			readl_relaxed(dev->base + REG_TG_SEN_MODE));

}

void stagger_disable(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val & (~SCQ_STAGGER_MODE), dev->base + REG_CQ_EN);
	wmb(); /* TBC */
	dev->stagger_en = 0;
	dev_dbg(dev->dev, "%s - CQ_EN:0x%x->0x%x, TG_PATH_CFG:0x%x, TG_DCIF:0x%x, TG_SEN:0x%x\n",
		__func__,
			val,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_TG_PATH_CFG),
			readl_relaxed(dev->base + REG_TG_DCIF_CTL),
			readl_relaxed(dev->base + REG_TG_SEN_MODE));
}

static void subsample_set_sensor_time(struct mtk_raw_device *dev)
{
	dev->sub_sensor_ctrl_en = true;
	dev->set_sensor_idx = dev->subsample_ratio - 1;
	dev->cur_vsync_idx = -1;
}

void subsample_enable(struct mtk_raw_device *dev, u32 ratio)
{
	u32 val;
	u32 sub_ratio = ratio;

	dev->subsample_ratio = sub_ratio;
	subsample_set_sensor_time(dev);

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_SUBSAMPLE_EN, dev->base + REG_CQ_EN);
	writel_relaxed(SW_DONE_SAMPLE_EN | sub_ratio,
		dev->base + REG_CTL_SW_PASS1_DONE);
	writel_relaxed(SW_DONE_SAMPLE_EN | sub_ratio,
		dev->base_inner + REG_CTL_SW_PASS1_DONE);
	wmb(); /* TBC */
	dev_dbg(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CTL_SW_PASS1_DONE:0x%8x, ratio:%d\n",
			__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CTL_SW_PASS1_DONE),
			sub_ratio);
}

void initialize(struct mtk_raw_device *dev, int is_slave)
{
#if USINGSCQ
	u32 val;

	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_EN | CQ_DROP_FRAME_EN, dev->base + REG_CQ_EN);

	//writel_relaxed(0x100010, dev->base + REG_CQ_EN);
	writel_relaxed(0xffffffff, dev->base + REG_SCQ_START_PERIOD);
#endif

	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_THR0_CTL);
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_SUB_THR0_CTL);
	writel_relaxed(CAMCTL_CQ_THR0_DONE_ST, dev->base + REG_CTL_RAW_INT6_EN);
	writel_relaxed(BIT(10), dev->base + REG_CTL_RAW_INT7_EN);

	dev->is_slave = is_slave;
	dev->sof_count = 0;
	dev->vsync_count = 0;
	dev->sub_sensor_ctrl_en = false;
	dev->time_shared_busy = 0;
	atomic_set(&dev->vf_en, 0);
	dev->stagger_en = 0;
	reset_msgfifo(dev);

	init_dma_threshold(dev);

	/* Workaround: disable FLKO error_sof: double sof error
	 *   HW will send FLKO dma error when
	 *      FLKO rcnt = 0 (not going to output this frame)
	 *      However, HW_PASS1_DONE still comes as expected
	 */
	writel_relaxed(0xFFFE0000,
		       dev->base + REG_FLKO_R1_BASE + DMA_OFFSET_ERR_STAT);

	dev_dbg(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CQ_THR0_CTL:0x%8x\n",
		__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CQ_THR0_CTL));

}

static void immediate_stream_off_log(struct mtk_raw_device *dev, char *reg_name,
				     void __iomem *base,
				     void __iomem *base_inner,
				     u32 offset, u32 cur_val, u32 cfg_val)
{
	u32 read_val, read_val_2;

	read_val = readl_relaxed(base_inner + offset);
	read_val_2 = readl_relaxed(base + offset);
	dev_dbg(dev->dev,
		"%s:%s: before: r(0x%x), w(0x%x), after:in(0x%llx:0x%x),out(0x%llx:0x%x)\n",
		__func__, reg_name, cur_val, cfg_val,
		base_inner + offset, read_val,
		base + offset, read_val_2);
}

void immediate_stream_off(struct mtk_raw_device *dev)

{
	u32 chk_val, cur_val, cfg_val;
	u32 offset;

	atomic_set(&dev->vf_en, 0);


	writel_relaxed(~CQ_THR0_EN, dev->base + REG_CQ_THR0_CTL);
	wmb(); /* make sure committed */

	/* Disable Double Buffer */
	offset = REG_TG_PATH_CFG;
	cur_val = readl_relaxed(dev->base + offset);
	cfg_val = cur_val | 0x100; /* clear TG_DB_LOAD_DIS */
	writel(cfg_val, dev->base + offset); // has double buffer
	wmb(); /* make sure committed */
	immediate_stream_off_log(dev, "TG_PATH_CFG", dev->base, dev->base_inner,
				 offset, cur_val, cfg_val);


	/* Disable MISC CTRL */
	offset = REG_CTL_MISC;
	cur_val = readl_relaxed(dev->base + offset);
	cfg_val = cur_val & ~CTL_DB_EN;
	writel_relaxed(cfg_val, dev->base + offset);
	wmb(); /* make sure committed */
	immediate_stream_off_log(dev, "CTL_MISC", dev->base, dev->base_inner,
				 offset, cur_val, cfg_val);

	/* Disable VF */
	offset = REG_TG_VF_CON;
	cur_val = readl_relaxed(dev->base + offset);
	cfg_val = cur_val & ~TG_VFDATA_EN;
	writel(cfg_val, dev->base + offset);
	wmb(); /* make sure committed */
	if (readx_poll_timeout(readl, dev->base_inner + offset,
			       chk_val, chk_val == cfg_val,
			       1 /* sleep, us */,
			       10000 /* timeout, us*/) < 0) {
		dev_info(dev->dev, "%s: wait vf off timeout: TG_VF_CON 0x%x\n",
			 __func__, chk_val);
		immediate_stream_off_log(dev, "TG_VF_CON", dev->base,
					 dev->base_inner, offset,
					 cur_val, cfg_val);
	} else {
		dev_dbg(dev->dev, "%s: VF OFF success\n", __func__);
	}

	/* Disable CMOS */
	offset = REG_TG_SEN_MODE;
	cur_val = readl_relaxed(dev->base + offset);
	cfg_val = cur_val & ~TG_SEN_MODE_CMOS_EN;
	writel(cfg_val, dev->base + offset);
	wmb(); /* make sure committed */
	immediate_stream_off_log(dev, "TG_SEN_MODE", dev->base, dev->base_inner,
				 offset, cur_val, cfg_val);

	reset_reg(dev);
	reset(dev);

	wmb(); /* make sure committed */

	/* Enable MISC CTRL */
	offset = REG_CTL_MISC;
	cur_val = readl_relaxed(dev->base + offset);
	cfg_val = cur_val | CTL_DB_EN;
	writel_relaxed(cfg_val, dev->base + offset);
	wmb(); /* make sure committed */
	immediate_stream_off_log(dev, "CTL_MISC", dev->base, dev->base_inner,
				 offset, cur_val, cfg_val);

	/* nable Double Buffer */
	offset = REG_TG_PATH_CFG;
	cur_val = readl_relaxed(dev->base + offset);
	cfg_val = cur_val & ~0x100;
	writel(cfg_val, dev->base + offset);
	wmb(); /* make sure committed */
	immediate_stream_off_log(dev, "TG_PATH_CFG", dev->base, dev->base_inner,
				 offset, cur_val, cfg_val);
}

void stream_on(struct mtk_raw_device *dev, int on,
	int scq_period_ms, bool pass_vf_en)
{
	u32 val;
	u32 chk_val;
	u32 cfg_val;
	u32 fps_ratio = 1;

	dev_dbg(dev->dev, "raw %d %s %d\n", dev->id, __func__, on);
	if (on) {
		if (!dev->enable_hsf) {
			val = readl_relaxed(dev->base + REG_TG_TIME_STAMP_CNT);
			val = (val == 0) ? 1 : val;
			fps_ratio = get_fps_ratio(dev->fps);
			dev_info(dev->dev, "VF on - TG_TIME_STAMP val:%d, fps:%d, ms:%d\n",
				 val, fps_ratio, scq_period_ms);
			writel_relaxed(scq_period_ms * 1000 * SCQ_DEFAULT_CLK_RATE /
				(val * 2) / fps_ratio, dev->base + REG_SCQ_START_PERIOD);

			mtk_cam_set_topdebug_rdyreq(dev->dev, dev->base, dev->yuv_base,
						    TG_OVERRUN);
			dev->overrun_debug_dump_cnt = 0;

			enable_tg_db(dev, 0);
			enable_tg_db(dev, 1);
			toggle_db(dev);
			if (pass_vf_en)
				dev_info(dev->dev, "[%s] M2M view finder disable\n", __func__);
			else {
				val = readl_relaxed(dev->base + REG_TG_VF_CON);
				val |= TG_VFDATA_EN;
				writel_relaxed(val, dev->base + REG_TG_VF_CON);
				wmb(); /* TBC */
			}
		} else {
			//TODO: ccu_stream_on(dev);
		}
		atomic_set(&dev->vf_en, 1);
		dev_dbg(dev->dev,
			"%s - CQ_EN:0x%x, CQ_THR0_CTL:0x%8x, TG_VF_CON:0x%8x, SCQ_START_PERIOD:%lld\n",
			__func__,
			readl_relaxed(dev->base + REG_CQ_EN),
			readl_relaxed(dev->base + REG_CQ_THR0_CTL),
			readl_relaxed(dev->base + REG_TG_VF_CON),
			readl_relaxed(dev->base + REG_SCQ_START_PERIOD));
	} else {
		dev_info(dev->dev, "VF off\n");
		atomic_set(&dev->vf_en, 0);

		writel_relaxed(~CQ_THR0_EN, dev->base + REG_CQ_THR0_CTL);
		wmb(); /* TBC */

		cfg_val = readl_relaxed(dev->base + REG_TG_PATH_CFG);
		cfg_val |= 0x100;
		writel(cfg_val, dev->base + REG_TG_PATH_CFG);

		val = readl_relaxed(dev->base + REG_TG_VF_CON);
		val &= ~TG_VFDATA_EN;
		writel(val, dev->base + REG_TG_VF_CON);

		cfg_val = readl_relaxed(dev->base + REG_TG_PATH_CFG);
		cfg_val &= ~0x100;
		writel(cfg_val, dev->base + REG_TG_PATH_CFG);

		//writel_relaxed(val, dev->base_inner + REG_CTL_EN);
		//writel_relaxed(val, dev->base_inner + REG_CTL_EN2);

		wmb(); /* make sure committed */
		enable_tg_db(dev, 0);
		enable_tg_db(dev, 1);

		dev_dbg(dev->dev,
			"%s - [Force VF off] TG_VF_CON outer:0x%8x inner:0x%8x\n",
			__func__, readl_relaxed(dev->base + REG_TG_VF_CON),
			readl_relaxed(dev->base_inner + REG_TG_VF_CON));

		if (readx_poll_timeout(readl, dev->base_inner + REG_TG_VF_CON,
				       chk_val, chk_val == val,
				       1 /* sleep, us */,
				       33000 /* timeout, us*/) < 0) {

			dev_info(dev->dev, "%s: wait vf off timeout: TG_VF_CON 0x%x\n",
				 __func__, chk_val);
		}
		reset_reg(dev);
	}
}

static void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev,
				       int dequeued_frame_seq_no);
static void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev,
				       int dequeued_frame_seq_no);
static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no);
static void raw_handle_error(struct mtk_raw_device *raw_dev,
			     struct mtk_camsys_irq_info *data)
{
	int err_status = data->e.err_status;
	int frame_idx_inner = data->frame_idx_inner;

	/* Show DMA errors in detail */
	if (err_status & DMA_ERR_ST) {

		/*
		 * mtk_cam_dump_topdebug_rdyreq(dev,
		 *                              raw_dev->base, raw_dev->yuv_base);
		 */

		raw_irq_handle_dma_err(raw_dev, frame_idx_inner);

		/*
		 * mtk_cam_dump_req_rdy_status(raw_dev->dev, raw_dev->base,
		 *		    raw_dev->yuv_base);
		 */

		/*
		 * mtk_cam_dump_dma_debug(raw_dev->dev, raw_dev->base + CAMDMATOP_BASE,
		 *                        "IMGO_R1",
		 *                        dbg_IMGO_R1, ARRAY_SIZE(dbg_IMGO_R1));
		 */
	}
	/* Show TG register for more error detail*/
	if (err_status & TG_GBERR_ST)
		raw_irq_handle_tg_grab_err(raw_dev, frame_idx_inner);

	if (err_status & TG_OVRUN_ST) {
		if (raw_dev->overrun_debug_dump_cnt < 4) {
			mtk_cam_dump_topdebug_rdyreq(raw_dev->dev,
				raw_dev->base, raw_dev->yuv_base);
			raw_dev->overrun_debug_dump_cnt++;
		} else {
			dev_dbg(raw_dev->dev, "%s: TG_OVRUN_ST repeated skip dump raw_id:%d\n",
				__func__, raw_dev->id);
		}
		raw_irq_handle_tg_overrun_err(raw_dev, frame_idx_inner);
	}

	if (err_status & INT_ST_MASK_CAM_DBG) {
		dev_info(raw_dev->dev, "%s: err_status:0x%x\n",
			__func__, err_status);
	}

}

static bool is_sub_sample_sensor_timing(struct mtk_raw_device *dev)
{
	return dev->cur_vsync_idx >= dev->set_sensor_idx;
}

static irqreturn_t mtk_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	struct device *dev = raw_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int frame_idx, frame_idx_inner, fbc_fho_ctl2;
	unsigned int irq_status, err_status, dmao_done_status, dmai_done_status;
	unsigned int drop_status, dma_ofl_status, cq_done_status, cq2_done_status;
	bool wake_thread = 0;

	irq_status	 = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT_STAT);
	dmao_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT2_STAT);
	dmai_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT3_STAT);
	drop_status	 = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT4_STAT);
	dma_ofl_status	 = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT5_STAT);
	cq_done_status	 = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT6_STAT);
	cq2_done_status	 = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT7_STAT);

	frame_idx	= readl_relaxed(raw_dev->base + REG_FRAME_SEQ_NUM);
	frame_idx_inner	= readl_relaxed(raw_dev->base_inner + REG_FRAME_SEQ_NUM);

	fbc_fho_ctl2 =
		readl_relaxed(REG_FBC_CTL2(raw_dev->base + FBC_R1A_BASE, 1));

	err_status = irq_status & INT_ST_MASK_CAM_ERR;

	//if (unlikely(debug_raw))
	//	dev_dbg(dev,
	//		"INT:0x%x(err:0x%x) 2~7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x (in:%d)\n",
	//		irq_status, err_status,
	//		dmao_done_status, dmai_done_status, drop_status,
	//		dma_ofl_status, cq_done_status, cq2_done_status,
	//		frame_idx_inner);

#ifdef TO_REMOVE
	if (unlikely(!raw_dev->pipeline || !raw_dev->pipeline->enabled_raw)) {
		dev_dbg(dev, "%s: %i: raw pipeline is disabled\n",
			__func__, raw_dev->id);
		goto ctx_not_found;
	}
#endif

	/* SRT err interrupt */
	if (irq_status & P1_DONE_OVER_SOF_INT_ST)
		dev_dbg(dev, "P1_DONE_OVER_SOF_INT_ST");

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = frame_idx;
	irq_info.frame_idx_inner = frame_idx_inner;

	/* CQ done */
	if (cq_done_status & CAMCTL_CQ_THR0_DONE_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_SETTING_DONE;
	}
	/* DMAO done, only for AFO */
	if (dmao_done_status & AFO_DONE_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_AFO_DONE;
		/* enable AFO_DONE_EN at backend manually */
	}

	/* Frame skipped */
	if (irq_status & P1_SKIP_FRAME_INT_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_SKIPPED;

	/* Frame done */
	if (irq_status & SW_PASS1_DON_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_DONE;
		raw_dev->overrun_debug_dump_cnt = 0;
	}
	/* Frame start */
	if (irq_status & SOF_INT_ST || irq_status & DCIF_SUB_SOF_INT_EN) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START;
		raw_dev->sof_count++;
		raw_dev->cur_vsync_idx = 0;
		raw_dev->last_sof_time_ns = irq_info.ts_ns;
		irq_info.write_cnt = ((fbc_fho_ctl2 & WCNT_BIT_MASK) >> 8) - 1;
		irq_info.fbc_cnt = (fbc_fho_ctl2 & CNT_BIT_MASK) >> 16;
	}

	/* DCIF main sof */
	if (irq_status & DCIF_SOF_INT_EN)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START_DCIF_MAIN;

	/* Vsync interrupt */
	if (irq_status & VS_INT_ST)
		raw_dev->vsync_count++;

	if (raw_dev->sub_sensor_ctrl_en && irq_status & TG_VS_INT_ORG_ST
	    && raw_dev->cur_vsync_idx >= 0) {
		if (is_sub_sample_sensor_timing(raw_dev)) {
			raw_dev->cur_vsync_idx = -1;
			irq_info.irq_type |= 1 << CAMSYS_IRQ_TRY_SENSOR_SET;
		}
		++raw_dev->cur_vsync_idx;
	}

	if (irq_info.irq_type && !raw_dev->is_slave) {
		if (push_msgfifo(raw_dev, &irq_info) == 0)
			wake_thread = 1;
	}

	/* Check ISP error status */
	if (unlikely(err_status)) {
		struct mtk_camsys_irq_info err_info;

		err_info.irq_type = 1 << CAMSYS_IRQ_ERROR;
		err_info.ts_ns = irq_info.ts_ns;
		err_info.frame_idx = irq_info.frame_idx;
		err_info.frame_idx_inner = irq_info.frame_idx_inner;
		err_info.e.err_status = err_status;

		if (push_msgfifo(raw_dev, &err_info) == 0)
			wake_thread = 1;
	}

	/* enable to debug fbc related */
	//if (debug_raw && debug_dump_fbc && (irq_status & SOF_INT_ST))
	//	mtk_cam_raw_dump_fbc(raw_dev->dev, raw_dev->base, raw_dev->yuv_base);

	/* trace */
	if (irq_status || dmao_done_status || dmai_done_status)
		MTK_CAM_TRACE(HW_IRQ,
			      "%s: irq=0x%08x dmao=0x%08x dmai=0x%08x has_err=%d",
			      dev_name(dev),
			      irq_status, dmao_done_status, dmai_done_status,
			      !!err_status);

	if (MTK_CAM_TRACE_ENABLED(FBC) && (irq_status & TG_VS_INT_ORG_ST)) {
#ifdef DUMP_FBC_SEL_OUTER
		MTK_CAM_TRACE(FBC, "frame %d FBC_SEL 0x% 8x/0x% 8x (outer)",
			irq_info.frame_idx_inner,
			readl_relaxed(raw_dev->base + REG_CAMCTL_FBC_SEL),
			readl_relaxed(raw_dev->yuv_base + REG_CAMCTL_FBC_SEL));
#endif
		mtk_cam_raw_dump_fbc(raw_dev->dev, raw_dev->base, raw_dev->yuv_base);
	}

	if (drop_status)
		MTK_CAM_TRACE(HW_IRQ, "%s: drop=0x%08x",
			      dev_name(dev), drop_status);

	if (cq_done_status || cq2_done_status)
		MTK_CAM_TRACE(HW_IRQ, "%s: cq=0x%08x 0x%08x",
			      dev_name(dev), cq_done_status, cq2_done_status);

#ifdef TO_REMOVE
ctx_not_found:
#endif

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_thread_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	struct mtk_camsys_irq_info irq_info;

	if (unlikely(atomic_cmpxchg(&raw_dev->is_fifo_overflow, 1, 0)))
		dev_info(raw_dev->dev, "msg fifo overflow\n");

	while (kfifo_len(&raw_dev->msg_fifo) >= sizeof(irq_info)) {
		int len = kfifo_out(&raw_dev->msg_fifo, &irq_info, sizeof(irq_info));

		WARN_ON(len != sizeof(irq_info));

		dev_dbg(raw_dev->dev, "ts=%lu irq_type %d, req:%d/%d\n",
			irq_info.ts_ns / 1000,
			irq_info.irq_type,
			irq_info.frame_idx_inner,
			irq_info.frame_idx);

		/* error case */
		if (unlikely(irq_info.irq_type == (1 << CAMSYS_IRQ_ERROR))) {
			raw_handle_error(raw_dev, &irq_info);
			continue;
		}

		/* normal case */

		/* inform interrupt information to camsys controller */
		mtk_cam_ctrl_isr_event(raw_dev->cam,
				     CAMSYS_ENGINE_RAW, raw_dev->id,
				     &irq_info);
	}

	return IRQ_HANDLED;
}

void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev,
				int dequeued_frame_seq_no)
{
	// TODO
	dev_info_ratelimited(raw_dev->dev,
		"%d Grab_Err [Outter] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		dequeued_frame_seq_no,
		readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
		readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
		readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
		readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
		readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
		readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
	dev_info_ratelimited(raw_dev->dev,
		"%d Grab_Err [Inner] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		dequeued_frame_seq_no,
		readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG),
		readl_relaxed(raw_dev->base_inner + REG_TG_SEN_MODE),
		readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST),
		readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST_R),
		readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_PXL),
		readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_LIN));
}

void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev, int dequeued_frame_seq_no)
{
	// TODO
	dev_info(raw_dev->dev,
			 "%s: dequeued_frame_seq_no %d\n",
			 __func__, dequeued_frame_seq_no);
	mtk_cam_raw_dump_dma_err_st(raw_dev->dev, raw_dev->base);
	mtk_cam_yuv_dump_dma_err_st(raw_dev->dev, raw_dev->yuv_base);
	if (raw_dev->stagger_en)
		mtk_cam_dump_dma_debug(raw_dev->dev,
				       raw_dev->base + CAMDMATOP_BASE,
				       "RAWI_R2",
				       dbg_RAWI_R2, ARRAY_SIZE(dbg_RAWI_R2));
}

static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no)
{
	// TODO
	dev_info(raw_dev->dev,
		 "%d Overrun_Err [Outter] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		 dequeued_frame_seq_no,
		 readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
		 readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
		 readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
		 readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
		 readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
		 readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
	dev_info(raw_dev->dev,
		 "%d Overrun_Err [Inner] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		 dequeued_frame_seq_no,
		 readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG),
		 readl_relaxed(raw_dev->base_inner + REG_TG_SEN_MODE),
		 readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST),
		 readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST_R),
		 readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_PXL),
		 readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_LIN));
	dev_info(raw_dev->dev,
		 "REQ RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_REQ_STAT));
	dev_info(raw_dev->dev,
		 "RDY RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_RDY_STAT));
	dev_info(raw_dev->dev,
		 "REQ YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_REQ_STAT));
	dev_info(raw_dev->dev,
		 "RDY YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_RDY_STAT));
}

static int mtk_raw_pm_suspend_prepare(struct mtk_raw_device *dev)
{
	u32 val;
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_dbg(dev->dev, "cam suspend, disable VF\n");
	val = readl(dev->base + REG_TG_VF_CON);
	writel(val & (~TG_VFDATA_EN), dev->base + REG_TG_VF_CON);
	ret = readl_poll_timeout_atomic(dev->base + REG_TG_INTER_ST, val,
					(val & TG_CAM_CS_MASK) == TG_IDLE_ST,
					USEC_PER_MSEC, MTK_RAW_STOP_HW_TIMEOUT);
	if (ret)
		dev_dbg(dev->dev, "can't stop HW:%d:0x%x\n", ret, val);

	/* Disable CMOS */
	val = readl(dev->base + REG_TG_SEN_MODE);
	writel(val & (~TG_SEN_MODE_CMOS_EN), dev->base + REG_TG_SEN_MODE);

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev->dev);
	return ret;
}

static int mtk_raw_pm_post_suspend(struct mtk_raw_device *dev)
{
	u32 val;
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev->dev);
	if (ret)
		return ret;

	/* Enable CMOS */
	dev_dbg(dev->dev, "cam resume, enable CMOS/VF\n");
	val = readl(dev->base + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, dev->base + REG_TG_SEN_MODE);

	/* Enable VF */
	val = readl(dev->base + REG_TG_VF_CON);
	writel(val | TG_VFDATA_EN, dev->base + REG_TG_VF_CON);

	return 0;
}

static int raw_pm_notifier(struct notifier_block *nb,
							unsigned long action, void *data)
{
	struct mtk_raw_device *raw_dev =
			container_of(nb, struct mtk_raw_device, pm_notifier);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		mtk_raw_pm_suspend_prepare(raw_dev);
		break;
	case PM_POST_SUSPEND:
		mtk_raw_pm_post_suspend(raw_dev);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int mtk_raw_of_probe(struct platform_device *pdev,
			    struct mtk_raw_device *raw)
{
	struct device *dev = &pdev->dev;
	struct platform_device *larb_pdev;
	struct device_node *larb_node;
	struct device_link *link;
	struct resource *res;
	unsigned int i;
	int clks, larbs, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &raw->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_info(dev, "%s: No suitable DMA available\n", __func__);

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			dev_info(dev, "Failed to set DMA segment size\n");
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(raw->base);
	}

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(raw->base_inner);
	}

	/* will be assigned later */
	raw->yuv_base = NULL;

	raw->irq = platform_get_irq(pdev, 0);
	if (raw->irq < 0) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(dev, raw->irq,
					mtk_irq_raw,
					mtk_thread_irq_raw,
					0, dev_name(dev), raw);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", raw->irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", raw->irq);

	disable_irq(raw->irq);

	clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	raw->num_clks = (clks == -ENOENT) ? 0:clks;
	dev_info(dev, "clk_num:%d\n", raw->num_clks);

	if (raw->num_clks) {
		raw->clks = devm_kcalloc(dev, raw->num_clks, sizeof(*raw->clks),
					 GFP_KERNEL);
		if (!raw->clks)
			return -ENOMEM;
	}

	for (i = 0; i < raw->num_clks; i++) {
		raw->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(raw->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	larbs = of_count_phandle_with_args(
					pdev->dev.of_node, "mediatek,larbs", NULL);
	dev_info(dev, "larb_num:%d\n", larbs);

	raw->larb_pdev = NULL;
	for (i = 0; i < larbs; i++) {
		larb_node = of_parse_phandle(
					pdev->dev.of_node, "mediatek,larbs", i);
		if (!larb_node) {
			dev_info(dev, "failed to get larb id\n");
			continue;
		}

		larb_pdev = of_find_device_by_node(larb_node);
		if (WARN_ON(!larb_pdev)) {
			of_node_put(larb_node);
			dev_info(dev, "failed to get larb pdev\n");
			continue;
		}
		of_node_put(larb_node);

		link = device_link_add(&pdev->dev, &larb_pdev->dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			dev_info(dev, "unable to link smi larb%d\n", i);
		else
			raw->larb_pdev = larb_pdev;
	}

#ifdef CONFIG_PM_SLEEP
	raw->pm_notifier.notifier_call = raw_pm_notifier;
	ret = register_pm_notifier(&raw->pm_notifier);
	if (ret) {
		dev_info(dev, "failed to register notifier block.\n");
		return ret;
	}
#endif
	return 0;
}

static int mtk_raw_component_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;

	raw_dev->cam = cam_dev;
	return mtk_cam_set_dev_raw(cam_dev->dev, raw_dev->id, dev, NULL);
}

static void mtk_raw_component_unbind(struct device *dev, struct device *master,
				     void *data)
{
}

static const struct component_ops mtk_raw_component_ops = {
	.bind = mtk_raw_component_bind,
	.unbind = mtk_raw_component_unbind,
};

static int mtk_raw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_raw_device *raw_dev;
	int ret;

	//dev_info(dev, "%s\n", __func__);

	raw_dev = devm_kzalloc(dev, sizeof(*raw_dev), GFP_KERNEL);
	if (!raw_dev)
		return -ENOMEM;

	raw_dev->dev = dev;
	dev_set_drvdata(dev, raw_dev);

	ret = mtk_raw_of_probe(pdev, raw_dev);
	if (ret)
		return ret;

	ret = mtk_cam_qos_probe(dev, &raw_dev->qos,
				qos_raw_ids,
				ARRAY_SIZE(qos_raw_ids));
	if (ret)
		return ret;

	raw_dev->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_camsys_irq_info));

	raw_dev->msg_buffer = devm_kzalloc(dev, raw_dev->fifo_size, GFP_KERNEL);
	if (!raw_dev->msg_buffer)
		return -ENOMEM;

	raw_dev->default_printk_cnt = get_detect_count();

	pm_runtime_enable(dev);

	return component_add(dev, &mtk_raw_component_ops);
}

static int mtk_raw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	int i;

	//dev_info(dev, "%s\n", __func__);

	unregister_pm_notifier(&raw_dev->pm_notifier);

	pm_runtime_disable(dev);
	mtk_cam_qos_remove(&raw_dev->qos);
	component_del(dev, &mtk_raw_component_ops);

	for (i = 0; i < raw_dev->num_clks; i++)
		clk_put(raw_dev->clks[i]);

	return 0;
}

static int mtk_raw_runtime_suspend(struct device *dev)
{
	struct mtk_raw_device *drvdata = dev_get_drvdata(dev);
	int i;
	unsigned int pr_detect_count;

	dev_info(dev, "%s:disable clock\n", __func__);
	dev_dbg(dev, "%s:drvdata->default_printk_cnt = %d\n", __func__,
			drvdata->default_printk_cnt);

	// disable_irq(drvdata->irq);
	pr_detect_count = get_detect_count();
	if (pr_detect_count > drvdata->default_printk_cnt)
		set_detect_count(drvdata->default_printk_cnt);

	// reset(drvdata);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_disable_unprepare(drvdata->clks[i]);

	return 0;
}

static int mtk_raw_runtime_resume(struct device *dev)
{
	struct mtk_raw_device *drvdata = dev_get_drvdata(dev);
	int i, ret;
	unsigned int pr_detect_count;

	/* reset_msgfifo before enable_irq */
	ret = reset_msgfifo(drvdata);
	if (ret)
		return ret;

	enable_irq(drvdata->irq);
	dev_dbg(dev, "%s:drvdata->default_printk_cnt = %d\n", __func__,
			drvdata->default_printk_cnt);

	pr_detect_count = get_detect_count();
	if (pr_detect_count < KERNEL_LOG_MAX)
		set_detect_count(KERNEL_LOG_MAX);

	dev_info(dev, "%s:enable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++) {
		ret = clk_prepare_enable(drvdata->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(drvdata->clks[i--]);

			return ret;
		}
	}

	reset(drvdata);

	return 0;
}

static const struct dev_pm_ops mtk_raw_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_raw_runtime_suspend, mtk_raw_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_raw_of_ids[] = {
	{.compatible = "mediatek,cam-raw",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_raw_of_ids);

struct platform_driver mtk_cam_raw_driver = {
	.probe   = mtk_raw_probe,
	.remove  = mtk_raw_remove,
	.driver  = {
		.name  = "mtk-cam raw",
		.of_match_table = of_match_ptr(mtk_raw_of_ids),
		.pm     = &mtk_raw_pm_ops,
	}
};

static int mtk_yuv_component_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;

	dev_info(dev, "%s: id=%d\n", __func__, drvdata->id);
	return mtk_cam_set_dev_raw(cam_dev->dev, drvdata->id, NULL, dev);
}

static void mtk_yuv_component_unbind(struct device *dev, struct device *master,
				     void *data)
{
}

static const struct component_ops mtk_yuv_component_ops = {
	.bind = mtk_yuv_component_bind,
	.unbind = mtk_yuv_component_unbind,
};

static irqreturn_t mtk_irq_yuv(int irq, void *data)
{
	struct mtk_yuv_device *drvdata = (struct mtk_yuv_device *)data;
	//struct device *dev = drvdata->dev;

	unsigned int irq_status, err_status, dma_done_status;
	unsigned int drop_status, dma_ofl_status;

	irq_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT_STAT);
	dma_done_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT2_STAT);
	drop_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT4_STAT);
	dma_ofl_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT5_STAT);

	err_status = irq_status & 0x4; // bit2: DMA_ERR

	//if (unlikely(debug_raw))
	//	dev_dbg(dev, "YUV-INT:0x%x(err:0x%x) INT2/4/5 0x%x/0x%x/0x%x\n",
	//		irq_status, err_status,
	//		dma_done_status, drop_status, dma_ofl_status);

	/* trace */
	if (irq_status || dma_done_status)
		MTK_CAM_TRACE(HW_IRQ, "%s: irq=0x%08x dmao=0x%08x has_err=%d",
			      dev_name(dev),
			      irq_status, dma_done_status, !!err_status);

	if (drop_status)
		MTK_CAM_TRACE(HW_IRQ, "%s: drop=0x%08x",
			      dev_name(dev), drop_status);

	return IRQ_HANDLED;
}

static int mtk_yuv_pm_suspend_prepare(struct mtk_yuv_device *dev)
{
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev->dev);
	return ret;
}

static int mtk_yuv_pm_post_suspend(struct mtk_yuv_device *dev)
{
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev->dev);
	if (ret)
		return ret;

	return 0;
}

static int yuv_pm_notifier(struct notifier_block *nb,
							unsigned long action, void *data)
{
	struct mtk_yuv_device *yuv_dev =
			container_of(nb, struct mtk_yuv_device, pm_notifier);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		mtk_yuv_pm_suspend_prepare(yuv_dev);
		break;
	case PM_POST_SUSPEND:
		mtk_yuv_pm_post_suspend(yuv_dev);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int mtk_yuv_of_probe(struct platform_device *pdev,
			    struct mtk_yuv_device *drvdata)
{
	struct device *dev = &pdev->dev;
	struct platform_device *larb_pdev;
	struct device_node *larb_node;
	struct device_link *link;
	struct resource *res;
	unsigned int i;
	int irq, clks, larbs, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &drvdata->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_info(dev, "%s: No suitable DMA available\n", __func__);

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			dev_info(dev, "Failed to set DMA segment size\n");
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	drvdata->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(drvdata->base);
	}

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	drvdata->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(drvdata->base_inner);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_yuv, 0,
			       dev_name(dev), drvdata);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);


	clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	drvdata->num_clks  = (clks == -ENOENT) ? 0:clks;
	dev_info(dev, "clk_num:%d\n", drvdata->num_clks);

	if (drvdata->num_clks) {
		drvdata->clks = devm_kcalloc(dev,
					     drvdata->num_clks, sizeof(*drvdata->clks),
					     GFP_KERNEL);
		if (!drvdata->clks)
			return -ENOMEM;
	}

	for (i = 0; i < drvdata->num_clks; i++) {
		drvdata->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(drvdata->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	larbs = of_count_phandle_with_args(
					pdev->dev.of_node, "mediatek,larbs", NULL);
	dev_info(dev, "larb_num:%d\n", larbs);

	drvdata->larb_pdev = NULL;
	for (i = 0; i < larbs; i++) {
		larb_node = of_parse_phandle(
					pdev->dev.of_node, "mediatek,larbs", i);
		if (!larb_node) {
			dev_info(dev, "failed to get larb id\n");
			continue;
		}

		larb_pdev = of_find_device_by_node(larb_node);
		if (WARN_ON(!larb_pdev)) {
			of_node_put(larb_node);
			dev_info(dev, "failed to get larb pdev\n");
			continue;
		}
		of_node_put(larb_node);

		link = device_link_add(dev, &larb_pdev->dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			dev_info(dev, "unable to link smi larb%d\n", i);
		else
			drvdata->larb_pdev = larb_pdev;
	}

#ifdef CONFIG_PM_SLEEP
	drvdata->pm_notifier.notifier_call = yuv_pm_notifier;
	ret = register_pm_notifier(&drvdata->pm_notifier);
	if (ret) {
		dev_info(dev, "failed to register notifier block.\n");
		return ret;
	}
#endif

	return 0;
}

static int mtk_yuv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_yuv_device *drvdata;
	int ret;

	//dev_info(dev, "%s\n", __func__);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	dev_set_drvdata(dev, drvdata);

	ret = mtk_yuv_of_probe(pdev, drvdata);
	if (ret) {
		dev_info(dev, "mtk_yuv_of_probe failed\n");
		return ret;
	}

	ret = mtk_cam_qos_probe(dev, &drvdata->qos,
				qos_yuv_ids,
				ARRAY_SIZE(qos_yuv_ids));
	if (ret)
		return ret;

	pm_runtime_enable(dev);

	return component_add(dev, &mtk_yuv_component_ops);
}

static int mtk_yuv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int i;

	//dev_info(dev, "%s\n", __func__);

	unregister_pm_notifier(&drvdata->pm_notifier);

	pm_runtime_disable(dev);
	mtk_cam_qos_remove(&drvdata->qos);
	component_del(dev, &mtk_yuv_component_ops);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_put(drvdata->clks[i]);

	return 0;
}

/* driver for yuv part */
static int mtk_yuv_runtime_suspend(struct device *dev)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s:disable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_disable_unprepare(drvdata->clks[i]);

	return 0;
}

static int mtk_yuv_runtime_resume(struct device *dev)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int i, ret;

	dev_info(dev, "%s:enable clock\n", __func__);

	for (i = 0; i < drvdata->num_clks; i++) {
		ret = clk_prepare_enable(drvdata->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(drvdata->clks[i--]);

			return ret;
		}
	}

	return 0;
}

int mtk_cam_translation_fault_callback(int port, dma_addr_t mva, void *data)
{
	// TODO
	return 0;
}

static const struct dev_pm_ops mtk_yuv_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_yuv_runtime_suspend, mtk_yuv_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_yuv_of_ids[] = {
	{.compatible = "mediatek,cam-yuv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_yuv_of_ids);

struct platform_driver mtk_cam_yuv_driver = {
	.probe   = mtk_yuv_probe,
	.remove  = mtk_yuv_remove,
	.driver  = {
		.name  = "mtk-cam yuv",
		.of_match_table = of_match_ptr(mtk_yuv_of_ids),
		.pm     = &mtk_yuv_pm_ops,
	}
};
