// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>

/*
 * SCT entry contains of the following parameters
 * uid: Unique id for the client's use case
 * slice_id: llcc slice id for each client
 * max_cap: The maximum capacity of the cache slice provided in KB
 * priority: Priority of the client used to select victim line for replacement
 * fixed_size: Determine of the slice has a fixed capacity
 * bonus_ways: Bonus ways to be used by any slice, bonus way is used only if
 *             it't not a reserved way.
 * res_ways: Reserved ways for the cache slice, the reserved ways cannot be used
 *           by any other client than the one its assigned to.
 * cache_mode: Each slice operates as a cache, this controls the mode of the
 *             slice normal or TCM
 * probe_target_ways: Determines what ways to probe for access hit. When
 *                    configured to 1 only bonus and reserved ways are probed.
 *                    when configured to 0 all ways in llcc are probed.
 * dis_cap_alloc: Disable capacity based allocation for a client
 * write_scid_en: Bit enables write cache support for a given scid.
 * retain_on_pc: If this bit is set and client has maitained active vote
 *               then the ways assigned to this client are not flushed on power
 *               collapse.
 * activate_on_init: Activate the slice immidiately after the SCT is programmed
 */
#define SCT_ENTRY(uid, sid, mc, p, fs, bway, rway, cmod, ptw, dca, wse, rp, a) \
	{					\
		.usecase_id = uid,		\
		.slice_id = sid,		\
		.max_cap = mc,			\
		.priority = p,			\
		.fixed_size = fs,		\
		.bonus_ways = bway,		\
		.res_ways = rway,		\
		.cache_mode = cmod,		\
		.probe_target_ways = ptw,	\
		.dis_cap_alloc = dca,		\
		.write_scid_en = wse,		\
		.retain_on_pc = rp,		\
		.activate_on_init = a,		\
	}

static struct llcc_slice_config yupik_data[] =  {
	SCT_ENTRY(LLCC_CPUSS,    1, 768, 1, 0, 0x3F, 0x0, 0, 0, 0, 0, 1, 1),
	SCT_ENTRY(LLCC_MDMHPGRW, 7, 512, 2, 1, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_CMPT,     10, 768, 1, 1, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_GPUHTW,   11, 256, 1, 1, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_GPU,      12, 512, 1, 0, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_MMUHWT,   13, 256, 1, 1, 0x3F, 0x0, 0, 0, 0, 0, 0, 1),
	SCT_ENTRY(LLCC_MDMPNG,   21, 768, 0, 1, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_WLNHW,    24, 256, 1, 1, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_MDMVPE,   29, 64,  1, 1, 0x3F, 0x0, 0, 0, 0, 0, 1, 0),
};

static int yupik_qcom_llcc_probe(struct platform_device *pdev)
{
	return qcom_llcc_probe(pdev, yupik_data,
				 ARRAY_SIZE(yupik_data));
}

static const struct of_device_id yupik_qcom_llcc_of_match[] = {
	{ .compatible = "qcom,yupik-llcc", },
	{ },
};

static struct platform_driver yupik_qcom_llcc_driver = {
	.driver = {
		.name = "yupik-llcc",
		.of_match_table = yupik_qcom_llcc_of_match,
	},
	.probe = yupik_qcom_llcc_probe,
};
module_platform_driver(yupik_qcom_llcc_driver);

MODULE_DESCRIPTION("QCOM yupik LLCC driver");
MODULE_LICENSE("GPL v2");
