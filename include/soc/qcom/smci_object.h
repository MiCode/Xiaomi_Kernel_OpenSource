/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SMCINVOKE_OBJECT_H
#define __SMCINVOKE_OBJECT_H

#include <linux/firmware.h>
#include <linux/qtee_shmbridge.h>

/*
 * Method bits are not modified by transport layers.  These describe the
 * method (member function) being requested by the client.
 */

#define SMCI_OBJECT_OP_METHOD_MASK     (0x0000FFFFu)
#define SMCI_OBJECT_OP_METHODID(op)    ((op) & SMCI_OBJECT_OP_METHOD_MASK)
#define SMCI_OBJECT_OP_RELEASE       (SMCI_OBJECT_OP_METHOD_MASK - 0)
#define SMCI_OBJECT_OP_RETAIN        (SMCI_OBJECT_OP_METHOD_MASK - 1)
#define SMCI_OBJECT_OP_MAP_REGION    0
#define SMCI_OBJECT_OP_YIELD 1
#define SMCI_OBJECT_OP_SLEEP 2

#define SMCI_OBJECT_COUNTS_MAX_BI   0xF
#define SMCI_OBJECT_COUNTS_MAX_BO   0xF
#define SMCI_OBJECT_COUNTS_MAX_OI   0xF
#define SMCI_OBJECT_COUNTS_MAX_OO   0xF

/* unpack counts */

#define SMCI_OBJECT_COUNTS_NUM_BI(k)  ((size_t) (((k) >> 0) & SMCI_OBJECT_COUNTS_MAX_BI))
#define SMCI_OBJECT_COUNTS_NUM_BO(k)  ((size_t) (((k) >> 4) & SMCI_OBJECT_COUNTS_MAX_BO))
#define SMCI_OBJECT_COUNTS_NUM_OI(k)  ((size_t) (((k) >> 8) & SMCI_OBJECT_COUNTS_MAX_OI))
#define SMCI_OBJECT_COUNTS_NUM_OO(k)  ((size_t) (((k) >> 12) & SMCI_OBJECT_COUNTS_MAX_OO))
#define SMCI_OBJECT_COUNTS_NUM_BUFFERS(k)	\
			(SMCI_OBJECT_COUNTS_NUM_BI(k) + SMCI_OBJECT_COUNTS_NUM_BO(k))

#define SMCI_OBJECT_COUNTS_NUM_OBJECTS(k)	\
			(SMCI_OBJECT_COUNTS_NUM_OI(k) + SMCI_OBJECT_COUNTS_NUM_OO(k))

/* Indices into args[] */

#define SMCI_OBJECT_COUNTS_INDEX_BI(k)   0
#define SMCI_OBJECT_COUNTS_INDEX_BO(k)		\
			(SMCI_OBJECT_COUNTS_INDEX_BI(k) + SMCI_OBJECT_COUNTS_NUM_BI(k))
#define SMCI_OBJECT_COUNTS_INDEX_OI(k)		\
			(SMCI_OBJECT_COUNTS_INDEX_BO(k) + SMCI_OBJECT_COUNTS_NUM_BO(k))
#define SMCI_OBJECT_COUNTS_INDEX_OO(k)		\
			(SMCI_OBJECT_COUNTS_INDEX_OI(k) + SMCI_OBJECT_COUNTS_NUM_OI(k))
#define SMCI_OBJECT_COUNTS_TOTAL(k)		\
			(SMCI_OBJECT_COUNTS_INDEX_OO(k) + SMCI_OBJECT_COUNTS_NUM_OO(k))

#define SMCI_OBJECT_COUNTS_PACK(in_bufs, out_bufs, in_objs, out_objs) \
	((uint32_t) ((in_bufs) | ((out_bufs) << 4) | \
			((in_objs) << 8) | ((out_objs) << 12)))

#define SMCI_OBJECT_COUNTS_INDEX_BUFFERS(k)   SMCI_OBJECT_COUNTS_INDEX_BI(k)

/* smci_object_invoke return codes */

#define SMCI_OBJECT_IS_OK(err)        ((err) == 0)
#define SMCI_OBJECT_IS_ERROR(err)     ((err) != 0)

/* Generic error codes */

#define SMCI_OBJECT_OK                  0   /* non-specific success code */
#define SMCI_OBJECT_ERROR               1   /* non-specific error */
#define SMCI_OBJECT_ERROR_INVALID       2   /* unsupported/unrecognized request */
#define SMCI_OBJECT_ERROR_SIZE_IN       3   /* supplied buffer/string too large */
#define SMCI_OBJECT_ERROR_SIZE_OUT      4   /* supplied output buffer too small */

#define SMCI_OBJECT_ERROR_USERBASE     10   /* start of user-defined error range */

/* Transport layer error codes */

#define SMCI_OBJECT_ERROR_DEFUNCT     -90   /* smci_object no longer exists */
#define SMCI_OBJECT_ERROR_ABORT       -91   /* calling thread must exit */
#define SMCI_OBJECT_ERROR_BADOBJ      -92   /* invalid smci_object context */
#define SMCI_OBJECT_ERROR_NOSLOTS     -93   /* caller's smci_object table full */
#define SMCI_OBJECT_ERROR_MAXARGS     -94   /* too many args */
#define SMCI_OBJECT_ERROR_MAXDATA     -95   /* buffers too large */
#define SMCI_OBJECT_ERROR_UNAVAIL     -96   /* the request could not be processed */
#define SMCI_OBJECT_ERROR_KMEM        -97   /* kernel out of memory */
#define SMCI_OBJECT_ERROR_REMOTE      -98   /* local method sent to remote smci_object */
#define SMCI_OBJECT_ERROR_BUSY        -99   /* smci_object is busy */
#define SMCI_OBJECT_ERROR_TIMEOUT     -103  /* Call Back smci_object invocation timed out. */

#define FOR_ARGS(ndxvar, counts, section) \
	for (ndxvar = SMCI_OBJECT_COUNTS_INDEX_##section(counts); \
		ndxvar < (SMCI_OBJECT_COUNTS_INDEX_##section(counts) \
		+ SMCI_OBJECT_COUNTS_NUM_##section(counts)); \
		++ndxvar)

/* smci_objectOp */


#define SMCI_OBJECT_OP_LOCAL	((uint32_t) 0x00008000U)

#define SMCI_OBJECT_OP_IS_LOCAL(op)	(((op) & SMCI_OBJECT_OP_LOCAL) != 0)



/* smci_object */

union smci_object_arg;

typedef int32_t (*smci_object)(void *h,
				uint32_t op,
				union smci_object_arg *args,
				uint32_t counts);

struct smci_object {
	smci_object invoke;
	void *context;
};

struct smci_object_buf {
	void *ptr;
	size_t size;
};

struct smci_object_buf_in {
	const void *ptr;
	size_t size;
};

union smci_object_arg {
	struct smci_object_buf b;
	struct smci_object_buf_in bi;
	struct smci_object o;
};

static inline int32_t smci_object_invoke(struct smci_object o, uint32_t op,
				union smci_object_arg *args, uint32_t k)
{
	return o.invoke(o.context, op, args, k);
}

#define SMCI_OBJECT_NULL		((struct smci_object){NULL, NULL})


#define SMCI_OBJECT_NOT_RETAINED

#define SMCI_OBJECT_CONSUMED

static inline int32_t smci_object_release(SMCI_OBJECT_CONSUMED struct smci_object o)
{
	return smci_object_invoke((o), SMCI_OBJECT_OP_RELEASE, 0, 0);
}
static inline int32_t smci_object_retain(struct smci_object o)
{
	return smci_object_invoke((o), SMCI_OBJECT_OP_RETAIN, 0, 0);
}

#define SMCI_OBJECT_IS_NULL(o)	((o).invoke == NULL)

#define SMCI_OBJECT_RELEASE_IF(o)				\
	do {						\
		struct smci_object o_ = (o);			\
		if (!SMCI_OBJECT_IS_NULL(o_))			\
			(void) smci_object_release(o_);	\
	} while (0)

static inline void smci_object_replace(struct smci_object *loc, struct smci_object obj_new)
{
	if (!SMCI_OBJECT_IS_NULL(*loc))
		smci_object_release(*loc);

	if (!SMCI_OBJECT_IS_NULL(obj_new))
		smci_object_retain(obj_new);
	*loc = obj_new;
}

#define SMCI_OBJECT_ASSIGN_NULL(loc)  smci_object_replace(&(loc), SMCI_OBJECT_NULL)

/* API Exposed Functionality for Kernel Client to get root env smci_object */
int32_t get_client_env_object(struct smci_object *client_env_obj);

/* API to provide functionality to Client to load a firmware using shared memory Bridge
 * app name is the TA name which will load the firmware and we will search for firmware on
 * predefined path.
 */
char *firmware_request_from_smcinvoke(const char *appname, size_t *fw_size, struct qtee_shm *shm);

#endif /* __SMCI_OBJECT_H */
