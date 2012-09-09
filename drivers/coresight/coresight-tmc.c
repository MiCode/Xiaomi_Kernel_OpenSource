/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/memory_alloc.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/usb/usb_qdss.h>
#include <mach/memory.h>
#include <mach/sps.h>
#include <mach/usb_bam.h>

#include "coresight-priv.h"

#define tmc_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tmc_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define TMC_LOCK(drvdata)						\
do {									\
	mb();								\
	tmc_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TMC_UNLOCK(drvdata)						\
do {									\
	tmc_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define TMC_RSZ			(0x004)
#define TMC_STS			(0x00C)
#define TMC_RRD			(0x010)
#define TMC_RRP			(0x014)
#define TMC_RWP			(0x018)
#define TMC_TRG			(0x01C)
#define TMC_CTL			(0x020)
#define TMC_RWD			(0x024)
#define TMC_MODE		(0x028)
#define TMC_LBUFLEVEL		(0x02C)
#define TMC_CBUFLEVEL		(0x030)
#define TMC_BUFWM		(0x034)
#define TMC_RRPHI		(0x038)
#define TMC_RWPHI		(0x03C)
#define TMC_AXICTL		(0x110)
#define TMC_DBALO		(0x118)
#define TMC_DBAHI		(0x11C)
#define TMC_FFSR		(0x300)
#define TMC_FFCR		(0x304)
#define TMC_PSCR		(0x308)
#define TMC_ITMISCOP0		(0xEE0)
#define TMC_ITTRFLIN		(0xEE8)
#define TMC_ITATBDATA0		(0xEEC)
#define TMC_ITATBCTR2		(0xEF0)
#define TMC_ITATBCTR1		(0xEF4)
#define TMC_ITATBCTR0		(0xEF8)

#define BYTES_PER_WORD		4
#define TMC_ETR_BAM_PIPE_INDEX	0
#define TMC_ETR_BAM_NR_PIPES	2

enum tmc_config_type {
	TMC_CONFIG_TYPE_ETB,
	TMC_CONFIG_TYPE_ETR,
	TMC_CONFIG_TYPE_ETF,
};

enum tmc_mode {
	TMC_MODE_CIRCULAR_BUFFER,
	TMC_MODE_SOFTWARE_FIFO,
	TMC_MODE_HARDWARE_FIFO,
};

enum tmc_etr_out_mode {
	TMC_ETR_OUT_MODE_NONE,
	TMC_ETR_OUT_MODE_MEM,
	TMC_ETR_OUT_MODE_USB,
};

enum tmc_mem_intf_width {
	TMC_MEM_INTF_WIDTH_32BITS	= 0x2,
	TMC_MEM_INTF_WIDTH_64BITS	= 0x3,
	TMC_MEM_INTF_WIDTH_128BITS	= 0x4,
	TMC_MEM_INTF_WIDTH_256BITS	= 0x5,
};

struct tmc_etr_bam_data {
	struct sps_bam_props	props;
	uint32_t		handle;
	struct sps_pipe		*pipe;
	struct sps_connect	connect;
	uint32_t		src_pipe_idx;
	uint32_t		dest;
	uint32_t		dest_pipe_idx;
	struct sps_mem_buffer	desc_fifo;
	struct sps_mem_buffer	data_fifo;
	bool			enable;
};

struct tmc_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	struct clk		*clk;
	spinlock_t		spinlock;
	struct mutex		read_lock;
	int			read_count;
	bool			reading;
	char			*buf;
	unsigned long		paddr;
	void __iomem		*vaddr;
	uint32_t		size;
	struct mutex		usb_lock;
	struct usb_qdss_ch	*usbch;
	struct tmc_etr_bam_data	*bamdata;
	enum tmc_etr_out_mode	out_mode;
	bool			enable_to_bam;
	bool			enable;
	enum tmc_config_type	config_type;
	uint32_t		trigger_cntr;
};

static void tmc_wait_for_ready(struct tmc_drvdata *drvdata)
{
	int count;

	/* Ensure formatter, unformatter and hardware fifo are empty */
	for (count = TIMEOUT_US; BVAL(tmc_readl(drvdata, TMC_STS), 2) != 1
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while waiting for TMC ready, TMC_STS: %#x\n",
	     tmc_readl(drvdata, TMC_STS));
}

static void tmc_flush_and_stop(struct tmc_drvdata *drvdata)
{
	int count;
	uint32_t ffcr;

	ffcr = tmc_readl(drvdata, TMC_FFCR);
	ffcr |= BIT(12);
	tmc_writel(drvdata, ffcr, TMC_FFCR);
	ffcr |= BIT(6);
	tmc_writel(drvdata, ffcr, TMC_FFCR);
	/* Ensure flush completes */
	for (count = TIMEOUT_US; BVAL(tmc_readl(drvdata, TMC_FFCR), 6) != 0
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while flushing TMC, TMC_FFCR: %#x\n",
	     tmc_readl(drvdata, TMC_FFCR));

	tmc_wait_for_ready(drvdata);
}

static void __tmc_enable(struct tmc_drvdata *drvdata)
{
	tmc_writel(drvdata, 0x1, TMC_CTL);
}

static void __tmc_disable(struct tmc_drvdata *drvdata)
{
	tmc_writel(drvdata, 0x0, TMC_CTL);
}

static void tmc_etr_fill_usb_bam_data(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	get_bam2bam_connection_info(0, PEER_PERIPHERAL_TO_USB,
				    &bamdata->dest,
				    &bamdata->dest_pipe_idx,
				    &bamdata->src_pipe_idx,
				    &bamdata->desc_fifo,
				    &bamdata->data_fifo);
}

static void __tmc_etr_enable_to_bam(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	uint32_t axictl;

	if (drvdata->enable_to_bam)
		return;

	/* Configure and enable required CSR registers */
	msm_qdss_csr_enable_bam_to_usb();

	/* Configure and enable ETR for usb bam output */

	TMC_UNLOCK(drvdata);

	tmc_writel(drvdata, bamdata->data_fifo.size / BYTES_PER_WORD,
		   TMC_RSZ);
	tmc_writel(drvdata, TMC_MODE_CIRCULAR_BUFFER, TMC_MODE);

	axictl = tmc_readl(drvdata, TMC_AXICTL);
	axictl |= (0xF << 8);
	tmc_writel(drvdata, axictl, TMC_AXICTL);
	axictl &= ~(0x1 << 7);
	tmc_writel(drvdata, axictl, TMC_AXICTL);
	axictl = (axictl & ~0x3) | 0x2;
	tmc_writel(drvdata, axictl, TMC_AXICTL);

	tmc_writel(drvdata, bamdata->data_fifo.phys_base, TMC_DBALO);
	tmc_writel(drvdata, 0x0, TMC_DBAHI);
	tmc_writel(drvdata, 0x133, TMC_FFCR);
	tmc_writel(drvdata, drvdata->trigger_cntr, TMC_TRG);
	__tmc_enable(drvdata);

	TMC_LOCK(drvdata);

	drvdata->enable_to_bam = true;
}

static int tmc_etr_bam_enable(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;
	int ret;

	if (bamdata->enable)
		return 0;

	/* Configure and enable ndp bam */

	bamdata->pipe = sps_alloc_endpoint();
	if (!bamdata->pipe)
		return -ENOMEM;

	ret = sps_get_config(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err;

	bamdata->connect.mode = SPS_MODE_SRC;
	bamdata->connect.source = bamdata->handle;
	bamdata->connect.event_thresh = 0x4;
	bamdata->connect.src_pipe_index = TMC_ETR_BAM_PIPE_INDEX;
	bamdata->connect.options = SPS_O_AUTO_ENABLE;

	bamdata->connect.destination = bamdata->dest;
	bamdata->connect.dest_pipe_index = bamdata->dest_pipe_idx;
	bamdata->connect.desc = bamdata->desc_fifo;
	bamdata->connect.data = bamdata->data_fifo;

	ret = sps_connect(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err;

	bamdata->enable = true;
	return 0;
err:
	sps_free_endpoint(bamdata->pipe);
	return ret;
}

static void __tmc_etr_disable_to_bam(struct tmc_drvdata *drvdata)
{
	if (!drvdata->enable_to_bam)
		return;

	TMC_UNLOCK(drvdata);

	tmc_flush_and_stop(drvdata);
	__tmc_disable(drvdata);

	TMC_LOCK(drvdata);

	/* Disable CSR registers */
	msm_qdss_csr_disable_bam_to_usb();
	drvdata->enable_to_bam = false;
}

static void tmc_etr_bam_disable(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	if (!bamdata->enable)
		return;

	sps_disconnect(bamdata->pipe);
	sps_free_endpoint(bamdata->pipe);
	bamdata->enable = false;
}

static void usb_notifier(void *priv, unsigned int event,
			struct qdss_request *d_req, struct usb_qdss_ch *ch)
{
	struct tmc_drvdata *drvdata = priv;
	unsigned long flags;
	int ret = 0;

	mutex_lock(&drvdata->usb_lock);
	if (event == USB_QDSS_CONNECT) {
		tmc_etr_fill_usb_bam_data(drvdata);
		ret = tmc_etr_bam_enable(drvdata);
		if (ret)
			dev_err(drvdata->dev, "ETR BAM enable failed\n");

		spin_lock_irqsave(&drvdata->spinlock, flags);
		__tmc_etr_enable_to_bam(drvdata);
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	} else if (event == USB_QDSS_DISCONNECT) {
		spin_lock_irqsave(&drvdata->spinlock, flags);
		__tmc_etr_disable_to_bam(drvdata);
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
		tmc_etr_bam_disable(drvdata);
	}
	mutex_unlock(&drvdata->usb_lock);
}

static void __tmc_etb_enable(struct tmc_drvdata *drvdata)
{
	/* Zero out the memory to help with debug */
	memset(drvdata->buf, 0, drvdata->size);

	TMC_UNLOCK(drvdata);

	tmc_writel(drvdata, TMC_MODE_CIRCULAR_BUFFER, TMC_MODE);
	tmc_writel(drvdata, 0x133, TMC_FFCR);
	tmc_writel(drvdata, drvdata->trigger_cntr, TMC_TRG);
	__tmc_enable(drvdata);

	TMC_LOCK(drvdata);
}

static void __tmc_etr_enable_to_mem(struct tmc_drvdata *drvdata)
{
	uint32_t axictl;

	/* Zero out the memory to help with debug */
	memset(drvdata->vaddr, 0, drvdata->size);

	TMC_UNLOCK(drvdata);

	tmc_writel(drvdata, drvdata->size / BYTES_PER_WORD, TMC_RSZ);
	tmc_writel(drvdata, TMC_MODE_CIRCULAR_BUFFER, TMC_MODE);

	axictl = tmc_readl(drvdata, TMC_AXICTL);
	axictl |= (0xF << 8);
	tmc_writel(drvdata, axictl, TMC_AXICTL);
	axictl &= ~(0x1 << 7);
	tmc_writel(drvdata, axictl, TMC_AXICTL);
	axictl = (axictl & ~0x3) | 0x2;
	tmc_writel(drvdata, axictl, TMC_AXICTL);

	tmc_writel(drvdata, drvdata->paddr, TMC_DBALO);
	tmc_writel(drvdata, 0x0, TMC_DBAHI);
	tmc_writel(drvdata, 0x133, TMC_FFCR);
	tmc_writel(drvdata, drvdata->trigger_cntr, TMC_TRG);
	__tmc_enable(drvdata);

	TMC_LOCK(drvdata);
}

static void __tmc_etf_enable(struct tmc_drvdata *drvdata)
{
	TMC_UNLOCK(drvdata);

	tmc_writel(drvdata, TMC_MODE_HARDWARE_FIFO, TMC_MODE);
	tmc_writel(drvdata, 0x3, TMC_FFCR);
	tmc_writel(drvdata, 0x0, TMC_BUFWM);
	__tmc_enable(drvdata);

	TMC_LOCK(drvdata);
}

static int tmc_enable(struct tmc_drvdata *drvdata, enum tmc_mode mode)
{
	int ret;
	unsigned long flags;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	mutex_lock(&drvdata->usb_lock);
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
			drvdata->usbch = usb_qdss_open("qdss", drvdata,
						       usb_notifier);
			if (IS_ERR(drvdata->usbch)) {
				dev_err(drvdata->dev, "usb_qdss_open failed\n");
				ret = PTR_ERR(drvdata->usbch);
				goto err0;
			}
		}
	}

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto err1;
	}

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		__tmc_etb_enable(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			__tmc_etr_enable_to_mem(drvdata);
	} else {
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			__tmc_etb_enable(drvdata);
		else
			__tmc_etf_enable(drvdata);
	}
	drvdata->enable = true;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	mutex_unlock(&drvdata->usb_lock);

	dev_info(drvdata->dev, "TMC enabled\n");
	return 0;
err1:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR)
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			usb_qdss_close(drvdata->usbch);
err0:
	mutex_unlock(&drvdata->usb_lock);
	clk_disable_unprepare(drvdata->clk);
	return ret;
}

static int tmc_enable_sink(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return tmc_enable(drvdata, TMC_MODE_CIRCULAR_BUFFER);
}

static int tmc_enable_link(struct coresight_device *csdev, int inport,
			   int outport)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return tmc_enable(drvdata, TMC_MODE_HARDWARE_FIFO);
}

static void __tmc_etb_dump(struct tmc_drvdata *drvdata)
{
	enum tmc_mem_intf_width memwidth;
	uint8_t memwords;
	char *bufp;
	uint32_t read_data;
	int i;

	memwidth = BMVAL(tmc_readl(drvdata, CORESIGHT_DEVID), 8, 10);
	if (memwidth == TMC_MEM_INTF_WIDTH_32BITS)
		memwords = 1;
	else if (memwidth == TMC_MEM_INTF_WIDTH_64BITS)
		memwords = 2;
	else if (memwidth == TMC_MEM_INTF_WIDTH_128BITS)
		memwords = 4;
	else
		memwords = 8;

	bufp = drvdata->buf;
	while (1) {
		for (i = 0; i < memwords; i++) {
			read_data = tmc_readl(drvdata, TMC_RRD);
			if (read_data == 0xFFFFFFFF)
				return;
			memcpy(bufp, &read_data, BYTES_PER_WORD);
			bufp += BYTES_PER_WORD;
		}
	}
}

static void __tmc_etb_disable(struct tmc_drvdata *drvdata)
{
	TMC_UNLOCK(drvdata);

	tmc_flush_and_stop(drvdata);
	__tmc_etb_dump(drvdata);
	__tmc_disable(drvdata);

	TMC_LOCK(drvdata);
}

static void __tmc_etr_dump(struct tmc_drvdata *drvdata)
{
	uint32_t rwp, rwphi;

	rwp = tmc_readl(drvdata, TMC_RWP);
	rwphi = tmc_readl(drvdata, TMC_RWPHI);

	if (BVAL(tmc_readl(drvdata, TMC_STS), 0))
		drvdata->buf = drvdata->vaddr + rwp - drvdata->paddr;
	else
		drvdata->buf = drvdata->vaddr;
}

static void __tmc_etr_disable_to_mem(struct tmc_drvdata *drvdata)
{
	TMC_UNLOCK(drvdata);

	tmc_flush_and_stop(drvdata);
	__tmc_etr_dump(drvdata);
	__tmc_disable(drvdata);

	TMC_LOCK(drvdata);
}

static void __tmc_etf_disable(struct tmc_drvdata *drvdata)
{
	TMC_UNLOCK(drvdata);

	tmc_flush_and_stop(drvdata);
	__tmc_disable(drvdata);

	TMC_LOCK(drvdata);
}

static void tmc_disable(struct tmc_drvdata *drvdata, enum tmc_mode mode)
{
	unsigned long flags;
	bool etr_bam_disable = false;

	mutex_lock(&drvdata->usb_lock);
	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		__tmc_etb_disable(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			__tmc_etr_disable_to_mem(drvdata);
		else if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			etr_bam_disable = true;
	} else {
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			__tmc_etb_disable(drvdata);
		else
			__tmc_etf_disable(drvdata);
	}
out:
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (etr_bam_disable) {
		if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
			if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
				spin_lock_irqsave(&drvdata->spinlock, flags);
				__tmc_etr_disable_to_bam(drvdata);
				spin_unlock_irqrestore(&drvdata->spinlock,
						       flags);
				tmc_etr_bam_disable(drvdata);
				usb_qdss_close(drvdata->usbch);
			}
		}
	}
	mutex_unlock(&drvdata->usb_lock);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "TMC disabled\n");
}

static void tmc_disable_sink(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	tmc_disable(drvdata, TMC_MODE_CIRCULAR_BUFFER);
}

static void tmc_disable_link(struct coresight_device *csdev, int inport,
			     int outport)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	tmc_disable(drvdata, TMC_MODE_HARDWARE_FIFO);
}

static void tmc_abort(struct coresight_device *csdev)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;
	enum tmc_mode mode;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (drvdata->reading)
		goto out0;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		__tmc_etb_disable(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			__tmc_etr_disable_to_mem(drvdata);
	} else {
		mode = tmc_readl(drvdata, TMC_MODE);
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			__tmc_etb_disable(drvdata);
		else
			goto out1;
	}
out0:
	drvdata->enable = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC aborted\n");
	return;
out1:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
}

static const struct coresight_ops_sink tmc_sink_ops = {
	.enable		= tmc_enable_sink,
	.disable	= tmc_disable_sink,
	.abort		= tmc_abort,
};

static const struct coresight_ops_link tmc_link_ops = {
	.enable		= tmc_enable_link,
	.disable	= tmc_disable_link,
};

static const struct coresight_ops tmc_etb_cs_ops = {
	.sink_ops	= &tmc_sink_ops,
};

static const struct coresight_ops tmc_etr_cs_ops = {
	.sink_ops	= &tmc_sink_ops,
};

static const struct coresight_ops tmc_etf_cs_ops = {
	.sink_ops	= &tmc_sink_ops,
	.link_ops	= &tmc_link_ops,
};

static int tmc_read_prepare(struct tmc_drvdata *drvdata)
{
	int ret;
	unsigned long flags;
	enum tmc_mode mode;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->enable)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		__tmc_etb_disable(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
			__tmc_etr_disable_to_mem(drvdata);
		} else {
			ret = -ENODEV;
			goto err;
		}
	} else {
		mode = tmc_readl(drvdata, TMC_MODE);
		if (mode == TMC_MODE_CIRCULAR_BUFFER) {
			__tmc_etb_disable(drvdata);
		} else {
			ret = -ENODEV;
			goto err;
		}
	}
out:
	drvdata->reading = true;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC read start\n");
	return 0;
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	return ret;
}

static void tmc_read_unprepare(struct tmc_drvdata *drvdata)
{
	unsigned long flags;
	enum tmc_mode mode;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (!drvdata->enable)
		goto out;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		__tmc_etb_enable(drvdata);
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			__tmc_etr_enable_to_mem(drvdata);
	} else {
		mode = tmc_readl(drvdata, TMC_MODE);
		if (mode == TMC_MODE_CIRCULAR_BUFFER)
			__tmc_etb_enable(drvdata);
	}
out:
	drvdata->reading = false;
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	dev_info(drvdata->dev, "TMC read end\n");
}

static int tmc_open(struct inode *inode, struct file *file)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);
	int ret = 0;

	mutex_lock(&drvdata->read_lock);
	if (drvdata->read_count++)
		goto out;

	ret = tmc_read_prepare(drvdata);
	if (ret)
		goto err;
out:
	mutex_unlock(&drvdata->read_lock);
	nonseekable_open(inode, file);

	dev_dbg(drvdata->dev, "%s: successfully opened\n", __func__);
	return 0;
err:
	drvdata->read_count--;
	mutex_unlock(&drvdata->read_lock);
	return ret;
}

static ssize_t tmc_read(struct file *file, char __user *data, size_t len,
			loff_t *ppos)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);
	char *bufp = drvdata->buf + *ppos;

	if (*ppos + len > drvdata->size)
		len = drvdata->size - *ppos;

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		if (bufp == (char *)(drvdata->vaddr + drvdata->size))
			bufp = drvdata->vaddr;
		else if (bufp > (char *)(drvdata->vaddr + drvdata->size))
			bufp -= drvdata->size;
		if ((bufp + len) > (char *)(drvdata->vaddr + drvdata->size))
			len = (char *)(drvdata->vaddr + drvdata->size) - bufp;
	}

	if (copy_to_user(data, bufp, len)) {
		dev_dbg(drvdata->dev, "%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(drvdata->dev, "%s: %d bytes copied, %d bytes left\n",
		__func__, len, (int) (drvdata->size - *ppos));
	return len;
}

static int tmc_release(struct inode *inode, struct file *file)
{
	struct tmc_drvdata *drvdata = container_of(file->private_data,
						   struct tmc_drvdata, miscdev);

	mutex_lock(&drvdata->read_lock);
	if (--drvdata->read_count) {
		if (drvdata->read_count < 0) {
			WARN_ONCE(1, "mismatched close\n");
			drvdata->read_count = 0;
		}
		goto out;
	}

	tmc_read_unprepare(drvdata);
out:
	mutex_unlock(&drvdata->read_lock);
	dev_dbg(drvdata->dev, "%s: released\n", __func__);
	return 0;
}

static const struct file_operations tmc_fops = {
	.owner		= THIS_MODULE,
	.open		= tmc_open,
	.read		= tmc_read,
	.release	= tmc_release,
	.llseek		= no_llseek,
};

static ssize_t tmc_show_trigger_cntr(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->trigger_cntr;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tmc_store_trigger_cntr(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->trigger_cntr = val;
	return size;
}
static DEVICE_ATTR(trigger_cntr, S_IRUGO | S_IWUSR, tmc_show_trigger_cntr,
		   tmc_store_trigger_cntr);

static ssize_t tmc_etr_show_out_mode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->out_mode == TMC_ETR_OUT_MODE_MEM ?
			 "mem" : "usb");
}

static ssize_t tmc_etr_store_out_mode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct tmc_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	unsigned long flags;
	bool etr_bam_flag = false;
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->usb_lock);
	if (!strcmp(str, "mem")) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_MEM)
			goto out;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		if (drvdata->enable) {
			__tmc_etr_disable_to_bam(drvdata);
			__tmc_etr_enable_to_mem(drvdata);
			etr_bam_flag = true;
		}
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		if (etr_bam_flag) {
			tmc_etr_bam_disable(drvdata);
			usb_qdss_close(drvdata->usbch);
		}
	} else if (!strcmp(str, "usb")) {
		if (drvdata->out_mode == TMC_ETR_OUT_MODE_USB)
			goto out;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		if (drvdata->enable) {
			if (drvdata->reading) {
				ret = -EBUSY;
				goto err1;
			}
			__tmc_etr_disable_to_mem(drvdata);
			etr_bam_flag = true;
		}
		drvdata->out_mode = TMC_ETR_OUT_MODE_USB;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		if (etr_bam_flag) {
			drvdata->usbch = usb_qdss_open("qdss", drvdata,
						       usb_notifier);
			if (IS_ERR(drvdata->usbch)) {
				dev_err(drvdata->dev, "usb_qdss_open failed\n");
				ret = PTR_ERR(drvdata->usbch);
				goto err0;
			}
		}
	}
out:
	mutex_unlock(&drvdata->usb_lock);
	return size;
err1:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
err0:
	mutex_unlock(&drvdata->usb_lock);
	return ret;
}
static DEVICE_ATTR(out_mode, S_IRUGO | S_IWUSR, tmc_etr_show_out_mode,
		   tmc_etr_store_out_mode);

static struct attribute *tmc_attrs[] = {
	&dev_attr_trigger_cntr.attr,
	NULL,
};

static struct attribute_group tmc_attr_grp = {
	.attrs = tmc_attrs,
};

static struct attribute *tmc_etr_attrs[] = {
	&dev_attr_out_mode.attr,
	NULL,
};

static struct attribute_group tmc_etr_attr_grp = {
	.attrs = tmc_etr_attrs,
};

static const struct attribute_group *tmc_etb_attr_grps[] = {
	&tmc_attr_grp,
	NULL,
};

static const struct attribute_group *tmc_etr_attr_grps[] = {
	&tmc_attr_grp,
	&tmc_etr_attr_grp,
	NULL,
};

static const struct attribute_group *tmc_etf_attr_grps[] = {
	&tmc_attr_grp,
	NULL,
};

static int __devinit tmc_etr_bam_init(struct platform_device *pdev,
				      struct tmc_drvdata *drvdata)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct tmc_etr_bam_data *bamdata;

	bamdata = devm_kzalloc(dev, sizeof(*bamdata), GFP_KERNEL);
	if (!bamdata)
		return -ENOMEM;
	drvdata->bamdata = bamdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	bamdata->props.phys_addr = res->start;
	bamdata->props.virt_addr = devm_ioremap(dev, res->start,
						resource_size(res));
	if (!bamdata->props.virt_addr)
		return -ENOMEM;
	bamdata->props.virt_size = resource_size(res);

	bamdata->props.event_threshold = 0x4; /* Pipe event threshold */
	bamdata->props.summing_threshold = 0x10; /* BAM event threshold */
	bamdata->props.irq = 0;
	bamdata->props.num_pipes = TMC_ETR_BAM_NR_PIPES;

	return sps_register_bam_device(&bamdata->props, &bamdata->handle);
}

static void tmc_etr_bam_exit(struct tmc_drvdata *drvdata)
{
	struct tmc_etr_bam_data *bamdata = drvdata->bamdata;

	if (!bamdata->handle)
		return;
	sps_deregister_bam_device(bamdata->handle);
}

static int __devinit tmc_probe(struct platform_device *pdev)
{
	int ret;
	uint32_t devid;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct tmc_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	spin_lock_init(&drvdata->spinlock);
	mutex_init(&drvdata->read_lock);
	mutex_init(&drvdata->usb_lock);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	devid = tmc_readl(drvdata, CORESIGHT_DEVID);
	drvdata->config_type = BMVAL(devid, 6, 7);

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR)
		drvdata->size = SZ_1M;
	else
		drvdata->size = tmc_readl(drvdata, TMC_RSZ) * BYTES_PER_WORD;

	clk_disable_unprepare(drvdata->clk);

	if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		drvdata->paddr = allocate_contiguous_ebi_nomap(drvdata->size,
							       SZ_4K);
		if (!drvdata->paddr)
			return -ENOMEM;
		drvdata->vaddr = devm_ioremap(dev, drvdata->paddr,
					      drvdata->size);
		if (!drvdata->vaddr) {
			ret = -ENOMEM;
			goto err0;
		}
		memset(drvdata->vaddr, 0, drvdata->size);
		drvdata->out_mode = TMC_ETR_OUT_MODE_MEM;

		ret = tmc_etr_bam_init(pdev, drvdata);
		if (ret)
			goto err0;
	} else {
		drvdata->buf = devm_kzalloc(dev, drvdata->size, GFP_KERNEL);
		if (!drvdata->buf)
			return -ENOMEM;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto err1;
	}
	if (drvdata->config_type == TMC_CONFIG_TYPE_ETB) {
		desc->type = CORESIGHT_DEV_TYPE_SINK;
		desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc->ops = &tmc_etb_cs_ops;
		desc->pdata = pdev->dev.platform_data;
		desc->dev = &pdev->dev;
		desc->groups = tmc_etb_attr_grps;
		desc->owner = THIS_MODULE;
		drvdata->csdev = coresight_register(desc);
		if (IS_ERR(drvdata->csdev)) {
			ret = PTR_ERR(drvdata->csdev);
			goto err1;
		}
	} else if (drvdata->config_type == TMC_CONFIG_TYPE_ETR) {
		desc->type = CORESIGHT_DEV_TYPE_SINK;
		desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc->ops = &tmc_etr_cs_ops;
		desc->pdata = pdev->dev.platform_data;
		desc->dev = &pdev->dev;
		desc->groups = tmc_etr_attr_grps;
		desc->owner = THIS_MODULE;
		drvdata->csdev = coresight_register(desc);
		if (IS_ERR(drvdata->csdev)) {
			ret = PTR_ERR(drvdata->csdev);
			goto err1;
		}
	} else {
		desc->type = CORESIGHT_DEV_TYPE_LINKSINK;
		desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_BUFFER;
		desc->subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_FIFO;
		desc->ops = &tmc_etf_cs_ops;
		desc->pdata = pdev->dev.platform_data;
		desc->dev = &pdev->dev;
		desc->groups = tmc_etf_attr_grps;
		desc->owner = THIS_MODULE;
		drvdata->csdev = coresight_register(desc);
		if (IS_ERR(drvdata->csdev)) {
			ret = PTR_ERR(drvdata->csdev);
			goto err1;
		}
	}

	drvdata->miscdev.name = ((struct coresight_platform_data *)
				 (pdev->dev.platform_data))->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &tmc_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		goto err2;

	dev_info(dev, "TMC initialized\n");
	return 0;
err2:
	coresight_unregister(drvdata->csdev);
err1:
	tmc_etr_bam_exit(drvdata);
err0:
	free_contiguous_memory_by_paddr(drvdata->paddr);
	return ret;
}

static int __devexit tmc_remove(struct platform_device *pdev)
{
	struct tmc_drvdata *drvdata = platform_get_drvdata(pdev);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
	tmc_etr_bam_exit(drvdata);
	free_contiguous_memory_by_paddr(drvdata->paddr);
	return 0;
}

static struct of_device_id tmc_match[] = {
	{.compatible = "arm,coresight-tmc"},
	{}
};
EXPORT_COMPAT("arm,coresight-tmc");

static struct platform_driver tmc_driver = {
	.probe          = tmc_probe,
	.remove         = __devexit_p(tmc_remove),
	.driver         = {
		.name   = "coresight-tmc",
		.owner	= THIS_MODULE,
		.of_match_table = tmc_match,
	},
};

static int __init tmc_init(void)
{
	return platform_driver_register(&tmc_driver);
}
module_init(tmc_init);

static void __exit tmc_exit(void)
{
	platform_driver_unregister(&tmc_driver);
}
module_exit(tmc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Trace Memory Controller driver");
