/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * Header files for KREE memory related functions.
 */

#ifndef __KREE_MEM_H__
#define __KREE_MEM_H__

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) || defined(CONFIG_TRUSTY)

#include "tz_cross/trustzone.h"
#include "tz_cross/ree_service.h"
#include "kree/system.h"

#define KREE_SESSION_HANDLE_NULL    ((KREE_SESSION_HANDLE)0)
#define KREE_SESSION_HANDLE_FAIL    ((KREE_SESSION_HANDLE)-1)

/**
 * Memory handle define
 *
 * Handle is used to communicate with normal world:
 * 1. Memory information can not expose to normal world. (Major, important!)
 * 2. Too much information, and thet can be grouped by handle.
 *
 * All kinds of memory use the same handle define.
 * According to their different purpose, they are redefined to specific name.
 * Just for easy programming.
 */

/* Secure memory handle define */
typedef uint32_t KREE_SECUREMEM_HANDLE;

/* Secure chunk memory handle define */
typedef uint32_t KREE_SECURECM_HANDLE;

/* Release Secure chunk memory handle define */
typedef uint32_t KREE_RELEASECM_HANDLE;

/**
 * Shared memory parameter
 *
 * It defines the types for shared memory.
 *
 * @param buffer    A pointer to shared memory buffer
 * @param size    shared memory size in bytes
 */
struct KREE_SHAREDMEM_PARAM {
	void *buffer;
	uint32_t size;
};

/* map_p: 0 = no remap, 1 = remap */
int kree_register_sharedmem(KREE_SESSION_HANDLE session,
		KREE_SHAREDMEM_HANDLE *mem_handle, void *start,
		uint32_t size, void *map_p, const char *tag);

int kree_unregister_sharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE mem_handle);

/**
 * Shared memory
 *
 * A shared memory is normal memory, which can be seen by Normal world and
 * Secure world.
 * It is used to create the comminicattion between two worlds.
 * Currently, zero-copy data transfer is supportted, for simple and efficient
 * design.
 *
 * The shared memory lifetime:
 * 1. CA (Client Application at REE) prepares memory
 * 2. CA registers it to TEE scope.
 * 3. A handle is returned. CA can use it to communicate with TEE.
 * 4. If shared memory is not used, CA unregisters it.
 * 5. CA frees memory.
 *
 * Because it is zero-copy shared memory, the memory characteritics is
 * inherited. If the shared memory will be used for HW, CA must allocate
 * physical continuous memory.
 *
 * Note: Because shared memory can be seen by both Normal and Secure world.
 * It is a possible weakpoint to bed attacked or leak secure data.
 *
 * Note: ONLY support memory allocated by kmalloc!!!
 */

/**
 * Register shared memory
 *
 * @param session    The session handle.
 * @param shm_handle    [out] A pointer to shared memory handle.
 * @param param    A pointer to shared memory parameters.
 * @return    return code.
 */
int KREE_RegisterSharedmem(KREE_SESSION_HANDLE session,
		KREE_SHAREDMEM_HANDLE *shm_handle,
		struct KREE_SHAREDMEM_PARAM *param);


/**
 * Unregister shared memory
 *
 * @param session    The session handle.
 * @param shm_handle    The shared memory handle.
 * @return    return code.
 */
int KREE_UnregisterSharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE shm_handle);

/**
 * Secure memory
 *
 * A secure memory can be seen only in Secure world.
 * Secure memory, here, is defined as external memory (ex: DRAM) protected
 * by trustzone.
 * It can protect from software attack very well, but can not protect from
 * physical attack, like memory probe.
 * CA (Client Application at REE) can ask TEE for a secure buffer, then
 * control it:
 * to reference, or to free...etc.
 *
 * Secure memory spec.:
 * 1. Protected by trustzone (NS = 0).
 * 2. External memory (ex: external DRAM).
 * 3. With cache.
 */

/**
 * Secure memory allocation
 *
 * Allocate one memory.
 * If memory is allocated successfully, a handle will be provided.
 *
 * Memory lifetime:
 * 1. Allocate memory, and get the handle.
 * 2. If other process wants to use the same memory, reference it.
 * 3. If they stop to use it, unreference it.
 * 4. Free it (by unreference), if it is not used.
 *
 * Simple rules:
 * 1. start by allocate, end by unreference (for free).
 * 2. start by reference, end by unreference.
 *
 * @param session    The session handle.
 * @param mem_handle    [out] A pointer to secure memory handle.
 * @param alignment    Memory alignment in bytes.
 * @param size    The size of the buffer to be allocated in bytes.
 * @return    return code.
 */
int KREE_AllocSecuremem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size);

/**
 * Secure memory allocation With Tag
 *
 * Same as KREE_AllocSecuremem() but with one additional tag for debugging.
 *
 * @param session    The session handle.
 * @param mem_handle    [out] A pointer to secure memory handle.
 * @param alignment    Memory alignment in bytes.
 * @param size    The size of the buffer to be allocated in bytes.
 & @param tag     The string for marking the allocation
 * @return    return code.
 */
int KREE_AllocSecurememWithTag(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size,
	const char *tag);

/**
 * Zeroed secure memory allocation With Tag
 *
 * Same as KREE_AllocSecurememWithTag() but the content is initialized as zero.
 *
 * @param session    The session handle.
 * @param mem_handle    [out] A pointer to secure memory handle.
 * @param alignment    Memory alignment in bytes.
 * @param size    The size of the buffer to be allocated in bytes.
 & @param tag     The string for marking the allocation
 * @return    return code.
 */
int KREE_ZallocSecurememWithTag(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size,
	const char *tag);

/**
 * Secure memory reference
 *
 * Reference memory.
 * Referen count will be increased by 1 after reference.
 *
 * Reference lifetime:
 * 1. Reference the memory before using it, if the memory is allocated by
 * other process.
 * 2. Unreference it if it is not used.
 *
 * @param session    The session handle.
 * @param mem_handle    The secure memory handle.
 * @param return    return code.
 */
int KREE_ReferenceSecuremem(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE mem_handle);

/**
 * Secure memory unreference
 *
 * Unreference memory.
 * Reference count will be decreased by 1 after unreference.
 * Once reference count is zero, memory will be freed.
 *
 * @param session    The session handle.
 * @param mem_handle    The secure memory handle.
 * @param return    return code.
 */
int KREE_UnreferenceSecuremem(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE mem_handle);

/**
 * Secure chunk memory
 *
 * A secure chunk memory can be seen only in Secure world.
 * It is a kind of secure memory but with difference characteristic:
 * 1. It is designed and optimized for chunk memory usage.
 * 2. For future work, it can be released as normal memory for more flexible
 * memory usage.
 *
 * Secure chunk memory spec.:
 * 1. Protected by trustzone (NS = 0).
 * 2. External memory (ex: external DRAM).
 * 3. With cache.
 * 4. For future, it can be released to normal world.
 */

/**
 * Secure chunk memory allocation
 *
 * Allocate one memory.
 * If memory is allocated successfully, a handle will be provided.
 *
 * Memory lifetime:
 * 1. Allocate memory, and get the handle.
 * 2. If other process wants to use the same memory, reference it.
 * 3. If they stop to use it, unreference it.
 * 4. Free it (by unreference), if it is not used.
 *
 * Simple rules:
 * 1. start by allocate, end by unreference (for free).
 * 2. start by reference, end by unreference.
 *
 * @param session    The session handle.
 * @param cm_handle    [out] A pointer to secure chunk memory handle.
 * @param alignment    Memory alignment in bytes.
 * @param size    The size of the buffer to be allocated in bytes.

 * @return    return code.
 */
int KREE_AllocSecurechunkmem(KREE_SESSION_HANDLE session,
	KREE_SECURECM_HANDLE *cm_handle, uint32_t alignment, uint32_t size);

/**
 * Secure chunk memory allocation with tag
 *
 * Same as KREE_AllocSecuremem() but with one additional tag for debugging.
 *
 * @param session    The session handle.
 * @param cm_handle    [out] A pointer to secure chunk memory handle.
 * @param alignment    Memory alignment in bytes.
 * @param size    The size of the buffer to be allocated in bytes.
 * @param tag     The string for marking the allocation

 * @return    return code.
 */
int KREE_AllocSecurechunkmemWithTag(KREE_SESSION_HANDLE session,
	KREE_SECURECM_HANDLE *cm_handle, uint32_t alignment, uint32_t size,
	const char *tag);


/**
 * Zeroed secure chunk memory allocation with tag
 *
 * Same as KREE_AllocSecurememWithTag() but the context is initilaized as zero.
 *
 * @param session    The session handle.
 * @param cm_handle    [out] A pointer to secure chunk memory handle.
 * @param alignment    Memory alignment in bytes.
 * @param size    The size of the buffer to be allocated in bytes.
 * @param tag     The string for marking the allocation

 * @return    return code.
 */
int KREE_ZallocSecurechunkmemWithTag(KREE_SESSION_HANDLE session,
	KREE_SECURECM_HANDLE *cm_handle, uint32_t alignment, uint32_t size,
	const char *tag);

/**
 * Secure chunk memory reference
 *
 * Reference memory.
 * Referen count will be increased by 1 after reference.
 *
 * Reference lifetime:
 * 1. Reference the memory before using it, if the memory is allocated by
 * other process.
 * 2. Unreference it if it is not used.
 *
 * @param session    The session handle.
 * @param cm_handle    The secure chunk memory handle.
 * @param return    return code.
 */
int KREE_ReferenceSecurechunkmem(KREE_SESSION_HANDLE session,
					KREE_SECURECM_HANDLE cm_handle);

/**
 * Secure chunk memory unreference
 *
 * Unreference memory.
 * Reference count will be decreased by 1 after unreference.
 * Once reference count is zero, memory will be freed.
 *
 * @param session    The session handle.
 * @param cm_handle    The secure chunk memory handle.
 * @param return    return code.
 */
int KREE_UnreferenceSecurechunkmem(KREE_SESSION_HANDLE session,
					 KREE_SECURECM_HANDLE cm_handle);

/**
 * Released secure chunk memory Read
 *
 * Read release secure chunk memory for normal world usage.
 *
 * @param session    The session handle.
 * @param offset    offset in bytes.
 * @param size    size in bytes.
 * @param buffer    The pointer to read buffer.
 * @param return    return code.
 */
int KREE_ReadSecurechunkmem(KREE_SESSION_HANDLE session,
				uint32_t offset, uint32_t size, void *buffer);

/**
 * Released secure chunk memory Write
 *
 * Write release secure chunk memory for normal world usage.
 *
 * @param session    The session handle.
 * @param offset    offset in bytes.
 * @param size    size in bytes.
 * @param buffer    The pointer to write buffer.
 * @param return    return code.
 */
int KREE_WriteSecurechunkmem(KREE_SESSION_HANDLE session,
			uint32_t offset, uint32_t size, void *buffer);

/**
 * Released secure chunk memory get size
 *
 * Get released secure chunk memory for normal world usage size.
 *
 * @param session    The session handle.
 * @param size    [out] The pointer to size in bytes.
 * @param return    return code.
 */
int KREE_GetSecurechunkReleaseSize(KREE_SESSION_HANDLE session,
					uint32_t *size);


/**
 * Start chunk memory allocation service in TEE
 *
 * Pass the reserve the buffer for secure chunk memory usage
 *
 * @param session    The session handle.
 * @param start_pa   The physical address of chunk memory buffer.
 * @param size       The size in bytes of chunk memory buffer.
 * @param return    return code.
 */
int KREE_StartSecurechunkmemSvc(KREE_SESSION_HANDLE session,
				unsigned long start_pa, uint32_t size);

/**
 * Stop chunk memory allocation service in TEE
 *
 * reclaim the secure chunk memory used in TEE
 *
 * @param session    The session handle.
 * @param cm_pa      The physical address of chunk memory buffer.
 * @param size       The size in bytes of chunk memory buffer.
 * @param return    return code.
 */
int KREE_StopSecurechunkmemSvc(KREE_SESSION_HANDLE session,
					unsigned long *cm_pa, uint32_t *size);

/**
 * Query chunk memory physical location / size in TEE
 *
 * Query the secure chunk memory information in TEE
 *
 * @param session    The session handle.
 * @param cm_pa      The physical address of chunk memory buffer.
 * @param size       The size in bytes of chunk memory buffer.
 * @param return    return code.
 */
int KREE_QuerySecurechunkmem(KREE_SESSION_HANDLE session,
				unsigned long *cm_pa, uint32_t *size);

#ifdef CONFIG_MTEE_CMA_SECURE_MEMORY
/**
 * REE service call to allocate chunk memory
 *
 * Allocate the continuouse memory for TEE secure chunk memory
 *
 * @param op         will be KREE_SERV_GET_CHUNK_MEMPOOL.
 * @param uparam     the exchange buffer for parameters.
 * @param return     return code.
 */
int KREE_ServGetChunkmemPool(u32 op,
				   u8 uparam[REE_SERVICE_BUFFER_SIZE]);

/**
 * REE service call to release cma memory
 *
 * Release the continuouse memory from TEE secure chunk memory
 *
 * @param op         will be KREE_SERV_GET_CHUNK_MEMPOOL.
 * @param uparam     the exchange buffer for parameters.
 * @param return     return code.
 */
int KREE_ServReleaseChunkmemPool(u32 op,
				       u8 uparam[REE_SERVICE_BUFFER_SIZE]);
#endif  /* CONFIG_MTEE_CMA_SECURE_MEMORY */

#endif	/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT || CONFIG_TRUSTY*/
#endif	/* __KREE_MEM_H__ */
