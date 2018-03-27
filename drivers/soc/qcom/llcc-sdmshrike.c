/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
 *                    configured to 1 only bonus and reseved ways are probed.
 *                    when configured to 0 all ways in llcc are probed.
 * dis_cap_alloc: Disable capacity based allocation for a client
 * retain_on_pc: If this bit is set and client has maitained active vote
 *               then the ways assigned to this client are not flushed on power
 *               collapse.
 * activate_on_init: Activate the slice immidiately after the SCT is programmed
 */
#define SCT_ENTRY(n, uid, sid, mc, p, fs, bway, rway, cmod, ptw, dca, rp, a) \
	{					\
		.name = n,			\
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
		.retain_on_pc = rp,		\
		.activate_on_init = a,		\
	}

static struct llcc_slice_config sdmshrike_data[] =  {
	SCT_ENTRY("cpuss",       1, 1, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 1),
	SCT_ENTRY("vidsc0",      2, 2, 512, 2, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("vidsc1",      3, 3, 512, 2, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("rotator",     4, 4, 1024, 2, 1, 0xFFF, 0x0, 2, 0, 0, 1, 0),
	SCT_ENTRY("voice",       5, 5, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("audio",       6, 6, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("modemhp_grow", 7, 7, 1024, 2, 0, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("modem",       8, 8, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("compute",     10, 10, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("gpuhtw",      11, 11, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("gpu",         12, 12, 5120, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("mmuhwt",      13, 13, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1),
	SCT_ENTRY("compute_dma", 15, 15, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("display",     16, 16, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("videofw",     17, 17, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("modemhp_fix", 20, 20, 1024, 2, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("modem_paging", 21, 21, 1024, 0, 1, 0xF,  0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("audiohw",     22, 22, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("npu",         23, 23, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("wlan_hw",     24, 24, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
	SCT_ENTRY("pimem",       25, 25, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 1, 0),
};

static int sdmshrike_qcom_llcc_probe(struct platform_device *pdev)
{
	return qcom_llcc_probe(pdev, sdmshrike_data,
				 ARRAY_SIZE(sdmshrike_data));
}

static const struct of_device_id sdmshrike_qcom_llcc_of_match[] = {
	{ .compatible = "qcom,sdmshrike-llcc", },
	{ },
};

static struct platform_driver sdmshrike_qcom_llcc_driver = {
	.driver = {
		.name = "sdmshrike-llcc",
		.owner = THIS_MODULE,
		.of_match_table = sdmshrike_qcom_llcc_of_match,
	},
	.probe = sdmshrike_qcom_llcc_probe,
	.remove = qcom_llcc_remove,
};

static int __init sdmshrike_init_qcom_llcc_init(void)
{
	return platform_driver_register(&sdmshrike_qcom_llcc_driver);
}
module_init(sdmshrike_init_qcom_llcc_init);

static void __exit sdmshrike_exit_qcom_llcc_exit(void)
{
	platform_driver_unregister(&sdmshrike_qcom_llcc_driver);
}
module_exit(sdmshrike_exit_qcom_llcc_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc SDMSHRIKE LLCC driver");
MODULE_LICENSE("GPL v2");
