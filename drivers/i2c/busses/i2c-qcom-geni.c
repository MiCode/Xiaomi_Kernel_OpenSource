/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/qcom-geni-se.h>
#include <linux/ipc_logging.h>

#define SE_I2C_TX_TRANS_LEN		(0x26C)
#define SE_I2C_RX_TRANS_LEN		(0x270)
#define SE_I2C_SCL_COUNTERS		(0x278)
#define SE_GENI_IOS			(0x908)

#define SE_I2C_ERR  (M_CMD_OVERRUN_EN | M_ILLEGAL_CMD_EN | M_CMD_FAILURE_EN |\
			M_GP_IRQ_1_EN | M_GP_IRQ_3_EN | M_GP_IRQ_4_EN)
#define SE_I2C_ABORT (1U << 1)
/* M_CMD OP codes for I2C */
#define I2C_WRITE		(0x1)
#define I2C_READ		(0x2)
#define I2C_WRITE_READ		(0x3)
#define I2C_ADDR_ONLY		(0x4)
#define I2C_BUS_CLEAR		(0x6)
#define I2C_STOP_ON_BUS		(0x7)
/* M_CMD params for I2C */
#define PRE_CMD_DELAY		(BIT(0))
#define TIMESTAMP_BEFORE	(BIT(1))
#define STOP_STRETCH		(BIT(2))
#define TIMESTAMP_AFTER		(BIT(3))
#define POST_COMMAND_DELAY	(BIT(4))
#define IGNORE_ADD_NACK		(BIT(6))
#define READ_FINISHED_WITH_ACK	(BIT(7))
#define BYPASS_ADDR_PHASE	(BIT(8))
#define SLV_ADDR_MSK		(GENMASK(15, 9))
#define SLV_ADDR_SHFT		(9)

#define I2C_CORE2X_VOTE		(10000)
#define GP_IRQ0			0
#define GP_IRQ1			1
#define GP_IRQ2			2
#define GP_IRQ3			3
#define GP_IRQ4			4
#define GP_IRQ5			5
#define GENI_OVERRUN		6
#define GENI_ILLEGAL_CMD	7
#define GENI_ABORT_DONE		8
#define GENI_TIMEOUT		9

#define I2C_NACK		GP_IRQ1
#define I2C_BUS_PROTO		GP_IRQ3
#define I2C_ARB_LOST		GP_IRQ4
#define DM_I2C_RX_ERR		((GP_IRQ1 | GP_IRQ3 | GP_IRQ4) >> 4)

struct geni_i2c_dev {
	struct device *dev;
	void __iomem *base;
	unsigned int tx_wm;
	int irq;
	int err;
	struct i2c_adapter adap;
	struct completion xfer;
	struct i2c_msg *cur;
	struct se_geni_rsc i2c_rsc;
	int cur_wr;
	int cur_rd;
	struct device *wrapper_dev;
	void *ipcl;
};

struct geni_i2c_err_log {
	int err;
	const char *msg;
};

static struct geni_i2c_err_log gi2c_log[] = {
	[GP_IRQ0] = {-EINVAL, "Unknown I2C err GP_IRQ0"},
	[I2C_NACK] = {-ENOTCONN,
			"NACK: slv unresponsive, check its power/reset-ln"},
	[GP_IRQ2] = {-EINVAL, "Unknown I2C err GP IRQ2"},
	[I2C_BUS_PROTO] = {-EPROTO,
				"Bus proto err, noisy/unepxected start/stop"},
	[I2C_ARB_LOST] = {-EBUSY,
				"Bus arbitration lost, clock line undriveable"},
	[GP_IRQ5] = {-EINVAL, "Unknown I2C err GP IRQ5"},
	[GENI_OVERRUN] = {-EIO, "Cmd overrun, check GENI cmd-state machine"},
	[GENI_ILLEGAL_CMD] = {-EILSEQ,
				"Illegal cmd, check GENI cmd-state machine"},
	[GENI_ABORT_DONE] = {-ETIMEDOUT, "Abort after timeout successful"},
	[GENI_TIMEOUT] = {-ETIMEDOUT, "I2C TXN timed out"},
};

static inline void qcom_geni_i2c_conf(void __iomem *base, int dfs, int div)
{
	geni_write_reg(dfs, base, SE_GENI_CLK_SEL);
	geni_write_reg((div << 4) | 1, base, GENI_SER_M_CLK_CFG);
	geni_write_reg(((5 << 20) | (0xC << 10) | 0x18),
				base, SE_I2C_SCL_COUNTERS);
	/*
	 * Ensure Clk config completes before return.
	 */
	mb();
}

static void geni_i2c_err(struct geni_i2c_dev *gi2c, int err)
{
	u32 m_stat = readl_relaxed(gi2c->base + SE_GENI_M_IRQ_STATUS);
	u32 rx_st = readl_relaxed(gi2c->base + SE_GENI_RX_FIFO_STATUS);
	u32 tx_st = readl_relaxed(gi2c->base + SE_GENI_TX_FIFO_STATUS);
	u32 m_cmd = readl_relaxed(gi2c->base + SE_GENI_M_CMD0);
	u32 geni_s = readl_relaxed(gi2c->base + SE_GENI_STATUS);
	u32 geni_ios = readl_relaxed(gi2c->base + SE_GENI_IOS);

	if (err == I2C_NACK || err == GENI_ABORT_DONE) {
		GENI_SE_DBG(gi2c->ipcl, false, gi2c->dev, "%s\n",
			    gi2c_log[err].msg);
		GENI_SE_DBG(gi2c->ipcl, false, gi2c->dev,
			     "m_stat:0x%x, tx_stat:0x%x, rx_stat:0x%x, ",
			     m_stat, tx_st, rx_st);
		GENI_SE_DBG(gi2c->ipcl, false, gi2c->dev,
			     "m_cmd:0x%x, geni_status:0x%x, geni_ios:0x%x\n",
			     m_cmd, geni_s, geni_ios);
	} else {
		GENI_SE_ERR(gi2c->ipcl, true, gi2c->dev, "%s\n",
			     gi2c_log[err].msg);
		GENI_SE_ERR(gi2c->ipcl, true, gi2c->dev,
			     "m_stat:0x%x, tx_stat:0x%x, rx_stat:0x%x, ",
			     m_stat, tx_st, rx_st);
		GENI_SE_ERR(gi2c->ipcl, true, gi2c->dev,
			     "m_cmd:0x%x, geni_status:0x%x, geni_ios:0x%x\n",
			     m_cmd, geni_s, geni_ios);
	}
	gi2c->err = gi2c_log[err].err;
}

static irqreturn_t geni_i2c_irq(int irq, void *dev)
{
	struct geni_i2c_dev *gi2c = dev;
	int i, j;
	u32 m_stat = readl_relaxed(gi2c->base + SE_GENI_M_IRQ_STATUS);
	u32 rx_st = readl_relaxed(gi2c->base + SE_GENI_RX_FIFO_STATUS);
	u32 dm_tx_st = readl_relaxed(gi2c->base + SE_DMA_TX_IRQ_STAT);
	u32 dm_rx_st = readl_relaxed(gi2c->base + SE_DMA_RX_IRQ_STAT);
	u32 dma = readl_relaxed(gi2c->base + SE_GENI_DMA_MODE_EN);
	struct i2c_msg *cur = gi2c->cur;

	if (!cur || (m_stat & M_CMD_FAILURE_EN) ||
		    (dm_rx_st & (DM_I2C_RX_ERR)) ||
		    (m_stat & M_CMD_ABORT_EN)) {

		if (m_stat & M_GP_IRQ_1_EN)
			geni_i2c_err(gi2c, I2C_NACK);
		if (m_stat & M_GP_IRQ_3_EN)
			geni_i2c_err(gi2c, I2C_BUS_PROTO);
		if (m_stat & M_GP_IRQ_4_EN)
			geni_i2c_err(gi2c, I2C_ARB_LOST);
		if (m_stat & M_CMD_OVERRUN_EN)
			geni_i2c_err(gi2c, GENI_OVERRUN);
		if (m_stat & M_ILLEGAL_CMD_EN)
			geni_i2c_err(gi2c, GENI_ILLEGAL_CMD);
		if (m_stat & M_CMD_ABORT_EN)
			geni_i2c_err(gi2c, GENI_ABORT_DONE);
		if (m_stat & M_GP_IRQ_0_EN)
			geni_i2c_err(gi2c, GP_IRQ0);

		if (!dma)
			writel_relaxed(0, (gi2c->base +
					   SE_GENI_TX_WATERMARK_REG));
		gi2c->err = -EIO;
		goto irqret;
	}

	if (dma) {
		dev_dbg(gi2c->dev, "i2c dma tx:0x%x, dma rx:0x%x\n", dm_tx_st,
			dm_rx_st);
		goto irqret;
	}

	if (((m_stat & M_RX_FIFO_WATERMARK_EN) ||
		(m_stat & M_RX_FIFO_LAST_EN)) && (cur->flags & I2C_M_RD)) {
		u32 rxcnt = rx_st & RX_FIFO_WC_MSK;

		for (j = 0; j < rxcnt; j++) {
			u32 temp;
			int p;

			temp = readl_relaxed(gi2c->base + SE_GENI_RX_FIFOn);
			for (i = gi2c->cur_rd, p = 0; (i < cur->len && p < 4);
				i++, p++)
				cur->buf[i] = (u8) ((temp >> (p * 8)) & 0xff);
			gi2c->cur_rd = i;
			if (gi2c->cur_rd == cur->len) {
				dev_dbg(gi2c->dev, "FIFO i:%d,read 0x%x\n",
					i, temp);
				break;
			}
		}
	} else if ((m_stat & M_TX_FIFO_WATERMARK_EN) &&
					!(cur->flags & I2C_M_RD)) {
		for (j = 0; j < gi2c->tx_wm; j++) {
			u32 temp = 0;
			int p;

			for (i = gi2c->cur_wr, p = 0; (i < cur->len && p < 4);
				i++, p++)
				temp |= (((u32)(cur->buf[i]) << (p * 8)));
			writel_relaxed(temp, gi2c->base + SE_GENI_TX_FIFOn);
			gi2c->cur_wr = i;
			dev_dbg(gi2c->dev, "FIFO i:%d,wrote 0x%x\n", i, temp);
			if (gi2c->cur_wr == cur->len) {
				dev_dbg(gi2c->dev, "FIFO i2c bytes done writing\n");
				writel_relaxed(0,
				(gi2c->base + SE_GENI_TX_WATERMARK_REG));
				break;
			}
		}
	}
irqret:
	if (m_stat)
		writel_relaxed(m_stat, gi2c->base + SE_GENI_M_IRQ_CLEAR);

	if (dma) {
		if (dm_tx_st)
			writel_relaxed(dm_tx_st, gi2c->base +
				       SE_DMA_TX_IRQ_CLR);
		if (dm_rx_st)
			writel_relaxed(dm_rx_st, gi2c->base +
				       SE_DMA_RX_IRQ_CLR);
		/* Ensure all writes are done before returning from ISR. */
		wmb();
	}
	/* if this is err with done-bit not set, handle that thr' timeout. */
	if (m_stat & M_CMD_DONE_EN)
		complete(&gi2c->xfer);
	else if ((dm_tx_st & TX_DMA_DONE) || (dm_rx_st & RX_DMA_DONE))
		complete(&gi2c->xfer);

	return IRQ_HANDLED;
}

static int geni_i2c_xfer(struct i2c_adapter *adap,
			 struct i2c_msg msgs[],
			 int num)
{
	struct geni_i2c_dev *gi2c = i2c_get_adapdata(adap);
	int i, ret = 0, timeout = 0;

	gi2c->err = 0;
	gi2c->cur = &msgs[0];
	reinit_completion(&gi2c->xfer);
	ret = pm_runtime_get_sync(gi2c->dev);
	if (ret < 0) {
		GENI_SE_ERR(gi2c->ipcl, true, gi2c->dev,
			    "error turning SE resources:%d\n", ret);
		pm_runtime_put_noidle(gi2c->dev);
		/* Set device in suspended since resume failed */
		pm_runtime_set_suspended(gi2c->dev);
		return ret;
	}
	qcom_geni_i2c_conf(gi2c->base, 0, 2);
	dev_dbg(gi2c->dev, "i2c xfer:num:%d, msgs:len:%d,flg:%d\n",
				num, msgs[0].len, msgs[0].flags);
	for (i = 0; i < num; i++) {
		int stretch = (i < (num - 1));
		u32 m_param = 0;
		u32 m_cmd = 0;
		dma_addr_t tx_dma = 0;
		dma_addr_t rx_dma = 0;
		enum se_xfer_mode mode = FIFO_MODE;

		m_param |= (stretch ? STOP_STRETCH : 0);
		m_param |= ((msgs[i].addr & 0x7F) << SLV_ADDR_SHFT);

		gi2c->cur = &msgs[i];
		mode = msgs[i].len > 32 ? SE_DMA : FIFO_MODE;
		ret = geni_se_select_mode(gi2c->base, mode);
		if (ret) {
			dev_err(gi2c->dev, "%s: Error mode init %d:%d:%d\n",
				__func__, mode, i, msgs[i].len);
			break;
		}
		if (msgs[i].flags & I2C_M_RD) {
			dev_dbg(gi2c->dev,
				"READ,n:%d,i:%d len:%d, stretch:%d\n",
					num, i, msgs[i].len, stretch);
			geni_write_reg(msgs[i].len,
				       gi2c->base, SE_I2C_RX_TRANS_LEN);
			m_cmd = I2C_READ;
			geni_setup_m_cmd(gi2c->base, m_cmd, m_param);
			if (mode == SE_DMA) {
				ret = geni_se_rx_dma_prep(gi2c->wrapper_dev,
							gi2c->base, msgs[i].buf,
							msgs[i].len, &rx_dma);
				if (ret)
					mode = FIFO_MODE;
			}
			if (mode == FIFO_MODE)
				geni_se_select_mode(gi2c->base, mode);
		} else {
			dev_dbg(gi2c->dev,
				"WRITE:n:%d,i:%d len:%d, stretch:%d, m_param:0x%x\n",
					num, i, msgs[i].len, stretch, m_param);
			geni_write_reg(msgs[i].len, gi2c->base,
						SE_I2C_TX_TRANS_LEN);
			m_cmd = I2C_WRITE;
			geni_setup_m_cmd(gi2c->base, m_cmd, m_param);
			if (mode == SE_DMA) {
				ret = geni_se_tx_dma_prep(gi2c->wrapper_dev,
							gi2c->base, msgs[i].buf,
							msgs[i].len, &tx_dma);
				if (ret)
					mode = FIFO_MODE;
			}
			if (mode == FIFO_MODE) {
				geni_se_select_mode(gi2c->base, mode);
				/* Get FIFO IRQ */
				geni_write_reg(1, gi2c->base,
						SE_GENI_TX_WATERMARK_REG);
			}
		}
		/* Ensure FIFO write go through before waiting for Done evet */
		mb();
		timeout = wait_for_completion_timeout(&gi2c->xfer, HZ);
		if (!timeout) {
			geni_i2c_err(gi2c, GENI_TIMEOUT);
			gi2c->cur = NULL;
			geni_abort_m_cmd(gi2c->base);
			timeout = wait_for_completion_timeout(&gi2c->xfer, HZ);
		}
		gi2c->cur_wr = 0;
		gi2c->cur_rd = 0;
		if (mode == SE_DMA) {
			if (gi2c->err) {
				if (msgs[i].flags != I2C_M_RD)
					writel_relaxed(1, gi2c->base +
							SE_DMA_TX_FSM_RST);
				else
					writel_relaxed(1, gi2c->base +
							SE_DMA_RX_FSM_RST);
				wait_for_completion_timeout(&gi2c->xfer, HZ);
			}
			geni_se_rx_dma_unprep(gi2c->wrapper_dev, rx_dma,
					      msgs[i].len);
			geni_se_tx_dma_unprep(gi2c->wrapper_dev, tx_dma,
					      msgs[i].len);
		}
		ret = gi2c->err;
		if (gi2c->err) {
			dev_err(gi2c->dev, "i2c error :%d\n", gi2c->err);
			break;
		}
	}
	if (ret == 0)
		ret = i;
	pm_runtime_put_sync(gi2c->dev);
	gi2c->cur = NULL;
	gi2c->err = 0;
	dev_dbg(gi2c->dev, "i2c txn ret:%d\n", ret);
	return ret;
}

static u32 geni_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm geni_i2c_algo = {
	.master_xfer	= geni_i2c_xfer,
	.functionality	= geni_i2c_func,
};

static int geni_i2c_probe(struct platform_device *pdev)
{
	struct geni_i2c_dev *gi2c;
	struct resource *res;
	struct platform_device *wrapper_pdev;
	struct device_node *wrapper_ph_node;
	int ret;

	gi2c = devm_kzalloc(&pdev->dev, sizeof(*gi2c), GFP_KERNEL);
	if (!gi2c)
		return -ENOMEM;

	gi2c->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	wrapper_ph_node = of_parse_phandle(pdev->dev.of_node,
				"qcom,wrapper-core", 0);
	if (IS_ERR_OR_NULL(wrapper_ph_node)) {
		ret = PTR_ERR(wrapper_ph_node);
		dev_err(&pdev->dev, "No wrapper core defined\n");
		return ret;
	}
	wrapper_pdev = of_find_device_by_node(wrapper_ph_node);
	of_node_put(wrapper_ph_node);
	if (IS_ERR_OR_NULL(wrapper_pdev)) {
		ret = PTR_ERR(wrapper_pdev);
		dev_err(&pdev->dev, "Cannot retrieve wrapper device\n");
		return ret;
	}
	gi2c->wrapper_dev = &wrapper_pdev->dev;
	gi2c->i2c_rsc.wrapper_dev = &wrapper_pdev->dev;
	ret = geni_se_resources_init(&gi2c->i2c_rsc, I2C_CORE2X_VOTE,
				     (DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	if (ret) {
		dev_err(gi2c->dev, "geni_se_resources_init\n");
		return ret;
	}

	gi2c->i2c_rsc.se_clk = devm_clk_get(&pdev->dev, "se-clk");
	if (IS_ERR(gi2c->i2c_rsc.se_clk)) {
		ret = PTR_ERR(gi2c->i2c_rsc.se_clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		return ret;
	}

	gi2c->i2c_rsc.m_ahb_clk = devm_clk_get(&pdev->dev, "m-ahb");
	if (IS_ERR(gi2c->i2c_rsc.m_ahb_clk)) {
		ret = PTR_ERR(gi2c->i2c_rsc.m_ahb_clk);
		dev_err(&pdev->dev, "Err getting M AHB clk %d\n", ret);
		return ret;
	}

	gi2c->i2c_rsc.s_ahb_clk = devm_clk_get(&pdev->dev, "s-ahb");
	if (IS_ERR(gi2c->i2c_rsc.s_ahb_clk)) {
		ret = PTR_ERR(gi2c->i2c_rsc.s_ahb_clk);
		dev_err(&pdev->dev, "Err getting S AHB clk %d\n", ret);
		return ret;
	}

	gi2c->base = devm_ioremap_resource(gi2c->dev, res);
	if (IS_ERR(gi2c->base))
		return PTR_ERR(gi2c->base);

	gi2c->i2c_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(gi2c->i2c_rsc.geni_pinctrl)) {
		dev_err(&pdev->dev, "No pinctrl config specified\n");
		ret = PTR_ERR(gi2c->i2c_rsc.geni_pinctrl);
		return ret;
	}
	gi2c->i2c_rsc.geni_gpio_active =
		pinctrl_lookup_state(gi2c->i2c_rsc.geni_pinctrl,
							PINCTRL_DEFAULT);
	if (IS_ERR_OR_NULL(gi2c->i2c_rsc.geni_gpio_active)) {
		dev_err(&pdev->dev, "No default config specified\n");
		ret = PTR_ERR(gi2c->i2c_rsc.geni_gpio_active);
		return ret;
	}
	gi2c->i2c_rsc.geni_gpio_sleep =
		pinctrl_lookup_state(gi2c->i2c_rsc.geni_pinctrl,
							PINCTRL_SLEEP);
	if (IS_ERR_OR_NULL(gi2c->i2c_rsc.geni_gpio_sleep)) {
		dev_err(&pdev->dev, "No sleep config specified\n");
		ret = PTR_ERR(gi2c->i2c_rsc.geni_gpio_sleep);
		return ret;
	}

	gi2c->irq = platform_get_irq(pdev, 0);
	if (gi2c->irq < 0) {
		dev_err(gi2c->dev, "IRQ error for i2c-geni\n");
		return gi2c->irq;
	}

	gi2c->adap.algo = &geni_i2c_algo;
	init_completion(&gi2c->xfer);
	platform_set_drvdata(pdev, gi2c);
	ret = devm_request_irq(gi2c->dev, gi2c->irq, geni_i2c_irq,
			       IRQF_TRIGGER_HIGH, "i2c_geni", gi2c);
	if (ret) {
		dev_err(gi2c->dev, "Request_irq failed:%d: err:%d\n",
				   gi2c->irq, ret);
		return ret;
	}
	disable_irq(gi2c->irq);
	i2c_set_adapdata(&gi2c->adap, gi2c);
	gi2c->adap.dev.parent = gi2c->dev;
	gi2c->adap.dev.of_node = pdev->dev.of_node;

	strlcpy(gi2c->adap.name, "Geni-I2C", sizeof(gi2c->adap.name));

	pm_runtime_set_suspended(gi2c->dev);
	pm_runtime_enable(gi2c->dev);
	i2c_add_adapter(&gi2c->adap);

	dev_dbg(gi2c->dev, "I2C probed\n");
	return 0;
}

static int geni_i2c_remove(struct platform_device *pdev)
{
	struct geni_i2c_dev *gi2c = platform_get_drvdata(pdev);

	pm_runtime_disable(gi2c->dev);
	i2c_del_adapter(&gi2c->adap);
	if (gi2c->ipcl)
		ipc_log_context_destroy(gi2c->ipcl);
	return 0;
}

static int geni_i2c_resume_noirq(struct device *device)
{
	return 0;
}

#ifdef CONFIG_PM
static int geni_i2c_runtime_suspend(struct device *dev)
{
	struct geni_i2c_dev *gi2c = dev_get_drvdata(dev);

	disable_irq(gi2c->irq);
	se_geni_resources_off(&gi2c->i2c_rsc);
	return 0;
}

static int geni_i2c_runtime_resume(struct device *dev)
{
	int ret;
	struct geni_i2c_dev *gi2c = dev_get_drvdata(dev);

	if (!gi2c->ipcl) {
		char ipc_name[I2C_NAME_SIZE];

		snprintf(ipc_name, I2C_NAME_SIZE, "i2c-%d", gi2c->adap.nr);
		gi2c->ipcl = ipc_log_context_create(2, ipc_name, 0);
	}
	ret = se_geni_resources_on(&gi2c->i2c_rsc);
	if (ret)
		return ret;

	if (unlikely(!gi2c->tx_wm)) {
		int gi2c_tx_depth = get_tx_fifo_depth(gi2c->base);

		gi2c->tx_wm = gi2c_tx_depth - 1;
		geni_se_init(gi2c->base, gi2c->tx_wm, gi2c_tx_depth);
		se_config_packing(gi2c->base, 8, 4, true);
		GENI_SE_DBG(gi2c->ipcl, false, gi2c->dev,
			    "i2c fifo depth:%d\n", gi2c_tx_depth);
	}
	enable_irq(gi2c->irq);
	return 0;
}

static int geni_i2c_suspend_noirq(struct device *device)
{
	if (!pm_runtime_status_suspended(device))
		return -EBUSY;
	return 0;
}
#else
static int geni_i2c_runtime_suspend(struct device *dev)
{
	return 0;
}

static int geni_i2c_runtime_resume(struct device *dev)
{
	return 0;
}

static int geni_i2c_suspend_noirq(struct device *device)
{
	return 0;
}
#endif

static const struct dev_pm_ops geni_i2c_pm_ops = {
	.suspend_noirq		= geni_i2c_suspend_noirq,
	.resume_noirq		= geni_i2c_resume_noirq,
	.runtime_suspend	= geni_i2c_runtime_suspend,
	.runtime_resume		= geni_i2c_runtime_resume,
};

static const struct of_device_id geni_i2c_dt_match[] = {
	{ .compatible = "qcom,i2c-geni" },
	{}
};
MODULE_DEVICE_TABLE(of, geni_i2c_dt_match);

static struct platform_driver geni_i2c_driver = {
	.probe  = geni_i2c_probe,
	.remove = geni_i2c_remove,
	.driver = {
		.name = "i2c_geni",
		.pm = &geni_i2c_pm_ops,
		.of_match_table = geni_i2c_dt_match,
	},
};

module_platform_driver(geni_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:i2c_geni");
