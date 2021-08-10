/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TFE_CSID_HW_H_
#define _CAM_TFE_CSID_HW_H_

#include "cam_hw.h"
#include "cam_tfe_csid_hw_intf.h"
#include "cam_tfe_csid_soc.h"
#include "cam_csid_ppi_core.h"

#define CAM_TFE_CSID_CID_MAX                          4

/* Each word is taken as uint32_t, for dumping uint64_t count as 2 words
 * 1. soc_index
 * 2. clk_rate --> uint64_t -> 2 words
 */
#define CAM_TFE_CSID_DUMP_MISC_NUM_WORDS              3

#define TFE_CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED    BIT(0)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED    BIT(1)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED    BIT(2)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED    BIT(3)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED    BIT(4)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED    BIT(5)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED    BIT(6)
#define TFE_CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED    BIT(7)
#define TFE_CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED       BIT(8)
#define TFE_CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED      BIT(9)
#define TFE_CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED   BIT(10)
#define TFE_CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION     BIT(11)
#define TFE_CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION     BIT(12)
#define TFE_CSID_CSI2_RX_ERROR_CPHY_PH_CRC            BIT(13)
#define TFE_CSID_CSI2_RX_WARNING_ECC                  BIT(14)
#define TFE_CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW    BIT(15)
#define TFE_CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW    BIT(16)
#define TFE_CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW    BIT(17)
#define TFE_CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW    BIT(18)
#define TFE_CSID_CSI2_RX_ERROR_CRC                    BIT(19)
#define TFE_CSID_CSI2_RX_ERROR_ECC                    BIT(20)
#define TFE_CSID_CSI2_RX_ERROR_MMAPPED_VC_DT          BIT(21)
#define TFE_CSID_CSI2_RX_ERROR_UNMAPPED_VC_DT         BIT(22)
#define TFE_CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW       BIT(23)
#define TFE_CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME        BIT(24)
#define TFE_CSID_CSI2_RX_INFO_RST_DONE                BIT(27)

#define TFE_CSID_PATH_INFO_RST_DONE                   BIT(1)
#define TFE_CSID_PATH_ERROR_FIFO_OVERFLOW             BIT(2)
#define TFE_CSID_PATH_INFO_INPUT_EOF                  BIT(9)
#define TFE_CSID_PATH_INFO_INPUT_EOL                  BIT(10)
#define TFE_CSID_PATH_INFO_INPUT_SOL                  BIT(11)
#define TFE_CSID_PATH_INFO_INPUT_SOF                  BIT(12)
#define TFE_CSID_PATH_IPP_ERROR_CCIF_VIOLATION        BIT(15)
#define TFE_CSID_PATH_IPP_OVERFLOW_IRQ                BIT(16)
#define TFE_CSID_PATH_IPP_FRAME_DROP                  BIT(17)
#define TFE_CSID_PATH_RDI_FRAME_DROP                  BIT(16)
#define TFE_CSID_PATH_RDI_OVERFLOW_IRQ                BIT(17)
#define TFE_CSID_PATH_RDI_ERROR_CCIF_VIOLATION        BIT(18)

/*
 * Debug values enable the corresponding interrupts and debug logs provide
 * necessary information
 */
#define TFE_CSID_DEBUG_ENABLE_SOF_IRQ                 BIT(0)
#define TFE_CSID_DEBUG_ENABLE_EOF_IRQ                 BIT(1)
#define TFE_CSID_DEBUG_ENABLE_SOT_IRQ                 BIT(2)
#define TFE_CSID_DEBUG_ENABLE_EOT_IRQ                 BIT(3)
#define TFE_CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE       BIT(4)
#define TFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE        BIT(5)
#define TFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE        BIT(6)
#define TFE_CSID_DEBUG_ENABLE_HBI_VBI_INFO            BIT(7)
#define TFE_CSID_DEBUG_DISABLE_EARLY_EOF              BIT(8)
#define TFE_CSID_DEBUG_ENABLE_RST_IRQ_LOG             BIT(9)

#define CAM_CSID_EVT_PAYLOAD_MAX                  10

/* enum cam_csid_path_halt_mode select the path halt mode control */
enum cam_tfe_csid_path_halt_mode {
	TFE_CSID_HALT_MODE_INTERNAL,
	TFE_CSID_HALT_MODE_GLOBAL,
	TFE_CSID_HALT_MODE_MASTER,
	TFE_CSID_HALT_MODE_SLAVE,
};

/**
 *enum cam_csid_path_timestamp_stb_sel - select the sof/eof strobes used to
 *        capture the timestamp
 */
enum cam_tfe_csid_path_timestamp_stb_sel {
	TFE_CSID_TIMESTAMP_STB_PRE_HALT,
	TFE_CSID_TIMESTAMP_STB_POST_HALT,
	TFE_CSID_TIMESTAMP_STB_POST_IRQ,
	TFE_CSID_TIMESTAMP_STB_MAX,
};

struct cam_tfe_csid_pxl_reg_offset {
	/* Pxl path register offsets*/
	uint32_t csid_pxl_irq_status_addr;
	uint32_t csid_pxl_irq_mask_addr;
	uint32_t csid_pxl_irq_clear_addr;
	uint32_t csid_pxl_irq_set_addr;

	uint32_t csid_pxl_cfg0_addr;
	uint32_t csid_pxl_cfg1_addr;
	uint32_t csid_pxl_ctrl_addr;
	uint32_t csid_pxl_hcrop_addr;
	uint32_t csid_pxl_vcrop_addr;
	uint32_t csid_pxl_rst_strobes_addr;
	uint32_t csid_pxl_status_addr;
	uint32_t csid_pxl_misr_val_addr;
	uint32_t csid_pxl_timestamp_curr0_sof_addr;
	uint32_t csid_pxl_timestamp_curr1_sof_addr;
	uint32_t csid_pxl_timestamp_perv0_sof_addr;
	uint32_t csid_pxl_timestamp_perv1_sof_addr;
	uint32_t csid_pxl_timestamp_curr0_eof_addr;
	uint32_t csid_pxl_timestamp_curr1_eof_addr;
	uint32_t csid_pxl_timestamp_perv0_eof_addr;
	uint32_t csid_pxl_timestamp_perv1_eof_addr;
	uint32_t csid_pxl_err_recovery_cfg0_addr;
	uint32_t csid_pxl_err_recovery_cfg1_addr;
	uint32_t csid_pxl_err_recovery_cfg2_addr;

	/* configuration */
	uint32_t pix_store_en_shift_val;
	uint32_t early_eof_en_shift_val;
	uint32_t halt_master_sel_shift;
	uint32_t halt_mode_shift;
	uint32_t halt_master_sel_master_val;
	uint32_t halt_master_sel_slave_val;
};

struct cam_tfe_csid_rdi_reg_offset {
	uint32_t csid_rdi_irq_status_addr;
	uint32_t csid_rdi_irq_mask_addr;
	uint32_t csid_rdi_irq_clear_addr;
	uint32_t csid_rdi_irq_set_addr;

	/*RDI N register address */
	uint32_t csid_rdi_cfg0_addr;
	uint32_t csid_rdi_cfg1_addr;
	uint32_t csid_rdi_ctrl_addr;
	uint32_t csid_rdi_rst_strobes_addr;
	uint32_t csid_rdi_status_addr;
	uint32_t csid_rdi_misr_val0_addr;
	uint32_t csid_rdi_misr_val1_addr;
	uint32_t csid_rdi_timestamp_curr0_sof_addr;
	uint32_t csid_rdi_timestamp_curr1_sof_addr;
	uint32_t csid_rdi_timestamp_prev0_sof_addr;
	uint32_t csid_rdi_timestamp_prev1_sof_addr;
	uint32_t csid_rdi_timestamp_curr0_eof_addr;
	uint32_t csid_rdi_timestamp_curr1_eof_addr;
	uint32_t csid_rdi_timestamp_prev0_eof_addr;
	uint32_t csid_rdi_timestamp_prev1_eof_addr;
	uint32_t csid_rdi_err_recovery_cfg0_addr;
	uint32_t csid_rdi_err_recovery_cfg1_addr;
	uint32_t csid_rdi_err_recovery_cfg2_addr;
	uint32_t csid_rdi_byte_cntr_ping_addr;
	uint32_t csid_rdi_byte_cntr_pong_addr;

	/* configuration */
	uint32_t packing_format;
};

struct cam_tfe_csid_csi2_rx_reg_offset {
	uint32_t csid_csi2_rx_irq_status_addr;
	uint32_t csid_csi2_rx_irq_mask_addr;
	uint32_t csid_csi2_rx_irq_clear_addr;
	uint32_t csid_csi2_rx_irq_set_addr;
	uint32_t csid_csi2_rx_cfg0_addr;
	uint32_t csid_csi2_rx_cfg1_addr;
	uint32_t csid_csi2_rx_capture_ctrl_addr;
	uint32_t csid_csi2_rx_rst_strobes_addr;
	uint32_t csid_csi2_rx_cap_unmap_long_pkt_hdr_0_addr;
	uint32_t csid_csi2_rx_cap_unmap_long_pkt_hdr_1_addr;
	uint32_t csid_csi2_rx_captured_short_pkt_0_addr;
	uint32_t csid_csi2_rx_captured_short_pkt_1_addr;
	uint32_t csid_csi2_rx_captured_long_pkt_0_addr;
	uint32_t csid_csi2_rx_captured_long_pkt_1_addr;
	uint32_t csid_csi2_rx_captured_long_pkt_ftr_addr;
	uint32_t csid_csi2_rx_captured_cphy_pkt_hdr_addr;
	uint32_t csid_csi2_rx_total_pkts_rcvd_addr;
	uint32_t csid_csi2_rx_stats_ecc_addr; //no
	uint32_t csid_csi2_rx_total_crc_err_addr;

	/*configurations */
	uint32_t csi2_rst_srb_all;
	uint32_t csi2_rst_done_shift_val;
	uint32_t csi2_irq_mask_all;
	uint32_t csi2_misr_enable_shift_val;
	uint32_t csi2_vc_mode_shift_val;
	uint32_t csi2_capture_long_pkt_en_shift;
	uint32_t csi2_capture_short_pkt_en_shift;
	uint32_t csi2_capture_cphy_pkt_en_shift;
	uint32_t csi2_capture_long_pkt_dt_shift;
	uint32_t csi2_capture_long_pkt_vc_shift;
	uint32_t csi2_capture_short_pkt_vc_shift;
	uint32_t csi2_capture_cphy_pkt_dt_shift;
	uint32_t csi2_capture_cphy_pkt_vc_shift;
	uint32_t csi2_rx_phy_num_mask;
	uint32_t csi2_rx_long_pkt_hdr_rst_stb_shift;
	uint32_t csi2_rx_short_pkt_hdr_rst_stb_shift;
	uint32_t csi2_rx_cphy_pkt_hdr_rst_stb_shift;
};

struct cam_tfe_csid_common_reg_offset {
	/* MIPI CSID registers */
	uint32_t csid_hw_version_addr;
	uint32_t csid_cfg0_addr;
	uint32_t csid_ctrl_addr;
	uint32_t csid_rst_strobes_addr;

	uint32_t csid_test_bus_ctrl_addr;
	uint32_t csid_top_irq_status_addr;
	uint32_t csid_top_irq_mask_addr;
	uint32_t csid_top_irq_clear_addr;
	uint32_t csid_top_irq_set_addr;
	uint32_t csid_irq_cmd_addr;

	/*configurations */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t num_rdis;
	uint32_t num_pix;
	uint32_t csid_reg_rst_stb;
	uint32_t csid_rst_stb;
	uint32_t csid_rst_stb_sw_all;
	uint32_t ipp_path_rst_stb_all;
	uint32_t rdi_path_rst_stb_all;
	uint32_t path_rst_done_shift_val;
	uint32_t path_en_shift_val;
	uint32_t dt_id_shift_val;
	uint32_t vc_shift_val;
	uint32_t dt_shift_val;
	uint32_t fmt_shift_val;
	uint32_t plain_fmt_shit_val;
	uint32_t crop_v_en_shift_val;
	uint32_t crop_h_en_shift_val;
	uint32_t crop_shift;
	uint32_t ipp_irq_mask_all;
	uint32_t rdi_irq_mask_all;
	uint32_t top_tfe2_pix_pipe_fuse_reg;
	uint32_t top_tfe2_fuse_reg;
};

/**
 * struct cam_tfe_csid_reg_offset- CSID instance register info
 *
 * @cmn_reg:  csid common registers info
 * @ipp_reg:  ipp register offset information
 * @ppp_reg:  ppp register offset information
 * @rdi_reg:  rdi register offset information
 *
 */
struct cam_tfe_csid_reg_offset {
	const struct cam_tfe_csid_common_reg_offset   *cmn_reg;
	const struct cam_tfe_csid_csi2_rx_reg_offset  *csi2_reg;
	const struct cam_tfe_csid_pxl_reg_offset      *ipp_reg;
	const struct cam_tfe_csid_rdi_reg_offset *rdi_reg[CAM_TFE_CSID_RDI_MAX];
};

/**
 * struct cam_tfe_csid_hw_info- CSID HW info
 *
 * @csid_reg:        csid register offsets
 * @hw_dts_version:  HW DTS version
 * @csid_max_clk:    maximim csid clock
 *
 */
struct cam_tfe_csid_hw_info {
	const struct cam_tfe_csid_reg_offset   *csid_reg;
	uint32_t                                hw_dts_version;
	uint32_t                                csid_max_clk;
};

/**
 * struct cam_tfe_csid_csi2_rx_cfg- csid csi2 rx configuration data
 * @phy_sel:     input resource type for sensor only
 * @lane_type:   lane type: c-phy or d-phy
 * @lane_num :   active lane number
 * @lane_cfg:    lane configurations: 4 bits per lane
 *
 */
struct cam_tfe_csid_csi2_rx_cfg  {
	uint32_t                        phy_sel;
	uint32_t                        lane_type;
	uint32_t                        lane_num;
	uint32_t                        lane_cfg;
};

/**
 * struct cam_tfe_csid_cid_data- cid configuration private data
 *
 * @vc:          Virtual channel
 * @dt:          Data type
 * @cnt:         Cid resource reference count.
 *
 */
struct cam_tfe_csid_cid_data {
	uint32_t                     vc;
	uint32_t                     dt;
	uint32_t                     cnt;
};

/**
 * struct cam_tfe_csid_path_cfg- csid path configuration details. It is stored
 *                          as private data for IPP/ RDI paths
 * @vc :            Virtual channel number
 * @dt :            Data type number
 * @cid             cid number, it is same as DT_ID number in HW
 * @in_format:      input decode format
 * @out_format:     output format
 * @crop_enable:    crop is enable or disabled, if enabled
 *                  then remaining parameters are valid.
 * @start_pixel:    start pixel
 * @end_pixel:      end_pixel
 * @width:          width
 * @start_line:     start line
 * @end_line:       end_line
 * @height:         heigth
 * @sync_mode:      Applicable for IPP/RDI path reservation
 *                  Reserving the path for master IPP or slave IPP
 *                  master (set value 1), Slave ( set value 2)
 *                  for RDI, set  mode to none
 * @master_idx:     For Slave reservation, Give master TFE instance Index.
 *                  Slave will synchronize with master Start and stop operations
 * @clk_rate        Clock rate
 * @sensor_width    Sensor width in pixel
 * @sensor_height   Sensor height in pixel
 * @sensor_fps      Sensor fps
 * @sensor_hbi      Sensor horizontal blanking interval
 * @sensor_vbi      Sensor vertical blanking interval
 *
 */
struct cam_tfe_csid_path_cfg {
	uint32_t                        vc;
	uint32_t                        dt;
	uint32_t                        cid;
	uint32_t                        in_format;
	uint32_t                        out_format;
	bool                            crop_enable;
	uint32_t                        start_pixel;
	uint32_t                        end_pixel;
	uint32_t                        width;
	uint32_t                        start_line;
	uint32_t                        end_line;
	uint32_t                        height;
	enum cam_isp_hw_sync_mode       sync_mode;
	uint32_t                        master_idx;
	uint64_t                        clk_rate;
	uint32_t                        sensor_width;
	uint32_t                        sensor_height;
	uint32_t                        sensor_fps;
	uint32_t                        sensor_hbi;
	uint32_t                        sensor_vbi;
};

/**
 * struct cam_csid_evt_payload- payload for csid hw event
 * @list       : list head
 * @evt_type   : Event type from CSID
 * @irq_status : IRQ Status register
 * @hw_idx     : Hw index
 * @priv       : Private data of payload
 */
struct cam_csid_evt_payload {
	struct list_head   list;
	uint32_t           evt_type;
	uint32_t           irq_status[TFE_CSID_IRQ_REG_MAX];
	uint32_t           hw_idx;
	void              *priv;
};

/**
 * struct cam_tfe_csid_hw- csid hw device resources data
 *
 * @hw_intf:                  contain the csid hw interface information
 * @hw_info:                  csid hw device information
 * @csid_info:                csid hw specific information
 * @tasklet:                  tasklet to handle csid errors
 * @free_payload_list:        list head for payload
 * @evt_payload:              Event payload to be passed to tasklet
 * @in_res_id:                csid in resource type
 * @csi2_rx_cfg:              csi2 rx decoder configuration for csid
 * @csi2_rx_reserve_cnt:      csi2 reservations count value
 * @ipp_res:                  image pixel path resource
 * @rdi_res:                  raw dump image path resources
 * @cid_res:                  cid resources values
 * @csid_top_reset_complete:  csid top reset completion
 * @csid_csi2_reset_complete: csi2 reset completion
 * @csid_ipp_reset_complete:  ipp reset completion
 * @csid_ppp_complete:        ppp reset completion
 * @csid_rdin_reset_complete: rdi n completion
 * @csid_debug:               csid debug information to enable the SOT, EOT,
 *                            SOF, EOF, measure etc in the csid hw
 * @clk_rate                  Clock rate
 * @sof_irq_triggered:        Flag is set on receiving event to enable sof irq
 *                            incase of SOF freeze.
 * @irq_debug_cnt:            Counter to track sof irq's when above flag is set.
 * @error_irq_count           Error IRQ count, if continuous error irq comes
 *                            need to stop the CSID and mask interrupts.
 * @device_enabled            Device enabled will set once CSID powered on and
 *                            initial configuration are done.
 * @lock_state                csid spin lock
 * @fatal_err_detected        flag to indicate fatal errror is reported
 * @event_cb:                 Callback function to hw mgr in case of hw events
 * @event_cb_priv:            Context data
 * @ppi_hw_intf               interface to ppi hardware
 * @ppi_enabled               flag to specify if the hardware has ppi bridge
 *                            or not
 * @prev_boot_timestamp       previous frame bootime stamp
 * @prev_qtimer_ts            previous frame qtimer csid timestamp
 *
 */
struct cam_tfe_csid_hw {
	struct cam_hw_intf                 *hw_intf;
	struct cam_hw_info                 *hw_info;
	struct cam_tfe_csid_hw_info        *csid_info;
	void                               *tasklet;
	struct list_head                    free_payload_list;
	struct cam_csid_evt_payload   evt_payload[CAM_CSID_EVT_PAYLOAD_MAX];
	uint32_t                            in_res_id;
	struct cam_tfe_csid_csi2_rx_cfg     csi2_rx_cfg;
	uint32_t                            csi2_reserve_cnt;
	uint32_t                            pxl_pipe_enable;
	struct cam_isp_resource_node        ipp_res;
	struct cam_isp_resource_node        rdi_res[CAM_TFE_CSID_RDI_MAX];
	struct cam_tfe_csid_cid_data        cid_res[CAM_TFE_CSID_CID_MAX];
	struct completion                   csid_top_complete;
	struct completion                   csid_csi2_complete;
	struct completion                   csid_ipp_complete;
	struct completion     csid_rdin_complete[CAM_TFE_CSID_RDI_MAX];
	uint64_t                            csid_debug;
	uint64_t                            clk_rate;
	bool                                sof_irq_triggered;
	uint32_t                            irq_debug_cnt;
	uint32_t                            error_irq_count;
	uint32_t                            device_enabled;
	spinlock_t                          spin_lock;
	bool                                fatal_err_detected;
	cam_hw_mgr_event_cb_func            event_cb;
	void                               *event_cb_priv;
	struct cam_hw_intf                 *ppi_hw_intf[CAM_CSID_PPI_HW_MAX];
	bool                                ppi_enable;
	uint64_t                            prev_boot_timestamp;
	uint64_t                            prev_qtimer_ts;
};

int cam_tfe_csid_hw_probe_init(struct cam_hw_intf  *csid_hw_intf,
	uint32_t csid_idx);

int cam_tfe_csid_hw_deinit(struct cam_tfe_csid_hw *tfe_csid_hw);

#endif /* _CAM_TFE_CSID_HW_H_ */
