/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <mt-plat/aee.h>
#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/io.h>

struct dram_console_buffer {
	uint32_t sig;
	uint32_t start;
	uint32_t size;

	uint8_t data[0];
};

#define RAM_CONSOLE_SIG (0x43474244)	/* DBGC */

static char *ram_console_old_log;
static size_t ram_console_old_log_size;

static struct dram_console_buffer *dram_console_buffer;
static size_t dram_console_buffer_size;

static void dram_console_write(struct console *console, const char *msg, unsigned int count)
{
	struct dram_console_buffer *buffer = dram_console_buffer;
	int rem;

	/* count >= buffer_size, full the buffer */
	if (count >= dram_console_buffer_size) {
		memcpy(buffer->data, msg + (count - dram_console_buffer_size),
		       dram_console_buffer_size);
		buffer->start = 0;
		buffer->size = dram_console_buffer_size;
	} else if (count > (dram_console_buffer_size - buffer->start)) {
		/* count > last buffer, full them and fill the head buffer */
		rem = dram_console_buffer_size - buffer->start;
		memcpy(buffer->data + buffer->start, msg, rem);
		memcpy(buffer->data, msg + rem, count - rem);
		buffer->start = count - rem;
		buffer->size = dram_console_buffer_size;
	} else {		/* count <=  last buffer, fill in free buffer */

		memcpy(buffer->data + buffer->start, msg, count);	/* count <= last buffer, fill them */
		buffer->start += count;
		buffer->size += count;
		if (buffer->start >= dram_console_buffer_size)
			buffer->start = 0;
		if (buffer->size > dram_console_buffer_size)
			buffer->size = dram_console_buffer_size;
	}
}

static struct console dram_console = {
	.name = "dram",
	.write = dram_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index = -1,
};

static void __init ram_console_save_old(const struct dram_console_buffer *buffer)
{

	ram_console_old_log = kmalloc(buffer->size, GFP_KERNEL);
	if (ram_console_old_log == NULL) {
		LOGE("ram_console: failed to allocate old buffer\n");
		return;
	}
	ram_console_old_log_size = buffer->size;

	memcpy(ram_console_old_log, &buffer->data[buffer->start], buffer->size - buffer->start);
	memcpy(ram_console_old_log + buffer->size - buffer->start, &buffer->data[0], buffer->start);
}

static int __init ram_console_init(struct dram_console_buffer *buffer, size_t buffer_size)
{
	dram_console_buffer = buffer;
	dram_console_buffer_size = buffer_size - sizeof(struct dram_console_buffer);

	if (buffer->sig == RAM_CONSOLE_SIG) {
		if (buffer->size > dram_console_buffer_size || buffer->start > buffer->size)
			LOGE("ram_console: found existing invalid buffer, size %d, start %d\n",
					buffer->size, buffer->start);
		else {
			LOGE("ram_console: found existing buffer, size %d, start %d\n",
					buffer->size, buffer->start);
			ram_console_save_old(buffer);
		}
	} else {
		LOGE("ram_console: no valid data in buffer " "(sig = 0x%08x)\n", buffer->sig);
	}
	memset(buffer, 0, buffer_size);
	buffer->sig = RAM_CONSOLE_SIG;

	register_console(&dram_console);

	return 0;
}

static void __init *remap_lowmem(phys_addr_t start, phys_addr_t size)
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
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;


	}

	return vaddr + offset_in_page(start);
}

static int __init dram_console_early_init(void)
{
	struct dram_console_buffer *bufp = NULL;
	size_t buffer_size = 0;

	bufp = (struct dram_console_buffer *)CONFIG_MTK_AEE_DRAM_CONSOLE_ADDR;
	buffer_size = CONFIG_MTK_AEE_DRAM_CONSOLE_SIZE;

	bufp = remap_lowmem(CONFIG_MTK_AEE_DRAM_CONSOLE_ADDR, CONFIG_MTK_AEE_DRAM_CONSOLE_SIZE);
	if (bufp == NULL) {
		LOGE("ioremap failed, no ram console available\n");
		return 0;
	}
	buffer_size = CONFIG_MTK_AEE_DRAM_CONSOLE_SIZE;

	LOGE("%s: start: 0x%p, size: %d\n", __func__, bufp, buffer_size);
	return ram_console_init(bufp, buffer_size);
}

static ssize_t dram_console_read_old(struct file *file, char __user *buf,
				     size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= ram_console_old_log_size)
		return 0;

	count = min(len, (size_t) (ram_console_old_log_size - pos));
	if (copy_to_user(buf, ram_console_old_log + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations ram_console_file_ops = {
	.owner = THIS_MODULE,
	.read = dram_console_read_old,
};

void dram_console_init(struct proc_dir_entry *root_entry)
{
	struct proc_dir_entry *entry;

	if (ram_console_old_log == NULL) {
		LOGE("%s: old log is null!\n", KBUILD_MODNAME);
		return;
	}

	entry = create_proc_entry("last_dram_kmsg", S_IFREG | S_IRUGO, root_entry);
	if (!entry) {
		LOGE("%s: failed to create proc entry\n", __func__);
		kfree(ram_console_old_log);
		ram_console_old_log = NULL;
		return;
	}

	entry->proc_fops = &ram_console_file_ops;
	entry->size = ram_console_old_log_size;
}

void dram_console_done(struct proc_dir_entry *aed_proc_dir)
{
	remove_proc_entry("last_dram_kmsg", aed_proc_dir);
}

void aee_dram_console_reserve_memory(void)
{
	memblock_reserve(CONFIG_MTK_AEE_DRAM_CONSOLE_ADDR, CONFIG_MTK_AEE_DRAM_CONSOLE_SIZE);
}

console_initcall(dram_console_early_init);
