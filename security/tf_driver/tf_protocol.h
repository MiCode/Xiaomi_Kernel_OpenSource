/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __TF_PROTOCOL_H__
#define __TF_PROTOCOL_H__

/*----------------------------------------------------------------------------
 *
 * This header file defines the structure used in the SChannel Protocol.
 * See your Product Reference Manual for a specification of the SChannel
 * protocol.
 *---------------------------------------------------------------------------*/

/*
 * The driver interface version returned by the version ioctl
 */
#define TF_DRIVER_INTERFACE_VERSION     0x04000000

/*
 * Protocol version handling
 */
#define TF_S_PROTOCOL_MAJOR_VERSION  (0x06)
#define GET_PROTOCOL_MAJOR_VERSION(a) (a >> 24)
#define GET_PROTOCOL_MINOR_VERSION(a) ((a >> 16) & 0xFF)

/*
 * The S flag of the config_flag_s register.
 */
#define TF_CONFIG_FLAG_S   (1 << 3)

/*
 * The TimeSlot field of the sync_serial_n register.
 */
#define TF_SYNC_SERIAL_TIMESLOT_N   (1)

/*
 * status_s related defines.
 */
#define TF_STATUS_P_MASK            (0X00000001)
#define TF_STATUS_POWER_STATE_SHIFT (3)
#define TF_STATUS_POWER_STATE_MASK  (0x1F << TF_STATUS_POWER_STATE_SHIFT)

/*
 * Possible power states of the POWER_STATE field of the status_s register
 */
#define TF_POWER_MODE_COLD_BOOT          (0)
#define TF_POWER_MODE_WARM_BOOT          (1)
#define TF_POWER_MODE_ACTIVE             (3)
#define TF_POWER_MODE_READY_TO_SHUTDOWN  (5)
#define TF_POWER_MODE_READY_TO_HIBERNATE (7)
#define TF_POWER_MODE_WAKEUP             (8)
#define TF_POWER_MODE_PANIC              (15)

/*
 * Possible command values for MANAGEMENT commands
 */
#define TF_MANAGEMENT_HIBERNATE            (1)
#define TF_MANAGEMENT_SHUTDOWN             (2)
#define TF_MANAGEMENT_PREPARE_FOR_CORE_OFF (3)
#define TF_MANAGEMENT_RESUME_FROM_CORE_OFF (4)

/*
 * The capacity of the Normal Word message queue, in number of slots.
 */
#define TF_N_MESSAGE_QUEUE_CAPACITY  (512)

/*
 * The capacity of the Secure World message answer queue, in number of slots.
 */
#define TF_S_ANSWER_QUEUE_CAPACITY  (256)

/*
 * The value of the S-timeout register indicating an infinite timeout.
 */
#define TF_S_TIMEOUT_0_INFINITE  (0xFFFFFFFF)
#define TF_S_TIMEOUT_1_INFINITE  (0xFFFFFFFF)

/*
 * The value of the S-timeout register indicating an immediate timeout.
 */
#define TF_S_TIMEOUT_0_IMMEDIATE  (0x0)
#define TF_S_TIMEOUT_1_IMMEDIATE  (0x0)

/*
 * Identifies the get protocol version SMC.
 */
#define TF_SMC_GET_PROTOCOL_VERSION (0XFFFFFFFB)

/*
 * Identifies the init SMC.
 */
#define TF_SMC_INIT                 (0XFFFFFFFF)

/*
 * Identifies the reset irq SMC.
 */
#define TF_SMC_RESET_IRQ            (0xFFFFFFFE)

/*
 * Identifies the SET_W3B SMC.
 */
#define TF_SMC_WAKE_UP              (0xFFFFFFFD)

/*
 * Identifies the STOP SMC.
 */
#define TF_SMC_STOP                 (0xFFFFFFFC)

/*
 * Identifies the n-yield SMC.
 */
#define TF_SMC_N_YIELD              (0X00000003)


/* Possible stop commands for SMC_STOP */
#define SCSTOP_HIBERNATE           (0xFFFFFFE1)
#define SCSTOP_SHUTDOWN            (0xFFFFFFE2)

/*
 * representation of an UUID.
 */
struct tf_uuid {
	u32 time_low;
	u16 time_mid;
	u16 time_hi_and_version;
	u8 clock_seq_and_node[8];
};


/**
 * Command parameters.
 */
struct tf_command_param_value {
	u32    a;
	u32    b;
};

struct tf_command_param_temp_memref {
	u32    descriptor; /* data pointer for exchange message.*/
	u32    size;
	u32    offset;
};

struct tf_command_param_memref {
	u32      block;
	u32      size;
	u32      offset;
};

union tf_command_param {
	struct tf_command_param_value        value;
	struct tf_command_param_temp_memref  temp_memref;
	struct tf_command_param_memref       memref;
};

/**
 * Answer parameters.
 */
struct tf_answer_param_value {
	u32   a;
	u32   b;
};

struct tf_answer_param_size {
	u32   _ignored;
	u32   size;
};

union tf_answer_param {
	struct tf_answer_param_size    size;
	struct tf_answer_param_value   value;
};

/*
 * Descriptor tables capacity
 */
#define TF_MAX_W3B_COARSE_PAGES                 (2)
/* TF_MAX_COARSE_PAGES is the number of level 1 descriptors (describing
 * 1MB each) that can be shared with the secure world in a single registered
 * shared memory block. It must be kept in synch with
 * SCHANNEL6_MAX_DESCRIPTORS_PER_REGISTERED_SHARED_MEM in the SChannel
 * protocol spec. */
#define TF_MAX_COARSE_PAGES                     128
#define TF_DESCRIPTOR_TABLE_CAPACITY_BIT_SHIFT  (8)
#define TF_DESCRIPTOR_TABLE_CAPACITY \
	(1 << TF_DESCRIPTOR_TABLE_CAPACITY_BIT_SHIFT)
#define TF_DESCRIPTOR_TABLE_CAPACITY_MASK \
	(TF_DESCRIPTOR_TABLE_CAPACITY - 1)
/* Shared memories coarse pages can map up to 1MB */
#define TF_MAX_COARSE_PAGE_MAPPED_SIZE \
	(PAGE_SIZE * TF_DESCRIPTOR_TABLE_CAPACITY)
/* Shared memories cannot exceed 8MB */
#define TF_MAX_SHMEM_SIZE \
	(TF_MAX_COARSE_PAGE_MAPPED_SIZE << 3)

/*
 * Buffer size for version description fields
 */
#define TF_DESCRIPTION_BUFFER_LENGTH 64

/*
 * Shared memory type flags.
 */
#define TF_SHMEM_TYPE_READ         (0x00000001)
#define TF_SHMEM_TYPE_WRITE        (0x00000002)

/*
 * Shared mem flags
 */
#define TF_SHARED_MEM_FLAG_INPUT   1
#define TF_SHARED_MEM_FLAG_OUTPUT  2
#define TF_SHARED_MEM_FLAG_INOUT   3


/*
 * Parameter types
 */
#define TF_PARAM_TYPE_NONE               0x0
#define TF_PARAM_TYPE_VALUE_INPUT        0x1
#define TF_PARAM_TYPE_VALUE_OUTPUT       0x2
#define TF_PARAM_TYPE_VALUE_INOUT        0x3
#define TF_PARAM_TYPE_MEMREF_TEMP_INPUT  0x5
#define TF_PARAM_TYPE_MEMREF_TEMP_OUTPUT 0x6
#define TF_PARAM_TYPE_MEMREF_TEMP_INOUT  0x7
#define TF_PARAM_TYPE_MEMREF_ION_HANDLE  0xB
#define TF_PARAM_TYPE_MEMREF_INPUT       0xD
#define TF_PARAM_TYPE_MEMREF_OUTPUT      0xE
#define TF_PARAM_TYPE_MEMREF_INOUT       0xF

#define TF_PARAM_TYPE_MEMREF_FLAG               0x4
#define TF_PARAM_TYPE_REGISTERED_MEMREF_FLAG    0x8


#define TF_MAKE_PARAM_TYPES(t0, t1, t2, t3) \
	((t0) | ((t1) << 4) | ((t2) << 8) | ((t3) << 12))
#define TF_GET_PARAM_TYPE(t, i) (((t) >> (4 * i)) & 0xF)

/*
 * Login types.
 */
#define TF_LOGIN_PUBLIC              0x00000000
#define TF_LOGIN_USER                0x00000001
#define TF_LOGIN_GROUP               0x00000002
#define TF_LOGIN_APPLICATION         0x00000004
#define TF_LOGIN_APPLICATION_USER    0x00000005
#define TF_LOGIN_APPLICATION_GROUP   0x00000006
#define TF_LOGIN_AUTHENTICATION      0x80000000
#define TF_LOGIN_PRIVILEGED          0x80000002

/* Login variants */

#define TF_LOGIN_VARIANT(main_type, os, variant) \
	((main_type) | (1 << 27) | ((os) << 16) | ((variant) << 8))

#define TF_LOGIN_GET_MAIN_TYPE(type) \
	((type) & ~TF_LOGIN_VARIANT(0, 0xFF, 0xFF))

#define TF_LOGIN_OS_ANY       0x00
#define TF_LOGIN_OS_LINUX     0x01
#define TF_LOGIN_OS_ANDROID   0x04

/* OS-independent variants */
#define TF_LOGIN_USER_NONE \
	TF_LOGIN_VARIANT(TF_LOGIN_USER, TF_LOGIN_OS_ANY, 0xFF)
#define TF_LOGIN_GROUP_NONE \
	TF_LOGIN_VARIANT(TF_LOGIN_GROUP, TF_LOGIN_OS_ANY, 0xFF)
#define TF_LOGIN_APPLICATION_USER_NONE \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION_USER, TF_LOGIN_OS_ANY, 0xFF)
#define TF_LOGIN_AUTHENTICATION_BINARY_SHA1_HASH \
	TF_LOGIN_VARIANT(TF_LOGIN_AUTHENTICATION, TF_LOGIN_OS_ANY, 0x01)
#define TF_LOGIN_PRIVILEGED_KERNEL \
	TF_LOGIN_VARIANT(TF_LOGIN_PRIVILEGED, TF_LOGIN_OS_ANY, 0x01)

/* Linux variants */
#define TF_LOGIN_USER_LINUX_EUID \
	TF_LOGIN_VARIANT(TF_LOGIN_USER, TF_LOGIN_OS_LINUX, 0x01)
#define TF_LOGIN_GROUP_LINUX_GID \
	TF_LOGIN_VARIANT(TF_LOGIN_GROUP, TF_LOGIN_OS_LINUX, 0x01)
#define TF_LOGIN_APPLICATION_LINUX_PATH_SHA1_HASH \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION, TF_LOGIN_OS_LINUX, 0x01)
#define TF_LOGIN_APPLICATION_USER_LINUX_PATH_EUID_SHA1_HASH \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION_USER, TF_LOGIN_OS_LINUX, 0x01)
#define TF_LOGIN_APPLICATION_GROUP_LINUX_PATH_GID_SHA1_HASH \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION_GROUP, TF_LOGIN_OS_LINUX, 0x01)

/* Android variants */
#define TF_LOGIN_USER_ANDROID_EUID \
	TF_LOGIN_VARIANT(TF_LOGIN_USER, TF_LOGIN_OS_ANDROID, 0x01)
#define TF_LOGIN_GROUP_ANDROID_GID \
	TF_LOGIN_VARIANT(TF_LOGIN_GROUP, TF_LOGIN_OS_ANDROID, 0x01)
#define TF_LOGIN_APPLICATION_ANDROID_UID \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION, TF_LOGIN_OS_ANDROID, 0x01)
#define TF_LOGIN_APPLICATION_USER_ANDROID_UID_EUID \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION_USER, TF_LOGIN_OS_ANDROID, \
		0x01)
#define TF_LOGIN_APPLICATION_GROUP_ANDROID_UID_GID \
	TF_LOGIN_VARIANT(TF_LOGIN_APPLICATION_GROUP, TF_LOGIN_OS_ANDROID, \
		0x01)

/*
 *  return origins
 */
#define TF_ORIGIN_COMMS       2
#define TF_ORIGIN_TEE         3
#define TF_ORIGIN_TRUSTED_APP 4
/*
 * The message types.
 */
#define TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT   0x02
#define TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT  0xFD
#define TF_MESSAGE_TYPE_REGISTER_SHARED_MEMORY  0xF7
#define TF_MESSAGE_TYPE_RELEASE_SHARED_MEMORY   0xF9
#define TF_MESSAGE_TYPE_OPEN_CLIENT_SESSION     0xF0
#define TF_MESSAGE_TYPE_CLOSE_CLIENT_SESSION    0xF2
#define TF_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND   0xF5
#define TF_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND   0xF4
#define TF_MESSAGE_TYPE_MANAGEMENT              0xFE


/*
 * The SChannel error codes.
 */
#define S_SUCCESS 0x00000000
#define S_ERROR_OUT_OF_MEMORY 0xFFFF000C


struct tf_command_header {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info;
	u32                      operation_id;
};

struct tf_answer_header {
	u8                   message_size;
	u8                   message_type;
	u16                  message_info;
	u32                  operation_id;
	u32                  error_code;
};

/*
 * CREATE_DEVICE_CONTEXT command message.
 */
struct tf_command_create_device_context {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	u32                      operation_id;
	u32                      device_context_id;
};

/*
 * CREATE_DEVICE_CONTEXT answer message.
 */
struct tf_answer_create_device_context {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      error_code;
	/* an opaque Normal World identifier for the device context */
	u32                      device_context;
};

/*
 * DESTROY_DEVICE_CONTEXT command message.
 */
struct tf_command_destroy_device_context {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	u32                      operation_id;
	u32                      device_context;
};

/*
 * DESTROY_DEVICE_CONTEXT answer message.
 */
struct tf_answer_destroy_device_context {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      error_code;
	u32                      device_context_id;
};

/*
 * OPEN_CLIENT_SESSION command message.
 */
struct tf_command_open_client_session {
	u8                            message_size;
	u8                            message_type;
	u16                           param_types;
	/* an opaque Normal World identifier for the operation */
	u32                           operation_id;
	u32                           device_context;
	u32                           cancellation_id;
	u64                           timeout;
	struct tf_uuid                destination_uuid;
	union tf_command_param        params[4];
	u32                           login_type;
	/*
	 * Size = 0 for public, [16] for group identification, [20] for
	 * authentication
	 */
	u8                            login_data[20];
};

/*
 * OPEN_CLIENT_SESSION answer message.
 */
struct tf_answer_open_client_session {
	u8                       message_size;
	u8                       message_type;
	u8                       error_origin;
	u8                       __reserved;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      error_code;
	u32                      client_session;
	union tf_answer_param    answers[4];
};

/*
 * CLOSE_CLIENT_SESSION command message.
 */
struct tf_command_close_client_session {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      device_context;
	u32                      client_session;
};

/*
 * CLOSE_CLIENT_SESSION answer message.
 */
struct tf_answer_close_client_session {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      error_code;
};


/*
 * REGISTER_SHARED_MEMORY command message
 */
struct tf_command_register_shared_memory {
	u8  message_size;
	u8  message_type;
	u16 memory_flags;
	u32 operation_id;
	u32 device_context;
	u32 block_id;
	u32 shared_mem_size;
	u32 shared_mem_start_offset;
	u32 shared_mem_descriptors[TF_MAX_COARSE_PAGES];
};

/*
 * REGISTER_SHARED_MEMORY answer message.
 */
struct tf_answer_register_shared_memory {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      error_code;
	u32                      block;
};

/*
 * RELEASE_SHARED_MEMORY command message.
 */
struct tf_command_release_shared_memory {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                      operation_id;
	u32                      device_context;
	u32                      block;
};

/*
 * RELEASE_SHARED_MEMORY answer message.
 */
struct tf_answer_release_shared_memory {
	u8                       message_size;
	u8                       message_type;
	u16                      message_info_rfu;
	u32                      operation_id;
	u32                      error_code;
	u32                      block_id;
};

/*
 * INVOKE_CLIENT_COMMAND command message.
 */
struct tf_command_invoke_client_command {
	u8                       message_size;
	u8                       message_type;
	u16                      param_types;
	u32                      operation_id;
	u32                      device_context;
	u32                      client_session;
	u64                      timeout;
	u32                      cancellation_id;
	u32                      client_command_identifier;
	union tf_command_param   params[4];
};

/*
 * INVOKE_CLIENT_COMMAND command answer.
 */
struct tf_answer_invoke_client_command {
	u8                     message_size;
	u8                     message_type;
	u8                     error_origin;
	u8                     __reserved;
	u32                    operation_id;
	u32                    error_code;
	union tf_answer_param  answers[4];
};

/*
 * CANCEL_CLIENT_OPERATION command message.
 */
struct tf_command_cancel_client_operation {
	u8                   message_size;
	u8                   message_type;
	u16                  message_info_rfu;
	/* an opaque Normal World identifier for the operation */
	u32                  operation_id;
	u32                  device_context;
	u32                  client_session;
	u32                  cancellation_id;
};

struct tf_answer_cancel_client_operation {
	u8                   message_size;
	u8                   message_type;
	u16                  message_info_rfu;
	u32                  operation_id;
	u32                  error_code;
};

/*
 * MANAGEMENT command message.
 */
struct tf_command_management {
	u8                   message_size;
	u8                   message_type;
	u16                  command;
	u32                  operation_id;
	u32                  w3b_size;
	u32                  w3b_start_offset;
	u32                  shared_mem_descriptors[1];
};

/*
 * POWER_MANAGEMENT answer message.
 * The message does not provide message specific parameters.
 * Therefore no need to define a specific answer structure
 */

/*
 * Structure for L2 messages
 */
union tf_command {
	struct tf_command_header                  header;
	struct tf_command_create_device_context   create_device_context;
	struct tf_command_destroy_device_context  destroy_device_context;
	struct tf_command_open_client_session     open_client_session;
	struct tf_command_close_client_session    close_client_session;
	struct tf_command_register_shared_memory  register_shared_memory;
	struct tf_command_release_shared_memory   release_shared_memory;
	struct tf_command_invoke_client_command   invoke_client_command;
	struct tf_command_cancel_client_operation cancel_client_operation;
	struct tf_command_management              management;
};

/*
 * Structure for any L2 answer
 */

union tf_answer {
	struct tf_answer_header                  header;
	struct tf_answer_create_device_context   create_device_context;
	struct tf_answer_open_client_session     open_client_session;
	struct tf_answer_close_client_session    close_client_session;
	struct tf_answer_register_shared_memory  register_shared_memory;
	struct tf_answer_release_shared_memory   release_shared_memory;
	struct tf_answer_invoke_client_command   invoke_client_command;
	struct tf_answer_destroy_device_context  destroy_device_context;
	struct tf_answer_cancel_client_operation cancel_client_operation;
};

/* Structure of the Communication Buffer */
struct tf_l1_shared_buffer {
	#ifdef CONFIG_TF_ZEBRA
	u32 exit_code;
	u32 l1_shared_buffer_descr;
	u32 backing_store_addr;
	u32 backext_storage_addr;
	u32 workspace_addr;
	u32 workspace_size;
	u32 conf_descriptor;
	u32 conf_size;
	u32 conf_offset;
	u32 protocol_version;
	u32 rpc_command;
	u32 rpc_status;
	u8  reserved1[16];
	#else
	u32 config_flag_s;
	u32 w3b_size_max_s;
	u32 reserved0;
	u32 w3b_size_current_s;
	u8  reserved1[48];
	#endif
	u8  version_description[TF_DESCRIPTION_BUFFER_LENGTH];
	u32 status_s;
	u32 reserved2;
	u32 sync_serial_n;
	u32 sync_serial_s;
	u64 time_n[2];
	u64 timeout_s[2];
	u32 first_command;
	u32 first_free_command;
	u32 first_answer;
	u32 first_free_answer;
	u32 w3b_descriptors[128];
	#ifdef CONFIG_TF_ZEBRA
	u8  rpc_trace_buffer[140];
	u8  rpc_cus_buffer[180];
	#elif defined(CONFIG_SECURE_TRACES)
	u32 traces_status;
	u8  traces_buffer[140];
	u8  reserved3[176];
	#else
	u8  reserved3[320];
	#endif
	u32 command_queue[TF_N_MESSAGE_QUEUE_CAPACITY];
	u32 answer_queue[TF_S_ANSWER_QUEUE_CAPACITY];
};


/*
 * tf_version_information_buffer structure description
 * Description of the sVersionBuffer handed over from user space to kernel space
 * This field is filled by the driver during a CREATE_DEVICE_CONTEXT ioctl
 * and handed back to user space
 */
struct tf_version_information_buffer {
	u8 driver_description[65];
	u8 secure_world_description[65];
};


/* The IOCTLs the driver supports */
#include <linux/ioctl.h>

#define IOCTL_TF_GET_VERSION     _IO('z', 0)
#define IOCTL_TF_EXCHANGE        _IOWR('z', 1, union tf_command)
#define IOCTL_TF_GET_DESCRIPTION _IOR('z', 2, \
	struct tf_version_information_buffer)
#ifdef CONFIG_TF_ION
#define IOCTL_TF_ION_REGISTER    _IOR('z', 254, int)
#define IOCTL_TF_ION_UNREGISTER  _IOR('z', 255, int)
#endif

#endif  /* !defined(__TF_PROTOCOL_H__) */
