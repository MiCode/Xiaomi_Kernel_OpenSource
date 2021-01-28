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
#include <mtk_picachu.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <mt-plat/aee.h>
#endif


/*
 * Little cluster: L = 2, CCI = 2
 * Big cluster: Big = 2
 */
#define NR_EEM_EFUSE_PER_VPROC		(4)
#define PICACHU_SIG			(0xA5)
#define PICACHU_SIGNATURE_SHIFT_BIT     (24)

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

#define PICACHU_PROC_ENTRY_ATTR (0440)

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

static struct picachu_info picachu_data[NR_PICACHU_VPROC];

static struct picachu_proc picachu_proc_list[] = {
	{"little", MT_PICACHU_L_VPROC, PICACHU_PROC_ENTRY_ATTR},
	{"big", MT_PICACHU_B_VPROC, PICACHU_PROC_ENTRY_ATTR},
	{0},
};

phys_addr_t picachu_mem_base_phys;
phys_addr_t picachu_mem_size;
phys_addr_t picachu_mem_base_virt;

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#define PICACHU_LOG_RESERVED_MEMORY_KEY "mediatek,PICACHU"
#endif

#ifdef CONFIG_OF_RESERVED_MEM

int picachu_log_reserved_mem_of_init(struct reserved_mem *rmem)
{

	picachu_mem_base_phys = rmem->base;
	picachu_mem_size = rmem->size;
	pr_notice("%s, start: %llx, size: %llx\n",
		PICACHU_LOG_RESERVED_MEMORY_KEY,
		picachu_mem_base_phys, picachu_mem_size);
	return 0;
}

RESERVEDMEM_OF_DECLARE(picachu_log_reserved_init,
		PICACHU_LOG_RESERVED_MEMORY_KEY,
		picachu_log_reserved_mem_of_init);
#endif


phys_addr_t picachu_reserve_mem_get_phys(enum picachu_log_buffer_id id)
{
	unsigned int offset = 0;
	int i;

	if (picachu_mem_base_virt == 0) {
		pr_notice("[picachu] %s: No picachu buffer found.\n", __func__);
		return 0;
	}

	if (id < 0 || id >= (sizeof(picachu_log_buffer_size)/sizeof(unsigned int))) {
		pr_notice("[picachu] %s: id not right.\n", __func__);
		return 0;
	}

	for (i = 0; i < id; i++)
		offset += picachu_log_buffer_size[i];

	return picachu_mem_base_phys+offset;
}
EXPORT_SYMBOL_GPL(picachu_reserve_mem_get_phys);

phys_addr_t picachu_reserve_mem_get_virt(enum picachu_log_buffer_id id)
{
	unsigned int offset = 0;
	int i;

	if (picachu_mem_base_virt == 0) {
		pr_notice("[picachu] %s: No picachu buffer found.\n", __func__);
		return 0;
	}

	if (id < 0 || id >= (sizeof(picachu_log_buffer_size)/sizeof(unsigned int))) {
		pr_notice("[picachu] %s: id not right.\n", __func__);
		return 0;
	}

	for (i = 0; i < id; i++)
		offset += picachu_log_buffer_size[i];

	return picachu_mem_base_virt+offset;
}
EXPORT_SYMBOL_GPL(picachu_reserve_mem_get_virt);

phys_addr_t picachu_reserve_mem_get_size(enum picachu_log_buffer_id id)
{
	if (picachu_mem_base_virt == 0) {
		pr_notice("[picachu] %s: No picachu buffer found.\n", __func__);
		return 0;
	}

	if (id < 0 || id >= (sizeof(picachu_log_buffer_size)/sizeof(unsigned int))) {
		pr_notice("[picachu] %s: id not right.\n", __func__);
		return 0;
	}

	return picachu_log_buffer_size[id];
}
EXPORT_SYMBOL_GPL(picachu_reserve_mem_get_size);


#define EEM_PTPSPARE0		0x11278F20
static void get_picachu_mem_addr(void)
{
	picachu_mem_base_virt = 0;
	if ((void __iomem *)picachu_mem_base_phys != NULL) {
		unsigned int offset = 0;
		int i;

		for (i = 0; i < sizeof(picachu_log_buffer_size)/sizeof(unsigned int); i++)
			offset += picachu_log_buffer_size[i];

		if (offset > picachu_mem_size) {
			pr_notice("[picachu] %s: total log buffer size is too large.\n", __func__);
		} else {
			picachu_mem_base_virt =
				(phys_addr_t)(uintptr_t)ioremap_wc(
				picachu_mem_base_phys,
				picachu_mem_size);
		}
	}
	picachu_pr_notice("[picachu] phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)picachu_mem_base_phys,
		(unsigned long long)picachu_mem_size,
		(unsigned long long)picachu_mem_base_virt);
}

#define MCUCFG_SPARE_REG	0x0C53FFEC
static void dump_picachu_info(struct seq_file *m, struct picachu_info *info)
{
	unsigned int val;
	void __iomem *addr_ptr;
#if 1
	unsigned int i, cnt, sig;

	if ((void __iomem *)picachu_mem_base_virt != NULL) {
		/* 0x60000 was reserved for eem efuse using */
		addr_ptr = (void __iomem *)(picachu_mem_base_virt+0x60000);
		if (picachu_read(addr_ptr)&0x1) {
			if (picachu_read(addr_ptr)&0x2)
				seq_puts(m, "\nAging load (slt)\n");
			else
				seq_puts(m, "\nAging load\n");
		}

		/* Get signature */
		sig = (picachu_read(addr_ptr) >> PICACHU_SIGNATURE_SHIFT_BIT);
		sig = sig & 0xff;
		if (sig == PICACHU_SIG) {
			cnt = (picachu_read(addr_ptr) & 0xff00)>>8;
			seq_printf(m, "0x%X\n", cnt);
			addr_ptr += 4;
			for (i = 0; i < cnt; i++, addr_ptr += 4) {
				val = picachu_read(addr_ptr);
				seq_printf(m, "%d:0x%X\n", i, val);
			}
		}
	}
#endif
	addr_ptr = ioremap(MCUCFG_SPARE_REG, 0);
	val = picachu_read(addr_ptr);
	seq_printf(m, "\nAging counter value: 0x%08x\n", val);

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

static int __init picachu_init(void)
{
	create_procfs();
	get_picachu_mem_addr();

	return 0;
}

static void __exit picachu_exit(void)
{
}

subsys_initcall(picachu_init);

MODULE_DESCRIPTION("MediaTek Picachu Driver v0.1");
MODULE_LICENSE("GPL");
