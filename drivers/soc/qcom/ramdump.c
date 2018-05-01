/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/elf.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <soc/qcom/ramdump.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>


#define RAMDUMP_NUM_DEVICES	256
#define RAMDUMP_NAME		"ramdump"

static struct class *ramdump_class;
static dev_t ramdump_dev;
static DEFINE_MUTEX(rd_minor_mutex);
static DEFINE_IDA(rd_minor_id);
static bool ramdump_devnode_inited;
#define RAMDUMP_WAIT_MSECS	120000
#define MAX_STRTBL_SIZE 512
#define MAX_NAME_LENGTH 16

struct ramdump_device {
	char name[256];

	unsigned int data_ready;
	unsigned int consumer_present;
	int ramdump_status;

	struct completion ramdump_complete;
	struct cdev cdev;
	struct device *dev;

	wait_queue_head_t dump_wait_q;
	int nsegments;
	struct ramdump_segment *segments;
	size_t elfcore_size;
	char *elfcore_buf;
	unsigned long attrs;
	bool complete_ramdump;
};

static int ramdump_open(struct inode *inode, struct file *filep)
{
	struct ramdump_device *rd_dev = container_of(inode->i_cdev,
					struct ramdump_device, cdev);
	rd_dev->consumer_present = 1;
	rd_dev->ramdump_status = 0;
	filep->private_data = rd_dev;
	return 0;
}

static int ramdump_release(struct inode *inode, struct file *filep)
{

	struct ramdump_device *rd_dev = container_of(inode->i_cdev,
					struct ramdump_device, cdev);
	rd_dev->consumer_present = 0;
	rd_dev->data_ready = 0;
	complete(&rd_dev->ramdump_complete);
	return 0;
}

static unsigned long offset_translate(loff_t user_offset,
		struct ramdump_device *rd_dev, unsigned long *data_left,
		void **vaddr)
{
	int i = 0;
	*vaddr = NULL;

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

	if (rd_dev->segments[i].v_address)
		*vaddr = rd_dev->segments[i].v_address + user_offset;

	return rd_dev->segments[i].address + user_offset;
}

#define MAX_IOREMAP_SIZE SZ_1M

static ssize_t ramdump_read(struct file *filep, char __user *buf, size_t count,
			loff_t *pos)
{
	struct ramdump_device *rd_dev = filep->private_data;
	void *device_mem = NULL, *origdevice_mem = NULL, *vaddr = NULL;
	unsigned long data_left = 0, bytes_before, bytes_after;
	unsigned long addr = 0;
	size_t copy_size = 0, alignsize;
	unsigned char *alignbuf = NULL, *finalbuf = NULL;
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
				&data_left, &vaddr);

	/* EOF check */
	if (data_left == 0) {
		pr_debug("Ramdump(%s): Ramdump complete. %lld bytes read.",
			rd_dev->name, *pos);
		rd_dev->ramdump_status = 0;
		ret = 0;
		goto ramdump_done;
	}

	copy_size = min_t(size_t, count, (size_t)MAX_IOREMAP_SIZE);
	copy_size = min_t(unsigned long, (unsigned long)copy_size, data_left);

	rd_dev->attrs = 0;
	rd_dev->attrs |= DMA_ATTR_SKIP_ZEROING;
	device_mem = vaddr ?: dma_remap(rd_dev->dev->parent, NULL, addr,
						copy_size, rd_dev->attrs);
	origdevice_mem = device_mem;

	if (device_mem == NULL) {
		pr_err("Ramdump(%s): Unable to ioremap: addr %lx, size %zd\n",
			rd_dev->name, addr, copy_size);
		rd_dev->ramdump_status = -1;
		ret = -ENOMEM;
		goto ramdump_done;
	}

	alignbuf = kzalloc(copy_size, GFP_KERNEL);
	if (!alignbuf) {
		pr_err("Ramdump(%s): Unable to alloc mem for aligned buf\n",
				rd_dev->name);
		rd_dev->ramdump_status = -1;
		ret = -ENOMEM;
		goto ramdump_done;
	}

	finalbuf = alignbuf;
	alignsize = copy_size;

	if ((unsigned long)device_mem & 0x7) {
		bytes_before = 8 - ((unsigned long)device_mem & 0x7);
		memcpy_fromio(alignbuf, device_mem, bytes_before);
		device_mem += bytes_before;
		alignbuf += bytes_before;
		alignsize -= bytes_before;
	}

	if (alignsize & 0x7) {
		bytes_after = alignsize & 0x7;
		memcpy(alignbuf, device_mem, alignsize - bytes_after);
		device_mem += alignsize - bytes_after;
		alignbuf += (alignsize - bytes_after);
		alignsize = bytes_after;
		memcpy_fromio(alignbuf, device_mem, alignsize);
	} else
		memcpy(alignbuf, device_mem, alignsize);

	if (copy_to_user(buf, finalbuf, copy_size)) {
		pr_err("Ramdump(%s): Couldn't copy all data to user.",
			rd_dev->name);
		rd_dev->ramdump_status = -1;
		ret = -EFAULT;
		goto ramdump_done;
	}

	kfree(finalbuf);
	if (!vaddr && origdevice_mem)
		dma_unremap(rd_dev->dev->parent, origdevice_mem, copy_size);

	*pos += copy_size;

	pr_debug("Ramdump(%s): Read %zd bytes from address %lx.",
			rd_dev->name, copy_size, addr);

	return *pos - orig_pos;

ramdump_done:
	if (!vaddr && origdevice_mem)
		dma_unremap(rd_dev->dev->parent, origdevice_mem, copy_size);

	kfree(finalbuf);
	rd_dev->data_ready = 0;
	*pos = 0;
	complete(&rd_dev->ramdump_complete);
	return ret;
}

static unsigned int ramdump_poll(struct file *filep,
					struct poll_table_struct *wait)
{
	struct ramdump_device *rd_dev = filep->private_data;
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

static int ramdump_devnode_init(void)
{
	int ret;

	ramdump_class = class_create(THIS_MODULE, RAMDUMP_NAME);
	ret = alloc_chrdev_region(&ramdump_dev, 0, RAMDUMP_NUM_DEVICES,
				  RAMDUMP_NAME);
	if (ret < 0) {
		pr_warn("%s: unable to allocate major\n", __func__);
		return ret;
	}

	ramdump_devnode_inited = true;

	return 0;
}

void *create_ramdump_device(const char *dev_name, struct device *parent)
{
	int ret, minor;
	struct ramdump_device *rd_dev;

	if (!dev_name) {
		pr_err("%s: Invalid device name.\n", __func__);
		return NULL;
	}

	mutex_lock(&rd_minor_mutex);
	if (!ramdump_devnode_inited) {
		ret = ramdump_devnode_init();
		if (ret)
			return ERR_PTR(ret);
	}
	mutex_unlock(&rd_minor_mutex);

	rd_dev = kzalloc(sizeof(struct ramdump_device), GFP_KERNEL);

	if (!rd_dev) {
		pr_err("%s: Couldn't alloc space for ramdump device!",
			__func__);
		return NULL;
	}

	/* get a minor number */
	minor = ida_simple_get(&rd_minor_id, 0, RAMDUMP_NUM_DEVICES,
			GFP_KERNEL);
	if (minor < 0) {
		pr_err("%s: No more minor numbers left! rc:%d\n", __func__,
			minor);
		ret = -ENODEV;
		goto fail_out_of_minors;
	}

	snprintf(rd_dev->name, ARRAY_SIZE(rd_dev->name), "ramdump_%s",
		 dev_name);

	init_completion(&rd_dev->ramdump_complete);
	if (parent) {
		rd_dev->complete_ramdump = of_property_read_bool(
				parent->of_node, "qcom,complete-ramdump");
		if (!rd_dev->complete_ramdump)
			dev_info(parent,
			"for %s segments only will be dumped.", dev_name);
	}

	init_waitqueue_head(&rd_dev->dump_wait_q);

	rd_dev->dev = device_create(ramdump_class, parent,
				    MKDEV(MAJOR(ramdump_dev), minor),
				   rd_dev, rd_dev->name);
	if (IS_ERR(rd_dev->dev)) {
		ret = PTR_ERR(rd_dev->dev);
		pr_err("%s: device_create failed for %s (%d)", __func__,
				dev_name, ret);
		goto fail_return_minor;
	}

	cdev_init(&rd_dev->cdev, &ramdump_file_ops);

	ret = cdev_add(&rd_dev->cdev, MKDEV(MAJOR(ramdump_dev), minor), 1);
	if (ret < 0) {
		pr_err("%s: cdev_add failed for %s (%d)", __func__,
				dev_name, ret);
		goto fail_cdev_add;
	}

	return (void *)rd_dev;

fail_cdev_add:
	device_unregister(rd_dev->dev);
fail_return_minor:
	ida_simple_remove(&rd_minor_id, minor);
fail_out_of_minors:
	kfree(rd_dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(create_ramdump_device);

void destroy_ramdump_device(void *dev)
{
	struct ramdump_device *rd_dev = dev;
	int minor = MINOR(rd_dev->cdev.dev);

	if (IS_ERR_OR_NULL(rd_dev))
		return;

	cdev_del(&rd_dev->cdev);
	device_unregister(rd_dev->dev);
	ida_simple_remove(&rd_minor_id, minor);
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

	if (rd_dev->complete_ramdump) {
		for (i = 0; i < nsegments-1; i++)
			segments[i].size =
				segments[i + 1].address - segments[i].address;
	}

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

	reinit_completion(&rd_dev->ramdump_complete);

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

static inline unsigned int set_section_name(const char *name,
					    struct elfhdr *ehdr,
					    int *strtable_idx)
{
	char *strtab = elf_str_table(ehdr);
	int idx, ret = 0;

	idx = *strtable_idx;
	if ((strtab == NULL) || (name == NULL))
		return 0;

	ret = idx;
	idx += strlcpy((strtab + idx), name, MAX_NAME_LENGTH);
	*strtable_idx = idx + 1;

	return ret;
}

static int _do_minidump(void *handle, struct ramdump_segment *segments,
		int nsegments)
{
	int ret, i;
	struct ramdump_device *rd_dev = (struct ramdump_device *)handle;
	struct elfhdr *ehdr;
	struct elf_shdr *shdr;
	unsigned long offset, strtbl_off;
	int strtable_idx = 1;

	if (!rd_dev->consumer_present) {
		pr_err("Ramdump(%s): No consumers. Aborting..\n", rd_dev->name);
		return -EPIPE;
	}

	rd_dev->segments = segments;
	rd_dev->nsegments = nsegments;

	rd_dev->elfcore_size = sizeof(*ehdr) +
			(sizeof(*shdr) * (nsegments + 2)) + MAX_STRTBL_SIZE;
	ehdr = kzalloc(rd_dev->elfcore_size, GFP_KERNEL);
	rd_dev->elfcore_buf = (char *)ehdr;
	if (!rd_dev->elfcore_buf)
		return -ENOMEM;

	memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
	ehdr->e_ident[EI_CLASS] = ELF_CLASS;
	ehdr->e_ident[EI_DATA] = ELF_DATA;
	ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	ehdr->e_ident[EI_OSABI] = ELF_OSABI;
	ehdr->e_type = ET_CORE;
	ehdr->e_machine  = ELF_ARCH;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_ehsize = sizeof(*ehdr);
	ehdr->e_shoff = sizeof(*ehdr);
	ehdr->e_shentsize = sizeof(*shdr);
	ehdr->e_shstrndx = 1;


	offset = rd_dev->elfcore_size;
	shdr = (struct elf_shdr *)(ehdr + 1);
	strtbl_off = sizeof(*ehdr) + sizeof(*shdr) * (nsegments + 2);
	shdr++;
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_offset = (elf_addr_t)strtbl_off;
	shdr->sh_size = MAX_STRTBL_SIZE;
	shdr->sh_entsize = 0;
	shdr->sh_flags = 0;
	shdr->sh_name = set_section_name("STR_TBL", ehdr, &strtable_idx);
	shdr++;

	for (i = 0; i < nsegments; i++, shdr++) {
		/* Update elf header */
		shdr->sh_type = SHT_PROGBITS;
		shdr->sh_name = set_section_name(segments[i].name, ehdr,
							&strtable_idx);
		shdr->sh_addr = (elf_addr_t)segments[i].address;
		shdr->sh_size = segments[i].size;
		shdr->sh_flags = SHF_WRITE;
		shdr->sh_offset = offset;
		shdr->sh_entsize = 0;
		offset += shdr->sh_size;
	}
	ehdr->e_shnum = nsegments + 2;

	rd_dev->data_ready = 1;
	rd_dev->ramdump_status = -1;

	reinit_completion(&rd_dev->ramdump_complete);

	/* Tell userspace that the data is ready */
	wake_up(&rd_dev->dump_wait_q);

	/* Wait (with a timeout) to let the ramdump complete */
	ret = wait_for_completion_timeout(&rd_dev->ramdump_complete,
			msecs_to_jiffies(RAMDUMP_WAIT_MSECS));

	if (!ret) {
		pr_err("Ramdump(%s): Timed out waiting for userspace.\n",
		       rd_dev->name);
		ret = -EPIPE;
	} else {
		ret = (rd_dev->ramdump_status == 0) ? 0 : -EPIPE;
	}

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

int do_minidump(void *handle, struct ramdump_segment *segments, int nsegments)
{
	return _do_minidump(handle, segments, nsegments);
}
EXPORT_SYMBOL(do_minidump);

int
do_elf_ramdump(void *handle, struct ramdump_segment *segments, int nsegments)
{
	return _do_ramdump(handle, segments, nsegments, true);
}
EXPORT_SYMBOL(do_elf_ramdump);
