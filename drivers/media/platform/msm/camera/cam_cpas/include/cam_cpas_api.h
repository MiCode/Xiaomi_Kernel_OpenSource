/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_CPAS_API_H_
#define _CAM_CPAS_API_H_

#include <linux/device.h>
#include <linux/platform_device.h>

#include <media/cam_cpas.h>
#include "cam_soc_util.h"

#define CAM_HW_IDENTIFIER_LENGTH 128

/* Default AXI Bandwidth vote */
#define CAM_CPAS_DEFAULT_AXI_BW 1024

/**
 * enum cam_cpas_reg_base - Enum for register base identifier. These
 *                          are the identifiers used in generic register
 *                          write/read APIs provided by cpas driver.
 */
enum cam_cpas_reg_base {
	CAM_CPAS_REG_CPASTOP,
	CAM_CPAS_REG_CAMNOC,
	CAM_CPAS_REG_CSR_TCSR,
	CAM_CPAS_REG_CAMSS,
	CAM_CPAS_REG_MAX
};

/**
 * enum cam_cpas_hw_version - Enum for Titan CPAS HW Versions
 */
enum cam_cpas_hw_version {
	CAM_CPAS_TITAN_NONE = 0,
	CAM_CPAS_TITAN_170_V100 = 0x170100,
	CAM_CPAS_TITAN_170_V110 = 0x170110,
	CAM_CPAS_TITAN_170_V120 = 0x170120,
	CAM_CPAS_TITAN_175_V100 = 0x175100,
	CAM_CPAS_TITAN_175_V101 = 0x175101,
	CAM_CPAS_TITAN_MAX
};


/**
 * enum cam_camnoc_irq_type - Enum for camnoc irq types
 *
 * @CAM_CAMNOC_IRQ_SLAVE_ERROR: Each slave port in CAMNOC (3 QSB ports and
 *                              1 QHB port) has an error logger. The error
 *                              observed at any slave port is logged into
 *                              the error logger register and an IRQ is
 *                              triggered
 * @CAM_CAMNOC_IRQ_IFE02_UBWC_ENCODE_ERROR  : Triggered if any error detected
 *                                            in the IFE0 UBWC encoder instance
 * @CAM_CAMNOC_IRQ_IFE13_UBWC_ENCODE_ERROR  : Triggered if any error detected
 *                                            in the IFE1 or IFE3 UBWC encoder
 *                                            instance
 * @CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR: Triggered if any error detected
 *                                            in the IPE/BPS UBWC decoder
 *                                            instance
 * @CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR: Triggered if any error detected
 *                                            in the IPE/BPS UBWC encoder
 *                                            instance
 * @CAM_CAMNOC_IRQ_AHB_TIMEOUT              : Triggered when the QHS_ICP slave
 *                                            times out after 4000 AHB cycles
 */
enum cam_camnoc_irq_type {
	CAM_CAMNOC_IRQ_SLAVE_ERROR,
	CAM_CAMNOC_IRQ_IFE02_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_IRQ_IFE13_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR,
	CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_IRQ_AHB_TIMEOUT,
};

/**
 * struct cam_camnoc_irq_slave_err_data : Data for Slave error.
 *
 * @mainctrl     : Err logger mainctrl info
 * @errvld       : Err logger errvld info
 * @errlog0_low  : Err logger errlog0_low info
 * @errlog0_high : Err logger errlog0_high info
 * @errlog1_low  : Err logger errlog1_low info
 * @errlog1_high : Err logger errlog1_high info
 * @errlog2_low  : Err logger errlog2_low info
 * @errlog2_high : Err logger errlog2_high info
 * @errlog3_low  : Err logger errlog3_low info
 * @errlog3_high : Err logger errlog3_high info
 *
 */
struct cam_camnoc_irq_slave_err_data {
	union {
		struct {
			uint32_t stall_en : 1; /* bit 0 */
			uint32_t fault_en : 1; /* bit 1 */
			uint32_t rsv      : 30; /* bits 2-31 */
		};
		uint32_t value;
	} mainctrl;
	union {
		struct {
			uint32_t err_vld : 1; /* bit 0 */
			uint32_t rsv     : 31; /* bits 1-31 */
		};
		uint32_t value;
	} errvld;
	union {
		struct {
			uint32_t loginfo_vld : 1; /* bit 0 */
			uint32_t word_error  : 1; /* bit 1 */
			uint32_t non_secure  : 1; /* bit 2 */
			uint32_t device      : 1; /* bit 3 */
			uint32_t opc         : 3; /* bits 4 - 6 */
			uint32_t rsv0        : 1; /* bit 7 */
			uint32_t err_code    : 3; /* bits 8 - 10 */
			uint32_t sizef       : 3; /* bits 11 - 13 */
			uint32_t rsv1        : 2; /* bits 14 - 15 */
			uint32_t addr_space  : 6; /* bits 16 - 21 */
			uint32_t rsv2        : 10; /* bits 22 - 31 */
		};
		uint32_t value;
	}  errlog0_low;
	union {
		struct {
			uint32_t len1 : 10; /* bits 0 - 9 */
			uint32_t rsv  : 22; /* bits 10 - 31 */
		};
		uint32_t value;
	} errlog0_high;
	union {
		struct {
			uint32_t path : 16; /* bits 0 - 15 */
			uint32_t rsv  : 16; /* bits 16 - 31 */
		};
		uint32_t value;
	} errlog1_low;
	union {
		struct {
			uint32_t extid : 18; /* bits 0 - 17 */
			uint32_t rsv   : 14; /* bits 18 - 31 */
		};
		uint32_t value;
	} errlog1_high;
	union {
		struct {
			uint32_t errlog2_lsb : 32; /* bits 0 - 31 */
		};
		uint32_t value;
	} errlog2_low;
	union {
		struct {
			uint32_t errlog2_msb : 16; /* bits 0 - 16 */
			uint32_t rsv         : 16; /* bits 16 - 31 */
		};
		uint32_t value;
	} errlog2_high;
	union {
		struct {
			uint32_t errlog3_lsb : 32; /* bits 0 - 31 */
		};
		uint32_t value;
	} errlog3_low;
	union {
		struct {
			uint32_t errlog3_msb : 32; /* bits 0 - 31 */
		};
		uint32_t value;
	} errlog3_high;
};

/**
 * struct cam_camnoc_irq_ubwc_enc_data : Data for UBWC Encode error.
 *
 * @encerr_status : Encode error status
 *
 */
struct cam_camnoc_irq_ubwc_enc_data {
	union {
		struct {
			uint32_t encerrstatus : 3; /* bits 0 - 2 */
			uint32_t rsv          : 29; /* bits 3 - 31 */
		};
		uint32_t value;
	} encerr_status;
};

/**
 * struct cam_camnoc_irq_ubwc_dec_data : Data for UBWC Decode error.
 *
 * @decerr_status : Decoder error status
 * @thr_err       : Set to 1 if
 *                  At least one of the bflc_len fields in the bit steam exceeds
 *                  its threshold value. This error is possible only for
 *                  RGBA1010102, TP10, and RGB565 formats
 * @fcl_err       : Set to 1 if
 *                  Fast clear with a legal non-RGB format
 * @len_md_err    : Set to 1 if
 *                  The calculated burst length does not match burst length
 *                  specified by the metadata value
 * @format_err    : Set to 1 if
 *                  Illegal format
 *                  1. bad format :2,3,6
 *                  2. For 32B MAL, metadata=6
 *                  3. For 32B MAL RGB565, Metadata != 0,1,7
 *                  4. For 64B MAL RGB565, metadata[3:1] == 1,2
 *
 */
struct cam_camnoc_irq_ubwc_dec_data {
	union {
		struct {
			uint32_t thr_err    : 1; /* bit 0 */
			uint32_t fcl_err    : 1; /* bit 1 */
			uint32_t len_md_err : 1; /* bit 2 */
			uint32_t format_err : 1; /* bit 3 */
			uint32_t rsv        : 28; /* bits 4 - 31 */
		};
		uint32_t value;
	} decerr_status;
};

struct cam_camnoc_irq_ahb_timeout_data {
	uint32_t data;
};

/**
 * struct cam_cpas_irq_data : CAMNOC IRQ data
 *
 * @irq_type  : To identify the type of IRQ
 * @u         : Union of irq err data information
 * @slave_err : Data for Slave error.
 *              Valid if type is CAM_CAMNOC_IRQ_SLAVE_ERROR
 * @enc_err   : Data for UBWC Encode error.
 *              Valid if type is one of below:
 *              CAM_CAMNOC_IRQ_IFE02_UBWC_ENCODE_ERROR
 *              CAM_CAMNOC_IRQ_IFE13_UBWC_ENCODE_ERROR
 *              CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR
 * @dec_err   : Data for UBWC Decode error.
 *              Valid if type is CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR
 * @ahb_err   : Data for Slave error.
 *              Valid if type is CAM_CAMNOC_IRQ_AHB_TIMEOUT
 *
 */
struct cam_cpas_irq_data {
	enum cam_camnoc_irq_type irq_type;
	union {
		struct cam_camnoc_irq_slave_err_data   slave_err;
		struct cam_camnoc_irq_ubwc_enc_data    enc_err;
		struct cam_camnoc_irq_ubwc_dec_data    dec_err;
		struct cam_camnoc_irq_ahb_timeout_data ahb_err;
	} u;
};

/**
 * struct cam_cpas_register_params : Register params for cpas client
 *
 * @identifier        : Input identifier string which is the device label
 *                      from dt like vfe, ife, jpeg etc
 * @cell_index        : Input integer identifier pointing to the cell index
 *                      from dt of the device. This can be used to form a
 *                      unique string with @identifier like vfe0, ife1,
 *                      jpeg0, etc
 * @dev               : device handle
 * @userdata          : Input private data which will be passed as
 *                      an argument while callback.
 * @cam_cpas_callback : Input callback pointer for triggering the
 *                      callbacks from CPAS driver.
 *                      @client_handle : CPAS client handle
 *                      @userdata    : User data given at the time of register
 *                      @event_type  : event type
 *                      @event_data  : event data
 * @client_handle       : Output Unique handle generated for this register
 *
 */
struct cam_cpas_register_params {
	char            identifier[CAM_HW_IDENTIFIER_LENGTH];
	uint32_t        cell_index;
	struct device  *dev;
	void           *userdata;
	bool          (*cam_cpas_client_cb)(
			uint32_t                  client_handle,
			void                     *userdata,
			struct cam_cpas_irq_data *irq_data);
	uint32_t        client_handle;
};

/**
 * enum cam_vote_type - Enum for voting type
 *
 * @CAM_VOTE_ABSOLUTE : Absolute vote
 * @CAM_VOTE_DYNAMIC  : Dynamic vote
 */
enum cam_vote_type {
	CAM_VOTE_ABSOLUTE,
	CAM_VOTE_DYNAMIC,
};

/**
 * struct cam_ahb_vote : AHB vote
 *
 * @type  : AHB voting type.
 *          CAM_VOTE_ABSOLUTE : vote based on the value 'level' is set
 *          CAM_VOTE_DYNAMIC  : vote calculated dynamically using 'freq'
 *                              and 'dev' handle is set
 * @level : AHB vote level
 * @freq  : AHB vote dynamic frequency
 *
 */
struct cam_ahb_vote {
	enum cam_vote_type   type;
	union {
		enum cam_vote_level  level;
		unsigned long        freq;
	} vote;
};

/**
 * struct cam_axi_vote : AXI vote
 *
 * @uncompressed_bw : Bus bandwidth required in Bytes for uncompressed data
 *                    This is the required bandwidth for uncompressed
 *                    data traffic between hw core and camnoc.
 * @compressed_bw   : Bus bandwidth required in Bytes for compressed data.
 *                    This is the required bandwidth for compressed
 *                    data traffic between camnoc and mmnoc.
 *
 * If one of the above is not applicable to a hw client, it has to
 * fill the same values in both.
 *
 */
struct cam_axi_vote {
	uint64_t   uncompressed_bw;
	uint64_t   compressed_bw;
};

/**
 * cam_cpas_register_client()
 *
 * @brief: API to register cpas client
 *
 * @register_params: Input params to register as a client to CPAS
 *
 * @return 0 on success.
 *
 */
int cam_cpas_register_client(
	struct cam_cpas_register_params *register_params);

/**
 * cam_cpas_unregister_client()
 *
 * @brief: API to unregister cpas client
 *
 * @client_handle: Client handle to be unregistered
 *
 * @return 0 on success.
 *
 */
int cam_cpas_unregister_client(uint32_t client_handle);

/**
 * cam_cpas_start()
 *
 * @brief: API to start cpas client hw. Clients have to vote for minimal
 *     bandwidth requirements for AHB, AXI. Use cam_cpas_update_ahb_vote
 *     to scale bandwidth after start.
 *
 * @client_handle: client cpas handle
 * @ahb_vote     : Pointer to ahb vote info
 * @axi_vote     : Pointer to axi bandwidth vote info
 *
 * If AXI vote is not applicable to a particular client, use the value exposed
 * by CAM_CPAS_DEFAULT_AXI_BW as the default vote request.
 *
 * @return 0 on success.
 *
 */
int cam_cpas_start(
	uint32_t               client_handle,
	struct cam_ahb_vote   *ahb_vote,
	struct cam_axi_vote   *axi_vote);

/**
 * cam_cpas_stop()
 *
 * @brief: API to stop cpas client hw. Bandwidth for AHB, AXI votes
 *     would be removed for this client on this call. Clients should not
 *     use cam_cpas_update_ahb_vote or cam_cpas_update_axi_vote
 *     to remove their bandwidth vote.
 *
 * @client_handle: client cpas handle
 *
 * @return 0 on success.
 *
 */
int cam_cpas_stop(uint32_t client_handle);

/**
 * cam_cpas_update_ahb_vote()
 *
 * @brief: API to update AHB vote requirement. Use this function only
 *     between cam_cpas_start and cam_cpas_stop in case clients wants
 *     to scale to different vote level. Do not use this function to de-vote,
 *     removing client's vote is implicit on cam_cpas_stop
 *
 * @client_handle : Client cpas handle
 * @ahb_vote      : Pointer to ahb vote info
 *
 * @return 0 on success.
 *
 */
int cam_cpas_update_ahb_vote(
	uint32_t               client_handle,
	struct cam_ahb_vote   *ahb_vote);

/**
 * cam_cpas_update_axi_vote()
 *
 * @brief: API to update AXI vote requirement. Use this function only
 *     between cam_cpas_start and cam_cpas_stop in case clients wants
 *     to scale to different vote level. Do not use this function to de-vote,
 *     removing client's vote is implicit on cam_cpas_stop
 *
 * @client_handle : Client cpas handle
 * @axi_vote      : Pointer to axi bandwidth vote info
 *
 * @return 0 on success.
 *
 */
int cam_cpas_update_axi_vote(
	uint32_t             client_handle,
	struct cam_axi_vote *axi_vote);

/**
 * cam_cpas_reg_write()
 *
 * @brief: API to write a register value in CPAS register space
 *
 * @client_handle : Client cpas handle
 * @reg_base      : Register base identifier
 * @offset        : Offset from the register base address
 * @mb            : Whether to do reg write with memory barrier
 * @value         : Value to be written in register
 *
 * @return 0 on success.
 *
 */
int cam_cpas_reg_write(
	uint32_t                  client_handle,
	enum cam_cpas_reg_base    reg_base,
	uint32_t                  offset,
	bool                      mb,
	uint32_t                  value);

/**
 * cam_cpas_reg_read()
 *
 * @brief: API to read a register value from CPAS register space
 *
 * @client_handle : Client cpas handle
 * @reg_base      : Register base identifier
 * @offset        : Offset from the register base address
 * @mb            : Whether to do reg read with memory barrier
 * @value         : Value to be red from register
 *
 * @return 0 on success.
 *
 */
int cam_cpas_reg_read(
	uint32_t                  client_handle,
	enum cam_cpas_reg_base    reg_base,
	uint32_t                  offset,
	bool                      mb,
	uint32_t                 *value);

/**
 * cam_cpas_get_hw_info()
 *
 * @brief: API to get camera hw information
 *
 * @camera_family  : Camera family type. One of
 *                   CAM_FAMILY_CAMERA_SS
 *                   CAM_FAMILY_CPAS_SS
 * @camera_version : Camera platform version
 * @cpas_version   : Camera cpas version
 * @cam_caps       : Camera capability
 *
 * @return 0 on success.
 *
 */
int cam_cpas_get_hw_info(
	uint32_t                 *camera_family,
	struct cam_hw_version    *camera_version,
	struct cam_hw_version    *cpas_version,
	uint32_t                 *cam_caps);

/**
 * cam_cpas_get_cpas_hw_version()
 *
 * @brief: API to get camera cpas hw version
 *
 * @hw_version  : Camera cpas hw version
 *
 * @return 0 on success.
 *
 */
int cam_cpas_get_cpas_hw_version(
	uint32_t				 *hw_version);

#endif /* _CAM_CPAS_API_H_ */
