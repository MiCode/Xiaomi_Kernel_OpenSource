/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/bootmem.h>
#include <linux/dma-mapping.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/sps.h>
#include <mach/clk.h>
#include <mach/hardware.h>

#include "mdss_fb.h"
#include "mdss_qpic.h"

static int mdss_qpic_probe(struct platform_device *pdev);
static int mdss_qpic_remove(struct platform_device *pdev);

struct qpic_data_type *qpic_res;

/* for tuning */
static u32 use_bam = true;
static u32 use_irq;
static u32 use_vsync;

static const struct of_device_id mdss_qpic_dt_match[] = {
	{ .compatible = "qcom,mdss_qpic",},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_qpic_dt_match);

static struct platform_driver mdss_qpic_driver = {
	.probe = mdss_qpic_probe,
	.remove = mdss_qpic_remove,
	.shutdown = NULL,
	.driver = {
		/*
		 * Simulate mdp hw
		 */
		.name = "mdp",
		.of_match_table = mdss_qpic_dt_match,
	},
};

int qpic_on(struct msm_fb_data_type *mfd)
{
	int ret;
	ret = mdss_qpic_panel_on(qpic_res->panel_data);
	return ret;
}

int qpic_off(struct msm_fb_data_type *mfd)
{
	int ret;
	ret = mdss_qpic_panel_off(qpic_res->panel_data);
	return ret;
}

static void mdss_qpic_pan_display(struct msm_fb_data_type *mfd)
{

	struct fb_info *fbi;
	u32 offset, fb_offset, size;
	int bpp;

	if (!mfd) {
		pr_err("%s: mfd is NULL!", __func__);
		return;
	}

	fbi = mfd->fbi;

	bpp = fbi->var.bits_per_pixel / 8;
	offset = fbi->var.xoffset * bpp +
		 fbi->var.yoffset * fbi->fix.line_length;

	if (offset > fbi->fix.smem_len) {
		pr_err("invalid fb offset=%u total length=%u\n",
		       offset, fbi->fix.smem_len);
		return;
	}
	fb_offset = (u32)fbi->fix.smem_start + offset;

	mdss_qpic_panel_on(qpic_res->panel_data);
	size = fbi->var.xres * fbi->var.yres * bpp;

	qpic_send_frame(0, 0, fbi->var.xres, fbi->var.yres,
		(u32 *)fb_offset, size);
}

int mdss_qpic_alloc_fb_mem(struct msm_fb_data_type *mfd)
{
	size_t size;
	u32 yres = mfd->fbi->var.yres_virtual;

	size = PAGE_ALIGN(mfd->fbi->fix.line_length * yres);

	if (!qpic_res->res_init)
		return -EINVAL;

	if (mfd->index != 0) {
		mfd->fbi->fix.smem_start = 0;
		mfd->fbi->screen_base = NULL;
		mfd->fbi->fix.smem_len = 0;
		mfd->iova = 0;
		return 0;
	}

	if (!qpic_res->fb_virt) {
		qpic_res->fb_virt = (void *)dmam_alloc_coherent(
						&qpic_res->pdev->dev,
						size + QPIC_MAX_CMD_BUF_SIZE,
						&qpic_res->fb_phys,
						GFP_KERNEL);
		pr_err("%s size=%d vir_addr=%x phys_addr=%x",
			__func__, size, (int)qpic_res->fb_virt,
			(int)qpic_res->fb_phys);
		if (!qpic_res->fb_virt)
			return -ENOMEM;
		qpic_res->cmd_buf_virt = qpic_res->fb_virt + size;
		qpic_res->cmd_buf_phys = qpic_res->fb_phys + size;
	}
	mfd->fbi->fix.smem_start = qpic_res->fb_phys;
	mfd->fbi->screen_base = qpic_res->fb_virt;
	mfd->fbi->fix.smem_len = size;
	mfd->iova = 0;
	return 0;
}

u32 mdss_qpic_fb_stride(u32 fb_index, u32 xres, int bpp)
{
	return xres * bpp;
}

int mdss_qpic_overlay_init(struct msm_fb_data_type *mfd)
{
	struct msm_mdp_interface *qpic_interface = &mfd->mdp;
	qpic_interface->on_fnc = qpic_on;
	qpic_interface->off_fnc = qpic_off;
	qpic_interface->do_histogram = NULL;
	qpic_interface->cursor_update = NULL;
	qpic_interface->dma_fnc = mdss_qpic_pan_display;
	qpic_interface->ioctl_handler = NULL;
	qpic_interface->kickoff_fnc = NULL;
	return 0;
}

int qpic_register_panel(struct mdss_panel_data *pdata)
{
	struct platform_device *mdss_fb_dev = NULL;
	int rc;

	mdss_fb_dev = platform_device_alloc("mdss_fb", pdata->panel_info.pdest);
	if (!mdss_fb_dev) {
		pr_err("unable to allocate mdss_fb device\n");
		return -ENOMEM;
	}

	mdss_fb_dev->dev.platform_data = pdata;

	rc = platform_device_add(mdss_fb_dev);
	if (rc) {
		platform_device_put(mdss_fb_dev);
		pr_err("unable to probe mdss_fb device (%d)\n", rc);
		return rc;
	}

	qpic_res->panel_data = pdata;

	return rc;
}

int qpic_init_sps(struct platform_device *pdev,
				struct qpic_sps_endpt *end_point)
{
	int rc = 0;
	struct sps_pipe *pipe_handle;
	struct sps_connect *sps_config = &end_point->config;
	struct sps_register_event *sps_event = &end_point->bam_event;
	struct sps_bam_props bam = {0};
	u32 bam_handle = 0;

	if (qpic_res->sps_init)
		return 0;
	bam.phys_addr = qpic_res->qpic_phys + 0x4000;
	bam.virt_addr = qpic_res->qpic_base + 0x4000;
	bam.irq = qpic_res->irq - 4;
	bam.manage = SPS_BAM_MGR_DEVICE_REMOTE | SPS_BAM_MGR_MULTI_EE;

	rc = sps_phy2h(bam.phys_addr, &bam_handle);
	if (rc)
		rc = sps_register_bam_device(&bam, &bam_handle);
	if (rc) {
		pr_err("%s bam_handle is NULL", __func__);
		rc = -ENOMEM;
		goto out;
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
	sps_config->dest_pipe_index = 6;

	sps_config->options = SPS_O_AUTO_ENABLE | SPS_O_EOT;
	sps_config->lock_group = 0;
	/*
	 * Descriptor FIFO is a cyclic FIFO. If 64 descriptors
	 * are allowed to be submitted before we get any ack for any of them,
	 * the descriptor FIFO size should be: (SPS_MAX_DESC_NUM + 1) *
	 * sizeof(struct sps_iovec).
	 */
	sps_config->desc.size = (64) *
					sizeof(struct sps_iovec);
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
	sps_event->user = (void *)qpic_res;

	rc = sps_register_event(pipe_handle, sps_event);
	if (rc) {
		pr_err("sps_register_event() failed %d\n", rc);
		goto sps_disconnect;
	}

	end_point->handle = pipe_handle;
	qpic_res->sps_init = true;
	goto out;
sps_disconnect:
	sps_disconnect(pipe_handle);
free_endpoint:
	sps_free_endpoint(pipe_handle);
out:
	return rc;
}

void mdss_qpic_reset(void)
{
	u32 time_end;

	QPIC_OUTP(QPIC_REG_QPIC_LCDC_RESET, 1 << 0);
	/* wait 100 us after reset as suggested by hw */
	usleep(100);
	time_end = (u32)ktime_to_ms(ktime_get()) + QPIC_MAX_VSYNC_WAIT_TIME;
	while (((QPIC_INP(QPIC_REG_QPIC_LCDC_STTS) & (1 << 8)) == 0)) {
		if ((u32)ktime_to_ms(ktime_get()) > time_end) {
			pr_err("%s reset not finished", __func__);
			break;
		}
		/* yield 100 us for next polling by experiment*/
		usleep(100);
	}
}

void qpic_interrupt_en(u32 en)
{
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_IRQ_CLR, 0xff);
	if (en) {
		if (!qpic_res->irq_ena) {
			qpic_res->irq_ena = true;
			enable_irq(qpic_res->irq);
			QPIC_OUTP(QPIC_REG_QPIC_LCDC_IRQ_EN,
				(1 << 0) | (1 << 2));
		}
	} else {
		QPIC_OUTP(QPIC_REG_QPIC_LCDC_IRQ_EN, 0);
		disable_irq(qpic_res->irq);
		qpic_res->irq_ena = false;
	}
}

static irqreturn_t qpic_irq_handler(int irq, void *ptr)
{
	u32 data;
	data = QPIC_INP(QPIC_REG_QPIC_LCDC_IRQ_STTS);
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_IRQ_CLR, 0xff);
	return 0;
}

int qpic_flush_buffer_bam(u32 cmd, u32 len, u32 *param, u32 is_cmd)
{
	int  ret = 0;
	u32 phys_addr, cfg2, block_len , flags;
	if (is_cmd) {
		memcpy((u8 *)qpic_res->cmd_buf_virt, param, len);
		invalidate_caches((unsigned long)qpic_res->cmd_buf_virt,
		len,
		(unsigned long)qpic_res->cmd_buf_phys);
		phys_addr = qpic_res->cmd_buf_phys;
	} else {
		phys_addr = (u32)param;
	}

	cfg2 = QPIC_INP(QPIC_REG_QPIC_LCDC_CFG2);
	cfg2 &= ~0xFF;
	cfg2 |= cmd;
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_CFG2, cfg2);
	block_len = 0x7FF0;
	while (len > 0)  {
		if (len <= 0x7FF0) {
			flags = SPS_IOVEC_FLAG_EOT;
			block_len = len;
		} else {
			flags = 0;
		}
		ret = sps_transfer_one(qpic_res->qpic_endpt.handle,
				phys_addr, block_len, NULL, flags);
		if (ret)
			pr_err("failed to submit command %x ret %d\n",
				cmd, ret);
		phys_addr += block_len;
		len -= block_len;
	}
	ret = wait_for_completion_interruptible_timeout(
		&qpic_res->qpic_endpt.completion,
		msecs_to_jiffies(100 * 4));
	if (ret <= 0)
		pr_err("%s timeout %x", __func__, ret);
	else
		ret = 0;
	return ret;
}

int qpic_flush_buffer_sw(u32 cmd, u32 len, u32 *param, u32 is_cmd)
{
	u32 bytes_left, space, data, cfg2, time_end;
	int i, ret = 0;
	if ((len <= (sizeof(u32) * 4)) && (is_cmd)) {
		len >>= 2;/* len in dwords */
		data = 0;
		for (i = 0; i < len; i++)
			data |= param[i] << (8 * i);
		QPIC_OUTP(QPIC_REG_QPIC_LCDC_CMD_DATA_CYCLE_CNT, len);
		QPIC_OUTP(QPIC_REG_LCD_DEVICE_CMD0 + (4 * cmd), data);
		return 0;
	}
	if ((len & 0x1) != 0) {
		pr_err("%s: number of bytes needs be even", __func__);
		len = (len + 1) & (~0x1);
	}
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_IRQ_CLR, 0xff);
	cfg2 = QPIC_INP(QPIC_REG_QPIC_LCDC_CFG2);
	cfg2 |= (1 << 24); /* transparent mode */
	cfg2 &= ~0xFF;
	cfg2 |= cmd;
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_CFG2, cfg2);
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_FIFO_SOF, 0x0);
	bytes_left = len;
	while (bytes_left > 0) {
		time_end = (u32)ktime_to_ms(ktime_get()) +
			QPIC_MAX_VSYNC_WAIT_TIME;
		while (1) {
			data = QPIC_INP(QPIC_REG_QPIC_LCDC_STTS);
			data &= 0x3F;
			if (data == 0)
				break;
			/* yield 10 us for next polling by experiment*/
			usleep(10);
			if (ktime_to_ms(ktime_get()) > time_end) {
				pr_err("%s time out", __func__);
				ret = -EBUSY;
				goto exit_send_cmd_sw;
			}
		}
		space = (16 - data);

		while ((space > 0) && (bytes_left > 0)) {
			/* write to fifo */
			if (bytes_left >= 4) {
				QPIC_OUTP(QPIC_REG_QPIC_LCDC_FIFO_DATA_PORT0,
					param[0]);
				param++;
				bytes_left -= 4;
				space--;
			} else if (bytes_left == 2) {
				QPIC_OUTPW(QPIC_REG_QPIC_LCDC_FIFO_DATA_PORT0,
					*(u16 *)param);
				bytes_left -= 2;
			}
		}
	}
	/* finished */
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_FIFO_EOF, 0x0);

	time_end = (u32)ktime_to_ms(ktime_get()) + QPIC_MAX_VSYNC_WAIT_TIME;
	while (1) {
		data = QPIC_INP(QPIC_REG_QPIC_LCDC_IRQ_STTS);
		if (data & (1 << 2))
			break;
		/* yield 10 us for next polling by experiment*/
		usleep(10);
		if (ktime_to_ms(ktime_get()) > time_end) {
			pr_err("%s wait for eof time out", __func__);
			ret = -EBUSY;
			goto exit_send_cmd_sw;
		}
	}
exit_send_cmd_sw:
	cfg2 &= ~(1 << 24);
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_CFG2, cfg2);
	return ret;
}

int qpic_flush_buffer(u32 cmd, u32 len, u32 *param, u32 is_cmd)
{
	if (use_bam) {
		if (is_cmd)
			return qpic_flush_buffer_sw(cmd, len, param, is_cmd);
		else
			return qpic_flush_buffer_bam(cmd, len, param, is_cmd);
	} else {
		return qpic_flush_buffer_sw(cmd, len, param, is_cmd);
	}
}

int mdss_qpic_init(void)
{
	int ret = 0;
	u32 data;
	mdss_qpic_reset();

	pr_info("%s version=%x", __func__, QPIC_INP(QPIC_REG_LCDC_VERSION));
	data = QPIC_INP(QPIC_REG_QPIC_LCDC_CTRL);
	/* clear vsync wait , bam mode = 0*/
	data &= ~(3 << 0);
	data &= ~(0x1f << 3);
	data |= (1 << 3); /* threshold */
	data |= (1 << 8); /* lcd_en */
	data &= ~(0x1f << 9);
	data |= (1 << 9); /* threshold */
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_CTRL, data);

	if (use_irq && qpic_res->irq_requested) {
		ret = devm_request_irq(&qpic_res->pdev->dev,
			qpic_res->irq, qpic_irq_handler,
			IRQF_DISABLED,	"QPIC", qpic_res);
		if (ret) {
			pr_err("qpic request_irq() failed!\n");
			use_irq = false;
		}
		qpic_res->irq_requested = true;
	}

	qpic_interrupt_en(use_irq);
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_CFG0, 0x02108501);
	data = QPIC_INP(QPIC_REG_QPIC_LCDC_CFG2);
	data &= ~(0xFFF);
	data |= 0x200; /* XRGB */
	data |= 0x2C;
	QPIC_OUTP(QPIC_REG_QPIC_LCDC_CFG2, data);

	if (use_bam) {
		qpic_init_sps(qpic_res->pdev , &qpic_res->qpic_endpt);
		data = QPIC_INP(QPIC_REG_QPIC_LCDC_CTRL);
		data |= (1 << 1);
		QPIC_OUTP(QPIC_REG_QPIC_LCDC_CTRL, data);
	}
	/* TE enable */
	if (use_vsync) {
		data = QPIC_INP(QPIC_REG_QPIC_LCDC_CTRL);
		data |= (1 << 0);
		QPIC_OUTP(QPIC_REG_QPIC_LCDC_CTRL, data);
	}

	return ret;
}

static int mdss_qpic_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc = 0;
	static struct msm_mdp_interface qpic_interface = {
		.init_fnc = mdss_qpic_overlay_init,
		.fb_mem_alloc_fnc = mdss_qpic_alloc_fb_mem,
		.fb_stride = mdss_qpic_fb_stride,
	};


	if (!pdev->dev.of_node) {
		pr_err("qpic driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	if (!qpic_res)
		qpic_res = devm_kzalloc(&pdev->dev,
			sizeof(*qpic_res), GFP_KERNEL);

	if (!qpic_res)
		return -ENOMEM;

	if (qpic_res->res_init) {
		pr_err("qpic already initialized\n");
		return -EINVAL;
	}

	pdev->id = 0;

	qpic_res->pdev = pdev;
	platform_set_drvdata(pdev, qpic_res);

	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "qpic_base");
	if (!res) {
		pr_err("unable to get QPIC reg base address\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	qpic_res->qpic_reg_size = resource_size(res);
	qpic_res->qpic_base = devm_ioremap(&pdev->dev, res->start,
					qpic_res->qpic_reg_size);
	if (unlikely(!qpic_res->qpic_base)) {
		pr_err("unable to map MDSS QPIC base\n");
		rc = -ENOMEM;
		goto probe_done;
	}
	qpic_res->qpic_phys = res->start;
	pr_info("MDSS QPIC HW Base phy_Address=0x%x virt=0x%x\n",
		(int) res->start,
		(int) qpic_res->qpic_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get QPIC irq\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	qpic_res->irq = res->start;
	qpic_res->res_init = true;

	rc = mdss_fb_register_mdp_instance(&qpic_interface);
	if (rc)
		pr_err("unable to register QPIC instance\n");

probe_done:
	return rc;
}

static int mdss_qpic_remove(struct platform_device *pdev)
{
	return 0;
}

static int __init mdss_qpic_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&mdss_qpic_driver);
	if (ret)
		pr_err("mdss_qpic_register_driver() failed!\n");
	return ret;
}

module_init(mdss_qpic_driver_init);


