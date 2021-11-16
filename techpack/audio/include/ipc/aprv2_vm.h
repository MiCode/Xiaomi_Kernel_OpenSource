/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, 2019 The Linux Foundation. All rights reserved.
 */
#ifndef __APRV2_VM_H__
#define __APRV2_VM_H__

#define APRV2_VM_MAX_DNS_SIZE (31)
  /* Includes NULL character. */
#define APRV2_VM_PKT_SERVICE_ID_MASK (0x00FF)
  /* Bitmask of the service ID field. */

/* Packet Structure Definition */
struct aprv2_vm_packet_t {
	uint32_t header;
	uint16_t src_addr;
	uint16_t src_port;
	uint16_t dst_addr;
	uint16_t dst_port;
	uint32_t token;
	uint32_t opcode;
};

/**
 * In order to send command/event via MM HAB, the following buffer
 * format shall be followed, where the buffer is provided to the
 * HAB send API.
 * |-----cmd_id or evt_id -----| <- 32 bit, e.g. APRV2_VM_CMDID_REGISTER
 * |-----cmd payload ----------| e.g. aprv2_vm_cmd_register_t
 * | ...                       |
 *
 * In order to receive a command response or event ack, the following
 * buffer format shall be followed, where the buffer is provided to
 * the HAB receive API.
 * |-----cmd response ---------| e.g. aprv2_vm_cmd_register_rsp_t
 * | ...                       |
 */

/* Registers a service with the backend APR driver. */
#define APRV2_VM_CMDID_REGISTER          (0x00000001)

struct aprv2_vm_cmd_register_t {
	uint32_t name_size;
    /**< The service name string size in bytes. */
	char name[APRV2_VM_MAX_DNS_SIZE];
    /**<
     * The service name string to register.
     *
     * A NULL name means the service does not have a name.
     */
	uint16_t addr;
    /**<
     *  The address to register.
     *
     * A zero value means to auto-generate a free dynamic address.
     * A non-zero value means to directly use the statically assigned address.
     */
};

struct aprv2_vm_cmd_register_rsp_t {
	int32_t status;
    /**< The status of registration. */
	uint32_t handle;
    /**< The registered service handle. */
	uint16_t addr;
    /**< The actual registered address. */
};

#define APRV2_VM_CMDID_DEREGISTER        (0x00000002)

struct aprv2_vm_cmd_deregister_t {
	uint32_t handle;
    /**< The registered service handle. */
};

struct aprv2_vm_cmd_deregister_rsp_t {
	int32_t status;
    /**< The status of de-registration. */
};

#define APRV2_VM_CMDID_ASYNC_SEND        (0x00000003)

struct aprv2_vm_cmd_async_send_t {
	uint32_t handle;
    /**< The registered service handle. */
	struct aprv2_vm_packet_t pkt_header;
    /**< The packet header. */
    /* The apr packet payload follows */
};

struct aprv2_vm_cmd_async_send_rsp_t {
	int32_t status;
    /**< The status of send. */
};

#define APRV2_VM_EVT_RX_PKT_AVAILABLE    (0x00000004)

struct aprv2_vm_evt_rx_pkt_available_t {
	struct aprv2_vm_packet_t pkt_header;
    /**< The packet header. */
    /* The apr packet payload follows */
};

struct aprv2_vm_ack_rx_pkt_available_t {
	int32_t status;
};

#endif /* __APRV2_VM_H__ */
