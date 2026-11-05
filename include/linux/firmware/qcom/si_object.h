/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _LINUX_SI_OBJECT_H__
#define _LINUX_SI_OBJECT_H__

#include <linux/kref.h>
#include <linux/completion.h>

/* Primordial Object */

/* It is used for bootstrapping the Mink IPC connection between a VM and QTEE.
 *
 * Each side (both the VM and the QTEE) starts up with no object received from the
 * other side. They both ''assume'' the other side implements a permanent initial
 * object in the object table.
 *
 * QTEE's initial object is typically called the ''root client env'', and it's
 * invoked by VMs when they want to get a new clientEnv. The initial object created
 * by the VMs is invoked by QTEE, it's typically called the ''primordial object''.
 *
 * To gracefully SWITCH the primordial object, use 'init_si_object_user' with
 * 'SI_OT_ROOT' type and 'put_si_object' on the previous primordial object. si-core
 * will issue the release on the old primordial object if QTEE is not using it.
 */

enum si_object_type {
	SI_OT_USER = 0x1,		/* QTEE object. */
	SI_OT_CB_OBJECT = 0x2,	/* Callback Object. */
	SI_OT_ROOT = 0x8,		/* ''Root client env.'' or 'primordial' Object. */
	SI_OT_NULL = 0x10,		/* NULL object. */
};

/* Maximum number of argument that can fit in a QTEE message. */
#define MAX_ARGS 64

struct si_object;

/**
 * struct si_arg - argument for QTEE object invocation.
 * @type: type of argument
 * @flags: extra flags.
 * @b: address and size if type of argument is buffer
 * @o: si_object instance if type of argument is object
 */
struct si_arg {
	enum arg_type {
		SI_AT_END = 0,
		SI_AT_IB,	/* Input Buffer.  */
		SI_AT_OB,	/* Output Buffer. */
		SI_AT_IO,	/* Input Object.  */
		SI_AT_OO	/* Output Object. */
	} type;

/* 'uaddr' holds a __user address. */
#define SI_ARG_FLAGS_UADDR 1
	char flags;
	union {
		struct si_buffer {
			union {
				void *addr;
				void __user *uaddr;
			};
			size_t size;
		} b;
		struct si_object *o;
	};
};

static inline int size_of_arg(struct si_arg u[])
{
	int i = 0;

	while (u[i].type != SI_AT_END)
		i++;

	return i;
}

/* Context ID - It is a unique ID assigned to a invocation which is in progress.
 * Objects's dispatcher can use the ID to differentiate between concurrent calls.
 * ID [0 .. 10) are reserved, i.e. never passed to object's dispatcher.
 */

struct si_object_invoke_ctx {
	unsigned int context_id;

#define OIC_FLAG_BUSY		1	/* Context is busy. */
#define OIC_FLAG_NOTIFY		2	/* Context needs to notify the current object. */
#define OIC_FLAG_QTEE		4	/* Context has objects shared with QTEE. */
	unsigned int flags;

	/* Current object invoked in this callback context. */
	struct si_object *object;

	/* Arguments passed to dispatch callback. */
	struct si_arg u[MAX_ARGS + 1];

	/* Objects that are used in async buffer request on this context. */
	struct list_head objects_head;

	int errno;

	/* inbound and outbound buffers. */
	struct {
		struct si_buffer msg;
		phys_addr_t paddr;

		/* TODO. remove after moving to tzmem allocator. */
		struct qtee_shm shm;
	} in, out;
};

int si_object_do_invoke(struct si_object_invoke_ctx *oic,
	struct si_object *object, unsigned long op, struct si_arg u[], int *result);

/* Reserved Operations. */

#define SI_OBJECT_OP_METHOD_MASK	0x0000FFFFU
#define SI_OBJECT_OP_METHOD_ID(op)	((op) & SI_OBJECT_OP_METHOD_MASK)

#define SI_OBJECT_OP_RELEASE		(SI_OBJECT_OP_METHOD_MASK - 0)
#define SI_OBJECT_OP_RETAIN			(SI_OBJECT_OP_METHOD_MASK - 1)
#define SI_OBJECT_OP_NO_OP			(SI_OBJECT_OP_METHOD_MASK - 2)

struct si_object_operations {
	void (*release)(struct si_object *object);

	/**
	 * @op_supported:
	 *
	 * Query made to make sure the requested operation is supported. If defined,
	 * it is called before marshaling of the arguments (as optimisation).
	 */
	int (*op_supported)(unsigned long op);

	/**
	 * @dispatch:
	 *
	 * Object's dispatch function called on object invocation.
	 * Multiple operations can be dispatched concurrently.
	 */
	int (*dispatch)(unsigned int context_id,
		struct si_object *object, unsigned long op, struct si_arg args[]);

	/**
	 * @notify:
	 *
	 * Notify the change in status of the pervious invocation to the driver;
	 * i.e. transport errors or success (status is zero).
	 */
	void (*notify)(unsigned int context_id,	struct si_object *object, int status);

	/**
	 * @prepare:
	 *
	 * Called on object of type AT_IO on direct call (or AT_OO on callback
	 * response) to QTEE. The object provider can return (1) a buffer argument
	 * and (2) an object. @args is { { .type == AT_OB }, { .type == AT_OO },
	 * { .type == AT_END } }. On failour, returns SI_OBJECT_OP_NO_OP, otherwise
	 * an operation that provider has done on @object.
	 */
	unsigned long (*prepare)(struct si_object *object, struct si_arg args[]);
};

struct si_object {
	const char *name;
	struct kref refcount;

	enum si_object_type object_type;
	union object_info {
		unsigned long object_ptr;
	} info;

	struct si_object_operations *ops;

	/* see si_core_async.c. */
	struct list_head node;

	/* see si_core_wq.c. */
	struct work_struct work;

	/* Callback for any internal cleanup before the object's release. */
	void (*release)(struct si_object *object);
};

#define NULL_SI_OBJECT ((struct si_object *)(0))
#define ROOT_SI_OBJECT ((struct si_object *)(1))

static inline enum si_object_type typeof_si_object(struct si_object *object)
{
	if (object == NULL_SI_OBJECT)
		return SI_OT_NULL;

	if (object == ROOT_SI_OBJECT)
		return SI_OT_ROOT;

	return object->object_type;
}

static inline const char *si_object_name(struct si_object *object)
{
	if (object == NULL_SI_OBJECT)
		return "null";

	if (object == ROOT_SI_OBJECT)
		return "root";

	if (!object->name)
		return "noname";

	return object->name;
}

#define INIT_NULL_SI_OBJECT { .object_type = SI_OT_NULL }
#define SI_OBJECT(name, ...) __SI_OBJECT(name, ##__VA_ARGS__, 1)
#define __SI_OBJECT(name, n, ...) struct si_object name[(n)] = { INIT_NULL_SI_OBJECT }

struct si_object *allocate_si_object(void);
void free_si_object(struct si_object *object);

int init_si_object_user(struct si_object *object, enum si_object_type ot,
	struct si_object_operations *ops, const char *fmt, ...);

int get_si_object(struct si_object *object);
void put_si_object(struct si_object *object);
int get_async_proto_version(void);

#endif /* _LINUX_SI_OBJECT_H__ */
