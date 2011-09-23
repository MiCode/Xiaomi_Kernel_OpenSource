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

#ifndef _ABE_GAIN_H_
#define _ABE_GAIN_H_

#include "abe_typ.h"
#include "abe_dm_addr.h"
#include "abe_sm_addr.h"
#include "abe_cm_addr.h"

#define OMAP_ABE_GAIN_MUTED     (0x0001<<0)
#define OMAP_ABE_GAIN_DISABLED  (0x0001<<1)

#define OMAP_ABE_GAIN_DMIC1_LEFT    0
#define OMAP_ABE_GAIN_DMIC1_RIGTH   1
#define OMAP_ABE_GAIN_DMIC2_LEFT    2
#define OMAP_ABE_GAIN_DMIC2_RIGTH   3
#define OMAP_ABE_GAIN_DMIC3_LEFT    4
#define OMAP_ABE_GAIN_DMIC3_RIGTH   5
#define OMAP_ABE_GAIN_AMIC_LEFT     6
#define OMAP_ABE_GAIN_AMIC_RIGTH    7
#define OMAP_ABE_GAIN_DL1_LEFT      8
#define OMAP_ABE_GAIN_DL1_RIGTH     9
#define OMAP_ABE_GAIN_DL2_LEFT     10
#define OMAP_ABE_GAIN_DL2_RIGTH    11
#define OMAP_ABE_GAIN_SPLIT_LEFT   12
#define OMAP_ABE_GAIN_SPLIT_RIGTH  13
#define OMAP_ABE_MIXDL1_MM_DL      14
#define OMAP_ABE_MIXDL1_MM_UL2     15
#define OMAP_ABE_MIXDL1_VX_DL      16
#define OMAP_ABE_MIXDL1_TONES      17
#define OMAP_ABE_MIXDL2_MM_DL      18
#define OMAP_ABE_MIXDL2_MM_UL2     19
#define OMAP_ABE_MIXDL2_VX_DL      20
#define OMAP_ABE_MIXDL2_TONES      21
#define OMAP_ABE_MIXECHO_DL1       22
#define OMAP_ABE_MIXECHO_DL2       23
#define OMAP_ABE_MIXSDT_UL         24
#define OMAP_ABE_MIXECHO_DL        25
#define OMAP_ABE_MIXVXREC_MM_DL    26
#define OMAP_ABE_MIXVXREC_TONES    27
#define OMAP_ABE_MIXVXREC_VX_UL    28
#define OMAP_ABE_MIXVXREC_VX_DL    29
#define OMAP_ABE_MIXAUDUL_MM_DL    30
#define OMAP_ABE_MIXAUDUL_TONES    31
#define OMAP_ABE_MIXAUDUL_UPLINK   32
#define OMAP_ABE_MIXAUDUL_VX_DL    33
#define OMAP_ABE_GAIN_BTUL_LEFT    34
#define OMAP_ABE_GAIN_BTUL_RIGTH   35

void omap_abe_reset_gain_mixer(struct omap_abe *abe, u32 id, u32 p);

void abe_int_2_float16(u32 data, u32 *mantissa, u32 *exp);

#endif /* _ABE_GAIN_H_ */
