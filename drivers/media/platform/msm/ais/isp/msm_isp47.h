/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_ISP47_H__
#define __MSM_ISP47_H__

#define VFE47_NUM_STATS_COMP 2
#define VFE47_NUM_STATS_TYPE 9
/* composite mask order */
enum msm_vfe47_stats_comp_idx {
	STATS_COMP_IDX_HDR_BE = 0,
	STATS_COMP_IDX_BG,
	STATS_COMP_IDX_BF,
	STATS_COMP_IDX_HDR_BHIST,
	STATS_COMP_IDX_RS,
	STATS_COMP_IDX_CS,
	STATS_COMP_IDX_IHIST,
	STATS_COMP_IDX_BHIST,
	STATS_COMP_IDX_AEC_BG,
};

extern struct msm_vfe_hardware_info vfe47_hw_info;

uint32_t msm_vfe47_ub_reg_offset(struct vfe_device *vfe_dev, int wm_idx);
uint32_t msm_vfe47_get_ub_size(struct vfe_device *vfe_dev);
void msm_vfe47_read_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1);
void msm_vfe47_read_irq_status_and_clear(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1);
void msm_vfe47_enable_camif_error(struct vfe_device *vfe_dev,
			int enable);
void msm_vfe47_process_reg_update(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts);
void msm_vfe47_process_epoch_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts);
void msm_isp47_process_sof_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts);
void msm_isp47_process_eof_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0);
void msm_vfe47_reg_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src);
long msm_vfe47_reset_hardware(struct vfe_device *vfe_dev,
	uint32_t first_start, uint32_t blocking_call);
void msm_vfe47_axi_reload_wm(struct vfe_device *vfe_dev,
	void __iomem *vfe_base, uint32_t reload_mask);
void msm_vfe47_axi_update_cgc_override(struct vfe_device *vfe_dev,
	uint8_t wm_idx, uint8_t enable);
void msm_vfe47_axi_cfg_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);
void msm_vfe47_axi_clear_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);
void msm_vfe47_axi_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);
void msm_vfe47_axi_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);
void msm_vfe47_axi_clear_irq_mask(struct vfe_device *vfe_dev);
void msm_vfe47_cfg_framedrop(void __iomem *vfe_base,
	struct msm_vfe_axi_stream *stream_info, uint32_t framedrop_pattern,
	uint32_t framedrop_period);
void msm_vfe47_clear_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info);
int32_t msm_vfe47_cfg_io_format(struct vfe_device *vfe_dev,
	enum msm_vfe_axi_stream_src stream_src, uint32_t io_format);
int msm_vfe47_start_fetch_engine(struct vfe_device *vfe_dev,
	void *arg);
int msm_vfe47_start_fetch_engine_multi_pass(struct vfe_device *vfe_dev,
	void *arg);
void msm_vfe47_cfg_fetch_engine(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg);
void msm_vfe47_cfg_testgen(struct vfe_device *vfe_dev,
	struct msm_vfe_testgen_cfg *testgen_cfg);
void msm_vfe47_cfg_camif(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg);
void msm_vfe47_cfg_input_mux(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg);
void msm_vfe47_configure_hvx(struct vfe_device *vfe_dev,
	uint8_t is_stream_on);
void msm_vfe47_update_camif_state(struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state update_state);
void msm_vfe47_cfg_rdi_reg(
	struct vfe_device *vfe_dev, struct msm_vfe_rdi_cfg *rdi_cfg,
	enum msm_vfe_input_src input_src);
void msm_vfe47_axi_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx);
void msm_vfe47_axi_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx);
void msm_vfe47_axi_cfg_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx);
void msm_vfe47_axi_clear_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx);
void msm_vfe47_cfg_axi_ub_equal_default(
	struct vfe_device *vfe_dev, enum msm_vfe_input_src frame_src);
void msm_vfe47_cfg_axi_ub_equal_slicing(
	struct vfe_device *vfe_dev);
void msm_vfe47_cfg_axi_ub(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src);
void msm_vfe47_read_wm_ping_pong_addr(
	struct vfe_device *vfe_dev);
void msm_vfe47_update_ping_pong_addr(
	void __iomem *vfe_base,
	uint8_t wm_idx, uint32_t pingpong_bit, dma_addr_t paddr,
	int32_t buf_size);
int msm_vfe47_axi_halt(struct vfe_device *vfe_dev,
	uint32_t blocking);
int msm_vfe47_axi_restart(struct vfe_device *vfe_dev,
	uint32_t blocking, uint32_t enable_camif);
uint32_t msm_vfe47_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1);
uint32_t msm_vfe47_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1);
uint32_t msm_vfe47_get_pingpong_status(
	struct vfe_device *vfe_dev);
int msm_vfe47_get_stats_idx(enum msm_isp_stats_type stats_type);
int msm_vfe47_stats_check_streams(
	struct msm_vfe_stats_stream *stream_info);
void msm_vfe47_stats_cfg_comp_mask(
	struct vfe_device *vfe_dev, uint32_t stats_mask,
	uint8_t request_comp_index, uint8_t enable);
void msm_vfe47_stats_cfg_wm_irq_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info);
void msm_vfe47_stats_clear_wm_irq_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info);
void msm_vfe47_stats_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info);
void msm_vfe47_stats_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info);
void msm_vfe47_stats_cfg_ub(struct vfe_device *vfe_dev);
void msm_vfe47_stats_update_cgc_override(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable);
bool msm_vfe47_is_module_cfg_lock_needed(
	uint32_t reg_offset);
void msm_vfe47_stats_enable_module(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable);
void msm_vfe47_stats_update_ping_pong_addr(
	void __iomem *vfe_base, struct msm_vfe_stats_stream *stream_info,
	uint32_t pingpong_status, dma_addr_t paddr);
uint32_t msm_vfe47_stats_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1);
uint32_t msm_vfe47_stats_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1);
uint32_t msm_vfe47_stats_get_frame_id(
	struct vfe_device *vfe_dev);
void msm_vfe47_get_error_mask(
	uint32_t *error_mask0, uint32_t *error_mask1);
void msm_vfe47_get_overflow_mask(uint32_t *overflow_mask);
void msm_vfe47_get_rdi_wm_mask(struct vfe_device *vfe_dev,
	uint32_t *rdi_wm_mask);
void msm_vfe47_get_irq_mask(struct vfe_device *vfe_dev,
	uint32_t *irq0_mask, uint32_t *irq1_mask);
void msm_vfe47_restore_irq_mask(struct vfe_device *vfe_dev);
void msm_vfe47_get_halt_restart_mask(uint32_t *irq0_mask,
	uint32_t *irq1_mask);
int msm_vfe47_init_hardware(struct vfe_device *vfe_dev);
void msm_vfe47_release_hardware(struct vfe_device *vfe_dev);
void msm_vfe47_init_hardware_reg(struct vfe_device *vfe_dev);
void msm_vfe47_process_reset_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1);
void msm_vfe47_process_halt_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1);
void msm_vfe47_process_input_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts);
void msm_vfe47_process_violation_status(
	struct vfe_device *vfe_dev);
void msm_vfe47_process_error_status(struct vfe_device *vfe_dev);
void msm_vfe47_clear_status_reg(struct vfe_device *vfe_dev);
int msm_vfe47_get_platform_data(struct vfe_device *vfe_dev);
int msm_vfe47_enable_regulators(struct vfe_device *vfe_dev, int enable);
int msm_vfe47_get_regulators(struct vfe_device *vfe_dev);
void msm_vfe47_put_regulators(struct vfe_device *vfe_dev);
int msm_vfe47_enable_clks(struct vfe_device *vfe_dev, int enable);
int msm_vfe47_get_clks(struct vfe_device *vfe_dev);
void msm_vfe47_put_clks(struct vfe_device *vfe_dev);
int msm_vfe47_get_clk_rates(struct vfe_device *vfe_dev,
			struct msm_isp_clk_rates *rates);
int msm_vfe47_get_max_clk_rate(struct vfe_device *vfe_dev, long *rate);
int msm_vfe47_set_clk_rate(struct vfe_device *vfe_dev, long *rate);
int msm_vfe47_init_bandwidth_mgr(struct vfe_device *vfe_dev,
		struct msm_isp_bandwidth_mgr *isp_bandwidth_mgr);
void msm_vfe47_deinit_bandwidth_mgr(
		struct msm_isp_bandwidth_mgr *isp_bandwidth_mgr);
int msm_vfe47_update_bandwidth(
		struct msm_isp_bandwidth_mgr *isp_bandwidth_mgr);
void msm_vfe47_config_irq(struct vfe_device *vfe_dev,
		uint32_t irq0_mask, uint32_t irq1_mask,
		enum msm_isp_irq_operation oper);
#endif /* __MSM_ISP47_H__ */
