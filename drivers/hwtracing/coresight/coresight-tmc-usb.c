// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight TMC USB driver
 */

#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/qcom-iommu-util.h>
#include "coresight-tmc-usb.h"
#include "coresight-priv.h"
#include "coresight-common.h"
#include "coresight-tmc.h"

#define TMC_AXICTL_VALUE	(0xf02)
#define TMC_FFCR_VALUE		(0x133)

static int tmc_etr_fill_usb_bam_data(struct tmc_usb_data *usb_data)
{
	struct tmc_usb_bam_data *bamdata = usb_data->bamdata;
	struct tmc_drvdata *tmcdrvdata = usb_data->tmcdrvdata;
	dma_addr_t data_fifo_iova, desc_fifo_iova;

	get_qdss_bam_connection_info(&bamdata->dest,
				    &bamdata->dest_pipe_idx,
				    &bamdata->src_pipe_idx,
				    &bamdata->desc_fifo,
				    &bamdata->data_fifo,
				    NULL);

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		data_fifo_iova = dma_map_resource(tmcdrvdata->csdev->dev.parent,
			bamdata->data_fifo.phys_base, bamdata->data_fifo.size,
			DMA_BIDIRECTIONAL, 0);
		if (!data_fifo_iova)
			return -ENOMEM;
		dev_dbg(&tmcdrvdata->csdev->dev,
			"%s:data p_addr:%pa,iova:%pad,size:%x\n",
			__func__, &(bamdata->data_fifo.phys_base),
			&data_fifo_iova, bamdata->data_fifo.size);
		bamdata->data_fifo.iova = data_fifo_iova;
		desc_fifo_iova = dma_map_resource(tmcdrvdata->csdev->dev.parent,
			bamdata->desc_fifo.phys_base, bamdata->desc_fifo.size,
			DMA_BIDIRECTIONAL, 0);
		if (!desc_fifo_iova)
			return -ENOMEM;
		dev_dbg(&tmcdrvdata->csdev->dev,
			"%s:desc p_addr:%pa,iova:%pad,size:%x\n",
			__func__, &(bamdata->desc_fifo.phys_base),
			&desc_fifo_iova, bamdata->desc_fifo.size);
		bamdata->desc_fifo.iova = desc_fifo_iova;
	}
	return 0;
}

static void __tmc_etr_enable_to_bam(struct tmc_usb_data *usb_data)
{
	struct tmc_usb_bam_data *bamdata = usb_data->bamdata;
	struct tmc_drvdata *tmcdrvdata = usb_data->tmcdrvdata;

	if (usb_data->enable_to_bam)
		return;

	/* Configure and enable required CSR registers */
	msm_qdss_csr_enable_bam_to_usb(tmcdrvdata->csr);

	/* Configure and enable ETR for usb bam output */

	CS_UNLOCK(tmcdrvdata->base);

	writel_relaxed(bamdata->data_fifo.size / 4, tmcdrvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, tmcdrvdata->base + TMC_MODE);

	writel_relaxed(TMC_AXICTL_VALUE, tmcdrvdata->base + TMC_AXICTL);

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		writel_relaxed((uint32_t)bamdata->data_fifo.iova,
		       tmcdrvdata->base + TMC_DBALO);
		writel_relaxed((((uint64_t)bamdata->data_fifo.iova) >> 32)
			& 0xFF, tmcdrvdata->base + TMC_DBAHI);
	} else {
		writel_relaxed((uint32_t)bamdata->data_fifo.phys_base,
		       tmcdrvdata->base + TMC_DBALO);
		writel_relaxed((((uint64_t)bamdata->data_fifo.phys_base) >> 32)
			& 0xFF, tmcdrvdata->base + TMC_DBAHI);
	}
	/* Set FOnFlIn for periodic flush */
	writel_relaxed(TMC_FFCR_VALUE, tmcdrvdata->base + TMC_FFCR);
	writel_relaxed(tmcdrvdata->trigger_cntr, tmcdrvdata->base + TMC_TRG);
	tmc_enable_hw(tmcdrvdata);

	CS_LOCK(tmcdrvdata->base);

	msm_qdss_csr_enable_flush(tmcdrvdata->csr);
	usb_data->enable_to_bam = true;
}

static int get_usb_bam_iova(struct device *dev, unsigned long usb_bam_handle,
				unsigned long *iova)
{
	int ret = 0;
	phys_addr_t p_addr;
	u32 bam_size;

	ret = sps_get_bam_addr(usb_bam_handle, &p_addr, &bam_size);
	if (ret) {
		dev_err(dev, "sps_get_bam_addr failed at handle:%lx, err:%d\n",
			usb_bam_handle, ret);
		return ret;
	}
	*iova = dma_map_resource(dev, p_addr, bam_size, DMA_BIDIRECTIONAL, 0);
	if (!(*iova))
		return -ENOMEM;
	return 0;
}

static int tmc_etr_bam_enable(struct tmc_usb_data *usb_data)
{
	struct tmc_usb_bam_data *bamdata;
	struct tmc_drvdata *tmcdrvdata;
	unsigned long iova;
	int ret;

	if (usb_data == NULL)
		return -EINVAL;

	bamdata = usb_data->bamdata;
	tmcdrvdata = usb_data->tmcdrvdata;

	if (bamdata->enable)
		return 0;

	/* Reset bam to start with */
	ret = sps_device_reset(bamdata->handle);
	if (ret)
		goto err0;

	/* Now configure and enable bam */

	bamdata->pipe = sps_alloc_endpoint();
	if (!bamdata->pipe)
		return -ENOMEM;

	ret = sps_get_config(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err1;

	bamdata->connect.mode = SPS_MODE_SRC;
	bamdata->connect.source = bamdata->handle;
	bamdata->connect.event_thresh = 0x4;
	bamdata->connect.src_pipe_index = TMC_USB_BAM_PIPE_INDEX;
	bamdata->connect.options = SPS_O_AUTO_ENABLE;

	bamdata->connect.destination = bamdata->dest;
	bamdata->connect.dest_pipe_index = bamdata->dest_pipe_idx;
	bamdata->connect.desc = bamdata->desc_fifo;
	bamdata->connect.data = bamdata->data_fifo;

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		ret = get_usb_bam_iova(tmcdrvdata->csdev->dev.parent,
				bamdata->dest, &iova);
		if (ret)
			goto err1;
		bamdata->connect.dest_iova = iova;
	}
	ret = sps_connect(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err1;

	bamdata->enable = true;
	return 0;
err1:
	sps_free_endpoint(bamdata->pipe);
err0:
	return ret;
}

static void tmc_wait_for_flush(struct tmc_drvdata *drvdata)
{
	int count;

	/* Ensure no flush is in progress */
	for (count = TIMEOUT_US;
	     BVAL(readl_relaxed(drvdata->base + TMC_FFSR), 0) != 0
	     && count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while waiting for TMC flush, TMC_FFSR: %#x\n",
	     readl_relaxed(drvdata->base + TMC_FFSR));
}

void __tmc_etr_disable_to_bam(struct tmc_usb_data *usb_data)
{
	struct tmc_drvdata *tmcdrvdata = usb_data->tmcdrvdata;

	if (!usb_data->enable_to_bam)
		return;

	if (tmcdrvdata->csr == NULL)
		return;

	/* Ensure periodic flush is disabled in CSR block */
	msm_qdss_csr_disable_flush(tmcdrvdata->csr);

	CS_UNLOCK(tmcdrvdata->base);

	tmc_wait_for_flush(tmcdrvdata);
	tmc_disable_hw(tmcdrvdata);

	CS_LOCK(tmcdrvdata->base);

	/* Disable CSR configuration */
	msm_qdss_csr_disable_bam_to_usb(tmcdrvdata->csr);
	usb_data->enable_to_bam = false;
}

void tmc_etr_bam_disable(struct tmc_usb_data *usb_data)
{
	struct tmc_usb_bam_data *bamdata;

	if (usb_data == NULL)
		return;

	bamdata = usb_data->bamdata;
	if (!bamdata->enable)
		return;

	sps_disconnect(bamdata->pipe);
	sps_free_endpoint(bamdata->pipe);
	bamdata->enable = false;
}

void usb_notifier(void *priv, unsigned int event, struct qdss_request *d_req,
		  struct usb_qdss_ch *ch)
{
	struct tmc_drvdata *drvdata = priv;
	unsigned long flags;
	int ret = 0;

	if (drvdata->out_mode != TMC_ETR_OUT_MODE_USB
			|| drvdata->mode == CS_MODE_DISABLED) {
		dev_err(&drvdata->csdev->dev,
		"%s: ETR is not USB mode, or ETR is disabled.\n", __func__);
		return;
	}

	if (event == USB_QDSS_CONNECT) {
		ret = tmc_etr_fill_usb_bam_data(drvdata->usb_data);
		if (ret)
			dev_err(&drvdata->csdev->dev,
			"ETR get usb bam data failed\n");
		ret = tmc_etr_bam_enable(drvdata->usb_data);
		if (ret)
			dev_err(&drvdata->csdev->dev,
			"ETR BAM enable failed\n");

		spin_lock_irqsave(&drvdata->spinlock, flags);
		__tmc_etr_enable_to_bam(drvdata->usb_data);
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	} else if (event == USB_QDSS_DISCONNECT) {
		spin_lock_irqsave(&drvdata->spinlock, flags);
		__tmc_etr_disable_to_bam(drvdata->usb_data);
		spin_unlock_irqrestore(&drvdata->spinlock, flags);

		tmc_etr_bam_disable(drvdata->usb_data);
	}
}

int tmc_usb_enable(struct tmc_usb_data *usb_data)
{
	struct tmc_drvdata *tmcdrvdata;

	if (usb_data == NULL)
		return -EINVAL;

	tmcdrvdata = usb_data->tmcdrvdata;
	usb_data->usbch = usb_qdss_open("qdss", tmcdrvdata, usb_notifier);
	if (IS_ERR_OR_NULL(usb_data->usbch)) {
		dev_err(&tmcdrvdata->csdev->dev, "usb_qdss_open failed for qdss.\n");
		return -ENODEV;
	}
	return 0;
}

void tmc_usb_disable(struct tmc_usb_data *usb_data)
{
	unsigned long flags;
	struct tmc_drvdata *tmcdrvdata = usb_data->tmcdrvdata;

	spin_lock_irqsave(&tmcdrvdata->spinlock, flags);
	__tmc_etr_disable_to_bam(usb_data);
	spin_unlock_irqrestore(&tmcdrvdata->spinlock, flags);
	tmc_etr_bam_disable(usb_data);
	usb_qdss_close(usb_data->usbch);
}

int tmc_etr_bam_init(struct amba_device *adev,
		     struct tmc_drvdata *drvdata)
{
	int ret;
	struct device *dev = &adev->dev;
	struct resource res;
	struct tmc_usb_bam_data *bamdata;
	int s1_bypass = 0;
	struct iommu_domain *domain;
	struct tmc_usb_data *usb_data;

	usb_data = devm_kzalloc(dev, sizeof(*usb_data), GFP_KERNEL);
	if (!usb_data)
		return -ENOMEM;
	drvdata->usb_data = usb_data;
	drvdata->usb_data->tmcdrvdata = drvdata;

	bamdata = devm_kzalloc(dev, sizeof(*bamdata), GFP_KERNEL);
	if (!bamdata)
		return -ENOMEM;
	drvdata->usb_data->bamdata = bamdata;

	ret = of_address_to_resource(adev->dev.of_node, 1, &res);
	if (ret)
		return -ENODEV;

	bamdata->props.phys_addr = res.start;
	bamdata->props.virt_addr = devm_ioremap(dev, res.start,
						resource_size(&res));
	if (!bamdata->props.virt_addr)
		return -ENOMEM;
	bamdata->props.virt_size = resource_size(&res);

	bamdata->props.event_threshold = 0x4; /* Pipe event threshold */
	bamdata->props.summing_threshold = 0x10; /* BAM event threshold */
	bamdata->props.irq = 0;
	bamdata->props.num_pipes = TMC_USB_BAM_NR_PIPES;
	domain = iommu_get_domain_for_dev(dev);
	if (domain) {
		iommu_domain_get_attr(domain, DOMAIN_ATTR_S1_BYPASS,
			&s1_bypass);
		if (!s1_bypass) {
			pr_debug("%s: setting SPS_BAM_SMMU_EN flag with (%s)\n",
			__func__, dev_name(dev));
			bamdata->props.options |= SPS_BAM_SMMU_EN;
		}
	}

	return sps_register_bam_device(&bamdata->props, &bamdata->handle);
}
