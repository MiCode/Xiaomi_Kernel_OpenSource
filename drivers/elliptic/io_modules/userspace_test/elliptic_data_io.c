/**
 * Copyright Elliptic Labs
 * Copyright (C) 2017 XiaoMi, Inc.
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

static struct task_struct *simulating_task;
static atomic_t cancel;

struct elliptic_data_io_state {
};

struct data_packet_header {
	int32_t  t1;
	union {
		uint32_t t2;
		uint8_t  t2s[4];
	};
	union {
		uint32_t s1;
		uint8_t  s1s[4];
	};
};


#define APR_TEST_SIZE 460
struct elliptic_message {
	struct data_packet_header header;
	uint8_t data[APR_TEST_SIZE - (sizeof(struct data_packet_header))];
};

static void fill_buffer(int32_t *buffer, size_t len, int32_t value)
{
	size_t i;

	for (i = 0; i < len; ++i)
		buffer[i] = value;
}


static struct elliptic_message output_message;
int simulating_thread(void *context)
{
	static int32_t count;
	int result;

	count = 0;
	msleep(20);

	pr_debug("%s\n", __func__);
	output_message.header.t2s[0] = (1<<4) | 3;
	output_message.header.s1s[0] = 100;

	while (atomic_read(&cancel) == 0) {
		if (kthread_should_stop())
			do_exit(0);

		output_message.header.t1 = count;


		fill_buffer((int32_t *)output_message.data, 100, count);
		result = elliptic_data_push((const char *)&output_message,
			APR_TEST_SIZE);

		++count;
		if (result != 0)
			pr_warn("failed to push data\n");

		msleep(20);
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
	wake_up_interruptible(&elliptic_data->fifo_usp_not_empty);
}


int elliptic_data_io_initialize(void)
{
	pr_debug("%s\n", __func__);
	atomic_set(&cancel, 0);
	simulating_task = kthread_run(&simulating_thread, NULL,
		"el_simulating_thread");
	return 0;
}

int elliptic_data_io_cleanup(void)
{
	kthread_stop(simulating_task);
	atomic_set(&cancel, 1);
	msleep(200);
	return 0;
}
