/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides macros for MT6375 GAUGE device bindings.
 *
 * Copyright (c) 2021 Mediatek Inc.
 * Author: ShuFan Lee <shufan_lee@richtek.com>
 */

#ifndef _DT_BINDINGS_POWER_MT6375_GAUGE_H
#define _DT_BINDINGS_POWER_MT6375_GAUGE_H

#define RG_INT_STATUS_FG_BAT_H		0
#define RG_INT_STATUS_FG_BAT_L		1
#define RG_INT_STATUS_FG_CUR_H		2
#define RG_INT_STATUS_FG_CUR_L		3
#define RG_INT_STATUS_FG_ZCV		4
#define RG_INT_STATUS_FG_N_CHARGE_L	7
#define RG_INT_STATUS_FG_IAVG_H		8
#define RG_INT_STATUS_FG_IAVG_L		9
#define RG_INT_STATUS_FG_DISCHARGE	11
#define RG_INT_STATUS_FG_CHARGE		12
#define RG_BM_INT_STATUS0_RSV		13
#define RG_INT_STATUS_BATON_LV		16
#define RG_INT_STATUS_BATON_BAT_IN	18
#define RG_INT_STATUS_BATON_BAT_OUT	19

#endif
