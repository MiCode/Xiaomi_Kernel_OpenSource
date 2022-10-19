// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_def_id.h"
#include "mt6985.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/regmap.h>

#define CHEKCING_UNIVPLL_CLK_DONE 1

void __iomem *gpio_base_remap_addr;
void __iomem *pericfg_ao_remap_addr;
void __iomem *hw_ccf_base_remap_addr;
void __iomem *topckgen_base_remap_addr;
void __iomem *uarthub_base_remap_addr;
void __iomem *uart3_base_remap_addr;
void __iomem *ap_dma_uart_3_tx_int_remap_addr;
void __iomem *spm_remap_addr;
void __iomem *apmixedsys_remap_addr;
void __iomem *iocfg_rm_remap_addr;
void __iomem *sys_sram_remap_addr;

static int uarthub_init_remap_reg_mt6985(void);
static int uarthub_deinit_unmap_reg_mt6985(void);
static int uarthub_get_max_num_dev_host_mt6985(void);
static int uarthub_get_default_baud_rate_mt6985(int dev_index);
static int uarthub_config_gpio_trx_mt6985(void);
static int uarthub_get_gpio_trx_info_mt6985(struct uarthub_gpio_trx_info *info);
static int uarthub_get_uarthub_cg_info_mt6985(void);
static int uarthub_get_uarthub_clk_cg_info_mt6985(void);
#if CHEKCING_UNIVPLL_CLK_DONE
static int uarthub_get_hwccf_univpll_vote_info_mt6985(void);
static int uarthub_get_hwccf_univpll_on_info_mt6985(void);
#endif
static int uarthub_get_uart_mux_info_mt6985(void);
static int uarthub_get_uarthub_addr_info_mt6985(struct uarthub_reg_base_addr *info);
static void __iomem *uarthub_get_ap_uart_base_addr_mt6985(void);
static void __iomem *uarthub_get_ap_dma_tx_int_addr_mt6985(void);
static void __iomem *uarthub_get_sys_sram_addr_mt6985(void);
static int uarthub_get_spm_res_info_mt6985(void);
static int uarthub_get_spm_sys_timer_mt6985(uint32_t *hi, uint32_t *lo);
static int uarthub_get_peri_clk_info_mt6985(void);
static int uarthub_get_peri_uart_pad_mode_mt6985(void);

struct uarthub_ops_struct mt6985_plat_data = {
	.uarthub_plat_init_remap_reg = uarthub_init_remap_reg_mt6985,
	.uarthub_plat_deinit_unmap_reg = uarthub_deinit_unmap_reg_mt6985,
	.uarthub_plat_get_max_num_dev_host = uarthub_get_max_num_dev_host_mt6985,
	.uarthub_plat_get_default_baud_rate = uarthub_get_default_baud_rate_mt6985,
	.uarthub_plat_config_gpio_trx = uarthub_config_gpio_trx_mt6985,
	.uarthub_plat_get_gpio_trx_info = uarthub_get_gpio_trx_info_mt6985,
	.uarthub_plat_get_uarthub_cg_info = uarthub_get_uarthub_cg_info_mt6985,
	.uarthub_plat_get_uarthub_clk_cg_info = uarthub_get_uarthub_clk_cg_info_mt6985,
#if CHEKCING_UNIVPLL_CLK_DONE
	.uarthub_plat_get_hwccf_univpll_vote_info = uarthub_get_hwccf_univpll_vote_info_mt6985,
	.uarthub_plat_get_hwccf_univpll_on_info = uarthub_get_hwccf_univpll_on_info_mt6985,
#endif
	.uarthub_plat_get_uart_mux_info = uarthub_get_uart_mux_info_mt6985,
	.uarthub_plat_get_uarthub_addr_info = uarthub_get_uarthub_addr_info_mt6985,
	.uarthub_plat_get_ap_uart_base_addr = uarthub_get_ap_uart_base_addr_mt6985,
	.uarthub_plat_get_ap_dma_tx_int_addr = uarthub_get_ap_dma_tx_int_addr_mt6985,
	.uarthub_plat_get_sys_sram_base_addr = uarthub_get_sys_sram_addr_mt6985,
	.uarthub_plat_get_spm_res_info = uarthub_get_spm_res_info_mt6985,
	.uarthub_plat_get_spm_sys_timer = uarthub_get_spm_sys_timer_mt6985,
	.uarthub_plat_get_peri_clk_info = uarthub_get_peri_clk_info_mt6985,
	.uarthub_plat_get_peri_uart_pad_mode = uarthub_get_peri_uart_pad_mode_mt6985,
};

int uarthub_init_remap_reg_mt6985(void)
{
	gpio_base_remap_addr = ioremap(GPIO_BASE_ADDR, 0x500);
	pericfg_ao_remap_addr = ioremap(PERICFG_AO_BASE_ADDR, 0x100);
	hw_ccf_base_remap_addr = ioremap(HW_CCF_BASE_ADDR, 0x140C);
	topckgen_base_remap_addr = ioremap(TOPCKGEN_BASE_ADDR, 0x100);
	uarthub_base_remap_addr = ioremap(UARTHUB_BASE_ADDR, 0x500);
	uart3_base_remap_addr = ioremap(UART3_BASE_ADDR, 0x100);
	ap_dma_uart_3_tx_int_remap_addr = ioremap(AP_DMA_UART_3_TX_INT_FLAG_ADDR, 0x100);
	spm_remap_addr = ioremap(SPM_BASE_ADDR, 0x1000);
	apmixedsys_remap_addr = ioremap(APMIXEDSYS_BASE_ADDR, 0x500);
	iocfg_rm_remap_addr = ioremap(IOCFG_RM_BASE_ADDR, 100);
	sys_sram_remap_addr = ioremap(SYS_SRAM_BASE_ADDR, 0x200);

	return 0;
}

int uarthub_deinit_unmap_reg_mt6985(void)
{
	if (gpio_base_remap_addr)
		iounmap(gpio_base_remap_addr);

	if (pericfg_ao_remap_addr)
		iounmap(pericfg_ao_remap_addr);

	if (hw_ccf_base_remap_addr)
		iounmap(hw_ccf_base_remap_addr);

	if (topckgen_base_remap_addr)
		iounmap(topckgen_base_remap_addr);

	if (uarthub_base_remap_addr)
		iounmap(uarthub_base_remap_addr);

	if (uart3_base_remap_addr)
		iounmap(uart3_base_remap_addr);

	if (ap_dma_uart_3_tx_int_remap_addr)
		iounmap(ap_dma_uart_3_tx_int_remap_addr);

	if (spm_remap_addr)
		iounmap(spm_remap_addr);

	if (apmixedsys_remap_addr)
		iounmap(apmixedsys_remap_addr);

	if (iocfg_rm_remap_addr)
		iounmap(iocfg_rm_remap_addr);

	if (sys_sram_remap_addr)
		iounmap(sys_sram_remap_addr);

	return 0;
}

int uarthub_get_max_num_dev_host_mt6985(void)
{
	return UARTHUB_MAX_NUM_DEV_HOST;
}

int uarthub_get_default_baud_rate_mt6985(int dev_index)
{
	if (dev_index == 0)
		return UARTHUB_DEV_0_BAUD_RATE;
	else if (dev_index == 1)
		return UARTHUB_DEV_1_BAUD_RATE;
	else if (dev_index == 2)
		return UARTHUB_DEV_2_BAUD_RATE;
	else if (dev_index == 3)
		return UARTHUB_CMM_BAUD_RATE;

	return -1;
}

int uarthub_config_gpio_trx_mt6985(void)
{
	if (!gpio_base_remap_addr) {
		pr_notice("[%s] gpio_base_remap_addr is NULL\n", __func__);
		return -1;
	}

	UARTHUB_REG_WRITE_MASK(gpio_base_remap_addr + GPIO_HUB_MODE_TX,
		GPIO_HUB_MODE_TX_VALUE, GPIO_HUB_MODE_TX_MASK);
	UARTHUB_REG_WRITE_MASK(gpio_base_remap_addr + GPIO_HUB_MODE_RX,
		GPIO_HUB_MODE_RX_VALUE, GPIO_HUB_MODE_RX_MASK);

	return 0;
}

int uarthub_get_gpio_trx_info_mt6985(struct uarthub_gpio_trx_info *info)
{
	if (!info) {
		pr_notice("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (!gpio_base_remap_addr) {
		pr_notice("[%s] gpio_base_remap_addr is NULL\n", __func__);
		return -1;
	}

	if (!iocfg_rm_remap_addr) {
		pr_notice("[%s] iocfg_rm_remap_addr is NULL\n", __func__);
		return -1;
	}

	info->tx_mode.addr = GPIO_BASE_ADDR + GPIO_HUB_MODE_TX;
	info->tx_mode.mask = GPIO_HUB_MODE_TX_MASK;
	info->tx_mode.value = GPIO_HUB_MODE_TX_VALUE;
	info->tx_mode.gpio_value = UARTHUB_REG_READ(
		gpio_base_remap_addr + GPIO_HUB_MODE_TX);

	info->rx_mode.addr = GPIO_BASE_ADDR + GPIO_HUB_MODE_RX;
	info->rx_mode.mask = GPIO_HUB_MODE_RX_MASK;
	info->rx_mode.value = GPIO_HUB_MODE_RX_VALUE;
	info->rx_mode.gpio_value = UARTHUB_REG_READ(
		gpio_base_remap_addr + GPIO_HUB_MODE_RX);

	info->tx_dir.addr = GPIO_BASE_ADDR + GPIO_HUB_DIR_TX;
	info->tx_dir.mask = GPIO_HUB_DIR_TX_MASK;
	info->tx_dir.gpio_value = (UARTHUB_REG_READ_BIT(
		gpio_base_remap_addr + GPIO_HUB_DIR_TX,
		GPIO_HUB_DIR_TX_MASK) >> GPIO_HUB_DIR_TX_SHIFT);

	info->rx_dir.addr = GPIO_BASE_ADDR + GPIO_HUB_DIR_RX;
	info->rx_dir.mask = GPIO_HUB_DIR_RX_MASK;
	info->rx_dir.gpio_value = (UARTHUB_REG_READ_BIT(
		gpio_base_remap_addr + GPIO_HUB_DIR_RX,
		GPIO_HUB_DIR_RX_MASK) >> GPIO_HUB_DIR_RX_SHIFT);

	info->tx_ies.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_IES_TX;
	info->tx_ies.mask = GPIO_HUB_IES_TX_MASK;
	info->tx_ies.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_IES_TX,
		GPIO_HUB_IES_TX_MASK) >> GPIO_HUB_IES_TX_SHIFT);

	info->rx_ies.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_IES_RX;
	info->rx_ies.mask = GPIO_HUB_IES_RX_MASK;
	info->rx_ies.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_IES_RX,
		GPIO_HUB_IES_RX_MASK) >> GPIO_HUB_IES_RX_SHIFT);

	info->tx_pu.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PU_TX;
	info->tx_pu.mask = GPIO_HUB_PU_TX_MASK;
	info->tx_pu.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_PU_TX,
		GPIO_HUB_PU_TX_MASK) >> GPIO_HUB_PU_TX_SHIFT);

	info->rx_pu.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PU_RX;
	info->rx_pu.mask = GPIO_HUB_PU_RX_MASK;
	info->rx_pu.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_PU_RX,
		GPIO_HUB_PU_RX_MASK) >> GPIO_HUB_PU_RX_SHIFT);

	info->tx_pd.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PD_TX;
	info->tx_pd.mask = GPIO_HUB_PD_TX_MASK;
	info->tx_pd.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_PD_TX,
		GPIO_HUB_PD_TX_MASK) >> GPIO_HUB_PD_TX_SHIFT);

	info->rx_pd.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PD_RX;
	info->rx_pd.mask = GPIO_HUB_PD_RX_MASK;
	info->rx_pd.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_PD_RX,
		GPIO_HUB_PD_RX_MASK) >> GPIO_HUB_PD_RX_SHIFT);

	info->tx_drv.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_DRV_TX;
	info->tx_drv.mask = GPIO_HUB_DRV_TX_MASK;
	info->tx_drv.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_DRV_TX,
		GPIO_HUB_DRV_TX_MASK) >> GPIO_HUB_DRV_TX_SHIFT);

	info->rx_drv.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_DRV_RX;
	info->rx_drv.mask = GPIO_HUB_DRV_RX_MASK;
	info->rx_drv.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_DRV_RX,
		GPIO_HUB_DRV_RX_MASK) >> GPIO_HUB_DRV_RX_SHIFT);

	info->tx_smt.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SMT_TX;
	info->tx_smt.mask = GPIO_HUB_SMT_TX_MASK;
	info->tx_smt.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_SMT_TX,
		GPIO_HUB_SMT_TX_MASK) >> GPIO_HUB_SMT_TX_SHIFT);

	info->rx_smt.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SMT_RX;
	info->rx_smt.mask = GPIO_HUB_SMT_RX_MASK;
	info->rx_smt.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_SMT_RX,
		GPIO_HUB_SMT_RX_MASK) >> GPIO_HUB_SMT_RX_SHIFT);

	info->tx_tdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_TDSEL_TX;
	info->tx_tdsel.mask = GPIO_HUB_TDSEL_TX_MASK;
	info->tx_tdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_TDSEL_TX,
		GPIO_HUB_TDSEL_TX_MASK) >> GPIO_HUB_TDSEL_TX_SHIFT);

	info->rx_tdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_TDSEL_RX;
	info->rx_tdsel.mask = GPIO_HUB_TDSEL_RX_MASK;
	info->rx_tdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_TDSEL_RX,
		GPIO_HUB_TDSEL_RX_MASK) >> GPIO_HUB_TDSEL_RX_SHIFT);

	info->tx_rdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_RDSEL_TX;
	info->tx_rdsel.mask = GPIO_HUB_RDSEL_TX_MASK;
	info->tx_rdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_RDSEL_TX,
		GPIO_HUB_RDSEL_TX_MASK) >> GPIO_HUB_RDSEL_TX_SHIFT);

	info->rx_rdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_RDSEL_RX;
	info->rx_rdsel.mask = GPIO_HUB_RDSEL_RX_MASK;
	info->rx_rdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_RDSEL_RX,
		GPIO_HUB_RDSEL_RX_MASK) >> GPIO_HUB_RDSEL_RX_SHIFT);

	info->tx_sec_en.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SEC_EN_TX;
	info->tx_sec_en.mask = GPIO_HUB_SEC_EN_TX_MASK;
	info->tx_sec_en.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_SEC_EN_TX,
		GPIO_HUB_SEC_EN_TX_MASK) >> GPIO_HUB_SEC_EN_TX_SHIFT);

	info->rx_sec_en.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SEC_EN_RX;
	info->rx_sec_en.mask = GPIO_HUB_SEC_EN_RX_MASK;
	info->rx_sec_en.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr + GPIO_HUB_SEC_EN_RX,
		GPIO_HUB_SEC_EN_RX_MASK) >> GPIO_HUB_SEC_EN_RX_SHIFT);

	info->rx_din.addr = GPIO_BASE_ADDR + GPIO_HUB_DIN_RX;
	info->rx_din.mask = GPIO_HUB_DIN_RX_MASK;
	info->rx_din.gpio_value = (UARTHUB_REG_READ_BIT(
		gpio_base_remap_addr + GPIO_HUB_DIN_RX,
		GPIO_HUB_DIN_RX_MASK) >> GPIO_HUB_DIN_RX_SHIFT);

	return 0;
}

int uarthub_get_uarthub_cg_info_mt6985(void)
{
	if (!pericfg_ao_remap_addr) {
		pr_notice("[%s] pericfg_ao_remap_addr is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr + PERI_CG_1,
		PERI_CG_1_UARTHUB_CG_MASK) >> PERI_CG_1_UARTHUB_CG_SHIFT);
}

int uarthub_get_uarthub_clk_cg_info_mt6985(void)
{
	if (!pericfg_ao_remap_addr) {
		pr_notice("[%s] pericfg_ao_remap_addr is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr + PERI_CG_1,
		PERI_CG_1_UARTHUB_CLK_CG_MASK) >> PERI_CG_1_UARTHUB_CLK_CG_SHIFT);
}

int uarthub_get_peri_clk_info_mt6985(void)
{
	if (!pericfg_ao_remap_addr) {
		pr_notice("[%s] pericfg_ao_remap_addr is NULL\n", __func__);
		return -1;
	}

	return UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr + PERI_CLOCK_CON,
		PERI_UART_FBCLK_CKSEL);
}

#if CHEKCING_UNIVPLL_CLK_DONE
int uarthub_get_hwccf_univpll_vote_info_mt6985(void)
{
	if (!hw_ccf_base_remap_addr) {
		pr_notice("[%s] hw_ccf_base_remap_addr is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(hw_ccf_base_remap_addr + HW_CCF_PLL_DONE,
		(0x1 << HW_CCF_PLL_DONE_SHIFT)) >> HW_CCF_PLL_DONE_SHIFT);
}

int uarthub_get_hwccf_univpll_on_info_mt6985(void)
{
	if (!apmixedsys_remap_addr) {
		pr_notice("[%s] apmixedsys_remap_addr is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(apmixedsys_remap_addr + UNIVPLL_CON0,
		(0x1 << UNIVPLL_CON0_SHIFT)) >> UNIVPLL_CON0_SHIFT);
}
#endif

int uarthub_get_uart_mux_info_mt6985(void)
{
	if (!topckgen_base_remap_addr) {
		pr_notice("[%s] topckgen_base_remap_addr is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr + CLK_CFG_6,
		CLK_CFG_6_MASK) >> CLK_CFG_6_SHIFT);
}

int uarthub_get_uarthub_addr_info_mt6985(struct uarthub_reg_base_addr *info)
{
	if (!info) {
		pr_notice("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (!uarthub_base_remap_addr) {
		pr_notice("[%s] uarthub_base_remap_addr is NULL\n", __func__);
		return -1;
	}

	info->vir_addr = (unsigned long) uarthub_base_remap_addr;
	info->phy_addr = UARTHUB_BASE_ADDR;

	return 0;
}

void __iomem *uarthub_get_ap_uart_base_addr_mt6985(void)
{
	return uart3_base_remap_addr;
}

void __iomem *uarthub_get_ap_dma_tx_int_addr_mt6985(void)
{
	return ap_dma_uart_3_tx_int_remap_addr;
}

void __iomem *uarthub_get_sys_sram_addr_mt6985(void)
{
	return sys_sram_remap_addr;
}

int uarthub_get_spm_res_info_mt6985(void)
{
	unsigned int spm_res1 = 0, spm_res2 = 0;

	if (!spm_remap_addr) {
		pr_notice("[%s] spm_remap_addr is NULL\n", __func__);
		return -1;
	}

	spm_res1 = UARTHUB_REG_READ_BIT(spm_remap_addr + SPM_REQ_STA_9,
		SPM_REQ_STA_9_MASK) >> SPM_REQ_STA_9_SHIFT;
	spm_res2 = UARTHUB_REG_READ_BIT(spm_remap_addr + MD32PCM_SCU_CTRL1,
		MD32PCM_SCU_CTRL1_MASK) >> MD32PCM_SCU_CTRL1_SHIFT;

	if (spm_res1 != 0x1D || spm_res2 != 0x17)
		return 0;

	return 1;
}
int uarthub_get_spm_sys_timer_mt6985(uint32_t *hi, uint32_t *lo)
{
	if (hi == NULL || lo == NULL) {
		pr_notice("[%s] invalid argument\n", __func__);
		return -1;
	}

	if (!spm_remap_addr) {
		pr_notice("[%s] spm_remap_addr is NULL\n", __func__);
		return -1;
	}

	*hi = UARTHUB_REG_READ(spm_remap_addr + SPM_SYS_TIMER_H);
	*lo = UARTHUB_REG_READ(spm_remap_addr + SPM_SYS_TIMER_L);

	return 1;
}



int uarthub_get_peri_uart_pad_mode_mt6985(void)
{
	if (!pericfg_ao_remap_addr) {
		pr_notice("[%s] pericfg_ao_remap_addr is NULL\n", __func__);
		return -1;
	}

	/* 1: UART_PAD mode */
	/* 0: UARTHUB mode */
	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr + PERI_UART_WAKEUP,
		PERI_UART_WAKEUP_MASK) >> PERI_UART_WAKEUP_SHIFT);
}
