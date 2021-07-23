/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _HAL_TYPES_PUBLIC_H_
#define _HAL_TYPES_PUBLIC_H_

#include "val_types_public.h"

/**
 * @par Structure
 *  HAL_POWER_T
 * @par Description
 *  This is a parameter for power related function
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  eDriverType		[IN]     The driver type
 *  fgEnable		[IN]     Enable or not
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct _HAL_POWER_T {
	void *pvHandle;
	unsigned int u4HandleSize;
	enum VAL_DRIVER_TYPE_T eDriverType;
	char fgEnable;
	void *pvReserved;
	unsigned int u4ReservedSize;
};

/**
 * @par Structure
 *  HAL_ISR_T
 * @par Description
 *  This is a parameter for ISR related function
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  eDriverType		[IN]     The driver type
 *  fgRegister		[IN]     Register or un-register
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct HAL_ISR_T {
	void *pvHandle;
	unsigned int u4HandleSize;
	enum VAL_DRIVER_TYPE_T eDriverType;
	char fgRegister;
	void *pvReserved;
	unsigned int u4ReservedSize;
};

#endif /* #ifndef _HAL_TYPES_PUBLIC_H_ */
