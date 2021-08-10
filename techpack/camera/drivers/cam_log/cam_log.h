/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _CAM_LOG_H_
#define _CAM_LOG_H_

#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

struct camlog_dev {
    dev_t devt;
    uint8_t m_camlog_message[1024];
    struct mutex camlog_message_lock;
    wait_queue_head_t camlog_is_not_empty;
};
void camlog_send_message(void);

#endif /* _CAM_LOG_H_ */
