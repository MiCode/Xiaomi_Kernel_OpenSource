/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk-srclken-rc.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_SRCLKEN_RC_H__
#define __MTK_SRCLKEN_RC_H__

#include <linux/kernel.h>
#include <linux/mutex.h>

void srclken_stage_init(void);
int srclken_dts_map(struct platform_device *pdev);
int srclken_fs_init(void);
enum srclken_config srclken_hw_get_stage(void);
void srclken_hw_dump_last_sta_log(void);
void srclken_hw_dump_cfg_log(void);
void srclken_hw_dump_sta_log(void);
bool srclken_get_debug_cfg(void);

#endif

