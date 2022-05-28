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
#include "mtk_cam-raw_regs.h"
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

#define REG_FLKO_R1_BASE	0x4900
#define DMA_OFFSET_ERR_STAT	0x34

static int reset_msgfifo(struct mtk_raw_device *dev);

void initialize(struct mtk_raw_device *dev, int is_slave)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CAMCQ_CQ_EN);
	SET_FIELD(&val, CAMCQ_CQ_DROP_FRAME_EN, 1);
	writel_relaxed(val, dev->base + REG_CAMCQ_CQ_EN);

	writel_relaxed(0xffffffff, dev->base + REG_CAMCQ_SCQ_START_PERIOD);

	val = FBIT(CAMCQ_CQ_THR0_EN);
	SET_FIELD(&val, CAMCQ_CQ_THR0_MODE, 1);

	writel_relaxed(val, dev->base + REG_CAMCQ_CQ_THR0_CTL);
	writel_relaxed(val, dev->base + REG_CAMCQ_CQ_SUB_THR0_CTL);

	/* enable interrupt */
	val = FBIT(CAMCTL_CQ_THR0_DONE_EN) | FBIT(CAMCTL_CQ_THRSUB_DONE_EN);
	writel_relaxed(val, dev->base + REG_CAMCTL_INT6_EN);

	dev->is_slave = is_slave;
	dev->sof_count = 0;
	dev->vsync_count = 0;
	dev->sub_sensor_ctrl_en = false;
	dev->time_shared_busy = 0;
	atomic_set(&dev->vf_en, 0);
	dev->stagger_en = 0;
	reset_msgfifo(dev);

#ifdef DISABLE_FLKO_ERROR
	/* Workaround: disable FLKO error_sof: double sof error
	 *   HW will send FLKO dma error when
	 *      FLKO rcnt = 0 (not going to output this frame)
	 *      However, HW_PASS1_DONE still comes as expected
	 */
	writel_relaxed(0xFFFE0000,
		       dev->base + REG_FLKO_R1_BASE + DMA_OFFSET_ERR_STAT);
#endif
}

void subsample_enable(struct mtk_raw_device *dev/*, int subsample_ratio*/)
{
	u32 val;
	u32 sub_ratio = 0;

	if (WARN_ON_ONCE(1))
		dev_info(dev->dev, "not ready\n");

	val = readl_relaxed(dev->base + REG_CAMCQ_CQ_EN);
	SET_FIELD(&val, CAMCQ_SCQ_SUBSAMPLE_EN, 1);
	writel_relaxed(val, dev->base + REG_CAMCQ_CQ_EN);

	val = FBIT(CAMCTL_DOWN_SAMPLE_EN);
	SET_FIELD(&val, CAMCTL_DOWN_SAMPLE_PERIOD, sub_ratio);
	writel_relaxed(val, dev->base + REG_CAMCTL_SW_PASS1_DONE);
	writel_relaxed(val, dev->base_inner + REG_CAMCTL_SW_PASS1_DONE);
}

/* TODO: cq_set_stagger_mode(dev, 0/1) */
void stagger_enable(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CAMCQ_CQ_EN);
	SET_FIELD(&val, CAMCQ_SCQ_STAGGER_MODE, 1);
	writel_relaxed(val, dev->base + REG_CAMCQ_CQ_EN);

	dev->stagger_en = 1;
}

void stagger_disable(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl_relaxed(dev->base + REG_CAMCQ_CQ_EN);
	SET_FIELD(&val, CAMCQ_SCQ_STAGGER_MODE, 0);
	writel_relaxed(val, dev->base + REG_CAMCQ_CQ_EN);

	dev->stagger_en = 0;
}

void apply_cq(struct mtk_raw_device *dev,
	      int initial, dma_addr_t cq_addr,
	      unsigned int cq_size, unsigned int cq_offset,
	      unsigned int sub_cq_size, unsigned int sub_cq_offset)
{
	dma_addr_t main, sub;

	dev_dbg(dev->dev,
		"apply raw%d cq - addr:0x%llx ,size:%d/%d,offset:%d\n",
		dev->id, cq_addr, cq_size, sub_cq_size, sub_cq_offset);

	main = cq_addr + cq_offset;
	sub = cq_addr + sub_cq_offset;

	writel_relaxed(dmaaddr_lsb(main),
		       dev->base + REG_CAMCQ_CQ_THR0_BASEADDR);
	writel_relaxed(dmaaddr_msb(main),
		       dev->base + REG_CAMCQ_CQ_THR0_BASEADDR_MSB);
	writel_relaxed(cq_size,
		       dev->base + REG_CAMCQ_CQ_THR0_DESC_SIZE);

	writel_relaxed(dmaaddr_lsb(sub),
		       dev->base + REG_CAMCQ_CQ_SUB_THR0_BASEADDR_2);
	writel_relaxed(dmaaddr_msb(sub),
		       dev->base + REG_CAMCQ_CQ_SUB_THR0_BASEADDR_2_MSB);
	writel_relaxed(sub_cq_size,
		       dev->base + REG_CAMCQ_CQ_SUB_THR0_DESC_SIZE_2);

	writel(FBIT(CAMCTL_CQ_THR0_START), dev->base + REG_CAMCTL_START);
	wmb(); /* make sure committed */
}

void dbload_force(struct mtk_raw_device *dev)
{
	if (WARN_ON_ONCE(1))
		dev_info(dev->dev, "not ready\n");
}

void toggle_db(struct mtk_raw_device *dev)
{
	u32 val;

	val = readl(dev->base + REG_CAMCTL_MISC);
	writel(val & ~FBIT(CAMCTL_DB_EN), dev->base + REG_CAMCTL_MISC);

	/* read back to make sure committed */
	val = readl(dev->base + REG_CAMCTL_MISC);
	writel(val | FBIT(CAMCTL_DB_EN), dev->base + REG_CAMCTL_MISC);
}

void enable_tg_db(struct mtk_raw_device *dev, int en)
{
	u32 val;

	val = readl(dev->base + REG_TG_PATH_CFG);
	SET_FIELD(&val, TG_DB_LOAD_DIS, !en);
	writel(val, dev->base + REG_TG_PATH_CFG);
}

void stream_on(struct mtk_raw_device *dev, int on)
{
	u32 val;

	/* toggle db before stream-on */
	enable_tg_db(dev, 0);
	enable_tg_db(dev, 1);
	toggle_db(dev);

	val = readl_relaxed(dev->base + REG_TG_VF_CON);
	SET_FIELD(&val, TG_VFDATA_EN, on);
	writel_relaxed(val, dev->base + REG_TG_VF_CON);
	wmb(); /* make sure committed */
}

void immediate_stream_off(struct mtk_raw_device *dev)
{
	if (WARN_ON_ONCE(1))
		dev_info(dev->dev, "not ready\n");
}

void trigger_rawi(struct mtk_raw_device *dev, signed int hw_scene)
{
	if (WARN_ON_ONCE(1))
		dev_info(dev->dev, "not ready\n");
}

void reset(struct mtk_raw_device *dev)
{
	int sw_ctl;
	u32 val;
	int ret;

	/* Disable all DMA DCM before reset */
	writel(0xffffffff, dev->base + REG_CAMCTL_MOD5_DCM_DIS);
	writel(0xffffffff, dev->base + REG_CAMCTL_MOD6_DCM_DIS);
	writel(0xffffffff, dev->yuv_base + REG_CAMCTL2_MOD5_DCM_DIS);
	writel(0xffffffff, dev->yuv_base + REG_CAMCTL2_MOD6_DCM_DIS);

	/* enable CQI_R1 ~ R4 before reset and make sure loaded to inner */
	val = readl(dev->base + REG_CAMCTL_MOD6_EN)
		| FBIT(CAMCTL_CQI_R1_EN)
		| FBIT(CAMCTL_CQI_R2_EN)
		| FBIT(CAMCTL_CQI_R3_EN)
		| FBIT(CAMCTL_CQI_R4_EN);
	writel(val, dev->base + REG_CAMCTL_MOD6_EN);

	toggle_db(dev);

	writel(0, dev->base + REG_CAMCTL_SW_CTL);
	writel(FBIT(CAMCTL_SW_RST_TRIG), dev->base + REG_CAMCTL_SW_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, dev->base + REG_CAMCTL_SW_CTL, sw_ctl,
				 sw_ctl & FBIT(CAMCTL_SW_RST_ST),
				 1 /* delay, us */,
				 5000 /* timeout, us */);
	if (WARN_ON(ret < 0)) {
		dev_info(dev->dev, "%s: timeout! todo: implement debug flow...\n",
			 __func__);

		goto RESET_FAILURE;
	}

	/* do hw rst */
	writel(FBIT(CAMCTL_HW_RST), dev->base + REG_CAMCTL_SW_CTL);
	writel(0, dev->base + REG_CAMCTL_SW_CTL);

RESET_FAILURE:
	/* Enable all DMA DCM back */
	writel(0x0, dev->base + REG_CAMCTL_MOD5_DCM_DIS);
	writel(0x0, dev->base + REG_CAMCTL_MOD6_DCM_DIS);
	writel(0x0, dev->yuv_base + REG_CAMCTL2_MOD5_DCM_DIS);
	writel(0x0, dev->yuv_base + REG_CAMCTL2_MOD6_DCM_DIS);

	wmb(); /* make sure committed */
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

//static void subsample_set_sensor_time(struct mtk_raw_device *dev)
//{
//	dev->sub_sensor_ctrl_en = true;
//	dev->set_sensor_idx = dev->subsample_ratio - 1;
//	dev->cur_vsync_idx = -1;
//}

#ifdef NOT_READY
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
#endif

static bool is_sub_sample_sensor_timing(struct mtk_raw_device *dev)
{
	return dev->cur_vsync_idx >= dev->set_sensor_idx;
}

/* use spare register FH_SPARE_5 */
#define REG_FRAME_SEQ_NUM					0x4994

/* IRQ Error Mask */
#define INT_ST_MASK_CAM_ERR					\
	(FBIT(CAMCTL_TG_OVRUN_ST)			|	\
	 FBIT(CAMCTL_TG_GRABERR_ST)			|	\
	 FBIT(CAMCTL_TG_SOF_DROP_ST)			|	\
	 FBIT(CAMCTL_CQ_MAX_START_DLY_SMALL_INT_ST)	|	\
	 FBIT(CAMCTL_CQ_MAX_START_DLY_ERR_INT_ST)	|	\
	 FBIT(CAMCTL_CQ_MAIN_CODE_ERR_ST)		|	\
	 FBIT(CAMCTL_CQ_MAIN_VS_ERR_ST)			|	\
	 FBIT(CAMCTL_CQ_MAIN_VS_ERR_ST)			|	\
	 FBIT(CAMCTL_CQ_TRIG_DLY_INT_ST)		|	\
	 FBIT(CAMCTL_CQ_SUB_CODE_ERR_ST)		|	\
	 FBIT(CAMCTL_CQ_SUB_VS_ERR_ST)			|	\
	 FBIT(CAMCTL_DMA_ERR_ST))

static irqreturn_t mtk_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	//struct device *dev = raw_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int frame_idx, frame_idx_inner, fbc_fho_ctl2;
	unsigned int irq_status, err_status, dmao_done_status, dmai_done_status;
	unsigned int drop_status, dma_ofl_status, cq_done_status, cq2_done_status;
	bool wake_thread = 0;

	irq_status	 = readl_relaxed(raw_dev->base + REG_CAMCTL_INT_STATUS);
	dmao_done_status = readl_relaxed(raw_dev->base + REG_CAMCTL_INT2_STATUS);
	dmai_done_status = readl_relaxed(raw_dev->base + REG_CAMCTL_INT3_STATUS);
	drop_status	 = readl_relaxed(raw_dev->base + REG_CAMCTL_INT4_STATUS);
	dma_ofl_status	 = readl_relaxed(raw_dev->base + REG_CAMCTL_INT5_STATUS);
	cq_done_status	 = readl_relaxed(raw_dev->base + REG_CAMCTL_INT6_STATUS);
	cq2_done_status	 = readl_relaxed(raw_dev->base + REG_CAMCTL_INT7_STATUS);

	frame_idx	= readl_relaxed(raw_dev->base + REG_FRAME_SEQ_NUM);
	frame_idx_inner	= readl_relaxed(raw_dev->base_inner + REG_FRAME_SEQ_NUM);

	fbc_fho_ctl2 = readl_relaxed(raw_dev->base + REG_FBC_FHO_R1_CTL2);

	err_status = irq_status & INT_ST_MASK_CAM_ERR;

	//if (unlikely(debug_raw))
	//	dev_dbg(dev,
	//		"INT:0x%x(err:0x%x) 2~7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x (in:%d)\n",
	//		irq_status, err_status,
	//		dmao_done_status, dmai_done_status, drop_status,
	//		dma_ofl_status, cq_done_status, cq2_done_status,
	//		frame_idx_inner);
	/* translate fh reserved data to ctx id and seq no */

#ifdef TO_REMOVE
	if (unlikely(!raw_dev->pipeline || !raw_dev->pipeline->enabled_raw)) {
		dev_dbg(dev, "%s: %i: raw pipeline is disabled\n",
			__func__, raw_dev->id);
		goto ctx_not_found;
	}
#endif

#ifdef REMOVE
	/* SRT err interrupt */
	if (irq_status & P1_DONE_OVER_SOF_INT_ST)
		dev_dbg(dev, "P1_DONE_OVER_SOF_INT_ST");
#endif

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = frame_idx;
	irq_info.frame_idx_inner = frame_idx_inner;

	/* CQ done */
	if (cq_done_status & FBIT(CAMCTL_CQ_THR0_DONE_ST))
		irq_info.irq_type |= 1 << CAMSYS_IRQ_SETTING_DONE;

	/* DMAO done, only for AFO */
	if (dmao_done_status & FBIT(CAMCTL_AFO_R1_DONE_ST)) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_AFO_DONE;
		/* enable AFO_DONE_EN at backend manually */
	}

#ifdef HW_CHANGE
	/* Frame skipped */
	if (irq_status & P1_SKIP_FRAME_INT_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_SKIPPED;
#endif

	/* Frame done */
	if (irq_status & FBIT(CAMCTL_SW_PASS1_DONE_ST)) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_DONE;
		raw_dev->overrun_debug_dump_cnt = 0;
	}

#ifdef HW_CHANGE
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
#endif

	/* Vsync interrupt */
	if (irq_status & FBIT(CAMCTL_TG_VS_INT_ST))
		raw_dev->vsync_count++;

	if (raw_dev->sub_sensor_ctrl_en
	    && irq_status & FBIT(CAMCTL_TG_VS_INT_ORG_ST)
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

#ifdef NOT_READY
	if (MTK_CAM_TRACE_ENABLED(FBC) && (irq_status & TG_VS_INT_ORG_ST)) {
#ifdef DUMP_FBC_SEL_OUTER
		MTK_CAM_TRACE(FBC, "frame %d FBC_SEL 0x% 8x/0x% 8x (outer)",
			irq_info.frame_idx_inner,
			readl_relaxed(raw_dev->base + REG_CAMCTL_FBC_SEL),
			readl_relaxed(raw_dev->yuv_base + REG_CAMCTL_FBC_SEL));
#endif
		mtk_cam_raw_dump_fbc(raw_dev->dev, raw_dev->base, raw_dev->yuv_base);
	}
#endif

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
#ifdef NOT_READY
			raw_handle_error(raw_dev, &irq_info);
#else
			dev_info(raw_dev->dev, "TODO: raw_handle_error\n");
#endif
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

#ifdef NOT_READY
void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev,
				int dequeued_frame_seq_no)
{
	// TODO
}

void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev, int dequeued_frame_seq_no)
{
	// TODO
}

static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no)
{
	// TODO
}
#endif

static int mtk_raw_pm_suspend_prepare(struct mtk_raw_device *dev)
{
//	u32 val;
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_dbg(dev->dev, "cam suspend, disable VF\n");
#ifdef NOT_READY
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
#endif

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev->dev);
	return ret;
}

static int mtk_raw_pm_post_suspend(struct mtk_raw_device *dev)
{
//	u32 val;
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev->dev);
	if (ret)
		return ret;

#ifdef NOT_READY
	/* Enable CMOS */
	dev_dbg(dev->dev, "cam resume, enable CMOS/VF\n");
	val = readl(dev->base + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, dev->base + REG_TG_SEN_MODE);

	/* Enable VF */
	val = readl(dev->base + REG_TG_VF_CON);
	writel(val | TG_VFDATA_EN, dev->base + REG_TG_VF_CON);
#endif

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
	struct mtk_yuv_device *yuv = (struct mtk_yuv_device *)data;
	//struct device *dev = drvdata->dev;

	unsigned int irq_status, err_status, wdma_done_status, rdma_done_status;
	unsigned int drop_status, dma_ofl_status, dma_ufl_status;

	irq_status =
		readl_relaxed(yuv->base + REG_CAMCTL2_INT_STATUS);
	wdma_done_status =
		readl_relaxed(yuv->base + REG_CAMCTL2_INT2_STATUS);
	rdma_done_status =
		readl_relaxed(yuv->base + REG_CAMCTL2_INT3_STATUS);
	drop_status =
		readl_relaxed(yuv->base + REG_CAMCTL2_INT4_STATUS);
	dma_ofl_status =
		readl_relaxed(yuv->base + REG_CAMCTL2_INT5_STATUS);
	dma_ufl_status =
		readl_relaxed(yuv->base + REG_CAMCTL2_INT6_STATUS);

	err_status = irq_status & 0x4; // bit2: DMA_ERR

	//if (unlikely(debug_raw))
	//	dev_dbg(dev, "YUV-INT:0x%x(err:0x%x) INT2/4/5 0x%x/0x%x/0x%x\n",
	//		irq_status, err_status,
	//		dma_done_status, drop_status, dma_ofl_status);

	/* trace */
	if (irq_status || wdma_done_status)
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
