/*
 * arch/arm/mach-tegra/tegra11_soctherm.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/seq_file.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>

#include <mach/tegra_fuse.h>
#include <mach/iomap.h>

#include "tegra3_tsensor.h"
#include "tegra11_soctherm.h"
#include "gpio-names.h"

/* Min temp granularity specified as X in 2^X.
 * -1: Hi precision option: 2^-1 = 0.5C
 *  0: Lo precision option: 2^0  = 1.0C
 *  NB: We must use lower precision (0) due to cp_fuse corrections
 *  (see Sec9.2 T35_Thermal_Sensing_IAS.docx)
 */
static const int precision; /* default 0 -> low precision */
#define LOWER_PRECISION_FOR_CONV(val)	((!precision) ? ((val)*2) : (val))
#define LOWER_PRECISION_FOR_TEMP(val)	((!precision) ? ((val)/2) : (val))
#define PRECISION_IS_LOWER()		((!precision))
#define PRECISION_TO_STR()		((!precision) ? "Lo" : "Hi")

#define TS_TSENSE_REGS_SIZE		0x20
#define TS_TSENSE_REG_OFFSET(reg, ts)	((reg) + ((ts) * TS_TSENSE_REGS_SIZE))

#define TS_THERM_LVL_REGS_SIZE		0x20
#define TS_THERM_GRP_REGS_SIZE		0x04
#define TS_THERM_REG_OFFSET(rg, lv, gr)	((rg) + ((lv) * TS_THERM_LVL_REGS_SIZE)\
					+ ((gr) * TS_THERM_GRP_REGS_SIZE))

#define CTL_LVL0_CPU0			0x0
#define CTL_LVL0_CPU0_UP_THRESH_SHIFT	17
#define CTL_LVL0_CPU0_UP_THRESH_MASK	0xff
#define CTL_LVL0_CPU0_DN_THRESH_SHIFT	9
#define CTL_LVL0_CPU0_DN_THRESH_MASK	0xff
#define CTL_LVL0_CPU0_EN_SHIFT		8
#define CTL_LVL0_CPU0_EN_MASK		0x1
#define CTL_LVL0_CPU0_DEV_THROT_LIGHT	0x1
#define CTL_LVL0_CPU0_DEV_THROT_HEAVY	0x2
#define CTL_LVL0_CPU0_CPU_THROT_SHIFT	5
#define CTL_LVL0_CPU0_CPU_THROT_MASK	0x3
#define CTL_LVL0_CPU0_GPU_THROT_SHIFT	3
#define CTL_LVL0_CPU0_GPU_THROT_MASK	0x3
#define CTL_LVL0_CPU0_MEM_THROT_SHIFT	2
#define CTL_LVL0_CPU0_MEM_THROT_MASK	0x1
#define CTL_LVL0_CPU0_STATUS_SHIFT	0
#define CTL_LVL0_CPU0_STATUS_MASK	0x3

#define THERMTRIP			0x80
#define THERMTRIP_ANY_EN_SHIFT		28
#define THERMTRIP_ANY_EN_MASK		0x1
#define THERMTRIP_MEM_EN_SHIFT		27
#define THERMTRIP_MEM_EN_MASK		0x1
#define THERMTRIP_GPU_EN_SHIFT		26
#define THERMTRIP_GPU_EN_MASK		0x1
#define THERMTRIP_CPU_EN_SHIFT		25
#define THERMTRIP_CPU_EN_MASK		0x1
#define THERMTRIP_TSENSE_EN_SHIFT	24
#define THERMTRIP_TSENSE_EN_MASK	0x1
#define THERMTRIP_GPUMEM_THRESH_SHIFT	16
#define THERMTRIP_GPUMEM_THRESH_MASK	0xff
#define THERMTRIP_CPU_THRESH_SHIFT	8
#define THERMTRIP_CPU_THRESH_MASK	0xff
#define THERMTRIP_TSENSE_THRESH_SHIFT	0
#define THERMTRIP_TSENSE_THRESH_MASK	0xff

#define TS_CPU0_CONFIG0				0xc0
#define TS_CPU0_CONFIG0_TALL_SHIFT		8
#define TS_CPU0_CONFIG0_TALL_MASK		0xfffff
#define TS_CPU0_CONFIG0_TCALC_OVER_SHIFT	4
#define TS_CPU0_CONFIG0_TCALC_OVER_MASK		0x1
#define TS_CPU0_CONFIG0_OVER_SHIFT		3
#define TS_CPU0_CONFIG0_OVER_MASK		0x1
#define TS_CPU0_CONFIG0_CPTR_OVER_SHIFT		2
#define TS_CPU0_CONFIG0_CPTR_OVER_MASK		0x1
#define TS_CPU0_CONFIG0_STOP_SHIFT		0
#define TS_CPU0_CONFIG0_STOP_MASK		0x1

#define TS_CPU0_CONFIG1			0xc4
#define TS_CPU0_CONFIG1_EN_SHIFT	31
#define TS_CPU0_CONFIG1_EN_MASK		0x1
#define TS_CPU0_CONFIG1_TIDDQ_SHIFT	15
#define TS_CPU0_CONFIG1_TIDDQ_MASK	0x3f
#define TS_CPU0_CONFIG1_TEN_COUNT_SHIFT	24
#define TS_CPU0_CONFIG1_TEN_COUNT_MASK	0x3f
#define TS_CPU0_CONFIG1_TSAMPLE_SHIFT	0
#define TS_CPU0_CONFIG1_TSAMPLE_MASK	0x3ff

#define TS_CPU0_CONFIG2			0xc8
#define TS_CPU0_CONFIG2_THERM_A_SHIFT	16
#define TS_CPU0_CONFIG2_THERM_A_MASK	0xffff
#define TS_CPU0_CONFIG2_THERM_B_SHIFT	0
#define TS_CPU0_CONFIG2_THERM_B_MASK	0xffff

#define TS_CPU0_STATUS0			0xcc
#define TS_CPU0_STATUS0_VALID_SHIFT	31
#define TS_CPU0_STATUS0_VALID_MASK	0x1
#define TS_CPU0_STATUS0_CAPTURE_SHIFT	0
#define TS_CPU0_STATUS0_CAPTURE_MASK	0xffff

#define TS_CPU0_STATUS1				0xd0
#define TS_CPU0_STATUS1_TEMP_VALID_SHIFT	31
#define TS_CPU0_STATUS1_TEMP_VALID_MASK		0x1
#define TS_CPU0_STATUS1_TEMP_SHIFT		0
#define TS_CPU0_STATUS1_TEMP_MASK		0xffff

#define TS_CPU0_STATUS2			0xd4

#define TS_PDIV				0x1c0
#define TS_PDIV_CPU_SHIFT		12
#define TS_PDIV_CPU_MASK		0xf
#define TS_PDIV_GPU_SHIFT		8
#define TS_PDIV_GPU_MASK		0xf
#define TS_PDIV_MEM_SHIFT		4
#define TS_PDIV_MEM_MASK		0xf
#define TS_PDIV_PLLX_SHIFT		0
#define TS_PDIV_PLLX_MASK		0xf

#define TS_HOTSPOT_OFF			0x1c4
#define TS_HOTSPOT_OFF_CPU_SHIFT	16
#define TS_HOTSPOT_OFF_CPU_MASK		0xff
#define TS_HOTSPOT_OFF_GPU_SHIFT	8
#define TS_HOTSPOT_OFF_GPU_MASK		0xff
#define TS_HOTSPOT_OFF_MEM_SHIFT	0
#define TS_HOTSPOT_OFF_MEM_MASK		0xff

#define TS_TEMP1			0x1c8
#define TS_TEMP1_CPU_TEMP_SHIFT		16
#define TS_TEMP1_CPU_TEMP_MASK		0xffff
#define TS_TEMP1_GPU_TEMP_SHIFT		0
#define TS_TEMP1_GPU_TEMP_MASK		0xffff

#define TS_TEMP2			0x1cc
#define TS_TEMP2_MEM_TEMP_SHIFT		16
#define TS_TEMP2_MEM_TEMP_MASK		0xffff
#define TS_TEMP2_PLLX_TEMP_SHIFT	0
#define TS_TEMP2_PLLX_TEMP_MASK		0xffff

#define TH_INTR_STATUS			0x84
#define TH_INTR_ENABLE			0x88
#define TH_INTR_DISABLE			0x8c

#define LOCK_CTL			0x90

#define TH_INTR_POS_MD3_SHIFT		31
#define TH_INTR_POS_MD3_MASK		0x1
#define TH_INTR_POS_MU3_SHIFT		30
#define TH_INTR_POS_MU3_MASK		0x1
#define TH_INTR_POS_MD2_SHIFT		29
#define TH_INTR_POS_MD2_MASK		0x1
#define TH_INTR_POS_MU2_SHIFT		28
#define TH_INTR_POS_MU2_MASK		0x1
#define TH_INTR_POS_MD1_SHIFT		27
#define TH_INTR_POS_MD1_MASK		0x1
#define TH_INTR_POS_MU1_SHIFT		26
#define TH_INTR_POS_MU1_MASK		0x1
#define TH_INTR_POS_MD0_SHIFT		25
#define TH_INTR_POS_MD0_MASK		0x1
#define TH_INTR_POS_MU0_SHIFT		24
#define TH_INTR_POS_MU0_MASK		0x1
#define TH_INTR_POS_GD3_SHIFT		23
#define TH_INTR_POS_GD3_MASK		0x1
#define TH_INTR_POS_GU3_SHIFT		22
#define TH_INTR_POS_GU3_MASK		0x1
#define TH_INTR_POS_GD2_SHIFT		21
#define TH_INTR_POS_GD2_MASK		0x1
#define TH_INTR_POS_GU2_SHIFT		20
#define TH_INTR_POS_GU2_MASK		0x1
#define TH_INTR_POS_GD1_SHIFT		19
#define TH_INTR_POS_GD1_MASK		0x1
#define TH_INTR_POS_GU1_SHIFT		18
#define TH_INTR_POS_GU1_MASK		0x1
#define TH_INTR_POS_GD0_SHIFT		17
#define TH_INTR_POS_GD0_MASK		0x1
#define TH_INTR_POS_GU0_SHIFT		16
#define TH_INTR_POS_GU0_MASK		0x1
#define TH_INTR_POS_CD3_SHIFT		15
#define TH_INTR_POS_CD3_MASK		0x1
#define TH_INTR_POS_CU3_SHIFT		14
#define TH_INTR_POS_CU3_MASK		0x1
#define TH_INTR_POS_CD2_SHIFT		13
#define TH_INTR_POS_CD2_MASK		0x1
#define TH_INTR_POS_CU2_SHIFT		12
#define TH_INTR_POS_CU2_MASK		0x1
#define TH_INTR_POS_CD1_SHIFT		11
#define TH_INTR_POS_CD1_MASK		0x1
#define TH_INTR_POS_CU1_SHIFT		10
#define TH_INTR_POS_CU1_MASK		0x1
#define TH_INTR_POS_CD0_SHIFT		9
#define TH_INTR_POS_CD0_MASK		0x1
#define TH_INTR_POS_CU0_SHIFT		8
#define TH_INTR_POS_CU0_MASK		0x1
#define TH_INTR_POS_PD3_SHIFT		7
#define TH_INTR_POS_PD3_MASK		0x1
#define TH_INTR_POS_PU3_SHIFT		6
#define TH_INTR_POS_PU3_MASK		0x1
#define TH_INTR_POS_PD2_SHIFT		5
#define TH_INTR_POS_PD2_MASK		0x1
#define TH_INTR_POS_PU2_SHIFT		4
#define TH_INTR_POS_PU2_MASK		0x1
#define TH_INTR_POS_PD1_SHIFT		3
#define TH_INTR_POS_PD1_MASK		0x1
#define TH_INTR_POS_PU1_SHIFT		2
#define TH_INTR_POS_PU1_MASK		0x1
#define TH_INTR_POS_PD0_SHIFT		1
#define TH_INTR_POS_PD0_MASK		0x1
#define TH_INTR_POS_PU0_SHIFT		0
#define TH_INTR_POS_PU0_MASK		0x1


#define UP_STATS_L0		0x10
#define DN_STATS_L0		0x14

#define STATS_CTL		0x94
#define STATS_CTL_CLR_DN	0x8
#define STATS_CTL_EN_DN		0x4
#define STATS_CTL_CLR_UP	0x2
#define STATS_CTL_EN_UP		0x1

#define THROT_GLOBAL_CFG	0x400

#define OC1_CFG				0x310
#define OC1_CFG_LONG_LATENCY_SHIFT	6
#define OC1_CFG_LONG_LATENCY_MASK	0x1
#define OC1_CFG_HW_RESTORE_SHIFT	5
#define OC1_CFG_HW_RESTORE_MASK		0x1
#define OC1_CFG_PWR_GOOD_MASK_SHIFT	4
#define OC1_CFG_PWR_GOOD_MASK_MASK	0x1
#define OC1_CFG_THROTTLE_MODE_SHIFT	2
#define OC1_CFG_THROTTLE_MODE_MASK	0x3
#define OC1_CFG_ALARM_POLARITY_SHIFT	1
#define OC1_CFG_ALARM_POLARITY_MASK	0x1
#define OC1_CFG_EN_THROTTLE_SHIFT	0
#define OC1_CFG_EN_THROTTLE_MASK	0x1

#define OC1_CNT_THRESHOLD		0x314
#define OC1_THRESHOLD_PERIOD		0x318
#define OC1_ALARM_COUNT			0x31c
#define OC1_FILTER			0x320

#define OC1_STATS			0x3a8

#define OC_INTR_STATUS			0x39c
#define OC_INTR_ENABLE			0x3a0
#define OC_INTR_DISABLE			0x3a4
#define OC_INTR_POS_OC1_SHIFT		0
#define OC_INTR_POS_OC1_MASK		0x1
#define OC_INTR_POS_OC2_SHIFT		1
#define OC_INTR_POS_OC2_MASK		0x1
#define OC_INTR_POS_OC3_SHIFT		2
#define OC_INTR_POS_OC3_MASK		0x1
#define OC_INTR_POS_OC4_SHIFT		3
#define OC_INTR_POS_OC4_MASK		0x1
#define OC_INTR_POS_OC5_SHIFT		4
#define OC_INTR_POS_OC5_MASK		0x1

#define OC_STATS_CTL			0x3c4
#define OC_STATS_CTL_CLR_ALL		0x2
#define OC_STATS_CTL_EN_ALL		0x1

#define CPU_PSKIP_STATUS			0x418
#define CPU_PSKIP_STATUS_M_SHIFT		12
#define CPU_PSKIP_STATUS_M_MASK			0xff
#define CPU_PSKIP_STATUS_N_SHIFT		4
#define CPU_PSKIP_STATUS_N_MASK			0xff
#define CPU_PSKIP_STATUS_ENABLED_SHIFT		0
#define CPU_PSKIP_STATUS_ENABLED_MASK		0x1

#define THROT_PRIORITY_LOCK			0x424
#define THROT_PRIORITY_LOCK_PRIORITY_SHIFT	0
#define THROT_PRIORITY_LOCK_PRIORITY_MASK	0xff

#define THROT_STATUS				0x428
#define THROT_STATUS_BREACH_SHIFT		12
#define THROT_STATUS_BREACH_MASK		0x1
#define THROT_STATUS_STATE_SHIFT		4
#define THROT_STATUS_STATE_MASK			0xff
#define THROT_STATUS_ENABLED_SHIFT		0
#define THROT_STATUS_ENABLED_MASK		0x1

#define THROT_PSKIP_CTRL_LITE_CPU		0x430
#define THROT_PSKIP_CTRL_ENABLE_SHIFT		31
#define THROT_PSKIP_CTRL_ENABLE_MASK		0x1
#define THROT_PSKIP_CTRL_DIVIDEND_SHIFT		8
#define THROT_PSKIP_CTRL_DIVIDEND_MASK		0xff
#define THROT_PSKIP_CTRL_DIVISOR_SHIFT		0
#define THROT_PSKIP_CTRL_DIVISOR_MASK		0xff

#define THROT_PSKIP_RAMP_LITE_CPU		0x434
#define THROT_PSKIP_RAMP_DURATION_SHIFT		8
#define THROT_PSKIP_RAMP_DURATION_MASK		0xffff
#define THROT_PSKIP_RAMP_STEP_SHIFT		0
#define THROT_PSKIP_RAMP_STEP_MASK		0xff

#define THROT_PRIORITY_LITE			0x444
#define THROT_PRIORITY_LITE_PRIO_SHIFT		0
#define THROT_PRIORITY_LITE_PRIO_MASK		0xff

#define THROT_DELAY_LITE			0x448
#define THROT_DELAY_LITE_DELAY_SHIFT		0
#define THROT_DELAY_LITE_DELAY_MASK		0xff

#define THROT_OFFSET				0x30
#define ALARM_OFFSET				0x14

#define FUSE_BASE_CP_SHIFT	0
#define FUSE_BASE_CP_MASK	0x3ff
#define FUSE_BASE_FT_SHIFT	16
#define FUSE_BASE_FT_MASK	0x7ff
#define FUSE_SHIFT_CP_SHIFT	10
#define FUSE_SHIFT_CP_MASK	0x3f
#define FUSE_SHIFT_CP_BITS	6
#define FUSE_SHIFT_FT_SHIFT	27
#define FUSE_SHIFT_FT_MASK	0x1f
#define FUSE_SHIFT_FT_BITS	5

#define FUSE_TSENSOR_CALIB_FT_SHIFT	13
#define FUSE_TSENSOR_CALIB_FT_MASK	0x1fff
#define FUSE_TSENSOR_CALIB_CP_SHIFT	0
#define FUSE_TSENSOR_CALIB_CP_MASK	0x1fff
#define FUSE_TSENSOR_CALIB_BITS		13

/* car register offsets needed for enabling HW throttling */
#define CAR_SUPER_CCLK_DIVIDER		0x24
#define CDIV_USE_THERM_CONTROLS_SHIFT	30
#define CDIV_USE_THERM_CONTROLS_MASK	0x1

/* pmc register offsets needed for powering off PMU */
#define PMC_SCRATCH_WRITE_SHIFT			2
#define PMC_SCRATCH_WRITE_MASK			0x1
#define PMC_ENABLE_RST_SHIFT			1
#define PMC_ENABLE_RST_MASK			0x1
#define PMC_SENSOR_CTRL				0x1B0
#define PMC_SCRATCH54				0x258
#define PMC_SCRATCH55				0x25C

/* scratch54 register bit fields */
#define PMU_OFF_DATA_SHIFT			8
#define PMU_OFF_DATA_MASK			0xff
#define PMU_OFF_ADDR_SHIFT			0
#define PMU_OFF_ADDR_MASK			0xff

/* scratch55 register bit fields */
#define RESET_TEGRA_SHIFT			31
#define RESET_TEGRA_MASK			0x1
#define CONTROLLER_TYPE_SHIFT			30
#define CONTROLLER_TYPE_MASK			0x1
#define I2C_CONTROLLER_ID_SHIFT			27
#define I2C_CONTROLLER_ID_MASK			0x7
#define PINMUX_SHIFT				24
#define PINMUX_MASK				0x7
#define CHECKSUM_SHIFT				16
#define CHECKSUM_MASK				0xff
#define PMU_16BIT_SUPPORT_SHIFT			15
#define PMU_16BIT_SUPPORT_MASK			0x1
#define PMU_I2C_ADDRESS_SHIFT			0
#define PMU_I2C_ADDRESS_MASK			0x7f


#define THROT_PSKIP_CTRL(throt, dev)		(THROT_PSKIP_CTRL_LITE_CPU + \
						(THROT_OFFSET * throt) + \
						(8 * dev))
#define THROT_PSKIP_RAMP(throt, dev)		(THROT_PSKIP_RAMP_LITE_CPU + \
						(THROT_OFFSET * throt) + \
						(8 * dev))
#define THROT_PRIORITY_CTRL(throt)		(THROT_PRIORITY_LITE + \
						(THROT_OFFSET * throt))
#define THROT_DELAY_CTRL(throt)			(THROT_DELAY_LITE + \
						(THROT_OFFSET * throt))
#define ALARM_CFG(throt)			(OC1_CFG + \
						(ALARM_OFFSET * (throt - \
								THROTTLE_OC1)))
#define ALARM_CNT_THRESHOLD(throt)		(OC1_CNT_THRESHOLD + \
						(ALARM_OFFSET * (throt - \
								THROTTLE_OC1)))
#define ALARM_THRESHOLD_PERIOD(throt)		(OC1_THRESHOLD_PERIOD + \
						(ALARM_OFFSET * (throt - \
								THROTTLE_OC1)))
#define ALARM_ALARM_COUNT(throt)		(OC1_ALARM_COUNT + \
						(ALARM_OFFSET * (throt - \
								THROTTLE_OC1)))
#define ALARM_FILTER(throt)			(OC1_FILTER + \
						(ALARM_OFFSET * (throt - \
								THROTTLE_OC1)))
#define ALARM_STATS(throt)			(OC1_STATS + \
						(4 * (throt - THROTTLE_OC1)))

#define THROT_DEPTH_DIVIDEND(depth)	((256 * (100 - (depth)) / 100) - 1)
#define THROT_DEPTH_DEFAULT		(80)
#define THROT_DEPTH(th, dp)		{			\
		(th)->depth    = (dp);				\
		(th)->dividend = THROT_DEPTH_DIVIDEND(dp);	\
		(th)->divisor  = 255;				\
		(th)->duration = 0xff;				\
		(th)->step     = 0xf;				\
	}

#define REG_SET(r, _name, val)	(((r) & ~(_name##_MASK << _name##_SHIFT)) | \
				 (((val) & _name##_MASK) << _name##_SHIFT))
#define REG_GET_BIT(r, _name)	((r) & (_name##_MASK << _name##_SHIFT))
#define REG_GET(r, _name)	(REG_GET_BIT(r, _name) >> _name##_SHIFT)
#define MAKE_SIGNED32(val, nb)	((s32)(val) << (32 - (nb)) >> (32 - (nb)))

static const void __iomem *reg_soctherm_base = IO_ADDRESS(TEGRA_SOCTHERM_BASE);
static const void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
static const void __iomem *clk_reset_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

#define clk_reset_writel(value, reg) \
	__raw_writel(value, (u32)clk_reset_base + (reg))
#define clk_reset_readl(reg) __raw_readl((u32)clk_reset_base + (reg))

#define pmc_writel(value, reg) __raw_writel(value, (u32)pmc_base + (reg))
#define pmc_readl(reg) __raw_readl((u32)pmc_base + (reg))

#define soctherm_writel(value, reg)	\
	(soctherm_suspended ?:		\
		__raw_writel(value, (u32)reg_soctherm_base + (reg)))
#define soctherm_readl(reg)		\
	(soctherm_suspended ? 0 :	\
		__raw_readl((u32)reg_soctherm_base + (reg)))

static DEFINE_MUTEX(soctherm_suspend_resume_lock);

static int soctherm_suspend(void);
static int soctherm_resume(void);

static struct soctherm_platform_data plat_data;

/*
 * Remove this flag once this "driver" is structured as a platform driver and
 * the board files calls platform_device_register instead of directly calling
 * tegra11_soctherm_init(). See nvbug 1206311.
 */
static bool soctherm_init_platform_done;
static bool read_hw_temp = true;
static bool soctherm_suspended;

static struct clk *soctherm_clk;
static struct clk *tsensor_clk;

static inline long temp_convert(int cap, int a, int b)
{
	cap *= a;
	cap >>= 10;
	cap += (b << 3);
	cap *= LOWER_PRECISION_FOR_CONV(500);
	cap /= 8;
	return cap;
}

#ifdef CONFIG_THERMAL
static struct thermal_zone_device *thz[THERM_SIZE];
#endif
struct soctherm_oc_irq_chip_data {
	int			irq_base;
	struct mutex		irq_lock;
	struct irq_chip		irq_chip;
	struct irq_domain	*domain;
	int			irq_enable;
};
static struct soctherm_oc_irq_chip_data soc_irq_cdata;

static u32 fuse_calib_base_cp;
static u32 fuse_calib_base_ft;
static s32 actual_temp_cp;
static s32 actual_temp_ft;

static const char *const therm_names[] = {
	[THERM_CPU] = "CPU",
	[THERM_MEM] = "MEM",
	[THERM_GPU] = "GPU",
	[THERM_PLL] = "PLL",
};

static const char *const throt_names[] = {
	[THROTTLE_LIGHT]   = "light",
	[THROTTLE_HEAVY]   = "heavy",
	[THROTTLE_OC1]     = "oc1",
	[THROTTLE_OC2]     = "oc2",
	[THROTTLE_OC3]     = "oc3",
	[THROTTLE_OC4]     = "oc4",
	[THROTTLE_OC5]     = "oc5",
};

static const char *const throt_dev_names[] = {
	[THROTTLE_DEV_CPU] = "CPU",
	[THROTTLE_DEV_GPU] = "GPU",
};

static const char *const sensor_names[] = {
	[TSENSE_CPU0] = "cpu0",
	[TSENSE_CPU1] = "cpu1",
	[TSENSE_CPU2] = "cpu2",
	[TSENSE_CPU3] = "cpu3",
	[TSENSE_MEM0] = "mem0",
	[TSENSE_MEM1] = "mem1",
	[TSENSE_GPU]  = "gpu0",
	[TSENSE_PLLX] = "pllx",
};

static const int sensor2tsensorcalib[] = {
	[TSENSE_CPU0] = 0,
	[TSENSE_CPU1] = 1,
	[TSENSE_CPU2] = 2,
	[TSENSE_CPU3] = 3,
	[TSENSE_MEM0] = 5,
	[TSENSE_MEM1] = 6,
	[TSENSE_GPU]  = 4,
	[TSENSE_PLLX] = 7,
};

static const int tsensor2therm_map[] = {
	[TSENSE_CPU0] = THERM_CPU,
	[TSENSE_CPU1] = THERM_CPU,
	[TSENSE_CPU2] = THERM_CPU,
	[TSENSE_CPU3] = THERM_CPU,
	[TSENSE_GPU]  = THERM_GPU,
	[TSENSE_MEM0] = THERM_MEM,
	[TSENSE_MEM1] = THERM_MEM,
	[TSENSE_PLLX] = THERM_PLL,
};

static const enum soctherm_throttle_dev_id therm2dev[] = {
	[THERM_CPU] = THROTTLE_DEV_CPU,
	[THERM_MEM] = THROTTLE_DEV_NONE,
	[THERM_GPU] = THROTTLE_DEV_GPU,
	[THERM_PLL] = THROTTLE_DEV_NONE,
};

static const struct soctherm_sensor sensor_defaults = {
	.tall      = 16300,
	.tiddq     = 1,
	.ten_count = 1,
	.tsample   = 163,
	.pdiv      = 10,
};

/* SOC- OCx to theirt GPIO which is wakeup capable. This is T114 specific */
static int soctherm_ocx_to_wake_gpio[TEGRA_SOC_OC_IRQ_MAX] = {
	TEGRA_GPIO_PEE3,	/* TEGRA_SOC_OC_IRQ_1 */
	TEGRA_GPIO_INVALID,	/* TEGRA_SOC_OC_IRQ_2 */
	TEGRA_GPIO_INVALID,	/* TEGRA_SOC_OC_IRQ_3 */
	TEGRA_GPIO_PJ2,		/* TEGRA_SOC_OC_IRQ_4 */
	TEGRA_GPIO_INVALID,	/* TEGRA_SOC_OC_IRQ_5 */
};

static const unsigned long base_soctherm_clk_rate = 136000000;
static const unsigned long default_soctherm_clk_rate = 51000000;
static const unsigned long default_tsensor_clk_rate = 500000;

static int sensor2therm_a[TSENSE_SIZE];
static int sensor2therm_b[TSENSE_SIZE];

static inline bool is_thermal_dev_throttle_enabled(
		const struct soctherm_platform_data *pdata,
		size_t throttle_id,
		enum soctherm_therm_id therm_id)
{
	size_t idx;

	if ((throttle_id >= ARRAY_SIZE(pdata->throttle)) ||
	    (therm_id >= ARRAY_SIZE(therm2dev)))
		return false;

	idx = therm2dev[therm_id];
	if (idx == THROTTLE_DEV_NONE)
		return false;

	return pdata->throttle[throttle_id].devs[idx].enable;
}

static inline s64 div64_s64_precise(s64 a, s32 b)
{
	s64 r, al;

	/* scale up for increased precision in division */
	al = a << 16;

	r = div64_s64((al * 2) + 1, 2 * b);
	return r >> 16;
}

static inline long temp_translate(int readback)
{
	int abs = readback >> 8;
	int lsb = (readback & 0x80) >> 7;
	int sign = readback & 0x01 ? -1 : 1;

	return (abs * LOWER_PRECISION_FOR_CONV(1000) +
		lsb * LOWER_PRECISION_FOR_CONV(500)) * sign;
}

#ifdef CONFIG_THERMAL
static inline void prog_hw_shutdown(struct thermal_trip_info *trip_state,
				    int therm)
{
	int trip_temp;
	u32 r;

	trip_temp = LOWER_PRECISION_FOR_TEMP(trip_state->trip_temp / 1000);


	r = soctherm_readl(THERMTRIP);
	if (therm == THERM_CPU) {
		r = REG_SET(r, THERMTRIP_CPU_EN, 1);
		r = REG_SET(r, THERMTRIP_CPU_THRESH, trip_temp);
	} else if (therm == THERM_GPU) {
		r = REG_SET(r, THERMTRIP_GPU_EN, 1);
		r = REG_SET(r, THERMTRIP_GPUMEM_THRESH, trip_temp);
	} else if (therm == THERM_PLL) {
		r = REG_SET(r, THERMTRIP_TSENSE_EN, 1);
		r = REG_SET(r, THERMTRIP_TSENSE_THRESH, trip_temp);
	} else if (therm == THERM_MEM) {
		r = REG_SET(r, THERMTRIP_MEM_EN, 1);
		r = REG_SET(r, THERMTRIP_GPUMEM_THRESH, trip_temp);
	}
	r = REG_SET(r, THERMTRIP_ANY_EN, 0);
	soctherm_writel(r, THERMTRIP);
}

static inline void prog_hw_threshold(struct thermal_trip_info *trip_state,
				     int therm, int throt)
{
	int trip_temp;
	u32 r, reg_off;

	trip_temp = LOWER_PRECISION_FOR_TEMP(trip_state->trip_temp / 1000);

	/* Hardcode LITE on level-1 and HEAVY on level-2 */
	reg_off = TS_THERM_REG_OFFSET(CTL_LVL0_CPU0, throt + 1, therm);

	r = soctherm_readl(reg_off);
	r = REG_SET(r, CTL_LVL0_CPU0_UP_THRESH, trip_temp);
	r = REG_SET(r, CTL_LVL0_CPU0_DN_THRESH, trip_temp);
	r = REG_SET(r, CTL_LVL0_CPU0_EN, 1);
	r = REG_SET(r, CTL_LVL0_CPU0_CPU_THROT,
		    throt == THROTTLE_HEAVY ?
		    CTL_LVL0_CPU0_DEV_THROT_HEAVY :
		    CTL_LVL0_CPU0_DEV_THROT_LIGHT);
	r = REG_SET(r, CTL_LVL0_CPU0_GPU_THROT,
		    throt == THROTTLE_HEAVY ?
		    CTL_LVL0_CPU0_DEV_THROT_HEAVY :
		    CTL_LVL0_CPU0_DEV_THROT_LIGHT);

	soctherm_writel(r, reg_off);
}

static void soctherm_set_limits(enum soctherm_therm_id therm,
				long lo_limit, long hi_limit)
{
	u32 r, reg_off;

	reg_off = TS_THERM_REG_OFFSET(CTL_LVL0_CPU0, 0, therm);
	r = soctherm_readl(reg_off);

	lo_limit = LOWER_PRECISION_FOR_TEMP(lo_limit);
	hi_limit = LOWER_PRECISION_FOR_TEMP(hi_limit);

	r = REG_SET(r, CTL_LVL0_CPU0_DN_THRESH, lo_limit);
	r = REG_SET(r, CTL_LVL0_CPU0_UP_THRESH, hi_limit);
	r = REG_SET(r, CTL_LVL0_CPU0_EN, 1);
	soctherm_writel(r, reg_off);

	switch (therm) {
	case THERM_CPU:
		r = REG_SET(0, TH_INTR_POS_CD0, 1);
		r = REG_SET(r, TH_INTR_POS_CU0, 1);
		break;
	case THERM_GPU:
		r = REG_SET(0, TH_INTR_POS_GD0, 1);
		r = REG_SET(r, TH_INTR_POS_GU0, 1);
		break;
	case THERM_MEM:
		r = REG_SET(0, TH_INTR_POS_PD0, 1);
		r = REG_SET(r, TH_INTR_POS_PU0, 1);
		break;
	case THERM_PLL:
		r = REG_SET(0, TH_INTR_POS_MD0, 1);
		r = REG_SET(r, TH_INTR_POS_MU0, 1);
		break;
	default:
		r = 0;
		break;
	}
	soctherm_writel(r, TH_INTR_ENABLE);
}

static void soctherm_update_zone(int zn)
{
	const int MAX_HIGH_TEMP = 128000;
	long low_temp = 0, high_temp = MAX_HIGH_TEMP;
	long trip_temp, passive_low_temp = MAX_HIGH_TEMP, zone_temp;
	enum thermal_trip_type trip_type;
	struct thermal_trip_info *trip_state;
	struct thermal_zone_device *cur_thz = thz[zn];
	int count, trips;

	thermal_zone_device_update(cur_thz);

	trips = cur_thz->trips;
	for (count = 0; count < trips; count++) {
		cur_thz->ops->get_trip_type(cur_thz, count, &trip_type);
		if ((trip_type == THERMAL_TRIP_HOT) ||
		    (trip_type == THERMAL_TRIP_CRITICAL))
			continue; /* handled in HW */

		cur_thz->ops->get_trip_temp(cur_thz, count, &trip_temp);

		trip_state = &plat_data.therm[zn].trips[count];
		zone_temp = cur_thz->temperature;

		if (!trip_state->tripped) { /* not tripped? update high */
			if (trip_temp < high_temp)
				high_temp = trip_temp;
		} else { /* tripped? update low */
			if (trip_type != THERMAL_TRIP_PASSIVE) {
				/* get highest ACTIVE */
				if (trip_temp > low_temp)
					low_temp = trip_temp;
			} else {
				/* get lowest PASSIVE */
				if (trip_temp < passive_low_temp)
					passive_low_temp = trip_temp;
			}
		}
	}

	if (passive_low_temp != MAX_HIGH_TEMP)
		low_temp = max(low_temp, passive_low_temp);

	soctherm_set_limits(zn, low_temp/1000, high_temp/1000);
}

static void soctherm_update(void)
{
	int i;

	if (!soctherm_init_platform_done)
		return;

	for (i = 0; i < THERM_SIZE; i++) {
		if (thz[i] && thz[i]->trips)
			soctherm_update_zone(i);
	}
}

static int soctherm_hw_action_get_max_state(struct thermal_cooling_device *cdev,
					    unsigned long *max_state)
{
	struct thermal_trip_info *trip_state = cdev->devdata;

	if (!trip_state)
		return 0;

	*max_state = 3; /* bit 1: CPU  bit 2: GPU */
	return 0;
}

static int soctherm_hw_action_get_cur_state(struct thermal_cooling_device *cdev,
					    unsigned long *cur_state)
{
	struct thermal_trip_info *trip_state = cdev->devdata;
	u32 r, m, n;
	int i, j;

	if (!trip_state)
		return 0;

	*cur_state = 0;
	if (trip_state->trip_type != THERMAL_TRIP_HOT)
		return 0;

	for (j = 0; j < THROTTLE_DEV_SIZE; j++) {
		r = soctherm_readl(CPU_PSKIP_STATUS + (j * 4));
		if (!REG_GET(r, CPU_PSKIP_STATUS_ENABLED))
			continue;

		m = REG_GET(r, CPU_PSKIP_STATUS_M);
		n = REG_GET(r, CPU_PSKIP_STATUS_N);
		for (i = THROTTLE_LIGHT; i <= THROTTLE_HEAVY; i++) {
			if (strnstr(trip_state->cdev_type,
					throt_names[i], THERMAL_NAME_LENGTH) &&
				plat_data.throttle[i].devs[j].enable &&
				m == plat_data.throttle[i].devs[j].dividend &&
				n == plat_data.throttle[i].devs[j].divisor)
				*cur_state |= (j + 1); /* bit1: CPU bit2: GPU */
		}
	}
	return 0;
}

static int soctherm_hw_action_set_cur_state(struct thermal_cooling_device *cdev,
					    unsigned long cur_state)
{
	return 0; /* hw sets this state */
}

static struct thermal_cooling_device_ops soctherm_hw_action_ops = {
	.get_max_state = soctherm_hw_action_get_max_state,
	.get_cur_state = soctherm_hw_action_get_cur_state,
	.set_cur_state = soctherm_hw_action_set_cur_state,
};

static int soctherm_suspend_get_max_state(struct thermal_cooling_device *cdev,
					  unsigned long *max_state)
{
	*max_state = 1;
	return 0;
}

static int soctherm_suspend_get_cur_state(struct thermal_cooling_device *cdev,
					  unsigned long *cur_state)
{
	*cur_state = !soctherm_suspended;
	return 0;
}

static int soctherm_suspend_set_cur_state(struct thermal_cooling_device *cdev,
					  unsigned long cur_state)
{
	if (!cur_state != soctherm_suspended) {
		if (cur_state)
			soctherm_resume();
		else
			soctherm_suspend();
	}
	return 0;
}

static struct thermal_cooling_device_ops soctherm_suspend_ops = {
	.get_max_state = soctherm_suspend_get_max_state,
	.get_cur_state = soctherm_suspend_get_cur_state,
	.set_cur_state = soctherm_suspend_set_cur_state,
};

static int soctherm_bind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i, index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;

	if (index < 0)
		return 0;

	for (i = 0; i < plat_data.therm[index].num_trips; i++) {
		trip_state = &plat_data.therm[index].trips[i];
		if (trip_state->bound)
			continue;
		if (trip_state->cdev_type &&
		    !strncmp(trip_state->cdev_type, cdev->type,
						THERMAL_NAME_LENGTH)) {
			thermal_zone_bind_cooling_device(thz, i, cdev,
							 trip_state->upper,
							 trip_state->lower);
			trip_state->bound = true;
		}
	}

	return 0;
}

static int soctherm_unbind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i, index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;

	if (index < 0)
		return 0;

	for (i = 0; i < plat_data.therm[index].num_trips; i++) {
		trip_state = &plat_data.therm[index].trips[i];
		if (!trip_state->bound)
			continue;
		if (trip_state->cdev_type &&
		    !strncmp(trip_state->cdev_type, cdev->type,
						THERMAL_NAME_LENGTH)) {
			thermal_zone_unbind_cooling_device(thz, 0, cdev);
			trip_state->bound = false;
		}
	}

	return 0;
}

static int soctherm_get_temp(struct thermal_zone_device *thz,
					unsigned long *temp)
{
	int index = (int)thz->devdata;
	u32 r, regv, shft, mask;
	enum soctherm_sense i, j;
	int tt, ti;

	if (index < TSENSE_SIZE) { /* 'TSENSE_XXX' thermal zone */
		regv = TS_CPU0_STATUS1;
		shft = TS_CPU0_STATUS1_TEMP_SHIFT;
		mask = TS_CPU0_STATUS1_TEMP_MASK;
		i = j = index;
	} else {
		index -= TSENSE_SIZE; /* 'THERM_XXX' thermal zone */

		switch (index) {
		case THERM_CPU:
			regv = TS_TEMP1;
			shft = TS_TEMP1_CPU_TEMP_SHIFT;
			mask = TS_TEMP1_CPU_TEMP_MASK;
			i = TSENSE_CPU0;
			j = TSENSE_CPU3;
			break;

		case THERM_GPU:
			regv = TS_TEMP1;
			shft = TS_TEMP1_GPU_TEMP_SHIFT;
			mask = TS_TEMP1_GPU_TEMP_MASK;
			i = j = TSENSE_GPU;
			break;

		case THERM_PLL:
			regv = TS_TEMP2;
			shft = TS_TEMP2_PLLX_TEMP_SHIFT;
			mask = TS_TEMP2_PLLX_TEMP_MASK;
			i = j = TSENSE_PLLX;
			break;

		case THERM_MEM:
			regv = TS_TEMP2;
			shft = TS_TEMP2_MEM_TEMP_SHIFT;
			mask = TS_TEMP2_MEM_TEMP_MASK;
			i = TSENSE_MEM0;
			j = TSENSE_MEM1;
			break;

		default:
			return 0; /* error really */
		}
	}

	if (read_hw_temp) {
		r = soctherm_readl(regv);
		*temp = temp_translate((r & (mask << shft)) >> shft);
	} else {
		for (tt = 0; i <= j; i++) {
			r = soctherm_readl(TS_TSENSE_REG_OFFSET(
						TS_CPU0_STATUS0, i));
			ti = temp_convert(REG_GET(r, TS_CPU0_STATUS0_CAPTURE),
						sensor2therm_a[i],
						sensor2therm_b[i]);
			*temp = tt = max(tt, ti);
		}
	}
	return 0;
}

static int soctherm_get_trip_type(struct thermal_zone_device *thz,
				int trip, enum thermal_trip_type *type)
{
	int index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;

	if (index < 0)
		return -EINVAL;

	trip_state = &plat_data.therm[index].trips[trip];
	*type = trip_state->trip_type;
	return 0;
}

static int soctherm_get_trip_temp(struct thermal_zone_device *thz,
				int trip, unsigned long *temp)
{
	int index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;
	unsigned long trip_temp, zone_temp;

	if (index < 0)
		return -EINVAL;

	trip_state = &plat_data.therm[index].trips[trip];
	trip_temp = trip_state->trip_temp;
	zone_temp = thz->temperature;

	if (zone_temp >= trip_temp) {
		trip_temp -= trip_state->hysteresis;
		trip_state->tripped = true;
	} else if (trip_state->tripped) {
		trip_temp -= trip_state->hysteresis;
		if (zone_temp < trip_temp)
			trip_state->tripped = false;
	}

	*temp = trip_temp;

	return 0;
}

static int soctherm_set_trip_temp(struct thermal_zone_device *thz,
				int trip, unsigned long temp)
{
	int index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;
	long rem;

	if (index < 0)
		return -EINVAL;

	trip_state = &plat_data.therm[index].trips[trip];

	trip_state->trip_temp = temp;

	rem = trip_state->trip_temp % LOWER_PRECISION_FOR_CONV(1000);
	if (rem) {
		pr_warn("soctherm: zone%d/trip_point%d %ld mC rounded down\n",
			index, trip, trip_state->trip_temp);
		trip_state->trip_temp -= rem;
	}

	if (trip_state->trip_type == THERMAL_TRIP_HOT) {
		if (strnstr(trip_state->cdev_type,
				"heavy", THERMAL_NAME_LENGTH))
			prog_hw_threshold(trip_state, index, THROTTLE_HEAVY);
		else if (strnstr(trip_state->cdev_type,
				"light", THERMAL_NAME_LENGTH))
			prog_hw_threshold(trip_state, index, THROTTLE_LIGHT);
	}

	/* Allow SW to shutdown at 'Critical temperature reached' */
	soctherm_update_zone(index);

	/* Reprogram HW thermtrip */
	if (trip_state->trip_type == THERMAL_TRIP_CRITICAL)
		prog_hw_shutdown(trip_state, index);

	return 0;
}

static int soctherm_get_crit_temp(struct thermal_zone_device *thz,
				  unsigned long *temp)
{
	int i, index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;

	if (index < 0)
		return -EINVAL;

	for (i = 0; i < plat_data.therm[index].num_trips; i++) {
		trip_state = &plat_data.therm[index].trips[i];
		if (trip_state->trip_type != THERMAL_TRIP_CRITICAL)
			continue;
		*temp = trip_state->trip_temp;
		return 0;
	}

	return -EINVAL;
}

static int soctherm_get_trend(struct thermal_zone_device *thz,
				int trip,
				enum thermal_trend *trend)
{
	int index = ((int)thz->devdata) - TSENSE_SIZE;
	struct thermal_trip_info *trip_state;
	long trip_temp;

	if (index < 0)
		return -EINVAL;

	trip_state = &plat_data.therm[index].trips[trip];
	thz->ops->get_trip_temp(thz, trip, &trip_temp);

	switch (trip_state->trip_type) {
	case THERMAL_TRIP_ACTIVE:
		/* aggressive active cooling */
		*trend = THERMAL_TREND_RAISING;
		break;
	case THERMAL_TRIP_PASSIVE:
		if (thz->temperature > trip_state->trip_temp)
			*trend = THERMAL_TREND_RAISING;
		else if (thz->temperature < trip_temp)
			*trend = THERMAL_TREND_DROPPING;
		else
			*trend = THERMAL_TREND_STABLE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct thermal_zone_device_ops soctherm_ops = {
	.bind = soctherm_bind,
	.unbind = soctherm_unbind,
	.get_temp = soctherm_get_temp,
	.get_trip_type = soctherm_get_trip_type,
	.get_trip_temp = soctherm_get_trip_temp,
	.set_trip_temp = soctherm_set_trip_temp,
	.get_crit_temp = soctherm_get_crit_temp,
	.get_trend = soctherm_get_trend,
};

static int __init soctherm_thermal_sys_init(void)
{
	char name[THERMAL_NAME_LENGTH];
	struct soctherm_therm *therm;
	bool oc_en = false;
	int i, j, k;

	if (!soctherm_init_platform_done)
		return 0;

	for (i = 0; i < TSENSE_SIZE; i++) {
		if (plat_data.sensor_data[i].zone_enable) {
			snprintf(name, THERMAL_NAME_LENGTH,
				 "%s-tsensor", sensor_names[i]);
			/* Create a thermal zone device for each sensor */
			thermal_zone_device_register(
					name,
					0,
					0,
					(void *)i,
					&soctherm_ops,
					NULL,
					0,
					0);
		}
	}

	for (i = 0; i < THERM_SIZE; i++) {
		therm = &plat_data.therm[i];
		if (!therm->zone_enable)
			continue;

		for (j = 0; j < therm->num_trips; j++) {
			switch (therm->trips[j].trip_type) {
			case THERMAL_TRIP_CRITICAL:
				thermal_cooling_device_register(
						therm->trips[j].cdev_type,
						&therm->trips[j],
						&soctherm_hw_action_ops);
				break;

			case THERMAL_TRIP_HOT:
				for (k = 0; k < THROTTLE_SIZE; k++) {
					if (!is_thermal_dev_throttle_enabled(
							&plat_data, k, i))
						continue;

					if ((strnstr(therm->trips[j].cdev_type,
						     "heavy",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_LIGHT) ||
					    (strnstr(therm->trips[j].cdev_type,
						     "light",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_HEAVY) ||
					    (strnstr(therm->trips[j].cdev_type,
						     "oc1",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_OC1) ||
					    (strnstr(therm->trips[j].cdev_type,
						     "oc2",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_OC2) ||
					    (strnstr(therm->trips[j].cdev_type,
						     "oc3",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_OC3) ||
					    (strnstr(therm->trips[j].cdev_type,
						     "oc4",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_OC4) ||
					    (strnstr(therm->trips[j].cdev_type,
						     "oc5",
						     THERMAL_NAME_LENGTH)
					     && k == THROTTLE_OC5))
						continue;

					thermal_cooling_device_register(
						therm->trips[j].cdev_type,
						&therm->trips[j],
						&soctherm_hw_action_ops);
				}
				break;

			case THERMAL_TRIP_PASSIVE:
			case THERMAL_TRIP_ACTIVE:
				break; /* done elsewhere */
			}
		}

		snprintf(name, THERMAL_NAME_LENGTH,
			 "%s-therm", therm_names[i]);
		thz[i] = thermal_zone_device_register(
					name,
					therm->num_trips,
					(1 << therm->num_trips) - 1,
					(void *)TSENSE_SIZE + i,
					&soctherm_ops,
					therm->tzp,
					therm->passive_delay,
					0);

		for (k = THROTTLE_OC1; !oc_en && k < THROTTLE_SIZE; k++)
			if (is_thermal_dev_throttle_enabled(&plat_data, k, i))
				oc_en = true;
	}

	/* do not enable suspend feature if any OC alarms are enabled */
	if (!oc_en)
		thermal_cooling_device_register("suspend_soctherm", 0,
						&soctherm_suspend_ops);
	else
		pr_warn("soctherm: Suspend feature CANNOT be enabled %s\n",
			"when any OC alarm is enabled");

	soctherm_update();
	return 0;
}
module_init(soctherm_thermal_sys_init);

#else
static void soctherm_update_zone(int zn)
{
}
static void soctherm_update(void)
{
}
#endif

static irqreturn_t soctherm_thermal_work_func(int irq, void *arg)
{
	u32 st, ex = 0, cp = 0, gp = 0;

	st = soctherm_readl(TH_INTR_STATUS);

	/* deliberately clear expected interrupts handled in SW */
	cp |= REG_GET_BIT(st, TH_INTR_POS_CD0);
	cp |= REG_GET_BIT(st, TH_INTR_POS_CU0);
	ex |= cp;

	gp |= REG_GET_BIT(st, TH_INTR_POS_GD0);
	gp |= REG_GET_BIT(st, TH_INTR_POS_GU0);
	ex |= gp;

	if (ex) {
		soctherm_writel(ex, TH_INTR_STATUS);
		st &= ~ex;
		if (cp)
			soctherm_update_zone(THERM_CPU);
		if (gp)
			soctherm_update_zone(THERM_GPU);

	}

	/* deliberately ignore expected interrupts NOT handled in SW */
	ex |= REG_GET_BIT(st, TH_INTR_POS_PD0);
	ex |= REG_GET_BIT(st, TH_INTR_POS_PU0);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MD0);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MU0);

	ex |= REG_GET_BIT(st, TH_INTR_POS_CD1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_CU1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_CD2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_CU2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_CD3);
	ex |= REG_GET_BIT(st, TH_INTR_POS_CU3);

	ex |= REG_GET_BIT(st, TH_INTR_POS_GD1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_GU1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_GD2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_GU2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_GD3);
	ex |= REG_GET_BIT(st, TH_INTR_POS_GU3);

	ex |= REG_GET_BIT(st, TH_INTR_POS_PD1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_PU1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_PD2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_PU2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_PD3);
	ex |= REG_GET_BIT(st, TH_INTR_POS_PU3);

	ex |= REG_GET_BIT(st, TH_INTR_POS_MD1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MU1);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MD2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MU2);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MD3);
	ex |= REG_GET_BIT(st, TH_INTR_POS_MU3);
	st &= ~ex;

	if (st) {
		/* Whine about any other unexpected INTR bits still set */
		pr_err("soctherm: Ignored unexpected INTRs 0x%08x\n", st);
		soctherm_writel(st, TH_INTR_STATUS);
	}

	return IRQ_HANDLED;
}

static inline void soctherm_oc_intr_enable(enum soctherm_throttle_id alarm,
					   bool enable)
{
	u32 r;

	if (!enable)
		return;

	switch (alarm) {
	case THROTTLE_OC1:
		r = REG_SET(0, OC_INTR_POS_OC1, 1);
		break;
	case THROTTLE_OC2:
		r = REG_SET(0, OC_INTR_POS_OC2, 1);
		break;
	case THROTTLE_OC3:
		r = REG_SET(0, OC_INTR_POS_OC3, 1);
		break;
	case THROTTLE_OC4:
		r = REG_SET(0, OC_INTR_POS_OC4, 1);
		break;
	case THROTTLE_OC5:
		r = REG_SET(0, OC_INTR_POS_OC5, 1);
		break;
	default:
		r = 0;
		break;
	}
	soctherm_writel(r, OC_INTR_ENABLE);
}

/* Return 0 (success) if you want to  reenable OC alarm intr. */
static int soctherm_handle_alarm(enum soctherm_throttle_id alarm)
{
	int rv = -EINVAL;

	switch (alarm) {
	case THROTTLE_OC1:
		pr_warn("soctherm: Unexpected OC1 alarm\n");
		/* add OC1 alarm handling code here */
		break;

	case THROTTLE_OC2:
		pr_info("soctherm: Successfully handled OC2 alarm\n");
		/* TODO: add OC2 alarm handling code here */
		rv = 0;
		break;

	case THROTTLE_OC3:
		pr_warn("soctherm: Unexpected OC3 alarm\n");
		/* add OC3 alarm handling code here */
		break;

	case THROTTLE_OC4:
		pr_debug("soctherm: Successfully handled OC4 alarm\n");
		/* TODO: add OC4 alarm handling code here */
		rv = 0;

		break;
	case THROTTLE_OC5:
		pr_warn("soctherm: Unexpected OC5 alarm\n");
		/* add OC5 alarm handling code here */
		break;

	default:
		break;
	}

	if (rv)
		pr_err("soctherm: ERROR in handling %s alarm\n",
			throt_names[alarm]);

	return rv;
}

static irqreturn_t soctherm_edp_work_func(int irq, void *arg_data)
{
	u32 st, ex, oc1, oc2, oc3, oc4, oc5;

	st = soctherm_readl(OC_INTR_STATUS);

	/* deliberately clear expected interrupts handled in SW */
	oc1 = REG_GET_BIT(st, OC_INTR_POS_OC1);
	oc2 = REG_GET_BIT(st, OC_INTR_POS_OC2);
	oc3 = REG_GET_BIT(st, OC_INTR_POS_OC3);
	oc4 = REG_GET_BIT(st, OC_INTR_POS_OC4);
	oc5 = REG_GET_BIT(st, OC_INTR_POS_OC5);
	ex = oc1 | oc2 | oc3 | oc4 | oc5;

	if (ex) {
		soctherm_writel(st, OC_INTR_STATUS);
		st &= ~ex;

		if (oc1 && !soctherm_handle_alarm(THROTTLE_OC1))
			soctherm_oc_intr_enable(THROTTLE_OC1, true);

		if (oc2 && !soctherm_handle_alarm(THROTTLE_OC2))
			soctherm_oc_intr_enable(THROTTLE_OC2, true);

		if (oc3 && !soctherm_handle_alarm(THROTTLE_OC3))
			soctherm_oc_intr_enable(THROTTLE_OC3, true);

		if (oc4 && !soctherm_handle_alarm(THROTTLE_OC4))
			soctherm_oc_intr_enable(THROTTLE_OC4, true);

		if (oc5 && !soctherm_handle_alarm(THROTTLE_OC5))
			soctherm_oc_intr_enable(THROTTLE_OC5, true);

		if (oc1 && soc_irq_cdata.irq_enable & BIT(0))
			handle_nested_irq(
				irq_find_mapping(soc_irq_cdata.domain, 0));

		if (oc2 && soc_irq_cdata.irq_enable & BIT(1))
			handle_nested_irq(
				irq_find_mapping(soc_irq_cdata.domain, 1));

		if (oc3 && soc_irq_cdata.irq_enable & BIT(2))
			handle_nested_irq(
				irq_find_mapping(soc_irq_cdata.domain, 2));

		if (oc4 && soc_irq_cdata.irq_enable & BIT(3))
			handle_nested_irq(
				irq_find_mapping(soc_irq_cdata.domain, 3));

		if (oc5 && soc_irq_cdata.irq_enable & BIT(4))
			handle_nested_irq(
				irq_find_mapping(soc_irq_cdata.domain, 0));
	}

	if (st) {
		pr_err("soctherm: Ignored unexpected OC ALARM 0x%08x\n", st);
		soctherm_writel(st, OC_INTR_STATUS);
	}

	return IRQ_HANDLED;
}

static irqreturn_t soctherm_thermal_isr(int irq, void *arg)
{
	u32 r;

	r = soctherm_readl(TH_INTR_STATUS);
	soctherm_writel(r, TH_INTR_DISABLE);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t soctherm_edp_isr(int irq, void *arg_data)
{
	u32 r;

	r = soctherm_readl(OC_INTR_STATUS);
	soctherm_writel(r, OC_INTR_DISABLE);

	return IRQ_WAKE_THREAD;
}

static inline unsigned int soctherm_get_throttle_timing(unsigned int us)
{
	/*
	 * throttle period timing register is based on the soctherm device
	 * running at original designed frequency; we need scale it
	 * accordingly if we use new frequency.
	 */
	return (default_soctherm_clk_rate / 1000000) * us /
	       (base_soctherm_clk_rate / 1000000);
}

static void tegra11_soctherm_throttle_program(enum soctherm_throttle_id throt)
{
	u32 r;
	int i;
	bool throt_enable = false;
	struct soctherm_throttle_dev *dev;
	struct soctherm_throttle *data = &plat_data.throttle[throt];

	for (i = 0; i < THROTTLE_DEV_SIZE; i++) {
		dev = &data->devs[i];
		if (!dev->enable)
			continue;
		throt_enable = true;

		if (dev->depth) {
			THROT_DEPTH(dev, dev->depth);
		} else if (!dev->dividend || !dev->divisor || !dev->duration ||
			   !dev->step) {
			THROT_DEPTH(dev, THROT_DEPTH_DEFAULT);
		}

		r = soctherm_readl(THROT_PSKIP_CTRL(throt, i));
		r = REG_SET(r, THROT_PSKIP_CTRL_ENABLE, dev->enable);
		r = REG_SET(r, THROT_PSKIP_CTRL_DIVIDEND, dev->dividend);
		r = REG_SET(r, THROT_PSKIP_CTRL_DIVISOR, dev->divisor);
		soctherm_writel(r, THROT_PSKIP_CTRL(throt, i));

		r = soctherm_readl(THROT_PSKIP_RAMP(throt, i));
		r = REG_SET(r, THROT_PSKIP_RAMP_DURATION, dev->duration);
		r = REG_SET(r, THROT_PSKIP_RAMP_STEP, dev->step);
		soctherm_writel(r, THROT_PSKIP_RAMP(throt, i));
	}

	r = REG_SET(0, THROT_PRIORITY_LITE_PRIO, data->priority);
	soctherm_writel(r, THROT_PRIORITY_CTRL(throt));

	r = REG_SET(0, THROT_DELAY_LITE_DELAY, 0);
	soctherm_writel(r, THROT_DELAY_CTRL(throt));

	r = soctherm_readl(THROT_PRIORITY_LOCK);
	if (r < data->priority) {
		r = REG_SET(0, THROT_PRIORITY_LOCK_PRIORITY, data->priority);
		soctherm_writel(r, THROT_PRIORITY_LOCK);
	}

	if (!throt_enable || (throt < THROTTLE_OC1))
		return;

	/* ----- configure OC alarms ----- */

	if (!(data->throt_mode == BRIEF || data->throt_mode == STICKY))
		pr_warn("soctherm: Invalid throt_mode in %s\n",
			throt_names[throt]);

	r = soctherm_readl(ALARM_CFG(throt));
	r = REG_SET(r, OC1_CFG_HW_RESTORE, 1);
	r = REG_SET(r, OC1_CFG_THROTTLE_MODE, data->throt_mode);
	r = REG_SET(r, OC1_CFG_ALARM_POLARITY, data->polarity);
	r = REG_SET(r, OC1_CFG_EN_THROTTLE, 1);
	soctherm_writel(r, ALARM_CFG(throt));

	soctherm_oc_intr_enable(throt, data->intr);

	soctherm_writel(soctherm_get_throttle_timing(data->period),
			ALARM_THRESHOLD_PERIOD(throt));
	soctherm_writel(0xffffffff, ALARM_FILTER(throt));
}

static void soctherm_tsense_program(enum soctherm_sense sensor,
						struct soctherm_sensor *data)
{
	u32 r;

	r = REG_SET(0, TS_CPU0_CONFIG0_TALL, data->tall);
	soctherm_writel(r, TS_TSENSE_REG_OFFSET(TS_CPU0_CONFIG0, sensor));

	r = REG_SET(0, TS_CPU0_CONFIG1_TIDDQ, data->tiddq);
	r = REG_SET(r, TS_CPU0_CONFIG1_EN, 1);
	r = REG_SET(r, TS_CPU0_CONFIG1_TEN_COUNT, data->ten_count);
	r = REG_SET(r, TS_CPU0_CONFIG1_TSAMPLE, data->tsample - 1);
	soctherm_writel(r, TS_TSENSE_REG_OFFSET(TS_CPU0_CONFIG1, sensor));
}

static int __init soctherm_clk_init(void)
{
	soctherm_clk = clk_get_sys("soc_therm", NULL);
	tsensor_clk = clk_get_sys("tegra-tsensor", NULL);

	if (IS_ERR(tsensor_clk) || IS_ERR(soctherm_clk)) {
		clk_put(soctherm_clk);
		clk_put(tsensor_clk);
		soctherm_clk = tsensor_clk = NULL;
		return -EINVAL;
	}

	/* initialize default clock rates */
	plat_data.soctherm_clk_rate =
		plat_data.soctherm_clk_rate ?: default_soctherm_clk_rate;
	plat_data.tsensor_clk_rate =
		plat_data.tsensor_clk_rate ?: default_tsensor_clk_rate;

	if (clk_get_rate(soctherm_clk) != plat_data.soctherm_clk_rate)
		if (clk_set_rate(soctherm_clk, plat_data.soctherm_clk_rate))
			return -EINVAL;

	if (clk_get_rate(tsensor_clk) != plat_data.tsensor_clk_rate)
		if (clk_set_rate(tsensor_clk, plat_data.tsensor_clk_rate))
			return -EINVAL;

	return 0;
}

static int soctherm_clk_enable(bool enable)
{
	if (soctherm_clk == NULL || tsensor_clk == NULL)
		return -EINVAL;

	if (enable) {
		clk_enable(soctherm_clk);
		clk_enable(tsensor_clk);
	} else {
		clk_disable(soctherm_clk);
		clk_disable(tsensor_clk);
	}

	return 0;
}

static void soctherm_fuse_read_vsensor(void)
{
	u32 value;
	s32 calib_cp, calib_ft;

	tegra_fuse_get_vsensor_calib(&value);

	/* Extract bits */
	fuse_calib_base_cp = REG_GET(value, FUSE_BASE_CP);
	fuse_calib_base_ft = REG_GET(value, FUSE_BASE_FT);

	/* Extract bits and convert to signed 2's complement */
	calib_cp = REG_GET(value, FUSE_SHIFT_CP);
	calib_cp = MAKE_SIGNED32(calib_cp, FUSE_SHIFT_CP_BITS);

	calib_ft = REG_GET(value, FUSE_SHIFT_FT);
	calib_ft = MAKE_SIGNED32(calib_ft, FUSE_SHIFT_FT_BITS);

	/* default: HI precision: use fuse_temp in 0.5C */
	actual_temp_cp = 2 * 25 + calib_cp;
	actual_temp_ft = 2 * 90 + calib_ft;

	/* adjust: for LO precision: use fuse_temp in 1C */
	actual_temp_cp = LOWER_PRECISION_FOR_TEMP(actual_temp_cp);
	actual_temp_ft = LOWER_PRECISION_FOR_TEMP(actual_temp_ft);
}

static int fuse_corr_alpha[] = { /* scaled *1000000 */
	[TSENSE_CPU0] = 1196400,
	[TSENSE_CPU1] = 1196400,
	[TSENSE_CPU2] = 1196400,
	[TSENSE_CPU3] = 1196400,
	[TSENSE_GPU]  = 1124500,
	[TSENSE_PLLX] = 1224200,
};

static int fuse_corr_beta[] = { /* scaled *1000000 */
	[TSENSE_CPU0] = -13600000,
	[TSENSE_CPU1] = -13600000,
	[TSENSE_CPU2] = -13600000,
	[TSENSE_CPU3] = -13600000,
	[TSENSE_GPU]  =  -9793100,
	[TSENSE_PLLX] = -14665000,
};

static void soctherm_fuse_read_tsensor(enum soctherm_sense sensor)
{
	u32 r, value;
	s32 calib, delta_sens, delta_temp;
	s16 therm_a, therm_b;
	s32 div, mult, actual_tsensor_ft, actual_tsensor_cp;

	tegra_fuse_get_tsensor_calib(sensor2tsensorcalib[sensor], &value);

	/* Extract bits and convert to signed 2's complement */
	calib = REG_GET(value, FUSE_TSENSOR_CALIB_FT);
	calib = MAKE_SIGNED32(calib, FUSE_TSENSOR_CALIB_BITS);
	actual_tsensor_ft = (fuse_calib_base_ft * 32) + calib;

	calib = REG_GET(value, FUSE_TSENSOR_CALIB_CP);
	calib = MAKE_SIGNED32(calib, FUSE_TSENSOR_CALIB_BITS);
	actual_tsensor_cp = (fuse_calib_base_cp * 64) + calib;

	mult = plat_data.sensor_data[sensor].pdiv * 655;
	div = plat_data.sensor_data[sensor].tsample * 10;

	delta_sens = actual_tsensor_ft - actual_tsensor_cp;
	delta_temp = actual_temp_ft - actual_temp_cp;

	therm_a = div64_s64_precise((s64)delta_temp * (1LL << 13) * mult,
				    (s64)delta_sens * div);

	therm_b = div64_s64_precise((((s64)actual_tsensor_ft * actual_temp_cp) -
				     ((s64)actual_tsensor_cp * actual_temp_ft)),
				    (s64)delta_sens);

	if (PRECISION_IS_LOWER()) {
		/* cp_fuse corrections */
		fuse_corr_alpha[sensor] = fuse_corr_alpha[sensor] ?: 1000000;
		therm_a = div64_s64_precise(
				(s64)therm_a * fuse_corr_alpha[sensor],
				(s64)1000000LL);
		therm_b = div64_s64_precise(
				(s64)therm_b * fuse_corr_alpha[sensor] +
				fuse_corr_beta[sensor], (s64)1000000LL);
	}

	sensor2therm_a[sensor] = (s16)therm_a;
	sensor2therm_b[sensor] = (s16)therm_b;

	r = REG_SET(0, TS_CPU0_CONFIG2_THERM_A, therm_a);
	r = REG_SET(r, TS_CPU0_CONFIG2_THERM_B, therm_b);
	soctherm_writel(r, TS_TSENSE_REG_OFFSET(TS_CPU0_CONFIG2, sensor));
}

static void soctherm_therm_trip_init(struct tegra_tsensor_pmu_data *data)
{
	u32 val, checksum;

	if (!data)
		return;

	val = pmc_readl(PMC_SENSOR_CTRL);
	val = REG_SET(val, PMC_SCRATCH_WRITE, 1);
	val = REG_SET(val, PMC_ENABLE_RST, 1);
	pmc_writel(val, PMC_SENSOR_CTRL);

	/* Fill scratch registers to shutdown device on therm TRIP */
	val = REG_SET(0, PMU_OFF_DATA, data->poweroff_reg_data);
	val = REG_SET(val, PMU_OFF_ADDR, data->poweroff_reg_addr);
	pmc_writel(val, PMC_SCRATCH54);

	val = REG_SET(0, RESET_TEGRA, 1);
	val = REG_SET(val, CONTROLLER_TYPE, data->controller_type);
	val = REG_SET(val, I2C_CONTROLLER_ID, data->i2c_controller_id);
	val = REG_SET(val, PINMUX, data->pinmux);
	val = REG_SET(val, PMU_16BIT_SUPPORT, data->pmu_16bit_ops);
	val = REG_SET(val, PMU_I2C_ADDRESS, data->pmu_i2c_addr);

	checksum = data->poweroff_reg_addr +
		data->poweroff_reg_data +
		(val & 0xFF) +
		((val >> 8) & 0xFF) +
		((val >> 24) & 0xFF);
	checksum &= 0xFF;
	checksum = 0x100 - checksum;

	val = REG_SET(val, CHECKSUM, checksum);
	pmc_writel(val, PMC_SCRATCH55);
}

static int soctherm_init_platform_data(void)
{
	struct soctherm_therm *therm;
	struct soctherm_sensor *s;
	int i, j, k;
	long rem;
	u32 r;

	/* initialize default values for unspecified params */
	for (i = 0; i < TSENSE_SIZE; i++) {
		therm = &plat_data.therm[tsensor2therm_map[i]];
		s = &plat_data.sensor_data[i];
		s->sensor_enable = s->zone_enable;
		s->sensor_enable = s->sensor_enable ?: therm->zone_enable;
		s->tall      = s->tall      ?: sensor_defaults.tall;
		s->tiddq     = s->tiddq     ?: sensor_defaults.tiddq;
		s->ten_count = s->ten_count ?: sensor_defaults.ten_count;
		s->tsample   = s->tsample   ?: sensor_defaults.tsample;
		s->pdiv      = s->pdiv      ?: sensor_defaults.pdiv;
	}

	/* Pdiv */
	r = soctherm_readl(TS_PDIV);
	r = REG_SET(r, TS_PDIV_CPU, plat_data.sensor_data[TSENSE_CPU0].pdiv);
	r = REG_SET(r, TS_PDIV_GPU, plat_data.sensor_data[TSENSE_GPU].pdiv);
	r = REG_SET(r, TS_PDIV_MEM, plat_data.sensor_data[TSENSE_MEM0].pdiv);
	r = REG_SET(r, TS_PDIV_PLLX, plat_data.sensor_data[TSENSE_PLLX].pdiv);
	soctherm_writel(r, TS_PDIV);

	/* Thermal Sensing programming */
	soctherm_fuse_read_vsensor();
	for (i = 0; i < TSENSE_SIZE; i++) {
		if (plat_data.sensor_data[i].sensor_enable) {
			soctherm_tsense_program(i, &plat_data.sensor_data[i]);
			soctherm_fuse_read_tsensor(i);
		}
	}

	/* Sanitize therm trips */
	for (i = 0; i < THERM_SIZE; i++) {
		therm = &plat_data.therm[i];
		if (!therm->zone_enable)
			continue;

		for (j = 0; j < therm->num_trips; j++) {
			rem = therm->trips[j].trip_temp %
				LOWER_PRECISION_FOR_CONV(1000);
			if (rem) {
				pr_warn(
			"soctherm: zone%d/trip_point%d %ld mC rounded down\n",
					i, j, therm->trips[j].trip_temp);
				therm->trips[j].trip_temp -= rem;
			}
		}
	}

	/* Program hotspot offsets per THERM */
	r = REG_SET(0, TS_HOTSPOT_OFF_CPU,
		    plat_data.therm[THERM_CPU].hotspot_offset / 1000);
	r = REG_SET(r, TS_HOTSPOT_OFF_GPU,
		    plat_data.therm[THERM_GPU].hotspot_offset / 1000);
	r = REG_SET(r, TS_HOTSPOT_OFF_MEM,
		    plat_data.therm[THERM_MEM].hotspot_offset / 1000);
	soctherm_writel(r, TS_HOTSPOT_OFF);

	/* Sanitize HW throttle priority */
	for (i = 0; i < THROTTLE_SIZE; i++)
		if (!plat_data.throttle[i].priority)
			plat_data.throttle[i].priority = 0xE + i;
	if (plat_data.throttle[THROTTLE_HEAVY].priority <
	    plat_data.throttle[THROTTLE_LIGHT].priority)
		pr_err("soctherm: ERROR: Priority of HEAVY less than LIGHT\n");

	/* Thermal HW throttle programming */
	for (i = 0; i < THROTTLE_SIZE; i++) {
		/* Setup PSKIP parameters */
		tegra11_soctherm_throttle_program(i);

		/* Setup throttle thresholds per THERM */
		for (j = 0; j < THERM_SIZE; j++) {
			/* Check if PSKIP params are enabled */
			if (!is_thermal_dev_throttle_enabled(&plat_data, i, j))
				continue;

			therm = &plat_data.therm[j];
			for (k = 0; k < therm->num_trips; k++)
				if ((therm->trips[k].trip_type ==
				     THERMAL_TRIP_HOT) &&
				    strnstr(therm->trips[k].cdev_type,
					    i == THROTTLE_HEAVY ? "heavy" :
					    "light", THERMAL_NAME_LENGTH))
					break;
			if (k < therm->num_trips && i <= THROTTLE_HEAVY)
				prog_hw_threshold(&therm->trips[k], j, i);
		}
	}

	/* initialize stats collection */
	r = STATS_CTL_CLR_DN | STATS_CTL_EN_DN |
		STATS_CTL_CLR_UP | STATS_CTL_EN_UP;
	soctherm_writel(r, STATS_CTL);
	soctherm_writel(OC_STATS_CTL_EN_ALL, OC_STATS_CTL);

	/* Enable PMC to shutdown */
	soctherm_therm_trip_init(plat_data.tshut_pmu_trip_data);

	r = clk_reset_readl(CAR_SUPER_CCLK_DIVIDER);
	r = REG_SET(r, CDIV_USE_THERM_CONTROLS, 1);
	clk_reset_writel(r, CAR_SUPER_CCLK_DIVIDER);

	/* Thermtrip */
	for (i = 0; i < THERM_SIZE; i++) {
		therm = &plat_data.therm[i];
		if (!therm->zone_enable)
			continue;

		for (j = 0; j < therm->num_trips; j++)
			if (therm->trips[j].trip_type == THERMAL_TRIP_CRITICAL)
				prog_hw_shutdown(&therm->trips[j], i);
	}

	return 0;
}

static void soctherm_suspend_locked(void)
{
	if (!soctherm_suspended) {
		soctherm_writel((u32)-1, TH_INTR_DISABLE);
		soctherm_writel((u32)-1, OC_INTR_DISABLE);
		disable_irq(INT_THERMAL);
		disable_irq(INT_EDP);
		soctherm_clk_enable(false);
		soctherm_init_platform_done = false;
		soctherm_suspended = true;
	}
}

static int soctherm_suspend(void)
{
	mutex_lock(&soctherm_suspend_resume_lock);
	soctherm_suspend_locked();
	mutex_unlock(&soctherm_suspend_resume_lock);
	return 0;
}

static void soctherm_resume_locked(void)
{
	if (soctherm_suspended) {
		soctherm_suspended = false;
		soctherm_clk_enable(true);
		soctherm_init_platform_data();
		soctherm_init_platform_done = true;
		soctherm_update();
		enable_irq(INT_THERMAL);
		enable_irq(INT_EDP);
	}
}

static int soctherm_resume(void)
{
	mutex_lock(&soctherm_suspend_resume_lock);
	soctherm_resume_locked();
	mutex_unlock(&soctherm_suspend_resume_lock);
	return 0;
}

static int soctherm_sync(void)
{
	mutex_lock(&soctherm_suspend_resume_lock);

	if (soctherm_suspended) {
		soctherm_resume_locked();
		soctherm_suspend_locked();
	} else {
		soctherm_update();
	}

	mutex_unlock(&soctherm_suspend_resume_lock);
	return 0;
}
late_initcall_sync(soctherm_sync);

static int soctherm_pm_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		soctherm_suspend();
		break;
	case PM_POST_SUSPEND:
		soctherm_resume();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block soctherm_nb = {
	.notifier_call = soctherm_pm_notify,
};

static void soctherm_oc_irq_lock(struct irq_data *data)
{
	struct soctherm_oc_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	mutex_lock(&d->irq_lock);
}
static void soctherm_oc_irq_sync_unlock(struct irq_data *data)
{
	struct soctherm_oc_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	mutex_unlock(&d->irq_lock);
}
static void soctherm_oc_irq_enable(struct irq_data *data)
{
	struct soctherm_oc_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	d->irq_enable |= BIT(data->hwirq);
}

static void soctherm_oc_irq_disable(struct irq_data *data)
{
	struct soctherm_oc_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	d->irq_enable &= ~BIT(data->hwirq);
}

static int soctherm_oc_irq_set_type(struct irq_data *data, unsigned int type)
{
	return 0;
}

static int soctherm_oc_irq_set_wake(struct irq_data *data, unsigned int on)
{
	int gpio;
	int gpio_irq;

	gpio = soctherm_ocx_to_wake_gpio[data->hwirq];
	if (!gpio_is_valid(gpio)) {
		pr_err("No wakeup supported for irq %lu\n", data->hwirq);
		return -EINVAL;
	}

	gpio_irq = gpio_to_irq(gpio);
	if (gpio_irq < 0) {
		pr_err("No gpio_to_irq for gpio %d\n", gpio);
		return gpio;
	}

	irq_set_irq_wake(gpio_irq, on);
	return 0;
}

static int soctherm_oc_irq_map(struct irq_domain *h, unsigned int virq,
		irq_hw_number_t hw)
{
	struct soctherm_oc_irq_chip_data *data = h->host_data;

	irq_set_chip_data(virq, data);
	irq_set_chip(virq, &data->irq_chip);
	irq_set_nested_thread(virq, 1);
	set_irq_flags(virq, IRQF_VALID);
	return 0;
}

static struct irq_domain_ops soctherm_oc_domain_ops = {
	.map	= soctherm_oc_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

static int tegra11_soctherem_oc_int_init(int irq_base, int num_irqs)
{
	if (irq_base <= 0 || !num_irqs) {
		pr_info("%s(): OC interrupts are not enabled\n", __func__);
		return 0;
	}

	mutex_init(&soc_irq_cdata.irq_lock);
	soc_irq_cdata.irq_enable = 0;

	soc_irq_cdata.irq_chip.name = "sco_therm_oc";
	soc_irq_cdata.irq_chip.irq_bus_lock = soctherm_oc_irq_lock,
	soc_irq_cdata.irq_chip.irq_bus_sync_unlock = soctherm_oc_irq_sync_unlock,
	soc_irq_cdata.irq_chip.irq_disable = soctherm_oc_irq_disable,
	soc_irq_cdata.irq_chip.irq_enable = soctherm_oc_irq_enable,
	soc_irq_cdata.irq_chip.irq_set_type = soctherm_oc_irq_set_type,
	soc_irq_cdata.irq_chip.irq_set_wake = soctherm_oc_irq_set_wake,

	irq_base = irq_alloc_descs(irq_base, 0, num_irqs, 0);
	if (irq_base < 0) {
		pr_err("%s: Failed to allocate IRQs: %d\n", __func__, irq_base);
		return irq_base;
	}

	soc_irq_cdata.domain = irq_domain_add_legacy(NULL, num_irqs,
			irq_base, 0, &soctherm_oc_domain_ops, &soc_irq_cdata);
	if (!soc_irq_cdata.domain) {
		pr_err("%s: Failed to create IRQ domain\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s(): OC interrupts enabled successful\n", __func__);
	return 0;
}

int __init tegra11_soctherm_init(struct soctherm_platform_data *data)
{
	int ret;

	register_pm_notifier(&soctherm_nb);

	if (!data)
		return -1;
	plat_data = *data;

	if (soctherm_clk_init() < 0)
		return -1;

	if (soctherm_clk_enable(true) < 0)
		return -1;

	if (soctherm_init_platform_data() < 0)
		return -1;

	soctherm_init_platform_done = true;

	ret = tegra11_soctherem_oc_int_init(data->oc_irq_base,
			data->num_oc_irqs);
	if (ret < 0) {
		pr_err("soctherem_oc_int_init failed: %d\n", ret);
		return ret;
	}


	/* enable threaded interrupts */
	if (request_threaded_irq(INT_THERMAL, soctherm_thermal_isr,
				 soctherm_thermal_work_func, IRQF_ONESHOT,
				 "soctherm_thermal", NULL) < 0)
		return -1;

	if (request_threaded_irq(INT_EDP, soctherm_edp_isr,
				 soctherm_edp_work_func, IRQF_ONESHOT,
				 "soctherm_edp", NULL) < 0)
		return -1;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int regs_show(struct seq_file *s, void *data)
{
	u32 r;
	u32 state;
	int tcpu[TSENSE_SIZE];
	int i, j, level;

	if (soctherm_suspended) {
		seq_printf(s, "SOC_THERM is SUSPENDED\n");
		return 0;
	}

	seq_printf(s, "-----TSENSE (precision %s  convert %s)-----\n",
		PRECISION_TO_STR(), read_hw_temp ? "HW" : "SW");
	for (i = 0; i < TSENSE_SIZE; i++) {
		r = soctherm_readl(TS_TSENSE_REG_OFFSET(TS_CPU0_CONFIG1, i));
		state = REG_GET(r, TS_CPU0_CONFIG1_EN);
		if (!state)
			continue;

		seq_printf(s, "%s: ", sensor_names[i]);

		seq_printf(s, "En(%d) ", state);
		state = REG_GET(r, TS_CPU0_CONFIG1_TIDDQ);
		seq_printf(s, "tiddq(%d) ", state);
		state = REG_GET(r, TS_CPU0_CONFIG1_TEN_COUNT);
		seq_printf(s, "ten_count(%d) ", state);
		state = REG_GET(r, TS_CPU0_CONFIG1_TSAMPLE);
		seq_printf(s, "tsample(%d) ", state + 1);

		r = soctherm_readl(TS_TSENSE_REG_OFFSET(TS_CPU0_STATUS1, i));
		state = REG_GET(r, TS_CPU0_STATUS1_TEMP_VALID);
		seq_printf(s, "Temp(%d/", state);
		state = REG_GET(r, TS_CPU0_STATUS1_TEMP);
		seq_printf(s, "%d) ", tcpu[i] = temp_translate(state));

		r = soctherm_readl(TS_TSENSE_REG_OFFSET(TS_CPU0_STATUS0, i));
		state = REG_GET(r, TS_CPU0_STATUS0_VALID);
		seq_printf(s, "Capture(%d/", state);
		state = REG_GET(r, TS_CPU0_STATUS0_CAPTURE);
		seq_printf(s, "%d) (Converted-temp(%ld) ", state,
			   temp_convert(state, sensor2therm_a[i],
					sensor2therm_b[i]));

		r = soctherm_readl(TS_TSENSE_REG_OFFSET(TS_CPU0_CONFIG0, i));
		state = REG_GET(r, TS_CPU0_CONFIG0_TALL);
		seq_printf(s, "Tall(%d) ", state);
		state = REG_GET(r, TS_CPU0_CONFIG0_TCALC_OVER);
		seq_printf(s, "Over(%d/", state);
		state = REG_GET(r, TS_CPU0_CONFIG0_OVER);
		seq_printf(s, "%d/", state);
		state = REG_GET(r, TS_CPU0_CONFIG0_CPTR_OVER);
		seq_printf(s, "%d) ", state);

		r = soctherm_readl(TS_TSENSE_REG_OFFSET(TS_CPU0_CONFIG2, i));
		state = REG_GET(r, TS_CPU0_CONFIG2_THERM_A);
		seq_printf(s, "Therm_A/B(%d/", state);
		state = REG_GET(r, TS_CPU0_CONFIG2_THERM_B);
		seq_printf(s, "%d)\n", (s16)state);
	}

	r = soctherm_readl(TS_PDIV);
	seq_printf(s, "PDIV: 0x%x\n", r);

	seq_printf(s, "\n");
	seq_printf(s, "-----SOC_THERM-----\n");

	r = soctherm_readl(TS_TEMP1);
	state = REG_GET(r, TS_TEMP1_CPU_TEMP);
	seq_printf(s, "Temperatures: CPU(%ld) ", temp_translate(state));
	state = REG_GET(r, TS_TEMP1_GPU_TEMP);
	seq_printf(s, " GPU(%ld) ", temp_translate(state));
	r = soctherm_readl(TS_TEMP2);
	state = REG_GET(r, TS_TEMP2_PLLX_TEMP);
	seq_printf(s, " PLLX(%ld) ", temp_translate(state));
	state = REG_GET(r, TS_TEMP2_MEM_TEMP);
	seq_printf(s, " MEM(%ld)\n", temp_translate(state));

	for (i = 0; i < THERM_SIZE; i++) {
		if (i == THERM_MEM)
			continue;
		seq_printf(s, "%s:\n", therm_names[i]);

		for (level = 0; level < 4; level++) {
			r = soctherm_readl(TS_THERM_REG_OFFSET(CTL_LVL0_CPU0,
								level, i));
			state = REG_GET(r, CTL_LVL0_CPU0_UP_THRESH);
			seq_printf(s, "   %d: Up/Dn(%d/", level,
				   LOWER_PRECISION_FOR_CONV(state));
			state = REG_GET(r, CTL_LVL0_CPU0_DN_THRESH);
			seq_printf(s, "%d) ", LOWER_PRECISION_FOR_CONV(state));
			state = REG_GET(r, CTL_LVL0_CPU0_EN);
			seq_printf(s, "En(%d) ", state);
			state = REG_GET(r, CTL_LVL0_CPU0_CPU_THROT);
			seq_printf(s, "Throt-CPU");
			seq_printf(s, "(%s) ", state ?
				state == CTL_LVL0_CPU0_DEV_THROT_LIGHT ? "L" :
				state == CTL_LVL0_CPU0_DEV_THROT_HEAVY ? "H" :
				"H+L" : "none");
			state = REG_GET(r, CTL_LVL0_CPU0_GPU_THROT);
			seq_printf(s, "Throt-GPU");
			seq_printf(s, "(%s) ", state ?
				state == CTL_LVL0_CPU0_DEV_THROT_LIGHT ? "L" :
				state == CTL_LVL0_CPU0_DEV_THROT_HEAVY ? "H" :
				"H+L" : "none");
			state = REG_GET(r, CTL_LVL0_CPU0_STATUS);
			seq_printf(s, "Status(%s)\n",
				   state == 0 ? "LO" :
				   state == 1 ? "in" :
				   state == 2 ? "??" : "HI");
		}
	}

	r = soctherm_readl(STATS_CTL);
	seq_printf(s, "STATS: Up(%s) Dn(%s)\n",
		   r & STATS_CTL_EN_UP ? "En" : "--",
		   r & STATS_CTL_EN_DN ? "En" : "--");
	for (level = 0; level < 4; level++) {
		r = soctherm_readl(TS_TSENSE_REG_OFFSET(UP_STATS_L0, level));
		seq_printf(s, "  Level_%d Up(%d) ", level, r);
		r = soctherm_readl(TS_TSENSE_REG_OFFSET(DN_STATS_L0, level));
		seq_printf(s, "Dn(%d)\n", r);
	}

	r = soctherm_readl(THERMTRIP);
	state = REG_GET(r, THERMTRIP_ANY_EN);
	seq_printf(s, "ThermTRIP ANY En(%d)\n", state);
	state = REG_GET(r, THERMTRIP_CPU_EN);
	seq_printf(s, "     CPU En(%d) ", state);
	state = REG_GET(r, THERMTRIP_CPU_THRESH);
	seq_printf(s, "Thresh(%d)\n", LOWER_PRECISION_FOR_CONV(state));
	state = REG_GET(r, THERMTRIP_GPU_EN);
	seq_printf(s, "     GPU En(%d) ", state);
	state = REG_GET(r, THERMTRIP_GPUMEM_THRESH);
	seq_printf(s, "Thresh(%d)\n", LOWER_PRECISION_FOR_CONV(state));
	state = REG_GET(r, THERMTRIP_TSENSE_EN);
	seq_printf(s, "    PLLX En(%d) ", state);
	state = REG_GET(r, THERMTRIP_TSENSE_THRESH);
	seq_printf(s, "Thresh(%d)\n", LOWER_PRECISION_FOR_CONV(state));

	r = soctherm_readl(THROT_GLOBAL_CFG);
	seq_printf(s, "GLOBAL THROTTLE CONFIG: 0x%08x\n", r);

	seq_printf(s, "---------------------------------------------------\n");
	r = soctherm_readl(THROT_STATUS);
	state = REG_GET(r, THROT_STATUS_BREACH);
	seq_printf(s, "THROT STATUS: breach(%d) ", state);
	state = REG_GET(r, THROT_STATUS_STATE);
	seq_printf(s, "state(%d) ", state);
	state = REG_GET(r, THROT_STATUS_ENABLED);
	seq_printf(s, "enabled(%d)\n", state);

	for (j = 0; j < THROTTLE_DEV_SIZE; j++) {
		r = soctherm_readl(CPU_PSKIP_STATUS + (j * 4));
		state = REG_GET(r, CPU_PSKIP_STATUS_M);
		seq_printf(s, "%s PSKIP STATUS: M(%d) ",
			   throt_dev_names[j], state);
		state = REG_GET(r, CPU_PSKIP_STATUS_N);
		seq_printf(s, "N(%d) ", state);
		state = REG_GET(r, CPU_PSKIP_STATUS_ENABLED);
		seq_printf(s, "enabled(%d)\n", state);
	}

	seq_printf(s, "---------------------------------------------------\n");
	seq_printf(s, "THROTTLE control and PSKIP configuration:\n");
	seq_printf(s, "%5s  %3s  %2s  %8s  %7s  %8s  %4s  %4s  %5s  ",
		   "throt", "dev", "en", "dividend", "divisor", "duration",
		   "step", "prio", "delay");
	seq_printf(s, "%2s  %2s  %2s  %2s  %2s  %2s  ",
		   "LL", "HW", "PG", "MD", "01", "EN");
	seq_printf(s, "%8s  %8s  %8s  %8s  %8s\n",
		   "thresh", "period", "count", "filter", "stats");
	for (i = 0; i < THROTTLE_SIZE; i++) {
		for (j = 0; j < THROTTLE_DEV_SIZE; j++) {
			r = soctherm_readl(THROT_PSKIP_CTRL(i, j));
			state = REG_GET(r, THROT_PSKIP_CTRL_ENABLE);
			seq_printf(s, "%5s  %3s  %2d  ",
				   j ? "" : throt_names[i],
				   throt_dev_names[j], state);
			state = REG_GET(r, THROT_PSKIP_CTRL_DIVIDEND);
			seq_printf(s, "%8d  ", state);
			state = REG_GET(r, THROT_PSKIP_CTRL_DIVISOR);
			seq_printf(s, "%7d  ", state);
			r = soctherm_readl(THROT_PSKIP_RAMP(i, j));
			state = REG_GET(r, THROT_PSKIP_RAMP_DURATION);
			seq_printf(s, "%8d  ", state);
			state = REG_GET(r, THROT_PSKIP_RAMP_STEP);
			seq_printf(s, "%4d  ", state);

			if (j) {
				seq_printf(s, "\n");
				continue;
			}

			r = soctherm_readl(THROT_PRIORITY_CTRL(i));
			state = REG_GET(r, THROT_PRIORITY_LITE_PRIO);
			seq_printf(s, "%4d  ", state);

			r = soctherm_readl(THROT_DELAY_CTRL(i));
			state = REG_GET(r, THROT_DELAY_LITE_DELAY);
			seq_printf(s, "%5d  ", state);

			if (i >= THROTTLE_OC1) {
				r = soctherm_readl(ALARM_CFG(i));
				state = REG_GET(r, OC1_CFG_LONG_LATENCY);
				seq_printf(s, "%2d  ", state);
				state = REG_GET(r, OC1_CFG_HW_RESTORE);
				seq_printf(s, "%2d  ", state);
				state = REG_GET(r, OC1_CFG_PWR_GOOD_MASK);
				seq_printf(s, "%2d  ", state);
				state = REG_GET(r, OC1_CFG_THROTTLE_MODE);
				seq_printf(s, "%2d  ", state);
				state = REG_GET(r, OC1_CFG_ALARM_POLARITY);
				seq_printf(s, "%2d  ", state);
				state = REG_GET(r, OC1_CFG_EN_THROTTLE);
				seq_printf(s, "%2d  ", state);

				r = soctherm_readl(ALARM_CNT_THRESHOLD(i));
				seq_printf(s, "%8d  ", r);
				r = soctherm_readl(ALARM_THRESHOLD_PERIOD(i));
				seq_printf(s, "%8d  ", r);
				r = soctherm_readl(ALARM_ALARM_COUNT(i));
				seq_printf(s, "%8d  ", r);
				r = soctherm_readl(ALARM_FILTER(i));
				seq_printf(s, "%8d  ", r);
				r = soctherm_readl(ALARM_STATS(i));
				seq_printf(s, "%8d  ", r);
			}
			seq_printf(s, "\n");
		}
	}
	return 0;
}

static int temp_log_show(struct seq_file *s, void *data)
{
	u32 r, state;
	int i;
	u64 ts;
	u_long ns;
	bool was_suspended = false;

	ts = cpu_clock(0);
	ns = do_div(ts, 1000000000);
	seq_printf(s, "%6lu.%06lu", (u_long) ts, ns / 1000);

	if (soctherm_suspended) {
		mutex_lock(&soctherm_suspend_resume_lock);
		soctherm_resume_locked();
		was_suspended = true;
	}

	for (i = 0; i < TSENSE_SIZE; i++) {
		r = soctherm_readl(TS_TSENSE_REG_OFFSET(
					TS_CPU0_CONFIG1, i));
		state = REG_GET(r, TS_CPU0_CONFIG1_EN);
		if (!state)
			continue;

		r = soctherm_readl(TS_TSENSE_REG_OFFSET(
					TS_CPU0_STATUS1, i));
		if (!REG_GET(r, TS_CPU0_STATUS1_TEMP_VALID)) {
			seq_printf(s, "\tINVALID");
			continue;
		}

		if (read_hw_temp) {
			state = REG_GET(r, TS_CPU0_STATUS1_TEMP);
			seq_printf(s, "\t%ld", temp_translate(state));
		} else {
			r = soctherm_readl(TS_TSENSE_REG_OFFSET(
						TS_CPU0_STATUS0, i));
			state = REG_GET(r, TS_CPU0_STATUS0_CAPTURE);
			seq_printf(s, "\t%ld", temp_convert(state,
						sensor2therm_a[i],
						sensor2therm_b[i]));
		}
	}
	seq_printf(s, "\n");

	if (was_suspended) {
		soctherm_suspend_locked();
		mutex_unlock(&soctherm_suspend_resume_lock);
	}
	return 0;
}

static int regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, regs_show, inode->i_private);
}

static const struct file_operations regs_fops = {
	.open		= regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int convert_get(void *data, u64 *val)
{
	*val = !read_hw_temp;
	return 0;
}
static int convert_set(void *data, u64 val)
{
	read_hw_temp = !val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(convert_fops, convert_get, convert_set, "%llu\n");

static int temp_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, temp_log_show, inode->i_private);
}
static const struct file_operations temp_log_fops = {
	.open		= temp_log_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init soctherm_debug_init(void)
{
	struct dentry *tegra_soctherm_root;

	tegra_soctherm_root = debugfs_create_dir("tegra_soctherm", 0);
	debugfs_create_file("regs", 0644, tegra_soctherm_root,
				NULL, &regs_fops);
	debugfs_create_file("convert", 0644, tegra_soctherm_root,
				NULL, &convert_fops);
	debugfs_create_file("temp_log", 0644, tegra_soctherm_root,
				NULL, &temp_log_fops);
	return 0;
}
late_initcall(soctherm_debug_init);
#endif
