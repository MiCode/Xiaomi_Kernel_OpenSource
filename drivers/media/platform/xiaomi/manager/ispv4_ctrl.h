#ifndef _ISPV4_CTRL_H_
#define _ISPV4_CTRL_H_

#include <linux/types.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include "ispv4_ctrl_ext.h"

#define AP_INTC_REG_ADDR                     (0xD42C000)
#define AP_INTC_G0R0_INT_MASK_REG_ADDR       (0xD42C000)
#define AP_INTC_G1R0_INT_MASK_REG_ADDR       (0xD42C020)
#define AP_INTC_G2R0_INT_MASK_REG_ADDR       (0xD42C040)
#define AP_WDT_SYS_RSTT_BIT			(1<<9)
#define AP_WDT_SYS_RST_BIT			(1<<8)
#define AP_WDT_INTRT_BIT			(1<<7)
#define AP_WDT_INTR_BIT				(1<<6)
#define ISPV4_WDT_RESET_REG_ADDR1	(0x215400c)
#define ISPV4_WDT_RESET_REG_ADDR2	(0x215800c)

#define AP_INTC_G0R0_INT_STATUS_REG_ADDR     (0xD42C004)
#define AP_INTC_G1R1_INT_STATUS_REG_ADDR     (0xD42C02C)
#define AP_PMU_INT_BIT				(1<<14)

#define AP_INTC_G0R1_INT_MASK_REG_ADDR       (0xD42C008)
#define AP_INTC_G1R1_INT_MASK_REG_ADDR       (0xD42C028)
#define AP_INTC_G2R1_INT_MASK_REG_ADDR       (0xD42C048)

#define ISPV4_PMU_SRC_REG_ADDR               (0xD45003C)
#define ISPV4_PMU_SRC_BIT                    (1<<14)

#define AP_INTC_G2R1_INT_STATUS_REG_ADDR     (0xD42C04C)
#define AP_SOF_INT_BIT				(1<<25)

#define ISPV4_WDT_RESET_VAL	0x76
#define AHB_ISP_CON		0xd462018
#define AHB_ISP_CON_MASK	0x3
#define AHB_ISP_CON_VALUE	0x2
#define AHB_ISP_CON_VALUE_D	0x20002

#define AXI_MATRIX_CON		0xd46200c
#define AXI_MATRIX_CON_MASK	0x3
#define AXI_MATRIX_CON_VALUE	0x2
#define AXI_MATRIX_CON_VALUE_D	0x2

#define PHY_PCIE_CON		0xd462024
#define PHY_PCIE_CON_MASK	0x1
#define PHY_PCIE_CON_VALUE	0x1
#define PHY_PCIE_CON_VALUE_D	0x10201

#define CPUPLL_CON0	 	0xd460000
#define CPUPLL_CON1		0xd460004
#define CPUPLL_CON2		0xd460008
#define CPUPLL_CON4		0xd460010
#define CPUPLL_CON0_MASK	((1 << 0) | (1 << 5))
#define CPUPLL_CON0_VALUE	((1 << 0) | (1 << 5))
#define CPUPLL_CON0_DISEN	(1 << 5)
#define CPUPLL_CON4_MASK	0x1
#define CPUPLL_CON4_VALUE	0x1
#define CPUPLL_CON0_DISABLE	0x2c
#define CPUPLL_CON0_ENABLE  	0x2d
#define CPUPLL_CON1_CONFIG	0x98c0000
#define CPUPLL_CON2_CONFIG	0xdc1
#define CPUPLL_CON4_CONFIG	0x0


#define CPU_RST_CORE_SW_RST		0xd40040c
#define CPU_RST_CORE_SW_RST_VALUE	0x11f

#define TOP_CPU_PWR_CTL		0xd451218
#define TOP_CPU_PWR_CTL_MASK	0x1
#define TOP_CPU_PWR_CTL_VALUE	0x1

#define TOP_CPU_PWR_STATUS		0xd451214
#define TOP_CPU_PWR_STATUS_MASK		0x3
#define TOP_CPU_PWR_STATUS_VALUE	0x3

#define BUSMON_DEBUG_TIMEOUT		0xd470018
#define BUSMON_DEBUG_TIMEOUT_MASK	((1 << 16) - 1)
#define BUSMON_DEBUG_TIMEOUT_VALUE	500

#define ISPV4_CLK_PLL_BASE		0x0d460000
#define DDRPLL_CON0			(ISPV4_CLK_PLL_BASE + 0x14)
#define DDRPLL_CON1			(ISPV4_CLK_PLL_BASE + 0x18)
#define DDRPLL_CON2			(ISPV4_CLK_PLL_BASE + 0x1C)
#define DDRPLL_CON3			(ISPV4_CLK_PLL_BASE + 0x20)
#define DDRPLL_CON4			(ISPV4_CLK_PLL_BASE + 0x24)
#define PLL_CON4_MASK			0x1
#define PLL_CON4_VALUE			0x1

#define PLL_TEST_MASK			0x7
#define PLL_TEST_VALUE			0x6
#define DPLL_TEST_OUT			0x0d448048
#define DPLL_TEST_EN			0x0d44804c
#define DPLL_TEST_LOCK			0x0d448058
#define CPLL_TEST_OUT			0x0d44805c
#define CPLL_TEST_EN			0x0d448060
#define CPLL_TEST_LOCK			0x0d448064
#define DPLL_ENABLE 			0x2d
#define DPLL_DISABLE			0x2c
#define DPLL_2133_CON1_SET		0x98c0000
#define DPLL_2133_CON2_SET		0xdc1
#define DPLL_1600_CON1_SET		0x9aaaaaa
#define DPLL_1600_CON2_SET		0xa41
#define DPLL_CON4_CONFIG  		0x0
#define ISPV4_PLLTEST_TIMEOUT		3
#define PLL_POWER_ON			1
#define PLL_POWER_OFF			0
#define DPLL_SETRATE_1600M		1
#define DPLL_SETRATE_2133M		0

int ispv4_power_on_cpu(struct platform_device *pdev);
void ispv4_power_off_cpu(struct platform_device *pdev);
int ispv4_pll_en(void);
int ispv4_pll_disenable(void);
int ispv4_ddrpll_enable(void);
int ispv4_ddrpll_disable(void);
int ispv4_ddrpll_1600m(void);
int ispv4_ddrpll_2133m(void);
int ispv4_plltest_status(u32 addr);
int ispv4_config_dpll_gpio(struct platform_device *pdev);
int ispv4_config_cpll_gpio(struct platform_device *pdev);

#endif
