/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_COMMON_H_
#define _CAM_IFE_CSID_COMMON_H_

#include "cam_hw.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_ife_csid_soc.h"

#define CAM_IFE_CSID_VER_1_0  0x100
#define CAM_IFE_CSID_VER_2_0  0x200
#define CAM_IFE_CSID_MAX_ERR_COUNT  100

#define CAM_IFE_CSID_HW_CAP_IPP                           0x1
#define CAM_IFE_CSID_HW_CAP_RDI                           0x2
#define CAM_IFE_CSID_HW_CAP_PPP                           0x4
#define CAM_IFE_CSID_HW_CAP_TOP                           0x8

#define CAM_IFE_CSID_TPG_ENCODE_RAW8                      0x1
#define CAM_IFE_CSID_TPG_ENCODE_RAW10                     0x2
#define CAM_IFE_CSID_TPG_ENCODE_RAW12                     0x3
#define CAM_IFE_CSID_TPG_ENCODE_RAW14                     0x4
#define CAM_IFE_CSID_TPG_ENCODE_RAW16                     0x5

#define CAM_IFE_CSID_TPG_TEST_PATTERN_YUV                 0x4

#define CAM_IFE_CSID_HW_IDX_0                             0x1
#define CAM_IFE_CSID_HW_IDX_1                             0x2
#define CAM_IFE_CSID_HW_IDX_2                             0x4

#define CAM_IFE_CSID_LOG_BUF_LEN                          512

#define CAM_IFE_CSID_CAP_INPUT_LCR                        0x1
#define CAM_IFE_CSID_CAP_MIPI8_UNPACK                     0x2
#define CAM_IFE_CSID_CAP_MIPI10_UNPACK                    0x4
#define CAM_IFE_CSID_CAP_MIPI12_UNPACK                    0x8
#define CAM_IFE_CSID_CAP_MIPI14_UNPACK                    0x10
#define CAM_IFE_CSID_CAP_MIPI16_UNPACK                    0x20
#define CAM_IFE_CSID_CAP_MIPI20_UNPACK                    0x40
#define CAM_IFE_CSID_CAP_LINE_SMOOTHING_IN_RDI            0x80
#define CAM_IFE_CSID_CAP_SOF_RETIME_DIS                   0x100

/*
 * Debug values enable the corresponding interrupts and debug logs provide
 * necessary information
 */
#define CAM_IFE_CSID_DEBUG_ENABLE_SOF_IRQ                 BIT(0)
#define CAM_IFE_CSID_DEBUG_ENABLE_EOF_IRQ                 BIT(1)
#define CAM_IFE_CSID_DEBUG_ENABLE_SOT_IRQ                 BIT(2)
#define CAM_IFE_CSID_DEBUG_ENABLE_EOT_IRQ                 BIT(3)
#define CAM_IFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE       BIT(4)
#define CAM_IFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE        BIT(5)
#define CAM_IFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE        BIT(6)
#define CAM_IFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO            BIT(7)
#define CAM_IFE_CSID_DEBUG_DISABLE_EARLY_EOF              BIT(8)
#define CAM_IFE_DEBUG_ENABLE_UNMAPPED_VC_DT_IRQ           BIT(9)

/* Binning supported masks. Binning support changes for specific paths
 * and also for targets. With the mask, we handle the supported features
 * in reg files and handle in code accordingly.
 */

#define CAM_IFE_CSID_BIN_HORIZONTAL                       BIT(0)
#define CAM_IFE_CSID_BIN_QCFA                             BIT(1)
#define CAM_IFE_CSID_BIN_VERTICAL                         BIT(2)

#define CAM_IFE_CSID_WIDTH_FUSE_VAL_MAX			  4

/* enum for multiple mem base in some of the targets */
enum cam_ife_csid_mem_base_id {
	CAM_IFE_CSID_CLC_MEM_BASE_ID,
	CAM_IFE_CSID_TOP_MEM_BASE_ID,
};

/* enum cam_ife_csid_path_multi_vc_dt_grp: for multi vc dt suppot */
enum cam_ife_csid_path_multi_vc_dt_grp {
	CAM_IFE_CSID_MULTI_VC_DT_GRP_0,
	CAM_IFE_CSID_MULTI_VC_DT_GRP_1,
	CAM_IFE_CSID_MULTI_VC_DT_GRP_MAX,
};

/**
 * enum cam_ife_csid_irq_reg - Specify the csid irq reg
 */
enum cam_ife_csid_irq_reg {
	CAM_IFE_CSID_IRQ_REG_TOP,
	CAM_IFE_CSID_IRQ_REG_RX,
	CAM_IFE_CSID_IRQ_REG_RDI_0,
	CAM_IFE_CSID_IRQ_REG_RDI_1,
	CAM_IFE_CSID_IRQ_REG_RDI_2,
	CAM_IFE_CSID_IRQ_REG_RDI_3,
	CAM_IFE_CSID_IRQ_REG_RDI_4,
	CAM_IFE_CSID_IRQ_REG_IPP,
	CAM_IFE_CSID_IRQ_REG_PPP,
	CAM_IFE_CSID_IRQ_REG_UDI_0,
	CAM_IFE_CSID_IRQ_REG_UDI_1,
	CAM_IFE_CSID_IRQ_REG_UDI_2,
	CAM_IFE_CSID_IRQ_REG_MAX,
};

/*
 * struct cam_ife_csid_irq_desc: Structure to hold IRQ description
 *
 * @bitmask    :     Bitmask of the IRQ
 * @err_type   :     Error type for ISP hardware event
 * @irq_desc   :     String to describe the IRQ bit
 * @err_handler:     Error handler which gets invoked if error IRQ bit set
 */
struct cam_ife_csid_irq_desc {
	uint32_t    bitmask;
	uint32_t    err_type;
	uint8_t    *desc;
	void       (*err_handler)(void *csid_hw, void *res);
};

/*
 * struct cam_ife_csid_top_irq_desc: Structure to hold IRQ bitmask and description
 *
 * @bitmask    :        Bitmask of the IRQ
 * @err_type   :        Error type for ISP hardware event
 * @err_name   :        IRQ name
 * @desc       :        String to describe about the IRQ
 * @err_handler:        Error handler which gets invoked if error IRQ bit set
 */
struct cam_ife_csid_top_irq_desc {
	uint32_t    bitmask;
	uint32_t    err_type;
	char       *err_name;
	char       *desc;
	void       (*err_handler)(void *csid_hw);
};

/*
 * struct cam_ife_csid_vc_dt: Structure to hold vc dt combination
 *
 * @vc:              Virtual channel number
 * @dt:              Data type of incoming data
 * @valid:           flag to indicate if combimation is valid
 */

struct cam_ife_csid_vc_dt {
	uint32_t vc;
	uint32_t dt;
	bool valid;
};

/*
 * struct cam_ife_csid_path_format: Structure format info
 *
 * @decode_fmt:       Decode format
 * @packing_fmt:      Packing format
 * @plain_fmt:        Plain format
 * @bits_per_pixel:   Bits per pixel
 */
struct cam_ife_csid_path_format {
	uint32_t decode_fmt;
	uint32_t packing_fmt;
	uint32_t plain_fmt;
	uint32_t bits_per_pxl;
};

/*
 * struct cam_ife_csid_csi2_rx_reg_info: Structure to hold Rx reg offset
 * holds register address offsets
 * shift values
 * masks
 */
struct cam_ife_csid_csi2_rx_reg_info {
	uint32_t irq_status_addr;
	uint32_t irq_mask_addr;
	uint32_t irq_clear_addr;
	uint32_t irq_set_addr;
	uint32_t cfg0_addr;
	uint32_t cfg1_addr;
	uint32_t capture_ctrl_addr;
	uint32_t rst_strobes_addr;
	uint32_t de_scramble_cfg0_addr;
	uint32_t de_scramble_cfg1_addr;
	uint32_t cap_unmap_long_pkt_hdr_0_addr;
	uint32_t cap_unmap_long_pkt_hdr_1_addr;
	uint32_t captured_short_pkt_0_addr;
	uint32_t captured_short_pkt_1_addr;
	uint32_t captured_long_pkt_0_addr;
	uint32_t captured_long_pkt_1_addr;
	uint32_t captured_long_pkt_ftr_addr;
	uint32_t captured_cphy_pkt_hdr_addr;
	uint32_t lane0_misr_addr;
	uint32_t lane1_misr_addr;
	uint32_t lane2_misr_addr;
	uint32_t lane3_misr_addr;
	uint32_t total_pkts_rcvd_addr;
	uint32_t stats_ecc_addr;
	uint32_t total_crc_err_addr;
	uint32_t de_scramble_type3_cfg0_addr;
	uint32_t de_scramble_type3_cfg1_addr;
	uint32_t de_scramble_type2_cfg0_addr;
	uint32_t de_scramble_type2_cfg1_addr;
	uint32_t de_scramble_type1_cfg0_addr;
	uint32_t de_scramble_type1_cfg1_addr;
	uint32_t de_scramble_type0_cfg0_addr;
	uint32_t de_scramble_type0_cfg1_addr;

	/*configurations */
	uint32_t rst_srb_all;
	uint32_t rst_done_shift_val;
	uint32_t irq_mask_all;
	uint32_t misr_enable_shift_val;
	uint32_t vc_mode_shift_val;
	uint32_t capture_long_pkt_en_shift;
	uint32_t capture_short_pkt_en_shift;
	uint32_t capture_cphy_pkt_en_shift;
	uint32_t capture_long_pkt_dt_shift;
	uint32_t capture_long_pkt_vc_shift;
	uint32_t capture_short_pkt_vc_shift;
	uint32_t capture_cphy_pkt_dt_shift;
	uint32_t capture_cphy_pkt_vc_shift;
	uint32_t ecc_correction_shift_en;
	uint32_t phy_bist_shift_en;
	uint32_t epd_mode_shift_en;
	uint32_t eotp_shift_en;
	uint32_t dyn_sensor_switch_shift_en;
	uint32_t phy_num_mask;
	uint32_t vc_mask;
	uint32_t wc_mask;
	uint32_t dt_mask;
	uint32_t calc_crc_mask;
	uint32_t expected_crc_mask;
	uint32_t lane_num_shift;
	uint32_t lane_cfg_shift;
	uint32_t phy_type_shift;
	uint32_t phy_num_shift;
	uint32_t tpg_mux_en_shift;
	uint32_t tpg_num_sel_shift;
	uint32_t fatal_err_mask;
	uint32_t part_fatal_err_mask;
	uint32_t non_fatal_err_mask;
	uint32_t debug_irq_mask;
	uint32_t top_irq_mask;
};

/*
 * struct cam_ife_csid_timestamp: place holder for csid core info
 *
 * @prev_boot_timestamp:      Previous frame boot timestamp
 * @prev_sof_timestamp:       Previous frame SOF timetamp
 */
struct cam_ife_csid_timestamp {
	uint64_t                prev_boot_ts;
	uint64_t                prev_sof_ts;
};

/*
 * struct cam_ife_csid_core_info: place holder for csid core info
 *
 * @csid_reg:             Pointer to csid reg info
 * @sw_version:           sw version based on targets
 */
struct cam_ife_csid_core_info {
	void                     *csid_reg;
	uint32_t                  sw_version;
};

/*
 * struct cam_ife_csid_hw_counters: place holder for csid counters
 *
 * @csi2_reserve_cnt:       Reserve count for csi2
 * @irq_debug_cnt:          irq debug counter
 * @error_irq_count:        error irq counter
 */
struct cam_ife_csid_hw_counters {
	uint32_t                          csi2_reserve_cnt;
	uint32_t                          irq_debug_cnt;
	uint32_t                          error_irq_count;
};

/*
 * struct cam_ife_csid_debug_info: place holder for csid debug
 *
 * @debug_val:          Debug val for enabled features
 * @rx_mask:            Debug mask for rx irq
 * @path_mask:          Debug mask for path irq
 */
struct cam_ife_csid_debug_info {
	uint32_t                          debug_val;
	uint32_t                          rx_mask;
	uint32_t                          path_mask;
};

/*
 * struct cam_ife_csid_hw_flags: place holder for flags
 *
 * @device_enabled:         flag to indicate if device enabled
 * @binning_enabled:        flag to indicate if binning enabled
 * @sof_irq_triggered:      flag to indicate if sof irq triggered
 * @fatal_err_detected:     flag to indicate if fatal err detected
 * @rx_enabled:             flag to indicate if rx is enabled
 * @tpg_configured:         flag to indicate if internal_tpg is configured
 * @reset_awaited:          flag to indicate if reset is awaited
 * @offline_mode:           flag to indicate if csid in offline mode
 * @rdi_lcr_en:             flag to indicate if RDI to lcr is enabled
 * @sfe_en:                 flag to indicate if SFE is enabled
 */
struct cam_ife_csid_hw_flags {
	bool                  device_enabled;
	bool                  binning_enabled;
	bool                  sof_irq_triggered;
	bool                  process_reset;
	bool                  fatal_err_detected;
	bool                  rx_enabled;
	bool                  tpg_enabled;
	bool                  tpg_configured;
	bool                  reset_awaited;
	bool                  offline_mode;
	bool                  rdi_lcr_en;
	bool                  sfe_en;
};

/*
 * struct am_ife_csid_cid_data: place holder for cid data
 *
 * @vc_dt:        vc_dt structure
 * @cid_cnt:      count of cid acquired
 * @num_vc_dt:    num of vc dt combinaton for this cid in multi vcdt case
 */
struct cam_ife_csid_cid_data {
	struct cam_ife_csid_vc_dt vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_MAX];
	uint32_t cid_cnt;
	uint32_t num_vc_dt;
};

/*
 * struct cam_ife_csid_rx_cfg: place holder for rx cfg
 *
 * @phy_sel:                  Selected phy
 * @lane_type:                type of lane selected
 * @lane_num:                 number of lanes
 * @lane_cfg:                 lane configuration
 * @tpg_mux_sel:              TPG mux sel
 * @tpg_num_sel:              TPG num sel
 * @mup:                      Mode Update bit. 0 for odd vc, 1 for even VC
 * @epd_supported:            Flag to check if epd supported
 * @irq_handle:               IRQ Handle
 * @err_irq_handle:           Error IRQ Handle
 * @dynamic_sensor_switch_en: Flag if dynamic sensor switch is enabled
 */
struct cam_ife_csid_rx_cfg  {
	uint32_t                        phy_sel;
	uint32_t                        lane_type;
	uint32_t                        lane_num;
	uint32_t                        lane_cfg;
	uint32_t                        tpg_mux_sel;
	uint32_t                        tpg_num_sel;
	uint32_t                        mup;
	uint32_t                        epd_supported;
	uint32_t                        irq_handle;
	uint32_t                        err_irq_handle;
	bool                            dynamic_sensor_switch_en;
};

int cam_ife_csid_is_pix_res_format_supported(
	uint32_t in_format);

int cam_ife_csid_get_format_rdi(
	uint32_t in_format, uint32_t out_format,
	struct cam_ife_csid_path_format *path_format, bool mipi_pack_supported,
	bool mipi_unpacked);

int cam_ife_csid_get_format_ipp_ppp(
	uint32_t in_format,
	struct cam_ife_csid_path_format *path_format);

int cam_ife_csid_hw_probe_init(struct cam_hw_intf  *hw_intf,
	struct cam_ife_csid_core_info *core_info, bool is_custom);

int cam_ife_csid_hw_deinit(struct cam_hw_intf *hw_intf,
	struct cam_ife_csid_core_info *core_info);

int cam_ife_csid_cid_reserve(struct cam_ife_csid_cid_data *cid_data,
	uint32_t *cid_value,
	uint32_t hw_idx,
	struct cam_csid_hw_reserve_resource_args  *reserve);

int cam_ife_csid_cid_release(
	struct cam_ife_csid_cid_data *cid_data,
	uint32_t hw_idx,
	uint32_t cid);

int cam_ife_csid_check_in_port_args(
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t hw_idx);

int cam_ife_csid_is_vc_full_width(struct cam_ife_csid_cid_data *cid_data);

int cam_ife_csid_get_rt_irq_idx(
	uint32_t irq_reg, uint32_t num_ipp,
	uint32_t num_ppp, uint32_t num_rdi);

int cam_ife_csid_convert_res_to_irq_reg(uint32_t res_id);

int cam_ife_csid_get_base(struct cam_hw_soc_info *soc_info,
	uint32_t base_id, void *cmd_args, size_t arg_size);

const char *cam_ife_csid_reset_type_to_string(enum cam_ife_csid_reset_type reset_type);

const uint8_t **cam_ife_csid_get_irq_reg_tag_ptr(void);
#endif /*_CAM_IFE_CSID_COMMON_H_ */
