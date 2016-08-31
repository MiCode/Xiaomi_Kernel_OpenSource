/*
 * Copyright (C) 2012-2013 NVIDIA Corporation.
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

#ifndef _NVSHM_IF_H
#define _NVSHM_IF_H

/* Error type */
enum nvshm_error_id {
	NVSHM_NO_ERROR = 0,
	NVSHM_RESTART,
	NVSHM_IOBUF_ERROR,
	NVSHM_UNKNOWN_ERROR
};

/*
 * Interface operations - upper layer interface
 *
 * important note on nvshm_iobuf structure:
 * ALL pointer inside structure are in BB memory space
 * Read/write on these pointers must be done via macro (NVSHM_A2B/NVSHM_B2A)
 * NULL test/assignment should be done without macro
 * nvshm_iobuf pointers in parameters/return are in
 * ipc cached area in kernel space
 * see nvshm-iobuf.h for macro reference
 */
struct nvshm_if_operations {
	/**
	 * rx_event
	 *
	 * This is called by the NVSHM core when an event
	 * and/or a packet of data is received.
	 * receiver should consume all iobuf in given list.
	 * Note that packet could be fragmented with ->sg_next
	 * and multiple packet can be linked via ->next
	 *
	 * @param struct nvshm_channel channel handle
	 * @param struct nvshm_iobuf holding received data
	 */
	void (*rx_event)(struct nvshm_channel *handle, struct nvshm_iobuf *iob);

	/**
	 * error_event
	 *
	 * This is called by the NVSHM core when an error event is
	 * received
	 *
	 * @param struct nvshm_channel channel handle
	 * @param error type of error see enum nvshm_error_id
	 */
	void (*error_event)(struct nvshm_channel *handle,
			    enum nvshm_error_id error);

	/**
	 * start_tx
	 *
	 * This is called by the NVSHM core to restart Tx
	 * after flow control off
	 *
	 * @param struct nvshm_channel channel handle
	 */
	void (*start_tx)(struct nvshm_channel *handle);
};

/**
 * nvshm_open_channel
 *
 * This is used to register a new interface on a specified channel
 *
 * @param int channel to open
 * @param ops interface operations
 * @param void * interface data pointer (private)
 * @return struct nvshm_channel channel handle
 */
struct nvshm_channel *nvshm_open_channel(int chan,
					 struct nvshm_if_operations *ops,
					 void *interface_data);

/**
 * nvshm_close_channel
 *
 * This is used to unregister an interface on specified channel
 *
 * @param struct nvshm_channel channel handle
 *
 */
void nvshm_close_channel(struct nvshm_channel *handle);

/**
 * write an iobuf chain to an NVSHM channel
 *
 * Note that packet could be fragmented with ->sg_next
 * and multiple packet can be linked via ->next
 * Passed iobuf must have a ref count of 1 only or write will fail
 *
 * @param struct nvshm_channel handle
 * @param struct nvshm_iobuf holding packet to write
 *
 * @return 0 if write is ok, 1 if flow control is XOFF, negative for error
 */
int nvshm_write(struct nvshm_channel *handle, struct nvshm_iobuf *iob);

/**
 * Start TX on nvshm channel
 *
 * Used to signal upper driver to start tx again
 * after a XOFF situation
 * Can be called from irq context
 *
 * @param struct nvshm_channel
 *
 */
void nvshm_start_tx(struct nvshm_channel *handle);

#endif /* _NVSHM_IF_H */
