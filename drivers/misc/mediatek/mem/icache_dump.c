/*
* Copyright (C) 2016 MediaTek Inc.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/io.h>
#include <linux/of_fdt.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <mach/mt_secure_api.h>

#define ICACHE_DUMP_PROC_ROOT_DIR		"icache_dump"
#define ICACHE_DUMP_PROC_RAW_FILE		"raw"

static unsigned long atf_icache_log_phy;
static unsigned long atf_icache_log_size;
static unsigned long dump_init_done;

struct cache_dump_header {
	u32 cpu_id;
	u32 way;
	u32 set;
	u32 tag_size;
	u32 line_size;
	u32 dump_size;
};

struct ca72_icache_dump {
	uint32_t ramidx;
	uint32_t tag[2];
	uint32_t data[16];
};

int mt_icache_dump(void)
{
#ifdef MTK_SIP_KERNEL_ICACHE_DUMP

	if (dump_init_done != 0x1)
		return 0;

	return mt_secure_call(MTK_SIP_KERNEL_ICACHE_DUMP,
			      atf_icache_log_phy,
			      atf_icache_log_size,
			      0);
#else
	return 0;
#endif
}

void mt_icache_dump_clear(void)
{
	unsigned long *dump_buf = NULL;
	u32 *ptr, *end;

	if (dump_init_done != 0x1) {
		pr_err("dump doesn't init\n");
		goto out;
	}

	dump_buf = ioremap_wc(atf_icache_log_phy, atf_icache_log_size);

	if (!dump_buf) {
		pr_err("can't map\n");
		goto out;
	}

	pr_err("icache dump: addr=0x%lx, size=0x%lx, map=%p\n",
	       atf_icache_log_phy,
	       atf_icache_log_size,
	       dump_buf);

	ptr = (u32 *)dump_buf;
	end = ptr + atf_icache_log_size / sizeof(u32);

	while (ptr < end) {
		*ptr = 0x0;
		ptr++;
	}
out:
	if (dump_buf) {
		pr_err("unmap cache buffer\n");
		iounmap(dump_buf);
	}
}

static int icache_dump_show(struct seq_file *m, void *v)
{
	char str[256];
	struct cache_dump_header *header = NULL;
	struct ca72_icache_dump *dump = NULL;
	unsigned long *dump_buf = NULL;

	/* 32 bit pointer */
	u32 *ptr, *end, *magic;

	snprintf(str, sizeof(str),
		 "============== Cache Dump ==============\n");
	seq_write(m, str, strlen(str));

	if (dump_init_done != 0x1) {
		snprintf(str, sizeof(str), "Init unfinish\n");
		seq_write(m, str, strlen(str));
		goto out;
	}

	dump_buf = ioremap_wc(atf_icache_log_phy, atf_icache_log_size);
	ptr = (u32 *)dump_buf;
	header = (struct cache_dump_header *)ptr;

	if (!dump_buf) {
		pr_err("can't map");
		goto out_ioumap;
	}

	pr_err("icache dump: addr=0x%lx, size=0x%lx, map=%p\n",
	       atf_icache_log_phy,
	       atf_icache_log_size,
	       dump_buf);

	switch (header->cpu_id) {
	case 0xD08:  /* cortex ca72 */
		break;
	default:
		snprintf(str, sizeof(str), "empty or cpu id is not support\n");
		seq_write(m, str, strlen(str));
		goto out;
	}

	snprintf(str, sizeof(str), "cpuid=0x%x, set=0x%x, way=0x%x\n",
		 header->cpu_id, header->set, header->way);

	seq_write(m, str, strlen(str));
	snprintf(str, sizeof(str),
		 "tag_size=0x%x, line_size=0x%x, dump_size=0x%x\n",
		 header->tag_size,
		 header->line_size,
		 header->dump_size);
	seq_write(m, str, strlen(str));

	/*
	 * +--------+-------------+------------------+
	 * | header | dump buffer | end magic number |
	 * +--------+-------------+------------------+
	 */


	/* checking the end magic number */
	end = ptr + (header->dump_size)/sizeof(u32);
	magic = end - 2;

	pr_err("magic:0x%x, 0x%x\n", *magic, *(magic + 1));

	if (*magic != 0x12357BD ||
	    *(magic + 1) != ~*magic) {
		snprintf(str, sizeof(str), "magic number incorrect\n");
		seq_write(m, str, strlen(str));
		goto out;
	}

	ptr += sizeof(struct cache_dump_header) / sizeof(u32);
	dump = (struct ca72_icache_dump *)ptr;
	/*
	 * [way][set][tag1][tag0] | [data1][data0][data3][data2] ..
	 * 012345678901234567890123456789012345678901234567890123456
	 */
	snprintf(str, sizeof(str),
		 "[way][set] [tag0] [tag1]: %s\n",
		 "[data0] [data1] [data2] [data4] ...");

	seq_write(m, str, strlen(str));

	while ((unsigned long)dump < (unsigned long)magic) {
		u32 set, way, i;

		way = (dump->ramidx >> 18) & 0x3;
		set = (dump->ramidx >> 6) & 0xFF;
		snprintf(str, sizeof(str),
			 "[0x%02x][0x%03x] 0x%08x 0x%08x:",
			 way, set, dump->tag[0], dump->tag[1]);

		seq_write(m, str, strlen(str));

		for (i = 0; i < (sizeof(dump->data) / sizeof(u32)); i += 2) {
			snprintf(str, sizeof(str),
				 " 0x%08x 0x%08x",
				 dump->data[i], dump->data[i + 1]);
			seq_write(m, str, strlen(str));
		}

		snprintf(str, sizeof(str), "\n");
		seq_write(m, str, strlen(str));
		dump++;
	}
#if 0 /* fs read twice cause unknown error */
	pr_err("chear header\n");
	memset(header, 0x00, sizeof(struct cache_dump_header));
#endif
	snprintf(str, sizeof(str), "============== END ==============\n");
	seq_write(m, str, strlen(str));

out:

out_ioumap:
	if (dump_buf) {
		pr_err("unmap cache buffer\n");
		iounmap(dump_buf);
	}

	return 0;
}

static int icache_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, icache_dump_show, NULL);
}

static ssize_t icache_dump_write(struct file *file,
				 const char __user *ubuf,
				 size_t count,
				 loff_t *ppos)
{
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	pr_err("%s\n", __func__);

	if (copy_from_user(&buf,
			   ubuf,
			   min_t(size_t, sizeof(buf) - 1, count))) {
		pr_err("copy from user fail\n");
		return -EFAULT;
	}

	pr_err("%s%s\n", __func__, buf);

	if (!strncmp(buf, "test", 4)) {
		pr_err("force i cache dump test....");
		mt_icache_dump();
		pr_err("finish\n");
	}

	if (!strncmp(buf, "clear", 5)) {
		pr_err("clear i cache dump buffer\n");
		mt_icache_dump_clear();
		pr_err("finish\n");
	}

	pr_err("exit\n");
	return count;

}

static const struct file_operations proc_icache_dump_fops = {
	.owner = THIS_MODULE,
	.open = icache_dump_open,
	.write = icache_dump_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct proc_dir_entry *icache_dump_proc_dir;
static struct proc_dir_entry *icache_dump_raw_proc_file;

static int __init icache_dump_init(void)
{
	struct device_node *node;
	u32 reg[4];

	node = of_find_compatible_node(
		NULL,
		NULL,
		"mediatek,cache-dump-memory");
	if (!node) {
		pr_err("can't find compatible node\n");
		goto dt_err;
	}

	if (of_property_read_u32_array(node, "reg", reg, 4)) {
		pr_err("get data fail...\n");
		goto dt_err;
	}

	atf_icache_log_phy = reg[1];
	atf_icache_log_size = reg[3];

	pr_err("icache dump: addr=0x%lx, size=0x%lx\n",
	       atf_icache_log_phy,
	       atf_icache_log_size);

	icache_dump_proc_dir = proc_mkdir(ICACHE_DUMP_PROC_ROOT_DIR, NULL);
	if (!icache_dump_proc_dir) {
		pr_err("%s:%d: %s proc_mkdir failed\n",
		       __func__,
		       __LINE__,
		       ICACHE_DUMP_PROC_ROOT_DIR);
		goto root_proc_dir_err;
	}

	icache_dump_raw_proc_file =
		proc_create(ICACHE_DUMP_PROC_RAW_FILE, 0444,
			    icache_dump_proc_dir, &proc_icache_dump_fops);

	if (!icache_dump_raw_proc_file) {
		pr_err("%s:%d:%s/%s proc_create failed\n",
		       __func__,
		       __LINE__,
		       ICACHE_DUMP_PROC_ROOT_DIR,
		       ICACHE_DUMP_PROC_RAW_FILE);
		goto raw_proc_file_err;
	}

	dump_init_done  = 0x1;
	return 0;

raw_proc_file_err:
	remove_proc_entry(ICACHE_DUMP_PROC_ROOT_DIR, icache_dump_proc_dir);

root_proc_dir_err:
dt_err:
	dump_init_done  = 0x0;

	return -1;
}

static void __exit icache_dump_exit(void)
{
	remove_proc_entry(ICACHE_DUMP_PROC_RAW_FILE, icache_dump_proc_dir);
	remove_proc_entry(ICACHE_DUMP_PROC_ROOT_DIR, NULL);
}

module_init(icache_dump_init);
module_exit(icache_dump_exit);
