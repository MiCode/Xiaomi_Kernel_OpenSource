// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/sysmon_subsystem_stats.h>

#define SYSMON_SMEM_ID					634
#define SYSMON_POWER_STATS_VERSIONID			0x1
#define SYSMON_POWER_STATS_FEATUREID			0x3

/**
 * sysmon_stats_query_power_residency() - * API to query requested
 * DSP subsystem power residency.On success, returns power residency
 * statistics in the given sysmon_smem_power_stats structure.
 */
int sysmon_stats_query_power_residency(enum dsp_id_t dsp_id,
					struct sysmon_smem_power_stats *sysmon_power_stats)
{
	u32 featureId;
	int err = 0;
	int feature, size_rcvd;
	size_t size;
	char *smem_pointer = NULL;
	int size_of_u32 = sizeof(u32);

	if (!sysmon_power_stats || !dsp_id) {
		pr_err("%s: Null pointer received/invalid dsp ID\n", __func__);
		return -EINVAL;
	}

	smem_pointer = qcom_smem_get(dsp_id, SYSMON_SMEM_ID, &size);

	if (IS_ERR_OR_NULL(smem_pointer) || !size) {
		pr_err("Failed to fetch data from SMEM for ID %d: %d\n",
				dsp_id, PTR_ERR(smem_pointer));
		return -ENOMEM;
	}

	featureId = *(unsigned int *)smem_pointer;

	feature = featureId >> 28;
	size_rcvd = (featureId >> 16) & 0xFFF;

	while (size != 0) {
		switch (feature) {

		case SYSMON_POWER_STATS_FEATUREID:

			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				memcpy(sysmon_power_stats, (smem_pointer + size_of_u32),
						sizeof(struct sysmon_smem_power_stats));
				size = 0;
			} else {
				pr_err("%s: Requested feature not found\n", __func__);
				size = 0;
				return -ENOMEM;
			}
		break;
		default:

			if (!IS_ERR_OR_NULL(smem_pointer + size_rcvd)
					 && (size > size_rcvd)) {

				featureId = *(unsigned int *)(smem_pointer + size_rcvd);

				smem_pointer += size_rcvd;
				size = size - size_rcvd;

				feature = featureId >> 28;
				size_rcvd = (featureId >> 16) & 0xFFF;
			} else {
				pr_err("%s: Requested feature not found\n", __func__);
				size = 0;
				return -ENOMEM;
			}
		break;
		}
	}
	return err;
}
EXPORT_SYMBOL(sysmon_stats_query_power_residency);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Sysmon subsystem Stats driver");
