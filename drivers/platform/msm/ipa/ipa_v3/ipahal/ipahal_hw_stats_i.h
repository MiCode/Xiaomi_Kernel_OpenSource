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

#ifndef _IPAHAL_HW_STATS_I_H_
#define _IPAHAL_HW_STATS_I_H_

#include "ipahal_hw_stats.h"

int ipahal_hw_stats_init(enum ipa_hw_type ipa_hw_type);

struct ipahal_stats_quota_hw {
	u64 num_ipv4_bytes;
	u64 num_ipv4_pkts:32;
	u64 num_ipv6_pkts:32;
	u64 num_ipv6_bytes;
};

struct ipahal_stats_tethering_hdr_hw {
	u64 dst_mask:32;
	u64 offset:32;
};

struct ipahal_stats_tethering_hw {
	u64 num_ipv4_bytes;
	u64 num_ipv4_pkts:32;
	u64 num_ipv6_pkts:32;
	u64 num_ipv6_bytes;
};

struct ipahal_stats_flt_rt_hdr_hw {
	u64 en_mask:32;
	u64 reserved:16;
	u64 cnt_offset:16;
};

struct ipahal_stats_flt_rt_hw {
	u64 num_packets_hash:32;
	u64 num_packets:32;
};

struct ipahal_stats_drop_hw {
	u64 drop_byte_cnt:40;
	u64 drop_packet_cnt:24;
};

#endif /* _IPAHAL_HW_STATS_I_H_ */
