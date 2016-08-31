/*
 * mods_internal.h - This file is part of NVIDIA MODS kernel driver.
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

#ifndef _MODS_INTERNAL_H_
#define _MODS_INTERNAL_H_

#include <linux/version.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/slab.h>

#define		NvU8	u8
#define		NvU16	u16
#define		NvU32	u32
#define		NvS32	s32
#define		NvU64	u64

#include "mods_config.h"
#include "mods.h"

#ifndef true
#define true	1
#define false	0
#endif

/* function return code */
#define OK		 0
#define ERROR		-1

#define IRQ_FOUND	 1
#define IRQ_NOT_FOUND	 0

#define DEV_FOUND	 1
#define DEV_NOT_FOUND	 0

#define MSI_DEV_FOUND	  1
#define MSI_DEV_NOT_FOUND 0

struct SYS_PAGE_TABLE {
	NvU64	     dma_addr;
	struct page *p_page;
};

struct en_dev_entry {
	struct pci_dev	    *dev;
	struct en_dev_entry *next;
};

struct mem_type {
	NvU64 dma_addr;
	NvU64 size;
	NvU32 type;
};

/* file private data */
struct mods_file_private_data {
	struct list_head    *mods_alloc_list;
	struct list_head    *mods_mapping_list;
	wait_queue_head_t    interrupt_event;
	struct en_dev_entry *enabled_devices;
	int		     mods_id;
	struct mem_type	     mem_type;
	spinlock_t	     lock;
};

/* VM private data */
struct mods_vm_private_data {
	struct file *fp;
	atomic_t     usage_count;
};

/* system memory allocation tracking */
struct SYS_MEM_MODS_INFO {
	NvU32		 alloc_type;

	/* tells how the memory is cached:
	 * (MODS_MEMORY_CACHED, MODS_MEMORY_UNCACHED, MODS_MEMORY_WRITECOMBINE)
	 */
	NvU32		 cache_type;

	NvU32		 length;    /* actual number of bytes allocated */
	NvU32		 order;	    /* 2^order pages allocated (contig alloc) */
	NvU32		 num_pages; /* number of allocated pages */
	NvU32		 k_mapping_ref_cnt;

	NvU32		 addr_bits;
	struct page	*p_page;
	NvU64		 logical_addr; /* kernel logical address */
	NvU64		 dma_addr;     /* physical address, for contig alloc,
					  machine address on Xen */
	int		 numa_node;    /* numa node for the allocation */

	/* keeps information about allocated pages for noncontig allocation */
	struct SYS_PAGE_TABLE **p_page_tbl;

	struct list_head list;
};

#define MODS_ALLOC_TYPE_NON_CONTIG	0
#define MODS_ALLOC_TYPE_CONTIG		1
#define MODS_ALLOC_TYPE_BIGPHYS_AREA	2

/* map memory tracking */
struct SYS_MAP_MEMORY {
	NvU32 contiguous;
	NvU64 dma_addr;		  /* first physical address of given mapping,
				     machine address on Xen */
	NvU64 virtual_addr;   /* virtual address of given mapping */
	NvU32 mapping_length; /* tells how many bytes were mapped */

	/* helps to unmap noncontiguous memory, NULL for contiguous */
	struct SYS_MEM_MODS_INFO *p_mem_info;

	struct list_head   list;
};

/* functions used to avoid global debug variables */
int mods_check_debug_level(int);
int mods_get_mem4g(void);
int mods_get_highmem4g(void);
void mods_set_highmem4g(int);
int mods_get_multi_instance(void);
int mods_get_mem4goffset(void);

#define IRQ_MAX			(256+PCI_IRQ_MAX)
#define PCI_IRQ_MAX		15
#define MODS_CHANNEL_MAX	32

#define IRQ_VAL_POISON		0xfafbfcfdU

/* debug print masks */
#define DEBUG_IOCTL		0x2
#define DEBUG_PCICFG		0x4
#define DEBUG_ACPI		0x8
#define DEBUG_ISR		0x10
#define DEBUG_MEM		0x20
#define DEBUG_FUNC		0x40
#define DEBUG_CLOCK		0x80
#define DEBUG_DETAILED		0x100
#define DEBUG_ISR_DETAILED	(DEBUG_ISR | DEBUG_DETAILED)
#define DEBUG_MEM_DETAILED	(DEBUG_MEM | DEBUG_DETAILED)

#define LOG_ENT() mods_debug_printk(DEBUG_FUNC, "> %s\n", __func__)
#define LOG_EXT() mods_debug_printk(DEBUG_FUNC, "< %s\n", __func__)
#define LOG_ENT_C(format, args...) \
	mods_debug_printk(DEBUG_FUNC, "> %s: " format, __func__, ##args)
#define LOG_EXT_C(format, args...) \
	mods_debug_printk(DEBUG_FUNC, "< %s: " format, __func__, ##args)

#define mods_debug_printk(level, fmt, args...)\
	({ \
		if (mods_check_debug_level(level)) \
			pr_info("mods debug: " fmt, ##args); \
	})

#define mods_info_printk(fmt, args...)\
	pr_info("mods: " fmt, ##args)

#define mods_error_printk(fmt, args...)\
	pr_info("mods error: " fmt, ##args)

#define mods_warning_printk(fmt, args...)\
	pr_info("mods warning: " fmt, ##args)

struct irq_q_data {
	NvU32		time;
	struct pci_dev *dev;
	NvU32		irq;
};

struct irq_q_info {
	struct irq_q_data data[MODS_MAX_IRQS];
	NvU32		  head;
	NvU32		  tail;
};

struct dev_irq_map {
	void		*dev_irq_aperture;
	NvU32		*dev_irq_mask_reg;
	NvU32		*dev_irq_state;
	NvU32		 irq_and_mask;
	NvU32		 irq_or_mask;
	NvU32		 apic_irq;
	NvU8		 type;
	NvU8		 channel;
	struct pci_dev	*dev;
	struct list_head list;
};

struct mods_priv {
	/* map info from pci irq to apic irq */
	struct list_head  irq_head[MODS_CHANNEL_MAX];

	/* bits map for each allocated id. Each mods has an id. */
	/* the design is to take  into	account multi mods. */
	unsigned long	  channel_flags;

	/* fifo loop queue */
	struct irq_q_info rec_info[MODS_CHANNEL_MAX];
	spinlock_t	  lock;
};

/* ************************************************************************* */
/* ************************************************************************* */
/* **									     */
/* ** SYSTEM CALLS							     */
/* **									     */
/* ************************************************************************* */
/* ************************************************************************* */

/* MEMORY */
#define MODS_KMALLOC(ptr, size)						  \
	{								  \
		(ptr) = kmalloc(size, GFP_KERNEL);			  \
		MODS_ALLOC_RECORD(ptr, size, "km_alloc");		  \
	}

#define MODS_KMALLOC_ATOMIC(ptr, size)					  \
	{								  \
		(ptr) = kmalloc(size, GFP_ATOMIC);			  \
		MODS_ALLOC_RECORD(ptr, size, "km_alloc_atomic");	  \
	}

#define MODS_KFREE(ptr, size)						  \
	{								  \
		MODS_FREE_RECORD(ptr, size, "km_free");			  \
		kfree((void *) (ptr));					  \
	}

#define MODS_ALLOC_RECORD(ptr, size, name)				  \
	{if (ptr != NULL) {						  \
		mods_add_mem(ptr, size, __FILE__, __LINE__);		  \
	} }

#define MODS_FREE_RECORD(ptr, size, name)				  \
	{if (ptr != NULL) {						  \
		mods_del_mem(ptr, size, __FILE__, __LINE__);		  \
	} }

#define MEMDBG_ALLOC(a, b)	 (a = kmalloc(b, GFP_ATOMIC))
#define MEMDBG_FREE(a)		 (kfree(a))
#define MODS_FORCE_KFREE(ptr)	 (kfree(ptr))

#define __MODS_ALLOC_PAGES(page, order, gfp_mask, numa_node)		  \
	{								  \
		(page) = alloc_pages_node(numa_node, gfp_mask, order);	  \
	}

#define __MODS_FREE_PAGES(page, order)					  \
	{								  \
		__free_pages(page, order);				  \
	}

#ifndef MODS_HAS_SET_MEMORY
#	define MODS_SET_MEMORY_UC(addr, pages) \
	       change_page_attr(virt_to_page(addr), pages, PAGE_KERNEL_NOCACHE)
#	define MODS_SET_MEMORY_WC MODS_SET_MEMORY_UC
#	define MODS_SET_MEMORY_WB(addr, pages) \
	       change_page_attr(virt_to_page(addr), pages, PAGE_KERNEL)
#elif defined(CONFIG_ARCH_TEGRA) && !defined(CONFIG_CPA) && \
	  !defined(CONFIG_ARCH_TEGRA_3x_SOC)
#	define MODS_SET_MEMORY_UC(addr, pages) 0
#	define MODS_SET_MEMORY_WC(addr, pages) 0
#	define MODS_SET_MEMORY_WB(addr, pages) 0
#else
#	define MODS_SET_MEMORY_UC(addr, pages) set_memory_uc(addr, pages)
#	ifdef MODS_HAS_WC
#		define MODS_SET_MEMORY_WC(addr, pages)\
		       set_memory_wc(addr, pages)
#	else
#		define MODS_SET_MEMORY_WC(addr, pages)\
		       MODS_SET_MEMORY_UC(addr, pages)
#	endif
#	define MODS_SET_MEMORY_WB(addr, pages) set_memory_wb(addr, pages)
#endif

#define MODS_PGPROT_UC pgprot_noncached
#ifdef MODS_HAS_WC
#	define MODS_PGPROT_WC pgprot_writecombine
#else
#	define MODS_PGPROT_WC pgprot_noncached
#endif

/* VMA */
#define MODS_VMA_PGOFF(vma)	((vma)->vm_pgoff)
#define MODS_VMA_SIZE(vma)	((vma)->vm_end - (vma)->vm_start)
#define MODS_VMA_OFFSET(vma)	(((NvU64)(vma)->vm_pgoff) << PAGE_SHIFT)
#define MODS_VMA_PRIVATE(vma)	((vma)->vm_private_data)
#define MODS_VMA_FILE(vma)	((vma)->vm_file)

/* Xen adds a translation layer between the physical address
 * and real system memory address space.
 *
 * To illustrate if a PC has 2 GBs of RAM and each VM is given 1GB, then:
 * for guest OS in domain 0, physical address = machine address;
 * for guest OS in domain 1, physical address x = machine address 1GB+x
 *
 * In reality even domain's 0 physical address is not equal to machine
 * address and the mappings are not continuous.
 */

#if defined(CONFIG_XEN) && !defined(CONFIG_PARAVIRT)
	#define MODS_PHYS_TO_DMA(phys_addr) phys_to_machine(phys_addr)
	#define MODS_DMA_TO_PHYS(dma_addr)  machine_to_phys(dma_addr)
#else
	#define MODS_PHYS_TO_DMA(phys_addr) (phys_addr)
	#define MODS_DMA_TO_PHYS(dma_addr)  (dma_addr)
#endif

/* PCI */
#define MODS_PCI_GET_SLOT(mybus, devfn)					     \
({									     \
	struct pci_dev *__dev = NULL;					     \
	while ((__dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, __dev))) {    \
		if (__dev->bus->number == mybus				     \
		    && __dev->devfn == devfn)				     \
			break;						     \
	}								     \
	__dev;								     \
})

/* ACPI */
#ifdef MODS_HAS_NEW_ACPI_WALK
#define MODS_ACPI_WALK_NAMESPACE(type, start_object, max_depth, user_function, \
				 context, return_value)\
	acpi_walk_namespace(type, start_object, max_depth, user_function, NULL,\
			    context, return_value)
#else
#define MODS_ACPI_WALK_NAMESPACE acpi_walk_namespace
#endif

/* FILE */
#define MODS_PRIVATE_DATA(var, fp) \
	struct mods_file_private_data *var = (fp)->private_data
#define MODS_GET_FILE_PRIVATE_ID(fp) (((struct mods_file_private_data *)(fp) \
				      ->private_data)->mods_id)

/* ************************************************************************* */
/* ** MODULE WIDE FUNCTIONS						     */
/* ************************************************************************* */

/* irq */
void mods_init_irq(void);
void mods_cleanup_irq(void);
unsigned char mods_alloc_channel(void);
void mods_free_channel(unsigned char);
void mods_irq_dev_clr_pri(unsigned char);
void mods_irq_dev_set_pri(unsigned char id, void *pri);
int mods_irq_event_check(unsigned char);

/* mem */
void mods_init_mem(void);
void mods_add_mem(void *, NvU32, const char *, NvU32);
void mods_del_mem(void *, NvU32, const char *, NvU32);
void mods_check_mem(void);
void mods_unregister_all_alloc(struct file *fp);
struct SYS_MEM_MODS_INFO *mods_find_alloc(struct file *, NvU64);

/* clock */
#ifdef CONFIG_ARCH_TEGRA
void mods_init_clock_api(void);
void mods_shutdown_clock_api(void);
#endif

/* ioctl hanndlers */

/* mem */
int esc_mods_alloc_pages(struct file *, struct MODS_ALLOC_PAGES *);
int esc_mods_device_alloc_pages(struct file *,
			       struct MODS_DEVICE_ALLOC_PAGES *);
int esc_mods_free_pages(struct file *, struct MODS_FREE_PAGES *);
int esc_mods_set_mem_type(struct file *, struct MODS_MEMORY_TYPE *);
int esc_mods_get_phys_addr(struct file *,
			  struct MODS_GET_PHYSICAL_ADDRESS *);
int esc_mods_virtual_to_phys(struct file *,
			    struct MODS_VIRTUAL_TO_PHYSICAL *);
int esc_mods_phys_to_virtual(struct file *,
			    struct MODS_PHYSICAL_TO_VIRTUAL *);
int esc_mods_memory_barrier(struct file *);
/* acpi */
#ifdef CONFIG_ACPI
int esc_mods_eval_acpi_method(struct file *,
			     struct MODS_EVAL_ACPI_METHOD *);
int esc_mods_eval_dev_acpi_method(struct file *,
				 struct MODS_EVAL_DEV_ACPI_METHOD *);
int esc_mods_acpi_get_ddc(struct file *, struct MODS_ACPI_GET_DDC *);
#endif
/* pci */
#ifdef CONFIG_PCI
int esc_mods_find_pci_dev(struct file *, struct MODS_FIND_PCI_DEVICE *);
int esc_mods_find_pci_class_code(struct file *,
				struct MODS_FIND_PCI_CLASS_CODE *);
int esc_mods_pci_read(struct file *, struct MODS_PCI_READ *);
int esc_mods_pci_write(struct file *, struct MODS_PCI_WRITE *);
int esc_mods_pci_bus_add_dev(struct file *,
			    struct MODS_PCI_BUS_ADD_DEVICES *);
int esc_mods_pio_read(struct file *, struct MODS_PIO_READ *);
int esc_mods_pio_write(struct file *, struct MODS_PIO_WRITE  *);
int esc_mods_device_numa_info(struct file *,
			     struct MODS_DEVICE_NUMA_INFO  *);
#endif
/* irq */
int esc_mods_register_irq(struct file *, struct MODS_REGISTER_IRQ *);
int esc_mods_unregister_irq(struct file *, struct MODS_REGISTER_IRQ *);
int esc_mods_query_irq(struct file *, struct MODS_QUERY_IRQ *);
int esc_mods_set_irq_mask(struct file *, struct MODS_SET_IRQ_MASK *);
int esc_mods_irq_handled(struct file *, struct MODS_REGISTER_IRQ *);
/* clock */
#ifdef CONFIG_ARCH_TEGRA
int esc_mods_get_clock_handle(struct file *,
			     struct MODS_GET_CLOCK_HANDLE *);
int esc_mods_set_clock_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_get_clock_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_get_clock_max_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_set_clock_max_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_set_clock_parent(struct file *, struct MODS_CLOCK_PARENT *);
int esc_mods_get_clock_parent(struct file *, struct MODS_CLOCK_PARENT *);
int esc_mods_enable_clock(struct file *, struct MODS_CLOCK_HANDLE *);
int esc_mods_disable_clock(struct file *, struct MODS_CLOCK_HANDLE *);
int esc_mods_is_clock_enabled(struct file *pfile,
			     struct MODS_CLOCK_ENABLED *p);
int esc_mods_clock_reset_assert(struct file *,
			       struct MODS_CLOCK_HANDLE *);
int esc_mods_clock_reset_deassert(struct file *,
				 struct MODS_CLOCK_HANDLE *);
int esc_mods_flush_cpu_cache_range(struct file *,
				  struct MODS_FLUSH_CPU_CACHE_RANGE *);
#endif

#endif	/* _MODS_INTERNAL_H_  */
