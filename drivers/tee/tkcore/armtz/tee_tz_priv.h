/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef __TEE_TZ_PRIV__
#define __TEE_TZ_PRIV__

#include <linux/tee_kernel_lowlevel_api.h>

struct tee;
struct shm_pool;
struct tee_rpc_bf;

struct tee_tz {
	bool started;
	struct tee *tee;
	unsigned long shm_paddr;
	void *shm_vaddr;
	struct shm_pool *shm_pool;
	void *tz_outer_cache_mutex;
	struct tee_rpc_bf *rpc_buffers;
	bool shm_cached;
	struct tee_wait_queue_private wait_queue;
};

#endif /* __TEE_TZ_PRIV__ */
