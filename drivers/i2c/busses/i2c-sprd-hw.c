// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/iopoll.h>

#define I2C_CTL			0x000
#define I2C_STATUS		0x014
#define ADDR_DVD0		0x020
#define ADDR_DVD1		0x024
#define ADDR_STA0_DVD		0x028
#define ADDR_RST		0x02c

#define HW_CTL			0x030
#define HW_CHNL_PRIL		0x034
#define HW_CHNL_PRIH		0x038
#define CHNL_EN0		0x060
#define CHNL_EN1		0x064
#define ARM_CMD_WR		0x134
#define ARM_DAT_WR		0x138
#define ARM_RD_CMD		0x13c
#define ARM_RD_DATA		0x140
#define ARM_DEBUG0		0x144
#define ARM_DEBUG1		0x148
#define HW_RST			0x14c

/* I2C_CTL */
#define I2C_DVD_OPT		BIT(8)
#define I2C_OUT_OPT		BIT(7)
#define I2C_TRIM_OPT		BIT(6)
#define I2C_HS_MODE		BIT(4)
#define I2C_MODE		BIT(3)
#define I2C_EN			BIT(2)
#define I2C_INT_EN		BIT(1)
#define I2C_START		BIT(0)

/* I2C_STATUS */
#define I2C_INT			BIT(2)
#define I2C_RX_ACK		BIT(1)
#define I2C_BUSY		BIT(0)

/* ADDR_RST */
#define I2C_RST			BIT(0)

/* timeout (ms) for pm runtime autosuspend */
#define SPRD_I2C_PM_TIMEOUT	1000

/* HW_CTL */
#define HW_CTL_VALUE		0x30
#define PRIL_HIGH_APB		0x64

/* ARM_CMD_WR */
#define REG_ADDR_OFFSET		2
#define SLAVE_ADDR_OFFSET	10
#define ARM_RD_CMD_BUSY		BIT(31)

/* ARM_DEBUG1 */
#define CHNL_PENDING		BIT(6)
#define CHNL_SEL		GENMASK(4, 0)
#define TIMIMG_MAST_L		GENMASK(15, 0)
#define TIMIMG_MAST_H		GENMASK(31, 16)
#define CHNL_WRITE		0
#define CHNL_READ		1

#define I2C_TIMEOUT		1000	/* ms */
/* Absolutely safe for status update at 100 kHz I2C: */
#define I2C_WAIT		200

/* Absolutely safe for wait pending update at 100kHz */
#define I2C_100K_WRITE_WAIT	1500	/* us */

/* Absolutely safe for wait pending update at 400kHz */
#define I2C_400K_WRITE_WAIT	1000	/* us */

/* Absolutely safe for wait pending update at 1MHz */
#define I2C_1M_WRITE_WAIT	600	/* us */

/* Absolutely safe for wait pending update at 3.4MHz */
#define I2C_3M4_WRITE_WAIT	200	/* us */

/* i2c default source clock */
#define I2C_SOURCE_CLK_26M	26000000

/* For 100KHz speed */
#define I2C_CLK_100K	100000

/* For 400KHz speed */
#define I2C_CLK_400K	400000

/* For 3.4MHz speed */
#define I2C_CLK_3M4	3400000

/* For 1MHz speed */
#define I2C_CLK_1M	1000000

/* For 3.4MHz clock adjustment */
#define I2C_CLK_3M4_HIGH_ADJUST	1
#define I2C_CLK_3M4_LOW_ADJUST	1

/* i2c data structure */
struct sprd_i2c_hw {
	struct i2c_msg *msg;
	struct i2c_adapter adap;
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct clk *clk_hw;
	u32 src_clk;
	int irq;
	u32 bus_freq;
	u8 *buf;
	u32 count;
	u32 write_wait_time;
};

static void sprd_i2c_hw_dump_reg(struct sprd_i2c_hw *i2c_dev)
{
	dev_err(&i2c_dev->adap.dev,
		": ======dump i2c-%d reg=======\n", i2c_dev->adap.nr);
	dev_err(&i2c_dev->adap.dev, ": I2C_CTRL:0x%x\n",
		readl(i2c_dev->base + I2C_CTL));
	dev_err(&i2c_dev->adap.dev, ": I2C_STATUS:0x%x\n",
		readl(i2c_dev->base + I2C_STATUS));
	dev_err(&i2c_dev->adap.dev, ": ADDR_DVD0:0x%x\n",
		readl(i2c_dev->base + ADDR_DVD0));
	dev_err(&i2c_dev->adap.dev, ": ADDR_DVD1:0x%x\n",
		readl(i2c_dev->base + ADDR_DVD1));
	dev_err(&i2c_dev->adap.dev, ": ADDR_STA0_DVD:0x%x\n",
		readl(i2c_dev->base + ADDR_STA0_DVD));
	dev_err(&i2c_dev->adap.dev, ": HW_CTL:0x%x\n",
		readl(i2c_dev->base + HW_CTL));
	dev_err(&i2c_dev->adap.dev, ": HW_CHNL_PRIL:0x%x\n",
		readl(i2c_dev->base + HW_CHNL_PRIL));
	dev_err(&i2c_dev->adap.dev, ": CHNL_EN0:0x%x\n",
		readl(i2c_dev->base + CHNL_EN0));
	dev_err(&i2c_dev->adap.dev, ": ARM_CMD_WR:0x%x\n",
		readl(i2c_dev->base + ARM_CMD_WR));
	dev_err(&i2c_dev->adap.dev, ": ARM_DAT_WR:0x%x\n",
		readl(i2c_dev->base + ARM_DAT_WR));
	dev_err(&i2c_dev->adap.dev, ": ARM_RD_CMD:0x%x\n",
		readl(i2c_dev->base + ARM_RD_CMD));
	dev_err(&i2c_dev->adap.dev, ": ARM_DEBUG0:0x%x\n",
		readl(i2c_dev->base + ARM_DEBUG0));
	dev_err(&i2c_dev->adap.dev, ": ARM_DEBUG1:0x%x\n",
		readl(i2c_dev->base + ARM_DEBUG1));
}

static void sprd_i2c_hw_reset_fifo(struct sprd_i2c_hw *i2c_dev)
{
	writel(I2C_RST, i2c_dev->base + ADDR_RST);
}

static int sprd_i2c_hw_writebyte(struct sprd_i2c_hw *i2c_dev, u8 *buf, u32 len)
{
	u32 tmp, status;

	/* only support 2 bytes, 1:reg address; 2:reg data. */
	tmp = i2c_dev->msg->addr << SLAVE_ADDR_OFFSET |
		buf[0] << REG_ADDR_OFFSET;

	/* len: 2 write operation, len:1 read operation */
	if (len == 2) {
		writel(tmp, i2c_dev->base + ARM_CMD_WR);
		writel(buf[1], i2c_dev->base + ARM_DAT_WR);
	} else {
		writel(tmp, i2c_dev->base + ARM_RD_CMD);
		return 0;
	}

	/* waitting for write finish */
	usleep_range(i2c_dev->write_wait_time, i2c_dev->write_wait_time + 50);

	tmp = readl(i2c_dev->base + ARM_DEBUG1);
	if (!(tmp & CHNL_PENDING))
		return 0;

	status = readl(i2c_dev->base + I2C_STATUS);
	if (status & I2C_RX_ACK)  {
		if ((tmp & CHNL_SEL) == CHNL_WRITE)
			dev_err(&i2c_dev->adap.dev, "no ack error!\n");
		else
			dev_err(&i2c_dev->adap.dev, "hw channel no ack error!\n");

		sprd_i2c_hw_dump_reg(i2c_dev);
		return -EIO;
	}

	if ((tmp & CHNL_SEL) == CHNL_WRITE) {
		dev_err(&i2c_dev->adap.dev, "bus error!\n");
		sprd_i2c_hw_dump_reg(i2c_dev);
		sprd_i2c_hw_reset_fifo(i2c_dev);
		return -EIO;
	}

	return 0;
}

static void sprd_i2c_hw_chnl_priority(struct sprd_i2c_hw *i2c_dev)
{
	writel(PRIL_HIGH_APB, i2c_dev->base + HW_CHNL_PRIL);
}

static void sprd_i2c_hw_clear_ack(struct sprd_i2c_hw *i2c_dev)
{
	u32 tmp = readl(i2c_dev->base + I2C_STATUS);

	if (tmp & I2C_RX_ACK)  {
		dev_err(&i2c_dev->adap.dev, "no ack error!\n");
		sprd_i2c_hw_reset_fifo(i2c_dev);
		writel(tmp & ~I2C_RX_ACK, i2c_dev->base + I2C_STATUS);
	}
}

static int sprd_i2c_hw_check_noack(struct sprd_i2c_hw *i2c_dev)
{
	u32 tmp = readl(i2c_dev->base + I2C_STATUS);

	if (tmp & I2C_RX_ACK) {
		dev_warn(&i2c_dev->adap.dev,
			 "i2c report last time: no ack error!\n");
		sprd_i2c_hw_dump_reg(i2c_dev);
		writel(tmp & ~I2C_RX_ACK, i2c_dev->base + I2C_STATUS);
		sprd_i2c_hw_reset_fifo(i2c_dev);

		return -EIO;
	}

	return 0;
}

static int sprd_i2c_hw_readbyte(struct sprd_i2c_hw *i2c_dev, u8 *buf, u32 len)
{
	int data, i, err;

	for (i = 0; i < len; i++) {
		err = readl_poll_timeout_atomic(i2c_dev->base + ARM_RD_DATA,
						data,
						!(data & ARM_RD_CMD_BUSY),
						I2C_WAIT, I2C_TIMEOUT);
		if (err) {
			dev_err(&i2c_dev->adap.dev,
				"Timed out for reading data=0x%04x\n",
				data);
			sprd_i2c_hw_dump_reg(i2c_dev);
			return -ETIMEDOUT;
		}

		buf[i] = data;
	}

	return 0;
}

static int sprd_i2c_hw_handle_msg(struct i2c_adapter *i2c_adap,
				  struct i2c_msg *pmsg)
{
	struct sprd_i2c_hw *i2c_dev = i2c_adap->algo_data;
	int ret;

	i2c_dev->msg = pmsg;
	i2c_dev->buf = pmsg->buf;
	i2c_dev->count = pmsg->len;

	dev_warn(&i2c_dev->adap.dev, "This is a dedicated iic channel for dvfs.\n");
	ret = sprd_i2c_hw_check_noack(i2c_dev);
	if (ret)
		return ret;

	if (i2c_dev->count > 2) {
		dev_err(&i2c_dev->adap.dev,
			"i2c report: only support 1 byte transfer, bytes=%d !!!\n",
			i2c_dev->count);
		return -EINVAL;
	}

	if (pmsg->flags & I2C_M_RD)
		ret = sprd_i2c_hw_readbyte(i2c_dev, i2c_dev->buf,
						i2c_dev->count);
	else
		ret = sprd_i2c_hw_writebyte(i2c_dev, i2c_dev->buf,
						i2c_dev->count);

	/* Transmission is done and clear ack */
	sprd_i2c_hw_clear_ack(i2c_dev);

	return ret;
}

static int sprd_i2c_hw_master_xfer(struct i2c_adapter *i2c_adap,
				   struct i2c_msg *msgs, int num)
{
	int im, ret;
	struct sprd_i2c_hw *i2c_dev = i2c_adap->algo_data;

	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (ret < 0)
		return ret;

	for (im = 0; ret >= 0 && im != num; im++) {
		ret = sprd_i2c_hw_handle_msg(i2c_adap, &msgs[im]);
		if (ret)
			break;
	}

	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);

	return ret >= 0 ? im : ret;
}

static u32 sprd_i2c_hw_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm sprd_i2c_hw_algo = {
	.master_xfer = sprd_i2c_hw_master_xfer,
	.functionality = sprd_i2c_hw_func,
};

static void  sprd_i2c_hw_set_clk(struct sprd_i2c_hw *i2c_dev, u32 freq)
{
	u32 apb_clk = i2c_dev->src_clk, high, low, div0, div1;
	/*
	 * From I2C databook, the prescale calculation formula:
	 * prescale = freq_i2c / (4 * freq_scl) - 1;
	 */
	u32 i2c_dvd = apb_clk / (4 * freq) - 1;
	/*
	 * From I2C databook, the high period of SCL clock is recommended as
	 * 40% (2/5), and the low period of SCL clock is recommended as 60%
	 * (3/5), then the formula should be:
	 * high = (prescale * 2 * 2) / 5
	 * low = (prescale * 2 * 3) / 5
	 *
	 * For high spped mode, the SCL should be adjust after we get the
	 * prescale, we should adjust the high period of SCL clock is
	 * recommended more then 60ns, and the low period of SCL clock
	 * is recommended more then 160ns, then the formula should be:
	 * high = (((i2c_dvd -  I2C_CLK_3M4_HIGH_ADJUST) << 1) * 3) / 10;
	 * low = (((i2c_dvd -  I2C_CLK_3M4_LOW_ADJUST) << 1) * 7) / 10;
	 */
	if (freq == I2C_CLK_3M4) {
		high = (((i2c_dvd -  I2C_CLK_3M4_HIGH_ADJUST) << 1) * 3) / 10;
		low = (((i2c_dvd -  I2C_CLK_3M4_LOW_ADJUST) << 1) * 7) / 10;
	} else {
		high = ((i2c_dvd << 1) * 2) / 6;
		low = ((i2c_dvd << 1) * 3) / 6;
	}

	div0 = (high & TIMIMG_MAST_L) << 16 | (low & TIMIMG_MAST_L);
	div1 =  (high & TIMIMG_MAST_H) | ((low & TIMIMG_MAST_H) >> 16);

	writel(div0, i2c_dev->base + ADDR_DVD0);
	writel(div1, i2c_dev->base + ADDR_DVD1);

	/* Start hold timing = hold time(us) * source clock */
	switch (freq) {
	case I2C_CLK_3M4:
		writel((18 * apb_clk) / 100000000,
			i2c_dev->base + ADDR_STA0_DVD);
		i2c_dev->write_wait_time = I2C_3M4_WRITE_WAIT;
		break;
	case I2C_CLK_1M:
		writel((8 * apb_clk) / 10000000,
			i2c_dev->base + ADDR_STA0_DVD);
		i2c_dev->write_wait_time = I2C_1M_WRITE_WAIT;
		break;
	case I2C_CLK_400K:
		writel((6 * apb_clk) / 10000000,
			i2c_dev->base + ADDR_STA0_DVD);
		i2c_dev->write_wait_time = I2C_400K_WRITE_WAIT;
		break;
	case I2C_CLK_100K:
		writel((4 * apb_clk) / 1000000,
			i2c_dev->base + ADDR_STA0_DVD);
		i2c_dev->write_wait_time = I2C_100K_WRITE_WAIT;
		break;
	default:
		dev_err(&i2c_dev->adap.dev, "Invalid bus freqence!");
		break;
	}
}

static void sprd_i2c_hw_enable(struct sprd_i2c_hw *i2c_dev)
{
	u32 tmp = I2C_DVD_OPT;

	sprd_i2c_hw_clear_ack(i2c_dev);

	writel(tmp, i2c_dev->base + I2C_CTL);
	dev_dbg(&i2c_dev->adap.dev, "freq=%d\n", i2c_dev->bus_freq);

	sprd_i2c_hw_set_clk(i2c_dev, i2c_dev->bus_freq);

	tmp = readl(i2c_dev->base + I2C_CTL);
	writel(tmp | I2C_EN | I2C_INT_EN, i2c_dev->base + I2C_CTL);
	writel(HW_CTL_VALUE, i2c_dev->base + HW_CTL);
}

static int sprd_i2c_hw_clk_init(struct sprd_i2c_hw *i2c_dev)
{
	struct clk *clk_i2c, *clk_parent;

	clk_i2c = devm_clk_get(i2c_dev->dev, "i2c");
	if (IS_ERR(clk_i2c)) {
		dev_warn(&i2c_dev->adap.dev,
			 "i2c%d can't get the i2c clock\n",
			 i2c_dev->adap.nr);
		clk_i2c = NULL;
	}

	clk_parent = devm_clk_get(i2c_dev->dev, "source");
	if (IS_ERR(clk_parent)) {
		dev_warn(&i2c_dev->adap.dev,
			 "i2c%d can't get the source clock\n",
			 i2c_dev->adap.nr);
		clk_parent = NULL;
	}

	if (!!clk_i2c && !!clk_parent && !clk_set_parent(clk_i2c, clk_parent))
		i2c_dev->src_clk = clk_get_rate(clk_i2c);
	else
		i2c_dev->src_clk = I2C_SOURCE_CLK_26M;

	dev_dbg(&i2c_dev->adap.dev, "i2c%d set source clock is %d\n",
		i2c_dev->adap.nr, i2c_dev->src_clk);

	i2c_dev->clk = devm_clk_get(i2c_dev->dev, "enable");
	if (IS_ERR(i2c_dev->clk)) {
		dev_warn(&i2c_dev->adap.dev,
			"i2c%d can't get the enable clock\n",
			i2c_dev->adap.nr);
		i2c_dev->clk = NULL;
	}

	i2c_dev->clk_hw = devm_clk_get(i2c_dev->dev, "clk_hw_i2c");
	if (IS_ERR(i2c_dev->clk_hw)) {
		dev_warn(&i2c_dev->adap.dev,
			 "i2c%d can't get the clk_hw clock\n",
			 i2c_dev->adap.nr);
		i2c_dev->clk_hw = NULL;
	}

	return 0;
}

static int sprd_i2c_hw_probe(struct platform_device *pdev)
{
	int ret;
	u32 prop;
	struct sprd_i2c_hw *i2c_dev;
	struct device_node *np = pdev->dev.of_node;

	pdev->id = of_alias_get_id(np, "i2c");
	if (pdev->id < 0)
		return pdev->id;

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(struct sprd_i2c_hw),
			       GFP_KERNEL);
	if (!i2c_dev)
		return -ENOMEM;

	i2c_dev->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i2c_dev->base))
		return PTR_ERR(i2c_dev->base);

	i2c_dev->irq = platform_get_irq(pdev, 0);
	if (i2c_dev->irq < 0) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		return i2c_dev->irq;
	}

	i2c_set_adapdata(&i2c_dev->adap, i2c_dev);
	snprintf(i2c_dev->adap.name, sizeof(i2c_dev->adap.name),
		 "%s", "sprd-i2c-hw");

	i2c_dev->bus_freq = I2C_CLK_100K;
	i2c_dev->adap.owner = THIS_MODULE;
	i2c_dev->dev = &pdev->dev;
	i2c_dev->adap.retries = 3;
	i2c_dev->adap.algo = &sprd_i2c_hw_algo;
	i2c_dev->adap.algo_data = i2c_dev;
	i2c_dev->adap.dev.parent = &pdev->dev;
	i2c_dev->adap.nr = pdev->id;
	i2c_dev->adap.dev.of_node = pdev->dev.of_node;

	if (!of_property_read_u32(pdev->dev.of_node, "clock-frequency", &prop))
		i2c_dev->bus_freq = prop;

	ret = sprd_i2c_hw_clk_init(i2c_dev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, i2c_dev);

	ret = clk_prepare_enable(i2c_dev->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clk!\n");
		return ret;
	}

	ret = clk_prepare_enable(i2c_dev->clk_hw);
	if (ret) {
		clk_disable_unprepare(i2c_dev->clk);
		dev_err(&pdev->dev, "failed to enable clk_hw!\n");
		return ret;
	}

	sprd_i2c_hw_enable(i2c_dev);
	sprd_i2c_hw_chnl_priority(i2c_dev);

	pm_runtime_set_autosuspend_delay(i2c_dev->dev, SPRD_I2C_PM_TIMEOUT);
	pm_runtime_use_autosuspend(i2c_dev->dev);
	pm_runtime_set_active(i2c_dev->dev);
	pm_runtime_enable(i2c_dev->dev);

	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "i2c%d pm runtime resume failed!\n",
			pdev->id);
		goto err_rpm_put;
	}

	ret = i2c_add_numbered_adapter(&i2c_dev->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "add_adapter failed!\n");
		goto err_rpm_put;
	}

	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);

	return 0;

err_rpm_put:
	pm_runtime_put_noidle(i2c_dev->dev);
	pm_runtime_disable(i2c_dev->dev);
	clk_disable_unprepare(i2c_dev->clk_hw);
	clk_disable_unprepare(i2c_dev->clk);
	return ret;
}

static int sprd_i2c_hw_remove(struct platform_device *pdev)
{
	struct sprd_i2c_hw *i2c_dev = platform_get_drvdata(pdev);
	int ret = pm_runtime_get_sync(i2c_dev->dev);

	if (ret < 0)
		return ret;

	i2c_del_adapter(&i2c_dev->adap);
	clk_disable_unprepare(i2c_dev->clk_hw);
	clk_disable_unprepare(i2c_dev->clk);

	pm_runtime_put_noidle(i2c_dev->dev);
	pm_runtime_disable(i2c_dev->dev);
	return 0;
}

static int __maybe_unused sprd_i2c_hw_suspend_noirq(struct device *pdev)
{
	struct sprd_i2c_hw *i2c_dev = dev_get_drvdata(pdev);

	i2c_mark_adapter_suspended(&i2c_dev->adap);
	return pm_runtime_force_suspend(pdev);
}

static int __maybe_unused sprd_i2c_hw_resume_noirq(struct device *pdev)
{
	struct sprd_i2c_hw *i2c_dev = dev_get_drvdata(pdev);

	i2c_mark_adapter_resumed(&i2c_dev->adap);
	return pm_runtime_force_resume(pdev);
}

static int __maybe_unused sprd_i2c_hw_runtime_suspend(struct device *pdev)
{
	struct sprd_i2c_hw *i2c_dev = dev_get_drvdata(pdev);

	clk_disable_unprepare(i2c_dev->clk_hw);
	clk_disable_unprepare(i2c_dev->clk);

	return 0;
}

static int __maybe_unused sprd_i2c_hw_runtime_resume(struct device *pdev)
{
	struct sprd_i2c_hw *i2c_dev = dev_get_drvdata(pdev);
	int ret = clk_prepare_enable(i2c_dev->clk);

	if (ret) {
		dev_err(pdev, "clk enable fail !!!\n");
		return ret;
	}

	ret = clk_prepare_enable(i2c_dev->clk_hw);
	if (ret) {
		clk_disable_unprepare(i2c_dev->clk);
		dev_err(pdev, "clk_hw enable fail !!!\n");
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops sprd_i2c_hw_pm_ops = {
	SET_RUNTIME_PM_OPS(sprd_i2c_hw_runtime_suspend,
			   sprd_i2c_hw_runtime_resume, NULL)

	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(sprd_i2c_hw_suspend_noirq,
				      sprd_i2c_hw_resume_noirq)
};

static const struct of_device_id sprd_i2c_hw_of_match[] = {
	{ .compatible = "sprd,sharkl3-hw-i2c", },
	{ .compatible = "sprd,sc9860-hw-i2c", },
	{},
};

static struct platform_driver sprd_i2c_hw_driver = {
	.probe = sprd_i2c_hw_probe,
	.remove = sprd_i2c_hw_remove,
	.driver = {
		.name = "sprd-hw-i2c",
		.of_match_table = sprd_i2c_hw_of_match,
		.pm = &sprd_i2c_hw_pm_ops,
	},
};

module_platform_driver(sprd_i2c_hw_driver);
MODULE_DESCRIPTION("Spreadtrum hardware I2C support");
MODULE_LICENSE("GPL v2");
