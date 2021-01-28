// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

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
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <asm/memory.h>
#include <linux/of_fdt.h>
#include <linux/kmsg_dump.h>

#include "log_store_kernel.h"
#include "upmu_common.h"

static struct sram_log_header *sram_header;
static int sram_log_store_status = BUFF_NOT_READY;
static int dram_log_store_status = BUFF_NOT_READY;
static char *pbuff;
static struct pl_lk_log *dram_curlog_header;
static struct dram_buf_header *sram_dram_buff;
static bool early_log_disable;
struct proc_dir_entry *entry;
static u32 last_boot_phase;


#define EXPDB_PATH "/dev/block/by-name/expdb"

#define LOG_BLOCK_SIZE (512)
#define EXPDB_LOG_SIZE (2*1024*1024)

#ifdef CONFIG_MTK_PMIC_COMMON
u32 set_pmic_boot_phase(u32 boot_phase)
{
	u32 ret;

	boot_phase = boot_phase & BOOT_PHASE_MASK;
	ret = pmic_config_interface(0xA0E, boot_phase, BOOT_PHASE_MASK,
		PMIC_BOOT_PHASE_SHIFT);
	return ret;
}


u32 get_pmic_boot_phase(void)
{
	u32 value, ret;

	ret = pmic_read_interface(0xA0E, &value, BOOT_PHASE_MASK,
		PMIC_LAST_BOOT_PHASE_SHIFT);
	if (ret == 0)
		last_boot_phase = value;
	return value;
}
#endif

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

void set_boot_phase(u32 step)
{
	int fd;
	mm_segment_t fs;
	int file_size = 0;
	struct log_emmc_header pEmmc;

#ifdef CONFIG_MTK_PMIC_COMMON
	if (sram_header->reserve[SRAM_PMIC_BOOT_PHASE] == FLAG_ENABLE) {
		set_pmic_boot_phase(step);
		if (last_boot_phase == 0)
			get_pmic_boot_phase();
	}
#endif

	if ((sram_dram_buff->flag & NEED_SAVE_TO_EMMC) == NEED_SAVE_TO_EMMC) {
		pr_notice("log_store: set boot phase, last boot phase is %d.\n",
		last_boot_phase);
		pr_notice("log_store: not boot up, don't store log to expdb  ");
		return;
	}


	fs = get_fs();
	set_fs(get_ds());

	fd = ksys_open(EXPDB_PATH, O_RDWR, 0);
	if (fd < 0) {
		pr_notice("log_store can't open expdb file: %d.\n", fd);
		set_fs(fs);
		return;
	}

	file_size  = ksys_lseek(fd, 0, SEEK_END);
	ksys_lseek(fd, file_size - sram_header->reserve[1], 0);
	ksys_read(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	if (pEmmc.sig != LOG_EMMC_SIG) {
		pr_notice("log_store emmc header error, format it.\n");
		memset(&pEmmc, 0, sizeof(struct log_emmc_header));
		pEmmc.sig = LOG_EMMC_SIG;
	} else if (last_boot_phase == 0)
		// get last boot phase
		last_boot_phase = (pEmmc.reserve_flag[BOOT_STEP] >>
			LAST_BOOT_PHASE_SHIFT) & BOOT_PHASE_MASK;

	// clear now boot phase
	pEmmc.reserve_flag[BOOT_STEP] &= (BOOT_PHASE_MASK <<
		LAST_BOOT_PHASE_SHIFT);
	// set boot phase
	pEmmc.reserve_flag[BOOT_STEP] |= (step << NOW_BOOT_PHASE_SHIFT);
	ksys_lseek(fd, file_size - sram_header->reserve[1], 0);
	ksys_write(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	ksys_close(fd);
	set_fs(fs);
	pr_notice("log_store: set boot phase, last boot phase is %d.\n",
		last_boot_phase);
}

u32 get_last_boot_phase(void)
{
	return last_boot_phase;
}

void log_store_bootup(void)
{
	/* Boot up finish, don't save log to emmc in next boot.*/
	store_log_to_emmc_enable(false);
	set_boot_phase(BOOT_PHASE_ANDROID);
}

void log_store_to_emmc(void)
{
	int fd;
	mm_segment_t fs;
	char buff[LOG_BLOCK_SIZE];
	struct log_emmc_header pEmmc;
	struct kmsg_dumper dumper = { .active = true };
	size_t len = 0;
	int file_size, size = 0;
	struct emmc_log kernel_log_config;

	fs = get_fs();
	set_fs(get_ds());

	fd = ksys_open(EXPDB_PATH, O_RDWR, 0);
	if (fd < 0) {
		pr_notice("log_store can't open expdb file: %d.\n", fd);
		set_fs(fs);
		return;
	}

	file_size  = ksys_lseek(fd, 0, SEEK_END);
	ksys_lseek(fd, file_size - sram_header->reserve[1], 0);
	ksys_read(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	if (pEmmc.sig != LOG_EMMC_SIG) {
		pr_notice("log_store emmc header error, format it.\n");
		memset(&pEmmc, 0, sizeof(struct log_emmc_header));
		pEmmc.sig = LOG_EMMC_SIG;
	}
	kernel_log_config.start = pEmmc.offset;

	kmsg_dump_rewind_nolock(&dumper);
	memset(buff, 0, LOG_BLOCK_SIZE);
	while (kmsg_dump_get_line_nolock(&dumper, true, buff,
					 LOG_BLOCK_SIZE, &len)) {
		if (pEmmc.offset + len + LOG_BLOCK_SIZE > EXPDB_LOG_SIZE)
			pEmmc.offset = 0;
		ksys_lseek(fd, file_size - EXPDB_LOG_SIZE + pEmmc.offset, 0);
		size = ksys_write(fd, buff, len);
		if (size < 0)
			pr_notice_once("write expdb failed:%d.\n", size);
		else
			pEmmc.offset += size;
		memset(buff, 0, LOG_BLOCK_SIZE);
	}

	kernel_log_config.end = pEmmc.offset;
	kernel_log_config.type = LOG_LAST_KERNEL;
	size = file_size - sram_header->reserve[1] +
		sizeof(struct log_emmc_header) +
		pEmmc.reserve_flag[LOG_INDEX] * sizeof(struct emmc_log);
	ksys_lseek(fd, size, 0);
	ksys_write(fd, (char *)&kernel_log_config, sizeof(struct emmc_log));
	pEmmc.reserve_flag[LOG_INDEX] += 1;
	pEmmc.reserve_flag[LOG_INDEX] = pEmmc.reserve_flag[LOG_INDEX] %
		HEADER_INDEX_MAX;
	ksys_lseek(fd, file_size - sram_header->reserve[1], 0);
	ksys_write(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	ksys_close(fd);
	set_fs(fs);
	pr_notice("log_store write expdb done!\n");
}

int set_emmc_config(int type, int value)
{
	int fd;
	mm_segment_t fs;
	struct log_emmc_header pEmmc;
	int file_size;

	if (type >= EMMC_STORE_FLAG_TYPE_NR || type < 0) {
		pr_notice("invalid config type: %d.\n", type);
		return -1;
	}

	fs = get_fs();
	set_fs(get_ds());

	fd = ksys_open(EXPDB_PATH, O_RDWR, 0);
	if (fd < 0) {
		pr_notice("log_store can't open expdb file: %d.\n", fd);
		set_fs(fs);
		return -1;
	}

	file_size  = ksys_lseek(fd, 0, SEEK_END);
	ksys_lseek(fd, file_size - sram_header->reserve[1], 0);
	ksys_read(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	if (pEmmc.sig != LOG_EMMC_SIG) {
		pr_notice("log_store emmc header error.\n");
		ksys_close(fd);
		set_fs(fs);
		return -1;
	}

	if (type == UART_LOG || type == PRINTK_RATELIMIT ||
		type == KEDUMP_CTL) {
		if (value)
			pEmmc.reserve_flag[type] = FLAG_ENABLE;
		else
			pEmmc.reserve_flag[type] = FLAG_DISABLE;
	} else {
		pEmmc.reserve_flag[type] = value;
	}
	ksys_lseek(fd, file_size - LOG_BLOCK_SIZE, 0);
	ksys_write(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	ksys_close(fd);
	set_fs(fs);
	pr_notice("type:%d, value:%d.\n", type, value);
	return 0;
}

int read_emmc_config(struct log_emmc_header *log_header)
{
	int fd;
	mm_segment_t fs;
	int file_size;

	fs = get_fs();
	set_fs(get_ds());

	fd = ksys_open(EXPDB_PATH, O_RDWR, 0);
	if (fd < 0) {
		pr_notice("log_store can't open expdb file: %d.\n", fd);
		set_fs(fs);
		return -1;
	}

	file_size  = ksys_lseek(fd, 0, SEEK_END);
	ksys_lseek(fd, file_size - sram_header->reserve[1], 0);
	ksys_read(fd, (char *)log_header, sizeof(struct log_emmc_header));
	if (log_header->sig != LOG_EMMC_SIG) {
		pr_notice("log_store emmc header error.\n");
		ksys_close(fd);
		set_fs(fs);
		return -1;
	}
	ksys_close(fd);
	set_fs(fs);
	return 0;
}

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
			dram_curlog_header->sz_pl, dram_curlog_header->sz_lk, last_boot_phase);

	if (dram_log_store_status == BUFF_READY)
		if (dram_curlog_header->buff_size >= (dram_curlog_header->off_pl
		+ dram_curlog_header->sz_pl
		+ dram_curlog_header->sz_lk))
			seq_write(m, pbuff+dram_curlog_header->off_pl,
				dram_curlog_header->sz_lk
				+ dram_curlog_header->sz_pl);

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

static const struct file_operations pl_lk_file_ops = {
	.owner = THIS_MODULE,
	.open = pl_lk_file_open,
	.write = pl_lk_file_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int __init log_store_late_init(void)
{
	set_boot_phase(BOOT_PHASE_KERNEL);
	if (sram_dram_buff == NULL) {
		pr_notice("log_store: sram header DRAM buff is null.\n");
		dram_log_store_status = BUFF_ALLOC_ERROR;
		return -1;
	}

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

#ifndef MODULE
/* need mapping virtual address to phy address */
static void store_printk_buff(void)
{
	phys_addr_t log_buf;
	char *buff;
	int size;

	if (!sram_dram_buff) {
		pr_notice("log_store: sram_dram_buff is null.\n");
		return;
	}
	buff = log_buf_addr_get();
	log_buf = __pa_symbol(buff);
	size = log_buf_len_get();
	if ((log_buf & ~0XFFFFFFFF) == 0)
		sram_dram_buff->klog_addr = (u32)(log_buf & 0xffffffff);
	else
		sram_dram_buff->klog_addr = 0;
	sram_dram_buff->klog_size = size;
	if (!early_log_disable)
		sram_dram_buff->flag |= BUFF_EARLY_PRINTK;
	pr_notice("log_store printk_buff addr:0x%x,sz:0x%x,buff-flag:0x%x.\n",
		sram_dram_buff->klog_addr,
		sram_dram_buff->klog_size,
		sram_dram_buff->flag);
}
#endif

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


struct mem_desc_ls {
	unsigned int addr;
	unsigned int size;
};
static int __init dt_get_log_store(unsigned long node, const char *uname,
		int depth, void *data)
{
	struct mem_desc_ls *sram_ls;

	if (depth != 1 || (strcmp(uname, "chosen") != 0
			&& strcmp(uname, "chosen@0") != 0))
		return 0;
	sram_ls = (struct mem_desc_ls *) of_get_flat_dt_prop(node,
			"log_store", NULL);
	if (sram_ls) {
		pr_notice("log_store:[DT] log_store: 0x%x@0x%x\n",
				sram_ls->addr, sram_ls->size);
		*(struct mem_desc_ls *) data = *sram_ls;
	}
	return 1;
}
/* store log_store information to */
static int __init log_store_early_init(void)
{

#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
	struct mem_desc_ls sram_ls = { 0 };

	if (of_scan_flat_dt(dt_get_log_store, &sram_ls)) {
		pr_info("log_store: get ok, sram addr:0x%x, size:0x%x\n",
				sram_ls.addr, sram_ls.size);
	} else {
		pr_info("log_store: get fail\n");
	}

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

#ifndef MODULE
	/* store printk log buff information to DRAM */
	store_printk_buff();
#endif
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
	if (entry)
		proc_remove(entry);
}

module_init(log_store_early_init);
module_exit(log_store_exit);
#else
early_initcall(log_store_early_init);
late_initcall(log_store_late_init);
#endif
