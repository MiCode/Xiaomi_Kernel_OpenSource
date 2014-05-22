/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef _SI_MHL2_EDID_3D_API_H_
#define _SI_MHL2_EDID_3D_API_H_

#define EDID_BLOCK_SIZE				128
#define BIT_EDID_FIELD_FORMAT_HDMI_TO_RGB	0x00
#define BIT_EDID_FIELD_FORMAT_YCbCr422		0x01
#define BIT_EDID_FIELD_FORMAT_YCbCr444		0x02
#define BIT_EDID_FIELD_FORMAT_DVI_TO_RGB	0x03

struct edid_3d_flags_t {
	unsigned parse_3d_in_progress:1;
	unsigned FLAGS_SENT_3D_REQ:1;
	unsigned FLAGS_BURST_3D_VIC_DONE:1;
	unsigned FLAGS_BURST_3D_DTD_DONE:1;

	unsigned FLAGS_BURST_3D_DTD_VESA_DONE:1;
	unsigned FLAGS_BURST_3D_DONE:1;
	unsigned FLAGS_EDID_READ_DONE:1;
	unsigned reserved:1;
};

#define MAX_V_DESCRIPTORS			21
#define MAX_A_DESCRIPTORS			10
#define MAX_SPEAKER_CONFIGURATIONS		4
#define AUDIO_DESCR_SIZE			3

#define NUM_VIDEO_DATA_BLOCKS_LIMIT		3

struct edid_parse_data_t {
	struct edid_3d_flags_t flags;
	struct vsdb_t *p_HDMI_vsdb;
	struct video_data_block_t *
		p_video_data_blocks_2d[NUM_VIDEO_DATA_BLOCKS_LIMIT];
	struct video_capability_data_block_t *p_video_capability_data_block;
	struct VSDB_byte_13_through_byte_15_t *p_byte_13_through_byte_15;
	struct _3D_mask_t *p_3d_mask;
	union _3D_structure_and_detail_entry_u *p_three_d;
	uint8_t *p_3d_limit;
	/* counter for initial EDID parsing, persists afterwards */
	uint8_t num_video_data_blocks;
	/* counter for 3D write burst parsing. */
	uint8_t video_data_block_index;
	uint8_t burst_entry_count_3d_vic;
	uint8_t vic_2d_index;
	uint8_t vic_3d_index;
	uint8_t burst_entry_count_3d_dtd;
	uint8_t vesa_dtd_index;
	uint8_t cea_861_dtd_index;
	uint8_t num_vesa_timing_dtds;
	uint8_t num_cea_861_timing_dtds;
	/* maximum number of audio descriptors */
	struct CEA_short_audio_descriptor_t
		audio_descriptors[MAX_A_DESCRIPTORS];
	/* maximum number of speaker configurations */
	uint8_t speaker_alloc[MAX_SPEAKER_CONFIGURATIONS];
	/* "1" if DTV monitor underscans IT video formats by default */
	bool underscan;
	bool basic_audio;	/* Sink supports Basic Audio */
	bool YCbCr_4_4_4;	/* Sink supports YCbCr 4:4:4 */
	bool YCbCr_4_2_2;	/* Sink supports YCbCr 4:2:2 */
	bool HDMI_sink;		/* "1" if HDMI signature found */
	/* CEC Physical address. See HDMI 1.3 Table 8-6 */
	uint8_t CEC_A_B;
	uint8_t CEC_C_D;
	uint8_t video_capability_flags;
	/* IEC 61966-2-4 colorimetry support: 1 - xvYCC601; 2 - xvYCC709 */
	uint8_t colorimetry_support_flags;
	uint8_t meta_data_profile;
	bool _3D_supported;
	uint8_t num_EDID_extensions;
};

struct mhl_dev_context;

struct item_alloc_info_t {
	size_t num_items;
	size_t num_items_allocated;
	size_t index;
};

struct edid_3d_data_t {
	struct mhl_dev_context *dev_context;
	struct drv_hw_context *drv_context;
	struct MHL3_hev_dtd_item_t hev_dtd_payload;
	struct MHL3_hev_dtd_item_t *hev_dtd_list;
	struct item_alloc_info_t hev_dtd_info;
	struct MHL3_hev_vic_item_t *hev_vic_list;
	struct item_alloc_info_t hev_vic_info;
	struct MHL3_3d_dtd_item_t *_3d_dtd_list;
	struct item_alloc_info_t _3d_dtd_info;
	struct MHL3_3d_vic_item_t *_3d_vic_list;
	struct item_alloc_info_t _3d_vic_info;
	struct edid_parse_data_t parse_data;
	uint8_t num_emsc_edid_extensions;
	uint8_t num_edid_emsc_blocks;
	uint8_t cur_edid_emsc_block;
	uint8_t *p_edid_emsc;
	uint8_t EDID_block_data[4 * EDID_BLOCK_SIZE];

};

struct SI_PACK_THIS_STRUCT si_incoming_timing_t {
	uint32_t calculated_pixel_clock;
	uint16_t h_total;
	uint16_t v_total;
	uint16_t columns;
	uint16_t rows;
	uint16_t field_rate;
	uint8_t mhl3_vic;
};

struct mhl_dev_context *si_edid_create_context(
	struct mhl_dev_context *dev_context,
	struct drv_hw_context *drv_context);

void *si_edid_get_processed_edid(struct edid_3d_data_t *mhl_edid_3d_data);
void si_edid_destroy_context(struct edid_3d_data_t *mhl_edid_3d_data);
void si_mhl_tx_initiate_edid_sequence(struct edid_3d_data_t *mhl_edid_3d_data);
void si_mhl_tx_process_3d_vic_burst(void *context,
	struct MHL2_video_format_data_t *pWriteBurstData);
void si_mhl_tx_process_3d_dtd_burst(void *context,
	struct MHL2_video_format_data_t *pWriteBurstData);
void si_mhl_tx_process_hev_vic_burst(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL3_hev_vic_data_t *p_write_burst_data);
void si_mhl_tx_process_hev_dtd_a_burst(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL3_hev_dtd_a_data_t *p_burst);
void si_mhl_tx_process_hev_dtd_b_burst(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL3_hev_dtd_b_data_t *p_burst);
uint32_t si_mhl_tx_find_timings_from_totals(
	struct edid_3d_data_t *mhl_edid_3d_data,
	struct si_incoming_timing_t *p_timing);
int si_edid_sink_is_hdmi(void *context);
int si_edid_quantization_range_selectable(void *context);
int si_edid_sink_supports_YCbCr422(void *context);
int si_edid_sink_supports_YCbCr444(void *context);
int si_edid_find_pixel_clock_from_HDMI_VIC(void *context, uint8_t vic);
int si_edid_find_pixel_clock_from_AVI_VIC(void *context, uint8_t vic);
uint8_t si_edid_map_hdmi_vic_to_mhl3_vic(void *context, uint8_t vic);

enum NumExtensions_e {
	ne_NO_HPD = -4,
	ne_BAD_DATA = -3,
	ne_BAD_CHECKSUM = ne_BAD_DATA,
	ne_BAD_HEADER = -2,
	ne_BAD_HEADER_OFFSET_BY_1 = -1,
	ne_SUCCESS = 0
};

#ifdef MANUAL_EDID_FETCH
bool si_mhl_tx_check_edid_header(struct edid_3d_data_t *mhl_edid_3d_data,
	void *pdata);
#endif
int si_mhl_tx_get_num_cea_861_extensions(void *context, uint8_t block_number);
int si_edid_read_done(void *context);
void si_edid_reset(struct edid_3d_data_t *mhl_edid_3d_data);
uint8_t qualify_pixel_clock_for_mhl(struct edid_3d_data_t *mhl_edid_3d_data,
	uint32_t pixel_clock_frequency, uint8_t bits_per_pixel);
uint8_t calculate_generic_checksum(void *infoFrameData, uint8_t checkSum,
	uint8_t length);
uint32_t si_edid_find_pixel_clock_from_HEV_DTD(
	struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL_high_low_t hev_fmt);
void si_mhl_tx_display_timing_enumeration_end(
	struct edid_3d_data_t *mhl_edid_3d_data);

int process_emsc_edid_sub_payload(struct edid_3d_data_t *edid_context,
	struct si_adopter_id_data *p_burst);
#endif
