/*
 *  vl53l0x_types.h - Linux kernel modules for
 *  STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef VL_TYPES_H_
#define VL_TYPES_H_

#include <linux/types.h>

#ifndef NULL
#error "TODO review  NULL definition or add required include "
#define NULL 0
#endif

#if !defined(STDINT_H) &&  !defined(_GCC_STDINT_H) \
	&& !defined(_STDINT_H) && !defined(_LINUX_TYPES_H)

#pragma message(
"Review type definition of STDINT define for your platform and add to above")

/*
 *  target platform do not provide stdint or use a different #define than above
 *  to avoid seeing the message below addapt the #define list above or implement
 *  all type and delete these pragma
 */

unsigned int uint32_t;
int int32_t;

unsigned short uint16_t;
short int16_t;

unsigned char uint8_t;

signed char int8_t;


#endif /* _STDINT_H */

#endif /* VL_TYPES_H_ */
