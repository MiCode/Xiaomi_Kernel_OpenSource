/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __DSP_DEBUG_H_
#define __DSP_DEBUG_H_

typedef int (*dsp_state_cb)(int state);
int dsp_debug_register(dsp_state_cb ptr);

#define DSP_STATE_CRASHED         0x0
#define DSP_STATE_CRASH_DUMP_DONE 0x1

#endif
