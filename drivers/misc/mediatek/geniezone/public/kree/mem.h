/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */


/*
 * Mediatek GenieZone v1.2.0127
 * Header files for KREE memory related functions.
 */

#ifndef __KREE_MEM_H__
#define __KREE_MEM_H__

#if IS_ENABLED(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)	\
	|| IS_ENABLED(CONFIG_MTK_ENABLE_GENIEZONE)

#include <tz_cross/trustzone.h>
#include <tz_cross/gz_version.h>

#define KREE_SESSION_HANDLE_NULL ((KREE_SESSION_HANDLE)0)
#define KREE_SESSION_HANDLE_FAIL ((KREE_SESSION_HANDLE)-1)

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

/* / KREE session handle type. */
typedef int32_t KREE_SESSION_HANDLE;

/* Shared memory handle define */
typedef uint32_t KREE_SHAREDMEM_HANDLE;

/* Secure memory handle define */
typedef uint32_t KREE_SECUREMEM_HANDLE;

/* Secure chunk memory handle define */
typedef uint32_t KREE_SECURECM_HANDLE;

/* Release Secure chunk memory handle define */
typedef uint32_t KREE_RELEASECM_HANDLE;

/* ION memory handle define */
typedef uint32_t KREE_ION_HANDLE;
typedef uint32_t *KREE_ION_HANDLE_PTR;

/**
 * Shared memory parameter
 *
 * It defines the types for shared memory.
 *
 * @param buffer	A pointer to shared memory buffer
 * @param size	shared memory size in bytes
 */
struct kree_shared_mem_param {

	uint32_t size;
	uint32_t region_id;
	void *buffer;
	void *mapAry;
};
#define KREE_SHAREDMEM_PARAM struct kree_shared_mem_param

/**
 * compress a PA array by run-length coding
 *
 * It defines the types for shared memory.
 *
 * @param high	high bit of start PA address
 * @param low	 low  bit of start PA address
 * @param size	run length size
 */
struct KREE_SHM_RUNLENGTH_ENTRY {
	uint32_t high; /* (uint64_t) start PA address = high | low */
	uint32_t low;
	uint32_t size;
};

struct KREE_SHM_RUNLENGTH_LIST {
	struct KREE_SHM_RUNLENGTH_ENTRY entry;
	struct KREE_SHM_RUNLENGTH_LIST *next;
};


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
 * @param session	The session handle.
 * @param shm_handle	[out] A pointer to shared memory handle.
 * @param param	A pointer to shared memory parameters.
 * @return	return code.
 */
TZ_RESULT KREE_RegisterSharedmem(KREE_SESSION_HANDLE session,
				 KREE_SHAREDMEM_HANDLE *shm_handle,
				 KREE_SHAREDMEM_PARAM *param);

/**
 * Unregister shared memory
 *
 * @param session	The session handle.
 * @param shm_handle	The shared memory handle.
 * @return	return code.
 */
TZ_RESULT KREE_UnregisterSharedmem(KREE_SESSION_HANDLE session,
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
 * @param session	The session handle.
 * @param mem_handle	[out] A pointer to secure memory handle.
 * @param alignment	Memory alignment in bytes.
 * @param size	The size of the buffer to be allocated in bytes.
 * @return	return code.
 */
TZ_RESULT KREE_AllocSecuremem(KREE_SESSION_HANDLE session,
			      KREE_SECUREMEM_HANDLE *mem_handle,
			      uint32_t alignment, uint32_t size);
/**
 * Zeroed secure memory allocation
 *
 * Same as KREE_AllocSecuremem() but the content is initialized as zero.
 *
 * @param session	The session handle.
 * @param mem_handle	[out] A pointer to secure memory handle.
 * @param alignment	Memory alignment in bytes.
 * @param size	The size of the buffer to be allocated in bytes.
 * @return	return code.
 */
TZ_RESULT KREE_ZallocSecuremem(KREE_SESSION_HANDLE session,
				     KREE_SECUREMEM_HANDLE *mem_handle,
				     uint32_t alignment, uint32_t size);

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
/*fix mtee sync*/
TZ_RESULT KREE_AllocSecurememWithTag(KREE_SESSION_HANDLE session,
				     KREE_SECUREMEM_HANDLE *mem_handle,
				     uint32_t alignment, uint32_t size,
				     const char *tag);

/**
 * Zeroed secure memory allocation with tag
 *
 * Same as KREE_AllocSecurememWithTag() but the content is initialized as zero.
 *
 * @param session	The session handle.
 * @param mem_handle	[out] A pointer to secure memory handle.
 * @param alignment	Memory alignment in bytes.
 * @param size	The size of the buffer to be allocated in bytes.
 * @return	return code.
 */
TZ_RESULT KREE_ZallocSecurememWithTag(KREE_SESSION_HANDLE session,
				     KREE_SECUREMEM_HANDLE *mem_handle,
				     uint32_t alignment, uint32_t size,
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
 * @param session	The session handle.
 * @param mem_handle	The secure memory handle.
 * @param return	return code.
 */
TZ_RESULT KREE_ReferenceSecuremem(KREE_SESSION_HANDLE session,
				  KREE_SECUREMEM_HANDLE mem_handle);

/**
 * Secure memory unreference
 *
 * Unreference memory.
 * Reference count will be decreased by 1 after unreference.
 * Once reference count is zero, memory will be freed.
 *
 * @param session	The session handle.
 * @param mem_handle	The secure memory handle.
 * @param return	return code.
 */
TZ_RESULT KREE_UnreferenceSecuremem(KREE_SESSION_HANDLE session,
				    KREE_SECUREMEM_HANDLE mem_handle);

/**
 * Secure chunk memory release
 *
 * Release secure chunk memory for normal world usage.
 *
 * @param session    The session handle.
 * @param size    [out] The pointer to released size in bytes.
 * @param return    return code.
 */
TZ_RESULT KREE_ReleaseSecureMultichunkmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE cm_handle);

TZ_RESULT KREE_ReleaseSecureMultichunkmem_basic(KREE_SESSION_HANDLE session,
					  KREE_SHAREDMEM_HANDLE cm_handle);


/**
 * Secure chunk memory append
 *
 * Append secure chunk memory back to secure world.
 *
 * @param session    The session handle.
 * @param return    return code.
 */
TZ_RESULT KREE_AppendSecureMultichunkmem(KREE_SESSION_HANDLE session,
					 KREE_SHAREDMEM_HANDLE *cm_handle,
					 KREE_SHAREDMEM_PARAM *param);

TZ_RESULT KREE_AppendSecureMultichunkmem_basic(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *cm_handle,
					KREE_SHAREDMEM_PARAM *param);
TZ_RESULT KREE_AllocSecureMultichunkmem(KREE_SESSION_HANDLE session,
				KREE_SHAREDMEM_HANDLE chm_handle,
				KREE_SECUREMEM_HANDLE *mem_handle,
				uint32_t alignment, uint32_t size);

TZ_RESULT KREE_ZallocSecureMultichunkmem(KREE_SESSION_HANDLE session,
				KREE_SHAREDMEM_HANDLE chm_handle,
				KREE_SECUREMEM_HANDLE *mem_handle,
				uint32_t alignment, uint32_t size);

TZ_RESULT KREE_ReferenceSecureMultichunkmem(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE mem_handle);

TZ_RESULT KREE_UnreferenceSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE mem_handle, uint32_t *count);

TZ_RESULT KREE_ION_AllocChunkmem(KREE_SESSION_HANDLE session,
				 KREE_SHAREDMEM_HANDLE chm_handle,
				 KREE_SECUREMEM_HANDLE *secmHandle,
				 uint32_t alignment,
				 uint32_t size);

TZ_RESULT KREE_ION_ZallocChunkmem(KREE_SESSION_HANDLE session,
				  KREE_SHAREDMEM_HANDLE chm_handle,
				  KREE_SECUREMEM_HANDLE *secmHandle,
				  uint32_t alignment, uint32_t size);

TZ_RESULT KREE_ION_ReferenceChunkmem(KREE_SESSION_HANDLE session,
				     KREE_SECUREMEM_HANDLE secmHandle);

TZ_RESULT KREE_ION_UnreferenceChunkmem(KREE_SESSION_HANDLE session,
				       KREE_SECUREMEM_HANDLE secmHandle);

/*only for test*/
TZ_RESULT KREE_QueryChunkmem_TEST(KREE_SESSION_HANDLE session,
				KREE_SECUREMEM_HANDLE secmHandle, uint32_t cmd);
TZ_RESULT KREE_ION_QueryChunkmem_TEST(KREE_SESSION_HANDLE session,
				      KREE_SECUREMEM_HANDLE secmHandle, uint32_t cmd);

TZ_RESULT KREE_ConfigSecureMultiChunkMemInfo(KREE_SESSION_HANDLE session,
					     uint64_t pa, uint32_t size,
					     uint32_t region_id);

#endif /* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT || CONFIG_MTK_ENABLE_GENIEZONE*/
#endif /* __KREE_MEM_H__ */
