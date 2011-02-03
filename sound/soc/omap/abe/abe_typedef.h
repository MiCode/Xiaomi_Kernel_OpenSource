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

#ifndef _ABE_TYPEDEF_H_
#define _ABE_TYPEDEF_H_

#include "abe_define.h"
#include "abe_typ.h"

/*
 * Basic types definition
 */
/*
 * Commonly used structures
 */
typedef struct abetaskTag {
	/* 0 ... Index of called function */
	u16 iF;
	/* 2 ... for INITPTR of A0 */
	u16 A0;
	/* 4 ... for INITPTR of A1 */
	u16 A1;
	/* 6 ... for INITPTR of A2 & A3 */
	u16 A2_3;
	/* 8 ... for INITPTR of A4 & A5 */
	u16 A4_5;
	/* 10 ... for INITREG of R0, R1, R2, R3 */
	u16 R;
	/* 12 */
	u16 misc0;
	/* 14 */
	u16 misc1;
} ABE_STask;
typedef ABE_STask *pABE_STask;
typedef ABE_STask **ppABE_STask;

struct ABE_SIODescriptor {
	/* 0 */
	u16 drift_ASRC;
	/* 2 */
	u16 drift_io;
	/* 4 "Function index" of XLS sheet "Functions" */
	u8 io_type_idx;
	/* 5 1 = MONO or Stereo1616, 2= STEREO, ... */
	u8 samp_size;
	/* 6 drift "issues" for ASRC */
	s16 flow_counter;
	/* 8 address for IRQ or DMArequests */
	u16 hw_ctrl_addr;
	/* 10 DMA request bit-field or IRQ (DSP/MCU) */
	u8 atc_irq_data;
	/* 11 0 = Read, 3 = Write */
	u8 direction_rw;
	/* 12 */
	u8 repeat_last_samp;
	/* 13 12 at 48kHz, ... */
	u8 nsamp;
	/* 14 nsamp x samp_size */
	u8 x_io;
	/* 15 ON = 0x80, OFF = 0x00 */
	u8 on_off;
	/* 16 For Slimbus and TDM purpose */
	u16 split_addr1;
	/* 18 */
	u16 split_addr2;
	/* 20 */
	u16 split_addr3;
	/* 22 */
	u8 before_f_index;
	/* 23 */
	u8 after_f_index;
	/* 24 SM/CM INITPTR field */
	u16 smem_addr1;
	/* 26 in bytes */
	u16 atc_address1;
	/* 28 DMIC_ATC_PTR, MCPDM_UL_ATC_PTR, ... */
	u16 atc_pointer_saved1;
	/* 30 samp_size (except in TDM or Slimbus) */
	u8 data_size1;
	/* 31 "Function index" of XLS sheet "Functions" */
	u8 copy_f_index1;
	/* 32 For Slimbus and TDM purpose */
	u16 smem_addr2;
	/* 34 */
	u16 atc_address2;
	/* 36 */
	u16 atc_pointer_saved2;
	/* 38 */
	u8 data_size2;
	/* 39 */
	u8 copy_f_index2;
};

/* [w] asrc output used for the next asrc call (+/- 1 / 0) */
#define drift_asrc_ 0
/* [w] asrc output used for controlling the number of samples to be
    exchanged (+/- 1 / 0) */
#define drift_io_ 2
/* address of the IO subroutine */
#define io_type_idx_ 4
#define samp_size_ 5
/* flow error counter */
#define flow_counter_ 6
/* dmareq address or host irq buffer address (atc address) */
#define hw_ctrl_addr_ 8
/* data content to be loaded to "hw_ctrl_addr" */
#define atc_irq_data_ 10
/* read dmem =0, write dmem =3 (atc offset of the access pointer) */
#define direction_rw_ 11
/* flag set to allow repeating the last sample on downlink paths */
#define repeat_last_samp_ 12
/* number of samples (either mono stereo...) */
#define nsamp_ 13
/* x number of raw DMEM data moved */
#define x_io_ 14
#define on_off_ 15
/* internal smem buffer initptr pointer index */
#define split_addr1_ 16
/* internal smem buffer initptr pointer index */
#define split_addr2_ 18
/* internal smem buffer initptr pointer index */
#define split_addr3_ 20
/* index of the copy subroutine */
#define before_f_index_ 22
/* index of the copy subroutine */
#define after_f_index_ 23
#define minidesc1_ 24
/* internal smem buffer initptr pointer index */
#define rel_smem_ 0
/* atc descriptor address (byte address x4) */
#define rel_atc_ 2
/* location of the saved ATC pointer (+debug info) */
#define rel_atc_saved 4
/* size of each sample (1:mono/1616 2:stereo ... ) */
#define rel_size_ 6
/* index of the copy subroutine */
#define rel_f_ 7
#define s_mem_mm_ul 24
#define s_mm_ul_size 30
#define minidesc2_ 32
#define Struct_Size 40

struct ABE_SPingPongDescriptor {
	/* 0: [W] asrc output used for the next ASRC call (+/- 1 / 0) */
	u16 drift_ASRC;
	/* 2: [W] asrc output used for controlling the number of
	   samples to be exchanged (+/- 1 / 0) */
	u16 drift_io;
	/* 4: DMAReq address or HOST IRQ buffer address (ATC ADDRESS) */
	u16 hw_ctrl_addr;
	/* 6: index of the copy subroutine */
	u8 copy_func_index;
	/* 7: X number of SMEM samples to move */
	u8 x_io;
	/* 8: 0 for mono data, 1 for stereo data */
	u8 data_size;
	/* 9: internal SMEM buffer INITPTR pointer index */
	u8 smem_addr;
	/* 10: data content to be loaded to "hw_ctrl_addr" */
	u8 atc_irq_data;
	/* 11: ping/pong buffer flag */
	u8 counter;
	/* 12: current Base address of the working buffer */
	u16 workbuff_BaseAddr;
	/* 14: samples left in the working buffer */
	u16 workbuff_Samples;
	/* 16: Base address of the ping/pong buffer 0 */
	u16 nextbuff0_BaseAddr;
	/* 18: samples available in the ping/pong buffer 0 */
	u16 nextbuff0_Samples;
	/* 20: Base address of the ping/pong buffer 1 */
	u16 nextbuff1_BaseAddr;
	/* 22: samples available in the ping/pong buffer 1 */
	u16 nextbuff1_Samples;
};

#endif/* _ABE_TYPEDEF_H_ */
