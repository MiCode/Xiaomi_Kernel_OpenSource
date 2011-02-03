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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe_dbg.h"
#include "abe.h"
#include "abe_gain.h"
#include "abe_mem.h"

/*
 * ABE CONST AREA FOR PARAMETERS TRANSLATION
 */
#define min_mdb (-12000)
#define max_mdb (3000)
#define sizeof_db2lin_table (1 + ((max_mdb - min_mdb)/100))

const u32 abe_db2lin_table[sizeof_db2lin_table] = {
	0x00000000,		/* SMEM coding of -120 dB */
	0x00000000,		/* SMEM coding of -119 dB */
	0x00000000,		/* SMEM coding of -118 dB */
	0x00000000,		/* SMEM coding of -117 dB */
	0x00000000,		/* SMEM coding of -116 dB */
	0x00000000,		/* SMEM coding of -115 dB */
	0x00000000,		/* SMEM coding of -114 dB */
	0x00000000,		/* SMEM coding of -113 dB */
	0x00000000,		/* SMEM coding of -112 dB */
	0x00000000,		/* SMEM coding of -111 dB */
	0x00000000,		/* SMEM coding of -110 dB */
	0x00000000,		/* SMEM coding of -109 dB */
	0x00000001,		/* SMEM coding of -108 dB */
	0x00000001,		/* SMEM coding of -107 dB */
	0x00000001,		/* SMEM coding of -106 dB */
	0x00000001,		/* SMEM coding of -105 dB */
	0x00000001,		/* SMEM coding of -104 dB */
	0x00000001,		/* SMEM coding of -103 dB */
	0x00000002,		/* SMEM coding of -102 dB */
	0x00000002,		/* SMEM coding of -101 dB */
	0x00000002,		/* SMEM coding of -100 dB */
	0x00000002,		/* SMEM coding of -99 dB */
	0x00000003,		/* SMEM coding of -98 dB */
	0x00000003,		/* SMEM coding of -97 dB */
	0x00000004,		/* SMEM coding of -96 dB */
	0x00000004,		/* SMEM coding of -95 dB */
	0x00000005,		/* SMEM coding of -94 dB */
	0x00000005,		/* SMEM coding of -93 dB */
	0x00000006,		/* SMEM coding of -92 dB */
	0x00000007,		/* SMEM coding of -91 dB */
	0x00000008,		/* SMEM coding of -90 dB */
	0x00000009,		/* SMEM coding of -89 dB */
	0x0000000A,		/* SMEM coding of -88 dB */
	0x0000000B,		/* SMEM coding of -87 dB */
	0x0000000D,		/* SMEM coding of -86 dB */
	0x0000000E,		/* SMEM coding of -85 dB */
	0x00000010,		/* SMEM coding of -84 dB */
	0x00000012,		/* SMEM coding of -83 dB */
	0x00000014,		/* SMEM coding of -82 dB */
	0x00000017,		/* SMEM coding of -81 dB */
	0x0000001A,		/* SMEM coding of -80 dB */
	0x0000001D,		/* SMEM coding of -79 dB */
	0x00000021,		/* SMEM coding of -78 dB */
	0x00000025,		/* SMEM coding of -77 dB */
	0x00000029,		/* SMEM coding of -76 dB */
	0x0000002E,		/* SMEM coding of -75 dB */
	0x00000034,		/* SMEM coding of -74 dB */
	0x0000003A,		/* SMEM coding of -73 dB */
	0x00000041,		/* SMEM coding of -72 dB */
	0x00000049,		/* SMEM coding of -71 dB */
	0x00000052,		/* SMEM coding of -70 dB */
	0x0000005D,		/* SMEM coding of -69 dB */
	0x00000068,		/* SMEM coding of -68 dB */
	0x00000075,		/* SMEM coding of -67 dB */
	0x00000083,		/* SMEM coding of -66 dB */
	0x00000093,		/* SMEM coding of -65 dB */
	0x000000A5,		/* SMEM coding of -64 dB */
	0x000000B9,		/* SMEM coding of -63 dB */
	0x000000D0,		/* SMEM coding of -62 dB */
	0x000000E9,		/* SMEM coding of -61 dB */
	0x00000106,		/* SMEM coding of -60 dB */
	0x00000126,		/* SMEM coding of -59 dB */
	0x0000014A,		/* SMEM coding of -58 dB */
	0x00000172,		/* SMEM coding of -57 dB */
	0x0000019F,		/* SMEM coding of -56 dB */
	0x000001D2,		/* SMEM coding of -55 dB */
	0x0000020B,		/* SMEM coding of -54 dB */
	0x0000024A,		/* SMEM coding of -53 dB */
	0x00000292,		/* SMEM coding of -52 dB */
	0x000002E2,		/* SMEM coding of -51 dB */
	0x0000033C,		/* SMEM coding of -50 dB */
	0x000003A2,		/* SMEM coding of -49 dB */
	0x00000413,		/* SMEM coding of -48 dB */
	0x00000492,		/* SMEM coding of -47 dB */
	0x00000521,		/* SMEM coding of -46 dB */
	0x000005C2,		/* SMEM coding of -45 dB */
	0x00000676,		/* SMEM coding of -44 dB */
	0x0000073F,		/* SMEM coding of -43 dB */
	0x00000822,		/* SMEM coding of -42 dB */
	0x00000920,		/* SMEM coding of -41 dB */
	0x00000A3D,		/* SMEM coding of -40 dB */
	0x00000B7D,		/* SMEM coding of -39 dB */
	0x00000CE4,		/* SMEM coding of -38 dB */
	0x00000E76,		/* SMEM coding of -37 dB */
	0x0000103A,		/* SMEM coding of -36 dB */
	0x00001235,		/* SMEM coding of -35 dB */
	0x0000146E,		/* SMEM coding of -34 dB */
	0x000016EC,		/* SMEM coding of -33 dB */
	0x000019B8,		/* SMEM coding of -32 dB */
	0x00001CDC,		/* SMEM coding of -31 dB */
	0x00002061,		/* SMEM coding of -30 dB */
	0x00002455,		/* SMEM coding of -29 dB */
	0x000028C4,		/* SMEM coding of -28 dB */
	0x00002DBD,		/* SMEM coding of -27 dB */
	0x00003352,		/* SMEM coding of -26 dB */
	0x00003995,		/* SMEM coding of -25 dB */
	0x0000409C,		/* SMEM coding of -24 dB */
	0x0000487E,		/* SMEM coding of -23 dB */
	0x00005156,		/* SMEM coding of -22 dB */
	0x00005B43,		/* SMEM coding of -21 dB */
	0x00006666,		/* SMEM coding of -20 dB */
	0x000072E5,		/* SMEM coding of -19 dB */
	0x000080E9,		/* SMEM coding of -18 dB */
	0x000090A4,		/* SMEM coding of -17 dB */
	0x0000A24B,		/* SMEM coding of -16 dB */
	0x0000B618,		/* SMEM coding of -15 dB */
	0x0000CC50,		/* SMEM coding of -14 dB */
	0x0000E53E,		/* SMEM coding of -13 dB */
	0x00010137,		/* SMEM coding of -12 dB */
	0x0001209A,		/* SMEM coding of -11 dB */
	0x000143D1,		/* SMEM coding of -10 dB */
	0x00016B54,		/* SMEM coding of -9 dB */
	0x000197A9,		/* SMEM coding of -8 dB */
	0x0001C967,		/* SMEM coding of -7 dB */
	0x00020137,		/* SMEM coding of -6 dB */
	0x00023FD6,		/* SMEM coding of -5 dB */
	0x00028619,		/* SMEM coding of -4 dB */
	0x0002D4EF,		/* SMEM coding of -3 dB */
	0x00032D64,		/* SMEM coding of -2 dB */
	0x000390A4,		/* SMEM coding of -1 dB */
	0x00040000,		/* SMEM coding of 0 dB */
	0x00047CF2,		/* SMEM coding of 1 dB */
	0x00050923,		/* SMEM coding of 2 dB */
	0x0005A670,		/* SMEM coding of 3 dB */
	0x000656EE,		/* SMEM coding of 4 dB */
	0x00071CF5,		/* SMEM coding of 5 dB */
	0x0007FB26,		/* SMEM coding of 6 dB */
	0x0008F473,		/* SMEM coding of 7 dB */
	0x000A0C2B,		/* SMEM coding of 8 dB */
	0x000B4606,		/* SMEM coding of 9 dB */
	0x000CA62C,		/* SMEM coding of 10 dB */
	0x000E314A,		/* SMEM coding of 11 dB */
	0x000FEC9E,		/* SMEM coding of 12 dB */
	0x0011DE0A,		/* SMEM coding of 13 dB */
	0x00140C28,		/* SMEM coding of 14 dB */
	0x00167E60,		/* SMEM coding of 15 dB */
	0x00193D00,		/* SMEM coding of 16 dB */
	0x001C515D,		/* SMEM coding of 17 dB */
	0x001FC5EB,		/* SMEM coding of 18 dB */
	0x0023A668,		/* SMEM coding of 19 dB */
	0x00280000,		/* SMEM coding of 20 dB */
	0x002CE178,		/* SMEM coding of 21 dB */
	0x00325B65,		/* SMEM coding of 22 dB */
	0x00388062,		/* SMEM coding of 23 dB */
	0x003F654E,		/* SMEM coding of 24 dB */
	0x00472194,		/* SMEM coding of 25 dB */
	0x004FCF7C,		/* SMEM coding of 26 dB */
	0x00598C81,		/* SMEM coding of 27 dB */
	0x006479B7,		/* SMEM coding of 28 dB */
	0x0070BC3D,		/* SMEM coding of 29 dB */
	0x007E7DB9,		/* SMEM coding of 30 dB */
};

const u32 abe_1_alpha_iir[64] = {
	0x040002, 0x040002, 0x040002, 0x040002,	/* 0 */
	0x50E955, 0x48CA65, 0x40E321, 0x72BE78,	/* 1 [ms] */
	0x64BA68, 0x57DF14, 0x4C3D60, 0x41D690,	/* 2 */
	0x38A084, 0x308974, 0x297B00, 0x235C7C,	/* 4 */
	0x1E14B0, 0x198AF0, 0x15A800, 0x125660,	/* 8 */
	0x0F82A0, 0x0D1B5C, 0x0B113C, 0x0956CC,	/* 16 */
	0x07E054, 0x06A3B8, 0x059844, 0x04B680,	/* 32 */
	0x03F80C, 0x035774, 0x02D018, 0x025E0C,	/* 64 */
	0x7F8057, 0x6B482F, 0x5A4297, 0x4BEECB,	/* 128 */
	0x3FE00B, 0x35BAA7, 0x2D3143, 0x2602AF,	/* 256 */
	0x1FF803, 0x1AE2FB, 0x169C9F, 0x13042B,	/* 512 */
	0x0FFE03, 0x0D72E7, 0x0B4F4F, 0x0982CB,	/* 1.024 [s] */
	0x07FF83, 0x06B9CF, 0x05A7E7, 0x04C193,	/* 2.048 */
	0x03FFE3, 0x035CFF, 0x02D403, 0x0260D7,	/* 4.096 */
	0x01FFFB, 0x01AE87, 0x016A07, 0x01306F,	/* 8.192 */
	0x00FFFF, 0x00D743, 0x00B503, 0x009837,
};

const u32 abe_alpha_iir[64] = {
	0x000000, 0x000000, 0x000000, 0x000000,	/* 0 */
	0x5E2D58, 0x6E6B3C, 0x7E39C0, 0x46A0C5,	/* 1 [ms] */
	0x4DA2CD, 0x541079, 0x59E151, 0x5F14B9,	/* 2 */
	0x63AFC1, 0x67BB45, 0x6B4281, 0x6E51C1,	/* 4 */
	0x70F5A9, 0x733A89, 0x752C01, 0x76D4D1,	/* 8 */
	0x783EB1, 0x797251, 0x7A7761, 0x7B549D,	/* 16 */
	0x7C0FD5, 0x7CAE25, 0x7D33DD, 0x7DA4C1,	/* 32 */
	0x7E03FD, 0x7E5449, 0x7E97F5, 0x7ED0F9,	/* 64 */
	0x7F0101, 0x7F2971, 0x7F4B7D, 0x7F6825,	/* 128 */
	0x7F8041, 0x7F948D, 0x7FA59D, 0x7FB3FD,	/* 256 */
	0x7FC011, 0x7FCA3D, 0x7FD2C9, 0x7FD9F9,	/* 512 */
	0x7FE005, 0x7FE51D, 0x7FE961, 0x7FECFD,	/* 1.024 [s] */
	0x7FF001, 0x7FF28D, 0x7FF4B1, 0x7FF67D,	/* 2.048 */
	0x7FF801, 0x7FF949, 0x7FFA59, 0x7FFB41,	/* 4.096 */
	0x7FFC01, 0x7FFCA5, 0x7FFD2D, 0x7FFDA1,	/* 8.192 */
	0x7FFE01, 0x7FFE51, 0x7FFE95, 0x7FFED1,
};

/**
 * abe_int_2_float
 * returns a mantissa on 16 bits and the exponent
 * 0x4000.0000 leads to M=0x4000 X=15
 * 0x0004.0000 leads to M=0x4000 X=4
 * 0x0000.0001 leads to M=0x4000 X=-14
 *
 */
void abe_int_2_float16(u32 data, u32 *mantissa, u32 *exp)
{
	u32 i;
	*exp = 0;
	*mantissa = 0;
	for (i = 0; i < 32; i++) {
		if ((1 << i) > data)
			break;
	}
	*exp = i - 15;
	*mantissa = (*exp > 0) ? data >> (*exp) : data << (*exp);
}

/**
 * abe_use_compensated_gain
 * @on_off:
 *
 * Selects the automatic Mixer's gain management
 * on_off = 1 allows the "abe_write_gain" to adjust the overall
 * gains of the mixer to be tuned not to create saturation
 */
int omap_abe_use_compensated_gain(struct omap_abe *abe, int on_off)
{
	abe->compensated_mixer_gain = on_off;
	return 0;
}
EXPORT_SYMBOL(omap_abe_use_compensated_gain);

/**
 * omap_abe_gain_offset
 * returns the offset to firmware data structures
 *
 */
void omap_abe_gain_offset(struct omap_abe *abe, u32 id, u32 *mixer_offset)
{
	switch (id) {
	default:
	case GAINS_DMIC1:
		*mixer_offset = dmic1_gains_offset;
		break;
	case GAINS_DMIC2:
		*mixer_offset = dmic2_gains_offset;
		break;
	case GAINS_DMIC3:
		*mixer_offset = dmic3_gains_offset;
		break;
	case GAINS_AMIC:
		*mixer_offset = amic_gains_offset;
		break;
	case GAINS_DL1:
		*mixer_offset = dl1_gains_offset;
		break;
	case GAINS_DL2:
		*mixer_offset = dl2_gains_offset;
		break;
	case GAINS_SPLIT:
		*mixer_offset = splitters_gains_offset;
		break;
	case MIXDL1:
		*mixer_offset = mixer_dl1_offset;
		break;
	case MIXDL2:
		*mixer_offset = mixer_dl2_offset;
		break;
	case MIXECHO:
		*mixer_offset = mixer_echo_offset;
		break;
	case MIXSDT:
		*mixer_offset = mixer_sdt_offset;
		break;
	case MIXVXREC:
		*mixer_offset = mixer_vxrec_offset;
		break;
	case MIXAUDUL:
		*mixer_offset = mixer_audul_offset;
		break;
	case GAINS_BTUL:
		*mixer_offset = btul_gains_offset;
		break;
	}
}

/**
 * oamp_abe_write_equalizer
 * @abe: Pointer on abe handle
 * @id: name of the equalizer
 * @param : equalizer coefficients
 *
 * Load the coefficients in CMEM.
 */
int omap_abe_write_equalizer(struct omap_abe *abe,
			     u32 id, struct omap_abe_equ *param)
{
	u32 eq_offset, length, *src, eq_mem, eq_mem_len;
	_log(ABE_ID_WRITE_EQUALIZER, id, 0, 0);
	switch (id) {
	default:
	case EQ1:
		eq_offset = OMAP_ABE_C_DL1_COEFS_ADDR;
		eq_mem = OMAP_ABE_S_DL1_M_EQ_DATA_ADDR;
		eq_mem_len = OMAP_ABE_S_DL1_M_EQ_DATA_SIZE;
		break;
	case EQ2L:
		eq_offset = OMAP_ABE_C_DL2_L_COEFS_ADDR;
		eq_mem = OMAP_ABE_S_DL2_M_LR_EQ_DATA_ADDR;
		eq_mem_len = OMAP_ABE_S_DL2_M_LR_EQ_DATA_SIZE;
		break;
	case EQ2R:
		eq_offset = OMAP_ABE_C_DL2_R_COEFS_ADDR;
		eq_mem = OMAP_ABE_S_DL2_M_LR_EQ_DATA_ADDR;
		eq_mem_len = OMAP_ABE_S_DL2_M_LR_EQ_DATA_SIZE;
		break;
	case EQSDT:
		eq_offset = OMAP_ABE_C_SDT_COEFS_ADDR;
		eq_mem = OMAP_ABE_S_SDT_F_DATA_ADDR;
		eq_mem_len = OMAP_ABE_S_SDT_F_DATA_SIZE;
		break;
	case EQAMIC:
		eq_offset = OMAP_ABE_C_96_48_AMIC_COEFS_ADDR;
		eq_mem = OMAP_ABE_S_AMIC_96_48_DATA_ADDR;
		eq_mem_len = OMAP_ABE_S_AMIC_96_48_DATA_SIZE;
		break;
	case EQDMIC:
		eq_offset = OMAP_ABE_C_96_48_DMIC_COEFS_ADDR;
		eq_mem = OMAP_ABE_S_DMIC0_96_48_DATA_ADDR;
		eq_mem_len = OMAP_ABE_S_DMIC0_96_48_DATA_SIZE;
		/* three DMIC are clear at the same time DMIC0 DMIC1 DMIC2 */
		eq_mem_len *= 3;
		break;
	case APS1:
		eq_offset = OMAP_ABE_C_APS_DL1_COEFFS1_ADDR;
		eq_mem = OMAP_ABE_S_APS_IIRMEM1_ADDR;
		eq_mem_len = OMAP_ABE_S_APS_IIRMEM1_SIZE;
		break;
	case APS2L:
		eq_offset = OMAP_ABE_C_APS_DL2_L_COEFFS1_ADDR;
		eq_mem = OMAP_ABE_S_APS_M_IIRMEM2_ADDR;
		eq_mem_len = OMAP_ABE_S_APS_M_IIRMEM2_SIZE;
		break;
	case APS2R:
		eq_offset = OMAP_ABE_C_APS_DL2_R_COEFFS1_ADDR;
		eq_mem = OMAP_ABE_S_APS_M_IIRMEM2_ADDR;
		eq_mem_len = OMAP_ABE_S_APS_M_IIRMEM2_SIZE;
		break;
	}
	/* reset SMEM buffers before the coefficients are loaded */
	omap_abe_reset_mem(abe, OMAP_ABE_SMEM, eq_mem, eq_mem_len);

	length = param->equ_length;
	src = (u32 *) ((param->coef).type1);
	/* translate in bytes */
	length <<= 2;
	omap_abe_mem_write(abe,	OMAP_ABE_CMEM, eq_offset, src, length);

	/* reset SMEM buffers after the coefficients are loaded */
	omap_abe_reset_mem(abe, OMAP_ABE_SMEM, eq_mem, eq_mem_len);
	return 0;
}
EXPORT_SYMBOL(omap_abe_write_equalizer);

/**
 * omap_abe_disable_gain
 * @abe: Pointer on abe handle
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
int omap_abe_disable_gain(struct omap_abe *abe, u32 id, u32 p)
{
	u32 mixer_offset, f_g, ramp;
	omap_abe_gain_offset(abe, id, &mixer_offset);
	/* save the input parameters for mute/unmute */
	ramp = abe->desired_ramp_delay_ms[mixer_offset + p];
	f_g = GAIN_MUTE;
	if (!(abe->muted_gains_indicator[mixer_offset + p] &
	      OMAP_ABE_GAIN_DISABLED)) {
		/* Check if we are in mute */
		if (!(abe->muted_gains_indicator[mixer_offset + p] &
		      OMAP_ABE_GAIN_MUTED)) {
			abe->muted_gains_decibel[mixer_offset + p] =
				abe->desired_gains_decibel[mixer_offset + p];
			/* mute the gain */
			omap_abe_write_gain(abe, id, f_g, ramp, p);
		}
		abe->muted_gains_indicator[mixer_offset + p] |=
			OMAP_ABE_GAIN_DISABLED;
	}
	return 0;
}
EXPORT_SYMBOL(omap_abe_disable_gain);

/**
 * omap_abe_enable_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
int omap_abe_enable_gain(struct omap_abe *abe, u32 id, u32 p)
{
	u32 mixer_offset, f_g, ramp;
	omap_abe_gain_offset(abe, id, &mixer_offset);
	if ((abe->muted_gains_indicator[mixer_offset + p] &
	     OMAP_ABE_GAIN_DISABLED)) {
		/* restore the input parameters for mute/unmute */
		f_g = abe->muted_gains_decibel[mixer_offset + p];
		ramp = abe->desired_ramp_delay_ms[mixer_offset + p];
		abe->muted_gains_indicator[mixer_offset + p] &=
			~OMAP_ABE_GAIN_DISABLED;
		/* unmute the gain */
		omap_abe_write_gain(abe, id, f_g, ramp, p);
	}
	return 0;
}
EXPORT_SYMBOL(omap_abe_enable_gain);
/**
 * omap_abe_mute_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
int omap_abe_mute_gain(struct omap_abe *abe, u32 id, u32 p)
{
	u32 mixer_offset, f_g, ramp;
	omap_abe_gain_offset(abe, id, &mixer_offset);
	/* save the input parameters for mute/unmute */
	ramp = abe->desired_ramp_delay_ms[mixer_offset + p];
	f_g = GAIN_MUTE;
	if (!abe->muted_gains_indicator[mixer_offset + p]) {
		abe->muted_gains_decibel[mixer_offset + p] =
			abe->desired_gains_decibel[mixer_offset + p];
		/* mute the gain */
		omap_abe_write_gain(abe, id, f_g, ramp, p);
	}
	abe->muted_gains_indicator[mixer_offset + p] |= OMAP_ABE_GAIN_MUTED;
	return 0;
}
EXPORT_SYMBOL(omap_abe_mute_gain);
/**
 * omap_abe_unmute_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
int omap_abe_unmute_gain(struct omap_abe *abe, u32 id, u32 p)
{
	u32 mixer_offset, f_g, ramp;
	omap_abe_gain_offset(abe, id, &mixer_offset);
	if ((abe->muted_gains_indicator[mixer_offset + p] &
	    OMAP_ABE_GAIN_MUTED)) {
		/* restore the input parameters for mute/unmute */
		f_g = abe->muted_gains_decibel[mixer_offset + p];
		ramp = abe->desired_ramp_delay_ms[mixer_offset + p];
		abe->muted_gains_indicator[mixer_offset + p] &=
			~OMAP_ABE_GAIN_MUTED;
		/* unmute the gain */
		omap_abe_write_gain(abe, id, f_g, ramp, p);
	}
	return 0;
}
EXPORT_SYMBOL(omap_abe_unmute_gain);

/**
 * omap_abe_write_gain
 * @id: gain name or mixer name
 * @f_g: list of input gains of the mixer
 * @ramp: gain ramp speed factor
 * @p: list of ports corresponding to the above gains
 *
 * Loads the gain coefficients to FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's gain
 * in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
int omap_abe_write_gain(struct omap_abe *abe,
			u32 id, s32 f_g, u32 ramp, u32 p)
{
	u32 lin_g, sum_g, mixer_target, mixer_offset, i, mean_gain, mean_exp;
	u32 new_gain_linear[4];
	s32 gain_index;
	u32 alpha, beta;
	u32 ramp_index;

	_log(ABE_ID_WRITE_GAIN, id, f_g, p);
	gain_index = ((f_g - min_mdb) / 100);
	gain_index = maximum(gain_index, 0);
	gain_index = minimum(gain_index, sizeof_db2lin_table);
	lin_g = abe_db2lin_table[gain_index];
	omap_abe_gain_offset(abe, id, &mixer_offset);
	/* save the input parameters for mute/unmute */
	abe->desired_gains_linear[mixer_offset + p] = lin_g;
	abe->desired_gains_decibel[mixer_offset + p] = f_g;
	abe->desired_ramp_delay_ms[mixer_offset + p] = ramp;
	/* SMEM address in bytes */
	mixer_target = OMAP_ABE_S_GTARGET1_ADDR;
	mixer_target += (mixer_offset<<2);
	mixer_target += (p<<2);

	if (abe->compensated_mixer_gain) {
		switch (id) {
		case MIXDL1:
		case MIXDL2:
		case MIXVXREC:
		case MIXAUDUL:
			/* compute the sum of the gain of the mixer */
			for (sum_g = i = 0; i < 4; i++)
				sum_g += abe->desired_gains_linear[mixer_offset +
								  i];
			/* lets avoid a division by 0 */
			if (sum_g == 0)
				break;
			/* if the sum is OK with less than 1, then
			   do not weight the gains */
			if (sum_g < 0x00040000) {	/* REMOVE HARD CONST */
				/* recompute all gains from original
				   desired values */
				sum_g = 0x00040000;
			}
			/* translate it in Q16 format for the later division */
			abe_int_2_float16(sum_g, &mean_gain, &mean_exp);
			mean_exp = 10 - mean_exp;
			for (i = 0; i < 4; i++) {
				/* new gain = desired gain divided by sum of gains */
				new_gain_linear[i] =
					(abe->desired_gains_linear
					 [mixer_offset + i]
					 << 8) / mean_gain;
				new_gain_linear[i] = (mean_exp > 0) ?
					new_gain_linear[i] << mean_exp :
					new_gain_linear[i] >> mean_exp;
			}
			/* load the whole adpated S_G_Target SMEM MIXER table */
			omap_abe_mem_write(abe, OMAP_ABE_SMEM,
				       mixer_target - (p << 2),
				       new_gain_linear, (4 * sizeof(lin_g)));
			break;
		default:
			/* load the S_G_Target SMEM table */
			omap_abe_mem_write(abe, OMAP_ABE_SMEM,
				       mixer_target,
				       (u32 *) &lin_g, sizeof(lin_g));
			break;
		}
	} else {
		if (!abe->muted_gains_indicator[mixer_offset + p])
			/* load the S_G_Target SMEM table */
			omap_abe_mem_write(abe, OMAP_ABE_SMEM,
				       mixer_target, (u32 *) &lin_g,
				       sizeof(lin_g));
		else
			/* update muted gain with new value */
			abe->muted_gains_decibel[mixer_offset + p] = f_g;
	}
	ramp = maximum(minimum(RAMP_MAXLENGTH, ramp), RAMP_MINLENGTH);
	/* ramp data should be interpolated in the table instead */
	ramp_index = 8;
	if ((RAMP_5MS <= ramp) && (ramp < RAMP_50MS))
		ramp_index = 24;
	if ((RAMP_50MS <= ramp) && (ramp < RAMP_500MS))
		ramp_index = 36;
	if (ramp > RAMP_500MS)
		ramp_index = 48;
	beta = abe_alpha_iir[ramp_index];
	alpha = abe_1_alpha_iir[ramp_index];
	/* CMEM bytes address */
	mixer_target = OMAP_ABE_C_1_ALPHA_ADDR;
	/* a pair of gains is updated once in the firmware */
	mixer_target += (p + mixer_offset) << 1;
	/* load the ramp delay data */
	omap_abe_mem_write(abe, OMAP_ABE_CMEM, mixer_target,
		       (u32 *) &alpha, sizeof(alpha));
	/* CMEM bytes address */
	mixer_target = OMAP_ABE_C_ALPHA_ADDR;
	/* a pair of gains is updated once in the firmware */
	mixer_target += (p + mixer_offset) << 1;
	omap_abe_mem_write(abe, OMAP_ABE_CMEM, mixer_target,
		       (u32 *) &beta, sizeof(beta));
	return 0;
}
EXPORT_SYMBOL(omap_abe_write_gain);
/**
 * omap_abe_write_mixer
 * @id: name of the mixer
 * @param: input gains and delay ramp of the mixer
 * @p: port corresponding to the above gains
 *
 * Load the gain coefficients in FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's
 * gain in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
int omap_abe_write_mixer(struct omap_abe *abe,
				u32 id, s32 f_g, u32 f_ramp, u32 p)
{
	_log(ABE_ID_WRITE_MIXER, id, f_ramp, p);
	omap_abe_write_gain(abe, id, f_g, f_ramp, p);
	return 0;
}
EXPORT_SYMBOL(omap_abe_write_mixer);

/**
 * omap_abe_read_gain
 * @id: name of the mixer
 * @param: list of input gains of the mixer
 * @p: list of port corresponding to the above gains
 *
 */
int omap_abe_read_gain(struct omap_abe *abe,
				u32 id, u32 *f_g, u32 p)
{
	u32 mixer_target, mixer_offset, i;
	_log(ABE_ID_READ_GAIN, id, (u32) f_g, p);
	omap_abe_gain_offset(abe, id, &mixer_offset);
	/* SMEM bytes address */
	mixer_target = OMAP_ABE_S_GTARGET1_ADDR;
	mixer_target += (mixer_offset<<2);
	mixer_target += (p<<2);
	if (!abe->muted_gains_indicator[mixer_offset + p]) {
		/* load the S_G_Target SMEM table */
		omap_abe_mem_read(abe, OMAP_ABE_SMEM, mixer_target,
			       (u32 *) f_g, sizeof(*f_g));
		for (i = 0; i < sizeof_db2lin_table; i++) {
				if (abe_db2lin_table[i] == *f_g)
				goto found;
		}
		*f_g = 0;
		return -1;
	      found:
		*f_g = (i * 100) + min_mdb;
	} else {
		/* update muted gain with new value */
		*f_g = abe->muted_gains_decibel[mixer_offset + p];
	}
	return 0;
}
EXPORT_SYMBOL(omap_abe_read_gain);

/**
 * abe_read_mixer
 * @id: name of the mixer
 * @param: gains of the mixer
 * @p: port corresponding to the above gains
 *
 * Load the gain coefficients in FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's
 * gain in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
int omap_abe_read_mixer(struct omap_abe *abe,
				u32 id, u32 *f_g, u32 p)
{
	_log(ABE_ID_READ_MIXER, id, 0, p);
	omap_abe_read_gain(abe, id, f_g, p);
	return 0;
}
EXPORT_SYMBOL(omap_abe_read_mixer);

/**
 * abe_reset_gain_mixer
 * @id: name of the mixer
 * @p: list of port corresponding to the above gains
 *
 * restart the working gain value of the mixers when a port is enabled
 */
void omap_abe_reset_gain_mixer(struct omap_abe *abe, u32 id, u32 p)
{
	u32 lin_g, mixer_target, mixer_offset;
	switch (id) {
	default:
	case GAINS_DMIC1:
		mixer_offset = dmic1_gains_offset;
		break;
	case GAINS_DMIC2:
		mixer_offset = dmic2_gains_offset;
		break;
	case GAINS_DMIC3:
		mixer_offset = dmic3_gains_offset;
		break;
	case GAINS_AMIC:
		mixer_offset = amic_gains_offset;
		break;
	case GAINS_DL1:
		mixer_offset = dl1_gains_offset;
		break;
	case GAINS_DL2:
		mixer_offset = dl2_gains_offset;
		break;
	case GAINS_SPLIT:
		mixer_offset = splitters_gains_offset;
		break;
	case MIXDL1:
		mixer_offset = mixer_dl1_offset;
		break;
	case MIXDL2:
		mixer_offset = mixer_dl2_offset;
		break;
	case MIXECHO:
		mixer_offset = mixer_echo_offset;
		break;
	case MIXSDT:
		mixer_offset = mixer_sdt_offset;
		break;
	case MIXVXREC:
		mixer_offset = mixer_vxrec_offset;
		break;
	case MIXAUDUL:
		mixer_offset = mixer_audul_offset;
		break;
	case GAINS_BTUL:
		mixer_offset = btul_gains_offset;
		break;
	}
	/* SMEM bytes address for the CURRENT gain values */
	mixer_target = OMAP_ABE_S_GCURRENT_ADDR;
	mixer_target += (mixer_offset<<2);
	mixer_target += (p<<2);
	lin_g = 0;
	/* load the S_G_Target SMEM table */
	omap_abe_mem_write(abe, OMAP_ABE_SMEM, mixer_target,
		       (u32 *) &lin_g, sizeof(lin_g));
}
