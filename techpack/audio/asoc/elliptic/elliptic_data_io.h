/**
* Copyright Elliptic Labs 2015-2016
*
*/

#pragma once

#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#define ELLIPTIC_DATA_IO_AP_TO_DSP 0
#define ELLIPTIC_DATA_IO_DSP_TO_AP 1

#define ELLIPTIC_DATA_IO_READ_OK 0
#define ELLIPTIC_DATA_IO_READ_BUSY 1
#define ELLIPTIC_DATA_IO_READ_CANCEL 2

#define ELLIPTIC_MSG_BUF_SIZE 512

/* wake source timeout in ms*/
#define ELLIPTIC_WAKEUP_TIMEOUT 250

#define ELLIPTIC_DATA_FIFO_SIZE (PAGE_SIZE)


enum elliptic_message_id {
	ELLIPTIC_MESSAGE_PAYLOAD,   /* Input to AP*/
	ELLIPTIC_MESSAGE_RAW,       /* Output from AP*/
	ELLIPTIC_MESSAGE_CALIBRATION,
	ELLIPTIC_MAX_MESSAGE_IDS
};

struct elliptic_data {
	unsigned int wakeup_timeout; /* wake lock timeout */

	/* members for top half interrupt handling */
	struct kfifo fifo_isr;
	spinlock_t fifo_isr_spinlock;

	/* buffer to swap data from isr fifo to userspace fifo */
	uint8_t isr_swap_buffer[ELLIPTIC_MSG_BUF_SIZE];

	/* members for bottom half handling */
	struct kfifo fifo_userspace;
	struct mutex fifo_usp_lock;
	wait_queue_head_t fifo_usp_not_empty;

	atomic_t abort_io;
	struct work_struct work;
	struct workqueue_struct *wq;

	/* debug counters, reset between open/close */
	uint32_t isr_fifo_flush_count;
	uint32_t userspace_fifo_flush_count;

	/* debug counters, persistent */
	uint32_t isr_fifo_flush_count_total;
	uint32_t userspace_fifo_flush_count_total;
	uint32_t userspace_read_total;
	uint32_t isr_write_total;

};

/* Elliptic IO module API (implemented by IO module)*/

int elliptic_data_io_initialize(void);
int elliptic_data_io_cleanup(void);
void elliptic_data_io_cancel(struct elliptic_data *);

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size);

int32_t elliptic_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size);


/* Elliptic driver API (implemented by main driver)*/
int elliptic_data_initialize(struct elliptic_data *,
	size_t max_queue_size, unsigned int wakeup_timeout, int id);

int elliptic_data_cleanup(struct elliptic_data *);

void elliptic_data_reset_debug_counters(struct elliptic_data *);
void elliptic_data_update_debug_counters(struct elliptic_data *);
void elliptic_data_print_debug_counters(struct elliptic_data *);

size_t elliptic_data_pop(struct elliptic_data *,
	char __user *buffer, size_t buffer_size);

void elliptic_data_cancel(struct elliptic_data *);

/* Called from IO module*/
int elliptic_data_push(const char *buffer, size_t buffer_size);

