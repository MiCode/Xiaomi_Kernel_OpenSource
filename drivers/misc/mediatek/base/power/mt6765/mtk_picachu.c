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
#define NR_PTP1_EFUSE_SPARE_REG	5
#else
#define NR_PTP1_EFUSE_SPARE_REG	3
#endif

#define PICACHU_SIGNATURE		(0xA5)
#define PICACHU_PTP1_EFUSE_MASK		(0xFFFFFF)
#define PICACHU_SIGNATURE_SHIFT_BIT     (24)

#define EEM_BASEADDR    (0x1100B000)
#define EEM_SIZE	(0x1000)

#define TEMPSPARE0      (0xF0)
#define TEMPSPARE1      (0xF4)
#define TEMPSPARE2      (0xF8)

#define EEMSPARE0       (0xF20)
#define EEMSPARE1       (0xF24)
#define EEMSPARE2       (0xF28)
#define EEMSPARE3       (0xF2C)

#define TEMPSPARE0_OFFSET	(0x0F0)
#define EEMSPARE0_OFFSET	(0xF20)

#undef TAG
#define TAG     "[Picachu] "

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

enum {
	OPPH_VMIN_SEARCH, /* OPP High -> OPP0 */
#if ENABLE_LOO
	OPPM_VMIN_SEARCH, /* OPP Medium */
#endif

	NR_OPP_VMIN_SEARCH
};

struct picachu_info {
	/*
	 * Bit[7:0]: vmin@opp0
	 * Bit[15:8]: vmin@opp8
	 * Bit[23:16]: pi_offset@opp0
	 * Bit[31:24]: pi_offset@opp8
	 */
	unsigned int pi_offset_n_vmin;

	/*
	 * L-H, 2L-H, CCI, L-M, 2L-M:
	 *	Bit[7:0]: MTDES
	 *	Bit[15:8]: BDES
	 *	Bit[23:16]: MDES
	 */
	unsigned int ptp1_efuse[NR_PTP1_EFUSE_SPARE_REG];
};

enum mt_picachu_vproc_id {
	MT_PICACHU_LITTLE_VPROC,	/* Little-little, Little and CCI */

	NR_PICACHU_VPROC,
};

static struct picachu_info picachu_data[NR_PICACHU_VPROC];
static void __iomem *eem_base_addr;

/* L-H, 2L-H, CCI, L-M, 2L-M */
static unsigned int efuse_spare_reg[NR_PTP1_EFUSE_SPARE_REG] = {
	EEMSPARE0, EEMSPARE2, TEMPSPARE0, EEMSPARE1, EEMSPARE3,
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

static struct picachu_proc picachu_proc_list[] = {
	{"little", MT_PICACHU_LITTLE_VPROC, EEMSPARE0_OFFSET,
	 PICACHU_PROC_ENTRY_ATTR},
	{0},
};

static void dump_picachu_info(struct seq_file *m, struct picachu_info *info)
{
	unsigned int i;

	seq_printf(m, "0x%X\n", info->pi_offset_n_vmin);

	for (i = 0; i < NR_PTP1_EFUSE_SPARE_REG; i++)
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
			pr_notice("[%s]: create /proc/picachu/%s failed\n",
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
		pr_notice("[%s]: mkdir /proc/picachu failed\n", __func__);
		return -ENOMEM;
	}

	for (proc = picachu_proc_list; proc->name; proc++) {
		dir = proc_mkdir(proc->name, root);
		if (!dir) {
			pr_notice("[%s]: mkdir /proc/picachu/%s failed\n",
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

	if (vproc_id >= NR_PICACHU_VPROC)
		return;

	p = &picachu_data[vproc_id];

	for (proc = picachu_proc_list; proc->name; proc++) {
		if (proc->vproc_id == vproc_id)
			break;
	}

	for (i = 0; i < NR_PTP1_EFUSE_SPARE_REG; i++) {
		val = picachu_read(eem_base_addr + efuse_spare_reg[i]);

		tmp = (val >> PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;
		if (tmp != PICACHU_SIGNATURE)
			continue;

		p->ptp1_efuse[i] = val & PICACHU_PTP1_EFUSE_MASK;
	}

	p->pi_offset_n_vmin = picachu_read(eem_base_addr + TEMPSPARE1);

}

static int eem_ctrl_id[NR_PICACHU_VPROC][NR_PTP1_EFUSE_SPARE_REG] = {
#if ENABLE_LOO
	[MT_PICACHU_LITTLE_VPROC] = {EEM_CTRL_L_HI, EEM_CTRL_2L_HI,
				     EEM_CTRL_CCI, EEM_CTRL_L, EEM_CTRL_2L},
#else
	[MT_PICACHU_LITTLE_VPROC] = {EEM_CTRL_L, EEM_CTRL_2L, EEM_CTRL_CCI},
#endif
};

#if !EEM_FAKE_EFUSE
static void picachu_apply_efuse_to_eem(enum mt_picachu_vproc_id id,
				       struct picachu_info *p)
{
	int i;

	for (i = 0; i < NR_PTP1_EFUSE_SPARE_REG; i++) {

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
	unsigned int i;

	eem_base_addr = ioremap(EEM_BASEADDR, EEM_SIZE);

	if (!eem_base_addr) {
		pr_notice("ioremap failed!\n");
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
