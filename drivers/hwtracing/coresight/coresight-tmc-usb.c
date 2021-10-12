// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight TMC USB driver
 */

#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/qcom-iommu-util.h>
#include <linux/usb/usb_qdss.h>
#include <linux/time.h>
#include <linux/slab.h>
#include "coresight-tmc-usb.h"
#include "coresight-priv.h"
#include "coresight-common.h"
#include "coresight-tmc.h"

#define USB_BLK_SIZE 65536
#define USB_SG_NUM (USB_BLK_SIZE / PAGE_SIZE)
#define USB_BUF_NUM 255
#define USB_TIME_OUT (5 * HZ)

#define TMC_AXICTL_VALUE	(0xf02)
#define TMC_FFCR_VALUE		(0x133)

static int usb_bypass_start(struct byte_cntr *byte_cntr_data)
{
	long offset;
	struct tmc_drvdata *tmcdrvdata;

	if (!byte_cntr_data)
		return -ENOMEM;

	tmcdrvdata = byte_cntr_data->tmcdrvdata;
	mutex_lock(&byte_cntr_data->usb_bypass_lock);


	dev_info(&tmcdrvdata->csdev->dev,
			"%s: Start usb bypass\n", __func__);
	if (tmcdrvdata->mode != CS_MODE_SYSFS) {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return -EINVAL;
	}

	offset = tmc_get_rwp_offset(tmcdrvdata);
	if (offset < 0) {
		dev_err(&tmcdrvdata->csdev->dev,
			"%s: invalid rwp offset value\n", __func__);
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return offset;
	}
	byte_cntr_data->offset = offset;

	/*Ensure usbch is ready*/
	if (!tmcdrvdata->usb_data->usbch) {
		int i;

		for (i = TIMEOUT_US; i > 0; i--) {
			if (tmcdrvdata->usb_data->usbch)
				break;

			if (i - 1)
				udelay(1);
			else {
				dev_err(&tmcdrvdata->csdev->dev,
					"timeout while waiting usbch to be ready\n");
				mutex_unlock(&byte_cntr_data->usb_bypass_lock);
				return -EAGAIN;
			}
		}
	}
	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);

	byte_cntr_data->read_active = true;
	/*
	 * IRQ is a '8- byte' counter and to observe interrupt at
	 * 'block_size' bytes of data
	 */
	coresight_csr_set_byte_cntr(byte_cntr_data->csr,
			byte_cntr_data->irqctrl_offset,
			USB_BLK_SIZE / 8);

	atomic_set(&byte_cntr_data->irq_cnt, 0);
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

	return 0;
}

static void usb_bypass_stop(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->usb_bypass_lock);
	if (byte_cntr_data->read_active)
		byte_cntr_data->read_active = false;
	else {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return;
	}
	wake_up(&byte_cntr_data->usb_wait_wq);
	pr_info("coresight: stop usb bypass\n");
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, byte_cntr_data->irqctrl_offset, 0);
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

}

static int usb_transfer_small_packet(struct byte_cntr *drvdata, size_t *small_size)
{
	int ret = 0;
	struct tmc_drvdata *tmcdrvdata = drvdata->tmcdrvdata;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
	struct qdss_request *usb_req = NULL;
	size_t req_size, actual;
	long w_offset;

	w_offset = tmc_get_rwp_offset(tmcdrvdata);
	if (w_offset < 0) {
		ret = w_offset;
		dev_err_ratelimited(&tmcdrvdata->csdev->dev,
			"%s: RWP offset is invalid\n", __func__);
		goto out;
	}

	req_size = ((w_offset < drvdata->offset) ? etr_buf->size : 0) +
				w_offset - drvdata->offset;
	req_size = ((req_size + *small_size) < USB_BLK_SIZE) ? req_size :
		(USB_BLK_SIZE - *small_size);

	while (req_size > 0) {

		usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
		if (!usb_req) {
			ret = -EFAULT;
			goto out;
		}

		actual = tmc_etr_buf_get_data(etr_buf, drvdata->offset,
					req_size, &usb_req->buf);

		if (actual <= 0 || actual > req_size) {
			kfree(usb_req);
			usb_req = NULL;
			dev_err_ratelimited(&tmcdrvdata->csdev->dev,
				"%s: Invalid data in ETR\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		usb_req->length = actual;
		drvdata->usb_req = usb_req;
		req_size -= actual;

		if ((drvdata->offset + actual) >=
				tmcdrvdata->sysfs_buf->size)
			drvdata->offset = 0;
		else
			drvdata->offset += actual;

		*small_size += actual;

		if (atomic_read(&drvdata->usb_free_buf) > 0) {
			ret = usb_qdss_write(tmcdrvdata->usb_data->usbch, usb_req);

			if (ret) {
				kfree(usb_req);
				usb_req = NULL;
				drvdata->usb_req = NULL;
				dev_err_ratelimited(&tmcdrvdata->csdev->dev,
					"Write data failed:%d\n", ret);
				goto out;
			}

			atomic_dec(&drvdata->usb_free_buf);
		} else {
			dev_dbg(&tmcdrvdata->csdev->dev,
			"Drop data, offset = %d, len = %d\n",
				drvdata->offset, req_size);
			kfree(usb_req);
			drvdata->usb_req = NULL;
		}
	}

out:
	return ret;
}

static void usb_read_work_fn(struct work_struct *work)
{
	int ret, i, seq = 0;
	struct qdss_request *usb_req = NULL;
	size_t actual, req_size, req_sg_num, small_size = 0;
	ssize_t actual_total = 0;
	char *buf;
	struct byte_cntr *drvdata =
		container_of(work, struct byte_cntr, read_work);
	struct tmc_drvdata *tmcdrvdata = drvdata->tmcdrvdata;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;

	while (tmcdrvdata->mode == CS_MODE_SYSFS
		&& tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		if (!atomic_read(&drvdata->irq_cnt)) {
			ret =  wait_event_interruptible_timeout(
				drvdata->usb_wait_wq,
				atomic_read(&drvdata->irq_cnt) > 0
				|| tmcdrvdata->mode != CS_MODE_SYSFS || tmcdrvdata->out_mode
				!= TMC_ETR_OUT_MODE_USB
				|| !drvdata->read_active, USB_TIME_OUT);
			if (ret == -ERESTARTSYS || tmcdrvdata->mode != CS_MODE_SYSFS
			|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_USB
			|| !drvdata->read_active)
				break;

			if (ret == 0) {
				ret = usb_transfer_small_packet(drvdata, &small_size);
				if (ret && ret != -EAGAIN)
					return;
				continue;
			}
		}

		req_size = USB_BLK_SIZE - small_size;
		small_size = 0;
		actual_total = 0;

		if (req_size > 0) {
			seq++;
			req_sg_num = (req_size - 1) / PAGE_SIZE + 1;
			usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
			if (!usb_req)
				return;
			usb_req->sg = kcalloc(req_sg_num,
				sizeof(*(usb_req->sg)), GFP_KERNEL);
			if (!usb_req->sg) {
				kfree(usb_req);
				usb_req = NULL;
				return;
			}

			for (i = 0; i < req_sg_num; i++) {
				actual = tmc_etr_buf_get_data(etr_buf,
							drvdata->offset,
							PAGE_SIZE, &buf);

				if (actual <= 0 || actual > PAGE_SIZE) {
					kfree(usb_req->sg);
					kfree(usb_req);
					usb_req = NULL;
					dev_err_ratelimited(
						&tmcdrvdata->csdev->dev,
						"Invalid data in ETR\n");
					return;
				}

				sg_set_buf(&usb_req->sg[i], buf, actual);

				if (i == 0)
					usb_req->buf = buf;
				if (i == req_sg_num - 1)
					sg_mark_end(&usb_req->sg[i]);

				if ((drvdata->offset + actual) >=
					tmcdrvdata->sysfs_buf->size)
					drvdata->offset = 0;
				else
					drvdata->offset += actual;
				actual_total += actual;
			}

			usb_req->length = actual_total;
			drvdata->usb_req = usb_req;
			usb_req->num_sgs = i;

			if (atomic_read(&drvdata->usb_free_buf) > 0) {
				ret = usb_qdss_write(tmcdrvdata->usb_data->usbch,
						drvdata->usb_req);
				if (ret) {
					kfree(usb_req->sg);
					kfree(usb_req);
					usb_req = NULL;
					drvdata->usb_req = NULL;
					dev_err_ratelimited(
						&tmcdrvdata->csdev->dev,
						"Write data failed:%d\n", ret);
					if (ret == -EAGAIN)
						continue;
					return;
				}
				atomic_dec(&drvdata->usb_free_buf);

			} else {
				dev_dbg(&tmcdrvdata->csdev->dev,
				"Drop data, offset = %d, seq = %d, irq = %d\n",
					drvdata->offset, seq,
					atomic_read(&drvdata->irq_cnt));
				kfree(usb_req->sg);
				kfree(usb_req);
				drvdata->usb_req = NULL;
			}
		}

		if (atomic_read(&drvdata->irq_cnt) > 0)
			atomic_dec(&drvdata->irq_cnt);
	}
	dev_err(&tmcdrvdata->csdev->dev, "TMC has been stopped.\n");
}

static void usb_write_done(struct byte_cntr *drvdata,
				   struct qdss_request *d_req)
{
	atomic_inc(&drvdata->usb_free_buf);
	if (d_req->status)
		pr_err_ratelimited("USB write failed err:%d\n", d_req->status);
	kfree(d_req->sg);
	kfree(d_req);
}

static int usb_bypass_init(struct byte_cntr *byte_cntr_data)
{
	byte_cntr_data->usb_wq = create_singlethread_workqueue("byte-cntr");
	if (!byte_cntr_data->usb_wq)
		return -ENOMEM;

	byte_cntr_data->offset = 0;
	mutex_init(&byte_cntr_data->usb_bypass_lock);
	init_waitqueue_head(&byte_cntr_data->usb_wait_wq);
	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);
	INIT_WORK(&(byte_cntr_data->read_work), usb_read_work_fn);

	return 0;
}

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

	if (!drvdata)
		return;

	if (drvdata->out_mode != TMC_ETR_OUT_MODE_USB) {
		dev_err(&drvdata->csdev->dev,
		"%s: ETR is not USB mode.\n", __func__);
		return;
	}

	switch (event) {
	case USB_QDSS_CONNECT:
		if (drvdata->mode == CS_MODE_DISABLED) {
			dev_err_ratelimited(&drvdata->csdev->dev,
				"%s: ETR is disabled.\n", __func__);
			return;
		}

		if (drvdata->usb_data->usb_mode ==
						TMC_ETR_USB_BAM_TO_BAM) {
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
		} else if (drvdata->usb_data->usb_mode == TMC_ETR_USB_SW) {
			ret = usb_bypass_start(drvdata->byte_cntr);
			if (ret < 0)
				return;

			usb_qdss_alloc_req(ch, USB_BUF_NUM);
			queue_work(drvdata->byte_cntr->usb_wq, &(drvdata->byte_cntr->read_work));
		}
		break;

	case USB_QDSS_DISCONNECT:
		if (drvdata->mode == CS_MODE_DISABLED) {
			dev_err_ratelimited(&drvdata->csdev->dev,
				 "%s: ETR is disabled.\n", __func__);
			return;
		}

		if (drvdata->usb_data->usb_mode ==
						TMC_ETR_USB_BAM_TO_BAM) {
			spin_lock_irqsave(&drvdata->spinlock, flags);
			__tmc_etr_disable_to_bam(drvdata->usb_data);
			spin_unlock_irqrestore(&drvdata->spinlock, flags);

			tmc_etr_bam_disable(drvdata->usb_data);
		} else if (drvdata->usb_data->usb_mode == TMC_ETR_USB_SW) {
			usb_bypass_stop(drvdata->byte_cntr);
			flush_work(&((drvdata->byte_cntr->read_work)));
			usb_qdss_free_req(drvdata->usb_data->usbch);
		}
		break;

	case USB_QDSS_DATA_WRITE_DONE:
		if (drvdata->usb_data->usb_mode == TMC_ETR_USB_SW)
			usb_write_done(drvdata->byte_cntr, d_req);
		break;

	default:
		break;
	};

}

static bool tmc_etr_support_usb_bam(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "usb_bam_support");
}

static bool tmc_etr_support_usb_bypass(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "qcom,sw-usb");
}

int tmc_usb_enable(struct tmc_usb_data *usb_data)
{
	struct tmc_drvdata *tmcdrvdata;

	if (!usb_data)
		return -EINVAL;

	tmcdrvdata = usb_data->tmcdrvdata;
	if (usb_data->usb_mode == TMC_ETR_USB_BAM_TO_BAM)
		usb_data->usbch = usb_qdss_open(USB_QDSS_CH_MSM, tmcdrvdata, usb_notifier);
	else if (usb_data->usb_mode == TMC_ETR_USB_SW)
		usb_data->usbch = usb_qdss_open(USB_QDSS_CH_SW, tmcdrvdata, usb_notifier);

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

	if (usb_data->usb_mode == TMC_ETR_USB_BAM_TO_BAM) {
		spin_lock_irqsave(&tmcdrvdata->spinlock, flags);
		__tmc_etr_disable_to_bam(usb_data);
		spin_unlock_irqrestore(&tmcdrvdata->spinlock, flags);
		tmc_etr_bam_disable(usb_data);
	} else if (usb_data->usb_mode == TMC_ETR_USB_SW)
		usb_bypass_stop(tmcdrvdata->byte_cntr);

	if (usb_data->usbch)
		usb_qdss_close(usb_data->usbch);
	else
		dev_err(&tmcdrvdata->csdev->dev, "usb channel is null.\n");
}

int tmc_etr_usb_init(struct amba_device *adev,
		     struct tmc_drvdata *drvdata)
{
	int ret;
	struct device *dev = &adev->dev;
	struct resource res;
	struct tmc_usb_bam_data *bamdata;
	int s1_bypass = 0;
	struct iommu_domain *domain;
	struct tmc_usb_data *usb_data;
	struct byte_cntr *byte_cntr_data;

	usb_data = devm_kzalloc(dev, sizeof(*usb_data), GFP_KERNEL);
	if (!usb_data)
		return -ENOMEM;

	drvdata->usb_data = usb_data;
	drvdata->usb_data->tmcdrvdata = drvdata;
	byte_cntr_data = drvdata->byte_cntr;

	if (tmc_etr_support_usb_bypass(dev)) {
		usb_data->usb_mode = TMC_ETR_USB_SW;

		if (!byte_cntr_data)
			return -EINVAL;

		ret = usb_bypass_init(byte_cntr_data);
		if (ret)
			return -EINVAL;

		return 0;
	} else if (tmc_etr_support_usb_bam(dev)) {
		usb_data->usb_mode = TMC_ETR_USB_BAM_TO_BAM;
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

	usb_data->usb_mode = TMC_ETR_USB_NONE;
	pr_err("%s: ETR usb property is not configured!\n",
					__func__, dev_name(dev));
	return 0;
}
