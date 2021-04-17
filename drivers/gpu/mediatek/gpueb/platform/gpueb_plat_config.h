// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_PLAT_CONFIG_H__
#define __GPUEB_PLAT_CONFIG_H__

// For MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>
// For GPUEB reserve memory
#include "gpueb_common_reserved_mem.h"

// For platform config check
extern bool gpueb_plat_is_bringup(void);
extern bool gpueb_plat_is_logger_support(void);
extern bool gpueb_plat_is_ipi_test_support(void);
// For MBOX IPI
extern struct mtk_mbox_device   gpueb_plat_mboxdev;
extern struct mtk_ipi_device    gpueb_plat_ipidev;
extern struct mtk_mbox_info     gpueb_plat_mbox_table[];
extern struct mtk_mbox_pin_send gpueb_plat_mbox_pin_send[];
extern struct mtk_mbox_pin_recv gpueb_plat_mbox_pin_recv[];
extern int gpueb_plat_ipi_send_testing(void);
extern int gpueb_plat_get_channelID_by_name(char *channel_name);
// For GPUEB reserve memory
extern struct gpueb_reserve_mblock gpueb_reserve_mblock_ary[];

#endif /* __GPUEB_PLAT_CONFIG_H__ */