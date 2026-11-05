/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SI_CORE_H__
#define __SI_CORE_H__

#include <linux/qtee_shmbridge.h>
#include <linux/firmware/qcom/si_object.h>

#undef pr_fmt
#define pr_fmt(fmt) "si-core: %s: " fmt, __func__

/* QTEE object ID API. */

enum si_object_type si_object_type(unsigned int object_id);

/* 'get_object_id' allocates a QTEE handler for a si_object. */
/* '__put_object_id' erases the QTEE handler. */
/* 'qtee_get_si_object' returns si_object for a QTEE handler and increase the refcount. */

int get_object_id(struct si_object *object, unsigned int *object_id);
void __put_object_id(unsigned int object_id);
struct si_object *qtee_get_si_object(unsigned int object_id);

int si_object_invoke_ctx_invoke(struct si_object_invoke_ctx *oic, int *result,
	u64 *response_type, unsigned int *data);

#ifdef CONFIG_QCOM_SI_CORE_WQ
int init_si_core_wq(void);
void destroy_si_core_wq(void);
#else
static inline int init_si_core_wq(void) { return 0; }
static inline void destroy_si_core_wq(void) { }
#endif /* CONFIG_QCOM_SI_CORE_WQ */

void release_user_object(struct si_object *object);

/* ASYNC message management API. */

void __append__async_reqs(struct si_object_invoke_ctx *oic);
void __revive__async_queued_reqs(struct si_object_invoke_ctx *oic);
void __release__async_queued_reqs(struct si_object_invoke_ctx *oic);
void __fetch__async_reqs(struct si_object_invoke_ctx *oic);

/* ''QTEE'' related definitions. */

#define QTEE_RESULT_INBOUND_REQ_NEEDED 3

#define INVOKE_MESSAGE_ALIGN_BYTES 8U

#define QTEE_OBJ_NULL (0U)
#define QTEE_OBJ_ROOT (1U)

#define QTEE_OBJ_NS_BIT (1U << 31)

#define align_offset(o) PTR_ALIGN((o), INVOKE_MESSAGE_ALIGN_BYTES)

/* Definitions from QTEE as part of the transport protocol. */
/* 'qtee_smcinvoke_msg_arg', 'struct qtee_object_invoke', and 'struct qtee_callback'. */

union qtee_smcinvoke_msg_arg {
	struct {
		u32 offset;
		u32 size;
	} b;
	u32 o;
};

/* Check if a buffer argument 'arg' can fit in a message of size 'sz'. */
#define arg_in_bounds(arg, sz) \
	(((arg)->b.offset < (sz)) && ((arg)->b.size < ((sz) - (arg)->b.offset)))

struct qtee_object_invoke {
	u32 cxt;
	u32 op;
	u32 counts;
	union qtee_smcinvoke_msg_arg args[];
};

struct qtee_callback {
	u32 result;
	u32 cxt;
	u32 op;
	u32 counts;
	union qtee_smcinvoke_msg_arg args[];
};

#define OFFSET_TO_PTR(m, off) ((void *)&((char *)(m))[(off)])

/* Offset in the message for the beginning of buffer argument's contents. */
#define OFFSET_TO_BUFFER_ARGS(m, n) \
	align_offset(offsetof(typeof(*m), args) + (n * sizeof((m)->args[0])))

#define counts_num__bi_(x) (((x) >> 0) & 0xFU)
#define counts_num__bo_(x) (((x) >> 4) & 0xFU)
#define counts_num__oi_(x) (((x) >> 8) & 0xFU)
#define counts_num__oo_(x) (((x) >> 12) & 0xFU)

#define counts_idx__bi_(x) 0U
#define counts_idx__bo_(x) (counts_idx__bi_(x) + counts_num__bi_(x))
#define counts_idx__oi_(x) (counts_idx__bo_(x) + counts_num__bo_(x))
#define counts_idx__oo_(x) (counts_idx__oi_(x) + counts_num__oi_(x))
#define counts_total(x) (counts_idx__oo_(x) + counts_num__oo_(x))

#define FOR_ARGS(i, c, type) \
	for (i = counts_idx##type(c); i < (counts_idx##type(c) + counts_num##type(c)); i++)

#define for_each_input_buffer(i, c)  FOR_ARGS(i, c, __bi_)
#define for_each_output_buffer(i, c) FOR_ARGS(i, c, __bo_)
#define for_each_input_object(i, c)  FOR_ARGS(i, c, __oi_)
#define for_each_output_object(i, c) FOR_ARGS(i, c, __oo_)

#define bi_shift 0
#define ob_shift 4
#define io_shift 8
#define oo_shift 12

static inline void init_oi_msg(struct qtee_object_invoke *msg,
	u32 cxt, u32 op, int ib, int ob, int io, int oo)
{
	u32 counts = 0;

	counts |= ((oo - io) & 0xFU) << oo_shift;	/* No. Output Objects. */
	counts |= ((io - ob) & 0xFU) << io_shift;	/* No. Input Objects. */
	counts |= ((ob - ib) & 0xFU) << ob_shift;	/* No. Output Buffer. */
	counts |= (ib & 0xFU) << bi_shift;			/* No. Input Buffer. */

	msg->cxt = cxt;
	msg->op = op;
	msg->counts = counts;
}

static inline void err_to_qtee_err(struct qtee_callback *cb_msg, int err)
{

/* Generic error codes */

#define OBJECT_OK				0	/* non-specific success code */
#define OBJECT_ERROR			1	/* non-specific error */
#define OBJECT_ERROR_INVALID	2	/* unsupported/unrecognized request */
#define OBJECT_ERROR_SIZE_IN	3	/* supplied buffer/string too large */
#define OBJECT_ERROR_SIZE_OUT	4	/* supplied output buffer too small */

#define OBJECT_ERROR_USERBASE	10	/* start of user-defined error range */

/* Transport layer error codes */

#define OBJECT_ERROR_DEFUNCT	-90	/* object no longer exists */
#define OBJECT_ERROR_ABORT		-91	/* calling thread must exit */
#define OBJECT_ERROR_BADOBJ		-92	/* invalid object context */
#define OBJECT_ERROR_NOSLOTS	-93	/* caller's object table full */
#define OBJECT_ERROR_MAXARGS	-94	/* too many args */
#define OBJECT_ERROR_MAXDATA	-95	/* buffers too large */
#define OBJECT_ERROR_UNAVAIL	-96	/* the request could not be processed */
#define OBJECT_ERROR_KMEM		-97	/* kernel out of memory */
#define OBJECT_ERROR_REMOTE		-98	/* local method sent to remote object */
#define OBJECT_ERROR_BUSY		-99	/* Object is busy */
#define OBJECT_ERROR_TIMEOUT	-103	/* Call Back Object invocation timed out. */

	switch (err) {
	case 0:
		cb_msg->result = OBJECT_OK;

		break;
	case -ENOMEM:
		cb_msg->result = OBJECT_ERROR_KMEM;

		break;
	case -ENODEV:
		cb_msg->result = OBJECT_ERROR_DEFUNCT;

		break;
	case -ENOSPC:
	case -EBUSY:
		cb_msg->result = OBJECT_ERROR_BUSY;

		break;
	case -EBADF:
		cb_msg->result = OBJECT_ERROR_UNAVAIL;

		break;
	case -EINVAL:
		cb_msg->result = OBJECT_ERROR_INVALID;

		break;
	default:
		/* Positive err. are sent back as-is, negatives are transport related. */
		cb_msg->result = (err >= OBJECT_OK) ? err : OBJECT_ERROR;
	}
}

#endif /* __SI_CORE_H__ */
