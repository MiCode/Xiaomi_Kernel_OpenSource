/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */


#ifndef _CAM_TFE_CORE_H_
#define _CAM_TFE_CORE_H_

#include <linux/spinlock.h>
#include "cam_hw_intf.h"
#include "cam_tfe_bus.h"
#include "cam_tfe_hw_intf.h"
#include "cam_tfe_irq.h"

#define CAM_TFE_CAMIF_VER_1_0        0x10
#define CAM_TFE_RDI_VER_1_0          0x1000
#define CAM_TFE_TOP_1_0              0x1000
#define CAM_TFE_TOP_IN_PORT_MAX      4
#define CAM_TFE_RDI_MAX              4

#define CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS      BIT(0)
#define CAM_TFE_EVT_MAX                            256

#define CAM_TFE_MAX_REG_DUMP_ENTRIES  20
#define CAM_TFE_MAX_LUT_DUMP_ENTRIES  10

#define CAM_TFE_MAX_CLC               30
#define CAM_TFE_CLC_NAME_LENGTH_MAX   32

/*we take each word as uint32_t, for dumping uint64_t count as 2 words
 * soc index
 * clk_rate--> uint64_t--> count as 2 words
 * BW--> uint64_t --> count as 2 words
 * MAX_NIU
 */
#define CAM_TFE_CORE_DUMP_MISC_NUM_WORDS 4

enum cam_tfe_lut_word_size {
	CAM_TFE_LUT_WORD_SIZE_32,
	CAM_TFE_LUT_WORD_SIZE_64,
	CAM_TFE_LUT_WORD_SIZE_MAX,
};

struct cam_tfe_reg_dump_entry {
	uint32_t     start_offset;
	uint32_t     end_offset;
};

struct cam_tfe_lut_dump_entry {
	enum cam_tfe_lut_word_size  lut_word_size;
	uint32_t                    lut_bank_sel;
	uint32_t                    lut_addr_size;
	uint32_t                    dmi_reg_offset;
};
struct cam_tfe_reg_dump_data {
	uint32_t     num_reg_dump_entries;
	uint32_t     num_lut_dump_entries;
	uint32_t     bus_start_addr;
	uint32_t     bus_write_top_end_addr;
	uint32_t     bus_client_start_addr;
	uint32_t     bus_client_offset;
	uint32_t     num_bus_clients;
	struct cam_tfe_reg_dump_entry
		reg_entry[CAM_TFE_MAX_REG_DUMP_ENTRIES];
	struct cam_tfe_lut_dump_entry
		lut_entry[CAM_TFE_MAX_LUT_DUMP_ENTRIES];
};

struct cam_tfe_top_reg_offset_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t lens_feature;
	uint32_t stats_feature;
	uint32_t zoom_feature;
	uint32_t global_reset_cmd;
	uint32_t core_cgc_ctrl;
	uint32_t ahb_cgc_ctrl;
	uint32_t core_cfg_0;
	uint32_t core_cfg_1;
	uint32_t reg_update_cmd;
	uint32_t diag_config;
	uint32_t diag_sensor_status_0;
	uint32_t diag_sensor_status_1;
	uint32_t diag_sensor_frame_cnt_status;
	uint32_t violation_status;
	uint32_t stats_throttle_cnt_cfg_0;
	uint32_t stats_throttle_cnt_cfg_1;
	uint32_t debug_0;
	uint32_t debug_1;
	uint32_t debug_2;
	uint32_t debug_3;
	uint32_t debug_cfg;
	uint32_t perf_cnt_cfg;
	uint32_t perf_pixel_count;
	uint32_t perf_line_count;
	uint32_t perf_stall_count;
	uint32_t perf_always_count;
	uint32_t perf_count_status;

	/*reg data */
	uint32_t diag_min_hbi_error_shift;
	uint32_t diag_neq_hbi_shift;
	uint32_t diag_sensor_hbi_mask;
};

struct cam_tfe_camif_reg {
	uint32_t     hw_version;
	uint32_t     hw_status;
	uint32_t     module_cfg;
	uint32_t     pdaf_raw_crop_width_cfg;
	uint32_t     pdaf_raw_crop_height_cfg;
	uint32_t     line_skip_pattern;
	uint32_t     pixel_skip_pattern;
	uint32_t     period_cfg;
	uint32_t     irq_subsample_pattern;
	uint32_t     epoch_irq_cfg;
	uint32_t     debug_1;
	uint32_t     debug_0;
	uint32_t     test_bus_ctrl;
	uint32_t     spare;
	uint32_t     reg_update_cmd;
};

struct cam_tfe_camif_reg_data {
	uint32_t     extern_reg_update_mask;
	uint32_t     dual_tfe_pix_en_shift;
	uint32_t     extern_reg_update_shift;
	uint32_t     camif_pd_rdi2_src_sel_shift;
	uint32_t     dual_tfe_sync_sel_shift;
	uint32_t     delay_line_en_shift;

	uint32_t     pixel_pattern_shift;
	uint32_t     pixel_pattern_mask;
	uint32_t     module_enable_shift;
	uint32_t     pix_out_enable_shift;
	uint32_t     pdaf_output_enable_shift;

	uint32_t     dsp_mode_shift;
	uint32_t     dsp_mode_mask;
	uint32_t     dsp_en_shift;
	uint32_t     dsp_en_mask;

	uint32_t     reg_update_cmd_data;
	uint32_t     epoch_line_cfg;
	uint32_t     sof_irq_mask;
	uint32_t     epoch0_irq_mask;
	uint32_t     epoch1_irq_mask;
	uint32_t     eof_irq_mask;
	uint32_t     reg_update_irq_mask;
	uint32_t     error_irq_mask0;
	uint32_t     error_irq_mask2;
	uint32_t     subscribe_irq_mask[CAM_TFE_TOP_IRQ_REG_NUM];

	uint32_t     enable_diagnostic_hw;
	uint32_t     perf_cnt_start_cmd_shift;
	uint32_t     perf_cnt_continuous_shift;
	uint32_t     perf_client_sel_shift;
	uint32_t     perf_window_start_shift;
	uint32_t     perf_window_end_shift;
};

struct cam_tfe_camif_hw_info {
	struct cam_tfe_camif_reg      *camif_reg;
	struct cam_tfe_camif_reg_data *reg_data;
};

struct cam_tfe_rdi_reg {
	uint32_t     rdi_hw_version;
	uint32_t     rdi_hw_status;
	uint32_t     rdi_module_config;
	uint32_t     rdi_skip_period;
	uint32_t     rdi_irq_subsample_pattern;
	uint32_t     rdi_epoch_irq;
	uint32_t     rdi_debug_1;
	uint32_t     rdi_debug_0;
	uint32_t     rdi_test_bus_ctrl;
	uint32_t     rdi_spare;
	uint32_t     reg_update_cmd;
};

struct cam_tfe_rdi_reg_data {
	uint32_t     reg_update_cmd_data;
	uint32_t     epoch_line_cfg;

	uint32_t     pixel_pattern_shift;
	uint32_t     pixel_pattern_mask;
	uint32_t     rdi_out_enable_shift;

	uint32_t     sof_irq_mask;
	uint32_t     epoch0_irq_mask;
	uint32_t     epoch1_irq_mask;
	uint32_t     eof_irq_mask;
	uint32_t     error_irq_mask0;
	uint32_t     error_irq_mask2;
	uint32_t     subscribe_irq_mask[CAM_TFE_TOP_IRQ_REG_NUM];
	uint32_t     enable_diagnostic_hw;
	uint32_t     diag_sensor_sel;
	uint32_t     diag_sensor_shift;
};

struct cam_tfe_clc_hw_status {
	uint8_t     name[CAM_TFE_CLC_NAME_LENGTH_MAX];
	uint32_t    hw_status_reg;
};

struct cam_tfe_rdi_hw_info {
	struct cam_tfe_rdi_reg              *rdi_reg;
	struct cam_tfe_rdi_reg_data         *reg_data;
};

struct cam_tfe_top_hw_info {
	struct cam_tfe_top_reg_offset_common  *common_reg;
	struct cam_tfe_camif_hw_info           camif_hw_info;
	struct cam_tfe_rdi_hw_info             rdi_hw_info[CAM_TFE_RDI_MAX];
	uint32_t in_port[CAM_TFE_TOP_IN_PORT_MAX];
	struct cam_tfe_reg_dump_data           reg_dump_data;
};

struct cam_tfe_hw_info {
	uint32_t    top_irq_mask[CAM_TFE_TOP_IRQ_REG_NUM];
	uint32_t    top_irq_clear[CAM_TFE_TOP_IRQ_REG_NUM];
	uint32_t    top_irq_status[CAM_TFE_TOP_IRQ_REG_NUM];
	uint32_t    top_irq_cmd;
	uint32_t    global_clear_bitmask;

	uint32_t    bus_irq_mask[CAM_TFE_BUS_MAX_IRQ_REGISTERS];
	uint32_t    bus_irq_clear[CAM_TFE_BUS_MAX_IRQ_REGISTERS];
	uint32_t    bus_irq_status[CAM_TFE_BUS_MAX_IRQ_REGISTERS];
	uint32_t    bus_irq_cmd;

	uint32_t    bus_violation_reg;
	uint32_t    bus_overflow_reg;
	uint32_t    bus_image_size_vilation_reg;
	uint32_t    bus_overflow_clear_cmd;
	uint32_t    debug_status_top;

	uint32_t    reset_irq_mask[CAM_TFE_TOP_IRQ_REG_NUM];
	uint32_t    error_irq_mask[CAM_TFE_TOP_IRQ_REG_NUM];
	uint32_t    bus_reg_irq_mask[CAM_TFE_BUS_MAX_IRQ_REGISTERS];
	uint32_t    bus_error_irq_mask[CAM_TFE_BUS_MAX_IRQ_REGISTERS];

	uint32_t    num_clc;
	struct cam_tfe_clc_hw_status  *clc_hw_status_info;

	uint32_t    top_version;
	void       *top_hw_info;

	uint32_t    bus_version;
	void       *bus_hw_info;
};

struct cam_tfe_hw_core_info {
	uint32_t                            core_index;
	struct cam_tfe_hw_info             *tfe_hw_info;
	void                               *top_priv;
	struct cam_tfe_bus                 *tfe_bus;
	void                               *tasklet_info;
	struct cam_tfe_irq_evt_payload  evt_payload[CAM_TFE_EVT_MAX];
	struct list_head                    free_payload_list;
	bool                                irq_err_config;
	uint32_t                            irq_err_config_cnt;
	spinlock_t                          spin_lock;
	struct completion                   reset_complete;
};

int cam_tfe_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_tfe_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_tfe_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int cam_tfe_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size);
int cam_tfe_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_tfe_release(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_tfe_start(void *device_priv,
	void *start_args, uint32_t arg_size);
int cam_tfe_stop(void *device_priv,
	void *stop_args, uint32_t arg_size);
int cam_tfe_read(void *device_priv,
	void *read_args, uint32_t arg_size);
int cam_tfe_write(void *device_priv,
	void *write_args, uint32_t arg_size);
int cam_tfe_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

irqreturn_t cam_tfe_irq(int irq_num, void *data);

int cam_tfe_core_init(struct cam_tfe_hw_core_info *core_info,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	struct cam_tfe_hw_info             *tfe_hw_info);

int cam_tfe_core_deinit(struct cam_tfe_hw_core_info *core_info,
	struct cam_tfe_hw_info             *tfe_hw_info);

#endif /* _CAM_TFE_CORE_H_ */
