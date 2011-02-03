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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "abe_legacy.h"
#include "abe_dbg.h"
#include "abe_port.h"


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

#include "abe_gain.h"
#include "abe_aess.h"
#include "abe_seq.h"


int omap_abe_connect_debug_trace(struct omap_abe *abe,
				 struct omap_abe_dma *dma2);

int omap_abe_reset_hal(struct omap_abe *abe);
int omap_abe_load_fw(struct omap_abe *abe);
int omap_abe_wakeup(struct omap_abe *abe);
int omap_abe_irq_processing(struct omap_abe *abe);
int omap_abe_clear_irq(struct omap_abe *abe);
int omap_abe_disable_irq(struct omap_abe *abe);
int omap_abe_set_debug_trace(struct omap_abe_dbg *dbg, int debug);
int omap_abe_set_ping_pong_buffer(struct omap_abe *abe,
						u32 port, u32 n_bytes);
int omap_abe_read_next_ping_pong_buffer(struct omap_abe *abe,
						u32 port, u32 *p, u32 *n);
int omap_abe_init_ping_pong_buffer(struct omap_abe *abe,
					u32 id, u32 size_bytes, u32 n_buffers,
					u32 *p);
int omap_abe_read_offset_from_ping_buffer(struct omap_abe *abe,
						u32 id, u32 *n);
int omap_abe_set_router_configuration(struct omap_abe *abe,
					u32 id, u32 k, u32 *param);
int omap_abe_set_opp_processing(struct omap_abe *abe, u32 opp);
int omap_abe_disable_data_transfer(struct omap_abe *abe, u32 id);
int omap_abe_enable_data_transfer(struct omap_abe *abe, u32 id);
int omap_abe_connect_cbpr_dmareq_port(struct omap_abe *abe,
						u32 id, abe_data_format_t *f,
						u32 d,
						abe_dma_t *returned_dma_t);
int omap_abe_connect_irq_ping_pong_port(struct omap_abe *abe,
					     u32 id, abe_data_format_t *f,
					     u32 subroutine_id, u32 size,
					     u32 *sink, u32 dsp_mcu_flag);
int omap_abe_connect_serial_port(struct omap_abe *abe,
				 u32 id, abe_data_format_t *f,
				 u32 mcbsp_id);
int omap_abe_read_port_address(struct omap_abe *abe,
			       u32 port, abe_dma_t *dma2);
int omap_abe_check_activity(struct omap_abe *abe);

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

extern struct omap_abe *abe;

#if 0
/**
 * abe_init_mem - Allocate Kernel space memory map for ABE
 *
 * Memory map of ABE memory space for PMEM/DMEM/SMEM/DMEM
 */
void abe_init_mem(void __iomem *_io_base)
{
	omap_abe_init_mem(abe, _io_base);
}
EXPORT_SYMBOL(abe_init_mem);

struct omap_abe* abe_probe_aess(void)
{
	return omap_abe_probe_aess(abe);
}
EXPORT_SYMBOL(abe_probe_aess);

void abe_remove_aess(void)
{
	omap_abe_remove_aess(abe);
}
EXPORT_SYMBOL(abe_remove_aess);

void abe_add_subroutine(u32 *id, abe_subroutine2 f,
						u32 nparam, u32 *params)
{
	omap_abe_add_subroutine(abe, id, f, nparam, params);
}
EXPORT_SYMBOL(abe_add_subroutine);

#endif

/**
 * abe_reset_hal - reset the ABE/HAL
 * @rdev: regulator source
 * @constraints: constraints to apply
 *
 * Operations : reset the HAL by reloading the static variables and
 * default AESS registers.
 * Called after a PRCM cold-start reset of ABE
 */
u32 abe_reset_hal(void)
{
	omap_abe_reset_hal(abe);
	return 0;
}
EXPORT_SYMBOL(abe_reset_hal);

/**
 * abe_load_fw - Load ABE Firmware and initialize memories
 *
 */
u32 abe_load_fw(void)
{
	omap_abe_load_fw(abe);
	return 0;
}
EXPORT_SYMBOL(abe_load_fw);

/**
 * abe_wakeup - Wakeup ABE
 *
 * Wakeup ABE in case of retention
 */
u32 abe_wakeup(void)
{
	omap_abe_wakeup(abe);
	return 0;
}
EXPORT_SYMBOL(abe_wakeup);

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
u32 abe_irq_processing(void)
{
	omap_abe_irq_processing(abe);
	return 0;
}
EXPORT_SYMBOL(abe_irq_processing);

/**
 * abe_clear_irq - clear ABE interrupt
 *
 * This subroutine is call to clear MCU Irq
 */
u32 abe_clear_irq(void)
{
	omap_abe_clear_irq(abe);
	return 0;
}
EXPORT_SYMBOL(abe_clear_irq);

/**
 * abe_disable_irq - disable MCU/DSP ABE interrupt
 *
 * This subroutine is disabling ABE MCU/DSP Irq
 */
u32 abe_disable_irq(void)
{
	omap_abe_disable_irq(abe);

	return 0;
}
EXPORT_SYMBOL(abe_disable_irq);

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
u32 abe_write_event_generator(u32 e) // should integarte abe as parameter
{
	omap_abe_write_event_generator(abe, e);
	return 0;
}
EXPORT_SYMBOL(abe_write_event_generator);

/**
 * abe_start_event_generator - Starts event generator source
 *
 * Start the event genrator of AESS. No more event will be send to AESS engine.
 * Upper layer must wait 1/96kHz to be sure that engine reaches
 * the IDLE instruction.
 */
u32 abe_stop_event_generator(void)
{
	omap_abe_stop_event_generator(abe);
	return 0;
}
EXPORT_SYMBOL(abe_stop_event_generator);

/**
 * abe_connect_debug_trace
 * @dma2:pointer to the DMEM trace buffer
 *
 * returns the address and size of the real-time debug trace buffer,
 * the content of which will vary from one firmware release to another
 */
u32 abe_connect_debug_trace(abe_dma_t *dma2)
{
	omap_abe_connect_debug_trace(abe, (struct omap_abe_dma *)dma2);
	return 0;
}
EXPORT_SYMBOL(abe_connect_debug_trace);

/**
 * abe_set_debug_trace
 * @debug: debug ID from a list to be defined
 *
 * loads a mask which filters the debug trace to dedicated types of data
 */
u32 abe_set_debug_trace(abe_dbg_t debug)
{
	omap_abe_set_debug_trace(&abe->dbg, (int)(debug));
	return 0;
}
EXPORT_SYMBOL(abe_set_debug_trace);

/**
 * abe_set_ping_pong_buffer
 * @port: ABE port ID
 * @n_bytes: Size of Ping/Pong buffer
 *
 * Updates the next ping-pong buffer with "size" bytes copied from the
 * host processor. This API notifies the FW that the data transfer is done.
 */
u32 abe_set_ping_pong_buffer(u32 port, u32 n_bytes)
{
	omap_abe_set_ping_pong_buffer(abe, port, n_bytes);
	return 0;
}
EXPORT_SYMBOL(abe_set_ping_pong_buffer);

/**
 * abe_read_next_ping_pong_buffer
 * @port: ABE portID
 * @p: Next buffer address (pointer)
 * @n: Next buffer size (pointer)
 *
 * Tell the next base address of the next ping_pong Buffer and its size
 */
u32 abe_read_next_ping_pong_buffer(u32 port, u32 *p, u32 *n)
{
	omap_abe_read_next_ping_pong_buffer(abe, port, p, n);
	return 0;
}
EXPORT_SYMBOL(abe_read_next_ping_pong_buffer);

/**
 * abe_init_ping_pong_buffer
 * @id: ABE port ID
 * @size_bytes:size of the ping pong
 * @n_buffers:number of buffers (2 = ping/pong)
 * @p:returned address of the ping-pong list of base addresses
 *	(byte offset from DMEM start)
 *
 * Computes the base address of the ping_pong buffers
 */
u32 abe_init_ping_pong_buffer(u32 id, u32 size_bytes, u32 n_buffers,
					u32 *p)
{
	omap_abe_init_ping_pong_buffer(abe, id, size_bytes, n_buffers, p);
	return 0;
}
EXPORT_SYMBOL(abe_init_ping_pong_buffer);

/**
 * abe_read_offset_from_ping_buffer
 * @id: ABE port ID
 * @n:  returned address of the offset
 *	from the ping buffer start address (in samples)
 *
 * Computes the current firmware ping pong read pointer location,
 * expressed in samples, as the offset from the start address of ping buffer.
 */
u32 abe_read_offset_from_ping_buffer(u32 id, u32 *n)
{
	omap_abe_read_offset_from_ping_buffer(abe, id, n);
	return 0;
}
EXPORT_SYMBOL(abe_read_offset_from_ping_buffer);

/**
 * abe_write_equalizer
 * @id: name of the equalizer
 * @param : equalizer coefficients
 *
 * Load the coefficients in CMEM.
 */
u32 abe_write_equalizer(u32 id, abe_equ_t *param)
{
	omap_abe_write_equalizer(abe, id, (struct omap_abe_equ *)param);
	return 0;
}
EXPORT_SYMBOL(abe_write_equalizer);
/**
 * abe_disable_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
u32 abe_disable_gain(u32 id, u32 p)
{
	omap_abe_disable_gain(abe, id, p);
	return 0;
}
EXPORT_SYMBOL(abe_disable_gain);
/**
 * abe_enable_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
u32 abe_enable_gain(u32 id, u32 p)
{
	omap_abe_enable_gain(abe, id, p);
	return 0;
}

/**
 * abe_mute_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
u32 abe_mute_gain(u32 id, u32 p)
{
	omap_abe_mute_gain(abe, id, p);
	return 0;
}
EXPORT_SYMBOL(abe_mute_gain);

/**
 * abe_unmute_gain
 * Parameters:
 *	mixer id
 *	sub-port id
 *
 */
u32 abe_unmute_gain(u32 id, u32 p)
{
	omap_abe_unmute_gain(abe, id, p);
	return 0;
}
EXPORT_SYMBOL(abe_unmute_gain);

/**
 * abe_write_gain
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
u32 abe_write_gain(u32 id, s32 f_g, u32 ramp, u32 p)
{
	omap_abe_write_gain(abe, id, f_g, ramp, p);
	return 0;
}
EXPORT_SYMBOL(abe_write_gain);

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
u32 abe_write_mixer(u32 id, s32 f_g, u32 f_ramp, u32 p)
{
	omap_abe_write_gain(abe, id, f_g, f_ramp, p);
	return 0;
}
EXPORT_SYMBOL(abe_write_mixer);

/**
 * abe_read_gain
 * @id: name of the mixer
 * @param: list of input gains of the mixer
 * @p: list of port corresponding to the above gains
 *
 */
u32 abe_read_gain(u32 id, u32 *f_g, u32 p)
{
	omap_abe_read_gain(abe, id, f_g, p);
	return 0;
}
EXPORT_SYMBOL(abe_read_gain);

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
u32 abe_read_mixer(u32 id, u32 *f_g, u32 p)
{
	omap_abe_read_gain(abe, id, f_g, p);
	return 0;
}
EXPORT_SYMBOL(abe_read_mixer);

/**
 * abe_set_router_configuration
 * @Id: name of the router
 * @Conf: id of the configuration
 * @param: list of output index of the route
 *
 * The uplink router takes its input from DMIC (6 samples), AMIC (2 samples)
 * and PORT1/2 (2 stereo ports). Each sample will be individually stored in
 * an intermediate table of 10 elements.
 *
 * Example of router table parameter for voice uplink with phoenix microphones
 *
 * indexes 0 .. 9 = MM_UL description (digital MICs and MMEXTIN)
 *	DMIC1_L_labelID, DMIC1_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
 *	MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID, ZERO_labelID, ZERO_labelID,
 *	ZERO_labelID, ZERO_labelID,
 * indexes 10 .. 11 = MM_UL2 description (recording on DMIC3)
 *	DMIC3_L_labelID, DMIC3_R_labelID,
 * indexes 12 .. 13 = VX_UL description (VXUL based on PDMUL data)
 *	AMIC_L_labelID, AMIC_R_labelID,
 * indexes 14 .. 15 = RESERVED (NULL)
 *	ZERO_labelID, ZERO_labelID,
 */
u32 abe_set_router_configuration(u32 id, u32 k, u32 *param)
{
	omap_abe_set_router_configuration(abe, id, k, param);
	return 0;
}
EXPORT_SYMBOL(abe_set_router_configuration);

/**
 * abe_set_opp_processing - Set OPP mode for ABE Firmware
 * @opp: OOPP mode
 *
 * New processing network and OPP:
 * 0: Ultra Lowest power consumption audio player (no post-processing, no mixer)
 * 1: OPP 25% (simple multimedia features, including low-power player)
 * 2: OPP 50% (multimedia and voice calls)
 * 3: OPP100% ( multimedia complex use-cases)
 *
 * Rearranges the FW task network to the corresponding OPP list of features.
 * The corresponding AE ports are supposed to be set/reset accordingly before
 * this switch.
 *
 */
u32 abe_set_opp_processing(u32 opp)
{
	omap_abe_set_opp_processing(abe, opp);
	return 0;
}
EXPORT_SYMBOL(abe_set_opp_processing);

/**
 * abe_disable_data_transfer
 * @id: ABE port id
 *
 * disables the ATC descriptor and stop IO/port activities
 * disable the IO task (@f = 0)
 * clear ATC DMEM buffer, ATC enabled
 */
u32 abe_disable_data_transfer(u32 id)
{
	omap_abe_disable_data_transfer(abe, id);
	return 0;
}
EXPORT_SYMBOL(abe_disable_data_transfer);

/**
 * abe_enable_data_transfer
 * @ip: ABE port id
 *
 * enables the ATC descriptor
 * reset ATC pointers
 * enable the IO task (@f <> 0)
 */
u32 abe_enable_data_transfer(u32 id)
{
	omap_abe_enable_data_transfer(abe, id);
	return 0;
}
EXPORT_SYMBOL(abe_enable_data_transfer);

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
u32 abe_connect_cbpr_dmareq_port(u32 id, abe_data_format_t *f, u32 d,
					   abe_dma_t *returned_dma_t)
{
	omap_abe_connect_cbpr_dmareq_port(abe, id, f, d, returned_dma_t);
	return 0;
}
EXPORT_SYMBOL(abe_connect_cbpr_dmareq_port);

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
u32 abe_connect_irq_ping_pong_port(u32 id, abe_data_format_t *f,
					     u32 subroutine_id, u32 size,
					     u32 *sink, u32 dsp_mcu_flag)
{
	omap_abe_connect_irq_ping_pong_port(abe, id, f, subroutine_id, size,
					     sink, dsp_mcu_flag);
	return 0;
}
EXPORT_SYMBOL(abe_connect_irq_ping_pong_port);

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
u32 abe_connect_serial_port(u32 id, abe_data_format_t *f,
				      u32 mcbsp_id)
{
	omap_abe_connect_serial_port(abe, id, f, mcbsp_id);
	return 0;
}
EXPORT_SYMBOL(abe_connect_serial_port);

/**
 * abe_read_port_address
 * @dma: output pointer to the DMA iteration and data destination pointer
 *
 * This API returns the address of the DMA register used on this audio port.
 * Depending on the protocol being used, adds the base address offset L3
 * (DMA) or MPU (ARM)
 */
u32 abe_read_port_address(u32 port, abe_dma_t *dma2)
{
	omap_abe_read_port_address(abe, port, dma2);
	return 0;
}
EXPORT_SYMBOL(abe_read_port_address);

/**
 * abe_check_activity - Check if some ABE activity.
 *
 * Check if any ABE ports are running.
 * return 1: still activity on ABE
 * return 0: no more activity on ABE. Event generator can be stopped
 *
 */
u32 abe_check_activity(void)
{
	return (u32)omap_abe_check_activity(abe);
}
EXPORT_SYMBOL(abe_check_activity);
/**
 * abe_use_compensated_gain
 * @on_off:
 *
 * Selects the automatic Mixer's gain management
 * on_off = 1 allows the "abe_write_gain" to adjust the overall
 * gains of the mixer to be tuned not to create saturation
 */
abehal_status abe_use_compensated_gain(u32 on_off)
{
	omap_abe_use_compensated_gain(abe, (int)(on_off));
	return 0;
}
EXPORT_SYMBOL(abe_use_compensated_gain);
