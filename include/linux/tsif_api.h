/**
 * TSIF driver
 *
 * Kernel API
 *
 * Copyright (c) 2009-2010, 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _TSIF_API_H_
#define _TSIF_API_H_
/**
 * Theory of operation
 *
 * TSIF driver maintains internal cyclic data buffer where
 * received TSIF packets are stored. Size of buffer, in packets,
 * and its address, may be obtained by tsif_get_info().
 *
 * TSIF stream delivered to the client that should register with
 * TSIF driver using tsif_attach()
 *
 * Producer-consumer pattern used. TSIF driver act as producer,
 * writing data to the buffer; clientis consumer.
 * 2 indexes maintained by the TSIF driver:
 * - wi (write index) points to the next item to be written by
 *   TSIF
 * - ri (read index) points to the next item available for read
 *   by the client.
 * Write index advanced by the TSIF driver when new data
 * received;
 * Read index advanced only when client tell so to the TSIF
 * driver by tsif_reclaim_packets()
 *
 * Consumer may directly access data TSIF buffer between ri and
 * wi. When ri==wi, buffer is empty.
 *
 * TSIF driver notifies client about any change by calling
 * notify function. Client should use tsif_get_state() to query
 * new state.
 */

/* bytes in TSIF packet. not customizable */
#define TSIF_PKT_SIZE             (192)

/**
 * tsif_pkt_status - get TSIF packet status
 *
 * @pkt: TSIF packet location
 *
 * Return last DWORD of packet, containing status.
 * Status dword consists of:
 * - 3 low bytes TTS
 * - 1 byte (last byte of packet) with status bits
 */
static inline u32 tsif_pkt_status(void *pkt)
{
	u32 *x = pkt;
	return x[TSIF_PKT_SIZE / sizeof(u32) - 1];
}

/**
 * Status dword parts for status returned by @tsif_pkt_status
 */
#define TSIF_STATUS_TTS(x)   ((x) & 0xffffff)
#define TSIF_STATUS_VALID(x) ((x) & (1<<24))
#define TSIF_STATUS_FIRST(x) ((x) & (1<<25))
#define TSIF_STATUS_OVFLW(x) ((x) & (1<<26))
#define TSIF_STATUS_ERROR(x) ((x) & (1<<27))
#define TSIF_STATUS_NULL(x)  ((x) & (1<<28))
#define TSIF_STATUS_TIMEO(x) ((x) & (1<<30))

/**
 * enum tsif_state - TSIF device state
 * @tsif_state_stopped:     Idle state, data acquisition not running
 * @tsif_state_running:     Data acquisition in progress
 * @tsif_state_flushing:    Device is flushing
 *
 * State transition diagram:
 *
 * init -> tsif_state_stopped
 *
 * tsif_state_stopped:
 *   - open -> tsif_state_running
 *
 * tsif_state_running:
 *   - close -> tsif_state_flushing
 *
 * tsif_state_flushing:
 *   - flushed -> tsif_state_stopped
 */
enum tsif_state {
	tsif_state_stopped  = 0,
	tsif_state_running  = 1,
	tsif_state_flushing = 2,
	tsif_state_error    = 3,
};

/**
 * tsif_get_active - return active tsif hardware instance
 *
 * Return     TSIF instance to use (selected by CONFIG_MSM_USE_TSIF1)
 */
int tsif_get_active(void);

/**
 * tsif_attach - Attach to the device.
 * @id:       TSIF device ID, used to identify TSIF instance.
 * @notify:   client callback, called when
 *            any client visible TSIF state changed.
 *            This includes new data available and device state change
 * @data:     client data, will be passed to @notify
 *
 * Return     TSIF cookie or error code
 *
 * Should be called prior to any other tsif_XXX function.
 */
void *tsif_attach(int id, void (*notify)(void *client_data), void *client_data);

/**
 * tsif_detach - detach from device
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 */
void tsif_detach(void *cookie);

/**
 * tsif_get_info - get data buffer info
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 * @pdata:     if not NULL, TSIF data buffer will be stored there
 * @psize:     if not NULL, TSIF data buffer size, in packets,
 *             will be stored there
 *
 * Data buffer information should be queried after each tsif_start() before
 * using data; since data buffer will be re-allocated on tsif_start()
 */
void tsif_get_info(void *cookie, void **pdata, int *psize);

/**
 * tsif_set_mode - set TSIF mode
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 * @mode:      desired mode of operation
 *
 * Return      error code
 *
 * Mode may be changed only when TSIF device is stopped.
 */
int tsif_set_mode(void *cookie, int mode);

/**
 * tsif_set_time_limit - set TSIF time limit
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 * @value:     desired time limit, 0 to disable
 *
 * Return      error code
 *
 * Time limit may be changed only when TSIF device is stopped.
 */
int tsif_set_time_limit(void *cookie, u32 value);

/**
 * tsif_set_buf_config - configure data buffer
 *
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 * @pkts_in_chunk: requested number of packets per chunk
 * @chunks_in_buf: requested number of chunks in buffer
 *
 * Return      error code
 *
 * Parameter selection criteria:
 *
 * - @pkts_in_chunk defines size of DMA transfer and, in turn, time between
 *   consecutive DMA transfers. Increase @pkts_in_chunk reduces chance for
 *   hardware overflow. If TSIF stats reports overflows, increase it.
 *
 * - @chunks_in_buf * @pkts_in_chunk defines total buffer size. Increase this
 *   parameter if client latency is large and TSIF reports "soft drop" in its
 *   stats
 */
int tsif_set_buf_config(void *cookie, u32 pkts_in_chunk, u32 chunks_in_buf);

/**
 * tsif_get_state - query current data buffer information
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 * @ri:        if not NULL, read index will be stored here
 * @wi:        if not NULL, write index will be stored here
 * @state:     if not NULL, state will be stored here
 */
void tsif_get_state(void *cookie, int *ri, int *wi, enum tsif_state *state);

/**
 * tsif_set_clk_inverse - set whether to inverse the clock signal.
 * @cookie:   TSIF cookie previously obtained with tsif_attach()
 * @inverse:  1 to inverse the clock, 0 otherwise. Default is 0.
 *
 * Return      error code
 *
 * Setting may be changed only when TSIF device is stopped.
 */
int tsif_set_clk_inverse(void *cookie, int inverse);

/**
 * tsif_set_data_inverse - set whether to inverse the data signal.
 * @cookie:   TSIF cookie previously obtained with tsif_attach()
 * @inverse:  1 to inverse the clock, 0 otherwise. Default is 0.
 *
 * Return      error code
 *
 * Setting may be changed only when TSIF device is stopped.
 */
int tsif_set_data_inverse(void *cookie, int inverse);

/**
 * tsif_set_sync_inverse - set whether to inverse the sync signal.
 * @cookie:   TSIF cookie previously obtained with tsif_attach()
 * @inverse:  1 to inverse the clock, 0 otherwise. Default is 0.
 *
 * Return      error code
 *
 * Setting may be changed only when TSIF device is stopped.
 */
int tsif_set_sync_inverse(void *cookie, int inverse);

/**
 * tsif_set_enable_inverse - set whether to inverse the enable signal.
 * @cookie:   TSIF cookie previously obtained with tsif_attach()
 * @inverse:  1 to inverse the clock, 0 otherwise. Default is 0.
 *
 * Return      error code
 *
 * Setting may be changed only when TSIF device is stopped.
 */
int tsif_set_enable_inverse(void *cookie, int inverse);

/**
 * tsif_start - start data acquisition
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 *
 * Return      error code
 */
int tsif_start(void *cookie);

/**
 * tsif_stop - stop data acquisition
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 *
 * Data buffer allocated during this function call; thus client should
 * query data buffer info using tsif_get_info() and reset its data pointers.
 */
void tsif_stop(void *cookie);

/**
 * tsif_reclaim_packets - inform that buffer space may be reclaimed
 * @cookie:    TSIF cookie previously obtained with tsif_attach()
 * @ri:        new value for read index
 */
void tsif_reclaim_packets(void *cookie, int ri);

#endif /* _TSIF_API_H_ */

