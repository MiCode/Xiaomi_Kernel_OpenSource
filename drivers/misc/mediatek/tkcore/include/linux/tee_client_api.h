/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
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

#ifndef TEE_CLIENT_API_H
#define TEE_CLIENT_API_H

#define TEEC_CONFIG_PAYLOAD_REF_COUNT 4

#define TEEC_CONFIG_SHAREDMEM_MAX_SIZE 0x8000

#define TEEC_NONE					0x00000000
#define TEEC_VALUE_INPUT			0x00000001
#define TEEC_VALUE_OUTPUT			0x00000002
#define TEEC_VALUE_INOUT			0x00000003
#define TEEC_MEMREF_TEMP_INPUT		0x00000005
#define TEEC_MEMREF_TEMP_OUTPUT		0x00000006
#define TEEC_MEMREF_TEMP_INOUT		0x00000007
#define TEEC_MEMREF_WHOLE			0x0000000C
#define TEEC_MEMREF_PARTIAL_INPUT	0x0000000D
#define TEEC_MEMREF_PARTIAL_OUTPUT	0x0000000E
#define TEEC_MEMREF_PARTIAL_INOUT	0x0000000F

#define TEEC_MEMREF_PERMANENT		0x00000008

#define TEEC_MEM_INPUT	0x00000001
#define TEEC_MEM_OUTPUT	0x00000002
#define TEEC_MEM_DMABUF	0x00010000
#define TEEC_MEM_KAPI	0x00020000

#define TEEC_MEM_NONSECURE	0x00040000

#define TEEC_SUCCESS				0x00000000
#define TEEC_ERROR_GENERIC			0xFFFF0000
#define TEEC_ERROR_ACCESS_DENIED	0xFFFF0001
#define TEEC_ERROR_CANCEL			0xFFFF0002
#define TEEC_ERROR_ACCESS_CONFLICT	0xFFFF0003
#define TEEC_ERROR_EXCESS_DATA		0xFFFF0004
#define TEEC_ERROR_BAD_FORMAT		0xFFFF0005
#define TEEC_ERROR_BAD_PARAMETERS	0xFFFF0006
#define TEEC_ERROR_BAD_STATE		0xFFFF0007
#define TEEC_ERROR_ITEM_NOT_FOUND	0xFFFF0008
#define TEEC_ERROR_NOT_IMPLEMENTED	0xFFFF0009
#define TEEC_ERROR_NOT_SUPPORTED	0xFFFF000A
#define TEEC_ERROR_NO_DATA			0xFFFF000B
#define TEEC_ERROR_OUT_OF_MEMORY	0xFFFF000C
#define TEEC_ERROR_BUSY				0xFFFF000D
#define TEEC_ERROR_COMMUNICATION	0xFFFF000E
#define TEEC_ERROR_SECURITY			0xFFFF000F
#define TEEC_ERROR_SHORT_BUFFER		0xFFFF0010
#define TEEC_ERROR_TARGET_DEAD		0xFFFF3024

#define TEEC_ORIGIN_API			0x00000001
#define TEEC_ORIGIN_COMMS		0x00000002
#define TEEC_ORIGIN_TEE			0x00000003
#define TEEC_ORIGIN_TRUSTED_APP	0x00000004

#define TEEC_LOGIN_PUBLIC		0x00000000
#define TEEC_LOGIN_USER			0x00000001
#define TEEC_LOGIN_GROUP		0x00000002
#define TEEC_LOGIN_APPLICATION	0x00000004

#define TEEC_PARAM_TYPES(p0, p1, p2, p3) \
	((p0) | ((p1) << 4) | ((p2) << 8) | ((p3) << 12))

#define TEEC_PARAM_TYPE_GET(p, i)\
	(((p) >> (i * 4)) & 0xF)

typedef uint32_t TEEC_Result;

struct TEEC_Context {
	char devname[256];
	union {
		struct tee_context *ctx;
		int fd;
	};
};

struct TEEC_UUID {
	uint32_t timeLow;
	uint16_t timeMid;
	uint16_t timeHiAndVersion;
	uint8_t clockSeqAndNode[8];
};

struct TEEC_SharedMemory {
	union {
		void *buffer;
		uint64_t padding_ptr;
	};
	union {
		size_t size;
		uint64_t padding_sz;
	};
	uint32_t flags;

	uint32_t reserved;
	union {
		int fd;
		void *ptr;
		uint64_t padding_d;
	} d;
	uint64_t registered;
};

struct TEEC_TempMemoryReference {
	union {
		void *buffer;
		uint64_t padding_ptr;
	};
	union {
		size_t size;
		uint64_t padding_sz;
	};
};

struct TEEC_RegisteredMemoryReference {
	union {
		struct TEEC_SharedMemory *parent;
		uint64_t padding_ptr;
	};
	union {
		size_t size;
		uint64_t padding_sz;
	};
	union {
		size_t offset;
		uint64_t padding_off;
	};
};

struct TEEC_Value {
	uint32_t a;
	uint32_t b;
};

union TEEC_Parameter {
	struct TEEC_TempMemoryReference tmpref;
	struct TEEC_RegisteredMemoryReference memref;
	struct TEEC_Value value;
};

struct TEEC_Session {
	int fd;
};

struct TEEC_Operation {
	uint32_t started;
	uint32_t paramTypes;
	union TEEC_Parameter params[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	/* Implementation-Defined */
	union {
		struct TEEC_Session *session;
		uint64_t padding_ptr;
	};
	struct TEEC_SharedMemory memRefs[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	uint64_t flags;
};

#endif
