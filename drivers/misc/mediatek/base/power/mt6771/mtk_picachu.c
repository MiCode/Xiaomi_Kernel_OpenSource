/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <mt-plat/aee.h>
#endif

#include "mtk_eem.h"

#if ENABLE_LOO
/*
 * Little cluster: L = 2, CCI = 1
 * Big cluster: Big = 1
 */
#define NR_EEM_EFUSE_PER_VPROC	(3)
#else
/*
 * Little cluster: L = 1, CCI = 1
 * Big cluster: Big = 1
 */
#define NR_EEM_EFUSE_PER_VPROC	(2)
#endif

#define PICACHU_SIGNATURE		(0xA5)
#define PICACHU_PTP1_EFUSE_MASK		(0xFFFFFF)
#define PICACHU_SIGNATURE_SHIFT_BIT     (24)

#define EEM_BASEADDR		(0x1100B000)
#define EEM_SIZE		(0x1000)
#define TEMPSPARE0_OFFSET	(0x0F0)
#define TEMPSPARE2_OFFSET	(0x0F8)
#define EEMSPARE0_OFFSET	(0xF20)

#undef TAG
#define TAG     "[Picachu] "

#define PICACHU_PR_ERR(fmt, args...)		\
	pr_err(TAG"[ERROR]"fmt, ##args)
#define PICACHU_INFO(fmt, args...)		\
	pr_info(TAG""fmt, ##args)

#define picachu_read(addr)		__raw_readl((void __iomem *)(addr))
#define picachu_write(addr, val)	mt_reg_sync_writel(val, addr)

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
		.write          = name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

#define PICACHU_PROC_ENTRY_ATTR (0440)

struct picachu_info {
	unsigned int vmin : 16;

	unsigned int pi_offset : 8;

	unsigned int dvfs_low_b_l : 4;
	unsigned int dvfs_low_cci : 4;

	/*
	 * Bit[7:0]: MTDES
	 * Bit[15:8]: BDES
	 * Bit[23:16]: MDES
	 */
	unsigned int ptp1_efuse[NR_EEM_EFUSE_PER_VPROC];
};

struct picachu_proc {
	char *name;
	int vproc_id;
	unsigned int spare_reg_offset;
	umode_t mode;
};

struct pentry {
	const char *name;
	const struct file_operations *fops;
};

enum mt_picachu_vproc_id {
	MT_PICACHU_LITTLE_VPROC,	/* Little + CCI */
	MT_PICACHU_BIG_VPROC,		/* B */
	MT_PICACHU_GPU,

	NR_PICACHU_VPROC,
};

static struct picachu_info picachu_data[NR_PICACHU_VPROC];
static void __iomem *eem_base_addr;

static struct picachu_proc picachu_proc_list[] = {
	{"little", MT_PICACHU_LITTLE_VPROC, EEMSPARE0_OFFSET,
	 PICACHU_PROC_ENTRY_ATTR},
	{"big", MT_PICACHU_BIG_VPROC, TEMPSPARE0_OFFSET,
	 PICACHU_PROC_ENTRY_ATTR},
	{"gpu", MT_PICACHU_GPU, TEMPSPARE2_OFFSET,
	 PICACHU_PROC_ENTRY_ATTR},
	{0},
};

static void dump_picachu_info(struct seq_file *m, struct picachu_info *info)
{
	unsigned int i;

	seq_printf(m, "0x%X\n", info->vmin);
	seq_printf(m, "0x%X\n", info->pi_offset);
	seq_printf(m, "0x%X\n", info->dvfs_low_b_l);
	seq_printf(m, "0x%X\n", info->dvfs_low_cci);

	for (i = 0; i < NR_EEM_EFUSE_PER_VPROC; i++)
		seq_printf(m, "0x%X\n", info->ptp1_efuse[i]);
}

static int picachu_dump_proc_show(struct seq_file *m, void *v)
{
	dump_picachu_info(m, (struct picachu_info *) m->private);

	return 0;
}

PROC_FOPS_RO(picachu_dump);

static int create_procfs_entries(struct proc_dir_entry *dir,
				 struct picachu_proc *proc)
{
	int i, num;

	struct pentry entries[] = {
		PROC_ENTRY(picachu_dump),
	};

	num = ARRAY_SIZE(entries);

	for (i = 0; i < num; i++) {
		if (!proc_create_data(entries[i].name, proc->mode, dir,
				entries[i].fops,
				(void *) &picachu_data[proc->vproc_id])) {
			PICACHU_INFO("[%s]: create /proc/picachu/%s failed\n",
					__func__, entries[i].name);
			return -ENOMEM;
		}
	}

	return 0;
}

static int create_procfs(void)
{
	struct proc_dir_entry *root, *dir;
	struct picachu_proc *proc;
	int ret;

	root = proc_mkdir("picachu", NULL);

	if (!root) {
		PICACHU_PR_ERR("[%s]: mkdir /proc/picachu failed\n", __func__);
		return -ENOMEM;
	}

	for (proc = picachu_proc_list; proc->name; proc++) {
		dir = proc_mkdir(proc->name, root);
		if (!dir) {
			PICACHU_INFO("[%s]: mkdir /proc/picachu/%s failed\n",
					__func__, proc->name);
			return -ENOMEM;
		}

		ret = create_procfs_entries(dir, proc);
		if (ret)
			return ret;
	}

	return 0;
}

static void picachu_get_data(enum mt_picachu_vproc_id vproc_id)
{
	struct picachu_proc *proc;
	unsigned int i, val, tmp;
	struct picachu_info *p;
	void __iomem *reg;

	if (vproc_id >= NR_PICACHU_VPROC)
		return;

	p = &picachu_data[vproc_id];

	for (proc = picachu_proc_list; proc->name; proc++) {
		if (proc->vproc_id == vproc_id)
			break;
	}

	reg = eem_base_addr + proc->spare_reg_offset;

	if (vproc_id == MT_PICACHU_GPU) {
		/* GPU has only one register to be retrieved. */
		val = picachu_read(reg);

		tmp = (val >> PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;
		if (tmp != PICACHU_SIGNATURE)
			return;

		p->ptp1_efuse[0] = val & PICACHU_PTP1_EFUSE_MASK;

		return;
	}

	for (i = 0; i < NR_EEM_EFUSE_PER_VPROC - 1; i++, reg += 4) {
		val = picachu_read(reg);

		tmp = (val >> PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;
		if (tmp != PICACHU_SIGNATURE)
			continue;

		p->ptp1_efuse[i] = val & PICACHU_PTP1_EFUSE_MASK;
	}

	val = picachu_read(reg);

	p->vmin = val & 0xFFFF;
	p->pi_offset = (val >> 16) & 0xFF;
	p->dvfs_low_b_l = (val >> 24) & 0xF;
	p->dvfs_low_cci = (val >> 28) & 0xF;

	if (vproc_id != MT_PICACHU_LITTLE_VPROC)
		return;

	/* Read CCI PTP1 efuse */
	val = picachu_read(reg + 4);
	tmp = (val >> PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;
	if (tmp != PICACHU_SIGNATURE)
		return;

	p->ptp1_efuse[NR_EEM_EFUSE_PER_VPROC - 1] =
				val & PICACHU_PTP1_EFUSE_MASK;
}

static int eem_ctrl_id[NR_PICACHU_VPROC][NR_EEM_EFUSE_PER_VPROC] = {
#if ENABLE_LOO
	[MT_PICACHU_LITTLE_VPROC] = {EEM_CTRL_2L_HI, EEM_CTRL_2L, EEM_CTRL_CCI},
	[MT_PICACHU_BIG_VPROC] = {EEM_CTRL_L_HI, EEM_CTRL_L, -1},
	[MT_PICACHU_GPU] = {EEM_CTRL_GPU, -1, -1},
#else
	[MT_PICACHU_LITTLE_VPROC] = {EEM_CTRL_2L, EEM_CTRL_CCI},
	[MT_PICACHU_BIG_VPROC] = {EEM_CTRL_L, -1},
	[MT_PICACHU_GPU] = {EEM_CTRL_GPU, -1},
#endif
};

#if !EEM_FAKE_EFUSE
static void picachu_apply_efuse_to_eem(enum mt_picachu_vproc_id id,
				       struct picachu_info *p)
{
	int i;

	for (i = 0; i < NR_EEM_EFUSE_PER_VPROC; i++) {

		if (!p->ptp1_efuse[i] || eem_ctrl_id[id][i] == -1)
			continue;

		eem_set_pi_efuse(eem_ctrl_id[id][i], p->ptp1_efuse[i]);
	}
}
#else
static void picachu_apply_efuse_to_eem(enum mt_picachu_vproc_id id,
				       struct picachu_info *p)
{
}
#endif


static int __init picachu_init(void)
{
	struct picachu_info *p;
	unsigned int i;

	eem_base_addr = ioremap(EEM_BASEADDR, EEM_SIZE);

	if (!eem_base_addr) {
		PICACHU_PR_ERR("ioremap failed!\n");
		return -ENOMEM;
	}


	/* Update Picachu calibration data if the data is valid. */
	for (i = 0; i < NR_PICACHU_VPROC; i++) {
		picachu_get_data(i);

		picachu_apply_efuse_to_eem(i, &picachu_data[i]);
	}

	create_procfs();

	return 0;
}

static void __exit picachu_exit(void)
{
	if (eem_base_addr)
		iounmap(eem_base_addr);
}

subsys_initcall(picachu_init);

MODULE_DESCRIPTION("MediaTek Picachu Driver v0.1");
MODULE_LICENSE("GPL");
