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
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slimbus/slimbus.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_slimbus.h>
#include <mach/sps.h>
#include <mach/qdsp6v2/apr.h>
#include "slim-msm.h"

#define MSM_SLIM_NAME	"msm_slim_ctrl"
#define SLIM_ROOT_FREQ 24576000

#define QC_MFGID_LSB	0x2
#define QC_MFGID_MSB	0x17
#define QC_CHIPID_SL	0x10
#define QC_DEVID_SAT1	0x3
#define QC_DEVID_SAT2	0x4
#define QC_DEVID_PGD	0x5
#define QC_MSM_DEVS	5

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
	MGR_IE_STAT	= 0x2F0,
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
	INTR_WAKE	= 19,
};

static struct msm_slim_sat *msm_slim_alloc_sat(struct msm_slim_ctrl *dev);

static int msm_sat_enqueue(struct msm_slim_sat *sat, u32 *buf, u8 len)
{
	struct msm_slim_ctrl *dev = sat->dev;
	unsigned long flags;
	spin_lock_irqsave(&sat->lock, flags);
	if ((sat->stail + 1) % SAT_CONCUR_MSG == sat->shead) {
		spin_unlock_irqrestore(&sat->lock, flags);
		dev_err(dev->dev, "SAT QUEUE full!");
		return -EXFULL;
	}
	memcpy(sat->sat_msgs[sat->stail], (u8 *)buf, len);
	sat->stail = (sat->stail + 1) % SAT_CONCUR_MSG;
	spin_unlock_irqrestore(&sat->lock, flags);
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

static struct msm_slim_sat *addr_to_sat(struct msm_slim_ctrl *dev, u8 laddr)
{
	struct msm_slim_sat *sat = NULL;
	int i = 0;
	while (!sat && i < dev->nsats) {
		if (laddr == dev->satd[i]->satcl.laddr)
			sat = dev->satd[i];
		i++;
	}
	return sat;
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
			u32 mgr_stat = readl_relaxed(dev->base + MGR_STATUS);
			u32 mgr_ie_stat = readl_relaxed(dev->base +
						MGR_IE_STAT);
			u32 frm_stat = readl_relaxed(dev->base + FRM_STAT);
			u32 frm_cfg = readl_relaxed(dev->base + FRM_CFG);
			u32 frm_intr_stat = readl_relaxed(dev->base +
						FRM_INT_STAT);
			u32 frm_ie_stat = readl_relaxed(dev->base +
						FRM_IE_STAT);
			u32 intf_stat = readl_relaxed(dev->base + INTF_STAT);
			u32 intf_intr_stat = readl_relaxed(dev->base +
						INTF_INT_STAT);
			u32 intf_ie_stat = readl_relaxed(dev->base +
						INTF_IE_STAT);

			writel_relaxed(MGR_INT_TX_NACKED_2,
					dev->base + MGR_INT_CLR);
			pr_err("TX Nack MGR dump:int_stat:0x%x, mgr_stat:0x%x",
					stat, mgr_stat);
			pr_err("TX Nack MGR dump:ie_stat:0x%x", mgr_ie_stat);
			pr_err("TX Nack FRM dump:int_stat:0x%x, frm_stat:0x%x",
					frm_intr_stat, frm_stat);
			pr_err("TX Nack FRM dump:frm_cfg:0x%x, ie_stat:0x%x",
					frm_cfg, frm_ie_stat);
			pr_err("TX Nack INTF dump:intr_st:0x%x, intf_stat:0x%x",
					intf_intr_stat, intf_stat);
			pr_err("TX Nack INTF dump:ie_stat:0x%x", intf_ie_stat);

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
			u8 laddr = (u8)((rx_buf[0] >> 16) & 0xFF);
			struct msm_slim_sat *sat = addr_to_sat(dev, laddr);
			if (sat)
				msm_sat_enqueue(sat, rx_buf, len);
			else
				dev_err(dev->dev, "unknown sat:%d message",
						laddr);
			writel_relaxed(MGR_INT_RX_MSG_RCVD,
					dev->base + MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through before
			 * queuing work
			 */
			mb();
			if (sat)
				queue_work(sat->wq, &sat->wd);
		} else if (mt == SLIM_MSG_MT_CORE &&
			mc == SLIM_MSG_MC_REPORT_PRESENT) {
			u8 e_addr[6];
			msm_get_eaddr(e_addr, rx_buf);
			msm_slim_rx_enqueue(dev, rx_buf, len);
			writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
						MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through
			 * before signalling completion
			 */
			mb();
			complete(&dev->rx_msgq_notify);
		} else if (mt == SLIM_MSG_MT_CORE &&
			mc == SLIM_MSG_MC_REPORT_ABSENT) {
			writel_relaxed(MGR_INT_RX_MSG_RCVD, dev->base +
						MGR_INT_CLR);
			/*
			 * Guarantee that CLR bit write goes through
			 * before signalling completion
			 */
			mb();
			complete(&dev->rx_msgq_notify);

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
	pstat = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_ST_EEn, dev->ver));
	if (pstat != 0) {
		int i = 0;
		for (i = dev->pipe_b; i < MSM_SLIM_NPORTS; i++) {
			if (pstat & 1 << i) {
				u32 val = readl_relaxed(PGD_PORT(PGD_PORT_STATn,
							i, dev->ver));
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
			writel_relaxed(1, PGD_THIS_EE(PGD_PORT_INT_CL_EEn,
							dev->ver));
		}
		/*
		 * Guarantee that port interrupt bit(s) clearing writes go
		 * through before exiting ISR
		 */
		mb();
	}

	return IRQ_HANDLED;
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
	pbuf = msm_get_msg_buf(dev, txn->rl);
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
	msm_send_msg_buf(dev, pbuf, txn->rl, MGR_TX_MSG);
	timeout = wait_for_completion_timeout(&done, HZ);
	if (!timeout)
		dev->wr_comp = NULL;
	if (mc == SLIM_MSG_MC_RECONFIGURE_NOW) {
		if ((txn->mc == (SLIM_MSG_MC_RECONFIGURE_NOW |
					SLIM_MSG_CLK_PAUSE_SEQ_FLG)) &&
				timeout) {
			timeout = wait_for_completion_timeout(&dev->reconf, HZ);
			dev->reconf_busy = false;
			if (timeout) {
				clk_disable_unprepare(dev->rclk);
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

static void msm_slim_wait_retry(struct msm_slim_ctrl *dev)
{
	int msec_per_frm = 0;
	int sfr_per_sec;
	/* Wait for 1 superframe, or default time and then retry */
	sfr_per_sec = dev->framer.superfreq /
			(1 << (SLIM_MAX_CLK_GEAR - dev->ctrl.clkgear));
	if (sfr_per_sec)
		msec_per_frm = MSEC_PER_SEC / sfr_per_sec;
	if (msec_per_frm < DEF_RETRY_MS)
		msec_per_frm = DEF_RETRY_MS;
	msleep(msec_per_frm);
}
static int msm_set_laddr(struct slim_controller *ctrl, const u8 *ea,
				u8 elen, u8 laddr)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	struct completion done;
	int timeout, ret, retries = 0;
	u32 *buf;
retry_laddr:
	init_completion(&done);
	mutex_lock(&dev->tx_lock);
	buf = msm_get_msg_buf(dev, 9);
	buf[0] = SLIM_MSG_ASM_FIRST_WORD(9, SLIM_MSG_MT_CORE,
					SLIM_MSG_MC_ASSIGN_LOGICAL_ADDRESS,
					SLIM_MSG_DEST_LOGICALADDR,
					ea[5] | ea[4] << 8);
	buf[1] = ea[3] | (ea[2] << 8) | (ea[1] << 16) | (ea[0] << 24);
	buf[2] = laddr;

	dev->wr_comp = &done;
	ret = msm_send_msg_buf(dev, buf, 9, MGR_TX_MSG);
	timeout = wait_for_completion_timeout(&done, HZ);
	if (!timeout)
		dev->err = -ETIMEDOUT;
	if (dev->err) {
		ret = dev->err;
		dev->err = 0;
		dev->wr_comp = NULL;
	}
	mutex_unlock(&dev->tx_lock);
	if (ret) {
		pr_err("set LADDR:0x%x failed:ret:%d, retrying", laddr, ret);
		if (retries < INIT_MX_RETRIES) {
			msm_slim_wait_retry(dev);
			retries++;
			goto retry_laddr;
		} else {
			pr_err("set LADDR failed after retrying:ret:%d", ret);
		}
	}
	return ret;
}

static int msm_clk_pause_wakeup(struct slim_controller *ctrl)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	enable_irq(dev->irq);
	clk_prepare_enable(dev->rclk);
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

static int msm_sat_define_ch(struct msm_slim_sat *sat, u8 *buf, u8 len, u8 mc)
{
	struct msm_slim_ctrl *dev = sat->dev;
	enum slim_ch_control oper;
	int i;
	int ret = 0;
	if (mc == SLIM_USR_MC_CHAN_CTRL) {
		for (i = 0; i < sat->nsatch; i++) {
			if (buf[5] == sat->satch[i].chan)
				break;
		}
		if (i >= sat->nsatch)
			return -ENOTCONN;
		oper = ((buf[3] & 0xC0) >> 6);
		/* part of grp. activating/removing 1 will take care of rest */
		ret = slim_control_ch(&sat->satcl, sat->satch[i].chanh, oper,
					false);
		if (!ret) {
			for (i = 5; i < len; i++) {
				int j;
				for (j = 0; j < sat->nsatch; j++) {
					if (buf[i] == sat->satch[j].chan) {
						if (oper == SLIM_CH_REMOVE)
							sat->satch[j].req_rem++;
						else
							sat->satch[j].req_def++;
						break;
					}
				}
			}
		}
	} else {
		u16 chh[40];
		struct slim_ch prop;
		u32 exp;
		u16 *grph = NULL;
		u8 coeff, cc;
		u8 prrate = buf[6];
		if (len <= 8)
			return -EINVAL;
		for (i = 8; i < len; i++) {
			int j = 0;
			for (j = 0; j < sat->nsatch; j++) {
				if (sat->satch[j].chan == buf[i]) {
					chh[i - 8] = sat->satch[j].chanh;
					break;
				}
			}
			if (j < sat->nsatch) {
				u16 dummy;
				ret = slim_query_ch(&sat->satcl, buf[i],
							&dummy);
				if (ret)
					return ret;
				if (mc == SLIM_USR_MC_DEF_ACT_CHAN)
					sat->satch[j].req_def++;
				/* First channel in group from satellite */
				if (i == 8)
					grph = &sat->satch[j].chanh;
				continue;
			}
			if (sat->nsatch >= MSM_MAX_SATCH)
				return -EXFULL;
			ret = slim_query_ch(&sat->satcl, buf[i], &chh[i - 8]);
			if (ret)
				return ret;
			sat->satch[j].chan = buf[i];
			sat->satch[j].chanh = chh[i - 8];
			if (mc == SLIM_USR_MC_DEF_ACT_CHAN)
				sat->satch[j].req_def++;
			if (i == 8)
				grph = &sat->satch[j].chanh;
			sat->nsatch++;
		}
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
					true, &chh[0]);
		else
			ret = slim_define_ch(&sat->satcl, &prop,
					chh, 1, true, &chh[0]);
		dev_dbg(dev->dev, "define sat grp returned:%d", ret);
		if (ret)
			return ret;
		else if (grph)
			*grph = chh[0];

		/* part of group so activating 1 will take care of rest */
		if (mc == SLIM_USR_MC_DEF_ACT_CHAN)
			ret = slim_control_ch(&sat->satcl,
					chh[0],
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

			ret = slim_assign_laddr(&dev->ctrl, e_addr, 6, &laddr,
						false);
			/* Is this Qualcomm ported generic device? */
			if (!ret && e_addr[5] == QC_MFGID_LSB &&
				e_addr[4] == QC_MFGID_MSB &&
				e_addr[1] == QC_DEVID_PGD &&
				e_addr[2] != QC_CHIPID_SL)
				dev->pgdla = laddr;
			if (!ret && !pm_runtime_enabled(dev->dev) &&
				laddr == (QC_MSM_DEVS - 1))
				pm_runtime_enable(dev->dev);

			if (!ret && msm_is_sat_dev(e_addr)) {
				struct msm_slim_sat *sat = addr_to_sat(dev,
								laddr);
				if (!sat)
					sat = msm_slim_alloc_sat(dev);
				if (!sat)
					return;

				sat->satcl.laddr = laddr;
				msm_sat_enqueue(sat, (u32 *)buf, len);
				queue_work(sat->wq, &sat->wd);
			}
			if (ret)
				pr_err("assign laddr failed, error:%d", ret);
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
		u8 len, mc, mt;
		u32 bw_sl;
		int ret = 0;
		int satv = -1;
		bool gen_ack = false;
		u8 tid;
		u8 wbuf[8];
		int i, retries = 0;
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
			u8 e_addr[6];
			for (i = 0; i < 6; i++)
				e_addr[i] = buf[7-i];

			if (pm_runtime_enabled(dev->dev)) {
				satv = msm_slim_get_ctrl(dev);
				if (satv >= 0)
					sat->pending_capability = true;
			}
			/*
			 * Since capability message is already sent, present
			 * message will indicate subsystem hosting this
			 * satellite has restarted.
			 * Remove all active channels of this satellite
			 * when this is detected
			 */
			if (sat->sent_capability) {
				for (i = 0; i < sat->nsatch; i++) {
					if (sat->satch[i].reconf) {
						pr_err("SSR, sat:%d, rm ch:%d",
							sat->satcl.laddr,
							sat->satch[i].chan);
						slim_control_ch(&sat->satcl,
							sat->satch[i].chanh,
							SLIM_CH_REMOVE, true);
						slim_dealloc_ch(&sat->satcl,
							sat->satch[i].chanh);
						sat->satch[i].reconf = false;
					}
				}
			}
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
			if (sat->sent_capability) {
				if (mt == SLIM_MSG_MT_CORE)
					goto send_capability;
				else
					continue;
			}
			ret = slim_add_device(&dev->ctrl, &sat->satcl);
			if (ret) {
				dev_err(dev->dev,
					"Satellite-init failed");
				continue;
			}
			/* Satellite-channels */
			sat->satch = kzalloc(MSM_MAX_SATCH *
					sizeof(struct msm_sat_chan),
					GFP_KERNEL);
send_capability:
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
			ret = msm_xfer_msg(&dev->ctrl, &txn);
			if (ret) {
				pr_err("capability for:0x%x fail:%d, retry:%d",
						sat->satcl.laddr, ret, retries);
				if (retries < INIT_MX_RETRIES) {
					msm_slim_wait_retry(dev);
					retries++;
					goto send_capability;
				} else {
					pr_err("failed after all retries:%d",
							ret);
				}
			} else {
				sat->sent_capability = true;
			}
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
			for (i = 0; i < sat->nsatch; i++) {
				struct msm_sat_chan *sch = &sat->satch[i];
				if (sch->req_rem && sch->reconf) {
					if (!ret) {
						slim_dealloc_ch(&sat->satcl,
								sch->chanh);
						sch->reconf = false;
					}
					sch->req_rem--;
				} else if (sch->req_def) {
					if (ret)
						slim_dealloc_ch(&sat->satcl,
								sch->chanh);
					else
						sch->reconf = true;
					sch->req_def--;
				}
			}
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
			break;
		case SLIM_MSG_MC_REPORT_ABSENT:
			dev_info(dev->dev, "Received Report Absent Message\n");
			break;
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

static struct msm_slim_sat *msm_slim_alloc_sat(struct msm_slim_ctrl *dev)
{
	struct msm_slim_sat *sat;
	char *name;
	if (dev->nsats >= MSM_MAX_NSATS)
		return NULL;

	sat = kzalloc(sizeof(struct msm_slim_sat), GFP_KERNEL);
	if (!sat) {
		dev_err(dev->dev, "no memory for satellite");
		return NULL;
	}
	name = kzalloc(SLIMBUS_NAME_SIZE, GFP_KERNEL);
	if (!name) {
		dev_err(dev->dev, "no memory for satellite name");
		kfree(sat);
		return NULL;
	}
	dev->satd[dev->nsats] = sat;
	sat->dev = dev;
	snprintf(name, SLIMBUS_NAME_SIZE, "msm_sat%d", dev->nsats);
	sat->satcl.name = name;
	spin_lock_init(&sat->lock);
	INIT_WORK(&sat->wd, slim_sat_rxprocess);
	sat->wq = create_singlethread_workqueue(sat->satcl.name);
	if (!sat->wq) {
		kfree(name);
		kfree(sat);
		return NULL;
	}
	/*
	 * Both sats will be allocated from RX thread and RX thread will
	 * process messages sequentially. No synchronization necessary
	 */
	dev->nsats++;
	return sat;
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
		if (dev->use_rx_msgqs != MSM_MSGQ_ENABLED) {
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
				mt == SLIM_MSG_MT_SRC_REFERRED_USER) {
				u8 laddr;
				laddr = (u8)((buffer[0] >> 16) & 0xff);
				sat = addr_to_sat(dev, laddr);
			}
		}
		if ((index * 4) >= msg_len) {
			index = 0;
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
	enum apr_subsys_state q6_state;
	struct resource		*bam_mem, *bam_io;
	struct resource		*slim_mem, *slim_io;
	struct resource		*irq, *bam_irq;
	bool			rxreg_access = false;

	q6_state = apr_get_q6_state();
	if (q6_state == APR_SUBSYS_DOWN) {
		dev_dbg(&pdev->dev, "defering %s, adsp_state %d\n", __func__,
			q6_state);
		return -EPROBE_DEFER;
	} else
		dev_dbg(&pdev->dev, "adsp is ready\n");

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
	if (pdev->dev.of_node) {

		ret = of_property_read_u32(pdev->dev.of_node, "cell-index",
					&dev->ctrl.nr);
		if (ret) {
			dev_err(&pdev->dev, "Cell index not specified:%d", ret);
			goto err_of_init_failed;
		}
		rxreg_access = of_property_read_bool(pdev->dev.of_node,
					"qcom,rxreg-access");
		/* Optional properties */
		ret = of_property_read_u32(pdev->dev.of_node,
					"qcom,min-clk-gear", &dev->ctrl.min_cg);
		ret = of_property_read_u32(pdev->dev.of_node,
					"qcom,max-clk-gear", &dev->ctrl.max_cg);
		pr_debug("min_cg:%d, max_cg:%d, rxreg: %d", dev->ctrl.min_cg,
					dev->ctrl.max_cg, rxreg_access);
	} else {
		dev->ctrl.nr = pdev->id;
	}
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
	if (rxreg_access)
		dev->use_rx_msgqs = MSM_MSGQ_DISABLED;
	else
		dev->use_rx_msgqs = MSM_MSGQ_RESET;

	dev->irq = irq->start;
	dev->bam.irq = bam_irq->start;

	dev->hclk = clk_get(dev->dev, "iface_clk");
	if (IS_ERR(dev->hclk))
		dev->hclk = NULL;
	else
		clk_prepare_enable(dev->hclk);

	ret = msm_slim_sps_init(dev, bam_mem, MGR_STATUS, false);
	if (ret != 0) {
		dev_err(dev->dev, "error SPS init\n");
		goto err_sps_init_failed;
	}

	/* Fire up the Rx message queue thread */
	dev->rx_msgq_thread = kthread_run(msm_slim_rx_msgq_thread, dev,
					MSM_SLIM_NAME "_rx_msgq_thread");
	if (IS_ERR(dev->rx_msgq_thread)) {
		ret = PTR_ERR(dev->rx_msgq_thread);
		dev_err(dev->dev, "Failed to start Rx message queue thread\n");
		goto err_thread_create_failed;
	}

	dev->framer.rootfreq = SLIM_ROOT_FREQ >> 3;
	dev->framer.superfreq =
		dev->framer.rootfreq / SLIM_CL_PER_SUPERFRAME_DIV8;
	dev->ctrl.a_framer = &dev->framer;
	dev->ctrl.clkgear = SLIM_MAX_CLK_GEAR;
	dev->ctrl.dev.parent = &pdev->dev;
	dev->ctrl.dev.of_node = pdev->dev.of_node;

	ret = request_irq(dev->irq, msm_slim_interrupt, IRQF_TRIGGER_HIGH,
				"msm_slim_irq", dev);
	if (ret) {
		dev_err(&pdev->dev, "request IRQ failed\n");
		goto err_request_irq_failed;
	}

	msm_slim_prg_slew(pdev, dev);

	/* Register with framework before enabling frame, clock */
	ret = slim_add_numbered_controller(&dev->ctrl);
	if (ret) {
		dev_err(dev->dev, "error adding controller\n");
		goto err_ctrl_failed;
	}


	dev->rclk = clk_get(dev->dev, "core_clk");
	if (!dev->rclk) {
		dev_err(dev->dev, "slimbus clock not found");
		goto err_clk_get_failed;
	}
	clk_set_rate(dev->rclk, SLIM_ROOT_FREQ);
	clk_prepare_enable(dev->rclk);

	dev->ver = readl_relaxed(dev->base);
	/* Version info in 16 MSbits */
	dev->ver >>= 16;
	/* Component register initialization */
	writel_relaxed(1, dev->base + CFG_PORT(COMP_CFG, dev->ver));
	writel_relaxed((EE_MGR_RSC_GRP | EE_NGD_2 | EE_NGD_1),
				dev->base + CFG_PORT(COMP_TRUST_CFG, dev->ver));

	/*
	 * Manager register initialization
	 * If RX msg Q is used, disable RX_MSG_RCVD interrupt
	 */
	if (dev->use_rx_msgqs == MSM_MSGQ_ENABLED)
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

	/* Framer register initialization */
	writel_relaxed((1 << INTR_WAKE) | (0xA << REF_CLK_GEAR) |
		(0xA << CLK_GEAR) | (1 << ROOT_FREQ) | (1 << FRM_ACTIVE) | 1,
		dev->base + FRM_CFG);
	/*
	 * Make sure that framer wake-up and enabling writes go through
	 * before any other component is enabled. Framer is responsible for
	 * clocking the bus and enabling framer first will ensure that other
	 * devices can report presence when they are enabled
	 */
	mb();

	/* Enable RX msg Q */
	if (dev->use_rx_msgqs == MSM_MSGQ_ENABLED)
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
	writel_relaxed(1, dev->base + CFG_PORT(PGD_CFG, dev->ver));
	writel_relaxed(0x3F<<17, dev->base + CFG_PORT(PGD_OWN_EEn, dev->ver) +
				(4 * dev->ee));
	/*
	 * Make sure that ported generic device is enabled and port-EE settings
	 * are written through before finally enabling the component
	 */
	mb();

	writel_relaxed(1, dev->base + CFG_PORT(COMP_CFG, dev->ver));
	/*
	 * Make sure that all writes have gone through before exiting this
	 * function
	 */
	mb();

	/* Add devices registered with board-info now that controller is up */
	slim_ctrl_add_boarddevs(&dev->ctrl);

	if (pdev->dev.of_node)
		of_register_slim_devices(&dev->ctrl);

	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, MSM_SLIM_AUTOSUSPEND);
	pm_runtime_set_active(&pdev->dev);

	dev_dbg(dev->dev, "MSM SB controller is up!\n");
	return 0;

err_ctrl_failed:
	writel_relaxed(0, dev->base + CFG_PORT(COMP_CFG, dev->ver));
err_clk_get_failed:
	kfree(dev->satd);
err_request_irq_failed:
	kthread_stop(dev->rx_msgq_thread);
err_thread_create_failed:
	msm_slim_sps_exit(dev, true);
err_sps_init_failed:
	if (dev->hclk) {
		clk_disable_unprepare(dev->hclk);
		clk_put(dev->hclk);
	}
err_of_init_failed:
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
	int i;
	for (i = 0; i < dev->nsats; i++) {
		struct msm_slim_sat *sat = dev->satd[i];
		int j;
		for (j = 0; j < sat->nsatch; j++)
			slim_dealloc_ch(&sat->satcl, sat->satch[j].chanh);
		slim_remove_device(&sat->satcl);
		kfree(sat->satch);
		destroy_workqueue(sat->wq);
		kfree(sat->satcl.name);
		kfree(sat);
	}
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	free_irq(dev->irq, dev);
	slim_del_controller(&dev->ctrl);
	clk_put(dev->rclk);
	if (dev->hclk)
		clk_put(dev->hclk);
	msm_slim_sps_exit(dev, true);
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
		struct platform_device *pdev = to_platform_device(dev);
		struct msm_slim_ctrl *cdev = platform_get_drvdata(pdev);
		dev_dbg(dev, "system suspend");
		ret = msm_slim_runtime_suspend(dev);
		if (!ret) {
			if (cdev->hclk)
				clk_disable_unprepare(cdev->hclk);
		}
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
		struct platform_device *pdev = to_platform_device(dev);
		struct msm_slim_ctrl *cdev = platform_get_drvdata(pdev);
		int ret;
		dev_dbg(dev, "system resume");
		if (cdev->hclk)
			clk_prepare_enable(cdev->hclk);
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

static struct of_device_id msm_slim_dt_match[] = {
	{
		.compatible = "qcom,slim-msm",
	},
	{}
};

static struct platform_driver msm_slim_driver = {
	.probe = msm_slim_probe,
	.remove = msm_slim_remove,
	.driver	= {
		.name = MSM_SLIM_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_slim_dev_pm_ops,
		.of_match_table = msm_slim_dt_match,
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
