/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SND_SOC_RT5512_H
#define __SND_SOC_RT5512_H

/* Export function */
int rt5512_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
int rt5512_i2c_remove(struct i2c_client *client);

#endif /* __SND_SOC_RT5512_H */

