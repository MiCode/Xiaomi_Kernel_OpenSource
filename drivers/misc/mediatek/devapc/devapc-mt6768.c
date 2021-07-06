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
#include "devapc-mt6768.h"

static struct mtk_device_info mt6768_infra_devices[] = {
/* slave type,       config_idx, device name                enable_vio_irq */

 /* 0 */
{E_DAPC_INFRA_PERI_SLAVE, 0,   "INFRA_AO_TOPCKGEN",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 1,   "INFRA_AO_INFRASYS_CONFIG_REGS",         true},
{E_DAPC_INFRA_PERI_SLAVE, 2,   "IO_CFG_REG",                            true},
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
{E_DAPC_INFRA_PERI_SLAVE, 14,  "INFRA_AO_DEVICE_APC_AO_INFRA_PERI",     true},
{E_DAPC_INFRA_PERI_SLAVE, 15,  "INFRA_AO_SLEEP_CONTROLLER_MD",          true},
{E_DAPC_INFRA_PERI_SLAVE, 16,  "INFRA_AO_KEYPAD",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 17,  "INFRA_AO_TOP_MISC",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 18,  "INFRA_AO_ DVFS_CTRL_PROC",              true},
{E_DAPC_INFRA_PERI_SLAVE, 19,  "INFRA_AO_MBIST_AO_REG",                 true},

 /* 20 */
{E_DAPC_INFRA_PERI_SLAVE, 20,  "INFRA_AO_CLDMA_AO_AP",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 21,  "INFRA_AO_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 22,  "INFRA_AO_AES_TOP_0",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 23,  "INFRA_AO_SYS_TIMER",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 24,  "INFRA_AO_MDEM_TEMP_SHARE",              true},
{E_DAPC_INFRA_PERI_SLAVE, 25,  "INFRA_AO_DEVICE_APC_AO_MD",             true},
{E_DAPC_INFRA_PERI_SLAVE, 26,  "INFRA_AO_SECURITY_AO",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 27,  "INFRA_AO_TOPCKGEN_REG",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 28,  "INFRA_AO_DEVICE_APC_AO_MM",             true},
{E_DAPC_INFRA_PERI_SLAVE, 29,  "INFRA_AO_RESERVE",                      true},

 /* 30 */
{E_DAPC_INFRA_PERI_SLAVE, 30,  "INFRA_AO_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 31,  "INFRA_AO_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 32,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 33,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 34,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 35,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 36,  "INFRASYS_SYS_CIRQ",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 37,  "INFRASYS_MM_IOMMU",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 38,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 39,  "INFRASYS_DEVICE_APC",                   true},

 /* 40 */
{E_DAPC_INFRA_PERI_SLAVE, 40,  "INFRASYS_DBG_TRACKER",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 41,  "INFRASYS_CCIF0_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 42,  "INFRASYS_CCIF0_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 43,  "INFRASYS_CCIF1_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 44,  "INFRASYS_CCIF1_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 45,  "INFRASYS_MBIST",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 46,  "INFRASYS_INFRA_PDN_REGISTER",           true},
{E_DAPC_INFRA_PERI_SLAVE, 47,  "INFRASYS_TRNG",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 48,  "INFRASYS_DX_CC",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 49,  "INFRASYS_MCUPM_SRAM2",                  true},

 /* 50 */
{E_DAPC_INFRA_PERI_SLAVE, 50,  "INFRASYS_CQ_DMA",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 51,  "INFRASYS_MCUPM_SRAM3",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 52,  "INFRASYS_SRAMROM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 53,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 54,  "INFRASYS_MCUPM_REG",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 55,  "INFRASYS_MCUPM_SRAM0",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 56,  "INFRASYS_MCUPM_SRAM1",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 57,  "INFRASYS_EMI",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 58,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 59,  "INFRASYS_CLDMA_PDN_AP",                 true},

 /* 60 */
{E_DAPC_INFRA_PERI_SLAVE, 60,  "INFRASYS_CLDMA_PDN_MD",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 61,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 62,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 63,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 64,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 65,  "INFRASYS_EMI_MPU",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 66,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 67,  "INFRASYS_DRAMC_CH0_TOP0",               true},
{E_DAPC_INFRA_PERI_SLAVE, 68,  "INFRASYS_DRAMC_CH0_TOP1",               true},
{E_DAPC_INFRA_PERI_SLAVE, 69,  "INFRASYS_DRAMC_CH0_TOP2",               true},

 /* 70 */
{E_DAPC_INFRA_PERI_SLAVE, 70,  "INFRASYS_DRAMC_CH0_TOP3",               true},
{E_DAPC_INFRA_PERI_SLAVE, 71,  "INFRASYS_DRAMC_CH0_TOP4",               true},
{E_DAPC_INFRA_PERI_SLAVE, 72,  "INFRASYS_DRAMC_CH1_TOP0",               true},
{E_DAPC_INFRA_PERI_SLAVE, 73,  "INFRASYS_DRAMC_CH1_TOP1",               true},
{E_DAPC_INFRA_PERI_SLAVE, 74,  "INFRASYS_DRAMC_CH1_TOP2",               true},
{E_DAPC_INFRA_PERI_SLAVE, 75,  "INFRASYS_DRAMC_CH1_TOP3",               true},
{E_DAPC_INFRA_PERI_SLAVE, 76,  "INFRASYS_DRAMC_CH1_TOP4",               true},
{E_DAPC_INFRA_PERI_SLAVE, 77,  "INFRASYS_GCE",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 78,  "INFRASYS_CCIF2_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 79,  "INFRASYS_CCIF2_MD",                     true},

 /* 80 */
{E_DAPC_INFRA_PERI_SLAVE, 80,  "INFRASYS_CCIF3_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 81,  "INFRASYS_CCIF3_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 82,  "INFRA_AO_SSPM_1_1",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 83,  "INFRA_AO_SSPM_1_2",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 84,  "INFRA_AO_SSPM_1_3",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 85,  "INFRA_AO_SSPM_2",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 86,  "INFRA_AO_SSPM_3",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 87,  "INFRA_AO_SSPM_4",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 88,  "INFRA_AO_SSPM_5",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 89,  "INFRA_AO_SSPM_6",                       true},

 /* 90 */
{E_DAPC_INFRA_PERI_SLAVE, 90,  "INFRA_AO_SSPM_7",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 91,  "INFRA_AO_SSPM_8",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 92,  "INFRA_AO_SCP",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 93,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 94,  "INFRASYS_DBUGSYS",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 95,  "PERISYS_APDMA",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 96,  "PERISYS_AUXADC",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 97,  "PERISYS_UART0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 98,  "PERISYS_UART1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 99,  "PERISYS_I2C7",                          true},

 /* 100 */
{E_DAPC_INFRA_PERI_SLAVE, 100, "PERISYS_I2C8",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 101, "PERISYS_PWM",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 102, "PERISYS_I2C0",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 103, "PERISYS_I2C1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 104, "PERISYS_I2C2",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 105, "PERISYS_SPI0",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 106, "PERISYS_PTP",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 107, "PERISYS_BTIF",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 108, "PERISYS_I2C6",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 109, "PERISYS_DISP_PWM",                      true},

 /* 110 */
{E_DAPC_INFRA_PERI_SLAVE, 110, "PERISYS_I2C3",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 111, "PERISYS_SPI1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 112, "PERISYS_I2C4",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 113, "PERISYS_SPI2",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 114, "PERISYS_SPI3",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 115, "PERISYS_SPI4",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 116, "PERISYS_SPI5",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 117, "PERISYS_I2C5",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 118, "PERISYS_IMP_IIC_WRAP",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 119, "PERISYS_NFI",                           true},

 /* 120 */
{E_DAPC_INFRA_PERI_SLAVE, 120, "PERISYS_NFIECC",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 121, "PERISYS_USB",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 122, "PERISYS_USB_2.0_SUB",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 123, "PERISYS_MSDC0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 124, "PERISYS_MSDC1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 125, "PERISYS_RESERVE",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 126, "PERISYS_RESERVE",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 127, "PERISYS_RESERVE",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 128, "PERISYS_RESERVE",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 129, "PERISYS_RESERVE",                       true},

 /* 130 */
{E_DAPC_INFRA_PERI_SLAVE, 130, "PERISYS_RESERVE",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 131, "PERISYS_AUDIO",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 132, "EAST_RESERVE",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 133, "EAST_ CSI_TOP_AO",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 134, "EAST_ RESERVE",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 135, "EAST_ RESERVE",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 136, "SOUTH_ RESERVE",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 137, "SOUTH_EFUSE",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 138, "SOUTH_RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 139, "SOUTH_RESERVE_2",                       true},

 /* 140 */
{E_DAPC_INFRA_PERI_SLAVE, 140, "WEST_MIPI_TX_CONFIG",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 141, "WEST_MSDC1",                            true},
{E_DAPC_INFRA_PERI_SLAVE, 142, "WEST_RESERVE_1",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 143, "WEST_RESERVE_2",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 144, "NORTH_USBSIF_TOP",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 145, "NORTH_MSDC0",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 146, "NORTH_RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 147, "NORTH_RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 148, "PERISYS_CONN",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 149, "PERISYS_MD1",                           true},

 /* 150 */
{E_DAPC_INFRA_PERI_SLAVE, 150, "PERISYS_RESERVE",                       true},
{E_DAPC_MM_SLAVE,         0,   "GPU config",                            true},
{E_DAPC_MM_SLAVE,         1,   "GPU IP",                                true},
{E_DAPC_MM_SLAVE,         2,   "GPU mali_dvfs_hint",                    true},
{E_DAPC_MM_SLAVE,         3,   "MFG_OTHERS",                            true},
{E_DAPC_MM_SLAVE,         4,   "MMSYS_CONFIG",                          true},
{E_DAPC_MM_SLAVE,         5,   "DISP_MUTEX",                            true},
{E_DAPC_MM_SLAVE,         6,   "SMI_COMMON",                            true},
{E_DAPC_MM_SLAVE,         7,   "SMI_LARB0",                             true},
{E_DAPC_MM_SLAVE,         8,   "MDP_RDMA0",                             true},

 /* 160 */
{E_DAPC_MM_SLAVE,         9,   "MDP_CCORR0",                            true},
{E_DAPC_MM_SLAVE,         10,  "MDP_RSZ0",                              true},
{E_DAPC_MM_SLAVE,         11,  "MDP_RSZ1",                              true},
{E_DAPC_MM_SLAVE,         12,  "MDP_WDMA0",                             true},
{E_DAPC_MM_SLAVE,         13,  "MDP_WROT0",                             true},
{E_DAPC_MM_SLAVE,         14,  "MDP_TDSHP0",                            true},
{E_DAPC_MM_SLAVE,         15,  "DISP_OVL0",                             true},
{E_DAPC_MM_SLAVE,         16,  "DISP_OVL0_2L",                          true},
{E_DAPC_MM_SLAVE,         17,  "DISP_RDMA0",                            true},
{E_DAPC_MM_SLAVE,         18,  "DISP_WDMA0",                            true},

 /* 170 */
{E_DAPC_MM_SLAVE,         19,  "DISP_COLOR0",                           true},
{E_DAPC_MM_SLAVE,         20,  "DISP_CCORR0",                           true},
{E_DAPC_MM_SLAVE,         21,  "DISP_AAL0",                             true},
{E_DAPC_MM_SLAVE,         22,  "DISP_GAMMA0",                           true},
{E_DAPC_MM_SLAVE,         23,  "DISP_DITHER0",                          true},
{E_DAPC_MM_SLAVE,         24,  "DSI0",                                  true},
{E_DAPC_MM_SLAVE,         25,  "DISP_RSZ0",                             true},
{E_DAPC_MM_SLAVE,         26,  "MM_MUTEX",                              true},
{E_DAPC_MM_SLAVE,         27,  "SMI_LARB0",                             true},
{E_DAPC_MM_SLAVE,         28,  "SMI_LARB1",                             true},

 /* 180 */
{E_DAPC_MM_SLAVE,         29,  "SMI_COMMON",                            true},
{E_DAPC_MM_SLAVE,         30,  "IMGSYS_CONFIG",                         true},
{E_DAPC_MM_SLAVE,         31,  "IMGSYS_SMI_LARB2",                      true},
{E_DAPC_MM_SLAVE,         32,  "IMGSYS_DISP_A0",                        true},
{E_DAPC_MM_SLAVE,         33,  "IMGSYS_DISP_A1",                        true},
{E_DAPC_MM_SLAVE,         34,  "IMGSYS_DISP_A_NBC",                     true},
{E_DAPC_MM_SLAVE,         35,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         36,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         37,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         38,  "IMGSYS_DPE",                            true},

 /* 190 */
{E_DAPC_MM_SLAVE,         39,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         40,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         41,  "IMGSYS_FDVT",                           true},
{E_DAPC_MM_SLAVE,         42,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         43,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         44,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         45,  "IMGSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         46,  "VDEC_GLOBAL_CON",                       true},
{E_DAPC_MM_SLAVE,         47,  "VDEC_CONFIG",                           true},
{E_DAPC_MM_SLAVE,         48,  "VDEC_FULL",                             true},

 /* 200 */
{E_DAPC_MM_SLAVE,         49,  "VDEC_Reserved",                         true},
{E_DAPC_MM_SLAVE,         50,  "VENC_GLOBAL_CON",                       true},
{E_DAPC_MM_SLAVE,         51,  "VENC_CONFG",                            true},
{E_DAPC_MM_SLAVE,         52,  "VENC_I0",                               true},
{E_DAPC_MM_SLAVE,         53,  "VENC_JPB_ENC_REG",                      true},
{E_DAPC_MM_SLAVE,         54,  "VENC_SRAM_POOL_MBIST_CTRL",             true},
{E_DAPC_MM_SLAVE,         55,  "VENC_SRAM_POOL_MBIST_CTRL",             true},
{E_DAPC_MM_SLAVE,         56,  "VENC_SRAM_POOL_MBIST_CTRL",             true},
{E_DAPC_MM_SLAVE,         57,  "VENC_SRAM_POOL_MBIST_CTRL",             true},
{E_DAPC_MM_SLAVE,         58,  "CAMSYS_CAMSYS_TOP",                     true},

 /* 210 */
{E_DAPC_MM_SLAVE,         59,  "CAMSYS_LARB3",                          true},
{E_DAPC_MM_SLAVE,         60,  "CAMSYS_CAM_TOP",                        true},
{E_DAPC_MM_SLAVE,         61,  "CAMSYS_CAM_A",                          true},
{E_DAPC_MM_SLAVE,         62,  "CAMSYS_CAM_B",                          true},
{E_DAPC_MM_SLAVE,         63,  "CAMSYS_CAM_TOP_SET",                    true},
{E_DAPC_MM_SLAVE,         64,  "CAMSYS_CAM_A_SET",                      true},
{E_DAPC_MM_SLAVE,         65,  "CAMSYS_CAM_B_SET",                      true},
{E_DAPC_MM_SLAVE,         66,  "CAMSYS_CAM_TOP_INNER",                  true},
{E_DAPC_MM_SLAVE,         67,  "CAMSYS_CAM_A_INNER",                    true},
{E_DAPC_MM_SLAVE,         68,  "CAMSYS_CAM_B_INNER",                    true},

 /* 220 */
{E_DAPC_MM_SLAVE,         69,  "CAMSYS_CAM_TOP_CLR",                    true},
{E_DAPC_MM_SLAVE,         70,  "CAMSYS_CAM_A_CLR",                      true},
{E_DAPC_MM_SLAVE,         71,  "CAMSYS_CAM_B_CLR",                      true},
{E_DAPC_MM_SLAVE,         72,  "CAMSYS_CAM_A_EXT",                      true},
{E_DAPC_MM_SLAVE,         73,  "CAMSYS_CAM_B_EXT",                      true},
{E_DAPC_MM_SLAVE,         74,  "CAMSYS_SENINF_A",                       true},
{E_DAPC_MM_SLAVE,         75,  "CAMSYS_SENINF_B",                       true},
{E_DAPC_MM_SLAVE,         76,  "CAMSYS_SENINF_C",                       true},
{E_DAPC_MM_SLAVE,         77,  "CAMSYS_SENINF_D",                       true},
{E_DAPC_MM_SLAVE,         78,  "CAMSYS_SENINF_E",                       true},

 /* 230 */
{E_DAPC_MM_SLAVE,         79,  "CAMSYS_SENINF_F",                       true},
{E_DAPC_MM_SLAVE,         80,  "CAMSYS_SENINF_G",                       true},
{E_DAPC_MM_SLAVE,         81,  "CAMSYS_SENINF_H",                       true},
{E_DAPC_MM_SLAVE,         82,  "CAMSYS_CAMSV_A",                        true},
{E_DAPC_MM_SLAVE,         83,  "CAMSYS_CAMSV_B",                        true},
{E_DAPC_MM_SLAVE,         84,  "CAMSYS_CAMSV_C",                        true},
{E_DAPC_MM_SLAVE,         85,  "CAMSYS_CAMSV_D",                        true},
{E_DAPC_MM_SLAVE,         86,  "CAMSYS_OTHERS_0",                       true},
{E_DAPC_MM_SLAVE,         87,  "CAMSYS_MD32 DMEM",                      true},
{E_DAPC_MM_SLAVE,         88,  "CAMSYS_RESERVED",                       true},

 /* 240 */
{E_DAPC_MM_SLAVE,         89,  "CAMSYS_MD32 PMEM",                      true},
{E_DAPC_MM_SLAVE,         90,  "CAMSYS_MD32 IP",                        true},
{E_DAPC_MM_SLAVE,         91,  "CAMSYS_CCU_CTL",                        true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_SBUS2APB_BRIDGE_UNALIGN",          true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_SBUS2APB_BRIDGE_OUT_OF_BOUND",     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_SBUS2APB_BRIDGE_WAY_EN",           true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_BRIDGE_UNALIGN",                true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_BRIDGE_OUT_OF_BOUND",           true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_BRIDGE_WAY_EN",                 true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB_UNALIGN",                  true},

 /* 250 */
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB _OUT_OF_BOUND",            true},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB _ERR_WAY_EN",              true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB_UNALIGN",                 true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB _OUT_OF_BOUND",           true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB _ERR_WAY_EN",             true},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST_PERIAPB_UNALIGN",                  true},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST _PERIAPB _OUT_OF_BOUND",           true},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST _PERIAPB _ERR_WAY_EN",             true},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH_SBUS2APB_BRIDGE_UNALIGN",         true},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH_SBUS2APB_BRIDGE_OUT_OF_BOUND",    true},

 /* 260 */
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH_SBUS2APB_BRIDGE_ERR_WAY_EN",      true},
{E_DAPC_OTHERS_SLAVE,     -1,  "INFRA_PDN_SBUS2APB_BRIDGE_DECERR",      true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI2_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI1_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI0_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAXI_SI0_DECERR",                    true},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAXI_SI1_DECERR",                    true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI3_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI4_DECERR",                     true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SRAMROM_SI0_DECERR",                    true},

 /* 270 */
{E_DAPC_OTHERS_SLAVE,     -1,  "SRAMROM*",                              true},
{E_DAPC_OTHERS_SLAVE,     -1,  "AP_DMA*",                               false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC_AO_INFRA_PERI*",             false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC_AO_MD*",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC_AO_MM*",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "CM_DQ_SECURE*",                         false},
{E_DAPC_OTHERS_SLAVE,     -1,  "MM_IOMMU_DOMAIN*",                      false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DISP_GCE*",                             false},
{E_DAPC_OTHERS_SLAVE,     -1,  "DEVICE_APC*",                           false},
{E_DAPC_OTHERS_SLAVE,     -1,  "EMI*",                                  false},

 /* 280 */
{E_DAPC_OTHERS_SLAVE,     -1,  "EMI_MPU*",                              false},
{E_DAPC_OTHERS_SLAVE,     -1,  "PMIC_WRAP*",                            false},
{E_DAPC_OTHERS_SLAVE,     -1,  "IMP_IIC_WRAP*",                         false},

};

static struct PERIAXI_ID_INFO paxi_int_mi_id_to_master[] = {
	{"MD",          { 1, 1, 2, 0 } },
	{"APDMA",       { 1, 0, 2, 0 } },
	{"MCUPM",       { 0, 0, 0, 0 } },
	{"SPM",         { 0, 0, 1, 0 } },
	{"CCU",         { 0, 0, 0, 1 } },
	{"THERM",       { 0, 0, 1, 1 } },
};

static struct TOPAXI_ID_INFO topaxi_mi0_id_to_master[] = {
	{"DebugTop",          { 1, 0, 0, 0,	0, 2, 0, 0,	0, 0, 0, 0 } },
	{"MSDC0",             { 1, 0, 1, 0,	0, 0, 0, 0,	2, 2, 0, 0 } },
	{"SPI0",              { 1, 0, 1, 0,	0, 1, 0, 0,	1, 1, 0, 0 } },
	{"Audio",             { 1, 0, 1, 0,	0, 1, 0, 0,	0, 1, 0, 0 } },
	{"MSDC1",             { 1, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 0 } },
	{"PWM",               { 1, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 0 } },
	{"USB20",             { 1, 0, 1, 0,	0, 0, 1, 0,	0, 0, 0, 0 } },
	{"SPI1",              { 1, 0, 1, 0,	0, 0, 1, 0,	1, 1, 0, 0 } },
	{"SPI2",              { 1, 0, 1, 0,	0, 0, 1, 0,	0, 1, 0, 0 } },
	{"N/A",               { 1, 0, 1, 0,	0, 0, 1, 0,	1, 0, 0, 0 } },
	{"THERM",             { 1, 0, 1, 0,	0, 1, 1, 0,	1, 1, 0, 0 } },
	{"CCU",               { 1, 0, 1, 0,	0, 1, 1, 0,	0, 1, 0, 0 } },
	{"SPM",               { 1, 0, 1, 0,	0, 1, 1, 0,	1, 0, 0, 0 } },
	{"MCUPM",             { 1, 0, 1, 0,	0, 1, 1, 0,	0, 0, 0, 0 } },
	{"DMA_EXT",           { 1, 0, 1, 0,	0, 0, 0, 1,	2, 0, 0, 0 } },
	{"SPI5",              { 1, 0, 1, 0,	0, 1, 0, 1,	1, 1, 0, 0 } },
	{"SPI4",              { 1, 0, 1, 0,	0, 1, 0, 1,	0, 1, 0, 0 } },
	{"SPI3",              { 1, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 0 } },
	{"N/A",               { 1, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 0 } },
	{"Connsys WiFi",      { 1, 0, 0, 1,	0, 0, 0, 2,	0, 0, 0, 0 } },
	{"Connsys Rbist",     { 1, 0, 0, 1,	0, 1, 0, 2,	0, 0, 0, 0 } },
	{"Connsys MCU R/W",   { 1, 0, 0, 1,	0, 0, 1, 2,	0, 0, 0, 0 } },
	{"Connsys N/A",       { 1, 0, 0, 1,	0, 1, 1, 2,	0, 0, 0, 0 } },
	{"DX_CC",             { 1, 0, 1, 1,	0, 2, 2, 2,	2, 0, 0, 0 } },
	{"CQ_DMA",            { 1, 0, 0, 0,	1, 2, 2, 2,	0, 0, 0, 0 } },
	{"CLDMA",             { 1, 0, 1, 0,	1, 2, 2, 0,	0, 0, 0, 0 } },
	{"GCE_M",             { 1, 0, 0, 1,	1, 2, 2, 0,	0, 0, 0, 0 } },
	{"SCP",               { 0, 1, 0, 2,	2, 0, 0, 0,	0, 0, 0, 0 } },
	{"SSPM",              { 0, 1, 1, 2,	2, 0, 0, 0,	0, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 2, 2, 0,	0, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 2, 2, 1,	0, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 0, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 1, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 2, 1, 0,	1, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 2, 2, 1,	1, 0, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 0, 0,	1, 2, 2, 2,	2, 1, 0, 0 } },
	{"APMCU_mp0_Write",   { 0, 0, 1, 1,	1, 2, 2, 2,	2, 2, 2, 2 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 2, 2, 0,	0, 0, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 2, 2, 1,	0, 0, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 0, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 1, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 2, 1, 0,	1, 0, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 2, 2, 1,	1, 0, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 0, 0, 2,	2, 1, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 1, 0, 2,	2, 1, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 2, 1, 2,	2, 1, 0, 0 } },
	{"APMCU_mp0_Read",    { 0, 0, 0, 0,	1, 2, 2, 2,	2, 2, 1, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 2, 2, 0,	0, 0, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 2, 2, 1,	0, 0, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 0, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 1, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 2, 1, 0,	1, 0, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 2, 2, 1,	1, 0, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	0, 2, 2, 2,	2, 1, 0, 0 } },
	{"APMCU_mp1_Write",   { 0, 0, 1, 1,	1, 2, 2, 2,	2, 2, 2, 2 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 2, 2, 0,	0, 0, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 2, 2, 1,	0, 0, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 0, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 1, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 2, 1, 0,	1, 0, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 2, 2, 1,	1, 0, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 0, 0, 2,	2, 1, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 1, 0, 2,	2, 1, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 2, 1, 2,	2, 1, 0, 0 } },
	{"APMCU_mp1_Read",    { 0, 0, 1, 1,	0, 2, 2, 2,	2, 2, 1, 0 } },
	{"APMCU_Write",       { 0, 0, 2, 2,	2, 2, 0, 0,	0, 0, 0, 0 } },
	{"APMCU_Write",       { 0, 0, 2, 2,	2, 2, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_Write",       { 0, 0, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 0, 0,	0, 0, 0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 0, 0,	1, 0, 0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 0,	0, 0, 0, 0,	0, 1, 0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 1,	0, 0, 0, 0,	0, 1, 0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 2, 2,	2, 2, 2, 1 } },
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
			DEVAPC_MSG("%s %s %s\n",
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

	if ((bus_id & 0x3) == 0x2) {
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
			DEVAPC_MSG("%s %s %s\n",
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

	DEVAPC_DBG_MSG("bus id = 0x%x, vio_addr = 0x%x\n",
		bus_id, vio_addr);

	/* SPM MTCMOS disable will set way_en[7:4] reg to block transaction,
	 * and it will triggered TOPAXI_SI0_DECERR instead of slave vio.
	 */
	if (vio_idx == TOPAXI_SI0_DECERR) {
		DEVAPC_DBG_MSG("vio is from TOPAXI_SI0_DECERR\n");
		master = topaxi_mi0_trans(bus_id);
		return master;

	} else if (vio_idx == PERIAXI_SI1_DECERR) {
		DEVAPC_DBG_MSG("vio is from PERIAXI_SI1_DECERR\n");
		master = paxi_int_mi_trans(bus_id);
		return master;

	} else if (vio_idx == SRAMROM_VIO_INDEX) {
		DEVAPC_DBG_MSG("vio is from SRAMROM\n");
		if ((bus_id & 0x1) == 0x0)
			master = topaxi_mi0_trans(bus_id >> 1);
		else
			DEVAPC_MSG("[FAILED] Cannot decode bus_id: 0x%x\n",
				bus_id);

		return master;

	}

	h_byte = (vio_addr >> 24) & 0xFF;

	/* to Infra/Peri/Audio/MD/CONN
	 * or MMSYS
	 * or MFG
	 */
	if (((h_byte >> 4) == 0x0) && h_byte != 0x0C && h_byte != 0x0D &&
		h_byte != 0x0E) {
		DEVAPC_DBG_MSG("vio addr is from on-chip SRAMROM\n");
		if ((bus_id & 0x1) == 0x0)
			master = topaxi_mi0_trans(bus_id >> 1);
		else
			DEVAPC_MSG("decode failed for sram_s\n");

	} else if (h_byte == 0x10 || h_byte == 0x11 || h_byte == 0x18 ||
		h_byte == 0x0C || h_byte == 0x0D || h_byte == 0x0E ||
		(h_byte >> 4) == 0x2 || (h_byte >> 4) == 0x3) {
		DEVAPC_DBG_MSG("vio addr is from Infra/Peri\n");

		master = paxi_int_mi_trans(bus_id);

	} else if (h_byte == 0x14 || h_byte == 0x15 || h_byte == 0x16 ||
		h_byte == 0x17 || h_byte == 0x1A) {
		DEVAPC_DBG_MSG("vio addr is from MM\n");
		if ((bus_id & 0x1) == 1)
			return "GCE";
		master = topaxi_mi0_trans(bus_id >> 1);

	} else if (h_byte == 0x13) {
		DEVAPC_DBG_MSG("vio addr is from MFG\n");
		master = topaxi_mi0_trans(bus_id);

	} else {
		DEVAPC_MSG("[FAILED] Cannot decode vio addr\n");
		master = "UNKNOWN_MASTER";
	}

	return master;
}

/* violation index corresponds to subsys */
const char *index_to_subsys(uint32_t index)
{
	if (index >= MFGSYS_START && index <= MFGSYS_END)
		return "MFGSYS";
	else if (index == SMI_COMMON || index == SMI_LARB0 ||
		index == IMGSYS_SMI_LARB2 || index == VDECSYS_SMI_LARB1)
		return "SMI";
	else if (index >= MMSYS_MDP_START && index <= MMSYS_MDP_END)
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
	else if (index < ARRAY_SIZE(mt6768_infra_devices))
		return mt6768_infra_devices[index].device;
	else
		return "OUT_OF_BOUND";
}

#ifdef CONFIG_DEVAPC_MMAP_DEBUG
void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s: %s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				__func__, "phys_addr", &phys_addr,
				"size", size);
		BUG_ON(1);
	}
}
EXPORT_SYMBOL(devapc_catch_illegal_range);
#endif

static ssize_t mt6768_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	return mtk_devapc_dbg_read(file, buffer, count, ppos);
}

static ssize_t mt6768_devapc_dbg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	return mtk_devapc_dbg_write(file, buffer, count, data);
}

static const struct file_operations devapc_dbg_fops = {
	.owner = THIS_MODULE,
	.write = mt6768_devapc_dbg_write,
	.read = mt6768_devapc_dbg_read,
};

static struct mtk_devapc_dbg_status mt6768_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static struct mtk_devapc_vio_info mt6768_devapc_vio_info = {
	.vio_cfg_max_idx = PLAT_VIO_CFG_MAX_IDX,
	.vio_max_idx = PLAT_VIO_MAX_IDX,
	.vio_mask_sta_num = PLAT_VIO_MASK_STA_NUM,
	.vio_shift_max_bit = PLAT_VIO_SHIFT_MAX_BIT,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
};

static const struct mtk_infra_vio_dbg_desc mt6768_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6768_sramrom_sec_vios = {
	.sramrom_sec_vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.sramrom_sec_vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.sramrom_sec_vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.sramrom_sec_vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.sramrom_sec_vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.sramrom_sec_vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const struct mtk_devapc_pd_desc mt6768_devapc_pds = {
	.pd_vio_mask_offset = PD_VIO_MASK_OFFSET,
	.pd_vio_sta_offset = PD_VIO_STA_OFFSET,
	.pd_vio_dbg0_offset = PD_VIO_DBG0_OFFSET,
	.pd_vio_dbg1_offset = PD_VIO_DBG1_OFFSET,
	.pd_apc_con_offset = PD_APC_CON_OFFSET,
	.pd_shift_sta_offset = PD_SHIFT_STA_OFFSET,
	.pd_shift_sel_offset = PD_SHIFT_SEL_OFFSET,
	.pd_shift_con_offset = PD_SHIFT_CON_OFFSET,
};

static struct mtk_devapc_soc mt6768_data = {
	.dbg_stat = &mt6768_devapc_dbg_stat,
	.device_info = mt6768_infra_devices,
	.ndevices = ARRAY_SIZE(mt6768_infra_devices),
	.vio_info = &mt6768_devapc_vio_info,
	.vio_dbgs = &mt6768_vio_dbgs,
	.sramrom_sec_vios = &mt6768_sramrom_sec_vios,
	.devapc_pds = &mt6768_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &bus_id_to_master,
};

static const struct of_device_id mt6768_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6768-devapc" },
	{},
};

static int mt6768_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6768_data);
}

static int mt6768_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6768_devapc_driver = {
	.probe = mt6768_devapc_probe,
	.remove = mt6768_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.of_match_table = mt6768_devapc_dt_match,
	},
};

/*
 * devapc_init: module init function.
 */
static int __init mt6768_devapc_init(void)
{
	int ret;

	DEVAPC_MSG("module initialized\n");

	ret = platform_driver_register(&mt6768_devapc_driver);
	if (ret) {
		pr_err("Unable to register driver, ret(%d)\n", ret);
		return ret;
	}

	proc_create("devapc_dbg", 0664, NULL, &devapc_dbg_fops);

	return 0;
}

arch_initcall(mt6768_devapc_init);

MODULE_DESCRIPTION("Mediatek MT6799 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
