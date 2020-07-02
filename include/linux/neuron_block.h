/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/neuron.h>
#include <linux/skbuff.h>
#include <linux/uuid.h>

/* Block I/O request type */
enum neuron_block_req_type {
	NEURON_BLOCK_REQUEST_READ = 0,
	NEURON_BLOCK_REQUEST_WRITE,
	NEURON_BLOCK_REQUEST_DISCARD,
	NEURON_BLOCK_REQUEST_SECURE_ERASE,
	NEURON_BLOCK_REQUEST_WRITE_SAME,
	NEURON_BLOCK_REQUEST_WRITE_ZEROES
};

/* Block I/O request flags bit position */
enum {
	__NEURON_BLOCK_REQ_PREFLUSH = 0,
	__NEURON_BLOCK_REQ_FUA,
	__NEURON_BLOCK_REQ_SYNC
};

/* Block I/O request flags */
#define NEURON_BLOCK_REQ_PREFLUSH (1U << __NEURON_BLOCK_REQ_PREFLUSH)
#define NEURON_BLOCK_REQ_FUA (1U << __NEURON_BLOCK_REQ_FUA)
#define NEURON_BLOCK_REQ_SYNC (1U << __NEURON_BLOCK_REQ_SYNC)

/* Block I/O response status */
enum neuron_block_resp_status {
	BLOCK_RESP_SUCCESS = 0,
	BLOCK_RESP_TIMEOUT,
	BLOCK_RESP_IOERROR,
	BLOCK_RESP_INVAL,
	BLOCK_RESP_ROFS,
	BLOCK_RESP_NOMEM,
	BLOCK_RESP_NODEV,
	BLOCK_RESP_OPNOTSUPP
};

/* Block device params */
struct neuron_block_param {
	u32 logical_block_size;
	u32 physical_block_size;
	u64 num_device_sectors;
	u64 discard_max_hw_sectors;
	u64 discard_max_sectors;
	u32 discard_granularity;
	u16 alignment_offset;
	bool read_only;
	bool discard_zeroes_data;
	bool wc_flag;
	bool fua_flag;
	uuid_t  uuid;
	u8 label[];
};

enum neuron_protocol_block_client_event {
	NEURON_BLOCK_CLIENT_EVENT_REQUEST = 0,
	NEURON_BLOCK_CLIENT_EVENT__COUNT,
};

struct neuron_block_app_client_driver {
	struct neuron_app_driver base;

	/* Called by client protocol driver to read the I/O request.
	 * @param opaque_id An opaque ID to associate with this request
	 * @param skb A pointer to a generated skb, which contains pages shared
	 * with request bio.
	 * @return 0 for success, others for failure
	 */
	int (*get_request)(struct neuron_application *dev,
			   void **opaque_id,
			   enum neuron_block_req_type *req_type,
			   u16 *flags,
			   u64 *start_sector,
			   u32 *sectors,
			   struct sk_buff **skb);

	/* Called by client protocol driver when it receives
	 * block parameter from the server.
	 */
	int (*do_set_bd_params)(struct neuron_application *dev,
				struct neuron_block_param *param);

	/* Called by client protocol driver when it receives i/o
	 * resopnse.
	 * @param opaque_id The one retrieved from read_request.
	 */
	int (*do_response)(struct neuron_application *dev,
			   void *opaque_id,
			   enum neuron_block_resp_status status);
};

enum neuron_protocol_block_server_event {
	NEURON_BLOCK_SERVER_EVENT_BD_PARAMS = 0,
	NEURON_BLOCK_SERVER_EVENT_RESPONSE,
	NEURON_BLOCK_SERVER_EVENT__COUNT,
};

struct neuron_block_app_server_driver {
	struct neuron_app_driver base;
	// Called by server protocol driver to get block params
	int (*get_bd_params)(struct neuron_application *dev,
			     const struct neuron_block_param **param);

	/* Called by server protocol driver to read the I/O response.
	 * @param id An opaque ID to associate with this response
	 * @param status A pointer to status result
	 * @param skb A pointer to a generated skb pointer
	 * @return 0 for success, others for failure
	 */
	int (*get_response)(struct neuron_application *dev, u32 *id,
			    enum neuron_block_resp_status *status,
			    struct sk_buff **skb);

	/* These APIs are called by server protocol for
	 * the received request.
	 */
	int (*do_read)(struct neuron_application *dev, u32 req_id, u64 start,
		       u32 num, u16 flags);
	int (*do_write)(struct neuron_application *dev, u32 req_id, u64 start,
			u32 num, u16 flags, struct sk_buff *skb);
	int (*do_discard)(struct neuron_application *dev, u32 req_id,
			  u64 start, u32 num, u16 flags);
	int (*do_secure_erase)(struct neuron_application *dev, u32 req_id,
			       u64 start, u32 num, u16 flags);
	int (*do_write_same)(struct neuron_application *dev, u32 req_id,
			     u64 start, u32 num, u16 flags,
			     struct sk_buff *skb);
	int (*do_write_zeroes)(struct neuron_application *dev, u32 req_id,
			       u64 start, u32 num, u16 flags);
};

extern struct neuron_protocol_driver protocol_client_block_driver;
extern struct neuron_protocol_driver protocol_server_block_driver;
