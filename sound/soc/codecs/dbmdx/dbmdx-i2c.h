/*
 * dbmdx-i2c.h  --  DBMDX I2C interface common functions
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_I2C_COMMON_H
#define _DBMDX_I2C_COMMON_H

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#endif

#define RETRY_COUNT				5

struct dbmdx_i2c_private;

struct dbmdx_i2c_data {
	u32	boot_addr;
	u32	operation_addr;
	u32	read_chunk_size;
	u32	write_chunk_size;
	u8	read_buf[MAX_REQ_SIZE];
};


struct dbmdx_i2c_private {
	struct device			*dev;
	struct dbmdx_i2c_data		*pdata;
	struct i2c_client		*client;
	struct chip_interface		chip;
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source		ps_nosuspend_wl;
#endif
	u32				interface_enabled;
};

ssize_t write_i2c_data(struct dbmdx_private *p, const void *buf,
			      size_t len);
ssize_t read_i2c_data(struct dbmdx_private *p, void *buf, size_t len);
ssize_t send_i2c_cmd_va(struct dbmdx_private *p, u32 command,
				  u16 *response);
ssize_t send_i2c_cmd_vqe(struct dbmdx_private *p,
	u32 command, u16 *response);

int send_i2c_cmd_boot(struct dbmdx_private *p, u32 command);
int i2c_verify_boot_checksum(struct dbmdx_private *p,
	const void *checksum, size_t chksum_len);
int i2c_verify_chip_id(struct dbmdx_private *p);
int i2c_common_probe(struct i2c_client *client,
		const struct i2c_device_id *id);

int i2c_common_remove(struct i2c_client *client);
void i2c_interface_resume(struct dbmdx_i2c_private *i2c_p);
void i2c_interface_suspend(struct dbmdx_i2c_private *i2c_p);


#endif
