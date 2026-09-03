/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DPHY_H_
#define _SPRD_DPHY_H_

#include <asm/types.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <drm/drm_print.h>

#include "disp_lib.h"

struct dphy_context {
	struct regmap *regmap;
	unsigned long ctrlbase;
	unsigned long apbbase;
	struct mutex lock;
	bool enabled;
	u8 aod_mode;
	u32 freq;
	u8 lanes;
	bool ulps_enable;
	u8 id;
	u8 capability;
	u32 chip_id;
};

struct dphy_pll_ops {
	int (*pll_config)(struct dphy_context *ctx);
	int (*timing_config)(struct dphy_context *ctx);
	int (*hop_config)(struct dphy_context *ctx, int delta, int period);
	int (*ssc_en)(struct dphy_context *ctx, bool en);
	void (*force_pll)(struct dphy_context *ctx, int force);
};

struct dphy_ppi_ops {
	void (*rstz)(struct dphy_context *ctx, int level);
	void (*shutdownz)(struct dphy_context *ctx, int level);
	void (*force_pll)(struct dphy_context *ctx, int force);
	void (*clklane_ulps_rqst)(struct dphy_context *ctx, int en);
	void (*clklane_ulps_exit)(struct dphy_context *ctx, int en);
	void (*datalane_ulps_rqst)(struct dphy_context *ctx, int en);
	void (*datalane_ulps_exit)(struct dphy_context *ctx, int en);
	void (*stop_wait_time)(struct dphy_context *ctx, u8 byte_clk);
	void (*datalane_en)(struct dphy_context *ctx);
	void (*clklane_en)(struct dphy_context *ctx, int en);
	void (*clk_hs_rqst)(struct dphy_context *ctx, int en);
	u8 (*is_rx_direction)(struct dphy_context *ctx);
	u8 (*is_pll_locked)(struct dphy_context *ctx);
	u8 (*is_rx_ulps_esc_lane0)(struct dphy_context *ctx);
	u8 (*is_stop_state_clklane)(struct dphy_context *ctx);
	u8 (*is_stop_state_datalane)(struct dphy_context *ctx);
	u8 (*is_ulps_active_datalane)(struct dphy_context *ctx);
	u8 (*is_ulps_active_clklane)(struct dphy_context *ctx);
	void (*tst_clk)(struct dphy_context *ctx, u8 level);
	void (*tst_clr)(struct dphy_context *ctx, u8 level);
	void (*tst_en)(struct dphy_context *ctx, u8 level);
	u8 (*tst_dout)(struct dphy_context *ctx);
	void (*tst_din)(struct dphy_context *ctx, u8 data);
	void (*bist_en)(struct dphy_context *ctx, int en);
	u8 (*is_bist_ok)(struct dphy_context *ctx);
};

struct dphy_glb_ops {
	int (*parse_dt)(struct dphy_context *ctx,
			struct device_node *np);
	void (*enable)(struct dphy_context *ctx);
	void (*disable)(struct dphy_context *ctx);
	void (*power)(struct dphy_context *ctx, int enable);
};

struct sprd_dphy_ops {
	const struct dphy_ppi_ops *ppi;
	const struct dphy_pll_ops *pll;
	const struct dphy_glb_ops *glb;
};

struct sprd_dphy {
	struct device dev;
	struct dphy_context ctx;
	const struct dphy_ppi_ops *ppi;
	const struct dphy_pll_ops *pll;
	const struct dphy_glb_ops *glb;
};

int sprd_dphy_enable(struct sprd_dphy *dphy);
int sprd_dphy_disable(struct sprd_dphy *dphy);

extern const struct dphy_ppi_ops dsi_ctrl_ppi_ops;
extern const struct dphy_glb_ops pike2_dphy_glb_ops;
extern const struct dphy_pll_ops sharkle_dphy_pll_ops;
extern const struct dphy_glb_ops sharkle_dphy_glb_ops;
extern const struct dphy_glb_ops sharkl3_dphy_glb_ops;
extern const struct dphy_pll_ops sharkl5_dphy_pll_ops;
extern const struct dphy_glb_ops sharkl5_dphy_glb_ops;
extern const struct dphy_glb_ops sharkl5pro_dphy_glb_ops;
extern const struct dphy_glb_ops qogirl6_dphy_glb_ops;
extern const struct dphy_glb_ops qogirn6pro_dphy_glb_ops;
#endif /* _SPRD_DPHY_H_ */
