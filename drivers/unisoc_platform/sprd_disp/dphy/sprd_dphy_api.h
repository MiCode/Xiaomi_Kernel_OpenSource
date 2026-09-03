/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DPHY_API_H_
#define _SPRD_DPHY_API_H_

#include "sprd_dphy.h"

int sprd_dphy_init(struct sprd_dphy *dphy);
void sprd_dphy_fini(struct sprd_dphy *dphy);
void sprd_dphy_reset(struct sprd_dphy *dphy);
void sprd_dphy_shutdown(struct sprd_dphy *dphy);
int sprd_dphy_hop_config(struct sprd_dphy *dphy, int delta, int period);
int sprd_dphy_ssc_en(struct sprd_dphy *dphy, bool en);
void sprd_dphy_data_ulps_enter(struct sprd_dphy *dphy);
void sprd_dphy_data_ulps_exit(struct sprd_dphy *dphy);
void sprd_dphy_clk_ulps_enter(struct sprd_dphy *dphy);
void sprd_dphy_clk_ulps_exit(struct sprd_dphy *dphy);
void sprd_dphy_force_pll(struct sprd_dphy *dphy, bool enable);
void sprd_dphy_hs_clk_en(struct sprd_dphy *dphy, bool enable);
void sprd_dphy_test_write(struct sprd_dphy *dphy, u8 address, u8 data);
u8 sprd_dphy_test_read(struct sprd_dphy *dphy, u8 address);
void sprd_dphy_ulps_enter(struct sprd_dphy *dphy);
void sprd_dphy_ulps_exit(struct sprd_dphy *dphy);

#endif /* _SPRD_DPHY_API_H_ */