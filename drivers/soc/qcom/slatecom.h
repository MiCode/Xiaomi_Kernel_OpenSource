/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef SLATECOM_H
#define SLATECOM_H

#define SLATECOM_REG_TZ_TO_MASTER_STATUS        0x01
#define SLATECOM_REG_TZ_TO_MASTER_DATA          0x03
#define SLATECOM_REG_SLAVE_STATUS               0x05
#define SLATECOM_REG_TIMESTAMP                  0x07
#define SLATECOM_REG_SLAVE_STATUS_AUTO_CLEAR    0x09
#define SLATECOM_REG_FIFO_FILL                  0x0B
#define SLATECOM_REG_FIFO_SIZE                  0x0D
#define SLATECOM_REG_TZ_TO_SLAVE_COMMAND        0x0E
#define SLATECOM_REG_TZ_TO_SLAVE_DATA           0x10
#define SLATECOM_REG_MASTER_STATUS              0x12
#define SLATECOM_REG_MASTER_COMMAND             0x14
#define SLATECOM_REG_MSG_WR_REG_4               0x16
#define SLATECOM_REG_TO_SLAVE_FIFO              0x40
#define SLATECOM_REG_TO_MASTER_FIFO             0x41
#define SLATECOM_REG_TO_SLAVE_AHB               0x42
#define SLATECOM_REG_TO_MASTER_AHB              0x43

/* Enum to define the slatecom SPI state */
enum slatecom_spi_state {
	SLATECOM_SPI_FREE = 0,
	SLATECOM_SPI_BUSY,
};

/* Enums to identify Blackghost events */
enum slatecom_event_type {
	SLATECOM_EVENT_NONE	= 0,
	SLATECOM_EVENT_APPLICATION_RUNNING,
	SLATECOM_EVENT_TO_SLAVE_FIFO_READY,
	SLATECOM_EVENT_TO_MASTER_FIFO_READY,
	SLATECOM_EVENT_AHB_READY,
	SLATECOM_EVENT_TO_MASTER_FIFO_USED,
	SLATECOM_EVENT_TO_SLAVE_FIFO_FREE,
	SLATECOM_EVENT_TIMESTAMP_UPDATE,
	SLATECOM_EVENT_RESET_OCCURRED,

	SLATECOM_EVENT_ERROR_WRITE_FIFO_OVERRUN,
	SLATECOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR,
	SLATECOM_EVENT_ERROR_WRITE_FIFO_ACCESS,
	SLATECOM_EVENT_ERROR_READ_FIFO_UNDERRUN,
	SLATECOM_EVENT_ERROR_READ_FIFO_BUS_ERR,
	SLATECOM_EVENT_ERROR_READ_FIFO_ACCESS,
	SLATECOM_EVENT_ERROR_TRUNCATED_READ,
	SLATECOM_EVENT_ERROR_TRUNCATED_WRITE,
	SLATECOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS,
	SLATECOM_EVENT_ERROR_AHB_BUS_ERR,
	SLATECOM_EVENT_ERROR_UNKNOWN,
};

/* Event specific data */
union slatecom_event_data_type {
	uint32_t unused;
	bool application_running;      /* SLATECOM_EVENT_APPLICATION_RUNNING */
	bool to_slave_fifo_ready;      /* SLATECOM_EVENT_TO_SLAVE_FIFO_READY */
	bool to_master_fifo_ready;     /* SLATECOM_EVENT_TO_MASTER_FIFO_READY */
	bool ahb_ready;                /* SLATECOM_EVENT_AHB_READY */
	uint16_t to_slave_fifo_free;	/* SLATECOM_EVENT_TO_SLAVE_FIFO_FREE */
	struct fifo_event_data {
		uint16_t to_master_fifo_used;
		void *data;
	} fifo_data;
};

struct event {
	uint8_t sub_id;
	int16_t evnt_data;
	uint32_t evnt_tm;
};

/* Client specific data */
struct slatecom_open_config_type {
	/** Private data pointer for client to maintain context.
	 * This data is passed back to client in the notification callbacks.
	 */
	void		*priv;

	/* Notification callbacks to notify the SLATE events */
	void (*slatecom_notification_cb)(void *handle, void *priv,
			enum slatecom_event_type event,
			union slatecom_event_data_type *event_data);
};

/**
 * slatecom_open() - opens a channel to interact with Blackghost
 * @open_config: pointer to the open configuration structure
 *
 * Open a new connection to blackghost
 *
 * Return a handle on success or NULL on error
 */
void *slatecom_open(struct slatecom_open_config_type *open_config);

/**
 * slatecom_close() - close the exsting with Blackghost
 * @handle: pointer to the handle, provided by slatecom at
 *	slatecom_open
 *
 * Open a new connection to blackghost
 *
 * Return 0 on success or error on invalid handle
 */
int slatecom_close(void **handle);

/**
 * slatecom_reg_read() - Read from the one or more contiguous registers from SLATE
 * @handle: SLATECOM handle associated with the channel
 * @reg_start_addr : 8 bit start address of the registers to read from
 * @num_regs :	Number of contiguous registers to read, starting
 *				from reg_start_addr.
 * @read_buf : Buffer to read from the registers.
 * Return 0 on success or -Ve on error
 */
int slatecom_reg_read(void *handle, uint8_t reg_start_addr,
			uint32_t num_regs, void *read_buf);

/**
 * Write into the one or more contiguous registers.
 *
 * @param[in] handle         SLATECOM handle associated with the channel.
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
 * slatecom_reg_write() - Write to the one or more contiguous registers on SLATE
 * @handle: SLATECOM handle associated with the channel
 * @reg_start_addr : 8 bit start address of the registers to read from
 * @num_regs :	Number of contiguous registers to write, starting
 *				from reg_start_addr.
 * @write_buf : Buffer to be written to the registers.
 * Return 0 on success or -Ve on error
 */
int slatecom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf);

/**
 * slatecom_fifo_read() - Read data from the TO_MASTER_FIFO.
 * @handle: SLATECOM handle associated with the channel
 * @num_words : number of words to read from FIFO
 * @read_buf : Buffer read from FIFO.
 * Return 0 on success or -Ve on error
 */
int slatecom_fifo_read(void *handle, uint32_t num_words,
		void *read_buf);

/**
 * slatecom_fifo_write() - Write data to the TO_SLAVE_FIFO.
 * @handle: SLATECOM handle associated with the channel
 * @num_words : number of words to write on FIFO
 * @write_buf : Buffer written to FIFO.
 * Return 0 on success or -Ve on error
 */
int slatecom_fifo_write(void *handle, uint32_t num_words,
		void *write_buf);

/**
 * slatecom_ahb_read() - Read data from the AHB memory.
 * @handle: SLATECOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to read
 * @num_words : number of words to read from AHB
 * @read_buf : Buffer read from FIFO.
 * Return 0 on success or -Ve on error
 */
int slatecom_ahb_read(void *handle, uint32_t ahb_start_addr,
		uint32_t num_words, void *read_buf);

/**
 * slatecom_ahb_write() - Write data to the AHB memory.
 * @handle: SLATECOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to start write
 * @num_words : number of words to read from AHB
 * @write_buf : Buffer to write in AHB.
 * Return 0 on success or -Ve on error
 */
int slatecom_ahb_write(void *handle, uint32_t ahb_start_addr,
		uint32_t num_words, void *write_buf);

/**
 * slatecom_suspend() - Suspends the channel.
 * @handle: SLATECOM handle associated with the channel
 * Return 0 on success or -Ve on error
 */
int slatecom_suspend(void *handle);

/**
 * slatecom_resume() - Resumes the channel.
 * @handle: SLATECOM handle associated with the channel
 * Return 0 on success or -Ve on error
 */
int slatecom_resume(void *handle);

int slatecom_set_spi_state(enum slatecom_spi_state state);

void slatecom_slatedown_handler(void);

#endif /* SLATECOM_H */
