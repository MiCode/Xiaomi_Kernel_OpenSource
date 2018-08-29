/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _HTCA_INTERNAL_H_
#define _HTCA_INTERNAL_H_

#include "mbox_host_reg.h"

#if defined(DEBUG)
#define htcadebug(fmt, a...) \
	pr_err("htca %s:%d: " fmt, __func__, __LINE__, ##a)
#else
#define htcadebug(args...)
#endif

/* HTCA internal specific declarations and prototypes */

/* Target-side SDIO/SPI (mbox) controller supplies 4 mailboxes */
#define HTCA_NUM_MBOX 4

/* Software supports at most this many Target devices */
#define HTCA_NUM_DEVICES_MAX 2

/* Maximum supported mailbox message size.
 *
 * Quartz' SDIO/SPI mailbox alias spaces are 2KB each; so changes
 * would be required to exceed that. WLAN restricts packets to
 * under 1500B.
 */
#define HTCA_MESSAGE_SIZE_MAX 2048

#define HTCA_TARGET_RESPONSE_TIMEOUT 2000 /* in ms */

/* The maximum number of credits that we will reap
 * from the Target at one time.
 */
#define HTCA_TX_CREDITS_REAP_MAX 8

/* Mailbox address in SDIO address space */
#define MBOX_BASE_ADDR 0x800 /* Start of MBOX alias spaces */
#define MBOX_WIDTH 0x800     /* Width of each mailbox alias space */

#define MBOX_START_ADDR(mbox) (MBOX_BASE_ADDR + ((mbox) * (MBOX_WIDTH)))

/* The byte just before this causes an EndOfMessage interrupt to be generated */
#define MBOX_END_ADDR(mbox) (MBOX_START_ADDR(mbox) + MBOX_WIDTH)

/* extended MBOX address for larger MBOX writes to MBOX 0 (not used) */
#define MBOX0_EXTENDED_BASE_ADDR 0x2800
#define MBOX0_EXTENDED_WIDTH (6 * 1024)

/* HTCA message header */
struct HTCA_header {
	u16 total_msg_length;
} __packed;

#define HTCA_HEADER_LEN sizeof(struct HTCA_header)

/* Populate an htca_event_info structure to be passed to
 * a user's event handler.
 */
static inline void htca_frame_event(struct htca_event_info *event_info,
				    u8 *buffer, size_t buffer_length,
				    size_t actual_length, u32 status,
				    void *cookie)
{
	if (event_info) {
		event_info->buffer = buffer;
		event_info->buffer_length = buffer_length;
		event_info->actual_length = actual_length;
		event_info->status = status;
		event_info->cookie = cookie;
	}
}

/* Global and endpoint-specific event tables use these to
 * map an event ID --> handler + param.
 */
struct htca_event_table_element {
	htca_event_handler handler;
	void *param;
};

/* This layout MUST MATCH Target hardware layout! */
struct htca_intr_status {
	u8 host_int_status;
	u8 cpu_int_status;
	u8 err_int_status;
	u8 counter_int_status;
} __packed;

/* This layout MUST MATCH Target hardware layout! */
struct htca_intr_enables {
	u8 int_status_enb;
	u8 cpu_int_status_enb;
	u8 err_status_enb;
	u8 counter_int_status_enb;
} __packed;

/* The Register table contains Target SDIO/SPI interrupt/rxstatus
 * registers used by HTCA. Rather than read particular registers,
 * we use a bulk "register refresh" to read all at once.
 *
 * This layout MUST MATCH Target hardware layout!
 */
struct htca_register_table {
	struct htca_intr_status status;

	u8 mbox_frame;
	u8 rx_lookahead_valid;
	u8 hole[2];

	/* Four lookahead bytes for each mailbox */
	u32 rx_lookahead[HTCA_NUM_MBOX];
} __packed;

/* Two types of requests/responses are supported:
 * "mbox requests" are messages or data which
 * are sent to a Target mailbox
 * "register requests" are to read/write Target registers
 *
 * Mbox requests are managed with a per-endpoint
 * pending list and free list.
 *
 * Register requests are managed with a per-Target
 * pending list and free list.
 *
 * A generic HTCA request -- one which is either an
 * htca_mbox_request or an htca_reg_request is represented
 * by an htca_request.
 */

/* Number of mbox_requests and reg_requests allocated initially.  */
#define HTCA_MBOX_REQUEST_COUNT 16		   /* per mailbox */
#define HTCA_REG_REQUEST_COUNT (4 * HTCA_NUM_MBOX) /* per target */

/* An htca_request is at the start of a mbox_request structure
 * and at the start of a reg_request structure.
 *
 * Specific request types may be cast to a generic htca_request
 * (e.g. in order to invoke the completion callback function)
 */
struct htca_request {
	/*struct htca_request*/ void *next; /* linkage */
	struct htca_target *target;
	void (*completion_cb)(struct htca_request *req, int status);
	int status; /* completion status from HIF */
};

struct htca_endpoint; /* forward reference */

/* Mailbox request -- a message or bulk data */
struct htca_mbox_request {
	struct htca_request req; /* Must be first -- (cast to htca_request) */

	/* Caller-supplied cookie associated with this request */
	void *cookie;

	/* Pointer to the start of the buffer. In the transmit
	 * direction this points to the start of the payload. In the
	 * receive direction, however, the buffer when queued up
	 * points to the start of the HTCA header but when returned
	 * to the caller points to the start of the payload
	 *
	 * Note: buffer is set to NULL whenever this request is free.
	 */
	u8 *buffer;

	/* length, in bytes, of the buffer */
	u32 buffer_length;

	/* length, in bytes, of the payload within the buffer */
	u32 actual_length;

	struct htca_endpoint *end_point;
};

/* Round up a value (e.g. length) to a power of 2 (e.g. block size).  */
static inline u32 htca_round_up(u32 value, u32 pwrof2)
{
	return (((value) + (pwrof2) - 1) & ~((pwrof2) - 1));
}

/* Indicates reasons that we might access Target register space */
enum htca_req_purpose {
	UNUSED_PURPOSE,
	INTR_REFRESH,	/* Fetch latest interrupt/status registers */
	CREDIT_REFRESH, /* Reap credits */
	UPDATE_TARG_INTRS,
	UPDATE_TARG_AND_ENABLE_HOST_INTRS,
};

/* Register read request -- used to read registers from SDIO/SPI space.
 * Register writes are fire and forget; no completion is needed.
 *
 */
struct htca_reg_request {
	struct htca_request req; /* Must be first -- (cast to htca_request) */
	u8 *buffer;	 /* register value(s) */
	u32 length;

	/* Indicates the purpose this request was made */
	enum htca_req_purpose purpose;

	/* Which endpoint this read is for.
	 * Used when processing a completed credit refresh request.
	 */
	u8 epid; /* which endpoint ID [0..3] */

	/* A read to Target register space returns
	 * one specific Target register value OR
	 * all values in the register_table OR
	 * a special repeated read-and-dec from a credit register
	 *
	 * FUTURE: We could separate these into separate request
	 * types in order to perhaps save a bit of space....
	 * eliminate the union.
	 */
	union {
		struct htca_intr_enables enb;
		struct htca_register_table reg_table;
		u8 credit_dec_results[HTCA_TX_CREDITS_REAP_MAX];
	} u;
};

struct htca_request_queue {
	struct htca_request *head;
	struct htca_request *tail;
};

#define HTCA_IS_QUEUE_EMPTY(q) (!((q)->head))

/* List of Target registers in SDIO/SPI space which can be accessed by Host */
enum target_registers {
	UNUSED_REG = 0,
	INTR_ENB_REG = INT_STATUS_ENABLE_ADDRESS,
	ALL_STATUS_REG = HOST_INT_STATUS_ADDRESS,
	ERROR_INT_STATUS_REG = ERROR_INT_STATUS_ADDRESS,
	CPU_INT_STATUS_REG = CPU_INT_STATUS_ADDRESS,
	TX_CREDIT_COUNTER_DECREMENT_REG = COUNT_DEC_ADDRESS,
	INT_TARGET_REG = INT_TARGET_ADDRESS,
};

static inline u32 get_reg_addr(enum target_registers which,
			       u8 epid)
{
	return (((which) == TX_CREDIT_COUNTER_DECREMENT_REG)
	     ? (COUNT_DEC_ADDRESS + (HTCA_NUM_MBOX + (epid)) * 4)
	     : (which));
}

/* FUTURE: See if we can use lock-free operations
 * to manage credits and linked lists.
 * FUTURE: Use standard Linux queue ops; ESPECIALLY
 * if they support lock-free operation.
 */

/* One of these per endpoint */
struct htca_endpoint {
	/* Enabled or Disabled */
	bool enabled;

	/* If data is available, rxLengthPending
	 * indicates the length of the incoming message.
	 */
	u32 rx_frame_length; /* incoming frame length on this endpoint */
	/* includes HTCA header */
	/* Modified only by compl_task */

	bool rx_data_alerted; /* Caller was sent a BUFFER_AVAILABLE event */
	/* and has not supplied a new recv buffer */
	/* since that warning was sent.  */
	/* Modified only by work_task */

	bool tx_credits_to_reap; /* At least one credit available at the */
	/* Target waiting to be reaped. */
	/* Modified only by compl_task */

	/* Guards tx_credits_available and tx_credit_refresh_in_progress */
	spinlock_t tx_credit_lock;

	/* The number of credits that we have already reaped
	 * from the Target. (i.e. we have decremented the Target's
	 * count register so that we have ability to send future
	 * messages). We have the ability to send tx_credits_available
	 * messages without blocking.
	 *
	 * The size of a message is endpoint-dependent and always
	 * a multiple of the device's block_size.
	 */
	u32 tx_credits_available;

	/* Maximum message size */
	u32 max_msg_sz;

	/* Indicates that we are in the midst of a credit refresh cycle */
	bool tx_credit_refresh_in_progress;

	/* Free/Pending Send/Recv queues are used for mbox requests.
	 * An mbox Send request cannot be given to HIF until we have
	 * a tx credit. An mbox Recv request cannot be given to HIF
	 * until we have a pending rx msg.
	 *
	 * The HIF layer maintains its own queue of requests, which
	 * it uses to serialize access to SDIO. Its queue contains
	 * a mixture of sends/recvs and mbox/reg requests. HIF is
	 * "beyond" flow control so once a requets is given to HIF
	 * it is guaranteed to complete (after all previous requests
	 * complete).
	 */

	/* Guards Free/Pending send/recv queues */
	spinlock_t mbox_queue_lock;
	struct htca_request_queue send_free_queue;
	struct htca_request_queue send_pending_queue;
	struct htca_request_queue recv_free_queue;
	struct htca_request_queue recv_pending_queue;

	/* Inverse reference to the target */
	struct htca_target *target;

	/* Block size configured for the endpoint -- common across all endpoints
	 */
	u32 block_size;

	/* Mapping table for per-endpoint events */
	struct htca_event_table_element endpoint_event_tbl[HTCA_EVENT_EP_COUNT];

	/* Location of the endpoint's mailbox space */
	u32 mbox_start_addr;
	u32 mbox_end_addr;
};

#define ENDPOINT_UNUSED 0

/* Target interrupt states. */
enum intr_state_e {
	/* rxdata and txcred interrupts enabled.
	 * Only the DSR context can switch us to
	 * polled state.
	 */
	HTCA_INTERRUPT,

	/* rxdata and txcred interrupts are disabled.
	 * We are polling (via RegisterRefresh).
	 * Only the work_task can switch us to
	 * interrupt state.
	 */
	HTCA_POLL,
};

/* One of these per connected QCA402X device. */
struct htca_target {
	/* Target device is initialized and ready to go?
	 * This has little o do with Host state;
	 * it reflects readiness of the Target.
	 */
	bool ready;

	/* Handle passed to HIF layer for SDIO/SPI Host controller access */
	void *hif_handle; /* hif_device */

	/* Per-endpoint info */
	struct htca_endpoint end_point[HTCA_NUM_MBOX];

	/* Used during startup while the Host waits for the
	 * Target to initialize.
	 */
	wait_queue_head_t target_init_wait;

	/* Free queue for htca_reg_requests.
	 *
	 * We don't need a regPendingQueue because reads/writes to
	 * Target register space are not flow controlled by the Target.
	 * There is no need to wait for credits in order to hand off a
	 * register read/write to HIF.
	 *
	 * The register read/write may end up queued in a HIF queue
	 * behind both register and mbox reads/writes that were
	 * handed to HIF earlier. But they will never be queued
	 * by HTCA.
	 */
	spinlock_t reg_queue_lock;
	struct htca_request_queue reg_free_queue;

	/* comp task synchronization */
	struct mutex task_mutex;

	struct task_struct *work_task;
	struct task_struct *compl_task;

	/* work_task synchronization */
	wait_queue_head_t work_task_wait; /* wait for work to do */
	bool work_task_has_work;	  /* work available? */
	bool work_task_shutdown;	  /* requested stop? */
	struct completion work_task_completion;

	/* compl_task synchronization */
	wait_queue_head_t compl_task_wait; /* wait for work to do */
	bool compl_task_has_work;	   /* work available? */
	bool compl_task_shutdown;	   /* requested stop? */
	struct completion compl_cask_completion;

	/* Queue of completed mailbox and register requests */
	spinlock_t compl_queue_lock;
	struct htca_request_queue compl_queue;

	/* Software's shadow copy of interrupt enables.
	 * Only the work_task changes intr_enable bits,
	 * so no locking necessary.
	 *
	 * Committed to Target HW when
	 * we convert from polling to interrupts or
	 * we are using interrupts and enables have changed
	 */
	struct htca_intr_enables enb;
	struct htca_intr_enables last_committed_enb;

	enum intr_state_e intr_state;
	int need_start_polling;

	/* Set after we read data from a mailbox (to
	 * update lookahead and mailbox status bits).
	 * used only by work_task even though refreshes
	 * may be started in other contexts.
	 */
	int need_register_refresh;

	/* Guards pending_register_refresh and pending_recv_mask */
	spinlock_t pending_op_lock;

	/* Incremented when a RegisterRefresh is started;
	 * Decremented when it completes.
	 */
	int pending_register_refresh;

	/* Non-zero if a recv operation has been started
	 * but not yet completed. 1 bit for each ep.
	 */
	int pending_recv_mask;
};

/* Convert an endpoint POINTER into an endpoint ID [0..3] */
static inline u32 get_endpoint_id(struct htca_endpoint *ep)
{
	return (u32)(ep - ep->target->end_point);
}

void htca_receive_frame(struct htca_endpoint *end_point);

u32 htca_get_frame_length(struct htca_endpoint *end_point);

void htca_send_frame(struct htca_endpoint *end_point);

void htca_send_blk_size(struct htca_endpoint *end_point);

int htca_rw_completion_handler(void *req, int status);

void htca_send_compl(struct htca_request *req, int status);

void htca_recv_compl(struct htca_request *req, int status);

void htca_reg_compl(struct htca_request *req, int status);

int htca_target_inserted_handler(void *context,
				 void *hif_handle);

int htca_target_removed_handler(void *context, void *hif_handle);

int htca_dsr_handler(void *target_ctxt);

void htca_service_cpu_interrupt(struct htca_target *target,
				struct htca_reg_request *req);

void htca_service_error_interrupt(struct htca_target *target,
				  struct htca_reg_request *req);

void htca_service_credit_counter_interrupt(struct htca_target *target,
					   struct htca_reg_request *req);

void htca_enable_credit_counter_interrupt(struct htca_target *target,
					  u8 end_point_id);

void htca_disable_credit_counter_interrupt(struct htca_target *target,
					   u8 end_point_id);

int htca_add_to_mbox_queue(struct htca_mbox_request *queue,
			   u8 *buffer,
			   u32 buffer_length,
			   u32 actual_length, void *cookie);

struct htca_mbox_request *
htca_remove_from_mbox_queue(struct htca_mbox_request *queue);

void htca_mbox_queue_flush(struct htca_endpoint *end_point,
			   struct htca_request_queue *pending_queue,
			   struct htca_request_queue *free_queue,
			   u8 event_id);

int htca_add_to_event_table(struct htca_target *target,
			    u8 end_point_id,
			    u8 event_id,
			    htca_event_handler handler,
			    void *param);

int htca_remove_from_event_table(struct htca_target *target,
				 u8 end_point_id,
				 u8 event_id);

void htca_dispatch_event(struct htca_target *target,
			 u8 end_point_id,
			 u8 event_id,
			 struct htca_event_info *event_info);

struct htca_target *htca_target_instance(int i);

void htca_target_instance_add(struct htca_target *target);

void htca_target_instance_remove(struct htca_target *target);

u8 htca_get_bit_num_set(u32 data);

void htca_register_refresh(struct htca_target *target);

void free_request(struct htca_request *req,
		  struct htca_request_queue *queue);

extern struct htca_target *htca_target_list[HTCA_NUM_DEVICES_MAX];

int htca_work_task_start(struct htca_target *target);
int htca_compl_task_start(struct htca_target *target);
void htca_work_task_stop(struct htca_target *target);
void htca_compl_task_stop(struct htca_target *target);
void htca_work_task_poke(struct htca_target *target);
void htca_compl_task_poke(struct htca_target *target);

void htca_event_table_init(void);
struct htca_event_table_element *
htca_event_id_to_event(struct htca_target *target,
		       u8 end_point_id,
		       u8 event_id);

void htca_request_enq_tail(struct htca_request_queue *queue,
			   struct htca_request *req);
struct htca_request *htca_request_deq_head(struct htca_request_queue *queue);

void htca_register_refresh_start(struct htca_target *target);
void htca_register_refresh_compl(struct htca_target *target,
				 struct htca_reg_request *req);

int htca_credit_refresh_start(struct htca_endpoint *end_point);
void htca_credit_refresh_compl(struct htca_target *target,
			       struct htca_reg_request *req);

void htca_update_intr_enbs(struct htca_target *target,
			   int enable_host_intrs);
void htca_update_intr_enbs_compl(struct htca_target *target,
				 struct htca_reg_request *req);

bool htca_negotiate_config(struct htca_target *target);

int htca_recv_request_to_hif(struct htca_endpoint *end_point,
			     struct htca_mbox_request *mbox_request);
int htca_send_request_to_hif(struct htca_endpoint *endpoint,
			     struct htca_mbox_request *mbox_request);

int htca_manage_pending_sends(struct htca_target *target, int epid);
int htca_manage_pending_recvs(struct htca_target *target, int epid);

void _htca_stop(struct htca_target *target);

#endif /* _HTCA_INTERNAL_H_ */
