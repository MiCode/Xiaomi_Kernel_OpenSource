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

#ifndef _ABE_H_
#define _ABE_H_

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>

#include "abe_def.h"
#include "abe_define.h"
#include "abe_fw.h"
#include "abe_ext.h"
#include "abe_dbg.h"

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

struct omap_abe_equ {
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
};

struct omap_abe {
	void __iomem *io_base[5];
	u32 firmware_version_number;
	u16 MultiFrame[PROCESSING_SLOTS][TASKS_IN_SLOT];
	u32 compensated_mixer_gain;
	u8  muted_gains_indicator[MAX_NBGAIN_CMEM];
	u32 desired_gains_decibel[MAX_NBGAIN_CMEM];
	u32 muted_gains_decibel[MAX_NBGAIN_CMEM];
	u32 desired_gains_linear[MAX_NBGAIN_CMEM];
	u32 desired_ramp_delay_ms[MAX_NBGAIN_CMEM];
	struct mutex mutex;
	u32 warm_boot;

	u32 irq_dbg_read_ptr;

	struct omap_abe_dbg dbg;
};

extern struct omap_abe *abe;

void omap_abe_dbg_log(struct omap_abe *abe, u32 x, u32 y, u32 z, u32 t);
void omap_abe_dbg_error(struct omap_abe *abe, int level, int error);
int omap_abe_set_opp_processing(struct omap_abe *abe, u32 opp);
int omap_abe_connect_debug_trace(struct omap_abe *abe,
				 struct omap_abe_dma *dma2);

int omap_abe_use_compensated_gain(struct omap_abe *abe, int on_off);
int omap_abe_write_equalizer(struct omap_abe *abe,
			     u32 id, struct omap_abe_equ *param);

int omap_abe_disable_gain(struct omap_abe *abe, u32 id, u32 p);
int omap_abe_enable_gain(struct omap_abe *abe, u32 id, u32 p);
int omap_abe_mute_gain(struct omap_abe *abe, u32 id, u32 p);
int omap_abe_unmute_gain(struct omap_abe *abe, u32 id, u32 p);

int omap_abe_write_gain(struct omap_abe *abe,
				u32 id, s32 f_g, u32 ramp, u32 p);
int omap_abe_write_mixer(struct omap_abe *abe,
				u32 id, s32 f_g, u32 f_ramp, u32 p);
int omap_abe_read_gain(struct omap_abe *abe,
				u32 id, u32 *f_g, u32 p);
int omap_abe_read_mixer(struct omap_abe *abe,
				u32 id, u32 *f_g, u32 p);

/*
 * MACROS
 */
#define _log(x, y, z, t) { if (x & abe->dbg.mask) omap_abe_dbg_log(abe, x, y, z, t); }

#endif/* _ABE_H_ */
