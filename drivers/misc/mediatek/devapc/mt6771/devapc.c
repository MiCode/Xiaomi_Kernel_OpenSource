/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include <mt-plat/mtk_secure_api.h>

#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

/* CCF */
#include <linux/clk.h>

#include "mtk_io.h"
#include "sync_write.h"
#include "devapc.h"

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
#include <mt-plat/aee.h>
#endif

/* 0 for early porting */
#define DEVAPC_TURN_ON         1
#define DEVAPC_USE_CCF         1
#define DEVAPC_VIO_DEBUG       0

/* Debug message event */
#define DEVAPC_LOG_NONE        0x00000000
#define DEVAPC_LOG_INFO        0x00000001
#define DEVAPC_LOG_DBG         0x00000002

#define DEVAPC_LOG_LEVEL      (DEVAPC_LOG_DBG)

#define DEVAPC_DBG_MSG(fmt, args...) \
	do {    \
		if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_DBG) { \
			pr_debug(fmt, ##args); \
		} else if (DEVAPC_LOG_LEVEL & DEVAPC_LOG_INFO) { \
			pr_info(fmt, ##args); \
		} \
	} while (0)


#define DEVAPC_VIO_LEVEL      (DEVAPC_LOG_INFO)

#define DEVAPC_MSG(fmt, args...) \
	do {    \
		if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_DBG) { \
			pr_debug_ratelimited(fmt, ##args); \
		} else if (DEVAPC_VIO_LEVEL & DEVAPC_LOG_INFO) { \
			pr_info_ratelimited(fmt, ##args); \
		} \
	} while (0)



/* bypass clock! */
#if DEVAPC_USE_CCF
static struct clk *dapc_infra_clk;
#endif

static struct cdev *g_devapc_ctrl;
static unsigned int devapc_infra_irq;
static void __iomem *devapc_pd_infra_base;
static void __iomem *devapc_ao_infra_base;

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
unsigned int devapc_vio_current_aee_trigger_times;
#endif

#if DEVAPC_TURN_ON
static struct DEVICE_INFO devapc_infra_devices[] = {
/* slave type,       config_idx, device name                enable_vio_irq */

 /* 0 */
{E_DAPC_INFRA_PERI_SLAVE, 0,   "INFRA_AO_TOPCKGEN",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 1,   "INFRA_AO_INFRASYS_CONFIG_REGS",         true},
{E_DAPC_INFRA_PERI_SLAVE, 2,   "io_*_cfg",                              true},
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
{E_DAPC_INFRA_PERI_SLAVE, 21,  "INFRA_AO_DEVICE_MPU",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 22,  "INFRA_AO_AES_TOP_0",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 23,  "INFRA_AO_SYS_TIMER",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 24,  "INFRA_AO_MDEM_TEMP_SHARE",              true},
{E_DAPC_INFRA_PERI_SLAVE, 25,  "INFRA_AO_DEVICE_APC_AO_MD",             true},
{E_DAPC_INFRA_PERI_SLAVE, 26,  "INFRA_AO_SECURITY_AO",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 27,  "INFRA_AO_TOPCKGEN_REG",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 28,  "INFRA_AO_DEVICE_APC_AO_MM",             true},
{E_DAPC_INFRA_PERI_SLAVE, 29,  "INFRASYS_RESERVE",                      true},

 /* 30 */
{E_DAPC_INFRA_PERI_SLAVE, 30,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 31,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 32,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 33,  "INFRASYS_SYS_CIRQ",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 34,  "INFRASYS_MM_IOMMU",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 35,  "INFRASYS_EFUSE_PDN_DEBUG",              true},
{E_DAPC_INFRA_PERI_SLAVE, 36,  "INFRASYS_DEVICE_APC",                   true},
{E_DAPC_INFRA_PERI_SLAVE, 37,  "INFRASYS_DBG_TRACKER",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 38,  "INFRASYS_CCIF0_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 39,  "INFRASYS_CCIF0_MD",                     true},

 /* 40 */
{E_DAPC_INFRA_PERI_SLAVE, 40,  "INFRASYS_CCIF1_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 41,  "INFRASYS_CCIF1_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 42,  "INFRASYS_MBIST",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 43,  "INFRASYS_INFRA_PDN_REGISTER",           true},
{E_DAPC_INFRA_PERI_SLAVE, 44,  "INFRASYS_TRNG",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 45,  "INFRASYS_DX_CC",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 46,  "md_ccif_md1",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 47,  "INFRASYS_CQ_DMA",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 48,  "md_ccif_md2",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 49,  "INFRASYS_SRAMROM",                      true},

 /* 50 */
{E_DAPC_INFRA_PERI_SLAVE, 50,  "ANA_MIPI_DSI0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 51,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 52,  "ANA_MIPI_CSI0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 53,  "ANA_MIPI_CSI1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 54,  "INFRASYS_EMI",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 55,  "INFRASYS_RESERVE",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 56,  "INFRASYS_CLDMA_PDN",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 57,  "cldma_pdn_md_misc",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 58,  "infra_md",                              true},
{E_DAPC_INFRA_PERI_SLAVE, 59,  "BPI_BSI_SLV0",                          true},

 /* 60 */
{E_DAPC_INFRA_PERI_SLAVE, 60,  "BPI_BSI_SLV1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 61,  "BPI_BSI_SLV2",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 62,  "INFRASYS_EMI_MPU",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 63,  "INFRASYS_DVFS_PROC",                    true},
{E_DAPC_INFRA_PERI_SLAVE, 64,  "INFRASYS_DRAMC_CH0_TOP0",               true},
{E_DAPC_INFRA_PERI_SLAVE, 65,  "INFRASYS_DRAMC_CH0_TOP1",               true},
{E_DAPC_INFRA_PERI_SLAVE, 66,  "INFRASYS_DRAMC_CH0_TOP2",               true},
{E_DAPC_INFRA_PERI_SLAVE, 67,  "INFRASYS_DRAMC_CH0_TOP3",               true},
{E_DAPC_INFRA_PERI_SLAVE, 68,  "INFRASYS_DRAMC_CH0_TOP4",               true},
{E_DAPC_INFRA_PERI_SLAVE, 69,  "INFRASYS_DRAMC_CH1_TOP0",               true},

 /* 70 */
{E_DAPC_INFRA_PERI_SLAVE, 70,  "INFRASYS_DRAMC_CH1_TOP1",               true},
{E_DAPC_INFRA_PERI_SLAVE, 71,  "INFRASYS_DRAMC_CH1_TOP2",               true},
{E_DAPC_INFRA_PERI_SLAVE, 72,  "INFRASYS_DRAMC_CH1_TOP3",               true},
{E_DAPC_INFRA_PERI_SLAVE, 73,  "INFRASYS_DRAMC_CH1_TOP4",               true},
{E_DAPC_INFRA_PERI_SLAVE, 74,  "INFRASYS_GCE",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 75,  "INFRASYS_CCIF2_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 76,  "INFRASYS_CCIF2_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 77,  "INFRASYS_CCIF3_AP",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 78,  "INFRASYS_CCIF3_MD",                     true},
{E_DAPC_INFRA_PERI_SLAVE, 79,  "INFRA_AO_SSPM Partition 1",             true},

 /* 80 */
{E_DAPC_INFRA_PERI_SLAVE, 80,  "INFRA_AO_SSPM Partition 2",             true},
{E_DAPC_INFRA_PERI_SLAVE, 81,  "INFRA_AO_SSPM Partition 3",             true},
{E_DAPC_INFRA_PERI_SLAVE, 82,  "INFRA_AO_SSPM Partition 4",             true},
{E_DAPC_INFRA_PERI_SLAVE, 83,  "INFRA_AO_SSPM Partition 5",             true},
{E_DAPC_INFRA_PERI_SLAVE, 84,  "INFRA_AO_SSPM Partition 6",             true},
{E_DAPC_INFRA_PERI_SLAVE, 85,  "INFRA_AO_SSPM Partition 7",             true},
{E_DAPC_INFRA_PERI_SLAVE, 86,  "INFRA_AO_SSPM Partition 8",             true},
{E_DAPC_INFRA_PERI_SLAVE, 87,  "INFRA_AO_SCP",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 88,  "INFRA_AO_MCUCFG",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 89,  "INFRASYS_DBUGSYS",                      true},

 /* 90 */
{E_DAPC_INFRA_PERI_SLAVE, 90,  "PERISYS_APDMA",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 91,  "PERISYS_AUXADC",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 92,  "PERISYS_UART0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 93,  "PERISYS_UART1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 94,  "PERISYS_UART2",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 95,  "PERISYS_I2C6",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 96,  "PERISYS_PWM",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 97,  "PERISYS_I2C0",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 98,  "PERISYS_I2C1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 99,  "PERISYS_I2C2",                          true},

 /* 100 */
{E_DAPC_INFRA_PERI_SLAVE, 100, "PERISYS_SPI0",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 101, "PERISYS_PTP",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 102, "PERISYS_BTIF",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 103, "RESERVE",                               true},
{E_DAPC_INFRA_PERI_SLAVE, 104, "PERISYS_DISP_PWM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 105, "PERISYS_I2C3",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 106, "PERISYS_SPI1",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 107, "PERISYS_I2C4",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 108, "PERISYS_SPI2",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 109, "PERISYS_SPI3",                          true},

 /* 110 */
{E_DAPC_INFRA_PERI_SLAVE, 110, "PERISYS_I2C1_IMM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 111, "PERISYS_I2C2_IMM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 112, "PERISYS_I2C5",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 113, "PERISYS_I2C5_IMM",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 114, "PERISYS_SPI4",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 115, "PERISYS_SPI5",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 116, "PERISYS_I2C7",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 117, "PERISYS_I2C8",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 118, "PERISYS_USB",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 119, "PERISYS_USB_2.0_SUB",                   true},

 /* 120 */
{E_DAPC_INFRA_PERI_SLAVE, 120, "PERISYS_AUDIO",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 121, "PERISYS_MSDC0",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 122, "PERISYS_MSDC1",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 123, "PERISYS_MSDC2",                         true},
{E_DAPC_INFRA_PERI_SLAVE, 124, "RESERVE",                               true},
{E_DAPC_INFRA_PERI_SLAVE, 125, "PERISYS_UFS",                           true},
{E_DAPC_INFRA_PERI_SLAVE, 126, "RESERVE",                               true},
{E_DAPC_INFRA_PERI_SLAVE, 127, "RESERVE",                               true},
{E_DAPC_INFRA_PERI_SLAVE, 128, "PERISYS_RESERVE",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 129, "EAST_RESERVE_0",                        true},

 /* 130 */
{E_DAPC_INFRA_PERI_SLAVE, 130, "EAST_ RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 131, "EAST_ RESERVE_2",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 132, "EAST_ RESERVE_3",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 133, "EAST_ RESERVE_4",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 134, "EAST_IO_CFG_RT",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 135, "EAST_ RESERVE_6",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 136, "EAST_ RESERVE_7",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 137, "EAST_CSI0_TOP_AO",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 138, "RESERVE",                               true},
{E_DAPC_INFRA_PERI_SLAVE, 139, "EAST_ RESERVE_A",                       true},

 /* 140 */
{E_DAPC_INFRA_PERI_SLAVE, 140, "EAST_ RESERVE_B",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 141, "EAST_RESERVE_C",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 142, "EAST_RESERVE_D",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 143, "EAST_RESERVE_E",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 144, "EAST_RESERVE_F",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 145, "SOUTH_RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 146, "SOUTH_RESERVE_1",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 147, "SOUTH_IO_CFG_RM",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 148, "SOUTH_IO_CFG_RB",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 149, "SOUTH_EFUSE",                           true},

 /* 150 */
{E_DAPC_INFRA_PERI_SLAVE, 150, "SOUTH_ RESERVE_5",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 151, "SOUTH_ RESERVE_6",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 152, "SOUTH_ RESERVE_7",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 153, "SOUTH_ RESERVE_8",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 154, "SOUTH_ RESERVE_9",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 155, "SOUTH_ RESERVE_A",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 156, "SOUTH_ RESERVE_B",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 157, "SOUTH_RESERVE_C",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 158, "SOUTH_RESERVE_D",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 159, "SOUTH_RESERVE_E",                       true},

 /* 160 */
{E_DAPC_INFRA_PERI_SLAVE, 160, "SOUTH_RESERVE_F",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 161, "WEST_ RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 162, "WEST_ msdc1_pad_macro",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 163, "WEST_ RESERVE_2",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 164, "WEST_ RESERVE_3",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 165, "WEST_ RESERVE_4",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 166, "WEST_ MIPI_TX_CONFIG",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 167, "WEST_ RESERVE_6",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 168, "WEST_ IO_CFG_LB",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 169, "WEST_ IO_CFG_LM",                       true},

 /* 170 */
{E_DAPC_INFRA_PERI_SLAVE, 170, "WEST_ IO_CFG_BL",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 171, "WEST_ RESERVE_A",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 172, "WEST_ RESERVE_B",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 173, "WEST_ RESERVE_C",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 174, "WEST_ RESERVE_D",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 175, "WEST_RESERVE_E",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 176, "WEST_RESERVE_F",                        true},
{E_DAPC_INFRA_PERI_SLAVE, 177, "NORTH_RESERVE_0",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 178, "efuse_top",                             true},
{E_DAPC_INFRA_PERI_SLAVE, 179, "NORTH_IO_CFG_LT",                       true},

 /* 180 */
{E_DAPC_INFRA_PERI_SLAVE, 180, "NORTH_IO_CFG_TL",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 181, "NORTH_USB20 PHY",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 182, "NORTH_msdc0 pad macro",                 true},
{E_DAPC_INFRA_PERI_SLAVE, 183, "NORTH_ RESERVE_6",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 184, "NORTH_ RESERVE_7",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 185, "NORTH_ RESERVE_8",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 186, "NORTH_ RESERVE_9",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 187, "NORTH_ UFS_MPHY",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 188, "NORTH_ RESERVE_B",                      true},
{E_DAPC_INFRA_PERI_SLAVE, 189, "NORTH_RESERVE_C",                       true},

 /* 190 */
{E_DAPC_INFRA_PERI_SLAVE, 190, "NORTH_RESERVE_D",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 191, "NORTH_RESERVE_E",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 192, "NORTH_RESERVE_F",                       true},
{E_DAPC_INFRA_PERI_SLAVE, 193, "PERISYS_CONN",                          true},
{E_DAPC_INFRA_PERI_SLAVE, 194, "PERISYS_MD_VIOLATION",                  true},
{E_DAPC_INFRA_PERI_SLAVE, 195, "PERISYS_RESERVE",                       true},
{E_DAPC_MM_SLAVE,         0,   "G3D_CONFIG",                            true},
{E_DAPC_MM_SLAVE,         1,   "MFG VAD",                               true},
{E_DAPC_MM_SLAVE,         2,   "SC0 VAD",                               true},
{E_DAPC_MM_SLAVE,         3,   "MFG_OTHERS",                            true},

 /* 200 */
{E_DAPC_MM_SLAVE,         4,   "MMSYS_CONFIG",                          true},
{E_DAPC_MM_SLAVE,         5,   "MDP_RDMA0",                             true},
{E_DAPC_MM_SLAVE,         6,   "MDP_RDMA1",                             true},
{E_DAPC_MM_SLAVE,         7,   "MDP_RSZ0",                              true},
{E_DAPC_MM_SLAVE,         8,   "MDP_RSZ1",                              true},
{E_DAPC_MM_SLAVE,         9,   "MDP_WROT0",                             true},
{E_DAPC_MM_SLAVE,         10,  "MDP_WDMA",                              true},
{E_DAPC_MM_SLAVE,         11,  "MDP_TDSHP",                             true},
{E_DAPC_MM_SLAVE,         12,  "DISP_OVL0",                             true},
{E_DAPC_MM_SLAVE,         13,  "DISP_OVL0_2L",                          true},

 /* 210 */
{E_DAPC_MM_SLAVE,         14,  "DISP_OVL1_2L",                          true},
{E_DAPC_MM_SLAVE,         15,  "DISP_RDMA0",                            true},
{E_DAPC_MM_SLAVE,         16,  "DISP_RDMA1",                            true},
{E_DAPC_MM_SLAVE,         17,  "DISP_WDMA0",                            true},
{E_DAPC_MM_SLAVE,         18,  "DISP_COLOR0",                           true},
{E_DAPC_MM_SLAVE,         19,  "DISP_CCORR0",                           true},
{E_DAPC_MM_SLAVE,         20,  "DISP_AAL0",                             true},
{E_DAPC_MM_SLAVE,         21,  "DISP_GAMMA0",                           true},
{E_DAPC_MM_SLAVE,         22,  "DISP_DITHER0",                          true},
{E_DAPC_MM_SLAVE,         23,  "DSI_SPLIT",                             true},

 /* 220 */
{E_DAPC_MM_SLAVE,         24,  "DSI0",                                  true},
{E_DAPC_MM_SLAVE,         25,  "DPI",                                   true},
{E_DAPC_MM_SLAVE,         26,  "MM_MUTEX",                              true},
{E_DAPC_MM_SLAVE,         27,  "SMI_LARB0",                             true},
{E_DAPC_MM_SLAVE,         28,  "SMI_LARB1",                             true},
{E_DAPC_MM_SLAVE,         29,  "SMI_COMMON",                            true},
{E_DAPC_MM_SLAVE,         30,  "DISP_RSZ",                              true},
{E_DAPC_MM_SLAVE,         31,  "MDP_AAL",                               true},
{E_DAPC_MM_SLAVE,         32,  "MDP_CCORR",                             true},
{E_DAPC_MM_SLAVE,         33,  "DBI",                                   true},

 /* 230 */
{E_DAPC_MM_SLAVE,         34,  "MMSYS_OTHERS",                          true},
{E_DAPC_MM_SLAVE,         35,  "IMGSYS_CONFIG",                         true},
{E_DAPC_MM_SLAVE,         36,  "IMGSYS_SMI_LARB1",                      true},
{E_DAPC_MM_SLAVE,         37,  "IMGSYS_DISP_A0",                        true},
{E_DAPC_MM_SLAVE,         38,  "IMGSYS_DISP_A1",                        true},
{E_DAPC_MM_SLAVE,         39,  "IMGSYS_DISP_A2",                        true},
{E_DAPC_MM_SLAVE,         40,  "IMGSYS_DISP_A3",                        true},
{E_DAPC_MM_SLAVE,         41,  "IMGSYS_DISP_A4",                        true},
{E_DAPC_MM_SLAVE,         42,  "IMGSYS_DISP_A5",                        true},
{E_DAPC_MM_SLAVE,         43,  "IMGSYS_DPE",                            true},

 /* 240 */
{E_DAPC_MM_SLAVE,         44,  "IMGSYS_RSC",                            true},
{E_DAPC_MM_SLAVE,         45,  "IMGSYS_WPEA",                           true},
{E_DAPC_MM_SLAVE,         46,  "IMGSYS_WPEB",                           true},
{E_DAPC_MM_SLAVE,         47,  "IMGSYS_FDVT",                           true},
{E_DAPC_MM_SLAVE,         48,  "IMGSYS_OWE",                            true},
{E_DAPC_MM_SLAVE,         49,  "IMGSYS_MFB",                            true},
{E_DAPC_MM_SLAVE,         50,  "IMGSYS_SMI_LARB2",                      true},
{E_DAPC_MM_SLAVE,         51,  "IMGSYS_OTHERS",                         true},
{E_DAPC_MM_SLAVE,         58,  "VDECSYS_GLOBAL_CON",                    true},
{E_DAPC_MM_SLAVE,         59,  "VDECSYS_SMI_LARB1",                     true},

 /* 250 */
{E_DAPC_MM_SLAVE,         60,  "VDECSYS_FULL_TOP",                      true},
{E_DAPC_MM_SLAVE,         61,  "VDECSYS_OTHERS",                        true},
{E_DAPC_MM_SLAVE,         52,  "VENCSYS_GLOBAL_CON",                    true},
{E_DAPC_MM_SLAVE,         53,  "VENCSYSSYS_SMI_LARB4",                  true},
{E_DAPC_MM_SLAVE,         54,  "VENCSYS_VENC",                          true},
{E_DAPC_MM_SLAVE,         55,  "VENCSYS_JPGENC",                        true},
{E_DAPC_MM_SLAVE,         56,  "VENCSYS_MBIST_CTRL",                    true},
{E_DAPC_MM_SLAVE,         57,  "VENCSYS_OTHERS",                        true},
{E_DAPC_MM_SLAVE,         62,  "CAMSYS_CAMSYS_TOP",                     true},
{E_DAPC_MM_SLAVE,         63,  "CAMSYS_LARB6",                          true},

 /* 260 */
{E_DAPC_MM_SLAVE,         64,  "CAMSYS_LARB3",                          true},
{E_DAPC_MM_SLAVE,         65,  "CAMSYS_CAM_TOP",                        true},
{E_DAPC_MM_SLAVE,         66,  "CAMSYS_CAM_A",                          true},
{E_DAPC_MM_SLAVE,         67,  "CAMSYS_CAM_A",                          true},
{E_DAPC_MM_SLAVE,         68,  "CAMSYS_CAM_B",                          true},
{E_DAPC_MM_SLAVE,         69,  "CAMSYS_CAM_B",                          true},
{E_DAPC_MM_SLAVE,         70,  "CAMSYS_CAM_C",                          true},
{E_DAPC_MM_SLAVE,         71,  "CAMSYS_CAM_C",                          true},
{E_DAPC_MM_SLAVE,         72,  "CAMSYS_CAM_TOP_SET",                    true},
{E_DAPC_MM_SLAVE,         73,  "CAMSYS_CAM_A_SET",                      true},

 /* 270 */
{E_DAPC_MM_SLAVE,         74,  "CAMSYS_CAM_A_SET",                      true},
{E_DAPC_MM_SLAVE,         75,  "CAMSYS_CAM_B_SET",                      true},
{E_DAPC_MM_SLAVE,         76,  "CAMSYS_CAM_B_SET",                      true},
{E_DAPC_MM_SLAVE,         77,  "CAMSYS_CAM_C_SET",                      true},
{E_DAPC_MM_SLAVE,         78,  "CAMSYS_CAM_C_SET",                      true},
{E_DAPC_MM_SLAVE,         79,  "CAMSYS_CAM_TOP_INNER",                  true},
{E_DAPC_MM_SLAVE,         80,  "CAMSYS_CAM_A_INNER",                    true},
{E_DAPC_MM_SLAVE,         81,  "CAMSYS_CAM_A_INNER",                    true},
{E_DAPC_MM_SLAVE,         82,  "CAMSYS_CAM_B_INNER",                    true},
{E_DAPC_MM_SLAVE,         83,  "CAMSYS_CAM_B_INNER",                    true},

 /* 280 */
{E_DAPC_MM_SLAVE,         84,  "CAMSYS_CAM_C_INNER",                    true},
{E_DAPC_MM_SLAVE,         85,  "CAMSYS_CAM_C_INNER",                    true},
{E_DAPC_MM_SLAVE,         86,  "CAMSYS_CAM_A_EXT",                      true},
{E_DAPC_MM_SLAVE,         87,  "CAMSYS_CAM_B_EXT",                      true},
{E_DAPC_MM_SLAVE,         88,  "CAMSYS_CAM_C_EXT",                      true},
{E_DAPC_MM_SLAVE,         89,  "CAMSYS_CAM_TOP_CLR",                    true},
{E_DAPC_MM_SLAVE,         90,  "CAMSYS_CAM_A_CLR",                      true},
{E_DAPC_MM_SLAVE,         91,  "CAMSYS_CAM_A_CLR",                      true},
{E_DAPC_MM_SLAVE,         92,  "CAMSYS_CAM_B_CLR",                      true},
{E_DAPC_MM_SLAVE,         93,  "CAMSYS_CAM_B_CLR",                      true},

 /* 290 */
{E_DAPC_MM_SLAVE,         94,  "CAMSYS_CAM_C_CLR",                      true},
{E_DAPC_MM_SLAVE,         95,  "CAMSYS_CAM_C_CLR",                      true},
{E_DAPC_MM_SLAVE,         96,  "CAMSYS_CAM_A_EXT",                      true},
{E_DAPC_MM_SLAVE,         97,  "CAMSYS_CAM_B_EXT",                      true},
{E_DAPC_MM_SLAVE,         98,  "CAMSYS_CAM_C_EXT",                      true},
{E_DAPC_MM_SLAVE,         99,  "CAMSYS_CAM_RESERVE",                    true},
{E_DAPC_MM_SLAVE,         100, "CAMSYS_SENINF_A",                       true},
{E_DAPC_MM_SLAVE,         101, "CAMSYS_SENINF_B",                       true},
{E_DAPC_MM_SLAVE,         102, "CAMSYS_SENINF_C",                       true},
{E_DAPC_MM_SLAVE,         103, "CAMSYS_SENINF_D",                       true},

 /* 300 */
{E_DAPC_MM_SLAVE,         104, "CAMSYS_SENINF_E",                       true},
{E_DAPC_MM_SLAVE,         105, "CAMSYS_SENINF_F",                       true},
{E_DAPC_MM_SLAVE,         106, "CAMSYS_SENINF_G",                       true},
{E_DAPC_MM_SLAVE,         107, "CAMSYS_SENINF_H",                       true},
{E_DAPC_MM_SLAVE,         108, "CAMSYS_CAMSV_A",                        true},
{E_DAPC_MM_SLAVE,         109, "CAMSYS_CAMSV_B",                        true},
{E_DAPC_MM_SLAVE,         110, "CAMSYS_CAMSV_C",                        true},
{E_DAPC_MM_SLAVE,         111, "CAMSYS_CAMSV_D",                        true},
{E_DAPC_MM_SLAVE,         112, "CAMSYS_MD32 DMEM_12",                   true},
{E_DAPC_MM_SLAVE,         113, "CAMSYS_RESEVE",                         true},

 /* 310 */
{E_DAPC_MM_SLAVE,         114, "CAMSYS_CCU_CTL",                        true},
{E_DAPC_MM_SLAVE,         115, "CAMSYS_CCU_H2T_A",                      true},
{E_DAPC_MM_SLAVE,         116, "CAMSYS_CCU_T2H_A",                      true},
{E_DAPC_MM_SLAVE,         117, "CAMSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         118, "CAMSYS_RESERVE",                        true},
{E_DAPC_MM_SLAVE,         119, "CAMSYS_CCU_DMA",                        true},
{E_DAPC_MM_SLAVE,         120, "CAMSYS_TSF",                            true},
{E_DAPC_MM_SLAVE,         121, "CAMSYS_MD32_PMEM_24",                   true},
{E_DAPC_MM_SLAVE,         122, "CAMSYS_OTHERS",                         true},
{E_DAPC_MM_SLAVE,         123, "VPUSYS_CFG",                            true},

 /* 320 */
{E_DAPC_MM_SLAVE,         124, "VPUSYS_ADL_CTRL",                       true},
{E_DAPC_MM_SLAVE,         125, "VPUSYS_COREA_DMEM_0_128KB",             true},
{E_DAPC_MM_SLAVE,         126, "VPUSYS_COREA_DMEM_128_256KB",           true},
{E_DAPC_MM_SLAVE,         127, "VPUSYS_COREA_IMEM_256KB",               true},
{E_DAPC_MM_SLAVE,         128, "VPUSYS_COREA_CONTROL",                  true},
{E_DAPC_MM_SLAVE,         129, "VPUSYS_COREA_DEBUG",                    true},
{E_DAPC_MM_SLAVE,         130, "VPUSYS_COREB_DMEM_0_128KB",             true},
{E_DAPC_MM_SLAVE,         131, "VPUSYS_COREB_DMEM_128_256KB",           true},
{E_DAPC_MM_SLAVE,         132, "VPUSYS_COREB_IMEM_256KB",               true},
{E_DAPC_MM_SLAVE,         133, "VPUSYS_COREB_CONTROL",                  true},

 /* 330 */
{E_DAPC_MM_SLAVE,         134, "VPUSYS_COREB_DEBUG",                    true},
{E_DAPC_MM_SLAVE,         135, "VPUSYS_COREC_DMEM_0_128KB",             true},
{E_DAPC_MM_SLAVE,         136, "VPUSYS_COREC_DMEM_128_256KB",           true},
{E_DAPC_MM_SLAVE,         137, "VPUSYS_COREC_IMEM_256KB",               true},
{E_DAPC_MM_SLAVE,         138, "VPUSYS_COREC_CONTROL",                  true},
{E_DAPC_MM_SLAVE,         139, "VPUSYS_COREC_DEBUG",                    true},
{E_DAPC_MM_SLAVE,         140, "VPUSYS_OTHERS",                         true},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_UNALIGN",                         false},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_OUT_OF_BOUND",                    false},
{E_DAPC_OTHERS_SLAVE,     -1,  "SSPM_ERR_WAY_EN",                      false},

 /* 340 */
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB_UNALIGN",                  false},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB _OUT_OF_BOUND",            false},
{E_DAPC_OTHERS_SLAVE,     -1,  "EAST_PERIAPB _ERR_WAY_EN",              false},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB_UNALIGN",                 false},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB _OUT_OF_BOUND",           false},
{E_DAPC_OTHERS_SLAVE,     -1,  "SOUTH_PERIAPB _ERR_WAY_EN",             false},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST_PERIAPB_UNALIGN",                  false},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST _PERIAPB _OUT_OF_BOUND",           false},
{E_DAPC_OTHERS_SLAVE,     -1,  "WEST _PERIAPB _ERR_WAY_EN",             false},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH_PERIAPB_UNALIGN",                 false},

 /* 350 */
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH _PERIAPB _OUT_OF_BOUND",          false},
{E_DAPC_OTHERS_SLAVE,     -1,  "NORTH _PERIAPB _ERR_WAY_EN",            false},
{E_DAPC_OTHERS_SLAVE,     -1,  "INFRA_PDN_DECODE_ERROR",                false},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_UNALIGN",                       false},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_OUT_OF_BOUND",                  false},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAPB_ERR_WAY_EN",                    false},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI2_DECERR",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI1_DECERR",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI0_DECERR",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAXI_SI0_DECERR",                    false},

 /* 360 */
{E_DAPC_OTHERS_SLAVE,     -1,  "PERIAXI_SI1_DECERR",                    false},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI3_DECERR",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "TOPAXI_SI4_DECERR",                     false},
{E_DAPC_OTHERS_SLAVE,     -1,  "SRAMROM_DECERR",                        false},
};
#endif

/*
 * The extern functions for EMI MPU are removed because EMI MPU and Device APC
 * do not share the same IRQ now.
 */

/**************************************************************************
 *STATIC FUNCTION
 **************************************************************************/

#ifdef CONFIG_MTK_HIBERNATION
static int devapc_pm_restore_noirq(struct device *device)
{
	if (devapc_infra_irq != 0) {
		mt_irq_set_sens(devapc_infra_irq, MT_LEVEL_SENSITIVE);
		mt_irq_set_polarity(devapc_infra_irq, MT_POLARITY_LOW);
	}

	return 0;
}
#endif

#if DEVAPC_TURN_ON
static void unmask_infra_module_irq(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	if (module > PD_INFRA_VIO_MASK_MAX_INDEX) {
		pr_err("[DEVAPC] %s: module overflow!\n", __func__);
		return;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	*DEVAPC_PD_INFRA_VIO_MASK(apc_index) &=
		(0xFFFFFFFF ^ (1 << apc_bit_index));
}

static void mask_infra_module_irq(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	if (module > PD_INFRA_VIO_MASK_MAX_INDEX) {
		pr_err("[DEVAPC] %s: module overflow!\n", __func__);
		return;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	*DEVAPC_PD_INFRA_VIO_MASK(apc_index) |= (1 << apc_bit_index);
}

static int clear_infra_vio_status(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	if (module > PD_INFRA_VIO_STA_MAX_INDEX) {
		pr_err("[DEVAPC] %s: module overflow!\n", __func__);
		return -1;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	*DEVAPC_PD_INFRA_VIO_STA(apc_index) = (0x1 << apc_bit_index);

	return 0;
}

static int check_infra_vio_status(unsigned int module)
{
	unsigned int apc_index = 0;
	unsigned int apc_bit_index = 0;

	if (module > PD_INFRA_VIO_STA_MAX_INDEX) {
		pr_err("[DEVAPC] %s: module overflow!\n", __func__);
		return -1;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	if (*DEVAPC_PD_INFRA_VIO_STA(apc_index) & (0x1 << apc_bit_index))
		return 1;

	return 0;
}

static void print_vio_mask_sta(void)
{
	DEVAPC_DBG_MSG("%s INFRA VIO_MASK 0:0x%x 1:0x%x 2:0x%x 3:0x%x\n",
			"[DEVAPC]",
			readl(DEVAPC_PD_INFRA_VIO_MASK(0)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(1)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(2)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(3)));
	DEVAPC_DBG_MSG("%s INFRA VIO_MASK 4:0x%x 5:0x%x 6:0x%x 7:0x%x\n",
			"[DEVAPC]",
			readl(DEVAPC_PD_INFRA_VIO_MASK(4)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(5)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(6)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(7)));
	DEVAPC_DBG_MSG("%s INFRA VIO_MASK 8:0x%x 9:0x%x\n",
			"[DEVAPC]",
			readl(DEVAPC_PD_INFRA_VIO_MASK(8)),
			readl(DEVAPC_PD_INFRA_VIO_MASK(9)));

	DEVAPC_DBG_MSG("%s INFRA VIO_STA 0:0x%x 1:0x%x 2:0x%x 3:0x%x\n",
			"[DEVAPC]",
			readl(DEVAPC_PD_INFRA_VIO_STA(0)),
			readl(DEVAPC_PD_INFRA_VIO_STA(1)),
			readl(DEVAPC_PD_INFRA_VIO_STA(2)),
			readl(DEVAPC_PD_INFRA_VIO_STA(3)));
	DEVAPC_DBG_MSG("%s INFRA VIO_STA 4:0x%x 5:0x%x 6:0x%x 7:0x%x\n",
			"[DEVAPC]",
			readl(DEVAPC_PD_INFRA_VIO_STA(4)),
			readl(DEVAPC_PD_INFRA_VIO_STA(5)),
			readl(DEVAPC_PD_INFRA_VIO_STA(6)),
			readl(DEVAPC_PD_INFRA_VIO_STA(7)));
	DEVAPC_DBG_MSG("%s INFRA VIO_STA 8:0x%x 9:0x%x\n",
			"[DEVAPC]",
			readl(DEVAPC_PD_INFRA_VIO_STA(8)),
			readl(DEVAPC_PD_INFRA_VIO_STA(9)));

}

static void start_devapc(void)
{
	unsigned int i;

	mt_reg_sync_writel(0x80000000, DEVAPC_PD_INFRA_APC_CON);


	print_vio_mask_sta();
	DEVAPC_MSG("[DEVAPC] %s\n",
		"Clear INFRA VIO_STA and unmask INFRA VIO_MASK...");

	for (i = 0; i < ARRAY_SIZE(devapc_infra_devices); i++)
		if (true == devapc_infra_devices[i].enable_vio_irq) {
			clear_infra_vio_status(i);
			unmask_infra_module_irq(i);
		}

	print_vio_mask_sta();

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
	devapc_vio_current_aee_trigger_times = 0;
#endif

}

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
static void execute_aee(unsigned int i, unsigned int dbg0, unsigned int dbg1)
{
	char subsys_str[48] = {0};
	unsigned int domain_id;

	DEVAPC_MSG("[DEVAPC] Executing AEE Exception...\n");

	/* mask irq for module "i" */
	mask_infra_module_irq(i);

	if (devapc_vio_current_aee_trigger_times <
		DEVAPC_VIO_MAX_TOTAL_MODULE_AEE_TRIGGER_TIMES) {

		devapc_vio_current_aee_trigger_times++;
		domain_id = (dbg0 & INFRA_VIO_DBG_DMNID) >>
			INFRA_VIO_DBG_DMNID_START_BIT;
		if (domain_id == 1) {
			strncpy(subsys_str, "MD_SI", sizeof(subsys_str));
		} else {
			strncpy(subsys_str, devapc_infra_devices[i].device,
				sizeof(subsys_str));
		}
		subsys_str[sizeof(subsys_str)-1] = '\0';

		aee_kernel_exception("DEVAPC",
			"%s %s, Vio Addr: 0x%x\n%s%s\n",
			"[DEVAPC] Violation Slave:",
			devapc_infra_devices[i].device,
			dbg1,
			"CRDISPATCH_KEY:Device APC Violation Issue/",
			subsys_str
			);
	}

	/* unmask irq for module "i" */
	unmask_infra_module_irq(i);
}
#endif

static char *perm_to_string(uint32_t perm)
{
	if (perm == 0x0)
		return "NO_PROTECTION";
	else if (perm == 0x1)
		return "SECURE_RW_ONLY";
	else if (perm == 0x2)
		return "SECURE_RW_NS_R_ONLY";
	else if (perm == 0x3)
		return "FORBIDDEN";
	else
		return "UNKNOWN_PERM";
}

static unsigned int sync_vio_dbg(int shift_bit)
{
	unsigned int shift_count = 0;
	unsigned int sync_done;

	mt_reg_sync_writel(0x1 << shift_bit, DEVAPC_PD_INFRA_VIO_SHIFT_SEL);
	mt_reg_sync_writel(0x1,	DEVAPC_PD_INFRA_VIO_SHIFT_CON);

	for (shift_count = 0; (shift_count < 100) &&
		((readl(DEVAPC_PD_INFRA_VIO_SHIFT_CON) & 0x3) != 0x3);
		++shift_count)
		DEVAPC_DBG_MSG("[DEVAPC] Syncing INFRA DBG0 & DBG1 (%d, %d)\n",
				shift_bit, shift_count);

	DEVAPC_DBG_MSG("[DEVAPC] VIO_SHIFT_SEL=0x%X, VIO_SHIFT_CON=0x%X\n",
			readl(DEVAPC_PD_INFRA_VIO_SHIFT_SEL),
			readl(DEVAPC_PD_INFRA_VIO_SHIFT_CON));

	if ((readl(DEVAPC_PD_INFRA_VIO_SHIFT_CON) & 0x3) == 0x3)
		sync_done = 1;
	else {
		sync_done = 0;
		DEVAPC_MSG("[DEVAPC] sync failed, shift_bit: %d\n",
				shift_bit);
	}

	/* disable shift mechanism */
	mt_reg_sync_writel(0x0, DEVAPC_PD_INFRA_VIO_SHIFT_CON);
	mt_reg_sync_writel(0x0, DEVAPC_PD_INFRA_VIO_SHIFT_SEL);
	mt_reg_sync_writel(0x1 << shift_bit, DEVAPC_PD_INFRA_VIO_SHIFT_STA);

	return sync_done;
}

static uint32_t get_permission(int vio_index, int domain)
{
#ifndef MTK_SIP_KERNEL_DAPC_DUMP
	DEVAPC_MSG("Not supported get_permission yet\n");
	return 0xdead;
#else
	int slave_type;
	int config_idx;
	int apc_set_idx;
	uint32_t ret;

	slave_type = devapc_infra_devices[vio_index].DEVAPC_SLAVE_TYPE;
	config_idx = devapc_infra_devices[vio_index].config_index;

	DEVAPC_DBG_MSG("%s, slave type = 0x%x, config_idx = 0x%x\n",
			__func__,
			slave_type,
			config_idx);

	ret = mt_secure_call(MTK_SIP_KERNEL_DAPC_DUMP, slave_type,
			domain, config_idx, 0);

	DEVAPC_DBG_MSG("%s, dump perm = 0x%x\n", __func__, ret);

	apc_set_idx = config_idx % MOD_NO_IN_1_DEVAPC;
	ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

	DEVAPC_DBG_MSG("%s, after shipping, dump perm = 0x%x\n",
			__func__,
			(ret & 0x3));

	return (ret & 0x3);
#endif
}

static irqreturn_t devapc_violation_irq(int irq_number, void *dev_id)
{
	unsigned int dbg0 = 0, dbg1 = 0;
	unsigned int master_id;
	unsigned int domain_id = 0;
	unsigned int vio_addr_high;
	unsigned int read_violation;
	unsigned int write_violation;
	unsigned int device_count;
	unsigned int i;
	uint32_t perm;

	if (irq_number != devapc_infra_irq) {
		DEVAPC_MSG("[DEVAPC] (ERROR) irq_number %d %s\n",
				irq_number,
				"is not registered!");

		return IRQ_NONE;
	}
	print_vio_mask_sta();

	DEVAPC_DBG_MSG("[DEVAPC] VIO_SHIFT_STA: 0x%x\n",
		readl(DEVAPC_PD_INFRA_VIO_SHIFT_STA));

	for (i = 0; i <= PD_INFRA_VIO_SHIFT_MAX_BIT; ++i) {
		if (readl(DEVAPC_PD_INFRA_VIO_SHIFT_STA) & (0x1 << i)) {

			if (sync_vio_dbg(i) == 0)
				continue;

			DEVAPC_DBG_MSG("[DEVAPC] %s%X, %s%X, %s%X\n",
					"VIO_SHIFT_STA=0x",
					readl(DEVAPC_PD_INFRA_VIO_SHIFT_STA),
					"VIO_SHIFT_SEL=0x",
					readl(DEVAPC_PD_INFRA_VIO_SHIFT_SEL),
					"VIO_SHIFT_CON=0x",
					readl(DEVAPC_PD_INFRA_VIO_SHIFT_CON));

			dbg0 = readl(DEVAPC_PD_INFRA_VIO_DBG0);
			dbg1 = readl(DEVAPC_PD_INFRA_VIO_DBG1);

			master_id = (dbg0 & INFRA_VIO_DBG_MSTID)
				>> INFRA_VIO_DBG_MSTID_START_BIT;
			domain_id = (dbg0 & INFRA_VIO_DBG_DMNID)
				>> INFRA_VIO_DBG_DMNID_START_BIT;
			write_violation = (dbg0 & INFRA_VIO_DBG_W_VIO)
				>> INFRA_VIO_DBG_W_VIO_START_BIT;
			read_violation = (dbg0 & INFRA_VIO_DBG_R_VIO)
				>> INFRA_VIO_DBG_R_VIO_START_BIT;
			vio_addr_high = (dbg0 & INFRA_VIO_ADDR_HIGH)
				>> INFRA_VIO_ADDR_HIGH_START_BIT;

			/* violation information */
			DEVAPC_MSG("%s%s%s%s%x %s%x, %s%x, %s%x\n",
					"[DEVAPC] Violation(",
					read_violation == 1?" R":"",
					write_violation == 1?" W ) - ":" ) - ",
					"Vio Addr:0x", dbg1,
					"High:0x", vio_addr_high,
					"Bus ID:0x", master_id,
					"Dom ID:0x", domain_id);

			DEVAPC_MSG("%s - %s%s, %s%i\n",
					"[DEVAPC] Violation",
					"Process:", current->comm,
					"PID:", current->pid);

			break;
		}
	}

	device_count = ARRAY_SIZE(devapc_infra_devices);

	/* checking and showing violation normal slaves */
	for (i = 0; i < device_count; i++) {
		if (devapc_infra_devices[i].enable_vio_irq == true
			&& check_infra_vio_status(i) == 1) {
			clear_infra_vio_status(i);
			perm = get_permission(i, domain_id);
			DEVAPC_MSG("%s %s %s (%s=%d)\n",
				"[DEVAPC]",
				"Access Violation Slave:",
				devapc_infra_devices[i].device,
				"infra index",
				i);
			DEVAPC_MSG("%s %s %s\n",
				"[DEVAPC]",
				"permission:",
				perm_to_string(perm));

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(DEVAPC_ENABLE_AEE)
			execute_aee(i, dbg0, dbg1);
#endif
		}
	}

	return IRQ_HANDLED;
}
#endif

static int devapc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
#if DEVAPC_TURN_ON
	int ret;
#endif

	DEVAPC_MSG("[DEVAPC] module probe.\n");

	if (devapc_pd_infra_base == NULL) {
		if (node) {
			devapc_pd_infra_base = of_iomap(node,
				DAPC_DEVICE_TREE_NODE_PD_INFRA_INDEX);
			devapc_ao_infra_base = of_iomap(node,
				DAPC_DEVICE_TREE_NODE_AO_INFRA_INDEX);
			devapc_infra_irq = irq_of_parse_and_map(node,
				DAPC_DEVICE_TREE_NODE_PD_INFRA_INDEX);
			DEVAPC_MSG("[DEVAPC] PD_INFRA_ADDRESS: %p, IRQ: %d\n",
				devapc_pd_infra_base, devapc_infra_irq);
		} else {
			pr_err("[DEVAPC] %s\n",
				"can't find DAPC_INFRA_PD compatible node");
			return -1;
		}
	}

#if DEVAPC_TURN_ON
	ret = request_irq(devapc_infra_irq, (irq_handler_t)devapc_violation_irq,
			IRQF_TRIGGER_LOW | IRQF_SHARED,
			"devapc", &g_devapc_ctrl);
	if (ret) {
		pr_err("[DEVAPC] Failed to request infra irq! (%d)\n", ret);
		return ret;
	}
#endif

	/* CCF */
#if DEVAPC_USE_CCF
	dapc_infra_clk = devm_clk_get(&pdev->dev, "devapc-infra-clock");
	if (IS_ERR(dapc_infra_clk)) {
		pr_err("[DEVAPC] (Infra) %s\n",
			"Cannot get devapc clock from common clock framework.");
		return PTR_ERR(dapc_infra_clk);
	}
	clk_prepare_enable(dapc_infra_clk);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_DEVAPC,
		devapc_pm_restore_noirq, NULL);
#endif

#if DEVAPC_TURN_ON
	start_devapc();
#endif

	return 0;
}

static int devapc_remove(struct platform_device *dev)
{
	clk_disable_unprepare(dapc_infra_clk);
	return 0;
}

static int devapc_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int devapc_resume(struct platform_device *dev)
{
	DEVAPC_MSG("[DEVAPC] module resume.\n");
	return 0;
}

#ifdef DBG_ENABLE
static ssize_t devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	ssize_t retval = 0;
	char msg[256] = "DBG: test violation...\n";

	if (*ppos >= strlen(msg))
		return 0;

	DEVAPC_MSG("[DEVAPC] %s, test violation...\n", __func__);

	retval = simple_read_from_buffer(buffer, count, ppos, msg, strlen(msg));

	DEVAPC_MSG("[DEVAPC] %s, devapc_ao_infra_base = 0x%x\n",
			__func__,
			readl((unsigned int *)(devapc_ao_infra_base + 0x0)));
	DEVAPC_MSG("[DEVAPC] %s, test done, it should generate violation!\n",
			__func__);

	return retval;
}

static ssize_t devapc_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char input[32];
	char *pinput = NULL;
	char *tmp = NULL;
	long i;
	int len = 0;
#ifdef MTK_SIP_KERNEL_DAPC_DUMP
	int apc_set_idx;
	uint32_t ret;
#endif
	long slave_type = 0, domain = 0, index = 0;

	DEVAPC_MSG("[DEVAPC] debugging...\n");
	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_info("[DEVAPC] copy from user failed!\n");
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	tmp = strsep(&pinput, " ");
	if (tmp != NULL)
		i = kstrtol(tmp, 10, &slave_type);
	else
		slave_type = E_DAPC_OTHERS_SLAVE;

	if (slave_type >= E_DAPC_OTHERS_SLAVE) {
		pr_info("[DEVAPC] wrong input slave type\n");
		return -EFAULT;
	}
	DEVAPC_MSG("[DEVAPC] slave_type = %lu\n", slave_type);

	tmp = strsep(&pinput, " ");
	if (tmp != NULL)
		i = kstrtol(tmp, 10, &domain);
	else
		domain = E_DOMAIN_OTHERS;

	if (domain >= E_DOMAIN_OTHERS) {
		pr_info("[DEVAPC] wrong input domain type\n");
		return -EFAULT;
	}
	DEVAPC_MSG("[DEVAPC] domain id = %lu\n", domain);

	tmp = strsep(&pinput, " ");
	if (tmp != NULL)
		i = kstrtol(tmp, 10, &index);
	else
		index = 0xFFFFFFFF;

	if (index > DEVAPC_TOTAL_SLAVES) {
		pr_info("[DEVAPC] wrong input index type\n");
		return -EFAULT;
	}
	DEVAPC_MSG("[DEVAPC] slave config_idx = %lu\n", index);

#ifndef MTK_SIP_KERNEL_DAPC_DUMP
	DEVAPC_MSG("Not supported get_permission yet\n");
#else
	ret = mt_secure_call(MTK_SIP_KERNEL_DAPC_DUMP, slave_type,
			domain, index, 0);

	DEVAPC_MSG("[DEVAPC] dump perm = 0x%x\n", ret);

	apc_set_idx = index % MOD_NO_IN_1_DEVAPC;
	ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

	DEVAPC_MSG("%s the permission is %s\n",
			"[DEVAPC]",
			perm_to_string((ret & 0x3)));
#endif
	return count;
}
#endif

static int devapc_dbg_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations devapc_dbg_fops = {
	.owner = THIS_MODULE,
	.open  = devapc_dbg_open,
#ifdef DBG_ENABLE
	.write = devapc_dbg_write,
	.read = devapc_dbg_read,
#else
	.write = NULL,
	.read = NULL,
#endif
};

static const struct of_device_id plat_devapc_dt_match[] = {
	{ .compatible = "mediatek,devapc" },
	{},
};

static struct platform_driver devapc_driver = {
	.probe = devapc_probe,
	.remove = devapc_remove,
	.suspend = devapc_suspend,
	.resume = devapc_resume,
	.driver = {
		.name = "devapc",
		.owner = THIS_MODULE,
		.of_match_table	= plat_devapc_dt_match,
	},
};

/*
 * devapc_init: module init function.
 */
static int __init devapc_init(void)
{
	int ret;

	DEVAPC_MSG("[DEVAPC] kernel module init.\n");

	ret = platform_driver_register(&devapc_driver);
	if (ret) {
		pr_err("[DEVAPC] Unable to register driver (%d)\n", ret);
		return ret;
	}

	g_devapc_ctrl = cdev_alloc();
	if (!g_devapc_ctrl) {
		pr_err("[DEVAPC] Failed to add devapc device! (%d)\n", ret);
		platform_driver_unregister(&devapc_driver);
		return ret;
	}
	g_devapc_ctrl->owner = THIS_MODULE;

	proc_create("devapc_dbg", 0664, NULL, &devapc_dbg_fops);
	/* 0664: (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) */

	return 0;
}

/*
 * devapc_exit: module exit function.
 */
static void __exit devapc_exit(void)
{
	DEVAPC_MSG("[DEVAPC] DEVAPC module exit\n");
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_DEVAPC);
#endif
}

arch_initcall(devapc_init);
module_exit(devapc_exit);
MODULE_LICENSE("GPL");
