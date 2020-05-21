/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL2_FW_H_
#define _ATL2_FW_H_

/* Start of HW byte packed interface declaration */
#pragma pack(push, 1)

/* F W    A P I */

struct link_options_s {
	uint32_t link_up:1;
	uint32_t link_renegotiate:1;
	uint32_t minimal_link_speed:1;
	uint32_t internal_loopback:1;
	uint32_t external_loopback:1;
	uint32_t rate_10M_hd:1;
	uint32_t rate_100M_hd:1;
	uint32_t rate_1G_hd:1;


	uint32_t rate_10M:1;
	uint32_t rate_100M:1;
	uint32_t rate_1G:1;
	uint32_t rate_2P5G:1;
	uint32_t rate_N2P5G:1;
	uint32_t rate_5G:1;
	uint32_t rate_N5G:1;
	uint32_t rate_10G:1;

	uint32_t eee_100M:1;
	uint32_t eee_1G:1;
	uint32_t eee_2P5G:1;
	uint32_t eee_5G:1;
	uint32_t eee_10G:1;
	uint32_t rsvd3:2;
	uint32_t low_power_autoneg:1;

	uint32_t pause_rx:1;
	uint32_t pause_tx:1;
	uint32_t rsvd4:1;
	uint32_t downshift:1;
	uint32_t downshift_retry:4;
};

struct link_control_s {
	uint32_t mode:4;

	uint32_t disable_crc_corruption : 1;
	uint32_t discard_short_frames : 1;
	uint32_t flow_control_mode : 1;
	uint32_t disable_length_check : 1;
	uint32_t discard_errored_frames : 1;
	uint32_t control_frame_enable : 1;
	uint32_t enable_tx_padding : 1;
	uint32_t enable_crc_forwarding : 1;
	uint32_t enable_frame_padding_removal_rx: 1;
	uint32_t promiscuous_mode: 1;
	uint32_t rsvd:18;
};

struct thermal_shutdown_s {
	uint32_t shutdown_enable :1;
	uint32_t warning_enable :1;
	uint32_t rsvd:6;

	uint32_t shutdown_temp_threshold :8;
	uint32_t warning_cold_temp_threshold :8;
	uint32_t warning_hot_tempThreshold :8;
};

struct mac_address_s {
	uint8_t mac_address[6];
	uint16_t rsvd;
};

struct sleep_proxy_s {
	struct wake_on_lan_s {
		uint32_t wake_on_magic_packet:1;
		uint32_t wake_on_pattern:1;
		uint32_t wake_on_link_up:1;
		uint32_t wake_on_link_down:1;
		uint32_t wake_on_ping:1;
		uint32_t wake_on_timer:1;
		uint32_t wake_on_link_mac_method:1;
		uint32_t rsrvd1:1;
		uint32_t restore_link_before_wake:1;
		uint32_t rsvd:23;

		uint32_t link_up_timeout;
		uint32_t link_down_timeout;
		uint32_t timer;

		struct {
			uint32_t mask[4];
			uint32_t crc32;
		} wake_up_patterns[8];
	} wake_on_lan;

	struct {
		uint32_t arp_responder:1;
		uint32_t echo_responder:1;
		uint32_t igmp_client:1;
		uint32_t echo_truncate:1;
		uint32_t address_guard:1;
		uint32_t ignore_fragmented:1;
		uint32_t rsvd:2;
		uint32_t echo_max_len:16;
		uint32_t ipv4[8];
		uint32_t reserved[8];

	} ipv4_offload;

	struct {
		uint32_t ns_responder:1;
		uint32_t echo_responder:1;
		uint32_t mld_client:1;
		uint32_t echo_truncate:1;
		uint32_t address_guard:1;
		uint32_t rsvd:3;
		uint32_t echo_max_len:16;
		uint32_t ipv6[16][4];
	} ipv6_offload;

	struct {
		uint16_t ports[16];
	} tcp_port_offload;

	struct {
		uint16_t ports[16];
	} udp_port_offload;

	struct ka4_offloads_s {
		uint32_t retry_count;
		uint32_t retry_interval;

		struct ka4_offload_s {
			uint32_t timeout;
			uint16_t local_port;
			uint16_t remote_port;
			uint8_t remote_mac_addr[6];
			uint32_t rsvd:16;
			uint32_t rsvd2:32;
			uint32_t rsvd3:32;
			uint32_t rsvd4:16;
			uint16_t win_size;
			uint32_t seq_num;
			uint32_t ack_num;
			uint32_t local_ip;
			uint32_t remote_ip;
		} offloads[16];
	} ka4_offload;

	struct ka6_offloads_s {
		uint32_t retry_count;
		uint32_t retry_interval;

		struct ka6_offload_s {
			uint32_t timeout;
			uint16_t local_port;
			uint16_t remote_port;
			uint8_t remote_mac_addr[6];
			uint32_t rsvd:16;
			uint32_t rsvd2:32;
			uint32_t rsvd3:32;
			uint32_t rsvd4:16;
			uint16_t win_size;
			uint32_t seq_num;
			uint32_t ack_num;
			uint32_t local_ip[4];
			uint32_t remote_ip[4];
		} offloads[16];
	} ka6_offload;

	struct {
		uint32_t rr_count;
		uint32_t rr_buf_len;
		uint32_t idx_offset;
		uint32_t rr__offset;
	} mdns;
	/* WARN: where this gap actually is not known */
	uint32_t reserveFWGAP:16;
};

struct ptp_s {
	uint32_t enable:1;
};

struct pause_quanta_s {
	uint16_t quanta_10M;
	uint16_t threshold_10M;
	uint16_t quanta_100M;
	uint16_t threshold_100M;
	uint16_t quanta_1G;
	uint16_t threshold_1G;
	uint16_t quanta_2P5G;
	uint16_t threshold_2P5G;
	uint16_t quanta_5G;
	uint16_t threshold_5G;
	uint16_t quanta_10G;
	uint16_t threshold_10G;
};

struct data_buffer_status_s {
	uint32_t data_offset;
	uint32_t data_length;
};

struct device_caps_s {
	uint32_t finite_flashless: 1;
	uint32_t cable_diag: 1;
	uint32_t ncsi: 1;
	uint32_t avb: 1;
	uint32_t rsvd: 28;
	uint32_t rsvd2: 32;
};

struct version_s {
	struct bundle_version_t {
		uint32_t major:8;
		uint32_t minor:8;
		uint32_t build:16;
	} bundle;
	struct mac_version_t {
		uint32_t major:8;
		uint32_t minor:8;
		uint32_t build:16;
	} mac;
	struct phy_version_t {
		uint32_t major:8;
		uint32_t minor:8;
		uint32_t build:16;
	} phy;
	uint32_t rsvd:32;
};

struct link_status_s {
	uint32_t link_state:4;
	uint32_t link_rate:4;

	uint32_t pause_tx:1;
	uint32_t pause_rx:1;
	uint32_t eee:1;
	uint32_t duplex:1;
	uint32_t rsvd:4;

	uint32_t rsvd2:16;
};

struct wol_status_s {
	uint32_t wake_count:8;
	uint32_t wake_reason:8;
	uint32_t wake_up_packet_length :12;
	uint32_t wake_up_pattern_number :3;
	uint32_t rsvd:1;
	uint32_t wake_up_packet[379];
};

struct mac_health_monitor_s {
	uint32_t mac_ready:1;
	uint32_t mac_fault:1;
	uint32_t mac_flashless_finished:1;
	uint32_t rsvd:5;
	uint32_t mac_temperature:8;
	uint32_t mac_heart_beat:16;
	uint32_t mac_fault_code:16;
	uint32_t rsvd2:16;
};

struct phy_health_monitor_s {
	uint32_t phy_ready:1;
	uint32_t phy_fault:1;
	uint32_t phy_hot_warning:1;
	uint32_t rsvd:5;
	uint32_t phy_temperature:8;
	uint32_t phy_heart_beat:16;
	uint32_t phy_fault_code:16;
	uint32_t rsvd2:16;
};

struct device_link_caps_s {
	uint32_t rsvd:3;
	uint32_t internal_loopback:1;
	uint32_t external_loopback:1;
	uint32_t rate_10M_hd:1;
	uint32_t rate_100M_hd:1;
	uint32_t rate_1G_hd:1;

	uint32_t rate_10M:1;
	uint32_t rate_100M:1;
	uint32_t rate_1G:1;
	uint32_t rate_2P5G:1;
	uint32_t rate_N2P5G:1;
	uint32_t rate_5G:1;
	uint32_t rate_N5G:1;
	uint32_t rate_10G:1;

	uint32_t rsvd3:1;
	uint32_t eee_100M:1;
	uint32_t eee_1G:1;
	uint32_t eee_2P5G:1;
	uint32_t rsvd4:1;
	uint32_t eee_5G:1;
	uint32_t rsvd5:1;
	uint32_t eee_10G:1;

	uint32_t pause_rx:1;
	uint32_t pause_tx:1;
	uint32_t pfc:1;
	uint32_t downshift:1;
	uint32_t downshift_retry:4;
};

struct sleep_proxy_caps_s {
	uint32_t ipv4_offload:1;
	uint32_t ipv6_offload:1;
	uint32_t tcp_port_offload:1;
	uint32_t udp_port_offload:1;
	uint32_t ka4_offload:1;
	uint32_t ka6_offload:1;
	uint32_t mdns_offload:1;
	uint32_t wake_on_ping:1;

	uint32_t wake_on_magic_packet:1;
	uint32_t wake_on_pattern:1;
	uint32_t wake_on_timer:1;
	uint32_t wake_on_link:1;
	uint32_t wake_patterns_count:4;

	uint32_t ipv4_count:8;
	uint32_t ipv6_count:8;

	uint32_t tcp_port_offload_count:8;
	uint32_t udp_port_offload_count:8;

	uint32_t tcp4_ka_count:8;
	uint32_t tcp6_ka_count:8;

	uint32_t igmp_offload:1;
	uint32_t mld_offload:1;
	uint32_t rsvd:30;
};

struct lkp_link_caps_s {
	uint32_t rsvd:5;
	uint32_t rate_10M_hd:1;
	uint32_t rate_100M_hd:1;
	uint32_t rate_1G_hd:1;

	uint32_t rate_10M:1;
	uint32_t rate_100M:1;
	uint32_t rate_1G:1;
	uint32_t rate_2P5G:1;
	uint32_t rate_N2P5G:1;
	uint32_t rate_5G:1;
	uint32_t rate_N5G:1;
	uint32_t rate_10G:1;

	uint32_t rsvd2:1;
	uint32_t eee_100M:1;
	uint32_t eee_1G:1;
	uint32_t eee_2P5G:1;
	uint32_t rsvd3:1;
	uint32_t eee_5G:1;
	uint32_t rsvd4:1;
	uint32_t eee_10G:1;

	uint32_t pause_rx:1;
	uint32_t pause_tx:1;
	uint32_t rsvd5:6;
};

struct core_dump_s {
	uint32_t reg0;
	uint32_t reg1;
	uint32_t reg2;

	uint32_t hi;
	uint32_t lo;

	uint32_t regs[32];
};

struct trace_s {
	uint32_t sync_counter;
	uint32_t mem_buffer[0x1ff];
};

struct cable_diag_control_s {
	uint32_t toggle :1;
	uint32_t rsvd:7;
	uint32_t wait_timeout_sec:8;
	uint32_t rsvd2:16;

};

struct cable_diag_lane_data_s {
	uint32_t result_code :8;
	uint32_t dist :8;
	uint32_t far_dist :8;
	uint32_t rsvd:8;
};

struct cable_diag_status_s {
	struct cable_diag_lane_data_s lane_data[4];
	uint32_t transact_id:8;
	uint32_t status:4;
	uint32_t rsvd:20;
};

struct phy_fw_load_status_s {
	uint32_t phy_fw_load_from_host :1;
	uint32_t phy_fw_load_from_flash :1;
	uint32_t phy_fw_load_from_d_c :1;
	uint32_t phy_load_from_flash_failed :1;
	uint32_t phy_load_from_host_failed :1;
	uint32_t phy_load_from_d_c_failed :1;
	uint32_t phy_hash_validation_failed :1;
	uint32_t phy_fw_started :1;

	uint32_t phy_stall_timeout :1;
	uint32_t phy_unstall_timeout :1;
	uint32_t phy_fw_start_timeout :1;
	uint32_t phy_iram_load_error :1;
	uint32_t phy_dram_load_error :1;
	uint32_t phy_mcp_run_failed :1;
	uint32_t phy_mcp_stall_failed :1;
	uint32_t phy_mcp_unstall_failed :1;

	uint32_t phy_wait_for_semaphore :1;
	uint32_t phy_semaphore_locked :1;
	uint32_t rsvd :2;
	uint32_t phy_worst_block_upload_retry_number:4;

	uint32_t phy_worst_upload_block_number :6;
	uint32_t rsvd2:2;
};

struct statistics_s {
	struct {
		uint32_t link_up;
		uint32_t link_down;
	} link;

	struct {
		uint64_t tx_unicast_octets;
		uint64_t tx_multicast_octets;
		uint64_t tx_broadcast_octets;
		uint64_t rx_unicast_octets;
		uint64_t rx_multicast_octets;
		uint64_t rx_broadcast_octets;

		uint32_t tx_unicast_frames;
		uint32_t tx_multicast_frames;
		uint32_t tx_broadcast_frames;
		uint32_t tx_errors;

		uint32_t rx_unicast_frames;
		uint32_t rx_multicast_frames;
		uint32_t rx_broadcast_frames;
		uint32_t rx_dropped_frames;
		uint32_t rx_error_frames;

		uint32_t tx_good_frames;
		uint32_t rx_good_frames;
		uint32_t reserveFWGAP;
	} msm;
	uint32_t main_loop_cycles;
};

struct  filter_caps_s {
	uint8_t unicast_filters_count;
	uint8_t multicast_filters_count;
	uint8_t ethertype_filters_count;
	uint8_t vlan_filters_count;
	uint8_t l3_filters_count;
	uint8_t l4_filters_count;
	uint8_t l4_flex_filters_count;
	uint8_t flexible_filters_count;
};

struct fw_interface_in {
	uint32_t mtu;
	uint32_t rsvd1:32;
	struct mac_address_s mac_address;
	struct link_control_s link_control;
	uint32_t rsvd2:32;
	struct link_options_s link_options;
	uint32_t rsvd3:32;
	struct thermal_shutdown_s thermal_shutdown;
	uint32_t rsvd4:32;
	struct sleep_proxy_s sleep_proxy;
	uint32_t rsvd5:32;
	struct pause_quanta_s pause_quanta[8];
	struct cable_diag_control_s cable_diag_control;
	uint32_t rsvd6:32;
	struct data_buffer_status_s data_buffer_status;
};

struct transaction_counter_s {
	uint32_t transaction_cnt_a:16;
	uint32_t transaction_cnt_b:16;
};

struct fw_interface_out {
	struct transaction_counter_s transaction_id;
	struct version_s version;
	struct link_status_s link_status;
	struct wol_status_s wol_status;
	uint32_t rsvd:32;
	uint32_t rsvd2:32;
	struct mac_health_monitor_s mac_health_monitor;
	uint32_t rsvd3:32;
	uint32_t rsvd4:32;
	struct phy_health_monitor_s phy_health_monitor;
	uint32_t rsvd5:32;
	uint32_t rsvd6:32;
	struct cable_diag_status_s cable_diag_status;
	uint32_t rsvd7:32;
	struct device_link_caps_s device_link_caps;
	uint32_t rsvd8:32;
	struct sleep_proxy_caps_s sleep_proxy_caps;
	uint32_t rsvd9:32;
	struct lkp_link_caps_s lkp_link_caps;
	uint32_t rsvd10:32;
	struct core_dump_s core_dump;
	uint32_t rsvd11:32;
	struct statistics_s stats;
	uint32_t rsvd12:32;
	uint32_t rsvd13:32;
	struct filter_caps_s filter_caps;
	uint32_t rsvd14:32;
	struct device_caps_s device_caps;
	uint32_t reserve[30];
	struct trace_s trace;
};

struct fw_iti_subblock_header {
	uint32_t type :8;
	uint32_t length :24;
};

struct fw_iti_hdr {
	uint32_t instuction_bitmask;
	uint32_t reserved;
	struct fw_iti_subblock_header iti[6];
};
/* End of HW byte packed interface declaration */
#pragma pack(pop)


#define ATL2_FW_LINK_RATE_INVALID 0
#define ATL2_FW_LINK_RATE_10M     1
#define ATL2_FW_LINK_RATE_100M    2
#define ATL2_FW_LINK_RATE_1G      3
#define ATL2_FW_LINK_RATE_2G5     4
#define ATL2_FW_LINK_RATE_5G      5
#define ATL2_FW_LINK_RATE_10G     6

#define ATL2_HOST_MODE_INVALID      0
#define ATL2_HOST_MODE_ACTIVE       1
#define ATL2_HOST_MODE_SLEEP_PROXY  2
#define ATL2_HOST_MODE_LOW_POWER    3
#define ATL2_HOST_MODE_SHUTDOWN     4

#define ATL2_FW_CABLE_STATUS_OPEN_CIRCUIT  7
#define ATL2_FW_CABLE_STATUS_HIGH_MISMATCH 6
#define ATL2_FW_CABLE_STATUS_LOW_MISMATCH  5
#define ATL2_FW_CABLE_STATUS_SHORT_CIRCUIT 4
#define ATL2_FW_CABLE_STATUS_PAIR_D        3
#define ATL2_FW_CABLE_STATUS_PAIR_C        2
#define ATL2_FW_CABLE_STATUS_PAIR_B        1
#define ATL2_FW_CABLE_STATUS_OK            0

#define ATL2_FW_HOST_INTERRUPT_MAC_READY           0x0004
#define ATL2_FW_HOST_INTERRUPT_DATA_HANDLED        0x0100
#define ATL2_FW_HOST_INTERRUPT_LINK_UP             0x0200
#define ATL2_FW_HOST_INTERRUPT_LINK_DOWN           0x0400
#define ATL2_FW_HOST_INTERRUPT_PHY_FAULT           0x0800
#define ATL2_FW_HOST_INTERRUPT_MAC_FAULT           0x1000
#define ATL2_FW_HOST_INTERRUPT_TEMPERATURE_WARNING 0x2000
#define ATL2_FW_HOST_INTERRUPT_HEARTBEAT           0x4000

#define ATL2_ITI_ADDRESS_START    0x100000
#define ATL2_ITI_ADDRESS_BLOCK_1  (ATL2_ITI_ADDRESS_START +\
				   sizeof(struct fw_iti_hdr) / sizeof(uint32_t))
enum {
	ATL2_MEMORY_MAILBOX_STATUS_FAIL = 0,
	ATL2_MEMORY_MAILBOX_STATUS_SUCCESS = 1
};

enum {
	ATL2_MEMORY_MAILBOX_TARGET_MEMORY = 0,
	ATL2_MEMORY_MAILBOX_TARGET_MDIO = 1
};

enum {
	ATL2_MEMORY_MAILBOX_OPERATION_READ = 0,
	ATL2_MEMORY_MAILBOX_OPERATION_WRITE = 1
};

enum ATL2_WAKE_REASON {
	ATL2_WAKE_REASON_UNKNOWN,
	ATL2_WAKE_REASON_PANIC,
	ATL2_WAKE_REASON_LINK,
	ATL2_WAKE_REASON_TIMER,
	ATL2_WAKE_REASON_RESERVED,
	ATL2_WAKE_REASON_NAME_GUARD,
	ATL2_WAKE_REASON_ADDR_GUARD,
	ATL2_WAKE_REASON_TCPKA,

	ATL2_WAKE_REASON_DATA_FLAG,

	ATL2_WAKE_REASON_PING,
	ATL2_WAKE_REASON_SYN,
	ATL2_WAKE_REASON_UDP,
	ATL2_WAKE_REASON_PATTERN,
	ATL2_WAKE_REASON_MAGIC_PACKET
};

int atl2_fw_init(struct atl_hw *hw);
int atl2_get_fw_version(struct atl_hw *hw, u32 *fw_version);


#endif
