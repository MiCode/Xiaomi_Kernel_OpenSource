/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __MEMORY_ACCESS_H_INCLUDED__
#define __MEMORY_ACCESS_H_INCLUDED__

/*!
 * \brief
 * Define the public interface for virtual memory
 * access functions. Access types are limited to
 * those defined in <stdint.h>
 *
 * The address representation is private to the system
 * and represented as "hrt_vaddress" rather than a
 * pointer, as the memory allocation cannot be accessed
 * by dereferencing but reaquires load and store access
 * functions
 *
 * The page table selection or virtual memory context;
 * The page table base index; Is implicit. This page
 * table base index must be set by the implementation
 * of the access function
 *
 * "store" is a transfer to the system
 * "load" is a transfer from the system
 *
 * Allocation properties can be specified by setting
 * attributes (see below) in case of multiple physical
 * memories the memory ID is encoded on the attribute
 *
 * Allocations in the same physical memory, but in a
 * different (set of) page tables can be shared through
 * a page table information mapping function
 */

#include <type_support.h>

/*
 * User provided file that defines the (sub)system address types:
 *	- hrt_vaddress	a type that can hold the (sub)system virtual address range
 */
#include "system_types.h"

/*
 * The MMU base address is a physical address, thus the same type is used
 * as for the device base address
 */
#include "device_access.h"

/*!
 * \brief
 * Bit masks for specialised allocation functions
 * the default is "uncached", "not contiguous",
 * "not page aligned" and "not cleared"
 *
 * Forcing alignment (usually) returns a pointer
 * at an alignment boundary that is offset from
 * the allocated pointer. Without storing this
 * pointer/offset, we cannot free it. The memory
 * manager is responsible for the bookkeeping, e.g.
 * the allocation function creates a sentinel
 * within the allocation referencable from the
 * returned pointer/address.
 */
#define MMGR_ATTRIBUTE_MASK			0x000f
#define MMGR_ATTRIBUTE_CACHED		0x0001
#define MMGR_ATTRIBUTE_CONTIGUOUS	0x0002
#define MMGR_ATTRIBUTE_PAGEALIGN	0x0004
#define MMGR_ATTRIBUTE_CLEARED		0x0008
#define MMGR_ATTRIBUTE_UNUSED		0xfff0

/* #define MMGR_ATTRIBUTE_DEFAULT	(MMGR_ATTRIBUTE_CACHED) */
#define MMGR_ATTRIBUTE_DEFAULT	0

extern const hrt_vaddress	mmgr_NULL;
extern const hrt_vaddress	mmgr_EXCEPTION;

/*! Set the (sub)system virtual memory page table base address

 \param	base_addr[in]		The address where page table 0 is located

 \Note: The base_addr is an absolute system address, thus it is not
        relative to the DDR base address

 \return none,
 */
extern void mmgr_set_base_address(
	const sys_address		base_addr);

/*! Get the (sub)system virtual memory page table base address

 \return base_address,
 */
/* unused */
extern sys_address mmgr_get_base_address(void);


/*! Set the (sub)system virtual memory page table base index

 \param	base_addr[in]		The index  where page table 0 is located

 \Note: The base_index is the MSB section of an absolute system address,
        the in-page address bits are discared. The base address is not
		relative to the DDR base address

 \return none,
 */
/* unused */
extern void mmgr_set_base_index(
	const hrt_data			base_index);

/*! Get the (sub)system virtual memory page table base index

 \return base_address,
 */
/* unused */
extern hrt_data mmgr_get_base_index(void);

/*! Return the address of an allocation in memory

 \param	size[in]			Size in bytes of the allocation

 \return vaddress
 */
extern hrt_vaddress mmgr_malloc(
	const size_t			size);

/*! Return the address of a zero initialised allocation in memory

 \param	size[in]			Size in bytes of the allocation

 \return vaddress
 */
extern hrt_vaddress mmgr_calloc(
	const size_t			N,
	const size_t			size);

/*! Return the address of a reallocated allocation in memory

 \param	vaddr[in]			Address of an allocation
 \param	size[in]			Size in bytes of the allocation

 \Note
	All limitations and particularities of the C stdlib
	realloc function apply

 \return vaddress
 */
/* unused */
extern hrt_vaddress mmgr_realloc(
	hrt_vaddress			vaddr,
	const size_t			size);

/*! Free the memory allocation identified by the address

 \param	vaddr[in]			Address of the allocation

 \return vaddress
 */
extern void mmgr_free(
	hrt_vaddress			vaddr);

/*! Return the address of an allocation in memory

 \param	size[in]			Size in bytes of the allocation
 \param	attribute[in]		Bit vector specifying the properties
							of the allocation including zero
							initialisation

 \return vaddress
 */
extern hrt_vaddress mmgr_alloc_attr(
	const size_t			size,
	const uint16_t			attribute);

/*! Return the address of a reallocated allocation in memory

 \param	vaddr[in]			Address of an allocation
 \param	size[in]			Size in bytes of the allocation
 \param	attribute[in]		Bit vector specifying the properties
							of the allocation

 \Note
	All limitations and particularities of the C stdlib
	realloc function apply

 \return vaddress
 */
/* unused */
extern hrt_vaddress mmgr_realloc_attr(
	hrt_vaddress			vaddr,
	const size_t			size,
	const uint16_t			attribute);

/*! Return the address of a mapped existing allocation in memory

 \param	ptr[in]				Pointer to an allocation in a different
					virtual memory page table, but the same
					physical memory
 \param size[in]			Size of the memory of the pointer
 \param	attribute[in]			Bit vector specifying the properties
					of the allocation
 \param context				Pointer of a context provided by
					client/driver for additonal parameters
					needed by the implementation
 \Note
	This interface is tentative, limited to the desired function
	the actual interface may require furhter parameters

 \return vaddress
 */
extern hrt_vaddress mmgr_mmap(
	const void *ptr,
	const size_t size,
	uint16_t attribute,
	void *context);

/*! Zero initialise an allocation in memory

 \param	vaddr[in]			Address of an allocation
 \param	size[in]			Size in bytes of the area to be cleared

 \return none
 */
extern void mmgr_clear(
	hrt_vaddress			vaddr,
	const size_t			size);

/*! Set an allocation in memory to a value

 \param	vaddr[in]			Address of an allocation
 \param	data[in]			Value to set
 \param	size[in]			Size in bytes of the area to be set

 \return none
 */
/* unused */
extern void mmgr_set(
	hrt_vaddress			vaddr,
	const uint8_t			data,
	const size_t			size);

/*! Read an array of bytes from a virtual memory address

 \param	vaddr[in]			Address of an allocation
 \param	data[out]			pointer to the destination array
 \param	size[in]			number of bytes to read

 \return none
 */
extern void mmgr_load(
	const hrt_vaddress		vaddr,
	void					*data,
	const size_t			size);

/*! Write an array of bytes to device registers or memory in the device

 \param	vaddr[in]			Address of an allocation
 \param	data[in]			pointer to the source array
 \param	size[in]			number of bytes to write

 \return none
 */
extern void mmgr_store(
	const hrt_vaddress		vaddr,
	const void				*data,
	const size_t			size);

#endif /* __MEMORY_ACCESS_H_INCLUDED__ */
