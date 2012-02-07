/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slimbus/slimbus.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <mach/sps.h>

/* Per spec.max 40 bytes per received message */
#define SLIM_RX_MSGQ_BUF_LEN	40

#define SLIM_USR_MC_GENERIC_ACK		0x25
#define SLIM_USR_MC_MASTER_CAPABILITY	0x0
#define SLIM_USR_MC_REPORT_SATELLITE	0x1
#define SLIM_USR_MC_ADDR_QUERY		0xD
#define SLIM_USR_MC_ADDR_REPLY		0xE
#define SLIM_USR_MC_DEFINE_CHAN		0x20
#define SLIM_USR_MC_DEF_ACT_CHAN	0x21
#define SLIM_USR_MC_CHAN_CTRL		0x23
#define SLIM_USR_MC_RECONFIG_NOW	0x24
#define SLIM_USR_MC_REQ_BW		0x28
#define SLIM_USR_MC_CONNECT_SRC		0x2C
#define SLIM_USR_MC_CONNECT_SINK	0x2D
#define SLIM_USR_MC_DISCONNECT_PORT	0x2E

/* MSM Slimbus peripheral settings */
#define MSM_SLIM_PERF_SUMM_THRESHOLD	0x8000
#define MSM_SLIM_NCHANS			32
#define MSM_SLIM_NPORTS			24
#define MSM_SLIM_AUTOSUSPEND		MSEC_PER_SEC

/*
 * Need enough descriptors to receive present messages from slaves
 * if received simultaneously. Present message needs 3 descriptors
 * and this size will ensure around 10 simultaneous reports.
 */
#define MSM_SLIM_DESC_NUM		32

#define SLIM_MSG_ASM_FIRST_WORD(l, mt, mc, dt, ad) \
		((l) | ((mt) << 5) | ((mc) << 8) | ((dt) << 15) | ((ad) << 16))

#define MSM_SLIM_NAME	"msm_slim_ctrl"
#define SLIM_ROOT_FREQ 24576000

#define MSM_CONCUR_MSG	8
#define SAT_CONCUR_MSG	8
#define DEF_WATERMARK	(8 << 1)
#define DEF_ALIGN	0
#define DEF_PACK	(1 << 6)
#define ENABLE_PORT	1

#define DEF_BLKSZ	0
#define DEF_TRANSZ	0

#define SAT_MAGIC_LSB	0xD9
#define SAT_MAGIC_MSB	0xC5
#define SAT_MSG_VER	0x1
#define SAT_MSG_PROT	0x1
#define MSM_SAT_SUCCSS	0x20

#define QC_MFGID_LSB	0x2
#define QC_MFGID_MSB	0x17
#define QC_CHIPID_SL	0x10
#define QC_DEVID_SAT1	0x3
#define QC_DEVID_SAT2	0x4
#define QC_DEVID_PGD	0x5
#define QC_MSM_DEVS	5

/* Component registers */
enum comp_reg {
	COMP_CFG	= 0,
	COMP_TRUST_CFG	= 0x14,
};

/* Manager registers */
enum mgr_reg {
	MGR_CFG		= 0x200,
	MGR_STATUS	= 0x204,
	MGR_RX_MSGQ_CFG	= 0x208,
	MGR_INT_EN	= 0x210,
	MGR_INT_STAT	= 0x214,
	MGR_INT_CLR	= 0x218,
	MGR_TX_MSG	= 0x230,
	MGR_RX_MSG	= 0x270,
	MGR_VE_STAT	= 0x300,
};

enum msg_cfg {
	MGR_CFG_ENABLE		= 1,
	MGR_CFG_RX_MSGQ_EN	= 1 << 1,
	MGR_CFG_TX_MSGQ_EN_HIGH	= 1 << 2,
	MGR_CFG_TX_MSGQ_EN_LOW	= 1 << 3,
};
/* Message queue types */
enum msm_slim_msgq_type {
	MSGQ_RX		= 0,
	MSGQ_TX_LOW	= 1,
	MSGQ_TX_HIGH	= 2,
};
/* Framer registers */
enum frm_reg {
	FRM_CFG		= 0x400,
	FRM_STAT	= 0x404,
	FRM_INT_EN	= 0x410,
	FRM_INT_STAT	= 0x414,
	FRM_INT_CLR	= 0x418,
	FRM_WAKEUP	= 0x41C,
	FRM_CLKCTL_DONE	= 0x420,
	FRM_IE_STAT	= 0x430,
	FRM_VE_STAT	= 0x440,
};

/* Interface registers */
enum intf_reg {
	INTF_CFG	= 0x600,
	INTF_STAT	= 0x604,
	INTF_INT_EN	= 0x610,
	INTF_INT_STAT	= 0x614,
	INTF_INT_CLR	= 0x618,
	INTF_IE_STAT	= 0x630,
	INTF_VE_STAT	= 0x640,
};

/* Manager PGD registers */
enum pgd_reg {
	PGD_CFG			= 0x1000,
	PGD_STAT		= 0x1004,
	PGD_INT_EN		= 0x1010,
	PGD_INT_STAT		= 0x1014,
	PGD_INT_CLR		= 0x1018,
	PGD_OWN_EEn		= 0x1020,
	PGD_PORT_INT_EN_EEn	= 0x1030,
	PGD_PORT_INT_ST_EEn	= 0x1034,
	PGD_PORT_INT_CL_EEn	= 0x1038,
	PGD_PORT_CFGn		= 0x1080,
	PGD_PORT_STATn		= 0x1084,
	PGD_PORT_PARAMn		= 0x1088,
	PGD_PORT_BLKn		= 0x108C,
	PGD_PORT_TRANn		= 0x1090,
	PGD_PORT_MCHANn		= 0x1094,
	PGD_PORT_PSHPLLn	= 0x1098,
	PGD_PORT_PC_CFGn	= 0x1600,
	PGD_PORT_PC_VALn	= 0x1604,
	PGD_PORT_PC_VFR_TSn	= 0x1608,
	PGD_PORT_PC_VFR_STn	= 0x160C,
	PGD_PORT_PC_VFR_CLn	= 0x1610,
	PGD_IE_STAT		= 0x1700,
	PGD_VE_STAT		= 0x1710,
};

enum rsc_grp {
	EE_MGR_RSC_GRP	= 1 << 10,
	EE_NGD_2	= 2 << 6,
	EE_NGD_1	= 0,
};

enum mgr_intr {
	MGR_INT_RECFG_DONE	= 1 << 24,
	MGR_INT_TX_NACKED_2	= 1 << 25,
	MGR_INT_MSG_BUF_CONTE	= 1 << 26,
	MGR_INT_RX_MSG_RCVD	= 1 << 30,
	MGR_INT_TX_MSG_SENT	= 1 << 31,
};

enum frm_cfg {
	FRM_ACTIVE	= 1,
	CLK_GEAR	= 7,
	ROOT_FREQ	= 11,
	REF_CLK_GEAR	= 15,
};

enum msm_ctrl_state {
	MSM_CTRL_AWAKE,
	MSM_CTRL_SLEEPING,
	MSM_CTRL_ASLEEP,
};

struct msm_slim_sps_bam {
	u32			hdl;
	void __iomem		*base;
	int			irq;
};

struct msm_slim_endp {
	struct sps_pipe			*sps;
	struct sps_connect		config;
	struct sps_register_event	event;
	struct sps_mem_buffer		buf;
	struct completion		*xcomp;
	bool				connected;
};

struct msm_slim_ctrl {
	struct slim_controller  ctrl;
	struct slim_framer	framer;
	struct device		*dev;
	void __iomem		*base;
	struct resource		*slew_mem;
	u32			curr_bw;
	u8			msg_cnt;
	u32			tx_buf[10];
	u8			rx_msgs[MSM_CONCUR_MSG][SLIM_RX_MSGQ_BUF_LEN];
	spinlock_t		rx_lock;
	int			head;
	int			tail;
	int			irq;
	int			err;
	int			ee;
	struct completion	*wr_comp;
	struct msm_slim_sat	*satd;
	struct msm_slim_endp	pipes[7];
	struct msm_slim_sps_bam	bam;
	struct msm_slim_endp	rx_msgq;
	struct completion	rx_msgq_notify;
	struct task_struct	*rx_msgq_thread;
	struct clk		*rclk;
	struct mutex		tx_lock;
	u8			pgdla;
	bool			use_rx_msgqs;
	int			pipe_b;
	struct completion	reconf;
	bool			reconf_busy;
	bool			chan_active;
	enum msm_ctrl_state	state;
};

struct msm_slim_sat {
	struct slim_device	satcl;
	struct msm_slim_ctrl	*dev;
	struct workqueue_struct *wq;
	struct work_struct	wd;
	u8			sat_msgs[SAT_CONCUR_MSG][40];
	u16			*satch;
	u8			nsatch;
	bool			sent_capability;
	bool			pending_reconf;
	bool			pending_capability;
	int			shead;
	int			stail;
	spinlock_t lock;
};

static int msm_slim_rx_enqueue(struct msm_slim_ctrl *dev, u32 *buf, u8 len)
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

static int msm_slim_rx_dequeue(struct msm_slim_ctrl *dev, u8 *buf)
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

static int msm_sat_enqueue(struct msm_slim_sat *sat, u32 *buf, u8 len)
{
	struct msm_slim_ctrl *dev = sat->dev;
	spin_lock(&sat->lock);
	if ((sat->stail + 1) % SAT_CONCUR_MSG == sat->shead) {
		spin_unlock(&sat->lock);
		dev_err(dev->dev, "SAT QUEUE full!");
		return -EXFULL;
	}
	memcpy(sat->sat_msgs[sat->stail], (u8 *)buf, len);
	sat->stail = (sat->stail + 1) % SAT_CONCUR_MSG;
	spin_unlock(&sat->lock);
	return 0;
}

static int msm_sat_dequeue(struct msm_slim_sat *sat, u8 *buf)
{
	unsigned long flags;
	spin_lock_irqsave(&sat->lock, flags);
	if (sat->stail == sat->shead) {
		spin_unlock_irqrestore(&sat->lock, flags);
		return -ENODATA;
	}
	memcpy(buf, sat->sat_msgs[sat->shead], 40);
	sat->shead = (sat->shead + 1) % SAT_CONCUR_MSG;
	spin_unlock_irqrestore(&sat->lock, flags);
	return 0;
}

static void msm_get_eaddr(u8 *e_addr, u32 *buffer)
{
	e_addr[0] = (buffer[1] >> 24) & 0xff;
	e_addr[1] = (buffer[1] >> 16) & 0xff;
	e_addr[2] = (buffer[1] >> 8) & 0xff;
	e_addr[3] = buffer[1] & 0xff;
	e_addr[4] = (buffer[0] >> 24) & 0xff;
	e_addr[5] = (buffer[0] >> 16) & 0xff;
}

static bool msm_is_sat_dev(u8 *e_addr)
{
	if (e_addr[5] == QC_MFGID_LSB && e_addr[4] == QC_MFGID_MSB &&
		e_addr[2] != QC_CHIPID_SL &&
		(e_addr[1] == QC_DEVID_SAT1 || e_addr[1] == QC_DEVID_SAT2))
		return true;
	return false;
}

static int msm_slim_get_ctrl(struct msm_slim_ctrl *dev)
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
static void msm_slim_put_ctrl(struct msm_slim_ctrl *dev)
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

static irqreturn_t msm_slim_interrupt(int irq, void *d)
{
	struct msm_slim_ctrl *dev = d;
	u32 pstat;
	u32 stat = readl_relaxed(dev->base + MGR_INT_STAT);

	if (stat & MGR_INT_TX_MSG_SENT || stat & MGR_INT_TX_NACKED_2) {
		if (stat & MGR_INT_TX_MSG_SENT)
			writel_relaxed(MGR_INT_TX_MSG_SENT,
					dev->base + MGR_INT_CLR);
		else {
			writel_relaxed(MGR_INT_TX_NACKED_2,
					dev->base + MGR_INT_CLR);
			dev->err = -EIO;
		}
		/*
		 * Guarantee that interrupt clear bit write goes through before
		 * signalling completion/exiting ISR
		 */
		mb();
		if (dev->wr_comp)
			complete(dev->wr_comp);
	}
	if (stat & MGR_INT_RX_MSG_RCVD) {
		u32 rx_buf[10];
		u32 mc, mt;
		u8 len, i;
		rx_buf[0] = readl_relaxed(dev->base + MGR_RX_MSG);
		len = rx_buf[0] & 0x1F;
		for (i = 1; i < ((len + 3) >> 2); i++) {
			rx_buf[i] = readl_relaxed(dev->base + MGR_RX_MSG +
						(4 * i));
			dev_dbg(dev->dev, "reading data: %x\n", rx_buf[i]);
		}
		mt = (rx_buf[0] >> 5) & 0x7;
		mc = (rx_buf[0] >> 8) & 0xff;
		dev_dbg(dev->dev, "MC: %x, MT: %x\n", mc, mt);
		if (mt == SLIM_MSG_MT_DEST_REFERRED_USER ||
				mt == SLIM_MSG_MT_SRC_REFERRED_USER) {
			struct msm_slim_sat *sat = dev->satd;
			msm_sat_enqueue(sat, rx_buf, len);
			writel_relaxed(MGR_INT_RX_MSG_RCVD,
					dev->base + MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through before
			 * queuing work
			 */
			mb();
			queue_work(sat->wq, &sat->wd);
		} else if (mt == SLIM_MSG_MT_CORE &&
			mc == SLIM_MSG_MC_REPORT_PRESENT) {
			u8 e_addr[6];
			msm_get_eaddr(e_addr, rx_buf);
			if (msm_is_sat_dev(e_addr)) {
				/*
				 * Consider possibility that this device may
				 * be reporting more than once?
				 */
				struct msm_slim_sat *sat = dev->satd;
				msm_sat_enqueue(sat, rx_buf, len);
				writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
							MGR_INT_CLR);
				/*
				 * Guarantee that CLR bit write goes through
				 * before queuing work
				 */
				mb();
				queue_work(sat->wq, &sat->wd);
			} else {
				msm_slim_rx_enqueue(dev, rx_buf, len);
				writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
							MGR_INT_CLR);
				/*
				 * Guarantee that CLR bit write goes through
				 * before signalling completion
				 */
				mb();
				complete(&dev->rx_msgq_notify);
			}
		} else if (mc == SLIM_MSG_MC_REPLY_INFORMATION ||
				mc == SLIM_MSG_MC_REPLY_VALUE) {
			msm_slim_rx_enqueue(dev, rx_buf, len);
			writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
						MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through
			 * before signalling completion
			 */
			mb();
			complete(&dev->rx_msgq_notify);
		} else if (mc == SLIM_MSG_MC_REPORT_INFORMATION) {
			u8 *buf = (u8 *)rx_buf;
			u8 l_addr = buf[2];
			u16 ele = (u16)buf[4] << 4;
			ele |= ((buf[3] & 0xf0) >> 4);
			dev_err(dev->dev, "Slim-dev:%d report inf element:0x%x",
					l_addr, ele);
			for (i = 0; i < len - 5; i++)
				dev_err(dev->dev, "offset:0x%x:bit mask:%x",
						i, buf[i+5]);
			writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
						MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through
			 * before exiting
			 */
			mb();
		} else {
			dev_err(dev->dev, "Unexpected MC,%x MT:%x, len:%d",
						mc, mt, len);
			for (i = 0; i < ((len + 3) >> 2); i++)
				dev_err(dev->dev, "error msg: %x", rx_buf[i]);
			writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
						MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through
			 * before exiting
			 */
			mb();
		}
	}
	if (stat & MGR_INT_RECFG_DONE) {
		writel_relaxed(MGR_INT_RECFG_DONE, dev->base + MGR_INT_CLR);
		/*
		 * Guarantee that CLR bit write goes through
		 * before exiting ISR
		 */
		mb();
		complete(&dev->reconf);
	}
	pstat = readl_relaxed(dev->base + PGD_PORT_INT_ST_EEn + (16 * dev->ee));
	if (pstat != 0) {
		int i = 0;
		for (i = dev->pipe_b; i < MSM_SLIM_NPORTS; i++) {
			if (pstat & 1 << i) {
				u32 val = readl_relaxed(dev->base +
						PGD_PORT_STATn + (i * 32));
				if (val & (1 << 19)) {
					dev->ctrl.ports[i].err =
						SLIM_P_DISCONNECT;
					dev->pipes[i-dev->pipe_b].connected =
							false;
					/*
					 * SPS will call completion since
					 * ERROR flags are registered
					 */
				} else if (val & (1 << 2))
					dev->ctrl.ports[i].err =
							SLIM_P_OVERFLOW;
				else if (val & (1 << 3))
					dev->ctrl.ports[i].err =
						SLIM_P_UNDERFLOW;
			}
			writel_relaxed(1, dev->base + PGD_PORT_INT_CL_EEn +
				(dev->ee * 16));
		}
		/*
		 * Guarantee that port interrupt bit(s) clearing writes go
		 * through before exiting ISR
		 */
		mb();
	}

	return IRQ_HANDLED;
}

static int
msm_slim_init_endpoint(struct msm_slim_ctrl *dev, struct msm_slim_endp *ep)
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

static void
msm_slim_free_endpoint(struct msm_slim_endp *ep)
{
	sps_free_endpoint(ep->sps);
	ep->sps = NULL;
}

static int msm_slim_sps_mem_alloc(
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

static void
msm_slim_sps_mem_free(struct msm_slim_ctrl *dev, struct sps_mem_buffer *mem)
{
	dma_free_coherent(dev->dev, mem->size, mem->base, mem->phys_base);
	mem->size = 0;
	mem->base = NULL;
	mem->phys_base = 0;
}

static void msm_hw_set_port(struct msm_slim_ctrl *dev, u8 pn)
{
	u32 set_cfg = DEF_WATERMARK | DEF_ALIGN | DEF_PACK | ENABLE_PORT;
	u32 int_port = readl_relaxed(dev->base + PGD_PORT_INT_EN_EEn +
					(dev->ee * 16));
	writel_relaxed(set_cfg, dev->base + PGD_PORT_CFGn + (pn * 32));
	writel_relaxed(DEF_BLKSZ, dev->base + PGD_PORT_BLKn + (pn * 32));
	writel_relaxed(DEF_TRANSZ, dev->base + PGD_PORT_TRANn + (pn * 32));
	writel_relaxed((int_port | 1 << pn) , dev->base + PGD_PORT_INT_EN_EEn +
			(dev->ee * 16));
	/* Make sure that port registers are updated before returning */
	mb();
}

static int msm_slim_connect_pipe_port(struct msm_slim_ctrl *dev, u8 pn)
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

	stat = readl_relaxed(dev->base + PGD_PORT_STATn +
				(32 * (pn + dev->pipe_b)));
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

static u32 *msm_get_msg_buf(struct slim_controller *ctrl, int len)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	/*
	 * Currently we block a transaction until the current one completes.
	 * In case we need multiple transactions, use message Q
	 */
	return dev->tx_buf;
}

static int msm_send_msg_buf(struct slim_controller *ctrl, u32 *buf, u8 len)
{
	int i;
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	for (i = 0; i < (len + 3) >> 2; i++) {
		dev_dbg(dev->dev, "TX data:0x%x\n", buf[i]);
		writel_relaxed(buf[i], dev->base + MGR_TX_MSG + (i * 4));
	}
	/* Guarantee that message is sent before returning */
	mb();
	return 0;
}

static int msm_xfer_msg(struct slim_controller *ctrl, struct slim_msg_txn *txn)
{
	DECLARE_COMPLETION_ONSTACK(done);
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	u32 *pbuf;
	u8 *puc;
	int timeout;
	int msgv = -1;
	u8 la = txn->la;
	u8 mc = (u8)(txn->mc & 0xFF);
	/*
	 * Voting for runtime PM: Slimbus has 2 possible use cases:
	 * 1. messaging
	 * 2. Data channels
	 * Messaging case goes through messaging slots and data channels
	 * use their own slots
	 * This "get" votes for messaging bandwidth
	 */
	if (!(txn->mc & SLIM_MSG_CLK_PAUSE_SEQ_FLG))
		msgv = msm_slim_get_ctrl(dev);
	mutex_lock(&dev->tx_lock);
	if (dev->state == MSM_CTRL_ASLEEP ||
		((!(txn->mc & SLIM_MSG_CLK_PAUSE_SEQ_FLG)) &&
		dev->state == MSM_CTRL_SLEEPING)) {
		dev_err(dev->dev, "runtime or system PM suspended state");
		mutex_unlock(&dev->tx_lock);
		if (msgv >= 0)
			msm_slim_put_ctrl(dev);
		return -EBUSY;
	}
	if (txn->mt == SLIM_MSG_MT_CORE &&
		mc == SLIM_MSG_MC_BEGIN_RECONFIGURATION) {
		if (dev->reconf_busy) {
			wait_for_completion(&dev->reconf);
			dev->reconf_busy = false;
		}
		/* This "get" votes for data channels */
		if (dev->ctrl.sched.usedslots != 0 &&
			!dev->chan_active) {
			int chv = msm_slim_get_ctrl(dev);
			if (chv >= 0)
				dev->chan_active = true;
		}
	}
	txn->rl--;
	pbuf = msm_get_msg_buf(ctrl, txn->rl);
	dev->wr_comp = NULL;
	dev->err = 0;

	if (txn->dt == SLIM_MSG_DEST_ENUMADDR) {
		mutex_unlock(&dev->tx_lock);
		if (msgv >= 0)
			msm_slim_put_ctrl(dev);
		return -EPROTONOSUPPORT;
	}
	if (txn->mt == SLIM_MSG_MT_CORE && txn->la == 0xFF &&
		(mc == SLIM_MSG_MC_CONNECT_SOURCE ||
		 mc == SLIM_MSG_MC_CONNECT_SINK ||
		 mc == SLIM_MSG_MC_DISCONNECT_PORT))
		la = dev->pgdla;
	if (txn->dt == SLIM_MSG_DEST_LOGICALADDR)
		*pbuf = SLIM_MSG_ASM_FIRST_WORD(txn->rl, txn->mt, mc, 0, la);
	else
		*pbuf = SLIM_MSG_ASM_FIRST_WORD(txn->rl, txn->mt, mc, 1, la);
	if (txn->dt == SLIM_MSG_DEST_LOGICALADDR)
		puc = ((u8 *)pbuf) + 3;
	else
		puc = ((u8 *)pbuf) + 2;
	if (txn->rbuf)
		*(puc++) = txn->tid;
	if ((txn->mt == SLIM_MSG_MT_CORE) &&
		((mc >= SLIM_MSG_MC_REQUEST_INFORMATION &&
		mc <= SLIM_MSG_MC_REPORT_INFORMATION) ||
		(mc >= SLIM_MSG_MC_REQUEST_VALUE &&
		 mc <= SLIM_MSG_MC_CHANGE_VALUE))) {
		*(puc++) = (txn->ec & 0xFF);
		*(puc++) = (txn->ec >> 8)&0xFF;
	}
	if (txn->wbuf)
		memcpy(puc, txn->wbuf, txn->len);
	if (txn->mt == SLIM_MSG_MT_CORE && txn->la == 0xFF &&
		(mc == SLIM_MSG_MC_CONNECT_SOURCE ||
		 mc == SLIM_MSG_MC_CONNECT_SINK ||
		 mc == SLIM_MSG_MC_DISCONNECT_PORT)) {
		if (mc != SLIM_MSG_MC_DISCONNECT_PORT)
			dev->err = msm_slim_connect_pipe_port(dev, *puc);
		else {
			struct msm_slim_endp *endpoint = &dev->pipes[*puc];
			struct sps_register_event sps_event;
			memset(&sps_event, 0, sizeof(sps_event));
			sps_register_event(endpoint->sps, &sps_event);
			sps_disconnect(endpoint->sps);
			/*
			 * Remove channel disconnects master-side ports from
			 * channel. No need to send that again on the bus
			 */
			dev->pipes[*puc].connected = false;
			mutex_unlock(&dev->tx_lock);
			if (msgv >= 0)
				msm_slim_put_ctrl(dev);
			return 0;
		}
		if (dev->err) {
			dev_err(dev->dev, "pipe-port connect err:%d", dev->err);
			mutex_unlock(&dev->tx_lock);
			if (msgv >= 0)
				msm_slim_put_ctrl(dev);
			return dev->err;
		}
		*(puc) = *(puc) + dev->pipe_b;
	}
	if (txn->mt == SLIM_MSG_MT_CORE &&
		mc == SLIM_MSG_MC_BEGIN_RECONFIGURATION)
		dev->reconf_busy = true;
	dev->wr_comp = &done;
	msm_send_msg_buf(ctrl, pbuf, txn->rl);
	timeout = wait_for_completion_timeout(&done, HZ);

	if (mc == SLIM_MSG_MC_RECONFIGURE_NOW) {
		if ((txn->mc == (SLIM_MSG_MC_RECONFIGURE_NOW |
					SLIM_MSG_CLK_PAUSE_SEQ_FLG)) &&
				timeout) {
			timeout = wait_for_completion_timeout(&dev->reconf, HZ);
			dev->reconf_busy = false;
			if (timeout) {
				clk_disable(dev->rclk);
				disable_irq(dev->irq);
			}
		}
		if ((txn->mc == (SLIM_MSG_MC_RECONFIGURE_NOW |
					SLIM_MSG_CLK_PAUSE_SEQ_FLG)) &&
				!timeout) {
			dev->reconf_busy = false;
			dev_err(dev->dev, "clock pause failed");
			mutex_unlock(&dev->tx_lock);
			return -ETIMEDOUT;
		}
		if (txn->mt == SLIM_MSG_MT_CORE &&
			txn->mc == SLIM_MSG_MC_RECONFIGURE_NOW) {
			if (dev->ctrl.sched.usedslots == 0 &&
					dev->chan_active) {
				dev->chan_active = false;
				msm_slim_put_ctrl(dev);
			}
		}
	}
	mutex_unlock(&dev->tx_lock);
	if (msgv >= 0)
		msm_slim_put_ctrl(dev);

	if (!timeout)
		dev_err(dev->dev, "TX timed out:MC:0x%x,mt:0x%x", txn->mc,
					txn->mt);

	return timeout ? dev->err : -ETIMEDOUT;
}

static int msm_set_laddr(struct slim_controller *ctrl, const u8 *ea,
				u8 elen, u8 laddr)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	DECLARE_COMPLETION_ONSTACK(done);
	int timeout;
	u32 *buf;
	mutex_lock(&dev->tx_lock);
	buf = msm_get_msg_buf(ctrl, 9);
	buf[0] = SLIM_MSG_ASM_FIRST_WORD(9, SLIM_MSG_MT_CORE,
					SLIM_MSG_MC_ASSIGN_LOGICAL_ADDRESS,
					SLIM_MSG_DEST_LOGICALADDR,
					ea[5] | ea[4] << 8);
	buf[1] = ea[3] | (ea[2] << 8) | (ea[1] << 16) | (ea[0] << 24);
	buf[2] = laddr;

	dev->wr_comp = &done;
	msm_send_msg_buf(ctrl, buf, 9);
	timeout = wait_for_completion_timeout(&done, HZ);
	mutex_unlock(&dev->tx_lock);
	return timeout ? dev->err : -ETIMEDOUT;
}

static int msm_clk_pause_wakeup(struct slim_controller *ctrl)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	enable_irq(dev->irq);
	clk_enable(dev->rclk);
	writel_relaxed(1, dev->base + FRM_WAKEUP);
	/* Make sure framer wakeup write goes through before exiting function */
	mb();
	/*
	 * Workaround: Currently, slave is reporting lost-sync messages
	 * after slimbus comes out of clock pause.
	 * Transaction with slave fail before slave reports that message
	 * Give some time for that report to come
	 * Slimbus wakes up in clock gear 10 at 24.576MHz. With each superframe
	 * being 250 usecs, we wait for 20 superframes here to ensure
	 * we get the message
	 */
	usleep_range(5000, 5000);
	return 0;
}

static int msm_config_port(struct slim_controller *ctrl, u8 pn)
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

static enum slim_port_err msm_slim_port_xfer_status(struct slim_controller *ctr,
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

static int msm_slim_port_xfer(struct slim_controller *ctrl, u8 pn, u8 *iobuf,
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

static int msm_sat_define_ch(struct msm_slim_sat *sat, u8 *buf, u8 len, u8 mc)
{
	struct msm_slim_ctrl *dev = sat->dev;
	enum slim_ch_control oper;
	int i;
	int ret = 0;
	if (mc == SLIM_USR_MC_CHAN_CTRL) {
		u16 chanh = sat->satch[buf[5]];
		oper = ((buf[3] & 0xC0) >> 6);
		/* part of grp. activating/removing 1 will take care of rest */
		ret = slim_control_ch(&sat->satcl, chanh, oper, false);
	} else {
		u16 chh[40];
		struct slim_ch prop;
		u32 exp;
		u8 coeff, cc;
		u8 prrate = buf[6];
		for (i = 8; i < len; i++)
			chh[i-8] = sat->satch[buf[i]];
		prop.dataf = (enum slim_ch_dataf)((buf[3] & 0xE0) >> 5);
		prop.auxf = (enum slim_ch_auxf)((buf[4] & 0xC0) >> 5);
		prop.baser = SLIM_RATE_4000HZ;
		if (prrate & 0x8)
			prop.baser = SLIM_RATE_11025HZ;
		else
			prop.baser = SLIM_RATE_4000HZ;
		prop.prot = (enum slim_ch_proto)(buf[5] & 0x0F);
		prop.sampleszbits = (buf[4] & 0x1F)*SLIM_CL_PER_SL;
		exp = (u32)((buf[5] & 0xF0) >> 4);
		coeff = (buf[4] & 0x20) >> 5;
		cc = (coeff ? 3 : 1);
		prop.ratem = cc * (1 << exp);
		if (i > 9)
			ret = slim_define_ch(&sat->satcl, &prop, chh, len - 8,
						true, &sat->satch[buf[8]]);
		else
			ret = slim_define_ch(&sat->satcl, &prop,
						&sat->satch[buf[8]], 1, false,
						NULL);
		dev_dbg(dev->dev, "define sat grp returned:%d", ret);

		/* part of group so activating 1 will take care of rest */
		if (mc == SLIM_USR_MC_DEF_ACT_CHAN)
			ret = slim_control_ch(&sat->satcl,
					sat->satch[buf[8]],
					SLIM_CH_ACTIVATE, false);
	}
	return ret;
}

static void msm_slim_rxwq(struct msm_slim_ctrl *dev)
{
	u8 buf[40];
	u8 mc, mt, len;
	int i, ret;
	if ((msm_slim_rx_dequeue(dev, (u8 *)buf)) != -ENODATA) {
		len = buf[0] & 0x1F;
		mt = (buf[0] >> 5) & 0x7;
		mc = buf[1];
		if (mt == SLIM_MSG_MT_CORE &&
			mc == SLIM_MSG_MC_REPORT_PRESENT) {
			u8 laddr;
			u8 e_addr[6];
			for (i = 0; i < 6; i++)
				e_addr[i] = buf[7-i];

			ret = slim_assign_laddr(&dev->ctrl, e_addr, 6, &laddr);
			/* Is this Qualcomm ported generic device? */
			if (!ret && e_addr[5] == QC_MFGID_LSB &&
				e_addr[4] == QC_MFGID_MSB &&
				e_addr[1] == QC_DEVID_PGD &&
				e_addr[2] != QC_CHIPID_SL)
				dev->pgdla = laddr;
			if (!ret && !pm_runtime_enabled(dev->dev) &&
				laddr == (QC_MSM_DEVS - 1))
				pm_runtime_enable(dev->dev);

		} else if (mc == SLIM_MSG_MC_REPLY_INFORMATION ||
				mc == SLIM_MSG_MC_REPLY_VALUE) {
			u8 tid = buf[3];
			dev_dbg(dev->dev, "tid:%d, len:%d\n", tid, len - 4);
			slim_msg_response(&dev->ctrl, &buf[4], tid,
						len - 4);
			pm_runtime_mark_last_busy(dev->dev);
		} else if (mc == SLIM_MSG_MC_REPORT_INFORMATION) {
			u8 l_addr = buf[2];
			u16 ele = (u16)buf[4] << 4;
			ele |= ((buf[3] & 0xf0) >> 4);
			dev_err(dev->dev, "Slim-dev:%d report inf element:0x%x",
					l_addr, ele);
			for (i = 0; i < len - 5; i++)
				dev_err(dev->dev, "offset:0x%x:bit mask:%x",
						i, buf[i+5]);
		} else {
			dev_err(dev->dev, "unexpected message:mc:%x, mt:%x",
					mc, mt);
			for (i = 0; i < len; i++)
				dev_err(dev->dev, "error msg: %x", buf[i]);

		}
	} else
		dev_err(dev->dev, "rxwq called and no dequeue");
}

static void slim_sat_rxprocess(struct work_struct *work)
{
	struct msm_slim_sat *sat = container_of(work, struct msm_slim_sat, wd);
	struct msm_slim_ctrl *dev = sat->dev;
	u8 buf[40];

	while ((msm_sat_dequeue(sat, buf)) != -ENODATA) {
		struct slim_msg_txn txn;
		int i;
		u8 len, mc, mt;
		u32 bw_sl;
		int ret = 0;
		int satv = -1;
		bool gen_ack = false;
		u8 tid;
		u8 wbuf[8];
		txn.mt = SLIM_MSG_MT_SRC_REFERRED_USER;
		txn.dt = SLIM_MSG_DEST_LOGICALADDR;
		txn.ec = 0;
		txn.rbuf = NULL;
		txn.la = sat->satcl.laddr;
		/* satellite handling */
		len = buf[0] & 0x1F;
		mc = buf[1];
		mt = (buf[0] >> 5) & 0x7;

		if (mt == SLIM_MSG_MT_CORE &&
			mc == SLIM_MSG_MC_REPORT_PRESENT) {
			u8 laddr;
			u8 e_addr[6];
			for (i = 0; i < 6; i++)
				e_addr[i] = buf[7-i];

			if (pm_runtime_enabled(dev->dev)) {
				satv = msm_slim_get_ctrl(dev);
				if (satv >= 0)
					sat->pending_capability = true;
			}
			slim_assign_laddr(&dev->ctrl, e_addr, 6, &laddr);
			sat->satcl.laddr = laddr;
		} else if (mt != SLIM_MSG_MT_CORE &&
				mc != SLIM_MSG_MC_REPORT_PRESENT) {
			satv = msm_slim_get_ctrl(dev);
		}
		switch (mc) {
		case SLIM_MSG_MC_REPORT_PRESENT:
			/* Remove runtime_pm vote once satellite acks */
			if (mt != SLIM_MSG_MT_CORE) {
				if (pm_runtime_enabled(dev->dev) &&
					sat->pending_capability) {
					msm_slim_put_ctrl(dev);
					sat->pending_capability = false;
				}
				continue;
			}
			/* send a Manager capability msg */
			if (sat->sent_capability)
				continue;
			ret = slim_add_device(&dev->ctrl, &sat->satcl);
			if (ret) {
				dev_err(dev->dev,
					"Satellite-init failed");
				continue;
			}
			/* Satellite owns first 21 channels */
			sat->satch = kzalloc(21 * sizeof(u16), GFP_KERNEL);
			sat->nsatch = 20;
			/* alloc all sat chans */
			for (i = 0; i < 21; i++)
				slim_alloc_ch(&sat->satcl, &sat->satch[i]);
			txn.mc = SLIM_USR_MC_MASTER_CAPABILITY;
			txn.mt = SLIM_MSG_MT_SRC_REFERRED_USER;
			txn.la = sat->satcl.laddr;
			txn.rl = 8;
			wbuf[0] = SAT_MAGIC_LSB;
			wbuf[1] = SAT_MAGIC_MSB;
			wbuf[2] = SAT_MSG_VER;
			wbuf[3] = SAT_MSG_PROT;
			txn.wbuf = wbuf;
			txn.len = 4;
			sat->sent_capability = true;
			msm_xfer_msg(&dev->ctrl, &txn);
			break;
		case SLIM_USR_MC_ADDR_QUERY:
			memcpy(&wbuf[1], &buf[4], 6);
			ret = slim_get_logical_addr(&sat->satcl,
					&wbuf[1], 6, &wbuf[7]);
			if (ret)
				memset(&wbuf[1], 0, 6);
			wbuf[0] = buf[3];
			txn.mc = SLIM_USR_MC_ADDR_REPLY;
			txn.rl = 12;
			txn.len = 8;
			txn.wbuf = wbuf;
			msm_xfer_msg(&dev->ctrl, &txn);
			break;
		case SLIM_USR_MC_DEFINE_CHAN:
		case SLIM_USR_MC_DEF_ACT_CHAN:
		case SLIM_USR_MC_CHAN_CTRL:
			if (mc != SLIM_USR_MC_CHAN_CTRL)
				tid = buf[7];
			else
				tid = buf[4];
			gen_ack = true;
			ret = msm_sat_define_ch(sat, buf, len, mc);
			if (ret) {
				dev_err(dev->dev,
					"SAT define_ch returned:%d",
					ret);
			}
			if (!sat->pending_reconf) {
				int chv = msm_slim_get_ctrl(dev);
				if (chv >= 0)
					sat->pending_reconf = true;
			}
			break;
		case SLIM_USR_MC_RECONFIG_NOW:
			tid = buf[3];
			gen_ack = true;
			ret = slim_reconfigure_now(&sat->satcl);
			if (sat->pending_reconf) {
				msm_slim_put_ctrl(dev);
				sat->pending_reconf = false;
			}
			break;
		case SLIM_USR_MC_REQ_BW:
			/* what we get is in SLOTS */
			bw_sl = (u32)buf[4] << 3 |
						((buf[3] & 0xE0) >> 5);
			sat->satcl.pending_msgsl = bw_sl;
			tid = buf[5];
			gen_ack = true;
			break;
		case SLIM_USR_MC_CONNECT_SRC:
		case SLIM_USR_MC_CONNECT_SINK:
			if (mc == SLIM_USR_MC_CONNECT_SRC)
				txn.mc = SLIM_MSG_MC_CONNECT_SOURCE;
			else
				txn.mc = SLIM_MSG_MC_CONNECT_SINK;
			wbuf[0] = buf[4] & 0x1F;
			wbuf[1] = buf[5];
			tid = buf[6];
			txn.la = buf[3];
			txn.mt = SLIM_MSG_MT_CORE;
			txn.rl = 6;
			txn.len = 2;
			txn.wbuf = wbuf;
			gen_ack = true;
			ret = msm_xfer_msg(&dev->ctrl, &txn);
			break;
		case SLIM_USR_MC_DISCONNECT_PORT:
			txn.mc = SLIM_MSG_MC_DISCONNECT_PORT;
			wbuf[0] = buf[4] & 0x1F;
			tid = buf[5];
			txn.la = buf[3];
			txn.rl = 5;
			txn.len = 1;
			txn.mt = SLIM_MSG_MT_CORE;
			txn.wbuf = wbuf;
			gen_ack = true;
			ret = msm_xfer_msg(&dev->ctrl, &txn);
		default:
			break;
		}
		if (!gen_ack) {
			if (mc != SLIM_MSG_MC_REPORT_PRESENT && satv >= 0)
				msm_slim_put_ctrl(dev);
			continue;
		}

		wbuf[0] = tid;
		if (!ret)
			wbuf[1] = MSM_SAT_SUCCSS;
		else
			wbuf[1] = 0;
		txn.mc = SLIM_USR_MC_GENERIC_ACK;
		txn.la = sat->satcl.laddr;
		txn.rl = 6;
		txn.len = 2;
		txn.wbuf = wbuf;
		txn.mt = SLIM_MSG_MT_SRC_REFERRED_USER;
		msm_xfer_msg(&dev->ctrl, &txn);
		if (satv >= 0)
			msm_slim_put_ctrl(dev);
	}
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
static inline int
msm_slim_post_rx_msgq(struct msm_slim_ctrl *dev, int ix)
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

static inline int
msm_slim_rx_msgq_get(struct msm_slim_ctrl *dev, u32 *data, int offset)
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

static int msm_slim_rx_msgq_thread(void *data)
{
	struct msm_slim_ctrl *dev = (struct msm_slim_ctrl *)data;
	struct completion *notify = &dev->rx_msgq_notify;
	struct msm_slim_sat *sat = NULL;
	u32 mc = 0;
	u32 mt = 0;
	u32 buffer[10];
	int index = 0;
	u8 msg_len = 0;
	int ret;

	dev_dbg(dev->dev, "rx thread started");

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		ret = wait_for_completion_interruptible(notify);

		if (ret)
			dev_err(dev->dev, "rx thread wait error:%d", ret);

		/* 1 irq notification per message */
		if (!dev->use_rx_msgqs) {
			msm_slim_rxwq(dev);
			continue;
		}

		ret = msm_slim_rx_msgq_get(dev, buffer, index);
		if (ret) {
			dev_err(dev->dev, "rx_msgq_get() failed 0x%x\n", ret);
			continue;
		}

		pr_debug("message[%d] = 0x%x\n", index, *buffer);

		/* Decide if we use generic RX or satellite RX */
		if (index++ == 0) {
			msg_len = *buffer & 0x1F;
			pr_debug("Start of new message, len = %d\n", msg_len);
			mt = (buffer[0] >> 5) & 0x7;
			mc = (buffer[0] >> 8) & 0xff;
			dev_dbg(dev->dev, "MC: %x, MT: %x\n", mc, mt);
			if (mt == SLIM_MSG_MT_DEST_REFERRED_USER ||
				mt == SLIM_MSG_MT_SRC_REFERRED_USER)
				sat = dev->satd;

		} else if ((index * 4) >= msg_len) {
			index = 0;
			if (mt == SLIM_MSG_MT_CORE &&
				mc == SLIM_MSG_MC_REPORT_PRESENT) {
				u8 e_addr[6];
				msm_get_eaddr(e_addr, buffer);
				if (msm_is_sat_dev(e_addr))
					sat = dev->satd;
			}
			if (sat) {
				msm_sat_enqueue(sat, buffer, msg_len);
				queue_work(sat->wq, &sat->wd);
				sat = NULL;
			} else {
				msm_slim_rx_enqueue(dev, buffer, msg_len);
				msm_slim_rxwq(dev);
			}
		}
	}

	return 0;
}

static int __devinit msm_slim_init_rx_msgq(struct msm_slim_ctrl *dev)
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

	/* Allocate the endpoint */
	ret = msm_slim_init_endpoint(dev, endpoint);
	if (ret) {
		dev_err(dev->dev, "init_endpoint failed 0x%x\n", ret);
		goto sps_init_endpoint_failed;
	}

	/* Get the pipe indices for the message queues */
	pipe_offset = (readl_relaxed(dev->base + MGR_STATUS) & 0xfc) >> 2;
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

	/* Register completion for DESC_DONE */
	init_completion(notify);
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

	/* Fire up the Rx message queue thread */
	dev->rx_msgq_thread = kthread_run(msm_slim_rx_msgq_thread, dev,
					MSM_SLIM_NAME "_rx_msgq_thread");
	if (!dev->rx_msgq_thread) {
		dev_err(dev->dev, "Failed to start Rx message queue thread\n");
		ret = -EIO;
	} else
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
	return ret;
}

/* Registers BAM h/w resource with SPS driver and initializes msgq endpoints */
static int __devinit
msm_slim_sps_init(struct msm_slim_ctrl *dev, struct resource *bam_mem)
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
		dev_err(dev->dev, "sps_register_bam_device failed 0x%x\n", ret);
		return ret;
	}
	dev->bam.hdl = bam_handle;
	dev_dbg(dev->dev, "SLIM BAM registered, handle = 0x%x\n", bam_handle);

	ret = msm_slim_init_rx_msgq(dev);
	if (ret) {
		dev_err(dev->dev, "msm_slim_init_rx_msgq failed 0x%x\n", ret);
		goto rx_msgq_init_failed;
	}

	return 0;
rx_msgq_init_failed:
	sps_deregister_bam_device(bam_handle);
	dev->bam.hdl = 0L;
	return ret;
}

static void msm_slim_sps_exit(struct msm_slim_ctrl *dev)
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
	}
	sps_deregister_bam_device(dev->bam.hdl);
}

static void msm_slim_prg_slew(struct platform_device *pdev,
				struct msm_slim_ctrl *dev)
{
	struct resource *slew_io;
	void __iomem *slew_reg;
	/* SLEW RATE register for this slimbus */
	dev->slew_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"slimbus_slew_reg");
	if (!dev->slew_mem) {
		dev_dbg(&pdev->dev, "no slimbus slew resource\n");
		return;
	}
	slew_io = request_mem_region(dev->slew_mem->start,
				resource_size(dev->slew_mem), pdev->name);
	if (!slew_io) {
		dev_dbg(&pdev->dev, "slimbus-slew mem claimed\n");
		dev->slew_mem = NULL;
		return;
	}

	slew_reg = ioremap(dev->slew_mem->start, resource_size(dev->slew_mem));
	if (!slew_reg) {
		dev_dbg(dev->dev, "slew register mapping failed");
		release_mem_region(dev->slew_mem->start,
					resource_size(dev->slew_mem));
		dev->slew_mem = NULL;
		return;
	}
	writel_relaxed(1, slew_reg);
	/* Make sure slimbus-slew rate enabling goes through */
	wmb();
	iounmap(slew_reg);
}

static int __devinit msm_slim_probe(struct platform_device *pdev)
{
	struct msm_slim_ctrl *dev;
	int ret;
	struct resource		*bam_mem, *bam_io;
	struct resource		*slim_mem, *slim_io;
	struct resource		*irq, *bam_irq;
	slim_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"slimbus_physical");
	if (!slim_mem) {
		dev_err(&pdev->dev, "no slimbus physical memory resource\n");
		return -ENODEV;
	}
	slim_io = request_mem_region(slim_mem->start, resource_size(slim_mem),
					pdev->name);
	if (!slim_io) {
		dev_err(&pdev->dev, "slimbus memory already claimed\n");
		return -EBUSY;
	}

	bam_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"slimbus_bam_physical");
	if (!bam_mem) {
		dev_err(&pdev->dev, "no slimbus BAM memory resource\n");
		ret = -ENODEV;
		goto err_get_res_bam_failed;
	}
	bam_io = request_mem_region(bam_mem->start, resource_size(bam_mem),
					pdev->name);
	if (!bam_io) {
		release_mem_region(slim_mem->start, resource_size(slim_mem));
		dev_err(&pdev->dev, "slimbus BAM memory already claimed\n");
		ret = -EBUSY;
		goto err_get_res_bam_failed;
	}
	irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"slimbus_irq");
	if (!irq) {
		dev_err(&pdev->dev, "no slimbus IRQ resource\n");
		ret = -ENODEV;
		goto err_get_res_failed;
	}
	bam_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"slimbus_bam_irq");
	if (!bam_irq) {
		dev_err(&pdev->dev, "no slimbus BAM IRQ resource\n");
		ret = -ENODEV;
		goto err_get_res_failed;
	}

	dev = kzalloc(sizeof(struct msm_slim_ctrl), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "no memory for MSM slimbus controller\n");
		ret = -ENOMEM;
		goto err_get_res_failed;
	}
	dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, dev);
	slim_set_ctrldata(&dev->ctrl, dev);
	dev->base = ioremap(slim_mem->start, resource_size(slim_mem));
	if (!dev->base) {
		dev_err(&pdev->dev, "IOremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap_failed;
	}
	dev->bam.base = ioremap(bam_mem->start, resource_size(bam_mem));
	if (!dev->bam.base) {
		dev_err(&pdev->dev, "BAM IOremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap_bam_failed;
	}
	dev->ctrl.nr = pdev->id;
	dev->ctrl.nchans = MSM_SLIM_NCHANS;
	dev->ctrl.nports = MSM_SLIM_NPORTS;
	dev->ctrl.set_laddr = msm_set_laddr;
	dev->ctrl.xfer_msg = msm_xfer_msg;
	dev->ctrl.wakeup =  msm_clk_pause_wakeup;
	dev->ctrl.config_port = msm_config_port;
	dev->ctrl.port_xfer = msm_slim_port_xfer;
	dev->ctrl.port_xfer_status = msm_slim_port_xfer_status;
	/* Reserve some messaging BW for satellite-apps driver communication */
	dev->ctrl.sched.pending_msgsl = 30;

	init_completion(&dev->reconf);
	mutex_init(&dev->tx_lock);
	spin_lock_init(&dev->rx_lock);
	dev->ee = 1;
	dev->use_rx_msgqs = 1;
	dev->irq = irq->start;
	dev->bam.irq = bam_irq->start;

	ret = msm_slim_sps_init(dev, bam_mem);
	if (ret != 0) {
		dev_err(dev->dev, "error SPS init\n");
		goto err_sps_init_failed;
	}


	dev->rclk = clk_get(dev->dev, "audio_slimbus_clk");
	if (!dev->rclk) {
		dev_err(dev->dev, "slimbus clock not found");
		goto err_clk_get_failed;
	}
	dev->framer.rootfreq = SLIM_ROOT_FREQ >> 3;
	dev->framer.superfreq =
		dev->framer.rootfreq / SLIM_CL_PER_SUPERFRAME_DIV8;
	dev->ctrl.a_framer = &dev->framer;
	dev->ctrl.clkgear = SLIM_MAX_CLK_GEAR;
	dev->ctrl.dev.parent = &pdev->dev;

	ret = request_irq(dev->irq, msm_slim_interrupt, IRQF_TRIGGER_HIGH,
				"msm_slim_irq", dev);
	if (ret) {
		dev_err(&pdev->dev, "request IRQ failed\n");
		goto err_request_irq_failed;
	}

	dev->satd = kzalloc(sizeof(struct msm_slim_sat), GFP_KERNEL);
	if (!dev->satd) {
		ret = -ENOMEM;
		goto err_sat_failed;
	}

	msm_slim_prg_slew(pdev, dev);
	clk_set_rate(dev->rclk, SLIM_ROOT_FREQ);
	clk_enable(dev->rclk);

	dev->satd->dev = dev;
	dev->satd->satcl.name  = "msm_sat_dev";
	spin_lock_init(&dev->satd->lock);
	INIT_WORK(&dev->satd->wd, slim_sat_rxprocess);
	dev->satd->wq = create_singlethread_workqueue("msm_slim_sat");
	/* Component register initialization */
	writel_relaxed(1, dev->base + COMP_CFG);
	writel_relaxed((EE_MGR_RSC_GRP | EE_NGD_2 | EE_NGD_1),
				dev->base + COMP_TRUST_CFG);

	/*
	 * Manager register initialization
	 * If RX msg Q is used, disable RX_MSG_RCVD interrupt
	 */
	if (dev->use_rx_msgqs)
		writel_relaxed((MGR_INT_RECFG_DONE | MGR_INT_TX_NACKED_2 |
			MGR_INT_MSG_BUF_CONTE | /* MGR_INT_RX_MSG_RCVD | */
			MGR_INT_TX_MSG_SENT), dev->base + MGR_INT_EN);
	else
		writel_relaxed((MGR_INT_RECFG_DONE | MGR_INT_TX_NACKED_2 |
			MGR_INT_MSG_BUF_CONTE | MGR_INT_RX_MSG_RCVD |
			MGR_INT_TX_MSG_SENT), dev->base + MGR_INT_EN);
	writel_relaxed(1, dev->base + MGR_CFG);
	/*
	 * Framer registers are beyond 1K memory region after Manager and/or
	 * component registers. Make sure those writes are ordered
	 * before framer register writes
	 */
	wmb();

	/* Register with framework before enabling frame, clock */
	ret = slim_add_numbered_controller(&dev->ctrl);
	if (ret) {
		dev_err(dev->dev, "error adding controller\n");
		goto err_ctrl_failed;
	}

	/* Framer register initialization */
	writel_relaxed((0xA << REF_CLK_GEAR) | (0xA << CLK_GEAR) |
		(1 << ROOT_FREQ) | (1 << FRM_ACTIVE) | 1,
		dev->base + FRM_CFG);
	/*
	 * Make sure that framer wake-up and enabling writes go through
	 * before any other component is enabled. Framer is responsible for
	 * clocking the bus and enabling framer first will ensure that other
	 * devices can report presence when they are enabled
	 */
	mb();

	/* Enable RX msg Q */
	if (dev->use_rx_msgqs)
		writel_relaxed(MGR_CFG_ENABLE | MGR_CFG_RX_MSGQ_EN,
					dev->base + MGR_CFG);
	else
		writel_relaxed(MGR_CFG_ENABLE, dev->base + MGR_CFG);
	/*
	 * Make sure that manager-enable is written through before interface
	 * device is enabled
	 */
	mb();
	writel_relaxed(1, dev->base + INTF_CFG);
	/*
	 * Make sure that interface-enable is written through before enabling
	 * ported generic device inside MSM manager
	 */
	mb();
	writel_relaxed(1, dev->base + PGD_CFG);
	writel_relaxed(0x3F<<17, dev->base + (PGD_OWN_EEn + (4 * dev->ee)));
	/*
	 * Make sure that ported generic device is enabled and port-EE settings
	 * are written through before finally enabling the component
	 */
	mb();

	writel_relaxed(1, dev->base + COMP_CFG);
	/*
	 * Make sure that all writes have gone through before exiting this
	 * function
	 */
	mb();
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, MSM_SLIM_AUTOSUSPEND);
	pm_runtime_set_active(&pdev->dev);

	dev_dbg(dev->dev, "MSM SB controller is up!\n");
	return 0;

err_ctrl_failed:
	writel_relaxed(0, dev->base + COMP_CFG);
	kfree(dev->satd);
err_sat_failed:
	free_irq(dev->irq, dev);
err_request_irq_failed:
	clk_disable(dev->rclk);
	clk_put(dev->rclk);
err_clk_get_failed:
	msm_slim_sps_exit(dev);
err_sps_init_failed:
	iounmap(dev->bam.base);
err_ioremap_bam_failed:
	iounmap(dev->base);
err_ioremap_failed:
	kfree(dev);
err_get_res_failed:
	release_mem_region(bam_mem->start, resource_size(bam_mem));
err_get_res_bam_failed:
	release_mem_region(slim_mem->start, resource_size(slim_mem));
	return ret;
}

static int __devexit msm_slim_remove(struct platform_device *pdev)
{
	struct msm_slim_ctrl *dev = platform_get_drvdata(pdev);
	struct resource *bam_mem;
	struct resource *slim_mem;
	struct resource *slew_mem = dev->slew_mem;
	struct msm_slim_sat *sat = dev->satd;
	slim_remove_device(&sat->satcl);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	kfree(sat->satch);
	destroy_workqueue(sat->wq);
	kfree(sat);
	free_irq(dev->irq, dev);
	slim_del_controller(&dev->ctrl);
	clk_put(dev->rclk);
	msm_slim_sps_exit(dev);
	kthread_stop(dev->rx_msgq_thread);
	iounmap(dev->bam.base);
	iounmap(dev->base);
	kfree(dev);
	bam_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"slimbus_bam_physical");
	if (bam_mem)
		release_mem_region(bam_mem->start, resource_size(bam_mem));
	if (slew_mem)
		release_mem_region(slew_mem->start, resource_size(slew_mem));
	slim_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"slimbus_physical");
	if (slim_mem)
		release_mem_region(slim_mem->start, resource_size(slim_mem));
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int msm_slim_runtime_idle(struct device *device)
{
	dev_dbg(device, "pm_runtime: idle...\n");
	pm_request_autosuspend(device);
	return -EAGAIN;
}
#endif

/*
 * If PM_RUNTIME is not defined, these 2 functions become helper
 * functions to be called from system suspend/resume. So they are not
 * inside ifdef CONFIG_PM_RUNTIME
 */
#ifdef CONFIG_PM_SLEEP
static int msm_slim_runtime_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct msm_slim_ctrl *dev = platform_get_drvdata(pdev);
	int ret;
	dev_dbg(device, "pm_runtime: suspending...\n");
	dev->state = MSM_CTRL_SLEEPING;
	ret = slim_ctrl_clk_pause(&dev->ctrl, false, SLIM_CLK_UNSPECIFIED);
	if (ret) {
		dev_err(device, "clk pause not entered:%d", ret);
		dev->state = MSM_CTRL_AWAKE;
	} else {
		dev->state = MSM_CTRL_ASLEEP;
	}
	return ret;
}

static int msm_slim_runtime_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct msm_slim_ctrl *dev = platform_get_drvdata(pdev);
	int ret = 0;
	dev_dbg(device, "pm_runtime: resuming...\n");
	if (dev->state == MSM_CTRL_ASLEEP)
		ret = slim_ctrl_clk_pause(&dev->ctrl, true, 0);
	if (ret) {
		dev_err(device, "clk pause not exited:%d", ret);
		dev->state = MSM_CTRL_ASLEEP;
	} else {
		dev->state = MSM_CTRL_AWAKE;
	}
	return ret;
}

static int msm_slim_suspend(struct device *dev)
{
	int ret = 0;
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev)) {
		dev_dbg(dev, "system suspend");
		ret = msm_slim_runtime_suspend(dev);
	}
	if (ret == -EBUSY) {
		/*
		* If the clock pause failed due to active channels, there is
		* a possibility that some audio stream is active during suspend
		* We dont want to return suspend failure in that case so that
		* display and relevant components can still go to suspend.
		* If there is some other error, then it should be passed-on
		* to system level suspend
		*/
		ret = 0;
	}
	return ret;
}

static int msm_slim_resume(struct device *dev)
{
	/* If runtime_pm is enabled, this resume shouldn't do anything */
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev)) {
		int ret;
		dev_dbg(dev, "system resume");
		ret = msm_slim_runtime_resume(dev);
		if (!ret) {
			pm_runtime_mark_last_busy(dev);
			pm_request_autosuspend(dev);
		}
		return ret;

	}
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops msm_slim_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		msm_slim_suspend,
		msm_slim_resume
	)
	SET_RUNTIME_PM_OPS(
		msm_slim_runtime_suspend,
		msm_slim_runtime_resume,
		msm_slim_runtime_idle
	)
};

static struct platform_driver msm_slim_driver = {
	.probe = msm_slim_probe,
	.remove = msm_slim_remove,
	.driver	= {
		.name = MSM_SLIM_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_slim_dev_pm_ops,
	},
};

static int msm_slim_init(void)
{
	return platform_driver_register(&msm_slim_driver);
}
subsys_initcall(msm_slim_init);

static void msm_slim_exit(void)
{
	platform_driver_unregister(&msm_slim_driver);
}
module_exit(msm_slim_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("MSM Slimbus controller");
MODULE_ALIAS("platform:msm-slim");
