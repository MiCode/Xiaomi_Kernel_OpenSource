/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _QCN_SDIO_AL_
#define _QCN_SDIO_AL_


/**
 * ------------------------------------
 * ------- SDIO AL Interface ----------
 * ------------------------------------
 *
 * This file contains the proposed SDIO AL (Abstraction Layer) interface.
 * Terminologies:
 *	SDIO AL : SDIO host function-1 driver
 *	SDIO AL client: Clients of SDIO host function-1 driver.
 *			WLAN, QMI, DIAG etc. are possible clients.
 *	Remote SDIO client: SDIO client on device side which implements ADMA
 *			    functionality as function-1
 */

enum sdio_al_dma_direction {
	SDIO_AL_TX,
	SDIO_AL_RX,
};

/**
 * struct sdio_al_client_handle - unique handler to identify
 *				  each SDIO AL (Abstraction Layer) client
 *
 * @id: unique id for each client
 * @block_size: block size
 * @func: pointer to sdio_func data structure, some clients may need this.
 * @client_priv: This is client priv that can used by client driver.
 */
struct sdio_al_client_handle {
	int id;
	struct sdio_al_client_data *client_data;
	unsigned int block_size;
	struct sdio_func *func;
	void *client_priv;
};

/**
 * struct sdio_al_xfer_result - Completed buffer information
 *
 * @buf_addr: Address of data buffer
 *
 * @xfer_len: Transfer data length in bytes
 *
 * @xfer_status: status of transfer, 0 if successful,
 *			negative in case of error
 */
struct sdio_al_xfer_result {
	void *buf_addr;
	size_t xfer_len;
	int xfer_status;
};

enum sdio_al_lpm_event {
	LPM_ENTER, /* SDIO client will be put to LPM mode soon */
	LPM_EXIT,  /* SDIO client has exited LPM mode */
};

/**
 * sdio_al_client_data - client data of sdio_al
 *
 * @name: client name, could be one of the following:
 *                  "SDIO_AL_CLIENT_WLAN",
 *                  "SDIO_AL_CLIENT_QMI",
 *                  "SDIO_AL_CLIENT_DIAG",
 *                  "SDIO_AL_CLIENT_TTY"
 *
 * @probe: This probe function is called by SDIO AL driver when it is ready for
 *	   SDIO traffic. SDIO AL client must wait for this callback before
 *	   initiating any transfer over SDIO transport layer.
 *
 * @remove: This remove function is called by SDIO AL driver when it isn't ready
 *	    for SDIO traffic. SDIO AL client must stop issuing any transfers
 *	    after getting this callback, ongoing transfers would be errored out
 *	    by SDIO AL.
 *
 * @lpm_notify_cb: callback to notify SDIO AL clients about Low Power modes.
 *
 */
struct sdio_al_client_data {
	const char *name;

	int id;

	int mode;

	int (*probe)(struct sdio_al_client_handle *);

	int (*remove)(struct sdio_al_client_handle *);

	void (*lpm_notify_cb)(struct sdio_al_client_handle *,
			enum sdio_al_lpm_event event);
};

/**
 * sdio_al_channel_handle - channel handle of sdio_al
 *
 * @id: Channel id unique at the AL layer
 *
 * @client_data: Client to which this channel belongs
 *
 */
struct sdio_al_channel_handle {
	unsigned int channel_id;

	struct sdio_al_channel_data *channel_data;
	void *priv;
};

/**
 * sdio_al_channel_data - channel data of sdio_al
 *
 * @name: channel name, could be one of the following:
 *                  "SDIO_AL_WLAN_CH0",
 *                  "SDIO_AL_WLAN_CH1",
 *                  "SDIO_AL_QMI_CH0",
 *                  "SDIO_AL_DIAG_CH0",
 *                  "SDIO_AL_TTY_CH0"
 *
 * @client_data: The client driver by which this channel is being claimed
 *
 * @ul_xfer_cb: UL/TX data transfer callback.
 *		SDIO AL client can queue request using sdio_al_queue_transfer()
 *		asynchronous API, once request is transported over SDIO
 *		transport, SDIO AL calls "ul_xfer_cb" to notify the transfer
 complete.
 *
 * @dl_xfer_cb: DL/RX data transfer callback
 *		Once SDIO AL receives requested data from remote SDIO client
 *		then SDIO AL invokes "dl_xfer_cb" callback to notify the SDIO
 *		AL client.
 *
 * @dl_data_avail_cb: callback to notify SDIO AL client that it can read
 *		specified bytes of data from remote SDIO client, SDIO AL client
 *		is then expected call sdio_al_queue_transfer() to read the data.
 *		This is optional and if client doesn't provide this callback
 *		then SDIO AL would allocate the buffer and SDIO AL
 *		client would have to memcpy the buffer in dl_xfer_cb().
 *
 */
struct sdio_al_channel_data {
	const char *name;

	struct sdio_al_client_data *client_data;

	void (*ul_xfer_cb)(struct sdio_al_channel_handle *,
			struct sdio_al_xfer_result *, void *ctxt);

	void (*dl_xfer_cb)(struct sdio_al_channel_handle *,
			struct sdio_al_xfer_result *, void *ctxt);

	void (*dl_data_avail_cb)(struct sdio_al_channel_handle *,
			unsigned int len);

	void (*dl_meta_data_cb)(struct sdio_al_channel_handle *,
			unsigned int data);
};

/**
 * sdio_al_is_ready - API Check to know whether the al driver is ready
 * This API can be used to deffer the probe incase of early execution.
 *
 * @return zero on success and negative value on error.
 *
 */
int sdio_al_is_ready(void);

/**
 * sdio_al_register_client - register as client of sdio AL (function-1 driver)
 *  SDIO AL driver would allocate the unique instance of
 *  "struct sdio_al_client_handle" and returns to client.
 *
 * @client_data: pointer to SDIO AL client data (struct sdio_al_client_data)
 *
 * @return valid sdio_al_client_handler ptr on success, negative value on error.
 *
 */
struct sdio_al_client_handle *sdio_al_register_client(
		struct sdio_al_client_data *client_data);

/**
 * sdio_al_deregister_client - deregisters client from SDIO AL
 * (function-1 driver)
 *
 * @handle: sdio_al client handler
 *
 */
void sdio_al_deregister_client(struct sdio_al_client_handle *handle);

/**
 * sdio_al_register_channel - register a channel for a client of SDIO AL
 * SDIO AL driver would allocate a unique instance of the "struct
 * sdio_al_channel_handle" and returns to the client.
 *
 * @client_handle: The client to which the channel shall belong
 *
 * @channel_data: The channel data which contains the details of the channel
 *
 * @return valid channel handle in success error on success, error pointer on
 * failure
 */
struct sdio_al_channel_handle *sdio_al_register_channel(
		struct sdio_al_client_handle *client_handle,
		struct sdio_al_channel_data *client_data);

/**
 * sdio_al_deregister_channel - deregister a channel for a client of SDIO AL
 *
 * @ch_handle: The channel handle which needs to deregistered
 *
 * @return none
 */
void sdio_al_deregister_channel(struct sdio_al_channel_handle *ch_handle);


/**
 * sdio_al_queue_transfer_async - Queue asynchronous data transfer request
 * All transfers are asynchronous transfers, SDIO AL will call
 * ul_xfer_cb or dl_xfer_cb callback to nofity completion to SDIO AL client.
 *
 * @ch_handle: sdio_al channel handle
 *
 * @dir: Data direction (DMA_TO_DEVICE for TX, DMA_FROM_DEVICE for RX)
 *
 * @buf: Data buffer
 *
 * @len: Size in bytes
 *
 * @priority: set any non-zero value for higher priority, 0 for normal priority
 *	      All SDIO AL clients except WLAN client is expected to use normal
 *	      priority.
 *
 * @return 0 on success, non-zero in case of error
 */
int sdio_al_queue_transfer_async(struct sdio_al_channel_handle *handle,
		enum sdio_al_dma_direction dir,
		void *buf, size_t len, int priority, void *ctxt);

/**
 * sdio_al_queue_transfer - Queue synchronous data transfer request
 * In constrast to asynchronous transfer API sdio_al_queue_transfer(), this
 * API will completely the request synchronously. If there is no outstanding
 * request at SDIO AL Layer, request will be immediately initiated on SDIO bus.
 *
 * @ch_handle: sdio_al channel handle
 *
 * @dir: Data direction (DMA_TO_DEVICE for TX, DMA_FROM_DEVICE for RX)
 *
 * @buf: Data buffer
 *
 * @len: Size in bytes
 *
 * @priority: set any non-zero value for higher priority, 0 for normal priority
 *	      All SDIO AL clients except WLAN client is expected to use normal
 *	      priority.
 *
 * @return 0 on success, non-zero in case of error
 */
int sdio_al_queue_transfer(struct sdio_al_channel_handle *ch_handle,
		enum sdio_al_dma_direction dir,
		void *buf, size_t len, int priority);


/**
 * sdio_al_meta_transfer - Queue synchronous data transfer request
 * In constrast to asynchronous transfer API sdio_al_queue_transfer(), this
 * API will completely the request synchronously. If there is no outstanding
 * request at SDIO AL Layer, request will be immediately initiated on SDIO bus.
 *
 * @ch_handle: sdio_al channel handle
 *
 * @data: Meta data to be transferred
 *
 * @return 0 on success, non-zero in case of error
 */
int sdio_al_meta_transfer(struct sdio_al_channel_handle *ch_handle,
		unsigned int data, unsigned int trans);

extern void qcn_sdio_client_probe_complete(int id);
int qcn_sdio_card_state(bool enable);
#endif /* _QCN_SDIO_AL_ */
