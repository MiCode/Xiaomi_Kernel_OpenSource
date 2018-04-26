/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __QTEE_SHMBRIDGE_H__
#define __QTEE_SHMBRIDGE_H__

/**
 * struct qtee_shm - info of shared memory allocated from the default bridge
 * @ paddr: physical address of the shm allocated from the default bridge
 * @ vaddr: virtual address of the shm
 * @ size: size of the shm
 */
struct qtee_shm {
	phys_addr_t paddr;
	void *vaddr;
	size_t size;
};

/**
 * Check whether shmbridge mechanism is enabled in HYP or not
 *
 * return true when enabled, false when not enabled
 */
bool qtee_shmbridge_is_enabled(void);

/**
 * Register paddr & size as a bridge, get bridge handle
 *
 * @ paddr: paddr of buffer to be turned into bridge
 * @ size: size of the bridge
 * @ ns_vmid: non-secure vmid, like VMID_HLOS
 * @ ns_vm_perm: NS VM permission, like PERM_READ, PERM_WRITE
 * @ tz_perm: TZ permission
 * @ *handle: output shmbridge handle
 *
 * return success or error
 */
int32_t qtee_shmbridge_register(
		phys_addr_t paddr,
		size_t size,
		uint32_t ns_vmid,
		uint32_t ns_vm_perm,
		uint32_t tz_perm,
		uint64_t *handle);

/**
 * Deregister bridge
 *
 * @ handle: shmbridge handle
 *
 * return success or error
 */
int32_t qtee_shmbridge_deregister(uint64_t handle);

/**
 * Sub-allocate from default kernel bridge created by shmb driver
 *
 * @ size: size of the buffer to be sub-allocated from the bridge
 * @ *shm: output qtee_shm structure with buffer paddr, vaddr and
 *         size; returns ERR_PTR or NULL otherwise
 *
 * return success or error
 */
int32_t qtee_shmbridge_allocate_shm(size_t size, struct qtee_shm *shm);

/*
 * Free buffer that is sub-allocated from default kernel bridge
 *
 * @ shm: qtee_shm structure to be freed
 *
 */
void qtee_shmbridge_free_shm(struct qtee_shm *shm);

#endif /*__QTEE_SHMBRIDGE_H__*/
