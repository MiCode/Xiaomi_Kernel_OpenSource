/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_DVFSRC_REG_V2_H
#define __MTK_DVFSRC_REG_V2_H

#if defined(CONFIG_MACH_MT6779)
#define DVFSRC_IP_V2_1
#elif defined(CONFIG_MACH_MT6785)
#define DVFSRC_IP_V2_2
#endif

/**************************************
 * Define and Declare
 **************************************/

#define DVFSRC_BASIC_CONTROL             (0x0)
#define DVFSRC_SW_REQ1                   (0x4)
#define DVFSRC_SW_REQ2                   (0x8)
#define DVFSRC_SW_REQ3                   (0xC)
#define DVFSRC_SW_REQ4                   (0x10)
#define DVFSRC_SW_REQ5                   (0x14)
#define DVFSRC_SW_REQ6                   (0x18)
#define DVFSRC_SW_REQ7                   (0x1C)
#define DVFSRC_SW_REQ8                   (0x20)
#define DVFSRC_EMI_REQUEST               (0x24)
#define DVFSRC_EMI_REQUEST2              (0x28)
#define DVFSRC_EMI_REQUEST3              (0x2C)
#define DVFSRC_EMI_REQUEST4              (0x30)
#define DVFSRC_EMI_REQUEST5              (0x34)
#define DVFSRC_EMI_REQUEST6              (0x38)
#define DVFSRC_EMI_HRT                   (0x3C)
#define DVFSRC_EMI_HRT2                  (0x40)
#define DVFSRC_EMI_HRT3                  (0x44)
#define DVFSRC_EMI_QOS0                  (0x48)
#define DVFSRC_EMI_QOS1                  (0x4C)
#define DVFSRC_EMI_QOS2                  (0x50)
#define DVFSRC_EMI_MD2SPM0               (0x54)
#define DVFSRC_EMI_MD2SPM1               (0x58)
#define DVFSRC_EMI_MD2SPM2               (0x5C)
#define DVFSRC_EMI_MD2SPM0_T             (0x60)
#define DVFSRC_EMI_MD2SPM1_T             (0x64)
#define DVFSRC_EMI_MD2SPM2_T             (0x68)
#define DVFSRC_VCORE_REQUEST             (0x6C)
#define DVFSRC_VCORE_REQUEST2            (0x70)
#define DVFSRC_VCORE_REQUEST3            (0x74)
#define DVFSRC_VCORE_REQUEST4            (0x78)
#define DVFSRC_VCORE_HRT                 (0x7C)
#define DVFSRC_VCORE_HRT2                (0x80)
#define DVFSRC_VCORE_HRT3                (0x84)
#define DVFSRC_VCORE_QOS0                (0x88)
#define DVFSRC_VCORE_QOS1                (0x8C)
#define DVFSRC_VCORE_QOS2                (0x90)
#define DVFSRC_VCORE_MD2SPM0             (0x94)
#define DVFSRC_VCORE_MD2SPM1             (0x98)
#define DVFSRC_VCORE_MD2SPM2             (0x9C)
#define DVFSRC_VCORE_MD2SPM0_T           (0xA0)
#define DVFSRC_VCORE_MD2SPM1_T           (0xA4)
#define DVFSRC_VCORE_MD2SPM2_T           (0xA8)
#define DVFSRC_MD_VSRAM_REMAP            (0xBC)
#define DVFSRC_HALT_SW_CONTROL           (0xC0)
#define DVFSRC_INT                       (0xC4)
#define DVFSRC_INT_EN                    (0xC8)
#define DVFSRC_INT_CLR                   (0xCC)
#define DVFSRC_BW_MON_WINDOW             (0xD0)
#define DVFSRC_BW_MON_THRES_1            (0xD4)
#define DVFSRC_BW_MON_THRES_2            (0xD8)
#define DVFSRC_MD_TURBO                  (0xDC)
#define DVFSRC_PCIE_VCORE_REQ            (0xE0)
#define DVFSRC_VCORE_USER_REQ            (0xE4)
#define DVFSRC_BW_USER_REQ               (0xE8)
#define DVFSRC_DEBOUNCE_FOUR             (0xF0)
#define DVFSRC_DEBOUNCE_RISE_FALL        (0xF4)
#define DVFSRC_TIMEOUT_NEXTREQ           (0xF8)
#define DVFSRC_LEVEL_LABEL_0_1           (0x100)
#define DVFSRC_LEVEL_LABEL_2_3           (0x104)
#define DVFSRC_LEVEL_LABEL_4_5           (0x108)
#define DVFSRC_LEVEL_LABEL_6_7           (0x10C)
#define DVFSRC_LEVEL_LABEL_8_9           (0x110)
#define DVFSRC_LEVEL_LABEL_10_11         (0x114)
#define DVFSRC_LEVEL_LABEL_12_13         (0x118)
#define DVFSRC_LEVEL_LABEL_14_15         (0x11C)
#define DVFSRC_MM_BW_0                   (0x200)
#define DVFSRC_MM_BW_1                   (0x204)
#define DVFSRC_MM_BW_2                   (0x208)
#define DVFSRC_MM_BW_3                   (0x20C)
#define DVFSRC_MM_BW_4                   (0x210)
#define DVFSRC_MM_BW_5                   (0x214)
#define DVFSRC_MM_BW_6                   (0x218)
#define DVFSRC_MM_BW_7                   (0x21C)
#define DVFSRC_MM_BW_8                   (0x220)
#define DVFSRC_MM_BW_9                   (0x224)
#define DVFSRC_MM_BW_10                  (0x228)
#define DVFSRC_MM_BW_11                  (0x22C)
#define DVFSRC_MM_BW_12                  (0x230)
#define DVFSRC_MM_BW_13                  (0x234)
#define DVFSRC_MM_BW_14                  (0x238)
#define DVFSRC_MM_BW_15                  (0x23C)
#define DVFSRC_MD_BW_0                   (0x240)
#define DVFSRC_MD_BW_1                   (0x244)
#define DVFSRC_MD_BW_2                   (0x248)
#define DVFSRC_MD_BW_3                   (0x24C)
#define DVFSRC_MD_BW_4                   (0x250)
#define DVFSRC_MD_BW_5                   (0x254)
#define DVFSRC_MD_BW_6                   (0x258)
#define DVFSRC_MD_BW_7                   (0x25C)
#define DVFSRC_SW_BW_0                   (0x260)
#define DVFSRC_SW_BW_1                   (0x264)
#define DVFSRC_SW_BW_2                   (0x268)
#define DVFSRC_SW_BW_3                   (0x26C)
#define DVFSRC_SW_BW_4                   (0x270)
#define DVFSRC_SW_BW_5                   (0x274)
#define DVFSRC_SW_BW_6                   (0x278)
#define DVFSRC_QOS_EN                    (0x280)
#define DVFSRC_MD_BW_URG                 (0x284)
#define DVFSRC_ISP_HRT                   (0x290)
#define DVFSRC_HRT_BW_BASE               (0x294)
#define DVFSRC_SEC_SW_REQ                (0x304)
#define DVFSRC_EMI_MON_DEBOUNCE_TIME     (0x308)
#define DVFSRC_MD_LATENCY_IMPROVE        (0x30C)
#define DVFSRC_BASIC_CONTROL_3           (0x310)
#define DVFSRC_DEBOUNCE_TIME             (0x314)
#define DVFSRC_LEVEL_MASK                (0x318)
#define DVFSRC_DEFAULT_OPP               (0x31C)
#define DVFSRC_95MD_SCEN_EMI0            (0x500)
#define DVFSRC_95MD_SCEN_EMI1            (0x504)
#define DVFSRC_95MD_SCEN_EMI2            (0x508)
#define DVFSRC_95MD_SCEN_EMI3            (0x50C)
#define DVFSRC_95MD_SCEN_EMI0_T          (0x510)
#define DVFSRC_95MD_SCEN_EMI1_T          (0x514)
#define DVFSRC_95MD_SCEN_EMI2_T          (0x518)
#define DVFSRC_95MD_SCEN_EMI3_T          (0x51C)
#define DVFSRC_95MD_SCEN_EMI4            (0x520)
#define DVFSRC_95MD_SCEN_BW0             (0x524)
#define DVFSRC_95MD_SCEN_BW1             (0x528)
#define DVFSRC_95MD_SCEN_BW2             (0x52C)
#define DVFSRC_95MD_SCEN_BW3             (0x530)
#define DVFSRC_95MD_SCEN_BW0_T			 (0x534)
#define DVFSRC_95MD_SCEN_BW1_T			 (0x538)
#define DVFSRC_95MD_SCEN_BW2_T			 (0x53C)
#define DVFSRC_95MD_SCEN_BW3_T			 (0x540)
#define DVFSRC_95MD_SCEN_BW4             (0x544)
#define DVFSRC_MD_LEVEL_SW_REG           (0x548)
#define DVFSRC_RSRV_0                    (0x600)
#define DVFSRC_RSRV_1                    (0x604)
#define DVFSRC_RSRV_2                    (0x608)
#define DVFSRC_RSRV_3                    (0x60C)
#define DVFSRC_RSRV_4                    (0x610)
#define DVFSRC_RSRV_5                    (0x614)
#define DVFSRC_SPM_RESEND                (0x630)
#define DVFSRC_DEBUG_STA_0               (0x700)
#define DVFSRC_DEBUG_STA_1               (0x704)
#define DVFSRC_DEBUG_STA_2               (0x708)
#define DVFSRC_DEBUG_STA_3               (0x70C)
#define DVFSRC_DEBUG_STA_4               (0x710)
#define DVFSRC_DEBUG_STA_5               (0x714)
#define DVFSRC_DEBUG_STA_6               (0x718)
#define DVFSRC_EMI_REQUEST7              (0x800)
#define DVFSRC_EMI_HRT_1                 (0x804)
#define DVFSRC_EMI_HRT2_1                (0x808)
#define DVFSRC_EMI_HRT3_1                (0x80C)
#define DVFSRC_EMI_QOS3                  (0x810)
#define DVFSRC_EMI_QOS4                  (0x814)
#define DVFSRC_DDR_REQUEST               (0xA00)
#define DVFSRC_DDR_REQUEST2              (0xA04)
#define DVFSRC_DDR_REQUEST3              (0xA08)
#define DVFSRC_DDR_REQUEST4              (0xA0C)
#define DVFSRC_DDR_REQUEST5              (0xA10)
#define DVFSRC_DDR_REQUEST6              (0xA14)
#define DVFSRC_DDR_REQUEST7              (0xA18)
#define DVFSRC_DDR_HRT                   (0xA1C)
#define DVFSRC_DDR_HRT2                  (0xA20)
#define DVFSRC_DDR_HRT3                  (0xA24)
#define DVFSRC_DDR_HRT_1                 (0xA28)
#define DVFSRC_DDR_HRT2_1                (0xA2C)
#define DVFSRC_DDR_HRT3_1                (0xA30)
#define DVFSRC_DDR_QOS0                  (0xA34)
#define DVFSRC_DDR_QOS1                  (0xA38)
#define DVFSRC_DDR_QOS2                  (0xA3C)
#define DVFSRC_DDR_QOS3                  (0xA40)
#define DVFSRC_DDR_QOS4                  (0xA44)
#define DVFSRC_DDR_MD2SPM0               (0xA48)
#define DVFSRC_DDR_MD2SPM1               (0xA4C)
#define DVFSRC_DDR_MD2SPM2               (0xA50)
#define DVFSRC_DDR_MD2SPM0_T             (0xA54)
#define DVFSRC_DDR_MD2SPM1_T             (0xA58)
#define DVFSRC_DDR_MD2SPM2_T             (0xA5C)
#if defined(DVFSRC_IP_V2_1)
#define DVFSRC_HRT_REQ_UNIT              (0xA84)
#define DVSFRC_HRT_REQ_MD_URG            (0xA88)
#define DVFSRC_HRT_REQ_MD_BW_0           (0xA8C)
#define DVFSRC_HRT_REQ_MD_BW_1           (0xA90)
#define DVFSRC_HRT_REQ_MD_BW_2           (0xA94)
#define DVFSRC_HRT_REQ_MD_BW_3           (0xA98)
#define DVFSRC_HRT_REQ_MD_BW_4           (0xA9C)
#define DVFSRC_HRT_REQ_MD_BW_5           (0xAA0)
#define DVFSRC_HRT_REQ_MD_BW_6           (0xAA4)
#define DVFSRC_HRT_REQ_MD_BW_7           (0xAA8)
#define DVFSRC_HRT1_REQ_MD_BW_0          (0xAAC)
#define DVFSRC_HRT1_REQ_MD_BW_1          (0xAB0)
#define DVFSRC_HRT1_REQ_MD_BW_2          (0xAB4)
#define DVFSRC_HRT1_REQ_MD_BW_3          (0xAB8)
#define DVFSRC_HRT1_REQ_MD_BW_4          (0xABC)
#define DVFSRC_HRT1_REQ_MD_BW_5          (0xAC0)
#define DVFSRC_HRT1_REQ_MD_BW_6          (0xAC4)
#define DVFSRC_HRT1_REQ_MD_BW_7          (0xAC8)
#define DVFSRC_HRT_REQ_MD_BW_8           (0xACC)
#define DVFSRC_HRT_REQ_MD_BW_9           (0xAD0)
#define DVFSRC_HRT_REQ_MD_BW_10          (0xAD4)
#define DVFSRC_HRT1_REQ_MD_BW_8          (0xAD8)
#define DVFSRC_HRT1_REQ_MD_BW_9          (0xADC)
#define DVFSRC_HRT1_REQ_MD_BW_10         (0xAE0)
#define DVFSRC_HRT_REQ_BW_SW_REG         (0xAE4)
#define DVFSRC_HRT_REQUEST               (0xAE8)
#define DVFSRC_HRT_HIGH_2                (0xAEC)
#define DVFSRC_HRT_HIGH_1                (0xAF0)
#define DVFSRC_HRT_HIGH                  (0xAF4)
#define DVFSRC_HRT_LOW_2                 (0xAF8)
#define DVFSRC_HRT_LOW_1                 (0xAFC)
#define DVFSRC_HRT_LOW                   (0xB00)
#define DVFSRC_DDR_ADD_REQUEST           (0xB04)
#define DVFSRC_LAST                      (0xB08)
#define DVFSRC_LAST_L                    (0xB0C)
#define DVFSRC_MD_SCENARIO               (0xB10)
#define DVFSRC_RECORD_0_0                (0xB14)
#define DVFSRC_RECORD_0_1                (0xB18)
#define DVFSRC_RECORD_0_2                (0xB1C)
#define DVFSRC_RECORD_0_3                (0xB20)
#define DVFSRC_RECORD_0_4                (0xB24)
#define DVFSRC_RECORD_0_5                (0xB28)
#define DVFSRC_RECORD_0_6                (0xB2C)
#define DVFSRC_RECORD_0_L_0              (0xBF4)
#define DVFSRC_RECORD_0_L_1              (0xBF8)
#define DVFSRC_RECORD_0_L_2              (0xBFC)
#define DVFSRC_RECORD_0_L_3              (0xC00)
#define DVFSRC_RECORD_0_L_4              (0xC04)
#define DVFSRC_RECORD_0_L_5              (0xC08)
#define DVFSRC_RECORD_0_L_6              (0xC0C)
#else
#define DVFSRC_HRT_REQ_UNIT              (0xA60)
#define DVSFRC_HRT_REQ_MD_URG            (0xA64)
#define DVFSRC_HRT_REQ_MD_BW_0           (0xA68)
#define DVFSRC_HRT_REQ_MD_BW_1           (0xA6C)
#define DVFSRC_HRT_REQ_MD_BW_2           (0xA70)
#define DVFSRC_HRT_REQ_MD_BW_3           (0xA74)
#define DVFSRC_HRT_REQ_MD_BW_4           (0xA78)
#define DVFSRC_HRT_REQ_MD_BW_5           (0xA7C)
#define DVFSRC_HRT_REQ_MD_BW_6           (0xA80)
#define DVFSRC_HRT_REQ_MD_BW_7           (0xA84)
#define DVFSRC_HRT1_REQ_MD_BW_0          (0xA88)
#define DVFSRC_HRT1_REQ_MD_BW_1          (0xA8C)
#define DVFSRC_HRT1_REQ_MD_BW_2          (0xA90)
#define DVFSRC_HRT1_REQ_MD_BW_3          (0xA94)
#define DVFSRC_HRT1_REQ_MD_BW_4          (0xA98)
#define DVFSRC_HRT1_REQ_MD_BW_5          (0xA9C)
#define DVFSRC_HRT1_REQ_MD_BW_6          (0xAA0)
#define DVFSRC_HRT1_REQ_MD_BW_7          (0xAA4)
#define DVFSRC_HRT_REQ_MD_BW_8           (0xAA8)
#define DVFSRC_HRT_REQ_MD_BW_9           (0xAAC)
#define DVFSRC_HRT_REQ_MD_BW_10          (0xAB0)
#define DVFSRC_HRT1_REQ_MD_BW_8          (0xAB4)
#define DVFSRC_HRT1_REQ_MD_BW_9          (0xAB8)
#define DVFSRC_HRT1_REQ_MD_BW_10         (0xABC)
#define DVFSRC_HRT_REQ_BW_SW_REG         (0xAC0)
#define DVFSRC_HRT_REQUEST               (0xAC4)
#define DVFSRC_HRT_HIGH_2                (0xAC8)
#define DVFSRC_HRT_HIGH_1                (0xACC)
#define DVFSRC_HRT_HIGH                  (0xAD0)
#define DVFSRC_HRT_LOW_2                 (0xAD4)
#define DVFSRC_HRT_LOW_1                 (0xAD8)
#define DVFSRC_HRT_LOW                   (0xADC)
#define DVFSRC_DDR_ADD_REQUEST           (0xAE0)
#define DVFSRC_LAST                      (0xAE4)
#define DVFSRC_LAST_L                    (0xAE8)
#define DVFSRC_MD_SCENARIO               (0xAEC)
#define DVFSRC_RECORD_0_0                (0xAF0)
#define DVFSRC_RECORD_0_1                (0xAF4)
#define DVFSRC_RECORD_0_2                (0xAF8)
#define DVFSRC_RECORD_0_3                (0xAFC)
#define DVFSRC_RECORD_0_4                (0xB00)
#define DVFSRC_RECORD_0_5                (0xB04)
#define DVFSRC_RECORD_0_6                (0xB08)
#define DVFSRC_RECORD_0_7                (0xB0C)
#define DVFSRC_RECORD_0_L_0              (0xBF0)
#define DVFSRC_RECORD_0_L_1              (0xBF4)
#define DVFSRC_RECORD_0_L_2              (0xBF8)
#define DVFSRC_RECORD_0_L_3              (0xBFC)
#define DVFSRC_RECORD_0_L_4              (0xC00)
#define DVFSRC_RECORD_0_L_5              (0xC04)
#define DVFSRC_RECORD_0_L_6              (0xC08)
#define DVFSRC_RECORD_0_L_7              (0xC0C)
#endif
#define DVFSRC_EMI_REQUEST8              (0xCF0)
#define DVFSRC_DDR_REQUEST8              (0xCF4)
#define DVFSRC_EMI_HRT_2                 (0xCF8)
#define DVFSRC_EMI_HRT2_2                (0xCFC)
#define DVFSRC_EMI_HRT3_2                (0xD00)
#define DVFSRC_EMI_QOS5                  (0xD04)
#define DVFSRC_EMI_QOS6                  (0xD08)
#define DVFSRC_DDR_HRT_2                 (0xD0C)
#define DVFSRC_DDR_HRT2_2                (0xD10)
#define DVFSRC_DDR_HRT3_2                (0xD14)
#define DVFSRC_DDR_QOS5                  (0xD18)
#define DVFSRC_DDR_QOS6                  (0xD1C)
#define DVFSRC_VCORE_REQUEST5            (0xD20)
#define DVFSRC_VCORE_HRT_1               (0xD24)
#define DVFSRC_VCORE_HRT2_1              (0xD28)
#define DVFSRC_VCORE_HRT3_1              (0xD2C)
#define DVFSRC_VCORE_QOS3                (0xD30)
#define DVFSRC_VCORE_QOS4                (0xD34)
#define DVFSRC_HRT_HIGH_3                (0xD38)
#define DVFSRC_HRT_LOW_3                 (0xD3C)
#define DVFSRC_BASIC_CONTROL_2           (0xD40)
#define DVFSRC_CURRENT_LEVEL             (0xD44)
#define DVFSRC_TARGET_LEVEL              (0xD48)
#define DVFSRC_LEVEL_LABEL_16_17         (0xD4C)
#define DVFSRC_LEVEL_LABEL_18_19         (0xD50)
#define DVFSRC_LEVEL_LABEL_20_21         (0xD54)
#define DVFSRC_LEVEL_LABEL_22_23         (0xD58)
#define DVFSRC_LEVEL_LABEL_24_25         (0xD5C)
#define DVFSRC_LEVEL_LABEL_26_27         (0xD60)
#define DVFSRC_LEVEL_LABEL_28_29         (0xD64)
#define DVFSRC_LEVEL_LABEL_30_31         (0xD68)
#define DVFSRC_CURRENT_FORCE             (0xD6C)
#define DVFSRC_TARGET_FORCE              (0xD70)
#define DVFSRC_EMI_ADD_REQUEST           (0xD74)

/* DVFSRC_BASIC_CONTROL 0x0 */
#define DVFSRC_EN_SHIFT		0
#define DVFSRC_EN_MASK		0x1
#define DVFSRC_OUT_EN_SHIFT	8
#define DVFSRC_OUT_EN_MASK	0x1
#define FORCE_EN_CUR_SHIFT	14
#define FORCE_EN_CUR_MASK	0x1
#define FORCE_EN_TAR_SHIFT	15
#define FORCE_EN_TAR_MASK	0x1

/* DVFSRC_SW_REQX */
#define DDR_SW_AP_SHIFT		12
#define DDR_SW_AP_MASK		0x7
#define VCORE_SW_AP_SHIFT	4
#define VCORE_SW_AP_MASK	0x7
#define EMI_SW_AP_SHIFT		0
#define EMI_SW_AP_MASK		0x7

/* DVFSRC_VCORE_REQUEST 0x70 */
#define VCORE_SCP_GEAR_SHIFT	12
#define VCORE_SCP_GEAR_MASK     0x7

/* DVFSRC_LEVEL 0xFC */
#define CURRENT_LEVEL_SHIFT	0
#define CURRENT_LEVEL_MASK	0xFFFFFFFF

/* DVFSRC_FORCE 0x300 */
#define TARGET_FORCE_SHIFT	0
#define TARGET_FORCE_MASK	0xFFFFFFFF
#define CURRENT_FORCE_SHIFT	0
#define CURRENT_FORCE_MASK	0xFFFFFFFF

/* DVFSRC_DEBUG_STA_0 */
#define MD_EMI_URG_DEBUG_SHIFT	16
#define MD_EMI_URG_DEBUG_MASK	0x1
#define MD_SRC_CLK_DEBUG_SHIFT	17
#define MD_SRC_CLK_DEBUG_MASK	0x1
#define MD_EMI_VAL_DEBUG_SHIFT	0
#define MD_EMI_VAL_DEBUG_MASK	0xFFFF

/* DVFSRC_DEBUG_STA_4 */
#define MD_EMI_MD_IMP_SHIFT	19
#define MD_EMI_MD_IMP_MASK	0x7


/* DVSFRC_HRT_REQ_MD_URG */
#define MD_HRT_BW_URG_SHIFT     0
#define MD_HRT_BW_URG_MASK      0xFF
#define MD_HRT_BW_URG1_SHIFT    8
#define MD_HRT_BW_URG1_MASK     0xFF

/* DVFSRC_HRTX_REQ_MD_BW_x */
#define MD_HRT_BW_MASK     0x3FF

#define DEBUG_MDTURBO_SHIFT 18
#define DEBUG_MDTURBO_MASK  0x1

#define DEBUG_MD_RIS_DDR_SHIFT 29
#define DEBUG_MD_RIS_DDR_MASK  0x7

#define DEBUG_HIFI_RIS_DDR_SHIFT 22
#define DEBUG_HIFI_RIS_DDR_MASK  0x7


#define DEBUG_STA2_EMI_TOTAL_SHIFT 0
#define DEBUG_STA2_EMI_TOTAL_MASK 0xFFF

#define DEBUG_STA2_SCP_SHIFT 14
#define DEBUG_STA2_SCP_MASK  0x1

#define DEBUG_STA2_PCIE_SHIFT 27
#define DEBUG_STA2_PCIE_MASK  0x1

#define DEBUG_STA2_MD_EMI_LATENCY_SHIFT 12
#define DEBUG_STA2_MD_EMI_LATENCY_MASK  0x3

#define DEBUG_STA2_HIFI_SCENARIO_SHIFT 16
#define DEBUG_STA2_HIFI_SCENARIO_MASK  0xFF

#define DEBUG_STA4_HRT_BW_REQ_SHIFT 16
#define DEBUG_STA4_HRT_BW_REQ_MASK  0x7

#define DEBUG_STA3_MD_HRT_BW_SHIFT 0
#define DEBUG_STA3_MD_HRT_BW_MASK  0x3FF

#if defined(DVFSRC_IP_V2_1)
#define RECORD_SHIFT     0x1C
#else
#define RECORD_SHIFT     0x20
#endif

#if defined(DVFSRC_IP_V2_1)
#define MD_TURBO_SWITCH_SHIFT	5
#define MD_TURBO_SWITCH_MASK	0x1
#define RECORD_HIFI_DDR_LATENCY_REQ 15
#define RECORD_HIFI_DDR_LATENCY_MASK  0x7
#define RECORD_HRT_BW_REQ_SHIFT 2
#define RECORD_HRT_BW_REQ_MASK  0x7
#define RECORD_MD_DDR_LATENCY_REQ 9
#define RECORD_MD_DDR_LATENCY_MASK  0x7
#elif defined(DVFSRC_IP_V2_2)
#define RECORD_HRT_BW_REQ_SHIFT 21
#define RECORD_HRT_BW_REQ_MASK  0x7
#endif

#endif /* __MTK_DVFSRC_REG_V2_H */
