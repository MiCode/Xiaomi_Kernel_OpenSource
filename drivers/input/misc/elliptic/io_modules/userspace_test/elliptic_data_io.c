/**
 * Copyright Elliptic Labs
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "elliptic_data_io.h"
#include "elliptic_device.h"


#define USE_IRQ 11

static struct task_struct *simulating_task;
static atomic_t cancel;

struct elliptic_data_io_state {
};
#define BUFFER_SIZE 128

static int32_t output_buffer[BUFFER_SIZE];

irqreturn_t irq_handler(int irq, void *dev_id)
{
	int result;

	result = elliptic_data_push(ELLIPTIC_ALL_DEVICES,
				(const char *)output_buffer, BUFFER_SIZE * sizeof(int32_t));
	return 0;
}


static void fill_buffer(int32_t *buffer, size_t len, int32_t value)
{
	size_t i;

	for (i = 0; i < len; ++i)
		buffer[i] = value;
}


int simulating_thread(void *context)
{
	static int32_t count;
	int result;

	count = 0;
	msleep(100);

	pr_debug("%s\n", __func__);
	while (atomic_read(&cancel) == 0) {
		if (kthread_should_stop())
			do_exit(0);

		fill_buffer(output_buffer, BUFFER_SIZE, count);

		++count;
		if (result != 0) {
			pr_warn("failed to push data\n");
		}
		asm("int $0x3B");  /* Corresponding to irq 11 */
		msleep(0);
	}
	return 0;
}

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size) {
		return 0;
	}

int32_t elliptic_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size) {
	return 0;
}


void elliptic_data_io_cancel(struct elliptic_data *elliptic_data)
{
	atomic_set(&elliptic_data->abort_io, 1);
	wake_up_interruptible(&elliptic_data->fifo_isr_not_empty);
}


int elliptic_data_io_initialize(void)
{
	pr_debug("%s\n", __func__);
	atomic_set(&cancel, 0);
	simulating_task = kthread_run(&simulating_thread, NULL,
									"el_simulating_thread");


	if (request_irq(USE_IRQ, irq_handler, IRQF_SHARED, "my_device",
				(void *)(irq_handler))) {
		pr_debug("my_device: cannot register IRQ ");
		return -EPERM;
	}

	return 0;
}

int elliptic_data_io_cleanup(void)
{
	free_irq(USE_IRQ, (void *)(irq_handler));
	kthread_stop(simulating_task);
	atomic_set(&cancel, 1);
	msleep(200);
	return 0;
}
