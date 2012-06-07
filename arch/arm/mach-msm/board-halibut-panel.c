/* linux/arch/arm/mach-msm/board-halibut-mddi.c
** Author: Brian Swetland <swetland@google.com>
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/bootmem.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>

#include <mach/msm_fb.h>
#include <mach/vreg.h>
#include <mach/proc_comm.h>

#include "devices.h"
#include "board-halibut.h"

static void halibut_mddi_power_client(struct msm_mddi_client_data *mddi,
	int on)
{
}

static struct resource resources_msm_fb = {
	.start = MSM_FB_BASE,
	.end = MSM_FB_BASE + MSM_FB_SIZE - 1,
	.flags = IORESOURCE_MEM,
};

static struct msm_fb_data fb_data = {
	.xres = 800,
	.yres = 480,
	.output_format = 0,
};

static struct msm_mddi_platform_data mddi_pdata = {
	.clk_rate = 122880000,
	.power_client = halibut_mddi_power_client,
	.fb_resource = &resources_msm_fb,
	.num_clients = 1,
	.client_platform_data = {
		{
			.product_id = (0x4474 << 16 | 0xc065),
			.name = "mddi_c_dummy",
			.id = 0,
			.client_data = &fb_data,
			.clk_rate = 0,
		},
	},
};

int __init halibut_init_panel(void)
{
	int rc;

	if (!machine_is_halibut())
		return 0;

	rc = platform_device_register(&msm_device_mdp);
	if (rc)
		return rc;

	msm_device_mddi0.dev.platform_data = &mddi_pdata;
	return platform_device_register(&msm_device_mddi0);
}

device_initcall(halibut_init_panel);
