/* MobiCore driver module.(interface to the secure world SWD)
 * MobiCore Driver Kernel Module.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>

#include "main.h"
#include "mem.h"
#include "debug.h"

int mobicore_map_vmem(struct mc_instance *instance, void *addr,
	uint32_t len, uint32_t *handle)
{
	phys_addr_t phys;
	return mc_register_wsm_mmu(instance, addr, len,
		handle, &phys);
}
EXPORT_SYMBOL(mobicore_map_vmem);

/*
 * Unmap a virtual memory buffer from mobicore
 * @param instance
 * @param handle
 *
 * @return 0 if no error
 *
 */
int mobicore_unmap_vmem(struct mc_instance *instance, uint32_t handle)
{
	return mc_unregister_wsm_mmu(instance, handle);
}
EXPORT_SYMBOL(mobicore_unmap_vmem);

/*
 * Free a WSM buffer allocated with mobicore_allocate_wsm
 * @param instance
 * @param handle		handle of the buffer
 *
 * @return 0 if no error
 *
 */
int mobicore_free_wsm(struct mc_instance *instance, uint32_t handle)
{
	return mc_free_buffer(instance, handle);
}
EXPORT_SYMBOL(mobicore_free_wsm);


/*
 * Allocate WSM for given instance
 *
 * @param instance		instance
 * @param requested_size		size of the WSM
 * @param handle		pointer where the handle will be saved
 * @param virt_kernel_addr	pointer for the kernel virtual address
 *
 * @return error code or 0 for success
 */
int mobicore_allocate_wsm(struct mc_instance *instance,
	unsigned long requested_size, uint32_t *handle, void **virt_kernel_addr)
{
	struct mc_buffer *buffer = NULL;

	/* Setup the WSM buffer structure! */
	if (mc_get_buffer(instance, &buffer, requested_size))
		return -EFAULT;

	*handle = buffer->handle;
	*virt_kernel_addr = buffer->addr;
	return 0;
}
EXPORT_SYMBOL(mobicore_allocate_wsm);

/*
 * Initialize a new mobicore API instance object
 *
 * @return Instance or NULL if no allocation was possible.
 */
struct mc_instance *mobicore_open(void)
{
	struct mc_instance *instance = mc_alloc_instance();
	if(instance) {
		instance->admin = true;
	}
	return instance;
}
EXPORT_SYMBOL(mobicore_open);

/*
 * Release a mobicore instance object and all objects related to it
 * @param instance instance
 * @return 0 if Ok or -E ERROR
 */
int mobicore_release(struct mc_instance *instance)
{
	return mc_release_instance(instance);
}
EXPORT_SYMBOL(mobicore_release);

/*
 * Test if mobicore can sleep
 *
 * @return true if mobicore can sleep, false if it can't sleep
 */
bool mobicore_sleep_ready(void)
{
	return mc_sleep_ready();
}
EXPORT_SYMBOL(mobicore_sleep_ready);

