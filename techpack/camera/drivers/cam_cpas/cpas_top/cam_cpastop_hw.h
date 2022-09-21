/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CPASTOP_HW_H_
#define _CAM_CPASTOP_HW_H_

#include "cam_cpas_api.h"
#include "cam_cpas_hw.h"

/**
 * enum cam_camnoc_hw_irq_type - Enum for camnoc error types
 *
 * @CAM_CAMNOC_HW_IRQ_SLAVE_ERROR                  : Each slave port in CAMNOC
 *                                                  (3 QSB ports and 1 QHB port)
 *                                                   has an error logger. The
 *                                                   error observed at any slave
 *                                                   port is logged into the
 *                                                   error logger register and
 *                                                   an IRQ is triggered
 * @CAM_CAMNOC_HW_IRQ_IFE_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the IFE UBWC
 *                                                   encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_ENCODE_ERROR  : Triggered if any error
 *                                                   detected in the IFE UBWC-
 *                                                   Stats encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_1_ENCODE_ERROR: Triggered if any error
 *                                                   detected in the IFE UBWC-
 *                                                   Stats 1 encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR      : Triggered if any error
 *                                                   detected in the IFE0 UBWC
 *                                                   encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR      : Triggered if any error
 *                                                   detected in the IFE1 or
 *                                                   IFE3 UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE0_UBWC_ENCODE_ERROR       : Triggered if any UBWC error
 *                                                   is detected in IFE0 write
 *                                                   path
 * @CAM_CAMNOC_HW_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR : Triggered if any UBWC error
 *                                                   is detected in IFE1 write
 *                                                   path slave  times out after
 *                                                   4000 AHB cycles
 * @CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the IPE
 *                                                   UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_BPS_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the BPS
 *                                                   UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE1_BPS_UBWC_DECODE_ERROR   : Triggered if any error
 *                                                   detected in the IPE1/BPS
 *                                                   read path decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR       : Triggered if any error
 *                                                   detected in the IPE0 read
 *                                                   path decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE1_UBWC_DECODE_ERROR       : Triggered if any error
 *                                                   detected in the IPE1 read
 *                                                   path decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR    : Triggered if any error
 *                                                   detected in the IPE/BPS
 *                                                   UBWC decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR    : Triggered if any error
 *                                                   detected in the IPE/BPS
 *                                                   UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_RESERVED1                    : Reserved
 * @CAM_CAMNOC_HW_IRQ_RESERVED2                    : Reserved
 * @CAM_CAMNOC_HW_IRQ_CAMNOC_TEST                  : To test the IRQ logic
 */
enum cam_camnoc_hw_irq_type {
	CAM_CAMNOC_HW_IRQ_SLAVE_ERROR =
		CAM_CAMNOC_IRQ_SLAVE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE_UBWC_STATS_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_1_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE_UBWC_STATS_1_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE02_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE13_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE0_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE0_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IPE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_BPS_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_BPS_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE1_BPS_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE1_BPS_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE0_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE1_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE1_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT =
		CAM_CAMNOC_IRQ_AHB_TIMEOUT,
	CAM_CAMNOC_HW_IRQ_RESERVED1,
	CAM_CAMNOC_HW_IRQ_RESERVED2,
	CAM_CAMNOC_HW_IRQ_CAMNOC_TEST,
};

/**
 * enum cam_camnoc_port_type - Enum for different camnoc hw ports. All CAMNOC
 *         settings like QoS, LUT mappings need to be configured for
 *         each of these ports.
 *
 * @CAM_CAMNOC_CDM: Indicates CDM HW connection to camnoc
 * @CAM_CAMNOC_SFE_RD: Indicates read data from all SFEs to cammnoc
 * @CAM_CAMNOC_IFE02: Indicates IFE0, IFE2 HW connection to camnoc
 * @CAM_CAMNOC_IFE13: Indicates IFE1, IFE3 HW connection to camnoc
 * @CAM_CAMNOC_IFE_LITE: Indicates all IFE lites connection to camnoc
 * @CAM_CAMNOC_IFE_LINEAR: Indicates linear data from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE_LINEAR_STATS: Indicates linear and stats data from certan
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_LINEAR_STATS_1: Indicates linear and stats data from certan
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_PDAF: Indicates pdaf data from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE_UBWC: Indicates ubwc from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE_UBWC_STATS: Indicates ubwc+stats from certain IFEs to cammnoc
 * @CAM_CAMNOC_IFE_UBWC_STATS_1: Indicates ubwc+stats from certain
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_RDI_WR: Indicates RDI write data from certain IFEs to cammnoc
 * @CAM_CAMNOC_IFE_RDI_WR_1: Indicates RDI write data from certain
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_RDI_RD: Indicates RDI read data from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE0123_RDI_WRITE: RDI write only for all IFEx
 * @CAM_CAMNOC_IFE0_NRDI_WRITE: IFE0 non-RDI write
 * @CAM_CAMNOC_IFE01_RDI_READ: IFE0/1 RDI READ
 * @CAM_CAMNOC_IFE1_NRDI_WRITE: IFE1 non-RDI write
 * @CAM_CAMNOC_IPE_BPS_LRME_READ: Indicates IPE, BPS, LRME Read HW
 *         connection to camnoc
 * @CAM_CAMNOC_IPE_BPS_LRME_WRITE: Indicates IPE, BPS, LRME Write HW
 *         connection to camnoc
 * @CAM_CAMNOC_IPE_VID_DISP_WRITE: Indicates IPE's VID/DISP Wrire HW
 *         connection to camnoc
 * @CAM_CAMNOC_IPE_WR: Indicates IPE HW's write connection to camnoc
 * @CAM_CAMNOC_IPE0_RD: Indicates IPE's Read0 HW connection to camnoc
 * @CAM_CAMNOC_IPE1_RD: Indicates IPE's Read1 HW connection to camnoc
 * @CAM_CAMNOC_IPE1_BPS_RD: Indicates IPE's Read1 + BPS Read HW connection
 *         to camnoc
 * @CAM_CAMNOC_IPE_BPS_WR: Indicates IPE+BPS Write HW connection to camnoc
 * @CAM_CAMNOC_BPS_WR: Indicates BPS HW's write connection to camnoc
 * @CAM_CAMNOC_BPS_RD: Indicates BPS HW's read connection to camnoc
 * @CAM_CAMNOC_JPEG: Indicates JPEG HW connection to camnoc
 * @CAM_CAMNOC_FD: Indicates FD HW connection to camnoc
 * @CAM_CAMNOC_ICP: Indicates ICP HW connection to camnoc
 * @CAM_CAMNOC_TFE: Indicates TFE0 HW connection to camnoc
 * @CAM_CAMNOC_TFE_1: Indicates TFE1 HW connection to camnoc
 * @CAM_CAMNOC_TFE_2: Indicates TFE2 HW connection to camnoc
 * @CAM_CAMNOC_OPE: Indicates OPE HW connection to camnoc
 */
enum cam_camnoc_port_type {
	CAM_CAMNOC_CDM,
	CAM_CAMNOC_SFE_RD,
	CAM_CAMNOC_IFE02,
	CAM_CAMNOC_IFE13,
	CAM_CAMNOC_IFE_LITE,
	CAM_CAMNOC_IFE_LINEAR,
	CAM_CAMNOC_IFE_LINEAR_STATS,
	CAM_CAMNOC_IFE_LINEAR_STATS_1,
	CAM_CAMNOC_IFE_PDAF,
	CAM_CAMNOC_IFE_UBWC,
	CAM_CAMNOC_IFE_UBWC_STATS,
	CAM_CAMNOC_IFE_UBWC_STATS_1,
	CAM_CAMNOC_IFE_RDI_WR,
	CAM_CAMNOC_IFE_RDI_WR_1,
	CAM_CAMNOC_IFE_RDI_RD,
	CAM_CAMNOC_IFE0123_RDI_WRITE,
	CAM_CAMNOC_IFE0_NRDI_WRITE,
	CAM_CAMNOC_IFE01_RDI_READ,
	CAM_CAMNOC_IFE1_NRDI_WRITE,
	CAM_CAMNOC_IPE_BPS_LRME_READ,
	CAM_CAMNOC_IPE_BPS_LRME_WRITE,
	CAM_CAMNOC_IPE_VID_DISP_WRITE,
	CAM_CAMNOC_IPE_WR,
	CAM_CAMNOC_IPE0_RD,
	CAM_CAMNOC_IPE1_RD,
	CAM_CAMNOC_IPE1_BPS_RD,
	CAM_CAMNOC_IPE_BPS_WR,
	CAM_CAMNOC_BPS_WR,
	CAM_CAMNOC_BPS_RD,
	CAM_CAMNOC_JPEG,
	CAM_CAMNOC_FD,
	CAM_CAMNOC_ICP,
	CAM_CAMNOC_TFE,
	CAM_CAMNOC_TFE_1,
	CAM_CAMNOC_TFE_2,
	CAM_CAMNOC_OPE,
	CAM_CAMNOC_IFE01234_RDI_WRITE,
	CAM_CAMNOC_IFE01_NRDI_WRITE,
	CAM_CAMNOC_IFE2_NRDI_WRITE,
};

/**
 * struct cam_camnoc_specific : CPAS camnoc specific settings
 *
 * @port_type: Port type
 * @enable: Whether to enable settings for this connection
 * @priority_lut_low: Priority Low LUT mapping for this connection
 * @priority_lut_high: Priority High LUT mapping for this connection
 * @urgency: Urgency (QoS) settings for this connection
 * @danger_lut: Danger LUT mapping for this connection
 * @safe_lut: Safe LUT mapping for this connection
 * @ubwc_ctl: UBWC control settings for this connection
 * @qosgen_mainctl: qosgen shaping control configuration for this connection
 * @qosgen_shaping_low: qosgen shaping low configuration for this connection
 * @qosgen_shaping_high: qosgen shaping high configuration for this connection
 *
 */
struct cam_camnoc_specific {
	enum cam_camnoc_port_type port_type;
	bool enable;
	struct cam_cpas_reg priority_lut_low;
	struct cam_cpas_reg priority_lut_high;
	struct cam_cpas_reg urgency;
	struct cam_cpas_reg danger_lut;
	struct cam_cpas_reg safe_lut;
	struct cam_cpas_reg ubwc_ctl;
	struct cam_cpas_reg flag_out_set0_low;
	struct cam_cpas_reg qosgen_mainctl;
	struct cam_cpas_reg qosgen_shaping_low;
	struct cam_cpas_reg qosgen_shaping_high;
};

/**
 * struct cam_camnoc_irq_sbm : Sideband manager settings for all CAMNOC IRQs
 *
 * @sbm_enable: SBM settings for IRQ enable
 * @sbm_status: SBM settings for IRQ status
 * @sbm_clear: SBM settings for IRQ clear
 *
 */
struct cam_camnoc_irq_sbm {
	struct cam_cpas_reg sbm_enable;
	struct cam_cpas_reg sbm_status;
	struct cam_cpas_reg sbm_clear;
};

/**
 * struct cam_camnoc_irq_err : Error settings specific to each CAMNOC IRQ
 *
 * @irq_type: Type of IRQ
 * @enable: Whether to enable error settings for this IRQ
 * @sbm_port: Corresponding SBM port for this IRQ
 * @err_enable: Error enable settings for this IRQ
 * @err_status: Error status settings for this IRQ
 * @err_clear: Error clear settings for this IRQ
 *
 */
struct cam_camnoc_irq_err {
	enum cam_camnoc_hw_irq_type irq_type;
	bool enable;
	uint32_t sbm_port;
	struct cam_cpas_reg err_enable;
	struct cam_cpas_reg err_status;
	struct cam_cpas_reg err_clear;
};

/**
 * struct cam_cpas_hw_errata_wa : Struct for HW errata workaround info
 *
 * @enable: Whether to enable this errata workround
 * @data: HW Errata workaround data
 *
 */
struct cam_cpas_hw_errata_wa {
	bool enable;
	union {
		struct cam_cpas_reg reg_info;
	} data;
};

/**
 * struct cam_cpas_hw_errata_wa_list : List of HW Errata workaround info
 *
 * @camnoc_flush_slave_pending_trans: Errata workaround info for flushing
 *         camnoc slave pending transactions before turning off CPAS_TOP gdsc
 * @tcsr_camera_hf_sf_ares_glitch: Errata workaround info from ignoring
 *         erroneous signals at camera start
 */
struct cam_cpas_hw_errata_wa_list {
	struct cam_cpas_hw_errata_wa camnoc_flush_slave_pending_trans;
	struct cam_cpas_hw_errata_wa tcsr_camera_hf_sf_ares_glitch;
};

/**
 * struct cam_camnoc_err_logger_info : CAMNOC error logger register offsets
 *
 * @mainctrl: Register offset for mainctrl
 * @errvld: Register offset for errvld
 * @errlog0_low: Register offset for errlog0_low
 * @errlog0_high: Register offset for errlog0_high
 * @errlog1_low: Register offset for errlog1_low
 * @errlog1_high: Register offset for errlog1_high
 * @errlog2_low: Register offset for errlog2_low
 * @errlog2_high: Register offset for errlog2_high
 * @errlog3_low: Register offset for errlog3_low
 * @errlog3_high: Register offset for errlog3_high
 *
 */
struct cam_camnoc_err_logger_info {
	uint32_t mainctrl;
	uint32_t errvld;
	uint32_t errlog0_low;
	uint32_t errlog0_high;
	uint32_t errlog1_low;
	uint32_t errlog1_high;
	uint32_t errlog2_low;
	uint32_t errlog2_high;
	uint32_t errlog3_low;
	uint32_t errlog3_high;
};

/**
 * struct cam_camnoc_fifo_lvl_info : Struct for fifo fill level registers
 * @IFE0_nRDI_maxwr_offset: Register offset for fill level for IFE0
 * @IFE1_nRDI_maxwr_offset: Register offset for fill level for IFE1
 * @IFE0123_RDI_maxwr_low_offset: Register offset for RDI
 * @ife_linear: Register offset for ife linear
 * @ife_rdi_wr: Register offset for rdi wr
 * @ife_ubwc_stats: Register offset for ubwc stats
 * @IFE02_MAXWR_LOW: Register offset for IFE02
 * @IFE13_MAXWR_LOW: Register offset for IFE13
 * @IFE01_MAXWR_LOW: Register offset for IFE01
 * @IFE23_MAXWR_LOW: Register offset for IFE23
 */
struct cam_camnoc_fifo_lvl_info {
	uint32_t IFE0_nRDI_maxwr_offset;
	uint32_t IFE1_nRDI_maxwr_offset;
	uint32_t IFE0123_RDI_maxwr_offset;
	uint32_t ife_linear;
	uint32_t ife_rdi_wr;
	uint32_t ife_ubwc_stats;
	uint32_t IFE02_MAXWR_LOW;
	uint32_t IFE13_MAXWR_LOW;
	uint32_t IFE01_MAXWR_LOW;
	uint32_t IFE23_MAXWR_LOW;
	uint32_t IFE0_MAXWR_LOW;
	uint32_t IFE1_MAXWR_LOW;
};

/**
 * struct cam_camnoc_info : Overall CAMNOC settings info
 *
 * @specific: Pointer to CAMNOC SPECIFICTONTTPTR settings
 * @specific_size: Array size of SPECIFICTONTTPTR settings
 * @irq_sbm: Pointer to CAMNOC IRQ SBM settings
 * @irq_err: Pointer to CAMNOC IRQ Error settings
 * @irq_err_size: Array size of IRQ Error settings
 * @err_logger: Pointer to CAMNOC IRQ Error logger read registers
 * @errata_wa_list: HW Errata workaround info
 * @fill_level_register: Fill level registers
 *
 */
struct cam_camnoc_info {
	struct cam_camnoc_specific *specific;
	int specific_size;
	struct cam_camnoc_irq_sbm *irq_sbm;
	struct cam_camnoc_irq_err *irq_err;
	int irq_err_size;
	struct cam_camnoc_err_logger_info *err_logger;
	struct cam_cpas_hw_errata_wa_list *errata_wa_list;
	struct cam_camnoc_fifo_lvl_info *fill_lvl_register;
};

/**
 * struct cam_cpas_work_payload : Struct for cpas work payload data
 *
 * @hw: Pointer to HW info
 * @irq_status: IRQ status value
 * @irq_data: IRQ data
 * @workq_scheduled_ts: workqueue scheduled timestamp
 * @work: Work handle
 *
 */
struct cam_cpas_work_payload {
	struct cam_hw_info *hw;
	uint32_t irq_status;
	uint32_t irq_data;
	ktime_t workq_scheduled_ts;
	struct work_struct work;
};

/**
 * struct cam_cpas_camnoc_qchannel : Cpas camnoc qchannel info
 *
 * @qchannel_ctrl: offset to configure to control camnoc qchannel interface
 * @qchannel_status: offset to read camnoc qchannel interface status
 *
 */
struct cam_cpas_camnoc_qchannel {
	uint32_t qchannel_ctrl;
	uint32_t qchannel_status;
};

#endif /* _CAM_CPASTOP_HW_H_ */
