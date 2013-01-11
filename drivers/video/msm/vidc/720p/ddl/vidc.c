/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/unistd.h>
#include <media/msm/vidc_type.h>
#include "vidc.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define VIDC_720P_VERSION_STRING "VIDC_V1.0"
u8 *vidc_base_addr;

#ifdef VIDC_REGISTER_LOG_INTO_BUFFER
char vidclog[VIDC_REGLOG_BUFSIZE];
unsigned int vidclog_index;
#endif

void vidc_720p_set_device_virtual_base(u8 *core_virtual_base_addr)
{
	vidc_base_addr = core_virtual_base_addr;
}

void vidc_720p_init(char **ppsz_version, u32 i_firmware_size,
		     u32 *pi_firmware_address,
		     enum vidc_720p_endian dma_endian,
		     u32 interrupt_off,
		     enum vidc_720p_interrupt_level_selection
		     interrupt_sel, u32 interrupt_mask)
{
	if (ppsz_version)
		*ppsz_version = VIDC_720P_VERSION_STRING;

	if (interrupt_sel == VIDC_720P_INTERRUPT_LEVEL_SEL)
		VIDC_IO_OUT(REG_491082, 0);
	else
		VIDC_IO_OUT(REG_491082, 1);

	if (interrupt_off)
		VIDC_IO_OUT(REG_609676, 1);
	else
		VIDC_IO_OUT(REG_609676, 0);

	VIDC_IO_OUT(REG_614776, 1);

	VIDC_IO_OUT(REG_418173, 0);

	VIDC_IO_OUT(REG_418173, interrupt_mask);

	VIDC_IO_OUT(REG_736316, dma_endian);

	VIDC_IO_OUT(REG_215724, 0);

	VIDC_IO_OUT(REG_361582, 1);

	VIDC_IO_OUT(REG_591577, i_firmware_size);

	VIDC_IO_OUT(REG_203921, pi_firmware_address);

	VIDC_IO_OUT(REG_531515_ADDR, 0);

	VIDC_IO_OUT(REG_614413, 1);
}

u32 vidc_720p_do_sw_reset(void)
{

	u32 fw_start = 0;
	VIDC_BUSY_WAIT(5);
	VIDC_IO_OUT(REG_224135, 0);
	VIDC_BUSY_WAIT(5);
	VIDC_IO_OUT(REG_193553, 0);
	VIDC_BUSY_WAIT(5);
	VIDC_IO_OUT(REG_141269, 1);
	VIDC_BUSY_WAIT(15);
	VIDC_IO_OUT(REG_141269, 0);
	VIDC_BUSY_WAIT(5);
	VIDC_IO_IN(REG_193553, &fw_start);

	if (!fw_start) {
		DBG("\n VIDC-SW-RESET-FAILS!");
		return false;
	}
	return true;
}

u32 vidc_720p_reset_is_success()
{
	u32 stagecounter = 0;
	VIDC_IO_IN(REG_352831, &stagecounter);
	stagecounter &= 0xff;
	if (stagecounter != 0xe5) {
		DBG("\n VIDC-CPU_RESET-FAILS!");
		VIDC_IO_OUT(REG_224135, 0);
		msleep(10);
		return false;
	}
	return true;
}

void vidc_720p_start_cpu(enum vidc_720p_endian dma_endian,
						  u32 *icontext_bufferstart,
						  u32 *debug_core_dump_addr,
						  u32  debug_buffer_size)
{
	u32 dbg_info_input0_reg = 0x1;
	VIDC_IO_OUT(REG_361582, 0);
	VIDC_IO_OUT(REG_958768, icontext_bufferstart);
	VIDC_IO_OUT(REG_736316, dma_endian);
	if (debug_buffer_size) {
		dbg_info_input0_reg = (debug_buffer_size << 0x10)
			| (0x2 << 1) | 0x1;
		VIDC_IO_OUT(REG_166247, debug_core_dump_addr);
	}
	VIDC_IO_OUT(REG_699747, dbg_info_input0_reg);
	VIDC_IO_OUT(REG_224135, 1);
}

u32 vidc_720p_cpu_start()
{
	u32 fw_status = 0x0;
	VIDC_IO_IN(REG_381535, &fw_status);
	if (fw_status != 0x02)
		return false;
	return true;
}


void vidc_720p_stop_fw(void)
{
   VIDC_IO_OUT(REG_193553, 0);
   VIDC_IO_OUT(REG_224135, 0);
}

void vidc_720p_get_interrupt_status(u32 *interrupt_status,
	u32 *cmd_err_status, u32 *disp_pic_err_status, u32 *op_failed)
{
	u32 err_status;
	VIDC_IO_IN(REG_512143, interrupt_status);
	VIDC_IO_IN(REG_300310, &err_status);
	*cmd_err_status = err_status & 0xffff;
	*disp_pic_err_status = (err_status & 0xffff0000) >> 16;
	VIDC_IO_INF(REG_724381, OPERATION_FAILED, \
				 op_failed);
}

void vidc_720p_interrupt_done_clear(void)
{
	VIDC_IO_OUT(REG_614776, 1);
	VIDC_IO_OUT(REG_97293, 4);
}

void vidc_720p_submit_command(u32 ch_id, u32 cmd_id)
{
	u32 fw_status;
	VIDC_IO_OUT(REG_97293, ch_id);
	VIDC_IO_OUT(REG_62325, cmd_id);
	VIDC_DEBUG_REGISTER_LOG;
	VIDC_IO_IN(REG_381535, &fw_status);
	VIDC_IO_OUT(REG_926519, fw_status);
}

u32 vidc_720p_engine_reset(u32 ch_id,
	enum vidc_720p_endian dma_endian,
	enum vidc_720p_interrupt_level_selection interrupt_sel,
	u32 interrupt_mask
)
{
	u32 op_done = 0;
	u32 counter = 0;

	VIDC_LOGERR_STRING("ENG-RESET!!");
	/* issue the engine reset command */
	vidc_720p_submit_command(ch_id, VIDC_720P_CMD_MFC_ENGINE_RESET);

	do {
		VIDC_BUSY_WAIT(20);
		VIDC_IO_IN(REG_982553, &op_done);
		counter++;
	} while (!op_done && counter < 10);

	if (!op_done) {
		/* Reset fails */
		return  false ;
	}

	/* write invalid channel id */
	VIDC_IO_OUT(REG_97293, 4);

	/* Set INT_PULSE_SEL */
	if (interrupt_sel == VIDC_720P_INTERRUPT_LEVEL_SEL)
		VIDC_IO_OUT(REG_491082, 0);
	else
		VIDC_IO_OUT(REG_491082, 1);

	if (!interrupt_mask) {
		/* Disable interrupt */
		VIDC_IO_OUT(REG_609676, 1);
	} else {
	  /* Enable interrupt */
		VIDC_IO_OUT(REG_609676, 0);
	}

	/* Clear any pending interrupt */
	VIDC_IO_OUT(REG_614776, 1);

	/* Set INT_ENABLE_REG */
	VIDC_IO_OUT(REG_418173, interrupt_mask);

	/*Sets the DMA endianness */
	VIDC_IO_OUT(REG_736316, dma_endian);

	/*Restore ARM endianness */
	VIDC_IO_OUT(REG_215724, 0);

	/* retun engine reset success */
	return true ;
}

void vidc_720p_set_channel(u32 i_ch_id,
			    enum vidc_720p_enc_dec_selection
			    enc_dec_sel, enum vidc_720p_codec codec,
			    u32 *pi_fw, u32 i_firmware_size)
{
	u32 std_sel = 0;
	VIDC_IO_OUT(REG_661565, 0);

	if (enc_dec_sel)
		std_sel = VIDC_REG_713080_ENC_ON_BMSK;

	std_sel |= (u32) codec;

	VIDC_IO_OUT(REG_713080, std_sel);

	switch (codec) {
	default:
	case VIDC_720P_DIVX:
	case VIDC_720P_XVID:
	case VIDC_720P_MPEG4:
		{
			if (enc_dec_sel == VIDC_720P_ENCODER)
				VIDC_IO_OUT(REG_765787, pi_fw);
			else
				VIDC_IO_OUT(REG_225040, pi_fw);
			break;
		}
	case VIDC_720P_H264:
		{
			if (enc_dec_sel == VIDC_720P_ENCODER)
				VIDC_IO_OUT(REG_942456, pi_fw);
			else
				VIDC_IO_OUT(REG_942170_ADDR_3, pi_fw);
			break;
		}
	case VIDC_720P_H263:
		{
			if (enc_dec_sel == VIDC_720P_ENCODER)
				VIDC_IO_OUT(REG_765787, pi_fw);
			else
				VIDC_IO_OUT(REG_942170_ADDR_6, pi_fw);
			break;
		}
	case VIDC_720P_VC1:
		{
			VIDC_IO_OUT(REG_880188, pi_fw);
			break;
		}
	case VIDC_720P_MPEG2:
		{
			VIDC_IO_OUT(REG_40293, pi_fw);
			break;
		}
	}
	VIDC_IO_OUT(REG_591577, i_firmware_size);

	vidc_720p_submit_command(i_ch_id, VIDC_720P_CMD_CHSET);
}

void vidc_720p_encode_set_profile(u32 i_profile, u32 i_level)
{
	u32 profile_level = i_profile|(i_level << 0x8);
	VIDC_IO_OUT(REG_839021, profile_level);
}

void vidc_720p_set_frame_size(u32 i_size_x, u32 i_size_y)
{
	VIDC_IO_OUT(REG_999267, i_size_x);

	VIDC_IO_OUT(REG_345712, i_size_y);
}

void vidc_720p_encode_set_fps(u32 i_rc_frame_rate)
{
	VIDC_IO_OUT(REG_625444, i_rc_frame_rate);
}

void vidc_720p_encode_set_short_header(u32 i_short_header)
{
	VIDC_IO_OUT(REG_314290, i_short_header);
}

void vidc_720p_encode_set_vop_time(u32 vop_time_resolution,
				    u32 vop_time_increment)
{
	u32 enable_vop, vop_timing_reg;
	if (!vop_time_resolution)
		VIDC_IO_OUT(REG_64895, 0x0);
	else {
		enable_vop = 0x1;
		vop_timing_reg = (enable_vop << 0x1f) |
		(vop_time_resolution << 0x10) | vop_time_increment;
		VIDC_IO_OUT(REG_64895, vop_timing_reg);
	}
}

void vidc_720p_encode_set_hec_period(u32 hec_period)
{
	VIDC_IO_OUT(REG_407718, hec_period);
}

void vidc_720p_encode_set_qp_params(u32 i_max_qp, u32 i_min_qp)
{
	u32 qp = i_min_qp | (i_max_qp << 0x8);
	VIDC_IO_OUT(REG_734318, qp);
}

void vidc_720p_encode_set_rc_config(u32 enable_frame_level_rc,
				     u32 enable_mb_level_rc_flag,
				     u32 i_frame_qp, u32 pframe_qp)
{
   u32 rc_config = i_frame_qp;

	if (enable_frame_level_rc)
		rc_config |= (0x1 << 0x9);

	if (enable_mb_level_rc_flag)
		rc_config |= (0x1 << 0x8);

	VIDC_IO_OUT(REG_58211, rc_config);
	VIDC_IO_OUT(REG_548359, pframe_qp);
}

void vidc_720p_encode_set_bit_rate(u32 i_target_bitrate)
{
	VIDC_IO_OUT(REG_174150, i_target_bitrate);
}

void vidc_720p_encoder_set_param_change(u32 enc_param_change)
{
	VIDC_IO_OUT(REG_804959, enc_param_change);
}

void vidc_720p_encode_set_control_param(u32 param_val)
{
	VIDC_IO_OUT(REG_128234, param_val);
}

void vidc_720p_encode_set_frame_level_rc_params(u32 i_reaction_coeff)
{
	VIDC_IO_OUT(REG_677784, i_reaction_coeff);
}

void vidc_720p_encode_set_mb_level_rc_params(u32 dark_region_as_flag,
					      u32 smooth_region_as_flag,
					      u32 static_region_as_flag,
					      u32 activity_region_flag)
{
	u32 mb_level_rc = 0x0;
	if (activity_region_flag)
		mb_level_rc |= 0x1;
	if (static_region_as_flag)
		mb_level_rc |= (0x1 << 0x1);
	if (smooth_region_as_flag)
		mb_level_rc |= (0x1 << 0x2);
	if (dark_region_as_flag)
		mb_level_rc |= (0x1 << 0x3);
	/* Write MB level rate control */
	VIDC_IO_OUT(REG_995041, mb_level_rc);
}

void vidc_720p_encode_set_entropy_control(enum vidc_720p_entropy_sel
					   entropy_sel,
					   enum vidc_720p_cabac_model
					   cabac_model_number)
{
	u32 num;
	u32 entropy_params = (u32)entropy_sel;
	/* Set Model Number */
	if (entropy_sel == VIDC_720P_ENTROPY_SEL_CABAC) {
		num = (u32)cabac_model_number;
		entropy_params |= (num << 0x2);
	}
	/* Set Entropy parameters */
	VIDC_IO_OUT(REG_504878, entropy_params);
}

void vidc_720p_encode_set_db_filter_control(enum vidc_720p_DBConfig
					     db_config,
					     u32 i_slice_alpha_offset,
					     u32 i_slice_beta_offset)
{
	u32 deblock_params;
	deblock_params = (u32)db_config;
	deblock_params |=
		((i_slice_beta_offset << 0x2) | (i_slice_alpha_offset << 0x7));

	/* Write deblocking control settings */
	VIDC_IO_OUT(REG_458130, deblock_params);
}

void vidc_720p_encode_set_intra_refresh_mb_number(u32 i_cir_mb_number)
{
	VIDC_IO_OUT(REG_857491, i_cir_mb_number);
}

void vidc_720p_encode_set_multi_slice_info(enum
					    vidc_720p_MSlice_selection
					    m_slice_sel,
					    u32 multi_slice_size)
{
	switch (m_slice_sel) {
	case VIDC_720P_MSLICE_BY_MB_COUNT:
		{
			VIDC_IO_OUT(REG_588301, 0x1);
			VIDC_IO_OUT(REG_1517, m_slice_sel);
			VIDC_IO_OUT(REG_105335, multi_slice_size);
			break;
		}
	case VIDC_720P_MSLICE_BY_BYTE_COUNT:
		{
			VIDC_IO_OUT(REG_588301, 0x1);
			VIDC_IO_OUT(REG_1517, m_slice_sel);
			VIDC_IO_OUT(REG_561679, multi_slice_size);
			break;
		}
	case VIDC_720P_MSLICE_BY_GOB:
		{
			VIDC_IO_OUT(REG_588301, 0x1);
			break;
		}
	default:
	case VIDC_720P_MSLICE_OFF:
		{
			VIDC_IO_OUT(REG_588301, 0x0);
			break;
		}
	}
}

void vidc_720p_encode_set_dpb_buffer(u32 *pi_enc_dpb_addr, u32 alloc_len)
{
	VIDC_IO_OUT(REG_341928_ADDR, pi_enc_dpb_addr);
	VIDC_IO_OUT(REG_319934, alloc_len);
}

void vidc_720p_encode_set_i_period(u32 i_i_period)
{
	VIDC_IO_OUT(REG_950374, i_i_period);
}

void vidc_720p_encode_init_codec(u32 i_ch_id,
				  enum vidc_720p_memory_access_method
				  memory_access_model)
{

	VIDC_IO_OUT(REG_841539, memory_access_model);
	vidc_720p_submit_command(i_ch_id, VIDC_720P_CMD_INITCODEC);
}

void vidc_720p_encode_unalign_bitstream(u32 upper_unalign_word,
					 u32 lower_unalign_word)
{
	VIDC_IO_OUT(REG_792026, upper_unalign_word);
	VIDC_IO_OUT(REG_844152, lower_unalign_word);
}

void vidc_720p_encode_set_seq_header_buffer(u32 ext_buffer_start,
					     u32 ext_buffer_end,
					     u32 start_byte_num)
{
	VIDC_IO_OUT(REG_275113_ADDR, ext_buffer_start);

	VIDC_IO_OUT(REG_87912, ext_buffer_start);

	VIDC_IO_OUT(REG_988007_ADDR, ext_buffer_end);

	VIDC_IO_OUT(REG_66693, start_byte_num);
}

void vidc_720p_encode_frame(u32 ch_id,
			     u32 ext_buffer_start,
			     u32 ext_buffer_end,
			     u32 start_byte_number, u32 y_addr,
			     u32 c_addr)
{
	VIDC_IO_OUT(REG_275113_ADDR, ext_buffer_start);

	VIDC_IO_OUT(REG_988007_ADDR, ext_buffer_end);

	VIDC_IO_OUT(REG_87912, ext_buffer_start);

	VIDC_IO_OUT(REG_66693, start_byte_number);

	VIDC_IO_OUT(REG_99105, y_addr);

	VIDC_IO_OUT(REG_777113_ADDR, c_addr);

	vidc_720p_submit_command(ch_id, VIDC_720P_CMD_FRAMERUN);
}

void vidc_720p_encode_get_header(u32 *pi_enc_header_size)
{
	VIDC_IO_IN(REG_114286, pi_enc_header_size);
}

void vidc_720p_enc_frame_info(struct vidc_720p_enc_frame_info
			       *enc_frame_info)
{
	VIDC_IO_IN(REG_782249, &enc_frame_info->enc_size);

	VIDC_IO_IN(REG_441270, &enc_frame_info->frame);

	enc_frame_info->frame &= 0x03;

	VIDC_IO_IN(REG_613254,
		    &enc_frame_info->metadata_exists);
}

void vidc_720p_decode_bitstream_header(u32 ch_id,
					u32 dec_unit_size,
					u32 start_byte_num,
					u32 ext_buffer_start,
					u32 ext_buffer_end,
					enum
					vidc_720p_memory_access_method
					memory_access_model,
					u32 decode_order)
{
	VIDC_IO_OUT(REG_965480, decode_order);

	VIDC_IO_OUT(REG_639999, 0x8080);

	VIDC_IO_OUT(REG_275113_ADDR, ext_buffer_start);

	VIDC_IO_OUT(REG_988007_ADDR, ext_buffer_end);

	VIDC_IO_OUT(REG_87912, ext_buffer_end);

	VIDC_IO_OUT(REG_761892, dec_unit_size);

	VIDC_IO_OUT(REG_66693, start_byte_num);

	VIDC_IO_OUT(REG_841539, memory_access_model);

	vidc_720p_submit_command(ch_id, VIDC_720P_CMD_INITCODEC);
}

void vidc_720p_decode_get_seq_hdr_info(struct vidc_720p_seq_hdr_info
					*seq_hdr_info)
{
	u32 display_status;
	VIDC_IO_IN(REG_999267, &seq_hdr_info->img_size_x);

	VIDC_IO_IN(REG_345712, &seq_hdr_info->img_size_y);

	VIDC_IO_IN(REG_257463, &seq_hdr_info->min_num_dpb);

	VIDC_IO_IN(REG_854281, &seq_hdr_info->min_dpb_size);

	VIDC_IO_IN(REG_580603, &seq_hdr_info->dec_frm_size);

	VIDC_IO_INF(REG_606447, DISP_PIC_PROFILE,
				 &seq_hdr_info->profile);

	VIDC_IO_INF(REG_606447, DIS_PIC_LEVEL,
				 &seq_hdr_info->level);

	VIDC_IO_INF(REG_612715, DISPLAY_STATUS,
				&display_status);
	seq_hdr_info->progressive =
			((display_status & 0x4) >> 2);
	/* bit 3 is for crop existence */
	seq_hdr_info->crop_exists = ((display_status & 0x8) >> 3);

	if (seq_hdr_info->crop_exists) {
		/* read the cropping information */
		VIDC_IO_INF(REG_881638, CROP_RIGHT_OFFSET, \
			&seq_hdr_info->crop_right_offset);
		VIDC_IO_INF(REG_881638, CROP_LEFT_OFFSET, \
			&seq_hdr_info->crop_left_offset);
		VIDC_IO_INF(REG_161486, CROP_BOTTOM_OFFSET, \
			&seq_hdr_info->crop_bottom_offset);
		VIDC_IO_INF(REG_161486, CROP_TOP_OFFSET, \
			&seq_hdr_info->crop_top_offset);
	}
	/* Read the MPEG4 data partitioning indication */
	VIDC_IO_INF(REG_441270, DATA_PARTITIONED, \
				&seq_hdr_info->data_partitioned);

}

void vidc_720p_decode_set_dpb_release_buffer_mask(u32
						   i_dpb_release_buffer_mask)
{
	VIDC_IO_OUT(REG_603032, i_dpb_release_buffer_mask);
}

void vidc_720p_decode_set_dpb_buffers(u32 i_buf_index, u32 *pi_dpb_buffer)
{
	VIDC_IO_OUTI(REG_615716, i_buf_index, pi_dpb_buffer);
}

void vidc_720p_decode_set_comv_buffer(u32 *pi_dpb_comv_buffer,
				       u32 alloc_len)
{
	VIDC_IO_OUT(REG_456376_ADDR, pi_dpb_comv_buffer);

	VIDC_IO_OUT(REG_490443, alloc_len);
}

void vidc_720p_decode_set_dpb_details(u32 num_dpb, u32 alloc_len,
				       u32 *ref_buffer)
{
	VIDC_IO_OUT(REG_518133, ref_buffer);

	VIDC_IO_OUT(REG_267567, 0);

	VIDC_IO_OUT(REG_883500, num_dpb);

	VIDC_IO_OUT(REG_319934, alloc_len);
}

void vidc_720p_decode_set_mpeg4Post_filter(u32 enable_post_filter)
{
	if (enable_post_filter)
		VIDC_IO_OUT(REG_443811, 0x1);
	else
		VIDC_IO_OUT(REG_443811, 0x0);
}

void vidc_720p_decode_set_error_control(u32 enable_error_control)
{
	if (enable_error_control)
		VIDC_IO_OUT(REG_846346, 0);
	else
		VIDC_IO_OUT(REG_846346, 1);
}

void vidc_720p_set_deblock_line_buffer(u32 *pi_deblock_line_buffer_start,
					u32 alloc_len)
{
	VIDC_IO_OUT(REG_979942, pi_deblock_line_buffer_start);

	VIDC_IO_OUT(REG_101184, alloc_len);
}

void vidc_720p_decode_set_mpeg4_data_partitionbuffer(u32 *vsp_buf_start)
{
    VIDC_IO_OUT(REG_958768, vsp_buf_start);
}

void vidc_720p_decode_setH264VSPBuffer(u32 *pi_vsp_temp_buffer_start)
{
	VIDC_IO_OUT(REG_958768, pi_vsp_temp_buffer_start);
}

void vidc_720p_decode_frame(u32 ch_id, u32 ext_buffer_start,
			     u32 ext_buffer_end, u32 dec_unit_size,
			     u32 start_byte_num, u32 input_frame_tag)
{
	VIDC_IO_OUT(REG_275113_ADDR, ext_buffer_start);

	VIDC_IO_OUT(REG_988007_ADDR, ext_buffer_end);

	VIDC_IO_OUT(REG_87912, ext_buffer_end);

	VIDC_IO_OUT(REG_66693, start_byte_num);

	VIDC_IO_OUT(REG_94750, input_frame_tag);

	VIDC_IO_OUT(REG_761892, dec_unit_size);

	vidc_720p_submit_command(ch_id, VIDC_720P_CMD_FRAMERUN);
}

void vidc_720p_issue_eos(u32 i_ch_id)
{
    VIDC_IO_OUT(REG_896825, 0x1);

    VIDC_IO_OUT(REG_761892, 0);

    vidc_720p_submit_command(i_ch_id, VIDC_720P_CMD_FRAMERUN);
}

void vidc_720p_eos_info(u32 *disp_status, u32 *resl_change)
{
   VIDC_IO_INF(REG_612715, DISPLAY_STATUS, disp_status);
   (*disp_status) = (*disp_status) & 0x3;
   VIDC_IO_INF(REG_724381, RESOLUTION_CHANGE, resl_change);
}

void vidc_720p_decode_display_info(struct vidc_720p_dec_disp_info
				    *disp_info)
{
	u32 display_status = 0;
	VIDC_IO_INF(REG_612715, DISPLAY_STATUS, &display_status);

	disp_info->disp_status =
	    (enum vidc_720p_display_status)((display_status & 0x3));

	disp_info->disp_is_interlace = ((display_status & 0x4) >> 2);
	disp_info->crop_exists = ((display_status & 0x8) >> 3);

	disp_info->resl_change = ((display_status & 0x30) >> 4);

	VIDC_IO_INF(REG_724381, RESOLUTION_CHANGE,
		     &disp_info->reconfig_flush_done);

	VIDC_IO_IN(REG_999267, &disp_info->img_size_x);

	VIDC_IO_IN(REG_345712, &disp_info->img_size_y);
	VIDC_IO_IN(REG_151345, &disp_info->y_addr);
	VIDC_IO_IN(REG_293983, &disp_info->c_addr);
	VIDC_IO_IN(REG_370409, &disp_info->tag_top);
	VIDC_IO_IN(REG_438677, &disp_info->tag_bottom);
	VIDC_IO_IN(REG_679165, &disp_info->pic_time_top);
	VIDC_IO_IN(REG_374150, &disp_info->pic_time_bottom);

	if (disp_info->crop_exists) {
		VIDC_IO_INF(REG_881638, CROP_RIGHT_OFFSET,
			&disp_info->crop_right_offset);
		VIDC_IO_INF(REG_881638, CROP_LEFT_OFFSET,
			&disp_info->crop_left_offset);
		VIDC_IO_INF(REG_161486, CROP_BOTTOM_OFFSET,
			&disp_info->crop_bottom_offset);
		VIDC_IO_INF(REG_161486, CROP_TOP_OFFSET,
			&disp_info->crop_top_offset);
	}
	VIDC_IO_IN(REG_613254, &disp_info->metadata_exists);

	VIDC_IO_IN(REG_580603,
		    &disp_info->input_bytes_consumed);

	VIDC_IO_IN(REG_757835, &disp_info->input_frame_num);

	VIDC_IO_INF(REG_441270, FRAME_TYPE,
			   &disp_info->input_frame);

	disp_info->input_is_interlace =
	    ((disp_info->input_frame & 0x4) >> 2);

	if (disp_info->input_frame & 0x10)
		disp_info->input_frame = VIDC_720P_IDRFRAME;
	else
		disp_info->input_frame &= 0x3;
}

void vidc_720p_decode_skip_frm_details(u32 *free_luma_dpb)
{
	u32 disp_frm;
	VIDC_IO_IN(REG_697961, &disp_frm);

	if (disp_frm == VIDC_720P_NOTCODED)
		VIDC_IO_IN(REG_347105, free_luma_dpb);
}

void vidc_720p_metadata_enable(u32 flag, u32 *input_buffer)
{
	VIDC_IO_OUT(REG_854681, flag);
	VIDC_IO_OUT(REG_988552, input_buffer);
}

void vidc_720p_decode_dynamic_req_reset(void)
{
	VIDC_IO_OUT(REG_76706, 0x0);
	VIDC_IO_OUT(REG_147682, 0x0);
	VIDC_IO_OUT(REG_896825, 0x0);
}

void vidc_720p_decode_dynamic_req_set(u32 property)
{
	if (property == VIDC_720P_FLUSH_REQ)
		VIDC_IO_OUT(REG_76706, 0x1);
	else if (property == VIDC_720P_EXTRADATA)
		VIDC_IO_OUT(REG_147682, 0x1);
}

void vidc_720p_decode_setpassthrough_start(u32 pass_startaddr)
{
	VIDC_IO_OUT(REG_486169, pass_startaddr);
}
