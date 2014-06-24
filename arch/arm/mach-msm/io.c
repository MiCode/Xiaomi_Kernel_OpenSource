/* arch/arm/mach-msm/io.c
 *
 * MSM7K, QSD io support
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>

#include <mach/hardware.h>
#include <asm/page.h>
#include <mach/msm_iomap.h>
#include <mach/memory.h>
#include <asm/mach/map.h>
#include <linux/dma-mapping.h>
#include <linux/of_fdt.h>

#include <mach/board.h>
#include "board-dt.h"

#define MSM_CHIP_DEVICE_TYPE(name, chip, mem_type) {			      \
		.virtual = (unsigned long) MSM_##name##_BASE, \
		.pfn = __phys_to_pfn(chip##_##name##_PHYS), \
		.length = chip##_##name##_SIZE, \
		.type = MT_DEVICE, \
	 }

#define MSM_DEVICE_TYPE(name, mem_type) \
		MSM_CHIP_DEVICE_TYPE(name, MSM, mem_type)
#define MSM_CHIP_DEVICE(name, chip) \
		MSM_CHIP_DEVICE_TYPE(name, chip, MT_DEVICE)
#define MSM_DEVICE(name) MSM_CHIP_DEVICE(name, MSM)

#ifdef CONFIG_ARCH_MSM8974
static struct map_desc msm_8974_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(TLMM, MSM8974),
	MSM_CHIP_DEVICE(MPM2_PSHOLD, MSM8974),
#ifdef CONFIG_DEBUG_MSM8974_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_8974_io(void)
{
	iotable_init(msm_8974_io_desc, ARRAY_SIZE(msm_8974_io_desc));
}
#endif /* CONFIG_ARCH_MSM8974 */

#ifdef CONFIG_ARCH_APQ8084
static struct map_desc msm_8084_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(MPM2_PSHOLD, APQ8084),
	MSM_CHIP_DEVICE(TLMM, APQ8084),
#ifdef CONFIG_DEBUG_APQ8084_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_8084_io(void)
{
	iotable_init(msm_8084_io_desc, ARRAY_SIZE(msm_8084_io_desc));
}
#endif /* CONFIG_ARCH_APQ8084 */

#ifdef CONFIG_ARCH_FSM9900
static struct map_desc fsm9900_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(TLMM, FSM9900),
	MSM_CHIP_DEVICE(MPM2_PSHOLD, FSM9900),
#ifdef CONFIG_DEBUG_FSM9900_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_fsm9900_io(void)
{
	iotable_init(fsm9900_io_desc, ARRAY_SIZE(fsm9900_io_desc));
}
#endif /* CONFIG_ARCH_FSM9900 */

#ifdef CONFIG_ARCH_MDM9630
static struct map_desc mdm9630_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(TLMM, MDM9630),
	MSM_CHIP_DEVICE(MPM2_PSHOLD, MDM9630),
};

void __init msm_map_mdm9630_io(void)
{
	iotable_init(mdm9630_io_desc, ARRAY_SIZE(mdm9630_io_desc));
}
#endif /* CONFIG_ARCH_MDM9630 */

#ifdef CONFIG_ARCH_MSMFERRUM
static struct map_desc msmferrum_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(APCS_GCC, MSMFERRUM),
#ifdef CONFIG_DEBUG_MSMFERRUM_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_msmferrum_io(void)
{
	iotable_init(msmferrum_io_desc, ARRAY_SIZE(msmferrum_io_desc));
}
#endif /* CONFIG_ARCH_MSMFERRUM */

#ifdef CONFIG_ARCH_MSM8916
static struct map_desc msm8916_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(APCS_GCC, MSM8916),
#ifdef CONFIG_DEBUG_MSM8916_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_msm8916_io(void)
{
	iotable_init(msm8916_io_desc, ARRAY_SIZE(msm8916_io_desc));
}
#endif /* CONFIG_ARCH_MSM8916 */

#ifdef CONFIG_ARCH_MSM8226
static struct map_desc msm_8226_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(APCS_GCC, MSM8226),
	MSM_CHIP_DEVICE(TLMM, MSM8226),
	MSM_CHIP_DEVICE(MPM2_PSHOLD, MSM8226),
#ifdef CONFIG_DEBUG_MSM8226_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};


void __init msm_map_msm8226_io(void)
{
	iotable_init(msm_8226_io_desc, ARRAY_SIZE(msm_8226_io_desc));
}
#endif /* CONFIG_ARCH_MSM8226 */

#ifdef CONFIG_ARCH_MSM8610
static struct map_desc msm8610_io_desc[] __initdata = {
	MSM_CHIP_DEVICE(APCS_GCC, MSM8610),
	MSM_CHIP_DEVICE(TLMM, MSM8610),
	MSM_CHIP_DEVICE(MPM2_PSHOLD, MSM8610),
};

void __init msm_map_msm8610_io(void)
{
	iotable_init(msm8610_io_desc, ARRAY_SIZE(msm8610_io_desc));
}
#endif /* CONFIG_ARCH_MSM8610 */

#ifdef CONFIG_ARCH_MSMZIRC
static struct map_desc msmzirc_io_desc[] __initdata = {
#ifdef CONFIG_DEBUG_MSMZIRC_UART
	MSM_DEVICE(DEBUG_UART),
#endif
};

void __init msm_map_msmzirc_io(void)
{
	iotable_init(msmzirc_io_desc, ARRAY_SIZE(msmzirc_io_desc));
}
#endif /* CONFIG_ARCH_MSMZIRC */
