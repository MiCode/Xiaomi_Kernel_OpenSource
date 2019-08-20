/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#include <linux/slab.h>

#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/pstore.h>
#include <linux/io.h>
extern struct pstore_info *psinfo;

#ifdef CONFIG_PSTORE
void pstore_console_show(enum pstore_type_id type_id, struct seq_file *m, void *v)
{
	struct pstore_info *psi = psinfo;
	char *buf = NULL;
	ssize_t size;
	u64 id;
	int count;
	enum pstore_type_id type;
	struct timespec time;
	bool compressed;

	if (!psi)
		return;
	mutex_lock(&psi->read_mutex);
	if (psi->open && psi->open(psi))
		goto out;

	while ((size = psi->read(&id, &type, &count, &time, &buf, &compressed, psi)) > 0) {
		/*pr_err("ram_console: id %lld, type %d, count %d, size %zx\n", id, type, count,
		       size);*/
		if (type == type_id)
			seq_write(m, buf, size);
		kfree(buf);
		buf = NULL;
	}

	if (psi->close)
		psi->close(psi);
out:
	mutex_unlock(&psi->read_mutex);
}
#endif

static int ram_console_lastk_show(struct seq_file *m, void *v)
{

	pr_err("ram_console_lastk_show\n");

#ifdef CONFIG_PSTORE_CONSOLE
	/*pr_err("ram_console: pstore show start\n");*/
	pstore_console_show(PSTORE_TYPE_CONSOLE, m, v);
	/*pr_err("ram_console: pstore show end\n");*/
#endif
	return 0;
}



static int ram_console_show(struct seq_file *m, void *v)
{
	ram_console_lastk_show(m, v);
	return 0;
}

static int ram_console_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console_show, inode->i_private);
}

static const struct file_operations ram_console_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init ram_console_late_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("last_kmsg", 0444, NULL, &ram_console_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");

		return 0;
	}
	return 0;
}


late_initcall(ram_console_late_init);

