/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>

#define MODULE_NAME "LLCC AMON deadlock detector"
static bool qcom_llcc_amon_panic = IS_ENABLED(
			CONFIG_QCOM_LLCC_AMON_PANIC) ? true:false;
module_param(qcom_llcc_amon_panic, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(qcom_llcc_amon_panic,
		"Enables deadlock detection by AMON");

static int amon_interrupt_mode;
module_param(amon_interrupt_mode, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(amon_interrupt_mode,
		"Controls whether to use interrupt or poll mode");

enum channel_type {
	AMON_READ,
	AMON_WRITE,
	AMON_EVICT,
};

#define LLCC_AMON_FG_CNT_MASK			0x70
#define LLCC_AMON_CG_CNT_MASK			0x380
#define LLCC_AMON_FG_CNT_SHIFT			0x4
#define LLCC_AMON_CG_CNT_SHIFT			0x7
#define LLCC_COMMON_IRQ_AMON			0x8
#define LLCC_AMON_STATUS0_MASK			0x3ff
#define LLCC_AMON_CHAN_SHIFT			0x4
#define LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(x) (0x1 << x)
#define LLCC_AMON_CFG0_DLDM_SHIFT		0x2

/* AMON register offsets */
#define LLCC_AMON_START			0x3d000
#define LLCC_AMON_CFG0			0x3d000
#define LLCC_AMON_EVICT_REGS		0x3d020
#define LLCC_AMON_WRITE_REGS		0x3d060
#define LLCC_AMON_READ_REGS		0x3d100
#define LLCC_AMON_IRQ_STATUS		0x3d200
#define LLCC_AMON_IRQ_CLEAR		0x3d208
#define LLCC_AMON_IRQ_ENABLE		0x3d20c
#define LLCC_AMON_STATUS_0		0x3d210

/* AMON configuration register bits */
#define LLCC_AMON_CFG0_ENABLE	0x0 /* Enable AMON */
#define LLCC_AMON_CFG0_DLDM	0x2 /* AMON deadlock detection mode */
#define LLCC_AMON_CFG0_SCRM	0x3 /* Enable SCID based deadlock detection */

/* Number of entries maintained by AMON */
#define NR_READ_ENTRIES		40
#define NR_WRITE_ENTRIES	16
#define NR_EVICT_ENTRIES	8
#define AMON_IRQ_BIT		0x0
#define CHAN_OFF(x)		(x << 2)

/**
 * llcc_amon_data: Data structure containing AMON driver data
 *
 * @llcc_amon_regmap: map of AMON register block
 * @dev: LLCC activiy monitor device
 * @amon_irq: LLCC activity monitor irq number
 * @amon_fg_cnt: LLCC activity monitor fine grained counter overflow bit
 * @amon_work: LLCC activity monitor work item to poll and
 * dump AMON CSRs
 * @amon_wq: LLCC activity monitor workqueue to poll and
 * dump AMON CSRs
 * @amon_lock: lock to access and change AMON driver data
 *
 **/
struct llcc_amon_data {
	struct regmap *llcc_amon_regmap;
	struct device *dev;
	u32 amon_irq;
	u32 amon_fg_cnt;
	struct work_struct amon_work;
	struct workqueue_struct *amon_wq;
	struct mutex amon_lock;
};

static void amon_dump_channel_status(struct device *dev, u32 chnum, u32 type)
{
	u32 llcc_amon_csr;
	struct llcc_amon_data *amon_data = dev_get_drvdata(dev);

	switch (type) {
	case AMON_READ:
		regmap_read(amon_data->llcc_amon_regmap,
			(LLCC_AMON_READ_REGS + CHAN_OFF(chnum)),
			&llcc_amon_csr);
		dev_err(dev, "READ entry %u : %8x\n",
					chnum, llcc_amon_csr);
		break;
	case AMON_WRITE:
		regmap_read(amon_data->llcc_amon_regmap,
			(LLCC_AMON_WRITE_REGS + CHAN_OFF(chnum)),
			&llcc_amon_csr);
		dev_err(dev, "WRITE entry %u : %8x\n",
					chnum, llcc_amon_csr);
		break;
	case AMON_EVICT:
		regmap_read(amon_data->llcc_amon_regmap,
			(LLCC_AMON_EVICT_REGS + CHAN_OFF(chnum)),
			&llcc_amon_csr);
		dev_err(dev, "EVICT entry %u : %8x\n",
					chnum, llcc_amon_csr);
		break;
	}
}

static void amon_dump_read_channel_status(struct device *dev, u32 chnum)
{
	amon_dump_channel_status(dev, chnum, AMON_READ);
}

static void amon_dump_write_channel_status(struct device *dev, u32 chnum)
{
	amon_dump_channel_status(dev, chnum, AMON_WRITE);
}

static void amon_dump_evict_channel_status(struct device *dev, u32 chnum)
{
	amon_dump_channel_status(dev, chnum, AMON_EVICT);
}

static ssize_t amon_fg_count_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t n)
{
	struct llcc_amon_data *amon_data;
	int ret;
	u32 count;

	amon_data = dev_get_drvdata(dev);
	if (!amon_data)
		return -ENODEV;

	mutex_lock(&amon_data->amon_lock);
	ret = kstrtouint(buf, 0, &count);
	if (ret) {
		dev_err(amon_data->dev, "invalid user input\n");
		mutex_unlock(&amon_data->amon_lock);
		return ret;
	}

	/* Set fine grained counter */
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		LLCC_AMON_FG_CNT_MASK, (count << LLCC_AMON_FG_CNT_SHIFT));
	mutex_unlock(&amon_data->amon_lock);
	return n;
}

static ssize_t amon_fg_count_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct llcc_amon_data *amon_data;
	int ret;
	u32 count, llcc_amon_cfg0;

	amon_data = dev_get_drvdata(dev);
	if (!amon_data)
		return -ENODEV;

	mutex_lock(&amon_data->amon_lock);
	/* Get fine grained counter */
	regmap_read(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		&llcc_amon_cfg0);
	count = (llcc_amon_cfg0 & LLCC_AMON_FG_CNT_MASK)
					>> LLCC_AMON_FG_CNT_SHIFT;
	ret = snprintf(buf, PAGE_SIZE, "%u\n", count);
	mutex_unlock(&amon_data->amon_lock);
	return ret;
}

static ssize_t amon_deadlock_mode_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t n)
{
	struct llcc_amon_data *amon_data;
	int ret;
	u32 mode;

	amon_data = dev_get_drvdata(dev);
	if (!amon_data)
		return -ENODEV;

	mutex_lock(&amon_data->amon_lock);
	ret = kstrtouint(buf, 0, &mode);
	if (ret) {
		dev_err(amon_data->dev, "invalid user input\n");
		mutex_unlock(&amon_data->amon_lock);
		return ret;
	}

	/* Set deadlock detection mode */
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		BIT(LLCC_AMON_CFG0_DLDM), mode);
	mutex_unlock(&amon_data->amon_lock);
	return n;
}

static ssize_t amon_deadlock_mode_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct llcc_amon_data *amon_data;
	int ret;
	u32 val, llcc_amon_cfg0;

	amon_data = dev_get_drvdata(dev);
	if (!amon_data)
		return -ENODEV;

	mutex_lock(&amon_data->amon_lock);

	/* Get deadlock detection mode */
	regmap_read(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		&llcc_amon_cfg0);
	val = llcc_amon_cfg0 & BIT(LLCC_AMON_CFG0_DLDM);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			val >> LLCC_AMON_CFG0_DLDM_SHIFT);

	mutex_unlock(&amon_data->amon_lock);
	return ret;
}


static DEVICE_ATTR(amon_fg_count, S_IRUGO | S_IWUSR,
				amon_fg_count_show, amon_fg_count_store);
static DEVICE_ATTR(amon_deadlock_mode, S_IRUGO | S_IWUSR,
			amon_deadlock_mode_show, amon_deadlock_mode_store);

static const struct device_attribute *llcc_amon_attrs[] = {
	&dev_attr_amon_fg_count,
	&dev_attr_amon_deadlock_mode,
	NULL,
};

static int amon_create_sysfs_files(struct device *dev,
			const struct device_attribute **attrs)
{
	int ret = 0, i;

	for (i = 0; attrs[i] != NULL; i++) {
		ret = device_create_file(dev, attrs[i]);
		if (ret) {
			dev_err(dev, "AMON: Couldn't create sysfs entry: %s!\n",
				attrs[i]->attr.name);
			break;
		}
	}
	return ret;
}

static int amon_remove_sysfs_files(struct device *dev,
			const struct device_attribute **attrs)
{
	int ret = 0, i;

	for (i = 0; attrs[i] != NULL; i++)
		device_remove_file(dev, attrs[i]);

	return ret;
}

static void enable_qcom_amon_interrupt(struct llcc_amon_data *amon_data)
{

	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_IRQ_ENABLE,
		BIT(AMON_IRQ_BIT), BIT(AMON_IRQ_BIT));
}

void disable_qcom_amon_interrupt(struct llcc_amon_data *amon_data)
{
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_IRQ_ENABLE,
		AMON_IRQ_BIT, AMON_IRQ_BIT);
}

static void clear_qcom_amon_interrupt(struct llcc_amon_data *amon_data)
{
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_IRQ_CLEAR,
		BIT(AMON_IRQ_BIT), BIT(AMON_IRQ_BIT));
}

static void amon_poll_work(struct work_struct *work)
{
	u32 llcc_amon_status0, llcc_amon_irq_status, chnum;
	struct llcc_amon_data *amon_data = container_of(work,
				struct llcc_amon_data, amon_work);

	/* Check for deadlock */
	regmap_read(amon_data->llcc_amon_regmap, LLCC_AMON_IRQ_STATUS,
		&llcc_amon_irq_status);
	if (!(llcc_amon_irq_status & BIT(AMON_IRQ_BIT)))
		/* No deadlock interrupt */
		return;

	regmap_read(amon_data->llcc_amon_regmap, LLCC_AMON_STATUS_0,
		&llcc_amon_status0);
	if (!llcc_amon_status0)
		return;

	chnum = (llcc_amon_status0 & LLCC_AMON_STATUS0_MASK) >>
				 LLCC_AMON_CHAN_SHIFT;
	if (llcc_amon_status0 &
		LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(AMON_READ)) {
		/* Read channel error */
		amon_dump_read_channel_status(amon_data->dev, chnum);
	} else if (llcc_amon_status0 &
		LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(AMON_WRITE)) {
		/* Write channel error */
		amon_dump_write_channel_status(amon_data->dev, chnum);
	} else if (llcc_amon_status0 &
		LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(AMON_EVICT)) {
		/* Evict channel error */
		amon_dump_evict_channel_status(amon_data->dev, chnum);
	}

	clear_qcom_amon_interrupt(amon_data);

	if (qcom_llcc_amon_panic)
		panic("AMON deadlock detected");
}

static irqreturn_t llcc_amon_irq_handler(int irq, void *dev_data)
{
	u32 llcc_amon_status0, llcc_amon_irq_status;
	int chnum;
	struct llcc_amon_data *amon_data = dev_data;

	regmap_read(amon_data->llcc_amon_regmap, LLCC_AMON_IRQ_STATUS,
		&llcc_amon_irq_status);
	if (!(llcc_amon_irq_status & BIT(AMON_IRQ_BIT)))
		/* No deadlock interrupt */
		return IRQ_NONE;

	regmap_read(amon_data->llcc_amon_regmap, LLCC_AMON_STATUS_0,
		&llcc_amon_status0);
	if (unlikely(llcc_amon_status0 == 0))
		return IRQ_NONE;

	/*
	 * Check type of interrupt and channel number.
	 * Call corresponding handler with channel number.
	 */

	chnum = (llcc_amon_status0 & LLCC_AMON_STATUS0_MASK) >>
			 LLCC_AMON_CHAN_SHIFT;
	if (llcc_amon_status0 &
		LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(AMON_READ)) {
		/* Read channel error */
		amon_dump_read_channel_status(amon_data->dev, chnum);
	} else if (llcc_amon_status0 &
		LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(AMON_WRITE)) {
		/* Write channel error */
		amon_dump_write_channel_status(amon_data->dev, chnum);
	} else if (llcc_amon_status0 &
		LLCC_AMON_CNTR_SATURATED_SOURCE_TYPE(AMON_EVICT)) {
		/* Evict channel error */
		amon_dump_evict_channel_status(amon_data->dev, chnum);
	}

	clear_qcom_amon_interrupt(amon_data);

	if (qcom_llcc_amon_panic)
		panic("AMON deadlock detected");

	return IRQ_HANDLED;
}


static int qcom_llcc_amon_dt_to_pdata(struct platform_device *pdev,
					struct llcc_amon_data *pdata)
{
	struct device_node *node = pdev->dev.of_node;

	pdata->dev = &pdev->dev;

	pdata->llcc_amon_regmap = syscon_node_to_regmap(
				pdata->dev->parent->of_node);
	if (IS_ERR(pdata->llcc_amon_regmap)) {
		dev_err(pdata->dev, "No regmap for syscon amon parent\n");
		return -ENOMEM;
	}

	pdata->amon_irq = platform_get_irq(pdev, 0);
	if (!pdata->amon_irq)
		return -ENODEV;

	of_property_read_u32(node, "qcom,fg-cnt",
				&pdata->amon_fg_cnt);
	return 0;
}

static int qcom_llcc_amon_probe(struct platform_device *pdev)
{
	int ret;
	struct llcc_amon_data *amon_data;
	u32 cnt;

	if (!pdev->dev.of_node)
		return -ENODEV;

	amon_data = devm_kzalloc(&pdev->dev, sizeof(struct llcc_amon_data),
				GFP_KERNEL);
	if (!amon_data)
		return -ENOMEM;

	ret = qcom_llcc_amon_dt_to_pdata(pdev, amon_data);
	if (ret) {
		dev_err(amon_data->dev, "failed to get amon data\n");
		return ret;
	}

	amon_data->dev = &pdev->dev;


	platform_set_drvdata(pdev, amon_data);

	/* Enable Activity Monitor */
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		BIT(LLCC_AMON_CFG0_ENABLE), 0x1);

	/* Enable Activity Monitor (AMON) as deadlock detector */
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		BIT(LLCC_AMON_CFG0_DLDM), BIT(LLCC_AMON_CFG0_DLDM));

	/* Set fine grained counter */
	cnt = amon_data->amon_fg_cnt << LLCC_AMON_FG_CNT_SHIFT;
	if (cnt)
		regmap_update_bits(amon_data->llcc_amon_regmap,
			LLCC_AMON_CFG0,	LLCC_AMON_FG_CNT_MASK, cnt);

	mutex_init(&amon_data->amon_lock);
	ret = amon_create_sysfs_files(&pdev->dev, llcc_amon_attrs);
	if (ret) {
		dev_err(amon_data->dev,
			"failed to create sysfs entries\n");
		platform_set_drvdata(pdev, NULL);
		return ret;
	}

	if (amon_interrupt_mode) { /* Interrupt mode */
		ret = devm_request_irq(amon_data->dev, amon_data->amon_irq,
				llcc_amon_irq_handler, IRQF_TRIGGER_RISING,
				"amon_deadlock", amon_data);
		if (ret) {
			dev_err(amon_data->dev,
				"failed to request amon deadlock irq\n");
			platform_set_drvdata(pdev, NULL);
			return ret;
		}
		enable_qcom_amon_interrupt(amon_data);
	} else { /* Polling mode */
		amon_data->amon_wq = create_singlethread_workqueue(
						"amon_deadlock_detector");
		if (!amon_data->amon_wq) {
			dev_err(amon_data->dev,
				"failed to create polling work queue\n");
			platform_set_drvdata(pdev, NULL);
			return -ENOMEM;
		}
		INIT_WORK(&amon_data->amon_work, amon_poll_work);
		queue_work(amon_data->amon_wq, &amon_data->amon_work);
	}

	return 0;
}

static int qcom_llcc_amon_remove(struct platform_device *pdev)
{
	struct llcc_amon_data *amon_data = platform_get_drvdata(pdev);

	disable_qcom_amon_interrupt(amon_data);
	clear_qcom_amon_interrupt(amon_data);
	/* Disable Activity Monitor (AMON) as deadlock detector */
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		BIT(LLCC_AMON_CFG0_DLDM), 0x0);
	/* Disable Activity Monitor */
	regmap_update_bits(amon_data->llcc_amon_regmap, LLCC_AMON_CFG0,
		BIT(LLCC_AMON_CFG0_ENABLE), 0x0);

	amon_remove_sysfs_files(&pdev->dev, llcc_amon_attrs);
	destroy_workqueue(amon_data->amon_wq);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id qcom_llcc_amon_match_table[] = {
	{ .compatible = "qcom,llcc-amon" },
	{},
};

static struct platform_driver qcom_llcc_amon_driver = {
	.probe = qcom_llcc_amon_probe,
	.remove = qcom_llcc_amon_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = qcom_llcc_amon_match_table,
	},
};

static int __init init_qcom_llcc_amon(void)
{
	return platform_driver_register(&qcom_llcc_amon_driver);
}
module_init(init_qcom_llcc_amon);

static void __exit exit_qcom_llcc_amon(void)
{
	return platform_driver_unregister(&qcom_llcc_amon_driver);
}
module_exit(exit_qcom_llcc_amon);

MODULE_DESCRIPTION("QTI LLCC Activity Monitor Driver");
MODULE_LICENSE("GPL v2");
