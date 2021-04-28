/*
 * Copyright (C) 2015 Google, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#ifndef _MITEE_MEMLOG_H_
#define _MITEE_MEMLOG_H_

#include <linux/notifier.h>
#include <linux/log2.h>
#include <asm/page.h>

#define MITEE_MEMLOG_SIZE (PAGE_SIZE * 64)
#define MITEE_LINE_BUFFER_SIZE 256
#define KEY_LENGTH 345
#define RESERVED_LENGTH 256
/*
 * Ring buffer that supports one secure producer thread and one
 * linux side consumer thread.
 */
struct log_rb {
	volatile uint32_t alloc;//tee runtime log
	volatile uint32_t put;
	uint32_t sz;
	volatile uint32_t b_alloc;//tee bootup log
	volatile uint32_t b_put;
	uint32_t b_sz;
	volatile char old_aeskey[KEY_LENGTH];
	volatile char aeskey[KEY_LENGTH];
	volatile char reserved[RESERVED_LENGTH];
	bool rt_log_init;//if runtime log
	volatile char data[0];
} __packed;

struct mitee_memlog_state {
	struct device *dev;
	struct device *mitee_dev;
	struct proc_dir_entry *proc;

	/*
	 * This lock is here to ensure only one consumer will read
	 * from the log ring buffer at a time.
	 */
	spinlock_t lock;
	struct log_rb *log;
	uint32_t get;
	uint32_t b_get;

	struct page *log_pages;

	wait_queue_head_t mitee_log_wq;
	atomic_t mitee_log_event_count;
	atomic_t readable;
	int poll_event;

	struct notifier_block call_notifier;
	struct notifier_block panic_notifier;
	char line_buffer[MITEE_LINE_BUFFER_SIZE];
};

int mitee_memlog_probe(struct platform_device *pdev);
int mitee_memlog_remove(struct platform_device *pdev);
#endif

