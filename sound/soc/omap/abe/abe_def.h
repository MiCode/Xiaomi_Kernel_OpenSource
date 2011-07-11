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

#ifndef _ABE_DEF_H_
#define _ABE_DEF_H_
/*
 * HARDWARE AND PERIPHERAL DEFINITIONS
 */
/* MM_DL */
#define ABE_CBPR0_IDX 0
/* VX_DL */
#define ABE_CBPR1_IDX 1
/* VX_UL */
#define ABE_CBPR2_IDX 2
/* MM_UL */
#define ABE_CBPR3_IDX 3
/* MM_UL2 */
#define ABE_CBPR4_IDX 4
/* TONES */
#define ABE_CBPR5_IDX 5
/* VIB */
#define ABE_CBPR6_IDX 6
/* DEBUG/CTL */
#define ABE_CBPR7_IDX 7
#define CIRCULAR_BUFFER_PERIPHERAL_R__0 (0x100 + ABE_CBPR0_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__1 (0x100 + ABE_CBPR1_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__2 (0x100 + ABE_CBPR2_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__3 (0x100 + ABE_CBPR3_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__4 (0x100 + ABE_CBPR4_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__5 (0x100 + ABE_CBPR5_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__6 (0x100 + ABE_CBPR6_IDX*4)
#define CIRCULAR_BUFFER_PERIPHERAL_R__7 (0x100 + ABE_CBPR7_IDX*4)
#define PING_PONG_WITH_MCU_IRQ	 1
#define PING_PONG_WITH_DSP_IRQ	 2
/* ID used for LIB memory copy subroutines */
#define COPY_FROM_ABE_TO_HOST 1
#define COPY_FROM_HOST_TO_ABE 2
/*
 * INTERNAL DEFINITIONS
 */
#define ABE_FIRMWARE_MAX_SIZE 26629
/* 24 Q6.26 coefficients */
#define NBEQ1 25
/* 2x12 Q6.26 coefficients */
#define NBEQ2 13
/* TBD APS first set of parameters */
#define NBAPS1 10
/* TBD APS second set of parameters */
#define NBAPS2 10
/* Mixer used for sending tones to the uplink voice path */
#define NBMIX_AUDIO_UL 2
/* Main downlink mixer */
#define NBMIX_DL1 4
/* Handsfree downlink mixer */
#define NBMIX_DL2 4
/* Side-tone mixer */
#define NBMIX_SDT 2
/* Echo reference mixer */
#define NBMIX_ECHO 2
/* Voice record mixer */
#define NBMIX_VXREC 4
/* unsigned version of (-1) */
#define CC_M1 0xFF
#define CS_M1 0xFFFF
#define CL_M1 0xFFFFFFFFL
/*
	Mixer ID	 Input port ID		Comments
	DL1_MIXER	 0 MMDL path
	 1 MMUL2 path
	 2 VXDL path
	 3 TONES path
	SDT_MIXER	 0 Uplink path
	 1 Downlink path
	ECHO_MIXER	 0 DL1_MIXER path
	 1 DL2_MIXER path
	AUDUL_MIXER	 0 TONES_DL path
	 1 Uplink path
	 2 MM_DL path
	VXREC_MIXER	 0 TONES_DL path
	 1 VX_DL path
	 2 MM_DL path
	 3 VX_UL path
*/
#define MIX_VXUL_INPUT_MM_DL 0
#define MIX_VXUL_INPUT_TONES 1
#define MIX_VXUL_INPUT_VX_UL 2
#define MIX_VXUL_INPUT_VX_DL 3
#define MIX_DL1_INPUT_MM_DL 0
#define MIX_DL1_INPUT_MM_UL2 1
#define MIX_DL1_INPUT_VX_DL 2
#define MIX_DL1_INPUT_TONES 3
#define MIX_DL2_INPUT_MM_DL 0
#define MIX_DL2_INPUT_MM_UL2 1
#define MIX_DL2_INPUT_VX_DL 2
#define MIX_DL2_INPUT_TONES 3
#define MIX_SDT_INPUT_UP_MIXER	0
#define MIX_SDT_INPUT_DL1_MIXER 1
#define MIX_AUDUL_INPUT_MM_DL 0
#define MIX_AUDUL_INPUT_TONES 1
#define MIX_AUDUL_INPUT_UPLINK 2
#define MIX_AUDUL_INPUT_VX_DL 3
#define MIX_VXREC_INPUT_MM_DL 0
#define MIX_VXREC_INPUT_TONES 1
#define MIX_VXREC_INPUT_VX_UL 2
#define MIX_VXREC_INPUT_VX_DL 3
#define MIX_ECHO_DL1	0
#define MIX_ECHO_DL2	1
/* nb of samples to route */
#define NBROUTE_UL 16
/* 10 routing tables max */
#define NBROUTE_CONFIG_MAX 10
/* 5 pre-computed routing tables */
#define NBROUTE_CONFIG 6
/* AMIC on VX_UL */
#define UPROUTE_CONFIG_AMIC 0
/* DMIC first pair on VX_UL */
#define UPROUTE_CONFIG_DMIC1 1
/* DMIC second pair on VX_UL */
#define UPROUTE_CONFIG_DMIC2 2
/* DMIC last pair on VX_UL */
#define UPROUTE_CONFIG_DMIC3 3
/* BT_UL on VX_UL */
#define UPROUTE_CONFIG_BT 4
/* ECHO_REF on MM_UL2 */
#define UPROUTE_ECHO_MMUL2 5
/* call-back indexes */
#define MAXCALLBACK 100
/* subroutines */
#define MAXNBSUBROUTINE 100
/* time controlled sequenced */
#define MAXNBSEQUENCE 20
/* maximum simultaneous active sequences */
#define MAXACTIVESEQUENCE 20
/* max number of steps in the sequences */
#define MAXSEQUENCESTEPS 2
/* max number of feature associated to a port */
#define MAXFEATUREPORT 12
#define SUB_0_PARAM 0
/* number of parameters per sequence calls */
#define SUB_1_PARAM 1
#define SUB_2_PARAM 2
#define SUB_3_PARAM 3
#define SUB_4_PARAM 4
/* active sequence mask = 0 means the line is free */
#define FREE_LINE 0
/* no ask for collision protection */
#define NOMASK (1 << 0)
/* do not allow a PDM OFF during the execution of this sequence */
#define MASK_PDM_OFF (1 << 1)
/* do not allow a PDM ON during the execution of this sequence */
#define MASK_PDM_ON (1 << 2)
/* explicit name of the feature */
#define NBCHARFEATURENAME 16
/* explicit name of the port */
#define NBCHARPORTNAME 16
/* sink / input port from Host point of view (or AESS for DMIC/McPDM/.. */
#define SNK_P ABE_ATC_DIRECTION_IN
/* source / ouptut port */
#define SRC_P ABE_ATC_DIRECTION_OUT
/* no ASRC applied */
#define NODRIFT 0
/* for abe_set_asrc_drift_control */
#define FORCED_DRIFT_CONTROL 1
/* for abe_set_asrc_drift_control */
#define ADPATIVE_DRIFT_CONTROL 2
/* number of task/slot depending on the OPP value */
#define DOPPMODE32_OPP100 (0x00000010)
#define DOPPMODE32_OPP50 (0x0000000C)
#define DOPPMODE32_OPP25 (0x0000004)
/*
 * ABE CONST AREA FOR PARAMETERS TRANSLATION
 */
#define GAIN_MAXIMUM 3000L
#define GAIN_24dB 2400L
#define GAIN_18dB 1800L
#define GAIN_12dB 1200L
#define GAIN_6dB 600L
/* default gain = 1 */
#define GAIN_0dB  0L
#define GAIN_M6dB -600L
#define GAIN_M12dB -1200L
#define GAIN_M18dB -1800L
#define GAIN_M24dB -2400L
#define GAIN_M30dB -3000L
#define GAIN_M40dB -4000L
#define GAIN_M50dB -5000L
/* muted gain = -120 decibels */
#define MUTE_GAIN -12000L
#define GAIN_TOOLOW -13000L
#define GAIN_MUTE MUTE_GAIN
#define RAMP_MINLENGTH 0L
/* ramp_t is in milli- seconds */
#define RAMP_0MS 0L
#define RAMP_1MS 1L
#define RAMP_2MS 2L
#define RAMP_5MS 5L
#define RAMP_10MS 10L
#define RAMP_20MS 20L
#define RAMP_50MS 50L
#define RAMP_100MS 100L
#define RAMP_200MS  200L
#define RAMP_500MS  500L
#define RAMP_1000MS  1000L
#define RAMP_MAXLENGTH  10000L
/* for abe_translate_gain_format */
#define LINABE_TO_DECIBELS 1
#define DECIBELS_TO_LINABE 2
/* for abe_translate_ramp_format */
#define IIRABE_TO_MICROS 1
#define MICROS_TO_IIABE 2
/*
 * ABE CONST AREA FOR PERIPHERAL TUNING
 */
/* port idled IDLE_P */
#define OMAP_ABE_PORT_ACTIVITY_IDLE	1
/* port initialized, ready to be activated  */
#define OMAP_ABE_PORT_INITIALIZED	 3
/* port activated RUN_P */
#define OMAP_ABE_PORT_ACTIVITY_RUNNING	 2
#define NOCALLBACK 0
#define NOPARAMETER 0
/* number of ATC access upon AMIC DMArequests, all the FIFOs are enabled */
#define MCPDM_UL_ITER 4
/* All the McPDM FIFOs are enabled simultaneously */
#define MCPDM_DL_ITER 24
/* All the DMIC FIFOs are enabled simultaneously */
#define DMIC_ITER 12
/* TBD later if needed */
#define MAX_PINGPONG_BUFFERS 2
/*
 * Indexes to the subroutines
 */
#define SUB_WRITE_MIXER 1
#define SUB_WRITE_PORT_GAIN 2
/* OLD WAY */
#define c_feat_init_eq 1
#define c_feat_read_eq1 2
#define c_write_eq1 3
#define c_feat_read_eq2 4
#define c_write_eq2 5
#define c_feat_read_eq3 6
#define c_write_eq3 7
/* max number of gain to be controlled by HAL */
#define MAX_NBGAIN_CMEM 36
/*
 * MACROS
 */
#define maximum(a, b) (((a) < (b)) ? (b) : (a))
#define minimum(a, b) (((a) > (b)) ? (b) : (a))
#define absolute(a) (((a) > 0) ? (a) : ((-1)*(a)))
#define HAL_VERSIONS 9
#endif/* _ABE_DEF_H_ */
