/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

#include "slimport_edid.h"

#define EDID_BLOCK_SIZE      128
#define BIT_EDID_FIELD_FORMAT_HDMI_TO_RGB   0x00
#define BIT_EDID_FIELD_FORMAT_YCbCr422      0x01
#define BIT_EDID_FIELD_FORMAT_YCbCr444      0x02
#define BIT_EDID_FIELD_FORMAT_DVI_TO_RGB    0x03

typedef struct
{
	unsigned parse_3d_in_progress			:1;
	unsigned FLAGS_SENT_3D_REQ				:1;
	unsigned FLAGS_BURST_3D_VIC_DONE		:1;
	unsigned FLAGS_BURST_3D_DTD_DONE		:1;

	unsigned FLAGS_BURST_3D_DTD_VESA_DONE	:1;
	unsigned FLAGS_BURST_3D_DONE			:1;
	unsigned FLAGS_EDID_READ_DONE			:1;
	unsigned reserved						:1;
}edid_3d_flags_t,*edid_3d_flags_p;

#define MAX_V_DESCRIPTORS			21
#define MAX_A_DESCRIPTORS			10
#define MAX_SPEAKER_CONFIGURATIONS	 4
#define AUDIO_DESCR_SIZE			 3

#define NUM_VIDEO_DATA_BLOCKS_LIMIT 3

typedef struct _edid_parse_data_t
{
	edid_3d_flags_t flags;
    P_vsdb_t p_HDMI_vsdb;
    p_video_data_block_t p_video_data_blocks_2d[NUM_VIDEO_DATA_BLOCKS_LIMIT];
    Pvideo_capability_data_block_t p_video_capability_data_block;
    PVSDB_byte_13_through_byte_15_t p_byte_13_through_byte_15;
    P_3D_mask_t p_3d_mask;
    P_3D_structure_and_detail_entry_u    p_three_d;
    uint8_t    *p_3d_limit;
    uint8_t     num_video_data_blocks; /* counter for initial EDID parsing, persists afterwards */
    uint8_t     video_data_block_index; /* counter for 3D write burst parsing. */
    uint8_t     burst_entry_count_3d_vic;
    uint8_t     vic_2d_index;
    uint8_t     vic_3d_index;
    uint8_t     burst_entry_count_3d_dtd;
    uint8_t     vesa_dtd_index;
    uint8_t     cea_861_dtd_index;

	CEA_short_audio_descriptor_t audio_descriptors[MAX_A_DESCRIPTORS];	/* maximum number of audio descriptors */
	uint8_t speaker_alloc[MAX_SPEAKER_CONFIGURATIONS];	/* maximum number of speaker configurations */
	bool	underscan;								/* "1" if DTV monitor underscans IT video formats by default */
	bool	basic_audio;								/* Sink supports Basic Audio */
	bool	YCbCr_4_4_4;							/* Sink supports YCbCr 4:4:4 */
	bool	YCbCr_4_2_2;							/* Sink supports YCbCr 4:2:2 */
	bool	HDMI_sink;								/* "1" if HDMI signature found */
	uint8_t CEC_A_B;								/* CEC Physical address. See HDMI 1.3 Table 8-6 */
	uint8_t CEC_C_D;
	uint8_t video_capability_flags;
	uint8_t colorimetry_support_flags;				/* IEC 61966-2-4 colorimetry support: 1 - xvYCC601; 2 - xvYCC709 */
	uint8_t meta_data_profile;
	bool	_3D_supported;
	uint8_t	num_EDID_extensions;
}edid_parse_data_t;

typedef struct
{
	void	   *dev_context;
	void					*drv_context;
	edid_parse_data_t	parse_data;
	uint8_t EDID_block_data [4*EDID_BLOCK_SIZE];
}edid_3d_data_t,*edid_3d_data_p;

void *si_edid_create_context(void *dev_context,void *drv_context);
void si_edid_destroy_context(void *context);
void si_mhl_tx_handle_atomic_hw_edid_read_complete(edid_3d_data_p mhl_edid_3d_data);
#if 0
void si_mhl_tx_initiate_edid_sequence(void *context);
//void si_mhl_tx_send_3d_req(void *context);
//void si_mhl_tx_process_3d_vic_burst(void *context, PMHL2_video_format_data_t pWriteBurstData );
//void si_mhl_tx_process_3d_dtd_burst(void *context,PMHL2_video_format_data_t pWriteBurstData);
//uint32_t si_mhl_tx_find_timings_from_totals(edid_3d_data_p mhl_edid_3d_data);
int si_edid_sink_is_hdmi(void *context);
int si_edid_quantization_range_selectable(void *context);
int si_edid_sink_supports_YCbCr422(void *context);
int si_edid_sink_supports_YCbCr444(void *context);
int si_edid_find_pixel_clock_from_HDMI_VIC(void *context,uint8_t vic);
int si_edid_find_pixel_clock_from_AVI_VIC(void *context,uint8_t vic);
#endif
typedef enum
{
	ne_NO_HPD = -4
   ,ne_BAD_DATA = -3
   ,ne_BAD_CHECKSUM = ne_BAD_DATA
   ,ne_BAD_HEADER = -2
   ,ne_BAD_HEADER_OFFSET_BY_1 = -1
   ,ne_SUCCESS = 0
}NumExtensions_e;
#if 0
int si_mhl_tx_get_num_cea_861_extensions(void *context,
											 uint8_t block_number);
int si_edid_read_done(void *context);
void si_edid_reset(edid_3d_data_p mhl_edid_3d_data);
#endif
/*
uint8_t qualify_pixel_clock_for_mhl(edid_3d_data_p mhl_edid_3d_data,
									uint32_t pixel_clock_frequency,
									uint8_t bits_per_pixel);
*/
uint8_t calculate_generic_checksum(uint8_t *infoFrameData,uint8_t checkSum,uint8_t length);
