/*
 * Copyright (C) 2018 MediaTek Inc.
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

/*
 * Little cluster: L = 2, CCI = 2
 * Big cluster: Big = 2
 */
#define NR_EEM_EFUSE_PER_VPROC	(4)

#define PICACHU_SIG			(0xA5)
#define PICACHU_LOO_SIG			(0x5A)
#define PICACHU_PTP1_EFUSE_MASK		(0xFFFFFF)
#define PICACHU_SIGNATURE_SHIFT_BIT     (24)

#define EEM_BASEADDR		(0x1100B000)
#define EEM_SIZE		(0x1000)

#define TEMPSPARE0_OFFSET	(0x0F0)
#define TEMPSPARE0_1_OFFSET	(0x1F0)
#define EEMCTRLSPARE0_OFFSET	(0xCF0)
#define EEMSPARE0_OFFSET	(0xF20)

#define PICACHU_DVTFIXED_V1	(9)
#define PICACHU_DVTFIXED_V2	(6)

#undef TAG
#define TAG     "[Picachu] "

#define picachu_pr_notice(fmt, args...)	pr_notice(TAG fmt, ##args)

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

#define PICACHU_PROC_ENTRY_ATTR (0664)

struct picachu_info {
	union {
		unsigned int priv[6];

		struct {
			/*
			 * Bit[7:0]: MTDES
			 * Bit[15:8]: BDES
			 * Bit[23:16]: MDES
			 */
			unsigned int ptp1_efuse[NR_EEM_EFUSE_PER_VPROC];

			/*
			 * Bit[7:0]: vmin@opp0
			 * Bit[15:8]: vmin@mid_opp
			 * Bit[23:16]: pi_offset@opp0
			 * Bit[31:24]: pi_offset@mid_opp
			 */
			unsigned int pi_offset_n_vmin;

			unsigned int pi_dvtfixed : 4;
			unsigned int loo_enabled : 1;
			unsigned int reserved : 27;
		};
	};
};

struct picachu_proc {
	char *name;
	int vproc_id;
	unsigned int efuse_spare_reg_offset;
	unsigned int meta_spare_reg_offset;
	umode_t mode;
};

struct pentry {
	const char *name;
	const struct file_operations *fops;
};

enum mt_picachu_vproc_id {
	MT_PICACHU_L_VPROC,	/* Little + CCI */
	MT_PICACHU_B_VPROC,	/* B */

	NR_PICACHU_VPROC,
};


/* LOO-enabled eem ctrl id */
static int eem_ctrl_id_loo[NR_PICACHU_VPROC][NR_EEM_EFUSE_PER_VPROC] = {
	/* MT_PICACHU_L_VPROC */
	{EEM_CTRL_L, EEM_CTRL_CCI, EEM_CTRL_L, EEM_CTRL_CCI},
#if ENABLE_LOO
	/* MT_PICACHU_B_VPROC */
	{EEM_CTRL_B_HI, -1, EEM_CTRL_B, -1},
#endif
};

/* Legacy eem ctrl id */
static int eem_ctrl_id[NR_PICACHU_VPROC][NR_EEM_EFUSE_PER_VPROC] = {
	/* MT_PICACHU_L_VPROC */
	{EEM_CTRL_L, EEM_CTRL_CCI, -1, -1},

	/* MT_PICACHU_B_VPROC */
	{EEM_CTRL_B, -1, -1, -1},
};

static int *eem_ctrl_id_list[2] = {&eem_ctrl_id[0][0], &eem_ctrl_id_loo[0][0]};

static struct picachu_info picachu_data[NR_PICACHU_VPROC];
static void __iomem *eem_base_addr;

static struct picachu_proc picachu_proc_list[] = {
	{"little", MT_PICACHU_L_VPROC,
	 EEMSPARE0_OFFSET, TEMPSPARE0_1_OFFSET, PICACHU_PROC_ENTRY_ATTR},
	{"big", MT_PICACHU_B_VPROC,
	 TEMPSPARE0_OFFSET, EEMCTRLSPARE0_OFFSET, PICACHU_PROC_ENTRY_ATTR},
	{0},
};

static void picachu_apply_efuse_to_eem(enum mt_picachu_vproc_id id,
				       struct picachu_info *p)
{
	int i, array_idx;
	int *ctrl_id;

	ctrl_id = eem_ctrl_id_list[p->loo_enabled];

	/* Get the corresponding array index */
	array_idx = id * NR_EEM_EFUSE_PER_VPROC;

	for (i = 0; i < NR_EEM_EFUSE_PER_VPROC; i++, array_idx++) {

		if (p->pi_dvtfixed == PICACHU_DVTFIXED_V1 ||
			p->pi_dvtfixed == PICACHU_DVTFIXED_V2) {

			eem_set_pi_dvtfixed(*(ctrl_id + array_idx),
							p->pi_dvtfixed);
		}

		if (!p->ptp1_efuse[i] || *(ctrl_id + array_idx) == -1)
			continue;

		eem_set_pi_efuse(*(ctrl_id + array_idx),
				p->ptp1_efuse[i], p->loo_enabled);
	}
}

static void dump_picachu_info(struct seq_file *m, struct picachu_info *info)
{
	unsigned int i;

	seq_printf(m, "0x%X\n", info->pi_offset_n_vmin);
	seq_printf(m, "0x%X\n", info->pi_dvtfixed);

	for (i = 0; i < NR_EEM_EFUSE_PER_VPROC; i++)
		seq_printf(m, "0x%X\n", info->ptp1_efuse[i]);

	seq_printf(m, "loo_enable <%d>\n", info->loo_enabled);
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
			picachu_pr_notice("create /proc/picachu/%s failed\n",
					entries[i].name);
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
		picachu_pr_notice("mkdir /proc/picachu failed\n");
		return -ENOMEM;
	}

	for (proc = picachu_proc_list; proc->name; proc++) {
		dir = proc_mkdir(proc->name, root);
		if (!dir) {
			picachu_pr_notice("mkdir /proc/picachu/%s failed\n",
							proc->name);
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
	unsigned int i, val, tmp, sig, n;
	struct picachu_proc *proc;
	struct picachu_info *p;
	void __iomem *reg;

	if (vproc_id >= NR_PICACHU_VPROC)
		return;

	p = &picachu_data[vproc_id];

	for (proc = picachu_proc_list; proc->name; proc++) {
		if (proc->vproc_id == vproc_id)
			break;
	}

	reg = eem_base_addr + proc->efuse_spare_reg_offset;

	/* Get signature */
	sig = (picachu_read(reg) >> PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;

	if (sig == PICACHU_SIG || sig == PICACHU_LOO_SIG) {
		for (i = 0; i < NR_EEM_EFUSE_PER_VPROC; i++, reg += 4) {
			val = picachu_read(reg);

			tmp = (val >> PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;
			if (tmp != sig)
				continue;

			p->priv[i] = val & PICACHU_PTP1_EFUSE_MASK;
		}
	}

	reg = eem_base_addr + proc->meta_spare_reg_offset;

	n = sizeof(struct picachu_info) / sizeof(unsigned int);

	for (i = NR_EEM_EFUSE_PER_VPROC; i < n; i++, reg += 4)
		p->priv[i] = picachu_read(reg);

	p->loo_enabled = !(sig ^ PICACHU_LOO_SIG);
}

static int __init picachu_init(void)
{
	struct picachu_info *p;
	unsigned int i;

	eem_base_addr = ioremap(EEM_BASEADDR, EEM_SIZE);
	if (!eem_base_addr) {
		picachu_pr_notice("ioremap eem_base_addr failed!\n");
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
