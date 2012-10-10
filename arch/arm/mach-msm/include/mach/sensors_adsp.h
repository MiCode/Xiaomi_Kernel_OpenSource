/* Copyright (c) 2012,  The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SENSORS_ADSP_H
#define SENSORS_ADSP_H

#include <linux/types.h>

/** Maximum number of segments that may be mapped from DDR to OCMEM  */
#define SNS_OCMEM_MAX_NUM_SEG_V01 16

/**  Maximum size of the ocmem_vectors structure  */
#define SNS_OCMEM_MAX_VECTORS_SIZE_V01 512

/* Sensor OCMEM message id  */

#define SNS_OCMEM_CANCEL_REQ_V01 0x0000
#define SNS_OCMEM_CANCEL_RESP_V01 0x0000
#define SNS_OCMEM_VERSION_REQ_V01 0x0001
#define SNS_OCMEM_VERSION_RESP_V01 0x0001
#define SNS_OCMEM_PHYS_ADDR_REQ_V01 0x0002
#define SNS_OCMEM_PHYS_ADDR_RESP_V01 0x0002
#define SNS_OCMEM_HAS_CLIENT_IND_V01 0x0002
#define SNS_OCMEM_BW_VOTE_REQ_V01 0x0003
#define SNS_OCMEM_BW_VOTE_RESP_V01 0x0003
#define SNS_OCMEM_BW_VOTE_IND_V01 0x0003

enum {
	SNS_OCMEM_MODULE_KERNEL = 0,
	SNS_OCMEM_MODULE_ADSP
};

/**
 * Defines the types of response messages
 */
enum {
	SNS_OCMEM_MSG_TYPE_REQ = 0,  /* Request */
	SNS_OCMEM_MSG_TYPE_RESP,     /* Response to a request */
	SNS_OCMEM_MSG_TYPE_IND       /* Asynchronous indication */
};

/**
 * The message header. Used in both incoming and outgoing messages
 */
struct sns_ocmem_hdr_s {
	int32_t  msg_id ;	/* Message ID, as defined in the IDL */
	uint16_t msg_size;	/* Size of message, in bytes */
	uint8_t  dst_module;	/* Destination module */
	uint8_t  src_module;	/* Source module */
	uint8_t  msg_type;	/* The message type */
} __packed;

struct sns_ocmem_common_resp_s_v01 {
	/*  This shall be the first element of every response message  */
	uint8_t sns_result_t;
	/**<   0 == SUCCESS; 1 == FAILURE
	A result of FAILURE indicates that that any data contained in the
	response should not be used other than sns_err_t, to determine the
	type of error */
	uint8_t sns_err_t;
	/**<   See sns_ocmem_error_e in ocmem_sensors.h */
};

/* This structure represents a single memory region that must be
mapped from DDR to OCMEM */
struct sns_mem_segment_s_v01 {

	uint64_t start_address; /* Physical start address of segment */
	uint32_t size; /* Size (in bytes) of this segment */
	uint16_t type; /*  1 == Read only; 2 == Read/Write Data */
} __packed;

struct sns_ocmem_phys_addr_resp_msg_v01 {
	struct sns_ocmem_common_resp_s_v01 resp; /* response */
	uint32_t segments_len; /* number of elements in segments */
	/* Segments mapped from DDR to OCMEM */
	struct sns_mem_segment_s_v01 segments[SNS_OCMEM_MAX_NUM_SEG_V01];
	uint8_t segments_valid; /* true if segments is being passed */
} __packed ;

struct sns_ocmem_has_client_ind_msg_v01 {
	uint16_t num_clients; /* Number of active clients on the ADSP */
} __packed;

struct sns_ocmem_bw_vote_req_msg_v01 {
	uint8_t is_map;		/* True if mapping; false if unmapping */
	uint8_t vectors_valid;  /* True if vectors is being passed */
	uint32_t vectors_len;	/* Number of elements in vectors */
	uint8_t vectors[SNS_OCMEM_MAX_VECTORS_SIZE_V01]; /* vectors */
} __packed;

struct sns_ocmem_bw_vote_resp_msg_v01 {
	struct sns_ocmem_common_resp_s_v01 resp;
};

struct sns_ocmem_bw_vote_ind_msg_v01 {
	/* If the ADSP just voted for, or took away its vote for
	OCMEM bandwidth */
	uint8_t is_vote_on;
} __packed;

#endif /* SENSORS_ADSP_H */
