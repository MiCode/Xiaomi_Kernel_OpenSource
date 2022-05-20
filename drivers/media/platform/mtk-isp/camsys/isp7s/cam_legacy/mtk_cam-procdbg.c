// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define cam_readl(addr)		readl(addr)
#define cam_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

struct cmd_fn {
	const char	*cmd;
	int (*fn)(struct seq_file *s, void *v);
};

#define CMDFN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

static char last_cmd[128] = "null";

static void *reg_from_str(const char *str)
{
	static phys_addr_t phys;
	static void __iomem *virt;

	if (sizeof(void *) == sizeof(unsigned long)) {
		unsigned long v;

		if (kstrtoul(str, 0, &v) == 0U) {
			if ((0xf0000000 & v) < 0x20000000) {
				if (virt != NULL && v > phys
						&& v < phys + PAGE_SIZE)
					return virt + v - phys;

				if (virt != NULL)
					iounmap(virt);

				phys = v & ~(PAGE_SIZE - 1U);
				virt = ioremap(phys, PAGE_SIZE);

				return virt + v - phys;
			}

			return (void *)((uintptr_t)v);
		}
	} else if (sizeof(void *) == sizeof(unsigned long long)) {
		unsigned long long v;

		if (kstrtoull(str, 0, &v) == 0) {
			if ((0xfffffffff0000000ULL & v) < 0x20000000) {
				if (virt && v > phys && v < phys + PAGE_SIZE)
					return virt + v - phys;

				if (virt != NULL)
					iounmap(virt);

				phys = v & ~(PAGE_SIZE - 1);
				virt = ioremap(phys, PAGE_SIZE);

				return virt + v - phys;
			}

			return (void *)((uintptr_t)v);
		}
	} else {
		pr_info("unexpected pointer size: sizeof(void *): %zu\n",
			sizeof(void *));
	}

	pr_info("%s(): parsing error: %s\n", __func__, str);

	return NULL;
}

static int parse_reg_val_from_cmd(void __iomem **preg, unsigned long *pval)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *reg_str;
	char *val_str;
	int r = 0;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	reg_str = strsep(&c, " ");
	val_str = strsep(&c, " ");

	if (preg != NULL && reg_str != NULL) {
		*preg = reg_from_str(reg_str);
		if (*preg != NULL)
			r++;
	}

	if (pval != NULL && val_str != NULL && kstrtoul(val_str, 0, pval) == 0)
		r++;

	return r;
}

static int camdbg_reg_read(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	if (parse_reg_val_from_cmd(&reg, NULL) != 1)
		return 0;

	seq_printf(s, "cmd: %s\n", last_cmd);
	//seq_printf(s, "readl(0x%p): ", reg);

	val = cam_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int camdbg_reg_write(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	if (parse_reg_val_from_cmd(&reg, &val) != 2)
		return 0;

	seq_printf(s, "cmd: %s\n", last_cmd);
	//seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	cam_writel(reg, val);
	val = cam_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static const struct cmd_fn common_cmds[] = {
	CMDFN("reg_read", camdbg_reg_read),
	CMDFN("reg_write", camdbg_reg_write),
};

static int camdbg_show(struct seq_file *s, void *v)
{
	const struct cmd_fn *cf;
	char cmd[sizeof(last_cmd)];

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	for (cf = common_cmds; cf->cmd != NULL; cf++) {
		char *c = cmd;
		char *token = strsep(&c, " ");

		if (strcmp(cf->cmd, token) == 0)
			return cf->fn(s, v);
	}

	return 0;
}

static int camdbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, camdbg_show, NULL);
}

static ssize_t camdbg_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *data)
{
	if (count == 0 || count >= sizeof(last_cmd) - 1UL)
		return 0;

	if (copy_from_user(last_cmd, buffer, count) != 0UL)
		return 0;

	last_cmd[count] = '\0';

	if (last_cmd[count - 1UL] == '\n') {
		last_cmd[count - 1UL] = '\0';
		return (ssize_t)count;
	}

	return 0;
}

static const struct proc_ops camdbg_fops = {
	.proc_open	= camdbg_open,
	.proc_read	= seq_read,
	.proc_write	= camdbg_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int __init camdbg_debug_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("camdbg", 0644, NULL, &camdbg_fops);
	if (entry == 0)
		return -ENOMEM;

	return 0;
}

static int __init camdbg_module_init(void)
{
	int r = 0;

	r = camdbg_debug_init();
	if (r != 0)
		goto err;

err:
	return r;
}

static void __exit camdbg_module_exit(void)
{
}

module_init(camdbg_module_init);
module_exit(camdbg_module_exit);
MODULE_DESCRIPTION("Mediatek Camsys debug driver");
MODULE_LICENSE("GPL v2");
