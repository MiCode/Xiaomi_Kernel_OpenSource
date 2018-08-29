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

/* Host-Target Communication API */

#ifndef _HTCA_H_
#define _HTCA_H_

#define DEBUG
#undef DEBUG

/* The HTCA API is independent of the underlying interconnect and
 * independent of the protocols used across that interconnect.
 */

#define HTCA_OK 0        /* Success */
#define HTCA_ERROR 1     /* generic error */
#define HTCA_EINVAL 2    /* Invalid parameter */
#define HTCA_ECANCELED 3 /* Operation canceled */
#define HTCA_EPROTO 4    /* Protocol error */
#define HTCA_ENOMEM 5    /* Memory exhausted */

/* Note: An Endpoint ID is always Interconnect-relative. So we
 * are likely to see the same Endpoint ID with different Targets
 * on a multi-Target system.
 */
#define HTCA_EP_UNUSED (0xff)

#define HTCA_EVENT_UNUSED 0

/* Start global events */
#define HTCA_EVENT_GLOBAL_START 1
#define HTCA_EVENT_TARGET_AVAILABLE 1
#define HTCA_EVENT_TARGET_UNAVAILABLE 2
#define HTCA_EVENT_GLOBAL_END 2
#define HTCA_EVENT_GLOBAL_COUNT                                                \
		(HTCA_EVENT_GLOBAL_END - HTCA_EVENT_GLOBAL_START + 1)
/* End global events */

/* Start endpoint-specific events */
#define HTCA_EVENT_EP_START 3
#define HTCA_EVENT_BUFFER_RECEIVED 3
#define HTCA_EVENT_BUFFER_SENT 4
#define HTCA_EVENT_DATA_AVAILABLE 5
#define HTCA_EVENT_EP_END 5
#define HTCA_EVENT_EP_COUNT (HTCA_EVENT_EP_END - HTCA_EVENT_EP_START + 1)
/* End endpoint-specific events */

/* Maximum size of an HTC header across relevant implementations
 * (e.g. across interconnect types and platforms and OSes of interest).
 *
 * Callers of HTC must leave HTCA_HEADER_LEN_MAX bytes
 * reserved BEFORE the start of a buffer passed to HTCA htca_buffer_send
 * AT the start of a buffer passed to HTCBufferReceive
 * for use by HTC itself.
 *
 * FUTURE: Investigate ways to remove the need for callers to accommodate
 * for HTC headers.* Doesn't seem that hard to do....just tack on the
 * length in a separate buffer and send buffer pairs to HIF. When extracting,
 * first pull header then pull payload into paired buffers.
 */

#define HTCA_HEADER_LEN_MAX 2

struct htca_event_info {
	u8 *buffer;
	void *cookie;
	u32 buffer_length;
	u32 actual_length;
	int status;
};

typedef void (*htca_event_handler)(void *target,
				   u8 ep,
				   u8 event_id,
				   struct htca_event_info *event_info,
				   void *context);

int htca_init(void);

void htca_shutdown(void);

int htca_start(void *target);

void htca_stop(void *target);

int htca_event_reg(void *target,
		   u8 end_point_id,
		   u8 event_id,
		   htca_event_handler event_handler, void *context);

/* Notes:
 * buffer should be multiple of blocksize.
 * buffer should be large enough for header+largest message, rounded up to
 * blocksize.
 * buffer passed in should be start of the buffer -- where header will go.
 * length should be full length, including header.
 * On completion, buffer points to start of payload (AFTER header).
 * On completion, actual_length is the length of payload. Does not include
 * header nor padding. On completion, buffer_length matches the length that
 * was passed in here.
 */
int htca_buffer_receive(void *target,
			u8 end_point_id, u8 *buffer,
			u32 length, void *cookie);

/* Notes:
 * buffer should be multiple of blocksize.
 * buffer passed in should be start of payload; header will be tacked on BEFORE
 * this.
 * extra bytes will be sent, padding the message to blocksize.
 * length should be the number of payload bytes to be sent.
 * The actual number of bytes that go over SDIO is length+header, rounded up to
 * blocksize.
 * On completion, buffer points to start of payload (AFTER header).
 * On completion, actual_length is the length of payload. Does not include
 * header nor padding. On completion buffer_length is irrelevant.
 */
int htca_buffer_send(void *target,
		     u8 end_point_id,
		     u8 *buffer, u32 length, void *cookie);

#endif /* _HTCA_H_ */
