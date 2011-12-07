/* arch/arm/mach-msm/io.c
 *
 * MSM7K, QSD io support
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>

#include <mach/hardware.h>
#include <asm/page.h>
#include <mach/msm_iomap.h>
#include <asm/mach/map.h>

#include <mach/board.h>

#define MSM_CHIP_DEVICE(name, chip) { \
		.virtual = (unsigned long) MSM_##name##_BASE, \
		.pfn = __phys_to_pfn(chip##_##name##_PHYS), \
		.length = chip##_##name##_SIZE, \
		.type = MT_DEVICE_NONSHARED, \
	 }

#define MSM_DEVICE(name) MSM_CHIP_DEVICE(name, MSM)

/* msm_shared_ram_phys default value of 0x00100000 is the most common value
 * and should work as-is for any target without stacked memory.
 */
unsigned int msm_shared_ram_phys = 0x00100000;

static void msm_map_io(struct map_desc *io_desc, int size)
{
	int i;

	BUG_ON(!size);
	for (i = 0; i < size; i++)
		if (io_desc[i].virtual == (unsigned long)MSM_SHARED_RAM_BASE)
			io_desc[i].pfn = __phys_to_pfn(msm_shared_ram_phys);

	iotable_init(io_desc, size);
}

#if defined(CONFIG_ARCH_MSM7X01A) || defined(CONFIG_ARCH_MSM7X27) \
	|| defined(CONFIG_ARCH_MSM7X25)
static struct map_desc msm_io_desc[] __initdata = {
	MSM_DEVICE(VIC),
	MSM_DEVICE(CSR),
	MSM_DEVICE(TMR),
	MSM_DEVICE(GPIO1),
	MSM_DEVICE(GPIO2),
	MSM_DEVICE(CLK_CTL),
	MSM_DEVICE(AD5),
	MSM_DEVICE(MDC),
#if defined(CONFIG_DEBUG_MSM_UART1) || defined(CONFIG_DEBUG_MSM_UART2) || \
	defined(CONFIG_DEBUG_MSM_UART3)
	MSM_DEVICE(DEBUG_UART),
#endif
#ifdef CONFIG_CACHE_L2X0
	{
		.virtual =  (unsigned long) MSM_L2CC_BASE,
		.pfn =      __phys_to_pfn(MSM_L2CC_PHYS),
		.length =   MSM_L2CC_SIZE,
		.type =     MT_DEVICE,
	},
#endif
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
};

void __init msm_map_common_io(void)
{
	/*Peripheral port memory remap, nothing looks to be there for
	 * cortex a5.
	 */
#ifndef CONFIG_ARCH_MSM_CORTEX_A5
	/* Make sure the peripheral register window is closed, since
	 * we will use PTE flags (TEX[1]=1,B=0,C=1) to determine which
	 * pages are peripheral interface or not.
	 */
	asm("mcr p15, 0, %0, c15, c2, 4" : : "r" (0));
#endif
	msm_map_io(msm_io_desc, ARRAY_SIZE(msm_io_desc));
}
#endif

#ifdef CONFIG_ARCH_QSD8X50
static struct map_desc qsd8x50_io_desc[] __initdata = {
	MSM_DEVICE(VIC),
	MSM_DEVICE(CSR),
	MSM_DEVICE(TMR),
	MSM_DEVICE(GPIO1),
	MSM_DEVICE(GPIO2),
	MSM_DEVICE(CLK_CTL),
	MSM_DEVICE(SIRC),
	MSM_DEVICE(SCPLL),
	MSM_DEVICE(AD5),
	MSM_DEVICE(MDC),
	MSM_DEVICE(TCSR),
#if defined(CONFIG_DEBUG_MSM_UART1) || defined(CONFIG_DEBUG_MSM_UART2) || \
	defined(CONFIG_DEBUG_MSM_UART3)
	MSM_DEVICE(DEBUG_UART),
#endif
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
};

void __init msm_map_qsd8x50_io(void)
{
	msm_map_io(qsd8x50_io_desc, ARRAY_SIZE(qsd8x50_io_desc));
}
#endif /* CONFIG_ARCH_QSD8X50 */

#ifdef CONFIG_ARCH_MSM8X60
static struct map_desc msm8x60_io_desc[] __initdata = {
	MSM_DEVICE(QGIC_DIST),
	MSM_DEVICE(QGIC_CPU),
	MSM_DEVICE(TMR),
	MSM_DEVICE(TMR0),
	MSM_DEVICE(RPM_MPM),
	MSM_DEVICE(ACC),
	MSM_DEVICE(ACC0),
	MSM_DEVICE(ACC1),
	MSM_DEVICE(SAW0),
	MSM_DEVICE(SAW1),
	MSM_DEVICE(GCC),
	MSM_DEVICE(TLMM),
	MSM_DEVICE(SCPLL),
	MSM_DEVICE(RPM),
	MSM_DEVICE(CLK_CTL),
	MSM_DEVICE(MMSS_CLK_CTL),
	MSM_DEVICE(LPASS_CLK_CTL),
	MSM_DEVICE(TCSR),
	MSM_DEVICE(IMEM),
	MSM_DEVICE(HDMI),
#ifdef CONFIG_DEBUG_MSM8660_UART
	MSM_DEVICE(DEBUG_UART),
#endif
	MSM_DEVICE(SIC_NON_SECURE),
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
	MSM_DEVICE(QFPROM),
};

void __init msm_map_msm8x60_io(void)
{
	msm_map_io(msm8x60_io_desc, ARRAY_SIZE(msm8x60_io_desc));
}
#endif /* CONFIG_ARCH_MSM8X60 */

#ifdef CONFIG_ARCH_MSM8960
static struct map_desc msm8960_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(QGIC_DIST, MSM8960),
	MSM_CHIP_DEVICE(QGIC_CPU, MSM8960),
	MSM_CHIP_DEVICE(ACC0, MSM8960),
	MSM_CHIP_DEVICE(ACC1, MSM8960),
	MSM_CHIP_DEVICE(TMR, MSM8960),
	MSM_CHIP_DEVICE(TMR0, MSM8960),
	MSM_CHIP_DEVICE(RPM_MPM, MSM8960),
	MSM_CHIP_DEVICE(CLK_CTL, MSM8960),
	MSM_CHIP_DEVICE(MMSS_CLK_CTL, MSM8960),
	MSM_CHIP_DEVICE(LPASS_CLK_CTL, MSM8960),
	MSM_CHIP_DEVICE(RPM, MSM8960),
	MSM_CHIP_DEVICE(TLMM, MSM8960),
	MSM_CHIP_DEVICE(HFPLL, MSM8960),
	MSM_CHIP_DEVICE(SAW0, MSM8960),
	MSM_CHIP_DEVICE(SAW1, MSM8960),
	MSM_CHIP_DEVICE(SAW_L2, MSM8960),
	MSM_CHIP_DEVICE(SIC_NON_SECURE, MSM8960),
	MSM_CHIP_DEVICE(APCS_GCC, MSM8960),
	MSM_CHIP_DEVICE(IMEM, MSM8960),
	MSM_CHIP_DEVICE(HDMI, MSM8960),
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
#ifdef CONFIG_DEBUG_MSM8960_UART
	MSM_DEVICE(DEBUG_UART),
#endif
	MSM_CHIP_DEVICE(QFPROM, MSM8960),
};

void __init msm_map_msm8960_io(void)
{
	msm_map_io(msm8960_io_desc, ARRAY_SIZE(msm8960_io_desc));
}
#endif /* CONFIG_ARCH_MSM8960 */

#ifdef CONFIG_ARCH_MSM8930
static struct map_desc msm8930_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(QGIC_DIST, MSM8930),
	MSM_CHIP_DEVICE(QGIC_CPU, MSM8930),
	MSM_CHIP_DEVICE(ACC0, MSM8930),
	MSM_CHIP_DEVICE(ACC1, MSM8930),
	MSM_CHIP_DEVICE(TMR, MSM8930),
	MSM_CHIP_DEVICE(TMR0, MSM8930),
	MSM_CHIP_DEVICE(RPM_MPM, MSM8930),
	MSM_CHIP_DEVICE(CLK_CTL, MSM8930),
	MSM_CHIP_DEVICE(MMSS_CLK_CTL, MSM8930),
	MSM_CHIP_DEVICE(LPASS_CLK_CTL, MSM8930),
	MSM_CHIP_DEVICE(RPM, MSM8930),
	MSM_CHIP_DEVICE(TLMM, MSM8930),
	MSM_CHIP_DEVICE(HFPLL, MSM8930),
	MSM_CHIP_DEVICE(SAW0, MSM8930),
	MSM_CHIP_DEVICE(SAW1, MSM8930),
	MSM_CHIP_DEVICE(SAW_L2, MSM8930),
	MSM_CHIP_DEVICE(SIC_NON_SECURE, MSM8930),
	MSM_CHIP_DEVICE(APCS_GCC, MSM8930),
	MSM_CHIP_DEVICE(IMEM, MSM8930),
	MSM_CHIP_DEVICE(HDMI, MSM8930),
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
#ifdef CONFIG_DEBUG_MSM8930_UART
	MSM_DEVICE(DEBUG_UART),
#endif
	MSM_CHIP_DEVICE(QFPROM, MSM8930),
};

void __init msm_map_msm8930_io(void)
{
	msm_map_io(msm8930_io_desc, ARRAY_SIZE(msm8930_io_desc));
}
#endif /* CONFIG_ARCH_MSM8930 */

#ifdef CONFIG_ARCH_APQ8064
static struct map_desc apq8064_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(QGIC_DIST, APQ8064),
	MSM_CHIP_DEVICE(QGIC_CPU, APQ8064),
	MSM_CHIP_DEVICE(TMR, APQ8064),
	MSM_CHIP_DEVICE(TMR0, APQ8064),
	MSM_CHIP_DEVICE(TLMM, APQ8064),
	MSM_CHIP_DEVICE(ACC0, APQ8064),
	MSM_CHIP_DEVICE(ACC1, APQ8064),
	MSM_CHIP_DEVICE(ACC2, APQ8064),
	MSM_CHIP_DEVICE(ACC3, APQ8064),
	MSM_CHIP_DEVICE(HFPLL, APQ8064),
	MSM_CHIP_DEVICE(CLK_CTL, APQ8064),
	MSM_CHIP_DEVICE(MMSS_CLK_CTL, APQ8064),
	MSM_CHIP_DEVICE(LPASS_CLK_CTL, APQ8064),
	MSM_CHIP_DEVICE(APCS_GCC, APQ8064),
	MSM_CHIP_DEVICE(IMEM, APQ8064),
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
};

void __init msm_map_apq8064_io(void)
{
	msm_map_io(apq8064_io_desc, ARRAY_SIZE(apq8064_io_desc));
}
#endif /* CONFIG_ARCH_APQ8064 */

#ifdef CONFIG_ARCH_MSMCOPPER
static struct map_desc msm_copper_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(QGIC_DIST, COPPER),
	MSM_CHIP_DEVICE(QGIC_CPU, COPPER),
	MSM_CHIP_DEVICE(TLMM, COPPER),
#ifdef CONFIG_DEBUG_MSMCOPPER_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_copper_io(void)
{
	msm_map_io(msm_copper_io_desc, ARRAY_SIZE(msm_copper_io_desc));
}
#endif /* CONFIG_ARCH_MSMCOPPER */

#ifdef CONFIG_ARCH_MSM7X30
static struct map_desc msm7x30_io_desc[] __initdata = {
	MSM_DEVICE(VIC),
	MSM_DEVICE(CSR),
	MSM_DEVICE(TMR),
	MSM_DEVICE(GPIO1),
	MSM_DEVICE(GPIO2),
	MSM_DEVICE(CLK_CTL),
	MSM_DEVICE(CLK_CTL_SH2),
	MSM_DEVICE(AD5),
	MSM_DEVICE(MDC),
	MSM_DEVICE(ACC),
	MSM_DEVICE(SAW),
	MSM_DEVICE(GCC),
	MSM_DEVICE(TCSR),
#if defined(CONFIG_DEBUG_MSM_UART1) || defined(CONFIG_DEBUG_MSM_UART2) || \
	defined(CONFIG_DEBUG_MSM_UART3)
	MSM_DEVICE(DEBUG_UART),
#endif
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
};

void __init msm_map_msm7x30_io(void)
{
	msm_map_io(msm7x30_io_desc, ARRAY_SIZE(msm7x30_io_desc));
}
#endif /* CONFIG_ARCH_MSM7X30 */

#ifdef CONFIG_ARCH_FSM9XXX
static struct map_desc fsm9xxx_io_desc[] __initdata = {
	MSM_DEVICE(VIC),
	MSM_DEVICE(SIRC),
	MSM_DEVICE(CSR),
	MSM_DEVICE(TLMM),
	MSM_DEVICE(TCSR),
	MSM_DEVICE(CLK_CTL),
	MSM_DEVICE(ACC),
	MSM_DEVICE(SAW),
	MSM_DEVICE(GCC),
	MSM_DEVICE(GRFC),
	MSM_DEVICE(QFP_FUSE),
	MSM_DEVICE(HH),
#if defined(CONFIG_DEBUG_MSM_UART1) || defined(CONFIG_DEBUG_MSM_UART2) || \
	defined(CONFIG_DEBUG_MSM_UART3)
	MSM_DEVICE(DEBUG_UART),
#endif
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
};

void __init msm_map_fsm9xxx_io(void)
{
	msm_map_io(fsm9xxx_io_desc, ARRAY_SIZE(fsm9xxx_io_desc));
}
#endif /* CONFIG_ARCH_FSM9XXX */

#ifdef CONFIG_ARCH_MSM9615
static struct map_desc msm9615_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(QGIC_DIST, MSM9615),
	MSM_CHIP_DEVICE(QGIC_CPU, MSM9615),
	MSM_CHIP_DEVICE(ACC0, MSM9615),
	MSM_CHIP_DEVICE(TMR, MSM9615),
	MSM_CHIP_DEVICE(TLMM, MSM9615),
	MSM_CHIP_DEVICE(SAW0, MSM9615),
	MSM_CHIP_DEVICE(APCS_GCC, MSM9615),
	MSM_CHIP_DEVICE(TCSR, MSM9615),
	MSM_CHIP_DEVICE(L2CC, MSM9615),
	MSM_CHIP_DEVICE(CLK_CTL, MSM9615),
	MSM_CHIP_DEVICE(LPASS_CLK_CTL, MSM9615),
	MSM_CHIP_DEVICE(RPM, MSM9615),
	MSM_CHIP_DEVICE(RPM_MPM, MSM9615),
	MSM_CHIP_DEVICE(APCS_GLB, MSM9615),
	MSM_CHIP_DEVICE(IMEM, MSM9615),
	{
		.virtual =  (unsigned long) MSM_SHARED_RAM_BASE,
		.length =   MSM_SHARED_RAM_SIZE,
		.type =     MT_DEVICE,
	},
	MSM_CHIP_DEVICE(QFPROM, MSM9615),
};

void __init msm_map_msm9615_io(void)
{
	msm_map_io(msm9615_io_desc, ARRAY_SIZE(msm9615_io_desc));
}
#endif /* CONFIG_ARCH_MSM9615 */

void __iomem *
__msm_ioremap(unsigned long phys_addr, size_t size, unsigned int mtype)
{
	if (mtype == MT_DEVICE) {
		/* The peripherals in the 88000000 - F0000000 range
		 * are only accessable by type MT_DEVICE_NONSHARED.
		 * Adjust mtype as necessary to make this "just work."
		 */
		if ((phys_addr >= 0x88000000) && (phys_addr < 0xF0000000))
			mtype = MT_DEVICE_NONSHARED;
	}

	return __arm_ioremap(phys_addr, size, mtype);
}
EXPORT_SYMBOL(__msm_ioremap);
