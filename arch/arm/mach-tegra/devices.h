/*
 * Copyright (C) 2010,2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Erik Gilling <ccross@android.com>
 *
 * Copyright (C) 2010-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_TEGRA_DEVICES_H
#define __MACH_TEGRA_DEVICES_H

#include <linux/platform_device.h>
#include <linux/nvhost.h>
#include <linux/platform_data/tegra_usb.h>

extern struct platform_device tegra_emc_device;
extern struct platform_device tegra_gpio_device;
extern struct platform_device tegra_pinmux_device;
extern struct platform_device tegra_sdhci_device1;
extern struct platform_device tegra_sdhci_device2;
extern struct platform_device tegra_sdhci_device3;
extern struct platform_device tegra_sdhci_device4;
extern struct platform_device tegra_i2c_device1;
extern struct platform_device tegra_i2c_device2;
extern struct platform_device tegra_i2c_device3;
extern struct platform_device tegra_i2c_device4;
extern struct platform_device tegra_kbc_device;
extern struct platform_device tegra_pci_device;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
extern struct platform_device tegra_i2c_device5;
#endif
extern struct platform_device tegra11_i2c_device1;
extern struct platform_device tegra11_i2c_device2;
extern struct platform_device tegra11_i2c_device3;
extern struct platform_device tegra11_i2c_device4;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
extern struct platform_device tegra11_i2c_device5;
#endif
extern struct platform_device tegra_spi_device1;
extern struct platform_device tegra_spi_device2;
extern struct platform_device tegra_spi_device3;
extern struct platform_device tegra_spi_device4;
extern struct platform_device tegra11_spi_device1;
extern struct platform_device tegra11_spi_device2;
extern struct platform_device tegra11_spi_device3;
extern struct platform_device tegra11_spi_device4;
extern struct platform_device tegra_spi_slave_device1;
extern struct platform_device tegra_spi_slave_device2;
extern struct platform_device tegra_spi_slave_device3;
extern struct platform_device tegra_spi_slave_device4;
extern struct platform_device tegra11_spi_slave_device1;
extern struct platform_device tegra11_spi_slave_device2;
extern struct platform_device tegra11_spi_slave_device3;
extern struct platform_device tegra11_spi_slave_device4;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
extern struct platform_device tegra11_spi_device5;
extern struct platform_device tegra11_spi_device6;
extern struct platform_device tegra11_spi_slave_device5;
extern struct platform_device tegra11_spi_slave_device6;
extern struct platform_device tegra_spi_device5;
extern struct platform_device tegra_spi_device6;
extern struct platform_device tegra_spi_slave_device5;
extern struct platform_device tegra_spi_slave_device6;
extern struct platform_device tegra_dtv_device;
#endif
extern struct platform_device tegra_ehci1_device;
extern struct platform_device tegra_ehci2_device;
extern struct platform_device tegra_ehci3_device;
extern struct platform_device tegra_uarta_device;
extern struct platform_device tegra_uartb_device;
extern struct platform_device tegra_uartc_device;
extern struct platform_device tegra_uartd_device;
extern struct platform_device tegra_uarte_device;
extern struct platform_device tegra_pmu_device;
extern struct platform_device tegra_i2s_device1;
extern struct platform_device tegra_i2s_device2;
extern struct platform_device tegra_spdif_device;
extern struct platform_device tegra_das_device;
extern struct platform_device spdif_dit_device;
extern struct platform_device bluetooth_dit_device;
extern struct platform_device baseband_dit_device;
extern struct platform_device tegra_pcm_device;
extern struct platform_device tegra_tdm_pcm_device;
extern struct platform_device tegra_w1_device;
extern struct platform_device tegra_udc_device;
extern struct platform_device tegra_ehci1_device;
extern struct platform_device tegra_ehci2_device;
extern struct platform_device tegra_ehci3_device;
extern struct platform_device tegra_i2s_device1;
extern struct platform_device tegra_i2s_device2;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
extern struct platform_device tegra_i2s_device0;
extern struct platform_device tegra_i2s_device3;
extern struct platform_device tegra_i2s_device4;
extern struct platform_device tegra_ahub_device;
extern struct platform_device tegra_apbif0_device;
extern struct platform_device tegra_apbif1_device;
extern struct platform_device tegra_apbif2_device;
extern struct platform_device tegra_apbif3_device;
extern struct platform_device tegra_dam_device0;
extern struct platform_device tegra_dam_device1;
extern struct platform_device tegra_dam_device2;
extern struct platform_device tegra_hda_device;
extern struct platform_device tegra_sata_device;
#endif
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
extern struct platform_device tegra_gart_device;
#else
extern struct platform_device tegra_smmu_device;
#endif
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
extern struct platform_device tegra_wdt_device;
#else
extern struct platform_device tegra_wdt0_device;
#endif
extern struct platform_device tegra_pwfm0_device;
extern struct platform_device tegra_pwfm1_device;
extern struct platform_device tegra_pwfm2_device;
extern struct platform_device tegra_pwfm3_device;
extern struct platform_device tegra_otg_device;
extern struct platform_device tegra_uarta_device;
extern struct platform_device tegra_uartb_device;
extern struct platform_device tegra_uartc_device;
extern struct platform_device tegra_uartd_device;
extern struct platform_device tegra_uarte_device;
extern struct platform_device tegra_avp_device;
extern struct platform_device nvavp_device;
extern struct platform_device tegra_aes_device;
#if defined(CONFIG_TEGRA_SKIN_THROTTLE)
extern struct platform_device tegra_skin_therm_est_device;
#endif
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
extern struct platform_device tegra_tsensor_device;
#endif
extern struct platform_device tegra_nor_device;
extern struct platform_device debug_uarta_device;
extern struct platform_device debug_uartb_device;
extern struct platform_device debug_uartc_device;
extern struct platform_device debug_uartd_device;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
extern struct platform_device tegra_se_device;
extern struct platform_device tegra11_se_device;
extern struct platform_device debug_uarte_device;
#endif

extern struct platform_device tegra_disp1_device;
extern struct platform_device tegra_disp2_device;
extern struct platform_device tegra_nvmap_device;
extern struct platform_device tegra_xhci_device;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
extern struct platform_device tegra_cec_device;
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
extern struct platform_device tegra_cl_dvfs_device;
#endif
extern struct platform_device tegra_fuse_device;

extern struct platform_device etb_device;
extern struct platform_device ptm_device;

void __init tegra_init_debug_uart_rate(void);

#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
extern void tegra_smmu_init(void);
#else
static inline void tegra_smmu_init(void)
{
}
#endif

#endif
