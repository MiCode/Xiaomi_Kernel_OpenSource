// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/xarray.h>

#include "si_core.h"
#include "si_core_adci.h"

#if IS_ENABLED(CONFIG_QSEECOM_PROXY)
#include <linux/qseecom_kernel.h>
#include <soc/qcom/qseecomi.h>
#endif

/* Static 'Primordial Object' operations. */

#define OBJECT_OP_YIELD	1
#define OBJECT_OP_SLEEP	2

/* 'static_si_object_primordial' always exists! */
/* 'primordial_object_register' and 'primordial_object_release' extend it. */

static struct si_object static_si_object_primordial;

static int primordial_object_register(struct si_object *object);
static void primordial_object_release(struct si_object *object);

/* Marshaling API. */
/*
 * 'prepare_msg' Prepare input buffer for sending to QTEE.
 * 'update_args' Parse QTEE response in input buffer.
 * 'prepare_args' Parse QTEE request from output buffer.
 * 'update_msg' Update output buffer with response for QTEE request.
 *
 * 'prepare_msg' and 'update_args' are used in direct invocation path.
 * 'prepare_args' and 'update_msg' are used for QTEE request (callback or async).
 */

static int prepare_msg(struct si_object_invoke_ctx *oic,
	struct si_object *object, unsigned long op, struct si_arg u[]);
static int update_args(struct si_arg u[], struct si_object_invoke_ctx *oic);
static int prepare_args(struct si_object_invoke_ctx *oic);
static int update_msg(struct si_object_invoke_ctx *oic);

static int next_arg_type(struct si_arg u[], int i, enum arg_type type)
{
	while (u[i].type != SI_AT_END && u[i].type != type)
		i++;

	return i;
}

/**
 * args_for_each_type - Iterate over argument of given type.
 * @i: index in @args.
 * @args: array of arguments.
 * @at: type of argument.
 */
#define args_for_each_type(i, args, at) \
	for (i = 0, i = next_arg_type(args, i, at); \
		args[i].type != SI_AT_END; i = next_arg_type(args, ++i, at))

#define arg_for_each_input_buffer(i, args)  args_for_each_type(i, args, SI_AT_IB)
#define arg_for_each_output_buffer(i, args) args_for_each_type(i, args, SI_AT_OB)
#define arg_for_each_input_object(i, args)  args_for_each_type(i, args, SI_AT_IO)
#define arg_for_each_output_object(i, args) args_for_each_type(i, args, SI_AT_OO)

/* Outside this file we use 'struct si_object' to identify an object. */

/* We only allocate IDs with 'QTEE_OBJ_NS_BIT' set in range
 * ['SI_OBJECT_ID_START' .. SI_OBJECT_ID_END]. 'si_object' represents NS object.
 * The first ID with 'QTEE_OBJ_NS_BIT' set is reserved for primordial object.
 */

#define SI_OBJECT_PRIMORDIAL	(QTEE_OBJ_NS_BIT)
#define SI_OBJECT_ID_START		(SI_OBJECT_PRIMORDIAL + 1)
#define SI_OBJECT_ID_END		(UINT_MAX)

#define SET_SI_OBJECT(p, type, ...) __SET_SI_OBJECT(p, type, ##__VA_ARGS__, 0UL)
#define __SET_SI_OBJECT(p, type, optr, ...) do { \
		(p)->object_type = (type); \
		(p)->info.object_ptr = (unsigned long)(optr); \
		(p)->release = NULL; \
	} while (0)

struct si_object *allocate_si_object(void)
{
	struct si_object *object;

	/* TODO. Use slab?! **/

	object = kzalloc(sizeof(struct si_object), GFP_KERNEL);
	if (object)
		SET_SI_OBJECT(object, SI_OT_NULL);

	return object;
}
EXPORT_SYMBOL_GPL(allocate_si_object);

void free_si_object(struct si_object *object)
{
	kfree(object);
}
EXPORT_SYMBOL_GPL(free_si_object);

static DEFINE_XARRAY_ALLOC(xa_si_objects);

/* 'get_si_object' and 'put_si_object'. */

static int __free_si_object(struct si_object *object);
static void ____destroy_si_object(struct kref *refcount)
{
	struct si_object *object = container_of(refcount, struct si_object, refcount);

	__free_si_object(object);
}

int get_si_object(struct si_object *object)
{
	if (object != NULL_SI_OBJECT &&
		object != ROOT_SI_OBJECT)
		return kref_get_unless_zero(&object->refcount);

	return 0;
}
EXPORT_SYMBOL_GPL(get_si_object);

static struct si_object *qtee__get_si_object(unsigned int object_id)
{
	/* For input object QTEE guarantees their validity by not issuing any RELEASE
	 * through out the invocation (i.e. RCU is not necessary).
	 * The RCU is mostly here (1) if QTEE violate this rule, and (2) for output
	 * object where QTEE did not issue an appropriate RETAIN.
	 */

	XA_STATE(xas, &xa_si_objects, object_id);
	struct si_object *object;

	rcu_read_lock();
	do {
		object = xas_load(&xas);
		if (xa_is_zero(object)) {

			/* So that do not retry for 'XA_ZERO_ENTRY'. */
			object = NULL;
		}

	} while (xas_retry(&xas, object));

	if (!get_si_object(object))
		object = NULL;

	rcu_read_unlock();

	return object;
}

struct si_object *qtee_get_si_object(unsigned int object_id)
{
	switch (object_id) {
	case SI_OBJECT_PRIMORDIAL:
		return &static_si_object_primordial;

	default:
		return qtee__get_si_object(object_id);
	}
}

void put_si_object(struct si_object *object)
{
	if (object != &static_si_object_primordial &&
		object != NULL_SI_OBJECT &&
		object != ROOT_SI_OBJECT)
		kref_put(&object->refcount, ____destroy_si_object);
}
EXPORT_SYMBOL_GPL(put_si_object);

/* 'alloc_si_object_id' and 'erase_si_object'. */

static int alloc_si_object_id(struct si_object *object, u32 *idx)
{
	static u32 xa_si_last_id = SI_OBJECT_ID_START;

	/* Every ID allocated here, will have 'QTEE_OBJ_NS_BIT' set. */
	return xa_alloc_cyclic(&xa_si_objects, idx, object,
		XA_LIMIT(SI_OBJECT_ID_START, SI_OBJECT_ID_END),
			&xa_si_last_id, GFP_KERNEL);
}

struct si_object *erase_si_object(u32 idx)
{
	return xa_erase(&xa_si_objects, idx);
}

static int __free_si_object(struct si_object *object)
{
	/* This is used by si-core itself if it requires to do cleanup on the
	 * object before calling it's release operation.
	 */

	if (object->release)
		object->release(object);

	synchronize_rcu();

	switch (typeof_si_object(object)) {
	case SI_OT_USER:
		release_user_object(object);

		break;
	case SI_OT_CB_OBJECT: {
		/* Keep the name in case 'release' needs it! */
		const char *name = object->name;

		if (object->ops->release)
			object->ops->release(object);

		kfree_const(name);
		break;
	}
	case SI_OT_ROOT:
	case SI_OT_NULL:
	default:

		break;
	}

	return 0;
}

enum si_object_type si_object_type(unsigned int object_id)
{
	if (object_id == QTEE_OBJ_NULL)
		return SI_OT_NULL;

	if (object_id & QTEE_OBJ_NS_BIT)
		return SI_OT_CB_OBJECT;

	return SI_OT_USER;
}

/**
 * init_si_object_user - Initialize an instance of si_object.
 * @object: object to initialize.
 * @ot: type of object.
 * @ops: instance of callbacks.
 * @fmt: name assigned to the object.
 *
 * Return: On error, -EINVAL if the arguments are invalid.
 * On success, return zero.
 */
int init_si_object_user(struct si_object *object, enum si_object_type ot,
	struct si_object_operations *ops, const char *fmt, ...)
{
	int ret;
	va_list ap;

	kref_init(&object->refcount);
	SET_SI_OBJECT(object, SI_OT_NULL);

	/* **/
	/* 'init_si_object_user' only initializes 'si_object'. The 'object_id'
	 * allocation is postponed to 'get_object_id'. We want to use different
	 * IDs so user can decide to share a 'si_object'.
	 *
	 **/

	va_start(ap, fmt);
	switch (ot) {
	case SI_OT_NULL:

		ret = 0;
		break;
	case SI_OT_CB_OBJECT:
	case SI_OT_ROOT:
		object->ops = ops;
		if (!object->ops->dispatch)
			return -EINVAL;

		object->name = kvasprintf_const(GFP_KERNEL, fmt, ap);
		if (!object->name)
			return -ENOMEM;

		SET_SI_OBJECT(object, SI_OT_CB_OBJECT);

		if (ot == SI_OT_ROOT) {
			object->release = primordial_object_release;

			/* Finally, REGISTER it. */
			primordial_object_register(object);
		}

		ret = 0;
		break;
	case SI_OT_USER:
	default:
		ret = -EINVAL;
	}
	va_end(ap);

	return ret;
}
EXPORT_SYMBOL_GPL(init_si_object_user);

/* 'init_si_object' is to be consumed internally on return path.
 * It is used for processing input objects.
 */

static int init_si_object(struct si_object **object, unsigned int object_id)
{
	int ret;

	switch (si_object_type(object_id)) {
	case SI_OT_NULL:

		/* Should we receive ''SI_OT_NULL'' from QTEE!? Why not. **/
		*object = NULL_SI_OBJECT;

		ret = 0;
		break;
	case SI_OT_CB_OBJECT: {
		struct si_object *t_object = qtee_get_si_object(object_id);

		if (t_object != NULL) {
			*object = t_object;

			ret = 0;
		} else
			ret = -EINVAL;

		break;
	}
	case SI_OT_USER: {
		struct si_object *t_object = allocate_si_object();

		if (t_object != NULL) {
			kref_init(&t_object->refcount);

			/* "no-name"; it is not really a reason to fail here!. */
			t_object->name = kasprintf(GFP_KERNEL, "qtee-%u", object_id);

			SET_SI_OBJECT(t_object, SI_OT_USER, object_id);

			*object = t_object;

			ret = 0;
		} else
			ret = -ENOMEM;

		break;
	}
	default:
		/* Err. SHOULD NEVER GET HERE! **/

		ret = 0;

		break;
	}

	if (ret) {

		/* If unable to obtain an 'si_object' instance, set to NULL. */

		*object = NULL_SI_OBJECT;
	}

	return ret;
}

/* 'get_object_id' is to be consumed internally on direct path to QTEE.
 * Unlike 'init_si_object' 'get_object_id' does not increase the object's
 * reference counter, i.e. the client should do that.
 */

int get_object_id(struct si_object *object, unsigned int *object_id)
{
	int ret;

	switch (typeof_si_object(object)) {
	case SI_OT_CB_OBJECT: {
		u32 idx;

		ret = alloc_si_object_id(object, &idx);
		if (ret < 0)
			goto out;

		*object_id = idx;

		ret = 0;
	}

		break;
	case SI_OT_USER:
		*object_id = object->info.object_ptr;

		ret = 0;
		break;
	case SI_OT_NULL:
		*object_id = QTEE_OBJ_NULL;

		ret = 0;
		break;
	case SI_OT_ROOT:
		*object_id = QTEE_OBJ_ROOT;

		ret = 0;
		break;
	default:
		return -EBADF;
	}

out:

	return ret;
}

void __put_object_id(unsigned int object_id)
{
	/* Release 'idx' allocated in 'get_object_id'. **/

	erase_si_object(object_id);
}

/* Context management API */

/* 'shmem_alloc',
 * 'si_object_invoke_ctx_init', and
 * 'si_object_invoke_ctx_uninit'.
 */

#define OUT_BUFFER_SIZE SZ_32K

static DEFINE_IDA(si_object_invoke_ctxs_ida);

static int shmem_alloc(struct si_object_invoke_ctx *oic, struct si_arg u[])
{
	int i;

	/* See 'prepare_msg'. Calculate size of inbound message. */

	size_t size = OFFSET_TO_BUFFER_ARGS((struct qtee_object_invoke *)(0), size_of_arg(u));

	arg_for_each_input_buffer(i, u)
		size = align_offset(u[i].b.size + size);

	arg_for_each_output_buffer(i, u)
		size = align_offset(u[i].b.size + size);

	/* QTEE requires both input and output buffer
	 *   (1) to be PAGE_SIZE aligned and
	 *   (2) to be multiple of PAGE_SIZE.
	 */

	/* We assume 'qtee_shmbridge_allocate_shm' allocates PAGE_SIZE aligned memory. */

	size = PAGE_ALIGN(size);

	/* Get inbound buffer. */
	if (qtee_shmbridge_allocate_shm(size, &oic->in.shm))
		return -ENOMEM;

	/* Get outbound buffer. */
	if (qtee_shmbridge_allocate_shm(OUT_BUFFER_SIZE, &oic->out.shm)) {
		qtee_shmbridge_free_shm(&oic->in.shm);

		return -ENOMEM;
	}

	oic->in.msg.addr = oic->in.shm.vaddr;
	oic->in.msg.size = size;
	oic->in.paddr = oic->in.shm.paddr;

	oic->out.msg.addr = oic->out.shm.vaddr;
	oic->out.msg.size = OUT_BUFFER_SIZE;
	oic->out.paddr = oic->out.shm.paddr;

	/* QTEE assume unused buffers are zeroed; Do it now! */
	memset(oic->in.msg.addr, 0, oic->in.msg.size);
	memset(oic->out.msg.addr, 0, oic->out.msg.size);

	return 0;
}

static int si_object_invoke_ctx_init(struct si_object_invoke_ctx *oic, struct si_arg u[])
{
	memset(oic, 0, sizeof(*oic));

	/* First check if we can allocate an ID, then initialize it. */
	/* Context IDa [0 .. 10) are never used. */

	oic->context_id = ida_alloc_min(&si_object_invoke_ctxs_ida, 10, GFP_KERNEL);
	if (oic->context_id < 0) {
		pr_err("unable to allocate context ID (%d)\n", oic->context_id);

		return oic->context_id;
	}

	if (shmem_alloc(oic, u)) {
		ida_free(&si_object_invoke_ctxs_ida, oic->context_id);

		return -ENOMEM;
	}

	INIT_LIST_HEAD(&oic->objects_head);

	return 0;
}

static void si_object_invoke_ctx_uninit(struct si_object_invoke_ctx *oic)
{
	ida_free(&si_object_invoke_ctxs_ida, oic->context_id);

	qtee_shmbridge_free_shm(&oic->in.shm);
	qtee_shmbridge_free_shm(&oic->out.shm);
}

/* For X_msg functions, on failure we do the cleanup. Because, we could not
 * construct a message to send so the caller remains the owner of the objects.
 * For X_args functions, on failure wo do ''not'' do a cleanup. Because,
 * we received the message and receiver should be the new owner to cleanup.
 */

static int prepare_msg(struct si_object_invoke_ctx *oic,
	struct si_object *object, unsigned long op, struct si_arg u[])
{
	int i, ib = 0, ob = 0, io = 0, oo = 0;

	unsigned int object_id;

	/* Use input message buffer in 'oic'. */

	struct qtee_object_invoke *msg = (struct qtee_object_invoke *)oic->in.msg.addr;
	size_t msg_size = oic->in.msg.size;

	/* Start offset in a message for buffer argument. */

	unsigned int offset = OFFSET_TO_BUFFER_ARGS(msg, size_of_arg(u));

	if (get_object_id(object, &object_id))
		return -ENOSPC;

	arg_for_each_input_buffer(i, u) {
		void *msg_ptr;

		msg->args[ib].b.offset = offset;
		msg->args[ib].b.size = u[i].b.size;
		if (!arg_in_bounds(&msg->args[ib], msg_size))
			return -ENOMEM;

		msg_ptr = OFFSET_TO_PTR(msg, offset);

		if (!u[i].flags)
			memcpy(msg_ptr, u[i].b.addr, u[i].b.size);
		else if (copy_from_user(msg_ptr, u[i].b.uaddr, u[i].b.size))
			return -EFAULT;

		offset = align_offset(u[i].b.size + offset);
		ib++;
	}

	ob = ib;
	arg_for_each_output_buffer(i, u) {
		msg->args[ob].b.offset = offset;
		msg->args[ob].b.size = u[i].b.size;
		if (!arg_in_bounds(&msg->args[ob], msg_size))
			return -ENOMEM;

		offset = align_offset(u[i].b.size + offset);
		ob++;
	}

	io = ob;
	arg_for_each_input_object(i, u) {
		if (get_object_id(u[i].o, &msg->args[io].o)) {

			/* Unable to 'get_object_id'; 'put' whatever we got. **/

			__put_object_id(object_id);
			for (--io; io >= ob; io--)
				__put_object_id(msg->args[io].o);

			return -ENOSPC;
		}

		io++;
	}

	oo = io;
	arg_for_each_output_object(i, u)
		oo++;

	/* Set object, operation, and argument counts. */

	init_oi_msg(msg, object_id, op, ib, ob, io, oo);

	return 0;
}

static int update_args(struct si_arg u[], struct si_object_invoke_ctx *oic)
{
	int ret = 0;

	int i, ib = 0, ob = 0, io = 0, oo = 0;

	/* Use input message buffer in 'oic'. */

	struct qtee_object_invoke *msg = (struct qtee_object_invoke *)oic->in.msg.addr;

	arg_for_each_input_buffer(i, u)
		ib++;

	ob = ib;
	arg_for_each_output_buffer(i, u) {
		void *msg_ptr = OFFSET_TO_PTR(msg, msg->args[ob].b.offset);

		if (!u[i].flags) {
			memcpy(u[i].b.addr, msg_ptr, msg->args[ob].b.size);
		} else if (copy_to_user(u[i].b.uaddr, msg_ptr, msg->args[ob].b.size)) {
			/* On failour, continue so that we process output objects for RELEASE.*/
			ret = -EFAULT;
		}

		u[i].b.size = msg->args[ob].b.size;
		ob++;
	}

	io = ob;
	arg_for_each_input_object(i, u)
		io++;

	oo = io;
	arg_for_each_output_object(i, u) {
		int err;

		/* **/
		/* If 'init_si_object' returns error (e.g. requested handle is invalid or
		 * 'init_si_object' is unable to allocate 'si_object'), we continue to
		 * process arguments. It is necessary so that latter we can issue the 'RELEASE'.
		 *
		 * If 'init_si_object' failed to allocated the 'si_object', we could not
		 * release that object.
		 *
		 **/

		err = init_si_object(&u[i].o, msg->args[oo].o);
		if (err)
			ret = err;

		oo++;
	}

	return ret;
}

static int prepare_args(struct si_object_invoke_ctx *oic)
{
	/* We initialise user arguments based on the callback message.
	 * It is important to preserve the order of arguments, i.e. 'SI_AT_IB', 'SI_AT_OB',
	 * following by 'SI_AT_IO', and 'SI_AT_OO'.
	 */

	int ret = 0;

	int i;

	/* Use output message buffer in 'oic'. */

	struct qtee_callback *msg = (struct qtee_callback *)oic->out.msg.addr;

	/* We assume QTEE already checked the buffer boundaries! */

	for_each_input_buffer(i, msg->counts) {
		oic->u[i].b.addr = OFFSET_TO_PTR(msg, msg->args[i].b.offset);
		oic->u[i].b.size = msg->args[i].b.size;
		oic->u[i].type = SI_AT_IB;
	}

	for_each_output_buffer(i, msg->counts) {
		oic->u[i].b.addr = OFFSET_TO_PTR(msg, msg->args[i].b.offset);
		oic->u[i].b.size = msg->args[i].b.size;
		oic->u[i].type = SI_AT_OB;
	}

	for_each_input_object(i, msg->counts) {
		int err;

		/* See comments for 'for_each_output_object' in 'update_args'. **/

		err = init_si_object(&oic->u[i].o, msg->args[i].o);
		if (err)
			ret = err;

		oic->u[i].type = SI_AT_IO;
	}

	for_each_output_object(i, msg->counts)
		oic->u[i].type = SI_AT_OO;

	/* End of Arguments. */

	oic->u[i].type = SI_AT_END;

	return ret;
}

static int update_msg(struct si_object_invoke_ctx *oic)
{
	int i, ib = 0, ob = 0, io = 0, oo = 0;

	/* Use output message buffer in 'oic'. */

	struct qtee_callback *msg = (struct qtee_callback *)oic->out.msg.addr;

	arg_for_each_input_buffer(i, oic->u)
		ib++;

	ob = ib;
	arg_for_each_output_buffer(i, oic->u) {

		/* Only reduce size of client requested that; never increase it. */

		if (msg->args[ob].b.size < oic->u[i].b.size)
			return -EINVAL;

		msg->args[ob].b.size = oic->u[i].b.size;

		ob++;
	}

	io = ob;
	arg_for_each_input_object(i, oic->u)
		io++;

	oo = io;
	arg_for_each_output_object(i, oic->u) {
		if (get_object_id(oic->u[i].o, &msg->args[oo].o)) {

			/* Unable to 'get_object_id'; 'put' whatever we got. **/
			for (--oo; oo >= io; --oo)
				__put_object_id(msg->args[oo].o);

			return -ENOSPC;
		}

		oo++;
	}

	return 0;
}

/* Invoke an 'si_object' instance. */

static void si_object_invoke(struct si_object_invoke_ctx *oic, struct qtee_callback *msg)
{
	int i, errno;

	/* Get object being invoked!!! */
	unsigned int object_id = msg->cxt;
	struct si_object *object;

	/* QTEE can not invoke NULL object or objects it hosts. */
	if (si_object_type(object_id) == SI_OT_NULL ||
		si_object_type(object_id) == SI_OT_USER) {
		errno = -EINVAL;

		goto out;
	}

	object = qtee_get_si_object(object_id);
	if (!object) {
		errno = -EINVAL;

		goto out;
	}

	oic->object = object;

	switch (SI_OBJECT_OP_METHOD_ID(msg->op)) {
	case SI_OBJECT_OP_RELEASE:

		/* Remove the 'object' from 'xa_si_objects' so that the 'object_id'
		 * becomes invalid for further use. However, call 'put_si_object'
		 * to schedule the actual release if there is no user.
		 */

		erase_si_object(object_id);
		put_si_object(object);
		errno = 0;

		break;
	case SI_OBJECT_OP_RETAIN:
		get_si_object(object);
		errno = 0;

		break;
	default:

		/* Check if the operation is supported before going forward. */
		if (object->ops->op_supported) {
			if (object->ops->op_supported(msg->op)) {
				errno = -EINVAL;

				break;
			}
		}

		errno = prepare_args(oic);
		if (errno) {

			/* Unable to parse the message. Release any object arrived as input. */
			arg_for_each_input_buffer(i, oic->u)
				put_si_object(oic->u[i].o);

			break;
		}

		errno = object->ops->dispatch(oic->context_id,
			/* .dispatch(Object, Operation, Arguments). */
			object, msg->op, oic->u);

		if (!errno) {

			/* On SUCCESS, notify 'object' at appropriate time. */
			oic->flags |= OIC_FLAG_NOTIFY;
		}

	}

	switch (errno) {
	case 0:

		break;

	case -ERESTARTSYS:
	case -ERESTARTNOINTR:
	case -ERESTARTNOHAND:
	case -ERESTART_RESTARTBLOCK:

		/* There's no easy way to restart the syscall that end up in callback
		 * object invocation. Just fail the call with EINTR.
		 */

		/* We do not do any cleanup for input objects. */

		errno = -EINTR;

		fallthrough;
	default:

		/* On error, dispatcher should do the cleanup. */

		break;
	}

out:

	oic->errno = errno;
}

/**
 * si_object_do_invoke - Submit an invocation for si_object_invoke_ctx.
 * @oic: context to use for current invocation.
 * @object: object being invoked.
 * @op: requested operation on @object.
 * @u: array of argument for the current invocation.
 * @result: result return from QTEE.
 *
 * The caller is responsible to keep track of the refcount for each object,
 * including @object. On return (success or failure), the caller loses the
 * ownership of all input object of type SI_OT_CB_OBJECT.
 *
 * Return: On success return 0. On failure returns -EINVAL if unable to parse the
 * request or response. It returns -ENODEV if it can not communicate with QTEE, or
 * -EAGAIN if it can not communicate with QTEE but it is safe for the caller to
 * retry the call (after getting IO again as they are put on return). It returns
 * -ENOMEM if memory could not be allocated, or -ENOSPC if there is not free
 * context ID or QTEE handler.
 */
int si_object_do_invoke(struct si_object_invoke_ctx *oic,
	struct si_object *object, unsigned long op, struct si_arg u[], int *result)
{
	int i, ret, errno;
	unsigned int data;
	u64 response_type;

	struct qtee_callback *cb_msg;

	if (typeof_si_object(object) != SI_OT_USER &&
		typeof_si_object(object) != SI_OT_ROOT)
		return -EINVAL;

	ret = si_object_invoke_ctx_init(oic, u);
	if (ret)
		return ret;

	pr_debug("start an invocation for %s.\n", si_object_name(object));

	ret = prepare_msg(oic, object, op, u);
	if (ret)
		goto out;

	/* INVOKE remote object!! */

	cb_msg = (struct qtee_callback *)oic->out.msg.addr;

	while (1) {
		if (oic->flags & OIC_FLAG_BUSY) {
			errno = oic->errno;

			/* Update output buffer only if result is SUCCESS. */
			if (!errno)
				errno = update_msg(oic);

			err_to_qtee_err(cb_msg, errno);
		}

		/* APPEND async requests before handing the output buffer over to QTEE. */
		__append__async_reqs(oic);

		ret = si_object_invoke_ctx_invoke(oic, result, &response_type, &data);

		if (oic->flags & OIC_FLAG_BUSY) {
			struct si_object *t_object = oic->object;

			/* A busy 'oic' can have a 'NULL_SI_OBJECT' object if
			 * 'si_object_invoke' fails, internally.
			 */

			if (t_object) {

				/* Only notify if we should!? */
				if (oic->flags & OIC_FLAG_NOTIFY) {
					if (t_object->ops->notify)
						t_object->ops->notify(oic->context_id,
							t_object, (errno | ret));
				}

				put_si_object(t_object);
			}

			/* 'oic' is done. Cleanup. */
			oic->object = NULL_SI_OBJECT;
			oic->flags &= ~(OIC_FLAG_BUSY | OIC_FLAG_NOTIFY);
		}

		if (ret) {

			/* 'si_object_invoke_ctx_invoke' failed; We can not recover from this. */

			/* We never get a chance to communicate with QTEE.
			 * RETRIEVE all pending async requests queued for this context.
			 */

			__revive__async_queued_reqs(oic);

			/* SI_OT_CB_OBJECT input objects are orphan; let's put them. */
			if (!(oic->flags & OIC_FLAG_QTEE)) {
				arg_for_each_input_object(i, u)
					if (typeof_si_object(u[i].o) == SI_OT_CB_OBJECT)
						put_si_object(u[i].o);

				/* So QTEE is unaawre of this.
				 * Let the caller know in case they want to do something about it,
				 * e.g. retry the call.
				 */

				ret = -EAGAIN;

			} else {

				/* So QTEE is aware of this.
				 * On error, there is no clean way to clean up.
				 */

				ret = -ENODEV;
			}

			goto out;

		} else {

			/* QTEE obtained the ownership of SI_OT_CB_OBJECT input objects in 'u'.
			 * On further failure, QTEE is responsible to release them.
			 */

			oic->flags |= OIC_FLAG_QTEE;
		}

		/* SUCCESS. Release async request queued for this context.*/
		__release__async_queued_reqs(oic);

		/* Is it not a callback request?! */
		if (response_type == QTEE_RESULT_INBOUND_REQ_NEEDED) {
			oic->flags |= OIC_FLAG_BUSY;

			/* Before dispatching the request, handle any pending async requests. */
			__fetch__async_reqs(oic);

			si_object_invoke(oic, cb_msg);

		} else {
#if IS_ENABLED(CONFIG_QSEECOM_PROXY)
			if (response_type == QSEOS_RESULT_INCOMPLETE ||
			response_type == QSEOS_RESULT_BLOCKED_ON_LISTENER) {
				ret = qseecom_process_listener_from_smcinvoke(result,
					&response_type, &data);
				if (ret)
					pr_err("qseecom bridge failed with= %d\n", ret);
			}
#endif

			if (!*result) {
				ret = update_args(u, oic);
				if (ret) {
					arg_for_each_output_object(i, u)
						put_si_object(u[i].o);
				}
			}

			break;

		}
	}

	__fetch__async_reqs(oic);

out:
	si_object_invoke_ctx_uninit(oic);

	return ret;
}
EXPORT_SYMBOL_GPL(si_object_do_invoke);

/* Primordial Object. */
/* It is invoked by QTEE for kernel services. */

static struct si_object *primordial_object = NULL_SI_OBJECT;
static DEFINE_MUTEX(primordial_object_lock);

static int primordial_object_register(struct si_object *object)
{
	/* A primordial_object is a valid callback object. */
	if (typeof_si_object(object) != SI_OT_CB_OBJECT)
		return -EINVAL;

	/* Finally, REGISTER it. */

	/* We do not check if there is any other object registered. */
	/* This allows safe transition from one object to another without effecting QTEE. */

	mutex_lock(&primordial_object_lock);
	rcu_assign_pointer(primordial_object, object);
	mutex_unlock(&primordial_object_lock);

	return 0;
}

static void primordial_object_release(struct si_object *object)
{
	mutex_lock(&primordial_object_lock);

	/* Only reset 'primordial_object' if it points to this object. */
	if (primordial_object == object)
		rcu_assign_pointer(primordial_object, NULL_SI_OBJECT);

	mutex_unlock(&primordial_object_lock);
}

static struct si_object *get_primordial_object(void)
{
	struct si_object *object;

	rcu_read_lock();
	object = rcu_dereference(primordial_object);

	if (!get_si_object(object))
		object = NULL_SI_OBJECT;

	rcu_read_unlock();

	return object;
}

static int op_sleep(struct si_arg args[])
{
	if (size_of_arg(args) != 1 || args[0].type != SI_AT_IB)
		return -EINVAL;

	msleep(*(u32 *)(args[0].b.addr));

	return 0;
}

static int do_primordial_object_dispatch(unsigned int context_id,
	struct si_object *primordial_object, unsigned long op, struct si_arg args[])
{
	int i, ret = -EINVAL;

	struct si_object *object;

	/* Static 'primordial_object': Unused here! */

	switch (op) {
	case OBJECT_OP_YIELD:
		ret = 0;

		break;
	case OBJECT_OP_SLEEP:
		ret = op_sleep(args);

		break;
	default:
		object = get_primordial_object();

		if (object) {
			pr_info("QTEE invocation for %s, op %lu.\n", si_object_name(object), op);

			ret = object->ops->dispatch(context_id,
				/* .dispatch(Object, Operation, Arguments). */
				object, op, args);

			put_si_object(object);
		} else {
			pr_err("No primordial object registered.\n");

			/* Release any object arrived as input. */
			arg_for_each_input_object(i, args)
				put_si_object(args[i].o);
		}
	}

	return ret;
}

static struct si_object_operations primordial_ops = {
	.dispatch = do_primordial_object_dispatch
};

static struct si_object static_si_object_primordial = {
	.object_type = SI_OT_CB_OBJECT,
	.ops = &primordial_ops
};

/* Dump QTEE object table. */
static ssize_t ot_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct si_object *object;
	unsigned long idx;
	size_t len = 0;

	rcu_read_lock();
	xa_for_each_start(&xa_si_objects, idx, object, SI_OBJECT_ID_START) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%lx %4d %s\n",
			idx, kref_read(&object->refcount), si_object_name(object));
	}
	rcu_read_unlock();

	return len;
}

/* Info for registered primordial object. */
static ssize_t po_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct si_object *object = get_primordial_object();
	size_t len = 0;

	if (object) {
		len = scnprintf(buf, PAGE_SIZE, "%s %d\n",
			/* minus one for the above 'get_primordial_object'. */
			si_object_name(object), kref_read(&object->refcount) - 1);
		put_si_object(object);
	}

	return len;
}

/* Defined in si_core_async.c. */
ssize_t release_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

static struct kobj_attribute ot = __ATTR_RO(ot);
static struct kobj_attribute po = __ATTR_RO(po);
static struct kobj_attribute release = __ATTR_RO(release);
static struct attribute *attrs[] = {
	&ot.attr,
	&po.attr,
	&release.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *si_core_kobj;
static int __init si_core_init(void)
{
	int ret;

	ret = init_si_core_wq();
	if (ret)
		return ret;

	/* Create '/sys/kernel/si_core'. */
	si_core_kobj = kobject_create_and_add("si_core", kernel_kobj);
	if (!si_core_kobj) {
		destroy_si_core_wq();

		return -ENOMEM;
	}

	ret = sysfs_create_group(si_core_kobj, &attr_group);
	if (ret) {
		kobject_put(si_core_kobj);
		destroy_si_core_wq();
	}

	adci_start();

	return ret;
}

static void __exit si_core_exit(void)
{
	/* TODO. Prevent unloading if 'xa_si_objects' is not empty. */

	adci_shutdown();
	sysfs_remove_group(si_core_kobj, &attr_group);

	kobject_put(si_core_kobj);
}

module_init(si_core_init);
module_exit(si_core_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SI CORE driver");
