/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __SMCINVOKE_OBJECT_H
#define __SMCINVOKE_OBJECT_H

#include <linux/types.h>
#include <linux/firmware.h>
#include <linux/qtee_shmbridge.h>

/*
 * Method bits are not modified by transport layers.  These describe the
 * method (member function) being requested by the client.
 */

#define OBJECT_OP_METHOD_MASK     (0x0000FFFFu)
#define OBJECT_OP_METHODID(op)    ((op) & OBJECT_OP_METHOD_MASK)
#define OBJECT_OP_RELEASE       (OBJECT_OP_METHOD_MASK - 0)
#define OBJECT_OP_RETAIN        (OBJECT_OP_METHOD_MASK - 1)
#define OBJECT_OP_MAP_REGION    0
#define OBJECT_OP_YIELD 1

#define OBJECT_COUNTS_MAX_BI   0xF
#define OBJECT_COUNTS_MAX_BO   0xF
#define OBJECT_COUNTS_MAX_OI   0xF
#define OBJECT_COUNTS_MAX_OO   0xF

/* unpack counts */

#define OBJECT_COUNTS_NUM_BI(k)  ((size_t) (((k) >> 0) & OBJECT_COUNTS_MAX_BI))
#define OBJECT_COUNTS_NUM_BO(k)  ((size_t) (((k) >> 4) & OBJECT_COUNTS_MAX_BO))
#define OBJECT_COUNTS_NUM_OI(k)  ((size_t) (((k) >> 8) & OBJECT_COUNTS_MAX_OI))
#define OBJECT_COUNTS_NUM_OO(k)  ((size_t) (((k) >> 12) & OBJECT_COUNTS_MAX_OO))
#define OBJECT_COUNTS_NUM_buffers(k)	\
			(OBJECT_COUNTS_NUM_BI(k) + OBJECT_COUNTS_NUM_BO(k))

#define OBJECT_COUNTS_NUM_objects(k)	\
			(OBJECT_COUNTS_NUM_OI(k) + OBJECT_COUNTS_NUM_OO(k))

/* Indices into args[] */

#define OBJECT_COUNTS_INDEX_BI(k)   0
#define OBJECT_COUNTS_INDEX_BO(k)		\
			(OBJECT_COUNTS_INDEX_BI(k) + OBJECT_COUNTS_NUM_BI(k))
#define OBJECT_COUNTS_INDEX_OI(k)		\
			(OBJECT_COUNTS_INDEX_BO(k) + OBJECT_COUNTS_NUM_BO(k))
#define OBJECT_COUNTS_INDEX_OO(k)		\
			(OBJECT_COUNTS_INDEX_OI(k) + OBJECT_COUNTS_NUM_OI(k))
#define OBJECT_COUNTS_TOTAL(k)		\
			(OBJECT_COUNTS_INDEX_OO(k) + OBJECT_COUNTS_NUM_OO(k))

#define OBJECT_COUNTS_PACK(in_bufs, out_bufs, in_objs, out_objs) \
	((uint32_t) ((in_bufs) | ((out_bufs) << 4) | \
			((in_objs) << 8) | ((out_objs) << 12)))

#define OBJECT_COUNTS_INDEX_buffers(k)   OBJECT_COUNTS_INDEX_BI(k)

/* Object_invoke return codes */

#define OBJECT_isOK(err)        ((err) == 0)
#define OBJECT_isERROR(err)     ((err) != 0)

/* Generic error codes */

#define OBJECT_OK                  0   /* non-specific success code */
#define OBJECT_ERROR               1   /* non-specific error */
#define OBJECT_ERROR_INVALID       2   /* unsupported/unrecognized request */
#define OBJECT_ERROR_SIZE_IN       3   /* supplied buffer/string too large */
#define OBJECT_ERROR_SIZE_OUT      4   /* supplied output buffer too small */

#define OBJECT_ERROR_USERBASE     10   /* start of user-defined error range */

/* Transport layer error codes */

#define OBJECT_ERROR_DEFUNCT     -90   /* object no longer exists */
#define OBJECT_ERROR_ABORT       -91   /* calling thread must exit */
#define OBJECT_ERROR_BADOBJ      -92   /* invalid object context */
#define OBJECT_ERROR_NOSLOTS     -93   /* caller's object table full */
#define OBJECT_ERROR_MAXARGS     -94   /* too many args */
#define OBJECT_ERROR_MAXDATA     -95   /* buffers too large */
#define OBJECT_ERROR_UNAVAIL     -96   /* the request could not be processed */
#define OBJECT_ERROR_KMEM        -97   /* kernel out of memory */
#define OBJECT_ERROR_REMOTE      -98   /* local method sent to remote object */
#define OBJECT_ERROR_BUSY        -99   /* Object is busy */
#define Object_ERROR_TIMEOUT     -103  /* Call Back Object invocation timed out. */

#define FOR_ARGS(ndxvar, counts, section) \
	for (ndxvar = OBJECT_COUNTS_INDEX_##section(counts); \
		ndxvar < (OBJECT_COUNTS_INDEX_##section(counts) \
		+ OBJECT_COUNTS_NUM_##section(counts)); \
		++ndxvar)

/* ObjectOp */

#define ObjectOp_METHOD_MASK     ((uint32_t) 0x0000FFFFu)
#define ObjectOp_methodID(op)    ((op) & ObjectOp_METHOD_MASK)

#define ObjectOp_LOCAL           ((uint32_t) 0x00008000U)

#define ObjectOp_isLocal(op)     (((op) & ObjectOp_LOCAL) != 0)


#define Object_OP_release       (ObjectOp_METHOD_MASK - 0)
#define Object_OP_retain        (ObjectOp_METHOD_MASK - 1)

/* Object */

#define ObjectCounts_pack(nBuffersIn, nBuffersOut, nObjectsIn, nObjectsOut) \
	((uint32_t) ((nBuffersIn) |	\
	((nBuffersOut) << 4) |			\
	((nObjectsIn) << 8)  |			\
	((nObjectsOut) << 12)))

union ObjectArg;

typedef int32_t (*ObjectInvoke)(void *h,
				uint32_t op,
				union ObjectArg *args,
				uint32_t counts);

struct Object {
	ObjectInvoke invoke;
	void *context;
};

struct ObjectBuf {
	void *ptr;
	size_t size;
};

struct ObjectBufIn {
	const void *ptr;
	size_t size;
};

union ObjectArg {
	struct ObjectBuf b;
	struct ObjectBufIn bi;
	struct Object o;
};

static inline int32_t Object_invoke(struct Object o, uint32_t op,
				union ObjectArg *args, uint32_t k)
{
	return o.invoke(o.context, op, args, k);
}

#define Object_NULL		((struct Object){NULL, NULL})


#define OBJECT_NOT_RETAINED

#define OBJECT_CONSUMED

static inline int32_t Object_release(OBJECT_CONSUMED struct Object o)
{
	return Object_invoke((o), Object_OP_release, 0, 0);
}
static inline int32_t Object_retain(struct Object o)
{
	return Object_invoke((o), Object_OP_retain, 0, 0);
}

#define Object_isNull(o)	((o).invoke == NULL)

#define Object_RELEASE_IF(o)				\
	do {						\
		struct Object o_ = (o);			\
		if (!Object_isNull(o_))			\
			(void) Object_release(o_);	\
	} while (0)

static inline void Object_replace(struct Object *loc, struct Object objNew)
{
	if (!Object_isNull(*loc))
		Object_release(*loc);

	if (!Object_isNull(objNew))
		Object_retain(objNew);
	*loc = objNew;
}

#define Object_ASSIGN_NULL(loc)  Object_replace(&(loc), Object_NULL)

int smcinvoke_release_from_kernel_client(int fd);

int get_root_fd(int *root_fd);

int process_invoke_request_from_kernel_client(
		int fd, struct smcinvoke_cmd_req *req);

char *firmware_request_from_smcinvoke(const char *appname, size_t *fw_size, struct qtee_shm *shm);

#endif /* __SMCINVOKE_OBJECT_H */
