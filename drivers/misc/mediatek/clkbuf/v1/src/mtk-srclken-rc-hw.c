// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mtk_clkbuf_common.h"
#include "mtk-srclken-rc-hw.h"
#if IS_ENABLED(CONFIG_MTK_SRCLKEN_RC_V1)
#include "mtk-srclken-rc-hw-v1.h"
#endif /* IS_ENABLED(CONFIG_MTK_SRCLKEN_RC_V1) */

#define SRCLKEN_RC_ENABLE_PROP_NAME		"mediatek,enable"
#define SRCLKEN_RC_SUBSYS_PROP_NAME		"mediatek,subsys-ctl"

#define SRCLKEN_RC_SUBSYS_CTL_LEN		20

struct srclken_rc_hw rc_hw;

u8 srclken_rc_get_subsys_count(void)
{
	return rc_hw.subsys_num;
}

const char *srclken_rc_get_subsys_name(u8 idx)
{
	if (idx > rc_hw.subsys_num)
		return NULL;

	return rc_hw.subsys[idx].name;
}

int srclken_rc_subsys_ctrl(u8 idx, const char *mode)
{
	if (idx >= rc_hw.subsys_num)
		return -EINVAL;

	if (!strcmp(mode, "HW"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_HW, RC_NONE_REQ);
	else if (!strcmp(mode, "SW"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_NONE_REQ);
	else if (!strcmp(mode, "SW_OFF"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_LPM_REQ);
	else if (!strcmp(mode, "SW_FPM"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_FPM_REQ);
	else if (!strcmp(mode, "SW_BBLPM"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_BBLPM_REQ);
	else if (!strcmp(mode, "SW_LPM"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_SW, RC_LPM_VOTE_REQ);
	else if (!strcmp(mode, "INIT"))
		return __srclken_rc_subsys_ctrl(&rc_hw.subsys[idx],
			CLKBUF_CMD_INIT, RC_NONE_REQ);

	pr_notice("invalid mode: %s\n", mode);
	return -EPERM;
}

static int srclken_rc_dts_subsys_callback_init(struct device_node *node,
		struct srclken_rc_subsys *subsys)
{
	const char *str = NULL;
	char subsys_ctl[SRCLKEN_RC_SUBSYS_CTL_LEN];
	u8 i;

	snprintf(subsys_ctl, SRCLKEN_RC_SUBSYS_CTL_LEN, "%s-ctl", subsys->name);

	if (of_property_read_string(node, subsys_ctl, &str)) {
		pr_debug("no subsys_ctl node: %s found, skip\n", subsys_ctl);
		return 0;
	}

	for (i = 0; i < clk_buf_get_xo_num(); i++) {
		if (!strcmp(str, clk_buf_get_xo_name(i))) {
			__srclken_rc_xo_buf_callback_init(&subsys->xo_buf_ctl);
			clk_buf_register_xo_ctl_op(str, &subsys->xo_buf_ctl);
			return 0;
		}
	}

	pr_notice("cannot find subsys: %s to register %s callback\n",
		subsys->name, str);
	return -EXO_NOT_FOUND;
}

static int srclken_rc_dts_subsys_init(struct platform_device *pdev,
		struct device_node *node)
{
	int ret = 0;
	int i;

	ret = of_property_count_strings(node, SRCLKEN_RC_SUBSYS_PROP_NAME);
	if (ret <= 0) {
		pr_notice("get subsys count failed, ret: %d, hw count: %d\n",
			ret, rc_hw.subsys_num);
		return -EINVAL;
	}
	rc_hw.subsys_num = ret;

	rc_hw.subsys = devm_kmalloc(&pdev->dev,
			sizeof(struct srclken_rc_subsys) * rc_hw.subsys_num,
			GFP_KERNEL);
	if (!rc_hw.subsys) {
		pr_notice("allocate subsys name failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < rc_hw.subsys_num; i++) {
		ret = of_property_read_string_index(node,
				SRCLKEN_RC_SUBSYS_PROP_NAME,
				i, &rc_hw.subsys[i].name);
		if (ret) {
			pr_notice("get subsys name failed, idx: %d\n", i);
			return ret;
		}
		rc_hw.subsys[i].idx = i;
		srclken_rc_dts_subsys_callback_init(node, &rc_hw.subsys[i]);
	}

	return ret;
}

static int srclken_rc_dts_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	if (!of_property_read_bool(node, ENABLE_PROP_NAME)) {
		pr_notice("srclken_rc not enabled at dts\n");
		return -EHW_NOT_SUPPORT;
	}

	ret = srclken_rc_dts_subsys_init(pdev, node);
	if (ret)
		return ret;

	return ret;
}

static int mtk_srclken_rc_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = srclken_rc_dts_init(pdev);
	if (ret) {
		pr_notice("dts init failed with err: %d\n", ret);
		goto RC_INIT_FAILED;
	}

	if (srclken_rc_hw_init(pdev)) {
		pr_notice("srclken_rc hw init failed\n");
		goto RC_INIT_FAILED;
	}

	srclken_rc_init_done_callback(RC_INIT_DONE);

	return 0;

RC_INIT_FAILED:
	srclken_rc_init_done_callback(ret);

	return ret;
}

int srclken_rc_post_init(void)
{
	int ret = 0;
	u32 i;

	for (i = 0; i < rc_hw.subsys_num; i++) {
		ret = srclken_rc_get_subsys_req_mode(i,
				&rc_hw.subsys[i].init_mode);
		if (ret) {
			pr_notice("srclken_rc get subsys req mode failed\n");
			return ret;
		}

		ret = srclken_rc_get_subsys_sw_req(i, &rc_hw.subsys[i].init_req);
		if (ret) {
			pr_notice("srclken_rc get subsys sw req failed\n");
			return ret;
		}
	}

	return ret;
}

static const struct platform_device_id mtk_srclken_rc_ids[] = {
	{"mtk-srclken-rc", 0},
	{ /*sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mtk_srclken_rc_ids);

static const struct of_device_id mtk_srclken_rc_of_match[] = {
	{
		.compatible = "mediatek,srclken-rc",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_srclken_rc_of_match);

static struct platform_driver mtk_srclken_rc_driver = {
	.driver = {
		.name = "mtk-srclken-rc",
		.of_match_table = of_match_ptr(mtk_srclken_rc_of_match),
	},
	.probe = mtk_srclken_rc_probe,
	.id_table = mtk_srclken_rc_ids,
};

int srclken_rc_init(void)
{
	return platform_driver_register(&mtk_srclken_rc_driver);
}

void srclken_rc_exit(void)
{
	platform_driver_unregister(&mtk_srclken_rc_driver);
}

MODULE_AUTHOR("Ren-Ting Wang <ren-ting.wang@mediatek.com");
MODULE_DESCRIPTION("SOC Driver for MediaTek SRCLKEN_RC");
MODULE_LICENSE("GPL v2");
