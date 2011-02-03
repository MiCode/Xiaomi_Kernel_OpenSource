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

#ifndef _ABE_API_H_
#define _ABE_API_H_

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe_dm_addr.h"
#include "abe_dbg.h"

#define ABE_TASK_ID(ID) (OMAP_ABE_D_TASKSLIST_ADDR + sizeof(ABE_STask)*(ID))

#define TASK_ASRC_VX_DL_SLT 1
#define TASK_ASRC_VX_DL_IDX 2
#define TASK_VX_DL_SLT 1
#define TASK_VX_DL_IDX 3
#define TASK_VX_UL_SLT 12
#define TASK_VX_UL_IDX 5
#define TASK_BT_DL_48_8_SLT 14
#define TASK_BT_DL_48_8_IDX 4
#define TASK_ASRC_BT_UL_SLT 15
#define TASK_ASRC_BT_UL_IDX 6
#define TASK_ASRC_VX_UL_SLT 16
#define TASK_ASRC_VX_UL_IDX 2
#define TASK_BT_UL_8_48_SLT 17
#define TASK_BT_UL_8_48_IDX 2
#define TASK_IO_MM_DL_SLT 18
#define TASK_IO_MM_DL_IDX 0
#define TASK_ASRC_BT_DL_SLT 18
#define TASK_ASRC_BT_DL_IDX 6

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
	u32 dbg_param;

	struct omap_abe_dbg dbg;
};

/**
 * abe_reset_hal - reset the ABE/HAL
 * @rdev: regulator source
 * @constraints: constraints to apply
 *
 * Operations : reset the HAL by reloading the static variables and
 * default AESS registers.
 * Called after a PRCM cold-start reset of ABE
 */
abehal_status abe_reset_hal(void);
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
abehal_status abe_load_fw_param(u32 *FW);
/**
 * abe_reload_fw - Reload ABE Firmware after OFF mode
 */
abehal_status abe_reload_fw(void);
/**
 * abe_load_fw - Load ABE Firmware and initialize memories
 *
 */
abehal_status abe_load_fw(void);
/**
 * abe_irq_processing - Process ABE interrupt
 *
 * This subroutine is call upon reception of "MA_IRQ_99 ABE_MPU_IRQ" Audio
 * back-end interrupt. This subroutine will check the ATC Hrdware, the
 * IRQ_FIFO from the AE and act accordingly. Some IRQ source are originated
 * for the delivery of "end of time sequenced tasks" notifications, some are
 * originated from the Ping-Pong protocols, some are generated from
 * the embedded debugger when the firmware stops on programmable break-points,
 * etc ...
 */
abehal_status abe_irq_processing(void);
/**
 * abe_irq_clear - clear ABE interrupt
 *
 * This subroutine is call to clear MCU Irq
 */
abehal_status abe_clear_irq(void);
/**
 * abe_disable_irq - disable MCU/DSP ABE interrupt
 *
 * This subroutine is disabling ABE MCU/DSP Irq
 */
abehal_status abe_disable_irq(void);
/*
 * abe_check_activity - check all ports are closed
 */
u32 abe_check_activity(void);
/**
 * abe_wakeup - Wakeup ABE
 *
 * Wakeup ABE in case of retention
 */
abehal_status abe_wakeup(void);
/**
 * abe_start_event_generator - Stops event generator source
 *
 * Start the event genrator of AESS. No more event will be send to AESS engine.
 * Upper layer must wait 1/96kHz to be sure that engine reaches
 * the IDLE instruction.
 */
abehal_status abe_start_event_generator(void);
/**
 * abe_stop_event_generator - Stops event generator source
 *
 * Stop the event genrator of AESS. No more event will be send to AESS engine.
 * Upper layer must wait 1/96kHz to be sure that engine reaches
 * the IDLE instruction.
 */
abehal_status abe_stop_event_generator(void);

/**
 * abe_write_event_generator - Selects event generator source
 * @e: Event Generation Counter, McPDM, DMIC or default.
 *
 * Loads the AESS event generator hardware source.
 * Loads the firmware parameters accordingly.
 * Indicates to the FW which data stream is the most important to preserve
 * in case all the streams are asynchronous.
 * If the parameter is "default", then HAL decides which Event source
 * is the best appropriate based on the opened ports.
 *
 * When neither the DMIC and the McPDM are activated, the AE will have
 * its EVENT generator programmed with the EVENT_COUNTER.
 * The event counter will be tuned in order to deliver a pulse frequency higher
 * than 96 kHz.
 * The DPLL output at 100% OPP is MCLK = (32768kHz x6000) = 196.608kHz
 * The ratio is (MCLK/96000)+(1<<1) = 2050
 * (1<<1) in order to have the same speed at 50% and 100% OPP
 * (only 15 MSB bits are used at OPP50%)
 */
abehal_status abe_write_event_generator(u32 e);
/**
 * abe_set_opp_processing - Set OPP mode for ABE Firmware
 * @opp: OOPP mode
 *
 * New processing network and OPP:
 * 0: Ultra Lowest power consumption audio player (no post-processing, no mixer)
 * 1: OPP 25% (simple multimedia features, including low-power player)
 * 2: OPP 50% (multimedia and voice calls)
 * 3: OPP100% (EANC, multimedia complex use-cases)
 *
 * Rearranges the FW task network to the corresponding OPP list of features.
 * The corresponding AE ports are supposed to be set/reset accordingly before
 * this switch.
 *
 */
abehal_status abe_set_opp_processing(u32 opp);
/**
 * abe_set_ping_pong_buffer
 * @port: ABE port ID
 * @n_bytes: Size of Ping/Pong buffer
 *
 * Updates the next ping-pong buffer with "size" bytes copied from the
 * host processor. This API notifies the FW that the data transfer is done.
 */
abehal_status abe_set_ping_pong_buffer(u32 port, u32 n_bytes);
/**
 * abe_read_next_ping_pong_buffer
 * @port: ABE portID
 * @p: Next buffer address (pointer)
 * @n: Next buffer size (pointer)
 *
 * Tell the next base address of the next ping_pong Buffer and its size
 */
abehal_status abe_read_next_ping_pong_buffer(u32 port, u32 *p, u32 *n);
/**
 * abe_init_ping_pong_buffer
 * @id: ABE port ID
 * @size_bytes:size of the ping pong
 * @n_buffers:number of buffers (2 = ping/pong)
 * @p:returned address of the ping-pong list of base address (byte offset
	from DMEM start)
 *
 * Computes the base address of the ping_pong buffers
 */
abehal_status abe_init_ping_pong_buffer(u32 id, u32 size_bytes, u32 n_buffers,
					u32 *p);
/**
 * abe_read_offset_from_ping_buffer
 * @id: ABE port ID
 * @n:  returned address of the offset
 *	from the ping buffer start address (in samples)
 *
 * Computes the current firmware ping pong read pointer location,
 * expressed in samples, as the offset from the start address of ping buffer.
 */
abehal_status abe_read_offset_from_ping_buffer(u32 id, u32 *n);
/**
 * abe_plug_subroutine
 * @id: returned sequence index after plugging a new subroutine
 * @f: subroutine address to be inserted
 * @n: number of parameters of this subroutine
 * @params: pointer on parameters
 *
 * register a list of subroutines for call-back purpose
 */
abehal_status abe_plug_subroutine(u32 *id, abe_subroutine2 f, u32 n,
				  u32 *params);
/**
 * abe_set_sequence_time_accuracy
 * @fast: fast counter
 * @slow: slow counter
 *
 */
abehal_status abe_set_sequence_time_accuracy(u32 fast, u32 slow);
/**
 * abe_reset_port
 * @id: ABE port ID
 *
 * stop the port activity and reload default parameters on the associated
 * processing features.
 * Clears the internal AE buffers.
 */
abehal_status abe_reset_port(u32 id);
/**
 * abe_read_remaining_data
 * @id:	ABE port_ID
 * @n: size pointer to the remaining number of 32bits words
 *
 * computes the remaining amount of data in the buffer.
 */
abehal_status abe_read_remaining_data(u32 port, u32 *n);
/**
 * abe_disable_data_transfer
 * @id: ABE port id
 *
 * disables the ATC descriptor and stop IO/port activities
 * disable the IO task (@f = 0)
 * clear ATC DMEM buffer, ATC enabled
 */
abehal_status abe_disable_data_transfer(u32 id);
/**
 * abe_enable_data_transfer
 * @ip: ABE port id
 *
 * enables the ATC descriptor
 * reset ATC pointers
 * enable the IO task (@f <> 0)
 */
abehal_status abe_enable_data_transfer(u32 id);
/**
 * abe_set_dmic_filter
 * @d: DMIC decimation ratio : 16/25/32/40
 *
 * Loads in CMEM a specific list of coefficients depending on the DMIC sampling
 * frequency (2.4MHz or 3.84MHz). This table compensates the DMIC decimator
 * roll-off at 20kHz.
 * The default table is loaded with the DMIC 2.4MHz recommended configuration.
 */
abehal_status abe_set_dmic_filter(u32 d);
/**
 * abe_connect_cbpr_dmareq_port
 * @id: port name
 * @f: desired data format
 * @d: desired dma_request line (0..7)
 * @a: returned pointer to the base address of the CBPr register and number of
 *	samples to exchange during a DMA_request.
 *
 * enables the data echange between a DMA and the ABE through the
 *	CBPr registers of AESS.
 */
abehal_status abe_connect_cbpr_dmareq_port(u32 id, abe_data_format_t *f, u32 d,
					   abe_dma_t *returned_dma_t);
/**
 * abe_connect_irq_ping_pong_port
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
abehal_status abe_connect_irq_ping_pong_port(u32 id, abe_data_format_t *f,
					     u32 subroutine_id, u32 size,
					     u32 *sink, u32 dsp_mcu_flag);
/**
 * abe_connect_serial_port()
 * @id: port name
 * @f: data format
 * @i: peripheral ID (McBSP #1, #2, #3)
 *
 * Operations : enables the data echanges between a McBSP and an ATC buffer in
 * DMEM. This API is used connect 48kHz McBSP streams to MM_DL and 8/16kHz
 * voice streams to VX_UL, VX_DL, BT_VX_UL, BT_VX_DL. It abstracts the
 * abe_write_port API.
 */
abehal_status abe_connect_serial_port(u32 id, abe_data_format_t *f,
				      u32 mcbsp_id);
/**
 * abe_read_port_address
 * @dma: output pointer to the DMA iteration and data destination pointer
 *
 * This API returns the address of the DMA register used on this audio port.
 * Depending on the protocol being used, adds the base address offset L3
 * (DMA) or MPU (ARM)
 */
abehal_status abe_read_port_address(u32 port, abe_dma_t *dma2);
/**
 * abe_write_equalizer
 * @id: name of the equalizer
 * @param : equalizer coefficients
 *
 * Load the coefficients in CMEM.
 */
abehal_status abe_write_equalizer(u32 id, abe_equ_t *param);
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
abehal_status abe_write_asrc(u32 port, s32 dppm);
/**
 * abe_write_aps
 * @id: name of the aps filter
 * @param: table of filter coefficients
 *
 * Load the filters and thresholds coefficients in FW memory. This AP
 * can be called when the corresponding APS is not activated. After
 * reloading the firmware the default coefficients corresponds to "no APS
 * activated".
 * Loading all the coefficients value with zero disables the feature.
 */
abehal_status abe_write_aps(u32 id, struct abe_aps_t *param);
/**
 * abe_write_mixer
 * @id: name of the mixer
 * @param: list of input gains of the mixer
 * @p: list of port corresponding to the above gains
 *
 * Load the gain coefficients in FW memory. This API can be called when
 * the corresponding MIXER is not activated. After reloading the firmware
 * the default coefficients corresponds to "all input and output mixer's gain
 * in mute state". A mixer is disabled with a network reconfiguration
 * corresponding to an OPP value.
 */
abehal_status abe_write_gain(u32 id, s32 f_g, u32 ramp, u32 p);
abehal_status abe_use_compensated_gain(u32 on_off);
abehal_status abe_enable_gain(u32 id, u32 p);
abehal_status abe_disable_gain(u32 id, u32 p);
abehal_status abe_mute_gain(u32 id, u32 p);
abehal_status abe_unmute_gain(u32 id, u32 p);
/**
 * abe_write_mixer
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
abehal_status abe_write_mixer(u32 id, s32 f_g, u32 f_ramp, u32 p);
/**
 * abe_read_gain
 * @id: name of the mixer
 * @param: list of input gains of the mixer
 * @p: list of port corresponding to the above gains
 *
 */
abehal_status abe_read_gain(u32 id, u32 *f_g, u32 p);
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
abehal_status abe_read_mixer(u32 id, u32 *f_g, u32 p);
/**
 * abe_set_router_configuration
 * @Id: name of the router
 * @Conf: id of the configuration
 * @param: list of output index of the route
 *
 * The uplink router takes its input from DMIC (6 samples), AMIC (2 samples)
 * and PORT1/2 (2 stereo ports). Each sample will be individually stored in
 * an intermediate table of 10 elements. The intermediate table is used to
 * route the samples to three directions : REC1 mixer, 2 EANC DMIC source of
 * filtering and MM recording audio path.
 */
abehal_status abe_set_router_configuration(u32 id, u32 k, u32 *param);
/**
 * ABE_READ_DEBUG_TRACE
 *
 * Parameters :
 * @data: data destination pointer
 * @n	: max number of read data
 *
 * Operations :
 * Reads the AE circular data pointer that holds pairs of debug data +
 * timestamps, and stores the pairs, via linear addressing, to the parameter
 * pointer.
 * Stops the copy when the max parameter is reached or when the FIFO is empty.
 *
 * Return value :
 * None.
 */
abehal_status abe_read_debug_trace(u32 *data, u32 *n);
/**
 * abe_connect_debug_trace
 * @dma2:pointer to the DMEM trace buffer
 *
 * returns the address and size of the real-time debug trace buffer,
 * the content of which will vary from one firmware release to an other
 */
abehal_status abe_connect_debug_trace(abe_dma_t *dma2);
/**
 * abe_set_debug_trace
 * @debug: debug ID from a list to be defined
 *
 * load a mask which filters the debug trace to dedicated types of data
 */
abehal_status abe_set_debug_trace(abe_dbg_t debug);
/**
 * abe_init_mem - Allocate Kernel space memory map for ABE
 *
 * Memory map of ABE memory space for PMEM/DMEM/SMEM/DMEM
 */
void abe_init_mem(void __iomem **_io_base);

#endif/* _ABE_API_H_ */
