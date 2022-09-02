/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef HELIOSCOM_H
#define HELIOSCOM_H

#define HELIOSCOM_REG_TZ_TO_MASTER_STATUS        0x01
#define HELIOSCOM_REG_TZ_TO_MASTER_DATA          0x03
#define HELIOSCOM_REG_SLAVE_STATUS               0x05
#define HELIOSCOM_REG_TIMESTAMP                  0x07
#define HELIOSCOM_REG_SLAVE_STATUS_AUTO_CLEAR    0x09
#define HELIOSCOM_REG_FIFO_FILL                  0x0B
#define HELIOSCOM_REG_FIFO_SIZE                  0x0D
#define HELIOSCOM_REG_TZ_TO_SLAVE_COMMAND        0x0E
#define HELIOSCOM_REG_TZ_TO_SLAVE_DATA           0x10
#define HELIOSCOM_REG_MASTER_STATUS              0x12
#define HELIOSCOM_REG_MASTER_COMMAND             0x14
#define HELIOSCOM_REG_MSG_WR_REG_4               0x16
#define HELIOSCOM_REG_TO_SLAVE_FIFO              0x40
#define HELIOSCOM_REG_TO_MASTER_FIFO             0x41
#define HELIOSCOM_REG_TO_SLAVE_AHB               0x42
#define HELIOSCOM_REG_TO_MASTER_AHB              0x43

/* Enum to define the helioscom SPI state */
enum helioscom_spi_state {
	HELIOSCOM_SPI_FREE = 0,
	HELIOSCOM_SPI_BUSY,
	HELIOSCOM_SPI_PAUSE,
};

/* Enums to identify Blackghost events */
enum helioscom_event_type {
	HELIOSCOM_EVENT_NONE	= 0,
	HELIOSCOM_EVENT_APPLICATION_RUNNING,
	HELIOSCOM_EVENT_TO_SLAVE_FIFO_READY,
	HELIOSCOM_EVENT_TO_MASTER_FIFO_READY,
	HELIOSCOM_EVENT_AHB_READY,
	HELIOSCOM_EVENT_TO_MASTER_FIFO_USED,
	HELIOSCOM_EVENT_TO_SLAVE_FIFO_FREE,
	HELIOSCOM_EVENT_TIMESTAMP_UPDATE,
	HELIOSCOM_EVENT_RESET_OCCURRED,

	HELIOSCOM_EVENT_ERROR_WRITE_FIFO_OVERRUN,
	HELIOSCOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR,
	HELIOSCOM_EVENT_ERROR_WRITE_FIFO_ACCESS,
	HELIOSCOM_EVENT_ERROR_READ_FIFO_UNDERRUN,
	HELIOSCOM_EVENT_ERROR_READ_FIFO_BUS_ERR,
	HELIOSCOM_EVENT_ERROR_READ_FIFO_ACCESS,
	HELIOSCOM_EVENT_ERROR_TRUNCATED_READ,
	HELIOSCOM_EVENT_ERROR_TRUNCATED_WRITE,
	HELIOSCOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS,
	HELIOSCOM_EVENT_ERROR_AHB_BUS_ERR,
	HELIOSCOM_EVENT_ERROR_UNKNOWN,
};

enum helioscom_reset_type {
	/*HELIOSCOM reset request type*/
	HELIOSCOM_HELIOS_CRASH = 0,
	HELIOSCOM_HELIOS_STUCK = 1,
	HELIOSCOM_OEM_PROV_PASS = 2,
	HELIOSCOM_OEM_PROV_FAIL = 3,
};

/* Event specific data */
union helioscom_event_data_type {
	uint32_t unused;
	bool application_running;      /* HELIOSCOM_EVENT_APPLICATION_RUNNING */
	bool to_slave_fifo_ready;      /* HELIOSCOM_EVENT_TO_SLAVE_FIFO_READY */
	bool to_master_fifo_ready;     /* HELIOSCOM_EVENT_TO_MASTER_FIFO_READY */
	bool ahb_ready;                /* HELIOSCOM_EVENT_AHB_READY */
	uint16_t to_slave_fifo_free;	/* HELIOSCOM_EVENT_TO_SLAVE_FIFO_FREE */
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
struct helioscom_open_config_type {
	/** Private data pointer for client to maintain context.
	 * This data is passed back to client in the notification callbacks.
	 */
	void		*priv;

	/* Notification callbacks to notify the HELIOS events */
	void (*helioscom_notification_cb)(void *handle, void *priv,
			enum helioscom_event_type event,
			union helioscom_event_data_type *event_data);
};

/* Client specific data */
struct helioscom_reset_config_type {
	/** Private data pointer for client to maintain context.
	 * This data is passed back to client in the notification callbacks.
	 */
	void		*priv;

	/* Notification callbacks to notify the HELIOS events */
	void (*helioscom_reset_notification_cb)(void *handle, void *priv,
			enum helioscom_reset_type reset_type);
};

/**
 * helioscom_reset_register() - Register to get reset notification
 * @open_config: pointer to the open configuration structure helioscom_reset_config_type
 *
 * Open a new connection to Helioscom
 *
 * Return a handle on success or NULL on error
 */
void *helioscom_pil_reset_register(struct helioscom_reset_config_type *open_config);

/**
 * helioscom_pil_reset_unregister() - Unregister for reset notfication
 * @handle: pointer to the handle, provided by helioscom at
 *	helioscom_open
 *
 * Unregister for helioscom pil notification.
 *
 * Return 0 on success or error on invalid handle
 */
int helioscom_pil_reset_unregister(void **handle);

/**
 * helioscom_open() - opens a channel to interact with Helioscom
 * @open_config: pointer to the open configuration structure
 *
 * Open a new connection to Helioscom
 *
 * Return a handle on success or NULL on error
 */
void *helioscom_open(struct helioscom_open_config_type *open_config);

/**
 * helioscom_close() - close the exsting channel with Helioscom
 * @handle: pointer to the handle, provided by helioscom at
 *	helioscom_open
 *
 * close existing connection to Helioscom
 *
 * Return 0 on success or error on invalid handle
 */
int helioscom_close(void **handle);

/**
 * helioscom_reg_read() - Read from the one or more contiguous registers from HELIOS
 * @handle: HELIOSCOM handle associated with the channel
 * @reg_start_addr : 8 bit start address of the registers to read from
 * @num_regs :	Number of contiguous registers to read, starting
 *				from reg_start_addr.
 * @read_buf : Buffer to read from the registers.
 * Return 0 on success or -Ve on error
 */
int helioscom_reg_read(void *handle, uint8_t reg_start_addr,
			uint32_t num_regs, void *read_buf);

/**
 * Write into the one or more contiguous registers.
 *
 * @param[in] handle         HELIOSCOM handle associated with the channel.
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
 * helioscom_reg_write() - Write to the one or more contiguous registers on HELIOS
 * @handle: HELIOSCOM handle associated with the channel
 * @reg_start_addr : 8 bit start address of the registers to read from
 * @num_regs :	Number of contiguous registers to write, starting
 *				from reg_start_addr.
 * @write_buf : Buffer to be written to the registers.
 * Return 0 on success or -Ve on error
 */
int helioscom_reg_write(void *handle, uint8_t reg_start_addr,
	uint8_t num_regs, void *write_buf);

/**
 * helioscom_fifo_read() - Read data from the TO_MASTER_FIFO.
 * @handle: HELIOSCOM handle associated with the channel
 * @num_words : number of words to read from FIFO
 * @read_buf : Buffer read from FIFO.
 * Return 0 on success or -Ve on error
 */
int helioscom_fifo_read(void *handle, uint32_t num_words,
		void *read_buf);

/**
 * helioscom_fifo_write() - Write data to the TO_SLAVE_FIFO.
 * @handle: HELIOSCOM handle associated with the channel
 * @num_words : number of words to write on FIFO
 * @write_buf : Buffer written to FIFO.
 * Return 0 on success or -Ve on error
 */
int helioscom_fifo_write(void *handle, uint32_t num_words,
		void *write_buf);

/**
 * helioscom_ahb_read() - Read data from the AHB memory.
 * @handle: HELIOSCOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to read
 * @num_words : number of words to read from AHB
 * @read_buf : Buffer read from FIFO.
 * Return 0 on success or -Ve on error
 */
int helioscom_ahb_read(void *handle, uint32_t ahb_start_addr,
		uint32_t num_words, void *read_buf);

/**
 * helioscom_ahb_write_bytes() - Write byte data to the AHB memory.
 * @handle: HELIOSCOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to start write
 * @num_bytes : number of bytes to read from AHB
 * @write_buf : Buffer to write in AHB.
 * Return 0 on success or -Ve on error
 */
int helioscom_ahb_write_bytes(void *handle, uint32_t ahb_start_addr,
		uint32_t num_bytes, void *write_buf);
/**
 * helioscom_ahb_write() - Write data to the AHB memory.
 * @handle: HELIOSCOM handle associated with the channel
 * @ahb_start_addr : Memory start address from where to start write
 * @num_words : number of words to read from AHB
 * @write_buf : Buffer to write in AHB.
 * Return 0 on success or -Ve on error
 */
int helioscom_ahb_write(void *handle, uint32_t ahb_start_addr,
		uint32_t num_words, void *write_buf);

/**
 * helioscom_suspend() - Suspends the channel.
 * @handle: HELIOSCOM handle associated with the channel
 * Return 0 on success or -Ve on error
 */
int helioscom_suspend(void *handle);

/**
 * helioscom_resume() - Resumes the channel.
 * @handle: HELIOSCOM handle associated with the channel
 * Return 0 on success or -Ve on error
 */
int helioscom_resume(void *handle);

int helioscom_set_spi_state(enum helioscom_spi_state state);

void helioscom_heliosdown_handler(void);

int set_helios_sleep_state(bool sleep_state);

int get_helios_sleep_state(void);

#endif /* HELIOSCOM_H */
