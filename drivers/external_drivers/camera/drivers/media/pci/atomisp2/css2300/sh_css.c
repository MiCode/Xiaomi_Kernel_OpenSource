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

/*! \file */

#include "sh_css.h"
#include "sh_css_v2.h"
#include "sh_css_hrt.h"		/* only for file 2 MIPI */
#include "sh_css_binary.h"
#include "sh_css_internal.h"
#include "sh_css_sp.h"
#include "sh_css_sp_start.h"
#include "sh_css_rx.h"
#include "sh_css_defs.h"
#include "sh_css_firmware.h"
#include "sh_css_accelerate.h"
#include "sh_css_params.h"
#include "sh_css_params_internal.h"
#include "sh_css_param_shading.h"
#include "sh_css_pipeline.h"
#include "sh_css_refcount.h"
#include "ia_css_i_rmgr.h"
#include "sh_css_debug.h"
#include "sh_css_debug_internal.h"

#include "memory_access.h"
#include "tag.h"
#include "assert_support.h"
#include "queue.h"			/* host2sp_enqueue_frame_data() */
#include "sw_event.h"			/* encode_sw_event */
#include "input_formatter.h"/* input_formatter_cfg_t,
	input_formatter_get_alignment(), ... */
#include "input_system.h"
#include "mmu_device.h"		/* mmu_set_page_table_base_index(), ... */
#include "gdc_device.h"		/* HRT_GDC_N */
#include "irq.h"			/* virq */
#include "sp.h"				/* cnd_sp_irq_enable() */
#include "isp.h"			/* cnd_isp_irq_enable, ISP_VEC_NELEMS */
#define __INLINE_GPIO__
#include "gpio.h"
#include "timed_ctrl.h"
#include "platform_support.h" /* hrt_sleep() */

#define WITH_PC_MONITORING  0

#if WITH_PC_MONITORING
#define MULTIPLE_SAMPLES 1
#define NOF_SAMPLES      60
#include "linux/kthread.h"
#include "linux/sched.h"
#include "linux/delay.h"
#include "sh_css_metrics.h"




static int thread_alive;
#endif

#define DVS_REF_TESTING 0
#if DVS_REF_TESTING
#include <stdio.h>
#endif

/* Name of the sp program: should not be built-in */
#define SP_PROG_NAME "sp"

/* for JPEG, we don't know the length of the image upfront,
 * but since we support sensor upto 16MP, we take this as
 * upper limit.
 */
#define JPEG_BYTES (16 * 1024 * 1024)

#define IS_ODD(a)              ((a) & 0x1)

#define IMPLIES(a, b)           (!(a) || (b))   /* A => B */

#define STATS_ENABLED(stage) (stage && stage->binary && stage->binary->info && \
	(stage->binary->info->enable.s3a || stage->binary->info->enable.dis))

#define DEFAULT_IF_CONFIG \
{ \
	0,          /* start_line */\
	0,          /* start_column */\
	0,          /* left_padding */\
	0,          /* cropped_height */\
	0,          /* cropped_width */\
	0,          /* deinterleaving */\
	0,          /*.buf_vecs */\
	0,          /* buf_start_index */\
	0,          /* buf_increment */\
	0,          /* buf_eol_offset */\
	false,      /* is_yuv420_format */\
	false       /* block_no_reqs */\
}

enum sh_css_state {
	sh_css_state_idle,
	sh_css_state_executing_isp,
	sh_css_state_executing_sp_bin_copy,
};

#define DEFAULT_PLANES { {0, 0, 0, 0} }

#define DEFAULT_FRAME \
{ \
	DEFAULT_FRAME_INFO, \
	0, \
	0, \
	-1, \
	SH_CSS_FRAME_NO_FLASH, \
	0, \
	false, \
	{ 0 } \
}

#define DEFAULT_PIPELINE_SETTINGS \
{ \
	0, \
	NULL, \
	false, \
	NULL, \
	0, \
	DEFAULT_FRAME, \
	DEFAULT_FRAME, \
	DEFAULT_FRAME \
}

#define DEFAULT_PIPE \
	true,                      /* online */ \
	NULL,                      /* shading_table */ \
	DEFAULT_PIPELINE_SETTINGS, /* pipeline */ \
	DEFAULT_FRAME_INFO,        /* output_info */\
	DEFAULT_FRAME_INFO,        /* vf_output_info */ \
	DEFAULT_FRAME_INFO,        /* yuv_ds_input_info */\
	0,                         /* ch_id */ \
	N_SH_CSS_INPUT_FORMAT,     /* input_format */ \
	SH_CSS_INPUT_MODE_SENSOR,  /* input_mode */ \
	false,                     /* two_ppc */ \
	N_sh_css_bayer_order,      /* bayer_order */ \
	false,                     /* disable_vf_pp */\
	false,                     /* disable_capture_pp */ \
	false,                     /* input_needs_raw_binning */ \
	DEFAULT_MIPI_CONFIG,       /* mipi_config */ \
	NULL,                      /* output_stage */\
	NULL,                      /* vf_stage */ \
	0,                         /* input_width */ \
	0,                         /* input_height */ \
	DEFAULT_FRAME_INFO,        /* input_effective_info */ \
	SH_CSS_CAPTURE_MODE_PRIMARY,/* capture_mode */ \
	false,                     /* xnr */ \
	{ 0, 0 },                  /* dvs_envelope */ \
	false,                     /* invalid_first_frame */ \
	false,                     /* enable_yuv_ds */ \
	false,                     /* enable_high_speed */ \
	false,                     /* enable_dvs_6axis */ \
	true,                      /* enable_viewfinder */ \
	true,                      /* enable_dz */ \
	false,                     /* enable_reduced_pipe */ \
	1,                         /* isp_pipe_version */ \
	{ }                        /* settings */

struct sh_css_preview_settings {
	struct sh_css_binary copy_binary;
	struct sh_css_binary preview_binary;
	struct sh_css_binary vf_pp_binary;
	struct sh_css_frame *continuous_frames[NUM_CONTINUOUS_FRAMES];
};

#define DEFAULT_PREVIEW_SETTINGS \
{ \
	DEFAULT_BINARY_SETTINGS,  /* copy_binary */\
	DEFAULT_BINARY_SETTINGS,  /* preview_binary */\
	DEFAULT_BINARY_SETTINGS,  /* vf_pp_binary */\
	{ NULL },                 /* continuous_frames */\
}

#define DEFAULT_PREVIEW_PIPE \
{ \
	SH_CSS_PREVIEW_PIPELINE, \
	DEFAULT_PIPE \
}

struct sh_css_capture_settings {
	struct sh_css_binary copy_binary;
	struct sh_css_binary primary_binary;
	struct sh_css_binary pre_isp_binary;
	struct sh_css_binary gdc_binary;
	struct sh_css_binary post_isp_binary;
	struct sh_css_binary anr_binary;
	struct sh_css_binary capture_pp_binary;
	struct sh_css_binary vf_pp_binary;
	struct sh_css_frame *capture_pp_frame;
	struct sh_css_frame *output_frame;
	/*struct sh_css_frame *continuous_frames[NUM_CONTINUOUS_FRAMES];*/
};

#define DEFAULT_CAPTURE_SETTINGS \
{ \
	DEFAULT_BINARY_SETTINGS,     /* copy_binary */\
	DEFAULT_BINARY_SETTINGS,     /* primary_binary */\
	DEFAULT_BINARY_SETTINGS,     /* pre_isp_binary */\
	DEFAULT_BINARY_SETTINGS,     /* gdc_binary */\
	DEFAULT_BINARY_SETTINGS,     /* post_isp_binary */\
	DEFAULT_BINARY_SETTINGS,     /* anr_binary */\
	DEFAULT_BINARY_SETTINGS,     /* capture_pp_binary */\
	DEFAULT_BINARY_SETTINGS,     /* vf_pp_binary */\
	NULL,                        /* capture_pp_frame */\
	NULL,                        /* output_frame */\
	/*{ NULL },   */              /* continuous_frames */\
}

#define DEFAULT_CAPTURE_PIPE \
{ \
	SH_CSS_CAPTURE_PIPELINE, \
	DEFAULT_PIPE \
}

struct sh_css_video_settings {
	struct sh_css_binary copy_binary;
	struct sh_css_binary video_binary;
	struct sh_css_binary vf_pp_binary;
	struct sh_css_frame *ref_frames[NUM_VIDEO_REF_FRAMES];
	struct sh_css_frame *tnr_frames[NUM_VIDEO_TNR_FRAMES];
	struct sh_css_frame *vf_pp_in_frame;
};

#define DEFAULT_VIDEO_SETTINGS \
{ \
	DEFAULT_BINARY_SETTINGS,/* copy_binary */ \
	DEFAULT_BINARY_SETTINGS,/* video_binary */ \
	DEFAULT_BINARY_SETTINGS,/* vf_pp_binary */ \
	{ NULL },                /* ref_frames */ \
	{ NULL },                /* tnr_frames */ \
	NULL,                    /* vf_pp_in_frame */ \
}

#define DEFAULT_VIDEO_PIPE \
{ \
	SH_CSS_VIDEO_PIPELINE, \
	DEFAULT_PIPE \
}

#define DEFAULT_ACC_PIPE \
{ \
	SH_CSS_ACC_PIPELINE, \
	DEFAULT_PIPE \
}

struct sh_css_pipe {
	enum sh_css_pipe_id          mode;
	bool                         online;
	struct sh_css_shading_table *shading_table;
	struct sh_css_pipeline       pipeline;
	struct sh_css_frame_info     output_info;
	struct sh_css_frame_info     vf_output_info;
	struct sh_css_frame_info     yuv_ds_input_info;
	unsigned int                 ch_id;
	enum sh_css_input_format     input_format;
	enum sh_css_input_mode       input_mode;
	bool                         two_ppc;
	enum sh_css_bayer_order      bayer_order;
	bool                         disable_vf_pp;
	bool                         disable_capture_pp;
	bool                         input_needs_raw_binning;
	rx_cfg_t                     mipi_config;
	struct sh_css_fw_info	    *output_stage; /* extra output stage */
	struct sh_css_fw_info	    *vf_stage;     /* extra vf stage */
	unsigned int                   input_width;
	unsigned int                   input_height;
	struct sh_css_frame_info       input_effective_info;
	enum sh_css_capture_mode     capture_mode;
	bool                         xnr;
	struct sh_css_envelope       dvs_envelope;
	bool                         invalid_first_frame;
	bool                         enable_yuv_ds;
	bool                         enable_high_speed;
	bool                         enable_dvs_6axis;
	bool                         enable_viewfinder;
	bool                         enable_dz;
	bool                         enable_reduced_pipe;
	unsigned int                 isp_pipe_version;
	union {
		struct sh_css_preview_settings preview;
		struct sh_css_video_settings   video;
		struct sh_css_capture_settings capture;
	} pipe;
};

struct sh_css {
	struct sh_css_pipe             preview_pipe;
	struct sh_css_pipe             capture_pipe;
	struct sh_css_pipe             video_pipe;
	struct sh_css_pipe             acc_pipe;
	unsigned int                   left_padding;
	struct sh_css_pipe            *curr_pipe;
	bool                           reconfigure_css_rx;
	void *(*malloc) (size_t bytes, bool zero_mem);
	void (*free) (void *ptr);
	void (*flush) (struct sh_css_acc_fw *fw);
	enum sh_css_state              curr_state;
	unsigned int                   sensor_binning;
	input_formatter_cfg_t          curr_if_a_config;
	input_formatter_cfg_t          curr_if_b_config;
	unsigned int                   curr_fmt_type;
	unsigned int                   curr_ch_id;
	enum sh_css_input_mode         curr_input_mode;
	bool                           check_system_idle;
	bool                           continuous;
	bool                           cont_capt;
	bool						   stop_copy_preview;
	unsigned int                   num_cont_raw_frames;
	struct sh_css_event_irq_mask   sp_irq_mask[NR_OF_PIPELINES];
	bool                           start_sp_copy;
	hrt_vaddress                   sp_bin_addr;
	hrt_data		       page_table_base_index;
};

#define DEFAULT_CSS \
{ \
	DEFAULT_PREVIEW_PIPE,     /* preview_pipe */ \
	DEFAULT_CAPTURE_PIPE,     /* capture_pipe */ \
	DEFAULT_VIDEO_PIPE,       /* video_pipe */ \
	DEFAULT_ACC_PIPE,         /* acc_pipe */ \
	0,                        /* left_padding */ \
	NULL,                     /* curr_pipe */ \
	true,                     /* reconfigure_css_rx */ \
	NULL,                     /* malloc */ \
	NULL,                     /* free */ \
	NULL,                     /* flush */ \
	sh_css_state_idle,        /* curr_state */ \
	0,                        /* sensor_binning */ \
	DEFAULT_IF_CONFIG,        /* curr_if_a_config */ \
	DEFAULT_IF_CONFIG,        /* curr_if_b_config */ \
	-1,                       /* curr_fmt_type */ \
	0,                        /* curr_ch_id */ \
	SH_CSS_INPUT_MODE_SENSOR, /* curr_input_mode */ \
	true,                     /* check_system_idle */ \
	false,                    /* continuous */ \
	false,                    /* cont_capt */ \
	false,                    /* stop_copy_preview */ \
	NUM_CONTINUOUS_FRAMES,    /* num_cont_raw_frames */ \
	{{SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_VF_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE, SH_CSS_EVENT_IRQ_MASK_NONE}, \
	{SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_VF_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE, SH_CSS_EVENT_IRQ_MASK_NONE}, \
	{SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_VF_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE, SH_CSS_EVENT_IRQ_MASK_NONE}, \
	{SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_VF_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE, SH_CSS_EVENT_IRQ_MASK_NONE}, \
	{SH_CSS_EVENT_IRQ_MASK_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_VF_OUTPUT_FRAME_DONE | SH_CSS_EVENT_IRQ_MASK_3A_STATISTICS_DONE, SH_CSS_EVENT_IRQ_MASK_NONE}},    /* sp_irq_mask */ \
	false,                    /* start_sp_copy */ \
	0,                        /* sp_bin_addr */ \
	0,                        /* page_table_base_index */ \
}

#if defined(HAS_RX_VERSION_2)

#define DEFAULT_MIPI_CONFIG \
{ \
	MONO_4L_1L_0L, \
	MIPI_PORT0_ID, \
	0xffff4, \
	0, \
	0x28282828, \
	0x04040404, \
	MIPI_PREDICTOR_NONE, \
	false \
}

#elif defined(HAS_RX_VERSION_1) || defined(HAS_NO_RX)

#define DEFAULT_MIPI_CONFIG \
{ \
	MIPI_PORT1_ID, \
	1, \
	0xffff4, \
	0, \
	0, \
	MIPI_PREDICTOR_NONE, \
	false \
}

#else
#error "sh_css.c: RX version must be one of {RX_VERSION_1, RX_VERSION_2, NO_RX}"
#endif

int (*sh_css_printf) (const char *fmt, ...) = NULL;

unsigned int sh_css_stop_timeout_us;

static struct sh_css my_css;
/* static variables, temporarily used in load_<mode>_binaries.
   Declaring these inside the functions increases the size of the
   stack frames beyond the acceptable 128 bytes. */
static struct sh_css_binary_descr preview_descr,
				  vf_pp_descr,
				  copy_descr,
				  prim_descr,
				  pre_gdc_descr,
				  gdc_descr,
				  post_gdc_descr,
				  pre_anr_descr,
				  anr_descr,
				  post_anr_descr,
				  video_descr,
				  capture_pp_descr;

/* pqiao NOTICE: this is for css internal buffer recycling when stopping pipeline,
   this array is temporary and will be replaced by resource manager*/
#define MAX_HMM_BUFFER_NUM (SH_CSS_NUM_BUFFER_QUEUES * (SH_CSS_CIRCULAR_BUF_NUM_ELEMS + 2))
static struct ia_css_i_host_rmgr_vbuf_handle *hmm_buffer_record_h[MAX_HMM_BUFFER_NUM];

/* Should be consistent with SH_CSS_MAX_SP_THREADS */
/* See also pipe_threads in sp.hive.c, which should be consistent with this */
#define COPY_PIPELINE		(0)
#define PREVIEW_PIPELINE	(1)
#define CAPTURE_PIPELINE	(2)
#define ACCELERATION_PIPELINE	(3)
#define VIDEO_PIPELINE		(PREVIEW_PIPELINE)

/* From enum sh_css_pipe_id to struct sh_css_pipeline * */
static unsigned int sh_css_pipe_id_2_internal_thread_id[SH_CSS_NR_OF_PIPELINES]
	 = { PREVIEW_PIPELINE,
	     COPY_PIPELINE,
	     VIDEO_PIPELINE,
	     CAPTURE_PIPELINE,
	     ACCELERATION_PIPELINE };

#define GPIO_FLASH_PIN_MASK (1 << HIVE_GPIO_STROBE_TRIGGER_PIN)

static enum sh_css_buffer_queue_id
	sh_css_buf_type_2_internal_queue_id[SH_CSS_BUFFER_TYPE_NR_OF_TYPES] = {
		sh_css_s3a_buffer_queue,
		sh_css_dis_buffer_queue,
		sh_css_input_buffer_queue,
		sh_css_output_buffer_queue,
		sh_css_vf_output_buffer_queue,
		sh_css_output_buffer_queue,
		sh_css_input_buffer_queue,
		sh_css_output_buffer_queue,
		sh_css_param_buffer_queue };

/**
 * Local prototypes
 */
static bool
need_capture_pp(const struct sh_css_pipe *pipe);

static enum sh_css_err
sh_css_pipe_load_binaries(struct sh_css_pipe *pipe);

static enum sh_css_err
sh_css_pipe_get_output_frame_info(struct sh_css_pipe *pipe,
				  struct sh_css_frame_info *info);

static enum sh_css_err
capture_start(struct sh_css_pipe *pipe);

static enum sh_css_err
video_start(struct sh_css_pipe *pipe);

static enum sh_css_err
construct_capture_pipe(struct sh_css_pipe *pipe);

static enum sh_css_err
init_frame_planes(struct sh_css_frame *frame);

static enum sh_css_err
sh_css_pipeline_stop(enum sh_css_pipe_id pipe);

static uint32_t
translate_sp_event(uint32_t sp_event);

void
sh_css_set_stop_timeout(unsigned int timeout)
{
	sh_css_stop_timeout_us = timeout;
}

static void
sh_css_pipe_free_shading_table(struct sh_css_pipe *pipe)
{
	if (pipe->shading_table)
		sh_css_shading_table_free(pipe->shading_table);
	pipe->shading_table = NULL;
}

static enum sh_css_err
check_frame_info(const struct sh_css_frame_info *info)
{
	if (info->width == 0 || info->height == 0)
		return sh_css_err_illegal_resolution;
	return sh_css_success;
}

static enum sh_css_err
check_vf_info(const struct sh_css_frame_info *info)
{
	enum sh_css_err err;
	err = check_frame_info(info);
	if (err != sh_css_success)
		return err;
	if (info->width > sh_css_max_vf_width()*2)
		return sh_css_err_viewfinder_resolution_too_wide;
	return sh_css_success;
}

static enum sh_css_err
check_vf_out_info(const struct sh_css_frame_info *out_info,
		  const struct sh_css_frame_info *vf_info)
{
	enum sh_css_err err;
	err = check_frame_info(out_info);
	if (err != sh_css_success)
		return err;
	err = check_vf_info(vf_info);
	if (err != sh_css_success)
		return err;
	if (vf_info->width > out_info->width ||
	    vf_info->height > out_info->height)
		return sh_css_err_viewfinder_resolution_exceeds_output;
	return sh_css_success;
}

static enum sh_css_err
check_res(unsigned int width, unsigned int height)
{
	if (width  == 0   ||
	    height == 0   ||
	    IS_ODD(width) ||
	    IS_ODD(height)) {
		return sh_css_err_illegal_resolution;
	}
	return sh_css_success;
}

static enum sh_css_err
check_null_res(unsigned int width, unsigned int height)
{
	if (IS_ODD(width) || IS_ODD(height))
		return sh_css_err_illegal_resolution;

	return sh_css_success;
}

static bool
input_format_is_raw(enum sh_css_input_format format)
{
	return format == SH_CSS_INPUT_FORMAT_RAW_6 ||
	    format == SH_CSS_INPUT_FORMAT_RAW_7 ||
	    format == SH_CSS_INPUT_FORMAT_RAW_8 ||
	    format == SH_CSS_INPUT_FORMAT_RAW_10 ||
	    format == SH_CSS_INPUT_FORMAT_RAW_12;
	/* raw_14 and raw_16 are not supported as input formats to the ISP.
	 * They can only be copied to a frame in memory using the
	 * copy binary.
	 */
}

static bool
input_format_is_yuv(enum sh_css_input_format format)
{
	return format == SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY ||
	    format == SH_CSS_INPUT_FORMAT_YUV420_8 ||
	    format == SH_CSS_INPUT_FORMAT_YUV420_10 ||
	    format == SH_CSS_INPUT_FORMAT_YUV422_8 ||
	    format == SH_CSS_INPUT_FORMAT_YUV422_10;
}

bool
input_format_is_yuv_8(enum sh_css_input_format format)
{
	return format == SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY ||
	    format == SH_CSS_INPUT_FORMAT_YUV420_8 ||
	    format == SH_CSS_INPUT_FORMAT_YUV422_8;
}

static enum sh_css_err
check_input(struct sh_css_pipe *pipe, bool must_be_raw)
{
	if (pipe->input_effective_info.width == 0 ||
	    pipe->input_effective_info.height == 0) {
		return sh_css_err_effective_input_resolution_not_set;
	}
	if (must_be_raw &&
	    !input_format_is_raw(pipe->input_format)) {
		return sh_css_err_unsupported_input_format;
	}
	return sh_css_success;
}

/* Input network configuration functions */
static void
get_copy_out_frame_format(struct sh_css_pipe *pipe,
	enum sh_css_frame_format *format)
{
	switch (pipe->input_format) {
	case SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY:
	case SH_CSS_INPUT_FORMAT_YUV420_8:
		*format = SH_CSS_FRAME_FORMAT_YUV420;
		break;
	case SH_CSS_INPUT_FORMAT_YUV420_10:
		*format = SH_CSS_FRAME_FORMAT_YUV420_16;
		break;
	case SH_CSS_INPUT_FORMAT_YUV422_8:
		*format = SH_CSS_FRAME_FORMAT_YUV422;
		break;
	case SH_CSS_INPUT_FORMAT_YUV422_10:
		*format = SH_CSS_FRAME_FORMAT_YUV422_16;
		break;
	case SH_CSS_INPUT_FORMAT_RGB_444:
	case SH_CSS_INPUT_FORMAT_RGB_555:
	case SH_CSS_INPUT_FORMAT_RGB_565:
		if (*format != SH_CSS_FRAME_FORMAT_RGBA888)
			*format = SH_CSS_FRAME_FORMAT_RGB565;
		break;
	case SH_CSS_INPUT_FORMAT_RGB_666:
	case SH_CSS_INPUT_FORMAT_RGB_888:
		*format = SH_CSS_FRAME_FORMAT_RGBA888;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_6:
	case SH_CSS_INPUT_FORMAT_RAW_7:
	case SH_CSS_INPUT_FORMAT_RAW_8:
	case SH_CSS_INPUT_FORMAT_RAW_10:
	case SH_CSS_INPUT_FORMAT_RAW_12:
	case SH_CSS_INPUT_FORMAT_RAW_14:
	case SH_CSS_INPUT_FORMAT_RAW_16:
		*format = SH_CSS_FRAME_FORMAT_RAW;
		break;
	case SH_CSS_INPUT_FORMAT_BINARY_8:
		*format = SH_CSS_FRAME_FORMAT_BINARY_8;
		break;
	case N_SH_CSS_INPUT_FORMAT:
/* Fall through */
	default:
		*format = N_SH_CSS_FRAME_FORMAT;
		break;
	}
}

/* next function takes care of getting the settings from kernel
 * commited to hmm / isp
 * TODO: see if needs to be made public
 */
static enum sh_css_err
sh_css_commit_isp_config(void *me, bool queue)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_pipeline *pipeline = me;
	struct sh_css_pipeline_stage *stage;

	if (pipeline) {
		/* walk through pipeline and commit settings */
		/* TODO: check if this is needed (s3a is handled through this */
		for (stage = pipeline->stages; stage; stage = stage->next) {
			if (stage && stage->binary) {
				err = sh_css_params_write_to_ddr(stage->binary);
				if (err != sh_css_success)
					return err;
			}
		}
	}
	/* now propagate the set to sp */
	//sh_css_param_update_isp_params(queue);
	(void)me;
	(void)queue;
	return err;
}

static unsigned int
sh_css_pipe_input_format_bits_per_pixel(const struct sh_css_pipe *pipe)
{
	return sh_css_input_format_bits_per_pixel(pipe->input_format,
						  pipe->two_ppc);
}

/* MW: Table look-up ??? */
unsigned int
sh_css_input_format_bits_per_pixel(enum sh_css_input_format format,
	bool two_ppc)
{
	switch (format) {
	case SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY:
	case SH_CSS_INPUT_FORMAT_YUV420_8:
	case SH_CSS_INPUT_FORMAT_YUV422_8:
	case SH_CSS_INPUT_FORMAT_RGB_888:
	case SH_CSS_INPUT_FORMAT_RAW_8:
	case SH_CSS_INPUT_FORMAT_BINARY_8:
		return 8;
	case SH_CSS_INPUT_FORMAT_YUV420_10:
	case SH_CSS_INPUT_FORMAT_YUV422_10:
	case SH_CSS_INPUT_FORMAT_RAW_10:
		return 10;
	case SH_CSS_INPUT_FORMAT_RGB_444:
		return 4;
	case SH_CSS_INPUT_FORMAT_RGB_555:
		return 5;
	case SH_CSS_INPUT_FORMAT_RGB_565:
		return 65;
	case SH_CSS_INPUT_FORMAT_RGB_666:
	case SH_CSS_INPUT_FORMAT_RAW_6:
		return 6;
	case SH_CSS_INPUT_FORMAT_RAW_7:
		return 7;
	case SH_CSS_INPUT_FORMAT_RAW_12:
		return 12;
	case SH_CSS_INPUT_FORMAT_RAW_14:
		if (two_ppc)
			return 14;
		else
			return 12;
	case SH_CSS_INPUT_FORMAT_RAW_16:
		if (two_ppc)
			return 16;
		else
			return 12;
	case N_SH_CSS_INPUT_FORMAT:
/* Fall through */
	default:
		return 0;
	}
return 0;
}

/* compute the log2 of the downscale factor needed to get closest
 * to the requested viewfinder resolution on the upper side. The output cannot
 * be smaller than the requested viewfinder resolution.
 */
enum sh_css_err
sh_css_vf_downscale_log2(const struct sh_css_frame_info *out_info,
			 const struct sh_css_frame_info *vf_info,
			 unsigned int *downscale_log2)
{
	unsigned int ds_log2 = 0;
	unsigned int out_width = out_info ? out_info->width : 0;

	if (out_width == 0)
		return 0;

	/* downscale until width smaller than the viewfinder width. We don't
	 * test for the height since the vmem buffers only put restrictions on
	 * the width of a line, not on the number of lines in a frame.
	 */
	while (out_width >= vf_info->width) {
		ds_log2++;
		out_width /= 2;
	}
	/* now width is smaller, so we go up one step */
	if ((ds_log2 > 0) && (out_width < sh_css_max_vf_width()))
		ds_log2--;
	/* TODO: use actual max input resolution of vf_pp binary */
	if ((out_info->width >> ds_log2) >= 2*sh_css_max_vf_width())
		return sh_css_err_viewfinder_resolution_too_wide;
	
	/* currently the actual supported maximum input width for vf_pp is 
	   2*1280=2560 pixel, when the resolution is larger than this, we let
	   previous stage do extra downscaling and vf_pp do upscaling to get the
	   desired vf output resolution. In this case, the image quality is a bit
	   worse, but the customer requests a 1080p postview for 6M capture. The
	   image quality of postview is not that important, so we go for this 
	   easiest solution */
	while ((out_info->width >> ds_log2) > (2*SH_CSS_MAX_VF_WIDTH)) {
		ds_log2++;
	}

	*downscale_log2 = ds_log2;
	return sh_css_success;
}

/* ISP expects GRBG bayer order, we skip one line and/or one row
 * to correct in case the input bayer order is different.
 */
static unsigned int
lines_needed_for_bayer_order(struct sh_css_pipe *pipe)
{
	if (pipe->bayer_order == sh_css_bayer_order_bggr ||
	    pipe->bayer_order == sh_css_bayer_order_gbrg) {
		return 1;
	}
	return 0;
}

static unsigned int
columns_needed_for_bayer_order(struct sh_css_pipe *pipe)
{
	if (pipe->bayer_order == sh_css_bayer_order_rggb ||
	    pipe->bayer_order == sh_css_bayer_order_gbrg) {
		return 1;
	}
	return 0;
}

static enum sh_css_err
input_start_column(struct sh_css_pipe *pipe,
		   unsigned int bin_in,
		   unsigned int *start_column)
{
	unsigned int in = pipe->input_width,
	    for_bayer = columns_needed_for_bayer_order(pipe), start;

	if (bin_in + 2 * for_bayer > in)
		return sh_css_err_not_enough_input_columns;

	/* On the hardware, we want to use the middle of the input, so we
	 * divide the start column by 2. */
	start = (in - bin_in) / 2;
	/* in case the number of extra columns is 2 or odd, we round the start
	 * column down */
	start &= ~0x1;

	/* now we add the one column (if needed) to correct for the bayer
	 * order).
	 */
	start += for_bayer;
	*start_column = start;
	return sh_css_success;
}

static enum sh_css_err
input_start_line(struct sh_css_pipe *pipe,
		 unsigned int bin_in,
		 unsigned int *start_line)
{
	unsigned int in = pipe->input_height,
	    for_bayer = lines_needed_for_bayer_order(pipe), start;

	if (bin_in + 2 * for_bayer > in)
		return sh_css_err_not_enough_input_lines;

	/* On the hardware, we want to use the middle of the input, so we
	 * divide the start line by 2. On the simulator, we cannot handle extra
	 * lines at the end of the frame.
	 */
	start = (in - bin_in) / 2;
	/* in case the number of extra lines is 2 or odd, we round the start
	 * line down.
	 */
	start &= ~0x1;

	/* now we add the one line (if needed) to correct for the bayer order*/
	start += for_bayer;
	*start_line = start;
	return sh_css_success;
}

static enum sh_css_err
program_input_formatter(struct sh_css_pipe *pipe,
			struct sh_css_binary *binary,
			unsigned int left_padding)
{
	unsigned int start_line, start_column = 0,
		     cropped_height = binary->in_frame_info.height,
		     cropped_width  = binary->in_frame_info.width,
		     num_vectors,
		     buffer_height = 2,
		     buffer_width = binary->info->max_input_width,
		     two_ppc = pipe->two_ppc,
		     vmem_increment = 0,
		     deinterleaving = 0,
		     deinterleaving_b = 0,
		     width_a = 0,
		     width_b = 0,
		     bits_per_pixel,
		     vectors_per_buffer,
		     vectors_per_line = 0,
		     buffers_per_line = 0,
		     buf_offset_a = 0,
		     buf_offset_b = 0,
		     line_width = 0,
		     width_b_factor = 1,
		     start_column_b;
	input_formatter_cfg_t	if_a_config, if_b_config;
	enum sh_css_input_format input_format = binary->input_format;
	enum sh_css_err err = sh_css_success;

        bool input_is_raw = input_format_is_raw(input_format);

	if (pipe->input_needs_raw_binning &&
	    binary->info->enable.raw_binning) {
		cropped_width *= 2;
		cropped_width -= binary->info->left_cropping;
		cropped_height *= 2;
		cropped_height -= binary->info->left_cropping;
	}

	/* TODO: check to see if input is RAW and if current mode interprets
	 * RAW data in any particular bayer order. copy binary with output
	 * format other than raw should not result in dropping lines and/or
	 * columns.
	 */
	err = input_start_line(pipe, cropped_height, &start_line);
	if (err != sh_css_success)
		return err;
	err = input_start_column(pipe, cropped_width, &start_column);
		if (err != sh_css_success)
			return err;

	if (!left_padding)
		left_padding = binary->left_padding;
	if (left_padding) {
		num_vectors = CEIL_DIV(cropped_width + left_padding,
				       ISP_VEC_NELEMS);
	} else {
		num_vectors = CEIL_DIV(cropped_width, ISP_VEC_NELEMS);
		num_vectors *= buffer_height;
		/* todo: in case of left padding,
		   num_vectors is vectors per line,
		   otherwise vectors per line * buffer_height. */
	}

	start_column_b = start_column;

	bits_per_pixel = input_formatter_get_alignment(INPUT_FORMATTER0_ID)
		*8 / ISP_VEC_NELEMS;
	switch (input_format) {
	case SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY:
		if (two_ppc) {
			vmem_increment = 1;
			deinterleaving = 1;
			deinterleaving_b = 1;
			/* half lines */
			width_a = cropped_width * deinterleaving / 2;
			width_b_factor = 2;
			/* full lines */
			width_b = width_a * width_b_factor;
			buffer_width *= deinterleaving * 2;
			/* Patch from bayer to yuv */
			num_vectors *= deinterleaving;
			buf_offset_b = buffer_width / 2 / ISP_VEC_NELEMS;
			vectors_per_line = num_vectors / buffer_height;
			/* Even lines are half size */
			line_width = vectors_per_line *
				input_formatter_get_alignment(
				INPUT_FORMATTER0_ID) / 2;
			start_column /= 2;
		} else {
			vmem_increment = 1;
			deinterleaving = 3;
			width_a = cropped_width * deinterleaving / 2;
			buffer_width = buffer_width * deinterleaving / 2;
			/* Patch from bayer to yuv */
			num_vectors = num_vectors / 2 * deinterleaving;
			start_column = start_column * deinterleaving / 2;
		}
		break;
	case SH_CSS_INPUT_FORMAT_YUV420_8:
	case SH_CSS_INPUT_FORMAT_YUV420_10:
		if (two_ppc) {
			vmem_increment = 1;
			deinterleaving = 1;
			width_a = width_b = cropped_width * deinterleaving / 2;
			buffer_width *= deinterleaving * 2;
			num_vectors *= deinterleaving;
			buf_offset_b = buffer_width / 2 / ISP_VEC_NELEMS;
			vectors_per_line = num_vectors / buffer_height;
			/* Even lines are half size */
			line_width = vectors_per_line *
				input_formatter_get_alignment(
				INPUT_FORMATTER0_ID) / 2;
			start_column *= deinterleaving;
			start_column /= 2;
			start_column_b = start_column;
		} else {
			vmem_increment = 1;
			deinterleaving = 1;
			width_a = cropped_width * deinterleaving;
			buffer_width  *= deinterleaving * 2;
			num_vectors  *= deinterleaving;
			start_column *= deinterleaving;
		}
		break;
	case SH_CSS_INPUT_FORMAT_YUV422_8:
	case SH_CSS_INPUT_FORMAT_YUV422_10:
		if (two_ppc) {
			vmem_increment = 1;
			deinterleaving = 1;
			width_a = width_b = cropped_width * deinterleaving;
			buffer_width *= deinterleaving * 2;
			num_vectors  *= deinterleaving;
			start_column *= deinterleaving;
			buf_offset_b   = buffer_width / 2 / ISP_VEC_NELEMS;
			start_column_b = start_column;
		} else {
			vmem_increment = 1;
			deinterleaving = 2;
			width_a = cropped_width * deinterleaving;
			buffer_width *= deinterleaving;
			num_vectors  *= deinterleaving;
			start_column *= deinterleaving;
		}
		break;
	case SH_CSS_INPUT_FORMAT_RGB_444:
	case SH_CSS_INPUT_FORMAT_RGB_555:
	case SH_CSS_INPUT_FORMAT_RGB_565:
	case SH_CSS_INPUT_FORMAT_RGB_666:
	case SH_CSS_INPUT_FORMAT_RGB_888:
		num_vectors *= 2;
		if (two_ppc) {
			deinterleaving = 2;	/* BR in if_a, G in if_b */
			deinterleaving_b = 1;	/* BR in if_a, G in if_b */
			buffers_per_line = 4;
			start_column_b = start_column;
			start_column *= deinterleaving;
			start_column_b *= deinterleaving_b;
		} else {
			deinterleaving = 3;	/* BGR */
			buffers_per_line = 3;
			start_column *= deinterleaving;
		}
		vmem_increment = 1;
		width_a = cropped_width * deinterleaving;
		width_b = cropped_width * deinterleaving_b;
		buffer_width *= buffers_per_line;
		/* Patch from bayer to rgb */
		num_vectors = num_vectors / 2 * deinterleaving;
		buf_offset_b = buffer_width / 2 / ISP_VEC_NELEMS;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_6:
	case SH_CSS_INPUT_FORMAT_RAW_7:
	case SH_CSS_INPUT_FORMAT_RAW_8:
	case SH_CSS_INPUT_FORMAT_RAW_10:
	case SH_CSS_INPUT_FORMAT_RAW_12:
		if (two_ppc) {
			vmem_increment = 2;
			deinterleaving = 1;
			width_a = width_b = cropped_width / 2;
			//start_column /= 2;
			//start_column_b = start_column;
			buf_offset_b = 1;
		} else {
			vmem_increment = 1;
			deinterleaving = 2;
			if (my_css.continuous &&
			    binary->info->mode == SH_CSS_BINARY_MODE_COPY) {
				/* No deinterleaving for sp copy */
				deinterleaving = 1;
			}
			width_a = cropped_width;
			/* Must be multiple of deinterleaving */
			num_vectors = CEIL_MUL(num_vectors, deinterleaving);
		}
		buffer_height *= 2;
		if (my_css.continuous)
			buffer_height *= 2;
		vectors_per_line = CEIL_DIV(cropped_width, ISP_VEC_NELEMS);
		vectors_per_line = CEIL_MUL(vectors_per_line, deinterleaving);
		break;
	case SH_CSS_INPUT_FORMAT_RAW_14:
	case SH_CSS_INPUT_FORMAT_RAW_16:
		if (two_ppc) {
			num_vectors *= 2;
			vmem_increment = 1;
			deinterleaving = 2;
			width_a = width_b = cropped_width;
			/* B buffer is one line further */
			buf_offset_b = buffer_width / ISP_VEC_NELEMS;
			bits_per_pixel *= 2;
		} else {
			vmem_increment = 1;
			deinterleaving = 2;
			width_a = cropped_width;
			start_column /= deinterleaving;
		}
		buffer_height *= 2;
		break;
	case SH_CSS_INPUT_FORMAT_BINARY_8:
/* Fall through */
	case N_SH_CSS_INPUT_FORMAT:
		break;
	}
	if (width_a == 0)
		return sh_css_err_unsupported_input_mode;

	if (two_ppc)
		left_padding /= 2;

	/* Default values */
	if (left_padding)
		vectors_per_line = num_vectors;
	if (!vectors_per_line) {
		vectors_per_line = CEIL_MUL(num_vectors / buffer_height,
					    deinterleaving);
		line_width = 0;
	}
	if (!line_width)
		line_width = vectors_per_line *
		input_formatter_get_alignment(INPUT_FORMATTER0_ID);
#if 0
	/* Klocwork pacifier: VA_UNUSED.GEN */
	/* buffers_per_line is never used, this is SUSPICIOUS */
	/* NEEDS FURTHER INVESTIGATION */
	if (!buffers_per_line)
		buffers_per_line = deinterleaving;
#endif
	line_width = CEIL_MUL(line_width,
		input_formatter_get_alignment(INPUT_FORMATTER0_ID)
		* vmem_increment);

	vectors_per_buffer = buffer_height * buffer_width / ISP_VEC_NELEMS;
#if 0
	if (sh_css_continuous_is_enabled())
		vectors_per_buffer *= 2;
#endif

	if (pipe->input_mode == SH_CSS_INPUT_MODE_TPG &&
	    binary->info->mode == SH_CSS_BINARY_MODE_VIDEO) {
		/* workaround for TPG in video mode*/
		start_line = 0;
		start_column = 0;
		cropped_height -= start_line;
		width_a -= start_column;
	}

        /* When two_ppc is enabled, IF_A and IF_B gets seperate
         * bayer components. Therefore, it is not possible to
         * correct the bayer order to GRBG in horizontal direction
         * by shifting start_column.
         * Instead, IF_A and IF_B output (VMEM) addresses should be
         * swapped for this purpose (@Gokturk).
         */
	if (two_ppc && input_is_raw) {
		if (start_column%2 == 1) {
			/* Still correct for center of image. Just subtract 
			 * the part (which used to be correcting bayer order,
			 * now we do it by swapping the buffers) */
			start_column   = start_column - 1;
			
			/* Buffer start address swap from (0, buf_offset_b) ->
			 * (buf_offset_b, 0) */
			buf_offset_a = buf_offset_b;
			buf_offset_b = 0;
			/* Since each IF gets every two pixel in twoppc case, 
		 	* we need to halve the start_column per IF. */
			start_column /= 2;
			start_column_b = start_column;
			start_column += 1;
		} else {
			start_column /= 2;
			start_column_b = start_column;
		}

		
	}
	
	if_a_config.start_line = start_line;
	if_a_config.start_column = start_column;
	if_a_config.left_padding = left_padding / deinterleaving;
	if_a_config.cropped_height = cropped_height;
	if_a_config.cropped_width = width_a;
	if_a_config.deinterleaving = deinterleaving;
	if_a_config.buf_vecs = vectors_per_buffer;
	if_a_config.buf_start_index = buf_offset_a;
	if_a_config.buf_increment = vmem_increment;
	if_a_config.buf_eol_offset =
	    buffer_width * bits_per_pixel / 8 - line_width;
	if_a_config.is_yuv420_format =
		(input_format == SH_CSS_INPUT_FORMAT_YUV420_8)
		|| (input_format == SH_CSS_INPUT_FORMAT_YUV420_10);
	if_a_config.block_no_reqs =
		pipe->input_mode != SH_CSS_INPUT_MODE_SENSOR;

	if (two_ppc) {
		if (deinterleaving_b) {
			deinterleaving = deinterleaving_b;
			width_b = cropped_width * deinterleaving;
			buffer_width *= deinterleaving;
			/* Patch from bayer to rgb */
			num_vectors = num_vectors / 2 *
					deinterleaving * width_b_factor;
			vectors_per_line = num_vectors / buffer_height;
			line_width = vectors_per_line *
				input_formatter_get_alignment(
				INPUT_FORMATTER0_ID);
		}
		if_b_config.start_line = start_line;
		if_b_config.start_column = start_column_b;
		if_b_config.left_padding = left_padding / deinterleaving;
		if_b_config.cropped_height = cropped_height;
		if_b_config.cropped_width = width_b;
		if_b_config.deinterleaving = deinterleaving;
		if_b_config.buf_vecs = vectors_per_buffer;
		if_b_config.buf_start_index = buf_offset_b;
		if_b_config.buf_increment = vmem_increment;
		if_b_config.buf_eol_offset =
		    buffer_width * bits_per_pixel/8 - line_width;
		if_b_config.is_yuv420_format =
		    input_format == SH_CSS_INPUT_FORMAT_YUV420_8
		    || input_format == SH_CSS_INPUT_FORMAT_YUV420_10;
		if_b_config.block_no_reqs =
			pipe->input_mode != SH_CSS_INPUT_MODE_SENSOR;
		if (memcmp(&if_a_config, &my_css.curr_if_a_config,
			   sizeof(input_formatter_cfg_t)) ||
		    memcmp(&if_b_config, &my_css.curr_if_b_config,
			   sizeof(input_formatter_cfg_t))) {
			my_css.curr_if_a_config = if_a_config;
			my_css.curr_if_b_config = if_b_config;
			sh_css_sp_set_if_configs(&if_a_config, &if_b_config);
		}
	} else {
		if (memcmp(&if_a_config, &my_css.curr_if_a_config,
			   sizeof(input_formatter_cfg_t))) {
			my_css.curr_if_a_config = if_a_config;
			sh_css_sp_set_if_configs(&if_a_config, NULL);
		}
	}
	return sh_css_success;
}

static enum sh_css_err
sh_css_config_input_network(struct sh_css_pipe *pipe,
			    struct sh_css_binary *binary)
{
	unsigned int fmt_type;
	enum sh_css_err err = sh_css_success;

	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->pipeline.stages)
		binary = pipe->pipeline.stages->binary;

	err = sh_css_input_format_type(pipe->input_format,
				       pipe->mipi_config.comp,
				       &fmt_type);
	if (err != sh_css_success)
		return err;
	if (fmt_type != my_css.curr_fmt_type ||
	    pipe->ch_id != my_css.curr_ch_id ||
	    pipe->input_mode != my_css.curr_input_mode) {
		my_css.curr_fmt_type = fmt_type;
		my_css.curr_ch_id = pipe->ch_id;
		my_css.curr_input_mode = pipe->input_mode;
		sh_css_sp_program_input_circuit(fmt_type,
						pipe->ch_id,
						pipe->input_mode);
	}

	if (binary && (binary->online || my_css.continuous)) {
		if (my_css.continuous)
			my_css.start_sp_copy = true;
		err = program_input_formatter(pipe, binary, my_css.left_padding);
		if (err != sh_css_success)
			return err;
	}

	if (pipe->input_mode == SH_CSS_INPUT_MODE_TPG ||
	    pipe->input_mode == SH_CSS_INPUT_MODE_PRBS) {
		unsigned int hblank_cycles = 100,
			     vblank_lines = 6,
			     width,
			     height,
			     vblank_cycles;
		width  = pipe->input_width;
		height = pipe->input_height;
		vblank_cycles = vblank_lines * (width + hblank_cycles);
		sh_css_sp_configure_sync_gen(width, height, hblank_cycles,
					     vblank_cycles);
	}
	return sh_css_success;
}

#if WITH_PC_MONITORING
static struct task_struct *my_kthread;    /* Handle for the monitoring thread */
static int sh_binary_running;         /* Enable sampling in the thread */

static void print_pc_histo(char *core_name, struct sh_css_pc_histogram *hist)
{
	unsigned i;
	unsigned cnt_run = 0;
	unsigned cnt_stall = 0;
	sh_css_print("%s histogram length = %d\n", core_name, hist->length);
	sh_css_print("%s PC\trun\tstall\n", core_name);

	for (i = 0; i < hist->length; i++) {
		if ((hist->run[i] == 0) && (hist->run[i] == hist->stall[i]))
			continue;
		sh_css_print("%s %d\t%d\t%d\n",
				core_name, i, hist->run[i], hist->stall[i]);
		cnt_run += hist->run[i];
		cnt_stall += hist->stall[i];
	}

	sh_css_print(" Statistics for %s, cnt_run = %d, cnt_stall = %d, "
	       "hist->length = %d\n",
			core_name, cnt_run, cnt_stall, hist->length);
}

static void print_pc_histogram(void)
{
	struct sh_css_binary_metrics *metrics;

	for (metrics = sh_css_metrics.binary_metrics;
	     metrics;
	     metrics = metrics->next) {
		if (metrics->mode == SH_CSS_BINARY_MODE_PREVIEW ||
		    metrics->mode == SH_CSS_BINARY_MODE_VF_PP) {
			sh_css_print("pc_histogram for binary %d is SKIPPED\n",
				metrics->id);
			continue;
		}

		sh_css_print(" pc_histogram for binary %d\n", metrics->id);
		print_pc_histo("  ISP", &metrics->isp_histogram);
		print_pc_histo("  SP",   &metrics->sp_histogram);
		sh_css_print("print_pc_histogram() done for binay->id = %d, "
			     "done.\n", metrics->id);
	}

	sh_css_print("PC_MONITORING:print_pc_histogram() -- DONE\n");
}

static int pc_monitoring(void *data)
{
	int i = 0;

	while (true) {
		if (sh_binary_running) {
			sh_css_metrics_sample_pcs();
#if MULTIPLE_SAMPLES
			for (i = 0; i < NOF_SAMPLES; i++)
				sh_css_metrics_sample_pcs();
#endif
		}
		usleep_range(10, 50);
	}
	return 0;
}

static void spying_thread_create(void)
{
	my_kthread = kthread_run(pc_monitoring, NULL, "sh_pc_monitor");
	sh_css_metrics_enable_pc_histogram(1);
}

static void input_frame_info(struct sh_css_frame_info frame_info)
{
	sh_css_print("SH_CSS:input_frame_info() -- frame->info.width = %d, "
	       "frame->info.height = %d, format = %d\n",
			frame_info.width, frame_info.height, frame_info.format);
}
#endif /* WITH_PC_MONITORING */

static void
start_binary(struct sh_css_pipe *pipe,
	     struct sh_css_binary *binary)
{
	if (my_css.reconfigure_css_rx)
		sh_css_rx_disable();

	sh_css_metrics_start_binary(&binary->metrics);

#if WITH_PC_MONITORING
	sh_css_print("PC_MONITORING: %s() -- binary id = %d , "
		     "enable_dvs_envelope = %d\n",
		     __func__, binary->info->id,
		     binary->info->enable.dvs_envelope);
	input_frame_info(binary->in_frame_info);

	if (binary->info->mode == SH_CSS_BINARY_MODE_VIDEO)
		sh_binary_running = true;
#endif

	my_css.curr_state = sh_css_state_executing_isp;

	sh_css_sp_start_isp();

	if (my_css.reconfigure_css_rx) {
		pipe->mipi_config.is_two_ppc = pipe->two_ppc;
		sh_css_rx_configure(&pipe->mipi_config);
		my_css.reconfigure_css_rx = false;
	}
}

void
sh_css_frame_zero(struct sh_css_frame *frame)
{
	mmgr_clear(frame->data, frame->data_bytes);
}

/* start the copy function on the SP */
static enum sh_css_err
start_copy_on_sp(struct sh_css_pipe *pipe,
		 struct sh_css_binary *binary,
		 struct sh_css_frame *out_frame)
{
	if (my_css.reconfigure_css_rx)
		sh_css_rx_disable();

	if (pipe->input_format == SH_CSS_INPUT_FORMAT_BINARY_8)
		sh_css_sp_start_binary_copy(out_frame, pipe->two_ppc);
	else
		sh_css_sp_start_raw_copy(binary, out_frame, pipe->two_ppc,
					 pipe->input_needs_raw_binning,
					SH_CSS_PIPE_CONFIG_OVRD_THRD_2);

	sh_css_sp_start_isp();

	if (my_css.reconfigure_css_rx) {
		/* do we need to wait for the IF do be ready? */
		pipe->mipi_config.is_two_ppc = pipe->two_ppc;
		sh_css_rx_configure(&pipe->mipi_config);
		my_css.reconfigure_css_rx = false;
	}

	return sh_css_success;
}

/* Pipeline:
 * To organize the several different binaries for each type of mode,
 * we use a pipeline. A pipeline contains a number of stages, each with
 * their own binary and frame pointers.
 * When stages are added to a pipeline, output frames that are not passed
 * from outside are automatically allocated.
 * When input frames are not passed from outside, each stage will use the
 * output frame of the previous stage as input (the full resolution output,
 * not the viewfinder output).
 * Pipelines must be cleaned and re-created when settings of the binaries
 * change.
 */
static void
sh_css_pipeline_stage_destroy(struct sh_css_pipeline_stage *me)
{
	if (me->out_frame_allocated)
		sh_css_frame_free(me->args.out_frame);
	if (me->vf_frame_allocated)
		sh_css_frame_free(me->args.out_vf_frame);
	sh_css_free(me);
}

static void
sh_css_binary_args_reset(struct sh_css_binary_args *args)
{
	args->in_frame      = NULL;
	args->out_frame     = NULL;
	args->in_ref_frame  = NULL;
	args->out_ref_frame = NULL;
	args->in_tnr_frame  = NULL;
	args->out_tnr_frame = NULL;
	args->extra_frame   = NULL;
	args->out_vf_frame  = NULL;
	args->copy_vf       = false;
	args->copy_output   = true;
	args->vf_downscale_log2 = 0;
}

static enum sh_css_err
sh_css_pipeline_stage_create(struct sh_css_pipeline_stage **me,
			     struct sh_css_binary *binary,
			     const struct sh_css_fw_info *firmware,
			     int    mode,
			     struct sh_css_frame *cc_frame,
			     struct sh_css_frame *in_frame,
			     struct sh_css_frame *out_frame,
			     struct sh_css_frame *vf_frame)
{
	struct sh_css_pipeline_stage *stage = sh_css_malloc(sizeof(*stage));
	if (!stage)
		return sh_css_err_cannot_allocate_memory;
	stage->binary = firmware ? NULL : binary;
	stage->binary_info = firmware ?
			     (struct sh_css_binary_info *)
				&firmware->info.isp
			     : (struct sh_css_binary_info *)binary->info;
	stage->firmware = firmware;
	stage->mode = mode;
	stage->out_frame_allocated = false;
	stage->vf_frame_allocated = false;
	stage->irq_buf_flags = 0x0;
	stage->next = NULL;
	sh_css_binary_args_reset(&stage->args);

	if (!in_frame && !firmware && !binary->online)
		return sh_css_err_internal_error;

	if (!out_frame && binary && binary->out_frame_info.width) {
		enum sh_css_err ret =
		    sh_css_frame_allocate_from_info(&out_frame,
						    &binary->out_frame_info);
		if (ret != sh_css_success) {
			sh_css_free(stage);
			return ret;
		}
		stage->out_frame_allocated = true;
	}
	/* VF frame is not needed in case of need_pp
	   However, the capture binary needs a vf frame to write to.
	*/
	if (!vf_frame) {
		if ((binary && binary->vf_frame_info.width) ||
		    (firmware &&
		     firmware->info.isp.enable.vf_veceven)
		    ) {
			enum sh_css_err ret =
			    sh_css_frame_allocate_from_info(&vf_frame,
						    &binary->vf_frame_info);
			if (ret != sh_css_success) {
				if (stage->out_frame_allocated)
					sh_css_frame_free(out_frame);
				sh_css_free(stage);
				return ret;
			}
			stage->vf_frame_allocated = true;
		}
	} else if (vf_frame && binary && binary->vf_frame_info.width)
		stage->vf_frame_allocated = true;

	stage->args.cc_frame = cc_frame;
	stage->args.in_frame = in_frame;
	stage->args.out_frame = out_frame;
	stage->args.out_vf_frame = vf_frame;
	*me = stage;
	return sh_css_success;
}

static void
sh_css_pipeline_init(struct sh_css_pipeline *me, enum sh_css_pipe_id pipe_id)
{
	struct sh_css_frame init_frame = {
		.dynamic_data_index = SH_CSS_INVALID_FRAME_ID };

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipeline_init() enter:\n");

	assert(me != NULL);
	if (me == NULL)
		return;

	me->pipe_id = pipe_id;
	me->stages = NULL;
	me->reload = true;
	me->current_stage = NULL;
	me->in_frame = init_frame;
	me->out_frame = init_frame;
	me->vf_frame = init_frame;
}

/** @brief Add a stage to pipeline.
 *
 * @param	me	Pointer to the pipeline to be added to.
 * @param[in]	binary		ISP binary of new stage.
 * @param[in]	firmware	ISP firmware of new stage.
 * @param[in]	mode		ISP mode of new stage.
 * @param[in]	cc_frame		The cc frame to the stage.
 * @param[in]	in_frame		The input frame to the stage.
 * @param[in]	out_frame		The output frame of the stage.
 * @param[in]	vf_frame		The viewfinder frame of the stage.
 * @param[in]	stage			The successor of the stage.
 * @return			IA_CSS_SUCCESS or error code upon error.
 *
 * Add a new stage to a non-NULL pipeline.
 * The stage consists of an ISP binary or firmware and input and output arguments.
*/
static enum sh_css_err
sh_css_pipeline_add_stage(struct sh_css_pipeline *me,
			  struct sh_css_binary *binary,
			  const struct sh_css_fw_info *firmware,
			  unsigned int mode,
			  struct sh_css_frame *cc_frame,
			  struct sh_css_frame *in_frame,
			  struct sh_css_frame *out_frame,
			  struct sh_css_frame *vf_frame,
			  struct sh_css_pipeline_stage **stage)
{
	struct sh_css_pipeline_stage *last, *new_stage = NULL;
	enum sh_css_err err;


	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipeline_add_stage() enter:\n");

	/* other arguments can be NULL */
	assert(me != NULL);
	if (me == NULL)
		return sh_css_err_internal_error;

	last = me->stages;

	if (!binary && !firmware)
		return sh_css_err_internal_error;

	while (last && last->next)
		last = last->next;

	/* if in_frame is not set, we use the out_frame from the previous
	 * stage, if no previous stage, it's an error.
	 */
	if (!in_frame && !firmware && !binary->online) {
		if (last)
			in_frame = last->args.out_frame;
		if (!in_frame)
			return sh_css_err_internal_error;
	}
	err = sh_css_pipeline_stage_create(&new_stage, binary, firmware,
					   mode, cc_frame,
					   in_frame, out_frame, vf_frame);
	if (err != sh_css_success)
		return err;
	if (last)
		last->next = new_stage;
	else
		me->stages = new_stage;
	if (stage)
		*stage = new_stage;
	return sh_css_success;
}

static enum sh_css_err
sh_css_pipeline_get_stage(struct sh_css_pipeline *me,
			  int mode,
			  struct sh_css_pipeline_stage **stage)
{
	struct sh_css_pipeline_stage *s;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipeline_get_stage() enter:\n");

	assert(me != NULL);
	if (me == NULL)
		return sh_css_err_internal_error;

	assert(stage != NULL);
	if (stage == NULL)
		return sh_css_err_internal_error;

	for (s = me->stages; s; s = s->next) {
		if (s->mode == mode) {
			*stage = s;
			return sh_css_success;
		}
	}
	return sh_css_err_internal_error;
}

static enum sh_css_err
sh_css_pipeline_get_output_stage(struct sh_css_pipeline *me,
				 int mode,
				 struct sh_css_pipeline_stage **stage)
{
	struct sh_css_pipeline_stage *s;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipeline_get_output_stage() enter:\n");

	assert(me != NULL);
	if (me == NULL)
		return sh_css_err_internal_error;

	assert(stage != NULL);
	if (stage == NULL)
		return sh_css_err_internal_error;

	*stage = NULL;
	/* First find acceleration firmware at end of pipe */
	for (s = me->stages; s; s = s->next) {
		if (s->firmware && s->mode == mode &&
		    s->firmware->info.isp.enable.output)
			*stage = s;
	}
	if (*stage)
		return sh_css_success;
	/* If no firmware, find binary in pipe */
	return sh_css_pipeline_get_stage(me, mode, stage);
}

static void
sh_css_pipeline_restart(struct sh_css_pipeline *me)
{

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipeline_restart() enter:\n");

	assert(me != NULL);
	if (me == NULL)
		return;

	me->current_stage = NULL;
}

static void
sh_css_pipeline_clean(struct sh_css_pipeline *me)
{
	struct sh_css_pipeline_stage *s;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipeline_clean() enter:\n");

	assert(me != NULL);
	if (me == NULL)
		return;

	s = me->stages;

	while (s) {
		struct sh_css_pipeline_stage *next = s->next;
		sh_css_pipeline_stage_destroy(s);
		s = next;
	}
	sh_css_pipeline_init(me, me->pipe_id);
}

static void
sh_css_pipe_start(struct sh_css_pipe *pipe)
{
	struct sh_css_pipeline_stage *stage;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipe_start() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	stage = pipe->pipeline.stages;

	if (stage == NULL)
		return;

	pipe->pipeline.current_stage = stage;

	start_binary(pipe, stage->binary);
}

static void start_pipe(
	struct sh_css_pipe *me,
	enum sh_css_pipe_config_override copy_ovrd)
{
	bool low_light;
	bool is_preview;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"start_pipe() enter:\n");

	assert(me != NULL);
	if (me == NULL)
		return;

	low_light = me->mode == SH_CSS_CAPTURE_PIPELINE &&
			 (me->capture_mode == SH_CSS_CAPTURE_MODE_LOW_LIGHT ||
			  me->capture_mode == SH_CSS_CAPTURE_MODE_BAYER);
	is_preview = me->mode == SH_CSS_PREVIEW_PIPELINE;

	sh_css_sp_init_pipeline(&me->pipeline,
				me->mode,
				is_preview,
				low_light,
				me->xnr,
				me->two_ppc,
				my_css.continuous,
				false,
				me->input_needs_raw_binning,
			copy_ovrd);

	/* prepare update of params to ddr */
	sh_css_commit_isp_config(&me->pipeline, false);

	sh_css_pipe_start(me);
}

static void
sh_css_set_irq_buffer(struct sh_css_pipeline_stage *stage,
			enum sh_css_frame_id frame_id,
			struct sh_css_frame *frame)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_set_irq_buffer() enter:\n");
	if (stage && frame)
		stage->irq_buf_flags |= 1<<frame_id;
}

void sh_css_frame_info_set_width(
	struct sh_css_frame_info *info,
	unsigned int width, unsigned int min_padded_width)
{
	unsigned int align = width < min_padded_width ? min_padded_width : width;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_info_set_width() enter: "
		"width=%d, format=%d, align=%d\n", width, info->format, align);

	assert(info != NULL);
	if (info == NULL)
		return;

	info->width = width;
	/* frames with a U and V plane of 8 bits per pixel need to have
	   all planes aligned, this means double the alignment for the
	   Y plane if the horizontal decimation is 2. */
	if (info->format == SH_CSS_FRAME_FORMAT_YUV420 ||
	    info->format == SH_CSS_FRAME_FORMAT_YV12)
		info->padded_width = CEIL_MUL(align, 2*HIVE_ISP_DDR_WORD_BYTES);
	else if (info->format == SH_CSS_FRAME_FORMAT_YUV_LINE)
		info->padded_width = CEIL_MUL(align, 2*ISP_VEC_NELEMS);
	else if (info->format == SH_CSS_FRAME_FORMAT_RAW)
		info->padded_width = CEIL_MUL(align, 2*ISP_VEC_NELEMS);
	else
		info->padded_width = CEIL_MUL(align, HIVE_ISP_DDR_WORD_BYTES);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_info_set_width() leave: "
		"padded_width=%d\n",
		info->padded_width);
}

static void sh_css_frame_info_set_format(
	struct sh_css_frame_info *info,
	enum sh_css_frame_format format)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_frame_info_set_format() enter:\n");

	assert(info != NULL);
	if (info == NULL)
		return;

	/* yuv_line has 2*NWAY alignment */
	info->format = format;
	/* HACK: this resets the padded width incorrectly.
	   Lex needs to fix this in the vf_veceven module. */
	info->padded_width =  CEIL_MUL(info->padded_width, 2*ISP_VEC_NELEMS);
}

void sh_css_frame_info_init(
	struct sh_css_frame_info *info, unsigned int width, unsigned int height,
	unsigned int min_padded_width, enum sh_css_frame_format format)
{
	sh_css_dtrace(SH_DBG_TRACE,
		      "sh_css_frame_info_init() enter: width=%d, min_padded_width=%d, height=%d, format=%d\n",
		      width, min_padded_width, height, format);

	assert(info != NULL);
	if (info == NULL)
		return;

	info->height = height;
	info->format = format;
	sh_css_frame_info_set_width(info, width, min_padded_width);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_info_init() leave: return_void\n");
}

static void invalidate_video_binaries(
	struct sh_css_pipe *pipe)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"invalidate_video_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	pipe->pipeline.reload   = true;
	pipe->pipe.video.copy_binary.info = NULL;
	pipe->pipe.video.video_binary.info = NULL;
	pipe->pipe.video.vf_pp_binary.info = NULL;
	if (pipe->shading_table) {
		sh_css_shading_table_free(pipe->shading_table);
		pipe->shading_table = NULL;
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"invalidate_video_binaries() leave:\n");
}

void sh_css_set_shading_table(
	const struct sh_css_shading_table *table)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_shading_table() enter: "
		"table=%p\n",table);
#if 0
	if (table != my_css.shading_table)
		reset_mode_shading_tables();

	my_css.shading_table = table;
#endif
	if (sh_css_params_set_shading_table(table)) {
		sh_css_pipe_free_shading_table(&my_css.preview_pipe);
		sh_css_pipe_free_shading_table(&my_css.video_pipe);
		sh_css_pipe_free_shading_table(&my_css.capture_pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_set_shading_table() leave: return_void\n");
}

/* CSS receiver programming */
enum sh_css_err sh_css_pipe_configure_input_port(
	struct sh_css_pipe	*pipe,
	const mipi_port_ID_t	port,
	const unsigned int		num_lanes,
	const unsigned int		timeout)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() enter: "
		"port=%d, "
		"num_lanes=%d, "
		"timeout=%d\n",
		port, num_lanes,
		timeout);

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(port < N_MIPI_PORT_ID);
	if (port >= N_MIPI_PORT_ID)
		return sh_css_err_internal_error;

	if (num_lanes > MIPI_PORT_MAXLANES[port]) {
		return sh_css_err_conflicting_mipi_settings;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_err_conflicting_mipi_settings);
	}
	if (num_lanes > MIPI_4LANE_CFG) {
		return sh_css_err_conflicting_mipi_settings;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_err_conflicting_mipi_settings);
	}

	pipe->mipi_config.port = port;
#if defined(HAS_RX_VERSION_1)
	pipe->mipi_config.num_lanes = num_lanes;
#endif
	pipe->mipi_config.timeout = timeout;
	my_css.reconfigure_css_rx = true;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_success);
return sh_css_success;
}

enum sh_css_err sh_css_pipe_set_compression(
	struct sh_css_pipe    *pipe,
	const mipi_predictor_t	comp,
	const unsigned int compressed_bits_per_pixel,
	const unsigned int uncompressed_bits_per_pixel)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() enter: "
		"comp=%d, "
		"compressed_bits_per_pixel=%d, "
		"uncompressed_bits_per_pixel=%d\n",
		comp, compressed_bits_per_pixel,
		uncompressed_bits_per_pixel);

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(comp < N_MIPI_PREDICTOR_TYPES);
	if (comp >= N_MIPI_PREDICTOR_TYPES)
		return sh_css_err_internal_error;

	if (comp == MIPI_PREDICTOR_NONE) {
		if (compressed_bits_per_pixel || uncompressed_bits_per_pixel) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_err_conflicting_mipi_settings);
			return sh_css_err_conflicting_mipi_settings;
		}
	} else {
		if (compressed_bits_per_pixel < 6 ||
		    compressed_bits_per_pixel > 8) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_err_conflicting_mipi_settings);
			return sh_css_err_conflicting_mipi_settings;
		}
		if (uncompressed_bits_per_pixel != 10 &&
		    uncompressed_bits_per_pixel != 12) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_err_conflicting_mipi_settings);
			return sh_css_err_conflicting_mipi_settings;
		}
	}
	pipe->mipi_config.comp = comp;
#if defined(HAS_RX_VERSION_1)
	pipe->mipi_config.comp_bpp = compressed_bits_per_pixel;
	pipe->mipi_config.uncomp_bpp = uncompressed_bits_per_pixel;
#endif
	my_css.reconfigure_css_rx = true;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_compression() leave: return_err=%d", sh_css_success);

return sh_css_success;
}

void sh_css_tpg_configure(
	unsigned int x_mask,
	int x_delta,
	unsigned int y_mask,
	int y_delta,
	unsigned int xy_mask)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_tpg_configure() enter: "
		"x_mask=%d, y_mask=%d, xy_mask=%d, "
		"x_delta=%d, y_delta=%d\n",
		x_mask, y_mask, xy_mask,
		x_delta, y_delta);
	sh_css_sp_configure_tpg(x_mask, y_mask, x_delta, y_delta, xy_mask);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_tpg_configure() leave: return_void\n");
}

void sh_css_prbs_set_seed(
	int seed)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_prbs_set_seed() enter: "
		"seed=%d\n",seed);
	sh_css_sp_configure_prbs(seed);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_prbs_set_seed() leave: return_void\n");
}

/* currently, the capture pp binary requires an internal frame. This will
   be removed in the future. */
static enum sh_css_err alloc_capture_pp_frame(
	struct sh_css_pipe *pipe,
	const struct sh_css_binary *binary)
{
	struct sh_css_frame_info cpp_info;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "alloc_capture_pp_frame() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(binary != NULL);
	if (binary == NULL)
		return sh_css_err_internal_error;

	cpp_info = binary->internal_frame_info;
	cpp_info.format = SH_CSS_FRAME_FORMAT_YUV420;
	if (pipe->pipe.capture.capture_pp_frame)
		sh_css_frame_free(pipe->pipe.capture.capture_pp_frame);
	err = sh_css_frame_allocate_from_info(
			&pipe->pipe.capture.capture_pp_frame, &cpp_info);
	return err;
}

static void invalidate_preview_binaries(
	struct sh_css_pipe *pipe)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "invalidate_preview_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	pipe->pipeline.reload     = true;
	pipe->pipe.preview.preview_binary.info = NULL;
	pipe->pipe.preview.vf_pp_binary.info   = NULL;
	pipe->pipe.preview.copy_binary.info    = NULL;
	if (pipe->shading_table) {
		sh_css_shading_table_free(pipe->shading_table);
		pipe->shading_table = NULL;
	}
}

static void invalidate_capture_binaries(
	struct sh_css_pipe *pipe)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "invalidate_capture_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	pipe->pipeline.reload        = true;
	pipe->pipe.capture.copy_binary.info       = NULL;
	pipe->pipe.capture.primary_binary.info    = NULL;
	pipe->pipe.capture.pre_isp_binary.info    = NULL;
	pipe->pipe.capture.gdc_binary.info        = NULL;
	pipe->pipe.capture.post_isp_binary.info   = NULL;
	pipe->pipe.capture.anr_binary.info        = NULL;
	pipe->pipe.capture.capture_pp_binary.info = NULL;
	pipe->pipe.capture.vf_pp_binary.info      = NULL;
	if (pipe->shading_table) {
		sh_css_shading_table_free(pipe->shading_table);
		pipe->shading_table = NULL;
	}
}

static void sh_css_pipe_invalidate_binaries(
	struct sh_css_pipe *pipe)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_invalidate_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	switch (pipe->mode) {
	case SH_CSS_VIDEO_PIPELINE:
		invalidate_video_binaries(pipe);
		break;
	case SH_CSS_CAPTURE_PIPELINE:
		invalidate_capture_binaries(pipe);
		break;
	case SH_CSS_PREVIEW_PIPELINE:
		invalidate_preview_binaries(pipe);
		break;
	default:
		break;
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_invalidate_binaries() leave:\n");
}

static void
enable_interrupts(void)
{
/* Select whether the top IRQ delivers a level signal or not. In CSS 2.0 this choice is on the interface */
	bool enable_pulse = false;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "enable_interrupts() enter:\n");
/* Enable IRQ on the SP which signals that SP goes to idle (aka ready state) */
	cnd_sp_irq_enable(SP0_ID, true);
/* Set the IRQ device 0 to either level or pulse */
	irq_enable_pulse(IRQ0_ID, enable_pulse);
	virq_enable_channel(virq_sp, true);
	/* Triggered by SP to signal Host that there are new statistics */
	virq_enable_channel((virq_id_t)(IRQ_SW_CHANNEL1_ID + IRQ_SW_CHANNEL_OFFSET), true);
	/* Triggered by SP to signal Host that there is data in one of the
	 * SP->Host queues.*/
#if !defined(HAS_IRQ_MAP_VERSION_2)
/* IRQ_SW_CHANNEL2_ID does not exist on 240x systems */
	virq_enable_channel((virq_id_t)(IRQ_SW_CHANNEL2_ID + IRQ_SW_CHANNEL_OFFSET), true);
	virq_clear_all();
#endif

	sh_css_rx_enable_all_interrupts();

#if defined(HRT_CSIM)
/*
 * Enable IRQ on the SP which signals that SP goes to idle
 * to get statistics for each binary
 */
	cnd_isp_irq_enable(ISP0_ID, true);
	virq_enable_channel(virq_isp, true);
#endif
}

static const struct sh_css_env default_env = {
  .sh_env	  = { NULL, NULL, NULL },
  .print_env	  = { NULL, NULL }
};

struct sh_css_env
sh_css_default_env(void)
{
	struct sh_css_env env = default_env;
	return env;
}

enum sh_css_err sh_css_init(
	const struct sh_css_env *env,
	const char			*fw_data,
	const unsigned int	fw_size)
{
	enum sh_css_err err;
	void *(*malloc_func) (size_t size, bool zero_mem) = env->sh_env.alloc;
	void (*free_func) (void *ptr) = env->sh_env.free;
	void (*flush_func) (struct sh_css_acc_fw *fw) = env->sh_env.flush;

	static struct sh_css default_css = DEFAULT_CSS;
	static struct sh_css_preview_settings preview = DEFAULT_PREVIEW_SETTINGS;
	static struct sh_css_capture_settings capture = DEFAULT_CAPTURE_SETTINGS;
	static struct sh_css_video_settings   video   = DEFAULT_VIDEO_SETTINGS;

	hrt_data select = gpio_reg_load(GPIO0_ID, _gpio_block_reg_do_select)
						& (~GPIO_FLASH_PIN_MASK);
	hrt_data enable = gpio_reg_load(GPIO0_ID, _gpio_block_reg_do_e)
							| GPIO_FLASH_PIN_MASK;

	default_css.preview_pipe.pipe.preview = preview;
	default_css.capture_pipe.pipe.capture = capture;
	default_css.video_pipe.pipe.video     = video;

	if (malloc_func == NULL || free_func == NULL)
		return sh_css_err_invalid_arguments;

	memcpy(&my_css, &default_css, sizeof(my_css));

	my_css.malloc = malloc_func;
	my_css.free = free_func;
	my_css.flush = flush_func;
	/* Only after next line we can do dtrace */
	sh_css_printf = env->print_env.debug_print;

	sh_css_set_dtrace_level(SH_DBG_INFO);
	sh_css_set_stop_timeout(CSS_TIMEOUT_US);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_init() enter: env=%p, fw_data=%p, fw_size=%d\n",
		env, fw_data, fw_size);

	ia_css_i_host_rmgr_init();

	/* In case this has been programmed already, update internal
	   data structure ... DEPRECATED */
	my_css.page_table_base_index =
		sh_css_mmu_get_page_table_base_index();

	enable_interrupts();

	/* configure GPIO to output mode */
	gpio_reg_store(GPIO0_ID, _gpio_block_reg_do_select, select);
	gpio_reg_store(GPIO0_ID, _gpio_block_reg_do_e, enable);
	gpio_reg_store(GPIO0_ID, _gpio_block_reg_do_0, 0);

	err = sh_css_refcount_init();
	if (err != sh_css_success)
		return err;
	err = sh_css_params_init();
	if (err != sh_css_success)
		return err;
	err = sh_css_sp_init();
	if (err != sh_css_success)
		return err;
	err = sh_css_load_firmware(fw_data, fw_size);
	if (err != sh_css_success)
		return err;
	sh_css_init_binary_infos();
	my_css.sp_bin_addr = sh_css_sp_load_program(&sh_css_sp_fw,
						    SP_PROG_NAME,
						    my_css.sp_bin_addr);
	if (!my_css.sp_bin_addr) {
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_init() leave: return_err=%d\n",sh_css_err_cannot_allocate_memory);
		return sh_css_err_cannot_allocate_memory;
	}
	sh_css_pipeline_init(&my_css.preview_pipe.pipeline,
			     SH_CSS_PREVIEW_PIPELINE);
	sh_css_pipeline_init(&my_css.video_pipe.pipeline,
			     SH_CSS_VIDEO_PIPELINE);
	sh_css_pipeline_init(&my_css.capture_pipe.pipeline,
			     SH_CSS_CAPTURE_PIPELINE);

#if defined(HRT_CSIM)
	/**
	 * In compiled simulator context include debug support by default.
	 * In all other cases (e.g. Android phone), the user (e.g. driver)
	 * must explicitly enable debug support by calling this function.
	 */
	if (!sh_css_debug_mode_init()) {
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_init() leave: return_err=%d\n",sh_css_err_internal_error);
		return sh_css_err_internal_error;
	}
#endif

#if WITH_PC_MONITORING
	if (!thread_alive) {
		thread_alive++;
		sh_css_print("PC_MONITORING: %s() -- create thread DISABLED\n",
			     __func__);
		spying_thread_create();
	}
	sh_css_printf = printk;
#endif
	if (!sh_css_hrt_system_is_idle()) {
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_init() leave: return_err=%d\n",sh_css_err_system_not_idle);
		return sh_css_err_system_not_idle;
	}
	/* can be called here, queuing works, but:
	   - when sp is started later, it will wipe queued items
	   so for now we leave it for later and make sure
	   updates are not called to frequently.
	sh_css_init_buffer_queues();
	*/

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_init() leave: return_err=%d\n",err);

	return err;
}

/* Suspend does not need to do anything for now, this may change
   in the future though. */
void
sh_css_suspend(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_suspend() enter & leave\n");
}

void
sh_css_resume(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_resume() enter: void\n");
	/* trigger reconfiguration of necessary hardware */
	my_css.reconfigure_css_rx = true;
	my_css.curr_if_a_config.cropped_width  = 0;
	my_css.curr_if_a_config.cropped_height = 0;
	my_css.curr_if_b_config.cropped_width  = 0;
	my_css.curr_if_b_config.cropped_height = 0;
	my_css.curr_fmt_type = -1;

	sh_css_sp_set_sp_running(false);
	/* reload the SP binary. ISP binaries are automatically
	   reloaded by the ISP upon execution. */
	mmu_set_page_table_base_index(MMU0_ID,
			my_css.page_table_base_index);
	sh_css_params_reconfigure_gdc_lut();

	sh_css_sp_activate_program(&sh_css_sp_fw, my_css.sp_bin_addr,
				   SP_PROG_NAME);

	enable_interrupts();
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_resume() leave: return=void\n");
}

void *
sh_css_malloc(size_t size)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_malloc() enter: size=%d\n", size);
	if (size > 0 && my_css.malloc) {
		void *p = my_css.malloc(size, false);
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_malloc() leave: "
			"return=%p\n", p);
		return p;
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_malloc() leave: return=NULL\n");
	return NULL;
}

void
sh_css_free(void *ptr)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_free() enter: ptr=%p\n", ptr);
	if (ptr && my_css.free)
		my_css.free(ptr);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_free() leave: return=void\n");
}

/* For Acceleration API: Flush FW (shared buffer pointer) arguments */
void
sh_css_flush(struct sh_css_acc_fw *fw)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_flush() enter: fw=%p\n", fw);
	if ((fw != NULL) && (my_css.flush != NULL))
		my_css.flush(fw);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_flush() leave: return=void\n");
}

void
sh_css_uninit(void)
{
	int i;
	struct sh_css_pipe *preview_pipe = &my_css.preview_pipe;
	struct sh_css_pipe *video_pipe   = &my_css.video_pipe;
	struct sh_css_pipe *capture_pipe = &my_css.capture_pipe;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_uninit() enter: void\n");
#if WITH_PC_MONITORING
	sh_css_print("PC_MONITORING: %s() -- started\n", __func__);
	print_pc_histogram();
#endif

	/* cleanup generic data */
	sh_css_params_uninit();
	sh_css_pipeline_stream_clear_pipelines();
	sh_css_refcount_uninit();

	ia_css_i_host_rmgr_uninit();

	sh_css_binary_uninit();
	sh_css_sp_uninit();
	sh_css_unload_firmware();
	if (my_css.sp_bin_addr) {
		mmgr_free(my_css.sp_bin_addr);
		my_css.sp_bin_addr = mmgr_NULL;
	}

	/* cleanup preview data */
	sh_css_pipe_invalidate_binaries(preview_pipe);
	sh_css_pipeline_clean(&preview_pipe->pipeline);
	for (i = 0; i < NUM_CONTINUOUS_FRAMES; i++) {
		if (preview_pipe->pipe.preview.continuous_frames[i]) {
			sh_css_frame_free(
				preview_pipe->pipe.preview.continuous_frames[i]);
			preview_pipe->pipe.preview.continuous_frames[i] = NULL;
		}
	}

	/* cleanup video data */
	sh_css_pipe_invalidate_binaries(video_pipe);
	sh_css_pipeline_clean(&video_pipe->pipeline);
	for (i = 0; i < NUM_TNR_FRAMES; i++) {
		if (video_pipe->pipe.video.tnr_frames[i])
			sh_css_frame_free(video_pipe->pipe.video.tnr_frames[i]);
		video_pipe->pipe.video.tnr_frames[i] = NULL;
	}
	for (i = 0; i < NUM_REF_FRAMES; i++) {
		if (video_pipe->pipe.video.ref_frames[i])
			sh_css_frame_free(video_pipe->pipe.video.ref_frames[i]);
		video_pipe->pipe.video.ref_frames[i] = NULL;
	}

	/* cleanup capture data */
	sh_css_pipe_invalidate_binaries(capture_pipe);
	sh_css_pipeline_clean(&capture_pipe->pipeline);
	if (capture_pipe->pipe.capture.capture_pp_frame) {
		sh_css_frame_free(capture_pipe->pipe.capture.capture_pp_frame);
		capture_pipe->pipe.capture.capture_pp_frame = NULL;
	}
#if 0
	for (i = 0; i < NUM_CONTINUOUS_FRAMES; i++) {
		if (capture_pipe->pipe.capture.continuous_frames[i]) {
			sh_css_frame_free(
				capture_pipe->pipe.capture.continuous_frames[i]);
			capture_pipe->pipe.capture.continuous_frames[i] = NULL;
		}
	}
#endif


	sh_css_sp_set_sp_running(false);
	sh_css_sp_reset_global_vars();
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_uninit() leave: return=void\n");
}

static bool sh_css_frame_ready(void)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_frame_ready() enter:\n");
	if (my_css.curr_state == sh_css_state_executing_sp_bin_copy) {
		my_css.capture_pipe.pipe.capture.output_frame->planes.binary.size =
			sh_css_sp_get_binary_copy_size();
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_frame_ready() leave: return=true\n");
		return true;
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_frame_ready() leave: return=false\n");
	return false;
}

static unsigned int translate_sw_interrupt(unsigned value)
{
	enum sh_css_pipe_id pipe_id = value >> 24;
	unsigned stage_num;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "translate_sw_interrupt() enter:\n");
	value &= ~(0xff<<24);
	stage_num = value >> 16;
	value &= ~(0xff<<16);
	(void)pipe_id;
	(void)stage_num;
	return value;
}

static unsigned int translate_sw_interrupt1(void)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "translate_sw_interrupt1() enter:\n");
	return translate_sw_interrupt(sh_css_get_sw_interrupt_value(1));
}

#if 0
static unsigned int translate_sw_interrupt2(void)
{
	/* By smart coding the flag/bits in value (on the SP side),
	 * no translation is required. The returned value can be
	 * binary ORed with existing interrupt info
	 * (it is compatible with enum sh_css_interrupt_info)
	 */
/* MW: No smart coding required, we should just keep interrupt info
   and local context info separated */
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "translate_sw_interrupt2() enter:\n");
	return translate_sw_interrupt(sh_css_get_sw_interrupt_value(2));
}
#endif

/* Deprecated, this is an HRT backend function (memory_access.h) */
void
sh_css_mmu_set_page_table_base_index(hrt_data base_index)
{
	mmu_ID_t mmu_id;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_mmu_set_page_table_base_index() enter: base_index=0x%08x\n",base_index);
	my_css.page_table_base_index = base_index;
	for (mmu_id = (mmu_ID_t)0; mmu_id < (int)N_MMU_ID; mmu_id++) {
		mmu_set_page_table_base_index(mmu_id, base_index);
		mmu_invalidate_cache(mmu_id);
	}
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_mmu_set_page_table_base_index() leave: return_void\n");
}

/* Deprecated, this is an HRT backend function (memory_access.h) */
hrt_data
sh_css_mmu_get_page_table_base_index(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_mmu_get_page_table_base_index() enter & leave\n");
	return mmu_get_page_table_base_index(MMU0_ID);
}

void
sh_css_mmu_invalidate_cache(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_mmu_invalidate_cache() enter & leave\n");
	sh_css_sp_invalidate_mmu();
}


#if defined(HAS_IRQ_MAP_VERSION_1) || defined(HAS_IRQ_MAP_VERSION_1_DEMO)
enum sh_css_err sh_css_translate_interrupt(
	unsigned int *irq_infos)
{
	virq_id_t	irq;
	enum hrt_isp_css_irq_status status = hrt_isp_css_irq_status_more_irqs;
	unsigned int infos = 0;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_translate_interrupt() enter: irq_infos=%d\n", *irq_infos);

	while (status == hrt_isp_css_irq_status_more_irqs) {
		status = virq_get_channel_id(&irq);
		if (status == hrt_isp_css_irq_status_error)
			return sh_css_err_interrupt_error;

#if WITH_PC_MONITORING
		sh_css_print("PC_MONITORING: %s() irq = %d, "
			     "sh_binary_running set to 0\n", __func__, irq);
		sh_binary_running = 0 ;
#endif

		switch (irq) {
		case virq_sp:
			if (sh_css_frame_ready()) {
				infos |= SH_CSS_IRQ_INFO_BUFFER_DONE;
				if (my_css.curr_pipe &&
				    my_css.curr_pipe->invalid_first_frame) {
					infos |=
					  SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME;
					my_css.curr_pipe->invalid_first_frame = false;
				}
			}
			break;
		case virq_isp:
#ifdef HRT_CSIM
			/* Enable IRQ which signals that ISP goes to idle
			 * to get statistics for each binary */
			infos |= SH_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY;
#endif
			break;
		case virq_isys:
			/* css rx interrupt, read error bits from css rx */
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_ERROR;
			break;
		case virq_isys_fifo_full:
			infos |=
			    SH_CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW;
			break;
		case virq_isys_sof:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_SOF;
			break;
		case virq_isys_eof:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_EOF;
			break;
/* Temporarily removed, until we have a seperate flag for FRAME_READY irq */
#if 0
/* hmm, an interrupt mask, why would we have that ? */
		case virq_isys_sol:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_SOL;
			break;
#endif
		case virq_isys_eol:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_EOL;
			break;
/*
 * MW: The 2300 demo system does not have a receiver, and it
 * does not have the following three IRQ channels defined
 */
#if defined(HAS_IRQ_MAP_VERSION_1)
		case virq_ifmt_sideband_changed:
			infos |=
			    SH_CSS_IRQ_INFO_CSS_RECEIVER_SIDEBAND_CHANGED;
			break;
		case virq_gen_short_0:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_0;
			break;
		case virq_gen_short_1:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_1;
			break;
#endif
		case virq_ifmt0_id:
			infos |= SH_CSS_IRQ_INFO_IF_PRIM_ERROR;
			break;
		case virq_ifmt1_id:
			infos |= SH_CSS_IRQ_INFO_IF_PRIM_B_ERROR;
			break;
		case virq_ifmt2_id:
			infos |= SH_CSS_IRQ_INFO_IF_SEC_ERROR;
			break;
		case virq_ifmt3_id:
			infos |= SH_CSS_IRQ_INFO_STREAM_TO_MEM_ERROR;
			break;
		case virq_sw_pin_0:
			infos |= SH_CSS_IRQ_INFO_SW_0;
			break;
		case virq_sw_pin_1:
			infos |= translate_sw_interrupt1();

			if (my_css.curr_state == sh_css_state_executing_sp_bin_copy) {
				my_css.capture_pipe.pipe.capture.output_frame->planes.binary.size =
					sh_css_sp_get_binary_copy_size();
			}
			if (my_css.curr_pipe &&
					my_css.curr_pipe->invalid_first_frame) {
				infos |=
					 SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME;
				my_css.curr_pipe->invalid_first_frame = false;
			}
			break;
		default:
			break;
		}
	}

	if (irq_infos)
		*irq_infos = infos;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_translate_interrupt() "
		"leave: irq_infos=0x%08x\n", infos);

	return sh_css_success;
}

enum sh_css_err sh_css_enable_interrupt(
	enum sh_css_interrupt_info info,
	bool enable)
{
	virq_id_t	irq = N_virq_id;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_enable_interrupt() enter: info=%d, enable=%d\n",info,enable);

	switch (info) {
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_ERROR:
		irq = virq_isys;
		break;
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW:
		irq = virq_isys_fifo_full;
		break;
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_SOF:
		irq = virq_isys_sof;
		break;
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_EOF:
		irq = virq_isys_eof;
		break;
/* Temporarily removed, until we have a seperate flag for FRAME_READY irq */
#if 0
/* hmm, an interrupt mask, why would we have that ? */
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_SOL:
		irq = virq_isys_sol;
		break;
#endif
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_EOL:
		irq = virq_isys_eol;
		break;
#if defined(HAS_IRQ_MAP_VERSION_1)
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_SIDEBAND_CHANGED:
		irq = virq_ifmt_sideband_changed;
		break;
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_0:
		irq = virq_gen_short_0;
		break;
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_1:
		irq = virq_gen_short_1;
		break;
#endif
	case SH_CSS_IRQ_INFO_IF_PRIM_ERROR:
		irq = virq_ifmt0_id;
		break;
	case SH_CSS_IRQ_INFO_IF_PRIM_B_ERROR:
		irq = virq_ifmt1_id;
		break;
	case SH_CSS_IRQ_INFO_IF_SEC_ERROR:
		irq = virq_ifmt2_id;
		break;
	case SH_CSS_IRQ_INFO_STREAM_TO_MEM_ERROR:
		irq = virq_ifmt3_id;
		break;
	case SH_CSS_IRQ_INFO_SW_0:
		irq = virq_sw_pin_0;
		break;
	case SH_CSS_IRQ_INFO_SW_1:
		irq = virq_sw_pin_1;
		break;
	case SH_CSS_IRQ_INFO_SW_2:
		irq = virq_sw_pin_2;
		break;
	default:
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_enable_interrupt() leave: return_err=%d\n",sh_css_err_invalid_arguments);
		return sh_css_err_invalid_arguments;
	}

	virq_enable_channel(irq, enable);

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_enable_interrupt() leave: return_err=%d\n",sh_css_success);
	return sh_css_success;
}

#elif defined(HAS_IRQ_MAP_VERSION_2)

enum sh_css_err sh_css_translate_interrupt(
	unsigned int *irq_infos)
{
	virq_id_t	irq;
	enum hrt_isp_css_irq_status status = hrt_isp_css_irq_status_more_irqs;
	unsigned int infos = 0;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_translate_interrupt() enter: irq_infos=%d\n",irq_infos);

	while (status == hrt_isp_css_irq_status_more_irqs) {
		status = virq_get_channel_id(&irq);
		if (status == hrt_isp_css_irq_status_error)
			return sh_css_err_interrupt_error;

#if WITH_PC_MONITORING
		sh_css_print("PC_MONITORING: %s() irq = %d, "
			     "sh_binary_running set to 0\n", __func__, irq);
		sh_binary_running = 0 ;
#endif

		switch (irq) {
		case virq_sp:
			if (sh_css_frame_ready()) {
				infos |= SH_CSS_IRQ_INFO_BUFFER_DONE;
				if (my_css.curr_pipe &&
				    my_css.curr_pipe->invalid_first_frame) {
					infos |=
					  SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME;
					my_css.curr_pipe->invalid_first_frame = false;
				}
			}
			break;
		case virq_isp:
#ifdef HRT_CSIM
			/* Enable IRQ which signals that ISP goes to idle
			 * to get statistics for each binary */
			infos |= SH_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY;
#endif
			break;
		case virq_isys_sof:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_SOF;
			break;
		case virq_isys_eof:
			infos |= SH_CSS_IRQ_INFO_CSS_RECEIVER_EOF;
			break;
		case virq_isys_csi:
			infos |= SH_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR;
			break;
		case virq_ifmt0_id:
			infos |= SH_CSS_IRQ_INFO_IF_ERROR;
			break;
		case virq_dma:
			infos |= SH_CSS_IRQ_INFO_DMA_ERROR;
			break;
		case virq_sw_pin_0:
			infos |= SH_CSS_IRQ_INFO_SW_0;
			break;
		case virq_sw_pin_1:
			infos |= translate_sw_interrupt1();

			if (my_css.curr_state == sh_css_state_executing_sp_bin_copy) {
				my_css.capture_pipe.pipe.capture.output_frame->planes.binary.size =
					sh_css_sp_get_binary_copy_size();
			}
			if (my_css.curr_pipe &&
					my_css.curr_pipe->invalid_first_frame) {
				infos |=
					 SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME;
				my_css.curr_pipe->invalid_first_frame = false;
			}
			break;
		default:
			break;
		}
	}

	if (irq_infos)
		*irq_infos = infos;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_translate_interrupt() "
		"leave: irq_infos=0x%08x\n", infos);

	return sh_css_success;
}

enum sh_css_err sh_css_enable_interrupt(
	enum sh_css_interrupt_info info,
	bool enable)
{
	virq_id_t	irq = N_virq_id;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_enable_interrupt() enter: info=%d, enable=%d\n",info,enable);

	switch (info) {
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_SOF:
		irq = virq_isys_sof;
		break;
	case SH_CSS_IRQ_INFO_CSS_RECEIVER_EOF:
		irq = virq_isys_eof;
		break;
	case SH_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR:
		irq = virq_isys_csi;
		break;
	case SH_CSS_IRQ_INFO_IF_ERROR:
		irq = virq_ifmt0_id;
		break;
	case SH_CSS_IRQ_INFO_DMA_ERROR:
		irq = virq_dma;
		break;
	case SH_CSS_IRQ_INFO_SW_0:
		irq = virq_sw_pin_0;
		break;
	case SH_CSS_IRQ_INFO_SW_1:
		irq = virq_sw_pin_1;
		break;
	default:
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_enable_interrupt() leave: return_err=%d\n",sh_css_err_invalid_arguments);
		return sh_css_err_invalid_arguments;
	}

	virq_enable_channel(irq, enable);

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_enable_interrupt() leave: return_err=%d\n",sh_css_success);
	return sh_css_success;
}

#else
#error "sh_css.c: IRQ MAP must be one of \
	{IRQ_MAP_VERSION_1, IRQ_MAP_VERSION_1_DEMO, IRQ_MAP_VERSION_2}"
#endif

unsigned int sh_css_get_sw_interrupt_value(
	unsigned int irq)
{
unsigned int	irq_value;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_get_sw_interrupt_value() enter: irq=%d\n",irq);
	irq_value = sh_css_sp_get_sw_interrupt_value(irq);
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_get_sw_interrupt_value() leave: irq_value=%d\n",irq_value);
return irq_value;
}

enum sh_css_err sh_css_wait_for_completion(
	enum sh_css_pipe_id pipe_id)
{
	uint32_t sp_event;
	uint32_t event;
	enum sh_css_pipe_id pipe_id_event;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_wait_for_completion() enter: pipe_id=%d\n",pipe_id);

	do {
		bool rc;

		rc = sp2host_dequeue_irq_event(&sp_event);

		if (rc)
			sh_css_sp_snd_event(SP_SW_EVENT_ID_3, 0, 0, 0);

		if (rc) {
			event = translate_sp_event(sp_event);
			pipe_id_event = event >> 16;
			event &= 0xFFFF;
			if ((pipe_id_event ==  pipe_id) &&
					(event == SH_CSS_EVENT_PIPELINE_DONE)) {
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_wait_for_completion() leave: return_err=%d\n",sh_css_success);
				return sh_css_success;
			}
		}
		hrt_sleep();
	} while (true);

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_wait_for_completion() leave: return_err=%d\n",sh_css_err_internal_error);

	return sh_css_err_internal_error;
}

void
sh_css_uv_offset_is_zero(bool *uv_offset_is_zero)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_uv_offset_is_zero() enter:\n");
	if (uv_offset_is_zero != NULL) {
		*uv_offset_is_zero = SH_CSS_UV_OFFSET_IS_0;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_uv_offset_is_zero() leave: uv_offset_is_zero=%d\n",
		*uv_offset_is_zero);
	}
}

enum sh_css_err sh_css_pipe_set_input_resolution(struct sh_css_pipe *pipe,
				 unsigned int width,
				 unsigned int height)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_resolution() enter: width=%d, height=%d\n",width, height);

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	err = check_res(width, height);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_resolution() leave: return_err=%d\n",err);
		return err;
	}
	if (pipe->input_width != width || pipe->input_height != height)
		sh_css_invalidate_morph_table();

	pipe->input_width  = width;
	pipe->input_height = height;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_resolution() leave: return_err=%d\n",sh_css_success);
	return sh_css_success;
}

enum sh_css_err sh_css_pipe_set_effective_input_resolution(
	struct sh_css_pipe *pipe,
	unsigned int width,
	unsigned int height)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_effective_input_resolution() enter: width=%d, height=%d\n",width, height);

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	err = check_res(width, height);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_effective_input_resolution() leave: return_err=%d\n",err);
		return err;
	}
	if (pipe->input_effective_info.width != width ||
		pipe->input_effective_info.padded_width != width ||
		pipe->input_effective_info.height != height) {
		pipe->input_effective_info.width = width;
		pipe->input_effective_info.padded_width = width;
		pipe->input_effective_info.height = height;
		sh_css_pipe_invalidate_binaries(pipe);
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_effective_input_resolution() leave: return_err=%d\n",sh_css_success);

	return sh_css_success;
}

void sh_css_pipe_set_input_format(
	struct sh_css_pipe *me,
	enum sh_css_input_format format)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_format() enter: format=%d\n",format);

	assert(me != NULL);
	if (me == NULL)
		return;

	me->input_format = format;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_format() leave: return_void\n");
}

enum sh_css_input_format sh_css_pipe_get_input_format(
	struct sh_css_pipe *me)
{
	enum sh_css_input_format	format;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_get_input_format() enter: void\n");

	assert(me != NULL);
	if (me == NULL)
		return -1;

	format = me->input_format;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_get_two_pixels_per_clock() leave: format=%d\n",format);
return format;
}

void sh_css_input_set_binning_factor(
	unsigned int binning_factor)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_binning_factor() enter: binning_factor=%d\n",binning_factor);
	my_css.sensor_binning = binning_factor;
	if (sh_css_params_set_binning_factor(binning_factor)) {
		sh_css_pipe_free_shading_table(&my_css.preview_pipe);
		sh_css_pipe_free_shading_table(&my_css.video_pipe);
		sh_css_pipe_free_shading_table(&my_css.capture_pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_binning_factor() leave: return_void\n");
}

void sh_css_pipe_set_two_pixels_per_clock(
	struct sh_css_pipe *me,
	bool two_ppc)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_two_pixels_per_clock() enter: is_two_ppc=%d\n",two_ppc);

	assert(me != NULL);
	if (me == NULL)
		return;

	if (me->two_ppc != two_ppc) {
		me->two_ppc = two_ppc;
		my_css.reconfigure_css_rx = true;
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_two_pixels_per_clock() leave: return_void\n");
}

bool
sh_css_pipe_get_two_pixels_per_clock(struct sh_css_pipe *me)
{
bool	is_two_ppc;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_get_two_pixels_per_clock() enter: void\n");

	assert(me != NULL);
	if (me == NULL)
		return false;

	is_two_ppc = me->two_ppc;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_get_two_pixels_per_clock() leave: is_two_ppc=%d\n",is_two_ppc);
return is_two_ppc;
}

void
sh_css_pipe_set_input_bayer_order(struct sh_css_pipe *pipe,
				  enum sh_css_bayer_order bayer_order)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_bayer_order() enter: bayer_order=%d\n",bayer_order);

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	pipe->bayer_order = bayer_order;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_bayer_order() leave: return_void\n");
}

void sh_css_pipe_get_extra_pixels_count(
	struct sh_css_pipe *pipe,
	int *extra_rows,
	int *extra_cols)
{
	int rows = SH_CSS_MAX_LEFT_CROPPING,
	    cols = SH_CSS_MAX_LEFT_CROPPING;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_get_extra_pixels_count() enter: void\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(extra_rows != NULL);
	if (extra_rows == NULL)
		return;
	assert(extra_cols != NULL);
	if (extra_cols == NULL)
		return;

	if (lines_needed_for_bayer_order(pipe))
		rows += 2;

	if (columns_needed_for_bayer_order(pipe))
		cols  += 2;

	*extra_rows = rows;
	*extra_cols = cols;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_get_extra_pixels_count() leave: extra_rows=%d, extra_cols=%d\n",
		*extra_rows,*extra_cols);

	return;
}

void sh_css_pipe_set_input_channel(
	struct sh_css_pipe *pipe,
	unsigned int channel_id)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_channel() enter: channel_id=%d\n",channel_id);

	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	pipe->ch_id = channel_id;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_input_channel() leave: return_void\n");
}

void sh_css_pipe_set_input_mode(
	struct sh_css_pipe *pipe,
	enum sh_css_input_mode mode)
{
	enum sh_css_input_mode prev;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_mode() enter: mode=%d\n",mode);

	assert(pipe != NULL);
	if (pipe == NULL)
		return;


	prev = pipe->input_mode;

	if (prev != mode && pipe->mode == SH_CSS_VIDEO_PIPELINE) {
		if (mode == SH_CSS_INPUT_MODE_MEMORY
		    || prev == SH_CSS_INPUT_MODE_MEMORY) {
			/* if we switch from online to offline, we need to
			   reload the binary */
			sh_css_pipe_invalidate_binaries(pipe);
		}
	}
	pipe->input_mode = mode;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_mode() leave: return_void\n");
}

void sh_css_input_set_left_padding(
	unsigned int padding)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_left_padding() enter: padding=%d\n",padding);

	my_css.left_padding = 2*ISP_VEC_NELEMS-padding;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_left_padding() leave: return_void\n");
}

static void init_copy_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"init_copy_descr() enter:\n");

	/* out_info can be NULL */
	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;

	*in_info = *out_info;

	copy_descr.mode          = SH_CSS_BINARY_MODE_COPY;
	copy_descr.online        = true;
	copy_descr.stream_format = pipe->input_format;
	copy_descr.binning       = false;
	copy_descr.two_ppc       = pipe->two_ppc;
	copy_descr.in_info       = in_info;
	copy_descr.out_info      = out_info;
	copy_descr.vf_info       = NULL;
}

static void init_offline_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_binary_descr *descr,
	int mode,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info,
	struct sh_css_frame_info *vf_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"init_offline_descr() enter:\n");

	/* in_info, out_info, vf_info can be NULL */
	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(descr != NULL);
	if (descr == NULL)
		return;

	descr->mode          = mode;
	descr->online        = false;
	descr->stream_format = pipe->input_format;
	descr->binning       = false;
	descr->two_ppc       = false;
	descr->in_info       = in_info;
	descr->out_info      = out_info;
	descr->vf_info       = vf_info;
}

static void
init_vf_pp_descr(struct sh_css_pipe *pipe,
		 struct sh_css_frame_info *in_info,
		 struct sh_css_frame_info *out_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"init_vf_pp_descr() enter:\n");

	/* out_info can be NULL ??? */
	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;

	in_info->raw_bit_depth = 0;
	init_offline_descr(pipe,
			   &vf_pp_descr, SH_CSS_BINARY_MODE_VF_PP,
			   in_info, out_info, NULL);
}

static void init_preview_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info)
{
	int mode = SH_CSS_BINARY_MODE_PREVIEW;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"init_preview_descr() enter:\n");

	/* out_info can be NULL ??? */
	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;

	*in_info = pipe->input_effective_info;
	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	if (input_format_is_yuv(pipe->input_format))
		mode = SH_CSS_BINARY_MODE_COPY;
	else
		in_info->format = SH_CSS_FRAME_FORMAT_RAW;

	init_offline_descr(pipe,
			   &preview_descr, mode,
			   in_info, out_info, NULL);
	if (pipe->online) {
		preview_descr.online	    = pipe->online;
		preview_descr.two_ppc       = pipe->two_ppc;
	}
	preview_descr.stream_format = pipe->input_format;
	preview_descr.binning	    = pipe->input_needs_raw_binning;
}

/* configure and load the copy binary, the next binary is used to
   determine whether the copy binary needs to do left padding. */
static enum sh_css_err load_copy_binary(
	struct sh_css_pipe *pipe,
	struct sh_css_binary *copy_binary,
	struct sh_css_binary *next_binary)
{
	struct sh_css_frame_info copy_out_info, copy_in_info;
	unsigned int left_padding;
	enum sh_css_err err;
	int mode = SH_CSS_BINARY_MODE_COPY;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"load_copy_binary() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(copy_binary != NULL);
	if (copy_binary == NULL)
		return sh_css_err_internal_error;


	if (next_binary != NULL) {
		copy_out_info = next_binary->in_frame_info;
		left_padding = next_binary->left_padding;
	} else {
		copy_out_info = pipe->output_info;
		left_padding = 0;
	}

	init_copy_descr(pipe, &copy_in_info, &copy_out_info);
	copy_descr.mode = mode;
	err = sh_css_binary_find(&copy_descr, copy_binary, false);
	if (err != sh_css_success)
		return err;
	copy_binary->left_padding = left_padding;
	return sh_css_success;
}
#if 0
static enum sh_css_err
primary_alloc_continuous_frames(
	struct sh_css_pipe *pipe,
	unsigned int stride);
#endif


static enum sh_css_err load_preview_binaries(
	struct sh_css_pipe *pipe)
{
	struct sh_css_frame_info prev_in_info,
				 prev_out_info;
	enum sh_css_err err = sh_css_success;
	bool online;
	bool continuous = my_css.continuous;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"load_preview_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	online = pipe->online;

	if (pipe->pipe.preview.preview_binary.info &&
	    pipe->pipe.preview.vf_pp_binary.info)
		return sh_css_success;

	err = check_input(pipe, false);
	if (err != sh_css_success)
		return err;
	err = check_frame_info(&pipe->output_info);
	if (err != sh_css_success)
		return err;

	/* Preview */
	if (pipe->yuv_ds_input_info.width)
		prev_out_info = pipe->yuv_ds_input_info;
	else
		prev_out_info = pipe->output_info;
	sh_css_frame_info_set_format(&prev_out_info,
				     SH_CSS_FRAME_FORMAT_YUV_LINE);
	init_preview_descr(pipe, &prev_in_info, &prev_out_info);
	err = sh_css_binary_find(&preview_descr,
				 &pipe->pipe.preview.preview_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* Viewfinder post-processing */
	init_vf_pp_descr(pipe,
			&pipe->pipe.preview.preview_binary.out_frame_info,
			&pipe->output_info);
	err = sh_css_binary_find(&vf_pp_descr,
				 &pipe->pipe.preview.vf_pp_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* Copy */
	if (!online && !continuous) {
		err = load_copy_binary(pipe,
				       &pipe->pipe.preview.copy_binary,
				       &pipe->pipe.preview.preview_binary);
		if (err != sh_css_success)
			return err;
	}

	err = sh_css_allocate_continuous_frames(true);
	if (err != sh_css_success)
		return err;
#if 0
	if (my_css.continuous) {
		err = primary_alloc_continuous_frames(capture_pipe,
		    pipe->pipe.preview.continuous_frames[0]->info.padded_width);
		if (err != sh_css_success)
			return err;
	}
#endif

#if SH_CSS_PREVENT_UNINIT_READS /* Klocwork pacifier: CWARN.CONSTCOND.IF */
	sh_css_frame_zero(pipe->pipe.preview.continuous_frames[0]);
#endif

	if (pipe->shading_table) {
		sh_css_shading_table_free(pipe->shading_table);
		pipe->shading_table = NULL;
	}
	return sh_css_success;
}

static const struct sh_css_fw_info *last_output_firmware(
	const struct sh_css_fw_info *fw)
{
	const struct sh_css_fw_info *last_fw = NULL;
/* fw can be NULL */
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"last_output_firmware() enter:\n");

	for (; fw; fw = fw->next) {
		const struct sh_css_fw_info *info = fw;
		if (info->info.isp.enable.output)
			last_fw = fw;
	}
	return last_fw;
}

static enum sh_css_err add_firmwares(
	struct sh_css_pipeline *me,
	struct sh_css_binary *binary,
	const struct sh_css_fw_info *fw,
	const struct sh_css_fw_info *last_fw,
	unsigned int binary_mode,
	struct sh_css_frame *in_frame,
	struct sh_css_frame *out_frame,
	struct sh_css_frame *vf_frame,
	struct sh_css_pipeline_stage **my_stage,
	struct sh_css_pipeline_stage **vf_stage)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_pipeline_stage *extra_stage = NULL;

/* all args can be NULL ??? */
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"add_firmwares() enter:\n");

	for (; fw; fw = fw->next) {
		struct sh_css_frame *out = NULL;
		struct sh_css_frame *in = NULL;
		struct sh_css_frame *vf = NULL;
		if ((fw == last_fw) && (fw->info.isp.enable.out_frame  != 0)) {
			out = out_frame;
		}
		if (fw->info.isp.enable.in_frame != 0) {
			in = in_frame;
		}
		if (fw->info.isp.enable.out_frame != 0) {
			vf = vf_frame;
		}

		err = sh_css_pipeline_add_stage(me, binary, fw,
				binary_mode, NULL,
				in, out,
				vf, &extra_stage);
		if (err != sh_css_success)
			return err;
		if (fw->info.isp.enable.output != 0)
			in_frame = extra_stage->args.out_frame;
		if (my_stage && !*my_stage && extra_stage)
			*my_stage = extra_stage;
		if (vf_stage && !*vf_stage && extra_stage &&
		    fw->info.isp.enable.vf_veceven)
			*vf_stage = extra_stage;
	}
	return err;
}

static enum sh_css_err add_vf_pp_stage(
	struct sh_css_pipe *pipe,
	struct sh_css_frame *out_frame,
	struct sh_css_binary *vf_pp_binary,
	struct sh_css_pipeline_stage *post_stage,
	struct sh_css_pipeline_stage **vf_pp_stage)
{
	struct sh_css_pipeline *me;
	const struct sh_css_fw_info *last_fw;
	enum sh_css_err err = sh_css_success;
	struct sh_css_frame *in_frame;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"add_vf_pp_stage() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(vf_pp_binary != NULL);
	if (vf_pp_binary == NULL)
		return sh_css_err_internal_error;

	assert(post_stage != NULL);
	if (post_stage == NULL)
		return sh_css_err_internal_error;

	assert(vf_pp_stage != NULL);
	if (vf_pp_stage == NULL)
		return sh_css_err_internal_error;

	me = &pipe->pipeline;
	in_frame = post_stage->args.out_vf_frame;
	*vf_pp_stage = NULL;

	if (in_frame == NULL)
		in_frame = post_stage->args.out_frame;

	last_fw = last_output_firmware(pipe->vf_stage);
	if (!pipe->disable_vf_pp) {
		err = sh_css_pipeline_add_stage(me, vf_pp_binary, NULL,
				vf_pp_binary->info->mode, NULL,
				in_frame,
				last_fw ? NULL : out_frame,
				NULL, vf_pp_stage);
		if (err != sh_css_success)
			return err;
		in_frame = (*vf_pp_stage)->args.out_frame;
	}
	err = add_firmwares(me, vf_pp_binary, pipe->vf_stage, last_fw,
			    SH_CSS_BINARY_MODE_VF_PP,
			    in_frame, out_frame, NULL,
			    vf_pp_stage, NULL);
	return err;
}

static enum sh_css_err add_capture_pp_stage(
	struct sh_css_pipe *pipe,
	struct sh_css_pipeline *me,
	struct sh_css_frame *out_frame,
	struct sh_css_binary *capture_pp_binary,
	struct sh_css_pipeline_stage *capture_stage,
	struct sh_css_pipeline_stage **pre_vf_pp_stage)
{
	const struct sh_css_fw_info *last_fw;
	enum sh_css_err err = sh_css_success;
	struct sh_css_frame *in_frame = NULL;
	struct sh_css_frame *vf_frame = NULL;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"add_capture_pp_stage() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(me != NULL);
	if (me == NULL)
		return sh_css_err_internal_error;

	assert(capture_pp_binary != NULL);
	if (capture_pp_binary == NULL)
		return sh_css_err_internal_error;

	assert(capture_stage != NULL);
	if (capture_stage == NULL)
		return sh_css_err_internal_error;

	in_frame = capture_stage->args.out_frame;

	assert(pre_vf_pp_stage != NULL);
	if (pre_vf_pp_stage == NULL)
		return sh_css_err_internal_error;

	*pre_vf_pp_stage = NULL;

	if (in_frame == NULL)
		in_frame = capture_stage->args.out_frame;

	last_fw = last_output_firmware(pipe->output_stage);
	if (!pipe->disable_capture_pp &&
	    need_capture_pp(pipe)) {
		err = sh_css_frame_allocate_from_info(&vf_frame,
					    &capture_pp_binary->vf_frame_info);
		if (err != sh_css_success)
			return err;
		err = sh_css_pipeline_add_stage(me, capture_pp_binary, NULL,
				capture_pp_binary->info->mode, NULL,
				NULL,
				last_fw ? NULL : out_frame,
				vf_frame, pre_vf_pp_stage);
		if (err != sh_css_success)
			return err;
		in_frame = (*pre_vf_pp_stage)->args.out_frame;
	}
	err = add_firmwares(me, capture_pp_binary, pipe->output_stage, last_fw,
			    SH_CSS_BINARY_MODE_CAPTURE_PP,
			    in_frame, out_frame, vf_frame,
			    NULL, pre_vf_pp_stage);
	/* If a firmware produce vf_pp output, we set that as vf_pp input */
	if (*pre_vf_pp_stage) {
		(*pre_vf_pp_stage)->args.extra_frame =
		  pipe->pipe.capture.capture_pp_frame;
		(*pre_vf_pp_stage)->args.vf_downscale_log2 =
		  capture_pp_binary->vf_downscale_log2;
	} else {
		*pre_vf_pp_stage = capture_stage;
	}
	return err;
}

static void
number_stages(
	struct sh_css_pipe *pipe)
{
	unsigned i = 0;
	struct sh_css_pipeline_stage *stage;
	for (stage = pipe->pipeline.stages; stage; stage = stage->next) {
		stage->stage_num = i;
		i++;
	}
	pipe->pipeline.num_stages = i;
}

void
sh_css_init_buffer_queues(void)
{
	const struct sh_css_fw_info *fw;
	unsigned int HIVE_ADDR_host_sp_queues_initialized;
	unsigned int i;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_init_buffer_queues() enter:\n");

	for (i = 0; i < MAX_HMM_BUFFER_NUM; i++)
		hmm_buffer_record_h[i] = NULL;


	fw = &sh_css_sp_fw;
	HIVE_ADDR_host_sp_queues_initialized =
		fw->info.sp.host_sp_queues_initialized;

	/* initialize the "sp2host" queues */
	init_sp2host_queues();

	/* initialize the "host2sp" queues */
	init_host2sp_queues();

	/* set "host_sp_queues_initialized" to "true" */
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(host_sp_queues_initialized),
		(uint32_t)(1));

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_init_buffer_queues() leave:\n");
}

static enum sh_css_err preview_start(
	struct sh_css_pipe *pipe)
{
	struct sh_css_pipeline *me = &pipe->pipeline;
	struct sh_css_pipeline_stage *preview_stage;
	struct sh_css_pipeline_stage *vf_pp_stage;
	struct sh_css_frame *in_frame = NULL, *cc_frame = NULL;
	struct sh_css_binary *copy_binary, *preview_binary, *vf_pp_binary;
	enum sh_css_err err = sh_css_success;

	/**
	 * rvanimme: raw_out_frame support is broken and forced to NULL
	 * TODO: add a way to tell the pipeline construction that a
	 * raw_out_frame is used.
	 */
	struct sh_css_frame *raw_out_frame = NULL;
	struct sh_css_frame *out_frame = &me->out_frame;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"preview_start() enter: void\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	sh_css_pipeline_clean(me);

	my_css.curr_pipe = pipe;

	sh_css_preview_get_output_frame_info(&out_frame->info);
	out_frame->contiguous = false;
	out_frame->flash_state = SH_CSS_FRAME_NO_FLASH;
	out_frame->dynamic_data_index = sh_css_frame_out;
	err = init_frame_planes(out_frame);
	if (err != sh_css_success)
		return err;

	err = sh_css_pipe_load_binaries(pipe);
	if (err != sh_css_success)
		return err;

	copy_binary    = &pipe->pipe.preview.copy_binary;
	preview_binary = &pipe->pipe.preview.preview_binary;
	vf_pp_binary   = &pipe->pipe.preview.vf_pp_binary;

	sh_css_metrics_start_frame();

	if (me->reload) {
		struct sh_css_pipeline_stage *post_stage;
		if (pipe->pipe.preview.copy_binary.info) {
			err = sh_css_pipeline_add_stage(me, copy_binary, NULL,
					copy_binary->info->mode,
					NULL, NULL, raw_out_frame, NULL,
					&post_stage);
			if (err != sh_css_success)
				return err;
			in_frame = me->stages->args.out_frame;
		} else {
			in_frame = pipe->pipe.preview.continuous_frames[0];
		}
		err = sh_css_pipeline_add_stage(me, preview_binary, NULL,
						preview_binary->info->mode,
						cc_frame, in_frame, NULL, NULL,
						&post_stage);
		if (err != sh_css_success)
			return err;
		/* If we use copy iso preview, the input must be yuv iso raw */
		post_stage->args.copy_vf =
			preview_binary->info->mode == SH_CSS_BINARY_MODE_COPY;
		post_stage->args.copy_output = !post_stage->args.copy_vf;
		if (post_stage->args.copy_vf) {
			/* in case of copy, use the vf frame as output frame */
			post_stage->args.out_vf_frame =
				post_stage->args.out_frame;
		}

		err = add_vf_pp_stage(pipe, out_frame, vf_pp_binary,
				      post_stage, &vf_pp_stage);
		if (err != sh_css_success)
			return err;
		number_stages(pipe);
	} else {
		sh_css_pipeline_restart(me);
	}

	sh_css_pipeline_stream_clear_pipelines();
	sh_css_pipeline_stream_add_pipeline(&pipe->pipeline);

	err = sh_css_pipeline_get_output_stage(me, SH_CSS_BINARY_MODE_VF_PP,
					       &vf_pp_stage);

	if (err != sh_css_success)
		return err;
	err = sh_css_pipeline_get_stage(me, preview_binary->info->mode,
					&preview_stage);
	if (err != sh_css_success)
		return err;

	vf_pp_stage->args.out_frame = out_frame;

	if (my_css.continuous) {
		in_frame = pipe->pipe.preview.continuous_frames[0];
		cc_frame = pipe->pipe.preview.continuous_frames[1];
		preview_stage->args.cc_frame = cc_frame;
		preview_stage->args.in_frame = in_frame;
	}
	/* update the arguments with the latest info */

	sh_css_set_irq_buffer(preview_stage, sh_css_frame_in,  raw_out_frame);
	sh_css_set_irq_buffer(vf_pp_stage,   sh_css_frame_out, out_frame);

#if 1
	/* Construct and load the capture pipe */
	if (my_css.continuous) {
		bool low_light;
#if 1
		struct sh_css_pipe *capture_pipe = &my_css.capture_pipe;
		err = construct_capture_pipe(capture_pipe);
		if (err != sh_css_success)
			return err;

		low_light = (capture_pipe->capture_mode ==
				SH_CSS_CAPTURE_MODE_LOW_LIGHT) ||
				(capture_pipe->capture_mode ==
				SH_CSS_CAPTURE_MODE_BAYER);

		sh_css_sp_init_pipeline(&capture_pipe->pipeline, SH_CSS_CAPTURE_PIPELINE,
			false, low_light, capture_pipe->xnr,
			capture_pipe->two_ppc, false,
			false, capture_pipe->input_needs_raw_binning,
			SH_CSS_PIPE_CONFIG_OVRD_THRD_1_2);


		sh_css_pipeline_stream_add_pipeline(&capture_pipe->pipeline);
#endif
	}
#endif

	err = sh_css_config_input_network(pipe, copy_binary);
	if (err != sh_css_success)
		return err;

	start_pipe(pipe, SH_CSS_PIPE_CONFIG_OVRD_THRD_1_2);
	me->reload = false;

	return sh_css_success;
}

enum sh_css_err sh_css_pipe_stop(
	struct sh_css_pipe *me)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_stop() enter: me=%p\n",
		me);

	assert(me != NULL);
	if (me == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipeline_stop(me->mode);

	/* more settings should be reset here, maybe use the DEFAULT_PIPE */
	//pipe->input_needs_raw_binning = false;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_stop() leave: return_err=%d\n",err);
	return err;
}

enum sh_css_err sh_css_queue_buffer(
	enum sh_css_pipe_id pipe_id,
	enum sh_css_buffer_type buf_type,
	void *buffer)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_pipe *pipe;
	unsigned int thread_id, i;
	enum sh_css_buffer_queue_id queue_id;
	struct sh_css_pipeline *pipeline;
	struct sh_css_pipeline_stage *stage;
	struct ia_css_i_host_rmgr_vbuf_handle p_vbuf;
	struct ia_css_i_host_rmgr_vbuf_handle *h_vbuf;
	struct sh_css_hmm_buffer ddr_buffer;
	bool rc = true;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_queue_buffer() enter: pipe_id=%d, buf_type=%d, buffer=%p\n",
		pipe_id, buf_type, buffer);

	switch (pipe_id) {
	case SH_CSS_PREVIEW_PIPELINE:
		pipe = &my_css.preview_pipe;
		break;
	case SH_CSS_VIDEO_PIPELINE:
		pipe = &my_css.video_pipe;
		break;
	case SH_CSS_CAPTURE_PIPELINE:
		pipe = &my_css.capture_pipe;
		break;
	case SH_CSS_ACC_PIPELINE:
		pipe = &my_css.acc_pipe;
		break;
	case SH_CSS_COPY_PIPELINE:
		pipe = NULL;
		break;
	default:
		return sh_css_err_internal_error;
	}
	/* For convenience we allow NULL pointer arguments*/
	if (buffer == NULL)
		return sh_css_success;

	assert(pipe_id < SH_CSS_NR_OF_PIPELINES);
	assert(buf_type < SH_CSS_BUFFER_TYPE_NR_OF_TYPES);

	/**
	 * the sh_css_frame_done function needs to have acces to the
	 * latest (completed) output frame to store the sp_binary_copy_size
	 * TODO: this size is global but should be per output frame
	 */
	if (my_css.curr_state == sh_css_state_executing_sp_bin_copy &&
			buf_type == SH_CSS_BUFFER_TYPE_OUTPUT_FRAME)
		my_css.capture_pipe.pipe.capture.output_frame = buffer;

	sh_css_query_sp_thread_id(pipe_id, &thread_id);
	sh_css_query_internal_queue_id(buf_type, &queue_id);
	if (pipe)
		pipeline = &pipe->pipeline;
	else
		pipeline = NULL;

	assert(pipeline != NULL ||
	       pipe_id == SH_CSS_COPY_PIPELINE ||
	       pipe_id == SH_CSS_ACC_PIPELINE);



	ddr_buffer.kernel_ptr = (hrt_vaddress)HOST_ADDRESS(buffer);
	if (buf_type == SH_CSS_BUFFER_TYPE_3A_STATISTICS) {
		ddr_buffer.payload.s3a = *(union sh_css_s3a_data *)buffer;
	} else if (buf_type == SH_CSS_BUFFER_TYPE_DIS_STATISTICS) {
		ddr_buffer.payload.dis = *(struct sh_css_dis_data *)buffer;
	} else if ((buf_type == SH_CSS_BUFFER_TYPE_INPUT_FRAME)
		|| (buf_type == SH_CSS_BUFFER_TYPE_OUTPUT_FRAME)
		|| (buf_type == SH_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME)) {
		ddr_buffer.payload.frame.frame_data =
					((struct sh_css_frame *)buffer)->data;
		ddr_buffer.payload.frame.flashed = 0;
	}
/* start of test for using rmgr for acq/rel memory */
	p_vbuf.vptr = 0;
	p_vbuf.count = 0;
	p_vbuf.size = sizeof(struct sh_css_hmm_buffer);
	h_vbuf = &p_vbuf;
	// TODO: change next to correct pool for optimization
	ia_css_i_host_rmgr_acq_vbuf(hmm_buffer_pool, &h_vbuf);

	assert(h_vbuf != NULL);
	if (h_vbuf == NULL)
		return sh_css_err_internal_error;

	assert(h_vbuf->vptr != 0x0);
	if (h_vbuf->vptr == 0x0)
		return sh_css_err_internal_error;

	mmgr_store(h_vbuf->vptr,
				(void *)(&ddr_buffer),
				sizeof(struct sh_css_hmm_buffer));
	if ((buf_type == SH_CSS_BUFFER_TYPE_3A_STATISTICS)
		|| (buf_type == SH_CSS_BUFFER_TYPE_DIS_STATISTICS)) {
		/* update isp params to ddr */
		//sh_css_commit_isp_config(pipeline, false);
		assert(pipeline != NULL);
		/* Klockwork pacifier */
		if (pipeline == NULL)
			return sh_css_err_internal_error;
		for (stage = pipeline->stages; stage; stage = stage->next) {
			/* Update params before the enqueue of
				empty 3a and dis */
			/* The SP will read the params
				after it got empty 3a and dis */
			if (STATS_ENABLED(stage)) {
				rc = host2sp_enqueue_buffer(thread_id, 0,
						queue_id,
						(uint32_t)h_vbuf->vptr);
			}
		}
	} else if ((buf_type == SH_CSS_BUFFER_TYPE_INPUT_FRAME)
		|| (buf_type == SH_CSS_BUFFER_TYPE_OUTPUT_FRAME)
		|| (buf_type == SH_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME)) {
			rc = host2sp_enqueue_buffer(thread_id,
				0,
				queue_id,
				(uint32_t)h_vbuf->vptr);
	}

	err = (rc == true) ?
		sh_css_success : sh_css_err_buffer_queue_is_full;

	if (err == sh_css_success) {
		for (i = 0; i < MAX_HMM_BUFFER_NUM; i++) {
			if (hmm_buffer_record_h[i] == NULL) {
				hmm_buffer_record_h[i] = h_vbuf;
				break;
			}
		}
	} else {
		ia_css_i_host_rmgr_rel_vbuf(hmm_buffer_pool, &h_vbuf);
	}

		/*
		 * Tell the SP which queues are not empty,
		 * by sending the software event.
		 */
	if (err == sh_css_success)
		sh_css_sp_snd_event(SP_SW_EVENT_ID_1,
				thread_id,
				queue_id,
				0);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_queue_buffer() leave: return_err=%d\n",err);
	return err;
}

/*
 * TODO: Free up the hmm memory space.
	 */
enum sh_css_err
sh_css_dequeue_buffer(enum sh_css_pipe_id   pipe,
			enum sh_css_buffer_type buf_type,
			void **buffer)
{
	enum sh_css_err err;
	enum sh_css_buffer_queue_id queue_id;
	hrt_vaddress ddr_buffer_addr;
	struct sh_css_hmm_buffer ddr_buffer;
	bool rc;
	unsigned int i, found_record;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_dequeue_buffer() enter: pipe=%d, buf_type=%d\n",
		pipe, buf_type);

	assert(buffer != NULL);
	if (buffer == NULL)
		return sh_css_err_internal_error;

	ddr_buffer.kernel_ptr = 0;

	sh_css_query_internal_queue_id(buf_type, &queue_id);

	rc = sp2host_dequeue_buffer(0,
				0,
				queue_id,
				&ddr_buffer_addr);
	if (rc) {
		mmgr_load(ddr_buffer_addr,
				&ddr_buffer,
				sizeof(struct sh_css_hmm_buffer));
		found_record = 0;
		for (i = 0; i < MAX_HMM_BUFFER_NUM; i++) {
			if (hmm_buffer_record_h[i] != NULL && hmm_buffer_record_h[i]->vptr == ddr_buffer_addr) {
				ia_css_i_host_rmgr_rel_vbuf(hmm_buffer_pool, &hmm_buffer_record_h[i]);
				hmm_buffer_record_h[i] = NULL;
				found_record = 1;
				break;
			}
		}
		assert(found_record == 1);
		assert(ddr_buffer.kernel_ptr != 0);

		/* Klockwork pacifier */
		if (ddr_buffer.kernel_ptr == 0)
			return sh_css_err_internal_error;

		switch (buf_type) {
		case SH_CSS_BUFFER_TYPE_OUTPUT_FRAME:
			*buffer =
				(struct sh_css_frame *)HOST_ADDRESS(ddr_buffer.kernel_ptr);
			if (ddr_buffer.payload.frame.exp_id)
				((struct sh_css_frame *)(*buffer))->exp_id
					= ddr_buffer.payload.frame.exp_id;
			if (ddr_buffer.payload.frame.flashed == 0)
				((struct sh_css_frame *)(*buffer))->flash_state
					 = SH_CSS_FRAME_NO_FLASH;
			if (ddr_buffer.payload.frame.flashed == 1)
				((struct sh_css_frame *)(*buffer))->flash_state
					= SH_CSS_FRAME_PARTIAL_FLASH;
			if (ddr_buffer.payload.frame.flashed == 2)
				((struct sh_css_frame *)(*buffer))->flash_state
					= SH_CSS_FRAME_FULLY_FLASH;
			break;
		case SH_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME:
			*buffer =
				(struct sh_css_frame *)HOST_ADDRESS(ddr_buffer.kernel_ptr);
			if (ddr_buffer.payload.frame.exp_id)
				((struct sh_css_frame *)(*buffer))->exp_id
					= ddr_buffer.payload.frame.exp_id;
			if (ddr_buffer.payload.frame.flashed == 0)
				((struct sh_css_frame *)(*buffer))->flash_state
					 = SH_CSS_FRAME_NO_FLASH;
			if (ddr_buffer.payload.frame.flashed == 1)
				((struct sh_css_frame *)(*buffer))->flash_state
					= SH_CSS_FRAME_PARTIAL_FLASH;
			if (ddr_buffer.payload.frame.flashed == 2)
				((struct sh_css_frame *)(*buffer))->flash_state
					= SH_CSS_FRAME_FULLY_FLASH;
			break;
		case SH_CSS_BUFFER_TYPE_3A_STATISTICS:
			*buffer =
				(union sh_css_s3a_data *)HOST_ADDRESS(ddr_buffer.kernel_ptr);
			break;
		case SH_CSS_BUFFER_TYPE_DIS_STATISTICS:
			*buffer =
				(struct sh_css_dis_data *)HOST_ADDRESS(ddr_buffer.kernel_ptr);
			break;
		default:
			rc = false;
			break;
		}
	}

	err = rc ? sh_css_success : sh_css_err_buffer_queue_is_empty;

	/*
	 * Tell the SP which queues are not full,
	 * by sending the software event.
	 */
	if (err == sh_css_success)
		sh_css_sp_snd_event(SP_SW_EVENT_ID_2,
				0,
				queue_id,
				0);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_dequeue_buffer() leave: buffer=%p\n",
		buffer ? *buffer : (void *)-1);

	return err;
}

static uint32_t translate_sp_event(
	uint32_t	sp_event)
{
	unsigned int bit_width = 0, i;
	enum sh_css_sp_event_type sp_event_id;
	uint32_t event;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "translate_sp_event() enter:\n");

	i = SH_CSS_SP_EVENT_NR_OF_TYPES - 1;
	bit_width = 0;
	while (i != 0) {
		bit_width++;
		i = i >> 1;
	}
	sp_event_id = sp_event & ((1ull << bit_width) - 1);
	switch (sp_event_id) {
	case SH_CSS_SP_EVENT_OUTPUT_FRAME_DONE:
		event = (sp_event & 0xFFFF0000) |
					SH_CSS_EVENT_OUTPUT_FRAME_DONE;
		break;
	case SH_CSS_SP_EVENT_VF_OUTPUT_FRAME_DONE:
		event = (sp_event & 0xFFFF0000) |
					SH_CSS_EVENT_VF_OUTPUT_FRAME_DONE;
		break;
	case SH_CSS_SP_EVENT_3A_STATISTICS_DONE:
		event = (sp_event & 0xFFFF0000) |
					SH_CSS_EVENT_3A_STATISTICS_DONE;
		break;
	case SH_CSS_SP_EVENT_DIS_STATISTICS_DONE:
		event = (sp_event & 0xFFFF0000) |
					SH_CSS_EVENT_DIS_STATISTICS_DONE;
		break;
	case SH_CSS_SP_EVENT_PIPELINE_DONE:
		event = (sp_event & 0xFFFF0000) |
					SH_CSS_EVENT_PIPELINE_DONE;
		break;
	default:
		event = (sp_event & 0xFFFF0000) |
					SH_CSS_EVENT_NULL;
		break;
	}

	return event;
}

static void decode_sp_event(
	uint32_t event,
	enum sh_css_pipe_id *pipe_id,
	enum sh_css_event_type *event_id)
{
	unsigned int bit_width = 0, i;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "decode_sp_event() enter:\n");

	assert(pipe_id != NULL);
	if (pipe_id == NULL)
		return;

	assert(event_id != NULL);
	if (event_id == NULL)
		return;

	i = SH_CSS_NR_OF_PIPELINES - 1;
	while (i != 0) {
		bit_width++;
		i = i >> 1;
	}
	*pipe_id = (event >> 16) & ((1ull << bit_width) - 1);

	i = SH_CSS_NR_OF_EVENTS - 1;
	bit_width = 0;
	while (i != 0) {
		bit_width++;
		i = i >> 1;
	}
	*event_id = event & ((1ull << bit_width) - 1);
}


enum sh_css_err
sh_css_dequeue_event(enum sh_css_pipe_id *pipe_id,
			enum sh_css_event_type *event_id)
{
	bool is_event_available;
	uint32_t sp_event;
	uint32_t host_event;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_dequeue_event() enter:\n");

	assert(pipe_id != NULL);
	if (pipe_id == NULL)
		return sh_css_err_internal_error;

	assert(event_id != NULL);
	if (event_id == NULL)
		return sh_css_err_internal_error;

	/* dequeue the IRQ event */
	is_event_available =
		sp2host_dequeue_irq_event(&sp_event);

	/* check whether the IRQ event is available or not */
	if (!is_event_available) {
		sh_css_dtrace(SH_DBG_TRACE,
			"sh_css_dequeue_event() out: EVENT_QUEUE_EMPTY\n");
		return sh_css_err_event_queue_is_empty;
	} else {
		/*
		 * Tell the SP which queues are not full,
		 * by sending the software event.
		 */
		sh_css_sp_snd_event(SP_SW_EVENT_ID_3,
				0,
				0,
				0);
	}

	host_event = translate_sp_event(sp_event);

	decode_sp_event(host_event, pipe_id, event_id);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_dequeue_event() leave: pipe_id=%d, event_id=%d\n",
				*pipe_id, *event_id);

	return sh_css_success;
}

enum sh_css_err sh_css_start(
	enum sh_css_pipe_id pipe_id)
{
	enum sh_css_err err;
	unsigned long timeout = CSS_TIMEOUT_US;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_start() enter: pipe_id=%d\n", pipe_id);
#ifdef __KERNEL__
	printk("sh_css_start() enter: pipe_id=%d\n", pipe_id);
#endif

	switch (pipe_id) {
	case SH_CSS_PREVIEW_PIPELINE:
		err = preview_start(&my_css.preview_pipe);
		break;
	case SH_CSS_VIDEO_PIPELINE:
		err = video_start(&my_css.video_pipe);
		break;
	case SH_CSS_CAPTURE_PIPELINE:
		err = capture_start(&my_css.capture_pipe);
		break;
	default:
		err = sh_css_err_invalid_arguments;
	}

	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_start() leave: return_err=%d\n", err);
		return err;
	}

	/* waiting for the SP is completely started */
	while (!sh_css_sp_has_booted() && timeout) {
		timeout--;
		hrt_sleep();
	}
	if (timeout == 0) {
		sh_css_dump_debug_info("sh_css_start point1");
		sh_css_dump_sp_sw_debug_info();
#ifdef __KERNEL__
		printk(KERN_ERR "%s poll timeout point 1!!!\n", __func__);
#endif
	}

	sh_css_init_host_sp_control_vars();

	sh_css_event_init_irq_mask();

	sh_css_init_buffer_queues();

	/* Force ISP parameter calculation after a mode change */
	sh_css_invalidate_params();
	/* now only preview pipe supports raw binning. if more pipes support it later, we need to change this */
	sh_css_params_set_raw_binning(my_css.preview_pipe.input_needs_raw_binning);

	sh_css_param_update_isp_params(true);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_start() leave: return_err=%d\n", err);
	return err;
}


static enum sh_css_err sh_css_pipeline_stop(
	enum sh_css_pipe_id pipe)
{
	unsigned int i;
	unsigned long timeout = sh_css_stop_timeout_us;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_pipeline_stop()\n");
	(void)pipe;
	/* For now, stop whole SP */
	sh_css_write_host2sp_command(host2sp_cmd_terminate);

	/*
	 * Workaround against ISPFW bug. ISPFW sometimes misses the command
	 * from IA. This WA can be removed if the ISPFW is corrected to
	 * avoid race condition.
	 *
	 * WA: When the command is cleared immediately it is quite probable
	 * that the FW just forgets the command without any real action.
	 * To overcome this case resend the command if the previous one is
	 * handled faster than expected.
	 * There is a small probability that the cmd_terminate is handled twice.
	 * This doesn't cause any side effects on FW side.
	 */
	if (sh_css_read_host2sp_command() == host2sp_cmd_ready)
		sh_css_write_host2sp_command(host2sp_cmd_terminate);

	sh_css_sp_set_sp_running(false);
	sh_css_sp_uninit_pipeline(pipe);
#ifdef __KERNEL__
	printk("STOP_FUNC: reach point 1\n");
#endif
	while (!sh_css_sp_has_terminated() && timeout) {
		timeout--;
		hrt_sleep();
	}
	if (timeout == 0) {
#ifdef __KERNEL__
		printk(KERN_ERR "%s poll timeout point 1!!!\n", __func__);
#endif
	}
#ifdef __KERNEL__
	printk("STOP_FUNC: reach point 2\n");
#endif
	while (timeout && !isp_ctrl_getbit(ISP0_ID, ISP_SC_REG, ISP_IDLE_BIT)) {
		timeout--;
		hrt_sleep();
	}
	if (timeout == 0) {
#ifdef __KERNEL__
		printk(KERN_ERR "%s poll timeout point 2!!!\n", __func__);
#endif
	}
#ifdef __KERNEL__
	printk("STOP_FUNC: reach point 3\n");
#endif

	for (i = 0; i < MAX_HMM_BUFFER_NUM; i++) {
		if (hmm_buffer_record_h[i] != NULL) {
			ia_css_i_host_rmgr_rel_vbuf(hmm_buffer_pool, &hmm_buffer_record_h[i]);
		}
	}

	/* clear pending param sets from refcount */
	sh_css_param_clear_param_sets();
	sh_css_sp_reset_global_vars();

	my_css.curr_state = sh_css_state_idle;

	my_css.curr_pipe = NULL;
	my_css.start_sp_copy = false;

	if (timeout == 0)
		return sh_css_err_internal_error;

	return sh_css_success;
}

enum sh_css_err sh_css_preview_configure_pp_input(
	unsigned int width, unsigned int height)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_pipe *pipe = &my_css.preview_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_configure_pp_input() enter: width=%d, height=%d\n", width, height);
	err = check_null_res(width, height);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_preview_configure_pp_input() leave: return_err=%d\n",
		err);
		return err;
	}
	if (pipe->yuv_ds_input_info.width != width ||
	    pipe->yuv_ds_input_info.height != height) {
		sh_css_frame_info_init(&pipe->yuv_ds_input_info,
				       width, height, 0,
				       SH_CSS_FRAME_FORMAT_YUV_LINE);
		sh_css_pipe_invalidate_binaries(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_preview_configure_pp_input() leave: return_err=%d\n",
		sh_css_success);
	return sh_css_success;
}

enum sh_css_err sh_css_preview_get_input_resolution(
	unsigned int *width,
	unsigned int *height)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.preview_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_get_input_resolution() enter: void\n");

	assert(width != NULL);
	if (width == NULL)
		return sh_css_err_internal_error;

	assert(height != NULL);
	if (height == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success) {
		const struct sh_css_binary *binary;
		if (pipe->pipe.preview.copy_binary.info)
			binary = &pipe->pipe.preview.copy_binary;
		else
			binary = &pipe->pipe.preview.preview_binary;
		*width  = binary->in_frame_info.width +
			  columns_needed_for_bayer_order(pipe);
		*height = binary->in_frame_info.height +
			  lines_needed_for_bayer_order(pipe);

	/* TODO: Remove this when the decimated resolution is available */
	/* Only for continuous preview mode where we need 2xOut resolution */
		if (pipe->input_needs_raw_binning &&
		    pipe->pipe.preview.preview_binary.info->enable.raw_binning) {
			*width *= 2;
			*width -= binary->info->left_cropping;
			*height *= 2;
			*height -= binary->info->left_cropping;
		}
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_get_input_resolution() leave:"
		" width=%d, height=%d\n", *width, *height);
	return err;
}

void
sh_css_video_enable_yuv_ds(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_yuv_ds() enter: enable=%d\n", enable);
	my_css.video_pipe.enable_yuv_ds = enable;
}

void
sh_css_video_enable_high_speed(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_high_speed() enter: enable=%d\n", enable);
	my_css.video_pipe.enable_high_speed = enable;
}

void
sh_css_video_enable_dvs_6axis(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_dvs_6axis() enter: enable=%d\n", enable);
	my_css.video_pipe.enable_dvs_6axis = enable;
}

void
sh_css_video_enable_reduced_pipe(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_reduced_pipe() enter: enable=%d\n", enable);
	my_css.video_pipe.enable_reduced_pipe = enable;
}

void
sh_css_video_enable_viewfinder(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_viewfinder() enter: enable=%d\n", enable);
	my_css.video_pipe.enable_viewfinder = enable;
}

void
sh_css_enable_continuous(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_enable_continuous() enter: enable=%d\n", enable);
	my_css.continuous = enable;
}

void
sh_css_enable_cont_capt(bool enable, bool stop_copy_preview)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_enable_cont_capt() enter: enable=%d\n", enable);
	my_css.cont_capt = enable;
	my_css.stop_copy_preview = stop_copy_preview;
}

void
sh_css_pipe_enable_raw_binning(struct sh_css_pipe *pipe,
				 bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_enable_raw_binning() enter: pipe=%p, enable=%d\n",
		pipe, enable);
	pipe->input_needs_raw_binning = enable;
}

bool
sh_css_continuous_is_enabled(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_continuous_is_enabled() enter: void\n");
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_continuous_is_enabled() leave: enable=%d\n",
		my_css.continuous);
	return my_css.continuous;
}

int
sh_css_continuous_get_max_raw_frames(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_continuous_get_max_raw_frames() enter: void\n");
	return NUM_CONTINUOUS_FRAMES;
}

enum sh_css_err
sh_css_continuous_set_num_raw_frames(int num_frames)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_continuous_set_num_raw_frames() enter: num_frames=%d\n",num_frames);
	if (num_frames > NUM_CONTINUOUS_FRAMES || num_frames < 1)
		return sh_css_err_invalid_arguments;
	/* ok, value allowed */
	my_css.num_cont_raw_frames = num_frames;
	// TODO: check what to regarding initialization
	return sh_css_success;
}

int
sh_css_continuous_get_num_raw_frames(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_continuous_get_num_raw_frames() enter: void\n");
	return my_css.num_cont_raw_frames;
}

bool
sh_css_continuous_start_sp_copy(void)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_continuous_start_sp_copy() enter: void\n");
	return my_css.start_sp_copy;
}

void
sh_css_pipe_disable_vf_pp(struct sh_css_pipe *pipe,
			  bool disable)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_pipe_disable_vf_pp() enter: disable=%d\n",disable);
	pipe->disable_vf_pp = disable;
}

void
sh_css_disable_capture_pp(bool disable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_disable_capture_pp() enter: disable=%d\n",disable);
	my_css.capture_pipe.disable_capture_pp = disable;
}

enum sh_css_err sh_css_pipe_configure_output(
	struct sh_css_pipe *pipe, unsigned int width, unsigned int height,
	unsigned int min_padded_width, enum sh_css_frame_format format)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipe_configure_output() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	err = check_res(width, height);
	if (err != sh_css_success)
		return err;
	if (pipe->output_info.width != width ||
	    pipe->output_info.height != height ||
	    pipe->output_info.padded_width != min_padded_width ||
	    pipe->output_info.format != format) {
		sh_css_frame_info_init(&pipe->output_info, width, height,
				       min_padded_width, format);
		sh_css_pipe_invalidate_binaries(pipe);
	}
	return sh_css_success;
}

static enum sh_css_err sh_css_pipe_get_grid_info(
	struct sh_css_pipe *pipe,
	struct sh_css_grid_info *info)
{
	enum sh_css_err err;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipe_get_grid_info() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success) {
		struct sh_css_binary *s3a_binary = NULL;
#if 0
		/* TODO: this requires the pipeline to be created and filled
		 * in the pipe_load_binaries function. We should do this since
		 * it simplifies a lot of things.
		 */
		struct sh_css_pipeline_stage *stage;
		printf("stages: %p\n", pipe->pipeline.stages);
		for (stage = pipe->pipeline.stages; stage; stage = stage->next) {
			if (STATS_ENABLED(stage)) {
				s3a_binary = stage->binary;
				printf("found stats stage: %d\n", s3a_binary->info->mode);
				break;
			}
			printf("no stats stage: %d\n", stage->binary->info->mode);
		}
#else
	switch (pipe->mode) {
	case SH_CSS_PREVIEW_PIPELINE:
		s3a_binary = &pipe->pipe.preview.preview_binary;
		break;
	case SH_CSS_VIDEO_PIPELINE:
		s3a_binary = &pipe->pipe.video.video_binary;
		break;
	case SH_CSS_CAPTURE_PIPELINE:
		if (pipe->capture_mode == SH_CSS_CAPTURE_MODE_PRIMARY)
			s3a_binary = &pipe->pipe.capture.primary_binary;
		else if (pipe->capture_mode == SH_CSS_CAPTURE_MODE_ADVANCED ||
			 pipe->capture_mode == SH_CSS_CAPTURE_MODE_LOW_LIGHT ||
			 pipe->capture_mode == SH_CSS_CAPTURE_MODE_BAYER)
			s3a_binary = &pipe->pipe.capture.pre_isp_binary;
		break;
	default:
		break;
	}
#endif
		if (s3a_binary)
			err = sh_css_binary_grid_info(s3a_binary, info);
		else
			err = sh_css_err_mode_does_not_have_grid;
	}
	return err;
}

static void init_video_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *vf_info)
{
	int mode = SH_CSS_BINARY_MODE_VIDEO;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_video_descr() enter:\n");

	/* vf_info can be NULL */
	assert(pipe != NULL);
	if (pipe == NULL)
		return;

	assert(in_info != NULL);
	if (in_info == NULL)
		return;

	if (input_format_is_yuv(pipe->input_format))
		mode = SH_CSS_BINARY_MODE_COPY;
	*in_info = pipe->input_effective_info;
	in_info->format = SH_CSS_FRAME_FORMAT_RAW;
	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	init_offline_descr(pipe,
			   &video_descr,
			   mode,
			   in_info,
			   &pipe->output_info,
			   vf_info);
	if (pipe->online) {
		video_descr.online = pipe->online;
		video_descr.two_ppc = pipe->two_ppc;
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_video_descr() leave:\n");
}


/*
 * @GC: TEMPORARY CODE TO TEST DVS AGAINST THE REFERENCE
 * PLEASE DO NOT REMOVE IT!
 */
#if DVS_REF_TESTING
static enum sh_css_err alloc_frame_from_file(
	struct sh_css_pipe *pipe,
	int width,
	int height)
{
	FILE *fp;
	int len = 0, err;
	int bytes_per_pixel;
	const char *file = "../File_input/dvs_input2.yuv";
	char *y_buf, *u_buf, *v_buf;
	char *uv_buf;
	int offset = 0;
	int h, w;
	hrt_vaddress out_base_addr = pipe->pipe.video.ref_frames[0]->data;
	hrt_vaddress out_y_addr  = out_base_addr
		+ pipe->pipe.video.ref_frames[0]->planes.yuv.y.offset;
	hrt_vaddress out_uv_addr = out_base_addr
		+ pipe->pipe.video.ref_frames[0]->planes.yuv.u.offset;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "alloc_frame_from_file() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	bytes_per_pixel = sizeof(char);

	if (!file) {printf("Error: Input file for dvs is not specified\n"); return 1;}
	fp = fopen(file, "rb");
	if (!fp) {printf("Error: Input file for dvs is not found\n"); return 1;}

	err = fseek(fp, 0, SEEK_END);
	if (err) {
		fclose(fp);
	  	printf("Error: Fseek error\n");
	  	return 1;
	}
	len = ftell(fp);

	err = fseek(fp, 0, SEEK_SET);
	if (err) {
		fclose(fp);
		printf("Error: Fseek error2\n");
		return 1;
	}

	len = 2 * len / 3;
	if (len != width * height * bytes_per_pixel) {
		fclose(fp);
		printf("Error: File size mismatches with the internal resolution\n");
		return 1;
	}

	y_buf = (char *) malloc(len);
	u_buf = (char *) malloc(len/4);
	v_buf = (char *) malloc(len/4);
	uv_buf= (char *) malloc(len/2);

	fread(y_buf, 1, len, fp);
	fread(u_buf, 1, len/4, fp);
	fread(v_buf, 1, len/4, fp);

	for (h=0; h<height/2; h++) {
		for (w=0; w<width/2; w++) {
			*(uv_buf + offset + w) = *(u_buf++);
			*(uv_buf + offset + w + width/2) = *(v_buf++);
			//printf("width: %d\n", width);
			//printf("offset_u: %d\n", offset+w);
			//printf("offset_v: %d\n", offset+w+width/2);
		}
		offset += width;
	}

	mmgr_store(out_y_addr, y_buf, len);
	mmgr_store(out_uv_addr, uv_buf, len/2);

	out_base_addr = pipe->pipe.video.ref_frames[1]->data;
	out_y_addr  = out_base_addr + pipe->pipe.video.ref_frames[1]->planes.yuv.y.offset;
	out_uv_addr = out_base_addr + pipe->pipe.video.ref_frames[1]->planes.yuv.u.offset;
	mmgr_store(out_y_addr, y_buf, len);
	mmgr_store(out_uv_addr, uv_buf, len/2);

	fclose(fp);

	return sh_css_success;
}

/* MW: Why do we not pass the pointer to the struct ? */
static enum sh_css_err fill_ref_frame_for_dvs(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info ref_info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "fill_ref_frame_for_dvs() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	/* Allocate tmp_frame which is used to store YUV420 input.
	 * Read YUV420 input from the file to tmp_frame.
	 * Convert from YUV420 to NV12 format */
	err = alloc_frame_from_file(pipe, ref_info.width, ref_info.height);

	return err;
}
#endif

#define SH_CSS_TNR_BIT_DEPTH 8
#define SH_CSS_REF_BIT_DEPTH 8

static enum sh_css_err load_video_binaries(
	struct sh_css_pipe *pipe)
{
	struct sh_css_frame_info video_in_info, ref_info, tnr_info,
				 *video_vf_info;
	bool online;
	enum sh_css_err err = sh_css_success;
	int i;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_video_binaries() enter: "
		"pipe=%p\n", pipe);
	/* we only test the video_binary because offline video doesn't need a
	 * vf_pp binary and online does not (always use) the copy_binary.
	 * All are always reset at the same time anyway.
	 */

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->pipe.video.video_binary.info) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"load_video_binaries() leave: "
			"return=sh_css_success (binary already loaded)\n");
		return sh_css_success;
	}

	online = pipe->online;
	err = check_input(pipe, !online);
	if (err != sh_css_success)
		return err;
	/* cannot have online video and input_mode memory */
	if (online && pipe->input_mode == SH_CSS_INPUT_MODE_MEMORY)
		return sh_css_err_unsupported_configuration;
	if (my_css.video_pipe.enable_viewfinder) {
		err = check_vf_out_info(&pipe->output_info,
					&pipe->vf_output_info);
		if (err != sh_css_success)
			return err;
	} else {
		err = check_frame_info(&pipe->output_info);
		if (err != sh_css_success)
			return err;
	}

	/* Video */
	if (my_css.video_pipe.enable_viewfinder)
		video_vf_info = &pipe->vf_output_info;
	else
		video_vf_info = NULL;
	init_video_descr(pipe, &video_in_info, video_vf_info);
	if (pipe->enable_yuv_ds)
		video_descr.enable_yuv_ds = true;
	if (pipe->enable_high_speed)
		video_descr.enable_high_speed = true;
	if (pipe->enable_dvs_6axis)
		video_descr.enable_dvs_6axis = true;
	if (pipe->enable_reduced_pipe)
		video_descr.enable_reduced_pipe = true;
	video_descr.isp_pipe_version = pipe->isp_pipe_version;
	err = sh_css_binary_find(&video_descr,
				 &pipe->pipe.video.video_binary,
				 true);
	if (err != sh_css_success)
		return err;

	/* Copy */
	if (!online) {
		/* TODO: what exactly needs doing, prepend the copy binary to
		 *	 video base this only on !online?
		 */
		err = load_copy_binary(pipe,
				       &pipe->pipe.video.copy_binary,
				       &pipe->pipe.video.video_binary);
		if (err != sh_css_success)
			return err;
	}

	/* This is where we set the flag for invalid first frame */
	pipe->invalid_first_frame = true;

	/* Viewfinder post-processing */
	if (my_css.video_pipe.enable_viewfinder) {
		init_vf_pp_descr(pipe,
			&pipe->pipe.video.video_binary.vf_frame_info,
			&pipe->vf_output_info);
		err = sh_css_binary_find(&vf_pp_descr,
				&pipe->pipe.video.vf_pp_binary,
				false);
		if (err != sh_css_success)
			return err;
	}

	if (input_format_is_yuv(pipe->input_format))
		/* @GC: In case of YUV Zoom, we use reference frame
		 * to buffer the copy output! Since the sensor input
		 * is being copied, we need a buffer as big as
		 * input resolution */
		ref_info = pipe->pipe.video.video_binary.in_frame_info;
	else
		ref_info = pipe->pipe.video.video_binary.internal_frame_info;

	ref_info.format = SH_CSS_FRAME_FORMAT_YUV420;
	ref_info.raw_bit_depth = SH_CSS_REF_BIT_DEPTH;

	for (i = 0; i < NUM_REF_FRAMES; i++) {
		if (pipe->pipe.video.ref_frames[i])
			sh_css_frame_free(pipe->pipe.video.ref_frames[i]);
		err = sh_css_frame_allocate_from_info(
				&pipe->pipe.video.ref_frames[i],
				&ref_info);
		if (err != sh_css_success)
			return err;
	}

#if SH_CSS_PREVENT_UNINIT_READS /* Klocwork pacifier: CWARN.CONSTCOND.IF */
	sh_css_frame_zero(pipe->pipe.video.ref_frames[0]);
	sh_css_frame_zero(pipe->pipe.video.ref_frames[1]);
#endif

#if DVS_REF_TESTING
	/* @GC: TEMPORARY CODE TO TEST DVS AGAINST THE REFERENCE
	 * To test dvs-6axis:
	 * 1. Enable this function call
	 * 2. Set "reqs.ref_out_requests" to "0" in lineloop.hive.c
	 */
	err = fill_ref_frame_for_dvs(pipe, ref_info);
	if (err != sh_css_success)
		return err;
#endif

	/* YUV copy does not need tnr frames */
	if (input_format_is_yuv(pipe->input_format)) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_video_binaries() "
			"leave: return=sh_css_success (L%d)\n", __LINE__);
		return sh_css_success;
	}

	tnr_info = pipe->pipe.video.video_binary.internal_frame_info;
	tnr_info.format = SH_CSS_FRAME_FORMAT_YUV_LINE;
	tnr_info.raw_bit_depth = SH_CSS_TNR_BIT_DEPTH;

	for (i = 0; i < NUM_TNR_FRAMES; i++) {
		if (pipe->pipe.video.tnr_frames[i])
			sh_css_frame_free(pipe->pipe.video.tnr_frames[i]);
		err = sh_css_frame_allocate_from_info(
				&pipe->pipe.video.tnr_frames[i],
				&tnr_info);
		if (err != sh_css_success)
			return err;
	}

#if SH_CSS_PREVENT_UNINIT_READS /* Klocwork pacifier: CWARN.CONSTCOND.IF */
	sh_css_frame_zero(pipe->pipe.video.tnr_frames[0]);
	sh_css_frame_zero(pipe->pipe.video.tnr_frames[1]);
#endif
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_video_binaries() "
		"leave: return=sh_css_success (L%d)\n", __LINE__);
	return sh_css_success;
}

static enum sh_css_err video_start(
	struct sh_css_pipe *pipe)
{
	struct sh_css_pipeline *me = &pipe->pipeline;
	struct sh_css_pipeline_stage *copy_stage  = NULL;
	struct sh_css_pipeline_stage *video_stage = NULL;
	struct sh_css_pipeline_stage *vf_pp_stage = NULL;
	struct sh_css_pipeline_stage *in_stage    = NULL;
	struct sh_css_binary *copy_binary, *video_binary, *vf_pp_binary;
	enum sh_css_err err = sh_css_success;

	struct sh_css_frame out_frame_struct = { .data = 0 };
	struct sh_css_frame vf_frame_struct = { .data = 0 };

	/**
	 * rvanimme: in_frame support is broken and forced to NULL
	 * TODO: add a way to tell the pipeline construction that an in_frame
	 * is used.
	 */
	struct sh_css_frame *in_frame = NULL;
	struct sh_css_frame *out_frame = &out_frame_struct;
	struct sh_css_frame *vf_frame = &vf_frame_struct;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "video_start() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	my_css.curr_pipe = pipe;

	sh_css_video_get_output_frame_info(&out_frame->info);
	out_frame->contiguous = false;
	out_frame->flash_state = SH_CSS_FRAME_NO_FLASH;
	out_frame->dynamic_data_index = sh_css_frame_out;
	err = init_frame_planes(out_frame);
	if (err != sh_css_success)
		return err;

	if (!my_css.video_pipe.enable_viewfinder || in_frame) {
		/* These situations don't support viewfinder output */
		vf_frame = NULL;
	} else {
		sh_css_video_get_viewfinder_frame_info(&vf_frame->info);
		vf_frame->contiguous = false;
		vf_frame->flash_state = SH_CSS_FRAME_NO_FLASH;
		vf_frame->dynamic_data_index = sh_css_frame_out_vf;
		err = init_frame_planes(vf_frame);
		if (err != sh_css_success)
			return err;
	}

	copy_stage = NULL;
	in_stage = NULL;

	err = sh_css_pipe_load_binaries(pipe);
	if (err != sh_css_success)
		return err;

	copy_binary  = &pipe->pipe.video.copy_binary;
	video_binary = &pipe->pipe.video.video_binary;
	vf_pp_binary = &pipe->pipe.video.vf_pp_binary;

	sh_css_metrics_start_frame();

	sh_css_pipeline_clean(me);
	if (pipe->pipe.video.copy_binary.info) {
		err = sh_css_pipeline_add_stage(me, copy_binary,
			/* TODO: check next params */
			/* const struct sh_css_acc_fw *firmware, */
			NULL,
			/* unsigned int mode, */
			copy_binary->info->mode, /* unsigned int mode,*/
			/* struct sh_css_frame *cc_frame, */
			NULL,
			/* struct sh_css_frame *in_frame, */
			NULL,
			/* struct sh_css_frame *out_frame, */
			NULL,
			/* struct sh_css_frame *vf_frame, */
			NULL,
			/* struct sh_css_pipeline_stage **stage) */
			&copy_stage);
		if (err != sh_css_success)
			return err;
		in_frame = me->stages->args.out_frame;
		in_stage = copy_stage;
	}
	err = sh_css_pipeline_add_stage(me, video_binary, NULL,
					video_binary->info->mode, NULL,
					in_frame, out_frame, NULL,
					&video_stage);
	if (err != sh_css_success)
		return err;
	/* If we use copy iso video, the input must be yuv iso raw */
	video_stage->args.copy_vf =
		video_binary->info->mode == SH_CSS_BINARY_MODE_COPY;
	video_stage->args.copy_output = video_stage->args.copy_vf;
	if (!in_frame && my_css.video_pipe.enable_viewfinder) {
		err = add_vf_pp_stage(pipe, vf_frame, vf_pp_binary,
				      video_stage, &vf_pp_stage);
		if (err != sh_css_success)
			return err;
	}
	number_stages(pipe);

	err = sh_css_config_input_network(pipe, copy_binary);
	if (err != sh_css_success)
		return err;

	sh_css_pipeline_stream_clear_pipelines();
	sh_css_pipeline_stream_add_pipeline(&pipe->pipeline);

	video_stage->args.in_ref_frame = pipe->pipe.video.ref_frames[0];
	video_stage->args.out_ref_frame = pipe->pipe.video.ref_frames[1];
	video_stage->args.in_tnr_frame = pipe->pipe.video.tnr_frames[0];
	video_stage->args.out_tnr_frame = pipe->pipe.video.tnr_frames[1];

	/* update the arguments with the latest info */
	video_stage->args.out_frame = out_frame;

	if (vf_pp_stage)
		vf_pp_stage->args.out_frame = vf_frame;

	start_pipe(pipe, SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD);

	return sh_css_success;
}

enum sh_css_err sh_css_video_get_output_raw_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_output_raw_frame_info() enter:\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	if (pipe->online) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_viewfinder_frame_info() leave: "
		"return_err=%d\n",
			sh_css_err_mode_does_not_have_raw_output);
		return sh_css_err_mode_does_not_have_raw_output;
	}
	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success) {
		*info = pipe->pipe.video.copy_binary.out_frame_info;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_output_raw_frame_info() leave: "
		"info.width=%d, info.height=%d, info.padded_width=%d, "
		"info.format=%d, info.raw_bit_depth=%d, "
		"info.raw_bayer_order=%d\n",
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);
	} else {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_viewfinder_frame_info() leave: "
		"return_err=%d\n",
			err);
	}
		/* info->height += -4; */
		/* info->width += 12; */
	return err;
}

enum sh_css_err sh_css_video_get_viewfinder_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_viewfinder_frame_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_load_binaries(pipe);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_viewfinder_frame_info() leave: "
		"return_err=%d\n",
			err);
		return err;
	}
	/* offline video does not generate viewfinder output */
	if (!pipe->online) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_viewfinder_frame_info() leave: "
		"return_err=%d\n",
			sh_css_err_mode_does_not_have_viewfinder);
		return sh_css_err_mode_does_not_have_viewfinder;
	} else {
		*info = pipe->vf_output_info;
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_viewfinder_frame_info() leave: "
		"info.width=%d, info.height=%d, info.padded_width=%d, "
		"info.format=%d, info.raw_bit_depth=%d, "
		"info.raw_bayer_order=%d\n",
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);

	return sh_css_success;
}

enum sh_css_err sh_css_video_get_input_resolution(
	unsigned int *width,
	unsigned int *height)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_input_resolution() enter: void\n");

	assert(width != NULL);
	if (width == NULL)
		return sh_css_err_internal_error;

	assert(height != NULL);
	if (height == NULL)
		return sh_css_err_internal_error;


	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success) {
		const struct sh_css_binary *binary;
		if (pipe->pipe.video.copy_binary.info)
			binary = &pipe->pipe.video.copy_binary;
		else
			binary = &pipe->pipe.video.video_binary;
		*width  = binary->in_frame_info.width +
			  columns_needed_for_bayer_order(pipe);
		*height = binary->in_frame_info.height +
			  lines_needed_for_bayer_order(pipe);
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_input_resolution() leave: "
		"width=%d, height=%d\n", *width, *height);
	return err;
}

enum sh_css_err sh_css_video_configure_viewfinder(
	unsigned int width, unsigned int height, unsigned int min_padded_width,
	enum sh_css_frame_format format)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_configure_viewfinder() enter: "
		"width=%d, height=%d, min_padded_width=%d, format=%d\n",
		width, height, min_padded_width, format);

	err = check_res(width, height);
	if (err != sh_css_success) {
		sh_css_dtrace(SH_DBG_TRACE,
			"sh_css_video_configure_viewfinder() leave: "
			"return_err=%d\n", err);
		return err;
	}
	if (pipe->vf_output_info.width != width ||
	    pipe->vf_output_info.height != height ||
	    pipe->vf_output_info.padded_width != min_padded_width ||
	    pipe->vf_output_info.format != format) {
		sh_css_frame_info_init(&pipe->vf_output_info,
				       width, height, min_padded_width, format);
		sh_css_pipe_invalidate_binaries(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_configure_viewfinder() leave: return_err=%d\n",
		sh_css_success);
	return sh_css_success;
}

/* Specify the envelope to be used for DIS. */
void sh_css_video_set_dis_envelope(
	unsigned int width,
	unsigned int height)
{
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_set_dis_envelope() enter: width=%d, height=%d\n", width, height);
	if (width != pipe->dvs_envelope.width ||
	    height != pipe->dvs_envelope.height) {
		pipe->dvs_envelope.width = width;
		pipe->dvs_envelope.height = height;
		sh_css_pipe_invalidate_binaries(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_video_set_dis_envelope() leave: return=void\n");
}

void sh_css_video_get_dis_envelope(
	unsigned int *width,
	unsigned int *height)
{
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_video_get_dis_envelope() enter: void\n");

	assert(width != NULL);
	if (width == NULL)
		return;

	assert(height != NULL);
	if (height == NULL)
		return;

	*width = pipe->dvs_envelope.width;
	*height = pipe->dvs_envelope.height;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_dis_envelope() leave: width=%d, height=%d\n", *width, *height);
}

#if 0
/* rvanimme: Not supported for now, use global sh_css_set_zoom_factor */
void sh_css_pipe_set_zoom_factor(
	struct sh_css_pipe *me,
	unsigned int dx,
 	unsigned int dy)
{
	bool is_zoomed  = dx < HRT_GDC_N || dy < HRT_GDC_N;
	bool was_zoomed = me->curr_dx < HRT_GDC_N ||
			  me->curr_dy < HRT_GDC_N;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_zoom_factor() enter: pipe_id=%d, dx=%d, dy=%d\n",
		me->mode, dx, dy);

	if (is_zoomed != was_zoomed) {
		/* for with/without zoom, we use different binaries */
		me->zoom_changed   = true;
	}
	me->curr_dx = dx;
	me->curr_dy = dy;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_set_zoom_factor() leave: return=void\n");
}

void sh_css_pipe_get_zoom_factor(
	struct sh_css_pipe *me,
	unsigned int *dx,
	unsigned int *dy)
{
assert(dx != NULL);
assert(dy != NULL);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_get_zoom_factor() enter: pipe_id=%d\n", me->mode);
	*dx = me->curr_dx;
	*dy = me->curr_dy;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipe_get_zoom_factor() leave: dx=%d, dy=%d\n", *dx, *dy);

}
#endif

static enum sh_css_err load_copy_binaries(
	struct sh_css_pipe *pipe)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_copy_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->pipe.capture.copy_binary.info)
		return sh_css_success;

	err = check_frame_info(&pipe->output_info);
	if (err != sh_css_success)
		return err;

	get_copy_out_frame_format(pipe,
				  &pipe->output_info.format);
	return load_copy_binary(pipe,
				&pipe->pipe.capture.copy_binary,
				NULL);
}

static bool need_capture_pp(
	const struct sh_css_pipe *pipe)
{
	struct sh_css_zoom zoom;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "need_capture_pp() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return false;

	/* determine whether we need to use the capture_pp binary.
	 * This is needed for:
	 *   1. XNR or
	 *   2. Digital Zoom or
	 *   3. YUV downscaling
	 *   4. in continuous capture mode
	 */
	if (pipe->yuv_ds_input_info.width &&
	    ((pipe->yuv_ds_input_info.width != pipe->output_info.width) ||
	     (pipe->yuv_ds_input_info.height != pipe->output_info.height)))
		return true;
	if (pipe->xnr)
		return true;

	sh_css_get_zoom(&zoom);
	if (zoom.dx < HRT_GDC_N ||
	    zoom.dy < HRT_GDC_N)
		return true;

	if (my_css.cont_capt)
		return true;

	return false;
}

static void init_capture_pp_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *vf_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_capture_pp_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(vf_info != NULL);
	if (vf_info == NULL)
		return;

	/* the in_info is only used for resolution to enable
	   bayer down scaling. */
	if (pipe->yuv_ds_input_info.width)
		*in_info = pipe->yuv_ds_input_info;
	else
		*in_info = pipe->output_info;
	in_info->format = SH_CSS_FRAME_FORMAT_YUV420;
	in_info->raw_bit_depth = 0;
	sh_css_frame_info_set_width(in_info, in_info->width, 0);
	init_offline_descr(pipe,
			   &capture_pp_descr,
			   SH_CSS_BINARY_MODE_CAPTURE_PP,
			   in_info,
			   &pipe->output_info,
			   vf_info);
}

static void init_primary_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info,
	struct sh_css_frame_info *vf_info)
{
	int mode = SH_CSS_BINARY_MODE_PRIMARY;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_primary_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;
	assert(vf_info != NULL);
	if (vf_info == NULL)
		return;

	if (input_format_is_yuv(pipe->input_format))
		mode = SH_CSS_BINARY_MODE_COPY;

	*in_info = pipe->input_effective_info;

	in_info->format = SH_CSS_FRAME_FORMAT_RAW;

	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	init_offline_descr(pipe,
			   &prim_descr,
			   mode,
			   in_info,
			   out_info,
			   vf_info);
	if (pipe->online && pipe->input_mode != SH_CSS_INPUT_MODE_MEMORY) {
		prim_descr.online        = true;
		prim_descr.two_ppc       = pipe->two_ppc;
		prim_descr.stream_format = pipe->input_format;
	}
}

static void init_pre_gdc_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_pre_gdc_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;

	*in_info = *out_info;
	in_info->format = SH_CSS_FRAME_FORMAT_RAW;
	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	init_offline_descr(pipe,
			   &pre_gdc_descr, SH_CSS_BINARY_MODE_PRE_ISP,
			   in_info, out_info, NULL);
}

static void
init_gdc_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_gdc_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;

	*in_info = *out_info;
	in_info->format = SH_CSS_FRAME_FORMAT_QPLANE6;
	init_offline_descr(pipe,
			   &gdc_descr, SH_CSS_BINARY_MODE_GDC,
			   in_info, out_info, NULL);
}

static void init_post_gdc_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info,
	struct sh_css_frame_info *vf_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_post_gdc_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;
	assert(vf_info != NULL);
	if (vf_info == NULL)
		return;

	*in_info = *out_info;
	in_info->format = SH_CSS_FRAME_FORMAT_YUV420_16;
	init_offline_descr(pipe,
			   &post_gdc_descr, SH_CSS_BINARY_MODE_POST_ISP,
			   in_info, out_info, vf_info);
}

static void init_pre_anr_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_pre_anr_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;

	*in_info = *out_info;
	in_info->format = SH_CSS_FRAME_FORMAT_RAW;
	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	init_offline_descr(pipe,
			   &pre_anr_descr, SH_CSS_BINARY_MODE_PRE_ISP,
			   in_info, out_info, NULL);
	if (pipe->online) {
		pre_anr_descr.online        = true;
		pre_anr_descr.two_ppc       = pipe->two_ppc;
		pre_anr_descr.stream_format = pipe->input_format;
	}
}

static void init_anr_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_anr_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;

	*in_info = *out_info;
	in_info->format = SH_CSS_FRAME_FORMAT_RAW;
	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	init_offline_descr(pipe,
			   &anr_descr, SH_CSS_BINARY_MODE_ANR,
			   in_info, out_info, NULL);
}

static void init_post_anr_descr(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *in_info,
	struct sh_css_frame_info *out_info,
	struct sh_css_frame_info *vf_info)
{

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "init_post_anr_descr() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return;
	assert(in_info != NULL);
	if (in_info == NULL)
		return;
	assert(out_info != NULL);
	if (out_info == NULL)
		return;
	assert(vf_info != NULL);
	if (vf_info == NULL)
		return;

	*in_info = *out_info;
	in_info->format = SH_CSS_FRAME_FORMAT_RAW;
	in_info->raw_bit_depth = sh_css_pipe_input_format_bits_per_pixel(pipe);
	init_offline_descr(pipe,
			   &post_anr_descr, SH_CSS_BINARY_MODE_POST_ISP,
			   in_info, out_info, vf_info);
}
#if 0
static enum sh_css_err
primary_alloc_continuous_frames(
	struct sh_css_pipe *pipe,
	unsigned int stride)
{
	struct sh_css_capture_settings *mycs = &pipe->pipe.capture;
	enum sh_css_err err = sh_css_success;
	struct sh_css_frame_info ref_info;
	unsigned int i;

	ref_info = mycs->primary_binary.internal_frame_info;
	ref_info.format = SH_CSS_FRAME_FORMAT_RAW;
	ref_info.padded_width = stride;

	for (i = 0; i < NUM_CONTINUOUS_FRAMES; i++) {
		/* free previous frame */
		if (mycs->continuous_frames[i])
			sh_css_frame_free(mycs->continuous_frames[i]);
		/* check if new frame needed */
		if (i < my_css.num_cont_raw_frames) {
			/* allocate new frame */
			err = sh_css_frame_allocate_from_info(
				&mycs->continuous_frames[i], &ref_info);
			if (err != sh_css_success)
				return err;
		}
	}

	if (SH_CSS_PREVENT_UNINIT_READS)
		sh_css_frame_zero(mycs->continuous_frames[0]);
	return sh_css_success;
}
#endif

static enum sh_css_err load_primary_binaries(
	struct sh_css_pipe *pipe)
{
	bool online;
	bool continuous = my_css.continuous;
	bool need_pp = false;
	struct sh_css_frame_info prim_in_info,
				 prim_out_info, vf_info,
				 *vf_pp_in_info;
	enum sh_css_err err = sh_css_success;
	struct sh_css_capture_settings *mycs;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_primary_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	online = pipe->online;
	mycs = &pipe->pipe.capture;

	if (mycs->primary_binary.info)
		return sh_css_success;

	err = check_vf_out_info(&pipe->output_info, &pipe->vf_output_info);
	if (err != sh_css_success)
		return err;
	need_pp = need_capture_pp(pipe);

	/* we use the vf output info to get the primary/capture_pp binary
	   configured for vf_veceven. It will select the closest downscaling
	   factor. */
	vf_info = pipe->vf_output_info;
	sh_css_frame_info_set_format(&vf_info, SH_CSS_FRAME_FORMAT_YUV_LINE);

	/* we build up the pipeline starting at the end */
	/* Capture post-processing */
	if (need_pp) {
		init_capture_pp_descr(pipe, &prim_out_info, &vf_info);
		err = sh_css_binary_find(&capture_pp_descr,
					&mycs->capture_pp_binary,
					false);
		if (err != sh_css_success)
			return err;
	} else {
		prim_out_info = pipe->output_info;
	}

	/* Primary */
	init_primary_descr(pipe, &prim_in_info, &prim_out_info, &vf_info);
	err = sh_css_binary_find(&prim_descr, &mycs->primary_binary, false);
	if (err != sh_css_success)
		return err;

	/* Viewfinder post-processing */
	if (need_pp) {
		vf_pp_in_info =
		    &mycs->capture_pp_binary.vf_frame_info;
	} else {
		vf_pp_in_info =
		    &mycs->primary_binary.vf_frame_info;
	}

	init_vf_pp_descr(pipe, vf_pp_in_info, &pipe->vf_output_info);
	err = sh_css_binary_find(&vf_pp_descr,
				 &mycs->vf_pp_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* ISP Copy */
	if (!online && !continuous) {
		err = load_copy_binary(pipe,
				       &mycs->copy_binary,
				       &mycs->primary_binary);
		if (err != sh_css_success)
			return err;
	}

#if 0
	/* SP copy */
	if (continuous) {
		err = primary_alloc_continuous_frames(pipe);
		if (err != sh_css_success)
			return err;
	}
#endif


	if (need_pp)
		return alloc_capture_pp_frame(pipe, &mycs->capture_pp_binary);
	else
		return sh_css_success;
}

static enum sh_css_err load_advanced_binaries(
	struct sh_css_pipe *pipe)
{
	struct sh_css_frame_info pre_in_info, gdc_in_info,
				 post_in_info, post_out_info,
				 vf_info, *vf_pp_in_info;
	bool need_pp;
	enum sh_css_err err = sh_css_success;

	bool continuous = my_css.continuous;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_advanced_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->pipe.capture.pre_isp_binary.info)
		return sh_css_success;

	vf_info = pipe->vf_output_info;
	err = check_vf_out_info(&pipe->output_info,
				&vf_info);
	if (err != sh_css_success)
		return err;
	need_pp = need_capture_pp(pipe);

	sh_css_frame_info_set_format(&vf_info,
				     SH_CSS_FRAME_FORMAT_YUV_LINE);

	/* we build up the pipeline starting at the end */
	/* Capture post-processing */
	if (need_pp) {
		init_capture_pp_descr(pipe, &post_out_info, &vf_info);
		err = sh_css_binary_find(&capture_pp_descr,
				&pipe->pipe.capture.capture_pp_binary,
				false);
		if (err != sh_css_success)
			return err;
	} else {
		post_out_info = pipe->output_info;
	}

	/* Post-gdc */
	init_post_gdc_descr(pipe, &post_in_info, &post_out_info, &vf_info);
	err = sh_css_binary_find(&post_gdc_descr,
				 &pipe->pipe.capture.post_isp_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* Gdc */
	init_gdc_descr(pipe, &gdc_in_info,
		       &pipe->pipe.capture.post_isp_binary.in_frame_info);
	err = sh_css_binary_find(&gdc_descr,
				 &pipe->pipe.capture.gdc_binary,
				 false);
	if (err != sh_css_success)
		return err;
	pipe->pipe.capture.gdc_binary.left_padding =
		pipe->pipe.capture.post_isp_binary.left_padding;

	/* Pre-gdc */
	init_pre_gdc_descr(pipe, &pre_in_info,
			   &pipe->pipe.capture.gdc_binary.in_frame_info);
	err = sh_css_binary_find(&pre_gdc_descr,
				 &pipe->pipe.capture.pre_isp_binary,
				 false);
	if (err != sh_css_success)
		return err;
	pipe->pipe.capture.pre_isp_binary.left_padding =
		pipe->pipe.capture.gdc_binary.left_padding;

	/* Viewfinder post-processing */
	if (need_pp) {
		vf_pp_in_info =
		    &pipe->pipe.capture.capture_pp_binary.vf_frame_info;
	} else {
		vf_pp_in_info =
		    &pipe->pipe.capture.post_isp_binary.vf_frame_info;
	}

	init_vf_pp_descr(pipe, vf_pp_in_info, &pipe->vf_output_info);
	err = sh_css_binary_find(&vf_pp_descr,
				 &pipe->pipe.capture.vf_pp_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* If continuous, SP is responsible for the copy */
	if (!continuous) {
		/* Copy */
		err = load_copy_binary(pipe,
				       &pipe->pipe.capture.copy_binary,
				       &pipe->pipe.capture.pre_isp_binary);
		if (err != sh_css_success)
			return err;
	}

	if (need_pp)
		return alloc_capture_pp_frame(pipe,
				&pipe->pipe.capture.capture_pp_binary);
	else
		return sh_css_success;
}

static enum sh_css_err load_pre_isp_binaries(
	struct sh_css_pipe *pipe)
{
	struct sh_css_frame_info pre_isp_in_info;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_pre_isp_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->pipe.capture.pre_isp_binary.info)
		return sh_css_success;

	err = check_frame_info(&pipe->output_info);
	if (err != sh_css_success)
		return err;

	init_pre_anr_descr(pipe, &pre_isp_in_info,
			   &pipe->output_info);

	err = sh_css_binary_find(&pre_anr_descr,
				 &pipe->pipe.capture.pre_isp_binary,
				 false);

	return err;
}

static enum sh_css_err load_low_light_binaries(
	struct sh_css_pipe *pipe)
{
	struct sh_css_frame_info pre_in_info, anr_in_info,
				 post_in_info, post_out_info,
				 vf_info, *vf_pp_in_info;
	bool need_pp;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_low_light_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->pipe.capture.pre_isp_binary.info)
		return sh_css_success;

	vf_info = pipe->vf_output_info;
	err = check_vf_out_info(&pipe->output_info,
				&vf_info);
	if (err != sh_css_success)
		return err;
	need_pp = need_capture_pp(pipe);

	sh_css_frame_info_set_format(&vf_info,
				     SH_CSS_FRAME_FORMAT_YUV_LINE);

	/* we build up the pipeline starting at the end */
	/* Capture post-processing */
	if (need_pp) {
		init_capture_pp_descr(pipe, &post_out_info, &vf_info);
		err = sh_css_binary_find(&capture_pp_descr,
				&pipe->pipe.capture.capture_pp_binary,
				false);
		if (err != sh_css_success)
			return err;
	} else {
		post_out_info = pipe->output_info;
	}

	/* Post-anr */
	init_post_anr_descr(pipe, &post_in_info, &post_out_info, &vf_info);
	err = sh_css_binary_find(&post_anr_descr,
				 &pipe->pipe.capture.post_isp_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* Anr */
	init_anr_descr(pipe, &anr_in_info,
		       &pipe->pipe.capture.post_isp_binary.in_frame_info);
	err = sh_css_binary_find(&anr_descr,
				 &pipe->pipe.capture.anr_binary,
				 false);
	if (err != sh_css_success)
		return err;
	pipe->pipe.capture.anr_binary.left_padding =
		pipe->pipe.capture.post_isp_binary.left_padding;

	/* Pre-anr */
	init_pre_anr_descr(pipe, &pre_in_info,
			   &pipe->pipe.capture.anr_binary.in_frame_info);
	err = sh_css_binary_find(&pre_anr_descr,
				 &pipe->pipe.capture.pre_isp_binary,
				 false);
	if (err != sh_css_success)
		return err;
	pipe->pipe.capture.pre_isp_binary.left_padding =
		pipe->pipe.capture.anr_binary.left_padding;

	/* Viewfinder post-processing */
	if (need_pp) {
		vf_pp_in_info =
		    &pipe->pipe.capture.capture_pp_binary.vf_frame_info;
	} else {
		vf_pp_in_info =
		    &pipe->pipe.capture.post_isp_binary.vf_frame_info;
	}

	init_vf_pp_descr(pipe, vf_pp_in_info, &pipe->vf_output_info);
	err = sh_css_binary_find(&vf_pp_descr,
				 &pipe->pipe.capture.vf_pp_binary,
				 false);
	if (err != sh_css_success)
		return err;

	/* Copy */
	err = load_copy_binary(pipe,
			       &pipe->pipe.capture.copy_binary,
			       &pipe->pipe.capture.pre_isp_binary);
	if (err != sh_css_success)
		return err;

	if (need_pp)
		return alloc_capture_pp_frame(pipe,
				&pipe->pipe.capture.capture_pp_binary);
	else
		return sh_css_success;
}

static bool copy_on_sp(
	struct sh_css_pipe *pipe)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "copy_on_sp() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	if (pipe->mode != SH_CSS_CAPTURE_PIPELINE)
		return false;
	if (pipe->capture_mode != SH_CSS_CAPTURE_MODE_RAW)
		return false;
	return my_css.continuous ||
		pipe->input_format == SH_CSS_INPUT_FORMAT_BINARY_8;
}

static enum sh_css_err load_capture_binaries(
	struct sh_css_pipe *pipe)
{
	enum sh_css_err err = sh_css_success;
	bool must_be_raw;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "load_capture_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	/* in primary, advanced,low light or bayer,
						the input format must be raw */
	must_be_raw =
		pipe->capture_mode == SH_CSS_CAPTURE_MODE_ADVANCED ||
		pipe->capture_mode == SH_CSS_CAPTURE_MODE_BAYER ||
		pipe->capture_mode == SH_CSS_CAPTURE_MODE_LOW_LIGHT;
	err = check_input(pipe, must_be_raw);
	if (err != sh_css_success)
		return err;
	if (copy_on_sp(pipe)) {
		/* this is handled by the SP, no ISP binaries needed. */
		if (pipe->input_format == SH_CSS_INPUT_FORMAT_BINARY_8) {
			sh_css_frame_info_init(
				&pipe->output_info,
				JPEG_BYTES, 1, 0, SH_CSS_FRAME_FORMAT_BINARY_8);
			return sh_css_success;
		}
	}

	switch (pipe->capture_mode) {
	case SH_CSS_CAPTURE_MODE_RAW:
		return load_copy_binaries(pipe);
	case SH_CSS_CAPTURE_MODE_BAYER:
		return load_pre_isp_binaries(pipe);
	case SH_CSS_CAPTURE_MODE_PRIMARY:
		return load_primary_binaries(pipe);
	case SH_CSS_CAPTURE_MODE_ADVANCED:
		return load_advanced_binaries(pipe);
	case SH_CSS_CAPTURE_MODE_LOW_LIGHT:
		return load_low_light_binaries(pipe);
	}
	return sh_css_success;
}

static enum sh_css_err sh_css_pipe_load_binaries(
	struct sh_css_pipe *pipe)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_load_binaries() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	switch (pipe->mode) {
	case SH_CSS_PREVIEW_PIPELINE:
		err = load_preview_binaries(pipe);
		break;
	case SH_CSS_VIDEO_PIPELINE:
		err = load_video_binaries(pipe);
		break;
	case SH_CSS_CAPTURE_PIPELINE:
		err = load_capture_binaries(pipe);
		break;
	default:
		err = sh_css_err_internal_error;
		break;
	}
	return err;
}

static enum sh_css_err construct_capture_pipe(
	struct sh_css_pipe *pipe)
{
	struct sh_css_pipeline *me = &pipe->pipeline;
	enum sh_css_err err = sh_css_success;
	enum sh_css_capture_mode mode = pipe->capture_mode;

	struct sh_css_pipeline_stage *vf_pp_stage, *post_stage = NULL;

	struct sh_css_frame *cc_frame = NULL;
	struct sh_css_binary *copy_binary,
			     *primary_binary,
			     *vf_pp_binary,
			     *pre_isp_binary,
			     *gdc_binary,
			     *post_isp_binary,
			     *anr_binary,
			     *capture_pp_binary;
	bool need_pp = false;
	bool enable_vfpp = false;
	bool raw = mode == SH_CSS_CAPTURE_MODE_RAW;
	bool raw_copy = raw && copy_on_sp(pipe);

	/**
	 * rvanimme: in_frame support is broken and forced to NULL
	 * TODO: add a way to tell the pipeline construction that an in_frame
	 * is used.
	 */
	struct sh_css_frame *in_frame = &me->in_frame;
	struct sh_css_frame *out_frame = &me->out_frame;
	struct sh_css_frame *vf_frame = &me->vf_frame;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "construct_capture_pipe() enter:\n");
	sh_css_pipeline_clean(me);

	/* Construct in_frame info (only in case we have dynamic input) */
	if (pipe->input_mode == SH_CSS_INPUT_MODE_MEMORY) {
		in_frame->info.format = SH_CSS_FRAME_FORMAT_RAW;
		in_frame->info.width = pipe->input_width;
		in_frame->info.height = pipe->input_height;
		in_frame->info.raw_bit_depth =
			sh_css_pipe_input_format_bits_per_pixel(pipe);
		sh_css_frame_info_set_width(&in_frame->info, pipe->input_width,
					    in_frame->info.padded_width);

		in_frame->contiguous = false;
		in_frame->flash_state = SH_CSS_FRAME_NO_FLASH;
		in_frame->dynamic_data_index = sh_css_frame_in;
		err = init_frame_planes(in_frame);
		if (err != sh_css_success)
			return err;
	} else {
		in_frame = NULL;
	}

	err = sh_css_pipe_load_binaries(pipe);
	if (err != sh_css_success)
		return err;

	/* Construct out_frame info */
	sh_css_capture_get_output_frame_info(&out_frame->info);
	out_frame->contiguous = false;
	out_frame->flash_state = SH_CSS_FRAME_NO_FLASH;
	out_frame->dynamic_data_index = sh_css_frame_out;
	err = init_frame_planes(out_frame);
	if (err != sh_css_success)
		return err;

	need_pp = need_capture_pp(pipe) || pipe->output_stage;

	enable_vfpp = (mode != SH_CSS_CAPTURE_MODE_RAW &&
			mode != SH_CSS_CAPTURE_MODE_BAYER );

	/* Construct vf_frame info (only in case we have VF) */
	if (enable_vfpp) {
		sh_css_capture_get_viewfinder_frame_info(&vf_frame->info);
		vf_frame->contiguous = false;
		vf_frame->flash_state = SH_CSS_FRAME_NO_FLASH;
		vf_frame->dynamic_data_index = sh_css_frame_out_vf;
		err = init_frame_planes(vf_frame);
		if (err != sh_css_success)
			return err;
	} else {
		vf_frame = NULL;
	}

	copy_binary       = &pipe->pipe.capture.copy_binary;
	primary_binary    = &pipe->pipe.capture.primary_binary;
	vf_pp_binary      = &pipe->pipe.capture.vf_pp_binary;
	pre_isp_binary    = &pipe->pipe.capture.pre_isp_binary;
	gdc_binary        = &pipe->pipe.capture.gdc_binary;
	post_isp_binary   = &pipe->pipe.capture.post_isp_binary;
	anr_binary        = &pipe->pipe.capture.anr_binary;
	capture_pp_binary = &pipe->pipe.capture.capture_pp_binary;

	if (pipe->pipe.capture.copy_binary.info && !raw_copy) {
		err = sh_css_pipeline_add_stage(me, copy_binary, NULL,
				copy_binary->info->mode, NULL, NULL,
				raw ? out_frame : in_frame,
				NULL, &post_stage);
		if (err != sh_css_success)
			return err;
	} else if (my_css.continuous) {
		in_frame = my_css.preview_pipe.pipe.preview.continuous_frames[0];
	}

	if (mode == SH_CSS_CAPTURE_MODE_PRIMARY) {
		err = sh_css_pipeline_add_stage(me, primary_binary,
				NULL, primary_binary->info->mode,
				cc_frame, in_frame,
				need_pp ? NULL : out_frame,
				NULL, &post_stage);
		if (err != sh_css_success)
			return err;
		/* If we use copy iso primary,
		   the input must be yuv iso raw */
		post_stage->args.copy_vf =
			primary_binary->info->mode == SH_CSS_BINARY_MODE_COPY;
		post_stage->args.copy_output = post_stage->args.copy_vf;
	} else if (mode == SH_CSS_CAPTURE_MODE_ADVANCED) {
		err = sh_css_pipeline_add_stage(me, pre_isp_binary,
				NULL, pre_isp_binary->info->mode,
				cc_frame, in_frame, NULL, NULL, NULL);
		if (err != sh_css_success)
			return err;
		err = sh_css_pipeline_add_stage(me, gdc_binary,
				NULL, gdc_binary->info->mode,
				NULL, NULL, NULL, NULL, NULL);
		if (err != sh_css_success)
			return err;
		err = sh_css_pipeline_add_stage(me, post_isp_binary,
				NULL, post_isp_binary->info->mode,
				NULL, NULL,
				need_pp ? NULL : out_frame,
				NULL, &post_stage);
		if (err != sh_css_success)
			return err;
	} else if (mode == SH_CSS_CAPTURE_MODE_LOW_LIGHT) {
		err = sh_css_pipeline_add_stage(me, pre_isp_binary,
				NULL, pre_isp_binary->info->mode,
				cc_frame, in_frame, NULL, NULL, NULL);
		if (err != sh_css_success)
			return err;
		err = sh_css_pipeline_add_stage(me, anr_binary,
				NULL, anr_binary->info->mode,
				NULL, NULL, NULL, NULL, NULL);
		if (err != sh_css_success)
			return err;
		err = sh_css_pipeline_add_stage(me, post_isp_binary,
				NULL, post_isp_binary->info->mode,
				NULL, NULL,
				need_pp ? NULL : out_frame,
				NULL, &post_stage);
		if (err != sh_css_success)
			return err;
	} else if (mode == SH_CSS_CAPTURE_MODE_BAYER) {
		err = sh_css_pipeline_add_stage(me, pre_isp_binary,
				NULL, pre_isp_binary->info->mode,
				cc_frame, in_frame, out_frame,
				NULL, NULL);
		if (err != sh_css_success)
			return err;
	}
	if (need_pp) {
		err = add_capture_pp_stage(pipe, me, out_frame,
					   capture_pp_binary,
					   post_stage, &post_stage);
		if (err != sh_css_success)
			return err;
	}
	if (enable_vfpp) {
		err = add_vf_pp_stage(pipe, vf_frame, vf_pp_binary,
				      post_stage, &vf_pp_stage);
		if (err != sh_css_success)
			return err;
	}
	number_stages(pipe);

	return sh_css_success;

}

static enum sh_css_err capture_start(
	struct sh_css_pipe *pipe)
{
	struct sh_css_pipeline *me = &pipe->pipeline;

	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "capture_start() enter:\n");
	sh_css_pipeline_stream_clear_pipelines();
	sh_css_pipeline_stream_add_pipeline(me);

	err = construct_capture_pipe(pipe);
	if (err != sh_css_success)
		return err;

	my_css.curr_pipe = pipe;

	if (pipe->input_mode != SH_CSS_INPUT_MODE_MEMORY) {
	err = sh_css_config_input_network(pipe, &pipe->pipe.capture.copy_binary);
	if (err != sh_css_success)
		return err;
	}

	if (pipe->capture_mode == SH_CSS_CAPTURE_MODE_RAW ||
	    pipe->capture_mode == SH_CSS_CAPTURE_MODE_BAYER) {
		if (copy_on_sp(pipe)) {
			my_css.curr_state = sh_css_state_executing_sp_bin_copy;

			return start_copy_on_sp(pipe,
				&pipe->pipe.capture.copy_binary,
				&me->out_frame);
		}
	}

	start_pipe(pipe, SH_CSS_PIPE_CONFIG_OVRD_THRD_2);

	return sh_css_success;

}

static enum sh_css_err sh_css_pipe_get_output_frame_info(
	struct sh_css_pipe *pipe,
	struct sh_css_frame_info *info)
{
	enum sh_css_err err;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_get_output_frame_info() enter:\n");

	assert(pipe != NULL);
	if (pipe == NULL)
		return sh_css_err_internal_error;

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success)
		*info = pipe->output_info;
	if (copy_on_sp(pipe) &&
	    pipe->input_format == SH_CSS_INPUT_FORMAT_BINARY_8) {
		sh_css_frame_info_init(info, JPEG_BYTES, 1, 0,
				SH_CSS_FRAME_FORMAT_BINARY_8);
	} else if (info->format == SH_CSS_FRAME_FORMAT_RAW) {
		info->raw_bit_depth =
			sh_css_pipe_input_format_bits_per_pixel(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_get_output_frame_info() leave:\n");
	return err;
}

enum sh_css_err sh_css_capture_get_viewfinder_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.capture_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_viewfinder_frame_info()\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	if (pipe->capture_mode == SH_CSS_CAPTURE_MODE_RAW ||
	    pipe->capture_mode == SH_CSS_CAPTURE_MODE_BAYER)
		return sh_css_err_mode_does_not_have_viewfinder;
	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success)
		*info = pipe->vf_output_info;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_viewfinder_frame_info() leave: "
		"info.width=%d, info.height=%d, info.padded_width=%d, "
		"info.format=%d, info.raw_bit_depth=%d, "
		"info.raw_bayer_order=%d\n",
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);
	return err;
}

enum sh_css_err sh_css_capture_get_output_raw_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.capture_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_output_raw_frame_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	if (pipe->online || copy_on_sp(pipe)) {
		return sh_css_err_mode_does_not_have_raw_output;
	}
	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success)
		*info = pipe->pipe.capture.copy_binary.out_frame_info;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_output_raw_frame_info() leave: "
		"info.width=%d, info.height=%d, info.padded_width=%d, "
		"info.format=%d, info.raw_bit_depth=%d, "
		"info.raw_bayer_order=%d\n",
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);
	return err;
}

enum sh_css_err sh_css_capture_get_input_resolution(
	unsigned int *width,
	unsigned int *height)
{
	enum sh_css_err err;
	struct sh_css_pipe *pipe = &my_css.capture_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_input_resolution() enter: void\n");

	assert(width != NULL);
	if (width == NULL)
		return sh_css_err_internal_error;

	assert(height != NULL);
	if (height == NULL)
		return sh_css_err_internal_error;

	if (copy_on_sp(pipe) &&
	    pipe->input_format == SH_CSS_INPUT_FORMAT_BINARY_8) {
		*width = JPEG_BYTES;
		*height = 1;
		return sh_css_success;
	}

	err = sh_css_pipe_load_binaries(pipe);
	if (err == sh_css_success) {
		const struct sh_css_binary *binary;
		if (pipe->pipe.capture.copy_binary.info)
			binary = &pipe->pipe.capture.copy_binary;
		else if (pipe->pipe.capture.primary_binary.info)
			binary = &pipe->pipe.capture.primary_binary;
		else
			binary = &pipe->pipe.capture.pre_isp_binary;
		*width  = binary->in_frame_info.width +
			  columns_needed_for_bayer_order(pipe);
		*height = binary->in_frame_info.height +
			  lines_needed_for_bayer_order(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_capture_get_input_resolution() "
		"leave: width=%d, height=%d\n",*width,*height);
	return err;
}

void
sh_css_capture_set_mode(enum sh_css_capture_mode mode)
{
	struct sh_css_pipe *pipe = &my_css.capture_pipe;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_capture_set_mode() enter: mode=%d\n",mode);
	if (mode != pipe->capture_mode) {
		pipe->capture_mode = mode;
		sh_css_pipe_invalidate_binaries(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_set_mode() leave: return_void\n");
}

void
sh_css_capture_enable_xnr(bool enable)
{
	struct sh_css_pipe *pipe = &my_css.capture_pipe;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_capture_enable_xnr() enter: enable=%d\n",enable);
	if (pipe->mode == SH_CSS_CAPTURE_PIPELINE && pipe->xnr != enable) {
		sh_css_pipe_invalidate_binaries(pipe);
		pipe->xnr = enable;
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_enable_xnr() leave: return_void\n");
}

enum sh_css_err sh_css_capture_configure_viewfinder(
	unsigned int width, unsigned int height, unsigned int min_padded_width,
	enum sh_css_frame_format format)
{
	struct sh_css_pipe *pipe = &my_css.capture_pipe;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_capture_configure_viewfinder() "
		"enter: width=%d, height=%d, min_padded_width=%d, format=%d\n",
		width, height, min_padded_width, format);
	err = check_res(width, height);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_configure_viewfinder() leave: return_err=%d\n",
		err);
		return err;
	}
	if (pipe->vf_output_info.width != width ||
	    pipe->vf_output_info.height != height ||
	    pipe->vf_output_info.padded_width != min_padded_width ||
	    pipe->vf_output_info.format != format) {
		sh_css_frame_info_init(&pipe->vf_output_info,
				       width, height, min_padded_width, format);
		sh_css_pipe_invalidate_binaries(pipe);
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_configure_viewfinder() leave: return_err=%d\n",
		sh_css_success);
	return sh_css_success;
}

enum sh_css_err sh_css_capture_configure_pp_input(
	unsigned int width, unsigned int height)
{
	struct sh_css_pipe *pipe = &my_css.capture_pipe;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_configure_pp_input() enter: "
		"width=%d, height=%d\n",width, height);
	err = check_null_res(width, height);
	if (err != sh_css_success) {
		sh_css_dtrace(SH_DBG_TRACE,
			"sh_css_capture_configure_pp_input() leave: "
			"return_err=%d\n",
			err);
		return err;
	}
	if (pipe->yuv_ds_input_info.width != width ||
	    pipe->yuv_ds_input_info.height != height) {
		sh_css_frame_info_init(&pipe->yuv_ds_input_info,
				       width, height, 0,
				       SH_CSS_FRAME_FORMAT_YUV420);
		sh_css_pipe_invalidate_binaries(pipe);
	}
	return sh_css_success;
}

void
sh_css_pipe_enable_online(
	struct sh_css_pipe *me,
	bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipe_enable_online() enter & leave:\n");
	me->online = enable;
}

void sh_css_send_input_frame(
	unsigned short *data,
	unsigned int width,
	unsigned int height)
{
	sh_css_hrt_send_input_frame(data, width, height,
				    my_css.curr_pipe->ch_id,
				    my_css.curr_pipe->input_format,
				    my_css.curr_pipe->two_ppc);
}


void sh_css_streaming_to_mipi_start_frame(
	unsigned int channel_id,
	enum sh_css_input_format input_format,
	bool two_pixels_per_clock)
{
	sh_css_hrt_streaming_to_mipi_start_frame(channel_id,
						input_format,
						two_pixels_per_clock);
}


void
sh_css_streaming_to_mipi_send_line(
	unsigned int channel_id,
	unsigned short *data,
	unsigned int width,
	unsigned short *data2,
	unsigned int width2)
{
	sh_css_hrt_streaming_to_mipi_send_line(channel_id,
						data, width,
						data2, width2);
}


void sh_css_streaming_to_mipi_end_frame(
	unsigned int channel_id)
{
	sh_css_hrt_streaming_to_mipi_end_frame(channel_id);
}


static enum sh_css_err allocate_frame_data(
	struct sh_css_frame *frame)
{
	frame->data = mmgr_alloc_attr(frame->data_bytes,
		frame->contiguous ?
			MMGR_ATTRIBUTE_CONTIGUOUS : MMGR_ATTRIBUTE_DEFAULT);

	if (frame->data == mmgr_NULL)
		return sh_css_err_cannot_allocate_memory;
	return sh_css_success;
}

static void init_plane(
	struct sh_css_frame_plane *plane,
	unsigned int width,
	unsigned int stride,
	unsigned int height,
	unsigned int offset)
{
	plane->height = height;
	plane->width = width;
	plane->stride = stride;
	plane->offset = offset;
}

static void init_single_plane(
	struct sh_css_frame *frame,
	struct sh_css_frame_plane *plane,
	unsigned int height,
	unsigned int subpixels_per_line,
	unsigned int bytes_per_pixel)
{
	unsigned int stride;

	stride = subpixels_per_line * bytes_per_pixel;
	frame->data_bytes = stride * height;
	init_plane(plane, subpixels_per_line, stride, height, 0);
	return;
	}

static void init_nv_planes(
	struct sh_css_frame *frame,
	unsigned int horizontal_decimation,
	unsigned int vertical_decimation)
{
	unsigned int y_width = frame->info.padded_width,
		     y_height = frame->info.height,
		     uv_width = 2 * (y_width / horizontal_decimation),
		     uv_height = y_height / vertical_decimation,
		     y_bytes, uv_bytes;

	y_bytes   = y_width * y_height;
	uv_bytes  = uv_width * uv_height;

	frame->data_bytes = y_bytes + uv_bytes;
	init_plane(&frame->planes.nv.y, y_width, y_width, y_height, 0);
	init_plane(&frame->planes.nv.uv, uv_width,
			uv_width, uv_height, y_bytes);
	return;
}

static void init_yuv_planes(
	struct sh_css_frame *frame,
	unsigned int horizontal_decimation,
	unsigned int vertical_decimation,
	bool swap_uv,
	unsigned int bytes_per_element)
{
	unsigned int y_width = frame->info.padded_width,
		     y_height = frame->info.height,
		     uv_width = y_width / horizontal_decimation,
		     uv_height = y_height / vertical_decimation,
		     y_stride, y_bytes, uv_bytes, uv_stride;

	y_stride  = y_width * bytes_per_element;
	uv_stride = uv_width * bytes_per_element;
	y_bytes   = y_stride * y_height;
	uv_bytes  = uv_stride * uv_height;

	frame->data_bytes = y_bytes + 2 * uv_bytes;
	init_plane(&frame->planes.yuv.y, y_width, y_stride, y_height, 0);
		if (swap_uv) {
			init_plane(&frame->planes.yuv.v, uv_width, uv_stride,
				   uv_height, y_bytes);
			init_plane(&frame->planes.yuv.u, uv_width, uv_stride,
				   uv_height, y_bytes + uv_bytes);
		} else {
			init_plane(&frame->planes.yuv.u, uv_width, uv_stride,
				   uv_height, y_bytes);
			init_plane(&frame->planes.yuv.v, uv_width, uv_stride,
				   uv_height, y_bytes + uv_bytes);
		}
	return;
	}

static void init_rgb_planes(
	struct sh_css_frame *frame,
	unsigned int bytes_per_element)
{
	unsigned int width = frame->info.width,
		     height = frame->info.height, stride, bytes;

	stride = width * bytes_per_element;
	bytes  = stride * height;
	frame->data_bytes = 3 * bytes;
	init_plane(&frame->planes.planar_rgb.r,
			width, stride, height, 0);
	init_plane(&frame->planes.planar_rgb.g,
			width, stride, height, 1 * bytes);
	init_plane(&frame->planes.planar_rgb.b,
			width, stride, height, 2 * bytes);
	return;
	}

static void init_qplane6_planes(
	struct sh_css_frame *frame)
{
	unsigned int width = frame->info.padded_width / 2,
		     height = frame->info.height / 2,
		     bytes, stride;

	stride = width * 2;
	bytes  = stride * height;

	frame->data_bytes = 6 * bytes;
	init_plane(&frame->planes.plane6.r,
			width, stride, height, 0 * bytes);
	init_plane(&frame->planes.plane6.r_at_b,
			width, stride, height, 1 * bytes);
	init_plane(&frame->planes.plane6.gr,
			width, stride, height, 2 * bytes);
	init_plane(&frame->planes.plane6.gb,
			width, stride, height, 3 * bytes);
	init_plane(&frame->planes.plane6.b,
			width, stride, height, 4 * bytes);
	init_plane(&frame->planes.plane6.b_at_r,
			width, stride, height, 5 * bytes);
	return;
}

static enum sh_css_err init_frame_planes(
	struct sh_css_frame *frame)
{
	assert(frame != NULL);
	if (frame == NULL)
		return sh_css_err_internal_error;

	switch (frame->info.format) {
	case SH_CSS_FRAME_FORMAT_RAW:
		init_single_plane(frame, &frame->planes.raw,
				frame->info.height,
				frame->info.padded_width,
				// always use 2-bytes per pixel
				2);
		break;
	case SH_CSS_FRAME_FORMAT_RGB565:
		init_single_plane(frame, &frame->planes.rgb,
				    frame->info.height,
				    frame->info.padded_width, 2);
		break;
	case SH_CSS_FRAME_FORMAT_RGBA888:
		init_single_plane(frame, &frame->planes.rgb,
				    frame->info.height,
				    frame->info.padded_width * 4, 1);
		break;
	case SH_CSS_FRAME_FORMAT_PLANAR_RGB888:
		init_rgb_planes(frame, 1);
		break;
		/* yuyv and uyvu have the same frame layout, only the data
		 * positioning differs.
		 */
	case SH_CSS_FRAME_FORMAT_YUYV:
	case SH_CSS_FRAME_FORMAT_UYVY:
		init_single_plane(frame, &frame->planes.yuyv,
				    frame->info.height,
				    frame->info.padded_width * 2, 1);
		break;
	case SH_CSS_FRAME_FORMAT_YUV_LINE:
		/* Needs 3 extra lines to allow vf_pp prefetching */
		init_single_plane(frame, &frame->planes.yuyv,
				    frame->info.height * 3/2 + 3,
				    frame->info.padded_width, 1);
		break;
	case SH_CSS_FRAME_FORMAT_NV11:
		init_nv_planes(frame, 4, 1);
		break;
		/* nv12 and nv21 have the same frame layout, only the data
		 * positioning differs.
		 */
	case SH_CSS_FRAME_FORMAT_NV12:
	case SH_CSS_FRAME_FORMAT_NV21:
		init_nv_planes(frame, 2, 2);
		break;
		/* nv16 and nv61 have the same frame layout, only the data
		 * positioning differs.
		 */
	case SH_CSS_FRAME_FORMAT_NV16:
	case SH_CSS_FRAME_FORMAT_NV61:
		init_nv_planes(frame, 2, 1);
		break;
	case SH_CSS_FRAME_FORMAT_YUV420:
		init_yuv_planes(frame, 2, 2, false, 1);
		break;
	case SH_CSS_FRAME_FORMAT_YUV422:
		init_yuv_planes(frame, 2, 1, false, 1);
		break;
	case SH_CSS_FRAME_FORMAT_YUV444:
		init_yuv_planes(frame, 1, 1, false, 1);
		break;
	case SH_CSS_FRAME_FORMAT_YUV420_16:
		init_yuv_planes(frame, 2, 2, false, 2);
		break;
	case SH_CSS_FRAME_FORMAT_YUV422_16:
		init_yuv_planes(frame, 2, 1, false, 2);
		break;
	case SH_CSS_FRAME_FORMAT_YV12:
		init_yuv_planes(frame, 2, 2, true, 1);
		break;
	case SH_CSS_FRAME_FORMAT_YV16:
		init_yuv_planes(frame, 2, 1, true, 1);
		break;
	case SH_CSS_FRAME_FORMAT_QPLANE6:
		init_qplane6_planes(frame);
		break;
	case SH_CSS_FRAME_FORMAT_BINARY_8:
		init_single_plane(frame, &frame->planes.binary.data,
				    frame->info.height,
				    frame->info.padded_width, 1);
		frame->planes.binary.size = 0;
		break;
	default:
		return sh_css_err_invalid_frame_format;
	}
	return sh_css_success;
}


static enum sh_css_err allocate_frame_and_data(
	struct sh_css_frame **frame,
	unsigned int width,
	unsigned int height,
	enum sh_css_frame_format format,
	unsigned int padded_width,
	unsigned int raw_bit_depth,
	bool contiguous)
{
	enum sh_css_err err;
	struct sh_css_frame *me = sh_css_malloc(sizeof(*me));

	if (me == NULL)
		return sh_css_err_cannot_allocate_memory;

	me->info.width = width;
	me->info.height = height;
	me->info.format = format;
	me->info.padded_width = padded_width;
	me->info.raw_bit_depth = raw_bit_depth;
	me->contiguous = contiguous;
	me->dynamic_data_index = SH_CSS_INVALID_FRAME_ID;

	err = init_frame_planes(me);

	if (err == sh_css_success)
		err = allocate_frame_data(me);

	if (err != sh_css_success) {
		sh_css_free(me);
		return err;
	}

		*frame = me;

	return err;
}

enum sh_css_err sh_css_frame_allocate(
	struct sh_css_frame **frame,
	unsigned int width,
	unsigned int height,
	enum sh_css_frame_format format,
	unsigned int padded_width,
	unsigned int raw_bit_depth)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate() enter: width=%d, height=%d, "
		"padded_width=%d, format=%d\n",
		width, height, padded_width, format);


	err = allocate_frame_and_data(frame, width, height, format,
			      padded_width, raw_bit_depth, false);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate() leave: return=%d, frame=%p\n",
		err, frame ? *frame : (void *)-1);

	return err;
}

enum sh_css_err sh_css_frame_allocate_from_info(
	struct sh_css_frame **frame,
	const struct sh_css_frame_info *info)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate_from_info() enter: "
		"frame=%p, info=%p\n", frame, info);

	assert(frame != NULL);
	if (frame == NULL)
		return sh_css_err_internal_error;

	err = sh_css_frame_allocate(frame,
				     info->width,
				     info->height,
				     info->format,
				     info->padded_width,
				     info->raw_bit_depth);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate_from_info() leave: "
		"return=%d, *frame=%p\n", err, *frame);
return err;
}

enum sh_css_err sh_css_frame_allocate_contiguous(
	struct sh_css_frame **frame,
	unsigned int width,
	unsigned int height,
	enum sh_css_frame_format format,
	unsigned int padded_width,
	unsigned int raw_bit_depth)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate_contiguous() "
		"enter: width=%d, height=%d, format=%d\n",
		width, height, format);

	err = allocate_frame_and_data(frame, width, height, format,
					padded_width, raw_bit_depth, true);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate_contiguous() leave: frame=%p\n",
		frame ? *frame : (void *)-1);

	return err;
}

enum sh_css_err sh_css_frame_allocate_contiguous_from_info(
	struct sh_css_frame **frame,
	const struct sh_css_frame_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate_contiguous_from_info() enter:\n");

	assert(frame != NULL);
	if (frame == NULL)
		return sh_css_err_internal_error;

	err = sh_css_frame_allocate_contiguous(frame,
						info->width,
						info->height,
						info->format,
						info->padded_width,
						info->raw_bit_depth);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_allocate_contiguous_from_info() leave:\n");
return err;
}

enum sh_css_err
sh_css_frame_map(struct sh_css_frame **frame,
                 const struct sh_css_frame_info *info,
                 const void *data,
                 uint16_t attribute,
                 void *context)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_frame *me = sh_css_malloc(sizeof(*me));

	if (me == NULL)
		return sh_css_err_cannot_allocate_memory;

	me->info.width = info->width;
	me->info.height = info->height;
	me->info.format = info->format;
	me->info.padded_width = info->padded_width;
	me->info.raw_bit_depth = info->raw_bit_depth;
	me->contiguous = false; /* doublecheck */
	me->dynamic_data_index = SH_CSS_INVALID_FRAME_ID;

	err = init_frame_planes(me);

	if (err == sh_css_success) {
		/* use mmgr_mmap to map */
		me->data = mmgr_mmap(
				     data,
				     me->data_bytes,
				     attribute,
				     context);
		if (me->data == mmgr_NULL)
			err = sh_css_err_invalid_arguments;
	};

	if (err != sh_css_success) {
		sh_css_free(me);
		return err;
	}

	*frame = me;

	return err;
}

void
sh_css_frame_free(struct sh_css_frame *frame)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_free() enter: frame=%p\n", frame);

	if (frame != NULL) {
		mmgr_free(frame->data);
		sh_css_free(frame);
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_frame_free() leave: return_void\n");
}

bool sh_css_frame_info_equal_resolution(
	const struct sh_css_frame_info *info_a,
	const struct sh_css_frame_info *info_b)
{
	if (!info_a || !info_b)
		return false;
	return (info_a->width == info_b->width) &&
	    (info_a->height == info_b->height);
}

bool sh_css_frame_equal_types(
	const struct sh_css_frame *frame_a,
	const struct sh_css_frame *frame_b)
{
	bool is_equal = false;
	const struct sh_css_frame_info *info_a = &frame_a->info,
	    *info_b = &frame_b->info;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_frame_equal_types() enter:\n");

	if (!info_a || !info_b)
		return false;
	if (info_a->format != info_b->format)
		return false;
	if (info_a->padded_width != info_b->padded_width)
		return false;
	is_equal = sh_css_frame_info_equal_resolution(info_a, info_b);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_frame_equal_types() leave:\n");
return is_equal;
}

static void
append_firmware(struct sh_css_fw_info **l, struct sh_css_fw_info *firmware)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "append_firmware() enter:\n");
	while (*l)
		l = &(*l)->next;
	*l = firmware;
	firmware->next = NULL;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "append_firmware() leave:\n");
}

static void
remove_firmware(struct sh_css_fw_info **l, struct sh_css_fw_info *firmware)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "remove_firmware() enter:\n");
	while (*l && *l != firmware)
		l = &(*l)->next;
	if (!*l)
		return;
	*l = firmware->next;
	firmware->next = NULL;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "remove_firmware() leave:\n");
}

/* Load firmware for acceleration */
enum sh_css_err
sh_css_load_acceleration(struct sh_css_acc_fw *firmware)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_load_acceleration() enter:\n");
	err = sh_css_acc_load(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_load_acceleration() leave:\n");
return err;
}

/* Unload firmware for acceleration */
void
sh_css_unload_acceleration(struct sh_css_acc_fw *firmware)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_unload_acceleration() enter:\n");
	sh_css_acc_unload(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_unload_acceleration() leave:\n");
}

/* Load firmware for extension */
enum sh_css_err
sh_css_pipe_load_extension(struct sh_css_pipe *pipe,
			   struct sh_css_fw_info *firmware)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_load_extension() enter:\n");
	if (firmware->info.isp.type == SH_CSS_ACC_OUTPUT)
		append_firmware(&pipe->output_stage, firmware);
	else if (firmware->info.isp.type == SH_CSS_ACC_VIEWFINDER)
		append_firmware(&pipe->vf_stage, firmware);
	err = sh_css_acc_load_extension(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_load_extension() leave:\n");
return err;
}

/* Unload firmware for extension */
void
sh_css_pipe_unload_extension(struct sh_css_pipe *pipe,
			     struct sh_css_fw_info *firmware)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_unload_extension() enter:\n");
	if (firmware->info.isp.type == SH_CSS_ACC_OUTPUT)
		remove_firmware(&pipe->output_stage, firmware);
	else if (firmware->info.isp.type == SH_CSS_ACC_VIEWFINDER)
		remove_firmware(&pipe->vf_stage, firmware);
	sh_css_acc_unload_extension(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_pipe_unload_extension() leave:\n");
}

/* Set acceleration parameter to value <val> */
enum sh_css_err
sh_css_set_acceleration_parameter(struct sh_css_acc_fw *firmware,
				  hrt_vaddress val, size_t size)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_hmm_section par = { val, size };
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_set_acceleration_parameter() enter:\n");
	err = sh_css_acc_set_parameter(firmware, par);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_set_acceleration_parameter() leave:\n");
return err;
}

/* Set acceleration parameters to value <val> */
enum sh_css_err
sh_css_set_firmware_dmem_parameters(struct sh_css_fw_info *firmware,
				    enum sh_css_isp_memories mem,
				  hrt_vaddress val, size_t size)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_hmm_section par = { val, size };
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_set_firmware_dmem_parameters() enter:\n");
	err = sh_css_acc_set_firmware_parameters(firmware, mem, par);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_set_firmware_dmem_parameters() leave:\n");
return err;
}

/* Start acceleration of firmware with sp-args as SP arguments. */
enum sh_css_err
sh_css_start_acceleration(struct sh_css_acc_fw *firmware)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_start_acceleration() enter:\n");
	my_css.curr_pipe = NULL;
	err = sh_css_acc_start(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_start_acceleration() leave:\n");
return err;
}

/* To be called when acceleration has terminated.
*/
void
sh_css_acceleration_done(struct sh_css_acc_fw *firmware)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_acceleration_done() enter: firmware=%p\n", firmware);
	sh_css_acc_wait();
	sh_css_acc_done(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_acceleration_done() leave: return_void\n");
}

/* Abort acceleration within <deadline> microseconds
*/
void
sh_css_abort_acceleration(struct sh_css_acc_fw *firmware, unsigned deadline)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_abort_acceleration() enter:\n");
	/* TODO: implement time-out */
	(void)deadline;
	sh_css_acc_abort(firmware);
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_abort_acceleration() leave:\n");
}

bool
sh_css_pipe_uses_params(struct sh_css_pipeline *me)
{
	struct sh_css_pipeline_stage *stage;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipe_uses_params() enter: me=%p\n", me);

	assert(me != NULL);
	if (me == NULL)
		return false;

	for (stage = me->stages; stage; stage = stage->next)
		if (stage->binary_info->enable.params) {
			sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
				"sh_css_pipe_uses_params() leave: "
				"return_bool=true\n");
			return true;
		}
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_pipe_uses_params() leave: return_bool=false\n");
	return false;
}

/* Create a pipeline stage for firmware <isp_fw>
 * with input and output arguments.
*/
static enum sh_css_err sh_css_create_stage(
	struct sh_css_pipeline_stage **stage,
	const char *isp_fw,
	struct sh_css_frame *in,
	struct sh_css_frame *out,
	struct sh_css_frame *vf)
{
	struct sh_css_binary *binary;
	struct sh_css_blob_descr *blob;
	struct sh_css_binary_info *info;
	unsigned size;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_create_stage() enter:\n");

	assert(stage != NULL);
	if (stage == NULL)
		return sh_css_err_internal_error;

	*stage = sh_css_malloc(sizeof(**stage));
	if (*stage == NULL) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_create_stage() leave: return_err=%d\n",
			sh_css_err_cannot_allocate_memory);
		return sh_css_err_cannot_allocate_memory;
	}

	binary = sh_css_malloc(sizeof(*binary));
	if (binary == NULL) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_create_stage() leave: return_err=%d\n",
			sh_css_err_cannot_allocate_memory);
		return sh_css_err_cannot_allocate_memory;
	}

	blob = sh_css_malloc(sizeof(*blob));
	if (blob == NULL) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_create_stage() leave: return_err=%d\n",
			sh_css_err_cannot_allocate_memory);
		return sh_css_err_cannot_allocate_memory;
	}

	memset(&(*stage)->args, 0, sizeof((*stage)->args));
	(*stage)->args.in_frame = in;
	(*stage)->args.out_frame = out;
	(*stage)->args.out_vf_frame = vf;

	err = sh_css_load_blob_info(isp_fw, blob);
	if (err != sh_css_success) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_create_stage() leave: return_err=%d\n",
			err);
		return err;
	}
	err = sh_css_fill_binary_info(&blob->header.info.isp, false, false,
			    SH_CSS_INPUT_FORMAT_RAW_10,
			    in  ? &in->info  : NULL,
			    out ? &out->info : NULL,
			    vf  ? &vf->info  : NULL,
			    binary, false);
	if (err != sh_css_success) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_create_stage() leave: return_err=%d\n",
			err);
		return err;
	}
	blob->header.info.isp.xmem_addr = 0;
	size = blob->header.blob.size;
	if (size) {
		blob->header.info.isp.xmem_addr =
			sh_css_load_blob(blob->blob, size);
		if (!blob->header.info.isp.xmem_addr) {
			sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
				"sh_css_create_stage() leave: return_err=%d\n",
				sh_css_err_cannot_allocate_memory);
			return sh_css_err_cannot_allocate_memory;
		}
	}

	info = (struct sh_css_binary_info *)binary->info;
	info->blob = blob;
	(*stage)->binary = binary;
	(*stage)->binary_info = &blob->header.info.isp;
	(*stage)->firmware = NULL;
	(*stage)->mode = binary->info->mode;
	(*stage)->out_frame_allocated = false;
	(*stage)->vf_frame_allocated = false;
	(*stage)->next = NULL;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_create_stage() leave: return_err=%d\n", err);

	return err;
}

/* Append a new stage to *pipeline. When *pipeline is NULL, it will be created.
 * The stage consists of an ISP binary <isp_fw> and input and output arguments.
*/
enum sh_css_err sh_css_append_stage(
	void **me,
	const char *isp_fw,
	struct sh_css_frame *in,
	struct sh_css_frame *out,
	struct sh_css_frame *vf)
{
	struct sh_css_pipeline **pipeline = (struct sh_css_pipeline **)me;
	struct sh_css_pipeline_stage *stage;
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_append_stage() enter: "
		"me=%p, isp_f%s, in=%p, out=%p, vf=%p\n",
		me, isp_fw, in, out, vf);

	if (!*pipeline) {
		*pipeline = (struct sh_css_pipeline *)sh_css_create_pipeline();
		if (*pipeline == NULL) {

			sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
				"sh_css_append_stage() leave: return_err=%d\n",
				sh_css_err_cannot_allocate_memory);
			return sh_css_err_cannot_allocate_memory;
		}
	}

	err = sh_css_create_stage(&stage, isp_fw, in, out, vf);
	if (err != sh_css_success) {
		sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
			"sh_css_append_stage() leave: return_err=%d\n", err);

		return err;
	}

	stage->stage_num = (*pipeline)->num_stages++;
	if ((*pipeline)->current_stage)
		(*pipeline)->current_stage->next = stage;
	else
		(*pipeline)->stages = stage;

	(*pipeline)->current_stage = stage;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_append_stage() leave: return_err=%d\n", err);

	return err;
}

/* #error return of function is not consistent with implementation */
void *sh_css_create_pipeline(void)
{
	struct sh_css_pipeline *pipeline = sh_css_malloc(sizeof(struct sh_css_pipeline));

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_create_pipeline() enter:\n");

	if (pipeline != NULL) {
		pipeline->num_stages = 0;
		pipeline->stages = NULL;
		pipeline->current_stage = NULL;
	}

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_create_pipeline() leave: pipeline=%p\n", pipeline);
return (void *)pipeline;
}

enum sh_css_err sh_css_pipeline_add_acc_stage(
	void		*pipeline,
	const void	*acc_fw)
{
	struct sh_css_fw_info *fw = (struct sh_css_fw_info *)acc_fw;
	enum sh_css_err	err = sh_css_acc_load_extension(fw);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipeline_add_acc_stage() enter: pipeline=%p,"
		" acc_fw=%p\n", pipeline, acc_fw);

	if (err == sh_css_success) {
		err = sh_css_pipeline_add_stage(
			pipeline, NULL, fw,
			SH_CSS_BINARY_MODE_VF_PP, NULL,
			NULL, NULL,
			NULL,NULL);
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_pipeline_add_acc_stage() leave: return_err=%d\n",err);
return err;
}

void sh_css_destroy_pipeline(
	void		*pipeline)
{
	struct sh_css_pipeline *pipeline_loc = pipeline;
	struct sh_css_pipeline_stage *stage, *next = NULL;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_destroy_pipeline() enter: pipeline=%p\n", pipeline);

	for (stage = pipeline_loc->stages; (stage != NULL); stage = next) {
		struct sh_css_fw_info *fw = (struct sh_css_fw_info *)stage->firmware;

		next = stage->next;

		sh_css_acc_unload_extension(fw);
		sh_css_pipeline_stage_destroy(stage);
	}
	sh_css_free(pipeline_loc);

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_destroy_pipeline() leave: return_void\n");
return;
}

/* Run a pipeline and wait till it completes. */
void
sh_css_start_pipeline(enum sh_css_pipe_id pipe_id, void *me)
{
	struct sh_css_pipeline *pipeline = me;
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_start_pipeline() enter: pipe_id=%d, me=%p\n",
		pipe_id, me);
	pipeline->pipe_id = pipe_id;
	sh_css_sp_init_pipeline(pipeline, pipe_id,
				false, true, false, false, false, true, false,
				SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD);
	if (sh_css_pipe_uses_params(pipeline)) {
		sh_css_pipeline_stream_clear_pipelines();
		sh_css_pipeline_stream_add_pipeline(pipeline);
	}
	sh_css_sp_start_isp();
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_start_pipeline() leave: return_void\n");
}

/* Run a pipeline and free all memory allocated to it. */
void
sh_css_close_pipeline(void *me)
{
	struct sh_css_pipeline *pipeline = me;
	struct sh_css_pipeline_stage *stage;
	struct sh_css_pipeline_stage *next;

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE, "sh_css_close_pipeline() enter: "
		"pipeline=%p\n", pipeline);

	for (stage = pipeline->stages; stage; stage = next) {
		struct sh_css_blob_descr *blob;
		next = stage->next;
		blob = (struct sh_css_blob_descr *)stage->binary->info->blob;
		if (blob->header.info.isp.xmem_addr)
			mmgr_free(blob->header.info.isp.xmem_addr);
		sh_css_free(blob);
		sh_css_free(stage->binary);
		sh_css_pipeline_stage_destroy(stage);
	}
	sh_css_free(pipeline);

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_close_pipeline() leave: return_void\n");
}

/* Run an isp binary <isp_fw> with input, output and vf frames
*/
enum sh_css_err sh_css_run_isp_firmware(
	const char *isp_fw,
	struct sh_css_frame *in,
	struct sh_css_frame *out,
	struct sh_css_frame *vf)
{
	void *pipeline = NULL;
	enum sh_css_err err;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_run_isp_firmware() enter: isp_fw=%p,"
		" in=%p, out=%p, vf=%p\n", isp_fw,in, out, vf);

	err = sh_css_append_stage(&pipeline, isp_fw, in, out, vf);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_run_isp_firmware() leave: return_err=%d\n",err);
		return err;
	}
	sh_css_start_pipeline(SH_CSS_ACC_PIPELINE, pipeline);
	/* TODO: the following line must be changed if
		someone want to use this function */
	err = sh_css_wait_for_completion(SH_CSS_ACC_PIPELINE);
	if (err != sh_css_success) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_run_isp_firmware() leave: return_err=%d\n",err);
		return err;
	}
	sh_css_close_pipeline(pipeline);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_run_isp_firmware() leave: return_err=%d\n",err);
	return err;
}

/**
 * @brief Query the SP thread ID.
 * Refer to "sh_css_internal.h" for details.
 */
bool
sh_css_query_sp_thread_id(enum sh_css_pipe_id key,
		unsigned int *val)
{

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_query_sp_thread_id() enter: key=%d\n", key);

	assert(key < SH_CSS_NR_OF_PIPELINES);
	if (key >= SH_CSS_NR_OF_PIPELINES)
		return false;
	assert(val != NULL);
	if (val == NULL)
		return false;

	*val = sh_css_pipe_id_2_internal_thread_id[key];

	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_query_sp_thread_id() leave: return_val=%d\n", *val);
	return true;
}

/**
 * @brief Query the internal frame ID.
 * Refer to "sh_css_internal.h" for details.
 */
bool sh_css_query_internal_queue_id(
	enum sh_css_buffer_type key,
	enum sh_css_buffer_queue_id *val)
{
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_query_internal_queue_id() enter: key=%d\n", key);

	assert(key < SH_CSS_BUFFER_TYPE_NR_OF_TYPES);
	if (key >= SH_CSS_BUFFER_TYPE_NR_OF_TYPES)
		return false;

	assert(val != NULL);
	if (val == NULL)
		return false;

	*val = sh_css_buf_type_2_internal_queue_id[key];
	sh_css_dtrace(SH_DBG_TRACE_PRIVATE,
		"sh_css_query_internal_queue_id() leave: return_val=%d\n",
		*val);
	return true;
}

/**
 * @brief Tag a specific frame in continuous capture.
 * Refer to "sh_css_internal.h" for details.
 */
enum sh_css_err sh_css_offline_capture_tag_frame(
	unsigned int exp_id)
{
	struct sh_css_tag_descr tag_descr;
	unsigned int encoded_tag_descr;

	bool enqueue_successful = false;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_tag_frame() enter: exp_id=%d\n",
		exp_id);

	if (exp_id == 0) {
		sh_css_dtrace(SH_DBG_TRACE,
			"sh_css_offline_capture_tag_frame() "
			"leave: return_err=%d\n",
			sh_css_err_invalid_tag_description);
		return sh_css_err_invalid_tag_description;
	}

	/* Create the tag descriptor from the parameters */
	sh_css_create_tag_descr(0, 0, 0, exp_id, &tag_descr);


	/* Encode the tag descriptor into a 32-bit value */
	encoded_tag_descr = sh_css_encode_tag_descr(&tag_descr);


	/* Enqueue the encoded tag to the host2sp queue.
	 * Note: The pipe and stage IDs for tag_cmd queue are hard-coded to 0
	 * on both host and the SP side.
	 * It is mainly because it is enough to have only one tag_cmd queue */
	enqueue_successful = host2sp_enqueue_buffer(0, 0,
				sh_css_tag_cmd_queue,
				(uint32_t)encoded_tag_descr);


	/* Give an error if the tag command cannot be issued
	 * (because the cmd queue is full) */
	if (!enqueue_successful) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_tag_frame() leave: return_err=%d\n",
		sh_css_err_tag_queue_is_full);
		return sh_css_err_tag_queue_is_full;
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_tag_frame() leave: return_err=%d\n",
		sh_css_success);
	return sh_css_success;
}

/**
 * @brief Configure the continuous capture.
 * Refer to "sh_css_internal.h" for details.
 */
enum sh_css_err sh_css_offline_capture_configure(
	int num_captures,
	unsigned int skip,
	int offset)
{
	struct sh_css_tag_descr tag_descr;
	unsigned int encoded_tag_descr;

	bool enqueue_successful = false;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_configure() enter: num_captures=%d,"
		" skip=%d, offset=%d\n", num_captures, skip,offset);

	/* Check if the tag descriptor is valid */
	if (num_captures < SH_CSS_MINIMUM_TAG_ID) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_configure() leave: return_err=%d\n",
		sh_css_err_invalid_tag_description);
		return sh_css_err_invalid_tag_description;
	}

	/* Create the tag descriptor from the parameters */
	sh_css_create_tag_descr(num_captures, skip, offset, 0, &tag_descr);


	/* Encode the tag descriptor into a 32-bit value */
	encoded_tag_descr = sh_css_encode_tag_descr(&tag_descr);


	/* Enqueue the encoded tag to the host2sp queue.
	 * Note: The pipe and stage IDs for tag_cmd queue are hard-coded to 0
	 * on both host and the SP side.
	 * It is mainly because it is enough to have only one tag_cmd queue */
	enqueue_successful = host2sp_enqueue_buffer(0, 0,
				sh_css_tag_cmd_queue,
				(uint32_t)encoded_tag_descr);


	/* Give an error if the tag command cannot be issued
	 * (because the cmd queue is full) */
	if (!enqueue_successful) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_configure() leave: return_err=%d\n",
		sh_css_err_tag_queue_is_full);
		return sh_css_err_tag_queue_is_full;
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_offline_capture_configure() leave: return_err=%d\n",
		sh_css_success);
	return sh_css_success;
}

/* MW: This function does not invalidate the TLB, it sends an indicate to do so, better change the name */
void sh_css_enable_sp_invalidate_tlb(void)
{
	const struct sh_css_fw_info *fw = &sh_css_sp_fw;
	unsigned int HIVE_ADDR_sp_invalidate_tlb;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_enable_sp_invalidate_tlb() enter: void\n");
	HIVE_ADDR_sp_invalidate_tlb = fw->info.sp.invalidate_tlb;

	(void)HIVE_ADDR_sp_invalidate_tlb; /* Suppres warnings in CRUN */

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_invalidate_tlb),
		1);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_enable_sp_invalidate_tlb() leave: return_void\n");
}

void sh_css_request_flash(void)
{
	const struct sh_css_fw_info *fw= &sh_css_sp_fw;
	unsigned int HIVE_ADDR_sp_request_flash;

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_request_flash() enter: void\n");
	HIVE_ADDR_sp_request_flash = fw->info.sp.request_flash;

	(void)HIVE_ADDR_sp_request_flash;

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_request_flash),
		1);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_request_flash() leave: return_void\n");
}

/* CSS 1.5 wrapper */
enum sh_css_err
sh_css_preview_stop(void)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_preview_stop() enter: void\n");
	err = sh_css_pipe_stop(&my_css.preview_pipe);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_stop() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err
sh_css_video_stop(void)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_video_stop() enter: void\n");
	err = sh_css_pipe_stop(&my_css.video_pipe);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_stop() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err
sh_css_capture_stop(void)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_capture_stop() enter: void\n");
	err = sh_css_pipe_stop(&my_css.capture_pipe);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_stop() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err
sh_css_acceleration_stop(void)
{
	enum sh_css_err err = sh_css_success;
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_acceleration_stop() enter: void\n");
	err = sh_css_pipeline_stop(SH_CSS_ACC_PIPELINE);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_acceleration_stop() leave: return_err=%d\n",err);
return err;
}

void
sh_css_capture_enable_online(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_enable_online() enter: enable=%d\n", enable);
	sh_css_pipe_enable_online(&my_css.capture_pipe, enable);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_enable_online() leave: return_void\n");
}

void
sh_css_video_set_enable_dz(bool enable_dz)
{
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_set_enable_dz() enter: enable_dz=%d\n",enable_dz);
	pipe->enable_dz = enable_dz;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_set_enable_dz() leave: return_void\n");
}

void
sh_css_video_get_enable_dz(bool *enable_dz)
{
	struct sh_css_pipe *pipe = &my_css.video_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_enable_dz() enter: void\n");

	if (enable_dz != NULL) {
		*enable_dz = pipe->enable_dz;
		sh_css_dtrace(SH_DBG_TRACE,
			"sh_css_video_get_enable_dz() leave: enable_dz=%d\n",
			*enable_dz);
	} else {
		sh_css_dtrace(SH_DBG_TRACE,
			"sh_css_video_get_enable_dz() leave: "
			"enable_dz=UNDEFINED (pipe has no DZ)\n");
	}
}

enum sh_css_err sh_css_preview_configure_output(
	unsigned int width, unsigned int height, unsigned int min_padded_width,
	enum sh_css_frame_format format)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_configure_output() enter: "
		"width=%d, height=%d, min_padded_width=%d, format=%d\n",
		width, height, min_padded_width, format);

	err = sh_css_pipe_configure_output(&my_css.preview_pipe, width,
					   height, min_padded_width, format);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_configure_output() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err sh_css_capture_configure_output(
	unsigned int width, unsigned int height, unsigned int min_padded_width,
	enum sh_css_frame_format format)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_configure_output() enter: "
		"width=%d, height=%d, min_padded_width=%d,  format=%d\n",
		width, height, min_padded_width, format);

	err = sh_css_pipe_configure_output(&my_css.capture_pipe, width,
					   height, min_padded_width, format);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_configure_output() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err sh_css_video_configure_output(
	unsigned int width, unsigned int height, unsigned int min_padded_width,
	enum sh_css_frame_format format)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_configure_output() enter: "
		"width=%d, height=%d, min_padded_width=%d, format=%d\n",
		width, height, min_padded_width, format);

	err = sh_css_pipe_configure_output(&my_css.video_pipe, width,
					   height, min_padded_width, format);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_configure_output() leave: return_err=%d\n", err);
return err;
}

enum sh_css_err sh_css_capture_get_output_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_output_frame_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_get_output_frame_info(&my_css.capture_pipe,
						 info);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_output_frame_info() leave: "
		"return_err=%d, "
		"info.width=%d, info.height=%d, "
		"info.padded_width=%d, info.format=%d, "
		"info.raw_bit_depth=%d, info.raw_bayer_order=%d\n",
		err,
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);

return err;
}

enum sh_css_err sh_css_preview_get_output_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_get_output_frame_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_get_output_frame_info(&my_css.preview_pipe,
						 info);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_get_output_frame_info() leave: "
		"return_err=%d, "
		"info.width=%d, info.height=%d, "
		"info.padded_width=%d, info.format=%d, "
		"info.raw_bit_depth=%d, info.raw_bayer_order=%d\n",
		err,
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);

return err;
}

enum sh_css_err sh_css_video_get_output_frame_info(
	struct sh_css_frame_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_output_frame_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_get_output_frame_info(&my_css.video_pipe,
						 info);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_output_frame_info() leave: "
		"return_err=%d, "
		"info.width=%d, info.height=%d, "
		"info.padded_width=%d, info.format=%d, "
		"info.raw_bit_depth=%d, info.raw_bayer_order=%d\n",
		err,
		info->width,info->height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);

return err;
}

enum sh_css_err sh_css_preview_get_grid_info(
	struct sh_css_grid_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_get_grid_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_get_grid_info(&my_css.preview_pipe,
					 info);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_get_grid_info() leave: &info=%p\n", info);

return err;
}

enum sh_css_err sh_css_video_get_grid_info(
	struct sh_css_grid_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_grid_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_get_grid_info(&my_css.video_pipe,
					 info);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_get_grid_info() leave: &info=%p\n", info);

return err;
}

enum sh_css_err sh_css_capture_get_grid_info(
	struct sh_css_grid_info *info)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_grid_info() enter: void\n");

	assert(info != NULL);
	if (info == NULL)
		return sh_css_err_internal_error;

	err = sh_css_pipe_get_grid_info(&my_css.capture_pipe,
					 info);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_capture_get_grid_info() leave: &info=%p\n", info);


return err;
}

void
sh_css_preview_enable_online(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_enable_online() enter: enable=%d\n", enable);

	sh_css_pipe_enable_online(&my_css.preview_pipe, enable);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_preview_enable_online() leave: return_void\n");

}

void
sh_css_video_enable_online(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_online() enter: enable=%d\n", enable);

	sh_css_pipe_enable_online(&my_css.video_pipe, enable);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_enable_online() leave: return_void\n");
}

void
sh_css_input_set_channel(unsigned int channel_id)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_channel() enter: channel_id%d\n", channel_id);

	sh_css_pipe_set_input_channel(&my_css.preview_pipe, channel_id);
	sh_css_pipe_set_input_channel(&my_css.capture_pipe, channel_id);
	sh_css_pipe_set_input_channel(&my_css.video_pipe, channel_id);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_channel() leave: return_void\n");
}

void
sh_css_input_set_format(enum sh_css_input_format format)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_format() enter: format%d\n", format);

	sh_css_debug_pipe_graph_dump_input_set_format(format);
	sh_css_pipe_set_input_format(&my_css.preview_pipe, format);
	sh_css_pipe_set_input_format(&my_css.video_pipe, format);
	sh_css_pipe_set_input_format(&my_css.capture_pipe, format);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_format() leave: return_void\n");

}

void
sh_css_input_get_format(enum sh_css_input_format *format)
{
	sh_css_dtrace(SH_DBG_TRACE, "sh_css_input_get_format() enter: void\n");

	assert(format != NULL);
	if (format == NULL)
		return;

	/* arbitrarily pick preview, they are all the same */
	*format = sh_css_pipe_get_input_format(&my_css.preview_pipe);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_get_format() leave: format=%d\n", *format);
}

void
sh_css_input_set_mode(enum sh_css_input_mode mode)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_mode() enter: mode=%d\n",
		mode);

	sh_css_pipe_set_input_mode(&my_css.preview_pipe, mode);
	sh_css_pipe_set_input_mode(&my_css.video_pipe, mode);
	sh_css_pipe_set_input_mode(&my_css.capture_pipe, mode);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_mode() leave: return_void\n");

}

void
sh_css_input_set_two_pixels_per_clock(bool two_ppc)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_two_pixels_per_clock() enter: two_ppc=%d\n",
		two_ppc);

	sh_css_pipe_set_two_pixels_per_clock(&my_css.preview_pipe, two_ppc);
	sh_css_pipe_set_two_pixels_per_clock(&my_css.video_pipe, two_ppc);
	sh_css_pipe_set_two_pixels_per_clock(&my_css.capture_pipe, two_ppc);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_two_pixels_per_clock() leave: return_void\n");
}

void
sh_css_input_get_two_pixels_per_clock(bool *two_ppc)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_get_two_pixels_per_clock() enter: void\n");

	assert(two_ppc != NULL);
	if (two_ppc == NULL)
		return;

	*two_ppc = sh_css_pipe_get_two_pixels_per_clock(&my_css.preview_pipe);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_get_two_pixels_per_clock() leave: two_ppc=%d\n",
		*two_ppc);
}

void
sh_css_input_set_bayer_order(enum sh_css_bayer_order bayer_order)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_bayer_order() enter: "
		"bayer_order=%d\n", bayer_order);

	sh_css_pipe_set_input_bayer_order(&my_css.preview_pipe, bayer_order);
	sh_css_pipe_set_input_bayer_order(&my_css.video_pipe, bayer_order);
	sh_css_pipe_set_input_bayer_order(&my_css.capture_pipe, bayer_order);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_bayer_order() leave: return_void\n");
}

void
sh_css_get_extra_pixels_count(int *extra_rows, int *extra_cols)
{
	/* arbitrarily pick preview */
	struct sh_css_pipe *pipe = &my_css.preview_pipe;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_get_extra_pixels_count() enter: void\n");

	assert(extra_rows != NULL);
	if (extra_rows == NULL)
		return;

	assert(extra_cols != NULL);
	if (extra_cols == NULL)
		return;

	sh_css_pipe_get_extra_pixels_count(pipe, extra_rows, extra_cols);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_get_extra_pixels_count() leave: "
		"extra_rows=%d, extra_cols=%d\n", *extra_rows, *extra_cols);
}

void
sh_css_disable_vf_pp(bool disable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_disable_vf_pp() enter: disable=%d\n", disable);

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_disable_vf_pp()\n");
	sh_css_pipe_disable_vf_pp(&my_css.preview_pipe, disable);
	sh_css_pipe_disable_vf_pp(&my_css.video_pipe, disable);
	sh_css_pipe_disable_vf_pp(&my_css.capture_pipe, disable);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_disable_vf_pp() leave: return_void\n");
}

void
sh_css_enable_raw_binning(bool enable)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_enable_raw_binning() enter: enable=%d\n", enable);
	sh_css_pipe_enable_raw_binning(&my_css.preview_pipe, enable);
	sh_css_pipe_enable_raw_binning(&my_css.video_pipe, enable);
	sh_css_pipe_enable_raw_binning(&my_css.capture_pipe, enable);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_enable_raw_binning() leave: return_void\n");

}

enum sh_css_err
sh_css_input_configure_port(const mipi_port_ID_t port,
			    const unsigned int	 num_lanes,
			    const unsigned int	 timeout)
{
	enum sh_css_err err;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_configure_port() enter: "
		"port=%d, ""num_lanes=%d, timeout=%d\n",
		port, num_lanes, timeout);

	/* if one fails, all fail, so just check the last result */
	sh_css_pipe_configure_input_port(&my_css.preview_pipe, port,
					 num_lanes, timeout);
	sh_css_pipe_configure_input_port(&my_css.capture_pipe, port,
					 num_lanes, timeout);
	err = sh_css_pipe_configure_input_port(&my_css.video_pipe, port,
					 num_lanes, timeout);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_configure_port() leave: return_err=%d\n",err);
	return err;
}

enum sh_css_err
sh_css_input_set_compression(const mipi_predictor_t comp,
			     const unsigned int     compressed_bits_per_pixel,
			     const unsigned int     uncompressed_bits_per_pixel)
{
	enum sh_css_err err;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_compression() enter: "
		"comp=%d, ""comp_bpp=%d, uncomp_bpp=%d\n",
		comp, compressed_bits_per_pixel, uncompressed_bits_per_pixel);

	/* if one fails, all fail, so just check the last result */
	sh_css_pipe_set_compression(&my_css.preview_pipe, comp,
				    compressed_bits_per_pixel,
				    uncompressed_bits_per_pixel);
	sh_css_pipe_set_compression(&my_css.video_pipe, comp,
				    compressed_bits_per_pixel,
				    uncompressed_bits_per_pixel);
	err = sh_css_pipe_set_compression(&my_css.capture_pipe, comp,
				    compressed_bits_per_pixel,
				    uncompressed_bits_per_pixel);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_compression() leave: return_err=%d\n",err);

	return err;

}

enum sh_css_err sh_css_load_extension(
	struct sh_css_fw_info	*fw,
	enum sh_css_pipe_id		pipe_id,
	enum sh_css_acc_type	acc_type)
{
	enum sh_css_err err;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_load_extension() enter: "
		"fw=%p, pipe_id=%d, acc_type=%d\n",fw, pipe_id, acc_type);

	assert(fw != NULL);
	if (fw == NULL)
		return sh_css_err_internal_error;

	assert(pipe_id < SH_CSS_NR_OF_PIPELINES);
	if (pipe_id >= SH_CSS_NR_OF_PIPELINES)
		return sh_css_err_internal_error;

	assert((acc_type == SH_CSS_ACC_OUTPUT) ||
			(acc_type == SH_CSS_ACC_VIEWFINDER));

/* MW: Legacy from CSS 1.0, where the acc_type was encoded on FW creation; set */
	fw->info.isp.type = acc_type;

	switch (pipe_id) {
		case  SH_CSS_PREVIEW_PIPELINE:
			err = sh_css_pipe_load_extension(&my_css.preview_pipe, fw);
		break;
		case  SH_CSS_COPY_PIPELINE:
			err = sh_css_err_not_implemented;
		break;
		case  SH_CSS_VIDEO_PIPELINE:
			err = sh_css_pipe_load_extension(&my_css.video_pipe, fw);
		break;
		case  SH_CSS_CAPTURE_PIPELINE:
			err = sh_css_pipe_load_extension(&my_css.capture_pipe, fw);
		break;
		default:
			err = sh_css_err_not_implemented;
		break;
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_load_extension() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err sh_css_unload_extension(
	struct sh_css_fw_info	*fw,
	enum sh_css_pipe_id		pipe_id)
{
	enum sh_css_err err = sh_css_success;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_unload_extension() enter: fw=%p, pipe_id=%d\n", fw, pipe_id);

	assert(fw != NULL);
	if (fw == NULL)
		return sh_css_err_internal_error;

	assert(pipe_id < SH_CSS_NR_OF_PIPELINES);
	if (pipe_id >= SH_CSS_NR_OF_PIPELINES)
		return sh_css_err_internal_error;

	switch (pipe_id) {
		case  SH_CSS_PREVIEW_PIPELINE:
			sh_css_pipe_unload_extension(&my_css.preview_pipe, fw);
		break;
		case  SH_CSS_COPY_PIPELINE:
			err = sh_css_err_not_implemented;
		break;
		case  SH_CSS_VIDEO_PIPELINE:
			sh_css_pipe_unload_extension(&my_css.video_pipe, fw);
		break;
		case  SH_CSS_CAPTURE_PIPELINE:
			sh_css_pipe_unload_extension(&my_css.capture_pipe, fw);
		break;
		default:
			err = sh_css_err_not_implemented;
		break;
	}

/* MW: Legacy from CSS 1.0, where the acc_type was encoded on FW creation; rst */
	fw->info.isp.type = SH_CSS_ACC_NONE;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_unload_extension() leave: return_err=%d\n",err);
return err;
}

enum sh_css_err
sh_css_input_set_effective_resolution(unsigned int width, unsigned int height)
{
	enum sh_css_err err;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_effective_resolution() "
		"enter: width=%d, height=%d\n",width, height);
	sh_css_debug_pipe_graph_dump_input_set_effective_resolution(width, height);
	sh_css_pipe_set_effective_input_resolution(&my_css.preview_pipe, width, height);
	sh_css_pipe_set_effective_input_resolution(&my_css.video_pipe, width, height);
	err = sh_css_pipe_set_effective_input_resolution(&my_css.capture_pipe, width, height);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_effective_resolution() "
		"leave: return_err=%d\n",err);
	return err;
}

enum sh_css_err
sh_css_input_set_resolution(unsigned int width, unsigned int height)
{
	enum sh_css_err err;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_resolution() enter: width=%d, height=%d\n",
		width, height);
	sh_css_debug_pipe_graph_dump_input_set_resolution(width, height);
	sh_css_pipe_set_input_resolution(&my_css.preview_pipe, width, height);
	sh_css_pipe_set_input_resolution(&my_css.video_pipe, width, height);
	err = sh_css_pipe_set_input_resolution(&my_css.capture_pipe, width, height);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_input_set_resolution() leave: return_err=%d\n",err);
	return err;
}

void
sh_css_video_set_isp_pipe_version(unsigned int version)
{
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_set_isp_pipe_version() enter: version=%d\n",
		version);
	my_css.video_pipe.isp_pipe_version = version;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_video_set_isp_pipe_version() leave: return=void\n");
}

enum sh_css_err
sh_css_event_set_irq_mask(
	enum sh_css_pipe_id pipe_id,
	unsigned int or_mask,
	unsigned int and_mask)
{
	my_css.sp_irq_mask[pipe_id].or_mask = or_mask;
	my_css.sp_irq_mask[pipe_id].and_mask = and_mask;

	return sh_css_success;
}

enum sh_css_err
sh_css_event_get_irq_mask(
	enum sh_css_pipe_id pipe_id,
	unsigned int *or_mask,
	unsigned int *and_mask)
{
	const struct sh_css_fw_info *fw = &sh_css_sp_fw;
	unsigned int HIVE_ADDR_host_sp_com = fw->info.sp.host_sp_com;
	unsigned int offset;
	struct sh_css_event_irq_mask event_irq_mask;

	(void)HIVE_ADDR_host_sp_com; /* Suppres warnings in CRUN */

	sh_css_dtrace(SH_DBG_TRACE, "sh_css_event_get_irq_mask()\n");

	assert(SH_CSS_NR_OF_PIPELINES == NR_OF_PIPELINES);
	assert(pipe_id < SH_CSS_NR_OF_PIPELINES);
	if (pipe_id >= SH_CSS_NR_OF_PIPELINES)
		return sh_css_err_internal_error;

	offset = offsetof(struct host_sp_communication,
					host2sp_event_irq_mask[pipe_id]);
	assert(offset % HRT_BUS_BYTES == 0);
	sp_dmem_load(SP0_ID,
		(unsigned int)sp_address_of(host_sp_com) + offset,
		&event_irq_mask, sizeof(event_irq_mask));

	if (or_mask)
		*or_mask = event_irq_mask.or_mask;

	if (and_mask)
		*and_mask = event_irq_mask.and_mask;

	return sh_css_success;
}

void
sh_css_event_init_irq_mask(void)
{
	int i;
	const struct sh_css_fw_info *fw = &sh_css_sp_fw;
	unsigned int HIVE_ADDR_host_sp_com = fw->info.sp.host_sp_com;
	unsigned int offset;


	(void)HIVE_ADDR_host_sp_com; /* Suppres warnings in CRUN */

	/*assert(sizeof(event_irq_mask_init) % HRT_BUS_BYTES == 0);*/
	for (i = 0; i < SH_CSS_NR_OF_PIPELINES; i++) {
		offset = offsetof(struct host_sp_communication,
						host2sp_event_irq_mask[i]);
		assert(offset % HRT_BUS_BYTES == 0);
		sp_dmem_store(SP0_ID,
			(unsigned int)sp_address_of(host_sp_com) + offset,
			&my_css.sp_irq_mask[i], sizeof(struct sh_css_event_irq_mask));
	}

}

void
sh_css_init_host_sp_control_vars(void)
{
	const struct sh_css_fw_info *fw;
	unsigned int HIVE_ADDR_sp_isp_started;

	unsigned int HIVE_ADDR_host_sp_queues_initialized;
	unsigned int HIVE_ADDR_sp_sleep_mode;
	unsigned int HIVE_ADDR_sp_invalidate_tlb;
	unsigned int HIVE_ADDR_sp_request_flash;
	unsigned int HIVE_ADDR_sp_stop_copy_preview;
	unsigned int HIVE_ADDR_sp_copy_pack;
	unsigned int HIVE_ADDR_host_sp_com;
	unsigned int o = offsetof(struct host_sp_communication, host2sp_command)
				/ sizeof(int);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_init_host_sp_control_vars() enter:\n");

	fw = &sh_css_sp_fw;
	HIVE_ADDR_sp_isp_started = fw->info.sp.isp_started;

	HIVE_ADDR_host_sp_queues_initialized =
		fw->info.sp.host_sp_queues_initialized;
	HIVE_ADDR_sp_sleep_mode = fw->info.sp.sleep_mode;
	HIVE_ADDR_sp_invalidate_tlb = fw->info.sp.invalidate_tlb;
	HIVE_ADDR_sp_request_flash = fw->info.sp.request_flash;
	HIVE_ADDR_sp_stop_copy_preview = fw->info.sp.stop_copy_preview;
	HIVE_ADDR_sp_copy_pack = fw->info.sp.copy_pack;
	HIVE_ADDR_host_sp_com = fw->info.sp.host_sp_com;

	(void)HIVE_ADDR_sp_isp_started; /* Suppres warnings in CRUN */

	(void)HIVE_ADDR_sp_sleep_mode;
	(void)HIVE_ADDR_sp_invalidate_tlb;
	(void)HIVE_ADDR_sp_request_flash;
	(void)HIVE_ADDR_sp_stop_copy_preview;
	(void)HIVE_ADDR_sp_copy_pack;
	(void)HIVE_ADDR_host_sp_com;

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_isp_started),
		(uint32_t)(0));

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(host_sp_queues_initialized),
		(uint32_t)(0));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_sleep_mode),
		(uint32_t)(0));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_invalidate_tlb),
		(uint32_t)(0));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_request_flash),
		(uint32_t)(0));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_stop_copy_preview),
		my_css.stop_copy_preview?(uint32_t)(1):(uint32_t)(0));
	store_sp_array_uint(host_sp_com, o, host2sp_cmd_ready);

	sh_css_event_init_irq_mask();
	if (my_css.continuous) {
		int i;
		struct sh_css_pipe *pipe = &my_css.preview_pipe;
		sh_css_update_host2sp_cont_num_raw_frames
			(NUM_OFFLINE_INIT_CONTINUOUS_FRAMES, true);
		sh_css_update_host2sp_cont_num_raw_frames
			(my_css.num_cont_raw_frames, false);
		if (pipe->mode == SH_CSS_PREVIEW_PIPELINE)
			for (i = 0; i < NUM_OFFLINE_INIT_CONTINUOUS_FRAMES; i++) {
				sh_css_update_host2sp_offline_frame(i,
					pipe->pipe.preview.continuous_frames[i]);
			}
#if 0
		else if (pipe->mode == SH_CSS_CAPTURE_PIPELINE)
			for (i = 0; i < NUM_CONTINUOUS_FRAMES; i++) {
				sh_css_update_host2sp_offline_frame(i,
					pipe->pipe.capture.continuous_frames[i]);
			}
#endif
	}
	if (my_css.continuous && (my_css.curr_state != sh_css_state_executing_sp_bin_copy)) {
		sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_copy_pack),
		(uint32_t)(1));
	} else {
		sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_copy_pack),
		(uint32_t)(0));
	}

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_init_host_sp_control_vars() leave: return_void\n");
}

enum sh_css_err
sh_css_allocate_continuous_frames(
	bool init_time)
{
	enum sh_css_err err = sh_css_success;
	struct sh_css_frame_info ref_info;
	struct sh_css_pipe *pipe = &my_css.preview_pipe;
	bool input_needs_raw_binning = pipe->input_needs_raw_binning;
	unsigned int i, idx;
	unsigned int left_cropping, top_cropping;
	unsigned int num_frames;
	left_cropping = pipe->pipe.preview.preview_binary.info->left_cropping;
	top_cropping = pipe->pipe.preview.preview_binary.info->top_cropping;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_allocate_continuous_frames() enter: init_time=%d\n", init_time);

	if (my_css.continuous) {
		if (init_time)
			num_frames = NUM_OFFLINE_INIT_CONTINUOUS_FRAMES;
		else
			num_frames = my_css.num_cont_raw_frames;
	} else {
		num_frames = NUM_ONLINE_INIT_CONTINUOUS_FRAMES;
	}

	ref_info = pipe->pipe.preview.preview_binary.in_frame_info;
	if (input_needs_raw_binning &&
	    pipe->pipe.preview.preview_binary.info->enable.raw_binning) {
		/* TODO: Remove this when the decimated
		 * resolution is available */
		/* Only for continuous preview mode
		 * where we need 2xOut resolution */
		ref_info.padded_width *= 2;
		ref_info.width -= left_cropping;
		ref_info.width *= 2;
		/* In case of left-cropping, add 2 vectors */
		ref_info.width += left_cropping ? 2*ISP_VEC_NELEMS : 0;
		/* Must be even amount of vectors */
		ref_info.width  = CEIL_MUL(ref_info.width,2*ISP_VEC_NELEMS);
		ref_info.height -= top_cropping;
		ref_info.height *= 2;
		ref_info.height += top_cropping;
	} else if (my_css.continuous) {
		ref_info.width -= left_cropping;
		/* In case of left-cropping, add 2 vectors */
		ref_info.width += left_cropping ? 2*ISP_VEC_NELEMS : 0;
		/* Must be even amount of vectors */
		ref_info.width  = CEIL_MUL(ref_info.width,2*ISP_VEC_NELEMS);
	}

	ref_info.format = SH_CSS_FRAME_FORMAT_RAW;

	if (init_time)
		idx = 0;
	else
		idx = NUM_OFFLINE_INIT_CONTINUOUS_FRAMES;
	for (i = idx; i < NUM_CONTINUOUS_FRAMES; i++) {
		/* free previous frame */
		if (pipe->pipe.preview.continuous_frames[i]) {
			sh_css_frame_free(pipe->pipe.preview.continuous_frames[i]);
			pipe->pipe.preview.continuous_frames[i] = NULL;
		}
		/* check if new frame needed */
		if (i < num_frames) {
			/* allocate new frame */
			err = sh_css_frame_allocate_from_info(
				&pipe->pipe.preview.continuous_frames[i],
				&ref_info);
			if (err != sh_css_success)
				return err;
		}
	}
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_allocate_continuous_frames() leave: return success\n");
	return sh_css_success;
}

void
sh_css_update_continuous_frames(void)
{
	struct sh_css_pipe *pipe = &my_css.preview_pipe;
	unsigned int i;
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_update_continuous_frames() enter:\n");

	for (i = NUM_OFFLINE_INIT_CONTINUOUS_FRAMES; i < my_css.num_cont_raw_frames; i++) {
		sh_css_update_host2sp_offline_frame(i,
				pipe->pipe.preview.continuous_frames[i]);
	}
	sh_css_update_host2sp_cont_num_raw_frames
			(my_css.num_cont_raw_frames, true);
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_update_continuous_frames() leave: return_void\n");
}

void
sh_css_set_cont_prev_start_time(unsigned int overlap)
{
	const struct sh_css_fw_info *fw;
	unsigned int HIVE_ADDR_sp_copy_preview_overlap;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_set_cont_prev_start_time() enter:\n");

	fw = &sh_css_sp_fw;
	HIVE_ADDR_sp_copy_preview_overlap = fw->info.sp.copy_preview_overlap;

	(void)HIVE_ADDR_sp_copy_preview_overlap;

	overlap = overlap/4; /* we request 4 lines each time in copy pipe */

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_copy_preview_overlap),
		(uint32_t)(overlap));
}
