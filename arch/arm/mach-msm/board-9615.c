/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/msm_ssbi.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/mmc.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/msm_spi.h>
#include "timer.h"
#include "devices.h"
#include "board-9615.h"
#include "cpuidle.h"
#include "pm.h"

static struct platform_device *common_devices[] = {
	&msm9615_device_dmov,
	&msm_device_smd,
	&msm9615_device_uart_gsbi4,
	&msm9615_device_ssbi_pmic1,
	&msm9615_device_qup_i2c_gsbi5,
	&msm9615_device_qup_spi_gsbi3,
	&msm_device_sps,
	&msm9615_device_tsens,
	&msm_device_nand,
	&msm_rpm_device,

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
		&qcrypto_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
	&qcedev_device,
#endif
};

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata __devinitdata = {
	.irq_base		= PM8018_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(87),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata __devinitdata = {
	.gpio_base		= PM8018_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata __devinitdata = {
	.mpp_base		= PM8018_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_rtc_platform_data pm8xxx_rtc_pdata __devinitdata = {
	.rtc_write_enable	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 970,
	.wakeup			= 1,
};

static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

static struct pm8018_platform_data pm8018_platform_data __devinitdata = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.rtc_pdata		= &pm8xxx_rtc_pdata,
	.pwrkey_pdata		= &pm8xxx_pwrkey_pdata,
	.misc_pdata		= &pm8xxx_misc_pdata,
	.regulator_pdatas	= msm_pm8018_regulator_pdata,
};

static struct msm_ssbi_platform_data msm9615_ssbi_pm8018_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name		= PM8018_CORE_DEV_NAME,
		.platform_data	= &pm8018_platform_data,
	},
};

static struct platform_device msm9615_device_rpm_regulator __devinitdata = {
	.name	= "rpm-regulator",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_regulator_9615_pdata,
	},
};

static struct gpiomux_setting ps_hold = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi4 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi3 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi3_cs1_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

struct msm_gpiomux_config msm9615_ps_hold_config[] __initdata = {
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ps_hold,
		},
	},
};

struct msm_gpiomux_config msm9615_gsbi_configs[] __initdata = {
	{
		.gpio      = 8,		/* GSBI3 QUP SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 9,		/* GSBI3 QUP SPI_CS_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 10,	/* GSBI3 QUP SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 11,	/* GSBI3 QUP SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 12,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 13,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 14,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 15,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 16,	/* GSBI5 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
	{
		.gpio      = 17,	/* GSBI5 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
	{
		/* GPIO 19 can be used for I2C/UART on GSBI5 */
		.gpio      = 19,	/* GSBI3 QUP SPI_CS_1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3_cs1_config,
		},
	},
};

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

#define QCE_SIZE		0x10000
#define QCE_0_BASE		0x18500000

#define QCE_HW_KEY_SUPPORT	0
#define QCE_SHA_HMAC_SUPPORT	1
#define QCE_SHARE_CE_RESOURCE	1
#define QCE_CE_SHARED		0

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct resource qcedev_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

#endif

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)

static struct msm_ce_hw_support qcrypto_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
};

static struct platform_device qcrypto_device = {
	.name		= "qcrypto",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcrypto_resources),
	.resource	= qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcrypto_ce_hw_suppport,
	},
};
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

static struct msm_ce_hw_support qcedev_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
};

static struct platform_device qcedev_device = {
	.name		= "qce",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcedev_resources),
	.resource	= qcedev_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcedev_ce_hw_suppport,
	},
};
#endif

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT))

#define GPIO_SDCARD_PWR_EN 18

/* MDM9x15 have 2 SDCC controllers */
enum sdcc_controllers {
	SDCC1,
	SDCC2,
	MAX_SDCC_CONTROLLER
};

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
/* SDC1 pad data */
static struct msm_mmc_pad_drv sdc1_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_16MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_10MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_10MA}
};

static struct msm_mmc_pad_drv sdc1_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_DOWN}
};

static struct msm_mmc_pad_pull_data mmc_pad_pull_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_pull_on_cfg,
		.off = sdc1_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_pull_on_cfg)
	},
};

static struct msm_mmc_pad_drv_data mmc_pad_drv_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_drv_on_cfg,
		.off = sdc1_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_drv_on_cfg)
	},
};

static struct msm_mmc_pad_data mmc_pad_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pull = &mmc_pad_pull_data[SDCC1],
		.drv = &mmc_pad_drv_data[SDCC1]
	},
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct gpiomux_setting sdcc2_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdcc2_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc2_suspend_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm9615_sdcc2_configs[] __initdata = {
	{
		/* SDC2_DATA_0 */
		.gpio      = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		/* SDC2_DATA_1 */
		.gpio      = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		/* SDC2_DATA_2 */
		.gpio      = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		/* SDC2_DATA_3 */
		.gpio      = 28,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		/* SDC2_CMD GSBI1 */
		.gpio      = 29,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		/* SDC2_CLK GSBI1 */
		.gpio      = 30,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
};

static struct msm_mmc_gpio sdc2_gpio_cfg[] = {
	{25, "sdc2_dat_0"},
	{26, "sdc2_dat_1"},
	{27, "sdc2_dat_2"},
	{28, "sdc2_dat_3"},
	{29, "sdc2_cmd"},
	{30, "sdc2_clk"},
};

static struct msm_mmc_gpio_data mmc_gpio_data[MAX_SDCC_CONTROLLER] = {
	[SDCC2] = {
		.gpio = sdc2_gpio_cfg,
		.size = ARRAY_SIZE(sdc2_gpio_cfg),
	},
};
#else
static struct msm_gpiomux_config msm9615_sdcc2_configs[0];
#endif

static struct msm_mmc_pin_data mmc_slot_pin_data[MAX_SDCC_CONTROLLER] = {
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	[SDCC1] = {
		.is_gpio = 0,
		.pad_data = &mmc_pad_data[SDCC1],
	},
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	[SDCC2] = {
		.is_gpio = 1,
		.gpio_data = &mmc_gpio_data[SDCC2],
	},
#endif
};

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static unsigned int sdc1_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc1_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc1_sup_clk_rates),
	.sdcc_v4_sup    = true,
	.pin_data	= &mmc_slot_pin_data[SDCC1],
};
static struct mmc_platform_data *msm9615_sdc1_pdata = &sdc1_data;
#else
static struct mmc_platform_data *msm9615_sdc1_pdata;
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static unsigned int sdc2_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data sdc2_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc2_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc2_sup_clk_rates),
	.sdcc_v4_sup    = true,
	.pin_data	= &mmc_slot_pin_data[SDCC2],
};
static struct mmc_platform_data *msm9615_sdc2_pdata = &sdc2_data;
#else
static struct mmc_platform_data *msm9615_sdc2_pdata;
#endif

static void __init msm9615_init_mmc(void)
{
	int ret;

	if (msm9615_sdc1_pdata) {
		ret = gpio_request(GPIO_SDCARD_PWR_EN, "SDCARD_PWR_EN");

		if (ret) {
			pr_err("%s: sdcc1: Error requesting GPIO "
				"SDCARD_PWR_EN:%d\n", __func__, ret);
		} else {
			ret = gpio_direction_output(GPIO_SDCARD_PWR_EN, 1);
			if (ret) {
				pr_err("%s: sdcc1: Error setting o/p direction"
					" for GPIO SDCARD_PWR_EN:%d\n",
					__func__, ret);
				gpio_free(GPIO_SDCARD_PWR_EN);
			} else {
				msm_add_sdcc(1, msm9615_sdc1_pdata);
			}
		}
	}

	if (msm9615_sdc2_pdata) {
		msm_gpiomux_install(msm9615_sdcc2_configs,
			ARRAY_SIZE(msm9615_sdcc2_configs));

		/* SDC2: External card slot */
		msm_add_sdcc(2, msm9615_sdc2_pdata);
	}
}
#else
static void __init msm9615_init_mmc(void) { }
#endif
static  struct msm_cpuidle_state msm_cstates[] __initdata = {
	{0, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{0, 1, "C1", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{0, 2, "C2", "POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE},
};
static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
	},
};

static int __init gpiomux_init(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}
	msm_gpiomux_install(msm9615_gsbi_configs,
			ARRAY_SIZE(msm9615_gsbi_configs));

	msm_gpiomux_install(msm9615_ps_hold_config,
			ARRAY_SIZE(msm9615_ps_hold_config));
	return 0;
}

static struct msm_spi_platform_data msm9615_qup_spi_gsbi3_pdata = {
	.max_clock_speed = 24000000,
};

static struct msm_i2c_platform_data msm9615_i2c_qup_gsbi5_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

static void __init msm9615_i2c_init(void)
{
	msm9615_device_qup_i2c_gsbi5.dev.platform_data =
					&msm9615_i2c_qup_gsbi5_pdata;
}

static void __init msm9615_common_init(void)
{
	msm9615_device_init();
	gpiomux_init();
	msm9615_i2c_init();
	regulator_suppress_info_printing();
	platform_device_register(&msm9615_device_rpm_regulator);
	msm9615_device_qup_spi_gsbi3.dev.platform_data =
				&msm9615_qup_spi_gsbi3_pdata;
	msm9615_device_ssbi_pmic1.dev.platform_data =
						&msm9615_ssbi_pm8018_pdata;
	pm8018_platform_data.num_regulators = msm_pm8018_regulator_pdata_len;
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));

	msm9615_init_mmc();
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_APCC_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
						msm_pm_data);
}

static void __init msm9615_cdp_init(void)
{
	msm9615_common_init();
}

static void __init msm9615_mtp_init(void)
{
	msm9615_common_init();
}

MACHINE_START(MSM9615_CDP, "QCT MSM9615 CDP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_cdp_init,
MACHINE_END

MACHINE_START(MSM9615_MTP, "QCT MSM9615 MTP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_mtp_init,
MACHINE_END
