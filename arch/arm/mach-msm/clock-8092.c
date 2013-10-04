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

enum {
	GCC_BASE,
	MMSS_BASE,
	LPASS_BASE,
	APCS_BASE,
	BCSS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define GCC_REG_BASE(x) (void __iomem *)(virt_bases[GCC_BASE] + (x))
#define MMSS_REG_BASE(x) (void __iomem *)(virt_bases[MMSS_BASE] + (x))
#define LPASS_REG_BASE(x) (void __iomem *)(virt_bases[LPASS_BASE] + (x))
#define APCS_REG_BASE(x) (void __iomem *)(virt_bases[APCS_BASE] + (x))
#define BCSS_REG_BASE(x) (void __iomem *)(virt_bases[BCSS_BASE] + (x))

#define GPLL0_MODE				0x0000
#define GPLL0_L_VAL				0x0004
#define GPLL0_M_VAL				0x0008
#define GPLL0_N_VAL				0x000C
#define GPLL0_USER_CTL				0x0010
#define GPLL0_STATUS				0x001C
#define GPLL1_MODE				0x0040
#define GPLL1_L_VAL				0x0044
#define GPLL1_M_VAL				0x0048
#define GPLL1_N_VAL				0x004C
#define GPLL1_USER_CTL				0x0050
#define GPLL1_STATUS				0x005C
#define MMPLL0_MODE				0x0000
#define MMPLL0_L_VAL				0x0004
#define MMPLL0_M_VAL				0x0008
#define MMPLL0_N_VAL				0x000C
#define MMPLL0_USER_CTL				0x0010
#define MMPLL0_STATUS				0x001C
#define MMPLL1_MODE				0x0040
#define MMPLL1_L_VAL				0x0044
#define MMPLL1_M_VAL				0x0048
#define MMPLL1_N_VAL				0x004C
#define MMPLL1_USER_CTL				0x0050
#define MMPLL1_STATUS				0x005C
#define MMPLL3_MODE				0x0080
#define MMPLL3_L_VAL				0x0084
#define MMPLL3_M_VAL				0x0088
#define MMPLL3_N_VAL				0x008C
#define MMPLL3_USER_CTL				0x0090
#define MMPLL3_STATUS				0x009C
#define MMPLL6_MODE				0x00E0
#define MMPLL6_L_VAL				0x00E4
#define MMPLL6_M_VAL				0x00E8
#define MMPLL6_N_VAL				0x00EC
#define MMPLL6_USER_CTL				0x00F0
#define MMPLL6_STATUS				0x00FC
#define MMSS_PLL_VOTE_APCS			0x0100
#define VCODEC0_CMD_RCGR			0x1000
#define VENUS0_VCODEC0_CBCR			0x1028
#define VENUS0_CORE0_VCODEC_CBCR		0x1048
#define VENUS0_CORE1_VCODEC_CBCR		0x104C
#define VENUS0_AHB_CBCR				0x1030
#define VENUS0_AXI_CBCR				0x1034
#define VENUS0_OCMEMNOC_CBCR			0x1038
#define GPROC_CMD_RCGR				0x1200
#define KPROC_CMD_RCGR				0x1220
#define SDMC_FRCS_CMD_RCGR			0x1240
#define SDME_FRCF_CMD_RCGR			0x1260
#define SDME_VPROC_CMD_RCGR			0x12A0
#define HDMC_FRCF_CMD_RCGR			0x12C0
#define PREPROC_CMD_RCGR			0x12E0
#define VDP_CMD_RCGR				0x1300
#define MAPLE_CMD_RCGR				0x1320
#define VPU_BUS_CMD_RCGR			0x1340
#define VPU_VDP_XIN_CMD_RCGR			0x1360
#define VPU_FRC_XIN_CMD_RCGR			0x1380
#define VPU_GPROC_CBCR				0x1408
#define VPU_KPROC_CBCR				0x140C
#define VPU_SDMC_FRCS_CBCR			0x1410
#define VPU_SDME_FRCF_CBCR			0x1414
#define VPU_SDME_FRCS_CBCR			0x1418
#define VPU_SDME_VPROC_CBCR			0x141C
#define VPU_HDMC_FRCF_CBCR			0x1420
#define VPU_PREPROC_CBCR			0x1424
#define VPU_VDP_CBCR				0x1428
#define VPU_MAPLE_CBCR				0x142C
#define VPU_AHB_CBCR				0x1430
#define VPU_CXO_CBCR				0x1434
#define VPU_SLEEP_CBCR				0x1438
#define VPU_AXI_CBCR				0x143C
#define VPU_BUS_CBCR				0x1440
#define VPU_VDP_XIN_CBCR			0x1444
#define VPU_FRC_XIN_CBCR			0x1448
#define HDMI_RX_CMD_RCGR			0x1600
#define HDMI_BUS_CMD_RCGR			0x1620
#define MD_CMD_RCGR				0x1640
#define CFG_CMD_RCGR				0x1660
#define AUDIO_CMD_RCGR				0x1680
#define AFE_PIXEL_CMD_RCGR			0x16A0
#define VAFE_EXT_CMD_RCGR			0x16C0
#define VCAP_VP_CMD_RCGR			0x16E0
#define TTL_CMD_RCGR				0x1700
#define VCAP_HDMI_RX_CBCR			0x1808
#define VCAP_HDMI_BUS_CBCR			0x180C
#define VCAP_MD_CBCR				0x1810
#define VCAP_CFG_CBCR				0x1814
#define VCAP_AUDIO_CBCR				0x1818
#define VCAP_AFE_PIXEL_CBCR			0x181C
#define VCAP_VAFE_EXT_CBCR			0x1820
#define VCAP_VP_CBCR				0x1824
#define VCAP_TTL_CBCR				0x1828
#define VCAP_AHB_CBCR				0x1830
#define VCAP_AXI_CBCR				0x1834
#define BCSS_MMSS_IFDEMOD_CBCR			0x1838
#define MDP_CMD_RCGR				0x2040
#define EXTPCLK_CMD_RCGR			0x2060
#define LVDS_CMD_RCGR				0x21C0
#define VBYONE_CMD_RCGR				0x21E0
#define VBYONE_SYMBOL_CMD_RCGR			0x2200
#define HDMI_CMD_RCGR				0x2100
#define MDSS_AHB_CBCR				0x2308
#define MDSS_HDMI_AHB_CBCR			0x230C
#define MDSS_AXI_CBCR				0x2310
#define MDSS_MDP_CBCR				0x231C
#define MDSS_MDP_LUT_CBCR			0x2320
#define MDSS_EXTPCLK_CBCR			0x2324
#define MDSS_LVDS_CBCR				0x2358
#define MDSS_VBYONE_CBCR			0x235C
#define MDSS_VBYONE_SYMBOL_CBCR			0x2360
#define MDSS_HDMI_CBCR				0x2338
#define VP_CMD_RCGR				0x2430
#define AVSYNC_VP_CBCR				0x2404
#define AVSYNC_VBYONE_CBCR			0x2408
#define AVSYNC_LVDS_CBCR			0x240C
#define AVSYNC_EXTPCLK_CBCR			0x2410
#define AVSYNC_AHB_CBCR				0x2414
#define CAMSS_TOP_AHB_CBCR			0x3484
#define CAMSS_MICRO_AHB_CBCR			0x3494
#define JPEG2_CMD_RCGR				0x3540
#define CAMSS_JPEG_JPEG2_CBCR			0x35B0
#define CAMSS_JPEG_JPEG_AHB_CBCR		0x35B4
#define CAMSS_JPEG_JPEG_AXI_CBCR		0x35B8
#define OXILI_GFX3D_CBCR			0x4028
#define OXILI_OCMEMGX_CBCR			0x402C
#define OXILICX_AHB_CBCR			0x403C
#define OCMEMCX_OCMEMNOC_CBCR			0x4058
#define OCMEMCX_AHB_CBCR			0x405C
#define MMSS_MMSSNOC_AHB_CBCR			0x5024
#define MMSS_MMSSNOC_BTO_AHB_CBCR		0x5028
#define MMSS_MISC_AHB_CBCR			0x502C
#define AXI_CMD_RCGR				0x5040
#define MMSS_S0_AXI_CBCR			0x5064
#define MMSS_MMSSNOC_AXI_CBCR			0x506C
#define OCMEMNOC_CMD_RCGR			0x5090
#define MMSS_DEBUG_CLK_CTL			0x0900
#define SYS_NOC_USB3_AXI_CBCR			0x0108
#define MMSS_NOC_CFG_AHB_CBCR			0x024C
#define MMSS_A5SS_AXI_CBCR			0x0254
#define USB30_MASTER_CBCR			0x03C8
#define USB30_SLEEP_CBCR			0x03CC
#define USB30_MOCK_UTMI_CBCR			0x03D0
#define USB30_MASTER_CMD_RCGR			0x03D4
#define USB30_MOCK_UTMI_CMD_RCGR		0x03E8
#define USB_HS_HSIC_BCR				0x0400
#define USB_HSIC_AHB_CBCR			0x0408
#define USB_HSIC_SYSTEM_CMD_RCGR		0x041C
#define USB_HSIC_SYSTEM_CBCR			0x040C
#define USB_HSIC_CMD_RCGR			0x0440
#define USB_HSIC_CBCR				0x0410
#define USB_HSIC_IO_CAL_CMD_RCGR		0x0458
#define USB_HSIC_IO_CAL_CBCR			0x0414
#define USB_HSIC_IO_CAL_SLEEP_CBCR		0x0418
#define USB_HS_BCR				0x0480
#define USB_HS_SYSTEM_CBCR			0x0484
#define USB_HS_AHB_CBCR				0x0488
#define USB_HS_SYSTEM_CMD_RCGR			0x0490
#define USB2A_PHY_SLEEP_CBCR			0x04AC
#define USB2B_PHY_SLEEP_CBCR			0x04B4
#define SDCC1_APPS_CMD_RCGR			0x04D0
#define SDCC1_APPS_CBCR				0x04C4
#define SDCC1_AHB_CBCR				0x04C8
#define SDCC2_APPS_CMD_RCGR			0x0510
#define SDCC2_APPS_CBCR				0x0504
#define SDCC2_AHB_CBCR				0x0508
#define BLSP1_AHB_CBCR				0x05C4
#define BLSP1_QUP1_SPI_APPS_CBCR		0x0644
#define BLSP1_QUP1_I2C_APPS_CBCR		0x0648
#define BLSP1_QUP1_SPI_APPS_CMD_RCGR		0x064C
#define BLSP1_QUP1_I2C_APPS_CMD_RCGR		0x0660
#define BLSP1_UART1_APPS_CBCR			0x0684
#define BLSP1_UART1_APPS_CMD_RCGR		0x068C
#define BLSP1_QUP2_SPI_APPS_CBCR		0x06C4
#define BLSP1_QUP2_I2C_APPS_CBCR		0x06C8
#define BLSP1_QUP2_SPI_APPS_CMD_RCGR		0x06CC
#define BLSP1_QUP2_I2C_APPS_CMD_RCGR		0x06E0
#define BLSP1_UART2_APPS_CBCR			0x0704
#define BLSP1_UART2_APPS_CMD_RCGR		0x070C
#define BLSP1_QUP3_SPI_APPS_CBCR		0x0744
#define BLSP1_QUP3_I2C_APPS_CBCR		0x0748
#define BLSP1_QUP3_SPI_APPS_CMD_RCGR		0x074C
#define BLSP1_QUP3_I2C_APPS_CMD_RCGR		0x0760
#define BLSP1_UART3_APPS_CBCR			0x0784
#define BLSP1_UART3_APPS_CMD_RCGR		0x078C
#define BLSP1_QUP4_SPI_APPS_CBCR		0x07C4
#define BLSP1_QUP4_I2C_APPS_CBCR		0x07C8
#define BLSP1_QUP4_SPI_APPS_CMD_RCGR		0x07CC
#define BLSP1_QUP4_I2C_APPS_CMD_RCGR		0x07E0
#define BLSP1_UART4_APPS_CBCR			0x0804
#define BLSP1_UART4_APPS_CMD_RCGR		0x080C
#define BLSP1_QUP5_SPI_APPS_CBCR		0x0844
#define BLSP1_QUP5_I2C_APPS_CBCR		0x0848
#define BLSP1_QUP5_SPI_APPS_CMD_RCGR		0x084C
#define BLSP1_QUP5_I2C_APPS_CMD_RCGR		0x0860
#define BLSP1_UART5_APPS_CBCR			0x0884
#define BLSP1_UART5_APPS_CMD_RCGR		0x088C
#define BLSP1_QUP6_SPI_APPS_CBCR		0x08C4
#define BLSP1_QUP6_I2C_APPS_CBCR		0x08C8
#define BLSP1_QUP6_SPI_APPS_CMD_RCGR		0x08CC
#define BLSP1_QUP6_I2C_APPS_CMD_RCGR		0x08E0
#define BLSP1_UART6_APPS_CBCR			0x0904
#define BLSP1_UART6_APPS_CMD_RCGR		0x090C
#define BLSP2_AHB_CBCR				0x0944
#define BLSP2_QUP1_SPI_APPS_CBCR		0x0984
#define BLSP2_QUP1_I2C_APPS_CBCR		0x0988
#define BLSP2_QUP1_SPI_APPS_CMD_RCGR		0x098C
#define BLSP2_QUP1_I2C_APPS_CMD_RCGR		0x09A0
#define BLSP2_UART1_APPS_CBCR			0x09C4
#define BLSP2_UART1_APPS_CMD_RCGR		0x09CC
#define BLSP2_QUP2_SPI_APPS_CBCR		0x0A04
#define BLSP2_QUP2_I2C_APPS_CBCR		0x0A08
#define BLSP2_QUP2_SPI_APPS_CMD_RCGR		0x0A0C
#define BLSP2_QUP2_I2C_APPS_CMD_RCGR		0x0A20
#define BLSP2_UART2_APPS_CBCR			0x0A44
#define BLSP2_UART2_APPS_CMD_RCGR		0x0A4C
#define BLSP2_QUP3_SPI_APPS_CBCR		0x0A84
#define BLSP2_QUP3_I2C_APPS_CBCR		0x0A88
#define BLSP2_QUP3_SPI_APPS_CMD_RCGR		0x0A8C
#define BLSP2_QUP3_I2C_APPS_CMD_RCGR		0x0AA0
#define BLSP2_UART3_APPS_CBCR			0x0AC4
#define BLSP2_UART3_APPS_CMD_RCGR		0x0ACC
#define BLSP2_QUP4_SPI_APPS_CBCR		0x0B04
#define BLSP2_QUP4_I2C_APPS_CBCR		0x0B08
#define BLSP2_QUP4_SPI_APPS_CMD_RCGR		0x0B0C
#define BLSP2_QUP4_I2C_APPS_CMD_RCGR		0x0B20
#define BLSP2_UART4_APPS_CBCR			0x0B44
#define BLSP2_UART4_APPS_CMD_RCGR		0x0B4C
#define BLSP2_QUP5_SPI_APPS_CBCR		0x0B84
#define BLSP2_QUP5_I2C_APPS_CBCR		0x0B88
#define BLSP2_QUP5_SPI_APPS_CMD_RCGR		0x0B8C
#define BLSP2_QUP5_I2C_APPS_CMD_RCGR		0x0BA0
#define BLSP2_UART5_APPS_CBCR			0x0BC4
#define BLSP2_UART5_APPS_CMD_RCGR		0x0BCC
#define BLSP2_QUP6_SPI_APPS_CBCR		0x0C04
#define BLSP2_QUP6_I2C_APPS_CBCR		0x0C08
#define BLSP2_QUP6_SPI_APPS_CMD_RCGR		0x0C0C
#define BLSP2_QUP6_I2C_APPS_CMD_RCGR		0x0C20
#define BLSP2_UART6_APPS_CBCR			0x0C44
#define BLSP2_UART6_APPS_CMD_RCGR		0x0C4C
#define PDM_AHB_CBCR				0x0CC4
#define PDM2_CBCR				0x0CCC
#define PDM2_CMD_RCGR				0x0CD0
#define PWM_AHB_CBCR				0x1D84
#define PWM_CBCR				0x1D88
#define PWM_CMD_RCGR				0x1D8C
#define PRNG_AHB_CBCR				0x0D04
#define BAM_DMA_AHB_CBCR			0x0D44
#define TSIF_REF_CBCR				0x1A50
#define TSIF_REF_CMD_RCGR			0x1A54
#define BOOT_ROM_AHB_CBCR			0x0E04
#define SEC_CTRL_KLM_AHB_CBCR			0x0F58
#define CE1_CMD_RCGR				0x1050
#define CE1_CBCR				0x1044
#define CE1_AXI_CBCR				0x1048
#define CE1_AHB_CBCR				0x104C
#define CE2_CMD_RCGR				0x1090
#define CE2_CBCR				0x1084
#define CE2_AXI_CBCR				0x1088
#define CE2_AHB_CBCR				0x108C
#define GCC_XO_DIV4_CBCR			0x10C8
#define LPASS_Q6_AXI_CBCR			0x11C0
#define SYS_NOC_LPASS_SWAY_CBCR			0x11C4
#define SYS_NOC_LPASS_MPORT_CBCR		0x11C8
#define APCS_GPLL_ENA_VOTE			0x1480
#define APCS_CLOCK_BRANCH_ENA_VOTE		0x1484
#define APCS_CLOCK_SLEEP_ENA_VOTE		0x1488
#define GCC_DEBUG_CLK_CTL			0x1880
#define CLOCK_FRQ_MEASURE_CTL			0x1884
#define CLOCK_FRQ_MEASURE_STATUS		0x1888
#define PLLTEST_PAD_CFG				0x188C
#define GP1_CBCR				0x1900
#define GP1_CMD_RCGR				0x1904
#define GP2_CBCR				0x1940
#define GP2_CMD_RCGR				0x1944
#define GP3_CBCR				0x1980
#define GP3_CMD_RCGR				0x1984
#define BCSS_CFG_AHB_CBCR			0x1A44
#define BCSS_SLEEP_CBCR				0x1A4C
#define USB_HS2_SYSTEM_CBCR			0x1A84
#define USB_HS2_AHB_CBCR			0x1A88
#define USB_HS2_SYSTEM_CMD_RCGR			0x1A90
#define USB2C_PHY_SLEEP_CBCR			0x1AAC
#define GENI_SER_CMD_RCGR			0x1ACC
#define GENI_AHB_CBCR				0x1AC4
#define GENI_SER_CBCR				0x1AC8
#define CE3_CMD_RCGR				0x1B10
#define CE3_CBCR				0x1B04
#define CE3_AXI_CBCR				0x1B08
#define CE3_AHB_CBCR				0x1B0C
#define KLM_S_CBCR				0x1B44
#define KLM_CORE_CBCR				0x1B48
#define SATA_RX_OOB_CMD_RCGR			0x1BD4
#define SATA_PMALIVE_CMD_RCGR			0x1BEC
#define SATA_AXI_CBCR				0x1BC4
#define SATA_CFG_AHB_CBCR			0x1BC8
#define SATA_RX_OOB_CBCR			0x1BCC
#define SATA_PMALIVE_CBCR			0x1BD0
#define SATA_ASIC0_CMD_RCGR			0x1D44
#define SATA_ASIC0_CBCR				0x1D40
#define SATA_RX_CBCR				0x1D5C
#define SATA_RX_CMD_RCGR			0x1D60
#define PCIE_CFG_AHB_CBCR			0x1C04
#define PCIE_PIPE_CBCR				0x1C08
#define PCIE_AXI_CBCR				0x1C0C
#define PCIE_SLEEP_CBCR				0x1C10
#define PCIE_AXI_MSTR_CBCR			0x1C2C
#define PCIE_PIPE_CMD_RCGR			0x1C14
#define PCIE_AUX_CMD_RCGR			0x1E00
#define GMAC_CORE_CMD_RCGR			0x1C50
#define GMAC_AXI_CBCR				0x1C44
#define GMAC_CFG_AHB_CBCR			0x1C48
#define GMAC_CORE_CBCR				0x1C4C
#define GMAC_125M_CMD_RCGR			0x1C6C
#define GMAC_125M_CBCR				0x1C68
#define GMAC_RX_CBCR				0x1DC0
#define GMAC_SYS_25M_CBCR			0x1DDC
#define GMAC_SYS_CBCR				0x1DE0
#define GMAC_SYS_25M_CMD_RCGR			0x1DC4
#define SPSS_AHB_CBCR				0x1B84
#define PLL_SR_MODE				0x0000
#define PLL_SR_L_VAL				0x0004
#define PLL_SR_M_VAL				0x0008
#define PLL_SR_N_VAL				0x000C
#define PLL_SR_USER_CTL				0x0010
#define PLL_SR_STATUS				0x001C
#define BCC_XO_CBCR				0x0130
#define TSPP2_AHB_CBCR				0x0134
#define DEM_AHB_CBCR				0x0138
#define IMG_AHB_CBCR				0x013C
#define TSC_AHB_CBCR				0x0140
#define VBIF_AHB_CBCR				0x0144
#define DEM_CORE_X2_CBCR			0x0148
#define NIDAQ_OUT_CBCR				0x014C
#define NIDAQ_IN_CBCR				0x0150
#define ATV_X5_CBCR				0x0154
#define ATV_RXFE_RESAMP_CBCR			0x0158
#define ATV_RXFE_CBCR				0x015C
#define DEM_RXFE_CBCR				0x0160
#define ADC_2_CBCR				0x0164
#define ADC_1_CBCR				0x0168
#define ADC_0_CBCR				0x016C
#define TSC_SER_CBCR				0x0170
#define TSC_PAR_CBCR				0x0174
#define TSC_CI_CBCR				0x0178
#define TSPP2_CORE_CBCR				0x0180
#define DEM_CORE_CBCR				0x0184
#define LNB_AHB_CBCR				0x0188
#define LNB_SER_CBCR				0x018C
#define KLM_AHB_CBCR				0x0190
#define TS_OUT_CBCR				0x019C
#define FORZA_SYNC_X5_CBCR			0x01A0
#define DEM_CORE_DIV2_CBCR			0x01A4
#define VBIF_AXI_CBCR				0x01A8
#define VBIF_DEM_CORE_CBCR			0x01AC
#define VBIF_TSPP2_CBCR				0x01B0
#define ADC_CMD_RCGR				0x0400
#define ADC_CLK_MISC				0x0408
#define DIG_DEM_CORE_CMD_RCGR			0x0420
#define ATV_RXFE_RESAMP_CLK_MISC		0x0454
#define ATV_RESAMP_CMD_RCGR			0x0440
#define ATV_X5_CMD_RCGR				0x0460
#define DIG_DEM_RXFE_MISC			0x0474
#define NIDAQ_OUT_CMD_RCGR			0x0480
#define GLB_CLK_DIAG				0x001C
#define GLB_TEST_BUS_SEL			0x0020
#define L2_CBCR					0x004C
#define LPASS_DEBUG_CLK_CTL			0x29000

/* Mux source select values */
#define xo_source_val			0
#define gpll0_source_val		1
#define gpll1_source_val		2
#define pcie_pipe_clk_source_val	2
#define gpll1_hsic_source_val		4
#define sata_asic0_clk_source_val	2
#define sata_rx_clk_source_val		2
#define gmac_125m_clk_source_val	2
#define gmac_rx_clk_source_val		0
#define gmac_tx_clk_source_val		3

#define xo_mm_source_val		0
#define mmpll0_mm_source_val		1
#define mmpll1_mm_source_val		2
#define mmpll3_mm_source_val		3
#define mmpll6_mm_source_val		2
#define gpll0_mm_source_val		5
#define hdmiphypll_mm_source_val	3
#define lvdsphy0_pclk_mm_source_val	2
#define vbyonepll_pclk_mm_source_val	3
#define vbyonepll_sym_mm_source_val	4
#define tmds_clk_mm_source_val		1
#define afe_pixel_clk_mm_source_val	3
#define ifdemod_clk_mm_source_val	3
#define ttl_clk_mm_source_val		3

#define xo_lpass_source_val		0
#define lpaaudio_pll_lpass_source_val	1
#define gpll0_lpass_source_val		5
#define bcc_pll1_source_val		1

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

#define F_MMSS(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s##_clk_src.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_mm_source_val), \
	}

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig, \
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig, \
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

#define RPM_MISC_CLK_TYPE			0x306b6c63
#define RPM_BUS_CLK_TYPE			0x316b6c63
#define RPM_MEM_CLK_TYPE			0x326b6c63

#define RPM_SMD_KEY_ENABLE			0x62616E45

#define CXO_ID					0x0
#define QDSS_ID					0x1

#define PNOC_ID					0x0
#define SNOC_ID					0x1
#define CNOC_ID					0x2
#define MMSSNOC_AHB_ID				0x3

#define BIMC_ID					0x0
#define OXILI_ID				0x1
#define OCMEM_ID				0x2

DEFINE_CLK_RPM_SMD_BRANCH(xo_clk_src, xo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);
DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(mmssnoc_ahb_clk, mmssnoc_ahb_a_clk, RPM_BUS_CLK_TYPE,
			MMSSNOC_AHB_ID, NULL);
DEFINE_CLK_RPM_SMD(gfx3d_clk_src, gfx3d_a_clk_src, RPM_MEM_CLK_TYPE, OXILI_ID,
			NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);
DEFINE_CLK_RPM_SMD(ocmemgx_clk, ocmemgx_a_clk, RPM_MEM_CLK_TYPE, OCMEM_ID,
			NULL);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

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
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, 0);

static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_ehci_host_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &xo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &xo_clk_src.c);


static struct pll_vote_clk gpll0_clk_src = {
	.en_reg = (void __iomem *)APCS_GPLL_ENA_VOTE,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)GPLL0_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 600000000,
		.dbg_name = "gpll0_clk_src",
		.ops = &clk_ops_pll_vote,
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

static struct clk_freq_tbl ftbl_gcc_usb30_master_clk[] = {
	F( 125000000,	   gpll0,   1,	  5,   24),
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

static struct clk_freq_tbl ftbl_gcc_tsif_ref_clk[] = {
	F(  27027000,	   gpll0,  1,	  5,	111),
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
		VDD_DIG_FMAX_MAP2(LOW, 27000000, NOMINAL, 27027000),
		CLK_INIT(tsif_ref_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F(  19200000,	      xo,   1,	  0,	0),
	F(  50000000,	   gpll0,  12,	  0,	0),
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
	F(    960000,	      xo,  10,	  1,	2),
	F(   4800000,	      xo,   4,	  0,	0),
	F(   9600000,	      xo,   2,	  0,	0),
	F(  15000000,	   gpll0,  10,	  1,	4),
	F(  19200000,	      xo,   1,	  0,	0),
	F(  25000000,	   gpll0,  12,	  1,	2),
	F(  50000000,	   gpll0,  12,	  0,	0),
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
	F(   3686400,	   gpll0,   1,	 96, 15625),
	F(   7372800,	   gpll0,   1,	192, 15625),
	F(  14745600,	   gpll0,   1,	384, 15625),
	F(  16000000,	   gpll0,   5,	  2,   15),
	F(  19200000,	      xo,   1,	  0,	0),
	F(  24000000,	   gpll0,   5,	  1,	5),
	F(  32000000,	   gpll0,   1,	  4,   75),
	F(  40000000,	   gpll0,  15,	  0,	0),
	F(  46400000,	   gpll0,   1,	 29,  375),
	F(  48000000,	   gpll0, 12.5,    0,	 0),
	F(  51200000,	   gpll0,   1,	 32,  375),
	F(  56000000,	   gpll0,   1,	  7,   75),
	F(  58982400,	   gpll0,   1, 1536, 15625),
	F(  60000000,	   gpll0,  10,	  0,	0),
	F(  63160000,	   gpll0, 9.5,	  0,	0),
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
	F(  50000000,	   gpll0,  12,	  0,	0),
	F(  85710000,	   gpll0,   7,	  0,	0),
	F( 100000000,	   gpll0,   6,	  0,	0),
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
	F(  50000000,	   gpll0,  12,	  0,	0),
	F(  85710000,	   gpll0,   7,	  0,	0),
	F( 100000000,	   gpll0,   6,	  0,	0),
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

static struct clk_freq_tbl ftbl_gcc_ce3_clk[] = {
	F(  50000000,	   gpll0,  12,	  0,	0),
	F(  85710000,	   gpll0,   7,	  0,	0),
	F( 100000000,	   gpll0,   6,	  0,	0),
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
		VDD_DIG_FMAX_MAP2(LOW, 50000000, NOMINAL, 100000000),
		CLK_INIT(ce3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_geni_ser_clk[] = {
	F(  19200000,	      xo,   1,	  0,	0),
	F_END
};

static struct rcg_clk geni_ser_clk_src = {
	.cmd_rcgr_reg = GENI_SER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_geni_ser_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "geni_ser_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(LOW, 19200000),
		CLK_INIT(geni_ser_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gmac_125m_clk[] = {
	F_EXT(	125000000, gmac_125m_clk,   1,	  0,	0),
	F_END
};

static struct rcg_clk gmac_125m_clk_src = {
	.cmd_rcgr_reg = GMAC_125M_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_gmac_125m_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gmac_125m_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(NOMINAL, 125000000),
		CLK_INIT(gmac_125m_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gmac_core_clk[] = {
	F_EXT(	125000000, gmac_125m_clk,   1,	  0,	0),
	F_END
};

static struct rcg_clk gmac_core_clk_src = {
	.cmd_rcgr_reg = GMAC_CORE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_gmac_core_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gmac_core_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(NOMINAL, 125000000),
		CLK_INIT(gmac_core_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gmac_sys_25m_clk[] = {
	F_EXT(	125000000, gmac_125m_clk,   1,	  0,	0),
	F_END
};

static struct rcg_clk gmac_sys_25m_clk_src = {
	.cmd_rcgr_reg = GMAC_SYS_25M_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_gmac_sys_25m_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gmac_sys_25m_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(NOMINAL, 125000000),
		CLK_INIT(gmac_sys_25m_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_gp1_3_clk[] = {
	F(  19200000,	      xo,   1,	  0,	0),
	F( 100000000,	   gpll0,   6,	  0,	0),
	F( 200000000,	   gpll0,   3,	  0,	0),
	F_END
};

static struct rcg_clk gp1_clk_src = {
	.cmd_rcgr_reg = GP1_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_gp1_3_clk,
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
	.freq_tbl = ftbl_gcc_gp1_3_clk,
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
	.freq_tbl = ftbl_gcc_gp1_3_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gp3_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(gp3_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_sleep_clk[] = {
	F(   1000000,	      xo,   1,	  1,   19),
	F_END
};

static struct rcg_clk pcie_aux_clk_src = {
	.cmd_rcgr_reg = PCIE_AUX_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pcie_sleep_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_aux_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 1000000),
		CLK_INIT(pcie_aux_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pcie_pipe_clk[] = {
	F_EXT( 125000000, pcie_pipe_clk,   2,	 0,    0),
	F_EXT( 250000000, pcie_pipe_clk,   1,	 0,    0),
	F_END
};

static struct rcg_clk pcie_pipe_clk_src = {
	.cmd_rcgr_reg = PCIE_PIPE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_pcie_pipe_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pcie_pipe_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 125000000, NOMINAL, 250000000),
		CLK_INIT(pcie_pipe_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(  60000000,	   gpll0,  10,	  0,	0),
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

static struct clk_freq_tbl ftbl_gcc_pwm_clk[] = {
	F(   2400000,	      xo,   8,	  0,	0),
	F_END
};

static struct rcg_clk pwm_clk_src = {
	.cmd_rcgr_reg = PWM_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_pwm_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "pwm_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP1(LOW, 2400000),
		CLK_INIT(pwm_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_asic0_clk[] = {
	F_EXT(	75000000, sata_asic0_clk,   4,	  0,	0),
	F_EXT( 150000000, sata_asic0_clk,   2,	  0,	0),
	F_EXT( 300000000, sata_asic0_clk,   1,	  0,	0),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 300000000),
		CLK_INIT(sata_asic0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_pmalive_clk[] = {
	F(  19200000,	      xo,   1,	  0,	0),
	F(  50000000,	   gpll0,  12,	  0,	0),
	F( 100000000,	   gpll0,   6,	  0,	0),
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
	F_EXT(	75000000, sata_rx_clk,	 4,    0,    0),
	F_EXT( 150000000, sata_rx_clk,	 2,    0,    0),
	F_EXT( 300000000, sata_rx_clk,	 1,    0,    0),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 300000000),
		CLK_INIT(sata_rx_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sata_rx_oob_clk[] = {
	F( 100000000,	   gpll0,   6,	  0,	0),
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
		VDD_DIG_FMAX_MAP1(NOMINAL, 100000000),
		CLK_INIT(sata_rx_oob_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_sdcc1_2_apps_clk[] = {
	F(    144000,	      xo,  16,	  3,   25),
	F(    400000,	      xo,  12,	  1,	4),
	F(  20000000,	   gpll0,  15,	  1,	2),
	F(  25000000,	   gpll0,  12,	  1,	2),
	F(  50000000,	   gpll0,  12,	  0,	0),
	F( 100000000,	   gpll0,   6,	  0,	0),
	F( 200000000,	   gpll0,   3,	  0,	0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src = {
	.cmd_rcgr_reg = SDCC1_APPS_CMD_RCGR,
	.set_rate = set_rate_mnd,
	.freq_tbl = ftbl_gcc_sdcc1_2_apps_clk,
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
	.freq_tbl = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "sdcc2_apps_clk_src",
		.ops = &clk_ops_rcg_mnd,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 200000000),
		CLK_INIT(sdcc2_apps_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb30_mock_utmi_clk[] = {
	F(  60000000,	   gpll0,  10,	  0,	0),
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
	F(  75000000,	   gpll0,   8,	  0,	0),
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

static struct clk_freq_tbl ftbl_gcc_usb_hs2_system_clk[] = {
	F(  75000000,	   gpll0,   8,	  0,	0),
	F_END
};

static struct rcg_clk usb_hs2_system_clk_src = {
	.cmd_rcgr_reg = USB_HS2_SYSTEM_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gcc_usb_hs2_system_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "usb_hs2_system_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 37500000, NOMINAL, 75000000),
		CLK_INIT(usb_hs2_system_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gcc_usb_hsic_clk[] = {
	F_HSIC( 480000000,	   gpll1,   0,	  0,	0),
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
	F(   9600000,	      xo,   2,	  0,	0),
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
	F(  75000000,	   gpll0,   8,	  0,	0),
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

static struct pll_config_regs mmpll0_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL0_L_VAL,
	.m_reg = (void __iomem *)MMPLL0_M_VAL,
	.n_reg = (void __iomem *)MMPLL0_N_VAL,
	.config_reg = (void __iomem *)MMPLL0_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL0_MODE,
	.base = &virt_bases[MMSS_BASE],
};

static struct pll_config mmpll0_config __initdata = {
	.l = 41,
	.m = 2,
	.n = 3,
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
	.l_reg = (void __iomem *)MMPLL1_L_VAL,
	.m_reg = (void __iomem *)MMPLL1_M_VAL,
	.n_reg = (void __iomem *)MMPLL1_N_VAL,
	.config_reg = (void __iomem *)MMPLL1_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL1_MODE,
	.base = &virt_bases[MMSS_BASE],
};

static struct pll_config mmpll1_config __initdata = {
	.l = 48,
	.m = 31,
	.n = 48,
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
	.l_reg = (void __iomem *)MMPLL3_L_VAL,
	.m_reg = (void __iomem *)MMPLL3_M_VAL,
	.n_reg = (void __iomem *)MMPLL3_N_VAL,
	.config_reg = (void __iomem *)MMPLL3_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL3_MODE,
	.base = &virt_bases[MMSS_BASE],
};

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

static struct pll_config_regs mmpll6_regs __initdata = {
	.l_reg = (void __iomem *)MMPLL6_L_VAL,
	.m_reg = (void __iomem *)MMPLL6_M_VAL,
	.n_reg = (void __iomem *)MMPLL6_N_VAL,
	.config_reg = (void __iomem *)MMPLL6_USER_CTL,
	.mode_reg = (void __iomem *)MMPLL6_MODE,
	.base = &virt_bases[MMSS_BASE],
};

static struct pll_config mmpll6_config __initdata = {
	.l = 44,
	.m = 13,
	.n = 48,
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

static struct pll_vote_clk mmpll0_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(0),
	.status_reg = (void __iomem *)MMPLL0_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 800000000,
		.dbg_name = "mmpll0_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll0_clk_src.c),
	},
};

static struct pll_vote_clk mmpll1_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(1),
	.status_reg = (void __iomem *)MMPLL1_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 934000000,
		.dbg_name = "mmpll1_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll1_clk_src.c),
	},
};

static struct pll_clk mmpll3_clk_src = {
	.mode_reg = (void __iomem *)MMPLL3_MODE,
	.status_reg = (void __iomem *)MMPLL3_STATUS,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 930000000,
		.dbg_name = "mmpll3_clk_src",
		.ops = &clk_ops_local_pll,
		CLK_INIT(mmpll3_clk_src.c),
	},
};

static struct pll_vote_clk mmpll6_clk_src = {
	.en_reg = (void __iomem *)MMSS_PLL_VOTE_APCS,
	.en_mask = BIT(2),
	.status_reg = (void __iomem *)MMPLL6_STATUS,
	.status_mask = BIT(17),
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.parent = &xo_clk_src.c,
		.rate = 850000000,
		.dbg_name = "mmpll6_clk_src",
		.ops = &clk_ops_pll_vote,
		CLK_INIT(mmpll6_clk_src.c),
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

static struct branch_clk gcc_bcss_cfg_ahb_clk = {
	.cbcr_reg = BCSS_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bcss_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bcss_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_bcss_sleep_clk = {
	.cbcr_reg = BCSS_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_bcss_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_bcss_sleep_clk.c),
	},
};

static struct branch_clk gcc_tsif_ref_clk = {
	.cbcr_reg = TSIF_REF_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_tsif_ref_clk",
		.parent = &tsif_ref_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_tsif_ref_clk.c),
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
		.dbg_name = "gcc_blsp1_qup1_i2c_apps_clk",
		.parent = &blsp1_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup1_spi_apps_clk",
		.parent = &blsp1_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup2_i2c_apps_clk",
		.parent = &blsp1_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup2_spi_apps_clk",
		.parent = &blsp1_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup3_i2c_apps_clk",
		.parent = &blsp1_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup3_spi_apps_clk",
		.parent = &blsp1_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup4_i2c_apps_clk",
		.parent = &blsp1_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup4_spi_apps_clk",
		.parent = &blsp1_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup5_i2c_apps_clk",
		.parent = &blsp1_qup5_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup5_spi_apps_clk",
		.parent = &blsp1_qup5_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup6_i2c_apps_clk",
		.parent = &blsp1_qup6_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP1_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_qup6_spi_apps_clk",
		.parent = &blsp1_qup6_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart1_apps_clk = {
	.cbcr_reg = BLSP1_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart1_apps_clk",
		.parent = &blsp1_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart2_apps_clk = {
	.cbcr_reg = BLSP1_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart2_apps_clk",
		.parent = &blsp1_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart3_apps_clk = {
	.cbcr_reg = BLSP1_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart3_apps_clk",
		.parent = &blsp1_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart4_apps_clk = {
	.cbcr_reg = BLSP1_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart4_apps_clk",
		.parent = &blsp1_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart5_apps_clk = {
	.cbcr_reg = BLSP1_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.parent = &blsp1_uart5_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp1_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp1_uart6_apps_clk = {
	.cbcr_reg = BLSP1_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp1_uart6_apps_clk",
		.parent = &blsp1_uart6_apps_clk_src.c,
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
		.dbg_name = "gcc_blsp2_qup1_i2c_apps_clk",
		.parent = &blsp2_qup1_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup1_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP1_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup1_spi_apps_clk",
		.parent = &blsp2_qup1_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup1_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup2_i2c_apps_clk",
		.parent = &blsp2_qup2_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup2_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP2_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup2_spi_apps_clk",
		.parent = &blsp2_qup2_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup2_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup3_i2c_apps_clk",
		.parent = &blsp2_qup3_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup3_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP3_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup3_spi_apps_clk",
		.parent = &blsp2_qup3_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup3_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup4_i2c_apps_clk",
		.parent = &blsp2_qup4_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup4_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP4_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup4_spi_apps_clk",
		.parent = &blsp2_qup4_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup4_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup5_i2c_apps_clk",
		.parent = &blsp2_qup5_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup5_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP5_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup5_spi_apps_clk",
		.parent = &blsp2_qup5_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup5_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_i2c_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_I2C_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup6_i2c_apps_clk",
		.parent = &blsp2_qup6_i2c_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_i2c_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_qup6_spi_apps_clk = {
	.cbcr_reg = BLSP2_QUP6_SPI_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_qup6_spi_apps_clk",
		.parent = &blsp2_qup6_spi_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_qup6_spi_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart1_apps_clk = {
	.cbcr_reg = BLSP2_UART1_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart1_apps_clk",
		.parent = &blsp2_uart1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart1_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart2_apps_clk = {
	.cbcr_reg = BLSP2_UART2_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart2_apps_clk",
		.parent = &blsp2_uart2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart2_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart3_apps_clk = {
	.cbcr_reg = BLSP2_UART3_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart3_apps_clk",
		.parent = &blsp2_uart3_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart3_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart4_apps_clk = {
	.cbcr_reg = BLSP2_UART4_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart4_apps_clk",
		.parent = &blsp2_uart4_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart4_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart5_apps_clk = {
	.cbcr_reg = BLSP2_UART5_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart5_apps_clk",
		.parent = &blsp2_uart5_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_blsp2_uart5_apps_clk.c),
	},
};

static struct branch_clk gcc_blsp2_uart6_apps_clk = {
	.cbcr_reg = BLSP2_UART6_APPS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_blsp2_uart6_apps_clk",
		.parent = &blsp2_uart6_apps_clk_src.c,
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
		.dbg_name = "gcc_ce2_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce2_clk.c),
	},
};

static struct local_vote_clk gcc_ce3_ahb_clk = {
	.cbcr_reg = CE3_AHB_CBCR,
	.vote_reg = APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask = BIT(25),
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
	.en_mask = BIT(26),
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
	.en_mask = BIT(27),
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_ce3_clk",
		.ops = &clk_ops_vote,
		CLK_INIT(gcc_ce3_clk.c),
	},
};

static struct branch_clk gcc_geni_ahb_clk = {
	.cbcr_reg = GENI_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_geni_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_geni_ahb_clk.c),
	},
};

static struct branch_clk gcc_geni_ser_clk = {
	.cbcr_reg = GENI_SER_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_geni_ser_clk",
		.parent = &geni_ser_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_geni_ser_clk.c),
	},
};

static struct branch_clk gcc_gmac_125m_clk = {
	.cbcr_reg = GMAC_125M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_125m_clk",
		.parent = &gmac_125m_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_125m_clk.c),
	},
};

static struct branch_clk gcc_gmac_axi_clk = {
	.cbcr_reg = GMAC_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_axi_clk.c),
	},
};

static struct branch_clk gcc_gmac_cfg_ahb_clk = {
	.cbcr_reg = GMAC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_gmac_core_clk = {
	.cbcr_reg = GMAC_CORE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_core_clk",
		.parent = &gmac_core_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_core_clk.c),
	},
};

static struct branch_clk gcc_gmac_rx_clk = {
	.cbcr_reg = GMAC_RX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_rx_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_rx_clk.c),
	},
};

static struct branch_clk gcc_gmac_sys_25m_clk = {
	.cbcr_reg = GMAC_SYS_25M_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_sys_25m_clk",
		.parent = &gmac_sys_25m_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_sys_25m_clk.c),
	},
};

static struct branch_clk gcc_gmac_sys_clk = {
	.cbcr_reg = GMAC_SYS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gmac_sys_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gmac_sys_clk.c),
	},
};

static struct branch_clk gcc_gp1_clk = {
	.cbcr_reg = GP1_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp1_clk",
		.parent = &gp1_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp1_clk.c),
	},
};

static struct branch_clk gcc_gp2_clk = {
	.cbcr_reg = GP2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp2_clk",
		.parent = &gp2_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp2_clk.c),
	},
};

static struct branch_clk gcc_gp3_clk = {
	.cbcr_reg = GP3_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_gp3_clk",
		.parent = &gp3_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_gp3_clk.c),
	},
};

static struct branch_clk gcc_klm_core_clk = {
	.cbcr_reg = KLM_CORE_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_klm_core_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_klm_core_clk.c),
	},
};

static struct branch_clk gcc_klm_s_clk = {
	.cbcr_reg = KLM_S_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_klm_s_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_klm_s_clk.c),
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

static struct branch_clk gcc_sys_noc_lpass_mport_clk = {
	.cbcr_reg = SYS_NOC_LPASS_MPORT_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sys_noc_lpass_mport_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_lpass_mport_clk.c),
	},
};

static struct branch_clk gcc_sys_noc_lpass_sway_clk = {
	.cbcr_reg = SYS_NOC_LPASS_SWAY_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sys_noc_lpass_sway_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_lpass_sway_clk.c),
	},
};

static struct branch_clk gcc_mmss_a5ss_axi_clk = {
	.cbcr_reg = MMSS_A5SS_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mmss_a5ss_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_a5ss_axi_clk.c),
	},
};

static struct branch_clk gcc_mmss_noc_cfg_ahb_clk = {
	.cbcr_reg = MMSS_NOC_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_mmss_noc_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_mmss_noc_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_axi_clk = {
	.cbcr_reg = PCIE_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_axi_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_axi_clk.c),
	},
};

static struct branch_clk gcc_pcie_axi_mstr_clk = {
	.cbcr_reg = PCIE_AXI_MSTR_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_axi_mstr_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_axi_mstr_clk.c),
	},
};

static struct branch_clk gcc_pcie_cfg_ahb_clk = {
	.cbcr_reg = PCIE_CFG_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_cfg_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_cfg_ahb_clk.c),
	},
};

static struct branch_clk gcc_pcie_pipe_clk = {
	.cbcr_reg = PCIE_PIPE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_pipe_clk",
		.parent = &pcie_pipe_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_pipe_clk.c),
	},
};

static struct branch_clk gcc_pcie_sleep_clk = {
	.cbcr_reg = PCIE_SLEEP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pcie_sleep_clk",
		.parent = &pcie_aux_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pcie_sleep_clk.c),
	},
};

static struct branch_clk gcc_pdm2_clk = {
	.cbcr_reg = PDM2_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pdm2_clk",
		.parent = &pdm2_clk_src.c,
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

static struct branch_clk gcc_pwm_ahb_clk = {
	.cbcr_reg = PWM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pwm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pwm_ahb_clk.c),
	},
};

static struct branch_clk gcc_pwm_clk = {
	.cbcr_reg = PWM_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_pwm_clk",
		.parent = &pwm_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_pwm_clk.c),
	},
};

static struct branch_clk gcc_sata_asic0_clk = {
	.cbcr_reg = SATA_ASIC0_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sata_asic0_clk",
		.parent = &sata_asic0_clk_src.c,
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
		.dbg_name = "gcc_sata_pmalive_clk",
		.parent = &sata_pmalive_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_pmalive_clk.c),
	},
};

static struct branch_clk gcc_sata_rx_clk = {
	.cbcr_reg = SATA_RX_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sata_rx_clk",
		.parent = &sata_rx_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sata_rx_clk.c),
	},
};

static struct branch_clk gcc_sata_rx_oob_clk = {
	.cbcr_reg = SATA_RX_OOB_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sata_rx_oob_clk",
		.parent = &sata_rx_oob_clk_src.c,
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
		.dbg_name = "gcc_sdcc1_apps_clk",
		.parent = &sdcc1_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc1_apps_clk.c),
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
		.dbg_name = "gcc_sdcc2_apps_clk",
		.parent = &sdcc2_apps_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sdcc2_apps_clk.c),
	},
};

static struct branch_clk gcc_sec_ctrl_klm_ahb_clk = {
	.cbcr_reg = SEC_CTRL_KLM_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sec_ctrl_klm_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sec_ctrl_klm_ahb_clk.c),
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

static struct branch_clk gcc_sys_noc_usb3_axi_clk = {
	.cbcr_reg = SYS_NOC_USB3_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_sys_noc_usb3_axi_clk",
		.parent = &usb30_master_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_sys_noc_usb3_axi_clk.c),
	},
};

static struct branch_clk gcc_usb2a_phy_sleep_clk = {
	.cbcr_reg = USB2A_PHY_SLEEP_CBCR,
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

static struct branch_clk gcc_usb2c_phy_sleep_clk = {
	.cbcr_reg = USB2C_PHY_SLEEP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb2c_phy_sleep_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb2c_phy_sleep_clk.c),
	},
};

static struct branch_clk gcc_usb30_master_clk = {
	.cbcr_reg = USB30_MASTER_CBCR,
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
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb30_mock_utmi_clk",
		.parent = &usb30_mock_utmi_clk_src.c,
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
		.dbg_name = "gcc_usb_hs_system_clk",
		.parent = &usb_hs_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs_system_clk.c),
	},
};

static struct branch_clk gcc_usb_hs2_ahb_clk = {
	.cbcr_reg = USB_HS2_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs2_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs2_ahb_clk.c),
	},
};

static struct branch_clk gcc_usb_hs2_system_clk = {
	.cbcr_reg = USB_HS2_SYSTEM_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hs2_system_clk",
		.parent = &usb_hs2_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hs2_system_clk.c),
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
		.dbg_name = "gcc_usb_hsic_clk",
		.parent = &usb_hsic_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_clk.c),
	},
};

static struct branch_clk gcc_usb_hsic_io_cal_clk = {
	.cbcr_reg = USB_HSIC_IO_CAL_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_io_cal_clk",
		.parent = &usb_hsic_io_cal_clk_src.c,
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

static struct branch_clk gcc_usb_hsic_system_clk = {
	.cbcr_reg = USB_HSIC_SYSTEM_CBCR,
	.bcr_reg = USB_HS_HSIC_BCR,
	.base = &virt_bases[GCC_BASE],
	.c = {
		.dbg_name = "gcc_usb_hsic_system_clk",
		.parent = &usb_hsic_system_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gcc_usb_hsic_system_clk.c),
	},
};

static struct clk_freq_tbl ftbl_mmss_mmssnoc_axi_clk[] = {
	F_MMSS(  19200000,	   xo,	 1,    0,    0),
	F_MMSS(  37500000,	gpll0,	16,    0,    0),
	F_MMSS(  50000000,	gpll0,	12,    0,    0),
	F_MMSS(  75000000,	gpll0,	 8,    0,    0),
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 150000000,	gpll0,	 4,    0,    0),
	F_MMSS( 311330000,     mmpll1,	 3,    0,    0),
	F_MMSS( 400000000,     mmpll0,	 2,    0,    0),
	F_MMSS( 467000000,     mmpll1,	 2,    0,    0),
	F_END
};

static struct rcg_clk axi_clk_src = {
	.cmd_rcgr_reg = AXI_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_mmss_mmssnoc_axi_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "axi_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 467000000),
		CLK_INIT(axi_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_venus0_vcodec0_clk[] = {
	F_MMSS(  50000000,	gpll0,	12,    0,    0),
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 133330000,     mmpll0,	 6,    0,    0),
	F_MMSS( 200000000,     mmpll0,	 4,    0,    0),
	F_MMSS( 266670000,     mmpll0,	 3,    0,    0),
	F_MMSS( 465000000,     mmpll3,	 2,    0,    0),
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
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 465000000),
		CLK_INIT(vcodec0_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_mdss_mdp_clk[] = {
	F_MMSS(  37500000,	gpll0,	16,    0,    0),
	F_MMSS(  60000000,	gpll0,	10,    0,    0),
	F_MMSS(  75000000,	gpll0,	 8,    0,    0),
	F_MMSS(  85710000,	gpll0,	 7,    0,    0),
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 133330000,     mmpll0,	 6,    0,    0),
	F_MMSS( 160000000,     mmpll0,	 5,    0,    0),
	F_MMSS( 200000000,     mmpll0,	 4,    0,    0),
	F_MMSS( 228570000,     mmpll0, 3.5,    0,    0),
	F_MMSS( 340000000,     mmpll6, 2.5,    0,    0),
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
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 340000000),
		CLK_INIT(mdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_ocmemcx_ocmemnoc_clk[] = {
	F_MMSS(  19200000,	   xo,	 1,    0,    0),
	F_MMSS(  37500000,	gpll0,	16,    0,    0),
	F_MMSS(  50000000,	gpll0,	12,    0,    0),
	F_MMSS(  75000000,	gpll0,	 8,    0,    0),
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 150000000,	gpll0,	 4,    0,    0),
	F_MMSS( 320000000,     mmpll0, 2.5,    0,    0),
	F_MMSS( 400000000,     mmpll0,	 2,    0,    0),
	F_END
};

static struct rcg_clk ocmemnoc_clk_src = {
	.cmd_rcgr_reg = OCMEMNOC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_ocmemcx_ocmemnoc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "ocmemnoc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 400000000),
		CLK_INIT(ocmemnoc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_camss_jpeg_jpeg2_clk[] = {
	F_MMSS(  75000000,	gpll0,	 8,    0,    0),
	F_MMSS( 133330000,	gpll0, 4.5,    0,    0),
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 228570000,     mmpll0, 3.5,    0,    0),
	F_MMSS( 266670000,     mmpll0,	 3,    0,    0),
	F_MMSS( 320000000,     mmpll0, 2.5,    0,    0),
	F_END
};

static struct rcg_clk jpeg2_clk_src = {
	.cmd_rcgr_reg = JPEG2_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_camss_jpeg_jpeg2_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "jpeg2_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 320000000),
		CLK_INIT(jpeg2_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_sdme_frcf_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 425000000,     mmpll6,	 2,    0,    0),
	F_END
};

static struct rcg_clk sdme_frcf_clk_src = {
	.cmd_rcgr_reg = SDME_FRCF_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_sdme_frcf_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "sdme_frcf_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 425000000),
		CLK_INIT(sdme_frcf_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_frc_xin_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 467000000,     mmpll1,	 2,    0,    0),
	F_END
};

static struct rcg_clk vpu_frc_xin_clk_src = {
	.cmd_rcgr_reg = VPU_FRC_XIN_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_frc_xin_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_frc_xin_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 467000000),
		CLK_INIT(vpu_frc_xin_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_vdp_xin_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 467000000,     mmpll1,	 2,    0,    0),
	F_END
};

static struct rcg_clk vpu_vdp_xin_clk_src = {
	.cmd_rcgr_reg = VPU_VDP_XIN_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_vdp_xin_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_vdp_xin_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 467000000),
		CLK_INIT(vpu_vdp_xin_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_avsync_vp_clk[] = {
	F_MMSS( 150000000,	gpll0,	 4,    0,    0),
	F_MMSS( 320000000,     mmpll0, 2.5,    0,    0),
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

static struct clk_freq_tbl ftbl_mdss_hdmi_clk[] = {
	F_MMSS(  19200000,	   xo,	 1,    0,    0),
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

static struct clk_freq_tbl ftbl_vcap_cfg_clk[] = {
	F_MMSS(  66670000,	gpll0,	 9,    0,    0),
	F_MMSS( 133330000,     mmpll0,	 6,    0,    0),
	F_END
};

static struct rcg_clk cfg_clk_src = {
	.cmd_rcgr_reg = CFG_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vcap_cfg_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "cfg_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 66670000, NOMINAL, 133330000),
		CLK_INIT(cfg_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vcap_md_clk[] = {
	F_MMSS(  19200000,	  xo,	1,    0,    0),
	F_MMSS(  88890000,     mmpll0,	 9,    0,    0),
	F_END
};

static struct rcg_clk md_clk_src = {
	.cmd_rcgr_reg = MD_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vcap_md_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "md_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 19200000, NOMINAL, 88890000),
		CLK_INIT(md_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vcap_vp_clk[] = {
	F_MMSS( 150000000,	gpll0,	 4,    0,    0),
	F_MMSS( 340000000,     mmpll6, 2.5,    0,    0),
	F_END
};

static struct rcg_clk vcap_vp_clk_src = {
	.cmd_rcgr_reg = VCAP_VP_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vcap_vp_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcap_vp_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 150000000, NOMINAL, 340000000),
		CLK_INIT(vcap_vp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_gproc_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 425000000,     mmpll6,	 2,    0,    0),
	F_END
};

static struct rcg_clk gproc_clk_src = {
	.cmd_rcgr_reg = GPROC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_gproc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "gproc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 425000000),
		CLK_INIT(gproc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_hdmc_frcf_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 425000000,     mmpll6,	 2,    0,    0),
	F_END
};

static struct rcg_clk hdmc_frcf_clk_src = {
	.cmd_rcgr_reg = HDMC_FRCF_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_hdmc_frcf_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "hdmc_frcf_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 425000000),
		CLK_INIT(hdmc_frcf_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_kproc_clk[] = {
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 212500000,     mmpll6,	 4,    0,    0),
	F_END
};

static struct rcg_clk kproc_clk_src = {
	.cmd_rcgr_reg = KPROC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_kproc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "kproc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 212500000),
		CLK_INIT(kproc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_maple_clk[] = {
	F_MMSS(  50000000,	gpll0,	12,    0,    0),
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 200000000,     mmpll0,	 4,    0,    0),
	F_MMSS( 320000000,     mmpll0, 2.5,    0,    0),
	F_MMSS( 400000000,     mmpll0,	 2,    0,    0),
	F_MMSS( 533000000,     mmpll0, 1.5,    0,    0),
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
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 533330000),
		CLK_INIT(maple_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_preproc_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 425000000,     mmpll6,	 2,    0,    0),
	F_END
};

static struct rcg_clk preproc_clk_src = {
	.cmd_rcgr_reg = PREPROC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_preproc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "preproc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 425000000),
		CLK_INIT(preproc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_sdmc_frcs_clk[] = {
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 212500000,     mmpll6,	 4,    0,    0),
	F_END
};

static struct rcg_clk sdmc_frcs_clk_src = {
	.cmd_rcgr_reg = SDMC_FRCS_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_sdmc_frcs_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "sdmc_frcs_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 100000000, NOMINAL, 212500000),
		CLK_INIT(sdmc_frcs_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_sdme_vproc_clk[] = {
	F_MMSS( 200000000,	gpll0,	 3,    0,    0),
	F_MMSS( 425000000,     mmpll6,	 2,    0,    0),
	F_END
};

static struct rcg_clk sdme_vproc_clk_src = {
	.cmd_rcgr_reg = SDME_VPROC_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_vpu_sdme_vproc_clk,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "sdme_vproc_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 425000000),
		CLK_INIT(sdme_vproc_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_vdp_clk[] = {
	F_MMSS(  50000000,	gpll0,	12,    0,    0),
	F_MMSS( 100000000,	gpll0,	 6,    0,    0),
	F_MMSS( 200000000,     mmpll0,	 4,    0,    0),
	F_MMSS( 320000000,     mmpll0, 2.5,    0,    0),
	F_MMSS( 400000000,     mmpll0,	 2,    0,    0),
	F_MMSS( 425000000,     mmpll6,	 2,    0,    0),
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
		VDD_DIG_FMAX_MAP2(LOW, 200000000, NOMINAL, 425000000),
		CLK_INIT(vdp_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_vpu_bus_clk[] = {
	F_MMSS(  40000000,	gpll0,	15,    0,    0),
	F_MMSS(  80000000,     mmpll0,	10,    0,    0),
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
		.dbg_name = "avsync_vp_clk",
		.parent = &vp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(avsync_vp_clk.c),
	},
};

static struct branch_clk camss_jpeg_jpeg2_clk = {
	.cbcr_reg = CAMSS_JPEG_JPEG2_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "camss_jpeg_jpeg2_clk",
		.parent = &jpeg2_clk_src.c,
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
		.dbg_name = "camss_jpeg_jpeg_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(camss_jpeg_jpeg_axi_clk.c),
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
		.dbg_name = "mdss_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_axi_clk.c),
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
		.dbg_name = "mdss_hdmi_clk",
		.parent = &hdmi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_hdmi_clk.c),
	},
};

static struct branch_clk mdss_mdp_clk = {
	.cbcr_reg = MDSS_MDP_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_mdp_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_clk.c),
	},
};

static struct branch_clk mdss_mdp_lut_clk = {
	.cbcr_reg = MDSS_MDP_LUT_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mdss_mdp_lut_clk",
		.parent = &mdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mdss_mdp_lut_clk.c),
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

static struct branch_clk mmss_mmssnoc_ahb_clk = {
	.cbcr_reg = MMSS_MMSSNOC_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "mmss_mmssnoc_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_ahb_clk.c),
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
		.dbg_name = "mmss_mmssnoc_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(mmss_mmssnoc_axi_clk.c),
	},
};

static struct branch_clk mmss_s0_axi_clk = {
	.cbcr_reg = MMSS_S0_AXI_CBCR,
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
		.dbg_name = "ocmemcx_ocmemnoc_clk",
		.parent = &ocmemnoc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(ocmemcx_ocmemnoc_clk.c),
	},
};

static struct branch_clk oxili_ocmemgx_clk = {
	.cbcr_reg = OXILI_OCMEMGX_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "oxili_ocmemgx_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(oxili_ocmemgx_clk.c),
	},
};

static struct branch_clk oxili_gfx3d_clk = {
	.cbcr_reg = OXILI_GFX3D_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "oxili_gfx3d_clk",
		.parent = &gfx3d_clk_src.c,
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

static struct branch_clk bcss_mmss_ifdemod_clk = {
	.cbcr_reg = BCSS_MMSS_IFDEMOD_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "bcss_mmss_ifdemod_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(bcss_mmss_ifdemod_clk.c),
	},
};

static struct branch_clk vcap_ahb_clk = {
	.cbcr_reg = VCAP_AHB_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcap_ahb_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_ahb_clk.c),
	},
};

static struct branch_clk vcap_axi_clk = {
	.cbcr_reg = VCAP_AXI_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcap_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_axi_clk.c),
	},
};

static struct branch_clk vcap_cfg_clk = {
	.cbcr_reg = VCAP_CFG_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcap_cfg_clk",
		.parent = &cfg_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_cfg_clk.c),
	},
};

static struct branch_clk vcap_md_clk = {
	.cbcr_reg = VCAP_MD_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcap_md_clk",
		.parent = &md_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_md_clk.c),
	},
};

static struct branch_clk vcap_vp_clk = {
	.cbcr_reg = VCAP_VP_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vcap_vp_clk",
		.parent = &vcap_vp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vcap_vp_clk.c),
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
		.dbg_name = "venus0_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_axi_clk.c),
	},
};

static struct branch_clk venus0_core0_vcodec_clk = {
	.cbcr_reg = VENUS0_CORE0_VCODEC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "venus0_core0_vcodec_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_core0_vcodec_clk.c),
	},
};

static struct branch_clk venus0_core1_vcodec_clk = {
	.cbcr_reg = VENUS0_CORE1_VCODEC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "venus0_core1_vcodec_clk",
		.parent = &vcodec0_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(venus0_core1_vcodec_clk.c),
	},
};

static struct branch_clk venus0_ocmemnoc_clk = {
	.cbcr_reg = VENUS0_OCMEMNOC_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "venus0_ocmemnoc_clk",
		.parent = &ocmemnoc_clk_src.c,
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
		.dbg_name = "venus0_vcodec0_clk",
		.parent = &vcodec0_clk_src.c,
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
		.dbg_name = "vpu_axi_clk",
		.parent = &axi_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_axi_clk.c),
	},
};

static struct branch_clk vpu_bus_clk = {
	.cbcr_reg = VPU_BUS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_bus_clk",
		.parent = &vpu_bus_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_bus_clk.c),
	},
};

static struct branch_clk vpu_xo_clk = {
	.cbcr_reg = VPU_CXO_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_xo_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_xo_clk.c),
	},
};

static struct branch_clk vpu_frc_xin_clk = {
	.cbcr_reg = VPU_FRC_XIN_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_frc_xin_clk",
		.parent = &vpu_frc_xin_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_frc_xin_clk.c),
	},
};

static struct branch_clk vpu_gproc_clk = {
	.cbcr_reg = VPU_GPROC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_gproc_clk",
		.parent = &gproc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_gproc_clk.c),
	},
};

static struct branch_clk vpu_hdmc_frcf_clk = {
	.cbcr_reg = VPU_HDMC_FRCF_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_hdmc_frcf_clk",
		.parent = &hdmc_frcf_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_hdmc_frcf_clk.c),
	},
};

static struct branch_clk vpu_kproc_clk = {
	.cbcr_reg = VPU_KPROC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_kproc_clk",
		.parent = &kproc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_kproc_clk.c),
	},
};

static struct branch_clk vpu_maple_clk = {
	.cbcr_reg = VPU_MAPLE_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_maple_clk",
		.parent = &maple_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_maple_clk.c),
	},
};

static struct branch_clk vpu_preproc_clk = {
	.cbcr_reg = VPU_PREPROC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_preproc_clk",
		.parent = &preproc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_preproc_clk.c),
	},
};

static struct branch_clk vpu_sdmc_frcs_clk = {
	.cbcr_reg = VPU_SDMC_FRCS_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_sdmc_frcs_clk",
		.parent = &sdmc_frcs_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_sdmc_frcs_clk.c),
	},
};

static struct branch_clk vpu_sdme_frcf_clk = {
	.cbcr_reg = VPU_SDME_FRCF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_sdme_frcf_clk",
		.parent = &sdme_frcf_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_sdme_frcf_clk.c),
	},
};

static struct branch_clk vpu_sdme_frcs_clk = {
	.cbcr_reg = VPU_SDME_FRCS_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_sdme_frcs_clk",
		.parent = &sdme_frcf_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_sdme_frcs_clk.c),
	},
};

static struct branch_clk vpu_sdme_vproc_clk = {
	.cbcr_reg = VPU_SDME_VPROC_CBCR,
	.has_sibling = 0,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_sdme_vproc_clk",
		.parent = &sdme_vproc_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_sdme_vproc_clk.c),
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
		.dbg_name = "vpu_vdp_clk",
		.parent = &vdp_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_vdp_clk.c),
	},
};

static struct branch_clk vpu_vdp_xin_clk = {
	.cbcr_reg = VPU_VDP_XIN_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[MMSS_BASE],
	.c = {
		.dbg_name = "vpu_vdp_xin_clk",
		.parent = &vpu_vdp_xin_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(vpu_vdp_xin_clk.c),
	},
};

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

struct measure_mux_entry measure_mux[] = {
	{&gcc_sys_noc_usb3_axi_clk.c,		GCC_BASE,	0x0001},
	{&gcc_mmss_noc_cfg_ahb_clk.c,		GCC_BASE,	0x002a},
	{&gcc_mmss_a5ss_axi_clk.c,		GCC_BASE,	0x002d},
	{&gcc_usb30_master_clk.c,		GCC_BASE,	0x0050},
	{&gcc_usb30_sleep_clk.c,		GCC_BASE,	0x0051},
	{&gcc_usb30_mock_utmi_clk.c,		GCC_BASE,	0x0052},
	{&gcc_usb_hsic_ahb_clk.c,		GCC_BASE,	0x0058},
	{&gcc_usb_hsic_system_clk.c,		GCC_BASE,	0x0059},
	{&gcc_usb_hsic_clk.c,			GCC_BASE,	0x005a},
	{&gcc_usb_hsic_io_cal_clk.c,		GCC_BASE,	0x005b},
	{&gcc_usb_hsic_io_cal_sleep_clk.c,	GCC_BASE,	0x005c},
	{&gcc_usb_hs_system_clk.c,		GCC_BASE,	0x0060},
	{&gcc_usb_hs_ahb_clk.c,			GCC_BASE,	0x0061},
	{&gcc_usb2a_phy_sleep_clk.c,		GCC_BASE,	0x0063},
	{&gcc_usb2b_phy_sleep_clk.c,		GCC_BASE,	0x0064},
	{&gcc_sdcc1_apps_clk.c,			GCC_BASE,	0x0068},
	{&gcc_sdcc1_ahb_clk.c,			GCC_BASE,	0x0069},
	{&gcc_sdcc2_apps_clk.c,			GCC_BASE,	0x0070},
	{&gcc_sdcc2_ahb_clk.c,			GCC_BASE,	0x0071},
	{&gcc_blsp1_ahb_clk.c,			GCC_BASE,	0x0088},
	{&gcc_blsp1_qup1_spi_apps_clk.c,	GCC_BASE,	0x008a},
	{&gcc_blsp1_qup1_i2c_apps_clk.c,	GCC_BASE,	0x008b},
	{&gcc_blsp1_uart1_apps_clk.c,		GCC_BASE,	0x008c},
	{&gcc_blsp1_qup2_spi_apps_clk.c,	GCC_BASE,	0x008e},
	{&gcc_blsp1_qup2_i2c_apps_clk.c,	GCC_BASE,	0x0090},
	{&gcc_blsp1_uart2_apps_clk.c,		GCC_BASE,	0x0091},
	{&gcc_blsp1_qup3_spi_apps_clk.c,	GCC_BASE,	0x0093},
	{&gcc_blsp1_qup3_i2c_apps_clk.c,	GCC_BASE,	0x0094},
	{&gcc_blsp1_uart3_apps_clk.c,		GCC_BASE,	0x0095},
	{&gcc_blsp1_qup4_spi_apps_clk.c,	GCC_BASE,	0x0098},
	{&gcc_blsp1_qup4_i2c_apps_clk.c,	GCC_BASE,	0x0099},
	{&gcc_blsp1_uart4_apps_clk.c,		GCC_BASE,	0x009a},
	{&gcc_blsp1_qup5_spi_apps_clk.c,	GCC_BASE,	0x009c},
	{&gcc_blsp1_qup5_i2c_apps_clk.c,	GCC_BASE,	0x009d},
	{&gcc_blsp1_uart5_apps_clk.c,		GCC_BASE,	0x009e},
	{&gcc_blsp1_qup6_spi_apps_clk.c,	GCC_BASE,	0x00a1},
	{&gcc_blsp1_qup6_i2c_apps_clk.c,	GCC_BASE,	0x00a2},
	{&gcc_blsp1_uart6_apps_clk.c,		GCC_BASE,	0x00a3},
	{&gcc_blsp2_ahb_clk.c,			GCC_BASE,	0x00a8},
	{&gcc_blsp2_qup1_spi_apps_clk.c,	GCC_BASE,	0x00aa},
	{&gcc_blsp2_qup1_i2c_apps_clk.c,	GCC_BASE,	0x00ab},
	{&gcc_blsp2_uart1_apps_clk.c,		GCC_BASE,	0x00ac},
	{&gcc_blsp2_qup2_spi_apps_clk.c,	GCC_BASE,	0x00ae},
	{&gcc_blsp2_qup2_i2c_apps_clk.c,	GCC_BASE,	0x00b0},
	{&gcc_blsp2_uart2_apps_clk.c,		GCC_BASE,	0x00b1},
	{&gcc_blsp2_qup3_spi_apps_clk.c,	GCC_BASE,	0x00b3},
	{&gcc_blsp2_qup3_i2c_apps_clk.c,	GCC_BASE,	0x00b4},
	{&gcc_blsp2_uart3_apps_clk.c,		GCC_BASE,	0x00b5},
	{&gcc_blsp2_qup4_spi_apps_clk.c,	GCC_BASE,	0x00b8},
	{&gcc_blsp2_qup4_i2c_apps_clk.c,	GCC_BASE,	0x00b9},
	{&gcc_blsp2_uart4_apps_clk.c,		GCC_BASE,	0x00ba},
	{&gcc_blsp2_qup5_spi_apps_clk.c,	GCC_BASE,	0x00bc},
	{&gcc_blsp2_qup5_i2c_apps_clk.c,	GCC_BASE,	0x00bd},
	{&gcc_blsp2_uart5_apps_clk.c,		GCC_BASE,	0x00be},
	{&gcc_blsp2_qup6_spi_apps_clk.c,	GCC_BASE,	0x00c1},
	{&gcc_blsp2_qup6_i2c_apps_clk.c,	GCC_BASE,	0x00c2},
	{&gcc_blsp2_uart6_apps_clk.c,		GCC_BASE,	0x00c3},
	{&gcc_pdm_ahb_clk.c,			GCC_BASE,	0x00d0},
	{&gcc_pdm2_clk.c,			GCC_BASE,	0x00d2},
	{&gcc_prng_ahb_clk.c,			GCC_BASE,	0x00d8},
	{&gcc_bam_dma_ahb_clk.c,		GCC_BASE,	0x00e0},
	{&gcc_boot_rom_ahb_clk.c,		GCC_BASE,	0x00f8},
	{&gcc_sec_ctrl_klm_ahb_clk.c,		GCC_BASE,	0x0125},
	{&gcc_ce1_clk.c,			GCC_BASE,	0x0138},
	{&gcc_ce1_axi_clk.c,			GCC_BASE,	0x0139},
	{&gcc_ce1_ahb_clk.c,			GCC_BASE,	0x013a},
	{&gcc_ce2_clk.c,			GCC_BASE,	0x0140},
	{&gcc_ce2_axi_clk.c,			GCC_BASE,	0x0141},
	{&gcc_ce2_ahb_clk.c,			GCC_BASE,	0x0142},
	{&gcc_lpass_q6_axi_clk.c,		GCC_BASE,	0x0160},
	{&gcc_sys_noc_lpass_sway_clk.c,		GCC_BASE,	0x0162},
	{&gcc_sys_noc_lpass_mport_clk.c,	GCC_BASE,	0x0163},
	{&gcc_bcss_cfg_ahb_clk.c,		GCC_BASE,	0x01d8},
	{&gcc_bcss_sleep_clk.c,			GCC_BASE,	0x01db},
	{&gcc_tsif_ref_clk.c,			GCC_BASE,	0x01dc},
	{&gcc_usb_hs2_system_clk.c,		GCC_BASE,	0x01e0},
	{&gcc_usb_hs2_ahb_clk.c,		GCC_BASE,	0x01e1},
	{&gcc_usb2c_phy_sleep_clk.c,		GCC_BASE,	0x01e3},
	{&gcc_geni_ahb_clk.c,			GCC_BASE,	0x01e8},
	{&gcc_geni_ser_clk.c,			GCC_BASE,	0x01e9},
	{&gcc_ce3_clk.c,			GCC_BASE,	0x01f0},
	{&gcc_ce3_axi_clk.c,			GCC_BASE,	0x01f1},
	{&gcc_ce3_ahb_clk.c,			GCC_BASE,	0x01f2},
	{&gcc_klm_s_clk.c,			GCC_BASE,	0x01f8},
	{&gcc_klm_core_clk.c,			GCC_BASE,	0x01f9},
	{&gcc_spss_ahb_clk.c,			GCC_BASE,	0x0200},
	{&gcc_sata_axi_clk.c,			GCC_BASE,	0x0208},
	{&gcc_sata_cfg_ahb_clk.c,		GCC_BASE,	0x0209},
	{&gcc_sata_rx_oob_clk.c,		GCC_BASE,	0x020a},
	{&gcc_sata_pmalive_clk.c,		GCC_BASE,	0x020b},
	{&gcc_sata_asic0_clk.c,			GCC_BASE,	0x020c},
	{&gcc_sata_rx_clk.c,			GCC_BASE,	0x020d},
	{&gcc_pcie_cfg_ahb_clk.c,		GCC_BASE,	0x0210},
	{&gcc_pcie_pipe_clk.c,			GCC_BASE,	0x0211},
	{&gcc_pcie_axi_clk.c,			GCC_BASE,	0x0212},
	{&gcc_pcie_sleep_clk.c,			GCC_BASE,	0x0213},
	{&gcc_pcie_axi_mstr_clk.c,		GCC_BASE,	0x0214},
	{&gcc_gmac_axi_clk.c,			GCC_BASE,	0x0218},
	{&gcc_gmac_cfg_ahb_clk.c,		GCC_BASE,	0x0219},
	{&gcc_gmac_core_clk.c,			GCC_BASE,	0x021a},
	{&gcc_gmac_125m_clk.c,			GCC_BASE,	0x021b},
	{&gcc_gmac_rx_clk.c,			GCC_BASE,	0x021c},
	{&gcc_gmac_sys_25m_clk.c,		GCC_BASE,	0x021d},
	{&gcc_pwm_ahb_clk.c,			GCC_BASE,	0x0248},
	{&gcc_pwm_clk.c,			GCC_BASE,	0x0249},

	{&mmss_mmssnoc_ahb_clk.c,		MMSS_BASE,	0x0001},
	{&mmss_mmssnoc_bto_ahb_clk.c,		MMSS_BASE,	0x0002},
	{&mmss_misc_ahb_clk.c,			MMSS_BASE,	0x0003},
	{&mmss_mmssnoc_axi_clk.c,		MMSS_BASE,	0x0004},
	{&mmss_s0_axi_clk.c,			MMSS_BASE,	0x0005},
	{&ocmemcx_ocmemnoc_clk.c,		MMSS_BASE,	0x0007},
	{&ocmemcx_ahb_clk.c,			MMSS_BASE,	0x0008},
	{&oxilicx_ahb_clk.c,			MMSS_BASE,	0x000a},
	{&venus0_axi_clk.c,			MMSS_BASE,	0x000d},
	{&venus0_ocmemnoc_clk.c,		MMSS_BASE,	0x000e},
	{&venus0_ahb_clk.c,			MMSS_BASE,	0x000f},
	{&mdss_mdp_clk.c,			MMSS_BASE,	0x0012},
	{&mdss_mdp_lut_clk.c,			MMSS_BASE,	0x0013},
	{&mdss_hdmi_clk.c,			MMSS_BASE,	0x0019},
	{&mdss_ahb_clk.c,			MMSS_BASE,	0x001a},
	{&mdss_hdmi_ahb_clk.c,			MMSS_BASE,	0x001b},
	{&mdss_axi_clk.c,			MMSS_BASE,	0x001c},
	{&camss_top_ahb_clk.c,			MMSS_BASE,	0x001d},
	{&camss_micro_ahb_clk.c,		MMSS_BASE,	0x001e},
	{&camss_jpeg_jpeg2_clk.c,		MMSS_BASE,	0x001f},
	{&camss_jpeg_jpeg_ahb_clk.c,		MMSS_BASE,	0x0020},
	{&camss_jpeg_jpeg_axi_clk.c,		MMSS_BASE,	0x0021},
	{&vpu_gproc_clk.c,			MMSS_BASE,	0x0022},
	{&vpu_kproc_clk.c,			MMSS_BASE,	0x0023},
	{&vpu_sdmc_frcs_clk.c,			MMSS_BASE,	0x0024},
	{&vpu_sdme_frcf_clk.c,			MMSS_BASE,	0x0025},
	{&vpu_sdme_frcs_clk.c,			MMSS_BASE,	0x0026},
	{&vpu_sdme_vproc_clk.c,			MMSS_BASE,	0x0027},
	{&vpu_hdmc_frcf_clk.c,			MMSS_BASE,	0x0028},
	{&vpu_preproc_clk.c,			MMSS_BASE,	0x0029},
	{&vpu_vdp_clk.c,			MMSS_BASE,	0x002a},
	{&vpu_maple_clk.c,			MMSS_BASE,	0x002b},
	{&vpu_axi_clk.c,			MMSS_BASE,	0x002c},
	{&vpu_ahb_clk.c,			MMSS_BASE,	0x002d},
	{&vcap_md_clk.c,			MMSS_BASE,	0x0031},
	{&vcap_cfg_clk.c,			MMSS_BASE,	0x0032},
	{&vcap_vp_clk.c,			MMSS_BASE,	0x0036},
	{&vcap_axi_clk.c,			MMSS_BASE,	0x0038},
	{&vcap_ahb_clk.c,			MMSS_BASE,	0x0039},
	{&avsync_vp_clk.c,			MMSS_BASE,	0x004e},
	{&avsync_ahb_clk.c,			MMSS_BASE,	0x0052},
	{&dummy_clk,				N_BASES,	0x0000},
};

static int measure_clk_set_parent(struct clk *c, struct clk *parent)
{
	struct measure_clk *clk = to_measure_clk(c);
	unsigned long flags;
	u32 regval, clk_sel = 0, i;

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

	case BCSS_BASE:
		clk_sel = 0x1DA;
		regval = BVAL(11, 0, measure_mux[i].debug_mux);
		/* TODO: writel regval */
	default:
		return -EINVAL;
	}

	/* Set debug mux clock index */
	regval = BVAL(8, 0, clk_sel);
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
		pr_warn("CXO clock failed to enable. Can't measure\n");
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

	writel_relaxed(0x51A00, GCC_REG_BASE(PLLTEST_PAD_CFG));
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

/*
 * TODO: Drivers need to fill in the clock names and device names for the
 * clocks they need to control
 */
static struct clk_lookup mpq_clocks_8092_rumi[] = {
	CLK_DUMMY("core_clk",	BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("iface_clk",	BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("core_clk",	BLSP1_UART_CLK, "msm_serial_hsl.1", OFF),
	CLK_DUMMY("iface_clk",	BLSP1_UART_CLK, "msm_serial_hsl.1", OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	SDC2_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("iface_clk",	SDC2_P_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK,	"msm_sps", OFF),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,	"msm_sps", OFF),
	CLK_DUMMY("",	usb30_master_clk_src.c,	"", OFF),
	CLK_DUMMY("",	tsif_ref_clk_src.c,	"", OFF),
	CLK_DUMMY("",	ce1_clk_src.c,	"", OFF),
	CLK_DUMMY("",	ce2_clk_src.c,	"", OFF),
	CLK_DUMMY("",	ce3_clk_src.c,	"", OFF),
	CLK_DUMMY("",	geni_ser_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gmac_125m_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gmac_core_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gmac_sys_25m_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gp1_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gp2_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gp3_clk_src.c,	"", OFF),
	CLK_DUMMY("",	pcie_aux_clk_src.c,	"", OFF),
	CLK_DUMMY("",	pcie_pipe_clk_src.c,	"", OFF),
	CLK_DUMMY("",	pdm2_clk_src.c,	"", OFF),
	CLK_DUMMY("",	pwm_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sata_asic0_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sata_pmalive_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sata_rx_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sata_rx_oob_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sdcc1_apps_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sdcc2_apps_clk_src.c,	"", OFF),
	CLK_DUMMY("",	usb30_mock_utmi_clk_src.c,	"", OFF),
	CLK_DUMMY("",	usb_hs_system_clk_src.c,	"", OFF),
	CLK_DUMMY("",	usb_hs2_system_clk_src.c,	"", OFF),
	CLK_DUMMY("",	usb_hsic_clk_src.c,	"", OFF),
	CLK_DUMMY("",	usb_hsic_io_cal_clk_src.c,	"", OFF),
	CLK_DUMMY("",	usb_hsic_system_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gcc_bam_dma_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_bcss_cfg_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_bimc_gfx_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_bimc_kpss_axi_mstr_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_bimc_sysnoc_axi_mstr_clk.c,	"", OFF),
	CLK_DUMMY("iface_clk",	gcc_blsp1_ahb_clk.c,	"f9923000.spi", OFF),
	CLK_DUMMY("iface_clk",	gcc_blsp1_ahb_clk.c,	"f9924000.i2c", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup1_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("core_clk",	gcc_blsp1_qup1_spi_apps_clk.c,
							"f9923000.spi", OFF),
	CLK_DUMMY("core_clk",	gcc_blsp1_qup2_i2c_apps_clk.c,
							"f9924000.i2c", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup2_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup3_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup3_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup4_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup4_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup5_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup5_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup6_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_qup6_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_uart1_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_uart2_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_uart3_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_uart4_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_uart5_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp1_uart6_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup1_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup1_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup2_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup2_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup3_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup3_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup4_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup4_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup5_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup5_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup6_i2c_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_qup6_spi_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_uart1_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_uart2_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_uart3_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_uart4_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_uart5_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_blsp2_uart6_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_boot_rom_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce1_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce1_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce1_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce2_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce2_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce2_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce3_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce3_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_ce3_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_xo_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_xo_div4_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_geni_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_geni_ser_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_125m_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_cfg_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_core_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_rx_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_sys_25m_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gmac_sys_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gp1_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gp2_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_gp3_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_klm_core_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_klm_s_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_lpass_q6_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sys_noc_lpass_mport_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sys_noc_lpass_sway_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_mmss_a5ss_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_mmss_bimc_gfx_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pcie_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pcie_axi_mstr_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pcie_cfg_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pcie_pipe_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pcie_sleep_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pdm2_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pdm_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_prng_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pwm_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_pwm_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sata_asic0_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sata_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sata_cfg_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sata_pmalive_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sata_rx_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sata_rx_oob_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sdcc1_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sdcc1_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sdcc2_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sdcc2_apps_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_spss_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_sys_noc_usb3_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb2a_phy_sleep_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb2b_phy_sleep_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb2c_phy_sleep_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb30_master_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb30_mock_utmi_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb30_sleep_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hs_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hs_system_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hs2_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hs2_system_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hsic_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hsic_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hsic_io_cal_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_usb_hsic_system_clk.c,	"", OFF),
	/* MMSS Clock Dummy */
	CLK_DUMMY("",	axi_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mmpll0_pll_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mmpll1_pll_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mmpll2_pll_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mmpll3_pll_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mmpll6_pll_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vcodec0_clk_src.c,	"", OFF),
	CLK_DUMMY("",	extpclk_clk_src.c,	"", OFF),
	CLK_DUMMY("",	lvds_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mdp_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vbyone_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gfx3d_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vp_clk_src.c,	"", OFF),
	CLK_DUMMY("",	jpeg2_clk_src.c,	"", OFF),
	CLK_DUMMY("",	hdmi_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vbyone_symbol_clk_src.c,	"", OFF),
	CLK_DUMMY("",	mmss_spdm_axi_div_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_spdm_gfx3d_div_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_spdm_jpeg2_div_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_spdm_mdp_div_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_spdm_vcodec0_div_clk.c,	"", OFF),
	CLK_DUMMY("",	afe_pixel_clk_src.c,	"", OFF),
	CLK_DUMMY("",	cfg_clk_src.c,	"", OFF),
	CLK_DUMMY("",	hdmi_bus_clk_src.c,	"", OFF),
	CLK_DUMMY("",	hdmi_rx_clk_src.c,	"", OFF),
	CLK_DUMMY("",	md_clk_src.c,	"", OFF),
	CLK_DUMMY("",	ttl_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vafe_ext_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vcap_vp_clk_src.c,	"", OFF),
	CLK_DUMMY("",	gproc_clk_src.c,	"", OFF),
	CLK_DUMMY("",	hdmc_frcf_clk_src.c,	"", OFF),
	CLK_DUMMY("",	kproc_clk_src.c,	"", OFF),
	CLK_DUMMY("",	maple_clk_src.c,	"", OFF),
	CLK_DUMMY("",	preproc_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sdmc_frcs_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sdme_frcf_clk_src.c,	"", OFF),
	CLK_DUMMY("",	sdme_vproc_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vdp_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vpu_bus_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vpu_frc_xin_clk_src.c,	"", OFF),
	CLK_DUMMY("",	vpu_vdp_xin_clk_src.c,	"", OFF),
	CLK_DUMMY("",	avsync_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	avsync_extpclk_clk.c,	"", OFF),
	CLK_DUMMY("",	avsync_lvds_clk.c,	"", OFF),
	CLK_DUMMY("",	avsync_vbyone_clk.c,	"", OFF),
	CLK_DUMMY("",	avsync_vp_clk.c,	"", OFF),
	CLK_DUMMY("",	camss_jpeg_jpeg2_clk.c,	"", OFF),
	CLK_DUMMY("",	camss_jpeg_jpeg_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	camss_jpeg_jpeg_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	camss_micro_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	camss_top_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_extpclk_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_hdmi_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_hdmi_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_lvds_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_mdp_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_mdp_lut_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_vbyone_clk.c,	"", OFF),
	CLK_DUMMY("",	mdss_vbyone_symbol_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_misc_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_mmssnoc_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_mmssnoc_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	mmss_s0_axi_clk.c,	"", OFF),
	CLK_DUMMY("core_clk",  ocmemgx_core_clk.c, "fdd00000.qcom,ocmem", OFF),
	CLK_DUMMY("iface_clk",	ocmemcx_ocmemnoc_clk.c,	"fdd00000.qcom.ocmem",
							 OFF),
	CLK_DUMMY("",	oxili_ocmemgx_clk.c,	"", OFF),
	CLK_DUMMY("",	oxili_gfx3d_clk.c,	"", OFF),
	CLK_DUMMY("",	oxilicx_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcss_mmss_ifdemod_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_afe_pixel_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_audio_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_cfg_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_hdmi_bus_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_hdmi_rx_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_md_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_ttl_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_ttl_debug_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_vafe_ext_clk.c,	"", OFF),
	CLK_DUMMY("",	vcap_vp_clk.c,	"", OFF),
	CLK_DUMMY("",   venus0_vcodec0_clk.c,   "", OFF),
	CLK_DUMMY("iface_clk",	venus0_ahb_clk.c, "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("bus_clk",	venus0_axi_clk.c, "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("",	venus0_core0_vcodec_clk.c, "", OFF),
	CLK_DUMMY("",	venus0_core1_vcodec_clk.c, "", OFF),
	CLK_DUMMY("mem_clk", venus0_ocmemnoc_clk.c, "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("core_clk", venus0_vcodec0_clk.c, "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("",	vpu_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_bus_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_cxo_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_frc_xin_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_gproc_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_hdmc_frcf_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_kproc_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_maple_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_preproc_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_sdmc_frcs_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_sdme_frcf_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_sdme_frcs_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_sdme_vproc_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_sleep_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_vdp_clk.c,	"", OFF),
	CLK_DUMMY("",	vpu_vdp_xin_clk.c,	"", OFF),
	CLK_DUMMY("iface_clk", NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fd92a000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fd92a000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdee4000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fdee4000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fdfb6000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fdfb6000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fdfb6000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk", NULL, "fc734000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk", NULL, "fc734000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "fc734000.qcom,iommu", OFF),
	/* BCSS broadcast */
	CLK_DUMMY("",	bcc_dem_core_b_clk_src.c,	"", OFF),
	CLK_DUMMY("",	adc_01_clk_src.c,	"", OFF),
	CLK_DUMMY("",	bcc_adc_0_in_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_klm_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_lnb_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_tsc_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_tspp2_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_vbif_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_bcss_ahb_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_atv_rxfe_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_atv_rxfe_resamp_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_core_clk_src.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_core_div2_clk_src.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_core_x2_b_clk_src.c,	"", OFF),
	CLK_DUMMY("",	bcc_dem_core_x2_pre_cgf_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_tsc_ci_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_tsc_cicam_ts_clk_src.c,	"", OFF),
	CLK_DUMMY("",	bcc_tsc_par_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_tsc_ser_clk_src.c,	"", OFF),
	CLK_DUMMY("",	bcc_tspp2_clk_src.c,	"", OFF),
	CLK_DUMMY("",	dig_dem_core_b_div2_clk.c,	"", OFF),
	CLK_DUMMY("",	atv_x5_pre_cgc_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_albacore_cvbs_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_atv_x1_clk.c,	"", OFF),
	CLK_DUMMY("",	nidaq_out_clk.c,	"", OFF),
	CLK_DUMMY("",	gcc_bcss_axi_clk.c,	"", OFF),
	CLK_DUMMY("",	bcc_lnb_core_clk.c,	"", OFF),

	/* USB */
	CLK_DUMMY("core_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("iface_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("xo", NULL, "msm_otg", OFF),
	/* PRNG */
	CLK_DUMMY("core_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng", OFF),
};

static struct clk_lookup mpq_clocks_8092[] = {
	CLK_DUMMY("core_clk",	BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),

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
	CLK_LOOKUP("xo_clk", xo_a_clk_src.c, ""),
	CLK_LOOKUP("bus_clk", mmssnoc_ahb_clk.c, ""),
	CLK_LOOKUP("core_clk", gfx3d_clk_src.c, ""),
	CLK_LOOKUP("core_clk", gfx3d_a_clk_src.c, ""),
	CLK_LOOKUP("core_clk", qdss_clk.c, ""),

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
	CLK_LOOKUP("dfab_clk", pnoc_sps_clk.c, "msm_sps"),

	CLK_LOOKUP("",	gcc_bam_dma_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_bcss_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_bcss_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_tsif_ref_clk.c,	""),

	/* BLSP1 */
	CLK_LOOKUP("iface_clk",	gcc_blsp1_ahb_clk.c,	"msm_serial_hsl.0"),
	CLK_LOOKUP("",	gcc_blsp1_qup1_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup1_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup2_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup2_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup3_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup3_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup4_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup4_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup5_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup5_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup6_i2c_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_qup6_spi_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart1_apps_clk.c, ""),
	CLK_LOOKUP("",	gcc_blsp1_uart2_apps_clk.c, ""),
	CLK_LOOKUP("",	gcc_blsp1_uart3_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart4_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart5_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp1_uart6_apps_clk.c,	""),

	/* BLSP2 */
	CLK_LOOKUP("",	gcc_blsp2_ahb_clk.c,	""),
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
	CLK_LOOKUP("",	gcc_blsp2_uart1_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart2_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart3_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart4_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart5_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_blsp2_uart6_apps_clk.c,	""),

	CLK_LOOKUP("",	gcc_boot_rom_ahb_clk.c,	""),

	/* CE */
	CLK_LOOKUP("",	gcc_ce1_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce1_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce1_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce2_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce2_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce2_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce3_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce3_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_ce3_clk.c,	""),

	/* GENI */
	CLK_LOOKUP("",	gcc_geni_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_geni_ser_clk.c,	""),

	/* GMAC */
	CLK_LOOKUP("",	gcc_gmac_125m_clk.c,	""),
	CLK_LOOKUP("",	gcc_gmac_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_gmac_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_gmac_core_clk.c,	""),
	CLK_LOOKUP("",	gcc_gmac_rx_clk.c,	""),
	CLK_LOOKUP("",	gcc_gmac_sys_25m_clk.c,	""),

	/* GP */
	CLK_LOOKUP("",	gcc_gp1_clk.c,	""),
	CLK_LOOKUP("",	gcc_gp2_clk.c,	""),
	CLK_LOOKUP("",	gcc_gp3_clk.c,	""),

	CLK_LOOKUP("",	gcc_klm_core_clk.c,	""),
	CLK_LOOKUP("",	gcc_klm_s_clk.c,	""),
	CLK_LOOKUP("",	gcc_sec_ctrl_klm_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_lpass_q6_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_sys_noc_lpass_mport_clk.c,	""),
	CLK_LOOKUP("",	gcc_sys_noc_lpass_sway_clk.c,	""),
	CLK_LOOKUP("",	gcc_mmss_a5ss_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_mmss_noc_cfg_ahb_clk.c,	""),

	/* PCIE */
	CLK_LOOKUP("",	gcc_pcie_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_axi_mstr_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_pipe_clk.c,	""),
	CLK_LOOKUP("",	gcc_pcie_sleep_clk.c,	""),

	CLK_LOOKUP("",	gcc_pdm2_clk.c,	""),
	CLK_LOOKUP("",	gcc_pdm_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_prng_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_pwm_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_pwm_clk.c,	""),
	CLK_LOOKUP("",	gcc_spss_ahb_clk.c,	""),

	/* SATA */
	CLK_LOOKUP("",	gcc_sata_asic0_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_cfg_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_pmalive_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_rx_clk.c,	""),
	CLK_LOOKUP("",	gcc_sata_rx_oob_clk.c,	""),

	/* SDCC */
	CLK_LOOKUP("",	gcc_sdcc1_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sdcc1_apps_clk.c,	""),
	CLK_LOOKUP("",	gcc_sdcc2_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_sdcc2_apps_clk.c,	""),

	/* USB */
	CLK_LOOKUP("",	gcc_sys_noc_usb3_axi_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb2a_phy_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb2b_phy_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb2c_phy_sleep_clk.c,	""),
	CLK_LOOKUP("",	usb30_master_clk_src.c,	""),
	CLK_LOOKUP("",	gcc_usb30_master_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_mock_utmi_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb30_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hs_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hs_system_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hs2_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hs2_system_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hsic_ahb_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hsic_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hsic_io_cal_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hsic_io_cal_sleep_clk.c,	""),
	CLK_LOOKUP("",	gcc_usb_hsic_system_clk.c,	""),

	CLK_LOOKUP("bus_clk_src", axi_clk_src.c, ""),
	CLK_LOOKUP("",	avsync_ahb_clk.c,	""),
	CLK_LOOKUP("",	avsync_vp_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg2_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_jpeg_jpeg_axi_clk.c,	""),
	CLK_LOOKUP("",	camss_micro_ahb_clk.c,	""),
	CLK_LOOKUP("",	camss_top_ahb_clk.c,	""),
	CLK_LOOKUP("",	mdss_ahb_clk.c,	""),
	CLK_LOOKUP("",	mdss_axi_clk.c,	""),
	CLK_LOOKUP("",	mdss_hdmi_ahb_clk.c,	""),
	CLK_LOOKUP("",	mdss_hdmi_clk.c,	""),
	CLK_LOOKUP("",	mdp_clk_src.c,	""),
	CLK_LOOKUP("",	mdss_mdp_clk.c,	""),
	CLK_LOOKUP("",	mdss_mdp_lut_clk.c,	""),
	CLK_LOOKUP("",	mmss_misc_ahb_clk.c,	""),
	CLK_LOOKUP("",	mmss_mmssnoc_ahb_clk.c,	""),
	CLK_LOOKUP("",	mmss_mmssnoc_bto_ahb_clk.c,	""),
	CLK_LOOKUP("",	mmss_mmssnoc_axi_clk.c,	""),
	CLK_LOOKUP("",	mmss_s0_axi_clk.c,	""),
	CLK_LOOKUP("",	ocmemcx_ahb_clk.c,	""),
	CLK_LOOKUP("",	ocmemcx_ocmemnoc_clk.c,	""),
	CLK_LOOKUP("",	oxili_ocmemgx_clk.c,	""),
	CLK_LOOKUP("",	oxili_gfx3d_clk.c,	""),
	CLK_LOOKUP("",	oxilicx_ahb_clk.c,	""),
	CLK_LOOKUP("",	bcss_mmss_ifdemod_clk.c,	""),
	CLK_LOOKUP("",	vcap_ahb_clk.c,	""),
	CLK_LOOKUP("",	vcap_axi_clk.c,	""),
	CLK_LOOKUP("",	vcap_cfg_clk.c,	""),
	CLK_LOOKUP("",	vcap_md_clk.c,	""),
	CLK_LOOKUP("",	vcap_vp_clk.c,	""),
	CLK_LOOKUP("",	venus0_ahb_clk.c,	""),
	CLK_LOOKUP("",	venus0_axi_clk.c,	""),
	CLK_LOOKUP("",	venus0_core0_vcodec_clk.c,	""),
	CLK_LOOKUP("",	venus0_core1_vcodec_clk.c,	""),
	CLK_LOOKUP("",	venus0_ocmemnoc_clk.c,	""),
	CLK_LOOKUP("",	venus0_vcodec0_clk.c,	""),
	CLK_LOOKUP("",	vcodec0_clk_src.c,	""),
	CLK_LOOKUP("",	vpu_ahb_clk.c,	""),
	CLK_LOOKUP("",	vpu_axi_clk.c,	""),
	CLK_LOOKUP("",	vpu_bus_clk.c,	""),
	CLK_LOOKUP("",	vpu_frc_xin_clk.c,	""),
	CLK_LOOKUP("",	vpu_gproc_clk.c,	""),
	CLK_LOOKUP("",	vpu_hdmc_frcf_clk.c,	""),
	CLK_LOOKUP("",	vpu_kproc_clk.c,	""),
	CLK_LOOKUP("",	vpu_maple_clk.c,	""),
	CLK_LOOKUP("",	vpu_preproc_clk.c,	""),
	CLK_LOOKUP("",	vpu_sdmc_frcs_clk.c,	""),
	CLK_LOOKUP("",	vpu_sdme_frcf_clk.c,	""),
	CLK_LOOKUP("",	vpu_sdme_frcs_clk.c,	""),
	CLK_LOOKUP("",	vpu_sdme_vproc_clk.c,	""),
	CLK_LOOKUP("",	vpu_vdp_clk.c,	""),
	CLK_LOOKUP("",	vpu_vdp_xin_clk.c,	""),

	/* RCGs */
	CLK_LOOKUP("",	usb30_master_clk_src.c,	""),
	CLK_LOOKUP("",	tsif_ref_clk_src.c,	""),
	CLK_LOOKUP("",	ce1_clk_src.c,	""),
	CLK_LOOKUP("",	ce2_clk_src.c,	""),
	CLK_LOOKUP("",	ce3_clk_src.c,	""),
	CLK_LOOKUP("",	geni_ser_clk_src.c,	""),
	CLK_LOOKUP("",	gmac_125m_clk_src.c,	""),
	CLK_LOOKUP("",	gmac_core_clk_src.c,	""),
	CLK_LOOKUP("",	gmac_sys_25m_clk_src.c,	""),
	CLK_LOOKUP("",	gp1_clk_src.c,	""),
	CLK_LOOKUP("",	gp2_clk_src.c,	""),
	CLK_LOOKUP("",	gp3_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_aux_clk_src.c,	""),
	CLK_LOOKUP("",	pcie_pipe_clk_src.c,	""),
	CLK_LOOKUP("",	pdm2_clk_src.c,	""),
	CLK_LOOKUP("",	pwm_clk_src.c,	""),
	CLK_LOOKUP("",	sata_asic0_clk_src.c,	""),
	CLK_LOOKUP("",	sata_pmalive_clk_src.c,	""),
	CLK_LOOKUP("",	sata_rx_clk_src.c,	""),
	CLK_LOOKUP("",	sata_rx_oob_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc1_apps_clk_src.c,	""),
	CLK_LOOKUP("",	sdcc2_apps_clk_src.c,	""),
	CLK_LOOKUP("",	usb30_mock_utmi_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hs_system_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hs2_system_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_io_cal_clk_src.c,	""),
	CLK_LOOKUP("",	usb_hsic_system_clk_src.c,	""),
	CLK_LOOKUP("",	axi_clk_src.c,	""),
	CLK_LOOKUP("",	vcodec0_clk_src.c,	""),
	CLK_LOOKUP("",	mdp_clk_src.c,	""),
	CLK_LOOKUP("",	ocmemnoc_clk_src.c,	""),
	CLK_LOOKUP("",	jpeg2_clk_src.c,	""),
	CLK_LOOKUP("",	sdme_frcf_clk_src.c,	""),
	CLK_LOOKUP("",	vpu_frc_xin_clk_src.c,	""),
	CLK_LOOKUP("",	vpu_vdp_xin_clk_src.c,	""),
	CLK_LOOKUP("",	vp_clk_src.c,	""),
	CLK_LOOKUP("",	hdmi_clk_src.c,	""),
	CLK_LOOKUP("",	cfg_clk_src.c,	""),
	CLK_LOOKUP("",	md_clk_src.c,	""),
	CLK_LOOKUP("",	vcap_vp_clk_src.c,	""),
	CLK_LOOKUP("",	gproc_clk_src.c,	""),
	CLK_LOOKUP("",	hdmc_frcf_clk_src.c,	""),
	CLK_LOOKUP("",	kproc_clk_src.c,	""),
	CLK_LOOKUP("",	maple_clk_src.c,	""),
	CLK_LOOKUP("",	preproc_clk_src.c,	""),
	CLK_LOOKUP("",	sdmc_frcs_clk_src.c,	""),
	CLK_LOOKUP("",	sdme_vproc_clk_src.c,	""),
	CLK_LOOKUP("",	vdp_clk_src.c,	""),
	CLK_LOOKUP("",	vpu_bus_clk_src.c,	""),
	CLK_LOOKUP("measure",	measure_clk.c,	"debug"),
};

static void __init reg_init(void)
{
	u32 regval;

	configure_sr_hpm_lp_pll(&mmpll0_config, &mmpll0_regs, 1);
	configure_sr_hpm_lp_pll(&mmpll1_config, &mmpll1_regs, 1);
	configure_sr_hpm_lp_pll(&mmpll3_config, &mmpll3_regs, 0);
	configure_sr_hpm_lp_pll(&mmpll6_config, &mmpll6_regs, 1);

	/* Vote for GPLL0 to turn on. Needed by acpuclock. */
	regval = readl_relaxed(GCC_REG_BASE(APCS_GPLL_ENA_VOTE));
	regval |= BIT(0);
	writel_relaxed(regval, GCC_REG_BASE(APCS_GPLL_ENA_VOTE));

	/* Vote for MMSS_GPLL0_CLK & LPASS_GPLL0_CLK */
	regval = readl_relaxed(GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
	regval |= BIT(30) | BIT(29);
	writel_relaxed(regval, GCC_REG_BASE(APCS_CLOCK_BRANCH_ENA_VOTE));
	/*
	 * No clocks need to be enabled during sleep.
	 */
	writel_relaxed(0x0, GCC_REG_BASE(APCS_CLOCK_SLEEP_ENA_VOTE));
}

static void __init mpq8092_clock_post_init(void)
{
	clk_set_rate(&axi_clk_src.c, 311330000);
	clk_set_rate(&ocmemnoc_clk_src.c, 320000000);

	/*
	 * Hold an active set vote at a rate of 40MHz for the MMSS NOC AHB
	 * source. Sleep set vote is 0.
	 */
	clk_set_rate(&mmssnoc_ahb_a_clk.c, 40000000);
	clk_prepare_enable(&mmssnoc_ahb_a_clk.c);
	/*
	 * Hold an active set vote for CXO; this is because CXO is expected
	 * to remain on whenever CPUs aren't power collapsed.
	 */
	clk_prepare_enable(&xo_a_clk_src.c);

	/*
	 * TODO: Temporarily enable NOC configuration AHB clocks. Remove when
	 * the bus driver is ready.
	 */
	clk_prepare_enable(&gcc_mmss_noc_cfg_ahb_clk.c);
}

#define GCC_CC_PHYS		0xFC400000
#define GCC_CC_SIZE		SZ_16K

#define MMSS_CC_PHYS		0xFD8C0000
#define MMSS_CC_SIZE		SZ_256K

#define LPASS_CC_PHYS		0xFE000000
#define LPASS_CC_SIZE		SZ_256K

/* APCS_KPSS_GLB: 0xF9000000 + 0x00011000 (0xF9011000)*/
#define APCS_GCC_GLB_PHYS	0xF9011000
#define APCS_GCC_GLB_SIZE	SZ_4K

/* BCSS_CLK_CTRL: 0xFC600000 + 0x00144000 (0xFC744000) */
#define BCSS_CC_PHYS		0xFC744000
#define BCSS_CC_SIZE		SZ_4K

static void __init mpq8092_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-8092: Unable to ioremap GCC memory!");

	virt_bases[MMSS_BASE] = ioremap(MMSS_CC_PHYS, MMSS_CC_SIZE);
	if (!virt_bases[MMSS_BASE])
		panic("clock-8092: Unable to ioremap MMSS_CC memory!");

	virt_bases[LPASS_BASE] = ioremap(LPASS_CC_PHYS, LPASS_CC_SIZE);
	if (!virt_bases[LPASS_BASE])
		panic("clock-8092: Unable to ioremap LPASS_CC memory!");

	virt_bases[APCS_BASE] = ioremap(APCS_GCC_GLB_PHYS, APCS_GCC_GLB_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-8092: Unable to ioremap APCS_GCC_GLB memory!");

	virt_bases[BCSS_BASE] = ioremap(BCSS_CC_PHYS, BCSS_CC_SIZE);
	if (!virt_bases[APCS_BASE])
		panic("clock-8092: Unable to ioremap BCSS_CC memory!");

	clk_ops_local_pll.enable = sr_hpm_lp_pll_clk_enable;

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-8092: Unable to get the vdd_dig regulator!");

	enable_rpm_scaling();

	reg_init();
}

static void __init mpq8092_rumi_clock_pre_init(void)
{
	virt_bases[GCC_BASE] = ioremap(GCC_CC_PHYS, GCC_CC_SIZE);
	if (!virt_bases[GCC_BASE])
		panic("clock-8092: Unable to ioremap GCC memory!");

	vdd_dig.regulator[0] = regulator_get(NULL, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0]))
		panic("clock-8092: Unable to get the vdd_dig regulator!");
}

struct clock_init_data mpq8092_rumi_clock_init_data __initdata = {
	.table = mpq_clocks_8092_rumi,
	.size = ARRAY_SIZE(mpq_clocks_8092_rumi),
	.pre_init = mpq8092_rumi_clock_pre_init,
};

struct clock_init_data mpq8092_clock_init_data __initdata = {
	.table = mpq_clocks_8092,
	.size = ARRAY_SIZE(mpq_clocks_8092),
	.pre_init = mpq8092_clock_pre_init,
	.post_init = mpq8092_clock_post_init,
};
