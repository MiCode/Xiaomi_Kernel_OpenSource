/* Copyright (c) 2017-2018,2020 The Linux Foundation. All rights reserved.
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

#ifndef BGCOM_H
#define BGCOM_H

#define BGCOM_REG_TZ_TO_MASTER_STATUS        0x01
#define BGCOM_REG_TZ_TO_MASTER_DATA          0x03
#define BGCOM_REG_SLAVE_STATUS               0x05
#define BGCOM_REG_TIMESTAMP                  0x07
#define BGCOM_REG_SLAVE_STATUS_AUTO_CLEAR    0x09
#define BGCOM_REG_FIFO_FILL                  0x0B
#define BGCOM_REG_FIFO_SIZE                  0x0D
#define BGCOM_REG_TZ_TO_SLAVE_COMMAND        0x0E
#define BGCOM_REG_TZ_TO_SLAVE_DATA           0x10
#define BGCOM_REG_MASTER_STATUS              0x12
#define BGCOM_REG_MASTER_COMMAND             0x14
#define BGCOM_REG_MSG_WR_REG_4               0x16
#define BGCOM_REG_TO_SLAVE_FIFO              0x40
#define BGCOM_REG_TO_MASTER_FIFO             0x41
#define BGCOM_REG_TO_SLAVE_AHB               0x42
#define BGCOM_REG_TO_MASTER_AHB              0x43

/* Enum to define the bgcom SPI state */
enum bgcom_spi_state {
	BGCOM_SPI_FREE = 0,
	BGCOM_SPI_BUSY,
};

/* Enums to identify Blackghost events */
enum bgcom_event_type {
	BGCOM_EVENT_NONE	= 0,
	BGCOM_EVENT_APPLICATION_RUNNING,
	BGCOM_EVENT_TO_SLAVE_FIFO_READY,
	BGCOM_EVENT_TO_MASTER_FIFO_READY,
	BGCOM_EVENT_AHB_READY,
	BGCOM_EVENT_TO_MASTER_FIFO_USED,
	BGCOM_EVENT_TO_SLAVE_FIFO_FREE,
	BGCOM_EVENT_TIMESTAMP_UPDATE,
	BGCOM_EVENT_RESET_OCCURRED,

	BGCOM_EVENT_ERROR_WRITE_FIFO_OVERRUN,
	BGCOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR,
	BGCOM_EVENT_ERROR_WRITE_FIFO_ACCESS,
	BGCOM_EVENT_ERROR_READ_FIFO_UNDERRUN,
	BGCOM_EVENT_ERROR_READ_FIFO_BUS_ERR,
	BGCOM_EVENT_ERROR_READ_FIFO_ACCESS,
	BGCOM_EVENT_ERROR_TRUNCATED_READ,
	BGCOM_EVENT_ERROR_TRUNCATED_WRITE,
	BGCOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS,
	BGCOM_EVENT_ERROR_AHB_BUS_ERR,
	BGCOM_EVENT_ERROR_UNKNOWN,
};

/* Event specific data */
union bgcom_event_data_type {
	uint32_t unused;
	bool application_running;      /* BGCOM_EVENT_APPLICATION_RUNNING */
	bool to_slave_fifo_ready;      /* BGCOM_EVENT_TO_SLAVE_FIFO_READY */
	bool to_master_fifo_ready;     /* BGCOM_EVENT_TO_MASTER_FIFO_READY */
	bool ahb_ready;                /* BGCOM_EVENT_AHB_READY */
	uint16_t to_slave_fifo_free;	/* BGCOM_EVENT_TO_SLAVE_FIFO_FREE */
	struct fifo_event_data {
		uint16_t to_master_fifo_used;
		void *data;
	} fifo_data;
};

/* Client specific data */
struct bgcom_open_config_type {
	/** Private data pointer for client to maintain context.
	 * This data is passed back to client in the notification callbacks.
	 */
	void		*priv;

	/* Notification callbacks to notify the BG events */
	void (*bgcom_notification_cb)(void *handle, void *priv,
			enum bgcom_event_type event,
			union bgcom_event_data_type *event_data);
};

/**
 * bgcom_open() - opens a channel to interact with Blackghost
 * @open_config: pointer to the open configuration structure
 *
 * Open a new connection to blackghost
 *
 * Return a handle on success or NULL on error
 */
void *bgcom_open(struct bgcom_open_config_type *open_config);

/**
 * bgcom_close() - close the exsting with Blackghost
 * @handle: pointer to the handle, provided by bgcom at
 *	bgcom_open
 *
 * Open a new connection to blackghost
 *
 * Return 0 on success or error on invalid handle
 */
int bgcom_close(void **handle);

/**
 * bgcom_reg_read() - Read from the one or more contiguous registers from BG
 * @handle: BGCOM handle associated with the channel
 * @reg_start_addr : 8 bit start address of the registers to read from
 * @num_regs :	Number of contiguous registers to read, starting
 *				from reg_start_addr.
 * @read_buf : Buffer to read from the registers.
 * Return 0 on success or -Ve on error
 */
int bgcom_reg_read(void *handle, uint8_t reg_start_addr,
			uint32_t num_regs, void *read_buf);

/**
 * Write into the one or more contiguous registers.
 *
 * @param[in] handle         BGCOM handle associated with the channel.
 * @param[in] reg_start_addr 8bit start address of the registers to write into.
 * @param[in] num_regs       Number of contiguous registers to write, starting
 *                           from reg_start_addr.
 * @param[in] write_buf      Buffer to write into the registers.
 *
 * @return
 * 0 if function is successful,
 * Otherwise returns error code.
 *
 * @sideeffects  Causes the Blackghost SPI slave to wakeup. Depending up on
 * the operation, it may also wakeup the complete Blackghost.
 */

/**
 * bgcom_reg_write() - Write to the one or more contiguous registers on BG
 * @handle: BGCOM handle associated with the channel
 * @reg_start_addr : 8 bit start address of the registers to read from
 * @num_regs :	Number of contiguous registers to write, starting
 *				from reg_start_addr.
 * @write_buf : Buffer to be written to the registers.
 * Return 0 on success or -Ve on error
 */
int bgcom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf);

/**
 * bgcom_fifo_read() - Read data from the TO_MASTER_FIFO.
 * @handle: BGCOM handle associated with the channel
 * @num_words : number of words to read from FIFO
 * @read_buf : Buffer read from FIFO.
 * Return 0 on success or -Ve on error
 */
int bgcom_fifo_read(void *handle, uint32_t num_words,
		void *read_buf);

/**
 * bgcom_fifo_write() - Write data to the TO_SLAVE_FIFO.
 * @handle: BGCOM handle associated with the channel
 * @num_words : number of words to write on FIFO
 * @write_buf : Buffer written to FIFO.
 * Return 0 on success or -Ve on error
 */
int bgcom_fifo_write(void *handle, uint32_t num_words,
		void *write_buf);

/**
 * bgcom_ahb_read() - Read data from the AHB memory.
 * @handle: BGCOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to read
 * @num_words : number of words to read from AHB
 * @read_buf : Buffer read from FIFO.
 * Return 0 on success or -Ve on error
 */
int bgcom_ahb_read(void *handle, uint32_t ahb_start_addr,
		uint32_t num_words, void *read_buf);

/**
 * bgcom_ahb_write() - Write data to the AHB memory.
 * @handle: BGCOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to start write
 * @num_words : number of words to read from AHB
 * @write_buf : Buffer to write in AHB.
 * Return 0 on success or -Ve on error
 */
int bgcom_ahb_write(void *handle, uint32_t ahb_start_addr,
		uint32_t num_words, void *write_buf);

/**
 * bgcom_suspend() - Suspends the channel.
 * @handle: BGCOM handle associated with the channel
 * Return 0 on success or -Ve on error
 */
int bgcom_suspend(void *handle);

/**
 * bgcom_resume() - Resumes the channel.
 * @handle: BGCOM handle associated with the channel
 * Return 0 on success or -Ve on error
 */
int bgcom_resume(void *handle);

int bgcom_set_spi_state(enum bgcom_spi_state state);

void bgcom_bgdown_handler(void);

#endif /* BGCOM_H */
