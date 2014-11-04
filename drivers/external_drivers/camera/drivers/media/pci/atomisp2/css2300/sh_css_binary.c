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

#include <gdc_device.h>	/* HR_GDC_N */

#include "sh_css_binary.h"

#include "sh_css_debug.h"

#include "sh_css.h"
#include "sh_css_internal.h"
#include "sh_css_sp.h"
#include "sh_css_firmware.h"
#include "sh_css_defs.h"

#include "memory_access.h"

#include "assert_support.h"

static struct sh_css_binary_info *all_binaries; /* ISP binaries only (no SP) */
static struct sh_css_binary_info *binary_infos[SH_CSS_BINARY_NUM_MODES] = {NULL, };

enum sh_css_err
sh_css_binary_grid_info(struct sh_css_binary *binary,
			struct sh_css_grid_info *info)
{
	struct sh_css_3a_grid_info *s3a_info;
	struct sh_css_dvs_grid_info *dvs_info;

	assert(binary != NULL);
	assert(info != NULL);
	if ((binary == NULL) || (info == NULL)) {
		return sh_css_err_internal_error;
	}
	s3a_info = &info->s3a_grid;
	dvs_info = &info->dvs_grid;

	info->isp_in_width = binary->internal_frame_info.width;
	info->isp_in_height = binary->internal_frame_info.height;

	/* for DIS, we use a division instead of a ceil_div. If this is smaller
	 * than the 3a grid size, it indicates that the outer values are not
	 * valid for DIS.
	 */
	dvs_info->enable            = binary->info->enable.dis;
	dvs_info->width             = binary->dis_ver_proj_num_3a;
	dvs_info->height            = binary->dis_hor_proj_num_3a;
	dvs_info->aligned_width     = binary->dis_ver_proj_num_isp;
	dvs_info->aligned_height    = binary->dis_hor_proj_num_isp;
	dvs_info->bqs_per_grid_cell = 1 << binary->dis_deci_factor_log2;
	info->dvs_hor_coef_num      = binary->dis_hor_coef_num_3a;
	info->dvs_ver_coef_num      = binary->dis_ver_coef_num_3a;

	/* 3A statistics grid */
	s3a_info->enable            = binary->info->enable.s3a;
	s3a_info->width             = binary->s3atbl_width;
	s3a_info->height            = binary->s3atbl_height;
	s3a_info->aligned_width     = binary->s3atbl_isp_width;
	s3a_info->aligned_height    = binary->s3atbl_isp_height;
	s3a_info->bqs_per_grid_cell = (1 << binary->deci_factor_log2);
	s3a_info->use_dmem          = binary->info->s3atbl_use_dmem;

	return sh_css_success;
}

static void
init_pc_histogram(struct sh_css_pc_histogram *histo)
{
	assert(histo != NULL);
	if (histo == NULL) {
		sh_css_dtrace(SH_DBG_ERROR,
		"init_pc_histogram(): error: histo is NULL\n");
		return;
	}

	histo->length = 0;
	histo->run = NULL;
	histo->stall = NULL;
}

static void
init_metrics(struct sh_css_binary_metrics *metrics,
	     const struct sh_css_binary_info *info)
{
	assert(metrics != NULL);
	assert(info != NULL);
	if ((metrics == NULL) || (info == NULL)) {
		sh_css_dtrace(SH_DBG_ERROR,
		"init_metrics(): error: metrics or info is NULL\n");
		return;
	}

	metrics->mode = info->mode;
	metrics->id   = info->id;
	metrics->next = NULL;
	init_pc_histogram(&metrics->isp_histogram);
	init_pc_histogram(&metrics->sp_histogram);
}

static bool
supports_output_format(const struct sh_css_binary_info *info,
		       enum sh_css_frame_format format)
{
	int i;

	assert(info != NULL);
	if (info == NULL)
		return false;

	for (i = 0; i < info->num_output_formats; i++) {
		if (info->output_formats[i] == format)
			return true;
	}
	return false;
}

static enum sh_css_err
init_binary_info(struct sh_css_binary_info *info, unsigned int i,
		 bool *binary_found)
{
	const unsigned char *blob = sh_css_blob_info[i].blob;
	unsigned size = sh_css_blob_info[i].header.blob.size;

	assert(info != NULL);
	assert(binary_found != NULL);
	if ((info == NULL) || (binary_found == NULL))
		return sh_css_err_internal_error;

	*info = sh_css_blob_info[i].header.info.isp;
	*binary_found = blob != NULL;
	info->blob_index = i;
	/* we don't have this binary, skip it */
	if (!size)
		return sh_css_success;

	info->xmem_addr = sh_css_load_blob(blob, size);
	if (!info->xmem_addr)
		return sh_css_err_cannot_allocate_memory;
	return sh_css_success;
}

/* When binaries are put at the beginning, they will only
 * be selected if no other primary matches.
 */
enum sh_css_err
sh_css_init_binary_infos(void)
{
	unsigned int i;
	unsigned int num_of_isp_binaries = sh_css_num_binaries - 1;

	all_binaries = sh_css_malloc(num_of_isp_binaries *
						sizeof(*all_binaries));
	if (all_binaries == NULL)
		return sh_css_err_cannot_allocate_memory;

	for (i = 0; i < num_of_isp_binaries; i++) {
		enum sh_css_err ret;
		struct sh_css_binary_info *binary = &all_binaries[i];
		bool binary_found;

		if (binary != NULL) {
			ret = init_binary_info(binary, i, &binary_found);
			if (ret != sh_css_success)
				return ret;
			if (!binary_found)
				continue;
			/* Prepend new binary information */
			binary->next = binary_infos[binary->mode];
			binary_infos[binary->mode] = binary;
			binary->blob = &sh_css_blob_info[i];
		}
	}
	return sh_css_success;
}

enum sh_css_err
sh_css_binary_uninit(void)
{
	unsigned int i;
	struct sh_css_binary_info *b;

	for (i = 0; i < SH_CSS_BINARY_NUM_MODES; i++) {
		for (b = binary_infos[i]; b; b = b->next) {
			if (b->xmem_addr)
				mmgr_free(b->xmem_addr);
			b->xmem_addr = mmgr_NULL;
		}
		binary_infos[i] = NULL;
	}
	sh_css_free(all_binaries);
	return sh_css_success;
}

static int
sh_css_grid_deci_factor_log2(int width, int height)
{
	int fact, fact1;
	fact = 5;
	while (ISP_BQ_GRID_WIDTH(width, fact - 1) <= SH_CSS_MAX_BQ_GRID_WIDTH &&
	       ISP_BQ_GRID_HEIGHT(height, fact - 1) <= SH_CSS_MAX_BQ_GRID_HEIGHT
	       && fact > 3)
		fact--;

	/* fact1 satisfies the specification of grid size. fact and fact1 is
	   not the same for some resolution (fact=4 and fact1=5 for 5mp). */
	if (width >= 2560)
		fact1 = 5;
	else if (width >= 1280)
		fact1 = 4;
	else
		fact1 = 3;
	return max(fact, fact1);
}

enum sh_css_err
sh_css_fill_binary_info(const struct sh_css_binary_info *info,
		 bool online,
		 bool two_ppc,
		 enum sh_css_input_format stream_format,
		 const struct sh_css_frame_info *in_info, /* can be NULL */
		 const struct sh_css_frame_info *out_info, /* can be NULL */
		 const struct sh_css_frame_info *vf_info, /* can be NULL */
		 struct sh_css_binary *binary,
		 bool continuous)
{
	unsigned int dvs_env_width = 0,
		     dvs_env_height = 0,
		     vf_log_ds = 0,
		     s3a_log_deci = 0,
		     bits_per_pixel = 0,
		     ds_input_width = 0,
		     ds_input_height = 0,
		     isp_input_width,
		     isp_input_height,
		     isp_internal_width,
		     isp_internal_height,
		     isp_output_width = 0,
		     isp_output_height = 0,
		     s3a_isp_width;
	unsigned char enable_ds;
	bool enable_yuv_ds;
	bool enable_hus = false;
	bool enable_vus = false;
	bool is_out_format_rgba888 = false;
	unsigned int tmp_width, tmp_height;
	bool input_is_yuv_8 = input_format_is_yuv_8(stream_format);

	assert(info != NULL);
	assert(binary != NULL);
	if ((info == NULL) || (binary == NULL))
		return sh_css_err_internal_error;
	enable_ds = info->enable.ds;
	enable_yuv_ds = enable_ds & 2;

	if (in_info != NULL) {
		bits_per_pixel = in_info->raw_bit_depth;
		if (out_info != NULL) {
			enable_hus = in_info->width < out_info->width;
			enable_vus = in_info->height < out_info->height;
		}
	}
	if (out_info != NULL) {
		isp_output_width  = out_info->padded_width;
		isp_output_height = out_info->height;
		is_out_format_rgba888 =
			out_info->format == SH_CSS_FRAME_FORMAT_RGBA888;
	}
	(void)is_out_format_rgba888; /* Klocwork pacifier: VA_UNUSED.GEN */
	if (info->enable.dvs_envelope) {
		sh_css_video_get_dis_envelope(&dvs_env_width, &dvs_env_height);
		dvs_env_width  = MAX(dvs_env_width, SH_CSS_MIN_DVS_ENVELOPE);
		dvs_env_height = MAX(dvs_env_height, SH_CSS_MIN_DVS_ENVELOPE);
	}
	binary->dvs_envelope.width  = dvs_env_width;
	binary->dvs_envelope.height = dvs_env_height;
	if (vf_info != NULL) {
		enum sh_css_err err;
		err = sh_css_vf_downscale_log2(out_info, vf_info, &vf_log_ds);
		if (err != sh_css_success)
			return err;
		vf_log_ds = min(vf_log_ds, info->max_vf_log_downscale);
	}
	if (online) {
		bits_per_pixel = sh_css_input_format_bits_per_pixel(
			stream_format, two_ppc);
	}
	if (in_info != NULL) {
		ds_input_width  = in_info->padded_width + info->left_cropping;
		ds_input_height = in_info->height + info->top_cropping;
	}
	if (enable_hus) /* { */
		ds_input_width  += dvs_env_width;
	/* } */
	if (enable_vus) /* { */
		ds_input_height += dvs_env_height;
	/* } */
	tmp_width  = (enable_yuv_ds && (ds_input_width > isp_output_width)) ?
			ds_input_width  : isp_output_width;
	tmp_height = (enable_yuv_ds && (ds_input_height > isp_output_height)) ?
			ds_input_height : isp_output_height;

	/* We first calculate the resolutions used by the ISP. After that,
	 * we use those resolutions to compute sizes for tables etc. */
	isp_internal_width = __ISP_INTERNAL_WIDTH(tmp_width,
		dvs_env_width,
		info->left_cropping, info->mode,
		info->c_subsampling,
		info->output_num_chunks, info->pipelining,
		is_out_format_rgba888);
	isp_internal_height = __ISP_INTERNAL_HEIGHT(tmp_height,
		info->top_cropping,
		dvs_env_height);
	isp_input_width = _ISP_INPUT_WIDTH(isp_internal_width,
		ds_input_width,
		enable_ds || enable_hus);
	isp_input_height = _ISP_INPUT_HEIGHT(isp_internal_height,
		ds_input_height,
		enable_ds || enable_vus);
	s3a_isp_width = _ISP_S3A_ELEMS_ISP_WIDTH(isp_input_width,
		isp_internal_width, enable_hus || enable_yuv_ds,
		info->left_cropping);
	if (info->fixed_s3a_deci_log)
		s3a_log_deci = info->fixed_s3a_deci_log;
	else
		s3a_log_deci = sh_css_grid_deci_factor_log2(s3a_isp_width,
							    isp_input_height);

	/* In the yuv-copy binary, we have an internal buffer where the copy
	 * writes its output. Then the padded width should be bus-aligned */
	if (info->mode == SH_CSS_BINARY_MODE_COPY && input_is_yuv_8) {
		isp_input_width = CEIL_MUL(isp_input_width,
				2*HIVE_ISP_DDR_WORD_BYTES);
	}

	binary->vf_downscale_log2 = vf_log_ds;
	binary->deci_factor_log2  = s3a_log_deci;
	binary->input_buf_vectors =
			SH_CSS_NUM_INPUT_BUF_LINES * _ISP_VECS(isp_input_width);
	binary->online            = online;
	binary->input_format      = stream_format;
	/* input info */
	if (in_info != NULL) {
		binary->in_frame_info.format = in_info->format;
		binary->in_frame_info.width = in_info->width +
			info->left_cropping + dvs_env_width;
	}
	binary->in_frame_info.padded_width  = isp_input_width;
	binary->in_frame_info.height        = isp_input_height;
	binary->in_frame_info.raw_bit_depth = bits_per_pixel;
	/* internal frame info */
	if (out_info != NULL) /* { */
		binary->internal_frame_info.format          = out_info->format;
	/* } */
	binary->internal_frame_info.width           = isp_internal_width;
	binary->internal_frame_info.padded_width    = isp_internal_width;
	binary->internal_frame_info.height          = isp_internal_height;
	binary->internal_frame_info.raw_bit_depth   = bits_per_pixel;
	/* output info */
	if (out_info != NULL) {
		binary->out_frame_info.format        = out_info->format;
		binary->out_frame_info.width         = out_info->width;
	}
	binary->out_frame_info.padded_width  = isp_output_width;
	binary->out_frame_info.height        = isp_output_height;
	binary->out_frame_info.raw_bit_depth = bits_per_pixel;

	/* viewfinder output info */
	binary->vf_frame_info.format = SH_CSS_FRAME_FORMAT_YUV_LINE;
	if ((vf_info != NULL) && (in_info != NULL)) {
		unsigned int vf_out_vecs, vf_out_width, vf_out_height;
		vf_out_vecs = __ISP_VF_OUTPUT_WIDTH_VECS(isp_output_width,
			vf_log_ds);
		vf_out_width = _ISP_VF_OUTPUT_WIDTH(vf_out_vecs);
		vf_out_height = _ISP_VF_OUTPUT_HEIGHT(isp_output_height,
			vf_log_ds);
		/* If we are in continuous preview mode, then out port is
		 * active instead of vfout port
		 */
		if (info->enable.raw_binning && continuous) {
			binary->out_frame_info.width =
				(in_info->width >> vf_log_ds);
			binary->out_frame_info.padded_width = vf_out_width;
			binary->out_frame_info.height       = vf_out_height;
		} else {
		/* we also store the raw downscaled width. This is used for
		 * digital zoom in preview to zoom only on the width that
		 * we actually want to keep, not on the aligned width. */
			if (out_info == NULL)
				return sh_css_err_internal_error;
			binary->vf_frame_info.width =
				(out_info->width >> vf_log_ds);
			binary->vf_frame_info.padded_width = vf_out_width;
			binary->vf_frame_info.height       = vf_out_height;
		}
	} else {
		binary->vf_frame_info.width        = 0;
		binary->vf_frame_info.padded_width = 0;
		binary->vf_frame_info.height       = 0;
	}

	if (info->enable.ca_gdc) {
		binary->morph_tbl_width =
			_ISP_MORPH_TABLE_WIDTH(isp_internal_width);
		binary->morph_tbl_aligned_width  =
			_ISP_MORPH_TABLE_ALIGNED_WIDTH(isp_internal_width);
		binary->morph_tbl_height =
			_ISP_MORPH_TABLE_HEIGHT(isp_internal_height);
	} else {
		binary->morph_tbl_width  = 0;
		binary->morph_tbl_aligned_width  = 0;
		binary->morph_tbl_height = 0;
	}
	if (info->enable.sc)
		binary->sctbl_width_per_color =
			SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR;
	else
		binary->sctbl_width_per_color = 0;

	if (info->enable.s3a) {
		binary->s3atbl_width  =
			_ISP_S3ATBL_WIDTH(binary->in_frame_info.width,
				s3a_log_deci);
		binary->s3atbl_height =
			_ISP_S3ATBL_HEIGHT(binary->in_frame_info.height,
				s3a_log_deci);
		binary->s3atbl_isp_width =
			_ISP_S3ATBL_ISP_WIDTH(
				_ISP_S3A_ELEMS_ISP_WIDTH(isp_input_width,
					isp_internal_width,
					enable_hus || enable_yuv_ds,
					info->left_cropping),
					s3a_log_deci);
		binary->s3atbl_isp_height =
			_ISP_S3ATBL_ISP_HEIGHT(
				_ISP_S3A_ELEMS_ISP_HEIGHT(isp_input_height,
				isp_internal_height,
				enable_vus || enable_yuv_ds),
				s3a_log_deci);
	} else {
		binary->s3atbl_width  = 0;
		binary->s3atbl_height = 0;
		binary->s3atbl_isp_width  = 0;
		binary->s3atbl_isp_height = 0;
	}

	if (info->enable.sc) {
		binary->sctbl_width_per_color  =
			_ISP_SCTBL_WIDTH_PER_COLOR(isp_input_width,
				s3a_log_deci);
		binary->sctbl_aligned_width_per_color =
			SH_CSS_MAX_SCTBL_ALIGNED_WIDTH_PER_COLOR;
		binary->sctbl_height =
			_ISP_SCTBL_HEIGHT(isp_input_height, s3a_log_deci);
	} else {
		binary->sctbl_width_per_color         = 0;
		binary->sctbl_aligned_width_per_color = 0;
		binary->sctbl_height                  = 0;
	}
	if (info->enable.dis) {
		binary->dis_deci_factor_log2 = SH_CSS_DIS_DECI_FACTOR_LOG2;
		binary->dis_hor_coef_num_3a  =
			_ISP_SDIS_HOR_COEF_NUM_3A(binary->in_frame_info.width,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
		binary->dis_ver_coef_num_3a  =
			_ISP_SDIS_VER_COEF_NUM_3A(binary->in_frame_info.height,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
		binary->dis_hor_coef_num_isp =
			_ISP_SDIS_HOR_COEF_NUM_ISP(
				_ISP_SDIS_ELEMS_ISP(isp_input_width,
				isp_internal_width,
				enable_hus || enable_yuv_ds));
		binary->dis_ver_coef_num_isp =
			_ISP_SDIS_VER_COEF_NUM_ISP(
				_ISP_SDIS_ELEMS_ISP(isp_input_height,
				isp_internal_height,
				enable_vus || enable_yuv_ds));
		binary->dis_hor_proj_num_3a  =
			_ISP_SDIS_HOR_PROJ_NUM_3A(binary->in_frame_info.height,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
		binary->dis_ver_proj_num_3a  =
			_ISP_SDIS_VER_PROJ_NUM_3A(binary->in_frame_info.width,
						  SH_CSS_DIS_DECI_FACTOR_LOG2);
		binary->dis_hor_proj_num_isp =
			__ISP_SDIS_HOR_PROJ_NUM_ISP(
				_ISP_SDIS_ELEMS_ISP(isp_input_height,
				isp_internal_height,
				enable_vus || enable_yuv_ds),
						SH_CSS_DIS_DECI_FACTOR_LOG2);
		binary->dis_ver_proj_num_isp =
			__ISP_SDIS_VER_PROJ_NUM_ISP(
				_ISP_SDIS_ELEMS_ISP(isp_input_width,
				isp_internal_width,
				enable_hus || enable_yuv_ds),
						SH_CSS_DIS_DECI_FACTOR_LOG2);
	} else {
		binary->dis_deci_factor_log2 = 0;
		binary->dis_hor_coef_num_3a  = 0;
		binary->dis_ver_coef_num_3a  = 0;
		binary->dis_hor_coef_num_isp = 0;
		binary->dis_ver_coef_num_isp = 0;
		binary->dis_hor_proj_num_3a  = 0;
		binary->dis_ver_proj_num_3a  = 0;
		binary->dis_hor_proj_num_isp = 0;
		binary->dis_ver_proj_num_isp = 0;
	}
	if (info->left_cropping)
		binary->left_padding = 2 * ISP_VEC_NELEMS - info->left_cropping;
	else
		binary->left_padding = 0;

	binary->info = info;

	return sh_css_success;
}

enum sh_css_err
sh_css_binary_find(struct sh_css_binary_descr *descr,
		   struct sh_css_binary *binary,
		   bool is_video_usecase /* TODO: Remove this */)
{
	int mode;
	bool online;
	bool two_ppc;
	enum sh_css_input_format stream_format;
	bool input_is_yuv_8;
	const struct sh_css_frame_info *req_in_info,
				       *req_out_info,
				       *req_vf_info;

	struct sh_css_frame_info *cc_in_info
				= sh_css_malloc(sizeof(*req_in_info));
	struct sh_css_frame_info *cc_out_info
				= sh_css_malloc(sizeof(*req_out_info));
	struct sh_css_frame_info *cc_vf_info
				= sh_css_malloc(sizeof(*req_vf_info));

	struct sh_css_binary_info *candidate;
	unsigned int dvs_envelope_width = 0,
		     dvs_envelope_height = 0;
	bool need_ds = false,
	     need_dz = false,
	     need_dvs = false,
	     need_outputdeci = false;
	bool enable_yuv_ds;
	bool enable_high_speed;
	bool enable_dvs_6axis;
	bool enable_reduced_pipe;
	enum sh_css_err err = sh_css_err_internal_error;
	bool continuous = sh_css_continuous_is_enabled();
	unsigned int isp_pipe_version;

	assert(descr != NULL);
	if (descr == NULL)
		return sh_css_err_internal_error;
	assert(binary != NULL);
	if (binary == NULL)
		return sh_css_err_internal_error;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() enter: "
		"descr=%p, (mode=%d), "
		"binary=%p, "
		"is_video_usecase=%d\n",
		descr, descr->mode,
		binary, (int)is_video_usecase);

	mode = descr->mode;
	online = descr->online;
	two_ppc = descr->two_ppc;
	stream_format = descr->stream_format;
	input_is_yuv_8 = input_format_is_yuv_8(stream_format);
	req_in_info = descr->in_info;
	req_out_info = descr->out_info;
	req_vf_info = descr->vf_info;

	enable_yuv_ds = descr->enable_yuv_ds;
	enable_high_speed = descr->enable_high_speed;
	enable_dvs_6axis  = descr->enable_dvs_6axis;
	enable_reduced_pipe = descr->enable_reduced_pipe;
	isp_pipe_version = descr->isp_pipe_version;

	if (cc_in_info == NULL || cc_out_info == NULL || cc_vf_info == NULL) {
		sh_css_free(cc_in_info);
		sh_css_free(cc_out_info);
		sh_css_free(cc_vf_info);
		return err;
	}

	if (mode == SH_CSS_BINARY_MODE_VIDEO) {
		unsigned int dx, dy;
		sh_css_get_zoom_factor(&dx, &dy);
		sh_css_video_get_dis_envelope(&dvs_envelope_width,
					      &dvs_envelope_height);
		sh_css_video_get_enable_dz(&need_dz);
		/* Video is the only mode that has a nodz variant. */
		if (!need_dz)
			need_dz = ((dx != HRT_GDC_N) || (dy != HRT_GDC_N));
		need_dvs = dvs_envelope_width || dvs_envelope_height;
	}

	need_ds = req_in_info->width > req_out_info->width ||
		  req_in_info->height > req_out_info->height;

	/* In continuous mode, ds is not possible. Instead we are
	 * allowed to have output decimation (vf_veceven) */
	if (need_ds && continuous) {
		need_ds = false;
		need_outputdeci = true;
	}

	for (candidate = binary_infos[mode]; candidate;
	     candidate = candidate->next) {
		/* @GC: Input format is the differentiating factor in binary
		 * selection for the copy binaries although we dont know which
		 * binaries support which input format. We hack it as the
		 * sequence of two copy binaries in FW is known to us.
		 * TODO: Extend all binary defs with supported input format
		 * field (see CR 1955) */
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() candidate = %p, mode = %d ID = %d\n",candidate, candidate->mode, candidate->id);

		if (mode == SH_CSS_BINARY_MODE_COPY && candidate->enable.ds &&
		    (!input_is_yuv_8 || !is_video_usecase)/*TODO: change this*/) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d == %d) && %d && (!%d || !%d)\n", __LINE__,
			mode, SH_CSS_BINARY_MODE_COPY, candidate->enable.ds,
			input_is_yuv_8, is_video_usecase);
			continue;
		}
/*
 * MW: Only a limited set of jointly configured binaries can be used in a continuous preview/video mode
 * unless it is the copy mode and copy runs on SP
 */
		if (!candidate->enable.continuous && continuous && (mode != SH_CSS_BINARY_MODE_COPY)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d && (%d != %d)\n", __LINE__, candidate->enable.continuous, continuous, mode, SH_CSS_BINARY_MODE_COPY);
			continue;
		}

		if (mode == SH_CSS_BINARY_MODE_VIDEO &&
		    candidate->isp_pipe_version != isp_pipe_version) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d == %d) && (%d != %d)\n", __LINE__,
			mode, SH_CSS_BINARY_MODE_VIDEO, candidate->isp_pipe_version, isp_pipe_version);
			continue;
		}
		if (!candidate->enable.reduced_pipe && enable_reduced_pipe) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d\n", __LINE__, candidate->enable.reduced_pipe, enable_reduced_pipe);
			continue;
		}
		if (!candidate->enable.dvs_6axis && enable_dvs_6axis) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d\n", __LINE__, candidate->enable.dvs_6axis, enable_dvs_6axis);
			continue;
		}
		if (candidate->enable.high_speed && !enable_high_speed) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: %d && !%d\n", __LINE__, candidate->enable.high_speed, enable_high_speed);
			continue;
		}
		if (!(candidate->enable.ds & 2) && enable_yuv_ds) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d\n", __LINE__, ((candidate->enable.ds & 2) != 0), enable_yuv_ds);
			continue;
		}
		if ((candidate->enable.ds & 2) && !enable_yuv_ds) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: %d && !%d\n", __LINE__, ((candidate->enable.ds & 2) != 0), enable_yuv_ds);
			continue;
		}

		if (mode == SH_CSS_BINARY_MODE_VIDEO &&
		    candidate->enable.ds && need_ds)
			need_dz = false;

		if (mode != SH_CSS_BINARY_MODE_PREVIEW &&
		    mode != SH_CSS_BINARY_MODE_COPY &&
		    candidate->enable.vf_veceven && (req_vf_info == NULL)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d != %d) && (%d != %d) && %d && (%p == NULL)\n", __LINE__,
			mode, SH_CSS_BINARY_MODE_PREVIEW, mode, SH_CSS_BINARY_MODE_COPY,
			candidate->enable.vf_veceven, req_vf_info);
			continue;
		}
		if ((req_vf_info != NULL) && !(candidate->enable.vf_veceven ||
				     candidate->variable_vf_veceven)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%p != NULL) && !(%d || %d)\n", __LINE__, req_vf_info, candidate->enable.vf_veceven, candidate->variable_vf_veceven);
			continue;
		}
		if (!candidate->enable.dvs_envelope && need_dvs) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d\n", __LINE__, candidate->enable.dvs_envelope, (int)need_dvs);
			continue;
		}
		if (dvs_envelope_width > candidate->max_dvs_envelope_width) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d > %d)\n", __LINE__, dvs_envelope_width, candidate->max_dvs_envelope_width);
			continue;
		}
		if (dvs_envelope_height > candidate->max_dvs_envelope_height) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d > %d)\n", __LINE__, dvs_envelope_height, candidate->max_dvs_envelope_height);
			continue;
		}
		if (!candidate->enable.ds && need_ds) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d\n", __LINE__, candidate->enable.ds, (int)need_ds);
			continue;
		}
		if (!candidate->enable.uds && need_dz) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d\n", __LINE__, candidate->enable.uds, (int)need_dz);
			continue;
		}
		if (online && candidate->input == SH_CSS_BINARY_INPUT_MEMORY) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: %d && (%d == %d)\n", __LINE__, online, candidate->input, SH_CSS_BINARY_INPUT_MEMORY);
			continue;
		}
		if (!online && candidate->input == SH_CSS_BINARY_INPUT_SENSOR) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && (%d == %d)\n", __LINE__, online, candidate->input, SH_CSS_BINARY_INPUT_SENSOR);
			continue;
		}
		if (req_out_info->padded_width < candidate->min_output_width ||
		    req_out_info->padded_width > candidate->max_output_width) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d > %d) || (%d < %d)\n", __LINE__,
			req_out_info->padded_width, candidate->min_output_width,
			req_out_info->padded_width, candidate->max_output_width);
			continue;
		}

		if (req_in_info->padded_width > candidate->max_input_width) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: (%d > %d)\n", __LINE__, req_in_info->padded_width, candidate->max_input_width);
			continue;
		}
		if (!supports_output_format(candidate, req_out_info->format)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d\n", __LINE__, supports_output_format(candidate, req_out_info->format));
			continue;
		}

/*
 * Select either a binary with conditional decimation or one with fixed decimation
 */
		if (descr->binning && !(candidate->enable.raw_binning || candidate->enable.fixed_bayer_ds)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: %d && !(%d || %d)\n", __LINE__, descr->binning, candidate->enable.raw_binning, candidate->enable.fixed_bayer_ds);
			continue;
		}
/*
 * "candidate->enable.fixed_bayer_ds" is also used to get the correct buffer size reservation in the still capture and capture_pp binaries
 */
		if (!descr->binning && candidate->enable.fixed_bayer_ds && ((mode == SH_CSS_BINARY_MODE_PREVIEW) || (mode == SH_CSS_BINARY_MODE_VIDEO))) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && %d && ((%d == %d) || (%d == %d))\n", __LINE__,
		descr->binning, candidate->enable.fixed_bayer_ds, mode, SH_CSS_BINARY_MODE_PREVIEW, mode, SH_CSS_BINARY_MODE_VIDEO);
			continue;
		}

		if (descr->binning) {
			if (!candidate->enable.fixed_bayer_ds && (req_in_info->width > 3264)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: !%d && (%d > %d)\n", __LINE__, candidate->enable.fixed_bayer_ds,req_in_info->width, 3264);
				continue;
			}
			if (candidate->enable.fixed_bayer_ds  && (req_in_info->width <= 3264)) {
	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() [%d] continue: %d && (%d <= %d)\n", __LINE__, candidate->enable.fixed_bayer_ds,req_in_info->width, 3264);
				continue;
		}
		}

/*
 * If we are in continuous preview mode, it is possible to have
 * an output decimation. If output decimation is needed, the
 * decimation factor is calculated output->vf. So, we switch
 * the ports to share the same calculation module
 *
 * There are 2 flavours, an optimised binary for very large resolutions
 * that always decimates, or a binary that conditionally decimates
 */
		if ((candidate->enable.raw_binning || candidate->enable.fixed_bayer_ds)
				&& continuous && need_outputdeci) {
			*cc_in_info = *req_in_info;
			*cc_out_info = *req_out_info;
			*cc_vf_info = *req_out_info;

			if (descr->binning) {
			/* Take into account that we have (currently implicit)
			 * a decimation on preview-ISP which halves the
			 * resolution. Therefore, here we specify this. */
				cc_out_info->width        = req_in_info->width/2;
				cc_out_info->padded_width = req_in_info->padded_width/2;
				cc_out_info->height       = req_in_info->height/2;

				cc_in_info->width        = req_in_info->width/2;
				cc_in_info->padded_width = req_in_info->padded_width/2;
				cc_in_info->height       = req_in_info->height/2;
			} else {
				cc_out_info->width        = req_in_info->width;
				cc_out_info->padded_width = req_in_info->padded_width;
				cc_out_info->height       = req_in_info->height;
			}
		} else {
			*cc_in_info  = *req_in_info;
			*cc_out_info = *req_out_info;
			if (req_vf_info != NULL) {
				*cc_vf_info  = *req_vf_info;
			} else {
				sh_css_free(cc_vf_info);
				cc_vf_info  = NULL;
			}
		}

		/* reconfigure any variable properties of the binary */
		err = sh_css_fill_binary_info(candidate, online, two_ppc,
				       stream_format, cc_in_info,
				       cc_out_info, cc_vf_info,
				       binary, continuous);
		if (err)
			break;
		init_metrics(&binary->metrics, binary->info);
		break;
	}

	assert(candidate != NULL);
	if(candidate == NULL)
		return sh_css_err_internal_error;

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() selected = %p, mode = %d ID = %d\n",candidate, candidate->mode, candidate->id);

	sh_css_free(cc_in_info);
	sh_css_free(cc_out_info);
	sh_css_free(cc_vf_info);

	sh_css_dtrace(SH_DBG_TRACE,
		"sh_css_binary_find() leave: return_err=%d\n", err);

	return err;
}

unsigned
sh_css_max_vf_width(void)
{
  return binary_infos[SH_CSS_BINARY_MODE_VF_PP]->max_output_width;
}
