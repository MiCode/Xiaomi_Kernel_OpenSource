// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_LOGGER_H__
#define __GPUEB_LOGGER_H__

#include <linux/poll.h>

#define ROUNDUP(a, b)       (((a) + ((b)-1)) & ~((b)-1))

struct log_ctrl_s {
    unsigned int base;
    unsigned int size;
    unsigned int enable;
    unsigned int info_ofs;
    unsigned int buff_ofs;
    unsigned int buff_size;
};

struct buffer_info_s {
    unsigned int r_pos;
    unsigned int w_pos;
};

struct gpueb_work_struct {
    struct work_struct work;
    unsigned int flags;
    unsigned int id;
};

int gpueb_logger_init(struct platform_device *pdev,
                    phys_addr_t start, phys_addr_t limit);

unsigned int gpueb_log_if_poll(struct file *file, poll_table *wait);
ssize_t gpueb_log_if_read(struct file *file,
        char __user *data, size_t len, loff_t *ppos);
int gpueb_log_if_open(struct inode *inode, struct file *file);


extern struct device_attribute dev_attr_gpueb_mobile_log;

#endif /* __GPUEB_LOGGER_H__ */