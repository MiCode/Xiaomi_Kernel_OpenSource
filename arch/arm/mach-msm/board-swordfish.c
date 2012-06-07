/* linux/arch/arm/mach-msm/board-swordfish.c
 *
 * Copyright (C) 2009 Google, Inc.
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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/android_pmem.h>
#include <linux/msm_kgsl.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>

#include <mach/board.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_ts.h>
#include <mach/proc_comm.h>
#include <linux/usb/android_composite.h>

#include "board-swordfish.h"
#include "devices.h"

extern int swordfish_init_mmc(void);

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0x70000300,
		.end	= 0x70000400,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= MSM_GPIO_TO_INT(156),
		.end	= MSM_GPIO_TO_INT(156),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

static int swordfish_phy_init_seq[] = {
	0x0C, 0x31,
	0x1D, 0x0D,
	0x1D, 0x10,
	-1
};

static void swordfish_usb_phy_reset(void)
{
	u32 id;
	int ret;

	id = PCOM_CLKRGM_APPS_RESET_USB_PHY;
	ret = msm_proc_comm(PCOM_CLK_REGIME_SEC_RESET_ASSERT, &id, NULL);
	if (ret) {
		pr_err("%s: Cannot assert (%d)\n", __func__, ret);
		return;
	}

	msleep(1);

	id = PCOM_CLKRGM_APPS_RESET_USB_PHY;
	ret = msm_proc_comm(PCOM_CLK_REGIME_SEC_RESET_DEASSERT, &id, NULL);
	if (ret) {
		pr_err("%s: Cannot assert (%d)\n", __func__, ret);
		return;
	}
}

static void swordfish_usb_hw_reset(bool enable)
{
	u32 id;
	int ret;
	u32 func;

	id = PCOM_CLKRGM_APPS_RESET_USBH;
	if (enable)
		func = PCOM_CLK_REGIME_SEC_RESET_ASSERT;
	else
		func = PCOM_CLK_REGIME_SEC_RESET_DEASSERT;
	ret = msm_proc_comm(func, &id, NULL);
	if (ret)
		pr_err("%s: Cannot set reset to %d (%d)\n", __func__, enable,
		       ret);
}


static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_init_seq		= swordfish_phy_init_seq,
	.phy_reset		= swordfish_usb_phy_reset,
	.hw_reset		= swordfish_usb_hw_reset,
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "Qualcomm",
	.product	= "Swordfish",
	.release	= 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

static struct resource msm_kgsl_resources[] = {
        {
                .name   = "kgsl_reg_memory",
                .start  = MSM_GPU_REG_PHYS,
                .end    = MSM_GPU_REG_PHYS + MSM_GPU_REG_SIZE - 1,
                .flags  = IORESOURCE_MEM,
        },
        {
                .name   = "kgsl_phys_memory",
                .start  = MSM_GPU_MEM_BASE,
                .end    = MSM_GPU_MEM_BASE + MSM_GPU_MEM_SIZE - 1,
                .flags  = IORESOURCE_MEM,
        },
        {
                .start  = INT_GRAPHICS,
                .end    = INT_GRAPHICS,
                .flags  = IORESOURCE_IRQ,
        },
};

static struct platform_device msm_kgsl_device = {
        .name           = "kgsl",
        .id             = -1,
        .resource       = msm_kgsl_resources,
        .num_resources  = ARRAY_SIZE(msm_kgsl_resources),
};

static struct android_pmem_platform_data mdp_pmem_pdata = {
        .name           = "pmem",
        .start          = MSM_PMEM_MDP_BASE,
        .size           = MSM_PMEM_MDP_SIZE,
        .no_allocator   = 0,
        .cached         = 1,
};

static struct android_pmem_platform_data android_pmem_gpu0_pdata = {
        .name           = "pmem_gpu0",
        .start          = MSM_PMEM_GPU0_BASE,
        .size           = MSM_PMEM_GPU0_SIZE,
        .no_allocator   = 0,
        .cached         = 0,
};

static struct android_pmem_platform_data android_pmem_gpu1_pdata = {
        .name           = "pmem_gpu1",
        .start          = MSM_PMEM_GPU1_BASE,
        .size           = MSM_PMEM_GPU1_SIZE,
        .no_allocator   = 0,
        .cached         = 0,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
        .name           = "pmem_adsp",
        .start          = MSM_PMEM_ADSP_BASE,
        .size           = MSM_PMEM_ADSP_SIZE,
        .no_allocator   = 0,
        .cached         = 0,
};

static struct platform_device android_pmem_mdp_device = {
        .name           = "android_pmem",
        .id             = 0,
        .dev            = {
                .platform_data = &mdp_pmem_pdata
        },
};

static struct platform_device android_pmem_adsp_device = {
        .name           = "android_pmem",
        .id             = 1,
        .dev            = {
                .platform_data = &android_pmem_adsp_pdata,
        },
};

static struct platform_device android_pmem_gpu0_device = {
        .name           = "android_pmem",
        .id             = 2,
        .dev            = {
                .platform_data = &android_pmem_gpu0_pdata,
        },
};

static struct platform_device android_pmem_gpu1_device = {
        .name           = "android_pmem",
        .id             = 3,
        .dev            = {
                .platform_data = &android_pmem_gpu1_pdata,
        },
};

static char *usb_functions[] = { "usb_mass_storage" };
static char *usb_functions_adb[] = { "usb_mass_storage", "adb" };

static struct android_usb_product usb_products[] = {
	{
		.product_id	= 0x0c01,
		.num_functions	= ARRAY_SIZE(usb_functions),
		.functions	= usb_functions,
	},
	{
		.product_id	= 0x0c02,
		.num_functions	= ARRAY_SIZE(usb_functions_adb),
		.functions	= usb_functions_adb,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id		= 0x18d1,
	.product_id		= 0x0d01,
	.version		= 0x0100,
	.serial_number		= "42",
	.product_name		= "Swordfishdroid",
	.manufacturer_name	= "Qualcomm",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_adb),
	.functions = usb_functions_adb,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};

static struct platform_device fish_battery_device = {
	.name = "fish_battery",
};

static struct msm_ts_platform_data swordfish_ts_pdata = {
	.min_x		= 296,
	.max_x		= 3800,
	.min_y		= 296,
	.max_y		= 3800,
	.min_press	= 0,
	.max_press	= 256,
	.inv_x		= 4096,
	.inv_y		= 4096,
};

static struct platform_device *devices[] __initdata = {
#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart3,
#endif
	&msm_device_smd,
	&msm_device_dmov,
	&msm_device_nand,
	&msm_device_hsusb,
	&usb_mass_storage_device,
	&android_usb_device,
	&fish_battery_device,
	&smc91x_device,
	&msm_device_touchscreen,
	&android_pmem_mdp_device,
	&android_pmem_adsp_device,
	&android_pmem_gpu0_device,
	&android_pmem_gpu1_device,
	&msm_kgsl_device,
};

extern struct sys_timer msm_timer;

static struct msm_acpu_clock_platform_data swordfish_clock_data = {
	.acpu_switch_time_us	= 20,
	.max_speed_delta_khz	= 256000,
	.vdd_switch_time_us	= 62,
	.power_collapse_khz	= 128000000,
	.wait_for_irq_khz	= 128000000,
};

void msm_serial_debug_init(unsigned int base, int irq,
			   struct device *clk_device, int signal_irq);

static void __init swordfish_init(void)
{
	int rc;

	msm_acpu_clock_init(&swordfish_clock_data);
#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	msm_serial_debug_init(MSM_UART3_PHYS, INT_UART3,
			      &msm_device_uart3.dev, 1);
#endif
	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	msm_device_touchscreen.dev.platform_data = &swordfish_ts_pdata;
	platform_add_devices(devices, ARRAY_SIZE(devices));
	msm_hsusb_set_vbus_state(1);
	rc = swordfish_init_mmc();
	if (rc)
		pr_crit("%s: MMC init failure (%d)\n", __func__, rc);
}

static void __init swordfish_fixup(struct machine_desc *desc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].node = PHYS_TO_NID(PHYS_OFFSET);
	mi->bank[0].size = (101*1024*1024);
}

static void __init swordfish_map_io(void)
{
	msm_map_qsd8x50_io();
	msm_clock_init(msm_clocks_8x50, msm_num_clocks_8x50);
}

MACHINE_START(SWORDFISH, "Swordfish Board (QCT SURF8250)")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.atag_offset	= 0x100,
	.fixup		= swordfish_fixup,
	.map_io		= swordfish_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= swordfish_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(QSD8X50_FFA, "qsd8x50 FFA Board (QCT FFA8250)")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io	= MSM_DEBUG_UART_PHYS,
	.io_pg_offst	= ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.atag_offset	= 0x100,
	.fixup		= swordfish_fixup,
	.map_io		= swordfish_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= swordfish_init,
	.timer		= &msm_timer,
MACHINE_END
