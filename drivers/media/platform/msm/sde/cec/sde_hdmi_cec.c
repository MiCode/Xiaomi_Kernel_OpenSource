/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/cec.h>
#include <media/cec.h>
#include <media/cec-edid.h>
#include <media/cec-notifier.h>

#include "sde_hdmi_cec_util.h"

#define CEC_NAME "sde-hdmi-cec"

/* CEC Register Definition */
#define CEC_INTR_MASK (BIT(1) | BIT(3) | BIT(7))
#define CEC_SUPPORTED_HW_VERSION 0x30000001

#define HDMI_CEC_WR_RANGE                (0x000002DC)
#define HDMI_CEC_RD_RANGE                (0x000002E0)
#define HDMI_VERSION                     (0x000002E4)
#define HDMI_CEC_CTRL                    (0x0000028C)
#define HDMI_CEC_WR_DATA                 (0x00000290)
#define HDMI_CEC_RETRANSMIT              (0x00000294)
#define HDMI_CEC_STATUS                  (0x00000298)
#define HDMI_CEC_INT                     (0x0000029C)
#define HDMI_CEC_ADDR                    (0x000002A0)
#define HDMI_CEC_TIME                    (0x000002A4)
#define HDMI_CEC_REFTIMER                (0x000002A8)
#define HDMI_CEC_RD_DATA                 (0x000002AC)
#define HDMI_CEC_RD_FILTER               (0x000002B0)
#define HDMI_CEC_COMPL_CTL               (0x00000360)
#define HDMI_CEC_RD_START_RANGE          (0x00000364)
#define HDMI_CEC_RD_TOTAL_RANGE          (0x00000368)
#define HDMI_CEC_RD_ERR_RESP_LO          (0x0000036C)
#define HDMI_CEC_WR_CHECK_CONFIG         (0x00000370)

enum cec_irq_status {
	CEC_IRQ_FRAME_WR_DONE = 1 << 0,
	CEC_IRQ_FRAME_RD_DONE = 1 << 1,
	CEC_IRQ_FRAME_ERROR = 1 << 2,
};

struct sde_hdmi_cec {
	struct cec_adapter *adap;
	struct device *dev;
	struct cec_hw_resource hw_res;
	int irq;
	enum cec_irq_status irq_status;
	struct cec_notifier *notifier;
};

static int sde_hdmi_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	struct sde_hdmi_cec *cec = adap->priv;
	struct cec_hw_resource *hw = &cec->hw_res;
	u32 hdmi_hw_version, reg_val;

	pr_debug("adap enable %d\n", enable);

	if (enable) {
		pm_runtime_get_sync(cec->dev);

		/* 19.2Mhz * 0.00005 us = 950 = 0x3B6 */
		CEC_REG_WRITE(hw, HDMI_CEC_REFTIMER, (0x3B6 & 0xFFF) | BIT(16));

		hdmi_hw_version = CEC_REG_READ(hw, HDMI_VERSION);
		if (hdmi_hw_version >= CEC_SUPPORTED_HW_VERSION) {
			CEC_REG_WRITE(hw, HDMI_CEC_RD_RANGE, 0x30AB9888);
			CEC_REG_WRITE(hw, HDMI_CEC_WR_RANGE, 0x888AA888);

			CEC_REG_WRITE(hw, HDMI_CEC_RD_START_RANGE, 0x88888888);
			CEC_REG_WRITE(hw, HDMI_CEC_RD_TOTAL_RANGE, 0x99);
			CEC_REG_WRITE(hw, HDMI_CEC_COMPL_CTL, 0xF);
			CEC_REG_WRITE(hw, HDMI_CEC_WR_CHECK_CONFIG, 0x4);
		} else {
			pr_err("CEC version %d is not supported.\n",
				hdmi_hw_version);
			return -EPERM;
		}

		CEC_REG_WRITE(hw, HDMI_CEC_RD_FILTER, BIT(0) | (0x7FF << 4));
		CEC_REG_WRITE(hw, HDMI_CEC_TIME, BIT(0) | ((7 * 0x30) << 7));

		/* Enable CEC interrupts */
		CEC_REG_WRITE(hw, HDMI_CEC_INT, CEC_INTR_MASK);

		/* Enable Engine */
		CEC_REG_WRITE(hw, HDMI_CEC_CTRL, BIT(0));
	} else {
		/* Disable Engine */
		CEC_REG_WRITE(hw, HDMI_CEC_CTRL, 0);

		/* Disable CEC interrupts */
		reg_val = CEC_REG_READ(hw, HDMI_CEC_INT);
		CEC_REG_WRITE(hw, HDMI_CEC_INT, reg_val & ~CEC_INTR_MASK);

		pm_runtime_put(cec->dev);
	}

	return 0;
}

static int sde_hdmi_cec_adap_log_addr(struct cec_adapter *adap, u8 logical_addr)
{
	struct sde_hdmi_cec *cec = adap->priv;
	struct cec_hw_resource *hw = &cec->hw_res;

	pr_debug("set log addr %d\n", logical_addr);

	CEC_REG_WRITE(hw, HDMI_CEC_ADDR, logical_addr & 0xF);

	return 0;
}

static int sde_hdmi_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
				  u32 signal_free_time, struct cec_msg *msg)
{
	struct sde_hdmi_cec *cec = adap->priv;
	struct cec_hw_resource *hw = &cec->hw_res;
	u32 frame_type;
	u8 retransmits;
	int i;
	u32 line_check_retry = 10;

	pr_debug("transmit msg [%d]->[%d]: len = %d, attampts=%d, signal_free_time=%d\n",
		cec_msg_initiator(msg), cec_msg_destination(msg), msg->len,
		attempts, signal_free_time);

	/* toggle cec in order to flush out bad hw state, if any */
	CEC_REG_WRITE(hw, HDMI_CEC_CTRL, 0);
	CEC_REG_WRITE(hw, HDMI_CEC_CTRL, BIT(0));

	/* make sure state is cleared */
	wmb();

	retransmits = attempts ? (attempts - 1) : 0;

	CEC_REG_WRITE(hw, HDMI_CEC_RETRANSMIT, (retransmits << 4) | BIT(0));

	frame_type = cec_msg_is_broadcast(msg) ? BIT(0) : 0;

	for (i = 0; i < msg->len; i++)
		CEC_REG_WRITE(hw, HDMI_CEC_WR_DATA,
			(msg->msg[i] << 8) | frame_type);

	/* check line status */
	while ((CEC_REG_READ(hw, HDMI_CEC_STATUS) & BIT(0)) &&
		line_check_retry) {
		line_check_retry--;
		pr_debug("CEC line is busy(%d)\n", line_check_retry);
		schedule();
	}

	if (!line_check_retry && (CEC_REG_READ(hw, HDMI_CEC_STATUS) & BIT(0))) {
		pr_err("CEC line is busy. Retry failed\n");
		return -EBUSY;
	}

	/* start transmission */
	CEC_REG_WRITE(hw, HDMI_CEC_CTRL, BIT(0) | BIT(1) |
		((msg->len & 0x1F) << 4) | BIT(9));

	return 0;
}

static void sde_hdmi_cec_handle_rx_done(struct sde_hdmi_cec *cec)
{
	struct cec_hw_resource *hw = &cec->hw_res;
	struct cec_msg msg = {};
	u32 data;
	int i;

	pr_debug("rx done\n");

	data = CEC_REG_READ(hw, HDMI_CEC_RD_DATA);
	msg.len = (data & 0x1F00) >> 8;
	if (msg.len < 1 || msg.len > CEC_MAX_MSG_SIZE) {
		pr_err("invalid message size %d", msg.len);
		return;
	}
	msg.msg[0] = data & 0xFF;

	for (i = 1; i < msg.len; i++)
		msg.msg[i] = CEC_REG_READ(hw, HDMI_CEC_RD_DATA) & 0xFF;

	cec_received_msg(cec->adap, &msg);
}

static void sde_hdmi_cec_handle_tx_done(struct sde_hdmi_cec *cec)
{
	pr_debug("tx done\n");
	cec_transmit_done(cec->adap, CEC_TX_STATUS_OK, 0, 0, 0, 0);
}

static void sde_hdmi_cec_handle_tx_error(struct sde_hdmi_cec *cec)
{
	struct cec_hw_resource *hw = &cec->hw_res;
	u32 cec_status = CEC_REG_READ(hw, HDMI_CEC_STATUS);

	pr_debug("tx error status %x\n", cec_status);

	if ((cec_status & 0xF0) == 0x10)
		cec_transmit_done(cec->adap,
			CEC_TX_STATUS_NACK, 0, 1, 0, 0);
	else if ((cec_status & 0xF0) == 0x30)
		cec_transmit_done(cec->adap,
			CEC_TX_STATUS_ARB_LOST, 1, 0, 0, 0);
	else
		cec_transmit_done(cec->adap,
			CEC_TX_STATUS_ERROR | CEC_TX_STATUS_MAX_RETRIES,
			0, 0, 0, 1);
}

static irqreturn_t sde_hdmi_cec_irq_handler_thread(int irq, void *priv)
{
	struct sde_hdmi_cec *cec = priv;

	pr_debug("irq thread: status %x\n", cec->irq_status);

	if (cec->irq_status & CEC_IRQ_FRAME_WR_DONE)
		sde_hdmi_cec_handle_tx_done(cec);

	if (cec->irq_status & CEC_IRQ_FRAME_ERROR)
		sde_hdmi_cec_handle_tx_error(cec);

	if (cec->irq_status & CEC_IRQ_FRAME_RD_DONE)
		sde_hdmi_cec_handle_rx_done(cec);

	cec->irq_status = 0;

	return IRQ_HANDLED;
}

static irqreturn_t sde_hdmi_cec_irq_handler(int irq, void *priv)
{
	struct sde_hdmi_cec *cec = priv;
	struct cec_hw_resource *hw = &cec->hw_res;
	u32 data = CEC_REG_READ(hw, HDMI_CEC_INT);

	CEC_REG_WRITE(hw, HDMI_CEC_INT, data);

	pr_debug("irq handler: %x\n", data);

	if ((data & BIT(0)) && (data & BIT(1)))
		cec->irq_status |= CEC_IRQ_FRAME_WR_DONE;

	if ((data & BIT(2)) && (data & BIT(3)))
		cec->irq_status |= CEC_IRQ_FRAME_ERROR;

	if ((data & BIT(6)) && (data & BIT(7)))
		cec->irq_status |= CEC_IRQ_FRAME_RD_DONE;

	return cec->irq_status ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static const struct cec_adap_ops sde_hdmi_cec_adap_ops = {
	.adap_enable = sde_hdmi_cec_adap_enable,
	.adap_log_addr = sde_hdmi_cec_adap_log_addr,
	.adap_transmit = sde_hdmi_cec_adap_transmit,
};

static int sde_hdmi_cec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sde_hdmi_cec *cec;
	struct device_node *np;
	struct platform_device *hdmi_dev;
	int ret;

	cec = devm_kzalloc(dev, sizeof(*cec), GFP_KERNEL);
	if (!cec)
		return -ENOMEM;

	cec->dev = dev;

	np = of_parse_phandle(pdev->dev.of_node, "qcom,hdmi-dev", 0);
	if (!np) {
		pr_err("failed to find hdmi node in device tree\n");
		return -ENODEV;
	}
	hdmi_dev = of_find_device_by_node(np);
	if (hdmi_dev == NULL)
		return -EPROBE_DEFER;

	cec->irq = of_irq_get(dev->of_node, 0);
	if (cec->irq < 0) {
		pr_err("failed to get irq\n");
		return cec->irq;
	}

	ret = devm_request_threaded_irq(dev, cec->irq, sde_hdmi_cec_irq_handler,
					sde_hdmi_cec_irq_handler_thread, 0,
					pdev->name, cec);
	if (ret)
		return ret;

	ret = sde_hdmi_cec_init_resource(pdev, &cec->hw_res);
	if (ret)
		return ret;

	cec->adap = cec_allocate_adapter(&sde_hdmi_cec_adap_ops, cec,
			CEC_NAME,
			CEC_CAP_LOG_ADDRS | CEC_CAP_PASSTHROUGH |
			CEC_CAP_TRANSMIT, 1);
	ret = PTR_ERR_OR_ZERO(cec->adap);
	if (ret)
		return ret;

	ret = cec_register_adapter(cec->adap, &pdev->dev);
	if (ret)
		goto err_del_adap;

	cec->notifier = cec_notifier_get(&hdmi_dev->dev);
	if (!cec->notifier) {
		pr_err("failed to get cec notifier\n");
		goto err_del_adap;
	}

	platform_set_drvdata(pdev, cec);

	pm_runtime_enable(dev);

	/*
	 * cec_register_cec_notifier has to be later than pm_runtime_enable
	 * because it calls adap_enable.
	 */
	cec_register_cec_notifier(cec->adap, cec->notifier);

	pr_debug("probe done\n");

	return ret;

err_del_adap:
	cec_delete_adapter(cec->adap);
	return ret;
}

static int sde_hdmi_cec_remove(struct platform_device *pdev)
{
	struct sde_hdmi_cec *cec = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);

	cec_unregister_adapter(cec->adap);
	cec_notifier_put(cec->notifier);

	devm_free_irq(&pdev->dev, cec->irq, cec);
	sde_hdmi_cec_deinit_resource(pdev, &cec->hw_res);

	pr_debug("remove done\n");

	return 0;
}

static int __maybe_unused sde_hdmi_cec_runtime_suspend(struct device *dev)
{
	struct sde_hdmi_cec *cec = dev_get_drvdata(dev);
	struct cec_hw_resource *hw = &cec->hw_res;

	pr_debug("runtime suspend\n");

	return sde_hdmi_cec_enable_power(hw, false);
}

static int __maybe_unused sde_hdmi_cec_runtime_resume(struct device *dev)
{
	struct sde_hdmi_cec *cec = dev_get_drvdata(dev);
	struct cec_hw_resource *hw = &cec->hw_res;

	pr_debug("runtime resume\n");

	return sde_hdmi_cec_enable_power(hw, true);
}

static const struct dev_pm_ops sde_hdmi_cec_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(sde_hdmi_cec_runtime_suspend,
		sde_hdmi_cec_runtime_resume, NULL)
};

static const struct of_device_id sde_hdmi_cec_match[] = {
	{
		.compatible = "qcom,hdmi-cec",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sde_hdmi_cec_match);

static struct platform_driver sde_hdmi_cec_pdrv = {
	.probe = sde_hdmi_cec_probe,
	.remove = sde_hdmi_cec_remove,
	.driver = {
		.name = CEC_NAME,
		.of_match_table = sde_hdmi_cec_match,
		.pm = &sde_hdmi_cec_pm_ops,
	},
};

module_platform_driver(sde_hdmi_cec_pdrv);
MODULE_DESCRIPTION("MSM SDE HDMI CEC driver");
