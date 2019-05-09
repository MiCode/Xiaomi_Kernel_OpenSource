// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/bug.h>
#include "devapc-mtk-common.h"
#include "devapc-mt6779.h"

static struct mtk_device_info mt6779_infra_devices[] = {
/* slave type,       config_idx, device name                enable_vio_irq */

 /* 0 */
{E_DAPC_INFRA_PERI_SLAVE, 0,   "INFRA_AO_TOPCKGEN",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 1,   "INFRA_AO_INFRASYS_CONFIG_REGS",         true},
{E_DAPC_INFRA_PERI_SLAVE, 2,   "INFRA_AO_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 3,   "INFRA_AO_ PERICFG",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 4,   "INFRA_AO_EFUSE_AO_DEBUG",               true},
{E_DAPC_INFRA_PERI_SLAVE, 5,   "INFRA_AO_GPIO",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 6,   "INFRA_AO_SLEEP_CONTROLLER",             true},
{E_DAPC_INFRA_PERI_SLAVE, 7,   "INFRA_AO_TOPRGU",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 8,   "INFRA_AO_APXGPT",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 9,   "INFRA_AO_RESERVE",                      true},

 /* 10 */
{E_DAPC_INFRA_PERI_SLAVE, 10,  "INFRA_AO_SEJ",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 11,  "INFRA_AO_AP_CIRQ_EINT",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 12,  "INFRA_AO_APMIXEDSYS",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 13,  "INFRA_AO_PMIC_WRAP",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 14,  "INFRA_AO_DEVICE_APC_AO_MM",             true},
{E_DAPC_INFRA_PERI_SLAVE, 15,  "INFRA_AO_SLEEP_CONTROLLER_MD",          true},
{E_DAPC_INFRA_PERI_SLAVE, 16,  "INFRA_AO_KEYPAD",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 17,  "INFRA_AO_TOP_MISC",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 18,  "INFRA_AO_ DVFS_CTRL_PROC",              true},
{E_DAPC_INFRA_PERI_SLAVE, 19,  "INFRA_AO_MBIST_AO_REG",                 true},

 /* 20 */
{E_DAPC_INFRA_PERI_SLAVE, 20,  "INFRA_AO_CLDMA_AO_AP",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 21,  "INFRA_AO_DEVICE_MPU",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 22,  "INFRA_AO_AES_TOP_0",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 23,  "INFRA_AO_SYS_TIMER",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 24,  "INFRA_AO_MDEM_TEMP_SHARE",              true},
{E_DAPC_INFRA_PERI_SLAVE, 25,  "INFRA_AO_DEVICE_APC_AO_MD",             true},
{E_DAPC_INFRA_PERI_SLAVE, 26,  "INFRA_AO_SECURITY_AO",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 27,  "INFRA_AO_TOPCKGEN_REG",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 28,  "INFRA_AO_DEVICE_APC_AO_INFRA_PERI",     true},
{E_DAPC_INFRA_PERI_SLAVE, 29,  "INFRA_AO_SLEEP_SRAM",                   true},

 /* 30 */
{E_DAPC_INFRA_PERI_SLAVE, 30,  "INFRA_AO_SLEEP_SRAM",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 31,  "INFRA_AO_SLEEP_SRAM",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 32,  "INFRA_AO_SLEEP_SRAM",                   true},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, 37,  "INFRASYS_SYS_CIRQ",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 38,  "INFRASYS_MM_IOMMU",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 39,  "INFRASYS_EFUSE_PDN_DEBUG",              true},

 /* 40 */
{E_DAPC_INFRA_PERI_SLAVE, 40,  "INFRASYS_DEVICE_APC",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 41,  "INFRASYS_DBG_TRACKER",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 42,  "INFRASYS_CCIF0_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 43,  "INFRASYS_CCIF0_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 44,  "INFRASYS_CCIF1_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 45,  "INFRASYS_CCIF1_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 46,  "INFRASYS_MBIST",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 47,  "INFRASYS_INFRA_PDN_REGISTER",           true},
{E_DAPC_INFRA_PERI_SLAVE, 48,  "INFRASYS_TRNG",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 49,  "INFRASYS_DX_CC",                        true},

 /* 50 */
{E_DAPC_INFRA_PERI_SLAVE, 50,  "INFRASYS_MD_CCIF_MD1",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 51,  "INFRASYS_CQ_DMA",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 52,  "INFRASYS_MD_CCIF_MD2",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 53,  "INFRASYS_SRAMROM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 54,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 55,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 56,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 57,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 58,  "INFRASYS_EMI",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 59,  "INFRASYS_DEVICE_MPU",                   true},

 /* 60 */
{E_DAPC_INFRA_PERI_SLAVE, 60,  "INFRASYS_CLDMA_PDN",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 61,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 62,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 63,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 64,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},

 /* 70 */
{E_DAPC_INFRA_PERI_SLAVE, 65,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 66,  "INFRASYS_EMI_MPU",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 67,  "INFRASYS_DVFS_PROC",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 68,  "INFRASYS_GCE",                          true},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, -1,  "INFRASYS_RESERVE",                      false},
{E_DAPC_INFRA_PERI_SLAVE, 33,  "INFRASYS_DPMAIF",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 34,  "INFRASYS_DPMAIF",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 35,  "INFRASYS_DPMAIF",                       true},

 /* 80 */
{E_DAPC_INFRA_PERI_SLAVE, 35,  "INFRASYS_DPMAIF",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 70,  "INFRASYS_DRAMC_CH1_TOP0",               true},
{E_DAPC_INFRA_PERI_SLAVE, 71,  "INFRASYS_DRAMC_CH1_TOP1",               true},
{E_DAPC_INFRA_PERI_SLAVE, 72,  "INFRASYS_DRAMC_CH1_TOP2",               true},
{E_DAPC_INFRA_PERI_SLAVE, 73,  "INFRASYS_DRAMC_CH1_TOP3",               true},
{E_DAPC_INFRA_PERI_SLAVE, 74,  "INFRASYS_DRAMC_CH1_TOP4",               true},
{E_DAPC_INFRA_PERI_SLAVE, 75,  "INFRASYS_DRAMC_CH1_TOP5",               true},
{E_DAPC_INFRA_PERI_SLAVE, 76,  "INFRASYS_DRAMC_CH1_TOP6",               true},
{E_DAPC_INFRA_PERI_SLAVE, 77,  "INFRASYS_CCIF2_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 78,  "INFRASYS_CCIF2_MD",                     true},

 /* 90 */
{E_DAPC_INFRA_PERI_SLAVE, 79,  "INFRASYS_CCIF3_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 80,  "INFRASYS_CCIF3_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 81,  "INFRASYS_DRAMC_CH0_TOP0",               true},
{E_DAPC_INFRA_PERI_SLAVE, 82,  "INFRASYS_DRAMC_CH0_TOP1",               true},
{E_DAPC_INFRA_PERI_SLAVE, 83,  "INFRASYS_DRAMC_CH0_TOP2",               true},
{E_DAPC_INFRA_PERI_SLAVE, 84,  "INFRASYS_DRAMC_CH0_TOP3",               true},
{E_DAPC_INFRA_PERI_SLAVE, 85,  "INFRASYS_DRAMC_CH0_TOP4",               true},
{E_DAPC_INFRA_PERI_SLAVE, 86,  "INFRASYS_DRAMC_CH0_TOP5",               true},
{E_DAPC_INFRA_PERI_SLAVE, 87,  "INFRASYS_DRAMC_CH0_TOP6",               true},
{E_DAPC_INFRA_PERI_SLAVE, 88,  "INFRASYS_CCIF4_AP",                     true},

 /* 100 */
{E_DAPC_INFRA_PERI_SLAVE, 89,  "INFRASYS_CCIF4_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 90,  "INFRA_TOPAXI_BUS_TRACE",                true},
{E_DAPC_INFRA_PERI_SLAVE, 91,  "vpu_iommu/mm_iommu",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 92,  "vpu_iommu/mm_iommu",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 93,  "vpu_iommu/mm_iommu",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 94,  "vpu_iommu/mm_iommu",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 95,  "vpu_iommu/mm_iommu",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 96,  "sub_common2x1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 97,  "sub_common2x1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 98,  "sub_common2x1",                         true},

 /* 110 */
{E_DAPC_INFRA_PERI_SLAVE, 110, "INFRA_AO_SSPM Partition 1",             true},
{E_DAPC_INFRA_PERI_SLAVE, 111, "INFRA_AO_SSPM Partition 2",             true},
{E_DAPC_INFRA_PERI_SLAVE, 112, "INFRA_AO_SSPM Partition 3",             true},
{E_DAPC_INFRA_PERI_SLAVE, 113, "INFRA_AO_SSPM Partition 4",             true},
{E_DAPC_INFRA_PERI_SLAVE, 114, "INFRA_AO_SSPM Partition 5",             true},
{E_DAPC_INFRA_PERI_SLAVE, 115, "INFRA_AO_SSPM Partition 6",             true},
{E_DAPC_INFRA_PERI_SLAVE, 116, "INFRA_AO_SSPM Partition 7",             true},
{E_DAPC_INFRA_PERI_SLAVE, 117, "INFRA_AO_SSPM Partition 8",             true},
{E_DAPC_INFRA_PERI_SLAVE, 118, "INFRA_AO_SSPM Partition 9",             true},
{E_DAPC_INFRA_PERI_SLAVE, 119, "INFRA_AO_SSPM Partition 10",            true},

 /* 120 */
{E_DAPC_INFRA_PERI_SLAVE, 120, "INFRA_AO_SSPM Partition 11",            true},
{E_DAPC_INFRA_PERI_SLAVE, 121, "INFRA_AO_SCP",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 122, "INFRA_AO_HIFI3",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 123, "INFRA_AO_MCUCFG",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 124, "INFRASYS_DBUGSYS",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 125, "PERISYS_APDMA",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 126, "PERISYS_AUXADC",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 127, "PERISYS_UART0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 128, "PERISYS_UART1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 129, "PERISYS_UART2",                         true},

 /* 130 */
{E_DAPC_INFRA_PERI_SLAVE, 130, "PERISYS_UART3",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 131, "PERISYS_PWM",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 132, "PERISYS_I2C0",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 133, "PERISYS_I2C1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 134, "PERISYS_I2C2",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 135, "PERISYS_SPI0",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 136, "PERISYS_PTP",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 137, "PERISYS_BTIF",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 138, "PERISYS_IRTX",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 139, "PERISYS_DISP_PWM",                      true},

 /* 140 */
{E_DAPC_INFRA_PERI_SLAVE, 140, "PERISYS_I2C3",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 141, "PERISYS_SPI1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 142, "PERISYS_I2C4",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 143, "PERISYS_SPI2",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 144, "PERISYS_SPI3",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 145, "PERISYS_I2C1_IMM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 146, "PERISYS_I2C2_IMM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 147, "PERISYS_I2C5",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 148, "PERISYS_I2C5_IMM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 149, "PERISYS_SPI4",                          true},

 /* 150 */
{E_DAPC_INFRA_PERI_SLAVE, 150, "PERISYS_SPI5",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 151, "PERISYS_I2C7",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 152, "PERISYS_I2C8",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 153, "PERISYS_BUS_TRACE",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 154, "PERISYS_SPI6",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 155, "PERISYS_SPI7",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 156, "PERISYS_USB",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 157, "PERISYS_USB_2.0_SUB",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 158, "PERISYS_AUDIO",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 159, "PERISYS_MSDC0",                         true},

 /* 160 */
{E_DAPC_INFRA_PERI_SLAVE, 160, "PERISYS_MSDC1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 161, "PERISYS_MSDC2",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 162, "PERISYS_MSDC3",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 163, "PERISYS_UFS",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 164, "PERISUS_USB3.0_SIF",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 165, "PERISUS_USB3.0_SIF2",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 166, "EAST_RESERVE_0",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 167, "EAST_ RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 168, "EAST_ RESERVE_2",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 169, "EAST_ RESERVE_3",                       true},

 /* 170 */
{E_DAPC_INFRA_PERI_SLAVE, 170, "EAST_ RESERVE_4",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 171, "EAST_IO_CFG_RT",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 172, "EAST_ RESERVE_6",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 173, "EAST_ RESERVE_7",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 174, "EAST_CSI0_TOP_AO",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 175, "EAST_CSI1_TOP_AO",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 176, "EAST_ RESERVE_A",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 177, "EAST_ RESERVE_B",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 178, "EAST_RESERVE_C",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 179, "EAST_RESERVE_D",                        true},

 /* 180 */
{E_DAPC_INFRA_PERI_SLAVE, 180, "EAST_RESERVE_E",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 181, "EAST_RESERVE_F",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 182, "SOUTH_RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 183, "SOUTH_RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 184, "SOUTH_IO_CFG_RM",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 185, "SOUTH_IO_CFG_RB",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 186, "SOUTH_EFUSE",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 187, "SOUTH_ RESERVE_5",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 188, "SOUTH_ RESERVE_6",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 189, "SOUTH_ RESERVE_7",                      true},

 /* 190 */
{E_DAPC_INFRA_PERI_SLAVE, 190, "SOUTH_ RESERVE_8",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 191, "SOUTH_ RESERVE_9",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 192, "SOUTH_ RESERVE_A",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 193, "SOUTH_ RESERVE_B",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 194, "SOUTH_RESERVE_C",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 195, "SOUTH_RESERVE_D",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 196, "SOUTH_RESERVE_E",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 197, "SOUTH_RESERVE_F",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 198, "WEST_ RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 199, "WEST_ msdc1_pad_macro",                 true},

 /* 200 */
{E_DAPC_INFRA_PERI_SLAVE, 200, "WEST_ RESERVE_2",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 201, "WEST_ RESERVE_3",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 202, "WEST_ RESERVE_4",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 203, "WEST_ MIPI_TX_CONFIG",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 204, "WEST_ RESERVE_6",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 205, "WEST_ IO_CFG_LB",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 206, "WEST_ IO_CFG_LM",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 207, "WEST_ IO_CFG_BL",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 208, "WEST_ RESERVE_A",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 209, "WEST_ RESERVE_B",                       true},

 /* 210 */
{E_DAPC_INFRA_PERI_SLAVE, 210, "WEST_ RESERVE_C",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 211, "WEST_ RESERVE_D",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 212, "WEST_RESERVE_E",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 213, "WEST_RESERVE_F",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 214, "NORTH_RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 215, "NORTH_RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 216, "NORTH_IO_CFG_LT",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 217, "NORTH_IO_CFG_TL",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 218, "NORTH_USB20 PHY",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 219, "NORTH_msdc0 pad macro",                 true},

 /* 220 */
{E_DAPC_INFRA_PERI_SLAVE, 220, "NORTH_ RESERVE_6",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 221, "NORTH_ RESERVE_7",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 222, "NORTH_ RESERVE_8",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 223, "NORTH_ RESERVE_9",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 224, "NORTH_ UFS_MPHY",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 225, "NORTH_ RESERVE_B",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 226, "NORTH_RESERVE_C",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 227, "NORTH_RESERVE_D",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 228, "NORTH_RESERVE_E",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 229, "NORTH_RESERVE_F",                       true},

 /* 230 */
{E_DAPC_INFRA_PERI_SLAVE, 230, "PERISYS_CONN",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 231, "PERISYS_MD_VIOLATION",                  true},
{E_DAPC_MM_SLAVE,         0,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         1,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         2,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         3,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         4,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         5,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         6,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         7,   "Gpu",                                   true},

 /* 240 */
{E_DAPC_MM_SLAVE,         8,   "Gpu",                                   true},
{E_DAPC_MM_SLAVE,         9,   "DFD",                                   true},
{E_DAPC_MM_SLAVE,         10,  "G3D_CONFIG",                            true},
{E_DAPC_MM_SLAVE,         11,  "G3D_DVFS",                              true},
{E_DAPC_MM_SLAVE,         12,  "MFG_OTHERS",                            true},
{E_DAPC_MM_SLAVE,         13,  "MMSYS_CONFIG",                          true},
{E_DAPC_MM_SLAVE,         14,  "MDP_RDMA0",                             true},
{E_DAPC_MM_SLAVE,         15,  "MDP_RDMA1",                             true},
{E_DAPC_MM_SLAVE,         16,  "MDP_RSZ0",                              true},
{E_DAPC_MM_SLAVE,         17,  "MDP_RSZ1",                              true},

 /* 250 */
{E_DAPC_MM_SLAVE,         18,  "MDP_WROT0",                             true},
{E_DAPC_MM_SLAVE,         19,  "MDP_WDMA",                              true},
{E_DAPC_MM_SLAVE,         20,  "MDP_TDSHP",                             true},
{E_DAPC_MM_SLAVE,         21,  "DISP_OVL0",                             true},
{E_DAPC_MM_SLAVE,         22,  "DISP_OVL0_2L",                          true},
{E_DAPC_MM_SLAVE,         23,  "DISP_OVL1_2L",                          true},
{E_DAPC_MM_SLAVE,         24,  "DISP_RDMA0",                            true},
{E_DAPC_MM_SLAVE,         25,  "DISP_RDMA1",                            true},
{E_DAPC_MM_SLAVE,         26,  "DISP_WDMA0",                            true},
{E_DAPC_MM_SLAVE,         27,  "DISP_COLOR0",                           true},

 /* 260 */
{E_DAPC_MM_SLAVE,         28,  "DISP_CCORR0",                           true},
{E_DAPC_MM_SLAVE,         29,  "DISP_AAL0",                             true},
{E_DAPC_MM_SLAVE,         30,  "DISP_GAMMA0",                           true},
{E_DAPC_MM_SLAVE,         31,  "DISP_DITHER0",                          true},
{E_DAPC_MM_SLAVE,         32,  "DSI_SPLIT",                             true},
{E_DAPC_MM_SLAVE,         33,  "DSI0",                                  true},
{E_DAPC_MM_SLAVE,         34,  "DPI",                                   true},
{E_DAPC_MM_SLAVE,         35,  "MM_MUTEX",                              true},
{E_DAPC_MM_SLAVE,         36,  "SMI_LARB0",                             true},
{E_DAPC_MM_SLAVE,         37,  "SMI_LARB1",                             true},

 /* 270 */
{E_DAPC_MM_SLAVE,         38,  "SMI_COMMON",                            true},
{E_DAPC_MM_SLAVE,         39,  "DISP_RSZ",                              true},
{E_DAPC_MM_SLAVE,         40,  "MDP_AAL",                               true},
{E_DAPC_MM_SLAVE,         41,  "MDP_CCORR",                             true},
{E_DAPC_MM_SLAVE,         42,  "DBI",                                   true},
{E_DAPC_MM_SLAVE,         43,  "MMSYS_RESERVE",                         true},
{E_DAPC_MM_SLAVE,         44,  "MMSYS_RESERVE",                         true},
{E_DAPC_MM_SLAVE,         45,  "MDP_WROT1",                             true},
{E_DAPC_MM_SLAVE,         46,  "DISP_POSTMASK0",                        true},
{E_DAPC_MM_SLAVE,         47,  "MMSYS_OTHERS",                          true},

 /* 280 */
{E_DAPC_MM_SLAVE,         48,  "imgsys1_top",                           true},
{E_DAPC_MM_SLAVE,         49,  "dip_a0",                                true},
{E_DAPC_MM_SLAVE,         50,  "dip_a1",                                true},
{E_DAPC_MM_SLAVE,         51,  "dip_a2",                                true},
{E_DAPC_MM_SLAVE,         52,  "dip_a3",                                true},
{E_DAPC_MM_SLAVE,         53,  "dip_a4",                                true},
{E_DAPC_MM_SLAVE,         54,  "dip_a5",                                true},
{E_DAPC_MM_SLAVE,         55,  "dip_a6",                                true},
{E_DAPC_MM_SLAVE,         56,  "dip_a7",                                true},
{E_DAPC_MM_SLAVE,         57,  "dip_a8",                                true},

 /* 290 */
{E_DAPC_MM_SLAVE,         58,  "dip_a9",                                true},
{E_DAPC_MM_SLAVE,         59,  "dip_a10",                               true},
{E_DAPC_MM_SLAVE,         60,  "dip_a11",                               true},
{E_DAPC_MM_SLAVE,         61,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         62,  "smi_larb5",                             true},
{E_DAPC_MM_SLAVE,         63,  "smi_larb6",                             true},
{E_DAPC_MM_SLAVE,         64,  "IMGSYS_OTHERS",                         true},
{E_DAPC_MM_SLAVE,         65,  "VENCSYS_GLOBAL_CON",                    true},
{E_DAPC_MM_SLAVE,         66,  "VENCSYSSYS_SMI_LARB3",                  true},
{E_DAPC_MM_SLAVE,         67,  "VENCSYS_VENC",                          true},

 /* 300 */
{E_DAPC_MM_SLAVE,         68,  "VENCSYS_JPGENC",                        true},
{E_DAPC_MM_SLAVE,         69,  "VENC_RESERVE",                          true},
{E_DAPC_MM_SLAVE,         70,  "VENC_RESERVE",                          true},
{E_DAPC_MM_SLAVE,         71,  "VENC_RESERVE",                          true},
{E_DAPC_MM_SLAVE,         72,  "VENCSYS_MBIST_CTRL",                    true},
{E_DAPC_MM_SLAVE,         73,  "VENCSYS_OTHERS",                        true},
{E_DAPC_MM_SLAVE,         74,  "VDECSYS_GLOBAL_CON",                    true},
{E_DAPC_MM_SLAVE,         75,  "vdec_smi_larbx",                        true},
{E_DAPC_MM_SLAVE,         76,  "VDECSYS_FULL_TOP",                      true},
{E_DAPC_MM_SLAVE,         77,  "vdec_full_top_lat",                     true},

 /* 310 */
{E_DAPC_MM_SLAVE,         78,  "VDEC_RESERVE",                          true},
{E_DAPC_MM_SLAVE,         79,  "VDEC_RESERVE",                          true},
{E_DAPC_MM_SLAVE,         80,  "VDECSYS_OTHERS",                        true},
{E_DAPC_MM_SLAVE,         81,  "CAMSYS_CAMSYS_TOP",                     true},
{E_DAPC_MM_SLAVE,         82,  "CAMSYS_LARB9",                          true},
{E_DAPC_MM_SLAVE,         83,  "CAMSYS_LARB10",                         true},
{E_DAPC_MM_SLAVE,         84,  "CAMSYS_LARB11",                         true},
{E_DAPC_MM_SLAVE,         85,  "CAMSYS_seninf_a",                       true},
{E_DAPC_MM_SLAVE,         86,  "CAMSYS_seninf_b",                       true},
{E_DAPC_MM_SLAVE,         87,  "CAMSYS_seninf_c",                       true},

 /* 320 */
{E_DAPC_MM_SLAVE,         88,  "CAMSYS_seninf_d",                       true},
{E_DAPC_MM_SLAVE,         89,  "CAMSYS_seninf_e",                       true},
{E_DAPC_MM_SLAVE,         90,  "CAMSYS_seninf_f",                       true},
{E_DAPC_MM_SLAVE,         91,  "CAMSYS_seninf_g",                       true},
{E_DAPC_MM_SLAVE,         92,  "CAMSYS_seninf_h",                       true},
{E_DAPC_MM_SLAVE,         93,  "CAMSYS_smi_sub_common",                 true},
{E_DAPC_MM_SLAVE,         94,  "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         95,  "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         96,  "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         97,  "cam_raw_a_ip_group_0",                  true},

 /* 330 */
{E_DAPC_MM_SLAVE,         98,  "cam_raw_a_ip_group_1",                  true},
{E_DAPC_MM_SLAVE,         99,  "cam_raw_a_ip_group_2",                  true},
{E_DAPC_MM_SLAVE,         100, "cam_raw_a_ip_group_3",                  true},
{E_DAPC_MM_SLAVE,         101, "cam_raw_a_dma_0",                       true},
{E_DAPC_MM_SLAVE,         102, "cam_raw_a_dma_1",                       true},
{E_DAPC_MM_SLAVE,         103, "ltm_curve_a_0",                         true},
{E_DAPC_MM_SLAVE,         104, "ltm_curve_a_1",                         true},
{E_DAPC_MM_SLAVE,         105, "cam_raw_a_ip_group_0_inner",            true},
{E_DAPC_MM_SLAVE,         106, "cam_raw_a_ip_group_1_inner",            true},
{E_DAPC_MM_SLAVE,         107, "cam_raw_a_ip_group_2_inner",            true},

 /* 340 */
{E_DAPC_MM_SLAVE,         108, "cam_raw_a_ip_group_3_inner",            true},
{E_DAPC_MM_SLAVE,         109, "cam_raw_a_dma_0_inner",                 true},
{E_DAPC_MM_SLAVE,         110, "cam_raw_a_dma_1_inner",                 true},
{E_DAPC_MM_SLAVE,         111, "ltm_curve_a_0_inner",                   true},
{E_DAPC_MM_SLAVE,         112, "ltm_curve_a_1_inner",                   true},
{E_DAPC_MM_SLAVE,         113, "cam_raw_b_ip_group_0",                  true},
{E_DAPC_MM_SLAVE,         114, "cam_raw_b_ip_group_1",                  true},
{E_DAPC_MM_SLAVE,         115, "cam_raw_b_ip_group_2",                  true},
{E_DAPC_MM_SLAVE,         116, "cam_raw_b_ip_group_3",                  true},
{E_DAPC_MM_SLAVE,         117, "cam_raw_b_dma_0",                       true},

 /* 350 */
{E_DAPC_MM_SLAVE,         118, "cam_raw_b_dma_1",                       true},
{E_DAPC_MM_SLAVE,         119, "ltm_curve_b_0",                         true},
{E_DAPC_MM_SLAVE,         120, "ltm_curve_b_1",                         true},
{E_DAPC_MM_SLAVE,         121, "cam_raw_b_ip_group_0_inner",            true},
{E_DAPC_MM_SLAVE,         122, "cam_raw_b_ip_group_1_inner",            true},
{E_DAPC_MM_SLAVE,         123, "cam_raw_b_ip_group_2_inner",            true},
{E_DAPC_MM_SLAVE,         124, "cam_raw_b_ip_group_3_inner",            true},
{E_DAPC_MM_SLAVE,         125, "cam_raw_b_dma_0_inner",                 true},
{E_DAPC_MM_SLAVE,         126, "cam_raw_b_dma_1_inner",                 true},
{E_DAPC_MM_SLAVE,         127, "ltm_curve_b_0_inner",                   true},

 /* 360 */
{E_DAPC_MM_SLAVE,         128, "ltm_curve_b_1_inner",                   true},
{E_DAPC_MM_SLAVE,         129, "cam_raw_c_ip_group_0",                  true},
{E_DAPC_MM_SLAVE,         130, "cam_raw_c_ip_group_1",                  true},
{E_DAPC_MM_SLAVE,         131, "cam_raw_c_ip_group_2",                  true},
{E_DAPC_MM_SLAVE,         132, "cam_raw_c_ip_group_3",                  true},
{E_DAPC_MM_SLAVE,         133, "cam_raw_c_dma_0",                       true},
{E_DAPC_MM_SLAVE,         134, "cam_raw_c_dma_1",                       true},
{E_DAPC_MM_SLAVE,         135, "ltm_curve_c_0",                         true},
{E_DAPC_MM_SLAVE,         136, "ltm_curve_c_1",                         true},
{E_DAPC_MM_SLAVE,         137, "cam_raw_c_ip_group_0_inner",            true},

 /* 370 */
{E_DAPC_MM_SLAVE,         138, "cam_raw_c_ip_group_1_inner",            true},
{E_DAPC_MM_SLAVE,         139, "cam_raw_c_ip_group_2_inner",            true},
{E_DAPC_MM_SLAVE,         140, "cam_raw_c_ip_group_3_inner",            true},
{E_DAPC_MM_SLAVE,         141, "cam_raw_c_dma_0_inner",                 true},
{E_DAPC_MM_SLAVE,         142, "cam_raw_c_dma_1_inner",                 true},
{E_DAPC_MM_SLAVE,         143, "ltm_curve_c_0_inner",                   true},
{E_DAPC_MM_SLAVE,         144, "ltm_curve_c_1_inner",                   true},
{E_DAPC_MM_SLAVE,         145, "cam_raw_a_AA_histogram_0",              true},
{E_DAPC_MM_SLAVE,         146, "cam_raw_a_AA_histogram_1",              true},
{E_DAPC_MM_SLAVE,         147, "cam_raw_a_AA_histogram_2",              true},

 /* 380 */
{E_DAPC_MM_SLAVE,         148, "cam_raw_a_MAE_histogram",               true},
{E_DAPC_MM_SLAVE,         149, "cam_raw_b_AA_histogram_0",              true},
{E_DAPC_MM_SLAVE,         150, "cam_raw_b_AA_histogram_1",              true},
{E_DAPC_MM_SLAVE,         151, "cam_raw_b_AA_histogram_2",              true},
{E_DAPC_MM_SLAVE,         152, "cam_raw_b_MAE_histogram",               true},
{E_DAPC_MM_SLAVE,         153, "cam_raw_c_AA_histogram_0",              true},
{E_DAPC_MM_SLAVE,         154, "cam_raw_c_AA_histogram_1",              true},
{E_DAPC_MM_SLAVE,         155, "cam_raw_c_AA_histogram_2",              true},
{E_DAPC_MM_SLAVE,         156, "cam_raw_c_MAE_histogram",               true},
{E_DAPC_MM_SLAVE,         157, "CAMSYS_RESERVED",                       true},

 /* 390 */
{E_DAPC_MM_SLAVE,         158, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         159, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         160, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         161, "CAMSYS_adl",                            true},
{E_DAPC_MM_SLAVE,         162, "camsv_0",                               true},
{E_DAPC_MM_SLAVE,         163, "camsv_1",                               true},
{E_DAPC_MM_SLAVE,         164, "camsv_2",                               true},
{E_DAPC_MM_SLAVE,         165, "camsv_3",                               true},
{E_DAPC_MM_SLAVE,         166, "camsv_4",                               true},
{E_DAPC_MM_SLAVE,         167, "camsv_5",                               true},

 /* 400 */
{E_DAPC_MM_SLAVE,         168, "camsv_6",                               true},
{E_DAPC_MM_SLAVE,         169, "camsv_7",                               true},
{E_DAPC_MM_SLAVE,         170, "CAMSYS_asg",                            true},
{E_DAPC_MM_SLAVE,         171, "cam_raw_a_set",                         true},
{E_DAPC_MM_SLAVE,         172, "cam_raw_a_clr",                         true},
{E_DAPC_MM_SLAVE,         173, "cam_raw_b_set",                         true},
{E_DAPC_MM_SLAVE,         174, "cam_raw_b_clr",                         true},
{E_DAPC_MM_SLAVE,         175, "cam_raw_c_set",                         true},
{E_DAPC_MM_SLAVE,         176, "cam_raw_c_clr",                         true},
{E_DAPC_MM_SLAVE,         177, "CAMSYS_RESERVED",                       true},

 /* 410 */
{E_DAPC_MM_SLAVE,         178, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         179, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         180, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         181, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         182, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         183, "CAMSYS_tsf",                            true},
{E_DAPC_MM_SLAVE,         184, "ccu_ctl",                               true},
{E_DAPC_MM_SLAVE,         185, "ccu_h2t_a",                             true},
{E_DAPC_MM_SLAVE,         186, "ccu_t2h_a",                             true},
{E_DAPC_MM_SLAVE,         187, "ccu_dma",                               true},

 /* 420 */
{E_DAPC_MM_SLAVE,         188, "CAMSYS_RESERVED",                       true},
{E_DAPC_MM_SLAVE,         189, "DCCM",                                  true},
{E_DAPC_MM_SLAVE,         190, "CAMSYS_RESEVE",                         true},
{E_DAPC_MM_SLAVE,         191, "CAMSYS_OTHERS",                         true},
{E_DAPC_MM_SLAVE,         192, "APUSYS_apu_conn_config",                true},
{E_DAPC_MM_SLAVE,         193, "APUSYS_ADL_CTRL",                       true},
{E_DAPC_MM_SLAVE,         194, "APUSYS_vcore_config",                   true},
{E_DAPC_MM_SLAVE,         195, "APUSYS_apu_edma",                       true},
{E_DAPC_MM_SLAVE,         196, "APUSYS_apu1_edmb",                      true},
{E_DAPC_MM_SLAVE,         197, "APUSYS_apu2_edmc",                      true},

 /* 430 */
{E_DAPC_MM_SLAVE,         198, "APUSYS_COREA_DMEM_0",                   true},
{E_DAPC_MM_SLAVE,         199, "APUSYS_COREA_DMEM_1",                   true},
{E_DAPC_MM_SLAVE,         200, "APUSYS_COREA_IMEM",                     true},
{E_DAPC_MM_SLAVE,         201, "APUSYS_COREA_CONTROL",                  true},
{E_DAPC_MM_SLAVE,         202, "APUSYS_COREA_DEBUG",                    true},
{E_DAPC_MM_SLAVE,         203, "APUSYS_COREB_DMEM_0",                   true},
{E_DAPC_MM_SLAVE,         204, "APUSYS_COREB_DMEM_1",                   true},
{E_DAPC_MM_SLAVE,         205, "APUSYS_COREB_IMEM",                     true},
{E_DAPC_MM_SLAVE,         206, "APUSYS_COREB_CONTROL",                  true},
{E_DAPC_MM_SLAVE,         207, "APUSYS_COREB_DEBUG",                    true},

 /* 440 */
{E_DAPC_MM_SLAVE,         208, "APUSYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         209, "APUSYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         210, "APUSYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         211, "APUSYS_COREC_CONTROL",                  true},
{E_DAPC_MM_SLAVE,         212, "APUSYS_COREC_MDLA",                     true},
{E_DAPC_MM_SLAVE,         213, "APUSYS_OTHERS",                         true},
{E_DAPC_MM_SLAVE,         214, "IPESYS_ipesys_top",                     true},
{E_DAPC_MM_SLAVE,         215, "IPESYS_fdvt",                           true},
{E_DAPC_MM_SLAVE,         216, "IPESYS_fe",                             true},
{E_DAPC_MM_SLAVE,         217, "IPESYS_rsc",                            true},

 /* 450 */
{E_DAPC_MM_SLAVE,         218, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         219, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         220, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         221, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         222, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         223, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         224, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         225, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         226, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         227, "IPESYS_ RESERVED",                      true},

 /* 460 */
{E_DAPC_MM_SLAVE,         228, "IPESYS_smi_2x1_sub_common",             true},
{E_DAPC_MM_SLAVE,         229, "IPESYS_smi_larb8",                      true},
{E_DAPC_MM_SLAVE,         230, "IPESYS_depth",                          true},
{E_DAPC_MM_SLAVE,         231, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         232, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         233, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         234, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         235, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         236, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         237, "IPESYS_ RESERVED",                      true},

 /* 470 */
{E_DAPC_MM_SLAVE,         238, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         239, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         240, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         241, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         242, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         243, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         244, "IPESYS_ RESERVED",                      true},
{E_DAPC_MM_SLAVE,         245, "IPESYS_smi_larb7",                      true},
{E_DAPC_MM_SLAVE,         246, "IPESYS_OTHERS",                         true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_UNALIGN",                          true},

 /* 480 */
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_OUT_OF_BOUND",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_ERR_WAY_EN",                       true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB_UNALIGN",                  true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB _OUT_OF_BOUND",            true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB _ERR_WAY_EN",              true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB_UNALIGN",                 true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB _OUT_OF_BOUND",           true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB _ERR_WAY_EN",             true},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST_PERIAPB_UNALIGN",                  true},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST _PERIAPB _OUT_OF_BOUND",           true},

 /* 490 */
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST _PERIAPB _ERR_WAY_EN",             true},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH_PERIAPB_UNALIGN",                 true},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH _PERIAPB _OUT_OF_BOUND",          true},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH _PERIAPB _ERR_WAY_EN",            true},
{E_DAPC_OTHERS_SLAVE,     -1,  "INFRA_PDN_DECODE_ERROR",                true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_UNALIGN",                       true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_OUT_OF_BOUND",                  true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_ERR_WAY_EN",                    true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_NORTH_PERIAPB_UNALIGN",            true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_NORTH _PERIAPB _OUT_OF_BOUND",     true},

 /* 500 */
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_NORTH _PERIAPB _ERR_WAY_EN",       true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI2_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI1_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI0_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAXI_SI0_DECERR",                    true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAXI_SI1_DECERR",                    true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI3_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI4_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SRAMROM_DECERR",                        true},
{E_DAPC_OTHERS_SLAVE,     -1,  "HIFI3_DECERR",                          true},

 /* 510 */
{E_DAPC_OTHERS_SLAVE,     -1,  "MD_DECERR",                             true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SRAMROM*",                              true},
{E_DAPC_OTHERS_SLAVE,     -1,  "AP_DMA*",                               false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC_AO_INFRA_PERI*",             false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC_AO_MD*",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC_AO_MM*",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "CM_DQ_SECURE*",                         false},
{E_DAPC_OTHERS_SLAVE,     -1,  "MM_IOMMU_DOMAIN*",                      false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DISP_GCE*",                             false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC*",                           false},

 /* 520 */
{E_DAPC_OTHERS_SLAVE,     -1,  "EMI*",                                  false},
{E_DAPC_OTHERS_SLAVE,     -1,  "EMI_MPU*",                              false},
{E_DAPC_OTHERS_SLAVE,     -1,  "PMIC_WRAP*",                            false},
{E_DAPC_OTHERS_SLAVE,     -1,  "I2C1*",                                 false},
{E_DAPC_OTHERS_SLAVE,     -1,  "I2C2*",                                 false},
{E_DAPC_OTHERS_SLAVE,     -1,  "I2C4*",                                 false},

};

static struct PERIAXI_ID_INFO paxi_int_mi_id_to_master[] = {
	{"APDMA",       { 0, 1, 2, 0, 0 } },
	{"MD",          { 1, 0, 2, 2, 0 } },
	{"SPM",         { 0, 0, 1, 0, 0 } },
	{"CCU",         { 0, 0, 0, 0, 0 } },
	{"SPM",         { 0, 0, 0, 1, 0 } },
	{"N/A",         { 0, 0, 0, 0, 1 } },
	{"THERM",       { 0, 0, 0, 1, 1 } },
};

static struct TOPAXI_ID_INFO topaxi_mi0_id_to_master[] = {
	{"DebugTop",          { 1, 0, 0, 0,	0, 0, 2, 0,	0, 0, 0, 0 } },
	{"Audio",             { 1, 0, 0, 1,	0, 0, 0, 0,	0, 1, 0, 0 } },
	{"IPU",               { 1, 0, 0, 1,	0, 0, 0, 0,	0, 0, 1, 0 } },
	{"SPI1",              { 1, 0, 0, 1,	0, 0, 0, 0,	0, 1, 1, 0 } },
	{"SPI7",              { 1, 0, 0, 1,	0, 0, 0, 0,	0, 0, 0, 0 } },
	{"CCU",               { 1, 0, 0, 1,	0, 0, 1, 0,	0, 0, 0, 0 } },
	{"SPM ",              { 1, 0, 0, 1,	0, 0, 1, 0,	0, 1, 0, 0 } },
	{"N/A",               { 1, 0, 0, 1,	0, 0, 1, 0,	0, 0, 1, 0 } },
	{"THERM",             { 1, 0, 0, 1,	0, 0, 1, 0,	0, 1, 1, 0 } },
	{"UFS",               { 1, 0, 0, 1,	0, 0, 0, 1,	0, 2, 2, 0 } },
	{"DMA_EXT",           { 1, 0, 0, 1,	0, 0, 1, 1,	0, 2, 0, 0 } },
	{"SSUSB",             { 1, 0, 0, 1,	0, 0, 0, 0,	1, 0, 2, 2 } },
	{"PWM",               { 1, 0, 0, 1,	0, 0, 0, 0,	1, 1, 0, 0 } },
	{"MSDC1",             { 1, 0, 0, 1,	0, 0, 0, 0,	1, 1, 1, 0 } },
	{"SPI6",              { 1, 0, 0, 1,	0, 0, 0, 0,	1, 1, 0, 1 } },
	{"SPI0",              { 1, 0, 0, 1,	0, 0, 0, 0,	1, 1, 1, 1 } },
	{"SPI2",              { 1, 0, 0, 1,	0, 0, 1, 0,	1, 0, 0, 0 } },
	{"SPI3",              { 1, 0, 0, 1,	0, 0, 1, 0,	1, 1, 0, 0 } },
	{"SPI4",              { 1, 0, 0, 1,	0, 0, 1, 0,	1, 0, 1, 0 } },
	{"SPI5",              { 1, 0, 0, 1,	0, 0, 1, 0,	1, 1, 1, 0 } },
	{"MSDC0",             { 1, 0, 0, 1,	0, 0, 0, 1,	1, 2, 2, 2 } },
	{"DX_CC",             { 1, 0, 0, 0,	1, 0, 2, 2,	2, 2, 0, 0 } },
	{"CQ_DMA",            { 1, 0, 0, 1,	1, 0, 2, 2,	2, 0, 0, 0 } },
	{"GCE_M",             { 1, 0, 0, 0,	0, 1, 2, 2,	0, 0, 0, 0 } },
	{"HiFi3",             { 1, 1, 0, 0,	2, 2, 2, 2,	2, 0, 0, 0 } },
	{"CLDMA(dpmaif)",     { 1, 1, 0, 1,	2, 2, 2, 2,	0, 0, 0, 0 } },
	{"SSPM",              { 1, 0, 1, 1,	2, 2, 0, 0,	0, 0, 0, 0 } },
	{"SCP",               { 1, 0, 1, 0,	2, 2, 0, 0,	0, 0, 0, 0 } },
	{"CONNSYS",           { 1, 1, 1, 2,	2, 2, 0, 0,	0, 0, 0, 0 } },
	{"DSU_write",         { 0, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0 } },
	{"DSU_write",         { 0, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0 } },
	{"DSU_write",         { 0, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0 } },
	{"DSU_read",          { 0, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0 } },
	{"DSU_read",          { 0, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0 } },
	{"DSU_read",          { 0, 2, 0, 0,	0, 0, 0, 0,	1, 0, 0, 0 } },
	{"DSU_read",          { 0, 2, 1, 0,	0, 0, 0, 0,	1, 0, 0, 0 } },
	{"DSU_read",          { 0, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0 } },
	{"DSU_read",          { 0, 2, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0 } },
};

static const char *topaxi_mi0_trans(int bus_id)
{
	const char *master = "UNKNOWN_MASTER_FROM_TOPAXI";
	int master_count = ARRAY_SIZE(topaxi_mi0_id_to_master);
	int i, j;

	for (i = 0 ; i < master_count; i++) {
		for (j = 0 ; j < TOPAXI_MI0_BIT_LENGTH ; j++) {
			if (topaxi_mi0_id_to_master[i].bit[j] == 2)
				continue;

			if (((bus_id >> j) & 0x1) ==
				topaxi_mi0_id_to_master[i].bit[j]) {
				continue;
			} else {
				break;
			}
		}
		if (j == TOPAXI_MI0_BIT_LENGTH) {
			DEVAPC_MSG("%s %s %s %s\n",
				"[DEVAPC]",
				"catch it from TOPAXI_MI0.",
				"Master is:",
				topaxi_mi0_id_to_master[i].master);
			master = topaxi_mi0_id_to_master[i].master;
		}
	}

	return master;
}

static const char *paxi_int_mi_trans(int bus_id)
{
	const char *master = "UNKNOWN_MASTER_FROM_PAXI";
	int master_count = ARRAY_SIZE(paxi_int_mi_id_to_master);
	int i, j;

	if ((bus_id & 0x3) == 0x3) {
		master = topaxi_mi0_trans(bus_id >> 2);
		return master;
	}

	for (i = 0 ; i < master_count; i++) {
		for (j = 0 ; j < PERIAXI_INT_MI_BIT_LENGTH ; j++) {
			if (paxi_int_mi_id_to_master[i].bit[j] == 2)
				continue;

			if (((bus_id >> j) & 0x1) ==
				paxi_int_mi_id_to_master[i].bit[j]) {
				continue;
			} else {
				break;
			}
		}
		if (j == PERIAXI_INT_MI_BIT_LENGTH) {
			DEVAPC_MSG("%s %s %s %s\n",
				"[DEVAPC]",
				"catch it from PERIAXI_INT_MI.",
				"Master is:",
				paxi_int_mi_id_to_master[i].master);
			master = paxi_int_mi_id_to_master[i].master;
		}
	}

	return master;
}

const char *bus_id_to_master(int bus_id, uint32_t vio_addr, int vio_idx)
{
	uint32_t h_byte;
	const char *master = "UNKNOWN_MASTER";

	DEVAPC_DBG_MSG("[DEVAPC] bus id = 0x%x, vio_addr = 0x%x\n",
		bus_id, vio_addr);

	/* SPM MTCMOS disable will set way_en[7:4] reg to block transaction,
	 * and it will triggered TOPAXI_SI0_DECERR instead of slave vio.
	 */
	if (vio_idx == TOPAXI_SI0_DECERR) {
		DEVAPC_DBG_MSG("[DEVAPC] vio is from TOPAXI_SI0_DECERR\n");
		master = topaxi_mi0_trans(bus_id);

		return master;
	}

	h_byte = (vio_addr >> 24) & 0xFF;

	/* to Infra/Peri/Audio/MD/CONN
	 * or MMSYS
	 * or MFG
	 */
	if (((h_byte >> 4) == 0x0) && h_byte != 0x0C && h_byte != 0x0D) {
		DEVAPC_DBG_MSG("[DEVAPC] vio addr is from on-chip SRAMROM\n");
		master = topaxi_mi0_trans(bus_id);

	} else if (h_byte == 0x10 || h_byte == 0x11 || h_byte == 0x18 ||
		h_byte == 0x0C || h_byte == 0x0D ||
		(h_byte >> 4) == 0x2 || (h_byte >> 4) == 0x3) {
		DEVAPC_DBG_MSG("[DEVAPC] vio addr is from Infra/Peri\n");

		/* CONN */
		if (h_byte == 0x18) {
			if ((bus_id & 0x1) == 1)
				return "CQ_DMA"; /*should not be walked */
			bus_id = bus_id >> 1;
		}
		/* HIFI3/SCP/SSPM */
		h_byte = (vio_addr >> 20) & 0xFFF;
		if (h_byte == 0x106 || h_byte == 0x105 || h_byte == 0x104) {
			if ((bus_id & 0x1) == 0)
				return "MD";
			bus_id = bus_id >> 1;
		}
		master = paxi_int_mi_trans(bus_id);

	} else if (h_byte == 0x14 || h_byte == 0x15 || h_byte == 0x16 ||
		h_byte == 0x17 || h_byte == 0x19 || h_byte == 0x1A ||
		h_byte == 0x1B) {
		DEVAPC_DBG_MSG("[DEVAPC] vio addr is from MM\n");
		if ((bus_id & 0x1) == 1)
			return "GCE";
		master = topaxi_mi0_trans(bus_id >> 1);

	} else if (h_byte == 0x13) {
		DEVAPC_DBG_MSG("[DEVAPC] vio addr is from MFG\n");
		master = topaxi_mi0_trans(bus_id);

	} else {
		master = "UNKNOWN_MASTER";
	}

	return master;
}

/* violation index corresponds to subsys */
const char *index_to_subsys(uint32_t index)
{
	if (index >= MFGSYS_START && index <= MFGSYS_END)
		return "MFGSYS";
	else if (index == SMI_LARB0 || index == SMI_LARB1 ||
			index == SMI_COMMON || index == SMI_LARB5 ||
			index == SMI_LARB6 || index == VENCSYS_SMI_LARB ||
			index == VDECSYS_SMI_LARB || index == CAMSYS_SMI_LARB ||
			index == IPESYS_SMI_LARB || index == IPESYS_SMI_LARB8 ||
			index == IPESYS_SMI_LARB7)
		return "SMI";
	else if ((index >= MMSYS_MDP_START && index <= MMSYS_MDP_END) ||
			(index >= MMSYS_MDP2_START && index <= MMSYS_MDP2_END))
		return "MMSYS_MDP";
	else if (index >= MMSYS_DISP_START && index <= MMSYS_DISP_END)
		return "MMSYS_DISP";
	else if (index >= IMGSYS_START && index <= IMGSYS_END)
		return "IMGSYS";
	else if (index >= VENCSYS_START && index <= VENCSYS_END)
		return "VENCSYS";
	else if (index >= VDECSYS_START && index <= VDECSYS_END)
		return "VDECSYS";
	else if (index >= CAMSYS_START && index <= CAMSYS_END)
		return "CAMSYS";
	else if (index >= APUSYS_START && index <= APUSYS_END)
		return "APUSYS";
	else if (index >= IPESYS_START && index <= IPESYS_END)
		return "IPESYS";
	else if (index < ARRAY_SIZE(mt6779_infra_devices))
		return mt6779_infra_devices[index].device;
	else
		return "OUT_OF_BOUND";
}

static ssize_t mt6779_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	return mtk_devapc_dbg_read(file, buffer, count, ppos);
}

static ssize_t mt6779_devapc_dbg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	return mtk_devapc_dbg_write(file, buffer, count, data);
}

static const struct file_operations devapc_dbg_fops = {
	.owner = THIS_MODULE,
	.write = mt6779_devapc_dbg_write,
	.read = mt6779_devapc_dbg_read,
};

static struct mtk_devapc_dbg_status mt6779_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static struct mtk_devapc_vio_info mt6779_devapc_vio_info = {
	.vio_cfg_max_idx = PLAT_VIO_CFG_MAX_IDX,
	.vio_max_idx = PLAT_VIO_MAX_IDX,
	.vio_mask_sta_num = PLAT_VIO_MASK_STA_NUM,
	.vio_shift_max_bit = PLAT_VIO_SHIFT_MAX_BIT,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
};

static const struct mtk_infra_vio_dbg_desc mt6779_vio_dbgs = {
	.infra_vio_dbg_mstid = INFRA_VIO_DBG_MSTID,
	.infra_vio_dbg_mstid_start_bit = INFRA_VIO_DBG_MSTID_START_BIT,
	.infra_vio_dbg_dmnid = INFRA_VIO_DBG_DMNID,
	.infra_vio_dbg_dmnid_start_bit = INFRA_VIO_DBG_DMNID_START_BIT,
	.infra_vio_dbg_w_vio = INFRA_VIO_DBG_W_VIO,
	.infra_vio_dbg_w_vio_start_bit = INFRA_VIO_DBG_W_VIO_START_BIT,
	.infra_vio_dbg_r_vio = INFRA_VIO_DBG_R_VIO,
	.infra_vio_dbg_r_vio_start_bit = INFRA_VIO_DBG_R_VIO_START_BIT,
	.infra_vio_addr_high = INFRA_VIO_ADDR_HIGH,
	.infra_vio_addr_high_start_bit = INFRA_VIO_ADDR_HIGH_START_BIT,
};

static const struct mtk_sramrom_sec_vio_desc mt6779_sramrom_sec_vios = {
	.sramrom_sec_vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.sramrom_sec_vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.sramrom_sec_vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.sramrom_sec_vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.sramrom_sec_vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.sramrom_sec_vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const struct mtk_devapc_pd_desc mt6779_devapc_pds = {
	.pd_vio_mask_offset = PD_VIO_MASK_OFFSET,
	.pd_vio_sta_offset = PD_VIO_STA_OFFSET,
	.pd_vio_dbg0_offset = PD_VIO_DBG0_OFFSET,
	.pd_vio_dbg1_offset = PD_VIO_DBG1_OFFSET,
	.pd_apc_con_offset = PD_APC_CON_OFFSET,
	.pd_shift_sta_offset = PD_SHIFT_STA_OFFSET,
	.pd_shift_sel_offset = PD_SHIFT_SEL_OFFSET,
	.pd_shift_con_offset = PD_SHIFT_CON_OFFSET,
};

static struct mtk_devapc_soc mt6779_data = {
	.dbg_stat = &mt6779_devapc_dbg_stat,
	.device_info = mt6779_infra_devices,
	.ndevices = ARRAY_SIZE(mt6779_infra_devices),
	.vio_info = &mt6779_devapc_vio_info,
	.vio_dbgs = &mt6779_vio_dbgs,
	.sramrom_sec_vios = &mt6779_sramrom_sec_vios,
	.devapc_pds = &mt6779_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &bus_id_to_master,
};

static const struct of_device_id mt6779_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6779-devapc" },
	{},
};

static int mt6779_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6779_data);
}

static int mt6779_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6779_devapc_driver = {
	.probe = mt6779_devapc_probe,
	.remove = mt6779_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.of_match_table = mt6779_devapc_dt_match,
	},
};

/*
 * devapc_init: module init function.
 */
static int __init mt6779_devapc_init(void)
{
	int ret;

	DEVAPC_MSG("module initialized\n");

	ret = platform_driver_register(&mt6779_devapc_driver);
	if (ret) {
		pr_err("Unable to register driver, ret(%d)\n", ret);
		return ret;
	}

	proc_create("devapc_dbg", 0664, NULL, &devapc_dbg_fops);

	return 0;
}

arch_initcall(mt6779_devapc_init);

MODULE_DESCRIPTION("Mediatek MT6799 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
