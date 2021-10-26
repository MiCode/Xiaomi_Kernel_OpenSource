/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _MDLA_V2_0_HW_REG_H__
#define _MDLA_V2_0_HW_REG_H__

#include "mdla_hw_apu_dla_v2_0.h"
#include "mdla_hw_cmde_v2_0.h"
#include "mdla_hw_biu_v2_0.h"

#define MREG_CMD_SIZE      (0x1C0)

/**
 * MDLA_HAVE_reg_v1.xx
 * code buffer size = 0x1c0
 */
#define MREG_CMD_INPUT_ACTI_ADDR       (0x0)
#define MREG_CMD_INPUT_WT_ACTI_ADDR    (0x4)
#define MREG_CMD_OUTPUT_ACTI_ADDR      (0x8)
#define MREG_CMD_GENERAL_CTRL_0        (0x70)
#define MREG_CMD_TILE_CNT_INT          (0x154)
#define MREG_CMD_GENERAL_CTRL_1        (0x15C)


/* Register fields */

/* MREG_CMD_GENERAL_CTRL_0 : 0x70 */
#define MREG_CMD_LAYER_END             BIT(12)
#define MREG_CMD_AXI_MAX_BURST_LEN     BIT(25)

/* MREG_CMD_GENERAL_CTRL_1 : 0x15C */
#define MREG_CMD_INT_SWCMD_DONE        BIT(15)
#define MREG_CMD_WAIT_SWCMD_DONE       BIT(16)

/* MREG_CMD_TILE_CNT_INT : 0x154 */
#define MREG_CMD_SWCMD_FINISH_INT_EN   BIT(24)

#endif /* _MDLA_V2_0_HW_REG_H__ */
