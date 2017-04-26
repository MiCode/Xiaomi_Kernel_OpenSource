/*
 * Ti tusb320  driver IC for type C
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

#ifndef _TI_TUSB320_H_
#define _TI_TUSB320_H_

/*  ti id
  *  1: Device mode
  *  2: Host mode
  *  3: DRP mode
  */
enum ti_role_mode{
	DEVICE_MODE = 1,
	HOST_MODE,
	DRP_MODE
};

enum ti_drp_mode{
	NORMAL_DRP_MODE,
	TRYSNK_DRP_MODE,
	REVERSE_MODE,
	TRYSRC_DRP_MODE
};


#define TI_I2C_RETRY_TIMES 3

#define TI_STATUS_REG  0X09
#define TI_ROLE_MODE_REG 0x0A
#define TI_DISABLE_RD_RP_REG 0x45
#define TI_DEVICE_ID_REG  0x00

#define TI_ROLE_MODE_MASK  0x30
#define TI_ROLE_OFFSET 4
#define TI_DRP_ROLE_MODE_MASK  0x6
#define TI_DRP_ROLE_OFFSET 1
#define TI_SET_DISABLE_RD_RP 0x4
#define TI_CLR_DISABLE_RD_RP 0x0
#define TI_SOFT_RESET 0x8
#define TI_MAX_DUTY_CYCLE 0x06
#define TI_CLEAR_INT  0x10
#define TI_DISABLE_TERM_VAL 0x1

#define TI_TUSB320L_ADDR 0x47
#define TI_TUSB320L_ADDR_1 0x67

#endif
