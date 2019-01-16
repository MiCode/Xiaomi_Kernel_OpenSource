#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif
LCM_DRIVER otm8018b_dsi_vdo_txd_fwvga_lcm_drv = 
{
    .name           = "otm8018b_dsi_vdo_txd_fwvga",
};

