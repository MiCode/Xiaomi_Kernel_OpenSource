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
#include "abe_aess.h"
#include "abe_gain.h"
#include "abe_mem.h"
#include "abe_port.h"
#include "abe_seq.h"

#include "abe_taskid.h"


#define ABE_TASK_ID(ID) (OMAP_ABE_D_TASKSLIST_ADDR + sizeof(ABE_STask)*(ID))
void omap_abe_build_scheduler_table(struct omap_abe *abe);
void omap_abe_reset_all_ports(struct omap_abe *abe);

const u32 abe_firmware_array[ABE_FIRMWARE_MAX_SIZE] = {
#include "abe_firmware.c"
};


/*
 * initialize the default values for call-backs to subroutines
 * - FIFO IRQ call-backs for sequenced tasks
 * - FIFO IRQ call-backs for audio player/recorders (ping-pong protocols)
 * - Remote debugger interface
 * - Error monitoring
 * - Activity Tracing
 */

/**
 * abe_init_mem - Allocate Kernel space memory map for ABE
 *
 * Memory map of ABE memory space for PMEM/DMEM/SMEM/DMEM
 */
void abe_init_mem(void __iomem **_io_base)
{
	int i;

	abe = kzalloc(sizeof(struct omap_abe), GFP_KERNEL);
	if (abe == NULL)
		printk(KERN_ERR "ABE Allocation ERROR ");

	for (i = 0; i < 5; i++)
		abe->io_base[i] = _io_base[i];

	mutex_init(&abe->mutex);

}
EXPORT_SYMBOL(abe_init_mem);

/**
 * abe_load_fw_param - Load ABE Firmware memories
 * @PMEM: Pointer of Program memory data
 * @PMEM_SIZE: Size of PMEM data
 * @CMEM: Pointer of Coeffients memory data
 * @CMEM_SIZE: Size of CMEM data
 * @SMEM: Pointer of Sample memory data
 * @SMEM_SIZE: Size of SMEM data
 * @DMEM: Pointer of Data memory data
 * @DMEM_SIZE: Size of DMEM data
 *
 */
int abe_load_fw_param(u32 *ABE_FW)
{
	u32 pmem_size, dmem_size, smem_size, cmem_size;
	u32 *pmem_ptr, *dmem_ptr, *smem_ptr, *cmem_ptr, *fw_ptr;
	_log(ABE_ID_LOAD_FW_param, 0, 0, 0);
#define ABE_FW_OFFSET 5
	fw_ptr = ABE_FW;
	abe->firmware_version_number = *fw_ptr++;
	pmem_size = *fw_ptr++;
	cmem_size = *fw_ptr++;
	dmem_size = *fw_ptr++;
	smem_size = *fw_ptr++;
	pmem_ptr = fw_ptr;
	cmem_ptr = pmem_ptr + (pmem_size >> 2);
	dmem_ptr = cmem_ptr + (cmem_size >> 2);
	smem_ptr = dmem_ptr + (dmem_size >> 2);
	/* do not load PMEM */
	if (abe->warm_boot) {
		/* Stop the event Generator */
		omap_abe_stop_event_generator(abe);

		/* Now we are sure the firmware is stalled */
		omap_abe_mem_write(abe, OMAP_ABE_CMEM, 0, cmem_ptr,
			       cmem_size);
		omap_abe_mem_write(abe, OMAP_ABE_SMEM, 0, smem_ptr,
			       smem_size);
		omap_abe_mem_write(abe, OMAP_ABE_DMEM, 0, dmem_ptr,
			       dmem_size);
		/* Restore the event Generator status */
		omap_abe_start_event_generator(abe);
	} else {
		omap_abe_mem_write(abe, OMAP_ABE_PMEM, 0, pmem_ptr,
			       pmem_size);
		omap_abe_mem_write(abe, OMAP_ABE_CMEM, 0, cmem_ptr,
			       cmem_size);
		omap_abe_mem_write(abe, OMAP_ABE_SMEM, 0, smem_ptr,
			       smem_size);
		omap_abe_mem_write(abe, OMAP_ABE_DMEM, 0, dmem_ptr,
			       dmem_size);
	}
	abe->warm_boot = 1;
	return 0;
}
EXPORT_SYMBOL(abe_load_fw_param);

/**
 * omap_abe_load_fw - Load ABE Firmware and initialize memories
 * @abe: Pointer on abe handle
 *
 */
int omap_abe_load_fw(struct omap_abe *abe, u32 *firmware)
{
	_log(ABE_ID_LOAD_FW, 0, 0, 0);
	abe_load_fw_param(firmware);
	omap_abe_reset_all_ports(abe);
	omap_abe_build_scheduler_table(abe);
	omap_abe_reset_all_sequence(abe);
	omap_abe_select_main_port(OMAP_ABE_PDM_DL_PORT);
	return 0;
}
EXPORT_SYMBOL(omap_abe_load_fw);

/**
 * abe_reload_fw - Reload ABE Firmware after OFF mode
 */
int omap_abe_reload_fw(struct omap_abe *abe, u32 *firmware)
{
	abe->warm_boot = 0;
	abe_load_fw_param(firmware);
	omap_abe_build_scheduler_table(abe);
	omap_abe_dbg_reset(&abe->dbg);
	/* IRQ circular read pointer in DMEM */
	abe->irq_dbg_read_ptr = 0;
	/* Restore Gains not managed by the drivers */
	omap_abe_write_gain(abe, GAINS_SPLIT, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_SPLIT, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL1, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL1, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL2, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL2, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	return 0;
}
EXPORT_SYMBOL(omap_abe_reload_fw);

/**
 * omap_abe_get_default_fw
 *
 * Get default ABE firmware
 */
u32 *omap_abe_get_default_fw(struct omap_abe *abe)
{
	return (u32 *)abe_firmware_array;
}

/**
 * abe_build_scheduler_table
 *
 */
void omap_abe_build_scheduler_table(struct omap_abe *abe)
{
	u16 i, n;
	u8 *ptr;
	u16 aUplinkMuxing[NBROUTE_UL];

	/* LOAD OF THE TASKS' MULTIFRAME */
	/* WARNING ON THE LOCATION OF IO_MM_DL WHICH IS PATCHED
	   IN "abe_init_io_tasks" */
	for (ptr = (u8 *) &(abe->MultiFrame[0][0]), i = 0;
	     i < sizeof(abe->MultiFrame); i++)
		*ptr++ = 0;

	abe->MultiFrame[0][2] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_VX_DL)*/;
	abe->MultiFrame[0][3] = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_8);

	abe->MultiFrame[1][3] = ABE_TASK_ID(C_ABE_FW_TASK_VX_DL_8_48_FIR);
	abe->MultiFrame[1][6] = ABE_TASK_ID(C_ABE_FW_TASK_DL2Mixer);
	abe->MultiFrame[1][7] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_VIB_DL)*/;

	abe->MultiFrame[2][0] = ABE_TASK_ID(C_ABE_FW_TASK_DL1Mixer);
	abe->MultiFrame[2][1] = ABE_TASK_ID(C_ABE_FW_TASK_SDTMixer);
	abe->MultiFrame[2][5] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_DMIC)*/;

	abe->MultiFrame[3][0] = ABE_TASK_ID(C_ABE_FW_TASK_DL1_GAIN);
	abe->MultiFrame[3][6] = ABE_TASK_ID(C_ABE_FW_TASK_DL2_GAIN);
	abe->MultiFrame[3][7] = ABE_TASK_ID(C_ABE_FW_TASK_DL2_EQ);

	abe->MultiFrame[4][0] = ABE_TASK_ID(C_ABE_FW_TASK_DL1_EQ);
	abe->MultiFrame[4][2] = ABE_TASK_ID(C_ABE_FW_TASK_VXRECMixer);
	abe->MultiFrame[4][3] = ABE_TASK_ID(C_ABE_FW_TASK_VXREC_SPLIT);
	abe->MultiFrame[4][6] = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA1);
	abe->MultiFrame[4][7] = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA2);

	abe->MultiFrame[5][0] = 0;
	abe->MultiFrame[5][1] = ABE_TASK_ID(C_ABE_FW_TASK_EARP_48_96_LP);
	abe->MultiFrame[5][2] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_UL)*/;
	abe->MultiFrame[5][7] = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA_SPLIT);

	abe->MultiFrame[6][0] = ABE_TASK_ID(C_ABE_FW_TASK_EARP_48_96_LP);
	abe->MultiFrame[6][4] = ABE_TASK_ID(C_ABE_FW_TASK_EchoMixer);
	abe->MultiFrame[6][5] = ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_SPLIT);

	abe->MultiFrame[7][0] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_DL)*/;
	abe->MultiFrame[7][3] = ABE_TASK_ID(C_ABE_FW_TASK_DBG_SYNC);
	abe->MultiFrame[7][5] = ABE_TASK_ID(C_ABE_FW_TASK_ECHO_REF_SPLIT);

	abe->MultiFrame[8][2] = ABE_TASK_ID(C_ABE_FW_TASK_DMIC1_96_48_LP);
	abe->MultiFrame[8][4] = ABE_TASK_ID(C_ABE_FW_TASK_DMIC1_SPLIT);

	abe->MultiFrame[9][2] = ABE_TASK_ID(C_ABE_FW_TASK_DMIC2_96_48_LP);
	abe->MultiFrame[9][4] = ABE_TASK_ID(C_ABE_FW_TASK_DMIC2_SPLIT);
	abe->MultiFrame[9][6] = 0;
	abe->MultiFrame[9][7] = ABE_TASK_ID(C_ABE_FW_TASK_IHF_48_96_LP);

	abe->MultiFrame[10][2] = ABE_TASK_ID(C_ABE_FW_TASK_DMIC3_96_48_LP);
	abe->MultiFrame[10][4] = ABE_TASK_ID(C_ABE_FW_TASK_DMIC3_SPLIT);
	abe->MultiFrame[10][7] = ABE_TASK_ID(C_ABE_FW_TASK_IHF_48_96_LP);

	abe->MultiFrame[11][2] = ABE_TASK_ID(C_ABE_FW_TASK_AMIC_96_48_LP);
	abe->MultiFrame[11][4] = ABE_TASK_ID(C_ABE_FW_TASK_AMIC_SPLIT);
	abe->MultiFrame[11][7] = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA_PACK);

	abe->MultiFrame[12][3] = ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_ROUTING);
	abe->MultiFrame[12][4] = ABE_TASK_ID(C_ABE_FW_TASK_ULMixer);
	abe->MultiFrame[12][5] = ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_48_8);

	abe->MultiFrame[13][2] = ABE_TASK_ID(C_ABE_FW_TASK_MM_UL2_ROUTING);
	abe->MultiFrame[13][3] = ABE_TASK_ID(C_ABE_FW_TASK_SideTone);
	abe->MultiFrame[13][5] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_BT_VX_DL)*/;

	abe->MultiFrame[14][3] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_DMIC)*/;
	abe->MultiFrame[14][4] = ABE_TASK_ID(C_ABE_FW_TASK_BT_DL_48_8);

	abe->MultiFrame[15][0] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_EXT_OUT)*/;
	abe->MultiFrame[15][3] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_BT_VX_UL)*/;
	abe->MultiFrame[15][6] = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_BT_UL_8);

	abe->MultiFrame[16][2] = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_8);
	abe->MultiFrame[16][3] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_VX_UL)*/;

	abe->MultiFrame[17][2] = ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_8_48);
	abe->MultiFrame[17][3] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_UL2)*/;

	abe->MultiFrame[18][0] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_DL)*/;
	abe->MultiFrame[18][6] = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_BT_DL_8);

	abe->MultiFrame[19][0] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_DL)*/;

	/*         MM_UL is moved to OPP 100% */
	abe->MultiFrame[19][6] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_UL)*/;

	abe->MultiFrame[20][0] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_TONES_DL)*/;
	abe->MultiFrame[20][6] = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_MM_EXT_IN);

	abe->MultiFrame[21][1] = ABE_TASK_ID(C_ABE_FW_TASK_DEBUGTRACE_VX_ASRCs);
	abe->MultiFrame[21][3] = 0/*ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_EXT_IN)*/;
	/* MUST STAY ON SLOT 22 */
	abe->MultiFrame[22][0] = ABE_TASK_ID(C_ABE_FW_TASK_DEBUG_IRQFIFO);
	abe->MultiFrame[22][1] = ABE_TASK_ID(C_ABE_FW_TASK_INIT_FW_MEMORY);
	abe->MultiFrame[22][2] = 0;
	/* MM_EXT_IN_SPLIT task must be after IO_MM_EXT_IN and before
	   ASRC_MM_EXT_IN in order to manage OPP50 <-> transitions */
	abe->MultiFrame[22][4] = ABE_TASK_ID(C_ABE_FW_TASK_MM_EXT_IN_SPLIT);

	abe->MultiFrame[23][0] = ABE_TASK_ID(C_ABE_FW_TASK_GAIN_UPDATE);

	omap_abe_mem_write(abe, OMAP_ABE_DMEM, OMAP_ABE_D_MULTIFRAME_ADDR,
		       (u32 *) abe->MultiFrame, sizeof(abe->MultiFrame));
	/* reset the uplink router */
	n = (OMAP_ABE_D_AUPLINKROUTING_SIZE) >> 1;
	for (i = 0; i < n; i++)
		aUplinkMuxing[i] = ZERO_labelID;

	omap_abe_mem_write(abe, OMAP_ABE_DMEM, OMAP_ABE_D_AUPLINKROUTING_ADDR,
		       (u32 *) aUplinkMuxing, sizeof(aUplinkMuxing));
}

/**
 * omap_abe_reset_port
 * @id: ABE port ID
 *
 * stop the port activity and reload default parameters on the associated
 * processing features.
 * Clears the internal AE buffers.
 */
int omap_abe_reset_port(u32 id)
{
	_log(ABE_ID_RESET_PORT, id, 0, 0);
	abe_port[id] = ((abe_port_t *) abe_port_init)[id];
	return 0;
}

/**
 * abe_reset_all_ports
 *
 * load default configuration for all features
 */
void omap_abe_reset_all_ports(struct omap_abe *abe)
{
	u16 i;
	for (i = 0; i < LAST_PORT_ID; i++)
		omap_abe_reset_port(i);
	/* mixers' configuration */
	omap_abe_write_mixer(abe, MIXDL1, MUTE_GAIN,
			     RAMP_5MS, MIX_DL1_INPUT_MM_DL);
	omap_abe_write_mixer(abe, MIXDL1, MUTE_GAIN,
			     RAMP_5MS, MIX_DL1_INPUT_MM_UL2);
	omap_abe_write_mixer(abe, MIXDL1, MUTE_GAIN,
			     RAMP_5MS, MIX_DL1_INPUT_VX_DL);
	omap_abe_write_mixer(abe, MIXDL1, MUTE_GAIN,
			     RAMP_5MS, MIX_DL1_INPUT_TONES);
	omap_abe_write_mixer(abe, MIXDL2, MUTE_GAIN,
			     RAMP_5MS, MIX_DL2_INPUT_TONES);
	omap_abe_write_mixer(abe, MIXDL2, MUTE_GAIN,
			     RAMP_5MS, MIX_DL2_INPUT_VX_DL);
	omap_abe_write_mixer(abe, MIXDL2, MUTE_GAIN,
			     RAMP_5MS, MIX_DL2_INPUT_MM_DL);
	omap_abe_write_mixer(abe, MIXDL2, MUTE_GAIN,
			     RAMP_5MS, MIX_DL2_INPUT_MM_UL2);
	omap_abe_write_mixer(abe, MIXSDT, MUTE_GAIN,
			     RAMP_5MS, MIX_SDT_INPUT_UP_MIXER);
	omap_abe_write_mixer(abe, MIXSDT, GAIN_0dB,
			     RAMP_5MS, MIX_SDT_INPUT_DL1_MIXER);
	omap_abe_write_mixer(abe, MIXECHO, MUTE_GAIN,
			     RAMP_5MS, MIX_ECHO_DL1);
	omap_abe_write_mixer(abe, MIXECHO, MUTE_GAIN,
			     RAMP_5MS, MIX_ECHO_DL2);
	omap_abe_write_mixer(abe, MIXAUDUL, MUTE_GAIN,
			     RAMP_5MS, MIX_AUDUL_INPUT_MM_DL);
	omap_abe_write_mixer(abe, MIXAUDUL, MUTE_GAIN,
			     RAMP_5MS, MIX_AUDUL_INPUT_TONES);
	omap_abe_write_mixer(abe, MIXAUDUL, GAIN_0dB,
			     RAMP_5MS, MIX_AUDUL_INPUT_UPLINK);
	omap_abe_write_mixer(abe, MIXAUDUL, MUTE_GAIN,
			     RAMP_5MS, MIX_AUDUL_INPUT_VX_DL);
	omap_abe_write_mixer(abe, MIXVXREC, MUTE_GAIN,
			     RAMP_5MS, MIX_VXREC_INPUT_TONES);
	omap_abe_write_mixer(abe, MIXVXREC, MUTE_GAIN,
			     RAMP_5MS, MIX_VXREC_INPUT_VX_DL);
	omap_abe_write_mixer(abe, MIXVXREC, MUTE_GAIN,
			     RAMP_5MS, MIX_VXREC_INPUT_MM_DL);
	omap_abe_write_mixer(abe, MIXVXREC, MUTE_GAIN,
			     RAMP_5MS, MIX_VXREC_INPUT_VX_UL);
	omap_abe_write_gain(abe, GAINS_DMIC1, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DMIC1, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DMIC2, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DMIC2, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DMIC3, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DMIC3, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_AMIC, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_AMIC, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_SPLIT, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_SPLIT, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL1, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL1, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL2, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_DL2, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
	omap_abe_write_gain(abe, GAINS_BTUL, GAIN_0dB,
			    RAMP_5MS, GAIN_LEFT_OFFSET);
	omap_abe_write_gain(abe, GAINS_BTUL, GAIN_0dB,
			    RAMP_5MS, GAIN_RIGHT_OFFSET);
}
