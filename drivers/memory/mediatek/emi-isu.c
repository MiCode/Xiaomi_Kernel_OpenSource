// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <soc/mediatek/emi.h>

struct emi_isu {
	unsigned int buf_size;
	void __iomem *buf_addr;
	void __iomem *ver_addr;
	void __iomem *con_addr;
	unsigned int ctrl_intf;
};

/* global pointer for sysfs operations*/
static struct emi_isu *global_emi_isu;

static ssize_t emiisu_ctrl_show(struct device_driver *driver, char *buf)
{
	struct emi_isu *isu;
	unsigned int state;

	if (!global_emi_isu)
		return 0;

	isu = global_emi_isu;

	if (!(isu->con_addr))
		return 0;

	state = readl(isu->con_addr);

	return snprintf(buf, PAGE_SIZE, "isu_state: 0x%x\n", state);
}

static ssize_t emiisu_ctrl_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	struct emi_isu *isu;
	unsigned long state;
	char *command;
	char *backup_command;
	char *ptr;
	char *token[MTK_EMI_MAX_TOKEN];
	int i;

	if (!global_emi_isu)
		return count;

	isu = global_emi_isu;

	if (!(isu->ctrl_intf))
		return count;

	if (!(isu->con_addr))
		return count;

	if ((strlen(buf) + 1) > MTK_EMI_MAX_CMD_LEN) {
		pr_info("%s: store command overflow\n", __func__);
		return count;
	}

	command = kmalloc((size_t)MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;
	backup_command = command;
	strncpy(command, buf, (size_t)MTK_EMI_MAX_CMD_LEN);

	for (i = 0; i < MTK_EMI_MAX_TOKEN; i++) {
		ptr = strsep(&command, " ");
		if (!ptr)
			break;
		token[i] = ptr;
	}

	if (i < 1)
		goto emiisu_ctrl_store_end;

	if (kstrtoul(token[0], 16, &state) == 0)
		writel((unsigned int)state, isu->con_addr);

emiisu_ctrl_store_end:
	kfree(backup_command);

	return count;
}

static DRIVER_ATTR_RW(emiisu_ctrl);

static ssize_t read_emi_isu_buf(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buff, loff_t pos, size_t count)
{
	struct emi_isu *isu;
	ssize_t bytes = 0, ret;

	if (!global_emi_isu)
		return 0;

	isu = global_emi_isu;

	if (!(isu->buf_addr))
		return 0;
	if (!(isu->ver_addr))
		return 0;

	/*
	 * The EMI ISU buffer consists of a 4-byte header (which is stored in
	 * isu->ver_addr) and the content (which is stored in isu->buf_addr).
	 * Concatenate isu->ver_addr and isu->buf_addr and return to the reader.
	 */

	if (pos == 0) {
		if (count < 4)
			return -EINVAL;
		memcpy(buff, isu->ver_addr, 4);
		bytes += 4;
		buff += 4;
		count -= 4;
		pos += 4;
	}

	pos -= 4;
	ret = memory_read_from_buffer(buff, count, &pos,
				isu->buf_addr, isu->buf_size);
	if (ret < 0)
		return ret;
	else
		return bytes + ret;
}

static struct bin_attribute emi_isu_buf_attr = {
	.attr = {.name = "emi_isu_buf", .mode = 0444},
	.read = read_emi_isu_buf,
};

static const struct of_device_id emiisu_of_ids[] = {
	{.compatible = "mediatek,common-emiisu",},
	{}
};

static int emiisu_probe(struct platform_device *pdev)
{
	struct device_node *emiisu_node = pdev->dev.of_node;
	struct emi_isu *isu;
	unsigned long long addr_temp;
	int ret;

	dev_info(&pdev->dev, "driver probed\n");

	isu = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_isu), GFP_KERNEL);
	if (!isu)
		return -ENOMEM;

	ret = of_property_read_u32(emiisu_node,
		"buf_size", &(isu->buf_size));
	if (ret) {
		dev_err(&pdev->dev, "No buf_size\n");
		return -EINVAL;
	}

	ret = of_property_read_u64(emiisu_node, "buf_addr", &addr_temp);
	if (ret) {
		dev_err(&pdev->dev, "No buf_addr\n");
		return -EINVAL;
	}
	if (addr_temp)
		isu->buf_addr = ioremap_wc(
			(phys_addr_t)addr_temp, isu->buf_size);
	else
		isu->buf_addr = NULL;

	ret = of_property_read_u64(emiisu_node, "ver_addr", &addr_temp);
	if (ret) {
		dev_err(&pdev->dev, "No ver_addr\n");
		return -EINVAL;
	}
	if (addr_temp)
		isu->ver_addr = ioremap((phys_addr_t)addr_temp, 4);
	else
		isu->ver_addr = NULL;

	ret = of_property_read_u64(emiisu_node, "con_addr", &addr_temp);
	if (ret) {
		dev_err(&pdev->dev, "No con_addr\n");
		return -EINVAL;
	}
	if (addr_temp)
		isu->con_addr = ioremap((phys_addr_t)addr_temp, 4);
	else
		isu->con_addr = NULL;

	ret = of_property_read_u32(emiisu_node,
		"ctrl_intf", &(isu->ctrl_intf));
	if (ret) {
		dev_err(&pdev->dev, "No ctrl_intf\n");
		return -EINVAL;
	}

	/* max buffer size = ISU buffer size + 4 bytes of version header */
	emi_isu_buf_attr.size = isu->buf_size + 4;

	ret = sysfs_create_bin_file(&pdev->dev.kobj, &emi_isu_buf_attr);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create buf file\n");
		return ret;
	}

	platform_set_drvdata(pdev, isu);
	global_emi_isu = isu;

	dev_info(&pdev->dev, "%s(%x),%s(%lx),%s(%lx),%s(%lx),%s(%d)\n",
		"buf_size", isu->buf_size,
		"buf_addr", (unsigned long)(isu->buf_addr),
		"ver_addr", (unsigned long)(isu->ver_addr),
		"con_addr", (unsigned long)(isu->con_addr),
		"ctrl_intf", isu->ctrl_intf);

	return 0;
}

static int emiisu_remove(struct platform_device *pdev)
{
	sysfs_remove_bin_file(&pdev->dev.kobj, &emi_isu_buf_attr);

	global_emi_isu = NULL;

	return 0;
}

static struct platform_driver emiisu_drv = {
	.probe = emiisu_probe,
	.remove = emiisu_remove,
	.driver = {
		.name = "emiisu_drv",
		.owner = THIS_MODULE,
		.of_match_table = emiisu_of_ids,
	},
};

int emiisu_init(void)
{
	int ret;

	pr_info("emiisu wad loaded\n");

	ret = platform_driver_register(&emiisu_drv);
	if (ret) {
		pr_err("emiisu: failed to register dirver\n");
		return ret;
	}

	ret = driver_create_file(&emiisu_drv.driver,
		&driver_attr_emiisu_ctrl);
	if (ret) {
		pr_err("emiisu: failed to create control file\n");
		return ret;
	}

	return 0;
}

MODULE_DESCRIPTION("MediaTek EMI Driver");
MODULE_LICENSE("GPL v2");
