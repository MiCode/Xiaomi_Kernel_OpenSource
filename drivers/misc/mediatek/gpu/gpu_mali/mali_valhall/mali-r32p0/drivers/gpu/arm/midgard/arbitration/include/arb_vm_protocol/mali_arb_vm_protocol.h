/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT */

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * This is the Arbiter - VM protocol header.
 * It is a single source of implementation of the Arbiter - VM protocol and it
 * should be used by the modules implementing the communication channel between
 * the arbiter and the VM.
 */

#ifndef _MALI_ARB_VM_PROTOCOL_H_
#define _MALI_ARB_VM_PROTOCOL_H_

#include <linux/errno.h>
#include <linux/types.h>

/*
 * Protocol versioning
 */
#define MIN_SUPPORTED_VERSION		((uint8_t)0x1)
#define CURRENT_VERSION			((uint8_t)0x1)

/*
 * Arbiter to VM message IDs
 */
#define ARB_VM_GPU_STOP			((uint8_t)0x01)
#define ARB_VM_GPU_GRANTED		((uint8_t)0x02)
#define ARB_VM_GPU_LOST			((uint8_t)0x03)
#define ARB_VM_INIT			((uint8_t)0x04)

/*
 * VM to Arbiter message IDs
 */
#define VM_ARB_INIT			((uint8_t)0x05)
#define VM_ARB_GPU_IDLE			((uint8_t)0x06)
#define VM_ARB_GPU_ACTIVE		((uint8_t)0x07)
#define VM_ARB_GPU_REQUEST		((uint8_t)0x08)
#define VM_ARB_GPU_STOPPED		((uint8_t)0x09)

/*
 * Messages fields masks
 */
#define MESSAGE_ID_MASK			0xFF
#define ACK_MASK			0x1
#define VERSION_MASK			0x7F
#define REQUEST_MASK			0x1
#define FREQUENCY_MASK			0xFFFFFF
#define PRIORITY_MASK			0x3
#define MAX_L2_SLICES_MASK		0xFF
#define MAX_CORE_MASK_MASK		0xFFFFFFFF

/*
 * Message field offsets
 */
#define FREQUENCY_OFFSET		8
#define VERSION_OFFSET			9
#define MAX_L2_SLICES_OFFSET		24
#define MAX_CORE_MASK_OFFSET		32
#define PRIORITY_OFFSET			8
#define ACK_OFFSET			8
#define REQUEST_OFFSET			16

/**
 * arb_vm_gpu_stop_build_msg() - Builds the ARB_VM_GPU_STOP message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the ARB_VM_GPU_STOP message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int arb_vm_gpu_stop_build_msg(uint64_t *payload)
{
	uint8_t msg_id = ARB_VM_GPU_STOP;

	if (!payload)
		return -EINVAL;

	*payload = msg_id;

	return 0;
}

/**
 * arb_vm_gpu_granted_build_msg() - Builds the ARB_VM_GPU_GRANTED message.
 * @frequency:	The frequency value that is part of the GPU_GRANTED message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the ARB_VM_GPU_GRANTED message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int arb_vm_gpu_granted_build_msg(uint32_t frequency,
					       uint64_t *payload)
{
	uint8_t msg_id = ARB_VM_GPU_GRANTED;
	uint64_t enc_msg;

	if (!payload)
		return -EINVAL;

	enc_msg = msg_id;

	/* frequecy is 24 bits */
	enc_msg |= ((uint64_t)frequency & FREQUENCY_MASK) << FREQUENCY_OFFSET;

	*payload = enc_msg;

	return 0;
}

/**
 * arb_vm_gpu_lost_build_msg() - Builds the ARB_VM_GPU_LOST message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the ARB_VM_GPU_LOST message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int arb_vm_gpu_lost_build_msg(uint64_t *payload)
{
	uint8_t msg_id = ARB_VM_GPU_LOST;

	if (!payload)
		return -EINVAL;

	*payload = msg_id;

	return 0;
}

/**
 * arb_vm_init_build_msg() - Builds the ARB_VM_INIT message.
 * @max_l2_slices:	Max of L2 slices that can be assigned to a partition.
 * @max_core_mask:	Largest core mask that can be assigned to a partition.
 * @payload:		Pointer to return the built message payload.
 * @ack:		True if this in response to receiving a VM_ARB_INIT
 *				message.
 * @version:		Protocol version to use.
 *
 * Builds the ARB_VM_INIT message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int arb_vm_init_build_msg(uint8_t max_l2_slices,
					uint32_t max_core_mask,
					uint8_t ack,
					uint8_t version,
					uint64_t *payload)
{
	uint8_t msg_id = ARB_VM_INIT;
	uint64_t enc_msg;

	if (!payload)
		return -EINVAL;

	enc_msg = msg_id;
	enc_msg |= ((uint64_t)ack & ACK_MASK)
			<< ACK_OFFSET;
	enc_msg |= ((uint64_t)version & VERSION_MASK)
			<< VERSION_OFFSET;
	enc_msg |= ((uint64_t)max_l2_slices & MAX_L2_SLICES_MASK)
			<< MAX_L2_SLICES_OFFSET;
	enc_msg |= ((uint64_t)max_core_mask & MAX_CORE_MASK_MASK)
			<< MAX_CORE_MASK_OFFSET;

	*payload = enc_msg;

	return 0;
}

/**
 * vm_arb_init_build_msg() - Builds the VM_ARB_INIT message.
 * @payload:	Pointer to return the built message payload.
 * @ack:	True if this in response to receiving a ARB_VM_INIT
 *			message.
 * @version:	Protocol version to use.
 * @request:	True if requesting GPU access as a part of the handshake
 *
 * Builds the VM_ARB_INIT message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int vm_arb_init_build_msg(uint8_t ack,
					uint8_t version,
					uint8_t request,
					uint64_t *payload)
{
	uint8_t msg_id = VM_ARB_INIT;
	uint64_t enc_msg;

	if (!payload)
		return -EINVAL;

	enc_msg = msg_id;
	enc_msg |= ((uint64_t)ack & ACK_MASK)
			<< ACK_OFFSET;
	enc_msg |= ((uint64_t)version & VERSION_MASK)
			<< VERSION_OFFSET;
	enc_msg |= ((uint64_t)request & REQUEST_MASK)
			<< REQUEST_OFFSET;

	*payload = enc_msg;

	return 0;
}

/**
 * vm_arb_gpu_idle_build_msg() - Builds the VM_ARB_GPU_IDLE message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the VM_ARB_GPU_IDLE message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int vm_arb_gpu_idle_build_msg(uint64_t *payload)
{
	uint8_t msg_id = VM_ARB_GPU_IDLE;

	if (!payload)
		return -EINVAL;

	*payload = msg_id;

	return 0;
}

/**
 * vm_arb_gpu_active_build_msg() - Builds the VM_ARB_GPU_ACTIVE message.
 * @priority:	The priority value that is part of the GPU_ACTIVE message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the VM_ARB_GPU_ACTIVE message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int vm_arb_gpu_active_build_msg(uint8_t priority,
					      uint64_t *payload)
{
	uint8_t msg_id = VM_ARB_GPU_ACTIVE;
	uint64_t enc_msg;

	if (!payload)
		return -EINVAL;

	enc_msg = msg_id;
	enc_msg |= ((uint64_t)priority & PRIORITY_MASK) << PRIORITY_OFFSET;

	*payload = enc_msg;

	return 0;
}

/**
 * vm_arb_gpu_request_build_msg() - Builds the VM_ARB_GPU_REQUEST message.
 * @priority:	The priority value that is part of the GPU_REQUEST message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the VM_ARB_GPU_REQUEST message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int vm_arb_gpu_request_build_msg(uint8_t priority,
					       uint64_t *payload)
{
	uint8_t msg_id = VM_ARB_GPU_REQUEST;
	uint64_t enc_msg;

	if (!payload)
		return -EINVAL;

	enc_msg = msg_id;
	enc_msg |= ((uint64_t)priority & PRIORITY_MASK) << PRIORITY_OFFSET;

	*payload = enc_msg;

	return 0;
}

/**
 * vm_arb_gpu_stopped_build_msg() - Builds the VM_ARB_GPU_STOPPED message.
 * @priority:	The priority value that is part of the GPU_STOPPED message.
 * @payload:	Pointer to return the built message payload.
 *
 * Builds the VM_ARB_GPU_STOPPED message according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int vm_arb_gpu_stopped_build_msg(uint8_t priority,
					       uint64_t *payload)
{
	uint8_t msg_id = VM_ARB_GPU_STOPPED;
	uint64_t enc_msg;

	if (!payload)
		return -EINVAL;

	enc_msg = msg_id;
	enc_msg |= ((uint64_t)priority & PRIORITY_MASK) << PRIORITY_OFFSET;

	*payload = enc_msg;

	return 0;
}

/**
 * get_msg_id() - Gets the message ID from the payload.
 * @payload:	Message payload.
 * @msg_id:	The pointer to return the message ID from the payload.
 *
 * Gets the message ID from the payload according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_id(uint64_t *payload, uint8_t *msg_id)
{
	if (!payload || !msg_id)
		return -EINVAL;

	*msg_id = (uint8_t)(*payload & MESSAGE_ID_MASK);

	return 0;
}

/**
 * get_msg_priority() - Gets the message priority field from the payload.
 * @payload:	Message payload.
 * @priority:	The pointer to return the priority from the payload message.
 *
 * Gets the priority field from the payload according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_priority(uint64_t *payload, uint8_t *priority)
{
	if (!payload || !priority)
		return -EINVAL;

	*priority = (uint8_t)((*payload >> PRIORITY_OFFSET) & PRIORITY_MASK);

	return 0;
}

/**
 * get_msg_frequency() - Gets the message frequency field from the payload.
 * @payload:	Message payload.
 * @frequency:	The pointer to return the frequency from the payload message.
 *
 * Gets the frequency field from the payload according to the definitions of the
 * VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_frequency(uint64_t *payload, uint32_t *frequency)
{
	if (!payload || !frequency)
		return -EINVAL;

	*frequency = (uint32_t)
		((*payload >> FREQUENCY_OFFSET) & FREQUENCY_MASK);

	return 0;
}

/**
 * get_msg_max_config() - Gets the message max config fields from the payload.
 * @payload:		Message payload.
 * @max_l2_slices:	Pointer to return the maximum number of L2 slices.
 * @max_core_mask:	Pointer to return the largest core mask.
 *
 * Gets the max config fields from the payload according to the definitions of
 * the VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_max_config(uint64_t *payload, uint8_t *max_l2_slices,
				     uint32_t *max_core_mask)
{
	if (!payload || !max_l2_slices || !max_core_mask)
		return -EINVAL;

	*max_l2_slices = (uint8_t)((*payload >> MAX_L2_SLICES_OFFSET)
			& MAX_L2_SLICES_MASK);
	*max_core_mask = (uint32_t)((*payload >> MAX_CORE_MASK_OFFSET)
			& MAX_CORE_MASK_MASK);

	return 0;
}

/**
 * get_msg_protocol_version() - Gets the protocol version field from the
 *                              payload.
 * @payload: Message payload.
 * @version: Pointer to return the message's protocol version.
 *
 * Gets the protocol version field from the payload according to the definitions
 * of the VM-Arbiter protocol. The result is undefined if the message is not a
 * ARB_VM_INIT or VM_ARB_INIT.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_protocol_version(uint64_t *payload, uint8_t *version)
{
	if (!payload || !version)
		return -EINVAL;

	*version = (uint8_t)((*payload >> VERSION_OFFSET) & VERSION_MASK);
	return 0;
}

/**
 * get_msg_init_ack() - Gets the ACK field from the payload.
 * @payload: Message payload.
 * @ack:     Pointer to return the message's ACK state.
 *
 * Gets the ACK field from the payload according to the definitions of the
 * VM-Arbiter protocol. The result is undefined if the message is not a
 * ARB_VM_INIT or VM_ARB_INIT.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_init_ack(uint64_t *payload, uint8_t *ack)
{
	if (!payload || !ack)
		return -EINVAL;

	*ack = (uint8_t)((*payload >> ACK_OFFSET) & ACK_MASK);
	return 0;
}

/**
 * get_msg_init_request() - Gets the request field from the payload.
 * @payload: Message payload.
 * @request: Pointer to return the message's request state.
 *
 * Gets the request field from the payload according to the definitions of the
 * VM-Arbiter protocol. The result is undefined if the message is not a
 * VM_ARB_INIT.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int get_msg_init_request(uint64_t *payload, uint8_t *request)
{
	if (!payload || !request)
		return -EINVAL;

	*request = (uint8_t)((*payload >> REQUEST_OFFSET) & REQUEST_MASK);
	return 0;
}

#endif /* _MALI_ARB_VM_PROTOCOL_ */
