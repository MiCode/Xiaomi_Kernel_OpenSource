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

#define pr_fmt(fmt) "[APUSYS DEVAPC] " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <mt-plat/sync_write.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/slab.h>
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include "apusys_devapc.h"
#include "apusys_power.h"

static void __iomem *devapc_virt;
struct dentry *apusys_dbg_devapc;
bool print_debug_log;

struct devapc_ctx {
	void __iomem *virt;
	int irq;
	int enable_ke;
	int enable_aee;
	int enable_irq;
	struct dentry *dbg_file;
};

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

static int trigger_vio_test(struct devapc_ctx *dctx, int round)
{
	phys_addr_t test_base = 0x19021000; /* reviser sysctrl */
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
	flag_ke = dctx->enable_ke;
	dctx->enable_ke = 0;

	for (i = 0; i < round; i++) {
		get_random_bytes(&num, sizeof(num));
		num &= 0xF;
		/* mdla_core0 0x8 */
		mt_reg_sync_writel(num, (unsigned int *)(test_virt + 0x8));
		pr_info("write 0x%x to (0x%lx), read: 0x%x\n",
			num, (unsigned long)(test_base + 0x8),
			readl((unsigned int *)(test_virt + 0x8)));

		get_random_bytes(&num, sizeof(num));
		num &= 0xF;
		/* mdla_core1 0xC */
		mt_reg_sync_writel(num, (unsigned int *)(test_virt + 0xC));
		pr_info("write 0x%x to (0x%lx), read: 0x%x\n",
			num, (unsigned long)(test_base + 0xC),
			readl((unsigned int *)(test_virt + 0xC)));
	}

	iounmap(test_virt);

	dctx->enable_ke = flag_ke;

	return 0;
}

static ssize_t apusys_devapc_read(struct file *file, char __user *buf,
				  size_t size, loff_t *offset)
{
	struct devapc_ctx *dctx = (struct devapc_ctx *)file->private_data;
	char output[64] = {0};
	int ret, len;

	if (!buf)
		return -EINVAL;

	ret = snprintf(output, sizeof(output),
		"%s enable_KE: %d, enable_AEE: %d, enable_IRQ: %d\n",
		"[APUSYS_DEVAPC]", dctx->enable_ke, dctx->enable_aee,
		dctx->enable_irq);

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

static void start_apusys_devapc(void *data);

static ssize_t apusys_devapc_write(struct file *file, const char __user *buf,
				   size_t size, loff_t *ppos)
{
	struct devapc_ctx *dctx = (struct devapc_ctx *)file->private_data;
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
			dctx->enable_ke = 0;
			dctx->enable_aee = 0;
			dctx->enable_irq = 1;
			pr_info("APUSYS devapc debugging mode\n");
		} else {
			dctx->enable_ke = 1;
			dctx->enable_aee = 1;
			dctx->enable_irq = 1;
			pr_info("APUSYS devapc violation mode\n");
		}
	} else if (!strncmp(cmd_str, "enable_KE", sizeof("enable_KE"))) {
		dctx->enable_ke = param & 0x1;
		pr_info("APUSYS devapc %s KE\n",
			dctx->enable_ke ? "enable" : "disable");
	} else if (!strncmp(cmd_str, "enable_AEE", sizeof("enable_AEE"))) {
		dctx->enable_aee = param & 0x1;
		pr_info("APUSYS devapc %s AEE\n",
			dctx->enable_aee ? "enable" : "disable");
	} else if (!strncmp(cmd_str, "enable_IRQ", sizeof("enable_IRQ"))) {
		dctx->enable_irq = param & 0x1;
		pr_info("APUSYS devapc %s IRQ\n",
			dctx->enable_irq ? "enable" : "disable");
	} else if (!strncmp(cmd_str, "restart", sizeof("restart"))) {
		start_apusys_devapc(NULL);
		pr_info("APUSYS devapc restarted\n");
	} else if (!strncmp(cmd_str, "trigger_vio", sizeof("trigger_vio"))) {
		pr_info("APUSYS devapc trigger vio test %d +\n", param);
		trigger_vio_test(dctx, param);
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

static void unmask_module_irq(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit = 0;
	uint32_t tmp = 0;

	if (module > ARRAY_SIZE(apusys_modules)) {
		pr_info("module:%d overflow!\n", module);
		return;
	}

	apc_index = module / (MOD_NUM_1_DAPC);
	apc_bit = module % (MOD_NUM_1_DAPC);

	tmp = readl(DEVAPC_VIO_MASK(apc_index));
	tmp &= (0xFFFFFFFF ^ (1 << apc_bit));
	mt_reg_sync_writel(tmp, DEVAPC_VIO_MASK(apc_index));
}

static void mask_module_irq(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit = 0;
	uint32_t tmp = 0;

	if (module > ARRAY_SIZE(apusys_modules)) {
		pr_info("module:%d overflow!\n", module);
		return;
	}

	apc_index = module / (32);
	apc_bit = module % (32);

	tmp = readl(DEVAPC_VIO_MASK(apc_index));
	tmp |= (1 << apc_bit);
	mt_reg_sync_writel(tmp, DEVAPC_VIO_MASK(apc_index));
}

static void do_kernel_exception(struct devapc_ctx *dctx, unsigned int i,
			unsigned int dbg0, unsigned int dbg1, unsigned int dbg2)
{
	unsigned int domain_id, write_vio, read_vio, vio_addr_high;

	pr_info("Executing Exception...\n");

	/* mask irq for module "i" */
	mask_module_irq(i);

	domain_id = (dbg0 & DEVAPC_VIO_DBG_DMNID)
		>> DEVAPC_VIO_DBG_DMNID_START_BIT;
	write_vio = (dbg0 & DEVAPC_VIO_DBG_W_VIO)
		>> DEVAPC_VIO_DBG_W_VIO_START_BIT;
	read_vio = (dbg0 & DEVAPC_VIO_DBG_R_VIO)
		>> DEVAPC_VIO_DBG_R_VIO_START_BIT;
	vio_addr_high = (dbg0 & DEVAPC_VIO_ADDR_HIGH)
		>> DEVAPC_VIO_ADDR_HIGH_START_BIT;

	if (dctx->enable_ke) {
		pr_info("Violation Slave: %s %s%s %s%x %s%x, %s%x, %s%x\n",
		       apusys_modules[i].name,
		       (read_vio == 1) ? "( R" : "(",
		       (write_vio == 1) ? " W ) -" : " ) -",
		       "Vio transaction ID:0x", dbg1,
		       "Vio Addr:0x", dbg2,
		       "High:0x", vio_addr_high,
		       "Domain ID:0x", domain_id);
		WARN_ON(1);
	} else	if (dctx->enable_aee) {
#if defined(CONFIG_MTK_AEE_FEATURE)
		pr_info("execute aee\n");
		aee_kernel_exception("APUSYS_DEVAPC",
			"Violation Slave: %s %s%s %s%x %s%x, %s%x, %s%x\n",
			apusys_modules[i].name,
			(read_vio == 1) ? "( R" : "(",
			(write_vio == 1) ? " W ) -" : " ) -",
			"Vio transaction ID:0x", dbg1,
			"Vio Addr:0x", dbg2,
			"High:0x", vio_addr_high,
			"Domain ID:0x", domain_id);
#endif
	}

	/* unmask irq for module "i" */
	unmask_module_irq(i);
}

static int check_vio_status(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit = 0;

	if (module > ARRAY_SIZE(apusys_modules)) {
		pr_info("module:%d overflow!\n", module);
		return -EFAULT;
	}

	apc_index = module / (MOD_NUM_1_DAPC);
	apc_bit = module % (MOD_NUM_1_DAPC);

	if (readl(DEVAPC_VIO_STA(apc_index)) & (0x1 << apc_bit))
		return 1;

	return 0;
}

static int clear_vio_status(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit = 0;

	if (module > ARRAY_SIZE(apusys_modules)) {
		pr_info("module:%d overflow!\n", module);
		return -EFAULT;
	}

	apc_index = module / (MOD_NUM_1_DAPC);
	apc_bit = module % (MOD_NUM_1_DAPC);

	mt_reg_sync_writel(0x1 << apc_bit, DEVAPC_VIO_STA(apc_index));
	return 0;
}

static int shift_vio_dbg(int shift_bit)
{
	int count = 0, shift_done;

	/* start shift */
	mt_reg_sync_writel(0x1 << shift_bit, DEVAPC_VIO_SHIFT_SEL);
	mt_reg_sync_writel(0x1,	DEVAPC_VIO_SHIFT_CON);

	/* wait for shift done */
	while (((readl(DEVAPC_VIO_SHIFT_CON) & 0x3) != 0x3) && (count < 100))
		count++;

	pr_debug("Shift done %d, %d, SHIFT_SEL: 0x%x, SHIFT_CON=0x%x\n",
		 shift_bit, count, readl(DEVAPC_VIO_SHIFT_SEL),
		 readl(DEVAPC_VIO_SHIFT_CON));

	if ((readl(DEVAPC_VIO_SHIFT_CON) & 0x3) == 0x3)
		shift_done = 0;
	else {
		shift_done = -EFAULT;
		pr_info("Shift failed shift_bit: %d\n", shift_bit);
	}

	/* disable shift mechanism */
	mt_reg_sync_writel(0x0, DEVAPC_VIO_SHIFT_CON);
	mt_reg_sync_writel(0x0, DEVAPC_VIO_SHIFT_SEL);
	mt_reg_sync_writel(0x1 << shift_bit, DEVAPC_VIO_SHIFT_STA);

	return shift_done;
}

static void print_vio_mask_sta(void)
{
	if (!print_debug_log)
		return;

	pr_debug("%s VIO_MASK 0:0x%x, 1:0x%x, 2:0x%x, 3:0x%x\n",
		__func__,
		readl(DEVAPC_VIO_MASK(0)),
		readl(DEVAPC_VIO_MASK(1)),
		readl(DEVAPC_VIO_MASK(2)),
		readl(DEVAPC_VIO_MASK(3)));

	pr_debug("%s VIO_STA 0:0x%x, 1:0x%x, 2:0x%x, 3:0x%x\n",
		__func__,
		readl(DEVAPC_VIO_STA(0)),
		readl(DEVAPC_VIO_STA(1)),
		readl(DEVAPC_VIO_STA(2)),
		readl(DEVAPC_VIO_STA(3)));
}

/* Return 1 means there are some violations */
static inline int is_violation_irq(void)
{
	return (readl(DEVAPC_VIO_STA(0)) | readl(DEVAPC_VIO_STA(1)) |
		readl(DEVAPC_VIO_STA(2)) | readl(DEVAPC_VIO_STA(3))) != 0;
}

static irqreturn_t devapc_vio_handler(int irq_number, void *data)
{
	int i;
	unsigned int dbg0 = 0, dbg1 = 0, dbg2 = 0;
	unsigned int domain_id, read_vio, write_vio, vio_addr_high;
	struct devapc_ctx *dctx = (struct devapc_ctx *)data;

	if (!is_violation_irq())
		return IRQ_NONE;

	if (!dctx || IS_ERR_OR_NULL(devapc_virt)) {
		pr_info("driver abort\n");
		return IRQ_NONE;
	}

	if (irq_number != dctx->irq) {
		pr_info("get unknown irq %d\n", irq_number);
		return IRQ_NONE;
	}

	if (!dctx->irq) {
		pr_info("Disable vio irq handler\n");
		return IRQ_NONE;
	}

	disable_irq_nosync(irq_number);

	print_vio_mask_sta();

	for (i = 0; i <= DEVAPC_VIO_SHIFT_MAX_BIT; ++i) {
		if (readl(DEVAPC_VIO_SHIFT_STA) & (0x1 << i)) {

			if (shift_vio_dbg(i))
				continue;

			pr_info("%s=0x%x, %s=0x%x, %s=0x%x,\n",
				"VIO_SHIFT_STA",
				readl(DEVAPC_VIO_SHIFT_STA),
				"VIO_SHIFT_SEL",
				readl(DEVAPC_VIO_SHIFT_SEL),
				"VIO_SHIFT_CON",
				readl(DEVAPC_VIO_SHIFT_CON));

			dbg0 = readl(DEVAPC_VIO_DBG0);
			dbg1 = readl(DEVAPC_VIO_DBG1);
			dbg2 = readl(DEVAPC_VIO_DBG2);

			domain_id = (dbg0 & DEVAPC_VIO_DBG_DMNID)
				>> DEVAPC_VIO_DBG_DMNID_START_BIT;
			write_vio = (dbg0 & DEVAPC_VIO_DBG_W_VIO)
				>> DEVAPC_VIO_DBG_W_VIO_START_BIT;
			read_vio = (dbg0 & DEVAPC_VIO_DBG_R_VIO)
				>> DEVAPC_VIO_DBG_R_VIO_START_BIT;
			vio_addr_high = (dbg0 & DEVAPC_VIO_ADDR_HIGH)
				>> DEVAPC_VIO_ADDR_HIGH_START_BIT;

			/* violation information */
			pr_info("Violation %s%s%s%x %s%x, %s%x, %s%x\n",
				(read_vio == 1) ? "( R" : "(",
				(write_vio == 1) ? " W ) - " : " ) - ",
				"Vio transaction ID:0x", dbg1,
				"Vio Addr:0x", dbg2,
				"High:0x", vio_addr_high,
				"Domain ID:0x", domain_id);

			pr_info("current - %s%s, %s%i\n",
				"Process:", current->comm,
				"PID:", current->pid);
		}
	}

	for (i = 0; i < ARRAY_SIZE(apusys_modules); i++) {
		if (apusys_modules[i].enable_vio_irq &&
				check_vio_status(i) == 1) {
			clear_vio_status(i);
			pr_info("vio_sta device:%d, slave:%s\n",
				i, apusys_modules[i].name);

			do_kernel_exception(dctx, i, dbg0, dbg1, dbg2);
		}
	}

	print_vio_mask_sta();
	enable_irq(irq_number);

	return IRQ_HANDLED;
}

static void start_apusys_devapc(void *data)
{
	int i = 0;


	/* start apusys devapc violation irq */
	mt_reg_sync_writel(0x80000000, DEVAPC_APC_CON);

	print_vio_mask_sta();

	/* enable violation irq */
	for (i = 0; i < ARRAY_SIZE(apusys_modules); i++) {
		if (apusys_modules[i].enable_vio_irq) {
			clear_vio_status(i);
			unmask_module_irq(i);
		}
	}
	print_vio_mask_sta();

}

static int apusys_devapc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *devapc_node;
	struct devapc_ctx *dctx;

	dev_info(&pdev->dev, "%s +\n", __func__);

	if (!apusys_power_check())
		return 0;

	devapc_node = pdev->dev.of_node;
	if (!devapc_node) {
		dev_info(&pdev->dev, "of_node required\n");
		return -ENODEV;
	}

	dctx = kzalloc(sizeof(*dctx), GFP_KERNEL);
	if (!dctx) {
		ret = -ENOMEM;
		goto err_allocate;
	}

	dctx->virt = of_iomap(devapc_node, 0);
	if (!dctx->virt) {
		ret = -ENOMEM;
		goto err_iomap;
	}
	devapc_virt = dctx->virt;

	dctx->irq = irq_of_parse_and_map(devapc_node, 0);

	ret = request_irq(dctx->irq, devapc_vio_handler,
			  irq_get_trigger_type(dctx->irq) | IRQF_SHARED,
			  "apusys-devapc", dctx);
	if (ret) {
		ret = -EFAULT;
		goto err_request_irq;
	}

	dctx->enable_ke = dctx->enable_aee = dctx->enable_irq = 1;

	start_apusys_devapc(NULL);

	ret = apu_power_callback_device_register(DEVAPC, start_apusys_devapc,
						 NULL);
	if (ret) {
		dev_info(&pdev->dev,
			"callback register return error %d\n", ret);
	}

	print_debug_log = false;
	apusys_dbg_devapc = debugfs_create_dir("apusys_devapc", NULL);
	debugfs_create_bool("debug",
		0644, apusys_dbg_devapc, &print_debug_log);

	dctx->dbg_file = debugfs_create_file("apusys_devapc", 0644,
			apusys_dbg_devapc, dctx, &apusys_devapc_fops);

	if (!dctx->dbg_file)
		dev_info(&pdev->dev, "debugfs create failed\n");

	platform_set_drvdata(pdev, dctx);

	dev_info(&pdev->dev, "%s -\n", __func__);

	return 0;

err_request_irq:
	iounmap(dctx->virt);
err_iomap:
	kfree(dctx);
err_allocate:
	return ret;
}

static int apusys_devapc_remove(struct platform_device *pdev)
{
	struct devapc_ctx *dctx = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s +\n", __func__);

	free_irq(dctx->irq, dctx);
	iounmap(dctx->virt);
	debugfs_remove(dctx->dbg_file);
	kfree(dctx);
	apu_power_callback_device_unregister(DEVAPC);

	dev_dbg(&pdev->dev, "%s -\n", __func__);

	return 0;
}

static const struct of_device_id apusys_devapc_of_match[] = {
	{ .compatible = "mediatek,apusys_devapc", },
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
	return platform_driver_register(&apusys_devapc_driver);
}

static void __exit apusys_devapc_exit(void)
{
	platform_driver_unregister(&apusys_devapc_driver);
}

late_initcall(apusys_devapc_init);
module_exit(apusys_devapc_exit);

MODULE_DESCRIPTION("MTK APUSYS DEVAPC Driver");
MODULE_AUTHOR("SPT1/SS6");
MODULE_LICENSE("GPL");
