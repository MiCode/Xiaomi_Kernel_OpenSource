/*
 * mods.h - This file is part of NVIDIA MODS kernel driver.
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

#ifndef _MODS_H_
#define _MODS_H_

/* Driver version */
#define MODS_DRIVER_VERSION_MAJOR 3
#define MODS_DRIVER_VERSION_MINOR 42
#define MODS_DRIVER_VERSION ((MODS_DRIVER_VERSION_MAJOR << 8) | \
			     ((MODS_DRIVER_VERSION_MINOR/10) << 4) | \
			     (MODS_DRIVER_VERSION_MINOR%10))

#pragma pack(1)

/* ************************************************************************* */
/* ** ESCAPE INTERFACE STRUCTURE					     */
/* ************************************************************************* */

struct mods_pci_dev {
	NvU16 bus;
	NvU8  device;
	NvU8  function;
};

/* MODS_ESC_ALLOC_PAGES */
struct MODS_ALLOC_PAGES {
	/* IN */
	NvU32	num_bytes;
	NvU32	contiguous;
	NvU32	address_bits;
	NvU32	attrib;

	/* OUT */
	NvU64	memory_handle;
};

/* MODS_ESC_DEVICE_ALLOC_PAGES */
struct MODS_DEVICE_ALLOC_PAGES {
	/* IN */
	NvU32		    num_bytes;
	NvU32		    contiguous;
	NvU32		    address_bits;
	NvU32		    attrib;
	struct mods_pci_dev pci_device;

	/* OUT */
	NvU64		    memory_handle;
};

/* MODS_ESC_FREE_PAGES */
struct MODS_FREE_PAGES {
	/* IN */
	NvU64	memory_handle;
};

/* MODS_ESC_GET_PHYSICAL_ADDRESS */
struct MODS_GET_PHYSICAL_ADDRESS {
	/* IN */
	NvU64	memory_handle;
	NvU32	offset;

	/* OUT */
	NvU64	physical_address;
};

/* MODS_ESC_VIRTUAL_TO_PHYSICAL */
struct MODS_VIRTUAL_TO_PHYSICAL {
	/* IN */
	NvU64	virtual_address;

	/* OUT */
	NvU64	physical_address;
};

/* MODS_ESC_PHYSICAL_TO_VIRTUAL */
struct MODS_PHYSICAL_TO_VIRTUAL {
	/* IN */
	NvU64	physical_address;

	/* OUT */
	NvU64	virtual_address;

};

/* MODS_ESC_FLUSH_CACHE_RANGE */
#define MODS_FLUSH_CPU_CACHE	  1
#define MODS_INVALIDATE_CPU_CACHE 2

struct MODS_FLUSH_CPU_CACHE_RANGE {
	/* IN */
	NvU64 virt_addr_start;
	NvU64 virt_addr_end;
	NvU32 flags;
};

/* MODS_ESC_FIND_PCI_DEVICE */
struct MODS_FIND_PCI_DEVICE {
	/* IN */
	NvU32	  device_id;
	NvU32	  vendor_id;
	NvU32	  index;

	/* OUT */
	NvU32	  bus_number;
	NvU32	  device_number;
	NvU32	  function_number;
};

/* MODS_ESC_FIND_PCI_CLASS_CODE */
struct MODS_FIND_PCI_CLASS_CODE {
	/* IN */
	NvU32	class_code;
	NvU32	index;

	/* OUT */
	NvU32	bus_number;
	NvU32	device_number;
	NvU32	function_number;
};

/* MODS_ESC_PCI_READ */
struct MODS_PCI_READ {
	/* IN */
	NvU32	bus_number;
	NvU32	device_number;
	NvU32	function_number;
	NvU32	address;
	NvU32	data_size;

	/* OUT */
	NvU32	 data;
};

/* MODS_ESC_PCI_WRITE */
struct MODS_PCI_WRITE {
	/* IN */
	NvU32	bus_number;
	NvU32	device_number;
	NvU32	function_number;
	NvU32	address;
	NvU32	data;
	NvU32	data_size;
};

/* MODS_ESC_PCI_BUS_ADD_DEVICES*/
struct MODS_PCI_BUS_ADD_DEVICES {
	/* IN */
	NvU32	 bus;
};

/* MODS_ESC_PIO_READ */
struct MODS_PIO_READ {
	/* IN */
	NvU16	port;
	NvU32	data_size;

	/* OUT */
	NvU32	data;
};

/* MODS_ESC_PIO_WRITE */
struct MODS_PIO_WRITE {
	/* IN */
	NvU16	port;
	NvU32	data;
	NvU32	data_size;
};

#define INQ_CNT 8

struct mods_irq_data {
	NvU32 irq;
	NvU32 delay;
};

struct mods_irq_status {
	struct mods_irq_data data[INQ_CNT];
	NvU32 irqbits:INQ_CNT;
	NvU32 otherirq:1;
};

/* MODS_ESC_IRQ */
struct MODS_IRQ {
	/* IN */
	NvU32 cmd;
	NvU32 size;		/* memory size */
	NvU32 irq;		/* the irq number to be registered in driver */

	/* IN OUT */
	NvU32 channel;		/* application id allocated by driver. */

	/* OUT */
	struct mods_irq_status stat;	/* for querying irq */
	NvU64		 phys;	/* the memory physical address */
};

/* MODS_ESC_REGISTER_IRQ */
/* MODS_ESC_UNREGISTER_IRQ */
struct MODS_REGISTER_IRQ {
	/* IN */
	struct mods_pci_dev dev;   /* device which generates the interrupt */
	NvU8		    type;  /* MODS_IRQ_TYPE_* */
};

struct mods_irq {
	NvU32		    delay; /* delay in ns between the irq occuring and
				      MODS querying for it */
	struct mods_pci_dev dev;   /* device which generated the interrupt */
};

#define MODS_MAX_IRQS 32

/* MODS_ESC_QUERY_IRQ */
struct MODS_QUERY_IRQ {
	/* OUT */
	struct mods_irq irq_list[MODS_MAX_IRQS];
	NvU8		more;  /* indicates that more interrupts are waiting */
};

#define MODS_IRQ_TYPE_INT  0
#define MODS_IRQ_TYPE_MSI  1
#define MODS_IRQ_TYPE_CPU  2

/* MODS_ESC_SET_IRQ_MASK */
struct MODS_SET_IRQ_MASK {
	/* IN */
	NvU64		    aperture_addr; /* physical address of aperture */
	NvU32		    aperture_size; /* size of the mapped region */
	NvU32		    reg_offset;	   /* offset of the irq mask register
					      within the aperture */
	NvU32		    and_mask;	   /* and mask for clearing bits in
					      the irq mask register */
	NvU32		    or_mask;	   /* or mask for setting bits in
					      the irq mask register */
	struct mods_pci_dev dev;	   /* device identifying interrupt for
					      which the mask will be applied */
	NvU8		    irq_type;	   /* irq type */
	NvU8		    mask_type;	   /* mask type */
};

#define MODS_MASK_TYPE_IRQ_DISABLE 0

#define ACPI_MODS_TYPE_INTEGER		1
#define ACPI_MODS_TYPE_BUFFER		2
#define ACPI_MAX_BUFFER_LENGTH		4096
#define ACPI_MAX_METHOD_LENGTH		12
#define ACPI_MAX_ARGUMENT_NUMBER	12

union ACPI_ARGUMENT {
	NvU32	type;

	struct {
		NvU32 type;
		NvU32 value;
	} integer;

	struct {
		NvU32	type;
		NvU32	length;
		NvU32	offset;
	} buffer;
};

/* MODS_ESC_EVAL_ACPI_METHOD */
struct MODS_EVAL_ACPI_METHOD {
	/* IN */
	char		    method_name[ACPI_MAX_METHOD_LENGTH];
	NvU32		    argument_count;
	union ACPI_ARGUMENT argument[ACPI_MAX_ARGUMENT_NUMBER];
	NvU8		    in_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN OUT */
	NvU32		    out_data_size;

	/* OUT */
	NvU8		    out_buffer[ACPI_MAX_BUFFER_LENGTH];
	NvU32		    out_status;
};

/* MODS_ESC_EVAL_DEV_ACPI_METHOD */
struct MODS_EVAL_DEV_ACPI_METHOD {
	/* IN OUT */
	struct MODS_EVAL_ACPI_METHOD method;

	/* IN */
	struct mods_pci_dev device;
};

/* MODS_ESC_ACPI_GET_DDC */
struct MODS_ACPI_GET_DDC {
	/* OUT */
	NvU32		    out_data_size;
	NvU8		    out_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN */
	struct mods_pci_dev device;
};

/* MODS_ESC_GET_VERSION */
struct MODS_GET_VERSION {
	/* OUT */
	NvU64 version;
};

/* MODS_ESC_SET_PARA */
struct MODS_SET_PARA {
	/* IN */
	NvU64 Highmem4g;
	NvU64 debug;
};

/* MODS_ESC_SET_MEMORY_TYPE */
struct MODS_MEMORY_TYPE {
	/* IN */
	NvU64 physical_address;
	NvU64 size;
	NvU32 type;
};

#define MAX_CLOCK_HANDLE_NAME 64

/* MODS_ESC_GET_CLOCK_HANDLE */
struct MODS_GET_CLOCK_HANDLE {
	/* OUT */
	NvU32 clock_handle;

	/* IN */
	char  device_name[MAX_CLOCK_HANDLE_NAME];
	char  controller_name[MAX_CLOCK_HANDLE_NAME];
};

/* MODS_ESC_SET_CLOCK_RATE, MODS_ESC_GET_CLOCK_RATE,
 * MODS_ESC_GET_CLOCK_MAX_RATE, MODS_ESC_SET_CLOCK_MAX_RATE */
struct MODS_CLOCK_RATE {
	/* IN/OUT */
	NvU64 clock_rate_hz;

	/* IN */
	NvU32 clock_handle;
};

/* MODS_ESC_SET_CLOCK_PARENT, MODS_ESC_GET_CLOCK_PARENT */
struct MODS_CLOCK_PARENT {
	/* IN */
	NvU32 clock_handle;

	/* IN/OUT */
	NvU32 clock_parent_handle;
};

/* MODS_ESC_ENABLE_CLOCK, MODS_ESC_DISABLE_CLOCK, MODS_ESC_CLOCK_RESET_ASSERT,
 * MODS_ESC_CLOCK_RESET_DEASSERT */
struct MODS_CLOCK_HANDLE {
	/* IN */
	NvU32 clock_handle;
};

/* MODS_ESC_IS_CLOCK_ENABLED */
struct MODS_CLOCK_ENABLED {
	/* IN */
	NvU32 clock_handle;

	/* OUT */
	NvU32 enable_count;
};

/* MODS_ESC_DEVICE_NUMA_INFO */
#define MAX_CPU_MASKS 32  /* 32 masks of 32bits = 1024 CPUs max */
struct MODS_DEVICE_NUMA_INFO {
	/* IN */
	struct mods_pci_dev pci_device;

	/* OUT */
	NvS32  node;
	NvU32  node_count;
	NvU32  node_cpu_mask[MAX_CPU_MASKS];
	NvU32  cpu_count;
};

/* The ids match MODS ids */
#define MODS_MEMORY_CACHED		5
#define MODS_MEMORY_UNCACHED		1
#define MODS_MEMORY_WRITECOMBINE	2

#pragma pack()

/* ************************************************************************* */
/* ************************************************************************* */
/* **									     */
/* ** ESCAPE CALLS							     */
/* **									     */
/* ************************************************************************* */
/* ************************************************************************* */
#define MODS_IOC_MAGIC	  'x'
#define MODS_ESC_ALLOC_PAGES			\
		   _IOWR(MODS_IOC_MAGIC, 0, struct MODS_ALLOC_PAGES)
#define MODS_ESC_FREE_PAGES				\
		   _IOWR(MODS_IOC_MAGIC, 1, struct MODS_FREE_PAGES)
#define MODS_ESC_GET_PHYSICAL_ADDRESS	\
		   _IOWR(MODS_IOC_MAGIC, 2, struct MODS_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_VIRTUAL_TO_PHYSICAL	\
		   _IOWR(MODS_IOC_MAGIC, 3, struct MODS_VIRTUAL_TO_PHYSICAL)
#define MODS_ESC_PHYSICAL_TO_VIRTUAL	\
		   _IOWR(MODS_IOC_MAGIC, 4, struct MODS_PHYSICAL_TO_VIRTUAL)
#define MODS_ESC_FIND_PCI_DEVICE		\
		   _IOWR(MODS_IOC_MAGIC, 5, struct MODS_FIND_PCI_DEVICE)
#define MODS_ESC_FIND_PCI_CLASS_CODE	\
		   _IOWR(MODS_IOC_MAGIC, 6, struct MODS_FIND_PCI_CLASS_CODE)
#define MODS_ESC_PCI_READ				\
		   _IOWR(MODS_IOC_MAGIC, 7, struct MODS_PCI_READ)
#define MODS_ESC_PCI_WRITE				\
		   _IOWR(MODS_IOC_MAGIC, 8, struct MODS_PCI_WRITE)
#define MODS_ESC_PIO_READ				\
		   _IOWR(MODS_IOC_MAGIC, 9, struct MODS_PIO_READ)
#define MODS_ESC_PIO_WRITE				\
		   _IOWR(MODS_IOC_MAGIC, 10, struct MODS_PIO_WRITE)
#define MODS_ESC_IRQ_REGISTER			\
		   _IOWR(MODS_IOC_MAGIC, 11, struct MODS_IRQ)
#define MODS_ESC_IRQ_FREE				\
		   _IOWR(MODS_IOC_MAGIC, 12, struct MODS_IRQ)
#define MODS_ESC_IRQ_INQUIRY			\
		   _IOWR(MODS_IOC_MAGIC, 13, struct MODS_IRQ)
#define MODS_ESC_EVAL_ACPI_METHOD		\
		   _IOWR(MODS_IOC_MAGIC, 16, struct MODS_EVAL_ACPI_METHOD)
#define MODS_ESC_GET_API_VERSION		\
		   _IOWR(MODS_IOC_MAGIC, 17, struct MODS_GET_VERSION)
#define MODS_ESC_GET_KERNEL_VERSION		\
		   _IOWR(MODS_IOC_MAGIC, 18, struct MODS_GET_VERSION)
#define MODS_ESC_SET_DRIVER_PARA		\
		   _IOWR(MODS_IOC_MAGIC, 19, struct MODS_SET_PARA)
#define MODS_ESC_MSI_REGISTER			\
		   _IOWR(MODS_IOC_MAGIC, 20, struct MODS_IRQ)
#define MODS_ESC_REARM_MSI				\
		   _IOWR(MODS_IOC_MAGIC, 21, struct MODS_IRQ)
#define MODS_ESC_SET_MEMORY_TYPE		\
		    _IOW(MODS_IOC_MAGIC, 22, struct MODS_MEMORY_TYPE)
#define MODS_ESC_PCI_BUS_ADD_DEVICES	\
		    _IOW(MODS_IOC_MAGIC, 23, struct MODS_PCI_BUS_ADD_DEVICES)
#define MODS_ESC_REGISTER_IRQ			\
		    _IOW(MODS_IOC_MAGIC, 24, struct MODS_REGISTER_IRQ)
#define MODS_ESC_UNREGISTER_IRQ			\
		    _IOW(MODS_IOC_MAGIC, 25, struct MODS_REGISTER_IRQ)
#define MODS_ESC_QUERY_IRQ				\
		    _IOR(MODS_IOC_MAGIC, 26, struct MODS_QUERY_IRQ)
#define MODS_ESC_EVAL_DEV_ACPI_METHOD	\
		   _IOWR(MODS_IOC_MAGIC, 27, struct MODS_EVAL_DEV_ACPI_METHOD)
#define MODS_ESC_ACPI_GET_DDC			\
		   _IOWR(MODS_IOC_MAGIC, 28, struct MODS_ACPI_GET_DDC)
#define MODS_ESC_GET_CLOCK_HANDLE		\
		   _IOWR(MODS_IOC_MAGIC, 29, struct MODS_GET_CLOCK_HANDLE)
#define MODS_ESC_SET_CLOCK_RATE			\
		    _IOW(MODS_IOC_MAGIC, 30, struct MODS_CLOCK_RATE)
#define MODS_ESC_GET_CLOCK_RATE			\
		   _IOWR(MODS_IOC_MAGIC, 31, struct MODS_CLOCK_RATE)
#define MODS_ESC_SET_CLOCK_PARENT		\
		    _IOW(MODS_IOC_MAGIC, 32, struct MODS_CLOCK_PARENT)
#define MODS_ESC_GET_CLOCK_PARENT		\
		   _IOWR(MODS_IOC_MAGIC, 33, struct MODS_CLOCK_PARENT)
#define MODS_ESC_ENABLE_CLOCK			\
		    _IOW(MODS_IOC_MAGIC, 34, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_DISABLE_CLOCK			\
		    _IOW(MODS_IOC_MAGIC, 35, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_IS_CLOCK_ENABLED		\
		   _IOWR(MODS_IOC_MAGIC, 36, struct MODS_CLOCK_ENABLED)
#define MODS_ESC_CLOCK_RESET_ASSERT		\
		    _IOW(MODS_IOC_MAGIC, 37, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_CLOCK_RESET_DEASSERT	\
		    _IOW(MODS_IOC_MAGIC, 38, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_SET_IRQ_MASK			\
		    _IOW(MODS_IOC_MAGIC, 39, struct MODS_SET_IRQ_MASK)
#define MODS_ESC_MEMORY_BARRIER			\
		     _IO(MODS_IOC_MAGIC, 40)
#define MODS_ESC_IRQ_HANDLED			\
		    _IOW(MODS_IOC_MAGIC, 41, struct MODS_REGISTER_IRQ)
#define MODS_ESC_FLUSH_CPU_CACHE_RANGE	\
		    _IOW(MODS_IOC_MAGIC, 42, struct MODS_FLUSH_CPU_CACHE_RANGE)
#define MODS_ESC_GET_CLOCK_MAX_RATE		\
		   _IOWR(MODS_IOC_MAGIC, 43, struct MODS_CLOCK_RATE)
#define MODS_ESC_SET_CLOCK_MAX_RATE		\
		    _IOW(MODS_IOC_MAGIC, 44, struct MODS_CLOCK_RATE)
#define MODS_ESC_DEVICE_ALLOC_PAGES		\
		   _IOWR(MODS_IOC_MAGIC, 45, struct MODS_DEVICE_ALLOC_PAGES)
#define MODS_ESC_DEVICE_NUMA_INFO		\
		   _IOWR(MODS_IOC_MAGIC, 46, struct MODS_DEVICE_NUMA_INFO)

#endif /* _MODS_H_  */
