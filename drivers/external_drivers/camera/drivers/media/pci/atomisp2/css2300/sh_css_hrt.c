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

#include "platform_support.h"

#include "sh_css_hrt.h"

#include "device_access.h"

#define __INLINE_SP__
#include "sp.h"
#define __INLINE_ISP__
#include "isp.h"
#define __INLINE_IRQ__
#include "irq.h"
#define __INLINE_FIFO_MONITOR__
#include "fifo_monitor.h"

#include "input_system.h"	/* MIPI_PREDICTOR_NONE,... */

/* System independent */
#include "sh_css_internal.h"
#include "sh_css_sp_start.h"	/* sh_css_sp_start_function() */

#define HBLANK_CYCLES (187)
#define MARKER_CYCLES (6)

#include <hive_isp_css_streaming_to_mipi_types_hrt.h>

/* The data type is used to send special cases:
 * yuv420: odd lines (1, 3 etc) are twice as wide as even
 *         lines (0, 2, 4 etc).
 * rgb: for two pixels per clock, the R and B values are sent
 *      to output_0 while only G is sent to output_1. This means
 *      that output_1 only gets half the number of values of output_0.
 *      WARNING: This type should also be used for Legacy YUV420.
 * regular: used for all other data types (RAW, YUV422, etc)
 */
enum sh_css_mipi_data_type {
	sh_css_mipi_data_type_regular,
	sh_css_mipi_data_type_yuv420,
	sh_css_mipi_data_type_yuv420_legacy,
	sh_css_mipi_data_type_rgb,
};

static hrt_address gp_fifo_base_address     = GP_FIFO_BASE;
static unsigned int curr_ch_id, curr_fmt_type;

struct streaming_to_mipi_instance {
	unsigned int				ch_id;
	enum sh_css_input_format	input_format;
	bool						two_ppc;
	bool						streaming;
	unsigned int				hblank_cycles;
	unsigned int				marker_cycles;
	unsigned int				fmt_type;
	enum sh_css_mipi_data_type	type;
};

/*
 * Maintain a basic streaming to Mipi administration with ch_id as index
 * ch_id maps on the "Mipi virtual channel ID" and can have value 0..3
 */
#define NR_OF_S2M_CHANNELS	(4)
static struct streaming_to_mipi_instance s2m_inst_admin[NR_OF_S2M_CHANNELS];

void sh_css_hrt_sp_start_isp(void)
{
	const struct sh_css_fw_info *fw = &sh_css_sp_fw;
	unsigned int HIVE_ADDR_sp_start_isp_entry =
					fw->info.sp.sp_entry;
	(void)HIVE_ADDR_sp_start_isp_entry;
	sh_css_sp_start_function(sp_start_isp);
return;
}

bool sh_css_hrt_system_is_idle(void)
{
	hrt_data	status;
	bool not_idle = false;

	not_idle |= !isp_ctrl_getbit(ISP0_ID, ISP_SC_REG, ISP_IDLE_BIT);

	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
		HIVE_GP_REGS_SP_STREAM_STAT_IDX);
	not_idle |= ((status & FIFO_CHANNEL_SP_VALID_MASK) != 0);

#if defined(HAS_FIFO_MONITORS_VERSION_2)
	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
		HIVE_GP_REGS_SP_STREAM_STAT_B_IDX);
	not_idle |= ((status & FIFO_CHANNEL_SP_VALID_B_MASK) != 0);
#endif

	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
		HIVE_GP_REGS_ISP_STREAM_STAT_IDX);
	not_idle |= ((status & FIFO_CHANNEL_ISP_VALID_MASK) != 0);

	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
		HIVE_GP_REGS_MOD_STREAM_STAT_IDX);
	not_idle |= ((status & FIFO_CHANNEL_MOD_VALID_MASK) != 0);

return !not_idle;
}

enum sh_css_err sh_css_hrt_sp_wait(void)
{
#if defined(HAS_IRQ_MAP_VERSION_2)
	irq_sw_channel_id_t	irq_id = IRQ_SW_CHANNEL0_ID;
#else
	irq_sw_channel_id_t	irq_id = IRQ_SW_CHANNEL2_ID;
#endif
	/*
	 * Wait till SP is idle or till there is a SW2 interrupt
	 * The SW2 interrupt will be used when frameloop runs on SP
	 * and signals an event with similar meaning as SP idle
	 * (e.g. frame_done)
	 */
	while (!sp_ctrl_getbit(SP0_ID, SP_SC_REG, SP_IDLE_BIT) &&
		((irq_reg_load(IRQ0_ID,
			_HRT_IRQ_CONTROLLER_STATUS_REG_IDX) &
			(1U<<(irq_id + IRQ_SW_CHANNEL_OFFSET))) == 0)) {
		hrt_sleep();
	}

return sh_css_success;
}

/* Streaming to MIPI */
static unsigned _sh_css_wrap_marker(
/* STORAGE_CLASS_INLINE unsigned _sh_css_wrap_marker( */
	unsigned marker)
{
return marker |
	(curr_ch_id << HIVE_STR_TO_MIPI_CH_ID_LSB) |
	(curr_fmt_type << _HIVE_STR_TO_MIPI_FMT_TYPE_LSB);
}

static void sh_css_streaming_to_mipi_send_data_a(
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_data_a( */
	unsigned int data)
{
	unsigned int token = (1 << HIVE_STR_TO_MIPI_VALID_A_BIT) |
			     (data << HIVE_STR_TO_MIPI_DATA_A_LSB);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_data_b(
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_data_b( */
	unsigned int data)
{
	unsigned int token = (1 << HIVE_STR_TO_MIPI_VALID_B_BIT) |
			     (data << _HIVE_STR_TO_MIPI_DATA_B_LSB);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_data(
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_data( */
	unsigned int a,
	unsigned int b)
{
	unsigned int token = ((1 << HIVE_STR_TO_MIPI_VALID_A_BIT) |
			      (1 << HIVE_STR_TO_MIPI_VALID_B_BIT) |
			      (a << HIVE_STR_TO_MIPI_DATA_A_LSB) |
			      (b << _HIVE_STR_TO_MIPI_DATA_B_LSB));
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_sol(void)
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_sol(void) */
{
	hrt_data	token = _sh_css_wrap_marker(
		1 << HIVE_STR_TO_MIPI_SOL_BIT);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_eol(void)
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_eol(void) */
{
	hrt_data	token = _sh_css_wrap_marker(
		1 << HIVE_STR_TO_MIPI_EOL_BIT);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_sof(void)
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_sof(void) */
{
	hrt_data	token = _sh_css_wrap_marker(
		1 << HIVE_STR_TO_MIPI_SOF_BIT);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_eof(void)
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_eof(void) */
{
	hrt_data	token = _sh_css_wrap_marker(
		1 << HIVE_STR_TO_MIPI_EOF_BIT);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

#ifdef __ON__
static void sh_css_streaming_to_mipi_send_ch_id(
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_ch_id( */
	unsigned int ch_id)
{
	hrt_data	token;
	curr_ch_id = ch_id & _HIVE_ISP_CH_ID_MASK;
	/* we send an zero marker, this will wrap the ch_id and
	 * fmt_type automatically.
	 */
	token = _sh_css_wrap_marker(0);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_fmt_type(
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_fmt_type( */
	unsigned int fmt_type)
{
	hrt_data	token;
	curr_fmt_type = fmt_type & _HIVE_ISP_FMT_TYPE_MASK;
	/* we send an zero marker, this will wrap the ch_id and
	 * fmt_type automatically.
	 */
	token = _sh_css_wrap_marker(0);
	device_store_uint32(gp_fifo_base_address, token);
return;
}
#endif /*  __ON__ */

static void sh_css_streaming_to_mipi_send_ch_id_and_fmt_type(
/* STORAGE_CLASS_INLINE
void sh_css_streaming_to_mipi_send_ch_id_and_fmt_type( */
	unsigned int ch_id,
	unsigned int fmt_type)
{
	hrt_data	token;
	curr_ch_id = ch_id & _HIVE_ISP_CH_ID_MASK;
	curr_fmt_type = fmt_type & _HIVE_ISP_FMT_TYPE_MASK;
	/* we send an zero marker, this will wrap the ch_id and
	 * fmt_type automatically.
	 */
	token = _sh_css_wrap_marker(0);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_streaming_to_mipi_send_empty_token(void)
/* STORAGE_CLASS_INLINE void sh_css_streaming_to_mipi_send_empty_token(void) */
{
	hrt_data	token = _sh_css_wrap_marker(0);
	device_store_uint32(gp_fifo_base_address, token);
return;
}

static void sh_css_hrt_s2m_start_frame(
/* STORAGE_CLASS_INLINE void sh_css_hrt_s2m_start_frame( */
	unsigned int ch_id,
	unsigned int fmt_type)
{
	sh_css_streaming_to_mipi_send_ch_id_and_fmt_type(ch_id, fmt_type);
	sh_css_streaming_to_mipi_send_sof();
return;
}

static void sh_css_hrt_s2m_end_frame(
	unsigned int marker_cycles)
{
	unsigned int i;
	for (i = 0; i < marker_cycles; i++)
		sh_css_streaming_to_mipi_send_empty_token();
	sh_css_streaming_to_mipi_send_eof();
return;
}

static void sh_css_hrt_s2m_send_line2(
	unsigned short *data,
	unsigned int width,
	unsigned short *data2,
	unsigned int width2,
	unsigned int hblank_cycles,
	unsigned int marker_cycles,
	unsigned int two_ppc,
	enum sh_css_mipi_data_type type)
{
	unsigned int i, is_rgb = 0, is_legacy = 0;

	if (type == sh_css_mipi_data_type_rgb)
		is_rgb = 1;

	if (type == sh_css_mipi_data_type_yuv420_legacy)
		is_legacy = 1;

	for (i = 0; i < hblank_cycles; i++)
		sh_css_streaming_to_mipi_send_empty_token();
	sh_css_streaming_to_mipi_send_sol();
	for (i = 0; i < marker_cycles; i++)
		sh_css_streaming_to_mipi_send_empty_token();
	for (i = 0; i < width; i++, data++) {
		/* for RGB in two_ppc, we only actually send 2 pixels per
		 * clock in the even pixels (0, 2 etc). In the other cycles,
		 * we only send 1 pixel, to data[0].
		 */
		unsigned int send_two_pixels = two_ppc;
		if ((is_rgb || is_legacy) && (i % 3 == 2))
			send_two_pixels = 0;
		if (send_two_pixels) {
			if (i + 1 == width) {
				/* for jpg (binary) copy, this can occur
				 * if the file contains an odd number of bytes.
				 */
				sh_css_streaming_to_mipi_send_data(
							data[0], 0);
			} else {
				sh_css_streaming_to_mipi_send_data(
							data[0], data[1]);
			}
			/* Additional increment because we send 2 pixels */
			data++;
			i++;
		} else if (two_ppc && is_legacy) {
			sh_css_streaming_to_mipi_send_data_b(data[0]);
		} else {
			sh_css_streaming_to_mipi_send_data_a(data[0]);
		}
	}

	for (i = 0; i < width2; i++, data2++) {
		/* for RGB in two_ppc, we only actually send 2 pixels per
		 * clock in the even pixels (0, 2 etc). In the other cycles,
		 * we only send 1 pixel, to data2[0].
		 */
		unsigned int send_two_pixels = two_ppc;
		if ((is_rgb || is_legacy) && (i % 3 == 2))
			send_two_pixels = 0;
		if (send_two_pixels) {
			if (i + 1 == width2) {
				/* for jpg (binary) copy, this can occur
				 * if the file contains an odd number of bytes.
				 */
				sh_css_streaming_to_mipi_send_data(
							data2[0], 0);
			} else {
				sh_css_streaming_to_mipi_send_data(
							data2[0], data2[1]);
			}
			/* Additional increment because we send 2 pixels */
			data2++;
			i++;
		} else if (two_ppc && is_legacy) {
			sh_css_streaming_to_mipi_send_data_b(data2[0]);
		} else {
			sh_css_streaming_to_mipi_send_data_a(data2[0]);
		}
	}
	for (i = 0; i < hblank_cycles; i++)
		sh_css_streaming_to_mipi_send_empty_token();
	sh_css_streaming_to_mipi_send_eol();
return;
}

static void
sh_css_hrt_s2m_send_line(unsigned short *data,
				unsigned int width,
				unsigned int hblank_cycles,
				unsigned int marker_cycles,
				unsigned int two_ppc,
				enum sh_css_mipi_data_type type)
{
	sh_css_hrt_s2m_send_line2(data, width, NULL, 0,
					hblank_cycles,
					marker_cycles,
					two_ppc,
					type);
return;
}

/* Send a frame of data into the input network via the GP FIFO.
 *  Parameters:
 *   - data: array of 16 bit values that contains all data for the frame.
 *   - width: width of a line in number of subpixels, for yuv420 it is the
 *            number of Y components per line.
 *   - height: height of the frame in number of lines.
 *   - ch_id: channel ID.
 *   - fmt_type: format type.
 *   - hblank_cycles: length of horizontal blanking in cycles.
 *   - marker_cycles: number of empty cycles after start-of-line and before
 *                    end-of-frame.
 *   - two_ppc: boolean, describes whether to send one or two pixels per clock
 *              cycle. In this mode, we sent pixels N and N+1 in the same cycle,
 *              to IF_PRIM_A and IF_PRIM_B respectively. The caller must make
 *              sure the input data has been formatted correctly for this.
 *              For example, for RGB formats this means that unused values
 *              must be inserted.
 *   - yuv420: boolean, describes whether (non-legacy) yuv420 data is used. In
 *             this mode, the odd lines (1,3,5 etc) are half as long as the
 *             even lines (2,4,6 etc).
 *             Note that the first line is odd (1) and the second line is even
 *             (2).
 *
 * This function does not do any reordering of pixels, the caller must make
 * sure the data is in the righ format. Please refer to the CSS receiver
 * documentation for details on the data formats.
 */
static void sh_css_hrt_s2m_send_frame(
	unsigned short *data,
	unsigned int width,
	unsigned int height,
	unsigned int ch_id,
	unsigned int fmt_type,
	unsigned int hblank_cycles,
	unsigned int marker_cycles,
	unsigned int two_ppc,
	enum sh_css_mipi_data_type type)
{
	unsigned int i;

	sh_css_hrt_s2m_start_frame(ch_id, fmt_type);
	for (i = 0; i < height; i++) {
		if ((type == sh_css_mipi_data_type_yuv420) &&
		    (i & 1) == 1) {
			sh_css_hrt_s2m_send_line(data, 2 * width,
							   hblank_cycles,
							   marker_cycles,
							   two_ppc, type);
			data += 2 * width;
		} else {
			sh_css_hrt_s2m_send_line(data, width,
							   hblank_cycles,
							   marker_cycles,
							   two_ppc, type);
			data += width;
		}
	}
	sh_css_hrt_s2m_end_frame(marker_cycles);
return;
}

static enum sh_css_mipi_data_type sh_css_hrt_s2m_determine_type(
	enum sh_css_input_format input_format)
{
	enum sh_css_mipi_data_type type;

	type = sh_css_mipi_data_type_regular;
	if (input_format == SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY) {
		type =
			sh_css_mipi_data_type_yuv420_legacy;
	} else if (input_format == SH_CSS_INPUT_FORMAT_YUV420_8 ||
		   input_format == SH_CSS_INPUT_FORMAT_YUV420_10) {
		type =
			sh_css_mipi_data_type_yuv420;
	} else if (input_format >= SH_CSS_INPUT_FORMAT_RGB_444 &&
		   input_format <= SH_CSS_INPUT_FORMAT_RGB_888) {
		type =
			sh_css_mipi_data_type_rgb;
	}
return type;
}

static struct streaming_to_mipi_instance *sh_css_hrt_s2m_get_inst(
	unsigned int ch_id)
{
return &s2m_inst_admin[ch_id];
}

void sh_css_hrt_send_input_frame(
	unsigned short *data,
	unsigned int width,
	unsigned int height,
	unsigned int ch_id,
	enum sh_css_input_format input_format,
	bool two_ppc)
{
	unsigned int fmt_type, hblank_cycles, marker_cycles;
	enum sh_css_mipi_data_type type;

	hblank_cycles = HBLANK_CYCLES;
	marker_cycles = MARKER_CYCLES;
	sh_css_input_format_type(input_format,
				 MIPI_PREDICTOR_NONE,
				 &fmt_type);

	type = sh_css_hrt_s2m_determine_type(input_format);

	sh_css_hrt_s2m_send_frame(data, width, height,
			ch_id, fmt_type, hblank_cycles, marker_cycles,
			two_ppc, type);
return;
}

void sh_css_hrt_streaming_to_mipi_start_frame(
	unsigned int ch_id,
	enum sh_css_input_format input_format,
	bool two_ppc)
{
	struct streaming_to_mipi_instance *s2mi;
	s2mi = sh_css_hrt_s2m_get_inst(ch_id);

	s2mi->ch_id = ch_id;
	sh_css_input_format_type(input_format, MIPI_PREDICTOR_NONE,
				&s2mi->fmt_type);
	s2mi->two_ppc = two_ppc;
	s2mi->type = sh_css_hrt_s2m_determine_type(input_format);
	s2mi->hblank_cycles = HBLANK_CYCLES;
	s2mi->marker_cycles = MARKER_CYCLES;
	s2mi->streaming = true;

	sh_css_hrt_s2m_start_frame(ch_id, s2mi->fmt_type);
return;
}

void sh_css_hrt_streaming_to_mipi_send_line(
	unsigned int ch_id,
	unsigned short *data,
	unsigned int width,
	unsigned short *data2,
	unsigned int width2)
{
	struct streaming_to_mipi_instance *s2mi;
	s2mi = sh_css_hrt_s2m_get_inst(ch_id);

	/* Set global variables that indicate channel_id and format_type */
	curr_ch_id = (s2mi->ch_id) & _HIVE_ISP_CH_ID_MASK;
	curr_fmt_type = (s2mi->fmt_type) & _HIVE_ISP_FMT_TYPE_MASK;

	sh_css_hrt_s2m_send_line2(data, width, data2, width2,
					s2mi->hblank_cycles,
					s2mi->marker_cycles,
					s2mi->two_ppc,
					s2mi->type);
return;
}

void sh_css_hrt_streaming_to_mipi_end_frame(
	unsigned int	ch_id)
{
	struct streaming_to_mipi_instance *s2mi;
	s2mi = sh_css_hrt_s2m_get_inst(ch_id);

	/* Set global variables that indicate channel_id and format_type */
	curr_ch_id = (s2mi->ch_id) & _HIVE_ISP_CH_ID_MASK;
	curr_fmt_type = (s2mi->fmt_type) & _HIVE_ISP_FMT_TYPE_MASK;

	/* Call existing HRT function */
	sh_css_hrt_s2m_end_frame(s2mi->marker_cycles);

	s2mi->streaming = false;
return;
}

