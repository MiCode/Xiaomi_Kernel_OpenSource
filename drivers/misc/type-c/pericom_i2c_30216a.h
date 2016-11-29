/*
 * Pericom 30216a  driver IC for type C
 *
 * Copyright (C) 2015 xiaomi Incorporated
 *
 * Copyright (C) 2015 fengwei <fengwei@xiaomi.com	>
 * Copyright (c) 2015-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _PERICOM_30216A_H_
#define _PERICOM_30216A_H_

/*  pericom id
  *  0: Device mode
  *  1: Host mode
  *  2: DRP mode
  */
enum pericom_role_mode{
	DEVICE_MODE,
	HOST_MODE,
	DRP_MODE
};

enum pericom_power_mode{
	ACTIVE_MODE,
	POWERSAVING_MODE
};

#define PERICOM_I2C_RETRY_TIMES 5

#define PERICOM_POWER_SAVING_MASK 0x80
#define PERICOM_POWER_SAVING_OFFSET 7
#define PERICOM_ROLE_MODE_MASK  0x06
#define PERICOM_INTERRUPT_MASK 0x01
#define PERICOM_INTERRUPT_UNMASK 0
#define PERICOM_ROLE_OFFSET 1

#endif
