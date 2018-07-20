/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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
#include <asm/dma-iommu.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gcd.h>
#include <linux/msm-sps.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/slimbus/slimbus.h>
#include "slim-msm.h"

/* Pipe Number Offset Mask */
#define P_OFF_MASK 0x3FC
#define MSM_SLIM_VA_START	(0x40000000)
#define MSM_SLIM_VA_SIZE	(0xC0000000)

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
#ifdef CONFIG_PM
	int ref = 0;
	int ret = pm_runtime_get_sync(dev->dev);

	if (ret >= 0) {
		ref = atomic_read(&dev->dev->power.usage_count);
		if (ref <= 0) {
			SLIM_WARN(dev, "reference count -ve:%d", ref);
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
#ifdef CONFIG_PM
	int ref;

	pm_runtime_mark_last_busy(dev->dev);
	ref = atomic_read(&dev->dev->power.usage_count);
	if (ref <= 0)
		SLIM_WARN(dev, "reference count mismatch:%d", ref);
	else
		pm_runtime_put_sync(dev->dev);
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
	for (i = 0; i < dev->port_nums; i++) {
		struct msm_slim_endp *endpoint = &dev->pipes[i];

		if (pstat & (1 << endpoint->port_b)) {
			u32 val = readl_relaxed(PGD_PORT(PGD_PORT_STATn,
					endpoint->port_b, dev->ver));
			if (val & MSM_PORT_OVERFLOW) {
				dev->ctrl.ports[i].err =
						SLIM_P_OVERFLOW;
			} else if (val & MSM_PORT_UNDERFLOW) {
				dev->ctrl.ports[i].err =
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
	SLIM_INFO(dev, "disabled overflow/underflow for port 0x%x", pstat);

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

static int msm_slim_iommu_attach(struct msm_slim_ctrl *ctrl_dev)
{
	struct dma_iommu_mapping *iommu_map;
	dma_addr_t va_start = MSM_SLIM_VA_START;
	size_t va_size = MSM_SLIM_VA_SIZE;
	int bypass = 1;
	struct device *dev;

	if (unlikely(!ctrl_dev))
		return -EINVAL;

	if (!ctrl_dev->iommu_desc.cb_dev)
		return 0;

	if (!IS_ERR_OR_NULL(ctrl_dev->iommu_desc.iommu_map)) {
		arm_iommu_detach_device(ctrl_dev->iommu_desc.cb_dev);
		arm_iommu_release_mapping(ctrl_dev->iommu_desc.iommu_map);
		ctrl_dev->iommu_desc.iommu_map = NULL;
		SLIM_INFO(ctrl_dev, "NGD IOMMU Dettach complete\n");
	}

	dev = ctrl_dev->iommu_desc.cb_dev;
	iommu_map = arm_iommu_create_mapping(&platform_bus_type,
						va_start, va_size);
	if (IS_ERR(iommu_map)) {
		dev_err(dev, "%s iommu_create_mapping failure\n", __func__);
		return PTR_ERR(iommu_map);
	}

	if (ctrl_dev->iommu_desc.s1_bypass) {
		if (iommu_domain_set_attr(iommu_map->domain,
					DOMAIN_ATTR_S1_BYPASS, &bypass)) {
			dev_err(dev, "%s Can't bypass s1 translation\n",
				__func__);
			arm_iommu_release_mapping(iommu_map);
			return -EIO;
		}
	}

	if (arm_iommu_attach_device(dev, iommu_map)) {
		dev_err(dev, "%s can't arm_iommu_attach_device\n", __func__);
		arm_iommu_release_mapping(iommu_map);
		return -EIO;
	}
	ctrl_dev->iommu_desc.iommu_map = iommu_map;
	SLIM_INFO(ctrl_dev, "NGD IOMMU Attach complete\n");
	return 0;
}

int msm_slim_sps_mem_alloc(
		struct msm_slim_ctrl *dev, struct sps_mem_buffer *mem, u32 len)
{
	dma_addr_t phys;
	struct device *dma_dev = dev->iommu_desc.cb_dev ?
					dev->iommu_desc.cb_dev : dev->dev;

	mem->size = len;
	mem->min_size = 0;
	mem->base = dma_alloc_coherent(dma_dev, mem->size, &phys, GFP_KERNEL);

	if (!mem->base) {
		dev_err(dma_dev, "dma_alloc_coherent(%d) failed\n", len);
		return -ENOMEM;
	}

	mem->phys_base = phys;
	memset(mem->base, 0x00, mem->size);
	return 0;
}

void
msm_slim_sps_mem_free(struct msm_slim_ctrl *dev, struct sps_mem_buffer *mem)
{
	if (mem->base && mem->phys_base)
		dma_free_coherent(dev->dev, mem->size, mem->base,
							mem->phys_base);
	else
		dev_err(dev->dev, "cant dma free. they are NULL\n");
	mem->size = 0;
	mem->base = NULL;
	mem->phys_base = 0;
}

void msm_hw_set_port(struct msm_slim_ctrl *dev, u8 pipenum, u8 portnum)
{
	struct slim_controller *ctrl;
	struct slim_ch *chan;
	struct msm_slim_pshpull_parm *parm;
	u32 set_cfg = 0;
	struct slim_port_cfg cfg = dev->ctrl.ports[portnum].cfg;

	if (!dev) {
		pr_err("%s:Dev node is null\n", __func__);
		return;
	}
	if (portnum >= dev->port_nums) {
		pr_err("%s:Invalid port\n", __func__);
		return;
	}
	ctrl = &dev->ctrl;
	chan = ctrl->ports[portnum].ch;
	parm = &dev->pipes[portnum].psh_pull;

	if (cfg.watermark)
		set_cfg = (cfg.watermark << 1);
	else
		set_cfg = DEF_WATERMARK;

	if (cfg.port_opts & SLIM_OPT_NO_PACK)
		set_cfg |= DEF_NO_PACK;
	else
		set_cfg |= DEF_PACK;

	if (cfg.port_opts & SLIM_OPT_ALIGN_MSB)
		set_cfg |= DEF_ALIGN_MSB;
	else
		set_cfg |= DEF_ALIGN_LSB;

	set_cfg |= ENABLE_PORT;

	writel_relaxed(set_cfg, PGD_PORT(PGD_PORT_CFGn, pipenum, dev->ver));
	writel_relaxed(DEF_BLKSZ, PGD_PORT(PGD_PORT_BLKn, pipenum, dev->ver));
	writel_relaxed(DEF_TRANSZ, PGD_PORT(PGD_PORT_TRANn, pipenum, dev->ver));

	if (chan->prot == SLIM_PUSH || chan->prot == SLIM_PULL) {
		set_cfg = 0;
		set_cfg |= ((0xFFFF & parm->num_samples)<<16);
		set_cfg |= (0xFFFF & parm->rpt_period);
		writel_relaxed(set_cfg, PGD_PORT(PGD_PORT_PSHPLLn,
							pipenum, dev->ver));
	}
	/* Make sure that port registers are updated before returning */
	mb();
}

static void msm_slim_disconn_pipe_port(struct msm_slim_ctrl *dev, u8 pn)
{
	struct msm_slim_endp *endpoint = &dev->pipes[pn];
	struct sps_register_event sps_event;
	u32 int_port = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
					dev->ver));
	writel_relaxed(0, PGD_PORT(PGD_PORT_CFGn, (endpoint->port_b),
					dev->ver));
	writel_relaxed((int_port & ~(1 << endpoint->port_b)),
		PGD_THIS_EE(PGD_PORT_INT_EN_EEn, dev->ver));
	/* Make sure port register is updated */
	mb();
	memset(&sps_event, 0, sizeof(sps_event));
	sps_register_event(endpoint->sps, &sps_event);
	sps_disconnect(endpoint->sps);
	dev->pipes[pn].connected = false;
}

static void msm_slim_calc_pshpull_parm(struct msm_slim_ctrl *dev,
					u8 pn, struct slim_ch *prop)
{
	struct msm_slim_endp *endpoint = &dev->pipes[pn];
	struct msm_slim_pshpull_parm *parm = &endpoint->psh_pull;
	int	chan_freq, round_off, divisor, super_freq;

	super_freq = dev->ctrl.a_framer->superfreq;

	if (prop->baser == SLIM_RATE_4000HZ)
		chan_freq = 4000 * prop->ratem;
	else if (prop->baser == SLIM_RATE_11025HZ)
		chan_freq = 11025 * prop->ratem;
	else
		chan_freq = prop->baser * prop->ratem;

	/*
	 * If channel frequency is multiple of super frame frequency
	 * ISO protocol is suggested
	 */
	if (!(chan_freq % super_freq)) {
		prop->prot = SLIM_HARD_ISO;
		return;
	}
	round_off = DIV_ROUND_UP(chan_freq, super_freq);
	divisor = gcd(round_off * super_freq, chan_freq);
	parm->num_samples = chan_freq/divisor;
	parm->rpt_period = (round_off * super_freq)/divisor;
}

int msm_slim_connect_pipe_port(struct msm_slim_ctrl *dev, u8 pn)
{
	struct msm_slim_endp *endpoint;
	struct sps_connect *cfg;
	struct slim_ch *prop;
	u32 stat;
	int ret;

	if (!dev || pn >= dev->port_nums)
		return -ENODEV;
	endpoint = &dev->pipes[pn];
	cfg = &endpoint->config;
	prop = dev->ctrl.ports[pn].ch;

	endpoint = &dev->pipes[pn];
	ret = sps_get_config(dev->pipes[pn].sps, cfg);
	if (ret) {
		dev_err(dev->dev, "sps pipe-port get config error%x\n", ret);
		return ret;
	}
	cfg->options = SPS_O_DESC_DONE | SPS_O_ERROR |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	if (prop->prot == SLIM_PUSH || prop->prot ==  SLIM_PULL)
		msm_slim_calc_pshpull_parm(dev, pn, prop);

	if (dev->pipes[pn].connected &&
			dev->ctrl.ports[pn].state == SLIM_P_CFG) {
		return -EISCONN;
	} else if (dev->pipes[pn].connected) {
		writel_relaxed(0, PGD_PORT(PGD_PORT_CFGn,
			(endpoint->port_b), dev->ver));
		/* Make sure port disabling goes through */
		mb();
		/* Is pipe already connected in desired direction */
		if ((dev->ctrl.ports[pn].flow == SLIM_SRC &&
			cfg->mode == SPS_MODE_DEST) ||
			(dev->ctrl.ports[pn].flow == SLIM_SINK &&
			 cfg->mode == SPS_MODE_SRC)) {
			msm_hw_set_port(dev, endpoint->port_b, pn);
			return 0;
		}
		msm_slim_disconn_pipe_port(dev, pn);
	}

	stat = readl_relaxed(PGD_PORT(PGD_PORT_STATn, endpoint->port_b,
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
		msm_hw_set_port(dev, endpoint->port_b, pn);
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
	if (pn >= dev->port_nums)
		return -ENODEV;

	ret = msm_slim_iommu_attach(dev);
	if (ret)
		return ret;

	endpoint = &dev->pipes[pn];
	ret = msm_slim_init_endpoint(dev, endpoint);
	dev_dbg(dev->dev, "sps register bam error code:%x\n", ret);
	return ret;
}

void msm_dealloc_port(struct slim_controller *ctrl, u8 pn)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	struct msm_slim_endp *endpoint;

	if (pn >= dev->port_nums)
		return;
	endpoint = &dev->pipes[pn];
	if (dev->pipes[pn].connected) {
		struct sps_connect *config = &endpoint->config;

		msm_slim_disconn_pipe_port(dev, pn);
		msm_slim_sps_mem_free(dev, &config->desc);
	}
	if (endpoint->sps)
		msm_slim_free_endpoint(endpoint);
}

enum slim_port_err msm_slim_port_xfer_status(struct slim_controller *ctr,
				u8 pn, phys_addr_t *done_buf, u32 *done_len)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctr);
	struct sps_iovec sio;
	int ret;

	if (done_len)
		*done_len = 0;
	if (done_buf)
		*done_buf = 0;
	if (!dev->pipes[pn].connected)
		return SLIM_P_DISCONNECT;
	ret = sps_get_iovec(dev->pipes[pn].sps, &sio);
	if (!ret) {
		if (done_len)
			*done_len = sio.size;
		if (done_buf)
			*done_buf = (phys_addr_t)sio.addr;
	}
	dev_dbg(dev->dev, "get iovec returned %d\n", ret);
	return SLIM_P_INPROGRESS;
}

static dma_addr_t msm_slim_iommu_map(struct msm_slim_ctrl *dev, void *buf_addr,
			      u32 len)
{
	dma_addr_t ret;
	struct device *devp = dev->iommu_desc.cb_dev ? dev->iommu_desc.cb_dev :
							dev->dev;
	ret = dma_map_single(devp, buf_addr, len, DMA_BIDIRECTIONAL);

	if (dma_mapping_error(devp, ret))
		return DMA_ERROR_CODE;

	return ret;
}

static void msm_slim_iommu_unmap(struct msm_slim_ctrl *dev, dma_addr_t buf_addr,
				u32 len)
{
	struct device *devp = dev->iommu_desc.cb_dev ? dev->iommu_desc.cb_dev :
							dev->dev;
	dma_unmap_single(devp, buf_addr, len, DMA_BIDIRECTIONAL);
}

static void msm_slim_port_cb(struct sps_event_notify *ev)
{
	struct msm_slim_ctrl *dev = ev->user;
	struct completion *comp = ev->data.transfer.user;
	struct sps_iovec *iovec = &ev->data.transfer.iovec;

	if (ev->event_id == SPS_EVENT_DESC_DONE) {

		pr_debug("desc done iovec = (0x%x 0x%x 0x%x)\n",
			iovec->addr, iovec->size, iovec->flags);

	} else {
		pr_err("%s: ERR event %d\n",
					__func__, ev->event_id);
	}
	if (dev)
		msm_slim_iommu_unmap(dev, iovec->addr, iovec->size);
	if (comp)
		complete(comp);
}

int msm_slim_port_xfer(struct slim_controller *ctrl, u8 pn, void *buf,
			u32 len, struct completion *comp)
{
	struct sps_register_event sreg;
	int ret;
	dma_addr_t dma_buf;
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);

	if (pn >= dev->port_nums)
		return -ENODEV;

	if (!dev->pipes[pn].connected)
		return -ENOTCONN;

	dma_buf =  msm_slim_iommu_map(dev, buf, len);
	if (dma_buf == DMA_ERROR_CODE) {
		dev_err(dev->dev, "error DMA mapping buffers\n");
		return -ENOMEM;
	}

	sreg.options = (SPS_EVENT_DESC_DONE|SPS_EVENT_ERROR);
	sreg.mode = SPS_TRIGGER_WAIT;
	sreg.xfer_done = NULL;
	sreg.callback = msm_slim_port_cb;
	sreg.user = dev;
	ret = sps_register_event(dev->pipes[pn].sps, &sreg);
	if (ret) {
		dev_dbg(dev->dev, "sps register event error:%x\n", ret);
		msm_slim_iommu_unmap(dev, dma_buf, len);
		return ret;
	}
	ret = sps_transfer_one(dev->pipes[pn].sps, dma_buf, len, comp,
				SPS_IOVEC_FLAG_INT);
	dev_dbg(dev->dev, "sps submit xfer error code:%x\n", ret);
	if (!ret) {
		/* Enable port interrupts */
		u32 int_port = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_EN_EEn,
						dev->ver));
		if (!(int_port & (1 << (dev->pipes[pn].port_b))))
			writel_relaxed((int_port |
				(1 << dev->pipes[pn].port_b)),
				PGD_THIS_EE(PGD_PORT_INT_EN_EEn, dev->ver));
		/* Make sure that port registers are updated before returning */
		mb();
	} else {
		msm_slim_iommu_unmap(dev, dma_buf, len);
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
	int ix = (buf - (u8 *)mem->base);

	phys_addr_t phys_addr = mem->phys_base + ix;

	for (ret = 0; ret < ((len + 3) >> 2); ret++)
		pr_debug("BAM TX buf[%d]:0x%x", ret, ((u32 *)buf)[ret]);

	ret = sps_transfer_one(pipe, phys_addr, ((len + 3) & 0xFC), NULL,
				SPS_IOVEC_FLAG_EOT);
	if (ret)
		dev_err(dev->dev, "transfer_one() failed 0x%x, %d\n", ret, ix);

	return ret;
}

void msm_slim_tx_msg_return(struct msm_slim_ctrl *dev, int err)
{
	struct msm_slim_endp *endpoint = &dev->tx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_pipe *pipe = endpoint->sps;
	struct sps_iovec iovec;
	int idx, ret = 0;
	phys_addr_t addr;

	if (dev->use_tx_msgqs != MSM_MSGQ_ENABLED) {
		/* use 1 buffer, non-blocking writes are not possible */
		if (dev->wr_comp[0]) {
			struct completion *comp = dev->wr_comp[0];

			dev->wr_comp[0] = NULL;
			complete(comp);
		}
		return;
	}
	while (!ret) {
		memset(&iovec, 0, sizeof(iovec));
		ret = sps_get_iovec(pipe, &iovec);
		addr = DESC_FULL_ADDR(iovec.flags, iovec.addr);
		if (ret || addr == 0) {
			if (ret)
				pr_err("SLIM TX get IOVEC failed:%d", ret);
			return;
		}
		if (addr == dev->bulk.wr_dma) {
			dma_unmap_single(dev->dev, dev->bulk.wr_dma,
					 dev->bulk.size, DMA_TO_DEVICE);
			if (!dev->bulk.cb)
				SLIM_WARN(dev, "no callback for bulk WR?");
			else
				dev->bulk.cb(dev->bulk.ctx, err);
			dev->bulk.in_progress = false;
			pm_runtime_mark_last_busy(dev->dev);
			return;
		} else if (addr < mem->phys_base ||
			   (addr > (mem->phys_base +
				    (MSM_TX_BUFS * SLIM_MSGQ_BUF_LEN)))) {
			SLIM_WARN(dev, "BUF out of bounds:base:0x%pa, io:0x%pa",
					&mem->phys_base, &addr);
			continue;
		}
		idx = (int) ((addr - mem->phys_base)
			/ SLIM_MSGQ_BUF_LEN);
		if (dev->wr_comp[idx]) {
			struct completion *comp = dev->wr_comp[idx];

			dev->wr_comp[idx] = NULL;
			complete(comp);
		}
		if (err) {
			int i;
			u32 *addr = (u32 *)mem->base +
					(idx * (SLIM_MSGQ_BUF_LEN >> 2));
			/* print the descriptor that resulted in error */
			for (i = 0; i < (SLIM_MSGQ_BUF_LEN >> 2); i++)
				SLIM_WARN(dev, "err desc[%d]:0x%x", i, addr[i]);
		}
		/* reclaim all packets that were delivered out of order */
		if (idx != dev->tx_head)
			SLIM_WARN(dev, "SLIM OUT OF ORDER TX:idx:%d, head:%d",
				idx, dev->tx_head);
		dev->tx_head = (dev->tx_head + 1) % MSM_TX_BUFS;
	}
}

static u32 *msm_slim_modify_tx_buf(struct msm_slim_ctrl *dev,
					struct completion *comp)
{
	struct msm_slim_endp *endpoint = &dev->tx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	u32 *retbuf = NULL;

	if ((dev->tx_tail + 1) % MSM_TX_BUFS == dev->tx_head)
		return NULL;

	retbuf = (u32 *)((u8 *)mem->base +
				(dev->tx_tail * SLIM_MSGQ_BUF_LEN));
	dev->wr_comp[dev->tx_tail] = comp;
	dev->tx_tail = (dev->tx_tail + 1) % MSM_TX_BUFS;
	return retbuf;
}
u32 *msm_slim_manage_tx_msgq(struct msm_slim_ctrl *dev, bool getbuf,
					struct completion *comp, int err)
{
	int ret = 0;
	int retries = 0;
	u32 *retbuf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&dev->tx_buf_lock, flags);
	if (!getbuf) {
		msm_slim_tx_msg_return(dev, err);
		spin_unlock_irqrestore(&dev->tx_buf_lock, flags);
		return NULL;
	}

	retbuf = msm_slim_modify_tx_buf(dev, comp);
	if (retbuf) {
		spin_unlock_irqrestore(&dev->tx_buf_lock, flags);
		return retbuf;
	}

	do {
		msm_slim_tx_msg_return(dev, err);
		retbuf = msm_slim_modify_tx_buf(dev, comp);
		if (!retbuf)
			ret = -EAGAIN;
		else {
			if (retries > 0)
				SLIM_INFO(dev, "SLIM TX retrieved:%d retries",
							retries);
			spin_unlock_irqrestore(&dev->tx_buf_lock, flags);
			return retbuf;
		}

		/*
		 * superframe size will vary based on clock gear
		 * 1 superframe will consume at least 1 message
		 * if HW is in good condition. With MX_RETRIES,
		 * make sure we wait for ~2 superframes
		 * before deciding HW couldn't process descriptors
		 */
		udelay(50);
		retries++;
	} while (ret && (retries < INIT_MX_RETRIES));

	spin_unlock_irqrestore(&dev->tx_buf_lock, flags);
	return NULL;
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

u32 *msm_get_msg_buf(struct msm_slim_ctrl *dev, int len,
			struct completion *comp)
{
	/*
	 * Currently we block a transaction until the current one completes.
	 * In case we need multiple transactions, use message Q
	 */
	if (dev->use_tx_msgqs != MSM_MSGQ_ENABLED) {
		dev->wr_comp[0] = comp;
		return dev->tx_buf;
	}

	return msm_slim_manage_tx_msgq(dev, true, comp, 0);
}

static void
msm_slim_rx_msgq_event(struct msm_slim_ctrl *dev, struct sps_event_notify *ev)
{
	if (ev->event_id == SPS_EVENT_DESC_DONE)
		complete(&dev->rx_msgq_notify);
	else
		dev_err(dev->dev, "%s: unknown event %d\n",
					__func__, ev->event_id);
}

static void
msm_slim_handle_rx(struct msm_slim_ctrl *dev, struct sps_event_notify *ev)
{
	int ret = 0;
	u32 mc = 0;
	u32 mt = 0;
	u8 msg_len = 0;

	if (ev->event_id != SPS_EVENT_EOT) {
		dev_err(dev->dev, "%s: unknown event %d\n",
					__func__, ev->event_id);
		return;
	}

	do {
		ret = msm_slim_rx_msgq_get(dev, dev->current_rx_buf,
					   dev->current_count);
		if (ret == -ENODATA) {
			return;
		} else if (ret) {
			SLIM_ERR(dev, "rx_msgq_get() failed 0x%x\n",
								ret);
			return;
		}

		/* Traverse first byte of message for message length */
		if (dev->current_count++ == 0) {
			msg_len = *(dev->current_rx_buf) & 0x1F;
			mt = (*(dev->current_rx_buf) >> 5) & 0x7;
			mc = (*(dev->current_rx_buf) >> 8) & 0xff;
			dev_dbg(dev->dev, "MC: %x, MT: %x\n", mc, mt);
		}

		msg_len = (msg_len < 4) ? 0 : (msg_len - 4);

		if (!msg_len) {
			dev->rx_slim(dev, (u8 *)dev->current_rx_buf);
			dev->current_count = 0;
		}

	} while (1);
}

static void msm_slim_rx_msgq_cb(struct sps_event_notify *notify)
{
	struct msm_slim_ctrl *dev = (struct msm_slim_ctrl *)notify->user;
	/* is this manager controller or NGD controller? */
	if (dev->ctrl.wakeup)
		msm_slim_rx_msgq_event(dev, notify);
	else
		msm_slim_handle_rx(dev, notify);
}

/* Queue up Rx message buffer */
static int msm_slim_post_rx_msgq(struct msm_slim_ctrl *dev, int ix)
{
	int ret;
	struct msm_slim_endp *endpoint = &dev->rx_msgq;
	struct sps_mem_buffer *mem = &endpoint->buf;
	struct sps_pipe *pipe = endpoint->sps;

	/* Rx message queue buffers are 4 bytes in length */
	u8 *virt_addr = mem->base + (4 * ix);
	phys_addr_t phys_addr = mem->phys_base + (4 * ix);

	ret = sps_transfer_one(pipe, phys_addr, 4, virt_addr, 0);
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
	phys_addr_t addr;
	int index;
	int ret;

	ret = sps_get_iovec(pipe, &iovec);
	if (ret) {
		dev_err(dev->dev, "sps_get_iovec() failed 0x%x\n", ret);
		goto err_exit;
	}

	addr = DESC_FULL_ADDR(iovec.flags, iovec.addr);
	pr_debug("iovec = (0x%x 0x%x 0x%x)\n",
		iovec.addr, iovec.size, iovec.flags);

	/* no more descriptors */
	if (!ret && (iovec.addr == 0) && (iovec.size == 0)) {
		ret = -ENODATA;
		goto err_exit;
	}

	/* Calculate buffer index */
	index = (addr - mem->phys_base) / 4;
	*(data + offset) = *((u32 *)mem->base + index);

	pr_debug("buf = 0x%p, data = 0x%x\n", (u32 *)mem->base + index, *data);

	/* Add buffer back to the queue */
	(void)msm_slim_post_rx_msgq(dev, index);

err_exit:
	return ret;
}

int msm_slim_connect_endp(struct msm_slim_ctrl *dev,
				struct msm_slim_endp *endpoint)
{
	int i, ret;
	struct sps_register_event sps_error_event; /* SPS_ERROR */
	struct sps_register_event sps_descr_event; /* DESCR_DONE */
	struct sps_connect *config = &endpoint->config;
	unsigned long flags;

	ret = sps_connect(endpoint->sps, config);
	if (ret) {
		dev_err(dev->dev, "sps_connect failed 0x%x\n", ret);
		return ret;
	}

	memset(&sps_descr_event, 0x00, sizeof(sps_descr_event));

	if (endpoint == &dev->rx_msgq) {
		sps_descr_event.mode = SPS_TRIGGER_CALLBACK;
		sps_descr_event.options = SPS_O_EOT;
		sps_descr_event.user = (void *)dev;
		sps_descr_event.callback = msm_slim_rx_msgq_cb;
		sps_descr_event.xfer_done = NULL;

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
		spin_lock_irqsave(&dev->tx_buf_lock, flags);
		dev->tx_tail = 0;
		dev->tx_head = 0;
		for (i = 0; i < MSM_TX_BUFS; i++)
			dev->wr_comp[i] = NULL;
		spin_unlock_irqrestore(&dev->tx_buf_lock, flags);
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

	if (dev->use_rx_msgqs == MSM_MSGQ_DISABLED)
		return 0;

	/* Allocate the endpoint */
	ret = msm_slim_init_endpoint(dev, endpoint);
	if (ret) {
		dev_err(dev->dev, "init_endpoint failed 0x%x\n", ret);
		goto sps_init_endpoint_failed;
	}

	/* Get the pipe indices for the message queues */
	pipe_offset = (readl_relaxed(dev->base + pipe_reg) & P_OFF_MASK) >> 2;
	dev_dbg(dev->dev, "Message queue pipe offset %d\n", pipe_offset);

	config->mode = SPS_MODE_SRC;
	config->source = dev->bam.hdl;
	config->destination = SPS_DEV_HANDLE_MEM;
	config->src_pipe_index = pipe_offset;
	config->options = SPS_O_EOT | SPS_O_ERROR |
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

	ret = msm_slim_connect_endp(dev, endpoint);

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
	pipe_offset = (readl_relaxed(dev->base + pipe_reg) & P_OFF_MASK) >> 2;
	pipe_offset += 1;
	dev_dbg(dev->dev, "TX Message queue pipe offset %d\n", pipe_offset);

	config->mode = SPS_MODE_DEST;
	config->source = SPS_DEV_HANDLE_MEM;
	config->destination = dev->bam.hdl;
	config->dest_pipe_index = pipe_offset;
	config->src_pipe_index = 0;
	config->options = SPS_O_ERROR | SPS_O_NO_Q |
				SPS_O_ACK_TRANSFERS | SPS_O_AUTO_ENABLE;

	/* Desc and TX buf are circular queues */
	/* Allocate memory for the FIFO descriptors */
	ret = msm_slim_sps_mem_alloc(dev, descr,
				(MSM_TX_BUFS + 1) * sizeof(struct sps_iovec));
	if (ret) {
		dev_err(dev->dev, "unable to allocate SPS descriptors\n");
		goto alloc_descr_failed;
	}

	/* Allocate TX buffer from which descriptors are created */
	ret = msm_slim_sps_mem_alloc(dev, mem, ((MSM_TX_BUFS + 1) *
					SLIM_MSGQ_BUF_LEN));
	if (ret) {
		dev_err(dev->dev, "dma_alloc_coherent failed\n");
		goto alloc_buffer_failed;
	}
	ret = msm_slim_connect_endp(dev, endpoint);

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

static int msm_slim_data_port_assign(struct msm_slim_ctrl *dev)
{
	int i, data_ports = 0;
	/* First 7 bits are for message Qs */
	for (i = 7; i < 32; i++) {
		/* Check what pipes are owned by Apps. */
		if ((dev->pdata.apps_pipes >> i) & 0x1) {
			if (dev->pipes)
				dev->pipes[data_ports].port_b = i - 7;
			data_ports++;
		}
	}
	return data_ports;
}
/* Registers BAM h/w resource with SPS driver and initializes msgq endpoints */
int msm_slim_sps_init(struct msm_slim_ctrl *dev, struct resource *bam_mem,
			u32 pipe_reg, bool remote)
{
	int ret;
	unsigned long bam_handle;
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
		goto init_pipes;
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

	/* Register the BAM device with the SPS driver */
	ret = sps_register_bam_device(&bam_props, &bam_handle);
	if (ret) {
		dev_err(dev->dev, "disabling BAM: reg-bam failed 0x%x\n", ret);
		dev->use_rx_msgqs = MSM_MSGQ_DISABLED;
		dev->use_tx_msgqs = MSM_MSGQ_DISABLED;
		return ret;
	}
	dev->bam.hdl = bam_handle;
	dev_dbg(dev->dev, "SLIM BAM registered, handle = 0x%lx\n", bam_handle);

init_pipes:
	if (dev->port_nums)
		goto init_msgq;

	/* get the # of ports first */
	dev->port_nums = msm_slim_data_port_assign(dev);
	if (dev->port_nums && !dev->pipes) {
		dev->pipes = kzalloc(sizeof(struct msm_slim_endp) *
					dev->port_nums,
					GFP_KERNEL);
		if (IS_ERR_OR_NULL(dev->pipes)) {
			dev_err(dev->dev, "no memory for data ports");
			sps_deregister_bam_device(bam_handle);
			return PTR_ERR(dev->pipes);
		}
		/* assign the ports now */
		msm_slim_data_port_assign(dev);
	}

init_msgq:
	ret = msm_slim_iommu_attach(dev);
	if (ret) {
		sps_deregister_bam_device(bam_handle);
		return ret;
	}

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

	/*
	 * If command interface for BAM fails, register interface is used for
	 * commands.
	 * It is possible that other BAM usecases (e.g. apps channels) will
	 * still need BAM. Since BAM is successfully initialized, we can
	 * continue using it for non-command use cases.
	 */

	return 0;
}

void msm_slim_disconnect_endp(struct msm_slim_ctrl *dev,
					struct msm_slim_endp *endpoint,
					enum msm_slim_msgq *msgq_flag)
{
	if (*msgq_flag >= MSM_MSGQ_ENABLED) {
		sps_disconnect(endpoint->sps);
		*msgq_flag = MSM_MSGQ_RESET;
	}
}

static int msm_slim_discard_rx_data(struct msm_slim_ctrl *dev,
					struct msm_slim_endp *endpoint)
{
	struct sps_iovec sio;
	int desc_num = 0, ret = 0;

	ret = sps_get_unused_desc_num(endpoint->sps, &desc_num);
	if (ret) {
		dev_err(dev->dev, "sps_get_iovec() failed 0x%x\n", ret);
		return ret;
	}
	while (desc_num--)
		sps_get_iovec(endpoint->sps, &sio);
	return ret;
}

static void msm_slim_remove_ep(struct msm_slim_ctrl *dev,
					struct msm_slim_endp *endpoint,
					enum msm_slim_msgq *msgq_flag)
{
	struct sps_connect *config = &endpoint->config;
	struct sps_mem_buffer *descr = &config->desc;
	struct sps_mem_buffer *mem = &endpoint->buf;

	msm_slim_sps_mem_free(dev, mem);
	msm_slim_sps_mem_free(dev, descr);
	msm_slim_free_endpoint(endpoint);
}

void msm_slim_deinit_ep(struct msm_slim_ctrl *dev,
				struct msm_slim_endp *endpoint,
				enum msm_slim_msgq *msgq_flag)
{
	int ret = 0;
	struct sps_connect *config = &endpoint->config;

	if (*msgq_flag == MSM_MSGQ_ENABLED) {
		if (config->mode == SPS_MODE_SRC) {
			ret = msm_slim_discard_rx_data(dev, endpoint);
			if (ret)
				SLIM_WARN(dev, "discarding Rx data failed\n");
		}
		msm_slim_disconnect_endp(dev, endpoint, msgq_flag);
		msm_slim_remove_ep(dev, endpoint, msgq_flag);
	}
}

static void msm_slim_sps_unreg_event(struct sps_pipe *sps)
{
	struct sps_register_event sps_event;

	memset(&sps_event, 0x00, sizeof(sps_event));
	/* Disable interrupt and signal notification for Rx/Tx pipe */
	sps_register_event(sps, &sps_event);
}

void msm_slim_sps_exit(struct msm_slim_ctrl *dev, bool dereg)
{
	int i;

	if (dev->use_rx_msgqs >= MSM_MSGQ_ENABLED)
		msm_slim_sps_unreg_event(dev->rx_msgq.sps);
	if (dev->use_tx_msgqs >= MSM_MSGQ_ENABLED)
		msm_slim_sps_unreg_event(dev->tx_msgq.sps);

	for (i = 0; i < dev->port_nums; i++) {
		if (dev->pipes[i].connected)
			msm_slim_disconn_pipe_port(dev, i);
	}

	if (dereg) {
		for (i = 0; i < dev->port_nums; i++) {
			if (dev->pipes[i].connected)
				msm_dealloc_port(&dev->ctrl, i);
		}
		sps_deregister_bam_device(dev->bam.hdl);
		dev->bam.hdl = 0L;
		kfree(dev->pipes);
		dev->pipes = NULL;
	}
	dev->port_nums = 0;
}

/* Slimbus QMI Messaging */
#define SLIMBUS_QMI_SELECT_INSTANCE_REQ_V01 0x0020
#define SLIMBUS_QMI_SELECT_INSTANCE_RESP_V01 0x0020
#define SLIMBUS_QMI_POWER_REQ_V01 0x0021
#define SLIMBUS_QMI_POWER_RESP_V01 0x0021
#define SLIMBUS_QMI_CHECK_FRAMER_STATUS_REQ 0x0022
#define SLIMBUS_QMI_CHECK_FRAMER_STATUS_RESP 0x0022
#define SLIMBUS_QMI_DEFERRED_STATUS_REQ 0x0023
#define SLIMBUS_QMI_DEFERRED_STATUS_RESP 0x0023

#define SLIMBUS_QMI_POWER_REQ_MAX_MSG_LEN 14
#define SLIMBUS_QMI_POWER_RESP_MAX_MSG_LEN 7
#define SLIMBUS_QMI_SELECT_INSTANCE_REQ_MAX_MSG_LEN 14
#define SLIMBUS_QMI_SELECT_INSTANCE_RESP_MAX_MSG_LEN 7
#define SLIMBUS_QMI_CHECK_FRAMER_STAT_RESP_MAX_MSG_LEN 7
#define SLIMBUS_QMI_DEFERRED_STATUS_REQ_MSG_MAX_MSG_LEN 0
#define SLIMBUS_QMI_DEFERRED_STATUS_RESP_STAT_MSG_MAX_MSG_LEN 7

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

enum slimbus_resp_enum_type_v01 {
	SLIMBUS_RESP_ENUM_TYPE_MIN_VAL_V01 = INT_MIN,
	SLIMBUS_RESP_SYNCHRONOUS_V01 = 1,
	SLIMBUS_RESP_DEFERRED_V01 = 2,
	SLIMBUS_RESP_ENUM_TYPE_MAX_VAL_V01 = INT_MAX,
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

	/* Optional */
	/* Optional Deferred Response type Operation */
	/* Must be set to true if type is being passed */
	uint8_t resp_type_valid;
	enum slimbus_resp_enum_type_v01 resp_type;
};

struct slimbus_power_resp_msg_v01 {
	/* Mandatory */
	/* Result Code */
	struct qmi_response_type_v01 resp;
};

struct slimbus_chkfrm_resp_msg {
	/* Mandatory */
	/* Result Code */
	struct qmi_response_type_v01 resp;
};

struct slimbus_deferred_status_resp {
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
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct slimbus_power_req_msg_v01,
					   resp_type_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum slimbus_resp_enum_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct slimbus_power_req_msg_v01,
					   resp_type),
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

static struct elem_info slimbus_chkfrm_resp_msg_v01_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct slimbus_chkfrm_resp_msg, resp),
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

static struct elem_info slimbus_deferred_status_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct slimbus_deferred_status_resp,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
	},
};
static void msm_slim_qmi_recv_msg(struct kthread_work *work)
{
	int rc;
	struct msm_slim_qmi *qmi =
			container_of(work, struct msm_slim_qmi, kwork);

	/* Drain all packets received */
	do {
		rc = qmi_recv_msg(qmi->handle);
	} while (rc == 0);
	if (rc != -ENOMSG)
		pr_err("%s: Error receiving QMI message:%d\n", __func__, rc);
}

static void msm_slim_qmi_notify(struct qmi_handle *handle,
				enum qmi_event_type event, void *notify_priv)
{
	struct msm_slim_ctrl *dev = notify_priv;
	struct msm_slim_qmi *qmi = &dev->qmi;

	switch (event) {
	case QMI_RECV_MSG:
		kthread_queue_work(&qmi->kworker, &qmi->kwork);
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
			&resp_desc, &resp, sizeof(resp), SLIM_QMI_RESP_TOUT);
	if (rc < 0) {
		SLIM_ERR(dev, "%s: QMI send req failed %d\n", __func__, rc);
		return rc;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		SLIM_ERR(dev, "%s: QMI request failed 0x%x (%s)\n", __func__,
				resp.resp.result, get_qmi_error(&resp.resp));
		return -EREMOTEIO;
	}

	return 0;
}

static void slim_qmi_resp_cb(struct qmi_handle *handle, unsigned int msg_id,
			     void *msg, void *resp_cb_data, int stat)
{
	struct slimbus_power_resp_msg_v01 *resp = msg;
	struct msm_slim_ctrl *dev = resp_cb_data;

	if (msg_id != SLIMBUS_QMI_POWER_RESP_V01)
		SLIM_WARN(dev, "incorrect msg id in qmi-resp CB:0x%x", msg_id);
	else if (resp->resp.result != QMI_RESULT_SUCCESS_V01)
		SLIM_ERR(dev, "%s: QMI power failed 0x%x (%s)\n", __func__,
			 resp->resp.result, get_qmi_error(&resp->resp));

	complete(&dev->qmi.defer_comp);
}

static int msm_slim_qmi_send_power_request(struct msm_slim_ctrl *dev,
				struct slimbus_power_req_msg_v01 *req)
{
	struct slimbus_power_resp_msg_v01 *resp =
		(struct slimbus_power_resp_msg_v01 *)&dev->qmi.resp;
	struct msg_desc req_desc;
	struct msg_desc *resp_desc = &dev->qmi.resp_desc;
	int rc;

	req_desc.msg_id = SLIMBUS_QMI_POWER_REQ_V01;
	req_desc.max_msg_len = SLIMBUS_QMI_POWER_REQ_MAX_MSG_LEN;
	req_desc.ei_array = slimbus_power_req_msg_v01_ei;

	resp_desc->msg_id = SLIMBUS_QMI_POWER_RESP_V01;
	resp_desc->max_msg_len = SLIMBUS_QMI_POWER_RESP_MAX_MSG_LEN;
	resp_desc->ei_array = slimbus_power_resp_msg_v01_ei;

	if (dev->qmi.deferred_resp)
		rc = qmi_send_req_nowait(dev->qmi.handle, &req_desc, req,
				       sizeof(*req), resp_desc, resp,
				       sizeof(*resp), slim_qmi_resp_cb, dev);
	else
		rc = qmi_send_req_wait(dev->qmi.handle, &req_desc, req,
				       sizeof(*req), resp_desc, resp,
				       sizeof(*resp), SLIM_QMI_RESP_TOUT);
	if (rc < 0)
		SLIM_ERR(dev, "%s: QMI send req failed %d\n", __func__, rc);

	if (rc < 0 || dev->qmi.deferred_resp)
		return rc;

	/* Check the response */
	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		SLIM_ERR(dev, "%s: QMI request failed 0x%x (%s)\n", __func__,
				resp->resp.result, get_qmi_error(&resp->resp));
		return -EREMOTEIO;
	}

	return 0;
}

int msm_slim_qmi_init(struct msm_slim_ctrl *dev, bool apps_is_master)
{
	int rc = 0;
	struct qmi_handle *handle;
	struct slimbus_select_inst_req_msg_v01 req;

	if (dev->qmi.handle || dev->qmi.task) {
		pr_err("%s: Destroying stale QMI client handle\n", __func__);
		msm_slim_qmi_exit(dev);
	}

	kthread_init_worker(&dev->qmi.kworker);
	init_completion(&dev->qmi.defer_comp);

	dev->qmi.task = kthread_run(kthread_worker_fn,
			&dev->qmi.kworker, "msm_slim_qmi_clnt%d", dev->ctrl.nr);

	if (IS_ERR(dev->qmi.task)) {
		pr_err("%s: Failed to create QMI client kthread\n", __func__);
		return -ENOMEM;
	}

	kthread_init_work(&dev->qmi.kwork, msm_slim_qmi_recv_msg);

	handle = qmi_handle_create(msm_slim_qmi_notify, dev);
	if (!handle) {
		rc = -ENOMEM;
		pr_err("%s: QMI client handle alloc failed\n", __func__);
		goto qmi_handle_create_failed;
	}

	rc = qmi_connect_to_service(handle, SLIMBUS_QMI_SVC_ID,
						SLIMBUS_QMI_SVC_V1,
						SLIMBUS_QMI_INS_ID);
	if (rc < 0) {
		SLIM_ERR(dev, "%s: QMI server not found\n", __func__);
		goto qmi_connect_to_service_failed;
	}

	/* Instance is 0 based */
	req.instance = (dev->ctrl.nr >> 1);
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
	kthread_flush_worker(&dev->qmi.kworker);
	kthread_stop(dev->qmi.task);
	dev->qmi.task = NULL;
	return rc;
}

void msm_slim_qmi_exit(struct msm_slim_ctrl *dev)
{
	if (!dev->qmi.handle || !dev->qmi.task)
		return;
	qmi_handle_destroy(dev->qmi.handle);
	kthread_flush_worker(&dev->qmi.kworker);
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

	if (dev->qmi.deferred_resp) {
		req.resp_type = SLIMBUS_RESP_DEFERRED_V01;
		req.resp_type_valid = 1;
	} else {
		req.resp_type_valid = 0;
	}

	return msm_slim_qmi_send_power_request(dev, &req);
}

int msm_slim_qmi_check_framer_request(struct msm_slim_ctrl *dev)
{
	struct slimbus_chkfrm_resp_msg resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.msg_id = SLIMBUS_QMI_CHECK_FRAMER_STATUS_REQ;
	req_desc.max_msg_len = 0;
	req_desc.ei_array = NULL;

	resp_desc.msg_id = SLIMBUS_QMI_CHECK_FRAMER_STATUS_RESP;
	resp_desc.max_msg_len = SLIMBUS_QMI_CHECK_FRAMER_STAT_RESP_MAX_MSG_LEN;
	resp_desc.ei_array = slimbus_chkfrm_resp_msg_v01_ei;

	rc = qmi_send_req_wait(dev->qmi.handle, &req_desc, NULL, 0,
		&resp_desc, &resp, sizeof(resp), SLIM_QMI_RESP_TOUT);
	if (rc < 0) {
		SLIM_ERR(dev, "%s: QMI send req failed %d\n", __func__, rc);
		return rc;
	}
	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		SLIM_ERR(dev, "%s: QMI request failed 0x%x (%s)\n",
			__func__, resp.resp.result, get_qmi_error(&resp.resp));
		return -EREMOTEIO;
	}
	return 0;
}

int msm_slim_qmi_deferred_status_req(struct msm_slim_ctrl *dev)
{
	struct slimbus_deferred_status_resp resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int rc;

	req_desc.msg_id = SLIMBUS_QMI_DEFERRED_STATUS_REQ;
	req_desc.max_msg_len = 0;
	req_desc.ei_array = NULL;

	resp_desc.msg_id = SLIMBUS_QMI_DEFERRED_STATUS_RESP;
	resp_desc.max_msg_len =
		SLIMBUS_QMI_DEFERRED_STATUS_RESP_STAT_MSG_MAX_MSG_LEN;
	resp_desc.ei_array = slimbus_deferred_status_resp_msg_v01_ei;

	rc = qmi_send_req_wait(dev->qmi.handle, &req_desc, NULL, 0,
		&resp_desc, &resp, sizeof(resp), SLIM_QMI_RESP_TOUT);
	if (rc < 0) {
		SLIM_ERR(dev, "%s: QMI send req failed %d\n", __func__, rc);
		return rc;
	}
	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		SLIM_ERR(dev, "%s: QMI request failed 0x%x (%s)\n",
			__func__, resp.resp.result, get_qmi_error(&resp.resp));
		return -EREMOTEIO;
	}

	/* wait for the deferred response */
	rc = wait_for_completion_timeout(&dev->qmi.defer_comp, HZ);
	if (rc == 0) {
		SLIM_WARN(dev, "slimbus power deferred response not rcvd\n");
		return -ETIMEDOUT;
	}
	/* Check what response we got in callback */
	if (dev->qmi.resp.result != QMI_RESULT_SUCCESS_V01) {
		SLIM_WARN(dev, "QMI power req failed in CB");
		return -EREMOTEIO;
	}

	return 0;
}
