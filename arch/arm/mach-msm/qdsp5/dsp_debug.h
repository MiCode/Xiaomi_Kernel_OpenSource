/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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

#define RAMA_BASE MSM_AD5_BASE
#define RAMB_BASE ((RAMA_BASE) + (0x200000))
#define RAMC_BASE ((RAMB_BASE) + (0x200000))
#define DSP_RAM_SIZE 0x40000

#endif
