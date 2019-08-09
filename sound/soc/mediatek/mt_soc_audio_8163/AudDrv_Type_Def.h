/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef AUDIO_TYPE_DEF_H
#define AUDIO_TYPE_DEF_H
#include <linux/types.h>

/* Type re-definition */

#ifndef uint8
typedef uint8_t uint8;
#endif

#ifndef uint16
typedef u_int16_t uint16;
#endif

#ifndef int32
typedef int32_t int32;
#endif

#ifndef uint32
typedef u_int32_t uint32;
#endif

#endif


