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

#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_8620_internal_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"

#include "platform.h"
#include "si_mhl_callback_api.h"
#include "si_8620_drv.h"
#include "mhl_supp.h"

#define SET_3D_FLAG(context, x)				\
	do {						\
		context->parse_data.flags.x = 1;	\
		MHL_TX_EDID_INFO("set %s\n", #x);	\
	} while (0)

#define CLR_3D_FLAG(context, x)				\
	do {						\
		context->parse_data.flags.x = 0;	\
		MHL_TX_EDID_INFO("clr %s\n", #x);	\
	} while (0)

#define TEST_3D_FLAG(context, x) (context->parse_data.flags.x)

struct timing_mode_from_data_sheet_t {
	uint16_t h_total;
	uint16_t v_total;
	uint16_t columns;
	uint16_t rows;
	uint16_t field_rate;
	uint32_t pixel_clock;
	uint8_t mhl3_vic;
	struct {
		uint8_t interlaced:1;
		uint8_t reserved:7;
	} flags;
	char *description;
};

/* note that this table is sorted by:
	columns,
	then by rows,
	then by field_rate,
	then by h_total,
	then by v_total,
	then by pixel_clock.
*/

struct timing_mode_from_data_sheet_t timing_modes[] = {
	{ 832,  445,  640,  350,  85,  31500000,  0, {0, 0}, "640x350-85"},
	{ 832,  445,  640,  400,  85,  31500000,  0, {0, 0}, "640x400-85"},
	{ 800,  525,  640,  480,  60,  25175000,  0, {0, 0}, "VGA60"},
	{ 864,  525,  640,  480,  66,  29937600,  0, {0, 0}, "640x480-66"},
	{ 832,  520,  640,  480,  72,  31500000,  0, {0, 0}, "VGA72"},
	{ 840,  500,  640,  480,  75,  31500000,  0, {0, 0}, "VGA75"},
	{ 832,  509,  640,  480,  85,  36000000,  0, {0, 0}, "VGA85"},
	{1716,  262,  720,  240,  60,  27000000,  6, {1, 0}, "480i"},
	{1728,  312,  720,  288,  60,  27000000, 21, {1, 0}, "576i"},
	{ 936,  446,  720,  400,  85,  35500000,  0, {0, 0}, "720x400"},
	{ 858,  525,  720,  480,  60,  27000000,  2, {0, 0}, "480p"},
	{ 864,  625,  720,  576,  50,  27000000, 17, {0, 0}, "576p"},
	{1024,  625,  800,  600,  56,  36000000,  0, {0, 0}, "SVGA56"},
	{1056,  628,  800,  600,  60,  40000000,  0, {0, 0}, "SVGA60"},
	{1040,  666,  800,  600,  72,  50000000,  0, {0, 0}, "SVGA72"},
	{1056,  625,  800,  600,  75,  49500000,  0, {0, 0}, "SVGA75"},
	{1048,  631,  800,  600,  85,  56250000,  0, {0, 0}, "SVGA85"},
	{ 960,  636,  800,  600, 120,  73250000,  0, {0, 0}, "SVGA120RB"},
	{1120,  654,  832,  624,  75,  54936000,  0, {0, 0}, "832x624-75"},
	{1088,  517,  848,  480,  60,  33750000,  0, {0, 0}, "WVGA"},
	{1072,  529,  852,  480,  60,  34025280,  0, {0, 0}, "852x480-60"},
	{1264,  408, 1024,  384,  87,  44900000,  0, {1, 0}, "XGA87i"},
	{1320,  596, 1024,  576,  60,  47203200,  0, {0, 0}, "1024x576-60"},
	{1344,  806, 1024,  768,  60,  65000000,  0, {0, 0}, "XGA"},
	{1328,  806, 1024,  768,  70,  75000000,  0, {0, 0}, "XGA70"},
	{1328,  804, 1024,  768,  74,  79010688,  0, {0, 0}, "1024x768-74"},
	{1312,  800, 1024,  768,  75,  78750000,  0, {0, 0}, "XGA75"},
	{1376,  808, 1024,  768,  85,  94500000,  0, {0, 0}, "XGA85"},
	{1184,  813, 1024,  768, 120, 115500000,  0, {0, 0}, "XGA120RB"},
	{1600,  900, 1152,  864,  75, 108000000,  0, {0, 0}, "1152x864-75"},
	{1456,  915, 1152,  870,  75,  99918000,  0, {0, 0}, "1152x870-75"},
	{1696,  750, 1280,  720,  59,  75048000,  0, {0, 0}, "1280x720-59"},
	{1650,  750, 1280,  720,  60,  74250000,  4, {0, 0}, "720p"},
	{3300,  750, 1280,  720,  24,  59400000,  60, {0, 0}, "720p-24"},
	{3960,  750, 1280,  720,  25,  74250000,  61, {0, 0}, "720p-25"},
	{3300,  750, 1280,  720,  30,  74250000,  62, {0, 0}, "720p-30"},
	{1440,  790, 1280,  768,  60,  68250000,  0, {0, 0}, "1280x768-60RB"},
	{1664,  798, 1280,  768,  60,  79500000,  0, {0, 0}, "1280x768-60"},
	{1696,  805, 1280,  768,  75, 102250000,  0, {0, 0}, "1280x768-75"},
	{1712,  809, 1280,  768,  85, 117500000,  0, {0, 0}, "1280x768-85"},
	{1440,  813, 1280,  768, 120, 140250000,  0, {0, 0}, "1280x768-120RB"},
	{1440,  823, 1280,  800,  60,  71000000,  0, {0, 0}, "1280x8000RB"},
	{1680,  831, 1280,  800,  60,  83500000,  0, {0, 0}, "1280x800-60"},
	{1696,  838, 1280,  800,  75, 106550000,  0, {0, 0}, "1280x800-75"},
	{1712,  843, 1280,  800,  85, 122500000,  0, {0, 0}, "1280x800-85"},
	{1440,  847, 1280,  800, 120, 146250000,  0, {0, 0}, "1280x800-120RB"},
	{1800, 1000, 1280,  960,  60, 108000000,  0, {0, 0}, "1280x960-60"},
	{1728, 1011, 1280,  960,  85, 148500000,  0, {0, 0}, "1280x960-85"},
	{1440, 1017, 1280,  960, 120, 175500000,  0, {0, 0}, "1280x960-120RB"},
	{1688, 1066, 1280, 1024,  60, 108000000,  0, {0, 0}, "1280x1024-60"},
	{1688, 1066, 1280, 1024,  75, 135000000,  0, {0, 0}, "1280x1024-75"},
	{1728, 1072, 1280, 1024,  85, 157500000,  0, {0, 0}, "1280x1024-85"},
	{1760, 1082, 1280, 1024,  95, 180910400,  0, {0, 0}, "1280x1024-95"},
	{1440, 1084, 1280, 1024, 120, 187250000,  0, {0, 0}, "1280x1024-120RB"},
	{1792,  795, 1360,  768,  60,  85500000,  0, {0, 0}, "1360x768-60"},
	{1520,  813, 1360,  768, 120, 148250000,  0, {0, 0}, "1360x768-120RB"},
	{1840, 1087, 1365, 1024,  59, 118004720,  0, {0, 0}, "1365x1024-59"},
	{1800, 1065, 1365, 1024,  75, 143775000,  0, {0, 0}, "1365x1024-75"},
	{1500,  800, 1366,  768,  60,  72000000,  0, {0, 0}, "1366x768-60RB"},
	{1792,  798, 1366,  768,  60,  85500000,  0, {0, 0}, "1366x768-60"},
	{1800,  850, 1400,  788,  59,  90270000,  0, {0, 0}, "1400x788-59"},
	{1864, 1089, 1400, 1050,  59, 119763864,  0, {0, 0}, "1400x1050-59"},
	{1600,  926, 1440,  900,  60,  88750000,  0, {0, 0}, "1440x900-60RB"},
	{1904,  934, 1440,  900,  60, 106500000,  0, {0, 0}, "1440x900-60"},
	{1936,  942, 1440,  900,  75, 136750000,  0, {0, 0}, "1440x900-75"},
	{1952,  948, 1440,  900,  85, 157000000,  0, {0, 0}, "1440x900-85"},
	{1600,  953, 1440,  900, 120, 182750000,  0, {0, 0}, "1440x900-120RB"},
	{1792, 1000, 1440,  960,  71, 127232000,  0, {0, 0}, "1440x960-71"},
	{1560, 1080, 1440, 1050,  60, 101000000,  0, {0, 0}, "1440x1050-60RB"},
	{1864, 1089, 1440, 1050,  60, 121750000,  0, {0, 0}, "1440x1050-60"},
	{1896, 1099, 1440, 1050,  75, 156000000,  0, {0, 0}, "1440x1050-75"},
	{1912, 1105, 1440, 1050,  85, 179500000,  0, {0, 0}, "1440x1050-85"},
	{1560, 1112, 1440, 1050, 120, 208000000,  0, {0, 0}, "1440x1050-120RB"},
	{1800, 1000, 1600,  900,  60, 108000000,  0, {0, 0}, "1600x900-60RB"},
	{2144, 1060, 1600, 1024,  59, 134085760,  0, {0, 0}, "1600x1024-59"},
	{1840, 1080, 1600, 1050,  60, 119000000,  0, {0, 0}, "1600x1050-60RB"},
	{2240, 1089, 1600, 1050,  60, 146250000,  0, {0, 0}, "1600x1050-60"},
	{2272, 1099, 1600, 1050,  75, 187000000,  0, {0, 0}, "1600x1050-75"},
	{2288, 1105, 1600, 1050,  85, 214750000,  0, {0, 0}, "1600x1050-85"},
	{1840, 1112, 1600, 1050, 120, 245500000,  0, {0, 0}, "1600x1050-120RB"},
	{2160, 1250, 1600, 1200,  60, 162000000,  0, {0, 0}, "1600x1200"},
	{2160, 1250, 1600, 1200,  65, 175500000,  0, {0, 0}, "1600x1200-65"},
	{2160, 1250, 1600, 1200,  70, 189000000,  0, {0, 0}, "1600x1200-70"},
	{2160, 1250, 1600, 1200,  75, 202500000,  0, {0, 0}, "1600x1200-75"},
	{2160, 1250, 1600, 1200,  85, 229500000,  0, {0, 0}, "1600x1200-85"},
	{1760, 1271, 1600, 1200, 120, 245500000,  0, {0, 0}, "1600x1200-120RB"},
	{2240, 1089, 1680, 1050,  59, 143922240,  0, {0, 0}, "1680x1050-59"},
	{2448, 1394, 1792, 1344,  60, 204750000,  0, {0, 0}, "1792x1344-60"},
	{2456, 1417, 1792, 1344,  75, 261000000,  0, {0, 0}, "1792x1344-75"},
	{1952, 1423, 1792, 1344, 120, 333250000,  0, {0, 0}, "1792x1344-120RB"},
	{2528, 1439, 1856, 1392,  60, 218250000,  0, {0, 0}, "1856x1392-60"},
	{2560, 1500, 1856, 1392,  75, 288000000,  0, {0, 0}, "1856x1392-75"},
	{2016, 1474, 1856, 1392, 120, 356500000,  0, {0, 0}, "1856x1392-120RB"},
	{2200,  562, 1920,  540,  60,  74250000,  5, {1, 0}, "1080i"},
	{2750, 1125, 1920, 1080,  24,  74250000, 32, {0, 0}, "1080p24"},
	{2750, 1125, 1920, 1080,  30,  74250000, 34, {0, 0}, "1080p30"},
	{2640, 1125, 1920, 1080,  50, 148500000, 20, {0, 0}, "1080p50"},
	{2080, 1111, 1920, 1080,  59, 136341920,  0, {0, 0}, "1920x1080-59"},
	{2200, 1125, 1920, 1080,  60, 148500000, 16, {0, 0}, "1080p60"},
	{2080, 1235, 1920, 1200,  60, 154000000,  0, {0, 0}, "1920x1200-60RB"},
	{2592, 1245, 1920, 1200,  60, 193250000,  0, {0, 0}, "1920x1200-60"},
	{2608, 1255, 1920, 1200,  75, 245250000,  0, {0, 0}, "1920x1200-75"},
	{2624, 1262, 1920, 1200,  85, 281250000,  0, {0, 0}, "1920x1200-85"},
	{2080, 1271, 1920, 1200, 120, 317000000,  0, {0, 0}, "1920x1200-120RB"},
	{2600, 1500, 1920, 1440,  60, 234000000,  0, {0, 0}, "1920x1440-60"},
	{2640, 1500, 1920, 1440,  75, 297000000,  0, {0, 0}, "1920x1440-75"},
	{2080, 1525, 1920, 1440, 120, 380500000,  0, {0, 0}, "1920x1440-120RB"},
	{2250, 1200, 2048, 1152,  60, 162000000,  0, {0, 0}, "2048x1152-60RB"},
	{2784, 1325, 2048, 1280,  60, 221328000,  0, {0, 0}, "2048x1280-60"},
	{2720, 1646, 2560, 1600,  60, 268500000,  0, {0, 0}, "2560x1600-60RB"},
	{3504, 1658, 2560, 1600,  60, 348500000,  0, {0, 0}, "2560x1600-60"},
	{3536, 1672, 2560, 1600,  75, 443250000,  0, {0, 0}, "2560x1600-75"},
	{3536, 1682, 2560, 1600,  85, 505250000,  0, {0, 0}, "2560x1600-85"},
	{2720, 1694, 2560, 1600, 120, 552750000,  0, {0, 0}, "2560x1600-120RB"},
	{4400, 2250, 3840, 2160,  30, 297000000, 95, {0, 0}, "3840x2160-30"},
	{5280, 2250, 3840, 2160,  25, 297000000, 94, {0, 0}, "3840x2160-25"},
	{5500, 2250, 3840, 2160,  24, 296703000, 93, {0, 0}, "3840x2160-24"},
	{5500, 2250, 4096, 2160,  24, 297000000, 98, {0, 0},
		"4096x2160-24 SMPTE"}
};


void display_timing_enumeration_callback(
	struct edid_3d_data_t *mhl_edid_3d_data,
	uint16_t columns, uint16_t rows, uint8_t bits_per_pixel,
	uint32_t vertical_refresh_rate_in_milliHz, uint16_t burst_id,
	union video_burst_descriptor_u *p_descriptor)
{
	struct MHL2_video_descriptor_t *pMHL2_video_descriptor;
	struct MHL3_hev_vic_descriptor_t *hev_vic_descriptor;
	struct MHL3_hev_dtd_item_t *dtd_payload;

	if (p_descriptor) {
		struct drv_hw_context *hw_context =
		    mhl_edid_3d_data->drv_context;
		switch (burst_id) {
		case burst_id_3D_VIC:
		case burst_id_3D_DTD:
			{
				pMHL2_video_descriptor =
				    &p_descriptor->mhl2_3d_descriptor;
				MHL_TX_EDID_INFO("%4d x %4d, %2d bpp at %u Hz "
				     "3D - %16s %16s %16s\n\n",
				     columns, rows, (uint16_t) bits_per_pixel,
				     vertical_refresh_rate_in_milliHz,
				     pMHL2_video_descriptor->left_right ?
						"Left/Right" : "",
				     pMHL2_video_descriptor->top_bottom ?
						"Top/Bottom" : "",
				     pMHL2_video_descriptor->frame_sequential ?
						"Frame Sequential" : "");
			}
			break;
		case burst_id_HEV_VIC:
			{
				hev_vic_descriptor =
				    &p_descriptor->mhl3_hev_vic_descriptor;
				MHL_TX_EDID_INFO
				    ("%4d x %4d, %2d bpp at %u Hz VIC: %d\n\n",
				     columns, rows, (uint16_t) bits_per_pixel,
				     vertical_refresh_rate_in_milliHz,
				     hev_vic_descriptor->vic_cea861f);
			}
			break;
		case burst_id_HEV_DTDB:
			{
				dtd_payload = &p_descriptor->mhl3_hev_dtd;
				MHL_TX_EDID_INFO("seq: %d pclk: %9d h_active: "
				     "%5d h_blank: %5d h_fp: %5d h_sw: %5d "
				     "h_flags: 0x%02x\nv_tot: %5d v_blnk: %3d "
				     "v_fp: %3d v_sw: %3d v_fields: %3d\n",
				     p_descriptor->mhl3_hev_dtd.sequence_index,
				     ENDIAN_CONVERT_16(dtd_payload->a.
						       pixel_clock_in_MHz),
				     ENDIAN_CONVERT_16(dtd_payload->a.
						       h_active_in_pixels),
				     ENDIAN_CONVERT_16(dtd_payload->a.
						       h_blank_in_pixels),
				     ENDIAN_CONVERT_16(dtd_payload->a.
						       h_front_porch_in_pixels),
				     ENDIAN_CONVERT_16(dtd_payload->a.
						       h_sync_width_in_pixels),
				     dtd_payload->a.h_flags,
				     ENDIAN_CONVERT_16(dtd_payload->b.
						       v_total_in_lines),
				     dtd_payload->b.v_blank_in_lines,
				     dtd_payload->b.v_front_porch_in_lines,
				     dtd_payload->b.v_sync_width_in_lines,
				     dtd_payload->b.
				     v_refresh_rate_in_fields_per_second,
				     dtd_payload->b.v_flags);
			}
			break;
		}
		hw_context->callbacks.display_timing_enum_item(
			hw_context->callbacks.context,
			columns, rows, bits_per_pixel,
			vertical_refresh_rate_in_milliHz,
			burst_id, p_descriptor);
	} else {
		MHL_TX_EDID_INFO("%4d x %4d, %2d bpp at %u Hz 3D\n\n", columns,
				rows, (uint16_t) bits_per_pixel,
				vertical_refresh_rate_in_milliHz);
	}
}

void display_timing_enumeration_end(struct edid_3d_data_t *mhl_edid_3d_data)
{
	struct drv_hw_context *hw_context = mhl_edid_3d_data->drv_context;
	int vic_3d_idx, dtd_3d_idx, vic_hev_idx, dtd_hev_idx;
	int count_hev, count_3d;

	MHL_TX_DBG_WARN("\n");

	hw_context->callbacks.display_timing_enum_end(
		hw_context->callbacks.context);
	/* MHL3.0 spec requires that,in response to FEAT_REQ, all 3D_VIC and
	 * 3D_DTD bursts shall be sent before all HEV_VIC and HEV_DTD bursts.
	 * Now that we're done with all of the above, pair up the 3D_VIC and
	 * 3D_DTD data with the HEV_VIC and HEV_DTD data.
	 */
	if (NULL == mhl_edid_3d_data->hev_vic_list) {
		MHL_TX_DBG_WARN("no HEV_VICs\n");
	} else if (NULL == mhl_edid_3d_data->_3d_vic_list) {
		MHL_TX_DBG_WARN("no 3D_VICs\n");
		/* do nothing */
	} else if (mhl_edid_3d_data->hev_vic_info.num_items) {
		MHL_TX_DBG_WARN("\n");
		count_hev = mhl_edid_3d_data->hev_vic_info.num_items;
		count_3d = mhl_edid_3d_data->_3d_vic_info.num_items;
		vic_3d_idx = mhl_edid_3d_data->parse_data.vic_2d_index;
		for (vic_hev_idx = 0;
			(vic_hev_idx < count_hev) && (vic_3d_idx < count_3d);
			++vic_hev_idx, ++vic_3d_idx) {
			MHL_TX_DBG_WARN("\n");
			mhl_edid_3d_data->hev_vic_list[vic_hev_idx]._3d_info =
			mhl_edid_3d_data->_3d_vic_list[vic_3d_idx]._3d_info;
		}
	}
	if (NULL == mhl_edid_3d_data->hev_dtd_list) {
		/* do nothing */
		MHL_TX_DBG_WARN("no HEV_DTDs\n");
	} else if (NULL == mhl_edid_3d_data->_3d_dtd_list) {
		MHL_TX_DBG_WARN("no 3D_DTDs\n");
		/* do nothing */
	} else if (mhl_edid_3d_data->hev_dtd_info.num_items) {
		MHL_TX_DBG_WARN(
		    "num_vesa_timing_dtds:0x%x num_cea_861_timing_dtds:0x%x\n",
		    mhl_edid_3d_data->parse_data.num_vesa_timing_dtds,
		    mhl_edid_3d_data->parse_data.num_cea_861_timing_dtds);

		count_hev = mhl_edid_3d_data->hev_dtd_info.num_items;
		count_3d = mhl_edid_3d_data->_3d_dtd_info.num_items;
		dtd_3d_idx =
			mhl_edid_3d_data->parse_data.num_vesa_timing_dtds +
			mhl_edid_3d_data->parse_data.num_cea_861_timing_dtds;
		for (dtd_hev_idx = 0;
			(dtd_hev_idx < count_hev) && (dtd_3d_idx < count_3d);
			++dtd_hev_idx, ++dtd_3d_idx) {
			MHL_TX_DBG_WARN(
				"hev_dtd_index:0x%x _3d_dtd_index:0x%x\n",
				dtd_hev_idx, dtd_3d_idx);
			mhl_edid_3d_data->hev_dtd_list[dtd_hev_idx]._3d_info =
				mhl_edid_3d_data->
				_3d_dtd_list[dtd_3d_idx++]._3d_info;
		}
	}
}

static bool field_rate_within_measurement_precision(int table, int measured)
{
int difference;
	/* reference clock varies +/- 5% */
	difference = table - measured;
	difference *= 100;
	difference /= table;
	if ((difference < -5) || (difference > 5))
		return false;
	MHL_TX_DBG_ERR("field rate delta: %d\%\n", difference)
	return true;
}

/*
 * si_mhl_tx_validate_timings_table
 *
 * Returns the number of indistinct timing modes in
 *	timing_modes_from_data_sheet.
 *
 * This is included to sound the alarm if the table contains entries that
 *	cannot be reliably distinguished from each other.
 */
static int validate_timings_table(void)
{
	int match_count = 0;
	unsigned int i, j;

	for (i = 0; i < ARRAY_SIZE(timing_modes); ++i) {
		for (j = i + 1; j < ARRAY_SIZE(timing_modes); ++j) {
			if (timing_modes[i].h_total !=
				timing_modes[j].h_total) {
				continue;
			}
			if (timing_modes[i].v_total !=
				timing_modes[j].v_total) {
				continue;
			}
			if (timing_modes[i].columns !=
				timing_modes[j].columns) {
				continue;
			}
			if (timing_modes[i].rows !=
				timing_modes[j].rows) {
				continue;
			}
			if ( !field_rate_within_measurement_precision(
				timing_modes[i].field_rate,
				timing_modes[j].field_rate))
				continue;

			match_count++;
			MHL_TX_DBG_ERR(
				"%sentry %d:\n\t{%d,%d,%d,%d,%d}\n"
				"\tmatches entry %d:\n\t{%d,%d,%d,%d,%d}%s\n",
				ANSI_ESC_RED_TEXT, i,
				timing_modes[i].h_total,
				timing_modes[i].v_total,
				timing_modes[i].columns,
				timing_modes[i].rows,
				timing_modes[i].field_rate,
				j,
				timing_modes[j].h_total,
				timing_modes[j].v_total,
				timing_modes[j].columns,
				timing_modes[j].rows,
				timing_modes[j].field_rate,
				ANSI_ESC_RESET_TEXT);
		}
	}
	if (match_count) {
		MHL_TX_DBG_ERR(
			"%s%d matching table entries! We can't have this!%s\n",
			ANSI_ESC_RED_TEXT, match_count, ANSI_ESC_RESET_TEXT);
	} else {
		MHL_TX_DBG_WARN("%sAll timing entries distinguishable%s\n",
			ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT);
	}
	return match_count;
}

uint32_t si_mhl_tx_find_timings_from_totals(
	struct edid_3d_data_t *mhl_edid_3d_data,
	struct si_incoming_timing_t *p_timing)
{
	uint32_t ret_val = 0;
	uint8_t i;
	for (i = 0; i < ARRAY_SIZE(timing_modes); ++i) {

		if (timing_modes[i].h_total != p_timing->h_total)
			continue;
		if (timing_modes[i].v_total != p_timing->v_total)
			continue;
		if (timing_modes[i].columns != p_timing->columns)
			continue;
		if (timing_modes[i].rows != p_timing->rows)
			continue;

		if ( !field_rate_within_measurement_precision(
			timing_modes[i].field_rate,
			p_timing->field_rate))
			continue;

		p_timing->calculated_pixel_clock =
			timing_modes[i].h_total *
			timing_modes[i].v_total *
			timing_modes[i].field_rate;

		ret_val = timing_modes[i].pixel_clock;
		p_timing->mhl3_vic = timing_modes[i].mhl3_vic;
		MHL_TX_DBG_ERR("%svic: %d %s%s\n", ANSI_ESC_GREEN_TEXT,
			timing_modes[i].mhl3_vic,
			timing_modes[i].description,
			ANSI_ESC_RESET_TEXT)
		return ret_val;
	}
	MHL_TX_DBG_WARN("VIC was zero!!!\n"
			"\t\th_total:\t%d\n"
			"\t\tv_total:\t%d\n"
			"\t\tcolumns:\t%d\n"
			"\t\t   rows:\t%d\n"
			"\t\tfield_rate:\t%d\n",
			p_timing->h_total, p_timing->v_total,
			p_timing->columns, p_timing->rows,
			p_timing->field_rate);
	return 0;
}

PLACE_IN_CODE_SEG char *psz_space = "n/a";
PLACE_IN_CODE_SEG char *psz_frame_sequential = "FS ";
PLACE_IN_CODE_SEG char *psz_top_bottom = "TB ";
PLACE_IN_CODE_SEG char *psz_left_right = "LR ";

enum cea_image_aspect_ratio_e {
	cea_iar_4_to_3,
	cea_iar_16_to_9,
	cea_iar_64_to_27,
	cea_iar_256_to_135
};

enum VIC_info_flags_e {
	vif_single_frame_rate,
	vif_dual_frame_rate
};

enum VIC_scan_mode_e {
	vsm_progressive,
	vsm_interlaced
};

enum pixel_aspect_ratio_e {
	par_1_to_1,
	par_16_to_15,
	par_16_to_27,
	par_16_to_45,
	par_16_to_45_160_to_45,
	par_1_to_15_10_to_15,
	par_1_to_9_10_to_9,
	par_2_to_15_20_to_15,
	par_2_to_9,
	par_2_to_9_20_to_9,
	par_32_to_27,
	par_32_to_45,
	par_4_to_27_40_to_27,
	par_4_to_9,
	par_4_to_15,
	par_64_to_45,
	par_8_to_15,
	par_8_to_27,
	par_8_to_27_80_to_27,
	par_8_to_45_80_to_45,
	par_8_to_9,
	par_4_to_3,
	par_64_to_63
};

struct SI_PACK_THIS_STRUCT VIC_info_fields_t {
	enum cea_image_aspect_ratio_e image_aspect_ratio:2;
	enum VIC_scan_mode_e interlaced:1;
	enum pixel_aspect_ratio_e pixel_aspect_ratio:5;

	enum VIC_info_flags_e frame_rate_info:1;
	uint8_t clocks_per_pixel_shift_count:2;
	uint8_t field2_v_blank:2;
	uint8_t reserved:3;
};

struct SI_PACK_THIS_STRUCT VIC_info_t {
	uint16_t columns;
	uint16_t rows;
	uint16_t h_blank_in_pixels;
	uint16_t v_blank_in_pixels;
	uint32_t field_rate_in_milliHz;
	struct VIC_info_fields_t fields;
};

struct SI_PACK_THIS_STRUCT HDMI_VIC_info_t {
	uint16_t columns;
	uint16_t rows;
	uint32_t field_rate_0_in_milliHz;
	uint32_t field_rate_1_in_milliHz;
	uint32_t pixel_clock_0;
	uint32_t pixel_clock_1;
	uint8_t corresponding_MHL3_VIC;
};

/* VIC is a place holder, and not actually stored */
#define CEA_861_F_VIC_info_entry(VIC, columns, rows, HBlank, VBLank, \
	FieldRate, image_aspect_ratio, scanmode, PixelAspectRatio, flags, \
	clocksPerPelShift, AdditionalVBlank) \
	{columns, rows, HBlank, VBLank, FieldRate, {image_aspect_ratio, \
	scanmode, PixelAspectRatio, flags, clocksPerPelShift, \
	AdditionalVBlank} }

static struct VIC_info_t VIC_info[] = {
	CEA_861_F_VIC_info_entry(0, 0, 0, 0, 0, 0000, cea_iar_4_to_3,
		vsm_progressive, par_1_to_1, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(1, 640, 480, 160, 45, 60000, cea_iar_4_to_3,
		vsm_progressive, par_1_to_1, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(2, 720, 480, 138, 45, 60000, cea_iar_4_to_3,
		vsm_progressive, par_8_to_9, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(3, 720, 480, 138, 45, 60000, cea_iar_16_to_9,
		vsm_progressive, par_32_to_27, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(4, 1280, 720, 370, 30, 60000, cea_iar_16_to_9,
		vsm_progressive, par_1_to_1, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(5, 1920, 1080, 280, 22, 60000, cea_iar_16_to_9,
		vsm_interlaced, par_1_to_1, vif_dual_frame_rate, 0, 1),
	CEA_861_F_VIC_info_entry(6, 720, 480, 276, 22, 60000, cea_iar_4_to_3,
		vsm_interlaced, par_8_to_9, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(7, 720, 480, 276, 22, 60000, cea_iar_16_to_9,
		vsm_interlaced, par_32_to_27, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(8, 720, 240, 276, 22, 60000, cea_iar_4_to_3,
		vsm_progressive, par_4_to_9, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(9, 720, 428, 276, 22, 60000, cea_iar_16_to_9,
		vsm_progressive, par_16_to_27, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(10, 2880, 480, 552, 22, 60000, cea_iar_4_to_3,
		vsm_interlaced, par_2_to_9_20_to_9, vif_dual_frame_rate, 0, 1),
	CEA_861_F_VIC_info_entry(11, 2880, 480, 552, 22, 60000, cea_iar_16_to_9,
		vsm_interlaced, par_8_to_27_80_to_27, vif_dual_frame_rate, 0,
		1),
	CEA_861_F_VIC_info_entry(12, 2880, 240, 552, 22, 60000, cea_iar_4_to_3,
		vsm_progressive, par_1_to_9_10_to_9, vif_dual_frame_rate, 0, 1),
	CEA_861_F_VIC_info_entry(13, 2880, 240, 552, 22, 60000, cea_iar_16_to_9,
		vsm_progressive, par_4_to_27_40_to_27, vif_dual_frame_rate, 0,
		1),
	CEA_861_F_VIC_info_entry(14, 1440, 480, 276, 45, 60000, cea_iar_4_to_3,
		vsm_progressive, par_4_to_9, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(15, 1440, 480, 276, 45, 60000, cea_iar_16_to_9,
		vsm_progressive, par_16_to_27, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(16, 1920, 1080, 280, 45, 60000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(17, 720, 576, 144, 49, 50000, cea_iar_4_to_3,
		vsm_progressive, par_16_to_15, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(18, 720, 576, 144, 49, 50000, cea_iar_16_to_9,
		vsm_progressive, par_64_to_45, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(19, 1280, 720, 700, 30, 50000, cea_iar_16_to_9,
		vsm_progressive, par_1_to_1, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(20, 1920, 1080, 720, 22, 50000,
		cea_iar_16_to_9, vsm_interlaced, par_1_to_1,
		vif_single_frame_rate, 0, 1),
	CEA_861_F_VIC_info_entry(21, 720, 576, 288, 24, 50000, cea_iar_4_to_3,
		vsm_interlaced, par_16_to_15, vif_single_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(22, 720, 576, 288, 24, 50000, cea_iar_16_to_9,
		vsm_interlaced, par_64_to_45, vif_single_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(23, 720, 288, 288, 24, 50000, cea_iar_4_to_3,
		vsm_progressive, par_8_to_15, vif_single_frame_rate, 1, 2),
	CEA_861_F_VIC_info_entry(24, 720, 288, 288, 24, 50000, cea_iar_16_to_9,
		vsm_progressive, par_32_to_45, vif_single_frame_rate, 1, 2),
	CEA_861_F_VIC_info_entry(25, 2880, 576, 576, 24, 50000, cea_iar_4_to_3,
		vsm_interlaced, par_2_to_15_20_to_15, vif_single_frame_rate, 0,
		1),
	CEA_861_F_VIC_info_entry(26, 2880, 576, 576, 24, 50000, cea_iar_16_to_9,
		vsm_interlaced, par_16_to_45_160_to_45, vif_single_frame_rate,
		0, 1),
	CEA_861_F_VIC_info_entry(27, 2880, 288, 576, 24, 50000, cea_iar_4_to_3,
		vsm_progressive, par_1_to_15_10_to_15, vif_single_frame_rate, 0,
		2),
	CEA_861_F_VIC_info_entry(28, 2880, 288, 576, 24, 50000, cea_iar_16_to_9,
		vsm_progressive, par_8_to_45_80_to_45, vif_single_frame_rate,
		0, 2),
	CEA_861_F_VIC_info_entry(29, 1440, 576, 288, 49, 50000, cea_iar_4_to_3,
		vsm_progressive, par_8_to_15, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(30, 1440, 576, 288, 49, 50000, cea_iar_16_to_9,
		vsm_progressive, par_32_to_45, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(31, 1920, 1080, 720, 45, 50000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(32, 1920, 1080, 830, 45, 24000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(33, 1920, 1080, 720, 45, 25000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(34, 1920, 1080, 280, 45, 30000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(35, 2880, 480, 552, 45, 60000, cea_iar_4_to_3,
		vsm_progressive, par_2_to_9, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(36, 2880, 480, 552, 45, 60000, cea_iar_16_to_9,
		vsm_progressive, par_8_to_27, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(37, 2880, 576, 576, 49, 50000, cea_iar_4_to_3,
		vsm_progressive, par_4_to_15, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(38, 2880, 576, 576, 49, 50000, cea_iar_16_to_9,
		vsm_progressive, par_16_to_45, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(39, 1920, 1080, 384, 85, 50000,
		cea_iar_16_to_9, vsm_interlaced, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(40, 1920, 1080, 720, 22, 100000,
		cea_iar_16_to_9, vsm_interlaced, par_1_to_1,
		vif_single_frame_rate, 0, 1),
	CEA_861_F_VIC_info_entry(41, 1280, 720, 700, 30, 100000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(42, 720, 576, 144, 49, 100000, cea_iar_4_to_3,
		vsm_progressive, par_16_to_15, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(43, 720, 576, 144, 49, 100000, cea_iar_16_to_9,
		vsm_progressive, par_64_to_45, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(44, 720, 576, 288, 24, 100000, cea_iar_4_to_3,
		vsm_interlaced, par_16_to_15, vif_single_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(45, 720, 576, 288, 24, 100000, cea_iar_16_to_9,
		vsm_interlaced, par_64_to_45, vif_single_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(46, 1920, 1080, 280, 22, 120000,
		cea_iar_16_to_9, vsm_interlaced, par_1_to_1,
		vif_dual_frame_rate, 0, 1),
	CEA_861_F_VIC_info_entry(47, 1280, 720, 370, 30, 120000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(48, 720, 480, 138, 45, 120000, cea_iar_4_to_3,
		vsm_progressive, par_8_to_9, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(49, 720, 480, 138, 45, 120000, cea_iar_16_to_9,
		vsm_progressive, par_32_to_27, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(50, 720, 480, 276, 22, 120000, cea_iar_4_to_3,
		vsm_interlaced, par_8_to_9, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(51, 720, 480, 276, 22, 120000, cea_iar_16_to_9,
		vsm_interlaced, par_32_to_27, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(52, 720, 576, 144, 49, 200000, cea_iar_4_to_3,
		vsm_progressive, par_16_to_15, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(53, 720, 576, 144, 49, 200000, cea_iar_16_to_9,
		vsm_progressive, par_64_to_45, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(54, 720, 576, 288, 24, 200000, cea_iar_4_to_3,
		vsm_interlaced, par_16_to_15, vif_single_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(55, 720, 576, 288, 24, 200000, cea_iar_16_to_9,
		vsm_interlaced, par_64_to_45, vif_single_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(56, 720, 480, 138, 45, 240000, cea_iar_4_to_3,
		vsm_progressive, par_8_to_9, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(57, 720, 480, 138, 45, 240000, cea_iar_16_to_9,
		vsm_progressive, par_32_to_27, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(58, 720, 480, 276, 22, 240000, cea_iar_4_to_3,
		vsm_interlaced, par_8_to_9, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(59, 720, 480, 276, 22, 240000, cea_iar_16_to_9,
		vsm_interlaced, par_32_to_27, vif_dual_frame_rate, 1, 1),
	CEA_861_F_VIC_info_entry(60, 1280, 720, 2020, 30, 24000,
		cea_iar_16_to_9,
		vsm_progressive, par_1_to_1, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(61, 1280, 720, 2680, 30, 25000,
		cea_iar_16_to_9,
		vsm_progressive, par_1_to_1, vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(62, 1280, 720, 2020, 30, 30000,
		cea_iar_16_to_9,
		vsm_progressive, par_1_to_1, vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(63, 1920, 1080, 280, 45, 120000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(64, 1920, 1080, 720, 45, 100000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(65, 1280, 720, 2020, 30, 24000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(66, 1280, 720, 2680, 30, 25000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(67, 1280, 720, 2020, 30, 30000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(68, 1280, 720, 700, 30, 50000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(69, 1280, 720, 370, 30, 60000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(70, 1280, 720, 700, 30, 100000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(71, 1280, 720, 370, 30, 120000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(72, 1920, 1080, 830, 45, 24000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(73, 1920, 1080, 720, 45, 25000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(74, 1920, 1080, 280, 45, 30000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(75, 1920, 1080, 720, 45, 50000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(76, 1920, 1080, 280, 45, 60000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(77, 1920, 1080, 720, 45, 100000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(78, 1920, 1080, 280, 45, 120000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(79, 1680, 720, 1620, 30, 24000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(80, 1680, 720, 1488, 30, 25000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(81, 1680, 720, 960, 30, 30000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(82, 1680, 720, 520, 30, 50000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(83, 1680, 720, 520, 30, 60000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(84, 1680, 720, 320, 105, 100000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(85, 1680, 720, 320, 105, 120000,
		cea_iar_64_to_27, vsm_progressive, par_64_to_63,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(86, 2560, 1080, 1190, 20, 24000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(87, 2560, 1080, 640, 45, 25000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(88, 2560, 1080, 960, 45, 30000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(89, 2560, 1080, 740, 45, 50000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(90, 2560, 1080, 440, 20, 60000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(91, 2560, 1080, 410, 170, 100000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(92, 2560, 1080, 740, 170, 120000,
		cea_iar_64_to_27, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(93, 3840, 2160, 1660, 90, 24000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(94, 3840, 2160, 1440, 90, 25000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(95, 3840, 2160, 560, 90, 30000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(96, 3840, 2160, 1440, 90, 50000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(97, 3840, 2160, 560, 90, 60000,
		cea_iar_16_to_9, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(98, 4096, 2160, 1404, 90, 24000,
		cea_iar_256_to_135, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(99, 4096, 2160, 1184, 90, 25000,
		cea_iar_256_to_135, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(100, 4096, 2160, 304, 90, 30000,
		cea_iar_256_to_135, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(101, 4096, 2160, 1184, 90, 50000,
		cea_iar_256_to_135, vsm_progressive, par_1_to_1,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(102, 4096, 2160, 304, 90, 60000,
		cea_iar_256_to_135, vsm_progressive, par_1_to_1,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(103, 3840, 2160, 1660, 90, 24000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(104, 3840, 2160, 1440, 90, 25000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(105, 3840, 2160, 560, 90, 30000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(106, 3840, 2160, 1440, 90, 50000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_single_frame_rate, 0, 0),
	CEA_861_F_VIC_info_entry(107, 3840, 2160, 560, 90, 60000,
		cea_iar_64_to_27, vsm_progressive, par_4_to_3,
		vif_dual_frame_rate, 0, 0)
};

uint32_t calculate_pixel_clock(struct edid_3d_data_t *mhl_edid_3d_data,
	uint16_t columns, uint16_t rows,
	uint32_t vertical_sync_frequency_in_milliHz, uint8_t VIC)
{
	uint32_t pixel_clock_frequency;
	uint32_t vertical_sync_period_in_microseconds;
	uint32_t vertical_active_period_in_microseconds;
	uint32_t vertical_blank_period_in_microseconds;
	uint32_t horizontal_sync_frequency_in_hundredths_of_KHz;
	uint32_t horizontal_sync_period_in_nanoseconds;
	uint32_t horizontal_active_period_in_nanoseconds;
	uint32_t horizontal_blank_period_in_nanoseconds;

	MHL_TX_EDID_INFO("vertical_sync_frequency_in_milliHz: %u\n",
			 vertical_sync_frequency_in_milliHz);

	if (0 == vertical_sync_frequency_in_milliHz)
		return 0;

	vertical_sync_period_in_microseconds =
	    1000000000 / vertical_sync_frequency_in_milliHz;
	MHL_TX_EDID_INFO
	    ("vertical_sync_frequency_in_milliHz:%u "
	    "vertical_sync_period_in_microseconds: %u\n",
	     vertical_sync_frequency_in_milliHz,
	     vertical_sync_period_in_microseconds);

	VIC &= 0x7F;
	if (VIC >= sizeof(VIC_info) / sizeof(VIC_info[0])) {
		MHL_TX_DBG_ERR("%sVIC out of range%s\n", ANSI_ESC_RED_TEXT,
			       ANSI_ESC_RESET_TEXT);
		return 0;
	}

	if (0 == VIC) {
		/* rule of thumb: */
		vertical_active_period_in_microseconds =
		    (vertical_sync_period_in_microseconds * 8) / 10;

	} else {
		uint16_t v_total_in_lines;
		uint16_t v_blank_in_lines;

		if (vsm_interlaced == VIC_info[VIC].fields.interlaced) {
			/* fix up these two values */
			vertical_sync_frequency_in_milliHz /= 2;
			vertical_sync_period_in_microseconds *= 2;
			MHL_TX_EDID_INFO
			    ("interlaced vertical_sync_frequency_in_milliHz:%u"
			     " vertical_sync_period_in_microseconds: %u\n",
			     vertical_sync_frequency_in_milliHz,
			     vertical_sync_period_in_microseconds);

			/* proceed with calculations */
			v_blank_in_lines =
			    2 * VIC_info[VIC].v_blank_in_pixels +
			    VIC_info[VIC].fields.field2_v_blank;

		} else {
			/* when multiple vertical blanking values present,
			 * allow for higher clocks by calculating maximum
			 * possible
			 */
			v_blank_in_lines =
			    VIC_info[VIC].v_blank_in_pixels +
			    VIC_info[VIC].fields.field2_v_blank;
		}
		v_total_in_lines = VIC_info[VIC].rows + v_blank_in_lines;
		vertical_active_period_in_microseconds =
		    (vertical_sync_period_in_microseconds *
		     VIC_info[VIC].rows) / v_total_in_lines;

	}
	MHL_TX_EDID_INFO("vertical_active_period_in_microseconds: %u\n",
			 vertical_active_period_in_microseconds);

	/* rigorous calculation: */
	vertical_blank_period_in_microseconds =
	    vertical_sync_period_in_microseconds -
	    vertical_active_period_in_microseconds;
	MHL_TX_EDID_INFO("vertical_blank_period_in_microseconds: %u\n",
			 vertical_blank_period_in_microseconds);

	horizontal_sync_frequency_in_hundredths_of_KHz = rows * 100000;
	horizontal_sync_frequency_in_hundredths_of_KHz /=
	    vertical_active_period_in_microseconds;

	MHL_TX_EDID_INFO("horizontal_sync_frequency_in_hundredths_of_KHz: %u\n",
			 horizontal_sync_frequency_in_hundredths_of_KHz);

	horizontal_sync_period_in_nanoseconds =
	    100000000 / horizontal_sync_frequency_in_hundredths_of_KHz;

	MHL_TX_EDID_INFO("horizontal_sync_period_in_nanoseconds: %u\n",
			 horizontal_sync_period_in_nanoseconds);

	if (0 == VIC) {
		/* rule of thumb: */
		horizontal_active_period_in_nanoseconds =
		    (horizontal_sync_period_in_nanoseconds * 8) / 10;
		MHL_TX_EDID_INFO
		    ("horizontal_active_period_in_nanoseconds: %u\n",
		     horizontal_active_period_in_nanoseconds);
	} else {
		uint16_t h_total_in_pixels;
		uint16_t h_clocks;
		h_clocks =
		    VIC_info[VIC].columns << VIC_info[VIC].fields.
		    clocks_per_pixel_shift_count;
		h_total_in_pixels = h_clocks + VIC_info[VIC].h_blank_in_pixels;
		horizontal_active_period_in_nanoseconds =
		    (horizontal_sync_period_in_nanoseconds * h_clocks) /
		    h_total_in_pixels;
	}
	/* rigorous calculation: */
	horizontal_blank_period_in_nanoseconds =
	    horizontal_sync_period_in_nanoseconds -
	    horizontal_active_period_in_nanoseconds;
	MHL_TX_EDID_INFO("horizontal_blank_period_in_nanoseconds: %u\n",
			 horizontal_blank_period_in_nanoseconds);

	pixel_clock_frequency =
	    columns * (1000000000 / horizontal_active_period_in_nanoseconds);

	MHL_TX_EDID_INFO("pixel_clock_frequency: %u\n", pixel_clock_frequency);

	return pixel_clock_frequency;
}

uint8_t qualify_pixel_clock_for_mhl(struct edid_3d_data_t *mhl_edid_3d_data,
	uint32_t pixel_clock_frequency, uint8_t bits_per_pixel)
{
	uint32_t link_clock_frequency;
	uint32_t max_link_clock_frequency = 0;
	uint8_t ret_val;
	int peer_is_mhl3;

	peer_is_mhl3 =
		si_mhl_tx_drv_connection_is_mhl3(mhl_edid_3d_data->dev_context);
	if (16 != bits_per_pixel) {
		MHL_TX_EDID_INFO("not 16 bpp\n")
		;
	} else if (!si_edid_sink_supports_YCbCr422(mhl_edid_3d_data)) {
		MHL_TX_EDID_INFO("no EDID 4:2:2\n")
		return 0;
	} else if (peer_is_mhl3) {
		MHL_TX_EDID_INFO("peer is MHL3\n")
		if (!_16_BPP_AVAILABLE(mhl_edid_3d_data->dev_context)) {
			MHL_TX_EDID_INFO("no DEVCAP 16bpp\n")
			return 0;
		}
	} else {
		MHL_TX_EDID_INFO("peer is not MHL3\n")
		if (!PACKED_PIXEL_AVAILABLE(mhl_edid_3d_data->dev_context)) {
			MHL_TX_EDID_INFO("no DEVCAP packed pixel\n")
			return 0;
		}
	}

	link_clock_frequency =
	    pixel_clock_frequency * ((uint32_t) (bits_per_pixel >> 3));

	if (peer_is_mhl3) {
		switch (si_mhl_tx_drv_get_highest_tmds_link_speed
			(mhl_edid_3d_data->dev_context)) {
		case MHL_XDC_TMDS_600:
			MHL_TX_EDID_INFO("MHL_XDC_TMDS_600\n");
			max_link_clock_frequency = 600000000;
			break;
		case MHL_XDC_TMDS_300:
			MHL_TX_EDID_INFO("MHL_XDC_TMDS_300\n");
			max_link_clock_frequency = 300000000;
			break;
		case MHL_XDC_TMDS_150:
			MHL_TX_EDID_INFO("MHL_XDC_TMDS_150\n");
			max_link_clock_frequency = 150000000;
			break;
		case MHL_XDC_TMDS_000:
			max_link_clock_frequency = 000000000;
			break;
		}
	} else {
		if (si_peer_supports_packed_pixel
		    (mhl_edid_3d_data->dev_context)) {
			max_link_clock_frequency = 300000000;
		} else {
			max_link_clock_frequency = 225000000;
		}
	}

	if (link_clock_frequency < max_link_clock_frequency)
		ret_val = 1;
	else
		ret_val = 0;

	MHL_TX_EDID_INFO
	    ("Link clock:%u Hz %12s for MHL at %d bpp (max: %u Hz)\n",
	     link_clock_frequency, ret_val ? "valid" : "unattainable",
	     (uint16_t) bits_per_pixel, max_link_clock_frequency);
	return ret_val;
}

/*
  is_MHL_timing_mode

	MHL has a maximum link clock of 75Mhz.
	For now, we use a rule of thumb regarding
		blanking intervals to calculate a pixel clock,
		then we convert it to a link clock and compare to 75MHz

*/

static uint8_t is_MHL_timing_mode(struct edid_3d_data_t *mhl_edid_3d_data,
	uint16_t columns, uint16_t rows,
	uint32_t vertical_sync_frequency_in_milliHz, uint16_t burst_id,
	union video_burst_descriptor_u *p_descriptor, uint8_t VIC)
{
	uint32_t pixel_clock_frequency;
	uint8_t ret_val = 0;

	if (p_descriptor) {
		if (burst_id_HEV_DTDB == burst_id) {
			struct MHL3_hev_dtd_item_t *descriptor =
			    &p_descriptor->mhl3_hev_dtd;
			pixel_clock_frequency =
			    1000000 *
			    (uint32_t) ENDIAN_CONVERT_16(descriptor->a.
							 pixel_clock_in_MHz);
			columns =
			    ENDIAN_CONVERT_16(descriptor->a.h_active_in_pixels);
			rows = ENDIAN_CONVERT_16(descriptor->b.v_total_in_lines)
			    - descriptor->b.v_blank_in_lines;
			vertical_sync_frequency_in_milliHz =
			    1000 *
			    descriptor->b.v_refresh_rate_in_fields_per_second;
		} else {
			pixel_clock_frequency = calculate_pixel_clock(
				mhl_edid_3d_data, columns, rows,
				vertical_sync_frequency_in_milliHz, VIC);
		}
	} else {
		pixel_clock_frequency = calculate_pixel_clock(mhl_edid_3d_data,
			columns, rows, vertical_sync_frequency_in_milliHz, VIC);
	}
	if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data, pixel_clock_frequency,
		24)) {

		display_timing_enumeration_callback(mhl_edid_3d_data, columns,
			rows, 24, vertical_sync_frequency_in_milliHz, burst_id,
			p_descriptor);
		ret_val = 1;
	}
	if (qualify_pixel_clock_for_mhl(mhl_edid_3d_data, pixel_clock_frequency,
		16)) {

		display_timing_enumeration_callback(mhl_edid_3d_data, columns,
			rows, 16, vertical_sync_frequency_in_milliHz, burst_id,
			p_descriptor);
		ret_val = 1;
	}

	return ret_val;
}

static void tx_prune_dtd_list(struct edid_3d_data_t *mhl_edid_3d_data,
	union _18_byte_descriptor_u *p_desc, uint8_t limit)
{
	uint8_t i;
	uint8_t number_that_we_pruned = 0;
	union _18_byte_descriptor_u *p_start = p_desc;
	MHL_TX_EDID_INFO("limit: %d\n", (uint16_t) limit);
	if (limit) {
		for (i = 0; i < limit - 1; ++i) {
			MHL_TX_EDID_INFO("i: %d\n", (uint16_t) i);
			if ((0 != p_desc->dtd.pixel_clock_low) ||
				(0 != p_desc->dtd.pixel_clock_high)) {
				MHL_TX_EDID_INFO(
					"pix clock non-zero p_desc:%pK", p_desc)
				if ((0 == p_desc->dtd.horz_active_7_0) &&
				    (0 == p_desc->dtd.horz_active_blanking_high.
					horz_active_11_8)) {
					union _18_byte_descriptor_u
						*p_holder = p_desc,
						*p_next_desc = p_desc + 1;
					uint8_t j;
					MHL_TX_DBG_INFO("pruning\n");
					number_that_we_pruned++;
					for (j = i + 1; j < limit; ++j) {
						/* move the rest of the entries
						 * one by one
						 */
						MHL_TX_EDID_INFO(
						"p_desc:%pK p_next_desc:%pK\n",
						p_desc, p_next_desc)
						*p_desc++ = *p_next_desc++;
					}
					/* re-consider the new occupant of the
					 * i'th entry on the next iteration
					 */
					i--;
					p_desc = p_holder;
				} else {
					p_desc++;
					MHL_TX_EDID_INFO("p_desc:%pK\n", p_desc)
				}
			}
		}

		p_desc = p_start + (limit - number_that_we_pruned);
		for (; number_that_we_pruned > 0;
		     --number_that_we_pruned, --p_desc) {
			uint8_t *pu8_temp = (uint8_t *) p_desc;
			uint8_t size;

			for (size = sizeof(*p_desc); size > 0; --size)
				*pu8_temp++ = 0;
		}
	}
}

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
	struct edid_3d_data_t *mhl_edid_3d_data,
	union _18_byte_descriptor_u *p_desc, uint8_t Block,
	uint8_t *p_is_timing, uint16_t burst_id,
	union video_burst_descriptor_u *p_descriptor)
{
	uint8_t tmp_byte;
	uint8_t i;
	uint16_t tmp_word;

	*p_is_timing = 0;
	tmp_word =
	    p_desc->dtd.pixel_clock_low + 256 * p_desc->dtd.pixel_clock_high;
	/* 18 byte partition is used as either for Monitor Name or for Monitor
	 * Range Limits or it is unused
	 */
	if (tmp_word == 0x00) {
		/*  if called from Block #0 and first 2 bytes are 0 => either
		 * Monitor Name or for Monitor Range Limits
		 */
		if (Block == EDID_BLOCK_0) {
			if (0xFC == p_desc->name.data_type_tag) {
				char monitor_name[14];

				for (i = 0; i < 13; i++) {
					monitor_name[i] =
					p_desc->name.ascii_name[i];
				}
				monitor_name[13] = '\0';
				MHL_TX_EDID_INFO("Monitor Name:\"%s\"\n",
					monitor_name);
			} else if (0xFD == p_desc->name.data_type_tag) {
				MHL_TX_EDID_INFO("Monitor Range Limits:\n\n");

				/*i = 0;*/
				MHL_TX_EDID_INFO(
					"Min Vertical Rate in Hz: %d\n",
					(int)p_desc->range_limits.
					min_vertical_rate_in_Hz);
				MHL_TX_EDID_INFO(
					"Max Vertical Rate in Hz: %d\n",
					(int)p_desc->range_limits.
					max_vertical_rate_in_Hz);
				MHL_TX_EDID_INFO(
					"Min Horizontal Rate in KHz: %d\n",
					(int)p_desc->range_limits.
					min_horizontal_rate_in_KHz);
				MHL_TX_EDID_INFO(
					"Max Horizontal Rate in KHz: %d\n",
					(int)p_desc->range_limits.
					max_horizontal_rate_in_KHz);
				MHL_TX_EDID_INFO(
					"Max Supported pixel clock rate in "
					"MHz/10: %d\n",
					(int)p_desc->range_limits.
					max_pixel_clock_in_MHz_div_10);
				MHL_TX_EDID_INFO(
					"Tag for secondary timing formula "
					"(00h=not used): %d\n",
					(int)p_desc->range_limits.
					tag_secondary_formula);
				MHL_TX_EDID_INFO("\n");
			}
		} else if (Block == EDID_BLOCK_2_3) {
			/* if called from block #2 or #3 and first 2 bytes are
			 * 0x00 (padding) then this descriptor partition is not
			 * used and parsing should be stopped
			 */
			MHL_TX_EDID_INFO
			    ("No More Detailed descriptors in this block\n");
			MHL_TX_EDID_INFO("\n");
			return false;
		}
	} else {
		/* first 2 bytes are not 0 => this is a detailed timing
		 * descriptor from either block
		 */
		uint32_t pixel_clock_frequency;
		uint16_t columns, rows, vertical_sync_period_in_lines;
		uint32_t vertical_refresh_rate_in_milliHz,
		    horizontal_sync_frequency_in_Hz,
		    horizontal_sync_period_in_pixels;
		uint8_t this_mode_doable = 0;

		*p_is_timing = 1;

		pixel_clock_frequency = (uint32_t) tmp_word * 10000;

		MHL_TX_EDID_INFO
		    ("Pixel Clock: %d.%02d MHz or %d Hz (0x%x Hz)\n",
		     tmp_word / 100, tmp_word % 100, pixel_clock_frequency,
		     pixel_clock_frequency);

		columns =
		    p_desc->dtd.horz_active_7_0 +
		    256 *
		    p_desc->dtd.horz_active_blanking_high.horz_active_11_8;
		MHL_TX_EDID_INFO("Horizontal Active Pixels: %d\n", columns);

		tmp_word =
		    p_desc->dtd.horz_blanking_7_0 +
		    256 *
		    p_desc->dtd.horz_active_blanking_high.horz_blanking_11_8;
		MHL_TX_EDID_INFO("Horizontal Blanking (Pixels): %d\n",
				 tmp_word);

		horizontal_sync_period_in_pixels =
		    (uint32_t) columns + (uint32_t) tmp_word;
		horizontal_sync_frequency_in_Hz =
		    pixel_clock_frequency / horizontal_sync_period_in_pixels;

		MHL_TX_EDID_INFO("horizontal period %u pixels,  "
				 "horizontal_sync_frequency_in_Hz: %u Hz\n",
				 horizontal_sync_period_in_pixels,
				 horizontal_sync_frequency_in_Hz);

		rows =
		    p_desc->dtd.vert_active_7_0 +
		    256 *
		    p_desc->dtd.vert_active_blanking_high.vert_active_11_8;
		MHL_TX_EDID_INFO("Vertical Active (Lines): %u\n", rows);

		tmp_word =
		    p_desc->dtd.vert_blanking_7_0 +
		    256 *
		    p_desc->dtd.vert_active_blanking_high.vert_blanking_11_8;
		MHL_TX_EDID_INFO("Vertical Blanking (Lines): %u\n", tmp_word);

		vertical_sync_period_in_lines = rows + tmp_word;
		vertical_refresh_rate_in_milliHz =
		    horizontal_sync_frequency_in_Hz * 1000 /
		    (vertical_sync_period_in_lines);
		MHL_TX_EDID_INFO
		    ("vertical period %u lines, frequency %u MilliHz\n",
		     vertical_sync_period_in_lines,
		     vertical_refresh_rate_in_milliHz);

		tmp_word =
		    p_desc->dtd.horz_sync_offset_7_0 +
		    256 * p_desc->dtd.hs_vs_offset_pulse_width.
		    horz_sync_offset_9_8;
		MHL_TX_EDID_INFO("Horizontal Sync Offset (Pixels): %d\n",
				 tmp_word);

		tmp_word =
		    p_desc->dtd.horz_sync_pulse_width7_0 +
		    256 * p_desc->dtd.hs_vs_offset_pulse_width.
		    horz_sync_pulse_width_9_8;
		MHL_TX_EDID_INFO("Horizontal Sync Pulse Width (Pixels): %d\n",
				 tmp_word);

		tmp_word =
		    p_desc->dtd.vert_sync_offset_width.vert_sync_offset_3_0 +
		    16 * p_desc->dtd.hs_vs_offset_pulse_width.
		    vert_sync_offset_5_4;
		MHL_TX_EDID_INFO("Vertical Sync Offset (Lines): %d\n",
				 tmp_word);

		tmp_word =
		    p_desc->dtd.vert_sync_offset_width.
		    vert_sync_pulse_width_3_0 +
		    16 * p_desc->dtd.hs_vs_offset_pulse_width.
		    vert_sync_pulse_width_5_4;
		MHL_TX_EDID_INFO("Vertical Sync Pulse Width (Lines): %d\n",
				 tmp_word);

		tmp_word = p_desc->dtd.horz_image_size_in_mm_7_0
		    +
		    256 *
		    p_desc->dtd.image_size_high.horz_image_size_in_mm_11_8;
		MHL_TX_EDID_INFO("Horizontal Image Size (mm): %d\n", tmp_word);

		tmp_word = p_desc->dtd.vert_image_size_in_mm_7_0
		    +
		    256 *
		    p_desc->dtd.image_size_high.vert_image_size_in_mm_11_8;
		MHL_TX_EDID_INFO("Vertical Image Size (mm): %d\n", tmp_word);

		tmp_byte = p_desc->dtd.horz_border_in_lines;
		MHL_TX_EDID_INFO("Horizontal Border (Pixels): %d\n",
				 (int)tmp_byte);

		tmp_byte = p_desc->dtd.vert_border_in_pixels;
		MHL_TX_EDID_INFO("Vertical Border (Lines): %d\n",
				 (int)tmp_byte);

		MHL_TX_EDID_INFO("%snterlaced\n", p_desc->dtd.flags.interlaced
			? "I" : "Non-i");

		switch (p_desc->dtd.flags.stereo_bits_2_1) {
		case 0:
			MHL_TX_EDID_INFO("Normal Display, No Stereo\n");
			break;
		case 1:
			if (0 == p_desc->dtd.flags.stereo_bit_0) {
				MHL_TX_EDID_INFO
				    ("Field sequential stereo, right image "
				     "when stereo sync signal == 1\n");
			} else {
				MHL_TX_EDID_INFO
				    ("2-way interleaved stereo, right image "
				     "on even lines\n");
			}
			break;
		case 2:
			if (0 == p_desc->dtd.flags.stereo_bit_0) {
				MHL_TX_EDID_INFO
				    ("field-sequential stereo, left image "
				     "when stereo sync signal == 1\n");
			} else {
				MHL_TX_EDID_INFO
				    ("2-way interleaved stereo, left image on "
				     "even lines.\n");
			}
			break;
		case 3:
			if (0 == p_desc->dtd.flags.stereo_bit_0) {
				MHL_TX_EDID_INFO("4-way interleaved stereo\n");
			} else {
				MHL_TX_EDID_INFO
				    ("side-by-side interleaved stereo.\n");
			}
			break;
		}

		switch (p_desc->dtd.flags.sync_signal_type) {
		case 0x0:
			MHL_TX_EDID_INFO("Analog Composite\n");
			break;
		case 0x1:
			MHL_TX_EDID_INFO("Bipolar Analog Composite\n");
			break;
		case 0x2:
			MHL_TX_EDID_INFO("Digital Composite\n");
			break;
		case 0x3:
			MHL_TX_EDID_INFO("Digital Separate\n");
			break;
		}

		this_mode_doable = qualify_pixel_clock_for_mhl(
			(void *)mhl_edid_3d_data,
			pixel_clock_frequency, 24);
		if (this_mode_doable) {
			if (p_descriptor) {
				display_timing_enumeration_callback(
					mhl_edid_3d_data, columns, rows, 24,
					vertical_refresh_rate_in_milliHz,
					burst_id, p_descriptor);
			}
		}

		this_mode_doable |= qualify_pixel_clock_for_mhl(
			(void *)mhl_edid_3d_data,
			pixel_clock_frequency, 16);
		if (this_mode_doable) {
			if (p_descriptor) {
				display_timing_enumeration_callback(
					mhl_edid_3d_data, columns, rows, 16,
					vertical_refresh_rate_in_milliHz,
					burst_id, p_descriptor);
			}
		}

		if (!this_mode_doable) {
			/*
			 * Mark this mode for pruning by setting
			 * horizontal active to zero
			 */
			MHL_TX_DBG_ERR("%smark for pruning%s %pK\n",
				ANSI_ESC_YELLOW_TEXT,
				ANSI_ESC_RESET_TEXT,
				p_desc);
			p_desc->dtd.horz_active_7_0 = 0;
			p_desc->dtd.horz_active_blanking_high.
			    horz_active_11_8 = 0;
			return false;
		}
	}
	return true;
}

static uint8_t si_mhl_tx_parse_861_long_descriptors(
	struct edid_3d_data_t *mhl_edid_3d_data, uint8_t *p_EDID_block_data)
{
	struct CEA_extension_t *p_CEA_ext =
		(struct CEA_extension_t *) p_EDID_block_data;

	/* per CEA-861-D, table 27 */
	if (!p_CEA_ext->byte_offset_to_18_byte_descriptors) {
		MHL_TX_DBG_ERR("EDID -> No Detailed Descriptors\n");
		return EDID_NO_DETAILED_DESCRIPTORS;
	} else {
		uint8_t *puc_next_block;
		uint8_t descriptor_num = 1;
		union {
			uint8_t *puc_data_block;
			union _18_byte_descriptor_u *p_long_descriptors;
		} p_data_u;

		p_data_u.p_long_descriptors =
			(union _18_byte_descriptor_u *)(((uint8_t *)p_CEA_ext)
			+ p_CEA_ext->byte_offset_to_18_byte_descriptors);
		puc_next_block =
		    ((uint8_t *)p_CEA_ext) + EDID_BLOCK_SIZE;

		while ((uint8_t *) (p_data_u.p_long_descriptors + 1) <
		       puc_next_block) {
			uint8_t is_timing = 0;
			bool valid;
			MHL_TX_EDID_INFO(
			    "Parse Results - CEA-861 Long Descriptor #%d:\n",
			     (int)descriptor_num);
			MHL_TX_EDID_INFO(
			    "============================================\n");

			valid = si_mhl_tx_parse_detailed_timing_descriptor
			    (mhl_edid_3d_data, p_data_u.p_long_descriptors,
			     EDID_BLOCK_2_3, &is_timing, 0, NULL);
			if (is_timing) {
				++mhl_edid_3d_data->parse_data.
				    num_cea_861_timing_dtds;
			} else if (valid) {
				MHL_TX_EDID_INFO("stopping at %pK\n",
					p_data_u.p_long_descriptors)
				break;
			}
			p_data_u.p_long_descriptors++;
			descriptor_num++;
		}

		return EDID_LONG_DESCRIPTORS_OK;
	}
}

static void prune_hdmi_vsdb_vic_list(
		struct edid_3d_data_t *mhl_edid_3d_data,
		struct CEA_extension_t *p_CEA_extension,
		uint8_t length_VIC)
{
	uint8_t i, num_HDMI_VICs_pruned = 0;
	uint8_t inner_loop_limit = length_VIC;
	uint8_t outer_loop_limit = length_VIC - 1;
	uint8_t *pb_limit = (uint8_t *)(p_CEA_extension + 1);

	MHL_TX_EDID_INFO(
	    "len_VIC:%d inner_loop_limit: %d outer_loop_limit: %d\n",
	     (uint16_t) length_VIC, (uint16_t) inner_loop_limit,
	     (uint16_t) outer_loop_limit);
	for (i = 0; i < outer_loop_limit;) {
		if (0 == mhl_edid_3d_data->parse_data.
		    p_byte_13_through_byte_15->vicList[i]) {
			uint8_t j, prev;

			prev = i;
			for (j = i + 1; j < inner_loop_limit; ++j, ++prev) {
				uint16_t VIC0, VIC1;

				VIC0 = mhl_edid_3d_data->parse_data.
				    p_byte_13_through_byte_15->vicList[prev];
				VIC1 =
				    mhl_edid_3d_data->
				    parse_data.
				    p_byte_13_through_byte_15->
				    vicList[j];
				MHL_TX_EDID_INFO(
				    "replacing VIC: %3d at index: %3d"
				     " with VIC:%3d from index: %3d\n",
				     VIC0, (uint16_t) prev,
				     VIC1, (uint16_t) j);
				mhl_edid_3d_data->parse_data.
				    p_byte_13_through_byte_15->
				    vicList[prev]
				    = mhl_edid_3d_data->
				    parse_data.
				    p_byte_13_through_byte_15->
				    vicList[j];
			}
			num_HDMI_VICs_pruned++;
			inner_loop_limit--;
			outer_loop_limit--;
		} else {
			/* This mode is doable on MHL,
			 * so move on to the next index
			 */
			++i;
		}
	}
	/* check the last one */
	if (0 == mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->
	    vicList[outer_loop_limit]) {
		num_HDMI_VICs_pruned++;
		inner_loop_limit--;
	}

	DUMP_EDID_BLOCK(0, p_CEA_extension, sizeof(*p_CEA_extension));
	/* now move all other data up */
	if (num_HDMI_VICs_pruned) {
		uint8_t *pb_dest = (uint8_t *) &mhl_edid_3d_data->parse_data.
		    p_byte_13_through_byte_15->vicList[inner_loop_limit];
		uint8_t *pb_src = (uint8_t *) &mhl_edid_3d_data->parse_data.
		    p_byte_13_through_byte_15->vicList[length_VIC];

		SII_ASSERT(EDID_BLOCK_SIZE == sizeof(*p_CEA_extension),
			   ("\n\n unexpected extension size\n\n"));
		while (pb_src < pb_limit) {
			MHL_TX_EDID_INFO(
			    "moving data up %02x(0x%02x) <- %02x(0x%02x)\n",
			     pb_dest, (uint16_t) *pb_dest,
			     pb_src, (uint16_t) *pb_src);
			*pb_dest++ = *pb_src++;
		}

		while (pb_dest < pb_limit) {
			MHL_TX_EDID_INFO("clearing data %02x <- 0\n", pb_dest);
			*pb_dest++ = 0;
		}
	}

	mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.
		HDMI_VIC_len = inner_loop_limit;
	p_CEA_extension->byte_offset_to_18_byte_descriptors -=
	    num_HDMI_VICs_pruned;
	MHL_TX_EDID_INFO("%pK\n", mhl_edid_3d_data->parse_data.p_HDMI_vsdb);
	if (mhl_edid_3d_data->parse_data.p_HDMI_vsdb) {
		mhl_edid_3d_data->parse_data.p_HDMI_vsdb->
		    header.fields.length_following_header -=
		    num_HDMI_VICs_pruned;
	} else if (num_HDMI_VICs_pruned) {
		MHL_TX_DBG_ERR("HDMI VICs to prune but no HDMI VSDB!\n");
	}
}


static void inner_loop(
		struct edid_3d_data_t *mhl_edid_3d_data,
		int outer_index, int vdb_index, int limit)
{
	uint8_t j, prev;

	prev = outer_index;
	for (j = outer_index + 1; j < limit; ++j, ++prev) {
		uint16_t VIC0, VIC1;

		VIC0 = mhl_edid_3d_data->
		    parse_data.p_video_data_blocks_2d[vdb_index]->
		    short_descriptors[prev].VIC;
		VIC1 = mhl_edid_3d_data->
		    parse_data.p_video_data_blocks_2d[vdb_index]->
		    short_descriptors[j].VIC;
		MHL_TX_EDID_INFO(
		    "Replacing SVD:%6s 0x%02x at index: 0x%02x with "
		    "SVD:%6s 0x%02x from index: 0x%02x\n",
		    mhl_edid_3d_data->parse_data.
		    p_video_data_blocks_2d[vdb_index]->
		    short_descriptors[prev].native ? "Native" : "",
		    VIC0, (uint16_t)prev,
		    mhl_edid_3d_data->
		    parse_data.p_video_data_blocks_2d[vdb_index]->
		    short_descriptors[j].native ? "Native" : "",
		    VIC1, (uint16_t) j);
		mhl_edid_3d_data->parse_data.
		    p_video_data_blocks_2d[vdb_index]->
		    short_descriptors[prev] = mhl_edid_3d_data->parse_data.
			p_video_data_blocks_2d[vdb_index]->short_descriptors[j];
	}
}

static void prune_svd_list(
		struct edid_3d_data_t *mhl_edid_3d_data,
		struct CEA_extension_t *p_CEA_extension)
{
	uint8_t i, num_CEA_VICs_pruned = 0;
	uint8_t *pb_limit = (uint8_t *)(p_CEA_extension + 1);

	/*
	   pack each vdb to eliminate the bytes that have been zeroed.
	 */
	int8_t vdb_index;
	for (vdb_index = mhl_edid_3d_data->parse_data.num_video_data_blocks - 1;
	     vdb_index >= 0; --vdb_index) {
		uint8_t inner_loop_limit = mhl_edid_3d_data->parse_data.
		    p_video_data_blocks_2d[vdb_index]->header.fields.
		    length_following_header;
		if (inner_loop_limit) {
			uint8_t outer_loop_limit = (inner_loop_limit - 1);

			for (i = 0; i < outer_loop_limit;) {
				if (0 == mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[vdb_index]->
				    short_descriptors[i].VIC) {
					num_CEA_VICs_pruned++;
					inner_loop(mhl_edid_3d_data,
						i, vdb_index,
						inner_loop_limit);
					inner_loop_limit--;
					outer_loop_limit--;
					MHL_TX_EDID_INFO(
					    "outer_limit:%d inner_limit:%d\n",
					    (uint16_t)outer_loop_limit,
					    (uint16_t)inner_loop_limit);
				} else {
					/* This mode is doable on MHL,
					 * so move on to the next index.
					 */
					++i;
				}
			}
			/* check the last one */
			if (0 == mhl_edid_3d_data->parse_data.
			    p_video_data_blocks_2d[vdb_index]->
			    short_descriptors[outer_loop_limit].VIC) {
				num_CEA_VICs_pruned++;
				inner_loop_limit--;
				mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[vdb_index]->
				    short_descriptors[outer_loop_limit].
				    native = 0;
			}

			DUMP_EDID_BLOCK(0, p_CEA_extension,
					sizeof(*p_CEA_extension));
			{
				/* now move all other data up */
				uint8_t *pb_dest =
				    (uint8_t *) &mhl_edid_3d_data->
				    parse_data.
				    p_video_data_blocks_2d[vdb_index]->
				    short_descriptors[inner_loop_limit];
				uint8_t *pb_src =
				    (uint8_t *) &mhl_edid_3d_data->
				    parse_data.
				    p_video_data_blocks_2d[vdb_index]->
				    short_descriptors[mhl_edid_3d_data->
						      parse_data.
						      p_video_data_blocks_2d
						      [vdb_index]->
						      header.fields.
						      length_following_header];

				SII_ASSERT(EDID_BLOCK_SIZE ==
					   sizeof(*p_CEA_extension),
					   ("\n\nInvalid extension size\n\n"));
				while (pb_src < pb_limit) {
					MHL_TX_EDID_INFO(
					    "moving data up %pK(0x%02X) ",
					    pb_dest, (uint16_t)*pb_dest);
					MHL_TX_EDID_INFO(
						"<- %pK(0x%02X)\n",
					    pb_src, (uint16_t)*pb_src);
					*pb_dest++ = *pb_src++;
				}

				while (pb_dest < pb_limit) {
					MHL_TX_EDID_INFO(
					    "clearing data %02X <- 0\n",
					    *pb_dest);
					*pb_dest++ = 0;
				}

			}
			MHL_TX_EDID_INFO
			    ("CEA-861-D DTDs began at 0x%02x"
			     "CEA-861-D SVD count: 0x%x\n",
			     (uint16_t) p_CEA_extension->
			     byte_offset_to_18_byte_descriptors,
			     (uint16_t) mhl_edid_3d_data->parse_data.
			     p_video_data_blocks_2d[vdb_index]->header.
			     fields.length_following_header);

			p_CEA_extension->byte_offset_to_18_byte_descriptors -=
			    num_CEA_VICs_pruned;
			mhl_edid_3d_data->parse_data.
			    p_video_data_blocks_2d[vdb_index]->header.
			    fields.length_following_header =
			    inner_loop_limit;

			MHL_TX_EDID_INFO
			    ("CEA-861-D DTDs now begin at 0x%02x"
			     "CEA-861-D SVD count: 0x%x\n",
			     (uint16_t) p_CEA_extension->
			     byte_offset_to_18_byte_descriptors,
			     (uint16_t) mhl_edid_3d_data->parse_data.
			     p_video_data_blocks_2d[vdb_index]->header.
			     fields.length_following_header);

			DUMP_EDID_BLOCK(0, p_CEA_extension,
				sizeof(*p_CEA_extension));
		}
	}
}

static void si_mhl_tx_prune_edid(struct edid_3d_data_t *mhl_edid_3d_data)
{
	struct EDID_block0_t *p_EDID_block_0 =
	    (struct EDID_block0_t *) &mhl_edid_3d_data->EDID_block_data[0];
	uint8_t dtd_limit;
	struct CEA_extension_t *p_CEA_extension =
	    (struct CEA_extension_t *)&mhl_edid_3d_data->
	    EDID_block_data[EDID_BLOCK_SIZE];
	struct block_map_t *p_block_map = NULL;
	uint8_t *pb_limit;
	union {
		uint8_t *puc_data_block;
		union _18_byte_descriptor_u *p_long_descriptors;
	} p_data_u;

	if (EDID_EXTENSION_BLOCK_MAP == p_CEA_extension->tag) {
		/* save to overwrite later */
		p_block_map = (struct block_map_t *) p_CEA_extension;

		/* advance to next block */
		p_CEA_extension++;
	}
	pb_limit = (uint8_t *) (p_CEA_extension + 1);

	MHL_TX_EDID_INFO("mhl_tx: %s\n", __func__);
	p_data_u.puc_data_block = (uint8_t *) p_CEA_extension +
		p_CEA_extension->byte_offset_to_18_byte_descriptors;

	DUMP_EDID_BLOCK(0, p_EDID_block_0, sizeof(*p_EDID_block_0));

	/* zero out checksums before modifying data */
	p_CEA_extension->checksum = 0;
	p_EDID_block_0->checksum = 0;

	/* Is there an HDMI VSDB? */
	if (mhl_edid_3d_data->parse_data.p_HDMI_vsdb) {
		struct HDMI_LLC_vsdb_payload_t *p_HDMI_vendor_specific_payload =
		    &mhl_edid_3d_data->parse_data.p_HDMI_vsdb->payload_u.
		    HDMI_LLC;
		uint8_t *p_next_db =
		    (uint8_t *) p_HDMI_vendor_specific_payload +
		    mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header.fields.
		    length_following_header;

		/* if deep color information is provided... */
		if (((uint8_t *) &p_HDMI_vendor_specific_payload->byte6) <
		    p_next_db) {
			p_HDMI_vendor_specific_payload->byte6.DC_Y444 = 0;
			p_HDMI_vendor_specific_payload->byte6.DC_30bit = 0;
			p_HDMI_vendor_specific_payload->byte6.DC_36bit = 0;
			p_HDMI_vendor_specific_payload->byte6.DC_48bit = 0;
		}
	}
	/* prune the DTDs in block 0 */
	dtd_limit =
	    sizeof(p_EDID_block_0->detailed_timing_descriptors) /
	    sizeof(p_EDID_block_0->detailed_timing_descriptors[0]);
	tx_prune_dtd_list(mhl_edid_3d_data,
			(union _18_byte_descriptor_u *) &p_EDID_block_0->
			detailed_timing_descriptors[0], dtd_limit);
	DUMP_EDID_BLOCK(0, p_EDID_block_0, sizeof(*p_EDID_block_0));
	DUMP_EDID_BLOCK(0, p_CEA_extension, sizeof(*p_CEA_extension));

	/* prune the DTDs in the CEA-861D extension */
#if 0
	dtd_limit = (uint8_t) p_CEA_extension->version_u.version3.misc_support.
		total_number_native_dtds_in_entire_EDID;
#else

	dtd_limit = (EDID_BLOCK_SIZE
			- p_CEA_extension->byte_offset_to_18_byte_descriptors)
		/ sizeof(p_data_u.p_long_descriptors[0]) ;
#endif
	tx_prune_dtd_list(mhl_edid_3d_data,
				 &p_data_u.p_long_descriptors[0], dtd_limit);
	DUMP_EDID_BLOCK(0, p_CEA_extension, sizeof(*p_CEA_extension));
	/* adjust the mask according to which 2D VICs were set to zero */
	if (mhl_edid_3d_data->parse_data.p_3d_mask) {
		uint8_t lower_mask;
		uint32_t mask32;
		int8_t index =
		    mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->
		    header.fields.length_following_header - 1;
		index = (index > 15) ? 15 : index;

		mask32 = 0xFFFF00 >> (15 - index);
		lower_mask = (index > 7) ? 0x7F : (0x7F >> (7 - index));

		MHL_TX_EDID_INFO("3d mask 15..8: 0x%02x",
				 (uint16_t) mhl_edid_3d_data->parse_data.
				 p_3d_mask->_3D_mask_15_8);
		for (; index >= 8; mask32 >>= 1, lower_mask >>= 1, --index) {
			if (0 == mhl_edid_3d_data->parse_data.
			    p_video_data_blocks_2d[0]->short_descriptors[index].
			    VIC) {
				uint8_t lower_bits, upper_bits;
				uint8_t upper_mask;
				upper_mask = (uint8_t) mask32;

				/* preserve the lower bits */
				lower_bits =
				    lower_mask & mhl_edid_3d_data->parse_data.
				    p_3d_mask->_3D_mask_15_8;

				/* and out the bit in question */
				upper_bits =
				    upper_mask & mhl_edid_3d_data->parse_data.
				    p_3d_mask->_3D_mask_15_8;

				/* adjust the positions of the upper bits */
				upper_bits >>= 1;

				mhl_edid_3d_data->parse_data.p_3d_mask->
				    _3D_mask_15_8 = lower_bits | upper_bits;
				MHL_TX_EDID_INFO("3d mask 15..8: 0x%02x",
						 (uint16_t) mhl_edid_3d_data->
						 parse_data.p_3d_mask->
						 _3D_mask_15_8);
			}
		}
		MHL_TX_EDID_INFO("3d mask 7..0: 0x%02x",
				 (uint16_t) mhl_edid_3d_data->parse_data.
				 p_3d_mask->_3D_mask_7_0);
		lower_mask = 0x7F >> (7 - index);
		for (; index >= 0; mask32 >>= 1, lower_mask >>= 1, --index) {
			if (0 == mhl_edid_3d_data->parse_data.
			    p_video_data_blocks_2d[0]->
			    short_descriptors[index].VIC) {
				uint8_t lower_bits, upper_bits;
				uint8_t upper_mask;
				upper_mask = (uint8_t) mask32;

				/* preserve the lower bits */
				lower_bits =
				    lower_mask & mhl_edid_3d_data->parse_data.
				    p_3d_mask->_3D_mask_7_0;

				/* AND out the bit in question */
				upper_bits =
				    upper_mask & mhl_edid_3d_data->parse_data.
				    p_3d_mask->_3D_mask_7_0;

				/* adjust the positions of the upper bits */
				upper_bits >>= 1;

				mhl_edid_3d_data->parse_data.p_3d_mask->
				    _3D_mask_7_0 = lower_bits | upper_bits;
				MHL_TX_EDID_INFO("3d mask 7..0: 0x%02x\n",
						 (uint16_t) mhl_edid_3d_data->
						 parse_data.p_3d_mask->
						 _3D_mask_7_0);
			}
		}
	}

	if (mhl_edid_3d_data->parse_data.p_three_d &&
				mhl_edid_3d_data->parse_data.p_HDMI_vsdb) {
		uint8_t num_3D_structure_bytes_pruned = 0;
		union {
			union _3D_structure_and_detail_entry_u *p_3D;
			struct _3D_structure_and_detail_entry_sans_byte1_t
			    *p_sans_byte_1;
			struct _3D_structure_and_detail_entry_with_byte1_t
			    *p_with_byte_1;
			uint8_t *p_as_bytes;
		} p_3D_u;
		uint32_t deletion_mask = 0;
		uint8_t limit_2D_VIC =
		    mhl_edid_3d_data->parse_data.p_video_data_blocks_2d[0]->
		    header.fields.length_following_header;
		/*
		 * Prior to moving things around, make a bitmap of
		 * the positions of the VICs that are zero
		 */
		{
			uint8_t i;
			uint32_t this_bit;
			for (i = 0, this_bit = 1; i < limit_2D_VIC;
			     ++i, this_bit <<= 1) {
				uint8_t VIC;
				VIC =
				    mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[0]->
				    short_descriptors[i].VIC;
				if (0 == VIC) {
					/* set the bit that corresponds to the
					 * VIC that was set to zero
					 */
					deletion_mask |= this_bit;
					MHL_TX_EDID_INFO
					    ("vic: 0x%02x deletion_mask:0x%08x"
					    " this_bit:0x%08x\n",
					     VIC, deletion_mask, this_bit);
				}
			}
		}

		p_3D_u.p_3D = mhl_edid_3d_data->parse_data.p_three_d;
		while (p_3D_u.p_as_bytes <
		       mhl_edid_3d_data->parse_data.p_3d_limit) {
			uint8_t _2D_VIC_order =
			    p_3D_u.p_sans_byte_1->byte0._2D_VIC_order;
			enum _3D_structure_e _3D_structure =
			    p_3D_u.p_sans_byte_1->byte0._3D_structure;
			uint8_t VIC;
			VIC =
			    mhl_edid_3d_data->parse_data.
			    p_video_data_blocks_2d[0]->
			    short_descriptors[_2D_VIC_order].VIC;
			if (0 == VIC) {
				/* delete this 3D_Structure/3D_detail info */
				uint8_t *pSrc, *pDest = p_3D_u.p_as_bytes;

				if (_3D_structure < tdsSideBySide) {
					pSrc =
					    (uint8_t *) (p_3D_u.p_sans_byte_1 +
							 1);
					num_3D_structure_bytes_pruned +=
					    sizeof(*p_3D_u.p_sans_byte_1);
				} else {
					pSrc =
					    (uint8_t *) (p_3D_u.p_with_byte_1 +
							 1);
					num_3D_structure_bytes_pruned +=
					    sizeof(*p_3D_u.p_with_byte_1);
				}
				while (pSrc < pb_limit)
					*pDest++ = *pSrc++;

				while (pDest < pb_limit)
					*pDest++ = 0;
			} else {
				uint8_t i;
				uint8_t limit = _2D_VIC_order;
				uint32_t this_bit;
				MHL_TX_EDID_INFO
				    ("2D vic order: 0x%02x "
				    "deletion_mask:0x%08x\n",
				    _2D_VIC_order, deletion_mask);

				for (i = 0, this_bit = 1; i < limit;
				     ++i, this_bit <<= 1) {
					if (this_bit & deletion_mask)
						_2D_VIC_order--;
				}
				p_3D_u.p_sans_byte_1->byte0._2D_VIC_order =
				    _2D_VIC_order;
				MHL_TX_EDID_INFO
				    ("2D vic order: 0x%02x this_bit:0x%08x\n",
				     _2D_VIC_order, this_bit);

				if (_3D_structure < tdsSideBySide)
					p_3D_u.p_sans_byte_1++;
				else
					p_3D_u.p_with_byte_1++;
			}
		}
		MHL_TX_EDID_INFO("num_3D_structure_bytes_pruned:0x%x "
				 "byte14: 0x%02x "
				 "offset to DTDs: 0x%x "
				 "vsdb header: 0x%x\n",
				 num_3D_structure_bytes_pruned,
				 *((uint8_t *) &mhl_edid_3d_data->parse_data.
				   p_byte_13_through_byte_15->byte14),
				 p_CEA_extension->
				 byte_offset_to_18_byte_descriptors,
				 *((uint8_t *) &mhl_edid_3d_data->parse_data.
				   p_HDMI_vsdb->header)
		    );

		mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->byte14.
		    HDMI_3D_len -= num_3D_structure_bytes_pruned;
		p_CEA_extension->byte_offset_to_18_byte_descriptors -=
		    num_3D_structure_bytes_pruned;
		mhl_edid_3d_data->parse_data.p_HDMI_vsdb->header.fields.
		    length_following_header -= num_3D_structure_bytes_pruned;

		MHL_TX_EDID_INFO("num_3D_structure_bytes_pruned:0x%x "
				 "byte14: 0x%02x "
				 "offset to DTDs: 0x%x "
				 "vsdb header: 0x%x\n",
				 num_3D_structure_bytes_pruned,
				 *((uint8_t *) &mhl_edid_3d_data->parse_data.
				   p_byte_13_through_byte_15->byte14),
				 p_CEA_extension->
				 byte_offset_to_18_byte_descriptors,
				 *((uint8_t *) &mhl_edid_3d_data->parse_data.
				   p_HDMI_vsdb->header)
		    );
	}
	/* Prune the HDMI VSDB VIC list */
	if (mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15) {
		uint8_t length_VIC =
		    mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->
		    byte14.HDMI_VIC_len;

		if (0 == length_VIC) {
			MHL_TX_EDID_INFO("length_VIC:%d\n",
				(uint16_t) length_VIC);
		} else {
			prune_hdmi_vsdb_vic_list(mhl_edid_3d_data,
				p_CEA_extension, length_VIC);
		}
	}

	/* Prune the SVD list and move the CEA 861-D data blocks and DTDs up */
	prune_svd_list(mhl_edid_3d_data, p_CEA_extension);

	/* re-compute the checksum(s) */
	SII_ASSERT(EDID_BLOCK_SIZE == sizeof(*p_EDID_block_0),
		("\n\n unexpected size for block 0\n\n"));
	SII_ASSERT(EDID_BLOCK_SIZE == sizeof(*p_CEA_extension),
		("\n\n unexpected size for  CEA extension\n\n"));

	if (p_block_map) {
		struct CEA_extension_t *p_CEA_extensionDest =
			(struct CEA_extension_t *) p_block_map;
		*p_CEA_extensionDest = *p_CEA_extension;
		p_CEA_extension = p_CEA_extensionDest;
	}
	p_EDID_block_0->extension_flag = p_EDID_block_0->extension_flag ? 1 : 0;

	p_EDID_block_0->checksum =
		calculate_generic_checksum((uint8_t *)p_EDID_block_0, 0,
			sizeof(*p_EDID_block_0));

	p_CEA_extension->checksum =
		calculate_generic_checksum((uint8_t *)p_CEA_extension, 0,
			sizeof(*p_CEA_extension));

	DUMP_EDID_BLOCK(0, p_EDID_block_0, sizeof(*p_EDID_block_0));
	DUMP_EDID_BLOCK(0, p_CEA_extension, sizeof(*p_CEA_extension));

	/*
	 * TODO: adjust all pointers into the EDID along the way of pruning the
	 * contents, instead of re-parsing here
	 */
#ifndef EDID_PASSTHROUGH
	if (0 == si_mhl_tx_drv_set_upstream_edid(mhl_edid_3d_data->drv_context,
		mhl_edid_3d_data->EDID_block_data, 2 * EDID_BLOCK_SIZE))
#endif
		{
		SET_3D_FLAG(mhl_edid_3d_data, FLAGS_EDID_READ_DONE);
	}
}

/*
*/
static uint8_t IsQualifiedMhlVIC(struct edid_3d_data_t *mhl_edid_3d_data,
	uint8_t VIC, uint16_t burst_id,
	union video_burst_descriptor_u *p_descriptor)
{
	uint8_t ret_val = 0;
	if (VIC > 0) {
		ret_val = is_MHL_timing_mode(mhl_edid_3d_data,
				VIC_info[VIC].columns,
				VIC_info[VIC].rows,
				VIC_info[VIC].field_rate_in_milliHz,
				burst_id, p_descriptor, VIC);
		if (vif_dual_frame_rate ==
			VIC_info[VIC].fields.frame_rate_info) {
			uint32_t field_rate_in_milliHz;
			switch (VIC_info[VIC].field_rate_in_milliHz) {
			case 24000:	/* 23.97 */
				field_rate_in_milliHz = 23970;
				break;

			case 30000:	/* 29.97 */
				field_rate_in_milliHz = 29970;
				break;

			case 60000:	/* 59.94 */
				field_rate_in_milliHz = 59940;
				break;

			case 120000:	/* 119.88 */
				field_rate_in_milliHz = 119880;
				break;

			case 240000:	/* 239.76 */
				field_rate_in_milliHz = 239760;
				break;

			default:	/* error or unknown case */
				field_rate_in_milliHz = 0;
				break;
			}
			ret_val |=
			    is_MHL_timing_mode(mhl_edid_3d_data,
					       VIC_info[VIC].columns,
					       VIC_info[VIC].rows,
					       field_rate_in_milliHz, burst_id,
					       p_descriptor, VIC);
		}
	}
	return ret_val;
}

/* HDMI_VIC is a place holder, and not actually stored */
#define HDMI_VIC(HDMI_VIC, cols, rows, field0, field1, pclk0, pclk1, mhl_vic) \
	{cols, rows, field0, field1, pclk0, pclk1, mhl_vic}

PLACE_IN_CODE_SEG struct HDMI_VIC_info_t hdmi_vic_info[] = {
	HDMI_VIC(0, 0, 0, 0, 0, 0, 0, 0),
	HDMI_VIC(1, 3840, 2160, 30000, 29970, 297000000, 296703000, 95),
	HDMI_VIC(2, 3840, 2160, 25000, 25000, 297000000, 297000000, 94),
	HDMI_VIC(3, 3840, 2160, 24000, 23976, 297000000, 296703000, 93),
	HDMI_VIC(4, 4096, 2160, 24000, 24000, 297000000, 297000000, 98)
};

/*
*/
static uint8_t is_qualified_mhl_hdmi_vic(
	struct edid_3d_data_t *mhl_edid_3d_data, uint8_t VIC)
{
	uint8_t ret_val = 0;

	if (qualify_pixel_clock_for_mhl
	    (mhl_edid_3d_data, hdmi_vic_info[VIC].pixel_clock_0, 24)) {
		display_timing_enumeration_callback(mhl_edid_3d_data,
						    hdmi_vic_info[VIC].columns,
						    hdmi_vic_info[VIC].rows, 24,
						    hdmi_vic_info[VIC].
						    field_rate_0_in_milliHz, 0,
						    NULL);
		ret_val = 1;
	}
	if (qualify_pixel_clock_for_mhl
	    (mhl_edid_3d_data, hdmi_vic_info[VIC].pixel_clock_0, 16)) {
		display_timing_enumeration_callback(mhl_edid_3d_data,
						    hdmi_vic_info[VIC].columns,
						    hdmi_vic_info[VIC].rows, 16,
						    hdmi_vic_info[VIC].
						    field_rate_0_in_milliHz, 0,
						    NULL);
		ret_val = 1;
	}
	if (hdmi_vic_info[VIC].pixel_clock_0 !=
	    hdmi_vic_info[VIC].pixel_clock_1) {
		if (qualify_pixel_clock_for_mhl
		    (mhl_edid_3d_data, hdmi_vic_info[VIC].pixel_clock_1, 24)) {
			display_timing_enumeration_callback(
				mhl_edid_3d_data,
				hdmi_vic_info[VIC].columns,
				hdmi_vic_info[VIC].rows, 24,
				hdmi_vic_info[VIC].field_rate_1_in_milliHz,
				0, NULL);
			ret_val = 1;
		}
		if (qualify_pixel_clock_for_mhl
		    (mhl_edid_3d_data, hdmi_vic_info[VIC].pixel_clock_1, 16)) {
			display_timing_enumeration_callback(
				mhl_edid_3d_data,
				hdmi_vic_info[VIC].columns,
				hdmi_vic_info[VIC].rows, 16,
				hdmi_vic_info[VIC].field_rate_1_in_milliHz,
				0, NULL);
			ret_val = 1;
		}
	}
	return ret_val;
}

void si_mhl_tx_enumerate_hdmi_vsdb(struct edid_3d_data_t *mhl_edid_3d_data)
{
	int8_t vdb_index = 0;
	uint8_t hdmi3D_present, hdmi_3D_multi_present;
	struct HDMI_LLC_vsdb_payload_t *p_HDMI_vendor_specific_payload;
	uint8_t *p_next_db;
	uint8_t length_VIC;
	uint8_t index;
	uint8_t length_3D;
	union HDMI_3D_sub_block_t *pThree3DSubBlock;
	union {
		union _3D_structure_and_detail_entry_u *p_3D;
		struct _3D_structure_and_detail_entry_sans_byte1_t *psans_byte1;
		struct _3D_structure_and_detail_entry_with_byte1_t *pwith_byte1;
		uint8_t *p_bytes;
	} p_3D_u;
	uint8_t limit;

	if (mhl_edid_3d_data->parse_data.p_HDMI_vsdb == 0)
		return;
	if (mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15 == 0)
		return;

	p_HDMI_vendor_specific_payload = &mhl_edid_3d_data->
		parse_data.p_HDMI_vsdb->payload_u.HDMI_LLC;
	p_next_db = (uint8_t *)p_HDMI_vendor_specific_payload +
		mhl_edid_3d_data->parse_data.
		p_HDMI_vsdb->header.fields.length_following_header;

	if (((uint8_t *)&mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->byte13) >=
		p_next_db)
		return;

	hdmi3D_present = mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->
		byte13._3D_present;
	hdmi_3D_multi_present = mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->
		byte13._3D_multi_present;

	if (((uint8_t *)&mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->byte14) >=
		p_next_db)
		return;

	/*  HDMI_VIC_len is present... */
	length_VIC = mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->byte14.HDMI_VIC_len;
	for (index = 0; index < length_VIC; ++index) {
		uint8_t VIC;

		VIC = mhl_edid_3d_data->
			parse_data.p_byte_13_through_byte_15->vicList[index];
		if (!is_qualified_mhl_hdmi_vic(mhl_edid_3d_data, VIC)) {
			MHL_TX_EDID_INFO(
				"'can't do HDMI VIC:%d\n", (uint16_t) VIC);
			mhl_edid_3d_data->
				parse_data.
				p_byte_13_through_byte_15->vicList[index] = 0;
		}
	}
	if (hdmi3D_present == 0)
		return;

	pThree3DSubBlock = (union HDMI_3D_sub_block_t *)&mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->vicList[length_VIC];

	p_3D_u.p_3D = NULL;
	length_3D = mhl_edid_3d_data->
		parse_data.p_byte_13_through_byte_15->byte14.HDMI_3D_len;
	limit = mhl_edid_3d_data->
		parse_data.p_video_data_blocks_2d[vdb_index]->
		header.fields.length_following_header;

	limit = (limit > 16) ? 16 : limit;
	switch (hdmi_3D_multi_present) {
	case 0x00:
		/* 3D_Structure_ALL_15..0 and 3D_MASK_15..0
		 * fields are not present
		 * */
		p_3D_u.p_3D = &pThree3DSubBlock->
			HDMI_3D_sub_block_sans_all_AND_mask.
			_3D_structure_and_detail_list[0];
		break;
	case 0x01:
		/*
		 * 3D_Structure_ALL_15..0 is present and assigns 3D formats
		 * to all of the VICs listed in the first 16 entries in the
		 * EDID 3D_mask_15..0 is not present
		 */
		{
			union video_burst_descriptor_u descriptor;
			struct _3D_structure_all_t *p_3D_structure_all =
			    (struct _3D_structure_all_t *)&mhl_edid_3d_data->
			    parse_data.p_byte_13_through_byte_15->
			    vicList[length_VIC];
			descriptor.mhl2_3d_descriptor.left_right =
			    p_3D_structure_all->_3D_structure_all_7_0.
			    side_by_side;
			descriptor.mhl2_3d_descriptor.top_bottom =
			    p_3D_structure_all->_3D_structure_all_15_8.
			    top_bottom;
			descriptor.mhl2_3d_descriptor.frame_sequential =
			    p_3D_structure_all->_3D_structure_all_15_8.
			    frame_packing;
			DUMP_EDID_BLOCK(0,
			    (struct CEA_extension_t *)&mhl_edid_3d_data->
			    EDID_block_data[EDID_BLOCK_SIZE], EDID_BLOCK_SIZE);
			for (index = 0; index < limit; ++index) {
				uint8_t VIC;

				VIC = mhl_edid_3d_data->parse_data.
					p_video_data_blocks_2d[vdb_index]->
					short_descriptors[index].VIC;
				if (VIC) {
					if (!IsQualifiedMhlVIC(
						mhl_edid_3d_data, VIC,
						burst_id_3D_DTD, &descriptor)) {
						mhl_edid_3d_data->
						    parse_data.
						    p_video_data_blocks_2d[
						    vdb_index]->
						short_descriptors[index].VIC =
						0;
					}
				}
			}
			DUMP_EDID_BLOCK(0,
				(struct CEA_extension_t *)&mhl_edid_3d_data->
				EDID_block_data[EDID_BLOCK_SIZE],
				EDID_BLOCK_SIZE);
			length_3D -= sizeof(*p_3D_structure_all);
		}
		p_3D_u.p_3D = &pThree3DSubBlock->
			HDMI_3D_sub_block_sans_mask.
			_3D_structure_and_detail_list[0];
		break;
	case 0x02:
		/*
		   3D_Structure_ALL_15..0 and 3D_mask_15..0 are present and
		   assign 3D formats to some of the VICS listed in the first
		   16 entries in the EDID

		 */
	{
		struct _3D_structure_all_t *p_3D_structure_all =
			(struct _3D_structure_all_t *)&mhl_edid_3d_data->
			parse_data.p_byte_13_through_byte_15->
			vicList[length_VIC];
		struct _3D_mask_t *p3DMask =
			(struct _3D_mask_t *)(p_3D_structure_all + 1);
		uint8_t mask;
		union video_burst_descriptor_u descriptor;

		descriptor.mhl2_3d_descriptor.left_right = p_3D_structure_all->
				_3D_structure_all_7_0.side_by_side;
		descriptor.mhl2_3d_descriptor.top_bottom = p_3D_structure_all->
				_3D_structure_all_15_8.top_bottom;
		descriptor.mhl2_3d_descriptor.frame_sequential =
				p_3D_structure_all->
				_3D_structure_all_15_8.frame_packing;
		DUMP_EDID_BLOCK(0,
			(struct CEA_extension_t *)&mhl_edid_3d_data->
			EDID_block_data[EDID_BLOCK_SIZE], EDID_BLOCK_SIZE);
		for (mask = 1, index = 0; (mask > 0) && (index < limit);
			++index, mask <<= 1) {
			uint8_t VIC;
			union video_burst_descriptor_u this_desc;

			if (mask & p3DMask->_3D_mask_7_0) {
				this_desc = descriptor;
			} else {
				this_desc.mhl2_3d_descriptor.left_right = 0;
				this_desc.mhl2_3d_descriptor.top_bottom = 0;
				this_desc.
					mhl2_3d_descriptor.frame_sequential = 0;
			}

			VIC = mhl_edid_3d_data->
				parse_data.p_video_data_blocks_2d[vdb_index]->
				short_descriptors[index].VIC;
			if (VIC) {
				if (!IsQualifiedMhlVIC(
				    mhl_edid_3d_data, VIC, burst_id_3D_VIC,
				    &this_desc)) {
					mhl_edid_3d_data->parse_data.
					p_video_data_blocks_2d[vdb_index]->
					short_descriptors[index].VIC = 0;
				}
			}
		}
		DUMP_EDID_BLOCK(0,
			(struct CEA_extension_t *)&mhl_edid_3d_data->
			EDID_block_data[EDID_BLOCK_SIZE], EDID_BLOCK_SIZE);
		for (mask = 1; (mask > 0) && (index < limit);
			++index, mask <<= 1) {
			uint8_t VIC;
			union video_burst_descriptor_u this_desc;

			if (mask & p3DMask->_3D_mask_15_8) {
				this_desc = descriptor;
			} else {
				this_desc.mhl2_3d_descriptor.left_right = 0;
				this_desc.mhl2_3d_descriptor.top_bottom = 0;
				this_desc.
					mhl2_3d_descriptor.frame_sequential = 0;
			}

			VIC = mhl_edid_3d_data->
				parse_data.p_video_data_blocks_2d[vdb_index]->
				short_descriptors[index].VIC;
			if (VIC) {
				if (!IsQualifiedMhlVIC(
				    mhl_edid_3d_data, VIC, burst_id_3D_VIC,
				    &this_desc)) {
					mhl_edid_3d_data->parse_data.
					p_video_data_blocks_2d[vdb_index]->
					short_descriptors[index].VIC = 0;
				}
			}
		}
		DUMP_EDID_BLOCK(0,
			(struct CEA_extension_t *)&mhl_edid_3d_data->
			EDID_block_data[EDID_BLOCK_SIZE], EDID_BLOCK_SIZE);
		length_3D -= sizeof(*p_3D_structure_all);
		length_3D -= sizeof(*p3DMask);
	}
		p_3D_u.p_3D = &pThree3DSubBlock->
			HDMI_3D_sub_block_with_all_AND_mask.
			_3D_structure_and_detail_list[0];
		mhl_edid_3d_data->parse_data.p_3d_mask = &pThree3DSubBlock->
			HDMI_3D_sub_block_with_all_AND_mask._3D_mask;
		break;
	case 0x03:
		/*
		 * Reserved for future use.
		 * 3D_Structure_ALL_15..0 and 3D_mask_15..0 are NOT present
		 */
		p_3D_u.p_3D = &pThree3DSubBlock->
			HDMI_3D_sub_block_sans_all_AND_mask.
			_3D_structure_and_detail_list[0];
		break;
	}

	mhl_edid_3d_data->parse_data.p_three_d = p_3D_u.p_3D;
	mhl_edid_3d_data->parse_data.p_3d_limit = &p_3D_u.p_bytes[length_3D];
	DUMP_EDID_BLOCK(0,
		(struct CEA_extension_t *)&mhl_edid_3d_data->
		EDID_block_data[EDID_BLOCK_SIZE], EDID_BLOCK_SIZE);
	while (p_3D_u.p_bytes < mhl_edid_3d_data->parse_data.p_3d_limit) {
		uint8_t _2D_VIC_order;
		enum _3D_structure_e _3D_structure;
		uint8_t VIC;

		_2D_VIC_order = p_3D_u.psans_byte1->byte0._2D_VIC_order;
		_3D_structure = p_3D_u.psans_byte1->byte0._3D_structure;
		VIC = mhl_edid_3d_data->
			parse_data.p_video_data_blocks_2d[vdb_index]->
			short_descriptors[_2D_VIC_order].VIC;

		/* this VIC position might have gotten disqualified already */
		if (VIC) {
			union video_burst_descriptor_u descriptor;
			descriptor.mhl2_3d_descriptor.left_right = 0;
			descriptor.mhl2_3d_descriptor.top_bottom = 0;
			descriptor.mhl2_3d_descriptor.frame_sequential = 0;
			switch (_3D_structure) {
			case tdsSideBySide:
				{
					/*TODO: re-visit uint8_t _3D_detail =
					 * p_3D_u.pwith_byte1->byte1._3D_detail;
					 */
					descriptor.mhl2_3d_descriptor.
						left_right = 1;
				}
				break;
			case tdsTopAndBottom:
				descriptor.mhl2_3d_descriptor.top_bottom = 1;
				break;
			case tdsFramePacking:
				descriptor.mhl2_3d_descriptor.
					frame_sequential = 1;
				break;
			}
			if (!IsQualifiedMhlVIC(
				mhl_edid_3d_data,
				VIC, burst_id_3D_VIC,
				&descriptor)) {
				mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[vdb_index]->
				    short_descriptors[_2D_VIC_order].VIC = 0;
			}
		}
		if (_3D_structure < tdsSideBySide)
			p_3D_u.psans_byte1++;
		else
			p_3D_u.pwith_byte1++;
	}
	DUMP_EDID_BLOCK(0,
		(struct CEA_extension_t *)&mhl_edid_3d_data->
		EDID_block_data[EDID_BLOCK_SIZE], EDID_BLOCK_SIZE);
}

void si_mhl_tx_display_timing_enumeration_end(
	struct edid_3d_data_t *mhl_edid_3d_data)
{
	mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress = 0;
	/* finish off with any 3D modes reported via the HDMI VSDB */
	si_mhl_tx_enumerate_hdmi_vsdb(mhl_edid_3d_data);
	/* notify the app (board specific) layer */
	display_timing_enumeration_end(mhl_edid_3d_data);
	SET_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DONE);
	si_mhl_tx_prune_edid(mhl_edid_3d_data);
}

static void CheckForAll3DBurstDone(struct edid_3d_data_t *mhl_edid_3d_data)
{
	if (TEST_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_VIC_DONE)) {
		if (TEST_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DTD_DONE)) {
			if (!TEST_3D_FLAG
			    (mhl_edid_3d_data, FLAGS_BURST_3D_DONE)) {
				/* For MHL 3.0 and newer peers,
				 * we wait for MHL_INT_FEAT_COMPLETE
				 */
				if (si_get_peer_mhl_version
				    (mhl_edid_3d_data->dev_context) < 0x30) {
					si_mhl_tx_display_timing_enumeration_end
					    (mhl_edid_3d_data);
				}
			}
		}
	}
}

/*
*/
void si_mhl_tx_process_3d_vic_burst(void *context,
	struct MHL2_video_format_data_t *p_write_burst_data)
{
	struct edid_3d_data_t *mhl_edid_3d_data;
	uint8_t block_index = 0;
	/*
	 * TODO: re-visit uint8_t edidLimit =
	 * mhl_edid_3d_data->parse_data.p_byte_13_through_byte_15->
	 * byte14.HDMI_3D_len;
	 */
	mhl_edid_3d_data = (struct edid_3d_data_t *)context;

	MHL_TX_EDID_INFO("burstEntryCount3D_VIC: %d\n",
			 mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic);
	if (1 == p_write_burst_data->header.sequence_index) {
		size_t _3d_vic_size;
		_3d_vic_size =
		    p_write_burst_data->header.total_entries *
		    sizeof(*mhl_edid_3d_data->_3d_vic_list);
		if (p_write_burst_data->header.total_entries >=
		    mhl_edid_3d_data->_3d_vic_info.num_items_allocated) {
			if (NULL != mhl_edid_3d_data->_3d_vic_list)
				kfree(mhl_edid_3d_data->_3d_vic_list);

			mhl_edid_3d_data->_3d_vic_list =
			    kmalloc(_3d_vic_size, GFP_KERNEL);

			if (NULL == mhl_edid_3d_data->_3d_vic_list) {
				mhl_edid_3d_data->_3d_vic_info.
				    num_items_allocated = 0;
				mhl_edid_3d_data->_3d_vic_info.num_items = 0;
				MHL_TX_DBG_ERR
				    ("%sunable to allocate memory%s\n",
				     ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
				return;
			} else {
				mhl_edid_3d_data->_3d_vic_info.
				    num_items_allocated =
				    p_write_burst_data->header.total_entries;
			}
		}
		mhl_edid_3d_data->_3d_vic_info.num_items =
		    p_write_burst_data->header.total_entries;
	}

	if (NULL == mhl_edid_3d_data->_3d_vic_list) {
		MHL_TX_DBG_ERR("%s no place to put 3D_VIC burst%s\n",
			       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return;
	}
	if (mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress) {
		/* check to see if it's time to move on to the next block */
		if (mhl_edid_3d_data->parse_data.vic_2d_index >=
		    mhl_edid_3d_data->parse_data.
		    p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.
					   video_data_block_index]->header.
		    fields.length_following_header) {
			mhl_edid_3d_data->parse_data.video_data_block_index++;
			if (mhl_edid_3d_data->parse_data.
			    video_data_block_index >=
			    mhl_edid_3d_data->parse_data.
			    num_video_data_blocks) {
				SET_3D_FLAG(mhl_edid_3d_data,
					    FLAGS_BURST_3D_VIC_DONE);
				    CheckForAll3DBurstDone(mhl_edid_3d_data);
				return;
			}
		}

		if (mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic <
		    mhl_edid_3d_data->parse_data.
		    p_video_data_blocks_2d[mhl_edid_3d_data->parse_data.
					   video_data_block_index]->header.
		    fields.length_following_header) {
			/* each SVD is 1 byte long */
			DUMP_EDID_BLOCK(0,
				(struct CEA_extension_t *)&mhl_edid_3d_data->
				EDID_block_data[EDID_BLOCK_SIZE],
				EDID_BLOCK_SIZE);
			/* block_index is set to zero above */
			for (; (block_index <
					 p_write_burst_data->
					 num_entries_this_burst)
					&& (mhl_edid_3d_data->parse_data.
					    burst_entry_count_3d_vic <
					    p_write_burst_data->header.
					    total_entries);
					++block_index,
					++mhl_edid_3d_data->parse_data.
					burst_entry_count_3d_vic,
					++mhl_edid_3d_data->parse_data.
					vic_2d_index) {
				uint8_t VIC;
				struct cea_short_descriptor_t svd;
				uint8_t this_mode_doable = 0;
				union video_burst_descriptor_u *p_descriptor;
				/*
				 * Check to see if it's time to move on
				 * to the next block
				 */
				if (mhl_edid_3d_data->parse_data.vic_2d_index >=
				    mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[mhl_edid_3d_data->
						parse_data.
						video_data_block_index]->
				    header.fields.length_following_header) {
					mhl_edid_3d_data->parse_data.
					    video_data_block_index++;
					if (mhl_edid_3d_data->parse_data.
					    video_data_block_index >=
					    mhl_edid_3d_data->parse_data.
					    num_video_data_blocks) {
						SET_3D_FLAG(
						    mhl_edid_3d_data,
						    FLAGS_BURST_3D_VIC_DONE);
						break;
					}
				}

				p_descriptor =
				    (union video_burst_descriptor_u *) &
				    p_write_burst_data->
				    video_descriptors[block_index];
				svd =
				    mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[mhl_edid_3d_data->
					parse_data.
					video_data_block_index]->
					short_descriptors[mhl_edid_3d_data->
					    parse_data.vic_2d_index];
				VIC = svd.VIC;
				mhl_edid_3d_data->
				    _3d_vic_list[mhl_edid_3d_data->parse_data.
						 vic_2d_index].svd = svd;
				mhl_edid_3d_data->
				    _3d_vic_list[mhl_edid_3d_data->parse_data.
						 vic_2d_index]._3d_info =
				    p_descriptor->mhl3_3d_descriptor;

				if (VIC) {
					MHL_TX_EDID_INFO(
					    "short Descriptor[%d] 3D VIC: "
					    "%d %s %s %s\n",
					    mhl_edid_3d_data->parse_data.
					    burst_entry_count_3d_vic, VIC,
					    p_descriptor->mhl2_3d_descriptor.
					    left_right ? psz_left_right :
						psz_space,
					    p_descriptor->mhl2_3d_descriptor.
					    top_bottom ? psz_top_bottom :
						psz_space,
					    p_descriptor->mhl2_3d_descriptor.
					    frame_sequential ?
					    psz_frame_sequential : psz_space);
					this_mode_doable =
					    IsQualifiedMhlVIC(mhl_edid_3d_data,
							      VIC,
							      burst_id_3D_VIC,
							      p_descriptor);
					if (!this_mode_doable) {
						MHL_TX_EDID_INFO
						    ("'can't do CEA VIC:%d\n",
						     (uint16_t) VIC);
						mhl_edid_3d_data->parse_data.
						    p_video_data_blocks_2d
						    [mhl_edid_3d_data->
						    parse_data.
						    video_data_block_index]->
						    short_descriptors
						    [mhl_edid_3d_data->
						    parse_data.vic_2d_index].
						    VIC = 0;
					}
				}
			}
			DUMP_EDID_BLOCK(0,
				(struct CEA_extension_t *)&mhl_edid_3d_data->
				EDID_block_data[EDID_BLOCK_SIZE],
				EDID_BLOCK_SIZE);
		}

		if (mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic >=
		    p_write_burst_data->header.total_entries) {
			SET_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_VIC_DONE);
		}
		CheckForAll3DBurstDone(mhl_edid_3d_data);
	}
}

void check_3d_dtd_sequence_done(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL2_video_format_data_t *p_write_burst_data, uint8_t dtd_limit)
{
	int flag = 0;
	if (mhl_edid_3d_data->parse_data.cea_861_dtd_index >= dtd_limit)
		flag = 1;

	if (mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd >=
	    p_write_burst_data->header.total_entries) {
		flag = 1;
	}
	if (flag) {
		SET_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DTD_DONE);
		if (TEST_3D_FLAG(mhl_edid_3d_data,
			FLAGS_BURST_3D_VIC_DONE)) {
			if (!TEST_3D_FLAG(mhl_edid_3d_data,
				FLAGS_BURST_3D_DONE)) {
				/*
				 * For MHL 3.0 and newer peers, wait for
				 * MHL_INT_FEAT_COMPLETE
				 */
				if (si_get_peer_mhl_version(
					mhl_edid_3d_data->dev_context) < 0x30) {
					si_mhl_tx_display_timing_enumeration_end
						(mhl_edid_3d_data);
				}
			}
		}
	}
}

static void process_cea_dtds(struct edid_3d_data_t *mhl_edid_3d_data,
		struct MHL2_video_format_data_t *p_write_burst_data,
		int burst_index)
{
	uint8_t is_timing = 0;
	union video_burst_descriptor_u *p_burst_desc;
	struct CEA_extension_t *p_CEA_extension =
	    (struct CEA_extension_t *)&mhl_edid_3d_data->
	    EDID_block_data[EDID_BLOCK_SIZE];
#if 0
	uint8_t dtd_limit =
	    (uint8_t) p_CEA_extension->version_u.version3.misc_support.
	    total_number_native_dtds_in_entire_EDID;
#else
	uint8_t dtd_limit =
		(EDID_BLOCK_SIZE
			- p_CEA_extension->byte_offset_to_18_byte_descriptors)
		/ sizeof(struct detailed_timing_descriptor_t);
#endif
	union {
		uint8_t *puc_data_block;
		union _18_byte_descriptor_u *p_long_descriptors;
	} p_data_u;
	p_data_u.p_long_descriptors =
	    (union _18_byte_descriptor_u *)(((uint8_t *)p_CEA_extension) +
			p_CEA_extension->byte_offset_to_18_byte_descriptors);
	MHL_TX_EDID_INFO("continuing with CEA-861-D/E DTDs"
			 "\n\tburst_index: %d"
			 "\n\tburst_entry_count_3d_dtd: %d"
			 "\n\tnum_entries_this_burst: %d"
			 "\n\ttotal_entries:%d"
			 "\n\tdtd_limit:%d"
			 "\n\toffsetTo18_byte_descriptors:0x%x\n",
			 burst_index,
			 mhl_edid_3d_data->parse_data.
			 burst_entry_count_3d_dtd,
			 p_write_burst_data->
			 num_entries_this_burst,
			 p_write_burst_data->header.
			 total_entries, dtd_limit,
			 p_CEA_extension->
			 byte_offset_to_18_byte_descriptors);
	/* continue with CEA-861-D/E DTDs when done with VESA DTDs */
	/* burst_index is set to zero above */
	for (; (burst_index <
		     p_write_burst_data->num_entries_this_burst)
		    && (mhl_edid_3d_data->parse_data.
			burst_entry_count_3d_dtd <
			p_write_burst_data->header.
			total_entries)
		    && (mhl_edid_3d_data->parse_data.
			cea_861_dtd_index < dtd_limit);
		    ++mhl_edid_3d_data->parse_data.
		    cea_861_dtd_index) {
		union _18_byte_descriptor_u *p_desc =
		    &p_data_u.
		    p_long_descriptors[mhl_edid_3d_data->
				       parse_data.
				       cea_861_dtd_index];
		bool is_valid = 0;
		p_burst_desc =
		    (union video_burst_descriptor_u *)
		    &p_write_burst_data->
		    video_descriptors[burst_index];
		is_valid =
		    si_mhl_tx_parse_detailed_timing_descriptor
		    (mhl_edid_3d_data, p_desc, EDID_BLOCK_2_3,
		     &is_timing, burst_id_3D_DTD, p_burst_desc);
		/* only count it if it's a valid timing */
		if (is_timing) {

			if (is_valid) {
				MHL_TX_EDID_INFO
				    ("CEA-861 DTD index: %d burst index:%d DTD "
				     "SP index:%d %s %s %s\n\n",
				     (uint16_t)mhl_edid_3d_data->
				     parse_data.cea_861_dtd_index,
				     (uint16_t)burst_index,
				     (uint16_t)mhl_edid_3d_data->
				     parse_data.burst_entry_count_3d_dtd,
				     p_burst_desc->mhl2_3d_descriptor.
				     left_right ? psz_left_right : psz_space,
				     p_burst_desc->mhl2_3d_descriptor.
				     top_bottom ? psz_top_bottom : psz_space,
				     p_burst_desc->mhl2_3d_descriptor.
				     frame_sequential ?
					psz_frame_sequential : psz_space);
				mhl_edid_3d_data->
				    _3d_dtd_list[mhl_edid_3d_data->
				     parse_data.num_vesa_timing_dtds +
				     mhl_edid_3d_data->parse_data.
				     cea_861_dtd_index].dtd_cea_861 =
					p_desc->dtd;
				mhl_edid_3d_data->_3d_dtd_list[
				     mhl_edid_3d_data->parse_data.
				     num_vesa_timing_dtds +
				     mhl_edid_3d_data->parse_data.
				     cea_861_dtd_index]._3d_info =
				    p_burst_desc->mhl3_3d_descriptor;
			} else {
				/*
				 * Mark this mode for pruning by setting
				 * horizontal active to zero
				 */
				MHL_TX_DBG_INFO("%smark for pruning%s\n",
					ANSI_ESC_YELLOW_TEXT,
					ANSI_ESC_RESET_TEXT);
				p_desc->dtd.horz_active_7_0 = 0;
				p_desc->dtd.horz_active_blanking_high.
				    horz_active_11_8 = 0;
			}

			++burst_index;
			++mhl_edid_3d_data->parse_data.
			    burst_entry_count_3d_dtd;
		} else {
			MHL_TX_EDID_INFO
			    ("CEA-861 DTD index: %d\n",
			     (uint16_t) mhl_edid_3d_data->
			     parse_data.vesa_dtd_index);
		}
	}

	MHL_TX_EDID_INFO("DTD burst complete\n");
	check_3d_dtd_sequence_done(mhl_edid_3d_data,
		p_write_burst_data, dtd_limit);
}

static void print_vesa_dtd_index(int burst_index,
		struct edid_3d_data_t *mhl_edid_3d_data,
		union video_burst_descriptor_u *p_burst_desc)
{
	MHL_TX_EDID_INFO(
		"VESA DTD index: %d burst index:%d DTD SP index:%d %s %s %s\n",
		(uint16_t)mhl_edid_3d_data->parse_data.vesa_dtd_index,
		(uint16_t) burst_index,
		(uint16_t)mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd,
		p_burst_desc->mhl2_3d_descriptor.left_right ?
			psz_left_right : psz_space,
		p_burst_desc->mhl2_3d_descriptor.top_bottom ?
			psz_top_bottom : psz_space,
		p_burst_desc->mhl2_3d_descriptor.frame_sequential ?
			psz_frame_sequential : psz_space);
}


void si_mhl_tx_process_3d_dtd_burst(void *context,
	struct MHL2_video_format_data_t *p_write_burst_data)
{
	struct edid_3d_data_t *mhl_edid_3d_data;
	struct EDID_block0_t *p_EDID_block_0;
	union video_burst_descriptor_u *p_burst_desc;
	int burst_index = 0;
	uint8_t is_timing = 0;
	struct CEA_extension_t *p_CEA_extension;
	uint8_t dtd_limit;

	mhl_edid_3d_data = (struct edid_3d_data_t *)context;
	p_CEA_extension = (struct CEA_extension_t *) &mhl_edid_3d_data->
	    EDID_block_data[EDID_BLOCK_SIZE];
#if 0
	dtd_limit = (uint8_t) p_CEA_extension->version_u.version3.misc_support.
	    total_number_native_dtds_in_entire_EDID;
#else

	dtd_limit = (EDID_BLOCK_SIZE
			- p_CEA_extension->byte_offset_to_18_byte_descriptors)
		/ sizeof(struct detailed_timing_descriptor_t) ;
#endif

	if (1 == p_write_burst_data->header.sequence_index) {
		size_t _3d_dtd_size;
		_3d_dtd_size =
		    p_write_burst_data->header.total_entries *
		    sizeof(*mhl_edid_3d_data->_3d_dtd_list);
		if (p_write_burst_data->header.total_entries >=
		    mhl_edid_3d_data->_3d_dtd_info.num_items_allocated) {
			if (NULL != mhl_edid_3d_data->_3d_dtd_list)
				kfree(mhl_edid_3d_data->_3d_dtd_list);

			mhl_edid_3d_data->_3d_dtd_list =
			    kmalloc(_3d_dtd_size, GFP_KERNEL);
			if (NULL == mhl_edid_3d_data->_3d_dtd_list) {
				mhl_edid_3d_data->_3d_dtd_info.
				    num_items_allocated = 0;
				mhl_edid_3d_data->_3d_dtd_info.num_items = 0;
				MHL_TX_DBG_ERR
				    ("%sunable to allocate memory%s\n",
				     ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
				return;
			} else {
				mhl_edid_3d_data->_3d_dtd_info.
				    num_items_allocated =
				    p_write_burst_data->header.total_entries;
			}
		}
		mhl_edid_3d_data->_3d_dtd_info.num_items =
		    p_write_burst_data->header.total_entries;
	}
	if (NULL == mhl_edid_3d_data->_3d_dtd_list) {
		MHL_TX_DBG_ERR("%s no place to put 3D_DTD burst%s\n",
			       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return;
	}
	if (!mhl_edid_3d_data->parse_data.flags.parse_3d_in_progress)
		return;

	if (TEST_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DTD_VESA_DONE)) {
		process_cea_dtds(mhl_edid_3d_data,
				p_write_burst_data,
				burst_index);
		return;
	}


	p_EDID_block_0 = (struct EDID_block0_t *)&mhl_edid_3d_data->
		EDID_block_data[0];
	/*
	 * Up to four DTDs are possible in the base VESA EDID
	 * this will be covered by a single burst.
	 */
#define DESC_COUNT	(sizeof(p_EDID_block_0->detailed_timing_descriptors) / \
			sizeof(p_EDID_block_0->detailed_timing_descriptors[0]))

	for (; (burst_index < p_write_burst_data->num_entries_this_burst) &&
		    (mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd <
			p_write_burst_data->header.total_entries) &&
		    (mhl_edid_3d_data->parse_data.vesa_dtd_index < DESC_COUNT);
		    ++mhl_edid_3d_data->parse_data.vesa_dtd_index) {
		union _18_byte_descriptor_u *p_desc =
		    (union _18_byte_descriptor_u *)
		    &p_EDID_block_0->detailed_timing_descriptors[
			mhl_edid_3d_data->parse_data.vesa_dtd_index];
		bool is_valid = 0;

		p_burst_desc =
		    (union video_burst_descriptor_u *) &
		    p_write_burst_data->
		    video_descriptors[burst_index];
		is_valid =
		    si_mhl_tx_parse_detailed_timing_descriptor
		    (mhl_edid_3d_data, p_desc, EDID_BLOCK_0,
		     &is_timing, burst_id_3D_DTD, p_burst_desc);

		if (is_timing) {

			if (is_valid) {
				print_vesa_dtd_index(burst_index,
					mhl_edid_3d_data,
					p_burst_desc);
				mhl_edid_3d_data->_3d_dtd_list[
					mhl_edid_3d_data->parse_data.
					vesa_dtd_index].dtd_cea_861 =
					    p_desc->dtd;
				mhl_edid_3d_data->_3d_dtd_list[
					mhl_edid_3d_data->parse_data.
					vesa_dtd_index]._3d_info =
					    p_burst_desc->mhl3_3d_descriptor;
			} else {
				MHL_TX_DBG_INFO("%smark for pruning%s\n",
					ANSI_ESC_YELLOW_TEXT,
					ANSI_ESC_RESET_TEXT);
				/*
				 * Mark this mode for pruning by setting
				 * horizontal active to zero
				 */
				p_desc->dtd.horz_active_7_0 = 0;
				p_desc->dtd.horz_active_blanking_high.
					horz_active_11_8 = 0;
			}

			burst_index++;
			mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd++;
		} else {
			MHL_TX_EDID_INFO("VESA DTD index: %d\n",
				(uint16_t)mhl_edid_3d_data->
				parse_data.vesa_dtd_index);
		}
	}

	if (mhl_edid_3d_data->parse_data.vesa_dtd_index >=
	    sizeof(p_EDID_block_0->
		   detailed_timing_descriptors) /
	    sizeof(p_EDID_block_0->
		   detailed_timing_descriptors[0])) {
		/* we got past the VESA DTDs in this burst */
		SET_3D_FLAG(mhl_edid_3d_data,
			    FLAGS_BURST_3D_DTD_VESA_DONE);
	} else {
		check_3d_dtd_sequence_done(mhl_edid_3d_data,
					   p_write_burst_data,
					   dtd_limit);
		/* more VESA DTDs to process in next burst */
		MHL_TX_EDID_INFO(
			"%s\n",
			TEST_3D_FLAG(mhl_edid_3d_data,
				FLAGS_BURST_3D_DTD_DONE) ?
				 "3D DTD descriptors exhausted"
				 : "more VESA DTDs to process");
		return;
	}

	process_cea_dtds(mhl_edid_3d_data, p_write_burst_data, burst_index);
}

void si_mhl_tx_process_hev_vic_burst(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL3_hev_vic_data_t *p_burst)
{
	int i, hev_index = mhl_edid_3d_data->hev_vic_info.index;

	if (0 == p_burst->header.total_entries) {
		MHL_TX_DBG_ERR("invalid zero value for total entries\n")
		return;
	}
	if (1 == p_burst->header.sequence_index) {
		size_t hev_vic_size;
		hev_vic_size =
		    p_burst->header.total_entries *
		    sizeof(*mhl_edid_3d_data->hev_vic_list);
		if (p_burst->header.total_entries >
		    mhl_edid_3d_data->hev_vic_info.num_items_allocated) {
			if (NULL != mhl_edid_3d_data->hev_vic_list)
				kfree(mhl_edid_3d_data->hev_vic_list);

			mhl_edid_3d_data->hev_vic_list =
			    kmalloc(hev_vic_size, GFP_KERNEL);
			if (NULL == mhl_edid_3d_data->hev_vic_list) {
				mhl_edid_3d_data->hev_vic_info.
				    num_items_allocated = 0;
				mhl_edid_3d_data->hev_vic_info.num_items = 0;
				MHL_TX_DBG_ERR
				    ("%sunable to allocate memory%s\n",
				     ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
				return;
			} else {
				MHL_TX_DBG_WARN(" %d %pK\n", hev_index,
					mhl_edid_3d_data->hev_vic_list)
				mhl_edid_3d_data->hev_vic_info.
				    num_items_allocated =
				    p_burst->header.total_entries;
			}
		}
		mhl_edid_3d_data->hev_vic_info.num_items =
		    p_burst->header.total_entries;
	} else if (NULL == mhl_edid_3d_data->hev_vic_list) {
		MHL_TX_DBG_ERR("bogus write burst, no hev_vic_list\n")
		return;
	}
	MHL_TX_DBG_WARN(" %d %pK\n", hev_index, mhl_edid_3d_data->hev_vic_list)
	if (NULL == mhl_edid_3d_data->hev_vic_list) {
		MHL_TX_DBG_ERR("%s no place to put HEV_VIC burst%s\n",
			       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return;
	}
	for (i = 0; (i < p_burst->num_entries_this_burst) &&
			(hev_index < p_burst->header.total_entries);
				++i, ++hev_index) {
		/* We don't have an EDID entry to prune here, so just
		   throw away the return value
		 */

		if (IsQualifiedMhlVIC
		    (mhl_edid_3d_data,
		     p_burst->video_descriptors[i].vic_cea861f,
		     burst_id_HEV_VIC,
		     (union video_burst_descriptor_u *) &p_burst->
		     video_descriptors[i])) {
			MHL_TX_DBG_INFO(" %d %pK\n",
				hev_index, mhl_edid_3d_data->hev_vic_list)
			mhl_edid_3d_data->hev_vic_list[hev_index].
			    mhl3_hev_vic_descriptor =
			    p_burst->video_descriptors[i];
		}
	}
	mhl_edid_3d_data->hev_vic_info.index = hev_index;
}

void si_mhl_tx_process_hev_dtd_a_burst(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL3_hev_dtd_a_data_t *p_burst)
{
	mhl_edid_3d_data->hev_dtd_payload.sequence_index =
	    p_burst->header.sequence_index;
	mhl_edid_3d_data->hev_dtd_payload.a = p_burst->payload;
	if (0 == p_burst->header.total_entries) {
		MHL_TX_DBG_ERR("invalid zero value for total entries\n")
		return;
	}
	if (1 == p_burst->header.sequence_index) {
		size_t hev_dtd_size;
		hev_dtd_size =
		    p_burst->header.total_entries *
		    sizeof(*mhl_edid_3d_data->hev_dtd_list);
		if (p_burst->header.total_entries >=
		    mhl_edid_3d_data->hev_dtd_info.num_items_allocated) {
			if (NULL != mhl_edid_3d_data->hev_dtd_list)
				kfree(mhl_edid_3d_data->hev_dtd_list);

			mhl_edid_3d_data->hev_dtd_list =
			    kmalloc(hev_dtd_size, GFP_KERNEL);
			if (NULL == mhl_edid_3d_data->hev_dtd_list) {
				mhl_edid_3d_data->hev_dtd_info.
				    num_items_allocated = 0;
				mhl_edid_3d_data->hev_dtd_info.num_items = 0;
				MHL_TX_DBG_ERR
				    ("%sunable to allocate memory%s\n",
				     ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
				return;
			} else {
				mhl_edid_3d_data->hev_dtd_info.
				    num_items_allocated =
				    p_burst->header.total_entries;
			}
		}
		mhl_edid_3d_data->hev_dtd_info.num_items =
		    p_burst->header.total_entries;
	}
	if (mhl_edid_3d_data->hev_dtd_list) {
		if (p_burst->header.total_entries <
		    mhl_edid_3d_data->hev_dtd_info.num_items) {
			mhl_edid_3d_data->hev_dtd_list[p_burst->header.
						       sequence_index - 1].a =
			    p_burst->payload;
		}
	}
}

void si_mhl_tx_process_hev_dtd_b_burst(struct edid_3d_data_t *mhl_edid_3d_data,
	struct MHL3_hev_dtd_b_data_t *p_burst)
{
	mhl_edid_3d_data->hev_dtd_payload.b = p_burst->payload;

	if (0 == p_burst->header.total_entries) {
		MHL_TX_DBG_ERR("invalid zero value for total entries\n")
		return;
	}
	if (mhl_edid_3d_data->hev_dtd_list) {
		if (p_burst->header.total_entries <
		    mhl_edid_3d_data->hev_dtd_info.num_items) {
			mhl_edid_3d_data->hev_dtd_list[p_burst->header.
						       sequence_index - 1].b =
			    p_burst->payload;
		}
	}
	/* no EDID to prune here, so throw away the result */
	is_MHL_timing_mode(mhl_edid_3d_data, 0, 0, 0, burst_id_HEV_DTDB,
			   (union video_burst_descriptor_u *) p_burst, 0);
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

#define DUMP_OFFSET(s, m) \
	MHL_TX_EDID_INFO(STRINGIZE(m)" offset:%x\n", SII_OFFSETOF(s, m))

#define DUMP_ESTABLISHED_TIMING(group, width, height, refresh, progressive) \
if (p_EDID_block_0->group.et##width##x##height##_##refresh##progressive) { \
	MHL_TX_EDID_INFO(STRINGIZE(group)"."STRINGIZE(width)"x" \
		STRINGIZE(height)"@"STRINGIZE(refresh)STRINGIZE(progressive) \
		"\n"); \
	if (!is_MHL_timing_mode(mhl_edid_3d_data, width, height, refresh*1000, \
		0, NULL, 0)) { \
		p_EDID_block_0-> \
		group.et##width##x##height##_##refresh##progressive = 0; \
	} \
}

static void si_mhl_tx_parse_established_timing(
	struct edid_3d_data_t *mhl_edid_3d_data,
	struct EDID_block0_t *p_EDID_block_0)
{
	DUMP_OFFSET(struct EDID_block0_t, header_data[0]);
	DUMP_OFFSET(struct EDID_block0_t, id_manufacturer_name);
	DUMP_OFFSET(struct EDID_block0_t, id_product_code);
	DUMP_OFFSET(struct EDID_block0_t, serial_number[0]);
	DUMP_OFFSET(struct EDID_block0_t, week_of_manufacture);
	DUMP_OFFSET(struct EDID_block0_t, year_of_manufacture);
	DUMP_OFFSET(struct EDID_block0_t, EDID_version);
	DUMP_OFFSET(struct EDID_block0_t, EDID_revision);
	DUMP_OFFSET(struct EDID_block0_t, video_input_definition);
	DUMP_OFFSET(struct EDID_block0_t, horz_screen_size_or_aspect_ratio);
	DUMP_OFFSET(struct EDID_block0_t, vert_screen_size_or_aspect_ratio);
	DUMP_OFFSET(struct EDID_block0_t, display_transfer_characteristic);
	DUMP_OFFSET(struct EDID_block0_t, feature_support);
	DUMP_OFFSET(struct EDID_block0_t, red_green_bits_1_0);
	DUMP_OFFSET(struct EDID_block0_t, blue_white_bits_1_0);
	DUMP_OFFSET(struct EDID_block0_t, red_x);
	DUMP_OFFSET(struct EDID_block0_t, red_y);
	DUMP_OFFSET(struct EDID_block0_t, green_x);
	DUMP_OFFSET(struct EDID_block0_t, green_y);
	DUMP_OFFSET(struct EDID_block0_t, blue_x);
	DUMP_OFFSET(struct EDID_block0_t, blue_y);
	DUMP_OFFSET(struct EDID_block0_t, white_x);
	DUMP_OFFSET(struct EDID_block0_t, white_y);

	/* MHL cannot support these modes, so prune them */
	p_EDID_block_0->established_timings_II.et1280x1024_75p = 0;
	p_EDID_block_0->manufacturers_timings.et1152x870_75p = 0;

	MHL_TX_EDID_INFO("Parsing Established Timing:\n");
	MHL_TX_EDID_INFO("===========================\n");

	/* Parse Established Timing Byte #0 */
	DUMP_OFFSET(struct EDID_block0_t, established_timings_I);
	DUMP_ESTABLISHED_TIMING(established_timings_I, 720, 400, 70, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 720, 400, 88, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 640, 480, 60, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 640, 480, 67, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 640, 480, 72, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 640, 480, 75, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 800, 600, 56, p)
	DUMP_ESTABLISHED_TIMING(established_timings_I, 800, 600, 60, p)

	/* Parse Established Timing Byte #1: */
	DUMP_OFFSET(struct EDID_block0_t, established_timings_II);
	DUMP_ESTABLISHED_TIMING(established_timings_II, 800, 600, 72, p)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 800, 600, 75, p)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 832, 624, 75, p)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 1024, 768, 87, i)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 1024, 768, 60, p)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 1024, 768, 70, p)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 1024, 768, 75, p)
	DUMP_ESTABLISHED_TIMING(established_timings_II, 1280, 1024, 75, p)

	/* Parse Established Timing Byte #2: */
	DUMP_OFFSET(struct EDID_block0_t, manufacturers_timings);
	DUMP_ESTABLISHED_TIMING(manufacturers_timings, 1152, 870, 75, p)

	if ((!p_EDID_block_0->header_data[0]) &&
		(0 == *((uint8_t *) &p_EDID_block_0->established_timings_II)) &&
		(!p_EDID_block_0->header_data[2])) {
		MHL_TX_EDID_INFO("No established video modes\n");
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

static void si_mhl_tx_parse_standard_timing(
	struct edid_3d_data_t *mhl_edid_3d_data,
	struct EDID_block0_t *p_EDID_block_0)
{
	uint8_t i;
	uint8_t AR_code;

	MHL_TX_EDID_INFO("Parsing Standard Timing:\n");
	MHL_TX_EDID_INFO("========================\n");

	for (i = 0;
	     i <
	     sizeof(p_EDID_block_0->standard_timings) /
	     sizeof(p_EDID_block_0->standard_timings[0]); i += 2) {
		if ((1 ==
		     p_EDID_block_0->standard_timings[i].
		     horz_pix_div_8_minus_31)
		    && (1 ==
			p_EDID_block_0->standard_timings[i].
			field_refresh_rate_minus_60)
		    && (0 ==
			p_EDID_block_0->standard_timings[i].image_aspect_ratio)
		    ) {
			/* per VESA EDID standard, Release A, Revision 1,
			 * February 9, 2000, Sec. 3.9
			 */
			MHL_TX_EDID_INFO("Standard Timing Undefined\n");
		} else {
			uint16_t horz_active =
			    (uint16_t) ((p_EDID_block_0->standard_timings[i].
					 horz_pix_div_8_minus_31 + 31) * 8);
			uint16_t vert_active = 0;
			uint16_t refresh_rate_in_milliHz =
			    (uint16_t) (p_EDID_block_0->standard_timings[i].
					field_refresh_rate_minus_60 +
					60) * 1000;
			char *psz_ratio_string = "";

			/* per VESA EDID standard, Release A, Revision 1,
			 * February 9, 2000, Table 3.15
			 */
			AR_code =
			    p_EDID_block_0->standard_timings[i].
			    image_aspect_ratio;

			switch (AR_code) {
			case iar_16_to_10:
				psz_ratio_string = "16:10";
				vert_active = horz_active * 10 / 16;
				break;

			case iar_4_to_3:
				psz_ratio_string = "4:3";
				vert_active = horz_active * 3 / 4;
				break;

			case iar_5_to_4:
				psz_ratio_string = "5:4";
				vert_active = horz_active * 4 / 5;
				break;

			case iar_16_to_9:
				psz_ratio_string = "16:9";
				vert_active = horz_active * 9 / 16;
				break;
			}
			MHL_TX_EDID_INFO
			    ("Aspect Ratio: %5s %4d x %4d at %3d Hz.\n",
			     psz_ratio_string, horz_active, vert_active,
			     refresh_rate_in_milliHz);

			if (!is_MHL_timing_mode
			    (mhl_edid_3d_data, horz_active, vert_active,
			     refresh_rate_in_milliHz, 0, NULL, 0)) {
				/* disable this mode */
				p_EDID_block_0->standard_timings[i].
				    horz_pix_div_8_minus_31 = 1;
				p_EDID_block_0->standard_timings[i].
				    field_refresh_rate_minus_60 = 1;
				p_EDID_block_0->standard_timings[i].
				    image_aspect_ratio = 0;
				MHL_TX_EDID_INFO("Disabled\n\n");
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

static void si_mhl_tx_parse_block_zero_timing_descriptors(
	struct edid_3d_data_t *mhl_edid_3d_data,
	struct EDID_block0_t *p_EDID_block_0)
{
	uint8_t i;
	uint8_t is_timing = 0;
	si_mhl_tx_parse_established_timing(mhl_edid_3d_data, p_EDID_block_0);
	si_mhl_tx_parse_standard_timing(mhl_edid_3d_data, p_EDID_block_0);

	for (i = 0; i <
		sizeof(p_EDID_block_0->detailed_timing_descriptors) /
		sizeof(p_EDID_block_0->detailed_timing_descriptors[0]); i++) {
		MHL_TX_EDID_INFO
		    ("EDID Block #0, Detailed Descriptor Number %d:\n", (int)i);
		MHL_TX_EDID_INFO
		    ("============================================\n\n");
		si_mhl_tx_parse_detailed_timing_descriptor(mhl_edid_3d_data,
			(union _18_byte_descriptor_u *) &p_EDID_block_0->
			detailed_timing_descriptors[i], EDID_BLOCK_0,
			&is_timing, 0, NULL);
		if (is_timing)
			++mhl_edid_3d_data->parse_data.num_vesa_timing_dtds;
	}
}

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

	for (i = 0; i < EDID_BLOCK_SIZE; i++)
		checksum += p_EDID_block_data[i];

	if (checksum)
		return false;

	return true;
}

/*
 * Checks if EDID header is correct per VESA E-EDID standard
 * Must be 00 FF FF FF FF FF FF 00
 */

#define EDID_OFFSET_HEADER_FIRST_00	0x00
#define EDID_OFFSET_HEADER_FIRST_FF	0x01
#define EDID_OFFSET_HEADER_LAST_FF	0x06
#define EDID_OFFSET_HEADER_LAST_00	0x07

#ifndef MANUAL_EDID_FETCH
static
#endif
bool si_mhl_tx_check_edid_header(struct edid_3d_data_t *mhl_edid_3d_data,
	void *pdata)
{
	struct EDID_block0_t *p_EDID = (struct EDID_block0_t *)pdata;
	uint8_t i = 0;

	if (0 != p_EDID->header_data[EDID_OFFSET_HEADER_FIRST_00]) {
		DUMP_EDID_BLOCK(1, p_EDID, sizeof(*p_EDID));
		    MHL_TX_DBG_ERR("%sEDID 0 first check failed%s\n",
				   ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		return false;
	}

	for (i = EDID_OFFSET_HEADER_FIRST_FF; i <= EDID_OFFSET_HEADER_LAST_FF;
	     i++) {
		if (0xFF != p_EDID->header_data[i]) {
			DUMP_EDID_BLOCK(1, p_EDID, sizeof(*p_EDID));
			    MHL_TX_DBG_ERR("%sEDID -1 check failed%s\n",
					   ANSI_ESC_RED_TEXT,
					   ANSI_ESC_RESET_TEXT);
			return false;
		}
	}

	if (0x00 != p_EDID->header_data[EDID_OFFSET_HEADER_LAST_00]) {
		DUMP_EDID_BLOCK(1, p_EDID, sizeof(*p_EDID));
		    MHL_TX_DBG_ERR("EDID 0 last check failed\n");
		return false;
	}

	return true;
}

void SiiMhlTxMakeItDVI(struct edid_3d_data_t *mhl_edid_3d_data,
	struct EDID_block0_t *p_EDID_block_0)
{
	/* Make it DVI */
	mhl_edid_3d_data->parse_data.HDMI_sink = false;
#ifdef MHL3_DVI_SUPPORT_FORCE_HDMI
	if (si_mhl_tx_drv_connection_is_mhl3(mhl_edid_3d_data->dev_context)) {
		/*
		  MHL3 connections always require infoframes
			So present an EDID that indicates HDMI
			so that our upstream sink will send infoframes
		*/

		uint8_t hdmi_proxy_edid_block1[EDID_BLOCK_SIZE] = {
			0x02, 0x03, 0x19, 0x70,
			(DBTC_VIDEO_DATA_BLOCK << 5) | 0x0B,
				0x01, /* */
				0x03, /* 1280x720p60 */
				0x04,
				0x05, /* 1920x1080i60 */
				0x90,
				0x20,
				0x11,
				0x12,
				0x13,
				0x14,
				0x1f,
			(DBTC_VENDOR_SPECIFIC_DATA_BLOCK << 5) | 0x08,
				0x03, 0x0C, 0x00, /* HDMI LLC */
				0x10, 0x00, 0x00, 0x2D, 0x00,

			      0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,

			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,

			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,

			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
		};
		struct EDID_block0_t *p_EDID_block_0 =
			(struct EDID_block0_t *)
			&mhl_edid_3d_data->EDID_block_data[0];
		MHL_TX_DBG_ERR("HDMI EDID for MHL3 dongle\n")
		p_EDID_block_0->extension_flag = 1;
		memcpy(&mhl_edid_3d_data->
			EDID_block_data[EDID_BLOCK_SIZE],
			hdmi_proxy_edid_block1, EDID_BLOCK_SIZE);
		mhl_edid_3d_data->EDID_block_data[2*EDID_BLOCK_SIZE-1] =
			calculate_generic_checksum(&mhl_edid_3d_data->
				EDID_block_data[EDID_BLOCK_SIZE],
				0, EDID_BLOCK_SIZE);
	} else
#endif
	{
		uint8_t *p_EDID_block_data = (uint8_t *) p_EDID_block_0;
		uint8_t counter;

		p_EDID_block_0->extension_flag = 0;

		/* blank out the second block of the upstream EDID */
		MHL_TX_DBG_INFO("DVI EDID ...Setting second block to 0xFF %d\n",
				(uint16_t) EDID_REV_ADDR_ERROR);
		p_EDID_block_data += EDID_BLOCK_SIZE;
		for (counter = 0; counter < EDID_BLOCK_SIZE; counter++)
			p_EDID_block_data[counter] = 0xFF;
		MHL_TX_DBG_ERR("EDID: second block now all 0xFF\n");
	}

}

static void SiiMhlTx3dReq(struct edid_3d_data_t *mhl_edid_3d_data)
{
	MHL_TX_EDID_INFO("Mhl2Tx: outputMode: %s\n",
			 mhl_edid_3d_data->parse_data.
			 HDMI_sink ? "HDMI" : "DVI");
	if (mhl_edid_3d_data->parse_data.HDMI_sink) {
		uint8_t mhl_peer_version;
		mhl_peer_version =
			si_get_peer_mhl_version(mhl_edid_3d_data->dev_context);
		if (mhl_peer_version >= 0x20) {
			struct drv_hw_context *hw_context =
			    mhl_edid_3d_data->drv_context;
			MHL_TX_EDID_INFO("MHL 2.x sink detected\n");

			mhl_edid_3d_data->parse_data.flags.
			    parse_3d_in_progress = 1;
			hw_context->callbacks.display_timing_enum_begin(
				hw_context->callbacks.context);
			/* tell sink to begin sending 3D, etc. write bursts */
			si_mhl_tx_send_3d_req_or_feat_req(mhl_edid_3d_data->
							  dev_context);

			mhl_edid_3d_data->parse_data.video_data_block_index = 0;
			mhl_edid_3d_data->parse_data.burst_entry_count_3d_dtd =
			    0;
			mhl_edid_3d_data->parse_data.vesa_dtd_index = 0;
			mhl_edid_3d_data->parse_data.burst_entry_count_3d_vic =
			    0;
			mhl_edid_3d_data->parse_data.vic_2d_index = 0;
			mhl_edid_3d_data->parse_data.vic_3d_index = 0;
			mhl_edid_3d_data->parse_data.cea_861_dtd_index = 0;
			mhl_edid_3d_data->parse_data.num_vesa_timing_dtds = 0;
			mhl_edid_3d_data->parse_data.num_cea_861_timing_dtds =
			    0;

			mhl_edid_3d_data->hev_dtd_info.num_items = 0;
			mhl_edid_3d_data->hev_vic_info.num_items = 0;
			mhl_edid_3d_data->_3d_dtd_info.num_items = 0;
			mhl_edid_3d_data->_3d_vic_info.num_items = 0;

			mhl_edid_3d_data->hev_dtd_info.index = 0;
			mhl_edid_3d_data->hev_vic_info.index = 0;
			mhl_edid_3d_data->_3d_dtd_info.index = 0;
			mhl_edid_3d_data->_3d_vic_info.index = 0;

			SET_3D_FLAG(mhl_edid_3d_data, FLAGS_SENT_3D_REQ);
			CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_DONE);
			CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_BURST_3D_VIC_DONE);
			CLR_3D_FLAG(mhl_edid_3d_data,
				FLAGS_BURST_3D_DTD_VESA_DONE);
		} else {
			MHL_TX_EDID_INFO("MHL 1.x sink detected\n");
			si_mhl_tx_prune_edid(mhl_edid_3d_data);
		}
	} else {
		struct EDID_block0_t *p_EDID_block_0 = (struct EDID_block0_t *)
			&mhl_edid_3d_data->EDID_block_data[0];
		uint8_t dtd_limit;
		/* prune the DTDs in block 0 for DVI sinks */
		dtd_limit =
		    sizeof(p_EDID_block_0->detailed_timing_descriptors) /
		    sizeof(p_EDID_block_0->detailed_timing_descriptors[0]);
		tx_prune_dtd_list(mhl_edid_3d_data,
			(union _18_byte_descriptor_u *) &p_EDID_block_0->
			detailed_timing_descriptors[0], dtd_limit);
		p_EDID_block_0->checksum = 0;
		p_EDID_block_0->checksum =
			calculate_generic_checksum((uint8_t *)p_EDID_block_0, 0,
				sizeof(*p_EDID_block_0));
#ifndef EDID_PASSTHROUGH
		if (0 == si_mhl_tx_drv_set_upstream_edid(
			mhl_edid_3d_data->drv_context,
			mhl_edid_3d_data->EDID_block_data,
			2 * EDID_BLOCK_SIZE))
#endif
			{
			SET_3D_FLAG(mhl_edid_3d_data, FLAGS_EDID_READ_DONE);
		}
	}
}

static uint8_t parse_861_short_descriptors(
	struct edid_3d_data_t *mhl_edid_3d_data, uint8_t *p_EDID_block_data)
{
	uint8_t i;
	struct CEA_extension_t *p_CEA_extension;
	struct CEA_extension_version_3_t *p_CEA_extension_version_3;
	union {
		uint8_t *puc_data_block;
		struct CEA_data_block_collection_t *p_CEA_data_block;
	} p_data_u;
	uint8_t *puc_long_descriptors;
	struct extended_tag_code_t ext_tag_code;
	struct video_capability_data_block_t *p_video_capability;
	struct video_capability_data_payload_t *p_payload;
	struct colorimetry_data_block_t *p_colorimetry;
	struct colorimetry_data_payload_t *p_cpayload;
	struct vsdb_t *p_vsdb;
	uint8_t *puc_next_db;

	p_CEA_extension = (struct CEA_extension_t *) p_EDID_block_data;
	p_CEA_extension_version_3 = &p_CEA_extension->version_u.version3;

	if (EDID_EXTENSION_TAG != p_CEA_extension->tag) {
		enum NumExtensions_e ret_val;
		MHL_TX_DBG_ERR("%sEDID -> Non-CEA Extension read fifo again\n\t"
			"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
			"0x%02x%s\n", ANSI_ESC_RED_TEXT, p_EDID_block_data[0],
			p_EDID_block_data[1], p_EDID_block_data[2],
			p_EDID_block_data[3], p_EDID_block_data[4],
			p_EDID_block_data[5], p_EDID_block_data[6],
			p_EDID_block_data[7], ANSI_ESC_RESET_TEXT);
		ret_val = si_mhl_tx_drv_get_edid_fifo_next_block(
			    mhl_edid_3d_data->drv_context, p_EDID_block_data);
		if (ne_SUCCESS != ret_val)
			return ret_val;
		else if (EDID_EXTENSION_TAG != p_CEA_extension->tag)
			return EDID_EXT_TAG_ERROR;

	}
	if (EDID_REV_THREE != p_CEA_extension->revision) {
		MHL_TX_DBG_ERR("EDID -> Non-HDMI EIA-861 Revision ID. Expected "
			"%02X. Got %02X\n", (int)EDID_REV_THREE,
			(int)p_CEA_extension->revision);
		return EDID_REV_ADDR_ERROR;
	}

	/* block offset where long descriptors start */
	puc_long_descriptors = ((uint8_t *) p_CEA_extension) +
	    p_CEA_extension->byte_offset_to_18_byte_descriptors;

	/* byte #3 of CEA extension version 3 */
	mhl_edid_3d_data->parse_data.underscan =
	    p_CEA_extension_version_3->misc_support.
	    underscan_IT_formats_by_default ? 1 : 0;
	mhl_edid_3d_data->parse_data.basic_audio =
	    p_CEA_extension_version_3->misc_support.basic_audio_support ? 1 : 0;
	mhl_edid_3d_data->parse_data.YCbCr_4_4_4 =
	    p_CEA_extension_version_3->misc_support.YCrCb444_support;
	mhl_edid_3d_data->parse_data.YCbCr_4_2_2 =
	    p_CEA_extension_version_3->misc_support.YCrCb422_support;
	MHL_TX_EDID_INFO("misc support index-> %02x\n",
	    *((uint8_t *) &p_CEA_extension_version_3->misc_support));

	p_data_u.puc_data_block =
	    &p_CEA_extension->version_u.version3.Offset4_u.
	    data_block_collection[0];

	while (p_data_u.puc_data_block < puc_long_descriptors) {
		enum data_block_tag_code_e tag_code;
		uint8_t data_block_length;

		tag_code = p_data_u.p_CEA_data_block->header.fields.tag_code;
		data_block_length = p_data_u.p_CEA_data_block->header.fields.
			length_following_header;
		MHL_TX_EDID_INFO("tag_code:%d data_block_length:%d\n",
			tag_code, data_block_length);
		if ((p_data_u.puc_data_block + data_block_length) >
		    puc_long_descriptors) {
			MHL_TX_DBG_ERR(
			    "EDID -> V Descriptor Overflow\n");
			return EDID_V_DESCR_OVERFLOW;
		}

		/* num of short video descriptors in this data block */
		i = 0;
		switch (tag_code) {
		case DBTC_VIDEO_DATA_BLOCK:
			MHL_TX_EDID_INFO("DBTC_VIDEO_DATA_BLOCK:\n");
			if (mhl_edid_3d_data->parse_data.
			    num_video_data_blocks <
			    NUM_VIDEO_DATA_BLOCKS_LIMIT) {
				mhl_edid_3d_data->parse_data.
				    p_video_data_blocks_2d[
				    mhl_edid_3d_data->parse_data.
				    num_video_data_blocks] =
				    (struct video_data_block_t *)
				    p_data_u.puc_data_block;

				/* each SVD is 1 byte long */
				while (i < data_block_length) {
					uint8_t VIC;
					VIC =
					    mhl_edid_3d_data->
					    parse_data.p_video_data_blocks_2d
					    [mhl_edid_3d_data->parse_data.
					     num_video_data_blocks]->
					    short_descriptors[i].VIC;
					MHL_TX_EDID_INFO(
					    "short desc[%d]: VIC: %d\n",
					     i, VIC);
					if (!IsQualifiedMhlVIC
					    (mhl_edid_3d_data, VIC, 0,
					     NULL)) {
						mhl_edid_3d_data->parse_data.
						    p_video_data_blocks_2d
						    [mhl_edid_3d_data->
						     parse_data.
						     num_video_data_blocks]->
						    short_descriptors
						    [i].VIC = 0;
					}
					i++;
				}
				MHL_TX_EDID_INFO(
				    "EDID -> %d descriptors in "
				    "Short Descriptor Video Block\n",
				     i);
				mhl_edid_3d_data->parse_data.
				    num_video_data_blocks++;
			}
			break;

		case DBTC_AUDIO_DATA_BLOCK:
			{
				struct audio_data_block_t *p_audio_data_block =
				    (struct audio_data_block_t *) p_data_u.
				    puc_data_block;
				uint8_t a_desc_index = 0;

				while (i <
				       data_block_length /
				       sizeof(p_audio_data_block->
					      short_audio_descriptors[0])) {
					mhl_edid_3d_data->parse_data.
					    audio_descriptors[a_desc_index]
					    = p_audio_data_block->
					    short_audio_descriptors[i];
					a_desc_index++;
					i++;
				}
				MHL_TX_EDID_INFO(
				    "EDID -> Short Descriptor Audio Block\n");
			}
			break;

		case DBTC_SPEAKER_ALLOCATION_DATA_BLOCK:
			{
				struct speaker_allocation_data_block_t
				    *p_speaker =
				    (struct speaker_allocation_data_block_t *)
				    p_data_u.puc_data_block;

				*((struct speaker_allocation_flags_t *)
				  &mhl_edid_3d_data->parse_data.
				  speaker_alloc[i++]) =
					p_speaker->payload.speaker_alloc_flags;
				MHL_TX_EDID_INFO(
				    "EDID -> Short Desc Speaker Alloc Block\n");
			}
			break;

		case DBTC_USE_EXTENDED_TAG:
			ext_tag_code = p_data_u.p_CEA_data_block->
				payload_u.extended_tag;
			switch (ext_tag_code.etc) {
			case ETC_VIDEO_CAPABILITY_DATA_BLOCK:
				p_video_capability =
				    (struct video_capability_data_block_t *)
				    p_data_u.puc_data_block;
				p_payload = &p_video_capability->payload;
				mhl_edid_3d_data->parse_data.
				    video_capability_flags =
					*((uint8_t *)p_payload);
				mhl_edid_3d_data->parse_data.
				    p_video_capability_data_block =
					p_video_capability;
				MHL_TX_EDID_INFO(
				    "EDID -> Short Desc Video Cap Block\n");
				break;

			case ETC_COLORIMETRY_DATA_BLOCK:
				p_colorimetry =
				    (struct colorimetry_data_block_t *)p_data_u.
				    puc_data_block;
				p_cpayload = &p_colorimetry->payload;
				mhl_edid_3d_data->parse_data.
				    colorimetry_support_flags =
					p_cpayload->ci_data.xvYCC;
				mhl_edid_3d_data->parse_data.meta_data_profile =
				    p_cpayload->cm_meta_data.meta_data;

				MHL_TX_EDID_INFO(
				    "EDID -> Short Desc Colorimetry Block\n");
				break;
			}

			break;

		case DBTC_VENDOR_SPECIFIC_DATA_BLOCK:
			p_vsdb = (struct vsdb_t *) p_data_u.puc_data_block;
			puc_next_db = ((uint8_t *) &p_vsdb->header) +
				sizeof(p_vsdb->header) +
				data_block_length;

			if ((p_vsdb->IEEE_OUI[0] == 0x03)
			    && (p_vsdb->IEEE_OUI[1] == 0x0C)
			    && (p_vsdb->IEEE_OUI[2] == 0x00)
			    ) {
				struct HDMI_LLC_vsdb_payload_t
				    *p_HDMI_vs_payload =
					&p_vsdb->payload_u.HDMI_LLC;

				mhl_edid_3d_data->parse_data.p_HDMI_vsdb =
					p_vsdb;
				SII_ASSERT(5 <= data_block_length,
					   ("unexpected data_block_length\n"));
				mhl_edid_3d_data->parse_data.HDMI_sink = true;

				/* CEC Physical address */
				*((struct HDMI_LLC_BA_t *)
					&mhl_edid_3d_data->parse_data.CEC_A_B) =
							p_HDMI_vs_payload->B_A;
				*((struct HDMI_LLC_DC_t *)
				  &mhl_edid_3d_data->parse_data.CEC_C_D) =
						  p_HDMI_vs_payload->D_C;
				/* Offset of 3D_Present bit in VSDB */
				if (p_HDMI_vs_payload->byte8.
						latency_fields_present) {
					if (p_HDMI_vs_payload->byte8.
						I_latency_fields_present) {
						mhl_edid_3d_data->
						    parse_data.
						    p_byte_13_through_byte_15
						    =
						    &p_HDMI_vs_payload->
						    vsdb_fields_b9_thru_b15.
						    vsdb_all_fields_b9_thru_b15.
						    byte_13_through_byte_15;
					} else {
						mhl_edid_3d_data->parse_data.
						    p_byte_13_through_byte_15 =
						    &p_HDMI_vs_payload->
						    vsdb_fields_b9_thru_b15.
						    vsdb_b9_to_b15_no_i_latency.
						    byte_13_through_byte_15;
					}
				} else {
					if (p_HDMI_vs_payload->byte8.
						I_latency_fields_present) {
						mhl_edid_3d_data->parse_data.
						    p_byte_13_through_byte_15 =
						    &p_HDMI_vs_payload->
						    vsdb_fields_b9_thru_b15.
						    vsdb_b9_to_b15_no_p_latency.
						    byte_13_through_byte_15;
					} else {
						mhl_edid_3d_data->parse_data.
						    p_byte_13_through_byte_15 =
						    &p_HDMI_vs_payload->
						    vsdb_fields_b9_thru_b15.
						    vsdb_b9_to_b15_no_latency.
						    byte_13_through_byte_15;
					}
				}
				if (((u8 *)&mhl_edid_3d_data->parse_data.
				     p_byte_13_through_byte_15->byte13) >=
				     puc_next_db) {
					mhl_edid_3d_data->parse_data.
					    _3D_supported = false;
				} else if (mhl_edid_3d_data->parse_data.
					    p_byte_13_through_byte_15->byte13.
					    _3D_present) {
					mhl_edid_3d_data->parse_data.
					    _3D_supported = true;
				} else {
					mhl_edid_3d_data->parse_data.
					    _3D_supported = false;
				}

				MHL_TX_EDID_INFO(
				    "EDID indicates %s3D support\n",
				    mhl_edid_3d_data->parse_data.
				    _3D_supported ? "" : "NO ");
			}

			MHL_TX_EDID_INFO(
			    "EDID -> Short Descriptor Vendor Block\n\n");
			break;
		case DBTC_TERMINATOR:
			MHL_TX_EDID_INFO("found terminator tag code\n");
			return EDID_SHORT_DESCRIPTORS_OK;
			break;

		default:
			MHL_TX_DBG_ERR("EDID -> Unknown Tag Code:0x%02x\n",
			    (uint16_t) tag_code);
			return EDID_UNKNOWN_TAG_CODE;

		}
		p_data_u.puc_data_block +=
		    sizeof(p_data_u.p_CEA_data_block->header) +
		    data_block_length;
	}

	return EDID_SHORT_DESCRIPTORS_OK;
}

static uint8_t parse_861_block(struct edid_3d_data_t *mhl_edid_3d_data,
			       uint8_t *p_EDID_block_data)
{
	uint8_t err_code;
	struct CEA_extension_t *p_CEA_extension =
		(struct CEA_extension_t *) p_EDID_block_data;

	mhl_edid_3d_data->parse_data.p_HDMI_vsdb = NULL;

	MHL_TX_EDID_INFO("tag:place holder EDID block:%pK\n",
		p_EDID_block_data);
	if (EDID_EXTENSION_BLOCK_MAP == p_CEA_extension->tag) {
		struct block_map_t *p_block_map;
		int i;

		p_block_map = (struct block_map_t *) p_EDID_block_data;

		MHL_TX_EDID_INFO("Edid: Block Map\n");
		/* loop limit is adjusted by one to account for block map */
		for (i = 0;
		     i < mhl_edid_3d_data->parse_data.num_EDID_extensions - 1;
		     ++i) {
			if (EDID_EXTENSION_TAG != p_block_map->block_tags[i]) {
				MHL_TX_EDID_INFO(
					"Edid: Adjusting number of extensions "
					"according to Block Map\n");
				mhl_edid_3d_data->parse_data.
					num_EDID_extensions = i;
				break;
			}
		}

		return EDID_OK;

	} else {
		err_code =
		    parse_861_short_descriptors(mhl_edid_3d_data,
						p_EDID_block_data);
		if (err_code != EDID_SHORT_DESCRIPTORS_OK) {
			MHL_TX_DBG_ERR("EDID: Non-HDMI extension Errcode:%d\n",
				       (uint16_t) err_code);
			return err_code;
		}

		/* adjust */
		err_code =
		    si_mhl_tx_parse_861_long_descriptors(mhl_edid_3d_data,
							 p_EDID_block_data);
		if (err_code != EDID_LONG_DESCRIPTORS_OK) {
			MHL_TX_DBG_ERR("EDID: Errcode:%d\n",
				       (uint16_t) err_code);
			return err_code;
		}
	}
	return EDID_OK;
}

void si_mhl_tx_handle_atomic_hw_edid_read_complete(
	struct edid_3d_data_t *mhl_edid_3d_data)
{
	struct EDID_block0_t *p_EDID_block_0 =
	    (struct EDID_block0_t *)&mhl_edid_3d_data->EDID_block_data[0];
	uint8_t counter;
	MHL_TX_EDID_INFO("tag: Entire EDID Read complete\n");
#ifdef EDID_PASSTHROUGH
	si_mhl_tx_drv_set_upstream_edid(mhl_edid_3d_data->drv_context,
					mhl_edid_3d_data->EDID_block_data,
					2 * EDID_BLOCK_SIZE);
#endif
	/* assume DVI until we encounter an HDMI VSDB */
	mhl_edid_3d_data->parse_data.HDMI_sink = false;

	/* Parse EDID Block #0 Desctiptors */
	si_mhl_tx_parse_block_zero_timing_descriptors(mhl_edid_3d_data,
						      p_EDID_block_0);

	MHL_TX_EDID_INFO("EDID -> Number of 861 Extensions = %d\n",
			 (uint16_t) p_EDID_block_0->extension_flag);

	mhl_edid_3d_data->parse_data.num_EDID_extensions =
	    p_EDID_block_0->extension_flag;
	if (0 == p_EDID_block_0->extension_flag) {
		/* No extensions to worry about */
		DUMP_EDID_BLOCK(0, (uint8_t *) p_EDID_block_0, EDID_BLOCK_SIZE);
		    MHL_TX_DBG_ERR
		    ("EDID -> no extensions, assuming DVI. tag offset:0x%x\n",
		     SII_OFFSETOF(struct EDID_block0_t, extension_flag));
		SiiMhlTxMakeItDVI(mhl_edid_3d_data, p_EDID_block_0);
	} else {
		uint8_t Result = EDID_OK;
		MHL_TX_EDID_INFO(" tag:place holder\n");
		/* number of extensions is one less than number of blocks */
		for (counter = 1;
		     counter <=
		     mhl_edid_3d_data->parse_data.num_EDID_extensions;
		     ++counter) {
			MHL_TX_EDID_INFO
			    (" counter:%d tag:place holder EDID block:%pK\n",
			     counter,
			     &mhl_edid_3d_data->
			     EDID_block_data[EDID_BLOCK_SIZE * counter]);
			Result =
			    parse_861_block(mhl_edid_3d_data,
					    &mhl_edid_3d_data->
					    EDID_block_data[EDID_BLOCK_SIZE *
							    counter]);
			if (EDID_OK != Result) {
				MHL_TX_DBG_ERR
				    ("EDID -> Ext[%d] is not HDMI: Result:%d\n",
				     counter, (uint16_t) Result);
				SiiMhlTxMakeItDVI(mhl_edid_3d_data,
						  p_EDID_block_0);
				Result = EDID_OK;
			}
		}
	}
	/*
	 * Since our working copy of the block zero EDID gets modified,
	 *              we must re-compute its checksum
	 */
	p_EDID_block_0->checksum = 0;
	p_EDID_block_0->checksum =
	    calculate_generic_checksum((uint8_t *) p_EDID_block_0, 0,
				       sizeof(*p_EDID_block_0));

	SiiMhlTx3dReq(mhl_edid_3d_data);
}

static void handle_emsc_edid_complete(
	struct edid_3d_data_t *mhl_edid_3d_data)
{
	uint8_t *normal_edid = &mhl_edid_3d_data->EDID_block_data[0];
	if (NULL == mhl_edid_3d_data->p_edid_emsc) {
		MHL_TX_DBG_ERR("%soops!%s\n",
			ANSI_ESC_RED_TEXT,
			ANSI_ESC_RESET_TEXT)
		MHL_TX_DBG_ERR("normative EDID\n")
		si_edid_reset(mhl_edid_3d_data);
		si_mhl_tx_request_first_edid_block(mhl_edid_3d_data->
					dev_context->edid_parser_context);
	} else {
		size_t edid_size = EDID_BLOCK_SIZE *
				 mhl_edid_3d_data->num_edid_emsc_blocks;
		memcpy(normal_edid,
			mhl_edid_3d_data->p_edid_emsc, edid_size);

		DUMP_EDID_BLOCK(0, normal_edid, edid_size);
		si_mhl_tx_handle_atomic_hw_edid_read_complete(mhl_edid_3d_data);
	}
}
/*
		EXPORTED FUNCTIONS
*/

void si_mhl_tx_initiate_edid_sequence(struct edid_3d_data_t *mhl_edid_3d_data)
{
	MHL_TX_EDID_INFO("tag:\n");
	if (si_mhl_tx_drv_cbus_ready_for_edid(mhl_edid_3d_data->dev_context)) {
		mhl_edid_3d_data->parse_data.num_video_data_blocks = 0;

		if (mhl_edid_3d_data->dev_context->sii_adopter_id) {
			MHL_TX_DBG_ERR("vendor specific EDID\n")
			handle_emsc_edid_complete(mhl_edid_3d_data);
		} else {
			MHL_TX_DBG_ERR("normative EDID\n")
			/*
			   Initiate the EDID reading sequence see
			   SiiMhlTxMscCommandDone for additional processing.
			 */

			si_edid_reset(mhl_edid_3d_data);
			si_mhl_tx_request_first_edid_block(mhl_edid_3d_data->
				dev_context);
		}
	}
}

#define EDID_BLOCK_0_HEADER_SIZE 8

static int do_block_0(struct edid_3d_data_t *mhl_edid_3d_data)
{
	uint8_t *pb_data = &mhl_edid_3d_data->EDID_block_data[0];
	struct EDID_block0_t *p_EDID =
		(struct EDID_block0_t *)&mhl_edid_3d_data->EDID_block_data;

	if (ne_SUCCESS == si_mhl_tx_drv_get_edid_fifo_partial_block(
			mhl_edid_3d_data->drv_context, 0,
			EDID_BLOCK_0_HEADER_SIZE, pb_data)) {
		if (!si_mhl_tx_check_edid_header(mhl_edid_3d_data, p_EDID)) {
			MHL_TX_DBG_ERR
			    ("%sEDID -> Incorrect Header pb_data:%s"
			     "\t0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
			     "0x%02x 0x%02x 0x%02x\n",
			     ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT,
			     pb_data[0], pb_data[1], pb_data[2],
			     pb_data[3], pb_data[4], pb_data[5],
			     pb_data[6], pb_data[7]
			    );
			DUMP_EDID_BLOCK(1, pb_data,
				EDID_BLOCK_0_HEADER_SIZE);
			    if (si_mhl_tx_check_edid_header(
				mhl_edid_3d_data, &mhl_edid_3d_data->
				EDID_block_data[1])) {
				MHL_TX_DBG_ERR(
				    "%sEDID header misaligned:%x%s\n",
				    ANSI_ESC_RED_TEXT, pb_data,
				    ANSI_ESC_RESET_TEXT);
				return ne_BAD_HEADER_OFFSET_BY_1;
			} else {
				int ret_val;
				MHL_TX_DBG_ERR(
				    "%sre-reading buffer:%x%s\n",
				    ANSI_ESC_RED_TEXT, pb_data,
				    ANSI_ESC_RESET_TEXT);
				si_mhl_tx_drv_skip_to_next_edid_block(
				    mhl_edid_3d_data->drv_context);
				ret_val =
				    si_mhl_tx_drv_get_edid_fifo_next_block(
				    mhl_edid_3d_data->drv_context,
				     pb_data);
				if (ne_SUCCESS != ret_val) {
					MHL_TX_DBG_ERR("loss of HPD\n",
						       pb_data);
					return ret_val;
				} else
				    if (!si_mhl_tx_check_edid_header(
					mhl_edid_3d_data, &mhl_edid_3d_data->
					EDID_block_data[0])) {
					MHL_TX_DBG_ERR(
					    "%sBAD Header\n\t%02x %02x %02x "
					    "%02x %02x %02x %02x %02x%s\n",
					    ANSI_ESC_RED_TEXT,
					    pb_data[0], pb_data[1],
					    pb_data[2], pb_data[3],
					    pb_data[4], pb_data[5],
					    pb_data[6], pb_data[7],
					    ANSI_ESC_RESET_TEXT);
					return ne_BAD_HEADER;
				}
			}
		} else if (ne_SUCCESS !=
			   si_mhl_tx_drv_get_edid_fifo_partial_block
			   (mhl_edid_3d_data->drv_context,
			    EDID_BLOCK_0_HEADER_SIZE,
			    EDID_BLOCK_SIZE - EDID_BLOCK_0_HEADER_SIZE,
			    &pb_data[EDID_BLOCK_0_HEADER_SIZE])) {
			return ne_NO_HPD;
		}
	}

	return ne_SUCCESS;
}

int si_mhl_tx_get_num_cea_861_extensions(void *context, uint8_t block_number)
{
	struct edid_3d_data_t *mhl_edid_3d_data =
		(struct edid_3d_data_t *)context;

	struct EDID_block0_t *p_EDID_block_0 = (struct EDID_block0_t *)
	    &mhl_edid_3d_data->EDID_block_data;
	uint8_t limit_blocks = sizeof(mhl_edid_3d_data->EDID_block_data) /
	    EDID_BLOCK_SIZE;

	uint8_t *pb_data;
	if (block_number > NUM_VIDEO_DATA_BLOCKS_LIMIT)
		return ne_BAD_DATA;
	pb_data =
	    &mhl_edid_3d_data->EDID_block_data[EDID_BLOCK_SIZE * block_number];

	MHL_TX_EDID_INFO("block number:%d pb_data:%x\n", block_number, pb_data);

	if (0 == block_number) {
		int ret_val = do_block_0(mhl_edid_3d_data);
		if (ret_val != ne_SUCCESS)
			return ret_val;
	} else if (ne_SUCCESS != si_mhl_tx_drv_get_edid_fifo_next_block(
			   mhl_edid_3d_data->drv_context, pb_data)) {
		return ne_NO_HPD;
	}
	if (!si_mhl_tx_do_edid_checksum(pb_data)) {
		MHL_TX_DBG_ERR("%sEDID -> Checksum Error pb_data:%x%s\n",
			ANSI_ESC_RED_TEXT, pb_data, ANSI_ESC_RESET_TEXT);
		DUMP_EDID_BLOCK(1, pb_data, EDID_BLOCK_SIZE);
		return ne_BAD_CHECKSUM;
	}

	if (p_EDID_block_0->extension_flag < limit_blocks) {
		return p_EDID_block_0->extension_flag;
	} else {
		MHL_TX_DBG_ERR("%snot enough room for %d extensions%s\n",
			ANSI_ESC_RED_TEXT, p_EDID_block_0->extension_flag,
			ANSI_ESC_RESET_TEXT);
		return (int)limit_blocks - 1;
	}
}

int si_edid_sink_is_hdmi(void *context)
{
	struct edid_3d_data_t *mhl_edid_3d_data =
		(struct edid_3d_data_t *) context;
	return mhl_edid_3d_data->parse_data.HDMI_sink;
}

int si_edid_quantization_range_selectable(void *context)
{
	struct edid_3d_data_t *mhl_edid_3d_data =
		(struct edid_3d_data_t *) context;
	return mhl_edid_3d_data->parse_data.video_capability_flags & 0x80;
}

int si_edid_sink_supports_YCbCr422(void *context)
{
	struct edid_3d_data_t *mhl_edid_3d_data =
		(struct edid_3d_data_t *) context;
	MHL_TX_EDID_INFO("Mhl2Tx: YCbCr422 support:%s\n",
			 mhl_edid_3d_data->parse_data.
			 YCbCr_4_2_2 ? "Yup" : "Nope");
	return mhl_edid_3d_data->parse_data.YCbCr_4_2_2;
}

int si_edid_sink_supports_YCbCr444(void *context)
{
	struct edid_3d_data_t *mhl_edid_3d_data =
		(struct edid_3d_data_t *) context;
	MHL_TX_EDID_INFO("Mhl2Tx: YCbCr444 support:%s\n",
			 mhl_edid_3d_data->parse_data.
			 YCbCr_4_4_4 ? "Yup" : "Nope");
	return mhl_edid_3d_data->parse_data.YCbCr_4_4_4;
}

int si_edid_find_pixel_clock_from_HDMI_VIC(void *context, uint8_t vic)
{
	if (vic < ARRAY_SIZE(hdmi_vic_info)) {
		return hdmi_vic_info[vic].pixel_clock_0;
	} else {
		MHL_TX_DBG_ERR("vic:%d is out of range\n", vic);
		return 0;
	}
}

uint8_t si_edid_map_hdmi_vic_to_mhl3_vic(void *context, uint8_t vic)
{
	if (vic < ARRAY_SIZE(hdmi_vic_info)) {
		return hdmi_vic_info[vic].corresponding_MHL3_VIC;
	} else {
		MHL_TX_DBG_ERR("vic:%d is out of range\n", vic);
		return 0;
	}
}

int si_edid_find_pixel_clock_from_AVI_VIC(void *context, uint8_t vic)
{
	if (vic < ARRAY_SIZE(VIC_info)) {
		return calculate_pixel_clock(context,
			(uint16_t) VIC_info[vic].columns,
			(uint16_t) VIC_info[vic].rows,
			(uint32_t) VIC_info[vic].field_rate_in_milliHz, vic);
	} else {
		MHL_TX_DBG_ERR("vic:%d is out of range\n", vic);
		return 0;
	}
}

uint32_t si_edid_find_pixel_clock_from_HEV_DTD(
	struct edid_3d_data_t *mhl_edid_3d_data, struct MHL_high_low_t hev_fmt)
{
	/* SEQ numbers start at one */
	uint16_t index = ENDIAN_CONVERT_16(hev_fmt) - 16384 - 1;
	if (NULL == mhl_edid_3d_data->hev_dtd_list) {
		MHL_TX_DBG_ERR("No HEV DTDs available\n", index)
	} else if (index >= mhl_edid_3d_data->hev_dtd_info.num_items) {
		MHL_TX_DBG_ERR("hev_fmt out of range: %x\n", index)
	} else {
		return (uint32_t) ENDIAN_CONVERT_16(
			mhl_edid_3d_data->hev_dtd_list[index].a.
			pixel_clock_in_MHz) * 1000000;
	}
	return 0;
}

int si_edid_read_done(void *context)
{
	struct edid_3d_data_t *mhl_edid_3d_data =
		(struct edid_3d_data_t *) context;
	return TEST_3D_FLAG(mhl_edid_3d_data, FLAGS_EDID_READ_DONE);
}

void si_edid_reset(struct edid_3d_data_t *mhl_edid_3d_data)
{
	int i;
	uint8_t *pData = (uint8_t *)&mhl_edid_3d_data->parse_data;

	MHL_TX_DBG_INFO("\n")
	/* clear out EDID parse results */
	for (i = 0; i < sizeof(mhl_edid_3d_data->parse_data); ++i)
		pData[i] = 0;

	CLR_3D_FLAG(mhl_edid_3d_data, FLAGS_EDID_READ_DONE);
	mhl_edid_3d_data->num_emsc_edid_extensions = 0;
	mhl_edid_3d_data->num_edid_emsc_blocks = 0;
	mhl_edid_3d_data->cur_edid_emsc_block = 0;
}

struct mhl_dev_context *si_edid_create_context(
	struct mhl_dev_context *dev_context,
	struct drv_hw_context *drv_context)
{
	struct edid_3d_data_t *temp;
	temp = kmalloc(sizeof(struct edid_3d_data_t), GFP_KERNEL);
	if (temp) {
		memset((void *)temp, 0, sizeof(*temp));
		temp->dev_context = dev_context;
		temp->drv_context = drv_context;
		validate_timings_table();
	}
	return (void *)temp;
}

void *si_edid_get_processed_edid(struct edid_3d_data_t *mhl_edid_3d_data)
{
	return mhl_edid_3d_data->EDID_block_data;
}

void si_edid_destroy_context(struct edid_3d_data_t *mhl_edid_3d_data)
{
	if (mhl_edid_3d_data) {
		kfree(mhl_edid_3d_data->hev_dtd_list);
		kfree(mhl_edid_3d_data->hev_vic_list);
		kfree(mhl_edid_3d_data->_3d_dtd_list);
		kfree(mhl_edid_3d_data->_3d_vic_list);
		kfree(mhl_edid_3d_data->p_edid_emsc);

		memset(mhl_edid_3d_data, 0, sizeof(*mhl_edid_3d_data));
		kfree(mhl_edid_3d_data);
	}
}

#ifdef ENABLE_EDID_DEBUG_PRINT
void dump_EDID_block_impl(const char *pszFunction, int iLineNum,
			  uint8_t override, uint8_t *pData, uint16_t length)
{
	uint16_t i;
	char hex_digits[] = "0123456789ABCDEF";
	printk(KERN_DEFAULT "%s:%d EDID DATA:\n", pszFunction, iLineNum);
	for (i = 0; i < length;) {
		uint16_t j, k;
		uint16_t temp = i;
		char buffer[16*4+3];
		for (j = 0, k = 0; (j < 16) && (i < length); ++j, ++i) {
			buffer[k++] = ' ';
			buffer[k++] = hex_digits[(pData[i]>>4) & 0x0F];
			buffer[k++] = hex_digits[pData[i] & 0x0F];
		}

		buffer[k++] = ' ';
		buffer[k++] = '|';
		buffer[k++] = ' ';
		for (j = 0; (j < 16) && (temp < length); ++j, ++temp)
			buffer[k++] = ((pData[temp] >= ' ')
				&& (pData[temp] <= 'z')) ? pData[temp] : '.';

		printk(KERN_DEFAULT "%s\n", buffer);
	}
}
#endif

int process_emsc_edid_sub_payload(struct edid_3d_data_t *edid_context,
				struct si_adopter_id_data *p_burst)
{
	uint8_t *p_block;
	p_block = &p_burst->opcode_data.edid_blk.data[0];
	if (0 == p_burst->opcode_data.edid_blk.block_num) {
		size_t alloc_size;
		struct EDID_block0_t *edid_block0;
		edid_block0 = (struct EDID_block0_t *)p_block;
		si_edid_reset(edid_context);
		if (NULL != edid_context->p_edid_emsc) {
			MHL_TX_DBG_INFO("freeing edid bypass\n")
			kfree(edid_context->p_edid_emsc);
			edid_context->p_edid_emsc = NULL;
		}
		edid_context->num_emsc_edid_extensions =
			edid_block0->extension_flag;
		edid_context->num_edid_emsc_blocks = 1 +
			edid_block0->extension_flag;
		edid_context->cur_edid_emsc_block = 0;
		alloc_size = EDID_BLOCK_SIZE *
				edid_context->num_edid_emsc_blocks;
		edid_context->p_edid_emsc = kmalloc(alloc_size, GFP_KERNEL);
		if (NULL == edid_context->p_edid_emsc) {
			MHL_TX_DBG_ERR("%skmalloc failed%s\n",
				ANSI_ESC_RED_TEXT,
				ANSI_ESC_RESET_TEXT)
			return -1;
		} else {
			MHL_TX_DBG_ERR("block 0\n")
			memcpy(edid_context->p_edid_emsc,
				p_block,
				EDID_BLOCK_SIZE);
			if (0 == edid_block0->extension_flag) {
				MHL_TX_DBG_WARN("eMSC EDID done\n")
				MHL_TX_DBG_WARN("waiting for SET_HPD\n")
			}
		}

	} else if (edid_context->p_edid_emsc) {
		size_t edid_offset;
		uint8_t block_num;
		edid_offset = EDID_BLOCK_SIZE *
			p_burst->opcode_data.edid_blk.block_num;
		block_num = p_burst->opcode_data.edid_blk.block_num;
		if (edid_offset >= EDID_BLOCK_SIZE *
			edid_context->num_edid_emsc_blocks) {
			MHL_TX_DBG_ERR("EDID overflow %d offset: 0x%X\n",
				block_num, edid_offset)
			return -1;
		} else if (edid_context->cur_edid_emsc_block <
			edid_context->num_edid_emsc_blocks) {
			MHL_TX_DBG_ERR("block %d offset 0x%X\n",
				block_num, edid_offset)
			DUMP_EDID_BLOCK(0, p_block, EDID_BLOCK_SIZE);
			memcpy(&edid_context->p_edid_emsc[edid_offset],
				p_block, EDID_BLOCK_SIZE);
			edid_context->cur_edid_emsc_block++;
			if (edid_context->cur_edid_emsc_block >=
				edid_context->num_emsc_edid_extensions) {
				MHL_TX_DBG_WARN("eMSC EDID done\n")
				MHL_TX_DBG_WARN("waiting for SET_HPD\n")
			}
		}
	} else {
		MHL_TX_DBG_ERR("%s unexpected NULL pointer%s\n",
			ANSI_ESC_RED_TEXT,
			ANSI_ESC_RESET_TEXT)
	}

	return 0;
}
