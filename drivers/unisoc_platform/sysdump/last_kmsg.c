// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/memory.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/kmsg_dump.h>
#include <linux/uio.h>
#include <linux/time.h>
#include <trace/hooks/logbuf.h>
#include "unisoc_sysdump.h"
#include <uapi/mtd/ubi-user.h>
#include <../../drivers/mtd/ubi/ubi.h>
#include "../../kernel/printk/printk_ringbuffer.h"
#include "unisoc_vmcoreinfo.h"

static bool log_buf_get;

static const char *devicename = "/dev/block/by-name/common_rs1_a";
static const char *sdklog_path = "/dev/block/by-name/sd_klog";

static char *kmsg_buf;
#define KMSG_BUF_SIZE (128 * 1024)
#define DEFAULT_REASON "Normal"
#define PANIC_REASON "Panic"
static int kmsg_get_flag;
int shutdown_save_log_flag;

#define LAST_KMSG_OFFSET	0
#define LAST_ANDROID_LOG_OFFSET	(1 * 1024 * 1024)

static atomic_t log_saving;

int get_kernel_log_to_buffer(char *buf, size_t buf_size)
{
	struct kmsg_dump_iter iter;
	size_t dump_size;

	if (!buf)
		return -EFAULT;

	kmsg_dump_rewind(&iter);

	/* Write dump contents. */
	if (!kmsg_dump_get_buffer(&iter, true, buf, buf_size, &dump_size)) {
		pr_err("get last kernel message failed\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(get_kernel_log_to_buffer);
static void get_last_kmsg(char *buf, size_t buf_size, const char *why)
{
	int header_size = 0;

	if (!buf)
		return;
	if (kmsg_get_flag > 0)
		return;
	kmsg_get_flag = 1;

	header_size = snprintf(buf, buf_size, "%s\n", why);

	header_size = ALIGN(header_size, 8);
	buf_size -= header_size;
	if (get_kernel_log_to_buffer(buf + header_size, buf_size))
		pr_err("save kernel log to buffer failed!\n");
}
#ifdef CONFIG_MTD
static ssize_t sysdump_vol_cdev_direct_write(struct file *file, const char *buf,
				     size_t count, loff_t *offp)
{
	struct ubi_volume_desc *desc = file->private_data;
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;
	int lnum, off, len, tbuf_size, err = 0;
	size_t count_save = count;

	if (!vol->direct_writes)
		return -EPERM;

	pr_info("requested: write %zd bytes to offset %lld",
		count, *offp);

	if (vol->vol_type == UBI_STATIC_VOLUME)
		return -EROFS;

	lnum = div_u64_rem(*offp, vol->usable_leb_size, &off);
	if (off & (ubi->min_io_size - 1)) {
		pr_err("unaligned position");
		return -EINVAL;
	}

	if (*offp + count > vol->used_bytes)
		count_save = count = vol->used_bytes - *offp;

	/* We can write only in fractions of the minimum I/O unit */
	if (count & (ubi->min_io_size - 1)) {
		pr_err("unaligned write length");
		return -EINVAL;
	}

	tbuf_size = vol->usable_leb_size;
	if (count < tbuf_size)
		tbuf_size = ALIGN(count, ubi->min_io_size);

	len = count > tbuf_size ? tbuf_size : count;

	while (count) {
		cond_resched();

		if (off + len >= vol->usable_leb_size)
			len = vol->usable_leb_size - off;

		err = ubi_eba_write_leb(ubi, vol, lnum, buf, off, len);
		if (err)
			break;

		off += len;
		if (off == vol->usable_leb_size) {
			lnum += 1;
			off -= vol->usable_leb_size;
		}

		count -= len;
		*offp += len;
		buf += len;
		len = count > tbuf_size ? tbuf_size : count;
	}

	return err ? err : count_save - count;
}

static ssize_t sysdump_vol_cdev_write(struct file *file, const char *buf,
			      size_t count, loff_t *offp)
{
	int err = 0;
	struct ubi_volume_desc *desc = file->private_data;
	struct ubi_volume *vol = desc->vol;

	if (!vol->updating && !vol->changing_leb)
		return sysdump_vol_cdev_direct_write(file, buf, count, offp);
	return -1;

}

static int write_data_to_ubi_partition(struct file *filp, char *buf,
					size_t buf_size, long long offset)
{
	int ret = 0;

	ret = sysdump_vol_cdev_write(filp, buf, buf_size, (loff_t *)&offset);

	return ret;

}

static int sysdump_set_property(struct file *plog_file, int property, int value)
{
	struct ubi_volume_desc *desc = plog_file->private_data;
	struct ubi_volume *vol = desc->vol;
	struct ubi_device *ubi = vol->ubi;
	int ret = 0;

	switch (property) {
	case UBI_VOL_PROP_DIRECT_WRITE:
		mutex_lock(&ubi->device_mutex);
		desc->vol->direct_writes = !!value;
		mutex_unlock(&ubi->device_mutex);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	if (ret != 0) {
		pr_err("%s UBI_IOCVOLUP err ! ret = %d", __func__, ret);
		return -1;
	}
	pr_info("%s UBI_IOCVOLUP ok ! ", __func__);

	return ret;
}

static void save_log_to_partition(void *buf)
{
	int ret;
	static struct file *plog_file;

	if (shutdown_save_log_flag == 1)
		return;

	plog_file = filp_open_block(sdklog_path, O_RDWR | O_DSYNC | O_NOATIME | O_EXCL, 0);
	if (IS_ERR(plog_file)) {
		ret = PTR_ERR(plog_file);
		pr_err("failed to open '%s':%d!\n", sdklog_path, ret);
		plog_file = filp_open_block(devicename, O_RDWR | O_DSYNC | O_NOATIME | O_EXCL, 0);
		if (IS_ERR(plog_file)) {
			ret = PTR_ERR(plog_file);
			pr_err("failed to open '%s':%d!\n", devicename, ret);
			return;
		}
	}

	/* handle last kmsg */
	get_last_kmsg(kmsg_buf, KMSG_BUF_SIZE, buf);

	if (kmsg_buf == NULL) {
		pr_err("kmsg_buf is null, return!\n");
		goto end;
	}
	sysdump_set_property(plog_file, UBI_VOL_PROP_DIRECT_WRITE, 1);
	ret = write_data_to_ubi_partition(plog_file, kmsg_buf, KMSG_BUF_SIZE, LAST_KMSG_OFFSET);
	if (ret != KMSG_BUF_SIZE)
		pr_err("write kmsg to partition error! :%d!\n", ret);

	/* handle last android log */
	if (ylog_buffer == NULL) {
		pr_err("ylog_buffer is null, return!\n");
		goto end;
	}

	ret = write_data_to_ubi_partition(plog_file, ylog_buffer, YLOG_BUF_SIZE,
		LAST_ANDROID_LOG_OFFSET);
	if (ret != YLOG_BUF_SIZE)
		pr_err("write kmsg to partition error! :%d!\n", ret);

end:
	filp_close(plog_file, NULL);

}

#else
static int write_data_to_partition(struct file *filp, char *buf, size_t buf_size, int offset)
{
	struct iov_iter iiter;
	struct kiocb kiocb;
	struct kvec iov;
	int ret;

	iov.iov_base = (void *)buf;
	iov.iov_len = buf_size;

	kiocb.ki_filp = filp;
	kiocb.ki_flags = IOCB_DSYNC;
	kiocb.ki_pos = offset;

	iiter.iter_type = ITER_KVEC | WRITE;
	iiter.kvec = &iov;
	iiter.nr_segs = 1;
	iiter.iov_offset = 0;
	iiter.count = buf_size;

	ret = filp->f_op->write_iter(&kiocb, &iiter);

	return ret;

}

static void save_log_to_partition(void *buf)
{
	int ret;
	static struct file *plog_file;

	if (shutdown_save_log_flag == 1)
		return;

	plog_file = filp_open_block(sdklog_path, O_RDWR | O_DSYNC | O_NOATIME | O_EXCL, 0);
	if (IS_ERR(plog_file)) {
		ret = PTR_ERR(plog_file);
		pr_err("failed to open '%s':%d!\n", sdklog_path, ret);
		plog_file = filp_open_block(devicename, O_RDWR | O_DSYNC | O_NOATIME | O_EXCL, 0);
		if (IS_ERR(plog_file)) {
			ret = PTR_ERR(plog_file);
			pr_err("failed to open '%s':%d!\n", devicename, ret);
			return;
		}
	}

	/* handle last kmsg */
	get_last_kmsg(kmsg_buf, KMSG_BUF_SIZE, buf);

	if (kmsg_buf == NULL) {
		pr_err("kmsg_buf is null, return!\n");
		goto end;
	}
	ret = write_data_to_partition(plog_file, kmsg_buf, KMSG_BUF_SIZE, LAST_KMSG_OFFSET);
	if (ret)
		pr_err("write kmsg to partition error! :%d!\n", ret);

	/* handle last android log */
	if (ylog_buffer == NULL) {
		pr_err("ylog_buffer is null, return!\n");
		goto end;
	}

	ret = write_data_to_partition(plog_file, ylog_buffer, YLOG_BUF_SIZE,
			LAST_ANDROID_LOG_OFFSET);
	if (ret)
		pr_err("write ylog to partition error!:%d!\n", ret);

end:
	filp_close(plog_file, NULL);


}
#endif
static int save_log_to_partition_handler(struct notifier_block *nb, unsigned long event, void *buf)
{
	if (atomic_read(&log_saving))
		return 0;
	atomic_inc(&log_saving);
	save_log_to_partition(buf);
	atomic_dec(&log_saving);

	return 0;
}
const char *kmsg_dump_reason(enum kmsg_dump_reason reason)
{
	switch (reason) {
	case KMSG_DUMP_PANIC:
		return "Panic";
	case KMSG_DUMP_OOPS:
		return "Oops";
	case KMSG_DUMP_EMERG:
		return "Emergency";
	case KMSG_DUMP_SHUTDOWN:
		return "Shutdown";
	default:
		return "Unknown";
	}
}
void shutdown_save_log_to_partition(char *time, char *reason)
{
	char save_reason[100] = {0};

	shutdown_save_log_flag = 0;
	if ((time != NULL) && (reason != NULL))
		snprintf(save_reason, 99, "%s-%s-%s\n", "shutdown_error", time, reason);
	else
		snprintf(save_reason, 99, "%s\n", "shutdown_error");
	if (save_log_to_partition_handler(NULL, 0, save_reason)) {
		pr_err("save log to partition failed!\n");
		return;
	}
	shutdown_save_log_flag = 1;
}
EXPORT_SYMBOL(shutdown_save_log_to_partition);
static void last_kmsg_handler(struct kmsg_dumper *dumper,
		enum kmsg_dump_reason reason)
{
	const char *why;

	shutdown_save_log_flag = 0;
	why = kmsg_dump_reason(reason);
	if (kmsg_buf == NULL) {
		pr_err("no available buf, do nothing!\n");
	} else {
		get_last_kmsg(kmsg_buf, KMSG_BUF_SIZE, why);
	}
}

static struct notifier_block sysdump_log_notifier = {
	.notifier_call  = save_log_to_partition_handler,
};

static struct kmsg_dumper sysdump_dumper = {
	.dump = last_kmsg_handler,
	.max_reason = KMSG_DUMP_MAX,
};

static int log_buf_save_init(void)
{
	int ret;

	/* save log_buf */
	ret = minidump_save_extend_information("log_buf", 0, 0);
	if (ret < 0)
		return ret;

	/* save prb_descs */
	ret = minidump_save_extend_information("prb_descs", 0, 0);
	if (ret < 0)
		return ret;

	/* save printk_info */
	ret = minidump_save_extend_information("prb_infos", 0, 0);
	if (ret < 0)
		return ret;

	return 0;

}
static void minidump_save_prb_info(void *ignore, struct printk_ringbuffer *prb,
					struct printk_record *r)
{
	int ret;
	struct prb_desc_ring descring = prb->desc_ring;
	struct prb_data_ring dataring = prb->text_data_ring;
	char *log_buf = dataring.data;
	unsigned long log_buf_len = _DATA_SIZE(dataring.size_bits);
	struct prb_desc *descs = descring.descs;
	struct printk_info *infos = descring.infos;
	unsigned int descs_counts = _DESCS_COUNT(descring.count_bits);
	unsigned long paddr_end;
	unsigned long paddr_start;
	unsigned long size;

	if (log_buf_get)
		return;

	/* save log_buf */
	ret = minidump_change_extend_information("log_buf", virt_to_phys(log_buf),
					virt_to_phys(log_buf + log_buf_len));
	if (ret < 0)
		return;

	/* save prb_descs */
	paddr_start = virt_to_phys(descs);
	size = (unsigned long)descs_counts*sizeof(struct prb_desc);
	paddr_end = paddr_start + size;
	ret = minidump_change_extend_information("prb_descs", paddr_start, paddr_end);
	if (ret < 0)
		return;

	/* save printk_info */
	paddr_start = virt_to_phys(infos);
	size = (unsigned long)descs_counts*sizeof(struct printk_info);
	paddr_end = paddr_start + size;
	ret = minidump_change_extend_information("prb_infos", paddr_start, paddr_end);
	if (ret < 0)
		return;

	vmcoreinfo_append_str("SYMBOL(%s)=0x%llx\n", "prb",
				virt_to_phys(prb));
	vmcoreinfo_append_str("SYMBOL(%s)=0x%x\n", "descs_counts",
				descs_counts);
	vmcoreinfo_append_str("SYMBOL(%s)=0x%lx\n", "log_buf_len",
				log_buf_len);
	update_vmcoreinfo_note();
	log_buf_get = true;
}

static void minidump_save_log_buf_init(void)
{
	/* minidump save kernel log_buf and descs */
	if (log_buf_save_init())
		return;
	register_trace_android_vh_logbuf(
			minidump_save_prb_info, NULL);
}

static void kmsg_save_init(void)
{
	int ret;
	phys_addr_t lkmsg_paddr;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	struct page **pages;
	void *vaddr;
	unsigned int i;

	kmsg_buf = kzalloc(KMSG_BUF_SIZE, GFP_KERNEL);
	if (kmsg_buf == NULL)
		return;

	SetPageReserved(virt_to_page(kmsg_buf));

	pr_info("register sysdump log notifier\n");
	register_reboot_notifier(&sysdump_log_notifier);

	ret = minidump_save_extend_information("last_kmsg", __pa(kmsg_buf),
			__pa(kmsg_buf + KMSG_BUF_SIZE));
	if (ret)
		pr_err("last_kmsg added to minidump failed\n");
	/* map buffer as noncached */
	lkmsg_paddr = __pa(kmsg_buf);
	if (!pfn_valid(lkmsg_paddr >> PAGE_SHIFT)) {
		pr_err("invalid pfn, do nothing\n");
		return;
	}
	page_start = lkmsg_paddr - offset_in_page(lkmsg_paddr);
	page_count = DIV_ROUND_UP(KMSG_BUF_SIZE + offset_in_page(lkmsg_paddr), PAGE_SIZE);
	prot = pgprot_writecombine(PAGE_KERNEL);
	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n",
				__func__, page_count);
		return;
	}
	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	pages = NULL;
	kmsg_buf = vaddr + offset_in_page(lkmsg_paddr);

	kmsg_dump_register(&sysdump_dumper);
}

int last_kmsg_init(void)
{
	/* minidump save log_buf */
	minidump_save_log_buf_init();
	/* save panic kmsg and reboot kmsg */
	kmsg_save_init();

	return 0;
}
void last_kmsg_exit(void)
{
	unregister_trace_android_vh_logbuf(minidump_save_prb_info, NULL);
	if (kmsg_buf != NULL) {
		ClearPageReserved(virt_to_page(kmsg_buf));
		kfree(kmsg_buf);
	}
	unregister_reboot_notifier(&sysdump_log_notifier);
	kmsg_dump_unregister(&sysdump_dumper);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("save kernel log to disk");
