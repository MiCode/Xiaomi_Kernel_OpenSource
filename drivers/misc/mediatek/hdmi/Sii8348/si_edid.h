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

#if !defined(SI_EDID_H)
#define SI_EDID_H


SI_PUSH_STRUCT_PACKING //(

typedef struct SI_PACK_THIS_STRUCT _TwoBytes_t
{
    unsigned char low;
    unsigned char high;
}TwoBytes_t,*PTwoBytes_t;


#define EDID_EXTENSION_TAG  0x02
#define EDID_EXTENSION_BLOCK_MAP 0xF0
#define EDID_REV_THREE      0x03
//#define LONG_DESCR_LEN                  18
#define EDID_BLOCK_0        0x00
#define EDID_BLOCK_2_3      0x01
//#define EDID_BLOCK_0_OFFSET 0x0000

typedef enum
{
     DBTC_TERMINATOR                 	= 0
    ,DBTC_AUDIO_DATA_BLOCK             	= 1
    ,DBTC_VIDEO_DATA_BLOCK             	= 2
    ,DBTC_VENDOR_SPECIFIC_DATA_BLOCK    = 3
    ,DBTC_SPEAKER_ALLOCATION_DATA_BLOCK = 4
    ,DBTC_VESA_DTC_DATA_BLOCK           = 5
    //reserved                      	= 6
    ,DBTC_USE_EXTENDED_TAG            	= 7
}data_block_tag_code_e;
typedef struct SI_PACK_THIS_STRUCT _data_block_header_fields_t
{
    uint8_t length_following_header:5;
    data_block_tag_code_e tag_code:3;
}data_block_header_fields_t,*Pdata_block_header_fields_t;

typedef union SI_PACK_THIS_STRUCT _data_block_header_byte_t
{
	data_block_header_fields_t	fields;
	uint8_t						as_byte;
}data_block_header_byte_t,*Pdata_block_header_byte_t;

typedef enum
{
     ETC_VIDEO_CAPABILITY_DATA_BLOCK                    =  0
    ,ETC_VENDOR_SPECIFIC_VIDEO_DATA_BLOCK                =  1
    ,ETC_VESA_VIDEO_DISPLAY_DEVICE_INFORMATION_DATA_BLOCK  =  2
    ,ETC_VESA_VIDEO_DATA_BLOCK                          =  3
    ,ETC_HDMI_VIDEO_DATA_BLOCK                          =  4
    ,ETC_COLORIMETRY_DATA_BLOCK                        =  5
    ,ETC_VIDEO_RELATED                                =  6

    ,ETC_CEA_MISC_AUDIO_FIELDS                          = 16
    ,ETC_VENDOR_SPECIFIC_AUDIO_DATA_BLOCK                = 17
    ,ETC_HDMI_AUDIO_DATA_BLOCK                          = 18
    ,ETC_AUDIO_RELATED                               = 19

    ,ETC_GENERAL                                     = 32
}extended_tag_code_e;

typedef struct SI_PACK_THIS_STRUCT _extended_tag_code_t
{
    extended_tag_code_e etc:8;
}extended_tag_code_t,*Pextended_tag_code_t;

typedef struct SI_PACK_THIS_STRUCT _cea_short_descriptor_t
{
    unsigned char VIC:7;
    unsigned char native:1;

}cea_short_descriptor_t,*Pcea_short_descriptor_t;

typedef struct SI_PACK_THIS_STRUCT  _MHL_short_desc_t
{
    cea_short_descriptor_t cea_short_desc;
    MHL2_video_descriptor_t mhl_vid_desc;
}MHL_short_desc_t,*PMHL_short_desc_t;

typedef struct SI_PACK_THIS_STRUCT _video_data_block_t
{
    data_block_header_byte_t header;
    cea_short_descriptor_t short_descriptors[1];//open ended
}video_data_block_t,*p_video_data_block_t;

typedef enum
{
    // reserved             =  0
    afd_linear_PCM_IEC60958 =  1
    ,afd_AC3                =  2
    ,afd_MPEG1_layers_1_2   =  3
    ,afd_MPEG1_layer_3      =  4
    ,afdMPEG2_MultiChannel  =  5
    ,afd_AAC                =  6
    ,afd_DTS                =  7
    ,afd_ATRAC              =  8
    ,afd_one_bit_audio      =  9
    ,afd_dolby_digital      = 10
    ,afd_DTS_HD             = 11
    ,afd_MAT_MLP            = 12
    ,afd_DST                = 13
    ,afd_WMA_Pro            = 14
    //reserved              = 15
}AudioFormatCodes_e;

typedef struct SI_PACK_THIS_STRUCT _CEA_short_audio_descriptor_t
{
    unsigned char       max_channels_minus_one :3;
    AudioFormatCodes_e  audio_format_code     :4;
    unsigned char       F17                 :1;

    unsigned char freq_32_Khz   :1;
    unsigned char freq_44_1_KHz :1;
    unsigned char freq_48_KHz   :1;
    unsigned char freq_88_2_KHz :1;
    unsigned char freq_96_KHz   :1;
    unsigned char freq_176_4_KHz:1;
    unsigned char freq_192_KHz  :1;
    unsigned char F27         :1;

    union
    {
        struct SI_PACK_THIS_STRUCT 
        {
            unsigned res_16_bit:1;
            unsigned res_20_bit:1;
            unsigned res_24_bit:1;
            unsigned F33_37:5;
        }audio_code_1_LPCM;
        struct SI_PACK_THIS_STRUCT 
        {
            uint8_t max_bit_rate_div_by_8_KHz;
        }audio_codes_2_8;
        struct SI_PACK_THIS_STRUCT 
        {
            uint8_t default_zero;
        }audio_codes_9_15;
    }byte3;
}CEA_short_audio_descriptor_t,*PCEA_short_audio_descriptor_t;

typedef struct SI_PACK_THIS_STRUCT _audio_data_block_t
{
    data_block_header_byte_t header;
    CEA_short_audio_descriptor_t  short_audio_descriptors[1]; // open ended
}audio_data_block_t,*Paudio_data_block_t;

typedef struct SI_PACK_THIS_STRUCT _speaker_allocation_flags_t
{
    unsigned char   spk_front_left_front_right:1;
    unsigned char   spk_LFE:1;
    unsigned char   spk_front_center:1;
    unsigned char   spk_rear_left_rear_right:1;
    unsigned char   spk_rear_center:1;
    unsigned char   spk_front_left_center_front_right_center:1;
    unsigned char   spk_rear_left_center_rear_right_center:1;
    unsigned char   spk_reserved:1;
}speaker_allocation_flags_t,*Pspeaker_allocation_flags_t;
typedef struct SI_PACK_THIS_STRUCT _speaker_allocation_data_block_payload_t
{
    speaker_allocation_flags_t speaker_alloc_flags;

    uint8_t         reserved[2];
}speaker_allocation_data_block_payload_t,*Pspeaker_allocation_data_block_payload_t;

typedef struct SI_PACK_THIS_STRUCT _speaker_allocation_data_block_t
{
    data_block_header_byte_t header;
    speaker_allocation_data_block_payload_t payload;

}speaker_allocation_data_block_t,*Pspeaker_allocation_data_block_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_BA_t
{
    unsigned char B:4;
    unsigned char A:4;
}HDMI_LLC_BA_t,*PHDMI_LLC_BA_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_DC_t
{
    unsigned char D:4;
    unsigned char C:4;
}HDMI_LLC_DC_t,*PHDMI_LLC_DC_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_Byte6_t
{
    unsigned char DVI_dual      :1;
    unsigned char reserved      :2;
    unsigned char DC_Y444       :1;
    unsigned char DC_30bit      :1;
    unsigned char DC_36bit      :1;
    unsigned char DC_48bit      :1;
    unsigned char supports_AI   :1;
}HDMI_LLC_Byte6_t,*PHDMI_LLC_Byte6_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_byte8_t
{
    unsigned char CNC0_adjacent_pixels_independent        :1;
    unsigned char CNC1_specific_processing_still_pictures  :1;
    unsigned char CNC2_specific_processing_cinema_content  :1;
    unsigned char CNC3_specific_processing_low_AV_latency   :1;
    unsigned char reserved                              :1;
    unsigned char HDMI_video_present                     :1;
    unsigned char I_latency_fields_present                :1;
    unsigned char latency_fields_present                  :1;
}HDMI_LLC_byte8_t,*PHDMI_LLC_byte8_t;

typedef enum
{
    imsz_NO_ADDITIONAL                                    			= 0
    ,imsz_ASPECT_RATIO_CORRECT_BUT_NO_GUARRANTEE_OF_CORRECT_SIZE 	= 1
    ,imsz_CORRECT_SIZES_ROUNDED_TO_NEAREST_1_CM                		= 2
    ,imsz_CORRECT_SIZES_DIVIDED_BY_5_ROUNDED_TO_NEAREST_5_CM      	= 3
}image_size_e;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_Byte13_t
{
    unsigned char reserved          :3;
    image_size_e image_size          :2;
    unsigned char _3D_multi_present :2;
    unsigned char _3D_present       :1;
}HDMI_LLC_Byte13_t,*PHDMI_LLC_Byte13_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_Byte14_t
{
    unsigned char HDMI_3D_len :5;
    unsigned char HDMI_VIC_len:3;
}HDMI_LLC_Byte14_t,*PHDMI_LLC_Byte14_t;

typedef struct SI_PACK_THIS_STRUCT _VSDB_byte_13_through_byte_15_t
{
    HDMI_LLC_Byte13_t    byte13;
    HDMI_LLC_Byte14_t    byte14;
    uint8_t vicList[1]; // variable length list base on HDMI_VIC_len
}VSDB_byte_13_through_byte_15_t,*PVSDB_byte_13_through_byte_15_t;

typedef struct SI_PACK_THIS_STRUCT _VSDB_all_fields_byte_9_through_byte15_t
{
    uint8_t video_latency;
    uint8_t audio_latency;
    uint8_t interlaced_video_latency;
    uint8_t interlaced_audio_latency;
    VSDB_byte_13_through_byte_15_t   byte_13_through_byte_15;
    // There must be no fields after here
}VSDB_all_fields_byte_9_through_byte15_t,*PVSDB_all_fields_byte_9_through_byte15_t;

typedef struct SI_PACK_THIS_STRUCT _VSDB_all_fields_byte_9_through_byte_15_sans_progressive_latency_t
{
    uint8_t interlaced_video_latency;
    uint8_t interlaced_audio_latency;
    VSDB_byte_13_through_byte_15_t   byte_13_through_byte_15;
    // There must be no fields after here
}VSDB_all_fields_byte_9_through_byte_15_sans_progressive_latency_t,*PVSDB_all_fields_byte_9_through_byte_15_sans_progressive_latency_t;

typedef struct SI_PACK_THIS_STRUCT _VSDB_all_fields_byte_9_through_byte_15_sans_interlaced_latency_t
{
    uint8_t video_latency;
    uint8_t audio_latency;
    VSDB_byte_13_through_byte_15_t   byte_13_through_byte_15;
    // There must be no fields after here
}VSDB_all_fields_byte_9_through_byte_15_sans_interlaced_latency_t,*PVSDB_all_fields_byte_9_through_byte_15_sans_interlaced_latency_t;

typedef struct SI_PACK_THIS_STRUCT _VSDB_all_fields_byte_9_through_byte_15_sans_all_latency_t
{
    VSDB_byte_13_through_byte_15_t   byte_13_through_byte_15;
    // There must be no fields after here
}VSDB_all_fields_byte_9_through_byte_15_sans_all_latency_t,*PVSDB_all_fields_byte_9_through_byte_15_sans_all_latency_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_LLC_vsdb_payload_t
{
    HDMI_LLC_BA_t B_A;
    HDMI_LLC_DC_t D_C;
    HDMI_LLC_Byte6_t byte6;
    uint8_t maxTMDSclock;
    HDMI_LLC_byte8_t byte8;
    union
    {
        VSDB_all_fields_byte_9_through_byte_15_sans_all_latency_t         vsdb_all_fields_byte_9_through_byte_15_sans_all_latency;
        VSDB_all_fields_byte_9_through_byte_15_sans_progressive_latency_t vsdb_all_fields_byte_9_through_byte_15_sans_progressive_latency;
        VSDB_all_fields_byte_9_through_byte_15_sans_interlaced_latency_t  vsdb_all_fields_byte_9_through_byte_15_sans_interlaced_latency;
        VSDB_all_fields_byte_9_through_byte15_t                       vsdb_all_fields_byte_9_through_byte_15;
    }vsdb_fields_byte_9_through_byte_15;
    // There must be no fields after here
}HDMI_LLC_vsdb_payload_t,*PHDMI_LLC_vsdb_payload_t;

typedef struct SI_PACK_THIS_STRUCT st_3D_structure_all_15_8_t
{
    uint8_t frame_packing    : 1;
    uint8_t reserved1       : 5;
    uint8_t top_bottom       : 1;
    uint8_t reserved2       : 1;
}_3D_structure_all_15_8_t,*P_3D_structure_all_15_8_t;

typedef struct SI_PACK_THIS_STRUCT st_3D_structure_all_7_0_t
{
    uint8_t side_by_side      : 1;
    uint8_t reserved        : 7;
}_3D_structure_all_7_0_t,*P_3D_structure_all_7_0_t;


typedef struct SI_PACK_THIS_STRUCT tag_3D_structure_all_t
{
    _3D_structure_all_15_8_t _3D_structure_all_15_8;
    _3D_structure_all_7_0_t _3D_structure_all_7_0;
}_3D_structure_all_t,*P_3D_structure_all_t;


typedef struct SI_PACK_THIS_STRUCT tag_3D_mask_t
{
    uint8_t _3D_mask_15_8;
    uint8_t _3D_mask_7_0;
}_3D_mask_t,*P_3D_mask_t;



typedef struct SI_PACK_THIS_STRUCT tag_2D_VIC_order_3D_structure_t
{
    _3D_structure_e _3D_structure:4;     // definition from info frame
    unsigned    _2D_VIC_order:4;
}_2D_VIC_order_3D_structure_t,*P_2D_VIC_order_3D_structure_t;

typedef struct SI_PACK_THIS_STRUCT tag_3D_detail_t
{
    unsigned char reserved    :4;
    unsigned char _3D_detail  :4;
}_3D_detail_t,*P_3D_detail_t;

typedef struct SI_PACK_THIS_STRUCT tag_3D_structure_and_detail_entry_sans_byte1_t
{
    _2D_VIC_order_3D_structure_t    byte0;
/*see HDMI 1.4 spec w.r.t. contents of 3D_structure_X */
}_3D_structure_and_detail_entry_sans_byte1_t,*P_3D_structure_and_detail_entry_sans_byte1_t;

typedef struct SI_PACK_THIS_STRUCT tag_3D_structure_and_detail_entry_with_byte1_t
{
    _2D_VIC_order_3D_structure_t    byte0;
    _3D_detail_t                    byte1;
}_3D_structure_and_detail_entry_with_byte1_t,*P_3D_structure_and_detail_entry_with_byte1_t;

typedef union tag_3D_structure_and_detail_entry_u
{
    _3D_structure_and_detail_entry_sans_byte1_t   sans_byte1;
    _3D_structure_and_detail_entry_with_byte1_t   with_byte1;
}_3D_structure_and_detail_entry_u,*P_3D_structure_and_detail_entry_u;

typedef struct SI_PACK_THIS_STRUCT _HDMI_3D_sub_block_sans_all_AND_mask_t
{
    _3D_structure_and_detail_entry_u    _3D_structure_and_detail_list[1];
}HDMI_3D_sub_block_sans_all_AND_mask_t,*PHDMI_3D_sub_block_sans_all_AND_mask_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_3D_sub_block_sans_mask_t
{
    _3D_structure_all_t  _3D_structure_all;
    _3D_structure_and_detail_entry_u    _3D_structure_and_detail_list[1];
}HDMI_3D_sub_block_sans_mask_t,*PHDMI_3D_sub_block_sans_mask_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_3D_sub_block_with_all_AND_mask_t
{
    _3D_structure_all_t  _3D_structure_all;
    _3D_mask_t          _3D_mask;
    _3D_structure_and_detail_entry_u    _3D_structure_and_detail_list[1];
}HDMI_3D_sub_block_with_all_AND_mask_t,*PHDMI_3D_sub_block_with_all_AND_mask_t;

typedef union
{
    HDMI_3D_sub_block_sans_all_AND_mask_t   hDMI_3D_sub_block_sans_all_AND_mask;
    HDMI_3D_sub_block_sans_mask_t         HDMI_3D_sub_block_sans_mask;
    HDMI_3D_sub_block_with_all_AND_mask_t   HDMI_3D_sub_block_with_all_AND_mask;
}HDMI_3D_sub_block_t,*PHDMI_3D_sub_block_t;

typedef struct SI_PACK_THIS_STRUCT _vsdb_t
{
    data_block_header_byte_t header;
    uint8_t IEEE_OUI[3];
    union
    {
        HDMI_LLC_vsdb_payload_t HDMI_LLC;
        uint8_t payload[1]; // open ended
    }payload_u;
}vsdb_t,*P_vsdb_t;

typedef enum
{
     xvYCC_601   = 1
    ,xvYCC_709   = 2
}colorimetry_xvYCC_e;

typedef struct SI_PACK_THIS_STRUCT _colorimetry_xvYCC_t
{
    colorimetry_xvYCC_e     xvYCC       :2;
    unsigned char           reserved1   :6;
}colorimetry_xvYCC_t,*Pcolorimetry_xvYCC_t;

typedef struct SI_PACK_THIS_STRUCT _colorimetry_meta_data_t
{
    unsigned char           meta_data    :3;
    unsigned char           reserved2   :5;
}colorimetry_meta_data_t,*Pcolorimetry_meta_data_t;

typedef struct SI_PACK_THIS_STRUCT _colorimetry_data_payload_t
{
    colorimetry_xvYCC_t     ci_data;
    colorimetry_meta_data_t  cm_meta_data;
}colorimetry_data_payload_t,*Pcolorimetry_data_payload_t;
typedef struct SI_PACK_THIS_STRUCT _colorimetry_data_block_t
{
    data_block_header_byte_t   header;
    extended_tag_code_t       extended_tag;
    colorimetry_data_payload_t payload;

}colorimetry_data_block_t,*Pcolorimetry_data_block_t;

typedef enum
{
     ceou_NEITHER                    = 0
    ,ceou_ALWAYS_OVERSCANNED         = 1
    ,ceou_ALWAYS_UNDERSCANNED        = 2
    ,ceou_BOTH                       = 3
}CE_overscan_underscan_behavior_e;

typedef enum
{
     itou_NEITHER                   = 0
    ,itou_ALWAYS_OVERSCANNED        = 1
    ,itou_ALWAYS_UNDERSCANNED       = 2
    ,itou_BOTH                      = 3
}IT_overscan_underscan_behavior_e;

typedef enum
{
     ptou_NEITHER                    = 0
    ,ptou_ALWAYS_OVERSCANNED          = 1
    ,ptou_ALWAYS_UNDERSCANNED         = 2
    ,ptou_BOTH                       = 3
}PT_overscan_underscan_behavior_e;

typedef struct SI_PACK_THIS_STRUCT _video_capability_data_payload_t
{
    CE_overscan_underscan_behavior_e S_CE     :2;
    IT_overscan_underscan_behavior_e S_IT     :2;
    PT_overscan_underscan_behavior_e S_PT     :2;
    unsigned QS       :1;
    unsigned quantization_range_selectable:1;
}video_capability_data_payload_t,*Pvideo_capability_data_payload_t;

typedef struct SI_PACK_THIS_STRUCT _video_capability_data_block_t
{
    data_block_header_byte_t   header;
    extended_tag_code_t       extended_tag;
    video_capability_data_payload_t payload;

}video_capability_data_block_t,*Pvideo_capability_data_block_t;

typedef struct SI_PACK_THIS_STRUCT _CEA_data_block_collection_t
{
    data_block_header_byte_t header;

    union
    {
        extended_tag_code_t extended_tag;
        cea_short_descriptor_t short_descriptor;
    }payload_u;
    // open ended array of cea_short_descriptor_t starts here
}CEA_data_block_collection_t,*PCEA_data_block_collection_t;

typedef struct SI_PACK_THIS_STRUCT _CEA_extension_version_1_t
{
    uint8_t reservedMustBeZero;
    uint8_t reserved[123];
}CEA_extension_version_1_t,*PCEA_extension_version_1_t;

typedef struct SI_PACK_THIS_STRUCT _CEA_extension_2_3_misc_support_t
{
    uint8_t total_number_detailed_timing_descriptors_in_entire_EDID:4;
    uint8_t YCrCb422_support:1;
    uint8_t YCrCb444_support:1;
    uint8_t basic_audio_support:1;
    uint8_t underscan_IT_formats_by_default:1;
}CEA_extension_2_3_misc_support_t,*PCEA_extension_2_3_misc_support_t;
typedef struct SI_PACK_THIS_STRUCT _CEA_extension_version_2_t
{
    CEA_extension_2_3_misc_support_t misc_support;

    uint8_t reserved[123];
}CEA_extension_version_2_t,*PCEA_extension_version_2_t;

typedef struct SI_PACK_THIS_STRUCT _CEA_extension_version_3_t
{
    CEA_extension_2_3_misc_support_t misc_support;
    union
    {
        uint8_t data_block_collection[123];
        uint8_t reserved[123];
    }Offset4_u;
}CEA_extension_version_3_t,*PCEA_extension_version_3_t;

typedef struct SI_PACK_THIS_STRUCT _block_map_t
{
	uint8_t tag;
	uint8_t block_tags[126];
	uint8_t checksum;
}block_map_t, *Pblock_map_t;

typedef struct SI_PACK_THIS_STRUCT _CEA_extension_t
{
    uint8_t tag;
    uint8_t revision;
    uint8_t byte_offset_to_18_byte_descriptors;
    union
    {
        CEA_extension_version_1_t      version1;
        CEA_extension_version_2_t      version2;
        CEA_extension_version_3_t      version3;
    }version_u;
    uint8_t checksum;
}CEA_extension_t,*PCEA_extension_t;

typedef struct SI_PACK_THIS_STRUCT _detailed_timing_descriptor_t
{
    uint8_t pixel_clock_low;
    uint8_t pixel_clock_high;
    uint8_t horz_active_7_0;
    uint8_t horz_blanking_7_0;
    struct SI_PACK_THIS_STRUCT
    {
        unsigned char horz_blanking_11_8    :4;
        unsigned char horz_active_11_8      :4;
    }horz_active_blanking_high;
    uint8_t vert_active_7_0;
    uint8_t vert_blanking_7_0;
    struct SI_PACK_THIS_STRUCT
    {
        unsigned char vert_blanking_11_8    :4;
        unsigned char vert_active_11_8      :4;
    }vert_active_blanking_high;
    uint8_t horz_sync_offset_7_0;
    uint8_t horz_sync_pulse_width7_0;
    struct SI_PACK_THIS_STRUCT
    {
        unsigned char vert_sync_pulse_width_3_0 :4;
        unsigned char vert_sync_offset_3_0     :4;
    }vert_sync_offset_width;
    struct SI_PACK_THIS_STRUCT
    {
        unsigned char vert_sync_pulse_width_5_4 :2;
        unsigned char vert_sync_offset_5_4     :2;
        unsigned char horz_sync_pulse_width_9_8 :2;
        unsigned char horzSyncOffset9_8     :2;
    }hs_offset_hs_pulse_width_vs_offset_vs_pulse_width;
    uint8_t horz_image_size_in_mm_7_0;
    uint8_t vert_image_size_in_mm_7_0;
    struct SI_PACK_THIS_STRUCT
    {
        unsigned char vert_image_size_in_mm_11_8  :4;
        unsigned char horz_image_size_in_mm_11_8  :4;
    }image_size_high;
    uint8_t horz_border_in_lines;
    uint8_t vert_border_in_pixels;
    struct SI_PACK_THIS_STRUCT
    {
        unsigned char stereo_bit_0            :1;
        unsigned char sync_signal_options     :2;
        unsigned char sync_signal_type        :2;
        unsigned char stereo_bits_2_1         :2;
        unsigned char interlaced            :1;
    }flags;
}detailed_timing_descriptor_t,*Pdetailed_timing_descriptor_t;

typedef struct SI_PACK_THIS_STRUCT _red_green_bits_1_0_t
{
    unsigned char   green_y :2;
    unsigned char   green_x :2;
    unsigned char   red_y   :2;
    unsigned char   red_x   :2;
}red_green_bits_1_0_t,*Pred_green_bits_1_0_t;

typedef struct SI_PACK_THIS_STRUCT _blue_white_bits_1_0_t
{
    unsigned char   white_y :2;
    unsigned char   white_x :2;
    unsigned char   blue_y  :2;
    unsigned char   blue_x  :2;

}blue_white_bits_1_0_t,*Pblue_white_bits_1_0_t;


typedef struct SI_PACK_THIS_STRUCT _established_timings_I_t
{
    unsigned char et800x600_60Hz  :1;
    unsigned char et800x600_56Hz  :1;
    unsigned char et640x480_75Hz  :1;
    unsigned char et640x480_72Hz  :1;
    unsigned char et640x480_67Hz  :1;
    unsigned char et640x480_60Hz  :1;
    unsigned char et720x400_88Hz  :1;
    unsigned char et720x400_70Hz  :1;
}established_timings_I_t,*Pestablished_timings_I_t;

typedef struct SI_PACK_THIS_STRUCT _established_timings_II_t
{
    unsigned char et1280x1024_75Hz:1;
    unsigned char et1024x768_75Hz :1;
    unsigned char et1024x768_70Hz :1;
    unsigned char et1024x768_60Hz :1;
    unsigned char et1024x768_87HzI:1;
    unsigned char et832x624_75Hz  :1;
    unsigned char et800x600_75Hz  :1;
    unsigned char et800x600_72Hz  :1;
}established_timings_II_t,*Pestablished_timings_II_t;

typedef struct SI_PACK_THIS_STRUCT _manufacturers_timings_t
{
    unsigned char   reserved        :7;
    unsigned char   et1152x870_75Hz :1;
}manufacturers_timings_t,*Pmanufacturers_timings_t;

typedef enum
{
    iar_16_to_10   = 0
    ,iar_4_to_3    = 1
    ,iar_5_to_4    = 2
    ,iar_16_to_9   = 3
}image_aspect_ratio_e;

typedef struct SI_PACK_THIS_STRUCT _standard_timing_t
{
    unsigned char   horz_pix_div_8_minus_31;
    unsigned char   field_refresh_rate_minus_60:6;
    image_aspect_ratio_e image_aspect_ratio    :2;
}standard_timing_t,*Pstandard_timing_t;

typedef struct SI_PACK_THIS_STRUCT _EDID_block0_t
{
    unsigned char               header_data[8];
    TwoBytes_t                  id_manufacturer_name;
    TwoBytes_t                  id_product_code;
    unsigned char               serial_number[4];
    unsigned char               week_of_manufacture;
    unsigned char               year_of_manufacture;
    unsigned char               EDID_version;
    unsigned char               EDID_revision;
    unsigned char               video_input_definition;
    unsigned char               horz_screen_size_or_aspect_ratio;
    unsigned char               vert_screen_size_or_aspect_ratio;
    unsigned char               display_transfer_characteristic;
    unsigned char               feature_support;
    red_green_bits_1_0_t           red_green_bits_1_0;
    blue_white_bits_1_0_t          blue_white_bits_1_0;
    unsigned char               red_x;
    unsigned char               red_y;
    unsigned char               green_x;
    unsigned char               green_y;
    unsigned char               blue_x;
    unsigned char               blue_y;
    unsigned char               white_x;
    unsigned char               white_y;
    established_timings_I_t       established_timings_I;
    established_timings_II_t      established_timings_II;
    manufacturers_timings_t      manufacturers_timings;
    standard_timing_t            standard_timings[8];
    detailed_timing_descriptor_t  detailed_timing_descriptors[4];
    unsigned char               extension_flag;
    unsigned char               checksum;

}EDID_block0_t,*PEDID_block0_t;

typedef struct SI_PACK_THIS_STRUCT _monitor_name_t
{
    uint8_t flag_required[2];
    uint8_t flag_reserved;
    uint8_t data_type_tag;
    uint8_t flag;
    uint8_t ascii_name[13];


}monitor_name_t,*Pmonitor_name_t;

typedef struct SI_PACK_THIS_STRUCT _monitor_range_limits_t
{

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
}monitor_range_limits_t,*Pmonitor_range_limits_t;


typedef union tag_18_byte_descriptor_u
{
    detailed_timing_descriptor_t  dtd;
    monitor_name_t               name;
    monitor_range_limits_t        range_limits;
}_18_byte_descriptor_u,*P_18_byte_descriptor_u;


typedef struct SI_PACK_THIS_STRUCT _display_mode_3D_info_t
{
    unsigned char dmi_3D_supported:1;
    unsigned char dmi_sufficient_bandwidth:1;
}display_mode_3D_info_t,*Pdisplay_mode_3D_info_t;

typedef enum
{
      vif_single_frame_rate = 0x00
     ,vif_dual_frame_rate   = 0x01

}VIC_info_flags_e;

typedef enum
{
     vsm_progressive  = 0
    ,vsm_interlaced   = 1
}VIC_scan_mode_e;

typedef enum
{
     par_1_to_1
    ,par_16_to_15
    ,par_16_to_27
    ,par_16_to_45
    ,par_16_to_45_160_to_45
    ,par_1_to_15_10_to_15
    ,par_1_to_9_10_to_9
    ,par_2_to_15_20_to_15
    ,par_2_to_9
    ,par_2_to_9_20_to_9
    ,par_32_to_27
    ,par_32_to_45
    ,par_4_to_27_40_to_27
    ,par_4_to_9
    ,par_4_to_15
    ,par_64_to_45
    ,par_8_to_15
    ,par_8_to_27
    ,par_8_to_27_80_to_27
    ,par_8_to_45_80_to_45
    ,par_8_to_9
}pixel_aspect_ratio_e;



typedef struct SI_PACK_THIS_STRUCT _VIC_info_fields_t
{
    image_aspect_ratio_e  image_aspect_ratio        :2;
    VIC_scan_mode_e 		interlaced              :1;
    pixel_aspect_ratio_e 	pixel_aspect_ratio        :5;

    VIC_info_flags_e  frame_rate_info   :1;
    uint8_t         clocks_per_pixel_shift_count:2;
    uint8_t         field2_v_blank    :2;
    uint8_t         reserved        :3;
}VIC_info_fields_t,*PVIC_info_fields_t;



typedef struct SI_PACK_THIS_STRUCT _VIC_info_t
{
    uint16_t columns;
    uint16_t rows;
    uint16_t h_blank_in_pixels;
    uint16_t v_blank_in_pixels;
    uint32_t field_rate_in_milliHz;
    VIC_info_fields_t fields;
//    uint16_t pixClockDiv10000;
}VIC_info_t,*PVIC_info_t;

typedef struct SI_PACK_THIS_STRUCT _HDMI_VIC_info_t
{
    uint16_t columns;
    uint16_t rows;
    uint32_t field_rate_0_in_milliHz,field_rate_1_in_milliHz;
    uint32_t pixel_clock_0,pixel_clock_1;
}HDMI_VIC_info_t,*PHDMI_VIC_info_t;


#if 1//def ENABLE_EDID_DEBUG_PRINT //(
void dump_EDID_block_impl(const char *pszFunction, int iLineNum,uint8_t override,uint8_t *pData,uint16_t length);
void clear_EDID_block_impl(uint8_t *pData);
#define DUMP_EDID_BLOCK(override,pData,length) dump_EDID_block_impl(__FUNCTION__,__LINE__,override,(uint8_t *)pData,length);
#define CLEAR_EDID_BLOCK(pData) clear_EDID_block_impl(pData);
#else //)(
#define DUMP_EDID_BLOCK(override,pData,length) /* nothing to do */
#define CLEAR_EDID_BLOCK(pData) /* nothing to do */
#endif //)

enum EDID_error_codes
{
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
SI_POP_STRUCT_PACKING //)

#endif /* #if !defined(SI_EDID_H) */
