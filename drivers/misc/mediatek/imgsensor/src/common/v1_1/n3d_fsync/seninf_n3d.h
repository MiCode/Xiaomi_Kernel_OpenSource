/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SENINF_N3D_H__
#define __SENINF_N3D_H__

#define SENINF_N3D_A_CTL 0x0000
#define MODE_SHIFT 0
#define MODE_MASK (0x3 << 0)
#define I2C1_EN_SHIFT 2
#define I2C1_EN_MASK (0x1 << 2)
#define I2C2_EN_SHIFT 3
#define I2C2_EN_MASK (0x1 << 3)
#define I2C1_INT_EN_SHIFT 4
#define I2C1_INT_EN_MASK (0x1 << 4)
#define I2C2_INT_EN_SHIFT 5
#define I2C2_INT_EN_MASK (0x1 << 5)
#define N3D_EN_SHIFT 6
#define N3D_EN_MASK (0x1 << 6)
#define W1CLR_SHIFT 7
#define W1CLR_MASK (0x1 << 7)
#define DIFF_EN_SHIFT 8
#define DIFF_EN_MASK (0x1 << 8)
#define DDBG_SEL_SHIFT 9
#define DDBG_SEL_MASK (0x7 << 9)
#define MODE1_DBG_SHIFT 12
#define MODE1_DBG_MASK (0x1 << 12)
#define SEN1_TIM_EN_SHIFT 16
#define SEN1_TIM_EN_MASK (0x1 << 16)
#define SEN2_TIM_EN_SHIFT 17
#define SEN2_TIM_EN_MASK (0x1 << 17)
#define SEN1_OV_VS_INT_EN_SHIFT 18
#define SEN1_OV_VS_INT_EN_MASK (0x1 << 18)
#define SEN2_OV_VS_INT_EN_SHIFT 19
#define SEN2_OV_VS_INT_EN_MASK (0x1 << 19)
#define HW_SYNC_MODE_SHIFT 20
#define HW_SYNC_MODE_MASK (0x1 << 20)
#define VALID_TG_EN_SHIFT 21
#define VALID_TG_EN_MASK (0x1 << 21)
#define SYNC_PIN_A_EN_SHIFT 22
#define SYNC_PIN_A_EN_MASK (0x1 << 22)
#define SYNC_PIN_A_POLARITY_SHIFT 23
#define SYNC_PIN_A_POLARITY_MASK (0x1 << 23)
#define SYNC_PIN_B_EN_SHIFT 24
#define SYNC_PIN_B_EN_MASK (0x1 << 24)
#define SYNC_PIN_B_POLARITY_SHIFT 25
#define SYNC_PIN_B_POLARITY_MASK (0x1 << 25)

#define SENINF_N3D_A_POS 0x0004
#define N3D_POS_SHIFT 0
#define N3D_POS_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_TRIG 0x0008
#define I2CA_TRIG_SHIFT 0
#define I2CA_TRIG_MASK (0x1 << 0)
#define I2CB_TRIG_SHIFT 1
#define I2CB_TRIG_MASK (0x1 << 1)

#define SENINF_N3D_A_INT 0x000C
#define I2C1_INT_SHIFT 0
#define I2C1_INT_MASK (0x1 << 0)
#define I2C2_INT_SHIFT 1
#define I2C2_INT_MASK (0x1 << 1)
#define DIFF_INT_SHIFT 2
#define DIFF_INT_MASK (0x1 << 2)
#define SEN1_OV_VS_INT_SHIFT 4
#define SEN1_OV_VS_INT_MASK (0x1 << 4)
#define SEN2_OV_VS_INT_SHIFT 5
#define SEN2_OV_VS_INT_MASK (0x1 << 5)

#define SENINF_N3D_A_CNT0 0x0010
#define N3D_CNT0_SHIFT 0
#define N3D_CNT0_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_CNT1 0x0014
#define N3D_CNT1_SHIFT 0
#define N3D_CNT1_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_DBG 0x0018
#define N3D_DBG_SHIFT 0
#define N3D_DBG_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_DIFF_THR 0x001C
#define N3D_DIFF_THR_SHIFT 0
#define N3D_DIFF_THR_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_DIFF_CNT 0x0020
#define N3D_DIFF_CNT_SHIFT 0
#define N3D_DIFF_CNT_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_DBG_1 0x0024
#define N3D_DBG_1_SHIFT 0
#define N3D_DBG_1_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_VALID_TG_CNT 0x0028
#define N3D_VALID_TG_CNT_SHIFT 0
#define N3D_VALID_TG_CNT_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_SYNC_A_PERIOD 0x002C
#define N3D_SYNC_A_PERIOD_SHIFT 0
#define N3D_SYNC_A_PERIOD_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_SYNC_B_PERIOD 0x0030
#define N3D_SYNC_B_PERIOD_SHIFT 0
#define N3D_SYNC_B_PERIOD_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_SYNC_A_PULSE_LEN 0x0034
#define N3D_SYNC_A_PULSE_LEN_SHIFT 0
#define N3D_SYNC_A_PULSE_LEN_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_SYNC_B_PULSE_LEN 0x0038
#define N3D_SYNC_B_PULSE_LEN_SHIFT 0
#define N3D_SYNC_B_PULSE_LEN_MASK (0xFFFFFFFF << 0)

#define SENINF_N3D_A_SUB_CNT 0x003C
#define VS1_SUB_CNT_SHIFT 0
#define VS1_SUB_CNT_MASK (0x3F << 0)
#define VS1_SUB_CNT_EN_SHIFT 6
#define VS1_SUB_CNT_EN_MASK (0x1 << 6)
#define SYNC_PIN_A_RESET_SEL_SHIFT 7
#define SYNC_PIN_A_RESET_SEL_MASK (0x1 << 7)
#define SYNC_PIN_B_RESET_SEL_SHIFT 8
#define SYNC_PIN_B_RESET_SEL_MASK (0x1 << 8)
#define SYNC_PIN_A_RESET_SEL_EN_SHIFT 9
#define SYNC_PIN_A_RESET_SEL_EN_MASK (0x1 << 9)
#define SYNC_PIN_B_RESET_SEL_EN_SHIFT 10
#define SYNC_PIN_B_RESET_SEL_EN_MASK (0x1 << 10)
#define VS2_SUB_CNT_SHIFT 16
#define VS2_SUB_CNT_MASK (0x3F << 16)
#define VS2_SUB_CNT_EN_SHIFT 22
#define VS2_SUB_CNT_EN_MASK (0x1 << 22)

#define SENINF_N3D_A_VSYNC_CNT 0x0040
#define N3D_VSYNC_1_CNT_SHIFT 0
#define N3D_VSYNC_1_CNT_MASK (0xFFFF << 0)
#define N3D_VSYNC_2_CNT_SHIFT 16
#define N3D_VSYNC_2_CNT_MASK (0xFFFF << 16)

#endif

