/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2014-2020, Elliptic Laboratories AS. All rights reserved.
 * Elliptic Labs Linux driver
 */

#pragma once

#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>

#define ELLIPTIC_DEBUG_DATA_SIZE 512
#define ELLIPTIC_IPI_AP_TO_SCP_DATA_SIZE 40
#define ELLIPTIC_IPI_SCP_TO_AP_DATA_SIZE 40

struct elliptic_ipi_host_to_scp_message_header {
	uint32_t elliptic_ipi_message_id;
	uint32_t data_size;
};
typedef
	struct elliptic_ipi_host_to_scp_message_header
	elliptic_ipi_host_to_scp_message_header_t;

struct elliptic_scp_to_host_message_header {
	uint32_t parameter_id;
	uint16_t dram_payload_offset;
	uint16_t data_size;
};

typedef
	struct elliptic_scp_to_host_message_header
	elliptic_scp_to_host_message_header_t;

struct elliptic_ipi_host_to_scp_message {
	elliptic_ipi_host_to_scp_message_header_t header;
	uint8_t data[ELLIPTIC_IPI_AP_TO_SCP_DATA_SIZE -
			sizeof(elliptic_ipi_host_to_scp_message_header_t)];
};

typedef
	struct elliptic_ipi_host_to_scp_message
	elliptic_ipi_host_to_scp_message_t;

struct elliptic_dram_payload {
	uint8_t data[ELLIPTIC_DEBUG_DATA_SIZE];
};

typedef
	struct elliptic_dram_payload
	elliptic_dram_payload_t;

struct elliptic_ipi_scp_to_host_message {
	elliptic_scp_to_host_message_header_t header;
	uint8_t data[ELLIPTIC_IPI_SCP_TO_AP_DATA_SIZE -
		sizeof(elliptic_scp_to_host_message_header_t)];
};

typedef
	struct elliptic_ipi_scp_to_host_message
	elliptic_ipi_scp_to_host_message_t;

struct scp_elliptic_reserved_mem_t {
	phys_addr_t phys;
	phys_addr_t virt;
	phys_addr_t size;
	int reserved;
};

struct elliptic_ipi_handler_data_t {
	struct kfifo fifo;
	struct tasklet_struct handle_task;
	uint32_t task_running;
};