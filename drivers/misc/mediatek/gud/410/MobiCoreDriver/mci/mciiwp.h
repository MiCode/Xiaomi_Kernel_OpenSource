/*
 * Copyright (c) 2016-2017 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MCIIWP_H_
#define MCIIWP_H_

#include "public/GP/tee_client_types.h" /* teec_uuid FIXME it's all mixed up! */

/** Session ID for notifications for the Dragon CA-to-TA communication protocol
 *
 * Session ID are distinct from any valid MCP session identifier
 * and from the existing pseudo-session identifiers :
 * - SID_MCP = 0
 * - SID_INVALID = 0xffffffff
 *
 * A session ID is a thread ID, and since thread IDs have a nonzero task ID as
 * their lowest 16 bits, we can use values of the form 0x????0000
 */
#define SID_OPEN_SESSION        (0x00010000)
#define SID_INVOKE_COMMAND      (0x00020000)
#define SID_CLOSE_SESSION       (0x00030000)
#define SID_CANCEL_OPERATION    (0x00040000)
#define SID_MEMORY_REFERENCE    (0x00050000)
#define SID_OPEN_TA             (0x00060000)
#define SID_REQ_TA              (0x00070000)

/* To quickly detect IWP notifications */
#define SID_IWP_NOTIFICATION \
	(SID_OPEN_SESSION | SID_INVOKE_COMMAND | SID_CLOSE_SESSION | \
	 SID_CANCEL_OPERATION | SID_MEMORY_REFERENCE | SID_OPEN_TA | SID_REQ_TA)

struct interworld_parameter_value {
	u32	a;
	u32	b;
	u8	unused[8];
};

/** The API parameter type TEEC_MEMREF_WHOLE is translated into these types
 * and does not appear in the inter-world protocol.
 *
 * - memref_handle references a previously registered memory reference
 *   'offset' bytes <= memref_handle < 'offset + size' bytes
 *
 * These sizes must be contained within the memory reference.
 */
struct interworld_parameter_memref {
	u32	offset;
	u32	size;
	u32	memref_handle;
	u32	unused;
};

/** This structure is used for the parameter types TEEC_MEMREF_TEMP_xxx.
 *
 * The parameter is located in World Shared Memory which is established
 * for the command and torn down afterwards.
 *
 * The number of pages to share is 'size + offset' divided by the page
 * size, rounded up.
 * Inside the shared pages, the buffer starts at address 'offset'
 * and ends after 'size' bytes.
 *
 * - wsm_type parameter may be WSM_CONTIGUOUS or WSM_L1.
 * - offset must be less than the page size (4096).
 * - size must be less than 0xfffff000.
 */
struct interworld_parameter_tmpref {
	u16	wsm_type;
	u16	offset;
	u32	size;
	u64	physical_address;
};

/**
 *
 */
union interworld_parameter {
	struct interworld_parameter_value	value;
	struct interworld_parameter_memref	memref;
	struct interworld_parameter_tmpref	tmpref;
};

/**
 * An inter-world session structure represents an active session between
 * a normal world client and RTM.
 * It is located in the MCI buffer, must be 8-byte aligned
 *
 * NB : since the session structure is in shared memory, it must have the
 * same layout on both sides (normal world kernel and RTM).
 * All types use platform endianness (specifically, the endianness used by
 * the secure world).
 */
struct interworld_session {
	u32	status;
	u32	return_origin;
	u16	session_handle;
	u16	param_types;

	union {
		u32 command_id;    /** invoke-command only */
		u32 login;         /** open-session only */
	};

	union interworld_parameter params[4];

	/* The following fields are only used during open-session */
	struct teec_uuid target_uuid;
	struct teec_uuid client_uuid;
};

#endif /** MCIIWP_H_ */
