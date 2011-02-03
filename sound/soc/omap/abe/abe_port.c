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
#include "abe_port.h"
#include "abe_dbg.h"
#include "abe_mem.h"
#include "abe_gain.h"

/**
 * abe_clean_temporay buffers
 *
 * clear temporary buffers
 */
void omap_abe_clean_temporary_buffers(struct omap_abe *abe, u32 id)
{
	switch (id) {
	case OMAP_ABE_DMIC_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_DMIC_UL_FIFO_ADDR,
				   OMAP_ABE_D_DMIC_UL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_DMIC0_96_48_DATA_ADDR,
				   OMAP_ABE_S_DMIC0_96_48_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_DMIC1_96_48_DATA_ADDR,
				   OMAP_ABE_S_DMIC1_96_48_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_DMIC2_96_48_DATA_ADDR,
				   OMAP_ABE_S_DMIC2_96_48_DATA_SIZE);
		/* reset working values of the gain, target gain is preserved */
		omap_abe_reset_gain_mixer(abe, GAINS_DMIC1, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DMIC1, GAIN_RIGHT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DMIC2, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DMIC2, GAIN_RIGHT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DMIC3, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DMIC3, GAIN_RIGHT_OFFSET);
		break;
	case OMAP_ABE_PDM_UL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MCPDM_UL_FIFO_ADDR,
				   OMAP_ABE_D_MCPDM_UL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_AMIC_96_48_DATA_ADDR,
				   OMAP_ABE_S_AMIC_96_48_DATA_SIZE);
		/* reset working values of the gain, target gain is preserved */
		omap_abe_reset_gain_mixer(abe, GAINS_AMIC, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_AMIC, GAIN_RIGHT_OFFSET);
		break;
	case OMAP_ABE_BT_VX_UL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_BT_UL_FIFO_ADDR,
				   OMAP_ABE_D_BT_UL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_UL_ADDR,
				   OMAP_ABE_S_BT_UL_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_UL_8_48_HP_DATA_ADDR,
				   OMAP_ABE_S_BT_UL_8_48_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_UL_8_48_LP_DATA_ADDR,
				   OMAP_ABE_S_BT_UL_8_48_LP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_UL_16_48_HP_DATA_ADDR,
				   OMAP_ABE_S_BT_UL_16_48_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_UL_16_48_LP_DATA_ADDR,
				   OMAP_ABE_S_BT_UL_16_48_LP_DATA_SIZE);
		/* reset working values of the gain, target gain is preserved */
		omap_abe_reset_gain_mixer(abe, GAINS_BTUL, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_BTUL, GAIN_RIGHT_OFFSET);
		break;
	case OMAP_ABE_MM_UL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MM_UL_FIFO_ADDR,
				   OMAP_ABE_D_MM_UL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_MM_UL_ADDR,
				   OMAP_ABE_S_MM_UL_SIZE);
		break;
	case OMAP_ABE_MM_UL2_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MM_UL2_FIFO_ADDR,
				   OMAP_ABE_D_MM_UL2_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_MM_UL2_ADDR,
				   OMAP_ABE_S_MM_UL2_SIZE);
		break;
	case OMAP_ABE_VX_UL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_VX_UL_FIFO_ADDR,
				   OMAP_ABE_D_VX_UL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_UL_ADDR,
				   OMAP_ABE_S_VX_UL_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_UL_48_8_HP_DATA_ADDR,
				   OMAP_ABE_S_VX_UL_48_8_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_UL_48_8_LP_DATA_ADDR,
				   OMAP_ABE_S_VX_UL_48_8_LP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_UL_48_16_HP_DATA_ADDR,
				   OMAP_ABE_S_VX_UL_48_16_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_UL_48_16_LP_DATA_ADDR,
				   OMAP_ABE_S_VX_UL_48_16_LP_DATA_SIZE);
		omap_abe_reset_gain_mixer(abe, MIXAUDUL, MIX_AUDUL_INPUT_UPLINK);
		break;
	case OMAP_ABE_MM_DL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MM_DL_FIFO_ADDR,
				   OMAP_ABE_D_MM_DL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_MM_DL_ADDR,
				   OMAP_ABE_S_MM_DL_SIZE);
		omap_abe_reset_gain_mixer(abe, MIXDL1, MIX_DL1_INPUT_MM_DL);
		omap_abe_reset_gain_mixer(abe, MIXDL2, MIX_DL2_INPUT_MM_DL);
		break;
	case OMAP_ABE_VX_DL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_VX_DL_FIFO_ADDR,
				   OMAP_ABE_D_VX_DL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_DL_ADDR,
				   OMAP_ABE_S_VX_DL_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_DL_8_48_HP_DATA_ADDR,
				   OMAP_ABE_S_VX_DL_8_48_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_DL_8_48_LP_DATA_ADDR,
				   OMAP_ABE_S_VX_DL_8_48_LP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_DL_16_48_HP_DATA_ADDR,
				   OMAP_ABE_S_VX_DL_16_48_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VX_DL_16_48_LP_DATA_ADDR,
				   OMAP_ABE_S_VX_DL_16_48_LP_DATA_SIZE);
		omap_abe_reset_gain_mixer(abe, MIXDL1, MIX_DL1_INPUT_VX_DL);
		omap_abe_reset_gain_mixer(abe, MIXDL2, MIX_DL2_INPUT_VX_DL);
		break;
	case OMAP_ABE_TONES_DL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_TONES_DL_FIFO_ADDR,
				   OMAP_ABE_D_TONES_DL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_TONES_ADDR,
				   OMAP_ABE_S_TONES_SIZE);
		omap_abe_reset_gain_mixer(abe, MIXDL1, MIX_DL1_INPUT_TONES);
		omap_abe_reset_gain_mixer(abe, MIXDL2, MIX_DL2_INPUT_TONES);
		break;
	case OMAP_ABE_VIB_DL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_VIB_DL_FIFO_ADDR,
				   OMAP_ABE_D_VIB_DL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_VIBRA_ADDR,
				   OMAP_ABE_S_VIBRA_SIZE);
		break;
	case OMAP_ABE_BT_VX_DL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_BT_DL_FIFO_ADDR,
				   OMAP_ABE_D_BT_DL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_DL_ADDR,
				   OMAP_ABE_S_BT_DL_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_DL_48_8_HP_DATA_ADDR,
				   OMAP_ABE_S_BT_DL_48_8_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_DL_48_8_LP_DATA_ADDR,
				   OMAP_ABE_S_BT_DL_48_8_LP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_DL_48_16_HP_DATA_ADDR,
				   OMAP_ABE_S_BT_DL_48_16_HP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_BT_DL_48_16_LP_DATA_ADDR,
				   OMAP_ABE_S_BT_DL_48_16_LP_DATA_SIZE);
		break;
	case OMAP_ABE_PDM_DL_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MCPDM_DL_FIFO_ADDR,
				   OMAP_ABE_D_MCPDM_DL_FIFO_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_DL2_M_LR_EQ_DATA_ADDR,
				   OMAP_ABE_S_DL2_M_LR_EQ_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_DL1_M_EQ_DATA_ADDR,
				   OMAP_ABE_S_DL1_M_EQ_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_EARP_48_96_LP_DATA_ADDR,
				   OMAP_ABE_S_EARP_48_96_LP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_IHF_48_96_LP_DATA_ADDR,
				   OMAP_ABE_S_IHF_48_96_LP_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_APS_DL1_EQ_DATA_ADDR,
				   OMAP_ABE_S_APS_DL1_EQ_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_APS_DL2_EQ_DATA_ADDR,
				   OMAP_ABE_S_APS_DL2_EQ_DATA_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_APS_DL2_L_IIRMEM1_ADDR,
				   OMAP_ABE_S_APS_DL2_L_IIRMEM1_SIZE);
		omap_abe_reset_mem(abe, OMAP_ABE_SMEM,
				   OMAP_ABE_S_APS_DL2_R_IIRMEM1_ADDR,
				   OMAP_ABE_S_APS_DL2_R_IIRMEM1_SIZE);
		omap_abe_reset_gain_mixer(abe, GAINS_DL1, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DL1, GAIN_RIGHT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DL2, GAIN_LEFT_OFFSET);
		omap_abe_reset_gain_mixer(abe, GAINS_DL2, GAIN_RIGHT_OFFSET);
		omap_abe_reset_gain_mixer(abe, MIXSDT, MIX_SDT_INPUT_UP_MIXER);
		omap_abe_reset_gain_mixer(abe, MIXSDT, MIX_SDT_INPUT_DL1_MIXER);
		break;
	case OMAP_ABE_MM_EXT_OUT_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MM_EXT_OUT_FIFO_ADDR,
				   OMAP_ABE_D_MM_EXT_OUT_FIFO_SIZE);
		break;
	case OMAP_ABE_MM_EXT_IN_PORT:
		omap_abe_reset_mem(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MM_EXT_IN_FIFO_ADDR,
				   OMAP_ABE_D_MM_EXT_IN_FIFO_SIZE);
		break;
	}
}

/**
 * omap_abe_disable_enable_dma_request
 * Parameter:
 * Operations:
 * Return value:
 */
void omap_abe_disable_enable_dma_request(struct omap_abe *abe, u32 id,
					 u32 on_off)
{
	u8 desc_third_word[4], irq_dmareq_field;
	u32 sio_desc_address;
	u32 struct_offset;
	struct ABE_SIODescriptor sio_desc;
	struct ABE_SPingPongDescriptor desc_pp;

	if (abe_port[id].protocol.protocol_switch == PINGPONG_PORT_PROT) {
		irq_dmareq_field =
			(u8) (on_off *
			      abe_port[id].protocol.p.prot_pingpong.irq_data);
		sio_desc_address = OMAP_ABE_D_PINGPONGDESC_ADDR;
		struct_offset = (u32) &(desc_pp.data_size) - (u32) &(desc_pp);
		omap_abe_mem_read(abe, OMAP_ABE_DMEM,
			       sio_desc_address + struct_offset,
			       (u32 *) desc_third_word, 4);
		desc_third_word[2] = irq_dmareq_field;
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       sio_desc_address + struct_offset,
			       (u32 *) desc_third_word, 4);
	} else {
		/* serial interface: sync ATC with Firmware activity */
		sio_desc_address =
			OMAP_ABE_D_IODESCR_ADDR +
			(id * sizeof(struct ABE_SIODescriptor));
		omap_abe_mem_read(abe, OMAP_ABE_DMEM,
			sio_desc_address, (u32 *) &sio_desc,
			sizeof(sio_desc));
		if (on_off) {
			sio_desc.atc_irq_data =
				(u8) abe_port[id].protocol.p.prot_dmareq.
				dma_data;
			sio_desc.on_off = 0x80;
		} else {
			sio_desc.atc_irq_data = 0;
			sio_desc.on_off = 0;
		}
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			sio_desc_address, (u32 *) &sio_desc,
			sizeof(sio_desc));
	}

}

/**
 * omap_abe_enable_dma_request
 *
 * Parameter:
 * Operations:
 * Return value:
 *
 */
void omap_abe_enable_dma_request(struct omap_abe *abe, u32 id)
{
	omap_abe_disable_enable_dma_request(abe, id, 1);
}

/**
 * omap_abe_disable_dma_request
 *
 * Parameter:
 * Operations:
 * Return value:
 *
 */
void omap_abe_disable_dma_request(struct omap_abe *abe, u32 id)
{
	omap_abe_disable_enable_dma_request(abe, id, 0);
}

/**
 * abe_init_atc
 * @id: ABE port ID
 *
 * load the DMEM ATC/AESS descriptors
 */
void omap_abe_init_atc(struct omap_abe *abe, u32 id)
{
	u8 iter;
	s32 datasize;
	struct omap_abe_atc_desc atc_desc;

#define JITTER_MARGIN 4
	/* load default values of the descriptor */
	atc_desc.rdpt = 0;
	atc_desc.wrpt = 0;
	atc_desc.irqdest = 0;
	atc_desc.cberr = 0;
	atc_desc.desen = 0;
	atc_desc.nw = 0;
	atc_desc.reserved0 = 0;
	atc_desc.reserved1 = 0;
	atc_desc.reserved2 = 0;
	atc_desc.srcid = 0;
	atc_desc.destid = 0;
	atc_desc.badd = 0;
	atc_desc.iter = 0;
	atc_desc.cbsize = 0;
	datasize = abe_dma_port_iter_factor(&((abe_port[id]).format));
	iter = (u8) abe_dma_port_iteration(&((abe_port[id]).format));
	/* if the ATC FIFO is too small there will be two ABE firmware
	   utasks to do the copy this happems on DMIC and MCPDMDL */
	/* VXDL_8kMono = 4 = 2 + 2x1 */
	/* VXDL_16kstereo = 12 = 8 + 2x2 */
	/* MM_DL_1616 = 14 = 12 + 2x1 */
	/* DMIC = 84 = 72 + 2x6 */
	/* VXUL_8kMono = 2 */
	/* VXUL_16kstereo = 4 */
	/* MM_UL2_Stereo = 4 */
	/* PDMDL = 12 */
	/* IN from AESS point of view */
	if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
		if (iter + 2 * datasize > 126)
			atc_desc.wrpt = (iter >> 1) +
				((JITTER_MARGIN-1) * datasize);
		else
			atc_desc.wrpt = iter + ((JITTER_MARGIN-1) * datasize);
	else
		atc_desc.wrpt = 0 + ((JITTER_MARGIN+1) * datasize);
	switch ((abe_port[id]).protocol.protocol_switch) {
	case SLIMBUS_PORT_PROT:
		atc_desc.cbdir = (abe_port[id]).protocol.direction;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_slimbus.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_slimbus.buf_addr1) >> 4;
		atc_desc.iter = (abe_port[id]).protocol.p.prot_slimbus.iter;
		atc_desc.srcid =
			abe_atc_srcid[(abe_port[id]).protocol.p.prot_slimbus.
				      desc_addr1 >> 3];
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (abe_port[id]).protocol.p.prot_slimbus.
			       desc_addr1, (u32 *) &atc_desc, sizeof(atc_desc));
		atc_desc.badd =
			(abe_port[id]).protocol.p.prot_slimbus.buf_addr2;
		atc_desc.srcid =
			abe_atc_srcid[(abe_port[id]).protocol.p.prot_slimbus.
				      desc_addr2 >> 3];
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (abe_port[id]).protocol.p.prot_slimbus.
			       desc_addr2, (u32 *) &atc_desc, sizeof(atc_desc));
		break;
	case SERIAL_PORT_PROT:
		atc_desc.cbdir = (abe_port[id]).protocol.direction;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_serial.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_serial.buf_addr) >> 4;
		atc_desc.iter = (abe_port[id]).protocol.p.prot_serial.iter;
		atc_desc.srcid =
			abe_atc_srcid[(abe_port[id]).protocol.p.prot_serial.
				      desc_addr >> 3];
		atc_desc.destid =
			abe_atc_dstid[(abe_port[id]).protocol.p.prot_serial.
				      desc_addr >> 3];
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (abe_port[id]).protocol.p.prot_serial.desc_addr,
			       (u32 *) &atc_desc, sizeof(atc_desc));
		break;
	case DMIC_PORT_PROT:
		atc_desc.cbdir = ABE_ATC_DIRECTION_IN;
		atc_desc.cbsize = (abe_port[id]).protocol.p.prot_dmic.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_dmic.buf_addr) >> 4;
		atc_desc.iter = DMIC_ITER;
		atc_desc.srcid = abe_atc_srcid[ABE_ATC_DMIC_DMA_REQ];
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (ABE_ATC_DMIC_DMA_REQ*ATC_SIZE),
			       (u32 *) &atc_desc, sizeof(atc_desc));
		break;
	case MCPDMDL_PORT_PROT:
		atc_desc.cbdir = ABE_ATC_DIRECTION_OUT;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_mcpdmdl.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_mcpdmdl.buf_addr) >> 4;
		atc_desc.iter = MCPDM_DL_ITER;
		atc_desc.destid = abe_atc_dstid[ABE_ATC_MCPDMDL_DMA_REQ];
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (ABE_ATC_MCPDMDL_DMA_REQ*ATC_SIZE),
			       (u32 *) &atc_desc, sizeof(atc_desc));
		break;
	case MCPDMUL_PORT_PROT:
		atc_desc.cbdir = ABE_ATC_DIRECTION_IN;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_mcpdmul.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_mcpdmul.buf_addr) >> 4;
		atc_desc.iter = MCPDM_UL_ITER;
		atc_desc.srcid = abe_atc_srcid[ABE_ATC_MCPDMUL_DMA_REQ];
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (ABE_ATC_MCPDMUL_DMA_REQ*ATC_SIZE),
			       (u32 *) &atc_desc, sizeof(atc_desc));
		break;
	case PINGPONG_PORT_PROT:
		/* software protocol, nothing to do on ATC */
		break;
	case DMAREQ_PORT_PROT:
		atc_desc.cbdir = (abe_port[id]).protocol.direction;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_dmareq.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_dmareq.buf_addr) >> 4;
		/* CBPr needs ITER=1.
		It is the job of eDMA to do the iterations */
		atc_desc.iter = 1;
		/* input from ABE point of view */
		if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN) {
			/* atc_atc_desc.rdpt = 127; */
			/* atc_atc_desc.wrpt = 0; */
			atc_desc.srcid = abe_atc_srcid
				[(abe_port[id]).protocol.p.prot_dmareq.
				 desc_addr >> 3];
		} else {
			/* atc_atc_desc.rdpt = 0; */
			/* atc_atc_desc.wrpt = 127; */
			atc_desc.destid = abe_atc_dstid
				[(abe_port[id]).protocol.p.prot_dmareq.
				 desc_addr >> 3];
		}
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
			       (u32 *) &atc_desc, sizeof(atc_desc));
		break;
	}
}

/**
 * omap_abe_enable_pp_io_task
 * @id: port_id
 *
 *
 */
void omap_abe_enable_pp_io_task(struct omap_abe *abe, u32 id)
{
	if (OMAP_ABE_MM_DL_PORT == id) {
		/* MM_DL managed in ping-pong */
		abe->MultiFrame[TASK_IO_MM_DL_SLT][TASK_IO_MM_DL_IDX] =
			ABE_TASK_ID(C_ABE_FW_TASK_IO_PING_PONG);
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       OMAP_ABE_D_MULTIFRAME_ADDR, (u32 *) abe->MultiFrame,
			       sizeof(abe->MultiFrame));
	} else {
		/* ping_pong is only supported on MM_DL */
		omap_abe_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	}
}
/**
 * omap_abe_disable_pp_io_task
 * @id: port_id
 *
 *
 */
void omap_abe_disable_pp_io_task(struct omap_abe *abe, u32 id)
{
	if (OMAP_ABE_MM_DL_PORT == id) {
		/* MM_DL managed in ping-pong */
		abe->MultiFrame[TASK_IO_MM_DL_SLT][TASK_IO_MM_DL_IDX] = 0;
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       OMAP_ABE_D_MULTIFRAME_ADDR, (u32 *) abe->MultiFrame,
			       sizeof(abe->MultiFrame));
	} else {
		/* ping_pong is only supported on MM_DL */
		omap_abe_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	}
}

/**
 * omap_abe_disable_data_transfer
 * @id: ABE port id
 *
 * disables the ATC descriptor and stop IO/port activities
 * disable the IO task (@f = 0)
 * clear ATC DMEM buffer, ATC enabled
 */
int omap_abe_disable_data_transfer(struct omap_abe *abe, u32 id)
{

	_log(ABE_ID_DISABLE_DATA_TRANSFER, id, 0, 0);

	/* MM_DL managed in ping-pong */
	if (id == OMAP_ABE_MM_DL_PORT) {
		if (abe_port[OMAP_ABE_MM_DL_PORT].protocol.protocol_switch == PINGPONG_PORT_PROT)
			omap_abe_disable_pp_io_task(abe, OMAP_ABE_MM_DL_PORT);
	}
	/* local host variable status= "port is running" */
	abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_IDLE;
	/* disable DMA requests */
	omap_abe_disable_dma_request(abe, id);
	/* disable ATC transfers */
	omap_abe_init_atc(abe, id);
	omap_abe_clean_temporary_buffers(abe, id);
	/* select the main port based on the desactivation of this port */
	abe_decide_main_port();

	return 0;
}
EXPORT_SYMBOL(omap_abe_disable_data_transfer);

/**
 * omap_abe_enable_data_transfer
 * @ip: ABE port id
 *
 * enables the ATC descriptor
 * reset ATC pointers
 * enable the IO task (@f <> 0)
 */
int omap_abe_enable_data_transfer(struct omap_abe *abe, u32 id)
{
	abe_port_protocol_t *protocol;
	abe_data_format_t format;

	_log(ABE_ID_ENABLE_DATA_TRANSFER, id, 0, 0);
	omap_abe_clean_temporary_buffers(abe, id);
	if (id == OMAP_ABE_PDM_UL_PORT) {
		/* initializes the ABE ATC descriptors in DMEM - MCPDM_UL */
		protocol = &(abe_port[OMAP_ABE_PDM_UL_PORT].protocol);
		format = abe_port[OMAP_ABE_PDM_UL_PORT].format;
		omap_abe_init_atc(abe, OMAP_ABE_PDM_UL_PORT);
		abe_init_io_tasks(OMAP_ABE_PDM_UL_PORT, &format, protocol);
	}
	if (id == OMAP_ABE_PDM_DL_PORT) {
		/* initializes the ABE ATC descriptors in DMEM - MCPDM_DL */
		protocol = &(abe_port[OMAP_ABE_PDM_DL_PORT].protocol);
		format = abe_port[OMAP_ABE_PDM_DL_PORT].format;
		omap_abe_init_atc(abe, OMAP_ABE_PDM_DL_PORT);
		abe_init_io_tasks(OMAP_ABE_PDM_DL_PORT, &format, protocol);
	}
	/* MM_DL managed in ping-pong */
	if (id == OMAP_ABE_MM_DL_PORT) {
		protocol = &(abe_port[OMAP_ABE_MM_DL_PORT].protocol);
		if (protocol->protocol_switch == PINGPONG_PORT_PROT)
			omap_abe_enable_pp_io_task(abe, OMAP_ABE_MM_DL_PORT);
	}
	if (id == OMAP_ABE_DMIC_PORT) {
		/* one DMIC port enabled = all DMICs enabled,
		 * since there is a single DMIC path for all DMICs */
		protocol = &(abe_port[OMAP_ABE_DMIC_PORT].protocol);
		format = abe_port[OMAP_ABE_DMIC_PORT].format;
		omap_abe_init_atc(abe, OMAP_ABE_DMIC_PORT);
		abe_init_io_tasks(OMAP_ABE_DMIC_PORT, &format, protocol);
	}
	if (id == OMAP_ABE_VX_UL_PORT) {
		/* Init VX_UL ASRC and enable its adaptation */
		abe_init_asrc_vx_ul(250);
	}
	if (id == OMAP_ABE_VX_DL_PORT) {
		/* Init VX_DL ASRC and enable its adaptation */
		abe_init_asrc_vx_dl(250);
	}
	/* local host variable status= "port is running" */
	abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;
	/* enable DMA requests */
	omap_abe_enable_dma_request(abe, id);
	/* select the main port based on the activation of this new port */
	abe_decide_main_port();

	return 0;
}
EXPORT_SYMBOL(omap_abe_enable_data_transfer);

/**
 * omap_abe_connect_cbpr_dmareq_port
 * @id: port name
 * @f: desired data format
 * @d: desired dma_request line (0..7)
 * @a: returned pointer to the base address of the CBPr register and number of
 *	samples to exchange during a DMA_request.
 *
 * enables the data echange between a DMA and the ABE through the
 *	CBPr registers of AESS.
 */
int omap_abe_connect_cbpr_dmareq_port(struct omap_abe *abe,
						u32 id, abe_data_format_t *f,
						u32 d,
						abe_dma_t *returned_dma_t)
{
	_log(ABE_ID_CONNECT_CBPR_DMAREQ_PORT, id, f->f, f->samp_format);

	abe_port[id] = ((abe_port_t *) abe_port_init)[id];
	(abe_port[id]).format = (*f);
	abe_port[id].protocol.protocol_switch = DMAREQ_PORT_PROT;
	abe_port[id].protocol.p.prot_dmareq.iter = abe_dma_port_iteration(f);
	abe_port[id].protocol.p.prot_dmareq.dma_addr = ABE_DMASTATUS_RAW;
	abe_port[id].protocol.p.prot_dmareq.dma_data = (1 << d);
	abe_port[id].status = OMAP_ABE_PORT_INITIALIZED;
	/* load the dma_t with physical information from AE memory mapping */
	abe_init_dma_t(id, &((abe_port[id]).protocol));
	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port[id]).format),
			  &((abe_port[id]).protocol));
	/* load the ATC descriptors - disabled */
	omap_abe_init_atc(abe, id);
	/* return the dma pointer address */
	abe_read_port_address(id, returned_dma_t);
	return 0;
}
EXPORT_SYMBOL(omap_abe_connect_cbpr_dmareq_port);

/**
 * omap_abe_connect_irq_ping_pong_port
 * @id: port name
 * @f: desired data format
 * @I: index of the call-back subroutine to call
 * @s: half-buffer (ping) size
 * @p: returned base address of the first (ping) buffer)
 *
 * enables the data echanges between a direct access to the DMEM
 * memory of ABE using cache flush. On each IRQ activation a subroutine
 * registered with "abe_plug_subroutine" will be called. This subroutine
 * will generate an amount of samples, send them to DMEM memory and call
 * "abe_set_ping_pong_buffer" to notify the new amount of samples in the
 * pong buffer.
 */
int omap_abe_connect_irq_ping_pong_port(struct omap_abe *abe,
					u32 id, abe_data_format_t *f,
					u32 subroutine_id, u32 size,
					u32 *sink, u32 dsp_mcu_flag)
{
	_log(ABE_ID_CONNECT_IRQ_PING_PONG_PORT, id, f->f, f->samp_format);

	/* ping_pong is only supported on MM_DL */
	if (id != OMAP_ABE_MM_DL_PORT) {
		omap_abe_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	}
	abe_port[id] = ((abe_port_t *) abe_port_init)[id];
	(abe_port[id]).format = (*f);
	(abe_port[id]).protocol.protocol_switch = PINGPONG_PORT_PROT;
	(abe_port[id]).protocol.p.prot_pingpong.buf_addr =
		OMAP_ABE_D_PING_ADDR;
	(abe_port[id]).protocol.p.prot_pingpong.buf_size = size;
	(abe_port[id]).protocol.p.prot_pingpong.irq_data = (1);
	abe_init_ping_pong_buffer(OMAP_ABE_MM_DL_PORT, size, 2, sink);
	if (dsp_mcu_flag == PING_PONG_WITH_MCU_IRQ)
		(abe_port[id]).protocol.p.prot_pingpong.irq_addr =
			ABE_MCU_IRQSTATUS_RAW;
	if (dsp_mcu_flag == PING_PONG_WITH_DSP_IRQ)
		(abe_port[id]).protocol.p.prot_pingpong.irq_addr =
			ABE_DSP_IRQSTATUS_RAW;
	abe_port[id].status = OMAP_ABE_PORT_INITIALIZED;
	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port[id]).format),
			  &((abe_port[id]).protocol));
	/* load the ATC descriptors - disabled */
	omap_abe_init_atc(abe, id);
	*sink = (abe_port[id]).protocol.p.prot_pingpong.buf_addr;
	return 0;
}
EXPORT_SYMBOL(omap_abe_connect_irq_ping_pong_port);

/**
 * omap_abe_connect_serial_port()
 * @id: port name
 * @f: data format
 * @i: peripheral ID (McBSP #1, #2, #3)
 *
 * Operations : enables the data echanges between a McBSP and an ATC buffer in
 * DMEM. This API is used connect 48kHz McBSP streams to MM_DL and 8/16kHz
 * voice streams to VX_UL, VX_DL, BT_VX_UL, BT_VX_DL. It abstracts the
 * abe_write_port API.
 */
int omap_abe_connect_serial_port(struct omap_abe *abe,
				 u32 id, abe_data_format_t *f,
				 u32 mcbsp_id)
{
	_log(ABE_ID_CONNECT_SERIAL_PORT, id, f->samp_format, mcbsp_id);

	abe_port[id] = ((abe_port_t *) abe_port_init)[id];
	(abe_port[id]).format = (*f);
	(abe_port[id]).protocol.protocol_switch = SERIAL_PORT_PROT;
	/* McBSP peripheral connected to ATC */
	(abe_port[id]).protocol.p.prot_serial.desc_addr = mcbsp_id*ATC_SIZE;
	/* check the iteration of ATC */
	(abe_port[id]).protocol.p.prot_serial.iter =
		abe_dma_port_iter_factor(f);
	abe_port[id].status = OMAP_ABE_PORT_INITIALIZED;
	/* load the micro-task parameters */
	abe_init_io_tasks(id, &((abe_port[id]).format),
			  &((abe_port[id]).protocol));
	/* load the ATC descriptors - disabled */
	omap_abe_init_atc(abe, id);

	return 0;
}
EXPORT_SYMBOL(omap_abe_connect_serial_port);

/**
 * omap_abe_read_port_address
 * @dma: output pointer to the DMA iteration and data destination pointer
 *
 * This API returns the address of the DMA register used on this audio port.
 * Depending on the protocol being used, adds the base address offset L3
 * (DMA) or MPU (ARM)
 */
int omap_abe_read_port_address(struct omap_abe *abe,
					 u32 port, abe_dma_t *dma2)
{
	abe_dma_t_offset dma1;
	u32 protocol_switch;

	_log(ABE_ID_READ_PORT_ADDRESS, port, 0, 0);

	dma1 = (abe_port[port]).dma;
	protocol_switch = abe_port[port].protocol.protocol_switch;
	switch (protocol_switch) {
	case PINGPONG_PORT_PROT:
		/* return the base address of the buffer in L3 and L4 spaces */
		(*dma2).data = (void *)(dma1.data +
			ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU);
		(*dma2).l3_dmem = (void *)(dma1.data +
			ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU);
		(*dma2).l4_dmem = (void *)(dma1.data +
			ABE_DEFAULT_BASE_ADDRESS_L4 + ABE_DMEM_BASE_OFFSET_MPU);
		break;
	case DMAREQ_PORT_PROT:
		/* return the CBPr(L3), DMEM(L3), DMEM(L4) address */
		(*dma2).data = (void *)(dma1.data +
			ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_ATC_BASE_OFFSET_MPU);
		(*dma2).l3_dmem =
			(void *)((abe_port[port]).protocol.p.prot_dmareq.buf_addr +
			ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU);
		(*dma2).l4_dmem =
			(void *)((abe_port[port]).protocol.p.prot_dmareq.buf_addr +
			ABE_DEFAULT_BASE_ADDRESS_L4 + ABE_DMEM_BASE_OFFSET_MPU);
		break;
	default:
		break;
	}
	(*dma2).iter = (dma1.iter);

	return 0;
}
EXPORT_SYMBOL(omap_abe_read_port_address);

/**
 * abe_init_dma_t
 * @ id: ABE port ID
 * @ prot: protocol being used
 *
 * load the dma_t with physical information from AE memory mapping
 */
void abe_init_dma_t(u32 id, abe_port_protocol_t *prot)
{
	abe_dma_t_offset dma;
	u32 idx;
	/* default dma_t points to address 0000... */
	dma.data = 0;
	dma.iter = 0;
	switch (prot->protocol_switch) {
	case PINGPONG_PORT_PROT:
		for (idx = 0; idx < 32; idx++) {
			if (((prot->p).prot_pingpong.irq_data) ==
			    (u32) (1 << idx))
				break;
		}
		(prot->p).prot_dmareq.desc_addr =
			((CBPr_DMA_RTX0 + idx)*ATC_SIZE);
		/* translate byte address/size in DMEM words */
		dma.data = (prot->p).prot_pingpong.buf_addr >> 2;
		dma.iter = (prot->p).prot_pingpong.buf_size >> 2;
		break;
	case DMAREQ_PORT_PROT:
		for (idx = 0; idx < 32; idx++) {
			if (((prot->p).prot_dmareq.dma_data) ==
			    (u32) (1 << idx))
				break;
		}
		dma.data = (CIRCULAR_BUFFER_PERIPHERAL_R__0 + (idx << 2));
		dma.iter = (prot->p).prot_dmareq.iter;
		(prot->p).prot_dmareq.desc_addr =
			((CBPr_DMA_RTX0 + idx)*ATC_SIZE);
		break;
	case SLIMBUS_PORT_PROT:
	case SERIAL_PORT_PROT:
	case DMIC_PORT_PROT:
	case MCPDMDL_PORT_PROT:
	case MCPDMUL_PORT_PROT:
	default:
		break;
	}
	/* upload the dma type */
	abe_port[id].dma = dma;
}

/**
 * abe_enable_atc
 * Parameter:
 * Operations:
 * Return value:
 */
void abe_enable_atc(u32 id)
{
	struct omap_abe_atc_desc atc_desc;

	omap_abe_mem_read(abe, OMAP_ABE_DMEM,
		       (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
		       (u32 *) &atc_desc, sizeof(atc_desc));
	atc_desc.desen = 1;
	omap_abe_mem_write(abe, OMAP_ABE_DMEM,
		       (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
		       (u32 *) &atc_desc, sizeof(atc_desc));

}
/**
 * abe_disable_atc
 * Parameter:
 * Operations:
 * Return value:
 */
void abe_disable_atc(u32 id)
{
	struct omap_abe_atc_desc atc_desc;

	omap_abe_mem_read(abe, OMAP_ABE_DMEM,
		       (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
		       (u32 *) &atc_desc, sizeof(atc_desc));
	atc_desc.desen = 0;
	omap_abe_mem_write(abe, OMAP_ABE_DMEM,
		       (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
		       (u32 *) &atc_desc, sizeof(atc_desc));

}
/**
 * abe_init_io_tasks
 * @prot : protocol being used
 *
 * load the micro-task parameters doing to DMEM <==> SMEM data moves
 *
 * I/O descriptors input parameters :
 * For Read from DMEM usually THR1/THR2 = X+1/X-1
 * For Write to DMEM usually THR1/THR2 = 2/0
 * UP_1/2 =X+1/X-1
 */
void abe_init_io_tasks(u32 id, abe_data_format_t *format,
		       abe_port_protocol_t *prot)
{
	u32 x_io, direction, iter_samples, smem1, smem2, smem3, io_sub_id,
		io_flag;
	u32 copy_func_index, before_func_index, after_func_index;
	u32 dmareq_addr, dmareq_field;
	u32 sio_desc_address, datasize, iter, nsamp, datasize2, dOppMode32;
	u32 atc_ptr_saved, atc_ptr_saved2, copy_func_index1;
	u32 copy_func_index2, atc_desc_address1, atc_desc_address2;
	struct ABE_SPingPongDescriptor desc_pp;
	struct ABE_SIODescriptor sio_desc;

	if (prot->protocol_switch == PINGPONG_PORT_PROT) {
		/* ping_pong is only supported on MM_DL */
		if (OMAP_ABE_MM_DL_PORT != id) {
			omap_abe_dbg_error(abe, OMAP_ABE_ERR_API,
					   ABE_PARAMETER_ERROR);
		}
		smem1 = smem_mm_dl;
		copy_func_index = (u8) abe_dma_port_copy_subroutine_id(id);
		dmareq_addr = abe_port[id].protocol.p.prot_pingpong.irq_addr;
		dmareq_field = abe_port[id].protocol.p.prot_pingpong.irq_data;
		datasize = abe_dma_port_iter_factor(format);
		/* number of "samples" either mono or stereo */
		iter = abe_dma_port_iteration(format);
		iter_samples = (iter / datasize);
		/* load the IO descriptor */
		/* no drift */
		desc_pp.drift_ASRC = 0;
		/* no drift */
		desc_pp.drift_io = 0;
		desc_pp.hw_ctrl_addr = (u16) dmareq_addr;
		desc_pp.copy_func_index = (u8) copy_func_index;
		desc_pp.smem_addr = (u8) smem1;
		/* DMA req 0 is used for CBPr0 */
		desc_pp.atc_irq_data = (u8) dmareq_field;
		/* size of block transfer */
		desc_pp.x_io = (u8) iter_samples;
		desc_pp.data_size = (u8) datasize;
		/* address comunicated in Bytes */
		desc_pp.workbuff_BaseAddr =
			(u16) (abe_base_address_pingpong[1]);
		/* size comunicated in XIO sample */
		desc_pp.workbuff_Samples = 0;
		desc_pp.nextbuff0_BaseAddr =
			(u16) (abe_base_address_pingpong[0]);
		desc_pp.nextbuff1_BaseAddr =
			(u16) (abe_base_address_pingpong[1]);
		if (dmareq_addr == ABE_DMASTATUS_RAW) {
			desc_pp.nextbuff0_Samples =
				(u16) ((abe_size_pingpong >> 2) / datasize);
			desc_pp.nextbuff1_Samples =
				(u16) ((abe_size_pingpong >> 2) / datasize);
		} else {
			desc_pp.nextbuff0_Samples = 0;
			desc_pp.nextbuff1_Samples = 0;
		}
		/* next buffer to send is B1, first IRQ fills B0 */
		desc_pp.counter = 0;
		/* send a DMA req to fill B0 with N samples
		   abe_block_copy (COPY_FROM_HOST_TO_ABE,
			ABE_ATC,
			ABE_DMASTATUS_RAW,
			&(abe_port[id].protocol.p.prot_pingpong.irq_data),
			4); */
		sio_desc_address = OMAP_ABE_D_PINGPONGDESC_ADDR;
		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
			       sio_desc_address, (u32 *) &desc_pp,
			       sizeof(desc_pp));
	} else {
		io_sub_id = dmareq_addr = ABE_DMASTATUS_RAW;
		dmareq_field = 0;
		atc_desc_address1 = atc_desc_address2 = 0;
		/* default: repeat of the last downlink samples in case of
		   DMA errors, (disable=0x00) */
		io_flag = 0xFF;
		datasize2 = datasize = abe_dma_port_iter_factor(format);
		x_io = (u8) abe_dma_port_iteration(format);
		nsamp = (x_io / datasize);
		atc_ptr_saved2 = atc_ptr_saved = DMIC_ATC_PTR_labelID + id;
		smem1 = abe_port[id].smem_buffer1;
		smem3 = smem2 = abe_port[id].smem_buffer2;
		copy_func_index1 = (u8) abe_dma_port_copy_subroutine_id(id);
		before_func_index = after_func_index =
			copy_func_index2 = NULL_COPY_CFPID;
		switch (prot->protocol_switch) {
		case DMIC_PORT_PROT:
			/* DMIC port is read in two steps */
			x_io = x_io >> 1;
			nsamp = nsamp >> 1;
			atc_desc_address1 = (ABE_ATC_DMIC_DMA_REQ*ATC_SIZE);
			io_sub_id = IO_IP_CFPID;
			break;
		case MCPDMDL_PORT_PROT:
			/* PDMDL port is written to in two steps */
			x_io = x_io >> 1;
			atc_desc_address1 =
				(ABE_ATC_MCPDMDL_DMA_REQ*ATC_SIZE);
			io_sub_id = IO_IP_CFPID;
			break;
		case MCPDMUL_PORT_PROT:
			atc_desc_address1 =
				(ABE_ATC_MCPDMUL_DMA_REQ*ATC_SIZE);
			io_sub_id = IO_IP_CFPID;
			break;
		case SLIMBUS_PORT_PROT:
			atc_desc_address1 =
				abe_port[id].protocol.p.prot_slimbus.desc_addr1;
			atc_desc_address2 =
				abe_port[id].protocol.p.prot_slimbus.desc_addr2;
			copy_func_index2 = NULL_COPY_CFPID;
			/* @@@@@@
			   #define SPLIT_SMEM_CFPID 9
			   #define MERGE_SMEM_CFPID 10
			   #define SPLIT_TDM_12_CFPID 11
			   #define MERGE_TDM_12_CFPID 12
			 */
			io_sub_id = IO_IP_CFPID;
			break;
		case SERIAL_PORT_PROT:	/* McBSP/McASP */
			atc_desc_address1 =
				(s16) abe_port[id].protocol.p.prot_serial.
				desc_addr;
			io_sub_id = IO_IP_CFPID;
			break;
		case DMAREQ_PORT_PROT:	/* DMA w/wo CBPr */
			dmareq_addr =
				abe_port[id].protocol.p.prot_dmareq.dma_addr;
			dmareq_field = 0;
			atc_desc_address1 =
				abe_port[id].protocol.p.prot_dmareq.desc_addr;
			io_sub_id = IO_IP_CFPID;
			break;
		}
		/* special situation of the PING_PONG protocol which
		has its own SIO descriptor format */
		/*
		   Sequence of operations on ping-pong buffers B0/B1
		   -------------- time ---------------------------->>>>
		   Host Application is ready to send data from DDR to B0
		   SDMA is initialized from "abe_connect_irq_ping_pong_port" to B0
		   FIRMWARE starts with #12 B1 data,
		   sends IRQ/DMAreq, sends #pong B1 data,
		   sends IRQ/DMAreq, sends #ping B0,
		   sends B1 samples
		   ARM / SDMA | fills B0 | fills B1 ... | fills B0 ...
		   Counter 0 1 2 3
		 */
		switch (id) {
		case OMAP_ABE_PDM_DL_PORT:
			abe->MultiFrame[7][0] = ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_DL);
			abe->MultiFrame[19][0] = ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_DL);
			break;
		case OMAP_ABE_TONES_DL_PORT:
			abe->MultiFrame[20][0] = ABE_TASK_ID(C_ABE_FW_TASK_IO_TONES_DL);
			break;
		case OMAP_ABE_PDM_UL_PORT:
			abe->MultiFrame[5][2] = ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_UL);
			break;
		case OMAP_ABE_DMIC_PORT:
			abe->MultiFrame[2][5] = ABE_TASK_ID(C_ABE_FW_TASK_IO_DMIC);
			abe->MultiFrame[14][3] = ABE_TASK_ID(C_ABE_FW_TASK_IO_DMIC);
			break;
		case OMAP_ABE_MM_UL_PORT:
			copy_func_index1 = COPY_MM_UL_CFPID;
			before_func_index = ROUTE_MM_UL_CFPID;
			break;
		case OMAP_ABE_MM_UL2_PORT:
			abe->MultiFrame[17][3] = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_UL2);
			break;
		case OMAP_ABE_VX_DL_PORT:
			/* check for 8kHz/16kHz */
			if (abe_port[id].format.f == 8000) {
				abe->MultiFrame[TASK_ASRC_VX_DL_SLT]
					[TASK_ASRC_VX_DL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_8);
				abe->MultiFrame[TASK_VX_DL_SLT][TASK_VX_DL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_VX_DL_8_48);
				/*Voice_8k_DL_labelID */
				smem1 = IO_VX_DL_ASRC_labelID;
			} else {
				abe->MultiFrame[TASK_ASRC_VX_DL_SLT]
					[TASK_ASRC_VX_DL_IDX] =
					ABE_TASK_ID
					(C_ABE_FW_TASK_ASRC_VX_DL_16);
				abe->MultiFrame[TASK_VX_DL_SLT][TASK_VX_DL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_VX_DL_16_48);
				/* Voice_16k_DL_labelID */
				smem1 = IO_VX_DL_ASRC_labelID;
			}
			abe->MultiFrame[0][2] = ABE_TASK_ID(C_ABE_FW_TASK_IO_VX_DL);
			break;
		case OMAP_ABE_VX_UL_PORT:
			/* check for 8kHz/16kHz */
			if (abe_port[id].format.f == 8000) {
				abe->MultiFrame[TASK_ASRC_VX_UL_SLT]
					[TASK_ASRC_VX_UL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_8);
				abe->MultiFrame[TASK_VX_UL_SLT][TASK_VX_UL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_48_8);
				/* MultiFrame[TASK_ECHO_SLT][TASK_ECHO_IDX] =
				   ABE_TASK_ID(C_ABE_FW_TASK_ECHO_REF_48_8); */
				smem1 = Voice_8k_UL_labelID;
			} else {
				abe->MultiFrame[TASK_ASRC_VX_UL_SLT]
					[TASK_ASRC_VX_UL_IDX] =
					ABE_TASK_ID
					(C_ABE_FW_TASK_ASRC_VX_UL_16);
				abe->MultiFrame[TASK_VX_UL_SLT][TASK_VX_UL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_48_16);
				/* MultiFrame[TASK_ECHO_SLT][TASK_ECHO_IDX] =
				   ABE_TASK_ID(C_ABE_FW_TASK_ECHO_REF_48_16); */
				smem1 = Voice_16k_UL_labelID;
			}
			abe->MultiFrame[16][3] = ABE_TASK_ID(C_ABE_FW_TASK_IO_VX_UL);
			break;
		case OMAP_ABE_BT_VX_DL_PORT:
			/* check for 8kHz/16kHz */
			omap_abe_mem_read(abe, OMAP_ABE_DMEM,
				       OMAP_ABE_D_MAXTASKBYTESINSLOT_ADDR, &dOppMode32,
				       sizeof(u32));
			if (abe_port[id].format.f == 8000) {
				abe->MultiFrame[TASK_ASRC_BT_DL_SLT]
					[TASK_ASRC_BT_DL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_ASRC_BT_DL_8);
				if (dOppMode32 == DOPPMODE32_OPP100) {
					abe->MultiFrame[TASK_BT_DL_48_8_SLT]
						[TASK_BT_DL_48_8_IDX] =
						ABE_TASK_ID
						(C_ABE_FW_TASK_BT_DL_48_8_OPP100);
					smem1 = BT_DL_8k_opp100_labelID;
				} else {
					abe->MultiFrame[TASK_BT_DL_48_8_SLT]
						[TASK_BT_DL_48_8_IDX] =
						ABE_TASK_ID
						(C_ABE_FW_TASK_BT_DL_48_8);
					smem1 = BT_DL_8k_labelID;
				}
			} else {
				abe->MultiFrame[TASK_ASRC_BT_DL_SLT]
					[TASK_ASRC_BT_DL_IDX] =
					ABE_TASK_ID
					(C_ABE_FW_TASK_ASRC_BT_DL_16);
				if (dOppMode32 == DOPPMODE32_OPP100) {
					abe->MultiFrame[TASK_BT_DL_48_8_SLT]
						[TASK_BT_DL_48_8_IDX] =
						ABE_TASK_ID
						(C_ABE_FW_TASK_BT_DL_48_16_OPP100);
					smem1 = BT_DL_16k_opp100_labelID;
				} else {
					abe->MultiFrame[TASK_BT_DL_48_8_SLT]
						[TASK_BT_DL_48_8_IDX] =
						ABE_TASK_ID
						(C_ABE_FW_TASK_BT_DL_48_16);
					smem1 = BT_DL_16k_labelID;
				}
			}
			abe->MultiFrame[13][5] = ABE_TASK_ID(C_ABE_FW_TASK_IO_BT_VX_DL);
			break;
		case OMAP_ABE_BT_VX_UL_PORT:
			/* check for 8kHz/16kHz */
			/* set the SMEM buffer -- programming sequence */
			omap_abe_mem_read(abe, OMAP_ABE_DMEM,
				       OMAP_ABE_D_MAXTASKBYTESINSLOT_ADDR, &dOppMode32,
				       sizeof(u32));
			if (abe_port[id].format.f == 8000) {
				abe->MultiFrame[TASK_ASRC_BT_UL_SLT]
					[TASK_ASRC_BT_UL_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_ASRC_BT_UL_8);
				abe->MultiFrame[TASK_BT_UL_8_48_SLT]
					[TASK_BT_UL_8_48_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_8_48);
				if (dOppMode32 == DOPPMODE32_OPP100)
					/* ASRC input buffer, size 40 */
					smem1 = smem_bt_vx_ul_opp100;
				else
					/* at OPP 50 without ASRC */
					smem1 = BT_UL_8k_labelID;
			} else {
				abe->MultiFrame[TASK_ASRC_BT_UL_SLT]
					[TASK_ASRC_BT_UL_IDX] =
					ABE_TASK_ID
					(C_ABE_FW_TASK_ASRC_BT_UL_16);
				abe->MultiFrame[TASK_BT_UL_8_48_SLT]
					[TASK_BT_UL_8_48_IDX] =
					ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_16_48);
				if (dOppMode32 == DOPPMODE32_OPP100)
					/* ASRC input buffer, size 40 */
					smem1 = smem_bt_vx_ul_opp100;
				else
					/* at OPP 50 without ASRC */
					smem1 = BT_UL_16k_labelID;
			}
			abe->MultiFrame[15][3] = ABE_TASK_ID(C_ABE_FW_TASK_IO_BT_VX_UL);
			break;
		case OMAP_ABE_MM_DL_PORT:
			/* check for CBPr / serial_port / Ping-pong access */
			abe->MultiFrame[TASK_IO_MM_DL_SLT][TASK_IO_MM_DL_IDX] =
				ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_DL);
			smem1 = smem_mm_dl;
			break;
		case OMAP_ABE_MM_EXT_IN_PORT:
			/* set the SMEM buffer -- programming sequence */
			omap_abe_mem_read(abe, OMAP_ABE_DMEM,
				       OMAP_ABE_D_MAXTASKBYTESINSLOT_ADDR, &dOppMode32,
				       sizeof(u32));
			if (dOppMode32 == DOPPMODE32_OPP100)
				/* ASRC input buffer, size 40 */
				smem1 = smem_mm_ext_in_opp100;
			else
				/* at OPP 50 without ASRC */
				smem1 = smem_mm_ext_in_opp50;

			abe->MultiFrame[21][3] = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_EXT_IN);
			break;
		case OMAP_ABE_MM_EXT_OUT_PORT:
			abe->MultiFrame[15][0] = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_EXT_OUT);
			break;
		default:
			break;
		}

		if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
			direction = 0;
		else
			/* offset of the write pointer in the ATC descriptor */
			direction = 3;

		sio_desc.drift_ASRC = 0;
		sio_desc.drift_io = 0;
		sio_desc.io_type_idx = (u8) io_sub_id;
		sio_desc.samp_size = (u8) datasize;
		sio_desc.hw_ctrl_addr = (u16) (dmareq_addr << 2);
		sio_desc.atc_irq_data = (u8) dmareq_field;
		sio_desc.flow_counter = (u16) 0;
		sio_desc.direction_rw = (u8) direction;
		sio_desc.repeat_last_samp = (u8) io_flag;
		sio_desc.nsamp = (u8) nsamp;
		sio_desc.x_io = (u8) x_io;
		/* set ATC ON */
		sio_desc.on_off = 0x80;
		sio_desc.split_addr1 = (u16) smem1;
		sio_desc.split_addr2 = (u16) smem2;
		sio_desc.split_addr3 = (u16) smem3;
		sio_desc.before_f_index = (u8) before_func_index;
		sio_desc.after_f_index = (u8) after_func_index;
		sio_desc.smem_addr1 = (u16) smem1;
		sio_desc.atc_address1 = (u16) atc_desc_address1;
		sio_desc.atc_pointer_saved1 = (u16) atc_ptr_saved;
		sio_desc.data_size1 = (u8) datasize;
		sio_desc.copy_f_index1 = (u8) copy_func_index1;
		sio_desc.smem_addr2 = (u16) smem2;
		sio_desc.atc_address2 = (u16) atc_desc_address2;
		sio_desc.atc_pointer_saved2 = (u16) atc_ptr_saved2;
		sio_desc.data_size2 = (u8) datasize2;
		sio_desc.copy_f_index2 = (u8) copy_func_index2;
		sio_desc_address = OMAP_ABE_D_IODESCR_ADDR + (id *
				sizeof(struct ABE_SIODescriptor));

		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
				   sio_desc_address, (u32 *) &sio_desc,
				   sizeof(sio_desc));

		omap_abe_mem_write(abe, OMAP_ABE_DMEM,
				   OMAP_ABE_D_MULTIFRAME_ADDR, (u32 *) abe->MultiFrame,
				   sizeof(abe->MultiFrame));
	}

}

/**
 * omap_abe_select_main_port - Select stynchronization port for Event generator.
 * @id: audio port name
 *
 * tells the FW which is the reference stream for adjusting
 * the processing on 23/24/25 slots
 */
int omap_abe_select_main_port(u32 id)
{
	u32 selection;

	_log(ABE_ID_SELECT_MAIN_PORT, id, 0, 0);

	/* flow control */
	selection = OMAP_ABE_D_IODESCR_ADDR + id * sizeof(struct ABE_SIODescriptor) +
		flow_counter_;
	/* when the main port is a sink port from AESS point of view
	   the sign the firmware task analysis must be changed  */
	selection &= 0xFFFFL;
	if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
		selection |= 0x80000;
	omap_abe_mem_write(abe, OMAP_ABE_DMEM, OMAP_ABE_D_SLOT23_CTRL_ADDR,
			&selection, 4);
	return 0;
}
/**
 * abe_decide_main_port - Select stynchronization port for Event generator.
 * @id: audio port name
 *
 * tells the FW which is the reference stream for adjusting
 * the processing on 23/24/25 slots
 *
 * takes the first port in a list which is slave on the data interface
 */
u32 abe_valid_port_for_synchro(u32 id)
{
	if ((abe_port[id].protocol.protocol_switch ==
	     DMAREQ_PORT_PROT) ||
	    (abe_port[id].protocol.protocol_switch ==
	     PINGPONG_PORT_PROT) ||
	    (abe_port[id].status != OMAP_ABE_PORT_ACTIVITY_RUNNING))
		return 0;
	else
		return 1;
}
void abe_decide_main_port(void)
{
	u32 id, id_not_found;
	id_not_found = 1;
	for (id = 0; id < LAST_PORT_ID - 1; id++) {
		if (abe_valid_port_for_synchro(abe_port_priority[id])) {
			id_not_found = 0;
			break;
		}
	}
	/* if no port is currently activated, the default one is PDM_DL */
	if (id_not_found)
		omap_abe_select_main_port(OMAP_ABE_PDM_DL_PORT);
	else
		omap_abe_select_main_port(abe_port_priority[id]);
}
/**
 * abe_format_switch
 * @f: port format
 * @iter: port iteration
 * @mulfac: multiplication factor
 *
 * translates the sampling and data length to ITER number for the DMA
 * and the multiplier factor to apply during data move with DMEM
 *
 */
void abe_format_switch(abe_data_format_t *f, u32 *iter, u32 *mulfac)
{
	u32 n_freq;
#if FW_SCHED_LOOP_FREQ == 4000
	switch (f->f) {
		/* nb of samples processed by scheduling loop */
	case 8000:
		n_freq = 2;
		break;
	case 16000:
		n_freq = 4;
		break;
	case 24000:
		n_freq = 6;
		break;
	case 44100:
		n_freq = 12;
		break;
	case 96000:
		n_freq = 24;
		break;
	default/*case 48000 */ :
		n_freq = 12;
		break;
	}
#else
	/* erroneous cases */
	n_freq = 0;
#endif
	switch (f->samp_format) {
	case MONO_MSB:
	case MONO_RSHIFTED_16:
	case STEREO_16_16:
		*mulfac = 1;
		break;
	case STEREO_MSB:
	case STEREO_RSHIFTED_16:
		*mulfac = 2;
		break;
	case THREE_MSB:
		*mulfac = 3;
		break;
	case FOUR_MSB:
		*mulfac = 4;
		break;
	case FIVE_MSB:
		*mulfac = 5;
		break;
	case SIX_MSB:
		*mulfac = 6;
		break;
	case SEVEN_MSB:
		*mulfac = 7;
		break;
	case EIGHT_MSB:
		*mulfac = 8;
		break;
	case NINE_MSB:
		*mulfac = 9;
		break;
	default:
		*mulfac = 1;
		break;
	}
	*iter = (n_freq * (*mulfac));
}
/**
 * abe_dma_port_iteration
 * @f: port format
 *
 * translates the sampling and data length to ITER number for the DMA
 */
u32 abe_dma_port_iteration(abe_data_format_t *f)
{
	u32 iter, mulfac;
	abe_format_switch(f, &iter, &mulfac);
	return iter;
}
/**
 * abe_dma_port_iter_factor
 * @f: port format
 *
 * returns the multiplier factor to apply during data move with DMEM
 */
u32 abe_dma_port_iter_factor(abe_data_format_t *f)
{
	u32 iter, mulfac;
	abe_format_switch(f, &iter, &mulfac);
	return mulfac;
}
/**
 * omap_abe_dma_port_iter_factor
 * @f: port format
 *
 * returns the multiplier factor to apply during data move with DMEM
 */
u32 omap_abe_dma_port_iter_factor(struct omap_abe_data_format *f)
{
	u32 iter, mulfac;
	abe_format_switch((abe_data_format_t *)f, &iter, &mulfac);
	return mulfac;
}
/**
 * abe_dma_port_copy_subroutine_id
 *
 * @port_id: ABE port ID
 *
 * returns the index of the function doing the copy in I/O tasks
 */
u32 abe_dma_port_copy_subroutine_id(u32 port_id)
{
	u32 sub_id;
	if (abe_port[port_id].protocol.direction == ABE_ATC_DIRECTION_IN) {
		switch (abe_port[port_id].format.samp_format) {
		case MONO_MSB:
			sub_id = D2S_MONO_MSB_CFPID;
			break;
		case MONO_RSHIFTED_16:
			sub_id = D2S_MONO_RSHIFTED_16_CFPID;
			break;
		case STEREO_RSHIFTED_16:
			sub_id = D2S_STEREO_RSHIFTED_16_CFPID;
			break;
		case STEREO_16_16:
			sub_id = D2S_STEREO_16_16_CFPID;
			break;
		case STEREO_MSB:
			sub_id = D2S_STEREO_MSB_CFPID;
			break;
		case SIX_MSB:
			if (port_id == OMAP_ABE_DMIC_PORT) {
				sub_id = COPY_DMIC_CFPID;
				break;
			}
		default:
			sub_id = NULL_COPY_CFPID;
			break;
		}
	} else {
		switch (abe_port[port_id].format.samp_format) {
		case MONO_MSB:
			sub_id = S2D_MONO_MSB_CFPID;
			break;
		case MONO_RSHIFTED_16:
			sub_id = S2D_MONO_RSHIFTED_16_CFPID;
			break;
		case STEREO_RSHIFTED_16:
			sub_id = S2D_STEREO_RSHIFTED_16_CFPID;
			break;
		case STEREO_16_16:
			sub_id = S2D_STEREO_16_16_CFPID;
			break;
		case STEREO_MSB:
			sub_id = S2D_STEREO_MSB_CFPID;
			break;
		case SIX_MSB:
			if (port_id == OMAP_ABE_PDM_DL_PORT) {
				sub_id = COPY_MCPDM_DL_CFPID;
				break;
			}
			if (port_id == OMAP_ABE_MM_UL_PORT) {
				sub_id = COPY_MM_UL_CFPID;
				break;
			}
		case THREE_MSB:
		case FOUR_MSB:
		case FIVE_MSB:
		case SEVEN_MSB:
		case EIGHT_MSB:
		case NINE_MSB:
			sub_id = COPY_MM_UL_CFPID;
			break;
		default:
			sub_id = NULL_COPY_CFPID;
			break;
		}
	}
	return sub_id;
}

/**
 * abe_read_remaining_data
 * @id:	ABE port_ID
 * @n: size pointer to the remaining number of 32bits words
 *
 * computes the remaining amount of data in the buffer.
 */
abehal_status abe_read_remaining_data(u32 port, u32 *n)
{
	u32 sio_pp_desc_address;
	struct ABE_SPingPongDescriptor desc_pp;

	_log(ABE_ID_READ_REMAINING_DATA, port, 0, 0);

	/*
	 * read the port SIO descriptor and extract the
	 * current pointer address after reading the counter
	 */
	sio_pp_desc_address = OMAP_ABE_D_PINGPONGDESC_ADDR;
	omap_abe_mem_read(abe, OMAP_ABE_DMEM, sio_pp_desc_address,
			(u32 *) &desc_pp, sizeof(struct ABE_SPingPongDescriptor));
	*n = desc_pp.workbuff_Samples;

	return 0;
}
EXPORT_SYMBOL(abe_read_remaining_data);
