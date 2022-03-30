// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_panel_helper.h"

int parse_lcm_params_dbi(struct device_node *np,
		struct mtk_lcm_params_dbi *params)
{
	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np))
		return -EINVAL;

	memset(params, 0, sizeof(struct mtk_lcm_params_dbi));
	return 0;
}
EXPORT_SYMBOL(parse_lcm_params_dbi);

int parse_lcm_ops_dbi(struct device_node *np,
		struct mtk_lcm_ops_dbi *ops,
		struct mtk_lcm_params_dbi *params,
		struct mtk_panel_cust *cust)
{
	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np))
		return -EINVAL;

	memset(ops, 0, sizeof(struct mtk_lcm_ops_dbi));
	return 0;
}
EXPORT_SYMBOL(parse_lcm_ops_dbi);

void dump_lcm_params_dbi(struct mtk_lcm_params_dbi *params,
	struct mtk_panel_cust *cust)
{
}
EXPORT_SYMBOL(dump_lcm_params_dbi);

void dump_lcm_ops_dbi(struct mtk_lcm_ops_dbi *ops,
		struct mtk_lcm_params_dbi *params,
		struct mtk_panel_cust *cust)
{
}
EXPORT_SYMBOL(dump_lcm_ops_dbi);

void free_lcm_params_dbi(struct mtk_lcm_params_dbi *params)
{
}
EXPORT_SYMBOL(free_lcm_params_dbi);

void free_lcm_ops_dbi(struct mtk_lcm_ops_dbi *ops)
{
	LCM_KFREE(ops, sizeof(struct mtk_lcm_ops_dbi));
}
EXPORT_SYMBOL(free_lcm_ops_dbi);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dbi helper");
MODULE_LICENSE("GPL v2");
