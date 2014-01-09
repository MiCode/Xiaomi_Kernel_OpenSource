/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/elf.h>
#include <linux/wait.h>
#include <soc/qcom/ramdump.h>


#define RAMDUMP_WAIT_MSECS	120000

struct ramdump_device {
	char name[256];

	unsigned int data_ready;
	unsigned int consumer_present;
	int ramdump_status;

	struct completion ramdump_complete;
	struct miscdevice device;

	wait_queue_head_t dump_wait_q;
	int nsegments;
	struct ramdump_segment *segments;
	size_t elfcore_size;
	char *elfcore_buf;
};

static int ramdump_open(struct inode *inode, struct file *filep)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	rd_dev->consumer_present = 1;
	rd_dev->ramdump_status = 0;
	return 0;
}

static int ramdump_release(struct inode *inode, struct file *filep)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	rd_dev->consumer_present = 0;
	rd_dev->data_ready = 0;
	complete(&rd_dev->ramdump_complete);
	return 0;
}

static unsigned long offset_translate(loff_t user_offset,
		struct ramdump_device *rd_dev, unsigned long *data_left)
{
	int i = 0;

	for (i = 0; i < rd_dev->nsegments; i++)
		if (user_offset >= rd_dev->segments[i].size)
			user_offset -= rd_dev->segments[i].size;
		else
			break;

	if (i == rd_dev->nsegments) {
		pr_debug("Ramdump(%s): offset_translate returning zero\n",
				rd_dev->name);
		*data_left = 0;
		return 0;
	}

	*data_left = rd_dev->segments[i].size - user_offset;

	pr_debug("Ramdump(%s): Returning address: %llx, data_left = %ld\n",
		rd_dev->name, rd_dev->segments[i].address + user_offset,
		*data_left);

	return rd_dev->segments[i].address + user_offset;
}

#define MAX_IOREMAP_SIZE SZ_1M

static ssize_t ramdump_read(struct file *filep, char __user *buf, size_t count,
			loff_t *pos)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	void *device_mem = NULL;
	unsigned long data_left = 0;
	unsigned long addr = 0;
	size_t copy_size = 0;
	int ret = 0;
	loff_t orig_pos = *pos;

	if ((filep->f_flags & O_NONBLOCK) && !rd_dev->data_ready)
		return -EAGAIN;

	ret = wait_event_interruptible(rd_dev->dump_wait_q, rd_dev->data_ready);
	if (ret)
		return ret;

	if (*pos < rd_dev->elfcore_size) {
		copy_size = rd_dev->elfcore_size - *pos;
		copy_size = min(copy_size, count);

		if (copy_to_user(buf, rd_dev->elfcore_buf + *pos, copy_size)) {
			ret = -EFAULT;
			goto ramdump_done;
		}
		*pos += copy_size;
		count -= copy_size;
		buf += copy_size;
		if (count == 0)
			return copy_size;
	}

	addr = offset_translate(*pos - rd_dev->elfcore_size, rd_dev,
				&data_left);

	/* EOF check */
	if (data_left == 0) {
		pr_debug("Ramdump(%s): Ramdump complete. %lld bytes read.",
			rd_dev->name, *pos);
		rd_dev->ramdump_status = 0;
		ret = 0;
		goto ramdump_done;
	}

	copy_size = min(count, (size_t)MAX_IOREMAP_SIZE);
	copy_size = min((unsigned long)copy_size, data_left);
	device_mem = ioremap_nocache(addr, copy_size);

	if (device_mem == NULL) {
		pr_err("Ramdump(%s): Unable to ioremap: addr %lx, size %zd\n",
			rd_dev->name, addr, copy_size);
		rd_dev->ramdump_status = -1;
		ret = -ENOMEM;
		goto ramdump_done;
	}

	if (copy_to_user(buf, device_mem, copy_size)) {
		pr_err("Ramdump(%s): Couldn't copy all data to user.",
			rd_dev->name);
		iounmap(device_mem);
		rd_dev->ramdump_status = -1;
		ret = -EFAULT;
		goto ramdump_done;
	}

	iounmap(device_mem);
	*pos += copy_size;

	pr_debug("Ramdump(%s): Read %zd bytes from address %lx.",
			rd_dev->name, copy_size, addr);

	return *pos - orig_pos;

ramdump_done:
	rd_dev->data_ready = 0;
	*pos = 0;
	complete(&rd_dev->ramdump_complete);
	return ret;
}

static unsigned int ramdump_poll(struct file *filep,
					struct poll_table_struct *wait)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	unsigned int mask = 0;

	if (rd_dev->data_ready)
		mask |= (POLLIN | POLLRDNORM);

	poll_wait(filep, &rd_dev->dump_wait_q, wait);
	return mask;
}

static const struct file_operations ramdump_file_ops = {
	.open = ramdump_open,
	.release = ramdump_release,
	.read = ramdump_read,
	.poll = ramdump_poll
};

void *create_ramdump_device(const char *dev_name, struct device *parent)
{
	int ret;
	struct ramdump_device *rd_dev;

	if (!dev_name) {
		pr_err("%s: Invalid device name.\n", __func__);
		return NULL;
	}

	rd_dev = kzalloc(sizeof(struct ramdump_device), GFP_KERNEL);

	if (!rd_dev) {
		pr_err("%s: Couldn't alloc space for ramdump device!",
			__func__);
		return NULL;
	}

	snprintf(rd_dev->name, ARRAY_SIZE(rd_dev->name), "ramdump_%s",
		 dev_name);

	init_completion(&rd_dev->ramdump_complete);

	rd_dev->device.minor = MISC_DYNAMIC_MINOR;
	rd_dev->device.name = rd_dev->name;
	rd_dev->device.fops = &ramdump_file_ops;
	rd_dev->device.parent = parent;

	init_waitqueue_head(&rd_dev->dump_wait_q);

	ret = misc_register(&rd_dev->device);

	if (ret) {
		pr_err("%s: misc_register failed for %s (%d)", __func__,
				dev_name, ret);
		kfree(rd_dev);
		return NULL;
	}

	return (void *)rd_dev;
}
EXPORT_SYMBOL(create_ramdump_device);

void destroy_ramdump_device(void *dev)
{
	struct ramdump_device *rd_dev = dev;

	if (IS_ERR_OR_NULL(rd_dev))
		return;

	misc_deregister(&rd_dev->device);
	kfree(rd_dev);
}
EXPORT_SYMBOL(destroy_ramdump_device);

static int _do_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments, bool use_elf)
{
	int ret, i;
	struct ramdump_device *rd_dev = (struct ramdump_device *)handle;
	Elf32_Phdr *phdr;
	Elf32_Ehdr *ehdr;
	unsigned long offset;

	if (!rd_dev->consumer_present) {
		pr_err("Ramdump(%s): No consumers. Aborting..\n", rd_dev->name);
		return -EPIPE;
	}

	for (i = 0; i < nsegments; i++)
		segments[i].size = PAGE_ALIGN(segments[i].size);

	rd_dev->segments = segments;
	rd_dev->nsegments = nsegments;

	if (use_elf) {
		rd_dev->elfcore_size = sizeof(*ehdr) +
				       sizeof(*phdr) * nsegments;
		ehdr = kzalloc(rd_dev->elfcore_size, GFP_KERNEL);
		rd_dev->elfcore_buf = (char *)ehdr;
		if (!rd_dev->elfcore_buf)
			return -ENOMEM;

		memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
		ehdr->e_ident[EI_CLASS] = ELFCLASS32;
		ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
		ehdr->e_ident[EI_VERSION] = EV_CURRENT;
		ehdr->e_ident[EI_OSABI] = ELFOSABI_NONE;
		ehdr->e_type = ET_CORE;
		ehdr->e_version = EV_CURRENT;
		ehdr->e_phoff = sizeof(*ehdr);
		ehdr->e_ehsize = sizeof(*ehdr);
		ehdr->e_phentsize = sizeof(*phdr);
		ehdr->e_phnum = nsegments;

		offset = rd_dev->elfcore_size;
		phdr = (Elf32_Phdr *)(ehdr + 1);
		for (i = 0; i < nsegments; i++, phdr++) {
			phdr->p_type = PT_LOAD;
			phdr->p_offset = offset;
			phdr->p_vaddr = phdr->p_paddr = segments[i].address;
			phdr->p_filesz = phdr->p_memsz = segments[i].size;
			phdr->p_flags = PF_R | PF_W | PF_X;
			offset += phdr->p_filesz;
		}
	}

	rd_dev->data_ready = 1;
	rd_dev->ramdump_status = -1;

	INIT_COMPLETION(rd_dev->ramdump_complete);

	/* Tell userspace that the data is ready */
	wake_up(&rd_dev->dump_wait_q);

	/* Wait (with a timeout) to let the ramdump complete */
	ret = wait_for_completion_timeout(&rd_dev->ramdump_complete,
			msecs_to_jiffies(RAMDUMP_WAIT_MSECS));

	if (!ret) {
		pr_err("Ramdump(%s): Timed out waiting for userspace.\n",
			rd_dev->name);
		ret = -EPIPE;
	} else
		ret = (rd_dev->ramdump_status == 0) ? 0 : -EPIPE;

	rd_dev->data_ready = 0;
	rd_dev->elfcore_size = 0;
	kfree(rd_dev->elfcore_buf);
	rd_dev->elfcore_buf = NULL;
	return ret;

}

int do_ramdump(void *handle, struct ramdump_segment *segments, int nsegments)
{
	return _do_ramdump(handle, segments, nsegments, false);
}
EXPORT_SYMBOL(do_ramdump);

int
do_elf_ramdump(void *handle, struct ramdump_segment *segments, int nsegments)
{
	return _do_ramdump(handle, segments, nsegments, true);
}
EXPORT_SYMBOL(do_elf_ramdump);
