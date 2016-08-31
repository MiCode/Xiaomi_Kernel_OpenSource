/*
 * mods_krnl.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

/***********************************************************************
 * mods_krnl_* functions, driver interfaces called by the Linux kernel *
 ***********************************************************************/
static int mods_krnl_open(struct inode *, struct file *);
static int mods_krnl_close(struct inode *, struct file *);
static unsigned int mods_krnl_poll(struct file *, poll_table *);
static int mods_krnl_mmap(struct file *, struct vm_area_struct *);
static long mods_krnl_ioctl(struct file *, unsigned int, unsigned long);

/* character driver entry points */
const struct file_operations mods_fops = {
	.owner			= THIS_MODULE,
	.open			= mods_krnl_open,
	.release		= mods_krnl_close,
	.poll			= mods_krnl_poll,
	.mmap			= mods_krnl_mmap,
	.unlocked_ioctl = mods_krnl_ioctl,
#if defined(HAVE_COMPAT_IOCTL)
	.compat_ioctl	= mods_krnl_ioctl,
#endif
};

#define DEVICE_NAME "mods"

struct miscdevice mods_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &mods_fops
};

/***********************************************
 * module wide parameters and access functions *
 * used to avoid globalization of variables    *
 ***********************************************/
static int debug = -0x80000000;
static int multi_instance = -1;

int mods_check_debug_level(int mask)
{
	return ((debug & mask) == mask) ? 1 : 0;
}

int mods_get_multi_instance(void)
{
	return multi_instance > 0;
}

/******************************
 * INIT/EXIT MODULE FUNCTIONS *
 ******************************/
static int __init mods_init_module(void)
{
	int rc;

	LOG_ENT();

	/* Initilize memory tracker */
	mods_init_mem();

	rc = misc_register(&mods_dev);
	if (rc < 0)
		return -EBUSY;

	mods_init_irq();

#ifdef CONFIG_ARCH_TEGRA
	mods_init_clock_api();
#endif

	mods_info_printk("driver loaded, version %x.%02x\n",
			 (MODS_DRIVER_VERSION>>8),
			 (MODS_DRIVER_VERSION&0xFF));
	LOG_EXT();
	return OK;
}

static void __exit mods_exit_module(void)
{
	LOG_ENT();
	mods_cleanup_irq();

	misc_deregister(&mods_dev);

#ifdef CONFIG_ARCH_TEGRA
	mods_shutdown_clock_api();
#endif

	/* Check for memory leakage */
	mods_check_mem();

	mods_info_printk("driver unloaded\n");
	LOG_EXT();
}

/***************************
 * KERNEL INTERFACE SET UP *
 ***************************/
module_init(mods_init_module);
module_exit(mods_exit_module);

MODULE_LICENSE("GPL");

module_param(debug, int, 0);
MODULE_PARM_DESC(debug,
		 "debug level (0=normal, 1=debug, 2=irq, 3=rich debug)");
module_param(multi_instance, int, 0);
MODULE_PARM_DESC(multi_instance,
		 "allows more than one client to connect simultaneously to "
		 "the driver");

/********************
 * HELPER FUNCTIONS *
 ********************/
static int id_is_valid(unsigned char channel)
{
	if (channel <= 0 || channel > MODS_CHANNEL_MAX)
		return ERROR;

	return OK;
}

static void mods_disable_all_devices(struct mods_file_private_data *priv)
{
	while (priv->enabled_devices != 0) {
		struct en_dev_entry *old = priv->enabled_devices;
#ifdef CONFIG_PCI
		pci_disable_device(old->dev);
#endif
		priv->enabled_devices = old->next;
		MODS_KFREE(old, sizeof(*old));
	}
}

/*********************
 * MAPPING FUNCTIONS *
 *********************/
static int mods_register_mapping(
	struct file *fp,
	struct SYS_MEM_MODS_INFO *p_mem_info,
	NvU64 dma_addr,
	NvU64 virtual_address,
	NvU32 mapping_length)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	MODS_PRIVATE_DATA(private_data, fp);

	LOG_ENT();

	mods_debug_printk(DEBUG_MEM_DETAILED,
			  "mapped dma 0x%llx, virt 0x%llx, size 0x%x\n",
			  dma_addr, virtual_address, mapping_length);

	MODS_KMALLOC(p_map_mem, sizeof(*p_map_mem));
	if (unlikely(!p_map_mem)) {
		LOG_EXT();
		return -ENOMEM;
	}
	memset(p_map_mem, 0, sizeof(*p_map_mem));

	if (p_mem_info == NULL)
		p_map_mem->contiguous = true;
	else
		p_map_mem->contiguous = false;
	p_map_mem->dma_addr = dma_addr;
	p_map_mem->virtual_addr = virtual_address;
	p_map_mem->mapping_length = mapping_length;
	p_map_mem->p_mem_info = p_mem_info;

	list_add(&p_map_mem->list, private_data->mods_mapping_list);
	LOG_EXT();
	return OK;
}

static void mods_unregister_mapping(struct file *fp, NvU64 virtual_address)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	MODS_PRIVATE_DATA(private_data, fp);

	struct list_head  *head = private_data->mods_mapping_list;
	struct list_head  *iter;

	LOG_ENT();

	list_for_each(iter, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		if (p_map_mem->virtual_addr == virtual_address) {
			/* remove from the list */
			list_del(iter);

			/* free our data struct which keeps track of mapping */
			MODS_KFREE(p_map_mem, sizeof(*p_map_mem));

			return;
		}
	}
	LOG_EXT();
}

static void mods_unregister_all_mappings(struct file *fp)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	MODS_PRIVATE_DATA(private_data, fp);

	struct list_head  *head = private_data->mods_mapping_list;
	struct list_head  *iter;
	struct list_head  *tmp;

	LOG_ENT();

	list_for_each_safe(iter, tmp, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);
		mods_unregister_mapping(fp, p_map_mem->virtual_addr);
	}

	LOG_EXT();
}

static pgprot_t mods_get_prot(NvU32 mem_type, pgprot_t prot)
{
	switch (mem_type) {
	case MODS_MEMORY_CACHED:
		return prot;

	case MODS_MEMORY_UNCACHED:
		return MODS_PGPROT_UC(prot);

	case MODS_MEMORY_WRITECOMBINE:
		return MODS_PGPROT_WC(prot);

	default:
		mods_warning_printk("unsupported memory type: %u\n",
				    mem_type);
		return prot;
	}
}

static pgprot_t mods_get_prot_for_range(struct file *fp, NvU64 dma_addr,
					NvU64 size, pgprot_t prot)
{
	MODS_PRIVATE_DATA(private_data, fp);
	if ((dma_addr == private_data->mem_type.dma_addr) &&
		(size == private_data->mem_type.size)) {

		return mods_get_prot(private_data->mem_type.type, prot);
	}
	return prot;
}

static char *mods_get_prot_str(NvU32 mem_type)
{
	switch (mem_type) {
	case MODS_MEMORY_CACHED:
		return "WB";

	case MODS_MEMORY_UNCACHED:
		return "UC";

	case MODS_MEMORY_WRITECOMBINE:
		return "WC";

	default:
		return "unknown";
	}
}

static char *mods_get_prot_str_for_range(struct file *fp, NvU64 dma_addr,
					 NvU64 size)
{
	MODS_PRIVATE_DATA(private_data, fp);
	if ((dma_addr == private_data->mem_type.dma_addr) &&
		(size == private_data->mem_type.size)) {

		return mods_get_prot_str(private_data->mem_type.type);
	}
	return "default";
}

/********************
 * KERNEL FUNCTIONS *
 ********************/
static void mods_krnl_vma_open(struct vm_area_struct *vma)
{
	struct mods_vm_private_data *vma_private_data;

	LOG_ENT();
	mods_debug_printk(DEBUG_MEM_DETAILED,
			  "open vma, virt 0x%lx, phys 0x%llx\n",
			  vma->vm_start,
			  (NvU64)(MODS_VMA_PGOFF(vma) << PAGE_SHIFT));

	if (MODS_VMA_PRIVATE(vma)) {
		vma_private_data = MODS_VMA_PRIVATE(vma);
		atomic_inc(&vma_private_data->usage_count);
	}
	LOG_EXT();
}

static void mods_krnl_vma_close(struct vm_area_struct *vma)
{
	LOG_ENT();

	if (MODS_VMA_PRIVATE(vma)) {
		struct mods_vm_private_data *vma_private_data
			= MODS_VMA_PRIVATE(vma);
		if (atomic_dec_and_test(&vma_private_data->usage_count)) {
			MODS_PRIVATE_DATA(private_data, vma_private_data->fp);
			spin_lock(&private_data->lock);

			/* we need to unregister the mapping */
			mods_unregister_mapping(vma_private_data->fp,
						vma->vm_start);
			mods_debug_printk(DEBUG_MEM_DETAILED,
					  "closed vma, virt 0x%lx\n",
					  vma->vm_start);
			MODS_VMA_PRIVATE(vma) = NULL;
			MODS_KFREE(vma_private_data,
				   sizeof(*vma_private_data));

			spin_unlock(&private_data->lock);
		}
	}
	LOG_EXT();
}

static struct vm_operations_struct mods_krnl_vm_ops = {
	.open	= mods_krnl_vma_open,
	.close	= mods_krnl_vma_close
};

static int mods_krnl_open(struct inode *ip, struct file *fp)
{
	struct list_head *mods_alloc_list;
	struct list_head *mods_mapping_list;
	struct mods_file_private_data *private_data;
	int id = 0;

	LOG_ENT();

	MODS_KMALLOC(mods_alloc_list, sizeof(struct list_head));
	if (unlikely(!mods_alloc_list)) {
		LOG_EXT();
		return -ENOMEM;
	}

	MODS_KMALLOC(mods_mapping_list, sizeof(struct list_head));
	if (unlikely(!mods_mapping_list)) {
		MODS_KFREE(mods_alloc_list, sizeof(struct list_head));
		LOG_EXT();
		return -ENOMEM;
	}

	MODS_KMALLOC(private_data, sizeof(*private_data));
	if (unlikely(!private_data)) {
		MODS_KFREE(mods_alloc_list, sizeof(struct list_head));
		MODS_KFREE(mods_mapping_list, sizeof(struct list_head));
		LOG_EXT();
		return -ENOMEM;
	}

	id	=  mods_alloc_channel();
	if (id_is_valid(id) != OK) {
		mods_error_printk("too many clients\n");
		MODS_KFREE(mods_alloc_list, sizeof(struct list_head));
		MODS_KFREE(mods_mapping_list, sizeof(struct list_head));
		MODS_KFREE(private_data, sizeof(*private_data));
		LOG_EXT();
		return -EBUSY;
	}

	private_data->mods_id = id;
	mods_irq_dev_set_pri(private_data->mods_id, private_data);

	INIT_LIST_HEAD(mods_alloc_list);
	INIT_LIST_HEAD(mods_mapping_list);
	private_data->mods_alloc_list = mods_alloc_list;
	private_data->mods_mapping_list = mods_mapping_list;
	private_data->enabled_devices = 0;
	private_data->mem_type.dma_addr = 0;
	private_data->mem_type.size = 0;
	private_data->mem_type.type = 0;

	spin_lock_init(&private_data->lock);

	init_waitqueue_head(&private_data->interrupt_event);

	fp->private_data = private_data;

	mods_info_printk("driver opened\n");
	LOG_EXT();
	return OK;
}

static int mods_krnl_close(struct inode *ip, struct file *fp)
{
	MODS_PRIVATE_DATA(private_data, fp);
	unsigned char id = MODS_GET_FILE_PRIVATE_ID(fp);

	LOG_ENT();

	BUG_ON(id_is_valid(id) != OK);
	mods_free_channel(id);
	mods_irq_dev_clr_pri(private_data->mods_id);

	mods_unregister_all_mappings(fp);
	mods_unregister_all_alloc(fp);
	mods_disable_all_devices(private_data);

	MODS_KFREE(private_data->mods_alloc_list, sizeof(struct list_head));
	MODS_KFREE(private_data->mods_mapping_list, sizeof(struct list_head));
	MODS_KFREE(private_data, sizeof(*private_data));

	mods_info_printk("driver closed\n");
	LOG_EXT();
	return OK;
}

static unsigned int mods_krnl_poll(struct file *fp, poll_table *wait)
{
	unsigned int mask = 0;
	MODS_PRIVATE_DATA(private_data, fp);
	unsigned char id = MODS_GET_FILE_PRIVATE_ID(fp);

	if (!(fp->f_flags & O_NONBLOCK)) {
		mods_debug_printk(DEBUG_ISR_DETAILED, "poll wait\n");
		poll_wait(fp, &private_data->interrupt_event, wait);
	}
	/* if any interrupts pending then check intr, POLLIN on irq */
	mask |= mods_irq_event_check(id);
	mods_debug_printk(DEBUG_ISR_DETAILED, "poll mask 0x%x\n", mask);
	return mask;
}

static int mods_krnl_map_inner(struct file *fp, struct vm_area_struct *vma);

static int mods_krnl_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct mods_vm_private_data *vma_private_data;

	LOG_ENT();

	vma->vm_ops = &mods_krnl_vm_ops;

	MODS_KMALLOC(vma_private_data, sizeof(*vma_private_data));
	if (unlikely(!vma_private_data)) {
		LOG_EXT();
		return -ENOMEM;
	}

	/* set private data for vm_area_struct */
	atomic_set(&vma_private_data->usage_count, 0);
	vma_private_data->fp = fp;
	MODS_VMA_PRIVATE(vma) = vma_private_data;

	/* call for the first time open function */
	mods_krnl_vma_open(vma);

	{
		int ret = OK;
		MODS_PRIVATE_DATA(private_data, fp);
		spin_lock(&private_data->lock);
		ret = mods_krnl_map_inner(fp, vma);
		spin_unlock(&private_data->lock);
		LOG_EXT();
		return ret;
	}
}

static int mods_krnl_map_inner(struct file *fp, struct vm_area_struct *vma)
{
	struct SYS_MEM_MODS_INFO *p_mem_info;
	unsigned int pages;
	int i, j;

	pages = MODS_VMA_SIZE(vma) >> PAGE_SHIFT;

	/* find already allocated memory */
	p_mem_info = mods_find_alloc(fp, MODS_VMA_OFFSET(vma));

	/* system memory */
	if (p_mem_info != NULL) {
		if (p_mem_info->alloc_type != MODS_ALLOC_TYPE_NON_CONTIG) {
			NvU64 dma_addr = MODS_VMA_OFFSET(vma);
			NvU32 pfn = MODS_DMA_TO_PHYS(dma_addr) >> PAGE_SHIFT;
			pgprot_t prot = mods_get_prot(p_mem_info->cache_type,
						      vma->vm_page_prot);

			mods_debug_printk(DEBUG_MEM,
				"map contig sysmem: "
				"dma 0x%llx, virt 0x%lx, size 0x%x, "
				"caching %s\n",
				dma_addr,
				(unsigned long)vma->vm_start,
				(unsigned int)MODS_VMA_SIZE(vma),
				mods_get_prot_str(p_mem_info->cache_type));

			if (remap_pfn_range(vma,
					    vma->vm_start,
					    pfn,
					    MODS_VMA_SIZE(vma),
					    prot)) {
				mods_error_printk(
					"failed to map contiguous memory\n");
				return -EAGAIN;
			}

			/* MODS_VMA_OFFSET(vma) can change so it can't be used
			 * to register the mapping */
			mods_register_mapping(fp,
					      p_mem_info,
					      dma_addr,
					      vma->vm_start,
					      MODS_VMA_SIZE(vma));
		} else {
			/* insert consecutive pages one at a time */

			unsigned long start = 0;
			NvU64 dma_addr = 0;
			struct SYS_PAGE_TABLE **p_page_tbl
				= p_mem_info->p_page_tbl;
			const pgprot_t prot
				= mods_get_prot(p_mem_info->cache_type,
						vma->vm_page_prot);

			mods_debug_printk(DEBUG_MEM,
				"map noncontig sysmem: "
				"virt 0x%lx, size 0x%x, caching %s\n",
				(unsigned long)vma->vm_start,
				(unsigned int)MODS_VMA_SIZE(vma),
				mods_get_prot_str(p_mem_info->cache_type));

			for (i = 0; i < p_mem_info->num_pages; i++) {
				NvU64 offs = MODS_VMA_OFFSET(vma);
				dma_addr = p_page_tbl[i]->dma_addr;
				if ((offs >= dma_addr) &&
				    (offs <  dma_addr + PAGE_SIZE)) {

					break;
				}
			}

			if (i == p_mem_info->num_pages) {
				mods_error_printk(
			"unable to find noncontiguous memory allocation\n");
				return -EINVAL;
			}

			if ((i + pages) > p_mem_info->num_pages) {
				mods_error_printk(
			"requested mapping exceeds allocation's boundary!\n");
				return -EINVAL;
			}

			start = vma->vm_start;
			for (j = i; j < (i + pages); j++) {
				dma_addr = MODS_DMA_TO_PHYS(
						p_page_tbl[j]->dma_addr);
				if (remap_pfn_range(vma,
						    start,
						    dma_addr>>PAGE_SHIFT,
						    PAGE_SIZE,
						    prot)) {
					mods_error_printk(
						    "failed to map memory\n");
					return -EAGAIN;
				}

				start += PAGE_SIZE;
			}

			/* MODS_VMA_OFFSET(vma) can change so it can't be used
			 * to register the mapping */
			mods_register_mapping(fp,
					      p_mem_info,
					      p_page_tbl[i]->dma_addr,
					      vma->vm_start,
					      MODS_VMA_SIZE(vma));
		}
	} else {
		/* device memory */

		NvU64 dma_addr = MODS_VMA_OFFSET(vma);
		mods_debug_printk(DEBUG_MEM,
			    "map device mem: "
			    "dma 0x%llx, virt 0x%lx, size 0x%x, caching %s\n",
			    dma_addr,
			    (unsigned long)vma->vm_start,
			    (unsigned int)MODS_VMA_SIZE(vma),
			    mods_get_prot_str_for_range(fp,
							MODS_VMA_OFFSET(vma),
							MODS_VMA_SIZE(vma)));

		if (io_remap_pfn_range(
				vma,
				vma->vm_start,
				dma_addr>>PAGE_SHIFT,
				MODS_VMA_SIZE(vma),
				mods_get_prot_for_range(
					fp,
					MODS_VMA_OFFSET(vma),
					MODS_VMA_SIZE(vma),
					vma->vm_page_prot))) {
			mods_error_printk("failed to map device memory\n");
			return -EAGAIN;
		}

		/* MODS_VMA_OFFSET(vma) can change so it can't be used to
		 * register the mapping */
		mods_register_mapping(fp,
				      NULL,
				      dma_addr,
				      vma->vm_start,
				      MODS_VMA_SIZE(vma));
	}
	return OK;
}

/*************************
 * ESCAPE CALL FUNCTIONS *
 *************************/

int esc_mods_get_api_version(struct file *pfile, struct MODS_GET_VERSION *p)
{
	p->version = MODS_DRIVER_VERSION;
	return OK;
}

int esc_mods_get_kernel_version(struct file *pfile, struct MODS_GET_VERSION *p)
{
	p->version = MODS_KERNEL_VERSION;
	return OK;
}

int esc_mods_set_driver_para(struct file *pfile, struct MODS_SET_PARA *p)
{
	int rc = OK;
	return rc;
}

/**************
 * IO control *
 **************/

static long mods_krnl_ioctl(struct file  *fp,
			    unsigned int  cmd,
			    unsigned long i_arg)
{
	int ret;
	void *arg_copy = 0;
	void *arg = (void *) i_arg;
	int arg_size;

	LOG_ENT();

	arg_size = _IOC_SIZE(cmd);

	if (arg_size > 0) {
		MODS_KMALLOC(arg_copy, arg_size);
		if (unlikely(!arg_copy)) {
			LOG_EXT();
			return -ENOMEM;
		}

		if (copy_from_user(arg_copy, arg, arg_size)) {
			mods_error_printk("failed to copy ioctl data\n");
			MODS_KFREE(arg_copy, arg_size);
			LOG_EXT();
			return -EFAULT;
		}
	}

	switch (cmd) {

#define MODS_IOCTL(code, function, argtype)\
	({\
	do {\
		mods_debug_printk(DEBUG_IOCTL, "ioctl(" #code ")\n");\
		if (arg_size != sizeof(struct argtype)) {\
			ret = -EINVAL;\
			mods_error_printk( \
				"invalid parameter passed to ioctl " #code \
				"\n");\
		} else {\
			ret = function(fp, (struct argtype *)arg_copy);\
			if ((ret == OK) && \
			    copy_to_user(arg, arg_copy, arg_size)) {\
				ret = -EFAULT;\
				mods_error_printk( \
					"copying return value for ioctl " \
					#code " to user space failed\n");\
			} \
		} \
	} while (0);\
	})

#define MODS_IOCTL_NORETVAL(code, function, argtype)\
	({\
	do {\
		mods_debug_printk(DEBUG_IOCTL, "ioctl(" #code ")\n");\
		if (arg_size != sizeof(struct argtype)) {\
			ret = -EINVAL;\
			mods_error_printk( \
				"invalid parameter passed to ioctl " #code \
				"\n");\
		} else {\
			ret = function(fp, (struct argtype *)arg_copy);\
		} \
	} while (0);\
	})

#define MODS_IOCTL_VOID(code, function)\
	({\
	do {\
		mods_debug_printk(DEBUG_IOCTL, "ioctl(" #code ")\n");\
		if (arg_size != 0) {\
			ret = -EINVAL;\
			mods_error_printk( \
				"invalid parameter passed to ioctl " #code \
				"\n");\
		} else {\
			ret = function(fp);\
		} \
	} while (0);\
	})

#ifdef CONFIG_PCI
	case MODS_ESC_FIND_PCI_DEVICE:
		MODS_IOCTL(MODS_ESC_FIND_PCI_DEVICE,
			   esc_mods_find_pci_dev, MODS_FIND_PCI_DEVICE);
		break;

	case MODS_ESC_FIND_PCI_CLASS_CODE:
		MODS_IOCTL(MODS_ESC_FIND_PCI_CLASS_CODE,
			   esc_mods_find_pci_class_code,
			   MODS_FIND_PCI_CLASS_CODE);
		break;

	case MODS_ESC_PCI_READ:
		MODS_IOCTL(MODS_ESC_PCI_READ, esc_mods_pci_read, MODS_PCI_READ);
		break;

	case MODS_ESC_PCI_WRITE:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_WRITE,
				    esc_mods_pci_write, MODS_PCI_WRITE);
		break;

	case MODS_ESC_PCI_BUS_ADD_DEVICES:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_BUS_ADD_DEVICES,
				    esc_mods_pci_bus_add_dev,
				    MODS_PCI_BUS_ADD_DEVICES);
		break;

	case MODS_ESC_PIO_READ:
		MODS_IOCTL(MODS_ESC_PIO_READ,
			   esc_mods_pio_read, MODS_PIO_READ);
		break;

	case MODS_ESC_PIO_WRITE:
		MODS_IOCTL_NORETVAL(MODS_ESC_PIO_WRITE,
				    esc_mods_pio_write, MODS_PIO_WRITE);
		break;

	case MODS_ESC_DEVICE_NUMA_INFO:
		MODS_IOCTL(MODS_ESC_DEVICE_NUMA_INFO,
			   esc_mods_device_numa_info,
			   MODS_DEVICE_NUMA_INFO);
		break;
#endif

	case MODS_ESC_ALLOC_PAGES:
		MODS_IOCTL(MODS_ESC_ALLOC_PAGES,
			   esc_mods_alloc_pages, MODS_ALLOC_PAGES);
		break;

	case MODS_ESC_DEVICE_ALLOC_PAGES:
		MODS_IOCTL(MODS_ESC_DEVICE_ALLOC_PAGES,
			   esc_mods_device_alloc_pages,
			   MODS_DEVICE_ALLOC_PAGES);
		break;

	case MODS_ESC_FREE_PAGES:
		MODS_IOCTL(MODS_ESC_FREE_PAGES,
			   esc_mods_free_pages, MODS_FREE_PAGES);
		break;

	case MODS_ESC_GET_PHYSICAL_ADDRESS:
		MODS_IOCTL(MODS_ESC_GET_PHYSICAL_ADDRESS,
			   esc_mods_get_phys_addr,
			   MODS_GET_PHYSICAL_ADDRESS);
		break;

	case MODS_ESC_SET_MEMORY_TYPE:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_MEMORY_TYPE,
				    esc_mods_set_mem_type,
				    MODS_MEMORY_TYPE);
		break;

	case MODS_ESC_VIRTUAL_TO_PHYSICAL:
		MODS_IOCTL(MODS_ESC_VIRTUAL_TO_PHYSICAL,
			   esc_mods_virtual_to_phys,
			   MODS_VIRTUAL_TO_PHYSICAL);
		break;

	case MODS_ESC_PHYSICAL_TO_VIRTUAL:
		MODS_IOCTL(MODS_ESC_PHYSICAL_TO_VIRTUAL,
			   esc_mods_phys_to_virtual, MODS_PHYSICAL_TO_VIRTUAL);
		break;

	case MODS_ESC_IRQ_REGISTER:
	case MODS_ESC_MSI_REGISTER:
		ret = -EINVAL;
		break;

	case MODS_ESC_REGISTER_IRQ:
		MODS_IOCTL_NORETVAL(MODS_ESC_REGISTER_IRQ,
				    esc_mods_register_irq, MODS_REGISTER_IRQ);
		break;

	case MODS_ESC_UNREGISTER_IRQ:
		MODS_IOCTL_NORETVAL(MODS_ESC_UNREGISTER_IRQ,
				    esc_mods_unregister_irq, MODS_REGISTER_IRQ);
		break;

	case MODS_ESC_QUERY_IRQ:
		MODS_IOCTL(MODS_ESC_QUERY_IRQ,
			   esc_mods_query_irq, MODS_QUERY_IRQ);
		break;

	case MODS_ESC_SET_IRQ_MASK:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_IRQ_MASK,
				    esc_mods_set_irq_mask, MODS_SET_IRQ_MASK);
		break;

	case MODS_ESC_IRQ_HANDLED:
		MODS_IOCTL_NORETVAL(MODS_ESC_IRQ_HANDLED,
				    esc_mods_irq_handled, MODS_REGISTER_IRQ);
		break;

#ifdef CONFIG_ACPI
	case MODS_ESC_EVAL_ACPI_METHOD:
		MODS_IOCTL(MODS_ESC_EVAL_ACPI_METHOD,
			   esc_mods_eval_acpi_method, MODS_EVAL_ACPI_METHOD);
		break;

	case MODS_ESC_EVAL_DEV_ACPI_METHOD:
		MODS_IOCTL(MODS_ESC_EVAL_DEV_ACPI_METHOD,
			   esc_mods_eval_dev_acpi_method,
			   MODS_EVAL_DEV_ACPI_METHOD);
		break;

	case MODS_ESC_ACPI_GET_DDC:
		MODS_IOCTL(MODS_ESC_ACPI_GET_DDC,
			   esc_mods_acpi_get_ddc, MODS_ACPI_GET_DDC);
		break;

#elif defined(CONFIG_ARCH_TEGRA)
	case MODS_ESC_EVAL_ACPI_METHOD:
	case MODS_ESC_EVAL_DEV_ACPI_METHOD:
	case MODS_ESC_ACPI_GET_DDC:
		/* Silent failure on Tegra to avoid clogging kernel log */
		ret = -EINVAL;
		break;
#endif
	case MODS_ESC_GET_API_VERSION:
		MODS_IOCTL(MODS_ESC_GET_API_VERSION,
			   esc_mods_get_api_version, MODS_GET_VERSION);
		break;

	case MODS_ESC_GET_KERNEL_VERSION:
		MODS_IOCTL(MODS_ESC_GET_KERNEL_VERSION,
			   esc_mods_get_kernel_version, MODS_GET_VERSION);
		break;

	case MODS_ESC_SET_DRIVER_PARA:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_DRIVER_PARA,
				    esc_mods_set_driver_para, MODS_SET_PARA);
		break;

#ifdef CONFIG_ARCH_TEGRA
	case MODS_ESC_GET_CLOCK_HANDLE:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_HANDLE,
			   esc_mods_get_clock_handle, MODS_GET_CLOCK_HANDLE);
		break;

	case MODS_ESC_SET_CLOCK_RATE:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_CLOCK_RATE,
				    esc_mods_set_clock_rate, MODS_CLOCK_RATE);
		break;

	case MODS_ESC_GET_CLOCK_RATE:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_RATE,
			   esc_mods_get_clock_rate, MODS_CLOCK_RATE);
		break;

	case MODS_ESC_GET_CLOCK_MAX_RATE:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_MAX_RATE,
			   esc_mods_get_clock_max_rate, MODS_CLOCK_RATE);
		break;

	case MODS_ESC_SET_CLOCK_MAX_RATE:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_CLOCK_MAX_RATE,
				    esc_mods_set_clock_max_rate,
				    MODS_CLOCK_RATE);
		break;

	case MODS_ESC_SET_CLOCK_PARENT:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_CLOCK_PARENT,
				    esc_mods_set_clock_parent,
				    MODS_CLOCK_PARENT);
		break;

	case MODS_ESC_GET_CLOCK_PARENT:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_PARENT,
			   esc_mods_get_clock_parent, MODS_CLOCK_PARENT);
		break;

	case MODS_ESC_ENABLE_CLOCK:
		MODS_IOCTL_NORETVAL(MODS_ESC_ENABLE_CLOCK,
				    esc_mods_enable_clock, MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_DISABLE_CLOCK:
		MODS_IOCTL_NORETVAL(MODS_ESC_DISABLE_CLOCK,
				    esc_mods_disable_clock, MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_IS_CLOCK_ENABLED:
		MODS_IOCTL(MODS_ESC_IS_CLOCK_ENABLED,
			   esc_mods_is_clock_enabled, MODS_CLOCK_ENABLED);
		break;

	case MODS_ESC_CLOCK_RESET_ASSERT:
		MODS_IOCTL_NORETVAL(MODS_ESC_CLOCK_RESET_ASSERT,
				    esc_mods_clock_reset_assert,
				    MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_CLOCK_RESET_DEASSERT:
		MODS_IOCTL_NORETVAL(MODS_ESC_CLOCK_RESET_DEASSERT,
				    esc_mods_clock_reset_deassert,
				    MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_FLUSH_CPU_CACHE_RANGE:
		MODS_IOCTL_NORETVAL(MODS_ESC_FLUSH_CPU_CACHE_RANGE,
				    esc_mods_flush_cpu_cache_range,
				    MODS_FLUSH_CPU_CACHE_RANGE);
		break;
#endif
	case MODS_ESC_MEMORY_BARRIER:
		MODS_IOCTL_VOID(MODS_ESC_MEMORY_BARRIER,
				esc_mods_memory_barrier);
		break;

	default:
		mods_error_printk("unrecognized ioctl (0x%x)\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (arg_copy)
		MODS_KFREE(arg_copy, arg_size);

	LOG_EXT();
	return ret;
}
