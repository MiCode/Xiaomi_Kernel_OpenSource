/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * Copyright (C) 2011-2012 NVIDIA Corporation.
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

#include <linux/mman.h>
#include "tf_util.h"

/*----------------------------------------------------------------------------
 * Tegra-specific routines
 *----------------------------------------------------------------------------*/

u32 notrace tegra_read_cycle(void)
{
	u32 cycle_count;

	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycle_count));

	return cycle_count;
}

/*----------------------------------------------------------------------------
 * Debug printing routines
 *----------------------------------------------------------------------------*/
#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT

void tf_trace_array(const char *fun, const char *msg,
		    const void *ptr, size_t len)
{
	char hex[511];
	bool ell = (len > sizeof(hex)/2);
	unsigned lim = (len > sizeof(hex)/2 ? sizeof(hex)/2 : len);
	unsigned i;
	for (i = 0; i < lim; i++)
		sprintf(hex + 2 * i, "%02x", ((unsigned char *)ptr)[i]);
	pr_info("%s: %s[%u] = %s%s\n",
		fun, msg, len, hex, ell ? "..." : "");
}

void address_cache_property(unsigned long va)
{
	unsigned long pa;
	unsigned long inner;
	unsigned long outer;

	asm volatile ("mcr p15, 0, %0, c7, c8, 0" : : "r" (va));
	asm volatile ("mrc p15, 0, %0, c7, c4, 0" : "=r" (pa));

	dprintk(KERN_INFO "VA:%x, PA:%x\n",
		(unsigned int) va,
		(unsigned int) pa);

	if (pa & 1) {
		dprintk(KERN_INFO "Prop Error\n");
		return;
	}

	outer = (pa >> 2) & 3;
	dprintk(KERN_INFO "\touter : %x", (unsigned int) outer);

	switch (outer) {
	case 3:
		dprintk(KERN_INFO "Write-Back, no Write-Allocate\n");
		break;
	case 2:
		dprintk(KERN_INFO "Write-Through, no Write-Allocate.\n");
		break;
	case 1:
		dprintk(KERN_INFO "Write-Back, Write-Allocate.\n");
		break;
	case 0:
		dprintk(KERN_INFO "Non-cacheable.\n");
		break;
	}

	inner = (pa >> 4) & 7;
	dprintk(KERN_INFO "\tinner : %x", (unsigned int)inner);

	switch (inner) {
	case 7:
		dprintk(KERN_INFO "Write-Back, no Write-Allocate\n");
		break;
	case 6:
		dprintk(KERN_INFO "Write-Through.\n");
		break;
	case 5:
		dprintk(KERN_INFO "Write-Back, Write-Allocate.\n");
		break;
	case 3:
		dprintk(KERN_INFO "Device.\n");
		break;
	case 1:
		dprintk(KERN_INFO "Strongly-ordered.\n");
		break;
	case 0:
		dprintk(KERN_INFO "Non-cacheable.\n");
		break;
	}

	if (pa & 0x00000002)
		dprintk(KERN_INFO "SuperSection.\n");
	if (pa & 0x00000080)
		dprintk(KERN_INFO "Memory is shareable.\n");
	else
		dprintk(KERN_INFO "Memory is non-shareable.\n");

	if (pa & 0x00000200)
		dprintk(KERN_INFO "Non-secure.\n");
}

/*
 * Dump the L1 shared buffer.
 */
void tf_dump_l1_shared_buffer(struct tf_l1_shared_buffer *buffer)
{
	dprintk(KERN_INFO
		"buffer@%p:\n"
		#ifndef CONFIG_TF_ZEBRA
		"  config_flag_s=%08X\n"
		#endif
		"  version_description=%64s\n"
		"  status_s=%08X\n"
		"  sync_serial_n=%08X\n"
		"  sync_serial_s=%08X\n"
		"  time_n[0]=%016llX\n"
		"  time_n[1]=%016llX\n"
		"  timeout_s[0]=%016llX\n"
		"  timeout_s[1]=%016llX\n"
		"  first_command=%08X\n"
		"  first_free_command=%08X\n"
		"  first_answer=%08X\n"
		"  first_free_answer=%08X\n\n",
		buffer,
		#ifndef CONFIG_TF_ZEBRA
		buffer->config_flag_s,
		#endif
		buffer->version_description,
		buffer->status_s,
		buffer->sync_serial_n,
		buffer->sync_serial_s,
		buffer->time_n[0],
		buffer->time_n[1],
		buffer->timeout_s[0],
		buffer->timeout_s[1],
		buffer->first_command,
		buffer->first_free_command,
		buffer->first_answer,
		buffer->first_free_answer);
}


/*
 * Dump the specified SChannel message using dprintk.
 */
void tf_dump_command(union tf_command *command)
{
	u32 i;

	dprintk(KERN_INFO "message@%p:\n", command);

	switch (command->header.message_type) {
	case TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   message_size             = 0x%02X\n"
			"   message_type             = 0x%02X "
				"TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT\n"
			"   operation_id             = 0x%08X\n"
			"   device_context_id         = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->header.operation_id,
			command->create_device_context.device_context_id
		);
		break;

	case TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   message_size    = 0x%02X\n"
			"   message_type    = 0x%02X "
				"TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT\n"
			"   operation_id    = 0x%08X\n"
			"   device_context  = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->header.operation_id,
			command->destroy_device_context.device_context);
		break;

	case TF_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   message_size                = 0x%02X\n"
			"   message_type                = 0x%02X "
				"TF_MESSAGE_TYPE_OPEN_CLIENT_SESSION\n"
			"   param_types                 = 0x%04X\n"
			"   operation_id                = 0x%08X\n"
			"   device_context              = 0x%08X\n"
			"   cancellation_id             = 0x%08X\n"
			"   timeout                    = 0x%016llX\n"
			"   destination_uuid            = "
				"%08X-%04X-%04X-%02X%02X-"
				"%02X%02X%02X%02X%02X%02X\n",
			command->header.message_size,
			command->header.message_type,
			command->open_client_session.param_types,
			command->header.operation_id,
			command->open_client_session.device_context,
			command->open_client_session.cancellation_id,
			command->open_client_session.timeout,
			command->open_client_session.destination_uuid.
				time_low,
			command->open_client_session.destination_uuid.
				time_mid,
			command->open_client_session.destination_uuid.
				time_hi_and_version,
			command->open_client_session.destination_uuid.
				clock_seq_and_node[0],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[1],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[2],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[3],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[4],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[5],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[6],
			command->open_client_session.destination_uuid.
				clock_seq_and_node[7]
		);

		for (i = 0; i < 4; i++) {
			uint32_t *param = (uint32_t *) &command->
				open_client_session.params[i];
			dprintk(KERN_INFO "   params[%d] = "
				"0x%08X:0x%08X:0x%08X\n",
				i, param[0], param[1], param[2]);
		}

		switch (TF_LOGIN_GET_MAIN_TYPE(
			command->open_client_session.login_type)) {
		case TF_LOGIN_PUBLIC:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_PUBLIC\n");
			break;
		case TF_LOGIN_USER:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_USER\n");
			break;
		 case TF_LOGIN_GROUP:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_GROUP\n");
			break;
		case TF_LOGIN_APPLICATION:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_APPLICATION\n");
			break;
		case TF_LOGIN_APPLICATION_USER:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_APPLICATION_USER\n");
			break;
		case TF_LOGIN_APPLICATION_GROUP:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_APPLICATION_GROUP\n");
			break;
		case TF_LOGIN_AUTHENTICATION:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_AUTHENTICATION\n");
			break;
		case TF_LOGIN_PRIVILEGED:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_PRIVILEGED\n");
			break;
		case TF_LOGIN_PRIVILEGED_KERNEL:
			dprintk(
				KERN_INFO "   login_type               = "
					"TF_LOGIN_PRIVILEGED_KERNEL\n");
			break;
		default:
			dprintk(
				KERN_ERR "   login_type               = "
					"0x%08X (Unknown login type)\n",
				command->open_client_session.login_type);
			break;
		}

		dprintk(
			KERN_INFO "   login_data               = ");
		for (i = 0; i < 20; i++)
			dprintk(
				KERN_INFO "%d",
				command->open_client_session.
					login_data[i]);
		dprintk("\n");
		break;

	case TF_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   message_size                = 0x%02X\n"
			"   message_type                = 0x%02X "
				"TF_MESSAGE_TYPE_CLOSE_CLIENT_SESSION\n"
			"   operation_id                = 0x%08X\n"
			"   device_context              = 0x%08X\n"
			"   client_session              = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->header.operation_id,
			command->close_client_session.device_context,
			command->close_client_session.client_session
		);
		break;

	case TF_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   message_size             = 0x%02X\n"
			"   message_type             = 0x%02X "
				"TF_MESSAGE_TYPE_REGISTER_SHARED_MEMORY\n"
			"   memory_flags             = 0x%04X\n"
			"   operation_id             = 0x%08X\n"
			"   device_context           = 0x%08X\n"
			"   block_id                 = 0x%08X\n"
			"   shared_mem_size           = 0x%08X\n"
			"   shared_mem_start_offset    = 0x%08X\n"
			"   shared_mem_descriptors[0] = 0x%08X\n"
			"   shared_mem_descriptors[1] = 0x%08X\n"
			"   shared_mem_descriptors[2] = 0x%08X\n"
			"   shared_mem_descriptors[3] = 0x%08X\n"
			"   shared_mem_descriptors[4] = 0x%08X\n"
			"   shared_mem_descriptors[5] = 0x%08X\n"
			"   shared_mem_descriptors[6] = 0x%08X\n"
			"   shared_mem_descriptors[7] = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->register_shared_memory.memory_flags,
			command->header.operation_id,
			command->register_shared_memory.device_context,
			command->register_shared_memory.block_id,
			command->register_shared_memory.shared_mem_size,
			command->register_shared_memory.
				shared_mem_start_offset,
			command->register_shared_memory.
				shared_mem_descriptors[0],
			command->register_shared_memory.
				shared_mem_descriptors[1],
			command->register_shared_memory.
				shared_mem_descriptors[2],
			command->register_shared_memory.
				shared_mem_descriptors[3],
			command->register_shared_memory.
				shared_mem_descriptors[4],
			command->register_shared_memory.
				shared_mem_descriptors[5],
			command->register_shared_memory.
				shared_mem_descriptors[6],
			command->register_shared_memory.
				shared_mem_descriptors[7]);
		break;

	case TF_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   message_size    = 0x%02X\n"
			"   message_type    = 0x%02X "
				"TF_MESSAGE_TYPE_RELEASE_SHARED_MEMORY\n"
			"   operation_id    = 0x%08X\n"
			"   device_context  = 0x%08X\n"
			"   block          = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->header.operation_id,
			command->release_shared_memory.device_context,
			command->release_shared_memory.block);
		break;

	case TF_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
		dprintk(KERN_INFO
			 "   message_size                = 0x%02X\n"
			"   message_type                = 0x%02X "
				"TF_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND\n"
			"   param_types                 = 0x%04X\n"
			"   operation_id                = 0x%08X\n"
			"   device_context              = 0x%08X\n"
			"   client_session              = 0x%08X\n"
			"   timeout                    = 0x%016llX\n"
			"   cancellation_id             = 0x%08X\n"
			"   client_command_identifier    = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->invoke_client_command.param_types,
			command->header.operation_id,
			command->invoke_client_command.device_context,
			command->invoke_client_command.client_session,
			command->invoke_client_command.timeout,
			command->invoke_client_command.cancellation_id,
			command->invoke_client_command.
				client_command_identifier
		);

		for (i = 0; i < 4; i++) {
			uint32_t *param = (uint32_t *) &command->
				open_client_session.params[i];
			dprintk(KERN_INFO "   params[%d] = "
				"0x%08X:0x%08X:0x%08X\n", i,
				param[0], param[1], param[2]);
		}
		break;

	case TF_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
		dprintk(KERN_INFO
			"   message_size       = 0x%02X\n"
			"   message_type       = 0x%02X "
				"TF_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND\n"
			"   operation_id       = 0x%08X\n"
			"   device_context     = 0x%08X\n"
			"   client_session     = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->header.operation_id,
			command->cancel_client_operation.device_context,
			command->cancel_client_operation.client_session);
		break;

	case TF_MESSAGE_TYPE_MANAGEMENT:
		dprintk(KERN_INFO
			"   message_size             = 0x%02X\n"
			"   message_type             = 0x%02X "
				"TF_MESSAGE_TYPE_MANAGEMENT\n"
			"   operation_id             = 0x%08X\n"
			"   command                 = 0x%08X\n"
			"   w3b_size                 = 0x%08X\n"
			"   w3b_start_offset          = 0x%08X\n",
			command->header.message_size,
			command->header.message_type,
			command->header.operation_id,
			command->management.command,
			command->management.w3b_size,
			command->management.w3b_start_offset);
		break;

	default:
		dprintk(
			KERN_ERR "   message_type = 0x%08X "
				"(Unknown message type)\n",
			command->header.message_type);
		break;
	}
}


/*
 * Dump the specified SChannel answer using dprintk.
 */
void tf_dump_answer(union tf_answer *answer)
{
	u32 i;
	dprintk(
		KERN_INFO "answer@%p:\n",
		answer);

	switch (answer->header.message_type) {
	case TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   message_size    = 0x%02X\n"
			"   message_type    = 0x%02X "
				"tf_answer_create_device_context\n"
			"   operation_id    = 0x%08X\n"
			"   error_code      = 0x%08X\n"
			"   device_context  = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->create_device_context.error_code,
			answer->create_device_context.device_context);
		break;

	case TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT:
		dprintk(KERN_INFO
			"   message_size     = 0x%02X\n"
			"   message_type     = 0x%02X "
				"ANSWER_DESTROY_DEVICE_CONTEXT\n"
			"   operation_id     = 0x%08X\n"
			"   error_code       = 0x%08X\n"
			"   device_context_id = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->destroy_device_context.error_code,
			answer->destroy_device_context.device_context_id);
		break;


	case TF_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   message_size      = 0x%02X\n"
			"   message_type      = 0x%02X "
				"tf_answer_open_client_session\n"
			"   error_origin     = 0x%02X\n"
			"   operation_id      = 0x%08X\n"
			"   error_code        = 0x%08X\n"
			"   client_session    = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->open_client_session.error_origin,
			answer->header.operation_id,
			answer->open_client_session.error_code,
			answer->open_client_session.client_session);
		for (i = 0; i < 4; i++) {
			dprintk(KERN_INFO "   answers[%d]=0x%08X:0x%08X\n",
				i,
				answer->open_client_session.answers[i].
					value.a,
				answer->open_client_session.answers[i].
					value.b);
		}
		break;

	case TF_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
		dprintk(KERN_INFO
			"   message_size      = 0x%02X\n"
			"   message_type      = 0x%02X "
				"ANSWER_CLOSE_CLIENT_SESSION\n"
			"   operation_id      = 0x%08X\n"
			"   error_code        = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->close_client_session.error_code);
		break;

	case TF_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   message_size    = 0x%02X\n"
			"   message_type    = 0x%02X "
				"tf_answer_register_shared_memory\n"
			"   operation_id    = 0x%08X\n"
			"   error_code      = 0x%08X\n"
			"   block          = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->register_shared_memory.error_code,
			answer->register_shared_memory.block);
		break;

	case TF_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
		dprintk(KERN_INFO
			"   message_size    = 0x%02X\n"
			"   message_type    = 0x%02X "
				"ANSWER_RELEASE_SHARED_MEMORY\n"
			"   operation_id    = 0x%08X\n"
			"   error_code      = 0x%08X\n"
			"   block_id        = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->release_shared_memory.error_code,
			answer->release_shared_memory.block_id);
		break;

	case TF_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
		dprintk(KERN_INFO
			"   message_size      = 0x%02X\n"
			"   message_type      = 0x%02X "
				"tf_answer_invoke_client_command\n"
			"   error_origin     = 0x%02X\n"
			"   operation_id      = 0x%08X\n"
			"   error_code        = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->invoke_client_command.error_origin,
			answer->header.operation_id,
			answer->invoke_client_command.error_code
			);
		for (i = 0; i < 4; i++) {
			dprintk(KERN_INFO "   answers[%d]=0x%08X:0x%08X\n",
				i,
				answer->invoke_client_command.answers[i].
					value.a,
				answer->invoke_client_command.answers[i].
					value.b);
		}
		break;

	case TF_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
		dprintk(KERN_INFO
			"   message_size      = 0x%02X\n"
			"   message_type      = 0x%02X "
				"TF_ANSWER_CANCEL_CLIENT_COMMAND\n"
			"   operation_id      = 0x%08X\n"
			"   error_code        = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->cancel_client_operation.error_code);
		break;

	case TF_MESSAGE_TYPE_MANAGEMENT:
		dprintk(KERN_INFO
			"   message_size      = 0x%02X\n"
			"   message_type      = 0x%02X "
				"TF_MESSAGE_TYPE_MANAGEMENT\n"
			"   operation_id      = 0x%08X\n"
			"   error_code        = 0x%08X\n",
			answer->header.message_size,
			answer->header.message_type,
			answer->header.operation_id,
			answer->header.error_code);
		break;

	default:
		dprintk(
			KERN_ERR "   message_type = 0x%02X "
				"(Unknown message type)\n",
			answer->header.message_type);
		break;

	}
}

#endif  /* defined(TF_DRIVER_DEBUG_SUPPORT) */

/*----------------------------------------------------------------------------
 * SHA-1 implementation
 * This is taken from the Linux kernel source crypto/sha1.c
 *----------------------------------------------------------------------------*/

struct sha1_ctx {
	u64 count;
	u32 state[5];
	u8 buffer[64];
};

static inline u32 rol(u32 value, u32 bits)
{
	return ((value) << (bits)) | ((value) >> (32 - (bits)));
}

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#define blk0(i) block32[i]

#define blk(i) (block32[i & 15] = rol( \
	block32[(i + 13) & 15] ^ block32[(i + 8) & 15] ^ \
	block32[(i + 2) & 15] ^ block32[i & 15], 1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v, w, x, y, z, i) do { \
	z += ((w & (x ^ y)) ^ y) + blk0(i) + 0x5A827999 + rol(v, 5); \
	w = rol(w, 30); } \
	while (0)

#define R1(v, w, x, y, z, i) do { \
	z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5A827999 + rol(v, 5); \
	w = rol(w, 30); } \
	while (0)

#define R2(v, w, x, y, z, i) do { \
	z += (w ^ x ^ y) + blk(i) + 0x6ED9EBA1 + rol(v, 5); \
	w = rol(w, 30); } \
	while (0)

#define R3(v, w, x, y, z, i) do { \
	z += (((w | x) & y) | (w & x)) + blk(i) + 0x8F1BBCDC + rol(v, 5); \
	w = rol(w, 30); } \
	while (0)

#define R4(v, w, x, y, z, i) do { \
	z += (w ^ x ^ y) + blk(i) + 0xCA62C1D6 + rol(v, 5); \
	w = rol(w, 30); } \
	while (0)


/* Hash a single 512-bit block. This is the core of the algorithm. */
static void sha1_transform(u32 *state, const u8 *in)
{
	u32 a, b, c, d, e;
	u32 block32[16];

	/* convert/copy data to workspace */
	for (a = 0; a < sizeof(block32)/sizeof(u32); a++)
		block32[a] = ((u32) in[4 * a]) << 24 |
			     ((u32) in[4 * a + 1]) << 16 |
			     ((u32) in[4 * a + 2]) <<  8 |
			     ((u32) in[4 * a + 3]);

	/* Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];

	/* 4 rounds of 20 operations each. Loop unrolled. */
	R0(a, b, c, d, e, 0); R0(e, a, b, c, d, 1);
	R0(d, e, a, b, c, 2); R0(c, d, e, a, b, 3);
	R0(b, c, d, e, a, 4); R0(a, b, c, d, e, 5);
	R0(e, a, b, c, d, 6); R0(d, e, a, b, c, 7);
	R0(c, d, e, a, b, 8); R0(b, c, d, e, a, 9);
	R0(a, b, c, d, e, 10); R0(e, a, b, c, d, 11);
	R0(d, e, a, b, c, 12); R0(c, d, e, a, b, 13);
	R0(b, c, d, e, a, 14); R0(a, b, c, d, e, 15);

	R1(e, a, b, c, d, 16); R1(d, e, a, b, c, 17);
	R1(c, d, e, a, b, 18); R1(b, c, d, e, a, 19);

	R2(a, b, c, d, e, 20); R2(e, a, b, c, d, 21);
	R2(d, e, a, b, c, 22); R2(c, d, e, a, b, 23);
	R2(b, c, d, e, a, 24); R2(a, b, c, d, e, 25);
	R2(e, a, b, c, d, 26); R2(d, e, a, b, c, 27);
	R2(c, d, e, a, b, 28); R2(b, c, d, e, a, 29);
	R2(a, b, c, d, e, 30); R2(e, a, b, c, d, 31);
	R2(d, e, a, b, c, 32); R2(c, d, e, a, b, 33);
	R2(b, c, d, e, a, 34); R2(a, b, c, d, e, 35);
	R2(e, a, b, c, d, 36); R2(d, e, a, b, c, 37);
	R2(c, d, e, a, b, 38); R2(b, c, d, e, a, 39);

	R3(a, b, c, d, e, 40); R3(e, a, b, c, d, 41);
	R3(d, e, a, b, c, 42); R3(c, d, e, a, b, 43);
	R3(b, c, d, e, a, 44); R3(a, b, c, d, e, 45);
	R3(e, a, b, c, d, 46); R3(d, e, a, b, c, 47);
	R3(c, d, e, a, b, 48); R3(b, c, d, e, a, 49);
	R3(a, b, c, d, e, 50); R3(e, a, b, c, d, 51);
	R3(d, e, a, b, c, 52); R3(c, d, e, a, b, 53);
	R3(b, c, d, e, a, 54); R3(a, b, c, d, e, 55);
	R3(e, a, b, c, d, 56); R3(d, e, a, b, c, 57);
	R3(c, d, e, a, b, 58); R3(b, c, d, e, a, 59);

	R4(a, b, c, d, e, 60); R4(e, a, b, c, d, 61);
	R4(d, e, a, b, c, 62); R4(c, d, e, a, b, 63);
	R4(b, c, d, e, a, 64); R4(a, b, c, d, e, 65);
	R4(e, a, b, c, d, 66); R4(d, e, a, b, c, 67);
	R4(c, d, e, a, b, 68); R4(b, c, d, e, a, 69);
	R4(a, b, c, d, e, 70); R4(e, a, b, c, d, 71);
	R4(d, e, a, b, c, 72); R4(c, d, e, a, b, 73);
	R4(b, c, d, e, a, 74); R4(a, b, c, d, e, 75);
	R4(e, a, b, c, d, 76); R4(d, e, a, b, c, 77);
	R4(c, d, e, a, b, 78); R4(b, c, d, e, a, 79);

	/* Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/* Wipe variables */
	a = b = c = d = e = 0;
	memset(block32, 0x00, sizeof(block32));
}


static void sha1_init(void *ctx)
{
	struct sha1_ctx *sctx = ctx;
	static const struct sha1_ctx initstate = {
		0,
		{ 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 },
		{ 0, }
	};

	*sctx = initstate;
}


static void sha1_update(void *ctx, const u8 *data, unsigned int len)
{
	struct sha1_ctx *sctx = ctx;
	unsigned int i, j;

	j = (sctx->count >> 3) & 0x3f;
	sctx->count += len << 3;

	if ((j + len) > 63) {
		memcpy(&sctx->buffer[j], data, (i = 64 - j));
		sha1_transform(sctx->state, sctx->buffer);
		for ( ; i + 63 < len; i += 64)
			sha1_transform(sctx->state, &data[i]);
		j = 0;
	} else
		i = 0;
	memcpy(&sctx->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */
static void sha1_final(void *ctx, u8 *out)
{
	struct sha1_ctx *sctx = ctx;
	u32 i, j, index, padlen;
	u64 t;
	u8 bits[8] = { 0, };
	static const u8 padding[64] = { 0x80, };

	t = sctx->count;
	bits[7] = 0xff & t; t >>= 8;
	bits[6] = 0xff & t; t >>= 8;
	bits[5] = 0xff & t; t >>= 8;
	bits[4] = 0xff & t; t >>= 8;
	bits[3] = 0xff & t; t >>= 8;
	bits[2] = 0xff & t; t >>= 8;
	bits[1] = 0xff & t; t >>= 8;
	bits[0] = 0xff & t;

	/* Pad out to 56 mod 64 */
	index = (sctx->count >> 3) & 0x3f;
	padlen = (index < 56) ? (56 - index) : ((64+56) - index);
	sha1_update(sctx, padding, padlen);

	/* Append length */
	sha1_update(sctx, bits, sizeof(bits));

	/* Store state in digest */
	for (i = j = 0; i < 5; i++, j += 4) {
		u32 t2 = sctx->state[i];
		out[j+3] = t2 & 0xff; t2 >>= 8;
		out[j+2] = t2 & 0xff; t2 >>= 8;
		out[j+1] = t2 & 0xff; t2 >>= 8;
		out[j] = t2 & 0xff;
	}

	/* Wipe context */
	memset(sctx, 0, sizeof(*sctx));
}




/*----------------------------------------------------------------------------
 * Process identification
 *----------------------------------------------------------------------------*/

/* This function generates a processes hash table for authentication */
int tf_get_current_process_hash(void *hash)
{
	int result = 0;
	void *buffer;
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	buffer = internal_kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buffer == NULL) {
		dprintk(
			KERN_ERR "tf_get_current_process_hash:"
			" Out of memory for buffer!\n");
		return -ENOMEM;
	}

	mm = current->mm;

	down_read(&(mm->mmap_sem));
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) != 0 && vma->vm_file
				!= NULL) {
			struct dentry *dentry;
			unsigned long start;
			unsigned long cur;
			unsigned long end;
			struct sha1_ctx sha1;

			dentry = dget(vma->vm_file->f_dentry);

			dprintk(
				KERN_DEBUG "tf_get_current_process_hash: "
					"Found executable VMA for inode %lu "
					"(%lu bytes).\n",
					dentry->d_inode->i_ino,
					(unsigned long) (dentry->d_inode->
						i_size));

			start = do_mmap(vma->vm_file, 0,
				dentry->d_inode->i_size,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_PRIVATE, 0);
			if (start < 0) {
				dprintk(
					KERN_ERR "tf_get_current_process_hash"
					"Hash: do_mmap failed (error %d)!\n",
					(int) start);
				dput(dentry);
				result = -EFAULT;
				goto vma_out;
			}

			end = start + dentry->d_inode->i_size;

			sha1_init(&sha1);
			cur = start;
			while (cur < end) {
				unsigned long chunk;

				chunk = end - cur;
				if (chunk > PAGE_SIZE)
					chunk = PAGE_SIZE;
				if (copy_from_user(buffer, (const void *) cur,
						chunk) != 0) {
					dprintk(
						KERN_ERR "tf_get_current_"
						"process_hash: copy_from_user "
						"failed!\n");
					result = -EINVAL;
					(void) do_munmap(mm, start,
						dentry->d_inode->i_size);
					dput(dentry);
					goto vma_out;
				}
				sha1_update(&sha1, buffer, chunk);
				cur += chunk;
			}
			sha1_final(&sha1, hash);
			result = 0;

			(void) do_munmap(mm, start, dentry->d_inode->i_size);
			dput(dentry);
			break;
		}
	}
vma_out:
	up_read(&(mm->mmap_sem));

	internal_kfree(buffer);

	if (result == -ENOENT)
		dprintk(
			KERN_ERR "tf_get_current_process_hash: "
				"No executable VMA found for process!\n");
	return result;
}

#ifndef CONFIG_ANDROID
/* This function hashes the path of the current application.
 * If data = NULL ,nothing else is added to the hash
		else add data to the hash
  */
int tf_hash_application_path_and_data(char *buffer, void *data,
	u32 data_len)
{
	int result = -ENOENT;
	char *tmp = NULL;
	struct mm_struct *mm;
	struct vm_area_struct *vma;

	tmp = internal_kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (tmp == NULL) {
		result = -ENOMEM;
		goto end;
	}

	mm = current->mm;

	down_read(&(mm->mmap_sem));
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if ((vma->vm_flags & VM_EXECUTABLE) != 0
				&& vma->vm_file != NULL) {
			struct path *path;
			char *endpath;
			size_t pathlen;
			struct sha1_ctx sha1;
			u8 hash[SHA1_DIGEST_SIZE];

			path = &vma->vm_file->f_path;

			endpath = d_path(path, tmp, PAGE_SIZE);
			if (IS_ERR(path)) {
				result = PTR_ERR(endpath);
				up_read(&(mm->mmap_sem));
				goto end;
			}
			pathlen = (tmp + PAGE_SIZE) - endpath;

#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT
			{
				char *c;
				dprintk(KERN_DEBUG "current process path = ");
				for (c = endpath;
				     c < tmp + PAGE_SIZE;
				     c++)
					dprintk("%c", *c);

				dprintk(", uid=%d, euid=%d\n", current_uid(),
					current_euid());
			}
#endif /* defined(CONFIG_TF_DRIVER_DEBUG_SUPPORT) */

			sha1_init(&sha1);
			sha1_update(&sha1, endpath, pathlen);
			if (data != NULL) {
				dprintk(KERN_INFO "current process path: "
					"Hashing additional data\n");
				sha1_update(&sha1, data, data_len);
			}
			sha1_final(&sha1, hash);
			memcpy(buffer, hash, sizeof(hash));

			result = 0;

			break;
		}
	}
	up_read(&(mm->mmap_sem));

end:
	if (tmp != NULL)
		internal_kfree(tmp);

	return result;
}
#endif /* !CONFIG_ANDROID */

void *internal_kmalloc(size_t size, int priority)
{
	void *ptr;
	struct tf_device *dev = tf_get_device();

	ptr = kmalloc(size, priority);

	if (ptr != NULL)
		atomic_inc(
			&dev->stats.stat_memories_allocated);

	return ptr;
}

void internal_kfree(void *ptr)
{
	struct tf_device *dev = tf_get_device();

	if (ptr != NULL)
		atomic_dec(
			&dev->stats.stat_memories_allocated);
	return kfree(ptr);
}

void internal_vunmap(void *ptr)
{
	struct tf_device *dev = tf_get_device();

	if (ptr != NULL)
		atomic_dec(
			&dev->stats.stat_memories_allocated);

	vunmap((void *) (((unsigned int)ptr) & 0xFFFFF000));
}

void *internal_vmalloc(size_t size)
{
	void *ptr;
	struct tf_device *dev = tf_get_device();

	ptr = vmalloc(size);

	if (ptr != NULL)
		atomic_inc(
			&dev->stats.stat_memories_allocated);

	return ptr;
}

void internal_vfree(void *ptr)
{
	struct tf_device *dev = tf_get_device();

	if (ptr != NULL)
		atomic_dec(
			&dev->stats.stat_memories_allocated);
	return vfree(ptr);
}

unsigned long internal_get_zeroed_page(int priority)
{
	unsigned long result;
	struct tf_device *dev = tf_get_device();

	result = get_zeroed_page(priority);

	if (result != 0)
		atomic_inc(&dev->stats.
				stat_pages_allocated);

	return result;
}

void internal_free_page(unsigned long addr)
{
	struct tf_device *dev = tf_get_device();

	if (addr != 0)
		atomic_dec(
			&dev->stats.stat_pages_allocated);
	return free_page(addr);
}

int internal_get_user_pages(
		struct task_struct *tsk,
		struct mm_struct *mm,
		unsigned long start,
		int len,
		int write,
		int force,
		struct page **pages,
		struct vm_area_struct **vmas)
{
	int result;
	struct tf_device *dev = tf_get_device();

	result = get_user_pages(
		tsk,
		mm,
		start,
		len,
		write,
		force,
		pages,
		vmas);

	if (result > 0)
		atomic_add(result,
			&dev->stats.stat_pages_locked);

	return result;
}

void internal_get_page(struct page *page)
{
	struct tf_device *dev = tf_get_device();

	atomic_inc(&dev->stats.stat_pages_locked);

	get_page(page);
}

void internal_page_cache_release(struct page *page)
{
	struct tf_device *dev = tf_get_device();

	atomic_dec(&dev->stats.stat_pages_locked);

	page_cache_release(page);
}
