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

#include "log_store_kernel.h"
/*#include <linux/syscalls.h>*/

static struct sram_log_header *sram_header;
static int sram_log_store_status = BUFF_NOT_READY;
static int dram_log_store_status = BUFF_NOT_READY;
static char *pbuff;
static struct pl_lk_log *dram_curlog_header;
static struct dram_buf_header *sram_dram_buff;
static bool early_log_disable;
struct proc_dir_entry *entry;


#define EXPDB_PATH "/dev/block/platform/bootdevice/by-name/expdb"

#define LOG_BLOCK_SIZE (512)

#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
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

void log_store_bootup(void)
{
	/* Boot up finish, don't save log to emmc in next boot.*/
	store_log_to_emmc_enable(false);
}

#ifndef MODULE
int set_emmc_config(int type, int value)
{
	int fd;
	mm_segment_t fs;
	struct log_emmc_header pEmmc;
	int file_size;

	if (type >= EMMC_STORE_TYPE_NR || type < 0) {
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
	ksys_lseek(fd, file_size - LOG_BLOCK_SIZE, 0);
	ksys_read(fd, (char *)&pEmmc, sizeof(struct log_emmc_header));
	if (pEmmc.sig != LOG_EMMC_SIG) {
		pr_notice("log_store emmc header error.\n");
		ksys_close(fd);
		set_fs(fs);
		return -1;
	}
	if (type == UART_LOG) {
		if (value)
			pEmmc.uart_flag = 1;
		else
			pEmmc.uart_flag = 2;
	} else
		pEmmc.reserve[type - 1] = value;
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
	ksys_lseek(fd, file_size - LOG_BLOCK_SIZE, 0);
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
#endif
#endif

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
	if (!dram_curlog_header || !pbuff) {
		seq_puts(m, "log buff is null.\n");
		return 0;
	}

	seq_printf(m, "show buff sig 0x%x, size 0x%x,pl size 0x%x, lk size 0x%x!\n",
			dram_curlog_header->sig, dram_curlog_header->buff_size,
			dram_curlog_header->sz_pl, dram_curlog_header->sz_lk);

	if (dram_log_store_status == BUFF_READY)
		if (dram_curlog_header->buff_size >= (dram_curlog_header->off_pl
		+ dram_curlog_header->sz_pl
		+ dram_curlog_header->sz_lk))
			seq_write(m, pbuff+dram_curlog_header->off_pl,
				dram_curlog_header->sz_lk
				+ dram_curlog_header->sz_pl);
	log_store_bootup();
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
	if (!sram_dram_buff) {
		pr_notice("log_store: sram header is null.\n");
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
	log_buf = virt_to_phys(buff);
	size = log_buf_len_get();
	sram_dram_buff->klog_addr = log_buf;
	sram_dram_buff->klog_size = size;
	if (!early_log_disable)
		sram_dram_buff->flag |= BUFF_EARLY_PRINTK;
	pr_notice("log_store printk_buff addr:0x%x,sz:0x%x,buff-flag:0x%x.\n",
		sram_dram_buff->klog_addr,
		sram_dram_buff->klog_size,
		sram_dram_buff->flag);
}
#endif

#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
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
#endif

/* store log_store information to */
static int __init log_store_early_init(void)
{

#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
	/*pr_notice("log_store: sram header is null.\n");*/
	sram_header = ioremap_wc(CONFIG_MTK_DRAM_LOG_STORE_ADDR,
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

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek LogStore Driver");
MODULE_AUTHOR("MediaTek Inc.");
