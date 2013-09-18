/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include "clock-mdss-8974.h"
#include "clock.h"
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

#define GPLL0_MODE_REG                 0x0000
#define GPLL0_L_REG                    0x0004
#define GPLL0_M_REG                    0x0008
#define GPLL0_N_REG                    0x000C
#define GPLL0_USER_CTL_REG             0x0010
#define GPLL0_CONFIG_CTL_REG           0x0014
#define GPLL0_TEST_CTL_REG             0x0018
#define GPLL0_STATUS_REG               0x001C

#define GPLL1_MODE_REG                 0x0040
#define GPLL1_L_REG                    0x0044
#define GPLL1_M_REG                    0x0048
#define GPLL1_N_REG                    0x004C
#define GPLL1_USER_CTL_REG             0x0050
#define GPLL1_CONFIG_CTL_REG           0x0054
#define GPLL1_TEST_CTL_REG             0x0058
#define GPLL1_STATUS_REG               0x005C

#define GPLL4_MODE_REG                 0x1DC0
#define GPLL4_L_REG                    0x1DC4
#define GPLL4_M_REG                    0x1DC8
#define GPLL4_N_REG                    0x1DCC
#define GPLL4_USER_CTL_REG             0x1DD0
#define GPLL4_CONFIG_CTL_REG           0x1DD4
#define GPLL4_TEST_CTL_REG             0x1DD8
#define GPLL4_STATUS_REG               0x1DDC

#define MMPLL0_MODE_REG                0x0000
#define MMPLL0_L_REG                   0x0004
#define MMPLL0_M_REG                   0x0008
#define MMPLL0_N_REG                   0x000C
#define MMPLL0_USER_CTL_REG            0x0010
#define MMPLL0_CONFIG_CTL_REG          0x0014
#define MMPLL0_TEST_CTL_REG            0x0018
#define MMPLL0_STATUS_REG              0x001C

#define MMPLL1_MODE_REG                0x0040
#define MMPLL1_L_REG                   0x0044
#define MMPLL1_M_REG                   0x0048
#define MMPLL1_N_REG                   0x004C
#define MMPLL1_USER_CTL_REG            0x0050
#define MMPLL1_CONFIG_CTL_REG          0x0054
#define MMPLL1_TEST_CTL_REG            0x0058
#define MMPLL1_STATUS_REG              0x005C

#define MMPLL3_MODE_REG                0x0080
#define MMPLL3_L_REG                   0x0084
#define MMPLL3_M_REG                   0x0088
#define MMPLL3_N_REG                   0x008C
#define MMPLL3_USER_CTL_REG            0x0090
#define MMPLL3_CONFIG_CTL_REG          0x0094
#define MMPLL3_TEST_CTL_REG            0x0098
#define MMPLL3_STATUS_REG              0x009C

#define LPAPLL_MODE_REG                0x0000
#define LPAPLL_L_REG                   0x0004
#define LPAPLL_M_REG                   0x0008
#define LPAPLL_N_REG                   0x000C
#define LPAPLL_USER_CTL_REG            0x0010
#define LPAPLL_CONFIG_CTL_REG          0x0014
#define LPAPLL_TEST_CTL_REG            0x0018
#define LPAPLL_STATUS_REG              0x001C

#define GCC_DEBUG_CLK_CTL_REG          0x1880
#define CLOCK_FRQ_MEASURE_CTL_REG      0x1884
#define CLOCK_FRQ_MEASURE_STATUS_REG   0x1888
#define GCC_XO_DIV4_CBCR_REG           0x10C8
#define GCC_PLLTEST_PAD_CFG_REG        0x188C
#define APCS_GPLL_ENA_VOTE_REG         0x1480
#define MMSS_PLL_VOTE_APCS_REG         0x0100
#define MMSS_DEBUG_CLK_CTL_REG         0x0900
#define LPASS_DEBUG_CLK_CTL_REG        0x29000
#define LPASS_LPA_PLL_VOTE_APPS_REG    0x2000

#define GLB_CLK_DIAG_REG               0x001C
#define L2_CBCR_REG                    0x004C

#define USB30_MASTER_CMD_RCGR          0x03D4
#define USB30_MOCK_UTMI_CMD_RCGR       0x03E8
#define USB_HSIC_SYSTEM_CMD_RCGR       0x041C
#define USB_HSIC_CMD_RCGR              0x0440
#define USB_HSIC_IO_CAL_CMD_RCGR       0x0458
#define USB_HS_SYSTEM_CMD_RCGR         0x0490
#define SYS_NOC_USB3_AXI_CBCR	       0x0108
#define USB30_SLEEP_CBCR	       0x03CC
#define USB2A_PHY_SLEEP_CBCR	       0x04AC
#define USB2B_PHY_SLEEP_CBCR	       0x04B4
#define SDCC1_APPS_CMD_RCGR            0x04D0
#define SDCC2_APPS_CMD_RCGR            0x0510
#define SDCC3_APPS_CMD_RCGR            0x0550
#define SDCC4_APPS_CMD_RCGR            0x0590
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR   0x064C
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR   0x0660
#define BLSP1_UART1_APPS_CMD_RCGR      0x068C
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR   0x06CC
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR   0x06E0
#define BLSP1_UART2_APPS_CMD_RCGR      0x070C
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR   0x074C
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR   0x0760
#define BLSP1_UART3_APPS_CMD_RCGR      0x078C
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR   0x07CC
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR   0x07E0
#define BLSP1_UART4_APPS_CMD_RCGR      0x080C
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR   0x084C
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR   0x0860
#define BLSP1_UART5_APPS_CMD_RCGR      0x088C
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR   0x08CC
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR   0x08E0
#define BLSP1_UART6_APPS_CMD_RCGR      0x090C
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR   0x098C
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR   0x09A0
#define BLSP2_UART1_APPS_CMD_RCGR      0x09CC
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR   0x0A0C
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR   0x0A20
#define BLSP2_UART2_APPS_CMD_RCGR      0x0A4C
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR   0x0A8C
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR   0x0AA0
#define BLSP2_UART3_APPS_CMD_RCGR      0x0ACC
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR   0x0B0C
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR   0x0B20
#define BLSP2_UART4_APPS_CMD_RCGR      0x0B4C
#define BLSP2_QUP5_SPI_APPS_CMD_RCGR   0x0B8C
#define BLSP2_QUP5_I2C_APPS_CMD_RCGR   0x0BA0
#define BLSP2_UART5_APPS_CMD_RCGR      0x0BCC
#define BLSP2_QUP6_SPI_APPS_CMD_RCGR   0x0C0C
#define BLSP2_QUP6_I2C_APPS_CMD_RCGR   0x0C20
#define BLSP2_UART6_APPS_CMD_RCGR      0x0C4C
#define PDM2_CMD_RCGR                  0x0CD0
#define TSIF_REF_CMD_RCGR              0x0D90
#define CE1_CMD_RCGR                   0x1050
#define CE2_CMD_RCGR                   0x1090
#define GP1_CMD_RCGR                   0x1904
#define GP2_CMD_RCGR                   0x1944
#define GP3_CMD_RCGR                   0x1984
#define LPAIF_SPKR_CMD_RCGR            0xA000
#define LPAIF_PRI_CMD_RCGR             0xB000
#define LPAIF_SEC_CMD_RCGR             0xC000
#define LPAIF_TER_CMD_RCGR             0xD000
#define LPAIF_QUAD_CMD_RCGR            0xE000
#define LPAIF_PCM0_CMD_RCGR            0xF000
#define LPAIF_PCM1_CMD_RCGR            0x10000
#define RESAMPLER_CMD_RCGR             0x11000
#define SLIMBUS_CMD_RCGR               0x12000
#define LPAIF_PCMOE_CMD_RCGR           0x13000
#define AHBFABRIC_CMD_RCGR             0x18000
#define VCODEC0_CMD_RCGR               0x1000
#define PCLK0_CMD_RCGR                 0x2000
#define PCLK1_CMD_RCGR                 0x2020
#define MDP_CMD_RCGR                   0x2040
#define EXTPCLK_CMD_RCGR               0x2060
#define VSYNC_CMD_RCGR                 0x2080
#define EDPPIXEL_CMD_RCGR              0x20A0
#define EDPLINK_CMD_RCGR               0x20C0
#define EDPAUX_CMD_RCGR                0x20E0
#define HDMI_CMD_RCGR                  0x2100
#define BYTE0_CMD_RCGR                 0x2120
#define BYTE1_CMD_RCGR                 0x2140
#define ESC0_CMD_RCGR                  0x2160
#define ESC1_CMD_RCGR                  0x2180
#define CSI0PHYTIMER_CMD_RCGR          0x3000
#define CSI1PHYTIMER_CMD_RCGR          0x3030
#define CSI2PHYTIMER_CMD_RCGR          0x3060
#define CSI0_CMD_RCGR                  0x3090
#define CSI1_CMD_RCGR                  0x3100
#define CSI2_CMD_RCGR                  0x3160
#define CSI3_CMD_RCGR                  0x31C0
#define CCI_CMD_RCGR                   0x3300
#define MCLK0_CMD_RCGR                 0x3360
#define MCLK1_CMD_RCGR                 0x3390
#define MCLK2_CMD_RCGR                 0x33C0
#define MCLK3_CMD_RCGR                 0x33F0
#define MMSS_GP0_CMD_RCGR              0x3420
#define MMSS_GP1_CMD_RCGR              0x3450
#define JPEG0_CMD_RCGR                 0x3500
#define JPEG1_CMD_RCGR                 0x3520
#define JPEG2_CMD_RCGR                 0x3540
#define VFE0_CMD_RCGR                  0x3600
#define VFE1_CMD_RCGR                  0x3620
#define CPP_CMD_RCGR                   0x3640
#define GFX3D_CMD_RCGR                 0x4000
#define RBCPR_CMD_RCGR                 0x4060
#define AHB_CMD_RCGR                   0x5000
#define AXI_CMD_RCGR                   0x5040
#define OCMEMNOC_CMD_RCGR              0x5090
#define OCMEMCX_OCMEMNOC_CBCR          0x4058

#define MMSS_BCR                  0x0240
#define USB_30_BCR                0x03C0
#define USB3_PHY_BCR              0x03FC
#define USB_HS_HSIC_BCR           0x0400
#define USB_HS_BCR                0x0480
#define SDCC1_BCR                 0x04C0
#define SDCC2_BCR                 0x0500
#define SDCC3_BCR                 0x0540
#define SDCC4_BCR                 0x0580
#define BLSP1_BCR                 0x05C0
#define BLSP1_QUP1_BCR            0x0640
#define BLSP1_UART1_BCR           0x0680
#define BLSP1_QUP2_BCR            0x06C0
#define BLSP1_UART2_BCR           0x0700
#define BLSP1_QUP3_BCR            0x0740
#define BLSP1_UART3_BCR           0x0780
#define BLSP1_QUP4_BCR            0x07C0
#define BLSP1_UART4_BCR           0x0800
#define BLSP1_QUP5_BCR            0x0840
#define BLSP1_UART5_BCR           0x0880
#define BLSP1_QUP6_BCR            0x08C0
#define BLSP1_UART6_BCR           0x0900
#define BLSP2_BCR                 0x0940
#define BLSP2_QUP1_BCR            0x0980
#define BLSP2_UART1_BCR           0x09C0
#define BLSP2_QUP2_BCR            0x0A00
#define BLSP2_UART2_BCR           0x0A40
#define BLSP2_QUP3_BCR            0x0A80
#define BLSP2_UART3_BCR           0x0AC0
#define BLSP2_QUP4_BCR            0x0B00
#define BLSP2_UART4_BCR           0x0B40
#define BLSP2_QUP5_BCR            0x0B80
#define BLSP2_UART5_BCR           0x0BC0
#define BLSP2_QUP6_BCR            0x0C00
#define BLSP2_UART6_BCR           0x0C40
#define BOOT_ROM_BCR              0x0E00
#define PDM_BCR                   0x0CC0
#define PRNG_BCR                  0x0D00
#define BAM_DMA_BCR               0x0D40
#define TSIF_BCR                  0x0D80
#define CE1_BCR                   0x1040
#define CE2_BCR                   0x1080
#define AUDIO_CORE_BCR            0x4000
#define VENUS0_BCR                0x1020
#define MDSS_BCR                  0x2300
#define CAMSS_PHY0_BCR            0x3020
#define CAMSS_PHY1_BCR            0x3050
#define CAMSS_PHY2_BCR            0x3080
#define CAMSS_CSI0_BCR            0x30B0
#define CAMSS_CSI0PHY_BCR         0x30C0
#define CAMSS_CSI0RDI_BCR         0x30D0
#define CAMSS_CSI0PIX_BCR         0x30E0
#define CAMSS_CSI1_BCR            0x3120
#define CAMSS_CSI1PHY_BCR         0x3130
#define CAMSS_CSI1RDI_BCR         0x3140
#define CAMSS_CSI1PIX_BCR         0x3150
#define CAMSS_CSI2_BCR            0x3180
#define CAMSS_CSI2PHY_BCR         0x3190
#define CAMSS_CSI2RDI_BCR         0x31A0
#define CAMSS_CSI2PIX_BCR         0x31B0
#define CAMSS_CSI3_BCR            0x31E0
#define CAMSS_CSI3PHY_BCR         0x31F0
#define CAMSS_CSI3RDI_BCR         0x3200
#define CAMSS_CSI3PIX_BCR         0x3210
#define CAMSS_ISPIF_BCR           0x3220
#define CAMSS_CCI_BCR             0x3340
#define CAMSS_MCLK0_BCR           0x3380
#define CAMSS_MCLK1_BCR           0x33B0
#define CAMSS_MCLK2_BCR           0x33E0
#define CAMSS_MCLK3_BCR           0x3410
#define CAMSS_GP0_BCR             0x3440
#define CAMSS_GP1_BCR             0x3470
#define CAMSS_TOP_BCR             0x3480
#define CAMSS_MICRO_BCR           0x3490
#define CAMSS_JPEG_BCR            0x35A0
#define CAMSS_VFE_BCR             0x36A0
#define CAMSS_CSI_VFE0_BCR        0x3700
#define CAMSS_CSI_VFE1_BCR        0x3710
#define OCMEMNOC_BCR              0x50B0
#define MMSSNOCAHB_BCR            0x5020
#define MMSSNOCAXI_BCR            0x5060
#define OXILI_GFX3D_CBCR          0x4028
#define OXILICX_AHB_CBCR          0x403C
#define OXILICX_AXI_CBCR          0x4038
#define OXILI_BCR                 0x4020
#define OXILICX_BCR               0x4030
#define LPASS_Q6SS_BCR            0x6000

#define OCMEM_SYS_NOC_AXI_CBCR                   0x0244
#define OCMEM_NOC_CFG_AHB_CBCR                   0x0248
#define MMSS_NOC_CFG_AHB_CBCR                    0x024C

#define USB30_MASTER_CBCR                        0x03C8
#define USB30_MOCK_UTMI_CBCR                     0x03D0
#define USB_HSIC_AHB_CBCR                        0x0408
#define USB_HSIC_SYSTEM_CBCR                     0x040C
#define USB_HSIC_CBCR                            0x0410
#define USB_HSIC_IO_CAL_CBCR                     0x0414
#define USB_HS_SYSTEM_CBCR                       0x0484
#define USB_HS_AHB_CBCR                          0x0488
#define SDCC1_APPS_CBCR                          0x04C4
#define SDCC1_AHB_CBCR                           0x04C8
#define SDCC1_CDCCAL_SLEEP_CBCR                  0x04E4
#define SDCC1_CDCCAL_FF_CBCR                     0x04E8
#define SDCC2_APPS_CBCR                          0x0504
#define SDCC2_AHB_CBCR                           0x0508
#define SDCC3_APPS_CBCR                          0x0544
#define SDCC3_AHB_CBCR                           0x0548
#define SDCC4_APPS_CBCR                          0x0584
#define SDCC4_AHB_CBCR                           0x0588
#define BLSP1_AHB_CBCR                           0x05C4
#define BLSP1_QUP1_SPI_APPS_CBCR                 0x0644
#define BLSP1_QUP1_I2C_APPS_CBCR                 0x0648
#define BLSP1_UART1_APPS_CBCR                    0x0684
#define BLSP1_UART1_SIM_CBCR                     0x0688
#define BLSP1_QUP2_SPI_APPS_CBCR                 0x06C4
#define BLSP1_QUP2_I2C_APPS_CBCR                 0x06C8
#define BLSP1_UART2_APPS_CBCR                    0x0704
#define BLSP1_UART2_SIM_CBCR                     0x0708
#define BLSP1_QUP3_SPI_APPS_CBCR                 0x0744
#define BLSP1_QUP3_I2C_APPS_CBCR                 0x0748
#define BLSP1_UART3_APPS_CBCR                    0x0784
#define BLSP1_UART3_SIM_CBCR                     0x0788
#define BLSP1_QUP4_SPI_APPS_CBCR                 0x07C4
#define BLSP1_QUP4_I2C_APPS_CBCR                 0x07C8
#define BLSP1_UART4_APPS_CBCR                    0x0804
#define BLSP1_UART4_SIM_CBCR                     0x0808
#define BLSP1_QUP5_SPI_APPS_CBCR                 0x0844
#define BLSP1_QUP5_I2C_APPS_CBCR                 0x0848
#define BLSP1_UART5_APPS_CBCR                    0x0884
#define BLSP1_UART5_SIM_CBCR                     0x0888
#define BLSP1_QUP6_SPI_APPS_CBCR                 0x08C4
#define BLSP1_QUP6_I2C_APPS_CBCR                 0x08C8
#define BLSP1_UART6_APPS_CBCR                    0x0904
#define BLSP1_UART6_SIM_CBCR                     0x0908
#define BLSP2_AHB_CBCR                           0x0944
#define BOOT_ROM_AHB_CBCR                        0x0E04
#define BLSP2_QUP1_SPI_APPS_CBCR                 0x0984
#define BLSP2_QUP1_I2C_APPS_CBCR                 0x0988
#define BLSP2_UART1_APPS_CBCR                    0x09C4
#define BLSP2_UART1_SIM_CBCR                     0x09C8
#define BLSP2_QUP2_SPI_APPS_CBCR                 0x0A04
#define BLSP2_QUP2_I2C_APPS_CBCR                 0x0A08
#define BLSP2_UART2_APPS_CBCR                    0x0A44
#define BLSP2_UART2_SIM_CBCR                     0x0A48
#define BLSP2_QUP3_SPI_APPS_CBCR                 0x0A84
#define BLSP2_QUP3_I2C_APPS_CBCR                 0x0A88
#define BLSP2_UART3_APPS_CBCR                    0x0AC4
#define BLSP2_UART3_SIM_CBCR                     0x0AC8
#define BLSP2_QUP4_SPI_APPS_CBCR                 0x0B04
#define BLSP2_QUP4_I2C_APPS_CBCR                 0x0B08
#define BLSP2_UART4_APPS_CBCR                    0x0B44
#define BLSP2_UART4_SIM_CBCR                     0x0B48
#define BLSP2_QUP5_SPI_APPS_CBCR                 0x0B84
#define BLSP2_QUP5_I2C_APPS_CBCR                 0x0B88
#define BLSP2_UART5_APPS_CBCR                    0x0BC4
#define BLSP2_UART5_SIM_CBCR                     0x0BC8
#define BLSP2_QUP6_SPI_APPS_CBCR                 0x0C04
#define BLSP2_QUP6_I2C_APPS_CBCR                 0x0C08
#define BLSP2_UART6_APPS_CBCR                    0x0C44
#define BLSP2_UART6_SIM_CBCR                     0x0C48
#define PDM_AHB_CBCR                             0x0CC4
#define PDM_XO4_CBCR                             0x0CC8
#define PDM2_CBCR                                0x0CCC
#define PRNG_AHB_CBCR                            0x0D04
#define BAM_DMA_AHB_CBCR                         0x0D44
#define TSIF_AHB_CBCR                            0x0D84
#define TSIF_REF_CBCR                            0x0D88
#define MSG_RAM_AHB_CBCR                         0x0E44
#define CE1_CBCR                                 0x1044
#define CE1_AXI_CBCR                             0x1048
#define CE1_AHB_CBCR                             0x104C
#define CE2_CBCR                                 0x1084
#define CE2_AXI_CBCR                             0x1088
#define CE2_AHB_CBCR                             0x108C
#define GCC_AHB_CBCR                             0x10C0
#define GP1_CBCR                                 0x1900
#define GP2_CBCR                                 0x1940
#define GP3_CBCR                                 0x1980
#define AUDIO_CORE_GDSCR			 0x7000
#define AUDIO_CORE_IXFABRIC_CBCR		 0x1B000
#define AUDIO_CORE_LPAIF_CODEC_SPKR_OSR_CBCR     0xA014
#define AUDIO_CORE_LPAIF_CODEC_SPKR_IBIT_CBCR    0xA018
#define AUDIO_CORE_LPAIF_CODEC_SPKR_EBIT_CBCR    0xA01C
#define AUDIO_CORE_LPAIF_PRI_OSR_CBCR            0xB014
#define AUDIO_CORE_LPAIF_PRI_IBIT_CBCR           0xB018
#define AUDIO_CORE_LPAIF_PRI_EBIT_CBCR           0xB01C
#define AUDIO_CORE_LPAIF_SEC_OSR_CBCR            0xC014
#define AUDIO_CORE_LPAIF_SEC_IBIT_CBCR           0xC018
#define AUDIO_CORE_LPAIF_SEC_EBIT_CBCR           0xC01C
#define AUDIO_CORE_LPAIF_TER_OSR_CBCR            0xD014
#define AUDIO_CORE_LPAIF_TER_IBIT_CBCR           0xD018
#define AUDIO_CORE_LPAIF_TER_EBIT_CBCR           0xD01C
#define AUDIO_CORE_LPAIF_QUAD_OSR_CBCR           0xE014
#define AUDIO_CORE_LPAIF_QUAD_IBIT_CBCR          0xE018
#define AUDIO_CORE_LPAIF_QUAD_EBIT_CBCR          0xE01C
#define AUDIO_CORE_LPAIF_PCM0_IBIT_CBCR          0xF014
#define AUDIO_CORE_LPAIF_PCM0_EBIT_CBCR          0xF018
#define AUDIO_CORE_LPAIF_PCM1_IBIT_CBCR          0x10014
#define AUDIO_CORE_LPAIF_PCM1_EBIT_CBCR          0x10018
#define AUDIO_CORE_RESAMPLER_CORE_CBCR           0x11014
#define AUDIO_CORE_RESAMPLER_LFABIF_CBCR         0x11018
#define AUDIO_CORE_SLIMBUS_CORE_CBCR             0x12014
#define AUDIO_CORE_SLIMBUS_LFABIF_CBCR           0x12018
#define AUDIO_CORE_LPAIF_PCM_DATA_OE_CBCR        0x13014
#define VENUS0_VCODEC0_CBCR                      0x1028
#define VENUS0_AHB_CBCR                          0x1030
#define VENUS0_AXI_CBCR                          0x1034
#define VENUS0_OCMEMNOC_CBCR                     0x1038
#define MDSS_AHB_CBCR                            0x2308
#define MDSS_HDMI_AHB_CBCR                       0x230C
#define MDSS_AXI_CBCR                            0x2310
#define MDSS_PCLK0_CBCR                          0x2314
#define MDSS_PCLK1_CBCR                          0x2318
#define MDSS_MDP_CBCR                            0x231C
#define MDSS_MDP_LUT_CBCR                        0x2320
#define MDSS_EXTPCLK_CBCR                        0x2324
#define MDSS_VSYNC_CBCR                          0x2328
#define MDSS_EDPPIXEL_CBCR                       0x232C
#define MDSS_EDPLINK_CBCR                        0x2330
#define MDSS_EDPAUX_CBCR                         0x2334
#define MDSS_HDMI_CBCR                           0x2338
#define MDSS_BYTE0_CBCR                          0x233C
#define MDSS_BYTE1_CBCR                          0x2340
#define MDSS_ESC0_CBCR                           0x2344
#define MDSS_ESC1_CBCR                           0x2348
#define CAMSS_PHY0_CSI0PHYTIMER_CBCR             0x3024
#define CAMSS_PHY1_CSI1PHYTIMER_CBCR             0x3054
#define CAMSS_PHY2_CSI2PHYTIMER_CBCR             0x3084
#define CAMSS_CSI0_CBCR                          0x30B4
#define CAMSS_CSI0_AHB_CBCR                      0x30BC
#define CAMSS_CSI0PHY_CBCR                       0x30C4
#define CAMSS_CSI0RDI_CBCR                       0x30D4
#define CAMSS_CSI0PIX_CBCR                       0x30E4
#define CAMSS_CSI1_CBCR                          0x3124
#define CAMSS_CSI1_AHB_CBCR                      0x3128
#define CAMSS_CSI1PHY_CBCR                       0x3134
#define CAMSS_CSI1RDI_CBCR                       0x3144
#define CAMSS_CSI1PIX_CBCR                       0x3154
#define CAMSS_CSI2_CBCR                          0x3184
#define CAMSS_CSI2_AHB_CBCR                      0x3188
#define CAMSS_CSI2PHY_CBCR                       0x3194
#define CAMSS_CSI2RDI_CBCR                       0x31A4
#define CAMSS_CSI2PIX_CBCR                       0x31B4
#define CAMSS_CSI3_CBCR                          0x31E4
#define CAMSS_CSI3_AHB_CBCR                      0x31E8
#define CAMSS_CSI3PHY_CBCR                       0x31F4
#define CAMSS_CSI3RDI_CBCR                       0x3204
#define CAMSS_CSI3PIX_CBCR                       0x3214
#define CAMSS_ISPIF_AHB_CBCR                     0x3224
#define CAMSS_CCI_CCI_CBCR                       0x3344
#define CAMSS_CCI_CCI_AHB_CBCR                   0x3348
#define CAMSS_MCLK0_CBCR                         0x3384
#define CAMSS_MCLK1_CBCR                         0x33B4
#define CAMSS_MCLK2_CBCR                         0x33E4
#define CAMSS_MCLK3_CBCR                         0x3414
#define CAMSS_GP0_CBCR                           0x3444
#define CAMSS_GP1_CBCR                           0x3474
#define CAMSS_TOP_AHB_CBCR                       0x3484
#define CAMSS_MICRO_AHB_CBCR                     0x3494
#define CAMSS_JPEG_JPEG0_CBCR                    0x35A8
#define CAMSS_JPEG_JPEG1_CBCR                    0x35AC
#define CAMSS_JPEG_JPEG2_CBCR                    0x35B0
#define CAMSS_JPEG_JPEG_AHB_CBCR                 0x35B4
#define CAMSS_JPEG_JPEG_AXI_CBCR                 0x35B8
#define CAMSS_JPEG_JPEG_OCMEMNOC_CBCR            0x35BC
#define CAMSS_VFE_VFE0_CBCR                      0x36A8
#define CAMSS_VFE_VFE1_CBCR                      0x36AC
#define CAMSS_VFE_CPP_CBCR                       0x36B0
#define CAMSS_VFE_CPP_AHB_CBCR                   0x36B4
#define CAMSS_VFE_VFE_AHB_CBCR                   0x36B8
#define CAMSS_VFE_VFE_AXI_CBCR                   0x36BC
#define CAMSS_VFE_VFE_OCMEMNOC_CBCR              0x36C0
#define CAMSS_CSI_VFE0_CBCR                      0x3704
#define CAMSS_CSI_VFE1_CBCR                      0x3714
#define MMSS_MMSSNOC_AXI_CBCR                    0x506C
#define MMSS_MMSSNOC_AHB_CBCR                    0x5024
#define MMSS_MMSSNOC_BTO_AHB_CBCR                0x5028
#define MMSS_MISC_AHB_CBCR                       0x502C
#define MMSS_S0_AXI_CBCR                         0x5064
#define OCMEMNOC_CBCR                            0x50B4
#define LPASS_Q6SS_AHB_LFABIF_CBCR               0x22000
#define LPASS_Q6SS_XO_CBCR                       0x26000
#define LPASS_Q6_AXI_CBCR			 0x11C0
#define Q6SS_AHBM_CBCR				 0x22004
#define AUDIO_WRAPPER_BR_CBCR			 0x24000
#define MSS_CFG_AHB_CBCR                         0x0280
#define MSS_Q6_BIMC_AXI_CBCR			 0x0284

#define APCS_CLOCK_BRANCH_ENA_VOTE 0x1484
#define APCS_CLOCK_SLEEP_ENA_VOTE  0x1488

/* Mux source select values */
#define cxo_source_val	0
#define gpll0_source_val 1
#define gpll1_source_val 2
#define gpll4_source_val 5
#define gnd_source_val	5
#define mmpll0_mm_source_val 1
#define mmpll1_mm_source_val 2
#define mmpll3_mm_source_val 3
#define gpll0_mm_source_val 5
#define cxo_mm_source_val 0
#define mm_gnd_source_val 6
#define gpll1_hsic_source_val 4
#define cxo_lpass_source_val 0
#define gpll0_lpass_source_val 5
#define edp_mainlink_mm_source_val 4
#define edp_pixel_mm_source_val 5
#define edppll_350_mm_source_val 4
#define dsipll_750_mm_source_val 1
#define dsipll0_byte_mm_source_val 1
#define dsipll0_pixel_mm_source_val 1
#define hdmipll_mm_source_val 3

#define F_GCC_GND \
	{ \
		.freq_hz = 0, \
		.m_val = 0, \
		.n_val  = 0, \
		.div_src_val = BVAL(4, 0, 1) | BVAL(10, 8, gnd_source_val), \
	}

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

#define F_HDMI(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define F_EDP(f, s, div, m, n) \
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

#define F_LPASS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_lpass_source_val), \
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

#define D0_ID		 1
#define D1_ID		 2
#define A0_ID		 4
#define A1_ID		 5
#define A2_ID		 6
#define DIFF_CLK_ID	 7
#define DIV_CLK1_ID	11
#define DIV_CLK2_ID	12

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

DEFINE_CLK_RPM_SMD_BRANCH(cxo_clk_src, cxo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d0, cxo_d0_a, D0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d1, cxo_d1_a, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a0, cxo_a0_a, A0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a1, cxo_a1_a, A1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a2, cxo_a2_a, A2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_a_clk1, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_a_clk2, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(diff_clk, diff_a_clk, DIFF_CLK_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d0_pin, cxo_d0_a_pin, D0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d1_pin, cxo_d1_a_pin, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a0_pin, cxo_a0_a_pin, A0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a1_pin, cxo_a1_a_pin, A1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a2_pin, cxo_a2_a_pin, A2_ID);

static unsigned int soft_vote_gpll0;

static struct pll_vote_clk gpll0_ao_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_ACPU,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_a_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_ao_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_ao_clk_src.c),
	},
};

static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.soft_vote = &soft_vote_gpll0,
	.soft_vote_mask = PLL_SOFT_VOTE_PRIMARY,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_acpu_vote,
		CLK_INIT(gpll0_clk_src.c),
	},
};

static struct pll_vote_clk gpll1_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)GPLL1_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 480000000,
		.dbg_name = "gpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll1_clk_src.c),
	},
};

static struct pll_vote_clk gpll4_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE_REG,
	.en_mask = BIT(4),
	.status_reg = (void __iomem *)GPLL4_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.rate = 768000000,
		.dbg_name = "gpll4_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(gpll4_clk_src.c),
	},
};

static struct pll_vote_clk mmpll0_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS_REG,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)MMPLL0_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "mmpll0_clk_src",
		.rate = 800000000,
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll0_clk_src.c),
	},
};

static struct pll_vote_clk mmpll1_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS_REG,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)MMPLL1_STATUS_REG,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "mmpll1_clk_src",
		.rate = 846000000,
		.ops = &clk_ops_pll_vote,
		/* May be reassigned at runtime; alloc memory at compile time */
		VDD_DIG_FMAX_MAP1(LOW, 846000000),
		CLK_INIT(mmpll1_clk_src.c),
	},
};

static struct pll_clk mmpll3_clk_src = {
	.mode_reg = (void __iomem *)MMPLL3_MODE_REG,
	.status_reg = (void __iomem *)MMPLL3_STATUS_REG,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "mmpll3_clk_src",
		.rate = 820000000,
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll3_clk_src.c),
	},
};

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

static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_mss_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_wlan_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_pronto_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_ehci_host_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &cxo_clk_src.c);

static struct clk_freq_tbl ftbl_gcc_usb30_master_clk[] = {
	F(125000000,  gpll0,   1,   5,  24),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 125000000),
		CLK_INIT(usb30_master_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk[] = {
	F(  960000,    cxo,  10,   1,   2),
	F( 4800000,    cxo,   4,   0,   0),
	F( 9600000,    cxo,   2,   0,   0),
	F(15000000,  gpll0,  10,   1,   4),
	F(19200000,    cxo,   1,   0,   0),
	F(25000000,  gpll0,  12,   1,   2),
	F(50000000,  gpll0,  12,   0,   0),
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

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F(19200000,    cxo,   1,   0,   0),
	F(50000000,  gpll0,  12,   0,   0),
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

static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart1_6_apps_clk[] = {
	F_GCC_GND,
	F( 3686400,  gpll0,    1,  96,  15625),
	F( 7372800,  gpll0,    1, 192,  15625),
	F(14745600,  gpll0,    1, 384,  15625),
	F(16000000,  gpll0,    5,   2,     15),
	F(19200000,    cxo,    1,   0,      0),
	F(24000000,  gpll0,    5,   1,      5),
	F(32000000,  gpll0,    1,   4,     75),
	F(40000000,  gpll0,   15,   0,      0),
	F(46400000,  gpll0,    1,  29,    375),
	F(48000000,  gpll0, 12.5,   0,      0),
	F(51200000,  gpll0,    1,  32,    375),
	F(56000000,  gpll0,    1,   7,     75),
	F(58982400,  gpll0,    1, 1536, 15625),
	F(60000000,  gpll0,   10,   0,      0),
	F(63160000,  gpll0,  9.5,   0,      0),
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
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_ce1_pro_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F( 75000000,  gpll0,   8,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(150000000,  gpll0,   4,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ce1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_ce2_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_ce2_pro_clk[] = {
	F( 50000000,  gpll0,  12,   0,   0),
	F( 75000000,  gpll0,   8,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(150000000,  gpll0,   4,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ce2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp_clk[] = {
	F( 4800000,   cxo,  4,  0,   0),
	F( 6000000, gpll0, 10,  1,  10),
	F( 6750000, gpll0,  1,  1,  89),
	F( 8000000, gpll0, 15,  1,   5),
	F( 9600000,   cxo,  2,  0,   0),
	F(16000000, gpll0,  1,  2,  75),
	F(19200000,   cxo,  1,  0,   0),
	F(24000000, gpll0,  5,  1,   5),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp1_clk_src.c),
	},
};

static struct rcg_clk gp2_clk_src = {
	.cmd_rcgr_reg = GP2_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp2_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp2_clk_src.c),
	},
};

static struct rcg_clk gp3_clk_src = {
	.cmd_rcgr_reg = GP3_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(60000000,  gpll0,  10,   0,   0),
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

static struct clk_freq_tbl ftbl_gcc_sdcc1_4_apps_clk[] = {
	F(   144000,    cxo,  16,   3,  25),
	F(   400000,    cxo,  12,   1,   4),
	F( 20000000,  gpll0,  15,   1,   2),
	F( 25000000,  gpll0,  12,   1,   2),
	F( 50000000,  gpll0,  12,   0,   0),
	F(100000000,  gpll0,   6,   0,   0),
	F(200000000,  gpll0,   3,   0,   0),
	F(384000000,  gpll4,   2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_gcc_sdcc_apps_rumi_clk[] = {
	F(   400000,    cxo,  12,   1,   4),
	F( 19200000,    cxo,  1,    0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
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
	F(105000,    cxo,   2,   1,  91),
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
	F(60000000,  gpll0,   10,   0,   0),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 60000000),
		CLK_INIT(usb30_mock_utmi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(75000000,  gpll0,   8,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 37500000, NOMINAL, 75000000),
		CLK_INIT(usb_hs_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_clk[] = {
	F_HSIC(480000000,  gpll1,   1,   0,   0),
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
	F(9600000,    cxo,   2,   0,   0),
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

static struct clk_freq_tbl ftbl_gcc_usb_hsic_system_clk[] = {
	F(75000000,  gpll0,   8,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 37500000, NOMINAL, 75000000),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp1_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart6_apps_clk.c),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_SPI_APPS_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &blsp2_uart6_apps_clk_src.c,
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart6_apps_clk.c),
	},
};

static struct local_vote_clk gcc_ce1_clk = {
	.cbcr_reg = CE1_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(5),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce1_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce1_clk.c),
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

static struct local_vote_clk gcc_ce2_clk = {
	.cbcr_reg = CE2_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(2),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce2_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_clk.c),
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

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp1_clk_src.c,
		.dbg_name = "gcc_gp1_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GP2_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp2_clk_src.c,
		.dbg_name = "gcc_gp2_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GP3_CBCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &gp3_clk_src.c,
		.dbg_name = "gcc_gp3_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &cxo_clk_src.c,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &sdcc4_apps_clk_src.c,
		.dbg_name = "gcc_sdcc4_apps_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc4_apps_clk.c),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &tsif_ref_clk_src.c,
		.dbg_name = "gcc_tsif_ref_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ref_clk.c),
	},
};

struct branch_clk gcc_sys_noc_usb3_axi_clk = {
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

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
	.bcr_reg = USB_30_BCR,
	.has_sibling = 1,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb30_mock_utmi_clk_src.c,
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_mock_utmi_clk.c),
	},
};

struct branch_clk gcc_usb30_sleep_clk = {
	.cbcr_reg = USB30_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb30_sleep_clk.c),
	},
};

struct branch_clk gcc_usb2a_phy_sleep_clk = {
	.cbcr_reg = USB2A_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2a_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2a_phy_sleep_clk.c),
	},
};

struct branch_clk gcc_usb2b_phy_sleep_clk = {
	.cbcr_reg = USB2B_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2b_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2b_phy_sleep_clk.c),
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hs_system_clk_src.c,
		.dbg_name = "gcc_usb_hs_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_ahb_clk = {
	.cbcr_reg = USB_HSIC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_clk = {
	.cbcr_reg = USB_HSIC_CBCR,
	.bcr_reg = USB_HS_HSIC_BCR,
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
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_io_cal_clk_src.c,
		.dbg_name = "gcc_usb_hsic_io_cal_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_io_cal_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_system_clk = {
	.cbcr_reg = USB_HSIC_SYSTEM_CBCR,
	.bcr_reg = USB_HS_HSIC_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &usb_hsic_system_clk_src.c,
		.dbg_name = "gcc_usb_hsic_system_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

struct branch_clk gcc_mmss_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mmss_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_noc_cfg_ahb_clk.c),
	},
};

struct branch_clk gcc_ocmem_noc_cfg_ahb_clk = {
	.cbcr_reg = OCMEM_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ocmem_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_ocmem_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_cfg_ahb_clk = {
	.cbcr_reg = MSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_mss_q6_bimc_axi_clk = {
	.cbcr_reg = MSS_Q6_BIMC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mss_q6_bimc_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mss_q6_bimc_axi_clk.c),
	},
};

static struct clk_freq_tbl ftbl_mmss_axi_clk[] = {
	F_MM( 19200000,    cxo,     1,   0,   0),
	F_MM( 37500000,  gpll0,    16,   0,   0),
	F_MM( 50000000,  gpll0,    12,   0,   0),
	F_MM( 75000000,  gpll0,     8,   0,   0),
	F_MM(100000000,  gpll0,     6,   0,   0),
	F_MM(150000000,  gpll0,     4,   0,   0),
	F_MM(282000000, mmpll1,     3,   0,   0),
	F_MM(400000000, mmpll0,     2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_mmss_axi_v2_clk[] = {
	F_MM( 19200000,    cxo,     1,   0,   0),
	F_MM( 37500000,  gpll0,    16,   0,   0),
	F_MM( 50000000,  gpll0,    12,   0,   0),
	F_MM( 75000000,  gpll0,     8,   0,   0),
	F_MM(100000000,  gpll0,     6,   0,   0),
	F_MM(150000000,  gpll0,     4,   0,   0),
	F_MM(291750000, mmpll1,     4,   0,   0),
	F_MM(400000000, mmpll0,     2,   0,   0),
	F_MM(466800000, mmpll1,   2.5,   0,   0),
	F_END
};

static struct rcg_clk axi_clk_src = {
	.cmd_rcgr_reg = 0x5040,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mmss_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 282000000,
				  HIGH, 400000000),
		CLK_INIT(axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ocmemnoc_clk[] = {
	F_MM( 19200000,    cxo,   1,   0,   0),
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(282000000, mmpll1,   3,   0,   0),
	F_MM(400000000, mmpll0,   2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_ocmemnoc_v2_clk[] = {
	F_MM( 19200000,    cxo,   1,   0,   0),
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(291750000, mmpll1,   4,   0,   0),
	F_MM(400000000, mmpll0,   2,   0,   0),
	F_END
};

struct rcg_clk ocmemnoc_clk_src = {
	.cmd_rcgr_reg = OCMEMNOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ocmemnoc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "ocmemnoc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP3(LOW, 150000000, NOMINAL, 282000000,
				  HIGH, 400000000),
		CLK_INIT(ocmemnoc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_csi0_3_clk[] = {
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
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

static struct clk_freq_tbl ftbl_camss_vfe_vfe0_1_clk[] = {
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM( 60000000,  gpll0,  10,   0,   0),
	F_MM( 80000000,  gpll0, 7.5,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(109090000,  gpll0, 5.5,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(200000000,  gpll0,   3,   0,   0),
	F_MM(228570000, mmpll0, 3.5,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
	F_MM(465000000, mmpll3,   2,   0,   0),
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
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
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
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(vfe1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_mdp_clk[] = {
	F_MM( 37500000,  gpll0,  16,   0,   0),
	F_MM( 60000000,  gpll0,  10,   0,   0),
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM( 85710000,  gpll0,   7,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(133330000, mmpll0,   6,   0,   0),
	F_MM(160000000, mmpll0,   5,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_MM(240000000,  gpll0, 2.5,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
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
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_cci_cci_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
	F_END
};

static struct rcg_clk cci_clk_src = {
	.cmd_rcgr_reg = CCI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_cci_cci_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cci_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(cci_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_gp0_1_clk[] = {
	F_MM(   10000,    cxo,  16,   1, 120),
	F_MM(   20000,    cxo,  16,   1,  50),
	F_MM( 6000000,  gpll0,  10,   1,  10),
	F_MM(12000000,  gpll0,  10,   1,   5),
	F_MM(13000000,  gpll0,  10,  13,  60),
	F_MM(24000000,  gpll0,   5,   1,   5),
	F_END
};

static struct rcg_clk mmss_gp0_clk_src = {
	.cmd_rcgr_reg = MMSS_GP0_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_gp0_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(mmss_gp0_clk_src.c),
	},
};

static struct rcg_clk mmss_gp1_clk_src = {
	.cmd_rcgr_reg = MMSS_GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_camss_gp0_1_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_gp1_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(mmss_gp1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_jpeg_jpeg0_2_clk[] = {
	F_MM( 75000000,  gpll0,   8,   0,   0),
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(200000000,  gpll0,   3,   0,   0),
	F_MM(228570000, mmpll0, 3.5,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
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

static struct clk_freq_tbl ftbl_camss_mclk0_3_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
	F_MM(66670000,  gpll0,   9,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_camss_mclk0_3_pro_clk[] = {
	F_MM( 4800000,    cxo,    4,   0,   0),
	F_MM( 6000000,  gpll0,   10,   1,  10),
	F_MM( 8000000,  gpll0,   15,   1,   5),
	F_MM( 9600000,    cxo,    2,   0,   0),
	F_MM(16000000,  gpll0, 12.5,   1,   3),
	F_MM(19200000,    cxo,    1,   0,   0),
	F_MM(24000000,  gpll0,    5,   1,   5),
	F_MM(32000000, mmpll0,    5,   1,   5),
	F_MM(48000000,  gpll0, 12.5,   0,   0),
	F_MM(64000000, mmpll0, 12.5,   0,   0),
	F_MM(66670000,  gpll0,    9,   0,   0),
	F_END
};

static struct rcg_clk mclk0_clk_src = {
	.cmd_rcgr_reg = MCLK0_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk0_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk0_clk_src.c),
	},
};

static struct rcg_clk mclk1_clk_src = {
	.cmd_rcgr_reg = MCLK1_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk1_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk1_clk_src.c),
	},
};

static struct rcg_clk mclk2_clk_src = {
	.cmd_rcgr_reg = MCLK2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk2_clk_src.c),
	},
};

static struct rcg_clk mclk3_clk_src = {
	.cmd_rcgr_reg = MCLK3_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_mclk0_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mclk3_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 66670000),
		CLK_INIT(mclk3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_phy0_2_csi0_2phytimer_clk[] = {
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
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
	F_MM(150000000,  gpll0,   4,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(320000000, mmpll0, 2.5,   0,   0),
	F_MM(465000000, mmpll3,   2,   0,   0),
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
		VDD_DIG_FMAX_MAP3(LOW, 133330000, NOMINAL, 266670000,
				  HIGH, 320000000),
		CLK_INIT(cpp_clk_src.c),
	},
};

static struct clk_freq_tbl byte_freq_tbl[] = {
	{
		.src_clk = &byte_clk_src_8974.c,
		.div_src_val = BVAL(10, 8, dsipll0_byte_mm_source_val),
	},
	F_END
};

static struct rcg_clk byte0_clk_src = {
	.cmd_rcgr_reg = BYTE0_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_8974.c,
		.dbg_name = "byte0_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP3(LOW, 93800000, NOMINAL, 187500000,
				  HIGH, 188000000),
		CLK_INIT(byte0_clk_src.c),
	},
};

static struct rcg_clk byte1_clk_src = {
	.cmd_rcgr_reg = BYTE1_CMD_RCGR,
	.current_freq = byte_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &byte_clk_src_8974.c,
		.dbg_name = "byte1_clk_src",
		.ops = &clk_ops_byte,
		VDD_DIG_FMAX_MAP3(LOW, 93800000, NOMINAL, 187500000,
				  HIGH, 188000000),
		CLK_INIT(byte1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_edpaux_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(edpaux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_edplink_clk[] = {
	F_EDP(162000000, edp_mainlink,  1,   0,   0),
	F_EDP(270000000, edp_mainlink,  1,   0,   0),
	F_END
};

static struct rcg_clk edplink_clk_src = {
	.cmd_rcgr_reg = EDPLINK_CMD_RCGR,
	.freq_tbl = ftbl_mdss_edplink_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "edplink_clk_src",
		.ops = &clk_ops_rcg_edp,
		VDD_DIG_FMAX_MAP2(LOW, 135000000, NOMINAL, 270000000),
		CLK_INIT(edplink_clk_src.c),
	},
};

static struct clk_freq_tbl edp_pixel_freq_tbl[] = {
	{
		.src_clk = &edp_pixel_clk_src.c,
		.div_src_val = BVAL(10, 8, edp_pixel_mm_source_val)
				| BVAL(4, 0, 0),
	},
	F_END
};

static struct rcg_clk edppixel_clk_src = {
	.cmd_rcgr_reg = EDPPIXEL_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.current_freq = edp_pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edp_pixel_clk_src.c,
		.dbg_name = "edppixel_clk_src",
		.ops = &clk_ops_edppixel,
		VDD_DIG_FMAX_MAP2(LOW, 175000000, NOMINAL, 350000000),
		CLK_INIT(edppixel_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_esc0_1_clk[] = {
	F_MM(19200000,    cxo,   1,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
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
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(esc1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_extpclk_clk[] = {
	F_HDMI( 25200000, hdmipll, 1, 0, 0),
	F_HDMI( 27000000, hdmipll, 1, 0, 0),
	F_HDMI( 27030000, hdmipll, 1, 0, 0),
	F_HDMI( 65000000, hdmipll, 1, 0, 0),
	F_HDMI( 74250000, hdmipll, 1, 0, 0),
	F_HDMI(108000000, hdmipll, 1, 0, 0),
	F_HDMI(148500000, hdmipll, 1, 0, 0),
	F_HDMI(268500000, hdmipll, 1, 0, 0),
	F_HDMI(297000000, hdmipll, 1, 0, 0),
	F_END
};

static struct rcg_clk extpclk_clk_src = {
	.cmd_rcgr_reg = EXTPCLK_CMD_RCGR,
	.freq_tbl = ftbl_mdss_extpclk_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "extpclk_clk_src",
		.ops = &clk_ops_rcg_hdmi,
		VDD_DIG_FMAX_MAP2(LOW, 148500000, NOMINAL, 297000000),
		CLK_INIT(extpclk_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_hdmi_clk[] = {
	F_MDSS(19200000,    cxo,   1,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(hdmi_clk_src.c),
	},
};

static struct clk_freq_tbl pixel_freq_tbl[] = {
	{
		.src_clk = &pixel_clk_src_8974.c,
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
		.parent = &pixel_clk_src_8974.c,
		.dbg_name = "pclk0_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pclk0_clk_src.c),
	},
};

static struct rcg_clk pclk1_clk_src = {
	.cmd_rcgr_reg = PCLK1_CMD_RCGR,
	.current_freq = pixel_freq_tbl,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &pixel_clk_src_8974.c,
		.dbg_name = "pclk1_clk_src",
		.ops = &clk_ops_pixel,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pclk1_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_vsync_clk[] = {
	F_MDSS(19200000,    cxo,   1,   0,   0),
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
		VDD_DIG_FMAX_MAP2(LOW, 20000000, NOMINAL, 40000000),
		CLK_INIT(vsync_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_clk[] = {
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(133330000, mmpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(410000000, mmpll3,   2,   0,   0),
	F_END
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_v2_clk[] = {
	F_MM( 50000000,  gpll0,  12,   0,   0),
	F_MM(100000000,  gpll0,   6,   0,   0),
	F_MM(133330000, mmpll0,   6,   0,   0),
	F_MM(200000000, mmpll0,   4,   0,   0),
	F_MM(266670000, mmpll0,   3,   0,   0),
	F_MM(465000000, mmpll3,   2,   0,   0),
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
				  HIGH, 410000000),
		CLK_INIT(vcodec0_clk_src.c),
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
	.bcr_reg = CAMSS_CSI_VFE0_BCR,
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
	.bcr_reg = CAMSS_CSI_VFE1_BCR,
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
		.parent = &mmss_gp0_clk_src.c,
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
		.parent = &mmss_gp1_clk_src.c,
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
	.bcr_reg = CAMSS_JPEG_BCR,
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

static struct branch_clk camss_jpeg_jpeg_ocmemnoc_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "camss_jpeg_jpeg_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_ocmemnoc_clk.c),
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
	.bcr_reg = CAMSS_VFE_BCR,
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

static struct branch_clk camss_vfe_vfe_ocmemnoc_clk = {
	.cbcr_reg = CAMSS_VFE_VFE_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "camss_vfe_vfe_ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(camss_vfe_vfe_ocmemnoc_clk.c),
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

static struct branch_clk mdss_edplink_clk = {
	.cbcr_reg = MDSS_EDPLINK_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edplink_clk_src.c,
		.dbg_name = "mdss_edplink_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_edplink_clk.c),
	},
};

static struct branch_clk mdss_edppixel_clk = {
	.cbcr_reg = MDSS_EDPPIXEL_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &edppixel_clk_src.c,
		.dbg_name = "mdss_edppixel_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_edppixel_clk.c),
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

static struct branch_clk mdss_extpclk_clk = {
	.cbcr_reg = MDSS_EXTPCLK_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &extpclk_clk_src.c,
		.dbg_name = "mdss_extpclk_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_extpclk_clk.c),
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
	.bcr_reg = MDSS_BCR,
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
	/* The bus driver needs set_rate to go through to the parent */
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

struct branch_clk ocmemnoc_clk = {
	.cbcr_reg = OCMEMNOC_CBCR,
	.has_sibling = 0,
	.bcr_reg = 0x50b0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &ocmemnoc_clk_src.c,
		.dbg_name = "ocmemnoc_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemnoc_clk.c),
	},
};

struct branch_clk ocmemcx_ocmemnoc_clk = {
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

static struct branch_clk venus0_vcodec0_clk = {
	.cbcr_reg = VENUS0_VCODEC0_CBCR,
	.bcr_reg = VENUS0_BCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &vcodec0_clk_src.c,
		.dbg_name = "venus0_vcodec0_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_vcodec0_clk.c),
	},
};

static struct branch_clk oxilicx_axi_clk = {
	.cbcr_reg = OXILICX_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &axi_clk_src.c,
		.dbg_name = "oxilicx_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxilicx_axi_clk.c),
	},
};

static struct branch_clk oxili_gfx3d_clk = {
	.cbcr_reg = OXILI_GFX3D_CBCR,
	.bcr_reg = OXILI_BCR,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &oxili_gfx3d_clk_src.c,
		.dbg_name = "oxili_gfx3d_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_gfx3d_clk.c),
		.depends = &oxilicx_axi_clk.c,
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

static struct branch_clk q6ss_ahb_lfabif_clk = {
	.cbcr_reg = LPASS_Q6SS_AHB_LFABIF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_ahb_lfabif_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_ahb_lfabif_clk.c),
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

static struct branch_clk q6ss_xo_clk = {
	.cbcr_reg = LPASS_Q6SS_XO_CBCR,
	.bcr_reg = LPASS_Q6SS_BCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_xo_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_xo_clk.c),
	},
};

static struct branch_clk q6ss_ahbm_clk = {
	.cbcr_reg = Q6SS_AHBM_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_ahbm_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_ahbm_clk.c),
	},
};

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
	{&gcc_pdm_ahb_clk.c,			GCC_BASE, 0x00d0},
	{&gcc_blsp2_qup1_i2c_apps_clk.c,	GCC_BASE, 0x00ab},
	{&gcc_blsp2_qup3_spi_apps_clk.c,	GCC_BASE, 0x00b3},
	{&gcc_blsp2_uart5_apps_clk.c,		GCC_BASE, 0x00be},
	{&gcc_usb30_master_clk.c,		GCC_BASE, 0x0050},
	{&gcc_blsp2_qup3_i2c_apps_clk.c,	GCC_BASE, 0x00b4},
	{&gcc_usb_hsic_system_clk.c,		GCC_BASE, 0x0059},
	{&gcc_sdcc1_cdccal_sleep_clk.c,		GCC_BASE, 0x006a},
	{&gcc_sdcc1_cdccal_ff_clk.c,		GCC_BASE, 0x006b},
	{&gcc_blsp2_uart3_apps_clk.c,		GCC_BASE, 0x00b5},
	{&gcc_usb_hsic_io_cal_clk.c,		GCC_BASE, 0x005b},
	{&gcc_ce2_axi_clk.c,			GCC_BASE, 0x0141},
	{&gcc_sdcc3_ahb_clk.c,			GCC_BASE, 0x0079},
	{&gcc_blsp1_qup5_i2c_apps_clk.c,	GCC_BASE, 0x009d},
	{&gcc_blsp1_qup1_spi_apps_clk.c,	GCC_BASE, 0x008a},
	{&gcc_blsp2_uart4_apps_clk.c,		GCC_BASE, 0x00ba},
	{&gcc_ce2_clk.c,			GCC_BASE, 0x0140},
	{&gcc_blsp1_uart2_apps_clk.c,		GCC_BASE, 0x0091},
	{&gcc_sdcc1_ahb_clk.c,			GCC_BASE, 0x0069},
	{&gcc_mss_cfg_ahb_clk.c,		GCC_BASE, 0x0030},
	{&gcc_tsif_ahb_clk.c,			GCC_BASE, 0x00e8},
	{&gcc_sdcc4_ahb_clk.c,			GCC_BASE, 0x0081},
	{&gcc_blsp1_qup4_spi_apps_clk.c,	GCC_BASE, 0x0098},
	{&gcc_blsp2_qup4_spi_apps_clk.c,	GCC_BASE, 0x00b8},
	{&gcc_blsp1_qup3_spi_apps_clk.c,	GCC_BASE, 0x0093},
	{&gcc_blsp1_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00a2},
	{&gcc_blsp2_qup6_i2c_apps_clk.c,	GCC_BASE, 0x00c2},
	{&gcc_bam_dma_ahb_clk.c,		GCC_BASE, 0x00e0},
	{&gcc_sdcc3_apps_clk.c,			GCC_BASE, 0x0078},
	{&gcc_usb_hs_system_clk.c,		GCC_BASE, 0x0060},
	{&gcc_blsp1_ahb_clk.c,			GCC_BASE, 0x0088},
	{&gcc_sdcc1_apps_clk.c,			GCC_BASE, 0x0068},
	{&gcc_blsp2_qup5_i2c_apps_clk.c,	GCC_BASE, 0x00bd},
	{&gcc_blsp1_uart4_apps_clk.c,		GCC_BASE, 0x009a},
	{&gcc_blsp2_qup2_spi_apps_clk.c,	GCC_BASE, 0x00ae},
	{&gcc_blsp2_qup6_spi_apps_clk.c,	GCC_BASE, 0x00c1},
	{&gcc_blsp2_uart2_apps_clk.c,		GCC_BASE, 0x00b1},
	{&gcc_blsp1_qup2_spi_apps_clk.c,	GCC_BASE, 0x008e},
	{&gcc_usb_hsic_ahb_clk.c,		GCC_BASE, 0x0058},
	{&gcc_blsp1_uart3_apps_clk.c,		GCC_BASE, 0x0095},
	{&gcc_usb30_mock_utmi_clk.c,		GCC_BASE, 0x0052},
	{&gcc_ce1_axi_clk.c,			GCC_BASE, 0x0139},
	{&gcc_sdcc4_apps_clk.c,			GCC_BASE, 0x0080},
	{&gcc_blsp1_qup5_spi_apps_clk.c,	GCC_BASE, 0x009c},
	{&gcc_usb_hs_ahb_clk.c,			GCC_BASE, 0x0061},
	{&gcc_blsp1_qup6_spi_apps_clk.c,	GCC_BASE, 0x00a1},
	{&gcc_blsp2_qup2_i2c_apps_clk.c,	GCC_BASE, 0x00b0},
	{&gcc_prng_ahb_clk.c,			GCC_BASE, 0x00d8},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,	GCC_BASE, 0x0094},
	{&gcc_usb_hsic_clk.c,			GCC_BASE, 0x005a},
	{&gcc_blsp1_uart6_apps_clk.c,		GCC_BASE, 0x00a3},
	{&gcc_sdcc2_apps_clk.c,			GCC_BASE, 0x0070},
	{&gcc_tsif_ref_clk.c,			GCC_BASE, 0x00e9},
	{&gcc_blsp1_uart1_apps_clk.c,		GCC_BASE, 0x008c},
	{&gcc_blsp2_qup5_spi_apps_clk.c,	GCC_BASE, 0x00bc},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,	GCC_BASE, 0x0099},
	{&gcc_mmss_noc_cfg_ahb_clk.c,		GCC_BASE, 0x002a},
	{&gcc_blsp2_ahb_clk.c,			GCC_BASE, 0x00a8},
	{&gcc_boot_rom_ahb_clk.c,		GCC_BASE, 0x00f8},
	{&gcc_ce1_ahb_clk.c,			GCC_BASE, 0x013a},
	{&gcc_pdm2_clk.c,			GCC_BASE, 0x00d2},
	{&gcc_blsp2_qup4_i2c_apps_clk.c,	GCC_BASE, 0x00b9},
	{&gcc_ce2_ahb_clk.c,			GCC_BASE, 0x0142},
	{&gcc_blsp1_uart5_apps_clk.c,		GCC_BASE, 0x009e},
	{&gcc_blsp2_qup1_spi_apps_clk.c,	GCC_BASE, 0x00aa},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,	GCC_BASE, 0x0090},
	{&gcc_blsp2_uart1_apps_clk.c,		GCC_BASE, 0x00ac},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,	GCC_BASE, 0x008b},
	{&gcc_blsp2_uart6_apps_clk.c,		GCC_BASE, 0x00c3},
	{&gcc_sdcc2_ahb_clk.c,			GCC_BASE, 0x0071},
	{&gcc_usb30_sleep_clk.c,		GCC_BASE, 0x0051},
	{&gcc_usb2a_phy_sleep_clk.c,		GCC_BASE, 0x0063},
	{&gcc_usb2b_phy_sleep_clk.c,		GCC_BASE, 0x0064},
	{&gcc_sys_noc_usb3_axi_clk.c,		GCC_BASE, 0x0001},
	{&gcc_ocmem_noc_cfg_ahb_clk.c,		GCC_BASE, 0x0029},
	{&gcc_ce1_clk.c,			GCC_BASE, 0x0138},
	{&gcc_lpass_q6_axi_clk.c,		GCC_BASE, 0x0160},
	{&gcc_mss_q6_bimc_axi_clk.c,		GCC_BASE, 0x0031},
	{&cnoc_clk.c,                           GCC_BASE, 0x0008},
	{&pnoc_clk.c,                           GCC_BASE, 0x0010},
	{&snoc_clk.c,                           GCC_BASE, 0x0000},
	{&bimc_clk.c,                           GCC_BASE, 0x0155},
	{&mmss_mmssnoc_axi_clk.c,		MMSS_BASE, 0x0004},
	{&ocmemnoc_clk.c,			MMSS_BASE, 0x0007},
	{&ocmemcx_ocmemnoc_clk.c,		MMSS_BASE, 0x0009},
	{&camss_cci_cci_ahb_clk.c,		MMSS_BASE, 0x002e},
	{&camss_cci_cci_clk.c,			MMSS_BASE, 0x002d},
	{&camss_csi0_ahb_clk.c,			MMSS_BASE, 0x0042},
	{&camss_csi0_clk.c,			MMSS_BASE, 0x0041},
	{&camss_csi0phy_clk.c,			MMSS_BASE, 0x0043},
	{&camss_csi0pix_clk.c,			MMSS_BASE, 0x0045},
	{&camss_csi0rdi_clk.c,			MMSS_BASE, 0x0044},
	{&camss_csi1_ahb_clk.c,			MMSS_BASE, 0x0047},
	{&camss_csi1_clk.c,			MMSS_BASE, 0x0046},
	{&camss_csi1phy_clk.c,			MMSS_BASE, 0x0048},
	{&camss_csi1pix_clk.c,			MMSS_BASE, 0x004a},
	{&camss_csi1rdi_clk.c,			MMSS_BASE, 0x0049},
	{&camss_csi2_ahb_clk.c,			MMSS_BASE, 0x004c},
	{&camss_csi2_clk.c,			MMSS_BASE, 0x004b},
	{&camss_csi2phy_clk.c,			MMSS_BASE, 0x004d},
	{&camss_csi2pix_clk.c,			MMSS_BASE, 0x004f},
	{&camss_csi2rdi_clk.c,			MMSS_BASE, 0x004e},
	{&camss_csi3_ahb_clk.c,			MMSS_BASE, 0x0051},
	{&camss_csi3_clk.c,			MMSS_BASE, 0x0050},
	{&camss_csi3phy_clk.c,			MMSS_BASE, 0x0052},
	{&camss_csi3pix_clk.c,			MMSS_BASE, 0x0054},
	{&camss_csi3rdi_clk.c,			MMSS_BASE, 0x0053},
	{&camss_csi_vfe0_clk.c,			MMSS_BASE, 0x003f},
	{&camss_csi_vfe1_clk.c,			MMSS_BASE, 0x0040},
	{&camss_gp0_clk.c,			MMSS_BASE, 0x0027},
	{&camss_gp1_clk.c,			MMSS_BASE, 0x0028},
	{&camss_ispif_ahb_clk.c,		MMSS_BASE, 0x0055},
	{&camss_jpeg_jpeg0_clk.c,		MMSS_BASE, 0x0032},
	{&camss_jpeg_jpeg1_clk.c,		MMSS_BASE, 0x0033},
	{&camss_jpeg_jpeg2_clk.c,		MMSS_BASE, 0x0034},
	{&camss_jpeg_jpeg_ahb_clk.c,		MMSS_BASE, 0x0035},
	{&camss_jpeg_jpeg_axi_clk.c,		MMSS_BASE, 0x0036},
	{&camss_jpeg_jpeg_ocmemnoc_clk.c,	MMSS_BASE, 0x0037},
	{&camss_mclk0_clk.c,			MMSS_BASE, 0x0029},
	{&camss_mclk1_clk.c,			MMSS_BASE, 0x002a},
	{&camss_mclk2_clk.c,			MMSS_BASE, 0x002b},
	{&camss_mclk3_clk.c,			MMSS_BASE, 0x002c},
	{&camss_micro_ahb_clk.c,		MMSS_BASE, 0x0026},
	{&camss_phy0_csi0phytimer_clk.c,	MMSS_BASE, 0x002f},
	{&camss_phy1_csi1phytimer_clk.c,	MMSS_BASE, 0x0030},
	{&camss_phy2_csi2phytimer_clk.c,	MMSS_BASE, 0x0031},
	{&camss_top_ahb_clk.c,			MMSS_BASE, 0x0025},
	{&camss_vfe_cpp_ahb_clk.c,		MMSS_BASE, 0x003b},
	{&camss_vfe_cpp_clk.c,			MMSS_BASE, 0x003a},
	{&camss_vfe_vfe0_clk.c,			MMSS_BASE, 0x0038},
	{&camss_vfe_vfe1_clk.c,			MMSS_BASE, 0x0039},
	{&camss_vfe_vfe_ahb_clk.c,		MMSS_BASE, 0x003c},
	{&camss_vfe_vfe_axi_clk.c,		MMSS_BASE, 0x003d},
	{&camss_vfe_vfe_ocmemnoc_clk.c,		MMSS_BASE, 0x003e},
	{&oxilicx_axi_clk.c,			MMSS_BASE, 0x000b},
	{&oxilicx_ahb_clk.c,			MMSS_BASE, 0x000c},
	{&ocmemcx_ocmemnoc_clk.c,		MMSS_BASE, 0x0009},
	{&oxili_gfx3d_clk.c,			MMSS_BASE, 0x000d},
	{&venus0_axi_clk.c,			MMSS_BASE, 0x000f},
	{&venus0_ocmemnoc_clk.c,		MMSS_BASE, 0x0010},
	{&venus0_ahb_clk.c,			MMSS_BASE, 0x0011},
	{&venus0_vcodec0_clk.c,			MMSS_BASE, 0x000e},
	{&mmss_s0_axi_clk.c,			MMSS_BASE, 0x0005},
	{&mmssnoc_ahb_clk.c,			MMSS_BASE, 0x0001},
	{&mdss_ahb_clk.c,			MMSS_BASE, 0x0022},
	{&mdss_hdmi_clk.c,			MMSS_BASE, 0x001d},
	{&mdss_mdp_clk.c,			MMSS_BASE, 0x0014},
	{&mdss_mdp_lut_clk.c,			MMSS_BASE, 0x0015},
	{&mdss_axi_clk.c,			MMSS_BASE, 0x0024},
	{&mdss_vsync_clk.c,			MMSS_BASE, 0x001c},
	{&mdss_esc0_clk.c,			MMSS_BASE, 0x0020},
	{&mdss_esc1_clk.c,			MMSS_BASE, 0x0021},
	{&mdss_edpaux_clk.c,			MMSS_BASE, 0x001b},
	{&mdss_byte0_clk.c,			MMSS_BASE, 0x001e},
	{&mdss_byte1_clk.c,			MMSS_BASE, 0x001f},
	{&mdss_edplink_clk.c,			MMSS_BASE, 0x001a},
	{&mdss_edppixel_clk.c,			MMSS_BASE, 0x0019},
	{&mdss_extpclk_clk.c,			MMSS_BASE, 0x0018},
	{&mdss_hdmi_ahb_clk.c,			MMSS_BASE, 0x0023},
	{&mdss_pclk0_clk.c,			MMSS_BASE, 0x0016},
	{&mdss_pclk1_clk.c,			MMSS_BASE, 0x0017},
	{&q6ss_xo_clk.c,			LPASS_BASE, 0x002b},
	{&q6ss_ahb_lfabif_clk.c,		LPASS_BASE, 0x001e},
	{&q6ss_ahbm_clk.c,			LPASS_BASE, 0x001d},

	{&krait0_clk.c,				APCS_BASE, M_ACPU0},
	{&krait1_clk.c,				APCS_BASE, M_ACPU1},
	{&krait2_clk.c,				APCS_BASE, M_ACPU2},
	{&krait3_clk.c,				APCS_BASE, M_ACPU3},
	{&l2_clk.c,				APCS_BASE, M_L2},

	{&dummy_clk,				N_BASES,   0x0000},
};

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
		writel_relaxed(0, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));
		clk_sel = measure_mux[i].debug_mux;
		break;

	case MMSS_BASE:
		writel_relaxed(0, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));
		clk_sel = 0x02C;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));

		/* Activate debug clock output */
		regval |= BIT(16);
		writel_relaxed(regval, MMSS_REG_BASE(MMSS_DEBUG_CLK_CTL_REG));
		break;

	case LPASS_BASE:
		writel_relaxed(0, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL_REG));
		clk_sel = 0x161;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL_REG));

		/* Activate debug clock output */
		regval |= BIT(20);
		writel_relaxed(regval, LPASS_REG_BASE(LPASS_DEBUG_CLK_CTL_REG));
		break;

	case APCS_BASE:
		clk->multiplier = 4;
		clk_sel = 0x16A;

		if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 1 &&
		    cpu_is_msm8974()) {
			if (measure_mux[i].debug_mux == M_L2)
				regval = BIT(7)|BIT(0);
			else
				regval = BIT(7)|(measure_mux[i].debug_mux << 3);
		} else {
			if (measure_mux[i].debug_mux == M_L2)
				regval = BIT(12);
			else
				regval = measure_mux[i].debug_mux << 8;
			writel_relaxed(BIT(0), APCS_REG_BASE(L2_CBCR_REG));
		}
		writel_relaxed(regval, APCS_REG_BASE(GLB_CLK_DIAG_REG));
		break;

	default:
		return -EINVAL;
	}

	/* Set debug mux clock index */
	regval = BVAL(8, 0, clk_sel);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));

	/* Activate debug clock output */
	regval |= BIT(16);
	writel_relaxed(regval, GCC_REG_BASE(GCC_DEBUG_CLK_CTL_REG));

	/* Make sure test vector is set before starting measurements. */
	mb();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL_REG));

	/* Wait for timer to become ready. */
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS_REG)) &
			BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, GCC_REG_BASE(CLOCK_FRQ_MEASURE_CTL_REG));
	while ((readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS_REG)) &
			BIT(25)) == 0)
		cpu_relax();

	/* Return measured ticks. */
	return readl_relaxed(GCC_REG_BASE(CLOCK_FRQ_MEASURE_STATUS_REG)) &
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

	ret = clk_prepare_enable(&cxo_clk_src.c);
	if (ret) {
		pr_warning("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	gcc_xo4_reg_backup = readl_relaxed(GCC_REG_BASE(GCC_XO_DIV4_CBCR_REG));
	writel_relaxed(0x1, GCC_REG_BASE(GCC_XO_DIV4_CBCR_REG));

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

	writel_relaxed(gcc_xo4_reg_backup, GCC_REG_BASE(GCC_XO_DIV4_CBCR_REG));

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short) {
		ret = 0;
	} else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((clk->sample_ticks * 10) + 35));
		ret = (raw_count_full * clk->multiplier);
	}

	writel_relaxed(0x51A00, GCC_REG_BASE(GCC_PLLTEST_PAD_CFG_REG));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(&cxo_clk_src.c);

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


static struct clk_lookup msm_clocks_8974_rumi[] = {
	CLK_LOOKUP("iface_clk", gcc_sdcc1_ahb_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("core_clk", gcc_sdcc1_apps_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("iface_clk", gcc_sdcc2_ahb_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("core_clk", gcc_sdcc2_apps_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("iface_clk", gcc_sdcc3_ahb_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("core_clk", gcc_sdcc3_apps_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("iface_clk", gcc_sdcc4_ahb_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("core_clk", gcc_sdcc4_apps_clk.c, "msm_sdcc.4"),
	CLK_DUMMY("xo",		XO_CLK,		NULL,	OFF),
	CLK_DUMMY("xo",		XO_CLK,		"fb21b000.qcom,pronto", OFF),
	CLK_DUMMY("core_clk",	BLSP2_UART_CLK,	"f991f000.serial",	OFF),
	CLK_DUMMY("iface_clk",	BLSP2_UART_CLK,	"f991f000.serial",	OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	NULL,			OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	NULL,			OFF),
	CLK_DUMMY("core_clk",	SDC3_CLK,	NULL,			OFF),
	CLK_DUMMY("iface_clk",	SDC3_P_CLK,	NULL,			OFF),
	CLK_DUMMY("phy_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("core_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("iface_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("xo", NULL, "msm_otg", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK,	NULL, 0),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,	NULL, 0),
	CLK_DUMMY("mem_clk",	NULL,	NULL, 0),
	CLK_DUMMY("core_clk",	SPI_CLK,	"spi_qsd.1",	OFF),
	CLK_DUMMY("iface_clk",	SPI_P_CLK,	"spi_qsd.1",	OFF),
	CLK_DUMMY("core_clk",	NULL,	"f9966000.i2c", 0),
	CLK_DUMMY("iface_clk",	NULL,	"f9966000.i2c", 0),
	CLK_DUMMY("core_clk",	NULL,	"fe12f000.slim",	OFF),
	CLK_DUMMY("core_clk", "mdp.0", NULL, 0),
	CLK_DUMMY("core_clk_src", "mdp.0", NULL, 0),
	CLK_DUMMY("lut_clk", "mdp.0", NULL, 0),
	CLK_DUMMY("vsync_clk", "mdp.0", NULL, 0),
	CLK_DUMMY("iface_clk", "mdp.0", NULL, 0),
	CLK_DUMMY("bus_clk", "mdp.0", NULL, 0),
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
	CLK_DUMMY("alt_core_clk", NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fdc84000.qcom,iommu", oFF),
	CLK_DUMMY("core_clk", NULL, "fdc84000.qcom,iommu", oFF),
};

static struct clk_lookup msm_clocks_8974ac_only[] __initdata = {
	CLK_LOOKUP("gpll4", gpll4_clk_src.c, ""),
	CLK_LOOKUP("sleep_clk", gcc_sdcc1_cdccal_sleep_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("cal_clk", gcc_sdcc1_cdccal_ff_clk.c, "msm_sdcc.1"),
};

static struct clk_lookup msm_clocks_8974_common[] __initdata = {
	CLK_LOOKUP("xo",        cxo_otg_clk.c,                  "msm_otg"),
	CLK_LOOKUP("xo",  cxo_pil_lpass_clk.c,      "fe200000.qcom,lpass"),
	CLK_LOOKUP("xo",    cxo_pil_mss_clk.c,        "fc880000.qcom,mss"),
	CLK_LOOKUP("xo",       cxo_wlan_clk.c, "fb000000.qcom,wcnss-wlan"),
	CLK_LOOKUP("rf_clk",         cxo_a2.c, "fb000000.qcom,wcnss-wlan"),
	CLK_LOOKUP("xo", cxo_pil_pronto_clk.c,     "fb21b000.qcom,pronto"),
	CLK_LOOKUP("xo",       cxo_dwc3_clk.c,                 "msm_dwc3"),
	CLK_LOOKUP("xo",  cxo_ehci_host_clk.c,            "msm_ehci_host"),
	CLK_LOOKUP("xo",        cxo_lpm_clk.c,        "fc4281d0.qcom,mpm"),

	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),

	CLK_LOOKUP("hfpll_src", cxo_a_clk_src.c,   "f9016000.qcom,clock-krait"),
	CLK_LOOKUP("aux_clk",   gpll0_ao_clk_src.c,
						"f9016000.qcom,clock-krait"),
	CLK_LOOKUP("gpll0", gpll0_clk_src.c, ""),

	CLK_LOOKUP("dma_bam_pclk", gcc_bam_dma_ahb_clk.c, "msm_sps"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991f000.serial"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9924000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f991e000.serial"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_i2c_apps_clk.c, "f9923000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9923000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_i2c_apps_clk.c, "f9924000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup2_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup1_spi_apps_clk.c, "f9923000.spi"),
	CLK_LOOKUP("iface_clk", gcc_blsp1_ahb_clk.c, "f9923000.spi"),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup3_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup3_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup4_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup4_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup5_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup5_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_qup6_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart1_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart2_apps_clk.c, "f991e000.serial"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart3_apps_clk.c, "f991f000.serial"),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart4_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart5_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp1_uart6_apps_clk.c, ""),

	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f9967000.i2c"),
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f9966000.spi"),
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f995e000.serial"),
	CLK_LOOKUP("iface_clk", gcc_blsp2_ahb_clk.c, "f995d000.uart"),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup1_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup1_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup2_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup2_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup3_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup3_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup4_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup5_i2c_apps_clk.c, "f9967000.i2c"),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup4_spi_apps_clk.c, "f9966000.spi"),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup5_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup6_i2c_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_qup6_spi_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart1_apps_clk.c, "f995d000.uart"),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart2_apps_clk.c, "f995e000.serial"),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart3_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart4_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart5_apps_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_blsp2_uart6_apps_clk.c, ""),

	CLK_LOOKUP("core_clk_src", ce1_clk_src.c, ""),
	CLK_LOOKUP("core_clk", gcc_ce1_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_ce2_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_ce1_ahb_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_ce2_ahb_clk.c, ""),
	CLK_LOOKUP("bus_clk", gcc_ce1_axi_clk.c, ""),
	CLK_LOOKUP("bus_clk", gcc_ce2_axi_clk.c, ""),

	CLK_LOOKUP("core_clk",     gcc_ce2_clk.c,         "qcedev.0"),
	CLK_LOOKUP("iface_clk",    gcc_ce2_ahb_clk.c,     "qcedev.0"),
	CLK_LOOKUP("bus_clk",      gcc_ce2_axi_clk.c,     "qcedev.0"),
	CLK_LOOKUP("core_clk_src", ce2_clk_src.c,         "qcedev.0"),

	CLK_LOOKUP("core_clk",     gcc_ce2_clk.c,     "qcrypto.0"),
	CLK_LOOKUP("iface_clk",    gcc_ce2_ahb_clk.c, "qcrypto.0"),
	CLK_LOOKUP("bus_clk",      gcc_ce2_axi_clk.c, "qcrypto.0"),
	CLK_LOOKUP("core_clk_src", ce2_clk_src.c,     "qcrypto.0"),

	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "qseecom"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "qseecom"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "qseecom"),
	CLK_LOOKUP("core_clk_src", ce1_clk_src.c,         "qseecom"),

	CLK_LOOKUP("ce_drv_core_clk",     gcc_ce2_clk.c,         "qseecom"),
	CLK_LOOKUP("ce_drv_iface_clk",    gcc_ce2_ahb_clk.c,     "qseecom"),
	CLK_LOOKUP("ce_drv_bus_clk",      gcc_ce2_axi_clk.c,     "qseecom"),
	CLK_LOOKUP("ce_drv_core_clk_src", ce2_clk_src.c,         "qseecom"),

	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "mcd"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "mcd"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "mcd"),
	CLK_LOOKUP("core_clk_src", ce1_clk_src.c,         "mcd"),

	CLK_LOOKUP("core_clk",     gcc_ce1_clk.c,         "scm"),
	CLK_LOOKUP("iface_clk",    gcc_ce1_ahb_clk.c,     "scm"),
	CLK_LOOKUP("bus_clk",      gcc_ce1_axi_clk.c,     "scm"),
	CLK_LOOKUP("core_clk_src", ce1_clk_src.c,         "scm"),

	CLK_LOOKUP("core_clk", gcc_gp1_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_gp2_clk.c, ""),
	CLK_LOOKUP("core_clk", gcc_gp3_clk.c, ""),

	CLK_LOOKUP("core_clk", gcc_pdm2_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_pdm_ahb_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_prng_ahb_clk.c, ""),

	CLK_LOOKUP("iface_clk", gcc_sdcc1_ahb_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("core_clk", gcc_sdcc1_apps_clk.c, "msm_sdcc.1"),
	CLK_LOOKUP("iface_clk", gcc_sdcc2_ahb_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("core_clk", gcc_sdcc2_apps_clk.c, "msm_sdcc.2"),
	CLK_LOOKUP("iface_clk", gcc_sdcc3_ahb_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("core_clk", gcc_sdcc3_apps_clk.c, "msm_sdcc.3"),
	CLK_LOOKUP("iface_clk", gcc_sdcc4_ahb_clk.c, "msm_sdcc.4"),
	CLK_LOOKUP("core_clk", gcc_sdcc4_apps_clk.c, "msm_sdcc.4"),

	CLK_LOOKUP("iface_clk", gcc_tsif_ahb_clk.c, "f99d8000.msm_tspp"),
	CLK_LOOKUP("ref_clk", gcc_tsif_ref_clk.c, "f99d8000.msm_tspp"),

	CLK_LOOKUP("mem_clk", gcc_usb30_master_clk.c,           "usb_bam"),
	CLK_LOOKUP("mem_iface_clk", gcc_sys_noc_usb3_axi_clk.c, "usb_bam"),
	CLK_LOOKUP("core_clk", gcc_usb30_master_clk.c,    "msm_dwc3"),
	CLK_LOOKUP("utmi_clk", gcc_usb30_mock_utmi_clk.c, "msm_dwc3"),
	CLK_LOOKUP("iface_clk", gcc_sys_noc_usb3_axi_clk.c, "msm_dwc3"),
	CLK_LOOKUP("iface_clk", gcc_sys_noc_usb3_axi_clk.c, "msm_usb3"),
	CLK_LOOKUP("sleep_clk", gcc_usb30_sleep_clk.c, "msm_dwc3"),
	CLK_LOOKUP("sleep_a_clk", gcc_usb2a_phy_sleep_clk.c, "msm_dwc3"),
	CLK_LOOKUP("sleep_b_clk", gcc_usb2b_phy_sleep_clk.c, "msm_dwc3"),
	CLK_LOOKUP("ref_clk", diff_clk.c, "msm_dwc3"),
	CLK_LOOKUP("iface_clk", gcc_usb_hs_ahb_clk.c,     "msm_otg"),
	CLK_LOOKUP("core_clk", gcc_usb_hs_system_clk.c,   "msm_otg"),
	CLK_LOOKUP("iface_clk", gcc_usb_hsic_ahb_clk.c,	  "msm_hsic_host"),
	CLK_LOOKUP("phy_clk", gcc_usb_hsic_clk.c,	  "msm_hsic_host"),
	CLK_LOOKUP("cal_clk", gcc_usb_hsic_io_cal_clk.c,  "msm_hsic_host"),
	CLK_LOOKUP("core_clk", gcc_usb_hsic_system_clk.c, "msm_hsic_host"),
	CLK_LOOKUP("osr_clk", div_clk1.c, "msm-dai-q6-dev.16384"),
	CLK_LOOKUP("ref_clk", div_clk2.c, "msm_smsc_hub"),
	CLK_LOOKUP("iface_clk", gcc_usb_hs_ahb_clk.c,     "msm_ehci_host"),
	CLK_LOOKUP("core_clk", gcc_usb_hs_system_clk.c,   "msm_ehci_host"),
	CLK_LOOKUP("sleep_clk", gcc_usb2b_phy_sleep_clk.c, "msm_ehci_host"),
	CLK_LOOKUP("pwm_clk", div_clk2.c, "0-0048"),

	/* Multimedia clocks */
	CLK_LOOKUP("bus_clk_src", axi_clk_src.c, ""),
	CLK_LOOKUP("bus_clk", mmss_mmssnoc_axi_clk.c, ""),
	CLK_LOOKUP("bus_clk", mmssnoc_ahb_clk.c, ""),
	CLK_LOOKUP("core_clk", mdss_edpaux_clk.c, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP("pixel_clk", mdss_edppixel_clk.c, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP("link_clk", mdss_edplink_clk.c, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP("byte_clk", mdss_byte0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("byte_clk", mdss_byte1_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_esc0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("core_clk", mdss_esc1_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("pixel_clk", mdss_pclk0_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("pixel_clk", mdss_pclk1_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("mdp_core_clk", mdss_mdp_clk.c, "fd922800.qcom,mdss_dsi"),
	CLK_LOOKUP("mdp_core_clk", mdss_mdp_clk.c, "fd922e00.qcom,mdss_dsi"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP("alt_iface_clk", mdss_hdmi_ahb_clk.c,
		"fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP("core_clk", mdss_hdmi_clk.c, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP("mdp_core_clk", mdss_mdp_clk.c, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP("extp_clk", mdss_extpclk_clk.c, "fd922100.qcom,hdmi_tx"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "mdp.0"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "mdp.0"),
	CLK_LOOKUP("core_clk_src", mdp_clk_src.c, "mdp.0"),
	CLK_LOOKUP("vsync_clk", mdss_vsync_clk.c, "mdp.0"),

	/* MM sensor clocks */
	CLK_LOOKUP("cam_src_clk", mmss_gp0_clk_src.c, "6e.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mmss_gp0_clk_src.c, "20.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", gp1_clk_src.c, "6c.qcom,camera"),
	CLK_LOOKUP("cam_src_clk", mmss_gp1_clk_src.c, "90.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_gp0_clk.c, "6e.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_gp0_clk.c, "20.qcom,camera"),
	CLK_LOOKUP("cam_clk", gcc_gp1_clk.c, "6c.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_gp1_clk.c, "90.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk1_clk.c, ""),
	CLK_LOOKUP("cam_clk", camss_mclk2_clk.c, ""),
	CLK_LOOKUP("cam_clk", camss_mclk3_clk.c, ""),
	CLK_LOOKUP("cam_gp0_src_clk", mmss_gp0_clk_src.c, ""),
	CLK_LOOKUP("cam_gp1_src_clk", mmss_gp1_clk_src.c, ""),
	CLK_LOOKUP("cam_gp0_clk", camss_gp0_clk.c, ""),
	CLK_LOOKUP("cam_gp1_clk", camss_gp1_clk.c, ""),
	/* CCI clocks */
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
		"fda0c000.qcom,cci"),
	CLK_LOOKUP("cci_ahb_clk", camss_cci_cci_ahb_clk.c, "fda0c000.qcom,cci"),
	CLK_LOOKUP("cci_src_clk", cci_clk_src.c, "fda0c000.qcom,cci"),
	CLK_LOOKUP("cci_clk", camss_cci_cci_clk.c, "fda0c000.qcom,cci"),
	/* CSIPHY clocks */
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_src_clk", csi0phytimer_clk_src.c,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_clk", camss_phy0_csi0phytimer_clk.c,
		"fda0ac00.qcom,csiphy"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_src_clk", csi1phytimer_clk_src.c,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_clk", camss_phy1_csi1phytimer_clk.c,
		"fda0b000.qcom,csiphy"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
		"fda0b400.qcom,csiphy"),
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
		"fda0b400.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_src_clk", csi2phytimer_clk_src.c,
		"fda0b400.qcom,csiphy"),
	CLK_LOOKUP("csiphy_timer_clk", camss_phy2_csi2phytimer_clk.c,
		"fda0b400.qcom,csiphy"),

	/* CSID clocks */
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi0_ahb_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi0_clk_src.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi0phy_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi0_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi0pix_clk.c,
					"fda08000.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi0rdi_clk.c,
					"fda08000.qcom,csid"),

	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi1_ahb_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi1_clk_src.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi1phy_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi1_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi1pix_clk.c,
					"fda08400.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi1rdi_clk.c,
					"fda08400.qcom,csid"),

	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi2_ahb_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi2_clk_src.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi2phy_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi2_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi2pix_clk.c,
					"fda08800.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi2rdi_clk.c,
					"fda08800.qcom,csid"),

	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("csi_ahb_clk", camss_csi3_ahb_clk.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("csi_src_clk", csi3_clk_src.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("csi_phy_clk", camss_csi3phy_clk.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("csi_clk", camss_csi3_clk.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("csi_pix_clk", camss_csi3pix_clk.c,
					"fda08c00.qcom,csid"),
	CLK_LOOKUP("csi_rdi_clk", camss_csi3rdi_clk.c,
					"fda08c00.qcom,csid"),

	/* ISPIF clocks */
	CLK_LOOKUP("ispif_ahb_clk", camss_ispif_ahb_clk.c,
		"fda0a000.qcom,ispif"),

	CLK_LOOKUP("vfe0_clk_src", vfe0_clk_src.c, "fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_vfe_vfe0_clk", camss_vfe_vfe0_clk.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_csi_vfe0_clk", camss_csi_vfe0_clk.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("vfe1_clk_src", vfe1_clk_src.c, "fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_vfe_vfe1_clk", camss_vfe_vfe1_clk.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("camss_csi_vfe1_clk", camss_csi_vfe1_clk.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_src_clk", csi0_clk_src.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_clk", camss_csi0_clk.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_pix_clk", camss_csi0pix_clk.c,
			   "fda0a000.qcom,ispif"),
	CLK_LOOKUP("csi0_rdi_clk", camss_csi0rdi_clk.c,
			   "fda0a000.qcom,ispif"),

	/*VFE clocks*/
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP("vfe_clk_src", vfe0_clk_src.c,	 "fda10000.qcom,vfe"),
	CLK_LOOKUP("camss_vfe_vfe_clk", camss_vfe_vfe0_clk.c,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP("camss_csi_vfe_clk", camss_csi_vfe0_clk.c,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c, "fda10000.qcom,vfe"),
	CLK_LOOKUP("bus_clk", camss_vfe_vfe_axi_clk.c,	 "fda10000.qcom,vfe"),
	CLK_LOOKUP("alt_bus_clk", camss_vfe_vfe_ocmemnoc_clk.c,
					"fda10000.qcom,vfe"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
					"fda14000.qcom,vfe"),
	CLK_LOOKUP("vfe_clk_src", vfe1_clk_src.c,	 "fda14000.qcom,vfe"),
	CLK_LOOKUP("camss_vfe_vfe_clk", camss_vfe_vfe1_clk.c,
					"fda14000.qcom,vfe"),
	CLK_LOOKUP("camss_csi_vfe_clk", camss_csi_vfe1_clk.c,
					"fda14000.qcom,vfe"),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c, "fda14000.qcom,vfe"),
	CLK_LOOKUP("bus_clk", camss_vfe_vfe_axi_clk.c,	 "fda14000.qcom,vfe"),
	CLK_LOOKUP("alt_bus_clk", camss_vfe_vfe_ocmemnoc_clk.c,
					"fda14000.qcom,vfe"),
	/*Jpeg Clocks*/
	CLK_LOOKUP("core_clk", camss_jpeg_jpeg0_clk.c, "fda1c000.qcom,jpeg"),
	CLK_LOOKUP("core_clk", camss_jpeg_jpeg1_clk.c, "fda20000.qcom,jpeg"),
	CLK_LOOKUP("core_clk", camss_jpeg_jpeg2_clk.c, "fda24000.qcom,jpeg"),
	CLK_LOOKUP("iface_clk", camss_jpeg_jpeg_ahb_clk.c,
						"fda1c000.qcom,jpeg"),
	CLK_LOOKUP("iface_clk", camss_jpeg_jpeg_ahb_clk.c,
						"fda20000.qcom,jpeg"),
	CLK_LOOKUP("iface_clk", camss_jpeg_jpeg_ahb_clk.c,
						"fda24000.qcom,jpeg"),
	CLK_LOOKUP("iface_clk", camss_jpeg_jpeg_ahb_clk.c,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP("core_clk", camss_jpeg_jpeg_axi_clk.c,
						"fda64000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", camss_top_ahb_clk.c, "fda64000.qcom,iommu"),
	CLK_LOOKUP("bus_clk0", camss_jpeg_jpeg_axi_clk.c, "fda1c000.qcom,jpeg"),
	CLK_LOOKUP("bus_clk0", camss_jpeg_jpeg_axi_clk.c, "fda20000.qcom,jpeg"),
	CLK_LOOKUP("bus_clk0", camss_jpeg_jpeg_axi_clk.c, "fda24000.qcom,jpeg"),
	CLK_LOOKUP("alt_bus_clk", camss_jpeg_jpeg_ocmemnoc_clk.c,
						"fda1c000.qcom,jpeg"),
	CLK_LOOKUP("alt_bus_clk", camss_jpeg_jpeg_ocmemnoc_clk.c,
						"fda20000.qcom,jpeg"),
	CLK_LOOKUP("alt_bus_clk", camss_jpeg_jpeg_ocmemnoc_clk.c,
						"fda24000.qcom,jpeg"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
						"fda1c000.qcom,jpeg"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
						"fda20000.qcom,jpeg"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
						"fda24000.qcom,jpeg"),
	CLK_LOOKUP("micro_iface_clk", camss_micro_ahb_clk.c,
		"fda04000.qcom,cpp"),
	CLK_LOOKUP("camss_top_ahb_clk", camss_top_ahb_clk.c,
		"fda04000.qcom,cpp"),
	CLK_LOOKUP("cpp_iface_clk", camss_vfe_cpp_ahb_clk.c,
		"fda04000.qcom,cpp"),
	CLK_LOOKUP("cpp_core_clk", camss_vfe_cpp_clk.c, "fda04000.qcom,cpp"),
	CLK_LOOKUP("cpp_bus_clk", camss_vfe_vfe_axi_clk.c, "fda04000.qcom,cpp"),
	CLK_LOOKUP("vfe_clk_src", vfe0_clk_src.c,	 "fda04000.qcom,cpp"),
	CLK_LOOKUP("camss_vfe_vfe_clk", camss_vfe_vfe0_clk.c,
					"fda04000.qcom,cpp"),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c, "fda04000.qcom,cpp"),


	CLK_LOOKUP("iface_clk", camss_micro_ahb_clk.c, ""),
	CLK_LOOKUP("iface_clk", camss_vfe_vfe_ahb_clk.c, "fda44000.qcom,iommu"),
	CLK_LOOKUP("core_clk", camss_vfe_vfe_axi_clk.c, "fda44000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", camss_top_ahb_clk.c, "fda44000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "mdp.0"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd923400.qcom,mdss_edp"),
	CLK_LOOKUP("iface_clk", mdss_ahb_clk.c, "fd928000.qcom,iommu"),
	CLK_LOOKUP("core_clk", mdss_axi_clk.c, "fd928000.qcom,iommu"),
	CLK_LOOKUP("bus_clk", mdss_axi_clk.c, "mdp.0"),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP("iface_clk", oxilicx_ahb_clk.c, "fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP("mem_iface_clk", ocmemcx_ocmemnoc_clk.c,
						"fdb00000.qcom,kgsl-3d0"),
	CLK_LOOKUP("core_clk", oxilicx_axi_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("iface_clk", oxilicx_ahb_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", oxili_gfx3d_clk.c, "fdb10000.qcom,iommu"),
	CLK_LOOKUP("core_clk", ocmemgx_core_clk.c, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP("iface_clk", ocmemcx_ocmemnoc_clk.c, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP("iface_clk", venus0_ahb_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("alt_core_clk", venus0_vcodec0_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("core_clk", venus0_axi_clk.c, "fdc84000.qcom,iommu"),
	CLK_LOOKUP("bus_clk", venus0_axi_clk.c, ""),
	CLK_LOOKUP("src_clk",  vcodec0_clk_src.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("core_clk", venus0_vcodec0_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("iface_clk",  venus0_ahb_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("bus_clk",  venus0_axi_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("mem_clk",  venus0_ocmemnoc_clk.c, "fdce0000.qcom,venus"),
	CLK_LOOKUP("core_clk", venus0_vcodec0_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("iface_clk",  venus0_ahb_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("bus_clk",  venus0_axi_clk.c, "fdc00000.qcom,vidc"),
	CLK_LOOKUP("mem_clk",  venus0_ocmemnoc_clk.c, "fdc00000.qcom,vidc"),

	CLK_LOOKUP("core_clk", venus0_vcodec0_clk.c, "fd8c1024.qcom,gdsc"),
	CLK_LOOKUP("core_clk", mdss_mdp_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("lut_clk", mdss_mdp_lut_clk.c, "fd8c2304.qcom,gdsc"),
	CLK_LOOKUP("core0_clk", camss_jpeg_jpeg0_clk.c, "fd8c35a4.qcom,gdsc"),
	CLK_LOOKUP("core1_clk", camss_jpeg_jpeg1_clk.c, "fd8c35a4.qcom,gdsc"),
	CLK_LOOKUP("core2_clk", camss_jpeg_jpeg2_clk.c, "fd8c35a4.qcom,gdsc"),
	CLK_LOOKUP("core0_clk", camss_vfe_vfe0_clk.c,	"fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("core1_clk", camss_vfe_vfe1_clk.c,	"fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("csi0_clk", camss_csi_vfe0_clk.c,	"fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("csi1_clk", camss_csi_vfe1_clk.c,	"fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("cpp_clk", camss_vfe_cpp_clk.c,	"fd8c36a4.qcom,gdsc"),
	CLK_LOOKUP("core_clk", oxili_gfx3d_clk.c, "fd8c4024.qcom,gdsc"),

	/* LPASS clocks */
	CLK_LOOKUP("bus_clk", gcc_mss_q6_bimc_axi_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("iface_clk", gcc_mss_cfg_ahb_clk.c, "fc880000.qcom,mss"),
	CLK_LOOKUP("mem_clk", gcc_boot_rom_ahb_clk.c,  "fc880000.qcom,mss"),

	CLK_LOOKUP("core_clk",         q6ss_xo_clk.c,  "fe200000.qcom,lpass"),
	CLK_LOOKUP("bus_clk", gcc_lpass_q6_axi_clk.c,  "fe200000.qcom,lpass"),
	CLK_LOOKUP("iface_clk", q6ss_ahb_lfabif_clk.c, "fe200000.qcom,lpass"),
	CLK_LOOKUP("reg_clk",        q6ss_ahbm_clk.c,  "fe200000.qcom,lpass"),
	CLK_LOOKUP("core_clk", gcc_prng_ahb_clk.c, "msm_rng"),

	CLK_LOOKUP("dfab_clk", pnoc_sps_clk.c, "msm_sps"),
	CLK_LOOKUP("bus_clk", pnoc_keepalive_a_clk.c, ""),

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
	CLK_LOOKUP("bus_clk",	ocmemnoc_clk.c,		"msm_ocmem_noc"),
	CLK_LOOKUP("bus_a_clk",	ocmemnoc_clk.c,		"msm_ocmem_noc"),
	CLK_LOOKUP("bus_clk",	mmss_s0_axi_clk.c,	"msm_mmss_noc"),
	CLK_LOOKUP("bus_a_clk",	mmss_s0_axi_clk.c,	"msm_mmss_noc"),
	CLK_LOOKUP("iface_clk", gcc_mmss_noc_cfg_ahb_clk.c, ""),
	CLK_LOOKUP("iface_clk", gcc_ocmem_noc_cfg_ahb_clk.c, ""),

	/* CoreSight clocks */
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc322000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc318000.tpiu"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc31c000.replicator"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc307000.tmc"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc31b000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc319000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc31a000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc345000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc364000.funnel"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc321000.stm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc33c000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc33d000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc33e000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc33f000.etm"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc308000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc309000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc30a000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc30b000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc30c000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc30d000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc30e000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc30f000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc340000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc341000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc342000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc343000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fc344000.cti"),
	CLK_LOOKUP("core_clk", qdss_clk.c, "fdf30018.hwevent"),

	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc322000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc318000.tpiu"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc31c000.replicator"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc307000.tmc"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc31b000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc319000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc31a000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc345000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc364000.funnel"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc321000.stm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc33c000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc33d000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc33e000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc33f000.etm"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc308000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc309000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc30a000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc30b000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc30c000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc30d000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc30e000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc30f000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc310000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc340000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc341000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc342000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc343000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fc344000.cti"),
	CLK_LOOKUP("core_a_clk", qdss_a_clk.c, "fdf30018.hwevent"),

	CLK_LOOKUP("core_mmss_clk", mmss_misc_ahb_clk.c, "fdf30018.hwevent"),

	/* DSI PLL clocks */
	CLK_LOOKUP("",		dsi_vco_clk_8974.c,                  ""),
	CLK_LOOKUP("",		analog_postdiv_clk_8974.c,         ""),
	CLK_LOOKUP("",		indirect_path_div2_clk_8974.c,     ""),
	CLK_LOOKUP("",		pixel_clk_src_8974.c,              ""),
	CLK_LOOKUP("",		byte_mux_8974.c,                   ""),
	CLK_LOOKUP("",		byte_clk_src_8974.c,               ""),
};

static struct clk_lookup msm_clocks_8974[ARRAY_SIZE(msm_clocks_8974_common)
	+ ARRAY_SIZE(msm_clocks_8974ac_only)];

static struct pll_config_regs mmpll0_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL0_L_REG,
	.m_reg = (void __iomem *)MMPLL0_M_REG,
	.n_reg = (void __iomem *)MMPLL0_N_REG,
	.config_reg = (void __iomem *)MMPLL0_USER_CTL_REG,
	.mode_reg = (void __iomem *)MMPLL0_MODE_REG,
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
	.l_reg = (void __iomem *)MMPLL1_L_REG,
	.m_reg = (void __iomem *)MMPLL1_M_REG,
	.n_reg = (void __iomem *)MMPLL1_N_REG,
	.config_reg = (void __iomem *)MMPLL1_USER_CTL_REG,
	.mode_reg = (void __iomem *)MMPLL1_MODE_REG,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL1 at 846 MHz, main output enabled. */
static struct pll_config mmpll1_config __initdata = {
	.l = 0x2C,
	.m = 0x1,
	.n = 0x10,
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

/* MMPLL1 at 1167 MHz, main output enabled. */
static struct pll_config mmpll1_v2_config __initdata = {
	.l = 60,
	.m = 25,
	.n = 32,
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
	.l_reg = (void __iomem *)MMPLL3_L_REG,
	.m_reg = (void __iomem *)MMPLL3_M_REG,
	.n_reg = (void __iomem *)MMPLL3_N_REG,
	.config_reg = (void __iomem *)MMPLL3_USER_CTL_REG,
	.mode_reg = (void __iomem *)MMPLL3_MODE_REG,
	.base = &virt_bases[MMSS_BASE],
};

/* MMPLL3 at 820 MHz, main output enabled. */
static struct pll_config mmpll3_config __initdata = {
	.l = 0x2A,
	.m = 0x11,
	.n = 0x18,
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

/* MMPLL3 at 930 MHz, main output enabled. */
static struct pll_config mmpll3_v2_config __initdata = {
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
	.aux_output_val = BIT(1),
	.aux_output_mask = BIT(1),
};

#define cpu_is_msm8974pro() (cpu_is_msm8974pro_aa() || cpu_is_msm8974pro_ab() \
			     || cpu_is_msm8974pro_ac())

static void __init reg_init(void)
{
	u32 regval;

	configure_sr_hpm_lp_pll(&mmpll0_config, &mmpll0_regs, 1);

	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2
	    || cpu_is_msm8974pro()) {
		configure_sr_hpm_lp_pll(&mmpll1_v2_config, &mmpll1_regs, 1);
		configure_sr_hpm_lp_pll(&mmpll3_v2_config, &mmpll3_regs, 0);
	} else {
		configure_sr_hpm_lp_pll(&mmpll1_config, &mmpll1_regs, 1);
		configure_sr_hpm_lp_pll(&mmpll3_config, &mmpll3_regs, 0);
	}

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE_REG));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE_REG));

	/*
	 * V2 requires additional votes to allow the LPASS and MMSS
	 * controllers to use GPLL0.
	 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2
	    || cpu_is_msm8974pro()) {
		regval = readl_relaxed(
				GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
		writel_relaxed(regval | BIT(26) | BIT(25),
				GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
	}
}

static void __init msm8974_clock_post_init(void)
{
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2
	    || cpu_is_msm8974pro()) {
		clk_set_rate(&axi_clk_src.c, 291750000);
		clk_set_rate(&ocmemnoc_clk_src.c, 291750000);
	} else {
		clk_set_rate(&axi_clk_src.c, 282000000);
		clk_set_rate(&ocmemnoc_clk_src.c, 282000000);
	}

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
	clk_prepare_enable(&cxo_a_clk_src.c);

	/*
	 * TODO: Temporarily enable NOC configuration AHB clocks. Remove when
	 * the bus driver is ready.
	 */
	clk_prepare_enable(&gcc_mmss_noc_cfg_ahb_clk.c);
	clk_prepare_enable(&gcc_ocmem_noc_cfg_ahb_clk.c);

	/* Set rates for single-rate clocks. */
	clk_set_rate(&usb30_master_clk_src.c,
			usb30_master_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&tsif_ref_clk_src.c,
			tsif_ref_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hs_system_clk_src.c,
			usb_hs_system_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_clk_src.c,
			usb_hsic_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_io_cal_clk_src.c,
			usb_hsic_io_cal_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb_hsic_system_clk_src.c,
			usb_hsic_system_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&usb30_mock_utmi_clk_src.c,
			usb30_mock_utmi_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&pdm2_clk_src.c, pdm2_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&cci_clk_src.c, cci_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&mclk0_clk_src.c, mclk0_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&mclk1_clk_src.c, mclk1_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&mclk2_clk_src.c, mclk2_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&edpaux_clk_src.c, edpaux_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&esc0_clk_src.c, esc0_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&esc1_clk_src.c, esc1_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&hdmi_clk_src.c, hdmi_clk_src.freq_tbl[0].freq_hz);
	clk_set_rate(&vsync_clk_src.c, vsync_clk_src.freq_tbl[0].freq_hz);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_16K

#define MMSS_CC_PHYS		0xFD8C0000
#define MMSS_CC_SIZE		SZ_256K

#define LPASS_CC_PHYS		0xFE000000
#define LPASS_CC_SIZE		SZ_256K

#define APCS_GCC_CC_PHYS	0xF9011000
#define APCS_GCC_CC_SIZE	SZ_4K

static struct clk *qup_i2c_clks[][2] __initdata = {
	{&gcc_blsp1_qup1_i2c_apps_clk.c, &blsp1_qup1_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup2_i2c_apps_clk.c, &blsp1_qup2_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup3_i2c_apps_clk.c, &blsp1_qup3_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup4_i2c_apps_clk.c, &blsp1_qup4_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup5_i2c_apps_clk.c, &blsp1_qup5_i2c_apps_clk_src.c,},
	{&gcc_blsp1_qup6_i2c_apps_clk.c, &blsp1_qup6_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup1_i2c_apps_clk.c, &blsp2_qup1_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup2_i2c_apps_clk.c, &blsp2_qup2_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup3_i2c_apps_clk.c, &blsp2_qup3_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup4_i2c_apps_clk.c, &blsp2_qup4_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup5_i2c_apps_clk.c, &blsp2_qup5_i2c_apps_clk_src.c,},
	{&gcc_blsp2_qup6_i2c_apps_clk.c, &blsp2_qup6_i2c_apps_clk_src.c,},
};

/* v1 to v2 clock changes */
static void __init msm8974_v2_clock_override(void)
{
	int i;

	mmpll3_clk_src.c.rate =  930000000;
	mmpll1_clk_src.c.rate = 1167000000;
	mmpll1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 1167000000;

	ocmemnoc_clk_src.freq_tbl = ftbl_ocmemnoc_v2_clk;
	ocmemnoc_clk_src.c.fmax[VDD_DIG_NOMINAL] = 291750000;

	axi_clk_src.freq_tbl = ftbl_mmss_axi_v2_clk;
	axi_clk_src.c.fmax[VDD_DIG_NOMINAL] = 291750000;
	axi_clk_src.c.fmax[VDD_DIG_HIGH] = 466800000;

	vcodec0_clk_src.freq_tbl = ftbl_venus0_vcodec0_v2_clk;
	vcodec0_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;

	mdp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 240000000;

	/* The parent of each of the QUP I2C clocks is an RCG on V2 */
	for (i = 0; i < ARRAY_SIZE(qup_i2c_clks); i++)
		qup_i2c_clks[i][0]->parent =  qup_i2c_clks[i][1];
}

/* v2 to pro clock changes */
static void __init msm8974_pro_clock_override(void)
{
	ce1_clk_src.c.fmax[VDD_DIG_LOW] = 75000000;
	ce1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 150000000;
	ce1_clk_src.freq_tbl = ftbl_gcc_ce1_pro_clk;
	ce2_clk_src.c.fmax[VDD_DIG_LOW] = 75000000;
	ce2_clk_src.c.fmax[VDD_DIG_NOMINAL] = 150000000;
	ce2_clk_src.freq_tbl = ftbl_gcc_ce2_pro_clk;

	if (cpu_is_msm8974pro_ac()) {
		sdcc1_apps_clk_src.c.fmax[VDD_DIG_LOW] = 200000000;
		sdcc1_apps_clk_src.c.fmax[VDD_DIG_NOMINAL] = 400000000;
	}

	vfe0_clk_src.c.fmax[VDD_DIG_LOW] = 150000000;
	vfe0_clk_src.c.fmax[VDD_DIG_NOMINAL] = 320000000;
	vfe1_clk_src.c.fmax[VDD_DIG_LOW] = 150000000;
	vfe1_clk_src.c.fmax[VDD_DIG_NOMINAL] = 320000000;
	cpp_clk_src.c.fmax[VDD_DIG_LOW] = 150000000;
	cpp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 320000000;

	if (cpu_is_msm8974pro_ab() || cpu_is_msm8974pro_ac()) {
		vfe0_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;
		vfe1_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;
		cpp_clk_src.c.fmax[VDD_DIG_HIGH] = 465000000;
	} else if (cpu_is_msm8974pro_aa()) {
		vfe0_clk_src.c.fmax[VDD_DIG_HIGH] = 320000000;
		vfe1_clk_src.c.fmax[VDD_DIG_HIGH] = 320000000;
		cpp_clk_src.c.fmax[VDD_DIG_HIGH] = 320000000;
	}

	mdp_clk_src.c.fmax[VDD_DIG_NOMINAL] = 266670000;

	mclk0_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk1_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk2_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk3_clk_src.freq_tbl = ftbl_camss_mclk0_3_pro_clk;
	mclk0_clk_src.set_rate = set_rate_mnd;
	mclk1_clk_src.set_rate = set_rate_mnd;
	mclk2_clk_src.set_rate = set_rate_mnd;
	mclk3_clk_src.set_rate = set_rate_mnd;
	mclk0_clk_src.c.ops = &clk_ops_rcg_mnd;
	mclk1_clk_src.c.ops = &clk_ops_rcg_mnd;
	mclk2_clk_src.c.ops = &clk_ops_rcg_mnd;
	mclk3_clk_src.c.ops = &clk_ops_rcg_mnd;
}

static void __init msm8974_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-8974: Unable to ioremap GCC memory!");

	virt_bases[MMSS_BASE] = ioremap(MMSS_CC_PHYS, MMSS_CC_SIZE);
	if (!virt_bases[MMSS_BASE])
		panic("clock-8974: Unable to ioremap MMSS_CC memory!");

	virt_bases[LPASS_BASE] = ioremap(LPASS_CC_PHYS, LPASS_CC_SIZE);
	if (!virt_bases[LPASS_BASE])
		panic("clock-8974: Unable to ioremap LPASS_CC memory!");

	virt_bases[APCS_BASE] = ioremap(APCS_GCC_CC_PHYS, APCS_GCC_CC_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-8974: Unable to ioremap APCS_GCC_CC memory!");

	clk_ops_local_pll.enable = sr_hpm_lp_pll_clk_enable;

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-8974: Unable to get the vdd_dig regulator!");

	enable_rpm_scaling();

	reg_init();

	memcpy(msm_clocks_8974, msm_clocks_8974_common,
	       sizeof(msm_clocks_8974_common));
	msm8974_clock_init_data.size -= ARRAY_SIZE(msm_clocks_8974ac_only);

	/* version specific changes */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 2
	    || cpu_is_msm8974pro())
		msm8974_v2_clock_override();
	if (cpu_is_msm8974pro()) {
		msm8974_pro_clock_override();
		memcpy(msm_clocks_8974 + ARRAY_SIZE(msm_clocks_8974_common),
		       msm_clocks_8974ac_only, sizeof(msm_clocks_8974ac_only));
		msm8974_clock_init_data.size +=
			ARRAY_SIZE(msm_clocks_8974ac_only);
	}

	/*
	 * MDSS needs the ahb clock and needs to init before we register the
	 * lookup table.
	 */
	mdss_clk_ctrl_pre_init(&mdss_ahb_clk.c);
}

static void __init msm8974_rumi_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-8974: Unable to ioremap GCC memory!");

	/* SDCC clocks are partially emulated in the RUMI */
	sdcc1_apps_clk_src.freq_tbl = ftbl_gcc_sdcc_apps_rumi_clk;
	sdcc2_apps_clk_src.freq_tbl = ftbl_gcc_sdcc_apps_rumi_clk;
	sdcc3_apps_clk_src.freq_tbl = ftbl_gcc_sdcc_apps_rumi_clk;
	sdcc4_apps_clk_src.freq_tbl = ftbl_gcc_sdcc_apps_rumi_clk;

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-8974: Unable to get the vdd_dig regulator!");
}

struct clock_init_data msm8974_clock_init_data __initdata = {
	.table = msm_clocks_8974,
	.size = ARRAY_SIZE(msm_clocks_8974),
	.pre_init = msm8974_clock_pre_init,
	.post_init = msm8974_clock_post_init,
};

struct clock_init_data msm8974_rumi_clock_init_data __initdata = {
	.table = msm_clocks_8974_rumi,
	.size = ARRAY_SIZE(msm_clocks_8974_rumi),
	.pre_init = msm8974_rumi_clock_pre_init,
};
