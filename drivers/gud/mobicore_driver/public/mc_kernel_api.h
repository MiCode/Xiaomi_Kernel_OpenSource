/** @addtogroup MCD_MCDIMPL_KMOD_KAPI Mobicore Driver Module API inside Kernel.
 * @ingroup  MCD_MCDIMPL_KMOD
 * @{
 * Interface to Mobicore Driver Kernel Module inside Kernel.
 * @file
 *
 * Interface to be used by module MobiCoreKernelAPI.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2010-2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MOBICORE_KERNELMODULE_API_H_
#define _MOBICORE_KERNELMODULE_API_H_

struct mc_instance;

/**
 * Initialize a new mobicore API instance object
 *
 * @return Instance or NULL if no allocation was possible.
 */
struct mc_instance *mobicore_open(
	void
);

/**
 * Release a mobicore instance object and all objects related to it
 * @param instance instance
 * @return 0 if Ok or -E ERROR
 */
int mobicore_release(
	struct mc_instance	*instance
);

/**
 * Free a WSM buffer allocated with mobicore_allocate_wsm
 * @param instance
 * @param handle		handle of the buffer
 *
 * @return 0 if no error
 *
 */
int mobicore_allocate_wsm(
	struct mc_instance	*instance,
	unsigned long		requested_size,
	uint32_t		*handle,
	void			**kernel_virt_addr,
	void			**phys_addr
);

/**
 * Free a WSM buffer allocated with mobicore_allocate_wsm
 * @param instance
 * @param handle		handle of the buffer
 *
 * @return 0 if no error
 *
 */
int mobicore_free(
	struct mc_instance	*instance,
	uint32_t		handle
);

/**
 * Map a virtual memory buffer structure to Mobicore
 * @param instance
 * @param addr		address of the buffer(NB it must be kernel virtual!)
 * @param len		buffer length
 * @param handle	pointer to handle
 * @param phys_wsm_l2_table	pointer to physical L2 table(?)
 *
 * @return 0 if no error
 *
 */
int mobicore_map_vmem(
	struct mc_instance	*instance,
	void			*addr,
	uint32_t		len,
	uint32_t		*handle,
	void			**phys_wsm_l2_table
);

/**
 * Unmap a virtual memory buffer from mobicore
 * @param instance
 * @param handle
 *
 * @return 0 if no error
 *
 */
int mobicore_unmap_vmem(
	struct mc_instance	*instance,
	uint32_t		handle
);
#endif /* _MOBICORE_KERNELMODULE_API_H_ */
/** @} */
