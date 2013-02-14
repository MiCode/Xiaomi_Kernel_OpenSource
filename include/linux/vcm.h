/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _VCM_H_
#define _VCM_H_

/* All undefined types must be defined using platform specific headers */

#include <linux/vcm_types.h>

/*
 * Virtual contiguous memory (VCM) region primitives.
 *
 * Current memory mapping software uses a CPU centric management
 * model. This makes sense in general, average hardware only contains an
 * CPU MMU and possibly a graphics MMU. If every device in the system
 * has one or more MMUs a CPU centric MM programming model breaks down.
 *
 * Looking at mapping from a system-wide perspective reveals a general
 * graph problem. Each node that talks to memory, either through an MMU
 * or directly (via physical memory) can be thought of as the device end
 * of a mapping edge. The other edge is the physical memory that is
 * mapped.
 *
 * In the direct mapped case, it is useful to give the device an
 * MMU. This one-to-one MMU allows direct mapped devices to
 * participate in graph management, they simply see memory through a
 * one-to-one mapping.
 *
 * The CPU nodes can also be brought under the same mapping
 * abstraction with the use of a light overlay on the existing
 * VMM. This light overlay brings the VMM's page table abstraction for
 * each process and the kernel into the graph management API.
 *
 * Taken together this system wide approach provides a capability that
 * is greater than the sum of its parts by allowing users to reason
 * about system wide mapping issues without getting bogged down in CPU
 * centric device page table management issues.
 */


/*
 * Creating, freeing and managing VCMs.
 *
 * A VCM region is a virtual space that can be reserved from and
 * associated with one or more devices. At creation the user can
 * specify an offset to start addresses and a length of the entire VCM
 * region. Reservations out of a VCM region are always contiguous.
 */

/**
 * vcm_create() - Create a VCM region
 * @start_addr:		The starting address of the VCM region.
 * @len:		The len of the VCM region. This must be at least
 *			vcm_get_min_page_size() bytes.
 *
 * A VCM typically abstracts a page table.
 *
 * All functions in this API are passed and return opaque things
 * because the underlying implementations will vary. The goal
 * is really graph management. vcm_create() creates the "device end"
 * of an edge in the mapping graph.
 *
 * The return value is non-zero if a VCM has successfully been
 * created. It will return zero if a VCM region cannot be created or
 * len is invalid.
 */
struct vcm *vcm_create(unsigned long start_addr, size_t len);


/**
 * vcm_create_from_prebuilt() - Create a VCM region from an existing region
 * @ext_vcm_id:		An external opaque value that allows the
 *			implementation to reference an already built table.
 *
 * The ext_vcm_id will probably reference a page table that's been built
 * by the VM.
 *
 * The platform specific implementation will provide this.
 *
 * The return value is non-zero if a VCM has successfully been created.
 */
struct vcm *vcm_create_from_prebuilt(size_t ext_vcm_id);


/**
 * vcm_clone() - Clone a VCM
 * @vcm:		A VCM to clone from.
 *
 * Perform a VCM "deep copy." The resulting VCM will match the original at
 * the point of cloning. Subsequent updates to either VCM will only be
 * seen by that VCM.
 *
 * The return value is non-zero if a VCM has been successfully cloned.
 */
struct vcm *vcm_clone(struct vcm *vcm);


/**
 * vcm_get_start_addr() - Get the starting address of the VCM region.
 * @vcm:		The VCM we're interested in getting the starting
 *			address of.
 *
 * The return value will be 1 if an error has occurred.
 */
size_t vcm_get_start_addr(struct vcm *vcm);


/**
 * vcm_get_len() - Get the length of the VCM region.
 * @vcm:		The VCM we're interested in reading the length from.
 *
 * The return value will be non-zero for a valid VCM. VCM regions
 * cannot have 0 len.
 */
size_t vcm_get_len(struct vcm *vcm);


/**
 * vcm_free() - Free a VCM.
 * @vcm:		The VCM we're interested in freeing.
 *
 * The return value is 0 if the VCM has been freed or:
 * -EBUSY		The VCM region contains reservations or has been
 *			associated (active or not) and cannot be freed.
 * -EINVAL		The vcm argument is invalid.
 */
int vcm_free(struct vcm *vcm);


/*
 * Creating, freeing and managing reservations out of a VCM.
 *
 */

/**
 * vcm_reserve() - Create a reservation from a VCM region.
 * @vcm:		The VCM region to reserve from.
 * @len:		The length of the reservation. Must be at least
 *			vcm_get_min_page_size() bytes.
 * @attr:		See 'Reservation Attributes'.
 *
 * A reservation, res_t, is a contiguous range from a VCM region.
 *
 * The return value is non-zero if a reservation has been successfully
 * created. It is 0 if any of the parameters are invalid.
 */
struct res *vcm_reserve(struct vcm *vcm, size_t len, u32 attr);


/**
 * vcm_reserve_at() - Make a reservation at a given logical location.
 * @memtarget:		A logical location to start the reservation from.
 * @vcm:		The VCM region to start the reservation from.
 * @len:		The length of the reservation.
 * @attr:		See 'Reservation Attributes'.
 *
 * The return value is non-zero if a reservation has been successfully
 * created.
 */
struct res *vcm_reserve_at(enum memtarget_t memtarget, struct vcm *vcm,
			   size_t len, u32 attr);


/**
 * vcm_get_vcm_from_res() - Return the VCM region of a reservation.
 * @res:		The reservation to return the VCM region of.
 *
 * Te return value will be non-zero if the reservation is valid. A valid
 * reservation is always associated with a VCM region; there is no such
 * thing as an orphan reservation.
 */
struct vcm *vcm_get_vcm_from_res(struct res *res);


/**
 * vcm_unreserve() - Unreserve the reservation.
 * @res:		The reservation to unreserve.
 *
 * The return value will be 0 if the reservation was successfully
 * unreserved and:
 * -EBUSY		The reservation is still backed,
 * -EINVAL		The vcm argument is invalid.
 */
int vcm_unreserve(struct res *res);


/**
 * vcm_set_res_attr() - Set attributes of an existing reservation.
 * @res:		An existing reservation of interest.
 * @attr:		See 'Reservation Attributes'.
 *
 * This function can only be used on an existing reservation; there
 * are no orphan reservations. All attributes can be set on a existing
 * reservation.
 *
 * The return value will be 0 for a success, otherwise it will be:
 * -EINVAL		res or attr are invalid.
 */
int vcm_set_res_attr(struct res *res, u32 attr);


/**
 * vcm_get_num_res() - Return the number of reservations in a VCM region.
 * @vcm:		The VCM region of interest.
 */
size_t vcm_get_num_res(struct vcm *vcm);


/**
 * vcm_get_next_res() - Read each reservation one at a time.
 * @vcm:		The VCM region of interest.
 * @res:		Contains the last reservation. Pass NULL on the
 *			first call.
 *
 * This function works like a foreach reservation in a VCM region.
 *
 * The return value will be non-zero for each reservation in a VCM. A
 * zero indicates no further reservations.
 */
struct res *vcm_get_next_res(struct vcm *vcm, struct res *res);


/**
 * vcm_res_copy() - Copy len bytes from one reservation to another.
 * @to:			The reservation to copy to.
 * @from:		The reservation to copy from.
 * @len:		The length of bytes to copy.
 *
 * The return value is the number of bytes copied.
 */
size_t vcm_res_copy(struct res *to, size_t to_off, struct res *from, size_t
		    from_off, size_t len);


/**
 * vcm_get_min_page_size() - Return the minimum page size supported by
 *			     the architecture.
 */
size_t vcm_get_min_page_size(void);


/**
 * vcm_back() - Physically back a reservation.
 * @res:		The reservation containing the virtual contiguous
 *			region to back.
 * @physmem:		The physical memory that will back the virtual
 *			contiguous memory region.
 *
 * One VCM can be associated with multiple devices. When you vcm_back()
 * each association must be active. This is not strictly necessary. It may
 * be changed in the future.
 *
 * This function returns 0 on a successful physical backing. Otherwise
 * it returns:
 * -EINVAL		res or physmem is invalid or res's len
 *			is different from physmem's len.
 * -EAGAIN		Try again, one of the devices hasn't been activated.
 */
int vcm_back(struct res *res, struct physmem *physmem);


/**
 * vcm_unback() - Unback a reservation.
 * @res:		The reservation to unback.
 *
 * One VCM can be associated with multiple devices. When you vcm_unback()
 * each association must be active.
 *
 * This function returns 0 on a successful unbacking. Otherwise
 * it returns:
 * -EINVAL		res is invalid.
 * -EAGAIN		Try again, one of the devices hasn't been activated.
 */
int vcm_unback(struct res *res);


/**
 * vcm_phys_alloc() - Allocate physical memory for the VCM region.
 * @memtype:		The memory type to allocate.
 * @len:		The length of the allocation.
 * @attr:		See 'Physical Allocation Attributes'.
 *
 * This function will allocate chunks of memory according to the attr
 * it is passed.
 *
 * The return value is non-zero if physical memory has been
 * successfully allocated.
 */
struct physmem *vcm_phys_alloc(enum memtype_t memtype, size_t len, u32 attr);


/**
 * vcm_phys_free() - Free a physical allocation.
 * @physmem:		The physical allocation to free.
 *
 * The return value is 0 if the physical allocation has been freed or:
 * -EBUSY		Their are reservation mapping the physical memory.
 * -EINVAL		The physmem argument is invalid.
 */
int vcm_phys_free(struct physmem *physmem);


/**
 * vcm_get_physmem_from_res() - Return a reservation's physmem
 * @res:		An existing reservation of interest.
 *
 * The return value will be non-zero on success, otherwise it will be:
 * -EINVAL		res is invalid
 * -ENOMEM		res is unbacked
 */
struct physmem *vcm_get_physmem_from_res(struct res *res);


/**
 * vcm_get_memtype_of_physalloc() - Return the memtype of a reservation.
 * @physmem:		The physical allocation of interest.
 *
 * This function returns the memtype of a reservation or VCM_INVALID
 * if res is invalid.
 */
enum memtype_t vcm_get_memtype_of_physalloc(struct physmem *physmem);


/*
 * Associate a VCM with a device, activate that association and remove it.
 *
 */

/**
 * vcm_assoc() - Associate a VCM with a device.
 * @vcm:		The VCM region of interest.
 * @dev:		The device to associate the VCM with.
 * @attr:		See 'Association Attributes'.
 *
 * This function returns non-zero if a association is made. It returns 0
 * if any of its parameters are invalid or VCM_ATTR_VALID is not present.
 */
struct avcm *vcm_assoc(struct vcm *vcm, struct device *dev, u32 attr);


/**
 * vcm_deassoc() - Deassociate a VCM from a device.
 * @avcm:		The association we want to break.
 *
 * The function returns 0 on success or:
 * -EBUSY		The association is currently activated.
 * -EINVAL		The avcm parameter is invalid.
 */
int vcm_deassoc(struct avcm *avcm);


/**
 * vcm_set_assoc_attr() - Set an AVCM's attributes.
 * @avcm:		The AVCM of interest.
 * @attr:		The new attr. See 'Association Attributes'.
 *
 * Every attribute can be set at runtime if an association isn't activated.
 *
 * This function returns 0 on success or:
 * -EBUSY		The association is currently activated.
 * -EINVAL		The avcm parameter is invalid.
 */
int vcm_set_assoc_attr(struct avcm *avcm, u32 attr);


/**
 * vcm_get_assoc_attr() - Return an AVCM's attributes.
 * @avcm:		The AVCM of interest.
 *
 * This function returns 0 on error.
 */
u32 vcm_get_assoc_attr(struct avcm *avcm);


/**
 * vcm_activate() - Activate an AVCM.
 * @avcm:		The AVCM to activate.
 *
 * You have to deactivate, before you activate.
 *
 * This function returns 0 on success or:
 * -EINVAL		avcm is invalid
 * -ENODEV		no device
 * -EBUSY		device is already active
 * -1			hardware failure
 */
int vcm_activate(struct avcm *avcm);


/**
 * vcm_deactivate() - Deactivate an association.
 * @avcm:		The AVCM to deactivate.
 *
 * This function returns 0 on success or:
 * -ENOENT		avcm is not activate
 * -EINVAL		avcm is invalid
 * -1			hardware failure
 */
int vcm_deactivate(struct avcm *avcm);


/**
 * vcm_is_active() - Query if an AVCM is active.
 * @avcm:		The AVCM of interest.
 *
 * returns 0 for not active, 1 for active or -EINVAL for error.
 *
 */
int vcm_is_active(struct avcm *avcm);


/*
 * Create, manage and remove a boundary in a VCM.
 */

/**
 * vcm_create_bound() - Create a bound in a VCM.
 * @vcm:		The VCM that needs a bound.
 * @len:		The len of the bound.
 *
 * The allocator picks the virtual addresses of the bound.
 *
 * This function returns non-zero if a bound was created.
 */
struct bound *vcm_create_bound(struct vcm *vcm, size_t len);


/**
 * vcm_free_bound() - Free a bound.
 * @bound:		The bound to remove.
 *
 * This function returns 0 if bound has been removed or:
 * -EBUSY		The bound contains reservations and cannot be removed.
 * -EINVAL		The bound is invalid.
 */
int vcm_free_bound(struct bound *bound);


/**
 * vcm_reserve_from_bound() - Make a reservation from a bounded area.
 * @bound:		The bound to reserve from.
 * @len:		The len of the reservation.
 * @attr:		See 'Reservation Attributes'.
 *
 * The return value is non-zero on success. It is 0 if any parameter
 * is invalid.
 */
struct res *vcm_reserve_from_bound(struct bound *bound, size_t len,
				   u32 attr);


/**
 * vcm_get_bound_start_addr() - Return the starting device address of the bound
 * @bound:		The bound of interest.
 *
 * On success this function returns the starting addres of the bound. On error
 * it returns:
 * 1			bound_id is invalid.
 */
size_t vcm_get_bound_start_addr(struct bound *bound);



/*
 * Perform low-level control over VCM regions and reservations.
 */

/**
 * vcm_map_phys_addr() - Produce a physmem from a contiguous
 *                       physical address
 *
 * @phys:		The physical address of the contiguous range.
 * @len:		The len of the contiguous address range.
 *
 * Returns non-zero on success, 0 on failure.
 */
struct physmem *vcm_map_phys_addr(phys_addr_t phys, size_t len);


/**
 * vcm_get_next_phys_addr() - Get the next physical addr and len of a physmem.
 * @physmem:		The physmem of interest.
 * @phys:		The current physical address. Set this to NULL to
 *			start the iteration.
 * @len			An output: the len of the next physical segment.
 *
 * physmems may contain physically discontiguous sections. This
 * function returns the next physical address and len. Pass NULL to
 * phys to get the first physical address. The len of the physical
 * segment is returned in *len.
 *
 * Returns 0 if there is no next physical address.
 */
size_t vcm_get_next_phys_addr(struct physmem *physmem, phys_addr_t phys,
			      size_t *len);


/**
 * vcm_get_dev_addr() - Return the device address of a reservation.
 * @res:		The reservation of interest.
 *
 *
 * On success this function returns the device address of a reservation. On
 * error it returns:
 * 1			res is invalid.
 *
 * Note: This may return a kernel address if the reservation was
 * created from vcm_create_from_prebuilt() and the prebuilt ext_vcm_id
 * references a VM page table.
 */
phys_addr_t vcm_get_dev_addr(struct res *res);


/**
 * vcm_get_res() - Return the reservation from a device address and a VCM
 * @dev_addr:		The device address of interest.
 * @vcm:		The VCM that contains the reservation
 *
 * This function returns 0 if there is no reservation whose device
 * address is dev_addr.
 */
struct res *vcm_get_res(unsigned long dev_addr, struct vcm *vcm);


/**
 * vcm_translate() - Translate from one device address to another.
 * @src_dev:		The source device address.
 * @src_vcm:		The source VCM region.
 * @dst_vcm:		The destination VCM region.
 *
 * Derive the device address from a VCM region that maps the same physical
 * memory as a device address from another VCM region.
 *
 * On success this function returns the device address of a translation. On
 * error it returns:
 * 1			res_id is invalid.
 */
size_t vcm_translate(struct device *src_dev, struct vcm *src_vcm,
		     struct vcm *dst_vcm);


/**
 * vcm_get_phys_num_res() - Return the number of reservations mapping a
 *			    physical address.
 * @phys:		The physical address to read.
 */
size_t vcm_get_phys_num_res(phys_addr_t phys);


/**
 * vcm_get_next_phys_res() - Return the next reservation mapped to a physical
 *			     address.
 * @phys:		The physical address to map.
 * @res:		The starting reservation. Set this to NULL for the first
 *			reservation.
 * @len:		The virtual length of the reservation
 *
 * This function returns 0 for the last reservation or no reservation.
 */
struct res *vcm_get_next_phys_res(phys_addr_t phys, struct res *res,
				  size_t *len);


/**
 * vcm_get_pgtbl_pa() - Return the physcial address of a VCM's page table.
 * @vcm:	The VCM region of interest.
 *
 * This function returns non-zero on success.
 */
phys_addr_t vcm_get_pgtbl_pa(struct vcm *vcm);


/**
 * vcm_get_cont_memtype_pa() - Return the phys base addr of a memtype's
 *			       first contiguous region.
 * @memtype:		The memtype of interest.
 *
 * This function returns non-zero on success. A zero return indicates that
 * the given memtype does not have a contiguous region or that the memtype
 * is invalid.
 */
phys_addr_t vcm_get_cont_memtype_pa(enum memtype_t memtype);


/**
 * vcm_get_cont_memtype_len() - Return the len of a memtype's
 *				first contiguous region.
 * @memtype:		The memtype of interest.
 *
 * This function returns non-zero on success. A zero return indicates that
 * the given memtype does not have a contiguous region or that the memtype
 * is invalid.
 */
size_t vcm_get_cont_memtype_len(enum memtype_t memtype);


/**
 * vcm_dev_addr_to_phys_addr() - Perform a device address page-table lookup.
 * @vcm:		VCM to use for translation.
 * @dev_addr:		The device address to map.
 *
 * This function returns the pa of a va from a device's page-table. It will
 * fault if the dev_addr is not mapped.
 */
phys_addr_t vcm_dev_addr_to_phys_addr(struct vcm *vcm, unsigned long dev_addr);


/*
 * Fault Hooks
 *
 * vcm_hook()
 */

/**
 * vcm_hook() - Add a fault handler.
 * @dev:		The device.
 * @handler:		The handler.
 * @data:		A private piece of data that will get passed to the
 *			handler.
 *
 * This function returns 0 for a successful registration or:
 * -EINVAL		The arguments are invalid.
 */
int vcm_hook(struct device *dev, vcm_handler handler, void *data);



/*
 * Low level, platform agnostic, HW control.
 *
 * vcm_hw_ver()
 */

/**
 * vcm_hw_ver() - Return the hardware version of a device, if it has one.
 * @dev		The device.
 */
size_t vcm_hw_ver(size_t dev);

#endif /* _VCM_H_ */

