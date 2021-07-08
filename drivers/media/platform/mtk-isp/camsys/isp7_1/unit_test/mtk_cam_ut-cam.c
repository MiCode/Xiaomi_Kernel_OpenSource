// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "mtk_cam_ut.h"
#include "mtk_cam_ut-engines.h"

#include "mtk_cam_regs.h"

static int ut_raw_reset(struct device *dev)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	void __iomem *base = raw->base;
	void __iomem *yuv_base = raw->yuv_base;
	u32 ctl;

	writel(0x00000fff, base + CAM_REG_CTL_RAW_MOD5_DCM_DIS);
	writel(0x0007ffff, base + CAM_REG_CTL_RAW_MOD6_DCM_DIS);
	writel(0xffffffff, yuv_base + CAM_REG_CTL_RAW_MOD5_DCM_DIS);

	/* Disable all DMA DCM before reset */
	wmb(); /* TBC */
	writel(1, base + REG_CTL_SW_CTL);

	if (!readl_poll_timeout_atomic(base + REG_CTL_SW_CTL, ctl,
				       ctl & 0x2, 1, 1000)) {
		writel(4, base + REG_CTL_SW_CTL);
		writel(0, base + REG_CTL_SW_CTL);
		dev_info(dev, "reset success\n");
	} else {
		dev_info(dev, "Reset hw timeout: SW_CTL 0x%x\n",
			 readl_relaxed(base + REG_CTL_SW_CTL));
	}

	writel(0x0, base + CAM_REG_CTL_RAW_MOD5_DCM_DIS);
	writel(0x0, base + CAM_REG_CTL_RAW_MOD6_DCM_DIS);
	writel(0x0, yuv_base + CAM_REG_CTL_RAW_MOD5_DCM_DIS);

	writel_relaxed(0x0, base + REG_CTL_SW_PASS1_DONE);
	writel_relaxed(0x0, raw->base_inner + REG_CTL_SW_PASS1_DONE);

	/* make sure reset take effect */
	wmb();

	return -1;
}

static void set_steamon_handle(struct device *dev, int type);

static int ut_raw_initialize(struct device *dev, void *ext_params)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	struct mtk_ut_raw_initial_params *p = ext_params;
	void __iomem *base = raw->base;
	u32 val;

	if (!p)
		return -1;

	/* initialize for CQ */
	if (p->subsample) {
		val = readl_relaxed(base + REG_CQ_EN);
		writel_relaxed(val | SCQ_EN | SCQ_SUBSAMPLE_EN, base + REG_CQ_EN);

		writel_relaxed(0x100 | p->subsample, base + REG_CTL_SW_PASS1_DONE);
		writel_relaxed(0x100 | p->subsample, raw->base_inner + REG_CTL_SW_PASS1_DONE);
	} else {
		val = readl_relaxed(base + REG_CQ_EN);
		writel_relaxed(val | SCQ_EN, base + REG_CQ_EN);
	}

	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       base + REG_CQ_THR0_CTL);
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       base + REG_CQ_SUB_THR0_CTL);

	writel_relaxed(0xffffffff, base + REG_SCQ_START_PERIOD);
	writel_relaxed(0xffffffff, raw->base_inner + REG_SCQ_START_PERIOD);

	/* make sure all the CQ setting take effect */
	wmb();

	set_steamon_handle(dev, p->streamon_type);

	raw->is_subsample = p->subsample;
	raw->is_initial_cq = 1;
	raw->cq_done_mask = 0;

	return 0;
}

static int ut_raw_s_stream(struct device *dev, int on)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	void __iomem *base = raw->base;
	u32 val;

	dev_info(dev, "%s: %s\n", __func__, on ? "on" : "off");

	val = readl_relaxed(base + REG_TG_VF_CON);
	if (on)
		val |= TG_VFDATA_EN;
	else
		val &= ~TG_VFDATA_EN;

	writel_relaxed(val, base + REG_TG_VF_CON);

	/* make sure VF enable setting take effect */
	wmb();

	return 0;
}

static int ut_raw_trigger_rawi_r2(struct device *dev, int on)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	void __iomem *base = raw->base;

	if (on)
		writel(0x1, base + REG_CTL_RAWI_TRIG);

	dev_info(raw->dev, "%s: on %d\n", __func__, on);
	return 0;
}

static int ut_raw_trigger_rawi_r6(struct device *dev, int on)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	void __iomem *base = raw->base;

	dev_info(dev, "[%s] ut_raw_trigger_rawi\n", __func__);

	writel_relaxed(0x10, base + REG_CTL_RAWI_TRIG);
	wmb(); /* TBC */

	return 0;
}

static void set_steamon_handle(struct device *dev, int type)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);

	dev_info(raw->dev, "streamon type %d\n", type);

	if (type == STREAM_FROM_TG)
		raw->ops.s_stream = ut_raw_s_stream;
	else if (type == STREAM_FROM_RAWI_R2)
		raw->ops.s_stream = ut_raw_trigger_rawi_r2;
	else if (type == STREAM_FROM_RAWI_R6)
		raw->ops.s_stream = ut_raw_trigger_rawi_r6;
	else
		dev_info(raw->dev, "type %d not supported yet\n", type);
}

static int ut_raw_apply_cq(struct device *dev,
			    dma_addr_t cq_addr, unsigned int cq_size, unsigned int cq_offset,
			    unsigned int sub_cq_size, unsigned int sub_cq_offset)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	void __iomem *base = raw->base;

	dev_info(dev, "[%s] cq baseaddr = 0x%x, cq size = 0x%x, cq offset = 0x%x, sub_cq size = 0x%x, sub_cq_offset = 0x%x\n",
		__func__, cq_addr, cq_size, cq_offset, sub_cq_size, sub_cq_offset);
		writel_relaxed(cq_addr + cq_offset, base + REG_CQ_THR0_BASEADDR);
		writel_relaxed(cq_size, base + REG_CQ_THR0_DESC_SIZE);

	writel_relaxed(cq_addr + sub_cq_offset,
		base + REG_CQ_SUB_THR0_BASEADDR_2);
	writel_relaxed(sub_cq_size,
		base + REG_CQ_SUB_THR0_DESC_SIZE_2);

	writel(CTL_CQ_THR0_START, base + REG_CTL_START);
	/* make sure reset take effect */
	wmb();

	return 0;
}

static void ut_raw_set_ops(struct device *dev)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);

	raw->ops.reset = ut_raw_reset;
	raw->ops.s_stream = ut_raw_s_stream;
	raw->ops.apply_cq = ut_raw_apply_cq;
	raw->ops.initialize = ut_raw_initialize;
}

static int mtk_ut_raw_component_bind(struct device *dev,
				     struct device *master, void *data)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	struct mtk_cam_ut *ut = data;
	struct ut_event evt;

	dev_info(dev, "%s\n", __func__);

	if (!data) {
		dev_info(dev, "no master data\n");
		return -1;
	}

	if (!ut->raw) {
		dev_info(dev, "no raw arr, num of raw %d\n", ut->num_raw);
		return -1;
	}
	ut->raw[raw->id] = dev;

	evt.mask = EVENT_SOF | EVENT_CQ_DONE | EVENT_SW_P1_DONE | EVENT_CQ_MAIN_TRIG_DLY;
	add_listener(&raw->event_src, &ut->listener, evt);

	return 0;
}

static void mtk_ut_raw_component_unbind(struct device *dev,
					struct device *master, void *data)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	struct mtk_cam_ut *ut = data;

	dev_dbg(dev, "%s\n", __func__);
	ut->raw[raw->id] = NULL;
	remove_listener(&raw->event_src, &ut->listener);
}

static const struct component_ops mtk_ut_raw_component_ops = {
	.bind = mtk_ut_raw_component_bind,
	.unbind = mtk_ut_raw_component_unbind,
};

static void raw_handle_tg_grab_err(struct mtk_ut_raw_device *raw)
{
	void __iomem *base = raw->base;
	int val;

	val = readl_relaxed(CAM_REG_TG_PATH_CFG(base));
	val = val | TG_TG_FULL_SEL;
	writel_relaxed(val, CAM_REG_TG_PATH_CFG(base));
	/* make sure all the TG setting take effect */
	wmb();
	val = readl_relaxed(CAM_REG_TG_SEN_MODE(base));
	val = val | TG_CMOS_RDY_SEL;
	writel_relaxed(val, CAM_REG_TG_SEN_MODE(base));
	/* make sure all the TG setting take effect */
	wmb();
	dev_dbg(raw->dev,
		"TG_PATH_CFG:0x%x, TG_SEN_MODE:0x%x TG_FRMSIZE_ST:0x%8x, TG_FRMSIZE_ST_R:0x%8x TG_SEN_GRAB_PXL:0x%8x, TG_SEN_GRAB_LIN:0x%8x\n",
		readl_relaxed(CAM_REG_TG_PATH_CFG(base)),
		readl_relaxed(CAM_REG_TG_SEN_MODE(base)),
		readl_relaxed(CAM_REG_TG_FRMSIZE_ST(base)),
		readl_relaxed(CAM_REG_TG_FRMSIZE_ST_R(base)),
		readl_relaxed(CAM_REG_TG_SEN_GRAB_PXL(base)),
		readl_relaxed(CAM_REG_TG_SEN_GRAB_LIN(base)));
}

static void raw_handle_dma_err(struct mtk_ut_raw_device *raw)
{
	void __iomem *base = raw->base;
	void __iomem *yuv_base = raw->yuv_base;

	dev_dbg_ratelimited(raw->dev,
			    "IMGO:%x,YUVO_R1/R2/R3/R4/R5:%x/%x/%x/%x/%x\n",
			    readl_relaxed(base + REG_IMGO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R2_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R3_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R4_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R5_ERR_STAT)
			   );

	dev_dbg_ratelimited(raw->dev,
			    "RZH1N2TO_R1/R2/R3:%x/%x/%x,DRZS4NO_R1/R2/R3:%x/%x/%x\n",
			    readl_relaxed(yuv_base + REG_RZH1N2TO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_RZH1N2TO_R2_ERR_STAT),
			    readl_relaxed(yuv_base + REG_RZH1N2TO_R3_ERR_STAT),
			    readl_relaxed(yuv_base + REG_DRZS4NO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_DRZS4NO_R2_ERR_STAT),
			    readl_relaxed(yuv_base + REG_DRZS4NO_R3_ERR_STAT)
			   );

	dev_dbg_ratelimited(raw->dev,
			    "AAO/AAHO/LTMSO/FLKO/AFO:%x/%x/%x/%x/%x,TSFSO_R1/R2:%x/%x\n",
			    readl_relaxed(base + REG_AAO_R1_ERR_STAT),
			    readl_relaxed(base + REG_AAHO_R1_ERR_STAT),
			    readl_relaxed(base + REG_LTMSO_R1_ERR_STAT),
			    readl_relaxed(base + REG_FLKO_R1_ERR_STAT),
			    readl_relaxed(base + REG_AFO_R1_ERR_STAT),
			    readl_relaxed(base + REG_TSFSO_R1_ERR_STAT),
			    readl_relaxed(base + REG_TSFSO_R2_ERR_STAT)
			   );

	dev_dbg_ratelimited(raw->dev,
			    "RAWI_R2/R3:%x/%x,LSCI:%x,BPCI_R1/R2/R3:%x/%x/%x\n",
			    readl_relaxed(base + REG_RAWI_R2_ERR_STAT),
			    readl_relaxed(base + REG_RAWI_R3_ERR_STAT),
			    readl_relaxed(base + REG_LSCI_R1_ERR_STAT),
			    readl_relaxed(base + REG_BPCI_R1_ERR_STAT),
			    readl_relaxed(base + REG_BPCI_R2_ERR_STAT),
			    readl_relaxed(base + REG_BPCI_R3_ERR_STAT)
			   );
}

static void raw_check_error_status(struct mtk_ut_raw_device *raw,
				   struct ut_raw_status *st)
{
	u32 err_status;

	err_status = st->irq & INT_ST_MASK_CAM_ERR;
	if (!err_status)
		return;
	dev_info(raw->dev, "int_err:0x%x 0x%x\n", st->irq, err_status);

	if (err_status & DMA_ERR_ST)
		raw_handle_dma_err(raw);
	/* Show TG register for more error detail*/
	if (err_status & TG_GBERR_ST)
		raw_handle_tg_grab_err(raw);
}

static irqreturn_t mtk_ut_raw_irq(int irq, void *data)
{
#define RAW_DEBUG 0
	struct mtk_ut_raw_device *raw = data;
	void __iomem *base = raw->base;
	struct ut_raw_status status;
	struct ut_raw_statusx statusx;
	struct ut_event event;
#if RAW_DEBUG
	void __iomem *base_inner = raw->base_inner;
	struct ut_raw_debug_csr dbg_csr;
#endif
	/* raw */
	status.irq = readl_relaxed(CAM_REG_CTL_RAW_INT_STATUS(base));
	status.wdma = readl_relaxed(CAM_REG_CTL_RAW_INT2_STATUS(base));
	status.rdma = readl_relaxed(CAM_REG_CTL_RAW_INT3_STATUS(base));
	status.drop = readl_relaxed(CAM_REG_CTL_RAW_INT4_STATUS(base));
	status.ofl = readl_relaxed(CAM_REG_CTL_RAW_INT5_STATUS(base));
	status.cq_done = readl_relaxed(CAM_REG_CTL_RAW_INT6_STATUS(base));
	status.cq_done2 = readl_relaxed(CAM_REG_CTL_RAW_INT7_STATUS(base));

	event.mask = 0;

	if (status.irq & SOF_INT_ST)
		event.mask |= EVENT_SOF;

	if (status.irq & SW_PASS1_DON_ST) {
		event.mask |= EVENT_SW_P1_DONE;

		/* raw */
		statusx.irq = readl_relaxed(CAM_REG_CTL_RAW_INT_STATUSX(base));
		statusx.wdma = readl_relaxed(CAM_REG_CTL_RAW_INT2_STATUSX(base));
		statusx.rdma = readl_relaxed(CAM_REG_CTL_RAW_INT3_STATUSX(base));
		statusx.drop = readl_relaxed(CAM_REG_CTL_RAW_INT4_STATUSX(base));
		statusx.ofl = readl_relaxed(CAM_REG_CTL_RAW_INT5_STATUSX(base));
		statusx.cq_done = readl_relaxed(CAM_REG_CTL_RAW_INT6_STATUSX(base));
		statusx.cq_done2 = readl_relaxed(CAM_REG_CTL_RAW_INT7_STATUSX(base));

		dev_info(raw->dev,
			 "STATUSX INT1-7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
			 statusx.irq, statusx.wdma, statusx.rdma, statusx.drop,
			 statusx.ofl, statusx.cq_done, statusx.cq_done2);
	}

	if (status.cq_done & CTL_CQ_THR0_DONE_ST)
		raw->cq_done_mask |= 0x1;

	if (status.cq_done2 & CTL_CQ_THRSUB_DONE_ST)
		raw->cq_done_mask |= 0x2;

	if (raw->is_subsample && !raw->is_initial_cq
	    && (raw->cq_done_mask & 0x1)) {
		raw->cq_done_mask = 0;
		event.mask |= EVENT_CQ_DONE;
	} else if (raw->cq_done_mask == 0x3) {
		raw->is_initial_cq = 0;
		raw->cq_done_mask = 0;
		event.mask |= EVENT_CQ_DONE;
	}

	if (status.irq & CQ_MAIN_TRIG_DLY_ST)
		event.mask |= EVENT_CQ_MAIN_TRIG_DLY;

	if (event.mask) {
		dev_dbg(raw->dev, "send event 0x%x\n", event.mask);
		send_event(&raw->event_src, event);
	}

		/* debug */
#if RAW_DEBUG
	dbg_csr.cq_sub1_addr =
		readl_relaxed(CAM_REG_CQ_SUB_THR0_BASEADDR_1(base));
	dbg_csr.cq_sub2_addr =
		readl_relaxed(CAM_REG_CQ_SUB_THR0_BASEADDR_2(base));
	dbg_csr.cq_sub1_szie =
		readl_relaxed(CAM_REG_SUB_THR0_DESC_SIZE_1(base));
	dbg_csr.cq_sub2_szie =
		readl_relaxed(CAM_REG_SUB_THR0_DESC_SIZE_2(base));

	dbg_csr.imgo_addr =
		readl_relaxed(CAM_REG_IMGO_ORIWDMA_BASE_ADDR(base));
	dbg_csr.fbc_imgo_ctl2 =
		readl_relaxed(CAM_REG_FBC_IMGO_R1_CTL2(base));

	dbg_csr.cq_en =
		readl_relaxed(CAM_REG_CQ_EN(base));
	dbg_csr.sub_cq_en =
		readl_relaxed(CAM_REG_SUB_CQ_EN(base));

	dbg_csr.sw_pass1_done =
		readl_relaxed(CAM_REG_SW_PASS1_DONE(base));
	dbg_csr.sw_sub_ctl =
		readl_relaxed(CAM_REG_SW_SUB_CTL(base));

	dbg_csr.aao_addr =
		readl_relaxed(CAM_REG_AAO_ORIWDMA_BASE_ADDR(base));
	dbg_csr.fbc_aao_ctl2 =
		readl_relaxed(CAM_REG_FBC_AAO_R1_CTL2(base));
	dbg_csr.fbc_aaho_ctl2 =
		readl_relaxed(CAM_REG_FBC_AAHO_R1_CTL2(base));
#endif
#if RAW_DEBUG
	dev_info(raw->dev,
		"INT1-7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		status.irq, status.wdma, status.rdma, status.drop,
		status.ofl, status.cq_done, status.cq_done2);

	dev_info(raw->dev,
		"cq_sub1_addr=0x%x, cq_sub2_addr=0x%x, cq_sub1_szie:0x%x, cq_sub2_szie=0x%x\n",
		dbg_csr.cq_sub1_addr, dbg_csr.cq_sub2_addr,
		dbg_csr.cq_sub1_szie, dbg_csr.cq_sub2_szie);

	dev_info(raw->dev,
		"imgo_addr=0x%x fbc_imgo_ctl2=0x%x cq 0x%x sub_cq 0x%x sw_pass1_done 0x%x sw_sub_ctl 0x%x, aao_addr=0x%x ctl2 0x%x, 0x%x\n",
		dbg_csr.imgo_addr, dbg_csr.fbc_imgo_ctl2, dbg_csr.cq_en, dbg_csr.sub_cq_en,
		dbg_csr.sw_pass1_done, dbg_csr.sw_sub_ctl, dbg_csr.aao_addr, dbg_csr.fbc_aao_ctl2,
		dbg_csr.fbc_aaho_ctl2);

	dbg_csr.sw_pass1_done =
		readl_relaxed(CAM_REG_SW_PASS1_DONE(base_inner));
	dbg_csr.sw_sub_ctl =
		readl_relaxed(CAM_REG_SW_SUB_CTL(base_inner));

	dev_info(raw->dev,
		"inner sw_pass1_done 0x%x sw_sub_ctl 0x%x\n",
		dbg_csr.sw_pass1_done, dbg_csr.sw_sub_ctl);
#endif

	raw_check_error_status(raw, &status);
	dev_info(raw->dev, "INT1-7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x",
		 status.irq, status.wdma, status.rdma, status.drop,
		 status.ofl, status.cq_done, status.cq_done2);

	return IRQ_HANDLED;
}

static int mtk_ut_raw_of_probe(struct platform_device *pdev,
			    struct mtk_ut_raw_device *raw)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret;
	int i;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &raw->id);
	dev_info(dev, "id = %d\n", raw->id);
	if (ret) {
		dev_info(dev, "missing camid property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base)) {
		dev_info(dev, "failed to map register base\n");
		return PTR_ERR(raw->base);
	}
	dev_dbg(dev, "raw, map_addr=0x%pK\n", raw->base);
	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base_inner)) {
		dev_info(dev, "failed to map register inner base\n");
		return PTR_ERR(raw->base_inner);
	}

	/* will be assigned later */
	raw->yuv_base = NULL;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_ut_raw_irq, 0,
			       dev_name(dev), raw);
	if (ret) {
		dev_info(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	raw->num_clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");
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

	return 0;
}

static int mtk_ut_raw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_raw_device *drvdata;
	int ret;

	dev_info(dev, "%s\n", __func__);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	dev_set_drvdata(dev, drvdata);

	ret = mtk_ut_raw_of_probe(pdev, drvdata);
	if (ret)
		return ret;

	init_event_source(&drvdata->event_src);
	ut_raw_set_ops(dev);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_ut_raw_component_ops);
	if (ret)
		return ret;

	dev_info(dev, "%s: success\n", __func__);
	return 0;
}

static int mtk_ut_raw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_raw_device *drvdata = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);
	component_del(dev, &mtk_ut_raw_component_ops);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_put(drvdata->clks[i]);

	return 0;
}

static int mtk_ut_raw_pm_suspend(struct device *dev)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_dbg(dev, "cam suspend, disable VF\n");
	val = readl(raw->base + REG_TG_VF_CON);
	writel(val & (~TG_VFDATA_EN), raw->base + REG_TG_VF_CON);
	ret = readl_poll_timeout_atomic(raw->base + REG_TG_INTER_ST, val,
					(val & TG_CAM_CS_MASK) == TG_IDLE_ST,
					USEC_PER_MSEC, 33);
	if (ret)
		dev_dbg(dev, "can't stop HW:%d:0x%x\n", ret, val);

	/* Disable CMOS */
	val = readl(raw->base + REG_TG_SEN_MODE);
	writel(val & (~TG_SEN_MODE_CMOS_EN), raw->base + REG_TG_SEN_MODE);

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev);
	return ret;
}

static int mtk_ut_raw_pm_resume(struct device *dev)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	/* Enable CMOS */
	dev_dbg(dev, "cam resume, enable CMOS/VF\n");
	val = readl(raw->base + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, raw->base + REG_TG_SEN_MODE);

	/* Enable VF */
	val = readl(raw->base + REG_TG_VF_CON);
	writel(val | TG_VFDATA_EN, raw->base + REG_TG_VF_CON);

	return 0;
}

static int mtk_ut_raw_runtime_suspend(struct device *dev)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	int i;

	ut_raw_reset(dev);

	for (i = 0; i < raw->num_clks; i++)
		clk_disable_unprepare(raw->clks[i]);

	return 0;
}

static int mtk_ut_raw_runtime_resume(struct device *dev)
{
	struct mtk_ut_raw_device *raw = dev_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < raw->num_clks; i++) {
		ret = clk_prepare_enable(raw->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(raw->clks[i--]);

			return ret;
		}
	}

	ut_raw_reset(dev);
	return 0;
}

static const struct dev_pm_ops mtk_ut_raw_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ut_raw_pm_suspend, mtk_ut_raw_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_ut_raw_runtime_suspend, mtk_ut_raw_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_ut_raw_of_ids[] = {
	{.compatible = "mediatek,mt8195-cam-raw",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ut_raw_of_ids);

struct platform_driver mtk_ut_raw_driver = {
	.probe   = mtk_ut_raw_probe,
	.remove  = mtk_ut_raw_remove,
	.driver  = {
		.name  = "mtk-cam ut-raw",
		.of_match_table = of_match_ptr(mtk_ut_raw_of_ids),
		.pm     = &mtk_ut_raw_pm_ops,
	}
};

/* YUV part driver */
static void ut_yuv_set_ops(struct device *dev)
{
	struct mtk_ut_yuv_device *drvdata = dev_get_drvdata(dev);

	drvdata->ops.reset = NULL;
	drvdata->ops.s_stream = NULL;
	drvdata->ops.apply_cq = NULL;
	drvdata->ops.initialize = NULL;
}

static int mtk_ut_yuv_component_bind(struct device *dev,
				     struct device *master, void *data)
{
	struct mtk_ut_yuv_device *drvdata = dev_get_drvdata(dev);
	struct mtk_cam_ut *ut = data;

	dev_info(dev, "%s\n", __func__);

	if (!data) {
		dev_info(dev, "no master data\n");
		return -1;
	}

	if (!ut->yuv) {
		dev_info(dev, "no yuv arr, num of yuv %d\n", ut->num_yuv);
		return -1;
	}
	ut->yuv[drvdata->id] = dev;

	return 0;
}

static void mtk_ut_yuv_component_unbind(struct device *dev,
					struct device *master, void *data)
{
	struct mtk_ut_yuv_device *drvdata = dev_get_drvdata(dev);
	struct mtk_cam_ut *ut = data;

	dev_dbg(dev, "%s\n", __func__);
	ut->yuv[drvdata->id] = NULL;
}

static const struct component_ops mtk_ut_yuv_component_ops = {
	.bind = mtk_ut_yuv_component_bind,
	.unbind = mtk_ut_yuv_component_unbind,
};

static void yuv_handle_dma_err(struct mtk_ut_yuv_device *raw)
{
	//void __iomem *base = raw->base;
	void __iomem *yuv_base = raw->base;

	dev_dbg_ratelimited(raw->dev,
			    "YUVO_R1/R2/R3/R4/R5:%x/%x/%x/%x/%x\n",
			    readl_relaxed(yuv_base + REG_YUVO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R2_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R3_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R4_ERR_STAT),
			    readl_relaxed(yuv_base + REG_YUVO_R5_ERR_STAT)
			   );

	dev_dbg_ratelimited(raw->dev,
			    "RZH1N2TO_R1/R2/R3:%x/%x/%x,DRZS4NO_R1/R2/R3:%x/%x/%x\n",
			    readl_relaxed(yuv_base + REG_RZH1N2TO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_RZH1N2TO_R2_ERR_STAT),
			    readl_relaxed(yuv_base + REG_RZH1N2TO_R3_ERR_STAT),
			    readl_relaxed(yuv_base + REG_DRZS4NO_R1_ERR_STAT),
			    readl_relaxed(yuv_base + REG_DRZS4NO_R2_ERR_STAT),
			    readl_relaxed(yuv_base + REG_DRZS4NO_R3_ERR_STAT)
			   );
}

static irqreturn_t mtk_ut_yuv_irq(int irq, void *data)
{
	struct mtk_ut_yuv_device *drvdata = data;
	void __iomem *base = drvdata->base;
	struct ut_yuv_status status;
	struct ut_yuv_statusx statusx;

	/* yuv */
	status.irq = readl_relaxed(CAM_REG_CTL2_RAW_INT_STATUS(base));
	status.wdma = readl_relaxed(CAM_REG_CTL2_RAW_INT2_STATUS(base));
	status.drop = readl_relaxed(CAM_REG_CTL2_RAW_INT4_STATUS(base));
	status.ofl = readl_relaxed(CAM_REG_CTL2_RAW_INT5_STATUS(base));

	if (status.irq & 0x04)
		yuv_handle_dma_err(data);

	if (status.irq & (YUV_PASS1_DON_ST|YUV_SW_PASS1_DON_ST|YUV_DMA_ERR_ST)) {

		statusx.irq = readl_relaxed(CAM_REG_CTL2_RAW_INT_STATUSX(base));
		statusx.wdma = readl_relaxed(CAM_REG_CTL2_RAW_INT2_STATUSX(base));
		statusx.drop = readl_relaxed(CAM_REG_CTL2_RAW_INT4_STATUSX(base));
		statusx.ofl = readl_relaxed(CAM_REG_CTL2_RAW_INT5_STATUSX(base));

		dev_info(drvdata->dev, "INT 1245 0x%x/0x%x/0x%x/0x%x\n",
		 status.irq, status.wdma, status.drop, status.ofl);

		dev_info(drvdata->dev, "INT-DONE 1245 0x%x/0x%x/0x%x/0x%x\n",
			 statusx.irq, statusx.wdma, statusx.drop, statusx.ofl);

	}
	return IRQ_HANDLED;
}

static int mtk_ut_yuv_of_probe(struct platform_device *pdev,
			    struct mtk_ut_yuv_device *drvdata)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret;
	int i;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &drvdata->id);
	dev_info(dev, "id = %d\n", drvdata->id);
	if (ret) {
		dev_info(dev, "missing camid property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	drvdata->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->base)) {
		dev_info(dev, "failed to map register base\n");
		return PTR_ERR(drvdata->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_ut_yuv_irq, 0,
			       dev_name(dev), drvdata);
	if (ret) {
		dev_info(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	drvdata->num_clks = of_count_phandle_with_args(pdev->dev.of_node,
						       "clocks",
						       "#clock-cells");
	dev_info(dev, "clk_num:%d\n", drvdata->num_clks);

	if (drvdata->num_clks) {
		drvdata->clks = devm_kcalloc(dev, drvdata->num_clks,
					     sizeof(*drvdata->clks),
					     GFP_KERNEL);
		if (!drvdata->clks)
			return -ENODEV;
	}

	for (i = 0; i < drvdata->num_clks; i++) {
		drvdata->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(drvdata->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	return 0;
}


static int mtk_ut_yuv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_yuv_device *drvdata;
	int ret;

	dev_info(dev, "%s\n", __func__);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	dev_set_drvdata(dev, drvdata);

	ret = mtk_ut_yuv_of_probe(pdev, drvdata);
	if (ret)
		return ret;

	ut_yuv_set_ops(dev);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_ut_yuv_component_ops);
	if (ret)
		return ret;

	dev_info(dev, "%s: success\n", __func__);
	return 0;
}

static int mtk_ut_yuv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_raw_device *drvdata = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);
	component_del(dev, &mtk_ut_yuv_component_ops);

	for (i = 0; i < drvdata->num_clks; i++)
		clk_put(drvdata->clks[i]);

	return 0;
}


static int mtk_ut_yuv_pm_suspend(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_suspend(dev);

	return ret;
}

static int mtk_ut_yuv_pm_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	return 0;
}

static int mtk_ut_yuv_runtime_suspend(struct device *dev)
{
	struct mtk_ut_yuv_device *raw = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < raw->num_clks; i++)
		clk_disable_unprepare(raw->clks[i]);
	return 0;
}

static int mtk_ut_yuv_runtime_resume(struct device *dev)
{
	struct mtk_ut_yuv_device *raw = dev_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < raw->num_clks; i++) {
		ret = clk_prepare_enable(raw->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(raw->clks[i--]);

			return ret;
		}
	}
	return 0;
}


static const struct dev_pm_ops mtk_ut_yuv_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ut_yuv_pm_suspend, mtk_ut_yuv_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_ut_yuv_runtime_suspend, mtk_ut_yuv_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_ut_yuv_of_ids[] = {
	{.compatible = "mediatek,mt8195-cam-yuv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ut_yuv_of_ids);

struct platform_driver mtk_ut_yuv_driver = {
	.probe   = mtk_ut_yuv_probe,
	.remove  = mtk_ut_yuv_remove,
	.driver  = {
		.name  = "mtk-cam ut-yuv",
		.of_match_table = of_match_ptr(mtk_ut_yuv_of_ids),
		.pm     = &mtk_ut_yuv_pm_ops,
	}
};

#if WITH_LARB_DRIVER
static int mtk_ut_larb_component_bind(struct device *dev,
				      struct device *master,
				      void *data)
{
	struct device_link *link;

	dev_dbg(dev, "%s\n", __func__);

	link = device_link_add(master, dev, DL_FLAG_AUTOREMOVE_CONSUMER |
			       DL_FLAG_PM_RUNTIME);
	if (!link) {
		dev_info(dev, "Unable to create link between %s and %s\n",
			 dev_name(master), dev_name(dev));
		return -ENODEV;
	}
	return 0;
}

static void mtk_ut_larb_component_unbind(struct device *dev,
					 struct device *master,
					 void *data)
{
	dev_dbg(dev, "%s\n", __func__);
}

static const struct component_ops mtk_ut_larb_component_ops = {
	.bind = mtk_ut_larb_component_bind,
	.unbind = mtk_ut_larb_component_unbind,
};

static int mtk_ut_larb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

#ifdef CONFIG_MTK_IOMMU_PGTABLE_EXT
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_dbg(dev, "%s: No suitable DMA available\n", __func__);
#endif
#endif

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	ret = component_add(dev, &mtk_ut_larb_component_ops);
	if (ret)
		return ret;

	return 0;
}

static int mtk_ut_larb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s disable larb\n", __func__);
	pm_runtime_put(dev);
	pm_runtime_disable(dev);
	return 0;
}

static const struct of_device_id mtk_ut_larb_of_ids[] = {
	{.compatible = "mediatek,mt8195-camisp-larb",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ut_larb_of_ids);

struct platform_driver mtk_ut_larb_driver = {
	.probe   = mtk_ut_larb_probe,
	.remove  = mtk_ut_larb_remove,
	.driver  = {
		.name  = "mtk-cam larb-ut",
		.of_match_table = of_match_ptr(mtk_ut_larb_of_ids),
	}
};
#endif
