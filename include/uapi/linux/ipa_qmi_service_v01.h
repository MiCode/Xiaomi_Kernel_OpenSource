/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

/*
 * This header file defines the types and structures that were defined in
 * ipa. It contains the constant values defined, enums, structures,
 * messages, and service message IDs (in that order) Structures that were
 * defined in the IDL as messages contain mandatory elements, optional
 * elements, a combination of mandatory and optional elements (mandatory
 * always come before optionals in the structure), or nothing (null message)

 * An optional element in a message is preceded by a uint8_t value that must be
 * set to true if the element is going to be included. When decoding a received
 * message, the uint8_t values will be set to true or false by the decode
 * routine, and should be checked before accessing the values that they
 * correspond to.

 * Variable sized arrays are defined as static sized arrays with an unsigned
 * integer (32 bit) preceding it that must be set to the number of elements
 * in the array that are valid. For Example:

 * uint32_t test_opaque_len;
 * uint8_t test_opaque[16];

 * If only 4 elements are added to test_opaque[] then test_opaque_len must be
 * set to 4 before sending the message.  When decoding, the _len value is set
 * by the decode routine and should be checked so that the correct number of
 * elements in the array will be accessed.
 */
#ifndef IPA_QMI_SERVICE_V01_H
#define IPA_QMI_SERVICE_V01_H

#define QMI_IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS_V01 2
#define QMI_IPA_IPFLTR_NUM_MEQ_32_EQNS_V01 2
#define QMI_IPA_IPFLTR_NUM_IHL_MEQ_32_EQNS_V01 2
#define QMI_IPA_IPFLTR_NUM_MEQ_128_EQNS_V01 2
#define QMI_IPA_MAX_FILTERS_V01 64

#define IPA_INT_MAX	((int)(~0U>>1))
#define IPA_INT_MIN	(-IPA_INT_MAX - 1)

/* IPA definition as msm_qmi_interface.h */

enum ipa_qmi_result_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use*/
	IPA_QMI_RESULT_TYPE_MIN_ENUM_VAL_V01 = IPA_INT_MIN,
	IPA_QMI_RESULT_SUCCESS_V01 = 0,
	IPA_QMI_RESULT_FAILURE_V01 = 1,
	IPA_QMI_RESULT_TYPE_MAX_ENUM_VAL_V01 = IPA_INT_MAX,
};

enum ipa_qmi_error_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use*/
	IPA_QMI_ERROR_TYPE_MIN_ENUM_VAL_V01 = IPA_INT_MIN,
	IPA_QMI_ERR_NONE_V01 = 0x0000,
	IPA_QMI_ERR_MALFORMED_MSG_V01 = 0x0001,
	IPA_QMI_ERR_NO_MEMORY_V01 = 0x0002,
	IPA_QMI_ERR_INTERNAL_V01 = 0x0003,
	IPA_QMI_ERR_CLIENT_IDS_EXHAUSTED_V01 = 0x0005,
	IPA_QMI_ERR_INVALID_ID_V01 = 0x0029,
	IPA_QMI_ERR_ENCODING_V01 = 0x003A,
	IPA_QMI_ERR_INCOMPATIBLE_STATE_V01 = 0x005A,
	IPA_QMI_ERR_NOT_SUPPORTED_V01 = 0x005E,
	IPA_QMI_ERROR_TYPE_MAX_ENUM_VAL_V01 = IPA_INT_MAX,
};

struct ipa_qmi_response_type_v01 {
	enum ipa_qmi_result_type_v01 result;
	enum ipa_qmi_error_type_v01 error;
};

enum ipa_platform_type_enum_v01 {
	IPA_PLATFORM_TYPE_ENUM_MIN_ENUM_VAL_V01 =
	-2147483647, /* To force a 32 bit signed enum.  Do not change or use */
	QMI_IPA_PLATFORM_TYPE_INVALID_V01 = 0,
	/*  Invalid platform identifier */
	QMI_IPA_PLATFORM_TYPE_TN_V01 = 1,
	/*  Platform identifier -	Data card device */
	QMI_IPA_PLATFORM_TYPE_LE_V01 = 2,
	/*  Platform identifier -	Data router device */
	QMI_IPA_PLATFORM_TYPE_MSM_ANDROID_V01 = 3,
	/*  Platform identifier -	MSM device with Android HLOS */
	QMI_IPA_PLATFORM_TYPE_MSM_WINDOWS_V01 = 4,
	/*  Platform identifier -	MSM device with Windows HLOS */
	QMI_IPA_PLATFORM_TYPE_MSM_QNX_V01 = 5,
	/*  Platform identifier -	MSM device with QNX HLOS */
	IPA_PLATFORM_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use */
};

struct ipa_hdr_tbl_info_type_v01 {
	uint32_t modem_offset_start;
	/*	Offset from the start of IPA Shared memory from which
	 *	modem driver may insert header table entries.
	 */
	uint32_t modem_offset_end;
	/*	Offset from the start of IPA shared mem beyond which modem
	 *	driver shall not insert header table entries. The space
	 *	available for the modem driver shall include the
	 *	modem_offset_start and modem_offset_end.
	 */
};  /* Type */

struct ipa_route_tbl_info_type_v01 {
	uint32_t route_tbl_start_addr;
	/*	Identifies the start of the routing table. Denotes the offset
	 *	from the start of the IPA Shared Mem
	 */

	uint32_t num_indices;
	/*	Number of indices (starting from 0) that is being allocated to
	 *	the modem. The number indicated here is also included in the
	 *	allocation. The value of num_indices shall not exceed 31
	 *	(5 bits used to specify the routing table index), unless there
	 *	is a change in the hardware.
	 */
};  /* Type */

struct ipa_modem_mem_info_type_v01 {

	uint32_t block_start_addr;
	/*	Identifies the start of the memory block allocated for the
	 *	modem. Denotes the offset from the start of the IPA Shared Mem
	 */

	uint32_t size;
	/*	Size of the block allocated for the modem driver */
};  /* Type */

/** Request Message; Requests the modem IPA driver to perform initializtion */
struct ipa_init_modem_driver_req_msg_v01 {

	/* Optional */
	/*  Platform info */
	uint8_t platform_type_valid;  /**< Must be set to true if platform_type
	is being passed */
	enum ipa_platform_type_enum_v01 platform_type;
	/*   Provides information about the platform (ex. TN/MN/LE/MSM,etc) */

	/* Optional */
	/*  Header table info */
	uint8_t hdr_tbl_info_valid;
	/* Must be set to true if hdr_tbl_info is being passed */
	struct ipa_hdr_tbl_info_type_v01 hdr_tbl_info;
	/*	Provides information about the header table */

	/* Optional */
	/*  IPV4 Routing table info */
	uint8_t v4_route_tbl_info_valid;
	/* Must be set to true if v4_route_tbl_info is being passed */
	struct ipa_route_tbl_info_type_v01 v4_route_tbl_info;
	/*	Provides information about the IPV4 routing table */

	/* Optional */
	/*  IPV6 Routing table info */
	uint8_t v6_route_tbl_info_valid;  /**< Must be set to true if
	v6_route_tbl_info is being passed */
	struct ipa_route_tbl_info_type_v01 v6_route_tbl_info;
	/*	Provides information about the IPV6 routing table */

	/* Optional */
	/*  IPV4 Filter table start address */
	uint8_t v4_filter_tbl_start_addr_valid;  /**< Must be set to true
	if v4_filter_tbl_start_addr is being passed */
	uint32_t v4_filter_tbl_start_addr;
	/*	Provides information about the starting address of IPV4 filter
	 *	tableDenotes the offset from the start of the IPA Shared Mem
	 */

	/* Optional */
	/* IPV6 Filter table start address */
	uint8_t v6_filter_tbl_start_addr_valid;
	/* Must be set to true if v6_filter_tbl_start_addr is being passed */
	uint32_t v6_filter_tbl_start_addr;
	/*	Provides information about the starting address of IPV6 filter
	 *	table Denotes the offset from the start of the IPA Shared Mem
	 */

	/* Optional */
	/*  Modem memory block */
	uint8_t modem_mem_info_valid;
	/* Must be set to true if modem_mem_info is being passed */
	struct ipa_modem_mem_info_type_v01 modem_mem_info;
	/*  Provides information about the start address and the size of
	 *	the memory block that is being allocated to the modem driver.
	 *	Denotes the physical address
	 */

	/* Optional */
	/*  Destination end point for control commands from modem */
	uint8_t ctrl_comm_dest_end_pt_valid;  /**< Must be set to true if
	ctrl_comm_dest_end_pt is being passed */
	uint32_t ctrl_comm_dest_end_pt;
	/*  Provides information about the destination end point on the
	 *	application processor to which the modem driver can send
	 *	control commands. The value of this parameter cannot exceed
	 *	19 since IPA only supports 20 end points.
	 */

	/* Optional */
	/*  Modem Bootup Information */
	uint8_t is_ssr_bootup_valid;  /**< Must be set to true if
	is_ssr_bootup is being passed */
	uint8_t is_ssr_bootup;
	/*	Specifies whether the modem is booting up after a modem only
	 *	sub-system restart or not. This will let the modem driver
	 *	know that it doesn't have to reinitialize some of the HW
	 *	blocks because IPA has not been reset since the previous
	 *	initialization.
	 */

};  /* Message */

/* Response Message; Requests the modem IPA driver about initializtion */
struct ipa_init_modem_driver_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type.*/

	/* Optional */
	/* Destination end point for control commands from master driver */
	uint8_t ctrl_comm_dest_end_pt_valid;
	/* Must be set to true if ctrl_comm_dest_ep is being passed */
	uint32_t ctrl_comm_dest_end_pt;
	/*	Provides information about the destination end point on the
	 *	modem processor to which the master driver can send control
	 *	commands. The value of this parameter cannot exceed 19 since
	 *	IPA only supports 20 end points. This field is looked at only
	 *	if the result in TLV RESULT_CODE is	QMI_RESULT_SUCCESS
	 */

	/* Optional */
	/*  Default end point */
	uint8_t default_end_pt_valid;
	/* Must be set to true if default_end_pt is being passed */
	uint32_t default_end_pt;
	/*  Provides information about the default end point. The master
	 *	driver may or may not set the register in the hardware with
	 *	this value. The value of this parameter cannot exceed 19
	 *	since IPA only supports 20 end points. This field is looked
	 *	at only if the result in TLV RESULT_CODE is QMI_RESULT_SUCCESS
	 */
};  /* Message */


/*	Request Message; This is the message that is exchanged between the
 *	control point and the service in order to register for indications.
 */
struct ipa_indication_reg_req_msg_v01 {
	/* Optional */
	/*  Master driver initialization completion */
	uint8_t master_driver_init_complete_valid;
	/* Must be set to true if master_driver_init_complete is being passed */
	uint8_t master_driver_init_complete;
	/*  If set to TRUE, this field indicates that the client is
	 *	interested in getting indications about the completion
	 *	of the initialization sequence of the master driver.
	 *	Setting this field in the request message makes sense
	 *	only when the QMI_IPA_INDICATION_REGISTER_REQ is being
	 *	originated from the modem driver
	 */
};  /* Message */


/* Response Message; This is the message that is exchanged between the
 *	control point and the service in order to register for indications.
 */
struct ipa_indication_reg_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/**<   Standard response type.*/
};  /* Message */


/*	Indication Message; Indication sent to the Modem IPA driver from
 *	master IPA driver about initialization being complete.
 */
struct ipa_master_driver_init_complt_ind_msg_v01 {
	/* Mandatory */
	/*  Master driver initialization completion status */
	struct ipa_qmi_response_type_v01 master_driver_init_status;
	/*	Indicates the status of initialization. If everything went
	 *	as expected, this field is set to SUCCESS. ERROR is set
	 *	otherwise. Extended error info may be used to convey
	 *	additional information about the error
	 */
};  /* Message */

struct ipa_ipfltr_range_eq_16_type_v01 {
	uint8_t offset;
	/*	Specifies the offset from the IHL (Internet Header length) */

	uint16_t range_low;
	/*	Specifies the lower bound of the range */

	uint16_t range_high;
	/*	Specifies the upper bound of the range */
};  /* Type */

struct ipa_ipfltr_mask_eq_32_type_v01 {
	uint8_t offset;
	/*	Specifies the offset either from IHL or from the start of
	 *	the IP packet. This depends on the equation that this structure
	 *	is used in.
	 */

	uint32_t mask;
	/*	Specifies the mask that has to be used in the comparison.
	 *	The field is ANDed with the mask and compared against the value.
	 */

	uint32_t value;
	/*	Specifies the 32 bit value that used in the comparison. */
};  /* Type */

struct ipa_ipfltr_eq_16_type_v01 {
	uint8_t offset;
	/*  Specifies the offset into the packet */

	uint16_t value;
	/* Specifies the 16 bit value that should be used in the comparison. */
};  /* Type */

struct ipa_ipfltr_eq_32_type_v01 {
	uint8_t offset;
	/* Specifies the offset into the packet */

	uint32_t value;
	/* Specifies the 32 bit value that should be used in the comparison. */
};  /* Type */

struct ipa_ipfltr_mask_eq_128_type_v01 {
	uint8_t offset;
	/* Specifies the offset into the packet */

	uint8_t mask[16];
	/*  Specifies the mask that has to be used in the comparison.
	 *	The field is ANDed with the mask and compared against the value.
	 */

	uint8_t value[16];
	/* Specifies the 128 bit value that should be used in the comparison. */
};  /* Type */


struct ipa_filter_rule_type_v01 {
	uint16_t rule_eq_bitmap;
	/* 16-bit Bitmask to indicate how many eqs are valid in this rule */

	uint8_t tos_eq_present;
	/* Specifies if a type of service check rule is present */

	uint8_t tos_eq;
	/* The value to check against the type of service (ipv4) field */

	uint8_t protocol_eq_present;
	/* Specifies if a protocol check rule is present */

	uint8_t protocol_eq;
	/* The value to check against the protocol field */

	uint8_t num_ihl_offset_range_16;
	/*  The number of 16 bit range check rules at the location
	 *	determined by IP header length plus a given offset offset
	 *	in this rule. See the definition of the ipa_filter_range_eq_16
	 *	for better understanding. The value of this field cannot exceed
	 *	IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS which is set as 2
	 */

	struct ipa_ipfltr_range_eq_16_type_v01
		ihl_offset_range_16[QMI_IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS_V01];
	/*	Array of the registered IP header length offset 16 bit range
	 *	check rules.
	 */

	uint8_t num_offset_meq_32;
	/*  The number of 32 bit masked comparison rules present
	 *  in this rule
	 */

	struct ipa_ipfltr_mask_eq_32_type_v01
		offset_meq_32[QMI_IPA_IPFLTR_NUM_MEQ_32_EQNS_V01];
	/*  An array of all the possible 32bit masked comparison rules
	 *	in this rule
	 */

	uint8_t tc_eq_present;
	/*  Specifies if the traffic class rule is present in this rule */

	uint8_t tc_eq;
	/* The value against which the IPV4 traffic class field has to
		be checked */

	uint8_t flow_eq_present;
	/* Specifies if the "flow equals" rule is present in this rule */

	uint32_t flow_eq;
	/* The value against which the IPV6 flow field has to be checked */

	uint8_t ihl_offset_eq_16_present;
	/*	Specifies if there is a 16 bit comparison required at the
	 *	location in	the packet determined by "Intenet Header length
	 *	+ specified offset"
	 */

	struct ipa_ipfltr_eq_16_type_v01 ihl_offset_eq_16;
	/* The 16 bit comparison equation */

	uint8_t ihl_offset_eq_32_present;
	/*	Specifies if there is a 32 bit comparison required at the
	 *	location in the packet determined by "Intenet Header length
	 *	+ specified offset"
	 */

	struct ipa_ipfltr_eq_32_type_v01 ihl_offset_eq_32;
	/*	The 32 bit comparison equation */

	uint8_t num_ihl_offset_meq_32;
	/*	The number of 32 bit masked comparison equations in this
	 *	rule. The location of the packet to be compared is
	 *	determined by the IP Header length + the give offset
	 */

	struct ipa_ipfltr_mask_eq_32_type_v01
		ihl_offset_meq_32[QMI_IPA_IPFLTR_NUM_IHL_MEQ_32_EQNS_V01];
	/*	Array of 32 bit masked comparison equations.
	*/

	uint8_t num_offset_meq_128;
	/*	The number of 128 bit comparison equations in this rule */

	struct ipa_ipfltr_mask_eq_128_type_v01
		offset_meq_128[QMI_IPA_IPFLTR_NUM_MEQ_128_EQNS_V01];
	/*	Array of 128 bit comparison equations. The location in the
	 *	packet is determined by the specified offset
	 */

	uint8_t metadata_meq32_present;
	/*  Boolean indicating if the 32 bit masked comparison equation
	 *	is present or not. Comparison is done against the metadata
	 *	in IPA. Metadata can either be extracted from the packet
	 *	header or from the "metadata" register.
	 */

	struct ipa_ipfltr_mask_eq_32_type_v01
			metadata_meq32;
	/* The metadata  32 bit masked comparison equation */

	uint8_t ipv4_frag_eq_present;
	/* Specifies if the IPv4 Fragment equation is present in this rule */
};  /* Type */


enum ipa_ip_type_enum_v01 {
	IPA_IP_TYPE_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum.  Do not change or use*/
	QMI_IPA_IP_TYPE_INVALID_V01 = 0,
	/*  Invalid IP type identifier */
	QMI_IPA_IP_TYPE_V4_V01 = 1,
	/*  IP V4 type */
	QMI_IPA_IP_TYPE_V6_V01 = 2,
	/*  IP V6 type */
	QMI_IPA_IP_TYPE_V4V6_V01 = 3,
	/*  Applies to both IP types */
	IPA_IP_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use*/
};


enum ipa_filter_action_enum_v01 {
	IPA_FILTER_ACTION_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum. Do not change or use */
	QMI_IPA_FILTER_ACTION_INVALID_V01 = 0,
	/*  Invalid action on filter hit */
	QMI_IPA_FILTER_ACTION_SRC_NAT_V01 = 1,
	/*  Pass packet to NAT block for Source NAT */
	QMI_IPA_FILTER_ACTION_DST_NAT_V01 = 2,
	/*  Pass packet to NAT block for Destination NAT */
	QMI_IPA_FILTER_ACTION_ROUTING_V01 = 3,
	/*  Pass packet to Routing block */
	QMI_IPA_FILTER_ACTION_EXCEPTION_V01 = 4,
	/*  Treat packet as exception and send to exception pipe */
	IPA_FILTER_ACTION_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use*/
};

struct ipa_filter_spec_type_v01 {
	uint32_t filter_spec_identifier;
	/*	This field is used to identify a filter spec in the list
	 *	of filter specs being sent from the client. This field
	 *	is applicable only in the filter install request and response.
	 */

	enum ipa_ip_type_enum_v01 ip_type;
	/*	This field identifies the IP type for which this rule is
	 *	applicable. The driver needs to identify the filter table
	 *	(V6 or V4) and this field is essential for that
	 */

	struct ipa_filter_rule_type_v01 filter_rule;
	/*	This field specifies the rules in the filter spec. These rules
	 *	are the ones that are matched against fields in the packet.
	 */

	enum ipa_filter_action_enum_v01 filter_action;
	/*	This field specifies the action to be taken when a filter match
	 *	occurs. The remote side should install this information into the
	 *	hardware along with the filter equations.
	 */

	uint8_t is_routing_table_index_valid;
	/*	Specifies whether the routing table index is present or not.
	 *	If the action is "QMI_IPA_FILTER_ACTION_EXCEPTION", this
	 *	parameter need not be provided.
	 */

	uint32_t route_table_index;
	/*	This is the index in the routing table that should be used
	 *	to route the packets if the filter rule is hit
	 */

	uint8_t is_mux_id_valid;
	/*	Specifies whether the mux_id is valid */

	uint32_t mux_id;
	/*	This field identifies the QMAP MUX ID. As a part of QMAP
	 *	protocol, several data calls may be multiplexed over the
	 *	same physical transport channel. This identifier is used to
	 *	identify one such data call. The maximum value for this
	 *	identifier is 255.
	 */
};  /* Type */


/*  Request Message; This is the message that is exchanged between the
 *	control point and the service in order to request the installation
 *	of filtering rules in the hardware block by the remote side.
 */
struct ipa_install_fltr_rule_req_msg_v01 {
	/* Optional */
	/*  IP type that this rule applies to
	Filter specification to be installed in the hardware */
	uint8_t filter_spec_list_valid;
	/* Must be set to true if filter_spec_list is being passed */
	uint32_t filter_spec_list_len;
	/* Must be set to # of elements in filter_spec_list */
	struct ipa_filter_spec_type_v01
		filter_spec_list[QMI_IPA_MAX_FILTERS_V01];
	/*	This structure defines the list of filters that have
	 *		to be installed in the hardware. The driver installing
	 *		these rules shall do so in the same order as specified
	 *		in this list.
	 */

	/* Optional */
	/*  Pipe index to intall rule */
	uint8_t source_pipe_index_valid;
	/* Must be set to true if source_pipe_index is being passed */
	uint32_t source_pipe_index;
	/*	This is the source pipe on which the filter rule is to be
	 *	installed. The requestor may always not know the pipe
	 *	indices. If not specified, the receiver shall install
	 *	this rule on all the pipes that it controls through
	 *	which data may be fed into IPA.
	 */
};  /* Message */

struct ipa_filter_rule_identifier_to_handle_map_v01 {
	uint32_t filter_spec_identifier;
	/*	This field is used to identify a filter spec in the list of
	 *	filter specs being sent from the client. This field is
	 *	applicable only in the filter install request and response.
	 */
	uint32_t filter_handle;
	/*  This field is used to identify a rule in any subsequent message.
	 *	This is a value that is provided by the server to the control
	 *	point
	 */
};  /* Type */

/* Response Message; This is the message that is exchanged between the
 * control point and the service in order to request the
 * installation of filtering rules in the hardware block by
 * the remote side.
 */
struct ipa_install_fltr_rule_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/*	Standard response type.
	 *	Standard response type. Contains the following data members:
	 *	- qmi_result_type -- QMI_RESULT_SUCCESS or QMI_RESULT_FAILURE
	 *	- qmi_error_type  -- Error code. Possible error code values are
	 *	described in the error codes section of each message definition.
	 */

	/* Optional */
	/*  Filter Handle list */
	uint8_t filter_handle_list_valid;
	/**< Must be set to true if filter_handle_list is being passed */
	uint32_t filter_handle_list_len;
	/* Must be set to # of elements in filter_handle_list */
	struct ipa_filter_rule_identifier_to_handle_map_v01
		filter_handle_list[QMI_IPA_MAX_FILTERS_V01];
	/*  This is a list of handles returned to the control point.
	 *	Each handle is mapped to the rule identifier that was
	 *	specified in the request message. Any further reference
	 *	to the rule is done using the filter handle
	 */
};  /* Message */

struct ipa_filter_handle_to_index_map_v01 {
	uint32_t filter_handle;
	/*	This is a handle that was given to the remote client that
	 *	requested the rule addition.
	 */
	uint32_t filter_index;
	/*	This index denotes the location in a filter table, where the
	 *	filter rule has been installed. The maximum value of this
	 *	field is 64.
	 */
};  /* Type */

/* Request Message; This is the message that is exchanged between the
 * control point and the service in order to notify the remote driver
 * about the installation of the filter rule supplied earlier by
 * the remote driver
 */
struct ipa_fltr_installed_notif_req_msg_v01 {
	/*	Mandatory	*/
	/*  Pipe index	*/
	uint32_t source_pipe_index;
	/*	This is the source pipe on which the filter rule has been
	 *	installed or was attempted to be installed
	 */

	/* Mandatory */
	/*  Installation Status */
	enum ipa_qmi_result_type_v01 install_status;
	/*	This is the status of installation. If this indicates
	 *	SUCCESS, other optional fields carry additional
	 *	information
	 */

	/* Mandatory */
	/*  List of filter indices */
	uint32_t filter_index_list_len;
	/* Must be set to # of elements in filter_index_list */
	struct ipa_filter_handle_to_index_map_v01
		filter_index_list[QMI_IPA_MAX_FILTERS_V01];
	/*	This field provides the list of filter indices and the
	 *	corresponding filter handle. If the installation_status
	 *	indicates failure, then the filter indices shall be set
	 *	to a reserve index (255)
	 */

	/* Optional */
	/*  Embedded pipe index */
	uint8_t embedded_pipe_index_valid;
	/* Must be set to true if embedded_pipe_index is being passed */
	uint32_t embedded_pipe_index;
	/*	This index denotes the embedded pipe number on which a call to
	 *	the same PDN has been made. If this field is set, it denotes
	 *	that this is a use case where PDN sharing is happening. The
	 *	embedded pipe is used to send data from the embedded client
	 *	in the device
	 */

	/* Optional */
	/*  Retain Header Configuration */
	uint8_t retain_header_valid;
	/* Must be set to true if retain_header is being passed */
	uint8_t retain_header;
	/*	This field indicates if the driver installing the rule has
	 *	turned on the "retain header" bit. If this is true, the
	 *	header that is removed by IPA is reinserted after the
	 *	packet processing is completed.
	 */

	/* Optional */
	/*  Embedded call Mux Id */
	uint8_t embedded_call_mux_id_valid;
	/**< Must be set to true if embedded_call_mux_id is being passed */
	uint32_t embedded_call_mux_id;
	/*	This identifies one of the many calls that have been originated
	 *	on the embedded pipe. This is how we identify the PDN gateway to
	 *	which traffic from the source pipe has to flow.
	 */

};  /* Message */

/* Response Message; This is the message that is exchanged between the control
 *	point and the service in order to notify the remote driver about the
 *	installation of the filter rule supplied earlier by the remote driver
 */
struct ipa_fltr_installed_notif_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/*	Standard response type.*/
};  /* Message */

/*Service Message Definition*/
#define QMI_IPA_INDICATION_REGISTER_REQ_V01 0x0020
#define QMI_IPA_INDICATION_REGISTER_RESP_V01 0x0020
#define QMI_IPA_INIT_MODEM_DRIVER_REQ_V01 0x0021
#define QMI_IPA_INIT_MODEM_DRIVER_RESP_V01 0x0021
#define QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_V01 0x0022
#define QMI_IPA_INSTALL_FILTER_RULE_REQ_V01 0x0023
#define QMI_IPA_INSTALL_FILTER_RULE_RESP_V01 0x0023
#define QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_V01 0x0024
#define QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_V01 0x0024

/* add for max length*/
#define QMI_IPA_INIT_MODEM_DRIVER_REQ_MAX_MSG_LEN_V01 76
#define QMI_IPA_INIT_MODEM_DRIVER_RESP_MAX_MSG_LEN_V01 21
#define QMI_IPA_INDICATION_REGISTER_REQ_MAX_MSG_LEN_V01 4
#define QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01 11019
#define QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01 523
#define QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01 546
#define QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01 7
/* Service Object Accessor */

#endif/* IPA_QMI_SERVICE_V01_H */
