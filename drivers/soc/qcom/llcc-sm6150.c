// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018,2020 The Linux Foundation. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>

/*
 * SCT entry contains of the following parameters
 * name: Name of the client's use case for which the llcc slice is used
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
 * retain_on_pc: If this bit is set and client has maitained active vote
 *               then the ways assigned to this client are not flushed on power
 *               collapse.
 * activate_on_init: Activate the slice immidiately after the SCT is programmed
 */
#define SCT_ENTRY(uid, sid, mc, p, fs, bway, rway, cmod, ptw, dca, wse, rp, a) \
{	.usecase_id = uid,              \
	.slice_id = sid,                \
	.max_cap = mc,                  \
	.priority = p,                  \
	.fixed_size = fs,               \
	.bonus_ways = bway,             \
	.res_ways = rway,               \
	.cache_mode = cmod,             \
	.probe_target_ways = ptw,       \
	.dis_cap_alloc = dca,           \
	.write_scid_en = wse,           \
	.retain_on_pc = rp,             \
	.activate_on_init = a,          \
}

static struct llcc_slice_config sm6150_data[] =  {
	SCT_ENTRY(LLCC_CPUSS,    1, 1, 128, 1, 0, 0xF, 0x0, 0, 0, 0, 1, 1),
	SCT_ENTRY(LLCC_MDM,      8, 8, 256, 0, 1, 0xF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_GPUHTW,   11, 11, 128, 1, 1, 0xF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_GPU,     12, 12, 128, 1, 0, 0xF, 0x0, 0, 0, 0, 1, 0),
};

static int sm6150_qcom_llcc_probe(struct platform_device *pdev)
{
	return qcom_llcc_probe(pdev, sm6150_data,
				 ARRAY_SIZE(sm6150_data));
}

static const struct of_device_id sm6150_qcom_llcc_of_match[] = {
	{ .compatible = "qcom,sm6150-llcc", },
	{ },
};

static struct platform_driver sm6150_qcom_llcc_driver = {
	.driver = {
		.name = "sm6150-llcc",
		.of_match_table = sm6150_qcom_llcc_of_match,
	},
	.probe = sm6150_qcom_llcc_probe,
	.remove = qcom_llcc_remove,
};

static int __init sm6150_init_qcom_llcc_init(void)
{
	return platform_driver_register(&sm6150_qcom_llcc_driver);
}
module_init(sm6150_init_qcom_llcc_init);

static void __exit sm6150_exit_qcom_llcc_exit(void)
{
	platform_driver_unregister(&sm6150_qcom_llcc_driver);
}
module_exit(sm6150_exit_qcom_llcc_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc sm6150 LLCC driver");
MODULE_LICENSE("GPL v2");
