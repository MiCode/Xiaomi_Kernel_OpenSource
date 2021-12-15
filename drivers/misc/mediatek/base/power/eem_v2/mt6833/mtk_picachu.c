/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>

#include "mtk_picachu.h" /* for picachu_reserve_mem_get_virt */
#include "mtk_picachu_reservedmem.h" /* for PICACHU_EEM_ID */

#define PICACHU_MEM_RESERVED_KEY "mediatek,PICACHU"
#endif

#define DEBUG

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

static phys_addr_t picachu_mem_base_phys;
static phys_addr_t picachu_mem_base_virt;
static phys_addr_t picachu_mem_size;


static void _cheak_aee_parameter(void);

static void dump_picachu_info(struct seq_file *m, struct picachu_info *info)
{
	void __iomem *addr_ptr;
	unsigned int sig;

	addr_ptr = (void __iomem *)picachu_reserve_mem_get_virt(PICACHU_EEM_ID);
	if (picachu_read(addr_ptr) & 0x1) {
		if (picachu_read(addr_ptr) & 0x2)
			seq_puts(m, "\nAging load (slt)\n");
		else
			seq_puts(m, "\nAging load\n");
	}

	/* Get signature */
	sig = (picachu_read(addr_ptr) >> PICACHU_SIGNATURE_SHIFT_BIT);
	sig = sig & 0xff;
	if (sig == PICACHU_SIG) {
		#define NR_FREQ 6
		#define NR_EEMSN_DET 3
		struct dvfs_vf_tbl {
			unsigned short pi_freq_tbl[NR_FREQ];
			unsigned char pi_volt_tbl[NR_FREQ];
			unsigned char pi_vf_num;
		};
		struct dvfs_vf_tbl (*vf_tbl_det)[NR_EEMSN_DET];
		int x, y;

		vf_tbl_det = addr_ptr + 0x4;
		for (x = 0; x < NR_EEMSN_DET; x++) {
			seq_printf(m, "%u\n",
				(*vf_tbl_det)[x].pi_vf_num);
			for (y = 0; y < NR_FREQ; y++)
				seq_printf(m, "%u ",
				    (*vf_tbl_det)[x].pi_volt_tbl[y]);
			seq_puts(m, "\n");
			for (y = 0; y < NR_FREQ; y++)
				seq_printf(m, "%u ",
				    (*vf_tbl_det)[x].pi_freq_tbl[y]);
			seq_puts(m, "\n");
		}
	}

#ifdef DEBUG
	_cheak_aee_parameter();
#endif

	return;
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

phys_addr_t picachu_reserve_mem_get_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[picachu] no reserve memory for 0x%x", id);
		return 0;
	} else
		return picachu_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(picachu_reserve_mem_get_phys);


phys_addr_t picachu_reserve_mem_get_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[picachu] no reserve memory for 0x%x", id);
		return 0;
	} else
		return picachu_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(picachu_reserve_mem_get_virt);


phys_addr_t picachu_reserve_mem_get_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[picachu] no reserve memory for 0x%x", id);
		return 0;
	} else
		return picachu_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(picachu_reserve_mem_get_size);


#ifdef CONFIG_OF_RESERVED_MEM
static int _picachu_reserve_memory_init(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;

	if (NUMS_MEM_ID == 0) {
		pr_info("[picachu] NUMS_MEM_ID is NULL\n");
		return 0;
	}

	if (picachu_mem_base_phys == 0) {
		pr_info("[picachu] picachu_mem_base_phys is NULL\n");
		return -1;
	}

	accumlate_memory_size = 0;
	picachu_mem_base_virt = (phys_addr_t)(uintptr_t)
			ioremap_wc(picachu_mem_base_phys, picachu_mem_size);

	pr_info("[picachu]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
			(unsigned long long)picachu_mem_base_virt,
			(unsigned long long)picachu_mem_base_virt +
			(unsigned long long)picachu_mem_size,
			(unsigned long long)picachu_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		picachu_reserve_mblock[id].start_virt = picachu_mem_base_virt +
							accumlate_memory_size;
		accumlate_memory_size += picachu_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or picachu_reserve_mblock does not match dts
	 */

	WARN_ON(accumlate_memory_size > picachu_mem_size);

#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_info("[picachu][mem_reserve-%d] ", id);
		pr_info("phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			(unsigned long long)picachu_reserve_mem_get_phys(id),
			(unsigned long long)picachu_reserve_mem_get_virt(id),
			(unsigned long long)picachu_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

static int __init picachu_reserve_mem_of_init(struct reserved_mem *rmem)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

	picachu_mem_base_phys = (phys_addr_t) rmem->base;
	picachu_mem_size = (phys_addr_t) rmem->size;

	pr_info("[picachu] phys:0x%llx - 0x%llx (0x%llx)\n",
		(unsigned long long)rmem->base,
		(unsigned long long)rmem->base + (unsigned long long)rmem->size,
		(unsigned long long)rmem->size);
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		picachu_reserve_mblock[id].start_phys = picachu_mem_base_phys +
							accumlate_memory_size;
		accumlate_memory_size += picachu_reserve_mblock[id].size;

		pr_info("[picachu][reserve_mem:%d]: ", id);
		pr_info("phys:0x%llx - 0x%llx (0x%llx)\n",
			picachu_reserve_mblock[id].start_phys,
			picachu_reserve_mblock[id].start_phys +
				picachu_reserve_mblock[id].size,
			picachu_reserve_mblock[id].size);
	}

	return 0;
}
RESERVEDMEM_OF_DECLARE(picachu_reservedmem,
	PICACHU_MEM_RESERVED_KEY,
	picachu_reserve_mem_of_init);


static void _cheak_aee_parameter(void)
{
	void __iomem *addr_ptr = NULL;
	unsigned int val = 0;
	unsigned char sig = 0;
	unsigned char efuse_major_ver = 0;
	unsigned char efuse_mirror_ver = 0;
	unsigned char db_major_ver = 0;
	unsigned char db_mirror_ver = 0;

	addr_ptr = (void __iomem *)picachu_reserve_mem_get_virt(PICACHU_AEE_ID);
	if (addr_ptr) {
		val = picachu_read(addr_ptr);
		/* Get signature */
		sig = val >> PICACHU_SIGNATURE_SHIFT_BIT;
		sig = sig & 0xff;
		if (sig == PICACHU_SIG) {
			val = val & 0x00FFFFFF;
			if (val & (0x1 << 0)) {
				aee_kernel_exception("PICACHU",
					"Error: picachu is disable via DOE");
				pr_info("[PICACHU] Error: picachu is disable via DOE");
			}
			if (val & (0x1 << 1)) {
				aee_kernel_exception("PICACHU",
					"Error: picachu para image not found");
				pr_info("[PICACHU] Error: picachu para image not found");
			}
			if (val & (0x1 << 2)) {
				aee_kernel_exception("PICACHU", "Error: use safe efuse");
				pr_info("[PICACHU] Error: use safe efuse");
			}
		} else {
			aee_kernel_exception("PICACHU", "Error: sig = %d", sig);
			pr_info("[PICACHU] Error: sig = %d", sig);
		}

		val = picachu_read(addr_ptr + 0x4);
		efuse_major_ver = (val & 0x000000FF);
		efuse_mirror_ver = (val & 0x0000FF00) >> 8;
		db_major_ver = (val & 0x00FF0000) >> 16;
		db_mirror_ver = (val & 0xFF000000) >> 24;

		pr_info("[PICACHU]fuse_major_ver=%d, efuse_mirror_ver=%d, db_major_ver=%d, db_mirror_ver=%d\n",
			efuse_major_ver,
			efuse_mirror_ver,
			db_major_ver,
			db_mirror_ver);

		if (efuse_major_ver > db_major_ver) {
			aee_kernel_exception("PICACHU",
			"Error:efuse_major_ver=%d > db_major_ver=%d, need to update DB",
			efuse_major_ver,
			db_major_ver);
			pr_info("[PICACHU] Error:efuse_major_ver=%d > db_major_ver=%d, need to update DB",
			efuse_major_ver,
			db_major_ver);
		}

		if (efuse_mirror_ver > db_mirror_ver) {
			aee_kernel_warning("PICACHU",
			"Warning:efuse_mirror_ver=%d > db_mirror_ver=%d",
			efuse_mirror_ver,
			db_mirror_ver);
			pr_info("PICACHU Warning:efuse_mirror_ver=%d > db_mirror_ver=%d",
			efuse_mirror_ver,
			db_mirror_ver);
		}
	}
}
#endif


static int __init picachu_init(void)
{
	create_procfs();

#ifdef CONFIG_OF_RESERVED_MEM
	_picachu_reserve_memory_init();
	_cheak_aee_parameter();
#endif

	return 0;
}


static void __exit picachu_exit(void)
{

}
subsys_initcall(picachu_init);
MODULE_DESCRIPTION("MediaTek Picachu Driver v0.1");
MODULE_LICENSE("GPL");
