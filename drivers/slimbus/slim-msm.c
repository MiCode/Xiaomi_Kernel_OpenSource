/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/slimbus/slimbus.h>
#include <mach/sps.h>
#include "slim-msm.h"

int msm_slim_rx_enqueue(struct msm_slim_ctrl *dev, u32 *buf, u8 len)
{
	spin_lock(&dev->rx_lock);
	if ((dev->tail + 1) % MSM_CONCUR_MSG == dev->head) {
		spin_unlock(&dev->rx_lock);
		dev_err(dev->dev, "RX QUEUE full!");
		return -EXFULL;
	}
	memcpy((u8 *)dev->rx_msgs[dev->tail], (u8 *)buf, len);
	dev->tail = (dev->tail + 1) % MSM_CONCUR_MSG;
	spin_unlock(&dev->rx_lock);
	return 0;
}

int msm_slim_rx_dequeue(struct msm_slim_ctrl *dev, u8 *buf)
{
	unsigned long flags;
	spin_lock_irqsave(&dev->rx_lock, flags);
	if (dev->tail == dev->head) {
		spin_unlock_irqrestore(&dev->rx_lock, flags);
		return -ENODATA;
	}
	memcpy(buf, (u8 *)dev->rx_msgs[dev->head], 40);
	dev->head = (dev->head + 1) % MSM_CONCUR_MSG;
	spin_unlock_irqrestore(&dev->rx_lock, flags);
	return 0;
}

int msm_slim_get_ctrl(struct msm_slim_ctrl *dev)
{
#ifdef CONFIG_PM_RUNTIME
	int ref = 0;
	int ret = pm_runtime_get_sync(dev->dev);
	if (ret >= 0) {
		ref = atomic_read(&dev->dev->power.usage_count);
		if (ref <= 0) {
			dev_err(dev->dev, "reference count -ve:%d", ref);
			ret = -ENODEV;
		}
	}
	return ret;
#else
	return -ENODEV;
#endif
}
void msm_slim_put_ctrl(struct msm_slim_ctrl *dev)
{
#ifdef CONFIG_PM_RUNTIME
	int ref;
	pm_runtime_mark_last_busy(dev->dev);
	ref = atomic_read(&dev->dev->power.usage_count);
	if (ref <= 0)
		dev_err(dev->dev, "reference count mismatch:%d", ref);
	else
		pm_runtime_put(dev->dev);
#endif
}

int msm_slim_init_endpoint(struct msm_slim_ctrl *dev, struct msm_slim_endp *ep)
{
	int ret;
	struct sps_pipe *endpoint;
	struct sps_connect *config = &ep->config;

	/* Allocate the endpoint */
	endpoint = sps_alloc_endpoint();
	if (!endpoint) {
		dev_err(dev->dev, "sps_alloc_endpoint failed\n");
		return -ENOMEM;
	}

	/* Get default connection configuration for an endpoint */
	ret = sps_get_config(endpoint, config);
	if (ret) {
		dev_err(dev->dev, "sps_get_config failed 0x%x\n", ret);
		goto sps_config_failed;
	}

	ep->sps = endpoint;
	return 0;

sps_config_failed:
	sps_free_endpoint(endpoint);
	return ret;
}

void msm_slim_free_endpoint(struct msm_slim_endp *ep)
{
	sps_free_endpoint(ep->sps);
	ep->sps = NULL;
}

int msm_slim_sps_mem_alloc(
		struct msm_slim_ctrl *dev, struct sps_mem_buffer *mem, u32 len)
{
	dma_addr_t phys;

	mem->size = len;
	mem->min_size = 0;
	mem->base = dma_alloc_coherent(dev->dev, mem->size, &phys, GFP_KERNEL);

	if (!mem->base) {
		dev_err(dev->dev, "dma_alloc_coherent(%d) failed\n", len);
		return -ENOMEM;
	}

	mem->phys_base = phys;
	memset(mem->base, 0x00, mem->size);
	return 0;
}

void
msm_slim_sps_mem_free(struct msm_slim_ctrl *dev, struct sps_mem_buffer *mem)
{
	dma_free_coherent(dev->dev, mem->size, mem->base, mem->phys_base);
	mem->size = 0;
	mem->base = NULL;
	mem->phys_base = 0;
}

void msm_hw_set_port(struct msm_slim_ctrl *dev, u8 pn)
{
	u32 set_cfg = DEF_WATERMARK | DEF_ALIGN | DEF_PACK | ENABLE_PORT;
	u32 int_port = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
					dev->ver));
	writel_relaxed(set_cfg, PGD_PORT(PGD_PORT_CFGn, pn, dev->ver));
	writel_relaxed(DEF_BLKSZ, PGD_PORT(PGD_PORT_BLKn, pn, dev->ver));
	writel_relaxed(DEF_TRANSZ, PGD_PORT(PGD_PORT_TRANn, pn, dev->ver));
	writel_relaxed((int_port | 1 << pn) , PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
								dev->ver));
	/* Make sure that port registers are updated before returning */
	mb();
}

int msm_slim_connect_pipe_port(struct msm_slim_ctrl *dev, u8 pn)
{
	struct msm_slim_endp *endpoint = &dev->pipes[pn];
	struct sps_connect *cfg = &endpoint->config;
	u32 stat;
	int ret = sps_get_config(dev->pipes[pn].sps, cfg);
	if (ret) {
		dev_err(dev->dev, "sps pipe-port get config error%x\n", ret);
		return ret;
	}
	cfg->options = SPS_O_DESC_DONE | SPS_O_ERROR |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	if (dev->pipes[pn].connected) {
		ret = sps_set_config(dev->pipes[pn].sps, cfg);
		if (ret) {
			dev_err(dev->dev, "sps pipe-port set config erro:%x\n",
						ret);
			return ret;
		}
	}

	stat = readl_relaxed(PGD_PORT(PGD_PORT_STATn, (pn + dev->pipe_b),
					dev->ver));
	if (dev->ctrl.ports[pn].flow == SLIM_SRC) {
		cfg->destination = dev->bam.hdl;
		cfg->source = SPS_DEV_HANDLE_MEM;
		cfg->dest_pipe_index = ((stat & (0xFF << 4)) >> 4);
		cfg->src_pipe_index = 0;
		dev_dbg(dev->dev, "flow src:pipe num:%d",
					cfg->dest_pipe_index);
		cfg->mode = SPS_MODE_DEST;
	} else {
		cfg->source = dev->bam.hdl;
		cfg->destination = SPS_DEV_HANDLE_MEM;
		cfg->src_pipe_index = ((stat & (0xFF << 4)) >> 4);
		cfg->dest_pipe_index = 0;
		dev_dbg(dev->dev, "flow dest:pipe num:%d",
					cfg->src_pipe_index);
		cfg->mode = SPS_MODE_SRC;
	}
	/* Space for desciptor FIFOs */
	cfg->desc.size = MSM_SLIM_DESC_NUM * sizeof(struct sps_iovec);
	cfg->config = SPS_CONFIG_DEFAULT;
	ret = sps_connect(dev->pipes[pn].sps, cfg);
	if (!ret) {
		dev->pipes[pn].connected = true;
		msm_hw_set_port(dev, pn + dev->pipe_b);
	}
	return ret;
}

int msm_config_port(struct slim_controller *ctrl, u8 pn)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	struct msm_slim_endp *endpoint;
	int ret = 0;
	if (ctrl->ports[pn].req == SLIM_REQ_HALF_DUP ||
		ctrl->ports[pn].req == SLIM_REQ_MULTI_CH)
		return -EPROTONOSUPPORT;
	if (pn >= (MSM_SLIM_NPORTS - dev->pipe_b))
		return -ENODEV;

	endpoint = &dev->pipes[pn];
	ret = msm_slim_init_endpoint(dev, endpoint);
	dev_dbg(dev->dev, "sps register bam error code:%x\n", ret);
	return ret;
}

enum slim_port_err msm_slim_port_xfer_status(struct slim_controller *ctr,
				u8 pn, u8 **done_buf, u32 *done_len)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctr);
	struct sps_iovec sio;
	int ret;
	if (done_len)
		*done_len = 0;
	if (done_buf)
		*done_buf = NULL;
	if (!dev->pipes[pn].connected)
		return SLIM_P_DISCONNECT;
	ret = sps_get_iovec(dev->pipes[pn].sps, &sio);
	if (!ret) {
		if (done_len)
			*done_len = sio.size;
		if (done_buf)
			*done_buf = (u8 *)sio.addr;
	}
	dev_dbg(dev->dev, "get iovec returned %d\n", ret);
	return SLIM_P_INPROGRESS;
}

int msm_slim_port_xfer(struct slim_controller *ctrl, u8 pn, u8 *iobuf,
			u32 len, struct completion *comp)
{
	struct sps_register_event sreg;
	int ret;
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	if (pn >= 7)
		return -ENODEV;


	ctrl->ports[pn].xcomp = comp;
	sreg.options = (SPS_EVENT_DESC_DONE|SPS_EVENT_ERROR);
	sreg.mode = SPS_TRIGGER_WAIT;
	sreg.xfer_done = comp;
	sreg.callback = NULL;
	sreg.user = &ctrl->ports[pn];
	ret = sps_register_event(dev->pipes[pn].sps, &sreg);
	if (ret) {
		dev_dbg(dev->dev, "sps register event error:%x\n", ret);
		return ret;
	}
	ret = sps_transfer_one(dev->pipes[pn].sps, (u32)iobuf, len, NULL,
				SPS_IOVEC_FLAG_INT);
	dev_dbg(dev->dev, "sps submit xfer error code:%x\n", ret);

	return ret;
}

int msm_send_msg_buf(struct msm_slim_ctrl *dev, u32 *buf, u8 len, u32 tx_reg)
{
	int i;
	for (i = 0; i < (len + 3) >> 2; i++) {
		dev_dbg(dev->dev, "TX data:0x%x\n", buf[i]);
		writel_relaxed(buf[i], dev->base + tx_reg + (i * 4));
	}
	/* Guarantee that message is sent before returning */
	mb();
	return 0;
}

u32 *msm_get_msg_buf(struct msm_slim_ctrl *dev, int len)
{
	/*
	 * Currently we block a transaction until the current one completes.
	 * In case we need multiple transactions, use message Q
	 */
	return dev->tx_buf;
}

static void
msm_slim_rx_msgq_event(struct msm_slim_ctrl *dev, struct sps_event_notify *ev)
{
	u32 *buf = ev->data.transfer.user;
	struct sps_iovec *iovec = &ev->data.transfer.iovec;

	/*
	 * Note the virtual address needs to be offset by the same index
	 * as the physical address or just pass in the actual virtual address
	 * if the sps_mem_buffer is not needed.  Note that if completion is
	 * used, the virtual address won't be available and will need to be
	 * calculated based on the offset of the physical address
	 */
	if (ev->event_id == SPS_EVENT_DESC_DONE) {

		pr_debug("buf = 0x%p, data = 0x%x\n", buf, *buf);

		pr_debug("iovec = (0x%x 0x%x 0x%x)\n",
			iovec->addr, iovec->size, iovec->flags);

	} else {
		dev_err(dev->dev, "%s: unknown event %d\n",
					__func__, ev->event_id);
	}
}

static void msm_slim_rx_msgq_cb(struct sps_event_notify *notify)
{
	struct msm_slim_ctrl *dev = (struct msm_slim_ctrl *)notify->user;
	msm_slim_rx_msgq_event(dev, notify);
}

/* Queue up Rx message buffer */
static int msm_slim_post_rx_msgq(struct msm_slim_ctrl *dev, int ix)
{
	int ret;
	u32 flags = SPS_IOVEC_FLAG_INT;
	struct msm_slim_endp *endpoint = &dev->rx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_pipe *pipe = endpoint->sps;

	/* Rx message queue buffers are 4 bytes in length */
	u8 *virt_addr = mem->base + (4 * ix);
	u32 phys_addr = mem->phys_base + (4 * ix);

	pr_debug("index:%d, phys:0x%x, virt:0x%p\n", ix, phys_addr, virt_addr);

	ret = sps_transfer_one(pipe, phys_addr, 4, virt_addr, flags);
	if (ret)
		dev_err(dev->dev, "transfer_one() failed 0x%x, %d\n", ret, ix);

	return ret;
}

int msm_slim_rx_msgq_get(struct msm_slim_ctrl *dev, u32 *data, int offset)
{
	struct msm_slim_endp *endpoint = &dev->rx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_pipe *pipe = endpoint->sps;
	struct sps_iovec iovec;
	int index;
	int ret;

	ret = sps_get_iovec(pipe, &iovec);
	if (ret) {
		dev_err(dev->dev, "sps_get_iovec() failed 0x%x\n", ret);
		goto err_exit;
	}

	pr_debug("iovec = (0x%x 0x%x 0x%x)\n",
		iovec.addr, iovec.size, iovec.flags);
	BUG_ON(iovec.addr < mem->phys_base);
	BUG_ON(iovec.addr >= mem->phys_base + mem->size);

	/* Calculate buffer index */
	index = (iovec.addr - mem->phys_base) / 4;
	*(data + offset) = *((u32 *)mem->base + index);

	pr_debug("buf = 0x%p, data = 0x%x\n", (u32 *)mem->base + index, *data);

	/* Add buffer back to the queue */
	(void)msm_slim_post_rx_msgq(dev, index);

err_exit:
	return ret;
}

static int msm_slim_init_rx_msgq(struct msm_slim_ctrl *dev, u32 pipe_reg)
{
	int i, ret;
	u32 pipe_offset;
	struct msm_slim_endp *endpoint = &dev->rx_msgq;
	struct sps_connect *config = &endpoint->config;
	struct sps_mem_buffer *descr = &config->desc;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct completion *notify = &dev->rx_msgq_notify;

	struct sps_register_event sps_error_event; /* SPS_ERROR */
	struct sps_register_event sps_descr_event; /* DESCR_DONE */

	init_completion(notify);
	if (!dev->use_rx_msgqs)
		return 0;

	/* Allocate the endpoint */
	ret = msm_slim_init_endpoint(dev, endpoint);
	if (ret) {
		dev_err(dev->dev, "init_endpoint failed 0x%x\n", ret);
		goto sps_init_endpoint_failed;
	}

	/* Get the pipe indices for the message queues */
	pipe_offset = (readl_relaxed(dev->base + pipe_reg) & 0xfc) >> 2;
	dev_dbg(dev->dev, "Message queue pipe offset %d\n", pipe_offset);

	config->mode = SPS_MODE_SRC;
	config->source = dev->bam.hdl;
	config->destination = SPS_DEV_HANDLE_MEM;
	config->src_pipe_index = pipe_offset;
	config->options = SPS_O_DESC_DONE | SPS_O_ERROR |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	/* Allocate memory for the FIFO descriptors */
	ret = msm_slim_sps_mem_alloc(dev, descr,
				MSM_SLIM_DESC_NUM * sizeof(struct sps_iovec));
	if (ret) {
		dev_err(dev->dev, "unable to allocate SPS descriptors\n");
		goto alloc_descr_failed;
	}

	ret = sps_connect(endpoint->sps, config);
	if (ret) {
		dev_err(dev->dev, "sps_connect failed 0x%x\n", ret);
		goto sps_connect_failed;
	}

	memset(&sps_descr_event, 0x00, sizeof(sps_descr_event));

	sps_descr_event.mode = SPS_TRIGGER_CALLBACK;
	sps_descr_event.options = SPS_O_DESC_DONE;
	sps_descr_event.user = (void *)dev;
	sps_descr_event.xfer_done = notify;

	ret = sps_register_event(endpoint->sps, &sps_descr_event);
	if (ret) {
		dev_err(dev->dev, "sps_connect() failed 0x%x\n", ret);
		goto sps_reg_event_failed;
	}

	/* Register callback for errors */
	memset(&sps_error_event, 0x00, sizeof(sps_error_event));
	sps_error_event.mode = SPS_TRIGGER_CALLBACK;
	sps_error_event.options = SPS_O_ERROR;
	sps_error_event.user = (void *)dev;
	sps_error_event.callback = msm_slim_rx_msgq_cb;

	ret = sps_register_event(endpoint->sps, &sps_error_event);
	if (ret) {
		dev_err(dev->dev, "sps_connect() failed 0x%x\n", ret);
		goto sps_reg_event_failed;
	}

	/* Allocate memory for the message buffer(s), N descrs, 4-byte mesg */
	ret = msm_slim_sps_mem_alloc(dev, mem, MSM_SLIM_DESC_NUM * 4);
	if (ret) {
		dev_err(dev->dev, "dma_alloc_coherent failed\n");
		goto alloc_buffer_failed;
	}

	/*
	 * Call transfer_one for each 4-byte buffer
	 * Use (buf->size/4) - 1 for the number of buffer to post
	 */

	/* Setup the transfer */
	for (i = 0; i < (MSM_SLIM_DESC_NUM - 1); i++) {
		ret = msm_slim_post_rx_msgq(dev, i);
		if (ret) {
			dev_err(dev->dev, "post_rx_msgq() failed 0x%x\n", ret);
			goto sps_transfer_failed;
		}
	}

	return 0;

sps_transfer_failed:
	msm_slim_sps_mem_free(dev, mem);
alloc_buffer_failed:
	memset(&sps_error_event, 0x00, sizeof(sps_error_event));
	sps_register_event(endpoint->sps, &sps_error_event);
sps_reg_event_failed:
	sps_disconnect(endpoint->sps);
sps_connect_failed:
	msm_slim_sps_mem_free(dev, descr);
alloc_descr_failed:
	msm_slim_free_endpoint(endpoint);
sps_init_endpoint_failed:
	dev->use_rx_msgqs = 0;
	return ret;
}

/* Registers BAM h/w resource with SPS driver and initializes msgq endpoints */
int msm_slim_sps_init(struct msm_slim_ctrl *dev, struct resource *bam_mem,
			u32 pipe_reg)
{
	int i, ret;
	u32 bam_handle;
	struct sps_bam_props bam_props = {0};

	static struct sps_bam_sec_config_props sec_props = {
		.ees = {
			[0] = {		/* LPASS */
				.vmid = 0,
				.pipe_mask = 0xFFFF98,
			},
			[1] = {		/* Krait Apps */
				.vmid = 1,
				.pipe_mask = 0x3F000007,
			},
			[2] = {		/* Modem */
				.vmid = 2,
				.pipe_mask = 0x00000060,
			},
		},
	};

	bam_props.ee = dev->ee;
	bam_props.virt_addr = dev->bam.base;
	bam_props.phys_addr = bam_mem->start;
	bam_props.irq = dev->bam.irq;
	bam_props.manage = SPS_BAM_MGR_LOCAL;
	bam_props.summing_threshold = MSM_SLIM_PERF_SUMM_THRESHOLD;

	bam_props.sec_config = SPS_BAM_SEC_DO_CONFIG;
	bam_props.p_sec_config_props = &sec_props;

	bam_props.options = SPS_O_DESC_DONE | SPS_O_ERROR |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	/* First 7 bits are for message Qs */
	for (i = 7; i < 32; i++) {
		/* Check what pipes are owned by Apps. */
		if ((sec_props.ees[dev->ee].pipe_mask >> i) & 0x1)
			break;
	}
	dev->pipe_b = i - 7;

	/* Register the BAM device with the SPS driver */
	ret = sps_register_bam_device(&bam_props, &bam_handle);
	if (ret) {
		dev_err(dev->dev, "disabling BAM: reg-bam failed 0x%x\n", ret);
		dev->use_rx_msgqs = 0;
		goto init_rx_msgq;
	}
	dev->bam.hdl = bam_handle;
	dev_dbg(dev->dev, "SLIM BAM registered, handle = 0x%x\n", bam_handle);

init_rx_msgq:
	ret = msm_slim_init_rx_msgq(dev, pipe_reg);
	if (ret)
		dev_err(dev->dev, "msm_slim_init_rx_msgq failed 0x%x\n", ret);
	if (ret && bam_handle) {
		sps_deregister_bam_device(bam_handle);
		dev->bam.hdl = 0L;
	}
	return ret;
}

void msm_slim_sps_exit(struct msm_slim_ctrl *dev)
{
	if (dev->use_rx_msgqs) {
		struct msm_slim_endp *endpoint = &dev->rx_msgq;
		struct sps_connect *config = &endpoint->config;
		struct sps_mem_buffer *descr = &config->desc;
		struct sps_mem_buffer *mem = &endpoint->buf;
		struct sps_register_event sps_event;
		memset(&sps_event, 0x00, sizeof(sps_event));
		msm_slim_sps_mem_free(dev, mem);
		sps_register_event(endpoint->sps, &sps_event);
		sps_disconnect(endpoint->sps);
		msm_slim_sps_mem_free(dev, descr);
		msm_slim_free_endpoint(endpoint);
		sps_deregister_bam_device(dev->bam.hdl);
	}
}
