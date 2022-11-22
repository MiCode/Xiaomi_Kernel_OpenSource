/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __N3D_IF_H__
#define __N3D_IF_H__

int __init n3d_init(void);
void __exit n3d_exit(void);

void set_sensor_streaming_state(int sensor_idx, int state);

#endif
