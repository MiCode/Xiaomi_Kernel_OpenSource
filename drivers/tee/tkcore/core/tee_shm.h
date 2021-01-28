/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef __TEE_SHM_H__
#define __TEE_SHM_H__

#include <linux/tee_client_api.h>

struct tee_context;
struct tee_shm;
struct tee_shm_io;
struct tee;

static inline int shm_test_nonsecure(uint32_t flags)
{
	return flags & TEEC_MEM_NONSECURE;
}

int tee_shm_alloc_io(struct tee_context *ctx, struct tee_shm_io *shm_io);
void tee_shm_free_io(struct tee_shm *shm);

struct tee_shm *tee_shm_from_paddr(struct tee *tee, void *paddr, bool ns);
int tee_shm_fd_for_rpc(struct tee_context *ctx, struct tee_shm_io *shm_io);

int tee_shm_alloc_io_perm(struct tee_context *ctx, struct tee_shm_io *shm_io);

struct tee_shm *tkcore_alloc_shm(struct tee *tee, size_t size, uint32_t flags);
void tkcore_shm_free(struct tee_shm *shm);

struct tee_shm *tkcore_shm_get(struct tee_context *ctx,
		struct TEEC_SharedMemory *c_shm,
		size_t size, int offset);
void tkcore_shm_put(struct tee_context *ctx, struct tee_shm *shm);

#endif /* __TEE_SHM_H__ */
