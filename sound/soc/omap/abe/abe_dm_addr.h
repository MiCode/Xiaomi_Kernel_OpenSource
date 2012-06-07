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
#define OMAP_ABE_D_ATCDESCRIPTORS_ADDR                     0x0
#define OMAP_ABE_D_ATCDESCRIPTORS_SIZE                     0x200
#define OMAP_ABE_STACK_ADDR                                0x200
#define OMAP_ABE_STACK_SIZE                                0x70
#define OMAP_ABE_D_VERSION_ADDR                            0x270
#define OMAP_ABE_D_VERSION_SIZE                            0x4
#define OMAP_ABE_D_IODESCR_ADDR                            0x274
#define OMAP_ABE_D_IODESCR_SIZE                            0x280
#define OMAP_ABE_D_ZERO_ADDR                               0x4F4
#define OMAP_ABE_D_ZERO_SIZE                               0x4
#define OMAP_ABE_DBG_TRACE1_ADDR                           0x4F8
#define OMAP_ABE_DBG_TRACE1_SIZE                           0x1
#define OMAP_ABE_DBG_TRACE2_ADDR                           0x4F9
#define OMAP_ABE_DBG_TRACE2_SIZE                           0x1
#define OMAP_ABE_DBG_TRACE3_ADDR                           0x4FA
#define OMAP_ABE_DBG_TRACE3_SIZE                           0x1
#define OMAP_ABE_D_MULTIFRAME_ADDR                         0x4FC
#define OMAP_ABE_D_MULTIFRAME_SIZE                         0x190
#define OMAP_ABE_D_IDLETASK_ADDR                           0x68C
#define OMAP_ABE_D_IDLETASK_SIZE                           0x2
#define OMAP_ABE_D_TYPELENGTHCHECK_ADDR                    0x68E
#define OMAP_ABE_D_TYPELENGTHCHECK_SIZE                    0x2
#define OMAP_ABE_D_MAXTASKBYTESINSLOT_ADDR                 0x690
#define OMAP_ABE_D_MAXTASKBYTESINSLOT_SIZE                 0x2
#define OMAP_ABE_D_REWINDTASKBYTES_ADDR                    0x692
#define OMAP_ABE_D_REWINDTASKBYTES_SIZE                    0x2
#define OMAP_ABE_D_PCURRENTTASK_ADDR                       0x694
#define OMAP_ABE_D_PCURRENTTASK_SIZE                       0x2
#define OMAP_ABE_D_PFASTLOOPBACK_ADDR                      0x696
#define OMAP_ABE_D_PFASTLOOPBACK_SIZE                      0x2
#define OMAP_ABE_D_PNEXTFASTLOOPBACK_ADDR                  0x698
#define OMAP_ABE_D_PNEXTFASTLOOPBACK_SIZE                  0x4
#define OMAP_ABE_D_PPCURRENTTASK_ADDR                      0x69C
#define OMAP_ABE_D_PPCURRENTTASK_SIZE                      0x2
#define OMAP_ABE_D_SLOTCOUNTER_ADDR                        0x6A0
#define OMAP_ABE_D_SLOTCOUNTER_SIZE                        0x2
#define OMAP_ABE_D_LOOPCOUNTER_ADDR                        0x6A4
#define OMAP_ABE_D_LOOPCOUNTER_SIZE                        0x4
#define OMAP_ABE_D_REWINDFLAG_ADDR                         0x6A8
#define OMAP_ABE_D_REWINDFLAG_SIZE                         0x2
#define OMAP_ABE_D_SLOT23_CTRL_ADDR                        0x6AC
#define OMAP_ABE_D_SLOT23_CTRL_SIZE                        0x4
#define OMAP_ABE_D_MCUIRQFIFO_ADDR                         0x6B0
#define OMAP_ABE_D_MCUIRQFIFO_SIZE                         0x40
#define OMAP_ABE_D_PINGPONGDESC_ADDR                       0x6F0
#define OMAP_ABE_D_PINGPONGDESC_SIZE                       0x18
#define OMAP_ABE_D_PP_MCU_IRQ_ADDR                         0x708
#define OMAP_ABE_D_PP_MCU_IRQ_SIZE                         0x2
#define OMAP_ABE_D_SRC44P1_MMDL_STRUCT_ADDR                0x70C
#define OMAP_ABE_D_SRC44P1_MMDL_STRUCT_SIZE                0x12
#define OMAP_ABE_D_SRC44P1_TONES_STRUCT_ADDR               0x720
#define OMAP_ABE_D_SRC44P1_TONES_STRUCT_SIZE               0x12
#define OMAP_ABE_D_CTRLPORTFIFO_ADDR                       0x740
#define OMAP_ABE_D_CTRLPORTFIFO_SIZE                       0x10
#define OMAP_ABE_D_IDLE_STATE_ADDR                         0x750
#define OMAP_ABE_D_IDLE_STATE_SIZE                         0x4
#define OMAP_ABE_D_STOP_REQUEST_ADDR                       0x754
#define OMAP_ABE_D_STOP_REQUEST_SIZE                       0x4
#define OMAP_ABE_D_REF0_ADDR                               0x758
#define OMAP_ABE_D_REF0_SIZE                               0x2
#define OMAP_ABE_D_DEBUGREGISTER_ADDR                      0x75C
#define OMAP_ABE_D_DEBUGREGISTER_SIZE                      0x8C
#define OMAP_ABE_D_GCOUNT_ADDR                             0x7E8
#define OMAP_ABE_D_GCOUNT_SIZE                             0x2
#define OMAP_ABE_D_FASTCOUNTER_ADDR                        0x7EC
#define OMAP_ABE_D_FASTCOUNTER_SIZE                        0x4
#define OMAP_ABE_D_SLOWCOUNTER_ADDR                        0x7F0
#define OMAP_ABE_D_SLOWCOUNTER_SIZE                        0x4
#define OMAP_ABE_D_AUPLINKROUTING_ADDR                     0x7F4
#define OMAP_ABE_D_AUPLINKROUTING_SIZE                     0x20
#define OMAP_ABE_D_VIRTAUDIOLOOP_ADDR                      0x814
#define OMAP_ABE_D_VIRTAUDIOLOOP_SIZE                      0x4
#define OMAP_ABE_D_ASRCVARS_DL_VX_ADDR                     0x818
#define OMAP_ABE_D_ASRCVARS_DL_VX_SIZE                     0x20
#define OMAP_ABE_D_ASRCVARS_UL_VX_ADDR                     0x838
#define OMAP_ABE_D_ASRCVARS_UL_VX_SIZE                     0x20
#define OMAP_ABE_D_COEFADDRESSES_VX_ADDR                   0x858
#define OMAP_ABE_D_COEFADDRESSES_VX_SIZE                   0x20
#define OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR                 0x878
#define OMAP_ABE_D_ASRCVARS_MM_EXT_IN_SIZE                 0x20
#define OMAP_ABE_D_COEFADDRESSES_MM_ADDR                   0x898
#define OMAP_ABE_D_COEFADDRESSES_MM_SIZE                   0x20
#define OMAP_ABE_D_TRACEBUFADR_ADDR                        0x8B8
#define OMAP_ABE_D_TRACEBUFADR_SIZE                        0x2
#define OMAP_ABE_D_TRACEBUFOFFSET_ADDR                     0x8BA
#define OMAP_ABE_D_TRACEBUFOFFSET_SIZE                     0x2
#define OMAP_ABE_D_TRACEBUFLENGTH_ADDR                     0x8BC
#define OMAP_ABE_D_TRACEBUFLENGTH_SIZE                     0x2
#define OMAP_ABE_D_PEMPTY_ADDR                             0x8C0
#define OMAP_ABE_D_PEMPTY_SIZE                             0x54
#define OMAP_ABE_D_ECHO_REF_48_16_WRAP_ADDR                0x914
#define OMAP_ABE_D_ECHO_REF_48_16_WRAP_SIZE                0x8
#define OMAP_ABE_D_ECHO_REF_48_8_WRAP_ADDR                 0x91C
#define OMAP_ABE_D_ECHO_REF_48_8_WRAP_SIZE                 0x8
#define OMAP_ABE_D_BT_UL_16_48_WRAP_ADDR                   0x924
#define OMAP_ABE_D_BT_UL_16_48_WRAP_SIZE                   0x8
#define OMAP_ABE_D_BT_UL_8_48_WRAP_ADDR                    0x92C
#define OMAP_ABE_D_BT_UL_8_48_WRAP_SIZE                    0x8
#define OMAP_ABE_D_BT_DL_48_16_WRAP_ADDR                   0x934
#define OMAP_ABE_D_BT_DL_48_16_WRAP_SIZE                   0x8
#define OMAP_ABE_D_BT_DL_48_8_WRAP_ADDR                    0x93C
#define OMAP_ABE_D_BT_DL_48_8_WRAP_SIZE                    0x8
#define OMAP_ABE_D_VX_DL_16_48_WRAP_ADDR                   0x944
#define OMAP_ABE_D_VX_DL_16_48_WRAP_SIZE                   0x8
#define OMAP_ABE_D_VX_DL_8_48_WRAP_ADDR                    0x94C
#define OMAP_ABE_D_VX_DL_8_48_WRAP_SIZE                    0x8
#define OMAP_ABE_D_VX_UL_48_16_WRAP_ADDR                   0x954
#define OMAP_ABE_D_VX_UL_48_16_WRAP_SIZE                   0x8
#define OMAP_ABE_D_VX_UL_48_8_WRAP_ADDR                    0x95C
#define OMAP_ABE_D_VX_UL_48_8_WRAP_SIZE                    0x8
#define OMAP_ABE_D_ASRCVARS_BT_UL_ADDR                     0x964
#define OMAP_ABE_D_ASRCVARS_BT_UL_SIZE                     0x20
#define OMAP_ABE_D_ASRCVARS_BT_DL_ADDR                     0x984
#define OMAP_ABE_D_ASRCVARS_BT_DL_SIZE                     0x20
#define OMAP_ABE_D_BT_DL_48_8_OPP100_WRAP_ADDR             0x9A4
#define OMAP_ABE_D_BT_DL_48_8_OPP100_WRAP_SIZE             0x8
#define OMAP_ABE_D_BT_DL_48_16_OPP100_WRAP_ADDR            0x9AC
#define OMAP_ABE_D_BT_DL_48_16_OPP100_WRAP_SIZE            0x8
#define OMAP_ABE_D_VX_DL_8_48_FIR_WRAP_ADDR                0x9B4
#define OMAP_ABE_D_VX_DL_8_48_FIR_WRAP_SIZE                0x8
#define OMAP_ABE_D_BT_UL_8_48_FIR_WRAP_ADDR                0x9BC
#define OMAP_ABE_D_BT_UL_8_48_FIR_WRAP_SIZE                0x8
#define OMAP_ABE_D_TASKSLIST_ADDR                          0x9C4
#define OMAP_ABE_D_TASKSLIST_SIZE                          0x880
#define OMAP_ABE_D_HW_TEST_ADDR                            0x1244
#define OMAP_ABE_D_HW_TEST_SIZE                            0x28
#define OMAP_ABE_D_TRACEBUFADR_HAL_ADDR                    0x126C
#define OMAP_ABE_D_TRACEBUFADR_HAL_SIZE                    0x4
#define OMAP_ABE_D_DEBUG_FW_TASK_ADDR                      0x1400
#define OMAP_ABE_D_DEBUG_FW_TASK_SIZE                      0x100
#define OMAP_ABE_D_DEBUG_FIFO_ADDR                         0x1500
#define OMAP_ABE_D_DEBUG_FIFO_SIZE                         0x60
#define OMAP_ABE_D_DEBUG_FIFO_HAL_ADDR                     0x1560
#define OMAP_ABE_D_DEBUG_FIFO_HAL_SIZE                     0x20
#define OMAP_ABE_D_FWMEMINIT_ADDR                          0x1580
#define OMAP_ABE_D_FWMEMINIT_SIZE                          0x3C0
#define OMAP_ABE_D_FWMEMINITDESCR_ADDR                     0x1940
#define OMAP_ABE_D_FWMEMINITDESCR_SIZE                     0x10
#define OMAP_ABE_D_BT_DL_FIFO_ADDR                         0x1C00
#define OMAP_ABE_D_BT_DL_FIFO_SIZE                         0x1E0
#define OMAP_ABE_D_BT_UL_FIFO_ADDR                         0x1E00
#define OMAP_ABE_D_BT_UL_FIFO_SIZE                         0x1E0
#define OMAP_ABE_D_MM_EXT_OUT_FIFO_ADDR                    0x2000
#define OMAP_ABE_D_MM_EXT_OUT_FIFO_SIZE                    0x1E0
#define OMAP_ABE_D_MM_EXT_IN_FIFO_ADDR                     0x2200
#define OMAP_ABE_D_MM_EXT_IN_FIFO_SIZE                     0x1E0
#define OMAP_ABE_D_MM_UL2_FIFO_ADDR                        0x2400
#define OMAP_ABE_D_MM_UL2_FIFO_SIZE                        0x1E0
#define OMAP_ABE_D_DMIC_UL_FIFO_ADDR                       0x2600
#define OMAP_ABE_D_DMIC_UL_FIFO_SIZE                       0x1E0
#define OMAP_ABE_D_MM_UL_FIFO_ADDR                         0x2800
#define OMAP_ABE_D_MM_UL_FIFO_SIZE                         0x1E0
#define OMAP_ABE_D_MM_DL_FIFO_ADDR                         0x2A00
#define OMAP_ABE_D_MM_DL_FIFO_SIZE                         0x1E0
#define OMAP_ABE_D_TONES_DL_FIFO_ADDR                      0x2C00
#define OMAP_ABE_D_TONES_DL_FIFO_SIZE                      0x1E0
#define OMAP_ABE_D_VIB_DL_FIFO_ADDR                        0x2E00
#define OMAP_ABE_D_VIB_DL_FIFO_SIZE                        0x1E0
#define OMAP_ABE_D_DEBUG_HAL_TASK_ADDR                     0x3000
#define OMAP_ABE_D_DEBUG_HAL_TASK_SIZE                     0x800
#define OMAP_ABE_D_MCPDM_DL_FIFO_ADDR                      0x3800
#define OMAP_ABE_D_MCPDM_DL_FIFO_SIZE                      0x1E0
#define OMAP_ABE_D_MCPDM_UL_FIFO_ADDR                      0x3A00
#define OMAP_ABE_D_MCPDM_UL_FIFO_SIZE                      0x1E0
#define OMAP_ABE_D_VX_UL_FIFO_ADDR                         0x3C00
#define OMAP_ABE_D_VX_UL_FIFO_SIZE                         0x1E0
#define OMAP_ABE_D_VX_DL_FIFO_ADDR                         0x3E00
#define OMAP_ABE_D_VX_DL_FIFO_SIZE                         0x1E0
#define OMAP_ABE_D_PING_ADDR                               0x4000
#define OMAP_ABE_D_PING_SIZE                               0x6000
#define OMAP_ABE_D_PONG_ADDR                               0xA000
#define OMAP_ABE_D_PONG_SIZE                               0x6000
