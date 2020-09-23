// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

static struct llcc_slice_config lahaina_data[] =  {
	SCT_ENTRY(LLCC_CPUSS,    1, 3072, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 1),
	SCT_ENTRY(LLCC_VIDSC0,   2, 512, 3, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_AUDIO,    6, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0),
	SCT_ENTRY(LLCC_MDMHPGRW, 7, 1024, 3, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_MDMHW,    9, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_CMPT,     10, 3072, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_GPUHTW,   11, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_GPU,      12, 1024, 1, 0, 0xFFF, 0x0, 0, 0, 0, 1, 1, 0),
	SCT_ENTRY(LLCC_MMUHWT,   13, 1024, 1, 1, 0xFFF,  0x0, 0, 0, 0, 0, 0, 1),
	SCT_ENTRY(LLCC_DISP,     16, 3072, 2, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_MDMPNG,   21, 1024, 0, 1, 0xF,  0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_AUDHW,    22, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_CVP,      28, 512, 3, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_MDMVPE,   29, 256, 1, 1, 0xF,  0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_APTCM,    30, 1024, 3, 1, 0x0,  0x1, 1, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_WRTCH,    31, 512, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 1),
	SCT_ENTRY(LLCC_CVPFW,    17, 512, 1, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_CPUSS1,   3, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0),
	SCT_ENTRY(LLCC_CPUHWT,   5, 512, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 1),
};

static int lahaina_qcom_llcc_probe(struct platform_device *pdev)
{
	return qcom_llcc_probe(pdev, lahaina_data,
				 ARRAY_SIZE(lahaina_data));
}

static const struct of_device_id lahaina_qcom_llcc_of_match[] = {
	{ .compatible = "qcom,lahaina-llcc" },
	{ },
};

static struct platform_driver lahaina_qcom_llcc_driver = {
	.driver = {
		.name = "lahaina-llcc",
		.of_match_table = lahaina_qcom_llcc_of_match,
	},
	.probe = lahaina_qcom_llcc_probe,
};
module_platform_driver(lahaina_qcom_llcc_driver);

MODULE_DESCRIPTION("QCOM lahaina LLCC driver");
MODULE_LICENSE("GPL v2");
