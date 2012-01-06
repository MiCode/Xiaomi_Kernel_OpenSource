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
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/mfd/wcd9310/core.h>
#include <linux/mfd/wcd9310/pdata.h>
#include <linux/msm_ssbi.h>
#include <linux/spi/spi.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach/mmc.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/usb/android.h>
#include <mach/socinfo.h>
#include <mach/msm_spi.h>
#include "timer.h"
#include "devices.h"
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <linux/android_pmem.h>
#include <mach/msm_memtypes.h>
#include <linux/bootmem.h>
#include <asm/setup.h>
#include <mach/dma.h>

#include "msm_watchdog.h"
#include "board-8064.h"

#define MSM_PMEM_KERNEL_EBI1_SIZE  0x600000
#define MSM_PMEM_ADSP_SIZE         0x3800000
#define MSM_PMEM_AUDIO_SIZE        0x2B4000
#define MSM_PMEM_SIZE 0x1800000 /* 24 Mbytes */

static struct memtype_reserve apq8064_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int apq8064_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static unsigned pmem_size = MSM_PMEM_SIZE;
static int __init pmem_size_setup(char *p)
{
	pmem_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_size", pmem_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;

static int __init pmem_adsp_size_setup(char *p)
{
	pmem_adsp_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_adsp_size", pmem_adsp_size_setup);

static unsigned pmem_audio_size = MSM_PMEM_AUDIO_SIZE;

static int __init pmem_audio_size_setup(char *p)
{
	pmem_audio_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_audio_size", pmem_audio_size_setup);

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_ALLORNOTHING,
	.cached = 1,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = {.platform_data = &android_pmem_pdata},
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static unsigned pmem_kernel_ebi1_size = MSM_PMEM_KERNEL_EBI1_SIZE;
static int __init pmem_kernel_ebi1_size_setup(char *p)
{
	pmem_kernel_ebi1_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_kernel_ebi1_size", pmem_kernel_ebi1_size_setup);

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};

static void __init size_pmem_devices(void)
{
	android_pmem_adsp_pdata.size = pmem_adsp_size;
	android_pmem_pdata.size = pmem_size;
	android_pmem_audio_pdata.size = MSM_PMEM_AUDIO_SIZE;
}

static void __init reserve_memory_for(struct android_pmem_platform_data *p)
{
	apq8064_reserve_table[p->memory_type].size += p->size;
}

static void __init reserve_pmem_memory(void)
{
	reserve_memory_for(&android_pmem_adsp_pdata);
	reserve_memory_for(&android_pmem_pdata);
	reserve_memory_for(&android_pmem_audio_pdata);
	apq8064_reserve_table[MEMTYPE_EBI1].size += pmem_kernel_ebi1_size;
}

static void __init apq8064_calculate_reserve_sizes(void)
{
	size_pmem_devices();
	reserve_pmem_memory();
}

static struct reserve_info apq8064_reserve_info __initdata = {
	.memtype_reserve_table = apq8064_reserve_table,
	.calculate_reserve_sizes = apq8064_calculate_reserve_sizes,
	.paddr_to_memtype = apq8064_paddr_to_memtype,
};

static int apq8064_memory_bank_size(void)
{
	return 1<<29;
}

static void __init locate_unstable_memory(void)
{
	struct membank *mb = &meminfo.bank[meminfo.nr_banks - 1];
	unsigned long bank_size;
	unsigned long low, high;

	bank_size = apq8064_memory_bank_size();
	low = meminfo.bank[0].start;
	high = mb->start + mb->size;
	low &= ~(bank_size - 1);

	if (high - low <= bank_size)
		return;
	apq8064_reserve_info.low_unstable_address = low + bank_size;
	apq8064_reserve_info.max_unstable_size = high - low - bank_size;
	apq8064_reserve_info.bank_size = bank_size;
	pr_info("low unstable address %lx max size %lx bank size %lx\n",
		apq8064_reserve_info.low_unstable_address,
		apq8064_reserve_info.max_unstable_size,
		apq8064_reserve_info.bank_size);
}

static void __init apq8064_reserve(void)
{
	reserve_info = &apq8064_reserve_info;
	locate_unstable_memory();
	msm_reserve();
}

static struct platform_device android_usb_device = {
	.name = "android_usb",
	.id = -1,
};

static struct msm_otg_platform_data msm_otg_pdata = {
	.mode			= USB_PERIPHERAL,
	.otg_control		= OTG_PHY_CONTROL,
	.phy_type		= SNPS_28NM_INTEGRATED_PHY,
	.pclk_src_name		= "dfab_usb_hs_clk",
};

#define TABLA_INTERRUPT_BASE (NR_MSM_IRQS + NR_GPIO_IRQS + NR_PM8921_IRQS)

/* Micbias setting is based on 8660 CDP/MTP/FLUID requirement
 * 4 micbiases are used to power various analog and digital
 * microphones operating at 1800 mV. Technically, all micbiases
 * can source from single cfilter since all microphones operate
 * at the same voltage level. The arrangement below is to make
 * sure all cfilters are exercised. LDO_H regulator ouput level
 * does not need to be as high as 2.85V. It is choosen for
 * microphone sensitivity purpose.
 */
static struct tabla_pdata apq8064_tabla_platform_data = {
	.slimbus_slave_device = {
		.name = "tabla-slave",
		.e_addr = {0, 0, 0x10, 0, 0x17, 2},
	},
	.irq = MSM_GPIO_TO_INT(42),
	.irq_base = TABLA_INTERRUPT_BASE,
	.num_irqs = NR_TABLA_IRQS,
	.reset_gpio = PM8921_GPIO_PM_TO_SYS(34),
	.micbias = {
		.ldoh_v = TABLA_LDOH_2P85_V,
		.cfilt1_mv = 1800,
		.cfilt2_mv = 1800,
		.cfilt3_mv = 1800,
		.bias1_cfilt_sel = TABLA_CFILT1_SEL,
		.bias2_cfilt_sel = TABLA_CFILT2_SEL,
		.bias3_cfilt_sel = TABLA_CFILT3_SEL,
		.bias4_cfilt_sel = TABLA_CFILT3_SEL,
	}
};

static struct slim_device apq8064_slim_tabla = {
	.name = "tabla-slim",
	.e_addr = {0, 1, 0x10, 0, 0x17, 2},
	.dev = {
		.platform_data = &apq8064_tabla_platform_data,
	},
};

static struct tabla_pdata apq8064_tabla20_platform_data = {
	.slimbus_slave_device = {
		.name = "tabla-slave",
		.e_addr = {0, 0, 0x60, 0, 0x17, 2},
	},
	.irq = MSM_GPIO_TO_INT(42),
	.irq_base = TABLA_INTERRUPT_BASE,
	.num_irqs = NR_TABLA_IRQS,
	.reset_gpio = PM8921_GPIO_PM_TO_SYS(34),
	.micbias = {
		.ldoh_v = TABLA_LDOH_2P85_V,
		.cfilt1_mv = 1800,
		.cfilt2_mv = 1800,
		.cfilt3_mv = 1800,
		.bias1_cfilt_sel = TABLA_CFILT1_SEL,
		.bias2_cfilt_sel = TABLA_CFILT2_SEL,
		.bias3_cfilt_sel = TABLA_CFILT3_SEL,
		.bias4_cfilt_sel = TABLA_CFILT3_SEL,
	}
};

static struct slim_device apq8064_slim_tabla20 = {
	.name = "tabla2x-slim",
	.e_addr = {0, 1, 0x60, 0, 0x17, 2},
	.dev = {
		.platform_data = &apq8064_tabla20_platform_data,
	},
};

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

#define QCE_SIZE		0x10000
#define QCE_0_BASE		0x11000000

#define QCE_HW_KEY_SUPPORT	0
#define QCE_SHA_HMAC_SUPPORT	1
#define QCE_SHARE_CE_RESOURCE	3
#define QCE_CE_SHARED		0

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV8064_CE_IN_CHAN,
		.end = DMOV8064_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV8064_CE_IN_CRCI,
		.end = DMOV8064_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV8064_CE_OUT_CRCI,
		.end = DMOV8064_CE_OUT_CRCI,
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
		.start = DMOV8064_CE_IN_CHAN,
		.end = DMOV8064_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV8064_CE_IN_CRCI,
		.end = DMOV8064_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV8064_CE_OUT_CRCI,
		.end = DMOV8064_CE_OUT_CRCI,
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
	.bus_scale_table = NULL,
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
	.bus_scale_table = NULL,
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


#define MSM_SHARED_RAM_PHYS 0x80000000
static void __init apq8064_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_apq8064_io();
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
}

static void __init apq8064_init_irq(void)
{
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
						(void *)MSM_QGIC_CPU_BASE);

	/* Edge trigger PPIs except AVS_SVICINT and AVS_SVICINTSWDONE */
	writel_relaxed(0xFFFFD7FF, MSM_QGIC_DIST_BASE + GIC_DIST_CONFIG + 4);

	writel_relaxed(0x0000FFFF, MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_SET);
	mb();
}

static struct platform_device msm8064_device_saw_regulator_core0 = {
	.name	= "saw-regulator",
	.id	= 0,
	.dev	= {
		.platform_data = &msm8064_saw_regulator_pdata_8921_s5,
	},
};

static struct platform_device msm8064_device_saw_regulator_core1 = {
	.name	= "saw-regulator",
	.id	= 1,
	.dev	= {
		.platform_data = &msm8064_saw_regulator_pdata_8921_s6,
	},
};

static struct platform_device msm8064_device_saw_regulator_core2 = {
	.name	= "saw-regulator",
	.id	= 2,
	.dev	= {
		.platform_data = &msm8064_saw_regulator_pdata_8821_s0,
	},
};

static struct platform_device msm8064_device_saw_regulator_core3 = {
	.name	= "saw-regulator",
	.id	= 3,
	.dev	= {
		.platform_data = &msm8064_saw_regulator_pdata_8821_s1,
	},
};

static struct platform_device *common_devices[] __initdata = {
	&apq8064_device_dmov,
	&apq8064_device_qup_i2c_gsbi4,
	&apq8064_device_qup_spi_gsbi5,
	&apq8064_slim_ctrl,
	&apq8064_device_ssbi_pmic1,
	&apq8064_device_ssbi_pmic2,
	&msm_device_smd_apq8064,
	&apq8064_device_otg,
	&apq8064_device_gadget_peripheral,
	&android_usb_device,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_audio_device,
	&msm8064_device_watchdog,
	&msm8064_device_saw_regulator_core0,
	&msm8064_device_saw_regulator_core1,
	&msm8064_device_saw_regulator_core2,
	&msm8064_device_saw_regulator_core3,
#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
	&qcrypto_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
	&qcedev_device,
#endif

#ifdef CONFIG_HW_RANDOM_MSM
	&apq8064_device_rng,
#endif
	&apq_pcm,
	&apq_pcm_routing,
	&apq_cpudai0,
	&apq_cpudai1,
	&apq_cpudai_hdmi_rx,
	&apq_cpudai_bt_rx,
	&apq_cpudai_bt_tx,
	&apq_cpudai_fm_rx,
	&apq_cpudai_fm_tx,
	&apq_cpu_fe,
	&apq_stub_codec,
	&apq_voice,
	&apq_voip,
	&apq_lpa_pcm,
	&apq_pcm_hostless,
	&apq_cpudai_afe_01_rx,
	&apq_cpudai_afe_01_tx,
	&apq_cpudai_afe_02_rx,
	&apq_cpudai_afe_02_tx,
	&apq_pcm_afe,
	&apq_cpudai_auxpcm_rx,
	&apq_cpudai_auxpcm_tx,
};

static struct platform_device *sim_devices[] __initdata = {
	&apq8064_device_uart_gsbi3,
	&msm_device_sps_apq8064,
};

static struct platform_device *rumi3_devices[] __initdata = {
	&apq8064_device_uart_gsbi1,
	&msm_device_sps_apq8064,
};

static struct msm_spi_platform_data apq8064_qup_spi_gsbi5_pdata = {
	.max_clock_speed = 24000000,
};

#define KS8851_IRQ_GPIO		43

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias               = "ks8851",
		.irq                    = MSM_GPIO_TO_INT(KS8851_IRQ_GPIO),
		.max_speed_hz           = 19200000,
		.bus_num                = 0,
		.chip_select            = 2,
		.mode                   = SPI_MODE_0,
	},
};

static struct slim_boardinfo apq8064_slim_devices[] = {
	{
		.bus_num = 1,
		.slim_slave = &apq8064_slim_tabla,
	},
	{
		.bus_num = 1,
		.slim_slave = &apq8064_slim_tabla20,
	},
	/* add more slimbus slaves as needed */
};

static struct msm_i2c_platform_data apq8064_i2c_qup_gsbi4_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

static void __init apq8064_i2c_init(void)
{
	apq8064_device_qup_i2c_gsbi4.dev.platform_data =
					&apq8064_i2c_qup_gsbi4_pdata;
}

#ifdef CONFIG_KS8851
static int ethernet_init(void)
{
	int ret;
	ret = gpio_request(KS8851_IRQ_GPIO, "ks8851_irq");
	if (ret) {
		pr_err("ks8851 gpio_request failed: %d\n", ret);
		goto fail;
	}

	return 0;
fail:
	return ret;
}
#else
static int ethernet_init(void)
{
	return 0;
}
#endif

static void __init apq8064_clock_init(void)
{
	if (machine_is_apq8064_sim())
		msm_clock_init(&apq8064_clock_init_data);
	else
		msm_clock_init(&apq8064_dummy_clock_init_data);
}

static void __init apq8064_common_init(void)
{
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
	apq8064_clock_init();
	apq8064_init_gpiomux();
	apq8064_i2c_init();

	apq8064_device_qup_spi_gsbi5.dev.platform_data =
						&apq8064_qup_spi_gsbi5_pdata;
	apq8064_init_pmic();
	apq8064_device_otg.dev.platform_data = &msm_otg_pdata;
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
	apq8064_init_mmc();
	slim_register_board_info(apq8064_slim_devices,
		ARRAY_SIZE(apq8064_slim_devices));
}

static void __init apq8064_sim_init(void)
{
	struct msm_watchdog_pdata *wdog_pdata = (struct msm_watchdog_pdata *)
		&msm8064_device_watchdog.dev.platform_data;

	wdog_pdata->bark_time = 15000;
	apq8064_common_init();
	platform_add_devices(sim_devices, ARRAY_SIZE(sim_devices));
}

static void __init apq8064_rumi3_init(void)
{
	apq8064_common_init();
	ethernet_init();
	platform_add_devices(rumi3_devices, ARRAY_SIZE(rumi3_devices));
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
}

MACHINE_START(APQ8064_SIM, "QCT APQ8064 SIMULATOR")
	.map_io = apq8064_map_io,
	.reserve = apq8064_reserve,
	.init_irq = apq8064_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = apq8064_sim_init,
MACHINE_END

MACHINE_START(APQ8064_RUMI3, "QCT APQ8064 RUMI3")
	.map_io = apq8064_map_io,
	.reserve = apq8064_reserve,
	.init_irq = apq8064_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = apq8064_rumi3_init,
MACHINE_END

