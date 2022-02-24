// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/regmap.h>
#include <asm/memory.h>
#include <linux/of_fdt.h>
#include <linux/kmsg_dump.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include "log_store_kernel.h"

static struct sram_log_header *sram_header;
static int sram_log_store_status = BUFF_NOT_READY;
static int dram_log_store_status = BUFF_NOT_READY;
static char *pbuff;
static struct pl_lk_log *dram_curlog_header;
static struct dram_buf_header *sram_dram_buff;
static bool early_log_disable;
static struct proc_dir_entry *entry;
static u32 last_boot_phase = FLAG_INVALID;
static struct regmap *map;
static u32 pmic_addr;


#define LOG_BLOCK_SIZE (512)
#define EXPDB_LOG_SIZE (2*1024*1024)

bool get_pmic_interface(void)
{
	struct device_node *np;
	struct platform_device *pmic_pdev = NULL;
	unsigned int reg_val = 0;

	if (pmic_addr == 0)
		return false;

	np = of_find_node_by_name(NULL, "pmic");
	if (!np) {
		pr_err("log_store: pmic node not found.\n");
		return false;
	}

	pmic_pdev = of_find_device_by_node(np->child);
	if (!pmic_pdev) {
		pr_err("log_store: pmic child device not found.\n");
		return false;
	}

	/* get regmap */
	map = dev_get_regmap(pmic_pdev->dev.parent, NULL);
	if (!map) {
		pr_err("log_store:pmic regmap not found.\n");
		return false;
	}
	regmap_read(map, pmic_addr, &reg_val);
	pr_info("log_store:read pmic register value 0x%x.\n", reg_val);
	return true;

}
EXPORT_SYMBOL_GPL(get_pmic_interface);

u32 set_pmic_boot_phase(u32 boot_phase)
{
	unsigned int reg_val = 0, ret;

	if (!map) {
		if (get_pmic_interface() == false)
			return -1;
	}
	boot_phase = boot_phase & BOOT_PHASE_MASK;
	ret = regmap_read(map, pmic_addr, &reg_val);
	if (ret == 0) {
		reg_val = reg_val & (BOOT_PHASE_MASK << LAST_BOOT_PHASE_SHIFT);
		reg_val |= boot_phase;
		ret = regmap_write(map, pmic_addr, reg_val);
		pr_info("log_store: write pmic value 0x%x, ret 0x%x.\n", reg_val, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(set_pmic_boot_phase);

u32 get_pmic_boot_phase(void)
{
	unsigned int reg_val = 0, ret;

	if (!map) {
		if (get_pmic_interface() == false)
			return -1;
	}

	ret = regmap_read(map, pmic_addr, &reg_val);

	if (ret == 0) {
		reg_val = (reg_val >> LAST_BOOT_PHASE_SHIFT) & BOOT_PHASE_MASK;
		last_boot_phase = reg_val;
		return reg_val;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(get_pmic_boot_phase);

/* set the flag whether store log to emmc in next boot phase in pl */
void store_log_to_emmc_enable(bool value)
{

	if (!sram_dram_buff) {
		pr_notice("%s: sram dram buff is NULL.\n", __func__);
		return;
	}

	if (value) {
		sram_dram_buff->flag |= NEED_SAVE_TO_EMMC;
	} else {
		sram_dram_buff->flag &= ~NEED_SAVE_TO_EMMC;
		sram_header->reboot_count = 0;
		sram_header->save_to_emmc = 0;
	}

	pr_notice(
		"log_store: sram_dram_buff flag 0x%x, reboot count %d, %d.\n",
		sram_dram_buff->flag, sram_header->reboot_count,
		sram_header->save_to_emmc);
}
EXPORT_SYMBOL_GPL(store_log_to_emmc_enable);

void set_boot_phase(u32 step)
{

	if (sram_header->reserve[SRAM_PMIC_BOOT_PHASE] == FLAG_ENABLE) {
		set_pmic_boot_phase(step);
		if (last_boot_phase == 0)
			get_pmic_boot_phase();
	}

	sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] &= ~BOOT_PHASE_MASK;
	sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] |= step;
}
EXPORT_SYMBOL_GPL(set_boot_phase);

u32 get_last_boot_phase(void)
{
	return last_boot_phase;
}
EXPORT_SYMBOL_GPL(get_last_boot_phase);

void log_store_bootup(void)
{
	/* Boot up finish, don't save log to emmc in next boot.*/
	store_log_to_emmc_enable(false);
	set_boot_phase(BOOT_PHASE_ANDROID);
}
EXPORT_SYMBOL_GPL(log_store_bootup);

static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_notice("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

static int pl_lk_log_show(struct seq_file *m, void *v)
{
	if (dram_curlog_header == NULL || pbuff == NULL) {
		seq_puts(m, "log buff is null.\n");
		return 0;
	}

	seq_printf(m, "show buff sig 0x%x, size 0x%x,pl size 0x%x, lk size 0x%x, last_boot step 0x%x!\n",
			dram_curlog_header->sig, dram_curlog_header->buff_size,
			dram_curlog_header->sz_pl, dram_curlog_header->sz_lk,
			sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] ?
			sram_header->reserve[SRAM_HISTORY_BOOT_PHASE] : last_boot_phase);

	if (dram_log_store_status == BUFF_READY)
		if (dram_curlog_header->buff_size >= (dram_curlog_header->off_pl
		+ dram_curlog_header->sz_pl
		+ dram_curlog_header->sz_lk))
			seq_write(m, pbuff, dram_curlog_header->off_pl +
				dram_curlog_header->sz_lk + dram_curlog_header->sz_pl);

	return 0;
}


static int pl_lk_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, pl_lk_log_show, inode->i_private);
}

static ssize_t pl_lk_file_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	switch (val) {
	case 0:
		log_store_bootup();
		break;

	default:
		break;
	}
	return cnt;
}

static int logstore_pm_notify(struct notifier_block *notify_block,
	unsigned long mode, void *unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		set_boot_phase(BOOT_PHASE_PRE_SUSPEND);
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		set_boot_phase(BOOT_PHASE_EXIT_RESUME);
		break;
	}
	return 0;
}
static const struct proc_ops pl_lk_file_ops = {
	.proc_open = pl_lk_file_open,
	.proc_write = pl_lk_file_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

struct logstore_tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};
#define NORMAL_BOOT_MODE 0

unsigned int get_boot_mode_from_dts(void)
{
	struct device_node *np_chosen = NULL;
	struct logstore_tag_bootmode *tag = NULL;

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen) {
		pr_notice("log_store: warning: not find node: '/chosen'\n");

		np_chosen = of_find_node_by_path("/chosen@0");
		if (!np_chosen) {
			pr_notice("log_store: warning: not find node: '/chosen@0'\n");
			return NORMAL_BOOT_MODE;
		}
	}

	tag = (struct logstore_tag_bootmode *)
			of_get_property(np_chosen, "atag,boot", NULL);
	if (!tag) {
		pr_notice("log_store: error: not find tag: 'atag,boot';\n");
		return NORMAL_BOOT_MODE;
	}

	pr_notice("log_store: bootmode: 0x%x boottype: 0x%x.\n",
		tag->bootmode, tag->boottype);

	return tag->bootmode;
}

static int __init log_store_late_init(void)
{
	static struct notifier_block logstore_pm_nb;

	logstore_pm_nb.notifier_call = logstore_pm_notify;
	register_pm_notifier(&logstore_pm_nb);
	set_boot_phase(BOOT_PHASE_KERNEL);
	if (sram_dram_buff == NULL) {
		pr_notice("log_store: sram header DRAM buff is null.\n");
		dram_log_store_status = BUFF_ALLOC_ERROR;
		return -1;
	}

	if (get_boot_mode_from_dts() != NORMAL_BOOT_MODE)
		store_log_to_emmc_enable(false);

	if (!sram_dram_buff->buf_addr || !sram_dram_buff->buf_size) {
		pr_notice("log_store: DRAM buff is null.\n");
		dram_log_store_status = BUFF_ALLOC_ERROR;
		return -1;
	}
	pr_notice("log store:sram_dram_buff addr 0x%x, size 0x%x.\n",
		sram_dram_buff->buf_addr, sram_dram_buff->buf_size);

	pbuff = remap_lowmem(sram_dram_buff->buf_addr,
		sram_dram_buff->buf_size);
	pr_notice("[PHY layout]log_store_mem:0x%08llx-0x%08llx (0x%llx)\n",
			(unsigned long long)sram_dram_buff->buf_addr,
			(unsigned long long)sram_dram_buff->buf_addr
			+ sram_dram_buff->buf_size - 1,
			(unsigned long long)sram_dram_buff->buf_size);
	if (!pbuff) {
		pr_notice("log_store: ioremap_wc failed.\n");
		dram_log_store_status = BUFF_ERROR;
		return -1;
	}

/* check buff flag */
	if (dram_curlog_header->sig != LOG_STORE_SIG) {
		pr_notice("log store: log sig: 0x%x.\n",
			dram_curlog_header->sig);
		dram_log_store_status = BUFF_ERROR;
		return 0;
	}

	dram_log_store_status = BUFF_READY;
	pr_notice("buff %p, sig %x size %x pl %x, sz %x lk %x, sz %x p %x, l %x\n",
		pbuff, dram_curlog_header->sig,
		dram_curlog_header->buff_size,
		dram_curlog_header->off_pl, dram_curlog_header->sz_pl,
		dram_curlog_header->off_lk, dram_curlog_header->sz_lk,
		dram_curlog_header->pl_flag, dram_curlog_header->lk_flag);

	entry = proc_create("pl_lk", 0664, NULL, &pl_lk_file_ops);
	if (!entry) {
		pr_notice("log_store: failed to create proc entry\n");
		return 1;
	}

	return 0;
}


/* need mapping virtual address to phy address */
void store_printk_buff(void)
{
	phys_addr_t log_buf;
	char *buff;
	int size;

	if (!sram_dram_buff) {
		pr_notice("log_store: sram_dram_buff is null.\n");
		return;
	}
	buff = log_buf_addr_get();
	log_buf = __virt_to_phys_nodebug(buff);
	size = log_buf_len_get();
	/* support 32/64 bits */
#ifdef CONFIG_PHYS_ADDR_T_64BIT
	if ((log_buf >> 32) == 0)
		sram_dram_buff->klog_addr = (u32)(log_buf & 0xffffffff);
	else
		sram_dram_buff->klog_addr = 0;
#else
	sram_dram_buff->klog_addr = log_buf;
#endif
	sram_dram_buff->klog_size = size;
	if (!early_log_disable)
		sram_dram_buff->flag |= BUFF_EARLY_PRINTK;
	pr_notice("log_store printk_buff addr:0x%x,sz:0x%x,buff-flag:0x%x.\n",
		sram_dram_buff->klog_addr,
		sram_dram_buff->klog_size,
		sram_dram_buff->flag);
}
EXPORT_SYMBOL_GPL(store_printk_buff);

void disable_early_log(void)
{
	pr_notice("log_store: %s.\n", __func__);
	early_log_disable = true;
	if (!sram_dram_buff) {
		pr_notice("log_store: sram_dram_buff is null.\n");
		return;
	}

	sram_dram_buff->flag &= ~BUFF_EARLY_PRINTK;
}
EXPORT_SYMBOL_GPL(disable_early_log);

int dt_get_log_store(struct mem_desc_ls *data)
{
	struct mem_desc_ls *sram_ls;
	struct device_node *np_chosen, *np_logstore;

	np_logstore = of_find_node_by_name(NULL, "logstore");
	if (np_logstore) {
		of_property_read_u32(np_logstore, "pmic_register", &pmic_addr);
		pr_notice("log_store: get address 0x%x.\n", pmic_addr);
	} else {
		pr_err("log_store: can't get pmic address.\n");
	}

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen)
		np_chosen = of_find_node_by_path("/chosen@0");

	sram_ls = (struct mem_desc_ls *) of_get_property(np_chosen,
			"log_store", NULL);
	if (sram_ls) {
		pr_notice("log_store:[DT] log_store: 0x%x@0x%x\n",
				sram_ls->addr, sram_ls->size);
		*data = *sram_ls;
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(dt_get_log_store);

void *get_sram_header(void)
{
	if ((sram_header != NULL) && (sram_header->sig == SRAM_HEADER_SIG))
		return sram_header;
	return NULL;
}
EXPORT_SYMBOL_GPL(get_sram_header);

/* store log_store information to */
static int __init log_store_early_init(void)
{

#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
	struct mem_desc_ls sram_ls = { 0 };

	if (dt_get_log_store(&sram_ls))
		pr_info("log_store: get ok, sram addr:0x%x, size:0x%x\n",
				sram_ls.addr, sram_ls.size);
	else
		pr_info("log_store: get fail\n");

	sram_header = ioremap_wc(sram_ls.addr,
		CONFIG_MTK_DRAM_LOG_STORE_SIZE);
	dram_curlog_header = &(sram_header->dram_curlog_header);
#else
	pr_notice("log_store: not Found CONFIG_MTK_DRAM_LOG_STORE!\n");
	return -1;
#endif
	pr_notice("log_store: sram header address 0x%p.\n",
		sram_header);
	if (sram_header->sig != SRAM_HEADER_SIG) {
		pr_notice("log_store: sram header sig 0x%x.\n",
			sram_header->sig);
		sram_log_store_status = BUFF_ERROR;
		sram_header = NULL;
		return -1;
	}

	sram_dram_buff = &(sram_header->dram_buf);
	if (sram_dram_buff->sig != DRAM_HEADER_SIG) {
		pr_notice("log_store: sram header DRAM sig error");
		sram_log_store_status = BUFF_ERROR;
		sram_dram_buff = NULL;
		return -1;
	}


	/* store printk log buff information to DRAM */
	store_printk_buff();

	if (sram_header->reserve[1] == 0 ||
		sram_header->reserve[1] > EXPDB_LOG_SIZE)
		sram_header->reserve[1] = LOG_BLOCK_SIZE;

	pr_notice("sig 0x%x flag 0x%x add 0x%x size 0x%x offsize 0x%x point 0x%x\n",
		sram_dram_buff->sig, sram_dram_buff->flag,
		sram_dram_buff->buf_addr, sram_dram_buff->buf_size,
		sram_dram_buff->buf_offsize, sram_dram_buff->buf_point);

#ifdef MODULE
	log_store_late_init();
#endif

	return 0;
}

#ifdef MODULE
static void __exit log_store_exit(void)
{
	static struct notifier_block logstore_pm_nb;

	if (entry)
		proc_remove(entry);

	logstore_pm_nb.notifier_call = logstore_pm_notify;
	unregister_pm_notifier(&logstore_pm_nb);
}

module_init(log_store_early_init);
module_exit(log_store_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek LogStore Driver");
MODULE_AUTHOR("MediaTek Inc.");
#else
early_initcall(log_store_early_init);
late_initcall(log_store_late_init);
#endif
