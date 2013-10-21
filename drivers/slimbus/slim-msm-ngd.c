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
#include <linux/dma-mapping.h>
#include <linux/slimbus/slimbus.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_slimbus.h>
#include <linux/timer.h>
#include <mach/sps.h>
#include "slim-msm.h"
#include <mach/qdsp6v2/apr.h>

#define NGD_SLIM_NAME	"ngd_msm_ctrl"
#define SLIM_LA_MGR	0xFF
#define SLIM_ROOT_FREQ	24576000
#define LADDR_RETRY	5

#define NGD_BASE_V1(r)	(((r) % 2) ? 0x800 : 0xA00)
#define NGD_BASE_V2(r)	(((r) % 2) ? 0x1000 : 0x2000)
#define NGD_BASE(r, v) ((v) ? NGD_BASE_V2(r) : NGD_BASE_V1(r))
/* NGD (Non-ported Generic Device) registers */
enum ngd_reg {
	NGD_CFG		= 0x0,
	NGD_STATUS	= 0x4,
	NGD_RX_MSGQ_CFG	= 0x8,
	NGD_INT_EN	= 0x10,
	NGD_INT_STAT	= 0x14,
	NGD_INT_CLR	= 0x18,
	NGD_TX_MSG	= 0x30,
	NGD_RX_MSG	= 0x70,
	NGD_IE_STAT	= 0xF0,
	NGD_VE_STAT	= 0x100,
};

enum ngd_msg_cfg {
	NGD_CFG_ENABLE		= 1,
	NGD_CFG_RX_MSGQ_EN	= 1 << 1,
	NGD_CFG_TX_MSGQ_EN	= 1 << 2,
};

enum ngd_intr {
	NGD_INT_RECFG_DONE	= 1 << 24,
	NGD_INT_TX_NACKED_2	= 1 << 25,
	NGD_INT_MSG_BUF_CONTE	= 1 << 26,
	NGD_INT_MSG_TX_INVAL	= 1 << 27,
	NGD_INT_IE_VE_CHG	= 1 << 28,
	NGD_INT_DEV_ERR		= 1 << 29,
	NGD_INT_RX_MSG_RCVD	= 1 << 30,
	NGD_INT_TX_MSG_SENT	= 1 << 31,
};

enum ngd_offsets {
	NGD_NACKED_MC		= 0x7F00000,
	NGD_ACKED_MC		= 0xFE000,
	NGD_ERROR		= 0x1800,
	NGD_MSGQ_SUPPORT	= 0x400,
	NGD_RX_MSGQ_TIME_OUT	= 0x16,
	NGD_ENUMERATED		= 0x1,
	NGD_TX_BUSY		= 0x0,
};

enum ngd_status {
	NGD_LADDR		= 1 << 1,
};

static int ngd_slim_runtime_resume(struct device *device);

static irqreturn_t ngd_slim_interrupt(int irq, void *d)
{
	struct msm_slim_ctrl *dev = (struct msm_slim_ctrl *)d;
	void __iomem *ngd = dev->base + NGD_BASE(dev->ctrl.nr, dev->ver);
	u32 stat = readl_relaxed(ngd + NGD_INT_STAT);
	u32 pstat;

	if (stat & NGD_INT_TX_MSG_SENT) {
		writel_relaxed(NGD_INT_TX_MSG_SENT, ngd + NGD_INT_CLR);
		/* Make sure interrupt is cleared */
		mb();
		if (dev->wr_comp)
			complete(dev->wr_comp);
	} else if ((stat & NGD_INT_MSG_BUF_CONTE) ||
		(stat & NGD_INT_MSG_TX_INVAL) || (stat & NGD_INT_DEV_ERR) ||
		(stat & NGD_INT_TX_NACKED_2)) {
		dev_err(dev->dev, "NGD interrupt error:0x%x", stat);
		writel_relaxed(stat, ngd + NGD_INT_CLR);
		/* Guarantee that error interrupts are cleared */
		mb();
		if (((stat & NGD_INT_TX_NACKED_2) ||
			(stat & NGD_INT_MSG_TX_INVAL))) {
			dev->err = -EIO;
		if (dev->wr_comp)
			complete(dev->wr_comp);
		}
	}
	if (stat & NGD_INT_RX_MSG_RCVD) {
		u32 rx_buf[10];
		u8 len, i;
		rx_buf[0] = readl_relaxed(ngd + NGD_RX_MSG);
		len = rx_buf[0] & 0x1F;
		for (i = 1; i < ((len + 3) >> 2); i++) {
			rx_buf[i] = readl_relaxed(ngd + NGD_RX_MSG +
						(4 * i));
			dev_dbg(dev->dev, "REG-RX data: %x\n", rx_buf[i]);
		}
		msm_slim_rx_enqueue(dev, rx_buf, len);
		writel_relaxed(NGD_INT_RX_MSG_RCVD,
				ngd + NGD_INT_CLR);
		/*
		 * Guarantee that CLR bit write goes through before
		 * queuing work
		 */
		mb();
		if (dev->use_rx_msgqs == MSM_MSGQ_ENABLED)
			dev_err(dev->dev,
				"direct message received even with RX MSGQs");
		else
			complete(&dev->rx_msgq_notify);
	}
	if (stat & NGD_INT_RECFG_DONE) {
		writel_relaxed(NGD_INT_RECFG_DONE, ngd + NGD_INT_CLR);
		/* Guarantee RECONFIG DONE interrupt is cleared */
		mb();
		/* In satellite mode, just log the reconfig done IRQ */
		dev_dbg(dev->dev, "reconfig done IRQ for NGD");
	}
	if (stat & NGD_INT_IE_VE_CHG) {
		writel_relaxed(NGD_INT_IE_VE_CHG, ngd + NGD_INT_CLR);
		/* Guarantee IE VE change interrupt is cleared */
		mb();
		dev_err(dev->dev, "NGD IE VE change");
	}

	pstat = readl_relaxed(PGD_THIS_EE(PGD_PORT_INT_ST_EEn, dev->ver));
	if (pstat != 0)
		return msm_slim_port_irq_handler(dev, pstat);
	return IRQ_HANDLED;
}

static int ngd_qmi_available(struct notifier_block *n, unsigned long code,
				void *_cmd)
{
	struct msm_slim_qmi *qmi = container_of(n, struct msm_slim_qmi, nb);
	struct msm_slim_ctrl *dev =
		container_of(qmi, struct msm_slim_ctrl, qmi);
	pr_info("Slimbus QMI NGD CB received event:%ld", code);
	switch (code) {
	case QMI_SERVER_ARRIVE:
		schedule_work(&qmi->ssr_up);
		break;
	case QMI_SERVER_EXIT:
		dev->state = MSM_CTRL_DOWN;
		/* make sure autosuspend is not called until ADSP comes up*/
		pm_runtime_get_noresume(dev->dev);
		/* Reset ctrl_up completion */
		init_completion(&dev->ctrl_up);
		schedule_work(&qmi->ssr_down);
		break;
	default:
		break;
	}
	return 0;
}

static int ngd_get_tid(struct slim_controller *ctrl, struct slim_msg_txn *txn,
				u8 *tid, struct completion *done)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	mutex_lock(&ctrl->m_ctrl);
	if (ctrl->last_tid <= 255) {
		ctrl->txnt = krealloc(ctrl->txnt,
				(ctrl->last_tid + 1) *
				sizeof(struct slim_msg_txn *),
				GFP_KERNEL);
		if (!ctrl->txnt) {
			mutex_unlock(&ctrl->m_ctrl);
			return -ENOMEM;
		}
		dev->msg_cnt = ctrl->last_tid;
		ctrl->last_tid++;
	} else {
		int i;
		for (i = 0; i < 256; i++) {
			dev->msg_cnt = ((dev->msg_cnt + 1) & 0xFF);
			if (ctrl->txnt[dev->msg_cnt] == NULL)
				break;
		}
		if (i >= 256) {
			dev_err(&ctrl->dev, "out of TID");
			mutex_unlock(&ctrl->m_ctrl);
			return -ENOMEM;
		}
	}
	ctrl->txnt[dev->msg_cnt] = txn;
	txn->tid = dev->msg_cnt;
	txn->comp = done;
	*tid = dev->msg_cnt;
	mutex_unlock(&ctrl->m_ctrl);
	return 0;
}
static int ngd_xfer_msg(struct slim_controller *ctrl, struct slim_msg_txn *txn)
{
	DECLARE_COMPLETION_ONSTACK(done);
	DECLARE_COMPLETION_ONSTACK(tx_sent);

	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	u32 *pbuf;
	u8 *puc;
	int ret = 0;
	u8 la = txn->la;
	u8 txn_mt;
	u16 txn_mc = txn->mc;
	u8 wbuf[SLIM_MSGQ_BUF_LEN];

	if (!pm_runtime_enabled(dev->dev) && dev->state == MSM_CTRL_ASLEEP &&
			txn->mc != SLIM_USR_MC_REPORT_SATELLITE) {
		/*
		 * Counter-part of system-suspend when runtime-pm is not enabled
		 * This way, resume can be left empty and device will be put in
		 * active mode only if client requests anything on the bus
		 * If the state was DOWN, SSR UP notification will take
		 * care of putting the device in active state.
		 */
		ngd_slim_runtime_resume(dev->dev);
	}
	if ((txn->mc == (SLIM_MSG_CLK_PAUSE_SEQ_FLG |
			SLIM_MSG_MC_RECONFIGURE_NOW)) &&
			dev->state <= MSM_CTRL_SLEEPING) {
		msm_slim_disconnect_endp(dev, &dev->rx_msgq,
					&dev->use_rx_msgqs);
		msm_slim_disconnect_endp(dev, &dev->tx_msgq,
					&dev->use_tx_msgqs);
		return msm_slim_qmi_power_request(dev, false);
	}
	else if (txn->mc & SLIM_MSG_CLK_PAUSE_SEQ_FLG)
		return 0;

	if (txn->mt == SLIM_MSG_MT_CORE &&
		(txn->mc >= SLIM_MSG_MC_BEGIN_RECONFIGURATION &&
		 txn->mc <= SLIM_MSG_MC_RECONFIGURE_NOW)) {
		return 0;
	}
	/* If txn is tried when controller is down, wait for ADSP to boot */
	if (txn->mc != SLIM_USR_MC_REPORT_SATELLITE) {
		if (dev->state == MSM_CTRL_DOWN) {
			u8 mc = (u8)txn->mc;
			int timeout;
			dev_err(dev->dev, "ADSP slimbus not up yet");
			/*
			 * Messages related to data channel management can't
			 * wait since they are holding reconfiguration lock.
			 * clk_pause in resume (which can change state back to
			 * MSM_CTRL_AWAKE), will need that lock
			 */
			if ((txn->mt == SLIM_MSG_MT_CORE) &&
				((mc >= SLIM_MSG_MC_CONNECT_SOURCE &&
				mc <= SLIM_MSG_MC_CHANGE_CONTENT) ||
				(mc >= SLIM_MSG_MC_BEGIN_RECONFIGURATION &&
				mc <= SLIM_MSG_MC_RECONFIGURE_NOW)))
				return -EREMOTEIO;
			if ((txn->mt == SLIM_MSG_MT_DEST_REFERRED_USER) &&
				((mc >= SLIM_USR_MC_DEFINE_CHAN &&
				mc <= SLIM_USR_MC_DISCONNECT_PORT)))
				return -EREMOTEIO;
			timeout = wait_for_completion_timeout(&dev->ctrl_up,
							HZ);
			if (!timeout)
				return -ETIMEDOUT;
		}
		msm_slim_get_ctrl(dev);
	}
	mutex_lock(&dev->tx_lock);

	if (txn->mc != SLIM_USR_MC_REPORT_SATELLITE &&
		(dev->state != MSM_CTRL_AWAKE)) {
		dev_err(dev->dev, "controller not ready");
		mutex_unlock(&dev->tx_lock);
		msm_slim_put_ctrl(dev);
		return -EREMOTEIO;
	}
	if (txn->mt == SLIM_MSG_MT_CORE &&
		(txn->mc == SLIM_MSG_MC_CONNECT_SOURCE ||
		txn->mc == SLIM_MSG_MC_CONNECT_SINK ||
		txn->mc == SLIM_MSG_MC_DISCONNECT_PORT)) {
		int i = 0;
		txn->mt = SLIM_MSG_MT_DEST_REFERRED_USER;
		if (txn->mc == SLIM_MSG_MC_CONNECT_SOURCE)
			txn->mc = SLIM_USR_MC_CONNECT_SRC;
		else if (txn->mc == SLIM_MSG_MC_CONNECT_SINK)
			txn->mc = SLIM_USR_MC_CONNECT_SINK;
		else if (txn->mc == SLIM_MSG_MC_DISCONNECT_PORT)
			txn->mc = SLIM_USR_MC_DISCONNECT_PORT;
		if (txn->la == SLIM_LA_MGR) {
			if (dev->pgdla == SLIM_LA_MGR) {
				u8 ea[] = {0, QC_DEVID_PGD, 0, 0, QC_MFGID_MSB,
						QC_MFGID_LSB};
				ea[2] = (u8)(dev->pdata.eapc & 0xFF);
				ea[3] = (u8)((dev->pdata.eapc & 0xFF00) >> 8);
				mutex_unlock(&dev->tx_lock);
				ret = dev->ctrl.get_laddr(&dev->ctrl, ea, 6,
						&dev->pgdla);
				pr_debug("SLIM PGD LA:0x%x, ret:%d", dev->pgdla,
						ret);
				if (ret) {
					pr_err("Incorrect SLIM-PGD EAPC:0x%x",
							dev->pdata.eapc);
					return ret;
				}
				mutex_lock(&dev->tx_lock);
			}
			txn->la = dev->pgdla;
		}
		wbuf[i++] = txn->la;
		la = SLIM_LA_MGR;
		wbuf[i++] = txn->wbuf[0];
		if (txn->mc != SLIM_USR_MC_DISCONNECT_PORT)
			wbuf[i++] = txn->wbuf[1];
		ret = ngd_get_tid(ctrl, txn, &wbuf[i++], &done);
		if (ret) {
			pr_err("TID for connect/disconnect fail:%d", ret);
			goto ngd_xfer_err;
		}
		txn->len = i;
		txn->wbuf = wbuf;
		txn->rl = txn->len + 4;
	}
	txn->rl--;
	pbuf = msm_get_msg_buf(dev, txn->rl);
	if (!pbuf) {
		dev_err(dev->dev, "Message buffer unavailable");
		ret = -ENOMEM;
		goto ngd_xfer_err;
	}
	dev->err = 0;

	if (txn->dt == SLIM_MSG_DEST_ENUMADDR) {
		ret = -EPROTONOSUPPORT;
		goto ngd_xfer_err;
	}
	if (txn->dt == SLIM_MSG_DEST_LOGICALADDR)
		*pbuf = SLIM_MSG_ASM_FIRST_WORD(txn->rl, txn->mt, txn->mc, 0,
				la);
	else
		*pbuf = SLIM_MSG_ASM_FIRST_WORD(txn->rl, txn->mt, txn->mc, 1,
				la);
	if (txn->dt == SLIM_MSG_DEST_LOGICALADDR)
		puc = ((u8 *)pbuf) + 3;
	else
		puc = ((u8 *)pbuf) + 2;
	if (txn->rbuf)
		*(puc++) = txn->tid;
	if ((txn->mt == SLIM_MSG_MT_CORE) &&
		((txn->mc >= SLIM_MSG_MC_REQUEST_INFORMATION &&
		txn->mc <= SLIM_MSG_MC_REPORT_INFORMATION) ||
		(txn->mc >= SLIM_MSG_MC_REQUEST_VALUE &&
		 txn->mc <= SLIM_MSG_MC_CHANGE_VALUE))) {
		*(puc++) = (txn->ec & 0xFF);
		*(puc++) = (txn->ec >> 8)&0xFF;
	}
	if (txn->wbuf)
		memcpy(puc, txn->wbuf, txn->len);
	if (txn->mt == SLIM_MSG_MT_DEST_REFERRED_USER &&
		(txn->mc == SLIM_USR_MC_CONNECT_SRC ||
		 txn->mc == SLIM_USR_MC_CONNECT_SINK ||
		 txn->mc == SLIM_USR_MC_DISCONNECT_PORT) && txn->wbuf &&
		wbuf[0] == dev->pgdla) {
		if (txn->mc != SLIM_USR_MC_DISCONNECT_PORT)
			dev->err = msm_slim_connect_pipe_port(dev, wbuf[1]);
		else {
			/*
			 * Remove channel disconnects master-side ports from
			 * channel. No need to send that again on the bus
			 * Only disable port
			 */
			writel_relaxed(0, PGD_PORT(PGD_PORT_CFGn,
					(wbuf[1] + dev->port_b), dev->ver));
			mutex_unlock(&dev->tx_lock);
			msm_slim_put_ctrl(dev);
			return 0;
		}
		if (dev->err) {
			dev_err(dev->dev, "pipe-port connect err:%d", dev->err);
			goto ngd_xfer_err;
		}
		/* Add port-base to port number if this is manager side port */
		puc[1] += dev->port_b;
	}
	dev->err = 0;
	/*
	 * If it's a read txn, it may be freed if a response is received by
	 * received thread before reaching end of this function.
	 * mc, mt may have changed to convert standard slimbus code/type to
	 * satellite user-defined message. Reinitialize again
	 */
	txn_mc = txn->mc;
	txn_mt = txn->mt;
	dev->wr_comp = &tx_sent;
	ret = msm_send_msg_buf(dev, pbuf, txn->rl,
			NGD_BASE(dev->ctrl.nr, dev->ver) + NGD_TX_MSG);
	if (!ret) {
		int timeout = wait_for_completion_timeout(&tx_sent, HZ);
		if (!timeout) {
			ret = -ETIMEDOUT;
			/*
			 * disconnect/recoonect pipe so that subsequent
			 * transactions don't timeout due to unavailable
			 * descriptors
			 */
			msm_slim_disconnect_endp(dev, &dev->tx_msgq,
						&dev->use_tx_msgqs);
			msm_slim_connect_endp(dev, &dev->tx_msgq, NULL);
		} else {
			ret = dev->err;
		}
	}
	dev->wr_comp = NULL;
	if (ret) {
		u32 conf, stat, rx_msgq, int_stat, int_en, int_clr;
		void __iomem *ngd = dev->base + NGD_BASE(dev->ctrl.nr,
							dev->ver);
		dev_err(dev->dev, "TX failed :MC:0x%x,mt:0x%x, ret:%d, ver:%d",
				txn_mc, txn_mt, ret, dev->ver);
		conf = readl_relaxed(ngd);
		stat = readl_relaxed(ngd + NGD_STATUS);
		rx_msgq = readl_relaxed(ngd + NGD_RX_MSGQ_CFG);
		int_stat = readl_relaxed(ngd + NGD_INT_STAT);
		int_en = readl_relaxed(ngd + NGD_INT_EN);
		int_clr = readl_relaxed(ngd + NGD_INT_CLR);

		pr_err("conf:0x%x,stat:0x%x,rxmsgq:0x%x", conf, stat, rx_msgq);
		pr_err("int_stat:0x%x,int_en:0x%x,int_cll:0x%x", int_stat,
						int_en, int_clr);
	} else if (txn_mt == SLIM_MSG_MT_DEST_REFERRED_USER &&
		(txn_mc == SLIM_USR_MC_CONNECT_SRC ||
		 txn_mc == SLIM_USR_MC_CONNECT_SINK ||
		 txn_mc == SLIM_USR_MC_DISCONNECT_PORT)) {
		int timeout;
		mutex_unlock(&dev->tx_lock);
		msm_slim_put_ctrl(dev);
		timeout = wait_for_completion_timeout(txn->comp, HZ);
		if (!timeout)
			ret = -ETIMEDOUT;
		else
			ret = txn->ec;
		if (ret) {
			pr_err("connect/disconnect:0x%x,tid:%d err:%d", txn->mc,
					txn->tid, ret);
			mutex_lock(&ctrl->m_ctrl);
			ctrl->txnt[txn->tid] = NULL;
			mutex_unlock(&ctrl->m_ctrl);
		}
		return ret ? ret : dev->err;
	}
ngd_xfer_err:
	mutex_unlock(&dev->tx_lock);
	if (txn_mc != SLIM_USR_MC_REPORT_SATELLITE)
		msm_slim_put_ctrl(dev);
	return ret ? ret : dev->err;
}

static int ngd_xferandwait_ack(struct slim_controller *ctrl,
				struct slim_msg_txn *txn)
{
	int ret = ngd_xfer_msg(ctrl, txn);
	if (!ret) {
		int timeout;
		timeout = wait_for_completion_timeout(txn->comp, HZ);
		if (!timeout)
			ret = -ETIMEDOUT;
		else
			ret = txn->ec;
	}
	if (ret) {
		pr_err("master msg:0x%x,tid:%d ret:%d", txn->mc,
				txn->tid, ret);
		mutex_lock(&ctrl->m_ctrl);
		ctrl->txnt[txn->tid] = NULL;
		mutex_unlock(&ctrl->m_ctrl);
	}

	return ret;
}

static int ngd_allocbw(struct slim_device *sb, int *subfrmc, int *clkgear)
{
	int ret;
	struct slim_pending_ch *pch;
	struct slim_msg_txn txn;
	struct slim_controller *ctrl = sb->ctrl;
	DECLARE_COMPLETION_ONSTACK(done);
	u8 wbuf[SLIM_MSGQ_BUF_LEN];

	*clkgear = ctrl->clkgear;
	*subfrmc = 0;
	txn.mt = SLIM_MSG_MT_DEST_REFERRED_USER;
	txn.dt = SLIM_MSG_DEST_LOGICALADDR;
	txn.la = SLIM_LA_MGR;
	txn.len = 0;
	txn.ec = 0;
	txn.wbuf = wbuf;
	txn.rbuf = NULL;

	if (ctrl->sched.msgsl != ctrl->sched.pending_msgsl) {
		pr_debug("slim reserve BW for messaging: req: %d",
				ctrl->sched.pending_msgsl);
		txn.mc = SLIM_USR_MC_REQ_BW;
		wbuf[txn.len++] = ((sb->laddr & 0x1f) |
				((u8)(ctrl->sched.pending_msgsl & 0x7) << 5));
		wbuf[txn.len++] = (u8)(ctrl->sched.pending_msgsl >> 3);
		ret = ngd_get_tid(ctrl, &txn, &wbuf[txn.len++], &done);
		if (ret)
			return ret;
		txn.rl = txn.len + 4;
		ret = ngd_xferandwait_ack(ctrl, &txn);
		if (ret)
			return ret;

		txn.mc = SLIM_USR_MC_RECONFIG_NOW;
		txn.len = 2;
		wbuf[1] = sb->laddr;
		txn.rl = txn.len + 4;
		ret = ngd_get_tid(ctrl, &txn, &wbuf[0], &done);
		if (ret)
			return ret;
		ret = ngd_xferandwait_ack(ctrl, &txn);
		if (ret)
			return ret;

		txn.len = 0;
	}
	list_for_each_entry(pch, &sb->mark_define, pending) {
		struct slim_ich *slc;
		slc = &ctrl->chans[pch->chan];
		if (!slc) {
			pr_err("no channel in define?");
			return -ENXIO;
		}
		if (txn.len == 0) {
			/* Per protocol, only last 5 bits for client no. */
			wbuf[txn.len++] = (u8) (slc->prop.dataf << 5) |
					(sb->laddr & 0x1f);
			wbuf[txn.len] = slc->seglen;
			if (slc->coeff == SLIM_COEFF_3)
				wbuf[txn.len] |= 1 << 5;
			wbuf[txn.len++] |= slc->prop.auxf << 6;
			wbuf[txn.len++] = slc->rootexp << 4 | slc->prop.prot;
			wbuf[txn.len++] = slc->prrate;
			ret = ngd_get_tid(ctrl, &txn, &wbuf[txn.len++], &done);
			if (ret) {
				pr_err("no tid for channel define?");
				return -ENXIO;
			}
		}
		wbuf[txn.len++] = slc->chan;
		pr_debug("slim define chan:%d, tid:0x%x", slc->chan, txn.tid);
	}
	if (txn.len) {
		txn.mc = SLIM_USR_MC_DEF_ACT_CHAN;
		txn.rl = txn.len + 4;
		ret = ngd_xferandwait_ack(ctrl, &txn);
		if (ret)
			return ret;

		txn.mc = SLIM_USR_MC_RECONFIG_NOW;
		txn.len = 2;
		wbuf[1] = sb->laddr;
		txn.rl = txn.len + 4;
		ret = ngd_get_tid(ctrl, &txn, &wbuf[0], &done);
		if (ret)
			return ret;
		ret = ngd_xferandwait_ack(ctrl, &txn);
		if (ret)
			return ret;
	}
	txn.len = 0;
	list_for_each_entry(pch, &sb->mark_removal, pending) {
		struct slim_ich *slc;
		slc = &ctrl->chans[pch->chan];
		if (!slc) {
			pr_err("no channel in removal?");
			return -ENXIO;
		}
		if (txn.len == 0) {
			/* Per protocol, only last 5 bits for client no. */
			wbuf[txn.len++] = (u8) (SLIM_CH_REMOVE << 6) |
					(sb->laddr & 0x1f);
			ret = ngd_get_tid(ctrl, &txn, &wbuf[txn.len++], &done);
			if (ret) {
				pr_err("no tid for channel define?");
				return -ENXIO;
			}
		}
		wbuf[txn.len++] = slc->chan;
		pr_debug("slim remove chan:%d, tid:0x%x", slc->chan, txn.tid);
	}
	if (txn.len) {
		txn.mc = SLIM_USR_MC_CHAN_CTRL;
		txn.rl = txn.len + 4;
		ret = ngd_xferandwait_ack(ctrl, &txn);
		if (ret)
			return ret;

		txn.mc = SLIM_USR_MC_RECONFIG_NOW;
		txn.len = 2;
		wbuf[1] = sb->laddr;
		txn.rl = txn.len + 4;
		ret = ngd_get_tid(ctrl, &txn, &wbuf[0], &done);
		if (ret)
			return ret;
		ret = ngd_xferandwait_ack(ctrl, &txn);
		if (ret)
			return ret;
		txn.len = 0;
	}
	return 0;
}

static int ngd_set_laddr(struct slim_controller *ctrl, const u8 *ea,
				u8 elen, u8 laddr)
{
	return 0;
}

static int ngd_get_laddr(struct slim_controller *ctrl, const u8 *ea,
				u8 elen, u8 *laddr)
{
	int ret;
	u8 wbuf[10];
	struct slim_msg_txn txn;
	DECLARE_COMPLETION_ONSTACK(done);
	txn.mt = SLIM_MSG_MT_DEST_REFERRED_USER;
	txn.dt = SLIM_MSG_DEST_LOGICALADDR;
	txn.la = SLIM_LA_MGR;
	txn.ec = 0;
	ret = ngd_get_tid(ctrl, &txn, &wbuf[0], &done);
	if (ret) {
		return ret;
	}
	memcpy(&wbuf[1], ea, elen);
	txn.mc = SLIM_USR_MC_ADDR_QUERY;
	txn.rl = 11;
	txn.len = 7;
	txn.wbuf = wbuf;
	txn.rbuf = NULL;
	ret = ngd_xferandwait_ack(ctrl, &txn);
	if (!ret && txn.la == 0xFF)
		ret = -ENXIO;
	else if (!ret)
		*laddr = txn.la;
	return ret;
}

static void ngd_slim_setup_msg_path(struct msm_slim_ctrl *dev)
{
	if (dev->state == MSM_CTRL_DOWN) {
		msm_slim_sps_init(dev, dev->bam_mem,
			NGD_BASE(dev->ctrl.nr,
			dev->ver) + NGD_STATUS, true);
	} else {
		if (dev->use_rx_msgqs == MSM_MSGQ_DISABLED)
			goto setup_tx_msg_path;
		msm_slim_connect_endp(dev, &dev->rx_msgq,
				&dev->rx_msgq_notify);

setup_tx_msg_path:
		if (dev->use_tx_msgqs == MSM_MSGQ_DISABLED)
			return;
		msm_slim_connect_endp(dev, &dev->tx_msgq,
				NULL);
	}
}

static void ngd_slim_rx(struct msm_slim_ctrl *dev, u8 *buf)
{
	u8 mc, mt, len;
	int ret;
	u32 msgq_en = 1;

	len = buf[0] & 0x1F;
	mt = (buf[0] >> 5) & 0x7;
	mc = buf[1];
	if (mc == SLIM_USR_MC_MASTER_CAPABILITY &&
		mt == SLIM_MSG_MT_SRC_REFERRED_USER) {
		struct slim_msg_txn txn;
		int retries = 0;
		u8 wbuf[8];
		txn.dt = SLIM_MSG_DEST_LOGICALADDR;
		txn.ec = 0;
		txn.rbuf = NULL;
		txn.mc = SLIM_USR_MC_REPORT_SATELLITE;
		txn.mt = SLIM_MSG_MT_SRC_REFERRED_USER;
		txn.la = SLIM_LA_MGR;
		wbuf[0] = SAT_MAGIC_LSB;
		wbuf[1] = SAT_MAGIC_MSB;
		wbuf[2] = SAT_MSG_VER;
		wbuf[3] = SAT_MSG_PROT;
		txn.wbuf = wbuf;
		txn.len = 4;
		pr_info("SLIM SAT: Received master capability");
		if (dev->state >= MSM_CTRL_ASLEEP) {
			ngd_slim_setup_msg_path(dev);
			if (dev->use_rx_msgqs == MSM_MSGQ_ENABLED)
				msgq_en |= NGD_CFG_RX_MSGQ_EN;
			if (dev->use_tx_msgqs == MSM_MSGQ_ENABLED)
				msgq_en |= NGD_CFG_TX_MSGQ_EN;
			writel_relaxed(msgq_en, dev->base +
					NGD_BASE(dev->ctrl.nr, dev->ver));
			/* make sure NGD MSG-Q config goes through */
			mb();
		}
capability_retry:
		txn.rl = 8;
		ret = ngd_xfer_msg(&dev->ctrl, &txn);
		if (!ret) {
			enum msm_ctrl_state prev_state = dev->state;
			dev->state = MSM_CTRL_AWAKE;
			if (prev_state >= MSM_CTRL_ASLEEP)
				complete(&dev->reconf);
			else
				pr_err("SLIM: unexpected capability, state:%d",
					prev_state);
			/* ADSP SSR, send device_up notifications */
			if (prev_state == MSM_CTRL_DOWN)
				schedule_work(&dev->slave_notify);
		} else if (ret == -EIO) {
			pr_info("capability message NACKed, retrying");
			if (retries < INIT_MX_RETRIES) {
				msleep(DEF_RETRY_MS);
				retries++;
				goto capability_retry;
			}
		}
	}
	if (mc == SLIM_MSG_MC_REPLY_INFORMATION ||
			mc == SLIM_MSG_MC_REPLY_VALUE) {
		u8 tid = buf[3];
		dev_dbg(dev->dev, "tid:%d, len:%d\n", tid, len);
		slim_msg_response(&dev->ctrl, &buf[4], tid,
					len - 4);
		pm_runtime_mark_last_busy(dev->dev);
	}
	if (mc == SLIM_USR_MC_ADDR_REPLY &&
		mt == SLIM_MSG_MT_SRC_REFERRED_USER) {
		struct slim_msg_txn *txn;
		u8 failed_ea[6] = {0, 0, 0, 0, 0, 0};
		mutex_lock(&dev->ctrl.m_ctrl);
		txn = dev->ctrl.txnt[buf[3]];
		if (!txn) {
			pr_err("LADDR response after timeout, tid:0x%x",
				buf[3]);
			mutex_unlock(&dev->ctrl.m_ctrl);
			return;
		}
		if (memcmp(&buf[4], failed_ea, 6))
			txn->la = buf[10];
		dev->ctrl.txnt[buf[3]] = NULL;
		mutex_unlock(&dev->ctrl.m_ctrl);
		complete(txn->comp);
	}
	if (mc == SLIM_USR_MC_GENERIC_ACK &&
		mt == SLIM_MSG_MT_SRC_REFERRED_USER) {
		struct slim_msg_txn *txn;
		mutex_lock(&dev->ctrl.m_ctrl);
		txn = dev->ctrl.txnt[buf[3]];
		if (!txn) {
			pr_err("ACK received after timeout, tid:0x%x",
				buf[3]);
			mutex_unlock(&dev->ctrl.m_ctrl);
			return;
		}
		dev_dbg(dev->dev, "got response:tid:%d, response:0x%x",
				(int)buf[3], buf[4]);
		if (!(buf[4] & MSM_SAT_SUCCSS)) {
			dev_err(dev->dev, "TID:%d, NACK code:0x%x", (int)buf[3],
						buf[4]);
			txn->ec = -EIO;
		}
		dev->ctrl.txnt[buf[3]] = NULL;
		mutex_unlock(&dev->ctrl.m_ctrl);
		complete(txn->comp);
	}
}

static int ngd_slim_power_up(struct msm_slim_ctrl *dev)
{
	void __iomem *ngd;
	int timeout, ret;
	enum msm_ctrl_state cur_state = dev->state;
	u32 laddr;
	u32 ngd_int = (NGD_INT_TX_NACKED_2 |
			NGD_INT_MSG_BUF_CONTE | NGD_INT_MSG_TX_INVAL |
			NGD_INT_IE_VE_CHG | NGD_INT_DEV_ERR |
			NGD_INT_TX_MSG_SENT | NGD_INT_RX_MSG_RCVD);

	if (cur_state == MSM_CTRL_DOWN) {
		int timeout = wait_for_completion_timeout(&dev->qmi.qmi_comp,
						HZ);
		if (!timeout)
			pr_err("slimbus QMI init timed out");
	}

	ret = msm_slim_qmi_power_request(dev, true);
	if (ret) {
		pr_err("SLIM QMI power request failed:%d", ret);
		return ret;
	}
	if (!dev->ver) {
		dev->ver = readl_relaxed(dev->base);
		/* Version info in 16 MSbits */
		dev->ver >>= 16;
	}
	ngd = dev->base + NGD_BASE(dev->ctrl.nr, dev->ver);
	laddr = readl_relaxed(ngd + NGD_STATUS);
	if (laddr & NGD_LADDR) {
		/*
		 * ADSP power collapse case, where HW wasn't reset.
		 * Reconnect BAM pipes if disconnected
		 */
		ngd_slim_setup_msg_path(dev);
		return 0;
	} else if (cur_state != MSM_CTRL_DOWN) {
		pr_info("ADSP P.C. CTRL state:%d NGD not enumerated:0x%x",
					dev->state, laddr);
	}
	/* ADSP SSR scenario, need to disconnect pipe before connecting */
	if (dev->use_rx_msgqs == MSM_MSGQ_DOWN) {
		struct msm_slim_endp *endpoint = &dev->rx_msgq;
		sps_disconnect(endpoint->sps);
		sps_free_endpoint(endpoint->sps);
		dev->use_rx_msgqs = MSM_MSGQ_RESET;
	}
	if (dev->use_tx_msgqs == MSM_MSGQ_DOWN) {
		struct msm_slim_endp *endpoint = &dev->tx_msgq;
		sps_disconnect(endpoint->sps);
		sps_free_endpoint(endpoint->sps);
		dev->use_tx_msgqs = MSM_MSGQ_RESET;
	}
	/*
	 * ADSP power collapse case (OR SSR), where HW was reset
	 * BAM programming will happen when capability message is received
	 */
	writel_relaxed(ngd_int, dev->base + NGD_INT_EN +
				NGD_BASE(dev->ctrl.nr, dev->ver));
	/*
	 * Enable NGD. Configure NGD in register acc. mode until master
	 * announcement is received
	 */
	writel_relaxed(1, dev->base + NGD_BASE(dev->ctrl.nr, dev->ver));
	/* make sure NGD enabling goes through */
	mb();

	timeout = wait_for_completion_timeout(&dev->reconf, HZ);
	if (!timeout) {
		pr_err("failed to received master capability");
		return -ETIMEDOUT;
	}
	if (cur_state == MSM_CTRL_DOWN)
		complete(&dev->ctrl_up);
	return 0;
}

static int ngd_slim_enable(struct msm_slim_ctrl *dev, bool enable)
{
	int ret = 0;
	if (enable) {
		ret = msm_slim_qmi_init(dev, false);
		/* controller state should be in sync with framework state */
		if (!ret) {
			ret = slim_ctrl_clk_pause(&dev->ctrl, false,
						SLIM_CLK_UNSPECIFIED);
			complete(&dev->qmi.qmi_comp);
			/*
			 * Power-up won't be called if clock pause failed.
			 * This can happen if ADSP SSR happened when audio
			 * session is in progress. Framework will think that
			 * clock pause failed so no need to wakeup controller.
			 * Call power-up explicitly in that case, since slimbus
			 * HW needs to be powered-on to be in sync with
			 * framework state
			 */
			if (ret)
				ngd_slim_power_up(dev);
			if (!pm_runtime_enabled(dev->dev) ||
					!pm_runtime_suspended(dev->dev))
				ngd_slim_runtime_resume(dev->dev);
			else
				pm_runtime_resume(dev->dev);
			pm_runtime_mark_last_busy(dev->dev);
			pm_runtime_put(dev->dev);
		} else
			dev_err(dev->dev, "qmi init fail, ret:%d, state:%d",
					ret, dev->state);
	} else {
		msm_slim_qmi_exit(dev);
	}

	return ret;
}

static int ngd_clk_pause_wakeup(struct slim_controller *ctrl)
{
	struct msm_slim_ctrl *dev = slim_get_ctrldata(ctrl);
	return ngd_slim_power_up(dev);
}

static int ngd_slim_rx_msgq_thread(void *data)
{
	struct msm_slim_ctrl *dev = (struct msm_slim_ctrl *)data;
	struct completion *notify = &dev->rx_msgq_notify;
	int ret = 0, index = 0;
	u32 mc = 0;
	u32 mt = 0;
	u32 buffer[10];
	u8 msg_len = 0;

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		ret = wait_for_completion_interruptible(notify);
		if (ret) {
			dev_err(dev->dev, "rx thread wait err:%d", ret);
			continue;
		}
		/* 1 irq notification per message */
		if (dev->use_rx_msgqs != MSM_MSGQ_ENABLED) {
			msm_slim_rx_dequeue(dev, (u8 *)buffer);
			ngd_slim_rx(dev, (u8 *)buffer);
			continue;
		}
		ret = msm_slim_rx_msgq_get(dev, buffer, index);
		if (ret) {
			dev_err(dev->dev, "rx_msgq_get() failed 0x%x\n", ret);
			continue;
		}

		/* Wait for complete message */
		if (index++ == 0) {
			msg_len = *buffer & 0x1F;
			mt = (buffer[0] >> 5) & 0x7;
			mc = (buffer[0] >> 8) & 0xff;
			dev_dbg(dev->dev, "MC: %x, MT: %x\n", mc, mt);
		}
		if ((index * 4) >= msg_len) {
			index = 0;
			ngd_slim_rx(dev, (u8 *)buffer);
		} else
			continue;
	}
	return 0;
}

static void ngd_laddr_lookup(struct work_struct *work)
{
	struct msm_slim_ctrl *dev =
		container_of(work, struct msm_slim_ctrl, slave_notify);
	struct slim_controller *ctrl = &dev->ctrl;
	struct slim_device *sbdev;
	int i;
	mutex_lock(&ctrl->m_ctrl);
	list_for_each_entry(sbdev, &ctrl->devs, dev_list) {
		int ret = 0;
		mutex_unlock(&ctrl->m_ctrl);
		for (i = 0; i < LADDR_RETRY; i++) {
			ret = slim_get_logical_addr(sbdev, sbdev->e_addr,
					6, &sbdev->laddr);
			if (!ret)
				break;
			else /* time for ADSP to assign LA */
				msleep(20);
		}
		mutex_lock(&ctrl->m_ctrl);
	}
	mutex_unlock(&ctrl->m_ctrl);
}

static void ngd_adsp_down(struct work_struct *work)
{
	struct msm_slim_qmi *qmi =
		container_of(work, struct msm_slim_qmi, ssr_down);
	struct msm_slim_ctrl *dev =
		container_of(qmi, struct msm_slim_ctrl, qmi);
	struct slim_controller *ctrl = &dev->ctrl;
	struct slim_device *sbdev;

	ngd_slim_enable(dev, false);
	/* disconnect BAM pipes */
	if (dev->use_rx_msgqs == MSM_MSGQ_ENABLED)
		dev->use_rx_msgqs = MSM_MSGQ_DOWN;
	if (dev->use_tx_msgqs == MSM_MSGQ_ENABLED)
		dev->use_tx_msgqs = MSM_MSGQ_DOWN;
	msm_slim_sps_exit(dev, false);
	/* device up should be called again after SSR */
	list_for_each_entry(sbdev, &ctrl->devs, dev_list)
		slim_report_absent(sbdev);
	pr_info("SLIM ADSP SSR (DOWN) done");
}

static void ngd_adsp_up(struct work_struct *work)
{
	struct msm_slim_qmi *qmi =
		container_of(work, struct msm_slim_qmi, ssr_up);
	struct msm_slim_ctrl *dev =
		container_of(qmi, struct msm_slim_ctrl, qmi);
	ngd_slim_enable(dev, true);
}

static int __devinit ngd_slim_probe(struct platform_device *pdev)
{
	struct msm_slim_ctrl *dev;
	int ret;
	struct resource		*bam_mem;
	struct resource		*slim_mem;
	struct resource		*irq, *bam_irq;
	enum apr_subsys_state q6_state;
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
	bam_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"slimbus_bam_physical");
	if (!bam_mem) {
		dev_err(&pdev->dev, "no slimbus BAM memory resource\n");
		return -ENODEV;
	}
	irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"slimbus_irq");
	if (!irq) {
		dev_err(&pdev->dev, "no slimbus IRQ resource\n");
		return -ENODEV;
	}
	bam_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"slimbus_bam_irq");
	if (!bam_irq) {
		dev_err(&pdev->dev, "no slimbus BAM IRQ resource\n");
		return -ENODEV;
	}

	dev = kzalloc(sizeof(struct msm_slim_ctrl), GFP_KERNEL);
	if (IS_ERR_OR_NULL(dev)) {
		dev_err(&pdev->dev, "no memory for MSM slimbus controller\n");
		return PTR_ERR(dev);
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
			goto err_ctrl_failed;
		}
		rxreg_access = of_property_read_bool(pdev->dev.of_node,
					"qcom,rxreg-access");
		of_property_read_u32(pdev->dev.of_node, "qcom,apps-ch-pipes",
					&dev->pdata.apps_pipes);
		of_property_read_u32(pdev->dev.of_node, "qcom,ea-pc",
					&dev->pdata.eapc);
	} else {
		dev->ctrl.nr = pdev->id;
	}
	/*
	 * Keep PGD's logical address as manager's. Query it when first data
	 * channel request comes in
	 */
	dev->pgdla = SLIM_LA_MGR;
	dev->ctrl.nchans = MSM_SLIM_NCHANS;
	dev->ctrl.nports = MSM_SLIM_NPORTS;
	dev->framer.rootfreq = SLIM_ROOT_FREQ >> 3;
	dev->framer.superfreq =
		dev->framer.rootfreq / SLIM_CL_PER_SUPERFRAME_DIV8;
	dev->ctrl.a_framer = &dev->framer;
	dev->ctrl.clkgear = SLIM_MAX_CLK_GEAR;
	dev->ctrl.set_laddr = ngd_set_laddr;
	dev->ctrl.get_laddr = ngd_get_laddr;
	dev->ctrl.allocbw = ngd_allocbw;
	dev->ctrl.xfer_msg = ngd_xfer_msg;
	dev->ctrl.wakeup =  ngd_clk_pause_wakeup;
	dev->ctrl.alloc_port = msm_alloc_port;
	dev->ctrl.dealloc_port = msm_dealloc_port;
	dev->ctrl.port_xfer = msm_slim_port_xfer;
	dev->ctrl.port_xfer_status = msm_slim_port_xfer_status;
	dev->bam_mem = bam_mem;

	init_completion(&dev->reconf);
	init_completion(&dev->ctrl_up);
	mutex_init(&dev->tx_lock);
	spin_lock_init(&dev->rx_lock);
	dev->ee = 1;
	dev->irq = irq->start;
	dev->bam.irq = bam_irq->start;

	if (rxreg_access)
		dev->use_rx_msgqs = MSM_MSGQ_DISABLED;
	else
		dev->use_rx_msgqs = MSM_MSGQ_RESET;

	/* Enable TX message queues by default as recommended by HW */
	dev->use_tx_msgqs = MSM_MSGQ_RESET;

	init_completion(&dev->rx_msgq_notify);

	/* Register with framework */
	ret = slim_add_numbered_controller(&dev->ctrl);
	if (ret) {
		dev_err(dev->dev, "error adding controller\n");
		goto err_ctrl_failed;
	}

	dev->ctrl.dev.parent = &pdev->dev;
	dev->ctrl.dev.of_node = pdev->dev.of_node;
	dev->state = MSM_CTRL_DOWN;

	ret = request_irq(dev->irq, ngd_slim_interrupt,
			IRQF_TRIGGER_HIGH, "ngd_slim_irq", dev);

	if (ret) {
		dev_err(&pdev->dev, "request IRQ failed\n");
		goto err_request_irq_failed;
	}

	init_completion(&dev->qmi.qmi_comp);
	pm_runtime_use_autosuspend(dev->dev);
	pm_runtime_set_autosuspend_delay(dev->dev, MSM_SLIM_AUTOSUSPEND);
	pm_runtime_set_suspended(dev->dev);
	pm_runtime_enable(dev->dev);

	INIT_WORK(&dev->slave_notify, ngd_laddr_lookup);
	INIT_WORK(&dev->qmi.ssr_down, ngd_adsp_down);
	INIT_WORK(&dev->qmi.ssr_up, ngd_adsp_up);
	dev->qmi.nb.notifier_call = ngd_qmi_available;
	pm_runtime_get_noresume(dev->dev);
	ret = qmi_svc_event_notifier_register(SLIMBUS_QMI_SVC_ID,
				SLIMBUS_QMI_INS_ID, &dev->qmi.nb);
	if (ret) {
		pr_err("Slimbus QMI service registration failed:%d", ret);
		goto qmi_register_failed;
	}


	/* Fire up the Rx message queue thread */
	dev->rx_msgq_thread = kthread_run(ngd_slim_rx_msgq_thread, dev,
					NGD_SLIM_NAME "_ngd_msgq_thread");
	if (IS_ERR(dev->rx_msgq_thread)) {
		ret = PTR_ERR(dev->rx_msgq_thread);
		dev_err(dev->dev, "Failed to start Rx message queue thread\n");
		goto err_thread_create_failed;
	}

	if (pdev->dev.of_node)
		of_register_slim_devices(&dev->ctrl);

	/* Add devices registered with board-info now that controller is up */
	slim_ctrl_add_boarddevs(&dev->ctrl);

	dev_dbg(dev->dev, "NGD SB controller is up!\n");
	return 0;

err_thread_create_failed:
	qmi_svc_event_notifier_unregister(SLIMBUS_QMI_SVC_ID,
				SLIMBUS_QMI_INS_ID, &dev->qmi.nb);
qmi_register_failed:
	free_irq(dev->irq, dev);
err_request_irq_failed:
	slim_del_controller(&dev->ctrl);
err_ctrl_failed:
	iounmap(dev->bam.base);
err_ioremap_bam_failed:
	iounmap(dev->base);
err_ioremap_failed:
	kfree(dev);
	return ret;
}

static int __devexit ngd_slim_remove(struct platform_device *pdev)
{
	struct msm_slim_ctrl *dev = platform_get_drvdata(pdev);
	ngd_slim_enable(dev, false);
	qmi_svc_event_notifier_unregister(SLIMBUS_QMI_SVC_ID,
				SLIMBUS_QMI_INS_ID, &dev->qmi.nb);
	pm_runtime_disable(&pdev->dev);
	free_irq(dev->irq, dev);
	slim_del_controller(&dev->ctrl);
	kthread_stop(dev->rx_msgq_thread);
	iounmap(dev->bam.base);
	iounmap(dev->base);
	kfree(dev);
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int ngd_slim_runtime_idle(struct device *device)
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
static int ngd_slim_runtime_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct msm_slim_ctrl *dev = platform_get_drvdata(pdev);
	int ret = 0;
	if (dev->state >= MSM_CTRL_ASLEEP)
		ret = slim_ctrl_clk_pause(&dev->ctrl, true, 0);
	if (ret) {
		dev_err(device, "clk pause not exited:%d", ret);
		dev->state = MSM_CTRL_ASLEEP;
	} else {
		dev->state = MSM_CTRL_AWAKE;
	}
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int ngd_slim_runtime_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct msm_slim_ctrl *dev = platform_get_drvdata(pdev);
	int ret = 0;
	dev->state = MSM_CTRL_SLEEPING;
	ret = slim_ctrl_clk_pause(&dev->ctrl, false, SLIM_CLK_UNSPECIFIED);
	if (ret) {
		if (ret != -EBUSY)
			dev_err(device, "clk pause not entered:%d", ret);
		dev->state = MSM_CTRL_AWAKE;
	} else {
		dev->state = MSM_CTRL_ASLEEP;
	}
	return ret;
}

static int ngd_slim_suspend(struct device *dev)
{
	int ret = -EBUSY;
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev)) {
		dev_dbg(dev, "system suspend");
		ret = ngd_slim_runtime_suspend(dev);
		/*
		 * If runtime-PM still thinks it's active, then make sure its
		 * status is in sync with HW status.
		 * Since this suspend calls QMI api, it results in holding a
		 * wakelock. That results in failure of first suspend.
		 * Subsequent suspend should not call low-power transition
		 * again since the HW is already in suspended state.
		 */
		if (!ret) {
			pm_runtime_disable(dev);
			pm_runtime_set_suspended(dev);
			pm_runtime_enable(dev);
		}
	}
	if (ret == -EBUSY) {
		/*
		* There is a possibility that some audio stream is active
		* during suspend. We dont want to return suspend failure in
		* that case so that display and relevant components can still
		* go to suspend.
		* If there is some other error, then it should be passed-on
		* to system level suspend
		*/
		ret = 0;
	}
	return ret;
}

static int ngd_slim_resume(struct device *dev)
{
	/*
	 * Rely on runtime-PM to call resume in case it is enabled.
	 * Even if it's not enabled, rely on 1st client transaction to do
	 * clock/power on
	 */
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops ngd_slim_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		ngd_slim_suspend,
		ngd_slim_resume
	)
	SET_RUNTIME_PM_OPS(
		ngd_slim_runtime_suspend,
		ngd_slim_runtime_resume,
		ngd_slim_runtime_idle
	)
};

static struct of_device_id ngd_slim_dt_match[] = {
	{
		.compatible = "qcom,slim-ngd",
	},
	{}
};

static struct platform_driver ngd_slim_driver = {
	.probe = ngd_slim_probe,
	.remove = ngd_slim_remove,
	.driver	= {
		.name = NGD_SLIM_NAME,
		.owner = THIS_MODULE,
		.pm = &ngd_slim_dev_pm_ops,
		.of_match_table = ngd_slim_dt_match,
	},
};

static int ngd_slim_init(void)
{
	return platform_driver_register(&ngd_slim_driver);
}
late_initcall(ngd_slim_init);

static void ngd_slim_exit(void)
{
	platform_driver_unregister(&ngd_slim_driver);
}
module_exit(ngd_slim_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Slimbus controller");
MODULE_ALIAS("platform:msm-slim-ngd");
