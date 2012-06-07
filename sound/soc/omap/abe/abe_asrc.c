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

#include "abe_legacy.h"
#include "abe_dbg.h"

#include "abe_typedef.h"
#include "abe_initxxx_labels.h"
#include "abe_dbg.h"
#include "abe_mem.h"
#include "abe_sm_addr.h"
#include "abe_cm_addr.h"

/**
 * abe_write_fifo
 * @mem_bank: currently only ABE_DMEM supported
 * @addr: FIFO descriptor address ( descriptor fields : READ ptr, WRITE ptr,
 * FIFO START_ADDR, FIFO END_ADDR)
 * @data: data to write to FIFO
 * @number: number of 32-bit words to write to DMEM FIFO
 *
 * write DMEM FIFO and update FIFO descriptor,
 * it is assumed that FIFO descriptor is located in DMEM
 */
void abe_write_fifo(u32 memory_bank, u32 descr_addr, u32 *data, u32 nb_data32)
{
	u32 fifo_addr[4];
	u32 i;
	/* read FIFO descriptor from DMEM */
	omap_abe_mem_read(abe, OMAP_ABE_DMEM, descr_addr,
		       &fifo_addr[0], 4 * sizeof(u32));
	/* WRITE ptr < FIFO start address */
	if (fifo_addr[1] < fifo_addr[2])
		omap_abe_dbg_error(abe, OMAP_ABE_ERR_DBG,
				   ABE_FW_FIFO_WRITE_PTR_ERR);
	/* WRITE ptr > FIFO end address */
	if (fifo_addr[1] > fifo_addr[3])
		omap_abe_dbg_error(abe, OMAP_ABE_ERR_DBG,
				   ABE_FW_FIFO_WRITE_PTR_ERR);
	switch (memory_bank) {
	case ABE_DMEM:
		for (i = 0; i < nb_data32; i++) {
			omap_abe_mem_write(abe, OMAP_ABE_DMEM,
				       (s32) fifo_addr[1], (u32 *) (data + i),
				       4);
			/* increment WRITE pointer */
			fifo_addr[1] = fifo_addr[1] + 4;
			if (fifo_addr[1] > fifo_addr[3])
				fifo_addr[1] = fifo_addr[2];
			if (fifo_addr[1] == fifo_addr[0])
				omap_abe_dbg_error(abe, OMAP_ABE_ERR_DBG,
						   ABE_FW_FIFO_WRITE_PTR_ERR);
		}
		/* update WRITE pointer in DMEM */
		omap_abe_mem_write(abe, OMAP_ABE_DMEM, descr_addr +
			       sizeof(u32), &fifo_addr[1], 4);
		break;
	default:
		break;
	}
}

/**
 * abe_write_asrc
 * @id: name of the port
 * @param: drift value to compensate [ppm]
 *
 * Load the drift variables to the FW memory. This API can be called only
 * when the corresponding port has been already opened and the ASRC has
 * been correctly initialized with API abe_init_asrc_... If this API is
 * used such that the drift has been changed from positive to negative drift
 * or vice versa, there will be click in the output signal. Loading the drift
 * value with zero disables the feature.
 */
abehal_status abe_write_asrc(u32 port, s32 dppm)
{
	s32 dtempvalue, adppm, drift_sign, drift_sign_addr, alpha_params_addr;
	s32 alpha_params[3];
	_log(ABE_ID_WRITE_ASRC, port, dppm, dppm >> 8);
	/*
	 * x = ppm
	 *
	 * - 1000000/x must be multiple of 16
	 * - deltaalpha = round(2^20*x*16/1000000)=round(2^18/5^6*x) on 22 bits.
	 *      then shifted by 2bits
	 * - minusdeltaalpha
	 * - oneminusepsilon = 1-deltaalpha/2.
	 *
	 * ppm = 250
	 * - 1000000/250=4000
	 * - deltaalpha = 4194.3 ~ 4195 => 0x00418c
	 */
	/* examples for -6250 ppm */
	/* atempvalue32[1] = -1;  d_driftsign */
	/* atempvalue32[3] = 0x00066668;  d_deltaalpha */
	/* atempvalue32[4] = 0xfff99998;  d_minusdeltaalpha */
	/* atempvalue32[5] = 0x003ccccc;  d_oneminusepsilon */
	/* example for 100 ppm */
	/* atempvalue32[1] = 1;* d_driftsign */
	/* atempvalue32[3] = 0x00001a38;  d_deltaalpha */
	/* atempvalue32[4] = 0xffffe5c8;  d_minusdeltaalpha */
	/* atempvalue32[5] = 0x003ccccc;  d_oneminusepsilon */
	/* compute new value for the ppm */
	if (dppm >= 0) {
		/* d_driftsign */
		drift_sign = 1;
		adppm = dppm;
	} else {
		/* d_driftsign */
		drift_sign = -1;
		adppm = (-1 * dppm);
	}
	if (dppm == 0) {
		/* delta_alpha */
		alpha_params[0] = 0;
		/* minusdelta_alpha */
		alpha_params[1] = 0;
		/* one_minusepsilon */
		alpha_params[2] = 0x003ffff0;
	} else {
		dtempvalue = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);
		/* delta_alpha */
		alpha_params[0] = dtempvalue << 2;
		/* minusdelta_alpha */
		alpha_params[1] = (-dtempvalue) << 2;
		/* one_minusepsilon */
		alpha_params[2] = (0x00100000 - (dtempvalue / 2)) << 2;
	}
	switch (port) {
	/* asynchronous sample-rate-converter for the uplink voice path */
	case OMAP_ABE_VX_DL_PORT:
		drift_sign_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (1 * sizeof(s32));
		alpha_params_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (3 * sizeof(s32));
		break;
	/* asynchronous sample-rate-converter for the downlink voice path */
	case OMAP_ABE_VX_UL_PORT:
		drift_sign_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (1 * sizeof(s32));
		alpha_params_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (3 * sizeof(s32));
		break;
	/* asynchronous sample-rate-converter for the BT_UL path */
	case OMAP_ABE_BT_VX_UL_PORT:
		drift_sign_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (1 * sizeof(s32));
		alpha_params_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (3 * sizeof(s32));
		break;
	/* asynchronous sample-rate-converter for the BT_DL path */
	case OMAP_ABE_BT_VX_DL_PORT:
		drift_sign_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (1 * sizeof(s32));
		alpha_params_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (3 * sizeof(s32));
		break;
	default:
	/* asynchronous sample-rate-converter for the MM_EXT_IN path */
	case OMAP_ABE_MM_EXT_IN_PORT:
		drift_sign_addr = OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (1 * sizeof(s32));
		alpha_params_addr =
			OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (3 * sizeof(s32));
		break;
	}
	omap_abe_mem_write(abe, OMAP_ABE_DMEM, drift_sign_addr,
		       (u32 *) &drift_sign, 4);
	omap_abe_mem_write(abe, OMAP_ABE_DMEM, alpha_params_addr,
		       (u32 *) &alpha_params[0], 12);
	return 0;
}
EXPORT_SYMBOL(abe_write_asrc);
/**
 * abe_init_asrc_vx_dl
 *
 * Initialize the following ASRC VX_DL parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_DL_VX_Coefs pointer
 * 10. CMEM for ASRC_DL_VX_Coefs pointer
 * ASRC_DL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
 * C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1
 * 11. SMEM for XinASRC_DL_VX pointer
 * 12. CMEM for XinASRC_DL_VX pointer
 * XinASRC_DL_VX = S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/0/1/0/0/0/0
 * 13. SMEM for IO_VX_DL_ASRC pointer
 * 14. CMEM for IO_VX_DL_ASRC pointer
 * IO_VX_DL_ASRC =
 *	S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/
 *	ASRC_DL_VX_FIR_L+ASRC_margin/1/0/0/0/0
 */
void abe_init_asrc_vx_dl(s32 dppm)
{
	s32 el[45];
	s32 temp0, temp1, adppm, dtemp, mem_tag, mem_addr;
	u32 i = 0;
	u32 n_fifo_el = 42;
	temp0 = 0;
	temp1 = 1;
	/* 1. DriftSign = D_AsrcVars[1] = 1 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (1 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm >= 0) {
		el[i + 1] = 1;
		adppm = dppm;
	} else {
		el[i + 1] = -1;
		adppm = (-1 * dppm);
	}
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	dtemp = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);
	/* 2. Subblock = D_AsrcVars[2] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (2 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 3. DeltaAlpha = D_AsrcVars[3] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (3 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = dtemp << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 4. MinusDeltaAlpha = D_AsrcVars[4] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (4 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = (-dtemp) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/*5. OneMinusEpsilon = D_AsrcVars[5] = 0x00400000 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_DL_VX_ADDR + (5 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0x00400000;
	else
		el[i + 1] = (0x00100000 - (dtemp / 2)) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 6. AlphaCurrent = 0x000020 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_ALPHACURRENT_DL_VX_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x00000020;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 7. BetaCurrent = 0x3fffe0 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_BETACURRENT_DL_VX_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x003fffe0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 8. drift_ASRC = 0 & drift_io = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_IODESCR_ADDR + (OMAP_ABE_VX_DL_PORT * sizeof(struct ABE_SIODescriptor))
		+ drift_asrc_;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 9. SMEM for ASRC_DL_VX_Coefs pointer */
	/* ASRC_DL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_SMEM;
	mem_addr = ASRC_DL_VX_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	if (dppm == 0) {
		el[i + 1] = OMAP_ABE_C_COEFASRC16_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC16_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC15_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC15_VX_SIZE >> 2);
	} else {
		el[i + 1] = OMAP_ABE_C_COEFASRC1_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC1_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC2_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC2_VX_SIZE >> 2);
	}
	i = i + 3;
	/* 10. CMEM for ASRC_DL_VX_Coefs pointer */
	/* ASRC_DL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_CMEM;
	mem_addr = ASRC_DL_VX_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp1;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 11. SMEM for XinASRC_DL_VX pointer */
	/* XinASRC_DL_VX =
		S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/0/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = XinASRC_DL_VX_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_DL_VX_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_DL_VX_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 12. CMEM for XinASRC_DL_VX pointer */
	/* XinASRC_DL_VX =
		S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/0/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = XinASRC_DL_VX_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 13. SMEM for IO_VX_DL_ASRC pointer */
	/* IO_VX_DL_ASRC = S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/
	   ASRC_DL_VX_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = IO_VX_DL_ASRC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_DL_VX_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_DL_VX_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 14. CMEM for IO_VX_DL_ASRC pointer */
	/* IO_VX_DL_ASRC = S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/
	   ASRC_DL_VX_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = IO_VX_DL_ASRC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_DL_VX_FIR_L + ASRC_margin) << 16) + (temp1 << 12)
		+ (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	abe_write_fifo(ABE_DMEM, OMAP_ABE_D_FWMEMINITDESCR_ADDR, (u32 *) &el[0],
		       n_fifo_el);
}
/**
 * abe_init_asrc_vx_ul
 *
 * Initialize the following ASRC VX_UL parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_UL_VX_Coefs pointer
 * 10. CMEM for ASRC_UL_VX_Coefs pointer
 * ASRC_UL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
 *	C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1
 * 11. SMEM for XinASRC_UL_VX pointer
 * 12. CMEM for XinASRC_UL_VX pointer
 * XinASRC_UL_VX = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/0/1/0/0/0/0
 * 13. SMEM for UL_48_8_DEC pointer
 * 14. CMEM for UL_48_8_DEC pointer
 * UL_48_8_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
 *	ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0
 * 15. SMEM for UL_48_16_DEC pointer
 * 16. CMEM for UL_48_16_DEC pointer
 * UL_48_16_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
 *	ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0
 */
void abe_init_asrc_vx_ul(s32 dppm)
{
	s32 el[51];
	s32 temp0, temp1, adppm, dtemp, mem_tag, mem_addr;
	u32 i = 0;
	u32 n_fifo_el = 48;
	temp0 = 0;
	temp1 = 1;
	/* 1. DriftSign = D_AsrcVars[1] = 1 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (1 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm >= 0) {
		el[i + 1] = 1;
		adppm = dppm;
	} else {
		el[i + 1] = -1;
		adppm = (-1 * dppm);
	}
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	dtemp = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);
	/* 2. Subblock = D_AsrcVars[2] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (2 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 3. DeltaAlpha = D_AsrcVars[3] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (3 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = dtemp << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 4. MinusDeltaAlpha = D_AsrcVars[4] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (4 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = (-dtemp) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 5. OneMinusEpsilon = D_AsrcVars[5] = 0x00400000 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_UL_VX_ADDR + (5 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0x00400000;
	else
		el[i + 1] = (0x00100000 - (dtemp / 2)) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 6. AlphaCurrent = 0x000020 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_ALPHACURRENT_UL_VX_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x00000020;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 7. BetaCurrent = 0x3fffe0 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_BETACURRENT_UL_VX_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x003fffe0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 8. drift_ASRC = 0 & drift_io = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_IODESCR_ADDR + (OMAP_ABE_VX_UL_PORT * sizeof(struct ABE_SIODescriptor))
		+ drift_asrc_;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 9. SMEM for ASRC_UL_VX_Coefs pointer */
	/* ASRC_UL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_SMEM;
	mem_addr = ASRC_UL_VX_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	if (dppm == 0) {
		el[i + 1] = OMAP_ABE_C_COEFASRC16_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC16_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC15_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC15_VX_SIZE >> 2);
	} else {
		el[i + 1] = OMAP_ABE_C_COEFASRC1_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC1_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC2_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC2_VX_SIZE >> 2);
	}
	i = i + 3;
	/* 10. CMEM for ASRC_UL_VX_Coefs pointer */
	/* ASRC_UL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_CMEM;
	mem_addr = ASRC_UL_VX_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp1;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 11. SMEM for XinASRC_UL_VX pointer */
	/* XinASRC_UL_VX = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/0/1/
		0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = XinASRC_UL_VX_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_UL_VX_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_UL_VX_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 12. CMEM for XinASRC_UL_VX pointer */
	/* XinASRC_UL_VX = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/0/1/
		0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = XinASRC_UL_VX_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 13. SMEM for UL_48_8_DEC pointer */
	/* UL_48_8_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
	   ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = UL_48_8_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_UL_VX_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_UL_VX_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 14. CMEM for UL_48_8_DEC pointer */
	/* UL_48_8_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
	   ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = UL_48_8_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_UL_VX_FIR_L + ASRC_margin) << 16) + (temp1 << 12)
		+ (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 15. SMEM for UL_48_16_DEC pointer */
	/* UL_48_16_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
	   ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = UL_48_16_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_UL_VX_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_UL_VX_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 16. CMEM for UL_48_16_DEC pointer */
	/* UL_48_16_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
	   ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = UL_48_16_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_UL_VX_FIR_L + ASRC_margin) << 16) + (temp1 << 12)
		+ (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	abe_write_fifo(ABE_DMEM, OMAP_ABE_D_FWMEMINITDESCR_ADDR, (u32 *) &el[0],
		       n_fifo_el);
}
/**
 * abe_init_asrc_mm_ext_in
 *
 * Initialize the following ASRC MM_EXT_IN parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_MM_EXT_IN_Coefs pointer
 * 10. CMEM for ASRC_MM_EXT_IN_Coefs pointer
 * ASRC_MM_EXT_IN_Coefs = C_CoefASRC16_MM_ADDR/C_CoefASRC16_MM_sizeof/0/1/
 *	C_CoefASRC15_MM_ADDR/C_CoefASRC15_MM_sizeof/0/1
 * 11. SMEM for XinASRC_MM_EXT_IN pointer
 * 12. CMEM for XinASRC_MM_EXT_IN pointer
 * XinASRC_MM_EXT_IN = S_XinASRC_MM_EXT_IN_ADDR/S_XinASRC_MM_EXT_IN_sizeof/0/1/
 *	0/0/0/0
 * 13. SMEM for IO_MM_EXT_IN_ASRC pointer
 * 14. CMEM for IO_MM_EXT_IN_ASRC pointer
 * IO_MM_EXT_IN_ASRC = S_XinASRC_MM_EXT_IN_ADDR/S_XinASRC_MM_EXT_IN_sizeof/
 *	ASRC_MM_EXT_IN_FIR_L+ASRC_margin+ASRC_N_48k/1/0/0/0/0
 */
void abe_init_asrc_mm_ext_in(s32 dppm)
{
	s32 el[45];
	s32 temp0, temp1, adppm, dtemp, mem_tag, mem_addr;
	u32 i = 0;
	u32 n_fifo_el = 42;
	temp0 = 0;
	temp1 = 1;
	/* 1. DriftSign = D_AsrcVars[1] = 1 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (1 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm >= 0) {
		el[i + 1] = 1;
		adppm = dppm;
	} else {
		el[i + 1] = -1;
		adppm = (-1 * dppm);
	}
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	dtemp = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);
	/* 2. Subblock = D_AsrcVars[2] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (2 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 3. DeltaAlpha = D_AsrcVars[3] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (3 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = dtemp << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 4. MinusDeltaAlpha = D_AsrcVars[4] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (4 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = (-dtemp) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 5. OneMinusEpsilon = D_AsrcVars[5] = 0x00400000 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_MM_EXT_IN_ADDR + (5 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0x00400000;
	else
		el[i + 1] = (0x00100000 - (dtemp / 2)) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 6. AlphaCurrent = 0x000020 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_ALPHACURRENT_MM_EXT_IN_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x00000020;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 7. BetaCurrent = 0x3fffe0 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_BETACURRENT_MM_EXT_IN_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x003fffe0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 8. drift_ASRC = 0 & drift_io = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_IODESCR_ADDR + (OMAP_ABE_MM_EXT_IN_PORT * sizeof(struct ABE_SIODescriptor))
		+ drift_asrc_;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 9. SMEM for ASRC_MM_EXT_IN_Coefs pointer */
	/* ASRC_MM_EXT_IN_Coefs = C_CoefASRC16_MM_ADDR/C_CoefASRC16_MM_sizeof/
		0/1/C_CoefASRC15_MM_ADDR/C_CoefASRC15_MM_sizeof/0/1 */
	mem_tag = ABE_SMEM;
	mem_addr = ASRC_MM_EXT_IN_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	if (dppm == 0) {
		el[i + 1] = OMAP_ABE_C_COEFASRC16_MM_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC16_MM_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC15_MM_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC15_MM_SIZE >> 2);
	} else {
		el[i + 1] = OMAP_ABE_C_COEFASRC1_MM_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC1_MM_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC2_MM_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC2_MM_SIZE >> 2);
	}
	i = i + 3;
	/*10. CMEM for ASRC_MM_EXT_IN_Coefs pointer */
	/* ASRC_MM_EXT_IN_Coefs = C_CoefASRC16_MM_ADDR/C_CoefASRC16_MM_sizeof/
		0/1/C_CoefASRC15_MM_ADDR/C_CoefASRC15_MM_sizeof/0/1 */
	mem_tag = ABE_CMEM;
	mem_addr = ASRC_MM_EXT_IN_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp1;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 11. SMEM for XinASRC_MM_EXT_IN pointer */
	/* XinASRC_MM_EXT_IN = S_XinASRC_MM_EXT_IN_ADDR/
		S_XinASRC_MM_EXT_IN_sizeof/0/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = XinASRC_MM_EXT_IN_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_MM_EXT_IN_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_MM_EXT_IN_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 12. CMEM for XinASRC_MM_EXT_IN pointer */
	/* XinASRC_MM_EXT_IN = S_XinASRC_MM_EXT_IN_ADDR/
		S_XinASRC_MM_EXT_IN_sizeof/0/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = XinASRC_MM_EXT_IN_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 13. SMEM for IO_MM_EXT_IN_ASRC pointer */
	/* IO_MM_EXT_IN_ASRC =
		S_XinASRC_MM_EXT_IN_ADDR/S_XinASRC_MM_EXT_IN_sizeof/
		ASRC_MM_EXT_IN_FIR_L+ASRC_margin+ASRC_N_48k/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = IO_MM_EXT_IN_ASRC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_MM_EXT_IN_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_MM_EXT_IN_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 14. CMEM for IO_MM_EXT_IN_ASRC pointer */
	/* IO_MM_EXT_IN_ASRC =
		S_XinASRC_MM_EXT_IN_ADDR/S_XinASRC_MM_EXT_IN_sizeof/
		ASRC_MM_EXT_IN_FIR_L+ASRC_margin+ASRC_N_48k/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = IO_MM_EXT_IN_ASRC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_MM_EXT_IN_FIR_L + ASRC_margin + ASRC_N_48k) << 16) +
		(temp1 << 12) + (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	abe_write_fifo(ABE_DMEM, OMAP_ABE_D_FWMEMINITDESCR_ADDR, (u32 *) &el[0],
		       n_fifo_el);
}
/**
 * abe_init_asrc_bt_ul
 *
 * Initialize the following ASRC BT_UL parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_BT_UL_Coefs pointer
 * 10. CMEM for ASRC_BT_UL_Coefs pointer
 * ASRC_BT_UL_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
 * C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1
 * 11. SMEM for XinASRC_BT_UL pointer
 * 12. CMEM for XinASRC_BT_UL pointer
 * XinASRC_BT_UL = S_XinASRC_BT_UL_ADDR/S_XinASRC_BT_UL_sizeof/0/1/0/0/0/0
 * 13. SMEM for IO_BT_UL_ASRC pointer
 * 14. CMEM for IO_BT_UL_ASRC pointer
 * IO_BT_UL_ASRC = S_XinASRC_BT_UL_ADDR/S_XinASRC_BT_UL_sizeof/
 *	ASRC_BT_UL_FIR_L+ASRC_margin/1/0/0/0/0
 */
void abe_init_asrc_bt_ul(s32 dppm)
{
	s32 el[45];
	s32 temp0, temp1, adppm, dtemp, mem_tag, mem_addr;
	u32 i = 0;
	u32 n_fifo_el = 42;
	temp0 = 0;
	temp1 = 1;
	/* 1. DriftSign = D_AsrcVars[1] = 1 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (1 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm >= 0) {
		el[i + 1] = 1;
		adppm = dppm;
	} else {
		el[i + 1] = -1;
		adppm = (-1 * dppm);
	}
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	dtemp = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);
	/* 2. Subblock = D_AsrcVars[2] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (2 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 3. DeltaAlpha = D_AsrcVars[3] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (3 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = dtemp << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 4. MinusDeltaAlpha = D_AsrcVars[4] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (4 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = (-dtemp) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/*5. OneMinusEpsilon = D_AsrcVars[5] = 0x00400000 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_UL_ADDR + (5 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0x00400000;
	else
		el[i + 1] = (0x00100000 - (dtemp / 2)) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 6. AlphaCurrent = 0x000020 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_ALPHACURRENT_BT_UL_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x00000020;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 7. BetaCurrent = 0x3fffe0 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_BETACURRENT_BT_UL_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x003fffe0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 8. drift_ASRC = 0 & drift_io = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_IODESCR_ADDR + (OMAP_ABE_BT_VX_UL_PORT * sizeof(struct ABE_SIODescriptor))
		+ drift_asrc_;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 9. SMEM for ASRC_BT_UL_Coefs pointer */
	/* ASRC_BT_UL_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_SMEM;
	mem_addr = ASRC_BT_UL_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	if (dppm == 0) {
		el[i + 1] = OMAP_ABE_C_COEFASRC16_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC16_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC15_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC15_VX_SIZE >> 2);
	} else {
		el[i + 1] = OMAP_ABE_C_COEFASRC1_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC1_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC2_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC2_VX_SIZE >> 2);
	}
	i = i + 3;
	/* 10. CMEM for ASRC_BT_UL_Coefs pointer */
	/* ASRC_BT_UL_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_CMEM;
	mem_addr = ASRC_BT_UL_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp1;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 11. SMEM for XinASRC_BT_UL pointer */
	/* XinASRC_BT_UL = S_XinASRC_BT_UL_ADDR/S_XinASRC_BT_UL_sizeof/0/1/
		0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = XinASRC_BT_UL_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_BT_UL_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_BT_UL_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 12. CMEM for XinASRC_BT_UL pointer */
	/* XinASRC_BT_UL = S_XinASRC_BT_UL_ADDR/S_XinASRC_BT_UL_sizeof/0/1/
		0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = XinASRC_BT_UL_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 13. SMEM for IO_BT_UL_ASRC pointer */
	/* IO_BT_UL_ASRC = S_XinASRC_BT_UL_ADDR/S_XinASRC_BT_UL_sizeof/
		ASRC_BT_UL_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = IO_BT_UL_ASRC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_BT_UL_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_BT_UL_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 14. CMEM for IO_BT_UL_ASRC pointer */
	/* IO_BT_UL_ASRC = S_XinASRC_BT_UL_ADDR/S_XinASRC_BT_UL_sizeof/
		ASRC_BT_UL_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = IO_BT_UL_ASRC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_BT_UL_FIR_L + ASRC_margin) << 16) + (temp1 << 12)
		+ (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	abe_write_fifo(ABE_DMEM, OMAP_ABE_D_FWMEMINITDESCR_ADDR, (u32 *) &el[0],
		       n_fifo_el);
}
/**
 * abe_init_asrc_bt_dl
 *
 * Initialize the following ASRC BT_DL parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_BT_DL_Coefs pointer
 * 10. CMEM for ASRC_BT_DL_Coefs pointer
 * ASRC_BT_DL_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
 *	C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1
 * 11. SMEM for XinASRC_BT_DL pointer
 * 12. CMEM for XinASRC_BT_DL pointer
 * XinASRC_BT_DL = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/0/1/0/0/0/0
 * 13. SMEM for DL_48_8_DEC pointer
 * 14. CMEM for DL_48_8_DEC pointer
 * DL_48_8_DEC = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/
 *	ASRC_BT_DL_FIR_L+ASRC_margin/1/0/0/0/0
 * 15. SMEM for DL_48_16_DEC pointer
 * 16. CMEM for DL_48_16_DEC pointer
 * DL_48_16_DEC = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/
 *	ASRC_BT_DL_FIR_L+ASRC_margin/1/0/0/0/0
 */
void abe_init_asrc_bt_dl(s32 dppm)
{
	s32 el[51];
	s32 temp0, temp1, adppm, dtemp, mem_tag, mem_addr;
	u32 i = 0;
	u32 n_fifo_el = 48;
	temp0 = 0;
	temp1 = 1;
	/* 1. DriftSign = D_AsrcVars[1] = 1 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (1 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm >= 0) {
		el[i + 1] = 1;
		adppm = dppm;
	} else {
		el[i + 1] = -1;
		adppm = (-1 * dppm);
	}
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	dtemp = (adppm << 4) + adppm - ((adppm * 3481L) / 15625L);
	/* 2. Subblock = D_AsrcVars[2] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (2 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 3. DeltaAlpha = D_AsrcVars[3] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (3 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = dtemp << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 4. MinusDeltaAlpha = D_AsrcVars[4] = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (4 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0;
	else
		el[i + 1] = (-dtemp) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 5. OneMinusEpsilon = D_AsrcVars[5] = 0x00400000 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_ASRCVARS_BT_DL_ADDR + (5 * sizeof(s32));
	el[i] = (mem_tag << 16) + mem_addr;
	if (dppm == 0)
		el[i + 1] = 0x00400000;
	else
		el[i + 1] = (0x00100000 - (dtemp / 2)) << 2;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 6. AlphaCurrent = 0x000020 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_ALPHACURRENT_BT_DL_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x00000020;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 7. BetaCurrent = 0x3fffe0 (CMEM) */
	mem_tag = ABE_CMEM;
	mem_addr = OMAP_ABE_C_BETACURRENT_BT_DL_ADDR;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = 0x003fffe0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 8. drift_ASRC = 0 & drift_io = 0 */
	mem_tag = ABE_DMEM;
	mem_addr = OMAP_ABE_D_IODESCR_ADDR + (OMAP_ABE_BT_VX_DL_PORT * sizeof(struct ABE_SIODescriptor))
		+ drift_asrc_;
	el[i] = (mem_tag << 16) + mem_addr;
	el[i + 1] = temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 9. SMEM for ASRC_BT_DL_Coefs pointer */
	/* ASRC_BT_DL_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_SMEM;
	mem_addr = ASRC_BT_DL_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	if (dppm == 0) {
		el[i + 1] = OMAP_ABE_C_COEFASRC16_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC16_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC15_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC15_VX_SIZE >> 2);
	} else {
		el[i + 1] = OMAP_ABE_C_COEFASRC1_VX_ADDR >> 2;
		el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_C_COEFASRC1_VX_SIZE >> 2);
		el[i + 2] = OMAP_ABE_C_COEFASRC2_VX_ADDR >> 2;
		el[i + 2] = (el[i + 2] << 8) + (OMAP_ABE_C_COEFASRC2_VX_SIZE >> 2);
	}
	i = i + 3;
	/* 10. CMEM for ASRC_BT_DL_Coefs pointer */
	/* ASRC_BT_DL_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
		C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1 */
	mem_tag = ABE_CMEM;
	mem_addr = ASRC_BT_DL_Coefs_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp1;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 11. SMEM for XinASRC_BT_DL pointer */
	/* XinASRC_BT_DL =
		S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/0/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = XinASRC_BT_DL_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_BT_DL_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_BT_DL_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 12. CMEM for XinASRC_BT_DL pointer */
	/* XinASRC_BT_DL =
		S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/0/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = XinASRC_BT_DL_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = (temp0 << 16) + (temp1 << 12) + (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 13. SMEM for DL_48_8_DEC pointer */
	/* DL_48_8_DEC = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/
		ASRC_BT_DL_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = DL_48_8_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_BT_DL_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_BT_DL_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 14. CMEM for DL_48_8_DEC pointer */
	/* DL_48_8_DEC = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/
		ASRC_BT_DL_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = DL_48_8_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_BT_DL_FIR_L + ASRC_margin) << 16) + (temp1 << 12)
		+ (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	i = i + 3;
	/* 15. SMEM for DL_48_16_DEC pointer */
	/* DL_48_16_DEC = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/
		ASRC_BT_DL_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_SMEM;
	mem_addr = DL_48_16_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	el[i + 1] = OMAP_ABE_S_XINASRC_BT_DL_ADDR >> 3;
	el[i + 1] = (el[i + 1] << 8) + (OMAP_ABE_S_XINASRC_BT_DL_SIZE >> 3);
	el[i + 2] = temp0;
	i = i + 3;
	/* 16. CMEM for DL_48_16_DEC pointer */
	/* DL_48_16_DEC = S_XinASRC_BT_DL_ADDR/S_XinASRC_BT_DL_sizeof/
		ASRC_BT_DL_FIR_L+ASRC_margin/1/0/0/0/0 */
	mem_tag = ABE_CMEM;
	mem_addr = DL_48_16_DEC_labelID;
	el[i] = (mem_tag << 16) + (mem_addr << 2);
	/* el[i+1] = iam1<<16 + inc1<<12 + iam2<<4 + inc2 */
	el[i + 1] = ((ASRC_BT_DL_FIR_L + ASRC_margin) << 16) + (temp1 << 12)
		+ (temp0 << 4) + temp0;
	/* dummy field */
	el[i + 2] = temp0;
	abe_write_fifo(ABE_DMEM, OMAP_ABE_D_FWMEMINITDESCR_ADDR, (u32 *) &el[0],
		       n_fifo_el);
}
