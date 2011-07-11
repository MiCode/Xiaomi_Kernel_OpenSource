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
#ifndef _ABE_FUNCTIONSID_H_
#define _ABE_FUNCTIONSID_H_
/*
 *    TASK function ID definitions
 */
#define C_ABE_FW_FUNCTION_IIR                               0
#define C_ABE_FW_FUNCTION_monoToStereoPack                  1
#define C_ABE_FW_FUNCTION_stereoToMonoSplit                 2
#define C_ABE_FW_FUNCTION_decimator                         3
#define C_ABE_FW_FUNCTION_OS0Fill                           4
#define C_ABE_FW_FUNCTION_mixer2                            5
#define C_ABE_FW_FUNCTION_mixer4                            6
#define C_ABE_FW_FUNCTION_mixer4_dual_mono                  7
#define C_ABE_FW_FUNCTION_inplaceGain                       8
#define C_ABE_FW_FUNCTION_StreamRouting                     9
#define C_ABE_FW_FUNCTION_gainConverge                      10
#define C_ABE_FW_FUNCTION_dualIir                           11
#define C_ABE_FW_FUNCTION_IO_DL_pp                          12
#define C_ABE_FW_FUNCTION_IO_generic                        13
#define C_ABE_FW_FUNCTION_irq_fifo_debug                    14
#define C_ABE_FW_FUNCTION_synchronize_pointers              15
#define C_ABE_FW_FUNCTION_VIBRA2                            16
#define C_ABE_FW_FUNCTION_VIBRA1                            17
#define C_ABE_FW_FUNCTION_IIR_SRC_MIC                       18
#define C_ABE_FW_FUNCTION_wrappers                          19
#define C_ABE_FW_FUNCTION_ASRC_DL_wrapper                   20
#define C_ABE_FW_FUNCTION_ASRC_UL_wrapper                   21
#define C_ABE_FW_FUNCTION_mem_init                          22
#define C_ABE_FW_FUNCTION_debug_vx_asrc                     23
#define C_ABE_FW_FUNCTION_IIR_SRC2                          24
#define C_ABE_FW_FUNCTION_ASRC_DL_wrapper_sibling           25
#define C_ABE_FW_FUNCTION_ASRC_UL_wrapper_sibling           26
#define C_ABE_FW_FUNCTION_FIR6                              27
#define C_ABE_FW_FUNCTION_SRC44P1                           28
#define C_ABE_FW_FUNCTION_SRC44P1_1211                      29
/*
 *    COPY function ID definitions
 */
#define NULL_COPY_CFPID                                     0
#define S2D_STEREO_16_16_CFPID                              1
#define S2D_MONO_MSB_CFPID                                  2
#define S2D_STEREO_MSB_CFPID                                3
#define S2D_STEREO_RSHIFTED_16_CFPID                        4
#define S2D_MONO_RSHIFTED_16_CFPID                          5
#define D2S_STEREO_16_16_CFPID                              6
#define D2S_MONO_MSB_CFPID                                  7
#define D2S_MONO_RSHIFTED_16_CFPID                          8
#define D2S_STEREO_RSHIFTED_16_CFPID                        9
#define D2S_STEREO_MSB_CFPID                                10
#define COPY_DMIC_CFPID                                     11
#define COPY_MCPDM_DL_CFPID                                 12
#define COPY_MM_UL_CFPID                                    13
#define SPLIT_SMEM_CFPID                                    14
#define MERGE_SMEM_CFPID                                    15
#define SPLIT_TDM_CFPID                                     16
#define MERGE_TDM_CFPID                                     17
#define ROUTE_MM_UL_CFPID                                   18
#define IO_IP_CFPID                                         19
#define COPY_UNDERFLOW_CFPID                                20
#endif /* _ABE_FUNCTIONSID_H_ */
