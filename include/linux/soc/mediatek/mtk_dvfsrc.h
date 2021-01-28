/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SOC_MTK_DVFSRC_H
#define __SOC_MTK_DVFSRC_H

#define MTK_DVFSRC_CMD_BW_REQUEST	0
#define MTK_DVFSRC_CMD_OPP_REQUEST	1
#define MTK_DVFSRC_CMD_VCORE_REQUEST	2
#define MTK_DVFSRC_CMD_HRTBW_REQUEST	3
#define MTK_DVFSRC_CMD_VSCP_REQUEST	4
#define MTK_DVFSRC_CMD_FORCE_OPP_REQUEST	5
#define MTK_DVFSRC_CMD_DRAM_REQUEST	7
#define MTK_DVFSRC_CMD_EXT_DRAM_REQUEST	8
#define MTK_DVFSRC_CMD_EXT_BW_REQUEST	9

#define MTK_DVFSRC_CMD_VCORE_QUERY	0
#define MTK_DVFSRC_CMD_VCP_QUERY	1
#define MTK_DVFSRC_CMD_CURR_LEVEL_QUERY	2

#define MTK_SIP_SPM_DVFSRC_INIT 0

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data);
int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd, int *data);
#else
static inline void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd,
						u64 data)
{ }
static inline int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd,
						int *data)
{ return -EINVAL; }
#endif

static inline struct device_node *dvfsrc_parse_required_opp(
	struct device_node *np, int index)
{
	struct device_node *required_np;

	required_np = of_parse_phandle(np, "required-opps", index);
	if (unlikely(!required_np)) {
		pr_notice("%s: Unable to parse required-opps: %pOF, index: %d\n",
		       __func__, np, index);
	}

	return required_np;
}

static inline int dvfsrc_get_required_opp_performance_state(
	struct device_node *np, int index)
{
	struct device_node *required_np;
	int pstate = -EINVAL;

	required_np = dvfsrc_parse_required_opp(np, index);
	if (!required_np)
		return -EINVAL;

	of_property_read_u32(required_np, "opp-level", &pstate);
	of_node_put(required_np);

	return pstate;
}
#endif/* CONFIG_MTK_DVFSRC */

