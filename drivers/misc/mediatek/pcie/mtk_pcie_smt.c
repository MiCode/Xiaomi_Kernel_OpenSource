// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <asm-generic/io.h>

#define IOCTL_DEV_IOCTLID	'6'
#define IOCTL_READ		_IOR(IOCTL_DEV_IOCTLID, 0, int)
#define IOCTL_WRITE		_IOW(IOCTL_DEV_IOCTLID, 1, int)

#define PCIE_PHY_BASE		0x11f40000
#define PCIE_DEV_NAME		"mtk_pcie"
#define PCIE_DEV_MAJOR		234
#define PCIE_BUF_SIZE		200
#define PCIE_MAX_ARG_SIZE	10

struct mtk_pcie {
	char *name;
	void __iomem *regs;
	int err_count;
};

static int mtk_pcie_test_open(struct inode *inode, struct file *file)
{

	pr_info("mtk_pcie_test open: successful\n");
	return 0;
}

static int mtk_pcie_test_release(struct inode *inode, struct file *file)
{

	pr_info("mtk_pcie_test release: successful\n");
	return 0;
}

static ssize_t mtk_pcie_test_read(struct file *file, char *buf, size_t count,
				  loff_t *ptr)
{

	pr_info("mtk_pcie_test read: returning zero bytes\n");
	return 0;
}

static ssize_t mtk_pcie_test_write(struct file *file, const char *buf,
				   size_t count, loff_t *ppos)
{

	pr_info("mtk_pcie_test write: accepting zero bytes\n");
	return 0;
}

static struct mtk_pcie *pcie_ctrl;

int mtk_pcie_loopback_test(void)
{
	int val;

	pr_info("%s: %d\n", __func__, __LINE__);

	if (!pcie_ctrl) {
		pr_info("pcie_ctrl is not initialed!\n");
		return -1;
	}

	/* L1ss = enable */
	val = readl(pcie_ctrl->regs + 0x28);
	val |= (0x01 << 5);
	writel(val, pcie_ctrl->regs + 0x28);

	val = readl(pcie_ctrl->regs + 0x28);
	val |= (0x01 << 4);
	writel(val, pcie_ctrl->regs + 0x28);

	val = readl(pcie_ctrl->regs + 0x28);
	val &= (~0x200);
	writel(val, pcie_ctrl->regs + 0x28);

	val = readl(pcie_ctrl->regs + 0x28);
	val |= (0x01 << 8);
	writel(val, pcie_ctrl->regs + 0x28);

	val = readl(pcie_ctrl->regs + 0x28);
	val &= (~0x800);
	writel(val, pcie_ctrl->regs + 0x28);

	val = readl(pcie_ctrl->regs + 0x28);
	val |= (0x01 << 10);
	writel(val, pcie_ctrl->regs + 0x28);

	/* Set Rate=Gen1 */
	usleep_range(1, 2);
	val = readl(pcie_ctrl->regs + 0x70);
	val |= (0x01);
	writel(val, pcie_ctrl->regs + 0x70);

	val = readl(pcie_ctrl->regs + 0x70);
	val &= (~0x30000);
	writel(val, pcie_ctrl->regs + 0x70);

	val = readl(pcie_ctrl->regs + 0x70);
	val |= (0x01 << 4);
	writel(val, pcie_ctrl->regs + 0x70);

	usleep_range(1, 2);
	val = readl(pcie_ctrl->regs + 0x70);
	val &= (~0x10);
	writel(val, pcie_ctrl->regs + 0x70);

	/* Force PIPE (P0) */
	val = readl(pcie_ctrl->regs + 0x70);
	val |= (0x01);
	writel(val, pcie_ctrl->regs + 0x70);

	val = readl(pcie_ctrl->regs + 0x70);
	val &= (~0xc00);
	writel(val, pcie_ctrl->regs + 0x70);

	val = readl(pcie_ctrl->regs + 0x70);
	val &= (~0x3000);
	writel(val, pcie_ctrl->regs + 0x70);

	val = readl(pcie_ctrl->regs + 0x70);
	val |= (0x01 << 8);
	writel(val, pcie_ctrl->regs + 0x70);

	val = readl(pcie_ctrl->regs + 0x70);
	val |= (0x01 << 4);
	writel(val, pcie_ctrl->regs + 0x70);

	usleep_range(1, 2);
	val = readl(pcie_ctrl->regs + 0x70);
	val &= (~0x10);
	writel(val, pcie_ctrl->regs + 0x70);

	/* Set TX output Pattern */
	usleep_range(1, 2);
	val = readl(pcie_ctrl->regs + 0x4010);
	val |= (0x0d);
	writel(val, pcie_ctrl->regs + 0x4010);

	/* Set TX PTG Enable */
	val = readl(pcie_ctrl->regs + 0x28);
	val |= (0x01 << 30);
	writel(val, pcie_ctrl->regs + 0x28);

	/* Set RX Pattern Checker (Type & Enable) */
	val = readl(pcie_ctrl->regs + 0x501c);
	val |= (0x01 << 1);
	writel(val, pcie_ctrl->regs + 0x501c);

	val = readl(pcie_ctrl->regs + 0x501c);
	val |= (0x0d << 4);
	writel(val, pcie_ctrl->regs + 0x501c);

	/* toggle ptc_en for status counter clear */
	val = readl(pcie_ctrl->regs + 0x501c);
	val &= (~0x2);
	writel(val, pcie_ctrl->regs + 0x501c);

	val = readl(pcie_ctrl->regs + 0x501c);
	val |= (0x01 << 1);
	writel(val, pcie_ctrl->regs + 0x501c);

	/* Check status */
	val = readl(pcie_ctrl->regs + 0x50c8);
	if ((val & 0x3) != 0x3) {
		pcie_ctrl->err_count = val >> 12;
		pr_info("PCIe test failed!\n");
		pr_info("error count: %d\n", pcie_ctrl->err_count);
		pr_info("reg val @0x11f450c8 = %#x\n", val);
		return -1;
	}

	return 0;
}

int pcie_call_function(char *buf)
{
	int argc = 0;
	char *argv[PCIE_MAX_ARG_SIZE];

	do {
		argv[argc] = strsep(&buf, " ");
		argc++;
	} while (buf);

	if (!strcmp(argv[1], "PCIE_SMT"))
		return mtk_pcie_loopback_test();

	return -1;
}

static char pcie_w_buf[PCIE_BUF_SIZE];
static char pcie_r_buf[PCIE_BUF_SIZE];

static long mtk_pcie_test_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int len = PCIE_BUF_SIZE;
	int ret = 0;

	switch (cmd) {
	case IOCTL_READ:
		ret = copy_to_user((char *) arg, pcie_r_buf, len);
		pr_info("IOCTL_READ: %s\r\n", pcie_r_buf);
		break;
	case IOCTL_WRITE:
		ret = copy_from_user(pcie_w_buf, (char *) arg, len);
		pr_info("IOCTL_WRITE: %s\r\n", pcie_w_buf);

		return pcie_call_function(pcie_w_buf);
	default:
		return -ENOTTY;
	}

	return len;
}

static const struct file_operations mtk_pcie_test_fops = {
	.owner = THIS_MODULE,
	.read = mtk_pcie_test_read,
	.write = mtk_pcie_test_write,
	.unlocked_ioctl = mtk_pcie_test_ioctl,
	.compat_ioctl = mtk_pcie_test_ioctl,
	.open = mtk_pcie_test_open,
	.release = mtk_pcie_test_release,
};

static struct class *f_class;

static int __init mtk_test_init(void)
{
	int ret = 0;

	pcie_ctrl = kzalloc(sizeof(*pcie_ctrl), GFP_KERNEL);

	pcie_ctrl->name = PCIE_DEV_NAME;
	pcie_ctrl->regs = ioremap(PCIE_PHY_BASE, 0x6000);

	/* char device support */
	ret = register_chrdev(PCIE_DEV_MAJOR, PCIE_DEV_NAME,
			      &mtk_pcie_test_fops);
	if (ret < 0) {
		pr_info("mtk_pcie_test Init failed, %d\n", ret);
		return ret;
	}

	f_class = class_create(THIS_MODULE, PCIE_DEV_NAME);
	device_create(f_class, NULL, MKDEV(PCIE_DEV_MAJOR, 0), NULL,
		      PCIE_DEV_NAME);

	return ret;
}

static void __exit mtk_test_exit(void)
{
	/* remove char device */
	device_destroy(f_class, MKDEV(PCIE_DEV_MAJOR, 0));
	class_destroy(f_class);
	unregister_chrdev(PCIE_DEV_MAJOR, PCIE_DEV_NAME);
}

module_init(mtk_test_init);
module_exit(mtk_test_exit);
