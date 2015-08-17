/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Qualcomm technologies inc, DMA API for BAM (Bus Access Manager).
 * This DMA driver uses sps-BAM API to access the HW, thus it is effectively a
 * DMA engine wrapper of the sps-BAM API.
 *
 * Client channel configuration example:
 * struct dma_slave_config config {
 *    .direction = DMA_MEM_TO_DEV;
 * };
 *
 * chan = dma_request_slave_channel(client_dev, "rx");
 * dmaengine_slave_config(chan, &config);
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/list.h>
#include <linux/msm-sps.h>
#include "dmaengine.h"

#define QBAM_OF_SLAVE_N_ARGS	(4)
#define QBAM_OF_MANAGE_LOCAL	"qcom,managed-locally"
#define QBAM_OF_SUM_THRESHOLD	"qcom,summing-threshold"
#define QBAM_MAX_DESCRIPTORS	(0x100)
#define QBAM_MAX_CHANNELS	(32)

/*
 * qbam_async_tx_descriptor - dma descriptor plus a list of xfer_bufs
 *
 * @sgl scatterlist of transfer buffers
 * @sg_len size of that list
 * @flags dma xfer flags
 */
struct qbam_async_tx_descriptor {
	struct dma_async_tx_descriptor	dma_desc;
	struct scatterlist		*sgl;
	unsigned int			sg_len;
	unsigned long			flags;
};

#define DMA_TO_QBAM_ASYNC_DESC(dma_async_desc) \
	container_of(dma_async_desc, struct qbam_async_tx_descriptor, dma_desc)

struct qbam_channel;
/*
 * qbam_device - top level device of current driver
 * @handle bam sps handle.
 * @regs bam register space virtual base address.
 * @mem_resource bam register space resource.
 * @deregister_required if bam is registered by this driver it need to be
 *   unregistered by this driver.
 * @manage is bame managed locally or remotely,
 * @summing_threshold event threshold.
 * @irq bam interrupt line.
 * @channels has the same channels as qbam_dev->dma_dev.channels but
 *   supports fast access by pipe index.
 */
struct qbam_device {
	struct dma_device		dma_dev;
	void __iomem			*regs;
	struct resource			*mem_resource;
	ulong				handle;
	bool				deregister_required;
	u32				summing_threshold;
	u32				manage;
	int				irq;
	struct qbam_channel		*channels[QBAM_MAX_CHANNELS];
};

/* qbam_pipe: aggregate of bam pipe related entries of qbam_channel */
struct qbam_pipe {
	u32				index;
	struct sps_pipe			*handle;
	struct sps_connect		cfg;
	u32				num_descriptors;
	u32				sps_connect_flags;
	u32				sps_register_event_flags;
};

/*
 * qbam_channel - dma channel plus bam pipe info and current pending transfers
 *
 * @direction is a producer or consumer (MEM => DEV or DEV => MEM)
 * @pending_desc next set of transfer to process
 * @error last error that took place on the current pending_desc
 */
struct qbam_channel {
	struct qbam_pipe		bam_pipe;

	struct dma_chan			chan;
	enum dma_transfer_direction	direction;
	struct qbam_async_tx_descriptor	pending_desc;

	struct qbam_device		*qbam_dev;
	struct mutex			lock;
	int				error;
};
#define DMA_TO_QBAM_CHAN(dma_chan) \
			container_of(dma_chan, struct qbam_channel, chan)
#define qbam_err(qbam_dev, fmt ...) dev_err(qbam_dev->dma_dev.dev, fmt)

/*  qbam_disconnect_chan - disconnect a channel */
static int qbam_disconnect_chan(struct qbam_channel *qbam_chan)
{
	struct qbam_device  *qbam_dev    = qbam_chan->qbam_dev;
	struct sps_pipe     *pipe_handle = qbam_chan->bam_pipe.handle;
	struct sps_connect   pipe_config_no_irq = {.options = SPS_O_POLL};
	int ret;

	/*
	 * SW workaround:
	 * When disconnecting BAM pipe a spurious interrupt sometimes appears.
	 * To avoid that, we change the pipe setting from interrupt (default)
	 * to polling (SPS_O_POLL) before diconnecting the pipe.
	 */
	ret = sps_set_config(pipe_handle, &pipe_config_no_irq);
	if (ret)
		qbam_err(qbam_dev,
			"error:%d sps_set_config(pipe:%d) before disconnect\n",
			ret, qbam_chan->bam_pipe.index);

	ret = sps_disconnect(pipe_handle);
	if (ret)
		qbam_err(qbam_dev, "error:%d sps_disconnect(pipe:%d)\n",
			 ret, qbam_chan->bam_pipe.index);

	return ret;
}

/*  qbam_free_chan - disconnect channel and free its resources */
static void qbam_free_chan(struct dma_chan *chan)
{
	struct qbam_channel *qbam_chan = DMA_TO_QBAM_CHAN(chan);
	struct qbam_device  *qbam_dev  = qbam_chan->qbam_dev;

	mutex_lock(&qbam_chan->lock);
	if (qbam_disconnect_chan(qbam_chan))
		qbam_err(qbam_dev,
			"error free_chan() failed to disconnect(pipe:%d)\n",
			qbam_chan->bam_pipe.index);
	qbam_chan->pending_desc.sgl = NULL;
	qbam_chan->pending_desc.sg_len = 0;
	mutex_unlock(&qbam_chan->lock);
}

static struct dma_chan *qbam_dma_xlate(struct of_phandle_args *dma_spec,
							struct of_dma *of)
{
	struct qbam_device  *qbam_dev  = of->of_dma_data;
	struct qbam_channel *qbam_chan;
	u32 channel_index;
	u32 num_descriptors;

	if (dma_spec->args_count != QBAM_OF_SLAVE_N_ARGS) {
		qbam_err(qbam_dev,
			"invalid number of dma arguments, expect:%d got:%d\n",
			QBAM_OF_SLAVE_N_ARGS, dma_spec->args_count);
		return NULL;
	};

	channel_index = dma_spec->args[0];

	if (channel_index >= QBAM_MAX_CHANNELS) {
		qbam_err(qbam_dev,
			"error: channel_index:%d out of bounds",
			channel_index);
		return NULL;
	}
	qbam_chan = qbam_dev->channels[channel_index];
	 /* return qbam_chan if exists, or create one */
	if (qbam_chan) {
		qbam_chan->chan.client_count = 1;
		return &qbam_chan->chan;
	}

	num_descriptors = dma_spec->args[1];
	if (!num_descriptors || (num_descriptors > QBAM_MAX_DESCRIPTORS)) {
		qbam_err(qbam_dev,
			"invalid number of descriptors, range[1..%d] got:%d\n",
			QBAM_MAX_DESCRIPTORS, num_descriptors);
		return NULL;
	}

	/* allocate a channel */
	qbam_chan = kzalloc(sizeof(*qbam_chan), GFP_KERNEL);
	if (!qbam_chan) {
		qbam_err(qbam_dev, "error kmalloc(size:%llu) failed\n",
			 (u64) sizeof(*qbam_chan));
		return NULL;
	}

	/* allocate BAM resources for that channel */
	qbam_chan->bam_pipe.handle = sps_alloc_endpoint();
	if (!qbam_chan->bam_pipe.handle) {
		qbam_err(qbam_dev, "error: sps_alloc_endpoint() return NULL\n");
		kfree(qbam_chan);
		return NULL;
	}

	/* init dma_chan */
	qbam_chan->chan.device = &qbam_dev->dma_dev;
	dma_cookie_init(&qbam_chan->chan);
	qbam_chan->chan.client_count                 = 1;
	/* init qbam_chan */
	qbam_chan->bam_pipe.index                    = channel_index;
	qbam_chan->bam_pipe.num_descriptors          = num_descriptors;
	qbam_chan->bam_pipe.sps_connect_flags        = dma_spec->args[2];
	qbam_chan->bam_pipe.sps_register_event_flags = dma_spec->args[3];
	qbam_chan->qbam_dev                          = qbam_dev;
	mutex_init(&qbam_chan->lock);

	/* add to dma_device list of channels */
	list_add(&qbam_chan->chan.device_node, &qbam_dev->dma_dev.channels);
	qbam_dev->channels[channel_index] = qbam_chan;

	return &qbam_chan->chan;
}

static enum dma_status qbam_tx_status(struct dma_chan *chan,
			dma_cookie_t cookie, struct dma_tx_state *state)
{
	struct qbam_channel *qbam_chan = DMA_TO_QBAM_CHAN(chan);
	struct qbam_async_tx_descriptor	*qbam_desc = &qbam_chan->pending_desc;
	enum dma_status ret;

	mutex_lock(&qbam_chan->lock);

	if (qbam_chan->error) {
		mutex_unlock(&qbam_chan->lock);
		return DMA_ERROR;
	}

	ret = dma_cookie_status(chan, cookie, state);
	if (ret == DMA_IN_PROGRESS) {
		struct scatterlist *sg;
		int i;
		u32 transfer_size = 0;

		for_each_sg(qbam_desc->sgl, sg, qbam_desc->sg_len, i)
			transfer_size += sg_dma_len(sg);

		dma_set_residue(state, transfer_size);
	}
	mutex_unlock(&qbam_chan->lock);

	return ret;
}

/*
 * qbam_init_bam_handle - find or create bam handle.
 *
 * BAM device needs to be registerd for each BLSP once and only once. if it was
 * registred, then we find the handle to the registerd bam and return it,
 * otherwise we register it here.
 * The module which registerd BAM is responsible for deregistering it.
 */
static int qbam_init_bam_handle(struct qbam_device *qbam_dev)
{
	int ret = 0;
	struct sps_bam_props bam_props = {0};

	/*
	 * Check if BAM is already registred with SPS on the current
	 * BLSP. If it isn't then go ahead and register it.
	 */
	ret = sps_phy2h(qbam_dev->mem_resource->start, &qbam_dev->handle);
	if (qbam_dev->handle)
		return 0;

	qbam_dev->regs = devm_ioremap_resource(qbam_dev->dma_dev.dev,
					       qbam_dev->mem_resource);
	if (IS_ERR(qbam_dev->regs)) {
		qbam_err(qbam_dev, "error:%ld ioremap(phy:0x%lx len:0x%lx)\n",
			 PTR_ERR(qbam_dev->regs),
			 (ulong) qbam_dev->mem_resource->start,
			 (ulong) resource_size(qbam_dev->mem_resource));
		return PTR_ERR(qbam_dev->regs);
	};

	bam_props.phys_addr		= qbam_dev->mem_resource->start;
	bam_props.virt_addr		= qbam_dev->regs;
	bam_props.summing_threshold	= qbam_dev->summing_threshold;
	bam_props.manage		= qbam_dev->manage;
	bam_props.irq			= qbam_dev->irq;

	ret = sps_register_bam_device(&bam_props, &qbam_dev->handle);
	if (ret)
		qbam_err(qbam_dev, "error:%d sps_register_bam_device\n"
			 "(phy:0x%lx virt:0x%lx irq:%d)\n",
			 ret, (ulong) bam_props.phys_addr,
			 (ulong) bam_props.virt_addr, qbam_dev->irq);
	else
		qbam_dev->deregister_required = true;

	return ret;
}


static int qbam_alloc_chan(struct dma_chan *chan)
{
	return 0;
}

static void qbam_eot_callback(struct sps_event_notify *notify)
{
	struct qbam_async_tx_descriptor *qbam_desc = notify->data.transfer.user;
	struct dma_async_tx_descriptor  *dma_desc  = &qbam_desc->dma_desc;
	dma_async_tx_callback callback	= dma_desc->callback;
	void *param			= dma_desc->callback_param;

	if (callback)
		callback(param);
}

static void qbam_error_callback(struct sps_event_notify *notify)
{
	struct qbam_channel *qbam_chan	= notify->user;
	qbam_err(qbam_chan->qbam_dev, "error: qbam_error_callback(pipe:%d\n)",
		 qbam_chan->bam_pipe.index);
}

static int qbam_connect_chan(struct qbam_channel *qbam_chan)
{
	int ret = 0;
	struct qbam_device       *qbam_dev = qbam_chan->qbam_dev;
	struct sps_register_event bam_eot_event = {
		.mode		= SPS_TRIGGER_CALLBACK,
		.options	= qbam_chan->bam_pipe.sps_register_event_flags,
		.callback	= qbam_eot_callback,
		};
	struct sps_register_event bam_error_event = {
		.mode		= SPS_TRIGGER_CALLBACK,
		.options	= SPS_O_ERROR,
		.callback	= qbam_error_callback,
		.user		= qbam_chan,
		};

	ret = sps_connect(qbam_chan->bam_pipe.handle, &qbam_chan->bam_pipe.cfg);
	if (ret) {
		qbam_err(qbam_dev, "error:%d sps_connect(pipe:%d)\n", ret,
			 qbam_chan->bam_pipe.index);
		return ret;
	}

	ret = sps_register_event(qbam_chan->bam_pipe.handle, &bam_eot_event);
	if (ret) {
		qbam_err(qbam_dev, "error:%d sps_register_event(eot@pipe:%d)\n",
			 ret, qbam_chan->bam_pipe.index);
		goto need_disconnect;
	}

	ret = sps_register_event(qbam_chan->bam_pipe.handle, &bam_error_event);
	if (ret) {
		qbam_err(qbam_dev, "error:%d sps_register_event(err@pipe:%d)\n",
			 ret, qbam_chan->bam_pipe.index);
		goto need_disconnect;
	}

	return 0;

need_disconnect:
	ret = sps_disconnect(qbam_chan->bam_pipe.handle);
	if (ret)
		qbam_err(qbam_dev, "error:%d sps_disconnect(pipe:%d)\n", ret,
			 qbam_chan->bam_pipe.index);
	return ret;
}

/*
 * qbam_slave_cfg - configure and connect a BAM pipe
 *
 * @cfg only cares about cfg->direction
 */
static int qbam_slave_cfg(struct qbam_channel *qbam_chan,
						struct dma_slave_config *cfg)
{
	int ret = 0;
	struct qbam_device *qbam_dev = qbam_chan->qbam_dev;
	struct sps_connect *pipe_cfg = &qbam_chan->bam_pipe.cfg;

	if (!qbam_dev->handle) {
		ret = qbam_init_bam_handle(qbam_dev);
		if (ret)
			return ret;
	}

	if (qbam_chan->bam_pipe.cfg.desc.base)
		goto cfg_done;

	ret = sps_get_config(qbam_chan->bam_pipe.handle,
						&qbam_chan->bam_pipe.cfg);
	if (ret) {
		qbam_err(qbam_dev, "error:%d sps_get_config(0x%p)\n",
			 ret, qbam_chan->bam_pipe.handle);
		return ret;
	}

	qbam_chan->direction = cfg->direction;
	if (cfg->direction == DMA_MEM_TO_DEV) {
		pipe_cfg->source          = SPS_DEV_HANDLE_MEM;
		pipe_cfg->destination     = qbam_dev->handle;
		pipe_cfg->mode            = SPS_MODE_DEST;
		pipe_cfg->src_pipe_index  = 0;
		pipe_cfg->dest_pipe_index = qbam_chan->bam_pipe.index;
	} else {
		pipe_cfg->source          = qbam_dev->handle;
		pipe_cfg->destination     = SPS_DEV_HANDLE_MEM;
		pipe_cfg->mode            = SPS_MODE_SRC;
		pipe_cfg->src_pipe_index  = qbam_chan->bam_pipe.index;
		pipe_cfg->dest_pipe_index = 0;
	}
	pipe_cfg->options   =  qbam_chan->bam_pipe.sps_connect_flags;
	pipe_cfg->desc.size = (qbam_chan->bam_pipe.num_descriptors + 1) *
						 sizeof(struct sps_iovec);
	/* managed dma_alloc_coherent() */
	pipe_cfg->desc.base = dmam_alloc_coherent(qbam_dev->dma_dev.dev,
						  pipe_cfg->desc.size,
						  &pipe_cfg->desc.phys_base,
						  GFP_KERNEL);
	if (!pipe_cfg->desc.base) {
		qbam_err(qbam_dev,
			"error dma_alloc_coherent(desc-sz:%llu * n-descs:%d)\n",
			(u64) sizeof(struct sps_iovec),
			qbam_chan->bam_pipe.num_descriptors);
		return -ENOMEM;
	}
cfg_done:
	ret = qbam_connect_chan(qbam_chan);
	if (ret)
		dmam_free_coherent(qbam_dev->dma_dev.dev, pipe_cfg->desc.size,
				 pipe_cfg->desc.base, pipe_cfg->desc.phys_base);

	return ret;
}

static int qbam_flush_chan(struct qbam_channel *qbam_chan)
{
	int ret = qbam_disconnect_chan(qbam_chan);
	if (ret) {
		qbam_err(qbam_chan->qbam_dev,
			 "error: disconnect flush(pipe:%d\n)",
			 qbam_chan->bam_pipe.index);
		return ret;
	}
	ret = qbam_connect_chan(qbam_chan);
	if (ret)
		qbam_err(qbam_chan->qbam_dev,
			 "error: reconnect flush(pipe:%d\n)",
			 qbam_chan->bam_pipe.index);
	return ret;
}

/*
 * qbam_control - DMA device control. entry point for channel configuration.
 * @chan: dma channel
 * @cmd: control cmd
 * @arg: cmd argument
 */
static int qbam_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
							unsigned long arg)
{
	struct qbam_channel *qbam_chan = DMA_TO_QBAM_CHAN(chan);
	int ret = 0;

	switch (cmd) {
	case DMA_SLAVE_CONFIG:
		ret = qbam_slave_cfg(qbam_chan, (struct dma_slave_config *)arg);
		break;
	case DMA_TERMINATE_ALL:
		ret = qbam_flush_chan(qbam_chan);
		break;
	default:
		ret = -ENXIO;
		qbam_err(qbam_chan->qbam_dev,
			"error qbam_control(cmd:%d) unsupported\n", cmd);
		break;
	};

	return ret;
}

/* qbam_tx_submit - sets the descriptor as the next one to be executed */
static dma_cookie_t qbam_tx_submit(struct dma_async_tx_descriptor *dma_desc)
{
	struct qbam_channel *qbam_chan = DMA_TO_QBAM_CHAN(dma_desc->chan);
	dma_cookie_t ret;
	mutex_lock(&qbam_chan->lock);

	ret = dma_cookie_assign(dma_desc);

	mutex_unlock(&qbam_chan->lock);

	return ret;
}

/*
 * qbam_prep_slave_sg - creates qbam_xfer_buf from a list of sg
 *
 * @chan: dma channel
 * @sgl: scatter gather list
 * @sg_len: length of sg
 * @direction: DMA transfer direction
 * @flags: DMA flags
 * @context: transfer context (unused)
 * @return the newly created descriptor or negative ERR_PTR() on error
 */
static struct dma_async_tx_descriptor *qbam_prep_slave_sg(struct dma_chan *chan,
	struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	struct qbam_channel *qbam_chan = DMA_TO_QBAM_CHAN(chan);
	struct qbam_device *qbam_dev = qbam_chan->qbam_dev;
	struct qbam_async_tx_descriptor *qbam_desc = &qbam_chan->pending_desc;

	if (qbam_chan->direction != direction) {
		qbam_err(qbam_dev,
			"invalid dma transfer direction expected:%d given:%d\n",
			qbam_chan->direction, direction);
		return ERR_PTR(-EINVAL);
	}

	qbam_desc->dma_desc.chan	= &qbam_chan->chan;
	qbam_desc->dma_desc.tx_submit	= qbam_tx_submit;
	qbam_desc->sgl			= sgl;
	qbam_desc->sg_len		= sg_len;
	qbam_desc->flags		= flags;
	return &qbam_desc->dma_desc;
}

/*
 * qbam_issue_pending - queue pending descriptor to BAM
 *
 * Iterate over the transfers of the pending descriptor and push them to bam
 */
static void qbam_issue_pending(struct dma_chan *chan)
{
	int i;
	int ret = 0;
	struct qbam_channel *qbam_chan = DMA_TO_QBAM_CHAN(chan);
	struct qbam_device  *qbam_dev  = qbam_chan->qbam_dev;
	struct qbam_async_tx_descriptor *qbam_desc = &qbam_chan->pending_desc;
	struct scatterlist		*sg;
	mutex_lock(&qbam_chan->lock);
	if (!qbam_chan->pending_desc.sgl) {
		qbam_err(qbam_dev,
		   "error qbam_issue_pending() no pending descriptor pipe:%d\n",
		   qbam_chan->bam_pipe.index);
		mutex_unlock(&qbam_chan->lock);
		return;
	}

	for_each_sg(qbam_desc->sgl, sg, qbam_desc->sg_len, i) {

		/* Add BAM flags only on the last buffer */
		bool is_last_buf = (i == ((qbam_desc->sg_len) - 1));

		ret = sps_transfer_one(qbam_chan->bam_pipe.handle,
					sg_dma_address(sg), sg_dma_len(sg),
					qbam_desc,
					(is_last_buf ? qbam_desc->flags : 0));
		if (ret < 0) {
			qbam_chan->error = ret;

			qbam_err(qbam_dev, "erorr:%d sps_transfer_one\n"
				"(addr:0x%lx len:%d flags:0x%lx pipe:%d)\n",
				ret, (ulong) sg_dma_address(sg), sg_dma_len(sg),
				qbam_desc->flags, qbam_chan->bam_pipe.index);
			break;
		}
	}

	dma_cookie_complete(&qbam_desc->dma_desc);
	qbam_chan->error = 0;
	qbam_desc->sgl = NULL;
	qbam_desc->sg_len = 0;
	mutex_unlock(&qbam_chan->lock);
};

static int qbam_deregister_bam_dev(struct qbam_device *qbam_dev)
{
	int ret;

	if (!qbam_dev->handle)
		return 0;

	ret = sps_deregister_bam_device(qbam_dev->handle);
	if (ret)
		qbam_err(qbam_dev,
			"error:%d sps_deregister_bam_device(hndl:0x%lx) failed",
			ret, qbam_dev->handle);
	return ret;
}

static void qbam_pipes_free(struct qbam_device *qbam_dev)
{
	struct qbam_channel *qbam_chan_cur, *qbam_chan_next;

	list_for_each_entry_safe(qbam_chan_cur, qbam_chan_next,
			&qbam_dev->dma_dev.channels, chan.device_node) {
		mutex_lock(&qbam_chan_cur->lock);
		qbam_free_chan(&qbam_chan_cur->chan);
		sps_free_endpoint(qbam_chan_cur->bam_pipe.handle);
		list_del(&qbam_chan_cur->chan.device_node);
		mutex_unlock(&qbam_chan_cur->lock);
		kfree(qbam_chan_cur);
	}
}

static int qbam_probe(struct platform_device *pdev)
{
	struct qbam_device *qbam_dev;
	int ret;
	bool managed_locally;
	struct device_node *of_node = pdev->dev.of_node;

	qbam_dev = devm_kzalloc(&pdev->dev, sizeof(*qbam_dev), GFP_KERNEL);
	if (!qbam_dev) {
		qbam_err(qbam_dev, "error kmalloc(size:%llu) failed",
			(u64) sizeof(*qbam_dev));
		return -ENOMEM;
	}
	qbam_dev->dma_dev.dev = &pdev->dev;
	platform_set_drvdata(pdev, qbam_dev);

	qbam_dev->mem_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!qbam_dev->mem_resource) {
		qbam_err(qbam_dev, "missing 'reg' DT entry");
		return -ENODEV;
	}

	qbam_dev->irq = platform_get_irq(pdev, 0);
	if (qbam_dev->irq < 0) {
		qbam_err(qbam_dev, "missing DT IRQ resource entry");
		return -EINVAL;
	}

	ret = of_property_read_u32(of_node, QBAM_OF_SUM_THRESHOLD,
				   &qbam_dev->summing_threshold);
	if (ret) {
		qbam_err(qbam_dev, "missing '%s' DT entry",
			 QBAM_OF_SUM_THRESHOLD);
		return ret;
	}

	/* read from DT and set sps_bam_props.manage */
	managed_locally = of_property_read_bool(of_node, QBAM_OF_MANAGE_LOCAL);
	qbam_dev->manage = managed_locally ? SPS_BAM_MGR_LOCAL :
					     SPS_BAM_MGR_DEVICE_REMOTE;

	/* Init channels */
	INIT_LIST_HEAD(&qbam_dev->dma_dev.channels);

	/* Set capabilities */
	dma_cap_zero(qbam_dev->dma_dev.cap_mask);
	dma_cap_set(DMA_SLAVE,		qbam_dev->dma_dev.cap_mask);
	dma_cap_set(DMA_PRIVATE,	qbam_dev->dma_dev.cap_mask);

	/* Initialize dmaengine callback apis */
	qbam_dev->dma_dev.device_alloc_chan_resources	= qbam_alloc_chan;
	qbam_dev->dma_dev.device_free_chan_resources	= qbam_free_chan;
	qbam_dev->dma_dev.device_prep_slave_sg		= qbam_prep_slave_sg;
	qbam_dev->dma_dev.device_control		= qbam_control;
	qbam_dev->dma_dev.device_issue_pending		= qbam_issue_pending;
	qbam_dev->dma_dev.device_tx_status		= qbam_tx_status;

	/* Regiser to DMA framework */
	ret = dma_async_device_register(&qbam_dev->dma_dev);
	if (ret) {
		qbam_err(qbam_dev, "error:%d dma_async_device_register()\n",
			 ret);
		goto err_unregister_bam;
	}

	ret = of_dma_controller_register(of_node, qbam_dma_xlate, qbam_dev);
	if (ret) {
		qbam_err(qbam_dev, "error:%d of_dma_controller_register()\n",
			 ret);
		goto err_unregister_dma;
	}
	return 0;

err_unregister_dma:
	dma_async_device_unregister(&qbam_dev->dma_dev);
err_unregister_bam:
	if (qbam_dev->deregister_required)
		return qbam_deregister_bam_dev(qbam_dev);

	return ret;
}

static int qbam_remove(struct platform_device *pdev)
{
	struct qbam_device *qbam_dev = platform_get_drvdata(pdev);

	dma_async_device_unregister(&qbam_dev->dma_dev);

	/* free BAM pipes resources */
	qbam_pipes_free(qbam_dev);

	if (qbam_dev->deregister_required)
		return qbam_deregister_bam_dev(qbam_dev);

	return 0;
}

static const struct of_device_id qbam_of_match[] = {
	{ .compatible = "qcom,sps-dma" },
	{}
};
MODULE_DEVICE_TABLE(of, qbam_of_match);

static struct platform_driver qbam_driver = {
	.probe = qbam_probe,
	.remove = qbam_remove,
	.driver = {
		.name = "qcom-sps-dma",
		.owner = THIS_MODULE,
		.of_match_table = qbam_of_match,
	},
};

module_platform_driver(qbam_driver);

MODULE_DESCRIPTION("DMA-API driver to qcom BAM");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qcom-sps-dma");
