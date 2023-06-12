/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef BOLERO_CLK_RSC_H
#define BOLERO_CLK_RSC_H

#include <linux/regmap.h>
#include <dt-bindings/sound/qcom,bolero-clk-rsc.h>

#if IS_ENABLED(CONFIG_SND_SOC_BOLERO)
int bolero_clk_rsc_mgr_init(void);
void bolero_clk_rsc_mgr_exit(void);
void bolero_clk_rsc_fs_gen_request(struct device *dev,
						bool enable);
int bolero_clk_rsc_request_clock(struct device *dev,
				int default_clk_id,
				int clk_id_req,
				bool enable);
int bolero_rsc_clk_reset(struct device *dev, int clk_id);
void bolero_clk_rsc_enable_all_clocks(struct device *dev, bool enable);
#else
static inline void bolero_clk_rsc_fs_gen_request(struct device *dev,
						bool enable)
{
}
static inline int bolero_clk_rsc_mgr_init(void)
{
	return 0;
}
static inline void bolero_clk_rsc_mgr_exit(void)
{
}
static inline int bolero_clk_rsc_request_clock(struct device *dev,
				int default_clk_id,
				int clk_id_req,
				bool enable)
{
	return 0;
}
static inline int bolero_rsc_clk_reset(struct device *dev, int clk_id)
{
	return 0;
}
static inline void bolero_clk_rsc_enable_all_clocks(struct device *dev,
						    bool enable)
{
	return;
}
#endif /* CONFIG_SND_SOC_BOLERO */
#endif /* BOLERO_CLK_RSC_H */
