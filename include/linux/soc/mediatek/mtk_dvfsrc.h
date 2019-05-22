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

#define MTK_DVFSRC_CMD_VCORE_QUERY	0
#define MTK_DVFSRC_CMD_VCP_QUERY	1

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

#endif /* CONFIG_MTK_DVFSRC */

#endif
