/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _ABE_FW_H_
#define _ABE_FW_H_

#include "abe_cm_addr.h"
#include "abe_sm_addr.h"
#include "abe_dm_addr.h"
#include "abe_typedef.h"
/*
 * GLOBAL DEFINITION
 */
/* one scheduler loop = 4kHz = 12 samples at 48kHz */
#define FW_SCHED_LOOP_FREQ	4000
/* one scheduler loop = 4kHz = 12 samples at 48kHz */
#define FW_SCHED_LOOP_FREQ_DIV1000	(FW_SCHED_LOOP_FREQ/1000)
#define EVENT_FREQUENCY 96000
#define SLOTS_IN_SCHED_LOOP (96000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_8kHz (8000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_16kHz (16000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_24kHz (24000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_48kHz (48000/FW_SCHED_LOOP_FREQ)
#define TASKS_IN_SLOT 8
/*
 * DMEM AREA - SCHEDULER
 */
#define dmem_mm_trace	OMAP_ABE_D_DEBUG_FIFO_ADDR
#define dmem_mm_trace_size ((OMAP_ABE_D_DEBUG_FIFO_SIZE)/4)
#define ATC_SIZE 8		/* 8 bytes per descriptors */
struct omap_abe_atc_desc {
	unsigned rdpt:7;	/* first 32bits word of the descriptor */
	unsigned reserved0:1;
	unsigned cbsize:7;
	unsigned irqdest:1;
	unsigned cberr:1;
	unsigned reserved1:5;
	unsigned cbdir:1;
	unsigned nw:1;
	unsigned wrpt:7;
	unsigned reserved2:1;
	unsigned badd:12;	/* second 32bits word of the descriptor */
	unsigned iter:7;	/* iteration field overlaps 16-bit boundary */
	unsigned srcid:6;
	unsigned destid:6;
	unsigned desen:1;
};
/*
 * Infinite counter incremented on each sheduler periods (~250 us)
 * uint16 dmem_debug_time_stamp
 */
#define dmem_debug_time_stamp	OMAP_ABE_D_LOOPCOUNTER_ADDR
/*
 * ATC BUFFERS + IO TASKS SMEM buffers
 */
#define dmem_dmic OMAP_ABE_D_DMIC_UL_FIFO_ADDR
#define dmem_dmic_size (OMAP_ABE_D_DMIC_UL_FIFO_SIZE/4)
#define dmem_amic OMAP_ABE_D_MCPDM_UL_FIFO_ADDR
#define dmem_amic_size (OMAP_ABE_D_MCPDM_UL_FIFO_SIZE/4)
#define smem_amic	AMIC_96_labelID
#define dmem_mcpdm OMAP_ABE_D_MCPDM_DL_FIFO_ADDR
#define dmem_mcpdm_size (OMAP_ABE_D_MCPDM_DL_FIFO_SIZE/4)
#define dmem_mm_ul OMAP_ABE_D_MM_UL_FIFO_ADDR
#define dmem_mm_ul_size (OMAP_ABE_D_MM_UL_FIFO_SIZE/4)
/* managed directly by the router */
#define smem_mm_ul MM_UL_labelID
#define dmem_mm_ul2 OMAP_ABE_D_MM_UL2_FIFO_ADDR
#define dmem_mm_ul2_size (OMAP_ABE_D_MM_UL2_FIFO_SIZE/4)
/* managed directly by the router */
#define smem_mm_ul2 MM_UL2_labelID
#define dmem_mm_dl OMAP_ABE_D_MM_DL_FIFO_ADDR
#define dmem_mm_dl_size (OMAP_ABE_D_MM_DL_FIFO_SIZE/4)
#define smem_mm_dl MM_DL_labelID
#define dmem_vx_dl OMAP_ABE_D_VX_DL_FIFO_ADDR
#define dmem_vx_dl_size (OMAP_ABE_D_VX_DL_FIFO_SIZE/4)
#define smem_vx_dl	IO_VX_DL_ASRC_labelID	/* Voice_16k_DL_labelID */
#define dmem_vx_ul OMAP_ABE_D_VX_UL_FIFO_ADDR
#define dmem_vx_ul_size (OMAP_ABE_D_VX_UL_FIFO_SIZE/4)
#define smem_vx_ul Voice_8k_UL_labelID
#define dmem_tones_dl OMAP_ABE_D_TONES_DL_FIFO_ADDR
#define dmem_tones_dl_size (OMAP_ABE_D_TONES_DL_FIFO_SIZE/4)
#define smem_tones_dl Tones_labelID
#define dmem_vib_dl OMAP_ABE_D_VIB_DL_FIFO_ADDR
#define dmem_vib_dl_size (OMAP_ABE_D_VIB_DL_FIFO_SIZE/4)
#define smem_vib IO_VIBRA_DL_labelID
#define dmem_mm_ext_out OMAP_ABE_D_MM_EXT_OUT_FIFO_ADDR
#define dmem_mm_ext_out_size (OMAP_ABE_D_MM_EXT_OUT_FIFO_SIZE/4)
#define smem_mm_ext_out DL1_GAIN_out_labelID
#define dmem_mm_ext_in OMAP_ABE_D_MM_EXT_IN_FIFO_ADDR
#define dmem_mm_ext_in_size (OMAP_ABE_D_MM_EXT_IN_FIFO_SIZE/4)
/*IO_MM_EXT_IN_ASRC_labelID	 ASRC input buffer, size 40 */
#define smem_mm_ext_in_opp100 IO_MM_EXT_IN_ASRC_labelID
/* at OPP 50 without ASRC */
#define smem_mm_ext_in_opp50 MM_EXT_IN_labelID
#define dmem_bt_vx_dl OMAP_ABE_D_BT_DL_FIFO_ADDR
#define dmem_bt_vx_dl_size (OMAP_ABE_D_BT_DL_FIFO_SIZE/4)
#define smem_bt_vx_dl_opp50 BT_DL_8k_labelID
/*BT_DL_8k_opp100_labelID  ASRC output buffer, size 40 */
#define smem_bt_vx_dl_opp100 BT_DL_8k_opp100_labelID
#define dmem_bt_vx_ul OMAP_ABE_D_BT_UL_FIFO_ADDR
#define dmem_bt_vx_ul_size (OMAP_ABE_D_BT_UL_FIFO_SIZE/4)
#define smem_bt_vx_ul_opp50 BT_UL_8k_labelID
/*IO_BT_UL_ASRC_labelID	 ASRC input buffer, size 40 */
#define smem_bt_vx_ul_opp100 IO_BT_UL_ASRC_labelID
/*
 * SMEM AREA
 */
/*
 * GAIN SMEM on PORT
 * int32 smem_G0 [18] : desired gain on the ports
 * format of G0 = 6 bits left shifted desired gain in linear 24bits format
 * int24 stereo G0 [18] = G0
 * int24 stereo GI [18] current value of the gain in the same format of G0
 * List of smoothed gains :
 * 6 DMIC 0 1 2 3 4 5
 * 2 AMIC L R
 * 4 PORT1/2_RX L R
 * 2 MM_EXT L R
 * 2 MM_VX_DL L R
 * 2 IHF L R
 * ---------------
 * 18 = TOTAL
 */
/*
 * COEFFICIENTS AREA
 */
/*
 * delay coefficients used in the IIR-1 filters
 * int24 cmem_gain_delay_iir1[9 x 2] (a, (1-a))
 *
 * 3 for 6 DMIC 0 1 2 3 4 5
 * 1 for 2 AMIC L R
 * 2 for 4 PORT1/2_RX L R
 * 1 for 2 MM_EXT L R
 * 1 for 2 MM_VX_DL L R
 * 1 for 2 IHF L R
 */
/*
 * gain controls
 */
#define GAIN_LEFT_OFFSET 0
#define GAIN_RIGHT_OFFSET 1
/* stereo gains */
#define dmic1_gains_offset 0
#define dmic2_gains_offset 2
#define dmic3_gains_offset 4
#define amic_gains_offset 6
#define dl1_gains_offset 8
#define dl2_gains_offset 10
#define splitters_gains_offset 12
#define mixer_dl1_offset 14
#define mixer_dl2_offset 18
#define mixer_echo_offset 22
#define mixer_sdt_offset 24
#define mixer_vxrec_offset 26
#define mixer_audul_offset 30
#define btul_gains_offset 34

#endif/* _ABE_FW_H_ */
