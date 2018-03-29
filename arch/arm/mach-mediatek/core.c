/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/pm.h>
#include <linux/bug.h>
#include <linux/memblock.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/page.h>
#include <linux/irqchip.h>
#include <linux/of_platform.h>

#if defined(CONFIG_ARCH_MT6580) || defined(CONFIG_ARCH_MT6570)
#include <mt-smp.h>
#endif

#ifdef CONFIG_OF

static const char *mt6570_dt_match[] __initconst = {
	"mediatek,MT6570",
	NULL
};

DT_MACHINE_START(MT6570_DT, "MT6570")
	.dt_compat      = mt6570_dt_match,
MACHINE_END

static const char *mt6580_dt_match[] __initconst = {
	"mediatek,MT6580",
	NULL
};

DT_MACHINE_START(MT6580_DT, "MT6580")
	.dt_compat	= mt6580_dt_match,
MACHINE_END

static const char *mt_dt_match[] __initconst = {
	"mediatek,mt6735",
	NULL
};

DT_MACHINE_START(MT6735_DT, "MT6735")
	.dt_compat	= mt_dt_match,
MACHINE_END

static const char *mt6755_dt_match[] __initconst = {
	"mediatek,MT6755",
	NULL
};
DT_MACHINE_START(MT6755_DT, "MT6755")
	.dt_compat	= mt6755_dt_match,
MACHINE_END

static const char *mt6757_dt_match[] __initconst = {
	"mediatek,mt6757",
	NULL
};

DT_MACHINE_START(MT6757_DT, "MT6757")
	.dt_compat	= mt6757_dt_match,
MACHINE_END

static const char *mt8127_dt_match[] __initconst = {
	"mediatek,mt8127",
	NULL
};

DT_MACHINE_START(MT8127_DT, "MT8127")
	.dt_compat	= mt8127_dt_match,
MACHINE_END

static const char *mt7623_dt_match[] __initconst = {
	"mediatek,mt7623",
	NULL
};

DT_MACHINE_START(MT7623_DT, "MT7623")
	.dt_compat	= mt7623_dt_match,
MACHINE_END
#endif
