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

#ifndef _ABE_MAIN_H_
#define _ABE_MAIN_H_

#include <linux/io.h>

#include "abe_initxxx_labels.h"

#define D_DEBUG_FIFO_ADDR                                   8160
#define D_DEBUG_FIFO_ADDR_END                               8255

#define SUB_0_PARAM 0
#define SUB_1_PARAM 1

#define ABE_DEFAULT_BASE_ADDRESS_L3 0x49000000L
#define ABE_DMEM_BASE_ADDRESS_MPU   0x49088000L
#define ABE_DMEM_BASE_OFFSET_MPU    0x00080000L
#define ABE_DMEM_BASE_ADDRESS_L3    ABE_DMEM_BASE_ADDRESS_MPU

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

/*
 *	OPP TYPE
 *
 *		0: Ultra Lowest power consumption audio player
 *		1: OPP 25% (simple multimedia features)
 *		2: OPP 50% (multimedia and voice calls)
 *		3: OPP100% (multimedia complex use-cases)
 */
#define ABE_OPP0 0
#define ABE_OPP25 1
#define ABE_OPP50 2
#define ABE_OPP100 3
/*
 *	SAMPLES TYPE
 *
 *	mono 16 bit sample LSB aligned, 16 MSB bits are unused;
 *	mono right shifted to 16bits LSBs on a 32bits DMEM FIFO for McBSP
 *	TX purpose;
 *	mono sample MSB aligned (16/24/32bits);
 *	two successive mono samples in one 32bits container;
 *	Two L/R 16bits samples in a 32bits container;
 *	Two channels defined with two MSB aligned samples;
 *	Three channels defined with three MSB aligned samples (MIC);
 *	Four channels defined with four MSB aligned samples (MIC);
 *	. . .
 *	Eight channels defined with eight MSB aligned samples (MIC);
 */
#define MONO_MSB 1
#define MONO_RSHIFTED_16 2
#define STEREO_RSHIFTED_16 3
#define STEREO_16_16 4
#define STEREO_MSB 5
#define THREE_MSB 6
#define FOUR_MSB 7
#define FIVE_MSB 8
#define SIX_MSB 9
#define SEVEN_MSB 10
#define EIGHT_MSB 11
#define NINE_MSB 12
#define TEN_MSB 13
/*
 *	PORT PROTOCOL TYPE - abe_port_protocol_switch_id
 */
#define SLIMBUS_PORT_PROT 1
#define SERIAL_PORT_PROT 2
#define TDM_SERIAL_PORT_PROT 3
#define DMIC_PORT_PROT 4
#define MCPDMDL_PORT_PROT 5
#define MCPDMUL_PORT_PROT 6
#define PINGPONG_PORT_PROT 7
#define DMAREQ_PORT_PROT 8
/*
 *	PORT IDs, this list is aligned with the FW data mapping
 */
#define DMIC_PORT 0
#define PDM_UL_PORT 1
#define BT_VX_UL_PORT 2
#define MM_UL_PORT 3
#define MM_UL2_PORT 4
#define VX_UL_PORT 5
#define MM_DL_PORT 6
#define VX_DL_PORT 7
#define TONES_DL_PORT 8
#define VIB_DL_PORT 9
#define BT_VX_DL_PORT 10
#define PDM_DL_PORT 11
#define MM_EXT_OUT_PORT 12
#define MM_EXT_IN_PORT 13
#define TDM_DL_PORT 14
#define TDM_UL_PORT 15
#define DEBUG_PORT 16
#define LAST_PORT_ID 17
/* definitions for the compatibility with HAL05xx */
#define PDM_DL1_PORT 18
#define PDM_DL2_PORT 19
#define PDM_VIB_PORT 20
/* There is only one DMIC port, always used with 6 samples
	per 96kHz periods   */
#define DMIC_PORT1 DMIC_PORT
#define DMIC_PORT2 DMIC_PORT
#define DMIC_PORT3 DMIC_PORT
/*
 *	Signal processing module names - EQ APS MIX ROUT
 */
/* equalizer downlink path headset + earphone */
#define FEAT_EQ1            1
/* equalizer downlink path integrated handsfree LEFT */
#define FEAT_EQ2L           (FEAT_EQ1+1)
/* equalizer downlink path integrated handsfree RIGHT */
#define FEAT_EQ2R           (FEAT_EQ2L+1)
/* equalizer downlink path side-tone */
#define FEAT_EQSDT          (FEAT_EQ2R+1)
/* equalizer uplink path AMIC */
#define FEAT_EQAMIC         (FEAT_EQSDT+1)
/* equalizer uplink path DMIC */
#define FEAT_EQDMIC         (FEAT_EQAMIC+1)
/* Acoustic protection for headset */
#define FEAT_APS1           (FEAT_EQDMIC+1)
/* acoustic protection high-pass filter for handsfree "Left" */
#define FEAT_APS2           (FEAT_APS1+1)
/* acoustic protection high-pass filter for handsfree "Right" */
#define FEAT_APS3           (FEAT_APS2+1)
/* asynchronous sample-rate-converter for the downlink voice path */
#define FEAT_ASRC1          (FEAT_APS3+1)
/* asynchronous sample-rate-converter for the uplink voice path */
#define FEAT_ASRC2          (FEAT_ASRC1+1)
/* asynchronous sample-rate-converter for the multimedia player */
#define FEAT_ASRC3          (FEAT_ASRC2+1)
/* asynchronous sample-rate-converter for the echo reference */
#define FEAT_ASRC4          (FEAT_ASRC3+1)
/* mixer of the headset and earphone path */
#define FEAT_MIXDL1         (FEAT_ASRC4+1)
/* mixer of the hands-free path */
#define FEAT_MIXDL2         (FEAT_MIXDL1+1)
/* mixer for audio being sent on the voice_ul path */
#define FEAT_MIXAUDUL       (FEAT_MIXDL2+1)
/* mixer for voice communication recording */
#define FEAT_MIXVXREC       (FEAT_MIXAUDUL+1)
/* mixer for side-tone */
#define FEAT_MIXSDT         (FEAT_MIXVXREC+1)
/* mixer for echo reference */
#define FEAT_MIXECHO        (FEAT_MIXSDT+1)
/* router of the uplink path */
#define FEAT_UPROUTE        (FEAT_MIXECHO+1)
/* all gains */
#define FEAT_GAINS          (FEAT_UPROUTE+1)
#define FEAT_GAINS_DMIC1    (FEAT_GAINS+1)
#define FEAT_GAINS_DMIC2    (FEAT_GAINS_DMIC1+1)
#define FEAT_GAINS_DMIC3    (FEAT_GAINS_DMIC2+1)
#define FEAT_GAINS_AMIC     (FEAT_GAINS_DMIC3+1)
#define FEAT_GAINS_SPLIT    (FEAT_GAINS_AMIC+1)
#define FEAT_GAINS_DL1      (FEAT_GAINS_SPLIT+1)
#define FEAT_GAINS_DL2      (FEAT_GAINS_DL1+1)
#define FEAT_GAIN_BTUL      (FEAT_GAINS_DL2+1)
/* sequencing queue of micro tasks */
#define FEAT_SEQ            (FEAT_GAIN_BTUL+1)
/* Phoenix control queue through McPDM */
#define FEAT_CTL            (FEAT_SEQ+1)
/* list of features of the firmware -------------------------------*/
#define MAXNBFEATURE    FEAT_CTL
/* abe_equ_id */
/* equalizer downlink path headset + earphone */
#define EQ1 FEAT_EQ1
/* equalizer downlink path integrated handsfree LEFT */
#define EQ2L FEAT_EQ2L
#define EQ2R FEAT_EQ2R
/* equalizer downlink path side-tone */
#define EQSDT  FEAT_EQSDT
#define EQAMIC FEAT_EQAMIC
#define EQDMIC FEAT_EQDMIC
/* abe_aps_id */
/* Acoustic protection for headset */
#define APS1 FEAT_APS1
#define APS2L FEAT_APS2
#define APS2R FEAT_APS3
/* abe_asrc_id */
/* asynchronous sample-rate-converter for the downlink voice path */
#define ASRC1 FEAT_ASRC1
/* asynchronous sample-rate-converter for the uplink voice path */
#define ASRC2 FEAT_ASRC2
/* asynchronous sample-rate-converter for the multimedia player */
#define ASRC3 FEAT_ASRC3
/* asynchronous sample-rate-converter for the voice uplink echo_reference */
#define ASRC4 FEAT_ASRC4
/* abe_mixer_id */
#define MIXDL1 FEAT_MIXDL1
#define MIXDL2 FEAT_MIXDL2
#define MIXSDT FEAT_MIXSDT
#define MIXECHO FEAT_MIXECHO
#define MIXAUDUL FEAT_MIXAUDUL
#define MIXVXREC FEAT_MIXVXREC
/* abe_router_id */
/* there is only one router up to now */
#define UPROUTE  FEAT_UPROUTE
/*
 * gain controls
 */
#define GAIN_LEFT_OFFSET 0
#define GAIN_RIGHT_OFFSET 1
/*
 *	GAIN IDs
 */
#define GAINS_DMIC1     FEAT_GAINS_DMIC1
#define GAINS_DMIC2     FEAT_GAINS_DMIC2
#define GAINS_DMIC3     FEAT_GAINS_DMIC3
#define GAINS_AMIC      FEAT_GAINS_AMIC
#define GAINS_SPLIT     FEAT_GAINS_SPLIT
#define GAINS_DL1       FEAT_GAINS_DL1
#define GAINS_DL2       FEAT_GAINS_DL2
#define GAINS_BTUL      FEAT_GAIN_BTUL
/*
 * ABE CONST AREA FOR PARAMETERS TRANSLATION
 */
#define sizeof_alpha_iir_table 61
#define sizeof_beta_iir_table 61
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
#define RAMP_MINLENGTH 3L
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
 *	EVENT GENERATORS - abe_event_id
 */
#define EVENT_TIMER 0
#define EVENT_44100 1
/*
 * DMA requests
 */
/*Internal connection doesn't connect at ABE boundary */
#define External_DMA_0	0
/*Transmit request digital microphone */
#define DMIC_DMA_REQ	1
/*Multichannel PDM downlink */
#define McPDM_DMA_DL	2
/*Multichannel PDM uplink */
#define McPDM_DMA_UP	3
/*MCBSP module 1 - transmit request */
#define MCBSP1_DMA_TX	4
/*MCBSP module 1 - receive request */
#define MCBSP1_DMA_RX	5
/*MCBSP module 2 - transmit request */
#define MCBSP2_DMA_TX	6
/*MCBSP module 2 - receive request */
#define MCBSP2_DMA_RX	7
/*MCBSP module 3 - transmit request */
#define MCBSP3_DMA_TX	8
/*MCBSP module 3 - receive request */
#define MCBSP3_DMA_RX	9
/*
 *	SERIAL PORTS IDs - abe_mcbsp_id
 */
#define MCBSP1_TX MCBSP1_DMA_TX
#define MCBSP1_RX MCBSP1_DMA_RX
#define MCBSP2_TX MCBSP2_DMA_TX
#define MCBSP2_RX MCBSP2_DMA_RX
#define MCBSP3_TX MCBSP3_DMA_TX
#define MCBSP3_RX MCBSP3_DMA_RX

#define PING_PONG_WITH_MCU_IRQ	 1
#define PING_PONG_WITH_DSP_IRQ	 2

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

/*
 *	DMA_T
 *
 *	dma structure for easing programming
 */
typedef struct {
	/* OCP L3 pointer to the first address of the */
	void *data;
	/* destination buffer (either DMA or Ping-Pong read/write pointers). */
	/* address L3 when addressing the DMEM buffer instead of CBPr */
	void *l3_dmem;
	/* address L3 translated to L4 the ARM memory space */
	void *l4_dmem;
	/* number of iterations for the DMA data moves. */
	u32 iter;
} abe_dma_t;
typedef u32 abe_dbg_t;
/*
 *	ROUTER_T
 *
 *	table of indexes in unsigned bytes
 */
typedef u16 abe_router_t;
/*
 *	DATA_FORMAT_T
 *
 *	used in port declaration
 */
typedef struct {
	/* Sampling frequency of the stream */
	u32 f;
	/* Sample format type  */
	u32 samp_format;
} abe_data_format_t;
/*
 *	PORT_PROTOCOL_T
 *
 *	port declaration
 */
typedef struct {
	/* Direction=0 means input from AESS point of view */
	u32 direction;
	/* Protocol type (switch) during the data transfers */
	u32 protocol_switch;
	union {
		/* Slimbus peripheral connected to ATC */
		struct {
			/* Address of ATC Slimbus descriptor's index */
			u32 desc_addr1;
			/* DMEM address 1 in bytes */
			u32 buf_addr1;
			/* DMEM buffer size size in bytes */
			u32 buf_size;
			/* ITERation on each DMAreq signals */
			u32 iter;
			/* Second ATC index for SlimBus reception (or NULL) */
			u32 desc_addr2;
			/* DMEM address 2 in bytes */
			u32 buf_addr2;
		} prot_slimbus;
		/* McBSP/McASP peripheral connected to ATC */
		struct {
			u32 desc_addr;
			/* Address of ATC McBSP/McASP descriptor's in bytes */
			u32 buf_addr;
			/* DMEM address in bytes */
			u32 buf_size;
			/* ITERation on each DMAreq signals */
			u32 iter;
		} prot_serial;
		/* DMIC peripheral connected to ATC */
		struct {
			/* DMEM address in bytes */
			u32 buf_addr;
			/* DMEM buffer size in bytes */
			u32 buf_size;
			/* Number of activated DMIC */
			u32 nbchan;
		} prot_dmic;
		/* McPDMDL peripheral connected to ATC */
		struct {
			/* DMEM address in bytes */
			u32 buf_addr;
			/* DMEM size in bytes */
			u32 buf_size;
			/* Control allowed on McPDM DL */
			u32 control;
		} prot_mcpdmdl;
		/* McPDMUL peripheral connected to ATC */
		struct {
			/* DMEM address size in bytes */
			u32 buf_addr;
			/* DMEM buffer size size in bytes */
			u32 buf_size;
		} prot_mcpdmul;
		/* Ping-Pong interface to the Host using cache-flush */
		struct {
			/* Address of ATC descriptor's */
			u32 desc_addr;
			/* DMEM buffer base address in bytes */
			u32 buf_addr;
			/* DMEM size in bytes for each ping and pong buffers */
			u32 buf_size;
			/* IRQ address (either DMA (0) MCU (1) or DSP(2)) */
			u32 irq_addr;
			/* IRQ data content loaded in the AESS IRQ register */
			u32 irq_data;
			/* Call-back function upon IRQ reception */
			u32 callback;
		} prot_pingpong;
		/* DMAreq line to CBPr */
		struct {
			/* Address of ATC descriptor's */
			u32 desc_addr;
			/* DMEM buffer address in bytes */
			u32 buf_addr;
			/* DMEM buffer size size in bytes */
			u32 buf_size;
			/* ITERation on each DMAreq signals */
			u32 iter;
			/* DMAreq address */
			u32 dma_addr;
			/* DMA/AESS = 1 << #DMA */
			u32 dma_data;
		} prot_dmareq;
		/* Circular buffer - direct addressing to DMEM */
		struct {
			/* DMEM buffer base address in bytes */
			u32 buf_addr;
			/* DMEM buffer size in bytes */
			u32 buf_size;
			/* DMAreq address */
			u32 dma_addr;
			/* DMA/AESS = 1 << #DMA */
			u32 dma_data;
		} prot_circular_buffer;
	} p;
} abe_port_protocol_t;

/*
 *	EQU_T
 *
 *	coefficients of the equalizer
 */
/* 24 Q6.26 coefficients */
#define NBEQ1 25
/* 2x12 Q6.26 coefficients */
#define NBEQ2 13

typedef struct {
	/* type of filter */
	u32 equ_type;
	/* filter length */
	u32 equ_length;
	union {
		/* parameters are the direct and recursive coefficients in */
		/* Q6.26 integer fixed-point format. */
		s32 type1[NBEQ1];
		struct {
			/* center frequency of the band [Hz] */
			s32 freq[NBEQ2];
			/* gain of each band. [dB] */
			s32 gain[NBEQ2];
			/* Q factor of this band [dB] */
			s32 q[NBEQ2];
		} type2;
	} coef;
	s32 equ_param3;
} abe_equ_t;


/* subroutine with no parameter */
typedef void (*abe_subroutine0) (void);
/* subroutine with one parameter */
typedef void (*abe_subroutine1) (u32);
typedef void (*abe_subroutine2) (u32, u32);
typedef void (*abe_subroutine3) (u32, u32, u32);
typedef void (*abe_subroutine4) (u32, u32, u32, u32);


extern u32 abe_irq_pingpong_player_id;


void abe_init_mem(void __iomem **_io_base);
u32 abe_reset_hal(void);
u32 abe_load_fw(void);
u32 abe_reload_fw(void);
u32 abe_wakeup(void);
u32 abe_irq_processing(void);
u32 abe_clear_irq(void);
u32 abe_disable_irq(void);
u32 abe_write_event_generator(u32 e);
u32 abe_stop_event_generator(void);
u32 abe_connect_debug_trace(abe_dma_t *dma2);
u32 abe_set_debug_trace(abe_dbg_t debug);
u32 abe_set_ping_pong_buffer(u32 port, u32 n_bytes);
u32 abe_read_next_ping_pong_buffer(u32 port, u32 *p, u32 *n);
u32 abe_init_ping_pong_buffer(u32 id, u32 size_bytes, u32 n_buffers,
					u32 *p);
u32 abe_read_offset_from_ping_buffer(u32 id, u32 *n);
u32 abe_write_equalizer(u32 id, abe_equ_t *param);
u32 abe_disable_gain(u32 id, u32 p);
u32 abe_enable_gain(u32 id, u32 p);
u32 abe_mute_gain(u32 id, u32 p);
u32 abe_unmute_gain(u32 id, u32 p);
u32 abe_write_gain(u32 id, s32 f_g, u32 ramp, u32 p);
u32 abe_write_mixer(u32 id, s32 f_g, u32 f_ramp, u32 p);
u32 abe_read_gain(u32 id, u32 *f_g, u32 p);
u32 abe_read_mixer(u32 id, u32 *f_g, u32 p);
u32 abe_set_router_configuration(u32 id, u32 k, u32 *param);
u32 abe_set_opp_processing(u32 opp);
u32 abe_disable_data_transfer(u32 id);
u32 abe_enable_data_transfer(u32 id);
u32 abe_connect_cbpr_dmareq_port(u32 id, abe_data_format_t *f, u32 d,
					   abe_dma_t *returned_dma_t);
u32 abe_connect_irq_ping_pong_port(u32 id, abe_data_format_t *f,
					     u32 subroutine_id, u32 size,
					     u32 *sink, u32 dsp_mcu_flag);
u32 abe_connect_serial_port(u32 id, abe_data_format_t *f,
				      u32 mcbsp_id);
u32 abe_read_port_address(u32 port, abe_dma_t *dma2);
void abe_add_subroutine(u32 *id, abe_subroutine2 f, u32 nparam, u32 *params);
u32 abe_read_next_ping_pong_buffer(u32 port, u32 *p, u32 *n);
u32 abe_check_activity(void);
void abe_add_subroutine(u32 *id, abe_subroutine2 f,
						u32 nparam, u32 *params);

u32 abe_plug_subroutine(u32 *id, abe_subroutine2 f, u32 n,
			u32 *params);

#endif				/* _ABE_MAIN_H_ */
