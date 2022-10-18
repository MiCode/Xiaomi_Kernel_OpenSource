// SPDX-License-Identifier: GPL-2.0-only
/*
 * MTD Oops/Panic logger
 *
 * Copyright © 2007 Nokia Corporation. All rights reserved.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/kmsg_dump.h>
#include <linux/version.h>
#include <linux/reboot.h>
#include <linux/platform_device.h>
#include <generated/utsrelease.h>


struct pmsg_buffer_hdr {
	uint32_t    sig;
	atomic_t    start;
	atomic_t    size;
	uint8_t     data[0];
};

/* Maximum MTD partition size */
#define MTDOOPS_MAX_MTD_SIZE (16 * 1024 * 1024)

#define MTDOOPS_KERNMSG_MAGIC 0x5d005d00
#define MTDOOPS_HEADER_SIZE   8

/* Expand kmsg_dump_reason
enum kmsg_dump_reason {
	KMSG_DUMP_UNDEF,
	KMSG_DUMP_PANIC,
	KMSG_DUMP_OOPS,
	KMSG_DUMP_EMERG,
	KMSG_DUMP_SHUTDOWN,
	KMSG_DUMP_MAX
};
*/

enum mtd_dump_reason {
	MTD_DUMP_UNDEF,
	MTD_DUMP_PANIC,
	MTD_DUMP_OOPS,
	MTD_DUMP_EMERG,
	MTD_DUMP_SHUTDOWN,
	MTD_DUMP_RESTART,
	MTD_DUMP_POWEROFF,
	MTD_DUMP_LONG_PRESS,
	MTD_DUMP_MAX
};


static char *kdump_reason[8] = {
	"Unknown",
	"Kernel Panic",
	"Oops!",
	"Emerg",
	"Shut Down",
	"Restart",
	"PowerOff",
	"Long Press"
};


enum mtdoops_log_type {
	MTDOOPS_TYPE_UNDEF,
	MTDOOPS_TYPE_DMESG,
	MTDOOPS_TYPE_PMSG,
};
static char *log_type[4] = {
	"Unknown",
	"LAST KMSG",
	"LAST LOGCAT"
};

int g_long_press_reason = MTD_DUMP_LONG_PRESS;
EXPORT_SYMBOL_GPL(g_long_press_reason);

static unsigned long record_size = 4096;
static unsigned long lkmsg_record_size = 512 * 1024;
module_param(record_size, ulong, 0400);
MODULE_PARM_DESC(record_size,
		"record size for MTD OOPS pages in bytes (default 4096)");

static char mtddev[80];
module_param_string(mtddev, mtddev, 80, 0400);
MODULE_PARM_DESC(mtddev,
		"name or index number of the MTD device to use");

static int dump_oops = 0;
module_param(dump_oops, int, 0600);
MODULE_PARM_DESC(dump_oops,
		"set to 1 to dump oopses, 0 to only dump panics (default 1)");

#define MAX_CMDLINE_PARAM_LEN 64
static char build_fingerprint[MAX_CMDLINE_PARAM_LEN] = {0};
module_param_string(fingerprint, build_fingerprint, MAX_CMDLINE_PARAM_LEN,0644);


struct pmsg_platform_data {
	unsigned long	mem_size;
	phys_addr_t		mem_address;
	unsigned long	console_size;
	unsigned long	pmsg_size;
};

static struct mtdoops_context {
	struct kmsg_dumper dump;
	struct notifier_block reboot_nb;
	struct pmsg_platform_data pmsg_data;

	int mtd_index;
	struct work_struct work_erase;
	struct work_struct work_write;
	struct mtd_info *mtd;
	int oops_pages;
	int nextpage;
	int nextcount;
	unsigned long *oops_page_used;

	void *oops_buf;
} oops_cxt;

static void mark_page_used(struct mtdoops_context *cxt, int page)
{
	set_bit(page, cxt->oops_page_used);
}

static void mark_page_unused(struct mtdoops_context *cxt, int page)
{
	clear_bit(page, cxt->oops_page_used);
}

static int page_is_used(struct mtdoops_context *cxt, int page)
{
	return test_bit(page, cxt->oops_page_used);
}

static int mtdoops_erase_block(struct mtdoops_context *cxt, int offset)
{
	struct mtd_info *mtd = cxt->mtd;
	u32 start_page_offset = mtd_div_by_eb(offset, mtd) * mtd->erasesize;
	u32 start_page = start_page_offset / record_size;
	u32 erase_pages = mtd->erasesize / record_size;
	struct erase_info erase;
	int ret;
	int page;

	erase.addr = offset;
	erase.len = mtd->erasesize;

	ret = mtd_erase(mtd, &erase);
	if (ret) {
		printk(KERN_WARNING "mtdoops: erase of region [0x%llx, 0x%llx] on \"%s\" failed\n",
		       (unsigned long long)erase.addr,
		       (unsigned long long)erase.len, mtddev);
		return ret;
	}

	/* Mark pages as unused */
	for (page = start_page; page < start_page + erase_pages; page++)
		mark_page_unused(cxt, page);

	return 0;
}

static void mtdoops_inc_counter(struct mtdoops_context *cxt)
{
	cxt->nextpage++;
	if (cxt->nextpage >= cxt->oops_pages)
		cxt->nextpage = 0;
	cxt->nextcount++;
	if (cxt->nextcount == 0xffffffff)
		cxt->nextcount = 0;

	printk(KERN_ERR "mtdoops: ready nextpage= %d, nextcount= %d (no erase), oops_page_used_flag = %lx",
	       cxt->nextpage, cxt->nextcount, *cxt->oops_page_used );

	if (page_is_used(cxt, cxt->nextpage)) {
		schedule_work(&cxt->work_erase);
		return;
	}

}

/* Scheduled work - when we can't proceed without erasing a block */
static void mtdoops_workfunc_erase(struct work_struct *work)
{
	struct mtdoops_context *cxt =
			container_of(work, struct mtdoops_context, work_erase);
	struct mtd_info *mtd = cxt->mtd;
	int i = 0, j, ret, mod;

	/* We were unregistered */
	if (!mtd)
		return;

	mod = (cxt->nextpage * record_size) % mtd->erasesize;
	if (mod != 0) {
		cxt->nextpage = cxt->nextpage + ((mtd->erasesize - mod) / record_size);
		if (cxt->nextpage >= cxt->oops_pages)
			cxt->nextpage = 0;
	}

	while ((ret = mtd_block_isbad(mtd, cxt->nextpage * record_size)) > 0) {
badblock:
		printk(KERN_WARNING "mtdoops: bad block at %08lx\n",
		       cxt->nextpage * record_size);
		i++;
		cxt->nextpage = cxt->nextpage + (mtd->erasesize / record_size);
		if (cxt->nextpage >= cxt->oops_pages)
			cxt->nextpage = 0;
		if (i == cxt->oops_pages / (mtd->erasesize / record_size)) {
			printk(KERN_ERR "mtdoops: all blocks bad!\n");
			return;
		}
	}

	if (ret < 0) {
		printk(KERN_ERR "mtdoops: mtd_block_isbad failed, aborting\n");
		return;
	}

	for (j = 0, ret = -1; (j < 3) && (ret < 0); j++)
		ret = mtdoops_erase_block(cxt, cxt->nextpage * record_size);

	if (ret >= 0) {
		printk(KERN_DEBUG "mtdoops: ready %d, %d\n",
		       cxt->nextpage, cxt->nextcount);
		return;
	}

	if (ret == -EIO) {
		ret = mtd_block_markbad(mtd, cxt->nextpage * record_size);
		if (ret < 0 && ret != -EOPNOTSUPP) {
			printk(KERN_ERR "mtdoops: block_markbad failed, aborting\n");
			return;
		}
	}
	goto badblock;
}

static void mtdoops_write(struct mtdoops_context *cxt, int panic)
{
	struct mtd_info *mtd = cxt->mtd;
	size_t retlen;
	u32 *hdr;
	int ret;

	/* Add mtdoops header to the buffer */
	hdr = cxt->oops_buf;
	hdr[0] = cxt->nextcount;
	hdr[1] = MTDOOPS_KERNMSG_MAGIC;

	if (panic) {
		ret = mtd_panic_write(mtd, cxt->nextpage * record_size,
				      record_size, &retlen, cxt->oops_buf);
		if (ret == -EOPNOTSUPP) {
			printk(KERN_ERR "mtdoops: Cannot write from panic without panic_write\n");
			return;
		}
	} else
		ret = mtd_write(mtd, cxt->nextpage * record_size,
				record_size, &retlen, cxt->oops_buf);

	if (retlen != record_size || ret < 0)
		printk(KERN_ERR "mtdoops: write failure at %ld (%td of %ld written), error %d\n",
		       cxt->nextpage * record_size, retlen, record_size, ret);
	mark_page_used(cxt, cxt->nextpage);

// device will reboot ,so do not need find next and erase
//	if (!panic)
//	mtdoops_inc_counter(cxt);
}

static void mtdoops_workfunc_write(struct work_struct *work)
{
	struct mtdoops_context *cxt =
			container_of(work, struct mtdoops_context, work_write);

	mtdoops_write(cxt, 0);
}

static void find_next_position(struct mtdoops_context *cxt)
{
	struct mtd_info *mtd = cxt->mtd;
	int ret, page, maxpos = 0;
	u32 count[2], maxcount = 0xffffffff;
	size_t retlen;

	for (page = 0; page < cxt->oops_pages; page++) {
		if (mtd_block_isbad(mtd, page * record_size))
			continue;
		/* Assume the page is used */
		mark_page_used(cxt, page);
		ret = mtd_read(mtd, page * record_size, MTDOOPS_HEADER_SIZE,
			       &retlen, (u_char *)&count[0]);
		if (retlen != MTDOOPS_HEADER_SIZE ||
				(ret < 0 && !mtd_is_bitflip(ret))) {
			printk(KERN_ERR "mtdoops: read failure at %ld (%td of %d read), err %d\n",
			       page * record_size, retlen,
			       MTDOOPS_HEADER_SIZE, ret);
			continue;
		}

		if (count[0] == 0xffffffff && count[1] == 0xffffffff)
			mark_page_unused(cxt, page);
		if (count[0] == 0xffffffff || count[1] != MTDOOPS_KERNMSG_MAGIC)
			continue;
		if (maxcount == 0xffffffff) {
			maxcount = count[0];
			maxpos = page;
		} else if (count[0] < 0x40000000 && maxcount > 0xc0000000) {
			maxcount = count[0];
			maxpos = page;
		} else if (count[0] > maxcount && count[0] < 0xc0000000) {
			maxcount = count[0];
			maxpos = page;
		} else if (count[0] > maxcount && count[0] > 0xc0000000
					&& maxcount > 0x80000000) {
			maxcount = count[0];
			maxpos = page;
		}
	}
	if (maxcount == 0xffffffff) {
		cxt->nextpage = cxt->oops_pages - 1;
		cxt->nextcount = 0;
	}
	else {
		cxt->nextpage = maxpos;
		cxt->nextcount = maxcount;
	}

	mtdoops_inc_counter(cxt);
}

static void mtdoops_add_reason(char *oops_buf, enum mtd_dump_reason reason, enum mtdoops_log_type type, int index, int nextpage)
{
	char str_buf[200] = {0};
	int ret_len = 0;
	struct timespec64 now;
	struct tm ts;

	ktime_get_coarse_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &ts);

	if (nextpage > 1)
		ret_len =  snprintf(str_buf, 200,
				"\n```\n## Index: %d\t\n### Build: %s\t\n## REASON: %s\n#### LOG TYPE:%s\n#####%04ld-%02d-%02d %02d:%02d:%02d\t\n```c\t\n",
				index, build_fingerprint, kdump_reason[reason], log_type[type], ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);
	else
		ret_len =  snprintf(str_buf, 200,
				"\n\n## Index: %d\t\n### Build: %s\t\n## REASON: %s\n#### LOG TYPE: %s\n#####%04ld-%02d-%02d %02d:%02d:%02d\t\n```c\t\n",
				index, build_fingerprint, kdump_reason[reason], log_type[type], ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);

	memcpy(oops_buf, str_buf, ret_len);
}


static void mtdoops_add_pmsg_head(char *oops_buf, enum mtdoops_log_type type)
{
	char str_buf[80] = {0};
	int ret_len = 0;
	struct timespec64 now;
	struct tm ts;

	ktime_get_coarse_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &ts);

	ret_len =  snprintf(str_buf, 80,
			"\n```\n#### LOG TYPE:%s\n#####%04ld-%02d-%02d %02d:%02d:%02d\t\n```c\t\n",
			log_type[type], ts.tm_year+1900, ts.tm_mon, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);

	memcpy(oops_buf, str_buf, ret_len);
}

static void mtdoops_do_dump(struct kmsg_dumper *dumper,
				enum mtd_dump_reason reason)
{
	static int do_dump_count = 0;
	struct mtdoops_context *cxt = container_of(dumper,
			struct mtdoops_context, dump);
	size_t ret_len = 0;
	char *pmsg_buffer_start = NULL;
	struct pmsg_buffer_hdr *p_hdr = NULL;
	int j, ret;

	if(cxt->mtd == NULL) {
		printk(KERN_ERR "mtdoops: init is not finish. Cannot write mtd logs \n");
		return;
	}

	do_dump_count++;
	printk(KERN_ERR "mtdoops: %s start , count = %d , page = %d, reason = %d, dump_count = %d\n",__func__,cxt->nextcount, cxt->nextpage,reason,do_dump_count);

	if(do_dump_count>1)
	{
		for (j = 0, ret = -1; (j < 3) && (ret < 0); j++)
			ret = mtdoops_erase_block(cxt, cxt->nextpage * record_size);
	}

	/* Only dump oopses if dump_oops is set */
	if (reason == KMSG_DUMP_OOPS && !dump_oops)
		return;

	kmsg_dump_get_buffer(dumper, true, cxt->oops_buf + MTDOOPS_HEADER_SIZE,
						lkmsg_record_size - MTDOOPS_HEADER_SIZE, &ret_len);

	mtdoops_add_reason(cxt->oops_buf + MTDOOPS_HEADER_SIZE, reason, MTDOOPS_TYPE_DMESG, cxt->nextcount, cxt->nextpage);

	pmsg_buffer_start = phys_to_virt((cxt->pmsg_data.mem_address + cxt->pmsg_data.mem_size) - cxt->pmsg_data.pmsg_size);
	p_hdr = (struct pmsg_buffer_hdr *)pmsg_buffer_start;

	pr_err("mtdoops: mtdoops_do_dump pmsg paddr = 0x%lx \n",pmsg_buffer_start);

	if(p_hdr->sig == 0x43474244)
	{
		void *oopsbuf = cxt->oops_buf + (MTDOOPS_HEADER_SIZE + ret_len);
		uint8_t *p_buff_end = p_hdr->data + p_hdr->size.counter;
		int pmsg_cp_size = 0;
		int pstart = p_hdr->start.counter;
		int psize = p_hdr->size.counter;

		pmsg_cp_size = (record_size - (ret_len + MTDOOPS_HEADER_SIZE));
		if (psize <= pmsg_cp_size)
			pmsg_cp_size = psize;

		if (pstart >= pmsg_cp_size)
			memcpy(oopsbuf, p_hdr->data, pmsg_cp_size);
		else{
			memcpy(oopsbuf, p_buff_end - (pmsg_cp_size - pstart), pmsg_cp_size - pstart);
			memcpy(oopsbuf + (pmsg_cp_size - pstart), p_hdr->data, pstart);
		}
		mtdoops_add_pmsg_head(cxt->oops_buf + (MTDOOPS_HEADER_SIZE + ret_len), MTDOOPS_TYPE_PMSG);
	}
	else
		printk(KERN_ERR "mtdoops: read pmsg failed sig = 0x%x \n", p_hdr->sig);

	/* Panics must be written immediately */
	if (reason == KMSG_DUMP_OOPS || reason == KMSG_DUMP_PANIC) {
		mtdoops_write(cxt, 1);
	} else {
		/* For other cases, schedule work to write it "nicely" */
		//schedule_work(&cxt->work_write);
		// we should write log immediately , if use work to write, ufs will shutdown before write log finish
		mtdoops_write(cxt, 0);
	}

	printk(KERN_ERR "mtdoops: mtdoops_do_dump() finish \n");
}


static void mtdoops_do_dump_kmsgdump(struct kmsg_dumper *dumper,
				enum kmsg_dump_reason reason)
{
	//mtdoops_do_dump(dumper, (enum mtd_dump_reason)reason);
}


void mtdoops_do_dump_if(int reason)
{
	struct mtdoops_context *cxt = &oops_cxt;

	cxt->dump.active = true;
	kmsg_dump_rewind(&cxt->dump);
	mtdoops_do_dump(&cxt->dump,(enum mtd_dump_reason)reason);
	cxt->dump.active = false;
}
EXPORT_SYMBOL_GPL(mtdoops_do_dump_if);


static int mtdoops_reboot_nb_handle(struct notifier_block *this, unsigned long event,
								  void *ptr)
{
	//char *cmd = ptr;
	enum mtd_dump_reason reason;

	if (event == SYS_RESTART)
		reason = MTD_DUMP_RESTART;
	else if(event == SYS_POWER_OFF)
		reason = MTD_DUMP_POWEROFF;
	else
		return NOTIFY_OK;

	mtdoops_do_dump_if(reason);

	return NOTIFY_OK;
}


static void mtdoops_notify_add(struct mtd_info *mtd)
{
	struct mtdoops_context *cxt = &oops_cxt;
	u64 mtdoops_pages = div_u64(mtd->size, record_size);
	int err;

	if (!strcmp(mtd->name, mtddev))
		cxt->mtd_index = mtd->index;

	if (mtd->index != cxt->mtd_index || cxt->mtd_index < 0)
		return;

	if (mtd->size < mtd->erasesize * 2) {
		printk(KERN_ERR "mtdoops: MTD partition %d not big enough for mtdoops\n",
		       mtd->index);
		return;
	}
	if (mtd->erasesize < record_size) {
		printk(KERN_ERR "mtdoops: eraseblock size of MTD partition %d too small\n",
		       mtd->index);
		return;
	}
	if (mtd->size > MTDOOPS_MAX_MTD_SIZE) {
		printk(KERN_ERR "mtdoops: mtd%d is too large (limit is %d MiB)\n",
		       mtd->index, MTDOOPS_MAX_MTD_SIZE / 1024 / 1024);
		return;
	}

	/* oops_page_used is a bit field */
	cxt->oops_page_used =
		vmalloc(array_size(sizeof(unsigned long),
				   DIV_ROUND_UP(mtdoops_pages,
						BITS_PER_LONG)));
	if (!cxt->oops_page_used) {
		printk(KERN_ERR "mtdoops: could not allocate page array\n");
		return;
	}

	// for panic
	cxt->dump.max_reason = KMSG_DUMP_MAX;
	cxt->dump.dump = mtdoops_do_dump_kmsgdump;
	err = kmsg_dump_register(&cxt->dump);
	if (err) {
		printk(KERN_ERR "mtdoops: registering kmsg dumper failed, error %d\n", err);
		vfree(cxt->oops_page_used);
		cxt->oops_page_used = NULL;
		return;
	}


	// for restart and power off
	cxt->reboot_nb.notifier_call = mtdoops_reboot_nb_handle;
	cxt->reboot_nb.priority = 255;
	register_reboot_notifier(&cxt->reboot_nb);

	cxt->mtd = mtd;
	cxt->oops_pages = (int)mtd->size / record_size;
	find_next_position(cxt);
	printk(KERN_INFO "mtdoops: Attached to MTD device %d\n", mtd->index);
}

static void mtdoops_notify_remove(struct mtd_info *mtd)
{
	struct mtdoops_context *cxt = &oops_cxt;

	if (mtd->index != cxt->mtd_index || cxt->mtd_index < 0)
		return;

	if (kmsg_dump_unregister(&cxt->dump) < 0)
		printk(KERN_WARNING "mtdoops: could not unregister kmsg_dumper\n");

	unregister_reboot_notifier(&cxt->reboot_nb);

	cxt->mtd = NULL;
	flush_work(&cxt->work_erase);
	flush_work(&cxt->work_write);
}


static struct mtd_notifier mtdoops_notifier = {
	.add	= mtdoops_notify_add,
	.remove	= mtdoops_notify_remove,
};

static int mtdoops_parse_dt_u32(struct platform_device *pdev,
				const char *propname,
				u32 default_value, u32 *value)
{
	u32 val32 = 0;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, propname, &val32);
	if (ret == -EINVAL) {
		/* field is missing, use default value. */
		val32 = default_value;
	} else if (ret < 0) {
		dev_err(&pdev->dev, "failed to parse property %s: %d\n",
			propname, ret);
		return ret;
	}

	/* Sanity check our results. */
	if (val32 > INT_MAX) {
		dev_err(&pdev->dev, "%s %u > INT_MAX\n", propname, val32);
		return -EOVERFLOW;
	}

	*value = val32;
	return 0;
}


static int mtdoops_pmsg_probe(struct platform_device *pdev)
{
	struct mtdoops_context *cxt = &oops_cxt;
	struct resource *res;
	u32 value;
	int ret;

	dev_dbg(&pdev->dev, "using Device Tree\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"failed to locate DT /reserved-memory resource\n");
		return -EINVAL;
	}

	cxt->pmsg_data.mem_size = resource_size(res);
	cxt->pmsg_data.mem_address = res->start;


#define parse_u32(name, field, default_value) {				\
		ret = mtdoops_parse_dt_u32(pdev, name, default_value,	\
					    &value);			\
		if (ret < 0)						\
			return ret;					\
		field = value;						\
	}
	parse_u32("console-size", cxt->pmsg_data.console_size, 0);
	parse_u32("pmsg-size", cxt->pmsg_data.pmsg_size, 0);
#undef parse_u32

	printk(KERN_ERR "mtdoops: pares mtd_dt, mem_address =0x%x, mem_size =0x%x \n", cxt->pmsg_data.mem_address, cxt->pmsg_data.mem_size);
	printk(KERN_ERR "mtdoops: pares mtd_dt, pmsg_size =0x%x \n", cxt->pmsg_data.pmsg_size);

	return 0;
}


static const struct of_device_id dt_match[] = {
	{ .compatible = "mtdoops_pmsg" },
	{}
};


static struct platform_driver mtdoops_pmsg_driver = {
	.probe		= mtdoops_pmsg_probe,
	.driver		= {
		.name		= "mtdoops_pmsg",
		.of_match_table	= dt_match,
	},
};

static int __init mtdoops_init(void)
{
	struct mtdoops_context *cxt = &oops_cxt;
	int mtd_index;
	char *endp;

	printk(KERN_ERR "mtdoops: %s \n",__func__);

	if (strlen(mtddev) == 0) {
		printk(KERN_ERR "mtdoops: mtd device (mtddev=name/number) must be supplied\n");
		return -EINVAL;
	}
	if ((record_size & 4095) != 0) {
		printk(KERN_ERR "mtdoops: record_size must be a multiple of 4096\n");
		return -EINVAL;
	}
	if (record_size < 4096) {
		printk(KERN_ERR "mtdoops: record_size must be over 4096 bytes\n");
		return -EINVAL;
	}

	/* Setup the MTD device to use */
	cxt->mtd_index = -1;
	mtd_index = simple_strtoul(mtddev, &endp, 0);
	if (*endp == '\0')
		cxt->mtd_index = mtd_index;

	//cxt->oops_buf = vmalloc(record_size);
	cxt->oops_buf = kmalloc(record_size, GFP_KERNEL);
	if (!cxt->oops_buf) {
		printk(KERN_ERR "mtdoops: failed to allocate buffer workspace\n");
		return -ENOMEM;
	}
	memset(cxt->oops_buf, 0xff, record_size);

	INIT_WORK(&cxt->work_erase, mtdoops_workfunc_erase);

	// we should write log immediately , if use work to write, ufs will shutdown before write log finish
	INIT_WORK(&cxt->work_write, mtdoops_workfunc_write);

	platform_driver_register(&mtdoops_pmsg_driver);

	register_mtd_user(&mtdoops_notifier);
	return 0;
}

static void __exit mtdoops_exit(void)
{
	struct mtdoops_context *cxt = &oops_cxt;

	unregister_mtd_user(&mtdoops_notifier);
	kfree(cxt->oops_buf);
	vfree(cxt->oops_page_used);
}


module_init(mtdoops_init);
module_exit(mtdoops_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Purdie <rpurdie@openedhand.com>");
MODULE_DESCRIPTION("MTD Oops/Panic console logger/driver");
