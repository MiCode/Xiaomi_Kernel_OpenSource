/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#ifndef __APR_TAL_H_
#define __APR_TAL_H_

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

/* APR Client IDs */
#define APR_CLIENT_AUDIO	0x0
#define APR_CLIENT_VOICE	0x1
#define APR_CLIENT_MAX	0x2

#define APR_DL_SMD    0
#define APR_DL_MAX    1

#define APR_DEST_MODEM 0
#define APR_DEST_QDSP6 1
#define APR_DEST_MAX   2

#define APR_MAX_BUF   8192

#define APR_OPEN_TIMEOUT_MS 5000

typedef void (*apr_svc_cb_fn)(void *buf, int len, void *priv);
struct apr_svc_ch_dev *apr_tal_open(uint32_t svc, uint32_t dest,
			uint32_t dl, apr_svc_cb_fn func, void *priv);
int apr_tal_write(struct apr_svc_ch_dev *apr_ch, void *data, int len);
int apr_tal_close(struct apr_svc_ch_dev *apr_ch);
struct apr_svc_ch_dev {
	struct smd_channel *ch;
	spinlock_t         lock;
	spinlock_t         w_lock;
	struct mutex       m_lock;
	apr_svc_cb_fn      func;
	char               data[APR_MAX_BUF];
	wait_queue_head_t  wait;
	void               *priv;
	uint32_t           smd_state;
	wait_queue_head_t  dest;
	uint32_t           dest_state;
};

#endif
