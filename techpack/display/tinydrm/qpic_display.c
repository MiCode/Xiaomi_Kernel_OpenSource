// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2016, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_probe_helper.h>

#include "qpic_display.h"
#include "qpic_display_panel.h"

/* for debugging */
static bool use_bam = true;
static bool use_irq = true;
static u32 use_vsync;

/* QPIC display default format */
static uint32_t qpic_pipe_formats[] = {
	DRM_FORMAT_RGB565,
};

static const uint64_t qpic_pipe_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

void qpic_lcdc_reset(struct qpic_display_data *qpic_display)
{
	u32 time_end;

	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_RESET, 1 << 0);
	/* wait 100 us after reset as suggested by hw */
	usleep_range(100, 110);

	time_end = (u32)ktime_to_ms(ktime_get()) + QPIC_MAX_VSYNC_WAIT_TIME_IN_MS;
	while (((QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_STTS) & (1 << 8)) == 0)) {
		if ((u32)ktime_to_ms(ktime_get()) > time_end) {
			pr_err("%s reset not finished\n", __func__);
			break;
		}
		/* yield 100 us for next polling by experiment*/
		usleep_range(100, 110);
	}
}

static void qpic_lcdc_interrupt_en(struct qpic_display_data *qpic_display, bool en)
{
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_CLR, 0xff);
	if (en && !qpic_display->irq_ena) {
		init_completion(&qpic_display->fifo_eof_comp);
		qpic_display->irq_ena = true;
		enable_irq(qpic_display->irq_id);
	} else if (!en && qpic_display->irq_ena) {
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN, 0);
		disable_irq(qpic_display->irq_id);
		qpic_display->irq_ena = false;
	}
}

static irqreturn_t qpic_lcdc_irq_handler(int irq, void *ptr)
{
	u32 data;
	struct qpic_display_data *qpic_display =
			(struct qpic_display_data *)ptr;

	data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_STTS);
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_CLR, 0xff);
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN, 0);

	if (data & (BIT(2) | (BIT(4))))
		complete(&qpic_display->fifo_eof_comp);

	return IRQ_HANDLED;
}

static int qpic_display_pinctrl_set_state(struct qpic_panel_io_desc *qpic_panel_io,
		bool active)
{
	struct pinctrl_state *pin_state;
	int rc = -EFAULT;

	if (IS_ERR_OR_NULL(qpic_panel_io->pin_res.pinctrl))
		return PTR_ERR(qpic_panel_io->pin_res.pinctrl);

	if (active)
		gpio_direction_output(qpic_panel_io->bl_gpio, 1);
	else
		gpio_direction_output(qpic_panel_io->bl_gpio, 0);

	pin_state = active ? qpic_panel_io->pin_res.gpio_state_active
		: qpic_panel_io->pin_res.gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(qpic_panel_io->pin_res.pinctrl,
			pin_state);
		if (rc)
			pr_err("%s: can not set %s pins\n", __func__,
				active ? QPIC_PINCTRL_STATE_DEFAULT
				: QPIC_PINCTRL_STATE_SLEEP);
	} else {
		pr_err("%s: invalid '%s' pinstate\n", __func__,
				active ? QPIC_PINCTRL_STATE_DEFAULT
				: QPIC_PINCTRL_STATE_SLEEP);
	}

	return rc;
}

static int msm_qpic_bus_set_vote(struct qpic_display_data *qpic_display, bool vote)
{
	struct qpic_display_bus_scale_pdata *bvd = qpic_display->data_bus_pdata;
	struct msm_bus_path *usecase = bvd->usecase;
	struct bus_scaling_data *vec = usecase[vote].vec;
	int ddr_rc;

	if (vote == bvd->curr_vote)
		return 0;

	pr_debug("vote:%d qpic-display-data-bus ab:%llu ib:%llu\n",
			vote, vec->ab, vec->ib);
	ddr_rc = icc_set_bw(bvd->data_bus_hdl, vec->ab, vec->ib);
	if (ddr_rc)
		pr_err("icc_set() failed\n");
	else
		bvd->curr_vote = vote;

	return ddr_rc;
}

static struct qpic_display_bus_scale_pdata *qpic_display_get_bus_vote_data(
			struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *of_node = dev->of_node;
	struct qpic_display_bus_scale_pdata *bvd = NULL;
	struct msm_bus_path *usecase = NULL;
	int ret = 0, i = 0, j, num_paths, len;
	const u32 *vec_arr = NULL;

	if (!pdev) {
		dev_err(dev, "null platform device!\n");
		return NULL;
	}

	bvd = devm_kzalloc(dev, sizeof(*bvd), GFP_KERNEL);
	if (!bvd)
		return bvd;

	ret = of_property_read_string(of_node, "qcom,msm-bus,name",
					&bvd->name);
	if (ret) {
		dev_err(dev, "Bus name missing err:(%d)\n", ret);
		goto out;
	}

	ret = of_property_read_u32(of_node, "qcom,msm-bus,num-cases",
		&bvd->num_usecase);
	if (ret) {
		dev_err(dev, "num-usecases not found err:(%d), bus name:%s\n",
			ret, bvd->name);
		goto out;
	}

	if (bvd->num_usecase > 5) {
		dev_err(dev, "invalid num_usecase:(%d), bus name:%s\n",
			bvd->num_usecase, bvd->name);
		goto out;
	}
	usecase = devm_kzalloc(dev, (sizeof(struct msm_bus_path) *
				   bvd->num_usecase), GFP_KERNEL);
	if (!usecase)
		goto out;

	ret = of_property_read_u32(of_node, "qcom,msm-bus,num-paths", &num_paths);
	if (ret) {
		dev_err(dev, "num_paths not found err:(%d)\n", ret);
		goto out;
	}

	vec_arr = of_get_property(of_node, "qcom,msm-bus,vectors-KBps", &len);
	if (!vec_arr) {
		dev_err(dev, "Vector array not found\n");
		goto out;
	}

	for (i = 0; i < bvd->num_usecase; i++) {
		usecase[i].num_paths = num_paths;
		usecase[i].vec = devm_kcalloc(dev, num_paths,
					      sizeof(struct bus_scaling_data),
					      GFP_KERNEL);
		if (!usecase[i].vec)
			goto out;
		for (j = 0; j < num_paths; j++) {
			int idx = ((i * num_paths) + j) * 2;

			usecase[i].vec[j].ab = (u64)
				be32_to_cpu(vec_arr[idx]);
			usecase[i].vec[j].ib = (u64)
				be32_to_cpu(vec_arr[idx + 1]);
		}
	}

	bvd->usecase = usecase;
	return bvd;
out:
	bvd = NULL;
	return bvd;
}

static int qpic_display_bus_register(struct platform_device *pdev,
		struct qpic_display_data *qpic_display)
{
	struct qpic_display_bus_scale_pdata *bus_pdata;
	struct device *dev = &pdev->dev;
	int ret = 0;

	bus_pdata = qpic_display_get_bus_vote_data(dev);
	if (!bus_pdata) {
		dev_err(&pdev->dev, "Failed to get bus_scale data\n");
		return -EINVAL;
	}
	qpic_display->data_bus_pdata = bus_pdata;

	bus_pdata->data_bus_hdl = of_icc_get(&pdev->dev, "qpic-display-data-bus");
	if (IS_ERR_OR_NULL(bus_pdata->data_bus_hdl)) {
		dev_err(&pdev->dev, "(%ld): failed getting %s path\n",
			PTR_ERR(bus_pdata->data_bus_hdl), "qpic-display-data-bus");
		ret = PTR_ERR(bus_pdata->data_bus_hdl);
		bus_pdata->data_bus_hdl = NULL;
	}

	return ret;
}

static void qpic_display_bus_unregister(struct qpic_display_data *qpic_display)
{
	struct qpic_display_bus_scale_pdata *bsd = qpic_display->data_bus_pdata;

	if (bsd)
		icc_put(bsd->data_bus_hdl);
}

static void qpic_display_clk_ctrl(struct qpic_display_data *qpic_display, bool enable)
{
	if (enable) {
		if (qpic_display->qpic_clk)
			clk_prepare_enable(qpic_display->qpic_clk);
		if (qpic_display->qpic_a_clk)
			clk_prepare_enable(qpic_display->qpic_a_clk);
	} else {
		if (qpic_display->qpic_a_clk)
			clk_disable_unprepare(qpic_display->qpic_a_clk);
		if (qpic_display->qpic_clk)
			clk_disable_unprepare(qpic_display->qpic_clk);
	}
}

static int qpic_lcdc_wait_for_fifo(struct qpic_display_data *qpic_display)
{
	u32 data, time_end;
	int ret = 0;

	if (use_irq) {
		data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_STTS);
		data &= 0x3F;
		if (data == 0)
			return ret;
		reinit_completion(&qpic_display->fifo_eof_comp);
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN, (1 << 4));
		ret = wait_for_completion_timeout(&qpic_display->fifo_eof_comp,
				msecs_to_jiffies(QPIC_MAX_VSYNC_WAIT_TIME_IN_MS));
		if (ret > 0) {
			ret = 0;
		} else {
			pr_err("%s timeout %x\n", __func__, ret);
			ret = -ETIMEDOUT;
		}
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN, 0);
	} else {
		time_end = (u32)ktime_to_ms(ktime_get()) +
			QPIC_MAX_VSYNC_WAIT_TIME_IN_MS;
		while (1) {
			data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_STTS);
			data &= 0x3F;
			if (data == 0)
				break;
			/* yield 10 us for next polling by experiment*/
			usleep_range(10, 11);
			if (ktime_to_ms(ktime_get()) > time_end) {
				pr_err("%s time out\n", __func__);
				ret = -EBUSY;
				break;
			}
		}
	}

	return ret;
}

static int qpic_lcdc_send_pkt_bam(struct qpic_display_data *qpic_display,
			u32 cmd, u32 len, u8 *param)
{
	int  ret = 0;
	u32 phys_addr, cfg2, block_len, flags;

	if ((cmd != OP_WRITE_MEMORY_START) &&
		(cmd != OP_WRITE_MEMORY_CONTINUE)) {
		memcpy((u8 *)qpic_display->cmd_buf_virt, param, len);
		phys_addr = qpic_display->cmd_buf_phys;
	} else {
		phys_addr = (u32)param;
	}
	cfg2 = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2);
	cfg2 &= ~0xFF;
	cfg2 |= cmd;
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2, cfg2);
	block_len = 0x7FF0;
	while (len > 0)  {
		if (len <= 0x7FF0) {
			flags = SPS_IOVEC_FLAG_EOT;
			block_len = len;
		} else {
			flags = 0;
		}
		ret = sps_transfer_one(qpic_display->qpic_endpt.handle,
				phys_addr, block_len, NULL, flags);
		if (ret)
			pr_err("failed to submit command %x ret %d\n",
				cmd, ret);
		phys_addr += block_len;
		len -= block_len;
	}
	ret = wait_for_completion_timeout(
		&qpic_display->qpic_endpt.completion,
		msecs_to_jiffies(100 * 4));
	if (ret <= 0)
		pr_err("%s timeout %x\n", __func__, ret);
	else
		ret = 0;

	return ret;
}

static int qpic_lcdc_wait_for_eof(struct qpic_display_data *qpic_display)
{
	u32 data, time_end;
	int ret = 0;

	if (use_irq) {
		data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_STTS);
		if (data & (1 << 2))
			return ret;
		reinit_completion(&qpic_display->fifo_eof_comp);
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN, (1 << 2));
		ret = wait_for_completion_timeout(&qpic_display->fifo_eof_comp,
				msecs_to_jiffies(QPIC_MAX_VSYNC_WAIT_TIME_IN_MS));
		if (ret > 0) {
			ret = 0;
		} else {
			pr_err("%s timeout %x\n", __func__, ret);
			ret = -ETIMEDOUT;
		}
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN, 0);
	} else {
		time_end = (u32)ktime_to_ms(ktime_get()) +
			QPIC_MAX_VSYNC_WAIT_TIME_IN_MS;
		while (1) {
			data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_STTS);
			if (data & (1 << 2))
				break;
			/* yield 10 us for next polling by experiment*/
			usleep_range(10, 11);
			if (ktime_to_ms(ktime_get()) > time_end) {
				pr_err("%s wait for eof time out\n", __func__);
				ret = -EBUSY;
				break;
			}
		}
	}

	return ret;
}

static int qpic_send_pkt_sw(struct qpic_display_data *qpic_display,
				u32 cmd, u32 len, u8 *param)
{
	u32 bytes_left, space, data, cfg2;
	int i, ret = 0;

	if (len <= 4) {
		len = (len + 3) / 4; /* len in dwords */
		data = 0;
		if (param) {
			for (i = 0; i < len; i++)
				data |= (u32)param[i] << (8 * i);
		}
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CMD_DATA_CYCLE_CNT, len);
		QPIC_OUTP(qpic_display, QPIC_REG_LCD_DEVICE_CMD0 + (4 * cmd), data);
		return 0;
	}

	if ((len & 0x1) != 0) {
		pr_debug("%s: number of bytes needs be even\n", __func__);
		len = (len + 1) & (~0x1);
	}
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_CLR, 0xff);
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CMD_DATA_CYCLE_CNT, 0);
	cfg2 = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2);
	if ((cmd != OP_WRITE_MEMORY_START) &&
		(cmd != OP_WRITE_MEMORY_CONTINUE))
		cfg2 |= (1 << 24); /* transparent mode */
	else
		cfg2 &= ~(1 << 24);

	cfg2 &= ~0xFF;
	cfg2 |= cmd;
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2, cfg2);
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_FIFO_SOF, 0x0);
	bytes_left = len;

	while (bytes_left > 0) {
		ret = qpic_lcdc_wait_for_fifo(qpic_display);
		if (ret)
			goto exit_send_cmd_sw;

		space = 16;

		while ((space > 0) && (bytes_left > 0)) {
			/* write to fifo */
			if (bytes_left >= 4) {
				QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_FIFO_DATA_PORT0,
					*(u32 *)param);
				param += 4;
				bytes_left -= 4;
				space--;
			} else if (bytes_left == 2) {
				QPIC_OUTPW(qpic_display, QPIC_REG_QPIC_LCDC_FIFO_DATA_PORT0,
					*(u16 *)param);
				bytes_left -= 2;
			}
		}
	}
	/* finished */
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_FIFO_EOF, 0x0);
	ret = qpic_lcdc_wait_for_eof(qpic_display);
exit_send_cmd_sw:
	cfg2 &= ~(1 << 24);
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2, cfg2);
	return ret;
}

int qpic_init_sps(struct qpic_display_data *qpic_display)
{
	int rc = 0;
	struct platform_device *pdev = qpic_display->pdev;
	struct qpic_sps_endpt *end_point = &qpic_display->qpic_endpt;
	struct sps_pipe *pipe_handle;
	struct sps_connect *sps_config = &end_point->config;
	struct sps_register_event *sps_event = &end_point->bam_event;
	struct sps_bam_props bam = {0};
	unsigned long bam_handle = 0;

	if (qpic_display->sps_init)
		return 0;

	bam.phys_addr = qpic_display->qpic_phys + 0x4000;
	bam.virt_addr = qpic_display->qpic_base + 0x4000;
	bam.irq = qpic_display->irq_id - 4;
	bam.manage = SPS_BAM_MGR_DEVICE_REMOTE | SPS_BAM_MGR_MULTI_EE;

	if (sps_phy2h(bam.phys_addr, &bam_handle)) {
		if (sps_register_bam_device(&bam, &bam_handle)) {
			pr_err("%s bam_handle is NULL\n", __func__);
			rc = -ENOMEM;
			goto out;
		}
	}

	pipe_handle = sps_alloc_endpoint();
	if (!pipe_handle) {
		pr_err("sps_alloc_endpoint() failed\n");
		rc = -ENOMEM;
		goto out;
	}

	rc = sps_get_config(pipe_handle, sps_config);
	if (rc) {
		pr_err("sps_get_config() failed %d\n", rc);
		goto free_endpoint;
	}

	/* WRITE CASE: source - system memory; destination - BAM */
	sps_config->source = SPS_DEV_HANDLE_MEM;
	sps_config->destination = bam_handle;
	sps_config->mode = SPS_MODE_DEST;
	sps_config->dest_pipe_index = 8;

	sps_config->options = SPS_O_AUTO_ENABLE | SPS_O_EOT;
	sps_config->lock_group = 0;
	/*
	 * Descriptor FIFO is a cyclic FIFO. If 64 descriptors
	 * are allowed to be submitted before we get any ack for any of them,
	 * the descriptor FIFO size should be: (SPS_MAX_DESC_NUM + 1) *
	 * sizeof(struct sps_iovec).
	 */
	sps_config->desc.size = (64) * sizeof(struct sps_iovec);
	sps_config->desc.base = dmam_alloc_coherent(&pdev->dev,
					sps_config->desc.size,
					&sps_config->desc.phys_base,
					GFP_KERNEL);
	if (!sps_config->desc.base) {
		pr_err("dmam_alloc_coherent() failed for size %x\n",
				sps_config->desc.size);
		rc = -ENOMEM;
		goto free_endpoint;
	}
	memset(sps_config->desc.base, 0x00, sps_config->desc.size);

	rc = sps_connect(pipe_handle, sps_config);
	if (rc) {
		pr_err("sps_connect() failed %d\n", rc);
		goto free_endpoint;
	}

	init_completion(&end_point->completion);
	sps_event->mode = SPS_TRIGGER_WAIT;
	sps_event->options = SPS_O_EOT;
	sps_event->xfer_done = &end_point->completion;
	sps_event->user = (void *)qpic_display;

	rc = sps_register_event(pipe_handle, sps_event);
	if (rc) {
		pr_err("sps_register_event() failed %d\n", rc);
		goto sps_disconnect;
	}

	end_point->handle = pipe_handle;
	qpic_display->sps_init = true;
	goto out;
sps_disconnect:
	sps_disconnect(pipe_handle);
free_endpoint:
	sps_free_endpoint(pipe_handle);
out:
	return rc;
}

void qpic_dump_lcdc_reg(struct qpic_display_data *qpic_display)
{
	pr_info("%s\n", __func__);
	pr_info("QPIC_REG_QPIC_LCDC_CTRL = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL));
	pr_info("QPIC_REG_QPIC_LCDC_CMD_DATA_CYCLE_CNT = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CMD_DATA_CYCLE_CNT));
	pr_info("QPIC_REG_QPIC_LCDC_CFG0 = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CFG0));
	pr_info("QPIC_REG_QPIC_LCDC_CFG1 = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CFG1));
	pr_info("QPIC_REG_QPIC_LCDC_CFG2 = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2));
	pr_info("QPIC_REG_QPIC_LCDC_IRQ_EN = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_EN));
	pr_info("QPIC_REG_QPIC_LCDC_IRQ_STTS = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_IRQ_STTS));
	pr_info("QPIC_REG_QPIC_LCDC_STTS = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_STTS));
	pr_info("QPIC_REG_QPIC_LCDC_FIFO_SOF = %x\n",
		QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_FIFO_SOF));
}

int qpic_send_pkt(struct qpic_display_data *qpic_display,
				u32 cmd, u8 *param, u32 len)
{
	if (!use_bam || ((cmd != OP_WRITE_MEMORY_CONTINUE) &&
			(cmd != OP_WRITE_MEMORY_START)))
		return qpic_send_pkt_sw(qpic_display, cmd, len, param);
	else
		return qpic_lcdc_send_pkt_bam(qpic_display, cmd, len, param);
}

int qpic_display_init(struct qpic_display_data *qpic_display)
{
	int ret = 0;
	u32 data;

	qpic_lcdc_reset(qpic_display);

	pr_info("%s QPIC version=%x\n", __func__,
			QPIC_INP(qpic_display, QPIC_REG_LCDC_VERSION));
	data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL);
	/* clear vsync wait , bam mode = 0*/
	data &= ~(3 << 0);
	data &= ~(0x1f << 3);
	data |= (1 << 3); /* threshold */
	data |= (1 << 8); /* lcd_en */
	data &= ~(0x1f << 9);
	data |= (1 << 9); /* threshold */
	QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL, data);

	if (use_irq && (!qpic_display->irq_requested)) {
		ret = devm_request_irq(&qpic_display->pdev->dev,
			qpic_display->irq_id, qpic_lcdc_irq_handler,
			IRQF_TRIGGER_NONE, "QPIC", &qpic_display);
		if (ret) {
			pr_err("qpic lcdc irq request failed! irq:%d\n", qpic_display->irq_id);
			use_irq = false;
		} else {
			disable_irq(qpic_display->irq_id);
		}
		qpic_display->irq_requested = true;
	}

	qpic_lcdc_interrupt_en(qpic_display, use_irq);

	data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2);
	if (qpic_display->panel_config->bpp == 24) {
		data &= ~(0xFFF);
		data |= 0x200; /* XRGB */
		data |= 0x2C;
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CFG2, data);
	}

	if (use_bam) {
		qpic_init_sps(qpic_display);
		data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL);
		data |= (1 << 1);
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL, data);
	}
	/* TE enable */
	if (use_vsync) {
		data = QPIC_INP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL);
		data |= (1 << 0);
		QPIC_OUTP(qpic_display, QPIC_REG_QPIC_LCDC_CTRL, data);
	}

	return ret;
}

/* write a frame of pixels to a MIPI screen */
u32 qpic_send_frame(struct qpic_display_data *qpic_display,
		u32 x_start, u32 y_start, u32 x_end, u32 y_end,
		u32 *data, u32 total_bytes)
{
	u8 param[4];
	u32 status;
	u32 start_0_7;
	u32 end_0_7;
	u32 start_8_15;
	u32 end_8_15;

	/* convert to 16 bit representation */
	x_start = x_start & 0xffff;
	y_start = y_start & 0xffff;
	x_end = x_end & 0xffff;
	y_end = y_end & 0xffff;

	/* set column/page */
	start_0_7 = x_start & 0xff;
	end_0_7 = x_end & 0xff;
	start_8_15 = (x_start >> 8) & 0xff;
	end_8_15 = (x_end >> 8) & 0xff;
	param[0] = start_8_15;
	param[1] = start_0_7;
	param[2] = end_8_15;
	param[3] = end_0_7;
	status = qpic_send_pkt(qpic_display, OP_SET_COLUMN_ADDRESS, param, 4);
	if (status) {
		pr_err("Failed to set column address\n");
		return status;
	}

	start_0_7 = y_start & 0xff;
	end_0_7 = y_end & 0xff;
	start_8_15 = (y_start >> 8) & 0xff;
	end_8_15 = (y_end >> 8) & 0xff;
	param[0] = start_8_15;
	param[1] = start_0_7;
	param[2] = end_8_15;
	param[3] = end_0_7;
	status = qpic_send_pkt(qpic_display, OP_SET_PAGE_ADDRESS, param, 4);
	if (status) {
		pr_err("Failed to set page address\n");
		return status;
	}

	status = qpic_send_pkt(qpic_display, OP_WRITE_MEMORY_START, (u8 *)data, total_bytes);
	if (status) {
		pr_err("Failed to start memory write\n");
		return status;
	}

	return 0;
}

static int qpic_panel_regulator_init(struct qpic_panel_io_desc *panel_io)
{
	int rc;

	if (panel_io->vdd_vreg) {
		rc = regulator_set_voltage(panel_io->vdd_vreg,
			1800000, 1800000);
		if (rc) {
			pr_err("iovdd_vreg->set_voltage failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	if (panel_io->avdd_vreg) {
		rc = regulator_set_voltage(panel_io->avdd_vreg,
			2704000, 2704000);
		if (rc) {
			pr_err("avdd_vreg->set_voltage failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}

	return 0;
}

static void panel_io_disable(struct qpic_panel_io_desc *qpic_panel_io)
{
	if (qpic_display_pinctrl_set_state(qpic_panel_io, false))
		pr_warn("%s panel on: pinctrl not enabled\n", __func__);

	if (qpic_panel_io->vdd_vreg)
		regulator_disable(qpic_panel_io->vdd_vreg);
	if (qpic_panel_io->avdd_vreg)
		regulator_disable(qpic_panel_io->avdd_vreg);
}

static void qpic_display_io_free(struct qpic_panel_io_desc *qpic_panel_io)
{
	if (qpic_panel_io->ad8_gpio)
		gpio_free(qpic_panel_io->ad8_gpio);
	if (qpic_panel_io->cs_gpio)
		gpio_free(qpic_panel_io->cs_gpio);
	if (qpic_panel_io->rst_gpio)
		gpio_free(qpic_panel_io->rst_gpio);
	if (qpic_panel_io->te_gpio)
		gpio_free(qpic_panel_io->te_gpio);
	if (qpic_panel_io->bl_gpio)
		gpio_free(qpic_panel_io->bl_gpio);
}

static int panel_io_enable(struct qpic_panel_io_desc *qpic_panel_io)
{
	int rc;

	if (qpic_panel_io->vdd_vreg) {
		rc = regulator_enable(qpic_panel_io->vdd_vreg);
		if (rc) {
			pr_err("enable vdd failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	if (qpic_panel_io->avdd_vreg) {
		rc = regulator_enable(qpic_panel_io->avdd_vreg);
		if (rc) {
			pr_err("enable avdd failed, rc=%d\n", rc);
			goto power_io_error;
		}
	}

	/* GPIO settings using pinctrl */
	if (qpic_display_pinctrl_set_state(qpic_panel_io, true))
		pr_warn("%s panel on: pinctrl not enabled\n", __func__);

	/* wait for 20 ms after enable gpio as suggested by hw */
	msleep(20);

	return 0;

power_io_error:
	if (qpic_panel_io->avdd_vreg)
		regulator_disable(qpic_panel_io->avdd_vreg);
	return -EINVAL;
}

static int qpic_display_io_request(struct qpic_panel_io_desc *qpic_panel_io)
{
	if ((qpic_panel_io->rst_gpio) &&
		(gpio_request(qpic_panel_io->rst_gpio, "disp_rst_n"))) {
		pr_err("%s request reset gpio failed\n", __func__);
		goto power_io_error;
	}

	if ((qpic_panel_io->cs_gpio) &&
		(gpio_request(qpic_panel_io->cs_gpio, "disp_cs_n"))) {
		pr_err("%s request cs gpio failed\n", __func__);
		goto power_io_error;
	}

	if ((qpic_panel_io->ad8_gpio) &&
		(gpio_request(qpic_panel_io->ad8_gpio, "disp_ad8_n"))) {
		pr_err("%s request ad8 gpio failed\n", __func__);
		goto power_io_error;
	}

	if ((qpic_panel_io->te_gpio) &&
		(gpio_request(qpic_panel_io->te_gpio, "disp_te_n"))) {
		pr_err("%s request te gpio failed\n", __func__);
		goto power_io_error;
	}

	if ((qpic_panel_io->bl_gpio) &&
		(gpio_request(qpic_panel_io->bl_gpio, "disp_bl_n"))) {
		pr_err("%s request bl gpio failed\n", __func__);
		goto power_io_error;
	}

	return 0;

power_io_error:
	return -EINVAL;
}

int qpic_display_on(struct qpic_display_data *qpic_display)
{
	int rc = 0;

	qpic_display_clk_ctrl(qpic_display, true);

	if (qpic_display->is_qpic_on) {
		pr_info("qpic already enabled\n");
		return rc;
	}

	rc = qpic_display_init(qpic_display);
	if (rc)
		return rc;

	if (qpic_display->panel_on && !qpic_display->is_panel_on) {
		panel_io_enable(&qpic_display->panel_io);
		rc = qpic_display->panel_on(qpic_display);
		if (rc) {
			pr_err("failed to enable panel\n");
			return -EINVAL;
		}
		qpic_display->is_panel_on = true;
	} else {
		pr_info("panel on function is not specified or panel already on\n");
		return 0;
	}

	qpic_display->is_qpic_on = true;
	qpic_display->is_panel_on = true;

	return 0;
}

int qpic_display_off(struct qpic_display_data *qpic_display)
{

	if (qpic_display->panel_off && qpic_display->is_panel_on) {
		qpic_display->panel_off(qpic_display);
		qpic_display->is_panel_on = false;
	}

	if (use_irq)
		qpic_lcdc_interrupt_en(qpic_display, false);

	panel_io_disable(&qpic_display->panel_io);

	qpic_display_clk_ctrl(qpic_display, false);

	qpic_display->is_qpic_on = false;
	qpic_display->is_panel_on = false;

	return 0;
}

static void qpic_display_driver_release(struct drm_device *dev)
{
	DRM_DEBUG_DRIVER("\n");

	drm_mode_config_cleanup(dev);
	drm_dev_fini(dev);
}

#define DRIVER_NAME		"drm_qpic"
#define DRIVER_DESC		"DRM QPIC display driver"
#define DRIVER_DATE		"2021"
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0

DEFINE_DRM_GEM_CMA_FOPS(qpic_display_fops);

static struct drm_driver qpic_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,

	.name		 = DRIVER_NAME,
	.desc		 = DRIVER_DESC,
	.date		 = DRIVER_DATE,
	.major		 = DRIVER_MAJOR,
	.minor		 = DRIVER_MINOR,

	.release	 = qpic_display_driver_release,
	.fops		 = &qpic_display_fops,
	DRM_GEM_CMA_VMAP_DRIVER_OPS,
};

static const struct drm_mode_config_funcs qpic_mode_config_funcs = {
	.fb_create = drm_gem_fb_create_with_dirty,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void qpic_display_fb_mark_dirty(struct drm_framebuffer *fb, struct drm_rect *rect)
{
	u32 size;
	struct drm_gem_cma_object *cma_obj = NULL;
	struct dma_buf_attachment *import_attach = NULL;
	struct qpic_display_data *qpic_display = fb->dev->dev_private;

	if (!qpic_display->is_qpic_on || !qpic_display->is_panel_on) {
		pr_info("%s: qpic or panel is not enabled\n", __func__);
		return;
	}

	cma_obj = drm_fb_cma_get_gem_obj(fb, 0);
	if (!cma_obj) {
		pr_err("failed to get gem obj\n");
		return;
	}
	import_attach = cma_obj->base.import_attach;

	/* currently QPIC display SW can't support partial updates */
	rect->x1 = 0;
	rect->x2 = fb->width;
	rect->y1 = 0;
	rect->y2 = fb->height;

	drm_framebuffer_get(fb);

	if (import_attach)
		dma_buf_begin_cpu_access(import_attach->dmabuf, DMA_FROM_DEVICE);

	msm_qpic_bus_set_vote(qpic_display, 1);
	size = fb->width * fb->height * fb->format->depth / 8;

	qpic_send_frame(qpic_display, rect->x1, rect->x2,
		rect->y1 - 1, rect->y2 - 1,
		use_bam ? (u32 *)cma_obj->paddr:(u32 *)cma_obj->vaddr, size);

	msm_qpic_bus_set_vote(qpic_display, 0);
	drm_framebuffer_put(fb);

	if (import_attach)
		dma_buf_end_cpu_access(import_attach->dmabuf, DMA_FROM_DEVICE);

}

static void qpic_display_pipe_enable(struct drm_simple_display_pipe *pipe,
				 struct drm_crtc_state *crtc_state,
				 struct drm_plane_state *plane_state)
{
	struct qpic_display_data *qpic_display = pipe->crtc.dev->dev_private;

	qpic_display_on(qpic_display);
	qpic_display->pipe_enabled = true;
}

static void qpic_display_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	struct qpic_display_data *qpic_display = pipe->crtc.dev->dev_private;

	qpic_display->pipe_enabled = false;
	qpic_display_off(qpic_display);
}

static void qpic_display_pipe_update(struct drm_simple_display_pipe *pipe,
				 struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = pipe->plane.state;
	struct qpic_display_data *qpic_display = pipe->crtc.dev->dev_private;
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_rect rect = { 0, 0,
		qpic_display->panel_config->xres, qpic_display->panel_config->yres };

	if (drm_atomic_helper_damage_merged(old_state, state, &rect))
		qpic_display_fb_mark_dirty(pipe->plane.state->fb, &rect);

	if (crtc->state->event) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
		spin_unlock_irq(&crtc->dev->event_lock);
	}
}

static int qpic_display_conn_get_modes(struct drm_connector *connector)
{
	struct qpic_display_data *qpic_display = connector->dev->dev_private;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &qpic_display->drm_mode);
	if (!mode) {
		DRM_ERROR("Failed to duplicate mode\n");
		return 0;
	}

	drm_mode_set_name(mode);
	mode->type |= DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_connector_helper_funcs qpic_display_conn_helper_funcs = {
	.get_modes = qpic_display_conn_get_modes,
};

static const struct drm_connector_funcs qpic_display_conn_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int qpic_display_conn_init(struct qpic_display_data *qpic_display)
{
	drm_connector_helper_add(&qpic_display->conn, &qpic_display_conn_helper_funcs);
	return drm_connector_init(&qpic_display->drm_dev, &qpic_display->conn,
				  &qpic_display_conn_funcs, DRM_MODE_CONNECTOR_9PinDIN);
}

static void qpic_display_drm_mode_init(struct qpic_display_data *qpic_display)
{
	struct qpic_panel_config *panel_config =  qpic_display->panel_config;
	struct drm_display_mode qpic_display_mode = {
		DRM_SIMPLE_MODE(panel_config->xres, panel_config->yres, 0, 0),
	};

	drm_mode_copy(&qpic_display->drm_mode, &qpic_display_mode);

	if (panel_config->bpp == 16)
		qpic_pipe_formats[0] = DRM_FORMAT_RGB565;
	else if (panel_config->bpp == 24)
		qpic_pipe_formats[0] = DRM_FORMAT_RGB888;
}

static const struct drm_simple_display_pipe_funcs qpic_pipe_funcs = {
	.enable    = qpic_display_pipe_enable,
	.disable   = qpic_display_pipe_disable,
	.update    = qpic_display_pipe_update,
};

static int qpic_get_panel_config(struct platform_device *pdev,
	struct qpic_display_data *qpic_display)
{
	struct device_node *np = pdev->dev.of_node;
	static const char *panel_name;

	panel_name = of_get_property(np, "panel-name", NULL);
	if (panel_name)
		pr_info("%s: panel name = %s\n", __func__, panel_name);
	else
		pr_info("panel name not specified\n");

	/* select panel according to panel name */
	if (panel_name && !strcmp(panel_name, "ili_qvga")) {
		pr_info("%s: select ili qvga lcdc panel\n");
		get_ili_qvga_panel_config(qpic_display);
	} else {
		/* select default panel */
		pr_info("%s: select default panel\n");
		get_ili_qvga_panel_config(qpic_display);
	}

	if (!qpic_display->panel_config) {
		pr_err("get panel config failed\n");
		return -EINVAL;
	}

	return 0;
}

static int qpic_display_pinctrl_init(struct platform_device *pdev,
		struct qpic_panel_io_desc *qpic_panel_io)
{
	qpic_panel_io->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(qpic_panel_io->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(qpic_panel_io->pin_res.pinctrl);
	}

	qpic_panel_io->pin_res.gpio_state_active
		= pinctrl_lookup_state(qpic_panel_io->pin_res.pinctrl,
				QPIC_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(qpic_panel_io->pin_res.gpio_state_active))
		pr_warn("%s: cannot get default pinstate\n", __func__);

	qpic_panel_io->pin_res.gpio_state_suspend
		= pinctrl_lookup_state(qpic_panel_io->pin_res.pinctrl,
				QPIC_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(qpic_panel_io->pin_res.gpio_state_suspend))
		pr_warn("%s: cannot get sleep pinstate\n", __func__);

	return 0;
}

int qpic_display_io_init(struct platform_device *pdev,
	struct qpic_panel_io_desc *qpic_panel_io)
{
	int rc = 0;
	struct device_node *np = pdev->dev.of_node;
	int rst_gpio, cs_gpio, te_gpio, ad8_gpio, bl_gpio;
	struct regulator *vdd_vreg;
	struct regulator *avdd_vreg;

	rc = qpic_display_pinctrl_init(pdev, qpic_panel_io);
	if (rc) {
		pr_err("%s: failed to get pin resources\n", __func__);
		return rc;
	}

	rst_gpio = of_get_named_gpio(np, "qcom,rst-gpio", 0);
	cs_gpio = of_get_named_gpio(np, "qcom,cs-gpio", 0);
	ad8_gpio = of_get_named_gpio(np, "qcom,ad8-gpio", 0);
	te_gpio = of_get_named_gpio(np, "qcom,panel-te-gpio", 0);
	bl_gpio = of_get_named_gpio(np, "qcom,panel-bl-gpio", 0);

	if (!gpio_is_valid(rst_gpio))
		pr_warn("%s: reset gpio not specified\n", __func__);
	else
		qpic_panel_io->rst_gpio = rst_gpio;

	if (!gpio_is_valid(cs_gpio))
		pr_warn("%s: cs gpio not specified\n", __func__);
	else
		qpic_panel_io->cs_gpio = cs_gpio;

	if (!gpio_is_valid(ad8_gpio))
		pr_warn("%s: ad8 gpio not specified\n", __func__);
	else
		qpic_panel_io->ad8_gpio = ad8_gpio;

	if (!gpio_is_valid(te_gpio))
		pr_warn("%s: te gpio not specified\n", __func__);
	else
		qpic_panel_io->te_gpio = te_gpio;

	if (!gpio_is_valid(bl_gpio))
		pr_warn("%s: te gpio not specified\n", __func__);
	else
		qpic_panel_io->bl_gpio = bl_gpio;

	vdd_vreg = devm_regulator_get_optional(&pdev->dev, "vdd");
	if (IS_ERR_OR_NULL(vdd_vreg))
		pr_err("%s could not get vdd\n", __func__);
	else
		qpic_panel_io->vdd_vreg = vdd_vreg;

	avdd_vreg = devm_regulator_get_optional(&pdev->dev, "avdd");
	if (IS_ERR_OR_NULL(avdd_vreg))
		pr_err("%s could not get avdd\n", __func__);
	else
		qpic_panel_io->avdd_vreg = avdd_vreg;

	return 0;
}

int qpic_display_alloc_cmd_buf(struct qpic_display_data *qpic_display)
{
	if (!qpic_display->res_init)
		return -EINVAL;

	if (qpic_display->cmd_buf_virt)
		return 0;

	qpic_display->cmd_buf_virt = dmam_alloc_coherent(
		&qpic_display->pdev->dev, QPIC_MAX_CMD_BUF_SIZE_IN_BYTES,
		&qpic_display->cmd_buf_phys, GFP_KERNEL);
	if (!qpic_display->cmd_buf_virt) {
		pr_err("%s cmd buf allocation failed\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s cmd_buf virt=%x phys=%x\n", __func__,
		(int) qpic_display->cmd_buf_virt,
		qpic_display->cmd_buf_phys);

	return 0;
}

int qpic_display_get_resource(struct qpic_display_data *qpic_display)
{
	struct resource *res;
	int rc;
	struct platform_device *pdev = qpic_display->pdev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qpic_base");
	if (!res) {
		pr_err("unable to get QPIC reg base address\n");
		return -ENOMEM;
	}

	qpic_display->qpic_reg_size = resource_size(res);
	qpic_display->qpic_base = devm_ioremap(&pdev->dev, res->start,
					qpic_display->qpic_reg_size);
	if (unlikely(!qpic_display->qpic_base)) {
		pr_err("unable to map MDSS QPIC base\n");
		return -ENOMEM;
	}
	qpic_display->qpic_phys = res->start;
	pr_info("MDSS QPIC HW Base phy_Address=0x%x virt=0x%x\n",
		(int) res->start,
		(int) qpic_display->qpic_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get QPIC irq\n");
		return -ENODEV;
	}

	qpic_display->qpic_clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(qpic_display->qpic_clk)) {
		qpic_display->qpic_clk = NULL;
		pr_err("%s: Can't find core_clk\n", __func__);
		return -ENODEV;
	}
	qpic_display->irq_id = res->start;
	qpic_display->res_init = true;

	return 0;
}

static int qpic_display_probe(struct platform_device *pdev)
{
	struct drm_device *drm_dev;
	struct qpic_display_data *qpic_display;
	int rc;

	if (!pdev->dev.of_node) {
		pr_err("qpic driver only supports device tree probe\n");
		return -EOPNOTSUPP;
	}

	qpic_display = devm_kzalloc(&pdev->dev, sizeof(*qpic_display), GFP_KERNEL);
	if (!qpic_display)
		return -ENOMEM;

	qpic_display->pdev = pdev;
	platform_set_drvdata(pdev, qpic_display);

	rc = qpic_get_panel_config(pdev, qpic_display);
	if (rc) {
		pr_err("failed to get panel config\n");
		goto out;
	}

	qpic_display->qpic_transfer = qpic_send_pkt;

	rc = qpic_display_get_resource(qpic_display);
	if (rc) {
		pr_err("qpic display get resource failed, rc = %d\n", rc);
		goto out;
	}

	rc = qpic_display_io_init(pdev, &qpic_display->panel_io);
	if (rc) {
		pr_err("qpic display IO init failed, rc = %d\n", rc);
		goto out;
	}

	rc = qpic_panel_regulator_init(&qpic_display->panel_io);
	if (rc) {
		pr_err("qpic panel regulator init failed, rc = %d\n", rc);
		goto out;
	}

	rc = qpic_display_io_request(&qpic_display->panel_io);
	if (rc) {
		pr_err("qpic display io request failed, rc = %d\n", rc);
		goto out;
	}

	rc = qpic_display_bus_register(pdev, qpic_display);
	if (rc) {
		pr_err("qpic display bus register failed, rc = %d\n", rc);
		goto out;
	}

	rc = qpic_display_alloc_cmd_buf(qpic_display);
	if (rc) {
		pr_err("qpic display allocate cmd buffer failed, rc = %d\n", rc);
		goto bus_unregister;
	}

	drm_dev = &qpic_display->drm_dev;
	rc = drm_dev_init(drm_dev, &qpic_drm_driver, &pdev->dev);
	if (rc || !drm_dev) {
		pr_err("drm_dev_init failed, rc = %d\n", rc);
		goto free_buf;
	}
	drm_dev->dev_private = qpic_display;

	qpic_display_drm_mode_init(qpic_display);

	drm_mode_config_init(drm_dev);
	drm_dev->mode_config.funcs = &qpic_mode_config_funcs;
	drm_dev->mode_config.min_width = qpic_display->drm_mode.hdisplay;
	drm_dev->mode_config.max_width = qpic_display->drm_mode.hdisplay;
	drm_dev->mode_config.min_height = qpic_display->drm_mode.vdisplay;
	drm_dev->mode_config.max_height = qpic_display->drm_mode.vdisplay;

	rc = qpic_display_conn_init(qpic_display);
	if (rc) {
		pr_err("qpic display connector init failed, rc = %d\n", rc);
		goto err_put;
	}

	/* initialize a simple pipe line for drm driver*/
	rc = drm_simple_display_pipe_init(&qpic_display->drm_dev,
					   &qpic_display->pipe,
					   &qpic_pipe_funcs,
					   qpic_pipe_formats,
					   ARRAY_SIZE(qpic_pipe_formats),
					   qpic_pipe_modifiers,
					   &qpic_display->conn);
	if (rc)
		goto err_put;

	drm_mode_config_reset(drm_dev);

	rc = drm_dev_register(drm_dev, 0);
	if (rc)
		goto err_put;

	return rc;

err_put:
	drm_dev_put(drm_dev);
free_buf:
	dmam_free_coherent(&qpic_display->pdev->dev, QPIC_MAX_CMD_BUF_SIZE_IN_BYTES,
			qpic_display->cmd_buf_virt, qpic_display->cmd_buf_phys);
bus_unregister:
	qpic_display_bus_unregister(qpic_display);
out:
	return rc;
}

static int qpic_display_remove(struct platform_device *pdev)
{
	struct qpic_display_data *qpic_display = platform_get_drvdata(pdev);

	drm_dev_unplug(&qpic_display->drm_dev);
	drm_atomic_helper_shutdown(&qpic_display->drm_dev);

	qpic_display_io_free(&qpic_display->panel_io);
	qpic_display_clk_ctrl(qpic_display, 0);
	msm_qpic_bus_set_vote(qpic_display, 0);
	qpic_display_bus_unregister(qpic_display);
	return 0;
}

static const struct of_device_id qpic_display_dt_match[] = {
	{ .compatible = "qcom,mdss_qpic",},
	{}
};

static struct platform_driver qpic_display_driver = {
	.probe = qpic_display_probe,
	.remove = qpic_display_remove,
	.driver = {
		.name = "qpic_display",
		.of_match_table = qpic_display_dt_match,
	},
};

static int __init qpic_display_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&qpic_display_driver);
	if (ret)
		pr_err("qpic_display_register_driver() failed!,ret=%d\n", ret);
	return ret;
}

MODULE_DEVICE_TABLE(of, qpic_display_dt_match);

module_init(qpic_display_driver_init);

