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

#include <linux/types.h>

#define QMI_IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS_V01 2
#define QMI_IPA_IPFLTR_NUM_MEQ_32_EQNS_V01 2
#define QMI_IPA_IPFLTR_NUM_IHL_MEQ_32_EQNS_V01 2
#define QMI_IPA_IPFLTR_NUM_MEQ_128_EQNS_V01 2
#define QMI_IPA_MAX_FILTERS_V01 64
#define QMI_IPA_MAX_FILTERS_EX_V01 128
#define QMI_IPA_MAX_PIPES_V01 20
#define QMI_IPA_MAX_APN_V01 8
#define QMI_IPA_MAX_PER_CLIENTS_V01 64
#define QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01 6
#define QMI_IPA_REMOTE_MHI_MEMORY_MAPPING_NUM_MAX_V01 6
/* Currently max we can use is only 1. But for scalability purpose
 * we are having max value as 8.
 */
#define QMI_IPA_MAX_CLIENT_DST_PIPES_V01 8
#define QMI_IPA_MAX_UL_FIREWALL_RULES_V01 64

/*
 * Indicates presence of newly added member to support HW stats.
 */
#define IPA_QMI_SUPPORTS_STATS

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

struct ipa_hdr_proc_ctx_tbl_info_type_v01 {

	uint32_t modem_offset_start;
	/*  Offset from the start of IPA shared memory from which the modem
	 *	driver may insert header processing context table entries.
	 */

	uint32_t modem_offset_end;
	/*  Offset from the start of IPA shared memory beyond which the modem
	 *	driver may not insert header proc table entries. The space
	 *	available for the modem driver includes modem_offset_start and
	 *	modem_offset_end.
	 */
};  /* Type */

struct ipa_zip_tbl_info_type_v01 {

	uint32_t modem_offset_start;
	/*  Offset from the start of IPA shared memory from which the modem
	 *	driver may insert compression/decompression command entries.
	 */

	uint32_t modem_offset_end;
	/*  Offset from the start of IPA shared memory beyond which the modem
	 *	driver may not insert compression/decompression command entries.
	 *	The space available for the modem driver includes
	 *  modem_offset_start and modem_offset_end.
	 */
};  /* Type */

/**
 * Request Message; Requests the modem IPA driver
 * to perform initialization
 */
struct ipa_init_modem_driver_req_msg_v01 {

	/* Optional */
	/*  Platform info */
	uint8_t platform_type_valid;
	/* Must be set to true if platform_type is being passed */
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
	uint8_t v6_route_tbl_info_valid;
	/* Must be set to true if v6_route_tbl_info is being passed */
	struct ipa_route_tbl_info_type_v01 v6_route_tbl_info;
	/*	Provides information about the IPV6 routing table */

	/* Optional */
	/*  IPV4 Filter table start address */
	uint8_t v4_filter_tbl_start_addr_valid;
	/* Must be set to true if v4_filter_tbl_start_addr is being passed */
	uint32_t v4_filter_tbl_start_addr;
	/*	Provides information about the starting address of IPV4 filter
	 *	table in IPAv2 or non-hashable IPv4 filter table in IPAv3.
	 *	Denotes the offset from the start of the IPA Shared Mem
	 */

	/* Optional */
	/* IPV6 Filter table start address */
	uint8_t v6_filter_tbl_start_addr_valid;
	/* Must be set to true if v6_filter_tbl_start_addr is being passed */
	uint32_t v6_filter_tbl_start_addr;
	/*	Provides information about the starting address of IPV6 filter
	 *	table in IPAv2 or non-hashable IPv6 filter table in IPAv3.
	 *	Denotes the offset from the start of the IPA Shared Mem
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
	uint8_t ctrl_comm_dest_end_pt_valid;
	/* Must be set to true if ctrl_comm_dest_end_pt is being passed */
	uint32_t ctrl_comm_dest_end_pt;
	/*  Provides information about the destination end point on the
	 *	application processor to which the modem driver can send
	 *	control commands. The value of this parameter cannot exceed
	 *	19 since IPA only supports 20 end points.
	 */

	/* Optional */
	/*  Modem Bootup Information */
	uint8_t is_ssr_bootup_valid;
	/* Must be set to true if is_ssr_bootup is being passed */
	uint8_t is_ssr_bootup;
	/*	Specifies whether the modem is booting up after a modem only
	 *	sub-system restart or not. This will let the modem driver
	 *	know that it doesn't have to reinitialize some of the HW
	 *	blocks because IPA has not been reset since the previous
	 *	initialization.
	 */

	/* Optional */
	/*  Header Processing Context Table Information */
	uint8_t hdr_proc_ctx_tbl_info_valid;
	/* Must be set to true if hdr_proc_ctx_tbl_info is being passed */
	struct ipa_hdr_proc_ctx_tbl_info_type_v01 hdr_proc_ctx_tbl_info;
	/* Provides information about the header processing context table.
	 */

	/* Optional */
	/*  Compression Decompression Table Information */
	uint8_t zip_tbl_info_valid;
	/* Must be set to true if zip_tbl_info is being passed */
	struct ipa_zip_tbl_info_type_v01 zip_tbl_info;
	/* Provides information about the zip table.
	 */

	/* Optional */
	/*  IPv4 Hashable Routing Table Information */
	/** Must be set to true if v4_hash_route_tbl_info is being passed */
	uint8_t v4_hash_route_tbl_info_valid;
	struct ipa_route_tbl_info_type_v01 v4_hash_route_tbl_info;

	/* Optional */
	/*  IPv6 Hashable Routing Table Information */
	/** Must be set to true if v6_hash_route_tbl_info is being passed */
	uint8_t v6_hash_route_tbl_info_valid;
	struct ipa_route_tbl_info_type_v01 v6_hash_route_tbl_info;

	/*
	 * Optional
	 * IPv4 Hashable Filter Table Start Address
	 * Must be set to true if v4_hash_filter_tbl_start_addr
	 * is being passed
	 */
	uint8_t v4_hash_filter_tbl_start_addr_valid;
	uint32_t v4_hash_filter_tbl_start_addr;
	/* Identifies the starting address of the IPv4 hashable filter
	 * table in IPAv3 onwards. Denotes the offset from the start of
	 * the IPA shared memory.
	 */

	/* Optional
	 * IPv6 Hashable Filter Table Start Address
	 * Must be set to true if v6_hash_filter_tbl_start_addr
	 * is being passed
	 */
	uint8_t v6_hash_filter_tbl_start_addr_valid;
	uint32_t v6_hash_filter_tbl_start_addr;
	/* Identifies the starting address of the IPv6 hashable filter
	 * table in IPAv3 onwards. Denotes the offset from the start of
	 * the IPA shared memory.
	 */

	/* Optional
	 * Modem HW Stats Quota Base address
	 * Must be set to true if hw_stats_quota_base_addr
	 * is being passed
	 */
	uint8_t hw_stats_quota_base_addr_valid;
	uint32_t hw_stats_quota_base_addr;

	/* Optional
	 * Modem HW Stats Quota Size
	 * Must be set to true if hw_stats_quota_size
	 * is being passed
	 */
	uint8_t hw_stats_quota_size_valid;
	uint32_t hw_stats_quota_size;

	/* Optional
	 * Modem HW Drop Stats Table Start Address
	 * Must be set to true if hw_drop_stats_base_addr
	 * is being passed
	 */
	uint8_t hw_drop_stats_base_addr_valid;
	uint32_t hw_drop_stats_base_addr;

	/* Optional
	 * Modem HW Drop Stats Table size
	 * Must be set to true if hw_drop_stats_table_size
	 * is being passed
	 */
	uint8_t hw_drop_stats_table_size_valid;
	uint32_t hw_drop_stats_table_size;
};  /* Message */

/* Response Message; Requests the modem IPA driver about initialization */
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

	/* Optional */
	/*  Modem Driver Initialization Pending */
	uint8_t modem_driver_init_pending_valid;
	/* Must be set to true if modem_driver_init_pending is being passed */
	uint8_t modem_driver_init_pending;
	/*
	 * Identifies if second level message handshake is needed
	 *	between drivers to indicate when IPA HWP loading is completed.
	 *	If this is set by modem driver, AP driver will need to wait
	 *	for a INIT_MODEM_DRIVER_CMPLT message before communicating with
	 *	IPA HWP.
	 */
};  /* Message */

/*
 * Request Message; Request from Modem IPA driver to indicate
 *	modem driver init completion
 */
struct ipa_init_modem_driver_cmplt_req_msg_v01 {
	/* Mandatory */
	/*  Modem Driver init complete status; */
	uint8_t status;
	/*
	 * Specifies whether the modem driver initialization is complete
	 *	including the micro controller image loading.
	 */
};  /* Message */

/*
 * Response Message; Request from Modem IPA driver to indicate
 *	modem driver init completion
 */
struct ipa_init_modem_driver_cmplt_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/**<   Standard response type.*/
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

	/* Optional */
	/*  Data Usage Quota Reached */
	uint8_t data_usage_quota_reached_valid;
	/*  Must be set to true if data_usage_quota_reached is being passed */
	uint8_t data_usage_quota_reached;
	/*  If set to TRUE, this field indicates that the client wants to
	 *  receive indications about reaching the data usage quota that
	 *  previously set via QMI_IPA_SET_DATA_USAGE_QUOTA. Setting this field
	 *  in the request message makes sense only when the
	 *  QMI_IPA_INDICATION_REGISTER_REQ is being originated from the Master
	 *  driver
	 */

	/* Optional */
	/* IPA MHI Ready Indication */
	uint8_t ipa_mhi_ready_ind_valid;
	/*  Must be set to true if ipa_mhi_ready_ind is being passed */
	uint8_t ipa_mhi_ready_ind;
	/*
	 * If set to TRUE, this field indicates that the client wants to
	 * receive indications about MHI ready for Channel allocations.
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
	/*
	 * tos_eq_present field has two meanings:
	 * IPA ver < 4.5:
	 *  specifies if a type of service check rule is present
	 *  (as the field name reveals).
	 * IPA ver >= 4.5:
	 *  specifies if a tcp pure ack check rule is present
	 */

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
	 * be checked
	 */

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

struct ipa_filter_spec_ex_type_v01 {
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

	uint32_t rule_id;
	/* Rule Id of the given filter. The Rule Id is populated in the rule
	 * header when installing the rule in IPA.
	 */

	uint8_t is_rule_hashable;
	/** Specifies whether the given rule is hashable.
	 */
};  /* Type */


/*  Request Message; This is the message that is exchanged between the
 *	control point and the service in order to request the installation
 *	of filtering rules in the hardware block by the remote side.
 */
struct ipa_install_fltr_rule_req_msg_v01 {
	/* Optional
	 * IP type that this rule applies to
	 * Filter specification to be installed in the hardware
	 */
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

	/* Optional */
	/*  Total number of IPv4 filters in the filter spec list */
	uint8_t num_ipv4_filters_valid;
	/* Must be set to true if num_ipv4_filters is being passed */
	uint32_t num_ipv4_filters;
	/*   Number of IPv4 rules included in filter spec list */

	/* Optional */
	/*  Total number of IPv6 filters in the filter spec list */
	uint8_t num_ipv6_filters_valid;
	/* Must be set to true if num_ipv6_filters is being passed */
	uint32_t num_ipv6_filters;
	/* Number of IPv6 rules included in filter spec list */

	/* Optional */
	/*  List of XLAT filter indices in the filter spec list */
	uint8_t xlat_filter_indices_list_valid;
	/* Must be set to true if xlat_filter_indices_list
	 * is being passed
	 */
	uint32_t xlat_filter_indices_list_len;
	/* Must be set to # of elements in xlat_filter_indices_list */
	uint32_t xlat_filter_indices_list[QMI_IPA_MAX_FILTERS_V01];
	/* List of XLAT filter indices. Filter rules at specified indices
	 * will need to be modified by the receiver if the PDN is XLAT
	 * before installing them on the associated IPA consumer pipe.
	 */

	/* Optional */
	/*  Extended Filter Specification  */
	uint8_t filter_spec_ex_list_valid;
	/* Must be set to true if filter_spec_ex_list is being passed */
	uint32_t filter_spec_ex_list_len;
	/* Must be set to # of elements in filter_spec_ex_list */
	struct ipa_filter_spec_ex_type_v01
		filter_spec_ex_list[QMI_IPA_MAX_FILTERS_V01];
	/*
	 * List of filter specifications of filters that must be installed in
	 *	the IPAv3.x hardware.
	 *	The driver installing these rules must do so in the same
	 *	order as specified in this list.
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
	/*  Filter Handle List */
	uint8_t filter_handle_list_valid;
	/* Must be set to true if filter_handle_list is being passed */
	uint32_t filter_handle_list_len;
	/* Must be set to # of elements in filter_handle_list */
	struct ipa_filter_rule_identifier_to_handle_map_v01
		filter_handle_list[QMI_IPA_MAX_FILTERS_V01];
	/*
	 * List of handles returned to the control point. Each handle is
	 *	mapped to the rule identifier that was specified in the
	 *	request message. Any further reference to the rule is done
	 *	using the filter handle.
	 */

	/* Optional */
	/*  Rule id List */
	uint8_t rule_id_valid;
	/* Must be set to true if rule_id is being passed */
	uint32_t rule_id_len;
	/* Must be set to # of elements in rule_id */
	uint32_t rule_id[QMI_IPA_MAX_FILTERS_V01];
	/*
	 * List of rule ids returned to the control point.
	 *	Any further reference to the rule is done using the
	 *	filter rule id specified in this list.
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
 * of the installation of the filter rule supplied earlier by the
 * remote driver.
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
	/*  List of Filter Indices */
	uint32_t filter_index_list_len;
	/* Must be set to # of elements in filter_index_list */
	struct ipa_filter_handle_to_index_map_v01
		filter_index_list[QMI_IPA_MAX_FILTERS_V01];
	/*
	 * Provides the list of filter indices and the corresponding
	 *	filter handle. If the installation_status indicates a
	 *	failure, the filter indices must be set to a reserve
	 *	index (255).
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
	 *	on the embedded pipe. This is how we identify the PDN gateway
	 *	to which traffic from the source pipe has to flow.
	 */

	/* Optional */
	/*  Total number of IPv4 filters in the filter index list */
	uint8_t num_ipv4_filters_valid;
	/* Must be set to true if num_ipv4_filters is being passed */
	uint32_t num_ipv4_filters;
	/* Number of IPv4 rules included in filter index list */

	/* Optional */
	/*  Total number of IPv6 filters in the filter index list */
	uint8_t num_ipv6_filters_valid;
	/* Must be set to true if num_ipv6_filters is being passed */
	uint32_t num_ipv6_filters;
	/* Number of IPv6 rules included in filter index list */

	/* Optional */
	/*  Start index on IPv4 filters installed on source pipe */
	uint8_t start_ipv4_filter_idx_valid;
	/* Must be set to true if start_ipv4_filter_idx is being passed */
	uint32_t start_ipv4_filter_idx;
	/* Start index of IPv4 rules in filter index list */

	/* Optional */
	/*  Start index on IPv6 filters installed on source pipe */
	uint8_t start_ipv6_filter_idx_valid;
	/* Must be set to true if start_ipv6_filter_idx is being passed */
	uint32_t start_ipv6_filter_idx;
	/* Start index of IPv6 rules in filter index list */

	/* Optional */
	/*  List of Rule Ids */
	uint8_t rule_id_valid;
	/* Must be set to true if rule_id is being passed */
	uint32_t rule_id_len;
	/* Must be set to # of elements in rule_id */
	uint32_t rule_id[QMI_IPA_MAX_FILTERS_V01];
	/*
	 * Provides the list of Rule Ids of rules added in IPA on the given
	 *	source pipe index. If the install_status TLV indicates a
	 *	failure, the Rule Ids in this list must be set to a reserved
	 *	index (255).
	 */

	/* Optional */
	/*	List of destination pipe IDs. */
	uint8_t dst_pipe_id_valid;
	/* Must be set to true if dst_pipe_id is being passed. */
	uint32_t dst_pipe_id_len;
	/* Must be set to # of elements in dst_pipe_id. */
	uint32_t dst_pipe_id[QMI_IPA_MAX_CLIENT_DST_PIPES_V01];
	/* Provides the list of destination pipe IDs for a source pipe. */

};  /* Message */

/* Response Message; This is the message that is exchanged between the
 * control point and the service in order to notify the remote driver
 * of the installation of the filter rule supplied earlier by the
 * remote driver.
 */
struct ipa_fltr_installed_notif_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/*	Standard response type */
};  /* Message */

/* Request Message; Notifies the remote driver of the need to clear the data
 * path to prevent the IPA from being blocked at the head of the processing
 * pipeline
 */
struct ipa_enable_force_clear_datapath_req_msg_v01 {
	/* Mandatory */
	/*  Pipe Mask */
	uint32_t source_pipe_bitmask;
	/* Set of consumer (source) pipes that must be clear of
	 * active data transfers.
	 */

	/* Mandatory */
	/* Request ID */
	uint32_t request_id;
	/* Identifies the ID of the request that is sent to the server
	 * The same request ID is used in the message to remove the force_clear
	 * request. The server is expected to keep track of the request ID and
	 * the source_pipe_bitmask so that it can revert as needed
	 */

	/* Optional */
	/*  Source Throttle State */
	uint8_t throttle_source_valid;
	/* Must be set to true if throttle_source is being passed */
	uint8_t throttle_source;
	/*  Specifies whether the server is to throttle the data from
	 *	these consumer (source) pipes after clearing the exisiting
	 *	data present in the IPA that were pulled from these pipes
	 *	The server is expected to put all the source pipes in the
	 *	source_pipe_bitmask in the same state
	 */
};  /* Message */

/* Response Message; Notifies the remote driver of the need to clear the
 * data path to prevent the IPA from being blocked at the head of the
 * processing pipeline
 */
struct ipa_enable_force_clear_datapath_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type */
};  /* Message */

/* Request Message; Notifies the remote driver that the forceful clearing
 * of the data path can be lifted
 */
struct ipa_disable_force_clear_datapath_req_msg_v01 {
	/* Mandatory */
	/* Request ID */
	uint32_t request_id;
	/* Identifies the request that was sent to the server to
	 * forcibly clear the data path. This request simply undoes
	 * the operation done in that request
	 */
};  /* Message */

/* Response Message; Notifies the remote driver that the forceful clearing
 * of the data path can be lifted
 */
struct ipa_disable_force_clear_datapath_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type */
};  /* Message */

enum ipa_peripheral_speed_enum_v01 {
	IPA_PERIPHERAL_SPEED_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum.  Do not change or use */
	QMI_IPA_PER_USB_FS_V01 = 1,
	/*  Full-speed USB connection */
	QMI_IPA_PER_USB_HS_V01 = 2,
	/*  High-speed USB connection */
	QMI_IPA_PER_USB_SS_V01 = 3,
	/*  Super-speed USB connection */
	QMI_IPA_PER_WLAN_V01 = 4,
	/*  WLAN connection */
	IPA_PERIPHERAL_SPEED_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use*/
};

enum ipa_pipe_mode_enum_v01 {
	IPA_PIPE_MODE_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum.  Do not change or use */
	QMI_IPA_PIPE_MODE_HW_V01 = 1,
	/*  Pipe is connected with a hardware block */
	QMI_IPA_PIPE_MODE_SW_V01 = 2,
	/*  Pipe is controlled by the software */
	IPA_PIPE_MODE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use */
};

enum ipa_peripheral_type_enum_v01 {
	IPA_PERIPHERAL_TYPE_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum.  Do not change or use */
	QMI_IPA_PERIPHERAL_USB_V01 = 1,
	/*  Specifies a USB peripheral */
	QMI_IPA_PERIPHERAL_HSIC_V01 = 2,
	/*  Specifies an HSIC peripheral */
	QMI_IPA_PERIPHERAL_PCIE_V01 = 3,
	/*  Specifies a PCIe	peripheral */
	IPA_PERIPHERAL_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use */
};

struct ipa_config_req_msg_v01 {
	/* Optional */
	/*  Peripheral Type */
	uint8_t peripheral_type_valid;
	/* Must be set to true if peripheral_type is being passed */
	enum ipa_peripheral_type_enum_v01 peripheral_type;
	/* Informs the remote driver about the perhipheral for
	 * which this configuration information is relevant. Values:
	 *	- QMI_IPA_PERIPHERAL_USB (1) -- Specifies a USB peripheral
	 *	- QMI_IPA_PERIPHERAL_HSIC(2) -- Specifies an HSIC peripheral
	 *	- QMI_IPA_PERIPHERAL_PCIE(3) -- Specifies a PCIe peripheral
	 */

	/* Optional */
	/*  HW Deaggregation Support */
	uint8_t hw_deaggr_supported_valid;
	/* Must be set to true if hw_deaggr_supported is being passed */
	uint8_t hw_deaggr_supported;
	/* Informs the remote driver whether the local IPA driver
	 * allows de-aggregation to be performed in the hardware
	 */

	/* Optional */
	/*  Maximum Aggregation Frame Size */
	uint8_t max_aggr_frame_size_valid;
	/* Must be set to true if max_aggr_frame_size is being passed */
	uint32_t max_aggr_frame_size;
	/* Specifies the maximum size of the aggregated frame that
	 * the remote driver can expect from this execution environment
	 *	- Valid range: 128 bytes to 32768 bytes
	 */

	/* Optional */
	/*  IPA Ingress Pipe Mode */
	uint8_t ipa_ingress_pipe_mode_valid;
	/* Must be set to true if ipa_ingress_pipe_mode is being passed */

	enum ipa_pipe_mode_enum_v01 ipa_ingress_pipe_mode;
	/* Indicates to the remote driver if the ingress pipe into the
	 *	IPA is in direct connection with another hardware block or
	 *	if the producer of data to this ingress pipe is a software
	 *  module. Values:
	 *	-QMI_IPA_PIPE_MODE_HW(1) --Pipe is connected with hardware block
	 *	-QMI_IPA_PIPE_MODE_SW(2) --Pipe is controlled by the software
	 */

	/* Optional */
	/*  Peripheral Speed Info */
	uint8_t peripheral_speed_info_valid;
	/* Must be set to true if peripheral_speed_info is being passed */

	enum ipa_peripheral_speed_enum_v01 peripheral_speed_info;
	/* Indicates the speed that the peripheral connected to the IPA supports
	 * Values:
	 *	- QMI_IPA_PER_USB_FS (1) --  Full-speed USB connection
	 *	- QMI_IPA_PER_USB_HS (2) --  High-speed USB connection
	 *	- QMI_IPA_PER_USB_SS (3) --  Super-speed USB connection
	 *  - QMI_IPA_PER_WLAN   (4) --  WLAN connection
	 */

	/* Optional */
	/*  Downlink Accumulation Time limit */
	uint8_t dl_accumulation_time_limit_valid;
	/* Must be set to true if dl_accumulation_time_limit is being passed */
	uint32_t dl_accumulation_time_limit;
	/* Informs the remote driver about the time for which data
	 * is accumulated in the downlink direction before it is pushed into the
	 * IPA (downlink is with respect to the WWAN air interface)
	 * - Units: milliseconds
	 * - Maximum value: 255
	 */

	/* Optional */
	/*  Downlink Accumulation Packet limit */
	uint8_t dl_accumulation_pkt_limit_valid;
	/* Must be set to true if dl_accumulation_pkt_limit is being passed */
	uint32_t dl_accumulation_pkt_limit;
	/* Informs the remote driver about the number of packets
	 * that are to be accumulated in the downlink direction before it is
	 * pushed into the IPA - Maximum value: 1023
	 */

	/* Optional */
	/*  Downlink Accumulation Byte Limit */
	uint8_t dl_accumulation_byte_limit_valid;
	/* Must be set to true if dl_accumulation_byte_limit is being passed */
	uint32_t dl_accumulation_byte_limit;
	/* Inform the remote driver about the number of bytes
	 * that are to be accumulated in the downlink direction before it
	 * is pushed into the IPA - Maximum value: TBD
	 */

	/* Optional */
	/*  Uplink Accumulation Time Limit */
	uint8_t ul_accumulation_time_limit_valid;
	/* Must be set to true if ul_accumulation_time_limit is being passed */
	uint32_t ul_accumulation_time_limit;
	/* Inform thes remote driver about the time for which data
	 * is to be accumulated in the uplink direction before it is pushed into
	 * the IPA (downlink is with respect to the WWAN air interface).
	 * - Units: milliseconds
	 * - Maximum value: 255
	 */

	/* Optional */
	/*  HW Control Flags */
	uint8_t hw_control_flags_valid;
	/* Must be set to true if hw_control_flags is being passed */
	uint32_t hw_control_flags;
	/* Informs the remote driver about the hardware control flags:
	 *	- Bit 0: IPA_HW_FLAG_HALT_SYSTEM_ON_NON_TERMINAL_FAILURE --
	 *	Indicates to the hardware that it must not continue with
	 *	any subsequent operation even if the failure is not terminal
	 *	- Bit 1: IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_ERORR --
	 *	Indicates to the hardware that it is not required to report
	 *	channel errors to the host.
	 *	- Bit 2: IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_WAKE_UP --
	 *	Indicates to the hardware that it is not required to generate
	 *	wake-up events to the host.
	 *	- Bit 4: IPA_HW_FLAG_WORK_OVER_DDR --
	 *	Indicates to the hardware that it is accessing addresses in
	 *  the DDR and not over PCIe
	 *	- Bit 5: IPA_HW_FLAG_INTERRUPT_MODE_CTRL_FLAG --
	 *	Indicates whether the device must
	 *	raise an event to let the host know that it is going into an
	 *	interrupt mode (no longer polling for data/buffer availability)
	 */

	/* Optional */
	/*  Uplink MSI Event Threshold */
	uint8_t ul_msi_event_threshold_valid;
	/* Must be set to true if ul_msi_event_threshold is being passed */
	uint32_t ul_msi_event_threshold;
	/* Informs the remote driver about the threshold that will
	 * cause an interrupt (MSI) to be fired to the host. This ensures
	 * that the remote driver does not accumulate an excesive number of
	 * events before firing an interrupt.
	 * This threshold is applicable for data moved in the UL direction.
	 * - Maximum value: 65535
	 */

	/* Optional */
	/*  Downlink MSI Event Threshold */
	uint8_t dl_msi_event_threshold_valid;
	/* Must be set to true if dl_msi_event_threshold is being passed */
	uint32_t dl_msi_event_threshold;
	/* Informs the remote driver about the threshold that will
	 * cause an interrupt (MSI) to be fired to the host. This ensures
	 * that the remote driver does not accumulate an excesive number of
	 * events before firing an interrupt
	 * This threshold is applicable for data that is moved in the
	 * DL direction - Maximum value: 65535
	 */

	/* Optional */
	/*  Uplink Fifo Size */
	uint8_t ul_fifo_size_valid;
	/* Must be set to true if ul_fifo_size is being passed */
	uint32_t ul_fifo_size;
	/*
	 * Informs the remote driver about the total Uplink xDCI
	 *	buffer size that holds the complete aggregated frame
	 *	or BAM data fifo size of the peripheral channel/pipe(in Bytes).
	 *	This deprecates the max_aggr_frame_size field. This TLV
	 *	deprecates max_aggr_frame_size TLV from version 1.9 onwards
	 *	and the max_aggr_frame_size TLV will be ignored in the presence
	 *	of this TLV.
	 */

	/* Optional */
	/*  Downlink Fifo Size */
	uint8_t dl_fifo_size_valid;
	/* Must be set to true if dl_fifo_size is being passed */
	uint32_t dl_fifo_size;
	/*
	 * Informs the remote driver about the total Downlink xDCI buffering
	 *	capacity or BAM data fifo size of the peripheral channel/pipe.
	 *	(In Bytes). dl_fifo_size = n * dl_buf_size. This deprecates the
	 *	max_aggr_frame_size field. If this value is set
	 *	max_aggr_frame_size is ignored.
	 */

	/* Optional */
	/*  Downlink Buffer Size */
	uint8_t dl_buf_size_valid;
	/* Must be set to true if dl_buf_size is being passed */
	uint32_t dl_buf_size;
	/* Informs the remote driver about the single xDCI buffer size.
	 * This is applicable only in GSI mode(in Bytes).\n
	 */
};  /* Message */

/* Response Message; Notifies the remote driver of the configuration
 * information
 */
struct ipa_config_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/**<   Standard response type.*/
}; /* Message */

enum ipa_stats_type_enum_v01 {
	IPA_STATS_TYPE_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum.  Do not change or use */
	QMI_IPA_STATS_TYPE_INVALID_V01 = 0,
	/* Invalid stats type identifier */
	QMI_IPA_STATS_TYPE_PIPE_V01 = 1,
	/* Pipe stats type */
	QMI_IPA_STATS_TYPE_FILTER_RULES_V01 = 2,
	/* Filter rule stats type */
	IPA_STATS_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use */
};

struct ipa_pipe_stats_info_type_v01 {
	uint32_t pipe_index;
	/* Pipe index for statistics to be retrieved. */

	uint64_t num_ipv4_packets;
	/* Accumulated number of IPv4 packets over this pipe. */

	uint64_t num_ipv4_bytes;
	/* Accumulated number of IPv4 bytes over this pipe. */

	uint64_t num_ipv6_packets;
	/* Accumulated number of IPv6 packets over this pipe. */

	uint64_t num_ipv6_bytes;
	/* Accumulated number of IPv6 bytes over this pipe. */
};

struct ipa_stats_type_filter_rule_v01 {
	uint32_t filter_rule_index;
	/* Filter rule index for statistics to be retrieved. */

	uint64_t num_packets;
	/* Accumulated number of packets over this filter rule. */
};

/* Request Message; Retrieve the data statistics collected on modem
 * IPA driver.
 */
struct ipa_get_data_stats_req_msg_v01 {
	/* Mandatory */
	/*  Stats Type  */
	enum ipa_stats_type_enum_v01 ipa_stats_type;
	/* Indicates the type of statistics to be retrieved. */

	/* Optional */
	/* Reset Statistics */
	uint8_t reset_stats_valid;
	/* Must be set to true if reset_stats is being passed */
	uint8_t reset_stats;
	/* Option to reset the specific type of data statistics
	 * currently collected.
	 */
};  /* Message */

/* Response Message; Retrieve the data statistics collected
 * on modem IPA driver.
 */
struct ipa_get_data_stats_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type. */

	/* Optional */
	/*  Stats Type  */
	uint8_t ipa_stats_type_valid;
	/* Must be set to true if ipa_stats_type is passed */
	enum ipa_stats_type_enum_v01 ipa_stats_type;
	/* Indicates the type of statistics that are retrieved. */

	/* Optional */
	/*  Uplink Source Pipe Statistics List */
	uint8_t ul_src_pipe_stats_list_valid;
	/* Must be set to true if ul_src_pipe_stats_list is being passed */
	uint32_t ul_src_pipe_stats_list_len;
	/* Must be set to # of elements in ul_src_pipe_stats_list */
	struct ipa_pipe_stats_info_type_v01
		ul_src_pipe_stats_list[QMI_IPA_MAX_PIPES_V01];
	/* List of all Uplink pipe statistics that are retrieved. */

	/* Optional */
	/*  Downlink Destination Pipe Statistics List */
	uint8_t dl_dst_pipe_stats_list_valid;
	/* Must be set to true if dl_dst_pipe_stats_list is being passed */
	uint32_t dl_dst_pipe_stats_list_len;
	/* Must be set to # of elements in dl_dst_pipe_stats_list */
	struct ipa_pipe_stats_info_type_v01
		dl_dst_pipe_stats_list[QMI_IPA_MAX_PIPES_V01];
	/* List of all Downlink pipe statistics that are retrieved. */

	/* Optional */
	/*  Downlink Filter Rule Stats List */
	uint8_t dl_filter_rule_stats_list_valid;
	/* Must be set to true if dl_filter_rule_stats_list is being passed */
	uint32_t dl_filter_rule_stats_list_len;
	/* Must be set to # of elements in dl_filter_rule_stats_list */
	struct ipa_stats_type_filter_rule_v01
		dl_filter_rule_stats_list[QMI_IPA_MAX_FILTERS_V01];
	/* List of all Downlink filter rule statistics retrieved. */
};  /* Message */

struct ipa_apn_data_stats_info_type_v01 {
	uint32_t mux_id;
	/* Indicates the MUX ID associated with the APN for which the data
	 * usage statistics is queried
	 */

	uint64_t num_ul_packets;
	/* Accumulated number of uplink packets corresponding to
	 * this Mux ID
	 */

	uint64_t num_ul_bytes;
	/* Accumulated number of uplink bytes corresponding to
	 * this Mux ID
	 */

	uint64_t num_dl_packets;
	/* Accumulated number of downlink packets corresponding
	 * to this Mux ID
	 */

	uint64_t num_dl_bytes;
	/* Accumulated number of downlink bytes corresponding to
	 * this Mux ID
	 */
};  /* Type */

/* Request Message; Retrieve the APN data statistics collected from modem */
struct ipa_get_apn_data_stats_req_msg_v01 {
	/* Optional */
	/*  Mux ID List */
	uint8_t mux_id_list_valid;
	/* Must be set to true if mux_id_list is being passed */
	uint32_t mux_id_list_len;
	/* Must be set to # of elements in mux_id_list */
	uint32_t mux_id_list[QMI_IPA_MAX_APN_V01];
	/* The list of MUX IDs associated with APNs for which the data usage
	 * statistics is being retrieved
	 */
};  /* Message */

/* Response Message; Retrieve the APN data statistics collected from modem */
struct ipa_get_apn_data_stats_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type.*/

	/* Optional */
	/* APN Data Statistics List */
	uint8_t apn_data_stats_list_valid;
	/* Must be set to true if apn_data_stats_list is being passed */
	uint32_t apn_data_stats_list_len;
	/* Must be set to # of elements in apn_data_stats_list */
	struct ipa_apn_data_stats_info_type_v01
		apn_data_stats_list[QMI_IPA_MAX_APN_V01];
	/* List of APN data retrieved as per request on mux_id.
	 * For now, only one APN monitoring is supported on modem driver.
	 * Making this as list for expandability to support more APNs in future.
	 */
};  /* Message */

struct ipa_data_usage_quota_info_type_v01 {
	uint32_t mux_id;
	/* Indicates the MUX ID associated with the APN for which the data usage
	 * quota needs to be set
	 */

	uint64_t num_Mbytes;
	/* Number of Mega-bytes of quota value to be set on this APN associated
	 * with this Mux ID.
	 */
};  /* Type */

/* Request Message; Master driver sets a data usage quota value on
 * modem driver
 */
struct ipa_set_data_usage_quota_req_msg_v01 {
	/* Optional */
	/* APN Quota List */
	uint8_t apn_quota_list_valid;
	/* Must be set to true if apn_quota_list is being passed */
	uint32_t apn_quota_list_len;
	/* Must be set to # of elements in apn_quota_list */
	struct ipa_data_usage_quota_info_type_v01
		apn_quota_list[QMI_IPA_MAX_APN_V01];
	/* The list of APNs on which a data usage quota to be set on modem
	 * driver. For now, only one APN monitoring is supported on modem
	 * driver. Making this as list for expandability to support more
	 * APNs in future.
	 */
};  /* Message */

/* Response Message; Master driver sets a data usage on modem driver. */
struct ipa_set_data_usage_quota_resp_msg_v01 {
	/* Mandatory */
	/* Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type.*/
};  /* Message */

/* Indication Message; Modem driver sends this indication to master
 * driver when the data usage quota is reached
 */
struct ipa_data_usage_quota_reached_ind_msg_v01 {
	/* Mandatory */
	/*  APN Quota List */
	struct ipa_data_usage_quota_info_type_v01 apn;
	/* This message indicates which APN has the previously set quota
	 * reached. For now, only one APN monitoring is supported on modem
	 * driver.
	 */
};  /* Message */

/* Request Message; Master driver request modem driver to terminate
 * the current data usage quota monitoring session.
 */
struct ipa_stop_data_usage_quota_req_msg_v01 {
	/* This element is a placeholder to prevent the declaration of
	 *  an empty struct.  DO NOT USE THIS FIELD UNDER ANY CIRCUMSTANCE
	 */
	char __placeholder;
};  /* Message */

/* Response Message; Master driver request modem driver to terminate
 * the current quota monitoring session.
 */
struct ipa_stop_data_usage_quota_resp_msg_v01 {
	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/**<   Standard response type.*/
};  /* Message */

/* Request Message; Request from Modem IPA driver to set DPL peripheral pipe */
struct ipa_install_fltr_rule_req_ex_msg_v01 {

	/* Optional */
	/*  Extended Filter Specification  */
	uint8_t filter_spec_ex_list_valid;
	uint32_t filter_spec_ex_list_len;
	struct ipa_filter_spec_ex_type_v01
		filter_spec_ex_list[QMI_IPA_MAX_FILTERS_EX_V01];
	/* List of filter specifications of filters that must be installed in
	 * the IPAv3.x hardware.
	 * The driver installing these rules must do so in the same order as
	 * specified in this list.
	 */

	/* Optional */
	/* Pipe Index to Install Rule */
	uint8_t source_pipe_index_valid;
	uint32_t source_pipe_index;
	/* Pipe index to install the filter rule.
	 * The requester may not always know the pipe indices. If not specified,
	 * the receiver must install this rule on all pipes that it controls,
	 * through which data may be fed into the IPA.
	 */

	/* Optional */
	/* Total Number of IPv4 Filters in the Filter Spec List */
	uint8_t num_ipv4_filters_valid;
	uint32_t num_ipv4_filters;
	/* Number of IPv4 rules included in the filter specification list. */

	/* Optional */
	/* Total Number of IPv6 Filters in the Filter Spec List */
	uint8_t num_ipv6_filters_valid;
	uint32_t num_ipv6_filters;
	/* Number of IPv6 rules included in the filter specification list. */

	/* Optional */
	/* List of XLAT Filter Indices in the Filter Spec List */
	uint8_t xlat_filter_indices_list_valid;
	uint32_t xlat_filter_indices_list_len;
	uint32_t xlat_filter_indices_list[QMI_IPA_MAX_FILTERS_EX_V01];
	/* List of XLAT filter indices.
	 * Filter rules at specified indices must be modified by the
	 * receiver if the PDN is XLAT before installing them on the associated
	 * IPA consumer pipe.
	 */
};  /* Message */

/* Response Message; Requests installation of filtering rules in the hardware
 * block on the remote side.
 */
struct ipa_install_fltr_rule_resp_ex_msg_v01 {
	/* Mandatory */
	/* Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type.
	 * Standard response type. Contains the following data members:
	 * - qmi_result_type -- QMI_RESULT_SUCCESS or QMI_RESULT_FAILURE
	 * - qmi_error_type  -- Error code. Possible error code values are
	 *					 described in the error codes
	 *					 section of each message
	 *					 definition.
	 */

	/* Optional */
	/* Rule ID List */
	uint8_t rule_id_valid;
	uint32_t rule_id_len;
	uint32_t rule_id[QMI_IPA_MAX_FILTERS_EX_V01];
	/* List of rule IDs returned to the control point.
	 * Any further reference to the rule is done using the filter rule ID
	 * specified in this list.
	 */
};  /* Message */

/*
 * Request Message; Requests the modem IPA driver to enable or
 * disable collection of per client statistics.
 */
struct ipa_enable_per_client_stats_req_msg_v01 {

	/* Mandatory */
	/* Collect statistics per client; */
	uint8_t enable_per_client_stats;
	/*
	 * Indicates whether to start or stop collecting
	 * per client statistics.
	 */
};  /* Message */

/*
 * Response Message; Requests the modem IPA driver to enable or disable
 * collection of per client statistics.
 */
struct ipa_enable_per_client_stats_resp_msg_v01 {

	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type. */
};  /* Message */

struct ipa_per_client_stats_info_type_v01 {

	uint32_t client_id;
	/*
	 * Id of the client on APPS processor side for which Modem processor
	 * needs to send uplink/downlink statistics.
	 */

	uint32_t src_pipe_id;
	/*
	 * IPA consumer pipe on which client on APPS side sent uplink
	 * data to modem.
	 */

	uint64_t num_ul_ipv4_bytes;
	/*
	 * Accumulated number of uplink IPv4 bytes for a client.
	 */

	uint64_t num_ul_ipv6_bytes;
	/*
	 * Accumulated number of uplink IPv6 bytes for a client.
	 */

	uint64_t num_dl_ipv4_bytes;
	/*
	 * Accumulated number of downlink IPv4 bytes for a client.
	 */

	uint64_t num_dl_ipv6_bytes;
	/*
	 * Accumulated number of downlink IPv6 byes for a client.
	 */


	uint32_t num_ul_ipv4_pkts;
	/*
	 * Accumulated number of uplink IPv4 packets for a client.
	 */

	uint32_t num_ul_ipv6_pkts;
	/*
	 * Accumulated number of uplink IPv6 packets for a client.
	 */

	uint32_t num_dl_ipv4_pkts;
	/*
	 * Accumulated number of downlink IPv4 packets for a client.
	 */

	uint32_t num_dl_ipv6_pkts;
	/*
	 * Accumulated number of downlink IPv6 packets for a client.
	 */
};  /* Type */

/*
 * Request Message; Requests the modem IPA driver to provide statistics
 * for a givenclient.
 */
struct ipa_get_stats_per_client_req_msg_v01 {

	/* Mandatory */
	/*  Client id */
	uint32_t client_id;
	/*
	 * Id of the client on APPS processor side for which Modem processor
	 * needs to send uplink/downlink statistics. if client id is specified
	 * as 0xffffffff, then Q6 will send the stats for all the clients of
	 * the specified source pipe.
	 */

	/* Mandatory */
	/*  Source pipe id */
	uint32_t src_pipe_id;
	/*
	 * IPA consumer pipe on which client on APPS side sent uplink
	 * data to modem. In future, this implementation can be extended
	 * to provide 0xffffffff as the source pipe id, where Q6 will send
	 * the stats of all the clients across all different tethered-pipes.
	 */

	/* Optional */
	/*  Reset client statistics. */
	uint8_t reset_stats_valid;
	/* Must be set to true if reset_stats is being passed. */
	uint8_t reset_stats;
	/*
	 * Option to reset the statistics currently collected by modem for this
	 * particular client.
	 */
};  /* Message */

/*
 * Response Message; Requests the modem IPA driver to provide statistics
 * for a given client.
 */
struct ipa_get_stats_per_client_resp_msg_v01 {

	/* Mandatory */
	/*  Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type. */

	/* Optional */
	/*  Per clients Statistics List */
	uint8_t per_client_stats_list_valid;
	/* Must be set to true if per_client_stats_list is being passed. */
	uint32_t per_client_stats_list_len;
	/* Must be set to # of elements in per_client_stats_list. */
	struct ipa_per_client_stats_info_type_v01
		per_client_stats_list[QMI_IPA_MAX_PER_CLIENTS_V01];
	/*
	 * List of all per client statistics that are retrieved.
	 */
};  /* Message */

struct ipa_ul_firewall_rule_type_v01 {

	enum ipa_ip_type_enum_v01 ip_type;
	/*
	 * IP type for which this rule is applicable.
	 * The driver must identify the filter table (v6 or v4), and this
	 * field is essential for that. Values:
	 * - QMI_IPA_IP_TYPE_INVALID (0) --  Invalid IP type identifier
	 * - QMI_IPA_IP_TYPE_V4 (1) --  IPv4 type
	 * - QMI_IPA_IP_TYPE_V6 (2) --  IPv6 type
	 */

	struct ipa_filter_rule_type_v01 filter_rule;
	/*
	 * Rules in the filter specification. These rules are the
	 * ones that are matched against fields in the packet.
	 * Currently we only send IPv6 whitelist rules to Q6.
	 */
};  /* Type */

/*
 * Request Message; Requestes remote IPA driver to install uplink
 * firewall rules.
 */
struct ipa_configure_ul_firewall_rules_req_msg_v01 {

	/* Optional */
	/*  Uplink Firewall Specification  */
	uint32_t firewall_rules_list_len;
	/* Must be set to # of elements in firewall_rules_list. */
	struct ipa_ul_firewall_rule_type_v01
		firewall_rules_list[QMI_IPA_MAX_UL_FIREWALL_RULES_V01];
	/*
	 * List of uplink firewall specifications of filters that must be
	 * installed.
	 */

	uint32_t mux_id;
	/*
	 * QMAP Mux ID. As a part of the QMAP protocol,
	 * several data calls may be multiplexed over the same physical
	 * transport channel. This identifier is used to identify one
	 * such data call. The maximum value for this identifier is 255.
	 */

	/* Optional */
	uint8_t disable_valid;
	/* Must be set to true if enable is being passed. */
	uint8_t disable;
	/*
	 * Indicates whether uplink firewall needs to be enabled or disabled.
	 */

	/* Optional */
	uint8_t are_blacklist_filters_valid;
	/* Must be set to true if are_blacklist_filters is being passed. */
	uint8_t are_blacklist_filters;
	/*
	 * Indicates whether the filters received as part of this message are
	 * blacklist filters. i.e. drop uplink packets matching these rules.
	 */
};  /* Message */

/*
 * Response Message; Requestes remote IPA driver to install
 * uplink firewall rules.
 */
struct ipa_configure_ul_firewall_rules_resp_msg_v01 {

	/* Mandatory */
	/* Result Code */
	struct ipa_qmi_response_type_v01 resp;
	/*
	 * Standard response type.
	 * Standard response type. Contains the following data members:
	 * qmi_result_type -- QMI_RESULT_SUCCESS or QMI_RESULT_FAILURE
	 * qmi_error_type  -- Error code. Possible error code values are
	 * described in the error codes section of each message definition.
	 */
};  /* Message */

enum ipa_ul_firewall_status_enum_v01 {
	IPA_UL_FIREWALL_STATUS_ENUM_MIN_ENUM_VAL_V01 = -2147483647,
	/* To force a 32 bit signed enum.  Do not change or use*/
	QMI_IPA_UL_FIREWALL_STATUS_SUCCESS_V01 = 0,
	/* Indicates that the uplink firewall rules
	 * are configured successfully.
	 */
	QMI_IPA_UL_FIREWALL_STATUS_FAILURE_V01 = 1,
	/* Indicates that the uplink firewall rules
	 * are not configured successfully.
	 */
	IPA_UL_FIREWALL_STATUS_ENUM_MAX_ENUM_VAL_V01 = 2147483647
	/* To force a 32 bit signed enum.  Do not change or use*/
};

struct ipa_ul_firewall_config_result_type_v01 {

	enum ipa_ul_firewall_status_enum_v01 is_success;
	/*
	 * Indicates whether the uplink firewall rules are configured
	 * successfully.
	 */

	uint32_t mux_id;
	/*
	 * QMAP Mux ID. As a part of the QMAP protocol,
	 * several data calls may be multiplexed over the same physical
	 * transport channel. This identifier is used to identify one
	 * such data call. The maximum value for this identifier is 255.
	 */
};

/*
 * Indication Message; Requestes remote IPA driver to install
 * uplink firewall rules.
 */
struct ipa_configure_ul_firewall_rules_ind_msg_v01 {

	 struct ipa_ul_firewall_config_result_type_v01 result;
};  /* Message */


struct ipa_mhi_ch_init_info_type_v01 {
	uint8_t ch_id;
	/* Remote MHI channel ID */

	uint8_t er_id;
	/* Remote MHI Event ring ID */

	uint32_t ch_doorbell_addr;
	/* TR Channel Doorbell addr */

	uint32_t er_doorbell_addr;
	/* Event ring Doorbell addr */

	uint32_t direction_type;
	/* Direction type */
};

struct ipa_mhi_smmu_info_type_v01 {
	uint64_t iova_ctl_base_addr;
	/* IOVA mapped Control Region base address */

	uint64_t iova_ctl_size;
	/* IOVA Control region size */

	uint64_t iova_data_base_addr;
	/* IOVA mapped Data Region base address */

	uint64_t iova_data_size;
	/* IOVA Data Region size */
};

struct ipa_mhi_ready_indication_msg_v01 {
	/* Mandatory */
	uint32_t ch_info_arr_len;
	/* Must be set to # of elements in ch_info_arr. */
	struct ipa_mhi_ch_init_info_type_v01
		ch_info_arr[QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01];
	/* Channel Information array */

	/* Mandatory */
	uint8_t smmu_info_valid;
	/* Must be set to true if smmu_info is being passed. */
	struct ipa_mhi_smmu_info_type_v01 smmu_info;
	/* SMMU enabled indication */
};
#define IPA_MHI_READY_INDICATION_MSG_V01_MAX_MSG_LEN 123

struct ipa_mhi_mem_addr_info_type_v01 {
	uint64_t pa;
	/* Memory region start physical addr */

	uint64_t iova;
	/* Memory region start iova mapped addr */

	uint64_t size;
	/* Memory region size */
};

enum ipa_mhi_brst_mode_enum_v01 {
	IPA_MHI_BRST_MODE_ENUM_MIN_VAL_V01 = IPA_INT_MIN,

	QMI_IPA_BURST_MODE_DEFAULT_V01 = 0,
	/*
	 * Default - burst mode enabled for hardware channels,
	 * disabled for software channels
	 */

	QMI_IPA_BURST_MODE_ENABLED_V01 = 1,
	/* Burst mode is enabled for this channel */

	QMI_IPA_BURST_MODE_DISABLED_V01 = 2,
	/* Burst mode is disabled for this channel */

	IPA_MHI_BRST_MODE_ENUM_MAX_VAL_V01 = IPA_INT_MAX,
};

struct ipa_mhi_tr_info_type_v01 {
	uint8_t ch_id;
	/* TR Channel ID */

	uint16_t poll_cfg;
	/*
	 * Poll Configuration - Default or timer to poll the
	 * MHI context in milliseconds
	 */

	enum ipa_mhi_brst_mode_enum_v01 brst_mode_type;
	/* Burst mode configuration */

	uint64_t ring_iova;
	/* IOVA mapped ring base address */

	uint64_t ring_len;
	/* Ring Length in bytes */

	uint64_t rp;
	/* IOVA mapped Read pointer address */

	uint64_t wp;
	/* IOVA mapped write pointer address */
};

struct ipa_mhi_er_info_type_v01 {
	uint8_t er_id;
	/* Event ring ID */

	uint32_t intmod_cycles;
	/* Interrupt moderation cycles */

	uint32_t intmod_count;
	/* Interrupt moderation count */

	uint32_t msi_addr;
	/* IOVA mapped MSI address for this ER */

	uint64_t ring_iova;
	/* IOVA mapped ring base address */

	uint64_t ring_len;
	/* Ring length in bytes */

	uint64_t rp;
	/* IOVA mapped Read pointer address */

	uint64_t wp;
	/* IOVA mapped Write pointer address */
};

struct ipa_mhi_alloc_channel_req_msg_v01 {
	/* Mandatory */
	uint32_t tr_info_arr_len;
	/* Must be set to # of elements in tr_info_arr. */
	struct ipa_mhi_tr_info_type_v01
		tr_info_arr[QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01];
	/* Array of TR context information for Remote MHI channels */

	/* Mandatory */
	uint32_t er_info_arr_len;
	/* Must be set to # of elements in er_info_arr. */
	struct ipa_mhi_er_info_type_v01
		er_info_arr[QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01];
	/* Array of ER context information for Remote MHI channels */

	/* Mandatory */
	uint32_t ctrl_addr_map_info_len;
	/* Must be set to # of elements in ctrl_addr_map_info. */

	struct ipa_mhi_mem_addr_info_type_v01
	ctrl_addr_map_info[QMI_IPA_REMOTE_MHI_MEMORY_MAPPING_NUM_MAX_V01];
	/*
	 * List of PA-IOVA address mappings for control regions
	 * used by Modem
	 */

	/* Mandatory */
	uint32_t data_addr_map_info_len;
	/* Must be set to # of elements in data_addr_map_info. */
	struct ipa_mhi_mem_addr_info_type_v01
	data_addr_map_info[QMI_IPA_REMOTE_MHI_MEMORY_MAPPING_NUM_MAX_V01];
	/* List of PA-IOVA address mappings for data regions used by Modem */
};
#define IPA_MHI_ALLOC_CHANNEL_REQ_MSG_V01_MAX_MSG_LEN 808

struct ipa_mhi_ch_alloc_resp_type_v01 {
	uint8_t ch_id;
	/* Remote MHI channel ID */

	uint8_t is_success;
	/* Channel Allocation Status */
};

struct ipa_mhi_alloc_channel_resp_msg_v01 {
	/* Mandatory */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type. Contains the following data members:
	 * - qmi_result_type -- QMI_RESULT_SUCCESS or QMI_RESULT_FAILURE
	 * - qmi_error_type  -- Error code. Possible error code values
	 *			are described in the error codes section
	 *			of each message definition.
	 */

	/* Optional */
	uint8_t alloc_resp_arr_valid;
	/* Must be set to true if alloc_resp_arr is being passed. */
	uint32_t alloc_resp_arr_len;
	/* Must be set to # of elements in alloc_resp_arr. */
	struct ipa_mhi_ch_alloc_resp_type_v01
		alloc_resp_arr[QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01];
	/* MHI channel allocation response array */
};
#define IPA_MHI_ALLOC_CHANNEL_RESP_MSG_V01_MAX_MSG_LEN 23

struct ipa_mhi_clk_vote_req_msg_v01 {
	/* Mandatory */
	uint8_t mhi_vote;
	/*
	 * MHI vote request
	 * TRUE  - ON
	 * FALSE - OFF
	 */
};
#define IPA_MHI_CLK_VOTE_REQ_MSG_V01_MAX_MSG_LEN 4

struct ipa_mhi_clk_vote_resp_msg_v01 {
	/* Mandatory */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type. Contains the following data members:
	 * - qmi_result_type -- QMI_RESULT_SUCCESS or QMI_RESULT_FAILURE
	 * - qmi_error_type  -- Error code. Possible error code values
	 *			are described in the error codes section
	 *			of each message definition.
	 */
};
#define IPA_MHI_CLK_VOTE_RESP_MSG_V01_MAX_MSG_LEN 7

struct ipa_mhi_cleanup_req_msg_v01 {
	/* Optional */
	uint8_t cleanup_valid;
	/* Must be set to true if cleanup is being passed. */
	uint8_t cleanup;
	/*
	 * a Flag to indicate the type of action
	 * 1 - Cleanup Request
	 */
};
#define IPA_MHI_CLEANUP_REQ_MSG_V01_MAX_MSG_LEN 4

struct ipa_mhi_cleanup_resp_msg_v01 {
	/* Mandatory */
	struct ipa_qmi_response_type_v01 resp;
	/* Standard response type. Contains the following data members:
	 * - qmi_result_type -- QMI_RESULT_SUCCESS or QMI_RESULT_FAILURE
	 * - qmi_error_type  -- Error code. Possible error code values
	 *			are described in the error codes section
	 *			of each message definition.
	 */
};
#define IPA_MHI_CLEANUP_RESP_MSG_V01_MAX_MSG_LEN 7

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
#define QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_V01 0x0025
#define QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_RESP_V01 0x0025
#define QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_V01 0x0026
#define QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_RESP_V01 0x0026
#define QMI_IPA_CONFIG_REQ_V01 0x0027
#define QMI_IPA_CONFIG_RESP_V01 0x0027
#define QMI_IPA_DISABLE_LINK_LOW_PWR_STATE_REQ_V01 0x0028
#define QMI_IPA_DISABLE_LINK_LOW_PWR_STATE_RESP_V01 0x0028
#define QMI_IPA_ENABLE_LINK_LOW_PWR_STATE_REQ_V01 0x0029
#define QMI_IPA_ENABLE_LINK_LOW_PWR_STATE_RESP_V01 0x0029
#define QMI_IPA_GET_DATA_STATS_REQ_V01 0x0030
#define QMI_IPA_GET_DATA_STATS_RESP_V01 0x0030
#define QMI_IPA_GET_APN_DATA_STATS_REQ_V01 0x0031
#define QMI_IPA_GET_APN_DATA_STATS_RESP_V01 0x0031
#define QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_V01 0x0032
#define QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_V01 0x0032
#define QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_V01 0x0033
#define QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_V01 0x0034
#define QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_V01 0x0034
#define QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_V01 0x0035
#define QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_V01 0x0035
#define QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_V01 0x0037
#define QMI_IPA_INSTALL_FILTER_RULE_EX_RESP_V01 0x0037
#define QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_V01 0x0038
#define QMI_IPA_ENABLE_PER_CLIENT_STATS_RESP_V01 0x0038
#define QMI_IPA_GET_STATS_PER_CLIENT_REQ_V01 0x0039
#define QMI_IPA_GET_STATS_PER_CLIENT_RESP_V01 0x0039
#define QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_V01 0x003A
#define QMI_IPA_INSTALL_UL_FIREWALL_RULES_RESP_V01 0x003A
#define QMI_IPA_INSTALL_UL_FIREWALL_RULES_IND_V01 0x003A
#define QMI_IPA_MHI_CLK_VOTE_REQ_V01 0x003B
#define QMI_IPA_MHI_CLK_VOTE_RESP_V01 0x003B
#define QMI_IPA_MHI_READY_IND_V01 0x003C
#define QMI_IPA_MHI_ALLOC_CHANNEL_REQ_V01 0x003D
#define QMI_IPA_MHI_ALLOC_CHANNEL_RESP_V01 0x003D
#define QMI_IPA_MHI_CLEANUP_REQ_V01 0x003E
#define QMI_IPA_MHI_CLEANUP_RESP_V01 0x003E

/* add for max length*/
#define QMI_IPA_INIT_MODEM_DRIVER_REQ_MAX_MSG_LEN_V01 162
#define QMI_IPA_INIT_MODEM_DRIVER_RESP_MAX_MSG_LEN_V01 25
#define QMI_IPA_INDICATION_REGISTER_REQ_MAX_MSG_LEN_V01 8
#define QMI_IPA_INDICATION_REGISTER_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_INSTALL_FILTER_RULE_REQ_MAX_MSG_LEN_V01 22369
#define QMI_IPA_INSTALL_FILTER_RULE_RESP_MAX_MSG_LEN_V01 783
#define QMI_IPA_FILTER_INSTALLED_NOTIF_REQ_MAX_MSG_LEN_V01 870
#define QMI_IPA_FILTER_INSTALLED_NOTIF_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_MASTER_DRIVER_INIT_COMPLETE_IND_MAX_MSG_LEN_V01 7
#define QMI_IPA_DATA_USAGE_QUOTA_REACHED_IND_MAX_MSG_LEN_V01 15


#define QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_REQ_MAX_MSG_LEN_V01 18
#define QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_REQ_MAX_MSG_LEN_V01 7
#define QMI_IPA_ENABLE_FORCE_CLEAR_DATAPATH_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_DISABLE_FORCE_CLEAR_DATAPATH_RESP_MAX_MSG_LEN_V01 7


#define QMI_IPA_CONFIG_REQ_MAX_MSG_LEN_V01 102
#define QMI_IPA_CONFIG_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_DISABLE_LINK_LOW_PWR_STATE_REQ_MAX_MSG_LEN_V01 18
#define QMI_IPA_DISABLE_LINK_LOW_PWR_STATE_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_ENABLE_LINK_LOW_PWR_STATE_REQ_MAX_MSG_LEN_V01 7
#define QMI_IPA_ENABLE_LINK_LOW_PWR_STATE_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_GET_DATA_STATS_REQ_MAX_MSG_LEN_V01 11
#define QMI_IPA_GET_DATA_STATS_RESP_MAX_MSG_LEN_V01 2234
#define QMI_IPA_GET_APN_DATA_STATS_REQ_MAX_MSG_LEN_V01 36
#define QMI_IPA_GET_APN_DATA_STATS_RESP_MAX_MSG_LEN_V01 299
#define QMI_IPA_SET_DATA_USAGE_QUOTA_REQ_MAX_MSG_LEN_V01 100
#define QMI_IPA_SET_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_STOP_DATA_USAGE_QUOTA_REQ_MAX_MSG_LEN_V01 0
#define QMI_IPA_STOP_DATA_USAGE_QUOTA_RESP_MAX_MSG_LEN_V01 7

#define QMI_IPA_INIT_MODEM_DRIVER_CMPLT_REQ_MAX_MSG_LEN_V01 4
#define QMI_IPA_INIT_MODEM_DRIVER_CMPLT_RESP_MAX_MSG_LEN_V01 7

#define QMI_IPA_INSTALL_FILTER_RULE_EX_REQ_MAX_MSG_LEN_V01 22685
#define QMI_IPA_INSTALL_FILTER_RULE_EX_RESP_MAX_MSG_LEN_V01 523

#define QMI_IPA_ENABLE_PER_CLIENT_STATS_REQ_MAX_MSG_LEN_V01 4
#define QMI_IPA_ENABLE_PER_CLIENT_STATS_RESP_MAX_MSG_LEN_V01 7

#define QMI_IPA_GET_STATS_PER_CLIENT_REQ_MAX_MSG_LEN_V01 18
#define QMI_IPA_GET_STATS_PER_CLIENT_RESP_MAX_MSG_LEN_V01 3595

#define QMI_IPA_INSTALL_UL_FIREWALL_RULES_REQ_MAX_MSG_LEN_V01 9875
#define QMI_IPA_INSTALL_UL_FIREWALL_RULES_RESP_MAX_MSG_LEN_V01 7
#define QMI_IPA_INSTALL_UL_FIREWALL_RULES_IND_MAX_MSG_LEN_V01 11
/* Service Object Accessor */

/* This is the largest MAX_MSG_LEN we have for all the messages
 * we expect to receive. This argument will be used in
 * qmi_handle_init to allocate a receive buffer for the socket
 * associated with our qmi_handle
 */
#define QMI_IPA_MAX_MSG_LEN 22685

#endif/* IPA_QMI_SERVICE_V01_H */
