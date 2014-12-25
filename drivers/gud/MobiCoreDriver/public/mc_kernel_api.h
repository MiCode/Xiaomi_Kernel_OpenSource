/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Interface to be used by module MobiCoreKernelAPI.
 */
#ifndef _MC_KERNEL_API_H_
#define _MC_KERNEL_API_H_

struct mc_instance;

/*
 * mobicore_open() - Initialize a new MobiCore API instance object
 *
 * Returns a MobiCore Instance or NULL if no allocation was possible.
 */
struct mc_instance *mobicore_open(void);

/*
 * mobicore_release() - Release a MobiCore instance object
 * @instance:		MobiCore instance
 *
 * Returns 0 if Ok or -E ERROR
 */
int mobicore_release(struct mc_instance *instance);

/*
 * mobicore_allocate_wsm() - Allocate MobiCore WSM
 * @instance:		instance data for MobiCore Daemon and TLCs
 * @requested_size:	memory size requested in bytes
 * @handle:		pointer to handle
 * @kernel_virt_addr:	virtual user start address
 *
 * Returns 0 if OK
 */
int mobicore_allocate_wsm(struct mc_instance *instance,
			  unsigned long requested_size, uint32_t *handle,
			  void **virt_kernel_addr);

/*
 * mobicore_free() - Free a WSM buffer allocated with mobicore_allocate_wsm
 * @instance:		instance data for MobiCore Daemon and TLCs
 * @handle:		handle of the buffer
 *
 * Returns 0 if OK
 */
int mobicore_free_wsm(struct mc_instance *instance, uint32_t handle);

/*
 * mobicore_map_vmem() - Map a virtual memory buffer structure to Mobicore
 * @instance:		instance data for MobiCore Daemon and TLCs
 * @addr:		address of the buffer (NB it must be kernel virtual!)
 * @len:		buffer length (in bytes)
 * @handle:		unique handle
 *
 * Returns 0 if no error
 */
int mobicore_map_vmem(struct mc_instance *instance, void *addr,
		      uint32_t len, uint32_t *handle);

/*
 * mobicore_unmap_vmem() - Unmap a virtual memory buffer from MobiCore
 * @instance:		instance data for MobiCore Daemon and TLCs
 * @handle:		unique handle
 *
 * Returns 0 if no error
 */
int mobicore_unmap_vmem(struct mc_instance *instance, uint32_t handle);

/*
 * mobicore_sleep_ready() - Test if mobicore can sleep
 *
 * Returns true if mobicore can sleep, false if it can't sleep
 */
bool mobicore_sleep_ready(void);


#endif /* _MC_KERNEL_API_H_ */
