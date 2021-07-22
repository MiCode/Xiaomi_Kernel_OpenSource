/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_SLBC_SRAM_H__
#define __MTK_SLBC_SRAM_H__

/*
 * BIT Operation
 */
#define SLBC_BIT(_bit_) \
	((unsigned int)(1U << (_bit_)))
#define SLBC_BITMASK(_bits_) \
	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) & \
	 ~((1U << ((0) ? _bits_)) - 1))
#define SLBC_BITS(_bits_, _val_) \
	(SLBC_BITMASK(_bits_) & \
	 ((_val_) << ((0) ? _bits_)))
#define SLBC_GET_BITS_VAL_0(_bits_, _val_) \
	(((_val_) & (SLBC_BITMASK(_bits_))) >> ((0) ? _bits_))
#define SLBC_NAME_VAL(_name_, _val_) \
	(_val_ << _name_##_SHIFT)
#define SLBC_GET_NAME_VAL(_reg_val_, _name_) \
	((_reg_val_) & (_name_##_MASK))
#define SLBC_GET_NAME_VAL_0(_reg_val_, _name_) \
	(((_reg_val_) & (_name_##_MASK)) >> _name_##_SHIFT)
#define SLBC_SET_NAME_VAL(_reg_val_, _name_, _val_) \
	(((_reg_val_) & ~(_name_##_MASK)) | \
	 ((_val_ << _name_##_SHIFT) & (_name_##_MASK)))

#define SLBC_UID_USED                   0x0
#define SLBC_SID_MASK                   0x4
#define SLBC_SID_REQ_Q                  0x8
#define SLBC_SID_REL_Q                  0xC
#define SLBC_SLOT_USED                  0x10
#define SLBC_FORCE                      0x14
#define SLBC_BUFFER_REF                 0x18
#define SLBC_REF                        0x1C
#define SLBC_DEBUG_0                    0x20
#define SLBC_DEBUG_1                    0x24
#define SLBC_DEBUG_2                    0x28
#define SLBC_DEBUG_3                    0x2C
#define SLBC_DEBUG_4                    0x30
#define SLBC_DEBUG_5                    0x34
#define SLBC_DEBUG_6                    0x38
#define SLBC_DEBUG_7                    0x3C
#define SLBC_APU_BW                     0x40
#define SLBC_MM_BW                      0x44
#define SLBC_MM_EST_BW                  0x48
#define SLBC_CACHE_USED                 0x4C
#define SLBC_SRAM_CON                   0x48
#define SLBC_STA                        0x90
#define SLBC_ACK_C                      0x94
#define SLBC_ACK_G                      0x98
#define CPUQOS_MODE                     0x9C

/* SLBC_UID_USED */
#define SLBC_UID_USED_STA_LSB           SLBC_BIT(0)
#define SLBC_UID_USED_STA_SHIFT         0
#define SLBC_UID_USED_STA_MASK          SLBC_BITMASK(31:0)
/* SLBC_SID_MASK */
#define SLBC_SID_MASK_STA_LSB           SLBC_BIT(0)
#define SLBC_SID_MASK_STA_SHIFT         0
#define SLBC_SID_MASK_STA_MASK          SLBC_BITMASK(31:0)
/* SLBC_SID_REQ_Q */
#define SLBC_SID_REQ_Q_STA_LSB          SLBC_BIT(0)
#define SLBC_SID_REQ_Q_STA_SHIFT        0
#define SLBC_SID_REQ_Q_STA_MASK         SLBC_BITMASK(31:0)
/* SLBC_SID_REL_Q */
#define SLBC_SID_REL_Q_STA_LSB          SLBC_BIT(0)
#define SLBC_SID_REL_Q_STA_SHIFT        0
#define SLBC_SID_REL_Q_STA_MASK         SLBC_BITMASK(31:0)
/* SLBC_SLOT_USED */
#define SLBC_SLOT_USED_STA_LSB          SLBC_BIT(0)
#define SLBC_SLOT_USED_STA_SHIFT        0
#define SLBC_SLOT_USED_STA_MASK         SLBC_BITMASK(31:0)
/* SLBC_FORCE */
#define SLBC_FORCE_STA_LSB              SLBC_BIT(0)
#define SLBC_FORCE_STA_SHIFT            0
#define SLBC_FORCE_STA_MASK             SLBC_BITMASK(15:0)
#define SLBC_FORCE_ENABLE_LSB           SLBC_BIT(16)
#define SLBC_FORCE_ENABLE_SHIFT         16
#define SLBC_FORCE_ENABLE_MASK          SLBC_BITMASK(16:16)
/* SLBC_BUFFER_REF */
#define SLBC_BUFFER_REF_STA_LSB         SLBC_BIT(0)
#define SLBC_BUFFER_REF_STA_SHIFT       0
#define SLBC_BUFFER_REF_STA_MASK        SLBC_BITMASK(31:0)
/* SLBC_REF */
#define SLBC_REF_STA_LSB                SLBC_BIT(0)
#define SLBC_REF_STA_SHIFT              0
#define SLBC_REF_STA_MASK               SLBC_BITMASK(31:0)
/* SLBC_DEBUG_0 */
#define SLBC_DEBUG_0_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_0_STA_SHIFT          0
#define SLBC_DEBUG_0_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_1 */
#define SLBC_DEBUG_1_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_1_STA_SHIFT          0
#define SLBC_DEBUG_1_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_2 */
#define SLBC_DEBUG_2_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_2_STA_SHIFT          0
#define SLBC_DEBUG_2_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_3 */
#define SLBC_DEBUG_3_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_3_STA_SHIFT          0
#define SLBC_DEBUG_3_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_4 */
#define SLBC_DEBUG_4_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_4_STA_SHIFT          0
#define SLBC_DEBUG_4_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_5 */
#define SLBC_DEBUG_5_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_5_STA_SHIFT          0
#define SLBC_DEBUG_5_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_6 */
#define SLBC_DEBUG_6_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_6_STA_SHIFT          0
#define SLBC_DEBUG_6_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_DEBUG_7 */
#define SLBC_DEBUG_7_STA_LSB            SLBC_BIT(0)
#define SLBC_DEBUG_7_STA_SHIFT          0
#define SLBC_DEBUG_7_STA_MASK           SLBC_BITMASK(31:0)
/* SLBC_APU_BW */
#define SLBC_APU_BW_R_LSB               SLBC_BIT(0)
#define SLBC_APU_BW_R_SHIFT             0
#define SLBC_APU_BW_R_MASK              SLBC_BITMASK(31:0)
/* SLBC_MM_BW */
#define SLBC_MM_BW_R_LSB                SLBC_BIT(0)
#define SLBC_MM_BW_R_SHIFT              0
#define SLBC_MM_BW_R_MASK               SLBC_BITMASK(31:0)
/* SLBC_MM_EST_BW */
#define SLBC_MM_BW_EST_R_LSB            SLBC_BIT(0)
#define SLBC_MM_BW_EST_R_SHIFT          0
#define SLBC_MM_BW_EST_R_MASK           SLBC_BITMASK(31:0)
/* SLBC_CACHE_USED */
#define SLBC_CACHE_USED_R_LSB           SLBC_BIT(0)
#define SLBC_CACHE_USED_R_SHIFT         0
#define SLBC_CACHE_USED_R_MASK          SLBC_BITMASK(15:0)
#define SLBC_CACHE_CPU_STA_LSB          SLBC_BIT(16)
#define SLBC_CACHE_CPU_STA_SHIFT        16
#define SLBC_CACHE_CPU_STA_MASK         SLBC_BITMASK(23:16)
#define SLBC_CACHE_GPU_STA_LSB          SLBC_BIT(24)
#define SLBC_CACHE_GPU_STA_SHIFT        24
#define SLBC_CACHE_GPU_STA_MASK         SLBC_BITMASK(31:24)
/* SLBC_STA */
#define SLBC_STA_C_MODE_LSB             SLBC_BIT(0)
#define SLBC_STA_C_MODE_SHIFT           0
#define SLBC_STA_C_MODE_MASK            SLBC_BITMASK(1:0)
#define SLBC_STA_C_L3C_PD_DIS_LSB       SLBC_BIT(2)
#define SLBC_STA_C_L3C_PD_DIS_SHIFT     2
#define SLBC_STA_C_L3C_PD_DIS_MASK      SLBC_BITMASK(2:2)
#define SLBC_STA_C_WAY_LSB              SLBC_BIT(3)
#define SLBC_STA_C_WAY_SHIFT            3
#define SLBC_STA_C_WAY_MASK             SLBC_BITMASK(6:3)
#define SLBC_STA_C_DRAM_OPP_LSB         SLBC_BIT(7)
#define SLBC_STA_C_DRAM_OPP_SHIFT       7
#define SLBC_STA_C_DRAM_OPP_MASK        SLBC_BITMASK(10:7)
#define SLBC_STA_G_MODE_LSB             SLBC_BIT(16)
#define SLBC_STA_G_MODE_SHIFT           16
#define SLBC_STA_G_MODE_MASK            SLBC_BITMASK(17:16)
#define SLBC_STA_G_WAY_LSB              SLBC_BIT(18)
#define SLBC_STA_G_WAY_SHIFT            18
#define SLBC_STA_G_WAY_MASK             SLBC_BITMASK(21:18)
/* SLBC_ACK_C */
#define SLBC_ACK_C_MODE_LSB             SLBC_BIT(0)
#define SLBC_ACK_C_MODE_SHIFT           0
#define SLBC_ACK_C_MODE_MASK            SLBC_BITMASK(1:0)
#define SLBC_ACK_C_L3C_LSB              SLBC_BIT(2)
#define SLBC_ACK_C_L3C_SHIFT            2
#define SLBC_ACK_C_L3C_MASK             SLBC_BITMASK(5:2)
/* SLBC_ACK_G */
#define SLBC_ACK_G_MODE_LSB             SLBC_BIT(0)
#define SLBC_ACK_G_MODE_SHIFT           0
#define SLBC_ACK_G_MODE_MASK            SLBC_BITMASK(1:0)
/* CPUQOS_MODE */
#define POWER_CPUQOS_MODE_LSB           SLBC_BIT(0)
#define POWER_CPUQOS_MODE_SHIFT         0
#define POWER_CPUQOS_MODE_MASK          SLBC_BITMASK(3:0)

#endif
