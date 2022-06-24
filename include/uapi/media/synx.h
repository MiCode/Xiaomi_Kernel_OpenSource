/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __UAPI_SYNX_H__
#define __UAPI_SYNX_H__

#include <linux/types.h>
#include <linux/ioctl.h>

/* Size of opaque payload sent to kernel for safekeeping until signal time */
#define SYNX_USER_PAYLOAD_SIZE               4

#define SYNX_STATE_INVALID                   0
#define SYNX_STATE_ACTIVE                    1
#define SYNX_STATE_SIGNALED_SUCCESS          2
#define SYNX_STATE_SIGNALED_ERROR            3
#define SYNX_STATE_SIGNALED_CANCEL           4
#define SYNX_STATE_SIGNALED_EXTERNAL         5

#define SYNX_MAX_WAITING_SYNX                16

#define SYNX_CALLBACK_RESULT_SUCCESS         2
#define SYNX_CALLBACK_RESULT_FAILED          3
#define SYNX_CALLBACK_RESULT_CANCELED        4

/**
 * type of external sync object
 *
 * SYNX_TYPE_CSL  : Object is a CSL sync object
 */
#define SYNX_TYPE_CSL       0
#define SYNX_MAX_BIND_TYPES 1
/**
 * struct synx_info - Sync object creation information
 *
 * @name     : Optional string representation of the synx object
 * @synx_obj : Sync object returned after creation in kernel
 */
struct synx_info {
	char name[64];
	__s32 synx_obj;
};

/**
 * struct synx_userpayload_info - Payload info from user space
 *
 * @synx_obj:   Sync object for which payload has to be registered for
 * @reserved:   Reserved
 * @payload:    Pointer to user payload
 */
struct synx_userpayload_info {
	__s32 synx_obj;
	__u32 reserved;
	__u64 payload[SYNX_USER_PAYLOAD_SIZE];
};

/**
 * struct synx_signal - Sync object signaling struct
 *
 * @synx_obj   : Sync object to be signaled
 * @synx_state : State of the synx object to which it should be signaled
 */
struct synx_signal {
	__s32 synx_obj;
	__u32 synx_state;
};

/**
 * struct synx_merge - Merge information for synx objects
 *
 * @synx_objs :  Pointer to synx object array to merge
 * @num_objs  :  Number of objects in the array
 * @merged    :  Merged synx object
 */
struct synx_merge {
	__u64 synx_objs;
	__u32 num_objs;
	__s32 merged;
};

/**
 * struct synx_wait - Sync object wait information
 *
 * @synx_obj   : Sync object to wait on
 * @reserved   : Reserved
 * @timeout_ms : Timeout in milliseconds
 */
struct synx_wait {
	__s32 synx_obj;
	__u32 reserved;
	__u64 timeout_ms;
};

/**
 * struct synx_external_desc - info of external sync object
 *
 * @type     : Synx type
 * @reserved : Reserved
 * @id       : Sync object id
 *
 */
struct synx_external_desc {
	__u32 type;
	__u32 reserved;
	__s32 id[2];
};

/**
 * struct synx_bind - info for binding two synx objects
 *
 * @synx_obj      : Synx object
 * @Reserved      : Reserved
 * @ext_sync_desc : External synx to bind to
 *
 */
struct synx_bind {
	__s32 synx_obj;
	__u32 reserved;
	struct synx_external_desc ext_sync_desc;
};

/**
 * struct synx_addrefcount - info for refcount increment
 *
 * @synx_obj : Synx object
 * @count    : Count to increment
 *
 */
struct synx_addrefcount {
	__s32 synx_obj;
	__u32 count;
};

/**
 * struct synx_id_info - info for import and export of a synx object
 *
 * @synx_obj     : Synx object to be exported
 * @secure_key   : Secure key created in export and used in import
 * @new_synx_obj : Synx object created in import
 *
 */
struct synx_id_info {
	__s32 synx_obj;
	__u32 secure_key;
	__s32 new_synx_obj;
	__u32 padding;
};

/**
 * struct synx_private_ioctl_arg - Sync driver ioctl argument
 *
 * @id        : IOCTL command id
 * @size      : Size of command payload
 * @result    : Result of command execution
 * @reserved  : Reserved
 * @ioctl_ptr : Pointer to user data
 */
struct synx_private_ioctl_arg {
	__u32 id;
	__u32 size;
	__u32 result;
	__u32 reserved;
	__u64 ioctl_ptr;
};

#define SYNX_PRIVATE_MAGIC_NUM 's'

#define SYNX_PRIVATE_IOCTL_CMD \
	_IOWR(SYNX_PRIVATE_MAGIC_NUM, 130, struct synx_private_ioctl_arg)

#define SYNX_CREATE                          0
#define SYNX_RELEASE                         1
#define SYNX_SIGNAL                          2
#define SYNX_MERGE                           3
#define SYNX_REGISTER_PAYLOAD                4
#define SYNX_DEREGISTER_PAYLOAD              5
#define SYNX_WAIT                            6
#define SYNX_BIND                            7
#define SYNX_ADDREFCOUNT                     8
#define SYNX_GETSTATUS                       9
#define SYNX_IMPORT                          10
#define SYNX_EXPORT                          11

#endif /* __UAPI_SYNX_H__ */
