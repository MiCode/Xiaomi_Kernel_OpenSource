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

#if !defined(SI_EDID_H)
#define SI_EDID_H

SI_PUSH_STRUCT_PACKING

struct SI_PACK_THIS_STRUCT two_bytes_t {
	unsigned char low;
	unsigned char high;
};

#define EDID_EXTENSION_TAG		0x02
#define EDID_EXTENSION_BLOCK_MAP	0xF0
#define EDID_REV_THREE			0x03
#define EDID_BLOCK_0			0x00
#define EDID_BLOCK_2_3			0x01

enum data_block_tag_code_e {
	DBTC_TERMINATOR,
	DBTC_AUDIO_DATA_BLOCK,
	DBTC_VIDEO_DATA_BLOCK,
	DBTC_VENDOR_SPECIFIC_DATA_BLOCK,
	DBTC_SPEAKER_ALLOCATION_DATA_BLOCK,
	DBTC_VESA_DTC_DATA_BLOCK,
	/* reserved = 6 */
	DBTC_USE_EXTENDED_TAG = 7
};

struct SI_PACK_THIS_STRUCT data_block_header_fields_t {
	uint8_t length_following_header:5;
	enum data_block_tag_code_e tag_code:3;
};

union SI_PACK_THIS_STRUCT data_block_header_byte_t {
	struct data_block_header_fields_t fields;
	uint8_t as_byte;
};

enum extended_tag_code_e {
	ETC_VIDEO_CAPABILITY_DATA_BLOCK,
	ETC_VENDOR_SPECIFIC_VIDEO_DATA_BLOCK,
	ETC_VESA_VIDEO_DISPLAY_DEVICE_INFORMATION_DATA_BLOCK,
	ETC_VESA_VIDEO_DATA_BLOCK,
	ETC_HDMI_VIDEO_DATA_BLOCK,
	ETC_COLORIMETRY_DATA_BLOCK,
	ETC_VIDEO_RELATED,
	ETC_CEA_MISC_AUDIO_FIELDS = 16,
	ETC_VENDOR_SPECIFIC_AUDIO_DATA_BLOCK,
	ETC_HDMI_AUDIO_DATA_BLOCK,
	ETC_AUDIO_RELATED,
	ETC_GENERAL = 32
};

struct SI_PACK_THIS_STRUCT extended_tag_code_t {
	enum extended_tag_code_e etc:8;
};

struct SI_PACK_THIS_STRUCT cea_short_descriptor_t {
	unsigned char VIC:7;
	unsigned char native:1;
};

#if 0
struct SI_PACK_THIS_STRUCT MHL_short_desc_t {
	cea_short_descriptor_t cea_short_desc;
	MHL2_video_descriptor_t mhl_vid_desc;
};
#endif

struct SI_PACK_THIS_STRUCT video_data_block_t {
	union data_block_header_byte_t header;
	struct cea_short_descriptor_t short_descriptors[1]; /*open ended */
};

enum AudioFormatCodes_e {
	/* reserved = 0 */
	afd_linear_PCM_IEC60958 = 1,
	afd_AC3,
	afd_MPEG1_layers_1_2,
	afd_MPEG1_layer_3,

	afdMPEG2_MultiChannel,
	afd_AAC,
	afd_DTS,
	afd_ATRAC,

	afd_one_bit_audio,
	afd_dolby_digital,
	afd_DTS_HD,
	afd_MAT_MLP,

	afd_DST,
	afd_WMA_Pro
	/* reserved = 15 */
};

struct SI_PACK_THIS_STRUCT CEA_short_audio_descriptor_t {
	unsigned char max_channels_minus_one:3;
	enum AudioFormatCodes_e audio_format_code:4;
	unsigned char F17:1;
	unsigned char freq_32_Khz:1;
	unsigned char freq_44_1_KHz:1;
	unsigned char freq_48_KHz:1;
	unsigned char freq_88_2_KHz:1;
	unsigned char freq_96_KHz:1;
	unsigned char freq_176_4_KHz:1;
	unsigned char freq_192_KHz:1;
	unsigned char F27:1;

	union {
		struct SI_PACK_THIS_STRUCT {
			unsigned res_16_bit:1;
			unsigned res_20_bit:1;
			unsigned res_24_bit:1;
			unsigned F33_37:5;
		} audio_code_1_LPCM;

		struct SI_PACK_THIS_STRUCT {
			uint8_t max_bit_rate_div_by_8_KHz;
		} audio_codes_2_8;

		struct SI_PACK_THIS_STRUCT {
			uint8_t default_zero;
		} audio_codes_9_15;
	} byte3;
};

struct SI_PACK_THIS_STRUCT audio_data_block_t {
	union data_block_header_byte_t header;
	/* open ended */
	struct CEA_short_audio_descriptor_t short_audio_descriptors[1];
};

struct SI_PACK_THIS_STRUCT speaker_allocation_flags_t {
	unsigned char spk_front_left_front_right:1;
	unsigned char spk_LFE:1;
	unsigned char spk_front_center:1;
	unsigned char spk_rear_left_rear_right:1;
	unsigned char spk_rear_center:1;
	unsigned char spk_front_left_center_front_right_center:1;
	unsigned char spk_rear_left_center_rear_right_center:1;
	unsigned char spk_reserved:1;
};

struct SI_PACK_THIS_STRUCT speaker_allocation_data_block_payload_t {
	struct speaker_allocation_flags_t speaker_alloc_flags;
	uint8_t reserved[2];
};

struct SI_PACK_THIS_STRUCT speaker_allocation_data_block_t {
	union data_block_header_byte_t header;
	struct speaker_allocation_data_block_payload_t payload;
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_BA_t {
	unsigned char B:4;
	unsigned char A:4;
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_DC_t {
	unsigned char D:4;
	unsigned char C:4;
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_Byte6_t {
	unsigned char DVI_dual:1;
	unsigned char reserved:2;
	unsigned char DC_Y444:1;
	unsigned char DC_30bit:1;
	unsigned char DC_36bit:1;
	unsigned char DC_48bit:1;
	unsigned char supports_AI:1;
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_byte8_t {
	unsigned char CNC0_adjacent_pixels_independent:1;
	unsigned char CNC1_specific_processing_still_pictures:1;
	unsigned char CNC2_specific_processing_cinema_content:1;
	unsigned char CNC3_specific_processing_low_AV_latency:1;
	unsigned char reserved:1;
	unsigned char HDMI_video_present:1;
	unsigned char I_latency_fields_present:1;
	unsigned char latency_fields_present:1;
};

enum image_size_e {
	imsz_NO_ADDITIONAL,
	imsz_ASPECT_RATIO_CORRECT_BUT_NO_GUARRANTEE_OF_CORRECT_SIZE,
	imsz_CORRECT_SIZES_ROUNDED_TO_NEAREST_1_CM,
	imsz_CORRECT_SIZES_DIVIDED_BY_5_ROUNDED_TO_NEAREST_5_CM
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_Byte13_t {
	unsigned char reserved:3;
	enum image_size_e image_size:2;
	unsigned char _3D_multi_present:2;
	unsigned char _3D_present:1;
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_Byte14_t {
	unsigned char HDMI_3D_len:5;
	unsigned char HDMI_VIC_len:3;
};

struct SI_PACK_THIS_STRUCT VSDB_byte_13_through_byte_15_t {
	struct HDMI_LLC_Byte13_t byte13;
	struct HDMI_LLC_Byte14_t byte14;
	uint8_t vicList[1];	/* variable length list base on HDMI_VIC_len */
};

struct SI_PACK_THIS_STRUCT VSDB_all_fields_b9_thru_b15_t {
	uint8_t video_latency;
	uint8_t audio_latency;
	uint8_t interlaced_video_latency;
	uint8_t interlaced_audio_latency;
	struct VSDB_byte_13_through_byte_15_t byte_13_through_byte_15;
	/* There must be no fields after here */
};

struct SI_PACK_THIS_STRUCT
	VSDB_all_fields_b9_thru_b15_sans_progressive_latency_t {
	uint8_t interlaced_video_latency;
	uint8_t interlaced_audio_latency;
	struct VSDB_byte_13_through_byte_15_t byte_13_through_byte_15;
	/* There must be no fields after here */
};

struct SI_PACK_THIS_STRUCT
	VSDB_all_fields_b9_thru_b15_sans_interlaced_latency_t {
	uint8_t video_latency;
	uint8_t audio_latency;
	struct VSDB_byte_13_through_byte_15_t byte_13_through_byte_15;
	/* There must be no fields after here */
};

struct SI_PACK_THIS_STRUCT
	VSDB_all_fields_b9_thru_b15_sans_all_latency_t {
	struct VSDB_byte_13_through_byte_15_t byte_13_through_byte_15;
	/* There must be no fields after here */
};

struct SI_PACK_THIS_STRUCT HDMI_LLC_vsdb_payload_t {
	struct HDMI_LLC_BA_t B_A;
	struct HDMI_LLC_DC_t D_C;
	struct HDMI_LLC_Byte6_t byte6;
	uint8_t maxTMDSclock;
	struct HDMI_LLC_byte8_t byte8;

	union {
	    struct VSDB_all_fields_b9_thru_b15_sans_all_latency_t
		vsdb_b9_to_b15_no_latency;
	    struct VSDB_all_fields_b9_thru_b15_sans_progressive_latency_t
		vsdb_b9_to_b15_no_p_latency;
	    struct VSDB_all_fields_b9_thru_b15_sans_interlaced_latency_t
		vsdb_b9_to_b15_no_i_latency;
	    struct VSDB_all_fields_b9_thru_b15_t vsdb_all_fields_b9_thru_b15;
	} vsdb_fields_b9_thru_b15;
	/* There must be no fields after here */
};

struct SI_PACK_THIS_STRUCT _3D_structure_all_15_8_t {
	uint8_t frame_packing:1;
	uint8_t reserved1:5;
	uint8_t top_bottom:1;
	uint8_t reserved2:1;
};

struct SI_PACK_THIS_STRUCT _3D_structure_all_7_0_t {
	uint8_t side_by_side:1;
	uint8_t reserved:7;
};

struct SI_PACK_THIS_STRUCT _3D_structure_all_t {
	struct _3D_structure_all_15_8_t _3D_structure_all_15_8;
	struct _3D_structure_all_7_0_t _3D_structure_all_7_0;
};

struct SI_PACK_THIS_STRUCT _3D_mask_t {
	uint8_t _3D_mask_15_8;
	uint8_t _3D_mask_7_0;
};

struct SI_PACK_THIS_STRUCT _2D_VIC_order_3D_structure_t {
	enum _3D_structure_e _3D_structure:4; /* definition from infoframe */
	unsigned _2D_VIC_order:4;
};

struct SI_PACK_THIS_STRUCT _3D_detail_t {
	unsigned char reserved:4;
	unsigned char _3D_detail:4;
};

struct SI_PACK_THIS_STRUCT _3D_structure_and_detail_entry_sans_byte1_t {
	struct _2D_VIC_order_3D_structure_t byte0;
	/*see HDMI 1.4 spec w.r.t. contents of 3D_structure_X */
};

struct SI_PACK_THIS_STRUCT _3D_structure_and_detail_entry_with_byte1_t {
	struct _2D_VIC_order_3D_structure_t byte0;
	struct _3D_detail_t byte1;
};

union _3D_structure_and_detail_entry_u {
	struct _3D_structure_and_detail_entry_sans_byte1_t sans_byte1;
	struct _3D_structure_and_detail_entry_with_byte1_t with_byte1;
};

struct SI_PACK_THIS_STRUCT HDMI_3D_sub_block_sans_all_AND_mask_t {
	union _3D_structure_and_detail_entry_u
		_3D_structure_and_detail_list[1];
};

struct SI_PACK_THIS_STRUCT HDMI_3D_sub_block_sans_mask_t {
	struct _3D_structure_all_t _3D_structure_all;
	union _3D_structure_and_detail_entry_u _3D_structure_and_detail_list[1];
};

struct SI_PACK_THIS_STRUCT HDMI_3D_sub_block_with_all_AND_mask_t {
	struct _3D_structure_all_t _3D_structure_all;
	struct _3D_mask_t _3D_mask;
	union _3D_structure_and_detail_entry_u
		_3D_structure_and_detail_list[1];
};

union HDMI_3D_sub_block_t {
	struct HDMI_3D_sub_block_sans_all_AND_mask_t
		HDMI_3D_sub_block_sans_all_AND_mask;
	struct HDMI_3D_sub_block_sans_mask_t HDMI_3D_sub_block_sans_mask;
	struct HDMI_3D_sub_block_with_all_AND_mask_t
		HDMI_3D_sub_block_with_all_AND_mask;
};

struct SI_PACK_THIS_STRUCT vsdb_t {
	union data_block_header_byte_t header;
	uint8_t IEEE_OUI[3];
	union {
		struct HDMI_LLC_vsdb_payload_t HDMI_LLC;
		uint8_t payload[1];	/* open ended */
	} payload_u;
};

enum colorimetry_xvYCC_e {
	xvYCC_601 = 1,
	xvYCC_709 = 2
};

struct SI_PACK_THIS_STRUCT colorimetry_xvYCC_t {
	enum colorimetry_xvYCC_e xvYCC:2;
	unsigned char reserved1:6;
};

struct SI_PACK_THIS_STRUCT colorimetry_meta_data_t {
	unsigned char meta_data:3;
	unsigned char reserved2:5;
};

struct SI_PACK_THIS_STRUCT colorimetry_data_payload_t {
	struct colorimetry_xvYCC_t ci_data;
	struct colorimetry_meta_data_t cm_meta_data;
};

struct SI_PACK_THIS_STRUCT colorimetry_data_block_t {
	union data_block_header_byte_t header;
	struct extended_tag_code_t extended_tag;
	struct colorimetry_data_payload_t payload;
};

enum CE_overscan_underscan_behavior_e {
	ceou_NEITHER,
	ceou_ALWAYS_OVERSCANNED,
	ceou_ALWAYS_UNDERSCANNED,
	ceou_BOTH
};

enum IT_overscan_underscan_behavior_e {
	itou_NEITHER,
	itou_ALWAYS_OVERSCANNED,
	itou_ALWAYS_UNDERSCANNED,
	itou_BOTH
};

enum PT_overscan_underscan_behavior_e {
	ptou_NEITHER,
	ptou_ALWAYS_OVERSCANNED,
	ptou_ALWAYS_UNDERSCANNED,
	ptou_BOTH,
};

struct SI_PACK_THIS_STRUCT video_capability_data_payload_t {
	enum CE_overscan_underscan_behavior_e S_CE:2;
	enum IT_overscan_underscan_behavior_e S_IT:2;
	enum PT_overscan_underscan_behavior_e S_PT:2;
	unsigned QS:1;
	unsigned quantization_range_selectable:1;
};

struct SI_PACK_THIS_STRUCT video_capability_data_block_t {
	union data_block_header_byte_t header;
	struct extended_tag_code_t extended_tag;
	struct video_capability_data_payload_t payload;
};

struct SI_PACK_THIS_STRUCT CEA_data_block_collection_t {
	union data_block_header_byte_t header;
	union {
		struct extended_tag_code_t extended_tag;
		struct cea_short_descriptor_t short_descriptor;
	} payload_u;
	/* open ended array of cea_short_descriptor_t starts here */
};

struct SI_PACK_THIS_STRUCT CEA_extension_version_1_t {
	uint8_t reservedMustBeZero;
	uint8_t reserved[123];
};

struct SI_PACK_THIS_STRUCT CEA_extension_2_3_misc_support_t {
	uint8_t total_number_native_dtds_in_entire_EDID:4;
	uint8_t YCrCb422_support:1;
	uint8_t YCrCb444_support:1;
	uint8_t basic_audio_support:1;
	uint8_t underscan_IT_formats_by_default:1;
};

struct SI_PACK_THIS_STRUCT CEA_extension_version_2_t {
	struct CEA_extension_2_3_misc_support_t misc_support;
	uint8_t reserved[123];
};

struct SI_PACK_THIS_STRUCT CEA_extension_version_3_t {
	struct CEA_extension_2_3_misc_support_t misc_support;
	union {
		uint8_t data_block_collection[123];
		uint8_t reserved[123];
	} Offset4_u;
};

struct SI_PACK_THIS_STRUCT block_map_t {
	uint8_t tag;
	uint8_t block_tags[126];
	uint8_t checksum;
};

struct SI_PACK_THIS_STRUCT CEA_extension_t {
	uint8_t tag;
	uint8_t revision;
	uint8_t byte_offset_to_18_byte_descriptors;
	union {
		struct CEA_extension_version_1_t version1;
		struct CEA_extension_version_2_t version2;
		struct CEA_extension_version_3_t version3;
	} version_u;
	uint8_t checksum;
};

struct SI_PACK_THIS_STRUCT detailed_timing_descriptor_t {
	uint8_t pixel_clock_low;
	uint8_t pixel_clock_high;
	uint8_t horz_active_7_0;
	uint8_t horz_blanking_7_0;
	struct SI_PACK_THIS_STRUCT {
		unsigned char horz_blanking_11_8:4;
		unsigned char horz_active_11_8:4;
	} horz_active_blanking_high;
	uint8_t vert_active_7_0;
	uint8_t vert_blanking_7_0;
	struct SI_PACK_THIS_STRUCT {
		unsigned char vert_blanking_11_8:4;
		unsigned char vert_active_11_8:4;
	} vert_active_blanking_high;
	uint8_t horz_sync_offset_7_0;
	uint8_t horz_sync_pulse_width7_0;
	struct SI_PACK_THIS_STRUCT {
		unsigned char vert_sync_pulse_width_3_0:4;
		unsigned char vert_sync_offset_3_0:4;
	} vert_sync_offset_width;
	struct SI_PACK_THIS_STRUCT {
		unsigned char vert_sync_pulse_width_5_4:2;
		unsigned char vert_sync_offset_5_4:2;
		unsigned char horz_sync_pulse_width_9_8:2;
		unsigned char horz_sync_offset_9_8:2;
	} hs_vs_offset_pulse_width;
	uint8_t horz_image_size_in_mm_7_0;
	uint8_t vert_image_size_in_mm_7_0;
	struct SI_PACK_THIS_STRUCT {
		unsigned char vert_image_size_in_mm_11_8:4;
		unsigned char horz_image_size_in_mm_11_8:4;
	} image_size_high;
	uint8_t horz_border_in_lines;
	uint8_t vert_border_in_pixels;
	struct SI_PACK_THIS_STRUCT {
		unsigned char stereo_bit_0:1;
		unsigned char sync_signal_options:2;
		unsigned char sync_signal_type:2;
		unsigned char stereo_bits_2_1:2;
		unsigned char interlaced:1;
	} flags;
};

struct SI_PACK_THIS_STRUCT red_green_bits_1_0_t {
	unsigned char green_y:2;
	unsigned char green_x:2;
	unsigned char red_y:2;
	unsigned char red_x:2;
};

struct SI_PACK_THIS_STRUCT blue_white_bits_1_0_t {
	unsigned char white_y:2;
	unsigned char white_x:2;
	unsigned char blue_y:2;
	unsigned char blue_x:2;
};

struct SI_PACK_THIS_STRUCT established_timings_I_t {
	unsigned char et800x600_60p:1;
	unsigned char et800x600_56p:1;
	unsigned char et640x480_75p:1;
	unsigned char et640x480_72p:1;
	unsigned char et640x480_67p:1;
	unsigned char et640x480_60p:1;
	unsigned char et720x400_88p:1;
	unsigned char et720x400_70p:1;
};

struct SI_PACK_THIS_STRUCT established_timings_II_t {
	unsigned char et1280x1024_75p:1;
	unsigned char et1024x768_75p:1;
	unsigned char et1024x768_70p:1;
	unsigned char et1024x768_60p:1;
	unsigned char et1024x768_87i:1;
	unsigned char et832x624_75p:1;
	unsigned char et800x600_75p:1;
	unsigned char et800x600_72p:1;
};

struct SI_PACK_THIS_STRUCT manufacturers_timings_t {
	unsigned char reserved:7;
	unsigned char et1152x870_75p:1;
};

enum image_aspect_ratio_e {
	iar_16_to_10,
	iar_4_to_3,
	iar_5_to_4,
	iar_16_to_9
};

struct SI_PACK_THIS_STRUCT standard_timing_t {
	unsigned char horz_pix_div_8_minus_31;
	unsigned char field_refresh_rate_minus_60:6;
	enum image_aspect_ratio_e image_aspect_ratio:2;
};

struct SI_PACK_THIS_STRUCT EDID_block0_t {
	unsigned char header_data[8];
	struct two_bytes_t id_manufacturer_name;
	struct two_bytes_t id_product_code;
	unsigned char serial_number[4];
	unsigned char week_of_manufacture;
	unsigned char year_of_manufacture;
	unsigned char EDID_version;
	unsigned char EDID_revision;
	unsigned char video_input_definition;
	unsigned char horz_screen_size_or_aspect_ratio;
	unsigned char vert_screen_size_or_aspect_ratio;
	unsigned char display_transfer_characteristic;
	unsigned char feature_support;
	struct red_green_bits_1_0_t red_green_bits_1_0;
	struct blue_white_bits_1_0_t blue_white_bits_1_0;
	unsigned char red_x;
	unsigned char red_y;
	unsigned char green_x;
	unsigned char green_y;
	unsigned char blue_x;
	unsigned char blue_y;
	unsigned char white_x;
	unsigned char white_y;
	struct established_timings_I_t established_timings_I;
	struct established_timings_II_t established_timings_II;
	struct manufacturers_timings_t manufacturers_timings;
	struct standard_timing_t standard_timings[8];
	struct detailed_timing_descriptor_t detailed_timing_descriptors[4];
	unsigned char extension_flag;
	unsigned char checksum;
};

struct SI_PACK_THIS_STRUCT monitor_name_t {
	uint8_t flag_required[2];
	uint8_t flag_reserved;
	uint8_t data_type_tag;
	uint8_t flag;
	uint8_t ascii_name[13];
};

struct SI_PACK_THIS_STRUCT monitor_range_limits_t {
	uint8_t flag_required[2];
	uint8_t flag_reserved;
	uint8_t data_type_tag;
	uint8_t flag;
	uint8_t min_vertical_rate_in_Hz;
	uint8_t max_vertical_rate_in_Hz;
	uint8_t min_horizontal_rate_in_KHz;
	uint8_t max_horizontal_rate_in_KHz;
	uint8_t max_pixel_clock_in_MHz_div_10;
	uint8_t tag_secondary_formula;
	uint8_t filler[7];
};

union _18_byte_descriptor_u {
	struct detailed_timing_descriptor_t dtd;
	struct monitor_name_t name;
	struct monitor_range_limits_t range_limits;
};

struct SI_PACK_THIS_STRUCT display_mode_3D_info_t {
	unsigned char dmi_3D_supported:1;
	unsigned char dmi_sufficient_bandwidth:1;
};

#ifdef ENABLE_EDID_DEBUG_PRINT
void dump_EDID_block_impl(const char *pszFunction, int iLineNum,
	uint8_t override, uint8_t *pData, uint16_t length);

void clear_EDID_block_impl(uint8_t *pData);

#define DUMP_EDID_BLOCK(override, pData, length) \
	dump_EDID_block_impl(__func__, __LINE__, override, (uint8_t *)pData, \
	length)
#define CLEAR_EDID_BLOCK(pData) clear_EDID_block_impl(pData)
#else
#define DUMP_EDID_BLOCK(override, pData, length)	/* nothing to do */
#define CLEAR_EDID_BLOCK(pData)	/* nothing to do */
#endif

enum EDID_error_codes {
	EDID_OK,
	EDID_INCORRECT_HEADER,
	EDID_CHECKSUM_ERROR,
	EDID_NO_861_EXTENSIONS,
	EDID_SHORT_DESCRIPTORS_OK,
	EDID_LONG_DESCRIPTORS_OK,
	EDID_EXT_TAG_ERROR,
	EDID_REV_ADDR_ERROR,
	EDID_V_DESCR_OVERFLOW,
	EDID_UNKNOWN_TAG_CODE,
	EDID_NO_DETAILED_DESCRIPTORS,
	EDID_DDC_BUS_REQ_FAILURE,
	EDID_DDC_BUS_RELEASE_FAILURE,
	EDID_READ_TIMEOUT
};

SI_POP_STRUCT_PACKING
#endif /* #if !defined(SI_EDID_H) */
