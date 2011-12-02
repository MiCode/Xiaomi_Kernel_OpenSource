/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include "vidc.h"
#include "vidc_hwio.h"


#define VIDC_1080P_INIT_CH_INST_ID      0x0000ffff
#define VIDC_1080P_RESET_VI             0x3f7
#define VIDC_1080P_RESET_VI_RISC        0x3f6
#define VIDC_1080P_RESET_VI_VIDC_RISC    0x3f2
#define VIDC_1080P_RESET_ALL            0
#define VIDC_1080P_RESET_RISC           0x3fe
#define VIDC_1080P_RESET_NONE           0x3ff
#define VIDC_1080P_INTERRUPT_CLEAR      0
#define VIDC_1080P_MAX_H264DECODER_DPB  32
#define VIDC_1080P_MAX_DEC_RECON_BUF    32

#define VIDC_1080P_SI_RG7_DISPLAY_STATUS_MASK    0x00000007
#define VIDC_1080P_SI_RG7_DISPLAY_STATUS_SHIFT   0
#define VIDC_1080P_SI_RG7_DISPLAY_CODING_MASK    0x00000008
#define VIDC_1080P_SI_RG7_DISPLAY_CODING_SHIFT   3
#define VIDC_1080P_SI_RG7_DISPLAY_RES_MASK       0x00000030
#define VIDC_1080P_SI_RG7_DISPLAY_RES_SHIFT      4

#define VIDC_1080P_SI_RG7_DISPLAY_CROP_MASK      0x00000040
#define VIDC_1080P_SI_RG7_DISPLAY_CROP_SHIFT     6

#define VIDC_1080P_SI_RG7_DISPLAY_CORRECT_MASK    0x00000180
#define VIDC_1080P_SI_RG7_DISPLAY_CORRECT_SHIFT   7
#define VIDC_1080P_SI_RG8_DECODE_FRAMETYPE_MASK  0x00000007

#define VIDC_1080P_SI_RG10_NUM_DPB_BMSK      0x00003fff
#define VIDC_1080P_SI_RG10_NUM_DPB_SHFT      0
#define VIDC_1080P_SI_RG10_DPB_FLUSH_BMSK    0x00004000
#define VIDC_1080P_SI_RG10_DPB_FLUSH_SHFT    14
#define VIDC_1080P_SI_RG10_DMX_DISABLE_BMSK  0x00008000
#define VIDC_1080P_SI_RG10_DMX_DISABLE_SHFT  15

#define VIDC_1080P_SI_RG11_DECODE_STATUS_MASK    0x00000007
#define VIDC_1080P_SI_RG11_DECODE_STATUS_SHIFT   0
#define VIDC_1080P_SI_RG11_DECODE_CODING_MASK    0x00000008
#define VIDC_1080P_SI_RG11_DECODE_CODING_SHIFT   3
#define VIDC_1080P_SI_RG11_DECODE_RES_MASK       0x000000C0
#define VIDC_1080P_SI_RG11_DECODE_RES_SHIFT      6
#define VIDC_1080P_SI_RG11_DECODE_CROPP_MASK     0x00000100
#define VIDC_1080P_SI_RG11_DECODE_CROPP_SHIFT    8

#define VIDC_1080P_SI_RG11_DECODE_CORRECT_MASK    0x00000600
#define VIDC_1080P_SI_RG11_DECODE_CORRECT_SHIFT   9
#define VIDC_1080P_BASE_OFFSET_SHIFT         11


#define VIDC_1080P_H264DEC_LUMA_ADDR      HWIO_REG_759068_ADDR
#define VIDC_1080P_H264DEC_CHROMA_ADDR    HWIO_REG_515200_ADDR
#define VIDC_1080P_H264DEC_MV_PLANE_ADDR  HWIO_REG_466192_ADDR

#define VIDC_1080P_DEC_LUMA_ADDR        HWIO_REG_759068_ADDR
#define VIDC_1080P_DEC_CHROMA_ADDR      HWIO_REG_515200_ADDR

#define VIDC_1080P_DEC_TYPE_SEQ_HEADER         0x00010000
#define VIDC_1080P_DEC_TYPE_FRAME_DATA         0x00020000
#define VIDC_1080P_DEC_TYPE_LAST_FRAME_DATA    0x00030000
#define VIDC_1080P_DEC_TYPE_INIT_BUFFERS       0x00040000

#define VIDC_1080P_ENC_TYPE_SEQ_HEADER       0x00010000
#define VIDC_1080P_ENC_TYPE_FRAME_DATA       0x00020000
#define VIDC_1080P_ENC_TYPE_LAST_FRAME_DATA  0x00030000

#define VIDC_1080P_MAX_INTRA_PERIOD 0xffff

u8 *VIDC_BASE_PTR;

void vidc_1080p_do_sw_reset(enum vidc_1080p_reset init_flag)
{
	if (init_flag == VIDC_1080P_RESET_IN_SEQ_FIRST_STAGE) {
		u32 sw_reset_value = 0;

		VIDC_HWIO_IN(REG_557899, &sw_reset_value);
		sw_reset_value &= (~HWIO_REG_557899_RSTN_VI_BMSK);
		VIDC_HWIO_OUT(REG_557899, sw_reset_value);
		sw_reset_value &= (~HWIO_REG_557899_RSTN_RISC_BMSK);
		VIDC_HWIO_OUT(REG_557899, sw_reset_value);
		sw_reset_value &= (~(HWIO_REG_557899_RSTN_VIDCCORE_BMSK |
					HWIO_REG_557899_RSTN_DMX_BMSK));

		VIDC_HWIO_OUT(REG_557899, sw_reset_value);
	} else if (init_flag == VIDC_1080P_RESET_IN_SEQ_SECOND_STAGE) {
		VIDC_HWIO_OUT(REG_557899, VIDC_1080P_RESET_ALL);
		VIDC_HWIO_OUT(REG_557899, VIDC_1080P_RESET_RISC);
	}
}

void vidc_1080p_release_sw_reset(void)
{
	u32 nAxiCtl;
	u32 nAxiStatus;
	u32 nRdWrBurst;
	u32 nOut_Order;

	nOut_Order = VIDC_SETFIELD(1, HWIO_REG_5519_AXI_AOOORD_SHFT,
					HWIO_REG_5519_AXI_AOOORD_BMSK);
	VIDC_HWIO_OUT(REG_5519, nOut_Order);

	nOut_Order = VIDC_SETFIELD(1, HWIO_REG_606364_AXI_AOOOWR_SHFT,
					HWIO_REG_606364_AXI_AOOOWR_BMSK);
	VIDC_HWIO_OUT(REG_606364, nOut_Order);

	nAxiCtl = VIDC_SETFIELD(1, HWIO_REG_471159_AXI_HALT_REQ_SHFT,
				HWIO_REG_471159_AXI_HALT_REQ_BMSK);

	VIDC_HWIO_OUT(REG_471159, nAxiCtl);

	do {
		VIDC_HWIO_IN(REG_437878, &nAxiStatus);
		nAxiStatus = VIDC_GETFIELD(nAxiStatus,
					 HWIO_REG_437878_AXI_HALT_ACK_BMSK,
					 HWIO_REG_437878_AXI_HALT_ACK_SHFT);
	} while (0x3 != nAxiStatus);

	nAxiCtl  =  VIDC_SETFIELD(1,
				HWIO_REG_471159_AXI_RESET_SHFT,
				HWIO_REG_471159_AXI_RESET_BMSK);

	VIDC_HWIO_OUT(REG_471159, nAxiCtl);
	VIDC_HWIO_OUT(REG_471159, 0);

	nRdWrBurst = VIDC_SETFIELD(8,
				HWIO_REG_922106_XBAR_OUT_MAX_RD_BURST_SHFT,
				HWIO_REG_922106_XBAR_OUT_MAX_RD_BURST_BMSK) |
	VIDC_SETFIELD(8, HWIO_REG_922106_XBAR_OUT_MAX_WR_BURST_SHFT,
				HWIO_REG_922106_XBAR_OUT_MAX_WR_BURST_BMSK);

	VIDC_HWIO_OUT(REG_922106, nRdWrBurst);

	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_611794, VIDC_1080P_HOST2RISC_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_557899, VIDC_1080P_RESET_NONE);
}

void vidc_1080p_clear_interrupt(void)
{
	VIDC_HWIO_OUT(REG_575377, VIDC_1080P_INTERRUPT_CLEAR);
}

void vidc_1080p_set_host2risc_cmd(enum vidc_1080p_host2risc_cmd
	host2risc_command, u32 host2risc_arg1, u32 host2risc_arg2,
	u32 host2risc_arg3, u32 host2risc_arg4)
{
	VIDC_HWIO_OUT(REG_611794, VIDC_1080P_HOST2RISC_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_356340, host2risc_arg1);
	VIDC_HWIO_OUT(REG_899023, host2risc_arg2);
	VIDC_HWIO_OUT(REG_987762, host2risc_arg3);
	VIDC_HWIO_OUT(REG_544000, host2risc_arg4);
	VIDC_HWIO_OUT(REG_611794, host2risc_command);
}

void vidc_1080p_get_risc2host_cmd(u32 *pn_risc2host_command,
	u32 *pn_risc2host_arg1, u32 *pn_risc2host_arg2,
	u32 *pn_risc2host_arg3, u32 *pn_risc2host_arg4)
{
	VIDC_HWIO_IN(REG_695082, pn_risc2host_command);
	VIDC_HWIO_IN(REG_156596, pn_risc2host_arg1);
	VIDC_HWIO_IN(REG_222292, pn_risc2host_arg2);
	VIDC_HWIO_IN(REG_790962, pn_risc2host_arg3);
	VIDC_HWIO_IN(REG_679882, pn_risc2host_arg4);
}

void vidc_1080p_get_risc2host_cmd_status(u32 err_status,
	u32 *dec_err_status, u32 *disp_err_status)
{
	*dec_err_status = VIDC_GETFIELD(err_status,
		VIDC_RISC2HOST_ARG2_VIDC_DEC_ERROR_STATUS_BMSK,
		VIDC_RISC2HOST_ARG2_VIDC_DEC_ERROR_STATUS_SHFT);
	*disp_err_status = VIDC_GETFIELD(err_status,
		VIDC_RISC2HOST_ARG2_VIDC_DISP_ERROR_STATUS_BMSK,
		VIDC_RISC2HOST_ARG2_VIDC_DISP_ERROR_STATUS_SHFT);

}

void vidc_1080p_clear_risc2host_cmd(void)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
}

void vidc_1080p_get_fw_version(u32 *pn_fw_version)
{
	VIDC_HWIO_IN(REG_653206, pn_fw_version);
}

void vidc_1080p_get_fw_status(u32 *pn_fw_status)
{
	VIDC_HWIO_IN(REG_350619, pn_fw_status);
}

void vidc_1080p_init_memory_controller(u32 dram_base_addr_a,
	u32 dram_base_addr_b)
{
	VIDC_HWIO_OUT(REG_64440, dram_base_addr_a);
	VIDC_HWIO_OUT(REG_675915, dram_base_addr_b);
}

void vidc_1080p_get_memory_controller_status(u32 *pb_mc_abusy,
	u32 *pb_mc_bbusy)
{
	u32 mc_status = 0;

	VIDC_HWIO_IN(REG_399911, &mc_status);
	*pb_mc_abusy = (u32) ((mc_status &
			HWIO_REG_399911_MC_BUSY_A_BMSK) >>
			HWIO_REG_399911_MC_BUSY_A_SHFT);
	*pb_mc_bbusy = (u32) ((mc_status &
			HWIO_REG_399911_MC_BUSY_B_BMSK) >>
			HWIO_REG_399911_MC_BUSY_B_SHFT);
}

void vidc_1080p_set_h264_decode_buffers(u32 dpb, u32 dec_vert_nb_mv_offset,
	u32 dec_nb_ip_offset, u32 *pn_dpb_luma_offset,
	u32 *pn_dpb_chroma_offset, u32 *pn_mv_buffer_offset)
{
	u32 count = 0, num_dpb_used = dpb;
	u8 *vidc_dpb_luma_reg = (u8 *) VIDC_1080P_H264DEC_LUMA_ADDR;
	u8 *vidc_dpb_chroma_reg = (u8 *) VIDC_1080P_H264DEC_CHROMA_ADDR;
	u8 *vidc_mv_buffer_reg = (u8 *) VIDC_1080P_H264DEC_MV_PLANE_ADDR;

	VIDC_HWIO_OUT(REG_931311, (dec_vert_nb_mv_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_16277, (dec_nb_ip_offset >>
	VIDC_1080P_BASE_OFFSET_SHIFT));
	if (num_dpb_used > VIDC_1080P_MAX_H264DECODER_DPB)
		num_dpb_used = VIDC_1080P_MAX_H264DECODER_DPB;
	for (count = 0; count < num_dpb_used; count++) {
		VIDC_OUT_DWORD(vidc_dpb_luma_reg,
			(pn_dpb_luma_offset[count] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_OUT_DWORD(vidc_dpb_chroma_reg,
			(pn_dpb_chroma_offset[count] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_OUT_DWORD(vidc_mv_buffer_reg,
			(pn_mv_buffer_offset[count] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		vidc_dpb_luma_reg += 4;
		vidc_dpb_chroma_reg += 4;
		vidc_mv_buffer_reg += 4;
	}
}

void vidc_1080p_set_decode_recon_buffers(u32 recon_buffer,
	u32 *pn_dec_luma, u32 *pn_dec_chroma)
{
	u32 count = 0, recon_buf_to_program = recon_buffer;
	u8 *dec_recon_luma_reg = (u8 *) VIDC_1080P_DEC_LUMA_ADDR;
	u8 *dec_recon_chroma_reg = (u8 *) VIDC_1080P_DEC_CHROMA_ADDR;

	if (recon_buf_to_program > VIDC_1080P_MAX_DEC_RECON_BUF)
		recon_buf_to_program = VIDC_1080P_MAX_DEC_RECON_BUF;
	for (count = 0; count < recon_buf_to_program; count++) {
		VIDC_OUT_DWORD(dec_recon_luma_reg, (pn_dec_luma[count] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_OUT_DWORD(dec_recon_chroma_reg,
			(pn_dec_chroma[count] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		dec_recon_luma_reg += 4;
		dec_recon_chroma_reg += 4;
	}
}

void vidc_1080p_set_mpeg4_divx_decode_work_buffers(u32 nb_dcac_buffer_offset,
	u32 upnb_mv_buffer_offset, u32 sub_anchor_buffer_offset,
	u32 overlay_transform_buffer_offset, u32 stx_parser_buffer_offset)
{
	VIDC_HWIO_OUT(REG_931311, (nb_dcac_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_16277, (upnb_mv_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_654169, (sub_anchor_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_802794,
		(overlay_transform_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_252167, (stx_parser_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
}

void vidc_1080p_set_h263_decode_work_buffers(u32 nb_dcac_buffer_offset,
	u32 upnb_mv_buffer_offset, u32 sub_anchor_buffer_offset,
	u32 overlay_transform_buffer_offset)
{
	VIDC_HWIO_OUT(REG_931311, (nb_dcac_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_16277, (upnb_mv_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_654169, (sub_anchor_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_802794,
		(overlay_transform_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
}

void vidc_1080p_set_vc1_decode_work_buffers(u32 nb_dcac_buffer_offset,
	u32 upnb_mv_buffer_offset, u32 sub_anchor_buffer_offset,
	u32 overlay_transform_buffer_offset, u32 bitplain1Buffer_offset,
	u32 bitplain2Buffer_offset, u32 bitplain3Buffer_offset)
{
	VIDC_HWIO_OUT(REG_931311, (nb_dcac_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_16277, (upnb_mv_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_654169, (sub_anchor_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_802794,
		(overlay_transform_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_724376, (bitplain3Buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_551674, (bitplain2Buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_115991, (bitplain1Buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
}

void vidc_1080p_set_encode_recon_buffers(u32 recon_buffer,
	u32 *pn_enc_luma, u32 *pn_enc_chroma)
{
	if (recon_buffer > 0) {
		VIDC_HWIO_OUT(REG_294579, (pn_enc_luma[0] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_HWIO_OUT(REG_759068, (pn_enc_chroma[0] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
	}
	if (recon_buffer > 1) {
		VIDC_HWIO_OUT(REG_616802, (pn_enc_luma[1] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_HWIO_OUT(REG_833502, (pn_enc_chroma[1] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
	}
	if (recon_buffer > 2) {
		VIDC_HWIO_OUT(REG_61427, (pn_enc_luma[2] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_HWIO_OUT(REG_68356, (pn_enc_chroma[2] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
	}
	if (recon_buffer > 3) {
		VIDC_HWIO_OUT(REG_23318, (pn_enc_luma[3] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
		VIDC_HWIO_OUT(REG_127855, (pn_enc_chroma[3] >>
			VIDC_1080P_BASE_OFFSET_SHIFT));
	}
}

void vidc_1080p_set_h264_encode_work_buffers(u32 up_row_mv_buffer_offset,
	u32 direct_colzero_flag_buffer_offset,
	u32 upper_intra_md_buffer_offset,
	u32 upper_intra_pred_buffer_offset, u32 nbor_infor_buffer_offset,
	u32 mb_info_offset)
{
	VIDC_HWIO_OUT(REG_515200, (up_row_mv_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_69832,
		(direct_colzero_flag_buffer_offset>>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_256132,
		(upper_intra_md_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_475648,
		(upper_intra_pred_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_29510, (nbor_infor_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_175929, (mb_info_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
}

void vidc_1080p_set_h263_encode_work_buffers(u32 up_row_mv_buffer_offset,
	u32 up_row_inv_quanti_coeff_buffer_offset)
{
	VIDC_HWIO_OUT(REG_515200, (up_row_mv_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_29510, (
		up_row_inv_quanti_coeff_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
}

void vidc_1080p_set_mpeg4_encode_work_buffers(u32 skip_flag_buffer_offset,
	u32 up_row_inv_quanti_coeff_buffer_offset, u32 upper_mv_offset)
{
	VIDC_HWIO_OUT(REG_69832, (skip_flag_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_29510, (
		up_row_inv_quanti_coeff_buffer_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
	VIDC_HWIO_OUT(REG_515200, (upper_mv_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT));
}

void vidc_1080p_set_encode_frame_size(u32 hori_size, u32 vert_size)
{
	VIDC_HWIO_OUT(REG_934655, hori_size);
	VIDC_HWIO_OUT(REG_179070, vert_size);
}

void vidc_1080p_set_encode_profile_level(u32 encode_profile, u32 enc_level)
{
	u32 profile_level = 0;

	profile_level = VIDC_SETFIELD(enc_level,
				HWIO_REG_63643_LEVEL_SHFT,
				HWIO_REG_63643_LEVEL_BMSK) |
				VIDC_SETFIELD(encode_profile,
				HWIO_REG_63643_PROFILE_SHFT,
				HWIO_REG_63643_PROFILE_BMSK);
	VIDC_HWIO_OUT(REG_63643, profile_level);
}

void vidc_1080p_set_encode_field_picture_structure(u32 enc_field_picture)
{
	VIDC_HWIO_OUT(REG_786024, enc_field_picture);
}

void vidc_1080p_set_decode_mpeg4_pp_filter(u32 lf_enables)
{
	VIDC_HWIO_OUT(REG_152500, lf_enables);
}

void vidc_1080p_set_decode_qp_save_control(u32 enable_q_pout)
{
	VIDC_HWIO_OUT(REG_143629, enable_q_pout);
}

void vidc_1080p_get_returned_channel_inst_id(u32 *pn_rtn_chid)
{
	VIDC_HWIO_IN(REG_607589, pn_rtn_chid);
}

void vidc_1080p_clear_returned_channel_inst_id(void)
{
	VIDC_HWIO_OUT(REG_607589, VIDC_1080P_INIT_CH_INST_ID);
}

void vidc_1080p_get_decode_seq_start_result(
	struct vidc_1080p_seq_hdr_info *seq_hdr_info)
{
	u32 dec_disp_result;
	u32 frame = 0;
	VIDC_HWIO_IN(REG_845544, &seq_hdr_info->img_size_y);
	VIDC_HWIO_IN(REG_859906, &seq_hdr_info->img_size_x);
	VIDC_HWIO_IN(REG_490078, &seq_hdr_info->min_num_dpb);
	VIDC_HWIO_IN(REG_489688, &seq_hdr_info->dec_frm_size);
	VIDC_HWIO_IN(REG_853667, &dec_disp_result);
	seq_hdr_info->disp_progressive = VIDC_GETFIELD(dec_disp_result,
					VIDC_1080P_SI_RG7_DISPLAY_CODING_MASK,
					VIDC_1080P_SI_RG7_DISPLAY_CODING_SHIFT);
	seq_hdr_info->disp_crop_exists  = VIDC_GETFIELD(dec_disp_result,
		VIDC_1080P_SI_RG7_DISPLAY_CROP_MASK,
		VIDC_1080P_SI_RG7_DISPLAY_CROP_SHIFT);
	VIDC_HWIO_IN(REG_692991, &dec_disp_result);
	seq_hdr_info->dec_progressive = VIDC_GETFIELD(dec_disp_result,
					VIDC_1080P_SI_RG11_DECODE_CODING_MASK,
					VIDC_1080P_SI_RG11_DECODE_CODING_SHIFT);
	seq_hdr_info->dec_crop_exists  = VIDC_GETFIELD(dec_disp_result,
		VIDC_1080P_SI_RG11_DECODE_CROPP_MASK,
		VIDC_1080P_SI_RG11_DECODE_CROPP_SHIFT);
	VIDC_HWIO_IN(REG_760102, &frame);
	seq_hdr_info->data_partition = ((frame & 0x8) >> 3);
}

void vidc_1080p_get_decoded_frame_size(u32 *pn_decoded_size)
{
	VIDC_HWIO_IN(REG_489688, pn_decoded_size);
}

void vidc_1080p_get_display_frame_result(
	struct vidc_1080p_dec_disp_info *dec_disp_info)
{
	u32 display_result;
	VIDC_HWIO_IN(REG_640904, &dec_disp_info->display_y_addr);
	VIDC_HWIO_IN(REG_60114, &dec_disp_info->display_c_addr);
	VIDC_HWIO_IN(REG_853667, &display_result);
	VIDC_HWIO_IN(REG_845544, &dec_disp_info->img_size_y);
	VIDC_HWIO_IN(REG_859906, &dec_disp_info->img_size_x);
	dec_disp_info->display_status =
		(enum vidc_1080p_display_status)
		VIDC_GETFIELD(display_result,
		VIDC_1080P_SI_RG7_DISPLAY_STATUS_MASK,
		VIDC_1080P_SI_RG7_DISPLAY_STATUS_SHIFT);
	dec_disp_info->display_coding =
		(enum vidc_1080p_display_coding)
	VIDC_GETFIELD(display_result, VIDC_1080P_SI_RG7_DISPLAY_CODING_MASK,
		VIDC_1080P_SI_RG7_DISPLAY_CODING_SHIFT);
	dec_disp_info->disp_resl_change = VIDC_GETFIELD(display_result,
		VIDC_1080P_SI_RG7_DISPLAY_RES_MASK,
		VIDC_1080P_SI_RG7_DISPLAY_RES_SHIFT);
	dec_disp_info->disp_crop_exists = VIDC_GETFIELD(display_result,
		VIDC_1080P_SI_RG7_DISPLAY_CROP_MASK,
		VIDC_1080P_SI_RG7_DISPLAY_CROP_SHIFT);
	dec_disp_info->display_correct = VIDC_GETFIELD(display_result,
		VIDC_1080P_SI_RG7_DISPLAY_CORRECT_MASK,
		VIDC_1080P_SI_RG7_DISPLAY_CORRECT_SHIFT);
}

void vidc_1080p_get_decode_frame(
	enum vidc_1080p_decode_frame *pe_frame)
{
	u32 frame = 0;

	VIDC_HWIO_IN(REG_760102, &frame);
	*pe_frame = (enum vidc_1080p_decode_frame)
		(frame & VIDC_1080P_SI_RG8_DECODE_FRAMETYPE_MASK);
}

void vidc_1080p_get_decode_frame_result(
	struct vidc_1080p_dec_disp_info *dec_disp_info)
{
	u32 decode_result;

	VIDC_HWIO_IN(REG_378318, &dec_disp_info->decode_y_addr);
	VIDC_HWIO_IN(REG_203487, &dec_disp_info->decode_c_addr);
	VIDC_HWIO_IN(REG_692991, &decode_result);
	dec_disp_info->decode_status = (enum vidc_1080p_display_status)
				VIDC_GETFIELD(decode_result,
				VIDC_1080P_SI_RG11_DECODE_STATUS_MASK,
				VIDC_1080P_SI_RG11_DECODE_STATUS_SHIFT);
	dec_disp_info->decode_coding = (enum vidc_1080p_display_coding)
				VIDC_GETFIELD(decode_result,
				VIDC_1080P_SI_RG11_DECODE_CODING_MASK,
				VIDC_1080P_SI_RG11_DECODE_CODING_SHIFT);
	dec_disp_info->dec_resl_change = VIDC_GETFIELD(decode_result,
		VIDC_1080P_SI_RG11_DECODE_RES_MASK,
		VIDC_1080P_SI_RG11_DECODE_RES_SHIFT);
	dec_disp_info->dec_crop_exists = VIDC_GETFIELD(decode_result,
		VIDC_1080P_SI_RG11_DECODE_CROPP_MASK,
		VIDC_1080P_SI_RG11_DECODE_CROPP_SHIFT);
	dec_disp_info->decode_correct = VIDC_GETFIELD(decode_result,
		VIDC_1080P_SI_RG11_DECODE_CORRECT_MASK,
		VIDC_1080P_SI_RG11_DECODE_CORRECT_SHIFT);
}

void vidc_1080p_decode_seq_start_ch0(
	struct vidc_1080p_dec_seq_start_param *param)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_117192,
		param->stream_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_145068, param->stream_frame_size);
	VIDC_HWIO_OUT(REG_921356,
		param->descriptor_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_190381,  param->stream_buffersize);
	VIDC_HWIO_OUT(REG_85655,  param->descriptor_buffer_size);
	VIDC_HWIO_OUT(REG_889944,  param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_404623, 0);
	VIDC_HWIO_OUT(REG_397087, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_DEC_TYPE_SEQ_HEADER |
		param->inst_id);
}

void vidc_1080p_decode_seq_start_ch1(
	struct vidc_1080p_dec_seq_start_param *param)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_980194,
		param->stream_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_936704, param->stream_frame_size);
	VIDC_HWIO_OUT(REG_821977,
		param->descriptor_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_887095, param->stream_buffersize);
	VIDC_HWIO_OUT(REG_576987, param->descriptor_buffer_size);
	VIDC_HWIO_OUT(REG_652528, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_404623, 0);
	VIDC_HWIO_OUT(REG_254093, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_DEC_TYPE_SEQ_HEADER |
		param->inst_id);
}

void vidc_1080p_decode_frame_start_ch0(
	struct vidc_1080p_dec_frame_start_param *param)
{
	u32 dpb_config;

	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	if ((param->decode == VIDC_1080P_DEC_TYPE_LAST_FRAME_DATA) &&
		((!param->stream_buffer_addr_offset) ||
		(!param->stream_frame_size))) {
		VIDC_HWIO_OUT(REG_117192, 0);
		VIDC_HWIO_OUT(REG_145068, 0);
		VIDC_HWIO_OUT(REG_190381, 0);
	} else {
		VIDC_HWIO_OUT(REG_117192,
			param->stream_buffer_addr_offset >>
			VIDC_1080P_BASE_OFFSET_SHIFT);
		VIDC_HWIO_OUT(REG_145068,
			param->stream_frame_size);
		VIDC_HWIO_OUT(REG_190381,
			param->stream_buffersize);
	}
	dpb_config = VIDC_SETFIELD(param->dmx_disable,
					VIDC_1080P_SI_RG10_DMX_DISABLE_SHFT,
					VIDC_1080P_SI_RG10_DMX_DISABLE_BMSK) |
				VIDC_SETFIELD(param->dpb_flush,
					VIDC_1080P_SI_RG10_DPB_FLUSH_SHFT,
					VIDC_1080P_SI_RG10_DPB_FLUSH_BMSK) |
				VIDC_SETFIELD(param->dpb_count,
					VIDC_1080P_SI_RG10_NUM_DPB_SHFT,
					VIDC_1080P_SI_RG10_NUM_DPB_BMSK);
	VIDC_HWIO_OUT(REG_921356,
		param->descriptor_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_85655, param->descriptor_buffer_size);
	VIDC_HWIO_OUT(REG_86830, param->release_dpb_bit_mask);
	VIDC_HWIO_OUT(REG_889944, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_404623, dpb_config);
	VIDC_HWIO_OUT(REG_397087, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_666957, (u32)param->decode |
		param->inst_id);
}


void vidc_1080p_decode_frame_start_ch1(
	struct vidc_1080p_dec_frame_start_param *param)
{
	u32 dpb_config;

	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_INIT_CH_INST_ID);
	if ((param->decode == VIDC_1080P_DEC_TYPE_LAST_FRAME_DATA) &&
		((!param->stream_buffer_addr_offset) ||
		(!param->stream_frame_size))) {
		VIDC_HWIO_OUT(REG_980194, 0);
		VIDC_HWIO_OUT(REG_936704, 0);
		VIDC_HWIO_OUT(REG_887095, 0);
	} else {
		VIDC_HWIO_OUT(REG_980194,
			param->stream_buffer_addr_offset >>
			VIDC_1080P_BASE_OFFSET_SHIFT);
		VIDC_HWIO_OUT(REG_936704,
			param->stream_frame_size);
		VIDC_HWIO_OUT(REG_887095,
			param->stream_buffersize);
	}
	dpb_config = VIDC_SETFIELD(param->dmx_disable,
					VIDC_1080P_SI_RG10_DMX_DISABLE_SHFT,
					VIDC_1080P_SI_RG10_DMX_DISABLE_BMSK) |
				VIDC_SETFIELD(param->dpb_flush,
					VIDC_1080P_SI_RG10_DPB_FLUSH_SHFT,
					VIDC_1080P_SI_RG10_DPB_FLUSH_BMSK) |
				VIDC_SETFIELD(param->dpb_count,
					VIDC_1080P_SI_RG10_NUM_DPB_SHFT,
					VIDC_1080P_SI_RG10_NUM_DPB_BMSK);
	VIDC_HWIO_OUT(REG_821977,
		param->descriptor_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_576987, param->descriptor_buffer_size);
	VIDC_HWIO_OUT(REG_70448, param->release_dpb_bit_mask);
	VIDC_HWIO_OUT(REG_652528, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_220637, dpb_config);
	VIDC_HWIO_OUT(REG_254093, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_313350, (u32)param->decode |
		param->inst_id);
}

void vidc_1080p_decode_init_buffers_ch0(
	struct vidc_1080p_dec_init_buffers_param *param)
{
	u32 dpb_config;
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	dpb_config = VIDC_SETFIELD(param->dmx_disable,
					VIDC_1080P_SI_RG10_DMX_DISABLE_SHFT,
					VIDC_1080P_SI_RG10_DMX_DISABLE_BMSK) |
				VIDC_SETFIELD(param->dpb_count,
					VIDC_1080P_SI_RG10_NUM_DPB_SHFT,
					VIDC_1080P_SI_RG10_NUM_DPB_BMSK);
	VIDC_HWIO_OUT(REG_889944, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_404623, dpb_config);
	VIDC_HWIO_OUT(REG_397087, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_DEC_TYPE_INIT_BUFFERS |
		param->inst_id);
}

void vidc_1080p_decode_init_buffers_ch1(
	struct vidc_1080p_dec_init_buffers_param *param)
{
	u32 dpb_config;
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_INIT_CH_INST_ID);
	dpb_config = VIDC_SETFIELD(param->dmx_disable,
					VIDC_1080P_SI_RG10_DMX_DISABLE_SHFT,
					VIDC_1080P_SI_RG10_DMX_DISABLE_BMSK) |
				VIDC_SETFIELD(param->dpb_count,
					VIDC_1080P_SI_RG10_NUM_DPB_SHFT,
					VIDC_1080P_SI_RG10_NUM_DPB_BMSK);
	VIDC_HWIO_OUT(REG_652528,  param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_220637, dpb_config);
	VIDC_HWIO_OUT(REG_254093, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_DEC_TYPE_INIT_BUFFERS |
		param->inst_id);
}

void vidc_1080p_set_dec_resolution_ch0(u32 width, u32 height)
{
	VIDC_HWIO_OUT(REG_612810, height);
	VIDC_HWIO_OUT(REG_175608, width);
}

void vidc_1080p_set_dec_resolution_ch1(u32 width, u32 height)
{
	VIDC_HWIO_OUT(REG_655721, height);
	VIDC_HWIO_OUT(REG_548308, width);
}

void vidc_1080p_get_encode_frame_info(
	struct vidc_1080p_enc_frame_info *frame_info)
{
	VIDC_HWIO_IN(REG_845544, &(frame_info->enc_frame_size));
	VIDC_HWIO_IN(REG_859906,
		&(frame_info->enc_picture_count));
	VIDC_HWIO_IN(REG_490078,
		&(frame_info->enc_write_pointer));
	VIDC_HWIO_IN(REG_640904,
		(u32 *)(&(frame_info->enc_frame)));
	VIDC_HWIO_IN(REG_60114,
		&(frame_info->enc_luma_address));
	frame_info->enc_luma_address = frame_info->enc_luma_address <<
		VIDC_1080P_BASE_OFFSET_SHIFT;
	VIDC_HWIO_IN(REG_489688,
		&(frame_info->enc_chroma_address));
	frame_info->enc_chroma_address = frame_info->\
		enc_chroma_address << VIDC_1080P_BASE_OFFSET_SHIFT;
}

void vidc_1080p_encode_seq_start_ch0(
	struct vidc_1080p_enc_seq_start_param *param)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_117192,
		param->stream_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_921356, param->stream_buffer_size);
	VIDC_HWIO_OUT(REG_889944, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_397087, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_ENC_TYPE_SEQ_HEADER |
		param->inst_id);
}

void vidc_1080p_encode_seq_start_ch1(
	struct vidc_1080p_enc_seq_start_param *param)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_980194,
		param->stream_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_821977, param->stream_buffer_size);
	VIDC_HWIO_OUT(REG_652528, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_254093, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_ENC_TYPE_SEQ_HEADER |
		param->inst_id);
}

void vidc_1080p_encode_frame_start_ch0(
	struct vidc_1080p_enc_frame_start_param *param)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_117192,
		param->stream_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_921356, param->stream_buffer_size);
	VIDC_HWIO_OUT(REG_612810, param->current_y_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_175608, param->current_c_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_190381, param->intra_frame);
	VIDC_HWIO_OUT(REG_889944, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_404623, param->input_flush);
	VIDC_HWIO_OUT(REG_397087, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_666957, (u32)param->encode |
		param->inst_id);
}

void vidc_1080p_encode_frame_start_ch1(
	struct vidc_1080p_enc_frame_start_param *param)
{

	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_313350, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_980194,
		param->stream_buffer_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_821977, param->stream_buffer_size);
	VIDC_HWIO_OUT(REG_655721, param->current_y_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_548308,  param->current_c_addr_offset >>
		VIDC_1080P_BASE_OFFSET_SHIFT);
	VIDC_HWIO_OUT(REG_887095, param->intra_frame);
	VIDC_HWIO_OUT(REG_652528, param->shared_mem_addr_offset);
	VIDC_HWIO_OUT(REG_404623, param->input_flush);
	VIDC_HWIO_OUT(REG_254093, param->cmd_seq_num);
	VIDC_HWIO_OUT(REG_313350, (u32)param->encode |
		param->inst_id);
}

void vidc_1080p_set_encode_picture(u32 number_p, u32 number_b)
{
	u32 picture, ifrm_ctrl;
	if (number_p >= VIDC_1080P_MAX_INTRA_PERIOD)
		ifrm_ctrl = 0;
	else
		ifrm_ctrl = number_p + 1;
	picture = VIDC_SETFIELD(1 ,
				HWIO_REG_783891_ENC_PIC_TYPE_USE_SHFT,
				HWIO_REG_783891_ENC_PIC_TYPE_USE_BMSK) |
				VIDC_SETFIELD(ifrm_ctrl,
					HWIO_REG_783891_I_FRM_CTRL_SHFT,
					HWIO_REG_783891_I_FRM_CTRL_BMSK)
				| VIDC_SETFIELD(number_b ,
				HWIO_REG_783891_B_FRM_CTRL_SHFT ,
				HWIO_REG_783891_B_FRM_CTRL_BMSK);
	VIDC_HWIO_OUT(REG_783891, picture);
}

void vidc_1080p_set_encode_multi_slice_control(
	enum vidc_1080p_MSlice_selection multiple_slice_selection,
	u32 mslice_mb, u32 mslice_byte)
{
	VIDC_HWIO_OUT(REG_226332, multiple_slice_selection);
	VIDC_HWIO_OUT(REG_696136, mslice_mb);
	VIDC_HWIO_OUT(REG_515564, mslice_byte);
}

void vidc_1080p_set_encode_circular_intra_refresh(u32 cir_num)
{
	VIDC_HWIO_OUT(REG_886210, cir_num);
}

void vidc_1080p_set_encode_input_frame_format(
	enum vidc_1080p_memory_access_method memory_format)
{
	VIDC_HWIO_OUT(REG_645603, memory_format);
}

void vidc_1080p_set_encode_padding_control(u32 pad_ctrl_on,
	u32 cr_pad_val, u32 cb_pad_val, u32 luma_pad_val)
{
	u32 padding = VIDC_SETFIELD(pad_ctrl_on ,
				HWIO_REG_811733_PAD_CTRL_ON_SHFT,
				HWIO_REG_811733_PAD_CTRL_ON_BMSK) |
			VIDC_SETFIELD(cr_pad_val ,
				HWIO_REG_811733_CR_PAD_VIDC_SHFT ,
				HWIO_REG_811733_CR_PAD_VIDC_BMSK) |
			VIDC_SETFIELD(cb_pad_val ,
				HWIO_REG_811733_CB_PAD_VIDC_SHFT ,
				HWIO_REG_811733_CB_PAD_VIDC_BMSK) |
			VIDC_SETFIELD(luma_pad_val ,
				HWIO_REG_811733_LUMA_PAD_VIDC_SHFT ,
				HWIO_REG_811733_LUMA_PAD_VIDC_BMSK) ;
	VIDC_HWIO_OUT(REG_811733, padding);
}

void vidc_1080p_encode_set_rc_config(u32 enable_frame_level_rc,
	u32 enable_mb_level_rc_flag, u32 frame_qp)
{
	u32 rc_config = VIDC_SETFIELD(enable_frame_level_rc ,
					HWIO_REG_559908_FR_RC_EN_SHFT ,
					HWIO_REG_559908_FR_RC_EN_BMSK) |
			VIDC_SETFIELD(enable_mb_level_rc_flag ,
					HWIO_REG_559908_MB_RC_EN_SHFT,
					HWIO_REG_559908_MB_RC_EN_BMSK) |
			VIDC_SETFIELD(frame_qp ,
					HWIO_REG_559908_FRAME_QP_SHFT ,
					HWIO_REG_559908_FRAME_QP_BMSK);
	VIDC_HWIO_OUT(REG_559908, rc_config);
}

void vidc_1080p_encode_set_frame_level_rc_params(u32 rc_frame_rate,
	u32 target_bitrate, u32 reaction_coeff)
{
	VIDC_HWIO_OUT(REG_977937, rc_frame_rate);
	VIDC_HWIO_OUT(REG_166135, target_bitrate);
	VIDC_HWIO_OUT(REG_550322, reaction_coeff);
}

void vidc_1080p_encode_set_qp_params(u32 max_qp, u32 min_qp)
{
	u32 qbound = VIDC_SETFIELD(max_qp , HWIO_REG_109072_MAX_QP_SHFT,
					HWIO_REG_109072_MAX_QP_BMSK) |
					VIDC_SETFIELD(min_qp,
					HWIO_REG_109072_MIN_QP_SHFT ,
					HWIO_REG_109072_MIN_QP_BMSK);
	VIDC_HWIO_OUT(REG_109072, qbound);
}

void vidc_1080p_encode_set_mb_level_rc_params(u32 disable_dark_region_as_flag,
	u32 disable_smooth_region_as_flag , u32 disable_static_region_as_flag,
	u32 disable_activity_region_flag)
{
	u32 rc_active_feature = VIDC_SETFIELD(
					disable_dark_region_as_flag,
					HWIO_REG_949086_DARK_DISABLE_SHFT,
					HWIO_REG_949086_DARK_DISABLE_BMSK) |
					VIDC_SETFIELD(
					disable_smooth_region_as_flag,
					HWIO_REG_949086_SMOOTH_DISABLE_SHFT,
					HWIO_REG_949086_SMOOTH_DISABLE_BMSK) |
					VIDC_SETFIELD(
					disable_static_region_as_flag,
					HWIO_REG_949086_STATIC_DISABLE_SHFT,
					HWIO_REG_949086_STATIC_DISABLE_BMSK) |
					VIDC_SETFIELD(
					disable_activity_region_flag,
					HWIO_REG_949086_ACT_DISABLE_SHFT,
					HWIO_REG_949086_ACT_DISABLE_BMSK);
	VIDC_HWIO_OUT(REG_949086, rc_active_feature);
}

void vidc_1080p_set_h264_encode_entropy(
	enum vidc_1080p_entropy_sel entropy_sel)
{
	VIDC_HWIO_OUT(REG_447796, entropy_sel);
}

void vidc_1080p_set_h264_encode_loop_filter(
	enum vidc_1080p_DBConfig db_config, u32 slice_alpha_offset,
	u32 slice_beta_offset)
{
	VIDC_HWIO_OUT(REG_152500, db_config);
	VIDC_HWIO_OUT(REG_266285, slice_alpha_offset);
	VIDC_HWIO_OUT(REG_964731, slice_beta_offset);
}

void vidc_1080p_set_h264_encoder_p_frame_ref_count(u32 max_reference)
{
	u32 ref_frames;
	ref_frames = VIDC_SETFIELD(max_reference,
		HWIO_REG_744348_P_SHFT,
		HWIO_REG_744348_P_BMSK);
	VIDC_HWIO_OUT(REG_744348, ref_frames);
}

void vidc_1080p_set_h264_encode_8x8transform_control(u32 enable_8x8transform)
{
	VIDC_HWIO_OUT(REG_672163, enable_8x8transform);
}

void vidc_1080p_set_mpeg4_encode_quarter_pel_control(
	u32 enable_mpeg4_quarter_pel)
{
	VIDC_HWIO_OUT(REG_330132, enable_mpeg4_quarter_pel);
}

void vidc_1080p_set_device_base_addr(u8 *mapped_va)
{
	VIDC_BASE_PTR = mapped_va;
}

void vidc_1080p_get_intra_bias(u32 *bias)
{
	u32 intra_bias;

	VIDC_HWIO_IN(REG_676866, &intra_bias);
	*bias = VIDC_GETFIELD(intra_bias,
					HWIO_REG_676866_RMSK,
					HWIO_REG_676866_SHFT);
}

void vidc_1080p_set_intra_bias(u32 bias)
{
	u32 intra_bias;

	intra_bias = VIDC_SETFIELD(bias,
					HWIO_REG_676866_SHFT,
					HWIO_REG_676866_RMSK);
	VIDC_HWIO_OUT(REG_676866, intra_bias);
}

void vidc_1080p_get_bi_directional_bias(u32 *bi_directional_bias)
{
	u32 nbi_direct_bias;

	VIDC_HWIO_IN(REG_54267, &nbi_direct_bias);
	*bi_directional_bias = VIDC_GETFIELD(nbi_direct_bias,
					HWIO_REG_54267_RMSK,
					HWIO_REG_54267_SHFT);
}

void vidc_1080p_set_bi_directional_bias(u32 bi_directional_bias)
{
	u32 nbi_direct_bias;

	nbi_direct_bias = VIDC_SETFIELD(bi_directional_bias,
					HWIO_REG_54267_SHFT,
					HWIO_REG_54267_RMSK);
	VIDC_HWIO_OUT(REG_54267, nbi_direct_bias);
}

void vidc_1080p_get_encoder_sequence_header_size(u32 *seq_header_size)
{
	VIDC_HWIO_IN(REG_845544, seq_header_size);
}

void vidc_1080p_get_intermedia_stage_debug_counter(
	u32 *intermediate_stage_counter)
{
	VIDC_HWIO_IN(REG_805993, intermediate_stage_counter);
}

void vidc_1080p_get_exception_status(u32 *exception_status)
{
	VIDC_HWIO_IN(REG_493355, exception_status);
}

void vidc_1080p_frame_start_realloc(u32 instance_id)
{
	VIDC_HWIO_OUT(REG_695082, VIDC_1080P_RISC2HOST_CMD_EMPTY);
	VIDC_HWIO_OUT(REG_666957, VIDC_1080P_INIT_CH_INST_ID);
	VIDC_HWIO_OUT(REG_666957,
		VIDC_1080P_DEC_TYPE_FRAME_START_REALLOC | instance_id);
}
