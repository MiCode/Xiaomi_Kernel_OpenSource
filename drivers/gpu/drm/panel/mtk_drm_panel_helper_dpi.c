// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_panel_helper.h"

int parse_lcm_params_dpi(struct device_node *np,
		struct mtk_lcm_params_dpi *params)
{
	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np))
		return -EINVAL;

	memset(params, 0, sizeof(struct mtk_lcm_params_dpi));
	return 0;
}
EXPORT_SYMBOL(parse_lcm_params_dpi);

int parse_lcm_ops_dpi(struct device_node *np,
		struct mtk_lcm_ops_dpi *ops,
		struct mtk_lcm_params_dpi *params,
		struct mtk_panel_cust *cust)
{
	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np))
		return -EINVAL;

	memset(ops, 0, sizeof(struct mtk_lcm_ops_dpi));
	return 0;
}
EXPORT_SYMBOL(parse_lcm_ops_dpi);

void dump_lcm_params_dpi(struct mtk_lcm_params_dpi *params,
	struct mtk_panel_cust *cust)
{
}
EXPORT_SYMBOL(dump_lcm_params_dpi);

void dump_lcm_ops_dpi(struct mtk_lcm_ops_dpi *ops,
		struct mtk_lcm_params_dpi *params,
		struct mtk_panel_cust *cust)
{
}
EXPORT_SYMBOL(dump_lcm_ops_dpi);

void free_lcm_params_dpi(struct mtk_lcm_params_dpi *params)
{
}
EXPORT_SYMBOL(free_lcm_params_dpi);

void free_lcm_ops_dpi(struct mtk_lcm_ops_dpi *ops)
{
	LCM_KFREE(ops, sizeof(struct mtk_lcm_ops_dpi));
}
EXPORT_SYMBOL(free_lcm_ops_dpi);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dsi helper");
MODULE_LICENSE("GPL v2");
