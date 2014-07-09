#ifndef _UAPI_MSM_IPA_H_
#define _UAPI_MSM_IPA_H_

#ifndef __KERNEL__
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#endif
#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * unique magic number of the IPA device
 */
#define IPA_IOC_MAGIC 0xCF

/**
 * name of the default routing tables for v4 and v6
 */
#define IPA_DFLT_RT_TBL_NAME "ipa_dflt_rt"

/**
 *   the commands supported by IPA driver
 */
#define IPA_IOCTL_ADD_HDR            0
#define IPA_IOCTL_DEL_HDR            1
#define IPA_IOCTL_ADD_RT_RULE        2
#define IPA_IOCTL_DEL_RT_RULE        3
#define IPA_IOCTL_ADD_FLT_RULE       4
#define IPA_IOCTL_DEL_FLT_RULE       5
#define IPA_IOCTL_COMMIT_HDR         6
#define IPA_IOCTL_RESET_HDR          7
#define IPA_IOCTL_COMMIT_RT          8
#define IPA_IOCTL_RESET_RT           9
#define IPA_IOCTL_COMMIT_FLT        10
#define IPA_IOCTL_RESET_FLT         11
#define IPA_IOCTL_DUMP              12
#define IPA_IOCTL_GET_RT_TBL        13
#define IPA_IOCTL_PUT_RT_TBL        14
#define IPA_IOCTL_COPY_HDR          15
#define IPA_IOCTL_QUERY_INTF        16
#define IPA_IOCTL_QUERY_INTF_TX_PROPS 17
#define IPA_IOCTL_QUERY_INTF_RX_PROPS 18
#define IPA_IOCTL_GET_HDR           19
#define IPA_IOCTL_PUT_HDR           20
#define IPA_IOCTL_SET_FLT        21
#define IPA_IOCTL_ALLOC_NAT_MEM  22
#define IPA_IOCTL_V4_INIT_NAT    23
#define IPA_IOCTL_NAT_DMA        24
#define IPA_IOCTL_V4_DEL_NAT     26
#define IPA_IOCTL_PULL_MSG       27
#define IPA_IOCTL_GET_NAT_OFFSET 28
#define IPA_IOCTL_RM_ADD_DEPENDENCY 29
#define IPA_IOCTL_RM_DEL_DEPENDENCY 30
#define IPA_IOCTL_GENERATE_FLT_EQ 31
#define IPA_IOCTL_QUERY_INTF_EXT_PROPS 32
#define IPA_IOCTL_QUERY_EP_MAPPING 33
#define IPA_IOCTL_QUERY_RT_TBL_INDEX 34
#define IPA_IOCTL_WRITE_QMAPID 35
#define IPA_IOCTL_MDFY_FLT_RULE 36
#define IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_ADD	37
#define IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_DEL	38
#define IPA_IOCTL_MAX            39

/**
 * max size of the header to be inserted
 */
#define IPA_HDR_MAX_SIZE 64

/**
 * max size of the name of the resource (routing table, header)
 */
#define IPA_RESOURCE_NAME_MAX 20

/**
 * max number of interface properties
 */
#define IPA_NUM_PROPS_MAX 20

/**
 * size of the mac address
 */
#define IPA_MAC_ADDR_SIZE  6

/**
 * max number of mbim streams
 */
#define IPA_MBIM_MAX_STREAM_NUM 8

/**
 * the attributes of the rule (routing or filtering)
 */
#define IPA_FLT_TOS            (1ul << 0)
#define IPA_FLT_PROTOCOL       (1ul << 1)
#define IPA_FLT_SRC_ADDR       (1ul << 2)
#define IPA_FLT_DST_ADDR       (1ul << 3)
#define IPA_FLT_SRC_PORT_RANGE (1ul << 4)
#define IPA_FLT_DST_PORT_RANGE (1ul << 5)
#define IPA_FLT_TYPE           (1ul << 6)
#define IPA_FLT_CODE           (1ul << 7)
#define IPA_FLT_SPI            (1ul << 8)
#define IPA_FLT_SRC_PORT       (1ul << 9)
#define IPA_FLT_DST_PORT       (1ul << 10)
#define IPA_FLT_TC             (1ul << 11)
#define IPA_FLT_FLOW_LABEL     (1ul << 12)
#define IPA_FLT_NEXT_HDR       (1ul << 13)
#define IPA_FLT_META_DATA      (1ul << 14)
#define IPA_FLT_FRAGMENT       (1ul << 15)
#define IPA_FLT_TOS_MASKED     (1ul << 16)

/**
 * enum ipa_client_type - names for the various IPA "clients"
 * these are from the perspective of the clients, for e.g.
 * HSIC1_PROD means HSIC client is the producer and IPA is the
 * consumer
 */
enum ipa_client_type {
	IPA_CLIENT_PROD,
	IPA_CLIENT_HSIC1_PROD = IPA_CLIENT_PROD,
	IPA_CLIENT_WLAN1_PROD,
	IPA_CLIENT_HSIC2_PROD,
	IPA_CLIENT_USB2_PROD,
	IPA_CLIENT_HSIC3_PROD,
	IPA_CLIENT_USB3_PROD,
	IPA_CLIENT_HSIC4_PROD,
	IPA_CLIENT_USB4_PROD,
	IPA_CLIENT_HSIC5_PROD,
	IPA_CLIENT_USB_PROD,
	IPA_CLIENT_A5_WLAN_AMPDU_PROD,
	IPA_CLIENT_A2_EMBEDDED_PROD,
	IPA_CLIENT_A2_TETHERED_PROD,
	IPA_CLIENT_APPS_LAN_WAN_PROD,
	IPA_CLIENT_APPS_CMD_PROD,
	IPA_CLIENT_Q6_LAN_PROD,
	IPA_CLIENT_Q6_CMD_PROD,

	IPA_CLIENT_CONS,
	IPA_CLIENT_HSIC1_CONS = IPA_CLIENT_CONS,
	IPA_CLIENT_WLAN1_CONS,
	IPA_CLIENT_HSIC2_CONS,
	IPA_CLIENT_USB2_CONS,
	IPA_CLIENT_WLAN2_CONS,
	IPA_CLIENT_HSIC3_CONS,
	IPA_CLIENT_USB3_CONS,
	IPA_CLIENT_WLAN3_CONS,
	IPA_CLIENT_HSIC4_CONS,
	IPA_CLIENT_USB4_CONS,
	IPA_CLIENT_WLAN4_CONS,
	IPA_CLIENT_HSIC5_CONS,
	IPA_CLIENT_USB_CONS,
	IPA_CLIENT_A2_EMBEDDED_CONS,
	IPA_CLIENT_A2_TETHERED_CONS,
	IPA_CLIENT_A5_LAN_WAN_CONS,
	IPA_CLIENT_APPS_LAN_CONS,
	IPA_CLIENT_APPS_WAN_CONS,
	IPA_CLIENT_Q6_LAN_CONS,
	IPA_CLIENT_Q6_WAN_CONS,
	IPA_CLIENT_Q6_DUN_CONS,

	IPA_CLIENT_MAX,
};

#define IPA_CLIENT_IS_USB_CONS(client) \
	((client) == IPA_CLIENT_USB_CONS || \
	(client) == IPA_CLIENT_USB2_CONS || \
	(client) == IPA_CLIENT_USB3_CONS || \
	(client) == IPA_CLIENT_USB4_CONS)

#define IPA_CLIENT_IS_WLAN_CONS(client) \
	((client) == IPA_CLIENT_WLAN1_CONS || \
	(client) == IPA_CLIENT_WLAN2_CONS || \
	(client) == IPA_CLIENT_WLAN3_CONS || \
	(client) == IPA_CLIENT_WLAN4_CONS)

#define IPA_CLIENT_IS_Q6_CONS(client) \
	((client) == IPA_CLIENT_Q6_LAN_CONS || \
	(client) == IPA_CLIENT_Q6_WAN_CONS || \
	(client) == IPA_CLIENT_Q6_DUN_CONS)

#define IPA_CLIENT_IS_Q6_PROD(client) \
	((client) == IPA_CLIENT_Q6_LAN_PROD || \
	(client) == IPA_CLIENT_Q6_CMD_PROD)

/**
 * enum ipa_ip_type - Address family: IPv4 or IPv6
 */
enum ipa_ip_type {
	IPA_IP_v4,
	IPA_IP_v6,
	IPA_IP_MAX
};

/**
 * enum ipa_flt_action - action field of filtering rule
 *
 * Pass to routing: 5'd0
 * Pass to source NAT: 5'd1
 * Pass to destination NAT: 5'd2
 * Pass to default output pipe (e.g., Apps or Modem): 5'd3
 */
enum ipa_flt_action {
	IPA_PASS_TO_ROUTING,
	IPA_PASS_TO_SRC_NAT,
	IPA_PASS_TO_DST_NAT,
	IPA_PASS_TO_EXCEPTION
};

/**
 * enum ipa_wlan_event - Events for wlan client
 *
 * wlan client connect: New wlan client connected
 * wlan client disconnect: wlan client disconnected
 * wlan client power save: wlan client moved to power save
 * wlan client normal: wlan client moved out of power save
 * sw routing enable: ipa routing is disabled
 * sw routing disable: ipa routing is enabled
 * wlan ap connect: wlan AP(access point) is up
 * wlan ap disconnect: wlan AP(access point) is down
 * wlan sta connect: wlan STA(station) is up
 * wlan sta disconnect: wlan STA(station) is down
 */
enum ipa_wlan_event {
	WLAN_CLIENT_CONNECT,
	WLAN_CLIENT_DISCONNECT,
	WLAN_CLIENT_POWER_SAVE_MODE,
	WLAN_CLIENT_NORMAL_MODE,
	SW_ROUTING_ENABLE,
	SW_ROUTING_DISABLE,
	WLAN_AP_CONNECT,
	WLAN_AP_DISCONNECT,
	WLAN_STA_CONNECT,
	WLAN_STA_DISCONNECT,
	WLAN_CLIENT_CONNECT_EX,
	IPA_WLAN_EVENT_MAX
};

/**
 * enum ipa_wan_event - Events for wan client
 *
 * wan default route add/del
 */
enum ipa_wan_event {
	WAN_UPSTREAM_ROUTE_ADD = IPA_WLAN_EVENT_MAX,
	WAN_UPSTREAM_ROUTE_DEL,
	IPA_WAN_EVENT_MAX
};

enum ipa_ecm_event {
	ECM_CONNECT = IPA_WAN_EVENT_MAX,
	ECM_DISCONNECT,
	IPA_EVENT_MAX_NUM
};

#define IPA_EVENT_MAX ((int)IPA_EVENT_MAX_NUM)

/**
 * enum ipa_rm_resource_name - IPA RM clients identification names
 *
 * Add new mapping to ipa_rm_dep_prod_index() / ipa_rm_dep_cons_index()
 * when adding new entry to this enum.
 */
enum ipa_rm_resource_name {
	IPA_RM_RESOURCE_PROD = 0,
	IPA_RM_RESOURCE_Q6_PROD = IPA_RM_RESOURCE_PROD,
	IPA_RM_RESOURCE_USB_PROD,
	IPA_RM_RESOURCE_HSIC_PROD,
	IPA_RM_RESOURCE_STD_ECM_PROD,
	IPA_RM_RESOURCE_RNDIS_PROD,
	IPA_RM_RESOURCE_WWAN_0_PROD,
	IPA_RM_RESOURCE_ODU_PROD,
	IPA_RM_RESOURCE_ODU_BRIDGE_PROD,
	IPA_RM_RESOURCE_WLAN_PROD,
	IPA_RM_RESOURCE_PROD_MAX,

	IPA_RM_RESOURCE_Q6_CONS = IPA_RM_RESOURCE_PROD_MAX,
	IPA_RM_RESOURCE_USB_CONS,
	IPA_RM_RESOURCE_HSIC_CONS,
	IPA_RM_RESOURCE_WLAN_CONS,
	IPA_RM_RESOURCE_APPS_CONS,
	IPA_RM_RESOURCE_MAX
};

/**
 * struct ipa_rule_attrib - attributes of a routing/filtering
 * rule, all in LE
 * @attrib_mask: what attributes are valid
 * @src_port_lo: low port of src port range
 * @src_port_hi: high port of src port range
 * @dst_port_lo: low port of dst port range
 * @dst_port_hi: high port of dst port range
 * @type: ICMP/IGMP type
 * @code: ICMP/IGMP code
 * @spi: IPSec SPI
 * @src_port: exact src port
 * @dst_port: exact dst port
 * @meta_data: meta-data val
 * @meta_data_mask: meta-data mask
 * @u.v4.tos: type of service
 * @u.v4.protocol: protocol
 * @u.v4.src_addr: src address value
 * @u.v4.src_addr_mask: src address mask
 * @u.v4.dst_addr: dst address value
 * @u.v4.dst_addr_mask: dst address mask
 * @u.v6.tc: traffic class
 * @u.v6.flow_label: flow label
 * @u.v6.next_hdr: next header
 * @u.v6.src_addr: src address val
 * @u.v6.src_addr_mask: src address mask
 * @u.v6.dst_addr: dst address val
 * @u.v6.dst_addr_mask: dst address mask
 */
struct ipa_rule_attrib {
	uint32_t attrib_mask;
	uint16_t src_port_lo;
	uint16_t src_port_hi;
	uint16_t dst_port_lo;
	uint16_t dst_port_hi;
	uint8_t type;
	uint8_t code;
	uint8_t tos_value;
	uint8_t tos_mask;
	uint32_t spi;
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t meta_data;
	uint32_t meta_data_mask;
	union {
		struct {
			uint8_t tos;
			uint8_t protocol;
			uint32_t src_addr;
			uint32_t src_addr_mask;
			uint32_t dst_addr;
			uint32_t dst_addr_mask;
		} v4;
		struct {
			uint8_t tc;
			uint32_t flow_label;
			uint8_t next_hdr;
			uint32_t src_addr[4];
			uint32_t src_addr_mask[4];
			uint32_t dst_addr[4];
			uint32_t dst_addr_mask[4];
		} v6;
	} u;
};

/*! @brief The maximum number of Mask Equal 32 Eqns */
#define IPA_IPFLTR_NUM_MEQ_32_EQNS 2

/*! @brief The maximum number of IHL offset Mask Equal 32 Eqns */
#define IPA_IPFLTR_NUM_IHL_MEQ_32_EQNS 2

/*! @brief The maximum number of Mask Equal 128 Eqns */
#define IPA_IPFLTR_NUM_MEQ_128_EQNS 2

/*! @brief The maximum number of IHL offset Range Check 16 Eqns */
#define IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS 2

/*! @brief Offset and 16 bit comparison equation */
struct ipa_ipfltr_eq_16 {
	uint8_t offset;
	uint16_t value;
};

/*! @brief Offset and 32 bit comparison equation */
struct ipa_ipfltr_eq_32 {
	uint8_t offset;
	uint32_t value;
};

/*! @brief Offset and 128 bit masked comparison equation */
struct ipa_ipfltr_mask_eq_128 {
	uint8_t offset;
	uint8_t mask[16];
	uint8_t value[16];
};

/*! @brief Offset and 32 bit masked comparison equation */
struct ipa_ipfltr_mask_eq_32 {
	uint8_t offset;
	uint32_t mask;
	uint32_t value;
};

/*! @brief Equation for identifying a range. Ranges are inclusive */
struct ipa_ipfltr_range_eq_16 {
	uint8_t offset;
	uint16_t range_low;
	uint16_t range_high;
};

/*! @brief Rule equations which are set according to DS filter installation */
struct ipa_ipfltri_rule_eq {
	/*! 16-bit Bitmask to indicate how many eqs are valid in this rule  */
	uint16_t rule_eq_bitmap;
	/*! Specifies if a type of service check rule is present */
	uint8_t tos_eq_present;
	/*! The value to check against the type of service (ipv4) field */
	uint8_t tos_eq;
	/*! Specifies if a protocol check rule is present */
	uint8_t protocol_eq_present;
	/*! The value to check against the protocol (ipv6) field */
	uint8_t protocol_eq;
	/*! The number of ip header length offset 16 bit range check
	 * rules in this rule */
	uint8_t num_ihl_offset_range_16;
	/*! An array of the registered ip header length offset 16 bit
	 * range check rules */
	struct ipa_ipfltr_range_eq_16
		ihl_offset_range_16[IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS];
	/*! The number of mask equal 32 rules present in this rule */
	uint8_t num_offset_meq_32;
	/*! An array of all the possible mask equal 32 rules in this rule */
	struct ipa_ipfltr_mask_eq_32
		offset_meq_32[IPA_IPFLTR_NUM_MEQ_32_EQNS];
	/*! Specifies if the traffic class rule is present in this rule */
	uint8_t tc_eq_present;
	/*! The value to check the traffic class (ipv4) field against */
	uint8_t tc_eq;
	/*! Specifies if the flow equals rule is present in this rule */
	uint8_t fl_eq_present;
	/*! The value to check the flow (ipv6) field against */
	uint32_t fl_eq;
	/*! The number of ip header length offset 16 bit equations in this
	 * rule */
	uint8_t ihl_offset_eq_16_present;
	/*! The ip header length offset 16 bit equation */
	struct ipa_ipfltr_eq_16 ihl_offset_eq_16;
	/*! The number of ip header length offset 32 bit equations in this
	 * rule */
	uint8_t ihl_offset_eq_32_present;
	/*! The ip header length offset 32 bit equation */
	struct ipa_ipfltr_eq_32 ihl_offset_eq_32;
	/*! The number of ip header length offset 32 bit mask equations in
	 * this rule */
	uint8_t num_ihl_offset_meq_32;
	/*! The ip header length offset 32 bit mask equation */
	struct ipa_ipfltr_mask_eq_32
		ihl_offset_meq_32[IPA_IPFLTR_NUM_IHL_MEQ_32_EQNS];
	/*! The number of ip header length offset 128 bit equations in this
	 * rule */
	uint8_t num_offset_meq_128;
	/*! The ip header length offset 128 bit equation */
	struct ipa_ipfltr_mask_eq_128
		offset_meq_128[IPA_IPFLTR_NUM_MEQ_128_EQNS];
	/*! The metadata 32 bit masked comparison equation present or not */
	/* Metadata based rules are added internally by IPA driver */
	uint8_t metadata_meq32_present;
	/*! The metadata 32 bit masked comparison equation */
	struct ipa_ipfltr_mask_eq_32 metadata_meq32;
	/*! Specifies if the IPv4 Fragment equation is present in this rule */
	uint8_t ipv4_frag_eq_present;
};

/**
 * struct ipa_flt_rule - attributes of a filtering rule
 * @retain_hdr: bool switch to instruct IPA core to add back to the packet
 *  the header removed as part of header removal
 * @to_uc: bool switch to pass packet to micro-controller
 * @action: action field
 * @rt_tbl_hdl: handle of table from "get"
 * @attrib: attributes of the rule
 * @eq_attrib: attributes of the rule in equation form (valid when
 * eq_attrib_type is true)
 * @rt_tbl_idx: index of RT table referred to by filter rule (valid when
 * eq_attrib_type is true and non-exception action)
 * @eq_attrib_type: true if equation level form used to specify attributes
 */
struct ipa_flt_rule {
	uint8_t retain_hdr;
	uint8_t to_uc;
	enum ipa_flt_action action;
	uint32_t rt_tbl_hdl;
	struct ipa_rule_attrib attrib;
	struct ipa_ipfltri_rule_eq eq_attrib;
	uint32_t rt_tbl_idx;
	uint8_t eq_attrib_type;
};

/**
 * struct ipa_rt_rule - attributes of a routing rule
 * @dst: dst "client"
 * @hdr_hdl: handle to the dynamic header
	it is not an index or an offset
 * @attrib: attributes of the rule
 */
struct ipa_rt_rule {
	enum ipa_client_type dst;
	uint32_t hdr_hdl;
	struct ipa_rule_attrib attrib;
};

/**
 * struct ipa_hdr_add - header descriptor includes in and out
 * parameters
 * @name: name of the header
 * @hdr: actual header to be inserted
 * @hdr_len: size of above header
 * @is_partial: header not fully specified
 * @hdr_hdl: out paramerer, handle to header, valid when status is 0
 * @status:	out paramerer, status of header add operation,
 *		0 for success,
 *		-1 for failure
 * @is_eth2_ofst_valid: is eth2_ofst field valid?
 * @eth2_ofst: offset to start of Ethernet-II/802.3 header
 */
struct ipa_hdr_add {
	char name[IPA_RESOURCE_NAME_MAX];
	uint8_t hdr[IPA_HDR_MAX_SIZE];
	uint8_t hdr_len;
	uint8_t is_partial;
	uint32_t hdr_hdl;
	int status;
	uint8_t is_eth2_ofst_valid;
	uint16_t eth2_ofst;
};

/**
 * struct ipa_ioc_add_hdr - header addition parameters (support
 * multiple headers and commit)
 * @commit: should headers be written to IPA HW also?
 * @num_hdrs: num of headers that follow
 * @ipa_hdr_add hdr:	all headers need to go here back to
 *			back, no pointers
 */
struct ipa_ioc_add_hdr {
	uint8_t commit;
	uint8_t num_hdrs;
	struct ipa_hdr_add hdr[0];
};

/**
 * struct ipa_ioc_copy_hdr - retrieve a copy of the specified
 * header - caller can then derive the complete header
 * @name: name of the header resource
 * @hdr:	out parameter, contents of specified header,
 *	valid only when ioctl return val is non-negative
 * @hdr_len: out parameter, size of above header
 *	valid only when ioctl return val is non-negative
 * @is_partial:	out parameter, indicates whether specified header is partial
 *		valid only when ioctl return val is non-negative
 * @is_eth2_ofst_valid: is eth2_ofst field valid?
 * @eth2_ofst: offset to start of Ethernet-II/802.3 header
 */
struct ipa_ioc_copy_hdr {
	char name[IPA_RESOURCE_NAME_MAX];
	uint8_t hdr[IPA_HDR_MAX_SIZE];
	uint8_t hdr_len;
	uint8_t is_partial;
	uint8_t is_eth2_ofst_valid;
	uint16_t eth2_ofst;
};

/**
 * struct ipa_ioc_get_hdr - header entry lookup parameters, if lookup was
 * successful caller must call put to release the reference count when done
 * @name: name of the header resource
 * @hdl:	out parameter, handle of header entry
 *		valid only when ioctl return val is non-negative
 */
struct ipa_ioc_get_hdr {
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t hdl;
};

/**
 * struct ipa_hdr_del - header descriptor includes in and out
 * parameters
 *
 * @hdl: handle returned from header add operation
 * @status:	out parameter, status of header remove operation,
 *		0 for success,
 *		-1 for failure
 */
struct ipa_hdr_del {
	uint32_t hdl;
	int status;
};

/**
 * struct ipa_ioc_del_hdr - header deletion parameters (support
 * multiple headers and commit)
 * @commit: should headers be removed from IPA HW also?
 * @num_hdls: num of headers being removed
 * @ipa_hdr_del hdl: all handles need to go here back to back, no pointers
 */
struct ipa_ioc_del_hdr {
	uint8_t commit;
	uint8_t num_hdls;
	struct ipa_hdr_del hdl[0];
};

/**
 * struct ipa_rt_rule_add - routing rule descriptor includes in
 * and out parameters
 * @rule: actual rule to be added
 * @at_rear:	add at back of routing table, it is NOT possible to add rules at
 *		the rear of the "default" routing tables
 * @rt_rule_hdl: output parameter, handle to rule, valid when status is 0
 * @status:	output parameter, status of routing rule add operation,
 *		0 for success,
 *		-1 for failure
 */
struct ipa_rt_rule_add {
	struct ipa_rt_rule rule;
	uint8_t at_rear;
	uint32_t rt_rule_hdl;
	int status;
};

/**
 * struct ipa_ioc_add_rt_rule - routing rule addition parameters (supports
 * multiple rules and commit);
 *
 * all rules MUST be added to same table
 * @commit: should rules be written to IPA HW also?
 * @ip: IP family of rule
 * @rt_tbl_name: name of routing table resource
 * @num_rules: number of routing rules that follow
 * @ipa_rt_rule_add rules: all rules need to go back to back here, no pointers
 */
struct ipa_ioc_add_rt_rule {
	uint8_t commit;
	enum ipa_ip_type ip;
	char rt_tbl_name[IPA_RESOURCE_NAME_MAX];
	uint8_t num_rules;
	struct ipa_rt_rule_add rules[0];
};

/**
 * struct ipa_rt_rule_del - routing rule descriptor includes in
 * and out parameters
 * @hdl: handle returned from route rule add operation
 * @status:	output parameter, status of route rule delete operation,
 *		0 for success,
 *		-1 for failure
 */
struct ipa_rt_rule_del {
	uint32_t hdl;
	int status;
};

/**
 * struct ipa_ioc_del_rt_rule - routing rule deletion parameters (supports
 * multiple headers and commit)
 * @commit: should rules be removed from IPA HW also?
 * @ip: IP family of rules
 * @num_hdls: num of rules being removed
 * @ipa_rt_rule_del hdl: all handles need to go back to back here, no pointers
 */
struct ipa_ioc_del_rt_rule {
	uint8_t commit;
	enum ipa_ip_type ip;
	uint8_t num_hdls;
	struct ipa_rt_rule_del hdl[0];
};

/**
 * struct ipa_ioc_get_rt_tbl_indx - routing table index lookup parameters
 * @ip: IP family of table
 * @name: name of routing table resource
 * @index:	output parameter, routing table index, valid only when ioctl
 *		return val is non-negative
 */
struct ipa_ioc_get_rt_tbl_indx {
	enum ipa_ip_type ip;
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t idx;
};

/**
 * struct ipa_flt_rule_add - filtering rule descriptor includes
 * in and out parameters
 * @rule: actual rule to be added
 * @at_rear: add at back of filtering table?
 * @flt_rule_hdl: out parameter, handle to rule, valid when status is 0
 * @status:	output parameter, status of filtering rule add   operation,
 *		0 for success,
 *		-1 for failure
 *
 */
struct ipa_flt_rule_add {
	struct ipa_flt_rule rule;
	uint8_t at_rear;
	uint32_t flt_rule_hdl;
	int status;
};

/**
 * struct ipa_ioc_add_flt_rule - filtering rule addition parameters (supports
 * multiple rules and commit)
 * all rules MUST be added to same table
 * @commit: should rules be written to IPA HW also?
 * @ip: IP family of rule
 * @ep:	which "clients" pipe does this rule apply to?
 *	valid only when global is 0
 * @global: does this apply to global filter table of specific IP family
 * @num_rules: number of filtering rules that follow
 * @rules: all rules need to go back to back here, no pointers
 */
struct ipa_ioc_add_flt_rule {
	uint8_t commit;
	enum ipa_ip_type ip;
	enum ipa_client_type ep;
	uint8_t global;
	uint8_t num_rules;
	struct ipa_flt_rule_add rules[0];
};

/**
 * struct ipa_flt_rule_mdfy - filtering rule descriptor includes
 * in and out parameters
 * @rule: actual rule to be added
 * @flt_rule_hdl: handle to rule
 * @status:	output parameter, status of filtering rule modify  operation,
 *		0 for success,
 *		-1 for failure
 *
 */
struct ipa_flt_rule_mdfy {
	struct ipa_flt_rule rule;
	uint32_t rule_hdl;
	int status;
};

/**
 * struct ipa_ioc_mdfy_flt_rule - filtering rule modify parameters (supports
 * multiple rules and commit)
 * @commit: should rules be written to IPA HW also?
 * @ip: IP family of rule
 * @num_rules: number of filtering rules that follow
 * @rules: all rules need to go back to back here, no pointers
 */
struct ipa_ioc_mdfy_flt_rule {
	uint8_t commit;
	enum ipa_ip_type ip;
	uint8_t num_rules;
	struct ipa_flt_rule_mdfy rules[0];
};

/**
 * struct ipa_flt_rule_del - filtering rule descriptor includes
 * in and out parameters
 *
 * @hdl: handle returned from filtering rule add operation
 * @status:	output parameter, status of filtering rule delete operation,
 *		0 for success,
 *		-1 for failure
 */
struct ipa_flt_rule_del {
	uint32_t hdl;
	int status;
};

/**
 * struct ipa_ioc_del_flt_rule - filtering rule deletion parameters (supports
 * multiple headers and commit)
 * @commit: should rules be removed from IPA HW also?
 * @ip: IP family of rules
 * @num_hdls: num of rules being removed
 * @hdl: all handles need to go back to back here, no pointers
 */
struct ipa_ioc_del_flt_rule {
	uint8_t commit;
	enum ipa_ip_type ip;
	uint8_t num_hdls;
	struct ipa_flt_rule_del hdl[0];
};

/**
 * struct ipa_ioc_get_rt_tbl - routing table lookup parameters, if lookup was
 * successful caller must call put to release the reference
 * count when done
 * @ip: IP family of table
 * @name: name of routing table resource
 * @htl:	output parameter, handle of routing table, valid only when ioctl
 *		return val is non-negative
 */
struct ipa_ioc_get_rt_tbl {
	enum ipa_ip_type ip;
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t hdl;
};

/**
 * struct ipa_ioc_query_intf - used to lookup number of tx and
 * rx properties of interface
 * @name: name of interface
 * @num_tx_props:	output parameter, number of tx properties
 *			valid only when ioctl return val is non-negative
 * @num_rx_props:	output parameter, number of rx properties
 *			valid only when ioctl return val is non-negative
 * @num_ext_props:	output parameter, number of ext properties
 *			valid only when ioctl return val is non-negative
 * @excp_pipe:		exception packets of this interface should be
 *			routed to this pipe
 */
struct ipa_ioc_query_intf {
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t num_tx_props;
	uint32_t num_rx_props;
	uint32_t num_ext_props;
	enum ipa_client_type excp_pipe;
};

/**
 * struct ipa_ioc_tx_intf_prop - interface tx property
 * @ip: IP family of routing rule
 * @attrib: routing rule
 * @dst_pipe: routing output pipe
 * @hdr_name: name of associated header if any, empty string when no header
 */
struct ipa_ioc_tx_intf_prop {
	enum ipa_ip_type ip;
	struct ipa_rule_attrib attrib;
	enum ipa_client_type dst_pipe;
	char hdr_name[IPA_RESOURCE_NAME_MAX];
};

/**
 * struct ipa_ioc_query_intf_tx_props - interface tx propertie
 * @name: name of interface
 * @num_tx_props: number of TX properties
 * @tx[0]: output parameter, the tx properties go here back to back
 */
struct ipa_ioc_query_intf_tx_props {
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t num_tx_props;
	struct ipa_ioc_tx_intf_prop tx[0];
};

/**
 * struct ipa_ioc_ext_intf_prop - interface extended property
 * @ip: IP family of routing rule
 * @eq_attrib: attributes of the rule in equation form
 * @action: action field
 * @rt_tbl_idx: index of RT table referred to by filter rule
 * @mux_id: MUX_ID
 * @filter_hdl: handle of filter (as specified by provider of filter rule)
 */
struct ipa_ioc_ext_intf_prop {
	enum ipa_ip_type ip;
	struct ipa_ipfltri_rule_eq eq_attrib;
	enum ipa_flt_action action;
	uint32_t rt_tbl_idx;
	uint8_t mux_id;
	uint32_t filter_hdl;
};

/**
 * struct ipa_ioc_query_intf_ext_props - interface ext propertie
 * @name: name of interface
 * @num_ext_props: number of EXT properties
 * @ext[0]: output parameter, the ext properties go here back to back
 */
struct ipa_ioc_query_intf_ext_props {
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t num_ext_props;
	struct ipa_ioc_ext_intf_prop ext[0];
};

/**
 * struct ipa_ioc_rx_intf_prop - interface rx property
 * @ip: IP family of filtering rule
 * @attrib: filtering rule
 * @src_pipe: input pipe
 */
struct ipa_ioc_rx_intf_prop {
	enum ipa_ip_type ip;
	struct ipa_rule_attrib attrib;
	enum ipa_client_type src_pipe;
};

/**
 * struct ipa_ioc_query_intf_rx_props - interface rx propertie
 * @name: name of interface
 * @num_rx_props: number of RX properties
 * @rx: output parameter, the rx properties go here back to back
 */
struct ipa_ioc_query_intf_rx_props {
	char name[IPA_RESOURCE_NAME_MAX];
	uint32_t num_rx_props;
	struct ipa_ioc_rx_intf_prop rx[0];
};

/**
 * struct ipa_ioc_nat_alloc_mem - nat table memory allocation
 * properties
 * @dev_name: input parameter, the name of table
 * @size: input parameter, size of table in bytes
 * @offset: output parameter, offset into page in case of system memory
 */
struct ipa_ioc_nat_alloc_mem {
	char dev_name[IPA_RESOURCE_NAME_MAX];
	size_t size;
	off_t offset;
};

/**
 * struct ipa_ioc_v4_nat_init - nat table initialization
 * parameters
 * @tbl_index: input parameter, index of the table
 * @ipv4_rules_offset: input parameter, ipv4 rules address offset
 * @expn_rules_offset: input parameter, ipv4 expansion rules address offset
 * @index_offset: input parameter, index rules offset
 * @index_expn_offset: input parameter, index expansion rules offset
 * @table_entries: input parameter, ipv4 rules table size in entries
 * @expn_table_entries: input parameter, ipv4 expansion rules table size
 * @ip_addr: input parameter, public ip address
 */
struct ipa_ioc_v4_nat_init {
	uint8_t tbl_index;
	uint32_t ipv4_rules_offset;
	uint32_t expn_rules_offset;

	uint32_t index_offset;
	uint32_t index_expn_offset;

	uint16_t table_entries;
	uint16_t expn_table_entries;
	uint32_t ip_addr;
};

/**
 * struct ipa_ioc_v4_nat_del - nat table delete parameter
 * @table_index: input parameter, index of the table
 * @public_ip_addr: input parameter, public ip address
 */
struct ipa_ioc_v4_nat_del {
	uint8_t table_index;
	uint32_t public_ip_addr;
};

/**
 * struct ipa_ioc_nat_dma_one - nat dma command parameter
 * @table_index: input parameter, index of the table
 * @base_addr:	type of table, from which the base address of the table
 *		can be inferred
 * @offset: destination offset within the NAT table
 * @data: data to be written.
 */
struct ipa_ioc_nat_dma_one {
	uint8_t table_index;
	uint8_t base_addr;

	uint32_t offset;
	uint16_t data;

};

/**
 * struct ipa_ioc_nat_dma_cmd - To hold multiple nat dma commands
 * @entries: number of dma commands in use
 * @dma: data pointer to the dma commands
 */
struct ipa_ioc_nat_dma_cmd {
	uint8_t entries;
	struct ipa_ioc_nat_dma_one dma[0];

};

/**
 * struct ipa_msg_meta - Format of the message meta-data.
 * @msg_type: the type of the message
 * @rsvd: reserved bits for future use.
 * @msg_len: the length of the message in bytes
 *
 * For push model:
 * Client in user-space should issue a read on the device (/dev/ipa) with a
 * sufficiently large buffer in a continuous loop, call will block when there is
 * no message to read. Upon return, client can read the ipa_msg_meta from start
 * of buffer to find out type and length of message
 * size of buffer supplied >= (size of largest message + size of metadata)
 *
 * For pull model:
 * Client in user-space can also issue a pull msg IOCTL to device (/dev/ipa)
 * with a payload containing space for the ipa_msg_meta and the message specific
 * payload length.
 * size of buffer supplied == (len of specific message  + size of metadata)
 */
struct ipa_msg_meta {
	uint8_t msg_type;
	uint8_t rsvd;
	uint16_t msg_len;
};

/**
 * struct ipa_wlan_msg - To hold information about wlan client
 * @name: name of the wlan interface
 * @mac_addr: mac address of wlan client
 *
 * wlan drivers need to pass name of wlan iface and mac address of
 * wlan client along with ipa_wlan_event, whenever a wlan client is
 * connected/disconnected/moved to power save/come out of power save
 */
struct ipa_wlan_msg {
	char name[IPA_RESOURCE_NAME_MAX];
	uint8_t mac_addr[IPA_MAC_ADDR_SIZE];
};

/**
 * enum ipa_wlan_hdr_attrib_type - attribute type
 * in wlan client header
 *
 * WLAN_HDR_ATTRIB_MAC_ADDR: attrib type mac address
 * WLAN_HDR_ATTRIB_STA_ID: attrib type station id
 */
enum ipa_wlan_hdr_attrib_type {
	WLAN_HDR_ATTRIB_MAC_ADDR,
	WLAN_HDR_ATTRIB_STA_ID
};

/**
 * struct ipa_wlan_hdr_attrib_val - header attribute value
 * @attrib_type: type of attribute
 * @offset: offset of attribute within header
 * @u.mac_addr: mac address
 * @u.sta_id: station id
 */
struct ipa_wlan_hdr_attrib_val {
	enum ipa_wlan_hdr_attrib_type attrib_type;
	uint8_t offset;
	union {
		uint8_t mac_addr[IPA_MAC_ADDR_SIZE];
		uint8_t sta_id;
	} u;
};

/**
 * struct ipa_wlan_msg_ex - To hold information about wlan client
 * @name: name of the wlan interface
 * @num_of_attribs: number of attributes
 * @attrib_val: holds attribute values
 *
 * wlan drivers need to pass name of wlan iface and mac address
 * of wlan client or station id along with ipa_wlan_event,
 * whenever a wlan client is connected/disconnected/moved to
 * power save/come out of power save
 */
struct ipa_wlan_msg_ex {
	char name[IPA_RESOURCE_NAME_MAX];
	uint8_t num_of_attribs;
	struct ipa_wlan_hdr_attrib_val attribs[0];
};

struct ipa_ecm_msg {
	char name[IPA_RESOURCE_NAME_MAX];
	int ifindex;
};

/**
 * struct ipa_wan_msg - To hold information about wan client
 * @name: name of the wan interface
 *
 * CnE need to pass the name of default wan iface when connected/disconnected.
 */
struct ipa_wan_msg {
	char name[IPA_RESOURCE_NAME_MAX];
	enum ipa_ip_type ip;
};

/**
 * struct ipa_ioc_rm_dependency - parameters for add/delete dependency
 * @resource_name: name of dependent resource
 * @depends_on_name: name of its dependency
 */
struct ipa_ioc_rm_dependency {
	enum ipa_rm_resource_name resource_name;
	enum ipa_rm_resource_name depends_on_name;
};

struct ipa_ioc_generate_flt_eq {
	enum ipa_ip_type ip;
	struct ipa_rule_attrib attrib;
	struct ipa_ipfltri_rule_eq eq_attrib;
};

/**
 * struct ipa_ioc_write_qmapid - to write mux id to endpoint meta register
 * @mux_id: mux id of wan
 */
struct ipa_ioc_write_qmapid {
	enum ipa_client_type client;
	uint8_t qmap_id;
};


/**
 *   actual IOCTLs supported by IPA driver
 */
#define IPA_IOC_ADD_HDR _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_HDR, \
					struct ipa_ioc_add_hdr *)
#define IPA_IOC_DEL_HDR _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_HDR, \
					struct ipa_ioc_del_hdr *)
#define IPA_IOC_ADD_RT_RULE _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_RT_RULE, \
					struct ipa_ioc_add_rt_rule *)
#define IPA_IOC_DEL_RT_RULE _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_RT_RULE, \
					struct ipa_ioc_del_rt_rule *)
#define IPA_IOC_ADD_FLT_RULE _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_FLT_RULE, \
					struct ipa_ioc_add_flt_rule *)
#define IPA_IOC_DEL_FLT_RULE _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_FLT_RULE, \
					struct ipa_ioc_del_flt_rule *)
#define IPA_IOC_COMMIT_HDR _IO(IPA_IOC_MAGIC,\
					IPA_IOCTL_COMMIT_HDR)
#define IPA_IOC_RESET_HDR _IO(IPA_IOC_MAGIC,\
					IPA_IOCTL_RESET_HDR)
#define IPA_IOC_COMMIT_RT _IOW(IPA_IOC_MAGIC, \
					IPA_IOCTL_COMMIT_RT, \
					enum ipa_ip_type)
#define IPA_IOC_RESET_RT _IOW(IPA_IOC_MAGIC, \
					IPA_IOCTL_RESET_RT, \
					enum ipa_ip_type)
#define IPA_IOC_COMMIT_FLT _IOW(IPA_IOC_MAGIC, \
					IPA_IOCTL_COMMIT_FLT, \
					enum ipa_ip_type)
#define IPA_IOC_RESET_FLT _IOW(IPA_IOC_MAGIC, \
			IPA_IOCTL_RESET_FLT, \
			enum ipa_ip_type)
#define IPA_IOC_DUMP _IO(IPA_IOC_MAGIC, \
			IPA_IOCTL_DUMP)
#define IPA_IOC_GET_RT_TBL _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_RT_TBL, \
				struct ipa_ioc_get_rt_tbl *)
#define IPA_IOC_PUT_RT_TBL _IOW(IPA_IOC_MAGIC, \
				IPA_IOCTL_PUT_RT_TBL, \
				uint32_t)
#define IPA_IOC_COPY_HDR _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_COPY_HDR, \
				struct ipa_ioc_copy_hdr *)
#define IPA_IOC_QUERY_INTF _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF, \
				struct ipa_ioc_query_intf *)
#define IPA_IOC_QUERY_INTF_TX_PROPS _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF_TX_PROPS, \
				struct ipa_ioc_query_intf_tx_props *)
#define IPA_IOC_QUERY_INTF_RX_PROPS _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_RX_PROPS, \
					struct ipa_ioc_query_intf_rx_props *)
#define IPA_IOC_QUERY_INTF_EXT_PROPS _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_EXT_PROPS, \
					struct ipa_ioc_query_intf_ext_props *)
#define IPA_IOC_GET_HDR _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_HDR, \
				struct ipa_ioc_get_hdr *)
#define IPA_IOC_PUT_HDR _IOW(IPA_IOC_MAGIC, \
				IPA_IOCTL_PUT_HDR, \
				uint32_t)
#define IPA_IOC_ALLOC_NAT_MEM _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ALLOC_NAT_MEM, \
				struct ipa_ioc_nat_alloc_mem *)
#define IPA_IOC_V4_INIT_NAT _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_INIT_NAT, \
				struct ipa_ioc_v4_nat_init *)
#define IPA_IOC_NAT_DMA _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NAT_DMA, \
				struct ipa_ioc_nat_dma_cmd *)
#define IPA_IOC_V4_DEL_NAT _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_DEL_NAT, \
				struct ipa_ioc_v4_nat_del *)
#define IPA_IOC_GET_NAT_OFFSET _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_NAT_OFFSET, \
				uint32_t *)
#define IPA_IOC_SET_FLT _IOW(IPA_IOC_MAGIC, \
			IPA_IOCTL_SET_FLT, \
			uint32_t)
#define IPA_IOC_PULL_MSG _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_PULL_MSG, \
				struct ipa_msg_meta *)
#define IPA_IOC_RM_ADD_DEPENDENCY _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_ADD_DEPENDENCY, \
				struct ipa_ioc_rm_dependency *)
#define IPA_IOC_RM_DEL_DEPENDENCY _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_DEL_DEPENDENCY, \
				struct ipa_ioc_rm_dependency *)
#define IPA_IOC_GENERATE_FLT_EQ _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GENERATE_FLT_EQ, \
				struct ipa_ioc_generate_flt_eq *)
#define IPA_IOC_QUERY_EP_MAPPING _IOR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_EP_MAPPING, \
				uint32_t)
#define IPA_IOC_QUERY_RT_TBL_INDEX _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_RT_TBL_INDEX, \
				struct ipa_ioc_get_rt_tbl_indx *)
#define IPA_IOC_WRITE_QMAPID  _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_WRITE_QMAPID, \
				struct ipa_ioc_write_qmapid *)
#define IPA_IOC_MDFY_FLT_RULE _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_MDFY_FLT_RULE, \
					struct ipa_ioc_mdfy_flt_rule *)

#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_ADD, \
				struct ipa_wan_msg *)

#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_DEL, \
				struct ipa_wan_msg *)
/*
 * unique magic number of the Tethering bridge ioctls
 */
#define TETH_BRIDGE_IOC_MAGIC 0xCE

/*
 * Ioctls supported by Tethering bridge driver
 */
#define TETH_BRIDGE_IOCTL_SET_BRIDGE_MODE	0
#define TETH_BRIDGE_IOCTL_SET_AGGR_PARAMS	1
#define TETH_BRIDGE_IOCTL_GET_AGGR_PARAMS	2
#define TETH_BRIDGE_IOCTL_GET_AGGR_CAPABILITIES	3
#define TETH_BRIDGE_IOCTL_MAX			4


/**
 * enum teth_link_protocol_type - link protocol (IP / Ethernet)
 */
enum teth_link_protocol_type {
	TETH_LINK_PROTOCOL_IP,
	TETH_LINK_PROTOCOL_ETHERNET,
	TETH_LINK_PROTOCOL_MAX,
};

/**
 * enum teth_aggr_protocol_type - Aggregation protocol (MBIM / TLP)
 */
enum teth_aggr_protocol_type {
	TETH_AGGR_PROTOCOL_NONE,
	TETH_AGGR_PROTOCOL_MBIM,
	TETH_AGGR_PROTOCOL_TLP,
	TETH_AGGR_PROTOCOL_MAX,
};

/**
 * struct teth_aggr_params_link - Aggregation parameters for uplink/downlink
 * @aggr_prot:			Aggregation protocol (MBIM / TLP)
 * @max_transfer_size_byte:	Maximal size of aggregated packet in bytes.
 *				Default value is 16*1024.
 * @max_datagrams:		Maximal number of IP packets in an aggregated
 *				packet. Default value is 16
 */
struct teth_aggr_params_link {
	enum teth_aggr_protocol_type aggr_prot;
	uint32_t max_transfer_size_byte;
	uint32_t max_datagrams;
};


/**
 * struct teth_aggr_params - Aggregation parmeters
 * @ul:	Uplink parameters
 * @dl: Downlink parmaeters
 */
struct teth_aggr_params {
	struct teth_aggr_params_link ul;
	struct teth_aggr_params_link dl;
};

/**
 * struct teth_aggr_capabilities - Aggregation capabilities
 * @num_protocols:		Number of protocols described in the array
 * @prot_caps[]:		Array of aggregation capabilities per protocol
 */
struct teth_aggr_capabilities {
	uint16_t num_protocols;
	struct teth_aggr_params_link prot_caps[0];
};

/**
 * struct teth_ioc_set_bridge_mode
 * @link_protocol: link protocol (IP / Ethernet)
 * @lcid: logical channel number
 */
struct teth_ioc_set_bridge_mode {
	enum teth_link_protocol_type link_protocol;
	uint16_t lcid;
};

/**
 * struct teth_ioc_set_aggr_params
 * @aggr_params: Aggregation parmeters
 * @lcid: logical channel number
 */
struct teth_ioc_aggr_params {
	struct teth_aggr_params aggr_params;
	uint16_t lcid;
};


#define TETH_BRIDGE_IOC_SET_BRIDGE_MODE _IOW(TETH_BRIDGE_IOC_MAGIC, \
				TETH_BRIDGE_IOCTL_SET_BRIDGE_MODE, \
				struct teth_ioc_set_bridge_mode *)
#define TETH_BRIDGE_IOC_SET_AGGR_PARAMS _IOW(TETH_BRIDGE_IOC_MAGIC, \
				TETH_BRIDGE_IOCTL_SET_AGGR_PARAMS, \
				struct teth_ioc_aggr_params *)
#define TETH_BRIDGE_IOC_GET_AGGR_PARAMS _IOR(TETH_BRIDGE_IOC_MAGIC, \
				TETH_BRIDGE_IOCTL_GET_AGGR_PARAMS, \
				struct teth_ioc_aggr_params *)
#define TETH_BRIDGE_IOC_GET_AGGR_CAPABILITIES _IOWR(TETH_BRIDGE_IOC_MAGIC, \
				TETH_BRIDGE_IOCTL_GET_AGGR_CAPABILITIES, \
				struct teth_aggr_capabilities *)

#endif /* _UAPI_MSM_IPA_H_ */
