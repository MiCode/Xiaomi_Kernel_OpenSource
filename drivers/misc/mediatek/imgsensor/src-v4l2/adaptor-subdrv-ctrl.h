/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 MediaTek Inc. */

#ifndef __ADAPTOR_SUBDRV_CTRL_H__
#define __ADAPTOR_SUBDRV_CTRL_H__

#include "adaptor-subdrv.h"

#define DRV_LOG(ctx, format, args...) do { \
	struct v4l2_subdev *_sd = NULL; \
	struct adaptor_ctx *_adaptor_ctx = NULL; \
	if (ctx->i2c_client) \
		_sd = i2c_get_clientdata(ctx->i2c_client); \
	if (_sd) \
		_adaptor_ctx = to_ctx(_sd); \
	if (_adaptor_ctx && (_adaptor_ctx)->subdrv \
		&& unlikely(*((_adaptor_ctx)->sensor_debug_flag))) { \
		dev_info(_adaptor_ctx->dev, "[%s][%s] " format, \
			(_adaptor_ctx)->subdrv->name, __func__, ##args); \
	} \
} while (0)

#define DRV_LOGE(ctx, format, args...) do { \
	struct v4l2_subdev *_sd = NULL; \
	struct adaptor_ctx *_adaptor_ctx = NULL; \
	if (ctx->i2c_client) \
		_sd = i2c_get_clientdata(ctx->i2c_client); \
	if (_sd) \
		_adaptor_ctx = to_ctx(_sd); \
	if (_adaptor_ctx && (_adaptor_ctx)->subdrv) { \
		dev_info(_adaptor_ctx->dev, "[%s][%s] error: " format, \
			(_adaptor_ctx)->subdrv->name, __func__, ##args); \
	} \
} while (0)

#define DRV_LOG_MUST(ctx, format, args...) do { \
	struct v4l2_subdev *_sd = NULL; \
	struct adaptor_ctx *_adaptor_ctx = NULL; \
	if (ctx->i2c_client) \
		_sd = i2c_get_clientdata(ctx->i2c_client); \
	if (_sd) \
		_adaptor_ctx = to_ctx(_sd); \
	if (_adaptor_ctx && (_adaptor_ctx)->subdrv) { \
		dev_info(_adaptor_ctx->dev, "[%s][%s] " format, \
			(_adaptor_ctx)->subdrv->name, __func__, ##args); \
	} \
} while (0)

void check_current_scenario_id_bound(struct subdrv_ctx *ctx);
void i2c_table_write(struct subdrv_ctx *ctx, u16 *list, u32 len);
void commit_i2c_buffer(struct subdrv_ctx *ctx);
void set_i2c_buffer(struct subdrv_ctx *ctx, u16 reg, u16 val);
u16 i2c_read_eeprom(struct subdrv_ctx *ctx, u16 addr);
void get_pdaf_reg_setting(struct subdrv_ctx *ctx, u32 regNum, u16 *regDa);
void set_pdaf_reg_setting(struct subdrv_ctx *ctx, u32 regNum, u16 *regDa);
void set_mirror_flip(struct subdrv_ctx *ctx, u8 image_mirror);
bool probe_eeprom(struct subdrv_ctx *ctx);
void read_sensor_Cali(struct subdrv_ctx *ctx);
void write_sensor_Cali(struct subdrv_ctx *ctx);
void write_frame_length(struct subdrv_ctx *ctx, u32 fll);
void set_dummy(struct subdrv_ctx *ctx);
void set_frame_length(struct subdrv_ctx *ctx, u16 frame_length);
void set_max_framerate(struct subdrv_ctx *ctx, u16 framerate, bool min_framelength_en);
void set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate);
bool set_auto_flicker(struct subdrv_ctx *ctx, bool min_framelength_en);
void set_long_exposure(struct subdrv_ctx *ctx);
void set_shutter(struct subdrv_ctx *ctx, u32 shutter);
void set_shutter_frame_length(struct subdrv_ctx *ctx, u32 shutter, u32 frame_length);
void set_hdr_tri_shutter(struct subdrv_ctx *ctx, u64 *shutters, u16 exp_cnt);
void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
		u32 *shutters, u16 shutter_cnt,	u16 frame_length);
u16 gain2reg(u32 gain);
void set_gain(struct subdrv_ctx *ctx, u32 gain);
void set_hdr_tri_gain(struct subdrv_ctx *ctx, u64 *gains, u16 exp_cnt);
void set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt);
void set_multi_dig_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt);
void get_lens_driver_id(struct subdrv_ctx *ctx, u32 *lens_id);
void check_stream_off(struct subdrv_ctx *ctx);
void streaming_control(struct subdrv_ctx *ctx, bool enable);
void set_video_mode(struct subdrv_ctx *ctx, u16 framerate);
void set_auto_flicker_mode(struct subdrv_ctx *ctx, bool enable, u16 framerate);
void get_output_format_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *sensor_output_dataformat);
void get_ana_gain_table(struct subdrv_ctx *ctx, u64 *size, void *data);
void get_gain_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *min_gain, u64 *max_gain);
void get_base_gain_iso_and_step(struct subdrv_ctx *ctx,
		u64 *min_gain_iso, u64 *gain_step, u64 *gain_type);
void get_dig_gain_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *min_dgain, u64 *max_dgain);
void get_dig_gain_step(struct subdrv_ctx *ctx, u64 *gain_step);
void get_min_shutter_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *min_shutter, u64 *exposure_step);
void get_offset_to_start_of_exposure(struct subdrv_ctx *ctx,	u32 *offset);
void get_pixel_clock_freq_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *pclk);
void get_period_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *period, u64 flag);
void get_period(struct subdrv_ctx *ctx,
		u16 *line_length, u16 *frame_length);
void get_pixel_clock_freq(struct subdrv_ctx *ctx, u32 *pclk);
void get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *framerate);
void get_fine_integ_line_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *fine_integ_line);
void set_test_pattern(struct subdrv_ctx *ctx, u32 mode);
void set_test_pattern_data(struct subdrv_ctx *ctx, struct mtk_test_pattern_data *data);
void get_test_pattern_checksum_value(struct subdrv_ctx *ctx, u32 *checksum);
void set_framerate(struct subdrv_ctx *ctx, u32 framerate);
void set_hdr(struct subdrv_ctx *ctx, u32 mode);
void get_crop_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		struct SENSOR_WINSIZE_INFO_STRUCT *wininfo);
void get_pdaf_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		struct SET_PD_BLOCK_INFO_T *pd_info);
void get_sensor_pdaf_capacity(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *pdaf_cap);
void extend_frame_length(struct subdrv_ctx *ctx, u32 ns);
void seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *ae_ctrl);
void get_seamless_scenarios(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *pScenarios);
void get_sensor_hdr_capacity(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *hdr_mode);
void get_stagger_target_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		enum IMGSENSOR_HDR_MODE_ENUM hdr_mode, u32 *pScenarios);
void get_frame_ctrl_info_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *margin);
void get_feature_get_4cell_data(struct subdrv_ctx *ctx, u16 type, char *data);
void get_stagger_max_exp_time(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		enum VC_FEATURE vc, u64 *exposure_max);
void get_temperature_value(struct subdrv_ctx *ctx, u32 *value);
void set_pdaf(struct subdrv_ctx *ctx, u16 mode);
void get_binning_type(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *binning_ratio);
void get_ae_frame_mode_for_le(struct subdrv_ctx *ctx, u32 *ae_frm_mode);
void get_ae_effective_frame_for_le(struct subdrv_ctx *ctx, u32 *ae_effective_frame);
void preload_eeprom_data(struct subdrv_ctx *ctx, u32 *is_read);
void get_mipi_pixel_rate(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *mipi_pixel_rate);
void get_sensor_rgbw_output_mode(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *rgbw_output_mode);

int common_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
void subdrv_ctx_init(struct subdrv_ctx *ctx);
void sensor_init(struct subdrv_ctx *ctx);
int common_open(struct subdrv_ctx *ctx);
int common_get_info(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data);
int common_get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution);
void update_mode_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id);
bool check_is_no_crop(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id);
int common_control(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data);
int common_feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
	u8 *feature_para, u32 *feature_para_len);
int common_close(struct subdrv_ctx *ctx);
int common_get_frame_desc(struct subdrv_ctx *ctx,
	int scenario_id, struct mtk_mbus_frame_desc *fd);
int common_get_temp(struct subdrv_ctx *ctx, int *temp);
int common_get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param);
int common_update_sof_cnt(struct subdrv_ctx *ctx, u32 sof_cnt);
#endif
