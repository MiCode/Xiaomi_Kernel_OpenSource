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

#include "abe_legacy.h"

struct omap_abe *abe;

/*
 * HAL/FW ports status / format / sampling / protocol(call_back) / features
 *	/ gain / name
 */
abe_port_t abe_port[LAST_PORT_ID];	/* list of ABE ports */
const abe_port_t abe_port_init[LAST_PORT_ID] = {
	/* Status Data Format Drift Call-Back Protocol+selector desc_addr;
	   buf_addr; buf_size; iter; irq_addr irq_data DMA_T $Features
	   reseted at start Port Name for the debug trace */
	/* DMIC */ {
		    OMAP_ABE_PORT_ACTIVITY_IDLE, {96000, SIX_MSB},
		    NODRIFT, NOCALLBACK, 0, (DMIC_ITER/6),
		    {
		     SNK_P, DMIC_PORT_PROT,
		     {{dmem_dmic, dmem_dmic_size, DMIC_ITER} }
		     },
		    {0, 0},
		    {EQDMIC, 0}, "DMIC"},
	/* PDM_UL */ {
		      OMAP_ABE_PORT_ACTIVITY_IDLE, {96000, STEREO_MSB},
		      NODRIFT, NOCALLBACK, smem_amic, (MCPDM_UL_ITER/2),
		      {
		       SNK_P, MCPDMUL_PORT_PROT,
		       {{dmem_amic, dmem_amic_size, MCPDM_UL_ITER} }
		       },
		      {0, 0},
		      {EQAMIC, 0}, "PDM_UL"},
	/* BT_VX_UL */ {
			OMAP_ABE_PORT_ACTIVITY_IDLE, {8000, STEREO_MSB},
			NODRIFT, NOCALLBACK, smem_bt_vx_ul_opp50, 1,
			{
			 SNK_P, SERIAL_PORT_PROT, {{
						   (MCBSP1_DMA_TX*ATC_SIZE),
						   dmem_bt_vx_ul,
						   dmem_bt_vx_ul_size,
						   (1*SCHED_LOOP_8kHz)
						   } }
			 },
			{0, 0}, {0}, "BT_VX_UL"},
	/* MM_UL */ {
		     OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
		     NODRIFT, NOCALLBACK, smem_mm_ul, 1,
		     {
		      SRC_P, DMAREQ_PORT_PROT, {{
						(CBPr_DMA_RTX3*ATC_SIZE),
						dmem_mm_ul, dmem_mm_ul_size,
						(10*SCHED_LOOP_48kHz),
						ABE_DMASTATUS_RAW, (1 << 3)
						} }
		      },
		     {CIRCULAR_BUFFER_PERIPHERAL_R__3, 120},
		     {UPROUTE, 0}, "MM_UL"},
	/* MM_UL2 */ {
		      OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
		      NODRIFT, NOCALLBACK, smem_mm_ul2, 1,
		      {
		       SRC_P, DMAREQ_PORT_PROT, {{
						 (CBPr_DMA_RTX4*ATC_SIZE),
						 dmem_mm_ul2, dmem_mm_ul2_size,
						 (2*SCHED_LOOP_48kHz),
						 ABE_DMASTATUS_RAW, (1 << 4)
						 } }
		       },
		      {CIRCULAR_BUFFER_PERIPHERAL_R__4, 24},
		      {UPROUTE, 0}, "MM_UL2"},
	/* VX_UL */ {
		     OMAP_ABE_PORT_ACTIVITY_IDLE, {8000, MONO_MSB},
		     NODRIFT, NOCALLBACK, smem_vx_ul, 1,
		     {
		      SRC_P, DMAREQ_PORT_PROT, {{
						(CBPr_DMA_RTX2*ATC_SIZE),
						dmem_vx_ul, dmem_vx_ul_size,
						(1*SCHED_LOOP_8kHz),
						ABE_DMASTATUS_RAW, (1 << 2)
						} }
		      }, {
			  CIRCULAR_BUFFER_PERIPHERAL_R__2, 2},
		     {ASRC2, 0}, "VX_UL"},
	/* MM_DL */ {
		     OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
		     NODRIFT, NOCALLBACK, smem_mm_dl, 1,
		     {
		      SNK_P, PINGPONG_PORT_PROT, {{
						  (CBPr_DMA_RTX0*ATC_SIZE),
						  dmem_mm_dl, dmem_mm_dl_size,
						  (2*SCHED_LOOP_48kHz),
						  ABE_DMASTATUS_RAW, (1 << 0)
						  } }
		      },
		     {CIRCULAR_BUFFER_PERIPHERAL_R__0, 24},
		     {ASRC3, 0}, "MM_DL"},
	/* VX_DL */ {
		     OMAP_ABE_PORT_ACTIVITY_IDLE, {8000, MONO_MSB},
		     NODRIFT, NOCALLBACK, smem_vx_dl, 1,
		     {
		      SNK_P, DMAREQ_PORT_PROT, {{
						(CBPr_DMA_RTX1*ATC_SIZE),
						dmem_vx_dl, dmem_vx_dl_size,
						(1*SCHED_LOOP_8kHz),
						ABE_DMASTATUS_RAW, (1 << 1)
						} }
		      },
		     {CIRCULAR_BUFFER_PERIPHERAL_R__1, 2},
		     {ASRC1, 0}, "VX_DL"},
	/* TONES_DL */ {
			OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
			NODRIFT, NOCALLBACK, smem_tones_dl, 1,
			{
			 SNK_P, DMAREQ_PORT_PROT, {{
						   (CBPr_DMA_RTX5*ATC_SIZE),
						   dmem_tones_dl,
						   dmem_tones_dl_size,
						   (2*SCHED_LOOP_48kHz),
						   ABE_DMASTATUS_RAW, (1 << 5)
						   } }
			 },
			{CIRCULAR_BUFFER_PERIPHERAL_R__5, 24},
			{0}, "TONES_DL"},
	/* VIB_DL */ {
		      OMAP_ABE_PORT_ACTIVITY_IDLE, {24000, STEREO_MSB},
		      NODRIFT, NOCALLBACK, smem_vib, 1,
		      {
		       SNK_P, DMAREQ_PORT_PROT, {{
						 (CBPr_DMA_RTX6*ATC_SIZE),
						 dmem_vib_dl, dmem_vib_dl_size,
						 (2*SCHED_LOOP_24kHz),
						 ABE_DMASTATUS_RAW, (1 << 6)
						 } }
		       },
		      {CIRCULAR_BUFFER_PERIPHERAL_R__6, 12},
		      {0}, "VIB_DL"},
	/* BT_VX_DL */ {
			OMAP_ABE_PORT_ACTIVITY_IDLE, {8000, MONO_MSB},
			NODRIFT, NOCALLBACK, smem_bt_vx_dl_opp50, 1,
			{
			 SRC_P, SERIAL_PORT_PROT, {{
						   (MCBSP1_DMA_RX*ATC_SIZE),
						   dmem_bt_vx_dl,
						   dmem_bt_vx_dl_size,
						   (1*SCHED_LOOP_8kHz),
						   } }
			 },
			{0, 0}, {0}, "BT_VX_DL"},
	/* PDM_DL */ {
		      OMAP_ABE_PORT_ACTIVITY_IDLE, {96000, SIX_MSB},
		      NODRIFT, NOCALLBACK, 0, (MCPDM_DL_ITER/6),
		      {SRC_P, MCPDMDL_PORT_PROT, {{dmem_mcpdm,
						dmem_mcpdm_size} } },
		      {0, 0},
		      {MIXDL1, EQ1, APS1, MIXDL2, EQ2L, EQ2R, APS2L, APS2R, 0},
		      "PDM_DL"},
	/* MM_EXT_OUT */
	{
	 OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
	 NODRIFT, NOCALLBACK, smem_mm_ext_out, 1,
	 {
	  SRC_P, SERIAL_PORT_PROT, {{
				    (MCBSP1_DMA_TX*ATC_SIZE),
				    dmem_mm_ext_out, dmem_mm_ext_out_size,
				    (2*SCHED_LOOP_48kHz)
				    } }
	  }, {0, 0}, {0}, "MM_EXT_OUT"},
	/* MM_EXT_IN */
	{
	 OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
	 NODRIFT, NOCALLBACK, smem_mm_ext_in_opp100, 1,
	 {
	  SNK_P, SERIAL_PORT_PROT, {{
				    (MCBSP1_DMA_RX*ATC_SIZE),
				    dmem_mm_ext_in, dmem_mm_ext_in_size,
				    (2*SCHED_LOOP_48kHz)
				    } }
	  },
	 {0, 0}, {0}, "MM_EXT_IN"},
	/* PCM3_TX */ {
		       OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
		       NODRIFT, NOCALLBACK, 0, 1,
		       {
			SRC_P, TDM_SERIAL_PORT_PROT, {{
						      (MCBSP3_DMA_TX *
						       ATC_SIZE),
						      dmem_mm_ext_out,
						      dmem_mm_ext_out_size,
						      (2*SCHED_LOOP_48kHz)
						      } }
			},
		       {0, 0}, {0}, "TDM_OUT"},
	/* PCM3_RX */ {
		       OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, STEREO_MSB},
		       NODRIFT, NOCALLBACK, 0, 1,
		       {
			SRC_P, TDM_SERIAL_PORT_PROT, {{
						      (MCBSP3_DMA_RX *
						       ATC_SIZE),
						      dmem_mm_ext_in,
						      dmem_mm_ext_in_size,
						      (2*SCHED_LOOP_48kHz)
						      } }
			},
		       {0, 0}, {0}, "TDM_IN"},
	/* SCHD_DBG_PORT */ {
			     OMAP_ABE_PORT_ACTIVITY_IDLE, {48000, MONO_MSB},
			     NODRIFT, NOCALLBACK, 0, 1,
			     {
			      SRC_P, DMAREQ_PORT_PROT, {{
							(CBPr_DMA_RTX7 *
							 ATC_SIZE),
							dmem_mm_trace,
							dmem_mm_trace_size,
							(2*SCHED_LOOP_48kHz),
							ABE_DMASTATUS_RAW,
							(1 << 4)
							} }
			      }, {CIRCULAR_BUFFER_PERIPHERAL_R__7, 24},
			     {FEAT_SEQ, FEAT_CTL, FEAT_GAINS, 0}, "SCHD_DBG"},
};
/*
 * AESS/ATC destination and source address translation (except McASPs)
 * from the original 64bits words address
 */
const u32 abe_atc_dstid[ABE_ATC_DESC_SIZE >> 3] = {
	/* DMA_0 DMIC PDM_DL PDM_UL McB1TX McB1RX McB2TX McB2RX 0 .. 7 */
	0, 0, 12, 0, 1, 0, 2, 0,
	/* McB3TX McB3RX SLIMT0 SLIMT1 SLIMT2 SLIMT3 SLIMT4 SLIMT5 8 .. 15 */
	3, 0, 4, 5, 6, 7, 8, 9,
	/* SLIMT6 SLIMT7 SLIMR0 SLIMR1 SLIMR2 SLIMR3 SLIMR4 SLIMR5 16 .. 23 */
	10, 11, 0, 0, 0, 0, 0, 0,
	/* SLIMR6 SLIMR7 McASP1X ----- ----- McASP1R ----- ----- 24 .. 31 */
	0, 0, 14, 0, 0, 0, 0, 0,
	/* CBPrT0 CBPrT1 CBPrT2 CBPrT3 CBPrT4 CBPrT5 CBPrT6 CBPrT7 32 .. 39 */
	63, 63, 63, 63, 63, 63, 63, 63,
	/* CBP_T0 CBP_T1 CBP_T2 CBP_T3 CBP_T4 CBP_T5 CBP_T6 CBP_T7 40 .. 47 */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* CBP_T8 CBP_T9 CBP_T10 CBP_T11 CBP_T12 CBP_T13 CBP_T14
	   CBP_T15 48 .. 63 */
	0, 0, 0, 0, 0, 0, 0, 0,
};
const u32 abe_atc_srcid[ABE_ATC_DESC_SIZE >> 3] = {
	/* DMA_0 DMIC PDM_DL PDM_UL McB1TX McB1RX McB2TX McB2RX 0 .. 7 */
	0, 12, 0, 13, 0, 1, 0, 2,
	/* McB3TX McB3RX SLIMT0 SLIMT1 SLIMT2 SLIMT3 SLIMT4 SLIMT5 8 .. 15 */
	0, 3, 0, 0, 0, 0, 0, 0,
	/* SLIMT6 SLIMT7 SLIMR0 SLIMR1 SLIMR2 SLIMR3 SLIMR4 SLIMR5 16 .. 23 */
	0, 0, 4, 5, 6, 7, 8, 9,
	/* SLIMR6 SLIMR7 McASP1X ----- ----- McASP1R ----- ----- 24 .. 31 */
	10, 11, 0, 0, 0, 14, 0, 0,
	/* CBPrT0 CBPrT1 CBPrT2 CBPrT3 CBPrT4 CBPrT5 CBPrT6 CBPrT7 32 .. 39 */
	63, 63, 63, 63, 63, 63, 63, 63,
	/* CBP_T0 CBP_T1 CBP_T2 CBP_T3 CBP_T4 CBP_T5 CBP_T6 CBP_T7 40 .. 47 */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* CBP_T8 CBP_T9 CBP_T10 CBP_T11 CBP_T12 CBP_T13 CBP_T14
	   CBP_T15 48 .. 63 */
	0, 0, 0, 0, 0, 0, 0, 0,
};
/*
 * preset default routing configurations
 * This is given as implementation EXAMPLES
 * the programmer uses "abe_set_router_configuration" with its own tables
	*/
const abe_router_t abe_router_ul_table_preset[NBROUTE_CONFIG][NBROUTE_UL] = {
	/* VOICE UPLINK WITH PHOENIX MICROPHONES - UPROUTE_CONFIG_AMIC */
	{
	/* 0 .. 9 = MM_UL */
	 DMIC1_L_labelID, DMIC1_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
	 MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID, AMIC_L_labelID,
	 AMIC_L_labelID,
	 ZERO_labelID, ZERO_labelID,
	/* 10 .. 11 = MM_UL2 */
	 AMIC_L_labelID, AMIC_L_labelID,
	/* 12 .. 13 = VX_UL */
	 AMIC_L_labelID, AMIC_R_labelID,
	/* 14 .. 15 = RESERVED */
	 ZERO_labelID, ZERO_labelID,
	 },
	/* VOICE UPLINK WITH THE FIRST DMIC PAIR - UPROUTE_CONFIG_DMIC1 */
	{
	/* 0 .. 9 = MM_UL */
	 DMIC2_L_labelID, DMIC2_R_labelID, DMIC3_L_labelID, DMIC3_R_labelID,
	 DMIC1_L_labelID, DMIC1_R_labelID, ZERO_labelID, ZERO_labelID,
	 ZERO_labelID, ZERO_labelID,
	/* 10 .. 11 = MM_UL2 */
	 DMIC1_L_labelID, DMIC1_R_labelID,
	/* 12 .. 13 = VX_UL */
	 DMIC1_L_labelID, DMIC1_R_labelID,
	/* 14 .. 15 = RESERVED */
	 ZERO_labelID, ZERO_labelID,
	 },
	/* VOICE UPLINK WITH THE SECOND DMIC PAIR - UPROUTE_CONFIG_DMIC2 */
	{
	/* 0 .. 9 = MM_UL */
	 DMIC3_L_labelID, DMIC3_R_labelID, DMIC1_L_labelID, DMIC1_R_labelID,
	 DMIC2_L_labelID, DMIC2_R_labelID, ZERO_labelID, ZERO_labelID,
	 ZERO_labelID, ZERO_labelID,
	/* 10 .. 11 = MM_UL2 */
	 DMIC2_L_labelID, DMIC2_R_labelID,
	/* 12 .. 13 = VX_UL */
	 DMIC2_L_labelID, DMIC2_R_labelID,
	/* 14 .. 15 = RESERVED */
	 ZERO_labelID, ZERO_labelID,
	 },
	/* VOICE UPLINK WITH THE LAST DMIC PAIR - UPROUTE_CONFIG_DMIC3 */
	{
	/* 0 .. 9 = MM_UL */
	 AMIC_L_labelID, AMIC_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
	 DMIC3_L_labelID, DMIC3_R_labelID, ZERO_labelID, ZERO_labelID,
	 ZERO_labelID, ZERO_labelID,
	/* 10 .. 11 = MM_UL2 */
	 DMIC3_L_labelID, DMIC3_R_labelID,
	/* 12 .. 13 = VX_UL */
	 DMIC3_L_labelID, DMIC3_R_labelID,
	/* 14 .. 15 = RESERVED */
	 ZERO_labelID, ZERO_labelID,
	 },
	/* VOICE UPLINK WITH THE BT - UPROUTE_CONFIG_BT */
	{
	/* 0 .. 9 = MM_UL */
	 BT_UL_L_labelID, BT_UL_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
	 DMIC3_L_labelID, DMIC3_R_labelID, DMIC1_L_labelID, DMIC1_R_labelID,
	 ZERO_labelID, ZERO_labelID,
	/* 10 .. 11 = MM_UL2 */
	 AMIC_L_labelID, AMIC_R_labelID,
	/* 12 .. 13 = VX_UL */
	 BT_UL_L_labelID, BT_UL_R_labelID,
	/* 14 .. 15 = RESERVED */
	 ZERO_labelID, ZERO_labelID,
	 },
	/* VOICE UPLINK WITH THE BT - UPROUTE_ECHO_MMUL2 */
	{
	/* 0 .. 9 = MM_UL */
	 MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID, BT_UL_L_labelID,
	 BT_UL_R_labelID, AMIC_L_labelID, AMIC_R_labelID,
	 ZERO_labelID, ZERO_labelID, ZERO_labelID, ZERO_labelID,
	/* 10 .. 11 = MM_UL2 */
	 EchoRef_L_labelID, EchoRef_R_labelID,
	/* 12 .. 13 = VX_UL */
	 AMIC_L_labelID, AMIC_L_labelID,
	/* 14 .. 15 = RESERVED */
	 ZERO_labelID, ZERO_labelID,
	 },
};
/* all default routing configurations */
abe_router_t abe_router_ul_table[NBROUTE_CONFIG_MAX][NBROUTE_UL];

const abe_sequence_t seq_null = {
	NOMASK, {CL_M1, 0, {0, 0, 0, 0}, 0}, {CL_M1, 0, {0, 0, 0, 0}, 0}
};
/* table of new subroutines called in the sequence */
abe_subroutine2 abe_all_subsubroutine[MAXNBSUBROUTINE];
/* number of parameters per calls */
u32 abe_all_subsubroutine_nparam[MAXNBSUBROUTINE];
/* index of the subroutine */
u32 abe_subroutine_id[MAXNBSUBROUTINE];
/* paramters of the subroutine (if any) */
u32 *abe_all_subroutine_params[MAXNBSUBROUTINE];
u32 abe_subroutine_write_pointer;
/* table of all sequences */
abe_sequence_t abe_all_sequence[MAXNBSEQUENCE];
u32 abe_sequence_write_pointer;
/* current number of pending sequences (avoids to look in the table) */
u32 abe_nb_pending_sequences;
/* pending sequences due to ressource collision */
u32 abe_pending_sequences[MAXNBSEQUENCE];
/* mask of unsharable ressources among other sequences */
u32 abe_global_sequence_mask;
/* table of active sequences */
abe_seq_t abe_active_sequence[MAXACTIVESEQUENCE][MAXSEQUENCESTEPS];
/* index of the plugged subroutine doing ping-pong cache-flush DMEM accesses */
u32 abe_irq_pingpong_player_id;
EXPORT_SYMBOL(abe_irq_pingpong_player_id);
/* index of the plugged subroutine doing acoustics protection adaptation */
u32 abe_irq_aps_adaptation_id;
/* base addresses of the ping pong buffers in bytes addresses */
u32 abe_base_address_pingpong[MAX_PINGPONG_BUFFERS];
/* size of each ping/pong buffers */
u32 abe_size_pingpong;
/* number of ping/pong buffer being used */
u32 abe_nb_pingpong;
/*
 * MAIN PORT SELECTION
 */
const u32 abe_port_priority[LAST_PORT_ID - 1] = {
	OMAP_ABE_PDM_DL_PORT,
	OMAP_ABE_PDM_UL_PORT,
	OMAP_ABE_MM_EXT_OUT_PORT,
	OMAP_ABE_MM_EXT_IN_PORT,
	OMAP_ABE_DMIC_PORT,
	OMAP_ABE_MM_UL_PORT,
	OMAP_ABE_MM_UL2_PORT,
	OMAP_ABE_MM_DL_PORT,
	OMAP_ABE_TONES_DL_PORT,
	OMAP_ABE_VX_UL_PORT,
	OMAP_ABE_VX_DL_PORT,
	OMAP_ABE_BT_VX_DL_PORT,
	OMAP_ABE_BT_VX_UL_PORT,
	OMAP_ABE_VIB_DL_PORT,
};
