/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_DEBUG_H
#define APU_DEBUG_H


extern struct mtk_apu *g_apu_struct;
extern uint32_t g_apu_log;

#define apu_drv_debug(x, ...) do { if (g_apu_log) \
		dev_info(g_apu_struct->dev, \
		"[debug] %s: " x, __func__, ##__VA_ARGS__); \
		} while (0)

#endif /* APU_DEBUG_H */
