// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <mt-plat/aee.h>
#include <soc/mediatek/emi.h>

struct emi_slb {
	unsigned int dump_cnt;
	struct reg_info_t *dump_reg;

	unsigned int clear_reg_cnt;
	struct reg_info_t *clear_reg;

	unsigned int emi_slb_cnt;
	void __iomem **emi_mpu_base;
	void __iomem **emi_slb_base;

	/* interrupt id */
	unsigned int irq;

	/* debugging log for EMI MPU violation */
	char *vio_msg;
	unsigned int in_msg_dump;

};

/* global pointer for exported functions */
static struct emi_slb *global_emi_slb;

static void set_regs(
	struct reg_info_t *reg_list, unsigned int reg_cnt,
	void __iomem *emi_mpu_base)
{
	unsigned int i, j;

	for (i = 0; i < reg_cnt; i++)
		for (j = 0; j < reg_list[i].leng; j++)
			writel(reg_list[i].value, emi_mpu_base +
				reg_list[i].offset + 4 * j);

	/*
	 * Use the memory barrier to make sure the interrupt signal is
	 * de-asserted (by programming registers) before exiting the
	 * ISR and re-enabling the interrupt.
	 */
	mb();
}

static void clear_violation(
	struct emi_slb *slb, unsigned int emi_id)
{
	void __iomem *emi_mpu_base;

	emi_mpu_base = slb->emi_mpu_base[emi_id];

	set_regs(slb->clear_reg,
		slb->clear_reg_cnt, emi_mpu_base);
}

static void emislb_vio_dump(struct work_struct *work)
{
	struct emi_slb *slb;

	slb = global_emi_slb;
	if (!slb)
		return;

	if (slb->vio_msg)
		aee_kernel_exception("EMISLB", slb->vio_msg);

	slb->in_msg_dump = 0;
}
static DECLARE_WORK(emislb_work, emislb_vio_dump);

static irqreturn_t emislb_violation_irq(int irq, void *dev_id)
{
	struct emi_slb *slb = (struct emi_slb *)dev_id;
	struct reg_info_t *dump_reg = slb->dump_reg;
	void __iomem *emi_slb_base;
	unsigned int emi_id, i;
	ssize_t msg_len;
	int n, nr_vio;
	bool violation;

	nr_vio = 0;
	msg_len = 0;
	for (emi_id = 0; emi_id < slb->emi_slb_cnt; emi_id++) {
		violation = false;
		emi_slb_base = slb->emi_slb_base[emi_id];

		for (i = 0; i < slb->dump_cnt; i++) {
			dump_reg[i].value = readl(
				emi_slb_base + dump_reg[i].offset);
			if (dump_reg[i].value)
				violation = true;
		}

		if (!violation)
			continue;

		nr_vio++;
		for (i = 0; i < slb->dump_cnt; i++)
			if (msg_len < MTK_EMI_MAX_CMD_LEN) {
				n = snprintf(slb->vio_msg + msg_len,
					MTK_EMI_MAX_CMD_LEN - msg_len,
					"%s(%d),%s(%x),%s(%x);\n",
					"emi", emi_id,
					"off", dump_reg[i].offset,
					"val", dump_reg[i].value);
				msg_len += (n < 0) ? 0 : (ssize_t)n;
			}

		clear_violation(slb, emi_id);
	}

	if (nr_vio && !slb->in_msg_dump && msg_len) {
		pr_info("%s: %s", __func__, slb->vio_msg);
		slb->in_msg_dump = 1;
		schedule_work(&emislb_work);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id emislb_of_ids[] = {
	{.compatible = "mediatek,common-emislb",},
	{}
};
MODULE_DEVICE_TABLE(of, emislb_of_ids);

static int emislb_probe(struct platform_device *pdev)
{
	struct device_node *emislb_node = pdev->dev.of_node;
	struct device_node *emimpu_node =
		of_parse_phandle(emislb_node, "mediatek,emimpu-reg", 0);
	struct emi_slb *slb;
	struct resource *res;
	int ret, size, i;
	unsigned int *dump_list;

	dev_info(&pdev->dev, "driver probed\n");

	if (!emimpu_node) {
		dev_err(&pdev->dev, "No emi-reg\n");
		return -ENXIO;
	}

	slb = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_slb), GFP_KERNEL);
	if (!slb)
		return -ENOMEM;

	ret = of_property_count_elems_of_size(emislb_node,
		"reg", sizeof(unsigned int) * 4);
	if (ret <= 0) {
		dev_err(&pdev->dev, "No reg\n");
		return -ENXIO;
	}
	slb->emi_slb_cnt = (unsigned int)ret;

	slb->emi_slb_base = devm_kmalloc_array(&pdev->dev,
		slb->emi_slb_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!slb->emi_slb_base)
		return -ENOMEM;

	slb->emi_mpu_base = devm_kmalloc_array(&pdev->dev,
		slb->emi_slb_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(slb->emi_mpu_base))
		return -ENOMEM;

	for (i = 0; i < slb->emi_slb_cnt; i++) {
		slb->emi_mpu_base[i] = of_iomap(emimpu_node, i);
		if (IS_ERR(slb->emi_mpu_base[i])) {
			dev_err(&pdev->dev, "Failed to map EMI%d CEN base\n",
				i);
			return -EIO;
		}
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		slb->emi_slb_base[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(slb->emi_slb_base[i])) {
			dev_err(&pdev->dev, "Failed to map EMI%d SLB base\n",
				i);
			return -EIO;
		}
	}
//dump
	size = of_property_count_elems_of_size(emislb_node,
		"dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No dump\n");
		return -ENXIO;
	}
	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENOMEM;
	size >>= 2;
	slb->dump_cnt = size;
	ret = of_property_read_u32_array(emislb_node, "dump",
		dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "No dump\n");
		return -ENXIO;
	}
	slb->dump_reg = devm_kmalloc(&pdev->dev,
		size * sizeof(struct reg_info_t), GFP_KERNEL);
	if (!(slb->dump_reg))
		return -ENOMEM;
	for (i = 0; i < slb->dump_cnt; i++) {
		slb->dump_reg[i].offset = dump_list[i];
		slb->dump_reg[i].value = 0;
		slb->dump_reg[i].leng = 0;
	}
//dump end
//clear
	size = of_property_count_elems_of_size(emislb_node,
		"clear", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear\n");
		return  -ENXIO;
	}
	slb->clear_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(slb->clear_reg))
		return -ENOMEM;
	slb->clear_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emislb_node, "clear",
		(unsigned int *)(slb->clear_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear\n");
		return -ENXIO;
	}

//clear end
	slb->vio_msg = devm_kmalloc(&pdev->dev,
		MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!(slb->vio_msg))
		return -ENOMEM;

	global_emi_slb = slb;
	platform_set_drvdata(pdev, slb);

//irq
	slb->irq = irq_of_parse_and_map(emislb_node, 0);
	if (slb->irq == 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	ret = request_irq(slb->irq, (irq_handler_t)emislb_violation_irq,
		IRQF_TRIGGER_NONE, "emislb", slb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq");
		return -EINVAL;
	}
//irq end
	devm_kfree(&pdev->dev, dump_list);

	return 0;
}

static int emislb_remove(struct platform_device *pdev)
{
	struct emi_slb *slb = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "driver removed\n");

	free_irq(slb->irq, slb);

	flush_work(&emislb_work);

	global_emi_slb = NULL;

	return 0;
}

static struct platform_driver emislb_driver = {
	.probe = emislb_probe,
	.remove = emislb_remove,
	.driver = {
		.name = "emislb_driver",
		.owner = THIS_MODULE,
		.of_match_table = emislb_of_ids,
	},
};

static __init int emislb_init(void)
{
	int ret;

	pr_info("emislb was loaded\n");

	ret = platform_driver_register(&emislb_driver);
	if (ret) {
		pr_err("emislb: failed to register driver\n");
		return ret;
	}

	return 0;
}

module_init(emislb_init);

MODULE_DESCRIPTION("MediaTek EMI SLB MPU Driver");
MODULE_LICENSE("GPL v2");

