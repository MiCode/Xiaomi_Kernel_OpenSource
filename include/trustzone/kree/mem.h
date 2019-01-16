/*
 * Header files for KREE memory related functions.
 */

#ifndef __KREE_MEM_H__
#define __KREE_MEM_H__

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT

#include "trustzone/tz_cross/trustzone.h"


/// KREE session handle type.
typedef void* KREE_SESSION_HANDLE;

#define KREE_SESSION_HANDLE_NULL    ((KREE_SESSION_HANDLE)0)
#define KREE_SESSION_HANDLE_FAIL    ((KREE_SESSION_HANDLE)-1)

/**
 * Memory handle define
 *
 * Handle is used to communicate with normal world:
 * 1. Memory information can not expose to normal world. (Major, important!)
 * 2. Too many informations, and thet can be grouped by handle.
 *
 * All kinds of memory use the same handle define.
 * According to their different purpose, they are redefined to specific name.
 * Just for easy programming.
 */

// Shared memory handle define
typedef uint32_t KREE_SHAREDMEM_HANDLE;

// Secure memory handle define
typedef uint32_t KREE_SECUREMEM_HANDLE;

// Secure chunk memory handle define
typedef uint32_t KREE_SECURECM_HANDLE;

// Release Secure chunk memory handle define
typedef uint32_t KREE_RELEASECM_HANDLE;

/**
 * Shared memory parameter
 *
 * It defines the types for shared memory.
 *
 * @param buffer    A pointer to shared memory buffer
 * @param size    shared memory size in bytes
 */
typedef struct { 
    void* buffer; 
    uint32_t size; 
} KREE_SHAREDMEM_PARAM;

// map_p: 0 = no remap, 1 = remap
TZ_RESULT kree_register_sharedmem (KREE_SESSION_HANDLE session, KREE_SHAREDMEM_HANDLE *mem_handle, 
    uint32_t start, uint32_t size, uint32_t map_p);

TZ_RESULT kree_unregister_sharedmem (KREE_SESSION_HANDLE session, KREE_SHAREDMEM_HANDLE mem_handle);    

/**
 * Shared memory
 *
 * A shared memory is normal memory, which can be seen by Normal world and Secure world.
 * It is used to create the comminicattion between two worlds.
 * Currently, zero-copy data transfer is supportted, for simple and efficient design.
 *
 * The shared memory lifetime:
 * 1. CA (Client Application at REE) prepares memory
 * 2. CA registers it to TEE scope.
 * 3. A handle is returned. CA can use it to communicate with TEE.
 * 4. If shared memory is not used, CA unregisters it.
 * 5. CA frees memory.
 *
 * Because it is zero-copy shared memory, the memory characteritics is inherited.
 * If the shared memory will be used for HW, CA must allocate physical continuous memory.
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
TZ_RESULT KREE_RegisterSharedmem (    KREE_SESSION_HANDLE session, 
    KREE_SHAREDMEM_HANDLE *shm_handle, KREE_SHAREDMEM_PARAM *param);

    
/**
 * Unregister shared memory
 *
 * @param session    The session handle.  
 * @param shm_handle    The shared memory handle.
 * @return    return code.
 */    
TZ_RESULT KREE_UnregisterSharedmem (KREE_SESSION_HANDLE session, KREE_SHAREDMEM_HANDLE shm_handle);

/**
 * Secure memory
 *
 * A secure memory can be seen only in Secure world.
 * Secure memory, here, is defined as external memory (ex: DRAM) protected by trustzone.
 * It can protect from software attack very well, but can not protect from physical attack, like memory probe.
 * CA (Client Application at REE) can ask TEE for a secure buffer, then control it: 
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
TZ_RESULT KREE_AllocSecuremem (KREE_SESSION_HANDLE session, 
    KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size);

/**
 * Secure memory reference
 *
 * Reference memory.
 * Referen count will be increased by 1 after reference.
 * 
 * Reference lifetime:
 * 1. Reference the memory before using it, if the memory is allocated by other process.
 * 2. Unreference it if it is not used.
 *
 * @param session    The session handle.
 * @param mem_handle    The secure memory handle.
 * @param return    return code.
 */ 
TZ_RESULT KREE_ReferenceSecuremem (KREE_SESSION_HANDLE session, KREE_SECUREMEM_HANDLE mem_handle);

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
TZ_RESULT KREE_UnreferenceSecuremem (KREE_SESSION_HANDLE session, KREE_SECUREMEM_HANDLE mem_handle);

/**
 * Secure chunk memory
 *
 * A secure chunk memory can be seen only in Secure world.
 * It is a kind of secure memory but with difference characteristic:
 * 1. It is designed and optimized for chunk memory usage.
 * 2. For future work, it can be released as normal memory for more flexible memory usage.
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
TZ_RESULT KREE_AllocSecurechunkmem (KREE_SESSION_HANDLE session, 
    KREE_SECURECM_HANDLE *cm_handle, uint32_t alignment, uint32_t size);

/**
 * Secure chunk memory reference
 *
 * Reference memory.
 * Referen count will be increased by 1 after reference.
 * 
 * Reference lifetime:
 * 1. Reference the memory before using it, if the memory is allocated by other process.
 * 2. Unreference it if it is not used.
 *
 * @param session    The session handle.
 * @param cm_handle    The secure chunk memory handle.
 * @param return    return code.
 */ 
TZ_RESULT KREE_ReferenceSecurechunkmem (KREE_SESSION_HANDLE session, KREE_SECURECM_HANDLE cm_handle);

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
TZ_RESULT KREE_UnreferenceSecurechunkmem (KREE_SESSION_HANDLE session, KREE_SECURECM_HANDLE cm_handle);

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
TZ_RESULT KREE_ReadSecurechunkmem (KREE_SESSION_HANDLE session, uint32_t offset, uint32_t size, void *buffer);

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
TZ_RESULT KREE_WriteSecurechunkmem (KREE_SESSION_HANDLE session, uint32_t offset, uint32_t size, void *buffer);    

/**
 * Released secure chunk memory get size
 *
 * Get released secure chunk memory for normal world usage size.
 *
 * @param session    The session handle.
 * @param size    [out] The pointer to size in bytes.
 * @param return    return code.
 */
TZ_RESULT KREE_GetSecurechunkReleaseSize (KREE_SESSION_HANDLE session, uint32_t *size);    


/**
 * Get TEE memory size
 *
 * Get the total memory size of trusted execution environment
 *
 * @param session    The session handle.
 * @param size    [out] The pointer to size in bytes.
 * @param return    return code.
 */
TZ_RESULT KREE_GetTEETotalSize (KREE_SESSION_HANDLE session, uint32_t *size);    

#endif /* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */
#endif /* __KREE_MEM_H__ */
