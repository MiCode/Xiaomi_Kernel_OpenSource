/** include/asm-arm/arch-msm/msm_rpcrouter.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM__ARCH_MSM_RPCROUTER_H
#define __ASM__ARCH_MSM_RPCROUTER_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/platform_device.h>

/* RPC API version structure
 * Version bit 31 : 1->hashkey versioning,
 *                  0->major-minor (backward compatible) versioning
 * hashkey versioning:
 *   Version bits 31-0 hashkey
 * major-minor (backward compatible) versioning
 *   Version bits 30-28 reserved (no match)
 *   Version bits 27-16 major (must match)
 *   Version bits 15-0  minor (greater or equal)
 */
#define RPC_VERSION_MODE_MASK  0x80000000
#define RPC_VERSION_MAJOR_MASK 0x0fff0000
#define RPC_VERSION_MINOR_MASK 0x0000ffff

/* callback ID for NULL callback function is -1 */
#define MSM_RPC_CLIENT_NULL_CB_ID 0xffffffff

struct msm_rpc_endpoint;

struct rpcsvr_platform_device
{
	struct platform_device base;
	uint32_t prog;
	uint32_t vers;
};

#define RPC_DATA_IN	0
/*
 * Structures for sending / receiving direct RPC requests
 * XXX: Any cred/verif lengths > 0 not supported
 */

struct rpc_request_hdr
{
	uint32_t xid;
	uint32_t type;	/* 0 */
	uint32_t rpc_vers; /* 2 */
	uint32_t prog;
	uint32_t vers;
	uint32_t procedure;
	uint32_t cred_flavor;
	uint32_t cred_length;
	uint32_t verf_flavor;
	uint32_t verf_length;
};

typedef struct
{
	uint32_t low;
	uint32_t high;
} rpc_reply_progmismatch_data;

typedef struct
{
} rpc_denied_reply_hdr;

typedef struct
{
	uint32_t verf_flavor;
	uint32_t verf_length;
	uint32_t accept_stat;
#define RPC_ACCEPTSTAT_SUCCESS 0
#define RPC_ACCEPTSTAT_PROG_UNAVAIL 1
#define RPC_ACCEPTSTAT_PROG_MISMATCH 2
#define RPC_ACCEPTSTAT_PROC_UNAVAIL 3
#define RPC_ACCEPTSTAT_GARBAGE_ARGS 4
#define RPC_ACCEPTSTAT_SYSTEM_ERR 5
#define RPC_ACCEPTSTAT_PROG_LOCKED 6
	/*
	 * Following data is dependant on accept_stat
	 * If ACCEPTSTAT == PROG_MISMATCH then there is a
	 * 'rpc_reply_progmismatch_data' structure following the header.
	 * Otherwise the data is procedure specific
	 */
} rpc_accepted_reply_hdr;

struct rpc_reply_hdr
{
	uint32_t xid;
	uint32_t type;
	uint32_t reply_stat;
#define RPCMSG_REPLYSTAT_ACCEPTED 0
#define RPCMSG_REPLYSTAT_DENIED 1
	union {
		rpc_accepted_reply_hdr acc_hdr;
		rpc_denied_reply_hdr dny_hdr;
	} data;
};

struct rpc_board_dev {
	uint32_t prog;
	struct platform_device pdev;
};

/* flags for msm_rpc_connect() */
#define MSM_RPC_UNINTERRUPTIBLE 0x0001

/* use IS_ERR() to check for failure */
struct msm_rpc_endpoint *msm_rpc_open(void);
/* Connect with the specified server version */
struct msm_rpc_endpoint *msm_rpc_connect(uint32_t prog, uint32_t vers, unsigned flags);
/* Connect with a compatible server version */
struct msm_rpc_endpoint *msm_rpc_connect_compatible(uint32_t prog,
	uint32_t vers, unsigned flags);
/* check if server version can handle client requested version */
int msm_rpc_is_compatible_version(uint32_t server_version,
				  uint32_t client_version);

int msm_rpc_close(struct msm_rpc_endpoint *ept);
int msm_rpc_write(struct msm_rpc_endpoint *ept,
		  void *data, int len);
int msm_rpc_read(struct msm_rpc_endpoint *ept,
		 void **data, unsigned len, long timeout);
void msm_rpc_read_wakeup(struct msm_rpc_endpoint *ept);
void msm_rpc_setup_req(struct rpc_request_hdr *hdr,
		       uint32_t prog, uint32_t vers, uint32_t proc);
int msm_rpc_register_server(struct msm_rpc_endpoint *ept,
			    uint32_t prog, uint32_t vers);
int msm_rpc_unregister_server(struct msm_rpc_endpoint *ept,
			      uint32_t prog, uint32_t vers);

int msm_rpc_add_board_dev(struct rpc_board_dev *board_dev, int num);

int msm_rpc_clear_netreset(struct msm_rpc_endpoint *ept);

int msm_rpc_get_curr_pkt_size(struct msm_rpc_endpoint *ept);
/* simple blocking rpc call
 *
 * request is mandatory and must have a rpc_request_hdr
 * at the start.  The header will be filled out for you.
 *
 * reply provides a buffer for replies of reply_max_size
 */
int msm_rpc_call_reply(struct msm_rpc_endpoint *ept, uint32_t proc,
		       void *request, int request_size,
		       void *reply, int reply_max_size,
		       long timeout);
int msm_rpc_call(struct msm_rpc_endpoint *ept, uint32_t proc,
		 void *request, int request_size,
		 long timeout);

struct msm_rpc_xdr {
	void *in_buf;
	uint32_t in_size;
	uint32_t in_index;
	wait_queue_head_t in_buf_wait_q;

	void *out_buf;
	uint32_t out_size;
	uint32_t out_index;
	struct mutex out_lock;

	struct msm_rpc_endpoint *ept;
};

int xdr_send_int8(struct msm_rpc_xdr *xdr, const int8_t *value);
int xdr_send_uint8(struct msm_rpc_xdr *xdr, const uint8_t *value);
int xdr_send_int16(struct msm_rpc_xdr *xdr, const int16_t *value);
int xdr_send_uint16(struct msm_rpc_xdr *xdr, const uint16_t *value);
int xdr_send_int32(struct msm_rpc_xdr *xdr, const int32_t *value);
int xdr_send_uint32(struct msm_rpc_xdr *xdr, const uint32_t *value);
int xdr_send_bytes(struct msm_rpc_xdr *xdr, const void **data, uint32_t *size);

int xdr_recv_int8(struct msm_rpc_xdr *xdr, int8_t *value);
int xdr_recv_uint8(struct msm_rpc_xdr *xdr, uint8_t *value);
int xdr_recv_int16(struct msm_rpc_xdr *xdr, int16_t *value);
int xdr_recv_uint16(struct msm_rpc_xdr *xdr, uint16_t *value);
int xdr_recv_int32(struct msm_rpc_xdr *xdr, int32_t *value);
int xdr_recv_uint32(struct msm_rpc_xdr *xdr, uint32_t *value);
int xdr_recv_bytes(struct msm_rpc_xdr *xdr, void **data, uint32_t *size);

struct msm_rpc_server
{
	struct list_head list;
	uint32_t flags;

	uint32_t prog;
	uint32_t vers;

	struct mutex cb_req_lock;

	struct msm_rpc_endpoint *cb_ept;

	struct msm_rpc_xdr cb_xdr;

	uint32_t version;

	int (*rpc_call)(struct msm_rpc_server *server,
			struct rpc_request_hdr *req, unsigned len);

	int (*rpc_call2)(struct msm_rpc_server *server,
			 struct rpc_request_hdr *req,
			 struct msm_rpc_xdr *xdr);
};

int msm_rpc_create_server(struct msm_rpc_server *server);
int msm_rpc_create_server2(struct msm_rpc_server *server);

#define MSM_RPC_MSGSIZE_MAX 8192

struct msm_rpc_client;

struct msm_rpc_client {
	struct task_struct *read_thread;
	struct task_struct *cb_thread;

	struct msm_rpc_endpoint *ept;
	wait_queue_head_t reply_wait;

	uint32_t prog, ver;

	void *buf;

	struct msm_rpc_xdr xdr;
	struct msm_rpc_xdr cb_xdr;

	uint32_t version;

	int (*cb_func)(struct msm_rpc_client *, void *, int);
	int (*cb_func2)(struct msm_rpc_client *, struct rpc_request_hdr *req,
			struct msm_rpc_xdr *);
	void *cb_buf;
	int cb_size;

	struct list_head cb_item_list;
	struct mutex cb_item_list_lock;

	wait_queue_head_t cb_wait;
	int cb_avail;

	atomic_t next_cb_id;
	spinlock_t cb_list_lock;
	struct list_head cb_list;

	uint32_t exit_flag;
	struct completion complete;
	struct completion cb_complete;

	struct mutex req_lock;

	void (*cb_restart_teardown)(struct msm_rpc_client *client);
	void (*cb_restart_setup)(struct msm_rpc_client *client);
	int in_reset;
};

struct msm_rpc_client_info {
	uint32_t pid;
	uint32_t cid;
	uint32_t prog;
	uint32_t vers;
};


int msm_rpc_client_in_reset(struct msm_rpc_client *client);

struct msm_rpc_client *msm_rpc_register_client(
	const char *name,
	uint32_t prog, uint32_t ver,
	uint32_t create_cb_thread,
	int (*cb_func)(struct msm_rpc_client *, void *, int));

struct msm_rpc_client *msm_rpc_register_client2(
	const char *name,
	uint32_t prog, uint32_t ver,
	uint32_t create_cb_thread,
	int (*cb_func)(struct msm_rpc_client *, struct rpc_request_hdr *req,
		       struct msm_rpc_xdr *xdr));

int msm_rpc_unregister_client(struct msm_rpc_client *client);

int msm_rpc_client_req(struct msm_rpc_client *client, uint32_t proc,
		       int (*arg_func)(struct msm_rpc_client *,
				       void *, void *), void *arg_data,
		       int (*result_func)(struct msm_rpc_client *,
					  void *, void *), void *result_data,
		       long timeout);

int msm_rpc_client_req2(struct msm_rpc_client *client, uint32_t proc,
			int (*arg_func)(struct msm_rpc_client *,
					struct msm_rpc_xdr *, void *),
			void *arg_data,
			int (*result_func)(struct msm_rpc_client *,
					   struct msm_rpc_xdr *, void *),
			void *result_data,
			long timeout);

int msm_rpc_register_reset_callbacks(
	struct msm_rpc_client *client,
	void (*teardown)(struct msm_rpc_client *client),
	void (*setup)(struct msm_rpc_client *client)
	);

void *msm_rpc_start_accepted_reply(struct msm_rpc_client *client,
				   uint32_t xid, uint32_t accept_status);

int msm_rpc_send_accepted_reply(struct msm_rpc_client *client, uint32_t size);

void *msm_rpc_server_start_accepted_reply(struct msm_rpc_server *server,
					  uint32_t xid, uint32_t accept_status);

int msm_rpc_server_send_accepted_reply(struct msm_rpc_server *server,
				       uint32_t size);

int msm_rpc_add_cb_func(struct msm_rpc_client *client, void *cb_func);

void *msm_rpc_get_cb_func(struct msm_rpc_client *client, uint32_t cb_id);

void msm_rpc_remove_cb_func(struct msm_rpc_client *client, void *cb_func);

int msm_rpc_server_cb_req(struct msm_rpc_server *server,
			  struct msm_rpc_client_info *clnt_info,
			  uint32_t cb_proc,
			  int (*arg_func)(struct msm_rpc_server *server,
					  void *buf, void *data),
			  void *arg_data,
			  int (*ret_func)(struct msm_rpc_server *server,
					  void *buf, void *data),
			  void *ret_data, long timeout);

int msm_rpc_server_cb_req2(struct msm_rpc_server *server,
			   struct msm_rpc_client_info *clnt_info,
			   uint32_t cb_proc,
			   int (*arg_func)(struct msm_rpc_server *server,
					   struct msm_rpc_xdr *xdr, void *data),
			   void *arg_data,
			   int (*ret_func)(struct msm_rpc_server *server,
					   struct msm_rpc_xdr *xdr, void *data),
			   void *ret_data, long timeout);

void msm_rpc_server_get_requesting_client(
	struct msm_rpc_client_info *clnt_info);

int xdr_send_pointer(struct msm_rpc_xdr *xdr, void **obj,
		     uint32_t obj_size, void *xdr_op);

int xdr_recv_pointer(struct msm_rpc_xdr *xdr, void **obj,
		     uint32_t obj_size, void *xdr_op);

int xdr_send_array(struct msm_rpc_xdr *xdr, void **addr, uint32_t *size,
		   uint32_t maxsize, uint32_t elm_size, void *xdr_op);

int xdr_recv_array(struct msm_rpc_xdr *xdr, void **addr, uint32_t *size,
		   uint32_t maxsize, uint32_t elm_size, void *xdr_op);

int xdr_recv_req(struct msm_rpc_xdr *xdr, struct rpc_request_hdr *req);
int xdr_recv_reply(struct msm_rpc_xdr *xdr, struct rpc_reply_hdr *reply);
int xdr_start_request(struct msm_rpc_xdr *xdr, uint32_t prog,
		      uint32_t ver, uint32_t proc);
int xdr_start_accepted_reply(struct msm_rpc_xdr *xdr, uint32_t accept_status);
int xdr_send_msg(struct msm_rpc_xdr *xdr);

#endif
