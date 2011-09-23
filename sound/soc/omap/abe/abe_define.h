/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _ABE_DEFINE_H_
#define _ABE_DEFINE_H_

#define ATC_DESCRIPTOR_NUMBER                               64
#define PROCESSING_SLOTS                                    25
#define TASK_POOL_LENGTH                                    136
#define MCU_IRQ                                            0x24
#define MCU_IRQ_SHIFT2                                     0x90
#define DMA_REQ_SHIFT2                                     0x210
#define DSP_IRQ                                            0x4c
#define IRQtag_APS                                         0x000a
#define IRQtag_COUNT                                       0x000c
#define IRQtag_PP                                          0x000d
#define DMAreq_7                                           0x0080
#define IRQ_FIFO_LENGTH                                     16
#define SDT_EQ_ORDER                                        4
#define DL_EQ_ORDER                                         12
#define MIC_FILTER_ORDER                                    4
#define GAINS_WITH_RAMP1                                    14
#define GAINS_WITH_RAMP2                                    22
#define GAINS_WITH_RAMP_TOTAL                               36
#define ASRC_MEMLENGTH                                      40
#define ASRC_UL_VX_FIR_L                                    19
#define ASRC_DL_VX_FIR_L                                    19
#define ASRC_MM_EXT_IN_FIR_L                                18
#define ASRC_margin                                         2
#define ASRC_N_8k                                           2
#define ASRC_N_16k                                          4
#define ASRC_N_48k                                          12
#define VIBRA_N                                             5
#define VIBRA1_IIR_MEMSIZE                                  11
#define SAMP_LOOP_96K                                       24
#define SAMP_LOOP_48K                                       12
#define SAMP_LOOP_48KM1                                     11
#define SAMP_LOOP_48KM2                                     10
#define SAMP_LOOP_16K                                       4
#define SAMP_LOOP_8K                                        2
#define INPUT_SCALE_SHIFTM2                                 5156
#define SATURATION                                          8420
#define SATURATION_7FFF                                     8416
#define OUTPUT_SCALE_SHIFTM2                                5160
#define NTAPS_SRC_44P1                                      24
#define NTAPS_SRC_44P1_M4                                   96
#define NTAPS_SRC_44P1_THR                                  48
#define NTAPS_SRC_44P1_THRM4                                192
#define DRIFT_COUNTER_44P1M1                                443
#define NB_OF_PHASES_SRC44P1                                12
#define NB_OF_PHASES_SRC44P1M1                              11
#define SRC44P1_BUFFER_SIZE                                 96
#define SRC44P1_BUFFER_SIZE_M4                              384
#define SRC44P1_INIT_RPTR                                   60
#define MUTE_SCALING                                        5164
#define ABE_PMEM                                            1
#define ABE_CMEM                                            2
#define ABE_SMEM                                            3
#define ABE_DMEM                                            4
#define ABE_ATC                                             5
#define ASRC_BT_UL_FIR_L                                    19
#define ASRC_BT_DL_FIR_L                                    19
#define SRC44P1_COEF_ADDR                                   1466
#define NTAPS_P_SRC_44P1_M4                                 144

#endif /* _ABE_DEFINE_H_ */
