/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef TEE_WAIT_QUEUE_H
#define TEE_WAIT_QUEUE_H

#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/device.h>

struct tee_wait_queue_private {
	struct mutex mu;
	struct list_head db;
};

void tee_wait_queue_init(struct tee_wait_queue_private *priv);
void tee_wait_queue_exit(struct tee_wait_queue_private *priv);
void tee_wait_queue_sleep(struct device *dev,
			struct tee_wait_queue_private *priv, u32 key);
void tee_wait_queue_wakeup(struct device *dev,
			struct tee_wait_queue_private *priv, u32 key);

#endif /*TEE_WAIT_QUEUE_H*/
