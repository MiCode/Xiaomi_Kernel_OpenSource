/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _scmi_scmi_cpufreqstats_h
#define _scmi_scmi_cpufreqstats_h

#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>

#define SCMI_VENDOR_MSG_MODULE_START   (16)
#define SCMI_CPUFREQ_STATS_PROTOCOL    0x84

struct scmi_protocol_handle;
struct cpufreq_stats_prot_attr {
	uint32_t attributes;
	uint32_t statistics_address_low;
	uint32_t statistics_address_high;
	uint32_t statistics_len;
};
/**
 * struct scmi_cpufreq_stats_vendor_ops - represents the various operations
 *     provided by SCMI CPUFREQ STATS Protocol
 *
 * @cpufreq_stats_info_get: Generate and send an SCMI message to retrieve stats
 *                          params
 * @set_log_level: Generate and send an SCMI message to set log
 *                          level
 */
struct scmi_cpufreq_stats_vendor_ops {
	int (*cpufreq_stats_info_get)(const struct scmi_protocol_handle *ph, void *buf);
	int (*set_log_level)(const struct scmi_protocol_handle *ph, u32 val);
};

#endif /* _scmi_scmi_cpufreqstats_h */

