/*
 * include/linux/uio_driver.h
 *
 * Copyright(C) 2005, Benedikt Spranger <b.spranger@linutronix.de>
 * Copyright(C) 2005, Thomas Gleixner <tglx@linutronix.de>
 * Copyright(C) 2006, Hans J. Koch <hjk@hansjkoch.de>
 * Copyright(C) 2006, Greg Kroah-Hartman <greg@kroah.com>
 *
 * Userspace IO driver.
 *
 * Licensed under the GPLv2 only.
 */

#ifndef _EXM_DRIVER_H_
#define _EXM_DRIVER_H_

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>

struct module;
struct exm_map;

/**
 * struct exm_mem - description of a EXM memory region
 * @name:		name of the memory region for identification
 * @addr:		address of the device's memory (phys_addr is used since
 *			addr can be logical, virtual, or physical & phys_addr_t
 *			should always be large enough to handle any of the
 *			address types)
 * @size:		size of IO
 * @memtype:		type of memory addr points to
 * @internal_addr:	ioremap-ped version of addr, for driver internal use
 * @map:		for use by the EXM core only.
 */
struct exm_mem {
	const char *name;
	phys_addr_t addr;
	unsigned long size;
	int memtype;
	void __iomem *internal_addr;
	struct exm_map *map;
};

#define MAX_EXM_MAPS	5

struct exm_portio;

/**
 * struct exm_port - description of a EXM port region
 * @name:		name of the port region for identification
 * @start:		start of port region
 * @size:		size of port region
 * @porttype:		type of port (see EXM_PORT_* below)
 * @portio:		for use by the EXM core only.
 */
struct exm_port {
	const char *name;
	unsigned long start;
	unsigned long size;
	int porttype;
	struct exm_portio *portio;
};

#define MAX_EXM_PORT_REGIONS	5

struct exm_device;

/**
 * struct exm_info - EXM device capabilities
 * @exm_dev:		the EXM device this info belongs to
 * @name:		device name
 * @version:		device driver version
 * @mem:		list of mappable memory regions, size==0 for end of list
 * @port:		list of port regions, size==0 for end of list
 * @irq:		interrupt number or EXM_IRQ_CUSTOM
 * @irq_flags:		flags for request_irq()
 * @priv:		optional private data
 * @handler:		the device's irq handler
 * @mmap:		mmap operation for this exm device
 * @open:		open operation for this exm device
 * @release:		release operation for this exm device
 * @irqcontrol:		disable/enable irqs when 0/1 is written to /dev/exmX
 */
struct exm_info {
	struct exm_device *exm_dev;
	const char *name;
	const char *version;
	struct exm_mem mem[MAX_EXM_MAPS];
	struct exm_port port[MAX_EXM_PORT_REGIONS];
	long irq;
	unsigned long irq_flags;
	void *priv;
	irqreturn_t (*handler)(int irq, struct exm_info *dev_info);
	int (*mmap)(struct exm_info *info, struct vm_area_struct *vma);
	int (*open)(struct exm_info *info, struct inode *inode);
	int (*release)(struct exm_info *info, struct inode *inode);
	int (*irqcontrol)(struct exm_info *info, s32 irq_on);
};

extern int __must_check
__exm_register_device(struct module *owner, struct device *parent, struct exm_info *info);

/* use a define to avoid include chaining to get THIS_MODULE */
#define exm_register_device(parent, info) \
	__exm_register_device(THIS_MODULE, parent, info)

extern void exm_unregister_device(struct exm_info *info);
extern void exm_event_notify(struct exm_info *info);

/* defines for exm_info->irq */
#define EXM_IRQ_CUSTOM	-1
#define EXM_IRQ_NONE	0

/* defines for exm_mem->memtype */
#define EXM_MEM_NONE	0
#define EXM_MEM_PHYS	1
#define EXM_MEM_LOGICAL	2
#define EXM_MEM_VIRTUAL 3

/* defines for exm_port->porttype */
#define EXM_PORT_NONE	0
#define EXM_PORT_X86	1
#define EXM_PORT_GPIO	2
#define EXM_PORT_OTHER	3

extern bool extmem_in_mspace(struct vm_area_struct *vma);
extern unsigned long get_virt_from_mspace(unsigned long pa);
extern void *extmem_malloc(size_t bytes);
extern void *extmem_malloc_page_align(size_t bytes);
extern size_t extmem_get_mem_size(unsigned long pgoff);
extern void extmem_free(void *mem);
extern void init_debug_alloc_pool_aligned(void);

#endif				/* _LINUX_EXM_DRIVER_H_ */
