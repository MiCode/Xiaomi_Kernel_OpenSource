/*
 * arch/arm/mach-tegra/include/mach/io.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011-2012 NVIDIA Corporation.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Erik Gilling <konkers@google.com>
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

#ifndef __MACH_TEGRA_IO_H
#define __MACH_TEGRA_IO_H

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define IO_SPACE_LIMIT 0xffff
#else
#define IO_SPACE_LIMIT 0xffffffff
#endif

/* On TEGRA, many peripherals are very closely packed in
 * two 256 MB io windows (that actually only use about 64 KB
 * at the start of each).
 *
 * We will just map the first 1 MB of each window (to minimize
 * pt entries needed) and provide a macro to transform physical
 * io addresses to an appropriate void __iomem *.
 *
 * Always map simulation specific devices to lowest address.
 *
 * The base address of each aperture must be aligned to a PMD
 * (2 MB boundary).
 *
 */

#ifdef __ASSEMBLY__
#define IOMEM(x)	(x)
#else
#define IOMEM(x)	((void __force __iomem *)(x))
#endif

#ifdef CONFIG_ARM_LPAE
#define ROUND_UP(x, n)		(((x) + (n) - 1) & ~((n) - 1))
#define IO_VIRT_ROUND_UP(x)	ROUND_UP(x, SZ_2M)
#else
#define IO_VIRT_ROUND_UP(x)	(x)
#endif

/* Define physical aperture limits */

#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
#define IO_SMC_PHYS		0x77000000
#define IO_SMC_SIZE		SZ_1M

#define IO_SIM_ESCAPE_PHYS	0x538f0000
#define IO_SIM_ESCAPE_SIZE	SZ_4K
#endif

#define IO_IRAM_PHYS		0x40000000
#define IO_IRAM_SIZE		SZ_256K

#define IO_CPU_PHYS		0x50000000
#define IO_CPU_SIZE		SZ_1M

#define IO_PPSB_PHYS		0x60000000
#define IO_PPSB_SIZE		SZ_1M

#define IO_APB_PHYS		0x70000000
#define IO_APB_SIZE		SZ_2M

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define IO_USB_PHYS		0xC5000000
#else
#define IO_USB_PHYS		0x7D000000
#endif
#define IO_USB_SIZE		SZ_1M

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define IO_SDMMC_PHYS		0xC8000000
#else
#define IO_SDMMC_PHYS		0x78000000
#endif
#define IO_SDMMC_SIZE		SZ_1M

#define IO_HOST1X_PHYS		0x54000000
#define IO_HOST1X_SIZE		SZ_8M

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define IO_PPCS_PHYS		0xC4000000
#else
#define IO_PPCS_PHYS		0x7C000000
#endif
#define IO_PPCS_SIZE		SZ_1M

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define IO_PCIE_PHYS	0x80000000
#else
#define IO_PCIE_PHYS	0x00000000
#endif
#define IO_PCIE_SIZE	(SZ_16M * 3)

#if defined(CONFIG_MTD_NOR_TEGRA) || defined(CONFIG_MTD_NOR_M2601)
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define IO_NOR_PHYS	0xD0000000
#define IO_NOR_SIZE	(SZ_64M)
#else
#define IO_NOR_PHYS	0x48000000
#define IO_NOR_SIZE	(SZ_128M)
#endif
#else
#define IO_NOR_PHYS	0x0
#define IO_NOR_SIZE	0
#endif



/* Virtual aperture limits are packed into the I/O space from the higest
   address to lowest with each aperture base address adjusted as necessary
   for proper section mapping boundary (2 MB) rounding. */

#define IO_LAST_ADDR		0xFF000000
#define IO_HOST1X_VIRT		(IO_LAST_ADDR - IO_VIRT_ROUND_UP(IO_HOST1X_SIZE))
#define IO_SDMMC_VIRT		(IO_HOST1X_VIRT - IO_VIRT_ROUND_UP(IO_SDMMC_SIZE))
#define IO_USB_VIRT		(IO_SDMMC_VIRT - IO_VIRT_ROUND_UP(IO_USB_SIZE))
#define IO_APB_VIRT		(IO_USB_VIRT - IO_VIRT_ROUND_UP(IO_APB_SIZE))
#define IO_PPSB_VIRT		(IO_APB_VIRT - IO_VIRT_ROUND_UP(IO_PPSB_SIZE))
#define IO_CPU_VIRT		(IO_PPSB_VIRT - IO_VIRT_ROUND_UP(IO_CPU_SIZE))
#define IO_IRAM_VIRT		(IO_CPU_VIRT - IO_VIRT_ROUND_UP(IO_IRAM_SIZE))
#define IO_PPCS_VIRT		(IO_IRAM_VIRT - IO_VIRT_ROUND_UP(IO_PPCS_SIZE))
#define IO_PCIE_VIRT		(IO_PPCS_VIRT - IO_VIRT_ROUND_UP(IO_PCIE_SIZE))
#define IO_NOR_VIRT		(IO_PCIE_VIRT - IO_VIRT_ROUND_UP(IO_NOR_SIZE))
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
#define IO_SIM_ESCAPE_VIRT	(IO_NOR_VIRT - IO_VIRT_ROUND_UP(IO_SIM_ESCAPE_SIZE))
#define IO_SMC_VIRT		(IO_SIM_ESCAPE_VIRT - IO_VIRT_ROUND_UP(IO_SMC_SIZE))
#endif

#define IO_TO_VIRT_BETWEEN(p, st, sz)	((p) >= (st) && (p) < ((st) + (sz)))
#define IO_TO_VIRT_XLATE(p, pst, vst)	(((p) - (pst) + (vst)))

#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
#define IO_TO_VIRT_SMC(n) \
	IO_TO_VIRT_BETWEEN((n), IO_SMC_PHYS, IO_SMC_SIZE) ?             \
		IO_TO_VIRT_XLATE((n), IO_SMC_PHYS, IO_SMC_VIRT) :
#define IO_TO_VIRT_SIM_ESCAPE(n) \
	IO_TO_VIRT_BETWEEN((n), IO_SIM_ESCAPE_PHYS, IO_SIM_ESCAPE_SIZE) ? \
		IO_TO_VIRT_XLATE((n), IO_SIM_ESCAPE_PHYS, IO_SIM_ESCAPE_VIRT) :
#else
#define IO_TO_VIRT_SMC(n)
#define IO_TO_VIRT_SIM_ESCAPE(n)
#endif

#define IO_TO_VIRT(n) ( \
	IO_TO_VIRT_BETWEEN((n), IO_PPSB_PHYS, IO_PPSB_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_PPSB_PHYS, IO_PPSB_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_APB_PHYS, IO_APB_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_APB_PHYS, IO_APB_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_CPU_PHYS, IO_CPU_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_CPU_PHYS, IO_CPU_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_IRAM_PHYS, IO_IRAM_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_IRAM_PHYS, IO_IRAM_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_HOST1X_PHYS, IO_HOST1X_SIZE) ?	\
		IO_TO_VIRT_XLATE((n), IO_HOST1X_PHYS, IO_HOST1X_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_USB_PHYS, IO_USB_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_USB_PHYS, IO_USB_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_SDMMC_PHYS, IO_SDMMC_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_SDMMC_PHYS, IO_SDMMC_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_PPCS_PHYS, IO_PPCS_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_PPCS_PHYS, IO_PPCS_VIRT) :	\
	IO_TO_VIRT_BETWEEN((n), IO_PCIE_PHYS, IO_PCIE_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_PCIE_PHYS, IO_PCIE_VIRT) :	\
	IO_TO_VIRT_SMC((n))        \
	IO_TO_VIRT_SIM_ESCAPE((n)) \
	IO_TO_VIRT_BETWEEN((n), IO_NOR_PHYS, IO_NOR_SIZE) ?		\
		IO_TO_VIRT_XLATE((n), IO_NOR_PHYS, IO_NOR_VIRT) :	\
	0)

#ifndef __ASSEMBLER__

#define IO_ADDRESS(n) (IOMEM(IO_TO_VIRT(n)))

#if defined(CONFIG_TEGRA_PCI)
extern void __iomem *tegra_pcie_io_base;

static inline void __iomem *__io(unsigned long addr)
{
	return tegra_pcie_io_base + (addr & IO_SPACE_LIMIT);
}
#else
static inline void __iomem *__io(unsigned long addr)
{
	return (void __iomem *)addr;
}
#endif

#define __io(a)         __io(a)
#define __mem_pci(a)    (a)

#endif

#endif
