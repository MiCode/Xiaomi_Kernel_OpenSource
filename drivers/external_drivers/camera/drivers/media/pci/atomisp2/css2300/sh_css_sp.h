/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

#ifndef _SH_CSS_SP_H_
#define _SH_CSS_SP_H_

#include "input_formatter.h"

#include "sh_css_binary.h"
#include "sh_css_internal.h"
#include "sh_css_types.h"

/* Function to initialize the data and bss section descr of the binary */
void
sh_css_sp_store_init_dmem(const struct sh_css_fw_info *fw);

void
store_sp_stage_data(enum sh_css_pipe_id id, unsigned stage);

void
sh_css_stage_write_binary_info(struct sh_css_binary_info *info);

void
sh_css_sp_init_group(bool two_ppc, enum sh_css_input_format input_format,
		     bool no_isp_sync);

void
store_sp_group_data(void);

enum sh_css_err
sh_css_sp_init(void);

void
sh_css_sp_uninit(void);

/* Start binary (jpeg) copy on the SP */
void
sh_css_sp_start_binary_copy(struct sh_css_frame *out_frame,
			    unsigned two_ppc);

/* Start raw copy on the SP */
void
sh_css_sp_start_raw_copy(struct sh_css_binary *binary,
			 struct sh_css_frame *out_frame,
			 unsigned two_ppc,
			 bool input_needs_raw_binning,
			 enum sh_css_pipe_config_override pco);

unsigned int
sh_css_sp_get_binary_copy_size(void);

/* Return the value of a SW interrupt */
unsigned int
sh_css_sp_get_sw_interrupt_value(unsigned int irq);

void
sh_css_sp_init_pipeline(struct sh_css_pipeline *me,
			enum sh_css_pipe_id id,
			bool preview_mode,
			bool low_light,
			bool xnr,
			bool two_ppc,
			bool continuous,
			bool offline,
			bool input_needs_raw_binning,
			enum sh_css_pipe_config_override copy_ovrd);

void
sh_css_sp_uninit_pipeline(enum sh_css_pipe_id pipe_id);

void
sh_css_write_host2sp_command(enum host2sp_commands host2sp_command);

enum host2sp_commands
sh_css_read_host2sp_command(void);

void
sh_css_init_host2sp_frame_data(void);

#if 0
/* Temporarily prototypes till we have the proper header files and final
 * function names
 */
extern bool
host2sp_enqueue_frame_data(
			unsigned int pipe_num,
			enum sh_css_frame_id frame_id,
			void *frame_data);	/* IN */

extern bool sp2host_dequeue_irq_event(void *irq_event);
#endif

/**
 * @brief Update the offline frame information in host_sp_communication.
 *
 * @param[in] frame_num The offline frame number.
 * @param[in] frame The pointer to the offline frame.
 */
void
sh_css_update_host2sp_offline_frame(
				unsigned frame_num,
				struct sh_css_frame *frame);

/**
 * @brief Update the nr of offline frames to use in host_sp_communication.
 *
 * @param[in] num_frames The number of raw frames to use.
 */
void
sh_css_update_host2sp_cont_num_raw_frames(unsigned num_frames, bool set_avail);

void
sh_css_sp_start_isp(void);

void
sh_css_sp_set_sp_running(bool flag);

#if SP_DEBUG !=SP_DEBUG_NONE

void
sh_css_sp_get_debug_state(struct sh_css_sp_debug_state *state);

#endif

extern void sh_css_sp_set_if_configs(
	const input_formatter_cfg_t		*config_a,
	const input_formatter_cfg_t		*config_b);

void
sh_css_sp_program_input_circuit(int fmt_type,
				int ch_id,
				enum sh_css_input_mode input_mode);

void
sh_css_sp_configure_sync_gen(int width,
			     int height,
			     int hblank_cycles,
			     int vblank_cycles);

void
sh_css_sp_configure_tpg(int x_mask,
			int y_mask,
			int x_delta,
			int y_delta,
			int xy_mask);

void
sh_css_sp_configure_prbs(int seed);

void
sh_css_sp_reset_global_vars(void);

enum sh_css_err
sh_css_sp_write_frame_pointers(const struct sh_css_binary_args *args,
				unsigned pipe_num, unsigned stage_num);

/**
 * @brief Initialize the DMA software-mask in the debug mode.
 * This API should be ONLY called in the debugging mode.
 * And it should be always called before the first call of
 * "sh_css_set_dma_sw_reg(...)".
 *
 * @param[in]	dma_id		The ID of the target DMA.
 *
 * @return
 *	- true, if it is successful.
 *	- false, otherwise.
 */
extern bool sh_css_sp_init_dma_sw_reg(int dma_id);

/**
 * @brief Set the DMA software-mask in the debug mode.
 * This API should be ONLYL called in the debugging mode. Must
 * call "sh_css_set_dma_sw_reg(...)" before this
 * API is called for the first time.
 *
 * @param[in]	dma_id		The ID of the target DMA.
 * @param[in]	channel_id	The ID of the target DMA channel.
 * @param[in]	request_type	The type of the DMA request.
 *				For example:
 *				- "0" indicates the writing request.
 *				- "1" indicates the reading request.
 *
 * @param[in]	enable		If it is "true", the target DMA
 *				channel is enabled in the software.
 *				Otherwise, the target DMA channel
 *				is disabled in the software.
 *
 * @return
 *	- true, if it is successful.
 *	- false, otherwise.
 */
extern bool sh_css_sp_set_dma_sw_reg(int dma_id,
		int channel_id,
		int request_type,
		bool enable);

/**
 * @brief The Host sends the event to the SP.
 * The caller of this API will be blocked until the event
 * is sent.
 *
 * @param[in]	evt_id		The event ID.
 * @param[in]	evt_payload_0	The event payload.
 * @param[in]	evt_payload_1	The event payload.
 * @param[in]	evt_payload_2	The event payload.
 */
extern void sh_css_sp_snd_event(int evt_id,
		int evt_payload_0,
		int evt_payload_1,
		int evt_payload_2);

extern struct sh_css_sp_group sh_css_sp_group;
extern struct sh_css_sp_stage sh_css_sp_stage;
extern struct sh_css_isp_stage sh_css_isp_stage;

#endif /* _SH_CSS_SP_H_ */

