/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * Copyright (C) 2015 Google, Inc.
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

#ifndef _TRUSTY_LOG_H_
#define _TRUSTY_LOG_H_

#include <imsg_log.h>

#define TZ_LOG_SIZE           (PAGE_SIZE * 64)
#define TZ_LINE_BUFFER_SIZE   256

#define TZ_LOG_RATELIMIT_INTERVAL	(1 * HZ)
#define TZ_LOG_RATELIMIT_BURST		200

/*
 * Ring buffer that supports one secure producer thread and one
 * linux side consumer thread.
 */
struct log_rb {
	TZ_VOLATILE(uint32_t alloc);
	TZ_VOLATILE(uint32_t put);
	TZ_NON_VOLATILE(uint32_t sz);
	TZ_VOLATILE(char data[0]);
} __packed;

struct boot_log_rb {
	uint32_t get;
	uint32_t put;
	uint32_t sz;
	char data[0];
} __packed;

struct tz_log_state {
	struct device *dev;

	/*
	 * This lock is here to ensure only one consumer will read
	 * from the log ring buffer at a time.
	 */
	spinlock_t lock;
	struct log_rb *log;
	struct boot_log_rb *boot_log;
	uint32_t get;
	uint32_t read_get;

	struct page *log_pages;
	struct page *boot_log_pages;

	struct notifier_block call_notifier;
	struct notifier_block panic_notifier;
	char line_buffer[TZ_LINE_BUFFER_SIZE];
};

int tz_log_probe(struct platform_device *pdev);
int tz_log_remove(struct platform_device *pdev);
int tz_driver_read_logs(char *buffer, unsigned long count);
int teei_log_fn(void *work);
int init_tlog_comp_fn(void);
void teei_notify_log_fn(void);
int teei_change_log_status(unsigned long new_status);
#endif

