// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_log.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _WCN_LOG
#define _WCN_LOG

#define WCN_LOG_MAX_MINOR 2

struct mdbg_device_t {
	int			open_count;
	struct mutex		mdbg_lock;
	wait_queue_head_t	rxwait;
	struct wcnlog_dev *dev[WCN_LOG_MAX_MINOR];
	struct ring_device *ring_dev;
	bool exit_flag;
};

extern struct mdbg_device_t	*mdbg_dev;
extern wait_queue_head_t	mdbg_wait;
extern unsigned char flag_reset;
extern struct completion ge2_completion;

void wakeup_loopcheck_int(void);
int get_loopcheck_status(void);
void marlin_hold_cpu(void);
void wcnlog_clear_log(void);

int log_dev_init(void);
int log_dev_exit(void);
int wake_up_log_wait(void);
int log_cdev_exit(void);
int log_cdev_init(void);
#endif
