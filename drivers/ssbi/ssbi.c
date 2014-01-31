/* Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2010, Google Inc.
 *
 * Original authors: Code Aurora Forum
 *
 * Author: Dima Zavin <dima@android.com>
 *  - Largely rewritten from original to not be an i2c driver.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ssbi.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

/* SSBI 2.0 controller registers */
#define SSBI2_CMD			0x0008
#define SSBI2_RD			0x0010
#define SSBI2_STATUS			0x0014
#define SSBI2_MODE2			0x001C

/* SSBI_CMD fields */
#define SSBI_CMD_RDWRN			(1 << 24)

/* SSBI_STATUS fields */
#define SSBI_STATUS_RD_READY		(1 << 2)
#define SSBI_STATUS_READY		(1 << 1)
#define SSBI_STATUS_MCHN_BUSY		(1 << 0)

/* SSBI_MODE2 fields */
#define SSBI_MODE2_REG_ADDR_15_8_SHFT	0x04
#define SSBI_MODE2_REG_ADDR_15_8_MASK	(0x7f << SSBI_MODE2_REG_ADDR_15_8_SHFT)

#define SET_SSBI_MODE2_REG_ADDR_15_8(MD, AD) \
	(((MD) & 0x0F) | ((((AD) >> 8) << SSBI_MODE2_REG_ADDR_15_8_SHFT) & \
	SSBI_MODE2_REG_ADDR_15_8_MASK))

/* SSBI PMIC Arbiter command registers */
#define SSBI_PA_CMD			0x0000
#define SSBI_PA_RD_STATUS		0x0004

/* SSBI_PA_CMD fields */
#define SSBI_PA_CMD_RDWRN		(1 << 24)
#define SSBI_PA_CMD_ADDR_MASK		0x7fff /* REG_ADDR_7_0, REG_ADDR_8_14*/

/* SSBI_PA_RD_STATUS fields */
#define SSBI_PA_RD_STATUS_TRANS_DONE	(1 << 27)
#define SSBI_PA_RD_STATUS_TRANS_DENIED	(1 << 26)

#define SSBI_TIMEOUT_US			100

/* GENI SSBI Arbiter command registers */
#define  GENI_SSBI_ARB_CHNL_CMD		0x3800
#define  GENI_SSBI_ARB_CHNL_CONFIG	0x3804
#define  GENI_SSBI_ARB_CHNL_STATUS	0x3808
#define  GENI_SSBI_ARB_CHNL_WDATA0	0x3810
#define  GENI_SSBI_ARB_CHNL_WDATA1	0x3814
#define  GENI_SSBI_ARB_CHNL_RDATA0	0x3818
#define  GENI_SSBI_ARB_CHNL_RDATA1	0x381C

/* GENI SSBI Arbiter CMD fields */
#define GENI_SSBI_CMD_WR		(0 << 27)
#define GENI_SSBI_CMD_RD		(1 << 27)
#define GENI_SSBI2_CMD_WR		(2 << 27)
#define GENI_SSBI2_CMD_RD		(3 << 27)

/* GENI SSBI ARB STATUS fields */
#define GENI_SSBI_STATUS_TRANS_DONE	1
#define GENI_SSBI_STATUS_TRANS_FAILURE  2
#define GENI_SSBI_STATUS_TRANS_DENIED	4
#define GENI_SSBI_STATUS_TRANS_DROPPED	8

#define SSBI_GSA_CMD_ADDR_MASK		0x7fff /* REG_ADDR_7_0, REG_ADDR_8_14*/

#define GENI_OFFSET			  0x4000
#define GENI_CFG_REG0_ADDR		  0x100
#define GENI_RAM_ADDR			  0x200

#define GENI_CLK_CONTROL_ADDR		  0x0
#define GENI_FW_REVISION_ADDR		  0x8
#define GENI_S_FW_REVISION_ADDR		  0xC
#define GENI_FORCE_DEFAULT_REG_ADDR       0x10
#define GENI_OUTPUT_CONTROL_ADDR	  0x14
#define GENI_SER_CLK_CFG_ADDR		  0x34
#define GENI_ARB_MISC_CONFIG_ADDR	  0x3004
#define GENI_ARB_CHNL_CONFIG_ADDDR	  0x3804
#define GENI_FW_VERSION			  0x00000140
#define GENI_DEFAULT			  0x00000001
#define GENI_SOE0_EN			  0x00000001
#define GENI_SER_CLK			  0x00000001
#define GENI_IRQ_DONE			  0x00000001
#define GENI_IRQ_EN			  0x00000001
#define GENI_CLK_SET			  0x00004006
#define GENI_CFG_SIZE			  31
#define GENI_RAM_SIZE			  25
#define GENI_MAX_CHNL			  5

int mode_1_cfg_array[GENI_CFG_SIZE] = {
	0x00004879, 0x0001F178, 0x0000FEEE, 0x00000000, 0x00000000,
	0x108802A8, 0x00000110, 0x00280000, 0x0000001F, 0x000000FF,
	0x00000000, 0x00554515, 0x00555555, 0x00000000, 0x00000000,
	0x001800A4, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000001, 0x00000020, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000
};

int mode_1_ram_array[GENI_RAM_SIZE] = {
	0x0000000f, 0x00000000, 0x00014201, 0x001E4880, 0x0001B203,
	0x001E4880, 0x00024201, 0x001E4880, 0x0002D203, 0x001E4880,
	0x00040008, 0x000A2216, 0x00142E08, 0x00040008, 0x000A221C,
	0x0004601A, 0x00005E00, 0x00101000, 0x00041210, 0x000A2226,
	0x00041017, 0x00101000, 0x00001200, 0x0008201C, 0x00000000
};

int mode_2_cfg_array[GENI_CFG_SIZE] = {
	0x00004879, 0x00027978, 0x0000FEEE, 0x00000000, 0x00000000,
	0x108802A8, 0x00000110, 0x00280000, 0x0000001F, 0x000000FF,
	0x00000000, 0x00554515, 0x00555555, 0x00000000, 0x00000000,
	0x001800A4, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000001, 0x00000020, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000
};

int mode_2_ram_array[GENI_RAM_SIZE] = {
	0x0000000f, 0x00000000, 0x00014201, 0x001E48C0, 0x0002D203,
	0x001E48C0, 0x00024201, 0x001E48C0, 0x0002D203, 0x001E48C0,
	0x00040008, 0x000A2216, 0x00142E08, 0x00040008, 0x000A221C,
	0x0004601A, 0x00005E00, 0x00101000, 0x00041210, 0x000A2226,
	0x00041017, 0x00101000, 0x00001200, 0x0008201C, 0x00000000
};

struct ssbi {
	struct device		*slave;
	void __iomem		*base;
	spinlock_t		lock;
	enum ssbi_controller_type controller_type;
	int (*read)(struct ssbi *, u16 addr, u8 *buf, int len);
	int (*write)(struct ssbi *, u16 addr, u8 *buf, int len);
};

#define to_ssbi(dev)	platform_get_drvdata(to_platform_device(dev))

static inline u32 ssbi_readl(struct ssbi *ssbi, u32 reg)
{
	return readl_relaxed(ssbi->base + reg);
}

static inline void ssbi_writel(struct ssbi *ssbi, u32 val, u32 reg)
{
	writel_relaxed(val, ssbi->base + reg);
}

/*
 * Via private exchange with one of the original authors, the hardware
 * should generally finish a transaction in about 5us.  The worst
 * case, is when using the arbiter and both other CPUs have just
 * started trying to use the SSBI bus will result in a time of about
 * 20us.  It should never take longer than this.
 *
 * As such, this wait merely spins, with a udelay.
 */
static int ssbi_wait_mask(struct ssbi *ssbi, u32 set_mask, u32 clr_mask)
{
	u32 timeout = SSBI_TIMEOUT_US;
	u32 val;

	while (timeout--) {
		val = ssbi_readl(ssbi, SSBI2_STATUS);
		if (((val & set_mask) == set_mask) && ((val & clr_mask) == 0))
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int
ssbi_read_bytes(struct ssbi *ssbi, u16 addr, u8 *buf, int len)
{
	u32 cmd = SSBI_CMD_RDWRN | ((addr & 0xff) << 16);
	int ret = 0;

	if (ssbi->controller_type == MSM_SBI_CTRL_SSBI2) {
		u32 mode2 = ssbi_readl(ssbi, SSBI2_MODE2);
		mode2 = SET_SSBI_MODE2_REG_ADDR_15_8(mode2, addr);
		ssbi_writel(ssbi, mode2, SSBI2_MODE2);
	}

	while (len) {
		ret = ssbi_wait_mask(ssbi, SSBI_STATUS_READY, 0);
		if (ret)
			goto err;

		ssbi_writel(ssbi, cmd, SSBI2_CMD);
		ret = ssbi_wait_mask(ssbi, SSBI_STATUS_RD_READY, 0);
		if (ret)
			goto err;
		*buf++ = ssbi_readl(ssbi, SSBI2_RD) & 0xff;
		len--;
	}

err:
	return ret;
}

static int
ssbi_write_bytes(struct ssbi *ssbi, u16 addr, u8 *buf, int len)
{
	int ret = 0;

	if (ssbi->controller_type == MSM_SBI_CTRL_SSBI2) {
		u32 mode2 = ssbi_readl(ssbi, SSBI2_MODE2);
		mode2 = SET_SSBI_MODE2_REG_ADDR_15_8(mode2, addr);
		ssbi_writel(ssbi, mode2, SSBI2_MODE2);
	}

	while (len) {
		ret = ssbi_wait_mask(ssbi, SSBI_STATUS_READY, 0);
		if (ret)
			goto err;

		ssbi_writel(ssbi, ((addr & 0xff) << 16) | *buf, SSBI2_CMD);
		ret = ssbi_wait_mask(ssbi, 0, SSBI_STATUS_MCHN_BUSY);
		if (ret)
			goto err;
		buf++;
		len--;
	}

err:
	return ret;
}

/*
 * See ssbi_wait_mask for an explanation of the time and the
 * busywait.
 */
static inline int
ssbi_pa_transfer(struct ssbi *ssbi, u32 cmd, u8 *data)
{
	u32 timeout = SSBI_TIMEOUT_US;
	u32 rd_status = 0;

	ssbi_writel(ssbi, cmd, SSBI_PA_CMD);

	while (timeout--) {
		rd_status = ssbi_readl(ssbi, SSBI_PA_RD_STATUS);

		if (rd_status & SSBI_PA_RD_STATUS_TRANS_DENIED)
			return -EPERM;

		if (rd_status & SSBI_PA_RD_STATUS_TRANS_DONE) {
			if (data)
				*data = rd_status & 0xff;
			return 0;
		}
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int
ssbi_pa_read_bytes(struct ssbi *ssbi, u16 addr, u8 *buf, int len)
{
	u32 cmd;
	int ret = 0;

	cmd = SSBI_PA_CMD_RDWRN | (addr & SSBI_PA_CMD_ADDR_MASK) << 8;

	while (len) {
		ret = ssbi_pa_transfer(ssbi, cmd, buf);
		if (ret)
			goto err;
		buf++;
		len--;
	}

err:
	return ret;
}

static int
ssbi_pa_write_bytes(struct ssbi *ssbi, u16 addr, u8 *buf, int len)
{
	u32 cmd;
	int ret = 0;

	while (len) {
		cmd = (addr & SSBI_PA_CMD_ADDR_MASK) << 8 | *buf;
		ret = ssbi_pa_transfer(ssbi, cmd, NULL);
		if (ret)
			goto err;
		buf++;
		len--;
	}

err:
	return ret;
}

static inline int
ssbi_gsa_transfer(struct ssbi *ssbi, u32 cmd, u8 *data)
{
	u32 timeout = SSBI_TIMEOUT_US;
	u32 rd_status = 0;

	ssbi_writel(ssbi, cmd, GENI_SSBI_ARB_CHNL_CMD);

	while (timeout--) {
		rd_status = ssbi_readl(ssbi, GENI_SSBI_ARB_CHNL_STATUS);

		if (rd_status & GENI_SSBI_STATUS_TRANS_DENIED)
			return -EPERM;

		if (rd_status & GENI_SSBI_STATUS_TRANS_FAILURE)
			return -EPERM;

		if (rd_status & GENI_SSBI_STATUS_TRANS_DROPPED)
			return -EPERM;

		if (rd_status & GENI_SSBI_STATUS_TRANS_DONE) {
			if (data)
				*data = ssbi_readl(ssbi,
					GENI_SSBI_ARB_CHNL_RDATA0) & 0xff;
			return 0;
		}
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int
ssbi_gsa_read_bytes(struct ssbi *ssbi, u16 addr, u8 *buf, int len)
{
	u32 cmd;
	int ret = 0;

	if (ssbi->controller_type == FSM_SBI_CTRL_GENI_SSBI2_ARBITER)
		cmd = GENI_SSBI2_CMD_RD | (addr & SSBI_GSA_CMD_ADDR_MASK) << 8;
	else
		cmd = GENI_SSBI_CMD_RD | (addr & SSBI_GSA_CMD_ADDR_MASK) << 8;

	while (len) {
		ret = ssbi_gsa_transfer(ssbi, cmd, buf);
		if (ret)
			goto err;
		buf++;
		len--;
	}

err:
	return ret;
}

static int
ssbi_gsa_write_bytes(struct ssbi *ssbi, u16 addr, u8 *buf, int len)
{
	u32 cmd;
	int ret = 0;

	while (len) {
		if (ssbi->controller_type == FSM_SBI_CTRL_GENI_SSBI2_ARBITER)
			cmd = GENI_SSBI2_CMD_WR |
				(addr & SSBI_GSA_CMD_ADDR_MASK) << 8 | *buf;
		else
			cmd = (addr & SSBI_GSA_CMD_ADDR_MASK) << 8 | *buf;

		ret = ssbi_gsa_transfer(ssbi, cmd, NULL);
		if (ret)
			goto err;
		buf++;
		len--;
	}

err:
	return ret;
}

int ssbi_read(struct device *dev, u16 addr, u8 *buf, int len)
{
	struct ssbi *ssbi = to_ssbi(dev);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ssbi->lock, flags);
	ret = ssbi->read(ssbi, addr, buf, len);
	spin_unlock_irqrestore(&ssbi->lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(ssbi_read);

int ssbi_write(struct device *dev, u16 addr, u8 *buf, int len)
{
	struct ssbi *ssbi = to_ssbi(dev);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ssbi->lock, flags);
	ret = ssbi->write(ssbi, addr, buf, len);
	spin_unlock_irqrestore(&ssbi->lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(ssbi_write);

static void  set_ssbi_mode_2(void __iomem *geni_offset)
{
	int i;

	writel_relaxed(GENI_FW_VERSION, geni_offset + GENI_FW_REVISION_ADDR);
	writel_relaxed(GENI_FW_VERSION, geni_offset + GENI_S_FW_REVISION_ADDR);

	for (i = 0; i < GENI_CFG_SIZE; i++)
		writel_relaxed(mode_2_cfg_array[i], geni_offset +
			GENI_CFG_REG0_ADDR + 4 * i);

	for (i = 0; i < GENI_RAM_SIZE; i++)
		writel_relaxed(mode_2_ram_array[i], geni_offset +
			GENI_RAM_ADDR + 4 * i);

	writel_relaxed(GENI_DEFAULT, geni_offset + GENI_FORCE_DEFAULT_REG_ADDR);
	writel_relaxed(GENI_SOE0_EN, geni_offset + GENI_OUTPUT_CONTROL_ADDR);
	writel_relaxed(GENI_SER_CLK, geni_offset + GENI_CLK_CONTROL_ADDR);
	writel_relaxed(GENI_CLK_SET, geni_offset + GENI_SER_CLK_CFG_ADDR);

	writel_relaxed(GENI_IRQ_DONE, geni_offset + GENI_ARB_MISC_CONFIG_ADDR);

	for (i = 0; i < GENI_MAX_CHNL; i++)
		writel_relaxed(GENI_IRQ_EN, geni_offset +
			GENI_ARB_CHNL_CONFIG_ADDDR + 4 * i);
}

static void  set_ssbi_mode_1(void __iomem *geni_offset)
{
	int i;

	writel_relaxed(GENI_FW_VERSION, geni_offset + GENI_FW_REVISION_ADDR);
	writel_relaxed(GENI_FW_VERSION, geni_offset + GENI_S_FW_REVISION_ADDR);

	for (i = 0; i < GENI_CFG_SIZE; i++)
		writel_relaxed(mode_1_cfg_array[i], geni_offset +
			GENI_CFG_REG0_ADDR + 4 * i);

	for (i = 0; i < GENI_RAM_SIZE; i++)
		writel_relaxed(mode_1_ram_array[i], geni_offset +
			GENI_RAM_ADDR + 4 * i);

	writel_relaxed(GENI_DEFAULT, geni_offset + GENI_FORCE_DEFAULT_REG_ADDR);
	writel_relaxed(GENI_SOE0_EN, geni_offset + GENI_OUTPUT_CONTROL_ADDR);
	writel_relaxed(GENI_SER_CLK, geni_offset + GENI_CLK_CONTROL_ADDR);
	writel_relaxed(GENI_CLK_SET, geni_offset + GENI_SER_CLK_CFG_ADDR);

	writel_relaxed(GENI_IRQ_DONE, geni_offset + GENI_ARB_MISC_CONFIG_ADDR);

	for (i = 0; i < GENI_MAX_CHNL; i++)
		writel_relaxed(GENI_IRQ_EN, geni_offset +
			GENI_ARB_CHNL_CONFIG_ADDDR + 4 * i);
}

static int ssbi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *mem_res;
	struct ssbi *ssbi;
	int ret = 0;
	const char *type;

	ssbi = kzalloc(sizeof(struct ssbi), GFP_KERNEL);
	if (!ssbi) {
		pr_err("can not allocate ssbi_data\n");
		return -ENOMEM;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		pr_err("missing mem resource\n");
		ret = -EINVAL;
		goto err_get_mem_res;
	}

	ssbi->base = ioremap(mem_res->start, resource_size(mem_res));
	if (!ssbi->base) {
		pr_err("ioremap of 0x%p failed\n", (void *)mem_res->start);
		ret = -EINVAL;
		goto err_ioremap;
	}

	platform_set_drvdata(pdev, ssbi);

	type = of_get_property(np, "qcom,controller-type", NULL);
	if (type == NULL) {
		pr_err("Missing qcom,controller-type property\n");
		ret = -EINVAL;
		goto err_ssbi_controller;
	}
	dev_info(&pdev->dev, "SSBI controller type: '%s'\n", type);
	if (strcmp(type, "ssbi") == 0)
		ssbi->controller_type = MSM_SBI_CTRL_SSBI;
	else if (strcmp(type, "ssbi2") == 0)
		ssbi->controller_type = MSM_SBI_CTRL_SSBI2;
	else if (strcmp(type, "pmic-arbiter") == 0)
		ssbi->controller_type = MSM_SBI_CTRL_PMIC_ARBITER;
	else if (strcmp(type, "geni-ssbi-arbiter") == 0) {
		ssbi->controller_type = FSM_SBI_CTRL_GENI_SSBI_ARBITER;
		set_ssbi_mode_1(ssbi->base);
	} else if (strcmp(type, "geni-ssbi2-arbiter") == 0) {
		ssbi->controller_type = FSM_SBI_CTRL_GENI_SSBI2_ARBITER;
		set_ssbi_mode_2(ssbi->base);
	} else {
		pr_err("Unknown qcom,controller-type\n");
		ret = -EINVAL;
		goto err_ssbi_controller;
	}

	if (ssbi->controller_type == MSM_SBI_CTRL_PMIC_ARBITER) {
		ssbi->read = ssbi_pa_read_bytes;
		ssbi->write = ssbi_pa_write_bytes;
	} else if ((ssbi->controller_type == FSM_SBI_CTRL_GENI_SSBI_ARBITER) ||
		(ssbi->controller_type == FSM_SBI_CTRL_GENI_SSBI2_ARBITER)) {
		ssbi->read = ssbi_gsa_read_bytes;
		ssbi->write = ssbi_gsa_write_bytes;
	} else {
		ssbi->read = ssbi_read_bytes;
		ssbi->write = ssbi_write_bytes;
	}

	spin_lock_init(&ssbi->lock);

	ret = of_platform_populate(np, NULL, NULL, &pdev->dev);
	if (ret)
		goto err_ssbi_controller;

	return 0;

err_ssbi_controller:
	platform_set_drvdata(pdev, NULL);
	iounmap(ssbi->base);
err_ioremap:
err_get_mem_res:
	kfree(ssbi);
	return ret;
}

static int ssbi_remove(struct platform_device *pdev)
{
	struct ssbi *ssbi = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	iounmap(ssbi->base);
	kfree(ssbi);
	return 0;
}

static struct of_device_id ssbi_match_table[] = {
	{ .compatible = "qcom,ssbi" },
	{}
};

static struct platform_driver ssbi_driver = {
	.probe		= ssbi_probe,
	.remove		= ssbi_remove,
	.driver		= {
		.name	= "ssbi",
		.owner	= THIS_MODULE,
		.of_match_table = ssbi_match_table,
	},
};

static int __init ssbi_init(void)
{
	return platform_driver_register(&ssbi_driver);
}
module_init(ssbi_init);

static void __exit ssbi_exit(void)
{
	platform_driver_unregister(&ssbi_driver);
}
module_exit(ssbi_exit)

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:ssbi");
MODULE_AUTHOR("Dima Zavin <dima@android.com>");
