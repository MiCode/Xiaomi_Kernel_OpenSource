/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_HW_PTP_H_
#define _ATL_HW_PTP_H_

#include "atl_hw.h"

struct atl_rx_desc_hwts_wb;

void hw_atl_get_ptp_ts(struct atl_hw *hw, u64 *stamp);
int hw_atl_adj_sys_clock(struct atl_hw *hw, s64 delta);
int hw_atl_ts_to_sys_clock(struct atl_hw *hw, u64 ts, u64 *time);
int hw_atl_adj_clock_freq(struct atl_hw *hw, s32 ppb);
int hw_atl_gpio_pulse(struct atl_hw *hw, u32 index, u64 start, u32 period);
int hw_atl_extts_gpio_enable(struct atl_hw *hw, u32 index, u32 enable);
int hw_atl_get_sync_ts(struct atl_hw *hw, u64 *ts);
u16 hw_atl_rx_extract_ts(struct atl_hw *hw, u8 *p, unsigned int len,
			 u64 *timestamp);
void hw_atl_extract_hwts(struct atl_hw *hw, struct atl_rx_desc_hwts_wb *hwts_wb,
			 u64 *timestamp);

#endif
