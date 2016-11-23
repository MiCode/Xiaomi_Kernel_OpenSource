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
#include <linux/platform_device.h>
#include <linux/qcom-geni-se.h>

#define SE_I2C_TX_TRANS_LEN		(0x26C)
#define SE_I2C_RX_TRANS_LEN		(0x270)
#define SE_I2C_SCL_COUNTERS		(0x278)

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

struct geni_i2c_dev {
	struct device *dev;
	void __iomem *base;
	int irq;
	int err;
	struct i2c_adapter adap;
	struct completion xfer;
	struct i2c_msg *cur;
	int cur_wr;
	int cur_rd;
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

static irqreturn_t geni_i2c_irq(int irq, void *dev)
{
	struct geni_i2c_dev *gi2c = dev;
	int i, j;
	u32 m_stat = readl_relaxed(gi2c->base + SE_GENI_M_IRQ_STATUS);
	u32 tx_stat = readl_relaxed(gi2c->base + SE_GENI_TX_FIFO_STATUS);
	u32 rx_stat = readl_relaxed(gi2c->base + SE_GENI_RX_FIFO_STATUS);
	struct i2c_msg *cur = gi2c->cur;

	dev_dbg(gi2c->dev,
		"got i2c irq:%d, stat:0x%x, tx stat:0x%x, rx stat:0x%x\n",
		irq, m_stat, tx_stat, rx_stat);
	if (!cur || m_stat & SE_I2C_ERR) {
		dev_err(gi2c->dev, "i2c txn err");
		writel_relaxed(0, (gi2c->base + SE_GENI_TX_WATERMARK_REG));
		gi2c->err = -EIO;
		goto irqret;
	}
	if (((m_stat & M_RX_FIFO_WATERMARK_EN) ||
		(m_stat & M_RX_FIFO_LAST_EN)) && (cur->flags & I2C_M_RD)) {
		u32 rxcnt = rx_stat & RX_FIFO_WC_MSK;

		for (j = 0; j < rxcnt; j++) {
			u32 temp;
			int p;

			temp = readl_relaxed(gi2c->base + SE_GENI_RX_FIFOn);
			for (i = gi2c->cur_rd, p = 0; (i < cur->len && p < 4);
				i++, p++)
				cur->buf[i] = (u8) ((temp >> (p * 8)) & 0xff);
			gi2c->cur_rd = i;
			if (gi2c->cur_rd == cur->len) {
				dev_dbg(gi2c->dev, "i:%d,read 0x%x\n", i, temp);
				break;
			}
			dev_dbg(gi2c->dev, "i: %d, read 0x%x\n", i, temp);
		}
	} else if ((m_stat & M_TX_FIFO_WATERMARK_EN) &&
					!(cur->flags & I2C_M_RD)) {
		for (j = 0; j < 0x1f; j++) {
			u32 temp = 0;
			int p;

			for (i = gi2c->cur_wr, p = 0; (i < cur->len && p < 4);
				i++, p++)
				temp |= (((u32)(cur->buf[i]) << (p * 8)));
			writel_relaxed(temp, gi2c->base + SE_GENI_TX_FIFOn);
			gi2c->cur_wr = i;
			dev_dbg(gi2c->dev, "i:%d,wrote 0x%x\n", i, temp);
			if (gi2c->cur_wr == cur->len) {
				dev_dbg(gi2c->dev, "i2c bytes done writing\n");
				writel_relaxed(0,
				(gi2c->base + SE_GENI_TX_WATERMARK_REG));
				break;
			}
		}
	}
irqret:
	writel_relaxed(m_stat, gi2c->base + SE_GENI_M_IRQ_CLEAR);
	/* Ensure all writes are done before returning from ISR. */
	wmb();
	/* if this is err with done-bit not set, handle that thr' timeout. */
	if (m_stat & M_CMD_DONE_EN) {
		dev_dbg(gi2c->dev, "i2c irq: err:%d, stat:0x%x\n",
							gi2c->err, m_stat);
		complete(&gi2c->xfer);
	}
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
	enable_irq(gi2c->irq);
	qcom_geni_i2c_conf(gi2c->base, 0, 2);
	se_config_packing(gi2c->base, 8, 4, true);
	dev_dbg(gi2c->dev, "i2c xfer:num:%d, msgs:len:%d,flg:%d\n",
				num, msgs[0].len, msgs[0].flags);
	for (i = 0; i < num; i++) {
		int stretch = (i < (num - 1));
		u32 m_param = 0;
		u32 m_cmd = 0;

		m_param |= (stretch ? STOP_STRETCH : ~(STOP_STRETCH));
		m_param |= ((msgs[i].addr & 0x7F) << SLV_ADDR_SHFT);

		gi2c->cur = &msgs[i];
		if (msgs[i].flags & I2C_M_RD) {
			dev_dbg(gi2c->dev,
				"READ,n:%d,i:%d len:%d, stretch:%d\n",
					num, i, msgs[i].len, stretch);
			geni_write_reg(msgs[i].len,
				       gi2c->base, SE_I2C_RX_TRANS_LEN);
			m_cmd = I2C_READ;
			geni_setup_m_cmd(gi2c->base, m_cmd, m_param);
		} else {
			dev_dbg(gi2c->dev,
				"WRITE:n:%d,i%d len:%d, stretch:%d\n",
					num, i, msgs[i].len, stretch);
			geni_write_reg(msgs[i].len, gi2c->base,
						SE_I2C_TX_TRANS_LEN);
			m_cmd = I2C_WRITE;
			geni_setup_m_cmd(gi2c->base, m_cmd, m_param);
			/* Get FIFO IRQ */
			geni_write_reg(1, gi2c->base, SE_GENI_TX_WATERMARK_REG);
		}
		/* Ensure FIFO write go through before waiting for Done evet */
		mb();
		timeout = wait_for_completion_timeout(&gi2c->xfer, HZ);
		if (!timeout) {
			dev_err(gi2c->dev, "Timed out\n");
			gi2c->err = -ETIMEDOUT;
			gi2c->cur = NULL;
			geni_abort_m_cmd(gi2c->base);
			timeout = wait_for_completion_timeout(&gi2c->xfer, HZ);
		}
		gi2c->cur_wr = 0;
		gi2c->cur_rd = 0;
		if (gi2c->err) {
			dev_err(gi2c->dev, "i2c error :%d\n", gi2c->err);
			ret = gi2c->err;
			break;
		}
	}
	if (ret == 0)
		ret = i;
	disable_irq(gi2c->irq);
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
	int ret;

	gi2c = devm_kzalloc(&pdev->dev, sizeof(*gi2c), GFP_KERNEL);
	if (!gi2c)
		return -ENOMEM;

	gi2c->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	gi2c->base = devm_ioremap_resource(gi2c->dev, res);
	if (IS_ERR(gi2c->base))
		return PTR_ERR(gi2c->base);

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

	i2c_add_adapter(&gi2c->adap);
	geni_se_init(gi2c->base, FIFO_MODE, 0xF, 0x10);

	return 0;
}

static int geni_i2c_remove(struct platform_device *pdev)
{
	struct geni_i2c_dev *gi2c = platform_get_drvdata(pdev);

	disable_irq(gi2c->irq);
	i2c_del_adapter(&gi2c->adap);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int geni_i2c_suspend(struct device *device)
{
	return 0;
}

static int geni_i2c_resume(struct device *device)
{
	return 0;
}
#endif

static const struct dev_pm_ops geni_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		geni_i2c_suspend,
		geni_i2c_resume)
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
