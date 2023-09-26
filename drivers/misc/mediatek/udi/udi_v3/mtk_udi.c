// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define __MTK_UDI_C__

/* system includes */
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#else
#include <common.h>
#endif
/* local includes */
#include "mtk_udi_internal.h"
#include <linux/sched/debug.h>

#ifdef CONFIG_OF
#ifndef CONFIG_FPGA_EARLY_PORTING
static unsigned long __iomem *udipin_base;
static unsigned int udi_offset1;
static unsigned int udi_value1;
static unsigned int udi_offset2;
static unsigned int udi_value2;
#endif
static unsigned long __iomem *udipin_mux1;
static unsigned long __iomem *udipin_mux2;
static unsigned int udipin_value1;
static unsigned int udipin_value2;
#endif

static unsigned int func_lv_mask_udi;

/*-----------------------------------------*/
/* Reused code start                       */
/*-----------------------------------------*/
#define udi_read(addr)			readl((void *)addr)
#define udi_write(addr, val) \
	do { writel(val, (void *)addr); wmb(); } while (0) /* sync write */

/*
 * LOG
 */
#define	UDI_TAG	  "[mt_udi] "
#ifdef __KERNEL__
#define udi_info(fmt, args...)		pr_notice(UDI_TAG	fmt, ##args)
#define udi_ver(fmt, args...)	\
	do {	\
		if (func_lv_mask_udi)	\
			pr_notice(UDI_TAG	fmt, ##args);	\
	} while (0)

#else
#define udi_info(fmt, args...)		printf(UDI_TAG fmt, ##args)
#define udi_ver(fmt, args...)		printf(UDI_TAG fmt, ##args)
#endif


unsigned int udi_addr_phy;

unsigned int udi_reg_read(unsigned int addr)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_UDI_READ,
		addr,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

void udi_reg_write(unsigned int addr, unsigned int val)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_UDI_WRITE,
		addr,
		val,
		0, 0, 0, 0, 0, &res);

}

struct platform_device udi_pdev = {
	.name   = "mt_udi",
	.id     = -1,
};


#ifdef CONFIG_OF
static const struct of_device_id mt_udi_of_match[] = {
	{ .compatible = "mediatek,udi", },
	{},
};
#endif


#ifdef __KERNEL__ /* __KERNEL__ */

static int udi_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct device_node *node = NULL;
	int rc = 0;

	node = of_find_matching_node(NULL, mt_udi_of_match);
	if (!node) {
		udi_info("error: cannot find node UDI!\n");
		return 0;
	}

	/* Setup IO addresses and printf */
	udipin_base = of_iomap(node, 0); /* UDI pinmux reg */
	udi_info("udipin_base = 0x%lx.\n", (unsigned long)udipin_base);
	if (!udipin_base) {
		udi_info("udi pinmux get some base NULL.\n");
		return 0;
	}

	rc = of_property_read_u32(node, "udi_offset1", &udi_offset1);
	if (!rc) {
		udi_info("get udi_offset1(0x%x)\n", udi_offset1);
		if (udi_offset1 != 0)
			udipin_mux1 = (unsigned long *)(
					(unsigned long)udipin_base
					+ (unsigned long)udi_offset1);
	}

	rc = of_property_read_u32(node, "udi_value1", &udi_value1);
	if (!rc) {
		udi_info("get udi_value1(0x%x)\n", udi_value1);
		if (udi_value1 != 0)
			udipin_value1 = udi_value1;
	}

	rc = of_property_read_u32(node, "udi_offset2", &udi_offset2);
	if (!rc) {
		udi_info("get udi_offset2(0x%x)\n", udi_offset2);
		if (udi_offset2 != 0)
			udipin_mux2 = (unsigned long *)(
						(unsigned long)udipin_base
						+ (unsigned long)udi_offset2);
	}

	rc = of_property_read_u32(node, "udi_value2", &udi_value2);
	if (!rc) {
		udi_info("get udi_value2(0x%x)\n", udi_value2);
		if (udi_value2 != 0)
			udipin_value2 = udi_value2;
	}

#endif

	return 0;
}

static struct platform_driver udi_pdrv = {
	.remove     = NULL,
	.shutdown   = NULL,
	.probe      = udi_probe,
	.suspend    = NULL,
	.resume     = NULL,
	.driver     = {
		.name   = "mt_udi",
#ifdef CONFIG_OF
	.of_match_table = mt_udi_of_match,
#endif
	},
};


#ifdef CONFIG_PROC_FS

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;
	if (count >= PAGE_SIZE)
		goto out0;
	if (copy_from_user(buf, buffer, count))
		goto out0;

	buf[count] = '\0';
	return buf;

out0:
	free_page((unsigned long)buf);
	return NULL;
}

/* udi_debug_reg */
static int udi_reg_proc_show(struct seq_file *m, void *v)
{
	if (func_lv_mask_udi == 1)
		seq_printf(m, "Reg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	return 0;
}

static ssize_t udi_reg_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int udi_value = 0, udi_reg_msb, udi_reg_lsb;
	unsigned char udi_rw[5] = { 0, 0, 0, 0, 0 };

	if (!buf)
		return -EINVAL;

	/* protect udi reg read/write */
	if (func_lv_mask_udi == 0) {
		free_page((unsigned long)buf);
		return count;
	}

	if (sscanf(buf, "%1s %x %d %d %x", udi_rw, &udi_addr_phy,
			&udi_reg_msb, &udi_reg_lsb, &udi_value) == 5) {
		/* f format or 'f', addr, MSB, LSB, value */
		udi_reg_field(udi_addr_phy,
			udi_reg_msb : udi_reg_lsb, udi_value);
		udi_info("Read back, Reg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	} else if (sscanf(buf, "%1s %x %x", udi_rw,
				&udi_addr_phy, &udi_value) == 3) {
		/* w format or 'w', addr, value */
		udi_reg_write(udi_addr_phy, udi_value);
		udi_info("Read back, Reg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	} else if (sscanf(buf, "%1s %x", udi_rw, &udi_addr_phy) == 2) {
		/* r format or 'r', addr */
		udi_info("Read back, aReg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	} else {
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_debug\n");
		memset(udi_rw, 0, sizeof(udi_rw));
	}

	free_page((unsigned long)buf);
	return count;
}

/* udi_pinmux_switch */
static int udi_pinmux_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU UDI pinmux reg[0x%lx] = 0x%x.\n",
		(unsigned long)udipin_mux1, udi_read(udipin_mux1));
	seq_printf(m, "CPU UDI pinmux reg[0x%lx] = 0x%x.\n",
		(unsigned long)udipin_mux2, udi_read(udipin_mux2));
	return 0;
}

static ssize_t udi_pinmux_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int pin_switch = 0U;

	if (buf == NULL)
		return -EINVAL;

	if (kstrtoint(buf, 10, &pin_switch) == 0) {
		if (pin_switch == 1U) {
			udi_write(udipin_mux1, udipin_value1);
			udi_write(udipin_mux2, udipin_value2);
		}
	} else
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_pinmux\n");

	free_page((unsigned long)buf);
	return (long)count;
}

/* udi_debug_info_print_flag */
static int udi_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "UDI debug (log level) = %d.\n", func_lv_mask_udi);
	return 0;
}

static ssize_t udi_debug_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int dbg_lv = 0;

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &dbg_lv))
		func_lv_mask_udi = dbg_lv;
	else
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_debug\n");

	free_page((unsigned long)buf);
	return count;
}


#define PROC_FOPS_RW(name)          \
static int name ## _proc_open(struct inode *inode, struct file *file)   \
{                                   \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
}                                   \
static const struct file_operations name ## _proc_fops = {      \
	.owner          = THIS_MODULE,                  \
	.open           = name ## _proc_open,               \
	.read           = seq_read,                 \
	.llseek         = seq_lseek,                    \
	.release        = single_release,               \
	.write          = name ## _proc_write,              \
}

#define PROC_FOPS_RO(name)         \
static int name ## _proc_open(struct inode *inode, struct file *file)   \
{                                   \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
}                                   \
static const struct file_operations name ## _proc_fops = {      \
	.owner          = THIS_MODULE,                  \
	.open           = name ## _proc_open,               \
	.read           = seq_read,                 \
	.llseek         = seq_lseek,                    \
	.release        = single_release,               \
}

#define PROC_ENTRY(name)    {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(udi_reg);			/* for any register read/write */
PROC_FOPS_RW(udi_pinmux);		/* for udi pinmux switch */
PROC_FOPS_RW(udi_debug);		/* for debug information */

static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(udi_reg),
		PROC_ENTRY(udi_pinmux),
		PROC_ENTRY(udi_debug),
	};

	dir = proc_mkdir("udi", NULL);

	if (!dir) {
		udi_info("fail to create /proc/udi @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
			0664, dir, entries[i].fops))
			udi_info("%s(), create /proc/udi/%s failed\n",
				__func__, entries[i].name);
	}

	return 0;
}

#endif /* CONFIG_PROC_FS */

/*
 * Module driver
 */
static int __init udi_init(void)
{
	int err = 0;

	/* initial value */
	func_lv_mask_udi = 0;

	err = platform_driver_register(&udi_pdrv);
	if (err) {
		udi_info("%s(), UDI driver callback register failed..\n",
				__func__);
		return err;
	}

#ifdef CONFIG_PROC_FS
	/* init proc */
	if (_create_procfs()) {
		err = -ENOMEM;
		return err;
	}
#endif

	return 0;
}

static void __exit udi_exit(void)
{
	udi_info("UDI de-initialization\n");
	platform_driver_unregister(&udi_pdrv);
}

module_init(udi_init);
module_exit(udi_exit);

MODULE_DESCRIPTION("MediaTek UDI Driver v3");
MODULE_LICENSE("GPL");
#endif /* __KERNEL__ */
#undef __MTK_UDI_C__
