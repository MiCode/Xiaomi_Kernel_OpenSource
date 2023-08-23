// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */


#include "common.h"
#include <linux/scmi_cpufreq_stats.h>

#define SCMI_VENDOR_MSG_START    (3)

enum scmi_cpufreq_stats_protocol_cmd {
	CPUFREQSTATS_SET_LOG_LEVEL = SCMI_VENDOR_MSG_START,
	CPUFREQSTATS_GET_MEM_INFO = SCMI_VENDOR_MSG_MODULE_START,
	CPUFREQSTATS_MAX_MSG
};
static int scmi_get_tunable_cpufreq_stats(const struct scmi_protocol_handle *ph,
				    void *buf, u32 msg_id)
{
	int ret;
	struct scmi_xfer *t;
	struct cpufreq_stats_prot_attr *msg;

	ret = ph->xops->xfer_get_init(ph, msg_id, sizeof(*msg), sizeof(*msg),
				      &t);
	if (ret)
		return ret;

	ret = ph->xops->do_xfer(ph, t);
	if (!ret)
		memcpy(buf, t->rx.buf, t->rx.len);
	ph->xops->xfer_put(ph, t);
	return ret;
}



static int scmi_set_global_var(const struct scmi_protocol_handle *ph, u32 val, u32 msg_id)
{
	int ret = 0;
	struct scmi_xfer *t;
	u32 *ptr;

	ret = ph->xops->xfer_get_init(ph, msg_id, sizeof(u32), sizeof(u32), &t);
	if (ret)
		return ret;
	ptr = (u32 *)t->tx.buf;
	*ptr = cpu_to_le32(val);
	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);

	return ret;
}

static int scmi_set_log_level(const struct scmi_protocol_handle *ph, u32 val)
{
	return scmi_set_global_var(ph, val, CPUFREQSTATS_SET_LOG_LEVEL);
}

static int scmi_get_cpufreq_stats_info(const struct scmi_protocol_handle *ph, void *buf)
{
	return scmi_get_tunable_cpufreq_stats(ph, buf, CPUFREQSTATS_GET_MEM_INFO);
}


static struct scmi_cpufreq_stats_vendor_ops cpufreq_stats_config_ops = {
	.cpufreq_stats_info_get = scmi_get_cpufreq_stats_info,
	.set_log_level = scmi_set_log_level,
};

static int scmi_cpufreq_stats_protocol_init(const struct scmi_protocol_handle *ph)
{
	u32 version;
	int ret;

	ret =  ph->xops->version_get(ph, &version);
	if (ret)
		return ret;

	dev_dbg(ph->dev, "cpufreq stats version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	return 0;
}

static const struct scmi_protocol scmi_cpufreq_stats = {
	.id = SCMI_CPUFREQ_STATS_PROTOCOL,
	.owner = THIS_MODULE,
	.init_instance = &scmi_cpufreq_stats_protocol_init,
	.ops = &cpufreq_stats_config_ops,
};
module_scmi_protocol(scmi_cpufreq_stats);

MODULE_DESCRIPTION("SCMI CPUFREQ_STATS vendor Protocol");
MODULE_LICENSE("GPL v2");
