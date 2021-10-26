/*
 * cs35l41_ext.h -- Export probe api to MTK speaker framework
 *
 * Copyright 2018 Cirrus Logic, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/i2c.h>

int cs35l41_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
int cs35l41_i2c_remove(struct i2c_client *client);
