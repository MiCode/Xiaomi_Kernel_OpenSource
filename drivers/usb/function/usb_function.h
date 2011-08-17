/* drivers/usb/function/usb_function.h
 *
 * USB Function Device Interface
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DRIVERS_USB_FUNCTION_USB_FUNCTION_H_
#define _DRIVERS_USB_FUNCTION_USB_FUNCTION_H_

#include <linux/list.h>
#include <linux/usb/ch9.h>

#define EPT_BULK_IN   1
#define EPT_BULK_OUT  2
#define EPT_INT_IN  3

#define USB_CONFIG_ATT_SELFPOWER_POS	(6)	/* self powered */
#define USB_CONFIG_ATT_WAKEUP_POS	(5)	/* can wakeup */

struct usb_endpoint {
	struct usb_info *ui;
	struct msm_request *req; /* head of pending requests */
	struct msm_request *last;
	unsigned flags;

	/* bit number (0-31) in various status registers
	** as well as the index into the usb_info's array
	** of all endpoints
	*/
	unsigned char bit;
	unsigned char num;

	unsigned short max_pkt;

	unsigned ept_halted;

	/* pointers to DMA transfer list area */
	/* these are allocated from the usb_info dma space */
	struct ept_queue_head *head;
	struct usb_endpoint_descriptor *ep_descriptor;
	unsigned int alloced;
};

struct usb_request {
	void *buf;          /* pointer to associated data buffer */
	unsigned length;    /* requested transfer length */
	int status;         /* status upon completion */
	unsigned actual;    /* actual bytes transferred */

	void (*complete)(struct usb_endpoint *ep, struct usb_request *req);
	void *context;

	void *device;

	struct list_head list;
};

struct usb_function {
	/* bind() is called once when the function has had its endpoints
	** allocated, but before the bus is active.
	**
	** might be a good place to allocate some usb_request objects
	*/
	void (*bind)(void *);

	/* unbind() is called when the function is being removed.
	** it is illegal to call and usb_ept_* hooks at this point
	** and all endpoints must be released.
	*/
	void (*unbind)(void *);

	/* configure() is called when the usb client has been configured
	** by the host and again when the device is unconfigured (or
	** when the client is detached)
	**
	** currently called from interrupt context.
	*/
	void (*configure)(int configured, void *);
	void (*disconnect)(void *);

	/* setup() is called to allow functions to handle class and vendor
	** setup requests.  If the request is unsupported or can not be handled,
	** setup() should return -1.
	** For OUT requests, buf will point to a buffer to data received in the
	** request's data phase, and len will contain the length of the data.
	** setup() should return 0 after handling an OUT request successfully.
	** for IN requests, buf will contain a pointer to a buffer for setup()
	** to write data to, and len will be the maximum size of the data to
	** be written back to the host.
	** After successfully handling an IN request, setup() should return
	** the number of bytes written to buf that should be sent in the
	** response to the host.
	*/
	int (*setup)(struct usb_ctrlrequest *req, void *buf,
			int len, void *);

	int (*set_interface)(int ifc_num, int alt_set, void *_ctxt);
	int (*get_interface)(int ifc_num, void *ctxt);
	/* driver name */
	const char *name;
	void *context;

	/* interface class/subclass/protocol for descriptor */
	unsigned char ifc_class;
	unsigned char ifc_subclass;
	unsigned char ifc_protocol;

	/* name string for descriptor */
	const char *ifc_name;

	/* number of needed endpoints and their types */
	unsigned char ifc_ept_count;
	unsigned char ifc_ept_type[8];

	/* if the endpoint is disabled, its interface will not be
	** included in the configuration descriptor
	*/
	unsigned char   disabled;

	struct usb_descriptor_header **fs_descriptors;
	struct usb_descriptor_header **hs_descriptors;

	struct usb_request *ep0_out_req, *ep0_in_req;
	struct usb_endpoint *ep0_out, *ep0_in;
};

int usb_function_register(struct usb_function *driver);
int usb_function_unregister(struct usb_function *driver);

int usb_msm_get_speed(void);
void usb_configure_endpoint(struct usb_endpoint *ep,
			struct usb_endpoint_descriptor *ep_desc);
int usb_remote_wakeup(void);
/* To allocate endpoint from function driver*/
struct usb_endpoint *usb_alloc_endpoint(unsigned direction);
int usb_free_endpoint(struct usb_endpoint *ept);
/* To enable endpoint from frunction driver*/
void usb_ept_enable(struct usb_endpoint *ept, int yes);
int usb_msm_get_next_ifc_number(struct usb_function *);
int usb_msm_get_next_strdesc_id(char *);
void usb_msm_enable_iad(void);

void usb_function_enable(const char *function, int enable);

/* Allocate a USB request.
** Must be called from a context that can sleep.
** If bufsize is nonzero, req->buf will be allocated for
** you and free'd when the request is free'd.  Otherwise
** it is your responsibility to provide.
*/
struct usb_request *usb_ept_alloc_req(struct usb_endpoint *ept, unsigned bufsize);
void usb_ept_free_req(struct usb_endpoint *ept, struct usb_request *req);

/* safely callable from any context
** returns 0 if successfully queued and sets req->status = -EBUSY
** req->status will change to a different value upon completion
** (0 for success, -EIO, -ENODEV, etc for error)
*/
int usb_ept_queue_xfer(struct usb_endpoint *ept, struct usb_request *req);
int usb_ept_flush(struct usb_endpoint *ept);
int usb_ept_get_max_packet(struct usb_endpoint *ept);
int usb_ept_cancel_xfer(struct usb_endpoint *ept, struct usb_request *_req);
void usb_ept_fifo_flush(struct usb_endpoint *ept);
int usb_ept_set_halt(struct usb_endpoint *ept);
int usb_ept_clear_halt(struct usb_endpoint *ept);
struct device *usb_get_device(void);
struct usb_endpoint *usb_ept_find(struct usb_endpoint **ept, int type);
struct usb_function *usb_ept_get_function(struct usb_endpoint *ept);
int usb_ept_is_stalled(struct usb_endpoint *ept);
void usb_request_set_buffer(struct usb_request *req, void *buf, dma_addr_t dma);
void usb_free_endpoint_all_req(struct usb_endpoint *ep);
void usb_remove_function_driver(struct usb_function *func);
int usb_remote_wakeup(void);
#endif
