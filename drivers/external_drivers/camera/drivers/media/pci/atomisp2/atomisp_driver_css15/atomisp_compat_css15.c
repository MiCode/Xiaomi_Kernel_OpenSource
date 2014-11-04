/*
 * Support for Clovertrail PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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

#include <linux/kfifo.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-dev.h>

#include "sh_css_debug.h"
#include "host/mmu_local.h"
#include "device_access/device_access.h"
#include "memory_access/memory_access.h"

#include "atomisp_compat.h"
#include "atomisp_internal.h"
#include "atomisp_cmd.h"

#include "sh_css_accelerate.h"
#include <asm/intel-mid.h>

void atomisp_css_debug_dump_sp_sw_debug_info(void)
{
	sh_css_dump_sp_sw_debug_info();
}

void atomisp_css_debug_dump_debug_info(const char *context)
{
	sh_css_dump_debug_info(context);
}

void atomisp_css_debug_set_dtrace_level(const unsigned int trace_level)
{
	sh_css_set_dtrace_level(trace_level);
}

void atomisp_store_uint32(hrt_address addr, uint32_t data)
{
	device_store_uint32(addr, data);
}

void atomisp_load_uint32(hrt_address addr, uint32_t *data)
{
	*data = device_load_uint32(addr);
}

int atomisp_css_init(struct atomisp_device *isp)
{
	device_set_base_address(0);

	/* set css env */
	isp->css_env.isp_css_env = sh_css_default_env();
	isp->css_env.isp_css_env.sh_env.alloc = atomisp_kernel_zalloc;
	isp->css_env.isp_css_env.sh_env.free = atomisp_kernel_free;

	/*
	 * if the driver gets closed and reopened, the HMM is not reinitialized
	 * This means we need to put the L1 page table base address back into
	 * the ISP
	 */
	if (isp->mmu_l1_base)
		/*
		 * according to sh_css.c sh_css_mmu_set_page_table_base_index
		 * is deprecated and mmgr_set_base_address should be used
		 * instead. But just for now (with CSS "alpha") replacing
		 * all sh_cssh_mmu_set_page_table_base_index() -calls
		 * with mmgr_set_base_address() is not working.
		 */
		sh_css_mmu_set_page_table_base_index(
				HOST_ADDRESS(isp->mmu_l1_base));

	/* With CSS "alpha" it is mandatory to set base address always */
	mmgr_set_base_address(HOST_ADDRESS(isp->mmu_l1_base));

	/* Init ISP */
	if (sh_css_init(&isp->css_env.isp_css_env,
			isp->firmware->data, isp->firmware->size)) {
		dev_err(isp->dev, "css init failed --- bad firmware?\n");
		return -EINVAL;
	}

	/* CSS has default zoom factor of 61x61, we want no zoom
	   because the zoom binary for capture is broken (XNR). */
	if (IS_ISP24XX(isp))
		sh_css_set_zoom_factor(MRFLD_MAX_ZOOM_FACTOR,
					MRFLD_MAX_ZOOM_FACTOR);
	else
		sh_css_set_zoom_factor(MFLD_MAX_ZOOM_FACTOR,
					MFLD_MAX_ZOOM_FACTOR);

	dev_dbg(isp->dev, "sh_css_init success\n");

	return 0;
}

void atomisp_css_uninit(struct atomisp_device *isp)
{
	sh_css_uninit();

	/* store L1 base address for next time we init the CSS */
	isp->mmu_l1_base = (void *)sh_css_mmu_get_page_table_base_index();
}

void atomisp_css_suspend(void)
{
	sh_css_suspend();
}

int atomisp_css_resume(struct atomisp_device *isp)
{
	sh_css_resume();

	return 0;
}

int atomisp_css_irq_translate(struct atomisp_device *isp,
			      unsigned int *infos)
{
	int err;

	err = sh_css_translate_interrupt(infos);
	if (err != sh_css_success) {
		dev_warn(isp->dev,
			  "%s:failed to translate irq (err = %d,infos = %d)\n",
			  __func__, err, *infos);
		return -EINVAL;
	}

	return 0;
}

void atomisp_css_rx_get_irq_info(unsigned int *infos)
{
	sh_css_rx_get_interrupt_info(infos);
}

void atomisp_css_rx_clear_irq_info(unsigned int infos)
{
	sh_css_rx_clear_interrupt_info(infos);
}

int atomisp_css_irq_enable(struct atomisp_device *isp,
			    enum atomisp_css_irq_info info, bool enable)
{
	if (sh_css_enable_interrupt(info, enable) != sh_css_success) {
		dev_warn(isp->dev, "%s:Invalid irq info.\n", __func__);
		return -EINVAL;
	}

	return 0;
}

void atomisp_css_init_struct(struct atomisp_sub_device *asd)
{
	/* obtain the pointers to the default configurations */
	sh_css_get_tnr_config((const struct atomisp_css_tnr_config **)
				&asd->params.default_tnr_config);
	sh_css_get_nr_config((const struct atomisp_css_nr_config **)
				&asd->params.default_nr_config);
	sh_css_get_ee_config((const struct atomisp_css_ee_config **)
				&asd->params.default_ee_config);
	sh_css_get_ob_config((const struct atomisp_css_ob_config **)
				&asd->params.default_ob_config);
	sh_css_get_dp_config((const struct atomisp_css_dp_config **)
				&asd->params.default_dp_config);
	sh_css_get_wb_config((const struct atomisp_css_wb_config **)
				&asd->params.default_wb_config);
	sh_css_get_cc_config((const struct atomisp_css_cc_config **)
				&asd->params.default_cc_config);
	sh_css_get_de_config((const struct atomisp_css_de_config **)
				&asd->params.default_de_config);
	sh_css_get_gc_config((const struct atomisp_css_gc_config **)
				&asd->params.default_gc_config);
	sh_css_get_3a_config((const struct atomisp_css_3a_config **)
				&asd->params.default_3a_config);
	sh_css_get_macc_table((const struct atomisp_css_macc_table **)
				&asd->params.default_macc_table);
	sh_css_get_ctc_table((const struct atomisp_css_ctc_table **)
				&asd->params.default_ctc_table);
	sh_css_get_gamma_table((const struct atomisp_css_gamma_table **)
				&asd->params.default_gamma_table);

	/* we also initialize our configurations with the defaults */
	asd->params.tnr_config  = *asd->params.default_tnr_config;
	asd->params.nr_config   = *asd->params.default_nr_config;
	asd->params.ee_config   = *asd->params.default_ee_config;
	asd->params.ob_config   = *asd->params.default_ob_config;
	asd->params.dp_config   = *asd->params.default_dp_config;
	asd->params.wb_config   = *asd->params.default_wb_config;
	asd->params.cc_config   = *asd->params.default_cc_config;
	asd->params.de_config   = *asd->params.default_de_config;
	asd->params.gc_config   = *asd->params.default_gc_config;
	asd->params.s3a_config  = *asd->params.default_3a_config;
	asd->params.macc_table  = *asd->params.default_macc_table;
	asd->params.ctc_table   = *asd->params.default_ctc_table;
	asd->params.gamma_table =
		*asd->params.default_gamma_table;
}

int atomisp_q_video_buffer_to_css(struct atomisp_sub_device *asd,
			struct videobuf_vmalloc_memory *vm_mem,
			enum atomisp_input_stream_id stream_id,
			enum atomisp_css_buffer_type css_buf_type,
			enum atomisp_css_pipe_id css_pipe_id)
{
	enum sh_css_err err;

	err = sh_css_queue_buffer(css_pipe_id, css_buf_type, vm_mem->vaddr);
	if (err != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_q_s3a_buffer_to_css(struct atomisp_sub_device *asd,
			struct atomisp_s3a_buf *s3a_buf,
			enum atomisp_input_stream_id stream_id,
			enum atomisp_css_pipe_id css_pipe_id)
{
	if (sh_css_queue_buffer(css_pipe_id, SH_CSS_BUFFER_TYPE_3A_STATISTICS,
				&s3a_buf->s3a_data)) {
		dev_dbg(asd->isp->dev, "failed to q s3a stat buffer\n");
		return -EINVAL;
	}

	return 0;
}

int atomisp_q_dis_buffer_to_css(struct atomisp_sub_device *asd,
			struct atomisp_dis_buf *dis_buf,
			enum atomisp_input_stream_id stream_id,
			enum atomisp_css_pipe_id css_pipe_id)
{
	if (sh_css_queue_buffer(css_pipe_id,
				SH_CSS_BUFFER_TYPE_DIS_STATISTICS,
				&dis_buf->dis_data)) {
		dev_dbg(asd->isp->dev, "failed to q dis stat buffer\n");
		return -EINVAL;
	}

	return 0;
}

void atomisp_css_mmu_invalidate_cache(void)
{
	sh_css_mmu_invalidate_cache();
}

void atomisp_css_mmu_invalidate_tlb(void)
{
	sh_css_enable_sp_invalidate_tlb();
}

void atomisp_css_mmu_set_page_table_base_index(unsigned long base_index)
{
	sh_css_mmu_set_page_table_base_index((hrt_data)base_index);
}

int atomisp_css_start(struct atomisp_sub_device *asd,
			enum atomisp_css_pipe_id pipe_id, bool in_reset)
{
	enum sh_css_err err;

	err = sh_css_start(pipe_id);
	if (err != sh_css_success) {
		dev_err(asd->isp->dev, "sh_css_start error:%d.\n", err);
		return -EINVAL;
	}

	return 0;
}

void atomisp_css_update_isp_params(struct atomisp_sub_device *asd)
{
	sh_css_update_isp_params();
}

int atomisp_css_queue_buffer(struct atomisp_sub_device *asd,
			     enum atomisp_input_stream_id stream_id,
			     enum atomisp_css_pipe_id pipe_id,
			     enum atomisp_css_buffer_type buf_type,
			     struct atomisp_css_buffer *isp_css_buffer)
{
	void *buffer;

	switch (buf_type) {
	case SH_CSS_BUFFER_TYPE_3A_STATISTICS:
		buffer = isp_css_buffer->s3a_data;
		break;
	case SH_CSS_BUFFER_TYPE_DIS_STATISTICS:
		buffer = isp_css_buffer->dis_data;
		break;
	case SH_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME:
		buffer = isp_css_buffer->css_buffer.data.frame;
		break;
	case SH_CSS_BUFFER_TYPE_OUTPUT_FRAME:
		buffer = isp_css_buffer->css_buffer.data.frame;
		break;
	default:
		return -EINVAL;
	}

	if (sh_css_queue_buffer(pipe_id, buf_type, buffer) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_dequeue_buffer(struct atomisp_sub_device *asd,
				enum atomisp_input_stream_id stream_id,
				enum atomisp_css_pipe_id pipe_id,
				enum atomisp_css_buffer_type buf_type,
				struct atomisp_css_buffer *isp_css_buffer)
{
	enum sh_css_err err;
	void *buffer;

	err = sh_css_dequeue_buffer(pipe_id, buf_type, (void **)&buffer);
	if (err != sh_css_success) {
		dev_err(asd->isp->dev,
			"sh_css_dequeue_buffer failed: 0x%x\n", err);
		return -EINVAL;
	}

	switch (buf_type) {
	case SH_CSS_BUFFER_TYPE_3A_STATISTICS:
		isp_css_buffer->s3a_data = buffer;
		break;
	case SH_CSS_BUFFER_TYPE_DIS_STATISTICS:
		isp_css_buffer->dis_data = buffer;
		break;
	case SH_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME:
		isp_css_buffer->css_buffer.data.frame = buffer;
		break;
	case SH_CSS_BUFFER_TYPE_OUTPUT_FRAME:
		isp_css_buffer->css_buffer.data.frame = buffer;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int atomisp_css_allocate_3a_dis_bufs(struct atomisp_sub_device *asd,
				struct atomisp_s3a_buf *s3a_buf,
				struct atomisp_dis_buf *dis_buf)
{
	if (sh_css_allocate_stat_buffers_from_info(&s3a_buf->s3a_data,
			&dis_buf->dis_data, &asd->params.curr_grid_info)
			!= sh_css_success) {
		dev_err(asd->isp->dev, "3a and dis buf allocation failed.\n");
		return -EINVAL;
	}

	return 0;
}

void atomisp_css_free_3a_buffers(struct atomisp_s3a_buf *s3a_buf)
{
	sh_css_free_stat_buffers(&s3a_buf->s3a_data, NULL);
}

void atomisp_css_free_dis_buffers(struct atomisp_dis_buf *dis_buf)
{
	sh_css_free_stat_buffers(NULL, &dis_buf->dis_data);
}

void atomisp_css_free_3a_dis_buffers(struct atomisp_sub_device *asd)
{
	struct atomisp_s3a_buf *s3a_buf, *_s3a_buf;
	struct atomisp_dis_buf *dis_buf, *_dis_buf;

	/* 3A statistics use vmalloc, DIS use kmalloc */
	vfree(asd->params.s3a_output_buf);
	asd->params.s3a_output_buf = NULL;
	asd->params.s3a_output_bytes = 0;
	asd->params.s3a_buf_data_valid = false;

	kfree(asd->params.dis_hor_proj_buf);
	kfree(asd->params.dis_ver_proj_buf);
	kfree(asd->params.dis_hor_coef_buf);
	kfree(asd->params.dis_ver_coef_buf);

	asd->params.dis_hor_proj_buf = NULL;
	asd->params.dis_ver_proj_buf = NULL;
	asd->params.dis_hor_coef_buf = NULL;
	asd->params.dis_ver_coef_buf = NULL;
	asd->params.dis_hor_proj_bytes = 0;
	asd->params.dis_ver_proj_bytes = 0;
	asd->params.dis_hor_coef_bytes = 0;
	asd->params.dis_ver_coef_bytes = 0;
	asd->params.dis_proj_data_valid = false;

	list_for_each_entry_safe(s3a_buf, _s3a_buf, &asd->s3a_stats, list) {
		sh_css_free_stat_buffers(&s3a_buf->s3a_data, NULL);
		list_del(&s3a_buf->list);
		kfree(s3a_buf);
	}

	list_for_each_entry_safe(dis_buf, _dis_buf, &asd->dis_stats, list) {
		sh_css_free_stat_buffers(NULL, &dis_buf->dis_data);
		list_del(&dis_buf->list);
		kfree(dis_buf);
	}
}

int atomisp_css_get_grid_info(struct atomisp_sub_device *asd,
				enum atomisp_css_pipe_id pipe_id,
				int stream_index)
{
	enum sh_css_err err;
	struct atomisp_css_grid_info old_info = asd->params.curr_grid_info;

	switch (asd->run_mode->val) {
	case ATOMISP_RUN_MODE_PREVIEW:
		err = sh_css_preview_get_grid_info(
					&asd->params.curr_grid_info);
		if (err != sh_css_success) {
			dev_err(asd->isp->dev,
				 "sh_css_preview_get_grid_info failed: %d\n",
				 err);
			return -EINVAL;
		}
		break;
	case ATOMISP_RUN_MODE_VIDEO:
		err = sh_css_video_get_grid_info(
					&asd->params.curr_grid_info);
		if (err != sh_css_success) {
			dev_err(asd->isp->dev,
				 "sh_css_video_get_grid_info failed: %d\n",
				 err);
			return -EINVAL;
		}
		break;
	default:
		err = sh_css_capture_get_grid_info(
					&asd->params.curr_grid_info);
		if (err != sh_css_success) {
			dev_err(asd->isp->dev,
				 "sh_css_capture_get_grid_info failed: %d\n",
				 err);
			return -EINVAL;
		}
	}

	/* If the grid info has not changed and the buffers for 3A and
	 * DIS statistics buffers are allocated or buffer size would be zero
	 * then no need to do anything. */
	if ((!memcmp(&old_info, &asd->params.curr_grid_info, sizeof(old_info))
	    && asd->params.s3a_output_buf
	    && asd->params.dis_hor_coef_buf)
	    || asd->params.curr_grid_info.s3a_grid.width == 0
	    || asd->params.curr_grid_info.s3a_grid.height == 0)
		return -EINVAL;

	return 0;
}

int atomisp_alloc_3a_output_buf(struct atomisp_sub_device *asd)
{
	/* 3A statistics. These can be big, so we use vmalloc. */
	asd->params.s3a_output_bytes =
		asd->params.curr_grid_info.s3a_grid.width *
		asd->params.curr_grid_info.s3a_grid.height *
		sizeof(*asd->params.s3a_output_buf);

	dev_dbg(asd->isp->dev, "asd->params.s3a_output_bytes: %d\n",
		asd->params.s3a_output_bytes);
	asd->params.s3a_output_buf = vmalloc(
					asd->params.s3a_output_bytes);

	if (asd->params.s3a_output_buf == NULL
	    && asd->params.s3a_output_bytes != 0)
		return -ENOMEM;

	memset(asd->params.s3a_output_buf, 0, asd->params.s3a_output_bytes);
	asd->params.s3a_buf_data_valid = false;

	return 0;
}

int atomisp_alloc_dis_coef_buf(struct atomisp_sub_device *asd)
{
	/* DIS coefficients. */
	asd->params.dis_hor_coef_bytes =
		asd->params.curr_grid_info.dvs_hor_coef_num *
		SH_CSS_DIS_NUM_COEF_TYPES *
		sizeof(*asd->params.dis_hor_coef_buf);

	asd->params.dis_ver_coef_bytes =
		asd->params.curr_grid_info.dvs_ver_coef_num *
		SH_CSS_DIS_NUM_COEF_TYPES *
		sizeof(*asd->params.dis_ver_coef_buf);

	asd->params.dis_hor_coef_buf =
		kzalloc(asd->params.dis_hor_coef_bytes, GFP_KERNEL);
	if (asd->params.dis_hor_coef_buf == NULL &&
			asd->params.dis_hor_coef_bytes != 0)
		return -ENOMEM;

	asd->params.dis_ver_coef_buf =
		kzalloc(asd->params.dis_ver_coef_bytes, GFP_KERNEL);
	if (asd->params.dis_ver_coef_buf == NULL &&
			asd->params.dis_ver_coef_bytes != 0)
		return -ENOMEM;

	/* DIS projections. */
	asd->params.dis_proj_data_valid = false;
	asd->params.dis_hor_proj_bytes =
		asd->params.curr_grid_info.dvs_grid.aligned_height *
		SH_CSS_DIS_NUM_COEF_TYPES *
		sizeof(*asd->params.dis_hor_proj_buf);

	asd->params.dis_ver_proj_bytes =
		asd->params.curr_grid_info.dvs_grid.aligned_width *
		SH_CSS_DIS_NUM_COEF_TYPES *
		sizeof(*asd->params.dis_ver_proj_buf);

	asd->params.dis_hor_proj_buf = kzalloc(
					asd->params.dis_hor_proj_bytes,
					GFP_KERNEL);
	if (asd->params.dis_hor_proj_buf == NULL &&
			asd->params.dis_hor_proj_bytes != 0)
		return -ENOMEM;

	asd->params.dis_ver_proj_buf = kzalloc(
					asd->params.dis_ver_proj_bytes,
					GFP_KERNEL);
	if (asd->params.dis_ver_proj_buf == NULL &&
			asd->params.dis_ver_proj_bytes != 0)
		return -ENOMEM;

	return 0;
}

int atomisp_css_get_3a_statistics(struct atomisp_sub_device *asd,
				  struct atomisp_css_buffer *isp_css_buffer)
{
	enum sh_css_err err;

	if (asd->params.s3a_output_buf && asd->params.s3a_output_bytes) {
		/* To avoid racing with atomisp_3a_stat() */
		err = sh_css_get_3a_statistics(
			asd->params.s3a_output_buf,
			asd->params.curr_grid_info.s3a_grid.use_dmem,
			isp_css_buffer->s3a_data);
		if (err != sh_css_success) {
			dev_err(asd->isp->dev,
				"sh_css_get_3a_statistics failed: 0x%x\n", err);
			return -EINVAL;
		}
		asd->params.s3a_buf_data_valid = true;
	}

	return 0;
}

void atomisp_css_get_dis_statistics(struct atomisp_sub_device *asd,
				    struct atomisp_css_buffer *isp_css_buffer)
{
	if (asd->params.dis_ver_proj_bytes
	    && asd->params.dis_ver_proj_buf
	    && asd->params.dis_hor_proj_buf
	    && asd->params.dis_hor_proj_bytes) {
		/* To avoid racing with atomisp_get_dis_stat()*/
		sh_css_get_dis_projections(asd->params.dis_hor_proj_buf,
				   asd->params.dis_ver_proj_buf,
				   isp_css_buffer->dis_data);
		asd->params.dis_proj_data_valid = true;
	}
}

int atomisp_css_dequeue_event(struct atomisp_css_event *current_event)
{
	if (sh_css_dequeue_event(&current_event->pipe,
				&current_event->event.type) != sh_css_success)
		return -EINVAL;

	return 0;
}

void atomisp_css_temp_pipe_to_pipe_id(struct atomisp_css_event *current_event)
{
}

int atomisp_css_input_set_resolution(struct atomisp_sub_device *asd,
					enum atomisp_input_stream_id stream_id,
					struct v4l2_mbus_framefmt *ffmt)
{
	if (sh_css_input_set_resolution(ffmt->width, ffmt->height)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

void atomisp_css_input_set_binning_factor(struct atomisp_sub_device *asd,
					enum atomisp_input_stream_id stream_id,
					unsigned int bin_factor)
{
	sh_css_input_set_binning_factor(bin_factor);
}

void atomisp_css_input_set_bayer_order(struct atomisp_sub_device *asd,
				enum atomisp_input_stream_id stream_id,
				enum atomisp_css_bayer_order bayer_order)
{
	sh_css_input_set_bayer_order(bayer_order);
}

void atomisp_css_input_set_format(struct atomisp_sub_device *asd,
				enum atomisp_input_stream_id stream_id,
				enum atomisp_css_stream_format format)
{
	sh_css_input_set_format(format);
}

int atomisp_css_input_set_effective_resolution(
					struct atomisp_sub_device *asd,
					enum atomisp_input_stream_id stream_id,
					unsigned int width, unsigned int height)
{
	if (sh_css_input_set_effective_resolution(width, height)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

void atomisp_css_video_set_dis_envelope(struct atomisp_sub_device *asd,
					unsigned int dvs_w, unsigned int dvs_h)
{
	sh_css_video_set_dis_envelope(dvs_w, dvs_h);
}

void atomisp_css_input_set_two_pixels_per_clock(
					struct atomisp_sub_device *asd,
					bool two_ppc)
{
	sh_css_input_set_two_pixels_per_clock(two_ppc);
}

void atomisp_css_enable_raw_binning(struct atomisp_sub_device *asd,
					bool enable)
{
	sh_css_enable_raw_binning(enable);
}

void atomisp_css_enable_dz(struct atomisp_sub_device *asd, bool enable)
{
}

void atomisp_css_capture_set_mode(struct atomisp_sub_device *asd,
				enum atomisp_css_capture_mode mode)
{
	sh_css_capture_set_mode(mode);
}

void atomisp_css_input_set_mode(struct atomisp_sub_device *asd,
				enum atomisp_css_input_mode mode)
{
	sh_css_input_set_mode(mode);
}

void atomisp_css_capture_enable_online(struct atomisp_sub_device *asd,
							bool enable)
{
	sh_css_capture_enable_online(enable);
}

void atomisp_css_preview_enable_online(struct atomisp_sub_device *asd,
							bool enable)
{
	sh_css_preview_enable_online(enable);
}

void atomisp_css_enable_continuous(struct atomisp_sub_device *asd,
							bool enable)
{
	sh_css_enable_continuous(enable);
}

void atomisp_css_enable_cont_capt(bool enable, bool stop_copy_preview)
{
	sh_css_enable_cont_capt(enable, stop_copy_preview);
}

int atomisp_css_input_configure_port(struct atomisp_sub_device *asd,
					mipi_port_ID_t port,
					unsigned int num_lanes,
					unsigned int timeout)
{
	if (sh_css_input_configure_port(port, num_lanes, timeout)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_frame_allocate(struct atomisp_css_frame **frame,
				unsigned int width, unsigned int height,
				enum atomisp_css_frame_format format,
				unsigned int padded_width,
				unsigned int raw_bit_depth)
{
	if (sh_css_frame_allocate(frame, width, height, format,
			padded_width, raw_bit_depth) != sh_css_success)
		return -ENOMEM;

	return 0;
}

int atomisp_css_frame_allocate_from_info(struct atomisp_css_frame **frame,
				const struct atomisp_css_frame_info *info)
{
	if (sh_css_frame_allocate_from_info(frame, info) != sh_css_success)
		return -ENOMEM;

	return 0;
}

void atomisp_css_frame_free(struct atomisp_css_frame *frame)
{
	sh_css_frame_free(frame);
}

int atomisp_css_frame_map(struct atomisp_css_frame **frame,
				const struct atomisp_css_frame_info *info,
				const void *data, uint16_t attribute,
				void *context)
{
	if (sh_css_frame_map(frame, info, data, attribute, context)
	    != sh_css_success)
		return -ENOMEM;

	return 0;
}

int atomisp_css_set_black_frame(struct atomisp_sub_device *asd,
				const struct atomisp_css_frame *raw_black_frame)
{
	if (sh_css_set_black_frame(raw_black_frame) != sh_css_success)
		return -ENOMEM;

	return 0;
}

int atomisp_css_allocate_continuous_frames(bool init_time,
				struct atomisp_sub_device *asd)
{
	if (sh_css_allocate_continuous_frames(init_time) != sh_css_success)
		return -EINVAL;

	return 0;
}

void atomisp_css_update_continuous_frames(struct atomisp_sub_device *asd)
{
	sh_css_update_continuous_frames();
}

int atomisp_css_stop(struct atomisp_sub_device *asd,
			enum atomisp_css_pipe_id pipe_id, bool in_reset)
{
	enum sh_css_err ret;

	/* No need to dump debug traces when css is stopped. */
	sh_css_set_dtrace_level(0);
	switch (pipe_id) {
	case SH_CSS_PREVIEW_PIPELINE:
		ret = sh_css_preview_stop();
		break;
	case SH_CSS_VIDEO_PIPELINE:
		ret = sh_css_video_stop();
		break;
	case SH_CSS_CAPTURE_PIPELINE:
		/* fall through */
	default:
		ret = sh_css_capture_stop();
		break;
	}
	sh_css_set_dtrace_level(CSS_DTRACE_VERBOSITY_LEVEL);

	if (ret != sh_css_success) {
		dev_err(asd->isp->dev, "stop css fatal error.\n");
		return ret == sh_css_err_internal_error ? -EIO : -EINVAL;
	}

	return 0;
}

int atomisp_css_continuous_set_num_raw_frames(
					struct atomisp_sub_device *asd,
					int num_frames)
{
	int max_raw_frames = sh_css_continuous_get_max_raw_frames();

	if (num_frames > max_raw_frames) {
		dev_warn(asd->isp->dev, "continuous_num_raw_frames %d->%d\n",
				num_frames, max_raw_frames);
		num_frames = max_raw_frames;
	}

	if (sh_css_continuous_set_num_raw_frames(num_frames) != sh_css_success)
		return -EINVAL;

	return 0;
}

void atomisp_css_disable_vf_pp(struct atomisp_sub_device *asd,
			       bool disable)
{
	sh_css_disable_vf_pp(disable);
}

int atomisp_css_preview_configure_output(struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format format)
{
	if (sh_css_preview_configure_output(width, height, min_width, format)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_configure_output(struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format format)
{
	if (sh_css_capture_configure_output(width, height, min_width, format)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_video_configure_output(struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format format)
{
	if (sh_css_video_configure_output(width, height, min_width, format)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_video_configure_viewfinder(
				struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format format)
{
	if (sh_css_video_configure_viewfinder(width, height, min_width, format)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_configure_viewfinder(
				struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format format)
{
	if (sh_css_capture_configure_viewfinder(width, height, min_width,
						format) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_video_get_viewfinder_frame_info(
					struct atomisp_sub_device *asd,
					struct atomisp_css_frame_info *info)
{
	if (sh_css_video_get_viewfinder_frame_info(info) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_get_viewfinder_frame_info(
					struct atomisp_sub_device *asd,
					struct atomisp_css_frame_info *info)
{
	if (sh_css_capture_get_viewfinder_frame_info(info)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_get_output_raw_frame_info(
					struct atomisp_sub_device *asd,
					struct atomisp_css_frame_info *info)
{
	if (sh_css_capture_get_output_raw_frame_info(info)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_preview_get_output_frame_info(
					struct atomisp_sub_device *asd,
					struct atomisp_css_frame_info *info)
{
	if (sh_css_preview_get_output_frame_info(info) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_get_output_frame_info(
					struct atomisp_sub_device *asd,
					struct atomisp_css_frame_info *info)
{
	if (sh_css_capture_get_output_frame_info(info) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_video_get_output_frame_info(
					struct atomisp_sub_device *asd,
					struct atomisp_css_frame_info *info)
{
	if (sh_css_video_get_output_frame_info(info) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_get_css_frame_info(struct atomisp_sub_device *asd,
				uint16_t source_pad,
				struct atomisp_css_frame_info *frame_info)
{
	enum sh_css_err ret = sh_css_err_internal_error;

	switch (source_pad) {
	case ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE:
	case ATOMISP_SUBDEV_PAD_SOURCE_VIDEO:
		if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO
		    || asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER)
			ret = sh_css_video_get_output_frame_info(frame_info);
		else
			ret = sh_css_capture_get_output_frame_info(frame_info);
		break;
	case ATOMISP_SUBDEV_PAD_SOURCE_VF:
		if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO)
			ret = sh_css_video_get_viewfinder_frame_info(
					frame_info);
		else if (!atomisp_is_mbuscode_raw(
				asd->fmt[asd->capture_pad].fmt.code))
			ret = sh_css_capture_get_viewfinder_frame_info(
					frame_info);
		break;
	case ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW:
		if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO)
			ret = sh_css_video_get_viewfinder_frame_info(
					frame_info);
		else
			ret = sh_css_preview_get_output_frame_info(frame_info);
		break;
	default:
		/* Return with error */
		break;
	}

	return ret != sh_css_success ? -EINVAL : 0;
}

int atomisp_css_preview_configure_pp_input(
				struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height)
{
	if (sh_css_preview_configure_pp_input(width, height) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_configure_pp_input(
				struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height)
{
	if (sh_css_capture_configure_pp_input(width, height) != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_video_configure_pp_input(
				struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height)
{
	/* Not supported on CSS1.5, Dummy function for compiling. */
	return 0;
}

int atomisp_css_offline_capture_configure(struct atomisp_sub_device *asd,
				int num_captures, unsigned int skip, int offset)
{
	if (sh_css_offline_capture_configure(num_captures, skip, offset)
	    != sh_css_success)
		return -EINVAL;

	return 0;
}

int atomisp_css_capture_enable_xnr(struct atomisp_sub_device *asd,
				   bool enable)
{
	sh_css_capture_enable_xnr(enable);
	return 0;
}

void atomisp_css_send_input_frame(struct atomisp_sub_device *asd,
				  unsigned short *data, unsigned int width,
				  unsigned int height)
{
	sh_css_send_input_frame(data, width, height);
}

bool atomisp_css_isp_has_started(void)
{
	return sh_css_isp_has_started();
}

void atomisp_css_request_flash(struct atomisp_sub_device *asd)
{
	sh_css_request_flash();
}

void atomisp_css_set_wb_config(struct atomisp_sub_device *asd,
			struct atomisp_css_wb_config *wb_config)
{
	sh_css_set_wb_config(wb_config);
}

void atomisp_css_set_ob_config(struct atomisp_sub_device *asd,
			struct atomisp_css_ob_config *ob_config)
{
	sh_css_set_ob_config(ob_config);
}

void atomisp_css_set_dp_config(struct atomisp_sub_device *asd,
			struct atomisp_css_dp_config *dp_config)
{
	sh_css_set_dp_config(dp_config);
}

void atomisp_css_set_de_config(struct atomisp_sub_device *asd,
			struct atomisp_css_de_config *de_config)
{
	sh_css_set_de_config(de_config);
}

void atomisp_css_set_default_de_config(struct atomisp_sub_device *asd)
{
	sh_css_set_de_config(asd->params.default_de_config);
}

void atomisp_css_set_ce_config(struct atomisp_sub_device *asd,
			struct atomisp_css_ce_config *ce_config)
{
	sh_css_set_ce_config(ce_config);
}

void atomisp_css_set_nr_config(struct atomisp_sub_device *asd,
			struct atomisp_css_nr_config *nr_config)
{
	sh_css_set_nr_config(nr_config);
}

void atomisp_css_set_ee_config(struct atomisp_sub_device *asd,
			struct atomisp_css_ee_config *ee_config)
{
	sh_css_set_ee_config(ee_config);
}

void atomisp_css_set_tnr_config(struct atomisp_sub_device *asd,
			struct atomisp_css_tnr_config *tnr_config)
{
	sh_css_set_tnr_config(tnr_config);
}

void atomisp_css_set_cc_config(struct atomisp_sub_device *asd,
			struct atomisp_css_cc_config *cc_config)
{
	sh_css_set_cc_config(cc_config);
}

void atomisp_css_set_macc_table(struct atomisp_sub_device *asd,
			struct atomisp_css_macc_table *macc_table)
{
	sh_css_set_macc_table(macc_table);
}

void atomisp_css_set_gamma_table(struct atomisp_sub_device *asd,
			struct atomisp_css_gamma_table *gamma_table)
{
	sh_css_set_gamma_table(gamma_table);
}

void atomisp_css_set_ctc_table(struct atomisp_sub_device *asd,
			struct atomisp_css_ctc_table *ctc_table)
{
	sh_css_set_ctc_table(ctc_table);
}

void atomisp_css_set_gc_config(struct atomisp_sub_device *asd,
			struct atomisp_css_gc_config *gc_config)
{
	sh_css_set_gc_config(gc_config);
}

void atomisp_css_set_3a_config(struct atomisp_sub_device *asd,
			struct atomisp_css_3a_config *s3a_config)
{
	sh_css_set_3a_config(s3a_config);
}

void atomisp_css_video_set_dis_vector(struct atomisp_sub_device *asd,
				struct atomisp_dis_vector *vector)
{
	sh_css_video_set_dis_vector(vector->x, vector->y);
}

int atomisp_css_set_dis_coefs(struct atomisp_sub_device *asd,
			  struct atomisp_dis_coefficients *coefs)
{
	if (coefs->horizontal_coefficients == NULL ||
	    coefs->vertical_coefficients   == NULL ||
	    asd->params.dis_hor_coef_buf   == NULL ||
	    asd->params.dis_ver_coef_buf   == NULL)
		return -EINVAL;

	if (copy_from_user(asd->params.dis_hor_coef_buf,
	    coefs->horizontal_coefficients, asd->params.dis_hor_coef_bytes))
		return -EFAULT;
	if (copy_from_user(asd->params.dis_ver_coef_buf,
	    coefs->vertical_coefficients, asd->params.dis_ver_coef_bytes))
		return -EFAULT;

	sh_css_set_dis_coefficients(asd->params.dis_hor_coef_buf,
				    asd->params.dis_ver_coef_buf);
	asd->params.dis_proj_data_valid = false;

	return 0;
}

void atomisp_css_set_zoom_factor(struct atomisp_sub_device *asd,
					unsigned int zoom)
{
	sh_css_set_zoom_factor(zoom, zoom);
}

int atomisp_css_get_wb_config(struct atomisp_sub_device *asd,
			struct atomisp_wb_config *config)
{
	memcpy(config, &asd->params.wb_config, sizeof(*config));
	return 0;
}

int atomisp_css_get_ob_config(struct atomisp_sub_device *asd,
			struct atomisp_ob_config *config)
{
	const struct atomisp_css_ob_config *ob_config;
	sh_css_get_ob_config(&ob_config);
	memcpy(config, ob_config, sizeof(*config));

	return 0;
}

int atomisp_css_get_dp_config(struct atomisp_sub_device *asd,
			struct atomisp_dp_config *config)
{
	memcpy(config, &asd->params.dp_config, sizeof(*config));
	return 0;
}

int atomisp_css_get_de_config(struct atomisp_sub_device *asd,
			struct atomisp_de_config *config)
{
	memcpy(config, &asd->params.de_config, sizeof(*config));
	return 0;
}

int atomisp_css_get_nr_config(struct atomisp_sub_device *asd,
			struct atomisp_nr_config *config)
{
	const struct atomisp_css_nr_config *nr_config;
	sh_css_get_nr_config(&nr_config);
	memcpy(config, nr_config, sizeof(*config));

	return 0;
}

int atomisp_css_get_ee_config(struct atomisp_sub_device *asd,
			struct atomisp_ee_config *config)
{
	const struct atomisp_css_ee_config *ee_config;
	sh_css_get_ee_config(&ee_config);
	memcpy(config, ee_config, sizeof(*config));

	return 0;
}

int atomisp_css_get_tnr_config(struct atomisp_sub_device *asd,
			struct atomisp_tnr_config *config)
{
	memcpy(config, &asd->params.tnr_config, sizeof(*config));
	return 0;
}

int atomisp_css_get_ctc_table(struct atomisp_sub_device *asd,
			struct atomisp_ctc_table *config)
{
	const struct sh_css_ctc_table *tab;

	sh_css_get_ctc_table(&tab);
	memcpy(config, tab->data, sizeof(tab->data));
	return 0;
}

int atomisp_css_get_gamma_table(struct atomisp_sub_device *asd,
			struct atomisp_gamma_table *config)
{
	const struct sh_css_gamma_table *tab;

	sh_css_get_gamma_table(&tab);
	memcpy(config, tab->data, sizeof(tab->data));

	return 0;
}

int atomisp_css_get_gc_config(struct atomisp_sub_device *asd,
			struct atomisp_gc_config *config)
{
	memcpy(config, &asd->params.gc_config, sizeof(*config));
	return 0;
}

int atomisp_css_get_3a_config(struct atomisp_sub_device *asd,
			struct atomisp_3a_config *config)
{
	memcpy(config, &asd->params.s3a_config, sizeof(*config));
	return 0;
}

int atomisp_css_get_zoom_factor(struct atomisp_sub_device *asd,
					unsigned int *zoom)
{
	sh_css_get_zoom_factor(zoom, zoom);
	return 0;
}

/*
 * Function to set/get image stablization statistics
 */
int atomisp_css_get_dis_stat(struct atomisp_sub_device *asd,
			 struct atomisp_dis_statistics *stats)
{
	struct atomisp_device *isp = asd->isp;
	unsigned long flags;
	int error;

	if (stats->vertical_projections   == NULL ||
	    stats->horizontal_projections == NULL ||
	    asd->params.dis_hor_proj_buf  == NULL ||
	    asd->params.dis_ver_proj_buf  == NULL)
		return -EINVAL;

	/* isp needs to be streaming to get DIS statistics */
	spin_lock_irqsave(&isp->lock, flags);
	if (asd->streaming != ATOMISP_DEVICE_STREAMING_ENABLED) {
		spin_unlock_irqrestore(&isp->lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	if (!asd->params.video_dis_en)
		return -EINVAL;

	if (atomisp_compare_grid(asd, &stats->grid_info) != 0)
		/* If the grid info in the argument differs from the current
		   grid info, we tell the caller to reset the grid size and
		   try again. */
		return -EAGAIN;

	if (!asd->params.dis_proj_data_valid)
		return -EBUSY;

	error = copy_to_user(stats->vertical_projections,
			     asd->params.dis_ver_proj_buf,
			     asd->params.dis_ver_proj_bytes);

	error |= copy_to_user(stats->horizontal_projections,
			     asd->params.dis_hor_proj_buf,
			     asd->params.dis_hor_proj_bytes);

	if (error)
		return -EFAULT;

	return 0;
}

struct atomisp_css_shading_table *atomisp_css_shading_table_alloc(
				unsigned int width, unsigned int height)
{
	return sh_css_shading_table_alloc(width, height);
}

void atomisp_css_set_shading_table(struct atomisp_sub_device *asd,
				struct atomisp_css_shading_table *table)
{
	sh_css_set_shading_table(table);
}

void atomisp_css_shading_table_free(struct atomisp_css_shading_table *table)
{
	sh_css_shading_table_free(table);
}

struct atomisp_css_morph_table *atomisp_css_morph_table_allocate(
				unsigned int width, unsigned int height)
{
	return sh_css_morph_table_allocate(width, height);
}

void atomisp_css_set_morph_table(struct atomisp_sub_device *asd,
					struct atomisp_css_morph_table *table)
{
	sh_css_set_morph_table(table);
}

void atomisp_css_get_morph_table(struct atomisp_sub_device *asd,
				struct atomisp_css_morph_table *table)
{
	const struct atomisp_css_morph_table *tab;
	sh_css_get_morph_table(&tab);
	memcpy(table, tab, sizeof(*table));
}

void atomisp_css_morph_table_free(struct atomisp_css_morph_table *table)
{
	sh_css_morph_table_free(table);
}

void atomisp_css_set_cont_prev_start_time(struct atomisp_device *isp,
					unsigned int overlap)
{
	sh_css_set_cont_prev_start_time(overlap);
}

int atomisp_css_update_stream(struct atomisp_sub_device *asd)
{
	return 0;
}

int atomisp_css_wait_acc_finish(struct atomisp_sub_device *asd)
{
	if (sh_css_wait_for_completion(SH_CSS_ACC_PIPELINE) != sh_css_success)
		return -EIO;

	return 0;
}

void atomisp_css_acc_done(struct atomisp_sub_device *asd)
{
}

int atomisp_css_create_acc_pipe(struct atomisp_sub_device *asd)
{
	struct atomisp_device *isp = asd->isp;

	isp->acc.pipeline = sh_css_create_pipeline();
	if (!isp->acc.pipeline)
		return -EBADE;

	return 0;
}

int atomisp_css_start_acc_pipe(struct atomisp_sub_device *asd)
{
	unsigned int i = 0;
	struct atomisp_device *isp = asd->isp;

	sh_css_start_pipeline(SH_CSS_ACC_PIPELINE, isp->acc.pipeline);
	/* wait 2-4ms before failing */
	while (!sh_css_sp_has_booted()) {
		if (i > 100) {
			atomisp_reset(isp);
			return -EIO;
		}
		usleep_range(20, 40);
		i++;
	}

	sh_css_init_buffer_queues();
	return 0;
}

int atomisp_css_stop_acc_pipe(struct atomisp_sub_device *asd)
{
	enum sh_css_err ret;
	ret = sh_css_acceleration_stop();
	if (ret != sh_css_success) {
		dev_err(asd->isp->dev, "cannot stop acceleration pipeline\n");
		return ret == sh_css_err_internal_error ? -EIO : -EINVAL;
	}
	return 0;
}

void atomisp_css_destroy_acc_pipe(struct atomisp_sub_device *asd)
{
	struct atomisp_device *isp = asd->isp;

	if (isp->acc.pipeline) {
		sh_css_destroy_pipeline(isp->acc.pipeline);
		isp->acc.pipeline = NULL;
	}
}

/* Set the ACC binary arguments */
int atomisp_css_set_acc_parameters(struct atomisp_acc_fw *acc_fw)
{
	struct sh_css_hmm_section sec;
	unsigned int mem;

	for (mem = 0; mem < ATOMISP_ACC_NR_MEMORY; mem++) {
		if (acc_fw->args[mem].length == 0)
			continue;

		sec.ddr_address = acc_fw->args[mem].css_ptr;
		sec.ddr_size = acc_fw->args[mem].length;
		if (sh_css_acc_set_firmware_parameters(acc_fw->fw, mem, sec)
			!= sh_css_success)
			return -EIO;
	}

	return 0;
}

/* Load acc binary extension */
int atomisp_css_load_acc_extension(struct atomisp_sub_device *asd,
					struct atomisp_css_fw_info *fw,
					enum atomisp_css_pipe_id pipe_id,
					unsigned int type)
{
	if (sh_css_load_extension(fw, pipe_id, type) != sh_css_success)
		return -EBADSLT;

	return 0;
}

/* Unload acc binary extension */
void atomisp_css_unload_acc_extension(struct atomisp_sub_device *asd,
					struct atomisp_css_fw_info *fw,
					enum atomisp_css_pipe_id pipe_id)
{
	sh_css_unload_extension(fw, pipe_id);
}

int atomisp_css_load_acc_binary(struct atomisp_sub_device *asd,
					struct atomisp_css_fw_info *fw,
					unsigned int index)
{
	struct atomisp_device *isp = asd->isp;

	if (sh_css_pipeline_add_acc_stage(
	    isp->acc.pipeline, fw) != sh_css_success)
		return -EBADSLT;

	return 0;
}

void atomisp_set_stop_timeout(unsigned int timeout)
{
	sh_css_set_stop_timeout(timeout);
}

int atomisp_css_isr_thread(struct atomisp_device *isp,
			   bool *frame_done_found,
			   bool *css_pipe_done,
			   bool *reset_wdt_timer)
{
	struct atomisp_css_event current_event;
	DEFINE_KFIFO(events, struct atomisp_css_event, ATOMISP_CSS_EVENTS_MAX);
	/* ISP1.5 has only 1 stream */
	struct atomisp_sub_device *asd = &isp->asd[0];

	while (!atomisp_css_dequeue_event(&current_event)) {
		switch (current_event.event.type) {
		case CSS_EVENT_PIPELINE_DONE:
			css_pipe_done[asd->index] = true;
			break;
		case CSS_EVENT_OUTPUT_FRAME_DONE:
		case CSS_EVENT_VF_OUTPUT_FRAME_DONE:
			*reset_wdt_timer = true; /* ISP running */
			/* fall through */
		case CSS_EVENT_3A_STATISTICS_DONE:
		case CSS_EVENT_DIS_STATISTICS_DONE:
			break;
		default:
			dev_err(isp->dev, "unknown event 0x%x pipe:%d\n",
				current_event.event.type, current_event.pipe);
			break;
		}
		kfifo_in(&events, &current_event, 1);
	}

	while (kfifo_out(&events, &current_event, 1)) {
		atomisp_css_temp_pipe_to_pipe_id(&current_event);
		switch (current_event.event.type) {
		case CSS_EVENT_OUTPUT_FRAME_DONE:
			frame_done_found[asd->index] = true;
			atomisp_buf_done(asd, 0, CSS_BUFFER_TYPE_OUTPUT_FRAME,
					 current_event.pipe, true,
					 ATOMISP_INPUT_STREAM_GENERAL);
			break;
		case CSS_EVENT_3A_STATISTICS_DONE:
			atomisp_buf_done(asd, 0,
					 CSS_BUFFER_TYPE_3A_STATISTICS,
					 current_event.pipe,
					 css_pipe_done[asd->index],
					 ATOMISP_INPUT_STREAM_GENERAL);
			break;
		case CSS_EVENT_VF_OUTPUT_FRAME_DONE:
			atomisp_buf_done(asd, 0,
					 CSS_BUFFER_TYPE_VF_OUTPUT_FRAME,
					 current_event.pipe, true,
					 ATOMISP_INPUT_STREAM_GENERAL);
			break;
		case CSS_EVENT_DIS_STATISTICS_DONE:
			atomisp_buf_done(asd, 0,
					 CSS_BUFFER_TYPE_DIS_STATISTICS,
					 current_event.pipe,
					 css_pipe_done[asd->index],
					 ATOMISP_INPUT_STREAM_GENERAL);
			break;
		case CSS_EVENT_PIPELINE_DONE:
			break;
		default:
			dev_err(isp->dev, "unhandled css stored event: 0x%x\n",
					current_event.event.type);
			break;
		}
	}

	return 0;
}
