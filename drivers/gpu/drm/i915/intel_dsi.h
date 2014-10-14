/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _INTEL_DSI_H
#define _INTEL_DSI_H

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "intel_drv.h"

#define HV_DDI0_HPD_GPIONC_0_PCONF0		0x4130
#define HV_DDI0_HPD_GPIONC_0_PAD		0x4138
#define HV_DDI0_DDC_SDA_GPIONC_1_PCONF0		0x4120
#define HV_DDI0_DDC_SDA_GPIONC_1_PAD		0x4128
#define HV_DDI0_DDC_SCL_GPIONC_2_PCONF0		0x4110
#define HV_DDI0_DDC_SCL_GPIONC_2_PAD		0x4118
#define PANEL0_VDDEN_GPIONC_3_PCONF0		0x4140
#define PANEL0_VDDEN_GPIONC_3_PAD		0x4148
#define PANEL0_BKLTEN_GPIONC_4_PCONF0		0x4150
#define PANEL0_BKLTEN_GPIONC_4_PAD		0x4158
#define PANEL0_BKLTCTL_GPIONC_5_PCONF0		0x4160
#define PANEL0_BKLTCTL_GPIONC_5_PAD		0x4168
#define HV_DDI1_HPD_GPIONC_6_PCONF0		0x4180
#define HV_DDI1_HPD_GPIONC_6_PAD		0x4188
#define HV_DDI1_DDC_SDA_GPIONC_7_PCONF0		0x4190
#define HV_DDI1_DDC_SDA_GPIONC_7_PAD		0x4198
#define HV_DDI1_DDC_SCL_GPIONC_8_PCONF0		0x4170
#define HV_DDI1_DDC_SCL_GPIONC_8_PAD		0x4178
#define PANEL1_VDDEN_GPIONC_9_PCONF0		0x4100
#define PANEL1_VDDEN_GPIONC_9_PAD		0x4108
#define PANEL1_BKLTEN_GPIONC_10_PCONF0		0x40E0
#define PANEL1_BKLTEN_GPIONC_10_PAD		0x40E8
#define PANEL1_BKLTCTL_GPIONC_11_PCONF0		0x40F0
#define PANEL1_BKLTCTL_GPIONC_11_PAD		0x40F8
#define GP_INTD_DSI_TE1_GPIONC_12_PCONF0	0x40C0
#define GP_INTD_DSI_TE1_GPIONC_12_PAD		0x40C8
#define HV_DDI2_DDC_SDA_GPIONC_13_PCONF0	0x41A0
#define HV_DDI2_DDC_SDA_GPIONC_13_PAD		0x41A8
#define HV_DDI2_DDC_SCL_GPIONC_14_PCONF0	0x41B0
#define HV_DDI2_DDC_SCL_GPIONC_14_PAD		0x41B8
#define GP_CAMERASB00_GPIONC_15_PCONF0		0x4010
#define GP_CAMERASB00_GPIONC_15_PAD		0x4018
#define GP_CAMERASB01_GPIONC_16_PCONF0		0x4040
#define GP_CAMERASB01_GPIONC_16_PAD		0x4048
#define GP_CAMERASB02_GPIONC_17_PCONF0		0x4080
#define GP_CAMERASB02_GPIONC_17_PAD		0x4088
#define GP_CAMERASB03_GPIONC_18_PCONF0		0x40B0
#define GP_CAMERASB03_GPIONC_18_PAD		0x40B8
#define GP_CAMERASB04_GPIONC_19_PCONF0		0x4000
#define GP_CAMERASB04_GPIONC_19_PAD		0x4008
#define GP_CAMERASB05_GPIONC_20_PCONF0		0x4030
#define GP_CAMERASB05_GPIONC_20_PAD		0x4038
#define GP_CAMERASB06_GPIONC_21_PCONF0		0x4060
#define GP_CAMERASB06_GPIONC_21_PAD		0x4068
#define GP_CAMERASB07_GPIONC_22_PCONF0		0x40A0
#define GP_CAMERASB07_GPIONC_22_PAD		0x40A8
#define GP_CAMERASB08_GPIONC_23_PCONF0		0x40D0
#define GP_CAMERASB08_GPIONC_23_PAD		0x40D8
#define GP_CAMERASB09_GPIONC_24_PCONF0		0x4020
#define GP_CAMERASB09_GPIONC_24_PAD		0x4028
#define GP_CAMERASB10_GPIONC_25_PCONF0		0x4050
#define GP_CAMERASB10_GPIONC_25_PAD		0x4058
#define GP_CAMERASB11_GPIONC_26_PCONF0		0x4090
#define GP_CAMERASB11_GPIONC_26_PAD		0x4098

#define SATA_GP0_GPIOC_0_PCONF0			0x4550
#define SATA_GP0_GPIOC_0_PAD			0x4558
#define SATA_GP1_GPIOC_1_PCONF0			0x4590
#define SATA_GP1_GPIOC_1_PAD			0x4598
#define SATA_LEDN_GPIOC_2_PCONF0		0x45D0
#define SATA_LEDN_GPIOC_2_PAD			0x45D8
#define PCIE_CLKREQ0B_GPIOC_3_PCONF0		0x4600
#define PCIE_CLKREQ0B_GPIOC_3_PAD		0x4608
#define PCIE_CLKREQ1B_GPIOC_4_PCONF0		0x4630
#define PCIE_CLKREQ1B_GPIOC_4_PAD		0x4638
#define PCIE_CLKREQ2B_GPIOC_5_PCONF0		0x4660
#define PCIE_CLKREQ2B_GPIOC_5_PAD		0x4668
#define PCIE_CLKREQ3B_GPIOC_6_PCONF0		0x4620
#define PCIE_CLKREQ3B_GPIOC_6_PAD		0x4628
#define PCIE_CLKREQ4B_GPIOC_7_PCONF0		0x4650
#define PCIE_CLKREQ4B_GPIOC_7_PAD		0x4658
#define HDA_RSTB_GPIOC_8_PCONF0			0x4220
#define HDA_RSTB_GPIOC_8_PAD			0x4228
#define HDA_SYNC_GPIOC_9_PCONF0			0x4250
#define HDA_SYNC_GPIOC_9_PAD			0x4258
#define HDA_CLK_GPIOC_10_PCONF0			0x4240
#define HDA_CLK_GPIOC_10_PAD			0x4248
#define HDA_SDO_GPIOC_11_PCONF0			0x4260
#define HDA_SDO_GPIOC_11_PAD			0x4268
#define HDA_SDI0_GPIOC_12_PCONF0		0x4270
#define HDA_SDI0_GPIOC_12_PAD			0x4278
#define HDA_SDI1_GPIOC_13_PCONF0		0x4230
#define HDA_SDI1_GPIOC_13_PAD			0x4238
#define HDA_DOCKRSTB_GPIOC_14_PCONF0		0x4280
#define HDA_DOCKRSTB_GPIOC_14_PAD		0x4288
#define HDA_DOCKENB_GPIOC_15_PCONF0		0x4540
#define HDA_DOCKENB_GPIOC_15_PAD		0x4548
#define SDMMC1_CLK_GPIOC_16_PCONF0		0x43E0
#define SDMMC1_CLK_GPIOC_16_PAD			0x43E8
#define SDMMC1_D0_GPIOC_17_PCONF0		0x43D0
#define SDMMC1_D0_GPIOC_17_PAD			0x43D8
#define SDMMC1_D1_GPIOC_18_PCONF0		0x4400
#define SDMMC1_D1_GPIOC_18_PAD			0x4408
#define SDMMC1_D2_GPIOC_19_PCONF0		0x43B0
#define SDMMC1_D2_GPIOC_19_PAD			0x43B8
#define SDMMC1_D3_CD_B_GPIOC_20_PCONF0		0x4360
#define SDMMC1_D3_CD_B_GPIOC_20_PAD		0x4368
#define MMC1_D4_SD_WE_GPIOC_21_PCONF0		0x4380
#define MMC1_D4_SD_WE_GPIOC_21_PAD		0x4388
#define MMC1_D5_GPIOC_22_PCONF0			0x43C0
#define MMC1_D5_GPIOC_22_PAD			0x43C8
#define MMC1_D6_GPIOC_23_PCONF0			0x4370
#define MMC1_D6_GPIOC_23_PAD			0x4378
#define MMC1_D7_GPIOC_24_PCONF0			0x43F0
#define MMC1_D7_GPIOC_24_PAD			0x43F8
#define SDMMC1_CMD_GPIOC_25_PCONF0		0x4390
#define SDMMC1_CMD_GPIOC_25_PAD			0x4398
#define MMC1_RESET_B_GPIOC_26_PCONF0		0x4330
#define MMC1_RESET_B_GPIOC_26_PAD		0x4338
#define SDMMC2_CLK_GPIOC_27_PCONF0		0x4320
#define SDMMC2_CLK_GPIOC_27_PAD			0x4328
#define SDMMC2_D0_GPIOC_28_PCONF0		0x4350
#define SDMMC2_D0_GPIOC_28_PAD			0x4358
#define SDMMC2_D1_GPIOC_29_PCONF0		0x42F0
#define SDMMC2_D1_GPIOC_29_PAD			0x42F8
#define SDMMC2_D2_GPIOC_30_PCONF0		0x4340
#define SDMMC2_D2_GPIOC_30_PAD			0x4348
#define SDMMC2_D3_CD_B_GPIOC_31_PCONF0		0x4310
#define SDMMC2_D3_CD_B_GPIOC_31_PAD		0x4318
#define SDMMC2_CMD_GPIOC_32_PCONF0		0x4300
#define SDMMC2_CMD_GPIOC_32_PAD			0x4308
#define SDMMC3_CLK_GPIOC_33_PCONF0		0x42B0
#define SDMMC3_CLK_GPIOC_33_PAD			0x42B8
#define SDMMC3_D0_GPIOC_34_PCONF0		0x42E0
#define SDMMC3_D0_GPIOC_34_PAD			0x42E8
#define SDMMC3_D1_GPIOC_35_PCONF0		0x4290
#define SDMMC3_D1_GPIOC_35_PAD			0x4298
#define SDMMC3_D2_GPIOC_36_PCONF0		0x42D0
#define SDMMC3_D2_GPIOC_36_PAD			0x42D8
#define SDMMC3_D3_GPIOC_37_PCONF0		0x42A0
#define SDMMC3_D3_GPIOC_37_PAD			0x42A8
#define SDMMC3_CD_B_GPIOC_38_PCONF0		0x43A0
#define SDMMC3_CD_B_GPIOC_38_PAD		0x43A8
#define SDMMC3_CMD_GPIOC_39_PCONF0		0x42C0
#define SDMMC3_CMD_GPIOC_39_PAD			0x42C8
#define SDMMC3_1P8_EN_GPIOC_40_PCONF0		0x45F0
#define SDMMC3_1P8_EN_GPIOC_40_PAD		0x45F8
#define SDMMC3_PWR_EN_B_GPIOC_41_PCONF0		0x4690
#define SDMMC3_PWR_EN_B_GPIOC_41_PAD		0x4698
#define LPC_AD0_GPIOC_42_PCONF0			0x4460
#define LPC_AD0_GPIOC_42_PAD			0x4468
#define LPC_AD1_GPIOC_43_PCONF0			0x4440
#define LPC_AD1_GPIOC_43_PAD			0x4448
#define LPC_AD2_GPIOC_44_PCONF0			0x4430
#define LPC_AD2_GPIOC_44_PAD			0x4438
#define LPC_AD3_GPIOC_45_PCONF0			0x4420
#define LPC_AD3_GPIOC_45_PAD			0x4428
#define LPC_FRAMEB_GPIOC_46_PCONF0		0x4450
#define LPC_FRAMEB_GPIOC_46_PAD			0x4458
#define LPC_CLKOUT0_GPIOC_47_PCONF0		0x4470
#define LPC_CLKOUT0_GPIOC_47_PAD		0x4478
#define LPC_CLKOUT1_GPIOC_48_PCONF0		0x4410
#define LPC_CLKOUT1_GPIOC_48_PAD		0x4418
#define LPC_CLKRUNB_GPIOC_49_PCONF0		0x4480
#define LPC_CLKRUNB_GPIOC_49_PAD		0x4488
#define ILB_SERIRQ_GPIOC_50_PCONF0		0x4560
#define ILB_SERIRQ_GPIOC_50_PAD			0x4568
#define SMB_DATA_GPIOC_51_PCONF0		0x45A0
#define SMB_DATA_GPIOC_51_PAD			0x45A8
#define SMB_CLK_GPIOC_52_PCONF0			0x4580
#define SMB_CLK_GPIOC_52_PAD			0x4588
#define SMB_ALERTB_GPIOC_53_PCONF0		0x45C0
#define SMB_ALERTB_GPIOC_53_PAD			0x45C8
#define SPKR_GPIOC_54_PCONF0			0x4670
#define SPKR_GPIOC_54_PAD			0x4678
#define MHSI_ACDATA_GPIOC_55_PCONF0		0x44D0
#define MHSI_ACDATA_GPIOC_55_PAD		0x44D8
#define MHSI_ACFLAG_GPIOC_56_PCONF0		0x44F0
#define MHSI_ACFLAG_GPIOC_56_PAD		0x44F8
#define MHSI_ACREADY_GPIOC_57_PCONF0		0x4530
#define MHSI_ACREADY_GPIOC_57_PAD		0x4538
#define MHSI_ACWAKE_GPIOC_58_PCONF0		0x44E0
#define MHSI_ACWAKE_GPIOC_58_PAD		0x44E8
#define MHSI_CADATA_GPIOC_59_PCONF0		0x4510
#define MHSI_CADATA_GPIOC_59_PAD		0x4518
#define MHSI_CAFLAG_GPIOC_60_PCONF0		0x4500
#define MHSI_CAFLAG_GPIOC_60_PAD		0x4508
#define MHSI_CAREADY_GPIOC_61_PCONF0		0x4520
#define MHSI_CAREADY_GPIOC_61_PAD		0x4528
#define GP_SSP_2_CLK_GPIOC_62_PCONF0		0x40D0
#define GP_SSP_2_CLK_GPIOC_62_PAD		0x40D8
#define GP_SSP_2_FS_GPIOC_63_PCONF0		0x40C0
#define GP_SSP_2_FS_GPIOC_63_PAD		0x40C8
#define GP_SSP_2_RXD_GPIOC_64_PCONF0		0x40F0
#define GP_SSP_2_RXD_GPIOC_64_PAD		0x40F8
#define GP_SSP_2_TXD_GPIOC_65_PCONF0		0x40E0
#define GP_SSP_2_TXD_GPIOC_65_PAD		0x40E8
#define SPI1_CS0_B_GPIOC_66_PCONF0		0x4110
#define SPI1_CS0_B_GPIOC_66_PAD			0x4118
#define SPI1_MISO_GPIOC_67_PCONF0		0x4120
#define SPI1_MISO_GPIOC_67_PAD			0x4128
#define SPI1_MOSI_GPIOC_68_PCONF0		0x4130
#define SPI1_MOSI_GPIOC_68_PAD			0x4138
#define SPI1_CLK_GPIOC_69_PCONF0		0x4100
#define SPI1_CLK_GPIOC_69_PAD			0x4108
#define UART1_RXD_GPIOC_70_PCONF0		0x4020
#define UART1_RXD_GPIOC_70_PAD			0x4028
#define UART1_TXD_GPIOC_71_PCONF0		0x4010
#define UART1_TXD_GPIOC_71_PAD			0x4018
#define UART1_RTS_B_GPIOC_72_PCONF0		0x4000
#define UART1_RTS_B_GPIOC_72_PAD		0x4008
#define UART1_CTS_B_GPIOC_73_PCONF0		0x4040
#define UART1_CTS_B_GPIOC_73_PAD		0x4048
#define UART2_RXD_GPIOC_74_PCONF0		0x4060
#define UART2_RXD_GPIOC_74_PAD			0x4068
#define UART2_TXD_GPIOC_75_PCONF0		0x4070
#define UART2_TXD_GPIOC_75_PAD			0x4078
#define UART2_RTS_B_GPIOC_76_PCONF0		0x4090
#define UART2_RTS_B_GPIOC_76_PAD		0x4098
#define UART2_CTS_B_GPIOC_77_PCONF0		0x4080
#define UART2_CTS_B_GPIOC_77_PAD		0x4088
#define I2C0_SDA_GPIOC_78_PCONF0		0x4210
#define I2C0_SDA_GPIOC_78_PAD			0x4218
#define I2C0_SCL_GPIOC_79_PCONF0		0x4200
#define I2C0_SCL_GPIOC_79_PAD			0x4208
#define I2C1_SDA_GPIOC_80_PCONF0		0x41F0
#define I2C1_SDA_GPIOC_80_PAD			0x41F8
#define I2C1_SCL_GPIOC_81_PCONF0		0x41E0
#define I2C1_SCL_GPIOC_81_PAD			0x41E8
#define I2C2_SDA_GPIOC_82_PCONF0		0x41D0
#define I2C2_SDA_GPIOC_82_PAD			0x41D8
#define I2C2_SCL_GPIOC_83_PCONF0		0x41B0
#define I2C2_SCL_GPIOC_83_PAD			0x41B8
#define I2C3_SDA_GPIOC_84_PCONF0		0x4190
#define I2C2_SCL_GPIOC_83_PAD			0x41B8
#define I2C3_SDA_GPIOC_84_PCONF0		0x4190
#define I2C3_SDA_GPIOC_84_PAD			0x4198
#define I2C3_SCL_GPIOC_85_PCONF0		0x41C0
#define I2C3_SCL_GPIOC_85_PAD			0x41C8
#define I2C4_SDA_GPIOC_86_PCONF0		0x41A0
#define I2C4_SDA_GPIOC_86_PAD			0x41A8
#define I2C4_SCL_GPIOC_87_PCONF0		0x4170
#define I2C4_SCL_GPIOC_87_PAD			0x4178
#define I2C5_SDA_GPIOC_88_PCONF0		0x4150
#define I2C5_SDA_GPIOC_88_PAD			0x4158
#define I2C5_SCL_GPIOC_89_PCONF0		0x4140
#define I2C5_SCL_GPIOC_89_PAD			0x4148
#define I2C6_SDA_GPIOC_90_PCONF0		0x4180
#define I2C6_SDA_GPIOC_90_PAD			0x4188
#define I2C6_SCL_GPIOC_91_PCONF0		0x4160
#define I2C6_SCL_GPIOC_91_PAD			0x4168
#define I2C_NFC_SDA_GPIOC_92_PCONF0		0x4050
#define I2C_NFC_SDA_GPIOC_92_PAD		0x4058
#define I2C_NFC_SCL_GPIOC_93_PCONF0		0x4030
#define I2C_NFC_SCL_GPIOC_93_PAD		0x4038
#define PWM0_GPIOC_94_PCONF0			0x40A0
#define PWM0_GPIOC_94_PAD			0x40A8
#define PWM1_GPIOC_95_PCONF0			0x40B0
#define PWM1_GPIOC_95_PAD			0x40B8
#define PLT_CLK0_GPIOC_96_PCONF0		0x46A0
#define PLT_CLK0_GPIOC_96_PAD			0x46A8
#define PLT_CLK1_GPIOC_97_PCONF0		0x4570
#define PLT_CLK1_GPIOC_97_PAD			0x4578
#define PLT_CLK2_GPIOC_98_PCONF0		0x45B0
#define PLT_CLK2_GPIOC_98_PAD			0x45B8
#define PLT_CLK3_GPIOC_99_PCONF0		0x4680
#define PLT_CLK3_GPIOC_99_PAD			0x4688
#define PLT_CLK4_GPIOC_100_PCONF0		0x4610
#define PLT_CLK4_GPIOC_100_PAD			0x4618
#define PLT_CLK5_GPIOC_101_PCONF0		0x4640
#define PLT_CLK5_GPIOC_101_PAD			0x4648

#define GPIO_SUS0_GPIO_SUS0_PCONF0		0x41D0
#define GPIO_SUS0_GPIO_SUS0_PAD			0x41D8
#define GPIO_SUS1_GPIO_SUS1_PCONF0		0x4210
#define GPIO_SUS1_GPIO_SUS1_PAD			0x4218
#define GPIO_SUS2_GPIO_SUS2_PCONF0		0x41E0
#define GPIO_SUS2_GPIO_SUS2_PAD			0x41E8
#define GPIO_SUS3_GPIO_SUS3_PCONF0		0x41F0
#define GPIO_SUS3_GPIO_SUS3_PAD			0x41F8
#define GPIO_SUS4_GPIO_SUS4_PCONF0		0x4200
#define GPIO_SUS4_GPIO_SUS4_PAD			0x4208
#define GPIO_SUS5_GPIO_SUS5_PCONF0		0x4220
#define GPIO_SUS5_GPIO_SUS5_PAD			0x4228
#define GPIO_SUS6_GPIO_SUS6_PCONF0		0x4240
#define GPIO_SUS6_GPIO_SUS6_PAD			0x4248
#define GPIO_SUS7_GPIO_SUS7_PCONF0		0x4230
#define GPIO_SUS7_GPIO_SUS7_PAD			0x4238
#define SEC_GPIO_SUS8_GPIO_SUS8_PCONF0		0x4260
#define SEC_GPIO_SUS8_GPIO_SUS8_PAD		0x4268
#define SEC_GPIO_SUS9_GPIO_SUS9_PCONF0		0x4250
#define SEC_GPIO_SUS9_GPIO_SUS9_PAD		0x4258
#define SEC_GPIO_SUS10_GPIO_SUS10_PCONF0	0x4120
#define SEC_GPIO_SUS10_GPIO_SUS10_PAD		0x4128
#define SUSPWRDNACK_GPIOS_11_PCONF0		0x4070
#define SUSPWRDNACK_GPIOS_11_PAD		0x4078
#define PMU_SUSCLK_GPIOS_12_PCONF0		0x40B0
#define PMU_SUSCLK_GPIOS_12_PAD			0x40B8
#define PMU_SLP_S0IX_B_GPIOS_13_PCONF0		0x4140
#define PMU_SLP_S0IX_B_GPIOS_13_PAD		0x4148
#define PMU_SLP_LAN_B_GPIOS_14_PCONF0		0x4110
#define PMU_SLP_LAN_B_GPIOS_14_PAD		0x4118
#define PMU_WAKE_B_GPIOS_15_PCONF0		0x4010
#define PMU_WAKE_B_GPIOS_15_PAD			0x4018
#define PMU_PWRBTN_B_GPIOS_16_PCONF0		0x4080
#define PMU_PWRBTN_B_GPIOS_16_PAD		0x4088
#define PMU_WAKE_LAN_B_GPIOS_17_PCONF0		0x40A0
#define PMU_WAKE_LAN_B_GPIOS_17_PAD		0x40A8
#define SUS_STAT_B_GPIOS_18_PCONF0		0x4130
#define SUS_STAT_B_GPIOS_18_PAD			0x4138
#define USB_OC0_B_GPIOS_19_PCONF0		0x40C0
#define USB_OC0_B_GPIOS_19_PAD			0x40C8
#define USB_OC1_B_GPIOS_20_PCONF0		0x4000
#define USB_OC1_B_GPIOS_20_PAD			0x4008
#define SPI_CS1_B_GPIOS_21_PCONF0		0x4020
#define SPI_CS1_B_GPIOS_21_PAD			0x4028
#define GPIO_DFX0_GPIOS_22_PCONF0		0x4170
#define GPIO_DFX0_GPIOS_22_PAD			0x4178
#define GPIO_DFX1_GPIOS_23_PCONF0		0x4270
#define GPIO_DFX1_GPIOS_23_PAD			0x4278
#define GPIO_DFX2_GPIOS_24_PCONF0		0x41C0
#define GPIO_DFX2_GPIOS_24_PAD			0x41C8
#define GPIO_DFX3_GPIOS_25_PCONF0		0x41B0
#define GPIO_DFX3_GPIOS_25_PAD			0x41B8
#define GPIO_DFX4_GPIOS_26_PCONF0		0x4160
#define GPIO_DFX4_GPIOS_26_PAD			0x4168
#define GPIO_DFX5_GPIOS_27_PCONF0		0x4150
#define GPIO_DFX5_GPIOS_27_PAD			0x4158
#define GPIO_DFX6_GPIOS_28_PCONF0		0x4180
#define GPIO_DFX6_GPIOS_28_PAD			0x4188
#define GPIO_DFX7_GPIOS_29_PCONF0		0x4190
#define GPIO_DFX7_GPIOS_29_PAD			0x4198
#define GPIO_DFX8_GPIOS_30_PCONF0		0x41A0
#define GPIO_DFX8_GPIOS_30_PAD			0x41A8
#define USB_ULPI_0_CLK_GPIOS_31_PCONF0		0x4330
#define USB_ULPI_0_CLK_GPIOS_31_PAD		0x4338
#define USB_ULPI_0_DATA0_GPIOS_32_PCONF0	0x4380
#define USB_ULPI_0_DATA0_GPIOS_32_PAD		0x4388
#define USB_ULPI_0_DATA1_GPIOS_33_PCONF0	0x4360
#define USB_ULPI_0_DATA1_GPIOS_33_PAD		0x4368
#define USB_ULPI_0_DATA2_GPIOS_34_PCONF0	0x4310
#define USB_ULPI_0_DATA2_GPIOS_34_PAD		0x4318
#define USB_ULPI_0_DATA3_GPIOS_35_PCONF0	0x4370
#define USB_ULPI_0_DATA3_GPIOS_35_PAD		0x4378
#define USB_ULPI_0_DATA4_GPIOS_36_PCONF0	0x4300
#define USB_ULPI_0_DATA4_GPIOS_36_PAD		0x4308
#define USB_ULPI_0_DATA5_GPIOS_37_PCONF0	0x4390
#define USB_ULPI_0_DATA5_GPIOS_37_PAD		0x4398
#define USB_ULPI_0_DATA6_GPIOS_38_PCONF0	0x4320
#define USB_ULPI_0_DATA6_GPIOS_38_PAD		0x4328
#define USB_ULPI_0_DATA7_GPIOS_39_PCONF0	0x43A0
#define USB_ULPI_0_DATA7_GPIOS_39_PAD		0x43A8
#define USB_ULPI_0_DIR_GPIOS_40_PCONF0		0x4340
#define USB_ULPI_0_DIR_GPIOS_40_PAD		0x4348
#define USB_ULPI_0_NXT_GPIOS_41_PCONF0		0x4350
#define USB_ULPI_0_NXT_GPIOS_41_PAD		0x4358
#define USB_ULPI_0_STP_GPIOS_42_PCONF0		0x43B0
#define USB_ULPI_0_STP_GPIOS_42_PAD		0x43B8
#define USB_ULPI_0_REFCLK_GPIOS_43_PCONF0	0x4280
#define USB_ULPI_0_REFCLK_GPIOS_43_PAD		0x4288

#define PMIC_PANEL_EN		0x52
#define PMIC_PWM_EN		0x51
#define PMIC_BKL_EN		0x4B
#define PMIC_PWM_LEVEL		0x4E


/* Dual Link support */
#define MIPI_DUAL_LINK_NONE		0
#define MIPI_DUAL_LINK_FRONT_BACK	1
#define MIPI_DUAL_LINK_PIXEL_ALT	2

struct intel_dsi_device {
	unsigned int panel_id;
	const char *name;
	struct intel_dsi_dev_ops *dev_ops;
	void *dev_priv;
};

struct intel_dsi_dev_ops {
	bool (*init)(struct intel_dsi_device *dsi);

	void (*get_info)(int pipe, struct drm_connector *connector);

	void (*panel_reset)(struct intel_dsi_device *dsi);

	void (*disable_panel_power)(struct intel_dsi_device *dsi);

	/* one time programmable commands if needed */
	void (*send_otp_cmds)(struct intel_dsi_device *dsi);

	/* This callback must be able to assume DSI commands can be sent */
	void (*enable)(struct intel_dsi_device *dsi);

	/* This callback must be able to assume DSI commands can be sent */
	void (*disable)(struct intel_dsi_device *dsi);

	void (*enable_backlight)(struct intel_dsi_device *dsi);

	void (*disable_backlight)(struct intel_dsi_device *dsi);

	int (*mode_valid)(struct intel_dsi_device *dsi,
			  struct drm_display_mode *mode);

	bool (*mode_fixup)(struct intel_dsi_device *dsi,
			   const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);

	void (*mode_set)(struct intel_dsi_device *dsi,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);

	enum drm_connector_status (*detect)(struct intel_dsi_device *dsi);

	bool (*get_hw_state)(struct intel_dsi_device *dev);

	struct drm_display_mode *(*get_modes)(struct intel_dsi_device *dsi);

	void (*destroy) (struct intel_dsi_device *dsi);
	void (*power_on)(struct intel_dsi_device *dsi);
	void (*power_off)(struct intel_dsi_device *dsi);
};

struct intel_dsi {
	struct intel_encoder base;

	struct intel_dsi_device dev;

	struct intel_connector *attached_connector;

	/* if true, use HS mode, otherwise LP */
	bool hs;

	/* virtual channel */
	int channel;

	/* Video mode or command mode */
	u16 operation_mode;

	/* number of DSI lanes */
	unsigned int lane_count;

	/* video mode pixel format for MIPI_DSI_FUNC_PRG register */
	u32 pixel_format;

	/* video mode format for MIPI_VIDEO_MODE_FORMAT register */
	u32 video_mode_format;

	/* eot for MIPI_EOT_DISABLE register */
	u8 eotp_pkt;
	u8 clock_stop;

	u8 escape_clk_div;
	u32 port_bits;
	u32 bw_timer;
	u32 dphy_reg;
	u32 video_frmt_cfg_bits;
	u16 lp_byte_clk;

	/* timeouts in byte clocks */
	u16 lp_rx_timeout;
	u16 turn_arnd_val;
	u16 rst_timer_val;
	u16 hs_to_lp_count;
	u16 clk_lp_to_hs_count;
	u16 clk_hs_to_lp_count;
	u16 port;
	u16 init_count;

	/* all delays in ms */
	u16 backlight_off_delay;
	u16 backlight_on_delay;
	u16 panel_on_delay;
	u16 panel_off_delay;
	u16 panel_pwr_cycle_delay;

	u8 dual_link;
	u8 pixel_overlap;
};

static inline struct intel_dsi *enc_to_intel_dsi(struct drm_encoder *encoder)
{
	return container_of(encoder, struct intel_dsi, base.base);
}

extern void vlv_enable_dsi_pll(struct intel_encoder *encoder);
extern void vlv_disable_dsi_pll(struct intel_encoder *encoder);

extern struct intel_dsi_dev_ops vbt_generic_dsi_display_ops;
extern struct intel_dsi *intel_attached_dsi(struct drm_connector *connector);


void generic_enable_bklt(struct intel_dsi_device *dsi);
void generic_disable_bklt(struct intel_dsi_device *dsi);
#endif /* _INTEL_DSI_H */
