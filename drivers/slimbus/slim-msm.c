/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

irqreturn_t msm_slim_port_irq_handler(struct msm_slim_ctrl *dev, u32 pstat)
{
	int i;
	u32 int_en = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
							dev->ver));
	/*
	 * different port-interrupt than what we enabled, ignore.
	 * This may happen if overflow/underflow is reported, but
	 * was disabled due to unavailability of buffers provided by
	 * client.
	 */
	if ((pstat & int_en) == 0)
		return IRQ_HANDLED;
	for (i = dev->port_b; i < MSM_SLIM_NPORTS; i++) {
		if (pstat & (1 << i)) {
			u32 val = readl_relaxed(PGD_PORT(PGD_PORT_STATn,
						i, dev->ver));
			if (val & MSM_PORT_OVERFLOW) {
				dev->ctrl.ports[i-dev->port_b].err =
						SLIM_P_OVERFLOW;
			} else if (val & MSM_PORT_UNDERFLOW) {
				dev->ctrl.ports[i-dev->port_b].err =
					SLIM_P_UNDERFLOW;
			}
		}
	}
	/*
	 * Disable port interrupt here. Re-enable when more
	 * buffers are provided for this port.
	 */
	writel_relaxed((int_en & (~pstat)),
			PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
					dev->ver));
	/* clear port interrupts */
	writel_relaxed(pstat, PGD_THIS_EE(PGD_PORT_INT_CL_EEn,
							dev->ver));
	pr_info("disabled overflow/underflow for port 0x%x", pstat);

	/*
	 * Guarantee that port interrupt bit(s) clearing writes go
	 * through before exiting ISR
	 */
	mb();
	return IRQ_HANDLED;
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
	writel_relaxed(set_cfg, PGD_PORT(PGD_PORT_CFGn, pn, dev->ver));
	writel_relaxed(DEF_BLKSZ, PGD_PORT(PGD_PORT_BLKn, pn, dev->ver));
	writel_relaxed(DEF_TRANSZ, PGD_PORT(PGD_PORT_TRANn, pn, dev->ver));
	/* Make sure that port registers are updated before returning */
	mb();
}

static void msm_slim_disconn_pipe_port(struct msm_slim_ctrl *dev, u8 pn)
{
	struct msm_slim_endp *endpoint = &dev->pipes[pn];
	struct sps_register_event sps_event;
	writel_relaxed(0, PGD_PORT(PGD_PORT_CFGn, (pn + dev->port_b),
					dev->ver));
	/* Make sure port register is updated */
	mb();
	memset(&sps_event, 0, sizeof(sps_event));
	sps_register_event(endpoint->sps, &sps_event);
	sps_disconnect(endpoint->sps);
	dev->pipes[pn].connected = false;
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

	if (dev->pipes[pn].connected &&
			dev->ctrl.ports[pn].state == SLIM_P_CFG) {
		return -EISCONN;
	} else if (dev->pipes[pn].connected) {
		writel_relaxed(0, PGD_PORT(PGD_PORT_CFGn, (pn + dev->port_b),
						dev->ver));
		/* Make sure port disabling goes through */
		mb();
		/* Is pipe already connected in desired direction */
		if ((dev->ctrl.ports[pn].flow == SLIM_SRC &&
			cfg->mode == SPS_MODE_DEST) ||
			(dev->ctrl.ports[pn].flow == SLIM_SINK &&
			 cfg->mode == SPS_MODE_SRC)) {
			msm_hw_set_port(dev, pn + dev->port_b);
			return 0;
		}
		msm_slim_disconn_pipe_port(dev, pn);
	}

	stat = readl_relaxed(PGD_PORT(PGD_PORT_STATn, (pn + dev->port_b),
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
	ret = msm_slim_sps_mem_alloc(dev, &cfg->desc,
				MSM_SLIM_DESC_NUM * sizeof(struct sps_iovec));
	if (ret)
		pr_err("mem alloc for descr failed:%d", ret);
	else
		ret = sps_connect(dev->pipes[pn].sps, cfg);

	if (!ret) {
		dev->pipes[pn].connected = true;
		msm_hw_set_port(dev, pn + dev->port_b);
	}
	return ret;
}

int msm_alloc_port(struct slim_controller *ctrl, u8 pn)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	struct msm_slim_endp *endpoint;
	int ret = 0;
	if (ctrl->ports[pn].req == SLIM_REQ_HALF_DUP ||
		ctrl->ports[pn].req == SLIM_REQ_MULTI_CH)
		return -EPROTONOSUPPORT;
	if (pn >= (MSM_SLIM_NPORTS - dev->port_b))
		return -ENODEV;

	endpoint = &dev->pipes[pn];
	ret = msm_slim_init_endpoint(dev, endpoint);
	dev_dbg(dev->dev, "sps register bam error code:%x\n", ret);
	return ret;
}

void msm_dealloc_port(struct slim_controller *ctrl, u8 pn)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	struct msm_slim_endp *endpoint;
	if (pn >= (MSM_SLIM_NPORTS - dev->port_b))
		return;
	endpoint = &dev->pipes[pn];
	if (dev->pipes[pn].connected)
		msm_slim_disconn_pipe_port(dev, pn);
	if (endpoint->sps) {
		struct sps_connect *config = &endpoint->config;
		msm_slim_free_endpoint(endpoint);
		msm_slim_sps_mem_free(dev, &config->desc);
	}
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

static void msm_slim_port_cb(struct sps_event_notify *ev)
{

	struct completion *comp = ev->data.transfer.user;
	struct sps_iovec *iovec = &ev->data.transfer.iovec;

	if (ev->event_id == SPS_EVENT_DESC_DONE) {

		pr_debug("desc done iovec = (0x%x 0x%x 0x%x)\n",
			iovec->addr, iovec->size, iovec->flags);

	} else {
		pr_err("%s: ERR event %d\n",
					__func__, ev->event_id);
	}
	if (comp)
		complete(comp);
}

int msm_slim_port_xfer(struct slim_controller *ctrl, u8 pn, u8 *iobuf,
			u32 len, struct completion *comp)
{
	struct sps_register_event sreg;
	int ret;
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	if (pn >= 7)
		return -ENODEV;


	sreg.options = (SPS_EVENT_DESC_DONE|SPS_EVENT_ERROR);
	sreg.mode = SPS_TRIGGER_WAIT;
	sreg.xfer_done = NULL;
	sreg.callback = msm_slim_port_cb;
	sreg.user = NULL;
	ret = sps_register_event(dev->pipes[pn].sps, &sreg);
	if (ret) {
		dev_dbg(dev->dev, "sps register event error:%x\n", ret);
		return ret;
	}
	ret = sps_transfer_one(dev->pipes[pn].sps, (u32)iobuf, len, comp,
				SPS_IOVEC_FLAG_INT);
	dev_dbg(dev->dev, "sps submit xfer error code:%x\n", ret);
	if (!ret) {
		/* Enable port interrupts */
		u32 int_port = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
						dev->ver));
		if (!(int_port & (1 << (dev->port_b + pn))))
			writel_relaxed((int_port | (1 << (dev->port_b + pn))),
				PGD_THIS_EE(PGD_PORT_INT_EN_EEn, dev->ver));
		/* Make sure that port registers are updated before returning */
		mb();
	}

	return ret;
}

/* Queue up Tx message buffer */
static int msm_slim_post_tx_msgq(struct msm_slim_ctrl *dev, u8 *buf, int len)
{
	int ret;
	struct msm_slim_endp *endpoint = &dev->tx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_pipe *pipe = endpoint->sps;
	int ix = (buf - (u8 *)mem->base) / SLIM_MSGQ_BUF_LEN;

	u32 phys_addr = mem->phys_base + (SLIM_MSGQ_BUF_LEN * ix);

	for (ret = 0; ret < ((len + 3) >> 2); ret++)
		pr_debug("BAM TX buf[%d]:0x%x", ret, ((u32 *)buf)[ret]);

	ret = sps_transfer_one(pipe, phys_addr, ((len + 3) & 0xFC), NULL,
				SPS_IOVEC_FLAG_EOT);
	if (ret)
		dev_err(dev->dev, "transfer_one() failed 0x%x, %d\n", ret, ix);

	return ret;
}

static u32 *msm_slim_tx_msgq_return(struct msm_slim_ctrl *dev)
{
	struct msm_slim_endp *endpoint = &dev->tx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_pipe *pipe = endpoint->sps;
	struct sps_iovec iovec;
	int ret;

	/* first transaction after establishing connection */
	if (dev->tx_idx == -1) {
		dev->tx_idx = 0;
		return mem->base;
	}
	ret = sps_get_iovec(pipe, &iovec);
	if (ret || iovec.addr == 0) {
		dev_err(dev->dev, "sps_get_iovec() failed 0x%x\n", ret);
		return NULL;
	}

	/* Calculate buffer index */
	dev->tx_idx = (iovec.addr - mem->phys_base) / SLIM_MSGQ_BUF_LEN;

	return (u32 *)((u8 *)mem->base + (dev->tx_idx * SLIM_MSGQ_BUF_LEN));
}

int msm_send_msg_buf(struct msm_slim_ctrl *dev, u32 *buf, u8 len, u32 tx_reg)
{
	if (dev->use_tx_msgqs != MSM_MSGQ_ENABLED) {
		int i;
		for (i = 0; i < (len + 3) >> 2; i++) {
			dev_dbg(dev->dev, "AHB TX data:0x%x\n", buf[i]);
			writel_relaxed(buf[i], dev->base + tx_reg + (i * 4));
		}
		/* Guarantee that message is sent before returning */
		mb();
		return 0;
	}
	return msm_slim_post_tx_msgq(dev, (u8 *)buf, len);
}

u32 *msm_get_msg_buf(struct msm_slim_ctrl *dev, int len)
{
	/*
	 * Currently we block a transaction until the current one completes.
	 * In case we need multiple transactions, use message Q
	 */
	if (dev->use_tx_msgqs != MSM_MSGQ_ENABLED)
		return dev->tx_buf;

	return msm_slim_tx_msgq_return(dev);
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

int msm_slim_connect_endp(struct msm_slim_ctrl *dev,
				struct msm_slim_endp *endpoint,
				struct completion *notify)
{
	int i, ret;
	struct sps_register_event sps_error_event; /* SPS_ERROR */
	struct sps_register_event sps_descr_event; /* DESCR_DONE */
	struct sps_connect *config = &endpoint->config;

	ret = sps_connect(endpoint->sps, config);
	if (ret) {
		dev_err(dev->dev, "sps_connect failed 0x%x\n", ret);
		return ret;
	}

	memset(&sps_descr_event, 0x00, sizeof(sps_descr_event));

	if (notify) {
		sps_descr_event.mode = SPS_TRIGGER_CALLBACK;
		sps_descr_event.options = SPS_O_DESC_DONE;
		sps_descr_event.user = (void *)dev;
		sps_descr_event.xfer_done = notify;

		ret = sps_register_event(endpoint->sps, &sps_descr_event);
		if (ret) {
			dev_err(dev->dev, "sps_connect() failed 0x%x\n", ret);
			goto sps_reg_event_failed;
		}
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

	/*
	 * Call transfer_one for each 4-byte buffer
	 * Use (buf->size/4) - 1 for the number of buffer to post
	 */

	if (endpoint == &dev->rx_msgq) {
		/* Setup the transfer */
		for (i = 0; i < (MSM_SLIM_DESC_NUM - 1); i++) {
			ret = msm_slim_post_rx_msgq(dev, i);
			if (ret) {
				dev_err(dev->dev,
					"post_rx_msgq() failed 0x%x\n", ret);
				goto sps_transfer_failed;
			}
		}
		dev->use_rx_msgqs = MSM_MSGQ_ENABLED;
	} else {
		dev->tx_idx = -1;
		dev->use_tx_msgqs = MSM_MSGQ_ENABLED;
	}

	return 0;
sps_transfer_failed:
	memset(&sps_error_event, 0x00, sizeof(sps_error_event));
	sps_register_event(endpoint->sps, &sps_error_event);
sps_reg_event_failed:
	sps_disconnect(endpoint->sps);
	return ret;
}

static int msm_slim_init_rx_msgq(struct msm_slim_ctrl *dev, u32 pipe_reg)
{
	int ret;
	u32 pipe_offset;
	struct msm_slim_endp *endpoint = &dev->rx_msgq;
	struct sps_connect *config = &endpoint->config;
	struct sps_mem_buffer *descr = &config->desc;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct completion *notify = &dev->rx_msgq_notify;

	init_completion(notify);
	if (dev->use_rx_msgqs == MSM_MSGQ_DISABLED)
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

	/* Allocate memory for the message buffer(s), N descrs, 4-byte mesg */
	ret = msm_slim_sps_mem_alloc(dev, mem, MSM_SLIM_DESC_NUM * 4);
	if (ret) {
		dev_err(dev->dev, "dma_alloc_coherent failed\n");
		goto alloc_buffer_failed;
	}

	ret = msm_slim_connect_endp(dev, endpoint, notify);

	if (!ret)
		return 0;

	msm_slim_sps_mem_free(dev, mem);
alloc_buffer_failed:
	msm_slim_sps_mem_free(dev, descr);
alloc_descr_failed:
	msm_slim_free_endpoint(endpoint);
sps_init_endpoint_failed:
	dev->use_rx_msgqs = MSM_MSGQ_DISABLED;
	return ret;
}

static int msm_slim_init_tx_msgq(struct msm_slim_ctrl *dev, u32 pipe_reg)
{
	int ret;
	u32 pipe_offset;
	struct msm_slim_endp *endpoint = &dev->tx_msgq;
	struct sps_connect *config = &endpoint->config;
	struct sps_mem_buffer *descr = &config->desc;
	struct sps_mem_buffer *mem = &endpoint->buf;

	if (dev->use_tx_msgqs == MSM_MSGQ_DISABLED)
		return 0;

	/* Allocate the endpoint */
	ret = msm_slim_init_endpoint(dev, endpoint);
	if (ret) {
		dev_err(dev->dev, "init_endpoint failed 0x%x\n", ret);
		goto sps_init_endpoint_failed;
	}

	/* Get the pipe indices for the message queues */
	pipe_offset = (readl_relaxed(dev->base + pipe_reg) & 0xfc) >> 2;
	pipe_offset += 1;
	dev_dbg(dev->dev, "TX Message queue pipe offset %d\n", pipe_offset);

	config->mode = SPS_MODE_DEST;
	config->source = SPS_DEV_HANDLE_MEM;
	config->destination = dev->bam.hdl;
	config->dest_pipe_index = pipe_offset;
	config->src_pipe_index = 0;
	config->options = SPS_O_ERROR | SPS_O_NO_Q |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	/* Allocate memory for the FIFO descriptors */
	ret = msm_slim_sps_mem_alloc(dev, descr,
				MSM_TX_BUFS * sizeof(struct sps_iovec));
	if (ret) {
		dev_err(dev->dev, "unable to allocate SPS descriptors\n");
		goto alloc_descr_failed;
	}

	/* Allocate memory for the message buffer(s), N descrs, 40-byte mesg */
	ret = msm_slim_sps_mem_alloc(dev, mem, MSM_TX_BUFS * SLIM_MSGQ_BUF_LEN);
	if (ret) {
		dev_err(dev->dev, "dma_alloc_coherent failed\n");
		goto alloc_buffer_failed;
	}
	ret = msm_slim_connect_endp(dev, endpoint, NULL);

	if (!ret)
		return 0;

	msm_slim_sps_mem_free(dev, mem);
alloc_buffer_failed:
	msm_slim_sps_mem_free(dev, descr);
alloc_descr_failed:
	msm_slim_free_endpoint(endpoint);
sps_init_endpoint_failed:
	dev->use_tx_msgqs = MSM_MSGQ_DISABLED;
	return ret;
}

/* Registers BAM h/w resource with SPS driver and initializes msgq endpoints */
int msm_slim_sps_init(struct msm_slim_ctrl *dev, struct resource *bam_mem,
			u32 pipe_reg, bool remote)
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

	if (dev->bam.hdl) {
		bam_handle = dev->bam.hdl;
		goto init_msgq;
	}
	bam_props.ee = dev->ee;
	bam_props.virt_addr = dev->bam.base;
	bam_props.phys_addr = bam_mem->start;
	bam_props.irq = dev->bam.irq;
	if (!remote) {
		bam_props.manage = SPS_BAM_MGR_LOCAL;
		bam_props.sec_config = SPS_BAM_SEC_DO_CONFIG;
	} else {
		bam_props.manage = SPS_BAM_MGR_DEVICE_REMOTE |
					SPS_BAM_MGR_MULTI_EE;
		bam_props.sec_config = SPS_BAM_SEC_DO_NOT_CONFIG;
	}
	bam_props.summing_threshold = MSM_SLIM_PERF_SUMM_THRESHOLD;

	bam_props.p_sec_config_props = &sec_props;

	bam_props.options = SPS_O_DESC_DONE | SPS_O_ERROR |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	/* override apps channel pipes if specified in platform-data or DT */
	if (dev->pdata.apps_pipes)
		sec_props.ees[dev->ee].pipe_mask = dev->pdata.apps_pipes;

	/* First 7 bits are for message Qs */
	for (i = 7; i < 32; i++) {
		/* Check what pipes are owned by Apps. */
		if ((sec_props.ees[dev->ee].pipe_mask >> i) & 0x1)
			break;
	}
	dev->port_b = i - 7;

	/* Register the BAM device with the SPS driver */
	ret = sps_register_bam_device(&bam_props, &bam_handle);
	if (ret) {
		dev_err(dev->dev, "disabling BAM: reg-bam failed 0x%x\n", ret);
		dev->use_rx_msgqs = MSM_MSGQ_DISABLED;
		dev->use_tx_msgqs = MSM_MSGQ_DISABLED;
		return ret;
	}
	dev->bam.hdl = bam_handle;
	dev_dbg(dev->dev, "SLIM BAM registered, handle = 0x%x\n", bam_handle);

init_msgq:
	ret = msm_slim_init_rx_msgq(dev, pipe_reg);
	if (ret)
		dev_err(dev->dev, "msm_slim_init_rx_msgq failed 0x%x\n", ret);
	if (ret && bam_handle)
		dev->use_rx_msgqs = MSM_MSGQ_DISABLED;

	ret = msm_slim_init_tx_msgq(dev, pipe_reg);
	if (ret)
		dev_err(dev->dev, "msm_slim_init_tx_msgq failed 0x%x\n", ret);
	if (ret && bam_handle)
		dev->use_tx_msgqs = MSM_MSGQ_DISABLED;

	if (dev->use_tx_msgqs == MSM_MSGQ_DISABLED &&
		dev->use_rx_msgqs == MSM_MSGQ_DISABLED && bam_handle) {
		sps_deregister_bam_device(bam_handle);
		dev->bam.hdl = 0L;
	}

	return ret;
}

void msm_slim_disconnect_endp(struct msm_slim_ctrl *dev,
					struct msm_slim_endp *endpoint,
					enum msm_slim_msgq *msgq_flag)
{
	if (*msgq_flag == MSM_MSGQ_ENABLED) {
		sps_disconnect(endpoint->sps);
		*msgq_flag = MSM_MSGQ_RESET;
	}
}

static void msm_slim_remove_ep(struct msm_slim_ctrl *dev,
					struct msm_slim_endp *endpoint,
					enum msm_slim_msgq *msgq_flag)
{
	struct sps_connect *config = &endpoint->config;
	struct sps_mem_buffer *descr = &config->desc;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_register_event sps_event;
	memset(&sps_event, 0x00, sizeof(sps_event));
	msm_slim_sps_mem_free(dev, mem);
	sps_register_event(endpoint->sps, &sps_event);
	if (*msgq_flag == MSM_MSGQ_ENABLED) {
		msm_slim_disconnect_endp(dev, endpoint, msgq_flag);
		msm_slim_free_endpoint(endpoint);
	}
	msm_slim_sps_mem_free(dev, descr);
}

void msm_slim_sps_exit(struct msm_slim_ctrl *dev, bool dereg)
{
	if (dev->use_rx_msgqs >= MSM_MSGQ_ENABLED)
		msm_slim_remove_ep(dev, &dev->rx_msgq, &dev->use_rx_msgqs);
	if (dev->use_tx_msgqs >= MSM_MSGQ_ENABLED)
		msm_slim_remove_ep(dev, &dev->tx_msgq, &dev->use_tx_msgqs);
	if (dereg) {
		int i;
		for (i = dev->port_b; i < MSM_SLIM_NPORTS; i++) {
			if (dev->pipes[i - dev->port_b].connected)
				msm_dealloc_port(&dev->ctrl,
						i - dev->port_b);
		}
		sps_deregister_bam_device(dev->bam.hdl);
		dev->bam.hdl = 0L;
	}
}

/* Slimbus QMI Messaging */
#define SLIMBUS_QMI_SELECT_INSTANCE_REQ_V01 0x0020
#define SLIMBUS_QMI_SELECT_INSTANCE_RESP_V01 0x0020
#define SLIMBUS_QMI_POWER_REQ_V01 0x0021
#define SLIMBUS_QMI_POWER_RESP_V01 0x0021

#define SLIMBUS_QMI_POWER_REQ_MAX_MSG_LEN 7
#define SLIMBUS_QMI_POWER_RESP_MAX_MSG_LEN 7
#define SLIMBUS_QMI_SELECT_INSTANCE_REQ_MAX_MSG_LEN 14
#define SLIMBUS_QMI_SELECT_INSTANCE_RESP_MAX_MSG_LEN 7

enum slimbus_mode_enum_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use*/
	SLIMBUS_MODE_ENUM_TYPE_MIN_ENUM_VAL_V01 = INT_MIN,
	SLIMBUS_MODE_SATELLITE_V01 = 1,
	SLIMBUS_MODE_MASTER_V01 = 2,
	SLIMBUS_MODE_ENUM_TYPE_MAX_ENUM_VAL_V01 = INT_MAX,
};

enum slimbus_pm_enum_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use*/
	SLIMBUS_PM_ENUM_TYPE_MIN_ENUM_VAL_V01 = INT_MIN,
	SLIMBUS_PM_INACTIVE_V01 = 1,
	SLIMBUS_PM_ACTIVE_V01 = 2,
	SLIMBUS_PM_ENUM_TYPE_MAX_ENUM_VAL_V01 = INT_MAX,
};

struct slimbus_select_inst_req_msg_v01 {
	/* Mandatory */
	/* Hardware Instance Selection */
	uint32_t instance;

	/* Optional */
	/* Optional Mode Request Operation */
	/* Must be set to true if mode is being passed */
	uint8_t mode_valid;
	enum slimbus_mode_enum_type_v01 mode;
};

struct slimbus_select_inst_resp_msg_v01 {
	/* Mandatory */
	/* Result Code */
	struct qmi_response_type_v01 resp;
};

struct slimbus_power_req_msg_v01 {
	/* Mandatory */
	/* Power Request Operation */
	enum slimbus_pm_enum_type_v01 pm_req;
};

struct slimbus_power_resp_msg_v01 {
	/* Mandatory */
	/* Result Code */
	struct qmi_response_type_v01 resp;
};

static struct elem_info slimbus_select_inst_req_msg_v01_ei[] = {
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len  = 1,
		.elem_size = sizeof(uint32_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x01,
		.offset    = offsetof(struct slimbus_select_inst_req_msg_v01,
				      instance),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len  = 1,
		.elem_size = sizeof(uint8_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(struct slimbus_select_inst_req_msg_v01,
				      mode_valid),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len  = 1,
		.elem_size = sizeof(enum slimbus_mode_enum_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(struct slimbus_select_inst_req_msg_v01,
				      mode),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x00,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

static struct elem_info slimbus_select_inst_resp_msg_v01_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct slimbus_select_inst_resp_msg_v01,
				      resp),
		.ei_array  = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x00,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

static struct elem_info slimbus_power_req_msg_v01_ei[] = {
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len  = 1,
		.elem_size = sizeof(enum slimbus_pm_enum_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x01,
		.offset    = offsetof(struct slimbus_power_req_msg_v01, pm_req),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x00,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

static struct elem_info slimbus_power_resp_msg_v01_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct slimbus_power_resp_msg_v01, resp),
		.ei_array  = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x00,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

static void msm_slim_qmi_recv_msg(struct kthread_work *work)
{
	int rc;
	struct msm_slim_qmi *qmi =
			container_of(work, struct msm_slim_qmi, kwork);

	rc = qmi_recv_msg(qmi->handle);
	if (rc < 0)
		pr_err("%s: Error receiving QMI message\n", __func__);
}

static void msm_slim_qmi_notify(struct qmi_handle *handle,
				enum qmi_event_type event, void *notify_priv)
{
	struct msm_slim_ctrl *dev = notify_priv;
	struct msm_slim_qmi *qmi = &dev->qmi;

	switch (event) {
	case QMI_RECV_MSG:
		queue_kthread_work(&qmi->kworker, &qmi->kwork);
		break;
	default:
		break;
	}
}

static const char *get_qmi_error(struct qmi_response_type_v01 *r)
{
	if (r->result == QMI_RESULT_SUCCESS_V01 || r->error == QMI_ERR_NONE_V01)
		return "No Error";
	else if (r->error == QMI_ERR_NO_MEMORY_V01)
		return "Out of Memory";
	else if (r->error == QMI_ERR_INTERNAL_V01)
		return "Unexpected error occurred";
	else if (r->error == QMI_ERR_INCOMPATIBLE_STATE_V01)
		return "Slimbus s/w already configured to a different mode";
	else if (r->error == QMI_ERR_INVALID_ID_V01)
		return "Slimbus hardware instance is not valid";
	else
		return "Unknown error";
}

static int msm_slim_qmi_send_select_inst_req(struct msm_slim_ctrl *dev,
				struct slimbus_select_inst_req_msg_v01 *req)
{
	struct slimbus_select_inst_resp_msg_v01 resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.msg_id = SLIMBUS_QMI_SELECT_INSTANCE_REQ_V01;
	req_desc.max_msg_len = SLIMBUS_QMI_SELECT_INSTANCE_REQ_MAX_MSG_LEN;
	req_desc.ei_array = slimbus_select_inst_req_msg_v01_ei;

	resp_desc.msg_id = SLIMBUS_QMI_SELECT_INSTANCE_RESP_V01;
	resp_desc.max_msg_len = SLIMBUS_QMI_SELECT_INSTANCE_RESP_MAX_MSG_LEN;
	resp_desc.ei_array = slimbus_select_inst_resp_msg_v01_ei;

	rc = qmi_send_req_wait(dev->qmi.handle, &req_desc, req, sizeof(*req),
					&resp_desc, &resp, sizeof(resp), 5000);
	if (rc < 0) {
		pr_err("%s: QMI send req failed %d\n", __func__, rc);
		return rc;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed 0x%x (%s)\n", __func__,
				resp.resp.result, get_qmi_error(&resp.resp));
		return -EREMOTEIO;
	}

	return 0;
}

static int msm_slim_qmi_send_power_request(struct msm_slim_ctrl *dev,
				struct slimbus_power_req_msg_v01 *req)
{
	struct slimbus_power_resp_msg_v01 resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.msg_id = SLIMBUS_QMI_POWER_REQ_V01;
	req_desc.max_msg_len = SLIMBUS_QMI_POWER_REQ_MAX_MSG_LEN;
	req_desc.ei_array = slimbus_power_req_msg_v01_ei;

	resp_desc.msg_id = SLIMBUS_QMI_POWER_RESP_V01;
	resp_desc.max_msg_len = SLIMBUS_QMI_POWER_RESP_MAX_MSG_LEN;
	resp_desc.ei_array = slimbus_power_resp_msg_v01_ei;

	rc = qmi_send_req_wait(dev->qmi.handle, &req_desc, req, sizeof(*req),
					&resp_desc, &resp, sizeof(resp), 5000);
	if (rc < 0) {
		pr_err("%s: QMI send req failed %d\n", __func__, rc);
		return rc;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed 0x%x (%s)\n", __func__,
				resp.resp.result, get_qmi_error(&resp.resp));
		return -EREMOTEIO;
	}

	return 0;
}

int msm_slim_qmi_init(struct msm_slim_ctrl *dev, bool apps_is_master)
{
	int rc = 0;
	struct qmi_handle *handle;
	struct slimbus_select_inst_req_msg_v01 req;

	init_kthread_worker(&dev->qmi.kworker);

	dev->qmi.task = kthread_run(kthread_worker_fn,
			&dev->qmi.kworker, "msm_slim_qmi_clnt%d", dev->ctrl.nr);

	if (IS_ERR(dev->qmi.task)) {
		pr_err("%s: Failed to create QMI client kthread\n", __func__);
		return -ENOMEM;
	}

	init_kthread_work(&dev->qmi.kwork, msm_slim_qmi_recv_msg);

	handle = qmi_handle_create(msm_slim_qmi_notify, dev);
	if (!handle) {
		rc = -ENOMEM;
		pr_err("%s: QMI client handle alloc failed\n", __func__);
		goto qmi_handle_create_failed;
	}

	rc = qmi_connect_to_service(handle, SLIMBUS_QMI_SVC_ID,
						SLIMBUS_QMI_INS_ID);
	if (rc < 0) {
		pr_err("%s: QMI server not found\n", __func__);
		goto qmi_connect_to_service_failed;
	}

	/* Instance is 0 based */
	req.instance = dev->ctrl.nr - 1;
	req.mode_valid = 1;

	/* Mode indicates the role of the ADSP */
	if (apps_is_master)
		req.mode = SLIMBUS_MODE_SATELLITE_V01;
	else
		req.mode = SLIMBUS_MODE_MASTER_V01;

	dev->qmi.handle = handle;

	rc = msm_slim_qmi_send_select_inst_req(dev, &req);
	if (rc) {
		pr_err("%s: failed to select h/w instance\n", __func__);
		goto qmi_select_instance_failed;
	}

	return 0;

qmi_select_instance_failed:
	dev->qmi.handle = NULL;
qmi_connect_to_service_failed:
	qmi_handle_destroy(handle);
qmi_handle_create_failed:
	flush_kthread_worker(&dev->qmi.kworker);
	kthread_stop(dev->qmi.task);
	dev->qmi.task = NULL;
	return rc;
}

void msm_slim_qmi_exit(struct msm_slim_ctrl *dev)
{
	qmi_handle_destroy(dev->qmi.handle);
	flush_kthread_worker(&dev->qmi.kworker);
	kthread_stop(dev->qmi.task);
	dev->qmi.task = NULL;
	dev->qmi.handle = NULL;
}

int msm_slim_qmi_power_request(struct msm_slim_ctrl *dev, bool active)
{
	struct slimbus_power_req_msg_v01 req;

	if (active)
		req.pm_req = SLIMBUS_PM_ACTIVE_V01;
	else
		req.pm_req = SLIMBUS_PM_INACTIVE_V01;

	return msm_slim_qmi_send_power_request(dev, &req);
}
