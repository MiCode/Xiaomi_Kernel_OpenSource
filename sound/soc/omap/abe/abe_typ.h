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

#include "abe_def.h"
#include "abe_initxxx_labels.h"

#ifndef _ABE_TYP_H_
#define _ABE_TYP_H_
/*
 *	BASIC TYPES
 */
#define MAX_UINT8	((((1L <<  7) - 1) << 1) + 1)
#define MAX_UINT16	((((1L << 15) - 1) << 1) + 1)
#define MAX_UINT32	((((1L << 31) - 1) << 1) + 1)
#define s8 char
#define u8 unsigned char
#define s16 short
#define u16 unsigned short
#define s32 int
#define u32 unsigned int
/* returned status from HAL APIs */
#define abehal_status u32
/* 4 bytes    Bit field indicating the type of informations to be traced */
typedef u32 abe_dbg_mask_t;
/* scheduling task loops (250us / 272us with respectively 48kHz /
	44.1kHz on Phoenix). */
typedef u32 abe_dbg_t;
/* Index to the table of sequences */
typedef u32 abe_seq_code_t;
/* Index to the table of subroutines called in the sequence */
typedef u32 abe_sub_code_t;
/* subroutine with no parameter */
typedef void (*abe_subroutine0) (void);
/* subroutine with one parameter */
typedef void (*abe_subroutine1) (u32);
typedef void (*abe_subroutine2) (u32, u32);
typedef void (*abe_subroutine3) (u32, u32, u32);
typedef void (*abe_subroutine4) (u32, u32, u32, u32);
/*
 *	CODE PORTABILITY - FUTURE PATCHES
 *
 *	32bits field for having the code compatible with future revisions of
 *	the hardware (audio integration) or evolution of the software
 *	partitionning. Used for the highest level APIs (launch_sequences)
 */
typedef u32 abe_patch_rev;
/*
 *	ENUMS
 */
/*
 *	MEMORY CONFIG TYPE
 *
 *		0: Ultra Lowest power consumption audio player
 *		1: OPP 25% (simple multimedia features)
 *		2: OPP 50% (multimedia and voice calls)
 *		3: OPP100% (multimedia complex use-cases)
 */
#define ABE_AUDIO_PLAYER_ON_HEADSET_OR_EARPHONE 1
#define ABE_DRIFT_MANAGEMENT_FOR_AUDIO_PLAYER 2
#define ABE_DRIFT_MANAGEMENT_FOR_VOICE_CALL 3
#define ABE_VOICE_CALL_ON_HEADSET_OR_EARPHONE_OR_BT 4
#define ABE_MULTIMEDIA_AUDIO_RECORDER 5
#define ABE_VIBRATOR_OR_HAPTICS 6
#define ABE_VOICE_CALL_ON_HANDS_FREE_SPEAKER 7
#define ABE_RINGER_TONES 8
#define ABE_VOICE_CALL_WITH_EARPHONE_ACTIVE_NOISE_CANCELLER 9
#define ABE_LAST_USE_CASE 10
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
 *	DMIC DECIMATION RATIO
 *
 */
#define ABE_DEC16 16
#define ABE_DEC25 25
#define ABE_DEC32 32
#define ABE_DEC40 40
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
#define OMAP_ABE_DMIC_PORT 0
#define OMAP_ABE_PDM_UL_PORT 1
#define OMAP_ABE_BT_VX_UL_PORT 2
#define OMAP_ABE_MM_UL_PORT 3
#define OMAP_ABE_MM_UL2_PORT 4
#define OMAP_ABE_VX_UL_PORT 5
#define OMAP_ABE_MM_DL_PORT 6
#define OMAP_ABE_VX_DL_PORT 7
#define OMAP_ABE_TONES_DL_PORT 8
#define OMAP_ABE_VIB_DL_PORT 9
#define OMAP_ABE_BT_VX_DL_PORT 10
#define OMAP_ABE_PDM_DL_PORT 11
#define OMAP_ABE_MM_EXT_OUT_PORT 12
#define OMAP_ABE_MM_EXT_IN_PORT 13
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
 *	ABE_DL_SRC_ID     source of samples
 */
#define SRC_DL1_MIXER_OUTPUT DL1_M_labelID
#define SRC_SDT_MIXER_OUTPUT SDT_M_labelID
#define SRC_DL1_GAIN_OUTPUT DL1_GAIN_out_labelID
#define SRC_DL1_EQ_OUTPUT DL1_EQ_labelID
#define SRC_DL2_GAIN_OUTPUT DL2_GAIN_out_labelID
#define SRC_DL2_EQ_OUTPUT DL2_EQ_labelID
#define SRC_MM_DL MM_DL_labelID
#define SRC_TONES_DL  Tones_labelID
#define SRC_VX_DL VX_DL_labelID
#define SRC_VX_UL VX_UL_labelID
#define SRC_MM_UL2 MM_UL2_labelID
#define SRC_MM_UL MM_UL_labelID
/*
 *	abe_patched_pattern_id
 *		selection of the audio engine signal to
 *		replace by a precomputed pattern
 */
#define DBG_PATCH_AMIC 1
#define DBG_PATCH_DMIC1 2
#define DBG_PATCH_DMIC2 3
#define DBG_PATCH_DMIC3 4
#define DBG_PATCH_VX_REC 5
#define DBG_PATCH_BT_UL 6
#define DBG_PATCH_MM_DL 7
#define DBG_PATCH_DL2_EQ 8
#define DBG_PATCH_VIBRA 9
#define DBG_PATCH_MM_EXT_IN 10
#define DBG_PATCH_EANC_FBK_Out 11
#define DBG_PATCH_MIC4 12
#define DBG_PATCH_MM_DL_MIXDL1 13
#define DBG_PATCH_MM_DL_MIXDL2 14
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
 *	EVENT GENERATORS - abe_event_id
 */
#define EVENT_TIMER 0
#define EVENT_44100 1
/*
 *	SERIAL PORTS IDs - abe_mcbsp_id
 */
#define MCBSP1_TX MCBSP1_DMA_TX
#define MCBSP1_RX MCBSP1_DMA_RX
#define MCBSP2_TX MCBSP2_DMA_TX
#define MCBSP2_RX MCBSP2_DMA_RX
#define MCBSP3_TX MCBSP3_DMA_TX
#define MCBSP3_RX MCBSP3_DMA_RX
/*
 *	SERIAL PORTS IDs - abe_slimbus_id;
 */
#define SLIMBUS1_TX0  SLIMBUS1_DMA_TX0
#define SLIMBUS1_TX1  SLIMBUS1_DMA_TX1
#define SLIMBUS1_TX2  SLIMBUS1_DMA_TX2
#define SLIMBUS1_TX3  SLIMBUS1_DMA_TX3
#define SLIMBUS1_TX4  SLIMBUS1_DMA_TX4
#define SLIMBUS1_TX5  SLIMBUS1_DMA_TX5
#define SLIMBUS1_TX6  SLIMBUS1_DMA_TX6
#define SLIMBUS1_TX7  SLIMBUS1_DMA_TX7
#define SLIMBUS1_RX0  SLIMBUS1_DMA_RX0
#define SLIMBUS1_RX1  SLIMBUS1_DMA_RX1
#define SLIMBUS1_RX2  SLIMBUS1_DMA_RX2
#define SLIMBUS1_RX3  SLIMBUS1_DMA_RX3
#define SLIMBUS1_RX4  SLIMBUS1_DMA_RX4
#define SLIMBUS1_RX5  SLIMBUS1_DMA_RX5
#define SLIMBUS1_RX6  SLIMBUS1_DMA_RX6
#define SLIMBUS1_RX7  SLIMBUS1_DMA_RX7
#define SLIMBUS_UNUSED  _DUMMY_FIFO_
/*
 *	-----------------   TYPES USED FOR APIS  ---------------
 */

/*
 *	EQU_T
 *
 *	coefficients of the equalizer
 */
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

/*
 *	APS_T
 *
 *	coefficients of the Acoustics Protection and Safety
 */
struct abe_aps_t {
	s32 coef1[NBAPS1];
	s32 coef2[NBAPS2];
};

struct abe_aps_energy_t {
	/* structure of two energy_t estimation for coil and membrane */
	u32 e1;
	u32 e2;
};
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

typedef struct {
	/* Offset to the first address of the */
	u32 data;
	/* number of iterations for the DMA data moves. */
	u32 iter;
} abe_dma_t_offset;
/*
 *	SEQ_T
 *
 *	struct {
 *		micros_t time;          Waiting time before executing next line
 *		seq_code_t code         Subroutine index interpreted in the HAL
 *					and translated to FW subroutine codes
 *					in case of ABE tasks
 *		int32 param[2]		Two parameters
 *		} seq_t
 *
 */
typedef struct {
	u32 delta_time;
	u32 code;
	u32 param[4];
	u8 tag;
} abe_seq_t;

typedef struct {
	u32 mask;
	abe_seq_t seq1;
	abe_seq_t seq2;
} abe_sequence_t;
/*
 *	DRIFT_T abe_drift_t = s32
 *
 *	ASRC drift parameter in [ppm] value
 */
/*
 *  --------------------   INTERNAL DATA TYPES  ---------------------
 */
/*
 *	ABE_IRQ_DATA_T
 *
 *	IRQ FIFO content declaration
 *	APS interrupts : IRQ_FIFO[31:28] = IRQtag_APS,
 *		IRQ_FIFO[27:16] = APS_IRQs, IRQ_FIFO[15:0] = loopCounter
 *	SEQ interrupts : IRQ_FIFO[31:28] IRQtag_COUNT,
 *		IRQ_FIFO[27:16] = Count_IRQs, IRQ_FIFO[15:0] = loopCounter
 *	Ping-Pong Interrupts : IRQ_FIFO[31:28] = IRQtag_PP,
 *		IRQ_FIFO[27:16] = PP_MCU_IRQ, IRQ_FIFO[15:0] = loopCounter
 */
typedef struct {
	unsigned int counter:16;
	unsigned int data:12;
	unsigned int tag:4;
} abe_irq_data_t;
/*
 *	ABE_PORT_T status / format / sampling / protocol(call_back) /
 *	features / gain / name ..
 *
 */
typedef struct {
	/* running / idled */
	u16 status;
	/* Sample format type  */
	abe_data_format_t format;
	/* API : for ASRC */
	s32 drift;
	/* optionnal call-back index for errors and ack */
	u16 callback;
	/* IO tasks buffers */
	u16 smem_buffer1;
	u16 smem_buffer2;
	abe_port_protocol_t protocol;
	/* pointer and iteration counter of the xDMA */
	abe_dma_t_offset dma;
	/* list of features associated to a port (EQ, APS, ... , ends with 0) */
	u16 feature_index[MAXFEATUREPORT];
	char name[NBCHARPORTNAME];
} abe_port_t;
/*
 *	ABE_SUBROUTINE_T
 *
 */
typedef struct {
	u32 sub_id;
	s32 param[4];
} abe_subroutine_t;

#endif/* ifndef _ABE_TYP_H_ */
