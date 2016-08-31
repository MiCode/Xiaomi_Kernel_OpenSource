/*
 * arch/arm/mach-tegra/common.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010-2014 NVIDIA Corporation. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
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

#include <linux/platform_device.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/clk/tegra.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/pstore_ram.h>
#include <linux/dma-mapping.h>
#include <linux/sys_soc.h>
#if defined(CONFIG_SMSC911X)
#include <linux/smsc911x.h>
#endif
#include <linux/pm.h>
#include <linux/tegra-powergate.h>

#include <linux/export.h>
#include <linux/bootmem.h>
#include <linux/tegra-soc.h>
#include <linux/dma-contiguous.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra_sm.h>

#ifdef CONFIG_ARM64
#include <linux/irqchip/gic.h>
#else
#include <asm/system.h>
#include <asm/hardware/cache-l2x0.h>
#endif
#include <asm/dma-mapping.h>

#include <mach/tegra_smmu.h>
#include <mach/nct.h>
#include <mach/dc.h>

#include "apbio.h"
#include "board.h"
#include "clock.h"
#include "common.h"
#include "dvfs.h"
#include "iomap.h"
#include "pm.h"
#include "sleep.h"
#include "reset.h"
#include "devices.h"
#include "pmc.h"

#define MC_SECURITY_CFG2	0x7c

#define AHB_ARBITRATION_PRIORITY_CTRL		0x4
#define   AHB_PRIORITY_WEIGHT(x)	(((x) & 0x7) << 29)
#define   PRIORITY_SELECT_USB	BIT(6)
#define   PRIORITY_SELECT_USB2	BIT(18)
#define   PRIORITY_SELECT_USB3	BIT(17)
#define   PRIORITY_SELECT_SE BIT(14)

#define AHB_GIZMO_AHB_MEM		0xc
#define   ENB_FAST_REARBITRATE	BIT(2)
#define   DONT_SPLIT_AHB_WR     BIT(7)
#define   WR_WAIT_COMMIT_ON_1K	BIT(8)
#define   EN_USB_WAIT_COMMIT_ON_1K_STALL	BIT(9)

#define   RECOVERY_MODE	BIT(31)
#define   BOOTLOADER_MODE	BIT(30)
#define   FORCED_RECOVERY_MODE	BIT(1)

#define AHB_GIZMO_USB		0x1c
#define AHB_GIZMO_USB2		0x78
#define AHB_GIZMO_USB3		0x7c
#define AHB_GIZMO_SE		0x4c
#define   IMMEDIATE	BIT(18)

#define AHB_MEM_PREFETCH_CFG5	0xc8
#define AHB_MEM_PREFETCH_CFG3	0xe0
#define AHB_MEM_PREFETCH_CFG4	0xe4
#define AHB_MEM_PREFETCH_CFG1	0xec
#define AHB_MEM_PREFETCH_CFG2	0xf0
#define AHB_MEM_PREFETCH_CFG6	0xcc
#define   PREFETCH_ENB	BIT(31)
#define   MST_ID(x)	(((x) & 0x1f) << 26)
#define   AHBDMA_MST_ID	MST_ID(5)
#define   USB_MST_ID	MST_ID(6)
#define SDMMC4_MST_ID	MST_ID(12)
#define   USB2_MST_ID	MST_ID(18)
#define   USB3_MST_ID	MST_ID(17)
#define   SE_MST_ID	MST_ID(14)
#define   ADDR_BNDRY(x)	(((x) & 0xf) << 21)
#define   INACTIVITY_TIMEOUT(x)	(((x) & 0xffff) << 0)

phys_addr_t tegra_bootloader_fb_start;
phys_addr_t tegra_bootloader_fb_size;
phys_addr_t tegra_bootloader_fb2_start;
phys_addr_t tegra_bootloader_fb2_size;
phys_addr_t tegra_fb_start;
phys_addr_t tegra_fb_size;
phys_addr_t tegra_fb2_start;
phys_addr_t tegra_fb2_size;
phys_addr_t tegra_carveout_start;
phys_addr_t tegra_carveout_size;
phys_addr_t tegra_vpr_start;
phys_addr_t tegra_vpr_size;
phys_addr_t tegra_tsec_start;
phys_addr_t tegra_tsec_size;
phys_addr_t tegra_lp0_vec_start;
phys_addr_t tegra_lp0_vec_size;
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
phys_addr_t tegra_wb0_params_address;
phys_addr_t tegra_wb0_params_instances;
phys_addr_t tegra_wb0_params_block_size;
#endif

#ifdef CONFIG_TEGRA_NVDUMPER
unsigned long nvdumper_reserved;
#endif
#ifdef CONFIG_TEGRA_USE_SECURE_KERNEL
unsigned long tegra_tzram_start;
unsigned long tegra_tzram_size;
#endif
bool tegra_lp0_vec_relocate;
unsigned long tegra_grhost_aperture = ~0ul;
static   bool is_tegra_debug_uart_hsport;
static struct board_info main_board_info;
static struct board_info pmu_board_info;
static struct board_info display_board_info;
static int panel_id;
static struct board_info camera_board_info;
static int touch_vendor_id;
static int touch_panel_id;
static struct board_info io_board_info;
static struct board_info button_board_info;
static struct board_info joystick_board_info;
static struct board_info rightspeaker_board_info;
static struct board_info leftspeaker_board_info;
#ifdef CONFIG_TEGRA_USE_NCT
unsigned long tegra_nck_start;
unsigned long tegra_nck_size;
#endif

static int pmu_core_edp;
static int board_panel_type;
static enum power_supply_type pow_supply_type = POWER_SUPPLY_TYPE_MAINS;
static int pwr_i2c_clk = 400;
static u8 power_config;
static u8 display_config;

static int tegra_split_mem_set;

struct device tegra_generic_cma_dev;
struct device tegra_vpr_cma_dev;

#define CREATE_TRACE_POINTS
#include <trace/events/nvsecurity.h>

u32 notrace tegra_read_cycle(void)
{
	u32 cycle_count;

	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycle_count));

	return cycle_count;
}

/*
 * Storage for debug-macro.S's state.
 *
 * This must be in .data not .bss so that it gets initialized each time the
 * kernel is loaded. The data is declared here rather than debug-macro.S so
 * that multiple inclusions of debug-macro.S point at the same data.
 */
u32 tegra_uart_config[4] = {
	/* Debug UART initialization required */
	1,
	/* Debug UART physical address */
	0,
	/* Debug UART virtual address */
	0,
	/* Scratch space for debug macro */
	0,
};


#define NEVER_RESET 0
static DEFINE_SPINLOCK(ahb_lock);

void ahb_gizmo_writel(unsigned long val, void __iomem *reg)
{
	unsigned long check;
	int retry = 10;
	unsigned long flags;

	/* Read and check if write is successful,
	 * if val doesn't match with read, retry write.
	 */
	spin_lock_irqsave(&ahb_lock, flags);
	do {
		writel(val, reg);
		check = readl(reg);
		if (likely(check == val))
			break;
		else
			pr_err("AHB register access fail for reg\n");
	} while (--retry);
	spin_unlock_irqrestore(&ahb_lock, flags);
}

void tegra_assert_system_reset(char mode, const char *cmd)
{
	void __iomem *reset = IO_ADDRESS(TEGRA_PMC_BASE + 0);
	u32 reg;
	bool empty_command = false;

	if (tegra_platform_is_fpga() || NEVER_RESET) {
		pr_info("tegra_assert_system_reset() ignored.....");
		do { } while (1);
	}

	reg = readl_relaxed(reset + PMC_SCRATCH0);
	/* Writing recovery kernel or Bootloader mode in SCRATCH0 31:30:1 */
	if (cmd) {
		if (!strcmp(cmd, "recovery"))
			reg |= RECOVERY_MODE;
		else if (!strcmp(cmd, "bootloader"))
			reg |= BOOTLOADER_MODE;
		else if (!strcmp(cmd, "forced-recovery"))
			reg |= FORCED_RECOVERY_MODE;
		else {
			reg &= ~(BOOTLOADER_MODE | RECOVERY_MODE | FORCED_RECOVERY_MODE);
			empty_command = true;
		}
	}
	else {
		/* Clearing SCRATCH0 31:30:1 on default reboot */
		reg &= ~(BOOTLOADER_MODE | RECOVERY_MODE | FORCED_RECOVERY_MODE);
	}
	writel_relaxed(reg, reset + PMC_SCRATCH0);
	if ((!cmd || empty_command) && pm_power_reset) {
		pm_power_reset();
	} else {
		reg = readl_relaxed(reset);
		reg |= 0x10;
		writel_relaxed(reg, reset);
	}
}
static int modem_id;
static int commchip_id;
static int sku_override;
static int debug_uart_port_id;
static bool uart_over_sd;
static enum audio_codec_type audio_codec_name;
static enum image_type board_image_type = system_image;
static int max_cpu_current;
static int max_core_current;
static int emc_max_dvfs;
static unsigned int memory_type;
static int usb_port_owner_info;
static int lane_owner_info;

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
static __initdata struct tegra_clk_init_table tegra11x_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "clk_m",	NULL,		0,		true },
	{ "emc",	NULL,		0,		true },
	{ "cpu",	NULL,		0,		true },
	{ "kfuse",	NULL,		0,		true },
	{ "fuse",	NULL,		0,		true },
	{ "sclk",	NULL,		0,		true },
	{ "pll_p",	NULL,		0,		true },
	{ "pll_p_out1",	"pll_p",	0,		false },
	{ "pll_p_out3",	"pll_p",	0,		false },
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	{ "pll_m_out1",	"pll_m",	275000000,	false },
	{ "pll_p_out2",	 "pll_p",	102000000,	false },
	{ "sclk",	 "pll_p_out2",	102000000,	true },
	{ "pll_p_out4",	 "pll_p",	204000000,	true },
	{ "hclk",	"sclk",		102000000,	true },
	{ "pclk",	"hclk",		51000000,	true },
	{ "mselect",	"pll_p",	102000000,	true },
	{ "host1x",	"pll_p",	102000000,	false },
	{ "cl_dvfs_ref", "pll_p",       51000000,       true },
	{ "cl_dvfs_soc", "pll_p",       51000000,       true },
	{ "dsialp", "pll_p",	70000000,	false },
	{ "dsiblp", "pll_p",	70000000,	false },
#else
	{ "pll_m_out1",	"pll_m",	275000000,	true },
	{ "pll_p_out2",	"pll_p",	108000000,	false },
	{ "sclk",	"pll_p_out2",	108000000,	true },
	{ "pll_p_out4",	"pll_p",	216000000,	true },
	{ "hclk",	"sclk",		108000000,	true },
	{ "pclk",	"hclk",		54000000,	true },
	{ "mselect",	"pll_p",	108000000,	true },
	{ "host1x",	"pll_p",	108000000,	false },
	{ "cl_dvfs_ref", "clk_m",	13000000,	false },
	{ "cl_dvfs_soc", "clk_m",	13000000,	false },
#endif
#ifdef CONFIG_TEGRA_SLOW_CSITE
	{ "csite",	"clk_m",	1000000,	true },
#else
	{ "csite",      NULL,           0,              true },
#endif
	{ "pll_u",	NULL,		480000000,	true },
	{ "pll_re_vco",	NULL,		612000000,	true },
	{ "xusb_falcon_src",	"pll_p",	204000000,	false},
	{ "xusb_host_src",	"pll_p",	102000000,	false},
	{ "xusb_ss_src",	"pll_re_vco",	122400000,	false},
	{ "xusb_hs_src",	"xusb_ss_div2",	61200000,	false},
	{ "xusb_fs_src",	"pll_u_48M",	48000000,	false},
	{ "sdmmc1",	"pll_p",	48000000,	false},
	{ "sdmmc3",	"pll_p",	48000000,	false},
	{ "sdmmc4",	"pll_p",	48000000,	false},
	{ "sbc1.sclk",	NULL,		40000000,	false},
	{ "sbc2.sclk",	NULL,		40000000,	false},
	{ "sbc3.sclk",	NULL,		40000000,	false},
	{ "sbc4.sclk",	NULL,		40000000,	false},
	{ "sbc5.sclk",	NULL,		40000000,	false},
	{ "sbc6.sclk",	NULL,		40000000,	false},
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	250000000,	false },
	{ "c3bus",	"pll_c3",	250000000,	false },
	{ "pll_c",	NULL,		624000000,	false },
#else
	{ "cbus",	"pll_c",	250000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	150000000,	false },
#ifdef CONFIG_TEGRA_PLLM_SCALED
	{ "vi",		"pll_p",	0,		false},
#endif
#ifdef CONFIG_TEGRA_SOCTHERM
	{ "soc_therm",	"pll_p",	136000000,	false },
	{ "tsensor",	"clk_m",	500000,		false },
#endif
	{ "csite",	NULL,		0,		true },
	{ NULL,		NULL,		0,		0},
};
static __initdata struct tegra_clk_init_table tegra11x_cbus_init_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	250000000,	false },
	{ "c3bus",	"pll_c3",	250000000,	false },
	{ "pll_c",	NULL,		624000000,	false },
#else
	{ "cbus",	"pll_c",	250000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	150000000,	false },
	{ NULL,		NULL,		0,		0},
};
#endif
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
static __initdata struct tegra_clk_init_table tegra12x_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "clk_m",	NULL,		0,		true },
	{ "mc",		NULL,		0,		true },
	{ "cpu",	NULL,		0,		true },
	{ "kfuse",	NULL,		0,		true },
	{ "fuse",	NULL,		0,		true },
	{ "sclk",	NULL,		0,		true },
	{ "pll_p",	NULL,		0,		true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out1",	"pll_p",	0,		false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out3",	"pll_p",	0,		true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_m_out1",	"pll_m",	275000000,	false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out2",	"pll_p",	102000000,	false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "sclk",	"pll_p_out2",	102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out4",	"pll_p",	204000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "host1x",	"pll_p",	102000000,	false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "cl_dvfs_ref", "pll_p",	54000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "cl_dvfs_soc", "pll_p",	54000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "hclk",	"sclk",		102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pclk",	"hclk",		51000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "wake.sclk",	NULL,		40000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "mselect",	"pll_p",	102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out5", "pll_p",	102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
	{ "pll_m_out1",	"pll_m",	275000000,	true,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "pll_p_out2",	"pll_p",	108000000,	false,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "sclk",	"pll_p_out2",	108000000,	true,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "pll_p_out4",	"pll_p",	216000000,	true,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "hclk",	"sclk",		108000000,	true,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "pclk",	"hclk",		54000000,	true,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "mselect",	"pll_p",	108000000,	true,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "host1x",	"pll_p",	108000000,	false,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "cl_dvfs_ref", "clk_m",	13000000,	false,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "cl_dvfs_soc", "clk_m",	13000000,	false,
		TEGRA_CLK_INIT_PLATFORM_NON_SI },
	{ "vde",	"pll_c3",	48400000,	true,
		TEGRA_CLK_INIT_CPU_ASIM},
#endif
#ifdef CONFIG_TEGRA_SLOW_CSITE
	{ "csite",	"clk_m",	1000000,	true },
#else
	{ "csite",      NULL,           0,              true },
#endif
	{ "pll_u",	NULL,		480000000,	true },
	{ "pll_re_vco",	NULL,		672000000,	true },
	{ "xusb_falcon_src",	"pll_re_out",	224000000,	false},
	{ "xusb_host_src",	"pll_re_out",	112000000,	false},
	{ "xusb_ss_src",	"pll_u_480M",	120000000,	false},
	{ "xusb_hs_src",	"pll_u_60M",	60000000,	false},
	{ "xusb_fs_src",	"pll_u_48M",	48000000,	false},
	{ "sdmmc1",	"pll_p",	48000000,	false},
	{ "sdmmc3",	"pll_p",	48000000,	false},
	{ "sdmmc4",	"pll_p",	48000000,	false},
	{ "sdmmc1_ddr",	"pll_p",	48000000,	false},
	{ "sdmmc3_ddr",	"pll_p",	48000000,	false},
	{ "sdmmc4_ddr",	"pll_p",	48000000,	false},
	{ "sbc1.sclk",	NULL,		40000000,	false},
	{ "sbc2.sclk",	NULL,		40000000,	false},
	{ "sbc3.sclk",	NULL,		40000000,	false},
	{ "sbc4.sclk",	NULL,		40000000,	false},
	{ "sbc5.sclk",	NULL,		40000000,	false},
	{ "sbc6.sclk",	NULL,		40000000,	false},
	{ "cpu.mselect", NULL,		102000000,	true},
	{ "gpu_ref",	NULL,		0,		true},
	{ "gk20a.gbus",	NULL,		252000000,	false},
#ifdef CONFIG_TEGRA_PLLM_SCALED
	{ "vi",		"pll_p",	0,		false},
	{ "isp",	"pll_p",	0,		false},
#endif
#ifdef CONFIG_TEGRA_SOCTHERM
	{ "soc_therm",	"pll_p",	51000000,	false },
	{ "tsensor",	"clk_m",	500000,		false },
#endif
	{ "pll_d",	NULL,		0,		true },
	{ NULL,		NULL,		0,		0},
};
static __initdata struct tegra_clk_init_table tegra12x_cbus_init_table[] = {
	/* Initialize c2bus, c3bus, or cbus at the end of the list
	 * after all the clocks are moved under the proper parents.
	 */
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	250000000,	false },
	{ "c3bus",	"pll_c3",	250000000,	false },
	{ "pll_c",	NULL,		600000000,	false },
#else
	{ "cbus",	"pll_c",	200000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	100000000,	false },
	{ "c4bus",	"pll_c4",	200000000,	false },
	{ NULL,		NULL,		0,		0},
};
#endif

#ifdef CONFIG_ARCH_TEGRA_14x_SOC
static __initdata struct tegra_clk_init_table tegra14x_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "osc",	NULL,		0,		true },
	{ "clk_m",	"osc",		0,		true },
	{ "emc",	NULL,		0,		true },
	{ "cpu",	NULL,		0,		true },
	{ "kfuse",	NULL,		0,		true },
	{ "fuse",	NULL,		0,		true },
	{ "sclk",	NULL,		0,		true },
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	{ "pll_p",	NULL,		0,		true },
	{ "pll_p_out1",	"pll_p",	0,		false },
	{ "pll_p_out3",	"pll_p",	0,		true },
	{ "pll_m_out1",	"pll_m",	275000000,	false },
	{ "pll_p_out2",	 "pll_p",	102000000,	false },
	{ "sclk",	 "pll_p_out2",	102000000,	true },
	{ "pll_p_out4",	 "pll_p",	204000000,	true },
	{ "hclk",	"sclk",		102000000,	true },
	{ "pclk",	"hclk",		51000000,	true },
	{ "mselect",	"pll_p",	102000000,	true },
	{ "host1x",	"pll_p",	102000000,	false },
	{ "cl_dvfs_ref", "pll_p",       51000000,       true },
	{ "cl_dvfs_soc", "pll_p",       51000000,       true },
#else
	{ "pll_p",	NULL,		0,		true },
	{ "pll_p_out1",	"pll_p",	0,		false },
	{ "pll_p_out3",	"pll_p",	0,		true },
	{ "pll_m_out1",	"pll_m",	275000000,	true },
	{ "pll_p_out2",	"pll_p",	108000000,	false },
	{ "sclk",	"pll_p_out2",	108000000,	true },
	{ "pll_p_out4",	"pll_p",	216000000,	true },
	{ "host1x",	"pll_p",	108000000,	false },
	{ "cl_dvfs_ref", "clk_m",	13000000,	false },
	{ "cl_dvfs_soc", "clk_m",	13000000,	false },
	{ "hclk",	"sclk",		108000000,	true },
	{ "pclk",	"hclk",		54000000,	true },
	{ "wake.sclk",  NULL,           250000000,	true },
	{ "mselect",	"pll_p",	108000000,	true },
#endif
#ifdef CONFIG_TEGRA_SLOW_CSITE
	{ "csite",	"clk_m",	1000000,	true },
#else
	{ "csite",      NULL,           0,              true },
#endif
	{ "pll_u",	NULL,		480000000,	false },
	{ "sdmmc1",	"pll_p",	48000000,	false},
	{ "sdmmc3",	"pll_p",	48000000,	false},
	{ "sdmmc4",	"pll_p",	48000000,	false},
	{ "mon.avp",	NULL,		80000000,	true },
	{ "sbc1.sclk",	NULL,		20000000,	false},
	{ "sbc2.sclk",	NULL,		20000000,	false},
	{ "sbc3.sclk",	NULL,		20000000,	false},
	{ "msenc",	"pll_p",	108000000,	false },
	{ "tsec",	"pll_p",	108000000,	false },
	{ "mc_capa",	"emc",		0,		true },
	{ "mc_cbpa",	"emc",		0,		true },
#ifdef CONFIG_TEGRA_SOCTHERM
	{ "soc_therm",	"pll_p",	51000000,	false },
	{ "tsensor",	"clk_m",	400000,		false },
#endif
	{ NULL,		NULL,		0,		0},
};
static __initdata struct tegra_clk_init_table tegra14x_cbus_init_table[] = {
	/* Initialize c2bus, c3bus, or cbus at the end of the list
	 * after all the clocks are moved under the proper parents.
	 */
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	200000000,	false },
	{ "c3bus",	"pll_c3",	200000000,	false },
	{ "pll_c",	NULL,		768000000,	false },
#else
	{ "cbus",	"pll_c",	200000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	100000000,	false },
	{ NULL,		NULL,		0,		0},
};
#endif

#ifdef CONFIG_CACHE_L2X0
#ifdef CONFIG_TEGRA_USE_SECURE_KERNEL
static void tegra_cache_smc(bool enable, u32 arg)
{
	void __iomem *p = IO_ADDRESS(TEGRA_ARM_PL310_BASE);
	bool need_affinity_switch;
	bool can_switch_affinity;
	bool l2x0_enabled;
	cpumask_t local_cpu_mask;
	cpumask_t saved_cpu_mask;
	unsigned long flags;
	long ret;

	/*
	 * ISSUE : Some registers of PL310 controler must be written
	 *              from Secure context (and from CPU0)!
	 *
	 * When called form Normal we obtain an abort or do nothing.
	 * Instructions that must be called in Secure:
	 *      - Write to Control register (L2X0_CTRL==0x100)
	 *      - Write in Auxiliary controler (L2X0_AUX_CTRL==0x104)
	 *      - Invalidate all entries (L2X0_INV_WAY==0x77C),
	 *              mandatory at boot time.
	 *      - Tag and Data RAM Latency Control Registers
	 *              (0x108 & 0x10C) must be written in Secure.
	 */
	need_affinity_switch = (smp_processor_id() != 0);
	can_switch_affinity = !irqs_disabled();

	WARN_ON(need_affinity_switch && !can_switch_affinity);
	if (need_affinity_switch && can_switch_affinity) {
		cpu_set(0, local_cpu_mask);
		sched_getaffinity(0, &saved_cpu_mask);
		ret = sched_setaffinity(0, &local_cpu_mask);
		WARN_ON(ret != 0);
	}

	local_irq_save(flags);
	l2x0_enabled = readl_relaxed(p + L2X0_CTRL) & 1;
	if (enable && !l2x0_enabled)
		tegra_sm_generic(0x82000002, 0x00000001, arg);
	else if (!enable && l2x0_enabled)
		tegra_sm_generic(0x82000002, 0x00000002, arg);
	local_irq_restore(flags);

	if (need_affinity_switch && can_switch_affinity) {
		ret = sched_setaffinity(0, &saved_cpu_mask);
		WARN_ON(ret != 0);
	}
}

static void tegra_l2x0_disable(void)
{
	unsigned long flags;
	static u32 l2x0_way_mask;

	if (!l2x0_way_mask) {
		void __iomem *p = IO_ADDRESS(TEGRA_ARM_PL310_BASE);
		u32 aux_ctrl;
		u32 ways;

		aux_ctrl = readl_relaxed(p + L2X0_AUX_CTRL);
		ways = (aux_ctrl & (1 << 16)) ? 16 : 8;
		l2x0_way_mask = (1 << ways) - 1;
	}

	local_irq_save(flags);
	tegra_cache_smc(false, l2x0_way_mask);
	local_irq_restore(flags);
}
#endif	/* CONFIG_TEGRA_USE_SECURE_KERNEL */

void tegra_init_cache(bool init)
{
	void __iomem *p = IO_ADDRESS(TEGRA_ARM_PL310_BASE);
	u32 aux_ctrl;
#ifndef CONFIG_TEGRA_USE_SECURE_KERNEL
	u32 cache_type;
	u32 tag_latency, data_latency;
#endif

#ifdef CONFIG_TEGRA_USE_SECURE_KERNEL
	/* issue the SMC to enable the L2 */
	aux_ctrl = readl_relaxed(p + L2X0_AUX_CTRL);
	trace_smc_init_cache(NVSEC_SMC_START);
	tegra_cache_smc(true, aux_ctrl);
	trace_smc_init_cache(NVSEC_SMC_DONE);

	/* after init, reread aux_ctrl and register handlers */
	aux_ctrl = readl_relaxed(p + L2X0_AUX_CTRL);
	l2x0_init(p, aux_ctrl, 0xFFFFFFFF);

	/* override outer_disable() with our disable */
	outer_cache.disable = tegra_l2x0_disable;
#else
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	tag_latency = 0x331;
	data_latency = 0x441;
#else
	if (!tegra_platform_is_silicon()) {
		tag_latency = 0x770;
		data_latency = 0x770;
	} else if (is_lp_cluster()) {
		tag_latency = tegra_cpu_c1_l2_tag_latency;
		data_latency = tegra_cpu_c1_l2_data_latency;
	} else {
		tag_latency = tegra_cpu_c0_l2_tag_latency;
		data_latency = tegra_cpu_c0_l2_data_latency;
	}
#endif
	writel_relaxed(tag_latency, p + L2X0_TAG_LATENCY_CTRL);
	writel_relaxed(data_latency, p + L2X0_DATA_LATENCY_CTRL);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (!tegra_platform_is_fpga()) {
#ifdef CONFIG_ARCH_TEGRA_14x_SOC
		/* Enable double line fill */
		writel(0x40000007, p + L2X0_PREFETCH_CTRL);
#else
		writel(0x7, p + L2X0_PREFETCH_CTRL);
#endif
		writel(0x3, p + L2X0_POWER_CTRL);
	}
#endif
	cache_type = readl(p + L2X0_CACHE_TYPE);
	aux_ctrl = (cache_type & 0x700) << (17-8);
	aux_ctrl |= 0x7C400001;
	if (init) {
		l2x0_init(p, aux_ctrl, 0x8200c3fe);
	} else {
		u32 tmp;

		tmp = aux_ctrl;
		aux_ctrl = readl(p + L2X0_AUX_CTRL);
		aux_ctrl &= 0x8200c3fe;
		aux_ctrl |= tmp;
		writel(aux_ctrl, p + L2X0_AUX_CTRL);
	}
	l2x0_enable();
#endif
}
#endif

static void __init tegra_perf_init(void)
{
	u32 reg;

#ifdef CONFIG_ARM64
	asm volatile("mrs %0, PMCR_EL0" : "=r"(reg));
	reg >>= 11;
	reg = (1 << (reg & 0x1f))-1;
	reg |= 0x80000000;
	asm volatile("msr PMINTENCLR_EL1, %0" : : "r"(reg));
	reg = 1;
	asm volatile("msr PMUSERENR_EL0, %0" : : "r"(reg));
#else
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(reg));
	reg >>= 11;
	reg = (1 << (reg & 0x1f))-1;
	reg |= 0x80000000;
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(reg));
	reg = 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(reg));
#endif
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
static void __init tegra_ramrepair_init(void)
{
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	if (tegra_spare_fuse(10)  | tegra_spare_fuse(11)) {
#endif
		u32 reg;
		reg = readl(FLOW_CTRL_RAM_REPAIR);
		reg &= ~FLOW_CTRL_RAM_REPAIR_BYPASS_EN;
		writel(reg, FLOW_CTRL_RAM_REPAIR);
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	}
#endif
}
#endif

static void __init tegra_init_power(void)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_SATA);
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_PCIE);
#endif

#if defined(CONFIG_TEGRA_XUSB_PLATFORM)
	/* powergate xusb partitions by default */
	tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_XUSBB);
	tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_XUSBA);
	tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_XUSBC);
#endif

}

static inline unsigned long gizmo_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static inline void gizmo_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static void __init tegra_init_ahb_gizmo_settings(void)
{
	unsigned long val;

	val = gizmo_readl(AHB_GIZMO_AHB_MEM);
	val |= ENB_FAST_REARBITRATE | IMMEDIATE | DONT_SPLIT_AHB_WR;

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11)
		val |= WR_WAIT_COMMIT_ON_1K;
#if defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	val |= WR_WAIT_COMMIT_ON_1K | EN_USB_WAIT_COMMIT_ON_1K_STALL;
#endif
	gizmo_writel(val, AHB_GIZMO_AHB_MEM);

	val = gizmo_readl(AHB_GIZMO_USB);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB);

	val = gizmo_readl(AHB_GIZMO_USB2);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB2);

	val = gizmo_readl(AHB_GIZMO_USB3);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB3);

	val = gizmo_readl(AHB_ARBITRATION_PRIORITY_CTRL);
	val |= PRIORITY_SELECT_USB | PRIORITY_SELECT_USB2 | PRIORITY_SELECT_USB3
				| AHB_PRIORITY_WEIGHT(7);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	val |= PRIORITY_SELECT_SE;
#endif

	gizmo_writel(val, AHB_ARBITRATION_PRIORITY_CTRL);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG1);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | AHBDMA_MST_ID |
		ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG1));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG2);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG2));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG3);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB3_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG3));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG4);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB2_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG4));

	/*
	 * SDMMC controller is removed from AHB interface in T124 and
	 * later versions of Tegra. Configure AHB prefetcher for SDMMC4
	 * in T11x and T14x SOCs.
	 */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	val = gizmo_readl(AHB_MEM_PREFETCH_CFG5);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | SDMMC4_MST_ID;
	val &= ~SDMMC4_MST_ID;
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG5));
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	val = gizmo_readl(AHB_MEM_PREFETCH_CFG6);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | SE_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG6));
#endif
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
void __init tegra20_init_early(void)
{
#ifndef CONFIG_SMP
	/* For SMP system, initializing the reset handler here is too
	   late. For non-SMP systems, the function that calls the reset
	   handler initializer is not called, so do it here for non-SMP. */
	tegra_cpu_reset_handler_init();
#endif
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_init_cache(true);
	tegra_pmc_init();
	tegra_powergate_init();
	tegra20_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
	tegra_init_debug_uart_rate();
	tegra_ram_console_debug_reserve(SZ_1M);
}
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
void __init tegra30_init_early(void)
{
	u32 speedo;
	u32 tag_latency, data_latency;

	display_tegra_dt_info();
#ifndef CONFIG_SMP
	/* For SMP system, initializing the reset handler here is too
	   late. For non-SMP systems, the function that calls the reset
	   handler initializer is not called, so do it here for non-SMP. */
	tegra_cpu_reset_handler_init();
#endif
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	/*
	 * Store G/LP cluster L2 latencies to IRAM and DRAM
	 */
	tegra_cpu_c1_l2_tag_latency = 0x221;
	tegra_cpu_c1_l2_data_latency = 0x221;
	writel_relaxed(0x221, tegra_cpu_c1_l2_tag_latency_iram);
	writel_relaxed(0x221, tegra_cpu_c1_l2_data_latency_iram);
	/* relax l2-cache latency for speedos 4,5,6 (T33's chips) */
	speedo = tegra_cpu_speedo_id();
	if (speedo == 4 || speedo == 5 || speedo == 6 ||
	    speedo == 12 || speedo == 13) {
		tag_latency = 0x442;
		data_latency = 0x552;
	} else {
		tag_latency = 0x441;
		data_latency = 0x551;
	}
	tegra_cpu_c0_l2_tag_latency = tag_latency;
	tegra_cpu_c0_l2_data_latency = data_latency;
	writel_relaxed(tag_latency, tegra_cpu_c0_l2_tag_latency_iram);
	writel_relaxed(data_latency, tegra_cpu_c0_l2_data_latency_iram);
	tegra_init_cache(true);
	tegra_pmc_init();
	tegra_powergate_init();
	tegra30_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
	tegra_init_debug_uart_rate();
	tegra_ram_console_debug_reserve(SZ_1M);

	init_dma_coherent_pool_size(SZ_1M);
}
#endif
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
void __init tegra11x_init_early(void)
{
	display_tegra_dt_info();
#ifndef CONFIG_SMP
	/* For SMP system, initializing the reset handler here is too
	   late. For non-SMP systems, the function that calls the reset
	   handler initializer is not called, so do it here for non-SMP. */
	tegra_cpu_reset_handler_init();
#endif
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_ramrepair_init();
	tegra11x_init_clocks();
	tegra11x_init_dvfs();
	tegra_common_init_clock();
	tegra_clk_init_from_table(tegra11x_clk_init_table);
	tegra_clk_init_cbus_plls_from_table(tegra11x_cbus_init_table);
	tegra11x_clk_init_la();
	tegra_pmc_init();
	tegra_powergate_init();
	tegra30_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
	tegra_init_debug_uart_rate();

	init_dma_coherent_pool_size(SZ_2M);
}
#endif
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
void __init tegra12x_init_early(void)
{
	display_tegra_dt_info();
#ifndef CONFIG_SMP
	/* For SMP system, initializing the reset handler here is too
	   late. For non-SMP systems, the function that calls the reset
	   handler initializer is not called, so do it here for non-SMP. */
	tegra_cpu_reset_handler_init();
#endif
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_ramrepair_init();
	tegra12x_init_clocks();
	tegra12x_init_dvfs();
	tegra_common_init_clock();
	tegra_clk_init_from_table(tegra12x_clk_init_table);
	tegra_clk_init_cbus_plls_from_table(tegra12x_cbus_init_table);
	tegra_pmc_init();
	tegra_powergate_init();
	tegra30_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
	tegra_init_debug_uart_rate();
}
#endif
#ifdef CONFIG_ARCH_TEGRA_14x_SOC
void __init tegra14x_init_early(void)
{
	display_tegra_dt_info();
#ifndef CONFIG_SMP
	/* For SMP system, initializing the reset handler here is too
	   late. For non-SMP systems, the function that calls the reset
	   handler initializer is not called, so do it here for non-SMP. */
	tegra_cpu_reset_handler_init();
#endif
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_ramrepair_init();
	tegra14x_init_clocks();
	tegra14x_init_dvfs();
	tegra_common_init_clock();
	tegra_clk_init_from_table(tegra14x_clk_init_table);
	tegra_clk_init_cbus_plls_from_table(tegra14x_cbus_init_table);
	tegra_cpu_c1_l2_tag_latency = 0x110;
	tegra_cpu_c1_l2_data_latency = 0x331;
	writel_relaxed(0x110, tegra_cpu_c1_l2_tag_latency_iram);
	writel_relaxed(0x331, tegra_cpu_c1_l2_data_latency_iram);
	tegra_cpu_c0_l2_tag_latency = 0x111;
	tegra_cpu_c0_l2_data_latency = 0x441;
	writel_relaxed(0x111, tegra_cpu_c0_l2_tag_latency_iram);
	writel_relaxed(0x441, tegra_cpu_c0_l2_data_latency_iram);
	tegra_init_cache(true);
	tegra_pmc_init();
	tegra_powergate_init();
	tegra30_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
	tegra_init_debug_uart_rate();
	tegra_ram_console_debug_reserve(SZ_1M);
}
#endif
static int __init tegra_lp0_vec_arg(char *options)
{
	char *p = options;

	tegra_lp0_vec_size = memparse(p, &p);
	if (*p == '@')
		tegra_lp0_vec_start = memparse(p+1, &p);
	if (!tegra_lp0_vec_size || !tegra_lp0_vec_start) {
		tegra_lp0_vec_size = 0;
		tegra_lp0_vec_start = 0;
	}

	return 0;
}
early_param("lp0_vec", tegra_lp0_vec_arg);

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
static int __init tegra_wb0_params_arg(char *options)
{
	char *p = options;

	tegra_wb0_params_address = memparse(p, &p);
	if (*p == ',')
		tegra_wb0_params_instances = memparse(p+1, &p);
	if (*p == ',')
		tegra_wb0_params_block_size = memparse(p+1, &p);

	return 0;
}
early_param("wb0_params", tegra_wb0_params_arg);
#endif

#ifdef CONFIG_TEGRA_NVDUMPER
static int __init tegra_nvdumper_arg(char *options)
{
	char *p = options;

	nvdumper_reserved = memparse(p, &p);
	return 0;
}
early_param("nvdumper_reserved", tegra_nvdumper_arg);
#endif

static int __init tegra_bootloader_fb_arg(char *options)
{
	char *p = options;

	tegra_bootloader_fb_size = memparse(p, &p);
	if (*p == '@')
		tegra_bootloader_fb_start = memparse(p+1, &p);

	pr_info("Found tegra_fbmem: %08llx@%08llx\n",
		(u64)tegra_bootloader_fb_size, (u64)tegra_bootloader_fb_start);

	return 0;
}
early_param("tegra_fbmem", tegra_bootloader_fb_arg);

static int __init tegra_bootloader_fb2_arg(char *options)
{
	char *p = options;

	tegra_bootloader_fb2_size = memparse(p, &p);
	if (*p == '@')
		tegra_bootloader_fb2_start = memparse(p+1, &p);

	pr_info("Found tegra_fbmem2: %08llx@%08llx\n",
		(u64)tegra_bootloader_fb2_size,
		(u64)tegra_bootloader_fb2_start);

	return 0;
}
early_param("tegra_fbmem2", tegra_bootloader_fb2_arg);

static int __init tegra_sku_override(char *id)
{
	char *p = id;

	sku_override = memparse(p, &p);

	return 0;
}
early_param("sku_override", tegra_sku_override);

int tegra_get_sku_override(void)
{
	return sku_override;
}

static int __init tegra_vpr_arg(char *options)
{
	char *p = options;

	tegra_vpr_size = memparse(p, &p);
	if (*p == '@')
		tegra_vpr_start = memparse(p+1, &p);
	pr_info("Found vpr, start=0x%llx size=%llx",
		(u64)tegra_vpr_start, (u64)tegra_vpr_size);
	return 0;
}
early_param("vpr", tegra_vpr_arg);

static int __init tegra_tsec_arg(char *options)
{
	char *p = options;

	tegra_tsec_size = memparse(p, &p);
	if (*p == '@')
		tegra_tsec_start = memparse(p+1, &p);
	pr_info("Found tsec, start=0x%llx size=%llx",
		(u64)tegra_tsec_start, (u64)tegra_tsec_size);
	return 0;
}
early_param("tsec", tegra_tsec_arg);

#ifdef CONFIG_TEGRA_USE_SECURE_KERNEL
static int __init tegra_tzram_arg(char *options)
{
	char *p = options;

	tegra_tzram_size = memparse(p, &p);
	if (*p == '@')
		tegra_tzram_start = memparse(p + 1, &p);
	return 0;
}
early_param("tzram", tegra_tzram_arg);
#endif

#ifdef CONFIG_TEGRA_USE_NCT
static int __init tegra_nck_arg(char *options)
{
	char *p = options;

	tegra_nck_size = memparse(p, &p);
	if (*p == '@')
		tegra_nck_start = memparse(p+1, &p);
	if (!tegra_nck_size || !tegra_nck_start) {
		tegra_nck_size = 0;
		tegra_nck_start = 0;
	}

	return 0;
}
early_param("nck", tegra_nck_arg);
#endif	/* CONFIG_TEGRA_USE_NCT */

enum panel_type get_panel_type(void)
{
	return board_panel_type;
}
static int __init tegra_board_panel_type(char *options)
{
	if (!strcmp(options, "lvds"))
		board_panel_type = panel_type_lvds;
	else if (!strcmp(options, "dsi"))
		board_panel_type = panel_type_dsi;
	else
		return 0;
	return 1;
}
__setup("panel=", tegra_board_panel_type);

int tegra_get_board_panel_id(void)
{
	return panel_id;
}
static int __init tegra_board_panel_id(char *options)
{
	char *p = options;
	panel_id = memparse(p, &p);
	return panel_id;
}
__setup("display_panel=", tegra_board_panel_id);

int tegra_get_touch_vendor_id(void)
{
	return touch_vendor_id;
}
int tegra_get_touch_panel_id(void)
{
	return touch_panel_id;
}
static int __init tegra_touch_id(char *options)
{
	char *p = options;
	touch_vendor_id = memparse(p, &p);
	if (*p == '@')
		touch_panel_id = memparse(p+1, &p);
	return 1;
}
__setup("touch_id=", tegra_touch_id);

u8 get_power_config(void)
{
	return power_config;
}
static int __init tegra_board_power_config(char *options)
{
	char *p = options;
	power_config = memparse(p, &p);
	return 1;
}
__setup("power-config=", tegra_board_power_config);

u8 get_display_config(void)
{
	return display_config;
}
static int __init tegra_board_display_config(char *options)
{
	char *p = options;
	display_config = memparse(p, &p);
	return 1;
}
__setup("display-config=", tegra_board_display_config);

enum power_supply_type get_power_supply_type(void)
{
	return pow_supply_type;
}
static int __init tegra_board_power_supply_type(char *options)
{
	if (!strcmp(options, "Adapter"))
		pow_supply_type = POWER_SUPPLY_TYPE_MAINS;
	if (!strcmp(options, "Mains"))
		pow_supply_type = POWER_SUPPLY_TYPE_MAINS;
	else if (!strcmp(options, "Battery"))
		pow_supply_type = POWER_SUPPLY_TYPE_BATTERY;
	else
		return 0;
	return 1;
}
__setup("power_supply=", tegra_board_power_supply_type);

int get_core_edp(void)
{
	return pmu_core_edp;
}
static int __init tegra_pmu_core_edp(char *options)
{
	char *p = options;
	int core_edp = memparse(p, &p);
	if (core_edp != 0)
		pmu_core_edp = core_edp;
	return 0;
}
early_param("core_edp_mv", tegra_pmu_core_edp);

int get_maximum_cpu_current_supported(void)
{
	return max_cpu_current;
}
static int __init tegra_max_cpu_current(char *options)
{
	char *p = options;
	max_cpu_current = memparse(p, &p);
	return 1;
}
__setup("max_cpu_cur_ma=", tegra_max_cpu_current);

int get_maximum_core_current_supported(void)
{
	return max_core_current;
}
static int __init tegra_max_core_current(char *options)
{
	char *p = options;
	max_core_current = memparse(p, &p);
	return 0;
}
early_param("core_edp_ma", tegra_max_core_current);

int get_emc_max_dvfs(void)
{
	return emc_max_dvfs;
}
static int __init tegra_emc_max_dvfs(char *options)
{
	char *p = options;
	emc_max_dvfs = memparse(p, &p);
	return 1;
}
__setup("emc_max_dvfs=", tegra_emc_max_dvfs);

int tegra_get_memory_type(void)
{
	return memory_type;
}
static int __init tegra_memory_type(char *options)
{
	char *p = options;
	memory_type = memparse(p, &p);
	return 1;
}
__setup("memtype=", tegra_memory_type);

static int __init tegra_debug_uartport(char *info)
{
	char *p = info;
	unsigned long long port_id;
	if (!strncmp(p, "hsport", 6))
		is_tegra_debug_uart_hsport = true;
	else if (!strncmp(p, "lsport", 6))
		is_tegra_debug_uart_hsport = false;

	if (p[6] == ',') {
		if (p[7] == '-') {
			debug_uart_port_id = -1;
		} else {
			port_id = memparse(p + 7, &p);
			debug_uart_port_id = (int) port_id;
			if (debug_uart_port_id == 5)
				uart_over_sd = true;
		}
	} else {
		debug_uart_port_id = -1;
	}

	return 1;
}

bool is_tegra_debug_uartport_hs(void)
{
	return is_tegra_debug_uart_hsport;
}

bool is_uart_over_sd_enabled(void)
{
	return uart_over_sd;
}

void set_sd_uart_port_id(int port_id)
{
	debug_uart_port_id = port_id;
}

int get_tegra_uart_debug_port_id(void)
{
	return debug_uart_port_id;
}
__setup("debug_uartport=", tegra_debug_uartport);

static int __init tegra_image_type(char *options)
{
	if (!strcmp(options, "RCK"))
		board_image_type = rck_image;

	return 0;
}

enum image_type get_tegra_image_type(void)
{
	return board_image_type;
}

__setup("image=", tegra_image_type);

static int __init tegra_audio_codec_type(char *info)
{
	char *p = info;
	if (!strncmp(p, "wm8903", 6))
		audio_codec_name = audio_codec_wm8903;
	else
		audio_codec_name = audio_codec_none;

	return 1;
}

enum audio_codec_type get_audio_codec_type(void)
{
	return audio_codec_name;
}
__setup("audio_codec=", tegra_audio_codec_type);

static int tegra_get_pwr_i2c_clk_rate(char *options)
{
	int clk = simple_strtol(options, NULL, 16);
	if (clk != 0)
		pwr_i2c_clk = clk;
	return 0;
}

int get_pwr_i2c_clk_rate(void)
{
	return pwr_i2c_clk;
}
__setup("pwr_i2c=", tegra_get_pwr_i2c_clk_rate);

static bool pmic_wdt_disable;
bool is_pmic_wdt_disabled_at_boot(void)
{
	return pmic_wdt_disable;
}

static int parse_arg_pmic_wdt_disable(char *options)
{
	/* no kernel command line argument "watchdog" interpreted as
	 * watchdog enable
	 */
	pmic_wdt_disable = false;
	if (!(strcmp(options, "enable")))
		pmic_wdt_disable = false;
	else if (!(strcmp(options, "disable")))
		pmic_wdt_disable = true;
	/* kernel command line from fastboot does not support
	 * other values
	 */

	return 0;
}
__setup("watchdog=", parse_arg_pmic_wdt_disable);

void tegra_get_board_info(struct board_info *bi)
{
#ifdef CONFIG_OF
	struct device_node *board_info;
	u32 prop_val;
	int err;

	board_info = of_find_node_by_path("/chosen/board_info");
	if (!IS_ERR_OR_NULL(board_info)) {
		memset(bi, 0, sizeof(*bi));

		err = of_property_read_u32(board_info, "id", &prop_val);
		if (err)
			pr_err("failed to read /chosen/board_info/id\n");
		else
			bi->board_id = prop_val;

		err = of_property_read_u32(board_info, "sku", &prop_val);
		if (err)
			pr_err("failed to read /chosen/board_info/sku\n");
		else
			bi->sku = prop_val;

		err = of_property_read_u32(board_info, "fab", &prop_val);
		if (err)
			pr_err("failed to read /chosen/board_info/fab\n");
		else
			bi->fab = prop_val;

		err = of_property_read_u32(board_info, "major_revision", &prop_val);
		if (err)
			pr_err("failed to read /chosen/board_info/major_revision\n");
		else
			bi->major_revision = prop_val;

		err = of_property_read_u32(board_info, "minor_revision", &prop_val);
		if (err)
			pr_err("failed to read /chosen/board_info/minor_revision\n");
		else
			bi->minor_revision = prop_val;
#ifndef CONFIG_ARM64
		system_serial_high = (bi->board_id << 16) | bi->sku;
		system_serial_low = (bi->fab << 24) |
			(bi->major_revision << 16) | (bi->minor_revision << 8);
#endif
	} else {
#endif
#ifdef CONFIG_ARM64
		/* FIXME:
		 * use dummy values for now as system_serial_high/low
		 * are gone in arm64.
		 */
		bi->board_id = 0xDEAD;
		bi->sku = 0xDEAD;
		bi->fab = 0xDD;
		bi->major_revision = 0xDD;
		bi->minor_revision = 0xDD;
#else
		if (system_serial_high || system_serial_low) {
			bi->board_id = (system_serial_high >> 16) & 0xFFFF;
			bi->sku = (system_serial_high) & 0xFFFF;
			bi->fab = (system_serial_low >> 24) & 0xFF;
			bi->major_revision = (system_serial_low >> 16) & 0xFF;
			bi->minor_revision = (system_serial_low >> 8) & 0xFF;
		} else {
			memcpy(bi, &main_board_info, sizeof(struct board_info));
		}
#endif
#ifdef CONFIG_OF
	}
#endif
}

#ifdef CONFIG_OF
void find_dc_node(struct device_node **dc1_node,
		struct device_node **dc2_node) {
	*dc1_node =
		of_find_node_by_path("/host1x/dc@54200000");
	*dc2_node =
		of_find_node_by_path("/host1x/dc@54240000");
}
#else
void find_dc_node(struct device_node *dc1_node,
		struct device_node *dc2_node) {
	return;
}
#endif

static int __init tegra_main_board_info(char *info)
{
	char *p = info;
	main_board_info.board_id = memparse(p, &p);
	main_board_info.sku = memparse(p+1, &p);
	main_board_info.fab = memparse(p+1, &p);
	main_board_info.major_revision = memparse(p+1, &p);
	main_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

__setup("board_info=", tegra_main_board_info);

static int __init tegra_pmu_board_info(char *info)
{
	char *p = info;
	pmu_board_info.board_id = memparse(p, &p);
	pmu_board_info.sku = memparse(p+1, &p);
	pmu_board_info.fab = memparse(p+1, &p);
	pmu_board_info.major_revision = memparse(p+1, &p);
	pmu_board_info.minor_revision = memparse(p+1, &p);
	return 0;
}

void tegra_get_pmu_board_info(struct board_info *bi)
{
	memcpy(bi, &pmu_board_info, sizeof(struct board_info));
}

early_param("pmuboard", tegra_pmu_board_info);

static int __init tegra_display_board_info(char *info)
{
	char *p = info;
	display_board_info.board_id = memparse(p, &p);
	display_board_info.sku = memparse(p+1, &p);
	display_board_info.fab = memparse(p+1, &p);
	display_board_info.major_revision = memparse(p+1, &p);
	display_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_display_board_info(struct board_info *bi)
{
	memcpy(bi, &display_board_info, sizeof(struct board_info));
}

__setup("displayboard=", tegra_display_board_info);

static int __init tegra_camera_board_info(char *info)
{
	char *p = info;
	camera_board_info.board_id = memparse(p, &p);
	camera_board_info.sku = memparse(p+1, &p);
	camera_board_info.fab = memparse(p+1, &p);
	camera_board_info.major_revision = memparse(p+1, &p);
	camera_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_camera_board_info(struct board_info *bi)
{
	memcpy(bi, &camera_board_info, sizeof(struct board_info));
}

__setup("cameraboard=", tegra_camera_board_info);

static int __init tegra_leftspeaker_board_info(char *info)
{
	char *p = info;
	leftspeaker_board_info.board_id = memparse(p, &p);
	leftspeaker_board_info.sku = memparse(p+1, &p);
	leftspeaker_board_info.fab = memparse(p+1, &p);
	leftspeaker_board_info.major_revision = memparse(p+1, &p);
	leftspeaker_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_leftspeaker_board_info(struct board_info *bi)
{
	memcpy(bi, &leftspeaker_board_info, sizeof(struct board_info));
}

__setup("leftspeakerboard=", tegra_leftspeaker_board_info);

static int __init tegra_rightspeaker_board_info(char *info)
{
	char *p = info;
	rightspeaker_board_info.board_id = memparse(p, &p);
	rightspeaker_board_info.sku = memparse(p+1, &p);
	rightspeaker_board_info.fab = memparse(p+1, &p);
	rightspeaker_board_info.major_revision = memparse(p+1, &p);
	rightspeaker_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_rightspeaker_board_info(struct board_info *bi)
{
	memcpy(bi, &rightspeaker_board_info, sizeof(struct board_info));
}

__setup("rightspeakerboard=", tegra_rightspeaker_board_info);

static int __init tegra_joystick_board_info(char *info)
{
	char *p = info;
	joystick_board_info.board_id = memparse(p, &p);
	joystick_board_info.sku = memparse(p+1, &p);
	joystick_board_info.fab = memparse(p+1, &p);
	joystick_board_info.major_revision = memparse(p+1, &p);
	joystick_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_joystick_board_info(struct board_info *bi)
{
	memcpy(bi, &joystick_board_info, sizeof(struct board_info));
}

__setup("joystickboard=", tegra_joystick_board_info);

static int __init tegra_button_board_info(char *info)
{
	char *p = info;
	button_board_info.board_id = memparse(p, &p);
	button_board_info.sku = memparse(p+1, &p);
	button_board_info.fab = memparse(p+1, &p);
	button_board_info.major_revision = memparse(p+1, &p);
	button_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_button_board_info(struct board_info *bi)
{
	memcpy(bi, &button_board_info, sizeof(struct board_info));
}

__setup("buttonboard=", tegra_button_board_info);

static int __init tegra_io_board_info(char *info)
{
	char *p = info;
	io_board_info.board_id = memparse(p, &p);
	io_board_info.sku = memparse(p+1, &p);
	io_board_info.fab = memparse(p+1, &p);
	io_board_info.major_revision = memparse(p+1, &p);
	io_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_io_board_info(struct board_info *bi)
{
	memcpy(bi, &io_board_info, sizeof(struct board_info));
}

__setup("ioboard=", tegra_io_board_info);

static int __init tegra_modem_id(char *id)
{
	char *p = id;

	modem_id = memparse(p, &p);
	return 1;
}

int tegra_get_modem_id(void)
{
	return modem_id;
}

__setup("modem_id=", tegra_modem_id);

static int __init tegra_usb_port_owner_info(char *id)
{
	char *p = id;

	usb_port_owner_info = memparse(p, &p);
	return 1;
}

int tegra_get_usb_port_owner_info(void)
{
	return usb_port_owner_info;
}

__setup("usb_port_owner_info=", tegra_usb_port_owner_info);

static int __init tegra_lane_owner_info(char *id)
{
	char *p = id;

	lane_owner_info = memparse(p, &p);
	return 1;
}

int tegra_get_lane_owner_info(void)
{
	return lane_owner_info;
}

__setup("lane_owner_info=", tegra_lane_owner_info);

static int __init tegra_commchip_id(char *id)
{
	char *p = id;

	if (get_option(&p, &commchip_id) != 1)
		return 0;
	return 1;
}

int tegra_get_commchip_id(void)
{
	return commchip_id;
}

__setup("commchip_id=", tegra_commchip_id);

/*
 * Tegra has a protected aperture that prevents access by most non-CPU
 * memory masters to addresses above the aperture value.  Enabling it
 * secures the CPU's memory from the GPU, except through the GART.
 */
void __init tegra_protected_aperture_init(unsigned long aperture)
{
	void __iomem *mc_base = IO_ADDRESS(TEGRA_MC_BASE);
	pr_info("Enabling Tegra protected aperture at 0x%08lx\n", aperture);
	writel(aperture, mc_base + MC_SECURITY_CFG2);
}

/*
 * Due to conflicting restrictions on the placement of the framebuffer,
 * the bootloader is likely to leave the framebuffer pointed at a location
 * in memory that is outside the grhost aperture.  This function will move
 * the framebuffer contents from a physical address that is anywhere (lowmem,
 * highmem, or outside the memory map) to a physical address that is outside
 * the memory map.
 */
void __tegra_move_framebuffer(struct platform_device *pdev,
	phys_addr_t to, phys_addr_t from,
	size_t size)
{
	struct page *page;
	void __iomem *to_io;
	void *from_virt;
	unsigned long i;

	BUG_ON(PAGE_ALIGN((unsigned long)to) != (unsigned long)to);
	BUG_ON(PAGE_ALIGN(from) != from);
	BUG_ON(PAGE_ALIGN(size) != size);

	to_io = ioremap(to, size);
	if (!to_io) {
		pr_err("%s: Failed to map target framebuffer\n", __func__);
		return;
	}

	if (from && pfn_valid(page_to_pfn(phys_to_page(from)))) {
		for (i = 0 ; i < size; i += PAGE_SIZE) {
			page = phys_to_page(from + i);
			from_virt = kmap(page);
			memcpy(to_io + i, from_virt, PAGE_SIZE);
			kunmap(page);
		}
	} else if (from) {
		void __iomem *from_io = ioremap(from, size);
		if (!from_io) {
			pr_err("%s: Failed to map source framebuffer\n",
				__func__);
			goto out;
		}

		for (i = 0; i < size; i += 4)
			writel(readl(from_io + i), to_io + i);

		iounmap(from_io);
	}

out:
	iounmap(to_io);
}

void __tegra_clear_framebuffer(struct platform_device *pdev,
			       unsigned long to, unsigned long size)
{
	void __iomem *to_io;
	unsigned long i;

	BUG_ON(PAGE_ALIGN((unsigned long)to) != (unsigned long)to);
	BUG_ON(PAGE_ALIGN(size) != size);

	to_io = ioremap(to, size);
	if (!to_io) {
		pr_err("%s: Failed to map target framebuffer\n", __func__);
		return;
	}

	if (pfn_valid(page_to_pfn(phys_to_page(to)))) {
		for (i = 0 ; i < size; i += PAGE_SIZE)
			memset(to_io + i, 0, PAGE_SIZE);
	} else {
		for (i = 0; i < size; i += 4)
			writel(0, to_io + i);
	}

	iounmap(to_io);
}

void __init tegra_reserve(unsigned long carveout_size, unsigned long fb_size,
	unsigned long fb2_size)
{
	struct iommu_linear_map map[4];

#ifndef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	if (carveout_size) {
		/*
		 * Place the carveout below the 4 GB physical address limit
		 * because IOVAs are only 32 bit wide.
		 */
		BUG_ON(memblock_end_of_4G() == 0);
		tegra_carveout_start = memblock_end_of_4G() - carveout_size;
		if (memblock_remove(tegra_carveout_start, carveout_size)) {
			pr_err("Failed to remove carveout %08lx@%08llx "
				"from memory map\n",
				carveout_size, (u64)tegra_carveout_start);
			tegra_carveout_start = 0;
			tegra_carveout_size = 0;
		} else
			tegra_carveout_size = carveout_size;
	}
#endif

	if (fb2_size) {
		/*
		 * Place fb2 below the 4 GB physical address limit because
		 * IOVAs are only 32 bit wide.
		 */
		BUG_ON(memblock_end_of_4G() == 0);
		tegra_fb2_start = memblock_end_of_4G() - fb2_size;
		if (memblock_remove(tegra_fb2_start, fb2_size)) {
			pr_err("Failed to remove second framebuffer "
				"%08lx@%08llx from memory map\n",
				fb2_size, (u64)tegra_fb2_start);
			tegra_fb2_start = 0;
			tegra_fb2_size = 0;
		} else
			tegra_fb2_size = fb2_size;
	}

	if (fb_size) {
		/*
		 * Place fb below the 4 GB physical address limit because
		 * IOVAs are only 32 bit wide.
		 */
		BUG_ON(memblock_end_of_4G() == 0);
		tegra_fb_start = memblock_end_of_4G() - fb_size;
		if (memblock_remove(tegra_fb_start, fb_size)) {
			pr_err("Failed to remove framebuffer %08lx@%08llx "
				"from memory map\n",
				fb_size, (u64)tegra_fb_start);
			tegra_fb_start = 0;
			tegra_fb_size = 0;
		} else
			tegra_fb_size = fb_size;
	}

	if (tegra_cpu_is_asim()) {
		if (tegra_split_mem_active()) {
			tegra_fb_start = TEGRA_ASIM_QT_FB_START;
			tegra_fb_size = TEGRA_ASIM_QT_FB_SIZE;

			if (tegra_vpr_size == 0) {
				tegra_carveout_start =
				   TEGRA_ASIM_QT_CARVEOUT_VPR_DISABLED_START;
				tegra_carveout_size =
				   TEGRA_ASIM_QT_CARVEOUT_VPR_DISABLED_SIZE;
			} else if (
				    (tegra_vpr_start <
				     TEGRA_ASIM_QT_FB_START +
				     TEGRA_ASIM_QT_FB_SIZE) ||
				     (tegra_vpr_start + tegra_vpr_size - 1 >
				     TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
				     TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE - 1)) {
				/*
				 * On ASIM/ASIM + QT with
				 * CONFIG_TEGRA_SIMULATION_SPLIT_MEM enabled,
				 * the VPR region needs to be within the front
				 * door memory region. Moreover, the VPR region
				 * can't exist where the framebuffer resides.
				 */
				BUG();
			} else if (
					(tegra_vpr_start -
					 (TEGRA_ASIM_QT_FB_START +
					  TEGRA_ASIM_QT_FB_SIZE) <
					 TEGRA_ASIM_QT_CARVEOUT_MIN_SIZE) &&
					(TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
					 TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE -
					 (tegra_vpr_start + tegra_vpr_size) <
					 TEGRA_ASIM_QT_CARVEOUT_MIN_SIZE)) {
				/*
				 * The tegra ASIM/QT carveout has a min size:-
				 * TEGRA_ASIM_QT_CARVEOUT_MIN_SIZE. All free
				 * regions in front door mem are smaller than
				 * the min carveout size. Therefore, we can't
				 * fit the carveout in front door mem.
				 */
				BUG();
			} else if (
					(tegra_vpr_start -
					 (TEGRA_ASIM_QT_FB_START +
					  TEGRA_ASIM_QT_FB_SIZE)) >=
					(TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
					 TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE -
					 (tegra_vpr_start + tegra_vpr_size))) {
				/*
				 * Place the tegra ASIM/QT carveout between the
				 * framebuffer and VPR.
				 */
				tegra_carveout_start =
				  TEGRA_ASIM_QT_CARVEOUT_VPR_DISABLED_START;
				tegra_carveout_size = tegra_vpr_start -
					(TEGRA_ASIM_QT_FB_START +
					 TEGRA_ASIM_QT_FB_SIZE);
			} else {
				/*
				 * Place the tegra ASIM/QT carveout after VPR.
				 */
				tegra_carveout_start = tegra_vpr_start +
							 tegra_vpr_size;
				tegra_carveout_size =
					TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
					TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE -
					(tegra_vpr_start + tegra_vpr_size);
			}
		} else if (tegra_vpr_size != 0) {
			/*
			 * VPR cannot work on ASIM/ASIM + QT if split mem is not
			 * enabled.
			 */
			BUG();
		}
	}

	if (tegra_fb_size)
		tegra_grhost_aperture = tegra_fb_start;

	if (tegra_fb2_size && tegra_fb2_start < tegra_grhost_aperture)
		tegra_grhost_aperture = tegra_fb2_start;

	if (tegra_carveout_size && tegra_carveout_start < tegra_grhost_aperture)
		tegra_grhost_aperture = tegra_carveout_start;

	if (tegra_lp0_vec_size &&
	   (tegra_lp0_vec_start < memblock_end_of_DRAM())) {
		if (memblock_reserve(tegra_lp0_vec_start, tegra_lp0_vec_size)) {
			pr_err("Failed to reserve lp0_vec %08llx@%08llx\n",
				(u64)tegra_lp0_vec_size,
				(u64)tegra_lp0_vec_start);
			tegra_lp0_vec_start = 0;
			tegra_lp0_vec_size = 0;
		}
		tegra_lp0_vec_relocate = false;
	} else
		tegra_lp0_vec_relocate = true;

#ifdef CONFIG_TEGRA_NVDUMPER
	if (nvdumper_reserved) {
		if (memblock_reserve(nvdumper_reserved, NVDUMPER_RESERVED_SIZE)) {
			pr_err("Failed to reserve nvdumper page %08lx@%08lx\n",
			       nvdumper_reserved, NVDUMPER_RESERVED_SIZE);
			nvdumper_reserved = 0;
		}
	}
#endif

#ifdef CONFIG_TEGRA_USE_NCT
	if (tegra_nck_size &&
	   (tegra_nck_start < memblock_end_of_DRAM())) {
		if (memblock_reserve(tegra_nck_start, tegra_nck_size)) {
			pr_err("Failed to reserve nck %08lx@%08lx\n",
				tegra_nck_size, tegra_nck_start);
			tegra_nck_start = 0;
			tegra_nck_size = 0;
		}
	}
#endif

	/*
	 * We copy the bootloader's framebuffer to the framebuffer allocated
	 * above, and then free this one.
	 * */
	if (tegra_bootloader_fb_size) {
		tegra_bootloader_fb_size = PAGE_ALIGN(tegra_bootloader_fb_size);
		if (memblock_reserve(tegra_bootloader_fb_start,
				tegra_bootloader_fb_size)) {
			pr_err("Failed to reserve bootloader frame buffer "
				"%08llx@%08llx\n",
				(u64)tegra_bootloader_fb_size,
				(u64)tegra_bootloader_fb_start);
			tegra_bootloader_fb_start = 0;
			tegra_bootloader_fb_size = 0;
		}
	}

	if (tegra_bootloader_fb2_size) {
		tegra_bootloader_fb2_size =
				PAGE_ALIGN(tegra_bootloader_fb2_size);
		if (memblock_reserve(tegra_bootloader_fb2_start,
				tegra_bootloader_fb2_size)) {
			pr_err("Failed to reserve bootloader fb2 %08llx@%08llx\n",
				(u64)tegra_bootloader_fb2_size,
				(u64)tegra_bootloader_fb2_start);
			tegra_bootloader_fb2_start = 0;
			tegra_bootloader_fb2_size = 0;
		}
	}

	pr_info("Tegra reserved memory:\n"
		"LP0:                    %08llx - %08llx\n"
		"Bootloader framebuffer: %08llx - %08llx\n"
		"Bootloader framebuffer2: %08llx - %08llx\n"
		"Framebuffer:            %08llx - %08llx\n"
		"2nd Framebuffer:        %08llx - %08llx\n"
#ifndef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
		"Carveout:               %08llx - %08llx\n"
		"Vpr:                    %08llx - %08llx\n"
#endif
		"Tsec:                   %08llx - %08llx\n",
		(u64)tegra_lp0_vec_start,
		(u64)(tegra_lp0_vec_size ?
			tegra_lp0_vec_start + tegra_lp0_vec_size - 1 : 0),
		(u64)tegra_bootloader_fb_start,
		(u64)(tegra_bootloader_fb_size ?
			tegra_bootloader_fb_start +
			tegra_bootloader_fb_size - 1 : 0),
		(u64)tegra_bootloader_fb2_start,
		(u64)(tegra_bootloader_fb2_size ?
			tegra_bootloader_fb2_start +
			tegra_bootloader_fb2_size - 1 : 0),
		(u64)tegra_fb_start,
		(u64)(tegra_fb_size ?
			tegra_fb_start + tegra_fb_size - 1 : 0),
		(u64)tegra_fb2_start,
		(u64)(tegra_fb2_size ?
			tegra_fb2_start + tegra_fb2_size - 1 : 0),
#ifndef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
		(u64)tegra_carveout_start,
		(u64)(tegra_carveout_size ?
			tegra_carveout_start + tegra_carveout_size - 1 : 0),
		(u64)tegra_vpr_start,
		(u64)(tegra_vpr_size ?
			tegra_vpr_start + tegra_vpr_size - 1 : 0),
#endif
		(u64)tegra_tsec_start,
		(u64)(tegra_tsec_size ?
			tegra_tsec_start + tegra_tsec_size - 1 : 0));


#ifdef CONFIG_TEGRA_NVDUMPER
	if (nvdumper_reserved) {
		pr_info("Nvdumper:               %08lx - %08lx\n",
			nvdumper_reserved,
			nvdumper_reserved + NVDUMPER_RESERVED_SIZE - 1);
	}
#endif

#ifdef CONFIG_TEGRA_USE_SECURE_KERNEL
	pr_info("Tzram:               %08lx - %08lx\n",
		tegra_tzram_start,
		tegra_tzram_size ?
			tegra_tzram_start + tegra_tzram_size - 1 : 0);
#endif

#ifdef CONFIG_TEGRA_USE_NCT
	if (tegra_nck_size) {
		pr_info("Nck:                    %08lx - %08lx\n",
			tegra_nck_start,
			tegra_nck_size ?
				tegra_nck_start + tegra_nck_size - 1 : 0);
	}
#endif

#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	/* Keep these at the end */
	if (carveout_size) {
		if (dma_declare_contiguous(&tegra_generic_cma_dev,
			carveout_size, 0, memblock_end_of_4G()))
			pr_err("dma_declare_contiguous failed for generic\n");
		tegra_carveout_size = carveout_size;
	}

	if (tegra_vpr_size)
		if (dma_declare_contiguous(&tegra_vpr_cma_dev,
			tegra_vpr_size, 0, memblock_end_of_4G()))
			pr_err("dma_declare_contiguous failed VPR carveout\n");
#endif

	tegra_fb_linear_set(map);
}

void tegra_get_fb_resource(struct resource *fb_res)
{
	fb_res->start = (resource_size_t) tegra_fb_start;
	fb_res->end = fb_res->start +
			(resource_size_t) tegra_fb_size - 1;
}

void tegra_get_fb2_resource(struct resource *fb2_res)
{
	fb2_res->start = (resource_size_t) tegra_fb2_start;
	fb2_res->end = fb2_res->start +
			(resource_size_t) tegra_fb2_size - 1;
}

#ifdef CONFIG_PSTORE_RAM
static struct persistent_ram_descriptor desc = {
	.name = "ramoops",
};

static struct persistent_ram ram = {
	.descs = &desc,
	.num_descs = 1,
};

void __init tegra_ram_console_debug_reserve(unsigned long ram_console_size)
{
	int ret;

	ram.start = memblock_end_of_DRAM() - ram_console_size;
	ram.size = ram_console_size;
	ram.descs->size = ram_console_size;

	INIT_LIST_HEAD(&ram.node);

	ret = persistent_ram_early_init(&ram);
	if (ret)
		goto fail;

	return;

fail:
	pr_err("Failed to reserve memory block for ram console\n");
}
#endif

int __init tegra_register_fuse(void)
{
	return platform_device_register(&tegra_fuse_device);
}

int __init tegra_release_bootloader_fb(void)
{
	/* Since bootloader fb is reserved in common.c, it is freed here. */
	if (tegra_bootloader_fb_size) {
		if (memblock_free(tegra_bootloader_fb_start,
						tegra_bootloader_fb_size))
			pr_err("Failed to free bootloader fb.\n");
		else
			free_bootmem_late(tegra_bootloader_fb_start,
						tegra_bootloader_fb_size);
	}
	if (tegra_bootloader_fb2_size) {
		if (memblock_free(tegra_bootloader_fb2_start,
						tegra_bootloader_fb2_size))
			pr_err("Failed to free bootloader fb2.\n");
		else
			free_bootmem_late(tegra_bootloader_fb2_start,
						tegra_bootloader_fb2_size);
	}
	return 0;
}
late_initcall(tegra_release_bootloader_fb);

static struct platform_device *pinmux_devices[] = {
	&tegra_gpio_device,
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	&tegra114_pinctrl_device,
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
	&tegra124_pinctrl_device,
#else
	&tegra124_pinctrl_device,
#endif
};

void tegra_enable_pinmux(void)
{
	platform_add_devices(pinmux_devices, ARRAY_SIZE(pinmux_devices));
}

static const char *tegra_revision_name[TEGRA_REVISION_MAX] = {
	[TEGRA_REVISION_UNKNOWN] = "unknown",
	[TEGRA_REVISION_A01]     = "A01",
	[TEGRA_REVISION_A02]     = "A02",
	[TEGRA_REVISION_A03]     = "A03",
	[TEGRA_REVISION_A03p]    = "A03 prime",
	[TEGRA_REVISION_A04]     = "A04",
	[TEGRA_REVISION_A04p]    = "A04 prime",
};

static const char * __init tegra_get_revision(void)
{
	return kasprintf(GFP_KERNEL, "%s", tegra_revision_name[tegra_revision]);
}

static const char * __init tegra_get_family(void)
{
	void __iomem *chip_id = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804;
	u32 cid = readl(chip_id);
	cid = (cid >> 8) & 0xFF;

	switch (cid) {
	case TEGRA_CHIPID_TEGRA2:
		cid = 2;
		break;
	case TEGRA_CHIPID_TEGRA3:
		cid = 3;
		break;
	case TEGRA_CHIPID_TEGRA11:
		cid = 11;
		break;
	case TEGRA_CHIPID_TEGRA12:
		cid = 12;
		break;
	case TEGRA_CHIPID_TEGRA14:
		cid = 14;
		break;

	case TEGRA_CHIPID_UNKNOWN:
	default:
		cid = 0;
	}
	return kasprintf(GFP_KERNEL, "Tegra%d", cid);
}

static const char * __init tegra_get_soc_id(void)
{
	int package_id = tegra_package_id();

	return kasprintf(GFP_KERNEL, "REV=%s:SKU=0x%x:PID=0x%x",
		tegra_revision_name[tegra_revision],
		tegra_get_sku_id(), package_id);
}

static void __init tegra_soc_info_populate(struct soc_device_attribute
	*soc_dev_attr, const char *machine)
{
	soc_dev_attr->soc_id = tegra_get_soc_id();
	soc_dev_attr->machine  = machine;
	soc_dev_attr->family   = tegra_get_family();
	soc_dev_attr->revision = tegra_get_revision();
}

int __init tegra_soc_device_init(const char *machine)
{
	struct soc_device *soc_dev;
	struct soc_device_attribute *soc_dev_attr;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	tegra_soc_info_populate(soc_dev_attr, machine);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR_OR_NULL(soc_dev)) {
		kfree(soc_dev_attr);
		return -1;
	}

	return 0;
}

void __init tegra_init_late(void)
{
#ifndef CONFIG_COMMON_CLK
	tegra_clk_debugfs_init();
#endif
	tegra_powergate_debugfs_init();
}

#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
#define ASIM_SHUTDOWN_REG	0x538f0ffc

static void asim_power_off(void)
{
	pr_err("ASIM Powering off the device\n");
	writel(1, IO_ADDRESS(ASIM_SHUTDOWN_REG));
	while (1)
		;
}

static int __init asim_power_off_init(void)
{
	if (tegra_cpu_is_asim())
		pm_power_off = asim_power_off;
	return 0;
}

arch_initcall(asim_power_off_init);

#if defined(CONFIG_SMC91X)
static struct resource tegra_asim_smc91x_resources[] = {
	[0] = {
		.start		= TEGRA_SIM_ETH_BASE,
		.end		= TEGRA_SIM_ETH_BASE + TEGRA_SIM_ETH_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_ETH,
		.end		= IRQ_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_asim_smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tegra_asim_smc91x_resources),
	.resource	= tegra_asim_smc91x_resources,
};

static int __init asim_enet_smc91x_init(void)
{
	if (tegra_cpu_is_asim() && !tegra_cpu_is_dsim())
		platform_device_register(&tegra_asim_smc91x_device);
	return 0;
}

rootfs_initcall(asim_enet_smc91x_init);
#endif
#endif

#if defined(CONFIG_SMSC911X)
static struct resource tegra_smsc911x_resources[] = {
	[0] = {
		.start		= 0x4E000000,
		.end		= 0x4E000000 + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_ETH,
		.end		= IRQ_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct smsc911x_platform_config tegra_smsc911x_config = {
	.flags          = SMSC911X_USE_32BIT,
	.irq_polarity   = SMSC911X_IRQ_POLARITY_ACTIVE_HIGH,
	.irq_type       = SMSC911X_IRQ_TYPE_PUSH_PULL,
	.phy_interface  = PHY_INTERFACE_MODE_MII,
};

static struct platform_device tegra_smsc911x_device = {
	.name              = "smsc911x",
	.id                = 0,
	.resource          = tegra_smsc911x_resources,
	.num_resources     = ARRAY_SIZE(tegra_smsc911x_resources),
	.dev.platform_data = &tegra_smsc911x_config,
};

static int __init enet_smsc911x_init(void)
{
	if (!tegra_cpu_is_dsim())
		platform_device_register(&tegra_smsc911x_device);
	return 0;
}

rootfs_initcall(enet_smsc911x_init);
#endif

int tegra_split_mem_active(void)
{
	return tegra_split_mem_set;
}

static int __init set_tegra_split_mem(char *options)
{
	tegra_split_mem_set = 1;
	return 0;
}
early_param("tegra_split_mem", set_tegra_split_mem);

#define FUSE_SKU_INFO       0x110
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define STRAP_OPT 0x464
#else
#define STRAP_OPT 0x008
#endif
#define GMI_AD0 BIT(4)
#define GMI_AD1 BIT(5)
#define RAM_ID_MASK (GMI_AD0 | GMI_AD1)
#define RAM_CODE_SHIFT 4

#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
static enum tegra_platform tegra_platform;
static bool cpu_is_asim;
static bool cpu_is_dsim;
static const char *tegra_platform_name[TEGRA_PLATFORM_MAX] = {
	[TEGRA_PLATFORM_SILICON] = "silicon",
	[TEGRA_PLATFORM_QT]      = "quickturn",
	[TEGRA_PLATFORM_LINSIM]  = "linsim",
	[TEGRA_PLATFORM_FPGA]    = "fpga",
};
#endif

static u32 tegra_chip_sku_id;
static u32 tegra_chip_id;
static u32 tegra_chip_bct_strapping;
enum tegra_revision tegra_revision;

u32 tegra_read_pmc_reg(int offset)
{
	return readl(IO_ADDRESS(TEGRA_PMC_BASE) + offset);
}

u32 tegra_read_clk_ctrl_reg(int offset)
{
	return readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + offset);
}

u32 tegra_read_apb_misc_reg(int offset)
{
	return readl(IO_ADDRESS(TEGRA_APB_MISC_BASE) + offset);
}

u32 tegra_fuse_readl(unsigned long offset)
{
	return readl(IO_ADDRESS(TEGRA_FUSE_BASE + offset));
}

void tegra_fuse_writel(u32 val, unsigned long offset)
{
	writel(val, IO_ADDRESS(TEGRA_FUSE_BASE + offset));
}

u32 tegra_read_chipid(void)
{
	return readl_relaxed(IO_ADDRESS(TEGRA_APB_MISC_BASE)
			+ 0x804);
}

static void tegra_set_sku_id(void)
{
	u32 reg;

	reg = tegra_fuse_readl(FUSE_SKU_INFO);
	tegra_chip_sku_id = reg & 0xFF;
}

static void tegra_set_chip_id(void)
{
	u32 id;

	id = tegra_read_chipid();
	tegra_chip_id = (id >> 8) & 0xff;
}

static void tegra_set_bct_strapping(void)
{
	u32 reg;

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	reg = readl(IO_ADDRESS(TEGRA_PMC_BASE + STRAP_OPT));
#else
	reg = readl(IO_ADDRESS(TEGRA_APB_MISC_BASE + STRAP_OPT));
#endif
	tegra_chip_bct_strapping = (reg & RAM_ID_MASK) >> RAM_CODE_SHIFT;
}

u32 tegra_get_sku_id(void)
{
	return tegra_chip_sku_id;
}

u32 tegra_get_chip_id(void)
{
	return tegra_chip_id;
}

u32 tegra_get_bct_strapping(void)
{
	return tegra_chip_bct_strapping;
}

static void tegra_fuse_cfg_reg_visible(void)
{
	/* Make all fuse registers visible */
	u32 reg = readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE + 0x48));
	reg |= BIT(28);
	writel(reg, IO_ADDRESS(TEGRA_CLK_RESET_BASE + 0x48));
}

void tegra_init_fuse(void)
{
	u32 sku_id;

	tegra_fuse_cfg_reg_visible();
	tegra_set_sku_id();
	sku_id = tegra_get_sku_id();
	tegra_set_bct_strapping();
	tegra_set_chip_id();
	tegra_revision = tegra_chip_get_revision();
	tegra_init_speedo_data();
	pr_info("Tegra Revision: %s SKU: 0x%x CPU Process: %d Core Process: %d\n",
		tegra_revision_name[tegra_revision],
		sku_id, tegra_cpu_process_id(),
		tegra_core_process_id());
#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
	if (!tegra_platform_is_silicon()) {
		pr_info("Tegra Platform: %s%s%s\n",
			tegra_cpu_is_asim() ? "ASIM+" : "",
			tegra_cpu_is_dsim() ? "DSIM+" : "",
			tegra_platform_name[tegra_platform]);
	}
#endif
}

void __init display_tegra_dt_info(void)
{
	unsigned long dt_root;
	const char *dts_fname;


	dt_root = of_get_flat_dt_root();

	dts_fname = of_get_flat_dt_prop(dt_root, "nvidia,dtsfilename", NULL);
	if (dts_fname)
		pr_info("DTS File Name: %s\n", dts_fname);
	else
		pr_info("DTS File Name: <unknown>\n");
}

static int __init tegra_get_last_reset_reason(void)
{
#define PMC_RST_STATUS 0x1b4
#define RESET_STR(REASON) "last reset is due to "#REASON"\n"
	char *reset_reason[] = {
		RESET_STR(power on reset),
		RESET_STR(watchdog timeout),
		RESET_STR(sensor),
		RESET_STR(software reset),
		RESET_STR(deep sleep reset),
	};

	u32 val = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_RST_STATUS) & 0x7;
	if (val >= ARRAY_SIZE(reset_reason))
		pr_info("last reset value is invalid 0x%x\n", val);
	else
		pr_info("%s\n", reset_reason[val]);
	return 0;
}
late_initcall(tegra_get_last_reset_reason);
