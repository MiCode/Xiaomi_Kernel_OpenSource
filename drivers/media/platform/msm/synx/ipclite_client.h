/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __IPCLITE_CLIENT_H__
#define __IPCLITE_CLIENT_H__

typedef atomic_t ipclite_atomic_uint32_t;
typedef atomic_t ipclite_atomic_int32_t;

/**
 * A list of hosts supported in IPCMEM
 */
enum ipcmem_host_type {
	IPCMEM_APPS         =  0,                     /**< Apps Processor */
	IPCMEM_MODEM        =  1,                     /**< Modem processor */
	IPCMEM_LPASS        =  2,                     /**< Audio processor */
	IPCMEM_SLPI         =  3,                     /**< Sensor processor */
	IPCMEM_GPU          =  4,                     /**< Graphics processor */
	IPCMEM_CDSP         =  5,                     /**< Compute DSP processor */
	IPCMEM_CVP          =  6,                     /**< Computer Vision processor */
	IPCMEM_CAM          =  7,                     /**< Camera processor */
	IPCMEM_VPU          =  8,                     /**< Video processor */
	IPCMEM_NUM_HOSTS    =  9,                     /**< Max number of host in target */

	IPCMEM_GLOBAL_HOST  =  0xFE,                  /**< Global Host */
	IPCMEM_INVALID_HOST =  0xFF,				  /**< Invalid processor */
};

/**
 * IPCLite return codes
 */
#define IPCLITE_SUCCESS		0 /*< Success > */
#define IPCLITE_FAILURE		1 /*< Failure > */
#define IPCLITE_EINCHAN		9 /*< Inactive Channel */

struct global_region_info {
	void *virt_base;
	uint32_t size;
};

typedef int32_t (*IPCLite_Client)(uint32_t proc_id,  int64_t data,  void *priv);

/**
 * ipclite_msg_send() - Sends message to remote client.
 *
 * @proc_id  : Identifier for remote client or subsystem.
 * @data       : 64 bit message value.
 *
 * @return Zero on successful registration, negative on failure.
 */
int32_t ipclite_msg_send(int32_t proc_id, uint64_t data);

/**
 * ipclite_register_client() - Registers client callback with framework.
 *
 * @cb_func_ptr : Client callback function to be called on message receive.
 * @priv        : Private data required by client for handling callback.
 *
 * @return Zero on successful registration, negative on failure.
 */
int32_t ipclite_register_client(IPCLite_Client cb_func_ptr, void *priv);

/**
 * ipclite_test_msg_send() - Sends message to remote client.
 *
 * @proc_id  : Identifier for remote client or subsystem.
 * @data       : 64 bit message value.
 *
 * @return Zero on successful registration, negative on failure.
 */
int32_t ipclite_test_msg_send(int32_t proc_id, uint64_t data);

/**
 * ipclite_register_test_client() - Registers client callback with framework.
 *
 * @cb_func_ptr : Client callback function to be called on message receive.
 * @priv        : Private data required by client for handling callback.
 *
 * @return Zero on successful registration, negative on failure.
 */
int32_t ipclite_register_test_client(IPCLite_Client cb_func_ptr, void *priv);

/**
 * get_global_partition_info() - Gets info about IPCMEM's global partitions.
 *
 * @global_ipcmem : Pointer to global_region_info structure.
 *
 * @return Zero on successful registration, negative on failure.
 */
int32_t get_global_partition_info(struct global_region_info *global_ipcmem);

/**
 * ipclite_recover() - Recovers the ipclite if any core goes for SSR
 *
 * core_id	: takes the core id of the core which went to SSR.
 *
 * @return None.
 */
void ipclite_recover(enum ipcmem_host_type core_id);

/**
 * ipclite_atomic_init_u32() - Initializes the global memory with uint32_t value.
 *
 * @addr	: Pointer to global memory
 * @data	: Value to store in global memory
 *
 * @return None.
 */
void ipclite_atomic_init_u32(ipclite_atomic_uint32_t *addr, uint32_t data);

/**
 * ipclite_atomic_init_i32() - Initializes the global memory with int32_t value.
 *
 * @addr	: Pointer to global memory
 * @data	: Value to store in global memory
 *
 * @return None.
 */
void ipclite_atomic_init_i32(ipclite_atomic_int32_t *addr, int32_t data);

/**
 * ipclite_global_atomic_store_u32() - Writes uint32_t value to global memory.
 *
 * @addr	: Pointer to global memory
 * @data	: Value to store in global memory
 *
 * @return None.
 */
void ipclite_global_atomic_store_u32(ipclite_atomic_uint32_t *addr, uint32_t data);

/**
 * ipclite_global_atomic_store_i32() - Writes int32_t value to global memory.
 *
 * @addr	: Pointer to global memory
 * @data	: Value to store in global memory
 *
 * @return None.
 */
void ipclite_global_atomic_store_i32(ipclite_atomic_int32_t *addr, int32_t data);

/**
 * ipclite_global_atomic_load_u32() - Reads the value from global memory.
 *
 * @addr	: Pointer to global memory
 *
 * @return uint32_t value.
 */
uint32_t ipclite_global_atomic_load_u32(ipclite_atomic_uint32_t *addr);

/**
 * ipclite_global_atomic_load_i32() - Reads the value from global memory.
 *
 * @addr	: Pointer to global memory
 *
 * @return int32_t value.
 */
int32_t ipclite_global_atomic_load_i32(ipclite_atomic_int32_t *addr);

/**
 * ipclite_global_test_and_set_bit() - Sets a bit in global memory.
 *
 * @nr		: Bit position to set.
 * @addr	: Pointer to global memory
 *
 * @return previous value.
 */
uint32_t ipclite_global_test_and_set_bit(uint32_t nr, ipclite_atomic_uint32_t *addr);

/**
 * ipclite_global_test_and_clear_bit() - Clears a bit in global memory.
 *
 * @nr		: Bit position to clear.
 * @addr	: Pointer to global memory
 *
 * @return previous value.
 */
uint32_t ipclite_global_test_and_clear_bit(uint32_t nr, ipclite_atomic_uint32_t *addr);

/**
 * ipclite_global_atomic_inc() - Increments an atomic variable by one.
 *
 * @addr	: Pointer to global memory
 *
 * @return previous value.
 */
int32_t ipclite_global_atomic_inc(ipclite_atomic_int32_t *addr);

/**
 * ipclite_global_atomic_dec() - Decrements an atomic variable by one.
 *
 * @addr	: Pointer to global variable
 *
 * @return previous value.
 */
int32_t ipclite_global_atomic_dec(ipclite_atomic_int32_t *addr);

#endif
