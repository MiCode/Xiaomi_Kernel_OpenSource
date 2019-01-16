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

#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "sii_hal.h"
#include "si_fw_macros.h"
#include "si_mhl_defs.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_8348_internal_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "platform.h"
#include "hdmi_drv.h"
#include "si_timing_defs.h"

#define SET_3D_FLAG(context,x)							\
{														\
	context->parse_data.flags.x = 1;			\
	MHL_TX_EDID_INFO(context->dev_context,				\
					"set %s\n",#x);	\
}
#define CLR_3D_FLAG(context,x)							\
{														\
	context->parse_data.flags.x = 0;			\
	MHL_TX_EDID_INFO(context->dev_context,				\
					"clr %s\n",#x);	\
}
#define TEST_3D_FLAG(context,x) (context->parse_data.flags.x)

typedef struct _timing_mode_from_data_sheet_t
{
	uint16_t h_total;
	uint16_t v_total;
	uint32_t pixel_clock;
	struct
	{
	uint8_t interlaced:1;
	uint8_t reserved:7;
	}flags;
	char	*description;
}timing_mode_from_data_sheet_t,*Ptiming_mode_from_data_sheet_t;


timing_mode_from_data_sheet_t timing_modes_from_data_sheet[]=
{
	 { 800, 525, 25175000,{0,0},"VGA"}
	,{1088, 517, 33750000,{0,0},"WVGA"}
	,{1056, 628, 40000000,{0,0},"SVGA"}
	,{1344, 806, 65000000,{0,0},"XGA"}
	,{1716, 262, 27000000,{1,0},"480i"}/* DS has VTOTAL for progressive */
	,{1728, 312, 27000000,{1,0},"576i"}/* DS has VTOTAL for progressive */
	,{ 858, 525, 27000000,{0,0},"480p"}
	,{ 864, 625, 27000000,{0,0},"576p"}
	,{1650, 750, 74250000,{0,0},"720p"}
	,{2200, 562, 74250000,{1,0},"1080i"}/* DS has VTOTAL for progressive */
	,{2750,1125, 74250000,{0,0},"1080p,24/30"}
	,{2640,1125,148500000,{0,0},"1080p50"}
	,{2200,1125,148500000,{0,0},"1080p60"}
};

void display_timing_enumeration_begin(edid_3d_data_p mhl_edid_3d_data)
{
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"Begin MHL display timing enumeration\n");
}

void display_timing_enumeration_callback(edid_3d_data_p mhl_edid_3d_data,
								uint16_t columns, uint16_t rows,
								uint8_t bits_per_pixel,
								uint32_t vertical_refresh_rate_in_milliHz,
								MHL2_video_descriptor_t mhl2_video_descriptor)
{

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
		"%4d x %4d, %2d bpp at %u Hz 3D - %16s %16s %16s\n\n"
		,columns
		,rows
		,(uint16_t)bits_per_pixel
		,vertical_refresh_rate_in_milliHz
		,mhl2_video_descriptor.left_right      ?"Left/Right"      :""
		,mhl2_video_descriptor.top_bottom      ?"Top/Bottom"      :""
		,mhl2_video_descriptor.frame_sequential?"Frame Sequential":""
		);
}

void display_timing_enumeration_end(edid_3d_data_p mhl_edid_3d_data)
{
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"End MHL display timing enumeration\n");
}

uint32_t si_mhl_tx_find_timings_from_totals( edid_3d_data_p mhl_edid_3d_data)
{
	uint32_t ret_val=0;
	uint8_t i;
	uint16_t h_total, v_total;
	/* Measure the HTOTAL and VTOTAL and look them up in a table */
	h_total = si_mhl_tx_drv_get_incoming_horizontal_total(mhl_edid_3d_data->drv_context);
	v_total = si_mhl_tx_drv_get_incoming_vertical_total(mhl_edid_3d_data->drv_context);
	for (i = 0 ; i < sizeof(timing_modes_from_data_sheet)/sizeof(timing_modes_from_data_sheet[0]); ++i) {
		if (timing_modes_from_data_sheet[i].h_total == h_total) {
			if (timing_modes_from_data_sheet[i].v_total == v_total) {
				ret_val = timing_modes_from_data_sheet[i].pixel_clock;
				MHL_TX_DBG_ERR(,"vic was 0, %s\n"
					,timing_modes_from_data_sheet[i].description);
				return ret_val;
			}
		}
	}
	MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
				   "VIC was zero!!! h_total: 0x%04x v_total:0x%04x\n",
				   h_total,v_total);
	return ret_val;
}

PLACE_IN_CODE_SEG char *psz_space           ="n/a";
PLACE_IN_CODE_SEG char *psz_frame_sequential ="FS ";
PLACE_IN_CODE_SEG char *psz_top_bottom       ="TB ";
PLACE_IN_CODE_SEG char *psz_left_right       ="LR ";

/* VIC is a place holder, and not actually stored */
#define CEA_861_D_VIC_info_entry(VIC,columns,rows,HBlank,VBLank,FieldRate,image_aspect_ratio,scanmode,PixelAspectRatio,flags,clocksPerPelShift,AdditionalVBlank) \
									{columns,rows,HBlank,VBLank,FieldRate,{image_aspect_ratio,scanmode,PixelAspectRatio,flags,clocksPerPelShift,AdditionalVBlank}} 
VIC_info_t VIC_info[]=
{
	 CEA_861_D_VIC_info_entry( 0,   0,   0,  0, 0,  0000 ,iar_16_to_10 ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0) 
	,CEA_861_D_VIC_info_entry( 1, 640, 480,160,45, 60000 ,iar_4_to_3   ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry( 2, 720, 480,138,45, 60000 ,iar_4_to_3   ,vsm_progressive,par_8_to_9             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry( 3, 720, 480,138,45, 60000 ,iar_16_to_9  ,vsm_progressive,par_32_to_27           ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry( 4,1280, 720,370,30, 60000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry( 5,1920,1080,280,22, 60000 ,iar_16_to_9  ,vsm_interlaced ,par_1_to_1             ,vif_dual_frame_rate  ,0,1)
	,CEA_861_D_VIC_info_entry( 6, 720, 480,276,22, 60000 ,iar_4_to_3   ,vsm_interlaced ,par_8_to_9             ,vif_dual_frame_rate  ,1,1)
	,CEA_861_D_VIC_info_entry( 7, 720, 480,276,22, 60000 ,iar_16_to_9  ,vsm_interlaced ,par_32_to_27           ,vif_dual_frame_rate  ,1,1)
	,CEA_861_D_VIC_info_entry( 8, 720, 240,276,22, 60000 ,iar_4_to_3   ,vsm_progressive,par_4_to_9             ,vif_dual_frame_rate  ,1,1)
	,CEA_861_D_VIC_info_entry( 9, 720, 428,276,22, 60000 ,iar_16_to_9  ,vsm_progressive,par_16_to_27           ,vif_dual_frame_rate  ,1,1)
	,CEA_861_D_VIC_info_entry(10,2880, 480,552,22, 60000 ,iar_4_to_3   ,vsm_interlaced ,par_2_to_9_20_to_9     ,vif_dual_frame_rate  ,0,1)
	,CEA_861_D_VIC_info_entry(11,2880, 480,552,22, 60000 ,iar_16_to_9  ,vsm_interlaced ,par_8_to_27_80_to_27   ,vif_dual_frame_rate  ,0,1)
	,CEA_861_D_VIC_info_entry(12,2880, 240,552,22, 60000 ,iar_4_to_3   ,vsm_progressive,par_1_to_9_10_to_9     ,vif_dual_frame_rate  ,0,1)
	,CEA_861_D_VIC_info_entry(13,2880, 240,552,22, 60000 ,iar_16_to_9  ,vsm_progressive,par_4_to_27_40_to_27   ,vif_dual_frame_rate  ,0,1)
	,CEA_861_D_VIC_info_entry(14,1440, 480,276,45, 60000 ,iar_4_to_3   ,vsm_progressive,par_4_to_9             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(15,1440, 480,276,45, 60000 ,iar_16_to_9  ,vsm_progressive,par_16_to_27           ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(16,1920,1080,280,45, 60000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(17, 720, 576,144,49, 50000 ,iar_4_to_3   ,vsm_progressive,par_16_to_15           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(18, 720, 576,144,49, 50000 ,iar_16_to_9  ,vsm_progressive,par_64_to_45           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(19,1280, 720,700,30, 50000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(20,1920,1080,720,22, 50000 ,iar_16_to_9  ,vsm_interlaced ,par_1_to_1             ,vif_single_frame_rate,0,1)
	,CEA_861_D_VIC_info_entry(21, 720, 576,288,24, 50000 ,iar_4_to_3   ,vsm_interlaced ,par_16_to_15           ,vif_single_frame_rate,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(22, 720, 576,288,24, 50000 ,iar_16_to_9  ,vsm_interlaced ,par_64_to_45           ,vif_single_frame_rate,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(23, 720, 288,288,24, 50000 ,iar_4_to_3   ,vsm_progressive,par_8_to_15            ,vif_single_frame_rate,1,2) /* (1440) */
	,CEA_861_D_VIC_info_entry(24, 720, 288,288,24, 50000 ,iar_16_to_9  ,vsm_progressive,par_32_to_45           ,vif_single_frame_rate,1,2) /* (1440) */
	,CEA_861_D_VIC_info_entry(25,2880, 576,576,24, 50000 ,iar_4_to_3   ,vsm_interlaced ,par_2_to_15_20_to_15   ,vif_single_frame_rate,0,1)
	,CEA_861_D_VIC_info_entry(26,2880, 576,576,24, 50000 ,iar_16_to_9  ,vsm_interlaced ,par_16_to_45_160_to_45 ,vif_single_frame_rate,0,1)
	,CEA_861_D_VIC_info_entry(27,2880, 288,576,24, 50000 ,iar_4_to_3   ,vsm_progressive,par_1_to_15_10_to_15   ,vif_single_frame_rate,0,2)
	,CEA_861_D_VIC_info_entry(28,2880, 288,576,24, 50000 ,iar_16_to_9  ,vsm_progressive,par_8_to_45_80_to_45   ,vif_single_frame_rate,0,2)
	,CEA_861_D_VIC_info_entry(29,1440, 576,288,49, 50000 ,iar_4_to_3   ,vsm_progressive,par_8_to_15            ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(30,1440, 576,288,49, 50000 ,iar_16_to_9  ,vsm_progressive,par_32_to_45           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(31,1920,1080,720,45, 50000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(32,1920,1080,830,45, 24000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(33,1920,1080,720,45, 25000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(34,1920,1080,280,45, 30000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(35,2880, 480,552,45, 60000 ,iar_4_to_3   ,vsm_progressive,par_2_to_9             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(36,2880, 480,552,45, 60000 ,iar_16_to_9  ,vsm_progressive,par_8_to_27            ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(37,2880, 576,576,49, 50000 ,iar_4_to_3   ,vsm_progressive,par_4_to_15            ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(38,2880, 576,576,49, 50000 ,iar_16_to_9  ,vsm_progressive,par_16_to_45           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(39,1920,1080,384,85, 50000 ,iar_16_to_9  ,vsm_interlaced ,par_1_to_1             ,vif_single_frame_rate,0,0) /*1250,total*/
	,CEA_861_D_VIC_info_entry(40,1920,1080,720,22,100000 ,iar_16_to_9  ,vsm_interlaced ,par_1_to_1             ,vif_single_frame_rate,0,1)
	,CEA_861_D_VIC_info_entry(41,1280, 720,700,30,100000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(42, 720, 576,144,49,100000 ,iar_4_to_3   ,vsm_progressive,par_16_to_15           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(43, 720, 576,144,49,100000 ,iar_16_to_9  ,vsm_progressive,par_64_to_45           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(44, 720, 576,288,24,100000 ,iar_4_to_3   ,vsm_interlaced ,par_16_to_15           ,vif_single_frame_rate,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(45, 720, 576,288,24,100000 ,iar_16_to_9  ,vsm_interlaced ,par_64_to_45           ,vif_single_frame_rate,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(46,1920,1080,280,22,120000 ,iar_16_to_9  ,vsm_interlaced ,par_1_to_1             ,vif_dual_frame_rate  ,0,1)
	,CEA_861_D_VIC_info_entry(47,1280, 720,370,30,120000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(48, 720, 480,138,45,120000 ,iar_4_to_3   ,vsm_progressive,par_8_to_9             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(49, 720, 480,138,45,120000 ,iar_16_to_9  ,vsm_progressive,par_32_to_27           ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(50, 720, 480,276,22,120000 ,iar_4_to_3   ,vsm_interlaced ,par_8_to_9             ,vif_dual_frame_rate  ,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(51, 720, 480,276,22,120000 ,iar_16_to_9  ,vsm_interlaced ,par_32_to_27           ,vif_dual_frame_rate  ,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(52, 720, 576,144,49,200000 ,iar_4_to_3   ,vsm_progressive,par_16_to_15           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(53, 720, 576,144,49,200000 ,iar_16_to_9  ,vsm_progressive,par_64_to_45           ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(54, 720, 576,288,24,200000 ,iar_4_to_3   ,vsm_interlaced ,par_16_to_15           ,vif_single_frame_rate,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(55, 720, 576,288,24,200000 ,iar_16_to_9  ,vsm_interlaced ,par_64_to_45           ,vif_single_frame_rate,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(56, 720, 480,138,45,240000 ,iar_4_to_3   ,vsm_progressive,par_8_to_9             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(57, 720, 480,138,45,240000 ,iar_16_to_9  ,vsm_progressive,par_32_to_27           ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(58, 720, 480,276,22,240000 ,iar_4_to_3   ,vsm_interlaced ,par_8_to_9             ,vif_dual_frame_rate  ,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(59, 720, 480,276,22,240000 ,iar_16_to_9  ,vsm_interlaced ,par_32_to_27           ,vif_dual_frame_rate  ,1,1) /* (1440) */
	,CEA_861_D_VIC_info_entry(60,1280, 720,370,30, 24000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(61,1280, 720,370,30, 25000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0)
	,CEA_861_D_VIC_info_entry(62,1280, 720,370,30, 30000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(63,1920,1080,280,45,120000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_dual_frame_rate  ,0,0)
	,CEA_861_D_VIC_info_entry(64,1920,1080,280,45,100000 ,iar_16_to_9  ,vsm_progressive,par_1_to_1             ,vif_single_frame_rate,0,0)

};

uint32_t calculate_pixel_clock(edid_3d_data_p mhl_edid_3d_data,
		uint16_t columns, uint16_t rows,
		uint32_t vertical_sync_frequency_in_milliHz,
		uint8_t VIC)
{
	uint32_t pixel_clock_frequency;
	uint32_t vertical_sync_period_in_microseconds;
	uint32_t vertical_active_period_in_microseconds;
	uint32_t vertical_blank_period_in_microseconds;
	uint32_t horizontal_sync_frequency_in_hundredths_of_KHz;
	uint32_t horizontal_sync_period_in_nanoseconds;
	uint32_t horizontal_active_period_in_nanoseconds;
	uint32_t horizontal_blank_period_in_nanoseconds;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"vertical_sync_frequency_in_milliHz: %u\n",vertical_sync_frequency_in_milliHz );

	vertical_sync_period_in_microseconds = 1000000000/vertical_sync_frequency_in_milliHz;
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"vertical_sync_frequency_in_milliHz:%u vertical_sync_period_in_microseconds: %u\n",vertical_sync_frequency_in_milliHz,vertical_sync_period_in_microseconds);

	if (0 == VIC) {
		/* rule of thumb: */
		vertical_active_period_in_microseconds = (vertical_sync_period_in_microseconds * 8) / 10;

	} else {
		uint16_t v_total_in_lines;
		uint16_t v_blank_in_lines;

		if (vsm_interlaced == VIC_info[VIC].fields.interlaced) {
			/* fix up these two values */
			vertical_sync_frequency_in_milliHz /= 2;
			vertical_sync_period_in_microseconds *= 2;
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"interlaced vertical_sync_frequency_in_milliHz:%u"
					" vertical_sync_period_in_microseconds: %u\n"
					,vertical_sync_frequency_in_milliHz
					,vertical_sync_period_in_microseconds);

			/* proceed with calculations */
			v_blank_in_lines = 2 * VIC_info[VIC].v_blank_in_pixels + VIC_info[VIC].fields.field2_v_blank;

		} else {
			/*  when multiple vertical blanking values present,
			  		allow for higher clocks by calculating maximum possible
			*/
			v_blank_in_lines = VIC_info[VIC].v_blank_in_pixels + VIC_info[VIC].fields.field2_v_blank;
		}
		v_total_in_lines = VIC_info[VIC].rows +v_blank_in_lines ;
		vertical_active_period_in_microseconds = (vertical_sync_period_in_microseconds * VIC_info[VIC].rows) / v_total_in_lines;

	}
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"vertical_active_period_in_microseconds: %u\n",vertical_active_period_in_microseconds);

	/* rigorous calculation: */
	vertical_blank_period_in_microseconds  = vertical_sync_period_in_microseconds - vertical_active_period_in_microseconds;
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"vertical_blank_period_in_microseconds: %u\n",vertical_blank_period_in_microseconds);

	horizontal_sync_frequency_in_hundredths_of_KHz = rows * 100000;
	horizontal_sync_frequency_in_hundredths_of_KHz /= vertical_active_period_in_microseconds;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"horizontal_sync_frequency_in_hundredths_of_KHz: %u\n",horizontal_sync_frequency_in_hundredths_of_KHz);

	horizontal_sync_period_in_nanoseconds    = 100000000 / horizontal_sync_frequency_in_hundredths_of_KHz;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"horizontal_sync_period_in_nanoseconds: %u\n",horizontal_sync_period_in_nanoseconds);

	if (0 == VIC) {
		/* rule of thumb: */
		horizontal_active_period_in_nanoseconds = (horizontal_sync_period_in_nanoseconds * 8) / 10;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"horizontal_active_period_in_nanoseconds: %u\n",horizontal_active_period_in_nanoseconds);
	} else {
		uint16_t h_total_in_pixels;
		uint16_t h_clocks;
		h_clocks = VIC_info[VIC].columns << VIC_info[VIC].fields.clocks_per_pixel_shift_count;
		h_total_in_pixels = h_clocks + VIC_info[VIC].h_blank_in_pixels;
		horizontal_active_period_in_nanoseconds = (horizontal_sync_period_in_nanoseconds * h_clocks) / h_total_in_pixels;
	}
	/* rigorous calculation: */
	horizontal_blank_period_in_nanoseconds = horizontal_sync_period_in_nanoseconds - horizontal_active_period_in_nanoseconds;
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"horizontal_blank_period_in_nanoseconds: %u\n",horizontal_blank_period_in_nanoseconds);

	pixel_clock_frequency = columns * (1000000000/ horizontal_active_period_in_nanoseconds);

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"pixel_clock_frequency: %u\n",pixel_clock_frequency);

	return pixel_clock_frequency;
}

uint8_t qualify_pixel_clock_for_mhl(edid_3d_data_p mhl_edid_3d_data, uint32_t pixel_clock_frequency, uint8_t bits_per_pixel)
{
	uint32_t pixel_clock_frequency_div_8;
	uint32_t link_clock_frequency;
	uint32_t max_link_clock_frequency;
	uint8_t ret_val;

	pixel_clock_frequency_div_8 = pixel_clock_frequency / 8;

	link_clock_frequency = pixel_clock_frequency_div_8 * (uint32_t)bits_per_pixel;

	if ( si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) ) {
		max_link_clock_frequency = 300000000;
	} else {
		max_link_clock_frequency = 225000000;
	}

    if (link_clock_frequency <  max_link_clock_frequency) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }
    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
    		"Link clock:%u Hz %12s for MHL at %d bpp (max: %u Hz)\n"
            ,link_clock_frequency
            ,ret_val?"valid":"unattainable"
            ,(uint16_t)bits_per_pixel
            ,max_link_clock_frequency);
    return ret_val;
}

/*
  is_MHL_timing_mode

	MHL has a maximum link clock of 75Mhz.
	For now, we use a rule of thumb regarding
		blanking intervals to calculate a pixel clock,
		then we convert it to a link clock and compare to 75MHz

*/


static uint8_t is_MHL_timing_mode(edid_3d_data_p mhl_edid_3d_data,
						uint16_t columns, uint16_t rows,
						uint32_t vertical_sync_frequency_in_milliHz,
						PMHL2_video_descriptor_t p_MHL2_video_descriptor_parm,
						uint8_t VIC)
{
uint32_t pixel_clock_frequency;
uint8_t ret_val = 0;
MHL2_video_descriptor_t dummy;
PMHL2_video_descriptor_t pMHL2_video_descriptor = p_MHL2_video_descriptor_parm;
	if (NULL == p_MHL2_video_descriptor_parm) {
		dummy.frame_sequential=0;
		dummy.left_right=0;
		dummy.top_bottom=0;
		dummy.reserved_high=0;
		dummy.reserved_low=0;
		pMHL2_video_descriptor=&dummy;
	}

	pixel_clock_frequency = calculate_pixel_clock(mhl_edid_3d_data, columns, rows,
									vertical_sync_frequency_in_milliHz, VIC);

	if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data,pixel_clock_frequency, 24)) {
		display_timing_enumeration_callback(mhl_edid_3d_data, columns, rows,
				24, vertical_sync_frequency_in_milliHz,*pMHL2_video_descriptor);
		ret_val = 1;
	}
	if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data,pixel_clock_frequency, 16)) {
		display_timing_enumeration_callback(mhl_edid_3d_data, columns, rows,
				16, vertical_sync_frequency_in_milliHz,*pMHL2_video_descriptor);
		ret_val = 1;
	}

	return ret_val;
}
#ifdef PRUNE_EDID
void si_mhl_tx_prune_dtd_list(edid_3d_data_p mhl_edid_3d_data,
							  P_18_byte_descriptor_u p_desc,uint8_t limit)
{
	uint8_t i;
	uint8_t number_that_we_pruned = 0;
	#define DTD_Demoresolution
	#ifdef DTD_Demoresolution
	const uint8_t DemodetailTiming [18]={0x01,  0x1D,  0x00,  0x72,  0x51, 0xD0,  0x1E,  0x20,  0x6E,  0x28,  0x55,  0x00,  0xC4,  0x8E,  0x21,  0x00,  0x00,  0x1E};
	//extern void *memcpy(void *dest, const void *src,  uint8_t n);
	#endif

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"limit: %d\n",(uint16_t)limit);
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context," si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) :0x%02x\n",   (uint16_t) si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) );
	  if (limit&&(! si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) ))
    {
        for (i = 0 ; i < limit -1 ; ++i)
        {
        	TX_PRUNE_PRINT(("limit4444: %d,i=%d,(p_desc->dtd.pixel_clock_high):0x%02x, si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) :0x%02x\n",
        		(uint16_t)limit,i,(uint16_t)(p_desc->dtd.pixel_clock_high),(uint16_t) si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) ));
		//DumpEdidBlock(pEdidBlock0,sizeof(*pEdidBlock0));

		if ((0 != p_desc->dtd.pixel_clock_low) || (0 != p_desc->dtd.pixel_clock_high))
	            {
	                if ((0 == p_desc->dtd.horz_active_7_0)&&(0 == p_desc->dtd.horz_active_blanking_high.horz_blanking_11_8))
	                {
	                P_18_byte_descriptor_u pHolder=p_desc,pNextDesc = p_desc+1;
	                uint8_t j;
	                    number_that_we_pruned++;
	                    for (j = i+1; j < limit ; ++j)
	                    {TX_PRUNE_PRINT(("limit555555: %d, i=%d, j=%d\n",(uint16_t)limit,i,j));
	                        // move the rest of the entries one by one
	                        *p_desc++ = *pNextDesc++;
	                    }
	                    // re-consider the new occupant of the i'th entry on the next iteration
	                    //i--;
	                    p_desc=pHolder;
	                }
	            }
			if ((p_desc->dtd.pixel_clock_high>0x20)&&(! si_peer_supports_packed_pixel(mhl_edid_3d_data->dev_context) ))
				#ifndef DTD_Demoresolution
				{
					P_18_byte_descriptor_u pNextDesc = p_desc+1;//pHolder=p_desc,
					uint8_t j;
					number_that_we_pruned++;
					for (j = i+1; j < limit ; ++j)
					{
						//TX_PRUNE_PRINT(("limit6666666: %d,*p_desc:0x%02x\n",(uint16_t)limit,*p_desc));
						// move the rest of the entries one by one
						*p_desc++ = *pNextDesc++;
					}
					// re-consider the new occupant of the i'th entry on the next iteration
					i--;
					//p_desc=pHolder;
               		 }
				#else
				{
					//TX_PRUNE_PRINT(("limit6666666: %d,*p_desc:0x%02x\n",(uint16_t)limit,*p_desc));
				        memcpy(p_desc, &DemodetailTiming[0],18);				   
				}
				#endif
	
        }
        TX_PRUNE_PRINT(("limit2: %d\n",(uint16_t)limit));
        // at this point "i" holds the value of mhlTxConfig.svdDataBlockLength-1
        //  and p_desc points to the last entry in the list
        for (;number_that_we_pruned >0;--number_that_we_pruned,--p_desc)
        {
        uint8_t *pu8Temp = (uint8_t *)p_desc;
        uint8_t size;

            for (size = sizeof(*p_desc); size > 0; --size)
            {TX_PRUNE_PRINT(("*pu8Temp: %d\n",(uint16_t)*pu8Temp));
                *pu8Temp++ = 0;
			}
        }
        TX_PRUNE_PRINT(("limit3: %d\n",(uint16_t)limit));
    }
}
#endif


/*

 FUNCTION     :   si_mhl_tx_parse_detailed_timing_descriptor()

 PURPOSE      :   Parse the detailed timing section of EDID Block 0 and
				  print their decoded meaning to the screen.

 INPUT PARAMS :   Pointer to the array where the data read from EDID
				  Block0 is stored.

				  Offset to the beginning of the Detailed Timing Descriptor
				  data.

									  Block indicator to distinguish between block #0 and blocks
									  #2, #3

 OUTPUT PARAMS:   None

 GLOBALS USED :   None

 RETURNS      :   true if valid timing, false if not

*/

static bool si_mhl_tx_parse_detailed_timing_descriptor(
					edid_3d_data_p mhl_edid_3d_data,
					P_18_byte_descriptor_u p_desc ,
					uint8_t Block, uint8_t *p_is_timing,
					PMHL2_video_descriptor_t pMHL2_video_descriptor)
{
	uint8_t tmp_byte;
	uint8_t i;
	uint16_t tmp_word;

	*p_is_timing = 0;
	tmp_word = p_desc->dtd.pixel_clock_low + 256 * p_desc->dtd.pixel_clock_high;
	/*  18 byte partition is used as either for Monitor Name or for Monitor Range Limits or it is unused */
	if (tmp_word == 0x00) {
		/*  if called from Block #0 and first 2 bytes are 0 => either Monitor Name or for Monitor Range Limits */
		if (Block == EDID_BLOCK_0) {
			if (0xFC == p_desc->name.data_type_tag) {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
							  "Monitor Name: ");

				for (i = 0; i < 13; i++) {
// TODO: FD, TBI
					printk("%c", p_desc->name.ascii_name[i]); /* Display monitor name */
#if 0
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
							"%c", p_desc->name.ascii_name[i]); /* Display monitor name */
#endif
				}
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context, "\n");
			} else if (0xFD == p_desc->name.data_type_tag) {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
							  "Monitor Range Limits:\n\n");

				//i = 0;
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Min Vertical Rate in Hz: %d\n",
						(int)p_desc->range_limits.min_vertical_rate_in_Hz);
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Max Vertical Rate in Hz: %d\n",
						(int)p_desc->range_limits.max_vertical_rate_in_Hz);
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Min Horizontal Rate in KHz: %d\n",
						(int)p_desc->range_limits.min_horizontal_rate_in_KHz);
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Max Horizontal Rate in KHz: %d\n",
						(int)p_desc->range_limits.max_horizontal_rate_in_KHz);
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Max Supported pixel clock rate in MHz/10: %d\n",
						(int)p_desc->range_limits.max_pixel_clock_in_MHz_div_10);
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Tag for secondary timing formula (00h=not used): %d\n",
						(int)p_desc->range_limits.tag_secondary_formula);
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"\n");
			}
		} else if (Block == EDID_BLOCK_2_3) {
			/* if called from block #2 or #3 and first 2 bytes are 0x00 (padding) then this
			 descriptor partition is not used and parsing should be stopped
			*/
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"No More Detailed descriptors in this block\n");
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context, "\n");
			return false;
		}
	} else {
		 /* first 2 bytes are not 0 => this is a detailed timing descriptor from either block */
		uint32_t pixel_clock_frequency;
		uint16_t columns,rows,vertical_sync_period_in_lines;
		uint32_t vertical_refresh_rate_in_milliHz,horizontal_sync_frequency_in_Hz,horizontal_sync_period_in_pixels;

		*p_is_timing = 1;

		pixel_clock_frequency = (uint32_t)tmp_word * 10000;

		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Pixel Clock: %d.%02d MHz or %d Hz (0x%x Hz)\n",
				tmp_word/100, tmp_word%100,pixel_clock_frequency,pixel_clock_frequency);

		columns = p_desc->dtd.horz_active_7_0 + 256 * p_desc->dtd.horz_active_blanking_high.horz_active_11_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Horizontal Active Pixels: %d\n", columns);

		tmp_word = p_desc->dtd.horz_blanking_7_0 + 256 * p_desc->dtd.horz_active_blanking_high.horz_blanking_11_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Horizontal Blanking (Pixels): %d\n", tmp_word);

		horizontal_sync_period_in_pixels = (uint32_t)columns + (uint32_t)tmp_word;
		horizontal_sync_frequency_in_Hz = pixel_clock_frequency/horizontal_sync_period_in_pixels;

		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"horizontal period %u pixels,  "\
				"horizontal_sync_frequency_in_Hz: %u Hz\n",
				horizontal_sync_period_in_pixels,horizontal_sync_frequency_in_Hz);

		rows = p_desc->dtd.vert_active_7_0 + 256 * p_desc->dtd.vert_active_blanking_high.vert_active_11_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Vertical Active (Lines): %u\n", rows);

		tmp_word = p_desc->dtd.vert_blanking_7_0 + 256 * p_desc->dtd.vert_active_blanking_high.vert_blanking_11_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Vertical Blanking (Lines): %u\n", tmp_word);

		vertical_sync_period_in_lines = rows + tmp_word;
		vertical_refresh_rate_in_milliHz = horizontal_sync_frequency_in_Hz*1000/(vertical_sync_period_in_lines);
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"vertical period %u lines, frequency %u MilliHz\n",
				vertical_sync_period_in_lines,vertical_refresh_rate_in_milliHz);

		tmp_word = p_desc->dtd.horz_sync_offset_7_0 + 256 * p_desc->dtd.hs_offset_hs_pulse_width_vs_offset_vs_pulse_width.horzSyncOffset9_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Horizontal Sync Offset (Pixels): %d\n", tmp_word);

		tmp_word = p_desc->dtd.horz_sync_pulse_width7_0 + 256 * p_desc->dtd.hs_offset_hs_pulse_width_vs_offset_vs_pulse_width.horz_sync_pulse_width_9_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Horizontal Sync Pulse Width (Pixels): %d\n", tmp_word);

		tmp_word = p_desc->dtd.vert_sync_offset_width.vert_sync_offset_3_0                             +  16 * p_desc->dtd.hs_offset_hs_pulse_width_vs_offset_vs_pulse_width.vert_sync_offset_5_4;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Vertical Sync Offset (Lines): %d\n", tmp_word);

		tmp_word = p_desc->dtd.vert_sync_offset_width.vert_sync_pulse_width_3_0 
				+  16 * p_desc->dtd.hs_offset_hs_pulse_width_vs_offset_vs_pulse_width.vert_sync_pulse_width_5_4;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Vertical Sync Pulse Width (Lines): %d\n", tmp_word);

		tmp_word = p_desc->dtd.horz_image_size_in_mm_7_0 
				+ 256 * p_desc->dtd.image_size_high.horz_image_size_in_mm_11_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Horizontal Image Size (mm): %d\n", tmp_word);

		tmp_word = p_desc->dtd.vert_image_size_in_mm_7_0
				+ 256 * p_desc->dtd.image_size_high.vert_image_size_in_mm_11_8;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Vertical Image Size (mm): %d\n", tmp_word);

		tmp_byte = p_desc->dtd.horz_border_in_lines;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Horizontal Border (Pixels): %d\n", (int)tmp_byte);

		tmp_byte = p_desc->dtd.vert_border_in_pixels;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"Vertical Border (Lines): %d\n", (int)tmp_byte);

		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context, "%sInterlaced\n",p_desc->dtd.flags.interlaced ? "" : "Non-" );

		switch (p_desc->dtd.flags.stereo_bits_2_1)
		{
		case 0:
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Normal Display, No Stereo\n");
			break;
		case 1:
			if (0 == p_desc->dtd.flags.stereo_bit_0) {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Field sequential stereo, right image when "\
						"stereo sync signal == 1\n");
			} else {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"2-way interleaved stereo, right image on "\
						"even lines\n");
			}
			break;
		case 2:
			if (0 == p_desc->dtd.flags.stereo_bit_0) {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"field-sequential stereo, left image when "\
						"stereo sync signal == 1\n");
			} else {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"2-way interleaved stereo, left image on even "\
						"lines.\n");
			}
			break;
		case 3:
			if (0 == p_desc->dtd.flags.stereo_bit_0) {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"4-way interleaved stereo\n");
			} else {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"side-by-side interleaved stereo.\n");
			}
			break;
		}

		switch ( p_desc->dtd.flags.sync_signal_type )
		{
		case 0x0:
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Analog Composite\n");
			break;
		case 0x1:
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Bipolar Analog Composite\n");
			break;
		case 0x2:
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Digital Composite\n");
			break;
		case 0x3:
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Digital Separate\n");
			break;
		}
		if (pMHL2_video_descriptor) {
			uint8_t this_mode_doable=0;
			if ((this_mode_doable=qualify_pixel_clock_for_mhl((void*)mhl_edid_3d_data,pixel_clock_frequency,24))) {
				display_timing_enumeration_callback(mhl_edid_3d_data, columns,
									rows, 24, vertical_refresh_rate_in_milliHz,
									*pMHL2_video_descriptor);
			}

			if (this_mode_doable |=qualify_pixel_clock_for_mhl((void*)mhl_edid_3d_data,pixel_clock_frequency,16)) {
				display_timing_enumeration_callback(mhl_edid_3d_data, columns,
									rows, 16, vertical_refresh_rate_in_milliHz,
									*pMHL2_video_descriptor);
			}
			if (!this_mode_doable) {
				return false;
			}
		}
	}
	return true;
}

static uint8_t si_mhl_tx_parse_861_long_descriptors(edid_3d_data_p mhl_edid_3d_data,uint8_t *p_EDID_block_data)
{
	PCEA_extension_t p_CEA_extension = (PCEA_extension_t)p_EDID_block_data;


	/* per CEA-861-D, table 27 */
	if (!p_CEA_extension->byte_offset_to_18_byte_descriptors) {
		MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
				"EDID -> No Detailed Descriptors\n");
		return EDID_NO_DETAILED_DESCRIPTORS;
	} else {
		uint8_t *puc_next_block;
		uint8_t descriptor_num = 1;
		union
		{
			uint8_t *puc_data_block;
			P_18_byte_descriptor_u p_long_descriptors;
		}p_data_u;

		p_data_u.p_long_descriptors= (P_18_byte_descriptor_u)(((uint8_t *)p_CEA_extension) + p_CEA_extension->byte_offset_to_18_byte_descriptors);
		puc_next_block = ((uint8_t *)p_CEA_extension) + EDID_BLOCK_SIZE;

		while ((uint8_t *)(p_data_u.p_long_descriptors + 1) < puc_next_block) {
			uint8_t is_timing=0;
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Parse Results - CEA-861 Long Descriptor #%d:\n",
					(int) descriptor_num);
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"===============================================================\n");

			if (!si_mhl_tx_parse_detailed_timing_descriptor(mhl_edid_3d_data,p_data_u.p_long_descriptors, EDID_BLOCK_2_3,&is_timing,NULL)) {
				break;
			}
			p_data_u.p_long_descriptors++;
			descriptor_num++;
		}

		return EDID_LONG_DESCRIPTORS_OK;
	}

}

#ifdef PRUNE_EDID
static void si_mhl_tx_prune_edid(edid_3d_data_p mhl_edid_3d_data)
{
	PEDID_block0_t p_EDID_block_0 = (PEDID_block0_t)&mhl_edid_3d_data->EDID_block_data[0];
	uint8_t dtd_limit;
	PCEA_extension_t p_CEA_extension = (PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE];
	Pblock_map_t p_block_map = NULL;
	uint8_t *pb_limit;
	union
	{
		uint8_t *puc_data_block;
		P_18_byte_descriptor_u p_long_descriptors;
	}p_data_u;

	if (EDID_EXTENSION_BLOCK_MAP == p_CEA_extension->tag) {
		/* save to overwrite later */
		p_block_map = (Pblock_map_t)p_CEA_extension;

		/* advance to next block */
		p_CEA_extension++;
	}
	pb_limit = (uint8_t *)(p_CEA_extension+1);

    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"mhl_tx: %s\n",__FUNCTION__);
    p_data_u.puc_data_block = (uint8_t *)p_CEA_extension + p_CEA_extension->byte_offset_to_18_byte_descriptors;

	DUMP_EDID_BLOCK(0,p_EDID_block_0,sizeof(*p_EDID_block_0))  /* no semicolon needed here */
	/* zero out checksums before modifying data */
	p_CEA_extension->checksum=0;
	p_EDID_block_0->checksum = 0;

    /* Is there an HDMI VSDB? */
    if (mhl_edid_3d_data->parse_data.p_HDMI_vsdb) {
        PHDMI_LLC_vsdb_payload_t p_HDMI_vendor_specific_payload = &mhl_edid_3d_data->parse_data.p_HDMI_vsdb->payload_u.HDMI_LLC;
        uint8_t *p_next_db = (uint8_t *)p_HDMI_vendor_specific_payload +mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header.fields.length_following_header;

		/* if deep color information is provided... */
		if (((uint8_t *)&p_HDMI_vendor_specific_payload->byte6) < p_next_db) {
			p_HDMI_vendor_specific_payload->byte6.DC_Y444 =0;
			p_HDMI_vendor_specific_payload->byte6.DC_30bit=0;
			p_HDMI_vendor_specific_payload->byte6.DC_36bit=0;
			p_HDMI_vendor_specific_payload->byte6.DC_48bit=0;
		}
	}
	/* prune the DTDs in block 0 */
	dtd_limit = sizeof(p_EDID_block_0->detailed_timing_descriptors)/sizeof(p_EDID_block_0->detailed_timing_descriptors[0]);
	si_mhl_tx_prune_dtd_list(mhl_edid_3d_data,(P_18_byte_descriptor_u)&p_EDID_block_0->detailed_timing_descriptors[0],dtd_limit);
	DUMP_EDID_BLOCK(0,p_EDID_block_0,sizeof(*p_EDID_block_0))  /* no semicolon needed here */

	DUMP_EDID_BLOCK(0,p_CEA_extension,sizeof(*p_CEA_extension))  /* no semicolon needed here */
	/* prune the DTDs in the CEA-861D extension */
	dtd_limit = (uint8_t)p_CEA_extension->version_u.version3.misc_support.total_number_detailed_timing_descriptors_in_entire_EDID;
	si_mhl_tx_prune_dtd_list(mhl_edid_3d_data,&p_data_u.p_long_descriptors[0],dtd_limit);
    /* adjust the mask according to which 2D VICs were set to zero */
    if (mhl_edid_3d_data->parse_data.p_3d_mask) {
		uint8_t lower_mask;
		uint32_t mask32;
		int8_t index = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->header.fields.length_following_header-1;
		index = (index > 15) ? 15 : index;

		mask32 = 0xFFFF00 >> (15 - index);
		lower_mask = (index > 7) ? 0x7F : (0x7F >> (7 - index));

		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"3d mask 15..8: 0x%02x",
				(uint16_t)mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_15_8);
		for (
				; index >= 8
					; mask32>>=1,lower_mask >>=1, --index) {
			if (0 == mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->short_descriptors[index].VIC) {
			uint8_t lower_bits,upper_bits;
			uint8_t upper_mask;
				upper_mask = (uint8_t)mask32;

				/* preserve the lower bits */
				lower_bits = lower_mask  &  mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_15_8;

				/* and out the bit in question */
				upper_bits = upper_mask  &  mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_15_8;

				/* adjust the positions of the upper bits */
				upper_bits >>=1;

				mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_15_8 = lower_bits | upper_bits;
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"3d mask 15..8: 0x%02x",
						(uint16_t)mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_15_8);
			}
		}
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				"3d mask 7..0: 0x%02x",
				(uint16_t)mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_7_0);
		lower_mask =   0x7F >> (7 - index);
		for (
				; index >= 0
					; mask32>>=1,lower_mask >>=1, --index) {
			if (0 == mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->short_descriptors[index].VIC) {
			uint8_t lower_bits,upper_bits;
			uint8_t upper_mask;
				upper_mask = (uint8_t)mask32;

				/* preserve the lower bits */
				lower_bits = lower_mask  &  mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_7_0;

				/* AND out the bit in question */
				upper_bits = upper_mask  &  mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_7_0;

				/* adjust the positions of the upper bits */
				upper_bits >>=1;

				mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_7_0 = lower_bits | upper_bits;
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"3d mask 7..0: 0x%02x\n",
						(uint16_t)mhl_edid_3d_data->parse_data.p_3d_mask->_3D_mask_7_0);
			}
		}
	}

    if (mhl_edid_3d_data->parse_data.p_three_d) {
		uint8_t num_3D_structure_bytes_pruned=0;
		union
		{
			P_3D_structure_and_detail_entry_u            p_3D;
			P_3D_structure_and_detail_entry_sans_byte1_t   p_sans_byte_1;
			P_3D_structure_and_detail_entry_with_byte1_t   p_with_byte_1;
			uint8_t                                 *p_as_bytes;
		}p_3D_u;
		uint32_t deletion_mask=0;
		uint8_t limit_2D_VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->header.fields.length_following_header;
		/*
		prior to moving things around,
         make a bitmap of the positions of the VICs that are zero
		*/
        {
			uint8_t i;
			uint32_t this_bit;
            for (i =0,this_bit=1; i < limit_2D_VIC;++i,this_bit<<=1)
            {
            uint8_t VIC;
                VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->short_descriptors[i].VIC;
                if (0 == VIC)
                {
                    // set the bit that corresponds to the VIC that was set to zero
                    deletion_mask |= this_bit;
                    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"vic: 0x%02x deletion_mask:0x%08x this_bit:0x%08x\n"
                            ,VIC ,deletion_mask ,this_bit
                            );
                }
            }
        }

    	p_3D_u.p_3D = mhl_edid_3d_data->parse_data.p_three_d;
		while ( p_3D_u.p_as_bytes < mhl_edid_3d_data->parse_data.p_3d_limit) {
				uint8_t _2D_VIC_order           = p_3D_u.p_sans_byte_1->byte0._2D_VIC_order;
				_3D_structure_e _3D_structure = p_3D_u.p_sans_byte_1->byte0._3D_structure;
				uint8_t VIC;
			VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->short_descriptors[_2D_VIC_order].VIC;
			if (0 == VIC) {
				/* delete this 3D_Structure/3D_detail information */
				uint8_t *pSrc,*pDest=p_3D_u.p_as_bytes;

				if (_3D_structure < tdsSideBySide) {
					pSrc = (uint8_t *)(p_3D_u.p_sans_byte_1+1);
					num_3D_structure_bytes_pruned += sizeof(*p_3D_u.p_sans_byte_1);
				} else {
					pSrc = (uint8_t *)(p_3D_u.p_with_byte_1+1);
					num_3D_structure_bytes_pruned += sizeof(*p_3D_u.p_with_byte_1);
				}
				while (pSrc < pb_limit) {
					*pDest++=*pSrc++;
				}
				while (pDest < pb_limit) {
					*pDest++=0;
				}
			} else {
				uint8_t i;
				uint8_t limit = _2D_VIC_order;
				uint32_t this_bit;
                MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"2D vic order: 0x%02x deletion_mask:0x%08x\n"
                        ,_2D_VIC_order
                        ,deletion_mask
                        );

                for (i = 0,this_bit=1; i < limit;++i,this_bit<<=1)
                {
                    if (this_bit & deletion_mask)
                    {
                        _2D_VIC_order--;
                    }
                }
                p_3D_u.p_sans_byte_1->byte0._2D_VIC_order = _2D_VIC_order;
                MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"2D vic order: 0x%02x this_bit:0x%08x\n"
                        ,_2D_VIC_order
                        ,this_bit
                        );


				if (_3D_structure < tdsSideBySide) {
					p_3D_u.p_sans_byte_1++;
				} else {
					p_3D_u.p_with_byte_1++;
				}
			}
		}
        MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"num_3D_structure_bytes_pruned:0x%x "
                        "byte14: 0x%02x "
                        "offset to DTDs: 0x%x "
                        "vsdb header: 0x%x\n"
                        ,num_3D_structure_bytes_pruned
                        ,*((uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14)
                        ,p_CEA_extension->byte_offset_to_18_byte_descriptors
                        ,*((uint8_t *)&mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header)
                        );

    	mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.HDMI_3D_len       -= num_3D_structure_bytes_pruned;
        p_CEA_extension->byte_offset_to_18_byte_descriptors               -= num_3D_structure_bytes_pruned;
        mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header.fields.length_following_header -= num_3D_structure_bytes_pruned;

        MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"num_3D_structure_bytes_pruned:0x%x "
                        "byte14: 0x%02x "
                        "offset to DTDs: 0x%x "
                        "vsdb header: 0x%x\n"
                        ,num_3D_structure_bytes_pruned
                        ,*((uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14)
                        ,p_CEA_extension->byte_offset_to_18_byte_descriptors
                        ,*((uint8_t *)&mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header)
                        );
    }
    /* now prune the HDMI VSDB VIC list */
    if (mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15) {
        uint8_t length_VIC= mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.HDMI_VIC_len;

		if (0 ==length_VIC) {
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"length_VIC:%d \n",(uint16_t)length_VIC);
		} else {
			uint8_t i,num_HDMI_VICs_pruned=0;
			uint8_t inner_loop_limit;
			uint8_t outer_loop_limit;
			inner_loop_limit = length_VIC;
			outer_loop_limit = length_VIC - 1;
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"length_VIC:%d inner_loop_limit: %d outer_loop_limit: %d \n",
					(uint16_t)length_VIC,(uint16_t)inner_loop_limit,
					(uint16_t)outer_loop_limit);
			for (i=0; i < outer_loop_limit;) {
				if (0 == mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[i]) {
					uint8_t j,prev;
					for (prev=i,j = i+1; j < inner_loop_limit;++j,++prev) {
						uint16_t VIC0,VIC1;

                    VIC0 = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[prev];
                    VIC1 = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[j];
                        MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
                        		"replacing VIC: %3d at index: %3d with "\
                        		"VIC:%3d from index: %3d \n"
                            ,VIC0
                            ,(uint16_t)prev
                            ,VIC1
                            ,(uint16_t)j
                            );
                        mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[prev]
                            = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[j];
                    }
                    /* we just removed one */
                    num_HDMI_VICs_pruned++;
                    inner_loop_limit--;
                    outer_loop_limit--;
                } else {
                    /* this mode is doable on MHL, so move on to the next index */
                    ++i;
                }
            }
            /* check the last one */
            if (0 == mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[outer_loop_limit]) {
                num_HDMI_VICs_pruned++;
				inner_loop_limit--;
            }

            DUMP_EDID_BLOCK(0,p_CEA_extension,sizeof(*p_CEA_extension))  /* no semicolon needed here */
            /* now move all other data up */
            if (num_HDMI_VICs_pruned) {
                uint8_t *pb_dest  = (uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[inner_loop_limit];
                uint8_t *pb_src   = (uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[length_VIC];

				SII_ASSERT(EDID_BLOCK_SIZE==sizeof(*p_CEA_extension),("\n\n unexpected extension size\n\n"));
				while(pb_src < pb_limit) {
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								   "moving data up %02x(0x%02x) <- %02x(0x%02x)\n",
								   pb_dest,(uint16_t)*pb_dest,pb_src,(uint16_t)*pb_src);
					*pb_dest++=*pb_src++;
				}

				while(pb_dest < pb_limit) {
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								   "clearing data %02x <- 0\n",pb_dest);
					*pb_dest++=0;
				}

            }
            mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.HDMI_VIC_len = inner_loop_limit;
            p_CEA_extension->byte_offset_to_18_byte_descriptors         -= num_HDMI_VICs_pruned;

	    MHL_TX_EDID_INFO(,"%p\n",mhl_edid_3d_data->parse_data.p_HDMI_vsdb);
	    if (mhl_edid_3d_data->parse_data.p_HDMI_vsdb){
		    mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header.fields.length_following_header -= num_HDMI_VICs_pruned;
	    }else if (num_HDMI_VICs_pruned){
		    MHL_TX_DBG_ERR(,"How can you have HDMI vics to prune if you have no HDMI VSDB?\n");	
	    }
        }
    }

    /* Now prune the SVD list and move the CEA 861-D data blocks and DTDs up */
    {
		uint8_t i,num_CEA_VICs_pruned=0;
		/*
		pack each vdb to eliminate the bytes that have been zeroed.
		*/
		int8_t vdb_index;
		for (vdb_index =mhl_edid_3d_data->parse_data.num_video_data_blocks-1
				;vdb_index >= 0 ;--vdb_index) {
			uint8_t inner_loop_limit = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->header.fields.length_following_header;
			if (inner_loop_limit) {
				uint8_t outer_loop_limit = inner_loop_limit-1;

				for (i=0; i < outer_loop_limit;) {
					if (0 == mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[i].VIC) {
						uint8_t j,prev;
						num_CEA_VICs_pruned++;
						for (prev=i,j = i+1; j < inner_loop_limit;++j,++prev) {
							uint16_t VIC0,VIC1;

							VIC0 = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[prev].VIC;
							VIC1 = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[j].VIC;
							MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
									"replacing SVD:%6s 0x%02x at index: 0x%02x with "\
									"SVD:%6s 0x%02x from index: 0x%02x \n"
									,mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->
									short_descriptors[prev].native ? "Native":"" 
									,VIC0
									,(uint16_t)prev
									,mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->
									short_descriptors[j].native ? "Native":"" 
									,VIC1
									,(uint16_t)j
									);
							mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[prev]
							   = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[j];
						}
						/* we just removed one */
						inner_loop_limit--;
						outer_loop_limit--;
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context
								,"outer_loop_limit:0x%x inner_loop_limit:0x%x\n"
								, (uint16_t)outer_loop_limit
								, (uint16_t)inner_loop_limit
								);
					} else {
						/* this mode is doable on MHL, so move on to the next index */
						++i;
					}
				}
				/* check the last one */
				if (0 == mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[outer_loop_limit].VIC) {
					num_CEA_VICs_pruned++;
					inner_loop_limit--;
					mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[outer_loop_limit].native=0;
				}


				DUMP_EDID_BLOCK(0,p_CEA_extension,sizeof(*p_CEA_extension))  /* no semicolon needed here */

				/* now move all other data up */
				{
					uint8_t *pb_dest = (uint8_t *)&mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[inner_loop_limit];
					uint8_t *pb_src= (uint8_t *)&mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->header.fields.length_following_header];

					SII_ASSERT(EDID_BLOCK_SIZE==sizeof(*p_CEA_extension),("\n\n unexpected extension size\n\n"));
					while(pb_src < pb_limit) {
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"moving data up %p(0x%02x) <- %p(0x%02x)\n",
								pb_dest,(uint16_t)*pb_dest,pb_src,(uint16_t)*pb_src);
						*pb_dest++=*pb_src++;
					}

					while(pb_dest < pb_limit) {
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"clearing data %02x <- 0\n", *pb_dest);
						*pb_dest++=0;
					}

				}
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"CEA-861-D DTDs began at 0x%02x" \
						"CEA-861-D SVD count: 0x%x\n"
						,(uint16_t)p_CEA_extension->byte_offset_to_18_byte_descriptors
						,(uint16_t)mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->
						header.fields.length_following_header);

				p_CEA_extension->byte_offset_to_18_byte_descriptors               -= num_CEA_VICs_pruned;
				mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->header.fields.length_following_header = inner_loop_limit;

				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"CEA-861-D DTDs now begin at 0x%02x" \
						"CEA-861-D SVD count: 0x%x\n"
						,(uint16_t)p_CEA_extension->byte_offset_to_18_byte_descriptors
						,(uint16_t)mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->
						header.fields.length_following_header);

				DUMP_EDID_BLOCK(0,p_CEA_extension,sizeof(*p_CEA_extension))  /* no semicolon needed here */
			}
		}
	}

	/* re-compute the checksum(s) */
	SII_ASSERT(EDID_BLOCK_SIZE==sizeof(*p_EDID_block_0),("\n\n unexpected size for block 0\n\n"));
	SII_ASSERT(EDID_BLOCK_SIZE==sizeof(*p_CEA_extension),("\n\n unexpected size for  CEA extension\n\n"));

	if (p_block_map) {
		PCEA_extension_t p_CEA_extensionDest=(PCEA_extension_t)p_block_map;
		*p_CEA_extensionDest = *p_CEA_extension;
	}


	p_EDID_block_0->checksum = calculate_generic_checksum((uint8_t *)p_EDID_block_0,0,sizeof(*p_EDID_block_0));

	p_CEA_extension->checksum=calculate_generic_checksum((uint8_t *)p_CEA_extension,0,sizeof(*p_CEA_extension));


	DUMP_EDID_BLOCK(0,p_CEA_extension,sizeof(*p_CEA_extension))  /* no semicolon needed here */


	/*
		TODO: adjust all pointers into the EDID along the way of pruning the contents, instead of re-parsing here
	*/
#ifndef EDID_PASSTHROUGH //(
    if (0 == si_mhl_tx_drv_set_upstream_edid(mhl_edid_3d_data->drv_context,mhl_edid_3d_data->EDID_block_data,2*EDID_BLOCK_SIZE))
#endif //)
	{
		SET_3D_FLAG(mhl_edid_3d_data,FLAGS_EDID_READ_DONE)
		si_mhl_tx_drv_enable_video_path(mhl_edid_3d_data->drv_context);	// TODO: FD, TBI, debug?
	}
}
#endif
/*
*/
static uint8_t IsQualifiedMhlVIC(edid_3d_data_p mhl_edid_3d_data,uint8_t VIC,PMHL2_video_descriptor_t p_mhl2_video_descriptor)
{
	uint8_t ret_val=0;
	if (VIC > 0) {
		ret_val= is_MHL_timing_mode(mhl_edid_3d_data,VIC_info[VIC].columns, VIC_info[VIC].rows, VIC_info[VIC].field_rate_in_milliHz,p_mhl2_video_descriptor,VIC);
		if (vif_dual_frame_rate == VIC_info[VIC].fields.frame_rate_info) {
			uint32_t field_rate_in_milliHz;
			switch(VIC_info[VIC].field_rate_in_milliHz)
			{
			case 24000: /* 23.97 */
				field_rate_in_milliHz = 23970;
				break;

			case 30000: /* 29.97 */
				field_rate_in_milliHz = 29970;
				break;

			case 60000: /* 59.94 */
				field_rate_in_milliHz = 59940;
				break;

			case 120000: /* 119.88 */
				field_rate_in_milliHz = 119880;
				break;

			case 240000: /* 239.76 */
				field_rate_in_milliHz = 239760;
				break;

			default: /* error or unknown case */
				field_rate_in_milliHz=0;
				break;
			}
			ret_val |= is_MHL_timing_mode(mhl_edid_3d_data,VIC_info[VIC].columns, VIC_info[VIC].rows, field_rate_in_milliHz,p_mhl2_video_descriptor,VIC);
		}
	}
	return ret_val;
}
/* HDMI_VIC is a place holder, and not actually stored */
#define hdmi_vic_infoEntry(HDMI_VIC,columns,rows,FieldRate0,FieldRate1,pixel_clock_0,pixel_clock_1) \
							{columns,rows,FieldRate0,FieldRate1,pixel_clock_0,pixel_clock_1} 

PLACE_IN_CODE_SEG  HDMI_VIC_info_t hdmi_vic_info[]=
{
	 hdmi_vic_infoEntry( 0,   0,   0,    0,    0,         0,        0) 
	,hdmi_vic_infoEntry( 1,3840,2160,30000,29970, 297000000,296703000)
	,hdmi_vic_infoEntry( 2,3840,2160,25000,25000, 297000000,297000000)
	,hdmi_vic_infoEntry( 3,3840,2160,24000,23976, 297000000,296703000)
	,hdmi_vic_infoEntry( 4,4096,2160,24000,24000, 297000000,297000000)
};
/*
*/
static uint8_t is_qualified_mhl_hdmi_vic(edid_3d_data_p mhl_edid_3d_data,uint8_t VIC,PMHL2_video_descriptor_t pMHL2_video_descriptor)
{
uint8_t ret_val=0;

	if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data, hdmi_vic_info[VIC].pixel_clock_0, 24)) {
		display_timing_enumeration_callback(mhl_edid_3d_data,
				hdmi_vic_info[VIC].columns, hdmi_vic_info[VIC].rows, 24,
				hdmi_vic_info[VIC].field_rate_0_in_milliHz,
				*pMHL2_video_descriptor);
		ret_val = 1;
	}
	if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data,hdmi_vic_info[VIC].pixel_clock_0, 16)) {
		display_timing_enumeration_callback(mhl_edid_3d_data,
				hdmi_vic_info[VIC].columns, hdmi_vic_info[VIC].rows, 16,
				hdmi_vic_info[VIC].field_rate_0_in_milliHz,*pMHL2_video_descriptor);
		ret_val = 1;
	}
	if (hdmi_vic_info[VIC].pixel_clock_0 != hdmi_vic_info[VIC].pixel_clock_1) {
		if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data,hdmi_vic_info[VIC].pixel_clock_1, 24)) {
			display_timing_enumeration_callback(mhl_edid_3d_data,
					hdmi_vic_info[VIC].columns, hdmi_vic_info[VIC].rows, 24,
					hdmi_vic_info[VIC].field_rate_1_in_milliHz,
					*pMHL2_video_descriptor);
			ret_val = 1;
		}
		if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data,hdmi_vic_info[VIC].pixel_clock_1, 16)) {
			display_timing_enumeration_callback(mhl_edid_3d_data,
					hdmi_vic_info[VIC].columns, hdmi_vic_info[VIC].rows, 16,
					hdmi_vic_info[VIC].field_rate_1_in_milliHz,
					*pMHL2_video_descriptor);
			ret_val = 1;
		}
	}
	return ret_val;
}

void si_mhl_tx_enumerate_hdmi_vsdb(edid_3d_data_p mhl_edid_3d_data)
{
	int8_t vdb_index=0;
    if (mhl_edid_3d_data->parse_data.p_HDMI_vsdb) {
        PHDMI_LLC_vsdb_payload_t p_HDMI_vendor_specific_payload = &mhl_edid_3d_data->parse_data.p_HDMI_vsdb->payload_u.HDMI_LLC;
        uint8_t *p_next_db = (uint8_t *)p_HDMI_vendor_specific_payload +mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header.fields.length_following_header;
        /*  if 3D_present field is included */
        if (mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15) {
            if (((uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte13) < p_next_db) {
            uint8_t hdmi3D_present          = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte13._3D_present;
            uint8_t hdmi_3D_multi_present   = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte13._3D_multi_present;

				/*  if HDMI_VIC_len is present... */
				if (((uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14) < p_next_db) {
					uint8_t length_VIC;
					uint8_t index;
					MHL2_video_descriptor_t mhl2_video_descriptor;
					length_VIC =  mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.HDMI_VIC_len;
					mhl2_video_descriptor.left_right      = 0;
					mhl2_video_descriptor.top_bottom      = 0;
					mhl2_video_descriptor.frame_sequential= 0;
					for (index = 0; index < length_VIC;++index) {
						uint8_t VIC;

                    	VIC = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[index];
                        if (!is_qualified_mhl_hdmi_vic(mhl_edid_3d_data,VIC,&mhl2_video_descriptor)) {
                            MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
                            		"'can't do HDMI VIC:%d\n",(uint16_t)VIC);
                            mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[index] = 0;
                        }
                    }
                    if (hdmi3D_present) {
                    uint8_t length_3D;
                    PHDMI_3D_sub_block_t pThree3DSubBlock= (PHDMI_3D_sub_block_t)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[length_VIC];
                    union
                    {
                        P_3D_structure_and_detail_entry_u            p_3D;
                        P_3D_structure_and_detail_entry_sans_byte1_t   p_sans_byte_1;
                        P_3D_structure_and_detail_entry_with_byte1_t   p_with_byte_1;
                        uint8_t                                 *p_as_bytes;
                    }p_3D_u;
                    uint8_t limit;
                        p_3D_u.p_3D=NULL;
                        length_3D  =  mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.HDMI_3D_len;
                        limit =mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->header.fields.length_following_header;
                        /* only do the first 16 */
                        limit = (limit > 16)?16:limit;
                        switch(hdmi_3D_multi_present)
                        {
                        case 0x00:
                            /* 3D_Structure_ALL_15..0 and 3D_MASK_15..0 fields are not present */
                            p_3D_u.p_3D = &pThree3DSubBlock->hDMI_3D_sub_block_sans_all_AND_mask._3D_structure_and_detail_list[0];
                            break;
                        case 0x01:
                            /*
                                3D_Structure_ALL_15..0 is present and assigns 3D formats
                                    to all of the VICs listed in the first 16 entries in the EDID
                                3D_mask_15..0 is not present
                            */
                            {
                                P_3D_structure_all_t p_3D_structure_all=(P_3D_structure_all_t)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[length_VIC];
                                mhl2_video_descriptor.left_right      = p_3D_structure_all->_3D_structure_all_7_0.side_by_side;
                                mhl2_video_descriptor.top_bottom      = p_3D_structure_all->_3D_structure_all_15_8.top_bottom;
                                mhl2_video_descriptor.frame_sequential= p_3D_structure_all->_3D_structure_all_15_8.frame_packing;
								DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
                                for (index = 0; index < limit;++index) {
                                uint8_t VIC;

									VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[index].VIC;
									if (VIC) {
										if (!IsQualifiedMhlVIC(mhl_edid_3d_data,VIC,&mhl2_video_descriptor)) {
											mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[index].VIC=0;
										}
									}
								}
								DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
								length_3D -= sizeof(*p_3D_structure_all);
							}
							p_3D_u.p_3D = &pThree3DSubBlock->HDMI_3D_sub_block_sans_mask._3D_structure_and_detail_list[0];
							break;
						case 0x02:
							/*
								3D_Structure_ALL_15..0 and 3D_mask_15..0 are present and
									assign 3D formats to some of the VICS listed in the first
									16 entries in the EDID

							*/
							{
								P_3D_structure_all_t p_3D_structure_all=(P_3D_structure_all_t)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->vicList[length_VIC];
								P_3D_mask_t p3DMask = (P_3D_mask_t)(p_3D_structure_all+1);
								uint8_t mask;
								mhl2_video_descriptor.left_right      = p_3D_structure_all->_3D_structure_all_7_0.side_by_side;
								mhl2_video_descriptor.top_bottom      = p_3D_structure_all->_3D_structure_all_15_8.top_bottom;
								mhl2_video_descriptor.frame_sequential= p_3D_structure_all->_3D_structure_all_15_8.frame_packing;
								DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
								for (mask=1,index = 0; (mask > 0) && (index < limit);++index,mask<<=1) {
									uint8_t VIC;
									MHL2_video_descriptor_t this_MHL2_video_descriptor;

									if (mask & p3DMask->_3D_mask_7_0) {
										this_MHL2_video_descriptor = mhl2_video_descriptor;
									} else {
										this_MHL2_video_descriptor.left_right      = 0;
										this_MHL2_video_descriptor.top_bottom      = 0;
										this_MHL2_video_descriptor.frame_sequential= 0;
									}

									VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[index].VIC;
									if (VIC) {
										if (!IsQualifiedMhlVIC(mhl_edid_3d_data,VIC,&mhl2_video_descriptor)) {
											mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[index].VIC=0;
										}
									}
								}
								DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
								for (mask=1;(mask > 0) && (index < limit);++index,mask<<=1) {
									uint8_t VIC;
									MHL2_video_descriptor_t this_MHL2_video_descriptor;

									if (mask & p3DMask->_3D_mask_15_8) {
										this_MHL2_video_descriptor = mhl2_video_descriptor;
									} else {
										this_MHL2_video_descriptor.left_right      = 0;
										this_MHL2_video_descriptor.top_bottom      = 0;
										this_MHL2_video_descriptor.frame_sequential= 0;
									}

									VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[index].VIC;
									if (VIC) {
										if (!IsQualifiedMhlVIC(mhl_edid_3d_data,VIC,&mhl2_video_descriptor)) {
											mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[index].VIC=0;
										}
									}
								}
								DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
								length_3D -= sizeof(*p_3D_structure_all);
								length_3D -= sizeof(*p3DMask);
							}
							p_3D_u.p_3D = &pThree3DSubBlock->HDMI_3D_sub_block_with_all_AND_mask._3D_structure_and_detail_list[0];
							mhl_edid_3d_data->parse_data.p_3d_mask = &pThree3DSubBlock->HDMI_3D_sub_block_with_all_AND_mask._3D_mask;
							break;
						case 0x03:
							/*
								Reserved for future use.
								3D_Structure_ALL_15..0 and 3D_mask_15..0 are NOT present
							*/
							p_3D_u.p_3D = &pThree3DSubBlock->hDMI_3D_sub_block_sans_all_AND_mask._3D_structure_and_detail_list[0];
							break;
						}
						mhl_edid_3d_data->parse_data.p_three_d =p_3D_u.p_3D;
						mhl_edid_3d_data->parse_data.p_3d_limit = &p_3D_u.p_as_bytes[length_3D];
						DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
						while ( p_3D_u.p_as_bytes < mhl_edid_3d_data->parse_data.p_3d_limit) {
							uint8_t _2D_VIC_order           = p_3D_u.p_sans_byte_1->byte0._2D_VIC_order;
							_3D_structure_e _3D_structure = p_3D_u.p_sans_byte_1->byte0._3D_structure;
							uint8_t VIC;
							VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[_2D_VIC_order].VIC;
							/* this VIC position might have gotten disqualified already */
							if (VIC) {
								mhl2_video_descriptor.left_right           = 0;
								mhl2_video_descriptor.top_bottom           = 0;
								mhl2_video_descriptor.frame_sequential     = 0;
								switch(_3D_structure)
								{
								case tdsSideBySide:
									{
									//re-visit uint8_t _3D_detail    = p_3D_u.p_with_byte_1->byte1._3D_detail;
										mhl2_video_descriptor.left_right   = 1;
									}
									break;
								case tdsTopAndBottom:
									mhl2_video_descriptor.top_bottom       = 1;
									break;
								case tdsFramePacking:
									mhl2_video_descriptor.frame_sequential = 1;
									break;
								}
								if (!IsQualifiedMhlVIC(mhl_edid_3d_data,VIC,&mhl2_video_descriptor)) {
									mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[vdb_index]->short_descriptors[_2D_VIC_order].VIC=0;
								}
							}
							if (_3D_structure < tdsSideBySide) {
								p_3D_u.p_sans_byte_1++;
							} else {
								p_3D_u.p_with_byte_1++;
							}
						}
						DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
					}
				}
			}
		}
	}
}

static void si_mhl_tx_display_timing_enumeration_end(edid_3d_data_p mhl_edid_3d_data)
{
	mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress = 0;
	/* finish off with any 3D modes reported via the HDMI VSDB */
	si_mhl_tx_enumerate_hdmi_vsdb(mhl_edid_3d_data);
	/* notify the app (board specific) layer */
	display_timing_enumeration_end(mhl_edid_3d_data);
	SET_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_DONE);
	#ifdef PRUNE_EDID
	si_mhl_tx_prune_edid(mhl_edid_3d_data);
	#else
	SET_3D_FLAG(mhl_edid_3d_data,FLAGS_EDID_READ_DONE)
	si_mhl_tx_drv_enable_video_path(mhl_edid_3d_data->drv_context);	// //TODO: FD, TBI, debug?
	#endif
}

static void CheckForAll3DBurstDone(edid_3d_data_p mhl_edid_3d_data)
{
	if (TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_VIC_DONE)){
		if (TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_DTD_DONE)) {
			if (!TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_DONE)) {
				si_mhl_tx_display_timing_enumeration_end(mhl_edid_3d_data);
			}
		}
	}
}

/*
*/
void si_mhl_tx_process_3d_vic_burst(
			void *context
			, PMHL2_video_format_data_t p_write_burst_data /* from 3D_REQ */
	)
{
	edid_3d_data_p mhl_edid_3d_data=(edid_3d_data_p)context; 
	uint8_t block_index = 0;
	PMHL2_video_descriptor_t p_mhl2_video_descriptor;
	//re-visit uint8_t edidLimit = mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.HDMI_3D_len;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"burstEntryCount3D_VIC: %d\n",mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic);

	if ( mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress) {
		/* check to see if it's time to move on to the next block */
		if (mhl_edid_3d_data->parse_data.vic_2d_index >= mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.video_data_block_index]->header.fields.length_following_header) {
			mhl_edid_3d_data->parse_data.video_data_block_index++;
			if ( mhl_edid_3d_data->parse_data.video_data_block_index >= mhl_edid_3d_data->parse_data.num_video_data_blocks){
				SET_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_VIC_DONE)
				CheckForAll3DBurstDone(mhl_edid_3d_data);
				return;
			}
		}

		if (mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic < mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.video_data_block_index]->header.fields.length_following_header) {
			/* each SVD is 1 byte long */
			DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
			for (/* block_index is set to zero above */
					;(block_index < p_write_burst_data->num_entries_this_burst)
						&&
						 (mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic < p_write_burst_data->total_entries )
						;++block_index
						 ,++mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic
						 ,++mhl_edid_3d_data->parse_data.vic_2d_index) {
				uint8_t VIC;
				uint8_t this_mode_doable=0;
				/* check to see if it's time to move on to the next block */
				if (mhl_edid_3d_data->parse_data.vic_2d_index >= mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.video_data_block_index]->header.fields.length_following_header) {
					mhl_edid_3d_data->parse_data.video_data_block_index++;
					if ( mhl_edid_3d_data->parse_data.video_data_block_index >= mhl_edid_3d_data->parse_data.num_video_data_blocks){
						SET_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_VIC_DONE)
						break;
					}
				}

				p_mhl2_video_descriptor = &p_write_burst_data->video_descriptors[block_index];
				VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.video_data_block_index]->short_descriptors[mhl_edid_3d_data->parse_data.vic_2d_index].VIC;

				if (VIC) {
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
							"short Descriptor[%d] 3D VIC: %d %s %s %s\n"
							,mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic
							,VIC
							,p_mhl2_video_descriptor->left_right      ?psz_left_right      :psz_space
							,p_mhl2_video_descriptor->top_bottom      ?psz_top_bottom      :psz_space
							,p_mhl2_video_descriptor->frame_sequential?psz_frame_sequential:psz_space
					);
					this_mode_doable = IsQualifiedMhlVIC(mhl_edid_3d_data,VIC,p_mhl2_video_descriptor);
					if (!this_mode_doable) {
						/* prune this mode from EDID */

						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"'can't do CEA VIC:%d\n",(uint16_t)VIC);
						mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.video_data_block_index]->short_descriptors[mhl_edid_3d_data->parse_data.vic_2d_index].VIC = 0;
					}
				}
			}	
			DUMP_EDID_BLOCK(0,(PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE],EDID_BLOCK_SIZE)  /* no semicolon needed here */
		}

		if ( mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic >= p_write_burst_data->total_entries ) {
			SET_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_VIC_DONE)
		}
		CheckForAll3DBurstDone(mhl_edid_3d_data);
	}
}

void check_3d_dtd_sequence_done(edid_3d_data_p mhl_edid_3d_data
			  	,PMHL2_video_format_data_t p_write_burst_data,uint8_t dtd_limit)
{
int flag=0;
	if (mhl_edid_3d_data->parse_data.cea_861_dtd_index>=  dtd_limit) {
		flag = 1;
	}

	if ( mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd >= p_write_burst_data->total_entries ) {
		flag =1;
	}
	if (flag) {
		SET_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DTD_DONE)
		if (TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_VIC_DONE)) {
			if (!TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_DONE)) {
				si_mhl_tx_display_timing_enumeration_end(mhl_edid_3d_data);
			}
		}
	}
}


void si_mhl_tx_process_3d_dtd_burst(void *context,PMHL2_video_format_data_t p_write_burst_data)
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;
	PMHL2_video_descriptor_t p_mhl2_video_descriptor;
	int burst_index=0;
	uint8_t is_timing=0;
	PCEA_extension_t p_CEA_extension = (PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE];
	uint8_t dtd_limit = (uint8_t)p_CEA_extension->version_u.version3.misc_support.total_number_detailed_timing_descriptors_in_entire_EDID;

	if ( mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress) {
		if (!TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_DTD_VESA_DONE)) {
			PEDID_block0_t p_EDID_block_0 = (PEDID_block0_t)&mhl_edid_3d_data->EDID_block_data[0];
			/*
			  up to four DTDs are possible in the base VESA EDID
				this will be covered by a single burst. 
			*/
			for (/* burst_index is set to zero above */
					;(burst_index < p_write_burst_data->num_entries_this_burst)
					 &&
					 (mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd < p_write_burst_data->total_entries )
					 &&
					 (mhl_edid_3d_data->parse_data.vesa_dtd_index < sizeof(p_EDID_block_0->detailed_timing_descriptors)/sizeof(p_EDID_block_0->detailed_timing_descriptors[0]))
					;++mhl_edid_3d_data->parse_data.vesa_dtd_index) {
				P_18_byte_descriptor_u p_desc = (P_18_byte_descriptor_u)&p_EDID_block_0->detailed_timing_descriptors[mhl_edid_3d_data->parse_data.vesa_dtd_index];
				bool is_valid=0;

				p_mhl2_video_descriptor = &p_write_burst_data->video_descriptors[burst_index];
				is_valid = si_mhl_tx_parse_detailed_timing_descriptor(
								mhl_edid_3d_data, p_desc, EDID_BLOCK_0,
								&is_timing, p_mhl2_video_descriptor);
				/* only count it if it's a valid timing */
				if (is_timing) {

					if (is_valid) {
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"VESA DTD index: %d burst index:%d DTD SP "\
								"index:%d %s %s %s\n"
								,(uint16_t)mhl_edid_3d_data->parse_data.vesa_dtd_index,(uint16_t)burst_index,(uint16_t)mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd
								,p_mhl2_video_descriptor->left_right      ?psz_left_right      :psz_space
								,p_mhl2_video_descriptor->top_bottom      ?psz_top_bottom      :psz_space
								,p_mhl2_video_descriptor->frame_sequential?psz_frame_sequential:psz_space
								);
					} else {
						/* mark this mode for pruning by setting horizontal active to zero */
						p_desc->dtd.horz_active_7_0 = 0;
						p_desc->dtd.horz_active_blanking_high.horz_active_11_8= 0;
					}

					burst_index++;
					mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd++;
				} else {
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
							"VESA DTD index: %d\n",
							(uint16_t)mhl_edid_3d_data->parse_data.vesa_dtd_index);
				}
			}

			if (mhl_edid_3d_data->parse_data.vesa_dtd_index >= sizeof(p_EDID_block_0->detailed_timing_descriptors)/sizeof(p_EDID_block_0->detailed_timing_descriptors[0])) {
				/* we got past the VESA DTDs in this burst */
				SET_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DTD_VESA_DONE)
			} else {
				check_3d_dtd_sequence_done(mhl_edid_3d_data,p_write_burst_data,dtd_limit);
				/* more VESA DTDs to process in next burst */
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"%s\n",TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_BURST_3D_DTD_DONE)
							?"3D DTD descriptors exhausted"
							:"more VESA DTDs to process");
				return;
			}
		}
		{
			PCEA_extension_t p_CEA_extension = (PCEA_extension_t)&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE];
			uint8_t dtd_limit = (uint8_t)p_CEA_extension->version_u.version3.misc_support.total_number_detailed_timing_descriptors_in_entire_EDID;
			union
			{
			uint8_t *puc_data_block;
			P_18_byte_descriptor_u p_long_descriptors;
			}p_data_u;
			p_data_u.p_long_descriptors= (P_18_byte_descriptor_u)(((uint8_t *)p_CEA_extension) + p_CEA_extension->byte_offset_to_18_byte_descriptors);
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context
				,"continuing with CEA-861-D/E DTDs"
				 "\n\tburst_index: %d"
				 "\n\tburst_entry_count_3d_dtd: %d"
				 "\n\tnum_entries_this_burst: %d"
				 "\n\ttotal_entries:%d"
				 "\n\tdtd_limit:%d"
				 "\n\toffsetTo18_byte_descriptors:0x%x\n"
				,burst_index
				,mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd
				,p_write_burst_data->num_entries_this_burst
				,p_write_burst_data->total_entries
				,dtd_limit
				,p_CEA_extension->byte_offset_to_18_byte_descriptors
				);
			/* continue with CEA-861-D/E DTDs when done with VESA DTDs */
			for (/* burst_index is set to zero above */
					;(burst_index < p_write_burst_data->num_entries_this_burst)
						&&
						 (mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd < p_write_burst_data->total_entries )
							&&
						 (mhl_edid_3d_data->parse_data.cea_861_dtd_index <  dtd_limit)
						;++mhl_edid_3d_data->parse_data.cea_861_dtd_index) {
				P_18_byte_descriptor_u p_desc = &p_data_u.p_long_descriptors[mhl_edid_3d_data->parse_data.cea_861_dtd_index];
				bool is_valid=0;
				p_mhl2_video_descriptor = &p_write_burst_data->video_descriptors[burst_index];
				is_valid=si_mhl_tx_parse_detailed_timing_descriptor(mhl_edid_3d_data,p_desc, EDID_BLOCK_2_3,&is_timing,p_mhl2_video_descriptor);
				/* only count it if it's a valid timing */
				if (is_timing) {

					if (is_valid) {
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"CEA-861 DTD index: %d burst index:%d DTD "\
								"SP index:%d %s %s %s\n\n"
								,(uint16_t)mhl_edid_3d_data->parse_data.cea_861_dtd_index,(uint16_t)burst_index,(uint16_t)mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd
								,p_mhl2_video_descriptor->left_right      ?psz_left_right      :psz_space
								,p_mhl2_video_descriptor->top_bottom      ?psz_top_bottom      :psz_space
								,p_mhl2_video_descriptor->frame_sequential?psz_frame_sequential:psz_space
								);
					} else {
						/* mark this mode for pruning by setting horizontal active to zero */
						p_desc->dtd.horz_active_7_0 = 0;
						p_desc->dtd.horz_active_blanking_high.horz_active_11_8= 0;
					}

					++burst_index;
					++mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd;
				} else {
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
							"CEA-861 DTD index: %d\n",
							(uint16_t)mhl_edid_3d_data->parse_data.vesa_dtd_index);
				}
			}

			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"DTD burst complete\n");
			check_3d_dtd_sequence_done(mhl_edid_3d_data,p_write_burst_data,dtd_limit);
		}
	}
}


/*

 FUNCTION     :   si_mhl_tx_parse_established_timing()

 PURPOSE      :   Parse the established timing section of EDID Block 0 and
				  print their decoded meaning to the screen.

 INPUT PARAMS :   Pointer to the array where the data read from EDID
				  Block0 is stored.

 OUTPUT PARAMS:   None

 GLOBALS USED :   None

 RETURNS      :   Void

*/

#define STRINGIZE(x) #x
#define DUMP_OFFSET(c,s,m) MHL_TX_EDID_INFO(c->dev_context, STRINGIZE(m)" offset:%x\n",SII_OFFSETOF(s,m) );
#define DUMP_ESTABLISHED_TIMING(context,group,width,height,refresh,progressive) \
if (p_EDID_block_0->group.et##width##x##height##_##refresh##Hz##progressive) { \
	MHL_TX_EDID_INFO(context->dev_context, STRINGIZE(group)"."STRINGIZE(width)"x"STRINGIZE(height)"_"STRINGIZE(refresh)"Hz"STRINGIZE(progressive)"\n"); \
	if (!is_MHL_timing_mode(mhl_edid_3d_data,width, height, refresh*1000,NULL,0)) { \
		p_EDID_block_0->group.et##width##x##height##_##refresh##Hz##progressive = 0; \
	} \
}

static void si_mhl_tx_parse_established_timing (edid_3d_data_p mhl_edid_3d_data,PEDID_block0_t p_EDID_block_0)
{


	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,header_data[0]);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,id_manufacturer_name);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,id_product_code);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,serial_number[0]);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,week_of_manufacture);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,year_of_manufacture);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,EDID_version);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,EDID_revision);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,video_input_definition);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,horz_screen_size_or_aspect_ratio);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,vert_screen_size_or_aspect_ratio);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,display_transfer_characteristic);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,feature_support);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,red_green_bits_1_0);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,blue_white_bits_1_0);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,red_x);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,red_y);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,green_x);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,green_y);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,blue_x);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,blue_y);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,white_x);
	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,white_y);

	/* MHL cannot support these modes, so prune them */
	p_EDID_block_0->established_timings_II.et1280x1024_75Hz = 0;
	p_EDID_block_0->manufacturers_timings.et1152x870_75Hz = 0;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"Parsing Established Timing:\n");
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"===========================\n");


	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,established_timings_I) /* no semicolon needed here ... ... see macro */
	/* Parse Established Timing Byte #0 */
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,720,400,70,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,720,400,88,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,640,480,60,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,640,480,67,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,640,480,72,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,640,480,75,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,800,600,56,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_I,800,600,60,)

	/* Parse Established Timing Byte #1: */

	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,established_timings_II) /* no semicolon needed here ... ... see macro */
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II, 800, 600,72,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II, 800, 600,75,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II, 832, 624,75,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II,1024, 768,87,I)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II,1024, 768,60,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II,1024, 768,70,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II,1024, 768,75,)
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, established_timings_II,1280,1024,75,)

	/* Parse Established Timing Byte #2: */

	DUMP_OFFSET(mhl_edid_3d_data, EDID_block0_t,manufacturers_timings) /* no semicolon needed here ... ... see macro */
	DUMP_ESTABLISHED_TIMING(mhl_edid_3d_data, manufacturers_timings,1152,870,75,)

	if(   (!p_EDID_block_0->header_data[0])
			&&(0 == *((uint8_t *)&p_EDID_block_0->established_timings_II)  )
			&&(!p_EDID_block_0->header_data[2])
	  ) {
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"No established video modes\n");
	}
}
/*

 FUNCTION     :   si_mhl_tx_parse_standard_timing()

 PURPOSE      :   Parse the standard timing section of EDID Block 0 and
				  print their decoded meaning to the screen.

 INPUT PARAMS :   Pointer to the array where the data read from EDID
				  Block0 is stored.

 OUTPUT PARAMS:   None

 GLOBALS USED :   None

 RETURNS      :   Void

*/

static void si_mhl_tx_parse_standard_timing(edid_3d_data_p mhl_edid_3d_data,PEDID_block0_t p_EDID_block_0)
{
	uint8_t i;
	uint8_t AR_code;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context, "Parsing Standard Timing:\n");
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context, "========================\n");

	for (i = 0; i < sizeof(p_EDID_block_0->standard_timings)/sizeof(p_EDID_block_0->standard_timings[0]); i += 2) {
		if (
				(1 == p_EDID_block_0->standard_timings[i].horz_pix_div_8_minus_31)
				&& (1 == p_EDID_block_0->standard_timings[i].field_refresh_rate_minus_60)
				&& (0 == p_EDID_block_0->standard_timings[i].image_aspect_ratio)
		   ) {
			/* per VESA EDID standard, Release A, Revision 1, February 9, 2000, Sec. 3.9 */
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Standard Timing Undefined\n");
		} else {
			uint16_t horz_active=(uint16_t)((p_EDID_block_0->standard_timings[i].horz_pix_div_8_minus_31 + 31)*8);
			uint16_t vert_active=0;
			uint16_t refresh_rate_in_milliHz = (uint16_t)(p_EDID_block_0->standard_timings[i].field_refresh_rate_minus_60+ 60)*1000;
			char *psz_ratio_string="";

			/* per VESA EDID standard, Release A, Revision 1, February 9, 2000, Table 3.15 */
			AR_code = p_EDID_block_0->standard_timings[i].image_aspect_ratio;

			switch(AR_code)
			{
				case iar_16_to_10:
					psz_ratio_string = "16:10";
					vert_active = horz_active * 10/16;
					break;

				case iar_4_to_3:
					psz_ratio_string = "4:3";
					vert_active = horz_active *  3/ 4;
					break;

				case iar_5_to_4:
					psz_ratio_string = "5:4";
					vert_active = horz_active *  4/ 5;
					break;

				case iar_16_to_9:
					psz_ratio_string = "16:9";
					vert_active = horz_active *  9/16;
					break;
			}
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Aspect Ratio: %5s %4d x %4d at %3d Hz.\n",
					psz_ratio_string, horz_active,vert_active,
					refresh_rate_in_milliHz);

			if (!is_MHL_timing_mode(mhl_edid_3d_data,horz_active, vert_active, refresh_rate_in_milliHz,NULL,0)) {
				/* disable this mode */
				p_EDID_block_0->standard_timings[i].horz_pix_div_8_minus_31 = 1;
				p_EDID_block_0->standard_timings[i].field_refresh_rate_minus_60 = 1;
				p_EDID_block_0->standard_timings[i].image_aspect_ratio = 0;
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"Disabled\n\n");
			}
		}
	}
}


/*

 FUNCTION     :   si_mhl_tx_parse_block_zero_timing_descriptors()

 PURPOSE      :   Parse EDID Block 0 timing descriptors per EEDID 1.3
				  standard. printf() values to screen.

 INPUT PARAMS :   Pointer to the 128 byte array where the data read from EDID
				  Block0 is stored.

 OUTPUT PARAMS:   None

 GLOBALS USED :   None

 RETURNS      :   Void

*/
#if 0
static void si_mhl_tx_parse_block_zero_timing_descriptors(edid_3d_data_p mhl_edid_3d_data,PEDID_block0_t p_EDID_block_0)
{
	uint8_t i;
	uint8_t is_timing=0;
	si_mhl_tx_parse_established_timing(mhl_edid_3d_data,p_EDID_block_0);
	si_mhl_tx_parse_standard_timing(mhl_edid_3d_data,p_EDID_block_0);

	for (i = 0; i < sizeof(p_EDID_block_0->detailed_timing_descriptors)/sizeof(p_EDID_block_0->detailed_timing_descriptors[0]); i++) {
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Parse Results, EDID Block #0, Detailed Descriptor Number %d:\n",
					(int)i);
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"===========================================================\n\n");
		si_mhl_tx_parse_detailed_timing_descriptor(mhl_edid_3d_data,(P_18_byte_descriptor_u)&p_EDID_block_0->detailed_timing_descriptors[i], EDID_BLOCK_0,&is_timing,NULL);
	}
}
#endif
/*

 FUNCTION     :   bool si_mhl_tx_do_edid_checksum()

 PURPOSE      :   Calculte checksum of the 128 byte block pointed to by the
				  pointer passed as parameter

 INPUT PARAMS :   Pointer to a 128 byte block whose checksum needs to be
				  calculated

 OUTPUT PARAMS:   None

 GLOBALS USED :   None

 RETURNS      :   true if chcksum is 0. false if not.

*/

static bool si_mhl_tx_do_edid_checksum(uint8_t *p_EDID_block_data)
{
	uint8_t i;
	uint8_t checksum = 0;

	for (i = 0; i < EDID_BLOCK_SIZE; i++) {
		checksum += p_EDID_block_data[i];
	}

	if (checksum) {
		return false;
	}

	return true;
}


/*

 FUNCTION     :   bool si_mhl_tx_check_edid_header()

 PURPOSE      :   Checks if EDID header is correct per VESA E-EDID standard
					Must be 00 FF FF FF FF FF FF 00

 INPUT PARAMS :   Pointer to EDID parser context area
					Pointer to 1st EDID block

 OUTPUT PARAMS:   None

 GLOBALS USED :   None

 RETURNS      :   true if Header is correct. false if not.

*/


/* Block 0 */
#define EDID_OFFSET_HEADER_FIRST_00	0x00
#define EDID_OFFSET_HEADER_FIRST_FF	0x01
#define EDID_OFFSET_HEADER_LAST_FF	0x06
#define EDID_OFFSET_HEADER_LAST_00	0x07
static bool si_mhl_tx_check_edid_header(edid_3d_data_p mhl_edid_3d_data,
		PEDID_block0_t p_EDID_block_0)
{
	uint8_t i = 0;

	if (0 != p_EDID_block_0->header_data[EDID_OFFSET_HEADER_FIRST_00]) {
		DUMP_EDID_BLOCK(1,p_EDID_block_0,sizeof(*p_EDID_block_0))
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context
						,"EDID 0 first check failed\n\n\n");
		return false;
	}

	for (i = EDID_OFFSET_HEADER_FIRST_FF; i <= EDID_OFFSET_HEADER_LAST_FF; i++) {
		if(0xFF != p_EDID_block_0->header_data[i]) {
			DUMP_EDID_BLOCK(1,p_EDID_block_0,sizeof(*p_EDID_block_0))
				MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context
							,"EDID -1 check failed\n");
			return false;
		}
	}

	if (0x00 != p_EDID_block_0->header_data[EDID_OFFSET_HEADER_LAST_00]) {
		DUMP_EDID_BLOCK(1,p_EDID_block_0,sizeof(*p_EDID_block_0))
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context
						,"EDID 0 last check failed\n");
		return false;
	}

	return true;
}



void SiiMhlTxMakeItDVI(edid_3d_data_p mhl_edid_3d_data,PEDID_block0_t p_EDID_block_0)
{
	/* Make it DVI */
	mhl_edid_3d_data->parse_data.HDMI_sink = false;
	{
		uint8_t *p_EDID_block_data =(uint8_t *)p_EDID_block_0;
		uint8_t counter;

		p_EDID_block_0->extension_flag = 0;

		// blank out the second block of the upstream EDID
		MHL_TX_DBG_INFO(mhl_edid_3d_data->dev_context,
							"DVI EDID ...Setting second block to 0xFF %d\n",
							(uint16_t)EDID_REV_ADDR_ERROR);
		p_EDID_block_data += EDID_BLOCK_SIZE;
		for (counter = 0; counter < EDID_BLOCK_SIZE; counter++)
			p_EDID_block_data[counter] = 0xFF;
	}

	MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context
			,"EDID: second block now all 0xFF\n");
}

#if 0
static void SiiMhlTx3dReqForNonTranscodeMode( edid_3d_data_p mhl_edid_3d_data )
{
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
			"Mhl2Tx: outputMode: %s\n",
			mhl_edid_3d_data->parse_data.HDMI_sink ? "HDMI":"DVI");
	if (mhl_edid_3d_data->parse_data.HDMI_sink) {
		if (0x20 <= si_get_peer_mhl_version(mhl_edid_3d_data->dev_context)) {
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"MHL 2.x sink detected\n");

			mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress = 1;
			display_timing_enumeration_begin(mhl_edid_3d_data);
			/* tell the sink to begin sending 3D write bursts */
			si_mhl_tx_set_int( mhl_edid_3d_data->dev_context,MHL_RCHANGE_INT, MHL2_INT_3D_REQ,0);

			mhl_edid_3d_data->parse_data.video_data_block_index=0;
			mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd=0;
			mhl_edid_3d_data->parse_data.vesa_dtd_index=0;
			mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic=0;
			mhl_edid_3d_data->parse_data.vic_2d_index=0;
			mhl_edid_3d_data->parse_data.vic_3d_index=0;
			mhl_edid_3d_data->parse_data.cea_861_dtd_index=0;

			SET_3D_FLAG(mhl_edid_3d_data, FLAGS_SENT_3D_REQ)
			CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DONE)
			CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_VIC_DONE)
			CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DTD_VESA_DONE)
		} else {
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"MHL 1.x sink detected\n");

			// EDID read need to be done in SW TPI mode, as it is done now, switch back to default TPI mode: HW TPI mode
			si_mhl_tx_drv_set_hw_tpi_mode( mhl_edid_3d_data->drv_context, true );
			#ifdef PRUNE_EDID
			si_mhl_tx_prune_edid(mhl_edid_3d_data);
			#else
			SET_3D_FLAG(mhl_edid_3d_data,FLAGS_EDID_READ_DONE)
			si_mhl_tx_drv_enable_video_path(mhl_edid_3d_data->drv_context);	// //TODO: FD, TBI, debug?
			#endif

		}
	} else {
#ifndef EDID_PASSTHROUGH //(
		if (0 == si_mhl_tx_drv_set_upstream_edid(mhl_edid_3d_data->drv_context,mhl_edid_3d_data->EDID_block_data,2*EDID_BLOCK_SIZE))
#endif //)
		{
			// EDID read need to be done in SW TPI mode, as it is done now, switch back to default TPI mode: HW TPI mode
			si_mhl_tx_drv_set_hw_tpi_mode( mhl_edid_3d_data->drv_context, true );

			SET_3D_FLAG(mhl_edid_3d_data, FLAGS_EDID_READ_DONE);
			si_mhl_tx_drv_enable_video_path(mhl_edid_3d_data->drv_context);	// TODO: FD, TBI, debug?
		}
	}
}
#endif


uint8_t CA=0;//Channel/Speaker Allocation.
uint8_t MAX_channel=2;//Channel.
uint8_t  Samplebit;

uint8_t  Cap_MAX_channel;
uint8_t  Cap_Samplebit;
uint16_t Cap_SampleRate;
static uint8_t parse_861_short_descriptors (
				  edid_3d_data_p mhl_edid_3d_data
			  	, uint8_t *p_EDID_block_data)
{
uint8_t i;
	PCEA_extension_t p_CEA_extension = (PCEA_extension_t)p_EDID_block_data;

	if (EDID_EXTENSION_TAG != p_CEA_extension->tag) {
		MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
				"EDID -> Non-CEA Extension\n");
		return EDID_EXT_TAG_ERROR;
	} else {
		if (EDID_REV_THREE != p_CEA_extension->revision) {
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
					"EDID -> Non-HDMI EIA-861 Revision ID. Expected %02X. Got %02X\n",
					(int)EDID_REV_THREE, (int)p_CEA_extension->revision);
			return EDID_REV_ADDR_ERROR;
		} else {
			PCEA_extension_version_3_t p_CEA_extension_version_3 = &p_CEA_extension->version_u.version3;
			union
			{
				uint8_t *puc_data_block;
				PCEA_data_block_collection_t p_CEA_data_block;
			}p_data_u;
			uint8_t *puc_long_descriptors;
			/* block offset where long descriptors start */
			puc_long_descriptors= ((uint8_t *)p_CEA_extension) + p_CEA_extension->byte_offset_to_18_byte_descriptors;

			/* byte #3 of CEA extension version 3 */
			mhl_edid_3d_data->parse_data.underscan   = p_CEA_extension_version_3->misc_support.underscan_IT_formats_by_default?1:0;
			mhl_edid_3d_data->parse_data.basic_audio  = p_CEA_extension_version_3->misc_support.basic_audio_support?1:0;
			mhl_edid_3d_data->parse_data.YCbCr_4_4_4 = p_CEA_extension_version_3->misc_support.YCrCb444_support;
			mhl_edid_3d_data->parse_data.YCbCr_4_2_2 = p_CEA_extension_version_3->misc_support.YCrCb422_support;
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"misc support index-> %02x\n",
					*((uint8_t *)&p_CEA_extension_version_3->misc_support) );

#ifdef	NEVER
			/* choose output color depth in order of preference */
			if (mhl_edid_3d_data->parse_data.YCbCr_4_4_4) {
				si_mhl_tx_drv_set_output_color_space( mhl_edid_3d_data->drv_context,BIT_EDID_FIELD_FORMAT_YCbCr444 );
			} else if (mhl_edid_3d_data->parse_data.YCbCr_4_2_2) {
				si_mhl_tx_drv_set_output_color_space( mhl_edid_3d_data->drv_context,BIT_EDID_FIELD_FORMAT_YCbCr422 );
			} else {
				si_mhl_tx_drv_set_output_color_space( mhl_edid_3d_data->drv_context,BIT_EDID_FIELD_FORMAT_HDMI_TO_RGB);
			}
#endif	//	NEVER

			p_data_u.puc_data_block = &p_CEA_extension->version_u.version3.Offset4_u.data_block_collection[0];

            while (p_data_u.puc_data_block < puc_long_descriptors) {
            data_block_tag_code_e tag_code;
            uint8_t data_block_length;
                tag_code = p_data_u.p_CEA_data_block->header.fields.tag_code;
                data_block_length = p_data_u.p_CEA_data_block->header.fields.length_following_header;
                MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
                		"tag_code:%d data_block_length:%d\n",
                		tag_code,data_block_length);
                if (( p_data_u.puc_data_block + data_block_length) > puc_long_descriptors) {
                    MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
                    		"EDID -> V Descriptor Overflow\n");
                    return EDID_V_DESCR_OVERFLOW;
                }

				i = 0;  /* num of short video descriptors in current data block */

				switch (tag_code)
				{
				case DBTC_VIDEO_DATA_BLOCK:
					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
						"DBTC_VIDEO_DATA_BLOCK:\n");
					if (mhl_edid_3d_data->parse_data.num_video_data_blocks < NUM_VIDEO_DATA_BLOCKS_LIMIT) {
						mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.num_video_data_blocks] = (p_video_data_block_t)p_data_u.puc_data_block;

						/* each SVD is 1 byte long */ 
						while (i < data_block_length) {
							uint8_t VIC;
							VIC = mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.num_video_data_blocks]->short_descriptors[i].VIC;
							MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
									"short desc[%d]: VIC: %d\n",i,VIC);
//							update_av_info_edid(true, VIC, 0);
							mhl_event_notify(mhl_edid_3d_data->dev_context, MHL_TX_EVENT_EDID_UPDATE, VIC, NULL);

							if (!IsQualifiedMhlVIC(mhl_edid_3d_data,VIC,NULL)) {
								mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.num_video_data_blocks]->short_descriptors[i].VIC = 0;
							}
							i++;
						}
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"EDID -> %d descriptors in Short Descriptor Video Block\n",i);
						mhl_edid_3d_data->parse_data.num_video_data_blocks++;
					}
					break;

				case DBTC_AUDIO_DATA_BLOCK:
					{
						Paudio_data_block_t p_audio_data_block = (Paudio_data_block_t) p_data_u.puc_data_block;
						uint8_t A_descriptor_index = 0;  
						uint8_t AudioFormatCode;
						while (i < data_block_length/sizeof(p_audio_data_block->short_audio_descriptors[0]) ) {
							mhl_edid_3d_data->parse_data.audio_descriptors[A_descriptor_index]
								= p_audio_data_block->short_audio_descriptors[i];

						pr_info("\n(uint8_t)p_audio_data_block->short_audio_descriptors[%d].max_channels_minus_one: %d",i,(uint8_t)p_audio_data_block->short_audio_descriptors[i].max_channels_minus_one);
							
						AudioFormatCode =p_audio_data_block->short_audio_descriptors[i].audio_format_code;
						pr_info("\nAudio Format Code %i",(int)AudioFormatCode);
						switch (AudioFormatCode){
							case 1: pr_info("Liniar PCM"); break;
							case 2: pr_info("AC-3"); break;
							case 3: pr_info("MPEG-1"); break;
							case 4: pr_info("MP3"); break;
							case 5: pr_info("MPEG2"); break;
							case 6: pr_info("ACC"); break;
							case 7: pr_info("DTS"); break;
							case 8: pr_info("ATRAC"); break;
							default: pr_info("reserved");
						}
						if(AudioFormatCode==1){
							MAX_channel=(uint8_t)p_audio_data_block->short_audio_descriptors[i].max_channels_minus_one+1;
							pr_info("\nMax N of channels %d",MAX_channel);
							pr_info("\nFs: ");

							if(MAX_channel>2){
								MAX_channel=8;
								Cap_MAX_channel = HDMI_CHANNEL_8;
/*
								if(p_audio_data_block->short_audio_descriptors[i].freq_32_Khz){
									pr_info(" AUDIO_32K_8CH");
									siHdmiTx_AudioSel(AUDIO_32K_8CH);
									}
								else  if(p_audio_data_block->short_audio_descriptors[i].freq_44_1_KHz){
									pr_info(" AUDIO_44K_8CH");
									siHdmiTx_AudioSel(AUDIO_44K_8CH);
									}
								 else  if(p_audio_data_block->short_audio_descriptors[i].freq_48_KHz){
									pr_info(" AUDIO_48K_8CH ");
									siHdmiTx_AudioSel(AUDIO_48K_8CH);
									}
								 else  if(p_audio_data_block->short_audio_descriptors[i].freq_96_KHz){
									pr_info(" AUDIO_96K_8CH");
									siHdmiTx_AudioSel(AUDIO_96K_8CH);
									}
								 else  if(p_audio_data_block->short_audio_descriptors[i].freq_192_KHz){
									pr_info(" AUDIO_192K_8CH ");
									siHdmiTx_AudioSel(AUDIO_192K_8CH);
									Cap_SampleRate = 192;
									}
*/
								 if(p_audio_data_block->short_audio_descriptors[i].freq_32_Khz){
									pr_info(" AUDIO_32K_8CH");
									siHdmiTx_AudioSel(AUDIO_32K_8CH);
									Cap_SampleRate = HDMI_SAMPLERATE_32;
									}
								 if(p_audio_data_block->short_audio_descriptors[i].freq_44_1_KHz){
									pr_info(" AUDIO_44K_8CH");
									siHdmiTx_AudioSel(AUDIO_44K_8CH);
									Cap_SampleRate = HDMI_SAMPLERATE_44;
									}
								 if(p_audio_data_block->short_audio_descriptors[i].freq_48_KHz){
									pr_info(" AUDIO_48K_8CH ");
									Cap_SampleRate = HDMI_SAMPLERATE_48;
									siHdmiTx_AudioSel(AUDIO_48K_8CH);
									}
								 if(p_audio_data_block->short_audio_descriptors[i].freq_96_KHz){
									pr_info(" AUDIO_96K_8CH");
									siHdmiTx_AudioSel(AUDIO_96K_8CH);
									Cap_SampleRate = HDMI_SAMPLERATE_96;
									}
								 if(p_audio_data_block->short_audio_descriptors[i].freq_192_KHz){
									pr_info(" AUDIO_192K_8CH ");
									siHdmiTx_AudioSel(AUDIO_192K_8CH);
									Cap_SampleRate = HDMI_SAMPLERATE_192;
									}
							}
							else{
								MAX_channel=2;
								Cap_MAX_channel = HDMI_CHANNEL_2;
/*
								if(p_audio_data_block->short_audio_descriptors[i].freq_32_Khz){
									pr_info(" AUDIO_32K_2CH");
									siHdmiTx_AudioSel(AUDIO_32K_2CH);
									}
								else  if(p_audio_data_block->short_audio_descriptors[i].freq_44_1_KHz){
									pr_info(" AUDIO_44K_2CH");
									siHdmiTx_AudioSel(AUDIO_44K_2CH);
									}
								else  if(p_audio_data_block->short_audio_descriptors[i].freq_48_KHz){
									pr_info(" AUDIO_48K_2CH ");
									siHdmiTx_AudioSel(AUDIO_48K_2CH);
									}
								else  if(p_audio_data_block->short_audio_descriptors[i].freq_96_KHz){
									pr_info(" AUDIO_96K_2CH");
									siHdmiTx_AudioSel(AUDIO_96K_2CH);
									}
								else  if(p_audio_data_block->short_audio_descriptors[i].freq_192_KHz){
									pr_info(" AUDIO_192K_2CH ");
									siHdmiTx_AudioSel(AUDIO_192K_2CH);
									}
*/
								if(p_audio_data_block->short_audio_descriptors[i].freq_32_Khz){
									pr_info(" AUDIO_32K_2CH");
									siHdmiTx_AudioSel(AUDIO_32K_2CH);
									Cap_SampleRate = HDMI_SAMPLERATE_32;
									}
								if(p_audio_data_block->short_audio_descriptors[i].freq_44_1_KHz){
									pr_info(" AUDIO_44K_2CH");
									siHdmiTx_AudioSel(AUDIO_44K_2CH);
									Cap_SampleRate = HDMI_SAMPLERATE_44;
									}
								if(p_audio_data_block->short_audio_descriptors[i].freq_48_KHz){
									pr_info(" AUDIO_48K_2CH ");
									siHdmiTx_AudioSel(AUDIO_48K_2CH);
									Cap_SampleRate = HDMI_SAMPLERATE_48;
									}
								if(p_audio_data_block->short_audio_descriptors[i].freq_96_KHz){
									pr_info(" AUDIO_96K_2CH");
									siHdmiTx_AudioSel(AUDIO_96K_2CH);
									Cap_SampleRate = HDMI_SAMPLERATE_96;
									}
								if(p_audio_data_block->short_audio_descriptors[i].freq_192_KHz){
									pr_info(" AUDIO_192K_2CH ");
									siHdmiTx_AudioSel(AUDIO_192K_2CH);
									Cap_SampleRate = HDMI_SAMPLERATE_192;
									}
							}
							if(AudioFormatCode == 1){
								pr_info("Supported length: ");

							if(p_audio_data_block->short_audio_descriptors[i].byte3.audio_code_1_LPCM.res_16_bit){
								pr_info("16bits ");
								Samplebit=16;
								Cap_Samplebit = HDMI_BITWIDTH_16;
								}						
							 if(p_audio_data_block->short_audio_descriptors[i].byte3.audio_code_1_LPCM.res_20_bit){
								pr_info("20 ");
								Samplebit=20;
								Cap_Samplebit = HDMI_BITWIDTH_20;
								}
							 if(p_audio_data_block->short_audio_descriptors[i].byte3.audio_code_1_LPCM.res_24_bit){
								 pr_info("24bits ");
								 Samplebit=24;
								 Cap_Samplebit = HDMI_BITWIDTH_24;
								}
								pr_info("\n");
							}
						}
							A_descriptor_index++;
							i++;
						}
						MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
								"EDID -> Short Descriptor Audio Block\n");
					}
					break;

				case DBTC_SPEAKER_ALLOCATION_DATA_BLOCK:
					{
						uint8_t  p_speaker_payload;
						CA = 0;
						Pspeaker_allocation_data_block_t p_speaker_allocation_data_block = (Pspeaker_allocation_data_block_t) p_data_u.puc_data_block;

						*((Pspeaker_allocation_flags_t)&mhl_edid_3d_data->parse_data.speaker_alloc[i++]) = p_speaker_allocation_data_block->payload.speaker_alloc_flags;
						p_speaker_payload = mhl_edid_3d_data->parse_data.speaker_alloc[i-1];
						pr_info("EDID -> Short Descriptor Speaker Allocation Block\n");
						{
							pr_info("\nSpeakers' allocation: ");
							if(p_speaker_payload & 0x01){
								CA+=0;
								pr_info("FL/FR");
								}
							if( p_speaker_payload& 0x02){
								CA+=0x01;
								pr_info("LFE");
								}
							if( p_speaker_payload & 0x04){
								CA+=0x02;
								pr_info("FC");
								}
							if(p_speaker_payload & 0x08){
								CA+=0x08;
								pr_info("RL/RR");
							}
							if(p_speaker_payload & 0x10){
								CA+=0x04;
								pr_info("RC");
							}
							if(p_speaker_payload& 0x20){
								CA+=0x14;
								pr_info("FLC/FRC");
								}
							if(p_speaker_payload & 0x40){
								CA+=0x08;
								pr_info("RLC/RRC");
								}
							pr_info("\nCA=0x%x\n",CA);
						}
					}
					break;

                case DBTC_USE_EXTENDED_TAG:
                    {
                    extended_tag_code_t extended_tag_code;
                        extended_tag_code = p_data_u.p_CEA_data_block->payload_u.extended_tag; 
                        switch (extended_tag_code.etc)
                        {
                            case ETC_VIDEO_CAPABILITY_DATA_BLOCK:
                                {
                                Pvideo_capability_data_block_t p_video_capability_data_block = (Pvideo_capability_data_block_t)p_data_u.puc_data_block;
                                Pvideo_capability_data_payload_t p_payload = &p_video_capability_data_block->payload;
                                mhl_edid_3d_data->parse_data.video_capability_flags = *((uint8_t *)p_payload);
                                mhl_edid_3d_data->parse_data.p_video_capability_data_block = p_video_capability_data_block;
                					MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
                							"EDID -> Short Descriptor Video Capability Block\n");
                                }
            					break;

							case ETC_COLORIMETRY_DATA_BLOCK:
								{
									Pcolorimetry_data_block_t p_colorimetry_data_block = (Pcolorimetry_data_block_t)p_data_u.puc_data_block;
									Pcolorimetry_data_payload_t p_payload= &p_colorimetry_data_block->payload;
									mhl_edid_3d_data->parse_data.colorimetry_support_flags = p_payload->ci_data.xvYCC;
									mhl_edid_3d_data->parse_data.meta_data_profile         = p_payload->cm_meta_data.meta_data;
								}

								MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
										"EDID -> Short Descriptor Colorimetry Block\n");
								break;
						}
					}

					break;

                case DBTC_VENDOR_SPECIFIC_DATA_BLOCK:
                    {
                    P_vsdb_t p_vsdb = (P_vsdb_t) p_data_u.puc_data_block;
                    uint8_t *puc_next_db = ((uint8_t *)&p_vsdb->header) + sizeof(p_vsdb->header) + data_block_length;

		    // TODO: FD, TBI, any chance of MHL OUI here? 0x030C00 is HDMI OUI
		    if (   (p_vsdb->IEEE_OUI[0] == 0x03)
				    && (p_vsdb->IEEE_OUI[1] == 0x0C)
				    && (p_vsdb->IEEE_OUI[2] == 0x00)
		       ) {
			    PHDMI_LLC_vsdb_payload_t p_HDMI_vendor_specific_payload = &p_vsdb->payload_u.HDMI_LLC;
			    mhl_edid_3d_data->parse_data.p_HDMI_vsdb = p_vsdb;
			    SII_ASSERT (5 <= data_block_length,("unexpected data_block_length\n"));
			    mhl_edid_3d_data->parse_data.HDMI_sink = true;
			    *((PHDMI_LLC_BA_t)&mhl_edid_3d_data->parse_data.CEC_A_B) = p_HDMI_vendor_specific_payload->B_A;   /* CEC Physical address */
			    *((PHDMI_LLC_DC_t)&mhl_edid_3d_data->parse_data.CEC_C_D) = p_HDMI_vendor_specific_payload->D_C;
			    /* Offset of 3D_Present bit in VSDB */
			    if (p_HDMI_vendor_specific_payload->byte8.latency_fields_present) {
				    if(p_HDMI_vendor_specific_payload->byte8.I_latency_fields_present) {
					    mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15= &p_HDMI_vendor_specific_payload->vsdb_fields_byte_9_through_byte_15.vsdb_all_fields_byte_9_through_byte_15.byte_13_through_byte_15;
				    } else {
					    mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15 = &p_HDMI_vendor_specific_payload->vsdb_fields_byte_9_through_byte_15.vsdb_all_fields_byte_9_through_byte_15_sans_interlaced_latency.byte_13_through_byte_15;
				    }
			    } else {
				    if(p_HDMI_vendor_specific_payload->byte8.I_latency_fields_present) {
					    mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15 = &p_HDMI_vendor_specific_payload->vsdb_fields_byte_9_through_byte_15.vsdb_all_fields_byte_9_through_byte_15_sans_progressive_latency.byte_13_through_byte_15;
				    } else {
					    mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15 = &p_HDMI_vendor_specific_payload->vsdb_fields_byte_9_through_byte_15.vsdb_all_fields_byte_9_through_byte_15_sans_all_latency.byte_13_through_byte_15;
				    }
			    }
			    if  ( ((uint8_t *)&mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte13) >= puc_next_db ) {
				    mhl_edid_3d_data->parse_data._3D_supported = false;
			    } else if (mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte13._3D_present) {
				    mhl_edid_3d_data->parse_data._3D_supported = true;
			    } else {
				    mhl_edid_3d_data->parse_data._3D_supported = false;
			    }

			    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					    "EDID indicates %s3D support\n",
					    mhl_edid_3d_data->parse_data._3D_supported?"":"NO " );
		    } else {
			    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"check VS IEEE code failed!\n");
			    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"00:%02X, 01:%02X, 02:%02X\n", p_vsdb->IEEE_OUI[0], p_vsdb->IEEE_OUI[1],p_vsdb->IEEE_OUI[2]);
			    mhl_edid_3d_data->parse_data.HDMI_sink = false;
		    }

		    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
				    "EDID -> Short Descriptor Vendor Block\n\n");
		    }

                    break;
                case DBTC_TERMINATOR:
                    MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
                    		"found terminator tag code\n");
				    return EDID_SHORT_DESCRIPTORS_OK;
                    break;

				default:
					MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
							"EDID -> Unknown Tag Code:0x%02x\n",(uint16_t)tag_code);
					return EDID_UNKNOWN_TAG_CODE;

				}
				p_data_u.puc_data_block += sizeof(p_data_u.p_CEA_data_block->header)+ data_block_length;   
			}

			return EDID_SHORT_DESCRIPTORS_OK;
		}
	}
}

static uint8_t parse_861_block(edid_3d_data_p mhl_edid_3d_data,uint8_t *p_EDID_block_data)
{

	uint8_t err_code;
	PCEA_extension_t p_CEA_extension = (PCEA_extension_t)p_EDID_block_data;

	mhl_edid_3d_data->parse_data.p_HDMI_vsdb = NULL;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"tag:place holder EDID block:%p\n",p_EDID_block_data);			
	if (EDID_EXTENSION_BLOCK_MAP == p_CEA_extension->tag) {
		Pblock_map_t p_block_map = (Pblock_map_t)p_EDID_block_data;
		int i;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"Edid: Block Map\n");
		/* loop limit is adjusted by one to account for block map */
		for(i=0;i<mhl_edid_3d_data->parse_data.num_EDID_extensions-1;++i) {
			if (EDID_EXTENSION_TAG != p_block_map->block_tags[i]) {
				MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"Edid: Adjusting number of extensions according to Block Map\n");
				mhl_edid_3d_data->parse_data.num_EDID_extensions=i; /* include block map in count */
				break;
			}
		}

		return EDID_OK;

	} else {
		err_code = parse_861_short_descriptors(mhl_edid_3d_data,p_EDID_block_data);
		if (err_code != EDID_SHORT_DESCRIPTORS_OK) {
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
					"EDID: Non-HDMI extension Errcode:%d\n",(uint16_t)err_code);
			return err_code;
		}
#if 0
		/* adjust */
		err_code = si_mhl_tx_parse_861_long_descriptors(mhl_edid_3d_data,p_EDID_block_data);
		if (err_code != EDID_LONG_DESCRIPTORS_OK) {
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
					"EDID: Errcode:%d\n",(uint16_t)err_code);
			return err_code;
		}
#endif
	}    
	return EDID_OK;
}

void si_mhl_tx_handle_atomic_hw_edid_read_complete(edid_3d_data_p mhl_edid_3d_data,struct cbus_req *req)
{
	PEDID_block0_t p_EDID_block_0 = (PEDID_block0_t)&mhl_edid_3d_data->EDID_block_data[0];
	uint8_t counter;
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"tag: Entire EDID Read complete\n");
#ifdef EDID_PASSTHROUGH //(
	si_mhl_tx_drv_set_upstream_edid(mhl_edid_3d_data->drv_context,mhl_edid_3d_data->EDID_block_data,2*EDID_BLOCK_SIZE);
#endif //)
	/* Parse EDID Block #0 Desctiptors */
	//si_mhl_tx_parse_block_zero_timing_descriptors(mhl_edid_3d_data,p_EDID_block_0);

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context
			,"EDID -> Number of 861 Extensions = %d\n"
			, (uint16_t)p_EDID_block_0->extension_flag );

	mhl_edid_3d_data->parse_data.num_EDID_extensions = p_EDID_block_0->extension_flag;
	if (0 == p_EDID_block_0->extension_flag) {
		/* No extensions to worry about */
		DUMP_EDID_BLOCK(0,(uint8_t *)p_EDID_block_0,EDID_BLOCK_SIZE)
		MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context
					,"EDID -> no extensions, assuming DVI. tag offset:0x%x\n"
					,  SII_OFFSETOF(EDID_block0_t,extension_flag));
		mhl_edid_3d_data->parse_data.HDMI_sink = false;
		SiiMhlTxMakeItDVI(mhl_edid_3d_data,p_EDID_block_0);
	} else {
		uint8_t Result = EDID_OK;
		MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context," tag:place holder \n");			
		/* number of extensions is one less than number of blocks */
		for (counter = 1; counter <= mhl_edid_3d_data->parse_data.num_EDID_extensions;++counter) {
			MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context," counter:%d tag:place holder EDID block:%p\n",counter,&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE * counter]);			
			Result = parse_861_block(mhl_edid_3d_data,&mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE * counter]);
			if (EDID_OK != Result) {
				MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context
					,"EDID -> Extension[%d] is not HDMI: Result:%d\n"
					, counter
					,(uint16_t)Result);
				SiiMhlTxMakeItDVI(mhl_edid_3d_data,p_EDID_block_0);
				Result = EDID_OK;
			}
		}
	}
	/*
	 * Since our working copy of the block zero EDID gets modified,
	 * 		we must re-compute its checksum
	 */
	p_EDID_block_0->checksum = 0;
	p_EDID_block_0->checksum = calculate_generic_checksum((uint8_t *)p_EDID_block_0,0,sizeof(*p_EDID_block_0));

	//SiiMhlTx3dReqForNonTranscodeMode(mhl_edid_3d_data);
}

/*
		EXPORTED FUNCTIONS
*/

void si_mhl_tx_initiate_edid_sequence(void *context)
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;

	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"tag:\n");
	mhl_edid_3d_data->parse_data.num_video_data_blocks=0;

	/*
	   Initiate the EDID reading sequence see
	   SiiMhlTxMscCommandDone for additional processing.
	 */

	si_edid_reset(mhl_edid_3d_data);
	si_mhl_tx_request_first_edid_block( mhl_edid_3d_data->dev_context);
}

int si_mhl_tx_get_num_cea_861_extensions(void *context, uint8_t block_number)
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;

	PEDID_block0_t p_EDID_block_0 = (PEDID_block0_t) &mhl_edid_3d_data->EDID_block_data;
	uint8_t	limit_blocks = sizeof(mhl_edid_3d_data->EDID_block_data) /
								EDID_BLOCK_SIZE;

	uint8_t *pb_data = &mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE * block_number];
	
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,"block number:%d pb_data:%x\n",block_number,pb_data);

	if (ne_SUCCESS == si_mhl_tx_drv_get_edid_fifo_next_block(mhl_edid_3d_data->drv_context, pb_data)) {
		if (0 == block_number) {
			if (!si_mhl_tx_check_edid_header(mhl_edid_3d_data,p_EDID_block_0)) {
				MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,"EDID -> Incorrect Header pb_data:%x\n",pb_data);
				DUMP_EDID_BLOCK(1,pb_data,sizeof(*p_EDID_block_0))  /* no semicolon needed here */
				if (si_mhl_tx_check_edid_header(mhl_edid_3d_data,(PEDID_block0_t)&mhl_edid_3d_data->EDID_block_data[1])) {
//					return ne_BAD_HEADER_OFFSET_BY_1;
				}
			}
		}
		if (! si_mhl_tx_do_edid_checksum(pb_data)) {
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,"EDID -> Checksum Error pb_data:%x\n",pb_data);
			DUMP_EDID_BLOCK(1,pb_data,EDID_BLOCK_SIZE)  /* no semicolon needed here */
//			return ne_BAD_CHECKSUM;
		}

		if(p_EDID_block_0->extension_flag < limit_blocks) {
			return p_EDID_block_0->extension_flag;
		} else {
			MHL_TX_DBG_ERR(mhl_edid_3d_data->dev_context,
							   "not enough room for %d extension\n",
							   p_EDID_block_0->extension_flag);
			return (int)limit_blocks-1;
		}
	} else {
		return ne_NO_HPD;
	}

}

int si_edid_sink_is_hdmi ( void *context )
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;
	return mhl_edid_3d_data->parse_data.HDMI_sink;
}

int si_edid_quantization_range_selectable( void *context )
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;
	return mhl_edid_3d_data->parse_data.video_capability_flags & 0x80;
}

int si_edid_sink_supports_YCbCr422 ( void *context )
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Mhl2Tx: YCbCr422 support:%s\n",
					mhl_edid_3d_data->parse_data.YCbCr_4_2_2?"Yup":"Nope");
	return mhl_edid_3d_data->parse_data.YCbCr_4_2_2;
}

int si_edid_sink_supports_YCbCr444 ( void *context )
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;
	MHL_TX_EDID_INFO(mhl_edid_3d_data->dev_context,
					"Mhl2Tx: YCbCr444 support:%s\n",
					mhl_edid_3d_data->parse_data.YCbCr_4_4_4?"Yup":"Nope");
	return mhl_edid_3d_data->parse_data.YCbCr_4_4_4;
}

int si_edid_find_pixel_clock_from_HDMI_VIC(void *context, uint8_t vic)
{
	return hdmi_vic_info[vic].pixel_clock_0;
}
int si_edid_find_pixel_clock_from_AVI_VIC(void *context, uint8_t vic)
{
	return calculate_pixel_clock(context
			, (uint16_t)VIC_info[vic].columns
			, (uint16_t)VIC_info[vic].rows
			, (uint32_t)VIC_info[vic].field_rate_in_milliHz
			, vic 
			);
}
int si_edid_read_done(void *context)
{
	edid_3d_data_p mhl_edid_3d_data =(edid_3d_data_p)context;
	return TEST_3D_FLAG(mhl_edid_3d_data,FLAGS_EDID_READ_DONE);
}

void si_edid_reset( edid_3d_data_p mhl_edid_3d_data )
{
	int i;
	uint8_t *pData=(uint8_t *) &mhl_edid_3d_data->parse_data;

	/* clear out EDID parse results */
	for (i=0; i < sizeof(mhl_edid_3d_data->parse_data);++i) {
		pData[i]=0;
	}
	CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_EDID_READ_DONE);
}

void *si_edid_create_context(void *dev_context,void *drv_context)
{
	edid_3d_data_p temp;
	temp = (edid_3d_data_p )kmalloc(sizeof(edid_3d_data_t),GFP_KERNEL);
	if (temp) {
		memset((void *)temp,0,sizeof(*temp));
		temp->dev_context=dev_context;
		temp->drv_context=drv_context;
	}
	return (void *)temp; 
}
void si_edid_destroy_context(void *context)
{
	if (context) {
		memset(context,0,sizeof(edid_3d_data_t));
		kfree(context);
	}
}
#if 1//def ENABLE_EDID_DEBUG_PRINT //(
void dump_EDID_block_impl(const char *pszFunction, int iLineNum,uint8_t override,uint8_t *pData,uint16_t length)
{
    uint16_t i;
    printk("%s:%d EDID DATA:\n",pszFunction,iLineNum);
	for (i = 0; i < length; )
	{
	uint16_t j;
	uint16_t temp = i;
		for(j=0; (j < 16)&& (i<length);++j,++i)
		{
			printk("%02X ", pData[i]);
		}
		printk(" | ");
		for(j=0; (j < 16)&& (temp<length);++j,++temp)
		{
			printk("%c",((pData[temp]>=' ')&&(pData[temp]<='z'))?pData[temp]:'.');
		}
		printk("\n");
	}
}
#endif //)
