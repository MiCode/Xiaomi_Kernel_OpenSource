/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
	void          (*cam_cpas_client_cb)(
			uint32_t                  client_handle,
			void                     *userdata,
			enum cam_camnoc_irq_type  event_type,
			uint32_t                  event_data);
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
 *
 * @return 0 on success.
 *
 */
int cam_cpas_get_hw_info(
	uint32_t                 *camera_family,
	struct cam_hw_version    *camera_version,
	struct cam_hw_version    *cpas_version);

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
