/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_MSGQ_H
#define __HH_MSGQ_H

#include <linux/types.h>
#include <linux/platform_device.h>

#include "hh_common.h"

enum hh_msgq_label {
	HH_MSGQ_LABEL_RM,
	HH_MSGQ_LABEL_MEMBUF,
	HH_MSGQ_LABEL_DISPLAY,
	HH_MSGQ_LABEL_MAX
};

#define HH_MSGQ_MAX_MSG_SIZE_BYTES 240

#define HH_MSGQ_DIRECTION_TX	0
#define HH_MSGQ_DIRECTION_RX	1

/* Possible flags to pass for Tx or Rx */
#define HH_MSGQ_TX_PUSH		BIT(0)
#define HH_MSGQ_NONBLOCK	BIT(32)

#if IS_ENABLED(CONFIG_HH_MSGQ)
void *hh_msgq_register(enum hh_msgq_label label);
int hh_msgq_unregister(void *msgq_client_desc);
int hh_msgq_send(void *msgq_client_desc,
			void *buff, size_t size, unsigned long flags);
int hh_msgq_recv(void *msgq_client_desc,
			void *buff, size_t buff_size,
			size_t *recv_size, unsigned long flags);

int hh_msgq_populate_cap_info(enum hh_msgq_label label, u64 cap_id,
				int direction, int irq);
int hh_msgq_probe(struct platform_device *pdev, enum hh_msgq_label label);
#else
static inline void *hh_msgq_register(enum hh_msgq_label label)
{
	return ERR_PTR(-ENODEV);
}

static inline int hh_msgq_unregister(void *msgq_client_desc)
{
	return -EINVAL;
}

static inline int hh_msgq_send(void *msgq_client_desc,
			void *buff, size_t size, unsigned long flags)
{
	return -EINVAL;
}

static inline int hh_msgq_recv(void *msgq_client_desc,
			void *buff, size_t buff_size,
			size_t *recv_size, unsigned long flags)
{
	return -EINVAL;
}

static inline int hh_msgq_populate_cap_info(enum hh_msgq_label label,
					    u64 cap_id,
					    int direction,
					    int irq)
{
	return -EINVAL;
}

static inline int hh_msgq_probe(struct platform_device *pdev,
				enum hh_msgq_label label)
{
	return -ENODEV;
}
#endif
#endif
