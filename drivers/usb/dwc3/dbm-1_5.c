/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "dbm.h"

/**
*  USB DBM Hardware registers.
*
*/
#define DBM_EP_CFG(n)		(0x00 + 4 * (n))
#define DBM_DATA_FIFO(n)	(0x280 + 4 * (n))
#define DBM_DATA_FIFO_SIZE(n)	(0x80 + 4 * (n))
#define DBM_DATA_FIFO_EN	(0x26C)
#define DBM_GEVNTADR		(0x270)
#define DBM_GEVNTSIZ		(0x268)
#define DBM_DBG_CNFG		(0x208)
#define DBM_HW_TRB0_EP(n)	(0x220 + 4 * (n))
#define DBM_HW_TRB1_EP(n)	(0x230 + 4 * (n))
#define DBM_HW_TRB2_EP(n)	(0x240 + 4 * (n))
#define DBM_HW_TRB3_EP(n)	(0x250 + 4 * (n))
#define DBM_PIPE_CFG		(0x274)
#define DBM_SOFT_RESET		(0x20C)
#define DBM_GEN_CFG		(0x210)
#define DBM_GEVNTADR_LSB	(0x260)
#define DBM_GEVNTADR_MSB	(0x264)
#define DBM_DATA_FIFO_LSB(n)	(0x100 + 8 * (n))
#define DBM_DATA_FIFO_MSB(n)	(0x104 + 8 * (n))

#define DBM_DATA_FIFO_ADDR_EN	(0x200)
#define DBM_DATA_FIFO_SIZE_EN	(0x204)

#define DBM_1_5_NUM_EP		8

struct dbm_data {
	void __iomem *base;

	int dbm_num_eps;
	u8 ep_num_mapping[DBM_1_5_NUM_EP];
	bool dbm_reset_ep_after_lpm;
};

static struct dbm_data *dbm_data;

/**
 * Write register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 * @val - value to write.
 *
 */
static inline void msm_dbm_write_reg_field(void *base, u32 offset,
					    const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = ioread32(base + offset);

	tmp &= ~mask;		/* clear written bits */
	val = tmp | (val << shift);
	iowrite32(val, base + offset);
}

/**
 *
 * Read register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 *
 * @return u32
 */
static inline u32 msm_dbm_read_reg(void *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	return val;
}

/**
 *
 * Write register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @val - value to write.
 *
 */
static inline void msm_dbm_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}


/**
 * Return DBM EP number according to usb endpoint number.
 *
 */
static int msm_dbm_find_matching_dbm_ep(u8 usb_ep)
{
	int i;

	for (i = 0; i < dbm_data->dbm_num_eps; i++)
		if (dbm_data->ep_num_mapping[i] == usb_ep)
			return i;

	pr_err("%s: No DBM EP matches USB EP %d", __func__, usb_ep);
	return -ENODEV; /* Not found */
}


/**
 * Reset the DBM registers upon initialization.
 *
 */
static int soft_reset(bool reset)
{
	pr_debug("%s DBM reset\n", (reset ? "Enter" : "Exit"));

	msm_dbm_write_reg_field(dbm_data->base, DBM_SOFT_RESET,
		DBM_SFT_RST_MASK, reset);

	return 0;
}

/**
 * Soft reset specific DBM ep.
 * This function is called by the function driver upon events
 * such as transfer aborting, USB re-enumeration and USB
 * disconnection.
 *
 * @dbm_ep - DBM ep number.
 * @enter_reset - should we enter a reset state or get out of it.
 *
 */
static int ep_soft_reset(u8 dbm_ep, bool enter_reset)
{
	pr_debug("Setting DBM ep %d reset to %d\n", dbm_ep, enter_reset);

	if (dbm_ep >= dbm_data->dbm_num_eps) {
		pr_err("Invalid DBM ep index %d\n", dbm_ep);
		return -ENODEV;
	}

	if (enter_reset) {
		msm_dbm_write_reg_field(dbm_data->base, DBM_SOFT_RESET,
			DBM_SFT_RST_EPS_MASK & 1 << dbm_ep, 1);
	} else {
		msm_dbm_write_reg_field(dbm_data->base, DBM_SOFT_RESET,
			DBM_SFT_RST_EPS_MASK & 1 << dbm_ep, 0);
	}

	return 0;
}


/**
 * Soft reset specific DBM ep (by USB EP number).
 * This function is called by the function driver upon events
 * such as transfer aborting, USB re-enumeration and USB
 * disconnection.
 *
 * The function relies on ep_soft_reset() for checking
 * the legality of the resulting DBM ep number.
 *
 * @usb_ep - USB ep number.
 * @enter_reset - should we enter a reset state or get out of it.
 *
 */
static int usb_ep_soft_reset(u8 usb_ep, bool enter_reset)
{
	int dbm_ep = msm_dbm_find_matching_dbm_ep(usb_ep);

	pr_debug("Setting USB ep %d reset to %d\n", usb_ep, enter_reset);
	return ep_soft_reset(dbm_ep, enter_reset);
}

/**
 * Configure a USB DBM ep to work in BAM mode.
 *
 *
 * @usb_ep - USB physical EP number.
 * @producer - producer/consumer.
 * @disable_wb - disable write back to system memory.
 * @internal_mem - use internal USB memory for data fifo.
 * @ioc - enable interrupt on completion.
 *
 * @return int - DBM ep number.
 */
static int ep_config(u8 usb_ep, u8 bam_pipe, bool producer, bool disable_wb,
		bool internal_mem, bool ioc)
{
	int dbm_ep;
	u32 ep_cfg;

	pr_debug("Configuring DBM ep\n");

	dbm_ep = msm_dbm_find_matching_dbm_ep(usb_ep);

	if (dbm_ep < 0) {
		pr_err("usb ep index %d has no corresponding dbm ep\n", usb_ep);
		return -ENODEV;
	}

	/* Due to HW issue, EP 7 can be set as IN EP only */
	if (dbm_ep == 7 && producer) {
		pr_err("last DBM EP can't be OUT EP\n");
		return -ENODEV;
	}

	/* First, reset the dbm endpoint */
	ep_soft_reset(dbm_ep, 0);

	/* Set ioc bit for dbm_ep if needed */
	msm_dbm_write_reg_field(dbm_data->base, DBM_DBG_CNFG,
		DBM_ENABLE_IOC_MASK & 1 << dbm_ep, ioc ? 1 : 0);

	ep_cfg = (producer ? DBM_PRODUCER : 0) |
		(disable_wb ? DBM_DISABLE_WB : 0) |
		(internal_mem ? DBM_INT_RAM_ACC : 0);

	msm_dbm_write_reg_field(dbm_data->base, DBM_EP_CFG(dbm_ep),
		DBM_PRODUCER | DBM_DISABLE_WB | DBM_INT_RAM_ACC, ep_cfg >> 8);

	msm_dbm_write_reg_field(dbm_data->base, DBM_EP_CFG(dbm_ep), USB3_EPNUM,
		usb_ep);

	msm_dbm_write_reg_field(dbm_data->base, DBM_EP_CFG(dbm_ep), DBM_EN_EP,
		1);

	return dbm_ep;
}

/**
 * Return number of configured DBM endpoints.
 */
static int get_num_of_eps_configured(void)
{
	int i;
	int count = 0;

	for (i = 0; i < dbm_data->dbm_num_eps; i++)
		if (dbm_data->ep_num_mapping[i])
			count++;

	return count;
}

/**
 * Configure a USB DBM ep to work in normal mode.
 *
 * @usb_ep - USB ep number.
 *
 */
static int ep_unconfig(u8 usb_ep)
{
	int dbm_ep;
	u32 data;

	pr_debug("Unconfiguring DB ep\n");

	dbm_ep = msm_dbm_find_matching_dbm_ep(usb_ep);

	if (dbm_ep < 0) {
		pr_err("usb ep index %d has no corresponding dbm ep\n", usb_ep);
		return -ENODEV;
	}

	dbm_data->ep_num_mapping[dbm_ep] = 0;

	data = msm_dbm_read_reg(dbm_data->base, DBM_EP_CFG(dbm_ep));
	data &= (~0x1);
	msm_dbm_write_reg(dbm_data->base, DBM_EP_CFG(dbm_ep), data);

	/* Reset the dbm endpoint */
	ep_soft_reset(dbm_ep, true);
	/*
	 * The necessary delay between asserting and deasserting the dbm ep
	 * reset is based on the number of active endpoints. If there is more
	 * than one endpoint, a 1 msec delay is required. Otherwise, a shorter
	 * delay will suffice.
	 *
	 * As this function can be called in atomic context, sleeping variants
	 * for delay are not possible - albeit a 1ms delay.
	 */
	if (get_num_of_eps_configured() > 1)
		udelay(1000);
	else
		udelay(10);
	ep_soft_reset(dbm_ep, false);

	return 0;
}

/**
 * Configure the DBM with the USB3 core event buffer.
 * This function is called by the SNPS UDC upon initialization.
 *
 * @addr - address of the event buffer.
 * @size - size of the event buffer.
 *
 */
static int event_buffer_config(u32 addr_lo, u32 addr_hi, int size)
{
	pr_debug("Configuring event buffer\n");

	if (size < 0) {
		pr_err("Invalid size. size = %d", size);
		return -EINVAL;
	}

	/* In case event buffer is already configured, Do nothing. */
	if (msm_dbm_read_reg(dbm_data->base, DBM_GEVNTSIZ))
		return 0;

	msm_dbm_write_reg(dbm_data->base, DBM_GEVNTADR_LSB, addr_lo);
	msm_dbm_write_reg(dbm_data->base, DBM_GEVNTADR_MSB, addr_hi);
	msm_dbm_write_reg_field(dbm_data->base, DBM_GEVNTSIZ,
		DBM_GEVNTSIZ_MASK, size);

	return 0;
}


static int data_fifo_config(u8 dep_num, phys_addr_t addr,
		 u32 size, u8 dst_pipe_idx)
{
	u8 dbm_ep = dst_pipe_idx;
	u32 lo = lower_32_bits(addr);
	u32 hi = upper_32_bits(addr);

	dbm_data->ep_num_mapping[dbm_ep] = dep_num;

	msm_dbm_write_reg(dbm_data->base,
				DBM_DATA_FIFO_LSB(dbm_ep), lo);
	msm_dbm_write_reg(dbm_data->base,
				DBM_DATA_FIFO_MSB(dbm_ep), hi);

	msm_dbm_write_reg_field(dbm_data->base, DBM_DATA_FIFO_SIZE(dbm_ep),
		DBM_DATA_FIFO_SIZE_MASK, size);

	return 0;

}

static void set_speed(bool speed)
{
	msm_dbm_write_reg(dbm_data->base, DBM_GEN_CFG, speed);
}

static void enable(void)
{
	msm_dbm_write_reg(dbm_data->base, DBM_DATA_FIFO_ADDR_EN, 0x000000FF);
	msm_dbm_write_reg(dbm_data->base, DBM_DATA_FIFO_SIZE_EN, 0x000000FF);
}

static bool reset_ep_after_lpm(void)
{
	return dbm_data->dbm_reset_ep_after_lpm;
}

static bool l1_lpm_interrupt(void)
{
	return true;
}


static int msm_dbm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct dbm *dbm;
	struct resource *res;
	int ret = 0;

	dbm_data = devm_kzalloc(dev, sizeof(*dbm_data), GFP_KERNEL);
	if (!dbm_data)
		return -ENOMEM;
	dbm_data->dbm_num_eps = DBM_1_5_NUM_EP;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto free_dbm_data;
	}

	dbm_data->base = devm_ioremap_nocache(&pdev->dev, res->start,
		resource_size(res));
	if (!dbm_data->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto free_dbm_data;
	}


	dbm = devm_kzalloc(dev, sizeof(*dbm), GFP_KERNEL);
	if (!dbm) {
		dev_err(&pdev->dev, "not enough memory\n");
		ret = -ENOMEM;
		goto free_dbm_data;
	}

	dbm_data->dbm_reset_ep_after_lpm = of_property_read_bool(node,
			"qcom,reset-ep-after-lpm-resume");

	dbm->dev = dev;

	dbm->soft_reset = soft_reset;
	dbm->ep_config = ep_config;
	dbm->ep_unconfig = ep_unconfig;
	dbm->get_num_of_eps_configured = get_num_of_eps_configured;
	dbm->event_buffer_config = event_buffer_config;
	dbm->data_fifo_config = data_fifo_config;
	dbm->set_speed = set_speed;
	dbm->enable = enable;
	dbm->ep_soft_reset = usb_ep_soft_reset;
	dbm->reset_ep_after_lpm = reset_ep_after_lpm;
	dbm->l1_lpm_interrupt = l1_lpm_interrupt;

	platform_set_drvdata(pdev, dbm);

	return usb_add_dbm(dbm);

free_dbm_data:
	kfree(dbm_data);
	return ret;
}

static int msm_dbm_remove(struct platform_device *pdev)
{
	struct dbm *dbm = platform_get_drvdata(pdev);

	kfree(dbm);
	kfree(dbm_data);

	return 0;
}

static const struct of_device_id msm_dbm_1_5_id_table[] = {
	{
		.compatible = "qcom,usb-dbm-1p5",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_dbm_1_5_id_table);

static struct platform_driver msm_dbm_driver = {
	.probe		= msm_dbm_probe,
	.remove		= msm_dbm_remove,
	.driver = {
		.name	= "msm-usb-dbm-1-5",
		.of_match_table = of_match_ptr(msm_dbm_1_5_id_table),
	},
};

module_platform_driver(msm_dbm_driver);

MODULE_DESCRIPTION("MSM USB DBM 1.5 driver");
MODULE_LICENSE("GPL v2");


