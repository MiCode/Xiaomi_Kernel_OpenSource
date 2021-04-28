/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/slab.h>
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include "dapc.h"
#include "dapc_cfg.h"
#include "apusys_power.h"

struct dapc_driver *dapc_drv;

#ifdef CONFIG_MTK_ENG_BUILD
/* apusys devapc debug file operations */
static int apusys_devapc_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static int apusys_devapc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int trigger_vio_test(struct dapc_driver *drv, int round)
{
	phys_addr_t test_base = drv->cfg->ut_base;
	void __iomem *test_virt;
	int i, num, flag_ke;

	if (round <= 0) {
		pr_info("invalid round %d\n", round);
		return -EINVAL;
	}

	test_virt = ioremap(test_base, 4096);
	if (!test_virt) {
		pr_info("ioremap test base(0x%lx) virt failed\n",
			(unsigned long)test_base);
		return -ENOMEM;
	}

	/* do not trigger KE while testing */
	flag_ke = drv->enable_ke;
	drv->enable_ke = 0;

	for (i = 0; i < round; i++) {
		get_random_bytes(&num, sizeof(num));
		num &= 0xF;
		/* mdla_core0 0x8 */
		iowrite32(num, (unsigned int *)(test_virt + 0x8));
		pr_info("write 0x%x to (0x%lx), read: 0x%x\n",
			num, (unsigned long)(test_base + 0x8),
			readl((unsigned int *)(test_virt + 0x8)));

		get_random_bytes(&num, sizeof(num));
		num &= 0xF;
		/* mdla_core1 0xC */
		iowrite32(num, (unsigned int *)(test_virt + 0xC));
		pr_info("write 0x%x to (0x%lx), read: 0x%x\n",
			num, (unsigned long)(test_base + 0xC),
			readl((unsigned int *)(test_virt + 0xC)));
	}

	iounmap(test_virt);

	drv->enable_ke = flag_ke;

	return 0;
}

static ssize_t apusys_devapc_read(struct file *file, char __user *buf,
				  size_t size, loff_t *offset)
{
	struct dapc_driver *drv = (struct dapc_driver *)file->private_data;
	char output[64] = {0};
	int ret, len;

	if (!buf)
		return -EINVAL;

	ret = snprintf(output, sizeof(output),
		"%s enable_KE: %d, enable_AEE: %d, enable_IRQ: %d\n",
		"[APUSYS_DEVAPC]", drv->enable_ke, drv->enable_aee,
		drv->enable_irq);

	if (ret <= 0)
		return 0;

	len = min((int)size, (int)(strlen(output) - *offset));
	if (len <= 0)
		return 0;

	ret = copy_to_user(buf, output, len);
	if (ret) {
		pr_info("Fail to copy %s, ret %d\n", output, ret);
		return -EFAULT;
	}

	*offset += len;

	return len;
}

static void apusys_devapc_start(void *data);

static ssize_t apusys_devapc_write(struct file *file, const char __user *buf,
				   size_t size, loff_t *ppos)
{
	struct dapc_driver *drv = (struct dapc_driver *)file->private_data;
	char input[32] = {0};
	char *cmd_str, *param_str, *tmp_str;
	unsigned int param = 0;
	size_t len;
	int ret;

	if (!buf)
		return -EINVAL;

	len = min(size, 31UL);

	if (copy_from_user(input, buf, len)) {
		pr_info("Fail to copy from user\n");
		return -EFAULT;
	}

	input[len] = '\0';
	tmp_str = input;

	cmd_str = strsep(&tmp_str, " ");
	if (!cmd_str) {
		pr_info("Fail to get cmd\n");
		return -EINVAL;
	}

	param_str = strsep(&tmp_str, " ");
	if (!param_str) {
		pr_info("Fail to get param\n");
		return -EINVAL;
	}

	ret = kstrtouint(param_str, 10, &param);

	if (ret)
		return ret;

	if (!strncmp(cmd_str, "enable_UT", sizeof("enable_UT"))) {
		if (param & 0x1) {
			drv->enable_ke = 0;
			drv->enable_aee = 0;
			drv->enable_irq = 1;
			pr_info("APUSYS devapc debugging mode\n");
		} else {
			drv->enable_ke = 1;
			drv->enable_aee = 1;
			drv->enable_irq = 1;
			pr_info("APUSYS devapc violation mode\n");
		}
	} else if (!strncmp(cmd_str, "enable_KE", sizeof("enable_KE"))) {
		drv->enable_ke = param & 0x1;
		pr_info("APUSYS devapc %s KE\n",
			drv->enable_ke ? "enable" : "disable");
	} else if (!strncmp(cmd_str, "enable_AEE", sizeof("enable_AEE"))) {
		drv->enable_aee = param & 0x1;
		pr_info("APUSYS devapc %s AEE\n",
			drv->enable_aee ? "enable" : "disable");
	} else if (!strncmp(cmd_str, "enable_IRQ", sizeof("enable_IRQ"))) {
		drv->enable_irq = param & 0x1;
		if (drv->enable_irq)
			enable_irq(drv->irq);
		else
			disable_irq(drv->irq);
		pr_info("APUSYS devapc %s IRQ\n",
			drv->enable_irq ? "enable" : "disable");
	} else if (!strncmp(cmd_str, "restart", sizeof("restart"))) {
		apusys_devapc_start(NULL);
		pr_info("APUSYS devapc restarted\n");
	} else if (!strncmp(cmd_str, "trigger_vio", sizeof("trigger_vio"))) {
		pr_info("APUSYS devapc trigger vio test %d +\n", param);
		trigger_vio_test(drv, param);
		pr_info("APUSYS devapc trigger vio test %d -\n", param);
	} else {
		pr_info("Unknown cmd %s\n", cmd_str);
		return -EINVAL;
	}

	return len;
}

static const struct file_operations apusys_devapc_fops = {
	.owner = THIS_MODULE,
	.open = apusys_devapc_open,
	.read = apusys_devapc_read,
	.write = apusys_devapc_write,
	.release = apusys_devapc_release,
};
/* apusys devapc debug file operations */

static int apusys_devapc_debug_init(struct dapc_driver *drv)
{
	struct dentry *droot = NULL;
	int ret = 0;

	droot = debugfs_create_dir("apusys_devapc", NULL);
	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: unable to create debugfs folder: %d\n",
			__func__, ret);
		return ret;
	}
	debugfs_create_u32("debug",
		0644, droot, &drv->debug_log);
	debugfs_create_file("apusys_devapc", 0644,
		droot, drv, &apusys_devapc_fops);

	drv->droot = droot;
	return ret;
}

static void apusys_devapc_debug_exit(struct dapc_driver *drv)
{
	debugfs_remove_recursive(drv->droot);
	drv->droot = NULL;
}
#else
static int apusys_devapc_debug_init(struct dapc_driver *drv)
{
	return 0;
}

static void apusys_devapc_debug_exit(struct dapc_driver *drv)
{
}
#endif

static void slv_irq(unsigned int slv, bool enable)
{
	uint32_t reg;
	uint32_t mask;
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;

	if (slv > cfg->slv_cnt) {
		pr_info("%s: slv: %d is out of index, max: %d\n",
			slv, cfg->slv_cnt);
		return;
	}

	reg = cfg->vio_mask(slv / cfg->slv_per_dapc);
	mask = 1 << (slv % cfg->slv_per_dapc);

	if (enable)
		dapc_reg_clr(d, reg, mask);
	else
		dapc_reg_set(d, reg, mask);
}

static void do_kernel_exception(struct dapc_driver *drv, unsigned int i,
	const char *slv_name,
	struct dapc_exception *ex)
{
	/* mask irq for slv "i" */
	slv_irq(i, false);

	if (drv->enable_ke) {
		WARN_ON(1);
		goto out;
	}

#if defined(CONFIG_MTK_AEE_FEATURE)
	if (drv->enable_aee) {
		aee_kernel_exception("APUSYS_DEVAPC",
			"Violation Slave: %s (%s%s): transaction ID:0x%x, Addr:0x%x, HighAddr: %x, Domain: 0x%x\n",
			slv_name,
			(ex->read_vio) ? "R" : "",
			(ex->write_vio) ? " W" : "",
			ex->trans_id,
			ex->addr,
			ex->addr_high,
			ex->domain_id);
	}
#endif

out:
	/* unmask irq for slv "i" */
	slv_irq(i, true);
}

static uint32_t check_vio_status(unsigned int slv)
{
	uint32_t reg;
	uint32_t mask;
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;

	if (slv > cfg->slv_cnt) {
		pr_info("%s: slv: %d is out of index, max: %d\n",
			slv, cfg->slv_cnt);
		return -EINVAL;
	}

	reg = cfg->vio_sta(slv / cfg->slv_per_dapc);
	mask = 0x1 << (slv % cfg->slv_per_dapc);

	return dapc_reg_r(d, reg) & mask;
}

static void clear_vio_status(unsigned int slv)
{
	uint32_t reg;
	uint32_t mask;
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;

	if (slv > cfg->slv_cnt) {
		pr_info("%s: slv: %d is out of index, max: %d\n",
			slv, cfg->slv_cnt);
		return;
	}

	reg = cfg->vio_sta(slv / cfg->slv_per_dapc);
	mask = 0x1 << (slv % cfg->slv_per_dapc);

	dapc_reg_w(d, reg, mask);
}

static int shift_vio_dbg(int shift_bit)
{
	int count = 0;
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;
	uint32_t sel = cfg->vio_shift_sel;
	uint32_t con = cfg->vio_shift_con;
	uint32_t mask = cfg->vio_shift_con_mask;

	/* start shift */
	dapc_reg_w(d, sel, (0x1 << shift_bit));
	dapc_reg_w(d, con, 0x1);

	/* wait for shift done */
	while (((dapc_reg_r(d, con) & mask) != mask) && (count < 100))
		count++;

	pr_debug("%s: shift done %d, %d, SHIFT_SEL: 0x%x, SHIFT_CON=0x%x\n",
		__func__, shift_bit, count,
		dapc_reg_r(d, sel), dapc_reg_r(d, con));

	if ((dapc_reg_r(d, con) & mask) != mask) {
		pr_info("%s: shift bit %d failed\n", shift_bit);
		return -EFAULT;
	}

	/* disable shift mechanism */
	dapc_reg_w(d, con, 0);
	dapc_reg_w(d, sel, 0);
	/* SHIFT_STA must be write-cleared before clear VIO_STA */
	dapc_reg_w(d, cfg->vio_shift_sta, (0x1 << shift_bit));

	return 0;
}

static void apusys_devapc_dbg(const char *prefix)
{
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;

	if (!dapc_drv->debug_log)
		return;

	pr_info("%s: %s: VIO_MASK0~3: 0x%x, 0x%x, 0x%x, 0x%x, VIO_STA0~3: 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__,
		prefix,
		dapc_reg_r(d, cfg->vio_mask(0)),
		dapc_reg_r(d, cfg->vio_mask(1)),
		dapc_reg_r(d, cfg->vio_mask(2)),
		dapc_reg_r(d, cfg->vio_mask(3)),
		dapc_reg_r(d, cfg->vio_sta(0)),
		dapc_reg_r(d, cfg->vio_sta(1)),
		dapc_reg_r(d, cfg->vio_sta(2)),
		dapc_reg_r(d, cfg->vio_sta(3)));
}

/* Return 1 means there are some violations */
static inline int is_violation_irq(void)
{
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;

	return (dapc_reg_r(d, cfg->vio_sta(0)) ||
		dapc_reg_r(d, cfg->vio_sta(1)) ||
		dapc_reg_r(d, cfg->vio_sta(2)) ||
		dapc_reg_r(d, cfg->vio_sta(3)));
}

static irqreturn_t apusys_devapc_isr(int irq_number, void *data)
{
	int i;
	struct dapc_driver *d = (struct dapc_driver *)data;
	struct dapc_config *cfg;
	unsigned int shift_max;
	struct dapc_slave *slv;
	struct dapc_exception ex;

	if (!is_violation_irq())
		return IRQ_NONE;

	if (!d || IS_ERR_OR_NULL(d->reg)) {
		pr_info("%s: driver abort\n", __func__);
		return IRQ_NONE;
	}

	cfg = d->cfg;
	slv = cfg->slv;
	shift_max = cfg->vio_shift_max_bit;

	if (irq_number != d->irq) {
		pr_info("%s: get unknown irq %d\n", __func__, irq_number);
		return IRQ_NONE;
	}

	if (!d->irq) {
		pr_info("%s: Disable vio irq handler\n", __func__);
		return IRQ_NONE;
	}

	memset(&ex, 0, sizeof(struct dapc_exception));
	disable_irq_nosync(irq_number);
	apusys_devapc_dbg("ISR begin");

	for (i = 0; i <= shift_max; ++i) {
		if (!(dapc_reg_r(d, cfg->vio_shift_sta) & (0x1 << i)))
			continue;
		if (shift_vio_dbg(i))
			continue;

		pr_info("%s: VIO_SHIFT_STA=0x%x, VIO_SHIFT_SEL=0x%x, VIO_SHIFT_CON=0x%x,\n",
			__func__,
			dapc_reg_r(d, cfg->vio_shift_sta),
			dapc_reg_r(d, cfg->vio_shift_sel),
			dapc_reg_r(d, cfg->vio_shift_con));

		cfg->excp_info(d, &ex);

		/* violation information */
		pr_info("%s: Violation(%s%s): transaction ID:0x%x, Addr:0x%x, HighAddr: %x, Domain: 0x%x\n",
			__func__,
			(ex.read_vio) ? "R" : "",
			(ex.write_vio) ? " W" : "",
			ex.trans_id,
			ex.addr,
			ex.addr_high,
			ex.domain_id);
	}

	/* Clear VIO_STA */
	for_each_dapc_slv(cfg, i) {
		if (!(slv[i].vio_irq_en && check_vio_status(i)))
			continue;

		clear_vio_status(i);
		pr_info("%s: vio_sta device: %d, slave: %s\n",
			__func__, i, slv[i].name);
		do_kernel_exception(d, i, slv[i].name, &ex);
	}

	apusys_devapc_dbg("ISR end");
	enable_irq(irq_number);

	return IRQ_HANDLED;
}

static void apusys_devapc_start(void *data)
{
	int i = 0;
	struct dapc_driver *d = dapc_drv;
	struct dapc_config *cfg = d->cfg;
	struct dapc_slave *slv = cfg->slv;

	/* Clear APC violation status */
	dapc_reg_w(d, cfg->apc_con, cfg->apc_con_vio);
	apusys_devapc_dbg("Init begin");

	/* Enable violation IRQ */
	for_each_dapc_slv(cfg, i) {
		if (!slv[i].vio_irq_en)
			continue;

		clear_vio_status(i);
		slv_irq(i, true);
	}
	apusys_devapc_dbg("Init end");
}

static int apusys_devapc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *devapc_node;
	struct dapc_driver *drv;
	struct dapc_config *cfg;

	dev_info(&pdev->dev, "%s +\n", __func__);

	if (!apusys_power_check())
		return 0;

	devapc_node = pdev->dev.of_node;
	if (!devapc_node) {
		dev_info(&pdev->dev, "of_node required\n");
		return -ENODEV;
	}

	if (dapc_drv) {
		dev_info(&pdev->dev, "duplicated device node\n");
		return -ENODEV;
	}

	cfg = (struct dapc_config *)of_device_get_match_data(&pdev->dev);
	if (!cfg) {
		dev_info(&pdev->dev, "unsupported device: %s\n", pdev->name);
		return -ENODEV;
	}

	drv = kzalloc(sizeof(struct dapc_driver), GFP_KERNEL);
	if (!drv) {
		ret = -ENOMEM;
		goto err_allocate;
	}
	dapc_drv = drv;

	drv->cfg = cfg;
	drv->reg = of_iomap(devapc_node, 0);
	if (!drv->reg) {
		ret = -ENOMEM;
		goto err_iomap;
	}

	drv->irq = irq_of_parse_and_map(devapc_node, 0);

	ret = request_irq(drv->irq, apusys_devapc_isr,
			  irq_get_trigger_type(drv->irq) | IRQF_SHARED,
			  "apusys-devapc", drv);
	if (ret) {
		ret = -EFAULT;
		goto err_request_irq;
	}

	drv->enable_ke = drv->enable_aee = 1;

	if (drv->cfg->irq_enable) {
		drv->enable_irq = 1;
	} else {
		drv->enable_irq = 0;
		disable_irq(drv->irq);
	}

	apusys_devapc_start(NULL);
	ret = apu_power_callback_device_register(DEVAPC,
		apusys_devapc_start, NULL);
	if (ret)
		dev_info(&pdev->dev,
			"unable to register to apu power: %d\n", ret);

	drv->debug_log = 0;
	apusys_devapc_debug_init(drv);
	platform_set_drvdata(pdev, drv);
	dev_info(&pdev->dev, "%s -\n", __func__);

	return 0;

err_request_irq:
	iounmap(drv->reg);
err_iomap:
	kfree(drv);
err_allocate:
	dapc_drv = NULL;
	return ret;
}

static int apusys_devapc_remove(struct platform_device *pdev)
{
	struct dapc_driver *drv = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s +\n", __func__);

	free_irq(drv->irq, drv);
	iounmap(drv->reg);
	apusys_devapc_debug_exit(drv);
	apu_power_callback_device_unregister(DEVAPC);
	kfree(drv);
	dev_dbg(&pdev->dev, "%s -\n", __func__);
	dapc_drv = NULL;
	return 0;
}

static const struct of_device_id apusys_devapc_of_match[] = {
	{ .compatible = "mediatek,mt6885-apusys_devapc", &dapc_cfg_mt6885 },
	{ .compatible = "mediatek,mt6873-apusys_devapc", &dapc_cfg_mt6873 },
	{ .compatible = "mediatek,mt6853-apusys_devapc", &dapc_cfg_mt6853 },
	{ .compatible = "mediatek,mt6893-apusys_devapc", &dapc_cfg_mt6885 },
	{},
};

static struct platform_driver apusys_devapc_driver = {
	.probe = apusys_devapc_probe,
	.remove = apusys_devapc_remove,
	.driver = {
		.name = "apusys-devapc",
		.owner = THIS_MODULE,
		.of_match_table = apusys_devapc_of_match,
	},
};

static int __init apusys_devapc_init(void)
{
	dapc_drv = NULL;
	return platform_driver_register(&apusys_devapc_driver);
}

static void __exit apusys_devapc_exit(void)
{
	platform_driver_unregister(&apusys_devapc_driver);
}

late_initcall(apusys_devapc_init);
module_exit(apusys_devapc_exit);

MODULE_DESCRIPTION("Mediatek APUSYS DEVAPC Driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");
