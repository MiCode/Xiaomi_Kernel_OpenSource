/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>

#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include <mach/rpm-smd.h>
#include <mach/clock-generic.h>

#include "clock-local2.h"
#include "clock-pll.h"
#include "clock-rpm.h"
#include "clock-voter.h"
#include "clock.h"
#include "clock-mdss-8974.h"
#include "clock-krait.h"

enum {
	GCC_BASE,
	MMSS_BASE,
	LPASS_BASE,
	APCS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define MMSS_REG_BASE(x) (void __iomem *)(virt_bases[MMSS_BASE] + (x))
#define LPASS_REG_BASE(x) (void __iomem *)(virt_bases[LPASS_BASE] + (x))
#define APCS_REG_BASE(x) (void __iomem *)(virt_bases[APCS_BASE] + (x))

#define GPLL0_MODE                       0x0000
#define GPLL0_L                          0x0004
#define GPLL0_M                          0x0008
#define GPLL0_N                          0x000C
#define GPLL0_USER_CTL                   0x0010
#define GPLL0_CONFIG_CTL                 0x0014
#define GPLL0_TEST_CTL                   0x0018
#define GPLL0_STATUS                     0x001C
#define GPLL1_MODE                       0x0040
#define GPLL1_L                          0x0044
#define GPLL1_M                          0x0048
#define GPLL1_N                          0x004C
#define GPLL1_USER_CTL                   0x0050
#define GPLL1_CONFIG_CTL                 0x0054
#define GPLL1_TEST_CTL                   0x0058
#define GPLL1_STATUS                     0x005C
#define GPLL2_MODE                       0x0080
#define GPLL2_L                          0x0084
#define GPLL2_M                          0x0088
#define GPLL2_N                          0x008C
#define GPLL2_USER_CTL                   0x0090
#define GPLL2_CONFIG_CTL                 0x0094
#define GPLL2_TEST_CTL                   0x0098
#define GPLL2_STATUS                     0x009C
#define SYS_NOC_USB3_AXI_CBCR            0x0108
#define PERIPH_NOC_USB_HSIC_AHB_CBCR     0x01A4
#define SYS_NOC_USB3_SEC_AXI_CBCR        0x0138
#define SYS_NOC_UFS_AXI_CBCR             0x013C
#define OCMEM_NOC_CFG_AHB_CBCR           0x0248
#define MMSS_NOC_CFG_AHB_CBCR            0x024C
#define MMSS_VPU_MAPLE_SYS_NOC_AXI_CBCR  0x026C
#define USB_30_BCR                       0x03C0
#define USB30_MASTER_CBCR                0x03C8
#define USB30_SLEEP_CBCR                 0x03CC
#define USB30_MOCK_UTMI_CBCR             0x03D0
#define USB30_MASTER_CMD_RCGR            0x03D4
#define USB_HSIC_AHB_CMD_RCGR            0x046C
#define USB30_MOCK_UTMI_CMD_RCGR         0x03E8
#define USB3_PHY_BCR                     0x03FC
#define USB_HS_HSIC_BCR                  0x0400
#define USB_HSIC_AHB_CBCR                0x0408
#define USB_HSIC_SYSTEM_CMD_RCGR         0x041C
#define USB_HSIC_SYSTEM_CBCR             0x040C
#define USB_HSIC_CMD_RCGR                0x0440
#define USB_HSIC_CBCR                    0x0410
#define USB_HSIC_IO_CAL_CMD_RCGR         0x0458
#define USB_HSIC_IO_CAL_CBCR             0x0414
#define USB_HSIC_IO_CAL_SLEEP_CBCR       0x0418
#define USB_HS_BCR                       0x0480
#define USB_HS_SYSTEM_CBCR               0x0484
#define USB_HS_AHB_CBCR                  0x0488
#define USB_HS_INACTIVITY_TIMERS_CBCR    0x048C
#define USB_HS_SYSTEM_CMD_RCGR           0x0490
#define USB2A_PHY_BCR                    0x04A8
#define USB2A_PHY_SLEEP_CBCR             0x04AC
#define USB2B_PHY_BCR                    0x04B0
#define USB2B_PHY_SLEEP_CBCR             0x04B4
#define SDCC1_BCR                        0x04C0
#define SDCC1_APPS_CMD_RCGR              0x04D0
#define SDCC1_APPS_CBCR                  0x04C4
#define SDCC1_AHB_CBCR                   0x04C8
#define SDCC1_CDCCAL_SLEEP_CBCR          0x04E4
#define SDCC1_CDCCAL_FF_CBCR             0x04E8
#define SDCC2_BCR                        0x0500
#define SDCC2_APPS_CMD_RCGR              0x0510
#define SDCC2_APPS_CBCR                  0x0504
#define SDCC2_AHB_CBCR                   0x0508
#define SDCC2_INACTIVITY_TIMERS_CBCR     0x050C
#define SDCC3_BCR                        0x0540
#define SDCC3_APPS_CMD_RCGR              0x0550
#define SDCC3_APPS_CBCR                  0x0544
#define SDCC3_AHB_CBCR                   0x0548
#define SDCC3_INACTIVITY_TIMERS_CBCR     0x054C
#define SDCC4_BCR                        0x0580
#define SDCC4_APPS_CMD_RCGR              0x0590
#define SDCC4_APPS_CBCR                  0x0584
#define SDCC4_AHB_CBCR                   0x0588
#define SDCC4_INACTIVITY_TIMERS_CBCR     0x058C
#define BLSP1_BCR                        0x05C0
#define BLSP1_AHB_CBCR                   0x05C4
#define BLSP1_QUP1_BCR                   0x0640
#define BLSP1_QUP1_SPI_APPS_CBCR         0x0644
#define BLSP1_QUP1_I2C_APPS_CBCR         0x0648
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR     0x0660
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR     0x06E0
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR     0x0760
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR     0x07E0
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR     0x0860
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR     0x08E0
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR     0x09A0
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR     0x0A20
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR     0x0AA0
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR     0x0B20
#define BLSP2_QUP5_I2C_APPS_CMD_RCGR     0x0BA0
#define BLSP2_QUP6_I2C_APPS_CMD_RCGR     0x0C20
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR     0x064C
#define BLSP1_UART1_BCR                  0x0680
#define BLSP1_UART1_APPS_CBCR            0x0684
#define BLSP1_UART1_APPS_CMD_RCGR        0x068C
#define BLSP1_QUP2_BCR                   0x06C0
#define BLSP1_QUP2_SPI_APPS_CBCR         0x06C4
#define BLSP1_QUP2_I2C_APPS_CBCR         0x06C8
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR     0x06CC
#define BLSP1_UART2_BCR                  0x0700
#define BLSP1_UART2_APPS_CBCR            0x0704
#define BLSP1_UART2_APPS_CMD_RCGR        0x070C
#define BLSP1_QUP3_BCR                   0x0740
#define BLSP1_QUP3_SPI_APPS_CBCR         0x0744
#define BLSP1_QUP3_I2C_APPS_CBCR         0x0748
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR     0x074C
#define BLSP1_UART3_BCR                  0x0780
#define BLSP1_UART3_APPS_CBCR            0x0784
#define BLSP1_UART3_APPS_CMD_RCGR        0x078C
#define BLSP1_QUP4_BCR                   0x07C0
#define BLSP1_QUP4_SPI_APPS_CBCR         0x07C4
#define BLSP1_QUP4_I2C_APPS_CBCR         0x07C8
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR     0x07CC
#define BLSP1_UART4_BCR                  0x0800
#define BLSP1_UART4_APPS_CBCR            0x0804
#define BLSP1_UART4_APPS_CMD_RCGR        0x080C
#define BLSP1_QUP5_BCR                   0x0840
#define BLSP1_QUP5_SPI_APPS_CBCR         0x0844
#define BLSP1_QUP5_I2C_APPS_CBCR         0x0848
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR     0x084C
#define BLSP1_UART5_BCR                  0x0880
#define BLSP1_UART5_APPS_CBCR            0x0884
#define BLSP1_UART5_APPS_CMD_RCGR        0x088C
#define BLSP1_UART5_APPS_CFG_RCGR        0x0890
#define BLSP1_QUP6_BCR                   0x08C0
#define BLSP1_QUP6_SPI_APPS_CBCR         0x08C4
#define BLSP1_QUP6_I2C_APPS_CBCR         0x08C8
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR     0x08CC
#define BLSP1_UART6_BCR                  0x0900
#define BLSP1_UART6_APPS_CBCR            0x0904
#define BLSP1_UART6_APPS_CMD_RCGR        0x090C
#define BLSP2_BCR                        0x0940
#define BLSP2_AHB_CBCR                   0x0944
#define BLSP2_QUP1_BCR                   0x0980
#define BLSP2_QUP1_SPI_APPS_CBCR         0x0984
#define BLSP2_QUP1_I2C_APPS_CBCR         0x0988
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR     0x098C
#define BLSP2_UART1_BCR                  0x09C0
#define BLSP2_UART1_APPS_CBCR            0x09C4
#define BLSP2_UART1_APPS_CMD_RCGR        0x09CC
#define BLSP2_QUP2_BCR                   0x0A00
#define BLSP2_QUP2_SPI_APPS_CBCR         0x0A04
#define BLSP2_QUP2_I2C_APPS_CBCR         0x0A08
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR     0x0A0C
#define BLSP2_UART2_BCR                  0x0A40
#define BLSP2_UART2_APPS_CBCR            0x0A44
#define BLSP2_UART2_APPS_CMD_RCGR        0x0A4C
#define BLSP2_QUP3_BCR                   0x0A80
#define BLSP2_QUP3_SPI_APPS_CBCR         0x0A84
#define BLSP2_QUP3_I2C_APPS_CBCR         0x0A88
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR     0x0A8C
#define BLSP2_UART3_BCR                  0x0AC0
#define BLSP2_UART3_APPS_CBCR            0x0AC4
#define BLSP2_UART3_APPS_CMD_RCGR        0x0ACC
#define BLSP2_QUP4_BCR                   0x0B00
#define BLSP2_QUP4_SPI_APPS_CBCR         0x0B04
#define BLSP2_QUP4_I2C_APPS_CBCR         0x0B08
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR     0x0B0C
#define BLSP2_UART4_BCR                  0x0B40
#define BLSP2_UART4_APPS_CBCR            0x0B44
#define BLSP2_UART4_APPS_CMD_RCGR        0x0B4C
#define BLSP2_QUP5_BCR                   0x0B80
#define BLSP2_QUP5_SPI_APPS_CBCR         0x0B84
#define BLSP2_QUP5_I2C_APPS_CBCR         0x0B88
#define BLSP2_QUP5_SPI_APPS_CMD_RCGR     0x0B8C
#define BLSP2_UART5_BCR                  0x0BC0
#define BLSP2_UART5_APPS_CBCR            0x0BC4
#define BLSP2_UART5_APPS_CMD_RCGR        0x0BCC
#define BLSP2_QUP6_BCR                   0x0C00
#define BLSP2_QUP6_SPI_APPS_CBCR         0x0C04
#define BLSP2_QUP6_I2C_APPS_CBCR         0x0C08
#define BLSP2_QUP6_SPI_APPS_CMD_RCGR     0x0C0C
#define BLSP2_UART6_BCR                  0x0C40
#define BLSP2_UART6_APPS_CBCR            0x0C44
#define BLSP2_UART6_APPS_CMD_RCGR        0x0C4C
#define PDM_BCR                          0x0CC0
#define PDM_AHB_CBCR                     0x0CC4
#define PDM2_CBCR                        0x0CCC
#define PDM2_CMD_RCGR                    0x0CD0
#define PRNG_BCR                         0x0D00
#define PRNG_AHB_CBCR                    0x0D04
#define BAM_DMA_BCR                      0x0D40
#define BAM_DMA_AHB_CBCR                 0x0D44
#define BAM_DMA_INACTIVITY_TIMERS_CBCR   0x0D48
#define TSIF_BCR                         0x0D80
#define TSIF_AHB_CBCR                    0x0D84
#define TSIF_REF_CBCR                    0x0D88
#define TSIF_INACTIVITY_TIMERS_CBCR      0x0D8C
#define TSIF_REF_CMD_RCGR                0x0D90
#define BOOT_ROM_AHB_CBCR                0x0E04
#define CE1_BCR                          0x1040
#define CE1_CMD_RCGR                     0x1050
#define CE1_CBCR                         0x1044
#define CE1_AXI_CBCR                     0x1048
#define CE1_AHB_CBCR                     0x104C
#define CE2_BCR                          0x1080
#define CE2_CMD_RCGR                     0x1090
#define CE2_CBCR                         0x1084
#define CE2_AXI_CBCR                     0x1088
#define CE2_AHB_CBCR                     0x108C
#define GCC_XO_DIV4_CBCR                 0x10C8
#define LPASS_Q6_AXI_CBCR                0x11C0
#define LPASS_MPORT_AXI_CBCR             0x11C4
#define LPASS_SWAY_CBCR                  0x11C8
#define APCS_GPLL_ENA_VOTE               0x1480
#define APCS_CLOCK_BRANCH_ENA_VOTE       0x1484
#define APCS_CLOCK_SLEEP_ENA_VOTE        0x1488
#define GCC_DEBUG_CLK_CTL                0x1880
#define CLOCK_FRQ_MEASURE_CTL            0x1884
#define CLOCK_FRQ_MEASURE_STATUS         0x1888
#define GCC_PLLTEST_PAD_CFG              0x188C
#define GCC_GP1_CBCR                     0x1900
#define GCC_GP1_CMD_RCGR                 0x1904
#define GCC_GP2_CBCR                     0x1940
#define GCC_GP2_CMD_RCGR                 0x1944
#define GCC_GP3_CBCR                     0x1980
#define GCC_GP3_CMD_RCGR                 0x1984
#define GPLL4_MODE                       0x1DC0
#define GPLL4_L                          0x1DC4
#define GPLL4_M                          0x1DC8
#define GPLL4_N                          0x1DCC
#define GPLL4_USER_CTL                   0x1DD0
#define GPLL4_CONFIG_CTL                 0x1DD4
#define GPLL4_TEST_CTL                   0x1DD8
#define GPLL4_STATUS                     0x1DDC
#define COPSS_SMMU_BCR                   0x1A40
#define COPSS_SMMU_AXI_CBCR              0x1A44
#define COPSS_SMMU_AHB_CBCR              0x1A48
#define SPSS_BCR                         0x1A80
#define SPSS_AHB_CBCR                    0x1A84
#define SATA_BCR                         0x1C40
#define SATA_RX_OOB_CMD_RCGR             0x1C5C
#define SATA_PMALIVE_CMD_RCGR            0x1C80
#define SATA_AXI_CBCR                    0x1C44
#define SATA_CFG_AHB_CBCR                0x1C48
#define SATA_RX_OOB_CBCR                 0x1C4C
#define SATA_PMALIVE_CBCR                0x1C50
#define SATA_ASIC0_CMD_RCGR              0x1C94
#define SATA_ASIC0_CFG_RCGR              0x1C98
#define SATA_ASIC0_CBCR                  0x1C54
#define SATA_RX_CMD_RCGR                 0x1CA8
#define SATA_RX_CBCR                     0x1C58
#define USB_30_SEC_BCR                   0x1BC0
#define USB_30_SEC_MISC                  0x1BC4
#define USB30_SEC_MASTER_CBCR            0x1BC8
#define USB30_SEC_SLEEP_CBCR             0x1BCC
#define USB30_SEC_MOCK_UTMI_CBCR         0x1BD0
#define USB30_SEC_MASTER_CMD_RCGR        0x1BD4
#define USB30_SEC_MASTER_M               0x1BDC
#define USB30_SEC_MASTER_N               0x1BE0
#define USB30_SEC_MASTER_D               0x1BE4
#define USB30_SEC_MOCK_UTMI_CMD_RCGR     0x1BE8
#define USB3_SEC_PHY_BCR                 0x1BFC
#define PCIE_0_BCR                       0x1AC0
#define PCIE_0_PHY_BCR                   0x1B00
#define PCIE_0_CFG_AHB_CBCR              0x1B0C
#define PCIE_0_PIPE_CBCR                 0x1B14
#define PCIE_0_SLV_AXI_CBCR              0x1B04
#define PCIE_0_AUX_CBCR                  0x1B10
#define PCIE_0_MSTR_AXI_CBCR             0x1B08
#define PCIE_0_PIPE_CMD_RCGR             0x1B18
#define PCIE_0_AUX_CMD_RCGR              0x1B2C
#define PCIE_1_BCR                       0x1B40
#define PCIE_1_PHY_BCR                   0x1B80
#define PCIE_1_CFG_AHB_CBCR              0x1B8C
#define PCIE_1_PIPE_CBCR                 0x1B94
#define PCIE_1_SLV_AXI_CBCR              0x1B84
#define PCIE_1_AUX_CBCR                  0x1B90
#define PCIE_1_MSTR_AXI_CBCR             0x1B88
#define PCIE_1_PIPE_CMD_RCGR             0x1B98
#define PCIE_1_AUX_CMD_RCGR              0x1BAC
#define CE3_BCR                          0x1D00
#define CE3_CMD_RCGR                     0x1D10
#define CE3_CBCR                         0x1D04
#define CE3_AXI_CBCR                     0x1D08
#define CE3_AHB_CBCR                     0x1D0C
#define UFS_BCR                          0x1D40
#define UFS_AXI_CBCR                     0x1D44
#define UFS_TX_CFG_CBCR                  0x1D4C
#define UFS_AHB_CBCR                     0x1D48
#define UFS_RX_CFG_CBCR			 0x1D50
#define UFS_TX_SYMBOL_0_CBCR             0x1D54
#define UFS_TX_SYMBOL_1_CBCR             0x1D58
#define UFS_RX_SYMBOL_0_CBCR             0x1D5C
#define UFS_RX_SYMBOL_1_CBCR             0x1D60
#define UFS_AXI_CMD_RCGR                 0x1D64
#define PCIE_0_PHY_LDO_EN                0x1E00
#define PCIE_1_PHY_LDO_EN                0x1E04
#define SATA_PHY_LDO_EN                  0x1E08
#define USB30_PHY_COM_BCR                0x1E80
#define USB_HSIC_MOCK_UTMI_CMD_RCGR      0x1F00
#define USB_HSIC_MOCK_UTMI_CBCR          0x1F14

#define GLB_CLK_DIAG	                0x001C
#define L2_CBCR				0x004C

#define MMPLL0_PLL_MODE                 0x0000
#define MMPLL0_PLL_L_VAL                0x0004
#define MMPLL0_PLL_M_VAL                0x0008
#define MMPLL0_PLL_N_VAL                0x000C
#define MMPLL0_PLL_USER_CTL             0x0010
#define MMPLL0_PLL_CONFIG_CTL           0x0014
#define MMPLL0_PLL_TEST_CTL             0x0018
#define MMPLL0_PLL_STATUS               0x001C
#define MMPLL1_PLL_MODE                 0x0040
#define MMPLL1_PLL_L_VAL                0x0044
#define MMPLL1_PLL_M_VAL                0x0048
#define MMPLL1_PLL_N_VAL                0x004C
#define MMPLL1_PLL_USER_CTL             0x0050
#define MMPLL1_PLL_CONFIG_CTL           0x0054
#define MMPLL1_PLL_TEST_CTL             0x0058
#define MMPLL1_PLL_STATUS               0x005C
#define MMPLL3_PLL_MODE                 0x0080
#define MMPLL3_PLL_L_VAL                0x0084
#define MMPLL3_PLL_M_VAL                0x0088
#define MMPLL3_PLL_N_VAL                0x008C
#define MMPLL3_PLL_USER_CTL             0x0090
#define MMPLL3_PLL_CONFIG_CTL           0x0094
#define MMPLL3_PLL_TEST_CTL             0x0098
#define MMPLL3_PLL_STATUS               0x009C
#define MMPLL4_PLL_MODE                 0x00A0
#define MMPLL4_PLL_L_VAL                0x00A4
#define MMPLL4_PLL_M_VAL                0x00A8
#define MMPLL4_PLL_N_VAL                0x00AC
#define MMPLL4_PLL_USER_CTL             0x00B0
#define MMPLL4_PLL_CONFIG_CTL           0x00B4
#define MMPLL4_PLL_TEST_CTL             0x00B8
#define MMPLL4_PLL_STATUS               0x00BC
#define MMSS_PLL_VOTE_APCS              0x0100
#define VCODEC0_CMD_RCGR                0x1000
#define VENUS0_BCR                      0x1020
#define VENUS0_VCODEC0_CBCR             0x1028
#define VENUS0_CORE0_VCODEC_CBCR        0x1048
#define VENUS0_CORE1_VCODEC_CBCR        0x104C
#define VENUS0_AHB_CBCR                 0x1030
#define VENUS0_AXI_CBCR                 0x1034
#define VENUS0_OCMEMNOC_CBCR            0x1038
#define VDP_CMD_RCGR                    0x1300
#define MAPLE_CMD_RCGR                  0x1320
#define VPU_BUS_CMD_RCGR                0x1340
#define VPU_BCR                         0x1400
#define VPU_VDP_CBCR                    0x1428
#define VPU_MAPLE_CBCR                  0x142C
#define VPU_AHB_CBCR                    0x1430
#define VPU_BUS_CBCR                    0x1440
#define VPU_AXI_CBCR                    0x143C
#define VPU_CXO_CBCR                    0x1434
#define VPU_SLEEP_CBCR                  0x1438
#define PCLK0_CMD_RCGR                  0x2000
#define PCLK1_CMD_RCGR                  0x2020
#define MDP_CMD_RCGR                    0x2040
#define EXTPCLK_CMD_RCGR                0x2060
#define VSYNC_CMD_RCGR                  0x2080
#define EDPPIXEL_CMD_RCGR               0x20A0
#define EDPLINK_CMD_RCGR                0x20C0
#define EDPAUX_CMD_RCGR                 0x20E0
#define HDMI_CMD_RCGR                   0x2100
#define BYTE0_CMD_RCGR                  0x2120
#define BYTE1_CMD_RCGR                  0x2140
#define ESC0_CMD_RCGR                   0x2160
#define ESC1_CMD_RCGR                   0x2180
#define MDSS_BCR                        0x2300
#define MDSS_AHB_CBCR                   0x2308
#define MDSS_HDMI_AHB_CBCR              0x230C
#define MDSS_AXI_CBCR                   0x2310
#define MDSS_PCLK0_CBCR                 0x2314
#define MDSS_PCLK1_CBCR                 0x2318
#define MDSS_MDP_CBCR                   0x231C
#define MDSS_MDP_LUT_CBCR               0x2320
#define MDSS_EXTPCLK_CBCR               0x2324
#define MDSS_VSYNC_CBCR                 0x2328
#define MDSS_EDPPIXEL_CBCR              0x232C
#define MDSS_EDPLINK_CBCR               0x2330
#define MDSS_EDPAUX_CBCR                0x2334
#define MDSS_HDMI_CBCR                  0x2338
#define MDSS_BYTE0_CBCR                 0x233C
#define MDSS_BYTE1_CBCR                 0x2340
#define MDSS_ESC0_CBCR                  0x2344
#define MDSS_ESC1_CBCR                  0x2348
#define AVSYNC_BCR                      0x2400
#define VP_CMD_RCGR                     0x2430
#define AVSYNC_VP_CBCR                  0x2404
#define AVSYNC_AHB_CBCR                 0x2414
#define AVSYNC_EDPPIXEL_CBCR            0x2418
#define AVSYNC_EXTPCLK_CBCR             0x2410
#define AVSYNC_PCLK0_CBCR               0x241C
#define AVSYNC_PCLK1_CBCR               0x2420
#define CSI0PHYTIMER_CMD_RCGR           0x3000
#define CAMSS_PHY0_BCR                  0x3020
#define CAMSS_PHY0_CSI0PHYTIMER_CBCR    0x3024
#define CSI1PHYTIMER_CMD_RCGR           0x3030
#define CAMSS_PHY1_BCR                  0x3050
#define CAMSS_PHY1_CSI1PHYTIMER_CBCR    0x3054
#define CSI2PHYTIMER_CMD_RCGR           0x3060
#define CAMSS_PHY2_BCR                  0x3080
#define CAMSS_PHY2_CSI2PHYTIMER_CBCR    0x3084
#define CSI0_CMD_RCGR                   0x3090
#define CAMSS_CSI0_BCR                  0x30B0
#define CAMSS_CSI0_CBCR                 0x30B4
#define CAMSS_CSI0_AHB_CBCR             0x30BC
#define CAMSS_CSI0PHY_BCR               0x30C0
#define CAMSS_CSI0PHY_CBCR              0x30C4
#define CAMSS_CSI0RDI_BCR               0x30D0
#define CAMSS_CSI0RDI_CBCR              0x30D4
#define CAMSS_CSI0PIX_BCR               0x30E0
#define CAMSS_CSI0PIX_CBCR              0x30E4
#define CSI1_CMD_RCGR                   0x3100
#define CAMSS_CSI1_BCR                  0x3120
#define CAMSS_CSI1_CBCR                 0x3124
#define CAMSS_CSI1_AHB_CBCR             0x3128
#define CAMSS_CSI1PHY_BCR               0x3130
#define CAMSS_CSI1PHY_CBCR              0x3134
#define CAMSS_CSI1RDI_BCR               0x3140
#define CAMSS_CSI1RDI_CBCR              0x3144
#define CAMSS_CSI1PIX_BCR               0x3150
#define CAMSS_CSI1PIX_CBCR              0x3154
#define CSI2_CMD_RCGR                   0x3160
#define CAMSS_CSI2_BCR                  0x3180
#define CAMSS_CSI2_CBCR                 0x3184
#define CAMSS_CSI2_AHB_CBCR             0x3188
#define CAMSS_CSI2PHY_BCR               0x3190
#define CAMSS_CSI2PHY_CBCR              0x3194
#define CAMSS_CSI2RDI_BCR               0x31A0
#define CAMSS_CSI2RDI_CBCR              0x31A4
#define CAMSS_CSI2PIX_BCR               0x31B0
#define CAMSS_CSI2PIX_CBCR              0x31B4
#define CSI3_CMD_RCGR                   0x31C0
#define CAMSS_CSI3_BCR                  0x31E0
#define CAMSS_CSI3_CBCR                 0x31E4
#define CAMSS_CSI3_AHB_CBCR             0x31E8
#define CAMSS_CSI3PHY_BCR               0x31F0
#define CAMSS_CSI3PHY_CBCR              0x31F4
#define CAMSS_CSI3RDI_BCR               0x3200
#define CAMSS_CSI3RDI_CBCR              0x3204
#define CAMSS_CSI3PIX_BCR               0x3210
#define CAMSS_CSI3PIX_CBCR              0x3214
#define CAMSS_ISPIF_BCR                 0x3220
#define CAMSS_ISPIF_AHB_CBCR            0x3224
#define CCI_CMD_RCGR                    0x3300
#define CAMSS_CCI_BCR                   0x3340
#define CAMSS_CCI_CCI_CBCR              0x3344
#define CAMSS_CCI_CCI_AHB_CBCR          0x3348
#define MCLK0_CMD_RCGR                  0x3360
#define CAMSS_MCLK0_BCR                 0x3380
#define CAMSS_MCLK0_CBCR                0x3384
#define MCLK1_CMD_RCGR                  0x3390
#define CAMSS_MCLK1_BCR                 0x33B0
#define CAMSS_MCLK1_CBCR                0x33B4
#define MCLK2_CMD_RCGR                  0x33C0
#define CAMSS_MCLK2_BCR                 0x33E0
#define CAMSS_MCLK2_CBCR                0x33E4
#define MCLK3_CMD_RCGR                  0x33F0
#define CAMSS_MCLK3_BCR                 0x3410
#define CAMSS_MCLK3_CBCR                0x3414
#define GP0_CMD_RCGR                    0x3420
#define CAMSS_GP0_BCR                   0x3440
#define CAMSS_GP0_CBCR                  0x3444
#define GP1_CMD_RCGR                    0x3450
#define CAMSS_GP1_BCR                   0x3470
#define CAMSS_GP1_CBCR                  0x3474
#define CAMSS_TOP_BCR                   0x3480
#define CAMSS_TOP_AHB_CBCR              0x3484
#define CAMSS_AHB_BCR                   0x3488
#define CAMSS_AHB_CBCR                  0x348C
#define CAMSS_MICRO_BCR                 0x3490
#define CAMSS_MICRO_AHB_CBCR            0x3494
#define JPEG0_CMD_RCGR                  0x3500
#define JPEG1_CMD_RCGR                  0x3520
#define JPEG2_CMD_RCGR                  0x3540
#define CAMSS_JPEG_BCR                  0x35A0
#define CAMSS_JPEG_GDSCR                0x35A4
#define CAMSS_JPEG_JPEG0_CBCR           0x35A8
#define CAMSS_JPEG_JPEG1_CBCR           0x35AC
#define CAMSS_JPEG_JPEG2_CBCR           0x35B0
#define CAMSS_JPEG_JPEG_AHB_CBCR        0x35B4
#define CAMSS_JPEG_JPEG_AXI_CBCR        0x35B8
#define VFE0_CMD_RCGR                   0x3600
#define VFE1_CMD_RCGR                   0x3620
#define CPP_CMD_RCGR                    0x3640
#define CAMSS_VFE_BCR                   0x36A0
#define CAMSS_VFE_GDSCR                 0x36A4
#define CAMSS_VFE_VFE0_CBCR             0x36A8
#define CAMSS_VFE_VFE1_CBCR             0x36AC
#define CAMSS_VFE_CPP_CBCR              0x36B0
#define CAMSS_VFE_CPP_AHB_CBCR          0x36B4
#define CAMSS_VFE_VFE_AHB_CBCR          0x36B8
#define CAMSS_VFE_VFE_AXI_CBCR          0x36BC
#define CAMSS_CSI_VFE0_BCR              0x3700
#define CAMSS_CSI_VFE0_CBCR             0x3704
#define CAMSS_CSI_VFE1_BCR              0x3710
#define CAMSS_CSI_VFE1_CBCR             0x3714
#define OXILI_BCR                       0x4020
#define OXILI_GFX3D_CBCR                0x4028
#define OXILI_OCMEMGX_CBCR              0x402C
#define OXILICX_BCR                     0x4030
#define OXILICX_AHB_CBCR                0x403C
#define OCMEMCX_BCR                     0x4050
#define OCMEMCX_OCMEMNOC_CBCR           0x4058
#define OCMEMCX_AHB_CBCR                0x405C
#define MMPLL2_PLL_MODE                 0x4100
#define MMPLL2_PLL_L_VAL                0x4104
#define MMPLL2_PLL_M_VAL                0x4108
#define MMPLL2_PLL_N_VAL                0x410C
#define MMPLL2_PLL_USER_CTL             0x4110
#define MMPLL2_PLL_CONFIG_CTL           0x4114
#define MMPLL2_PLL_TEST_CTL             0x4118
#define MMPLL2_PLL_STATUS               0x411C
#define MMSS_PLL_VOTE_RPM               0x4200
#define AHB_CMD_RCGR                    0x5000
#define MMSS_MMSSNOC_AHB_CBCR           0x5024
#define MMSS_MMSSNOC_BTO_AHB_CBCR       0x5028
#define MMSS_MISC_AHB_CBCR              0x502C
#define AXI_CMD_RCGR                    0x5040
#define MMSS_S0_AXI_CBCR                0x5064
#define MMSS_MMSSNOC_AXI_CBCR           0x506C
#define OCMEMNOC_CMD_RCGR               0x5090
#define MMSS_DEBUG_CLK_CTL              0x0900

#define LPASS_CORE_SMMU_CFG_CLK_CBCR	0x2D000
#define LPASS_Q6_SMMU_CFG_CLK_CBCR	0x2D004
#define LPASS_DEBUG_CLK_CTL		0x32000

/* Mux source select values */
#define xo_source_val	0
#define gpll0_source_val 1
#define gpll1_source_val 2
#define gpll4_source_val 5
#define gnd_source_val	5
#define sdcc1_gnd_source_val 6
#define gpll1_hsic_source_val 4
#define pcie_pipe_source_val 2
#define sata_asic0_source_val 2
#define sata_rx_source_val 2
#define mmpll0_mm_source_val 1
#define mmpll1_mm_source_val 2
#define mmpll3_mm_source_val 3
#define mmpll4_mm_source_val 3
#define gpll0_mm_source_val 5
#define xo_mm_source_val 0
#define dsipll_750_mm_source_val 1
#define edppll_270_mm_source_val 4
#define edppll_350_mm_source_val 4
#define dsipll_750_mm_source_val 1
#define dsipll0_byte_mm_source_val 1
#define dsipll0_pixel_mm_source_val 1
#define hdmipll_mm_source_val 3

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_EXT(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_HSIC(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_hsic_source_val), \
	}

#define F_MM(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define F_MDSS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
	},					\
	.num_fmax = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_CORNER_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_CORNER_SVS_SOC,		/* VDD_DIG_LOW */
	RPM_REGULATOR_CORNER_NORMAL,		/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_CORNER_SUPER_TURBO,	/* VDD_DIG_HIGH */
};


static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63

#define RPM_SMD_KEY_ENABLE	0x62616E45

#define CXO_ID			0x0
#define QDSS_ID			0x1

#define PNOC_ID		0x0
#define SNOC_ID		0x1
#define CNOC_ID		0x2
#define MMSSNOC_AHB_ID  0x3

#define BIMC_ID		0x0
#define OXILI_ID	0x1
#define OCMEM_ID	0x2

#define BB_CLK1_ID	 1
#define BB_CLK2_ID	 2
#define RF_CLK1_ID	 4
#define RF_CLK2_ID	 5
#define RF_CLK3_ID	 6
#define DIFF_CLK1_ID	 7
#define DIV_CLK1_ID	11
#define DIV_CLK2_ID	12
#define DIV_CLK3_ID	13

DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);

DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(mmssnoc_ahb_clk, mmssnoc_ahb_a_clk, RPM_BUS_CLK_TYPE,
			MMSSNOC_AHB_ID, NULL);

DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);
DEFINE_CLK_RPM_SMD(ocmemgx_clk, ocmemgx_a_clk, RPM_MEM_CLK_TYPE, OCMEM_ID,
			NULL);
DEFINE_CLK_RPM_SMD(gfx3d_clk_src, gfx3d_a_clk_src, RPM_MEM_CLK_TYPE, OXILI_ID,
			NULL);

DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_a, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk2, bb_clk2_a, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk1, rf_clk1_a, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_a, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk3, rf_clk3_a, RF_CLK3_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(diff_clk1, diff_clk1_a, DIFF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_clk1_a, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_clk2_a, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk3, div_clk3_a, DIV_CLK3_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_a_pin, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk2_pin, bb_clk2_a_pin, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk1_pin, rf_clk1_a_pin, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk2_pin, rf_clk2_a_pin, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk3_pin, rf_clk3_a_pin, RF_CLK3_ID);

static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_acpu_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(oxili_gfx3d_clk_src, &gfx3d_clk_src.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_clk, &ocmemgx_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_a_clk, &ocmemgx_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_core_clk, &ocmemgx_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, 0);

static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &xo_clk_src.c);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0_ao_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_a_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src.c),
	},
};

static struct pll_vote_clk gpll1_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 480000000,
		.dbg_name = "gpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1_clk_src.c),
	},
};

static struct pll_vote_clk gpll4_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL4_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 800000000,
		.dbg_name = "gpll4_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll4_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ufs_axi_clk[] = {
	F(100000000,      gpll0,    6, 0, 0),
	F(200000000,      gpll0,    3, 0, 0),
	F(240000000,      gpll0,  2.5, 0, 0),
	F_END
};

static struct rcg_clk ufs_axi_clk_src = {
	.cmd_rcgr_reg = UFS_AXI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_ufs_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ufs_axi_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOW, 100000000, NOMINAL, 200000000,
				  HIGH, 240000000),
		CLK_INIT(ufs_axi_clk_src.c),
	},
};


static struct clk_freq_tbl ftbl_gcc_usb30_master_clk[] = {
	F(125000000,      gpll0,    1, 5, 24),
	F_END
};

static struct rcg_clk usb30_master_clk_src = {
	.cmd_rcgr_reg = USB30_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb30_master_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb30_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 125000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_sec_master_clk[] = {
	F(125000000,      gpll0,    1, 5, 24),
	F_END
};

static struct rcg_clk usb30_sec_master_clk_src = {
	.cmd_rcgr_reg = USB30_SEC_MASTER_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb30_sec_master_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb30_sec_master_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 125000000),
		CLK_INIT(usb30_sec_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_ahb_clk[] = {
	F_HSIC( 60000000,      gpll1,    8, 0, 0),
	F_END
};

static struct rcg_clk usb_hsic_ahb_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_AHB_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_usb_hsic_ahb_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_ahb_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb_hsic_ahb_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F( 19200000,         xo,    1, 0, 0),
	F( 50000000,      gpll0,   12, 0, 0),
	F_END
};

static struct rcg_clk blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup1_i2c_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk[] = {
	F(   960000,         xo,   10, 1, 2),
	F(  4800000,         xo,    4, 0, 0),
	F(  9600000,         xo,    2, 0, 0),
	F( 15000000,      gpll0,   10, 1, 4),
	F( 19200000,         xo,    1, 0, 0),
	F( 25000000,      gpll0,   12, 1, 2),
	F( 50000000,      gpll0,   12, 0, 0),
	F_END
};

static struct rcg_clk blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp1_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp1_qup6_spi_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart1_6_apps_clk[] = {
	F(  3686400,      gpll0,    1, 96, 15625),
	F(  7372800,      gpll0,    1, 192, 15625),
	F( 14745600,      gpll0,    1, 384, 15625),
	F( 16000000,      gpll0,    5, 2, 15),
	F( 19200000,         xo,    1, 0, 0),
	F( 24000000,      gpll0,    5, 1, 5),
	F( 32000000,      gpll0,    1, 4, 75),
	F( 40000000,      gpll0,   15, 0, 0),
	F( 46400000,      gpll0,    1, 29, 375),
	F( 48000000,      gpll0, 12.5, 0, 0),
	F( 51200000,      gpll0,    1, 32, 375),
	F( 56000000,      gpll0,    1, 7, 75),
	F( 58982400,      gpll0,    1, 1536, 15625),
	F( 60000000,      gpll0,   10, 0, 0),
	F( 63160000,      gpll0,  9.5, 0, 0),
	F_END
};

static struct rcg_clk blsp1_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp1_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP1_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp1_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp1_uart6_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup1_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup1_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP1_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup1_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup1_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup2_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup2_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP2_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup2_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup2_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup3_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup3_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP3_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup3_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup3_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup4_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup4_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP4_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup4_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup4_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP5_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup5_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup5_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup5_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP5_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup5_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup5_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_i2c_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP6_I2C_APPS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup6_i2c_apps_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 50000000),
		CLK_INIT(blsp2_qup6_i2c_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_qup6_spi_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_QUP6_SPI_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_qup6_spi_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 25000000, NOMINAL, 50000000),
		CLK_INIT(blsp2_qup6_spi_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart1_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart1_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart2_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart2_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart3_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart3_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart4_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart4_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart5_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART5_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart5_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart5_apps_clk_src.c),
	},
};

static struct rcg_clk blsp2_uart6_apps_clk_src = {
	.cmd_rcgr_reg = BLSP2_UART6_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "blsp2_uart6_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 31580000, NOMINAL, 63160000),
		CLK_INIT(blsp2_uart6_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F( 50000000,      gpll0,   12, 0, 0),
	F( 85710000,      gpll0,    7, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(171430000,      gpll0,  3.5, 0, 0),
	F_END
};

static struct rcg_clk ce1_clk_src = {
	.cmd_rcgr_reg = CE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 85710000, NOMINAL, 171430000),
		CLK_INIT(ce1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce2_clk[] = {
	F( 50000000,      gpll0,   12, 0, 0),
	F( 85710000,      gpll0,    7, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(171430000,      gpll0,  3.5, 0, 0),
	F_END
};

static struct rcg_clk ce2_clk_src = {
	.cmd_rcgr_reg = CE2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 85710000, NOMINAL, 171430000),
		CLK_INIT(ce2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce3_clk[] = {
	F( 50000000,      gpll0,   12, 0, 0),
	F( 85710000,      gpll0,    7, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(171430000,      gpll0,  3.5, 0, 0),
	F_END
};

static struct rcg_clk ce3_clk_src = {
	.cmd_rcgr_reg = CE3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_ce3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "ce3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 85710000, NOMINAL, 171430000),
		CLK_INIT(ce3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp1_3_clk[] = {
	F( 19200000,         xo,    1, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(200000000,      gpll0,    3, 0, 0),
	F_END
};

static struct rcg_clk gcc_gp1_clk_src = {
	.cmd_rcgr_reg = GCC_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gcc_gp1_clk_src.c),
	},
};

static struct rcg_clk gcc_gp2_clk_src = {
	.cmd_rcgr_reg = GCC_GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gcc_gp2_clk_src.c),
	},
};

static struct rcg_clk gcc_gp3_clk_src = {
	.cmd_rcgr_reg = GCC_GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gcc_gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_0_1_aux_clk[] = {
	F(  1010000,         xo,    1, 1, 19),
	F_END
};

static struct rcg_clk pcie_0_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_0_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pcie_0_1_aux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_0_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 1010000),
		CLK_INIT(pcie_0_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_0_1_pipe_clk[] = {
	F_EXT(125000000, pcie_pipe,    2, 0, 0),
	F_EXT(250000000, pcie_pipe,    1, 0, 0),
	F_END
};

static struct rcg_clk pcie_0_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_0_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pcie_0_1_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_0_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pcie_0_pipe_clk_src.c),
	},
};

static struct rcg_clk pcie_1_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_1_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pcie_0_1_aux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_1_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 1000000),
		CLK_INIT(pcie_1_aux_clk_src.c),
	},
};

static struct rcg_clk pcie_1_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_1_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pcie_0_1_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_1_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pcie_1_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F( 60000000,      gpll0,   10, 0, 0),
	F_END
};

static struct rcg_clk pdm2_clk_src = {
	.cmd_rcgr_reg = PDM2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pdm2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pdm2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(pdm2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_asic0_clk[] = {
	F_EXT( 75000000, sata_asic0,    4, 0, 0),
	F_EXT(150000000, sata_asic0,    2, 0, 0),
	F_EXT(300000000, sata_asic0,    1, 0, 0),
	F_END
};

static struct rcg_clk sata_asic0_clk_src = {
	.cmd_rcgr_reg = SATA_ASIC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_sata_asic0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sata_asic0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 300000000),
		CLK_INIT(sata_asic0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_pmalive_clk[] = {
	F( 19200000,         xo,    1, 0, 0),
	F( 50000000,      gpll0,   12, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F_END
};

static struct rcg_clk sata_pmalive_clk_src = {
	.cmd_rcgr_reg = SATA_PMALIVE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_sata_pmalive_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sata_pmalive_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sata_pmalive_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_rx_clk[] = {
	F_EXT( 75000000, sata_rx,    4, 0, 0),
	F_EXT(150000000, sata_rx,    2, 0, 0),
	F_EXT(300000000, sata_rx,    1, 0, 0),
	F_END
};

static struct rcg_clk sata_rx_clk_src = {
	.cmd_rcgr_reg = SATA_RX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_sata_rx_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sata_rx_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 300000000),
		CLK_INIT(sata_rx_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_rx_oob_clk[] = {
	F(100000000,      gpll0,    6, 0, 0),
	F_END
};

static struct rcg_clk sata_rx_oob_clk_src = {
	.cmd_rcgr_reg = SATA_RX_OOB_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_sata_rx_oob_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sata_rx_oob_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 100000000),
		CLK_INIT(sata_rx_oob_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_4_apps_clk[] = {
	F(   144000,         xo,   16, 3, 25),
	F(   400000,         xo,   12, 1, 4),
	F( 20000000,      gpll0,   15, 1, 2),
	F( 25000000,      gpll0,   12, 1, 2),
	F( 50000000,      gpll0,   12, 0, 0),
	F(100000000,      gpll0,    6, 0, 0),
	F(200000000,      gpll0,    3, 0, 0),
	F(400000000,      gpll4,    2, 0, 0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc1_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 400000000),
		CLK_INIT(sdcc1_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc2_apps_clk_src = {
	.cmd_rcgr_reg = SDCC2_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc3_apps_clk_src = {
	.cmd_rcgr_reg = SDCC3_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc3_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc3_apps_clk_src.c),
	},
};

static struct rcg_clk sdcc4_apps_clk_src = {
	.cmd_rcgr_reg = SDCC4_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc4_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(sdcc4_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_tsif_ref_clk[] = {
	F(   105000,         xo,    2, 1, 91),
	F_END
};

static struct rcg_clk tsif_ref_clk_src = {
	.cmd_rcgr_reg = TSIF_REF_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_tsif_ref_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "tsif_ref_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 105500),
		CLK_INIT(tsif_ref_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_mock_utmi_clk[] = {
	F( 19200000,      xo,      1,  0, 0),
	F( 60000000,      gpll0,   10, 0, 0),
	F_END
};

static struct rcg_clk usb30_mock_utmi_clk_src = {
	.cmd_rcgr_reg = USB30_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb30_mock_utmi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb30_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_sec_mock_utmi_clk[] = {
	F( 19200000,      xo,      1,  0, 0),
	F( 60000000,      gpll0,   10, 0, 0),
	F_END
};

static struct rcg_clk usb30_sec_mock_utmi_clk_src = {
	.cmd_rcgr_reg = USB30_SEC_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb30_sec_mock_utmi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb30_sec_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb30_sec_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F( 75000000,      gpll0,    8, 0, 0),
	F_END
};

static struct rcg_clk usb_hs_system_clk_src = {
	.cmd_rcgr_reg = USB_HS_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hs_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hs_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 60000000, NOMINAL, 75000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_clk[] = {
	F_HSIC(480000000,      gpll1,    1, 0, 0),
	F_END
};

static struct rcg_clk usb_hsic_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 480000000),
		CLK_INIT(usb_hsic_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_io_cal_clk[] = {
	F(  9600000,         xo,    2, 0, 0),
	F_END
};

static struct rcg_clk usb_hsic_io_cal_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_IO_CAL_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_io_cal_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_io_cal_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 9600000),
		CLK_INIT(usb_hsic_io_cal_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_mock_utmi_clk[] = {
	F( 19200000,      xo,      1,  0, 0),
	F( 60000000,      gpll0,   10, 0, 0),
	F_END
};

static struct rcg_clk usb_hsic_mock_utmi_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_MOCK_UTMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_mock_utmi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_mock_utmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 60000000),
		CLK_INIT(usb_hsic_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_system_clk[] = {
	F( 75000000,      gpll0,    8, 0, 0),
	F_END
};

static struct rcg_clk usb_hsic_system_clk_src = {
	.cmd_rcgr_reg = USB_HSIC_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hsic_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hsic_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 75000000),
		CLK_INIT(usb_hsic_system_clk_src.c),
	},
};

static struct local_vote_clk gcc_bam_dma_ahb_clk = {
	.cbcr_reg = BAM_DMA_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(12),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bam_dma_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_bam_dma_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp1_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup1_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup1_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup2_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup2_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup3_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup3_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup4_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup5_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup5_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup6_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_qup6_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart1_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart2_apps_clk = {
	.cbcr_reg = BLSP1_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart2_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart3_apps_clk = {
	.cbcr_reg = BLSP1_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart3_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart4_apps_clk = {
	.cbcr_reg = BLSP1_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart4_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart5_apps_clk = {
	.cbcr_reg = BLSP1_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart5_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart6_apps_clk = {
	.cbcr_reg = BLSP1_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart6_apps_clk.c),
	},
};

static struct local_vote_clk gcc_blsp2_ahb_clk = {
	.cbcr_reg = BLSP2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(15),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_blsp2_ahb_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup1_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup1_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup2_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup2_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup3_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup3_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup4_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup4_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup5_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup5_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup5_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup6_i2c_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_qup6_spi_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_qup6_spi_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart1_apps_clk = {
	.cbcr_reg = BLSP2_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart1_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart2_apps_clk = {
	.cbcr_reg = BLSP2_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart2_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart3_apps_clk = {
	.cbcr_reg = BLSP2_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart3_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart4_apps_clk = {
	.cbcr_reg = BLSP2_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart4_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart5_apps_clk = {
	.cbcr_reg = BLSP2_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart5_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart5_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart6_apps_clk = {
	.cbcr_reg = BLSP2_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
	},
};

static struct local_vote_clk gcc_boot_rom_ahb_clk = {
	.cbcr_reg = BOOT_ROM_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(10),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_boot_rom_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_boot_rom_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_ahb_clk = {
	.cbcr_reg = CE1_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(3),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_axi_clk = {
	.cbcr_reg = CE1_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(4),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_axi_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_clk = {
	.cbcr_reg = CE1_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(5),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ce1_clk_src.c,
		.dbg_name = "gcc_ce1_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_clk.c),
	},
};

static struct local_vote_clk gcc_ce2_ahb_clk = {
	.cbcr_reg = CE2_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce2_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce2_axi_clk = {
	.cbcr_reg = CE2_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(1),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce2_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_axi_clk.c),
	},
};

static struct local_vote_clk gcc_ce2_clk = {
	.cbcr_reg = CE2_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(2),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ce2_clk_src.c,
		.dbg_name = "gcc_ce2_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_clk.c),
	},
};

static struct local_vote_clk gcc_ce3_ahb_clk = {
	.cbcr_reg = CE3_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(28),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce3_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce3_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_ce3_axi_clk = {
	.cbcr_reg = CE3_AXI_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(29),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce3_axi_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce3_axi_clk.c),
	},
};

static struct local_vote_clk gcc_ce3_clk = {
	.cbcr_reg = CE3_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(30),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ce3_clk_src.c,
		.dbg_name = "gcc_ce3_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce3_clk.c),
	},
};

static struct branch_clk gcc_copss_smmu_ahb_clk = {
	.cbcr_reg = COPSS_SMMU_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_copss_smmu_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_copss_smmu_ahb_clk.c),
	},
};

static struct branch_clk gcc_copss_smmu_axi_clk = {
	.cbcr_reg = COPSS_SMMU_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_copss_smmu_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_copss_smmu_axi_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GCC_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_gp1_clk_src.c,
		.dbg_name = "gcc_gp1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GCC_GP2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_gp2_clk_src.c,
		.dbg_name = "gcc_gp2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GCC_GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gcc_gp3_clk_src.c,
		.dbg_name = "gcc_gp3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_lpass_mport_axi_clk = {
	.cbcr_reg = LPASS_MPORT_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_mport_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_mport_axi_clk.c),
	},
};

static struct branch_clk gcc_lpass_q6_axi_clk = {
	.cbcr_reg = LPASS_Q6_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_q6_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_q6_axi_clk.c),
	},
};

static struct branch_clk gcc_lpass_sway_clk = {
	.cbcr_reg = LPASS_SWAY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_lpass_sway_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_lpass_sway_clk.c),
	},
};

static struct branch_clk gcc_mmss_vpu_maple_sys_noc_axi_clk = {
	.cbcr_reg = MMSS_VPU_MAPLE_SYS_NOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mmss_vpu_maple_sys_noc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_vpu_maple_sys_noc_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_aux_clk = {
	.cbcr_reg = PCIE_0_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_0_aux_clk_src.c,
		.dbg_name = "gcc_pcie_0_aux_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_cfg_ahb_clk = {
	.cbcr_reg = PCIE_0_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_0_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_mstr_axi_clk = {
	.cbcr_reg = PCIE_0_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_0_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_pipe_clk = {
	.cbcr_reg = PCIE_0_PIPE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_0_pipe_clk_src.c,
		.dbg_name = "gcc_pcie_0_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_0_slv_axi_clk = {
	.cbcr_reg = PCIE_0_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_0_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_0_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_aux_clk = {
	.cbcr_reg = PCIE_1_AUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_1_aux_clk_src.c,
		.dbg_name = "gcc_pcie_1_aux_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_aux_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_cfg_ahb_clk = {
	.cbcr_reg = PCIE_1_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_1_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_mstr_axi_clk = {
	.cbcr_reg = PCIE_1_MSTR_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_1_mstr_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_mstr_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_pipe_clk = {
	.cbcr_reg = PCIE_1_PIPE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pcie_1_pipe_clk_src.c,
		.dbg_name = "gcc_pcie_1_pipe_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_1_slv_axi_clk = {
	.cbcr_reg = PCIE_1_SLV_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_1_slv_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_1_slv_axi_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &pdm2_clk_src.c,
		.dbg_name = "gcc_pdm2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm2_clk.c),
	},
};

static struct branch_clk gcc_pdm_ahb_clk = {
	.cbcr_reg = PDM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pdm_ahb_clk.c),
	},
};

static struct branch_clk gcc_periph_noc_usb_hsic_ahb_clk = {
	.cbcr_reg = PERIPH_NOC_USB_HSIC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_ahb_clk_src.c,
		.dbg_name = "gcc_periph_noc_usb_hsic_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_periph_noc_usb_hsic_ahb_clk.c),
	},
};

static struct local_vote_clk gcc_prng_ahb_clk = {
	.cbcr_reg = PRNG_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(13),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_prng_ahb_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_prng_ahb_clk.c),
	},
};

static struct branch_clk gcc_sata_asic0_clk = {
	.cbcr_reg = SATA_ASIC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sata_asic0_clk_src.c,
		.dbg_name = "gcc_sata_asic0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_asic0_clk.c),
	},
};

static struct branch_clk gcc_sata_axi_clk = {
	.cbcr_reg = SATA_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sata_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_axi_clk.c),
	},
};

static struct branch_clk gcc_sata_cfg_ahb_clk = {
	.cbcr_reg = SATA_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sata_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_sata_pmalive_clk = {
	.cbcr_reg = SATA_PMALIVE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sata_pmalive_clk_src.c,
		.dbg_name = "gcc_sata_pmalive_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_pmalive_clk.c),
	},
};

static struct branch_clk gcc_sata_rx_clk = {
	.cbcr_reg = SATA_RX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sata_rx_clk_src.c,
		.dbg_name = "gcc_sata_rx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_rx_clk.c),
	},
};

static struct branch_clk gcc_sata_rx_oob_clk = {
	.cbcr_reg = SATA_RX_OOB_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sata_rx_oob_clk_src.c,
		.dbg_name = "gcc_sata_rx_oob_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_rx_oob_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk = {
	.cbcr_reg = SDCC1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_apps_clk = {
	.cbcr_reg = SDCC1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc1_apps_clk_src.c,
		.dbg_name = "gcc_sdcc1_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_cdccal_ff_clk = {
	.cbcr_reg = SDCC1_CDCCAL_FF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_cdccal_ff_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_cdccal_ff_clk.c),
	},
};

static struct branch_clk gcc_sdcc1_cdccal_sleep_clk = {
	.cbcr_reg = SDCC1_CDCCAL_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc1_cdccal_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_cdccal_sleep_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_ahb_clk = {
	.cbcr_reg = SDCC2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc2_apps_clk = {
	.cbcr_reg = SDCC2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc2_apps_clk_src.c,
		.dbg_name = "gcc_sdcc2_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_ahb_clk = {
	.cbcr_reg = SDCC3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc3_apps_clk = {
	.cbcr_reg = SDCC3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc3_apps_clk_src.c,
		.dbg_name = "gcc_sdcc3_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc3_apps_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_ahb_clk = {
	.cbcr_reg = SDCC4_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sdcc4_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_ahb_clk.c),
	},
};

static struct branch_clk gcc_sdcc4_apps_clk = {
	.cbcr_reg = SDCC4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc4_apps_clk_src.c,
		.dbg_name = "gcc_sdcc4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_apps_clk.c),
	},
};

static struct branch_clk gcc_spss_ahb_clk = {
	.cbcr_reg = SPSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_spss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_spss_ahb_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_ufs_axi_clk = {
	.cbcr_reg = SYS_NOC_UFS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ufs_axi_clk_src.c,
		.dbg_name = "gcc_sys_noc_ufs_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_master_clk_src.c,
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_usb3_sec_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_SEC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_sec_master_clk_src.c,
		.dbg_name = "gcc_sys_noc_usb3_sec_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_sec_axi_clk.c),
	},
};

static struct branch_clk gcc_tsif_ahb_clk = {
	.cbcr_reg = TSIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_tsif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ahb_clk.c),
	},
};

static struct branch_clk gcc_tsif_ref_clk = {
	.cbcr_reg = TSIF_REF_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &tsif_ref_clk_src.c,
		.dbg_name = "gcc_tsif_ref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ref_clk.c),
	},
};

static struct branch_clk gcc_ufs_ahb_clk = {
	.cbcr_reg = UFS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ufs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_ahb_clk.c),
	},
};

static struct branch_clk gcc_ufs_axi_clk = {
	.cbcr_reg = UFS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ufs_axi_clk_src.c,
		.dbg_name = "gcc_ufs_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_axi_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_cfg_clk = {
	.cbcr_reg = UFS_RX_CFG_CBCR,
	.has_sibling = 1,
	.max_div = 16,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ufs_axi_clk_src.c,
		.dbg_name = "gcc_ufs_rx_cfg_clk",
		.ops = &clk_ops_branch,
		.rate = 2,
		CLK_INIT(gcc_ufs_rx_cfg_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_symbol_0_clk = {
	.cbcr_reg = UFS_RX_SYMBOL_0_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_rx_symbol_0_clk.c),
	},
};

static struct branch_clk gcc_ufs_rx_symbol_1_clk = {
	.cbcr_reg = UFS_RX_SYMBOL_1_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ufs_rx_symbol_1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_rx_symbol_1_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_cfg_clk = {
	.cbcr_reg = UFS_TX_CFG_CBCR,
	.has_sibling = 1,
	.max_div = 16,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &ufs_axi_clk_src.c,
		.dbg_name = "gcc_ufs_tx_cfg_clk",
		.ops = &clk_ops_branch,
		.rate = 2,
		CLK_INIT(gcc_ufs_tx_cfg_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_symbol_0_clk = {
	.cbcr_reg = UFS_TX_SYMBOL_0_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ufs_tx_symbol_0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_tx_symbol_0_clk.c),
	},
};

static struct branch_clk gcc_ufs_tx_symbol_1_clk = {
	.cbcr_reg = UFS_TX_SYMBOL_1_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ufs_tx_symbol_1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ufs_tx_symbol_1_clk.c),
	},
};

static struct branch_clk gcc_usb2a_phy_sleep_clk = {
	.cbcr_reg = USB2A_PHY_SLEEP_CBCR,
	.bcr_reg = USB2A_PHY_BCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2a_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2a_phy_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb2b_phy_sleep_clk = {
	.cbcr_reg = USB2B_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2b_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2b_phy_sleep_clk.c),
	},
};

/* Allow clk_set_rate on this branch clock */
static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
	.bcr_reg = USB_30_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_master_clk_src.c,
		.dbg_name = "gcc_usb30_master_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_master_clk.c),
		.depends = &gcc_sys_noc_usb3_axi_clk.c,
	},
};

static struct branch_clk gcc_usb30_mock_utmi_clk = {
	.cbcr_reg = USB30_MOCK_UTMI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_mock_utmi_clk_src.c,
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_mock_utmi_clk.c),
	},
};

static struct branch_clk gcc_usb30_sleep_clk = {
	.cbcr_reg = USB30_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

/* Set has_sibling to 0 to allow set rate on this branch clock */
static struct branch_clk gcc_usb30_sec_master_clk = {
	.cbcr_reg = USB30_SEC_MASTER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_sec_master_clk_src.c,
		.dbg_name = "gcc_usb30_sec_master_clk",
		.ops = &clk_ops_branch,
		.depends = &gcc_sys_noc_usb3_sec_axi_clk.c,
		CLK_INIT(gcc_usb30_sec_master_clk.c),
	},
};

static struct branch_clk gcc_usb30_sec_mock_utmi_clk = {
	.cbcr_reg = USB30_SEC_MOCK_UTMI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_sec_mock_utmi_clk_src.c,
		.dbg_name = "gcc_usb30_sec_mock_utmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sec_mock_utmi_clk.c),
	},
};

static struct branch_clk gcc_usb30_sec_sleep_clk = {
	.cbcr_reg = USB30_SEC_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_sec_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sec_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_ahb_clk = {
	.cbcr_reg = USB_HS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hs_system_clk = {
	.cbcr_reg = USB_HS_SYSTEM_CBCR,
	.bcr_reg = USB_HS_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hs_system_clk_src.c,
		.dbg_name = "gcc_usb_hs_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

/* Set has_sibling to 0 to allow set rate on this branch clock */
static struct branch_clk gcc_usb_hsic_ahb_clk = {
	.cbcr_reg = USB_HSIC_AHB_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_ahb_clk_src.c,
		.dbg_name = "gcc_usb_hsic_ahb_clk",
		.ops = &clk_ops_branch,
		.depends = &gcc_periph_noc_usb_hsic_ahb_clk.c,
		CLK_INIT(gcc_usb_hsic_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_clk = {
	.cbcr_reg = USB_HSIC_CBCR,
	.bcr_reg = USB_HS_HSIC_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_clk_src.c,
		.dbg_name = "gcc_usb_hsic_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_io_cal_clk = {
	.cbcr_reg = USB_HSIC_IO_CAL_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_io_cal_clk_src.c,
		.dbg_name = "gcc_usb_hsic_io_cal_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_io_cal_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_io_cal_sleep_clk = {
	.cbcr_reg = USB_HSIC_IO_CAL_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_io_cal_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_io_cal_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_mock_utmi_clk = {
	.cbcr_reg = USB_HSIC_MOCK_UTMI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_mock_utmi_clk_src.c,
		.dbg_name = "gcc_usb_hsic_mock_utmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_mock_utmi_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_system_clk = {
	.cbcr_reg = USB_HSIC_SYSTEM_CBCR,
	.bcr_reg = USB_HS_HSIC_BCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_system_clk_src.c,
		.dbg_name = "gcc_usb_hsic_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

static struct reset_clk gcc_usb30_phy_com_clk = {
	.reset_reg = USB30_PHY_COM_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_phy_com_clk",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb30_phy_com_clk.c),
	},
};

static struct reset_clk gcc_usb3_phy_clk = {
	.reset_reg = USB3_PHY_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb3_phy_clk",
		.ops = &clk_ops_rst,
		CLK_INIT(gcc_usb3_phy_clk.c),
	},
};

static struct pll_vote_clk mmpll0_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)MMPLL0_PLL_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 800000000,
		.dbg_name = "mmpll0_pll_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll0_clk_src.c),
	},
};

static struct pll_vote_clk mmpll1_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)MMPLL1_PLL_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 1167000000,
		.dbg_name = "mmpll1_pll_clk_src",
		.ops = &clk_ops_pll_vote,
		VDD_DIG_FMAX_MAP1(NOMINAL, 1167000000),
		CLK_INIT(mmpll1_clk_src.c),
	},
};

static struct pll_clk mmpll3_clk_src = {
	.mode_reg = (void __iomem *)MMPLL3_PLL_MODE,
	.status_reg = (void __iomem *)MMPLL3_PLL_STATUS,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.dbg_name = "mmpll3_pll_clk_src",
		.rate = 930000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll3_clk_src.c),
	},
};

static struct pll_clk mmpll4_clk_src = {
	.mode_reg = (void __iomem *)MMPLL4_PLL_MODE,
	.status_reg = (void __iomem *)MMPLL4_PLL_STATUS,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.dbg_name = "mmpll4_pll_clk_src",
		.rate = 930000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll4_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mmss_axi_clk[] = {
	F_MM( 19200000,    xo,     1,   0,   0),
	F_MM( 37500000,  gpll0,    16,   0,   0),
	F_MM( 50000000,  gpll0,    12,   0,   0),
	F_MM( 75000000,  gpll0,     8,   0,   0),
	F_MM(100000000,  gpll0,     6,   0,   0),
	F_MM(150000000,  gpll0,     4,   0,   0),
	F_MM(333430000, mmpll1,   3.5,   0,   0),
	F_MM(400000000, mmpll0,     2,   0,   0),
	F_MM(466800000, mmpll1,   2.5,   0,   0),
	F_END
};

static struct rcg_clk axi_clk_src = {
	.cmd_rcgr_reg = AXI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mmss_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 333430000,
				  HIGH, 466800000),
		CLK_INIT(axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_csi0_3_clk[] = {
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(200000000, mmpll0,    4, 0, 0),
	F_END
};

static struct rcg_clk csi0_clk_src = {
	.cmd_rcgr_reg = CSI0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi0_clk_src.c),
	},
};

static struct rcg_clk csi1_clk_src = {
	.cmd_rcgr_reg = CSI1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1_clk_src.c),
	},
};

static struct rcg_clk csi2_clk_src = {
	.cmd_rcgr_reg = CSI2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2_clk_src.c),
	},
};

static struct rcg_clk csi3_clk_src = {
	.cmd_rcgr_reg = CSI3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_csi0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_clk[] = {
	F_MM( 50000000,      gpll0,   12, 0, 0),
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(133330000,	    mmpll0,    6, 0, 0),
	F_MM(200000000,     mmpll0,    4, 0, 0),
	F_MM(266670000,     mmpll0,    3, 0, 0),
	F_MM(465000000,     mmpll3,    2, 0, 0),
	F_END
};

static struct rcg_clk vcodec0_clk_src = {
	.cmd_rcgr_reg = VCODEC0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_venus0_vcodec0_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcodec0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 465000000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_vfe_vfe0_1_clk[] = {
	F_MM( 37500000,      gpll0,   16, 0, 0),
	F_MM( 50000000,      gpll0,   12, 0, 0),
	F_MM( 60000000,      gpll0,   10, 0, 0),
	F_MM( 80000000,      gpll0,  7.5, 0, 0),
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(109090000,      gpll0,  5.5, 0, 0),
	F_MM(133330000,      gpll0,  4.5, 0, 0),
	F_MM(200000000,      gpll0,    3, 0, 0),
	F_MM(228570000,     mmpll0,  3.5, 0, 0),
	F_MM(266670000,     mmpll0,    3, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_MM(465000000,     mmpll4,    2, 0, 0),
	F_MM(600000000,      gpll0,    1, 0, 0),
	F_END
};

static struct rcg_clk vfe0_clk_src = {
	.cmd_rcgr_reg = VFE0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_vfe_vfe0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vfe0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 200000000, NOMINAL, 465000000,
				  HIGH, 600000000),
		CLK_INIT(vfe0_clk_src.c),
	},
};

static struct rcg_clk vfe1_clk_src = {
	.cmd_rcgr_reg = VFE1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_vfe_vfe0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vfe1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 200000000, NOMINAL, 465000000,
				  HIGH, 600000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_mdp_clk[] = {
	F_MM( 37500000,      gpll0,   16, 0, 0),
	F_MM( 60000000,      gpll0,   10, 0, 0),
	F_MM( 75000000,      gpll0,    8, 0, 0),
	F_MM( 85710000,      gpll0,    7, 0, 0),
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(133330000,     mmpll0,    6, 0, 0),
	F_MM(160000000,     mmpll0,    5, 0, 0),
	F_MM(200000000,     mmpll0,    4, 0, 0),
	F_MM(228570000,     mmpll0,  3.5, 0, 0),
	F_MM(300000000,      gpll0,    2, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_END
};

static struct rcg_clk mdp_clk_src = {
	.cmd_rcgr_reg = MDP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_mdp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 300000000,
				  HIGH, 320000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ocmemnoc_clk[] = {
	F_MM( 19200000,         xo,    1, 0, 0),
	F_MM( 37500000,      gpll0,   16, 0, 0),
	F_MM( 50000000,      gpll0,   12, 0, 0),
	F_MM( 75000000,      gpll0,    8, 0, 0),
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(150000000,      gpll0,    4, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_MM(400000000,     mmpll0,    2, 0, 0),
	F_END
};

static struct rcg_clk ocmemnoc_clk_src = {
	.cmd_rcgr_reg = OCMEMNOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ocmemnoc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "ocmemnoc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 320000000,
				  HIGH, 400000000),
		CLK_INIT(ocmemnoc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_jpeg_jpeg0_2_clk[] = {
	F_MM( 75000000,      gpll0,    8, 0, 0),
	F_MM(133330000,      gpll0,  4.5, 0, 0),
	F_MM(200000000,      gpll0,    3, 0, 0),
	F_MM(228570000,     mmpll0,  3.5, 0, 0),
	F_MM(266670000,     mmpll0,    3, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_END
};

static struct rcg_clk jpeg0_clk_src = {
	.cmd_rcgr_reg = JPEG0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg0_clk_src.c),
	},
};

static struct rcg_clk jpeg1_clk_src = {
	.cmd_rcgr_reg = JPEG1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg1_clk_src.c),
	},
};

static struct rcg_clk jpeg2_clk_src = {
	.cmd_rcgr_reg = JPEG2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg0_2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(jpeg2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_avsync_vp_clk[] = {
	F_MM(150000000,      gpll0,    4, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_END
};

static struct rcg_clk vp_clk_src = {
	.cmd_rcgr_reg = VP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_avsync_vp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 320000000),
		CLK_INIT(vp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_cci_cci_clk[] = {
	F_MM( 19200000,         xo,    1, 0, 0),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg = CCI_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_cci_cci_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cci_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp0_1_clk[] = {
	F_MM(    10000,         xo,   16, 1, 120),
	F_MM(    24000,         xo,   16, 1, 50),
	F_MM(  6000000,      gpll0,   10, 1, 10),
	F_MM( 12000000,      gpll0,   10, 1, 5),
	F_MM( 13000000,      gpll0,    4, 13, 150),
	F_MM( 24000000,      gpll0,    5, 1, 5),
	F_END
};

static struct rcg_clk gp0_clk_src = {
	.cmd_rcgr_reg = GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp0_clk_src.c),
	},
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_mclk0_3_clk[] = {
	F_MM(  4800000,         xo,    4, 0, 0),
	F_MM(  6000000,      gpll0,   10, 1, 10),
	F_MM(  8000000,      gpll0,   15, 1, 5),
	F_MM(  9600000,         xo,    2, 0, 0),
	F_MM( 16000000,     mmpll0,   10, 1, 5),
	F_MM( 19200000,         xo,    1, 0, 0),
	F_MM( 24000000,      gpll0,    5, 1, 5),
	F_MM( 32000000,     mmpll0,    5, 1, 5),
	F_MM( 48000000,      gpll0, 12.5, 0, 0),
	F_MM( 64000000,     mmpll0, 12.5, 0, 0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MCLK0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MCLK1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MCLK2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct rcg_clk mclk3_clk_src = {
	.cmd_rcgr_reg = MCLK3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_phy0_2_csi0_2phytimer_clk[] = {
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(200000000,     mmpll0,    4, 0, 0),
	F_END
};

static struct rcg_clk csi0phytimer_clk_src = {
	.cmd_rcgr_reg = CSI0PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_2_csi0_2phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi0phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi0phytimer_clk_src.c),
	},
};

static struct rcg_clk csi1phytimer_clk_src = {
	.cmd_rcgr_reg = CSI1PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_2_csi0_2phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi1phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi1phytimer_clk_src.c),
	},
};

static struct rcg_clk csi2phytimer_clk_src = {
	.cmd_rcgr_reg = CSI2PHYTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_phy0_2_csi0_2phytimer_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "csi2phytimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(csi2phytimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_vfe_cpp_clk[] = {
	F_MM(133330000,      gpll0,  4.5, 0, 0),
	F_MM(266670000,     mmpll0,    3, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_MM(465000000,     mmpll4,    2, 0, 0),
	F_MM(600000000,      gpll0,    1, 0, 0),
	F_END
};

static struct rcg_clk cpp_clk_src = {
	.cmd_rcgr_reg = CPP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_vfe_cpp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cpp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 200000000, NOMINAL, 465000000,
				  HIGH, 600000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_edpaux_clk[] = {
	F_MM( 19200000,         xo,    1, 0, 0),
	F_END
};

static struct rcg_clk edpaux_clk_src = {
	.cmd_rcgr_reg = EDPAUX_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_edpaux_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "edpaux_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(edpaux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_esc0_1_clk[] = {
	F_MM( 19200000,         xo,    1, 0, 0),
	F_END
};

static struct rcg_clk esc0_clk_src = {
	.cmd_rcgr_reg = ESC0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_esc0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "esc0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(esc0_clk_src.c),
	},
};

static struct rcg_clk esc1_clk_src = {
	.cmd_rcgr_reg = ESC1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_esc0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "esc1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(esc1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_hdmi_clk[] = {
	F_MM( 19200000,         xo,    1, 0, 0),
	F_END
};

static struct rcg_clk hdmi_clk_src = {
	.cmd_rcgr_reg = HDMI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_hdmi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "hdmi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(hdmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_vsync_clk[] = {
	F_MM( 19200000,         xo,    1, 0, 0),
	F_END
};

static struct rcg_clk vsync_clk_src = {
	.cmd_rcgr_reg = VSYNC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mdss_vsync_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vsync_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_maple_clk[] = {
	F_MM( 50000000,      gpll0,   12, 0, 0),
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(200000000,     mmpll0,    4, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_MM(400000000,     mmpll0,    2, 0, 0),
	F_END
};

static struct rcg_clk maple_clk_src = {
	.cmd_rcgr_reg = MAPLE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_maple_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "maple_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 400000000),
		CLK_INIT(maple_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_vdp_clk[] = {
	F_MM( 50000000,      gpll0,   12, 0, 0),
	F_MM(100000000,      gpll0,    6, 0, 0),
	F_MM(200000000,     mmpll0,    4, 0, 0),
	F_MM(320000000,     mmpll0,  2.5, 0, 0),
	F_MM(400000000,     mmpll0,    2, 0, 0),
	F_END
};

static struct rcg_clk vdp_clk_src = {
	.cmd_rcgr_reg = VDP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_vdp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vdp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 200000000, NOMINAL, 320000000,
				  HIGH, 400000000),
		CLK_INIT(vdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_bus_clk[] = {
	F_MM( 40000000,      gpll0,   15, 0, 0),
	F_MM( 80000000,     mmpll0,   10, 0, 0),
	F_END
};

static struct rcg_clk vpu_bus_clk_src = {
	.cmd_rcgr_reg = VPU_BUS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_bus_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_bus_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 40000000, NOMINAL, 80000000),
		CLK_INIT(vpu_bus_clk_src.c),
	},
};

static struct branch_clk avsync_ahb_clk = {
	.cbcr_reg = AVSYNC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "avsync_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(avsync_ahb_clk.c),
	},
};

static struct branch_clk avsync_vp_clk = {
	.cbcr_reg = AVSYNC_VP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vp_clk_src.c,
		.dbg_name = "avsync_vp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(avsync_vp_clk.c),
	},
};

static struct branch_clk camss_ahb_clk = {
	.cbcr_reg = CAMSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_cci_ahb_clk = {
	.cbcr_reg = CAMSS_CCI_CCI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_cci_cci_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_cci_ahb_clk.c),
	},
};

static struct branch_clk camss_cci_cci_clk = {
	.cbcr_reg = CAMSS_CCI_CCI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cci_clk_src.c,
		.dbg_name = "camss_cci_cci_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_cci_cci_clk.c),
	},
};

static struct branch_clk camss_csi0_ahb_clk = {
	.cbcr_reg = CAMSS_CSI0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_ahb_clk.c),
	},
};

static struct branch_clk camss_csi0_clk = {
	.cbcr_reg = CAMSS_CSI0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0_clk.c),
	},
};

static struct branch_clk camss_csi0phy_clk = {
	.cbcr_reg = CAMSS_CSI0PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0phy_clk.c),
	},
};

static struct branch_clk camss_csi0pix_clk = {
	.cbcr_reg = CAMSS_CSI0PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0pix_clk.c),
	},
};

static struct branch_clk camss_csi0rdi_clk = {
	.cbcr_reg = CAMSS_CSI0RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0_clk_src.c,
		.dbg_name = "camss_csi0rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi0rdi_clk.c),
	},
};

static struct branch_clk camss_csi1_ahb_clk = {
	.cbcr_reg = CAMSS_CSI1_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi1_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_ahb_clk.c),
	},
};

static struct branch_clk camss_csi1_clk = {
	.cbcr_reg = CAMSS_CSI1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1_clk.c),
	},
};

static struct branch_clk camss_csi1phy_clk = {
	.cbcr_reg = CAMSS_CSI1PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1phy_clk.c),
	},
};

static struct branch_clk camss_csi1pix_clk = {
	.cbcr_reg = CAMSS_CSI1PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1pix_clk.c),
	},
};

static struct branch_clk camss_csi1rdi_clk = {
	.cbcr_reg = CAMSS_CSI1RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1_clk_src.c,
		.dbg_name = "camss_csi1rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi1rdi_clk.c),
	},
};

static struct branch_clk camss_csi2_ahb_clk = {
	.cbcr_reg = CAMSS_CSI2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_ahb_clk.c),
	},
};

static struct branch_clk camss_csi2_clk = {
	.cbcr_reg = CAMSS_CSI2_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2_clk.c),
	},
};

static struct branch_clk camss_csi2phy_clk = {
	.cbcr_reg = CAMSS_CSI2PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2phy_clk.c),
	},
};

static struct branch_clk camss_csi2pix_clk = {
	.cbcr_reg = CAMSS_CSI2PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2pix_clk.c),
	},
};

static struct branch_clk camss_csi2rdi_clk = {
	.cbcr_reg = CAMSS_CSI2RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2_clk_src.c,
		.dbg_name = "camss_csi2rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi2rdi_clk.c),
	},
};

static struct branch_clk camss_csi3_ahb_clk = {
	.cbcr_reg = CAMSS_CSI3_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_csi3_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_ahb_clk.c),
	},
};

static struct branch_clk camss_csi3_clk = {
	.cbcr_reg = CAMSS_CSI3_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3_clk.c),
	},
};

static struct branch_clk camss_csi3phy_clk = {
	.cbcr_reg = CAMSS_CSI3PHY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3phy_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3phy_clk.c),
	},
};

static struct branch_clk camss_csi3pix_clk = {
	.cbcr_reg = CAMSS_CSI3PIX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3pix_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3pix_clk.c),
	},
};

static struct branch_clk camss_csi3rdi_clk = {
	.cbcr_reg = CAMSS_CSI3RDI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi3_clk_src.c,
		.dbg_name = "camss_csi3rdi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi3rdi_clk.c),
	},
};

static struct branch_clk camss_csi_vfe0_clk = {
	.cbcr_reg = CAMSS_CSI_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe0_clk_src.c,
		.dbg_name = "camss_csi_vfe0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe0_clk.c),
	},
};

static struct branch_clk camss_csi_vfe1_clk = {
	.cbcr_reg = CAMSS_CSI_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe1_clk_src.c,
		.dbg_name = "camss_csi_vfe1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_csi_vfe1_clk.c),
	},
};

static struct branch_clk camss_gp0_clk = {
	.cbcr_reg = CAMSS_GP0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &gp0_clk_src.c,
		.dbg_name = "camss_gp0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp0_clk.c),
	},
};

static struct branch_clk camss_gp1_clk = {
	.cbcr_reg = CAMSS_GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &gp1_clk_src.c,
		.dbg_name = "camss_gp1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_gp1_clk.c),
	},
};

static struct branch_clk camss_ispif_ahb_clk = {
	.cbcr_reg = CAMSS_ISPIF_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_ispif_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_ispif_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg0_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &jpeg0_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg0_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg1_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &jpeg1_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg1_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg2_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &jpeg2_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg2_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_ahb_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_jpeg_jpeg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_ahb_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg_axi_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_axi_clk.c),
	},
};

static struct branch_clk camss_mclk0_clk = {
	.cbcr_reg = CAMSS_MCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk0_clk_src.c,
		.dbg_name = "camss_mclk0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk0_clk.c),
	},
};

static struct branch_clk camss_mclk1_clk = {
	.cbcr_reg = CAMSS_MCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk1_clk_src.c,
		.dbg_name = "camss_mclk1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk1_clk.c),
	},
};

static struct branch_clk camss_mclk2_clk = {
	.cbcr_reg = CAMSS_MCLK2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk2_clk_src.c,
		.dbg_name = "camss_mclk2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk2_clk.c),
	},
};

static struct branch_clk camss_mclk3_clk = {
	.cbcr_reg = CAMSS_MCLK3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mclk3_clk_src.c,
		.dbg_name = "camss_mclk3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_mclk3_clk.c),
	},
};

static struct branch_clk camss_micro_ahb_clk = {
	.cbcr_reg = CAMSS_MICRO_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_micro_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_micro_ahb_clk.c),
	},
};

static struct branch_clk camss_phy0_csi0phytimer_clk = {
	.cbcr_reg = CAMSS_PHY0_CSI0PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi0phytimer_clk_src.c,
		.dbg_name = "camss_phy0_csi0phytimer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy0_csi0phytimer_clk.c),
	},
};

static struct branch_clk camss_phy1_csi1phytimer_clk = {
	.cbcr_reg = CAMSS_PHY1_CSI1PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi1phytimer_clk_src.c,
		.dbg_name = "camss_phy1_csi1phytimer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy1_csi1phytimer_clk.c),
	},
};

static struct branch_clk camss_phy2_csi2phytimer_clk = {
	.cbcr_reg = CAMSS_PHY2_CSI2PHYTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &csi2phytimer_clk_src.c,
		.dbg_name = "camss_phy2_csi2phytimer_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_phy2_csi2phytimer_clk.c),
	},
};

static struct branch_clk camss_top_ahb_clk = {
	.cbcr_reg = CAMSS_TOP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_top_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_top_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_vfe_cpp_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_cpp_clk = {
	.cbcr_reg = CAMSS_VFE_CPP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cpp_clk_src.c,
		.dbg_name = "camss_vfe_cpp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_cpp_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe0_clk = {
	.cbcr_reg = CAMSS_VFE_VFE0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe0_clk_src.c,
		.dbg_name = "camss_vfe_vfe0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe0_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe1_clk = {
	.cbcr_reg = CAMSS_VFE_VFE1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vfe1_clk_src.c,
		.dbg_name = "camss_vfe_vfe1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe1_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_ahb_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_vfe_vfe_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_ahb_clk.c),
	},
};

static struct branch_clk camss_vfe_vfe_axi_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "camss_vfe_vfe_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_axi_clk.c),
	},
};

static struct branch_clk mdss_ahb_clk = {
	.cbcr_reg = MDSS_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_ahb_clk.c),
	},
};

static struct branch_clk mdss_axi_clk = {
	.cbcr_reg = MDSS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "mdss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_axi_clk.c),
	},
};

static struct branch_clk mdss_edpaux_clk = {
	.cbcr_reg = MDSS_EDPAUX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edpaux_clk_src.c,
		.dbg_name = "mdss_edpaux_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_edpaux_clk.c),
	},
};

static struct branch_clk mdss_esc0_clk = {
	.cbcr_reg = MDSS_ESC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &esc0_clk_src.c,
		.dbg_name = "mdss_esc0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc0_clk.c),
	},
};

static struct branch_clk mdss_esc1_clk = {
	.cbcr_reg = MDSS_ESC1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &esc1_clk_src.c,
		.dbg_name = "mdss_esc1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_esc1_clk.c),
	},
};

static struct branch_clk mdss_hdmi_ahb_clk = {
	.cbcr_reg = MDSS_HDMI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_hdmi_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_ahb_clk.c),
	},
};

static struct branch_clk mdss_hdmi_clk = {
	.cbcr_reg = MDSS_HDMI_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &hdmi_clk_src.c,
		.dbg_name = "mdss_hdmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_clk.c),
	},
};

static struct branch_clk mdss_mdp_clk = {
	.cbcr_reg = MDSS_MDP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mdp_clk_src.c,
		.dbg_name = "mdss_mdp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_clk.c),
	},
};

static struct branch_clk mdss_mdp_lut_clk = {
	.cbcr_reg = MDSS_MDP_LUT_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &mdp_clk_src.c,
		.dbg_name = "mdss_mdp_lut_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_lut_clk.c),
	},
};

static struct branch_clk mdss_vsync_clk = {
	.cbcr_reg = MDSS_VSYNC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vsync_clk_src.c,
		.dbg_name = "mdss_vsync_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_vsync_clk.c),
	},
};

static struct branch_clk mmss_misc_ahb_clk = {
	.cbcr_reg = MMSS_MISC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_misc_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_misc_ahb_clk.c),
	},
};

static struct branch_clk mmss_mmssnoc_bto_ahb_clk = {
	.cbcr_reg = MMSS_MMSSNOC_BTO_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_mmssnoc_bto_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_bto_ahb_clk.c),
	},
};

static struct branch_clk mmss_mmssnoc_axi_clk = {
	.cbcr_reg = MMSS_MMSSNOC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "mmss_mmssnoc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_axi_clk.c),
	},
};

static struct branch_clk mmss_s0_axi_clk = {
	.cbcr_reg = MMSS_S0_AXI_CBCR,
	/* The bus driver needs set_rate to go through to parent */
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "mmss_s0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_s0_axi_clk.c),
		.depends = &mmss_mmssnoc_axi_clk.c,
	},
};

static struct branch_clk ocmemcx_ahb_clk = {
	.cbcr_reg = OCMEMCX_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "ocmemcx_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemcx_ahb_clk.c),
	},
};

static struct branch_clk ocmemcx_ocmemnoc_clk = {
	.cbcr_reg = OCMEMCX_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "ocmemcx_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemcx_ocmemnoc_clk.c),
	},
};

static struct branch_clk oxili_gfx3d_clk = {
	.cbcr_reg = OXILI_GFX3D_CBCR,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &oxili_gfx3d_clk_src.c,
		.dbg_name = "oxili_gfx3d_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_gfx3d_clk.c),
	},
};

static struct branch_clk oxilicx_ahb_clk = {
	.cbcr_reg = OXILICX_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "oxilicx_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxilicx_ahb_clk.c),
	},
};

static struct branch_clk venus0_ahb_clk = {
	.cbcr_reg = VENUS0_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "venus0_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_ahb_clk.c),
	},
};

static struct branch_clk venus0_axi_clk = {
	.cbcr_reg = VENUS0_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "venus0_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_axi_clk.c),
	},
};

static struct branch_clk venus0_core0_vcodec_clk = {
	.cbcr_reg = VENUS0_CORE0_VCODEC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vcodec0_clk_src.c,
		.dbg_name = "venus0_core0_vcodec_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_core0_vcodec_clk.c),
	},
};

static struct branch_clk venus0_core1_vcodec_clk = {
	.cbcr_reg = VENUS0_CORE1_VCODEC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vcodec0_clk_src.c,
		.dbg_name = "venus0_core1_vcodec_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_core1_vcodec_clk.c),
	},
};

static struct branch_clk venus0_ocmemnoc_clk = {
	.cbcr_reg = VENUS0_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "venus0_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_ocmemnoc_clk.c),
	},
};

/* Set has_sibling to 0 to allow set rate to the rcg through this clock */
static struct branch_clk venus0_vcodec0_clk = {
	.cbcr_reg = VENUS0_VCODEC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vcodec0_clk_src.c,
		.dbg_name = "venus0_vcodec0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_vcodec0_clk.c),
	},
};

static struct branch_clk vpu_ahb_clk = {
	.cbcr_reg = VPU_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_ahb_clk.c),
	},
};

static struct branch_clk vpu_axi_clk = {
	.cbcr_reg = VPU_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "vpu_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_axi_clk.c),
	},
};

static struct branch_clk vpu_bus_clk = {
	.cbcr_reg = VPU_BUS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vpu_bus_clk_src.c,
		.dbg_name = "vpu_bus_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_bus_clk.c),
	},
};

static struct branch_clk vpu_cxo_clk = {
	.cbcr_reg = VPU_CXO_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_cxo_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_cxo_clk.c),
	},
};

static struct branch_clk vpu_maple_clk = {
	.cbcr_reg = VPU_MAPLE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &maple_clk_src.c,
		.dbg_name = "vpu_maple_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_maple_clk.c),
	},
};

static struct branch_clk vpu_sleep_clk = {
	.cbcr_reg = VPU_SLEEP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_sleep_clk.c),
	},
};

static struct branch_clk vpu_vdp_clk = {
	.cbcr_reg = VPU_VDP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vdp_clk_src.c,
		.dbg_name = "vpu_vdp_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_vdp_clk.c),
	},
};

static struct clk_freq_tbl byte_freq_tbl[] = {
	{
		.src_clk = &byte_clk_src_8084.c,
		.div_src_val = BVAL(10, 8, dsipll0_byte_mm_source_val),
	},
	F_END
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_8084.c,
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP2(LOW, 112500000, NOMINAL, 187500000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct rcg_clk byte1_clk_src = {
	.cmd_rcgr_reg = BYTE1_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_8084.c,
		.dbg_name = "byte1_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP2(LOW, 112500000, NOMINAL, 187500000),
		CLK_INIT(byte1_clk_src.c),
	},
};

static struct branch_clk mdss_byte0_clk = {
	.cbcr_reg = MDSS_BYTE0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte0_clk_src.c,
		.dbg_name = "mdss_byte0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_byte0_clk.c),
	},
};

static struct branch_clk mdss_byte1_clk = {
	.cbcr_reg = MDSS_BYTE1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte1_clk_src.c,
		.dbg_name = "mdss_byte1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_byte1_clk.c),
	},
};

static struct clk_freq_tbl pixel_freq_tbl[] = {
	{
		.src_clk = &pixel_clk_src_8084.c,
		.div_src_val = BVAL(10, 8, dsipll0_pixel_mm_source_val)
				| BVAL(4, 0, 0),
	},
	F_END
};

static struct rcg_clk pclk0_clk_src = {
	.cmd_rcgr_reg = PCLK0_CMD_RCGR,
	.current_freq = pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pixel_clk_src_8084.c,
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 250000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = PCLK1_CMD_RCGR,
	.current_freq = pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pixel_clk_src_8084.c,
		.dbg_name = "pclk1_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 250000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct branch_clk avsync_pclk0_clk = {
	.cbcr_reg = AVSYNC_PCLK0_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pclk0_clk_src.c,
		.dbg_name = "avsync_pclk0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(avsync_pclk0_clk.c),
	},
};

static struct branch_clk avsync_pclk1_clk = {
	.cbcr_reg = AVSYNC_PCLK1_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pclk1_clk_src.c,
		.dbg_name = "avsync_pclk1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(avsync_pclk1_clk.c),
	},
};

/* Allow clk_set_rate on this branch clock */
static struct branch_clk mdss_pclk0_clk = {
	.cbcr_reg = MDSS_PCLK0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pclk0_clk_src.c,
		.dbg_name = "mdss_pclk0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_pclk0_clk.c),
	},
};

/* Allow clk_set_rate on this branch clock */
static struct branch_clk mdss_pclk1_clk = {
	.cbcr_reg = MDSS_PCLK1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pclk1_clk_src.c,
		.dbg_name = "mdss_pclk1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_pclk1_clk.c),
	},
};

static struct gate_clk pcie_0_phy_ldo = {
	.en_reg = PCIE_0_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_0_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(pcie_0_phy_ldo.c),
	},
};

static struct gate_clk pcie_1_phy_ldo = {
	.en_reg = PCIE_1_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_1_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(pcie_1_phy_ldo.c),
	},
};

static struct gate_clk sata_phy_ldo = {
	.en_reg = SATA_PHY_LDO_EN,
	.en_mask = BIT(0),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sata_phy_ldo",
		.ops = &clk_ops_gate,
		CLK_INIT(sata_phy_ldo.c),
	},
};

static DEFINE_CLK_VOTER(scm_ce1_clk_src, &ce1_clk_src.c, 100000000);

static DEFINE_CLK_MEASURE(l2_m_clk);
static DEFINE_CLK_MEASURE(krait0_m_clk);
static DEFINE_CLK_MEASURE(krait1_m_clk);
static DEFINE_CLK_MEASURE(krait2_m_clk);
static DEFINE_CLK_MEASURE(krait3_m_clk);

#ifdef CONFIG_DEBUG_FS

struct measure_mux_entry {
	struct clk *c;
	int base;
	u32 debug_mux;
};

enum {
	M_ACPU0 = 0,
	M_ACPU1,
	M_ACPU2,
	M_ACPU3,
	M_L2,
};

static struct measure_mux_entry measure_mux[] = {
	{&gcc_sys_noc_usb3_axi_clk.c,		GCC_BASE, 0x0001},
	{&gcc_sys_noc_usb3_sec_axi_clk.c,	GCC_BASE, 0x0007},
	{&gcc_periph_noc_usb_hsic_ahb_clk.c,	GCC_BASE, 0x0013},
	{&gcc_mmss_vpu_maple_sys_noc_axi_clk.c,	GCC_BASE, 0x002e},
	{&gcc_sys_noc_ufs_axi_clk.c,		GCC_BASE, 0x0030},
	{&gcc_usb30_master_clk.c,		GCC_BASE, 0x0050},
	{&gcc_usb30_sleep_clk.c,		GCC_BASE, 0x0051},
	{&gcc_usb30_mock_utmi_clk.c,		GCC_BASE, 0x0052},
	{&gcc_usb_hsic_ahb_clk.c,		GCC_BASE, 0x0058},
	{&gcc_usb_hsic_system_clk.c,		GCC_BASE, 0x0059},
	{&gcc_usb_hsic_clk.c,			GCC_BASE, 0x005a},
	{&gcc_usb_hsic_io_cal_clk.c,		GCC_BASE, 0x005b},
	{&gcc_usb_hsic_io_cal_sleep_clk.c,	GCC_BASE, 0x005c},
	{&gcc_usb_hsic_mock_utmi_clk.c,		GCC_BASE, 0x005d},
	{&gcc_usb_hs_system_clk.c,		GCC_BASE, 0x0060},
	{&gcc_usb_hs_ahb_clk.c,			GCC_BASE, 0x0061},
	{&gcc_usb2a_phy_sleep_clk.c,		GCC_BASE, 0x0063},
	{&gcc_usb2b_phy_sleep_clk.c,		GCC_BASE, 0x0064},
	{&gcc_sdcc1_apps_clk.c,			GCC_BASE, 0x0068},
	{&gcc_sdcc1_ahb_clk.c,			GCC_BASE, 0x0069},
	{&gcc_sdcc1_cdccal_sleep_clk.c,		GCC_BASE, 0x006a},
	{&gcc_sdcc1_cdccal_ff_clk.c,		GCC_BASE, 0x006b},
	{&gcc_sdcc2_apps_clk.c,			GCC_BASE, 0x0070},
	{&gcc_sdcc2_ahb_clk.c,			GCC_BASE, 0x0071},
	{&gcc_sdcc3_apps_clk.c,			GCC_BASE, 0x0078},
	{&gcc_sdcc3_ahb_clk.c,			GCC_BASE, 0x0079},
	{&gcc_sdcc4_apps_clk.c,			GCC_BASE, 0x0080},
	{&gcc_sdcc4_ahb_clk.c,			GCC_BASE, 0x0081},
	{&gcc_blsp1_ahb_clk.c,			GCC_BASE, 0x0088},
	{&gcc_blsp1_qup1_spi_apps_clk.c,	GCC_BASE, 0x008a},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,	GCC_BASE, 0x008b},
	{&gcc_blsp1_uart1_apps_clk.c,		GCC_BASE, 0x008c},
	{&gcc_blsp1_qup2_spi_apps_clk.c,	GCC_BASE, 0x008e},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,	GCC_BASE, 0x0090},
	{&gcc_blsp1_uart2_apps_clk.c,		GCC_BASE, 0x0091},
	{&gcc_blsp1_qup3_spi_apps_clk.c,	GCC_BASE, 0x0093},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,	GCC_BASE, 0x0094},
	{&gcc_blsp1_uart3_apps_clk.c,		GCC_BASE, 0x0095},
	{&gcc_blsp1_qup4_spi_apps_clk.c,	GCC_BASE, 0x0098},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,	GCC_BASE, 0x0099},
	{&gcc_blsp1_uart4_apps_clk.c,		GCC_BASE, 0x009a},
	{&gcc_blsp1_qup5_spi_apps_clk.c,	GCC_BASE, 0x009c},
	{&gcc_blsp1_qup5_i2c_apps_clk.c,	GCC_BASE, 0x009d},
	{&gcc_blsp1_uart5_apps_clk.c,		GCC_BASE, 0x009e},
	{&gcc_blsp1_qup6_spi_apps_clk.c,	GCC_BASE, 0x00a1},
	{&gcc_blsp1_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00a2},
	{&gcc_blsp1_uart6_apps_clk.c,		GCC_BASE, 0x00a3},
	{&gcc_blsp2_ahb_clk.c,			GCC_BASE, 0x00a8},
	{&gcc_blsp2_qup1_spi_apps_clk.c,	GCC_BASE, 0x00aa},
	{&gcc_blsp2_qup1_i2c_apps_clk.c,	GCC_BASE, 0x00ab},
	{&gcc_blsp2_uart1_apps_clk.c,		GCC_BASE, 0x00ac},
	{&gcc_blsp2_qup2_spi_apps_clk.c,	GCC_BASE, 0x00ae},
	{&gcc_blsp2_qup2_i2c_apps_clk.c,	GCC_BASE, 0x00b0},
	{&gcc_blsp2_uart2_apps_clk.c,		GCC_BASE, 0x00b1},
	{&gcc_blsp2_qup3_spi_apps_clk.c,	GCC_BASE, 0x00b3},
	{&gcc_blsp2_qup3_i2c_apps_clk.c,	GCC_BASE, 0x00b4},
	{&gcc_blsp2_uart3_apps_clk.c,		GCC_BASE, 0x00b5},
	{&gcc_blsp2_qup4_spi_apps_clk.c,	GCC_BASE, 0x00b8},
	{&gcc_blsp2_qup4_i2c_apps_clk.c,	GCC_BASE, 0x00b9},
	{&gcc_blsp2_uart4_apps_clk.c,		GCC_BASE, 0x00ba},
	{&gcc_blsp2_qup5_spi_apps_clk.c,	GCC_BASE, 0x00bc},
	{&gcc_blsp2_qup5_i2c_apps_clk.c,	GCC_BASE, 0x00bd},
	{&gcc_blsp2_uart5_apps_clk.c,		GCC_BASE, 0x00be},
	{&gcc_blsp2_qup6_spi_apps_clk.c,	GCC_BASE, 0x00c1},
	{&gcc_blsp2_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00c2},
	{&gcc_blsp2_uart6_apps_clk.c,		GCC_BASE, 0x00c3},
	{&gcc_pdm_ahb_clk.c,			GCC_BASE, 0x00d0},
	{&gcc_pdm2_clk.c,			GCC_BASE, 0x00d2},
	{&gcc_prng_ahb_clk.c,			GCC_BASE, 0x00d8},
	{&gcc_bam_dma_ahb_clk.c,		GCC_BASE, 0x00e0},
	{&gcc_tsif_ahb_clk.c,			GCC_BASE, 0x00e8},
	{&gcc_tsif_ref_clk.c,			GCC_BASE, 0x00e9},
	{&gcc_boot_rom_ahb_clk.c,		GCC_BASE, 0x00f8},
	{&gcc_ce1_clk.c,			GCC_BASE, 0x0138},
	{&gcc_ce1_axi_clk.c,			GCC_BASE, 0x0139},
	{&gcc_ce1_ahb_clk.c,			GCC_BASE, 0x013a},
	{&gcc_ce2_clk.c,			GCC_BASE, 0x0140},
	{&gcc_ce2_axi_clk.c,			GCC_BASE, 0x0141},
	{&gcc_ce2_ahb_clk.c,			GCC_BASE, 0x0142},
	{&gcc_lpass_q6_axi_clk.c,		GCC_BASE, 0x0160},
	{&gcc_lpass_mport_axi_clk.c,		GCC_BASE, 0x0162},
	{&gcc_lpass_sway_clk.c,			GCC_BASE, 0x0163},
	{&gcc_copss_smmu_axi_clk.c,		GCC_BASE, 0x01e8},
	{&gcc_copss_smmu_ahb_clk.c,		GCC_BASE, 0x01e9},
	{&gcc_spss_ahb_clk.c,			GCC_BASE, 0x01f0},
	{&gcc_pcie_0_slv_axi_clk.c,		GCC_BASE, 0x01f8},
	{&gcc_pcie_0_mstr_axi_clk.c,		GCC_BASE, 0x01f9},
	{&gcc_pcie_0_cfg_ahb_clk.c,		GCC_BASE, 0x01fa},
	{&gcc_pcie_0_aux_clk.c,			GCC_BASE, 0x01fb},
	{&gcc_pcie_0_pipe_clk.c,		GCC_BASE, 0x01fc},
	{&gcc_pcie_1_slv_axi_clk.c,		GCC_BASE, 0x0200},
	{&gcc_pcie_1_mstr_axi_clk.c,		GCC_BASE, 0x0201},
	{&gcc_pcie_1_cfg_ahb_clk.c,		GCC_BASE, 0x0202},
	{&gcc_pcie_1_aux_clk.c,			GCC_BASE, 0x0203},
	{&gcc_pcie_1_pipe_clk.c,		GCC_BASE, 0x0204},
	{&gcc_usb30_sec_master_clk.c,		GCC_BASE, 0x0208},
	{&gcc_usb30_sec_sleep_clk.c,		GCC_BASE, 0x0209},
	{&gcc_usb30_sec_mock_utmi_clk.c,	GCC_BASE, 0x020a},
	{&gcc_sata_axi_clk.c,			GCC_BASE, 0x0218},
	{&gcc_sata_cfg_ahb_clk.c,		GCC_BASE, 0x0219},
	{&gcc_sata_rx_oob_clk.c,		GCC_BASE, 0x021a},
	{&gcc_sata_pmalive_clk.c,		GCC_BASE, 0x021b},
	{&gcc_sata_asic0_clk.c,			GCC_BASE, 0x021c},
	{&gcc_sata_rx_clk.c,			GCC_BASE, 0x021d},
	{&gcc_ce3_clk.c,			GCC_BASE, 0x0228},
	{&gcc_ce3_axi_clk.c,			GCC_BASE, 0x0229},
	{&gcc_ce3_ahb_clk.c,			GCC_BASE, 0x022a},
	{&gcc_ufs_axi_clk.c,			GCC_BASE, 0x0230},
	{&gcc_ufs_ahb_clk.c,			GCC_BASE, 0x0231},
	{&gcc_ufs_tx_cfg_clk.c,			GCC_BASE, 0x0232},
	{&gcc_ufs_rx_cfg_clk.c,			GCC_BASE, 0x0233},
	{&gcc_ufs_tx_symbol_0_clk.c,		GCC_BASE, 0x0234},
	{&gcc_ufs_tx_symbol_1_clk.c,		GCC_BASE, 0x0235},
	{&gcc_ufs_rx_symbol_0_clk.c,		GCC_BASE, 0x0236},
	{&gcc_ufs_rx_symbol_1_clk.c,		GCC_BASE, 0x0237},
	{&cnoc_clk.c,				GCC_BASE, 0x0008},
	{&pnoc_clk.c,				GCC_BASE, 0x0010},
	{&snoc_clk.c,				GCC_BASE, 0x0000},
	{&bimc_clk.c,				GCC_BASE, 0x0155},

	{&mmssnoc_ahb_clk.c,			MMSS_BASE, 0x0001},
	{&mmss_mmssnoc_bto_ahb_clk.c,		MMSS_BASE, 0x0002},
	{&mmss_misc_ahb_clk.c,			MMSS_BASE, 0x0003},
	{&mmss_mmssnoc_axi_clk.c,		MMSS_BASE, 0x0004},
	{&mmss_s0_axi_clk.c,			MMSS_BASE, 0x0005},
	{&ocmemcx_ocmemnoc_clk.c,		MMSS_BASE, 0x0009},
	{&ocmemcx_ahb_clk.c,			MMSS_BASE, 0x000a},
	{&oxilicx_ahb_clk.c,			MMSS_BASE, 0x000c},
	{&oxili_gfx3d_clk.c,			MMSS_BASE, 0x000d},
	{&venus0_vcodec0_clk.c,			MMSS_BASE, 0x000e},
	{&venus0_axi_clk.c,			MMSS_BASE, 0x000f},
	{&venus0_ocmemnoc_clk.c,		MMSS_BASE, 0x0010},
	{&venus0_ahb_clk.c,			MMSS_BASE, 0x0011},
	{&mdss_mdp_clk.c,			MMSS_BASE, 0x0014},
	{&mdss_mdp_lut_clk.c,			MMSS_BASE, 0x0015},
	{&mdss_pclk0_clk.c,			MMSS_BASE, 0x0016},
	{&mdss_pclk1_clk.c,			MMSS_BASE, 0x0017},
	{&mdss_edpaux_clk.c,			MMSS_BASE, 0x001b},
	{&mdss_vsync_clk.c,			MMSS_BASE, 0x001c},
	{&mdss_hdmi_clk.c,			MMSS_BASE, 0x001d},
	{&mdss_byte0_clk.c,			MMSS_BASE, 0x001e},
	{&mdss_byte1_clk.c,			MMSS_BASE, 0x001f},
	{&mdss_esc0_clk.c,			MMSS_BASE, 0x0020},
	{&mdss_esc1_clk.c,			MMSS_BASE, 0x0021},
	{&mdss_ahb_clk.c,			MMSS_BASE, 0x0022},
	{&mdss_hdmi_ahb_clk.c,			MMSS_BASE, 0x0023},
	{&mdss_axi_clk.c,			MMSS_BASE, 0x0024},
	{&camss_top_ahb_clk.c,			MMSS_BASE, 0x0025},
	{&camss_micro_ahb_clk.c,		MMSS_BASE, 0x0026},
	{&camss_gp0_clk.c,			MMSS_BASE, 0x0027},
	{&camss_gp1_clk.c,			MMSS_BASE, 0x0028},
	{&camss_mclk0_clk.c,			MMSS_BASE, 0x0029},
	{&camss_mclk1_clk.c,			MMSS_BASE, 0x002a},
	{&camss_mclk2_clk.c,			MMSS_BASE, 0x002b},
	{&camss_mclk3_clk.c,			MMSS_BASE, 0x002c},
	{&camss_cci_cci_clk.c,			MMSS_BASE, 0x002d},
	{&camss_cci_cci_ahb_clk.c,		MMSS_BASE, 0x002e},
	{&camss_phy0_csi0phytimer_clk.c,	MMSS_BASE, 0x002f},
	{&camss_phy1_csi1phytimer_clk.c,	MMSS_BASE, 0x0030},
	{&camss_phy2_csi2phytimer_clk.c,	MMSS_BASE, 0x0031},
	{&camss_jpeg_jpeg0_clk.c,		MMSS_BASE, 0x0032},
	{&camss_jpeg_jpeg1_clk.c,		MMSS_BASE, 0x0033},
	{&camss_jpeg_jpeg2_clk.c,		MMSS_BASE, 0x0034},
	{&camss_jpeg_jpeg_ahb_clk.c,		MMSS_BASE, 0x0035},
	{&camss_jpeg_jpeg_axi_clk.c,		MMSS_BASE, 0x0036},
	{&camss_vfe_vfe0_clk.c,			MMSS_BASE, 0x0038},
	{&camss_vfe_vfe1_clk.c,			MMSS_BASE, 0x0039},
	{&camss_vfe_cpp_clk.c,			MMSS_BASE, 0x003a},
	{&camss_vfe_cpp_ahb_clk.c,		MMSS_BASE, 0x003b},
	{&camss_vfe_vfe_ahb_clk.c,		MMSS_BASE, 0x003c},
	{&camss_vfe_vfe_axi_clk.c,		MMSS_BASE, 0x003d},
	{&camss_csi_vfe0_clk.c,			MMSS_BASE, 0x003f},
	{&camss_csi_vfe1_clk.c,			MMSS_BASE, 0x0040},
	{&camss_csi0_clk.c,			MMSS_BASE, 0x0041},
	{&camss_csi0_ahb_clk.c,			MMSS_BASE, 0x0042},
	{&camss_csi0phy_clk.c,			MMSS_BASE, 0x0043},
	{&camss_csi0rdi_clk.c,			MMSS_BASE, 0x0044},
	{&camss_csi0pix_clk.c,			MMSS_BASE, 0x0045},
	{&camss_csi1_clk.c,			MMSS_BASE, 0x0046},
	{&camss_csi1_ahb_clk.c,			MMSS_BASE, 0x0047},
	{&camss_csi1phy_clk.c,			MMSS_BASE, 0x0048},
	{&camss_csi1rdi_clk.c,			MMSS_BASE, 0x0049},
	{&camss_csi1pix_clk.c,			MMSS_BASE, 0x004a},
	{&camss_csi2_clk.c,			MMSS_BASE, 0x004b},
	{&camss_csi2_ahb_clk.c,			MMSS_BASE, 0x004c},
	{&camss_csi2phy_clk.c,			MMSS_BASE, 0x004d},
	{&camss_csi2rdi_clk.c,			MMSS_BASE, 0x004e},
	{&camss_csi2pix_clk.c,			MMSS_BASE, 0x004f},
	{&camss_csi3_clk.c,			MMSS_BASE, 0x0050},
	{&camss_csi3_ahb_clk.c,			MMSS_BASE, 0x0051},
	{&camss_csi3phy_clk.c,			MMSS_BASE, 0x0052},
	{&camss_csi3rdi_clk.c,			MMSS_BASE, 0x0053},
	{&camss_csi3pix_clk.c,			MMSS_BASE, 0x0054},
	{&camss_ispif_ahb_clk.c,		MMSS_BASE, 0x0055},
	{&avsync_ahb_clk.c,			MMSS_BASE, 0x0065},
	{&vpu_vdp_clk.c,			MMSS_BASE, 0x006f},
	{&vpu_maple_clk.c,			MMSS_BASE, 0x0070},
	{&vpu_bus_clk.c,			MMSS_BASE, 0x0071},
	{&vpu_ahb_clk.c,			MMSS_BASE, 0x0072},
	{&vpu_axi_clk.c,			MMSS_BASE, 0x0073},
	{&vpu_cxo_clk.c,			MMSS_BASE, 0x0074},
	{&vpu_sleep_clk.c,			MMSS_BASE, 0x0075},
	{&avsync_vp_clk.c,			MMSS_BASE, 0x0076},
	{&camss_ahb_clk.c,			MMSS_BASE, 0x0078},
	{&venus0_core0_vcodec_clk.c,		MMSS_BASE, 0x0079},
	{&venus0_core1_vcodec_clk.c,		MMSS_BASE, 0x007a},

	{&krait0_clk.c,				APCS_BASE, M_ACPU0},
	{&krait1_clk.c,				APCS_BASE, M_ACPU1},
	{&krait2_clk.c,				APCS_BASE, M_ACPU2},
	{&krait3_clk.c,				APCS_BASE, M_ACPU3},
	{&l2_clk.c,				APCS_BASE, M_L2},

	{&dummy_clk,				N_BASES, 0x0000},
};

/* TODO: Need to consider the new mux selection for pll test de */
static int measure_clk_set_parent(struct clk *c, struct clk *parent)
{
	struct measure_clk *clk = to_measure_clk(c);
	unsigned long flags;
	u32 regval, clk_sel, i;

	if (!parent)
		return -EINVAL;

	for (i = 0; i < (ARRAY_SIZE(measure_mux) - 1); i++)
		if (measure_mux[i].c == parent)
			break;

	if (measure_mux[i].c == &dummy_clk)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	/*
	 * Program the test vector, measurement period (sample_ticks)
	 * and scaling multiplier.
	 */
	clk->sample_ticks = 0x10000;
	clk->multiplier = 1;

	switch (measure_mux[i].base) {

	case GCC_BASE:
		writel_relaxed(0, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));
		clk_sel = measure_mux[i].debug_mux;
		break;

	case MMSS_BASE:
		writel_relaxed(0, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
		clk_sel = 0x02C;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));

		/* Activate debug clock output */
		regval |= BIT(16);
		writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL));
		break;

	case LPASS_BASE:
		writel_relaxed(0, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL));
		clk_sel = 0x161;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL));

		/* Activate debug clock output */

		regval |= BIT(20);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL));
		break;

	case APCS_BASE:
		clk->multiplier = 4;
		clk_sel = 0x16A;

		if (measure_mux[i].debug_mux == M_L2)
			regval = BIT(12);
		else
			regval = measure_mux[i].debug_mux << 8;

		writel_relaxed(BIT(0), APCS_REG_BASE(L2_CBCR));
		writel_relaxed(regval, APCS_REG_BASE(GLB_CLK_DIAG));
		break;

	default:
		return -EINVAL;
	}

	/* Set debug mux clock index */
	regval = BVAL(9, 0, clk_sel);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/* Activate debug clock output */
	regval |= BIT(16);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL));

	/* Make sure test vector is set before starting measurements. */
	mb();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL));

	/* Wait for timer to become ready. */
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
			BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL));
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
			BIT(25)) == 0)
		cpu_relax();

	/* Return measured ticks. */
	return readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS)) &
				BM(24, 0);
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long measure_clk_get_rate(struct clk *c)
{
	unsigned long flags;
	u32 gcc_xo4_reg_backup;
	u64 raw_count_short, raw_count_full;
	struct measure_clk *clk = to_measure_clk(c);
	unsigned ret;

	ret = clk_prepare_enable(&xo_clk_src.c);
	if (ret) {
		pr_warning("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	gcc_xo4_reg_backup = readl_relaxed(GCC_REG_BASE(GCC_XO_DIV4_CBCR));
	writel_relaxed(0x1, GCC_REG_BASE(GCC_XO_DIV4_CBCR));

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(0x1000);
	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(clk->sample_ticks);

	writel_relaxed(gcc_xo4_reg_backup, GCC_REG_BASE(GCC_XO_DIV4_CBCR));

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short) {
		ret = 0;
	} else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((clk->sample_ticks * 10) + 35));
		ret = (raw_count_full * clk->multiplier);
	}

	/*TODO: confirm if this value is correct. */
	writel_relaxed(0x51A00, GCC_REG_BASE(GCC_PLLTEST_PAD_CFG));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&xo_clk_src.c);

	return ret;
}
#else /* !CONFIG_DEBUG_FS */
static int measure_clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -EINVAL;
}

static unsigned long measure_clk_get_rate(struct clk *clk)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static struct clk_ops clk_ops_measure = {
	.set_parent = measure_clk_set_parent,
	.get_rate = measure_clk_get_rate,
};

static struct measure_clk measure_clk = {
	.c = {
		.dbg_name = "measure_clk",
		.ops = &clk_ops_measure,
		CLK_INIT(measure_clk.c),
	},
	.multiplier = 1,
};


static struct clk_lookup apq_clocks_8084_rumi[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	SDC2_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("iface_clk",	SDC2_P_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("xo",   NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("core_clk",   NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("iface_clk",  NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("sleep_clk",  NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("sleep_a_clk",   NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("utmi_clk",   NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("ref_clk",    NULL, "f9200000.qcom,ssusb", OFF),
	CLK_DUMMY("iface_clk",	gcc_blsp1_ahb_clk.c,	"f9925000.i2c", OFF),
	CLK_DUMMY("core_clk",	gcc_blsp1_qup3_i2c_apps_clk.c,	"f9925000.i2c",
									OFF),
	CLK_DUMMY("mem_iface_clk", gcc_sys_noc_usb3_axi_clk.c,
						"f9304000.qcom,usbbam", OFF),
	CLK_DUMMY("mem_clk",	gcc_usb30_master_clk.c,
						"f9304000.qcom,usbbam", OFF),
	CLK_DUMMY("mem_iface_clk",	gcc_mmss_bimc_gfx_clk.c,
				     "fdb00000.qcom,kgsl-3d0", OFF),
	CLK_DUMMY("iface_clk", gcc_prng_ahb_clk.c,
					"f9bff000.qcom,msm-rng", OFF),
	CLK_DUMMY("iface_clk", mdss_ahb_clk.c, "fd900000.qcom,mdss_mdp", OFF),
	CLK_DUMMY("bus_clk", mdss_axi_clk.c, "fd900000.qcom,mdss_mdp", OFF),
	CLK_DUMMY("core_clk_src", mdp_clk_src.c, "fd900000.qcom,mdss_mdp", OFF),
	CLK_DUMMY("byte_clk", mdss_byte0_clk.c, "fd922800.qcom,mdss_dsi", OFF),
	CLK_DUMMY("byte_clk", mdss_byte1_clk.c, "fd922e00.qcom,mdss_dsi", OFF),
	CLK_DUMMY("core_clk", mdss_esc0_clk.c, "fd922800.qcom,mdss_dsi", OFF),
	CLK_DUMMY("core_clk", mdss_esc1_clk.c, "fd922e00.qcom,mdss_dsi", OFF),
	CLK_DUMMY("iface_clk", mdss_ahb_clk.c, "fd922800.qcom,mdss_dsi", OFF),
	CLK_DUMMY("iface_clk", mdss_ahb_clk.c, "fd922e00.qcom,mdss_dsi", OFF),
	CLK_DUMMY("bus_clk", mdss_axi_clk.c, "fd922800.qcom,mdss_dsi", OFF),
	CLK_DUMMY("bus_clk", mdss_axi_clk.c, "fd922e00.qcom,mdss_dsi", OFF),
	CLK_DUMMY("pixel_clk", mdss_pclk0_clk.c, "fd922800.qcom,mdss_dsi", OFF),
	CLK_DUMMY("pixel_clk", mdss_pclk1_clk.c, "fd922e00.qcom,mdss_dsi", OFF),
	CLK_DUMMY("core_clk", mdss_mdp_clk.c, "fd900000.qcom,mdss_mdp", OFF),
	CLK_DUMMY("lut_clk", mdss_mdp_lut_clk.c, "fd900000.qcom,mdss_mdp", OFF),
	CLK_DUMMY("vsync_clk", mdss_vsync_clk.c, "fd900000.qcom,mdss_mdp", OFF),
	CLK_DUMMY("core_clk",  ocmemgx_core_clk.c, "fdd00000.qcom,ocmem", OFF),
	CLK_DUMMY("iface_clk",	ocmemcx_ocmemnoc_clk.c,
						"fdd00000.qcom,ocmem", OFF),
	CLK_DUMMY("core_clk",	oxili_gfx3d_clk.c,
				     "fdb00000.qcom,kgsl-3d0", OFF),
	CLK_DUMMY("iface_clk",	oxilicx_ahb_clk.c,
				     "fdb00000.qcom,kgsl-3d0", OFF),
	CLK_DUMMY("iface_clk", NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fda44000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fda44000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fda44000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fd928000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fd928000.qcom,iommu", oFF),
	CLK_DUMMY("core_clk", NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "f9bc4000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "f9bc4000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdee4000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fdee4000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fe054000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fe054000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fe064000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fe064000.qcom,iommu", OFF),
};

static struct clk_lookup apq_clocks_8084[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991e000.serial", OFF),
	CLK_DUMMY("mem_iface_clk",	gcc_mmss_bimc_gfx_clk.c,
				     "fdb00000.qcom,kgsl-3d0", OFF),
	CLK_DUMMY("iface_clk", lcc_q6_smmu_cfg_clk.c, "fe054000.qcom,iommu",
									OFF),
	CLK_DUMMY("iface_clk", lcc_core_smmu_cfg_clk.c, "fe064000.qcom,iommu",
									OFF),

	CLK_LOOKUP("xo",  cxo_pil_lpass_clk.c,      "fe200000.qcom,lpass"),
	CLK_LOOKUP("xo",  cxo_dwc3_clk.c,           "f9200000.qcom,ssusb"),
	CLK_LOOKUP("xo",  cxo_lpm_clk.c,            "fc4281d0.qcom,mpm"),

	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),

	/* RPM clocks */
	CLK_LOOKUP("bus_clk", snoc_clk.c, ""),
	CLK_LOOKUP("bus_clk", pnoc_clk.c, ""),
	CLK_LOOKUP("bus_clk", cnoc_clk.c, ""),
	CLK_LOOKUP("mem_clk", bimc_clk.c, ""),
	CLK_LOOKUP("mem_clk", ocmemgx_clk.c, ""),
	CLK_LOOKUP("bus_clk", snoc_a_clk.c, ""),
	CLK_LOOKUP("bus_clk", pnoc_a_clk.c, ""),
	CLK_LOOKUP("bus_clk", cnoc_a_clk.c, ""),
	CLK_LOOKUP("mem_clk", bimc_a_clk.c, ""),
	CLK_LOOKUP("mem_clk", ocmemgx_a_clk.c, ""),
	CLK_LOOKUP("xo_clk", xo_clk_src.c, ""),
	CLK_LOOKUP("hfpll_src", xo_a_clk_src.c, "f9016000.qcom,clock-krait"),
	CLK_LOOKUP("bus_clk", mmssnoc_ahb_clk.c, ""),
	CLK_LOOKUP("core_clk", gfx3d_clk_src.c, ""),
	CLK_LOOKUP("core_clk", gfx3d_a_clk_src.c, ""),
	CLK_LOOKUP("core_clk", qdss_clk.c, ""),

	/* PLL */
	CLK_LOOKUP("gpll0", gpll0_clk_src.c, ""),
	CLK_LOOKUP("aux_clk", gpll0_ao_clk_src.c, "f9016000.qcom,clock-krait"),

	/* Voter clocks */
	CLK_LOOKUP("bus_clk",	cnoc_msmbus_clk.c,	"msm_config_noc"),
	CLK_LOOKUP("bus_a_clk",	cnoc_msmbus_a_clk.c,	"msm_config_noc"),
	CLK_LOOKUP("bus_clk",	snoc_msmbus_clk.c,	"msm_sys_noc"),
	CLK_LOOKUP("bus_a_clk",	snoc_msmbus_a_clk.c,	"msm_sys_noc"),
	CLK_LOOKUP("bus_clk",	pnoc_msmbus_clk.c,	"msm_periph_noc"),
	CLK_LOOKUP("bus_a_clk",	pnoc_msmbus_a_clk.c,	"msm_periph_noc"),
	CLK_LOOKUP("mem_clk",	bimc_msmbus_clk.c,	"msm_bimc"),
	CLK_LOOKUP("mem_a_clk",	bimc_msmbus_a_clk.c,	"msm_bimc"),
	CLK_LOOKUP("mem_clk",	bimc_acpu_a_clk.c,	""),
	CLK_LOOKUP("ocmem_clk",	ocmemgx_msmbus_clk.c,	  "msm_bus"),
	CLK_LOOKUP("ocmem_a_clk", ocmemgx_msmbus_a_clk.c, "msm_bus"),
	CLK_LOOKUP("bus_clk",	mmss_s0_axi_clk.c,	"msm_mmss_noc"),
	CLK_LOOKUP("bus_a_clk",	mmss_s0_axi_clk.c,	"msm_mmss_noc"),
	CLK_LOOKUP("dfab_clk", pnoc_sps_clk.c, "msm_sps"),
	CLK_LOOKUP("bus_clk", pnoc_keepalive_a_clk.c, ""),

	/* RCG source clocks */
	CLK_LOOKUP("",	usb30_master_clk_src.c,	""),
	CLK_LOOKUP("",	usb30_sec_master_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_ahb_clk_src.c,	""),
	CLK_LOOKUP("",	sata_asic0_clk_src.c,	""),
	CLK_LOOKUP("",	sata_pmalive_clk_src.c,	""),
	CLK_LOOKUP("",	sata_rx_clk_src.c,	""),
	CLK_LOOKUP("",	sata_rx_oob_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc1_apps_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc2_apps_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc3_apps_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc4_apps_clk_src.c,	""),
	CLK_LOOKUP("",	tsif_ref_clk_src.c,	""),
	CLK_LOOKUP("",	usb30_mock_utmi_clk_src.c,	""),
	CLK_LOOKUP("",	usb30_sec_mock_utmi_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hs_system_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_io_cal_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_mock_utmi_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_system_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_0_aux_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_0_pipe_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_1_aux_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_1_pipe_clk_src.c,	""),

	CLK_LOOKUP("dma_bam_pclk", gcc_bam_dma_ahb_clk.c, "msm_sps"),

	/* BLSP1 clocks */
	CLK_LOOKUP("iface_clk",	gcc_blsp1_ahb_clk.c,	"f991e000.serial"),
	CLK_LOOKUP("iface_clk",	gcc_blsp1_ahb_clk.c,	"f991f000.serial"),
	CLK_LOOKUP("iface_clk",	gcc_blsp1_ahb_clk.c,	"f9925000.i2c"),
	CLK_LOOKUP("",	gcc_blsp1_qup1_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup1_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup2_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup2_spi_apps_clk.c,	""),
	CLK_LOOKUP("core_clk",	gcc_blsp1_qup3_i2c_apps_clk.c, "f9925000.i2c"),
	CLK_LOOKUP("",	gcc_blsp1_qup3_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup4_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup4_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup5_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup5_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup6_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup6_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart1_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart2_apps_clk.c,	""),
	CLK_LOOKUP("core_clk",	gcc_blsp1_uart3_apps_clk.c, "f991f000.serial"),
	CLK_LOOKUP("",	gcc_blsp1_uart4_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart5_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart6_apps_clk.c,	""),

	/* BLSP2  clocks */
	CLK_LOOKUP("iface_clk",	gcc_blsp2_ahb_clk.c,	"f995e000.serial"),
	CLK_LOOKUP("",	gcc_blsp2_qup1_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup1_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup2_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup2_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup3_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup3_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup4_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup4_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup5_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup5_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup6_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_qup6_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart1_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart2_apps_clk.c, "f995e000.serial"),
	CLK_LOOKUP("",	gcc_blsp2_uart3_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart4_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart5_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart6_apps_clk.c,	""),

	CLK_LOOKUP("",	gcc_boot_rom_ahb_clk.c,	""),

	/* CE clocks */
	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "scm"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "scm"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "scm"),
	CLK_LOOKUP("core_clk_src", scm_ce1_clk_src.c,     "scm"),

	CLK_LOOKUP("",	gcc_ce1_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce1_axi_clk.c,	""),
	CLK_LOOKUP("",	ce1_clk_src.c,	""),
	CLK_LOOKUP("",	gcc_ce1_clk.c,	""),

	CLK_LOOKUP("",	gcc_ce2_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce2_axi_clk.c,	""),
	CLK_LOOKUP("",	ce2_clk_src.c,	""),
	CLK_LOOKUP("",	gcc_ce2_clk.c,	""),

	CLK_LOOKUP("",	gcc_ce3_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce3_axi_clk.c,	""),
	CLK_LOOKUP("",	ce3_clk_src.c,	""),
	CLK_LOOKUP("",	gcc_ce3_clk.c,	""),

	CLK_LOOKUP("",	gcc_copss_smmu_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_copss_smmu_axi_clk.c,	""),

	CLK_LOOKUP("",	gcc_gp1_clk.c,	""),
	CLK_LOOKUP("",	gcc_gp2_clk.c,	""),
	CLK_LOOKUP("",	gcc_gp3_clk.c,	""),

	CLK_LOOKUP("",	gcc_lpass_mport_axi_clk.c,	""),
	CLK_LOOKUP("bus_clk",	gcc_lpass_q6_axi_clk.c,	"fe200000.qcom,lpass"),
	CLK_LOOKUP("core_clk",	dummy_clk,	"fe200000.qcom,lpass"),
	CLK_LOOKUP("iface_clk",	dummy_clk,	"fe200000.qcom,lpass"),
	CLK_LOOKUP("reg_clk",	dummy_clk,	"fe200000.qcom,lpass"),
	CLK_LOOKUP("",	gcc_lpass_sway_clk.c,	""),

	CLK_LOOKUP("",	gcc_pdm2_clk.c,	""),
	CLK_LOOKUP("",	gcc_pdm_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_periph_noc_usb_hsic_ahb_clk.c,	""),
	CLK_LOOKUP("iface_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng"),

	/* SATA clocks */
	CLK_LOOKUP("",	gcc_sata_asic0_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_pmalive_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_rx_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_rx_oob_clk.c,	""),

	/* SDCC clocks */
	CLK_LOOKUP("iface_clk",	gcc_sdcc1_ahb_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",	gcc_sdcc1_apps_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("cal_clk",	gcc_sdcc1_cdccal_ff_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("sleep_clk",	gcc_sdcc1_cdccal_sleep_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",	gcc_sdcc2_ahb_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",	gcc_sdcc2_apps_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("",	gcc_sdcc3_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sdcc3_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_sdcc4_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sdcc4_apps_clk.c,	""),

	CLK_LOOKUP("",	gcc_spss_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sys_noc_usb3_axi_clk.c,	""),
	CLK_LOOKUP("mem_iface_clk",	gcc_sys_noc_usb3_axi_clk.c,
						"f9304000.qcom,usbbam"),
	CLK_LOOKUP("",	gcc_sys_noc_usb3_sec_axi_clk.c,	""),

	CLK_LOOKUP("",	gcc_tsif_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_tsif_ref_clk.c,	""),

	/* UFS clocks */
	CLK_LOOKUP("ref_clk", rf_clk1.c,                 "fc594000.ufshc"),
	CLK_LOOKUP("bus_clk", gcc_sys_noc_ufs_axi_clk.c, "fc594000.ufshc"),
	CLK_LOOKUP("iface_clk", gcc_ufs_ahb_clk.c,       "fc594000.ufshc"),
	CLK_LOOKUP("core_clk", gcc_ufs_axi_clk.c,        "fc594000.ufshc"),
	CLK_LOOKUP("core_clk_src", ufs_axi_clk_src.c, "fc594000.ufshc"),
	CLK_LOOKUP("rx_lane0_sync_clk", gcc_ufs_rx_symbol_0_clk.c,
							"fc594000.ufshc"),
	CLK_LOOKUP("rx_lane1_sync_clk", gcc_ufs_rx_symbol_1_clk.c,
							"fc594000.ufshc"),
	CLK_LOOKUP("tx_lane0_sync_clk", gcc_ufs_tx_symbol_0_clk.c,
							"fc594000.ufshc"),
	CLK_LOOKUP("tx_lane1_sync_clk", gcc_ufs_tx_symbol_1_clk.c,
							"fc594000.ufshc"),
	CLK_LOOKUP("tx_iface_clk", gcc_ufs_tx_cfg_clk.c, "fc597000.ufsphy"),
	CLK_LOOKUP("rx_iface_clk", gcc_ufs_rx_cfg_clk.c, "fc597000.ufsphy"),

	/* USB clocks */
	CLK_LOOKUP("ref_clk", diff_clk1.c, "f9200000.qcom,ssusb"),
	CLK_LOOKUP("ref_clk", diff_clk1.c, "f9400000.qcom,ssusb"),
	CLK_LOOKUP("xo", cxo_dwc3_clk.c, "f9200000.qcom,ssusb"),
	CLK_LOOKUP("core_clk", gcc_usb30_master_clk.c, "f9200000.qcom,ssusb"),
	CLK_LOOKUP("iface_clk", gcc_sys_noc_usb3_axi_clk.c,
			"f9200000.qcom,ssusb"),
	CLK_LOOKUP("iface_clk", gcc_sys_noc_usb3_axi_clk.c, "msm_usb3"),
	CLK_LOOKUP("sleep_clk", gcc_usb30_sleep_clk.c, "f9200000.qcom,ssusb"),
	CLK_LOOKUP("sleep_a_clk", gcc_usb2a_phy_sleep_clk.c,
			"f9200000.qcom,ssusb"),
	CLK_LOOKUP("utmi_clk",   gcc_usb30_mock_utmi_clk.c,
			"f9200000.qcom,ssusb"),
	CLK_LOOKUP("",	gcc_usb2b_phy_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_master_clk.c,	""),
	CLK_LOOKUP("mem_clk",	gcc_usb30_master_clk.c,	"f9304000.qcom,usbbam"),
	CLK_LOOKUP("",	gcc_usb30_mock_utmi_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_sec_master_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_sec_mock_utmi_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_sec_sleep_clk.c,	""),

	CLK_LOOKUP("xo",	cxo_otg_clk.c,			"f9a55000.usb"),
	CLK_LOOKUP("iface_clk",	gcc_usb_hs_ahb_clk.c,		"f9a55000.usb"),
	CLK_LOOKUP("core_clk",	gcc_usb_hs_system_clk.c,	"f9a55000.usb"),
	CLK_LOOKUP("sleep_clk", gcc_usb2a_phy_sleep_clk.c,	"f9a55000.usb"),

	CLK_LOOKUP("core_clk", gcc_usb_hsic_ahb_clk.c,
			"f9c00000.qcom,xhci-msm-hsic"),
	CLK_LOOKUP("hsic_clk", gcc_usb_hsic_clk.c,
			"f9c00000.qcom,xhci-msm-hsic"),
	CLK_LOOKUP("cal_clk", gcc_usb_hsic_io_cal_clk.c,
			"f9c00000.qcom,xhci-msm-hsic"),
	CLK_LOOKUP("phy_sleep_clk", gcc_usb_hsic_io_cal_sleep_clk.c,
			"f9c00000.qcom,xhci-msm-hsic"),
	CLK_LOOKUP("utmi_clk", gcc_usb_hsic_mock_utmi_clk.c,
			"f9c00000.qcom,xhci-msm-hsic"),
	CLK_LOOKUP("system_clk", gcc_usb_hsic_system_clk.c,
			"f9c00000.qcom,xhci-msm-hsic"),

	CLK_LOOKUP("",	gcc_usb30_phy_com_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb3_phy_clk.c,	""),

	/* PCIE clocks */
	CLK_LOOKUP("",	gcc_pcie_0_aux_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_0_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_0_mstr_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_0_pipe_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_0_slv_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_1_aux_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_1_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_1_mstr_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_1_pipe_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_1_slv_axi_clk.c,	""),

	/* CoreSight clocks */
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc326000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc320000.tpiu"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc324000.replicator"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc325000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc323000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc321000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc322000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc345000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc355000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc36c000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc302000.stm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc34c000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc34d000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc34e000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc34f000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc311000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc312000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc313000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc314000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc315000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc316000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc317000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc318000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc340000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc341000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc342000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc343000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc344000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fd828018.hwevent"),

	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc326000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc320000.tpiu"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc324000.replicator"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc325000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc323000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc321000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc322000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc345000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc355000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc36c000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc302000.stm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34c000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34d000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34e000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc34f000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc311000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc312000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc313000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc314000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc315000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc316000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc317000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc318000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc340000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc341000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc342000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc343000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc344000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fd828018.hwevent"),

	CLK_LOOKUP("core_mmss_clk", mmss_misc_ahb_clk.c, "fd828018.hwevent"),

	/* Multimedia clocks */
	CLK_LOOKUP("",	axi_clk_src.c,	""),
	CLK_LOOKUP("",	csi0_clk_src.c,	""),
	CLK_LOOKUP("",	csi1_clk_src.c,	""),
	CLK_LOOKUP("",	csi2_clk_src.c,	""),
	CLK_LOOKUP("",	csi3_clk_src.c,	""),
	CLK_LOOKUP("",	vcodec0_clk_src.c,	""),
	CLK_LOOKUP("",	vfe0_clk_src.c,	""),
	CLK_LOOKUP("",	vfe1_clk_src.c,	""),
	CLK_LOOKUP("",	mdp_clk_src.c,	""),
	CLK_LOOKUP("",	ocmemnoc_clk_src.c,	""),
	CLK_LOOKUP("",	gfx3d_clk_src.c,	""),
	CLK_LOOKUP("",	vp_clk_src.c,	""),
	CLK_LOOKUP("",	cci_clk_src.c,	""),
	CLK_LOOKUP("",	gp0_clk_src.c,	""),
	CLK_LOOKUP("",	gp1_clk_src.c,	""),
	CLK_LOOKUP("",	jpeg0_clk_src.c,	""),
	CLK_LOOKUP("",	jpeg1_clk_src.c,	""),
	CLK_LOOKUP("",	jpeg2_clk_src.c,	""),
	CLK_LOOKUP("",	mclk0_clk_src.c,	""),
	CLK_LOOKUP("",	mclk1_clk_src.c,	""),
	CLK_LOOKUP("",	mclk2_clk_src.c,	""),
	CLK_LOOKUP("",	mclk3_clk_src.c,	""),
	CLK_LOOKUP("",	csi0phytimer_clk_src.c,	""),
	CLK_LOOKUP("",	csi1phytimer_clk_src.c,	""),
	CLK_LOOKUP("",	csi2phytimer_clk_src.c,	""),
	CLK_LOOKUP("",	cpp_clk_src.c,	""),
	CLK_LOOKUP("",	edpaux_clk_src.c,	""),
	CLK_LOOKUP("",	esc0_clk_src.c,	""),
	CLK_LOOKUP("",	esc1_clk_src.c,	""),
	CLK_LOOKUP("",	hdmi_clk_src.c,	""),
	CLK_LOOKUP("",	vsync_clk_src.c,	""),
	CLK_LOOKUP("",	maple_clk_src.c,	""),
	CLK_LOOKUP("",	vdp_clk_src.c,	""),
	CLK_LOOKUP("",	vpu_bus_clk_src.c,	""),

	CLK_LOOKUP("",	avsync_ahb_clk.c,	""),
	CLK_LOOKUP("",	avsync_vp_clk.c,	""),

	CLK_LOOKUP("",	camss_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_cci_cci_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_cci_cci_clk.c,	""),
	CLK_LOOKUP("",	camss_csi0_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_csi0_clk.c,	""),
	CLK_LOOKUP("",	camss_csi0phy_clk.c,	""),
	CLK_LOOKUP("",	camss_csi0pix_clk.c,	""),
	CLK_LOOKUP("",	camss_csi0rdi_clk.c,	""),
	CLK_LOOKUP("",	camss_csi1_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_csi1_clk.c,	""),
	CLK_LOOKUP("",	camss_csi1phy_clk.c,	""),
	CLK_LOOKUP("",	camss_csi1pix_clk.c,	""),
	CLK_LOOKUP("",	camss_csi1rdi_clk.c,	""),
	CLK_LOOKUP("",	camss_csi2_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_csi2_clk.c,	""),
	CLK_LOOKUP("",	camss_csi2phy_clk.c,	""),
	CLK_LOOKUP("",	camss_csi2pix_clk.c,	""),
	CLK_LOOKUP("",	camss_csi2rdi_clk.c,	""),
	CLK_LOOKUP("",	camss_csi3_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_csi3_clk.c,	""),
	CLK_LOOKUP("",	camss_csi3phy_clk.c,	""),
	CLK_LOOKUP("",	camss_csi3pix_clk.c,	""),
	CLK_LOOKUP("",	camss_csi3rdi_clk.c,	""),
	CLK_LOOKUP("",	camss_csi_vfe0_clk.c,	""),
	CLK_LOOKUP("",	camss_csi_vfe1_clk.c,	""),
	CLK_LOOKUP("",	camss_gp0_clk.c,	""),
	CLK_LOOKUP("",	camss_gp1_clk.c,	""),
	CLK_LOOKUP("",	camss_ispif_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg0_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg1_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg2_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg_axi_clk.c,	""),
	CLK_LOOKUP("",	camss_mclk0_clk.c,	""),
	CLK_LOOKUP("",	camss_mclk1_clk.c,	""),
	CLK_LOOKUP("",	camss_mclk2_clk.c,	""),
	CLK_LOOKUP("",	camss_mclk3_clk.c,	""),
	CLK_LOOKUP("",	camss_micro_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_phy0_csi0phytimer_clk.c,	""),
	CLK_LOOKUP("",	camss_phy1_csi1phytimer_clk.c,	""),
	CLK_LOOKUP("",	camss_phy2_csi2phytimer_clk.c,	""),
	CLK_LOOKUP("",	camss_top_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_vfe_cpp_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_vfe_cpp_clk.c,	""),
	CLK_LOOKUP("",	camss_vfe_vfe0_clk.c,	""),
	CLK_LOOKUP("",	camss_vfe_vfe1_clk.c,	""),
	CLK_LOOKUP("",	camss_vfe_vfe_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_vfe_vfe_axi_clk.c,	""),

	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("core_clk_src", mdp_clk_src.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("mdp_core_clk", mdss_mdp_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("mdp_core_clk", mdss_mdp_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("byte_clk", mdss_byte0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("byte_clk", mdss_byte1_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("pixel_clk", mdss_pclk0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("pixel_clk", mdss_pclk1_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_esc0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_esc1_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("vsync_clk", mdss_vsync_clk.c, "fd900000.qcom,mdss_mdp"),
	CLK_LOOKUP("",	mdss_ahb_clk.c,	""),
	CLK_LOOKUP("",	mdss_axi_clk.c,	""),
	CLK_LOOKUP("",	mdss_edpaux_clk.c,	""),
	CLK_LOOKUP("",	mdss_esc0_clk.c,	""),
	CLK_LOOKUP("",	mdss_esc1_clk.c,	""),
	CLK_LOOKUP("",	mdss_hdmi_ahb_clk.c,	""),
	CLK_LOOKUP("",	mdss_hdmi_clk.c,	""),
	CLK_LOOKUP("",	mdss_mdp_clk.c,	""),
	CLK_LOOKUP("",	mdss_mdp_lut_clk.c,	""),
	CLK_LOOKUP("",	mdss_vsync_clk.c,	""),

	CLK_LOOKUP("",	mmss_misc_ahb_clk.c,	""),
	CLK_LOOKUP("",	mmss_mmssnoc_axi_clk.c,	""),
	CLK_LOOKUP("",	mmss_s0_axi_clk.c,	""),

	CLK_LOOKUP("core_clk",  ocmemgx_core_clk.c, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP("iface_clk",	ocmemcx_ocmemnoc_clk.c,
						"fdd00000.qcom,ocmem"),
	CLK_LOOKUP("core_clk",	oxili_gfx3d_clk.c, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP("iface_clk",	oxilicx_ahb_clk.c, "fdb00000.qcom,kgsl-3d0"),

	CLK_LOOKUP("",	venus0_ahb_clk.c,	""),
	CLK_LOOKUP("",	venus0_axi_clk.c,	""),
	CLK_LOOKUP("",	venus0_core0_vcodec_clk.c,	""),
	CLK_LOOKUP("",	venus0_core1_vcodec_clk.c,	""),
	CLK_LOOKUP("",	venus0_ocmemnoc_clk.c,	""),
	CLK_LOOKUP("",	venus0_vcodec0_clk.c,	""),
	CLK_LOOKUP("iface_clk",	venus0_ahb_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("bus_clk",	venus0_axi_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("mem_clk",	venus0_ocmemnoc_clk.c,
						 "fdce0000.qcom,venus"),
	CLK_LOOKUP("core_clk",	venus0_vcodec0_clk.c,
						 "fdce0000.qcom,venus"),
	CLK_LOOKUP("iface_clk",	venus0_ahb_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("bus_clk",	venus0_axi_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("mem_clk",	venus0_ocmemnoc_clk.c,
						 "fdc00000.qcom,vidc"),
	CLK_LOOKUP("core_clk",	venus0_vcodec0_clk.c,
						 "fdc00000.qcom,vidc"),
	CLK_LOOKUP("core0_clk",	venus0_core0_vcodec_clk.c,
						 "fdc00000.qcom,vidc"),
	CLK_LOOKUP("core1_clk",	venus0_core1_vcodec_clk.c,
						 "fdc00000.qcom,vidc"),

	CLK_LOOKUP("iface_clk", vpu_ahb_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("bus_clk", vpu_axi_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("vdp_clk", vpu_vdp_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("vdp_bus_clk", vpu_bus_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("cxo_clk", vpu_cxo_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("core_clk", vpu_maple_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("sleep_clk", vpu_sleep_clk.c, "fde0b000.qcom,vpu"),
	CLK_LOOKUP("maple_bus_clk", gcc_mmss_vpu_maple_sys_noc_axi_clk.c,
						"fde0b000.qcom,vpu"),
	CLK_LOOKUP("prng_clk", gcc_prng_ahb_clk.c, "fde0b000.qcom,vpu"),

	/* IOMMU clocks */
	CLK_LOOKUP("iface_clk", camss_jpeg_jpeg_ahb_clk.c,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP("core_clk", camss_jpeg_jpeg_axi_clk.c,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", camss_top_ahb_clk.c,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP("alt_iface_clk", camss_ahb_clk.c,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c,
						"fda44000.qcom,iommu"),
	CLK_LOOKUP("core_clk", camss_vfe_vfe_axi_clk.c,
						"fda44000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", camss_top_ahb_clk.c,
						"fda44000.qcom,iommu"),
	CLK_LOOKUP("alt_iface_clk", camss_ahb_clk.c,
						"fda44000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c , "fd928000.qcom,iommu"),
	CLK_LOOKUP("core_clk", mdss_axi_clk.c , "fd928000.qcom,iommu"),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", oxilicx_ahb_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", venus0_ahb_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", venus0_vcodec0_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("core_clk", venus0_axi_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", gcc_copss_smmu_ahb_clk.c,
						"f9bc4000.qcom,iommu"),
	CLK_LOOKUP("core_clk", gcc_copss_smmu_axi_clk.c, "f9bc4000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", vpu_ahb_clk.c, "fdee4000.qcom,iommu"),
	CLK_LOOKUP("core_clk", vpu_axi_clk.c, "fdee4000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", vpu_bus_clk.c, "fdee4000.qcom,iommu"),
	CLK_LOOKUP("core_clk", gcc_lpass_q6_axi_clk.c, "fe054000.qcom,iommu"),
	CLK_LOOKUP("core_clk", gcc_lpass_mport_axi_clk.c,
							"fe064000.qcom,iommu"),
	CLK_LOOKUP("core_clk", vpu_vdp_clk.c, "fd8c1404.qcom,gdsc"),
	CLK_LOOKUP("maple_clk", vpu_maple_clk.c, "fd8c1404.qcom,gdsc"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("cpp_clk", camss_vfe_cpp_clk.c, "fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("core0_clk", camss_vfe_vfe0_clk.c, "fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("core1_clk", camss_vfe_vfe1_clk.c, "fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fd8c4024.qcom,gdsc"),

	/* DSI PLL clocks */
	CLK_LOOKUP("",		dsi_vco_clk_8084.c,                  ""),
	CLK_LOOKUP("",		analog_postdiv_clk_8084.c,         ""),
	CLK_LOOKUP("",		indirect_path_div2_clk_8084.c,     ""),
	CLK_LOOKUP("",		pixel_clk_src_8084.c,              ""),
	CLK_LOOKUP("",		byte_mux_8084.c,                   ""),
	CLK_LOOKUP("",		byte_clk_src_8084.c,               ""),

	/* LDO */
	CLK_LOOKUP("",		pcie_0_phy_ldo.c,               ""),
	CLK_LOOKUP("",		pcie_1_phy_ldo.c,               ""),
	CLK_LOOKUP("",		sata_phy_ldo.c,               ""),
};

static struct pll_config_regs gpll4_regs __initdata = {
	.l_reg = (void __iomem *)GPLL4_L,
	.m_reg = (void __iomem *)GPLL4_M,
	.n_reg = (void __iomem *)GPLL4_N,
	.config_reg = (void __iomem *)GPLL4_USER_CTL,
	.mode_reg = (void __iomem *)GPLL4_MODE,
	.base = &virt_bases[GCC_BASE],
};

/* PLL4 at 800 MHz, main output enabled. LJ mode. */
static struct pll_config gpll4_config __initdata = {
	.l = 0x29,
	.m = 0x2,
	.n = 0x3,
	.vco_val = 0x1,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs mmpll0_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL0_PLL_L_VAL,
	.m_reg = (void __iomem *)MMPLL0_PLL_M_VAL,
	.n_reg = (void __iomem *)MMPLL0_PLL_N_VAL,
	.config_reg = (void __iomem *)MMPLL0_PLL_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL0_PLL_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL0 at 800 MHz, main output enabled. */
static struct pll_config mmpll0_config __initdata = {
	.l = 0x29,
	.m = 0x2,
	.n = 0x3,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs mmpll1_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL1_PLL_L_VAL,
	.m_reg = (void __iomem *)MMPLL1_PLL_M_VAL,
	.n_reg = (void __iomem *)MMPLL1_PLL_N_VAL,
	.config_reg = (void __iomem *)MMPLL1_PLL_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL1_PLL_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL1 at 1167MHz, main output enabled. */
static struct pll_config mmpll1_config __initdata = {
	.l = 0x3C,
	.m = 0x19,
	.n = 0x20,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs mmpll3_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL3_PLL_L_VAL,
	.m_reg = (void __iomem *)MMPLL3_PLL_M_VAL,
	.n_reg = (void __iomem *)MMPLL3_PLL_N_VAL,
	.config_reg = (void __iomem *)MMPLL3_PLL_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL3_PLL_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL3 at 930 MHz, main output enabled. */
static struct pll_config mmpll3_config __initdata = {
	.l = 48,
	.m = 7,
	.n = 16,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static struct pll_config_regs mmpll4_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL4_PLL_L_VAL,
	.m_reg = (void __iomem *)MMPLL4_PLL_M_VAL,
	.n_reg = (void __iomem *)MMPLL4_PLL_N_VAL,
	.config_reg = (void __iomem *)MMPLL4_PLL_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL4_PLL_MODE,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL4 at 930 MHz, main output enabled. */
static struct pll_config mmpll4_config __initdata = {
	.l = 48,
	.m = 7,
	.n = 16,
	.vco_val = 0x0,
	.vco_mask = BM(21, 20),
	.pre_div_val = 0x0,
	.pre_div_mask = BM(14, 12),
	.post_div_val = 0x0,
	.post_div_mask = BM(9, 8),
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_val = BIT(0),
	.main_output_mask = BIT(0),
};

static void __init reg_init(void)
{
	u32 regval;

	configure_sr_hpm_lp_pll(&gpll4_config, &gpll4_regs, 1);

	configure_sr_hpm_lp_pll(&mmpll0_config, &mmpll0_regs, 1);
	configure_sr_hpm_lp_pll(&mmpll1_config, &mmpll1_regs, 1);
	configure_sr_hpm_lp_pll(&mmpll3_config, &mmpll3_regs, 0);
	configure_sr_hpm_lp_pll(&mmpll4_config, &mmpll4_regs, 0);

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	regval = readl_relaxed(
			GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
	writel_relaxed(regval | BIT(26) | BIT(25),
			GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
}

static void __init apq8084_clock_post_init(void)
{

	clk_set_rate(&axi_clk_src.c, 150000000);
	clk_set_rate(&ocmemnoc_clk_src.c, 320000000);

	/*
	 * Hold an active set vote at a rate of 40MHz for the MMSS NOC AHB
	 * source. Sleep set vote is 0.
	 */
	clk_set_rate(&mmssnoc_ahb_a_clk.c, 40000000);
	clk_prepare_enable(&mmssnoc_ahb_a_clk.c);

	/*
	 * Hold an active set vote for the PNOC AHB source. Sleep set vote is 0.
	 */
	clk_set_rate(&pnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pnoc_keepalive_a_clk.c);

	/*
	 * Hold an active set vote for CXO; this is because CXO is expected
	 * to remain on whenever CPUs aren't power collapsed.
	 */
	clk_prepare_enable(&xo_a_clk_src.c);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_16K

#define MMSS_CC_PHYS		0xFD8C0000
#define MMSS_CC_SIZE		SZ_256K

#define LPASS_CC_PHYS		0xFE000000
#define LPASS_CC_SIZE		SZ_256K

#define APCS_GCC_CC_PHYS	0xF9011000
#define APCS_GCC_CC_SIZE	SZ_4K

static void __init apq8084_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-8084: Unable to ioremap GCC memory!");

	virt_bases[MMSS_BASE] = ioremap(MMSS_CC_PHYS, MMSS_CC_SIZE);
	if (!virt_bases[MMSS_BASE])
		panic("clock-8084: Unable to ioremap MMSS_CC memory!");

	virt_bases[LPASS_BASE] = ioremap(LPASS_CC_PHYS, LPASS_CC_SIZE);
	if (!virt_bases[LPASS_BASE])
		panic("clock-8084: Unable to ioremap LPASS_CC memory!");

	virt_bases[APCS_BASE] = ioremap(APCS_GCC_CC_PHYS, APCS_GCC_CC_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-8084: Unable to ioremap APCS_GCC_CC memory!");

	clk_ops_local_pll.enable = sr_hpm_lp_pll_clk_enable;

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-8084: Unable to get the vdd_dig regulator!");

	enable_rpm_scaling();

	reg_init();

	/*
	 * MDSS needs the ahb clock and needs to init before we register the
	 * lookup table.
	 */
	mdss_clk_ctrl_pre_init(&mdss_ahb_clk.c);
}

static void __init apq8084_rumi_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-8084: Unable to ioremap GCC memory!");

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-8084: Unable to get the vdd_dig regulator!");
}

struct clock_init_data apq8084_rumi_clock_init_data __initdata = {
	.table = apq_clocks_8084_rumi,
	.size = ARRAY_SIZE(apq_clocks_8084_rumi),
	.pre_init = apq8084_rumi_clock_pre_init,
};

struct clock_init_data apq8084_clock_init_data __initdata = {
	.table = apq_clocks_8084,
	.size = ARRAY_SIZE(apq_clocks_8084),
	.pre_init = apq8084_clock_pre_init,
	.post_init = apq8084_clock_post_init,
};
